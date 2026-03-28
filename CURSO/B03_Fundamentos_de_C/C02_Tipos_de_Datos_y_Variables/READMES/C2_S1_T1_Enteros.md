# T01 — Enteros

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| README.md:85 | `char c = 200;` descrito como "UB técnicamente" cuando `char` es signed | Es **implementation-defined**, no UB. C11 §6.3.1.3p3: cuando se convierte a un tipo signed y el valor no cabe, el resultado es implementation-defined o se genera una señal implementation-defined. No es comportamiento indefinido. |
| README.md:268 | `int addr = 0xDEADBEEF;` presentado sin advertencia | `0xDEADBEEF` = 3,735,928,559 > `INT_MAX` (2,147,483,647). El literal es `unsigned int` (hex literals buscan: int → unsigned int → ...). Asignar a `int` es implementation-defined (C11). Debería usar `unsigned int` o `uint32_t`. |

---

## 1. Los tipos enteros de C

C tiene varios tipos enteros. El estándar **no garantiza tamaños exactos** — solo tamaños mínimos y una jerarquía:

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

### Garantías del estándar

```c
// Jerarquía de tamaños (siempre se cumple):
// sizeof(char) == 1  (por definición)
// sizeof(char) <= sizeof(short) <= sizeof(int) <= sizeof(long) <= sizeof(long long)

// signed y unsigned del mismo tipo tienen el MISMO tamaño.
// Solo cambia la interpretación de los bits.
```

### Tamaños por plataforma (no garantizados)

| Tipo | ILP32 (32-bit) | LP64 (Linux 64-bit) | LLP64 (Windows 64-bit) |
|------|----------------|----------------------|------------------------|
| `char` | 1 | 1 | 1 |
| `short` | 2 | 2 | 2 |
| `int` | 4 | 4 | 4 |
| `long` | 4 | **8** | **4** |
| `long long` | 8 | 8 | 8 |
| puntero | 4 | 8 | 8 |

La diferencia clave: **`long` es 8 bytes en Linux 64-bit pero 4 en Windows 64-bit**. Nunca confiar en `sizeof(long)` para código portable.

---

## 2. `char` — El tipo más pequeño

`sizeof(char)` es **siempre 1** por definición del estándar. Es la unidad mínima addressable.

### Trampa: ¿signed o unsigned?

```c
// Implementation-defined. Depende de la plataforma:
// - x86 (GCC/Clang): char es signed (-128 a 127)
// - ARM (GCC default): char puede ser unsigned (0 a 255)

char c = 200;
// Si char es signed: 200 no cabe → resultado implementation-defined
//   (típicamente -56 en complemento a dos)
// Si char es unsigned: 200 → OK

// Si el comportamiento importa, ser explícito:
signed char   sc = -50;     // siempre signed
unsigned char uc = 200;     // siempre unsigned

// Para datos binarios (buffers, I/O):
unsigned char buffer[1024];  // o uint8_t
```

### `char` como carácter vs como entero

```c
char letter = 'A';           // valor ASCII: 65
char digit  = '7';           // valor ASCII: 55

// Aritmética con char:
char next = 'A' + 1;         // 'B'
int diff = 'Z' - 'A';        // 25

// Verificar si es letra minúscula (ASCII):
if (c >= 'a' && c <= 'z') { /* es minúscula */ }

// Convertir a mayúscula:
char upper = c - 'a' + 'A';   // o usar toupper() de <ctype.h>
```

---

## 3. Rangos de valores

```c
#include <limits.h>

// limits.h define los rangos de cada tipo:
printf("char:      %d a %d\n", CHAR_MIN, CHAR_MAX);
printf("short:     %d a %d\n", SHRT_MIN, SHRT_MAX);
printf("int:       %d a %d\n", INT_MIN, INT_MAX);
printf("long:      %ld a %ld\n", LONG_MIN, LONG_MAX);
printf("long long: %lld a %lld\n", LLONG_MIN, LLONG_MAX);
```

| Tipo | Mínimo | Máximo |
|------|--------|--------|
| `signed char` | -128 | 127 |
| `unsigned char` | 0 | 255 |
| `short` | -32,768 | 32,767 |
| `unsigned short` | 0 | 65,535 |
| `int` (32-bit) | -2,147,483,648 | 2,147,483,647 |
| `unsigned int` | 0 | 4,294,967,295 |
| `long` (LP64) | -9.2×10¹⁸ | 9.2×10¹⁸ |
| `long long` | -9.2×10¹⁸ | 9.2×10¹⁸ |

