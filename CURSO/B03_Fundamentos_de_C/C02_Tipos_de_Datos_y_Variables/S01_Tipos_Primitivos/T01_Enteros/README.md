# T01 — Enteros

## Los tipos enteros de C

C tiene varios tipos enteros de diferentes tamaños. El estándar
**no garantiza** tamaños exactos — solo garantiza tamaños mínimos:

```c
// Tipos enteros con signo (de menor a mayor):
char          // al menos 8 bits
short         // al menos 16 bits
int           // al menos 16 bits (casi siempre 32)
long          // al menos 32 bits
long long     // al menos 64 bits (desde C99)

// Cada uno tiene su versión unsigned:
unsigned char
unsigned short
unsigned int       // o simplemente: unsigned
unsigned long
unsigned long long
```

### Tamaños típicos (no garantizados)

```c
#include <stdio.h>

int main(void) {
    printf("char:      %zu bytes\n", sizeof(char));       // 1
    printf("short:     %zu bytes\n", sizeof(short));      // 2
    printf("int:       %zu bytes\n", sizeof(int));        // 4
    printf("long:      %zu bytes\n", sizeof(long));       // 4 u 8
    printf("long long: %zu bytes\n", sizeof(long long));  // 8
    return 0;
}
```

```
Tamaños en las plataformas más comunes:

| Tipo       | ILP32 (32-bit) | LP64 (Linux 64-bit) | LLP64 (Windows 64-bit) |
|------------|----------------|----------------------|------------------------|
| char       | 1              | 1                    | 1                      |
| short      | 2              | 2                    | 2                      |
| int        | 4              | 4                    | 4                      |
| long       | 4              | 8                    | 4                      |
| long long  | 8              | 8                    | 8                      |
| puntero    | 4              | 8                    | 8                      |

La diferencia clave: long es 8 bytes en Linux 64-bit pero
4 bytes en Windows 64-bit. Por eso no confiar en sizeof(long).
```

### Lo que SÍ garantiza el estándar

```c
// El estándar garantiza esta jerarquía de tamaños:
// sizeof(char) == 1  (siempre, por definición)
// sizeof(char) <= sizeof(short) <= sizeof(int) <= sizeof(long) <= sizeof(long long)

// Y estos mínimos:
// char:      al menos 8 bits
// short:     al menos 16 bits
// int:       al menos 16 bits
// long:      al menos 32 bits
// long long: al menos 64 bits

// Pero int puede ser 16, 32 o 64 bits según la plataforma.
// Asumir sizeof(int) == 4 es peligroso en código portable.
```

## char — El tipo más pequeño

```c
// char siempre tiene sizeof(char) == 1 (por definición).
// Es el tipo mínimo addressable.

// TRAMPA: ¿char es signed o unsigned?
// Implementation-defined. Depende de la plataforma:
// - x86 (GCC/Clang): char es signed (-128 a 127)
// - ARM (GCC default): char puede ser unsigned (0 a 255)

char c = 200;
// En signed char: overflow → valor negativo (UB técnicamente)
// En unsigned char: 200 → OK

// Si necesitas un comportamiento específico, ser explícito:
signed char   sc = -50;     // siempre signed
unsigned char uc = 200;     // siempre unsigned

// Para datos de bytes (buffers, I/O binario):
unsigned char buffer[1024];  // o uint8_t
```

```c
// char como entero vs como carácter:
char letter = 'A';           // valor ASCII: 65
char digit  = '7';           // valor ASCII: 55

// Aritmética con char:
char next = 'A' + 1;         // 'B'
int diff = 'Z' - 'A';        // 25

// Verificar si es letra minúscula (ASCII):
if (c >= 'a' && c <= 'z') {
    // es minúscula
}

// Convertir a mayúscula:
char upper = c - 'a' + 'A';   // o usar toupper() de <ctype.h>
```

## Rangos de valores

