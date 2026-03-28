# epoll

## Índice
1. [Por qué epoll](#1-por-qué-epoll)
2. [Arquitectura de epoll](#2-arquitectura-de-epoll)
3. [API: epoll_create, epoll_ctl, epoll_wait](#3-api-epoll_create-epoll_ctl-epoll_wait)
4. [Level-triggered vs edge-triggered](#4-level-triggered-vs-edge-triggered)
5. [Servidor TCP con epoll (level-triggered)](#5-servidor-tcp-con-epoll-level-triggered)
6. [Servidor TCP con epoll (edge-triggered)](#6-servidor-tcp-con-epoll-edge-triggered)
7. [EPOLLONESHOT](#7-epolloneshot)
8. [epoll con señales: signalfd y timerfd](#8-epoll-con-señales-signalfd-y-timerfd)
9. [Rendimiento y escalabilidad](#9-rendimiento-y-escalabilidad)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Por qué epoll

`select` y `poll` comparten un defecto estructural: en cada llamada,
el kernel debe escanear **todos** los fds registrados para
determinar cuáles están listos. Con 10,000 conexiones donde solo
5 tienen actividad:

```
poll():
  1. Copiar 10,000 pollfd structs al kernel       (~80 KB)
  2. Kernel registra waiters en 10,000 fds
  3. 5 fds disparan → kernel marca 5 revents
  4. Kernel desregistra 10,000 waiters
  5. Copiar 10,000 pollfd structs de vuelta        (~80 KB)
  6. Tu código itera 10,000 structs buscando 5

epoll_wait():
  1. Kernel ya tiene los fds registrados (de antes)
  2. 5 fds disparan → kernel los pone en la ready list
  3. Copiar 5 eventos al userspace                 (~60 bytes)
  4. Tu código itera solo 5 eventos
```

```
Costo por llamada:

            select/poll             epoll
          ┌─────────────┐       ┌─────────────┐
Registro  │ O(N) cada   │       │ O(1) por    │
          │ llamada     │       │ epoll_ctl   │
          │             │       │ (una vez)   │
          ├─────────────┤       ├─────────────┤
Espera    │ O(N) scan   │       │ O(1) por    │
          │ todos los   │       │ evento      │
          │ fds         │       │ (ready list)│
          ├─────────────┤       ├─────────────┤
Copia     │ O(N) todo   │       │ O(k) solo   │
          │ el array    │       │ los listos  │
          └─────────────┘       └─────────────┘

          N = total de fds       k = fds listos
```

---

## 2. Arquitectura de epoll

epoll tiene un diseño en dos fases: **registro** (una vez por fd)
y **espera** (cada iteración del event loop):

```
Fase 1: Registro (epoll_ctl)       Fase 2: Espera (epoll_wait)

   epoll_ctl(ADD, fd=5)               epoll_wait(epfd, events, ...)
   epoll_ctl(ADD, fd=8)                      │
   epoll_ctl(ADD, fd=12)                     ▼
         │                             ┌───────────┐
         ▼                             │ Ready list│
   ┌──────────────┐                    │  fd=8     │
   │   epoll       │                    │  fd=12    │
   │   instance    │   callback         └─────┬─────┘
   │  ┌─────────┐ │◀── cuando fd              │
   │  │ rb-tree │ │    está listo        Retorna solo
   │  │ fd=5    │ │                      los 2 listos
   │  │ fd=8    │ │                      (no los 10,000
   │  │ fd=12   │ │                       registrados)
   │  │ ...     │ │
   │  └─────────┘ │
   └──────────────┘
```

### Estructura interna del kernel

```
struct epoll (simplificado):

  ┌─────────────────────────────────────────┐
  │ Red-black tree (rb-tree)                │
  │   Almacena TODOS los fds registrados    │
  │   Búsqueda/inserción/eliminación O(log n)│
  │                                         │
  │   [fd=3] ── [fd=7] ── [fd=15]           │
  │            /    \                        │
  │        [fd=5]  [fd=12]                   │
  ├─────────────────────────────────────────┤
  │ Ready list (lista enlazada)             │
  │   Solo los fds que tienen eventos       │
  │   pendientes                            │
  │                                         │
  │   fd=7 → fd=12 → (vacía)               │
  ├─────────────────────────────────────────┤
  │ Wait queue                              │
  │   Threads dormidos en epoll_wait        │
  └─────────────────────────────────────────┘
```

Cuando un fd recibe datos, el kernel ejecuta un **callback** que
lo añade a la ready list. `epoll_wait` simplemente lee la ready
list sin escanear nada.

---

## 3. API: epoll_create, epoll_ctl, epoll_wait

### Crear la instancia epoll

```c
#include <sys/epoll.h>

int epoll_create1(int flags);
// flags: 0 o EPOLL_CLOEXEC
// Retorna: fd del epoll instance (¡es un fd, hay que cerrarlo!)

// Versión antigua (obsoleta pero funciona):
int epoll_create(int size);
// size: ignorado desde Linux 2.6.8, pero debe ser > 0
```

```c
int epfd = epoll_create1(EPOLL_CLOEXEC);
if (epfd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
}
// epfd es un fd como cualquier otro — cerrarlo con close(epfd)
```

### Registrar/modificar/eliminar fds

```c
int epoll_ctl(int epfd, int op, int fd,
              struct epoll_event *event);
// epfd:  fd del epoll instance
// op:    EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL
// fd:    el fd a registrar/modificar/eliminar
// event: qué eventos monitorear + datos asociados
// Retorna: 0 éxito, -1 error
```

```c
struct epoll_event {
    uint32_t     events;  // máscara de eventos
    epoll_data_t data;    // datos del usuario
};

typedef union epoll_data {
    void    *ptr;     // puntero genérico
    int      fd;      // file descriptor
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
```

Ejemplo de registro:

```c
struct epoll_event ev;
ev.events = EPOLLIN;        // monitorear lectura
ev.data.fd = client_fd;     // guardar el fd para identificarlo después

epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
```

### Operaciones de epoll_ctl

```c
// Añadir un fd
struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// Modificar eventos monitoreados
ev.events = EPOLLIN | EPOLLOUT;
epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

// Eliminar un fd (event puede ser NULL desde Linux 2.6.9)
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
```

> **Nota**: cuando cierras un fd con `close()`, se elimina
> **automáticamente** de todos los epoll instances. Pero si el fd
> fue duplicado con `dup()`, solo se elimina cuando se cierra la
> **última** referencia.

### Esperar eventos

```c
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
// events:     array donde el kernel escribe los eventos listos
// maxevents:  tamaño del array (cuántos eventos puedes recibir)
// timeout:    -1 = indefinido, 0 = polling, >0 = milisegundos
// Retorna:    número de eventos listos (0 si timeout, -1 si error)
```

```c
#define MAX_EVENTS 64
struct epoll_event events[MAX_EVENTS];

int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
for (int i = 0; i < nready; i++) {
    int fd = events[i].data.fd;
    uint32_t ev = events[i].events;

    if (ev & EPOLLIN) {
        // fd tiene datos para leer
    }
    if (ev & EPOLLOUT) {
        // fd puede aceptar escritura
    }
    if (ev & (EPOLLERR | EPOLLHUP)) {
        // error o desconexión
    }
}
```

### Eventos de epoll

| Evento | Significado |
|--------|-------------|
| `EPOLLIN` | Datos disponibles para leer |
| `EPOLLOUT` | Buffer de escritura disponible |
| `EPOLLERR` | Error (siempre monitoreado, no hace falta pedirlo) |
| `EPOLLHUP` | Hangup (siempre monitoreado) |
| `EPOLLRDHUP` | Peer hizo shutdown de escritura |
| `EPOLLET` | **Edge-triggered** (en vez de level-triggered) |
| `EPOLLONESHOT` | Desactivar después del primer evento |
| `EPOLLEXCLUSIVE` | Solo un thread recibe el wakeup (thundering herd) |

---

## 4. Level-triggered vs edge-triggered

Esta es la distinción más importante de epoll. Afecta cuándo
`epoll_wait` reporta que un fd está listo:

### Level-triggered (LT) — el default

epoll_wait retorna **mientras** el fd esté listo. Si hay datos
en el buffer y no los lees todos, epoll_wait te notifica de nuevo:

```
Buffer del fd:  [████████]  ← 8 KB de datos

epoll_wait #1 → EPOLLIN  (hay datos)
  read(fd, buf, 4096)    → lee 4 KB
Buffer:         [████    ]  ← 4 KB restantes

epoll_wait #2 → EPOLLIN  (sigue habiendo datos)
  read(fd, buf, 4096)    → lee 4 KB
Buffer:         [        ]  ← vacío

epoll_wait #3 → (duerme, no hay datos)
```

**LT es tolerante**: si olvidas leer todos los datos, epoll te
recuerda en la siguiente llamada. Es el modo por defecto y el más
seguro.

### Edge-triggered (ET) — EPOLLET

epoll_wait retorna solo cuando **el estado cambia** (de "no listo"
a "listo"). Si hay datos y no los lees todos, **no te vuelve a
notificar** hasta que lleguen datos **nuevos**:

```
Buffer del fd:  [████████]  ← 8 KB de datos

epoll_wait #1 → EPOLLIN  (cambio: vacío → datos)
  read(fd, buf, 4096)    → lee 4 KB
Buffer:         [████    ]  ← 4 KB restantes

epoll_wait #2 → (duerme, NO notifica — no hubo cambio de estado)
  ¡Los 4 KB restantes quedan perdidos hasta que lleguen datos nuevos!
```

### Diagrama de señal analógica

```
Nivel de datos en el buffer:

        ┌──────┐    ┌───┐        ┌────────┐
        │      │    │   │        │        │
────────┘      └────┘   └────────┘        └────

Level-triggered:
████████████████████████████████████████████████
        ████████    ███          ██████████
   "hay datos" → reporta continuamente

Edge-triggered:
        ↑      ↑    ↑   ↑        ↑        ↑
        │      │    │   │        │        │
   Solo en las transiciones (flancos ascendentes)
```

### Reglas para edge-triggered

ET requiere disciplina estricta:

1. **Siempre usar sockets non-blocking** (`O_NONBLOCK` o
   `SOCK_NONBLOCK`)
2. **Leer/escribir en loop hasta `EAGAIN`** — consumir todo lo
   disponible en cada notificación
3. Si no sigues estas reglas, pierdes datos silenciosamente

```c
// Patrón obligatorio con edge-triggered:
void handle_epollin_et(int fd) {
    char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;  // ya no hay más datos → salir del loop
            perror("read");
            break;
        }
        if (n == 0) {
            // EOF: peer cerró
            close(fd);
            return;
        }
        process_data(buf, n);
    }
}
```

### ¿Cuándo usar cada modo?

| Modo | Ventaja | Desventaja | Cuándo usar |
|------|---------|------------|-------------|
| LT (default) | Simple, tolerante | Más wakeups | Casi siempre |
| ET (EPOLLET) | Menos wakeups, mayor throughput | Fácil perder datos si no lees todo | Servidores de alto rendimiento |

> **Recomendación**: empieza con LT. Solo cambia a ET cuando
> tengas evidencia de que los wakeups extra son un cuello de botella
> (raro para la mayoría de aplicaciones).

---

## 5. Servidor TCP con epoll (level-triggered)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define PORT        8080
#define MAX_EVENTS  64
#define BUF_SIZE    4096

int main(void) {
    // Crear listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    printf("Listening on port %d\n", PORT);

    // Crear epoll instance
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    // Registrar listening socket
    struct epoll_event ev = {
        .events  = EPOLLIN,
        .data.fd = listen_fd
    };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    // Event loop
    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;

            // ¿Error?
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                fprintf(stderr, "epoll error on fd %d\n", fd);
                close(fd);  // se elimina de epoll automáticamente
                continue;
            }

            // ¿Nueva conexión?
            if (fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd,
                                       (struct sockaddr *)&client_addr,
                                       &client_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          ip, sizeof(ip));
                printf("Client [%d] connected: %s:%d\n",
                       client_fd, ip, ntohs(client_addr.sin_port));

                // Registrar en epoll
                struct epoll_event cev = {
                    .events  = EPOLLIN,
                    .data.fd = client_fd
                };
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                continue;
            }

            // ¿Datos de un cliente?
            if (events[i].events & EPOLLIN) {
                char buf[BUF_SIZE];
                ssize_t n = read(fd, buf, sizeof(buf));

                if (n <= 0) {
                    printf("Client [%d] disconnected\n", fd);
                    close(fd);
                } else {
                    write(fd, buf, n);  // echo
                }
            }
        }
    }

    close(epfd);
    close(listen_fd);
    return 0;
}
```

```bash
gcc -o epoll_server epoll_server.c && ./epoll_server
```

### Comparación con select/poll

Observa la simplicidad: no hay arrays que reconstruir, no hay
iteración de todos los fds — solo procesamos los eventos que
`epoll_wait` devuelve.

```
select:   FD_ZERO + FD_SET × N  →  select  →  FD_ISSET × max_fd
poll:     (array persiste)       →  poll    →  iterate N pollfds
epoll:    (registrado una vez)   →  wait    →  iterate k eventos
                                                (k = solo los listos)
```

---

## 6. Servidor TCP con epoll (edge-triggered)

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define PORT        8080
#define MAX_EVENTS  64
#define BUF_SIZE    4096

// Hacer un fd non-blocking (obligatorio para ET)
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);

    int epfd = epoll_create1(EPOLL_CLOEXEC);

    struct epoll_event ev = {
        .events  = EPOLLIN | EPOLLET,  // ← edge-triggered
        .data.fd = listen_fd
    };
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int nready = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nready == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                close(fd);
                continue;
            }

            // Nueva conexión (ET: aceptar TODAS las pendientes)
            if (fd == listen_fd) {
                for (;;) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept4(listen_fd,
                                            (struct sockaddr *)&client_addr,
                                            &client_len,
                                            SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;  // no más conexiones pendientes
                        perror("accept4");
                        break;
                    }

                    struct epoll_event cev = {
                        .events  = EPOLLIN | EPOLLET,
                        .data.fd = client_fd
                    };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                    printf("Client [%d] connected\n", client_fd);
                }
                continue;
            }

            // Datos de cliente (ET: leer TODO hasta EAGAIN)
            if (events[i].events & EPOLLIN) {
                char buf[BUF_SIZE];
                for (;;) {
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n == -1) {
                        if (errno == EAGAIN)
                            break;  // todo leído
                        perror("read");
                        close(fd);
                        break;
                    }
                    if (n == 0) {
                        printf("Client [%d] disconnected\n", fd);
                        close(fd);
                        break;
                    }
                    write(fd, buf, n);  // echo
                }
            }
        }
    }

    close(epfd);
    close(listen_fd);
    return 0;
}
```

### Diferencias clave con la versión LT

| Aspecto | Level-triggered | Edge-triggered |
|---------|----------------|----------------|
| Flag en events | `EPOLLIN` | `EPOLLIN \| EPOLLET` |
| Sockets | Pueden ser blocking | **DEBEN** ser non-blocking |
| accept() en listen | Un `accept` por wakeup | Loop hasta `EAGAIN` |
| read() en cliente | Un `read` basta | Loop hasta `EAGAIN` |
| `accept4` | Opcional | Recomendado (NONBLOCK atómico) |

---

## 7. EPOLLONESHOT

`EPOLLONESHOT` desactiva el monitoreo de un fd después de que se
reporta **un** evento. Para recibirlo de nuevo, debes rearmarlo
con `EPOLL_CTL_MOD`:

```c
// Registrar con EPOLLONESHOT
struct epoll_event ev = {
    .events  = EPOLLIN | EPOLLONESHOT,
    .data.fd = fd
};
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// Después de procesar el evento, rearmar:
ev.events = EPOLLIN | EPOLLONESHOT;
epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
```

### ¿Para qué sirve?

Cuando usas **múltiples threads** con un mismo epoll fd. Sin
EPOLLONESHOT, si un fd tiene datos, varios threads podrían recibir
el mismo evento y procesar el mismo fd concurrentemente:

```
Sin EPOLLONESHOT (peligroso con multi-thread):

Thread 1: epoll_wait → fd=5 listo → read(fd=5, ...)
Thread 2: epoll_wait → fd=5 listo → read(fd=5, ...)
  ↑ Ambos procesan el mismo fd → race condition

Con EPOLLONESHOT:

Thread 1: epoll_wait → fd=5 listo → read(fd=5, ...)
Thread 2: epoll_wait → (fd=5 desactivado, no lo ve)
Thread 1: termina → EPOLL_CTL_MOD(fd=5) → reactivado
Thread 2: epoll_wait → fd=5 listo (si hay datos nuevos)
```

### EPOLLEXCLUSIVE (Linux 4.5+)

Una alternativa para el "thundering herd" problem: si múltiples
threads esperan en `epoll_wait` en el mismo epfd, normalmente
**todos** despiertan cuando un fd está listo. `EPOLLEXCLUSIVE`
despierta solo **uno**:

```c
struct epoll_event ev = {
    .events  = EPOLLIN | EPOLLEXCLUSIVE,
    .data.fd = listen_fd
};
epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
```

---

## 8. epoll con señales: signalfd y timerfd

Una de las ventajas de epoll sobre select/poll es que se integra
con mecanismos de Linux que convierten **señales y timers en fds**,
permitiendo manejar todo en un solo event loop:

### signalfd — señales como fd

```c
#include <sys/signalfd.h>
#include <signal.h>

// Bloquear las señales que queremos manejar vía fd
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);
sigprocmask(SIG_BLOCK, &mask, NULL);

// Crear fd que recibe las señales
int sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

// Registrar en epoll
struct epoll_event ev = {
    .events  = EPOLLIN,
    .data.fd = sig_fd
};
epoll_ctl(epfd, EPOLL_CTL_ADD, sig_fd, &ev);

// En el event loop:
if (fd == sig_fd) {
    struct signalfd_siginfo ssi;
    read(sig_fd, &ssi, sizeof(ssi));
    if (ssi.ssi_signo == SIGINT) {
        printf("Received SIGINT\n");
        // cleanup and exit
    }
}
```

No más `pselect`/`ppoll` para manejar señales — todo pasa por
el mismo `epoll_wait`.

### timerfd — timers como fd

```c
#include <sys/timerfd.h>

// Crear un timer que dispara cada 2 segundos
int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

struct itimerspec ts = {
    .it_interval = { .tv_sec = 2 },  // repetir cada 2s
    .it_value    = { .tv_sec = 2 }   // primer disparo en 2s
};
timerfd_settime(timer_fd, 0, &ts, NULL);

// Registrar en epoll
struct epoll_event ev = {
    .events  = EPOLLIN,
    .data.fd = timer_fd
};
epoll_ctl(epfd, EPOLL_CTL_ADD, timer_fd, &ev);

// En el event loop:
if (fd == timer_fd) {
    uint64_t expirations;
    read(timer_fd, &expirations, sizeof(expirations));
    printf("Timer expired %lu times\n", expirations);
    // ejecutar tarea periódica
}
```

### eventfd — notificaciones entre threads

```c
#include <sys/eventfd.h>

int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

// Registrar en epoll
struct epoll_event ev = { .events = EPOLLIN, .data.fd = efd };
epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &ev);

// Un thread puede despertar al event loop escribiendo:
uint64_t val = 1;
write(efd, &val, sizeof(val));

// El event loop lee:
uint64_t val;
read(efd, &val, sizeof(val));  // val = número de writes acumulados
```

### Event loop unificado

```
┌─────────────────────────────────────────┐
│              epoll_wait                  │
│                                          │
│  ┌────────────┐  ← listen socket        │
│  │ listen_fd  │     (nuevas conexiones)  │
│  ├────────────┤                          │
│  │ client_fd1 │  ← datos de clientes    │
│  │ client_fd2 │                          │
│  ├────────────┤                          │
│  │ signal_fd  │  ← SIGINT/SIGTERM       │
│  ├────────────┤                          │
│  │ timer_fd   │  ← ticks periódicos     │
│  ├────────────┤                          │
│  │ event_fd   │  ← notificaciones       │
│  └────────────┘     inter-thread        │
│                                          │
│  Todo en un solo thread,                 │
│  un solo punto de espera                 │
└─────────────────────────────────────────┘
```

---

## 9. Rendimiento y escalabilidad

### Benchmark: select vs poll vs epoll

Servicio echo, conexiones idle (pocas con tráfico):

```
Total connections   select     poll      epoll
──────────────────────────────────────────────────
100                 5 µs       5 µs      5 µs
1,000               50 µs      48 µs     5 µs
10,000              n/a*       500 µs    6 µs
100,000             n/a*       n/a**     8 µs

Tiempo por epoll_wait/poll/select call
* FD_SETSIZE limit   ** impractical
```

epoll escala linealmente con el número de eventos **activos**,
no con el número total de conexiones. Esto lo hace ideal para el
patrón **C10K** (10,000 conexiones concurrentes) y más allá.

### Cuándo epoll NO es mejor

- **Pocos fds (< 100)**: la ventaja de epoll es insignificante.
  select/poll son más simples y suficientes.
- **Todos los fds siempre activos**: si el 90% de los fds tienen
  datos en cada iteración, la ventaja de la ready list desaparece.
- **Portabilidad**: epoll es Linux-only. macOS usa `kqueue`,
  Windows usa `IOCP`.

### Alternativas en otros OS

| OS | Mecanismo | Similar a |
|----|-----------|-----------|
| Linux | epoll | — |
| FreeBSD/macOS | kqueue | epoll (más general) |
| Windows | IOCP | epoll (proactivo, no reactivo) |
| Solaris | /dev/poll, event ports | epoll |

Librerías cross-platform como **libevent**, **libev**, y **libuv**
(la base de Node.js) abstraen estas diferencias.

---

## 10. Errores comunes

### Error 1: usar EPOLLET sin non-blocking

```c
// MAL: edge-triggered con socket blocking
struct epoll_event ev = {
    .events = EPOLLIN | EPOLLET,
    .data.fd = client_fd  // blocking socket!
};
epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

// En el handler:
read(client_fd, buf, sizeof(buf));
// Si hay más datos pero no caben en buf,
// el segundo read bloquea el thread entero
// (porque no es NONBLOCK y ET no volverá a notificar)
```

**Solución**: siempre hacer el fd non-blocking con ET:

```c
set_nonblocking(client_fd);  // O usar SOCK_NONBLOCK en socket()
```

### Error 2: no consumir todo con edge-triggered

```c
// MAL: solo un read con ET
if (events[i].events & EPOLLIN) {
    read(fd, buf, sizeof(buf));
    // Quedan datos en el buffer → ET no notifica de nuevo
    // → datos perdidos hasta que lleguen datos NUEVOS
}
```

**Solución**: loop hasta EAGAIN:

```c
if (events[i].events & EPOLLIN) {
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1 && errno == EAGAIN) break;
        if (n <= 0) { close(fd); break; }
        process(buf, n);
    }
}
```

### Error 3: olvidar cerrar el epoll fd

```c
// MAL: leak del epoll fd
int epfd = epoll_create1(0);
// ... usar ...
// olvidar close(epfd)
// Es un fd como cualquier otro — hay que cerrarlo
```

**Solución**:

```c
close(epfd);
```

### Error 4: EPOLL_CTL_ADD sobre un fd ya registrado

```c
// MAL: añadir dos veces
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);  // EEXIST
```

**Solución**: usar `MOD` si el fd ya está registrado:

```c
if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    if (errno == EEXIST)
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}
```

### Error 5: asumir que close() desregistra siempre

```c
int fd = accept(listen_fd, ...);
int fd2 = dup(fd);
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

close(fd);
// fd2 sigue abierto → el fd NO se desregistra de epoll
// epoll puede seguir reportando eventos en la open file description

close(fd2);  // ahora sí se desregistra
```

Si usas `dup()` o `fork()` (que duplica los fds), asegúrate de
entender que epoll monitorea la **open file description** del
kernel, no el número de fd. El fd solo se elimina de epoll
cuando se cierra la última referencia a esa open file description.

**Regla práctica**: siempre haz `EPOLL_CTL_DEL` explícitamente
antes de `close()` para evitar sorpresas:

```c
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
close(fd);
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                          epoll                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  #include <sys/epoll.h>        // Linux-only                 │
│                                                              │
│  CREAR:                                                      │
│    int epfd = epoll_create1(EPOLL_CLOEXEC);                  │
│    // epfd es un fd — cerrarlo con close()                   │
│                                                              │
│  REGISTRAR/MODIFICAR/ELIMINAR:                               │
│    struct epoll_event ev = { .events=EPOLLIN, .data.fd=fd }; │
│    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);                  │
│    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);                  │
│    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);                 │
│                                                              │
│  ESPERAR:                                                    │
│    struct epoll_event events[64];                             │
│    int n = epoll_wait(epfd, events, 64, timeout_ms);         │
│    for (i = 0; i < n; i++) {                                 │
│        events[i].data.fd    // qué fd                        │
│        events[i].events     // qué pasó                      │
│    }                                                         │
│                                                              │
│  EVENTOS:                                                    │
│    EPOLLIN        lectura lista                              │
│    EPOLLOUT       escritura lista                             │
│    EPOLLERR       error (siempre reportado)                  │
│    EPOLLHUP       hangup (siempre reportado)                 │
│    EPOLLRDHUP     peer half-close                            │
│    EPOLLET        edge-triggered (default: level-triggered)  │
│    EPOLLONESHOT   un solo evento, luego rearmar con MOD      │
│    EPOLLEXCLUSIVE despertar solo 1 thread                    │
│                                                              │
│  LEVEL-TRIGGERED (default):                                  │
│    • Notifica mientras haya datos                            │
│    • Tolerante: no pierdes datos si no lees todo             │
│    • Sockets pueden ser blocking o non-blocking              │
│                                                              │
│  EDGE-TRIGGERED (EPOLLET):                                   │
│    • Notifica solo en cambio de estado                        │
│    • DEBE usar non-blocking sockets                          │
│    • DEBE leer/escribir en loop hasta EAGAIN                 │
│    • Menos wakeups, mayor throughput potencial               │
│                                                              │
│  INTEGRACIÓN:                                                │
│    signalfd  → señales como fd (SIGINT, SIGTERM...)          │
│    timerfd   → timers como fd (periódicos o one-shot)        │
│    eventfd   → notificaciones inter-thread                   │
│    Todo monitoreado en un solo epoll_wait                    │
│                                                              │
│  RENDIMIENTO:                                                │
│    • O(1) por evento (vs O(N) de select/poll)                │
│    • Registra fds una vez (vs copiar todo cada llamada)      │
│    • Devuelve solo fds listos (vs escanear todos)            │
│    • Escala a C10K+ conexiones                               │
│    • Para <100 fds, select/poll son igual de rápidos          │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: servidor echo con epoll LT

Implementa un servidor echo TCP con epoll level-triggered:

1. Escuchar en un puerto recibido por argumento.
2. Usar `epoll_create1` + `epoll_ctl` + `epoll_wait`.
3. Aceptar clientes dinámicamente (sin límite fijo).
4. Hacer echo de lo que reciba cada cliente.
5. Imprimir conexiones y desconexiones.
6. Usar un `timerfd` para imprimir estadísticas cada 10 segundos:
   número de clientes conectados, bytes totales transferidos.

Probar con múltiples `nc` concurrentes.

**Pregunta de reflexión**: en el servidor LT, ¿qué pasa si
`write()` no puede enviar todos los bytes (buffer de envío lleno)?
¿Necesitas monitorear EPOLLOUT? ¿Cómo manejarías datos parcialmente
enviados sin perder bytes?

---

### Ejercicio 2: servidor echo con epoll ET

Convierte el ejercicio anterior a edge-triggered:

1. Todos los sockets deben ser non-blocking.
2. Usar `accept4` con `SOCK_NONBLOCK` en loop hasta EAGAIN.
3. `read` en loop hasta EAGAIN.
4. Verificar que no se pierden datos: enviar un archivo grande
   desde el cliente y comparar con lo recibido de vuelta.

```bash
# Generar archivo de prueba
dd if=/dev/urandom of=test_input bs=1M count=10

# Enviar y recibir
cat test_input | nc localhost 9000 > test_output

# Comparar
md5sum test_input test_output
```

**Pregunta de reflexión**: en modo ET, si el servidor está
procesando datos del cliente A (en el loop de read hasta EAGAIN)
y el cliente B envía datos, ¿cuándo se procesarán los datos de B?
¿Puede el loop de A retrasar significativamente a B? ¿Cómo lo
resolverías?

---

### Ejercicio 3: event loop unificado

Construye un event loop que maneje todo en un solo `epoll_wait`:

1. **TCP server** (listen socket): acepta conexiones y hace echo.
2. **signalfd**: captura SIGINT para shutdown limpio (cerrar todas
   las conexiones, imprimir estadísticas, salir).
3. **timerfd**: cada 5 segundos, enviar `"PING\n"` a todos los
   clientes conectados. Los clientes que no respondan `"PONG\n"`
   en 10 segundos son desconectados (timeout).
4. **stdin**: si el operador escribe un comando en stdin:
   - `list` — mostrar clientes conectados
   - `kick N` — desconectar al cliente con fd N
   - `quit` — shutdown limpio

Probar el shutdown con Ctrl+C (debe cerrar todo limpiamente, no
dejar sockets en TIME_WAIT innecesarios).

**Pregunta de reflexión**: este event loop es esencialmente un
"mini reactor". Frameworks como libuv (Node.js), Tokio (Rust), y
libevent (C) implementan exactamente este patrón. ¿Qué ventaja
tiene centralizar todo el I/O en un solo punto de espera versus
tener threads dedicados para timers, señales, y conexiones?
