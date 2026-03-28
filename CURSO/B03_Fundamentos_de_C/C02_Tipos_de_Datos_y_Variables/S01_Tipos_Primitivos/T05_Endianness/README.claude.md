# T05 — Endianness

> Sin erratas detectadas en el material original.

---

## 1. Qué es endianness

Endianness es el orden en que los bytes de un valor multi-byte se almacenan en memoria. Solo afecta a tipos de más de 1 byte (`short`, `int`, `long`, `float`, `double`, etc.). Un `char`/`uint8_t` no tiene endianness.

```
El número 0x01020304 (uint32_t, 4 bytes) tiene bytes: 01, 02, 03, 04

Big-endian (byte más significativo primero — "big end first"):
  Dirección:  0x100  0x101  0x102  0x103
  Contenido:   01     02     03     04

Little-endian (byte menos significativo primero — "little end first"):
  Dirección:  0x100  0x101  0x102  0x103
  Contenido:   04     03     02     01
```

Analogía: es como escribir un número decimal. "Mil doscientos" se escribe `1200` (big-endian: dígito más significativo primero). En little-endian sería `0021`.

### Quién usa qué

```
Little-endian:
  - x86 / x86-64 (Intel, AMD) — casi todos los PCs y servidores
  - ARM en modo little-endian (la mayoría hoy, incluyendo smartphones)
  - RISC-V (default)

Big-endian:
  - Protocolos de red (IP, TCP, UDP) — "network byte order"
  - Java bytecode
  - Formatos de archivo (JPEG, PNG headers)
  - SPARC, PowerPC (en algunos modos)

Bi-endian (configurables):
  - ARM, MIPS, PowerPC
```

---

## 2. Detectar endianness en runtime

### Método 1: cast a `uint8_t*`

```c
#include <stdint.h>

int is_little_endian(void) {
    uint16_t x = 1;                // 0x0001: bytes son 00 y 01
    uint8_t *byte = (uint8_t *)&x;
    return byte[0] == 1;           // si byte[0] es 01 → little-endian
}
// C permite acceder a cualquier objeto a través de un puntero a
// tipo carácter (char, unsigned char, uint8_t). Esto es seguro y portable.
```

### Método 2: union

```c
union EndianCheck {
    uint32_t value;
    uint8_t  bytes[4];
};

int is_little_endian(void) {
    union EndianCheck check = { .value = 1 };
    return check.bytes[0] == 1;
}
// Leer un miembro de union diferente al almacenado es
// implementation-defined en C, pero funciona en todos los
// compiladores prácticos y es idiomático en C para type-punning.
```

Ambos métodos explotan la misma idea: almacenar un valor conocido (`1`) y ver qué byte queda en la dirección más baja.

---

## 3. Cuándo importa (y cuándo NO)

```
Endianness IMPORTA cuando:
  1. Lees/escribes archivos binarios
  2. Envías/recibes datos por red
  3. Trabajas con hardware (registros mapeados en memoria)
  4. Compartes datos binarios entre plataformas diferentes
  5. Inspeccionas memoria con un debugger (GDB, xxd)

Endianness NO importa para:
  1. Aritmética normal (int x = 42; x += 1;)
  2. Operaciones de bits en el mismo tipo (shifts, AND, OR, XOR)
  3. Comparaciones
  4. Datos en texto (strings, JSON, CSV, etc.)
```

Regla clave: **las operaciones aritméticas y de bits trabajan sobre el valor abstracto del entero, no sobre sus bytes en memoria**. Endianness solo se manifiesta cuando accedes a los bytes individuales (mediante `uint8_t*`, `fwrite` directo, o inspección de memoria).

```c
uint32_t x = 0x12345678;
uint32_t y = x >> 8;       // siempre 0x00123456, en cualquier endianness
uint32_t z = x & 0xFF;     // siempre 0x78, en cualquier endianness

// Pero si miras los bytes en memoria:
uint8_t *bytes = (uint8_t *)&x;
// En LE: bytes[0]=0x78, bytes[1]=0x56, bytes[2]=0x34, bytes[3]=0x12
// En BE: bytes[0]=0x12, bytes[1]=0x34, bytes[2]=0x56, bytes[3]=0x78
```

