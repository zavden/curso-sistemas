# T02 — Padding y alineación

## Por qué existe el padding

La CPU accede a memoria de forma más eficiente cuando los datos
están **alineados** a múltiplos de su tamaño. El compilador
inserta bytes de relleno (padding) para garantizar alineación:

```c
#include <stdio.h>

struct Bad {
    char a;       // 1 byte
    int b;        // 4 bytes — necesita alineación a 4
    char c;       // 1 byte
};

// Sin padding, Bad sería 6 bytes (1 + 4 + 1).
// PERO el compilador inserta padding:
//
// Offset 0: [a]           1 byte
// Offset 1: [pad][pad][pad]  3 bytes de padding (alinear b a múltiplo de 4)
// Offset 4: [b][b][b][b]  4 bytes
// Offset 8: [c]           1 byte
// Offset 9: [pad][pad][pad]  3 bytes de padding (alinear struct a múltiplo de 4)
//
// sizeof(struct Bad) = 12 (no 6)

int main(void) {
    printf("sizeof(struct Bad) = %zu\n", sizeof(struct Bad));    // 12
    return 0;
}
```

## Reglas de alineación

```c
// Regla 1: cada campo se alinea a un múltiplo de su tamaño natural:
// char   → alineación 1 (cualquier dirección)
// short  → alineación 2 (dirección par)
// int    → alineación 4 (múltiplo de 4)
// double → alineación 8 (múltiplo de 8)
// puntero → alineación 8 en 64-bit

// Regla 2: el struct completo se alinea al mayor de sus miembros:
struct Example {
    char a;      // align 1
    double b;    // align 8 ← el mayor
    int c;       // align 4
};
// struct align = 8
// sizeof debe ser múltiplo de 8

// Regla 3: sizeof(struct) es múltiplo de la alineación del struct.
// Esto garantiza que arrays de structs mantengan la alineación.
```

## Visualizar el layout

```c
#include <stdio.h>
#include <stddef.h>    // offsetof

struct A {
    char a;       // 1 byte
    double b;     // 8 bytes
    char c;       // 1 byte
};

struct B {
    double b;     // 8 bytes
    char a;       // 1 byte
    char c;       // 1 byte
};

int main(void) {
    printf("struct A: size=%zu\n", sizeof(struct A));
    printf("  a: offset=%zu, size=%zu\n", offsetof(struct A, a), sizeof(char));
    printf("  b: offset=%zu, size=%zu\n", offsetof(struct A, b), sizeof(double));
    printf("  c: offset=%zu, size=%zu\n", offsetof(struct A, c), sizeof(char));

    printf("struct B: size=%zu\n", sizeof(struct B));
    printf("  b: offset=%zu, size=%zu\n", offsetof(struct B, b), sizeof(double));
    printf("  a: offset=%zu, size=%zu\n", offsetof(struct B, a), sizeof(char));
    printf("  c: offset=%zu, size=%zu\n", offsetof(struct B, c), sizeof(char));

    return 0;
}

// struct A: size=24
//   a: offset=0, size=1      [a][pad×7]
//   b: offset=8, size=8      [bbbbbbbb]
//   c: offset=16, size=1     [c][pad×7]
//
// struct B: size=16
//   b: offset=0, size=8      [bbbbbbbb]
//   a: offset=8, size=1      [a]
//   c: offset=9, size=1      [c][pad×6]
//
// ¡MISMOS campos, diferente tamaño! (24 vs 16)
```

## Reordenar campos para ahorrar espacio

```c
// Regla práctica: ordenar campos de MAYOR a MENOR tamaño.
// Esto minimiza el padding:

// MAL — 24 bytes:
struct Bad {
    char a;       // 1 + 7 padding
    double b;     // 8
    char c;       // 1 + 7 padding
};

// BIEN — 16 bytes:
struct Good {
    double b;     // 8
    char a;       // 1
    char c;       // 1 + 6 padding
};

// MEJOR — agrupar por tamaño:
struct Best {
    double d;     // 8
    int i;        // 4
    short s;      // 2
    char a;       // 1
    char b;       // 1
};
// sizeof = 16 (8 + 4 + 2 + 1 + 1 = 16, ya alineado)
```

```c
// Ejemplo real: struct con muchos campos:

// Sin optimizar — 40 bytes:
struct UserBad {
    char active;         // 1 + 7
    double balance;      // 8
    char role;           // 1 + 3
    int id;              // 4
    short level;         // 2 + 6
};

// Optimizado — 24 bytes:
struct UserGood {
    double balance;      // 8
    int id;              // 4
    short level;         // 2
    char active;         // 1
    char role;           // 1
};
// Ahorro: 40% menos memoria.
// En un array de 1 millón de usuarios:
// Bad:  40 MB    Good: 24 MB
```

