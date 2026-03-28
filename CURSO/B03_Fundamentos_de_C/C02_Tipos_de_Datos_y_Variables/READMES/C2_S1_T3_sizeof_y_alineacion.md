# T03 — sizeof y alineación

> Sin erratas detectadas en el material original.

---

## 1. `sizeof` — Tamaño en bytes

`sizeof` es un **operador** (no una función) que retorna el tamaño en bytes. El resultado es de tipo `size_t`:

```c
// Con un tipo — requiere paréntesis:
sizeof(int)         // 4
sizeof(double)      // 8

// Con una expresión — paréntesis opcionales:
int x = 42;
sizeof x            // 4 (sin paréntesis)
sizeof(x)           // 4 (más común)

// Con arrays — retorna tamaño TOTAL:
int arr[10];
sizeof(arr)         // 40 (10 × 4), NO 10
sizeof(arr[0])      // 4

// Número de elementos:
size_t count = sizeof(arr) / sizeof(arr[0]);   // 10
```

### `sizeof` se evalúa en compilación

```c
int x = 5;
size_t s = sizeof(x++);    // x sigue siendo 5 — la expresión NO se ejecuta
int arr[sizeof(int)];       // OK — sizeof(int) es constante de compilación

// Excepción: VLAs — sizeof se evalúa en runtime:
int n = 10;
int vla[n];
sizeof(vla)                 // evaluado en runtime: n * sizeof(int)
```

### Trampas con `sizeof`

```c
// TRAMPA 1: array vs puntero
int arr[10];
int *ptr = arr;
sizeof(arr)     // 40 (tamaño del array)
sizeof(ptr)     // 8  (tamaño del PUNTERO)

// En parámetros de función, el array decae a puntero:
void foo(int arr[10]) {
    sizeof(arr)  // 8 (es un puntero, el [10] se ignora)
}

// TRAMPA 2: string literal incluye '\0'
sizeof("hello")     // 6 (5 chars + '\0')
strlen("hello")     // 5 (sin '\0')

// TRAMPA 3: sizeof de struct ≠ suma de campos (por padding)
```

### Macro `ARRAY_SIZE`

```c
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int numbers[] = {10, 20, 30, 40, 50};
for (size_t i = 0; i < ARRAY_SIZE(numbers); i++) { ... }

// CUIDADO: no funciona con punteros:
int *ptr = numbers;
ARRAY_SIZE(ptr)    // sizeof(int*)/sizeof(int) = 2 → INCORRECTO
```

---

## 2. Alineación

La CPU accede a la memoria más eficientemente cuando los datos están en direcciones que son múltiplo de su tamaño:

```
Dirección:  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
            ├──┤                                              char   — cualquier dirección
            ├─────┤                                           short  — múltiplo de 2
            ├───────────┤                                     int    — múltiplo de 4
            ├───────────────────────┤                         double — múltiplo de 8
```

| Tipo | `sizeof` | `alignof` | Acceso desalineado |
|------|---------|----------|-------------------|
| `char` | 1 | 1 | N/A (siempre alineado) |
| `short` | 2 | 2 | x86: lento. ARM: SIGBUS |
| `int` | 4 | 4 | x86: lento. ARM: SIGBUS |
| `double` | 8 | 8 | x86: lento. ARM: SIGBUS |
| `long double` | 16 | 16 | x86: lento. ARM: SIGBUS |

```c
#include <stdalign.h>    // macro alignof (C11)

printf("alignof(int):    %zu\n", alignof(int));      // 4
printf("alignof(double): %zu\n", alignof(double));    // 8
```

---

## 3. Padding en structs

El compilador inserta **bytes de padding** entre campos para cumplir alineación:

```c
struct Bad {
    char  a;     // 1 byte
    int   b;     // 4 bytes — necesita múltiplo de 4
    char  c;     // 1 byte
};

// Layout en memoria:
// Offset:  0     1  2  3    4  5  6  7    8     9  10 11
//         [a]  [pad pad pad][b  b  b  b]  [c]  [pad pad pad]
//
// sizeof(struct Bad) = 12 (no 6)
// Padding: 6 bytes desperdiciados
```