El rango signed **no es simétrico**: hay un negativo más que positivos (complemento a dos). `UINT_MAX = 2 × INT_MAX + 1`.

### Nombres irregulares de las macros

| Tipo | MIN | MAX | Unsigned MAX |
|------|-----|-----|-------------|
| `char` | `CHAR_MIN` | `CHAR_MAX` | `UCHAR_MAX` |
| `short` | `SHRT_MIN` | `SHRT_MAX` | `USHRT_MAX` |
| `int` | `INT_MIN` | `INT_MAX` | `UINT_MAX` |
| `long` | `LONG_MIN` | `LONG_MAX` | `ULONG_MAX` |
| `long long` | `LLONG_MIN` | `LLONG_MAX` | `ULLONG_MAX` |

No hay macros `_MIN` para unsigned porque siempre es 0.

---

## 4. `stdint.h` — Tipos de tamaño fijo

Cuando el tamaño importa, **no usar** `int` o `long`. Usar `<stdint.h>` (C99+):

### Exact-width (tamaño exacto)

```c
#include <stdint.h>

int8_t    a;    // exactamente 8 bits con signo
int16_t   b;    // exactamente 16 bits
int32_t   c;    // exactamente 32 bits
int64_t   d;    // exactamente 64 bits

uint8_t   e;    // exactamente 8 bits sin signo
uint16_t  f;    // exactamente 16 bits
uint32_t  g;    // exactamente 32 bits
uint64_t  h;    // exactamente 64 bits
```

### Least-width y fast types

```c
// TAMAÑO MÍNIMO (siempre existen, incluso si no hay exact-width):
int_least8_t   a;    // al menos 8 bits
int_least16_t  b;    // al menos 16 bits
// Pueden ser más grandes si la plataforma no tiene el tamaño exacto

// TIPOS RÁPIDOS (el más eficiente con al menos N bits):
int_fast8_t    a;    // el int más rápido con al menos 8 bits
int_fast16_t   b;    // puede ser 8 bytes en x86-64 (64-bit es más rápido que 16-bit)
```

### Tipos especiales

```c
intmax_t  m;    // el tipo entero más grande disponible
uintmax_t n;    // lo mismo, unsigned

intptr_t  p;    // entero del tamaño de un puntero (4 en 32-bit, 8 en 64-bit)
uintptr_t q;    // lo mismo, unsigned

size_t    s;    // unsigned, resultado de sizeof — el tipo correcto para tamaños
ptrdiff_t d;    // signed, resultado de restar punteros
```

### Constantes y macros de `inttypes.h`

```c
#include <stdint.h>
#include <inttypes.h>    // macros de formato para printf/scanf

// Constantes:
int32_t x = INT32_MAX;        // 2147483647
uint64_t y = UINT64_MAX;      // 18446744073709551615

// printf con tipos de stdint.h — usar macros de inttypes.h:
int32_t val = 42;
printf("val = %" PRId32 "\n", val);     // PRId32 = "d" o "ld" según plataforma

uint64_t big = 12345678901234ULL;
printf("big = %" PRIu64 "\n", big);

// Macros: PRId8/16/32/64, PRIu8/16/32/64, PRIx32 (hex), PRIX32 (HEX)
// scanf:  SCNd32, SCNu64, etc.
```

¿Por qué no usar `%d` directamente? Porque `int32_t` podría ser `int` en una plataforma y `long` en otra. `PRId32` se expande al format specifier correcto para esa plataforma.

### Cuándo usar qué tipo

| Situación | Tipo correcto |
|-----------|---------------|
| Loops, variables locales genéricas | `int` |
| Tamaños e índices de arrays | `size_t` |
| Puerto TCP, pixel RGB | `uint16_t`, `uint8_t` |
| Timestamp Unix en ms | `int64_t` |
| Dirección IPv4 | `uint32_t` |
| Almacenar puntero como entero | `uintptr_t` |
| Contador que puede ser muy grande | `uint64_t` |
| Datos de protocolo/archivo/hardware | `intN_t` / `uintN_t` exacto |

---

## 5. Literales enteros

