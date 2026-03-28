# socket, bind, listen, accept, connect

## Índice
1. [¿Qué es un socket?](#1-qué-es-un-socket)
2. [Familias de direcciones y tipos](#2-familias-de-direcciones-y-tipos)
3. [El flujo TCP completo](#3-el-flujo-tcp-completo)
4. [socket()](#4-socket)
5. [Estructuras de dirección](#5-estructuras-de-dirección)
6. [Lado servidor: bind, listen, accept](#6-lado-servidor-bind-listen-accept)
7. [Lado cliente: connect](#7-lado-cliente-connect)
8. [Comunicación: read/write vs send/recv](#8-comunicación-readwrite-vs-sendrecv)
9. [Cierre de conexión](#9-cierre-de-conexión)
10. [Servidor completo paso a paso](#10-servidor-completo-paso-a-paso)
11. [Cliente completo paso a paso](#11-cliente-completo-paso-a-paso)
12. [Opciones de socket esenciales](#12-opciones-de-socket-esenciales)
13. [El three-way handshake y el backlog](#13-el-three-way-handshake-y-el-backlog)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. ¿Qué es un socket?

Un socket es un **file descriptor** que representa un extremo de un
canal de comunicación. Así como `open()` devuelve un fd para un
archivo y `pipe()` devuelve fds para un pipe, `socket()` devuelve
un fd para comunicación en red (o local).

```
Proceso A                          Proceso B
┌──────────┐                       ┌──────────┐
│          │    ┌────────────┐     │          │
│  fd = 4 ─┼───▶  Socket    ◀─────┼─ fd = 3  │
│          │    │ (endpoint) │     │          │
│          │    └─────┬──────┘     │          │
└──────────┘          │            └──────────┘
                      │
               ┌──────▼──────┐
               │  Red / IPC  │
               │  (TCP, UDP, │
               │   Unix)     │
               └─────────────┘
```

Al ser un file descriptor, los sockets soportan las operaciones
estándar de fd:
- `read()`/`write()` — leer y escribir datos
- `close()` — cerrar el socket
- `fcntl()` — flags como `O_NONBLOCK`
- `select()`/`poll()`/`epoll` — I/O multiplexado
- `dup2()` — redirección (redirigir stdin/stdout a un socket)

### Socket vs pipe

| Aspecto | Pipe | Socket |
|---------|------|--------|
| Dirección | Unidireccional | Bidireccional (full-duplex) |
| Alcance | Mismo host (padre↔hijo) | Mismo host o red |
| Creación | `pipe()` → 2 fds | `socket()` → 1 fd por extremo |
| Protocolos | N/A | TCP, UDP, Unix, raw... |
| Identificación | Herencia de fd o FIFO | Dirección IP:puerto o path |

---

## 2. Familias de direcciones y tipos

### Familias (dominios)

La familia define **qué espacio de direcciones** usa el socket:

```c
#include <sys/socket.h>

// Las tres familias que usarás en la práctica:
AF_INET      // IPv4 (192.168.1.1:8080)
AF_INET6     // IPv6 ([::1]:8080)
AF_UNIX      // Local (ruta en el filesystem: /tmp/my.sock)
// AF_LOCAL es sinónimo de AF_UNIX
```

### Tipos de socket

El tipo define **la semántica de la comunicación**:

```c
SOCK_STREAM   // Flujo confiable, ordenado, bidireccional (TCP)
SOCK_DGRAM    // Datagramas independientes, sin garantía de orden (UDP)
SOCK_RAW      // Acceso directo al protocolo (requiere privilegios)
```

### Combinaciones comunes

| Familia | Tipo | Protocolo | Uso |
|---------|------|-----------|-----|
| `AF_INET` | `SOCK_STREAM` | TCP | Web, SSH, mail, bases de datos |
| `AF_INET` | `SOCK_DGRAM` | UDP | DNS, streaming, juegos |
| `AF_INET6` | `SOCK_STREAM` | TCP | Lo mismo, pero IPv6 |
| `AF_UNIX` | `SOCK_STREAM` | — | IPC local eficiente (Docker, DBus) |
| `AF_UNIX` | `SOCK_DGRAM` | — | Syslog, journald |

### Flags de tipo (Linux)

Linux permite combinar flags con el tipo usando OR:

```c
// Crear socket TCP que sea close-on-exec y non-blocking de una vez
int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
```

- `SOCK_CLOEXEC`: cierra el fd automáticamente en `exec()` (evita fd leak)
- `SOCK_NONBLOCK`: operaciones no bloquean (retornan `EAGAIN`)

---

## 3. El flujo TCP completo

TCP es un protocolo **orientado a conexión**. Antes de intercambiar
datos, cliente y servidor deben establecer una conexión. El flujo
de llamadas es asimétrico:

```
        SERVIDOR                              CLIENTE
        ────────                              ───────

   1. socket()                           1. socket()
        │                                      │
   2. bind(IP:puerto)                          │
        │                                      │
   3. listen(backlog)                          │
        │                                      │
   4. accept() ──── bloquea ────┐              │
        │                       │         2. connect(IP:puerto)
        │               ◄──────┘              │
        │          conexión establecida        │
        │         (three-way handshake)        │
        ▼                                      ▼
   5. read()/write()  ◄────────────────► 3. read()/write()
        │                                      │
   6. close()                            4. close()
```

### Desglose de responsabilidades

| Paso | Servidor | Cliente |
|------|----------|---------|
| `socket()` | Crear el fd | Crear el fd |
| `bind()` | Asignar dirección:puerto | Opcional (el kernel asigna puerto efímero) |
| `listen()` | Marcar como pasivo (acepta conexiones) | No aplica |
| `accept()` | Extraer conexión de la cola → nuevo fd | No aplica |
| `connect()` | No aplica | Iniciar three-way handshake |
| `read/write` | Comunicar con el cliente | Comunicar con el servidor |
| `close()` | Cerrar la conexión | Cerrar la conexión |

> **Clave**: `accept()` devuelve un **nuevo fd** para cada conexión.
> El socket original (el "listening socket") permanece abierto para
> aceptar más conexiones.

---

## 4. socket()

```c
#include <sys/socket.h>

int socket(int domain, int type, int protocol);
// domain:   AF_INET, AF_INET6, AF_UNIX
// type:     SOCK_STREAM, SOCK_DGRAM (| SOCK_CLOEXEC | SOCK_NONBLOCK)
// protocol: 0 (el kernel elige el protocolo por defecto para domain+type)
//           IPPROTO_TCP, IPPROTO_UDP (explícito)
// Retorna:  fd >= 0 en éxito, -1 en error
```

En la práctica, `protocol = 0` casi siempre es suficiente:
- `AF_INET + SOCK_STREAM + 0` → TCP
- `AF_INET + SOCK_DGRAM + 0` → UDP

```c
// Crear socket TCP/IPv4
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
}
```

El socket recién creado no está conectado a nada. Es solo un fd
con un tipo configurado. Las siguientes llamadas le dan identidad
(bind) y propósito (listen o connect).

---

## 5. Estructuras de dirección

Los sockets usan estructuras para especificar direcciones. La API
es genérica (diseñada en los años 80 antes de que C tuviera
polimorfismo), así que se usa casting:

### La familia de structs

```
struct sockaddr          ← genérica (para prototipos de funciones)
    │
    ├── struct sockaddr_in    ← IPv4
    ├── struct sockaddr_in6   ← IPv6
    ├── struct sockaddr_un    ← Unix domain
    └── struct sockaddr_storage ← suficiente para cualquiera
```

### sockaddr_in (IPv4)

```c
#include <netinet/in.h>

struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET
    in_port_t      sin_port;     // puerto en network byte order
    struct in_addr sin_addr;     // IP en network byte order
    // padding hasta 16 bytes (no usar)
};

struct in_addr {
    uint32_t s_addr;             // IPv4 en network byte order
};
```

### Llenando la estructura

```c
#include <arpa/inet.h>
#include <string.h>

struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));         // ¡siempre inicializar a cero!

addr.sin_family = AF_INET;
addr.sin_port   = htons(8080);          // host-to-network short

// Opción 1: aceptar en todas las interfaces
addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 0.0.0.0

// Opción 2: IP específica
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
// pton = "presentation to network" (texto → binario)
```

### sockaddr_in6 (IPv6)

```c
#include <netinet/in.h>

struct sockaddr_in6 {
    sa_family_t     sin6_family;   // AF_INET6
    in_port_t       sin6_port;     // puerto en network byte order
    uint32_t        sin6_flowinfo; // flow label (generalmente 0)
    struct in6_addr sin6_addr;     // 128-bit IPv6
    uint32_t        sin6_scope_id; // scope ID (link-local)
};

// Uso
struct sockaddr_in6 addr6;
memset(&addr6, 0, sizeof(addr6));
addr6.sin6_family = AF_INET6;
addr6.sin6_port   = htons(8080);
addr6.sin6_addr   = in6addr_any;  // :: (todas las interfaces)
// o: inet_pton(AF_INET6, "::1", &addr6.sin6_addr);
```

### sockaddr_storage (genérica)

Cuando necesitas almacenar una dirección sin saber si será IPv4,
IPv6 o Unix:

```c
struct sockaddr_storage storage;
socklen_t len = sizeof(storage);

// accept() rellena la estructura
int client_fd = accept(server_fd, (struct sockaddr *)&storage, &len);

// Después, inspeccionar la familia
if (storage.ss_family == AF_INET) {
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&storage;
    // usar addr4->sin_port, addr4->sin_addr
} else if (storage.ss_family == AF_INET6) {
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&storage;
    // usar addr6->sin6_port, addr6->sin6_addr
}
```

### El casting obligatorio

Todas las funciones de sockets aceptan `struct sockaddr *` (la forma
genérica). Debes hacer cast desde la estructura específica:

```c
struct sockaddr_in addr;
// ... llenar addr ...

bind(fd, (struct sockaddr *)&addr, sizeof(addr));
//        ^^^^^^^^^^^^^^^^^^^^^^^^^
//        cast obligatorio
```

Esto es un artefacto histórico. No es type-safe, pero es la API
estándar de POSIX.

---

## 6. Lado servidor: bind, listen, accept

### bind() — asignar dirección

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
// Retorna: 0 en éxito, -1 en error
```

`bind()` asocia el socket con una dirección local (IP + puerto):

```c
struct sockaddr_in addr = {
    .sin_family      = AF_INET,
    .sin_port        = htons(8080),
    .sin_addr.s_addr = htonl(INADDR_ANY)
};

if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(server_fd);
    exit(EXIT_FAILURE);
}
```

Errores comunes de `bind()`:

| Error | Causa |
|-------|-------|
| `EADDRINUSE` | El puerto ya está en uso |
| `EACCES` | Puertos < 1024 requieren `CAP_NET_BIND_SERVICE` (o root) |
| `EADDRNOTAVAIL` | La IP no pertenece a esta máquina |

### listen() — marcar como pasivo

```c
int listen(int sockfd, int backlog);
// backlog: tamaño de la cola de conexiones pendientes
// Retorna: 0 en éxito, -1 en error
```

`listen()` transforma el socket de activo (puede conectar) a
**pasivo** (acepta conexiones entrantes):

```c
if (listen(server_fd, 128) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
}
// El servidor ahora acepta conexiones TCP en el puerto 8080
```

El `backlog` define cuántas conexiones completas (three-way handshake
terminado) pueden esperar en la cola antes de que el kernel empiece
a rechazar nuevas conexiones. Valores típicos:

| Contexto | Backlog |
|----------|---------|
| Servidor de desarrollo | 5-10 |
| Servidor de producción | 128-4096 |
| SOMAXCONN | Valor máximo del sistema (normalmente 4096) |

```c
listen(server_fd, SOMAXCONN);  // usar el máximo del sistema
```

> En Linux, si pasas un valor mayor que `SOMAXCONN`, se trunca
> silenciosamente. El valor se puede ajustar con
> `/proc/sys/net/core/somaxconn`.

### accept() — extraer una conexión

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
// addr:    se llena con la dirección del cliente (puede ser NULL)
// addrlen: in/out — tamaño del buffer / tamaño real escrito
// Retorna: nuevo fd para esta conexión, -1 en error
```

`accept()` es **bloqueante** por defecto: el thread se duerme
hasta que un cliente se conecta. Al retornar, devuelve un **nuevo
fd** dedicado a esa conexión:

```c
struct sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);

int client_fd = accept(server_fd,
                        (struct sockaddr *)&client_addr,
                        &client_len);
if (client_fd == -1) {
    perror("accept");
    continue;  // en un loop, seguir aceptando
}

// Imprimir quién se conectó
char ip_str[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
printf("Client connected: %s:%d\n", ip_str, ntohs(client_addr.sin_port));
```

```
Relación entre los file descriptors:

server_fd (listening socket):
  ┌─────────────┐
  │ fd = 3      │ ← permanece abierto, sigue aceptando
  │ :8080       │
  └─────┬───────┘
        │ accept()
        ├──────────▶ client_fd_1 (fd = 4) ← conexión con cliente A
        │ accept()
        ├──────────▶ client_fd_2 (fd = 5) ← conexión con cliente B
        │ accept()
        └──────────▶ client_fd_3 (fd = 6) ← conexión con cliente C
```

### accept4() — Linux extension

```c
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen,
            int flags);
// flags: SOCK_CLOEXEC | SOCK_NONBLOCK
```

Evita la race condition de `accept()` + `fcntl(F_SETFL)` en
programas multi-threaded:

```c
int client_fd = accept4(server_fd,
                         (struct sockaddr *)&client_addr,
                         &client_len,
                         SOCK_CLOEXEC | SOCK_NONBLOCK);
```

---

## 7. Lado cliente: connect()

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
// addr:    dirección del servidor
// Retorna: 0 en éxito, -1 en error
```

`connect()` inicia el three-way handshake TCP con el servidor. Es
**bloqueante** por defecto y puede tardar varios segundos si el
servidor no responde:

```c
int client_fd = socket(AF_INET, SOCK_STREAM, 0);

struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port   = htons(8080)
};
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

if (connect(client_fd, (struct sockaddr *)&server_addr,
            sizeof(server_addr)) == -1) {
    perror("connect");
    close(client_fd);
    exit(EXIT_FAILURE);
}
printf("Connected to server\n");
```

### Errores de connect

| Error | Causa |
|-------|-------|
| `ECONNREFUSED` | Nadie escucha en ese puerto (RST recibido) |
| `ETIMEDOUT` | El servidor no respondió (timeout ~2 min por defecto) |
| `ENETUNREACH` | No hay ruta al host |
| `EINPROGRESS` | Socket non-blocking, conexión en progreso |

### connect() non-blocking

Para evitar que `connect()` bloquee al thread:

```c
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

int client_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

int ret = connect(client_fd, (struct sockaddr *)&server_addr,
                  sizeof(server_addr));
if (ret == -1 && errno != EINPROGRESS) {
    perror("connect");
    exit(EXIT_FAILURE);
}

if (ret == -1) {
    // Conexión en progreso, esperar con poll/epoll
    struct pollfd pfd = { .fd = client_fd, .events = POLLOUT };
    poll(&pfd, 1, 5000);  // timeout 5 segundos

    // Verificar si la conexión tuvo éxito
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
        fprintf(stderr, "connect failed: %s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
}
printf("Connected (non-blocking)\n");
```

---

## 8. Comunicación: read/write vs send/recv

Una vez establecida la conexión, ambos lados pueden leer y escribir.
Hay dos pares de funciones disponibles:

### read()/write() — la forma Unix

```c
ssize_t bytes = write(client_fd, "Hello\n", 6);
char buf[1024];
ssize_t n = read(client_fd, buf, sizeof(buf));
```

Funcionan igual que con archivos. Son portables y simples.

### send()/recv() — la forma sockets

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

Igual que read/write pero con un parámetro `flags`:

| Flag | Efecto |
|------|--------|
| `MSG_DONTWAIT` | No bloquear (como `O_NONBLOCK` pero por operación) |
| `MSG_NOSIGNAL` | No generar `SIGPIPE` si el peer cerró (retorna `EPIPE`) |
| `MSG_PEEK` | Leer datos sin consumirlos del buffer |
| `MSG_WAITALL` | Esperar hasta recibir exactamente `len` bytes |
| `MSG_MORE` | Indica que vienen más datos (agrupa en un paquete TCP) |

> **Recomendación**: usa `send()` con `MSG_NOSIGNAL` en vez de
> `write()` para evitar que `SIGPIPE` mate tu proceso si el cliente
> desconecta inesperadamente.

### TCP no preserva límites de mensajes

**Esto es fundamental**: TCP es un flujo de bytes, **no un flujo
de mensajes**. Si el emisor hace:

```c
send(fd, "Hello", 5, 0);
send(fd, "World", 5, 0);
```

El receptor puede recibir:
```
Posibilidad 1: "HelloWorld"       (un recv)
Posibilidad 2: "Hel" + "loWorld"  (dos recv)
Posibilidad 3: "Hello" + "World"  (dos recv — coincidencia, no garantía)
Posibilidad 4: "H" + "elloW" + "orld"  (tres recv)
```

### Short reads y short writes

`read()` puede retornar **menos bytes** de los solicitados. Debes
hacer un loop hasta recibir todos los datos esperados:

```c
// Leer exactamente n bytes
ssize_t read_all(int fd, void *buf, size_t n) {
    size_t total = 0;
    char *p = buf;

    while (total < n) {
        ssize_t r = read(fd, p + total, n - total);
        if (r == -1) {
            if (errno == EINTR) continue;  // interrumpido por señal
            return -1;
        }
        if (r == 0) return total;  // EOF: peer cerró la conexión
        total += r;
    }
    return total;
}

// Escribir exactamente n bytes
ssize_t write_all(int fd, const void *buf, size_t n) {
    size_t total = 0;
    const char *p = buf;

    while (total < n) {
        ssize_t w = write(fd, p + total, n - total);
        if (w == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += w;
    }
    return total;
}
```

### Protocolos de framing

Para saber dónde termina un mensaje en TCP, necesitas un protocolo
de framing. Las tres opciones más comunes:

```
1. Delimitador (texto):
   "Hello\n"  "World\n"
   └──── \n marca fin de mensaje

2. Prefijo de longitud (binario):
   [0005][Hello][0005][World]
   └──── 4 bytes con el tamaño del payload

3. Longitud fija:
   [Hello_____][World_____]
   └──── todos los mensajes tienen exactamente N bytes
```

Ejemplo con prefijo de longitud (el más robusto):

```c
#include <stdint.h>

// Enviar un mensaje con prefijo de longitud (4 bytes, network order)
int send_message(int fd, const void *data, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (write_all(fd, &net_len, 4) != 4)     return -1;
    if (write_all(fd, data, len) != (ssize_t)len) return -1;
    return 0;
}

// Recibir un mensaje con prefijo de longitud
ssize_t recv_message(int fd, void *buf, size_t bufsize) {
    uint32_t net_len;
    if (read_all(fd, &net_len, 4) != 4) return -1;
    uint32_t len = ntohl(net_len);

    if (len > bufsize) return -1;  // mensaje demasiado grande

    if (read_all(fd, buf, len) != (ssize_t)len) return -1;
    return len;
}
```

---

## 9. Cierre de conexión

### close() — cierre completo

```c
close(client_fd);
// Cierra ambas direcciones (lectura Y escritura)
// Los datos pendientes en el buffer se envían (normalmente)
// Se envía FIN al peer → inicia la secuencia de cierre TCP
```

### shutdown() — cierre parcial

```c
int shutdown(int sockfd, int how);
// how:
//   SHUT_RD    — cerrar lectura (no más recv)
//   SHUT_WR    — cerrar escritura (envía FIN, no más send)
//   SHUT_RDWR  — cerrar ambos
```

`shutdown()` es útil para señalar "terminé de enviar" sin cerrar
el fd (por ejemplo, para seguir leyendo la respuesta):

```c
// Cliente envía datos y luego indica que terminó
write_all(fd, request, req_len);
shutdown(fd, SHUT_WR);  // envía FIN → el servidor ve read() == 0

// Pero sigue leyendo la respuesta del servidor
while ((n = read(fd, buf, sizeof(buf))) > 0) {
    process(buf, n);
}
close(fd);
```

### Diferencia entre close() y shutdown()

```
                close(fd)              shutdown(fd, SHUT_WR)
                ─────────              ────────────────────
 ¿Cierra el fd?     Sí                    No
 ¿Envía FIN?        Sí (si último ref)    Sí (siempre)
 ¿Puedo seguir      No                    Sí (lectura)
   usando el fd?
 ¿Afecta dup'd      No (cuenta de ref)    Sí (todas las copias)
   fds?
```

### La secuencia de cierre TCP (four-way handshake)

```
  Cliente                          Servidor
     │                                │
     │──── FIN ──────────────────────▶│   close() o shutdown(SHUT_WR)
     │                                │
     │◀──── ACK ─────────────────────│   kernel responde
     │                                │
     │    (servidor puede seguir      │
     │     enviando datos)            │
     │                                │
     │◀──── FIN ─────────────────────│   servidor llama close()
     │                                │
     │──── ACK ──────────────────────▶│
     │                                │
  TIME_WAIT                           │
  (2×MSL ≈ 60s)                       │
```

### TIME_WAIT y SO_REUSEADDR

Cuando el servidor cierra una conexión, el socket entra en estado
`TIME_WAIT` durante ~60 segundos. Si reinicias el servidor
inmediatamente, `bind()` falla con `EADDRINUSE` porque el puerto
aún está en TIME_WAIT.

La solución estándar es `SO_REUSEADDR`:

```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
// ANTES de bind()
```

Esto permite reusar el puerto incluso si hay conexiones en
TIME_WAIT. **Casi todo servidor TCP debe usar `SO_REUSEADDR`**.

---

## 10. Servidor completo paso a paso

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BACKLOG 128
#define BUF_SIZE 4096

int main(void) {
    // 1. Crear socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Permitir reusar el puerto
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Bind a todas las interfaces, puerto 8080
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 4. Escuchar
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    // 5. Loop de accept: un cliente a la vez (iterativo)
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                                (struct sockaddr *)&client_addr,
                                &client_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        // Imprimir info del cliente
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        printf("Client connected: %s:%d\n",
               ip_str, ntohs(client_addr.sin_port));

        // 6. Echo: leer y reenviar
        char buf[BUF_SIZE];
        ssize_t n;
        while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
            if (write(client_fd, buf, n) != n) {
                perror("write");
                break;
            }
        }

        if (n == -1)
            perror("read");

        printf("Client disconnected\n");
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
```

```bash
gcc -o echo_server echo_server.c && ./echo_server
```

Probar con netcat:
```bash
# En otra terminal:
echo "Hello, server!" | nc localhost 8080
# O interactivo:
nc localhost 8080
```

### Limitación: servidor iterativo

Este servidor atiende **un cliente a la vez**. Mientras procesa
a un cliente, los demás esperan en la cola de `listen()`. En el
capítulo de I/O multiplexado veremos cómo atender múltiples
clientes concurrentemente.

---

## 11. Cliente completo paso a paso

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8080
#define BUF_SIZE    4096

int main(void) {
    // 1. Crear socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Conectar al servidor
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(SERVER_PORT)
    };
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        perror("connect");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. Enviar un mensaje
    const char *msg = "Hello from client!";
    write(fd, msg, strlen(msg));

    // 4. Leer la respuesta (echo)
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Server replied: %s\n", buf);
    }

    // 5. Cerrar
    close(fd);
    return 0;
}
```

```bash
gcc -o echo_client echo_client.c && ./echo_client
```

---

## 12. Opciones de socket esenciales

Las opciones se configuran con `setsockopt()` y se consultan con
`getsockopt()`:

```c
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname,
               void *optval, socklen_t *optlen);
```

### Opciones que debes conocer

#### SO_REUSEADDR — reusar puerto en TIME_WAIT

```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
// Llamar ANTES de bind()
// Casi obligatorio en todo servidor TCP
```

Sin `SO_REUSEADDR`, reiniciar un servidor da `EADDRINUSE` durante
~60 segundos (el puerto está en TIME_WAIT de la conexión anterior).

#### SO_REUSEPORT — balanceo de carga en kernel

```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
```

Permite que **múltiples procesos** hagan `bind()` al mismo puerto.
El kernel distribuye conexiones entrantes entre ellos. Útil para
servidores multi-proceso (nginx, HAProxy):

```
             ┌──────────────────────┐
             │  Puerto 8080         │
             │  (SO_REUSEPORT)      │
             └────┬─────┬─────┬────┘
                  │     │     │
                  ▼     ▼     ▼
              Worker Worker Worker
              (pid1) (pid2) (pid3)
```

#### TCP_NODELAY — deshabilitar algoritmo de Nagle

```c
int opt = 1;
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
```

El algoritmo de Nagle agrupa pequeños envíos en un solo paquete TCP
para reducir overhead. Esto añade latencia (~200ms en el peor caso).
Desactívalo para aplicaciones sensibles a latencia (juegos en red,
trading, aplicaciones interactivas):

```
Con Nagle (defecto):
  send("H") → espera 200ms → send("ello\n")
  Se envía: [Hello\n] en un paquete (eficiente, pero lento)

Sin Nagle (TCP_NODELAY):
  send("H") → envía inmediatamente [H]
  send("ello\n") → envía inmediatamente [ello\n]
  Dos paquetes (más overhead, pero baja latencia)
```

#### SO_KEEPALIVE — detectar conexiones muertas

```c
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

// Tunear los parámetros (Linux-specific)
int idle    = 60;   // segundos sin tráfico antes de enviar probe
int intvl   = 10;   // segundos entre probes
int cnt     = 5;    // número de probes sin respuesta antes de declarar muerta

setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
```

Sin keepalive, una conexión TCP donde ambos lados están en silencio
puede permanecer "abierta" indefinidamente, incluso si uno de los
lados se cayó (apagón, cable desconectado). Keepalive envía probes
periódicos para detectar esto.

#### SO_LINGER — controlar el comportamiento de close()

```c
struct linger lg = {
    .l_onoff  = 1,   // activar linger
    .l_linger = 5    // segundos máximos esperando enviar datos pendientes
};
setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
```

| l_onoff | l_linger | Comportamiento de close() |
|---------|----------|---------------------------|
| 0 | — | Default: cierra inmediato, datos se envían en background |
| 1 | 0 | **Hard reset**: envía RST, descarta datos pendientes |
| 1 | >0 | close() bloquea hasta N segundos esperando enviar datos |

El caso `l_linger = 0` es útil para cerrar conexiones problemáticas
sin esperar el four-way handshake:

```c
// Cerrar inmediatamente con RST (sin TIME_WAIT)
struct linger lg = { .l_onoff = 1, .l_linger = 0 };
setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
close(fd);  // envía RST, no FIN
```

### Tabla resumen de opciones

```
┌──────────────┬────────────┬──────────────────────────────────┐
│ Opción       │ Nivel      │ Efecto                           │
├──────────────┼────────────┼──────────────────────────────────┤
│ SO_REUSEADDR │ SOL_SOCKET │ Reusar puerto en TIME_WAIT       │
│ SO_REUSEPORT │ SOL_SOCKET │ Compartir puerto entre procesos  │
│ SO_KEEPALIVE │ SOL_SOCKET │ Detectar peer muerto             │
│ SO_LINGER    │ SOL_SOCKET │ Controlar close() blocking/RST   │
│ SO_RCVBUF    │ SOL_SOCKET │ Tamaño buffer de recepción       │
│ SO_SNDBUF    │ SOL_SOCKET │ Tamaño buffer de envío           │
│ TCP_NODELAY  │ IPPROTO_TCP│ Deshabilitar Nagle               │
│ TCP_KEEPIDLE │ IPPROTO_TCP│ Segundos idle antes de probe     │
│ TCP_KEEPINTVL│ IPPROTO_TCP│ Intervalo entre probes           │
│ TCP_KEEPCNT  │ IPPROTO_TCP│ Probes fallidos antes de drop    │
└──────────────┴────────────┴──────────────────────────────────┘
```

---

## 13. El three-way handshake y el backlog

### Three-way handshake

Cuando un cliente llama a `connect()` y el servidor está en
`listen()`, TCP realiza un handshake de tres pasos:

```
  Cliente                              Servidor
     │                                    │
     │──── SYN (seq=x) ─────────────────▶│
     │                                    │ cola SYN (SYN_RECEIVED)
     │◀──── SYN-ACK (seq=y, ack=x+1) ───│
     │                                    │
     │──── ACK (ack=y+1) ───────────────▶│
     │                                    │ cola accept (ESTABLISHED)
     │                                    │
  connect()                          accept()
  retorna                            retorna
```

### Las dos colas del kernel

El kernel mantiene **dos colas** por listening socket:

```
                      ┌────────────────────────┐
  SYN del cliente ──▶ │ SYN queue              │ ← conexiones a medio hacer
                      │ (incomplete)           │   (SYN recibido, esperando ACK)
                      │ Tamaño: tcp_max_syn_   │
                      │         backlog         │
                      └──────────┬─────────────┘
                                 │ ACK del cliente
                                 ▼
                      ┌────────────────────────┐
                      │ Accept queue           │ ← conexiones completas
                      │ (complete)             │   esperando accept()
                      │ Tamaño: backlog param  │
                      │ de listen()            │
                      └──────────┬─────────────┘
                                 │ accept()
                                 ▼
                           nuevo client_fd
```

Cuando la accept queue está llena y llega una nueva conexión
completa, el kernel tiene dos opciones (según
`tcp_abort_on_overflow`):
- **0 (default)**: ignorar el ACK final → el cliente piensa que
  conectó pero el servidor no lo registra → el cliente reintenta
- **1**: enviar RST → el cliente recibe error inmediatamente

### SYN flood y SYN cookies

Un ataque SYN flood envía muchos SYN sin completar el handshake,
llenando la SYN queue. Linux mitiga esto con **SYN cookies**
(`tcp_syncookies = 1`, activado por defecto): cuando la SYN queue
se llena, el kernel codifica el estado en el número de secuencia
del SYN-ACK, eliminando la necesidad de almacenar la conexión
parcial.

---

## 14. Errores comunes

### Error 1: olvidar SO_REUSEADDR

```c
// MAL: al reiniciar el servidor
// "bind: Address already in use"
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
// ... olvidó setsockopt SO_REUSEADDR ...
bind(server_fd, ...);  // EADDRINUSE durante ~60 segundos
```

**Solución**: siempre configurar `SO_REUSEADDR` antes de `bind()`:

```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
bind(server_fd, ...);
```

### Error 2: no manejar short reads

```c
// MAL: asumir que read() devuelve todo el mensaje
char buf[1024];
read(client_fd, buf, sizeof(buf));
// buf puede contener solo una parte del mensaje
process_complete_message(buf);  // ¡datos incompletos!
```

**Solución**: usar `read_all()` o un protocolo de framing con
delimitador o prefijo de longitud, y hacer loop hasta tener un
mensaje completo.

### Error 3: SIGPIPE al escribir en socket cerrado

```c
// MAL: el cliente cerró la conexión, el servidor escribe
write(client_fd, response, resp_len);
// → SIGPIPE → el servidor muere silenciosamente
```

**Solución**: ignorar SIGPIPE y manejar el error:

```c
// Opción A: ignorar SIGPIPE globalmente
signal(SIGPIPE, SIG_IGN);
// Ahora write() retorna -1 con errno = EPIPE

// Opción B: por operación con send()
send(client_fd, response, resp_len, MSG_NOSIGNAL);
// Retorna -1 con errno = EPIPE si el peer cerró
```

### Error 4: no inicializar sockaddr con memset/cero

```c
// MAL: campos basura pueden causar bind() inesperado
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_port   = htons(8080);
// sin_addr y padding tienen valores aleatorios del stack
bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
// Puede fallar o bindear a una IP inesperada
```

**Solución**: inicializar a cero:

```c
// Opción A: memset
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
// ...

// Opción B: designated initializer (preferido en C99+)
struct sockaddr_in addr = {
    .sin_family      = AF_INET,
    .sin_port        = htons(8080),
    .sin_addr.s_addr = htonl(INADDR_ANY)
};
// Los campos no mencionados se inicializan a 0
```

### Error 5: leaks de file descriptors

```c
// MAL: no cerrar client_fd en caso de error
int client_fd = accept(server_fd, NULL, NULL);
if (some_error) {
    return;  // ¡leak! client_fd nunca se cierra
    // Después de 1024 conexiones: "accept: Too many open files"
}
```

**Solución**: siempre cerrar los fds en todos los paths:

```c
int client_fd = accept(server_fd, NULL, NULL);
if (client_fd == -1) continue;

if (process_client(client_fd) == -1) {
    // Cerrar incluso en caso de error
}
close(client_fd);  // siempre
```

O usar el patrón goto-cleanup:

```c
int client_fd = accept(server_fd, NULL, NULL);
if (client_fd == -1) continue;

char *buf = malloc(BUF_SIZE);
if (!buf) goto cleanup;

if (read(client_fd, buf, BUF_SIZE) == -1) goto cleanup;
// ... más procesamiento ...

cleanup:
    free(buf);
    close(client_fd);
```

---

## 15. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                 TCP Socket API                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SERVIDOR:                                                   │
│    fd = socket(AF_INET, SOCK_STREAM, 0);                     │
│    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, 4);   │
│    bind(fd, (struct sockaddr*)&addr, sizeof(addr));           │
│    listen(fd, 128);                                          │
│    client = accept(fd, (struct sockaddr*)&cli, &len);        │
│                                                              │
│  CLIENTE:                                                    │
│    fd = socket(AF_INET, SOCK_STREAM, 0);                     │
│    connect(fd, (struct sockaddr*)&addr, sizeof(addr));        │
│                                                              │
│  COMUNICACIÓN:                                               │
│    n = read(fd, buf, size);   / recv(fd, buf, size, flags);  │
│    n = write(fd, buf, size);  / send(fd, buf, size, flags);  │
│                                                              │
│  CIERRE:                                                     │
│    shutdown(fd, SHUT_WR);   // media-close (envía FIN)       │
│    close(fd);               // cierre completo               │
│                                                              │
│  HEADERS:                                                    │
│    #include <sys/socket.h>  // socket, bind, listen, etc.    │
│    #include <netinet/in.h>  // sockaddr_in, IPPROTO_TCP      │
│    #include <arpa/inet.h>   // inet_pton, inet_ntop, htons   │
│    #include <unistd.h>      // close, read, write            │
│                                                              │
│  DIRECCIÓN IPv4:                                             │
│    struct sockaddr_in addr = {                                │
│        .sin_family      = AF_INET,                           │
│        .sin_port        = htons(PORT),                       │
│        .sin_addr.s_addr = htonl(INADDR_ANY)                  │
│    };                                                        │
│    inet_pton(AF_INET, "1.2.3.4", &addr.sin_addr);           │
│                                                              │
│  CONVERSIONES:                                               │
│    htons()/ntohs()  — 16-bit host↔network                   │
│    htonl()/ntohl()  — 32-bit host↔network                   │
│    inet_pton()      — string → binary IP                     │
│    inet_ntop()      — binary IP → string                     │
│                                                              │
│  ERRORES:                                                    │
│    bind:    EADDRINUSE, EACCES (port<1024)                   │
│    connect: ECONNREFUSED, ETIMEDOUT, EINPROGRESS             │
│    accept:  EMFILE (too many open files)                     │
│    write:   EPIPE, ECONNRESET                                │
│    read:    ECONNRESET (peer sent RST)                       │
│                                                              │
│  REGLAS:                                                     │
│    • Siempre SO_REUSEADDR antes de bind()                    │
│    • Manejar short reads/writes con loops                    │
│    • TCP = flujo de bytes, NO mensajes → usar framing        │
│    • Manejar SIGPIPE: signal(SIGPIPE, SIG_IGN) o MSG_NOSIGNAL│
│    • Cerrar fds en TODOS los paths (error y éxito)           │
└──────────────────────────────────────────────────────────────┘
```

---

## 16. Ejercicios

### Ejercicio 1: servidor de hora

Implementa un servidor TCP que responda con la fecha y hora actual
a cada cliente que se conecte, y luego cierre la conexión:

1. Crear socket, bind al puerto 9000, listen.
2. En el loop de accept, obtener la hora con `time()` + `ctime_r()`.
3. Enviar la cadena de hora al cliente con `write()`.
4. Cerrar `client_fd`.
5. Probar con `nc localhost 9000` — cada conexión debe recibir la
   hora y desconectarse.

Requisitos adicionales:
- Usar `SO_REUSEADDR`.
- Imprimir en el servidor la IP y puerto de cada cliente que se
  conecta (usar `inet_ntop` + `ntohs`).
- Ignorar `SIGPIPE`.

**Pregunta de reflexión**: este servidor cierra la conexión
inmediatamente después de enviar la hora. ¿Necesita un protocolo
de framing? ¿Qué cambiaría si el servidor tuviera que enviar
múltiples líneas antes de cerrar?

---

### Ejercicio 2: chat simple (dos procesos)

Implementa un programa de chat bidireccional entre dos terminales:

1. **Modo servidor** (`./chat -s 9001`): bind, listen, accept un
   cliente.
2. **Modo cliente** (`./chat -c 127.0.0.1 9001`): connect al
   servidor.
3. Una vez conectados, ambos lados alternan entre leer del stdin
   y leer del socket:
   - Usar `fork()`: el hijo lee del socket e imprime, el padre
     lee de stdin y envía al socket.
   - Usar `\n` como delimitador de mensajes.
4. Al recibir EOF en stdin (Ctrl+D), hacer `shutdown(SHUT_WR)` para
   señalar fin de envío, y seguir leyendo del socket hasta que el
   otro lado también cierre.

Probar abriendo dos terminales y escribiendo mensajes de ida y
vuelta.

**Pregunta de reflexión**: esta implementación usa `fork()` para
manejar la lectura y escritura concurrentemente. ¿Qué problemas
tendría si intentaras hacer ambas operaciones en un solo thread
sin fork? (Pista: `read()` de stdin y `read()` del socket son ambas
bloqueantes.)

---

### Ejercicio 3: servidor echo con protocolo de longitud

Implementa un servidor echo que use **prefijo de longitud** para
delimitar mensajes:

1. Definir el protocolo:
   - Cada mensaje va precedido por 4 bytes con su longitud en
     network byte order.
   - Tamaño máximo de mensaje: 65536 bytes.
2. Implementar `send_message(fd, data, len)` y
   `recv_message(fd, buf, bufsize)` con `read_all()`/`write_all()`.
3. El servidor recibe un mensaje y lo devuelve (echo) usando el
   mismo protocolo.
4. Escribir un cliente que envíe 3 mensajes de distintos tamaños
   y verifique que las respuestas coinciden.

Probar que un mensaje de 10000 bytes se recibe completo e íntegro
(no se trunca ni se mezcla con otros mensajes).

**Pregunta de reflexión**: ¿qué pasa si un cliente malicioso envía
un prefijo de longitud de 1 GB? ¿Cómo debería protegerse el
servidor contra este ataque? ¿Es suficiente con un tamaño máximo
o se necesita algo más?
