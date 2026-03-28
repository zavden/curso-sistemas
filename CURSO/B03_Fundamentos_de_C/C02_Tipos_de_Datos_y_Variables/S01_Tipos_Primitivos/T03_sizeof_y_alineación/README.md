# T03 — sizeof y alineación

## sizeof

`sizeof` es un operador (no una función) que retorna el tamaño
en bytes de un tipo o expresión. El resultado es de tipo `size_t`:

```c
#include <stdio.h>

int main(void) {
    // sizeof con un tipo — requiere paréntesis:
    printf("int:    %zu bytes\n", sizeof(int));
    printf("double: %zu bytes\n", sizeof(double));
    printf("char*:  %zu bytes\n", sizeof(char *));

    // sizeof con una expresión — paréntesis opcionales:
    int x = 42;
    printf("x:      %zu bytes\n", sizeof x);      // sin paréntesis
    printf("x:      %zu bytes\n", sizeof(x));      // con paréntesis (más común)

    // sizeof con arrays:
    int arr[10];
    printf("arr:    %zu bytes\n", sizeof(arr));     // 40 (10 × 4)
    printf("elem:   %zu bytes\n", sizeof(arr[0]));  // 4

    // Número de elementos de un array:
    size_t count = sizeof(arr) / sizeof(arr[0]);    // 10
    printf("count:  %zu\n", count);

    return 0;
}
```

### sizeof se evalúa en compilación

```c
// sizeof NO ejecuta la expresión — solo determina su tipo:
int x = 5;
size_t s = sizeof(x++);    // x sigue siendo 5, NO se incrementó
printf("x = %d\n", x);     // 5

// sizeof es una constante de compilación:
int arr[sizeof(int)];       // OK — sizeof(int) es constante

// Excepción: VLAs (variable-length arrays) — sizeof se evalúa en runtime
int n = 10;
int vla[n];
size_t vla_size = sizeof(vla);  // evaluado en runtime: n * sizeof(int)
```

### Trampas con sizeof

```c
// TRAMPA 1: sizeof de un array vs sizeof de un puntero:
int arr[10];
int *ptr = arr;

printf("%zu\n", sizeof(arr));    // 40 (tamaño del array)
printf("%zu\n", sizeof(ptr));    // 8 (tamaño del puntero, no del array)

// Cuando un array se pasa a una función, decay a puntero:
void foo(int arr[10]) {
    printf("%zu\n", sizeof(arr));  // 8 (es un puntero, no un array)
    // El [10] en el parámetro es ignorado por el compilador
}

// TRAMPA 2: sizeof de string literal:
printf("%zu\n", sizeof("hello"));   // 6 (incluye el '\0')
printf("%zu\n", strlen("hello"));   // 5 (NO incluye el '\0')

// TRAMPA 3: sizeof de struct puede ser mayor que la suma de sus campos
// (por padding — ver abajo)
```

### Macro ARRAY_SIZE

```c
// Patrón idiomático para contar elementos:
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int numbers[] = {10, 20, 30, 40, 50};
for (size_t i = 0; i < ARRAY_SIZE(numbers); i++) {
    printf("%d\n", numbers[i]);
}

// CUIDADO: no funciona con punteros:
int *ptr = numbers;
// ARRAY_SIZE(ptr) → sizeof(int*) / sizeof(int) = 2 (incorrecto)
```

## Alineación

La CPU accede a la memoria de forma más eficiente cuando los
datos están en direcciones que son múltiplo de su tamaño:

```
Dirección:  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
            ├──┤                                              char (1 byte) — cualquier dirección
            ├─────┤                                           short (2 bytes) — múltiplo de 2
            ├───────────┤                                     int (4 bytes) — múltiplo de 4
            ├───────────────────────┤                         double (8 bytes) — múltiplo de 8
```

