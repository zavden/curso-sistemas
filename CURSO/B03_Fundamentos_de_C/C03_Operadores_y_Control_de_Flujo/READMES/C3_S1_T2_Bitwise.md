# T02 — Operadores bitwise

## Erratas detectadas

| Ubicación | Error | Corrección |
|---|---|---|
| README.md línea 130 | `~x = 0xFFFFFF0 (en 32-bit int)` — falta un dígito hex | Debe ser `0xFFFFFFF0`. `~0x0F` en un `int` de 32 bits = `0xFFFFFFF0` (ocho dígitos hex, 32 bits), no `0xFFFFFF0` (siete dígitos, 28 bits) |

---

## 1. Los seis operadores bitwise

Los operadores bitwise trabajan sobre los **bits individuales** de un valor. Son fundamentales para flags, máscaras, protocolos y programación de bajo nivel:

| Operador | Nombre | Descripción |
|---|---|---|
| `&` | AND | 1 solo si ambos bits son 1 |
| `\|` | OR | 1 si al menos uno es 1 |
| `^` | XOR | 1 si los bits son diferentes |
| `~` | NOT | Invierte todos los bits |
| `<<` | Shift left | Desplaza bits a la izquierda, rellena con 0 |
| `>>` | Shift right | Desplaza bits a la derecha |

---

## 2. AND, OR, XOR, NOT

### AND (&) — extraer bits

```
  1010 1100
& 1111 0000
──────────
  1010 0000
```

```c
uint8_t byte = 0b11010110;
uint8_t low_nibble  = byte & 0x0F;   // 0b00000110 = 6
uint8_t high_nibble = byte & 0xF0;   // 0b11010000 = 0xD0

// Verificar si un bit está encendido:
if (byte & 0x04) {   // bit 2
    // está encendido
}

// Verificar paridad:
if ((n & 1) == 0) {
    // n es par
}
```

### OR (|) — encender bits

```
  1010 0000
| 0000 1100
──────────
  1010 1100
```

```c
uint8_t flags = 0;
flags |= 0x01;     // encender bit 0
flags |= 0x04;     // encender bit 2
// flags = 0b00000101 = 0x05
```

### XOR (^) — invertir bits

```
  1010 1100
^ 1111 0000
──────────
  0101 1100
```

```c
// Propiedades de XOR:
// a ^ a == 0        (cualquier valor XOR consigo mismo = 0)
// a ^ 0 == a        (XOR con 0 no cambia nada)
// a ^ b ^ b == a    (XOR doble cancela)

// Toggle (invertir un bit):
flags ^= 0x04;     // si bit 2 era 1 → 0, si era 0 → 1

// Swap sin variable temporal (curiosidad, no usar en código real):
int a = 5, b = 3;
a ^= b;     // a = 5^3 = 6
b ^= a;     // b = 3^6 = 5
a ^= b;     // a = 6^5 = 3
```

### NOT (~) — invertir todos los bits

```
~ 1010 1100
──────────
  0101 0011
```

```c
// Uso principal: crear máscara inversa para APAGAR bits
flags &= ~0x04;    // apagar bit 2
// ~0x04 = 0b...11111011 → AND apaga solo ese bit

// CUIDADO con el tipo: ~ promueve a int primero
unsigned char x = 0x0F;
// ~x = 0xFFFFFFF0 (en int de 32 bits), no 0xF0
// Importa si asignas a un tipo más grande
```

---

## 3. Shifts (<< y >>)

### Shift left (<<)

Desplaza bits a la izquierda, rellenando con ceros por la derecha. Equivale a multiplicar por 2^N:

```c
unsigned int x = 10;      // 0000 1010
x << 1    // 0001 0100 = 20  (×2)
x << 2    // 0010 1000 = 40  (×4)
x << 3    // 0101 0000 = 80  (×8)

// Crear máscara con un solo bit encendido:
1 << 0    // 0b00000001 = bit 0
1 << 3    // 0b00001000 = bit 3
1 << 7    // 0b10000000 = bit 7
1U << 31  // bit 31 (usar 1U para evitar UB con signed)
```

**Peligros del shift left:**

