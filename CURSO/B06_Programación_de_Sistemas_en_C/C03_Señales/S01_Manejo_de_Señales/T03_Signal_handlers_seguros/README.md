# Signal Handlers Seguros — Funciones Async-Signal-Safe

## Índice

1. [El problema de la reentrancia](#1-el-problema-de-la-reentrancia)
2. [Qué es async-signal-safe](#2-qué-es-async-signal-safe)
3. [Lista de funciones async-signal-safe](#3-lista-de-funciones-async-signal-safe)
4. [Funciones que NO son seguras](#4-funciones-que-no-son-seguras)
5. [volatile sig_atomic_t](#5-volatile-sig_atomic_t)
6. [Patrón: handler mínimo con flag](#6-patrón-handler-mínimo-con-flag)
7. [Escribir mensajes desde el handler](#7-escribir-mensajes-desde-el-handler)
8. [Self-pipe trick (introducción)](#8-self-pipe-trick-introducción)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema de la reentrancia

Un signal handler se ejecuta **asíncronamente**: puede interrumpir
cualquier instrucción del programa principal en cualquier momento.
Esto crea un problema grave si el handler y el programa principal
usan las mismas estructuras de datos:

```
  Programa principal                Handler
  ──────────────────                ───────
  printf("hello ");
     │
     ├─ printf adquiere un lock
     │   interno de stdio
     │
     │   ← SIGINT llega AQUÍ
     │                              printf("caught!\n");
     │                                 │
     │                                 ├─ printf intenta
     │                                 │   adquirir el MISMO
     │                                 │   lock de stdio
     │                                 │
     │                                 └─ DEADLOCK
     │                                    (el lock lo tiene
     │                                     el programa principal,
     │                                     que está suspendido)
```

El handler interrumpió `printf` del programa principal **mientras
tenía un lock**. Ahora el handler intenta usar `printf`, que necesita
el mismo lock. Deadlock: el handler no puede continuar, y el programa
principal no puede reanudar porque el handler no ha terminado.

### No solo deadlocks

El problema no es solo de locks. Cualquier estructura de datos que no
sea atómica puede quedar en un **estado inconsistente** si se interrumpe
a mitad de modificación:

```c
/* Global linked list */
struct node *head = NULL;

void add_node(struct node *n) {
    n->next = head;
    /* ← SIGNAL HERE: head still points to old node,
         but n->next also points to old node.
         If handler traverses the list, sees partial state */
    head = n;
}
```

```
  Memoria heap (malloc internals):

  ┌─────────┐    ┌─────────┐    ┌─────────┐
  │ chunk A  │───▶│ chunk B  │───▶│ chunk C  │
  │ (in use) │    │ (free)   │    │ (in use) │
  └─────────┘    └─────────┘    └─────────┘

  Si malloc() está reorganizando la free list cuando
  llega una señal, y el handler llama a malloc():
  → corrupción del heap
```

---

## 2. Qué es async-signal-safe

Una función es **async-signal-safe** si se puede llamar de forma segura
desde un signal handler, incluso si el programa principal estaba ejecutando
esa misma función (u otra función no segura) cuando llegó la señal.

POSIX define explícitamente qué funciones son async-signal-safe. Todas
las demás **no deben usarse** en signal handlers.

### Requisitos para ser async-signal-safe

1. No usa variables globales compartidas sin protección atómica.
2. No adquiere locks que el programa principal podría tener.
3. No llama a funciones que no son async-signal-safe.
4. Es **reentrant**: puede ser interrumpida por sí misma y seguir
   funcionando correctamente.

### La regla de oro

> **En un signal handler, solo usar funciones de la lista
> async-signal-safe de POSIX, o funciones puramente computacionales
> que no tocan estado global.**

---

## 3. Lista de funciones async-signal-safe

POSIX.1-2017 define la lista completa. Las más usadas en handlers:

### I/O de bajo nivel (syscalls)

```c
/* Estas SÍ son seguras en handlers: */
write(fd, buf, len);   /* La más usada para output en handlers */
read(fd, buf, len);
open(path, flags);
close(fd);
dup(fd);
dup2(oldfd, newfd);
fcntl(fd, cmd, ...);
lseek(fd, offset, whence);
```

### Señales

```c
signal(sig, handler);
sigaction(sig, act, oldact);
sigprocmask(how, set, oldset);
sigpending(set);
sigsuspend(mask);
kill(pid, sig);
raise(sig);
```

### Proceso

```c
_exit(status);     /* YES — safe */
/* exit(status);   NO — NOT safe (calls atexit handlers, flushes stdio) */

fork();
execve(path, argv, envp);
/* execvp, execlp... are NOT explicitly listed but usually safe */

getpid();
getppid();
getuid();
getgid();
```

### Espera

```c
wait(status);
waitpid(pid, status, options);
```

### Tiempo

```c
time(NULL);        /* Safe */
alarm(seconds);
```

### Atomics y flags

```c
/* Not functions, but safe to use: */
volatile sig_atomic_t flag;  /* Read/write is atomic */
```

### Otras funciones seguras

```c
access(path, mode);
chdir(path);
chmod(path, mode);
chown(path, uid, gid);
link(old, new);
unlink(path);
rename(old, new);
mkdir(path, mode);
rmdir(path);
stat(path, buf);
umask(mask);
```

---

## 4. Funciones que NO son seguras

Estas funciones **nunca** deben usarse en un signal handler:

### stdio (printf, fprintf, fgets, etc.)

```c
void bad_handler(int sig)
{
    printf("signal %d\n", sig);   /* DEADLOCK: stdio uses internal locks */
    fprintf(stderr, "error\n");   /* Same problem */
    fflush(stdout);               /* Same problem */
    puts("hello");                /* Same problem */
}
```

**Por qué**: stdio mantiene buffers internos protegidos por locks. Si el
programa principal estaba en medio de un `printf` cuando llegó la señal,
el lock está tomado. El handler intenta tomar el mismo lock → deadlock.

### Memoria dinámica (malloc, free, calloc, realloc)

```c
void bad_handler(int sig)
{
    char *buf = malloc(256);     /* CORRUPTION: heap may be inconsistent */
    free(some_global_ptr);       /* Same problem */
    char *dup = strdup("hello"); /* Uses malloc internally */
}
```

**Por qué**: malloc mantiene una free list con punteros. Si se interrumpe
a mitad de una reorganización, la lista queda corrupta. Llamar a malloc
en el handler corrompe el heap permanentemente.

### Funciones que usan buffers estáticos

```c
void bad_handler(int sig)
{
    char *s = strerror(errno);    /* Static buffer — not reentrant */
    struct tm *t = localtime(&now); /* Static buffer */
    char *host = gethostbyname("x"); /* Static buffer + network */
}
```

### Otras funciones no seguras

| Función / Familia | Razón                                      |
|-------------------|--------------------------------------------|
| `printf` / stdio  | Locks internos                             |
| `malloc` / `free` | Heap metadata corruption                   |
| `exit()`          | Llama `atexit` handlers + flushea stdio    |
| `syslog()`        | Usa stdio y malloc internamente            |
| `strerror()`      | Buffer estático interno                    |
| `localtime()`     | Buffer estático interno                    |
| `getenv()`        | No reentrant (modifica estado global)      |
| `setenv()`        | Modifica environ (no thread-safe)          |
| `dlopen()`        | Locks del dynamic linker                   |
| `pthread_*`       | La mayoría no son async-signal-safe        |
| `openlog()`       | Estado global de syslog                    |

### Versiones reentrant (_r)

Algunas funciones tienen versiones reentrant, pero eso **no** las hace
async-signal-safe:

```c
/* strerror_r is reentrant BUT still not async-signal-safe */
char buf[256];
strerror_r(errno, buf, sizeof(buf));  /* NOT safe in handler */

/* localtime_r is reentrant BUT still not async-signal-safe */
struct tm result;
localtime_r(&now, &result);  /* NOT safe in handler */
```

Reentrant ≠ async-signal-safe. Una función reentrant es segura para
llamar desde múltiples threads simultáneamente, pero puede no ser
segura cuando un signal handler interrumpe la misma función en el
mismo thread.

---

## 5. volatile sig_atomic_t

`sig_atomic_t` es un tipo entero garantizado como atómico para lectura y
escritura en presencia de señales. `volatile` impide que el compilador
optimice los accesos:

```c
#include <signal.h>

volatile sig_atomic_t flag = 0;  /* THE standard way */
```

### Por qué volatile

Sin `volatile`, el compilador puede optimizar el loop:

```c
int flag = 0;  /* Sin volatile */

void handler(int sig) { flag = 1; }

int main(void)
{
    /* Compiler may optimize to: if (!flag) while(1); */
    while (!flag) {
        /* compiler sees flag never changes in this loop */
        /* may load flag ONCE and loop forever */
    }
}
```

Con `volatile`, el compilador **siempre** lee de memoria:

```c
volatile sig_atomic_t flag = 0;

void handler(int sig) { flag = 1; }

int main(void)
{
    while (!flag) {
        /* Compiler reads flag from memory every iteration */
        pause();
    }
}
```

### Qué puedes hacer con sig_atomic_t

```c
volatile sig_atomic_t counter = 0;

/* SAFE: */
flag = 1;           /* Write */
flag = 0;           /* Write */
if (flag) {...}     /* Read */
x = flag;           /* Read */

/* NOT SAFE (not guaranteed atomic): */
counter++;          /* Read-modify-write — NOT atomic */
counter += 5;       /* Read-modify-write — NOT atomic */
flag = flag + 1;    /* Read-modify-write — NOT atomic */
```

> **Solo asignación y lectura** están garantizadas como atómicas.
> Las operaciones compuestas (incremento, suma) **no** son atómicas
> incluso con `sig_atomic_t`.

### Rango de sig_atomic_t

POSIX garantiza que `sig_atomic_t` puede contener al menos los valores
de `SIG_ATOMIC_MIN` a `SIG_ATOMIC_MAX` (típicamente el rango de `int`
en la práctica). Es suficiente para flags y números de señal.

---

## 6. Patrón: handler mínimo con flag

El patrón más seguro y más común: el handler solo pone un flag, y
el programa principal lo revisa:

```c
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

volatile sig_atomic_t got_sigterm = 0;
volatile sig_atomic_t got_sighup = 0;

void sigterm_handler(int sig) { (void)sig; got_sigterm = 1; }
void sighup_handler(int sig)  { (void)sig; got_sighup = 1;  }

int main(void)
{
    struct sigaction sa = { .sa_flags = SA_RESTART };
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);

    printf("server starting (PID %d)\n", getpid());
    load_config();

    while (!got_sigterm) {
        if (got_sighup) {
            got_sighup = 0;
            printf("reloading config...\n");
            load_config();
        }

        /* Main work */
        handle_next_request();
    }

    printf("shutting down...\n");
    cleanup();
    return 0;
}
```

### Variante: guardar el número de señal

```c
volatile sig_atomic_t last_signal = 0;

void generic_handler(int sig)
{
    last_signal = sig;
}

/* Main loop checks: */
while (1) {
    int sig = last_signal;
    last_signal = 0;

    switch (sig) {
    case SIGTERM: goto shutdown;
    case SIGHUP:  reload_config(); break;
    case SIGUSR1: toggle_debug();  break;
    case 0:       break;  /* No signal */
    }

    do_work();
}
```

### Variante: bitfield de señales pendientes

```c
volatile sig_atomic_t pending_signals = 0;

void handler(int sig)
{
    pending_signals |= (1 << sig);
    /* NOTE: |= is NOT atomic on sig_atomic_t!
       This is acceptable here because only the handler writes,
       and the same signal is blocked during its handler.
       BUT multiple different signals could race. For strict
       correctness, use separate flags per signal. */
}

/* Main loop: */
if (pending_signals) {
    int sigs = pending_signals;
    pending_signals = 0;  /* Clear (small race window, acceptable) */

    if (sigs & (1 << SIGTERM)) do_shutdown();
    if (sigs & (1 << SIGHUP))  do_reload();
    if (sigs & (1 << SIGUSR1)) do_debug();
}
```

---

## 7. Escribir mensajes desde el handler

`write()` es async-signal-safe. Es la forma correcta de emitir output
desde un handler:

### write con string literal

```c
void handler(int sig)
{
    const char msg[] = "caught signal, exiting\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
}
```

### Escribir números sin printf

`printf` no es seguro, pero puedes convertir enteros manualmente:

```c
/* Convert integer to string without printf (async-signal-safe) */
void write_int(int fd, int n)
{
    char buf[16];
    int i = sizeof(buf) - 1;
    int neg = 0;

    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) { buf[i--] = '0'; }

    while (n > 0) {
        buf[i--] = '0' + (n % 10);
        n /= 10;
    }

    if (neg) buf[i--] = '-';
    i++;

    write(fd, buf + i, sizeof(buf) - i);
}

void handler(int sig)
{
    const char msg[] = "caught signal ";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    write_int(STDERR_FILENO, sig);
    write(STDERR_FILENO, "\n", 1);
    _exit(1);
}
```

### Tabla de strings por señal

```c
/* Pre-built messages — no formatting needed in handler */
static const char *sig_messages[] = {
    [SIGHUP]  = "SIGHUP received\n",
    [SIGINT]  = "SIGINT received\n",
    [SIGTERM] = "SIGTERM received\n",
    [SIGUSR1] = "SIGUSR1 received\n",
    [SIGUSR2] = "SIGUSR2 received\n",
};

void handler(int sig)
{
    if (sig > 0 && sig < (int)(sizeof(sig_messages)/sizeof(sig_messages[0]))
        && sig_messages[sig]) {
        const char *msg = sig_messages[sig];
        write(STDERR_FILENO, msg, strlen(msg));
        /* strlen is NOT explicitly async-signal-safe, but it's
           a pure computation on a constant — safe in practice */
    }
    _exit(128 + sig);
}
```

---

## 8. Self-pipe trick (introducción)

El patrón handler+flag tiene un problema: ¿cómo despertar el programa
principal si está bloqueado en `poll()` o `select()`?

```c
/* Problem: poll blocks, signal arrives, handler sets flag,
   but poll doesn't return (SA_RESTART restarts it) */
while (!got_signal) {
    poll(fds, nfds, -1);  /* Blocks forever, ignoring the flag */
}
```

El **self-pipe trick** resuelve esto: el handler escribe un byte a un
pipe, y poll/select vigila ese pipe:

```c
#include <poll.h>
#include <signal.h>
#include <unistd.h>

static int signal_pipe[2];  /* [0]=read, [1]=write */

void handler(int sig)
{
    int saved_errno = errno;
    char c = sig;  /* Store signal number as a byte */
    write(signal_pipe[1], &c, 1);  /* write is async-signal-safe */
    errno = saved_errno;
}

int main(void)
{
    pipe(signal_pipe);

    /* Make write end non-blocking (prevent handler from blocking) */
    fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);

    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    struct pollfd fds[2];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    fds[1].fd = signal_pipe[0];   /* Watch the signal pipe */
    fds[1].events = POLLIN;

    while (1) {
        int ret = poll(fds, 2, -1);

        if (fds[1].revents & POLLIN) {
            /* Signal arrived — read and handle */
            char c;
            while (read(signal_pipe[0], &c, 1) > 0) {
                switch (c) {
                case SIGTERM: goto shutdown;
                case SIGHUP:  reload_config(); break;
                }
            }
        }

        if (fds[0].revents & POLLIN) {
            /* Normal I/O event */
            handle_client();
        }
    }

shutdown:
    cleanup();
    return 0;
}
```

```
  ┌──────────────────────────────────────────────────┐
  │                                                  │
  │  Signal arrives                                  │
  │      │                                           │
  │      ▼                                           │
  │  handler(sig)                                    │
  │      │                                           │
  │      write(signal_pipe[1], &sig, 1)              │
  │      │                                           │
  │      ▼                                           │
  │  poll() sees POLLIN on signal_pipe[0]            │
  │      │                                           │
  │      ▼                                           │
  │  Main loop reads pipe, handles signal            │
  │                                                  │
  └──────────────────────────────────────────────────┘
```

> **signalfd** (Linux) reemplaza el self-pipe trick con un fd dedicado
> para señales. Se cubre en S02 (Señales Avanzadas).

---

## 9. Errores comunes

### Error 1: printf en signal handler

```c
/* MAL: printf no es async-signal-safe */
void handler(int sig)
{
    printf("caught signal %d\n", sig);  /* DEADLOCK or corruption */
}

/* BIEN: usar write */
void handler(int sig)
{
    const char msg[] = "caught signal\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
}
```

Este es el error más frecuente. Funciona la mayoría de las veces en
pruebas simples, pero deadlockea bajo carga cuando `printf` se
interrumpe por la señal.

### Error 2: malloc/free en handler

```c
/* MAL: corrupción del heap */
void handler(int sig)
{
    char *msg = malloc(256);
    snprintf(msg, 256, "signal %d", sig);  /* snprintf: no safe either */
    log_message(msg);  /* Probably uses stdio or malloc internally */
    free(msg);
}

/* BIEN: usar un buffer estático preasignado */
void handler(int sig)
{
    /* Pre-allocated at startup — no dynamic allocation needed */
    static char buf[64];  /* Static, always available */
    /* Only safe if handler cannot be interrupted by itself
       (guaranteed because same signal is blocked during handler) */

    const char *name = (sig == SIGTERM) ? "TERM" : "UNKNOWN";
    /* ... write with write() ... */
}
```

### Error 3: exit() en lugar de _exit()

```c
/* MAL: exit() flushea stdio y ejecuta atexit handlers */
void handler(int sig)
{
    exit(1);
    /* exit() calls:
       1. atexit() handlers → may use malloc, stdio
       2. fflush() on all streams → stdio locks
       3. Other cleanup → unknown safety */
}

/* BIEN: _exit() termina inmediatamente */
void handler(int sig)
{
    (void)sig;
    const char msg[] = "fatal signal, exiting\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(128 + sig);  /* _exit: no atexit, no stdio flush */
}
```

### Error 4: Usar int en lugar de volatile sig_atomic_t

```c
/* MAL: el compilador puede optimizar la lectura */
int flag = 0;

void handler(int sig) { flag = 1; }

int main(void)
{
    while (!flag) {  /* Compiler may cache flag in register */
        pause();     /* Loop may never exit even after signal */
    }
}

/* BIEN: volatile sig_atomic_t */
volatile sig_atomic_t flag = 0;

void handler(int sig) { flag = 1; }

int main(void)
{
    while (!flag) {  /* Always reads from memory */
        pause();
    }
}
```

### Error 5: No guardar/restaurar errno

```c
/* MAL: handler modifica errno, corrompe el check del programa principal */
void handler(int sig)
{
    (void)sig;
    /* waitpid sets errno on failure */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    /* errno now might be ECHILD, overwriting the main program's errno */
}

/* BIEN: guardar y restaurar */
void handler(int sig)
{
    (void)sig;
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}
```

Si el programa principal estaba a punto de verificar `errno` después
de una syscall, el handler lo sobrescribe. Esto causa bugs
intermitentes extremadamente difíciles de diagnosticar.

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │             Signal Handlers Seguros                          │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  REGLA: en un handler, SOLO funciones async-signal-safe      │
  │                                                              │
  │  Seguras (más usadas):                                       │
  │    write, read, open, close, dup2                            │
  │    _exit (NO exit)                                           │
  │    kill, raise, signal, sigaction, sigprocmask               │
  │    fork, execve, waitpid                                     │
  │    getpid, alarm, time                                       │
  │    access, stat, chdir, unlink, rename                       │
  │                                                              │
  │  NO seguras (NUNCA en handler):                              │
  │    printf, fprintf, puts, fflush (stdio)                     │
  │    malloc, free, calloc, realloc (heap)                      │
  │    exit (atexit + stdio flush)                               │
  │    syslog, strerror, localtime, getenv                       │
  │    pthread_mutex_lock (la mayoría de pthread)                │
  │                                                              │
  │  volatile sig_atomic_t:                                      │
  │    → tipo para flags entre handler y programa principal      │
  │    → solo lectura/escritura son atómicas (NO incremento)     │
  │    → volatile impide que el compilador cache el valor        │
  │                                                              │
  │  Patrón estándar:                                            │
  │    volatile sig_atomic_t flag = 0;                           │
  │    void handler(int sig) { flag = 1; }                       │
  │    while (!flag) { pause(); }                                │
  │                                                              │
  │  Output en handler:                                          │
  │    write(STDERR_FILENO, msg, len);  (NO printf)              │
  │    Convertir int a string manualmente si necesario           │
  │                                                              │
  │  Self-pipe trick:                                            │
  │    handler → write(pipe[1], &sig, 1)                         │
  │    main loop → poll() incluye pipe[0]                        │
  │    → despierta poll sin SA_RESTART issues                    │
  │                                                              │
  │  errno en handler:                                           │
  │    int saved = errno;                                        │
  │    /* ... handler work ... */                                │
  │    errno = saved;                                            │
  │                                                              │
  │  reentrant ≠ async-signal-safe:                              │
  │    strerror_r es reentrant pero NO signal-safe               │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Safe signal reporter

Escribe un programa que capture SIGINT, SIGTERM, SIGHUP y SIGUSR1,
y para cada señal recibida imprima información **sin usar printf**:

```
  $ ./safe_reporter &
  PID 5678 listening for signals

  $ kill -USR1 5678
  [signal 10] SIGUSR1

  $ kill -HUP 5678
  [signal 1] SIGHUP

  $ kill -INT 5678
  [signal 2] SIGINT — shutting down
```

Requisitos:
1. Toda la salida del handler debe usar exclusivamente `write()`.
2. Implementar `write_int()` para convertir el número de señal.
3. Implementar `write_str()` wrapper que calcule la longitud.
4. Usar una tabla precalculada de nombres de señal (array de `const char *`).
5. SIGINT y SIGTERM causan shutdown limpio con `_exit()`.
6. SIGHUP y SIGUSR1 solo imprimen y continúan.
7. Guardar y restaurar `errno` en el handler.

**Predicción antes de codificar**: Si dos señales llegan casi
simultáneamente (SIGUSR1 y SIGHUP), ¿pueden los dos handlers ejecutarse
al mismo tiempo? ¿Qué papel juega `sa_mask` en esto?

**Pregunta de reflexión**: ¿Por qué `write()` es async-signal-safe pero
`printf()` no, si ambas terminan escribiendo al mismo file descriptor?

---

### Ejercicio 2: Watchdog con self-pipe

Implementa un watchdog que monitorice un proceso hijo y reaccione a
señales usando el self-pipe trick:

```c
int watchdog(const char *cmd, char *const argv[],
             int max_restarts, int cooldown_secs);
```

Comportamiento:
1. Crear el pipe para señales (self-pipe, write end non-blocking).
2. Instalar handlers para SIGCHLD, SIGTERM, SIGHUP.
3. Fork+exec el proceso hijo.
4. Entrar en un loop con `poll()` que vigila:
   - El self-pipe (señales).
   - Opcionalmente un timerfd o alarma para heartbeats.
5. En SIGCHLD: reap el hijo, decidir si reiniciar.
6. En SIGTERM: enviar SIGTERM al hijo, esperar, y salir.
7. En SIGHUP: enviar SIGHUP al hijo (recargar).
8. Si el hijo muere más de `max_restarts` veces, reportar y salir.

**Predicción antes de codificar**: ¿Por qué el write end del pipe debe
ser non-blocking? ¿Qué pasa si el handler intenta escribir y el pipe
está lleno?

**Pregunta de reflexión**: ¿Qué ventaja tendría `signalfd` sobre el
self-pipe trick en este caso? ¿Y qué desventaja de portabilidad?

---

### Ejercicio 3: Auditor de signal safety

Escribe un programa de análisis estático simplificado que lea un archivo
`.c` y detecte llamadas a funciones no seguras dentro de signal handlers:

```c
int audit_signal_safety(const char *filename);
```

Requisitos:
1. Identificar funciones que se pasan a `signal()` o `sigaction()`.
2. Dentro del cuerpo de esas funciones, buscar llamadas a funciones
   no seguras: `printf`, `fprintf`, `malloc`, `free`, `exit`,
   `syslog`, `strerror`, `strlen` (debatible), etc.
3. Reportar cada violación con número de línea.
4. No necesita ser un parser C completo — un scanner basado en regex
   o string matching es suficiente.

```
  $ ./audit_safety test_handler.c
  test_handler.c:15: WARNING: printf() in signal handler 'my_handler'
  test_handler.c:16: WARNING: malloc() in signal handler 'my_handler'
  test_handler.c:22: WARNING: exit() in signal handler 'cleanup'
                      (use _exit instead)
  Found 3 signal safety violations.
```

**Predicción antes de codificar**: ¿Cómo distinguirías el cuerpo de un
handler del resto del código? ¿Basta con buscar la función entre `{` y
`}`, o hay casos que fallarían?

**Pregunta de reflexión**: Los compiladores como GCC no advierten sobre
llamadas no seguras en handlers. ¿Por qué es difícil implementar esta
verificación en un compilador? (Pista: piensa en punteros a función e
indirección.)
