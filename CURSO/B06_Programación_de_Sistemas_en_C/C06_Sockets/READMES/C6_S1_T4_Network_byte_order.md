# Network byte order

## Índice
1. [Endianness: big-endian y little-endian](#1-endianness-big-endian-y-little-endian)
2. [Por qué importa en redes](#2-por-qué-importa-en-redes)
3. [htonl, htons, ntohl, ntohs](#3-htonl-htons-ntohl-ntohs)
4. [struct sockaddr_in en detalle](#4-struct-sockaddr_in-en-detalle)
5. [inet_pton e inet_ntop](#5-inet_pton-e-inet_ntop)
6. [getaddrinfo: la función moderna](#6-getaddrinfo-la-función-moderna)
7. [Serialización de datos en protocolos](#7-serialización-de-datos-en-protocolos)
8. [Funciones obsoletas](#8-funciones-obsoletas)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Endianness: big-endian y little-endian

Cuando un número multi-byte (como un `uint32_t`) se almacena en
memoria, hay dos formas de ordenar los bytes:

```
El número 0x01020304 (16909060 en decimal):

Big-endian (byte más significativo primero):
  Dirección:  0x00   0x01   0x02   0x03
  Contenido: [0x01] [0x02] [0x03] [0x04]
              MSB ────────────────▶ LSB
              "El grande primero"

Little-endian (byte menos significativo primero):
  Dirección:  0x00   0x01   0x02   0x03
  Contenido: [0x04] [0x03] [0x02] [0x01]
              LSB ────────────────▶ MSB
              "El pequeño primero"
```

### ¿Quién usa qué?

| Endianness | Arquitecturas |
|------------|--------------|
| Little-endian | x86, x86-64, ARM (modo default), RISC-V |
| Big-endian | SPARC, PowerPC (algunos), MIPS (algunos), z/Architecture |
| Bi-endian (configurable) | ARM, MIPS, PowerPC (nuevos) |

> **En la práctica**: tu PC, tu servidor, tu Raspberry Pi, y la
> mayoría de dispositivos modernos son **little-endian**. Pero la
> red y los protocolos de red son **big-endian**.

### Verificar el endianness de tu máquina

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t x = 0x01020304;
    uint8_t *p = (uint8_t *)&x;

    printf("0x%08X stored as: %02X %02X %02X %02X\n",
           x, p[0], p[1], p[2], p[3]);

    if (p[0] == 0x01)
        printf("Big-endian\n");
    else if (p[0] == 0x04)
        printf("Little-endian\n");

    return 0;
}
```

Resultado en x86-64:
```
0x01020304 stored as: 04 03 02 01
Little-endian
```

### Alternativa con union

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    union {
        uint32_t word;
        uint8_t  bytes[4];
    } test = { .word = 1 };

    if (test.bytes[0] == 1)
        printf("Little-endian\n");
    else
        printf("Big-endian\n");

    return 0;
}
```

---

## 2. Por qué importa en redes

Cuando dos máquinas se comunican por red, ambas deben interpretar
los bytes en el mismo orden. Si una máquina little-endian envía
el puerto 8080 (0x1F90) y la otra lo lee como big-endian, obtiene
0x901F (36895):

```
Máquina A (little-endian)            Máquina B (big-endian)
quiere enviar puerto 8080            recibe los bytes

8080 = 0x1F90                        Bytes: [0x90] [0x1F]
En memoria: [0x90] [0x1F]            Interpreta: 0x901F = 36895
            LSB    MSB               ¡INCORRECTO!

──── se envían los bytes tal cual ────▶
     [0x90] [0x1F]
```

### La solución: network byte order

Los estándares de red (RFC 1700) definen que todos los protocolos
de red usan **big-endian**, llamado **network byte order**:

```
Network byte order = Big-endian

Puerto 8080 en la red:
  [0x1F] [0x90]    (MSB primero)

Dirección IP 192.168.1.1 en la red:
  [0xC0] [0xA8] [0x01] [0x01]    (192.168.1.1, MSB primero)
```

Cada máquina convierte entre su formato nativo (**host byte order**)
y el formato de red (**network byte order**) al enviar y recibir.

```
Host A                Red              Host B
(little-endian)                        (big-endian)

host → network        ────▶           network → host
htons(8080)           [1F][90]        ntohs([1F][90])
= swap bytes                          = no-op (ya es big)
[90][1F] → [1F][90]                   [1F][90] → 8080 ✓
```

En una máquina big-endian, `htons()` no hace nada (ya está en el
formato correcto). En una máquina little-endian, intercambia los
bytes. Esto garantiza portabilidad.

---

## 3. htonl, htons, ntohl, ntohs

Cuatro funciones para convertir entre host y network byte order:

```c
#include <arpa/inet.h>

uint32_t htonl(uint32_t hostlong);    // host to network, 32-bit
uint16_t htons(uint16_t hostshort);   // host to network, 16-bit
uint32_t ntohl(uint32_t netlong);     // network to host, 32-bit
uint16_t ntohs(uint16_t netshort);    // network to host, 16-bit
```

### Nemotécnica

```
h = host (tu máquina)
n = network (big-endian)
to = conversión
l = long (32 bits)
s = short (16 bits)

htons = Host TO Network Short (16-bit)
ntohl = Network TO Host Long  (32-bit)
```

### Qué se convierte con qué

| Dato | Tamaño | Función |
|------|--------|---------|
| Puerto TCP/UDP | 16 bits | `htons()` / `ntohs()` |
| Dirección IPv4 | 32 bits | `htonl()` / `ntohl()` |
| Campos de protocolos personalizados | 16 bits | `htons()` / `ntohs()` |
| Campos de protocolos personalizados | 32 bits | `htonl()` / `ntohl()` |

### Ejemplo visual

```c
#include <stdio.h>
#include <arpa/inet.h>
#include <stdint.h>

int main(void) {
    // Puerto
    uint16_t port = 8080;           // 0x1F90
    uint16_t net_port = htons(port); // 0x901F en little-endian
    printf("Port %u:\n", port);
    printf("  Host order:    0x%04X\n", port);
    printf("  Network order: 0x%04X\n", net_port);

    uint8_t *p = (uint8_t *)&net_port;
    printf("  Bytes on wire: %02X %02X\n", p[0], p[1]);
    // → 1F 90 (big-endian, MSB primero)

    // Dirección IP
    uint32_t ip = 0xC0A80101;          // 192.168.1.1
    uint32_t net_ip = htonl(ip);
    printf("\nIP 192.168.1.1:\n");
    printf("  Host order:    0x%08X\n", ip);
    printf("  Network order: 0x%08X\n", net_ip);

    p = (uint8_t *)&net_ip;
    printf("  Bytes on wire: %02X %02X %02X %02X\n",
           p[0], p[1], p[2], p[3]);
    // → C0 A8 01 01 (192.168.1.1)

    return 0;
}
```

### Conversión ida y vuelta

Las conversiones son simétricas:

```c
htonl(ntohl(x)) == x;    // siempre
ntohl(htonl(x)) == x;    // siempre
htons(ntohs(x)) == x;    // siempre
ntohs(htons(x)) == x;    // siempre

// En big-endian: htonl == ntohl == no-op
// En little-endian: htonl == ntohl == byte swap
```

### ¿Y para 64 bits?

POSIX no define `htonll`/`ntohll`. Hay varias opciones:

```c
#include <endian.h>  // Linux/glibc

// Opción A: htobe64/be64toh (Linux, glibc)
uint64_t net64 = htobe64(host64);
uint64_t host64 = be64toh(net64);

// Opción B: manual
uint64_t htonll(uint64_t x) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)htonl(x & 0xFFFFFFFF) << 32) | htonl(x >> 32);
    #else
    return x;
    #endif
}
```

---

## 4. struct sockaddr_in en detalle

Revisando la estructura con los ojos puestos en byte order:

```c
#include <netinet/in.h>

struct sockaddr_in {
    sa_family_t    sin_family;   // AF_INET — host byte order
    in_port_t      sin_port;     // Puerto — NETWORK byte order
    struct in_addr sin_addr;     // IP     — NETWORK byte order
};

struct in_addr {
    uint32_t s_addr;             // IPv4   — NETWORK byte order
};
```

### Qué va en qué orden

```
┌─────────────┬──────────────────────────────────────────┐
│ Campo       │ Byte order                               │
├─────────────┼──────────────────────────────────────────┤
│ sin_family  │ Host (el kernel lo maneja internamente)  │
│ sin_port    │ NETWORK → usar htons()/ntohs()           │
│ sin_addr    │ NETWORK → usar htonl()/inet_pton()       │
└─────────────┴──────────────────────────────────────────┘
```

### Llenar correctamente la estructura

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));

addr.sin_family      = AF_INET;             // host order (OK)
addr.sin_port        = htons(8080);         // ← SIEMPRE htons
addr.sin_addr.s_addr = htonl(INADDR_ANY);  // ← SIEMPRE htonl

// O con inet_pton (ya devuelve network order):
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
// inet_pton hace la conversión internamente, no necesitas htonl
```

### Constantes especiales

```c
// Estas constantes YA están en host byte order,
// así que NECESITAN htonl():
INADDR_ANY       // 0x00000000 (0.0.0.0)
INADDR_LOOPBACK  // 0x7F000001 (127.0.0.1)
INADDR_BROADCAST // 0xFFFFFFFF (255.255.255.255)

addr.sin_addr.s_addr = htonl(INADDR_ANY);       // ← correcto
addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // ← correcto

// Nota: INADDR_ANY es 0, así que htonl(0) == 0 siempre.
// Pero usar htonl() es la convención correcta y ayuda a leer el código.
```

### Leer la estructura después de accept/recvfrom

```c
struct sockaddr_in client_addr;
socklen_t len = sizeof(client_addr);
int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);

// Los campos están en NETWORK byte order
// Convertir a host order para mostrar:
char ip_str[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

uint16_t port = ntohs(client_addr.sin_port);  // ← SIEMPRE ntohs
printf("Client: %s:%u\n", ip_str, port);
```

---

## 5. inet_pton e inet_ntop

Estas funciones convierten direcciones IP entre su representación
**texto** (presentación) y **binaria** (network byte order):

```
"192.168.1.1"  ←──inet_ntop──  0xC0A80101 (network order)
               ──inet_pton──▶

p = presentation (texto legible)
n = network (binario en network byte order)
```

### inet_pton — texto a binario

```c
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst);
// af:  AF_INET o AF_INET6
// src: cadena con la IP ("192.168.1.1" o "::1")
// dst: puntero a struct in_addr (IPv4) o struct in6_addr (IPv6)
// Retorna: 1 = éxito, 0 = formato inválido, -1 = af inválido
```

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_port   = htons(8080);

int ret = inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
if (ret != 1) {
    if (ret == 0)
        fprintf(stderr, "Invalid IP address format\n");
    else
        perror("inet_pton");
    exit(EXIT_FAILURE);
}
```

### inet_ntop — binario a texto

```c
const char *inet_ntop(int af, const void *src,
                      char *dst, socklen_t size);
// af:   AF_INET o AF_INET6
// src:  puntero a in_addr o in6_addr (en network byte order)
// dst:  buffer para la cadena resultante
// size: tamaño del buffer
// Retorna: puntero a dst, o NULL en error
```

```c
// Después de accept()
struct sockaddr_in client_addr;
// ... accept() llena client_addr ...

char ip_str[INET_ADDRSTRLEN];  // 16 bytes para IPv4
inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
printf("Client IP: %s\n", ip_str);  // "192.168.1.42"
```

### Tamaños de buffer

```c
#include <arpa/inet.h>

INET_ADDRSTRLEN    // 16 — "255.255.255.255\0"
INET6_ADDRSTRLEN   // 46 — "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255\0"
```

### IPv6

Las mismas funciones trabajan con IPv6 cambiando `AF_INET` por
`AF_INET6`:

```c
struct sockaddr_in6 addr6;
memset(&addr6, 0, sizeof(addr6));
addr6.sin6_family = AF_INET6;
addr6.sin6_port   = htons(8080);

inet_pton(AF_INET6, "::1", &addr6.sin6_addr);
// o: inet_pton(AF_INET6, "2001:db8::1", &addr6.sin6_addr);

// Convertir a texto
char ip6_str[INET6_ADDRSTRLEN];
inet_ntop(AF_INET6, &addr6.sin6_addr, ip6_str, sizeof(ip6_str));
printf("IPv6: %s\n", ip6_str);  // "::1"
```

### Mapa completo de conversiones

```
Texto legible         Binario (network order)        Numérico (host order)
─────────────         ──────────────────────         ────────────────────
"192.168.1.1"  ──pton──▶  in_addr.s_addr    ──ntohl──▶  0xC0A80101
               ◀──ntop──                    ◀──htonl──

"8080"         ──atoi──▶  uint16_t          ──htons──▶  sin_port (net)
               ◀─sprintf─                  ◀──ntohs──

"::1"          ──pton──▶  in6_addr.s6_addr  (128 bits, no hay htonl)
               ◀──ntop──
```

---

## 6. getaddrinfo: la función moderna

`getaddrinfo()` es la forma **moderna y recomendada** de resolver
nombres de host y configurar estructuras de dirección. Reemplaza
a `gethostbyname()`, `inet_aton()`, y la construcción manual de
`sockaddr_in`:

```c
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node,       // hostname o IP ("example.com" o "1.2.3.4")
                const char *service,    // puerto o servicio ("8080" o "http")
                const struct addrinfo *hints,  // filtros
                struct addrinfo **res); // resultado (lista enlazada)

void freeaddrinfo(struct addrinfo *res); // liberar la lista
const char *gai_strerror(int errcode);   // error → texto
```

### Estructura addrinfo

```c
struct addrinfo {
    int              ai_flags;      // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;     // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;   // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;   // IPPROTO_TCP, IPPROTO_UDP
    socklen_t        ai_addrlen;    // tamaño de ai_addr
    struct sockaddr *ai_addr;       // la dirección (ya en network order)
    char            *ai_canonname;  // nombre canónico (si AI_CANONNAME)
    struct addrinfo *ai_next;       // siguiente resultado
};
```

### Cliente con getaddrinfo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

int connect_to(const char *host, const char *port) {
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,     // IPv4 o IPv6
        .ai_socktype = SOCK_STREAM    // TCP
    };
    struct addrinfo *res, *rp;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int fd = -1;
    // Iterar la lista: intentar cada dirección hasta que una funcione
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;  // éxito

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return fd;  // -1 si ninguna dirección funcionó
}

int main(void) {
    int fd = connect_to("example.com", "80");
    if (fd == -1) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    const char *request = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
    write(fd, request, strlen(request));

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    close(fd);
    return 0;
}
```

### Servidor con getaddrinfo

```c
int create_listener(const char *port) {
    struct addrinfo hints = {
        .ai_family   = AF_INET6,     // IPv6 (acepta IPv4 con dual-stack)
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE     // para bind (usar INADDR_ANY / in6addr_any)
    };
    struct addrinfo *res;

    int err = getaddrinfo(NULL, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Dual-stack: aceptar IPv4 e IPv6 en el mismo socket
    int off = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

    bind(fd, res->ai_addr, res->ai_addrlen);
    listen(fd, 128);

    freeaddrinfo(res);
    return fd;
}
```

### Ventajas de getaddrinfo

| Ventaja | Detalle |
|---------|---------|
| IPv4 + IPv6 | Con `AF_UNSPEC`, prueba ambos automáticamente |
| Resolución DNS | Convierte hostnames a IPs |
| Servicios por nombre | "http" → 80, "ssh" → 22 |
| Portable | POSIX, funciona en Linux/macOS/Windows |
| Thread-safe | A diferencia de `gethostbyname()` |
| Maneja byte order | Las estructuras retornadas ya están en network order |

### AI_PASSIVE y AI_NUMERICHOST

| Flag | Efecto |
|------|--------|
| `AI_PASSIVE` | Si `node` es NULL, devuelve INADDR_ANY (para servidores) |
| `AI_NUMERICHOST` | No hacer DNS, `node` debe ser IP numérica |
| `AI_NUMERICSERV` | No buscar en /etc/services, `service` debe ser número |
| `AI_CANONNAME` | Rellenar `ai_canonname` con el nombre canónico |
| `AI_ADDRCONFIG` | Solo devolver IPv4 si hay interfaz IPv4, etc. |

### getnameinfo — la inversa

Convierte una `struct sockaddr` a strings de host+servicio:

```c
#include <netdb.h>

char host[NI_MAXHOST];   // 1025
char serv[NI_MAXSERV];   // 32

// Después de accept()
struct sockaddr_storage client_addr;
socklen_t len = sizeof(client_addr);
int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);

getnameinfo((struct sockaddr *)&client_addr, len,
            host, sizeof(host),
            serv, sizeof(serv),
            NI_NUMERICHOST | NI_NUMERICSERV);  // no DNS reverso

printf("Client: %s port %s\n", host, serv);
// "192.168.1.42 port 54321"
```

`NI_NUMERICHOST` evita el DNS reverso (que puede ser lento). Úsalo
siempre a menos que realmente necesites el hostname.

---

## 7. Serialización de datos en protocolos

Cuando diseñas un protocolo binario que envía estructuras por la
red, **cada campo multi-byte** debe convertirse a network byte
order:

### Ejemplo: protocolo de sensor

```c
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>

// Estructura del protocolo (wire format)
typedef struct {
    uint32_t sensor_id;     // network order
    uint32_t timestamp;     // network order (Unix epoch)
    int16_t  temperature;   // network order (centésimas de grado)
    uint16_t humidity;      // network order (centésimas de %)
} __attribute__((packed)) sensor_msg_t;

// Serializar para enviar
void serialize_sensor(sensor_msg_t *msg,
                      uint32_t id, uint32_t ts,
                      int16_t temp, uint16_t hum) {
    msg->sensor_id   = htonl(id);
    msg->timestamp   = htonl(ts);
    msg->temperature = (int16_t)htons((uint16_t)temp);
    msg->humidity    = htons(hum);
}

// Deserializar al recibir
void deserialize_sensor(const sensor_msg_t *msg,
                        uint32_t *id, uint32_t *ts,
                        int16_t *temp, uint16_t *hum) {
    *id   = ntohl(msg->sensor_id);
    *ts   = ntohl(msg->timestamp);
    *temp = (int16_t)ntohs((uint16_t)msg->temperature);
    *hum  = ntohs(msg->humidity);
}
```

### packed y alineamiento

```c
// Sin packed: el compilador puede insertar padding
struct without_packed {
    uint8_t  a;     // 1 byte
    // 3 bytes de padding (para alinear b a 4 bytes)
    uint32_t b;     // 4 bytes
};  // sizeof = 8

// Con packed: sin padding, layout exacto en wire
struct __attribute__((packed)) with_packed {
    uint8_t  a;     // 1 byte
    uint32_t b;     // 4 bytes (posiblemente desalineado)
};  // sizeof = 5
```

> **Cuidado**: acceder a campos desalineados en `packed` structs
> puede ser lento (o causar SIGBUS en algunas arquitecturas). En
> x86 funciona pero con penalización de rendimiento. La alternativa
> es usar `memcpy` para extraer campos:

```c
uint32_t value;
memcpy(&value, &msg->unaligned_field, sizeof(value));
value = ntohl(value);
```

### Alternativa: serialización manual con buffer

En vez de `packed` structs, puedes serializar campo a campo en un
buffer de bytes:

```c
// Serializar en un buffer
void encode_sensor(uint8_t *buf,
                   uint32_t id, uint32_t ts,
                   int16_t temp, uint16_t hum) {
    uint32_t net_id = htonl(id);
    uint32_t net_ts = htonl(ts);
    uint16_t net_temp = htons((uint16_t)temp);
    uint16_t net_hum  = htons(hum);

    memcpy(buf + 0,  &net_id,   4);
    memcpy(buf + 4,  &net_ts,   4);
    memcpy(buf + 8,  &net_temp, 2);
    memcpy(buf + 10, &net_hum,  2);
    // Total: 12 bytes
}

// Deserializar desde buffer
void decode_sensor(const uint8_t *buf,
                   uint32_t *id, uint32_t *ts,
                   int16_t *temp, uint16_t *hum) {
    uint32_t net_id, net_ts;
    uint16_t net_temp, net_hum;

    memcpy(&net_id,   buf + 0,  4);
    memcpy(&net_ts,   buf + 4,  4);
    memcpy(&net_temp, buf + 8,  2);
    memcpy(&net_hum,  buf + 10, 2);

    *id   = ntohl(net_id);
    *ts   = ntohl(net_ts);
    *temp = (int16_t)ntohs(net_temp);
    *hum  = ntohs(net_hum);
}
```

Este enfoque es más seguro, portable, y no depende de extensiones
del compilador.

### Campos de 1 byte

Los campos de 1 byte (`uint8_t`, `char`) **no necesitan conversión**
de byte order. Solo los campos multi-byte requieren hton*/ntoh*.

### Strings

Las cadenas de texto (arrays de `char`) se envían byte a byte, sin
conversión de byte order. Pero debes definir cómo se delimitan:

```
Opciones para strings en protocolos:

1. Longitud fija:    [H][e][l][l][o][\0][\0][\0]   (8 bytes siempre)
2. Prefijo de long:  [0x05][H][e][l][l][o]          (len en network order)
3. Null-terminated:  [H][e][l][l][o][\0]             (longitud variable)
```

---

## 8. Funciones obsoletas

Estas funciones todavía aparecen en código antiguo y tutoriales,
pero **no debes usarlas** en código nuevo:

### inet_aton / inet_addr — reemplazadas por inet_pton

```c
// OBSOLETO: inet_addr
// No distingue error (-1) de la IP válida 255.255.255.255
in_addr_t addr = inet_addr("192.168.1.1");

// OBSOLETO: inet_aton
// Solo IPv4, no thread-safe en algunas implementaciones
struct in_addr addr;
inet_aton("192.168.1.1", &addr);

// MODERNO: inet_pton
// Soporta IPv4 e IPv6, retorno claro
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
```

### inet_ntoa — reemplazada por inet_ntop

```c
// OBSOLETO: inet_ntoa
// Retorna puntero a buffer estático → no thread-safe, se sobreescribe
char *ip_str = inet_ntoa(addr.sin_addr);  // ¡peligroso!
printf("IP: %s\n", ip_str);
// Si otro thread llama inet_ntoa, tu puntero apunta a basura

// MODERNO: inet_ntop
// Thread-safe, soporta IPv6, buffer proporcionado por el llamante
char ip_str[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
```

### gethostbyname — reemplazada por getaddrinfo

```c
// OBSOLETO: gethostbyname
// No thread-safe (retorna struct estático), solo IPv4
struct hostent *he = gethostbyname("example.com");

// MODERNO: getaddrinfo
// Thread-safe, IPv4+IPv6, resolución de servicio
struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
struct addrinfo *res;
getaddrinfo("example.com", "80", &hints, &res);
// ... usar res ...
freeaddrinfo(res);
```

### Tabla de reemplazo

```
┌──────────────────┬──────────────────┬──────────────────────┐
│ Obsoleta         │ Reemplazo        │ Razón                │
├──────────────────┼──────────────────┼──────────────────────┤
│ inet_addr()      │ inet_pton()      │ Error indistinguible │
│ inet_aton()      │ inet_pton()      │ Solo IPv4            │
│ inet_ntoa()      │ inet_ntop()      │ No thread-safe       │
│ gethostbyname()  │ getaddrinfo()    │ No thread-safe,      │
│                  │                  │ solo IPv4            │
│ gethostbyaddr()  │ getnameinfo()    │ No thread-safe       │
│ getservbyname()  │ getaddrinfo()    │ No thread-safe       │
│ herror()         │ gai_strerror()   │ Formato antiguo      │
└──────────────────┴──────────────────┴──────────────────────┘
```

---

## 9. Errores comunes

### Error 1: olvidar htons/htonl

```c
// MAL: puerto sin conversión
addr.sin_port = 8080;                // 0x1F90 en host order
// En little-endian, el kernel interpreta: 0x901F = 36895
// → bind al puerto 36895, no al 8080
```

**Solución**: siempre usar `htons()` para puertos:

```c
addr.sin_port = htons(8080);         // correcto
```

> En x86 (little-endian), sin `htons` el programa parece funcionar
> pero en un puerto incorrecto. El error es silencioso y difícil
> de diagnosticar.

### Error 2: conversión doble

```c
// MAL: inet_pton ya devuelve en network order
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
addr.sin_addr.s_addr = htonl(addr.sin_addr.s_addr);  // ¡doble conversión!
// Resultado: IP incorrecta (bytes invertidos de nuevo)
```

**Solución**: no aplicar `htonl` después de `inet_pton`:

```c
// inet_pton ya produce network byte order
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);
// Listo, no tocar más
```

### Error 3: usar ntohs para imprimir pero olvidar en la lógica

```c
// MAL: comparar puerto sin convertir
if (client_addr.sin_port == 8080) {  // sin ntohs → nunca es true
    // ...
}

// BIEN: convertir a host order para comparar
if (ntohs(client_addr.sin_port) == 8080) {
    // ...
}
```

### Error 4: no usar getaddrinfo para hostnames

```c
// MAL: intentar inet_pton con un hostname
inet_pton(AF_INET, "example.com", &addr.sin_addr);
// Retorna 0 (formato inválido) — inet_pton no resuelve DNS

// BIEN: usar getaddrinfo para hostnames
struct addrinfo *res;
getaddrinfo("example.com", "80", &hints, &res);
```

`inet_pton` solo acepta IPs numéricas. Para nombres de host,
siempre usa `getaddrinfo`.

### Error 5: buffer demasiado pequeño para inet_ntop

```c
// MAL: buffer menor que INET_ADDRSTRLEN
char ip[10];  // "255.255.255.255" necesita 16 bytes
inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
// Si la IP tiene más de 9 caracteres → retorna NULL, errno = ENOSPC

// Peor con IPv6:
char ip6[20];  // INET6_ADDRSTRLEN es 46
inet_ntop(AF_INET6, &addr6.sin6_addr, ip6, sizeof(ip6));
// Casi siempre falla
```

**Solución**: usar las constantes estándar:

```c
char ip4[INET_ADDRSTRLEN];    // 16 bytes
char ip6[INET6_ADDRSTRLEN];   // 46 bytes
```

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Network Byte Order                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  REGLA: network byte order = big-endian                      │
│         host byte order    = depende de la CPU               │
│                                                              │
│  CONVERSIÓN (16-bit):                                        │
│    htons(port)    host → network (para sin_port)             │
│    ntohs(port)    network → host (para imprimir/comparar)    │
│                                                              │
│  CONVERSIÓN (32-bit):                                        │
│    htonl(addr)    host → network (para sin_addr.s_addr)      │
│    ntohl(addr)    network → host (para imprimir/comparar)    │
│                                                              │
│  IP TEXTO ↔ BINARIO:                                         │
│    inet_pton(AF, "1.2.3.4", &in_addr)   texto → network     │
│    inet_ntop(AF, &in_addr, buf, size)    network → texto     │
│    ¡inet_pton ya produce network order, NO usar htonl!       │
│                                                              │
│  BUFFERS:                                                    │
│    INET_ADDRSTRLEN  = 16  (IPv4: "255.255.255.255\0")        │
│    INET6_ADDRSTRLEN = 46  (IPv6 máximo)                      │
│                                                              │
│  RESOLUCIÓN DNS:                                             │
│    getaddrinfo(host, service, &hints, &res)                  │
│    freeaddrinfo(res)                                         │
│    getnameinfo(sa, len, host, hlen, serv, slen, flags)       │
│    gai_strerror(err)                                         │
│                                                              │
│  HINTS COMUNES:                                              │
│    { .ai_family=AF_UNSPEC, .ai_socktype=SOCK_STREAM }        │
│    { .ai_flags=AI_PASSIVE } // para servidores (NULL node)   │
│    { .ai_flags=AI_NUMERICHOST } // no DNS                    │
│                                                              │
│  OBSOLETAS (NO USAR):                                        │
│    inet_addr/aton → inet_pton                                │
│    inet_ntoa      → inet_ntop                                │
│    gethostbyname  → getaddrinfo                              │
│                                                              │
│  PROTOCOLO BINARIO:                                          │
│    • Cada campo multi-byte: hton*/ntoh*                      │
│    • Campos de 1 byte: sin conversión                        │
│    • Strings: sin conversión (byte a byte)                   │
│    • packed structs o memcpy para evitar padding              │
│                                                              │
│  CUÁNDO CONVERTIR:                                           │
│    Enviar → htons/htonl (host → network)                     │
│    Recibir → ntohs/ntohl (network → host)                    │
│    Llenar sockaddr_in → htons (port), inet_pton (IP)         │
│    Leer sockaddr_in → ntohs (port), inet_ntop (IP)           │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: inspector de byte order

Escribe un programa que:

1. Reciba una dirección IPv4 y un puerto por línea de comandos
   (`./inspector 192.168.1.1 8080`).
2. Llene una `struct sockaddr_in` correctamente.
3. Imprima cada campo mostrando su valor en host order, network
   order, y los bytes individuales en memoria:

```
Port: 8080
  Host order:    0x1F90
  Network order: 0x901F
  Bytes:         1F 90

IP: 192.168.1.1
  Host order:    0xC0A80101
  Network order: 0x0101A8C0
  Bytes:         C0 A8 01 01

sin_family: 2 (AF_INET)
  Bytes:         02 00
```

4. Verificar que `inet_pton` produce el mismo resultado que la
   construcción manual byte a byte.

**Pregunta de reflexión**: en una máquina big-endian, ¿qué
diferencia habría en la salida de tu programa? ¿Cambiarían los
"Bytes" de la IP? ¿Cambiaría el "Host order"?

---

### Ejercicio 2: cliente dual-stack con getaddrinfo

Escribe un cliente TCP que se conecte a un servidor especificado por
nombre de host, soportando IPv4 e IPv6 automáticamente:

1. Recibir host y puerto por argumentos (`./client example.com 80`).
2. Usar `getaddrinfo` con `AF_UNSPEC` para obtener todas las
   direcciones posibles.
3. Imprimir cada dirección candidata (IP y familia) antes de
   intentar conectar.
4. Intentar cada dirección en orden hasta que una tenga éxito
   (Happy Eyeballs simplificado).
5. Una vez conectado, enviar `GET / HTTP/1.0\r\nHost: <host>\r\n\r\n`
   y mostrar la respuesta.

Probar:
```bash
./client example.com 80        # probablemente tiene IPv4 e IPv6
./client localhost 8080         # ::1 y 127.0.0.1
./client ipv6.google.com 80    # solo IPv6
```

**Pregunta de reflexión**: el algoritmo "Happy Eyeballs" (RFC 8305)
no intenta las direcciones secuencialmente, sino que inicia
conexiones IPv6 e IPv4 en paralelo con un delay de 250ms.
¿Por qué no basta con intentar IPv6 primero y luego IPv4?
¿Qué pasa si el host tiene AAAA record pero la red no soporta
IPv6?

---

### Ejercicio 3: protocolo binario con byte order correcto

Diseña e implementa un protocolo de "sensor de temperatura" entre
un cliente y un servidor UDP:

1. **Formato del mensaje** (12 bytes):
   - `sensor_id` (uint32_t, network order)
   - `timestamp` (uint32_t, network order, Unix epoch)
   - `temperature` (int16_t, network order, centésimas de grado)
   - `humidity` (uint16_t, network order, centésimas de %)
2. **Cliente** (simulador de sensor):
   - Enviar un datagrama con datos simulados cada segundo.
   - `sensor_id` fijo, `timestamp` de `time()`, temperatura y
     humedad aleatorias.
3. **Servidor** (colector):
   - Recibir datagramas, deserializar, imprimir en formato legible:
     `[2026-03-20 10:30:45] Sensor 42: 23.50°C, 65.20% humidity`
4. Escribir funciones `serialize()` y `deserialize()` que manejen
   el byte order correctamente.
5. Verificar con `hexdump` o Wireshark que los bytes en la red están
   en big-endian.

**Pregunta de reflexión**: si agregaras un campo `float` (IEEE 754)
al protocolo, ¿podrías simplemente hacer `htonl(*(uint32_t*)&f)`
para enviarlo? ¿Es portable asumir que ambas máquinas usan
IEEE 754? ¿Qué alternativa más segura existe para transmitir
valores de punto flotante?
