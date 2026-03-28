# UDP

## Índice
1. [TCP vs UDP: dos filosofías](#1-tcp-vs-udp-dos-filosofías)
2. [Flujo UDP](#2-flujo-udp)
3. [sendto y recvfrom](#3-sendto-y-recvfrom)
4. [Servidor UDP paso a paso](#4-servidor-udp-paso-a-paso)
5. [Cliente UDP paso a paso](#5-cliente-udp-paso-a-paso)
6. [UDP conectado](#6-udp-conectado)
7. [Datagramas: tamaño y fragmentación](#7-datagramas-tamaño-y-fragmentación)
8. [Multicast](#8-multicast)
9. [Broadcast](#9-broadcast)
10. [Fiabilidad sobre UDP](#10-fiabilidad-sobre-udp)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. TCP vs UDP: dos filosofías

TCP y UDP son los dos protocolos de transporte sobre IP. Representan
filosofías opuestas:

```
TCP (SOCK_STREAM):                  UDP (SOCK_DGRAM):
┌─────────────────────┐             ┌─────────────────────┐
│ • Conexión          │             │ • Sin conexión      │
│ • Flujo de bytes    │             │ • Datagramas        │
│ • Entrega ordenada  │             │ • Sin orden         │
│ • Retransmisión     │             │ • Sin retransmisión │
│ • Control de flujo  │             │ • Sin control       │
│ • Congestión        │             │ • Sin congestión    │
│                     │             │                     │
│ "Tubo confiable"    │             │ "Buzón postal"      │
└─────────────────────┘             └─────────────────────┘
```

### Comparación detallada

| Aspecto | TCP | UDP |
|---------|-----|-----|
| Conexión | Sí (three-way handshake) | No |
| Unidad de datos | Flujo de bytes (sin límites) | Datagrama (mensaje completo) |
| Orden garantizado | Sí | No |
| Entrega garantizada | Sí (retransmisión) | No (fire-and-forget) |
| Duplicados | Eliminados | Posibles |
| Control de flujo | Sí (ventana deslizante) | No |
| Control de congestión | Sí | No |
| Overhead por paquete | 20 bytes (header TCP) | 8 bytes (header UDP) |
| Latencia | Mayor (handshake, ACKs, retransmisiones) | Menor (envía inmediatamente) |
| Broadcast/Multicast | No | Sí |

### ¿Cuándo usar UDP?

| Caso | Por qué UDP |
|------|-------------|
| DNS | Consulta+respuesta en 1 datagrama, no merece conexión TCP |
| Streaming de video/audio | Un frame perdido es mejor que esperar retransmisión |
| Juegos en red | La posición del frame anterior ya no importa |
| DHCP | El cliente aún no tiene IP, no puede hacer TCP |
| VoIP (SIP/RTP) | Latencia mínima, pérdida tolerable |
| IoT/sensores | Dispositivos con poca memoria, datos periódicos |
| NTP | Sincronización de tiempo, un paquete basta |

### ¿Cuándo NO usar UDP?

- Transferencia de archivos → TCP (necesitas que llegue todo)
- Web (HTTP) → TCP (o QUIC, que es UDP+fiabilidad propia)
- Bases de datos → TCP (las queries deben ser íntegras)
- Si necesitas fiabilidad y no quieres implementarla tú → TCP

---

## 2. Flujo UDP

A diferencia de TCP, UDP no tiene handshake ni conexión. El flujo
es simétrico y mucho más simple:

```
       SERVIDOR                           CLIENTE
       ────────                           ───────

  1. socket(SOCK_DGRAM)             1. socket(SOCK_DGRAM)
        │                                   │
  2. bind(IP:puerto)                        │ (bind opcional)
        │                                   │
  3. recvfrom() ── bloquea ──┐              │
        │                    │         2. sendto(IP:puerto del servidor)
        │            ◄───────┘              │
        │       datagrama recibido          │
        │                                   │
  4. sendto(dirección del cliente)     3. recvfrom()
        │                                   │
        │       (no hay close de            │
        │        conexión — no hay          │
        │        conexión)                  │
```

### Diferencias clave con TCP

1. **No hay `listen()` ni `accept()`**: el servidor hace `bind()` y
   directamente `recvfrom()`.

2. **No hay conexión**: cada `sendto()` especifica el destino. El
   servidor puede recibir datagramas de cualquier cliente en el
   mismo socket.

3. **Un solo socket para todos**: el servidor usa **un fd** para
   comunicarse con todos los clientes (en TCP necesitas un fd por
   conexión).

4. **Cada `recvfrom()` devuelve un datagrama completo**: a diferencia
   de TCP, UDP preserva los límites de mensaje.

```
TCP (1 fd por cliente):              UDP (1 fd para todos):

server_fd ──▶ client_fd_1            server_fd
             client_fd_2                │
             client_fd_3                ├── datagrama de A
             ...                        ├── datagrama de B
                                        └── datagrama de C
```

---

## 3. sendto y recvfrom

### recvfrom() — recibir un datagrama

```c
#include <sys/socket.h>

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
// buf:      buffer donde se almacena el datagrama
// len:      tamaño del buffer
// flags:    MSG_DONTWAIT, MSG_PEEK, etc. (0 para default)
// src_addr: se llena con la dirección del remitente (puede ser NULL)
// addrlen:  in/out — tamaño del buffer / tamaño real (puede ser NULL)
// Retorna:  número de bytes recibidos, -1 en error
```

Comportamiento importante:
- **Bloqueante** por defecto (espera hasta que llegue un datagrama)
- Cada llamada devuelve **exactamente un datagrama**
- Si `len` es menor que el datagrama, **se trunca** y los bytes
  sobrantes se pierden (no se guardan para la siguiente lectura)
- Si `len` es mayor que el datagrama, se reciben solo los bytes
  del datagrama (sin relleno)

### sendto() — enviar un datagrama

```c
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
// buf:       datos a enviar
// len:       tamaño de los datos
// flags:     MSG_DONTWAIT, MSG_MORE, etc. (0 para default)
// dest_addr: dirección de destino
// addrlen:   tamaño de dest_addr
// Retorna:   número de bytes enviados (siempre == len o error), -1 en error
```

A diferencia de TCP donde `write()` puede hacer short write, con
UDP `sendto()` envía el datagrama **completo** o falla. No hay
short writes.

### Ejemplo mínimo

```c
// Servidor: recibir y responder
struct sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);
char buf[1024];

ssize_t n = recvfrom(server_fd, buf, sizeof(buf), 0,
                     (struct sockaddr *)&client_addr, &client_len);
// n = bytes recibidos
// client_addr = dirección del remitente

// Responder al mismo remitente
sendto(server_fd, buf, n, 0,
       (struct sockaddr *)&client_addr, client_len);
```

### MSG_TRUNC — detectar truncamiento

En Linux, puedes usar `MSG_TRUNC` con `recvfrom()` para detectar si
un datagrama fue truncado:

```c
// recv con MSG_TRUNC retorna el tamaño REAL del datagrama,
// incluso si fue mayor que el buffer
ssize_t real_len = recvfrom(fd, buf, sizeof(buf), MSG_TRUNC,
                            NULL, NULL);
if ((size_t)real_len > sizeof(buf)) {
    fprintf(stderr, "Datagram truncated: %zd bytes, buffer is %zu\n",
            real_len, sizeof(buf));
}
```

> **Nota**: `MSG_TRUNC` con este comportamiento es **Linux-specific**.
> POSIX define `MSG_TRUNC` solo como flag informativo en `msghdr`.

---

## 4. Servidor UDP paso a paso

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT     9000
#define BUF_SIZE 1024

int main(void) {
    // 1. Crear socket UDP
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind al puerto
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("UDP server listening on port %d\n", PORT);

    // 3. Loop: recibir datagramas y responder (echo)
    char buf[BUF_SIZE];
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&client_addr,
                             &client_len);
        if (n == -1) {
            perror("recvfrom");
            continue;
        }

        // Imprimir info del remitente
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        printf("Received %zd bytes from %s:%d\n",
               n, ip_str, ntohs(client_addr.sin_port));

        // Echo: responder al remitente
        sendto(fd, buf, n, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(fd);
    return 0;
}
```

```bash
gcc -o udp_echo_server udp_echo_server.c && ./udp_echo_server
```

Probar con netcat en modo UDP:
```bash
echo "Hello UDP" | nc -u localhost 9000
# O interactivo:
nc -u localhost 9000
```

### Observaciones

- No hay `listen()` ni `accept()` — el servidor va directo a `recvfrom()`.
- Un solo fd sirve a todos los clientes.
- `SO_REUSEADDR` no es estrictamente necesario (no hay TIME_WAIT en
  UDP), pero sigue siendo buena práctica para `SO_REUSEPORT`.
- El servidor es naturalmente "concurrente" en el sentido de que
  puede recibir datagramas de cualquier cliente sin bloquear a otros.

---

## 5. Cliente UDP paso a paso

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000
#define BUF_SIZE    1024

int main(void) {
    // 1. Crear socket UDP
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Dirección del servidor
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(SERVER_PORT)
    };
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 3. Enviar datagrama (no se necesita connect)
    const char *msg = "Hello UDP server!";
    sendto(fd, msg, strlen(msg), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("Sent: %s\n", msg);

    // 4. Recibir respuesta
    char buf[BUF_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&from_addr, &from_len);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received: %s\n", buf);
    }

    close(fd);
    return 0;
}
```

```bash
gcc -o udp_echo_client udp_echo_client.c && ./udp_echo_client
```

### ¿El cliente necesita bind?

No. Si el cliente no llama a `bind()`, el kernel asigna
automáticamente un **puerto efímero** (generalmente 32768-60999)
la primera vez que llama a `sendto()`. El servidor ve ese puerto
en la dirección del remitente y puede responder.

```
Cliente sin bind:
  sendto() → kernel asigna 0.0.0.0:49152 automáticamente
  El servidor responde a 192.168.1.5:49152
```

---

## 6. UDP conectado

Aunque UDP es "sin conexión", puedes llamar a `connect()` en un
socket UDP. Esto **no** establece una conexión real (no hay
handshake). Solo le dice al kernel: "todos mis envíos van a esta
dirección":

```c
int fd = socket(AF_INET, SOCK_DGRAM, 0);

struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port   = htons(9000)
};
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

// "Conectar" el socket UDP (solo fija la dirección destino)
connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

// Ahora puedes usar send()/recv() en vez de sendto()/recvfrom()
send(fd, "Hello", 5, 0);       // no necesita dirección
char buf[1024];
ssize_t n = recv(fd, buf, sizeof(buf), 0);  // no necesita dirección
```

### Ventajas de UDP conectado

| Ventaja | Detalle |
|---------|---------|
| Sintaxis más simple | `send()`/`recv()` sin repetir la dirección |
| Errores ICMP | Recibes `ECONNREFUSED` si el destino no tiene el puerto abierto |
| Eficiencia | El kernel cachea la ruta, ~30% más rápido en algunos benchmarks |
| Filtrado | El kernel descarta datagramas de otras direcciones |

### Errores ICMP: la gran diferencia

Sin `connect()`, si envías un datagrama a un puerto donde nadie
escucha, el host remoto responde con un paquete ICMP "Port
Unreachable", pero **el kernel no puede asociarlo** con tu socket
(porque el socket no está "conectado" a nadie). El error se pierde:

```
Sin connect():
  sendto(fd, ..., server_addr) → datagrama enviado
  ICMP Port Unreachable llega → kernel descarta (¿a quién se lo doy?)
  recvfrom(fd, ...) → se queda bloqueado para siempre

Con connect():
  send(fd, ...) → datagrama enviado
  ICMP Port Unreachable llega → kernel asocia con este fd
  recv(fd, ...) → retorna -1, errno = ECONNREFUSED
```

### Cambiar el destino

Puedes "reconectar" un socket UDP a otra dirección llamando a
`connect()` de nuevo. Para "desconectar", conecta con
`AF_UNSPEC`:

```c
// Cambiar destino
struct sockaddr_in new_addr = { ... };
connect(fd, (struct sockaddr *)&new_addr, sizeof(new_addr));

// Desconectar (volver a sendto/recvfrom)
struct sockaddr_in unspec = { .sin_family = AF_UNSPEC };
connect(fd, (struct sockaddr *)&unspec, sizeof(unspec));
```

---

## 7. Datagramas: tamaño y fragmentación

### Tamaño máximo de un datagrama UDP

```
Capa          Límite                  Espacio para datos
───────────────────────────────────────────────────────
IP            65535 bytes total       65535 - 20 (IP hdr) = 65515
UDP           65535 bytes total       65535 - 8 (UDP hdr) = 65527
IP + UDP                              65535 - 20 - 8 = 65507 bytes
```

Así que el máximo teórico de datos en un datagrama UDP es
**65507 bytes**. Pero en la práctica hay límites más relevantes.

### MTU y fragmentación

El **MTU** (Maximum Transmission Unit) es el tamaño máximo de
paquete que la red puede transmitir sin fragmentar:

```
┌─────────────────────────────────────────────┐
│ Ethernet MTU = 1500 bytes                   │
│                                             │
│ IP header:  20 bytes                        │
│ UDP header:  8 bytes                        │
│ Datos:    1472 bytes máx sin fragmentar     │
│                                             │
│ 20 + 8 + 1472 = 1500 ✓                     │
└─────────────────────────────────────────────┘
```

Si envías un datagrama mayor que el MTU, IP lo **fragmenta** en
múltiples paquetes. El receptor los reensambla:

```
Envío: datagrama de 3000 bytes (MTU = 1500)

Fragmento 1: [IP hdr | UDP hdr | 1472 bytes datos]  = 1500 bytes
Fragmento 2: [IP hdr |           1480 bytes datos]  = 1500 bytes
Fragmento 3: [IP hdr |             48 bytes datos]  =   68 bytes

                    3000 = 1472 + 1480 + 48
```

### Problemas de la fragmentación

| Problema | Detalle |
|----------|---------|
| Si **un** fragmento se pierde, se pierde **todo** el datagrama | UDP no retransmite |
| Los firewalls pueden bloquear fragmentos | Algunos NATs no reensamblan bien |
| Mayor uso de CPU | Reensamblaje en el receptor |
| Ataques de fragmentación | Teardrop, fragment overlap |

### Recomendación práctica

```
Tamaño seguro para datagramas:

  Internet general:  ≤ 512 bytes (DNS históricamente)
  LAN Ethernet:      ≤ 1472 bytes (1500 MTU - 28 headers)
  Con Path MTU Disc: ≤ PMTU - 28

  Regla simple: mantén tus datagramas < 1400 bytes
```

### Path MTU Discovery

Para descubrir el MTU real del camino entre origen y destino:

```c
// En Linux, activar Path MTU Discovery
int opt = IP_PMTUDISC_DO;
setsockopt(fd, IPPROTO_IP, IP_MTU_DISCOVER, &opt, sizeof(opt));

// Ahora, si envías un datagrama mayor que el PMTU:
// sendto() → -1, errno = EMSGSIZE
// Puedes consultar el PMTU descubierto:
int mtu;
socklen_t len = sizeof(mtu);
getsockopt(fd, IPPROTO_IP, IP_MTU, &mtu, &len);
printf("Path MTU: %d\n", mtu);  // ej: 1500
```

Con `IP_PMTUDISC_DO`, el kernel pone el flag `DF` (Don't Fragment)
en los paquetes IP. Si un router en el camino no puede pasar el
paquete por su MTU, envía un ICMP "Fragmentation Needed" de vuelta,
y el kernel ajusta el PMTU.

---

## 8. Multicast

Multicast permite enviar **un solo datagrama** que llega a
**múltiples receptores** simultáneamente. A diferencia de broadcast
(todos los hosts de la subred), multicast envía solo a los hosts
que se han **suscrito** a un grupo:

```
Unicast:                        Multicast:
  A ──▶ B                        A ──▶ [grupo 239.1.1.1]
  A ──▶ C                              ├── B (suscrito)
  A ──▶ D                              ├── C (suscrito)
  (3 envíos)                           └── D (suscrito)
                                    (1 envío, la red lo replica)
```

### Direcciones multicast

IPv4 multicast usa el rango **224.0.0.0 — 239.255.255.255**
(clase D):

| Rango | Uso |
|-------|-----|
| 224.0.0.0/24 | Local (no sale del segmento de red) |
| 224.0.1.0/24 | Control de Internet |
| 239.0.0.0/8 | Administrativamente scoped (privado, similar a 192.168.x.x) |

### Receptor multicast

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MCAST_GROUP "239.1.1.1"
#define MCAST_PORT  5000

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Permitir múltiples receptores en la misma máquina
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind al puerto multicast
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(MCAST_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));

    // Unirse al grupo multicast
    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = inet_addr(MCAST_GROUP),
        .imr_interface.s_addr = htonl(INADDR_ANY)  // cualquier interfaz
    };
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) == -1) {
        perror("IP_ADD_MEMBERSHIP");
        exit(EXIT_FAILURE);
    }
    printf("Joined multicast group %s:%d\n", MCAST_GROUP, MCAST_PORT);

    // Recibir datagramas
    char buf[1024];
    for (;;) {
        ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("Received: %s\n", buf);
    }

    // Salir del grupo (opcional, close() lo hace automáticamente)
    setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(fd);
    return 0;
}
```

### Emisor multicast

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MCAST_GROUP "239.1.1.1"
#define MCAST_PORT  5000

int main(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Configurar TTL (cuántos routers puede cruzar)
    int ttl = 1;  // 1 = solo la LAN local
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    // Desactivar loopback si no quieres recibir tus propios mensajes
    int loop = 0;
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port   = htons(MCAST_PORT)
    };
    inet_pton(AF_INET, MCAST_GROUP, &dest.sin_addr);

    // Enviar
    const char *msg = "Hello multicast!";
    sendto(fd, msg, strlen(msg), 0,
           (struct sockaddr *)&dest, sizeof(dest));
    printf("Sent to %s:%d\n", MCAST_GROUP, MCAST_PORT);

    close(fd);
    return 0;
}
```

### IGMP

Cuando un host se une a un grupo multicast (`IP_ADD_MEMBERSHIP`),
envía un mensaje **IGMP** (Internet Group Management Protocol) al
router local. El router empieza a reenviar tráfico multicast de
ese grupo a ese segmento de red. Cuando el último host abandona
el grupo, el router deja de reenviar.

```
Host A                   Router               Red
  │                        │                    │
  │── IGMP Join ──────────▶│                    │
  │   (239.1.1.1)          │                    │
  │                        │── forward ────────▶│
  │                        │   multicast        │
  │◀── multicast data ────│◀── multicast data──│
  │                        │                    │
  │── IGMP Leave ─────────▶│                    │
  │                        │── stop forwarding ─│
```

---

## 9. Broadcast

Broadcast envía un datagrama a **todos** los hosts de una subred.
Es más simple que multicast pero menos preciso (todos reciben,
no solo los interesados):

```c
int fd = socket(AF_INET, SOCK_DGRAM, 0);

// Habilitar broadcast (obligatorio, deshabilitado por defecto)
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

struct sockaddr_in bcast_addr = {
    .sin_family      = AF_INET,
    .sin_port        = htons(5000),
    .sin_addr.s_addr = htonl(INADDR_BROADCAST)  // 255.255.255.255
};

sendto(fd, "Hello everyone!", 15, 0,
       (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
```

### Tipos de broadcast

| Dirección | Alcance |
|-----------|---------|
| 255.255.255.255 | Limited broadcast — solo la subred local |
| 192.168.1.255 | Directed broadcast — subred 192.168.1.0/24 |

### Broadcast vs multicast

| Aspecto | Broadcast | Multicast |
|---------|-----------|-----------|
| Alcance | Toda la subred | Solo los suscriptores |
| Cruza routers | No (limited) | Sí (con IGMP/PIM) |
| IPv6 | No existe | Sí |
| Procesamiento | Todos los hosts procesan | Solo los interesados |
| Uso | Discovery (DHCP, ARP) | Streaming, replicación |

> **Nota**: IPv6 elimina broadcast por completo. Todo se hace
> con multicast (ff02::1 para "all nodes", equivalente a broadcast).

---

## 10. Fiabilidad sobre UDP

Cuando necesitas UDP (por latencia, multicast, simplicidad) pero
también necesitas **algo** de fiabilidad, puedes construir tu propio
mecanismo encima de UDP. Varios protocolos hacen esto:

### QUIC (HTTP/3)

Google diseñó QUIC como "TCP reimplementado sobre UDP" con mejoras:

```
TCP+TLS:                            QUIC (sobre UDP):
┌──────────┐ ┌──────────┐          ┌─────────────────────┐
│   TLS    │ │   TCP    │          │        QUIC         │
│ handshake│+│ handshake│          │  (1 RTT o 0-RTT     │
│ (1-2 RTT)│ │ (1 RTT)  │          │   handshake)        │
└──────────┘ └──────────┘          │                     │
  Total: 2-3 RTT                   │  Multiplexing sin   │
                                    │  head-of-line block │
                                    └─────────────────────┘
                                      Total: 1 RTT (o 0-RTT)
```

### Implementación básica: ACK con timeout

Un esquema simple de fiabilidad con retransmisión:

```c
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

// Header de nuestro protocolo
typedef struct {
    uint32_t seq;      // número de secuencia
    uint16_t len;      // longitud del payload
    uint8_t  type;     // 0=DATA, 1=ACK
    uint8_t  padding;
} __attribute__((packed)) msg_header_t;

#define MAX_RETRIES 5
#define TIMEOUT_MS  500

// Enviar con retransmisión
int reliable_send(int fd, const void *data, size_t len,
                  uint32_t seq,
                  const struct sockaddr *dest, socklen_t dest_len) {
    // Construir mensaje
    char packet[sizeof(msg_header_t) + len];
    msg_header_t *hdr = (msg_header_t *)packet;
    hdr->seq  = htonl(seq);
    hdr->len  = htons(len);
    hdr->type = 0;  // DATA
    memcpy(packet + sizeof(msg_header_t), data, len);

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        // Enviar
        sendto(fd, packet, sizeof(msg_header_t) + len, 0,
               dest, dest_len);

        // Esperar ACK con timeout
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, TIMEOUT_MS);

        if (ret > 0) {
            char ack_buf[sizeof(msg_header_t)];
            ssize_t n = recvfrom(fd, ack_buf, sizeof(ack_buf), 0,
                                 NULL, NULL);
            if (n == sizeof(msg_header_t)) {
                msg_header_t *ack = (msg_header_t *)ack_buf;
                if (ack->type == 1 && ntohl(ack->seq) == seq)
                    return 0;  // ACK recibido, éxito
            }
        }
        // Timeout o ACK incorrecto: reintentar
    }
    return -1;  // fallo después de MAX_RETRIES intentos
}
```

### Otros protocolos sobre UDP

| Protocolo | Qué agrega sobre UDP |
|-----------|---------------------|
| QUIC | Fiabilidad completa, cifrado, multiplexing |
| DTLS | TLS para datagramas (VPN, WebRTC) |
| RTP | Timestamps, números de secuencia (streaming) |
| TFTP | ACK simple (transferencia de archivos) |
| KCP | Retransmisión rápida, ventana deslizante |
| ENet | Fiabilidad, ordering, fragmentación (juegos) |

### ¿Por qué no siempre TCP?

Si estas capas sobre UDP reimplementan lo que TCP ya tiene,
¿por qué no usar TCP directamente?

1. **Head-of-line blocking**: en TCP, si se pierde un paquete,
   todos los paquetes posteriores esperan (incluso si son de
   streams independientes). QUIC soluciona esto.

2. **Flexibilidad de congestión**: TCP está en el kernel, no puedes
   cambiar su algoritmo de congestión fácilmente. UDP en userspace
   da control total.

3. **0-RTT**: QUIC puede enviar datos en el primer paquete
   (reconexión rápida). TCP+TLS necesita 2-3 RTTs.

4. **Multipath**: protocolos sobre UDP pueden usar múltiples rutas
   simultáneamente.

---

## 11. Errores comunes

### Error 1: usar read()/write() sin connect() en UDP

```c
// MAL: write sin connect en socket UDP
int fd = socket(AF_INET, SOCK_DGRAM, 0);
write(fd, "Hello", 5);
// → -1, errno = EDESTADDRREQ ("Destination address required")
```

**Solución**: usar `sendto()` con dirección, o `connect()` primero:

```c
// Opción A: sendto() con dirección explícita
sendto(fd, "Hello", 5, 0, (struct sockaddr *)&dest, sizeof(dest));

// Opción B: connect() + send()
connect(fd, (struct sockaddr *)&dest, sizeof(dest));
send(fd, "Hello", 5, 0);
```

### Error 2: asumir que UDP entrega los datagramas

```c
// MAL: enviar sin verificar recepción
for (int i = 0; i < 1000; i++) {
    sendto(fd, &data[i], sizeof(data[i]), 0,
           (struct sockaddr *)&dest, sizeof(dest));
}
// El receptor puede haber recibido 990, 950, o 0 de los 1000
```

UDP no garantiza entrega. Los datagramas pueden:
- **Perderse** (congestión, buffer lleno)
- **Llegar desordenados** (ruta diferente)
- **Duplicarse** (retransmisión a nivel de red)

**Solución**: si necesitas fiabilidad, implementa ACKs y
retransmisión, o usa TCP.

### Error 3: buffer de recvfrom demasiado pequeño

```c
// MAL: buffer menor que el datagrama esperado
char buf[64];
ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
// Si el datagrama tiene 200 bytes:
// n = 64 (solo 64 bytes en buf)
// Los otros 136 bytes se PIERDEN (no se guardan para el siguiente recv)
```

A diferencia de TCP (donde los bytes sobrantes se quedan en el
buffer del kernel), en UDP el datagrama truncado **se descarta
parcialmente**.

**Solución**: usar un buffer suficientemente grande:

```c
// BIEN: buffer del tamaño máximo esperado
char buf[65507];  // máximo teórico UDP
// O al menos el MTU:
char buf[1500];   // suficiente para datagramas no fragmentados
```

### Error 4: no poner timeout en recvfrom del cliente

```c
// MAL: el cliente espera respuesta indefinidamente
sendto(fd, request, req_len, 0, ...);
recvfrom(fd, response, sizeof(response), 0, ...);
// Si el servidor no responde (o el datagrama se perdió):
// → bloqueado para siempre
```

**Solución**: usar timeout con `poll()` o `SO_RCVTIMEO`:

```c
// Opción A: poll()
sendto(fd, request, req_len, 0, ...);
struct pollfd pfd = { .fd = fd, .events = POLLIN };
if (poll(&pfd, 1, 3000) <= 0) {  // 3 segundos
    fprintf(stderr, "Timeout waiting for response\n");
    // reintentar o fallar
}
recvfrom(fd, response, sizeof(response), 0, ...);

// Opción B: SO_RCVTIMEO
struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
// Ahora recvfrom() retorna -1 con errno = EAGAIN/EWOULDBLOCK después de 3s
```

### Error 5: olvidar SO_BROADCAST para enviar broadcast

```c
// MAL: enviar a dirección de broadcast sin SO_BROADCAST
struct sockaddr_in bcast = {
    .sin_family      = AF_INET,
    .sin_port        = htons(5000),
    .sin_addr.s_addr = htonl(INADDR_BROADCAST)
};
sendto(fd, data, len, 0, (struct sockaddr *)&bcast, sizeof(bcast));
// → -1, errno = EACCES
```

**Solución**: activar `SO_BROADCAST` antes de enviar:

```c
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
// Ahora sendto a dirección broadcast funciona
```

El kernel exige este flag como protección contra envíos de
broadcast accidentales.

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                     UDP Socket API                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CREAR:                                                      │
│    fd = socket(AF_INET, SOCK_DGRAM, 0);                      │
│                                                              │
│  BIND (servidor o si necesitas puerto fijo):                  │
│    bind(fd, (struct sockaddr*)&addr, sizeof(addr));           │
│                                                              │
│  ENVIAR/RECIBIR (sin connect):                               │
│    sendto(fd, buf, len, 0, (struct sockaddr*)&dest, dlen);   │
│    recvfrom(fd, buf, len, 0, (struct sockaddr*)&src, &slen); │
│                                                              │
│  CONNECT (opcional, fija destino):                           │
│    connect(fd, (struct sockaddr*)&dest, sizeof(dest));        │
│    send(fd, buf, len, 0);    // sin dirección                │
│    recv(fd, buf, len, 0);    // sin dirección                │
│                                                              │
│  MULTICAST:                                                  │
│    struct ip_mreq mreq = { inet_addr("239.x.x.x"),          │
│                            htonl(INADDR_ANY) };              │
│    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, ...);       │
│    setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, ...);      │
│    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 4);   │
│                                                              │
│  BROADCAST:                                                  │
│    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &(int){1}, 4);   │
│    sendto(fd, ..., INADDR_BROADCAST);                        │
│                                                              │
│  TIMEOUT:                                                    │
│    struct timeval tv = { .tv_sec = 3 };                      │
│    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));│
│                                                              │
│  PROPIEDADES DE UDP:                                         │
│    • Sin conexión (no listen/accept)                         │
│    • Datagramas: límites de mensaje preservados              │
│    • Sin garantía de entrega, orden, ni duplicados           │
│    • sendto nunca hace short write (todo o nada)             │
│    • recvfrom trunca si buffer < datagrama                   │
│    • Un fd sirve a todos los clientes                        │
│    • Tamaño seguro: ≤ 1400 bytes (evitar fragmentación)      │
│    • Máximo teórico: 65507 bytes                             │
│                                                              │
│  TCP vs UDP RÁPIDO:                                          │
│    TCP = tubo confiable (flujo de bytes, ordenado)            │
│    UDP = buzón postal (datagramas, fire-and-forget)           │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: servidor de hora UDP

