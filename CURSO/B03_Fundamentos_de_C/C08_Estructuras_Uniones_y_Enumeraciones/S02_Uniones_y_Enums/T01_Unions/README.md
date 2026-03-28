# T01 — Unions

## Qué es una union

Una union es como un struct, pero todos los miembros
**comparten la misma memoria**. Solo un miembro es válido
a la vez:

```c
#include <stdio.h>

union Value {
    int i;
    float f;
    char c;
};

int main(void) {
    union Value v;

    v.i = 42;
    printf("int: %d\n", v.i);       // 42

    v.f = 3.14f;
    printf("float: %f\n", v.f);     // 3.14
    printf("int: %d\n", v.i);       // basura — v.i fue sobrescrito

    // sizeof(union Value) = sizeof(miembro más grande)
    printf("sizeof = %zu\n", sizeof(union Value));  // 4 (sizeof(int) o sizeof(float))

    return 0;
}
```

```c
// Modelo mental:
//
// struct (cada miembro tiene su propia memoria):
// ┌──────┬──────┬──────┐
// │  i   │  f   │  c   │  = 12 bytes (con padding)
// └──────┴──────┴──────┘
//
// union (todos comparten la misma memoria):
// ┌──────────────┐
// │ i / f / c    │  = 4 bytes (el más grande)
// └──────────────┘
//
// Escribir en un miembro sobrescribe a los demás.
// Solo el ÚLTIMO miembro escrito tiene un valor válido.
```

## Type punning con unions

Leer un miembro diferente al último escrito es
**implementation-defined** en C (pero definido en C99+
para el propósito de type punning):

```c
#include <stdio.h>
#include <stdint.h>

// Ver la representación de un float:
union FloatBits {
    float f;
    uint32_t u;
};

int main(void) {
    union FloatBits fb;
    fb.f = 3.14f;

    printf("float %.2f as bits: 0x%08X\n", fb.f, fb.u);
    // float 3.14 as bits: 0x4048F5C3

    // Descomponer el float IEEE 754:
    unsigned int sign = (fb.u >> 31) & 1;
    unsigned int exponent = (fb.u >> 23) & 0xFF;
    unsigned int mantissa = fb.u & 0x7FFFFF;

    printf("sign=%u exponent=%u mantissa=0x%06X\n",
           sign, exponent, mantissa);

    return 0;
}
```

```c
// En C99+, el type punning con unions está permitido:
// "If the member used to read the contents of a union object
//  is not the same as the member last used to store a value
//  in the object, the appropriate part of the object
//  representation of the value is reinterpreted as an object
//  representation in the new type"
//
// En C++, el type punning con unions es UB.
// En C, es la forma idiomática de hacer reinterpretación de bits.
//
// Alternativa portátil (C y C++): memcpy
// uint32_t bits;
// memcpy(&bits, &some_float, sizeof(bits));
```

## Tagged union (discriminated union)

El patrón más importante: un struct con un enum que indica
qué miembro de la union es válido:

```c
#include <stdio.h>
#include <string.h>

enum ValueType {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
};

struct TaggedValue {
    enum ValueType type;    // el "tag" indica qué miembro es válido
    union {
        int i;
        float f;
        char s[32];
    } data;                 // la union con los datos
};

void print_value(const struct TaggedValue *v) {
    switch (v->type) {
        case VAL_INT:
            printf("int: %d\n", v->data.i);
            break;
        case VAL_FLOAT:
            printf("float: %f\n", v->data.f);
            break;
        case VAL_STRING:
            printf("string: \"%s\"\n", v->data.s);
            break;
    }
}

int main(void) {
    struct TaggedValue values[] = {
        { .type = VAL_INT,    .data.i = 42 },
        { .type = VAL_FLOAT,  .data.f = 3.14f },
        { .type = VAL_STRING, .data.s = "hello" },
    };

    for (int i = 0; i < 3; i++) {
        print_value(&values[i]);
    }

    return 0;
}
```

```c
// Con union anónima (C11) — acceso más limpio:
struct Value {
    enum ValueType type;
    union {
        int i;
        float f;
        char s[32];
    };    // union anónima — sin nombre
};

struct Value v = { .type = VAL_INT, .i = 42 };
printf("%d\n", v.i);    // acceso directo, sin .data
```