## offsetof

```c
#include <stddef.h>

// offsetof(type, member) — retorna el offset en bytes de un campo:
struct Data {
    int a;
    double b;
    char c;
};

size_t off_a = offsetof(struct Data, a);    // 0
size_t off_b = offsetof(struct Data, b);    // 8 (4 + 4 padding)
size_t off_c = offsetof(struct Data, c);    // 16

// Uso: calcular la dirección de un struct dado un puntero a un miembro:
// container_of — macro del kernel de Linux:
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct Data d = { .a = 1, .b = 2.0, .c = 'x' };
double *bp = &d.b;

// Recuperar el puntero al struct a partir de bp:
struct Data *dp = container_of(bp, struct Data, b);
printf("a = %d\n", dp->a);    // 1
```

## #pragma pack — Eliminar padding

```c
// #pragma pack fuerza una alineación específica:

#pragma pack(push, 1)    // alineación a 1 byte — sin padding
struct Packed {
    char a;       // 1
    int b;        // 4
    char c;       // 1
};
#pragma pack(pop)        // restaurar alineación normal

// sizeof(struct Packed) = 6 (sin padding)

// CUIDADO:
// 1. Accesos desalineados son LENTOS (o causan fault en ARM):
//    b está en offset 1, no alineado a 4.
//    La CPU puede necesitar 2 lecturas para leer b.
//
// 2. No portátil entre arquitecturas.
//
// 3. Usar SOLO cuando el layout exacto importa:
//    - Protocolos de red
//    - Formatos de archivo binario
//    - Comunicación con hardware
```

```c
// Alternativa GCC: __attribute__((packed))
struct __attribute__((packed)) PackedGCC {
    char a;
    int b;
    char c;
};
// sizeof = 6

// Se puede aplicar a campos individuales:
struct Mixed {
    char a;
    int b __attribute__((packed));    // solo b está packed
    char c;
};
```

## __attribute__((aligned))

```c
// Forzar alineación MAYOR a la natural:

struct __attribute__((aligned(64))) CacheLine {
    int data[16];    // 64 bytes
};
// Cada instancia se alinea a 64 bytes (tamaño de cache line).
// Útil para evitar false sharing en programas multithreaded.

// También para campos individuales:
struct WithAligned {
    int a;
    int b __attribute__((aligned(16)));    // b alineado a 16 bytes
};

// C11 estándar: _Alignas / alignas:
#include <stdalign.h>
struct AlignedC11 {
    alignas(16) int data[4];
};
```

## Verificar layout con static_assert

```c
#include <assert.h>
#include <stddef.h>

// Verificar que un struct tiene el tamaño esperado:
struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
};

static_assert(sizeof(struct Packet) == 8,
              "Packet must be exactly 8 bytes");

static_assert(offsetof(struct Packet, type) == 0, "type at offset 0");
static_assert(offsetof(struct Packet, flags) == 1, "flags at offset 1");
static_assert(offsetof(struct Packet, length) == 2, "length at offset 2");
static_assert(offsetof(struct Packet, payload) == 4, "payload at offset 4");

// Si el layout cambia (por reordenar campos o cambiar tipos),
// la compilación falla con un mensaje claro.
```

## Tabla de alineación típica (x86-64)

| Tipo | Tamaño | Alineación |
|---|---|---|
| char | 1 | 1 |
| short | 2 | 2 |
| int | 4 | 4 |
| long | 8 | 8 |
| float | 4 | 4 |
| double | 8 | 8 |
| puntero | 8 | 8 |
| long double | 16 | 16 |

---

## Ejercicios

### Ejercicio 1 — Predecir sizeof

```c
// Sin compilar, predecir sizeof de cada struct:
struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; char b; int c; };
struct D { double d; int i; char c; };
struct E { char c; double d; int i; };

// Luego compilar y verificar con offsetof.
// Reordenar E para minimizar su tamaño.
```

### Ejercicio 2 — Packed protocol

```c
// Definir un struct packed para un header de protocolo de red:
// - uint8_t  version (1 byte)
// - uint8_t  type (1 byte)
// - uint16_t length (2 bytes)
// - uint32_t sequence (4 bytes)
// - uint32_t checksum (4 bytes)
// Verificar con static_assert que sizeof == 12.
// Llenar el header y escribirlo a un archivo binario.
```

### Ejercicio 3 — container_of

```c
// Implementar el macro container_of.
// Crear struct Employee { char name[50]; int id; double salary; }.
// Dada la dirección de employee.salary, usar container_of
// para obtener el puntero al struct Employee.
// Imprimir el nombre y el id a partir del puntero recuperado.
```