```c
// Requisito de alineación de cada tipo:
// char:   1 byte  (cualquier dirección)
// short:  2 bytes (dirección múltiplo de 2)
// int:    4 bytes (dirección múltiplo de 4)
// double: 8 bytes (dirección múltiplo de 8)
// puntero: 4 u 8 bytes según la plataforma

// Si un int está en la dirección 0x1003 (no múltiplo de 4):
// - En x86: funciona pero más lento (dos accesos a memoria)
// - En ARM: puede causar una excepción (SIGBUS)
// - El compilador añade padding para evitar esto
```

### alignof (C11)

```c
#include <stdalign.h>    // macro alignof

printf("Alineación de char:   %zu\n", alignof(char));     // 1
printf("Alineación de short:  %zu\n", alignof(short));    // 2
printf("Alineación de int:    %zu\n", alignof(int));      // 4
printf("Alineación de double: %zu\n", alignof(double));   // 8
printf("Alineación de long double: %zu\n", alignof(long double)); // 16
```

## Padding en structs

El compilador inserta **bytes de padding** entre los campos
de un struct para cumplir con los requisitos de alineación:

```c
struct Bad {
    char  a;     // 1 byte
    int   b;     // 4 bytes — necesita estar en múltiplo de 4
    char  c;     // 1 byte
};

// Layout en memoria (con padding):
//
// Offset:  0     1  2  3    4  5  6  7    8     9  10 11
//         [a]  [pad pad pad][b  b  b  b]  [c]  [pad pad pad]
//
// sizeof(struct Bad) = 12 (no 6)

printf("%zu\n", sizeof(struct Bad));  // 12
```

```c
// ¿Por qué hay padding al final?
// Para que arrays de structs mantengan la alineación:
struct Bad arr[2];
// arr[0] empieza en dirección múltiplo de 4
// arr[1] también debe empezar en múltiplo de 4
// Si el struct fuera 9 bytes, arr[1] estaría desalineado
// El padding final garantiza que sizeof es múltiplo de la
// alineación máxima del struct
```

### Reordenar campos para reducir padding

```c
// Mismo struct, campos reordenados:
struct Good {
    int   b;     // 4 bytes
    char  a;     // 1 byte
    char  c;     // 1 byte
};

// Layout en memoria:
//
// Offset:  0  1  2  3    4     5     6  7
//         [b  b  b  b]  [a]   [c]  [pad pad]
//
// sizeof(struct Good) = 8 (no 12)

printf("%zu\n", sizeof(struct Good));  // 8
```

```c
// Regla general: ordenar campos de mayor a menor tamaño:
struct Optimal {
    double d;    // 8 bytes (alineación 8)
    int    i;    // 4 bytes (alineación 4)
    short  s;    // 2 bytes (alineación 2)
    char   c;    // 1 byte  (alineación 1)
    char   pad;  // 1 byte  (padding implícito para alinear a 8)
};
// sizeof = 16 (mínimo posible para estos campos)

struct Terrible {
    char   c;    // 1 + 7 padding
    double d;    // 8
    char   c2;   // 1 + 3 padding
    int    i;    // 4
    char   c3;   // 1 + 1 padding
    short  s;    // 2
};
// sizeof = 32 (el doble, mismos datos)
```

### offsetof

```c
#include <stddef.h>    // offsetof

struct Example {
    char  a;
    int   b;
    char  c;
    double d;
};

printf("offset a: %zu\n", offsetof(struct Example, a));  // 0
printf("offset b: %zu\n", offsetof(struct Example, b));  // 4 (3 bytes padding)
printf("offset c: %zu\n", offsetof(struct Example, c));  // 8
printf("offset d: %zu\n", offsetof(struct Example, d));  // 16 (7 bytes padding)
printf("total:    %zu\n", sizeof(struct Example));        // 24
```

## #pragma pack — Eliminar padding

```c
// #pragma pack fuerza una alineación menor:
#pragma pack(push, 1)    // guardar alineación actual, forzar a 1 byte
struct Packed {
    char  a;     // 1 byte
    int   b;     // 4 bytes (sin padding antes)
    char  c;     // 1 byte
};
#pragma pack(pop)        // restaurar alineación original

printf("%zu\n", sizeof(struct Packed));  // 6 (sin padding)
```

