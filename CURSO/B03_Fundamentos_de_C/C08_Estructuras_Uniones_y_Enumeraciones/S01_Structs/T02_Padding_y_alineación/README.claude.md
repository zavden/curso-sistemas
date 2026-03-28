# T02 — Padding y alineación

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` línea 138 | Dice `struct UserBad` tiene 40 bytes | Tiene **32 bytes**: `active`(1+7pad) + `balance`(8) + `role`(1+3pad) + `id`(4) + `level`(2+6tail) = 32. Las propias anotaciones del código suman 8+8+4+4+8=32, no 40 |
| `README.md` línea 147 | Dice `struct UserGood` tiene 24 bytes | Tiene **16 bytes**: `balance`(8) + `id`(4) + `level`(2) + `active`(1) + `role`(1) = 16, sin padding. El ahorro real es 50% (32→16), no 40% |

---

## 1. Por qué existe el padding

La CPU accede a memoria de forma más eficiente cuando los datos están **alineados** — es decir, ubicados en direcciones que son múltiplos de su tamaño. Un `int` de 4 bytes se lee más rápido si está en dirección 0, 4, 8... que si está en dirección 1 o 3.

El compilador inserta automáticamente bytes de relleno (**padding**) entre los campos de un struct para garantizar esta alineación:

```c
#include <stdio.h>

struct Bad {
    char a;       // 1 byte
    int  b;       // 4 bytes — necesita alineación a 4
    char c;       // 1 byte
};

// Layout real en memoria (x86-64):
//
// Offset 0: [a ]              1 byte  (char, align 1)
// Offset 1: [..][..][..]      3 bytes de PADDING (alinear b a múltiplo de 4)
// Offset 4: [b ][b ][b ][b ]  4 bytes  (int, align 4)
// Offset 8: [c ]              1 byte  (char, align 1)
// Offset 9: [..][..][..]      3 bytes de TAIL PADDING (sizeof múltiplo de 4)
//
// sizeof(struct Bad) = 12    (no 6)
// Datos reales: 6 bytes.  Padding: 6 bytes.  50% desperdicio.

int main(void) {
    printf("sizeof(struct Bad) = %zu\n", sizeof(struct Bad));    // 12
    return 0;
}
```

Sin padding, el `int b` quedaría en offset 1 — dirección no alineada. En x86-64 esto funciona pero es **más lento** (la CPU puede necesitar 2 lecturas de memoria). En ARM estricto, un acceso desalineado causa **SIGBUS** (crash).

---

## 2. Las tres reglas de alineación

### Regla 1: Alineación de cada campo

Cada campo se alinea a un **múltiplo de su tamaño natural**:

| Tipo | Tamaño | Alineación | Restricción de offset |
|------|--------|------------|----------------------|
| `char` | 1 | 1 | Cualquier offset |
| `short` | 2 | 2 | Offset par (0, 2, 4...) |
| `int` | 4 | 4 | Múltiplo de 4 (0, 4, 8...) |
| `float` | 4 | 4 | Múltiplo de 4 |
| `double` | 8 | 8 | Múltiplo de 8 (0, 8, 16...) |
| `long` | 8 | 8 | Múltiplo de 8 |
| puntero | 8 | 8 | Múltiplo de 8 |
| `long double` | 16 | 16 | Múltiplo de 16 |

Ejemplo:

```c
struct RuleDemo1 {
    char  a;     // offset 0 (align 1, OK)
    short b;     // offset 1 → NO es par → padding 1 byte → offset 2
    char  c;     // offset 4 (align 1, OK)
    int   d;     // offset 5 → NO múltiplo de 4 → padding 3 bytes → offset 8
};
// sizeof = 12  (align del struct = 4, 12 es múltiplo de 4)
```

### Regla 2: Alineación del struct

La alineación del struct completo es igual al **miembro con mayor alineación**:

```c
struct RuleDemo2 {
    char   a;     // align 1
    double b;     // align 8 ← el mayor
    int    c;     // align 4
};
// Alineación del struct = 8 (por el double)
// sizeof debe ser múltiplo de 8
```

### Regla 3: Tail padding (sizeof múltiplo de alineación)

`sizeof(struct)` siempre es un **múltiplo de la alineación** del struct. Esto garantiza que en un **array** de structs, cada elemento mantenga la alineación correcta:

```c
struct TailPad {
    double d;    // offset 0, 8 bytes
    int    i;    // offset 8, 4 bytes
    char   c;    // offset 12, 1 byte → datos terminan en byte 13
};
// Alineación = 8 (por double). 13 no es múltiplo de 8.
// Siguiente múltiplo de 8 → 16. Tail padding: 3 bytes.
// sizeof = 16

