# T01 — Unions

> Sin erratas detectadas en el material original.

---

## 1. Qué es una union — memoria compartida

Una union es similar a un struct, pero todos los miembros **comparten la misma dirección de memoria**. Solo un miembro es válido a la vez:

```c
#include <stdio.h>

union Value {
    int    i;      // 4 bytes ┐
    float  f;      // 4 bytes ├ todos empiezan en offset 0
    char   c;      // 1 byte  ┘
};

int main(void) {
    union Value v;
    v.i = 42;
    printf("int: %d\n", v.i);       // 42

    v.f = 3.14f;
    printf("float: %f\n", v.f);     // 3.14
    printf("int: %d\n", v.i);       // 1078523331 (basura — sobrescrito)

    return 0;
}
```

### Modelo mental

```
struct (cada miembro tiene su propio espacio):
┌──────┬──────┬──────┐
│  i   │  f   │  c   │   = 12+ bytes (con padding)
└──────┴──────┴──────┘

union (todos comparten el mismo espacio):
┌──────────────┐
│  i / f / c   │   = 4 bytes (el miembro más grande)
└──────────────┘
```

Escribir en un miembro **sobrescribe** los bytes de todos los demás. Solo el **último miembro escrito** tiene un valor definido.

---

## 2. sizeof de una union

El sizeof de una union es el sizeof de su **miembro más grande**, más posible padding de alineación:

```c
union Small {
    int    i;      // 4 bytes
    float  f;      // 4 bytes
    char   c;      // 1 byte
};
// sizeof = 4 (el mayor es int/float, ambos 4)

union Big {
    int    i;      // 4 bytes
    float  f;      // 4 bytes
    char   c;      // 1 byte
    double d;      // 8 bytes ← el mayor
};
// sizeof = 8 (double domina)
```

La alineación de la union es la del miembro con mayor requisito de alineación:

```c
union Mix {
    char   c;          // align 1
    double d;          // align 8 ← domina
};
// sizeof = 8, alignof = 8
```

Todos los miembros tienen **offset 0** — empiezan en la misma dirección:

```c
#include <stddef.h>
printf("offset i = %zu\n", offsetof(union Big, i));    // 0
printf("offset f = %zu\n", offsetof(union Big, f));    // 0
printf("offset c = %zu\n", offsetof(union Big, c));    // 0
printf("offset d = %zu\n", offsetof(union Big, d));    // 0
```

---

## 3. Escribir sobrescribe: solo un miembro válido

Cuando escribes en un miembro de una union, los bytes de la unión se modifican. Leer otro miembro después produce **los mismos bytes reinterpretados como otro tipo**:

```c
union Value v;
v.i = 42;              // escribe bytes de un int
printf("%d\n", v.i);   // 42 — correcto

v.f = 3.14f;           // sobrescribe con bytes de un float
printf("%f\n", v.f);   // 3.14 — correcto (último escrito)
printf("%d\n", v.i);   // 1078523331 — los bytes de 3.14f leídos como int
```

El valor 1078523331 no es "basura aleatoria" — es la representación IEEE 754 de 3.14f (`0x4048F5C3`) interpretada como entero. Esto lleva al concepto de **type punning**.

### Regla fundamental

En una union, solo el **último miembro escrito** tiene un valor semánticamente válido. Leer cualquier otro miembro es **reinterpretar bytes**, no obtener un valor significativo.

---

## 4. Tagged union (discriminated union)

El patrón más importante con unions: un struct que combina un **enum tag** con una **union de datos**. El tag indica qué miembro de la union es válido:

```c
enum ValueType { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_BOOL };

struct TaggedValue {
    enum ValueType type;    // el "tag" — indica qué miembro es válido
    union {
        int    i;
        float  f;
        char   s[32];
        _Bool  b;
    } data;                 // la union con los datos
};
```

### Despacho seguro con switch

```c
void print_value(const struct TaggedValue *v) {
    switch (v->type) {
        case VAL_INT:    printf("int: %d\n", v->data.i);               break;
        case VAL_FLOAT:  printf("float: %.2f\n", v->data.f);           break;
        case VAL_STRING: printf("string: \"%s\"\n", v->data.s);        break;
        case VAL_BOOL:   printf("bool: %s\n", v->data.b ? "true" : "false"); break;
    }
}
```