```c
// Shift >= ancho del tipo → UB
int bad = 1 << 32;     // UB si int es 32 bits

// Shift de valor negativo → UB (en C11)
int bad2 = -1 << 2;    // UB

// Shift left de signed que produce overflow → UB
int bad3 = 1 << 31;    // UB: 1 × 2³¹ no cabe en int (signed)
unsigned int ok = 1U << 31;  // OK: unsigned no tiene overflow
```

### Shift right (>>)

Desplaza bits a la derecha. Para unsigned, equivale a dividir por 2^N:

```c
unsigned int x = 80;      // 0101 0000
x >> 1    // 0010 1000 = 40  (÷2)
x >> 2    // 0001 0100 = 20  (÷4)
x >> 3    // 0000 1010 = 10  (÷8)
```

El relleno depende del tipo:
- **Unsigned:** se rellena con ceros (logical shift)
- **Signed:** implementation-defined. En la práctica (GCC/x86): arithmetic shift (rellena con el bit de signo)

```c
int neg = -8;             // 1111...1000
int shifted = neg >> 1;   // 1111...1100 = -4 (arithmetic shift en GCC)
// NO garantizado por el estándar — usar unsigned para portabilidad
```

---

## 4. Flags con bits

Patrón clásico para representar opciones/estados compactos:

```c
// Definir flags como potencias de 2:
#define FLAG_READ    (1U << 0)    // 0b0001 = 1
#define FLAG_WRITE   (1U << 1)    // 0b0010 = 2
#define FLAG_EXEC    (1U << 2)    // 0b0100 = 4
#define FLAG_HIDDEN  (1U << 3)    // 0b1000 = 8

unsigned int perms = 0;
```

Las cuatro operaciones fundamentales:

| Operación | Código | Efecto |
|---|---|---|
| **Set** (encender) | `perms \|= FLAG_READ;` | Enciende el bit sin tocar los demás |
| **Clear** (apagar) | `perms &= ~FLAG_WRITE;` | NOT invierte la máscara, AND apaga solo ese bit |
| **Toggle** (invertir) | `perms ^= FLAG_HIDDEN;` | Si era 0→1, si era 1→0 |
| **Check** (verificar) | `if (perms & FLAG_READ)` | Distinto de cero si el bit está encendido |

Verificar múltiples flags:

```c
// TODOS deben estar presentes:
if ((perms & (FLAG_READ | FLAG_WRITE)) == (FLAG_READ | FLAG_WRITE)) {
    // lectura Y escritura
}

// AL MENOS uno presente:
if (perms & (FLAG_READ | FLAG_WRITE)) {
    // lectura O escritura (o ambas)
}
```

---

## 5. Permisos Unix — caso real

Los permisos de archivo Unix son un caso real de flags bitwise: 9 bits codifican `rwxrwxrwx` (owner/group/other):

```c
#define S_IRUSR  (1 << 8)    // 0400 — owner read
#define S_IWUSR  (1 << 7)    // 0200 — owner write
#define S_IXUSR  (1 << 6)    // 0100 — owner execute
#define S_IRGRP  (1 << 5)    // 0040 — group read
#define S_IWGRP  (1 << 4)    // 0020 — group write
#define S_IXGRP  (1 << 3)    // 0010 — group execute
#define S_IROTH  (1 << 2)    // 0004 — other read
#define S_IWOTH  (1 << 1)    // 0002 — other write
#define S_IXOTH  (1 << 0)    // 0001 — other execute
```

`chmod 755` = owner rwx (7=111) + group r-x (5=101) + other r-x (5=101). Cada dígito octal codifica exactamente 3 bits.

---

## 6. Máscaras para extraer campos

Para valores empaquetados, combinar shift y AND:

```c
// Color RGB565 empaquetado en 16 bits: RRRR RGGG GGGB BBBB
uint16_t pixel = 0xF81F;   // rojo + azul

uint8_t red   = (pixel >> 11) & 0x1F;   // 5 bits superiores
uint8_t green = (pixel >>  5) & 0x3F;   // 6 bits medios
uint8_t blue  =  pixel        & 0x1F;   // 5 bits inferiores

// Construir un pixel:
uint16_t new_pixel = ((uint16_t)red << 11)
                   | ((uint16_t)green << 5)
                   | blue;
```

---

## 7. Trucos clásicos con bits

### Potencia de 2

```c
// Una potencia de 2 tiene exactamente un bit encendido:
// 8 = 0b1000, 8-1 = 0b0111 → 8 & 7 = 0
int is_power_of_2(unsigned int n) {
    return n != 0 && (n & (n - 1)) == 0;
}
```