// Sin tail padding: arr[0] terminaría en byte 13.
// arr[1].d empezaría en offset 13 — NO múltiplo de 8 → violación.
```

---

## 3. Visualizar layout con offsetof

La macro `offsetof` (de `<stddef.h>`) retorna el offset en bytes de un campo dentro de un struct:

```c
#include <stdio.h>
#include <stddef.h>

struct A {
    char   a;     // 1 byte
    double b;     // 8 bytes
    char   c;     // 1 byte
};

struct B {
    double b;     // 8 bytes
    char   a;     // 1 byte
    char   c;     // 1 byte
};

int main(void) {
    printf("struct A: size=%zu\n", sizeof(struct A));
    printf("  a: offset=%zu\n", offsetof(struct A, a));    // 0
    printf("  b: offset=%zu\n", offsetof(struct A, b));    // 8  (7 bytes pad)
    printf("  c: offset=%zu\n", offsetof(struct A, c));    // 16

    printf("struct B: size=%zu\n", sizeof(struct B));
    printf("  b: offset=%zu\n", offsetof(struct B, b));    // 0
    printf("  a: offset=%zu\n", offsetof(struct B, a));    // 8
    printf("  c: offset=%zu\n", offsetof(struct B, c));    // 9
    return 0;
}
```

**Resultado**: `struct A` = 24 bytes, `struct B` = 16 bytes. **Mismos campos, diferente tamaño** — solo por el orden de declaración.

### Técnica memset para revelar padding

Llenar la memoria con un patrón conocido (ej. `0xAA`) antes de asignar campos revela visualmente qué bytes son datos y cuáles son padding:

```c
struct Bad bad;
memset(&bad, 0xAA, sizeof(bad));    // llenar todo con 0xAA
bad.a = 'X';     // 0x58 — este byte ya no es 0xAA
bad.b = 0.0;     // 8 bytes de 0x00 — estos tampoco
// Bytes que conservan 0xAA → padding
```

---

## 4. Reordenar campos para minimizar padding

**Regla práctica**: ordenar campos de **mayor a menor tamaño**. Esto minimiza (o elimina) el padding:

```c
// MAL — 24 bytes (8 padding, 33% desperdicio):
struct Bad {
    char   a;       // 1 + 7 pad
    double b;       // 8
    char   c;       // 1 + 7 tail pad
};

// BIEN — 16 bytes (0 padding):
struct Good {
    double b;       // 8
    char   a;       // 1
    char   c;       // 1 + 6 tail pad
};
```

Ejemplo realista con 5 campos:

```c
// Sin optimizar — 32 bytes:
struct UserBad {
    char   active;       // offset 0:  1 + 7 pad = 8
    double balance;      // offset 8:  8
    char   role;         // offset 16: 1 + 3 pad = 4
    int    id;           // offset 20: 4
    short  level;        // offset 24: 2 + 6 tail pad = 8
};                       // Total: 32 bytes (16 datos, 16 padding)

// Optimizado (mayor a menor) — 16 bytes:
struct UserGood {
    double balance;      // offset 0:  8
    int    id;           // offset 8:  4
    short  level;        // offset 12: 2
    char   active;       // offset 14: 1
    char   role;         // offset 15: 1
};                       // Total: 16 bytes (16 datos, 0 padding)

// Ahorro: 50%. En un array de 1 millón de registros:
// UserBad:  32 MB    UserGood: 16 MB
```

### Agrupar por tamaño

Cuando no puedes estrictamente ordenar de mayor a menor (por legibilidad semántica), agrupa los campos del mismo tamaño:

```c
struct Best {
    double d;     // 8
    int    i;     // 4
    short  s;     // 2
    char   a;     // 1
    char   b;     // 1
};
// sizeof = 16 (8 + 4 + 2 + 1 + 1 = 16, ya alineado a 8)
```

---

## 5. offsetof y container_of

### offsetof

`offsetof(type, member)` calcula el desplazamiento en bytes de un campo respecto al inicio del struct. Definido en `<stddef.h>`:

```c
#include <stddef.h>