### Funciones constructoras

Garantizan coherencia entre tag y dato:

```c
struct TaggedValue make_int(int n) {
    return (struct TaggedValue){ .type = VAL_INT, .data.i = n };
}

struct TaggedValue make_string(const char *s) {
    struct TaggedValue v = { .type = VAL_STRING };
    strncpy(v.data.s, s, sizeof(v.data.s) - 1);
    v.data.s[sizeof(v.data.s) - 1] = '\0';
    return v;
}
```

### sizeof

```c
// sizeof(enum ValueType) = 4
// sizeof(union data)     = 32 (char s[32] domina)
// sizeof(struct TaggedValue) = 36 (4 + 32, sin padding extra)
```

---

## 5. Union anónima (C11) para acceso limpio

Con una **union anónima** (sin nombre después del `}`), los miembros se acceden directamente, sin el nombre intermedio `.data`:

```c
// Con union nombrada:
struct Value {
    enum ValueType type;
    union {
        int i;
        float f;
        char s[32];
    } data;    // nombre "data"
};
struct Value v = { .type = VAL_INT, .data.i = 42 };
printf("%d\n", v.data.i);    // acceso con .data.i

// Con union anónima (C11):
struct Value {
    enum ValueType type;
    union {
        int i;
        float f;
        char s[32];
    };    // sin nombre — union anónima
};
struct Value v = { .type = VAL_INT, .i = 42 };
printf("%d\n", v.i);    // acceso directo con .i
```

La union anónima produce código más limpio. Los miembros de la union se acceden como si fueran miembros directos del struct.

---

## 6. Type punning con unions

**Type punning** = reinterpretar los bytes de un tipo como otro tipo. En C99+, usar una union para esto está **definido**:

```c
union FloatBits {
    float    f;
    uint32_t u;
};

union FloatBits fb;
fb.f = 3.14f;

// Leer los 4 bytes del float como entero:
printf("3.14f as bits: 0x%08X\n", fb.u);    // 0x4048F5C3
```

### Descomponer un float IEEE 754

```c
unsigned int sign     = (fb.u >> 31) & 1;        // bit 31
unsigned int exponent = (fb.u >> 23) & 0xFF;      // bits 30-23
unsigned int mantissa = fb.u & 0x7FFFFF;           // bits 22-0

printf("sign=%u exponent=%u (real=%d) mantissa=0x%06X\n",
       sign, exponent, (int)exponent - 127, mantissa);
// sign=0 exponent=128 (real=1) mantissa=0x48F5C3
```

### Negativo vs positivo

```c
union FloatBits pos, neg;
pos.f = 3.14f;
neg.f = -3.14f;
printf("XOR = 0x%08X\n", pos.u ^ neg.u);    // 0x80000000
// Solo difiere el bit 31 (bit de signo)
```

### Legalidad

- **C99+**: type punning con unions está definido — es la forma idiomática
- **C++**: type punning con unions es **UB** — usar `memcpy` en su lugar
- **Alternativa portátil** (válida en C y C++):
  ```c
  uint32_t bits;
  memcpy(&bits, &some_float, sizeof(bits));
  ```

---

## 7. Union para acceso a bytes y detección de endianness

### Acceder a bytes individuales

```c
union IntBytes {
    uint32_t value;
    uint8_t  bytes[4];
};

union IntBytes ib;
ib.value = 0x04030201;

printf("byte[0] = 0x%02X\n", ib.bytes[0]);    // 0x01 en little-endian
printf("byte[3] = 0x%02X\n", ib.bytes[3]);    // 0x04 en little-endian
```

### Detección de endianness

```c
union IntBytes ib;
ib.value = 0x04030201;

if (ib.bytes[0] == 0x01) {
    printf("little-endian\n");    // x86-64 → este resultado
} else {
    printf("big-endian\n");       // ARM big-endian, PowerPC
}
```

**Little-endian** (x86-64): el byte menos significativo (`0x01`) se almacena en la dirección más baja (`bytes[0]`).

**Big-endian**: el byte más significativo (`0x04`) se almacena en la dirección más baja.

---

## 8. Union para múltiples vistas de registros