### Population count (contar bits encendidos)

```c
// Versión simple:
int popcount(uint32_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// Versión Brian Kernighan (más rápida — solo itera tantas veces como bits hay):
int popcount_fast(uint32_t x) {
    int count = 0;
    while (x) {
        x &= x - 1;    // apaga el bit menos significativo
        count++;
    }
    return count;
}

// GCC built-in (usa instrucción POPCNT si disponible):
int n = __builtin_popcount(x);
```

### Swap de bytes (cambiar endianness)

```c
uint32_t swap_bytes(uint32_t x) {
    return ((x >> 24) & 0x000000FF)
         | ((x >>  8) & 0x0000FF00)
         | ((x <<  8) & 0x00FF0000)
         | ((x << 24) & 0xFF000000);
}
```

---

## 8. Tabla resumen

| Operación sobre bits | Operador | Ejemplo | Resultado |
|---|---|---|---|
| AND | `&` | `0b1100 & 0b1010` | `0b1000` |
| OR | `\|` | `0b1100 \| 0b1010` | `0b1110` |
| XOR | `^` | `0b1100 ^ 0b1010` | `0b0110` |
| NOT | `~` | `~0b1100` (8 bits) | `0b...0011` |
| Shift left | `<<` | `0b0001 << 3` | `0b1000` |
| Shift right | `>>` | `0b1000 >> 2` | `0b0010` |

| Operación sobre flags | Código |
|---|---|
| Set bit | `flags \|= BIT` |
| Clear bit | `flags &= ~BIT` |
| Toggle bit | `flags ^= BIT` |
| Test bit | `if (flags & BIT)` |

---

## Ejercicios

### Ejercicio 1 — Operaciones bit a bit básicas

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t a = 0b11001010;   // 0xCA
    uint8_t b = 0b10110011;   // 0xB3

    printf("a & b  = 0x%02X\n", (uint8_t)(a & b));
    printf("a | b  = 0x%02X\n", (uint8_t)(a | b));
    printf("a ^ b  = 0x%02X\n", (uint8_t)(a ^ b));
    printf("~a     = 0x%02X\n", (uint8_t)~a);

    return 0;
}
```

**Calcular cada resultado en binario y luego en hex.**

<details>
<summary>Predicción</summary>

```
a       = 1100 1010
b       = 1011 0011

a & b   = 1000 0010 = 0x82   (1 solo donde ambos son 1)
a | b   = 1111 1011 = 0xFB   (1 donde al menos uno es 1)
a ^ b   = 0111 1001 = 0x79   (1 donde difieren)
~a      = 0011 0101 = 0x35   (invertir todos)
```

Salida:
```
a & b  = 0x82
a | b  = 0xFB
a ^ b  = 0x79
~a     = 0x35
```

</details>

---

### Ejercicio 2 — Shifts como multiplicación/división

```c
#include <stdio.h>

int main(void) {
    unsigned int x = 13;

    printf("x      = %u\n", x);
    printf("x << 1 = %u\n", x << 1);
    printf("x << 3 = %u\n", x << 3);
    printf("x >> 1 = %u\n", x >> 1);

    unsigned int y = 100;
    printf("y >> 3 = %u\n", y >> 3);

    return 0;
}
```

**¿Cuánto vale cada expresión? Para `y >> 3`, ¿se pierde información?**

<details>
<summary>Predicción</summary>

```
x      = 13
x << 1 = 26     // 13 × 2
x << 3 = 104    // 13 × 8
x >> 1 = 6      // 13 / 2 = 6 (trunca — se pierde el bit menos significativo)
y >> 3 = 12     // 100 / 8 = 12 (trunca — 100 = 12×8 + 4, se pierden los 3 bits bajos)
```

Sí, `13 >> 1` pierde el bit 0 (13 = 0b1101, el 1 final se descarta → 0b110 = 6). Y `100 >> 3` pierde los 3 bits bajos (100 en binario = 1100100, los últimos 3 bits `100` = 4 se pierden).

</details>

---

### Ejercicio 3 — Manipulación de flags

```c
#include <stdio.h>

#define BOLD      (1U << 0)
#define ITALIC    (1U << 1)
#define UNDERLINE (1U << 2)
#define STRIKE    (1U << 3)