struct Data {
    int    a;       // offset 0
    double b;       // offset 8 (4 bytes de a + 4 padding)
    char   c;       // offset 16
};

size_t off_b = offsetof(struct Data, b);    // 8
```

### container_of (macro del kernel Linux)

Dado un puntero a un **miembro** de un struct, calcula el puntero al **struct que lo contiene**:

```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```

Funcionamiento:
1. `(char *)(ptr)` — convierte el puntero a `char *` para aritmética de bytes
2. `offsetof(type, member)` — cuántos bytes desde el inicio del struct hasta el miembro
3. Resta el offset → dirección del inicio del struct

```c
struct Data d = { .a = 42, .b = 3.14, .c = 'x' };
double *bp = &d.b;

// Recuperar puntero al struct completo desde bp:
struct Data *dp = container_of(bp, struct Data, b);
printf("a = %d\n", dp->a);    // 42
```

Este patrón es fundamental en el kernel Linux para implementar listas enlazadas genéricas — el nodo de la lista está **incrustado** dentro del struct de datos, y `container_of` recupera el struct a partir del nodo.

---

## 6. `#pragma pack` y `__attribute__((packed))`

### Eliminar todo el padding

`__attribute__((packed))` (GCC/Clang) fuerza alineación a 1 byte — elimina todo el padding:

```c
struct __attribute__((packed)) Packed {
    char a;       // offset 0
    int  b;       // offset 1 (¡no alineado!)
    char c;       // offset 5
};
// sizeof = 6 (sin padding)
```

Equivalente con `#pragma pack`:

```c
#pragma pack(push, 1)    // guardar alineación actual, forzar 1
struct Packed {
    char a;
    int  b;
    char c;
};
#pragma pack(pop)        // restaurar alineación anterior
```

### Consecuencias de packed

1. **Rendimiento**: accesos desalineados en x86-64 son más lentos (la CPU puede necesitar 2 lecturas de memoria para un solo `int`)
2. **Portabilidad**: en ARM estricto, acceso desalineado causa **SIGBUS** (crash)
3. **Punteros**: tomar `&packed_struct.field` produce un puntero desalineado — pasarlo a funciones que esperan alineación natural es **UB**

### Cuándo usar packed

Solo cuando el **layout exacto en bytes** importa:
- **Protocolos de red**: cada byte debe estar en la posición exacta
- **Formatos de archivo binario**: headers de archivos BMP, WAV, etc.
- **Comunicación con hardware**: registros mapeados en memoria

```c
struct __attribute__((packed)) NetHeader {
    uint8_t  version;     // offset 0
    uint8_t  type;        // offset 1
    uint16_t length;      // offset 2
    uint32_t sequence;    // offset 4
    uint32_t checksum;    // offset 8
};
// sizeof = 12 — exacto, sin sorpresas
```

---

## 7. Packing selectivo

Se puede aplicar `packed` a **campos individuales** en lugar de al struct completo:

```c
struct Selective {
    char   tag;
    int    value __attribute__((packed));    // solo value sin padding previo
    double score;                            // mantiene alineación natural a 8
};
```

Layout:
- `tag`: offset 0 (1 byte)
- `value`: offset 1 (packed — sin los 3 bytes de padding que normalmente habría)
- `score`: offset 8 (alineado a 8 normalmente, con 3 bytes de padding entre value y score)

Resultado: `sizeof = 16, alignof = 8`. Ahorra 3 bytes respecto al layout normal (donde `value` estaría en offset 4 y sizeof sería 16 igualmente en este caso... depende del struct).

**Ventaja**: compromiso entre espacio y rendimiento — solo los campos que realmente necesitan estar empaquetados pierden alineación, los demás conservan accesos eficientes.

---

## 8. `_Alignas` / `alignas` — sobre-alineación (C11)

### alignof — consultar la alineación

`alignof(type)` retorna el requisito de alineación de un tipo:

```c
#include <stdalign.h>

printf("alignof(char):   %zu\n", alignof(char));         // 1
printf("alignof(int):    %zu\n", alignof(int));           // 4
printf("alignof(double): %zu\n", alignof(double));        // 8
printf("alignof(void *): %zu\n", alignof(void *));        // 8
printf("alignof(max_align_t): %zu\n", alignof(max_align_t)); // 16
```

