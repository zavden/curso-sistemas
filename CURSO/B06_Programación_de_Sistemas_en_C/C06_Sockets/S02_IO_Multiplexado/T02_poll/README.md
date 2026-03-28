# poll

## Índice
1. [Mejora sobre select](#1-mejora-sobre-select)
2. [API de poll](#2-api-de-poll)
3. [struct pollfd y eventos](#3-struct-pollfd-y-eventos)
4. [Servidor TCP con poll](#4-servidor-tcp-con-poll)
5. [Gestión dinámica del array de pollfds](#5-gestión-dinámica-del-array-de-pollfds)
6. [poll con stdin + socket](#6-poll-con-stdin--socket)
7. [ppoll](#7-ppoll)
8. [poll vs select](#8-poll-vs-select)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Mejora sobre select

`poll()` (SVR3, 1986) resuelve los problemas de diseño más
molestos de `select()`, manteniendo la misma idea fundamental:

```
Problemas de select          Cómo poll los resuelve
─────────────────────        ──────────────────────────────
FD_SETSIZE fijo (1024)       Sin límite fijo — array dinámico
fd_set se destruye           events (input) y revents (output)
  después de cada llamada      son campos separados
nfds = max_fd + 1            nfds = número de elementos del array
  (confuso)                    (intuitivo)
3 bitmaps separados          1 struct por fd con máscara de eventos
  (read, write, except)
```

```
select:                           poll:
┌────────────────────────┐        ┌──────────────────────────┐
│ fd_set readfds  (1024) │        │ struct pollfd fds[N]     │
│ fd_set writefds (1024) │        │   { fd, events, revents }│
│ fd_set exceptfds(1024) │        │   { fd, events, revents }│
│                        │        │   { fd, events, revents }│
│ 3 × 128 bytes = 384 B │        │   N × 8 bytes            │
│ (siempre 384 B aunque  │        │   (solo lo que necesitas)│
│  solo uses 3 fds)      │        │                          │
└────────────────────────┘        └──────────────────────────┘
```

Lo que poll **no** resuelve: sigue siendo O(n) — el kernel escanea
todo el array en cada llamada, y tú iteras todo el array para
encontrar los fds listos. Para eso necesitas `epoll`.

---

## 2. API de poll

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
// fds:     array de struct pollfd
// nfds:    número de elementos en el array
// timeout: milisegundos
//          -1 = esperar indefinidamente
//           0 = retornar inmediatamente (polling)
//          >0 = esperar máximo N milisegundos
//
// Retorna: número de fds con revents != 0
//          0 si timeout expiró
//         -1 en error (errno = EINTR si fue señal)
```

### Flujo de poll

```
1. Llenar array de struct pollfd (fd + events)
2. Llamar a poll() → duerme
3. poll() retorna → revents indica qué pasó
4. Iterar el array, procesar los que tienen revents != 0
5. Volver al paso 2 (¡NO hay que reconstruir el array!)
```

> **Ventaja clave**: a diferencia de select, `poll()` **no destruye**
> la entrada. El campo `events` no se modifica; la salida va en
> `revents`. Puedes reusar el array sin reconstruirlo.

---

## 3. struct pollfd y eventos

```c
struct pollfd {
    int   fd;       // file descriptor a monitorear
    short events;   // eventos que nos interesan (input)
    short revents;  // eventos que ocurrieron (output, lo llena el kernel)
};
```

### Eventos de entrada (events)

| Constante | Significado |
|-----------|-------------|
| `POLLIN` | Hay datos para leer (o conexión nueva en listen socket) |
| `POLLOUT` | Se puede escribir sin bloquear |
| `POLLPRI` | Datos urgentes (out-of-band) |
| `POLLRDHUP` | El peer cerró su lado de escritura (Linux 2.6.17+) |

### Eventos solo de salida (revents)

Estos los pone el kernel aunque no los pidas en `events`:

| Constante | Significado |
|-----------|-------------|
| `POLLERR` | Error en el fd |
| `POLLHUP` | Hangup — el otro extremo cerró la conexión |
| `POLLNVAL` | fd inválido (no abierto) |

### Tabla de combinaciones comunes

```
events = POLLIN
  → revents posibles:
    POLLIN            datos disponibles para leer
    POLLIN | POLLHUP  datos + peer cerró (último read antes de EOF)
    POLLHUP           peer cerró, sin datos pendientes
    POLLERR           error en el socket
    POLLNVAL          fd inválido

events = POLLIN | POLLOUT
  → monitored para lectura Y escritura
```

### Diferencia entre POLLHUP y POLLRDHUP

```
POLLHUP:    ambas direcciones cerradas (el fd ya no sirve)
POLLRDHUP:  solo el peer cerró escritura (half-close, aún puedes
            escribir hacia el peer)

Ejemplo con shutdown:
  Peer llama shutdown(SHUT_WR):
    → tu socket recibe POLLRDHUP (puedes seguir enviando)
  Peer llama close():
    → tu socket recibe POLLHUP (todo cerrado)
```

Para usar `POLLRDHUP` necesitas definir `_GNU_SOURCE`:

```c
#define _GNU_SOURCE
#include <poll.h>
```

### fd negativo: ignorar un slot

Si pones `fd = -1`, poll ignora ese slot (no lo monitorea y
`revents` queda en 0). Esto permite "desactivar" fds sin
compactar el array:

```c
// Desactivar un fd sin reorganizar el array
fds[i].fd = -1;  // poll lo ignora en la siguiente llamada
```

---

## 4. Servidor TCP con poll

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT       8080
#define MAX_CLIENTS 1024
#define BUF_SIZE   4096

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

    // Array de pollfds: slot 0 = listening socket
    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd     = listen_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    for (;;) {
        // --- Esperar actividad ---
        int ready = poll(fds, nfds, -1);
        if (ready == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        // --- ¿Nueva conexión en listening socket? ---
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(listen_fd,
                                (struct sockaddr *)&client_addr,
                                &client_len);
            if (new_fd == -1) {
                perror("accept");
            } else if (nfds > MAX_CLIENTS) {
                fprintf(stderr, "Too many clients\n");
                close(new_fd);
            } else {
                fds[nfds].fd     = new_fd;
                fds[nfds].events = POLLIN;
                nfds++;

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          ip, sizeof(ip));
                printf("Client [%d] connected: %s:%d\n",
                       new_fd, ip, ntohs(client_addr.sin_port));
            }
        }

        // --- ¿Datos de clientes? ---
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents == 0)
                continue;

            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                // Error o desconexión
                printf("Client [%d] error/hangup\n", fds[i].fd);
                close(fds[i].fd);
                fds[i] = fds[--nfds];  // swap con el último
                i--;
                continue;
            }

            if (fds[i].revents & POLLIN) {
                char buf[BUF_SIZE];
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));

                if (n <= 0) {
                    printf("Client [%d] disconnected\n", fds[i].fd);
                    close(fds[i].fd);
                    fds[i] = fds[--nfds];
                    i--;
                } else {
                    // Echo
                    write(fds[i].fd, buf, n);
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}
```

```bash
gcc -o poll_server poll_server.c && ./poll_server

# Probar con múltiples clientes:
nc localhost 8080   # terminal 2
nc localhost 8080   # terminal 3
nc localhost 8080   # terminal 4
```

### Comparación con el servidor select

```
                    select                        poll
─────────────────────────────────────────────────────────────
Antes de llamar     FD_ZERO + FD_SET para         Nada (array persiste)
                    CADA fd, CADA iteración

Parámetro nfds      max_fd + 1                    nro de elementos

Detectar listos     FD_ISSET(fd, &readfds)        fds[i].revents & POLLIN

Límite de fds       FD_SETSIZE (1024)             Ilimitado (heap)

Resultado           readfds/writefds destruidos   revents separado de events
```

---

## 5. Gestión dinámica del array de pollfds

Para un servidor que puede crecer sin límite fijo, usa memoria
dinámica:

```c
#include <stdlib.h>
#include <poll.h>

typedef struct {
    struct pollfd *fds;
    int            count;     // fds en uso
    int            capacity;  // slots allocados
} pollfd_array_t;

void pollfd_array_init(pollfd_array_t *a, int initial_cap) {
    a->fds      = malloc(initial_cap * sizeof(struct pollfd));
    a->count    = 0;
    a->capacity = initial_cap;
}

void pollfd_array_add(pollfd_array_t *a, int fd, short events) {
    if (a->count == a->capacity) {
        a->capacity *= 2;
        a->fds = realloc(a->fds, a->capacity * sizeof(struct pollfd));
    }
    a->fds[a->count].fd      = fd;
    a->fds[a->count].events  = events;
    a->fds[a->count].revents = 0;
    a->count++;
}

void pollfd_array_remove(pollfd_array_t *a, int index) {
    a->fds[index] = a->fds[--a->count];  // swap con último
}

void pollfd_array_free(pollfd_array_t *a) {
    free(a->fds);
}
```

Uso en el servidor:

```c
pollfd_array_t clients;
pollfd_array_init(&clients, 64);
pollfd_array_add(&clients, listen_fd, POLLIN);

for (;;) {
    int ready = poll(clients.fds, clients.count, -1);
    // ...
    // accept → pollfd_array_add(&clients, new_fd, POLLIN);
    // close  → pollfd_array_remove(&clients, i); i--;
}
```

---

## 6. poll con stdin + socket

El mismo patrón que con select, pero más limpio:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(atoi(argv[2]))
    };
    inet_pton(AF_INET, argv[1], &addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    printf("Connected. Type messages:\n");

    struct pollfd fds[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = sock_fd,      .events = POLLIN }
    };

    for (;;) {
        int ret = poll(fds, 2, -1);
        if (ret == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        // Datos del teclado
        if (fds[0].revents & POLLIN) {
            char buf[BUF_SIZE];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                shutdown(sock_fd, SHUT_WR);
                fds[0].fd = -1;  // dejar de monitorear stdin
                continue;
            }
            write(sock_fd, buf, n);
        }

        // Datos del servidor
        if (fds[1].revents & POLLIN) {
            char buf[BUF_SIZE];
            ssize_t n = read(sock_fd, buf, sizeof(buf));
            if (n <= 0) {
                printf("Server disconnected\n");
                break;
            }
            write(STDOUT_FILENO, buf, n);
        }

        // Error en el socket
        if (fds[1].revents & (POLLERR | POLLHUP)) {
            printf("Connection closed\n");
            break;
        }
    }

    close(sock_fd);
    return 0;
}
```

Observa cómo `fds[0].fd = -1` desactiva elegantemente el
monitoreo de stdin sin reconstruir el array.

---

## 7. ppoll

`ppoll()` es a `poll()` lo que `pselect()` es a `select()` — añade
máscara de señales atómica y timeout de mayor resolución:

```c
#define _GNU_SOURCE
#include <poll.h>
#include <signal.h>

int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *timeout,    // nanosegundos (no ms)
          const sigset_t *sigmask);          // máscara atómica
```

### Diferencias con poll

| Aspecto | poll | ppoll |
|---------|------|-------|
| Timeout | `int` milisegundos | `struct timespec` nanosegundos |
| Máscara de señales | No | Sí (atómica) |
| Modifica timeout | No | No |
| Portabilidad | POSIX | Linux (GNU extension) |

### Ejemplo: ppoll con manejo de señales

```c
#define _GNU_SOURCE
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t got_signal = 0;

void handler(int sig) {
    (void)sig;
    got_signal = 1;
}

int main(void) {
    struct sigaction sa = { .sa_handler = handler };
    sigaction(SIGTERM, &sa, NULL);

    // Bloquear SIGTERM normalmente
    sigset_t block, empty;
    sigemptyset(&empty);
    sigemptyset(&block);
    sigaddset(&block, SIGTERM);
    sigprocmask(SIG_BLOCK, &block, NULL);

    struct pollfd fds[] = {
        { .fd = STDIN_FILENO, .events = POLLIN }
    };

    for (;;) {
        struct timespec ts = { .tv_sec = 5 };

        // ppoll desbloquea SIGTERM atómicamente mientras espera
        int ret = ppoll(fds, 1, &ts, &empty);

        if (ret == -1 && errno == EINTR) {
            if (got_signal) {
                printf("Received SIGTERM, shutting down\n");
                break;
            }
            continue;
        }
        if (ret == 0) {
            printf("Timeout (5s idle)\n");
            continue;
        }
        if (fds[0].revents & POLLIN) {
            char buf[256];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            printf("Read %zd bytes\n", n);
        }
    }

    return 0;
}
```

---

## 8. poll vs select

### Comparación directa

```
┌──────────────────┬────────────────────┬────────────────────┐
│ Aspecto          │ select             │ poll               │
├──────────────────┼────────────────────┼────────────────────┤
│ Límite de fds    │ FD_SETSIZE (1024)  │ Sin límite fijo    │
│ Estructura       │ 3 bitmaps (r/w/e)  │ Array de pollfd    │
│ Input/Output     │ Mismo bitmap       │ events / revents   │
│                  │ (se destruye)      │ (separados)        │
│ Reconstruir      │ Sí, cada iteración │ No necesario       │
│ nfds param       │ max_fd + 1         │ nro de elementos   │
│ Timeout          │ struct timeval (µs) │ int (ms)           │
│ Timeout mod      │ Sí (Linux)         │ No                 │
│ Complejidad      │ O(nfds) kernel     │ O(nfds) kernel     │
│                  │ O(maxfd) usuario   │ O(nfds) usuario    │
│ Portabilidad     │ POSIX + Windows    │ POSIX (no Windows) │
│ Señales          │ pselect            │ ppoll (Linux)      │
│ fd sparse        │ Escanea 0..maxfd   │ Solo los listados  │
│ Memoria (10 fds) │ 384 bytes fijo     │ 80 bytes           │
│ Memoria (500 fds)│ 384 bytes fijo     │ 4000 bytes         │
└──────────────────┴────────────────────┴────────────────────┘
```

### ¿Cuándo elegir cada uno?

| Escenario | Recomendación |
|-----------|--------------|
| Portabilidad a Windows | select (Winsock) |
| Pocos fds (< 10), código simple | select o poll, ambos OK |
| > 100 fds | poll (sin límite FD_SETSIZE) |
| > 10000 fds | epoll (O(1) por evento) |
| fds con valores altos (sparse) | poll (no escanea 0..maxfd) |
| Necesitas monitorear señales | ppoll o pselect |

### El problema O(n) que ambos comparten

Tanto `select` como `poll` tienen el mismo cuello de botella
fundamental:

```
Cada vez que llamas a poll():

1. Se copian TODOS los pollfds de userspace al kernel
2. El kernel registra waiters en CADA fd
3. Cuando algún fd está listo, el kernel desregistra TODOS
4. Se copian TODOS los pollfds de vuelta a userspace
5. Tú iteras TODOS los pollfds buscando revents != 0

Con 10,000 fds pero solo 3 listos:
  → copiar 10,000 structs ida y vuelta: ~80 KB
  → registrar/desregistrar 10,000 waiters
  → iterar 10,000 structs para encontrar 3
```

`epoll` resuelve esto con un diseño fundamentalmente diferente:
registras los fds **una vez**, y `epoll_wait` solo devuelve los
que están listos.

### Benchmark: select vs poll vs epoll

```
Connections   select    poll     epoll
────────────────────────────────────────
10            1.0x      1.0x     1.0x
100           1.0x      1.0x     1.0x
1,000         2.5x      2.0x     1.0x
10,000        n/a*      15x      1.0x
100,000       n/a*      n/a**    1.0x

* select: FD_SETSIZE = 1024
** poll: demasiado lento para ser práctico
Tiempos relativos a epoll (normalizado a 1.0x)
```

Para < 100 fds, los tres tienen rendimiento equivalente.

---

## 9. Errores comunes

### Error 1: no verificar revents correctamente

```c
// MAL: solo verificar POLLIN, ignorar errores
if (fds[i].revents & POLLIN) {
    read(fds[i].fd, buf, sizeof(buf));
}
// Si revents tiene POLLERR, el read puede fallar de formas extrañas
// Si revents tiene POLLNVAL, el fd es inválido
```

**Solución**: verificar errores primero:

```c
if (fds[i].revents & (POLLERR | POLLNVAL)) {
    // Error: cerrar y remover
    close(fds[i].fd);
    fds[i] = fds[--nfds];
    i--;
    continue;
}
if (fds[i].revents & (POLLIN | POLLHUP)) {
    ssize_t n = read(fds[i].fd, buf, sizeof(buf));
    if (n <= 0) {
        // EOF o error de lectura
        close(fds[i].fd);
        fds[i] = fds[--nfds];
        i--;
    } else {
        // procesar datos
    }
}
```

> `POLLHUP` puede venir junto con `POLLIN` (hay datos pendientes
> antes del cierre). Siempre intenta `read()` cuando ves `POLLIN`,
> incluso si también hay `POLLHUP`.

### Error 2: olvidar reinicializar revents

En la práctica, poll pone revents a 0 para los fds no listos, así
que esto no suele ser un bug. Pero si usas un esquema donde
acumulas revents entre llamadas, ten cuidado:

```c
// El kernel pone revents = 0 para fds sin actividad
// Pero si procesas parcialmente y vuelves a llamar a poll,
// asegúrate de no confundir revents viejos con nuevos.
```

### Error 3: swap incorrecto al eliminar del array

```c
// MAL: olvidar decrementar i después del swap
for (int i = 1; i < nfds; i++) {
    if (should_remove(i)) {
        close(fds[i].fd);
        fds[i] = fds[--nfds];
        // ¡FALTA i--! El fd que movimos al slot i no se revisará
    }
}
```

**Solución**: siempre `i--` después de swap-and-shrink:

```c
fds[i] = fds[--nfds];
i--;  // re-revisar el slot i (ahora tiene otro fd)
```

### Error 4: no manejar EINTR

```c
// MAL: tratar EINTR como error fatal
int ret = poll(fds, nfds, -1);
if (ret == -1) {
    perror("poll failed");
    exit(EXIT_FAILURE);  // puede ser solo SIGCHLD
}
```

**Solución**:

```c
int ret = poll(fds, nfds, -1);
if (ret == -1) {
    if (errno == EINTR) continue;
    perror("poll");
    break;
}
```

### Error 5: monitorear POLLOUT sin necesitarlo

```c
// MAL: siempre monitorear escritura
fds[i].events = POLLIN | POLLOUT;
// POLLOUT retorna inmediatamente si el buffer de envío tiene espacio
// → poll nunca duerme → busy loop al 100% CPU
```

Un socket con buffer disponible **siempre** está listo para escribir.
Si monitoreas POLLOUT permanentemente, poll retornará inmediatamente
cada vez.

**Solución**: solo monitorear POLLOUT cuando tengas datos pendientes
que no pudiste enviar (short write):

```c
// Por defecto, solo lectura
fds[i].events = POLLIN;

// Solo si write() retornó menos bytes de los solicitados:
ssize_t n = write(fd, buf, len);
if (n < len) {
    // Hay datos pendientes, activar monitoreo de escritura
    fds[i].events |= POLLOUT;
    save_pending_data(buf + n, len - n);
}

// Cuando POLLOUT dispara, enviar datos pendientes:
if (fds[i].revents & POLLOUT) {
    ssize_t n = write(fds[i].fd, pending_buf, pending_len);
    if (n == pending_len) {
        // Todo enviado, dejar de monitorear escritura
        fds[i].events &= ~POLLOUT;
    } else {
        // Aún hay pendientes
        advance_pending(n);
    }
}
```

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                          poll()                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  #include <poll.h>                                           │
│                                                              │
│  ESTRUCTURA:                                                 │
│    struct pollfd {                                            │
│        int   fd;       // -1 para ignorar este slot          │
│        short events;   // qué monitorear (input)             │
│        short revents;  // qué pasó (output, lo llena kernel) │
│    };                                                        │
│                                                              │
│  EVENTOS DE ENTRADA (events):                                │
│    POLLIN     lectura lista (datos o nueva conexión)         │
│    POLLOUT    escritura lista (buffer disponible)             │
│    POLLPRI    datos urgentes (OOB)                            │
│    POLLRDHUP  peer hizo shutdown(SHUT_WR) (GNU_SOURCE)       │
│                                                              │
│  EVENTOS SOLO DE SALIDA (revents):                           │
│    POLLERR    error en el fd                                 │
│    POLLHUP    peer cerró conexión                            │
│    POLLNVAL   fd no es válido (no abierto)                   │
│                                                              │
│  LLAMAR:                                                     │
│    struct pollfd fds[N];                                     │
│    fds[0] = (struct pollfd){ .fd = listen_fd, .events = POLLIN };│
│    int ready = poll(fds, N, timeout_ms);                     │
│    // timeout: -1=indefinido, 0=polling, >0=ms               │
│                                                              │
│  RETORNO:                                                    │
│    > 0 → nro de fds con revents != 0                         │
│    = 0 → timeout expiró                                      │
│    = -1 → error (EINTR = señal)                              │
│                                                              │
│  IGNORAR UN SLOT:                                            │
│    fds[i].fd = -1;  // poll lo salta                         │
│                                                              │
│  VENTAJAS SOBRE SELECT:                                      │
│    • Sin límite FD_SETSIZE                                   │
│    • events no se destruye (no reconstruir cada vez)          │
│    • nfds = nro de elementos (no max_fd+1)                   │
│    • Más eficiente con fds sparse                            │
│                                                              │
│  LIMITACIÓN (compartida con select):                         │
│    • O(n) por llamada (kernel escanea todo el array)         │
│    • Copia array completo user↔kernel cada vez               │
│    → Para >1000 fds activos, usar epoll                      │
│                                                              │
│  ppoll (Linux, _GNU_SOURCE):                                 │
│    ppoll(fds, nfds, &timespec, &sigmask)                     │
│    • Timeout en nanosegundos (struct timespec)                │
│    • Máscara de señales atómica                              │
│    • No modifica timeout                                     │
│                                                              │
│  PATRÓN POLLOUT:                                             │
│    Solo activar POLLOUT cuando tienes datos pendientes        │
│    Desactivar cuando todo se envió                           │
│    (POLLOUT permanente = busy loop)                          │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: chat room con poll

Implementa un servidor de chat multi-usuario con `poll()`:

1. Escuchar en un puerto recibido por argumento.
2. Aceptar clientes (máximo 20).
3. Cuando un cliente envía un mensaje, reenviarlo a **todos los
   demás** clientes conectados (broadcast).
4. Cuando un cliente se conecta, anunciar a los demás:
   `"*** User joined (N connected) ***"`.
5. Cuando un cliente se desconecta, anunciar:
   `"*** User left (N connected) ***"`.
6. Usar un array dinámico de pollfds (o un array fijo de 21 slots).

Probar con 4 terminales conectadas vía `nc`:
```bash
./chat_server 9000 &
nc localhost 9000   # terminal 2: escribir "hello"
# terminales 3 y 4 reciben "hello"
```

**Pregunta de reflexión**: cuando haces broadcast a todos los
clientes, ¿qué pasa si `write()` a un cliente bloquea porque su
buffer de envío está lleno? Esto bloquearía el servidor entero
(single-threaded). ¿Cómo lo resolverías? (Pista: piensa en
buffers de salida por cliente + POLLOUT.)

---

### Ejercicio 2: monitor de múltiples logs

Escribe un programa que monitoree múltiples archivos de log
simultáneamente:

1. Recibir N rutas de archivo por argumentos
   (`./logmon /var/log/syslog /tmp/app.log`).
2. Abrir cada archivo, hacer `lseek(fd, 0, SEEK_END)` para ir al
   final.
3. Usar `poll()` para monitorear todos los fds con timeout de
   1 segundo.
4. Cuando hay datos nuevos en un archivo, leer e imprimir con
   prefijo: `[syslog] new log line...`
5. En cada timeout (1s sin actividad), imprimir un punto `.` como
   heartbeat.

Probar:
```bash
# Terminal 1:
./logmon /tmp/test1.log /tmp/test2.log
# Terminal 2:
echo "hello from log1" >> /tmp/test1.log
# Terminal 3:
echo "hello from log2" >> /tmp/test2.log
```

**Pregunta de reflexión**: `poll()` en archivos regulares se
comporta de forma especial: **siempre retorna POLLIN** (un archivo
regular siempre es "legible"). ¿Funciona tu programa? ¿Qué
alternativa usarías para monitorear archivos de verdad? (Pista:
`inotify`.)

---

### Ejercicio 3: port scanner con poll + timeout

Implementa un escáner de puertos TCP usando sockets non-blocking
y `poll()`:

1. Recibir IP y rango de puertos por argumentos
   (`./scanner 127.0.0.1 1 1024`).
2. Para cada puerto:
   - Crear socket con `SOCK_NONBLOCK`.
   - Llamar a `connect()` (retorna `EINPROGRESS`).
   - Añadir al array de pollfds con `events = POLLOUT`.
3. Lanzar conexiones en lotes de 50 (no todas a la vez).
4. Usar `poll()` con timeout de 2 segundos para esperar resultados.
5. Verificar con `getsockopt(SO_ERROR)`:
   - `error == 0` → puerto abierto
   - `error == ECONNREFUSED` → puerto cerrado
   - Timeout → puerto filtrado
6. Imprimir solo los puertos abiertos.

Probar:
```bash
./scanner 127.0.0.1 1 1024
# Debería encontrar puertos como 22 (ssh), 80 (http), etc.
```

**Pregunta de reflexión**: ¿por qué usamos `POLLOUT` y no `POLLIN`
para detectar que un `connect()` non-blocking se completó?
¿Qué indica realmente "listo para escritura" en un socket que
está en proceso de conexión?