```c
// GCC también soporta __attribute__((packed)):
struct Packed2 {
    char  a;
    int   b;
    char  c;
} __attribute__((packed));

printf("%zu\n", sizeof(struct Packed2));  // 6
```

### Cuándo usar pack

```c
// Usar pack SOLO cuando es necesario:

// 1. Protocolos de red — los bytes deben estar exactamente donde se esperan:
#pragma pack(push, 1)
struct TCPHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_number;
    uint32_t ack_number;
    // ...
};
#pragma pack(pop)

// 2. Formatos de archivo binario:
#pragma pack(push, 1)
struct BMPHeader {
    uint16_t magic;       // 'BM'
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
};
#pragma pack(pop)

// 3. Comunicación con hardware (registros mapeados en memoria)
```

### Peligros de pack

```c
// Acceso desalineado = más lento en x86, crash en ARM:
#pragma pack(push, 1)
struct Packed {
    char  a;
    int   b;     // en dirección no alineada
};
#pragma pack(pop)

struct Packed p;
int *ptr = &p.b;     // ptr apunta a dirección no alineada
*ptr = 42;           // x86: funciona (lento). ARM: puede crashear

// NUNCA tomar la dirección de un campo en un struct packed
// si vas a usarlo como puntero de su tipo.
```

## alignas (C11) — Forzar alineación mayor

```c
#include <stdalign.h>

// Forzar una alineación mayor que la natural:
alignas(16) int data[4];          // alineado a 16 bytes
alignas(64) char cache_line[64];  // alineado a línea de caché

// Útil para:
// - SIMD (SSE/AVX requieren alineación a 16/32 bytes)
// - Evitar false sharing entre threads (alinear a 64 bytes)
// - Hardware que requiere alineación específica
```

```c
// alignas en structs:
struct alignas(64) CacheLine {
    int data[16];     // ocupa exactamente una línea de caché
};

// O por campo:
struct Mixed {
    alignas(16) float vector[4];   // alineado a 16 (para SIMD)
    int flags;
};
```

## Visualizar el layout

```c
// Programa para inspeccionar el layout de un struct:
#include <stdio.h>
#include <stddef.h>

struct Demo {
    char   a;
    int    b;
    char   c;
    double d;
    short  e;
};

int main(void) {
    printf("sizeof(struct Demo) = %zu\n", sizeof(struct Demo));
    printf("alignof(struct Demo) = %zu\n", alignof(struct Demo));
    printf("\n");
    printf("a: offset=%2zu  size=%zu\n", offsetof(struct Demo, a), sizeof(char));
    printf("b: offset=%2zu  size=%zu\n", offsetof(struct Demo, b), sizeof(int));
    printf("c: offset=%2zu  size=%zu\n", offsetof(struct Demo, c), sizeof(char));
    printf("d: offset=%2zu  size=%zu\n", offsetof(struct Demo, d), sizeof(double));
    printf("e: offset=%2zu  size=%zu\n", offsetof(struct Demo, e), sizeof(short));
    return 0;
}

// Salida típica:
// sizeof(struct Demo) = 32
// alignof(struct Demo) = 8
//
// a: offset= 0  size=1
// b: offset= 4  size=4   ← 3 bytes de padding después de a
// c: offset= 8  size=1
// d: offset=16  size=8   ← 7 bytes de padding después de c
// e: offset=24  size=2
//                         ← 6 bytes de padding al final
```

---

## Ejercicios

### Ejercicio 1 — Padding

```c
// Calcular a mano el sizeof de estos structs.
// Luego verificar con un programa.

struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; double b; char c; };
struct D { double b; char a; char c; };
```

### Ejercicio 2 — Optimizar layout

```c
// Este struct tiene mucho padding. Reordenar los campos
// para minimizar sizeof sin perder ningún campo.
// ¿Cuánto se reduce?

struct Wasteful {
    char   name;
    double salary;
    char   grade;
    int    id;
    char   dept;
    short  level;
};
```

### Ejercicio 3 — offsetof

```c
// Escribir un programa que imprima el offset, tamaño y
// padding de cada campo del struct del Ejercicio 2
// (antes y después de optimizar).
```
