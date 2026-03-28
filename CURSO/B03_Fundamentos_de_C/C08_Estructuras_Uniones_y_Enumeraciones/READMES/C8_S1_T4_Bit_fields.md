# T04 — Bit fields

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` líneas 166-168 | Dice "Sin bit fields: 16 bytes (12 + 4 bytes para 3 campos int)" para `struct Particle` | Con 3 campos `int` separados (type, alive, group) serían 3×4 = 12 bytes adicionales, total **24 bytes** (12 floats + 12 ints), no 16. El ahorro real es 24→16 = 8 bytes (33%), no "pequeño" |

---

## 1. Qué son los bit fields

Los bit fields permiten declarar campos de un struct que ocupan un número específico de **bits** en lugar de bytes:

```c
#include <stdio.h>

struct Flags {
    unsigned int read    : 1;    // 1 bit — valores 0 o 1
    unsigned int write   : 1;    // 1 bit
    unsigned int execute : 1;    // 1 bit
};

int main(void) {
    struct Flags perms = { .read = 1, .write = 1, .execute = 0 };
    printf("read=%u write=%u exec=%u\n",
           perms.read, perms.write, perms.execute);
    // read=1 write=1 exec=0

    printf("sizeof = %zu\n", sizeof(struct Flags));    // 4
    return 0;
}
```

### Sintaxis

```c
tipo nombre : ancho_en_bits;
```

- **tipo**: `unsigned int`, `signed int`, `int`, `_Bool` (C99+). Otros tipos son implementation-defined
- **ancho**: de 1 a `sizeof(tipo) * 8` bits

### Unidad de almacenamiento

Los bit fields se empaquetan dentro de la **unidad de almacenamiento** del tipo base. Para `unsigned int`, esa unidad es 4 bytes (32 bits). Mientras los bits totales quepan en esa unidad, `sizeof` del struct es 4:

```c
struct PackedDate {
    unsigned int day   : 5;    // 0-31  (5 bits)
    unsigned int month : 4;    // 0-15  (4 bits)
    unsigned int year  : 7;    // 0-127 (7 bits)
};
// Total: 16 bits → cabe en un unsigned int
// sizeof(struct PackedDate) = 4

struct PackedDate date = { .day = 15, .month = 3, .year = 126 };
printf("%u/%u/%u\n", date.day, date.month, date.year + 1900);
// 15/3/2026
```

---

## 2. Signed vs unsigned: siempre usar unsigned

Con un campo `signed int : N`, el bit más significativo es el **bit de signo** (complemento a dos). Esto reduce el rango útil y produce resultados sorprendentes:

```c
struct BadSigned {
    int value : 3;    // signed — rango: -4 a 3 (NO 0 a 7)
};

struct BadSigned s = { .value = 5 };
printf("%d\n", s.value);    // -3 (!)
// 5 en binario = 101
// 101 en complemento a dos de 3 bits = -3
```

Tabla de valores para 3 bits signed vs unsigned:

| Bits | Unsigned (0 a 7) | Signed (-4 a 3) |
|------|-----------------|-----------------|
| `000` | 0 | 0 |
| `001` | 1 | 1 |
| `010` | 2 | 2 |
| `011` | 3 | 3 |
| `100` | 4 | -4 |
| `101` | 5 | -3 |
| `110` | 6 | -2 |
| `111` | 7 | -1 |

**Regla**: siempre usar `unsigned int` para bit fields. Para campos de 1 bit, `_Bool` también funciona:

```c
struct BoolFields {
    _Bool active  : 1;     // 0 o 1, semántica booleana
    _Bool visible : 1;
    _Bool locked  : 1;
};
```

---

## 3. Truncamiento por overflow

Cuando asignas un valor que no cabe en el número de bits del campo, se **trunca** — solo se conservan los N bits inferiores:

```c
struct SmallField {
    unsigned int value : 3;    // rango: 0 a 7
};

struct SmallField sf;
sf.value = 7;     // 111 → lee 7 ✓
sf.value = 8;     // 1000 → truncado a 000 → lee 0
sf.value = 15;    // 1111 → truncado a 111 → lee 7
sf.value = 255;   // 11111111 → truncado a 111 → lee 7
```

La operación es equivalente a `valor & ((1 << N) - 1)` — una máscara de los N bits inferiores.

GCC con `-Wall` detecta estos overflows en tiempo de compilación:

```
warning: unsigned conversion from 'int' to 'unsigned int:3'
         changes value from '8' to '0' [-Woverflow]
