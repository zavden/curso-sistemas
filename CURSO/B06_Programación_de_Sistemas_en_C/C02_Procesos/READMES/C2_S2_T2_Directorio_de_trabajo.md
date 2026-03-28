# Directorio de Trabajo — getcwd, chdir y Rutas Relativas

## Índice

1. [Qué es el directorio de trabajo](#1-qué-es-el-directorio-de-trabajo)
2. [getcwd: obtener el directorio actual](#2-getcwd-obtener-el-directorio-actual)
3. [chdir y fchdir: cambiar de directorio](#3-chdir-y-fchdir-cambiar-de-directorio)
4. [Rutas absolutas vs relativas](#4-rutas-absolutas-vs-relativas)
5. [Herencia en fork y exec](#5-herencia-en-fork-y-exec)
6. [/proc/self/cwd y el kernel](#6-procselfcwd-y-el-kernel)
7. [Patrones y casos de uso](#7-patrones-y-casos-de-uso)
8. [Interacción con threads](#8-interacción-con-threads)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es el directorio de trabajo

Cada proceso tiene un **current working directory** (cwd) — el directorio
contra el cual se resuelven todas las rutas relativas. Cuando abres
`"data.txt"`, el kernel lo interpreta como `cwd + "/data.txt"`:

```
  Proceso (PID 1234)
  ┌─────────────────────────────────┐
  │  cwd: /home/user/project       │
  │                                 │
  │  open("data.txt", O_RDONLY)     │
  │     ↓ kernel resuelve como:     │
  │  open("/home/user/project/data.txt", O_RDONLY)
  │                                 │
  │  open("../config.ini", O_RDONLY)│
  │     ↓ kernel resuelve como:     │
  │  open("/home/user/config.ini", O_RDONLY)
  │                                 │
  │  open("/etc/hosts", O_RDONLY)   │
  │     ↓ ruta absoluta: no usa cwd│
  └─────────────────────────────────┘
```

### Dónde se almacena

El cwd **no** es una variable de entorno. Es un atributo del proceso
almacenado en el kernel (en la estructura `task_struct`). El kernel
guarda un **puntero al inodo** del directorio, no la ruta como string.

```
  task_struct (kernel)
  ┌──────────────────┐
  │ pid: 1234        │
  │ fs->pwd ────────────▶ inodo del directorio
  │ fs->root ───────────▶ inodo de /
  │ ...              │
  └──────────────────┘
```

> **Nota**: La variable de entorno `PWD` es una **conveniencia del shell**
> que intenta reflejar el cwd, pero no es la fuente de verdad. El kernel
> no la consulta.

---

## 2. getcwd: obtener el directorio actual

```c
#include <unistd.h>

char *getcwd(char *buf, size_t size);
```

- Escribe la ruta absoluta del cwd en `buf` (máximo `size` bytes, incluido `\0`).
- Retorna `buf` en éxito, `NULL` en error.
- Si `buf` es `NULL` y `size` es 0, glibc **asigna memoria** automáticamente
  (extensión GNU, no POSIX).

### Uso con buffer estático

```c
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    printf("current directory: %s\n", cwd);
    return 0;
}
```

`PATH_MAX` está definido en `<limits.h>` y suele ser **4096** en Linux.
Es el máximo garantizado para una ruta completa.

### Uso con asignación dinámica (GNU)

```c
#include <stdlib.h>
#include <unistd.h>

char *cwd = getcwd(NULL, 0);  /* glibc allocates with malloc */
if (cwd == NULL) {
    perror("getcwd");
    return 1;
}
printf("cwd: %s\n", cwd);
free(cwd);  /* Caller must free */
```

### Errores de getcwd

| errno       | Causa                                          |
|-------------|------------------------------------------------|
| `ERANGE`    | `buf` demasiado pequeño para la ruta           |
| `EACCES`    | Sin permiso de lectura en algún componente      |
| `ENOENT`    | El directorio actual fue **eliminado**          |

El caso `ENOENT` ocurre cuando otro proceso (o el mismo) borra el
directorio mientras estamos dentro:

```c
mkdir("/tmp/ephemeral", 0755);
chdir("/tmp/ephemeral");
rmdir("/tmp/ephemeral");

char buf[PATH_MAX];
if (getcwd(buf, sizeof(buf)) == NULL) {
    perror("getcwd");
    /* errno = ENOENT: directory deleted while we were in it */
}
```

El proceso sigue funcionando (el kernel mantiene el inodo abierto),
pero `getcwd` no puede reconstruir la ruta porque el nombre fue eliminado
del directorio padre.

---

## 3. chdir y fchdir: cambiar de directorio

### chdir

```c
#include <unistd.h>

int chdir(const char *path);
```

Cambia el cwd del proceso a `path`. Retorna 0 en éxito, -1 en error:

```c
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char cwd[PATH_MAX];

    printf("before: %s\n", getcwd(cwd, sizeof(cwd)));

    if (chdir("/tmp") == -1) {
        perror("chdir");
        return 1;
    }

    printf("after: %s\n", getcwd(cwd, sizeof(cwd)));

    return 0;
}
```

```
  $ ./chdir_demo
  before: /home/user/project
  after: /tmp
```

### Errores de chdir

| errno      | Causa                                      |
|------------|--------------------------------------------|
| `ENOENT`   | El path no existe                          |
| `ENOTDIR`  | Un componente del path no es directorio    |
| `EACCES`   | Sin permiso de ejecución (x) en el dir     |
| `ENAMETOOLONG` | Ruta demasiado larga                  |

### fchdir

```c
#include <unistd.h>

int fchdir(int fd);
```

Cambia el cwd usando un **file descriptor** de directorio abierto.
Ventaja principal: es atómico respecto a renombrado/eliminación del path,
y permite volver fácilmente a un directorio anterior:

```c
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    /* Save current directory */
    int saved_fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (saved_fd == -1) { perror("open ."); return 1; }

    /* Change to another directory and do work */
    if (chdir("/var/log") == -1) { perror("chdir"); return 1; }
    printf("now in /var/log\n");

    /* Work here... */

    /* Return to original directory */
    if (fchdir(saved_fd) == -1) { perror("fchdir"); return 1; }
    close(saved_fd);

    char cwd[PATH_MAX];
    printf("back in: %s\n", getcwd(cwd, sizeof(cwd)));

    return 0;
}
```

### chdir vs fchdir

| Aspecto              | chdir              | fchdir            |
|----------------------|--------------------|-------------------|
| Argumento            | path (string)      | fd (int)          |
| Resolución           | Cada componente    | Directa al inodo  |
| TOCTOU race          | Sí (path puede cambiar) | No           |
| Volver al anterior   | Guardar string con getcwd | Guardar fd  |
| Requiere             | Ruta válida        | fd abierto        |

`fchdir` es preferible cuando necesitas guardar y restaurar el cwd,
porque no depende de que la **ruta** siga existiendo — opera sobre el
inodo directamente.

---

## 4. Rutas absolutas vs relativas

El cwd solo afecta a **rutas relativas**. Es fundamental entender la
diferencia:

```
  Ruta absoluta:  /home/user/project/data.txt
                  ↑ empieza con /
                  → kernel la resuelve desde la raíz
                  → cwd no influye

  Ruta relativa:  data.txt
                  src/main.c
                  ../config.ini
                  ./script.sh
                  → kernel las resuelve desde cwd
```

### Cómo resuelve el kernel

```
  cwd = /home/user/project

  open("data.txt")        → /home/user/project/data.txt
  open("src/main.c")      → /home/user/project/src/main.c
  open("../config.ini")   → /home/user/config.ini
  open("./script.sh")     → /home/user/project/script.sh
  open("/etc/hosts")      → /etc/hosts  (absoluta, cwd ignorado)
```

### El fd especial AT_FDCWD

Las funciones `*at()` (openat, fstatat, etc.) aceptan un fd de directorio
como referencia. El valor especial `AT_FDCWD` significa "usar el cwd":

```c
#include <fcntl.h>

/* Estas dos líneas son equivalentes: */
int fd1 = open("data.txt", O_RDONLY);
int fd2 = openat(AT_FDCWD, "data.txt", O_RDONLY);
```

Pero `openat` con un fd real de directorio permite resolver rutas relativas
a **cualquier directorio**, sin cambiar el cwd:

```c
int dirfd = open("/var/log", O_RDONLY | O_DIRECTORY);
int fd = openat(dirfd, "syslog", O_RDONLY);
/* Opens /var/log/syslog without changing cwd */
close(dirfd);
```

---

## 5. Herencia en fork y exec

### fork: hijo hereda el cwd

Después de `fork`, el hijo tiene el **mismo cwd** que el padre. Pero
como son procesos independientes, si uno hace `chdir`, el otro no se
ve afectado:

```c
chdir("/home/user");

pid_t pid = fork();
if (pid == 0) {
    /* Child */
    chdir("/tmp");
    char cwd[PATH_MAX];
    printf("child cwd: %s\n", getcwd(cwd, sizeof(cwd)));
    /* Output: /tmp */
    _exit(0);
}

waitpid(pid, NULL, 0);

char cwd[PATH_MAX];
printf("parent cwd: %s\n", getcwd(cwd, sizeof(cwd)));
/* Output: /home/user — parent not affected */
```

### exec: cwd se preserva

El cwd sobrevive a `exec`. El nuevo programa comienza con el cwd que
tenía el proceso antes de exec:

```c
if (pid == 0) {
    chdir("/var/log");           /* Change cwd before exec */
    execlp("ls", "ls", NULL);   /* ls lists /var/log */
    _exit(127);
}
```

Este es el mecanismo que usa el shell para implementar `cd /tmp && ls`:
cambia su propio cwd y luego hace fork+exec; el hijo hereda el cwd.

### Por qué cd es un built-in del shell

Un dato importante: `cd` **no puede ser** un programa externo. Si fuera
un programa externo, el shell haría fork+exec para ejecutarlo. El `chdir`
ocurriría en el **hijo**, que luego termina. El shell padre no se vería
afectado:

```
  Si cd fuera externo:

  Shell (cwd=/home/user)
     │
     fork() → hijo (cwd=/home/user)
                │
                chdir("/tmp")  ← solo afecta al hijo
                │
                _exit(0)       ← hijo muere
     │
  Shell (cwd=/home/user)  ← SIN CAMBIO
```

Por eso `cd` es un **built-in**: el shell ejecuta `chdir` en su propio
proceso, sin fork.

---

## 6. /proc/self/cwd y el kernel

En Linux, el cwd de cualquier proceso es visible a través de `/proc`:

```bash
# Current process
ls -l /proc/self/cwd
# lrwxrwxrwx 1 user user 0 mar 20 10:00 /proc/self/cwd -> /home/user

# Any process by PID
ls -l /proc/1234/cwd
# lrwxrwxrwx 1 user user 0 mar 20 10:00 /proc/1234/cwd -> /var/log
```

### Leer el cwd de otro proceso

```c
#include <stdio.h>
#include <unistd.h>

int get_process_cwd(pid_t pid, char *buf, size_t size)
{
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/cwd", pid);

    ssize_t len = readlink(proc_path, buf, size - 1);
    if (len == -1) return -1;

    buf[len] = '\0';
    return 0;
}
```

### Internals del kernel

El kernel almacena el cwd como una estructura `path` (par dentry+vfsmount)
en `task_struct->fs->pwd`. Cuando haces `chdir`:

```
  chdir("/tmp")
     │
     ├─ 1. Resolver path: lookup_path("/tmp") → dentry
     ├─ 2. Verificar que es directorio
     ├─ 3. Verificar permiso de ejecución (x)
     ├─ 4. Incrementar refcount del dentry
     ├─ 5. Actualizar task->fs->pwd
     └─ 6. Decrementar refcount del dentry anterior
```

El refcount garantiza que **el directorio no se libera del VFS** mientras
un proceso lo use como cwd, incluso si se borra con `rmdir` (el
directorio persiste como inodo huérfano hasta que todos los procesos
hagan `chdir` a otro sitio o terminen).

---

## 7. Patrones y casos de uso

### Patrón 1: Guardar y restaurar cwd

```c
#include <fcntl.h>
#include <unistd.h>

int do_work_in_dir(const char *dir)
{
    int saved = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (saved == -1) return -1;

    if (chdir(dir) == -1) {
        close(saved);
        return -1;
    }

    /* Do work with relative paths... */
    process_files_in_cwd();

    /* Restore */
    fchdir(saved);
    close(saved);
    return 0;
}
```

### Patrón 2: Usar openat para evitar chdir

En muchos casos, `openat` elimina la necesidad de `chdir` por completo:

```c
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

void process_directory(const char *path)
{
    int dirfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd == -1) return;

    /* Open files relative to dirfd — no chdir needed */
    int fd1 = openat(dirfd, "config.ini", O_RDONLY);
    int fd2 = openat(dirfd, "data/input.csv", O_RDONLY);

    /* stat relative to dirfd */
    struct stat st;
    fstatat(dirfd, "output.log", &st, 0);

    /* Create file relative to dirfd */
    int fd3 = openat(dirfd, "result.txt",
                     O_WRONLY | O_CREAT | O_TRUNC, 0644);

    /* Cleanup... */
    close(dirfd);
}
```

Ventajas de `openat` sobre `chdir`:
- **Thread-safe**: el cwd es compartido por todos los threads; `openat`
  no lo modifica.
- **Sin race conditions**: el `dirfd` apunta al inodo, no a un path que
  pueda cambiar.
- **Sin restauración**: no necesitas guardar/restaurar el cwd.

### Patrón 3: Daemon: cambiar a /

Los daemons deben cambiar su cwd a `/` para no bloquear el desmontaje
de filesystems:

```c
void daemonize_cwd(void)
{
    if (chdir("/") == -1) {
        perror("chdir /");
        _exit(1);
    }
}
```

Si un daemon se ejecuta desde `/mnt/usb` y no hace `chdir("/")`, el
USB no se puede desmontar (`device is busy`) porque el cwd del daemon
mantiene una referencia al filesystem.

### Patrón 4: Build system — cwd relativo al proyecto

```c
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Change to the directory containing the executable */
int chdir_to_exe_dir(const char *argv0)
{
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) return -1;

    path[len] = '\0';

    /* dirname modifies its argument, so we already have a copy */
    char *dir = dirname(path);
    return chdir(dir);
}

int main(int argc, char *argv[])
{
    chdir_to_exe_dir(argv[0]);
    /* Now relative paths are relative to the executable's location */

    FILE *f = fopen("assets/config.ini", "r");
    /* Opens <exe_dir>/assets/config.ini regardless of where
       the user invoked the program from */
}
```

### Patrón 5: Traversal seguro con O_NOFOLLOW

```c
#include <fcntl.h>
#include <unistd.h>

/* Safely open a file in a directory without following symlinks */
int safe_open_in_dir(const char *dir, const char *file)
{
    int dirfd = open(dir, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dirfd == -1) return -1;

    int fd = openat(dirfd, file,
                    O_RDONLY | O_NOFOLLOW);  /* Reject symlinks */
    close(dirfd);
    return fd;
}
```

---

## 8. Interacción con threads

El cwd es un atributo del **proceso**, no del thread. Todos los threads
comparten el mismo cwd:

```
  Proceso
  ┌──────────────────────────────┐
  │  cwd: /home/user             │ ← compartido
  │                              │
  │  Thread 1    Thread 2        │
  │  ┌────────┐  ┌────────┐     │
  │  │        │  │        │     │
  │  │chdir   │  │open    │     │
  │  │("/tmp")│  │("f.txt")│    │
  │  └────────┘  └────────┘     │
  │                              │
  │  Si T1 hace chdir, T2       │
  │  ahora resuelve "f.txt"     │
  │  contra /tmp                 │
  └──────────────────────────────┘
```

### El problema

```c
/* Thread 1: */
chdir("/var/log");
int fd = open("syslog", O_RDONLY);
/* Between chdir and open, Thread 2 might do chdir("/tmp")
   → open resolves "syslog" against /tmp! */

/* Thread 2: */
chdir("/tmp");
int fd2 = open("data.txt", O_RDONLY);
```

### La solución: openat

```c
/* Thread-safe: no chdir needed */
/* Thread 1: */
int dir1 = open("/var/log", O_RDONLY | O_DIRECTORY);
int fd1 = openat(dir1, "syslog", O_RDONLY);
close(dir1);

/* Thread 2: */
int dir2 = open("/tmp", O_RDONLY | O_DIRECTORY);
int fd2 = openat(dir2, "data.txt", O_RDONLY);
close(dir2);
```

> **Regla**: En programas multihilo, **nunca usar `chdir`**. Usar `openat`
> y las demás funciones `*at()` para operar con rutas relativas a un
> directorio específico sin modificar el cwd global.

### unshare(CLONE_FS) — cwd por thread (Linux)

Linux permite que un thread tenga su propia copia del cwd:

```c
#define _GNU_SOURCE
#include <sched.h>

void *thread_func(void *arg)
{
    /* Give this thread its own cwd */
    if (unshare(CLONE_FS) == -1) {
        perror("unshare");
        return NULL;
    }

    chdir("/var/log");
    /* Only affects this thread now */
    /* ... */
    return NULL;
}
```

Esto es una extensión de Linux. En la práctica, `openat` es la solución
portable y preferida.

---

## 9. Errores comunes

### Error 1: Asumir que getcwd nunca falla

```c
/* MAL: sin verificación */
char cwd[PATH_MAX];
getcwd(cwd, sizeof(cwd));
printf("cwd: %s\n", cwd);  /* Undefined if getcwd failed */

/* BIEN: verificar retorno */
char cwd[PATH_MAX];
if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    /* cwd may have been deleted, or path exceeds PATH_MAX */
    return 1;
}
printf("cwd: %s\n", cwd);
```

`getcwd` falla cuando el directorio fue eliminado (`ENOENT`), cuando la
ruta es más larga que `size` (`ERANGE`), o cuando no hay permiso de
lectura en un directorio ancestro (`EACCES`).

### Error 2: Usar chdir en programas multihilo

```c
/* MAL: todos los threads comparten cwd */
void *worker(void *arg)
{
    chdir(((task_t *)arg)->directory);
    int fd = open("data.txt", O_RDONLY);  /* Race with other threads */
    /* ... */
}

/* BIEN: usar openat sin tocar cwd */
void *worker(void *arg)
{
    int dirfd = open(((task_t *)arg)->directory,
                     O_RDONLY | O_DIRECTORY);
    int fd = openat(dirfd, "data.txt", O_RDONLY);
    close(dirfd);
    /* ... */
}
```

### Error 3: Guardar cwd como string en lugar de fd

```c
/* MAL: el directorio puede ser renombrado entre getcwd y chdir de vuelta */
char saved_cwd[PATH_MAX];
getcwd(saved_cwd, sizeof(saved_cwd));
chdir("/tmp");
/* ... work ... */
chdir(saved_cwd);  /* TOCTOU: saved_cwd might be renamed/deleted */

/* BIEN: guardar como fd */
int saved = open(".", O_RDONLY | O_DIRECTORY);
chdir("/tmp");
/* ... work ... */
fchdir(saved);  /* Always works — fd references the inode */
close(saved);
```

### Error 4: Confiar en la variable PWD

```c
/* MAL: PWD puede no reflejar el cwd real */
char *cwd = getenv("PWD");
/* PWD is set by the shell — it may be wrong if:
   - Another process changed the env
   - A symlink was traversed
   - chdir was called without updating PWD */

/* BIEN: usar getcwd para la ruta real */
char cwd[PATH_MAX];
getcwd(cwd, sizeof(cwd));
```

La variable `PWD` preserva la ruta **lógica** (con symlinks sin resolver),
mientras que `getcwd` devuelve la ruta **física** (symlinks resueltos).
El shell actualiza `PWD` en cada `cd`, pero si tu programa C hace `chdir`,
`PWD` no se actualiza automáticamente.

### Error 5: No cambiar cwd en daemons

```c
/* MAL: daemon mantiene cwd en /mnt/usb → no se puede desmontar */
void start_daemon(void)
{
    /* ... fork, setsid, etc. ... */
    /* Forgot chdir("/") */
    while (1) { /* ... serve requests ... */ }
}

/* BIEN: cambiar a / */
void start_daemon(void)
{
    /* ... fork, setsid, etc. ... */
    if (chdir("/") == -1) {
        syslog(LOG_ERR, "chdir /: %m");
        _exit(1);
    }
    while (1) { /* ... serve requests ... */ }
}
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                Directorio de Trabajo                         │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Obtener:                                                    │
  │    getcwd(buf, size)          → ruta absoluta en buf         │
  │    getcwd(NULL, 0)            → malloc'd string (GNU)        │
  │    readlink /proc/self/cwd    → desde otro proceso           │
  │                                                              │
  │  Cambiar:                                                    │
  │    chdir("/path")             → por ruta                     │
  │    fchdir(fd)                 → por fd (sin TOCTOU)          │
  │                                                              │
  │  Guardar/restaurar:                                          │
  │    int saved = open(".", O_RDONLY | O_DIRECTORY);            │
  │    chdir("/elsewhere");                                      │
  │    fchdir(saved); close(saved);                              │
  │                                                              │
  │  Evitar chdir (thread-safe):                                 │
  │    int dirfd = open(dir, O_RDONLY | O_DIRECTORY);            │
  │    openat(dirfd, "file", O_RDONLY);                          │
  │    fstatat(dirfd, "file", &st, 0);                           │
  │    close(dirfd);                                             │
  │                                                              │
  │  Herencia:                                                   │
  │    fork → hijo hereda cwd (copia independiente)              │
  │    exec → cwd se preserva                                   │
  │    cd es builtin del shell (chdir no funciona como externo)  │
  │                                                              │
  │  Threads:                                                    │
  │    cwd es por proceso, NO por thread                         │
  │    → usar openat, no chdir                                   │
  │    → unshare(CLONE_FS) para cwd por thread (Linux)          │
  │                                                              │
  │  Daemons:                                                    │
  │    chdir("/") para no bloquear umount                        │
  │                                                              │
  │  Errores:                                                    │
  │    ENOENT → dir eliminado        ERANGE → buf pequeño        │
  │    EACCES → sin permiso          ENOTDIR → no es directorio  │
  │                                                              │
  │  PWD (env var) ≠ getcwd:                                     │
  │    PWD = lógica (shell), getcwd = física (kernel)            │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: pushd/popd en C

Implementa una pila de directorios similar a los builtins `pushd` y `popd`
del shell:

```c
#define MAX_STACK 32

typedef struct {
    int fds[MAX_STACK];   /* Saved directory fds */
    int top;
} dir_stack_t;

void dir_stack_init(dir_stack_t *s);
int  dir_push(dir_stack_t *s, const char *path);  /* chdir + push old */
int  dir_pop(dir_stack_t *s);                      /* pop + fchdir */
int  dir_stack_depth(const dir_stack_t *s);
void dir_stack_print(const dir_stack_t *s);        /* Print all entries */
void dir_stack_cleanup(dir_stack_t *s);            /* Close all fds */
```

Comportamiento:
1. `dir_push("/tmp")` — guarda el cwd actual en la pila y cambia a `/tmp`.
2. `dir_pop()` — vuelve al directorio más reciente de la pila.
3. `dir_stack_print()` — muestra toda la pila (getcwd de cada fd).
4. Usar `fds` (no strings) para guardar directorios.

Probar con:
```c
dir_push(&s, "/tmp");
dir_push(&s, "/var/log");
dir_push(&s, "/etc");
dir_stack_print(&s);   /* /etc (current), /var/log, /tmp, /home/user */
dir_pop(&s);           /* Back to /var/log */
dir_pop(&s);           /* Back to /tmp */
```

**Predicción antes de codificar**: Si después de `dir_push("/tmp")` alguien
borra el directorio original con `rmdir`, ¿`dir_pop` seguirá funcionando?
¿Por qué?

**Pregunta de reflexión**: ¿Por qué usamos `fds` en la pila en vez de
strings con `getcwd`? ¿Qué escenario fallaría con strings pero no con fds?

---

### Ejercicio 2: find simplificado con openat

Implementa un `find` simplificado que liste recursivamente todos los
archivos en un directorio, usando **solo funciones `*at()`** — sin
cambiar el cwd del proceso en ningún momento:

```c
void find_recursive(int dirfd, const char *prefix);
```

Requisitos:
1. Abrir subdirectorios con `openat` + `fdopendir`.
2. Obtener info con `fstatat` (no `stat`).
3. No usar `chdir` en ningún momento.
4. Imprimir la ruta completa de cada archivo (construida con `prefix`).
5. No seguir symlinks (usar `AT_SYMLINK_NOFOLLOW`).
6. Verificar que el cwd no cambió al final con `getcwd`.

```
  $ ./my_find /usr/share/doc/bash
  /usr/share/doc/bash
  /usr/share/doc/bash/CHANGES
  /usr/share/doc/bash/FAQ
  /usr/share/doc/bash/INTRO
  /usr/share/doc/bash/README
  ...
```

**Predicción antes de codificar**: ¿Qué ventaja tiene `fdopendir(fd)`
sobre `opendir(path)` cuando ya tienes un fd abierto con `openat`?

**Pregunta de reflexión**: Si este programa fuera multihilo (un thread
por subdirectorio), ¿funcionaría correctamente? ¿Qué pasaría si en vez
de `openat` hubieras usado `chdir` + `opendir(".")`?

---

### Ejercicio 3: Sandbox de directorio

Escribe una función que ejecute un comando externo confinado a un
directorio específico, de modo que el comando no pueda acceder a archivos
fuera de ese directorio:

```c
int run_sandboxed(const char *sandbox_dir,
                  const char *cmd, char *const argv[]);
```

Implementación (entre fork y exec):
1. `chdir(sandbox_dir)`.
2. `chroot(sandbox_dir)` — cambia la raíz del filesystem (requiere root).
3. `chdir("/")` — después de chroot, `/` es el sandbox.
4. Drop privileges: `setgid(nobody)` + `setuid(nobody)`.
5. `execvp(cmd, argv)`.

Si `chroot` no está disponible (no root), implementa una versión
simplificada que solo use `chdir` y verifique que `cmd` no usa rutas
con `..` ni absolutas fuera del sandbox.

**Predicción antes de codificar**: ¿Por qué es necesario hacer `chdir("/")`
**después** de `chroot`? ¿Qué pasa si el cwd al momento del chroot está
fuera del nuevo root?

**Pregunta de reflexión**: `chroot` no es una medida de seguridad robusta
por sí sola — un proceso root dentro del chroot puede escapar. ¿Qué
mecanismos modernos de Linux (namespaces, seccomp, etc.) ofrecen un
aislamiento más fuerte?
