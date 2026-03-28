# signalfd — Manejar Señales como File Descriptors

## Índice

1. [El problema de las señales asíncronas](#1-el-problema-de-las-señales-asíncronas)
2. [signalfd: señales como I/O](#2-signalfd-señales-como-io)
3. [struct signalfd_siginfo](#3-struct-signalfd_siginfo)
4. [Integración con poll/epoll](#4-integración-con-pollepoll)
5. [Event loop unificado](#5-event-loop-unificado)
6. [signalfd con señales RT](#6-signalfd-con-señales-rt)
7. [signalfd vs otras alternativas](#7-signalfd-vs-otras-alternativas)
8. [Consideraciones con threads](#8-consideraciones-con-threads)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema de las señales asíncronas

Los signal handlers tienen restricciones severas: solo funciones
async-signal-safe, no printf, no malloc, race conditions con el programa
principal. El self-pipe trick soluciona parte del problema, pero añade
complejidad. Lo ideal sería manejar señales **como cualquier otro
evento de I/O**:

```
  Problema: mundos separados

  ┌─────────────┐     ┌──────────────────┐
  │  Señales     │     │  I/O             │
  │              │     │                  │
  │  handler()   │     │  poll/epoll      │
  │  flags       │     │  read/write      │
  │  sig_atomic_t│     │  event loop      │
  │  EINTR       │     │                  │
  └──────────────┘     └──────────────────┘
       ↕                      ↕
  Async, restrictivo    Síncrono, flexible

  Solución: signalfd

  ┌──────────────────────────────────────┐
  │  Todo es I/O                         │
  │                                      │
  │  poll/epoll vigila:                  │
  │    - sockets     (POLLIN)            │
  │    - pipes       (POLLIN)            │
  │    - timerfd     (POLLIN)            │
  │    - signalfd    (POLLIN) ← señales  │
  │                                      │
  │  Un solo event loop, sin handlers    │
  └──────────────────────────────────────┘
```

---

## 2. signalfd: señales como I/O

`signalfd` (Linux 2.6.22+) crea un file descriptor desde el cual se
pueden **leer** señales:

```c
#include <sys/signalfd.h>

int signalfd(int fd, const sigset_t *mask, int flags);
```

- `fd`: `-1` para crear un nuevo fd, o un fd existente para modificar.
- `mask`: conjunto de señales a capturar.
- `flags`: `SFD_NONBLOCK`, `SFD_CLOEXEC` (o ambos con OR).
- Retorna el fd, o -1 en error.

### Ejemplo básico

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <unistd.h>

int main(void)
{
    /* 1. Block the signals (REQUIRED) */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /* 2. Create signalfd */
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd == -1) {
        perror("signalfd");
        return 1;
    }

    /* 3. Read signals as structs */
    printf("PID %d waiting (Ctrl+C or kill -TERM)...\n", getpid());

    struct signalfd_siginfo info;
    ssize_t n = read(sfd, &info, sizeof(info));

    if (n != sizeof(info)) {
        perror("read signalfd");
        return 1;
    }

    printf("received signal %d from PID %d\n",
           info.ssi_signo, info.ssi_pid);

    close(sfd);
    return 0;
}
```

### Flujo completo

```
  1. sigprocmask(SIG_BLOCK, {SIGINT, SIGTERM})
     → las señales no se entregan, quedan pendientes

  2. signalfd(-1, {SIGINT, SIGTERM}, SFD_CLOEXEC)
     → crea fd que "drena" las señales pendientes

  3. read(sfd, &info, sizeof(info))
     → bloquea hasta que una señal llegue
     → devuelve struct signalfd_siginfo con todos los detalles

  4. Procesamiento normal — printf, malloc, todo permitido
     → no estamos en un handler, estamos en código normal
```

### Por qué hay que bloquear las señales

Si las señales no están bloqueadas, se entregan a través del **handler
por defecto** (o un handler instalado) en vez de quedarse pendientes
para el signalfd. El signalfd lee señales **pendientes**; si no están
bloqueadas, nunca se vuelven pendientes:

```
  Señal NO bloqueada:
    señal llega → handler/default → signalfd nunca la ve

  Señal bloqueada:
    señal llega → pendiente → signalfd la consume con read()
```

---

## 3. struct signalfd_siginfo

Cada `read` del signalfd devuelve una o más estructuras de tamaño fijo
(128 bytes):

```c
struct signalfd_siginfo {
    uint32_t ssi_signo;    /* Signal number */
    int32_t  ssi_errno;    /* Error number (usually 0) */
    int32_t  ssi_code;     /* Signal code (SI_USER, SI_QUEUE...) */
    uint32_t ssi_pid;      /* Sender PID */
    uint32_t ssi_uid;      /* Sender UID */
    int32_t  ssi_fd;       /* File descriptor (SIGIO) */
    uint32_t ssi_tid;      /* Kernel timer ID (SIGALRM) */
    uint32_t ssi_band;     /* Band event (SIGIO) */
    uint32_t ssi_overrun;  /* Timer overrun count */
    uint32_t ssi_trapno;   /* Trap number */
    int32_t  ssi_status;   /* Exit status or signal (SIGCHLD) */
    int32_t  ssi_int;      /* Integer from sigqueue */
    uint64_t ssi_ptr;      /* Pointer from sigqueue */
    /* ... padding to 128 bytes ... */
};
```

### Campos más usados

| Campo          | Cuándo es relevante                     |
|----------------|-----------------------------------------|
| `ssi_signo`    | Siempre — número de señal               |
| `ssi_pid`      | Siempre — PID del remitente             |
| `ssi_uid`      | Siempre — UID del remitente             |
| `ssi_code`     | Siempre — cómo se generó                |
| `ssi_int`      | Con sigqueue — dato entero adjunto      |
| `ssi_status`   | Con SIGCHLD — exit status del hijo      |

### Leer múltiples señales

El buffer de `read` puede contener múltiples structs si hay varias
señales pendientes:

```c
struct signalfd_siginfo buf[8];  /* Read up to 8 signals at once */

ssize_t n = read(sfd, buf, sizeof(buf));
if (n <= 0) { /* handle error */ }

int count = n / sizeof(struct signalfd_siginfo);

for (int i = 0; i < count; i++) {
    printf("signal %d from PID %d\n",
           buf[i].ssi_signo, buf[i].ssi_pid);
}
```

---

## 4. Integración con poll/epoll

La verdadera potencia de signalfd es que el fd se puede vigilar con
`poll`, `epoll` o `select`, integrándolo con otro I/O:

### Con poll

```c
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>
#include <unistd.h>

int main(void)
{
    /* Block signals and create signalfd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

    /* Set up poll with a socket and the signalfd */
    int server_fd = create_server_socket(8080);

    struct pollfd fds[2];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    fds[1].fd = sfd;
    fds[1].events = POLLIN;

    printf("server running on port 8080 (PID %d)\n", getpid());

    int running = 1;
    while (running) {
        int ret = poll(fds, 2, -1);
        if (ret == -1) {
            perror("poll");
            break;
        }

        /* Handle signals */
        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo info;
            while (read(sfd, &info, sizeof(info)) == sizeof(info)) {
                switch (info.ssi_signo) {
                case SIGINT:
                case SIGTERM:
                    printf("shutdown signal from PID %d\n", info.ssi_pid);
                    running = 0;
                    break;
                case SIGCHLD:
                    printf("child %d exited with status %d\n",
                           info.ssi_pid, info.ssi_status);
                    waitpid(info.ssi_pid, NULL, WNOHANG);
                    break;
                }
            }
        }

        /* Handle new connections */
        if (running && (fds[0].revents & POLLIN)) {
            int client = accept(server_fd, NULL, NULL);
            if (client >= 0)
                handle_client(client);
        }
    }

    close(sfd);
    close(server_fd);
    printf("clean shutdown\n");
    return 0;
}
```

### Con epoll

```c
#include <sys/epoll.h>
#include <sys/signalfd.h>

int epfd = epoll_create1(EPOLL_CLOEXEC);

/* Add signalfd to epoll */
struct epoll_event ev = {
    .events = EPOLLIN,
    .data.fd = sfd,
};
epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

/* Add server socket to epoll */
ev.data.fd = server_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

/* Event loop */
struct epoll_event events[16];
while (running) {
    int n = epoll_wait(epfd, events, 16, -1);

    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == sfd) {
            /* Signal event */
            struct signalfd_siginfo info;
            read(sfd, &info, sizeof(info));
            handle_signal(&info);
        } else if (events[i].data.fd == server_fd) {
            /* New connection */
            accept_client(server_fd);
        }
    }
}
```

---

## 5. Event loop unificado

El patrón ideal combina signalfd con timerfd e inotifyfd para tener
**un solo mecanismo** para todos los eventos:

```c
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <unistd.h>

int main(void)
{
    /* Signal handling via signalfd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    int sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

    /* Periodic timer via timerfd */
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec ts = {
        .it_interval = { .tv_sec = 5 },  /* Every 5 seconds */
        .it_value    = { .tv_sec = 5 },
    };
    timerfd_settime(timer_fd, 0, &ts, NULL);

    /* File monitoring via inotify */
    int ino_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    inotify_add_watch(ino_fd, "/etc/myapp/config.ini",
                      IN_MODIFY | IN_CLOSE_WRITE);

    /* All events in one poll */
    enum { FD_SIGNAL, FD_TIMER, FD_INOTIFY, FD_COUNT };
    struct pollfd fds[FD_COUNT];
    fds[FD_SIGNAL].fd  = sig_fd;    fds[FD_SIGNAL].events  = POLLIN;
    fds[FD_TIMER].fd   = timer_fd;  fds[FD_TIMER].events   = POLLIN;
    fds[FD_INOTIFY].fd = ino_fd;    fds[FD_INOTIFY].events = POLLIN;

    int running = 1;
    while (running) {
        int ret = poll(fds, FD_COUNT, -1);
        if (ret == -1) break;

        /* Signals */
        if (fds[FD_SIGNAL].revents & POLLIN) {
            struct signalfd_siginfo info;
            while (read(sig_fd, &info, sizeof(info)) > 0) {
                if (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT)
                    running = 0;
                else if (info.ssi_signo == SIGHUP)
                    reload_config();
            }
        }

        /* Timer tick */
        if (fds[FD_TIMER].revents & POLLIN) {
            uint64_t expirations;
            read(timer_fd, &expirations, sizeof(expirations));
            print_stats();
        }

        /* Config file changed */
        if (fds[FD_INOTIFY].revents & POLLIN) {
            char buf[4096];
            read(ino_fd, buf, sizeof(buf));
            printf("config file changed, reloading...\n");
            reload_config();
        }
    }

    close(sig_fd);
    close(timer_fd);
    close(ino_fd);
    return 0;
}
```

```
  ┌──────────────────────────────────────────┐
  │            Event Loop (poll)             │
  │                                          │
  │  ┌───────────┐  ┌───────────┐           │
  │  │ signalfd  │  │ timerfd   │           │
  │  │ SIGTERM   │  │ cada 5s   │           │
  │  │ SIGINT    │  │ stats     │           │
  │  │ SIGHUP    │  │           │           │
  │  └─────┬─────┘  └─────┬─────┘           │
  │        │              │                  │
  │        ▼              ▼                  │
  │  ┌─────────────────────────────┐         │
  │  │       poll(fds, 3, -1)      │         │
  │  └─────────────────────────────┘         │
  │        ▲                                 │
  │        │                                 │
  │  ┌─────┴─────┐                           │
  │  │ inotifyfd │                           │
  │  │ config    │                           │
  │  │ changed   │                           │
  │  └───────────┘                           │
  └──────────────────────────────────────────┘
```

---

## 6. signalfd con señales RT

signalfd soporta señales de tiempo real con encolamiento y datos
adjuntos, combinando lo mejor de ambos mundos:

```c
#define SIG_EVENT (SIGRTMIN + 0)

int main(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIG_EVENT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

    printf("PID %d ready for RT events\n", getpid());

    int running = 1;
    while (running) {
        struct pollfd pfd = { .fd = sfd, .events = POLLIN };
        poll(&pfd, 1, -1);

        struct signalfd_siginfo info;
        while (read(sfd, &info, sizeof(info)) == sizeof(info)) {
            if (info.ssi_signo == SIGTERM) {
                running = 0;
                break;
            }

            if (info.ssi_signo == SIG_EVENT) {
                /* ssi_int contains the sigqueue'd integer */
                printf("event: data=%d from PID=%d (code=%d)\n",
                       info.ssi_int, info.ssi_pid, info.ssi_code);
            }
        }
    }

    close(sfd);
    return 0;
}
```

Enviar desde otro proceso:

```c
union sigval val = { .sival_int = 42 };
sigqueue(target_pid, SIGRTMIN + 0, val);
/* target reads: ssi_int = 42, ssi_code = SI_QUEUE */
```

Las señales RT se encolan, así que signalfd lee cada una por separado
con su dato adjunto. Esto hace de signalfd + señales RT un sistema de
mensajería sencillo entre procesos.

---

## 7. signalfd vs otras alternativas

### vs Signal handlers

| Aspecto                 | Handler               | signalfd               |
|-------------------------|-----------------------|------------------------|
| Funciones disponibles   | Solo async-signal-safe| Todas                  |
| Modelo de programación  | Asíncrono (interrumpe)| Síncrono (event loop)  |
| Race conditions         | Frecuentes            | Eliminadas             |
| Integración con poll    | Self-pipe trick       | Directo                |
| Portabilidad            | POSIX (todos los Unix)| Solo Linux             |

### vs Self-pipe trick

```c
/* Self-pipe trick: 15+ lines of boilerplate */
int pipe_fds[2];
pipe(pipe_fds);
fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK);
/* Install handler that writes to pipe */
/* Add pipe[0] to poll */
/* Read byte from pipe in event loop */
/* Parse which signal it was */

/* signalfd: 3 lines */
sigprocmask(SIG_BLOCK, &mask, NULL);
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
/* Add sfd to poll, read struct with all details */
```

signalfd es más limpio, da más información (PID, UID, datos RT), y
no necesita handler alguno.

### vs sigwaitinfo

| Aspecto          | sigwaitinfo            | signalfd                |
|------------------|------------------------|-------------------------|
| Multiplexar con I/O | No (bloquea solo en señales) | Sí (poll/epoll) |
| Timeout          | sigtimedwait           | poll timeout            |
| Información      | siginfo_t              | signalfd_siginfo        |
| Threads          | Per-thread             | fd compartible          |
| Portabilidad     | POSIX                  | Solo Linux              |

### Portabilidad

signalfd es **Linux-only**. Para código portable:

```c
#ifdef __linux__
    /* Use signalfd */
    #include <sys/signalfd.h>
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
#else
    /* Fallback: self-pipe trick or kqueue (BSD) */
    int pipe_fds[2];
    pipe(pipe_fds);
    /* ... setup signal handler that writes to pipe ... */
#endif
```

En BSD/macOS, `kqueue` con `EVFILT_SIGNAL` ofrece funcionalidad
equivalente.

---

## 8. Consideraciones con threads

### El signalfd es per-thread o per-process

La máscara de señales es **per-thread**, pero las señales dirigidas al
proceso (no a un thread específico) pueden ser entregadas a cualquier
thread que no las tenga bloqueadas.

Patrón recomendado para multithreading:

```c
int main(void)
{
    /* 1. Block signals in main thread BEFORE creating any threads */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /* 2. Create signalfd */
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

    /* 3. Create worker threads (they inherit the blocked mask) */
    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_create(&workers[i], NULL, worker_func, NULL);

    /* 4. Main thread handles signals via signalfd */
    while (running) {
        struct pollfd pfd = { .fd = sfd, .events = POLLIN };
        poll(&pfd, 1, 1000);

        if (pfd.revents & POLLIN) {
            struct signalfd_siginfo info;
            read(sfd, &info, sizeof(info));
            handle_signal(&info);
        }
    }

    /* 5. Signal workers to stop */
    for (int i = 0; i < NUM_WORKERS; i++)
        pthread_cancel(workers[i]);

    close(sfd);
    return 0;
}
```

### Thread dedicado a señales

```c
void *signal_thread(void *arg)
{
    int sfd = *(int *)arg;

    while (1) {
        struct signalfd_siginfo info;
        ssize_t n = read(sfd, &info, sizeof(info));
        if (n != sizeof(info)) break;

        switch (info.ssi_signo) {
        case SIGTERM:
            request_shutdown();
            return NULL;
        case SIGHUP:
            reload_configuration();
            break;
        }
    }
    return NULL;
}

int main(void)
{
    /* Block in all threads */
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);

    /* Dedicated signal-handling thread */
    pthread_t sig_thread;
    pthread_create(&sig_thread, NULL, signal_thread, &sfd);

    /* Other threads do normal work */
    /* ... */

    pthread_join(sig_thread, NULL);
    close(sfd);
}
```

### fork y signalfd

Después de `fork`, el hijo hereda el fd del signalfd. Pero las señales
dirigidas al padre no llegan al fd del hijo — cada proceso tiene su
propia cola de señales pendientes. El hijo debería crear su propio
signalfd o cerrar el heredado.

---

## 9. Errores comunes

### Error 1: No bloquear señales antes de crear signalfd

```c
/* MAL: señales se entregan via handler antes de llegar al fd */
int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
/* Signals delivered to default handler, signalfd never sees them */

/* BIEN: bloquear PRIMERO */
sigprocmask(SIG_BLOCK, &mask, NULL);
int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
```

Este es el error más común. Sin bloquear, las señales se entregan
inmediatamente (handler o default) y no quedan pendientes para el
signalfd.

### Error 2: No usar SFD_NONBLOCK con poll/epoll

```c
/* MAL: read bloquea, impidiendo procesar otros eventos */
while (1) {
    poll(fds, nfds, -1);

    if (fds[SIG_IDX].revents & POLLIN) {
        struct signalfd_siginfo info;
        /* This read blocks if poll woke for another fd */
        read(sfd, &info, sizeof(info));  /* May block */
    }
}

/* BIEN: SFD_NONBLOCK para drain loop seguro */
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

/* In event loop: */
if (fds[SIG_IDX].revents & POLLIN) {
    struct signalfd_siginfo info;
    while (read(sfd, &info, sizeof(info)) == sizeof(info)) {
        handle_signal(&info);
    }
    /* EAGAIN = no more signals, loop continues */
}
```

### Error 3: Olvidar consumir SIGCHLD con waitpid

```c
/* MAL: signalfd notifica SIGCHLD pero no reap el zombie */
if (info.ssi_signo == SIGCHLD) {
    printf("child died\n");
    /* Zombie still exists! */
}

/* BIEN: waitpid después de leer SIGCHLD */
if (info.ssi_signo == SIGCHLD) {
    printf("child %d died, status=%d\n",
           info.ssi_pid, info.ssi_status);
    /* Still need to reap — signalfd doesn't do it */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
```

signalfd notifica que SIGCHLD llegó, pero no llama a `waitpid`. El
zombie sigue existiendo hasta que se haga reaping explícito.

### Error 4: No verificar el tamaño de read

```c
/* MAL: partial read no verificado */
struct signalfd_siginfo info;
read(sfd, &info, sizeof(info));
printf("signal %d\n", info.ssi_signo);  /* May be garbage */

/* BIEN: verificar que se leyó una estructura completa */
struct signalfd_siginfo info;
ssize_t n = read(sfd, &info, sizeof(info));

if (n == -1) {
    if (errno == EAGAIN) return;  /* No signals (non-blocking) */
    perror("read signalfd");
    return;
}
if (n != sizeof(info)) {
    fprintf(stderr, "partial read from signalfd\n");
    return;
}

printf("signal %d\n", info.ssi_signo);
```

### Error 5: Heredar signalfd innecesariamente a hijos

```c
/* MAL: hijo hereda el signalfd sin SFD_CLOEXEC */
int sfd = signalfd(-1, &mask, 0);  /* No SFD_CLOEXEC */

if (fork() == 0) {
    /* Child has sfd open — fd leak */
    execvp("child_program", argv);
    /* child_program has an extra open fd it doesn't know about */
}

/* BIEN: siempre usar SFD_CLOEXEC */
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                      signalfd                                │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Crear:                                                      │
  │    sigprocmask(SIG_BLOCK, &mask, NULL);  ← PRIMERO           │
  │    int sfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC); │
  │                                                              │
  │  Leer:                                                       │
  │    struct signalfd_siginfo info;                              │
  │    read(sfd, &info, sizeof(info));                           │
  │    info.ssi_signo   → número de señal                        │
  │    info.ssi_pid     → PID remitente                          │
  │    info.ssi_uid     → UID remitente                          │
  │    info.ssi_int     → dato de sigqueue                       │
  │    info.ssi_status  → exit status (SIGCHLD)                  │
  │    info.ssi_code    → SI_USER, SI_QUEUE, etc.                │
  │                                                              │
  │  Integrar con poll/epoll:                                    │
  │    struct pollfd pfd = { .fd = sfd, .events = POLLIN };      │
  │    poll(&pfd, 1, timeout);                                   │
  │    if (pfd.revents & POLLIN) { read(sfd, ...); }             │
  │                                                              │
  │  Event loop unificado (Linux):                               │
  │    signalfd  → señales                                       │
  │    timerfd   → temporizadores                                │
  │    inotifyfd → cambios en archivos                           │
  │    sockets   → red                                           │
  │    → todo en un solo poll/epoll                              │
  │                                                              │
  │  Señales RT:                                                 │
  │    Se encolan, ssi_int tiene datos de sigqueue               │
  │                                                              │
  │  Threads:                                                    │
  │    Bloquear señales ANTES de crear threads                   │
  │    Main thread o thread dedicado lee signalfd                │
  │                                                              │
  │  SIGCHLD:                                                    │
  │    signalfd notifica, pero waitpid sigue siendo necesario    │
  │                                                              │
  │  vs alternativas:                                            │
  │    Handlers: restrictivo, async   → signalfd: flexible, sync │
  │    Self-pipe: boilerplate         → signalfd: 3 líneas       │
  │    sigwaitinfo: no multiplexa I/O → signalfd: sí con poll    │
  │    Portabilidad: solo Linux (BSD: kqueue EVFILT_SIGNAL)      │
  │                                                              │
  │  Modificar máscara:                                          │
  │    signalfd(sfd, &new_mask, 0)    reusar fd existente        │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Servidor con event loop unificado

Escribe un servidor TCP echo que maneje todo con un solo event loop
basado en poll + signalfd:

Requisitos:
1. `signalfd` para SIGINT, SIGTERM (shutdown) y SIGHUP (log reopen).
2. Socket servidor que acepta conexiones.
3. Poll vigila: signalfd + server socket + todos los client sockets.
4. Echo: lo que el cliente envía, el servidor lo devuelve.
5. SIGTERM/SIGINT: cerrar todas las conexiones y salir.
6. SIGHUP: imprimir estadísticas (conexiones activas, bytes totales).
7. Máximo 10 conexiones simultáneas.

```
  $ ./echo_server 8080 &
  [server] PID 5678 listening on port 8080

  $ echo "hello" | nc localhost 8080
  hello

  $ kill -HUP 5678
  [stats] connections: 0, total_bytes: 6

  $ kill -TERM 5678
  [server] shutting down...
```

**Predicción antes de codificar**: ¿Cuántos file descriptors necesitas
vigilar con poll si hay 5 clientes conectados? ¿Cómo dimensionas el
array de pollfd?

**Pregunta de reflexión**: ¿Por qué un solo poll con signalfd es más
robusto que un handler de SIGTERM que pone un flag? (Pista: piensa en
qué pasa si la señal llega justo cuando poll va a llamarse.)

---

### Ejercicio 2: Process manager con signalfd

Implementa un process manager que gestione varios procesos hijos:

```c
typedef struct {
    const char *name;
    const char *cmd;
    char *const *argv;
    pid_t       pid;        /* 0 = not running */
    int         restarts;
    int         auto_restart;
} managed_process_t;
```

Requisitos:
1. Leer una lista de procesos a gestionar (hardcoded o desde archivo).
2. Fork+exec cada proceso.
3. Event loop con signalfd vigilando SIGCHLD, SIGTERM, SIGHUP, SIGUSR1.
4. SIGCHLD: identificar qué hijo murió (por `ssi_pid`), reportar,
   y relanzar si `auto_restart` está activo.
5. SIGTERM: enviar SIGTERM a todos los hijos, esperar 5s, SIGKILL
   a los que sigan vivos, y salir.
6. SIGHUP: relanzar todos los procesos (graceful restart).
7. SIGUSR1: imprimir estado de todos los procesos gestionados.

**Predicción antes de codificar**: Si dos hijos mueren simultáneamente,
¿signalfd entrega dos lecturas separadas con SIGCHLD o solo una?
¿Cuál sería el comportamiento con un handler tradicional?

**Pregunta de reflexión**: systemd usa un mecanismo similar internamente.
¿Por qué systemd usa epoll en vez de poll, y signalfd en vez de handlers?

---

### Ejercicio 3: IPC ligero con signalfd + sigqueue

Implementa un sistema de mensajería entre dos procesos usando señales RT
y signalfd:

**Protocolo**:
- SIGRTMIN+0: mensaje de texto (ssi_int = offset en shared memory).
- SIGRTMIN+1: ping (ssi_int = sequence number).
- SIGRTMIN+2: pong (ssi_int = sequence number del ping).
- SIGRTMIN+3: shutdown.

**Servidor**:
1. Crea un signalfd para SIGRTMIN+0 a SIGRTMIN+3.
2. Responde pings con pongs.
3. Imprime mensajes de texto.
4. Shutdown limpio en SIGRTMIN+3.

**Cliente**:
1. Envía un ping con sigqueue, espera pong.
2. Mide round-trip time.
3. Envía mensajes de texto.
4. Envía shutdown al terminar.

```
  $ ./msg_server &
  [server] PID 5678 ready

  $ ./msg_client 5678
  ping #1: 0.042ms
  ping #2: 0.038ms
  sent message: "hello"
  sent shutdown
```

**Predicción antes de codificar**: ¿Cuál es el overhead de latencia de
sigqueue + signalfd + read comparado con write + read en un pipe?

**Pregunta de reflexión**: ¿Por qué las señales RT son un mecanismo de
IPC inferior a pipes o sockets para la mayoría de aplicaciones? ¿En qué
nicho específico son la mejor opción?
