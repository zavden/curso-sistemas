# select

## Índice
1. [El problema: I/O bloqueante con múltiples fds](#1-el-problema-io-bloqueante-con-múltiples-fds)
2. [¿Qué es I/O multiplexado?](#2-qué-es-io-multiplexado)
3. [API de select](#3-api-de-select)
4. [fd_set y sus macros](#4-fd_set-y-sus-macros)
5. [Timeout](#5-timeout)
6. [Servidor TCP con select](#6-servidor-tcp-con-select)
7. [select con stdin + socket](#7-select-con-stdin--socket)
8. [pselect](#8-pselect)
9. [Limitación FD_SETSIZE](#9-limitación-fd_setsize)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema: I/O bloqueante con múltiples fds

Considera un servidor que atiende a múltiples clientes. Con I/O
bloqueante, cada `read()` detiene el thread hasta que lleguen datos:

```c
// MAL: servidor bloqueante con 2 clientes
read(client1_fd, buf, sizeof(buf));   // bloqueado aquí...
// Mientras tanto, client2 envió datos pero nadie los lee

read(client2_fd, buf, sizeof(buf));   // solo llega aquí cuando
                                       // client1 envíe algo
```

### Las "soluciones" sin multiplexado

| Enfoque | Problema |
|---------|---------|
| Un thread por cliente | Miles de clientes = miles de threads (memoria, context switches) |
| Un proceso por cliente | Aún peor en recursos |
| Non-blocking con busy-wait | `while(read() == EAGAIN)` quema CPU al 100% |

```
Busy-wait (MAL):
  for (;;) {
      read(fd1, ...);  // EAGAIN
      read(fd2, ...);  // EAGAIN
      read(fd3, ...);  // EAGAIN
      // ↻ loop infinito quemando CPU
  }

I/O multiplexado (BIEN):
  select(fds, ...);     // duerme hasta que ALGUNO esté listo
  // Despierta solo cuando hay trabajo que hacer
  if (fd1 listo) read(fd1, ...);
  if (fd2 listo) read(fd2, ...);
```

---

## 2. ¿Qué es I/O multiplexado?

I/O multiplexado permite a un **único thread** monitorear
**múltiples file descriptors** simultáneamente, durmiendo hasta
que al menos uno esté listo para leer o escribir:

```
Un thread, múltiples fds:

                    ┌─────────────┐
  client1_fd ──────▶│             │
  client2_fd ──────▶│   select()  │──▶ "fd 4 y fd 7 están listos"
  client3_fd ──────▶│             │
  listen_fd  ──────▶│   (duerme   │
  stdin_fd   ──────▶│    hasta    │
                    │    alguno   │
                    │    esté     │
                    │    listo)   │
                    └─────────────┘
```

### Las tres generaciones

| Generación | Syscall | Año | Limitaciones |
|------------|---------|-----|-------------|
| 1ª | `select()` | 1983 (4.2BSD) | FD_SETSIZE (1024), O(n) scan |
| 2ª | `poll()` | 1986 (SVR3) | Sin límite fijo, pero O(n) scan |
| 3ª | `epoll()` | 2002 (Linux 2.5) | O(1) por evento, escalable |

`select()` es la más antigua y limitada, pero:
- Es la más portable (POSIX, Windows via Winsock)
- Es suficiente para pocos fds (< 100)
- Su API es la base para entender las demás

---

## 3. API de select

```c
#include <sys/select.h>

int select(int nfds,
           fd_set *readfds,     // fds a monitorear para lectura
           fd_set *writefds,    // fds a monitorear para escritura
           fd_set *exceptfds,   // fds a monitorear para excepciones
           struct timeval *timeout);

// nfds:     valor del fd más alto + 1
// readfds:  input: qué fds monitorear para lectura
//           output: qué fds ESTÁN listos para lectura
// writefds: igual pero para escritura (NULL si no interesa)
// exceptfds: condiciones excepcionales (NULL casi siempre)
// timeout:  NULL = esperar indefinidamente
//           {0,0} = retornar inmediatamente (polling)
//           {s,us} = esperar máximo s segundos + us microsegundos
//
// Retorna:  número total de fds listos
//           0 si timeout expiró
//           -1 en error (errno = EINTR si fue interrumpido por señal)
```

### El flujo de select

```
1. Preparar los fd_set (FD_ZERO + FD_SET para cada fd)
2. Llamar a select() → el thread duerme
3. select() retorna → los fd_set fueron MODIFICADOS
   (solo contienen los fds que están listos)
4. Iterar los fds y procesar los que están listos
5. Volver al paso 1 (¡los fd_set se destruyeron!)
```

> **Clave**: `select()` **modifica** los fd_set que le pasas. Después
> de retornar, solo contienen los fds listos. Por eso debes
> reconstruirlos en cada iteración del loop.

---

## 4. fd_set y sus macros

`fd_set` es un bitmap (array de bits) donde cada bit representa un
file descriptor:

```
fd_set internamente:

Bit: 0  1  2  3  4  5  6  7  8  9 ...  1023
     0  0  0  1  1  0  0  1  0  0 ...  0
              │  │        │
              fd3 fd4     fd7  ← estos tres están en el set
```

### Macros de manipulación

```c
fd_set readfds;

FD_ZERO(&readfds);           // Limpiar todos los bits → vacío
FD_SET(fd, &readfds);        // Añadir fd al set (poner bit a 1)
FD_CLR(fd, &readfds);        // Quitar fd del set (poner bit a 0)
FD_ISSET(fd, &readfds);      // ¿Está fd en el set? (leer el bit)
```

### Ejemplo paso a paso

```c
fd_set readfds;

// Paso 1: vaciar el set
FD_ZERO(&readfds);                    // todos los bits a 0

// Paso 2: añadir los fds que nos interesan
FD_SET(3, &readfds);                  // bit 3 = 1
FD_SET(5, &readfds);                  // bit 5 = 1
FD_SET(7, &readfds);                  // bit 7 = 1

// Paso 3: calcular nfds (fd más alto + 1)
int maxfd = 7;
int nfds = maxfd + 1;                // 8

// Paso 4: llamar a select
int ready = select(nfds, &readfds, NULL, NULL, NULL);

// Paso 5: readfds fue MODIFICADO — solo tiene los fds listos
// Si solo fd 5 tiene datos:
// readfds ahora: bit 3=0, bit 5=1, bit 7=0

if (FD_ISSET(3, &readfds)) read(3, ...);   // no
if (FD_ISSET(5, &readfds)) read(5, ...);   // sí → leer
if (FD_ISSET(7, &readfds)) read(7, ...);   // no
```

### ¿Por qué nfds = max_fd + 1?

`select()` necesita saber cuántos bits del bitmap revisar. Si tu
fd más alto es 7, select revisa los bits 0-7 (8 bits). Esto es una
optimización: evita escanear los 1024 bits si solo usas los
primeros 8.

```c
// Si tienes fds: 3, 5, 7
int maxfd = 7;
select(maxfd + 1, &readfds, ...);
// select revisa bits 0..7, ignora 8..1023
```

---

## 5. Timeout

```c
struct timeval {
    time_t      tv_sec;    // segundos
    suseconds_t tv_usec;   // microsegundos (0-999999)
};
```

### Tres modos de operación

```c
// 1. Espera indefinida (bloqueante)
select(nfds, &readfds, NULL, NULL, NULL);
// Duerme hasta que algún fd esté listo

// 2. Polling (no bloqueante)
struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
select(nfds, &readfds, NULL, NULL, &tv);
// Retorna inmediatamente — ready=0 si ninguno está listo

// 3. Timeout acotado
struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
select(nfds, &readfds, NULL, NULL, &tv);
// Espera hasta 5 segundos
// Si retorna 0 → timeout expiró sin que ningún fd estuviera listo
```

### Timeout se modifica (Linux)

En Linux, `select()` **modifica** el `struct timeval` para reflejar
el tiempo restante. Esto es específico de Linux y no es portable:

```c
struct timeval tv = { .tv_sec = 10 };
select(nfds, &readfds, NULL, NULL, &tv);
// Si select retornó después de 3 segundos:
// tv.tv_sec == 7 (tiempo restante)

// ¡En el siguiente loop, debes reinicializar tv!
```

> **POSIX** dice que el contenido de timeout es indeterminado después
> de select. Siempre reinicializa antes de cada llamada.

---

## 6. Servidor TCP con select

Un servidor que atiende múltiples clientes con un solo thread:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT     8080
#define MAX_FDS  FD_SETSIZE  // 1024
#define BUF_SIZE 4096

int main(void) {
    // Crear y configurar listening socket
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

    // Array de fds de clientes conectados
    int client_fds[MAX_FDS];
    int num_clients = 0;

    for (;;) {
        // --- Paso 1: construir fd_set (se destruye en cada select) ---
        fd_set readfds;
        FD_ZERO(&readfds);

        // Siempre monitorear el listening socket
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;

        // Añadir todos los clientes conectados
        for (int i = 0; i < num_clients; i++) {
            FD_SET(client_fds[i], &readfds);
            if (client_fds[i] > maxfd)
                maxfd = client_fds[i];
        }

        // --- Paso 2: esperar actividad ---
        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready == -1) {
            perror("select");
            break;
        }

        // --- Paso 3: ¿nueva conexión? ---
        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(listen_fd,
                                (struct sockaddr *)&client_addr,
                                &client_len);
            if (new_fd == -1) {
                perror("accept");
            } else if (num_clients >= MAX_FDS - 1) {
                fprintf(stderr, "Too many clients\n");
                close(new_fd);
            } else {
                client_fds[num_clients++] = new_fd;

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          ip, sizeof(ip));
                printf("New client [%d]: %s:%d\n",
                       new_fd, ip, ntohs(client_addr.sin_port));
            }
        }

        // --- Paso 4: ¿datos de clientes existentes? ---
        for (int i = 0; i < num_clients; i++) {
            if (!FD_ISSET(client_fds[i], &readfds))
                continue;

            char buf[BUF_SIZE];
            ssize_t n = read(client_fds[i], buf, sizeof(buf));

            if (n <= 0) {
                // Desconexión o error
                printf("Client [%d] disconnected\n", client_fds[i]);
                close(client_fds[i]);

                // Eliminar del array (swap con el último)
                client_fds[i] = client_fds[--num_clients];
                i--;  // re-revisar esta posición
            } else {
                // Echo: reenviar los datos al cliente
                write(client_fds[i], buf, n);
            }
        }
    }

    close(listen_fd);
    return 0;
}
```

```bash
gcc -o select_server select_server.c && ./select_server

# En varias terminales:
nc localhost 8080   # cliente 1
nc localhost 8080   # cliente 2
nc localhost 8080   # cliente 3
# Escribir en cualquiera → responde sin bloquear los demás
```

### Diagrama del flujo

```
┌─────────────────────────────────────────────────┐
│                  select loop                     │
│                                                  │
│  ┌──────────────────────────────────────┐        │
│  │ FD_ZERO + FD_SET(listen_fd)          │        │
│  │ + FD_SET(cada client_fd)             │        │
│  └──────────────┬───────────────────────┘        │
│                 ▼                                 │
│  ┌──────────────────────────────────────┐        │
│  │ select(maxfd+1, &readfds, ...)       │        │
│  │ ← duerme hasta actividad            │        │
│  └──────────────┬───────────────────────┘        │
│                 ▼                                 │
│  ┌────────────────────┐  ┌───────────────────┐   │
│  │listen_fd listo?     │  │client_fd[i] listo?│   │
│  │→ accept()           │  │→ read()           │   │
│  │  añadir al array    │  │  n>0: echo        │   │
│  │                     │  │  n≤0: close,      │   │
│  │                     │  │       quitar       │   │
│  └────────────────────┘  └───────────────────┘   │
│                 │                                 │
│                 └─── volver al inicio ────────────┘
```

---

## 7. select con stdin + socket

Un uso clásico: leer tanto de stdin (teclado) como de un socket,
sin que ninguno bloquee al otro:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUF_SIZE 4096

int main(void) {
    // Conectar al servidor
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(8080)
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr));

    printf("Connected. Type messages (Ctrl+D to quit):\n");

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);   // fd 0
        FD_SET(sock_fd, &readfds);

        int maxfd = sock_fd;  // sock_fd > 0 siempre
        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        // ¿Datos del teclado?
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buf[BUF_SIZE];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;  // EOF (Ctrl+D)
            write(sock_fd, buf, n);
        }

        // ¿Datos del servidor?
        if (FD_ISSET(sock_fd, &readfds)) {
            char buf[BUF_SIZE];
            ssize_t n = read(sock_fd, buf, sizeof(buf));
            if (n <= 0) {
                printf("Server disconnected\n");
                break;
            }
            write(STDOUT_FILENO, buf, n);
        }
    }

    close(sock_fd);
    return 0;
}
```

Este patrón es la base de clientes de chat, telnet, y cualquier
programa interactivo que comunique con un servidor.

---

## 8. pselect

`pselect()` es una variante de `select()` que resuelve dos
problemas:

```c
#include <sys/select.h>

int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds,
            const struct timespec *timeout,    // ← nanosegundos
            const sigset_t *sigmask);          // ← máscara de señales
```

### Diferencia 1: timeout con nanosegundos

```c
// select: struct timeval (microsegundos)
struct timeval tv = { .tv_sec = 1, .tv_usec = 500000 };  // 1.5s

// pselect: struct timespec (nanosegundos)
struct timespec ts = { .tv_sec = 1, .tv_nsec = 500000000 };  // 1.5s
```

### Diferencia 2: no modifica timeout

A diferencia de `select()` en Linux, `pselect()` **nunca** modifica
el timeout. Es seguro reusar el mismo struct timespec.

### Diferencia 3: máscara de señales atómica

El problema clásico con `select()` y señales:

```c
// MAL: race condition entre sigprocmask y select
sigset_t empty;
sigemptyset(&empty);

sigprocmask(SIG_SETMASK, &empty, NULL);  // desbloquear señales
// ← VENTANA: la señal puede llegar AQUÍ, antes de select
//   Si llega aquí, select() no la ve y se bloquea
select(nfds, &readfds, NULL, NULL, NULL);
// select se bloquea para siempre porque la señal ya se procesó
```

`pselect()` cambia la máscara de señales y entra en espera
**atómicamente**, eliminando la race condition:

```c
// BIEN: pselect cambia máscara y espera de forma atómica
sigset_t empty;
sigemptyset(&empty);

// Esto es atómico: desbloquea señales Y espera fds
pselect(nfds, &readfds, NULL, NULL, NULL, &empty);
// Si llega una señal, pselect retorna con errno = EINTR
```

### Ejemplo: select con manejo de SIGINT

```c
#include <signal.h>
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t got_sigint = 0;

void handler(int sig) {
    (void)sig;
    got_sigint = 1;
}

int main(void) {
    // Instalar handler para SIGINT
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    // Bloquear SIGINT normalmente
    sigset_t block_sigint, empty;
    sigemptyset(&empty);
    sigemptyset(&block_sigint);
    sigaddset(&block_sigint, SIGINT);
    sigprocmask(SIG_BLOCK, &block_sigint, NULL);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // pselect desbloquea SIGINT atómicamente mientras espera
        int ret = pselect(STDIN_FILENO + 1, &readfds, NULL, NULL,
                          NULL, &empty);

        if (ret == -1 && errno == EINTR) {
            if (got_sigint) {
                printf("\nCaught SIGINT, cleaning up...\n");
                break;
            }
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
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

## 9. Limitación FD_SETSIZE

### El problema fundamental

`fd_set` es un bitmap de tamaño fijo, definido en compilación:

```c
// En <sys/select.h> o <bits/typesizes.h>:
#define FD_SETSIZE 1024

// fd_set es esencialmente:
typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;
// En x86-64: 1024 / 64 = 16 unsigned longs = 128 bytes
```

### Las consecuencias

1. **Máximo 1024 fds simultáneos**: si tu proceso tiene el fd 1025
   (posible si abriste muchos archivos/sockets), no puedes usarlo
   con select.

2. **FD_SET con fd >= FD_SETSIZE es buffer overflow**:
   ```c
   // MAL: si fd >= 1024, esto escribe fuera del bitmap
   FD_SET(1500, &readfds);
   // Undefined behavior — corrompe el stack o el heap
   ```

3. **select escanea linealmente** 0 a nfds-1, incluso si solo
   monitoreas 3 fds. Con nfds=1000, revisa 1000 bits aunque solo
   3 estén seteados.

### ¿Puedo cambiar FD_SETSIZE?

Técnicamente, en Linux puedes redefinirlo antes de incluir los
headers:

```c
#define FD_SETSIZE 4096  // antes de cualquier include
#include <sys/select.h>
```

Pero esto:
- No es portable
- Puede romper librerías enlazadas con el tamaño original
- Aumenta el tamaño del bitmap en el stack (4096/8 = 512 bytes)
- No soluciona el problema de rendimiento O(n)

> **Recomendación**: si necesitas más de ~100 fds, usa `poll()` o
> `epoll()` en vez de intentar hackear `FD_SETSIZE`.

### Cuándo select es suficiente

| Escenario | select OK? |
|-----------|-----------|
| Chat server, < 50 clientes | Sí |
| stdin + 1-2 sockets | Sí |
| Servidor web (miles de conexiones) | No → epoll |
| Proxy con muchas conexiones | No → epoll |
| Código portable (Windows + Unix) | Sí (select es el único portable) |

---

## 10. Errores comunes

### Error 1: no reconstruir fd_set en cada iteración

```c
// MAL: fd_set se destruye después de select
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(listen_fd, &readfds);
FD_SET(client_fd, &readfds);

for (;;) {
    select(maxfd + 1, &readfds, NULL, NULL, NULL);
    // readfds fue modificado — solo tiene los fds listos
    // En la siguiente iteración, los fds no listos ya no están en el set
    // → select no los monitorea → nunca los detecta
}
```

**Solución**: reconstruir fd_set antes de cada select:

```c
for (;;) {
    fd_set readfds;                     // ← dentro del loop
    FD_ZERO(&readfds);                  // ← limpiar
    FD_SET(listen_fd, &readfds);        // ← añadir de nuevo
    FD_SET(client_fd, &readfds);        // ← añadir de nuevo

    select(maxfd + 1, &readfds, NULL, NULL, NULL);
    // ...
}
```

### Error 2: calcular mal nfds

```c
// MAL: usar el número de fds en vez del valor máximo + 1
int num_fds = 3;
select(num_fds, &readfds, NULL, NULL, NULL);
// Si los fds son 3, 5, 7 → nfds=3 solo revisa bits 0,1,2
// ¡Nunca detecta actividad en fds 3, 5, 7!
```

**Solución**: nfds = fd más alto + 1:

```c
int maxfd = -1;
for (int i = 0; i < num_clients; i++) {
    FD_SET(client_fds[i], &readfds);
    if (client_fds[i] > maxfd)
        maxfd = client_fds[i];
}
select(maxfd + 1, &readfds, NULL, NULL, NULL);
```

### Error 3: no reinicializar timeout (Linux)

```c
// MAL: en Linux, select modifica tv
struct timeval tv = { .tv_sec = 5 };

for (;;) {
    select(nfds, &readfds, NULL, NULL, &tv);
    // Después de la primera iteración, tv puede ser {0, 0}
    // → las siguientes llamadas son polling (no esperan)
}
```

**Solución**: reinicializar antes de cada select:

```c
for (;;) {
    struct timeval tv = { .tv_sec = 5 };  // ← dentro del loop
    // ...
    select(nfds, &readfds, NULL, NULL, &tv);
}
```

### Error 4: no manejar EINTR

```c
// MAL: select puede retornar -1 por una señal
int ret = select(nfds, &readfds, NULL, NULL, NULL);
if (ret == -1) {
    perror("select failed!");  // puede ser solo SIGCHLD, SIGALRM...
    exit(EXIT_FAILURE);        // ¡salir por nada!
}
```

**Solución**: reintentar si fue interrumpido por señal:

```c
int ret = select(nfds, &readfds, NULL, NULL, &tv);
if (ret == -1) {
    if (errno == EINTR) continue;  // señal benigna, reintentar
    perror("select");
    break;  // error real
}
```

### Error 5: fd >= FD_SETSIZE

```c
// MAL: si un proceso tiene muchos fds abiertos,
// un nuevo accept() puede devolver fd >= 1024
int client_fd = accept(listen_fd, NULL, NULL);
FD_SET(client_fd, &readfds);  // buffer overflow si fd >= 1024
```

**Solución**: verificar antes de añadir:

```c
int client_fd = accept(listen_fd, NULL, NULL);
if (client_fd >= FD_SETSIZE) {
    fprintf(stderr, "fd %d exceeds FD_SETSIZE, rejecting\n", client_fd);
    close(client_fd);
    continue;
}
FD_SET(client_fd, &readfds);
```

O mejor aún, migrar a `poll()` o `epoll()` si tu servidor puede
tener muchos clientes.

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                        select()                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  #include <sys/select.h>                                     │
│                                                              │
│  PREPARAR:                                                   │
│    fd_set readfds;                                           │
│    FD_ZERO(&readfds);              // vaciar                 │
│    FD_SET(fd, &readfds);           // añadir                 │
│    FD_CLR(fd, &readfds);           // quitar                 │
│    FD_ISSET(fd, &readfds);         // consultar              │
│                                                              │
│  LLAMAR:                                                     │
│    int nfds = max_fd + 1;                                    │
│    struct timeval tv = { .tv_sec = 5 };                      │
│    int ready = select(nfds, &readfds, &writefds,             │
│                       &exceptfds, &tv);                      │
│                                                              │
│  RETORNO:                                                    │
│    > 0 → número de fds listos (revisar con FD_ISSET)         │
│    = 0 → timeout expiró                                      │
│    = -1 → error (EINTR = señal, reintentar)                  │
│                                                              │
│  REGLAS CRÍTICAS:                                            │
│    1. fd_set se DESTRUYE después de select → reconstruir     │
│    2. timeout se puede modificar (Linux) → reinicializar     │
│    3. nfds = max_fd + 1, NO número de fds                    │
│    4. fd >= FD_SETSIZE (1024) → buffer overflow              │
│    5. Manejar EINTR (señales)                                │
│                                                              │
│  PATRÓN DE SERVIDOR:                                         │
│    for (;;) {                                                │
│        FD_ZERO + FD_SET(listen_fd) + FD_SET(clients...)      │
│        select(maxfd+1, &readfds, NULL, NULL, NULL)           │
│        if listen_fd listo → accept, añadir cliente           │
│        for cada cliente → if listo → read/write              │
│    }                                                         │
│                                                              │
│  LIMITACIONES:                                               │
│    • Máximo FD_SETSIZE (1024) fds                            │
│    • O(n) scan en cada llamada                               │
│    • fd_set se copia user↔kernel en cada llamada             │
│    → Para >100 fds, usar poll() o epoll()                    │
│                                                              │
│  pselect:                                                    │
│    • Timeout struct timespec (ns) en vez de timeval (µs)     │
│    • No modifica timeout                                     │
│    • Máscara de señales atómica (evita race condition)        │
│    pselect(nfds, r, w, e, &timespec, &sigmask)               │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: servidor echo multi-cliente

Implementa un servidor echo TCP con `select()` que:

1. Escuche en un puerto recibido por argumento
   (`./echo_select 9000`).
2. Acepte hasta 10 clientes simultáneos.
3. Haga echo de lo que reciba cada cliente.
4. Imprima un mensaje cuando un cliente se conecte o desconecte,
   incluyendo su IP y puerto.
5. Use un timeout de 30 segundos en select: si no hay actividad en
   30s, imprimir "Idle..." y seguir esperando.

Probar:
```bash
./echo_select 9000 &
# Conectar 5 clientes con nc en terminales distintas
# Escribir en cualquiera → responde sin bloquear los demás
# Cerrar uno → los demás siguen funcionando
```

**Pregunta de reflexión**: ¿qué pasa si un cliente envía datos muy
lentamente (1 byte cada 10 segundos)? ¿Bloquea a los demás clientes?
¿Por qué no, dado que hay un solo thread? (Pista: piensa en qué
hace `read()` cuando hay al menos 1 byte disponible.)

---

### Ejercicio 2: multiplexor stdin + UDP + timer

Escribe un programa que use `select()` para monitorear tres fuentes
simultáneamente:

1. **stdin**: leer líneas del teclado e imprimirlas con prefijo
   `[STDIN]`.
2. **Socket UDP**: escuchar en puerto 5555, imprimir datagramas
   recibidos con prefijo `[UDP]` e IP del remitente.
3. **Timer**: cada 5 segundos (usando timeout de select), imprimir
   `[TIMER] tick` con la hora actual.

Probar:
```bash
./multiplexer &
# Escribir en el teclado → [STDIN] ...
# Desde otra terminal: echo "hello" | nc -u localhost 5555 → [UDP] ...
# Cada 5 segundos → [TIMER] tick
```

**Pregunta de reflexión**: estás usando el timeout de select como
"timer". ¿Qué pasa con la precisión del timer si una lectura de
stdin o del socket tarda 3 segundos? ¿El tick se retrasa? ¿Cómo
podrías hacer un timer más preciso? (Pista: `timerfd_create`.)

---

### Ejercicio 3: proxy TCP simple

Implementa un proxy TCP que reenvíe datos entre un cliente y un
servidor remoto:

1. Recibir puerto local y dirección:puerto remoto por argumentos
   (`./proxy 9000 127.0.0.1 8080`).
2. Escuchar en el puerto local (9000).
3. Cuando un cliente se conecte, conectar al servidor remoto (8080).
4. Usar `select()` para monitorear ambos fds (cliente y servidor
   remoto) y reenviar datos bidireccionalmente.
5. Cuando cualquiera de los dos cierre, cerrar el otro.

Probar:
```bash
# Terminal 1: servidor real
nc -l 8080
# Terminal 2: proxy
./proxy 9000 127.0.0.1 8080
# Terminal 3: cliente
nc localhost 9000
# Escribir en terminal 3 → aparece en terminal 1 y viceversa
```

Bonus: soportar múltiples conexiones simultáneas (un par de fds
cliente↔remoto por cada conexión, todos monitoreados con el mismo
select).

**Pregunta de reflexión**: este proxy copia datos byte a byte de
un fd a otro. Si el servidor remoto envía datos más rápido de lo
que el cliente los consume, ¿qué pasa? ¿Dónde se acumulan los
datos? ¿Necesitarías monitorear `writefds` además de `readfds`?
¿En qué caso `write()` a un socket podría bloquear?