---

## 4. Archivos binarios portables

Si usas `fwrite(&value, sizeof(value), 1, f)` directamente, los bytes en el archivo dependerán del endianness del sistema que lo escribió. Un archivo escrito en little-endian se leerá incorrectamente en big-endian.

### Serialización portable con shifts

```c
// Escritura en big-endian (el estándar más común):
void write_uint32_be(FILE *f, uint32_t val) {
    uint8_t bytes[4];
    bytes[0] = (val >> 24) & 0xFF;    // byte más significativo primero
    bytes[1] = (val >> 16) & 0xFF;
    bytes[2] = (val >> 8)  & 0xFF;
    bytes[3] =  val        & 0xFF;    // byte menos significativo último
    fwrite(bytes, 4, 1, f);
}

// Lectura desde big-endian:
uint32_t read_uint32_be(FILE *f) {
    uint8_t bytes[4];
    fread(bytes, 4, 1, f);
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           ((uint32_t)bytes[3]);
}
```

Este enfoque funciona correctamente en **cualquier** plataforma porque descompone/reconstruye el valor byte a byte usando operaciones aritméticas (que son independientes del endianness).

Versiones equivalentes para 16 bits:

```c
void write_uint16_be(FILE *f, uint16_t val) {
    uint8_t bytes[2];
    bytes[0] = (val >> 8) & 0xFF;
    bytes[1] =  val       & 0xFF;
    fwrite(bytes, 2, 1, f);
}

uint16_t read_uint16_be(FILE *f) {
    uint8_t bytes[2];
    fread(bytes, 2, 1, f);
    return ((uint16_t)bytes[0] << 8) |
           ((uint16_t)bytes[1]);
}
```

Se puede verificar con `xxd`:

```bash
xxd data.bin
# 0x01020304 aparece como: 01 02 03 04 (big-endian en el archivo)
# 0xDEADBEEF aparece como: DE AD BE EF
```

---

## 5. Protocolos de red: network byte order

Los protocolos de red (IP, TCP, UDP) usan big-endian, llamado "network byte order". Las funciones de conversión están en `<arpa/inet.h>`:

```c
#include <arpa/inet.h>

// Host → Network:
uint32_t htonl(uint32_t hostlong);    // 32 bits
uint16_t htons(uint16_t hostshort);   // 16 bits

// Network → Host:
uint32_t ntohl(uint32_t netlong);     // 32 bits
uint16_t ntohs(uint16_t netshort);    // 16 bits
```

```c
// Ejemplo: preparar un puerto TCP
uint16_t port = 8080;           // 0x1F90
uint16_t net_port = htons(port);
// En LE: bytes host = 90 1F → bytes network = 1F 90
// El valor numérico del host cambia (8080 → 36895),
// pero los bytes en memoria están ahora en el orden que la red espera.

// Ejemplo: leer una dirección IPv4 recibida por red
uint32_t host_addr = ntohl(net_addr);
```

Comportamiento:
- En una máquina **little-endian**, `htonl`/`htons` invierten los bytes
- En una máquina **big-endian**, son no-ops (no hacen nada)
- `ntohl(htonl(x)) == x` **siempre**, en cualquier plataforma
- Después de `htons()`, **no usar el valor convertido para aritmética local** — es solo para enviar por red

---

## 6. Byte swapping

### Manual

```c
uint16_t swap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

uint32_t swap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}
```

### GCC built-ins (una sola instrucción `bswap` en x86)

```c
uint16_t s16 = __builtin_bswap16(x);
uint32_t s32 = __builtin_bswap32(x);
uint64_t s64 = __builtin_bswap64(x);
```

### Linux `<endian.h>` (conversión explícita a/desde endianness específico)