¿Por qué padding al final? Para que arrays de structs mantengan la alineación. `sizeof` debe ser múltiplo de la alineación máxima del struct.

### Reordenar campos: la solución

```c
struct Good {
    int   b;     // 4 bytes
    char  a;     // 1 byte
    char  c;     // 1 byte
};

// Layout:
// Offset:  0  1  2  3    4     5     6  7
//         [b  b  b  b]  [a]   [c]  [pad pad]
//
// sizeof(struct Good) = 8 (no 12) — mismos datos, 4 bytes menos
```

**Regla general**: ordenar campos de **mayor a menor** tamaño:

```c
struct Optimal {
    double d;    // 8 (alineación 8)
    int    i;    // 4 (alineación 4)
    short  s;    // 2 (alineación 2)
    char   c;    // 1 (alineación 1)
};
// sizeof = 16 (1 byte de padding implícito al final → múltiplo de 8)

struct Terrible {
    char   c;    // 1 + 7 padding
    double d;    // 8
    char   c2;   // 1 + 3 padding
    int    i;    // 4
    char   c3;   // 1 + 1 padding
    short  s;    // 2 + 4 padding (tail)
};
// sizeof = 32 — el doble, mismos datos
```

---

## 4. `offsetof` — Visualizar el layout

```c
#include <stddef.h>    // offsetof

struct Demo {
    char   a;
    int    b;
    char   c;
    double d;
    short  e;
};

printf("a: offset=%2zu  size=%zu\n", offsetof(struct Demo, a), sizeof(char));    // 0
printf("b: offset=%2zu  size=%zu\n", offsetof(struct Demo, b), sizeof(int));     // 4
printf("c: offset=%2zu  size=%zu\n", offsetof(struct Demo, c), sizeof(char));    // 8
printf("d: offset=%2zu  size=%zu\n", offsetof(struct Demo, d), sizeof(double));  // 16
printf("e: offset=%2zu  size=%zu\n", offsetof(struct Demo, e), sizeof(short));   // 24
printf("total: %zu\n", sizeof(struct Demo));                                      // 32
```

Mapa de memoria:

```
Offset: 0   1-3    4-7    8   9-15   16-23  24-25  26-31
       [a] [pad3] [bbbb] [c] [pad7] [dddddddd] [ee] [pad6]
```

---

## 5. `#pragma pack` — Eliminar padding

```c
#pragma pack(push, 1)    // guardar alineación actual, forzar a 1 byte
struct Packed {
    char  a;     // 1 byte
    int   b;     // 4 bytes (sin padding antes)
    char  c;     // 1 byte
};
#pragma pack(pop)        // restaurar alineación original

sizeof(struct Packed)    // 6 (sin padding)

// Alternativa GCC:
struct Packed2 {
    char a; int b; char c;
} __attribute__((packed));
```

### Cuándo usar `pack`

- **Protocolos de red**: los bytes deben estar en posiciones exactas
- **Formatos de archivo binario**: header BMP, ELF, etc.
- **Registros de hardware**: memoria mapeada con layout fijo

### Peligros

```c
struct Packed p;
int *ptr = &p.b;     // ptr apunta a dirección NO alineada
*ptr = 42;           // x86: funciona (lento). ARM: SIGBUS

// NUNCA tomar la dirección de un campo packed como puntero de su tipo.
// GCC advierte con -Waddress-of-packed-member
```

**Preferir reordenar campos** sobre `pack` para structs de uso general.

---

## 6. `alignas` (C11) — Forzar alineación mayor

```c
#include <stdalign.h>

alignas(16) int data[4];          // alineado a 16 bytes (SIMD)
alignas(64) char cache_line[64];  // alineado a línea de caché

// En structs:
struct alignas(64) CacheLine {
    int data[16];     // toda la struct alineada a 64 bytes
};

// Por campo:
struct Mixed {
    alignas(16) float vector[4];   // campo alineado a 16 (SSE)
    int flags;
};
```

Casos de uso: SIMD (SSE/AVX requieren 16/32 bytes), evitar false sharing entre threads (alinear a 64 bytes).