Una union permite acceder a los mismos bytes con diferentes "vistas" — útil para emular registros de hardware o manipulación de datos binarios:

```c
union Register32 {
    uint32_t full;            // vista completa: 32 bits
    struct {
        uint16_t low;         // mitad inferior: bits 0-15
        uint16_t high;        // mitad superior: bits 16-31
    };
    uint8_t bytes[4];         // vista byte a byte
};

union Register32 reg;
reg.full = 0xDEADBEEF;

printf("full  = 0x%08X\n", reg.full);      // 0xDEADBEEF
printf("low   = 0x%04X\n", reg.low);       // 0xBEEF (little-endian)
printf("high  = 0x%04X\n", reg.high);      // 0xDEAD
printf("byte0 = 0x%02X\n", reg.bytes[0]);  // 0xEF
```

**Nota**: el resultado de `low` y `high` depende del endianness. En little-endian (x86-64), `low` contiene los bytes en las direcciones más bajas, que corresponden a los bits menos significativos.

### Caso de uso: emular registros x86

En la arquitectura x86, el registro EAX (32 bits) se puede acceder como AX (16 bits inferiores), AH (byte alto de AX), y AL (byte bajo de AX):

```c
union EAX {
    uint32_t eax;
    struct {
        union {
            uint16_t ax;
            struct {
                uint8_t al;    // byte bajo
                uint8_t ah;    // byte alto
            };
        };
        uint16_t _high;
    };
};
```

---

## 9. Cuándo usar union vs struct

| Criterio | struct | union |
|----------|--------|-------|
| Campos simultáneos | Todos válidos a la vez | Solo uno válido a la vez |
| Memoria | Suma de todos los campos + padding | Solo el campo más grande |
| Uso típico | Agrupar datos relacionados | Representar alternativas |
| Ejemplo | `{nombre, edad, email}` | `{int OR float OR string}` |

### Ejemplo: sistema de eventos

```c
// Struct — desperdicia memoria (todos los campos siempre alocados):
struct EventStruct {
    int type;              // 4
    int x, y;              // 8  (solo para click)
    char key;              // 1  (solo para keypress)
    int width, height;     // 8  (solo para resize)
};
// sizeof = 24 bytes — la mayoría desperdiciada

// Union — eficiente (solo el campo relevante ocupa espacio):
struct EventUnion {
    int type;
    union {
        struct { int x, y; }          click;
        struct { char key; }          keypress;
        struct { int width, height; } resize;
    } data;
};
// sizeof = 12 bytes (4 tag + 8 union)
```

Con 1000 eventos: 24000 bytes (struct) vs 12000 bytes (union) — **50% ahorro**.

### Regla práctica

- **Struct**: necesitas todos los campos simultáneamente
- **Union + tag**: solo un campo es válido a la vez, quieres ahorrar memoria y/o representar un tipo que puede ser "una cosa u otra"

---

## 10. Conexión con Rust: enums con datos

### C: tagged union — responsabilidad del programador

```c
struct Shape {
    enum { CIRCLE, RECT } type;
    union {
        double radius;
        struct { double w, h; };
    };
};

// Nada impide leer .radius cuando type == RECT → basura silenciosa
double area = shape.radius * shape.radius * 3.14159;  // ¿es un circle?
```

### Rust: enum con datos — verificación del compilador

```rust
enum Shape {
    Circle { radius: f64 },
    Rect { w: f64, h: f64 },
}

fn area(s: &Shape) -> f64 {
    match s {
        Shape::Circle { radius } => std::f64::consts::PI * radius * radius,
        Shape::Rect { w, h }     => w * h,
    }
    // El match DEBE cubrir TODAS las variantes.
    // Acceder a radius cuando es Rect es imposible — error de compilación.
}
```

### Comparación

| Aspecto | C tagged union | Rust enum |
|---------|---------------|-----------|
| Tag manual | Sí (enum + switch) | No (el compilador lo gestiona) |
| Verificación exhaustiva | No (switch puede olvidar casos) | Sí (`match` debe cubrir todo) |
| Acceso al campo incorrecto | Compila, produce basura | Error de compilación |
| Overhead de memoria | Tag + union | Igual (tag + union internamente) |
| Destructores | Manual (`var_free`) | Automático (`Drop`) |