```c
#include <limits.h>

// limits.h define los rangos de cada tipo:
printf("char:      %d a %d\n", CHAR_MIN, CHAR_MAX);
printf("short:     %d a %d\n", SHRT_MIN, SHRT_MAX);
printf("int:       %d a %d\n", INT_MIN, INT_MAX);
printf("long:      %ld a %ld\n", LONG_MIN, LONG_MAX);
printf("long long: %lld a %lld\n", LLONG_MIN, LLONG_MAX);

printf("unsigned char:      0 a %u\n", UCHAR_MAX);
printf("unsigned short:     0 a %u\n", USHRT_MAX);
printf("unsigned int:       0 a %u\n", UINT_MAX);
printf("unsigned long:      0 a %lu\n", ULONG_MAX);
printf("unsigned long long: 0 a %llu\n", ULLONG_MAX);
```

```
Rangos típicos en plataforma de 64 bits:

| Tipo           | Mínimo                | Máximo                |
|----------------|----------------------|----------------------|
| signed char    | -128                 | 127                  |
| unsigned char  | 0                    | 255                  |
| short          | -32,768              | 32,767               |
| unsigned short | 0                    | 65,535               |
| int            | -2,147,483,648       | 2,147,483,647        |
| unsigned int   | 0                    | 4,294,967,295        |
| long (LP64)    | -9.2×10^18           | 9.2×10^18            |
| long long      | -9.2×10^18           | 9.2×10^18            |
```

## stdint.h — Tipos de tamaño fijo

Cuando el tamaño importa, **no usar** `int` o `long`. Usar los
tipos de `<stdint.h>` (disponibles desde C99):

```c
#include <stdint.h>

// Tipos de TAMAÑO EXACTO:
int8_t    a;    // exactamente 8 bits con signo
int16_t   b;    // exactamente 16 bits con signo
int32_t   c;    // exactamente 32 bits con signo
int64_t   d;    // exactamente 64 bits con signo

uint8_t   e;    // exactamente 8 bits sin signo
uint16_t  f;    // exactamente 16 bits sin signo
uint32_t  g;    // exactamente 32 bits sin signo
uint64_t  h;    // exactamente 64 bits sin signo
```

```c
// Tipos de TAMAÑO MÍNIMO (siempre existen):
int_least8_t   a;    // al menos 8 bits
int_least16_t  b;    // al menos 16 bits
int_least32_t  c;    // al menos 32 bits
int_least64_t  d;    // al menos 64 bits
// Pueden ser más grandes si la plataforma no tiene el tamaño exacto

// Tipos RÁPIDOS (el más eficiente con al menos N bits):
int_fast8_t    a;    // el int más rápido con al menos 8 bits
int_fast16_t   b;    // el int más rápido con al menos 16 bits
int_fast32_t   c;    // el int más rápido con al menos 32 bits
int_fast64_t   d;    // el int más rápido con al menos 64 bits
// Pueden ser más grandes que lo pedido si eso es más eficiente
```

```c
// Tipos especiales:
intmax_t  m;    // el tipo entero más grande disponible
uintmax_t n;    // lo mismo, unsigned

intptr_t  p;    // entero del tamaño de un puntero (puede guardar un void*)
uintptr_t q;    // lo mismo, unsigned
// intptr_t es del tamaño correcto para la plataforma:
// 4 bytes en 32-bit, 8 bytes en 64-bit

size_t    s;    // unsigned, resultado de sizeof, tamaño de objetos
// size_t es el tipo correcto para índices y tamaños
ptrdiff_t d;    // signed, resultado de restar punteros
```

### Constantes y macros de printf para stdint.h

```c
#include <stdint.h>
#include <inttypes.h>    // macros de formato para printf

// Constantes:
int32_t x = INT32_MAX;        // 2147483647
uint64_t y = UINT64_MAX;      // 18446744073709551615
int8_t z = INT8_MIN;          // -128

// printf con tipos de stdint.h — usar macros de inttypes.h:
int32_t val = 42;
printf("val = %" PRId32 "\n", val);     // PRId32 = "d" o "ld" según plataforma

uint64_t big = 12345678901234ULL;
printf("big = %" PRIu64 "\n", big);     // PRIu64 = "lu" o "llu"

// Macros disponibles: PRId8, PRId16, PRId32, PRId64
//                     PRIu8, PRIu16, PRIu32, PRIu64
//                     PRIx32 (hex), PRIX32 (hex mayúscula)

// scanf: SCNd32, SCNu64, etc.
int32_t input;
scanf("%" SCNd32, &input);
```

