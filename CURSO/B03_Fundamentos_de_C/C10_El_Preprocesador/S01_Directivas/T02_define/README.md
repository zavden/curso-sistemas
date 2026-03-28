# T02 — #define

## Macros sin parámetros (object-like macros)

```c
// #define NOMBRE valor
// El preprocesador reemplaza NOMBRE por valor en todo el código.

#define PI 3.14159265358979
#define MAX_SIZE 1024
#define VERSION "1.0.0"
#define DEBUG 1

double area = PI * r * r;           // → 3.14159... * r * r
char buffer[MAX_SIZE];               // → char buffer[1024]
printf("Version: %s\n", VERSION);    // → printf("Version: %s\n", "1.0.0")
```

```c
// La sustitución es de TEXTO — no conoce tipos:
#define FOO 1 + 2
int x = FOO * 3;    // → int x = 1 + 2 * 3 = 7 (no 9)
// Solución: siempre envolver en paréntesis:
#define FOO (1 + 2)
int x = FOO * 3;    // → int x = (1 + 2) * 3 = 9

// Macro vacío:
#define UNUSED
// Se puede usar para marcar parámetros no usados,
// o simplemente para que #ifdef UNUSED sea true.

// Macro sin valor para flags de compilación:
// gcc -DDEBUG prog.c
// Equivale a poner #define DEBUG 1 al inicio del archivo.
```

## Macros con parámetros (function-like macros)

```c
// #define NOMBRE(params) expresión
// NO hay espacio entre NOMBRE y el paréntesis:
#define SQUARE(x) ((x) * (x))           // OK
#define SQUARE (x) ((x) * (x))          // ERROR — es un object-like macro

int a = SQUARE(5);       // → ((5) * (5)) = 25
int b = SQUARE(2 + 3);  // → ((2 + 3) * (2 + 3)) = 25
```

```c
// Paréntesis en TODOS lados — regla de oro:
// 1. Cada parámetro entre paréntesis
// 2. La expresión completa entre paréntesis

// MAL:
#define SQUARE(x) x * x
int a = SQUARE(2 + 3);  // → 2 + 3 * 2 + 3 = 11 (no 25)

// MAL:
#define SQUARE(x) (x) * (x)
int b = 100 / SQUARE(5);  // → 100 / (5) * (5) = 100 (no 4)

// BIEN:
#define SQUARE(x) ((x) * (x))
int c = 100 / SQUARE(5);  // → 100 / ((5) * (5)) = 4
```

```c
// Macros comunes:
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x)    ((x) >= 0 ? (x) : -(x))
#define CLAMP(x, lo, hi) (MIN(MAX(x, lo), hi))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define UNUSED(x) ((void)(x))
```

## Operador # (stringification)

```c
// # convierte un argumento de macro a string literal:
#define STRINGIFY(x) #x

printf("%s\n", STRINGIFY(hello));      // → "hello"
printf("%s\n", STRINGIFY(1 + 2));      // → "1 + 2"
printf("%s\n", STRINGIFY(foo bar));    // → "foo bar"
```

```c
// Uso principal: macros de depuración/assert:
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s\n  at %s:%d\n", \
                #cond, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)

ASSERT(x > 0);
// Si x <= 0, imprime:
// Assertion failed: x > 0
//   at main.c:42

// #cond se expande a "x > 0" — el texto literal de la condición.
```

```c
// Para stringify un macro expandido, se necesita doble nivel:
#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)

#define VERSION 42
printf("%s\n", STRINGIFY(VERSION));     // → "VERSION" (no expandido)
printf("%s\n", XSTRINGIFY(VERSION));    // → "42" (expandido primero)

// STRINGIFY(VERSION) → #VERSION → "VERSION"
// XSTRINGIFY(VERSION) → STRINGIFY(42) → #42 → "42"
```

## Operador ## (token pasting)