int main(void) {
    unsigned int style = 0;

    style |= BOLD;
    style |= ITALIC;
    style |= UNDERLINE;
    printf("After set: 0x%02X\n", style);

    style &= ~ITALIC;
    printf("After clear ITALIC: 0x%02X\n", style);

    style ^= STRIKE;
    printf("After toggle STRIKE: 0x%02X\n", style);

    style ^= BOLD;
    printf("After toggle BOLD: 0x%02X\n", style);

    printf("Has UNDERLINE? %s\n", (style & UNDERLINE) ? "yes" : "no");
    printf("Has BOLD? %s\n", (style & BOLD) ? "yes" : "no");

    return 0;
}
```

**Seguir cada operación. ¿Qué flags están encendidos al final?**

<details>
<summary>Predicción</summary>

Paso a paso (bits: `SIUB` = Strike, Italic, Underline, Bold):

1. Set BOLD: `0001` → 0x01
2. Set ITALIC: `0011` → 0x03
3. Set UNDERLINE: `0111` → 0x07

```
After set: 0x07         // UNDERLINE + ITALIC + BOLD
```

4. Clear ITALIC: `0111 & ~0010 = 0111 & 1101 = 0101` → 0x05

```
After clear ITALIC: 0x05   // UNDERLINE + BOLD
```

5. Toggle STRIKE (estaba off → on): `0101 ^ 1000 = 1101` → 0x0D

```
After toggle STRIKE: 0x0D  // STRIKE + UNDERLINE + BOLD
```

6. Toggle BOLD (estaba on → off): `1101 ^ 0001 = 1100` → 0x0C

```
After toggle BOLD: 0x0C    // STRIKE + UNDERLINE
Has UNDERLINE? yes
Has BOLD? no
```

Al final: STRIKE y UNDERLINE encendidos, ITALIC y BOLD apagados.

</details>

---

### Ejercicio 4 — Extraer nibbles

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t byte = 0xA7;

    uint8_t low  = byte & 0x0F;
    uint8_t high = (byte >> 4) & 0x0F;

    printf("byte = 0x%02X\n", byte);
    printf("low  = 0x%X (%u)\n", low, low);
    printf("high = 0x%X (%u)\n", high, high);

    // Reconstruir:
    uint8_t rebuilt = (high << 4) | low;
    printf("rebuilt = 0x%02X\n", rebuilt);

    return 0;
}
```

**¿Cuáles son los nibbles? ¿La reconstrucción da el valor original?**

<details>
<summary>Predicción</summary>

```
byte = 0xA7           // 1010 0111
low  = 0x7 (7)        // 0xA7 & 0x0F = 0x07 → nibble bajo
high = 0xA (10)       // 0xA7 >> 4 = 0x0A, & 0x0F = 0x0A → nibble alto
rebuilt = 0xA7         // (0x0A << 4) | 0x07 = 0xA0 | 0x07 = 0xA7 ✓
```

La reconstrucción siempre da el valor original porque los nibbles cubren los 8 bits sin solaparse.

</details>

---

### Ejercicio 5 — Potencia de 2

```c
#include <stdio.h>

int is_power_of_2(unsigned int n) {
    return n != 0 && (n & (n - 1)) == 0;
}

int main(void) {
    unsigned int tests[] = {0, 1, 2, 3, 4, 7, 8, 16, 255, 256};
    int count = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < count; i++) {
        printf("%3u: %s\n", tests[i], is_power_of_2(tests[i]) ? "yes" : "no");
    }

    return 0;
}
```

**¿Cuáles son potencia de 2? ¿Por qué funciona `n & (n-1)`?**

<details>
<summary>Predicción</summary>

```
  0: no     // excluido por n != 0
  1: yes    // 2⁰ = 1   → 0b0001 & 0b0000 = 0
  2: yes    // 2¹ = 2   → 0b0010 & 0b0001 = 0
  3: no     //           → 0b0011 & 0b0010 = 0b0010 ≠ 0
  4: yes    // 2² = 4   → 0b0100 & 0b0011 = 0
  7: no     //           → 0b0111 & 0b0110 = 0b0110 ≠ 0
  8: yes    // 2³ = 8
 16: yes    // 2⁴ = 16
255: no     // 0xFF = 8 bits encendidos
256: yes    // 2⁸ = 256
```