`max_align_t` (de `<stddef.h>`) es el tipo con la alineación más grande del sistema. En x86-64 con glibc es típicamente 16 (por `long double`). **Todo lo que `malloc` retorna está alineado al menos a `alignof(max_align_t)`**.

### alignas — forzar alineación mayor

`alignas(N)` fuerza un tipo o variable a tener alineación de al menos N bytes (N debe ser potencia de 2):

```c
// En un struct:
struct BasicAligned {
    alignas(16) int data[4];    // data alineado a 16, no a 4
};
// sizeof = 16, alignof = 16

// En un miembro individual:
struct MemberAligned {
    char tag;
    alignas(16) int vector[4];    // forzado a offset múltiplo de 16
    char flag;
};
// tag en offset 0, vector en offset 16 (15 bytes de padding), flag en offset 32
// sizeof = 48, alignof = 16

// En variables locales:
alignas(32) char buffer[128];    // buffer alineado a 32 en el stack
```

### Para qué sirve la sobre-alineación

- **SIMD (SSE/AVX)**: instrucciones SSE requieren datos alineados a 16 bytes, AVX a 32 bytes
- **Alocación dinámica sobre-alineada**: `malloc` garantiza `alignof(max_align_t)` (16). Para alineación mayor, usar `aligned_alloc(alignment, size)` o `posix_memalign`

---

## 9. Cache line alignment y false sharing

### Qué es un cache line

La CPU no lee bytes individuales de RAM — lee bloques de **64 bytes** llamados **cache lines**. Cuando un core modifica un byte, invalida el cache line completo en todos los demás cores.

### False sharing

Cuando dos hilos modifican variables **diferentes** que residen en el **mismo cache line**, cada escritura invalida el cache del otro core. Ambos hilos se ralentizan mutuamente a pesar de no compartir datos lógicamente — esto es **false sharing**.

### Prevención con alineación a 64 bytes

```c
#define CACHE_LINE 64

struct __attribute__((aligned(CACHE_LINE))) Counter {
    int value;
    // El struct ocupa un cache line completo (64 bytes)
    // gracias a la alineación forzada
};

struct Counter counters[2];
// counters[0] está en un cache line
// counters[1] está en otro cache line (64 bytes después)
// Dos hilos pueden modificar value[0] y value[1] sin interferencia
```

**Resultado**: `sizeof(Counter) = 64`, cada instancia ocupa exactamente un cache line. La distancia entre `counters[0]` y `counters[1]` es 64 bytes → cache lines separados.

**Costo**: desperdicia 60 bytes por counter (solo usa 4 de 64). Es un trade-off válido cuando el rendimiento multithreaded importa más que la memoria.

### Equivalente C11 estándar

```c
#include <stdalign.h>
struct alignas(64) Counter {
    int value;
};
```

---

## 10. `_Static_assert` para validar layout — Conexión con Rust

### _Static_assert (C11)

Verifica condiciones en **tiempo de compilación**. Si la condición es falsa, la compilación falla con el mensaje indicado:

```c
#include <assert.h>
#include <stddef.h>

struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
};

static_assert(sizeof(struct Packet) == 8,
              "Packet must be exactly 8 bytes");
static_assert(offsetof(struct Packet, length) == 2,
              "length must be at offset 2");
```

Si alguien modifica el struct (agrega un campo, cambia un tipo), la compilación falla con un mensaje claro **en lugar de producir datos corruptos silenciosamente**. Esto es esencial para protocolos de red y formatos binarios.

### Conexión con Rust

En Rust, el compilador tiene **libertad total** para reordenar los campos de un struct:

```rust
struct Point {
    x: i32,
    y: i32,
}
// El compilador puede poner y antes de x si optimiza mejor el padding
```

Para obtener un layout predecible (compatible con C), se usa `#[repr(C)]`:

```rust
#[repr(C)]
struct Point {
    x: i32,
    y: i32,
}
// Layout idéntico a C: x en offset 0, y en offset 4
```