```c
#include <endian.h>

uint32_t be = htobe32(host_val);    // host → big-endian 32
uint32_t le = htole32(host_val);    // host → little-endian 32
uint32_t h  = be32toh(net_val);     // big-endian 32 → host
uint32_t h2 = le32toh(le_val);     // little-endian 32 → host
// También: htobe16, htobe64, htole16, htole64, etc.
```

Diferencia con `htonl`/`ntohl`: `<endian.h>` permite conversiones explícitas a/desde **cualquier** endianness (no solo network/big-endian). Útil para leer formatos little-endian como ext4 o FAT32.

---

## 7. Endianness y structs

Los structs **no se pueden enviar directamente** por red ni a archivos binarios portables. Tres problemas:

1. **Endianness** de cada campo puede diferir entre plataformas
2. **Padding** entre campos puede variar (ver T03)
3. **`sizeof`** del struct puede variar

Solución: serializar campo por campo:

```c
struct Packet {
    uint16_t type;
    uint32_t length;
    uint16_t flags;
};

void send_packet(int fd, const struct Packet *p) {
    uint16_t type  = htons(p->type);
    uint32_t len   = htonl(p->length);
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

Nota: en un programa real, se verificarían los valores de retorno de `read`/`write`.

---

## Ejercicios

### Ejercicio 1 — Detectar y visualizar

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t x = 0xCAFEBABE;
    uint8_t *bytes = (uint8_t *)&x;

    printf("Valor: 0x%08X\n", x);
    printf("Bytes en memoria: ");
    for (int i = 0; i < 4; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");

    if (bytes[0] == 0xBE) {
        printf("Little-endian\n");
    } else {
        printf("Big-endian\n");
    }
    return 0;
}
```

**Predicción:** En tu máquina x86-64, ¿en qué orden aparecerán los 4 bytes?

<details><summary>Respuesta</summary>

```
Valor: 0xCAFEBABE
Bytes en memoria: BE BA FE CA
Little-endian
```

En little-endian, el byte menos significativo (`0xBE`) va en la dirección más baja (bytes[0]). Los bytes se "invierten" respecto a cómo escribimos el número: `BE BA FE CA` en vez de `CA FE BA BE`.

</details>

---

### Ejercicio 2 — Detección con union

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
#include <stdio.h>
#include <stdint.h>

union EndianTest {
    uint32_t word;
    uint8_t  bytes[4];
};

int main(void) {
    union EndianTest t = { .word = 0x00000001 };

    printf("word = 0x%08X\n", t.word);
    printf("bytes[0] = 0x%02X\n", t.bytes[0]);
    printf("bytes[1] = 0x%02X\n", t.bytes[1]);
    printf("bytes[2] = 0x%02X\n", t.bytes[2]);
    printf("bytes[3] = 0x%02X\n", t.bytes[3]);

    printf("Endianness: %s\n",
           t.bytes[0] == 1 ? "Little" : "Big");
    return 0;
}
```

**Predicción:** ¿En cuál de los 4 bytes estará el `01`?

<details><summary>Respuesta</summary>

```
word = 0x00000001
bytes[0] = 0x01
bytes[1] = 0x00
bytes[2] = 0x00
bytes[3] = 0x00
Endianness: Little
```

En little-endian, el byte `01` (menos significativo) está en `bytes[0]` (la dirección más baja). En big-endian estaría en `bytes[3]`.

</details>

---

### Ejercicio 3 — Byte order de uint16_t

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint16_t values[] = { 0x0001, 0xABCD, 0xFF00, 0x00FF };
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++) {
        uint8_t *b = (uint8_t *)&values[i];
        printf("0x%04X → bytes: %02X %02X\n", values[i], b[0], b[1]);
    }
    return 0;
}
```

**Predicción:** Para cada valor, ¿qué byte estará primero en memoria?

<details><summary>Respuesta</summary>

```
0x0001 → bytes: 01 00
0xABCD → bytes: CD AB
0xFF00 → bytes: 00 FF
0x00FF → bytes: FF 00
```

En little-endian, el byte menos significativo va primero. `0xABCD` tiene byte bajo `CD` y byte alto `AB`, así que en memoria se ve `CD AB`. Observa cómo `0xFF00` y `0x00FF` se "invierten" en memoria.