```c
// Decimal:
int x = 42;

// Octal (prefijo 0):
int oct = 0777;           // 511 en decimal
int trap = 010;           // ¡CUIDADO! = 8, NO 10

// Hexadecimal (prefijo 0x):
unsigned int hex = 0xFF;           // 255
uint32_t addr = 0xDEADBEEF;       // usar uint32_t, no int

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

### Format specifiers de `printf`

| Format | Tipo | Ejemplo |
|--------|------|---------|
| `%d` | `int` | `printf("%d", 42)` |
| `%u` | `unsigned int` | `printf("%u", 42U)` |
| `%ld` | `long` | `printf("%ld", 42L)` |
| `%lu` | `unsigned long` | `printf("%lu", 42UL)` |
| `%lld` | `long long` | `printf("%lld", 42LL)` |
| `%llu` | `unsigned long long` | `printf("%llu", 42ULL)` |
| `%zu` | `size_t` | `printf("%zu", sizeof(int))` |
| `%x` / `%X` | hex (min/MAY) | `printf("%x", 255)` → `ff` |
| `%o` | octal | `printf("%o", 255)` → `377` |

---

## 6. Promoción de enteros

Cuando tipos menores que `int` se usan en expresiones, C los **promueve** automáticamente a `int`:

```c
char a = 10;
char b = 20;
// a + b → ambos se promueven a int antes de sumar
// El resultado es int, no char

short s1 = 30000;
short s2 = 30000;
// s1 + s2 se hace como int → 60000 es un int válido
int result = s1 + s2;    // OK: 60000
short bad = s1 + s2;     // implementation-defined: 60000 no cabe en short
```

### Sorpresa con `unsigned char`

```c
unsigned char x = 255;
unsigned char y = 1;
// x + y → ambos promovidos a int → 256 (int)
// NO hace wrapping a 0 como esperarías con unsigned char
if (x + y == 256) {
    // TRUE — la suma se hizo como int
}

unsigned char z = x + y;  // truncamiento: 256 → 0 (al asignar a unsigned char)
```

**Regla**: en cualquier expresión aritmética, tipos menores que `int` se promueven a `int` (o `unsigned int`) **antes** de la operación.

---

## Ejercicios

### Ejercicio 1 — `sizeof` de todos los tipos

Compila y ejecuta `labs/sizes.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic sizes.c -o sizes && ./sizes
rm -f sizes
```

**Predicción**: ¿`sizeof(long)` es 4 u 8 en tu sistema? ¿`char` es signed o unsigned?

<details><summary>Respuesta</summary>

```
=== sizeof of integer types ===

char:      1 byte(s)
short:     2 bytes
int:       4 bytes
long:      8 bytes
long long: 8 bytes

=== signed vs unsigned (same sizes) ===

unsigned char:      1 byte(s)
unsigned short:     2 bytes
unsigned int:       4 bytes
unsigned long:      8 bytes
unsigned long long: 8 bytes

=== is plain char signed or unsigned? ===

char is signed (CHAR_MIN = -128)
```

- `long` = 8 bytes (Linux 64-bit, modelo LP64). Sería 4 en Windows 64-bit (LLP64).
- `char` es signed en x86 con GCC. `CHAR_MIN = -128` lo confirma (si fuera unsigned, sería 0).
- signed y unsigned tienen el **mismo tamaño** — solo cambia la interpretación del bit más significativo.

</details>

### Ejercicio 2 — Rangos con `limits.h`

Compila y ejecuta `labs/limits.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic limits.c -o limits_prog && ./limits_prog
rm -f limits_prog
```

**Predicción**: ¿`long` y `long long` tienen los mismos rangos en Linux 64-bit? ¿Por qué el rango signed no es simétrico?

<details><summary>Respuesta</summary>

```
=== signed type ranges ===

char:      -128 to 127
short:     -32768 to 32767
int:       -2147483648 to 2147483647
long:      -9223372036854775808 to 9223372036854775807
long long: -9223372036854775808 to 9223372036854775807

=== unsigned type ranges ===

