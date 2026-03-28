# cgroups y Limites

## Indice
1. [Donde estamos en el proyecto](#1-donde-estamos-en-el-proyecto)
2. [Que son los cgroups v2](#2-que-son-los-cgroups-v2)
3. [Jerarquia de cgroups v2](#3-jerarquia-de-cgroups-v2)
4. [Crear un cgroup desde C](#4-crear-un-cgroup-desde-c)
5. [Asignar el proceso al cgroup](#5-asignar-el-proceso-al-cgroup)
6. [Limitar memoria (memory.max)](#6-limitar-memoria-memorymax)
7. [Limitar CPU (cpu.max)](#7-limitar-cpu-cpumax)
8. [Limitar procesos (pids.max)](#8-limitar-procesos-pidsmax)
9. [Leer estadisticas del cgroup](#9-leer-estadisticas-del-cgroup)
10. [Cleanup: eliminar el cgroup al salir](#10-cleanup-eliminar-el-cgroup-al-salir)
11. [Integrar en container.c](#11-integrar-en-containerc)
12. [Verificar que los limites funcionan](#12-verificar-que-los-limites-funcionan)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. Donde estamos en el proyecto

En S01 construimos un contenedor con aislamiento de namespaces:

```
    S01 (completado):
    [✓] clone() con CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC
    [✓] pivot_root() para filesystem aislado
    [✓] /proc montado
    [✓] Hostname propio

    S02 (esta seccion):
    [ ] Crear cgroup v2 para el contenedor
    [ ] Limitar memoria
    [ ] Limitar CPU
    [ ] Limitar numero de procesos
    [ ] Cleanup al salir
```

Ahora el contenedor esta aislado — no puede **ver** los
recursos del host. Pero puede **consumirlos** sin restriccion:
un proceso dentro del contenedor puede usar toda la RAM, todos
los CPUs y crear miles de procesos. Los cgroups resuelven esto.

```
    Sin cgroups:                    Con cgroups:
    +------------------+            +------------------+
    | Contenedor       |            | Contenedor       |
    | RAM: ilimitada   |            | RAM: max 256 MB  |
    | CPU: 100%        |            | CPU: max 50%     |
    | PIDs: ilimitados |            | PIDs: max 64     |
    +------------------+            +------------------+
```

---

## 2. Que son los cgroups v2

**cgroups** (control groups) es un mecanismo del kernel de Linux
que permite limitar, contabilizar y aislar el uso de recursos
(CPU, memoria, I/O, PIDs) de un grupo de procesos.

Existen dos versiones:
- **cgroups v1**: multiples jerarquias, una por controlador.
  Complejo y con inconsistencias. Deprecated.
- **cgroups v2**: jerarquia unica unificada. Mas simple y
  coherente. Es el estandar desde kernel 4.5+ y el default
  en distros modernas (Fedora 31+, Ubuntu 21.10+).

### Verificar que tu sistema usa cgroups v2

```bash
# Si este mount existe, tienes cgroups v2
$ mount | grep cgroup2
cgroup2 on /sys/fs/cgroup type cgroup2 (rw,nosuid,nodev,noexec,relatime)

# O verificar el tipo de filesystem
$ stat -f -c %T /sys/fs/cgroup
cgroup2fs
```

### La interfaz es el filesystem

Los cgroups se controlan **escribiendo en archivos**. No hay
syscalls especiales — todo es `mkdir`, `open`, `write`, `read`
sobre `/sys/fs/cgroup/`:

```
    /sys/fs/cgroup/                       <-- raiz de la jerarquia
    +-- cgroup.controllers                <-- controladores disponibles
    +-- cgroup.subtree_control            <-- controladores activos para hijos
    +-- cgroup.procs                      <-- PIDs en este grupo
    +-- memory.max                        <-- limite de memoria
    +-- cpu.max                           <-- limite de CPU
    +-- pids.max                          <-- limite de procesos
    +-- ...
```

---

## 3. Jerarquia de cgroups v2

```
    /sys/fs/cgroup/  (root cgroup)
    |
    +-- cgroup.controllers: "cpuset cpu io memory pids"
    +-- cgroup.subtree_control: "cpu memory pids"
    |
    +-- system.slice/           (servicios systemd)
    |   +-- sshd.service/
    |   +-- nginx.service/
    |
    +-- user.slice/             (sesiones de usuario)
    |   +-- user-1000.slice/
    |
    +-- container-XXXX/         <-- nuestro cgroup (lo crearemos)
        +-- cgroup.procs        <-- PID del contenedor
        +-- memory.max          <-- limite de memoria
        +-- memory.current      <-- uso actual de memoria
        +-- cpu.max             <-- limite de CPU
        +-- cpu.stat            <-- estadisticas de CPU
        +-- pids.max            <-- limite de PIDs
        +-- pids.current        <-- PIDs actuales
```

### Regla del "no internal processes"

En cgroups v2, un cgroup no puede tener **procesos directos**
y **sub-cgroups hijos** al mismo tiempo (con controladores
activos). Esto simplifica la contabilidad:

```
    MAL (v2 lo prohibe):          BIEN:
    /sys/fs/cgroup/mygroup/       /sys/fs/cgroup/mygroup/
    +-- cgroup.procs: "1234"      +-- child1/
    +-- child/                    |   +-- cgroup.procs: "1234"
        +-- cgroup.procs: "5678"  +-- child2/
                                      +-- cgroup.procs: "5678"
```

Para nuestro caso esto no es problema — creamos un cgroup leaf
(sin sub-cgroups) donde metemos el proceso contenedor.

---

## 4. Crear un cgroup desde C

Crear un cgroup es simplemente crear un directorio dentro de
`/sys/fs/cgroup/`:

```c
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define CGROUP_ROOT "/sys/fs/cgroup"

/* Escribir un string en un archivo (util para toda la config de cgroups) */
static int write_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror(path);
        return -1;
    }

    ssize_t len = strlen(value);
    if (write(fd, value, len) != len) {
        perror(path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* Crear un cgroup para el contenedor */
static int cgroup_create(const char *name, char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s", CGROUP_ROOT, name);

    if (mkdir(path, 0755) == -1) {
        if (errno == EEXIST) {
            /* Ya existe, reutilizar */
            fprintf(stderr, "[cgroup] reusing existing cgroup: %s\n", path);
            return 0;
        }
        perror("mkdir cgroup");
        return -1;
    }

    fprintf(stderr, "[cgroup] created: %s\n", path);
    return 0;
}
```

```
    Antes:                          Despues de mkdir:
    /sys/fs/cgroup/                 /sys/fs/cgroup/
    +-- system.slice/               +-- system.slice/
    +-- user.slice/                 +-- user.slice/
                                    +-- container-12345/  <-- NUEVO
                                        +-- cgroup.procs
                                        +-- memory.max
                                        +-- cpu.max
                                        +-- pids.max
                                        +-- ... (auto-creados por kernel)
```

El kernel **automaticamente** crea los archivos de control
dentro del nuevo directorio. No necesitas crearlos tu.

---

## 5. Asignar el proceso al cgroup

Para mover un proceso a un cgroup, escribes su PID en
`cgroup.procs`:

```c
static int cgroup_add_pid(const char *cgroup_path, pid_t pid)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/cgroup.procs", cgroup_path);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", pid);

    return write_file(path, buf);
}
```

### Cuando asignar el PID

Hay dos estrategias:

**Estrategia 1: padre asigna al hijo despues de clone()**

```c
pid_t child_pid = clone(child_func, stack + STACK_SIZE, flags, &ca);
cgroup_add_pid(cgroup_path, child_pid);  // padre asigna
```

Problema: hay una ventana entre `clone()` y la asignacion donde
el hijo ejecuta sin limites.

**Estrategia 2: hijo se asigna a si mismo (PID 0 = self)**

```c
/* Dentro de child_func: */
static int child_func(void *arg) {
    /* Escribir "0" en cgroup.procs significa "este proceso" */
    write_file("/sys/fs/cgroup/container-X/cgroup.procs", "0");
    /* A partir de aqui, los limites aplican */
    // ... setup_root, setup_proc, exec ...
}
```

Problema: despues de `pivot_root()`, `/sys/fs/cgroup` ya no
es accesible (estamos en el rootfs de Alpine).

**Estrategia 3 (la correcta): padre asigna ANTES de que hijo haga exec**

El padre crea el cgroup y asigna al hijo inmediatamente
despues de `clone()`. El hijo espera (con un pipe o similar)
antes de proceder con `setup_root()`:

```c
/* main: */
pid_t child_pid = clone(child_func, stack + STACK_SIZE, flags, &ca);
cgroup_add_pid(cgroup_path, child_pid);
/* Senalizar al hijo que puede continuar (via pipe) */
```

Para simplificar nuestro mini runtime, usaremos la
**estrategia 1** — la ventana es minima y aceptable para un
proyecto educativo.

---

## 6. Limitar memoria (memory.max)

Escribir en `memory.max` establece el limite maximo de memoria
(en bytes) que los procesos del cgroup pueden usar:

```c
static int cgroup_limit_memory(const char *cgroup_path, const char *limit)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/memory.max", cgroup_path);
    return write_file(path, limit);
}

/* Uso: */
cgroup_limit_memory(cgroup_path, "268435456");  /* 256 MB */
```

### Formato de memory.max

```
+---------------------+-----------------------------------+
| Valor               | Significado                       |
+---------------------+-----------------------------------+
| "268435456"         | 256 MB (en bytes)                 |
| "max"               | Sin limite (default)              |
+---------------------+-----------------------------------+
```

> **Nota**: cgroups v2 NO acepta sufijos como "256M". El valor
> es siempre en **bytes** como string numerico.

### Que pasa cuando se excede el limite

Cuando un proceso del cgroup intenta usar mas memoria que
`memory.max`:

1. El kernel intenta recuperar memoria (reclaim, swap si hay)
2. Si no puede, invoca el **OOM killer**: mata un proceso
   del cgroup
3. El proceso recibe SIGKILL (no SIGTERM — no puede manejarse)

```
    Proceso en cgroup (memory.max = 256MB):

    Uso actual: 200 MB  --> OK
    Uso actual: 250 MB  --> OK
    Uso actual: 256 MB  --> intenta reclaim
    malloc(10 MB)       --> OOM killer --> SIGKILL
```

### memory.swap.max

Para controlar tambien el uso de swap:

```c
static int cgroup_limit_swap(const char *cgroup_path, const char *limit)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/memory.swap.max", cgroup_path);
    return write_file(path, limit);
}

/* Sin swap: */
cgroup_limit_swap(cgroup_path, "0");
```

---

## 7. Limitar CPU (cpu.max)

`cpu.max` controla cuanto tiempo de CPU puede usar el cgroup
en un periodo dado. El formato es:

```
$MAX $PERIOD
```

Donde `$MAX` es microsegundos de CPU permitidos por cada
`$PERIOD` microsegundos:

```c
static int cgroup_limit_cpu(const char *cgroup_path,
                            int max_usec, int period_usec)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/cpu.max", cgroup_path);

    char buf[64];
    snprintf(buf, sizeof(buf), "%d %d", max_usec, period_usec);
    return write_file(path, buf);
}

/* 50% de un CPU: 50000 us de cada 100000 us */
cgroup_limit_cpu(cgroup_path, 50000, 100000);

/* 25% de un CPU: */
cgroup_limit_cpu(cgroup_path, 25000, 100000);

/* 200% (2 CPUs completos): */
cgroup_limit_cpu(cgroup_path, 200000, 100000);
```

### Ejemplos de cpu.max

```
+-------------------+-----------------------------------------------+
| Valor cpu.max     | Significado                                   |
+-------------------+-----------------------------------------------+
| "max 100000"      | Sin limite (default)                          |
| "100000 100000"   | 100% de 1 CPU                                 |
| "50000 100000"    | 50% de 1 CPU                                  |
| "200000 100000"   | 200% = 2 CPUs completos                       |
| "10000 100000"    | 10% de 1 CPU                                  |
+-------------------+-----------------------------------------------+

    Timeline con cpu.max = "50000 100000":

    |===== 50ms CPU =====|---- 50ms throttled ----|
    |<-------------- 100ms periodo -------------->|
    |===== 50ms CPU =====|---- 50ms throttled ----|
    ...
```

### Que pasa cuando se excede

A diferencia de memoria, exceder CPU no mata el proceso — lo
**throttlea** (lo suspende hasta el siguiente periodo):

```
    Sin limite:     [==============================] 100% CPU
    50% limite:     [====]    [====]    [====]       50% CPU
                         ^^^^      ^^^^
                        throttled  throttled
```

El proceso sigue funcionando pero mas lento. Puedes ver las
estadisticas de throttling en `cpu.stat`:

```bash
$ cat /sys/fs/cgroup/container-X/cpu.stat
usage_usec 123456
user_usec 100000
system_usec 23456
nr_periods 100
nr_throttled 45        # fue throttleado 45 veces
throttled_usec 2250000 # 2.25 segundos totales throttleado
```

---

## 8. Limitar procesos (pids.max)

`pids.max` limita el numero total de procesos (y threads)
que pueden existir en el cgroup:

```c
static int cgroup_limit_pids(const char *cgroup_path, const char *limit)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/pids.max", cgroup_path);
    return write_file(path, limit);
}

/* Maximo 64 procesos: */
cgroup_limit_pids(cgroup_path, "64");
```

### Por que es importante

Sin limite de PIDs, un proceso malicioso puede hacer un
**fork bomb** que crea procesos exponencialmente hasta colapsar
el sistema:

```bash
# Fork bomb clasica — NO ejecutar sin proteccion
:(){ :|:& };:
```

Con `pids.max = 64`, el fork bomb se detiene al llegar a 64
procesos:

```
    Sin pids.max:               Con pids.max = 64:
    1 -> 2 -> 4 -> 8 -> ...    1 -> 2 -> 4 -> ... -> 64
    -> 16 -> 32 -> 64 ->       fork() retorna EAGAIN
    128 -> 256 -> ...           (no puede crear mas procesos)
    SISTEMA MUERTO              Contenedor limitado, host OK
```

Cuando se alcanza el limite, `fork()` y `clone()` retornan
`-1` con `errno = EAGAIN`.

---

## 9. Leer estadisticas del cgroup

Puedes leer archivos del cgroup para obtener estadisticas
de uso actual:

```c
static int read_file(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror(path);
        return -1;
    }

    ssize_t n = read(fd, buf, size - 1);
    close(fd);

    if (n < 0) {
        perror(path);
        return -1;
    }

    buf[n] = '\0';
    /* Quitar newline final si existe */
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
    }
    return 0;
}

static void cgroup_print_stats(const char *cgroup_path)
{
    char buf[256];
    char path[512];

    snprintf(path, sizeof(path), "%s/memory.current", cgroup_path);
    if (read_file(path, buf, sizeof(buf)) == 0) {
        long mem = atol(buf);
        fprintf(stderr, "[cgroup] memory: %ld bytes (%.1f MB)\n",
                mem, mem / (1024.0 * 1024.0));
    }

    snprintf(path, sizeof(path), "%s/pids.current", cgroup_path);
    if (read_file(path, buf, sizeof(buf)) == 0) {
        fprintf(stderr, "[cgroup] pids: %s\n", buf);
    }
}
```

### Archivos de estadisticas importantes

```
+---------------------+-------------------------------------------+
| Archivo             | Contenido                                 |
+---------------------+-------------------------------------------+
| memory.current      | Bytes de memoria usados actualmente       |
| memory.max          | Limite configurado                        |
| memory.swap.current | Bytes de swap usados                      |
| memory.stat         | Desglose detallado (anon, file, slab...) |
| memory.events       | Conteo de OOM kills, reclaims, etc.       |
| cpu.stat            | usage_usec, nr_throttled, etc.            |
| pids.current        | Numero actual de PIDs                     |
| pids.max            | Limite configurado                        |
| cgroup.procs        | Lista de PIDs en este cgroup              |
| cgroup.events       | populated (0|1), frozen (0|1)             |
+---------------------+-------------------------------------------+
```

---

## 10. Cleanup: eliminar el cgroup al salir

Un cgroup se elimina con `rmdir()`, pero **solo** si no tiene
procesos ni sub-cgroups. Debemos asegurarnos de que el
contenedor ha terminado antes de limpiar:

```c
static int cgroup_destroy(const char *cgroup_path)
{
    /* rmdir solo funciona si el cgroup esta vacio (sin procesos) */
    if (rmdir(cgroup_path) == -1) {
        if (errno == EBUSY) {
            fprintf(stderr,
                "[cgroup] warning: cgroup not empty, cannot remove: %s\n",
                cgroup_path);
        } else {
            perror("rmdir cgroup");
        }
        return -1;
    }

    fprintf(stderr, "[cgroup] removed: %s\n", cgroup_path);
    return 0;
}
```

### Flujo de cleanup

```
    main():
    1. cgroup_create("container-XXXX")
    2. clone() --> child_pid
    3. cgroup_add_pid(cgroup_path, child_pid)
    4. waitpid(child_pid)          <-- espera a que termine
    5. cgroup_destroy(cgroup_path) <-- ahora el cgroup esta vacio
    6. munmap(stack)
    7. return
```

### Generar nombre unico para el cgroup

Cada invocacion del contenedor necesita un cgroup con nombre
unico para evitar conflictos:

```c
static void cgroup_name(char *buf, size_t size)
{
    snprintf(buf, size, "container-%d", getpid());
}

/* Resultado: "container-12345" */
```

Usar el PID del padre es simple y unico (mientras el padre
este vivo). Alternativas: UUID, timestamp, etc.

---

## 11. Integrar en container.c

Agregamos las funciones de cgroup al `container.c` de S01.
Los cambios se concentran en `main()` (el padre configura
el cgroup; el hijo no necesita saber nada):

### Nuevos argumentos de linea de comandos

```bash
sudo ./container [opciones] <rootfs> <command> [args...]

Opciones:
  --mem  <bytes>    Limite de memoria (default: sin limite)
  --cpu  <percent>  Limite de CPU en porcentaje (default: sin limite)
  --pids <max>      Limite de procesos (default: sin limite)
```

### Estructura de configuracion

```c
struct container_config {
    const char *rootfs;
    char *const *argv;
    const char *mem_limit;   /* NULL = sin limite */
    int cpu_percent;         /* 0 = sin limite */
    int pids_limit;          /* 0 = sin limite */
};
```

### container.c actualizado (fragmentos nuevos)

```c
/* --------------------------------------------------------
 * Funciones de cgroup (nuevas)
 * -------------------------------------------------------- */

#define CGROUP_ROOT "/sys/fs/cgroup"

static int write_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror(path);
        return -1;
    }
    ssize_t len = strlen(value);
    if (write(fd, value, len) != len) {
        perror(path);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int cgroup_setup(const char *cgroup_path,
                        const struct container_config *cfg,
                        pid_t child_pid)
{
    /* Crear el cgroup */
    if (mkdir(cgroup_path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir cgroup");
        return -1;
    }
    fprintf(stderr, "[cgroup] created: %s\n", cgroup_path);

    /* Asignar el proceso hijo */
    char procs_path[512], buf[64];
    snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);
    snprintf(buf, sizeof(buf), "%d", child_pid);
    if (write_file(procs_path, buf) == -1) {
        return -1;
    }
    fprintf(stderr, "[cgroup] added PID %d\n", child_pid);

    /* Limitar memoria */
    if (cfg->mem_limit) {
        char path[512];
        snprintf(path, sizeof(path), "%s/memory.max", cgroup_path);
        if (write_file(path, cfg->mem_limit) == -1) return -1;
        fprintf(stderr, "[cgroup] memory.max = %s\n", cfg->mem_limit);

        /* Deshabilitar swap */
        snprintf(path, sizeof(path), "%s/memory.swap.max", cgroup_path);
        write_file(path, "0");
    }

    /* Limitar CPU */
    if (cfg->cpu_percent > 0) {
        char path[512], val[64];
        snprintf(path, sizeof(path), "%s/cpu.max", cgroup_path);
        int max_usec = cfg->cpu_percent * 1000;  /* percent * 1000 */
        snprintf(val, sizeof(val), "%d 100000", max_usec);
        if (write_file(path, val) == -1) return -1;
        fprintf(stderr, "[cgroup] cpu.max = %s\n", val);
    }

    /* Limitar PIDs */
    if (cfg->pids_limit > 0) {
        char path[512], val[32];
        snprintf(path, sizeof(path), "%s/pids.max", cgroup_path);
        snprintf(val, sizeof(val), "%d", cfg->pids_limit);
        if (write_file(path, val) == -1) return -1;
        fprintf(stderr, "[cgroup] pids.max = %s\n", val);
    }

    return 0;
}

static void cgroup_destroy(const char *cgroup_path)
{
    if (rmdir(cgroup_path) == -1) {
        if (errno != ENOENT) {
            perror("rmdir cgroup");
        }
    } else {
        fprintf(stderr, "[cgroup] removed: %s\n", cgroup_path);
    }
}
```

### main() actualizado

```c
int main(int argc, char *argv[])
{
    struct container_config cfg = {0};
    int argi = 1;

    /* Parsear opciones */
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--mem") == 0 && argi + 1 < argc) {
            cfg.mem_limit = argv[++argi];
        } else if (strcmp(argv[argi], "--cpu") == 0 && argi + 1 < argc) {
            cfg.cpu_percent = atoi(argv[++argi]);
        } else if (strcmp(argv[argi], "--pids") == 0 && argi + 1 < argc) {
            cfg.pids_limit = atoi(argv[++argi]);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argi]);
            fprintf(stderr,
                "Usage: %s [--mem <bytes>] [--cpu <percent>] "
                "[--pids <max>] <rootfs> <command> [args...]\n", argv[0]);
            return 1;
        }
        argi++;
    }

    if (argc - argi < 2) {
        fprintf(stderr,
            "Usage: %s [--mem <bytes>] [--cpu <percent>] "
            "[--pids <max>] <rootfs> <command> [args...]\n", argv[0]);
        return 1;
    }

    cfg.rootfs = argv[argi];
    cfg.argv = &argv[argi + 1];

    /* Verificar rootfs */
    struct stat st;
    if (stat(cfg.rootfs, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", cfg.rootfs);
        return 1;
    }

    /* Generar nombre de cgroup */
    char cgroup_name[128];
    snprintf(cgroup_name, sizeof(cgroup_name),
             "%s/container-%d", CGROUP_ROOT, getpid());

    /* Determinar si necesitamos cgroups */
    int use_cgroup = (cfg.mem_limit || cfg.cpu_percent > 0 || cfg.pids_limit > 0);

    /* Preparar argumentos para el hijo */
    struct container_args ca = {
        .rootfs = cfg.rootfs,
        .argv = cfg.argv,
    };

    /* Allocar stack */
    char *stack = mmap(NULL, STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap stack");
        return 1;
    }

    int clone_flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS
                    | CLONE_NEWIPC | SIGCHLD;

    /* Crear proceso contenedor */
    pid_t child_pid = clone(child_func, stack + STACK_SIZE,
                            clone_flags, &ca);
    if (child_pid == -1) {
        perror("clone");
        munmap(stack, STACK_SIZE);
        return 1;
    }

    fprintf(stderr, "[parent] container PID: %d\n", child_pid);

    /* Configurar cgroup (despues de clone, antes de que hijo haga exec) */
    if (use_cgroup) {
        if (cgroup_setup(cgroup_name, &cfg, child_pid) == -1) {
            fprintf(stderr, "[parent] cgroup setup failed\n");
            /* Continuar sin cgroup — el contenedor funciona sin limites */
        }
    }

    /* Esperar a que termine */
    int status;
    waitpid(child_pid, &status, 0);

    /* Cleanup cgroup */
    if (use_cgroup) {
        cgroup_destroy(cgroup_name);
    }

    munmap(stack, STACK_SIZE);

    if (WIFEXITED(status)) {
        fprintf(stderr, "[parent] container exited with status %d\n",
                WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[parent] container killed by signal %d\n",
                WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return 1;
}
```

### Flujo completo

```
    $ sudo ./container --mem 268435456 --cpu 50 --pids 64 ./rootfs /bin/sh

    main():
    |-- parse args: mem=268435456, cpu=50, pids=64
    |-- clone(child_func, flags=NEWPID|NEWNS|NEWUTS|NEWIPC)
    |       |
    |       +-- child_func():
    |       |   +-- sethostname("container")
    |       |   +-- setup_root("./rootfs")  [pivot_root]
    |       |   +-- setup_proc()            [mount /proc]
    |       |   +-- execvp("/bin/sh")
    |       |
    |-- cgroup_setup():
    |   +-- mkdir /sys/fs/cgroup/container-PID
    |   +-- write PID to cgroup.procs
    |   +-- write "268435456" to memory.max
    |   +-- write "0" to memory.swap.max
    |   +-- write "50000 100000" to cpu.max
    |   +-- write "64" to pids.max
    |
    |-- waitpid(child_pid)   [espera a que /bin/sh termine]
    |-- cgroup_destroy()     [rmdir cgroup]
    |-- munmap(stack)
    |-- return
```

---

## 12. Verificar que los limites funcionan

### Verificar limite de memoria

```bash
$ sudo ./container --mem 67108864 ./rootfs /bin/sh
[cgroup] memory.max = 67108864
container#

# Dentro del contenedor, intentar usar >64MB:
container# dd if=/dev/zero of=/dev/null bs=1M count=128
# O usar un script que aloca memoria:
container# cat > /tmp/eat_mem.sh << 'EOF'
#!/bin/sh
data=""
i=0
while true; do
    data="${data}$(head -c 1048576 /dev/zero | tr '\0' 'A')"
    i=$((i + 1))
    echo "Allocated ~${i} MB"
done
EOF
container# sh /tmp/eat_mem.sh
Allocated ~1 MB
Allocated ~2 MB
...
Allocated ~63 MB
Killed          # <-- OOM killer
```

### Verificar limite de CPU

```bash
$ sudo ./container --cpu 25 ./rootfs /bin/sh
[cgroup] cpu.max = 25000 100000
container#

# Dentro del contenedor, burn CPU:
container# while true; do :; done &
# En otra terminal del host:
$ top -p $(pgrep -f "bin/sh")
# Deberia mostrar ~25% CPU
```

### Verificar limite de PIDs

```bash
$ sudo ./container --pids 10 ./rootfs /bin/sh
[cgroup] pids.max = 10
container#

# Intentar crear muchos procesos:
container# for i in $(seq 1 20); do sleep 100 & echo "started $i"; done
started 1
started 2
...
started 8
sh: can't fork: Resource temporarily unavailable  # <-- EAGAIN
```

### Verificar desde el host

```bash
# Mientras el contenedor corre:
$ cat /sys/fs/cgroup/container-*/memory.max
67108864

$ cat /sys/fs/cgroup/container-*/memory.current
4321768

$ cat /sys/fs/cgroup/container-*/pids.current
2

$ cat /sys/fs/cgroup/container-*/cpu.max
25000 100000
```

---

## 13. Errores comunes

### Error 1: controladores no disponibles en el cgroup

```bash
# Error al escribir memory.max:
$ echo 268435456 > /sys/fs/cgroup/container-X/memory.max
bash: memory.max: No such file or directory
```

Los controladores deben estar habilitados en el cgroup padre.
Verifica:

```bash
# Que controladores estan disponibles
$ cat /sys/fs/cgroup/cgroup.controllers
cpuset cpu io memory hugetlb pids

# Que controladores estan activos para hijos
$ cat /sys/fs/cgroup/cgroup.subtree_control
cpu memory pids
```

Si falta un controlador en `subtree_control`:

```bash
# Habilitar memory y cpu para sub-cgroups
$ echo "+memory +cpu +pids" > /sys/fs/cgroup/cgroup.subtree_control
```

En codigo C:

```c
static int cgroup_enable_controllers(void)
{
    return write_file(CGROUP_ROOT "/cgroup.subtree_control",
                      "+memory +cpu +pids");
}
```

### Error 2: rmdir del cgroup falla con EBUSY

```c
// MAL: intentar rmdir antes de que el contenedor termine
cgroup_destroy(cgroup_path);  // EBUSY: cgroup has processes
waitpid(child_pid, &status, 0);
```

```c
// BIEN: esperar a que termine, luego limpiar
waitpid(child_pid, &status, 0);
cgroup_destroy(cgroup_path);  // OK: cgroup vacio
```

**Regla**: un cgroup no se puede eliminar si tiene procesos
o sub-cgroups. Siempre hacer `waitpid()` primero.

### Error 3: memory.max en bytes, no en MB

```c
// MAL: cgroups v2 NO acepta sufijos
write_file(mem_path, "256M");    // Error silencioso o -EINVAL

// BIEN: siempre bytes como string numerico
write_file(mem_path, "268435456");  // 256 * 1024 * 1024
```

Si quieres aceptar "256M" en la CLI, convierte en el parser:

```c
static long parse_size(const char *s)
{
    char *end;
    long val = strtol(s, &end, 10);
    switch (*end) {
        case 'K': case 'k': val *= 1024; break;
        case 'M': case 'm': val *= 1024 * 1024; break;
        case 'G': case 'g': val *= 1024 * 1024 * 1024; break;
    }
    return val;
}
```

### Error 4: cpu.max con formato incorrecto

```c
// MAL: solo un numero
write_file(cpu_path, "50000");
// Error: formato es "$MAX $PERIOD"

// MAL: porcentaje directo
write_file(cpu_path, "50%");
// Error: no acepta porcentajes

// BIEN: microsegundos de CPU por periodo
write_file(cpu_path, "50000 100000");  // 50% de 1 CPU
```

### Error 5: no limpiar cgroup al recibir senal

```c
// MAL: si el padre recibe SIGINT, el cgroup queda huerfano
// /sys/fs/cgroup/container-12345/ persiste hasta reboot
```

```c
// BIEN: instalar signal handler que limpia
static volatile sig_atomic_t g_child_pid = 0;
static char g_cgroup_path[512];

static void cleanup_handler(int sig)
{
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGKILL);
        waitpid(g_child_pid, NULL, 0);
    }
    if (g_cgroup_path[0]) {
        rmdir(g_cgroup_path);
    }
    _exit(128 + sig);
}

/* En main(), antes de clone(): */
struct sigaction sa = {.sa_handler = cleanup_handler};
sigaction(SIGINT, &sa, NULL);
sigaction(SIGTERM, &sa, NULL);
```

---

## 14. Cheatsheet

```
CGROUPS v2 — REFERENCIA RAPIDA
================================

Raiz: /sys/fs/cgroup/

Verificar v2:
  mount | grep cgroup2
  stat -f -c %T /sys/fs/cgroup  # "cgroup2fs"

Crear cgroup:
  mkdir /sys/fs/cgroup/my-container

Asignar proceso:
  echo $PID > /sys/fs/cgroup/my-container/cgroup.procs
  echo 0 > .../cgroup.procs   # asignarse a si mismo

Limitar memoria:
  echo 268435456 > memory.max    # 256 MB (en bytes, NO sufijos)
  echo 0 > memory.swap.max      # sin swap
  cat memory.current             # uso actual

Limitar CPU:
  echo "50000 100000" > cpu.max  # 50% de 1 CPU
  echo "200000 100000" > cpu.max # 2 CPUs completos
  echo "max 100000" > cpu.max    # sin limite
  Formato: "$MAX_USEC $PERIOD_USEC"
  Porcentaje = MAX / PERIOD * 100

Limitar PIDs:
  echo 64 > pids.max             # maximo 64 procesos
  cat pids.current               # procesos actuales

Controladores:
  cat cgroup.controllers         # disponibles
  cat cgroup.subtree_control     # activos para hijos
  echo "+memory +cpu +pids" > cgroup.subtree_control

Destruir cgroup:
  rmdir /sys/fs/cgroup/my-container  # debe estar vacio

Desde C:
  mkdir()        crear cgroup
  open()+write() escribir limites
  open()+read()  leer estadisticas
  rmdir()        destruir cgroup

Consecuencias de exceder limites:
  memory.max  --> OOM killer (SIGKILL)
  cpu.max     --> throttling (proceso mas lento, no muere)
  pids.max    --> fork() retorna EAGAIN
```

---

## 15. Ejercicios

### Ejercicio 1: helper parse_size con sufijos

Implementa una funcion que acepte tamanos con sufijos humanos
en la linea de comandos:

```c
/* Parsear strings como "256M", "1G", "512K", "67108864" */
static long parse_size(const char *s)
{
    // TODO:
    // 1. Parsear el numero con strtol()
    // 2. Verificar el caracter siguiente: K, M, G (case insensitive)
    // 3. Multiplicar por el factor apropiado
    // 4. Retornar el valor en bytes
    // 5. Si no hay sufijo, retornar el numero tal cual
}

/* Tests: */
assert(parse_size("256") == 256);
assert(parse_size("1K") == 1024);
assert(parse_size("1k") == 1024);
assert(parse_size("256M") == 268435456);
assert(parse_size("1G") == 1073741824);
```

Tareas:
1. Implementa `parse_size()`
2. Integra en el parser de argumentos de `main()`
3. Ahora puedes escribir: `sudo ./container --mem 256M ./rootfs /bin/sh`
4. Agrega manejo de error para inputs invalidos (`"abc"`, `"0"`, negativos)

**Prediccion**: que pasa si el usuario escribe `--mem 256m`
(minuscula)? Tu implementacion lo acepta?

> **Pregunta de reflexion**: Docker acepta formatos como
> `--memory 256m`, `--memory 1g`, `--memory 134217728` (bytes
> sin sufijo). Investiga: Docker usa cgroups v1 o v2? Como
> detecta cual version usar? (Pista: Docker lo decidio hace
> poco como default — `docker info | grep "Cgroup Version"`.)

### Ejercicio 2: mostrar estadisticas al salir

Modifica el programa para que al terminar el contenedor,
muestre un resumen de los recursos usados:

```c
static void cgroup_print_summary(const char *cgroup_path)
{
    // TODO: leer e imprimir:
    // 1. memory.peak  — uso maximo de memoria alcanzado
    // 2. cpu.stat     — tiempo total de CPU usado (usage_usec)
    //                    y veces throttleado (nr_throttled)
    // 3. pids.current — PIDs al momento del exit (deberia ser 0)
    //
    // Formato sugerido:
    // [stats] peak memory: 12.3 MB
    // [stats] CPU time:    1.234 s
    // [stats] throttled:   15 times
}
```

Output esperado:

```bash
$ sudo ./container --mem 256M --cpu 50 --pids 64 ./rootfs /bin/sh
[container] ...
container# exit
[stats] peak memory: 3.2 MB
[stats] CPU time:    0.045 s
[stats] throttled:   0 times
[parent] container exited with status 0
[cgroup] removed: /sys/fs/cgroup/container-12345
```

Tareas:
1. Implementa `cgroup_print_summary()`
2. Llamala entre `waitpid()` y `cgroup_destroy()`
3. Ejecuta un workload que consuma memoria y CPU para verificar
   que los numeros tienen sentido
4. Ejecuta con un limite de CPU bajo (5%) y verifica que
   `nr_throttled` es mayor que cero

> **Pregunta de reflexion**: `memory.peak` muestra el maximo
> historico de memoria usada. `memory.current` muestra el uso
> en este instante. Despues de `waitpid()`, `memory.current`
> deberia ser cercano a 0 (el proceso termino). Pero
> `memory.peak` retiene el maximo. Por que es util esta
> distincion? Como usarias `memory.peak` para decidir el
> limite de memoria adecuado para un contenedor en produccion?

### Ejercicio 3: fork bomb defense

Verifica que el limite de PIDs defiende contra una fork bomb:

```bash
# PELIGRO: solo ejecutar DENTRO del contenedor con pids.max
# NUNCA ejecutar en el host sin proteccion

$ sudo ./container --pids 20 ./rootfs /bin/sh
container# :(){ :|:& };:
```

Tareas:
1. Ejecuta el contenedor con `--pids 20`
2. Dentro del contenedor, ejecuta la fork bomb
3. Observa que el contenedor se satura pero el host sigue
   funcionando
4. Desde otra terminal del host, verifica:
   `cat /sys/fs/cgroup/container-*/pids.current` (deberia
   mostrar 20 o cercano)
5. Mata el contenedor desde el host si se queda colgado:
   `sudo kill -9 $(cat /sys/fs/cgroup/container-*/cgroup.procs)`
6. Repite sin `--pids` y observa la diferencia (**en una VM
   o con otro cgroup protegiendo el host**)

> **Pregunta de reflexion**: la fork bomb `:(){ :|:& };:`
> define una funcion `:` que se llama a si misma dos veces
> (via pipe) en background. Sin limite de PIDs, esto crece
> exponencialmente: 1 -> 2 -> 4 -> 8 -> ... -> 2^20 en
> segundos, consumiendo toda la tabla de procesos del kernel.
> Con `pids.max = 20`, solo puede crear 20 procesos. El
> contenedor se vuelve inutilizable (no puede hacer fork
> para ejecutar comandos), pero el host sigue intacto. En
> produccion, cual seria un valor razonable para `pids.max`?
> Docker usa `--pids-limit` — cual es su default?