### Uso: AST (Abstract Syntax Tree)

```c
enum NodeType {
    NODE_NUMBER,
    NODE_BINOP,
    NODE_UNARY,
};

struct ASTNode {
    enum NodeType type;
    union {
        double number;                    // NODE_NUMBER
        struct {                          // NODE_BINOP
            char op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;
        struct {                          // NODE_UNARY
            char op;
            struct ASTNode *operand;
        } unary;
    };
};

// Expresión: -(3 + 4)
// Se representa como:
// UNARY('-', BINOP('+', NUMBER(3), NUMBER(4)))
```

### Uso: Variant / Any type

```c
// Un tipo que puede contener cualquier valor:
enum Type { TYPE_BOOL, TYPE_INT, TYPE_DOUBLE, TYPE_STRING, TYPE_NULL };

struct Variant {
    enum Type type;
    union {
        _Bool b;
        int64_t i;
        double d;
        char *s;        // alocado con strdup
    };
};

struct Variant var_int(int64_t val) {
    return (struct Variant){ .type = TYPE_INT, .i = val };
}

struct Variant var_string(const char *val) {
    return (struct Variant){ .type = TYPE_STRING, .s = strdup(val) };
}

void var_free(struct Variant *v) {
    if (v->type == TYPE_STRING) {
        free(v->s);
        v->s = NULL;
    }
    v->type = TYPE_NULL;
}
```

## Union para acceder a bytes

```c
#include <stdint.h>

// Acceder a los bytes individuales de un int:
union IntBytes {
    uint32_t value;
    uint8_t bytes[4];
};

union IntBytes ib;
ib.value = 0x04030201;

printf("byte[0] = 0x%02X\n", ib.bytes[0]);    // 0x01 en little-endian
printf("byte[3] = 0x%02X\n", ib.bytes[3]);    // 0x04 en little-endian

// Útil para inspeccionar representaciones binarias.
// El resultado depende del endianness de la plataforma.
```

## Union para conversión de registros

```c
// Registros de hardware con diferentes vistas:
union Register32 {
    uint32_t full;
    struct {
        uint16_t low;
        uint16_t high;
    };
    uint8_t bytes[4];
};

union Register32 reg;
reg.full = 0xDEADBEEF;

printf("full  = 0x%08X\n", reg.full);       // 0xDEADBEEF
printf("low   = 0x%04X\n", reg.low);        // 0xBEEF (little-endian)
printf("high  = 0x%04X\n", reg.high);       // 0xDEAD
printf("byte0 = 0x%02X\n", reg.bytes[0]);   // 0xEF
```

## Comparación con Rust

```c
// C tagged union — manual, sin verificación:
struct Shape {
    enum { CIRCLE, RECT } type;
    union {
        double radius;
        struct { double w, h; };
    };
};
// Si lees .radius cuando type == RECT → basura (no hay error)

// Rust enum — con verificación del compilador:
// enum Shape {
//     Circle { radius: f64 },
//     Rect { w: f64, h: f64 },
// }
// match shape {
//     Shape::Circle { radius } => ...,
//     Shape::Rect { w, h } => ...,
// }
// El match DEBE cubrir todas las variantes.
// Acceder al campo incorrecto es un error de compilación.
```

---

## Ejercicios

### Ejercicio 1 — Tagged union

```c
// Crear un tagged union que pueda representar:
// - int, double, string (char[64]), bool
// Implementar:
// - void value_print(const struct Value *v)
// - struct Value value_from_int(int n)
// - struct Value value_from_string(const char *s)
// Crear un array de 5 Values de tipos mixtos e imprimirlos.
```

### Ejercicio 2 — Type punning

```c
// Usar una union FloatBits { float f; uint32_t u; }
// para implementar el "fast inverse square root" de Quake III:
// float Q_rsqrt(float number) {
//     // usa type punning para aproximar 1/sqrt(x)
// }
// Comparar con 1.0f / sqrtf(x).
```

### Ejercicio 3 — Mini JSON

```c
// Implementar un tipo JSONValue como tagged union:
// - JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING
// Implementar:
// - json_print(const JSONValue *v) — imprime en formato JSON
// - json_free(JSONValue *v) — libera strings si hay
// Crear un "documento" con un array de JSONValues y imprimirlo.
```