### Casos de uso reales de tagged unions

- **AST (Abstract Syntax Tree)**: cada nodo es NUMBER, BINOP, o UNARY
- **Variant / Any type**: un valor que puede ser int, float, string, bool, null
- **Mensajes de protocolo**: REQUEST, RESPONSE, ERROR con datos diferentes
- **Resultados**: SUCCESS con datos o ERROR con código y mensaje

```c
// AST node como tagged union:
struct ASTNode {
    enum { NODE_NUMBER, NODE_BINOP } type;
    union {
        double number;
        struct {
            char op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;
    };
};
```

En Rust, esto sería:

```rust
enum ASTNode {
    Number(f64),
    BinOp {
        op: char,
        left: Box<ASTNode>,
        right: Box<ASTNode>,
    },
}
```

---

## Ejercicios

### Ejercicio 1 — Union básica: sizeof y sobrescritura

```c
// Declara:
//   union Data {
//       int    i;
//       double d;
//       char   s[20];
//   };
//
// 1. Imprime sizeof(union Data) y sizeof de cada miembro
// 2. Imprime offsetof de cada miembro (¿todos son 0?)
// 3. Escribe v.i = 12345, imprime v.i
// 4. Escribe v.d = 2.718, imprime v.d y luego v.i
//    ¿Qué valor tiene v.i ahora?
// 5. Escribe strcpy(v.s, "hello"), imprime v.s y luego v.i
//    ¿Qué valor tiene v.i ahora? (pista: "hell" = 0x6C6C6568 en LE)
```

<details><summary>Predicción</summary>

- `sizeof(union Data)` = **24 bytes** (char s[20] = 20 bytes, pero con alineación 8 por el double → redondeado a 24)
- Todos los offsets son **0**
- Después de `v.d = 2.718`: `v.i` contiene los 4 bytes inferiores de la representación IEEE 754 de 2.718 como double — un número aparentemente aleatorio
- Después de `strcpy(v.s, "hello")`: `v.i` contiene los primeros 4 bytes de "hello" → `'h'(0x68) 'e'(0x65) 'l'(0x6C) 'l'(0x6C)` → en little-endian: `0x6C6C6568` = **1819043176**
</details>

### Ejercicio 2 — Tagged union con print y constructores

```c
// Declara:
//   enum Type { T_INT, T_DOUBLE, T_STRING, T_BOOL };
//   struct Value {
//       enum Type type;
//       union { int i; double d; char s[64]; _Bool b; } data;
//   };
//
// Implementa:
//   struct Value val_int(int n);
//   struct Value val_double(double d);
//   struct Value val_string(const char *s);
//   struct Value val_bool(_Bool b);
//   void val_print(const struct Value *v);
//
// En main:
// 1. Crea un array de 6 Values de tipos mixtos
// 2. Imprime todos con val_print
// 3. Imprime sizeof(struct Value)
```

<details><summary>Predicción</summary>

- sizeof(union data): char s[64] domina → **64 bytes**
- sizeof(enum Type) = **4 bytes**
- sizeof(struct Value) = 4 + 64 = **68 bytes**. Con padding para alineación del double (8): el enum está en offset 0 (4 bytes), la union empieza en offset 8 (para alinear double) → sizeof = **72 bytes**

Wait, let me reconsider. The union contains int(align 4), double(align 8), char[64](align 1), _Bool(align 1). So union alignment = 8 (double). Enum at offset 0 (4 bytes), union needs alignment 8 → offset 8. Union is 64 bytes. Total = 8 + 64 = **72 bytes**.

- val_string debe usar strncpy para proteger contra overflow del buffer
- val_print usa switch sobre v->type
</details>

### Ejercicio 3 — Type punning: inspeccionar float

```c
// Implementa:
//   void float_inspect(float f);
//       → Usa union { float f; uint32_t u; }
//       → Imprime: valor, hex, signo, exponente (con y sin bias),
//         mantisa, fórmula: (-1)^s × 1.mantisa × 2^(exp-127)
//
// Prueba con:
//   float_inspect(1.0f);     // signo 0, exp 127, mantisa 0
//   float_inspect(-1.0f);    // signo 1, exp 127, mantisa 0
//   float_inspect(0.5f);     // signo 0, exp 126, mantisa 0
//   float_inspect(3.14f);    // signo 0, exp 128, mantisa 0x48F5C3
//   float_inspect(0.0f);     // caso especial: todo ceros
//   float_inspect(-0.0f);    // caso especial: solo signo = 1
```

