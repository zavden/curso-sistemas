# Señales Comunes — SIGINT, SIGTERM, SIGCHLD, SIGUSR, SIGPIPE, SIGALRM

## Índice

1. [Panorama de señales estándar](#1-panorama-de-señales-estándar)
2. [SIGINT y SIGQUIT: control desde terminal](#2-sigint-y-sigquit-control-desde-terminal)
3. [SIGTERM y SIGHUP: terminación y recarga](#3-sigterm-y-sighup-terminación-y-recarga)
4. [SIGCHLD: notificación de hijos](#4-sigchld-notificación-de-hijos)
5. [SIGPIPE: escritura a pipe roto](#5-sigpipe-escritura-a-pipe-roto)
6. [SIGALRM: temporizadores](#6-sigalrm-temporizadores)
7. [SIGUSR1 y SIGUSR2: señales de usuario](#7-sigusr1-y-sigusr2-señales-de-usuario)
8. [SIGSEGV, SIGBUS, SIGFPE: errores fatales](#8-sigsegv-sigbus-sigfpe-errores-fatales)
9. [SIGTSTP, SIGCONT: control de jobs](#9-sigtstp-sigcont-control-de-jobs)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Panorama de señales estándar

Las señales 1-31 son las señales estándar de Unix. Cada una tiene un
número, nombre, acción por defecto y propósito específico:

```
  Señales estándar (1-31):

  ┌─────┬──────────┬───────────┬──────────────────────────────┐
  │ Num │ Nombre   │ Default   │ Propósito                    │
  ├─────┼──────────┼───────────┼──────────────────────────────┤
  │  1  │ SIGHUP   │ Terminate │ Hangup / recarga config      │
  │  2  │ SIGINT   │ Terminate │ Ctrl+C                       │
  │  3  │ SIGQUIT  │ Core dump │ Ctrl+\ (quit + dump)         │
  │  4  │ SIGILL   │ Core dump │ Instrucción ilegal           │
  │  5  │ SIGTRAP  │ Core dump │ Breakpoint (debugger)        │
  │  6  │ SIGABRT  │ Core dump │ abort()                      │
  │  7  │ SIGBUS   │ Core dump │ Error de bus (alineamiento)  │
  │  8  │ SIGFPE   │ Core dump │ Error aritmético             │
  │  9  │ SIGKILL  │ Terminate │ Matar (no capturable)        │
  │ 10  │ SIGUSR1  │ Terminate │ Definida por usuario         │
  │ 11  │ SIGSEGV  │ Core dump │ Violación de segmento        │
  │ 12  │ SIGUSR2  │ Terminate │ Definida por usuario         │
  │ 13  │ SIGPIPE  │ Terminate │ Pipe roto                    │
  │ 14  │ SIGALRM  │ Terminate │ Alarma de timer              │
  │ 15  │ SIGTERM  │ Terminate │ Terminar (graceful)          │
  │ 17  │ SIGCHLD  │ Ignore    │ Hijo cambió de estado        │
  │ 18  │ SIGCONT  │ Continue  │ Reanudar proceso detenido    │
  │ 19  │ SIGSTOP  │ Stop      │ Detener (no capturable)      │
  │ 20  │ SIGTSTP  │ Stop      │ Ctrl+Z                       │
  │ 21  │ SIGTTIN  │ Stop      │ Lectura de terminal en bg    │
  │ 22  │ SIGTTOU  │ Stop      │ Escritura a terminal en bg   │
  │ 23  │ SIGURG   │ Ignore    │ Datos urgentes en socket     │
  │ 24  │ SIGXCPU  │ Core dump │ Límite de CPU excedido       │
  │ 25  │ SIGXFSZ  │ Core dump │ Límite de tamaño de archivo  │
  │ 26  │ SIGVTALRM│ Terminate │ Timer de tiempo virtual      │
  │ 27  │ SIGPROF  │ Terminate │ Timer de profiling           │
  │ 28  │ SIGWINCH │ Ignore    │ Ventana de terminal cambió   │
  │ 29  │ SIGIO    │ Terminate │ I/O asíncrono                │
  │ 30  │ SIGPWR   │ Terminate │ Fallo de energía             │
  │ 31  │ SIGSYS   │ Core dump │ Syscall inválida (seccomp)   │
  └─────┴──────────┴───────────┴──────────────────────────────┘

  Nota: los números pueden variar entre arquitecturas.
  Usar siempre los nombres simbólicos (SIGINT, no 2).
```

---

## 2. SIGINT y SIGQUIT: control desde terminal

### SIGINT (2) — Interrupción interactiva

Se envía cuando el usuario presiona **Ctrl+C**. Es la forma estándar de
pedirle a un programa interactivo que se detenga:

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile sig_atomic_t interrupted = 0;

void sigint_handler(int sig)
{
    (void)sig;
    interrupted = 1;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sigint_handler,
        .sa_flags   = 0,  /* Don't restart — let read() return EINTR */
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    char buf[256];
    printf("Type something (Ctrl+C to stop):\n");

    while (!interrupted) {
        printf("> ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            if (interrupted) {
                printf("\ninterrupted!\n");
                break;
            }
            break;  /* EOF */
        }
        printf("you said: %s", buf);
    }

    printf("cleaning up...\n");
    return 0;
}
```

### SIGQUIT (3) — Quit con core dump

Se envía con **Ctrl+\\**. A diferencia de SIGINT, la acción por defecto
genera un **core dump**. Es la señal de "algo va mal, quiero un dump
para depurar":

```bash
$ ./my_server
^\ Quit (core dumped)     # Ctrl+\

$ gdb ./my_server core    # Analyze the dump
```

### Cuándo capturar SIGINT

| Programa           | Acción recomendada con SIGINT               |
|--------------------|----------------------------------------------|
| Script/utilidad    | No capturar (SIG_DFL: terminar)              |
| Servidor           | Capturar → graceful shutdown                 |
| Editor/aplicación  | Capturar → preguntar si guardar              |
| Shell              | Capturar → cancelar comando actual, no salir |

### El grupo de procesos y la terminal

Ctrl+C envía SIGINT a **todo el grupo de procesos foreground**, no solo
a un proceso:

```
  Terminal
    │
    │ Ctrl+C → SIGINT
    │
    ▼
  ┌─────────────────────────┐
  │ Foreground process group │
  │                         │
  │  Shell ──fork──▶ Padre  │
  │                   │     │
  │                fork     │
  │                   │     │
  │                 Hijo    │  ← Todos reciben SIGINT
  └─────────────────────────┘
```

---

## 3. SIGTERM y SIGHUP: terminación y recarga

### SIGTERM (15) — Terminación limpia

Es la señal estándar para pedir a un proceso que **termine limpiamente**.
Es la señal que `kill` envía por defecto (sin `-9`):

```bash
kill 1234        # Sends SIGTERM
kill -TERM 1234  # Same thing explicitly
```

```c
volatile sig_atomic_t should_exit = 0;

void sigterm_handler(int sig)
{
    (void)sig;
    should_exit = 1;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sigterm_handler,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);

    /* Main loop */
    while (!should_exit) {
        process_next_request();
    }

    /* Graceful shutdown */
    close_connections();
    flush_buffers();
    save_state();
    printf("shutdown complete\n");
    return 0;
}
```

### SIGHUP (1) — Hangup / recarga

Originalmente significaba "la terminal se desconectó" (modem hangup).
Hoy tiene **dos usos**:

**Uso 1: Terminal cerrada** — se envía cuando el terminal de control se
cierra (ssh desconecta, xterm se cierra). Los procesos que no lo manejan
mueren:

```bash
# nohup evita que SIGHUP mate al proceso
nohup ./my_server &
# → SIGHUP will be ignored
```

**Uso 2: Recargar configuración** — Por convención, los daemons
interpretan SIGHUP como "releer tu archivo de configuración":

```c
volatile sig_atomic_t reload_config = 0;

void sighup_handler(int sig)
{
    (void)sig;
    reload_config = 1;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sighup_handler,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);

    load_config("/etc/myapp/config.ini");

    while (!should_exit) {
        if (reload_config) {
            reload_config = 0;
            printf("reloading config...\n");
            load_config("/etc/myapp/config.ini");
        }
        process_next_request();
    }
}
```

```bash
# Recargar config de nginx, sshd, etc:
kill -HUP $(pidof nginx)
systemctl reload nginx    # Also sends SIGHUP internally
```

---

## 4. SIGCHLD: notificación de hijos

Se envía al padre cuando un hijo **termina**, se **detiene** (SIGSTOP) o
se **reanuda** (SIGCONT). La acción por defecto es **ignorar** (pero el
zombie se crea igualmente).

### Reaping asíncrono con SIGCHLD

```c
void sigchld_handler(int sig)
{
    (void)sig;
    /* Reap ALL available children — signals don't queue */
    int saved_errno = errno;  /* Save errno (waitpid modifies it) */

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;  /* Restore errno */
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags   = SA_RESTART | SA_NOCLDSTOP,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    /* ... fork children, continue working ... */
}
```

### Puntos clave

```
  ┌─────────────────────────────────────────────────────────┐
  │ SIGCHLD gotchas:                                        │
  │                                                         │
  │ 1. Las señales NO se encolan                            │
  │    → 3 hijos mueren = quizá 1 SIGCHLD                  │
  │    → SIEMPRE usar while(waitpid(-1,...,WNOHANG)>0)     │
  │                                                         │
  │ 2. SA_NOCLDSTOP                                         │
  │    → Sin este flag, SIGCHLD llega en stop/continue      │
  │    → Con flag, solo llega en muerte del hijo            │
  │                                                         │
  │ 3. Guardar/restaurar errno                              │
  │    → waitpid modifica errno                             │
  │    → El código principal puede estar chequeando errno   │
  │                                                         │
  │ 4. SIG_IGN = auto-reap                                  │
  │    → signal(SIGCHLD, SIG_IGN) = no zombies, no wait     │
  │    → Pero pierdes el exit status de los hijos           │
  └─────────────────────────────────────────────────────────┘
```

### Con SA_SIGINFO: obtener detalles del hijo

```c
void sigchld_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)sig;
    (void)ctx;

    /* info has details without needing waitpid */
    pid_t pid    = info->si_pid;
    int   status = info->si_status;
    int   code   = info->si_code;

    /* si_code tells us what happened */
    if (code == CLD_EXITED) {
        /* Child called exit(status) */
    } else if (code == CLD_KILLED || code == CLD_DUMPED) {
        /* Child killed by signal si_status */
    }

    /* Still need waitpid to reap the zombie */
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

struct sigaction sa = {
    .sa_sigaction = sigchld_handler,
    .sa_flags     = SA_SIGINFO | SA_RESTART | SA_NOCLDSTOP,
};
```

---

## 5. SIGPIPE: escritura a pipe roto

Se envía cuando un proceso escribe a un pipe o socket cuyo **extremo de
lectura fue cerrado**. La acción por defecto **mata** al proceso:

```
  Escritor                   Lector
  ┌──────────┐    pipe      ┌──────────┐
  │ write()  │─────────────▶│ close()  │ ← lector cierra
  │          │              │ (muerto) │
  │ write()  │──── SIGPIPE! │          │
  │ (muere)  │              └──────────┘
  └──────────┘
```

### El problema clásico

```bash
# yes writes infinitely, head reads 10 lines and exits
yes | head -10

# Without SIGPIPE handling:
# head exits → pipe read end closes → yes gets SIGPIPE → yes dies
# This is the DESIRED behavior — yes should stop

# But in a server program, you don't want SIGPIPE to kill you:
# Client disconnects → server writes to closed socket → SIGPIPE → server dies!
```

### Ignorar SIGPIPE en servidores

Casi todos los servidores de red ignoran SIGPIPE y manejan el error
con `EPIPE` en el valor de retorno de `write`:

```c
/* At startup: */
signal(SIGPIPE, SIG_IGN);  /* One of the few valid uses of signal() */

/* Then in the write path: */
ssize_t n = write(client_fd, buf, len);
if (n == -1) {
    if (errno == EPIPE) {
        /* Client disconnected — clean up this connection */
        close(client_fd);
        remove_client(client_fd);
    } else {
        perror("write");
    }
}
```

### MSG_NOSIGNAL: por socket

Para sockets, se puede evitar SIGPIPE por operación sin ignorarla
globalmente:

```c
/* No SIGPIPE for this specific send */
ssize_t n = send(client_fd, buf, len, MSG_NOSIGNAL);
if (n == -1 && errno == EPIPE) {
    /* Handle disconnection */
}
```

### SO_NOSIGPIPE (macOS/BSD)

```c
/* BSD/macOS: disable SIGPIPE per-socket */
int one = 1;
setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
/* Not available on Linux — use MSG_NOSIGNAL or SIG_IGN instead */
```

---

## 6. SIGALRM: temporizadores

Se envía cuando expira un timer configurado con `alarm()`. Solo hay
**un** timer de alarma por proceso:

```c
#include <unistd.h>

unsigned int alarm(unsigned int seconds);
```

- Programa una alarma que enviará SIGALRM después de `seconds` segundos.
- Retorna los segundos restantes de la alarma anterior (0 si no había).
- `alarm(0)` **cancela** la alarma pendiente.

### Timeout para operaciones

```c
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

volatile sig_atomic_t timed_out = 0;

void alarm_handler(int sig)
{
    (void)sig;
    timed_out = 1;
}

/* Read with timeout */
ssize_t read_with_timeout(int fd, void *buf, size_t len, int timeout_secs)
{
    struct sigaction sa = {
        .sa_handler = alarm_handler,
        .sa_flags   = 0,  /* NO SA_RESTART — let read() return EINTR */
    };
    sigemptyset(&sa.sa_mask);

    struct sigaction old_sa;
    sigaction(SIGALRM, &sa, &old_sa);

    timed_out = 0;
    alarm(timeout_secs);

    ssize_t n = read(fd, buf, len);
    int saved_errno = errno;

    alarm(0);  /* Cancel alarm */
    sigaction(SIGALRM, &old_sa, NULL);  /* Restore old handler */

    if (n == -1 && saved_errno == EINTR && timed_out) {
        errno = ETIMEDOUT;
        return -1;
    }

    errno = saved_errno;
    return n;
}
```

### Limitaciones de alarm()

| Limitación                      | Alternativa                    |
|---------------------------------|--------------------------------|
| Solo un timer por proceso       | `timer_create` (múltiples)     |
| Resolución de 1 segundo         | `setitimer` (microsegundos)    |
| Interfiere con `sleep()`        | `nanosleep` (no usa SIGALRM)  |
| Señal asíncrona (handler)       | `timerfd_create` (fd, con poll)|

> **Nota**: `sleep()` está implementado con `alarm()` en algunos sistemas.
> Mezclar `sleep` y `alarm` puede causar interferencia. Usar `nanosleep`
> es más seguro.

### setitimer: mayor resolución

```c
#include <sys/time.h>

/* Fire SIGALRM every 100ms */
struct itimerval timer = {
    .it_interval = { .tv_sec = 0, .tv_usec = 100000 },  /* repeat */
    .it_value    = { .tv_sec = 0, .tv_usec = 100000 },  /* first fire */
};
setitimer(ITIMER_REAL, &timer, NULL);

/* ITIMER_REAL    → SIGALRM  (wall-clock time) */
/* ITIMER_VIRTUAL → SIGVTALRM (user CPU time) */
/* ITIMER_PROF    → SIGPROF  (user + system CPU time) */
```

---

## 7. SIGUSR1 y SIGUSR2: señales de usuario

Son las únicas señales **sin significado predefinido**. El programador
decide qué hacen. La acción por defecto es terminar el proceso:

### Casos de uso comunes

| Software    | SIGUSR1                      | SIGUSR2                     |
|-------------|------------------------------|-----------------------------|
| nginx       | Reopen log files             | Upgrade binary on-the-fly   |
| dd          | Print I/O statistics         | —                           |
| Apache      | Graceful restart             | Graceful stop               |
| Postfix     | —                            | —                           |
| Custom      | Toggle debug mode            | Dump internal state         |

### Ejemplo: toggle de debug

```c
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

volatile sig_atomic_t debug_mode = 0;

void sigusr1_handler(int sig)
{
    (void)sig;
    debug_mode = !debug_mode;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sigusr1_handler,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    printf("PID %d, send SIGUSR1 to toggle debug\n", getpid());

    for (int i = 0; ; i++) {
        if (debug_mode)
            printf("[DEBUG] iteration %d, state=0x%x\n", i, i * 17);
        else
            printf("working... (%d)\n", i);
        sleep(2);
    }
}
```

```bash
$ ./toggle_debug &
PID 5678, send SIGUSR1 to toggle debug
working... (0)
working... (1)

$ kill -USR1 5678
[DEBUG] iteration 2, state=0x22
[DEBUG] iteration 3, state=0x33

$ kill -USR1 5678
working... (4)
working... (5)
```

### Ejemplo: dump de estado interno

```c
volatile sig_atomic_t dump_requested = 0;

void sigusr2_handler(int sig)
{
    (void)sig;
    dump_requested = 1;
}

/* In the main loop: */
if (dump_requested) {
    dump_requested = 0;

    FILE *f = fopen("/tmp/myapp_state.txt", "w");
    if (f) {
        fprintf(f, "connections: %d\n", active_connections);
        fprintf(f, "requests: %lu\n", total_requests);
        fprintf(f, "uptime: %ld seconds\n", time(NULL) - start_time);
        fprintf(f, "memory: %zu bytes\n", allocated_memory);
        fclose(f);
    }
}
```

---

## 8. SIGSEGV, SIGBUS, SIGFPE: errores fatales

Estas señales indican **bugs** en el programa. Capturarlas para
"continuar" es casi siempre un error. Solo se capturan para logging
o para intentar un crash dump:

### SIGSEGV (11) — Segmentation fault

Acceso a memoria no mapeada o sin permisos:

```c
int *p = NULL;
*p = 42;          /* SIGSEGV: null pointer dereference */

int *q = (int *)0xDEADBEEF;
*q = 42;          /* SIGSEGV: invalid address */

char *s = "hello";
s[0] = 'H';       /* SIGSEGV: write to read-only memory */
```

### SIGBUS (7) — Bus error

Error de alineamiento o acceso a memoria mapeada más allá del archivo:

```c
/* Unaligned access (some architectures) */
char buf[8];
int *p = (int *)(buf + 1);  /* Misaligned */
*p = 42;                    /* SIGBUS on strict-alignment CPUs */

/* mmap beyond file size */
int fd = open("small.txt", O_RDONLY);  /* File is 10 bytes */
char *m = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
char c = m[4000];  /* SIGBUS: beyond end of file */
```

### SIGFPE (8) — Floating point exception

Nombre engañoso — también cubre errores de enteros:

```c
int a = 42, b = 0;
int c = a / b;      /* SIGFPE: integer division by zero */

int d = INT_MIN;
int e = d / -1;     /* SIGFPE: integer overflow (on some archs) */
```

### Capturar para diagnóstico

```c
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

void crash_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)ctx;

    /* Only use async-signal-safe functions */
    const char *name = "unknown";
    if (sig == SIGSEGV) name = "SIGSEGV";
    else if (sig == SIGBUS) name = "SIGBUS";
    else if (sig == SIGFPE) name = "SIGFPE";

    /* Write crash info (write is async-signal-safe) */
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                       "CRASH: %s at address %p\n",
                       name, info->si_addr);
    write(STDERR_FILENO, buf, len);

    /* Backtrace (not async-signal-safe, but we're dying anyway) */
    void *bt[32];
    int depth = backtrace(bt, 32);
    backtrace_symbols_fd(bt, depth, STDERR_FILENO);

    /* Re-raise with default handler to get core dump */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigaction(sig, &sa, NULL);
    raise(sig);
}

void install_crash_handlers(void)
{
    struct sigaction sa = {
        .sa_sigaction = crash_handler,
        .sa_flags     = SA_SIGINFO | SA_RESETHAND,  /* One-shot */
    };
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}
```

> **SA_RESETHAND**: Resetea a SIG_DFL después de la primera ejecución.
> Así, si el handler mismo causa SIGSEGV, no entra en loop infinito.

---

## 9. SIGTSTP, SIGCONT: control de jobs

### SIGTSTP (20) — Ctrl+Z

Detiene el proceso foreground. A diferencia de SIGSTOP, es **capturable**:

```c
void sigtstp_handler(int sig)
{
    (void)sig;

    /* Restore terminal state before suspending */
    restore_terminal();

    /* Re-raise with default action to actually stop */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigaction(SIGTSTP, &sa, NULL);
    raise(SIGTSTP);  /* This will stop us */

    /* Execution resumes HERE when we get SIGCONT */

    /* Re-install our handler */
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    /* Restore our terminal mode */
    setup_terminal();
}
```

### SIGCONT (18) — Reanudar

Se envía cuando un proceso detenido es reanudado con `fg` o `kill -CONT`.
No se puede bloquear ni ignorar de forma permanente (el proceso siempre
se reanuda), pero se puede capturar para ejecutar código al reanudar:

```c
void sigcont_handler(int sig)
{
    (void)sig;
    /* Redraw screen, re-read terminal size, etc. */
}

struct sigaction sa = {
    .sa_handler = sigcont_handler,
    .sa_flags   = SA_RESTART,
};
sigemptyset(&sa.sa_mask);
sigaction(SIGCONT, &sa, NULL);
```

### Flujo completo de job control

```
  Terminal foreground          Terminal background
  ──────────────────          ──────────────────
  $ ./editor
  (editing...)

  Ctrl+Z ──▶ SIGTSTP
  [1]+ Stopped ./editor

  $ bg                        ./editor (stopped → background)

  $ fg
  SIGCONT ──▶ (resumes)
  (editing again...)
```

### SIGTTIN y SIGTTOU

Un proceso en background que intenta leer de la terminal recibe `SIGTTIN`
(se detiene). Si intenta escribir, recibe `SIGTTOU` (si `stty tostop`
está activo). Esto evita que procesos background interfieran con la
terminal:

```bash
$ cat &              # Background job tries to read terminal
[1]+ Stopped (SIGTTIN)  cat

$ stty tostop
$ echo hello &       # Background job tries to write terminal
[2]+ Stopped (SIGTTOU)  echo hello
```

---

## 10. Errores comunes

### Error 1: No ignorar SIGPIPE en servidores

```c
/* MAL: un cliente que desconecta mata todo el servidor */
int main(void)
{
    int server_fd = setup_server(8080);
    while (1) {
        int client = accept(server_fd, NULL, NULL);
        /* Client disconnects → write() → SIGPIPE → server dies */
        write(client, response, strlen(response));
    }
}

/* BIEN: ignorar SIGPIPE al inicio */
int main(void)
{
    signal(SIGPIPE, SIG_IGN);  /* Handle EPIPE in write() instead */

    int server_fd = setup_server(8080);
    while (1) {
        int client = accept(server_fd, NULL, NULL);
        ssize_t n = write(client, response, strlen(response));
        if (n == -1 && errno == EPIPE) {
            close(client);  /* Client gone, clean up */
            continue;
        }
    }
}
```

### Error 2: Usar SIGKILL como primera opción

```bash
# MAL: SIGKILL no permite cleanup
kill -9 $(pidof myserver)
# → Connections dropped, data not flushed, lock files remain

# BIEN: SIGTERM primero, SIGKILL solo si no responde
kill $(pidof myserver)        # SIGTERM
sleep 5
kill -0 $(pidof myserver) 2>/dev/null && kill -9 $(pidof myserver)
```

### Error 3: Intentar recuperarse de SIGSEGV

```c
/* MAL: continuar después de SIGSEGV es undefined behavior */
void handler(int sig)
{
    (void)sig;
    printf("segfault caught, continuing...\n");
    /* Returns to the instruction that caused SIGSEGV
       → SIGSEGV again → infinite loop or corruption */
}

/* BIEN: log y morir limpiamente */
void handler(int sig, siginfo_t *info, void *ctx)
{
    (void)ctx;
    /* Log the crash */
    /* Re-raise to get core dump */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigaction(sig, &sa, NULL);
    raise(sig);
}
```

Después de SIGSEGV, el estado del programa es **corrupto**. Cualquier
intento de continuar es undefined behavior. Solo se captura para
loguear el crash antes de morir.

### Error 4: Mezclar alarm() con sleep()

```c
/* MAL: alarm y sleep interfieren (ambos usan SIGALRM internamente) */
alarm(10);         /* Set 10-second alarm */
sleep(5);          /* Some implementations cancel the alarm */
/* alarm may or may not fire */

/* BIEN: usar nanosleep (no usa SIGALRM) */
alarm(10);
struct timespec ts = { .tv_sec = 5 };
nanosleep(&ts, NULL);
/* alarm fires independently after 10 seconds */
```

### Error 5: Asumir que SIGUSR1/2 están libres

```c
/* MAL: asumir que nadie más usa SIGUSR1 */
sigaction(SIGUSR1, &sa, NULL);
/* Problem: a library or framework you linked might also use SIGUSR1
   (e.g., some garbage collectors, profilers, or sanitizers) */

/* BIEN: verificar si ya hay handler instalado */
struct sigaction old;
sigaction(SIGUSR1, NULL, &old);
if (old.sa_handler != SIG_DFL) {
    fprintf(stderr, "warning: SIGUSR1 already has a handler\n");
}
sigaction(SIGUSR1, &sa, NULL);
```

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                  Señales Comunes                              │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Control desde terminal:                                     │
  │    Ctrl+C  → SIGINT (2)     interrumpir                     │
  │    Ctrl+\  → SIGQUIT (3)    quit + core dump                │
  │    Ctrl+Z  → SIGTSTP (20)   suspender (capturable)          │
  │                                                              │
  │  Terminación:                                                │
  │    SIGTERM (15)  terminar limpiamente (kill default)         │
  │    SIGKILL (9)   matar incondicionalmente (no capturable)   │
  │    SIGHUP (1)    hangup / recargar config (por convención)  │
  │                                                              │
  │  Hijos:                                                      │
  │    SIGCHLD (17)  hijo terminó/paró/reanudó                  │
  │      → while(waitpid(-1,NULL,WNOHANG)>0)  (no se encolan)  │
  │      → SA_NOCLDSTOP: solo muerte, no stop                   │
  │      → SIG_IGN: auto-reap (no zombies)                      │
  │                                                              │
  │  I/O:                                                        │
  │    SIGPIPE (13)  pipe/socket roto → SIG_IGN en servidores   │
  │      → EPIPE en write() como alternativa                    │
  │      → MSG_NOSIGNAL per-send                                │
  │                                                              │
  │  Timers:                                                     │
  │    SIGALRM (14)  alarm(secs), un timer por proceso          │
  │      → NO SA_RESTART (queremos interrumpir la syscall)      │
  │      → No mezclar con sleep()                                │
  │      → setitimer para microsegundos                          │
  │                                                              │
  │  Usuario:                                                    │
  │    SIGUSR1 (10)  libre, definida por app                    │
  │    SIGUSR2 (12)  libre, definida por app                    │
  │      → debug toggle, log reopen, state dump...              │
  │                                                              │
  │  Errores fatales (bugs):                                     │
  │    SIGSEGV (11)  memoria inválida                           │
  │    SIGBUS (7)    alineamiento / mmap beyond EOF             │
  │    SIGFPE (8)    div by zero, int overflow                  │
  │      → Solo capturar para log + re-raise con SIG_DFL        │
  │      → SA_RESETHAND para evitar loop                        │
  │                                                              │
  │  Job control:                                                │
  │    SIGTSTP (20)  Ctrl+Z (capturable)                        │
  │    SIGSTOP (19)  detener (no capturable)                    │
  │    SIGCONT (18)  reanudar                                   │
  │    SIGTTIN (21)  bg read from terminal                      │
  │    SIGTTOU (22)  bg write to terminal                       │
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Daemon con SIGHUP y SIGTERM

Escribe un programa que simule un daemon con estas señales:

- **SIGTERM**: graceful shutdown (poner un flag, salir del loop principal,
  ejecutar cleanup).
- **SIGHUP**: recargar configuración desde un archivo.
- **SIGUSR1**: imprimir estadísticas internas a un archivo de log.
- **SIGPIPE**: ignorar (el daemon escribe a sockets).

```c
typedef struct {
    int   port;
    int   max_connections;
    char  log_file[256];
} config_t;

typedef struct {
    unsigned long requests;
    unsigned long errors;
    time_t        start_time;
    int           active_connections;
} stats_t;
```

Requisitos:
1. Cargar config desde `config.ini` al inicio y en cada SIGHUP.
2. Cada SIGUSR1, escribir las stats a `/tmp/daemon_stats.txt`.
3. SIGTERM pone flag, el loop principal sale, hace cleanup.
4. Usar `sigaction` con `SA_RESTART` para todo excepto donde se necesite
   interrumpir.
5. El loop principal simplemente hace `sleep(1)` e incrementa un
   contador de requests (simulación).

**Predicción antes de codificar**: Si SIGHUP llega mientras estás en
medio de `load_config()` (lectura de archivo), ¿qué puede pasar? ¿Cómo
lo prevendrías?

**Pregunta de reflexión**: ¿Por qué nginx usa SIGUSR1 para reabrir
logs en lugar de SIGHUP? ¿Qué pasaría si usara SIGHUP tanto para
recargar config como para reabrir logs?

---

### Ejercicio 2: Timeout con alarm para operaciones de red

Implementa un cliente HTTP mínimo que descargue una URL con timeout:

```c
int download_with_timeout(const char *host, int port,
                          const char *path, int timeout_secs,
                          char *buf, size_t bufsize);
```

Requisitos:
1. Crear socket TCP y conectar al host:port.
2. Usar `alarm(timeout_secs)` antes de `connect`.
3. Enviar request HTTP GET.
4. Usar `alarm()` de nuevo antes de `read` para timeout de respuesta.
5. Si se recibe SIGALRM, `connect`/`read` retornan con `EINTR`.
6. Reportar si el timeout fue en connect o en read.
7. Cancelar la alarma (`alarm(0)`) al terminar.
8. Restaurar el handler anterior de SIGALRM.

**Predicción antes de codificar**: ¿Qué pasa si `connect()` tarda 3
segundos y el timeout es 5, pero luego `read()` tarda 4 segundos? ¿Se
detecta el timeout total o solo el de read?

**Pregunta de reflexión**: `alarm()` tiene resolución de 1 segundo y un
solo timer por proceso. ¿Qué mecanismos modernos usarías para timeouts
con resolución de milisegundos en un servidor con miles de conexiones?

---

### Ejercicio 3: Signal logger

Escribe un programa que capture **todas** las señales capturables y
registre cuándo llega cada una, construyendo un log de actividad:

```c
void signal_logger(const char *logfile, int duration_secs);
```

Requisitos:
1. Instalar un handler con `SA_SIGINFO` para señales 1-31 (excepto
   SIGKILL y SIGSTOP).
2. El handler guarda el evento en un ring buffer global
   (`volatile sig_atomic_t` para el índice).
3. Registrar: número de señal, timestamp (no usar `time()` en handler;
   usar un contador atómico), PID remitente (`info->si_pid`).
4. El loop principal (`sleep` o `pause`) revisa el ring buffer cada
   segundo y escribe al archivo de log.
5. Después de `duration_secs`, restaurar todos los handlers y escribir
   un resumen.

Probar con:
```bash
$ ./signal_logger /tmp/siglog.txt 30 &
[1] 5678

$ kill -USR1 5678
$ kill -USR2 5678
$ kill -HUP 5678
$ kill -TERM 5678

$ cat /tmp/siglog.txt
[0.0s] Signal 10 (SIGUSR1) from PID 1000
[0.3s] Signal 12 (SIGUSR2) from PID 1000
[1.1s] Signal  1 (SIGHUP)  from PID 1000
[2.0s] Signal 15 (SIGTERM) from PID 1000 — shutting down
Summary: USR1=1, USR2=1, HUP=1, TERM=1
```

**Predicción antes de codificar**: ¿Qué tamaño debe tener el ring buffer
si el sistema puede enviar ráfagas de señales? ¿Qué pasa si el buffer
se llena antes de que el loop principal lo consuma?

**Pregunta de reflexión**: ¿Por qué no se puede usar `fprintf` ni
`time()` dentro del signal handler? ¿Qué funciones usarías para obtener
un timestamp de forma async-signal-safe?