`n & (n-1)` funciona porque una potencia de 2 tiene exactamente **un bit** encendido. Al restarle 1, ese bit se apaga y todos los bits inferiores se encienden. El AND de esos dos patrones siempre es 0. Si `n` tiene más de un bit encendido, `n-1` solo afecta al bit menos significativo, dejando los demás intactos → el AND no es 0.

</details>

---

### Ejercicio 6 — Construir permisos Unix

```c
#include <stdio.h>

#define R_USR  (1U << 8)
#define W_USR  (1U << 7)
#define X_USR  (1U << 6)
#define R_GRP  (1U << 5)
#define W_GRP  (1U << 4)
#define X_GRP  (1U << 3)
#define R_OTH  (1U << 2)
#define W_OTH  (1U << 1)
#define X_OTH  (1U << 0)

int main(void) {
    // Construir 0644 (rw-r--r--)
    unsigned int p = R_USR | W_USR | R_GRP | R_OTH;
    printf("0644: 0%03o\n", p);

    // Agregar permiso de ejecución para owner → 0744
    p |= X_USR;
    printf("+x:   0%03o\n", p);

    // Quitar write de owner → 0544
    p &= ~W_USR;
    printf("-w:   0%03o\n", p);

    return 0;
}
```

**¿Qué valor octal se imprime en cada paso?**

<details>
<summary>Predicción</summary>

```
0644: 0644    // rw-r--r-- = 110 100 100
+x:   0744    // rwxr--r-- = 111 100 100 (se encendió bit 6)
-w:   0544    // r-xr--r-- = 101 100 100 (se apagó bit 7)
```

Verificación de 0644: `R_USR(256) + W_USR(128) + R_GRP(32) + R_OTH(4) = 420 decimal = 0644 octal` ✓

</details>

---

### Ejercicio 7 — XOR swap y propiedades

```c
#include <stdio.h>

int main(void) {
    int a = 42, b = 17;

    printf("Before: a=%d, b=%d\n", a, b);

    a ^= b;
    printf("After a^=b:  a=%d, b=%d\n", a, b);

    b ^= a;
    printf("After b^=a:  a=%d, b=%d\n", a, b);

    a ^= b;
    printf("After a^=b:  a=%d, b=%d\n", a, b);

    // Propiedades
    int x = 0xFF;
    printf("\nx ^ x = %d\n", x ^ x);
    printf("x ^ 0 = 0x%X\n", x ^ 0);
    printf("x ^ x ^ x = 0x%X\n", x ^ x ^ x);

    return 0;
}
```

**Seguir cada paso del swap. ¿Qué valen `a` y `b` en cada línea?**

<details>
<summary>Predicción</summary>

```
Before: a=42, b=17
After a^=b:  a=59, b=17     // a = 42^17 = 59 (almacena la "diferencia")
After b^=a:  a=59, b=42     // b = 17^59 = 42 (recupera el valor original de a)
After a^=b:  a=17, b=42     // a = 59^42 = 17 (recupera el valor original de b)
```

Funciona porque XOR es su propia inversa: `a ^ b ^ b = a`. Pero en código real, usar una variable temporal es más claro y no más lento (el compilador optimiza ambos igual).

```
x ^ x = 0            // todo bit cancela consigo mismo
x ^ 0 = 0xFF         // XOR con 0 no cambia nada
x ^ x ^ x = 0xFF     // (x^x)=0, 0^x = x
```

</details>

---

### Ejercicio 8 — Máscara para campo empaquetado

```c
#include <stdio.h>
#include <stdint.h>

// Formato: [edad:7 bits][departamento:4 bits][activo:1 bit] = 12 bits
// Bits:     11-5              4-1                 0

uint16_t pack(uint8_t age, uint8_t dept, uint8_t active) {
    return ((uint16_t)(age & 0x7F) << 5)
         | ((uint16_t)(dept & 0x0F) << 1)
         | (active & 0x01);
}

int main(void) {
    uint16_t record = pack(30, 5, 1);
    printf("packed: 0x%04X\n", record);

    // Desempaquetar:
    uint8_t age    = (record >> 5) & 0x7F;
    uint8_t dept   = (record >> 1) & 0x0F;
    uint8_t active = record & 0x01;

    printf("age=%u, dept=%u, active=%u\n", age, dept, active);

    return 0;
}
```