</details>

---

### Ejercicio 4 — htons/htonl

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

int main(void) {
    uint16_t port = 443;     // HTTPS
    uint32_t addr = 0x7F000001;  // 127.0.0.1

    uint16_t net_port = htons(port);
    uint32_t net_addr = htonl(addr);

    printf("port host: 0x%04X (%u)\n", port, port);
    printf("port net:  0x%04X (%u)\n", net_port, net_port);

    printf("addr host: 0x%08X\n", addr);
    printf("addr net:  0x%08X\n", net_addr);

    printf("\nRound-trip port: %u\n", ntohs(net_port));
    printf("Round-trip addr: 0x%08X\n", ntohl(net_addr));
    return 0;
}
```

**Predicción:** ¿Cuáles serán los valores numéricos después de `htons`/`htonl`? ¿El round-trip recupera los originales?

<details><summary>Respuesta</summary>

```
port host: 0x01BB (443)
port net:  0xBB01 (47873)
addr host: 0x7F000001
addr net:  0x0100007F
Round-trip port: 443
Round-trip addr: 0x7F000001
```

`htons(443)` = `htons(0x01BB)` → invierte bytes → `0xBB01` = 47873. El valor numérico cambia, pero los bytes en memoria ahora están en el orden que la red espera. `htonl(0x7F000001)` → `0x0100007F`. El round-trip siempre recupera el valor original: `ntohs(htons(x)) == x`.

</details>

---

### Ejercicio 5 — Swap manual de 32 bits

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex5.c -o ex5
#include <stdio.h>
#include <stdint.h>

uint32_t swap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

int main(void) {
    uint32_t vals[] = { 0x01020304, 0xDEADBEEF, 0xAABBCCDD };
    int n = sizeof(vals) / sizeof(vals[0]);

    for (int i = 0; i < n; i++) {
        uint32_t swapped = swap32(vals[i]);
        uint32_t double_swap = swap32(swapped);
        printf("0x%08X → swap → 0x%08X → swap → 0x%08X  %s\n",
               vals[i], swapped, double_swap,
               double_swap == vals[i] ? "OK" : "FAIL");
    }
    return 0;
}
```

**Predicción:** ¿Qué produce `swap32(0xDEADBEEF)`? ¿El doble swap siempre recupera el original?

<details><summary>Respuesta</summary>

```
0x01020304 → swap → 0x04030201 → swap → 0x01020304  OK
0xDEADBEEF → swap → 0xEFBEADDE → swap → 0xDEADBEEF  OK
0xAABBCCDD → swap → 0xDDCCBBAA → swap → 0xAABBCCDD  OK
```

`swap32(0xDEADBEEF)`: byte 0 `DE` → posición 3, byte 1 `AD` → posición 2, byte 2 `BE` → posición 1, byte 3 `EF` → posición 0. Resultado: `0xEFBEADDE`. El doble swap siempre recupera el original porque la inversión es su propia inversa.

</details>

---

### Ejercicio 6 — __builtin_bswap vs manual

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
#include <stdio.h>
#include <stdint.h>