---

## Ejercicios

### Ejercicio 1 — `sizeof` y `alignof` de tipos primitivos

Compila y ejecuta `labs/sizeof_basics.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic sizeof_basics.c -o sizeof_basics && ./sizeof_basics
rm -f sizeof_basics
```

**Predicción**: ¿`sizeof(char*)` y `sizeof(int*)` son diferentes o iguales? ¿`sizeof("hello")` es 5 o 6?

<details><summary>Respuesta</summary>

```
char*:       8 bytes
int*:        8 bytes
void*:       8 bytes
char str[]:  6 bytes (incluye '\0')

alignof(char):        1
alignof(int):         4
alignof(double):      8
alignof(long double): 16
```

- Todos los punteros tienen el **mismo tamaño** (8 bytes en 64-bit), independientemente del tipo al que apuntan. El tamaño del puntero depende de la arquitectura, no del tipo de dato.
- `sizeof("hello")` = **6**: 5 caracteres + el `'\0'` terminador. `strlen("hello")` devolvería 5.
- `alignof` coincide con `sizeof` para todos los tipos en x86-64 Linux.

</details>

### Ejercicio 2 — Padding en structs

Compila y ejecuta `labs/padding_inspect.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic padding_inspect.c -o padding_inspect && ./padding_inspect
rm -f padding_inspect
```

**Predicción**: `struct Bad` tiene campos que suman 6 bytes. ¿Cuánto es `sizeof`? ¿Cuántos bytes son padding?

<details><summary>Respuesta</summary>

```
=== struct Bad (char, int, char) ===
sizeof:  12 bytes
alignof: 4
  a: offset=0  size=1
  b: offset=4  size=4
  c: offset=8  size=1

=== struct Good (int, char, char) ===
sizeof:  8 bytes
alignof: 4
  b: offset=0  size=4
  a: offset=4  size=1
  c: offset=5  size=1

Padding desperdiciado:
Bad:   sizeof=12, suma de campos=6, padding=6 bytes
Mixed: sizeof=32, suma de campos=16, padding=16 bytes
```

- `struct Bad`: **12 bytes** (6 de datos + 6 de padding = 100% overhead).
  - 3 bytes entre `a` y `b` (alinear `int` a múltiplo de 4)
  - 3 bytes después de `c` (sizeof debe ser múltiplo de 4)
- `struct Good`: **8 bytes** (6 de datos + 2 de padding). Mismos campos, diferente orden.
- `struct Mixed`: **50%** es padding. Un `double` después de un `char` genera 7 bytes de padding.

</details>

### Ejercicio 3 — Reordenar para minimizar padding

Compila y ejecuta `labs/reorder_fields.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic reorder_fields.c -o reorder_fields && ./reorder_fields
rm -f reorder_fields
```

**Predicción**: `Wasteful` vs `Optimized` — mismos 6 campos (17 bytes de datos). ¿Cuánto ahorra reordenar?

<details><summary>Respuesta</summary>

```
Wasteful:  32 bytes (padding: 15)
Optimized: 24 bytes (padding: 7)
Ahorro: 8 bytes por struct

En un array de 1000 elementos:
Wasteful[1000]:  32000 bytes
Optimized[1000]: 24000 bytes
Ahorro total:    8000 bytes
```

Reordenar de mayor a menor (double → int → short → chars) redujo de 32 a **24 bytes** (25% menos). En 1000 elementos: **8 KB** de ahorro. Los campos encajan naturalmente: `double` (8) + `int` (4) + `short` (2) + 3 `char` (3) = 17, redondeado a 24 (múltiplo de 8).

</details>

### Ejercicio 4 — `__attribute__((packed))`

Compila y ejecuta `labs/packed_struct.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic packed_struct.c -o packed_struct && ./packed_struct
rm -f packed_struct
```

**Predicción**: `struct Packed` (con `__attribute__((packed))`) — ¿cuánto? ¿`b` está en dirección múltiplo de 4?

<details><summary>Respuesta</summary>

