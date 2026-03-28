# Socket TCP listener

## Índice
1. [Arquitectura del servidor HTTP](#1-arquitectura-del-servidor-http)
2. [Crear el socket listener](#2-crear-el-socket-listener)
3. [bind: asociar dirección y puerto](#3-bind-asociar-dirección-y-puerto)
4. [listen: habilitar conexiones entrantes](#4-listen-habilitar-conexiones-entrantes)
5. [accept en loop: atender clientes](#5-accept-en-loop-atender-clientes)
6. [Fork por conexión](#6-fork-por-conexión)
7. [Servidor completo mínimo](#7-servidor-completo-mínimo)
8. [Opciones de socket esenciales](#8-opciones-de-socket-esenciales)
9. [Shutdown limpio con señales](#9-shutdown-limpio-con-señales)
10. [Probar con herramientas estándar](#10-probar-con-herramientas-estándar)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Arquitectura del servidor HTTP

Un servidor HTTP es un programa que escucha conexiones TCP en un
puerto (típicamente 80 o 8080), recibe peticiones HTTP, y envía
respuestas. En este capítulo construiremos uno paso a paso.

```
                        Internet / localhost
                              │
                              ▼
                    ┌───────────────────┐
                    │   Socket TCP      │
                    │   puerto 8080     │
                    │   (listen)        │
                    └────────┬──────────┘
                             │ accept()
                             ▼
                    ┌───────────────────┐
                    │  Conexión nueva   │
                    │  (client_fd)      │
                    └────────┬──────────┘
                             │ fork()
                    ┌────────┴──────────┐
                    │                   │
                    ▼                   ▼
            ┌──────────────┐    ┌──────────────┐
            │   Padre      │    │    Hijo       │
            │ close(client)│    │ close(listen) │
            │ → accept()   │    │ read request  │
            │   (siguiente)│    │ send response │
            └──────────────┘    │ close(client) │
                                │ _exit(0)      │
                                └──────────────┘
```

### Capas del proyecto

En este primer tópico nos enfocamos en la capa de red — el socket
TCP. Los tópicos siguientes cubrirán el parseo HTTP y las
respuestas:

```
┌──────────────────────────────────────────┐
│  T04: Servir archivos estáticos          │  ← filesystem
├──────────────────────────────────────────┤
│  T03: Response HTTP                      │  ← formato respuesta
├──────────────────────────────────────────┤
│  T02: Parser HTTP request                │  ← formato petición
├──────────────────────────────────────────┤
│  T01: Socket TCP listener  ← este tópico │  ← red
└──────────────────────────────────────────┘
```

---

## 2. Crear el socket listener

El primer paso es crear un socket TCP IPv4:

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int create_listener(const char *bind_addr, int port) {
    // 1. Crear socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    // SOCK_STREAM = TCP (flujo de bytes, confiable, orientado a conexión)
    // AF_INET     = IPv4

    return sockfd;
}
```

### ¿Por qué SOCK_STREAM?

HTTP es un protocolo sobre TCP. TCP garantiza:
- Entrega ordenada de bytes
- Retransmisión de paquetes perdidos
- Control de flujo y congestión

```
HTTP (aplicación)
  └── TCP (SOCK_STREAM)
        └── IP (AF_INET / AF_INET6)
              └── Ethernet / WiFi
```

UDP (`SOCK_DGRAM`) no garantiza orden ni entrega — no es apto
para HTTP/1.x (aunque HTTP/3 usa QUIC sobre UDP, eso es otra
historia).

---

## 3. bind: asociar dirección y puerto

`bind()` asocia el socket a una dirección IP y puerto específicos:

```c
int create_listener(const char *bind_addr, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    // 2. Configurar dirección
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);      // host to network byte order

    if (strcmp(bind_addr, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;   // escuchar en TODAS las interfaces
    } else {
        if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
            fprintf(stderr, "invalid address: %s\n", bind_addr);
            close(sockfd);
            return -1;
        }
    }

    // 3. Bind
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
```

### INADDR_ANY vs dirección específica

```
INADDR_ANY (0.0.0.0):
  Acepta conexiones en TODAS las interfaces de red:
  ├── 127.0.0.1 (loopback)
  ├── 192.168.1.100 (LAN)
  └── 10.0.0.5 (otra interfaz)

Dirección específica ("127.0.0.1"):
  Solo acepta conexiones en esa interfaz:
  └── 127.0.0.1 (solo loopback, solo local)
```

Para un servidor HTTP de desarrollo, `127.0.0.1` es más seguro.
Para producción, `0.0.0.0` permite acceso desde cualquier red.

### ¿Por qué htons?

Los enteros multibyte se almacenan en diferente orden según la
arquitectura (little-endian vs big-endian). La red usa big-endian
(network byte order). `htons()` convierte el puerto de host order
a network order:

```c
int port = 8080;          // 0x1F90 en hex

// En x86 (little-endian): almacenado como 0x90 0x1F
// La red espera big-endian:                0x1F 0x90

htons(8080);  // → convierte a network byte order
```

---

## 4. listen: habilitar conexiones entrantes

Después de `bind()`, el socket aún no acepta conexiones.
`listen()` lo marca como pasivo (listening):

```c
int create_listener(const char *bind_addr, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // SO_REUSEADDR: permitir reusar el puerto inmediatamente
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    // 4. Listen
    if (listen(sockfd, SOMAXCONN) == -1) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
```

### El argumento backlog

`listen(sockfd, backlog)` — el segundo argumento es el tamaño de
la **cola de conexiones pendientes**. Son conexiones que completaron
el three-way handshake TCP pero que la aplicación aún no recogió
con `accept()`:

```
                                    backlog queue
                                 ┌───┬───┬───┬───┐
Cliente 1 ──SYN──▶               │ 1 │ 2 │ 3 │ 4 │ ← SOMAXCONN (típico 4096)
Cliente 2 ──SYN──▶   three-way   ├───┴───┴───┴───┤
Cliente 3 ──SYN──▶   handshake   │ esperando      │
                     completado   │ accept()       │
                                 └────────────────┘
                                         │
                                    accept()
                                         │
                                         ▼
                                   client_fd
```

- `SOMAXCONN`: constante del sistema, típicamente 4096 en Linux
  moderno (`/proc/sys/net/core/somaxconn`)
- Si la cola está llena, el kernel descarta el SYN del cliente
  (el cliente reintenta)

### ¿Qué valor usar?

```
SOMAXCONN      Para servidores de producción. Usa el máximo
               del sistema.
128            Valor razonable para desarrollo.
1              Solo para pruebas. Cualquier conexión concurrente
               extra es rechazada.
```

---

## 5. accept en loop: atender clientes

`accept()` bloquea hasta que llega una conexión. Retorna un
**nuevo** file descriptor para la conexión con ese cliente
específico:

```c
void serve_forever(int listener_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len;

    for (;;) {
        client_len = sizeof(client_addr);

        int client_fd = accept(listener_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            perror("accept");
            continue;  // no morir por un accept fallido
        }

        // Obtener IP del cliente para logging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        printf("Connection from %s:%d (fd=%d)\n",
               client_ip, client_port, client_fd);

        // Atender al cliente...
        handle_client(client_fd, client_ip);

        close(client_fd);
    }
}
```

### Dos file descriptors, dos roles

```
listener_fd:                      client_fd:
  Escucha nuevas conexiones         Comunica con UN cliente
  Se usa solo con accept()          Se usa con read/write/send/recv
  Uno por servidor                  Uno por conexión
  Vive toda la vida del servidor    Se cierra al terminar la conexión

┌──────────────┐         ┌────────────────┐
│ listener_fd  │─accept──▶│  client_fd #1  │──▶ read/write ──▶ close
│              │─accept──▶│  client_fd #2  │──▶ read/write ──▶ close
│              │─accept──▶│  client_fd #3  │──▶ read/write ──▶ close
│  (nunca se   │         └────────────────┘
│   cierra)    │
└──────────────┘
```

### El problema del servidor secuencial

Con un solo hilo, el servidor atiende **un** cliente a la vez.
Mientras `handle_client()` procesa una petición, nadie llama
`accept()`, y los demás clientes esperan en la cola del backlog:

```
Cliente 1: ──connect──▶ accept ──▶ handle ──▶ close ──┐
                                                       │
Cliente 2: ──connect──▶ (espera en backlog)────────────┼─▶ accept
                                                       │
Cliente 3: ──connect──▶ (espera en backlog)────────────┘
```

Para un servidor HTTP real esto es inaceptable. La solución en
este tópico es fork por conexión.

---

## 6. Fork por conexión

El patrón más simple para concurrencia: el padre acepta la
conexión y hace fork. El hijo atiende al cliente mientras el padre
vuelve a accept:

```c
void serve_forever_fork(int listener_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len;

    // Recolectar zombies automáticamente
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = SA_NOCLDWAIT;  // Linux: no crear zombies
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    for (;;) {
        client_len = sizeof(client_addr);
        int client_fd = accept(listener_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            if (errno == EINTR)
                continue;  // señal interrumpió accept
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // --- Hijo ---
            close(listener_fd);    // el hijo no necesita el listener

            handle_client(client_fd, client_ip);

            close(client_fd);
            _exit(0);
        }

        // --- Padre ---
        close(client_fd);          // el padre no necesita este client_fd
        // → vuelve a accept()
    }
}
```

### ¿Por qué cerrar file descriptors?

Después de `fork()`, tanto padre como hijo tienen copias de
**ambos** fds. Si no cerramos los que no usamos:

```
Sin cerrar:
  Padre tiene: listener_fd, client_fd #1, client_fd #2, ...
  Hijo  tiene: listener_fd, client_fd

  Problema 1: el padre acumula client_fds → leak de fds
  Problema 2: el hijo tiene listener_fd → podría hacer accept
  Problema 3: cuando el hijo cierra client_fd, la conexión
               NO se cierra porque el padre tiene otra copia
               → el cliente no ve EOF

Con cerrar:
  Padre tiene: listener_fd  (cerró client_fd)
  Hijo  tiene: client_fd    (cerró listener_fd)

  Cuando el hijo cierra client_fd → conexión se cierra ✓
  Padre no acumula fds ✓
```

### Diagrama temporal

```
Padre:                         Hijo 1:         Hijo 2:
  │                              │               │
  ├─ accept() → client_fd=4     │               │
  ├─ fork() ─────────────────────┤               │
  ├─ close(4)                    ├─ close(3)     │
  │                              ├─ handle(4)    │
  ├─ accept() → client_fd=5     ├─ read/write   │
  ├─ fork() ──────────────────── │ ──────────────┤
  ├─ close(5)                    ├─ close(4)     ├─ close(3)
  │                              ├─ _exit(0)     ├─ handle(5)
  ├─ accept() ...                │               ├─ read/write
  │                                              ├─ close(5)
  │                                              ├─ _exit(0)
```

### SA_NOCLDWAIT vs SIGCHLD handler

Para evitar zombies en el servidor, hay dos estrategias:

```c
// Opción 1: SA_NOCLDWAIT (Linux)
// Los hijos no se convierten en zombies al terminar
sa.sa_flags = SA_NOCLDWAIT;
sigaction(SIGCHLD, &sa, NULL);

// Opción 2: Handler que recolecta
void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
```

`SA_NOCLDWAIT` es más simple. El handler es necesario si quieres
saber el exit status de los hijos.

---

## 7. Servidor completo mínimo

Un servidor que acepta conexiones y responde con texto plano.
Aún no parsea HTTP — eso viene en T02. Pero responde con una
respuesta HTTP válida mínima:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 8080
#define BACKLOG SOMAXCONN
#define BUFSIZE 4096

// Respuesta HTTP hardcoded (por ahora)
static const char *RESPONSE =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Hello, World!";

void handle_client(int client_fd, const char *client_ip) {
    char buf[BUFSIZE];

    // Leer la petición (por ahora la descartamos)
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    // Log de la primera línea (GET /path HTTP/1.1)
    char *newline = strchr(buf, '\r');
    if (newline) *newline = '\0';
    printf("[%s] %s\n", client_ip, buf);

    // Enviar respuesta
    const char *ptr = RESPONSE;
    size_t remaining = strlen(RESPONSE);

    while (remaining > 0) {
        ssize_t sent = write(client_fd, ptr, remaining);
        if (sent == -1) {
            if (errno == EINTR) continue;
            perror("write");
            return;
        }
        ptr       += sent;
        remaining -= sent;
    }
}

int main(void) {
    // 1. Crear socket
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket");
        exit(1);
    }

    // 2. SO_REUSEADDR
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Bind
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listener);
        exit(1);
    }

    // 4. Listen
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        close(listener);
        exit(1);
    }

    printf("Listening on 0.0.0.0:%d\n", PORT);

    // 5. Evitar zombies
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = SA_NOCLDWAIT;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    // 6. Accept loop
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listener,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  client_ip, sizeof(client_ip));

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(listener);
            handle_client(client_fd, client_ip);
            close(client_fd);
            _exit(0);
        }

        close(client_fd);
    }

    close(listener);
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -Wall -Wextra -o httpd httpd.c
./httpd
# En otra terminal:
curl http://localhost:8080/
# → Hello, World!
```

### ¿Por qué Connection: close?

`Connection: close` indica al cliente que el servidor cerrará la
conexión después de enviar la respuesta. Sin esto, el cliente
podría esperar más datos (HTTP keep-alive) y la conexión quedaría
abierta:

```
Con Connection: close:
  servidor: write(response) → close(fd) → cliente ve EOF → OK

Sin Connection: close (HTTP/1.1 default = keep-alive):
  servidor: write(response) → close(fd) → funciona pero es inesperado
  servidor: write(response) → (no close) → cliente espera más datos → timeout
```

Para nuestro servidor simple, cerramos la conexión después de
cada respuesta. Un servidor de producción mantendría la conexión
abierta para múltiples peticiones (keep-alive).

---

## 8. Opciones de socket esenciales

### SO_REUSEADDR

Sin `SO_REUSEADDR`, si el servidor se reinicia, `bind()` falla
con `EADDRINUSE` durante ~60 segundos (TIME_WAIT del TCP):

```c
int opt = 1;
setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

```
Sin SO_REUSEADDR:
  ./httpd         → OK, listening on :8080
  Ctrl+C
  ./httpd         → bind: Address already in use  (esperar 60s)

Con SO_REUSEADDR:
  ./httpd         → OK, listening on :8080
  Ctrl+C
  ./httpd         → OK, listening on :8080  (inmediato)
```

**Siempre** usa `SO_REUSEADDR` en servidores. No hay razón para
no usarlo en desarrollo, y en producción es prácticamente
obligatorio.

### SO_REUSEPORT

`SO_REUSEPORT` (Linux 3.9+) permite que **múltiples** procesos
hagan bind al mismo puerto. El kernel distribuye las conexiones
entre ellos:

```c
int opt = 1;
setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
```

```
Proceso 1: bind(:8080) + listen + accept → atiende ~50% conexiones
Proceso 2: bind(:8080) + listen + accept → atiende ~50% conexiones
```

Útil para servidores multi-proceso como nginx. Para nuestro
servidor simple no es necesario.

### TCP_NODELAY

Deshabilita el algoritmo de Nagle, que agrupa bytes pequeños
para enviarlos en un solo paquete TCP. Para HTTP, donde queremos
enviar la respuesta lo antes posible:

```c
#include <netinet/tcp.h>

int opt = 1;
setsockopt(client_fd, IPPROTO_TCP, SO_NODELAY, &opt, sizeof(opt));
```

```
Con Nagle (default):
  write("HTTP/1.1 200 OK\r\n")  → el kernel espera ~40ms por si
  write("Content-Type: ...")     → hay más datos antes de enviar
  write("\r\n")
  write("Hello")                 → todo se envía junto (bien)

  Pero si hacemos writes pequeños con delays, Nagle introduce
  latencia innecesaria.

Sin Nagle (TCP_NODELAY):
  Cada write se envía inmediatamente como un paquete TCP.
  Más paquetes, pero menos latencia.
```

Para un servidor HTTP que construye la respuesta completa antes
de enviar, Nagle no causa problemas. `TCP_NODELAY` es más
importante para protocolos interactivos o cuando se hacen muchos
writes pequeños.

---

## 9. Shutdown limpio con señales

El servidor debe poder terminar limpiamente con Ctrl+C (SIGINT)
o `kill` (SIGTERM): cerrar el listener, esperar hijos, y salir.

### Variable de control

```c
static volatile sig_atomic_t running = 1;

void shutdown_handler(int sig) {
    (void)sig;
    running = 0;
}

// Instalar antes del accept loop:
struct sigaction sa_shut;
sa_shut.sa_handler = shutdown_handler;
sa_shut.sa_flags   = 0;  // no SA_RESTART: queremos que accept sea interrumpido
sigemptyset(&sa_shut.sa_mask);
sigaction(SIGINT,  &sa_shut, NULL);
sigaction(SIGTERM, &sa_shut, NULL);
```

### Accept loop con shutdown

```c
while (running) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listener,
                           (struct sockaddr *)&client_addr,
                           &client_len);
    if (client_fd == -1) {
        if (errno == EINTR) {
            // Señal recibida: verificar running
            continue;
        }
        perror("accept");
        continue;
    }

    // ... fork + handle ...
}

// Cleanup
printf("\nShutting down...\n");
close(listener);

// Esperar a todos los hijos (opcional — ya evitamos zombies)
while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
```

### ¿Por qué no SA_RESTART?

Sin `SA_RESTART`, cuando SIGINT interrumpe `accept()`, `accept`
retorna -1 con `errno == EINTR`. Esto nos permite verificar
`running` y salir del loop:

```
Con SA_RESTART:
  accept() bloqueado
  Ctrl+C → SIGINT → handler pone running=0
  accept() se reinicia automáticamente → sigue bloqueado
  → nunca verificamos running → el servidor no termina

Sin SA_RESTART:
  accept() bloqueado
  Ctrl+C → SIGINT → handler pone running=0
  accept() retorna -1, errno=EINTR
  continue → while(running) es false → sale del loop ✓
```

---

## 10. Probar con herramientas estándar

### curl

La herramienta más directa para probar un servidor HTTP:

```bash
# Petición simple
curl http://localhost:8080/

# Ver headers de respuesta
curl -v http://localhost:8080/

# Solo headers (HEAD request)
curl -I http://localhost:8080/

# Enviar datos (POST)
curl -X POST -d "data=hello" http://localhost:8080/

# Con timeout
curl --connect-timeout 5 http://localhost:8080/
```

### nc (netcat)

Para enviar peticiones HTTP manualmente, byte por byte:

```bash
# Enviar una petición HTTP a mano
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

# Modo interactivo
nc localhost 8080
# Escribir: GET / HTTP/1.1<Enter>
# Escribir: Host: localhost<Enter>
# Escribir: <Enter>
# → ver la respuesta
```

### telnet

Similar a nc pero con interfaz más simple:

```bash
telnet localhost 8080
# Escribir la petición HTTP manualmente
```

### Múltiples conexiones concurrentes

```bash
# Con un loop bash
for i in $(seq 1 10); do
    curl -s http://localhost:8080/ &
done
wait

# Con ab (Apache Benchmark)
ab -n 100 -c 10 http://localhost:8080/
# -n 100: 100 peticiones totales
# -c 10:  10 conexiones concurrentes
```

### Verificar que el servidor escucha

```bash
# Ver puertos en uso
ss -tlnp | grep 8080
# t: TCP, l: listening, n: numérico, p: proceso

# Alternativa
netstat -tlnp | grep 8080
```

---

## 11. Errores comunes

### Error 1: olvidar SO_REUSEADDR

```c
// MAL: sin SO_REUSEADDR
bind(sockfd, ...);
// Después de reiniciar el servidor:
// bind: Address already in use
// → esperar 60 segundos o cambiar de puerto
```

**Solución**: siempre activar SO_REUSEADDR antes de bind:

```c
int opt = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
bind(sockfd, ...);
```

### Error 2: no cerrar client_fd en el padre

```c
// MAL: padre no cierra client_fd
if (pid == 0) {
    close(listener);
    handle_client(client_fd, ip);
    close(client_fd);
    _exit(0);
}
// padre: no close(client_fd) → fd leak
// Después de 1024 conexiones: accept: Too many open files
```

**Solución**: el padre cierra client_fd inmediatamente después
de fork:

```c
if (pid == 0) {
    close(listener);
    handle_client(client_fd, ip);
    close(client_fd);
    _exit(0);
}
close(client_fd);  // padre cierra su copia
```

### Error 3: write parcial sin retry loop

```c
// MAL: asumir que write envía todo
write(client_fd, response, strlen(response));
// write puede enviar MENOS bytes de los pedidos (short write)
// Especialmente con respuestas grandes o conexiones lentas
```

**Solución**: loop de write hasta enviar todo:

```c
const char *ptr = response;
size_t remaining = strlen(response);
while (remaining > 0) {
    ssize_t sent = write(client_fd, ptr, remaining);
    if (sent == -1) {
        if (errno == EINTR) continue;
        break;
    }
    ptr       += sent;
    remaining -= sent;
}
```

### Error 4: usar exit() en el hijo en vez de _exit()

```c
// MAL: exit() en el hijo
if (pid == 0) {
    handle_client(client_fd, ip);
    exit(0);  // ejecuta atexit handlers, flush stdio buffers del padre
}
```

**Solución**: siempre `_exit()` en el hijo de un fork:

```c
if (pid == 0) {
    handle_client(client_fd, ip);
    close(client_fd);
    _exit(0);  // no ejecuta atexit, no flush buffers del padre
}
```

`exit()` llama a los atexit handlers registrados por el padre y
hace flush de los buffers de stdio. Si el padre tenía datos
pendientes en stdout, el hijo los duplicaría.

### Error 5: no manejar EINTR en accept

```c
// MAL: accept falla → servidor muere
int client_fd = accept(listener, ...);
if (client_fd == -1) {
    perror("accept");
    exit(1);  // cualquier señal mata el servidor
}
```

**Solución**: EINTR no es un error fatal — simplemente reintentar:

```c
int client_fd = accept(listener, ...);
if (client_fd == -1) {
    if (errno == EINTR)
        continue;  // señal interrumpió, reintentar
    perror("accept");
    continue;  // otros errores: log y continuar
}
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│           Socket TCP Listener — Servidor HTTP                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FLUJO DEL SERVIDOR:                                         │
│    socket(AF_INET, SOCK_STREAM, 0)  crear socket TCP        │
│    setsockopt(SO_REUSEADDR)         reusar puerto            │
│    bind(addr:port)                  asociar dirección         │
│    listen(SOMAXCONN)                habilitar conexiones     │
│    accept() en loop                 recibir clientes         │
│                                                              │
│  FORK POR CONEXIÓN:                                          │
│    accept → fork                                             │
│      hijo:  close(listener) → handle → close(client) → _exit│
│      padre: close(client)   → volver a accept               │
│                                                              │
│  DIRECCIÓN:                                                  │
│    struct sockaddr_in addr;                                  │
│    addr.sin_family = AF_INET                                 │
│    addr.sin_port   = htons(port)                             │
│    addr.sin_addr   = INADDR_ANY                              │
│                                                              │
│  OPCIONES DE SOCKET:                                         │
│    SO_REUSEADDR     reusar puerto sin esperar TIME_WAIT     │
│    SO_REUSEPORT     múltiples procesos en mismo puerto      │
│    TCP_NODELAY      deshabilitar Nagle (baja latencia)       │
│                                                              │
│  EVITAR ZOMBIES:                                             │
│    SA_NOCLDWAIT     hijos no crean zombies                   │
│    SIGCHLD handler  waitpid(-1, WNOHANG) en loop            │
│                                                              │
│  SHUTDOWN LIMPIO:                                            │
│    volatile sig_atomic_t running = 1;                        │
│    SIGINT/SIGTERM → running = 0                              │
│    accept con EINTR → verificar running                      │
│    No SA_RESTART (para que accept sea interrumpido)          │
│                                                              │
│  WRITE COMPLETO:                                             │
│    while (remaining > 0) {                                   │
│        sent = write(fd, ptr, remaining);                     │
│        ptr += sent; remaining -= sent;                       │
│    }                                                         │
│                                                              │
│  PROBAR:                                                     │
│    curl http://localhost:8080/      petición simple           │
│    curl -v ...                     ver headers               │
│    nc localhost 8080               petición manual            │
│    ss -tlnp | grep 8080           verificar listen           │
│    ab -n 100 -c 10 url            benchmark                  │
│                                                              │
│  REGLAS DE ORO:                                              │
│    1. Siempre SO_REUSEADDR antes de bind                    │
│    2. Cerrar fds que no uses después de fork                │
│    3. _exit() en hijos, nunca exit()                        │
│    4. Loop de write para respuestas completas               │
│    5. Manejar EINTR en accept                               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: servidor echo con fork

Implementa un servidor TCP que haga echo (devuelva lo que el
cliente envía), usando fork por conexión.

**Pasos**:
1. Crea el listener socket (socket, SO_REUSEADDR, bind, listen)
2. En el accept loop: fork por cada conexión
3. El hijo lee del client_fd en un loop y escribe lo mismo de vuelta
4. El hijo sale cuando read retorna 0 (cliente cerró conexión)
5. Configura SA_NOCLDWAIT para evitar zombies

**Predicción antes de ejecutar**: si abres dos terminales con
`nc localhost 8080` simultáneamente, ¿ambas pueden enviar datos
al mismo tiempo? ¿Qué pasaría si no hicieras fork (servidor
secuencial)?

> **Pregunta de reflexión**: el padre cierra client_fd y el hijo
> cierra listener_fd después de fork. ¿Qué pasaría si el padre
> **no** cerrara client_fd? El hijo envía datos, cierra su copia,
> y sale — pero ¿el cliente ve EOF? ¿Por qué?

### Ejercicio 2: servidor HTTP con Content-Length dinámico

Modifica el servidor para que la respuesta incluya la IP del
cliente en el body:

**Pasos**:
1. Parte del servidor completo de la sección 7
2. En handle_client, construye un body con `snprintf()`:
   `"Hello from %s:%d\n"`
3. Calcula Content-Length con `strlen()` del body
4. Construye los headers con `snprintf()` en otro buffer
5. Envía headers + body con el loop de write

**Predicción antes de ejecutar**: si usas `curl -v` desde
localhost, ¿qué IP muestra? ¿127.0.0.1 o 0.0.0.0? ¿Por qué?
¿Y si accedes desde otra máquina en la misma red?

> **Pregunta de reflexión**: ¿por qué es importante calcular
> Content-Length correctamente? ¿Qué pasa si envías un
> Content-Length menor que el body real? ¿Y si es mayor?
> Piensa en cómo el cliente (curl, navegador) interpreta cada caso.

### Ejercicio 3: servidor con timeout y límite de conexiones

Implementa un servidor que limite las conexiones simultáneas y
tenga un timeout por conexión:

**Pasos**:
1. Añade un contador de hijos activos (incrementar en fork,
   decrementar en SIGCHLD handler con waitpid)
2. Si `active_children >= MAX_CHILDREN`, no hacer fork: enviar
   un "503 Service Unavailable" directamente en el padre y cerrar
3. Añade un timeout con `alarm(TIMEOUT)` en el hijo, y un handler
   de SIGALRM que llame `_exit(1)`
4. Prueba con `nc localhost 8080` (sin enviar datos) — debe
   desconectar después del timeout

**Predicción antes de ejecutar**: si MAX_CHILDREN=2 y lanzas 3
conexiones con `nc`, ¿qué respuesta recibe la tercera? ¿Qué pasa
con el contador cuando el alarm mata al hijo — se decrementa
correctamente?

> **Pregunta de reflexión**: usar `alarm()` para timeout funciona,
> pero tiene una limitación: solo puedes tener **un** alarm activo
> por proceso. ¿Qué alternativa usarías si necesitaras múltiples
> timers? ¿Cómo implementarías un timeout usando `setsockopt()`
> con `SO_RCVTIMEO` en vez de alarm?