uint32_t swap32_manual(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

int main(void) {
    uint32_t x = 0x12345678;

    uint32_t manual  = swap32_manual(x);
    uint32_t builtin = __builtin_bswap32(x);

    printf("Original:  0x%08X\n", x);
    printf("Manual:    0x%08X\n", manual);
    printf("Builtin:   0x%08X\n", builtin);
    printf("Iguales:   %s\n", manual == builtin ? "SI" : "NO");
    return 0;
}
```

**Predicción:** ¿Ambos métodos producen el mismo resultado? ¿Cuál es más eficiente?

<details><summary>Respuesta</summary>

```
Original:  0x12345678
Manual:    0x78563412
Builtin:   0x78563412
Iguales:   SI
```

Ambos producen `0x78563412`. `__builtin_bswap32` es más eficiente: el compilador lo traduce a una sola instrucción `bswap` en x86 (1 ciclo), mientras que la versión manual genera varias instrucciones de shift y OR. Se puede verificar con `gcc -S -O2`: la versión builtin será una instrucción, la manual será ~4 instrucciones (o el compilador reconocerá el patrón y la optimizará a `bswap` también con `-O2`).

</details>

---

### Ejercicio 7 — Escritura/lectura portable de archivo

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void write_u32_be(FILE *f, uint32_t val) {
    uint8_t bytes[4];
    bytes[0] = (val >> 24) & 0xFF;
    bytes[1] = (val >> 16) & 0xFF;
    bytes[2] = (val >> 8)  & 0xFF;
    bytes[3] =  val        & 0xFF;
    fwrite(bytes, 4, 1, f);
}

uint32_t read_u32_be(FILE *f) {
    uint8_t bytes[4];
    fread(bytes, 4, 1, f);
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  |
           ((uint32_t)bytes[3]);
}

int main(void) {
    const char *fname = "/tmp/ex7_endian.bin";
    uint32_t original = 0xCAFEBABE;

    FILE *f = fopen(fname, "wb");
    if (!f) { perror("fopen"); return 1; }
    write_u32_be(f, original);
    fclose(f);

    f = fopen(fname, "rb");
    if (!f) { perror("fopen"); return 1; }
    uint32_t recovered = read_u32_be(f);
    fclose(f);

    printf("Original:  0x%08X\n", original);
    printf("Recovered: 0x%08X\n", recovered);
    printf("Match: %s\n", original == recovered ? "YES" : "NO");

    remove(fname);
    return 0;
}
```

**Predicción:** ¿El valor recuperado coincidirá con el original? Si inspeccionas el archivo con `xxd` antes de `remove()`, ¿qué bytes verías?

<details><summary>Respuesta</summary>

```
Original:  0xCAFEBABE
Recovered: 0xCAFEBABE
Match: YES
```

El valor se recupera correctamente. Si inspeccionas con `xxd /tmp/ex7_endian.bin`, verías `CA FE BA BE` — big-endian, el byte más significativo primero. Esto es independiente de la plataforma: las funciones serializan/deserializan por valor (shifts), no por representación en memoria.

</details>

---

### Ejercicio 8 — fwrite directo vs portable

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t value = 0x01020304;
    const char *f1 = "/tmp/ex8_direct.bin";
    const char *f2 = "/tmp/ex8_portable.bin";

    // Escritura directa (NO portable)
    FILE *f = fopen(f1, "wb");
    fwrite(&value, sizeof(value), 1, f);
    fclose(f);

    // Escritura portable (big-endian)
    f = fopen(f2, "wb");
    uint8_t bytes[4] = {
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8)  & 0xFF,
         value        & 0xFF
    };
    fwrite(bytes, 4, 1, f);
    fclose(f);

    // Leer y mostrar bytes de cada archivo
    for (int which = 0; which < 2; which++) {
        const char *name = which == 0 ? f1 : f2;
        f = fopen(name, "rb");
        uint8_t buf[4];
        fread(buf, 4, 1, f);
        fclose(f);

        printf("%s: %02X %02X %02X %02X\n",
               which == 0 ? "Direct  " : "Portable",
               buf[0], buf[1], buf[2], buf[3]);
    }

    remove(f1);
    remove(f2);
    return 0;
}
```

**Predicción:** ¿Los bytes en ambos archivos serán iguales o diferentes? ¿Por qué?

<details><summary>Respuesta</summary>

```
Direct  : 04 03 02 01
Portable: 01 02 03 04
```

Son **diferentes**. `fwrite(&value, ...)` copia los bytes tal como están en memoria. En little-endian, `0x01020304` se almacena como `04 03 02 01` en memoria, y así se escriben al archivo. La versión portable descompone con shifts y siempre escribe `01 02 03 04` (big-endian), independientemente de la plataforma. El archivo directo solo se leería correctamente en otra máquina little-endian.

</details>

---

### Ejercicio 9 — Endianness y operaciones de bits

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t x = 0xAABBCCDD;

    uint32_t shifted = x >> 16;
    uint32_t masked  = x & 0x0000FF00;
    uint32_t ored    = x | 0x00000001;

    printf("x        = 0x%08X\n", x);
    printf("x >> 16  = 0x%08X\n", shifted);
    printf("x & FF00 = 0x%08X\n", masked);
    printf("x | 0001 = 0x%08X\n", ored);

    // Ahora veamos los bytes en memoria de x
    uint8_t *bytes = (uint8_t *)&x;
    printf("\nBytes de x en memoria: %02X %02X %02X %02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    printf("¿El shift cambia los bytes en memoria?\n");
    bytes = (uint8_t *)&shifted;
    printf("Bytes de shifted:      %02X %02X %02X %02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    return 0;
}
```