```
struct Normal: 12 bytes
struct Packed: 6 bytes
  b: offset=1

PackedHeader: 17 bytes (padding: 0)
Header normal: 32 bytes (padding: 15)

p.b está en dirección múltiplo de 4? No
```

- `struct Packed` = **6 bytes** (sin ningún padding). `b` queda en offset 1 (no múltiplo de 4).
- `PackedHeader` = **17 bytes** exactos (la suma de los campos). Necesario para que coincida con un protocolo de red.
- El campo `b` desalineado funciona en x86 pero puede causar SIGBUS en ARM. Nunca tomar `&p.b` como puntero `int *`.

</details>

### Ejercicio 5 — Predecir sizeof a mano

Calcula a mano el `sizeof` de cada struct, luego verifica:

```c
// predict_sizeof.c
#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdint.h>

struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; double b; char c; };
struct D { double b; char a; char c; };
struct E { char a; short b; char c; int d; };
struct F { int d; short b; char a; char c; };

int main(void) {
    printf("A (char,int,char):           %zu\n", sizeof(struct A));
    printf("B (int,char,char):           %zu\n", sizeof(struct B));
    printf("C (char,double,char):        %zu\n", sizeof(struct C));
    printf("D (double,char,char):        %zu\n", sizeof(struct D));
    printf("E (char,short,char,int):     %zu\n", sizeof(struct E));
    printf("F (int,short,char,char):     %zu\n", sizeof(struct F));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic predict_sizeof.c -o predict_sizeof && ./predict_sizeof
rm -f predict_sizeof
```

**Predicción**: calcula cada sizeof antes de compilar.

<details><summary>Respuesta</summary>

```
A (char,int,char):           12
B (int,char,char):           8
C (char,double,char):        24
D (double,char,char):        16
E (char,short,char,int):     12
F (int,short,char,char):     8
```

Proceso para `struct E` (char, short, char, int):
- `a` (char): offset 0, size 1
- `b` (short): necesita align 2 → offset 2, size 2 (1 byte padding)
- `c` (char): offset 4, size 1
- `d` (int): necesita align 4 → offset 8, size 4 (3 bytes padding)
- Total: 12 (ya múltiplo de 4)

`struct F` reordena: int(4) + short(2) + char(1) + char(1) = 8 (sin padding).

</details>

### Ejercicio 6 — `offsetof` manual

```c
// offsetof_manual.c
#include <stdio.h>
#include <stddef.h>

struct Record {
    char    type;       // 1
    double  value;      // 8
    char    flags;      // 1
    int     count;      // 4
    short   id;         // 2
};

int main(void) {
    size_t fields_sum = sizeof(char) + sizeof(double) + sizeof(char)
                      + sizeof(int) + sizeof(short);

    printf("sizeof(struct Record) = %zu\n", sizeof(struct Record));
    printf("Suma de campos:         %zu\n", fields_sum);
    printf("Padding total:          %zu bytes (%.0f%%)\n",
           sizeof(struct Record) - fields_sum,
           100.0 * (sizeof(struct Record) - fields_sum) / sizeof(struct Record));

    printf("\nCampo     Offset  Size  Padding antes\n");
    printf("-----     ------  ----  -------------\n");
    printf("type      %2zu      %zu     -\n", offsetof(struct Record, type), sizeof(char));
    printf("value     %2zu      %zu     %zu\n", offsetof(struct Record, value), sizeof(double),
           offsetof(struct Record, value) - (offsetof(struct Record, type) + sizeof(char)));
    printf("flags     %2zu      %zu     %zu\n", offsetof(struct Record, flags), sizeof(char),
           offsetof(struct Record, flags) - (offsetof(struct Record, value) + sizeof(double)));
    printf("count     %2zu      %zu     %zu\n", offsetof(struct Record, count), sizeof(int),
           offsetof(struct Record, count) - (offsetof(struct Record, flags) + sizeof(char)));
    printf("id        %2zu      %zu     %zu\n", offsetof(struct Record, id), sizeof(short),
           offsetof(struct Record, id) - (offsetof(struct Record, count) + sizeof(int)));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic offsetof_manual.c -o offsetof_manual && ./offsetof_manual
rm -f offsetof_manual
```