```

**Lección**: elegir el ancho suficiente para el rango necesario. Para valores 0-31, usar `:5` (2⁵ = 32). Para valores 0-255, usar `:8`.

---

## 4. Campos sin nombre y de ancho 0

### Campo sin nombre — padding explícito

Un campo sin nombre reserva bits sin crear un miembro accesible:

```c
struct Register {
    unsigned int opcode  : 4;
    unsigned int         : 4;    // 4 bits de padding (no accesible)
    unsigned int operand : 8;
};
// opcode en bits 0-3, gap de 4 bits, operand en bits 8-15
```

### Campo de ancho 0 — forzar nueva unidad

Un campo `: 0` fuerza que el siguiente campo empiece en una **nueva unidad de almacenamiento** (nuevo `unsigned int`):

```c
struct NoZero {
    unsigned int a : 3;
    unsigned int b : 5;
};
// a y b en el MISMO unsigned int → sizeof = 4

struct Aligned {
    unsigned int a : 3;
    unsigned int   : 0;    // fuerza nueva unidad
    unsigned int b : 5;
};
// a en un unsigned int, b en OTRO → sizeof = 8
```

Sin `: 0`, los campos `a` (3 bits) y `b` (5 bits) suman 8 bits y caben en un solo `unsigned int` — sizeof 4. Con `: 0`, cada campo vive en su propia unidad — sizeof 8.

Esto es útil para controlar la alineación de campos en registros de hardware.

---

## 5. Layout de bits: NO portátil

El estándar C **no especifica**:

1. El **orden** de los bits dentro del byte (MSB primero o LSB primero)
2. Si los bit fields **cruzan** límites de unidad de almacenamiento
3. La **alineación** de los bit fields
4. El **endianness** de los bits

```c
struct { unsigned int a : 4; unsigned int b : 4; };

// En GCC x86-64:   [aaaa bbbb]  (LSB a MSB)
// En otro compilador/arch: [bbbb aaaa]  (posiblemente)
```

### Consecuencia práctica

**Nunca** usar bit fields para datos que cruzan límites del programa:

```c
// MAL — no portátil (el orden de version e ihl depende del compilador):
struct __attribute__((packed)) IPHeader {
    unsigned int version : 4;
    unsigned int ihl     : 4;
};

// BIEN — portátil con masks:
uint8_t header[20];
uint8_t version = (header[0] >> 4) & 0x0F;
uint8_t ihl     = header[0] & 0x0F;
```

Las operaciones bitwise con shifts y masks producen el **mismo resultado** en cualquier compilador y arquitectura.

---

## 6. No se puede tomar la dirección (`&`)

Un puntero apunta a una dirección de **byte**. Un bit field puede empezar en una posición arbitraria **dentro** de un byte. Por tanto, no se puede obtener su dirección:

```c
struct Flags {
    unsigned int a : 1;
    unsigned int b : 1;
};

struct Flags f = {0};

// &f.a;           // ERROR: cannot take address of bit-field 'a'
// int *p = &f.a;  // ERROR
// scanf("%d", &f.a);  // ERROR — scanf necesita un puntero
```

### Implicaciones

- No se pueden pasar bit fields por **puntero** a funciones
- No se pueden usar con `scanf` directamente
- No se pueden usar con funciones genéricas que reciben `void *`

### Workaround

```c
int temp;
scanf("%d", &temp);    // leer en variable temporal
f.a = temp;            // asignar al bit field
```

---

## 7. Bit fields vs operaciones bitwise

Ambos enfoques logran lo mismo — empaquetar flags en poco espacio:

### Bit fields

```c
struct Perms {
    unsigned int read    : 1;
    unsigned int write   : 1;
    unsigned int execute : 1;
};

struct Perms p = { .read = 1, .write = 1 };
p.execute = 1;          // agregar
p.write = 0;            // quitar
if (p.read) { ... }     // verificar
```

### Operaciones bitwise

```c
#define PERM_READ  (1u << 0)
#define PERM_WRITE (1u << 1)
#define PERM_EXEC  (1u << 2)