Implementa un servicio de hora por UDP:

1. **Servidor**: bind al puerto 9001, espera datagramas. Cuando
   recibe cualquier dato, responde con la hora formateada
   (`YYYY-MM-DD HH:MM:SS\n`) usando `strftime()`.
2. **Cliente**: envía un byte cualquiera al servidor, recibe la
   hora, la imprime, y termina.
3. Añadir **timeout** en el cliente: si no recibe respuesta en 2
   segundos, imprimir error y reintentar hasta 3 veces.

Probar:
- Ejecutar el servidor, luego el cliente 5 veces seguidas.
- Detener el servidor y ejecutar el cliente — verificar que el
  timeout funciona correctamente.

**Pregunta de reflexión**: este servidor responde a cualquier
dirección que le envíe un datagrama. ¿Podría un atacante hacer
que el servidor envíe respuestas a una víctima que nunca las
solicitó? (Pista: ¿puede un atacante falsificar la dirección
origen de un datagrama UDP? ¿Cómo se llama este ataque?)

---

### Ejercicio 2: descubrimiento de servicios por broadcast

Implementa un mecanismo de discovery en LAN:

1. **Servicio** (`./discover -s "MiServidor" 8080`):
   - Escucha datagramas UDP en el puerto 5555.
   - Cuando recibe el mensaje "DISCOVER", responde con
     `"SERVICE:<nombre>:<puerto>"`.