| Concepto | C | Rust |
|----------|---|------|
| Orden de campos | Fijo (orden de declaración) | Libre (compilador reordena) |
| Layout predecible | Siempre (para misma ABI) | Solo con `#[repr(C)]` |
| Packed | `__attribute__((packed))` | `#[repr(packed)]` |
| Sobre-alineación | `_Alignas(N)` / `aligned(N)` | `#[repr(align(N))]` |
| Verificar tamaño | `static_assert(sizeof(...))` | `const _: () = assert!(size_of::<T>() == N);` |
| Padding automático | Sí, según ABI | Sí, pero puede reordenar para minimizar |

Rust minimiza padding **automáticamente** al reordenar campos. En C, el programador debe reordenar manualmente. Esto es una ventaja de Rust — obtener el layout óptimo sin esfuerzo:

```rust
// Rust puede reordenar internamente para ahorrar espacio:
struct User {
    active: bool,       // 1 byte
    balance: f64,       // 8 bytes
    role: u8,           // 1 byte
    id: u32,            // 4 bytes
    level: u16,         // 2 bytes
}
// Rust puede reorganizar a: balance, id, level, active, role
// → 16 bytes automáticamente, sin intervención del programador
```

---

## Ejercicios

### Ejercicio 1 — Predecir sizeof sin compilar

```c
// Para cada struct, calcula sizeof manualmente usando las 3 reglas.
// Anota el offset de CADA campo y el padding insertado.
// Luego compila y verifica con offsetof.

struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; char b; int c; };
struct D { double d; int i; char c; };
struct E { char c; double d; int i; };

// Reordena struct E para minimizar su tamaño.
// ¿Cuánto ahorraste?
```

<details><summary>Predicción</summary>

- **struct A** `{char, int, char}`: a(0,1) +3pad, b(4,4), c(8,1) +3tail = **12 bytes** (align 4)
- **struct B** `{int, char, char}`: b(0,4), a(4,1), c(5,1) +2tail = **8 bytes** (align 4)
- **struct C** `{char, char, int}`: a(0,1), b(1,1) +2pad, c(4,4) = **8 bytes** (align 4)
- **struct D** `{double, int, char}`: d(0,8), i(8,4), c(12,1) +3tail = **16 bytes** (align 8)
- **struct E** `{char, double, int}`: c(0,1) +7pad, d(8,8), i(16,4) +4tail = **24 bytes** (align 8)
- **E reordenado** → `{double, int, char}` = **16 bytes**. Ahorro: 8 bytes (33%)
</details>

### Ejercicio 2 — Mapa byte a byte con memset

```c
// Declara:
//   struct Mixed { char a; int b; short c; double d; char e; };
//
// 1. Usa memset(&m, 0xAA, sizeof(m)) para llenar con 0xAA
// 2. Asigna valores a todos los campos
// 3. Imprime cada byte con un loop:
//      for (size_t i = 0; i < sizeof(m); i++)
//          printf("byte[%2zu] = 0x%02X %s\n", i, bytes[i],
//                 bytes[i] == 0xAA ? "(pad)" : "(data)");
// 4. Marca cuáles bytes son padding y cuáles son datos
// 5. Calcula el porcentaje de desperdicio
```

<details><summary>Predicción</summary>

Layout de `struct Mixed { char a; int b; short c; double d; char e; }`:
- `a`: offset 0 (1 byte)
- pad: 3 bytes (alinear int a 4)
- `b`: offset 4 (4 bytes)
- `c`: offset 8 (2 bytes)
- pad: 6 bytes (alinear double a 8)
- `d`: offset 16 (8 bytes)
- `e`: offset 24 (1 byte)
- tail pad: 7 bytes (sizeof múltiplo de 8)
- **sizeof = 32 bytes**. Datos reales: 1+4+2+8+1 = 16. Padding: 16. **50% desperdicio**
- Reordenado como `{double, int, short, char, char}` → 16 bytes, 0 padding
</details>

### Ejercicio 3 — Optimizar un struct real

```c
// Este struct tiene un layout ineficiente:
struct Record {
    char     flag;           // 1 byte
    double   value;          // 8 bytes
    char     type;           // 1 byte
    int      count;          // 4 bytes
    char     status;         // 1 byte
    double   timestamp;      // 8 bytes
    short    code;           // 2 bytes
};
//
// 1. Calcula sizeof con el orden actual (predice, luego verifica)
// 2. Reordena los campos de mayor a menor para minimizar padding
// 3. Calcula sizeof del struct optimizado
// 4. ¿Cuánto ahorraste? ¿Cuánto es para un array de 1 millón?
// 5. Verifica ambos con sizeof y offsetof
```