<details><summary>Predicción</summary>

- `1.0f`: hex = `0x3F800000`, sign=0, exp=127 (real 0), mantissa=0 → (-1)⁰ × 1.0 × 2⁰ = 1.0
- `-1.0f`: hex = `0xBF800000`, sign=1, exp=127 (real 0), mantissa=0
- `0.5f`: hex = `0x3F000000`, sign=0, exp=126 (real -1), mantissa=0 → 1.0 × 2⁻¹ = 0.5
- `3.14f`: hex = `0x4048F5C3`, sign=0, exp=128 (real 1), mantissa=0x48F5C3
- `0.0f`: hex = `0x00000000`, todo ceros (caso especial, exp=0, mantissa=0 = zero)
- `-0.0f`: hex = `0x80000000`, solo bit de signo (zero negativo, `0.0f == -0.0f` es true)
</details>

### Ejercicio 4 — Detección de endianness

```c
// 1. Implementa int is_little_endian(void) usando una union:
//    union { uint32_t value; uint8_t bytes[4]; }
//    Asigna value = 1, verifica si bytes[0] == 1
//
// 2. Implementa void print_bytes(uint32_t value)
//    que imprime los 4 bytes en orden de dirección usando la union
//
// 3. Prueba con: 0x04030201, 0xDEADBEEF, 0x00000001
// 4. ¿En qué orden se imprimen los bytes en tu máquina?
// 5. ¿Qué resultado daría en big-endian?
```

<details><summary>Predicción</summary>

- `is_little_endian()` retorna **1** en x86-64 (little-endian)
- `0x04030201`: bytes = `01 02 03 04` en little-endian (LSB primero)
- `0xDEADBEEF`: bytes = `EF BE AD DE` en little-endian
- `0x00000001`: bytes = `01 00 00 00` en little-endian
- En big-endian: `0x04030201` → `04 03 02 01` (MSB primero)
- Nota: la palabra "DEADBEEF" se lee al revés en little-endian como bytes: `EF BE AD DE`
</details>

### Ejercicio 5 — Sistema de eventos con tagged union

```c
// Modela eventos de un GUI:
//   enum EventType { EV_CLICK, EV_KEY, EV_RESIZE, EV_SCROLL };
//   struct Event {
//       enum EventType type;
//       union {
//           struct { int x, y, button; } click;
//           struct { char key; int modifiers; } key;
//           struct { int width, height; } resize;
//           struct { int dx, dy; } scroll;
//       };  // union anónima
//   };
//
// 1. Implementa void event_print(const struct Event *e)
// 2. Implementa constructores: ev_click, ev_key, ev_resize, ev_scroll
// 3. Crea un array de 5 eventos variados
// 4. Imprime todos
// 5. Imprime sizeof(struct Event) — ¿qué variante domina el tamaño?
```

<details><summary>Predicción</summary>

- Tamaño de cada variante de la union: click{int,int,int}=12, key{char,int}=8(con pad), resize{int,int}=8, scroll{int,int}=8
- La variante click domina con **12 bytes**
- sizeof(struct Event) = 4 (enum) + padding + 12 (union) = **16 bytes** (alineación 4, 4+12=16)
- Usando union anónima (C11): acceso directo `e->click.x` sin `.data`
</details>

### Ejercicio 6 — Variant type con strings dinámicos

```c
// Extiende el tagged union con strings dinámicos:
//   struct Variant {
//       enum { V_NULL, V_INT, V_DOUBLE, V_STRING } type;
//       union { int64_t i; double d; char *s; };
//   };
//
// Implementa:
//   struct Variant var_null(void);
//   struct Variant var_int(int64_t n);
//   struct Variant var_string(const char *s);  // usa strdup
//   void var_print(const struct Variant *v);
//   void var_free(struct Variant *v);  // libera string si hay
//
// En main:
// 1. Crea 4 variants: null, int(42), double(3.14), string("hello")
// 2. Imprime todos
// 3. Libera todos con var_free
// 4. Verifica con valgrind
```