unsigned int p = PERM_READ | PERM_WRITE;
p |= PERM_EXEC;         // agregar
p &= ~PERM_WRITE;       // quitar
if (p & PERM_READ) { ... }  // verificar
```

### Comparación

| Aspecto | Bit fields | Bitwise |
|---------|-----------|---------|
| Legibilidad | `p.read = 1` — claro | `p |= PERM_READ` — requiere conocer operadores |
| Combinar flags | No directo | `PERM_READ | PERM_EXEC` en una operación |
| Pasar como argumento | Necesita el struct completo | Un solo `unsigned int` |
| Portabilidad del layout | No garantizada | Total |
| Control del orden de bits | Ninguno | Total |
| Tomar dirección | Imposible | Normal (`&p`) |

**Bitwise es preferido** cuando se necesita portabilidad, combinar flags, o pasar permisos como argumento. **Bit fields son preferidos** para uso interno donde la legibilidad importa más y no se necesita interoperabilidad.

---

## 8. Cuándo usar (y cuándo no) bit fields

### Usar para

**1. Flags booleanas internas del programa:**

```c
struct Options {
    unsigned int verbose   : 1;
    unsigned int debug     : 1;
    unsigned int color     : 1;
    unsigned int recursive : 1;
};
// 4 flags en 4 bits dentro de 4 bytes (un unsigned int)
```

**2. Enums con rango pequeño:**

```c
struct Status {
    unsigned int state    : 3;    // 8 estados posibles
    unsigned int priority : 4;    // 16 niveles
    unsigned int error    : 1;    // flag
};
```

**3. Ahorrar memoria en arrays muy grandes:**

```c
struct Particle {
    float x, y, z;               // 12 bytes
    unsigned int type  : 4;      // 16 tipos
    unsigned int alive : 1;      // vivo/muerto
    unsigned int group : 5;      // 32 grupos
};
// Con bit fields: sizeof = 16 bytes (12 floats + 4 para bit fields)
// Sin bit fields (3 int): sizeof = 24 bytes (12 + 3×4)
// En 1 millón de partículas: 16 MB vs 24 MB
```

### NO usar para

- **Protocolos de red** o formatos de archivo binario — layout no portátil
- **Registros de hardware** — excepto con documentación específica del compilador
- **Datos compartidos** entre programas o plataformas
- Cuando necesitas la **dirección** de un campo (`&field`)

---

## 9. Bit fields en structs con otros campos

Los bit fields pueden coexistir con campos normales en el mismo struct. El compilador agrupa los bit fields en unidades de almacenamiento y aplica padding normal entre los campos regulares:

```c
struct Particle {
    float x, y, z;               // 12 bytes — campos normales
    unsigned int type  : 4;      // ┐
    unsigned int alive : 1;      // ├ 10 bits — caben en 1 unsigned int (4 bytes)
    unsigned int group : 5;      // ┘
};
// sizeof = 16: 12 (floats) + 4 (unsigned int con bit fields)
```

### Regla de agrupamiento

Los bit fields consecutivos del **mismo tipo base** se empaquetan en la misma unidad. Un campo normal entre bit fields fuerza una nueva unidad:

```c
struct Mixed {
    unsigned int a : 3;      // ┐ unidad 1 (unsigned int)
    unsigned int b : 5;      // ┘
    float f;                 // campo normal — rompe la secuencia
    unsigned int c : 4;      // nueva unidad (unsigned int)
};
// sizeof = 12: 4 (a,b) + 4 (f) + 4 (c)
```

### Bit fields con diferentes tipos base

Si se mezclan tipos (`unsigned int` de 4 bytes y `unsigned char` de 1 byte), el comportamiento es **implementation-defined**. Para portabilidad, usar siempre el mismo tipo base para todos los bit fields de un grupo.

---

## 10. Conexión con Rust

Rust **no tiene bit fields** como característica del lenguaje. Hay razones de diseño para esta decisión:

### Por qué Rust no tiene bit fields

1. El layout de bits es non-portable en C — Rust prioriza comportamiento predecible
2. Las operaciones bitwise son igualmente expresivas y totalmente portátiles
3. Los bit fields complican el borrow checker (no se puede tomar referencia a un bit)

### Alternativas en Rust

**Operaciones bitwise manuales** (lo más directo):

```rust
const PERM_READ:  u32 = 1 << 0;
const PERM_WRITE: u32 = 1 << 1;
const PERM_EXEC:  u32 = 1 << 2;