**¿Qué valor hex tiene `record`? Verificar el desempaquetado.**

<details>
<summary>Predicción</summary>

Empaquetado:
- `age = 30`: 30 & 0x7F = 30 = 0b0011110. Shift << 5 → 0b0000 0011 1100 0000 = 0x03C0
- `dept = 5`: 5 & 0x0F = 5 = 0b0101. Shift << 1 → 0b0000 0000 0000 1010 = 0x000A
- `active = 1`: 1 & 0x01 = 1 = 0b0000 0000 0000 0001 = 0x0001
- OR: 0x03C0 | 0x000A | 0x0001 = 0x03CB

```
packed: 0x03CB
```

Desempaquetado:
- `age = (0x03CB >> 5) & 0x7F = 0x1E & 0x7F = 30` ✓
- `dept = (0x03CB >> 1) & 0x0F = 0x1E5 & 0x0F = 5` ✓
- `active = 0x03CB & 0x01 = 1` ✓

```
age=30, dept=5, active=1
```

</details>

---

### Ejercicio 9 — Contar bits encendidos

```c
#include <stdio.h>
#include <stdint.h>

int popcount(uint32_t x) {
    int count = 0;
    while (x) {
        x &= x - 1;
        count++;
    }
    return count;
}

int main(void) {
    printf("popcount(0)    = %d\n", popcount(0));
    printf("popcount(1)    = %d\n", popcount(1));
    printf("popcount(7)    = %d\n", popcount(7));
    printf("popcount(255)  = %d\n", popcount(255));
    printf("popcount(256)  = %d\n", popcount(256));

    return 0;
}
```

**¿Cuántos bits tiene cada valor? ¿Cuántas iteraciones hace el loop para `255`?**

<details>
<summary>Predicción</summary>

```
popcount(0)    = 0     // 0b0         → 0 bits, 0 iteraciones
popcount(1)    = 1     // 0b1         → 1 bit
popcount(7)    = 3     // 0b111       → 3 bits
popcount(255)  = 8     // 0b11111111  → 8 bits, 8 iteraciones
popcount(256)  = 1     // 0b100000000 → 1 bit, 1 iteración
```

La versión Brian Kernighan itera exactamente tantas veces como bits encendidos hay. Para `255` (8 bits), hace 8 iteraciones. Para `256` (1 solo bit), hace solo 1 iteración. Esto es más eficiente que la versión simple que siempre recorre los 32 bits.

</details>

---

### Ejercicio 10 — Redondear a siguiente potencia de 2

```c
#include <stdio.h>
#include <stdint.h>

uint32_t next_power_of_2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

int main(void) {
    printf("next_pow2(0)   = %u\n", next_power_of_2(0));
    printf("next_pow2(1)   = %u\n", next_power_of_2(1));
    printf("next_pow2(5)   = %u\n", next_power_of_2(5));
    printf("next_pow2(8)   = %u\n", next_power_of_2(8));
    printf("next_pow2(100) = %u\n", next_power_of_2(100));

    return 0;
}
```

**¿Qué valor retorna para cada entrada? ¿Por qué la serie de OR con shifts funciona?**

<details>
<summary>Predicción</summary>

```
next_pow2(0)   = 0     // 0-1 = UINT32_MAX, los OR no cambian nada, +1 = 0 (wrap)
next_pow2(1)   = 1     // 1-1=0, OR con 0 = 0, +1 = 1
next_pow2(5)   = 8     // 5-1=4=0b100, OR propaga → 0b111=7, +1 = 8
next_pow2(8)   = 8     // 8-1=7=0b111, OR propaga → 0b111=7, +1 = 8
next_pow2(100) = 128   // 100-1=99=0b1100011, OR propaga → 0b1111111=127, +1 = 128
```

La serie de OR con shifts funciona porque "propaga" el bit más significativo hacia la derecha, llenando de 1s todas las posiciones por debajo. El `n--` al inicio maneja el caso donde `n` ya es potencia de 2 (evita devolver la siguiente). El `n++` al final convierte el patrón `0b0...01...1` en la siguiente potencia de 2.

Nota: `next_power_of_2(0)` retorna 0 por wrapping de unsigned. En uso real, verificar `n == 0` antes.

</details>