2. **Buscador** (`./discover -b`):
   - Envía "DISCOVER" por **broadcast** a 255.255.255.255:5555.
   - Espera 2 segundos recibiendo respuestas de todos los servicios.
   - Imprime la lista de servicios encontrados con su IP y nombre.
3. Ejecutar múltiples servicios con distintos nombres en la misma
   máquina (usando `SO_REUSEADDR` + `SO_REUSEPORT`), verificar que
   el buscador los encuentra a todos.

**Pregunta de reflexión**: en una red grande, el broadcast llega a
**todos** los hosts, incluso los que no tienen el servicio de
discovery. ¿Por qué esto es un problema a escala? ¿Cómo lo
resolvería el multicast?

---

### Ejercicio 3: transferencia de archivo con ACK

Implementa una transferencia de archivo básica sobre UDP con
confirmación:

1. **Protocolo** (inspirado en TFTP simplificado):
   - `DATA`: `[seq:4 bytes][len:2 bytes][payload: ≤1024 bytes]`
   - `ACK`: `[seq:4 bytes]`
   - El emisor envía un bloque y espera ACK antes de enviar el
     siguiente (stop-and-wait).
   - Timeout de 500ms, hasta 5 reintentos por bloque.
2. **Emisor**: lee un archivo, lo envía bloque a bloque.
3. **Receptor**: recibe bloques, escribe en archivo, envía ACK por
   cada uno.
4. Verificar con `md5sum` que el archivo transferido es idéntico al
   original.

Probar con un archivo de texto y luego con un binario pequeño (~100KB).

**Pregunta de reflexión**: este protocolo usa stop-and-wait (envía
un bloque, espera ACK, envía el siguiente). Si el RTT es 100ms,
¿cuántos bloques de 1024 bytes puedes transferir por segundo?
¿Cuál sería el throughput máximo en KB/s? ¿Cómo mejoraría una
ventana deslizante (enviar N bloques antes de esperar ACKs)?
