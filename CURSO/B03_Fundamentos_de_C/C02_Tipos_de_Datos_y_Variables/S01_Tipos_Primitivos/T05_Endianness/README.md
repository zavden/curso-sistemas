# T05 — Endianness

## Qué es endianness

Endianness es el orden en que los bytes de un valor multi-byte
se almacenan en memoria:

```
El número 0x01020304 (int de 32 bits) tiene 4 bytes:
  01, 02, 03, 04

¿En qué orden se almacenan en memoria?

Big-endian (byte más significativo primero):
  Dirección:  0x100  0x101  0x102  0x103
  Contenido:   01     02     03     04
  (el "big end" va primero)

Little-endian (byte menos significativo primero):
  Dirección:  0x100  0x101  0x102  0x103
  Contenido:   04     03     02     01
  (el "little end" va primero)
```

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t x = 0x01020304;
    uint8_t *bytes = (uint8_t *)&x;

    printf("Valor: 0x%08X\n", x);
    printf("Bytes en memoria: ");
    for (int i = 0; i < 4; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");

    // En little-endian (x86): 04 03 02 01
    // En big-endian (PowerPC, network): 01 02 03 04

    return 0;
}
```

### Quién usa qué

```
Little-endian:
  - x86 / x86-64 (Intel, AMD) — casi todos los PCs
  - ARM en modo little-endian (la mayoría hoy)
  - RISC-V (default)

Big-endian:
  - Protocolos de red (IP, TCP, UDP) — "network byte order"
  - Java bytecode
  - Formatos de archivo (JPEG, PNG headers)
  - Algunas arquitecturas: SPARC, PowerPC (en algunos modos)

Bi-endian (pueden operar en ambos modos):
  - ARM (configurable)
  - MIPS (configurable)
  - PowerPC (configurable)
```

### Detectar endianness en runtime

```c
#include <stdio.h>
#include <stdint.h>

int is_little_endian(void) {
    uint16_t x = 1;            // 0x0001
    uint8_t *byte = (uint8_t *)&x;
    return byte[0] == 1;       // si el primer byte es 01 → little-endian
}

int main(void) {
    if (is_little_endian()) {
        printf("Little-endian\n");
    } else {
        printf("Big-endian\n");
    }
    return 0;
}
```

```c
// Alternativa con union:
union EndianCheck {
    uint32_t value;
    uint8_t  bytes[4];
};

int is_little_endian(void) {
    union EndianCheck check = { .value = 1 };
    return check.bytes[0] == 1;
}
```

## Por qué importa

Endianness **no importa** mientras te quedes dentro de un
programa. Importa cuando los bytes salen del programa:

```
Endianness importa cuando:
1. Lees/escribes archivos binarios
2. Envías/recibes datos por red
3. Trabajas con hardware (registros mapeados en memoria)
4. Compartes datos entre plataformas diferentes
5. Inspeccionas memoria con un debugger

Endianness NO importa para:
1. Aritmética normal (int x = 42; x += 1;)
2. Operaciones de bits en el mismo tipo
3. Comparaciones
4. Datos en texto (strings, JSON, etc.)
```

### Archivos binarios

```c
// Si escribes un int a un archivo en little-endian
// y lo lees en big-endian, el valor es diferente:

uint32_t value = 0x01020304;

// Escribir en x86 (little-endian):
fwrite(&value, sizeof(value), 1, file);
// Archivo contiene: 04 03 02 01

// Leer en big-endian:
fread(&value, sizeof(value), 1, file);
// value = 0x04030201 (incorrecto)
```

```c
// Solución: siempre escribir en un endianness definido.
// Convención: big-endian (network byte order) o especificar
// en la documentación del formato.

// Escritura portable:
void write_uint32_be(FILE *f, uint32_t val) {
    uint8_t bytes[4];
    bytes[0] = (val >> 24) & 0xFF;    // byte más significativo
    bytes[1] = (val >> 16) & 0xFF;
    bytes[2] = (val >> 8)  & 0xFF;
    bytes[3] =  val        & 0xFF;    // byte menos significativo
    fwrite(bytes, 4, 1, f);
}

// Lectura portable:
uint32_t read_uint32_be(FILE *f) {
    uint8_t bytes[4];
    fread(bytes, 4, 1, f);
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           ((uint32_t)bytes[3]);
}
```

### Protocolos de red

```c
// Los protocolos de red usan big-endian ("network byte order").
// Las funciones de conversión están en <arpa/inet.h>:

#include <arpa/inet.h>

// Host to Network:
uint32_t htonl(uint32_t hostlong);    // 32 bits, host → network
uint16_t htons(uint16_t hostshort);   // 16 bits, host → network

// Network to Host:
uint32_t ntohl(uint32_t netlong);     // 32 bits, network → host
uint16_t ntohs(uint16_t netshort);    // 16 bits, network → host
```

```c
// Ejemplo: preparar un puerto TCP para enviar por red:
uint16_t port = 8080;
uint16_t net_port = htons(port);
// En little-endian: 8080 = 0x1F90
//   host:    90 1F (little-endian)
//   network: 1F 90 (big-endian)

