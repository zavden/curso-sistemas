# Self-Pipe Trick — Despertar poll/select desde un Signal Handler

## Índice

1. [El problema: señales vs event loops](#1-el-problema-señales-vs-event-loops)
2. [La race condition fundamental](#2-la-race-condition-fundamental)
3. [El self-pipe trick](#3-el-self-pipe-trick)
4. [Implementación paso a paso](#4-implementación-paso-a-paso)
5. [Variante con eventfd](#5-variante-con-eventfd)
6. [pselect y ppoll: la solución POSIX](#6-pselect-y-ppoll-la-solución-posix)
7. [Comparación de estrategias](#7-comparación-de-estrategias)
8. [Patrones reales](#8-patrones-reales)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema: señales vs event loops

Los programas basados en event loops (servidores, GUIs, daemons) pasan
la mayor parte del tiempo bloqueados en `poll()`, `select()` o
`epoll_wait()`, esperando eventos de I/O. El problema: ¿cómo hacer que
una señal despierte al event loop?

```
  Event loop típico:

  while (running) {
      poll(fds, nfds, -1);  ← bloqueado aquí el 99% del tiempo
      for each ready fd:
          handle_event(fd);
  }

  Señal SIGTERM llega:
  ┌──────────────────────────────────────┐
  │ Si SA_RESTART está activo:           │
  │   handler ejecuta, pone flag         │
  │   poll se REINICIA automáticamente   │
  │   flag nunca se revisa ← PROBLEMA    │
  │                                      │
  │ Si SA_RESTART NO está activo:        │
  │   handler ejecuta, pone flag         │
  │   poll retorna -1 con EINTR          │
  │   flag se revisa ← OK, pero...       │
  │   TODOS los EINTR en el programa     │
  │   necesitan manejo manual            │
  └──────────────────────────────────────┘
```

Ninguna opción es buena:
- Con `SA_RESTART`: el event loop nunca se entera de la señal.
- Sin `SA_RESTART`: hay que manejar `EINTR` en cada syscall del programa.

---

## 2. La race condition fundamental

Incluso sin `SA_RESTART`, existe una race condition al combinar flags
con poll:

```c
volatile sig_atomic_t got_signal = 0;

void handler(int sig)
{
    (void)sig;
    got_signal = 1;
}

int main(void)
{
    /* ... setup handler without SA_RESTART ... */

    while (!got_signal) {           /* (1) Check flag */
        /* ← SIGNAL ARRIVES HERE ← */  /* (2) Window of vulnerability */
        poll(fds, nfds, -1);        /* (3) Poll blocks forever */
        /* handler set got_signal=1, but poll already started */
        /* Without SA_RESTART, poll returns EINTR... but what
           if the signal arrived between (1) and (3)? */
    }
}
```

```
  Timeline de la race condition:

  t1: while (!got_signal)    → got_signal is 0, enter loop
  t2:                        ← SIGNAL ARRIVES HERE
                             → handler sets got_signal = 1
                             → handler returns
  t3: poll(fds, nfds, -1)   → poll sees no pending signal
                             → blocks indefinitely
                             → got_signal is 1 but NEVER checked again
```

`sigsuspend` resuelve esto para `pause()`, pero no ayuda con `poll()`.
El self-pipe trick es la solución clásica para event loops.

---

## 3. El self-pipe trick

Inventado por Daniel J. Bernstein (djb). La idea: crear un pipe y
hacer que el signal handler escriba en él. El event loop vigila el
extremo de lectura del pipe junto con los demás fds:

```
  ┌───────────────────────────────────────────┐
  │                                           │
  │  Signal handler                           │
  │      │                                    │
  │      write(pipe[1], &byte, 1)             │
  │      │                                    │
  │      ▼                                    │
  │  ┌────────┐                               │
  │  │ pipe   │                               │
  │  │[1]→ [0]│                               │
  │  └───┬────┘                               │
  │      │                                    │
  │      ▼                                    │
  │  poll() sees POLLIN on pipe[0]            │
  │      │                                    │
  │      ▼                                    │
  │  Event loop wakes up, reads pipe          │
  │  → handles signal in normal code          │
  │                                           │
  └───────────────────────────────────────────┘
```

### Por qué funciona

1. `write()` es **async-signal-safe** — se puede llamar desde un handler.
2. Escribir al pipe hace que `poll()` retorne con `POLLIN` en el read
   end — sin importar `SA_RESTART`.
3. No hay race condition: si la señal llega antes de `poll()`, el byte
   ya está en el pipe; cuando `poll()` empiece, verá `POLLIN`
   inmediatamente.

---

## 4. Implementación paso a paso

### Implementación completa

```c
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ── Step 1: Global pipe ── */
static int sig_pipe[2];  /* [0]=read, [1]=write */

/* ── Step 2: Minimal handler — just write a byte ── */
static void signal_handler(int sig)
{
    int saved_errno = errno;
    unsigned char s = (unsigned char)sig;
    write(sig_pipe[1], &s, 1);  /* Async-signal-safe */
    errno = saved_errno;
}

/* ── Step 3: Setup function ── */
static int setup_signal_pipe(void)
{
    if (pipe(sig_pipe) == -1)
        return -1;

    /* Write end MUST be non-blocking:
       if pipe fills up, handler must not block */
    int flags = fcntl(sig_pipe[1], F_GETFL);
    fcntl(sig_pipe[1], F_SETFL, flags | O_NONBLOCK);

    /* Read end also non-blocking for drain loop */
    flags = fcntl(sig_pipe[0], F_GETFL);
    fcntl(sig_pipe[0], F_SETFL, flags | O_NONBLOCK);

    /* Close-on-exec for both ends */
    fcntl(sig_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(sig_pipe[1], F_SETFD, FD_CLOEXEC);

    return 0;
}

/* ── Step 4: Install handler for desired signals ── */
static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;  /* SA_RESTART is fine now! */
    sigfillset(&sa.sa_mask);   /* Block all signals during handler */

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
}

/* ── Step 5: Event loop ── */
int main(void)
{
    if (setup_signal_pipe() == -1) {
        perror("pipe");
        return 1;
    }
    install_signal_handlers();
    signal(SIGPIPE, SIG_IGN);

    int server_fd = create_server(8080);  /* Your server setup */

    printf("server PID %d running\n", getpid());

    int running = 1;
    while (running) {
        struct pollfd fds[2];
        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[1].fd = sig_pipe[0];
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, -1);
        if (ret == -1) {
            if (errno == EINTR) continue;  /* Extra safety */
            perror("poll");
            break;
        }

        /* ── Handle signals ── */
        if (fds[1].revents & POLLIN) {
            unsigned char sig;
            while (read(sig_pipe[0], &sig, 1) > 0) {
                switch (sig) {
                case SIGINT:
                case SIGTERM:
                    printf("shutdown requested\n");
                    running = 0;
                    break;
                case SIGHUP:
                    printf("reloading config\n");
                    reload_config();
                    break;
                case SIGCHLD:
                    while (waitpid(-1, NULL, WNOHANG) > 0)
                        ;
                    break;
                }
            }
        }

        /* ── Handle I/O ── */
        if (running && (fds[0].revents & POLLIN)) {
            accept_and_handle(server_fd);
        }
    }

    close(sig_pipe[0]);
    close(sig_pipe[1]);
    close(server_fd);
    printf("clean shutdown\n");
    return 0;
}
```

### Detalles críticos de implementación

```
  ┌─────────────────────────────────────────────────────────┐
  │  Checklist del self-pipe trick:                         │
  │                                                         │
  │  ✓ pipe[1] NONBLOCK — handler no debe bloquearse        │
  │  ✓ pipe[0] NONBLOCK — drain loop en event loop          │
  │  ✓ FD_CLOEXEC — no filtrar a hijos                     │
  │  ✓ SA_RESTART — seguro porque poll despierta vía pipe  │
  │  ✓ sigfillset(sa_mask) — handler atómico                │
  │  ✓ Guardar/restaurar errno en handler                   │
  │  ✓ Drain loop: while(read(pipe[0],&b,1)>0)             │
  │  ✓ Byte = número de señal (distinguir cuál llegó)       │
  └─────────────────────────────────────────────────────────┘
```

---

## 5. Variante con eventfd

`eventfd` (Linux 2.6.22+) es una alternativa más eficiente al pipe para
señalización entre contextos. Usa un solo fd en vez de dos:

```c
#include <sys/eventfd.h>

static int sig_efd;

static void signal_handler(int sig)
{
    int saved_errno = errno;
    uint64_t val = sig;
    write(sig_efd, &val, sizeof(val));
    errno = saved_errno;
}

int main(void)
{
    /* EFD_NONBLOCK: write doesn't block in handler
       EFD_CLOEXEC: don't leak to children
       EFD_SEMAPHORE: each read returns 1 event (not sum) */
    sig_efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (sig_efd == -1) { perror("eventfd"); return 1; }

    install_signal_handlers();

    struct pollfd fds[2];
    fds[0].fd = server_fd;    fds[0].events = POLLIN;
    fds[1].fd = sig_efd;      fds[1].events = POLLIN;

    while (running) {
        poll(fds, 2, -1);

        if (fds[1].revents & POLLIN) {
            uint64_t val;
            read(sig_efd, &val, sizeof(val));
            handle_signal((int)val);
        }

        /* ... handle I/O ... */
    }

    close(sig_efd);
}
```

### eventfd vs pipe

| Aspecto         | pipe              | eventfd              |
|-----------------|-------------------|----------------------|
| File descriptors| 2 (read + write)  | 1                    |
| Kernel memory   | Buffer de pipe    | Un uint64_t          |
| Semántica       | Stream de bytes   | Contador/semáforo    |
| Portabilidad    | POSIX             | Solo Linux           |
| Overhead        | Mayor             | Menor                |

> **Nota sobre eventfd sin EFD_SEMAPHORE**: Sin este flag, `write` **suma**
> al contador y `read` devuelve el **total acumulado** y lo resetea a 0.
> Con `EFD_SEMAPHORE`, cada read decrementa en 1. Para el self-pipe trick,
> el pipe clásico suele ser más claro porque preserva cada byte individual.

---

## 6. pselect y ppoll: la solución POSIX

POSIX añadió `pselect` y `ppoll` para resolver la race condition sin
necesidad de pipes adicionales:

### ppoll

```c
#include <poll.h>
#include <signal.h>

int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *timeout,
          const sigset_t *sigmask);
```

`ppoll` atómicamente:
1. Reemplaza la máscara de señales por `sigmask`.
2. Ejecuta `poll` con las señales desbloqueadas.
3. Restaura la máscara original al retornar.

```c
volatile sig_atomic_t got_sigterm = 0;

void handler(int sig)
{
    (void)sig;
    got_sigterm = 1;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,  /* No SA_RESTART needed */
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);

    /* Block SIGTERM in normal execution */
    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    /* Mask to use during ppoll: SIGTERM UNBLOCKED */
    sigset_t ppoll_mask;
    sigprocmask(SIG_BLOCK, NULL, &ppoll_mask);  /* Get current */
    sigdelset(&ppoll_mask, SIGTERM);             /* Remove SIGTERM */

    struct pollfd fds[1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    while (!got_sigterm) {
        int ret = ppoll(fds, 1, NULL, &ppoll_mask);
        /*
         * Atomically:
         * 1. mask = ppoll_mask (SIGTERM unblocked)
         * 2. poll(fds, 1, -1)
         *    → if SIGTERM arrives: handler runs, ppoll returns EINTR
         *    → if I/O ready: ppoll returns normally
         * 3. mask restored (SIGTERM blocked again)
         *
         * No race condition: unblock + poll is one atomic operation
         */

        if (ret == -1) {
            if (errno == EINTR) continue;
            perror("ppoll");
            break;
        }

        if (fds[0].revents & POLLIN)
            handle_client(server_fd);
    }

    printf("clean shutdown\n");
    return 0;
}
```

### pselect

```c
#include <sys/select.h>

int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds, const struct timespec *timeout,
            const sigset_t *sigmask);
```

Misma semántica atómica que `ppoll` pero con la interfaz de `select`.

### Por qué resuelve la race condition

```
  Sin ppoll (race):
  t1: check flag → 0          ← SIGTERM aquí → flag=1
  t2: poll() blocks           ← flag never checked, poll blocks

  Con ppoll (safe):
  t1: check flag → 0
  t2: ppoll() atomically:
      unblock SIGTERM + start waiting
      ← SIGTERM aquí → handler runs → ppoll returns EINTR
      → flag checked in next iteration
```

---

## 7. Comparación de estrategias

```
  ┌──────────────┬──────────┬───────────┬──────────┬──────────┐
  │ Estrategia   │Portable  │ Race-free │ Fds extra│Complejidad│
  ├──────────────┼──────────┼───────────┼──────────┼──────────┤
  │ Flag + EINTR │ POSIX    │ NO ✗      │ 0        │ Baja     │
  │ Self-pipe    │ POSIX    │ SÍ ✓      │ 2        │ Media    │
  │ eventfd      │ Linux    │ SÍ ✓      │ 1        │ Media    │
  │ ppoll/pselect│ POSIX    │ SÍ ✓      │ 0        │ Baja     │
  │ signalfd     │ Linux    │ SÍ ✓      │ 1        │ Baja     │
  └──────────────┴──────────┴───────────┴──────────┴──────────┘
```

### Cuándo usar cada uno

| Situación                                | Recomendación         |
|------------------------------------------|-----------------------|
| Linux moderno, sin restricciones         | signalfd (más limpio) |
| Código portable POSIX                    | ppoll/pselect         |
| ppoll no disponible (muy viejo)          | Self-pipe trick       |
| Ya tienes un event loop con handler      | Self-pipe o eventfd   |
| Programa simple, pocas señales           | ppoll                 |
| Framework que usa epoll (libev, libuv)   | signalfd o self-pipe  |

---

## 8. Patrones reales

### Patrón 1: OpenSSH usa self-pipe

OpenSSH, uno de los programas más auditados del mundo, usa el self-pipe
trick para manejar SIGCHLD en su event loop:

```c
/* Simplified from OpenSSH source */
static int sigchld_pipe[2];

static void sigchld_handler(int sig)
{
    int save_errno = errno;
    (void)sig;
    write(sigchld_pipe[1], "", 1);
    errno = save_errno;
}

/* In the main select/poll loop: */
FD_SET(sigchld_pipe[0], &readset);

if (FD_ISSET(sigchld_pipe[0], &readset)) {
    char c;
    while (read(sigchld_pipe[0], &c, 1) > 0)
        ;
    /* Reap children */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
```

### Patrón 2: Nginx usa socketpair

Nginx usa una variante con `socketpair` en vez de `pipe`, porque
los sockets permiten `MSG_DONTWAIT` sin necesidad de `fcntl`:

```c
/* Simplified from nginx source */
static int ngx_signal_pair[2];

static void ngx_signal_handler(int sig)
{
    /* ... */
    char buf = sig;
    send(ngx_signal_pair[0], &buf, 1, MSG_DONTWAIT);
    /* MSG_DONTWAIT = non-blocking per-operation */
}
```

### Patrón 3: Biblioteca reutilizable

```c
/* signal_pipe.h — reusable self-pipe module */
#ifndef SIGNAL_PIPE_H
#define SIGNAL_PIPE_H

#include <signal.h>

/* Initialize the signal pipe. Call once at startup. */
int  sigpipe_init(void);

/* Get the read fd to add to your poll/select set. */
int  sigpipe_fd(void);

/* Install handler for sig that writes to the pipe. */
int  sigpipe_watch(int sig);

/* Read and return the next pending signal number.
   Returns 0 if no signal pending (non-blocking). */
int  sigpipe_recv(void);

/* Cleanup. */
void sigpipe_destroy(void);

#endif
```

```c
/* signal_pipe.c */
#include "signal_pipe.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int sp_fds[2] = {-1, -1};

static void sp_handler(int sig)
{
    int saved = errno;
    unsigned char s = (unsigned char)sig;
    write(sp_fds[1], &s, 1);
    errno = saved;
}

int sigpipe_init(void)
{
    if (pipe(sp_fds) == -1) return -1;

    for (int i = 0; i < 2; i++) {
        int fl = fcntl(sp_fds[i], F_GETFL);
        fcntl(sp_fds[i], F_SETFL, fl | O_NONBLOCK);
        fcntl(sp_fds[i], F_SETFD, FD_CLOEXEC);
    }
    return 0;
}

int sigpipe_fd(void) { return sp_fds[0]; }

int sigpipe_watch(int sig)
{
    struct sigaction sa = {
        .sa_handler = sp_handler,
        .sa_flags   = SA_RESTART,
    };
    sigfillset(&sa.sa_mask);
    return sigaction(sig, &sa, NULL);
}

int sigpipe_recv(void)
{
    unsigned char s;
    if (read(sp_fds[0], &s, 1) == 1)
        return s;
    return 0;
}

void sigpipe_destroy(void)
{
    if (sp_fds[0] != -1) close(sp_fds[0]);
    if (sp_fds[1] != -1) close(sp_fds[1]);
    sp_fds[0] = sp_fds[1] = -1;
}
```

### Uso de la biblioteca

```c
#include "signal_pipe.h"
#include <poll.h>

int main(void)
{
    sigpipe_init();
    sigpipe_watch(SIGTERM);
    sigpipe_watch(SIGHUP);
    sigpipe_watch(SIGCHLD);

    struct pollfd fds[2];
    fds[0].fd = server_fd;      fds[0].events = POLLIN;
    fds[1].fd = sigpipe_fd();   fds[1].events = POLLIN;

    int running = 1;
    while (running) {
        poll(fds, 2, -1);

        int sig;
        while ((sig = sigpipe_recv()) != 0) {
            if (sig == SIGTERM) running = 0;
            else if (sig == SIGHUP) reload_config();
            else if (sig == SIGCHLD) reap_children();
        }

        if (running && (fds[0].revents & POLLIN))
            handle_connection(server_fd);
    }

    sigpipe_destroy();
}
```

---

## 9. Errores comunes

### Error 1: Write end bloqueante

```c
/* MAL: si el pipe está lleno, handler bloquea → deadlock */
int sig_pipe[2];
pipe(sig_pipe);  /* Both ends blocking by default */

void handler(int sig)
{
    char c = sig;
    write(sig_pipe[1], &c, 1);  /* BLOCKS if pipe is full */
    /* Event loop is suspended (running handler), can't drain pipe
       → pipe stays full → handler stays blocked → DEADLOCK */
}

/* BIEN: write end SIEMPRE non-blocking */
fcntl(sig_pipe[1], F_SETFL,
      fcntl(sig_pipe[1], F_GETFL) | O_NONBLOCK);
/* If pipe is full, write fails with EAGAIN — handler returns
   immediately. Signal info is lost, but deadlock is avoided.
   In practice, pipe buffer (64KiB) never fills up. */
```

### Error 2: No hacer drain loop

```c
/* MAL: solo lee un byte, puede quedar basura en el pipe */
if (fds[SIG].revents & POLLIN) {
    char c;
    read(sig_pipe[0], &c, 1);  /* Only reads ONE signal */
    handle_signal(c);
    /* If 3 signals arrived, 2 remain in the pipe
       → next poll iteration handles one more
       → latency increases */
}

/* BIEN: drain loop hasta EAGAIN */
if (fds[SIG].revents & POLLIN) {
    unsigned char c;
    while (read(sig_pipe[0], &c, 1) > 0) {
        handle_signal(c);
    }
    /* EAGAIN: pipe is now empty */
}
```

### Error 3: No guardar/restaurar errno

```c
/* MAL: write puede modificar errno */
void handler(int sig)
{
    char c = sig;
    write(sig_pipe[1], &c, 1);
    /* If write fails (EAGAIN), errno is now EAGAIN.
       Main program might be checking errno from its own syscall
       → wrong error code, mysterious bug */
}

/* BIEN: guardar y restaurar */
void handler(int sig)
{
    int saved_errno = errno;
    char c = sig;
    write(sig_pipe[1], &c, 1);
    errno = saved_errno;
}
```

### Error 4: Usar SA_RESTART sin self-pipe

```c
/* MAL: SA_RESTART + flag sin self-pipe → poll nunca despierta */
struct sigaction sa = {
    .sa_handler = handler,
    .sa_flags   = SA_RESTART,  /* poll is restarted after signal */
};

while (!got_signal) {
    poll(fds, nfds, -1);  /* Signal arrives → handler → flag=1
                             → poll restarts → loop never exits */
}

/* BIEN: SA_RESTART + self-pipe → poll despierta vía pipe */
/* handler writes to pipe → poll returns with POLLIN on pipe fd */
```

Con el self-pipe trick, `SA_RESTART` es **correcto y deseable**: no
necesitas manejar EINTR porque poll despierta por el pipe, no por
la señal directamente.

### Error 5: Olvidar FD_CLOEXEC

```c
/* MAL: hijos heredan el pipe → fd leak */
pipe(sig_pipe);

if (fork() == 0) {
    execvp("child", argv);
    /* child has two extra open fds it doesn't know about */
    /* If write end stays open in child, pipe never gets EOF */
}

/* BIEN: FD_CLOEXEC en ambos extremos */
fcntl(sig_pipe[0], F_SETFD, FD_CLOEXEC);
fcntl(sig_pipe[1], F_SETFD, FD_CLOEXEC);

/* O usar pipe2 (Linux): */
pipe2(sig_pipe, O_NONBLOCK | O_CLOEXEC);
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                  Self-Pipe Trick                              │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Problema:                                                   │
  │    señal + flag + poll = race condition                      │
  │    SA_RESTART: poll no despierta                             │
  │    sin SA_RESTART: EINTR en todas las syscalls               │
  │                                                              │
  │  Solución:                                                   │
  │    pipe() → handler write(pipe[1]) → poll ve POLLIN          │
  │                                                              │
  │  Setup:                                                      │
  │    pipe2(fds, O_NONBLOCK | O_CLOEXEC);                      │
  │    sa.sa_handler = handler;   /* writes to pipe[1] */        │
  │    sa.sa_flags = SA_RESTART;  /* safe: pipe wakes poll */    │
  │    sigfillset(&sa.sa_mask);                                  │
  │                                                              │
  │  Handler:                                                    │
  │    int saved = errno;                                        │
  │    char c = sig;                                             │
  │    write(pipe[1], &c, 1);                                    │
  │    errno = saved;                                            │
  │                                                              │
  │  Event loop:                                                 │
  │    pollfd[N] = { .fd = pipe[0], .events = POLLIN };          │
  │    if (revents & POLLIN)                                     │
  │        while (read(pipe[0], &c, 1) > 0)                     │
  │            handle_signal(c);                                 │
  │                                                              │
  │  Checklist:                                                  │
  │    ✓ pipe[1] O_NONBLOCK (handler no bloquea)                │
  │    ✓ pipe[0] O_NONBLOCK (drain loop)                        │
  │    ✓ FD_CLOEXEC (no filtrar a hijos)                        │
  │    ✓ SA_RESTART (seguro con self-pipe)                      │
  │    ✓ errno save/restore en handler                           │
  │    ✓ Drain loop completo                                     │
  │                                                              │
  │  Alternativas:                                               │
  │    eventfd      → 1 fd en vez de 2 (Linux)                  │
  │    ppoll/pselect → sin fds extra, atómico (POSIX)           │
  │    signalfd     → sin handler, lee structs (Linux)          │
  │                                                              │
  │  Usado por: OpenSSH, nginx, libevent, Redis, Node.js        │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Biblioteca self-pipe reutilizable

Implementa la biblioteca `signal_pipe.h` mostrada en la sección 8,
con estas funciones:

```c
int  sigpipe_init(void);
int  sigpipe_fd(void);
int  sigpipe_watch(int sig);
int  sigpipe_recv(void);
void sigpipe_destroy(void);
```

Requisitos adicionales:
1. `sigpipe_watch` debe poder llamarse para múltiples señales.
2. `sigpipe_recv` debe retornar 0 si no hay señal (non-blocking).
3. Manejar el caso de `pipe` lleno (EAGAIN en el handler).
4. Incluir `sigpipe_unwatch(int sig)` para restaurar SIG_DFL.
5. Escribir un programa de prueba que vigile SIGINT, SIGTERM, SIGHUP,
   SIGUSR1 y SIGUSR2, y que imprima cuál recibió.

Probar con:
```bash
$ ./test_sigpipe &
PID 5678 watching 5 signals

$ kill -USR1 5678
received SIGUSR1
$ kill -HUP 5678
received SIGHUP
$ kill -TERM 5678
received SIGTERM — shutting down
```

**Predicción antes de codificar**: Si envías 100 señales SIGUSR1 en un
loop rápido, ¿tu biblioteca recibirá las 100? ¿Depende del tamaño del
pipe buffer?

**Pregunta de reflexión**: ¿Qué pasaría si dos threads diferentes
llaman a `sigpipe_recv` simultáneamente? ¿Necesitas protección con
mutex?

---

### Ejercicio 2: ppoll vs self-pipe benchmark

Escribe un programa que compare la latencia y overhead de tres
estrategias para despertar un event loop con señales:

1. **Self-pipe trick** con `poll`.
2. **ppoll** con máscara de señales.
3. **signalfd** con `poll`.

Metodología:
1. Fork un hijo que envíe N señales (SIGUSR1) al padre.
2. El padre mide el tiempo desde que `poll`/`ppoll` retorna hasta que
   procesa la señal.
3. Repetir 10000 veces y calcular latencia promedio y percentil 99.
4. Medir uso de CPU con `clock_gettime(CLOCK_THREAD_CPUTIME_ID)`.

```
  $ ./signal_benchmark
  Strategy        Avg latency    P99 latency    CPU per signal
  ──────────────────────────────────────────────────────────────
  self-pipe       2.1 µs         8.3 µs         1.4 µs
  ppoll           1.8 µs         6.1 µs         1.1 µs
  signalfd        1.5 µs         5.2 µs         0.9 µs
```

**Predicción antes de codificar**: ¿Cuál crees que será más rápido y
por qué? (Pista: el self-pipe tiene dos syscalls extra: write + read.)

**Pregunta de reflexión**: ¿En qué escenario real importaría una
diferencia de 1 microsegundo por señal? ¿Las señales son un mecanismo
de IPC para alto throughput?

---

### Ejercicio 3: Graceful shutdown con ppoll

Implementa un servidor multi-conexión que haga shutdown limpio usando
`ppoll` (sin self-pipe, sin signalfd):

```c
void server_run(int port, int max_clients);
```

Requisitos:
1. Bloquear SIGTERM y SIGINT con `sigprocmask`.
2. Instalar handlers que pongan un flag.
3. Usar `ppoll` con una máscara que desbloquee SIGTERM/SIGINT solo
   durante la espera.
4. En shutdown:
   - Dejar de aceptar conexiones nuevas.
   - Enviar un mensaje "server shutting down" a todos los clientes.
   - Esperar hasta 5 segundos a que los clientes cierren la conexión.
   - Cerrar conexiones restantes.
5. Manejar SIGCHLD si usas fork para atender clientes.

```
  $ ./ppoll_server 8080
  listening on port 8080

  (clients connect and chat)

  $ kill -TERM $(pidof ppoll_server)
  [server] SIGTERM received, starting graceful shutdown
  [server] notified 3 clients
  [server] waiting 5s for clients to disconnect...
  [server] 2 clients disconnected gracefully
  [server] closing 1 remaining connection
  [server] shutdown complete
```

**Predicción antes de codificar**: ¿Por qué es importante bloquear
SIGTERM fuera de ppoll? ¿Qué pasa si SIGTERM llega mientras estás en
el loop de `send()` a los clientes?

**Pregunta de reflexión**: El patrón `ppoll` no necesita fds extra ni
handlers que escriban pipes. ¿Por qué entonces OpenSSH, nginx y otros
proyectos grandes prefieren el self-pipe trick sobre ppoll?