### Cuándo usar qué tipo

```c
// Para loops y variables locales genéricas:
int i;                    // int está bien para uso general

// Para tamaños e índices de arrays:
size_t len = strlen(s);   // size_t, no int
for (size_t i = 0; i < len; i++) { }

// Para datos de tamaño específico (protocolos, archivos, hardware):
uint16_t port = 8080;
uint32_t ipv4_addr;
int64_t  timestamp_ms;
uint8_t  pixel_value;

// Para almacenar punteros como enteros:
uintptr_t addr = (uintptr_t)ptr;

// Para contadores que pueden ser muy grandes:
uint64_t total_bytes;

// EVITAR:
// long para datos de tamaño fijo (varía entre plataformas)
// int para índices de arrays grandes (puede ser solo 16 bits en teoría)
// unsigned int para tamaños (usar size_t)
```

## Literales enteros

```c
// Decimal:
int x = 42;
int y = -100;

// Octal (prefijo 0):
int oct = 0777;           // 511 en decimal
// CUIDADO: 010 es 8 en decimal, NO 10
int trap = 010;           // = 8, no 10

// Hexadecimal (prefijo 0x o 0X):
int hex = 0xFF;           // 255
int addr = 0xDEADBEEF;

// Binary (C23 o extensión GNU):
int bin = 0b11110000;     // 240

// Sufijos para tipo:
42          // int
42L         // long
42LL        // long long
42U         // unsigned int
42UL        // unsigned long
42ULL       // unsigned long long
```

```c
// Formato de printf para cada tipo:
printf("%d\n", 42);           // int
printf("%u\n", 42U);          // unsigned int
printf("%ld\n", 42L);         // long
printf("%lu\n", 42UL);        // unsigned long
printf("%lld\n", 42LL);       // long long
printf("%llu\n", 42ULL);      // unsigned long long
printf("%zu\n", sizeof(int)); // size_t
printf("%x\n", 255);          // hexadecimal (ff)
printf("%X\n", 255);          // hexadecimal (FF)
printf("%o\n", 255);          // octal (377)
```

## Promoción de enteros

Cuando se usan tipos más pequeños que `int` en expresiones,
C los **promueve** automáticamente a `int`:

```c
// Integer promotion:
char a = 10;
char b = 20;
// a + b → ambos se promueven a int antes de sumar
// El resultado es int, no char

short s1 = 30000;
short s2 = 30000;
// s1 + s2 = 60000 → no cabe en short (max 32767)
// Pero la suma se hace como int → 60000 es un int válido
int result = s1 + s2;    // OK: 60000

short bad = s1 + s2;     // implementation-defined: 60000 no cabe en short
```

```c
// Regla: en cualquier expresión aritmética, los tipos
// menores que int se promueven a int (o unsigned int)
// ANTES de la operación.

// Esto puede sorprender:
unsigned char x = 255;
unsigned char y = 1;
// x + y → ambos promovidos a int → 256 (int)
// NO hace wrapping a 0 como esperarías con unsigned char
if (x + y == 256) {
    // esto es TRUE — la suma se hizo como int
}

unsigned char z = x + y;  // truncamiento: 256 → 0
```

---

## Ejercicios

### Ejercicio 1 — Tamaños en tu plataforma

```c
// Imprimir sizeof de todos los tipos enteros.
// ¿sizeof(long) es 4 u 8?
// ¿char es signed o unsigned?

#include <stdio.h>
#include <limits.h>

int main(void) {
    // Imprimir sizeof de: char, short, int, long, long long
    // Imprimir CHAR_MIN — si es 0, char es unsigned
    return 0;
}
```

### Ejercicio 2 — stdint.h

```c
// Declarar variables para almacenar:
// 1. Un pixel RGB (3 valores de 0-255)
// 2. Un puerto TCP (0-65535)
// 3. Un timestamp Unix en milisegundos
// 4. Un offset de archivo que puede ser muy grande
// Usar los tipos de stdint.h correctos.
```

### Ejercicio 3 — Literales y formatos

```c
// Imprimir el número 255 en decimal, hexadecimal y octal.
// Imprimir INT_MAX, LONG_MAX y LLONG_MAX.
// ¿Qué format specifier necesita cada uno?
```
