# Límites de Recursos — getrlimit, setrlimit y ulimit

## Índice

1. [Qué son los resource limits](#1-qué-son-los-resource-limits)
2. [Soft limit vs hard limit](#2-soft-limit-vs-hard-limit)
3. [getrlimit: consultar límites](#3-getrlimit-consultar-límites)
4. [setrlimit: modificar límites](#4-setrlimit-modificar-límites)
5. [Recursos principales](#5-recursos-principales)
6. [ulimit: la interfaz del shell](#6-ulimit-la-interfaz-del-shell)
7. [Herencia en fork y exec](#7-herencia-en-fork-y-exec)
8. [prlimit: consultar/modificar otro proceso](#8-prlimit-consultarmodificar-otro-proceso)
9. [Configuración persistente](#9-configuración-persistente)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué son los resource limits

El kernel impone **límites** sobre los recursos que cada proceso puede
consumir. Esto evita que un solo proceso monopolice CPU, memoria, disco
o file descriptors, protegiendo al sistema y a otros procesos:

```
  Proceso intenta usar un recurso
           │
           ▼
  ┌────────────────────┐
  │ ¿Excede el límite? │
  │                    │
  │  No ──▶ permitir   │
  │                    │
  │  Sí ──▶ denegar    │
  │         │          │
  │    ┌────┴────┐     │
  │    │ RLIMIT_ │     │
  │    │ NOFILE  │ → EMFILE (too many open files)
  │    │ FSIZE   │ → SIGXFSZ (file size exceeded)
  │    │ CPU     │ → SIGXCPU (CPU time exceeded)
  │    │ AS      │ → ENOMEM (out of memory)
  │    │ NPROC   │ → EAGAIN (fork fails)
  │    │ CORE    │ → core dump truncated/disabled
  │    └─────────┘     │
  └────────────────────┘
```

Cada recurso tiene dos límites:

- **Soft limit**: el límite activo que el kernel aplica.
- **Hard limit**: el techo máximo hasta donde un proceso no privilegiado
  puede subir su soft limit.

---

## 2. Soft limit vs hard limit

```
  0                    soft                  hard              ∞
  ├────────────────────┤─────────────────────┤
  │   uso permitido    │   zona ampliable    │  prohibido
  │                    │   (sin root)        │  (solo root)
  │                    │                     │
  │ El kernel aplica   │ setrlimit puede     │ Solo root puede
  │ el soft limit      │ subir soft hasta    │ subir el hard
  │                    │ aquí                │ limit
```

```c
#include <sys/resource.h>

struct rlimit {
    rlim_t rlim_cur;  /* Soft limit (current) */
    rlim_t rlim_max;  /* Hard limit (ceiling) */
};
```

### Reglas

| Operación                          | ¿Quién puede? |
|------------------------------------|----------------|
| Bajar soft limit                   | Cualquier proceso |
| Subir soft limit ≤ hard limit      | Cualquier proceso |
| Subir soft limit > hard limit      | Solo root (CAP_SYS_RESOURCE) |
| Bajar hard limit                   | Cualquier proceso (irreversible) |
| Subir hard limit                   | Solo root (CAP_SYS_RESOURCE) |

> **Clave**: Bajar el hard limit es **irreversible** sin privilegios.
> Un proceso no privilegiado no puede recuperar un hard limit que bajó.

### El valor especial RLIM_INFINITY

```c
#include <sys/resource.h>

/* RLIM_INFINITY means "no limit" */
struct rlimit rl = {
    .rlim_cur = RLIM_INFINITY,
    .rlim_max = RLIM_INFINITY,
};
```

`RLIM_INFINITY` significa que no hay límite. El proceso puede usar todo
lo que el sistema permita.

---

## 3. getrlimit: consultar límites

```c
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim);
```

- `resource`: constante `RLIMIT_*` que identifica el recurso.
- `rlim`: estructura donde se almacenan soft y hard limits.
- Retorna 0 en éxito, -1 en error.

### Ejemplo: consultar todos los límites

```c
#include <stdio.h>
#include <sys/resource.h>

typedef struct {
    int         resource;
    const char *name;
    const char *unit;
} rlimit_info_t;

static const rlimit_info_t resources[] = {
    {RLIMIT_NOFILE,  "NOFILE",  "descriptors"},
    {RLIMIT_NPROC,   "NPROC",   "processes"},
    {RLIMIT_FSIZE,   "FSIZE",   "bytes"},
    {RLIMIT_DATA,    "DATA",    "bytes"},
    {RLIMIT_STACK,   "STACK",   "bytes"},
    {RLIMIT_CORE,    "CORE",    "bytes"},
    {RLIMIT_AS,      "AS",      "bytes"},
    {RLIMIT_CPU,     "CPU",     "seconds"},
    {RLIMIT_MEMLOCK, "MEMLOCK", "bytes"},
    {RLIMIT_LOCKS,   "LOCKS",   "locks"},
    {RLIMIT_MSGQUEUE,"MSGQUEUE","bytes"},
};

static void print_limit(rlim_t val)
{
    if (val == RLIM_INFINITY)
        printf("%15s", "unlimited");
    else
        printf("%15lu", (unsigned long)val);
}

int main(void)
{
    printf("%-10s %15s %15s  %s\n",
           "Resource", "Soft", "Hard", "Unit");
    printf("%-10s %15s %15s  %s\n",
           "--------", "----", "----", "----");

    for (size_t i = 0; i < sizeof(resources) / sizeof(resources[0]); i++) {
        struct rlimit rl;
        if (getrlimit(resources[i].resource, &rl) == -1) {
            perror(resources[i].name);
            continue;
        }

        printf("%-10s", resources[i].name);
        print_limit(rl.rlim_cur);
        print_limit(rl.rlim_max);
        printf("  %s\n", resources[i].unit);
    }

    return 0;
}
```

```
  $ ./show_limits
  Resource            Soft            Hard  Unit
  --------            ----            ----  ----
  NOFILE              1024         1048576  descriptors
  NPROC              63204           63204  processes
  FSIZE          unlimited       unlimited  bytes
  DATA           unlimited       unlimited  bytes
  STACK            8388608       unlimited  bytes
  CORE                   0       unlimited  bytes
  AS             unlimited       unlimited  bytes
  CPU            unlimited       unlimited  seconds
  MEMLOCK           524288          524288  bytes
  LOCKS          unlimited       unlimited  locks
  MSGQUEUE          819200          819200  bytes
```

---

## 4. setrlimit: modificar límites

```c
#include <sys/resource.h>

int setrlimit(int resource, const struct rlimit *rlim);
```

### Subir un soft limit (no privilegiado)

```c
#include <stdio.h>
#include <sys/resource.h>

int increase_open_files(rlim_t desired)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        return -1;
    }

    printf("before: soft=%lu, hard=%lu\n",
           (unsigned long)rl.rlim_cur,
           (unsigned long)rl.rlim_max);

    if (desired > rl.rlim_max) {
        fprintf(stderr, "requested %lu exceeds hard limit %lu\n",
                (unsigned long)desired,
                (unsigned long)rl.rlim_max);
        /* Use hard limit as maximum */
        desired = rl.rlim_max;
    }

    rl.rlim_cur = desired;

    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        return -1;
    }

    printf("after: soft=%lu\n", (unsigned long)rl.rlim_cur);
    return 0;
}
```

### Bajar un límite (auto-restricción)

```c
#include <sys/resource.h>

/* Restrict this process to 100MB of virtual memory */
void limit_memory(void)
{
    struct rlimit rl = {
        .rlim_cur = 100 * 1024 * 1024,  /* 100 MiB soft */
        .rlim_max = 100 * 1024 * 1024,  /* 100 MiB hard (irreversible) */
    };

    if (setrlimit(RLIMIT_AS, &rl) == -1)
        perror("setrlimit RLIMIT_AS");
}

/* Restrict CPU time to 60 seconds */
void limit_cpu(void)
{
    struct rlimit rl = {
        .rlim_cur = 55,   /* Soft: SIGXCPU at 55s (warning) */
        .rlim_max = 60,   /* Hard: SIGKILL at 60s (final) */
    };

    if (setrlimit(RLIMIT_CPU, &rl) == -1)
        perror("setrlimit RLIMIT_CPU");
}
```

### Deshabilitar core dumps

```c
/* Prevent core dumps (security: they may contain secrets) */
struct rlimit rl = { .rlim_cur = 0, .rlim_max = 0 };
setrlimit(RLIMIT_CORE, &rl);
```

### Habilitar core dumps

```c
/* Enable core dumps for debugging */
struct rlimit rl;
getrlimit(RLIMIT_CORE, &rl);
rl.rlim_cur = rl.rlim_max;  /* Set soft to hard limit */
setrlimit(RLIMIT_CORE, &rl);
```

---

## 5. Recursos principales

### RLIMIT_NOFILE — File descriptors

Máximo de file descriptors que el proceso puede tener abiertos
simultáneamente. Cuando se excede, `open`, `socket`, `accept`, etc.
fallan con `EMFILE`:

```c
/* Typical default: soft=1024, hard=1048576 */
struct rlimit rl;
getrlimit(RLIMIT_NOFILE, &rl);
```

Servidores de red con muchas conexiones simultáneas necesitan subir este
límite. Cada conexión TCP consume un fd.

### RLIMIT_NPROC — Procesos por usuario

Máximo de procesos (y threads, en Linux) que el **UID real** del proceso
puede tener. `fork()` falla con `EAGAIN` si se excede:

```c
/* Protection against fork bombs */
struct rlimit rl = {
    .rlim_cur = 100,
    .rlim_max = 200,
};
setrlimit(RLIMIT_NPROC, &rl);
```

> **Nota**: RLIMIT_NPROC cuenta por **UID**, no por proceso. Todos los
> procesos del mismo usuario comparten este contador.

### RLIMIT_FSIZE — Tamaño máximo de archivo

Máximo tamaño de archivo que el proceso puede crear o ampliar. Si se
excede, `write` falla y el proceso recibe `SIGXFSZ`:

```c
/* Limit to 1 GiB files */
struct rlimit rl = {
    .rlim_cur = 1024UL * 1024 * 1024,
    .rlim_max = 1024UL * 1024 * 1024,
};
setrlimit(RLIMIT_FSIZE, &rl);
```

### RLIMIT_CPU — Tiempo de CPU

Segundos de CPU (no wall-clock) que el proceso puede consumir. Cuando
alcanza el soft limit, recibe `SIGXCPU`. Si lo ignora y alcanza el
hard limit, recibe `SIGKILL`:

```
  t=0           soft (SIGXCPU)        hard (SIGKILL)
  ├──────────────────┤──────────────────┤
  │  ejecución OK    │ gracia (handle   │ muere
  │                  │  SIGXCPU, cleanup│ incondicionalmente
  │                  │  y salir)        │
```

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

void sigxcpu_handler(int sig)
{
    (void)sig;
    const char msg[] = "CPU limit approaching, saving state...\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
}

int main(void)
{
    signal(SIGXCPU, sigxcpu_handler);

    struct rlimit rl = {
        .rlim_cur = 5,   /* SIGXCPU after 5 seconds of CPU */
        .rlim_max = 10,  /* SIGKILL after 10 seconds of CPU */
    };
    setrlimit(RLIMIT_CPU, &rl);

    /* CPU-intensive work */
    volatile long sum = 0;
    for (;;) sum++;

    return 0;
}
```

### RLIMIT_AS — Address space (memoria virtual)

Máximo de memoria virtual (mmap, malloc, stack combinados). `mmap` y
`brk` fallan con `ENOMEM`:

```c
/* Limit to 512 MiB of virtual memory */
struct rlimit rl = {
    .rlim_cur = 512UL * 1024 * 1024,
    .rlim_max = 512UL * 1024 * 1024,
};
setrlimit(RLIMIT_AS, &rl);
```

### RLIMIT_STACK — Tamaño del stack

Tamaño máximo del stack del thread principal. Si se excede, el proceso
recibe `SIGSEGV`:

```c
/* Default is typically 8 MiB */
struct rlimit rl;
getrlimit(RLIMIT_STACK, &rl);
/* rl.rlim_cur = 8388608 (8 MiB) */
```

### RLIMIT_CORE — Core dumps

Tamaño máximo del archivo core dump. Si es 0, no se generan core dumps:

```c
struct rlimit rl;
getrlimit(RLIMIT_CORE, &rl);
/* rl.rlim_cur = 0 → core dumps disabled (common default) */
```

### RLIMIT_MEMLOCK — Páginas bloqueadas en RAM

Máximo de bytes que pueden ser bloqueados en RAM con `mlock`/`mlockall`
(evitando que se envíen a swap):

```c
struct rlimit rl;
getrlimit(RLIMIT_MEMLOCK, &rl);
/* Typical: 524288 (512 KiB) for non-root */
```

### Tabla resumen

| Recurso          | Qué limita                    | Señal/Error al exceder     |
|------------------|-------------------------------|----------------------------|
| `RLIMIT_NOFILE`  | File descriptors abiertos     | `EMFILE`                   |
| `RLIMIT_NPROC`   | Procesos por UID              | `EAGAIN` en fork           |
| `RLIMIT_FSIZE`   | Tamaño de archivo             | `SIGXFSZ` + `EFBIG`       |
| `RLIMIT_CPU`     | Tiempo de CPU (segundos)      | `SIGXCPU` → `SIGKILL`     |
| `RLIMIT_AS`      | Memoria virtual total         | `ENOMEM`                   |
| `RLIMIT_DATA`    | Segmento de datos (brk/sbrk) | `ENOMEM`                   |
| `RLIMIT_STACK`   | Tamaño del stack              | `SIGSEGV`                  |
| `RLIMIT_CORE`    | Core dump                     | Truncado o no generado     |
| `RLIMIT_MEMLOCK` | Páginas bloqueadas (mlock)    | `ENOMEM` en mlock          |
| `RLIMIT_LOCKS`   | Locks de archivo (flock)      | `ENOLCK`                   |
| `RLIMIT_MSGQUEUE`| Bytes en colas POSIX          | `ENOMEM` en mq_open       |
| `RLIMIT_SIGPENDING` | Señales pendientes         | `EAGAIN` en sigqueue       |
| `RLIMIT_NICE`    | Rango de nice permitido       | `EPERM`                    |
| `RLIMIT_RTPRIO`  | Prioridad real-time máxima    | `EPERM`                    |

---

## 6. ulimit: la interfaz del shell

`ulimit` es un **builtin del shell** que llama a `getrlimit`/`setrlimit`:

```bash
# Show all limits
ulimit -a

# Specific limits
ulimit -n        # NOFILE (soft)
ulimit -Hn       # NOFILE (hard)
ulimit -u        # NPROC
ulimit -f        # FSIZE (in 512-byte blocks)
ulimit -c        # CORE
ulimit -v        # AS (in KiB)
ulimit -s        # STACK (in KiB)
ulimit -t        # CPU (seconds)
ulimit -l        # MEMLOCK (in KiB)
```

### Modificar desde el shell

```bash
# Set soft limit
ulimit -n 65536         # Raise open files to 65536

# Set hard limit (may require root)
ulimit -Hn 1048576

# Set both soft and hard
ulimit -Sn 65536 -Hn 1048576

# Disable core dumps
ulimit -c 0

# Unlimited
ulimit -c unlimited
```

### Por qué ulimit es builtin

Al igual que `cd`, `ulimit` **debe** ser un builtin. Si fuera un programa
externo, `setrlimit` se ejecutaría en el hijo (fork+exec), y al terminar,
el cambio se perdería. El shell necesita ejecutar `setrlimit` en su propio
proceso para que los hijos (programas que lance después) hereden el nuevo
límite.

---

## 7. Herencia en fork y exec

Los resource limits se **heredan** tanto en `fork` como en `exec`:

```c
/* Set limit in parent */
struct rlimit rl = {
    .rlim_cur = 256,
    .rlim_max = 1024,
};
setrlimit(RLIMIT_NOFILE, &rl);

pid_t pid = fork();
if (pid == 0) {
    /* Child inherits: soft=256, hard=1024 */

    /* Child can raise soft up to 1024 */
    rl.rlim_cur = 512;
    setrlimit(RLIMIT_NOFILE, &rl);

    /* exec also inherits limits */
    execvp("my_server", argv);
    /* my_server starts with NOFILE soft=512, hard=1024 */
    _exit(127);
}
```

### Patrón: restringir hijos antes de exec

```c
if (pid == 0) {
    /* Restrict child before exec */
    struct rlimit rl;

    /* Max 50 MiB memory */
    rl = (struct rlimit){50 * 1024 * 1024, 50 * 1024 * 1024};
    setrlimit(RLIMIT_AS, &rl);

    /* Max 10 seconds CPU */
    rl = (struct rlimit){10, 15};
    setrlimit(RLIMIT_CPU, &rl);

    /* Max 10 MiB file size */
    rl = (struct rlimit){10 * 1024 * 1024, 10 * 1024 * 1024};
    setrlimit(RLIMIT_FSIZE, &rl);

    /* No core dumps */
    rl = (struct rlimit){0, 0};
    setrlimit(RLIMIT_CORE, &rl);

    /* No fork (single-process) */
    rl = (struct rlimit){0, 0};
    setrlimit(RLIMIT_NPROC, &rl);

    execvp("untrusted_program", argv);
    _exit(127);
}
```

Este patrón es la base de sandboxing básico: el programa hijo hereda
los límites y no puede subirlos (el hard limit lo impide).

---

## 8. prlimit: consultar/modificar otro proceso

`prlimit` (Linux 2.6.36+) permite leer y modificar los límites de
**otro proceso** (requiere permisos adecuados):

```c
#define _GNU_SOURCE
#include <sys/resource.h>

int prlimit(pid_t pid, int resource,
            const struct rlimit *new_limit,
            struct rlimit *old_limit);
```

- `pid`: proceso objetivo (0 = proceso actual).
- `new_limit`: nuevo límite a aplicar (NULL para solo leer).
- `old_limit`: almacena el límite anterior (NULL si no interesa).

### Leer límites de otro proceso

```c
#include <stdio.h>
#include <sys/resource.h>

void show_process_limits(pid_t pid)
{
    struct rlimit rl;

    if (prlimit(pid, RLIMIT_NOFILE, NULL, &rl) == 0) {
        printf("PID %d NOFILE: soft=%lu, hard=%lu\n",
               pid, (unsigned long)rl.rlim_cur,
               (unsigned long)rl.rlim_max);
    }

    if (prlimit(pid, RLIMIT_AS, NULL, &rl) == 0) {
        if (rl.rlim_cur == RLIM_INFINITY)
            printf("PID %d AS: unlimited\n", pid);
        else
            printf("PID %d AS: %lu MiB\n",
                   pid, (unsigned long)(rl.rlim_cur / (1024 * 1024)));
    }
}
```

### Modificar límites de otro proceso

```c
/* Raise open files limit for PID 1234 */
struct rlimit new_rl = { .rlim_cur = 65536, .rlim_max = 65536 };
struct rlimit old_rl;

if (prlimit(1234, RLIMIT_NOFILE, &new_rl, &old_rl) == -1) {
    perror("prlimit");
} else {
    printf("changed from %lu to %lu\n",
           (unsigned long)old_rl.rlim_cur,
           (unsigned long)new_rl.rlim_cur);
}
```

### Comando prlimit

También existe como comando de línea:

```bash
# Show limits of PID 1234
prlimit --pid 1234

# Set NOFILE for PID 1234
prlimit --pid 1234 --nofile=65536:65536

# Run command with modified limits
prlimit --nofile=4096 --as=536870912 -- ./my_server
```

---

## 9. Configuración persistente

### /etc/security/limits.conf

En sistemas con PAM, los límites se configuran persistentemente en
`/etc/security/limits.conf`. Se aplican al hacer login:

```
# /etc/security/limits.conf
# <domain>  <type>  <item>   <value>
*            soft    nofile   4096
*            hard    nofile   1048576
www-data     soft    nofile   65536
www-data     hard    nofile   65536
@developers  soft    nproc    4096
root         soft    nofile   1048576
```

| Campo    | Significado                                   |
|----------|-----------------------------------------------|
| domain   | Usuario, grupo (@grupo), o `*` (todos)        |
| type     | `soft`, `hard`, o `-` (ambos)                 |
| item     | `nofile`, `nproc`, `fsize`, `core`, `as`, etc.|
| value    | Número o `unlimited`                          |

### /etc/security/limits.d/

Archivos adicionales en este directorio (ej: `90-custom.conf`) se cargan
después de `limits.conf`:

```
# /etc/security/limits.d/90-myapp.conf
myapp  soft  nofile  65536
myapp  hard  nofile  65536
myapp  soft  memlock unlimited
myapp  hard  memlock unlimited
```

### systemd service units

Para servicios gestionados por systemd, los límites se configuran en la
unit file:

```ini
# /etc/systemd/system/myapp.service
[Service]
LimitNOFILE=65536
LimitNPROC=4096
LimitCORE=infinity
LimitAS=infinity
LimitMEMLOCK=infinity
LimitCPU=infinity
```

systemd usa `prlimit` / `setrlimit` antes de exec para aplicarlos.

### Verificar límites aplicados

```bash
# Limits of a running process
cat /proc/$(pidof myapp)/limits

# Output:
# Limit                     Soft Limit  Hard Limit  Units
# Max open files            65536       65536       files
# Max processes             63204       63204       processes
# Max file size             unlimited   unlimited   bytes
# ...
```

---

## 10. Errores comunes

### Error 1: Subir soft sin verificar hard

```c
/* MAL: puede fallar si desired > hard limit */
struct rlimit rl = { .rlim_cur = 100000, .rlim_max = 100000 };
if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
    perror("setrlimit");  /* EPERM if hard was lower */
}

/* BIEN: leer hard limit primero */
struct rlimit rl;
getrlimit(RLIMIT_NOFILE, &rl);

rlim_t desired = 100000;
if (desired > rl.rlim_max)
    desired = rl.rlim_max;  /* Cap at hard limit */

rl.rlim_cur = desired;
if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
    perror("setrlimit");
```

### Error 2: Confundir RLIMIT_AS con memoria física

```c
/* RLIMIT_AS limita memoria VIRTUAL, no física */
/* Un proceso puede tener 10 GiB de virtual mapping
   pero solo usar 100 MiB de RAM física (demand paging) */

/* Si necesitas limitar memoria residente (RSS), usa cgroups:
   /sys/fs/cgroup/memory/myapp/memory.limit_in_bytes */
```

`RLIMIT_AS` limita el espacio de direcciones virtuales total. Un programa
que usa `mmap` con archivos grandes puede necesitar mucho address space
pero poco RAM. Usar `RLIMIT_AS` demasiado bajo puede romper programas
que hacen uso legítimo de mapeos.

### Error 3: Bajar hard limit accidentalmente

```c
/* MAL: baja el hard limit — no se puede recuperar */
struct rlimit rl = {
    .rlim_cur = 1024,
    .rlim_max = 1024,  /* Was 1048576, now permanently 1024 */
};
setrlimit(RLIMIT_NOFILE, &rl);
/* Process can NEVER raise above 1024 again (without CAP_SYS_RESOURCE) */

/* BIEN: solo modificar soft, mantener hard */
struct rlimit rl;
getrlimit(RLIMIT_NOFILE, &rl);
rl.rlim_cur = 1024;          /* Only change soft */
/* rl.rlim_max unchanged */
setrlimit(RLIMIT_NOFILE, &rl);
```

### Error 4: No manejar SIGXCPU/SIGXFSZ

```c
/* MAL: sin handler, SIGXCPU mata el proceso sin cleanup */
setrlimit(RLIMIT_CPU, &(struct rlimit){10, 20});
/* ... CPU-intensive work — killed at 10s without warning ... */

/* BIEN: handler para hacer cleanup antes del hard limit */
void sigxcpu_handler(int sig) {
    (void)sig;
    /* Async-signal-safe cleanup */
    const char msg[] = "CPU limit reached, shutting down\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
}

struct sigaction sa = { .sa_handler = sigxcpu_handler };
sigaction(SIGXCPU, &sa, NULL);

struct rlimit rl = {
    .rlim_cur = 10,  /* SIGXCPU at 10s → handler runs cleanup */
    .rlim_max = 15,  /* SIGKILL at 15s → unconditional death */
};
setrlimit(RLIMIT_CPU, &rl);
```

La diferencia de 5 segundos entre soft y hard es la ventana de gracia
para hacer cleanup.

### Error 5: Asumir que los límites son iguales en todos los sistemas

```c
/* MAL: asumir que el default de NOFILE es 1024 */
/* In containers, cloud instances, or custom configs the default may differ */

/* BIEN: siempre consultar con getrlimit */
struct rlimit rl;
getrlimit(RLIMIT_NOFILE, &rl);
printf("current limit: %lu\n", (unsigned long)rl.rlim_cur);

/* Adjust based on what we actually need */
if (rl.rlim_cur < needed) {
    rl.rlim_cur = (needed < rl.rlim_max) ? needed : rl.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rl);
}
```

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │               Límites de Recursos                            │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  struct rlimit { rlim_cur (soft), rlim_max (hard) }         │
  │  RLIM_INFINITY = sin límite                                  │
  │                                                              │
  │  Consultar:                                                  │
  │    getrlimit(RLIMIT_NOFILE, &rl)                            │
  │    prlimit(pid, RLIMIT_NOFILE, NULL, &rl)   otro proceso   │
  │    cat /proc/PID/limits                      desde shell    │
  │    ulimit -a                                 shell builtin  │
  │                                                              │
  │  Modificar:                                                  │
  │    setrlimit(RLIMIT_NOFILE, &rl)                            │
  │    prlimit(pid, RLIMIT_NOFILE, &new, &old)  otro proceso   │
  │    ulimit -n 65536                           shell          │
  │                                                              │
  │  Reglas:                                                     │
  │    soft ≤ hard                siempre                        │
  │    subir soft ≤ hard          cualquier proceso              │
  │    subir hard                 solo root/CAP_SYS_RESOURCE    │
  │    bajar hard                 cualquiera (¡irreversible!)   │
  │                                                              │
  │  Recursos clave:                                             │
  │    NOFILE   fds abiertos     → EMFILE                       │
  │    NPROC    procesos/UID     → EAGAIN en fork               │
  │    FSIZE    tamaño archivo   → SIGXFSZ                      │
  │    CPU      tiempo CPU       → SIGXCPU → SIGKILL            │
  │    AS       memoria virtual  → ENOMEM                       │
  │    STACK    tamaño stack     → SIGSEGV                      │
  │    CORE     core dump        → truncado/deshabilitado       │
  │    MEMLOCK  mlock bytes      → ENOMEM                       │
  │                                                              │
  │  Herencia:                                                   │
  │    fork → hereda            exec → hereda                   │
  │    ulimit es builtin (como cd, por la misma razón)          │
  │                                                              │
  │  Persistente:                                                │
  │    /etc/security/limits.conf    (PAM)                       │
  │    /etc/security/limits.d/*.conf                            │
  │    systemd: LimitNOFILE=65536  (unit files)                 │
  │                                                              │
  │  Shell: ulimit -n/-u/-f/-c/-v/-s/-t/-l                      │
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Sandbox de recursos

Escribe un programa que ejecute un comando externo con límites de recursos
estrictos, similar a como lo hace un juez de concursos de programación:

```c
typedef struct {
    int      cpu_secs;       /* Max CPU time */
    size_t   mem_bytes;      /* Max virtual memory */
    size_t   fsize_bytes;    /* Max output file size */
    int      nproc;          /* Max child processes (0 = none) */
} sandbox_limits_t;

int run_sandboxed(const char *cmd, char *const argv[],
                  const sandbox_limits_t *limits);
```

Requisitos:
1. Aplicar todos los límites en el hijo, antes de exec.
2. Desactivar core dumps.
3. Detectar y reportar el motivo de terminación:
   - Exit code normal.
   - `SIGXCPU` → "Time Limit Exceeded".
   - `SIGXFSZ` → "Output Limit Exceeded".
   - `SIGSEGV` → "Runtime Error (segfault)".
   - `SIGKILL` (por RLIMIT_CPU hard) → "Time Limit Exceeded (hard)".
   - `ENOMEM` / `SIGABRT` después de malloc → "Memory Limit Exceeded".

Probar con:
```c
sandbox_limits_t limits = { .cpu_secs = 2, .mem_bytes = 64*1024*1024,
                            .fsize_bytes = 1024*1024, .nproc = 0 };
run_sandboxed("./solution", (char *[]){"./solution", NULL}, &limits);
```

**Predicción antes de codificar**: Si pones `cpu_secs = 2` como soft y
`cpu_secs + 5` como hard, ¿cuánto tiempo tiene el proceso para hacer
cleanup cuando recibe `SIGXCPU`?

**Pregunta de reflexión**: `RLIMIT_AS` limita memoria virtual, no física.
¿Podría un programa malicioso consumir mucha RAM física sin exceder
`RLIMIT_AS`? ¿Qué mecanismo sería más apropiado para limitar RSS?

---

### Ejercicio 2: Monitor de límites

Escribe un programa que monitorice los límites y el uso actual de recursos
de un proceso dado, actualizando cada segundo:

```c
void monitor_resources(pid_t pid);
```

Para cada recurso, muestra:
1. Límite soft y hard (de `/proc/PID/limits`).
2. Uso actual (donde sea posible):
   - NOFILE actual: contar entradas en `/proc/PID/fd/`.
   - Memoria virtual: leer `VmSize` de `/proc/PID/status`.
   - RSS: leer `VmRSS` de `/proc/PID/status`.
   - CPU time: leer `utime + stime` de `/proc/PID/stat`.
   - Threads: leer `Threads` de `/proc/PID/status`.
3. Porcentaje de uso respecto al soft limit.
4. Alertar con `[!]` cuando el uso supere el 80% del soft limit.

```
  PID 1234 (my_server) — Resource Monitor
  ─────────────────────────────────────────
  Resource     Usage        Soft Limit    %
  NOFILE       847          1024          82% [!]
  VM Size      412 MiB      unlimited     -
  RSS          128 MiB      (no rlimit)   -
  CPU Time     34s          unlimited     -
  Threads      12           63204         0%
```

**Predicción antes de codificar**: ¿Qué archivo de `/proc/PID/` usarías
para contar los file descriptors abiertos? ¿Y si el proceso abre y cierra
fds muy rápido, tu lectura será exacta?

**Pregunta de reflexión**: ¿Por qué no existe un `RLIMIT_RSS` para limitar
la memoria física residente? ¿Qué mecanismo de Linux (`cgroups`) lo
reemplaza?

---

### Ejercicio 3: Auto-ajuste de NOFILE para servidor

Escribe una función que un servidor de red pueda llamar al inicio para
asegurar que tiene suficientes file descriptors para el número esperado
de conexiones:

```c
typedef struct {
    int      requested;   /* Desired soft limit */
    int      achieved;    /* Actual limit set */
    int      was_raised;  /* 1 if limit was changed */
    char     message[256];/* Human-readable status */
} nofile_result_t;

nofile_result_t ensure_nofile(int min_fds);
```

Comportamiento:
1. Leer el límite actual.
2. Si soft ≥ `min_fds`, no hacer nada.
3. Si soft < `min_fds` ≤ hard, subir soft a `min_fds`.
4. Si `min_fds` > hard, subir soft al hard limit y avisar que no es
   suficiente.
5. Reservar un margen del 10% sobre `min_fds` para fds internos
   (logging, config files, etc.).
6. Registrar lo que hizo en `message`.

Probar con:
```c
nofile_result_t r = ensure_nofile(10000);
printf("%s\n", r.message);
/* "NOFILE: raised from 1024 to 11000 (requested 10000 + 10% margin)" */

r = ensure_nofile(2000000);
printf("%s\n", r.message);
/* "NOFILE: WARNING: requested 2200000 but hard limit is 1048576.
    Set to 1048576. Run as root or edit limits.conf to increase." */
```

**Predicción antes de codificar**: Si el hard limit es 1048576 y
`min_fds` es 500000, ¿qué valor tendrá `achieved` después de aplicar
el margen del 10%?

**Pregunta de reflexión**: Nginx y HAProxy suben `RLIMIT_NOFILE` al
iniciar. ¿Por qué no dejan simplemente que el administrador lo configure
con `ulimit` o `limits.conf` en vez de hacerlo programáticamente?