<details><summary>Predicción</summary>

**Orden original**:
- flag(0,1) +7pad, value(8,8), type(16,1) +3pad, count(20,4), status(24,1) +7pad, timestamp(32,8), code(40,2) +6tail
- **sizeof = 48 bytes**. Datos reales: 1+8+1+4+1+8+2 = 25. Padding: 23 (48%)

**Optimizado (mayor a menor)**:
```c
struct RecordOpt {
    double   value;          // offset 0, 8
    double   timestamp;      // offset 8, 8
    int      count;          // offset 16, 4
    short    code;           // offset 20, 2
    char     flag;           // offset 22, 1
    char     type;           // offset 23, 1
    char     status;         // offset 24, 1
};
```
- 25 bytes de datos, sizeof = **32 bytes** (align 8, siguiente múltiplo de 8 después de 25). Tail padding: 7 bytes
- Ahorro: 48 → 32 = **16 bytes** (33%)
- Para 1 millón: 48 MB → 32 MB, ahorro de 16 MB
</details>

### Ejercicio 4 — offsetof para explorar layout

```c
// Declara:
//   struct Packet {
//       uint8_t  version;
//       uint32_t src_ip;
//       uint16_t src_port;
//       uint32_t dst_ip;
//       uint16_t dst_port;
//       uint8_t  flags;
//   };
//
// 1. Sin compilar, predice el offset de cada campo y sizeof total
// 2. Imprime con offsetof cada campo:
//      printf("%-10s offset=%2zu  size=%zu\n", "version",
//             offsetof(struct Packet, version), sizeof(uint8_t));
// 3. ¿Hay padding entre src_port y dst_ip? ¿Por qué?
// 4. ¿Cuál es la alineación del struct? (usa alignof)
```

<details><summary>Predicción</summary>

- `version`: offset 0 (1 byte), +3 pad (alinear src_ip a 4)
- `src_ip`: offset 4 (4 bytes)
- `src_port`: offset 8 (2 bytes), +2 pad (alinear dst_ip a 4)
- `dst_ip`: offset 12 (4 bytes)
- `dst_port`: offset 16 (2 bytes)
- `flags`: offset 18 (1 byte), +1 tail pad (sizeof múltiplo de 4)
- **sizeof = 20 bytes**, alignof = 4
- Sí hay padding entre `src_port` y `dst_ip`: 2 bytes, porque `uint32_t` necesita offset múltiplo de 4, y `src_port` termina en offset 10
</details>

### Ejercicio 5 — container_of

```c
// 1. Implementa el macro container_of:
//    #define container_of(ptr, type, member) \
//        ((type *)((char *)(ptr) - offsetof(type, member)))
//
// 2. Declara:
//    struct Employee {
//        char   name[50];
//        int    id;
//        double salary;
//    };
//
// 3. Crea un Employee en el stack: {"Alice", 42, 75000.0}
// 4. Toma puntero al campo salary: double *sp = &emp.salary;
// 5. Usa container_of para recuperar el puntero al Employee
// 6. Imprime emp.name y emp.id desde el puntero recuperado
// 7. Verifica que el puntero recuperado == &emp
```

<details><summary>Predicción</summary>

- `offsetof(struct Employee, salary)` = 56 (name[50] + 2pad para alinear int + int(4) = 56. Wait, let me recalculate: name[50] ends at offset 50. int id needs align 4. 50 → 52 (2 pad). id at 52 (4 bytes) → 56. double salary needs align 8. 56 is multiple of 8. salary at 56)
- `sp` apunta a `&emp + 56 bytes`
- `container_of(sp, struct Employee, salary)` = `(char *)sp - 56` = `&emp`
- Imprimirá: `name = "Alice"`, `id = 42`
- Verificación: `dp == &emp` → true
- Este patrón es la base de las listas enlazadas intrusivas del kernel Linux
</details>

### Ejercicio 6 — Struct packed para protocolo