**Predicción:** ¿Los resultados de las operaciones dependen del endianness? ¿Y los bytes en memoria?

<details><summary>Respuesta</summary>

```
x        = 0xAABBCCDD
x >> 16  = 0x0000AABB
x & FF00 = 0x0000CC00
x | 0001 = 0xAABBCCDD

Bytes de x en memoria: DD CC BB AA
¿El shift cambia los bytes en memoria?
Bytes de shifted:      BB AA 00 00
```

Los **resultados de las operaciones** son los mismos en cualquier endianness: `x >> 16` siempre es `0x0000AABB`. Las operaciones trabajan sobre el valor, no sobre los bytes. Pero la **representación en memoria** sí depende del endianness: `0x0000AABB` se almacena como `BB AA 00 00` en little-endian (byte menos significativo primero).

</details>

---

### Ejercicio 10 — Serializar un struct

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

struct Header {
    uint16_t version;
    uint32_t payload_size;
    uint16_t flags;
};

int main(void) {
    struct Header h = { .version = 1, .payload_size = 1024, .flags = 0x00FF };

    // Serializar campo por campo en network byte order
    uint8_t wire[8];  // 2 + 4 + 2 = 8 bytes
    uint16_t nv = htons(h.version);
    uint32_t np = htonl(h.payload_size);
    uint16_t nf = htons(h.flags);
    memcpy(wire + 0, &nv, 2);
    memcpy(wire + 2, &np, 4);
    memcpy(wire + 6, &nf, 2);

    printf("Serialized (%zu bytes): ", sizeof(wire));
    for (int i = 0; i < 8; i++) {
        printf("%02X ", wire[i]);
    }
    printf("\n");

    // Deserializar
    struct Header h2;
    memcpy(&nv, wire + 0, 2); h2.version      = ntohs(nv);
    memcpy(&np, wire + 2, 4); h2.payload_size  = ntohl(np);
    memcpy(&nf, wire + 6, 2); h2.flags         = ntohs(nf);

    printf("Deserialized: version=%u, size=%u, flags=0x%04X\n",
           h2.version, h2.payload_size, h2.flags);
    printf("Match: %s\n",
           h.version == h2.version &&
           h.payload_size == h2.payload_size &&
           h.flags == h2.flags ? "YES" : "NO");
    return 0;
}
```

**Predicción:** ¿Cuántos bytes ocupa el struct en memoria vs el buffer serializado? ¿Los 8 bytes serializados serán los mismos en LE y BE?

<details><summary>Respuesta</summary>

```
Serialized (8 bytes): 00 01 00 00 04 00 00 FF
Deserialized: version=1, size=1024, flags=0x00FF
Match: YES
```

El struct `Header` en memoria ocupa **12 bytes** (padding: 2 de version + **2 padding** + 4 de payload_size + 2 de flags + **2 tail padding** = 12). Pero el buffer serializado ocupa exactamente **8 bytes** (sin padding). Los 8 bytes serializados son idénticos en cualquier plataforma porque la serialización usa `htons`/`htonl` para normalizar a network byte order (big-endian). `version=1` → `00 01`, `payload_size=1024` (0x400) → `00 00 04 00`, `flags=0x00FF` → `00 FF`.

</details>
