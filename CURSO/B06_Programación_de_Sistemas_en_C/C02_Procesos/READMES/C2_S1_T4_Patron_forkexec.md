# Patrón fork+exec — El Idioma Estándar de Unix para Lanzar Programas

## Índice

1. [El modelo de dos pasos de Unix](#1-el-modelo-de-dos-pasos-de-unix)
2. [El patrón canónico](#2-el-patrón-canónico)
3. [Configuración entre fork y exec](#3-configuración-entre-fork-y-exec)
4. [Redirección de I/O](#4-redirección-de-io)
5. [Pipes entre procesos](#5-pipes-entre-procesos)
6. [system() y popen()](#6-system-y-popen)
7. [posix_spawn: alternativa moderna](#7-posix_spawn-alternativa-moderna)
8. [Patrones reales](#8-patrones-reales)
9. [Seguridad y robustez](#9-seguridad-y-robustez)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El modelo de dos pasos de Unix

Unix separa la creación de procesos en **dos operaciones independientes**:

1. **fork()** — crea una copia del proceso actual.
2. **exec()** — reemplaza el programa de ese proceso por otro.

```
  Otros SO (Windows CreateProcess):
  ┌────────────────────────────────────┐
  │ CreateProcess("programa", args...) │  ← una sola operación
  │   → crea proceso + carga programa │
  └────────────────────────────────────┘

  Unix (fork + exec):
  ┌──────────┐    ┌──────────────┐    ┌──────────────┐
  │  fork()  │───▶│  configurar  │───▶│   exec()     │
  │ (copiar) │    │  (fds, env,  │    │  (reemplazar │
  │          │    │   dir, uid)  │    │   programa)  │
  └──────────┘    └──────────────┘    └──────────────┘
                         ▲
                    Ventana de
                    personalización
```

La **ventana entre fork y exec** es la clave. En ese momento, el hijo
es un proceso completo con su propia tabla de file descriptors, variables
de entorno, directorio de trabajo, máscara de señales, etc. Todo se puede
modificar **antes** de que exec cargue el nuevo programa.

### Por qué dos pasos

| Capacidad                          | fork+exec | CreateProcess (1 paso) |
|------------------------------------|-----------|------------------------|
| Redirigir stdout a un archivo      | Trivial   | Parámetros especiales  |
| Conectar pipes entre procesos      | Trivial   | Complejo               |
| Cambiar directorio de trabajo      | `chdir`   | Parámetro              |
| Modificar entorno                  | `setenv`  | Parámetro              |
| Cambiar usuario (drop privileges)  | `setuid`  | Parámetro              |
| Cerrar file descriptors heredados  | `close`   | Flag por cada handle   |
| Cualquier combinación de lo anterior| Composable| Explosión de parámetros|

La filosofía Unix: operaciones simples que se **componen**.

---

## 2. El patrón canónico

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int run_command(const char *path, char *const argv[])
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* ── Child process ── */
        execvp(path, argv);

        /* If we get here, exec failed */
        perror("exec");
        _exit(127);  /* Convention: 127 = command not found */
    }

    /* ── Parent process ── */
    int status;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);  /* Shell convention */

    return -1;
}

int main(void)
{
    char *argv[] = {"ls", "-la", "/tmp", NULL};
    int code = run_command("ls", argv);
    printf("ls exited with code %d\n", code);
    return code;
}
```

### Flujo del patrón

```
  Padre                          Hijo
  ──────                         ────
  fork() ─────────────────────▶  (nace como copia)
  │                               │
  │                               │ [ventana de config]
  │                               │ dup2, close, chdir...
  │                               │
  │                               execvp("ls", argv)
  │                               │
  │                               ┌─────────────┐
  │                               │ ls -la /tmp  │
  waitpid(pid) ◀── bloquea ──    │ (ejecuta)    │
  │                               │ exit(0)      │
  │                               └─────────────┘
  │ ◀── retorna ──────────────
  │
  status → WEXITSTATUS → 0
```

### Convenciones de exit code

| Código     | Significado                            |
|------------|----------------------------------------|
| `0`        | Éxito                                  |
| `1-125`    | Error definido por el programa         |
| `126`      | Permiso denegado (no ejecutable)       |
| `127`      | Comando no encontrado                  |
| `128+N`    | Matado por señal N (ej: 137 = SIGKILL) |

Estas convenciones las usa el shell. Tu programa debería seguirlas para
ser consistente con el ecosistema Unix.

---

## 3. Configuración entre fork y exec

La ventana entre fork y exec permite configurar el entorno del nuevo
programa. Todo lo que se modifica aquí afecta **solo al hijo**:

### Cambiar directorio de trabajo

```c
if (pid == 0) {
    if (chdir("/var/log") == -1) {
        perror("chdir");
        _exit(126);
    }
    execvp("ls", (char *[]){"ls", "-l", NULL});
    _exit(127);
}
```

### Modificar variables de entorno

```c
if (pid == 0) {
    setenv("LANG", "C", 1);          /* Override */
    setenv("MY_CONFIG", "/etc/app", 1);
    unsetenv("SECRET_TOKEN");         /* Remove from child */

    execvp("my_program", argv);
    _exit(127);
}
```

### Cambiar máscara de señales

```c
if (pid == 0) {
    /* Unblock all signals for the child */
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    /* Reset handlers to default (exec does this too,
       but only for caught signals, not blocked ones) */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigaction(SIGPIPE, &sa, NULL);

    execvp(argv[0], argv);
    _exit(127);
}
```

### Cambiar usuario (drop privileges)

```c
if (pid == 0) {
    /* Drop root privileges before executing */
    if (setgid(nobody_gid) == -1) _exit(126);
    if (setuid(nobody_uid) == -1) _exit(126);

    execvp("untrusted_program", argv);
    _exit(127);
}
```

### Cerrar file descriptors innecesarios

```c
if (pid == 0) {
    /* Close all fds except stdin/stdout/stderr */
    for (int fd = 3; fd < sysconf(_SC_OPEN_MAX); fd++)
        close(fd);  /* Ignore errors — some won't be open */

    execvp(argv[0], argv);
    _exit(127);
}
```

> **Mejor práctica**: Usar `O_CLOEXEC` al abrir archivos es más eficiente
> que cerrar en loop. `close_range(3, ~0U, 0)` (Linux 5.9+) cierra todo
> de una sola llamada.

---

## 4. Redirección de I/O

El caso de uso más importante de la ventana fork-exec. Así es como el
shell implementa `>`, `<` y `2>`:

### Redirigir stdout a un archivo

```c
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* Equivalent to: ls -la /tmp > /tmp/listing.txt */
int main(void)
{
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return 1; }

    if (pid == 0) {
        /* Redirect stdout to file */
        int fd = open("/tmp/listing.txt",
                      O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd == -1) { perror("open"); _exit(126); }

        dup2(fd, STDOUT_FILENO);  /* fd → stdout */
        close(fd);                /* Original fd no longer needed */

        execlp("ls", "ls", "-la", "/tmp", NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
```

```
  Antes de dup2:              Después de dup2:
  ┌──────────────┐            ┌──────────────┐
  │ fd 0 → stdin │            │ fd 0 → stdin │
  │ fd 1 → tty   │            │ fd 1 → file  │ ← redirigido
  │ fd 2 → tty   │            │ fd 2 → tty   │
  │ fd 3 → file  │            │ fd 3 → cerrado│ ← close(fd)
  └──────────────┘            └──────────────┘

  Después de exec:
  ┌──────────────┐
  │ fd 0 → stdin │  ls escribe a fd 1 (= file)
  │ fd 1 → file  │  sin saber que no es la terminal
  │ fd 2 → tty   │
  └──────────────┘
```

### Redirigir stderr

```c
if (pid == 0) {
    int fd = open("/tmp/errors.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) _exit(126);

    dup2(fd, STDERR_FILENO);  /* fd → stderr */
    close(fd);

    execlp("make", "make", "all", NULL);
    _exit(127);
}
```

### Redirigir stdin desde archivo

```c
if (pid == 0) {
    /* Equivalent to: sort < data.txt */
    int fd = open("data.txt", O_RDONLY);
    if (fd == -1) _exit(126);

    dup2(fd, STDIN_FILENO);
    close(fd);

    execlp("sort", "sort", NULL);
    _exit(127);
}
```

### Combinar: stdout y stderr al mismo destino

```c
if (pid == 0) {
    /* Equivalent to: cmd > output.log 2>&1 */
    int fd = open("output.log",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) _exit(126);

    dup2(fd, STDOUT_FILENO);  /* stdout → file */
    dup2(fd, STDERR_FILENO);  /* stderr → same file */
    close(fd);

    execvp(argv[0], argv);
    _exit(127);
}
```

---

## 5. Pipes entre procesos

Así implementa el shell `ls | grep .c | wc -l`:

### Pipe simple: dos procesos

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* Implement: ls /tmp | grep log */
int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return 1; }

    /* First child: ls (writes to pipe) */
    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(pipefd[0]);                 /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO);   /* stdout → pipe write */
        close(pipefd[1]);

        execlp("ls", "ls", "/tmp", NULL);
        _exit(127);
    }

    /* Second child: grep (reads from pipe) */
    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(pipefd[1]);                 /* Close write end */
        dup2(pipefd[0], STDIN_FILENO);    /* stdin → pipe read */
        close(pipefd[0]);

        execlp("grep", "grep", "log", NULL);
        _exit(127);
    }

    /* Parent: close both ends and wait */
    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}
```

```
  ┌──────────┐   pipe    ┌──────────┐
  │    ls    │──────────▶│  grep    │
  │ stdout=W │  [R]──[W] │ stdin=R  │
  └──────────┘           └──────────┘

  Padre cierra ambos extremos (no los usa).
  Cada hijo cierra el extremo que no usa.
  → Cuando ls termina, se cierra W → grep lee EOF → grep termina.
```

### Por qué cerrar extremos es crítico

Si el padre **no cierra** `pipefd[1]`, el extremo de escritura queda
abierto en el padre. `grep` nunca recibirá EOF porque el pipe aún tiene
un escritor (el padre). `grep` se colgará esperando más datos.

Regla: **cada proceso cierra los extremos del pipe que no usa**.

### Pipeline genérico de N comandos

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    const char *path;
    char *const *argv;
} command_t;

int run_pipeline(command_t *cmds, int n)
{
    int prev_fd = -1;  /* Read end of previous pipe */
    pid_t *pids = calloc(n, sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        int pipefd[2] = {-1, -1};

        /* Create pipe between this command and the next (not for last) */
        if (i < n - 1) {
            if (pipe(pipefd) == -1) { perror("pipe"); return -1; }
        }

        pids[i] = fork();
        if (pids[i] == -1) { perror("fork"); return -1; }

        if (pids[i] == 0) {
            /* Connect stdin to previous pipe's read end */
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            /* Connect stdout to this pipe's write end */
            if (pipefd[1] != -1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            /* Close unused read end */
            if (pipefd[0] != -1)
                close(pipefd[0]);

            execvp(cmds[i].path, cmds[i].argv);
            _exit(127);
        }

        /* Parent: close fds we've handed off */
        if (prev_fd != -1) close(prev_fd);
        if (pipefd[1] != -1) close(pipefd[1]);

        prev_fd = pipefd[0];  /* Save read end for next iteration */
    }

    /* Wait for all children */
    int last_status = 0;
    for (int i = 0; i < n; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == n - 1)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }

    free(pids);
    return last_status;
}
```

---

## 6. system() y popen()

La biblioteca estándar ofrece atajos que encapsulan fork+exec+wait:

### system()

```c
#include <stdlib.h>

int status = system("ls -la /tmp > /tmp/listing.txt");
```

`system` hace: `fork` → exec `/bin/sh -c "command"` → `waitpid`.

```c
/* system() es aproximadamente esto: */
int my_system(const char *cmd)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return status;
}
```

**Problemas de system()**:

| Problema                    | Impacto                            |
|-----------------------------|------------------------------------|
| Invoca `/bin/sh`            | Overhead del shell                 |
| Pasa string al shell        | **Inyección de comandos** si cmd tiene input del usuario |
| Bloquea SIGCHLD             | Interfiere con otros hijos         |
| No da PID del hijo          | No se puede enviar señales         |
| No permite redirecciones custom | Solo lo que el shell string dice |

> **Regla**: `system()` es aceptable para prototipos rápidos y scripts.
> **Nunca** lo uses con input del usuario sin sanitizar.

### popen()

```c
#include <stdio.h>

/* Read from a command's stdout */
FILE *fp = popen("ls /tmp", "r");
char line[256];
while (fgets(line, sizeof(line), fp)) {
    printf("file: %s", line);
}
int status = pclose(fp);

/* Write to a command's stdin */
FILE *fp2 = popen("sort > /tmp/sorted.txt", "w");
fprintf(fp2, "banana\napple\ncherry\n");
pclose(fp2);
```

`popen` hace: `pipe` + `fork` + exec `sh -c "cmd"`. Devuelve un `FILE*`
conectado al stdin o stdout del hijo.

Tiene los **mismos problemas de seguridad** que `system()` (usa el shell).

---

## 7. posix_spawn: alternativa moderna

`posix_spawn` combina fork+exec en una sola llamada, evitando la ventana
entre ambos. Es más eficiente en sistemas sin copy-on-write y más seguro
en programas multihilo:

```c
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

int main(void)
{
    pid_t pid;
    char *argv[] = {"ls", "-la", "/tmp", NULL};

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    /* Redirect stdout to file (like dup2 between fork and exec) */
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
        "/tmp/listing.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    int err = posix_spawnp(&pid, "ls", &actions, &attr, argv, environ);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);

    if (err != 0) {
        fprintf(stderr, "spawn: %s\n", strerror(err));
        return 1;
    }

    int status;
    waitpid(pid, &status, 0);
    printf("exit code: %d\n", WEXITSTATUS(status));

    return 0;
}
```

### file_actions: configurar fds

```c
posix_spawn_file_actions_t actions;
posix_spawn_file_actions_init(&actions);

/* Open file as fd N */
posix_spawn_file_actions_addopen(&actions, fd, path, flags, mode);

/* Close a fd */
posix_spawn_file_actions_addclose(&actions, fd);

/* dup2(oldfd, newfd) */
posix_spawn_file_actions_adddup2(&actions, oldfd, newfd);
```

### Cuándo usar posix_spawn

| Situación                        | Recomendación      |
|----------------------------------|--------------------|
| Programa multihilo               | `posix_spawn` (fork no es thread-safe en la práctica) |
| Configuración simple (redir fds) | `posix_spawn` es más limpio |
| Configuración compleja (setuid, cgroups, chroot) | fork+exec (más flexible) |
| Máximo rendimiento sin CoW       | `posix_spawn` (puede usar vfork internamente) |
| Portabilidad POSIX               | `posix_spawn` (estándar desde POSIX.1-2001) |

---

## 8. Patrones reales

### Patrón 1: Capturar stdout de un comando

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Run a command and capture its stdout into a buffer */
char *capture_output(const char *path, char *const argv[], int *exit_code)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execvp(path, argv);
        _exit(127);
    }

    close(pipefd[1]);

    /* Read all output */
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char tmp[4096];
    ssize_t n;

    while ((n = read(pipefd[0], tmp, sizeof(tmp))) > 0) {
        if (len + n >= cap) {
            cap = (cap == 0) ? 4096 : cap * 2;
            buf = realloc(buf, cap);
        }
        memcpy(buf + len, tmp, n);
        len += n;
    }
    close(pipefd[0]);

    if (buf) buf[len] = '\0';

    int status;
    waitpid(pid, &status, 0);
    if (exit_code)
        *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return buf;
}
```

### Patrón 2: Ejecutar con timeout

```c
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

int run_with_timeout(char *const argv[], int timeout_secs)
{
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    /* Poll with timeout */
    for (int elapsed = 0; elapsed < timeout_secs; elapsed++) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r > 0)
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        sleep(1);
    }

    /* Timeout: kill gracefully, then forcefully */
    kill(pid, SIGTERM);
    sleep(2);

    if (waitpid(pid, NULL, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }

    return -2;  /* Timeout */
}
```

### Patrón 3: Daemon double-fork

Para crear un daemon que se desacopla completamente del proceso padre:

```c
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int daemonize(void)
{
    /* First fork: parent exits, child continues */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  /* Parent exits */

    /* Child becomes session leader */
    if (setsid() == -1) return -1;

    /* Second fork: prevent terminal reacquisition */
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  /* First child exits */

    /* Grandchild: the actual daemon */
    umask(0);
    chdir("/");

    /* Close stdin/stdout/stderr, redirect to /dev/null */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);  /* fd 0 */
    open("/dev/null", O_WRONLY);  /* fd 1 */
    open("/dev/null", O_WRONLY);  /* fd 2 */

    return 0;
}
```

```
  Terminal               Padre              Hijo 1            Hijo 2 (daemon)
  ───────               ──────              ──────            ───────────────
     │                    │                    │
     │ fork() ───────────▶│                    │
     │ ◀── parent exits ──│                    │
     │                    │ fork() ──────────▶│
     │                    │ ◀── child exits ──│
     │                    │                   │ setsid()
     │                    │                   │ fork() ──────▶│
     │                    │                   │ ◀── exits ────│
     │                    │                   │               │ (daemon runs)
     │                    │                   │               │ PPID = 1
     ×                    ×                   ×               │ ...
```

**¿Por qué doble fork?** El primer fork + `setsid()` crea una nueva sesión.
El segundo fork garantiza que el daemon **no es líder de sesión**, así no
puede adquirir accidentalmente una terminal de control.

> **Nota moderna**: `systemd` maneja la daemonización. Si tu daemon será
> gestionado por systemd, no necesitas double-fork — ejecuta en primer plano
> y deja que systemd lo gestione.

---

## 9. Seguridad y robustez

### Nunca pasar input del usuario al shell

```c
/* PELIGRO: inyección de comandos */
char cmd[256];
snprintf(cmd, sizeof(cmd), "ls %s", user_input);
system(cmd);  /* Si user_input = "; rm -rf /" → desastre */

/* SEGURO: usar execvp directamente */
pid_t pid = fork();
if (pid == 0) {
    execlp("ls", "ls", user_input, NULL);  /* user_input es UN argumento */
    _exit(127);
}
```

Con `execvp`, `user_input` se pasa como un **argumento** a `ls`, no como
parte de un comando shell. No hay interpretación de `;`, `|`, `$()`, etc.

### Cerrar file descriptors heredados

Los fds abiertos se heredan al hijo. Esto puede filtrar conexiones de
base de datos, sockets, archivos sensibles:

```c
/* Al abrir archivos, usar O_CLOEXEC */
int fd = open(path, O_RDONLY | O_CLOEXEC);
int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

/* O cerrar todo antes de exec (Linux 5.9+) */
#include <linux/close_range.h>
if (pid == 0) {
    close_range(3, ~0U, 0);  /* Close all fds >= 3 */
    execvp(argv[0], argv);
    _exit(127);
}
```

### Limpiar señales heredadas

```c
if (pid == 0) {
    /* Reset signal disposition */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    for (int sig = 1; sig < NSIG; sig++)
        sigaction(sig, &sa, NULL);  /* Ignore errors for unblockable */

    /* Unblock all signals */
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    execvp(argv[0], argv);
    _exit(127);
}
```

> **Nota**: `exec` ya resetea los handlers a `SIG_DFL` (las señales con
> handler custom). Pero señales con `SIG_IGN` y la máscara de señales
> bloqueadas **se heredan**. Hay que limpiarlas manualmente.

---

## 10. Errores comunes

### Error 1: No verificar el retorno de exec

```c
/* MAL: si exec falla, el hijo continúa ejecutando código del padre */
if (pid == 0) {
    execvp("nonexistent", argv);
    /* Falls through! Child runs parent's code */
    printf("parent code running in child!\n");
    /* Now two processes run the same parent code → chaos */
}

/* BIEN: _exit inmediatamente si exec falla */
if (pid == 0) {
    execvp("nonexistent", argv);
    perror("exec");
    _exit(127);
}
```

Si exec falla y no hay `_exit`, el hijo ejecuta el código que viene
después del `if (pid == 0)`, **incluyendo el código del padre** como
`waitpid`. Esto crea bugs muy difíciles de diagnosticar.

### Error 2: Usar exit() en lugar de _exit() en el hijo

```c
/* MAL: exit() flushea buffers de stdio del padre */
if (pid == 0) {
    execvp(argv[0], argv);
    exit(1);  /* Flushes parent's buffered output → duplicated */
}

/* BIEN: _exit() no ejecuta atexit ni flushea */
if (pid == 0) {
    execvp(argv[0], argv);
    _exit(127);
}
```

`exit()` llama a handlers `atexit`, flushea `stdio` buffers y cierra
streams. Esos buffers contienen datos del padre que se duplicarían.
`_exit()` termina limpiamente sin side effects.

### Error 3: No cerrar extremos del pipe no usados

```c
/* MAL: grep nunca recibe EOF porque padre aún tiene write end abierto */
pipe(pipefd);
if (fork() == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    execlp("ls", "ls", NULL);
    _exit(127);
}
if (fork() == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    execlp("grep", "grep", "txt", NULL);
    _exit(127);
    /* grep waits forever — parent still has pipefd[1] open */
}

/* BIEN: cerrar ambos extremos en el padre */
close(pipefd[0]);
close(pipefd[1]);
```

### Error 4: Race condition con señales entre fork y exec

```c
/* MAL: la señal puede llegar entre fork y exec, matando al hijo
   con el handler del padre */
signal(SIGUSR1, parent_handler);
pid_t pid = fork();
if (pid == 0) {
    /* Window: SIGUSR1 here executes parent_handler in child */
    execvp(argv[0], argv);
    _exit(127);
}

/* BIEN: bloquear señales antes de fork, desbloquear en hijo después de config */
sigset_t mask, old;
sigemptyset(&mask);
sigaddset(&mask, SIGUSR1);
sigprocmask(SIG_BLOCK, &mask, &old);

pid_t pid = fork();
if (pid == 0) {
    sigprocmask(SIG_SETMASK, &old, NULL);  /* Restore child's mask */
    execvp(argv[0], argv);
    _exit(127);
}
sigprocmask(SIG_SETMASK, &old, NULL);  /* Restore parent's mask */
```

### Error 5: Olvidar esperar al hijo

```c
/* MAL: padre no espera → hijo se vuelve zombie */
pid_t pid = fork();
if (pid == 0) {
    execvp("long_running_task", argv);
    _exit(127);
}
/* Parent continues without waitpid... zombie accumulates */

/* BIEN: esperar, o ignorar SIGCHLD si no importa el resultado */
signal(SIGCHLD, SIG_IGN);  /* Auto-reap: no zombies */

pid_t pid = fork();
if (pid == 0) {
    execvp("background_task", argv);
    _exit(127);
}
/* Parent continues, kernel reaps child automatically */
```

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                  Patrón fork+exec                            │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Canónico:                                                   │
  │    pid = fork();                                             │
  │    if (pid == 0) {                                           │
  │        /* config: dup2, chdir, setenv, close... */           │
  │        execvp(path, argv);                                   │
  │        _exit(127);           ← SIEMPRE _exit, no exit       │
  │    }                                                         │
  │    waitpid(pid, &status, 0);                                 │
  │                                                              │
  │  Redirección I/O (entre fork y exec):                        │
  │    open(file) → dup2(fd, STDOUT) → close(fd)                │
  │    open(file) → dup2(fd, STDIN)  → close(fd)                │
  │                                                              │
  │  Pipe:                                                       │
  │    pipe(pfd) → fork → hijo1: dup2(pfd[1],1), close ambos   │
  │                     → hijo2: dup2(pfd[0],0), close ambos   │
  │                     → padre: close ambos extremos            │
  │                                                              │
  │  Atajos (NO usar con input del usuario):                     │
  │    system("cmd")       fork+sh -c+wait                      │
  │    popen("cmd","r")    pipe+fork+sh -c                      │
  │                                                              │
  │  posix_spawn:                                                │
  │    file_actions: addopen, addclose, adddup2                  │
  │    posix_spawnp(&pid, cmd, &actions, &attr, argv, environ)  │
  │                                                              │
  │  Seguridad:                                                  │
  │    ✗ system(user_input)   → inyección de comandos            │
  │    ✓ execvp + argv        → sin interpretación shell         │
  │    ✓ O_CLOEXEC           → no filtrar fds al hijo           │
  │    ✓ close_range(3,~0,0) → cerrar todo (Linux 5.9+)        │
  │                                                              │
  │  Exit codes:                                                 │
  │    0=ok, 1-125=error, 126=no exec, 127=not found, 128+N=sig│
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Mini-shell con redirección

Implementa un programa que lea comandos de stdin y los ejecute. Debe
soportar:

1. Comandos simples: `ls -la /tmp`
2. Redirección de salida: `ls > output.txt`
3. Redirección de entrada: `sort < data.txt`
4. Comando `exit` para salir.

No hace falta soportar pipes ni comillas. Tokenizar por espacios es
suficiente. Usar `fork+exec+waitpid` (no `system`).

```
  $ ./minishell
  > ls /tmp
  file1.txt  file2.log
  > ls /tmp > listing.txt
  > cat listing.txt
  file1.txt  file2.log
  > sort < listing.txt
  file1.txt
  file2.log
  > exit
```

**Predicción antes de codificar**: Si el usuario escribe `ls > >`, ¿qué
debería hacer tu parser? ¿Y si el archivo de redirección no se puede abrir?

**Pregunta de reflexión**: ¿Por qué usas `_exit(127)` cuando exec falla
en el hijo, y no `exit(1)` ni `return 1`?

---

### Ejercicio 2: Pipeline de N comandos

Escribe una función que reciba un array de comandos y los conecte con pipes,
como haría el shell con `cmd1 | cmd2 | cmd3`:

```c
/* Usage: */
char *ls_argv[]   = {"ls", "/usr/bin", NULL};
char *grep_argv[] = {"grep", "python", NULL};
char *wc_argv[]   = {"wc", "-l", NULL};

command_t pipeline[] = {
    {"ls",   ls_argv},
    {"grep", grep_argv},
    {"wc",   wc_argv},
};

int status = run_pipeline(pipeline, 3);
```

Requisitos:
- Crear N-1 pipes para N comandos.
- Cada hijo cierra los extremos que no usa.
- El padre cierra todos los extremos y espera a todos los hijos.
- Retornar el exit code del **último** comando (como hace bash).

**Predicción antes de codificar**: Si tienes 3 comandos, ¿cuántos pipes
creas? ¿Cuántos file descriptors tiene abiertos el padre antes de cerrarlos?

**Pregunta de reflexión**: ¿Qué pasa si el segundo comando del pipeline
termina antes que el primero? ¿El primero recibe una señal? ¿Cuál?

---

### Ejercicio 3: Proceso supervisor

Implementa un supervisor que mantenga vivo un proceso hijo. Si el hijo
muere, el supervisor lo relanza automáticamente (hasta un máximo de
reintentos):

```c
typedef struct {
    const char *path;
    char *const *argv;
    int   max_restarts;
    int   restart_delay_secs;
} supervisor_config_t;

void supervise(const supervisor_config_t *config);
```

Comportamiento:
1. Lanzar el hijo con fork+exec.
2. Esperar con `waitpid`.
3. Si el hijo salió con código != 0 o fue matado por señal, imprimir
   diagnóstico y relanzar después de `restart_delay_secs`.
4. Si salió con código 0, terminar (éxito limpio, no relanzar).
5. Si se superó `max_restarts`, terminar con error.
6. Manejar `SIGTERM` en el supervisor: enviar `SIGTERM` al hijo y salir
   limpiamente.

```
  $ ./supervisor /usr/local/bin/my_server
  [supervisor] started child PID 1234
  [supervisor] child 1234 killed by signal 11 (Segmentation fault)
  [supervisor] restart 1/5, waiting 2s...
  [supervisor] started child PID 1235
  [supervisor] child 1235 exited with code 0
  [supervisor] clean exit, stopping.
```

**Predicción antes de codificar**: ¿Qué pasa si el hijo muere más rápido
de lo que tarda `restart_delay_secs`? ¿Podrías agotar `max_restarts` en
segundos?

**Pregunta de reflexión**: Los supervisores reales (systemd, supervisord)
implementan "backoff exponencial" — ¿por qué un delay fijo podría ser
problemático cuando un servicio falla repetidamente?