// Ejemplo: leer una dirección IPv4 de un paquete:
uint32_t net_addr;
// ... recibido por red ...
uint32_t host_addr = ntohl(net_addr);
```

```c
// En una máquina big-endian, htonl/ntohl no hacen nada (son no-ops).
// En una máquina little-endian, intercambian los bytes.
// El código es portable porque usa estas funciones en lugar de
// asumir un endianness.
```

### Inspeccionar memoria en GDB

```bash
# Endianness es visible al examinar memoria en GDB:

(gdb) print x
# $1 = 305419896             (0x12345678)

(gdb) x/4xb &x
# 0x7fffe4c4: 0x78 0x56 0x34 0x12    ← little-endian
# El byte 0x78 (el menos significativo) está primero

# Si estuviéramos en big-endian:
# 0x7fffe4c4: 0x12 0x34 0x56 0x78
```

## Byte swapping

```c
// Intercambiar bytes manualmente:

uint16_t swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

uint32_t swap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

uint64_t swap64(uint64_t x) {
    return ((x >> 56) & 0x00000000000000FF) |
           ((x >> 40) & 0x000000000000FF00) |
           ((x >> 24) & 0x0000000000FF0000) |
           ((x >>  8) & 0x00000000FF000000) |
           ((x <<  8) & 0x000000FF00000000) |
           ((x << 24) & 0x0000FF0000000000) |
           ((x << 40) & 0x00FF000000000000) |
           ((x << 56) & 0xFF00000000000000);
}
```

```c
// GCC tiene built-ins optimizados (una sola instrucción en x86):
uint32_t swapped = __builtin_bswap32(x);    // extensión GCC
uint64_t swapped64 = __builtin_bswap64(x);  // extensión GCC
uint16_t swapped16 = __builtin_bswap16(x);  // extensión GCC

// En Linux, <endian.h> tiene funciones portables:
#include <endian.h>

uint32_t be = htobe32(host_val);    // host to big-endian 32
uint32_t le = htole32(host_val);    // host to little-endian 32
uint32_t h  = be32toh(net_val);     // big-endian 32 to host
uint32_t h2 = le32toh(le_val);      // little-endian 32 to host
// También: htobe16, htobe64, htole16, htole64, etc.
```

## Endianness y structs

```c
// Los structs NO se pueden enviar directamente por red o a archivos
// binarios entre plataformas diferentes:

struct Packet {
    uint16_t type;
    uint32_t length;
    uint16_t flags;
};

// Problemas:
// 1. Endianness de cada campo puede ser diferente
// 2. Padding entre campos puede variar
// 3. sizeof del struct puede variar

// Solución: serializar campo por campo:
void send_packet(int fd, const struct Packet *p) {
    uint16_t type = htons(p->type);
    uint32_t len  = htonl(p->length);
    uint16_t flags = htons(p->flags);

    write(fd, &type, 2);
    write(fd, &len, 4);
    write(fd, &flags, 2);
}

void recv_packet(int fd, struct Packet *p) {
    uint16_t type;
    uint32_t len;
    uint16_t flags;

    read(fd, &type, 2);
    read(fd, &len, 4);
    read(fd, &flags, 2);

    p->type   = ntohs(type);
    p->length = ntohl(len);
    p->flags  = ntohs(flags);
}
```

## Endianness y operaciones de bits

```c
// Las operaciones de bits trabajan sobre el VALOR, no sobre
// los bytes en memoria. Endianness es transparente:

uint32_t x = 0x12345678;
uint32_t y = x >> 8;       // siempre 0x00123456
                            // independiente del endianness

uint32_t z = x & 0xFF;     // siempre 0x78
                            // el byte menos significativo

// Endianness solo importa cuando accedes a los BYTES
// individuales en memoria (a través de un uint8_t*).
// Las operaciones aritméticas y de bits son abstractas
// sobre el valor, no sobre la representación en memoria.
```

## Tabla resumen

| Concepto | Descripción |
|---|---|
| Little-endian | Byte menos significativo primero (x86) |
| Big-endian | Byte más significativo primero (red) |
| Network byte order | Big-endian (convención de protocolos) |
| htonl/htons | Host → Network (convertir antes de enviar) |
| ntohl/ntohs | Network → Host (convertir después de recibir) |
| __builtin_bswap32 | Byte swap rápido (extensión GCC) |
| htobe32/be32toh | Conversión explícita (Linux <endian.h>) |

---

## Ejercicios

### Ejercicio 1 — Detectar endianness

```c
// Escribir un programa que determine si tu sistema es
// little-endian o big-endian.
// Imprimir los bytes individuales de un uint32_t para verificar.
```

### Ejercicio 2 — Byte swap

```c
// Implementar swap32(uint32_t x) que invierta el orden de bytes.
// Verificar: swap32(0x01020304) == 0x04030201
// Verificar: swap32(swap32(x)) == x para cualquier x
```

### Ejercicio 3 — Lectura portable

```c
// Escribir funciones para leer/escribir un uint32_t en big-endian
// a un archivo, sin depender del endianness del host.
// Verificar que los bytes en el archivo son siempre
// el mismo orden independientemente de la plataforma.
```