<details><summary>Predicción</summary>

- `var_string` usa `strdup` → necesita `free` en `var_free`
- `var_free` solo libera si `v->type == V_STRING`; luego pone `v->type = V_NULL`
- sizeof(union): int64_t(8), double(8), char*(8) → todos 8 bytes → union = **8 bytes**
- sizeof(Variant) = 4(enum) + 4(pad) + 8(union) = **16 bytes**
- Valgrind sin leaks si var_free se llama para todos los variants
- Peligro: si copias un Variant string con `=`, tienes dos punteros al mismo string → double free si liberas ambos
</details>

### Ejercicio 7 — AST simple con tagged union

```c
// Modela expresiones aritméticas como un AST:
//   enum NodeType { NODE_NUM, NODE_ADD, NODE_MUL };
//   struct Node {
//       enum NodeType type;
//       union {
//           double number;
//           struct { struct Node *left, *right; } binop;
//       };
//   };
//
// Implementa:
//   struct Node *node_num(double value);
//   struct Node *node_add(struct Node *l, struct Node *r);
//   struct Node *node_mul(struct Node *l, struct Node *r);
//   double node_eval(const struct Node *n);  // evalúa recursivamente
//   void node_free(struct Node *n);          // libera recursivamente
//
// En main, construye y evalúa: (2 + 3) * 4
// Verifica con valgrind.
```

<details><summary>Predicción</summary>

- `(2 + 3) * 4` = `node_mul(node_add(node_num(2), node_num(3)), node_num(4))`
- `node_eval` para NUM retorna number; para ADD retorna eval(left)+eval(right); para MUL retorna eval(left)*eval(right)
- Resultado: (2+3)×4 = **20.0**
- `node_free` para binop: libera left, luego right, luego free(self) (post-order)
- `node_free` para NUM: solo free(self)
- Valgrind: 5 mallocs (3 NUM + 1 ADD + 1 MUL), 5 frees
- sizeof(Node): enum(4) + pad(4) + union(max(double=8, 2 punteros=16) = 16) = **24 bytes**
</details>

### Ejercicio 8 — Union anónima para Vector3

```c
// Declara un Vector3 que se pueda acceder por nombre Y por índice:
//   struct Vector3 {
//       union {
//           struct { float x, y, z; };
//           float data[3];
//       };
//   };
//
// 1. Crea v = {1.0f, 2.0f, 3.0f} con designated initializers
// 2. Imprime v.x, v.y, v.z
// 3. Imprime v.data[0], v.data[1], v.data[2]
// 4. Verifica que v.x == v.data[0] (misma memoria)
// 5. Modifica v.data[1] = 99.0f, imprime v.y
// 6. Implementa float dot(Vector3 a, Vector3 b) usando el array
// 7. Calcula dot({1,2,3}, {4,5,6}) → 1×4 + 2×5 + 3×6
```

<details><summary>Predicción</summary>

- `v.x == v.data[0]` → **true** (misma dirección de memoria)
- Después de `v.data[1] = 99.0f`: `v.y` = **99.0f** (comparten memoria)
- `dot({1,2,3}, {4,5,6})` = 4 + 10 + 18 = **32.0f**
- sizeof(Vector3) = **12 bytes** (3 floats × 4 = 12)
- La union anónima con struct anónimo permite acceso dual: por nombre para claridad (`v.x`) y por índice para loops (`v.data[i]`)
</details>

### Ejercicio 9 — Comparar memoria: union vs struct en array grande

```c
// Modelar registros de una base de datos donde cada campo puede ser
// NULL, int, double, o string:
//
// Versión struct (todos los tipos siempre alocados):
//   struct FieldStruct { int is_null; int i; double d; char s[64]; };
//
// Versión union:
//   struct FieldUnion {
//       enum { F_NULL, F_INT, F_DOUBLE, F_STRING } type;
//       union { int i; double d; char s[64]; };
//   };
//
// 1. Imprime sizeof de ambos
// 2. Calcula para una tabla de 100 columnas × 10000 filas
// 3. ¿Cuánta memoria ahorra la versión union?
// 4. Crea un array de 5 FieldUnion de tipos mixtos e imprímelos
```