unsigned char:      0 to 255
unsigned short:     0 to 65535
unsigned int:       0 to 4294967295
unsigned long:      0 to 18446744073709551615
unsigned long long: 0 to 18446744073709551615
```

- `long` y `long long` tienen los **mismos rangos** en LP64 porque ambos son de 8 bytes. En otras plataformas pueden diferir.
- El rango signed no es simétrico por el **complemento a dos**: con N bits hay 2^N combinaciones, la mitad para negativos (-2^(N-1) a -1) y la otra mitad para no-negativos (0 a 2^(N-1)-1). El cero "consume" un slot del lado positivo.

</details>

### Ejercicio 3 — Tipos de `stdint.h`

Compila y ejecuta `labs/fixed_width.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic fixed_width.c -o fixed_width && ./fixed_width
rm -f fixed_width
```

**Predicción**: ¿`int_fast16_t` tiene 2 bytes u 8 bytes? ¿Por qué?

<details><summary>Respuesta</summary>

```
=== exact-width types (stdint.h) ===

int8_t:   1 byte(s)
int16_t:  2 bytes
int32_t:  4 bytes
int64_t:  8 bytes
...

=== fast types ===

int_fast8_t:  1 byte(s)
int_fast16_t: 8 bytes
int_fast32_t: 8 bytes
int_fast64_t: 8 bytes
```

`int_fast16_t` es **8 bytes**, no 2. La implementación decide que operar con 64 bits es más eficiente en x86-64 que con 16 bits (las instrucciones de 64 bits son nativas, mientras que 16 bits puede requerir operandos de tamaño especial). "Fast" optimiza por velocidad, no por memoria.

</details>

### Ejercicio 4 — Format specifiers portables

Compila y ejecuta `labs/format_specifiers.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic format_specifiers.c -o format_specifiers && ./format_specifiers
rm -f format_specifiers
```

**Predicción**: ¿`0xDEADBEEF` se imprime igual con `PRIx32` (minúsculas) y `PRIX32` (mayúsculas)?

<details><summary>Respuesta</summary>

```
uint32_t hex (lowercase): deadbeef
uint32_t hex (uppercase): DEADBEEF
uint64_t hex (lowercase): cafebabedeadc0de
uint64_t hex (uppercase): CAFEBABEDEADC0DE
```

`PRIx32` produce minúsculas (`deadbeef`), `PRIX32` produce mayúsculas (`DEADBEEF`). El valor numérico es idéntico — solo cambia la representación textual. Este patrón se repite para todos los anchos: `PRIx8`, `PRIx16`, `PRIx64`.

</details>

### Ejercicio 5 — La trampa del octal

```c
// octal_trap.c
#include <stdio.h>