```c
// Define un header de protocolo de red packed:
//   struct __attribute__((packed)) DNSHeader {
//       uint16_t id;
//       uint16_t flags;
//       uint16_t qd_count;
//       uint16_t an_count;
//       uint16_t ns_count;
//       uint16_t ar_count;
//   };
//
// 1. Verifica con _Static_assert que sizeof == 12
// 2. Verifica con _Static_assert que cada campo está en el offset correcto
// 3. Llena un header con valores de ejemplo
// 4. Serializa a un buffer con memcpy
// 5. Imprime los 12 bytes en hex
// 6. Deserializa de vuelta a otro struct y verifica que los valores coinciden
//
// Pregunta: ¿Este struct NECESITA packed? ¿Tiene padding sin packed?
```

<details><summary>Predicción</summary>

- sizeof = **12 bytes** (6 × uint16_t de 2 bytes)
- Offsets: id(0), flags(2), qd_count(4), an_count(6), ns_count(8), ar_count(10)
- Todos los campos son `uint16_t` (align 2), y cada offset es par → **no hay padding** incluso sin packed
- Este struct **no necesita** packed — todos los campos tienen la misma alineación (2) y están contiguos naturalmente
- El packed sería necesario si hubiera mezcla de tamaños (ej. `uint8_t` seguido de `uint32_t`)
- La serialización con memcpy funcionará igual con o sin packed en este caso
</details>

### Ejercicio 7 — static_assert como red de seguridad

```c
// Declara un struct con layout exacto para un header de archivo:
//   struct __attribute__((packed)) BMPHeader {
//       uint16_t signature;    // "BM"
//       uint32_t file_size;
//       uint16_t reserved1;
//       uint16_t reserved2;
//       uint32_t data_offset;
//   };
//
// 1. Agrega _Static_assert para sizeof == 14
// 2. Agrega _Static_assert para el offset de CADA campo
// 3. Compila y verifica que todo pasa
// 4. Ahora ROMPE el layout: cambia uint16_t signature a uint32_t
// 5. Compila — ¿qué static_asserts fallan? ¿Cuántos?
// 6. Restaura el original y verifica que compila limpio
```

<details><summary>Predicción</summary>

- sizeof original: 2 + 4 + 2 + 2 + 4 = **14 bytes** (packed)
- Offsets: signature(0), file_size(2), reserved1(6), reserved2(8), data_offset(10)
- Al cambiar signature a uint32_t:
  - sizeof = 16 (4 + 4 + 2 + 2 + 4) → falla `sizeof == 14`
  - file_size pasa de offset 2 a offset 4 → falla `file_size at 2`
  - reserved1 pasa de offset 6 a offset 8 → falla
  - reserved2 pasa de offset 8 a offset 10 → falla
  - data_offset pasa de offset 10 a offset 12 → falla
  - **5 static_asserts fallan** (sizeof + 4 offsets), impidiendo compilación con datos corruptos
</details>

### Ejercicio 8 — Packing selectivo vs completo

```c
// Declara TRES versiones del mismo struct:
//   struct Normal    { char tag; int value; double score; };
//   struct FullPacked  __attribute__((packed)) { char tag; int value; double score; };
//   struct SelectPacked { char tag; int value __attribute__((packed)); double score; };
//
// Para cada uno, imprime:
//   sizeof, alignof, y offset de cada campo
//
// ¿Cuál tiene mejor compromiso entre tamaño y rendimiento?
// ¿En cuál el acceso a 'score' es siempre eficiente?
```

<details><summary>Predicción</summary>

- **Normal**: tag(0,1) +3pad, value(4,4), score(8,8). sizeof = **16**, alignof = 8. `score` alineado a 8 ✓
- **FullPacked**: tag(0,1), value(1,4), score(5,8). sizeof = **13**, alignof = 1. `score` en offset 5 — **desalineado** ✗
- **SelectPacked**: tag(0,1), value(1,4), score(8,8). sizeof = **16**, alignof = 8. `score` alineado a 8 ✓

Wait — SelectPacked: tag at 0 (1), value at 1 (packed, 4 bytes, ends at 5). score needs align 8 → next multiple of 8 after 5 is 8. So score at offset 8. sizeof = 16. alignof = 8.

Hmm, so Normal and SelectPacked both have sizeof = 16. The selective packing doesn't save space here because `double score` still needs alignment 8 and forces padding anyway.

Let me reconsider: Normal has value at offset 4 (3 pad), SelectPacked has value at offset 1 (0 pad between tag and value, but 3 pad between value and score). Both end up 16 bytes. So in this specific case, selective packing doesn't help.