let mut perms: u32 = PERM_READ | PERM_WRITE;
perms |= PERM_EXEC;        // agregar
perms &= !PERM_WRITE;      // quitar (! en Rust = ~ en C)
if perms & PERM_READ != 0 { /* ... */ }
```

**Crate `bitflags`** — macros que generan tipos seguros para flags:

```rust
use bitflags::bitflags;

bitflags! {
    struct Permissions: u32 {
        const READ    = 1 << 0;
        const WRITE   = 1 << 1;
        const EXECUTE = 1 << 2;
    }
}

let p = Permissions::READ | Permissions::WRITE;
if p.contains(Permissions::READ) { /* ... */ }
```

**Crate `modular-bitfield`** — simula bit fields con ergonomía similar a C:

```rust
use modular_bitfield::prelude::*;

#[bitfield]
struct PackedDate {
    day:   B5,    // 5 bits
    month: B4,    // 4 bits
    year:  B7,    // 7 bits
}
```

| Concepto | C | Rust |
|----------|---|------|
| Bit fields nativos | Sí (`unsigned int x : N`) | No — usa crates o bitwise |
| Layout portátil | No garantizado | Bitwise siempre portátil |
| Seguridad de tipos | Ninguna (todo es `unsigned int`) | `bitflags` genera tipos seguros |
| Tomar referencia | Imposible (`&bf.field` error) | N/A (no existen bit fields) |
| Overflow | Truncamiento silencioso | `bitflags` previene valores inválidos |

---

## Ejercicios

### Ejercicio 1 — Fecha compacta

```c
// Crear struct CompactDate con bit fields:
//   unsigned int day   : 5;    // 1-31
//   unsigned int month : 4;    // 1-12
//   unsigned int year  : 12;   // 0-4095 (año directo)
//
// Y struct RegularDate { int day, month, year; };
//
// 1. Almacena la fecha actual (2026-03-26) en ambos
// 2. Imprime sizeof de cada uno
// 3. Calcula cuántas CompactDates caben en 1 MB vs RegularDates
// 4. Imprime los valores almacenados para verificar
```

<details><summary>Predicción</summary>

- `sizeof(CompactDate)` = **4 bytes** (5+4+12 = 21 bits, cabe en un `unsigned int`)
- `sizeof(RegularDate)` = **12 bytes** (3 × int de 4)
- En 1 MB (1048576 bytes): CompactDate = 262144 fechas, RegularDate = 87381 fechas
- CompactDate almacena **3× más** fechas en el mismo espacio
- Los valores se imprimen correctamente: day=26, month=3, year=2026
</details>

### Ejercicio 2 — Permisos de archivo

```c
// Crear struct con bit fields para permisos Unix (rwx × user/group/other):
//   struct FilePerms {
//       unsigned int owner_r : 1;
//       unsigned int owner_w : 1;
//       unsigned int owner_x : 1;
//       unsigned int group_r : 1;
//       unsigned int group_w : 1;
//       unsigned int group_x : 1;
//       unsigned int other_r : 1;
//       unsigned int other_w : 1;
//       unsigned int other_x : 1;
//   };
//
// 1. Implementa void perms_print(struct FilePerms p)
//    → imprime como "rwxr-xr--" (formato ls -l)
// 2. Crea permisos para 755 (rwxr-xr-x) y 644 (rw-r--r--)
// 3. Imprime ambos
// 4. ¿Cuántos bits usa? ¿sizeof?
```

<details><summary>Predicción</summary>

- 9 campos de 1 bit = 9 bits totales, caben en un `unsigned int`
- sizeof = **4 bytes** (un unsigned int)
- 755 = rwxr-xr-x: owner_r=1, owner_w=1, owner_x=1, group_r=1, group_w=0, group_x=1, other_r=1, other_w=0, other_x=1
- 644 = rw-r--r--: owner_r=1, owner_w=1, owner_x=0, group_r=1, group_w=0, group_x=0, other_r=1, other_w=0, other_x=0
- perms_print imprime el string ternario: campo ? letra : '-'
</details>

### Ejercicio 3 — Overflow y truncamiento

```c
// Declara:
//   struct Field4 { unsigned int val : 4; };  // rango 0-15
//   struct Field4s { int val : 4; };           // rango -8 a 7
//
// 1. Asigna al campo unsigned: 0, 7, 15, 16, 31, 255
//    Imprime cada valor leído. ¿Cuáles se truncan?
// 2. Asigna al campo signed: 0, 7, 8, 15, -1
//    Imprime cada valor leído. ¿Cuáles dan resultado inesperado?
// 3. Compila con -Wall. ¿Qué warnings aparecen?
// 4. Calcula manualmente: 16 (10000) en 4 bits = ?
//    31 (11111) en 4 bits = ?
```

<details><summary>Predicción</summary>

**Unsigned 4 bits** (rango 0-15):
- 0 → 0 ✓, 7 → 7 ✓, 15 → 15 ✓ (valor máximo)
- 16 (10000) → truncado a 0000 → **0**
- 31 (11111) → truncado a 1111 → **15**
- 255 (11111111) → truncado a 1111 → **15**

**Signed 4 bits** (rango -8 a 7):
- 0 → 0 ✓, 7 → 7 ✓ (valor máximo positivo)
- 8 (1000) → complemento a dos: **-8**
- 15 (1111) → complemento a dos: **-1**
- -1 → 1111 → complemento a dos: **-1** ✓

GCC con `-Wall` genera warnings `-Woverflow` para cada asignación fuera del rango
</details>

### Ejercicio 4 — Bitwise equivalente

```c
// Implementa los MISMOS flags de dos formas:
//
// Forma 1 (bit fields):
//   struct StatusBF {
//       unsigned int connected : 1;
//       unsigned int encrypted : 1;
//       unsigned int compressed : 1;
//       unsigned int error : 1;
//   };
//
// Forma 2 (bitwise):
//   #define ST_CONNECTED  (1u << 0)
//   #define ST_ENCRYPTED  (1u << 1)
//   #define ST_COMPRESSED (1u << 2)
//   #define ST_ERROR      (1u << 3)
//
// Para ambas, implementa:
//   set_connected, clear_connected, is_connected
//   print_status (imprime "CEXE" o "----")
//
// En main, haz las mismas operaciones con ambas y verifica
// que producen la misma salida.
```

<details><summary>Predicción</summary>

- Bit fields: `s.connected = 1`, `s.connected = 0`, `if (s.connected)`
- Bitwise: `s |= ST_CONNECTED`, `s &= ~ST_CONNECTED`, `if (s & ST_CONNECTED)`
- Ambos producen la misma salida
- sizeof es 4 bytes para ambos
- Diferencia clave: con bitwise se puede hacer `unsigned int status = ST_CONNECTED | ST_ENCRYPTED` en una operación. Con bit fields, hay que asignar campo por campo
</details>

### Ejercicio 5 — Campo de ancho 0

```c
// Declara:
//   struct Packed { unsigned int a:3; unsigned int b:5; unsigned int c:4; };
//   struct Split1 { unsigned int a:3; unsigned int :0; unsigned int b:5; unsigned int c:4; };
//   struct Split2 { unsigned int a:3; unsigned int b:5; unsigned int :0; unsigned int c:4; };
//
// 1. Imprime sizeof de los 3 structs
// 2. ¿En cuál(es) a y b comparten unidad? ¿En cuál(es) no?
// 3. Crea una instancia de cada uno con a=5, b=20, c=10
// 4. Verifica que los valores se almacenan correctamente en los 3
```

<details><summary>Predicción</summary>

- **Packed**: 3+5+4 = 12 bits, caben en un `unsigned int` → sizeof = **4**
- **Split1**: a(3 bits) en unidad 1, `:0` fuerza nueva unidad, b(5)+c(4)=9 bits en unidad 2 → sizeof = **8**
- **Split2**: a(3)+b(5)=8 bits en unidad 1, `:0` fuerza nueva unidad, c(4) en unidad 2 → sizeof = **8**
- Los 3 almacenan los mismos valores correctamente: a=5 (cabe en 3 bits: 101, rango 0-7 ✓), b=20 (cabe en 5 bits: 10100, rango 0-31 ✓), c=10 (cabe en 4 bits: 1010, rango 0-15 ✓)
- El `:0` solo afecta el layout en memoria, no los valores
</details>

### Ejercicio 6 — Pixel RGB565

```c
// El formato RGB565 usa 16 bits por pixel:
//   5 bits rojo, 6 bits verde, 5 bits azul
//
// Declara:
//   struct RGB565 { unsigned int r:5; unsigned int g:6; unsigned int b:5; };
//   struct RGB888 { uint8_t r, g, b; };
//
// 1. Imprime sizeof de cada uno
// 2. Crea un array de 320×240 pixels (pantalla QVGA) de cada tipo
// 3. Imprime el tamaño total de cada array
// 4. Llena el array RGB565 con un gradiente: r varía 0-31, g=0, b=0
// 5. Imprime los primeros 5 y últimos 5 pixels
// 6. Calcula: un frame RGB565 vs RGB888 — ¿cuánto ahorra?
```

<details><summary>Predicción</summary>

- `sizeof(RGB565)` = **4 bytes** (16 bits de bit fields en un unsigned int de 32 bits)
- `sizeof(RGB888)` = **3 bytes** (3 × uint8_t, sin padding — alignment 1)
- Array 320×240: RGB565 = 320×240×4 = **307200 bytes** (~300 KB)
- Array 320×240: RGB888 = 320×240×3 = **230400 bytes** (~225 KB)
- Sorpresa: RGB565 ocupa **más** que RGB888 a pesar de tener menos bits de color, porque la unidad de almacenamiento `unsigned int` es 4 bytes (32 bits), desperdiciando 16 de los 32 bits
- Para un RGB565 real compacto, se usa `uint16_t` con bitwise, no bit fields
</details>

### Ejercicio 7 — Registro de CPU simulado

```c
// Simula un registro de instrucción de 32 bits:
//   struct Instruction {
//       unsigned int opcode  : 6;   // 64 operaciones
//       unsigned int rs      : 5;   // registro fuente
//       unsigned int rt      : 5;   // registro destino
//       unsigned int imm     : 16;  // valor inmediato
//   };
//
// 1. Verifica que sizeof == 4 (todo cabe en 32 bits)
// 2. Crea instrucciones:
//    ADD r1, r2 → opcode=1, rs=1, rt=2, imm=0
//    LOAD r5, 1024 → opcode=2, rs=0, rt=5, imm=1024
//    JUMP 42 → opcode=3, rs=0, rt=0, imm=42
// 3. Implementa void decode(struct Instruction instr)
//    que imprime: "OP=1 rs=1 rt=2 imm=0"
// 4. Decodifica las 3 instrucciones
// 5. ¿Qué pasa si imm = 70000? (mayor que 2^16 - 1 = 65535)
```

<details><summary>Predicción</summary>

- sizeof = **4 bytes** (6+5+5+16 = 32 bits = un unsigned int exacto)
- Las 3 instrucciones se decodifican correctamente
- imm = 70000 (en binario: 10001000101110000, 17 bits) → truncado a 16 bits → `70000 & 0xFFFF` = **4464**
- GCC con `-Wall` advertiría del truncamiento
- Nota: esto es un modelo simplificado de MIPS R-type/I-type encoding
</details>

### Ejercicio 8 — Comparar tamaños: bit fields vs int vs uint8_t

```c
// Declara 3 versiones de un struct con 8 campos booleanos:
//
//   struct BF_Bools {
//       unsigned int a:1, b:1, c:1, d:1, e:1, f:1, g:1, h:1;
//   };
//   struct Int_Bools { int a, b, c, d, e, f, g, h; };
//   struct U8_Bools  { uint8_t a, b, c, d, e, f, g, h; };
//
// 1. Imprime sizeof de cada uno
// 2. Calcula para un array de 1 millón de elementos:
//    - Memoria con bit fields
//    - Memoria con int
//    - Memoria con uint8_t
// 3. ¿Cuál es más eficiente en memoria? ¿Cuál en velocidad de acceso?
```

<details><summary>Predicción</summary>

- `sizeof(BF_Bools)` = **4 bytes** (8 bits en un unsigned int)
- `sizeof(Int_Bools)` = **32 bytes** (8 × 4 bytes)
- `sizeof(U8_Bools)` = **8 bytes** (8 × 1 byte)
- Para 1 millón: BF = **4 MB**, Int = **32 MB**, U8 = **8 MB**
- Bit fields son **8× más compactos** que int, **2× más compactos** que uint8_t
- Velocidad de acceso: int es el más rápido (acceso directo, alineado). uint8_t es cercano. Bit fields son los más lentos (el compilador genera shifts y masks para cada acceso)
- Trade-off: bit fields ahorran memoria pero cada lectura/escritura requiere operaciones extra
</details>

### Ejercicio 9 — Flags de estado de conexión

```c
// Modelar el estado de una conexión de red:
//   struct ConnState {
//       unsigned int phase      : 3;  // 0=CLOSED, 1=SYN_SENT, 2=ESTABLISHED,
//                                     // 3=FIN_WAIT, 4=CLOSE_WAIT, 5=TIME_WAIT
//       unsigned int retries    : 4;  // 0-15 reintentos
//       unsigned int keepalive  : 1;  // keepalive habilitado
//       unsigned int encrypted  : 1;  // TLS activo
//       unsigned int compressed : 1;  // compresión activa
//   };
//
// 1. Implementa const char *phase_name(unsigned int phase)
//    que retorne el nombre del estado
// 2. Implementa void conn_print(struct ConnState s)
// 3. Crea 3 conexiones en diferentes estados e imprímelas
// 4. Simula: incrementa retries de una conexión hasta que llegue a 15
// 5. ¿Qué pasa si incrementas retries una vez más (16)?
// 6. Imprime sizeof
```

<details><summary>Predicción</summary>

- sizeof = **4 bytes** (3+4+1+1+1 = 10 bits en un unsigned int)
- `phase_name` usa un switch o array de strings
- Retries = 15 (máximo de 4 bits). Si incrementas a 16 → truncamiento a 0 (wrap around silencioso)
- Esto es peligroso: un contador de reintentos que vuelve a 0 podría causar un loop infinito de reconexión. Para contadores, considerar usar campos más anchos o verificar el máximo antes de incrementar
</details>

### Ejercicio 10 — Empaquetamiento máximo

```c
// Objetivo: empaquetar la máxima información en exactamente 32 bits
// (un unsigned int).
//
// Diseña un struct con bit fields que almacene:
//   - Coordenada X: -512 a 511 (necesita signed, ¿cuántos bits?)
//   - Coordenada Y: -512 a 511
//   - Tipo de entidad: 0-15 (¿cuántos bits?)
//   - Equipo: 0-3 (¿cuántos bits?)
//   - Visible: sí/no (¿cuántos bits?)
//   - Vivo: sí/no
//
// 1. Calcula cuántos bits necesita cada campo
// 2. Verifica que el total no excede 32 bits
// 3. Declara el struct y verifica sizeof == 4
// 4. Crea una entidad: posición (100, -200), tipo 5, equipo 2,
//    visible=1, vivo=1
// 5. Imprime todos los campos
// 6. ¿Qué pasa si X = 600 (fuera del rango signed de 10 bits)?
```

<details><summary>Predicción</summary>

Bits necesarios:
- X: -512 a 511 → rango de 1024 → signed 10 bits (2⁹ = 512)
- Y: -512 a 511 → signed 10 bits
- Tipo: 0-15 → unsigned 4 bits (2⁴ = 16)
- Equipo: 0-3 → unsigned 2 bits (2² = 4)
- Visible: 0-1 → unsigned 1 bit
- Vivo: 0-1 → unsigned 1 bit

Total: 10 + 10 + 4 + 2 + 1 + 1 = **28 bits** → cabe en 32 bits con 4 bits sobrantes

```c
struct Entity {
    int x          : 10;   // signed para negativos
    int y          : 10;
    unsigned int type    : 4;
    unsigned int team    : 2;
    unsigned int visible : 1;
    unsigned int alive   : 1;
};
```

- sizeof = **4 bytes** ✓
- X = 600: 600 en 10 bits signed → 600 & 0x3FF = 600 (1001011000). Bit 9 = 1 → interpretado como negativo → **-424**. GCC advierte con `-Wall`
</details>