int main(void) {
    int a = 10;
    int b = 010;
    int c = 0x10;

    printf("decimal  10 = %d\n", a);
    printf("octal   010 = %d\n", b);
    printf("hex    0x10 = %d\n", c);

    // ¿Son iguales?
    printf("\n10 == 010? %s\n", a == b ? "yes" : "no");
    printf("10 == 0x10? %s\n", a == c ? "yes" : "no");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic octal_trap.c -o octal_trap && ./octal_trap
rm -f octal_trap
```

**Predicción**: ¿`010` es 10 o algo diferente? ¿`0x10`?

<details><summary>Respuesta</summary>

```
decimal  10 = 10
octal   010 = 8
hex    0x10 = 16

10 == 010? no
10 == 0x10? no
```

`010` es **8**, no 10. El prefijo `0` indica literal octal: 0×8¹ + 1×8⁰ = 8. `0x10` es **16** (hexadecimal: 1×16¹ + 0×16⁰). Ninguno de los tres es igual. Esta trampa aparece en código real cuando alguien alinea números con ceros a la izquierda (`int arr[] = {010, 020, 030}` → `{8, 16, 24}`).

</details>

### Ejercicio 6 — Elegir el tipo correcto

```c
// choose_type.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void) {
    // 1. Pixel RGB (0-255 por canal):
    uint8_t r = 255, g = 128, b = 0;

    // 2. Puerto TCP (0-65535):
    uint16_t port = 8080;

    // 3. Timestamp Unix en milisegundos:
    int64_t timestamp_ms = 1711468800000LL;  // 2024-03-26 aprox

    // 4. Offset de archivo grande:
    int64_t file_offset = 5368709120LL;  // 5 GB

    // 5. Dirección IPv4:
    uint32_t ipv4 = (192u << 24) | (168u << 16) | (1u << 8) | 100u;

    printf("RGB: (%"PRIu8", %"PRIu8", %"PRIu8")\n", r, g, b);
    printf("Port: %"PRIu16"\n", port);
    printf("Timestamp: %"PRId64" ms\n", timestamp_ms);
    printf("File offset: %"PRId64" bytes (%.1f GB)\n",
           file_offset, (double)file_offset / (1024.0*1024*1024));
    printf("IPv4: %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"\n",
           (uint8_t)(ipv4 >> 24), (uint8_t)(ipv4 >> 16),
           (uint8_t)(ipv4 >> 8), (uint8_t)ipv4);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic choose_type.c -o choose_type && ./choose_type
rm -f choose_type
```

**Predicción**: ¿por qué `timestamp_ms` necesita `int64_t` y no `int32_t`? ¿Cuánto es `INT32_MAX` en milisegundos?

<details><summary>Respuesta</summary>

```
RGB: (255, 128, 0)
Port: 8080
Timestamp: 1711468800000 ms
File offset: 5368709120 bytes (5.0 GB)
IPv4: 192.168.1.100
```

`INT32_MAX = 2,147,483,647`. En milisegundos, eso son ~24.8 días. Un timestamp Unix en milisegundos fácilmente excede ese rango (1.7 billones para 2024). Se necesita `int64_t` que soporta hasta ~292 millones de años en ms.

El offset de archivo de 5 GB (5,368,709,120) también excede `INT32_MAX`, por eso necesita `int64_t`.

</details>

### Ejercicio 7 — Promoción de enteros en acción

```c
// promotion.c
#include <stdio.h>

int main(void) {
    unsigned char a = 200;
    unsigned char b = 100;

    // ¿Qué tipo tiene a + b?
    printf("a + b = %d\n", a + b);    // ¿300 o algo raro?
    printf("sizeof(a + b) = %zu\n", sizeof(a + b));

    // ¿Y si guardamos en unsigned char?
    unsigned char c = a + b;
    printf("unsigned char c = a + b → %u\n", c);

    // short overflow test:
    short s1 = 30000;
    short s2 = 30000;
    printf("\nshort 30000 + 30000:\n");
    printf("  como int:   %d\n", s1 + s2);
    printf("  como short: %hd\n", (short)(s1 + s2));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic promotion.c -o promotion && ./promotion
rm -f promotion
```

**Predicción**: ¿`a + b` da 300 o se trunca? ¿`sizeof(a + b)` es 1 o 4?

<details><summary>Respuesta</summary>

```
a + b = 300
sizeof(a + b) = 4

unsigned char c = a + b → 44

short 30000 + 30000:
  como int:   60000
  como short: -5536
```

- `a + b` = **300** (no trunca). Ambos `unsigned char` se promueven a `int` antes de sumar.
- `sizeof(a + b)` = **4** (tamaño de `int`, no de `unsigned char`).
- `c = a + b` trunca a 8 bits: 300 mod 256 = **44**.
- `30000 + 30000` como `int` = 60000. Casteado a `short`: 60000 no cabe en 16 bits signed → implementation-defined, típicamente **-5536** (complemento a dos wrapping).

</details>

### Ejercicio 8 — `size_t` vs `int` para índices

```c
// size_t_index.c
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *text = "Hello, world!";
    size_t len = strlen(text);

    printf("Length: %zu\n", len);

    // Iterar con size_t (correcto):
    for (size_t i = 0; i < len; i++) {
        // ...
    }

    // Iterar con int — ¿warning?
    for (int i = 0; i < (int)len; i++) {
        // funciona, pero el cast es feo
    }

    // ¿Qué pasa si len es mayor que INT_MAX?
    printf("INT_MAX = %d\n", __INT_MAX__);
    printf("SIZE_MAX = %zu\n", (size_t)-1);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic size_t_index.c -o size_t_index && ./size_t_index
rm -f size_t_index
```

**Predicción**: ¿GCC produce warning si comparas `int i < len` (int vs size_t) sin cast?

<details><summary>Respuesta</summary>

Sí, comparar `int < size_t` produce un warning con `-Wextra`:

```
warning: comparison of integer expressions of different signedness: 'int' and 'size_t'
```

`size_t` es unsigned y `int` es signed — compararlos directamente puede dar resultados incorrectos si `i` es negativo. Soluciones:
1. Usar `size_t i` en el loop (lo correcto)
2. Cast explícito `(int)len` (funciona si estás seguro de que len cabe en int)

`SIZE_MAX` en 64-bit = 18,446,744,073,709,551,615 (2⁶⁴-1), muchísimo mayor que `INT_MAX`.

</details>

### Ejercicio 9 — Sufijos de literales

```c
// literal_types.c
#include <stdio.h>

int main(void) {
    // ¿Qué tipo tiene cada literal?
    printf("sizeof(42)    = %zu\n", sizeof(42));
    printf("sizeof(42L)   = %zu\n", sizeof(42L));
    printf("sizeof(42LL)  = %zu\n", sizeof(42LL));
    printf("sizeof(42U)   = %zu\n", sizeof(42U));
    printf("sizeof(42ULL) = %zu\n", sizeof(42ULL));

    // ¿Y los hex?
    printf("\nsizeof(0xFF)         = %zu\n", sizeof(0xFF));
    printf("sizeof(0xFFFFFFFF)   = %zu\n", sizeof(0xFFFFFFFF));
    printf("sizeof(0xFFFFFFFFFF) = %zu\n", sizeof(0xFFFFFFFFFF));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic literal_types.c -o literal_types && ./literal_types
rm -f literal_types
```

**Predicción**: ¿`sizeof(0xFFFFFFFF)` es 4 u 8? ¿Por qué?

<details><summary>Respuesta</summary>

```
sizeof(42)    = 4    (int)
sizeof(42L)   = 8    (long, LP64)
sizeof(42LL)  = 8    (long long)
sizeof(42U)   = 4    (unsigned int)
sizeof(42ULL) = 8    (unsigned long long)

sizeof(0xFF)         = 4    (int — cabe en int)
sizeof(0xFFFFFFFF)   = 4    (unsigned int)
sizeof(0xFFFFFFFFFF) = 8    (unsigned long o long)
```

`0xFFFFFFFF` = 4,294,967,295. No cabe en `int` (max 2,147,483,647) pero sí en `unsigned int`. Para literales hex, C busca en orden: int → unsigned int → long → unsigned long → long long → unsigned long long. Se queda en `unsigned int` (4 bytes).

`0xFFFFFFFFFF` no cabe en unsigned int (necesita >32 bits), así que escala a `long` (8 bytes en LP64).

</details>

### Ejercicio 10 — Complemento a dos: por qué `-INT_MIN` es UB

```c
// two_complement.c
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

int main(void) {
    printf("INT_MIN = %d\n", INT_MIN);
    printf("INT_MAX = %d\n", INT_MAX);

    // Negar INT_MAX es seguro:
    int neg_max = -INT_MAX;
    printf("-INT_MAX = %d\n", neg_max);  // -2147483647

    // Pero negar INT_MIN es UB:
    // int neg_min = -INT_MIN;  // UB: -(-2147483648) = 2147483648 > INT_MAX

    // Forma segura de obtener el valor absoluto de INT_MIN:
    // Usar unsigned:
    unsigned int abs_min = (unsigned int)INT_MAX + 1U;
    printf("|INT_MIN| via unsigned = %u\n", abs_min);

    // Verificar la asimetría:
    printf("\n|INT_MIN| - |INT_MAX| = %u\n",
           (unsigned int)(-(INT_MIN + 1)) + 1U - (unsigned int)INT_MAX);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic two_complement.c -o two_comp && ./two_comp
rm -f two_comp
```

**Predicción**: ¿por qué `-INT_MIN` es comportamiento indefinido? ¿Cuánto vale `|INT_MIN|`?

<details><summary>Respuesta</summary>

`INT_MIN = -2,147,483,648`. Negarlo daría 2,147,483,648, que excede `INT_MAX` (2,147,483,647). Esto es **signed integer overflow → UB**.

```
INT_MIN = -2147483648
INT_MAX = 2147483647
-INT_MAX = -2147483647
|INT_MIN| via unsigned = 2147483648
```

`|INT_MIN|` = 2,147,483,648 = `INT_MAX + 1`. Solo se puede representar como `unsigned int`. Esta asimetría del complemento a dos es la razón por la que `abs(INT_MIN)` es UB — muchas implementaciones de `abs()` simplemente hacen `-x`, que desborda.

</details>