<details><summary>Predicción</summary>

- sizeof(FieldStruct): is_null(4) + pad(4) + i(4) + pad(4) + d(8) + s[64] = wait let me recalculate.
  - is_null(int, 4) at 0
  - i(int, 4) at 4
  - d(double, 8) at 8 (needs align 8, 8 is multiple of 8 ✓)
  - s[64] at 16
  - Total: 16 + 64 = 80. Align of struct = 8. 80 is multiple of 8 ✓. sizeof = **80 bytes**
- sizeof(FieldUnion): enum(4) + pad(4) + union(64, dominated by char s[64], but union align = 8 because of double) = 4 + 4 + 64 = **72 bytes**

Hmm actually: FieldUnion: enum type at offset 0 (4 bytes). Union alignment = 8 (double). So union starts at offset 8. Union size = 64 (s[64] dominates, but needs to be multiple of 8: 64 is multiple of 8 ✓). Total = 8 + 64 = **72 bytes**.

Wait, actually the union contains int(4, align 4), double(8, align 8), char[64](64, align 1). Union alignment = 8. Union size = max(4, 8, 64) = 64, rounded to multiple of 8 = 64. So union = 64 bytes.

sizeof(FieldUnion) = 8 (enum + pad) + 64 (union) = **72 bytes**.

Hmm, that's not much savings. Let me recalculate FieldStruct:
- is_null: int(4) at 0
- i: int(4) at 4
- d: double(8) at 8
- s: char[64] at 16
- Total = 80, align = 8, 80 is multiple of 8 ✓.

So 80 vs 72 = only 8 bytes savings (10%). Not great for this example because the string dominates both.

For 100 cols × 10000 rows = 1M fields:
- Struct: 80 MB
- Union: 72 MB
- Savings: 8 MB (10%)

The savings would be much larger with smaller union members (no 64-byte string).

- sizeof(FieldStruct) = **80 bytes**
- sizeof(FieldUnion) = **72 bytes**
- Ahorro: 8 bytes por campo (10%). Para 100×10000 = 1M campos: 80 MB vs 72 MB = 8 MB
- El ahorro es pequeño porque `char s[64]` domina ambos. Si el string fuera `char *s` (puntero de 8 bytes), FieldUnion sería 16 bytes y FieldStruct sería 32 bytes — 50% ahorro
</details>

### Ejercicio 10 — Mini calculadora con AST

```c
// Combina todo: construye un evaluador de expresiones con AST.
//
// Soporta: números, +, -, *, /, negación unaria
//   enum NodeType { N_NUM, N_ADD, N_SUB, N_MUL, N_DIV, N_NEG };
//   struct Expr {
//       enum NodeType type;
//       union {
//           double number;
//           struct { struct Expr *left, *right; } binop;
//           struct { struct Expr *operand; } unary;
//       };
//   };
//
// Implementa:
//   struct Expr *expr_num(double v);
//   struct Expr *expr_binop(enum NodeType op, struct Expr *l, struct Expr *r);
//   struct Expr *expr_neg(struct Expr *operand);
//   double expr_eval(const struct Expr *e);
//   void expr_print(const struct Expr *e);  // imprime con paréntesis
//   void expr_free(struct Expr *e);
//
// Evalúa: -(3 + 4) * (10 / 2) - 1
// Esperado: -(7) * 5 - 1 = -35 - 1 = -36
```

<details><summary>Predicción</summary>

Construcción del AST:
```
expr_sub(
    expr_mul(
        expr_neg(expr_binop(N_ADD, expr_num(3), expr_num(4))),
        expr_binop(N_DIV, expr_num(10), expr_num(2))
    ),
    expr_num(1)
)
```

- eval: `-(3+4) * (10/2) - 1` = `-7 * 5 - 1` = `-35 - 1` = **-36.0**
- expr_print debería producir algo como: `((-(3 + 4)) * (10 / 2)) - 1`
- Total de nodos: 9 (5 NUM + 3 BINOP + 1 NEG) → 9 mallocs, 9 frees
- expr_free para binop: free left, free right, free self. Para unary: free operand, free self. Para num: free self
- Valgrind: 9 allocs, 9 frees, 0 leaks
</details>