- **Mejor compromiso**: en este caso, Normal y SelectPacked tienen **el mismo sizeof** (16). FullPacked ahorra 3 bytes pero `score` queda desalineado
- Para este struct específico, el packing selectivo de `value` no ahorra espacio porque el padding se desplaza (de antes de `value` a después de `value`) ya que `score` sigue necesitando alineación a 8
- El acceso a `score` es eficiente en Normal y SelectPacked (offset 8), pero ineficiente en FullPacked (offset 5)
</details>

### Ejercicio 9 — alignas y cache line

```c
// 1. Imprime alignof de: char, short, int, long, float, double,
//    long double, void *, max_align_t
//
// 2. Declara:
//    #define CACHE_LINE 64
//    struct __attribute__((aligned(CACHE_LINE))) PaddedCounter {
//        int value;
//    };
//
// 3. Crea un array de 4 PaddedCounters
// 4. Imprime la dirección de cada uno y verifica que:
//    a) Cada dirección es múltiplo de 64
//    b) La distancia entre consecutivos es exactamente 64 bytes
// 5. Calcula: ¿cuántos bytes de padding tiene cada counter?
//    (sizeof(PaddedCounter) - sizeof(int))
// 6. ¿Vale la pena desperdiciar 60 bytes por counter?
//    ¿En qué escenario?
```

<details><summary>Predicción</summary>

- `alignof(max_align_t)` = **16** en x86-64 glibc (por `long double`)
- `sizeof(PaddedCounter)` = **64 bytes**, `alignof` = 64
- Cada dirección será múltiplo de 64: `0x...00`, `0x...40`, `0x...80`, `0x...C0`
- Distancia entre consecutivos: exactamente 64 bytes
- Padding por counter: 64 - 4 = **60 bytes** (93.75% desperdicio)
- Vale la pena **solo** en programas multithreaded donde diferentes hilos modifican diferentes counters — elimina false sharing. En single-threaded es puro desperdicio
</details>

### Ejercicio 10 — Comparar normal, reordenado, packed

```c
// Dado:
//   struct Sensor {
//       char     type;        // 1
//       double   reading;     // 8
//       char     unit;        // 1
//       uint32_t timestamp;   // 4
//       char     status;      // 1
//       uint16_t id;          // 2
//   };
//
// 1. Calcula sizeof con el orden original (predice, luego verifica)
// 2. Crea SensorSorted con campos reordenados de mayor a menor
// 3. Crea SensorPacked con __attribute__((packed))
// 4. Para los 3 structs, imprime sizeof, alignof, y offset de cada campo
// 5. Compara para un array de 10000 sensores:
//      sizeof(Sensor) * 10000 vs sizeof(SensorSorted) * 10000
//      vs sizeof(SensorPacked) * 10000
// 6. ¿Cuál elegirías para una base de datos en memoria?
//    ¿Cuál para un protocolo de red? ¿Por qué?
```

<details><summary>Predicción</summary>

**Original** `{char, double, char, uint32_t, char, uint16_t}`:
- type(0,1) +7pad, reading(8,8), unit(16,1) +3pad, timestamp(20,4), status(24,1) +1pad, id(26,2) +4tail
- **sizeof = 32**, alignof = 8. Datos: 17. Padding: 15 (47%)

**Sorted** `{double, uint32_t, uint16_t, char, char, char}`:
- reading(0,8), timestamp(8,4), id(12,2), type(14,1), unit(15,1), status(16,1) +7tail
- **sizeof = 24**, alignof = 8. Datos: 17. Padding: 7 (29%)

**Packed**: todo contiguo, sin padding
- type(0,1), reading(1,8), unit(9,1), timestamp(10,4), status(14,1), id(15,2)
- **sizeof = 17**, alignof = 1. Datos: 17. Padding: 0

Para 10000 sensores: Original = 320 KB, Sorted = 240 KB, Packed = 170 KB

- **Base de datos en memoria**: Sorted — buen balance entre espacio y rendimiento de acceso
- **Protocolo de red**: Packed — layout exacto byte a byte, portabilidad binaria
- No usar Packed para procesamiento intensivo: los accesos desalineados a `reading` (double en offset 1) degradan rendimiento
</details>