**Predicción**: ¿cuántos bytes de padding hay entre `type` (char) y `value` (double)?

<details><summary>Respuesta</summary>

```
sizeof(struct Record) = 32
Suma de campos:         16
Padding total:          16 bytes (50%)

Campo     Offset  Size  Padding antes
-----     ------  ----  -------------
type       0      1     -
value      8      8     7
flags     16      1     0
count     20      4     3
id        24      2     0
```

**7 bytes** de padding entre `type` (char, offset 0) y `value` (double, offset 8). El `double` necesita alineación a 8. El 50% del struct es padding. Reordenando (double, int, short, char, char): `sizeof` bajaría a 24 (padding 8 bytes → 33%).

</details>

### Ejercicio 7 — Array decay en funciones

```c
// array_decay.c
#include <stdio.h>

void show_sizeof(int arr[10]) {
    printf("Dentro de función: sizeof(arr) = %zu\n", sizeof(arr));
}

int main(void) {
    int arr[10];
    printf("En main:           sizeof(arr) = %zu\n", sizeof(arr));
    show_sizeof(arr);

    // ¿Y con char[]?
    char str[] = "hello world";
    printf("\nchar str[]:        sizeof(str) = %zu\n", sizeof(str));
    printf("                   strlen(str) = %zu\n", __builtin_strlen(str));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic array_decay.c -o array_decay && ./array_decay
rm -f array_decay
```

**Predicción**: ¿`sizeof(arr)` dentro de la función es 40 o 8?

<details><summary>Respuesta</summary>

```
En main:           sizeof(arr) = 40
Dentro de función: sizeof(arr) = 8

char str[]:        sizeof(str) = 12
                   strlen(str) = 11
```

Dentro de la función, `sizeof(arr)` = **8** (tamaño de un puntero). `int arr[10]` en un parámetro de función es idéntico a `int *arr` — el `[10]` se ignora por completo. Esto se llama "array decay": al pasar un array a una función, decae a un puntero al primer elemento.

`sizeof(str)` = 12 (11 chars + `'\0'`). `strlen(str)` = 11 (sin `'\0'`).

</details>

### Ejercicio 8 — `sizeof` no evalúa la expresión

```c
// sizeof_no_eval.c
#include <stdio.h>

int main(void) {
    int x = 5;
    int y = 10;

    printf("Antes: x=%d, y=%d\n", x, y);

    size_t s1 = sizeof(x++);
    size_t s2 = sizeof(y = 100);
    size_t s3 = sizeof(x + y);

    printf("Después: x=%d, y=%d\n", x, y);
    printf("s1=%zu, s2=%zu, s3=%zu\n", s1, s2, s3);

    // ¿Se ejecutó x++ o y = 100?

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizeof_no_eval.c -o sizeof_no_eval && ./sizeof_no_eval
rm -f sizeof_no_eval
```

**Predicción**: ¿`x` vale 6 después de `sizeof(x++)`? ¿`y` vale 100 después de `sizeof(y = 100)`?

<details><summary>Respuesta</summary>

```
Antes: x=5, y=10
Después: x=5, y=10
s1=4, s2=4, s3=4
```

`x` sigue siendo **5** y `y` sigue siendo **10**. `sizeof` solo determina el **tipo** de la expresión — nunca la ejecuta. `sizeof(x++)` determina que `x++` tiene tipo `int` (4 bytes) sin incrementar `x`. `sizeof(y = 100)` determina que `y = 100` tiene tipo `int` sin asignar. Esto es por diseño: `sizeof` se evalúa en compilación.

</details>

### Ejercicio 9 — `alignas` en acción