```c
// ## concatena dos tokens en uno:
#define CONCAT(a, b) a##b

int CONCAT(my, Var) = 42;     // → int myVar = 42;
CONCAT(print, f)("hello\n");  // → printf("hello\n");
```

```c
// Uso: generar nombres de funciones o variables:
#define DECLARE_VECTOR(type) \
    struct vector_##type { \
        type *data; \
        size_t len; \
        size_t cap; \
    }; \
    void vector_##type##_push(struct vector_##type *v, type val); \
    void vector_##type##_free(struct vector_##type *v);

DECLARE_VECTOR(int)
// Genera:
// struct vector_int { int *data; size_t len; size_t cap; };
// void vector_int_push(struct vector_int *v, int val);
// void vector_int_free(struct vector_int *v);

DECLARE_VECTOR(double)
// Genera versión para double.
```

```c
// Uso: enum + string table automática:
#define COLORS(X) \
    X(RED)   \
    X(GREEN) \
    X(BLUE)

#define AS_ENUM(name) name,
#define AS_STRING(name) #name,

enum Color { COLORS(AS_ENUM) };
static const char *color_names[] = { COLORS(AS_STRING) };
// Genera:
// enum Color { RED, GREEN, BLUE, };
// static const char *color_names[] = { "RED", "GREEN", "BLUE", };
```

## Macros multi-línea

```c
// Usar \ al final de cada línea para continuar:
#define SWAP(a, b) do { \
    typeof(a) _tmp = (a); \
    (a) = (b); \
    (b) = _tmp; \
} while (0)

// El \ debe ser el ÚLTIMO carácter de la línea.
// Sin espacio después del \.
```

## #undef — Eliminar un macro

```c
#define DEBUG 1
// ... código con debug ...
#undef DEBUG
// DEBUG ya no está definido

// Uso: cambiar el comportamiento por secciones:
#define MODE "fast"
// ...
#undef MODE
#define MODE "safe"
// ...

// Uso: redefinir un macro (sin #undef da warning):
#define MAX 100
#undef MAX
#define MAX 200
```

## Macros vs constantes vs inline

```c
// Macro:
#define PI 3.14159
// Sin tipo, sustitución de texto, puede causar problemas.

// const:
const double PI = 3.14159;
// Con tipo, se verifica en compilación, aparece en debugger.
// PERO: en C, const no es una "constante de compilación"
// (no se puede usar en case labels ni tamaños de arrays globales).

// enum:
enum { MAX_SIZE = 1024 };
// Constante entera de compilación. Se puede usar en case y arrays.
// Solo para enteros.

// Recomendación:
// - Enteros constantes: enum { NAME = value }
// - Flotantes constantes: static const double NAME = value
//   o #define NAME value
// - Funciones pequeñas: static inline (no macros)
// - Macros: solo cuando no hay alternativa (stringify, concat,
//   genéricos, compilación condicional)
```

---

## Ejercicios

### Ejercicio 1 — Macros utilitarias

```c
// Implementar las siguientes macros:
// - SWAP(a, b) — intercambiar dos valores (usar typeof)
// - IS_POWER_OF_TWO(n) — verificar si n es potencia de 2
// - ARRAY_SIZE(arr) — número de elementos
// - BIT(n) — (1u << (n))
// Probar cada macro con varios tipos y valores.
```

### Ejercicio 2 — Stringify y token pasting

```c
// Implementar una macro PRINT_VAR(x) que imprima:
// "x = 42" (el nombre de la variable y su valor)
// Usar # para obtener el nombre como string.
// Soportar int (%d), double (%f) y char* (%s) con _Generic.
```

### Ejercicio 3 — X-macros

```c
// Usar X-macros para definir un enum de errores con:
// - El enum (ERR_NONE, ERR_FILE, ERR_MEMORY, ERR_PARSE)
// - Un array de strings ("NONE", "FILE", "MEMORY", "PARSE")
// - Una función error_name(enum Error e) que retorne el string
// Agregar un error nuevo y verificar que todo se actualiza.
```
