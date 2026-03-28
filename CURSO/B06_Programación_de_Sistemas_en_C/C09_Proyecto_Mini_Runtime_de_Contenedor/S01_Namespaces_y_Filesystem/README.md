# Namespaces y Filesystem

## Indice
1. [Vision general del proyecto](#1-vision-general-del-proyecto)
2. [Prerequisitos](#2-prerequisitos)
3. [clone() vs fork()](#3-clone-vs-fork)
4. [Flags de namespace](#4-flags-de-namespace)
5. [PID namespace (CLONE_NEWPID)](#5-pid-namespace-clone_newpid)
6. [UTS namespace (CLONE_NEWUTS)](#6-uts-namespace-clone_newuts)
7. [Mount namespace (CLONE_NEWNS)](#7-mount-namespace-clone_newns)
8. [IPC namespace (CLONE_NEWIPC)](#8-ipc-namespace-clone_newipc)
9. [Preparar un rootfs minimo](#9-preparar-un-rootfs-minimo)
10. [chroot() vs pivot_root()](#10-chroot-vs-pivot_root)
11. [Montar /proc en el nuevo namespace](#11-montar-proc-en-el-nuevo-namespace)
12. [Construir container.c paso a paso](#12-construir-containerc-paso-a-paso)
13. [Probando el contenedor](#13-probando-el-contenedor)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. Vision general del proyecto

En este capitulo construimos un **mini runtime de contenedor**
en C — un programa que crea un proceso aislado con su propio
PID namespace, filesystem, hostname y limites de recursos.
Esencialmente, lo que Docker y Podman hacen internamente.

```
    Nuestro objetivo: container.c

    $ sudo ./container /bin/sh
    [container] PID namespace: OK
    [container] UTS namespace: OK (hostname: container)
    [container] Mount namespace: OK
    [container] pivot_root: OK
    [container] /proc mounted
    container# ps aux
    PID   USER     COMMAND
        1 root     /bin/sh        <-- PID 1 dentro del contenedor
    container# hostname
    container
    container# ls /
    bin  dev  etc  home  lib  proc  ...   <-- rootfs aislado
```

El proyecto se construye incrementalmente en tres secciones:

```
    S01 (esta seccion):
    +-- clone() con CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC
    +-- Preparar rootfs minimo (Alpine Linux)
    +-- pivot_root() para aislar el filesystem
    +-- Montar /proc
    +-- El proceso hijo arranca con PID 1

    S02 (siguiente):
    +-- Crear cgroup v2
    +-- Limitar memoria, CPU, numero de procesos
    +-- Cleanup del cgroup al salir

    S03 (final):
    +-- CLONE_NEWNET para aislamiento de red
    +-- Crear par veth
    +-- Conectividad entre host y contenedor
```

---

## 2. Prerequisitos

Este proyecto usa conceptos de secciones anteriores:

- **B02/C03/S04**: namespaces y cgroups desde bash (`unshare`,
  `nsenter`, `/proc/self/ns/`). Ya viste como funcionan — ahora
  los implementamos en C.
- **B06/C02**: `fork()`, `exec()`, `wait()`. El modelo de
  creacion de procesos de Unix.
- **B06/C01/S03**: `mmap`. `clone()` necesita un stack mapeado.
- **B06/C03**: senales. El proceso hijo recibe senales.

### Headers necesarios

```c
#define _GNU_SOURCE       // para clone(), pivot_root(), etc.
#include <sched.h>        // clone(), CLONE_NEW*
#include <unistd.h>       // fork, exec, chroot, sethostname
#include <sys/mount.h>    // mount, umount2
#include <sys/syscall.h>  // SYS_pivot_root
#include <sys/wait.h>     // waitpid
#include <sys/stat.h>     // mkdir
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
```

> **Nota**: `_GNU_SOURCE` debe definirse **antes** de cualquier
> `#include`. Sin el, `clone()` y otros no estan disponibles.

---

## 3. clone() vs fork()

Ya conoces `fork()`: duplica el proceso y el hijo hereda una
copia de todo. `clone()` es la version **configurable** de
`fork()` — puedes elegir exactamente que comparte y que aisla
el nuevo proceso.

### fork() — lo que ya sabes

```c
pid_t pid = fork();
if (pid == 0) {
    // Hijo: mismos namespaces que el padre
    // mismo PID namespace, mismo mount namespace, etc.
    execvp(argv[0], argv);
}
waitpid(pid, &status, 0);
```

### clone() — control granular

```c
int child_func(void *arg) {
    // Este proceso tiene sus PROPIOS namespaces
    // PID 1 dentro de su PID namespace
    execvp(argv[0], argv);
    return 1;
}

// Crear stack para el hijo (clone no comparte stack como fork)
char *stack = mmap(NULL, STACK_SIZE,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
    -1, 0);

int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD;

pid_t pid = clone(child_func, stack + STACK_SIZE, flags, arg);
waitpid(pid, &status, 0);
```

### Por que clone() necesita un stack

`fork()` copia el espacio de direcciones del padre — el hijo
obtiene su propia copia del stack automaticamente. `clone()`
puede **compartir** el espacio de direcciones (como los threads),
asi que necesita un stack explicito:

```
    fork():                          clone():
    +----------+                     +----------+
    | Padre    |                     | Padre    |
    | [stack]  |--copia-->           | [stack]  |
    +----------+    +----------+     +----------+
                    | Hijo     |
                    | [stack]  |     Stack separado:
                    +----------+     +----------+
                                     | Hijo     |
                    (copia del       | [nuevo]  |  <-- mmap()
                     padre)          +----------+
```

El stack crece **hacia abajo** en x86_64, por eso pasamos
`stack + STACK_SIZE` (el **tope** del stack, no la base):

```
    mmap retorna:        clone recibe:
    stack (base)         stack + STACK_SIZE (tope)
    |                    |
    v                    v
    +--------------------+
    |                    |
    |    STACK_SIZE      |
    |                    |
    +--------------------+
    ^                    ^
    direccion baja       direccion alta

    El stack crece <--- de alta a baja
```

---

## 4. Flags de namespace

Cada flag `CLONE_NEW*` crea un nuevo namespace de un tipo
especifico para el proceso hijo:

```
+----------------+---------------------------------------------+
| Flag           | Aisla                                       |
+----------------+---------------------------------------------+
| CLONE_NEWPID   | PIDs: hijo ve PID 1, no ve procesos padre   |
| CLONE_NEWNS    | Mounts: hijo tiene su propia tabla de mount  |
| CLONE_NEWUTS   | Hostname: sethostname no afecta al host     |
| CLONE_NEWIPC   | IPC: colas de mensajes, semaforos, shm      |
| CLONE_NEWNET   | Red: interfaces de red propias (S03)        |
| CLONE_NEWUSER  | UIDs/GIDs: mapeo de usuarios (no usaremos)  |
| CLONE_NEWCGROUP| Cgroups: vista aislada (Linux 4.6+)         |
+----------------+---------------------------------------------+
```

En esta seccion usamos cuatro:

```c
int flags = CLONE_NEWPID    // PID aislado
          | CLONE_NEWNS     // mounts aislados
          | CLONE_NEWUTS    // hostname aislado
          | CLONE_NEWIPC    // IPC aislado
          | SIGCHLD;        // enviar SIGCHLD al padre cuando hijo muera
```

`SIGCHLD` no es un flag de namespace — le dice a `clone()` que
envie `SIGCHLD` al padre cuando el hijo termine, lo que permite
usar `waitpid()` normalmente.

---

## 5. PID namespace (CLONE_NEWPID)

Con `CLONE_NEWPID`, el proceso hijo es PID 1 dentro de su
namespace. No puede ver los procesos del host:

```
    Host PID namespace:          Contenedor PID namespace:
    PID 1: systemd               PID 1: /bin/sh (nuestro proceso)
    PID 500: sshd                PID 2: ps (lanzado desde /bin/sh)
    PID 1234: container (padre)
    PID 1235: child (visto como  (no ve nada del host)
              PID 1235 en host,
              PID 1 en contenedor)
```

### Ejemplo minimo

```c
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define STACK_SIZE (1024 * 1024)  // 1 MB

static int child_func(void *arg) {
    printf("Child PID (inside namespace): %d\n", getpid());
    // Deberia imprimir: 1
    return 0;
}

int main(void) {
    char *stack = mmap(NULL, STACK_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
        -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    pid_t pid = clone(child_func, stack + STACK_SIZE,
                      CLONE_NEWPID | SIGCHLD, NULL);
    if (pid == -1) {
        perror("clone");
        return 1;
    }

    printf("Parent sees child as PID: %d\n", pid);
    // Imprime el PID real (en el namespace del host)

    int status;
    waitpid(pid, &status, 0);

    munmap(stack, STACK_SIZE);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
```

```bash
$ sudo ./pid_ns_demo
Parent sees child as PID: 12345
Child PID (inside namespace): 1
```

> **Nota**: `clone()` con `CLONE_NEWPID` requiere **root**
> (o `CAP_SYS_ADMIN`). Todos los ejemplos de este capitulo
> se ejecutan con `sudo`.

### PID 1 y sus responsabilidades

Dentro de un PID namespace, PID 1 tiene un rol especial:

- Es el **init** del namespace
- Recibe los procesos huerfanos (reparenting)
- Si PID 1 muere, **todo el namespace se destruye** y todos
  sus procesos reciben SIGKILL
- Las senales no manejadas no matan a PID 1 (excepto SIGKILL
  y SIGSTOP desde el namespace padre)

---

## 6. UTS namespace (CLONE_NEWUTS)

UTS (Unix Time Sharing) aisla el hostname y el domainname.
El contenedor puede tener su propio hostname sin afectar al host:

```c
static int child_func(void *arg) {
    // Cambiar hostname dentro del contenedor
    if (sethostname("container", 9) == -1) {
        perror("sethostname");
        return 1;
    }

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("Container hostname: %s\n", hostname);
    // "container" — no afecta al host

    return 0;
}

// En main:
pid_t pid = clone(child_func, stack + STACK_SIZE,
                  CLONE_NEWUTS | SIGCHLD, NULL);
```

Sin `CLONE_NEWUTS`, `sethostname()` cambiaria el hostname
de todo el sistema.

---

## 7. Mount namespace (CLONE_NEWNS)

`CLONE_NEWNS` (New Namespace — fue el primer namespace en
Linux, por eso no se llama `CLONE_NEWMNT`) da al proceso hijo
su propia **tabla de mounts**. Los mounts y unmounts dentro del
contenedor no afectan al host.

### Propagacion de mounts

Por defecto, el hijo hereda una **copia** de la tabla de mounts
del padre. Pero los mounts del host pueden propagarse al
contenedor y viceversa por la caracteristica de **shared
subtrees**. Para evitarlo, convertimos todo a privado:

```c
static int child_func(void *arg) {
    // Hacer que todos los mounts sean privados (sin propagacion)
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE");
        return 1;
    }
    // Ahora nuestros mounts no afectan al host y viceversa
    // ...
    return 0;
}
```

```
    Sin MS_PRIVATE:              Con MS_PRIVATE:
    Host monta /mnt/usb         Host monta /mnt/usb
         |                           |
         v (propagacion)             x (bloqueado)
    Contenedor ve /mnt/usb      Contenedor NO ve /mnt/usb
```

### Flags de mount()

```
+-------------------+------------------------------------------------+
| Flag              | Efecto                                         |
+-------------------+------------------------------------------------+
| MS_BIND           | Bind mount (montar un dir en otro punto)       |
| MS_REC            | Recursivo (afecta submounts)                   |
| MS_PRIVATE        | Sin propagacion de mounts                      |
| MS_RDONLY         | Solo lectura                                   |
| MS_NOSUID         | Ignorar bits setuid/setgid                     |
| MS_NODEV          | No permitir archivos de dispositivo             |
| MS_NOEXEC         | No permitir ejecucion de binarios              |
| MS_REMOUNT        | Cambiar flags de un mount existente            |
+-------------------+------------------------------------------------+
```

---

## 8. IPC namespace (CLONE_NEWIPC)

`CLONE_NEWIPC` aisla los mecanismos IPC de System V y POSIX:
colas de mensajes, semaforos y segmentos de memoria compartida.
Sin este flag, un proceso en el contenedor podria leer colas
de mensajes del host.

```c
// Se agrega al conjunto de flags:
int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD;
```

No requiere configuracion adicional — basta con el flag.

---

## 9. Preparar un rootfs minimo

El contenedor necesita un **rootfs** (root filesystem) propio.
Usamos Alpine Linux porque es minimalista (~3 MB):

```bash
# Crear directorio para el rootfs
mkdir -p rootfs

# Descargar y extraer Alpine mini rootfs
# (verificar la version mas reciente en https://alpinelinux.org/downloads/)
curl -o alpine.tar.gz \
    https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/alpine-minirootfs-3.21.3-x86_64.tar.gz

# Extraer en rootfs/
tar -xzf alpine.tar.gz -C rootfs/

# Verificar estructura
ls rootfs/
# bin  dev  etc  home  lib  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
```

### Estructura del rootfs

```
    rootfs/
    +-- bin/           busybox, sh, ls, ps, ...
    +-- dev/           (vacio, se puebla al montar)
    +-- etc/           configuracion (resolv.conf, passwd, ...)
    +-- home/
    +-- lib/           musl-libc (libc de Alpine)
    +-- proc/          (vacio, montaremos procfs aqui)
    +-- root/          home de root
    +-- sbin/          binarios de sistema
    +-- sys/           (vacio)
    +-- tmp/
    +-- usr/
    +-- var/
```

### Por que no usar chroot sin namespaces

`chroot` solo cambia el directorio raiz — no aisla nada mas.
Un proceso con `CAP_SYS_CHROOT` puede **escapar** del chroot:

```c
// Escape clasico de chroot (NO funciona con pivot_root + mount ns):
mkdir("escape", 0755);
chroot("escape");
for (int i = 0; i < 100; i++) chdir("..");
chroot(".");
// Ahora estamos fuera del chroot original
```

Por eso usamos `pivot_root` + mount namespace: es irrompible.

---

## 10. chroot() vs pivot_root()

### chroot() — cambio simple de raiz

```c
chroot("/path/to/rootfs");
chdir("/");
// Ahora "/" apunta a /path/to/rootfs
// Pero el filesystem original sigue accesible (escape posible)
```

### pivot_root() — intercambio de raiz (seguro)

`pivot_root` mueve el root filesystem actual a un subdirectorio
y pone un nuevo filesystem como raiz. Luego puedes desmontar
el antiguo root:

```
    Antes de pivot_root:
    / (host root)
    +-- rootfs/ (nuestro Alpine)
    |   +-- bin/
    |   +-- etc/
    |   +-- old_root/   <-- creado para recibir el root antiguo
    +-- usr/
    +-- etc/
    +-- (resto del host)

    Despues de pivot_root("rootfs", "rootfs/old_root"):
    / (ahora es Alpine)
    +-- bin/
    +-- etc/
    +-- old_root/  <-- el root del host esta aqui
    |   +-- usr/
    |   +-- etc/
    |   +-- rootfs/
    +-- proc/

    Despues de umount("/old_root"):
    / (Alpine, limpio)
    +-- bin/
    +-- etc/
    +-- proc/
    (host root ya no es accesible)
```

### Implementacion en C

`pivot_root` no tiene wrapper en glibc — hay que usar `syscall()`:

```c
#include <sys/syscall.h>

static int pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}
```

El procedimiento completo:

```c
static int setup_root(const char *rootfs) {
    // 1. Bind mount del rootfs sobre si mismo
    //    (pivot_root requiere que new_root sea un mount point)
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("mount bind rootfs");
        return -1;
    }

    // 2. Crear directorio para el old root dentro del nuevo root
    char old_root[256];
    snprintf(old_root, sizeof(old_root), "%s/old_root", rootfs);
    if (mkdir(old_root, 0700) == -1 && errno != EEXIST) {
        perror("mkdir old_root");
        return -1;
    }

    // 3. pivot_root: el rootfs se convierte en /,
    //    el root antiguo va a rootfs/old_root
    if (pivot_root(rootfs, old_root) == -1) {
        perror("pivot_root");
        return -1;
    }

    // 4. Cambiar directorio de trabajo a la nueva raiz
    if (chdir("/") == -1) {
        perror("chdir /");
        return -1;
    }

    // 5. Desmontar el old root
    if (umount2("/old_root", MNT_DETACH) == -1) {
        perror("umount2 old_root");
        return -1;
    }

    // 6. Eliminar el directorio old_root (ya no tiene nada)
    if (rmdir("/old_root") == -1) {
        perror("rmdir old_root");
        return -1;
    }

    return 0;
}
```

### Por que bind mount antes de pivot_root

`pivot_root` requiere que `new_root` sea un **mount point** (no
un directorio comun). El bind mount de rootfs sobre si mismo
satisface este requisito sin montar nada nuevo.

---

## 11. Montar /proc en el nuevo namespace

`/proc` es un pseudo-filesystem que expone informacion del
kernel sobre los procesos. Sin montarlo, comandos como `ps`,
`top` y `free` no funcionan dentro del contenedor:

```c
static int setup_proc(void) {
    // Crear /proc si no existe
    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        perror("mkdir /proc");
        return -1;
    }

    // Montar procfs — gracias al PID namespace,
    // solo muestra los procesos del contenedor
    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1) {
        perror("mount /proc");
        return -1;
    }

    return 0;
}
```

### Que ve /proc dentro del PID namespace

```
    Host:                        Contenedor:
    /proc/1/   (systemd)        /proc/1/   (nuestro /bin/sh)
    /proc/500/ (sshd)           /proc/2/   (ps, si se ejecuta)
    /proc/1234/ (container)     (nada mas)
    /proc/1235/ (child)

    El contenedor solo ve SUS procesos en /proc
```

### Otros mounts utiles

En un contenedor real tambien montarias:

```c
// /dev — dispositivos minimos
mount("tmpfs", "/dev", "tmpfs",
      MS_NOSUID | MS_STRICTATIME, "mode=755,size=65536k");

// /dev/pts — pseudo-terminales (necesario para /bin/sh interactivo)
mkdir("/dev/pts", 0755);
mount("devpts", "/dev/pts", "devpts", 0, NULL);

// /sys — informacion del kernel (opcional, usualmente read-only)
mount("sysfs", "/sys", "sysfs", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
```

Para nuestro mini runtime, `/proc` es suficiente — `/dev`
ya viene parcialmente poblado en el rootfs de Alpine (al menos
`/dev/null`, `/dev/zero`, etc., como symlinks a nombres que
el kernel crea).

---

## 12. Construir container.c paso a paso

### Estructura del proyecto

```
    mini-container/
    +-- container.c     <-- programa principal
    +-- Makefile
    +-- rootfs/          <-- Alpine Linux rootfs (extraido)
    |   +-- bin/
    |   +-- etc/
    |   +-- ...
```

### Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
TARGET = container

$(TARGET): container.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: clean
```

### container.c completo

```c
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)   /* 1 MB stack for child */
#define HOSTNAME   "container"

/* --------------------------------------------------------
 * pivot_root — no glibc wrapper, usamos syscall directo
 * -------------------------------------------------------- */
static int pivot_root(const char *new_root, const char *put_old)
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

/* --------------------------------------------------------
 * setup_root — bind mount + pivot_root + cleanup
 * -------------------------------------------------------- */
static int setup_root(const char *rootfs)
{
    /* Hacer todos los mounts privados (sin propagacion al host) */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE /");
        return -1;
    }

    /* Bind mount del rootfs sobre si mismo (pivot_root lo requiere) */
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("mount bind rootfs");
        return -1;
    }

    /* Crear directorio para el old root */
    char old_root[256];
    snprintf(old_root, sizeof(old_root), "%s/old_root", rootfs);
    if (mkdir(old_root, 0700) == -1 && errno != EEXIST) {
        perror("mkdir old_root");
        return -1;
    }

    /* pivot_root: nuevo root = rootfs, viejo root = rootfs/old_root */
    if (pivot_root(rootfs, old_root) == -1) {
        perror("pivot_root");
        return -1;
    }

    /* Cambiar al nuevo / */
    if (chdir("/") == -1) {
        perror("chdir /");
        return -1;
    }

    /* Desmontar y eliminar el old root */
    if (umount2("/old_root", MNT_DETACH) == -1) {
        perror("umount2 /old_root");
        return -1;
    }

    if (rmdir("/old_root") == -1) {
        perror("rmdir /old_root");
        return -1;
    }

    return 0;
}

/* --------------------------------------------------------
 * setup_proc — montar /proc para que ps/top funcionen
 * -------------------------------------------------------- */
static int setup_proc(void)
{
    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        perror("mkdir /proc");
        return -1;
    }

    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1) {
        perror("mount /proc");
        return -1;
    }

    return 0;
}

/* --------------------------------------------------------
 * container_args — datos que pasa el padre al hijo
 * -------------------------------------------------------- */
struct container_args {
    const char *rootfs;
    char *const *argv;    /* comando a ejecutar en el contenedor */
};

/* --------------------------------------------------------
 * child_func — funcion que ejecuta el proceso contenedor
 * -------------------------------------------------------- */
static int child_func(void *arg)
{
    struct container_args *ca = (struct container_args *)arg;

    /* 1. Cambiar hostname */
    if (sethostname(HOSTNAME, strlen(HOSTNAME)) == -1) {
        perror("sethostname");
        return 1;
    }
    fprintf(stderr, "[container] hostname set to '%s'\n", HOSTNAME);

    /* 2. Configurar filesystem: pivot_root */
    if (setup_root(ca->rootfs) == -1) {
        fprintf(stderr, "[container] setup_root failed\n");
        return 1;
    }
    fprintf(stderr, "[container] pivot_root: OK\n");

    /* 3. Montar /proc */
    if (setup_proc() == -1) {
        fprintf(stderr, "[container] setup_proc failed\n");
        return 1;
    }
    fprintf(stderr, "[container] /proc mounted\n");

    /* 4. Ejecutar el comando solicitado */
    fprintf(stderr, "[container] executing: %s\n", ca->argv[0]);
    execvp(ca->argv[0], ca->argv);
    perror("execvp");
    return 1;
}

/* --------------------------------------------------------
 * main
 * -------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rootfs> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Example: %s ./rootfs /bin/sh\n", argv[0]);
        return 1;
    }

    const char *rootfs = argv[1];
    char *const *cmd = &argv[2];

    /* Verificar que el rootfs existe */
    struct stat st;
    if (stat(rootfs, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", rootfs);
        return 1;
    }

    /* Preparar argumentos para el hijo */
    struct container_args ca = {
        .rootfs = rootfs,
        .argv = cmd,
    };

    /* Allocar stack para clone() */
    char *stack = mmap(NULL, STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap stack");
        return 1;
    }

    /* Flags: crear PID, mount, UTS e IPC namespaces */
    int clone_flags = CLONE_NEWPID
                    | CLONE_NEWNS
                    | CLONE_NEWUTS
                    | CLONE_NEWIPC
                    | SIGCHLD;

    /* Crear el proceso contenedor */
    pid_t child_pid = clone(child_func, stack + STACK_SIZE,
                            clone_flags, &ca);
    if (child_pid == -1) {
        perror("clone");
        munmap(stack, STACK_SIZE);
        return 1;
    }

    fprintf(stderr, "[parent] container process PID: %d\n", child_pid);

    /* Esperar a que el contenedor termine */
    int status;
    if (waitpid(child_pid, &status, 0) == -1) {
        perror("waitpid");
        munmap(stack, STACK_SIZE);
        return 1;
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

### Flujo de ejecucion

```
    main()
    |
    +-- Verificar argc y rootfs
    |
    +-- mmap() stack de 1 MB
    |
    +-- clone(child_func, stack+STACK_SIZE, flags, &ca)
    |       |
    |       +-- child_func(&ca):
    |       |   |
    |       |   +-- sethostname("container")
    |       |   +-- setup_root(rootfs):
    |       |   |   +-- mount(/, MS_PRIVATE)
    |       |   |   +-- mount(rootfs, rootfs, MS_BIND)
    |       |   |   +-- mkdir(rootfs/old_root)
    |       |   |   +-- pivot_root(rootfs, rootfs/old_root)
    |       |   |   +-- chdir("/")
    |       |   |   +-- umount2("/old_root")
    |       |   |   +-- rmdir("/old_root")
    |       |   |
    |       |   +-- setup_proc():
    |       |   |   +-- mkdir("/proc")
    |       |   |   +-- mount("proc", "/proc", "proc")
    |       |   |
    |       |   +-- execvp(argv[0], argv)  <-- reemplaza el proceso
    |       |
    |       (hijo ejecuta /bin/sh o lo que sea)
    |
    +-- waitpid(child_pid, &status, 0)  <-- padre espera
    |
    +-- munmap(stack)
    |
    +-- return status
```

---

## 13. Probando el contenedor

### Compilar y ejecutar

```bash
# Compilar
make

# Ejecutar (requiere root)
sudo ./container ./rootfs /bin/sh
```

### Verificaciones dentro del contenedor

```bash
# Verificar PID namespace
container# ps aux
PID   USER     COMMAND
    1 root     /bin/sh
    2 root     ps aux

# Verificar hostname
container# hostname
container

# Verificar filesystem aislado
container# ls /
bin    dev    etc    home   lib    media  mnt    opt
proc   root   run    sbin   srv    sys    tmp    usr  var

# Verificar que no ve procesos del host
container# ls /proc/ | head -5
1
2
...
# Solo ve sus propios procesos

# Intentar montar algo — no afecta al host
container# mkdir /mnt/test
container# mount -t tmpfs tmpfs /mnt/test
container# ls /mnt/test
# Existe dentro del contenedor, invisible desde el host

# Salir
container# exit
[parent] container exited with status 0
```

### Verificar desde el host (en otra terminal)

```bash
# Mientras el contenedor esta corriendo:

# Ver el proceso del contenedor
$ ps aux | grep container
root     1234  ...  ./container ./rootfs /bin/sh
root     1235  ...  /bin/sh

# Ver los namespaces del proceso hijo
$ sudo ls -la /proc/1235/ns/
lrwxrwxrwx 1 root root 0 ... ipc -> ipc:[4026532xxx]
lrwxrwxrwx 1 root root 0 ... mnt -> mnt:[4026532xxx]
lrwxrwxrwx 1 root root 0 ... pid -> pid:[4026532xxx]
lrwxrwxrwx 1 root root 0 ... uts -> uts:[4026532xxx]

# Comparar con los del padre — son DIFERENTES
$ sudo ls -la /proc/1234/ns/
# (numeros de inode diferentes)
```

---

## 14. Errores comunes

### Error 1: ejecutar sin root

```bash
$ ./container ./rootfs /bin/sh
clone: Operation not permitted
```

```bash
# BIEN: usar sudo
$ sudo ./container ./rootfs /bin/sh
```

`clone()` con `CLONE_NEWPID` requiere `CAP_SYS_ADMIN`. En
produccion, runtimes como runc usan user namespaces
(`CLONE_NEWUSER`) para evitar necesitar root, pero eso agrega
complejidad que no necesitamos aqui.

### Error 2: olvidar MS_PRIVATE antes de pivot_root

```c
// MAL: sin MS_PRIVATE, los mounts pueden propagarse al host
static int setup_root(const char *rootfs) {
    // Falta: mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)
    mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL);
    // ...
}
```

```c
// BIEN: primero hacer / privado
static int setup_root(const char *rootfs) {
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);  // PRIMERO
    mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL);
    // ...
}
```

**Por que falla**: sin `MS_PRIVATE`, el bind mount puede
propagarse al namespace de mounts del host. En el mejor caso
no pasa nada; en el peor caso, `umount2("/old_root")` desmonta
cosas en el host.

### Error 3: pasar la base del stack a clone() en vez del tope

```c
// MAL: stack crece hacia abajo, clone necesita el tope
pid_t pid = clone(child_func, stack, flags, &ca);
// SEGFAULT: el stack se desborda inmediatamente
```

```c
// BIEN: pasar stack + STACK_SIZE (el tope)
pid_t pid = clone(child_func, stack + STACK_SIZE, flags, &ca);
```

```
    MAL:                    BIEN:
    stack -----> clone()    stack + SIZE -----> clone()
    |            aqui            |               aqui
    v            crece           v               crece
    [............]  hacia       [............]   hacia
     ^              donde?      ^               abajo (OK)
     |                          |
     base                       base
```

### Error 4: no desmontar /old_root

```c
// MAL: dejar /old_root montado
// El contenedor puede acceder al filesystem del host:
// ls /old_root/etc/shadow  <-- FUGA DE SEGURIDAD
```

```c
// BIEN: desmontar y eliminar
umount2("/old_root", MNT_DETACH);
rmdir("/old_root");
```

Si no desmontas `/old_root`, el contenedor tiene acceso
completo al filesystem del host. Es como dejar la puerta
de atras abierta.

### Error 5: olvidar SIGCHLD en los flags de clone()

```c
// MAL: sin SIGCHLD, waitpid no funciona correctamente
int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS;
pid_t pid = clone(child_func, stack + STACK_SIZE, flags, &ca);
waitpid(pid, &status, 0);
// waitpid retorna -1 con errno = ECHILD
```

```c
// BIEN: siempre incluir SIGCHLD
int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | SIGCHLD;
```

`SIGCHLD` le dice al kernel que envie la senal `SIGCHLD` al
padre cuando el hijo termine. Sin ella, `waitpid()` no puede
recoger el estado del hijo.

---

## 15. Cheatsheet

```
MINI CONTAINER RUNTIME — S01 REFERENCIA RAPIDA
================================================

Compilar y ejecutar:
  gcc -Wall -Wextra -std=c11 -o container container.c
  sudo ./container ./rootfs /bin/sh

Preparar rootfs:
  mkdir rootfs
  curl -O https://dl-cdn.alpinelinux.org/.../alpine-minirootfs-*.tar.gz
  tar -xzf alpine-minirootfs-*.tar.gz -C rootfs/

clone() vs fork():
  fork()   — copia todo, mismos namespaces
  clone()  — configurable, flags CLONE_NEW* para aislar

Flags de namespace:
  CLONE_NEWPID   PID aislado (hijo es PID 1)
  CLONE_NEWNS    Mounts aislados
  CLONE_NEWUTS   Hostname aislado
  CLONE_NEWIPC   IPC aislado
  CLONE_NEWNET   Red aislada (S03)
  | SIGCHLD      Siempre incluir para waitpid()

Stack para clone():
  char *stack = mmap(NULL, SIZE, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
  clone(func, stack + SIZE, flags, arg);  // tope, no base

pivot_root (sin wrapper glibc):
  syscall(SYS_pivot_root, new_root, put_old);

Secuencia de setup:
  1. mount(NULL, "/", NULL, MS_REC|MS_PRIVATE, NULL)
  2. mount(rootfs, rootfs, NULL, MS_BIND|MS_REC, NULL)
  3. mkdir(rootfs/old_root)
  4. pivot_root(rootfs, rootfs/old_root)
  5. chdir("/")
  6. umount2("/old_root", MNT_DETACH)
  7. rmdir("/old_root")
  8. mount("proc", "/proc", "proc", MS_NOSUID|MS_NODEV|MS_NOEXEC, NULL)
  9. execvp(command, args)

Diferencia chroot vs pivot_root:
  chroot:      cambia /, pero escapable (sin namespace)
  pivot_root:  intercambia roots, seguro con mount namespace

Verificar aislamiento:
  ps aux              — solo ve procesos del contenedor
  hostname             — muestra "container"
  ls /                 — rootfs de Alpine
  mount                — mounts propios, no los del host
  ls /proc/            — solo PIDs del namespace
```

---

## 16. Ejercicios

### Ejercicio 1: agregar argumentos de linea de comandos

Extiende `container.c` para aceptar un hostname personalizado:

```bash
sudo ./container --hostname mybox ./rootfs /bin/sh
```

Tareas:
1. Modifica `main()` para parsear `--hostname <nombre>` antes
   del rootfs y comando
2. Pasa el hostname a `child_func` a traves de `container_args`
3. Usa el hostname proporcionado en `sethostname()` en vez del
   hardcoded `"container"`
4. Si no se proporciona `--hostname`, usa `"container"` como
   default

**Prediccion**: al ejecutar `hostname` dentro del contenedor,
que hostname veras? Y si ejecutas `hostname` en el host
mientras el contenedor esta corriendo, sera diferente?

> **Pregunta de reflexion**: Docker permite `--hostname` con
> `docker run --hostname mybox ubuntu bash`. Internamente, Docker
> hace exactamente lo que implementaste: `clone(CLONE_NEWUTS)` +
> `sethostname()`. Que pasaria si Docker no usara `CLONE_NEWUTS`?
> Que efecto tendria `sethostname()` sobre el host?

### Ejercicio 2: montar /dev con dispositivos minimos

Mejora el contenedor para tener un `/dev` funcional:

```c
static int setup_dev(void)
{
    // 1. Montar tmpfs en /dev
    // mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755");

    // 2. Crear dispositivos minimos con mknod():
    //    /dev/null    (1, 3)   — agujero negro
    //    /dev/zero    (1, 5)   — fuente de ceros
    //    /dev/random  (1, 8)   — entropia
    //    /dev/urandom (1, 9)   — entropia no bloqueante
    //    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    //    ...

    // 3. Crear symlinks:
    //    /dev/stdin  -> /proc/self/fd/0
    //    /dev/stdout -> /proc/self/fd/1
    //    /dev/stderr -> /proc/self/fd/2

    // 4. Crear /dev/pts y montar devpts (para terminales)
    //    mkdir("/dev/pts", 0755);
    //    mount("devpts", "/dev/pts", "devpts", 0, NULL);

    return 0;
}
```

Tareas:
1. Implementa `setup_dev()` con los pasos indicados
2. Llamala desde `child_func()` despues de `setup_root()`
   pero antes de `setup_proc()`
3. Verifica que `/dev/null` funciona: `echo hello > /dev/null`
4. Verifica que `/dev/urandom` funciona: `head -c 16 /dev/urandom | xxd`

> **Pregunta de reflexion**: `mknod` crea archivos especiales
> de dispositivo. En el contenedor, `/dev/null` (1,3) accede
> al **mismo driver del kernel** que `/dev/null` del host — los
> device numbers son globales. Esto significa que el contenedor
> NO esta completamente aislado a nivel de dispositivos. Como
> resuelven esto los runtimes reales? (Pista: investiga
> `devices` cgroup controller y `CLONE_NEWUSER`.)

### Ejercicio 3: ejecutar un comando no interactivo

Modifica el contenedor para ejecutar un comando y retornar su
salida, en vez de lanzar un shell interactivo:

```bash
# Ejecutar un comando y obtener resultado
sudo ./container ./rootfs /bin/echo "Hello from container"
Hello from container
[parent] container exited with status 0

# Listar archivos del rootfs
sudo ./container ./rootfs /bin/ls /
bin    dev    etc    home   lib    media  mnt    opt
proc   root   run    sbin   srv    sys    tmp    usr  var

# Verificar PID
sudo ./container ./rootfs /bin/sh -c "echo PID=$$"
PID=1
```

Tareas:
1. Verifica que `container.c` ya funciona con comandos no
   interactivos (deberia — `execvp` ejecuta lo que le pases)
2. Modifica el programa para que los mensajes de `[container]`
   vayan a stderr (ya lo hacen con `fprintf(stderr, ...)`) y
   stdout sea limpio para redireccionar:
   `sudo ./container ./rootfs /bin/ls / > files.txt`
3. Agrega la opcion `--quiet` que suprime los mensajes
   de `[container]` y `[parent]`
4. Mide cuanto tarda en arrancar y salir el contenedor:
   `time sudo ./container ./rootfs /bin/true`

> **Pregunta de reflexion**: `time sudo ./container ./rootfs
> /bin/true` muestra el overhead del contenedor. Compara con
> `time /bin/true` (sin contenedor). La diferencia es el coste
> de `clone()` + `pivot_root()` + montar `/proc`. Cuanto es?
> Docker tiene overhead similar o mayor? Por que?