```c
// alignas_demo.c
#include <stdio.h>
#include <stdalign.h>
#include <stdint.h>

int main(void) {
    int normal;
    alignas(16) int aligned16;
    alignas(32) int aligned32;
    alignas(64) int cache_aligned;

    printf("Normal:    %p → mod 4  = %zu\n",
           (void *)&normal, (size_t)&normal % 4);
    printf("alignas16: %p → mod 16 = %zu\n",
           (void *)&aligned16, (size_t)&aligned16 % 16);
    printf("alignas32: %p → mod 32 = %zu\n",
           (void *)&aligned32, (size_t)&aligned32 % 32);
    printf("alignas64: %p → mod 64 = %zu\n",
           (void *)&cache_aligned, (size_t)&cache_aligned % 64);

    // alignas en struct:
    struct alignas(64) CachePadded {
        int value;
        char padding[60];  // llenar hasta 64 bytes
    };

    printf("\nsizeof(CachePadded) = %zu\n", sizeof(struct CachePadded));
    printf("alignof(CachePadded) = %zu\n", alignof(struct CachePadded));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic alignas_demo.c -o alignas_demo && ./alignas_demo
rm -f alignas_demo
```

**Predicción**: ¿`mod 16`, `mod 32`, `mod 64` son todos 0?

<details><summary>Respuesta</summary>

Sí, todos los residuos son **0**:

```
Normal:    0x7ffd...c → mod 4  = 0
alignas16: 0x7ffd...0 → mod 16 = 0
alignas32: 0x7ffd...0 → mod 32 = 0
alignas64: 0x7ffd...0 → mod 64 = 0

sizeof(CachePadded) = 64
alignof(CachePadded) = 64
```

`alignas(N)` garantiza que la dirección es múltiplo de N. Las direcciones exactas varían entre ejecuciones, pero los residuos siempre son 0. `alignas(64)` es útil para evitar false sharing: cada thread opera en su propia línea de caché (64 bytes en x86-64).

</details>

### Ejercicio 10 — Optimizar un struct real

```c
// optimize_struct.c
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

// Simula un registro de log — orden arbitrario:
struct LogEntry_bad {
    char      level;       // 1: 'I', 'W', 'E'
    uint64_t  timestamp;   // 8: Unix ms
    char      source[4];   // 4: "APP\0", "NET\0"
    uint32_t  thread_id;   // 4: ID del thread
    char      message;     // 1: código de mensaje
    uint16_t  line;        // 2: número de línea
};

// Tu versión optimizada (reordenar de mayor a menor):
struct LogEntry_good {
    uint64_t  timestamp;   // 8
    uint32_t  thread_id;   // 4
    char      source[4];   // 4
    uint16_t  line;        // 2
    char      level;       // 1
    char      message;     // 1
};

int main(void) {
    printf("LogEntry_bad:  %zu bytes\n", sizeof(struct LogEntry_bad));
    printf("LogEntry_good: %zu bytes\n", sizeof(struct LogEntry_good));
    printf("Ahorro: %zu bytes (%zu%%)\n",
           sizeof(struct LogEntry_bad) - sizeof(struct LogEntry_good),
           100 * (sizeof(struct LogEntry_bad) - sizeof(struct LogEntry_good))
               / sizeof(struct LogEntry_bad));

    // En un buffer de 10000 entradas:
    printf("\n10000 entradas:\n");
    printf("  bad:  %zu KB\n", 10000 * sizeof(struct LogEntry_bad) / 1024);
    printf("  good: %zu KB\n", 10000 * sizeof(struct LogEntry_good) / 1024);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic optimize_struct.c -o optimize_struct && ./optimize_struct
rm -f optimize_struct
```

**Predicción**: ¿cuánto mide `LogEntry_bad`? ¿Y la versión optimizada?

<details><summary>Respuesta</summary>

```
LogEntry_bad:  32 bytes
LogEntry_good: 20 bytes  (→ redondeado a 24 por alignof 8)
Ahorro: 8 bytes (25%)

10000 entradas:
  bad:  312 KB
  good: 234 KB
```

La suma real de campos es 20 bytes. `LogEntry_bad` desperdicia 12 bytes en padding (37%). `LogEntry_good` al reordenar tiene un layout mucho más compacto: timestamp(8) + thread_id(4) + source(4) + line(2) + level(1) + message(1) = 20, redondeado a 24 (múltiplo de 8 por timestamp). En 10000 entradas: **~78 KB** de ahorro.

</details>
