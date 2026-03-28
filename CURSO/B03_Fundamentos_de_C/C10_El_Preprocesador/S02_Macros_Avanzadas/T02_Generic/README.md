# T02 — _Generic

## Qué es _Generic

`_Generic` es una expresión de selección por tipo introducida
en C11. Permite elegir una expresión según el tipo de un argumento,
similar a function overloading de C++:

```c
#include <stdio.h>

#define print_val(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x),     \
    char*:  printf("%s\n", x)      \
)

int main(void) {
    print_val(42);        // selecciona int    → printf("%d\n", 42)
    print_val(3.14);      // selecciona double → printf("%f\n", 3.14)
    print_val("hello");   // selecciona char*  → printf("%s\n", "hello")
    return 0;
}
```

## Sintaxis

```c
_Generic(expresión_control,
    tipo1: resultado1,
    tipo2: resultado2,
    ...
    default: resultado_default
)

// - La expresión_control NO se evalúa — solo se usa su tipo.
// - Se selecciona el resultado cuyo tipo coincide.
// - Solo UN resultado se compila (los demás se descartan).
// - Si ningún tipo coincide y hay default, se usa default.
// - Si ningún tipo coincide y NO hay default → error de compilación.
```

```c
// La expresión de control determina el tipo:
int x = 5;
_Generic(x, int: "int", double: "double")    // → "int"

// El tipo se determina DESPUÉS de las conversiones habituales:
// - Arrays decaen a punteros
// - Funciones decaen a punteros a función
// - No se aplica integer promotion (short sigue siendo short)

char arr[10];
_Generic(arr, char*: "ptr", char[10]: "arr")  // → "ptr" (decay)

// Calificadores (const, volatile) SÍ importan:
const int cx = 5;
_Generic(cx, int: "int", const int: "const int")  // → "const int"
// Nota: en la práctica, esto varía entre compiladores.
// GCC ignora top-level qualifiers; Clang los mantiene.
// Para portabilidad, no depender de const en _Generic.
```

## Despacho a funciones por tipo

El uso principal de `_Generic` es seleccionar funciones
según el tipo del argumento:

```c
#include <math.h>

// C no tiene overloading, pero con _Generic se simula:
#define cbrt(x) _Generic((x), \
    float:       cbrtf,       \
    double:      cbrt,        \
    long double: cbrtl        \
)(x)

float  a = cbrt(27.0f);     // → cbrtf(27.0f)
double b = cbrt(27.0);      // → cbrt(27.0)

// <tgmath.h> de C11 usa _Generic internamente para esto:
// #include <tgmath.h>
// sqrt(27.0f) → sqrtf
// sqrt(27.0)  → sqrt
// sqrt(27.0L) → sqrtl
```

```c
// Implementar max type-safe:
static inline int    max_int(int a, int b)       { return a > b ? a : b; }
static inline double max_dbl(double a, double b) { return a > b ? a : b; }
static inline long   max_lng(long a, long b)     { return a > b ? a : b; }

#define max(a, b) _Generic((a), \
    int:    max_int,            \
    double: max_dbl,            \
    long:   max_lng             \
)(a, b)

int    m1 = max(3, 5);       // → max_int(3, 5)
double m2 = max(1.5, 2.7);   // → max_dbl(1.5, 2.7)
```

## Obtener el nombre del tipo como string

```c
#define type_name(x) _Generic((x), \
    _Bool:              "bool",     \
    char:               "char",     \
    signed char:        "signed char", \
    unsigned char:      "unsigned char", \
    short:              "short",    \
    unsigned short:     "unsigned short", \
    int:                "int",      \
    unsigned int:       "unsigned int", \
    long:               "long",     \
    unsigned long:      "unsigned long", \
    long long:          "long long", \
    unsigned long long: "unsigned long long", \
    float:              "float",    \
    double:             "double",   \
    long double:        "long double", \
    char*:              "char*",    \
    void*:              "void*",    \
    default:            "unknown"   \
)

printf("Type of x: %s\n", type_name(42));      // → "int"
printf("Type of y: %s\n", type_name(3.14f));   // → "float"
printf("Type of s: %s\n", type_name("hi"));    // → "char*"
```

## PRINT_VAR genérico

```c
// Combinar stringify (#) con _Generic para imprimir cualquier variable:

#define PRINT_VAR(var) _Generic((var), \
    int:                printf(#var " = %d\n",    var), \
    unsigned int:       printf(#var " = %u\n",    var), \
    long:               printf(#var " = %ld\n",   var), \
    unsigned long:      printf(#var " = %lu\n",   var), \
    long long:          printf(#var " = %lld\n",  var), \
    unsigned long long: printf(#var " = %llu\n",  var), \
    float:              printf(#var " = %f\n",    var), \
    double:             printf(#var " = %f\n",    var), \
    char:               printf(#var " = '%c'\n",  var), \
    char*:              printf(#var " = \"%s\"\n", var), \
    default:            printf(#var " = (unknown type)\n") \
)

int count = 42;
double pi = 3.14;
char *name = "Alice";

PRINT_VAR(count);    // → count = 42
PRINT_VAR(pi);       // → pi = 3.140000
PRINT_VAR(name);     // → name = "Alice"
```

## Contenedores genéricos (type-safe)

```c
// _Generic permite crear APIs genéricas type-safe:

// Supongamos que tenemos arrays dinámicos para varios tipos:
typedef struct { int    *data; size_t len, cap; } IntArray;
typedef struct { double *data; size_t len, cap; } DblArray;

void int_array_push(IntArray *a, int val);
void dbl_array_push(DblArray *a, double val);

int    int_array_get(const IntArray *a, size_t i);
double dbl_array_get(const DblArray *a, size_t i);

// Interfaz unificada con _Generic:
#define array_push(arr, val) _Generic((val), \
    int:    int_array_push,                  \
    double: dbl_array_push                   \
)(arr, val)

#define array_get(arr, i) _Generic(*(arr)->data, \
    int:    int_array_get,                       \
    double: dbl_array_get                        \
)(arr, i)

// Uso:
IntArray ints = {0};
array_push(&ints, 42);       // → int_array_push(&ints, 42)
int x = array_get(&ints, 0); // → int_array_get(&ints, 0)

// Si se pasa el tipo incorrecto → error de compilación.
```

## Limitaciones de _Generic

```c
// 1. No soporta tipos struct directamente por nombre:
struct Point { int x, y; };
_Generic(pt,
    struct Point: "point"    // funciona, pero engorroso
)
// Cada struct es un tipo diferente — no escala bien.

// 2. No soporta punteros const vs non-const de forma portable:
// GCC y Clang difieren en cómo tratan const en _Generic.

// 3. No puede inspeccionar tipos compuestos (array sizes, etc.):
int arr5[5], arr10[10];
// No se puede distinguir int[5] de int[10] — ambos decaen a int*.

// 4. Todos los branches deben ser sintácticamente válidos:
// El compilador parsea TODOS los branches, aunque solo compile uno.
// Esto limita lo que se puede hacer con tipos incompatibles.

// 5. Solo se puede despachar por UN argumento:
#define add(a, b) _Generic((a), ...)
// El tipo de b no se verifica.
// Para despachar por dos argumentos: anidar _Generic (feo):
#define add(a, b) _Generic((a), \
    int:    _Generic((b), int: add_ii, double: add_id), \
    double: _Generic((b), int: add_di, double: add_dd)  \
)(a, b)
```

## _Generic vs alternativas

```c
// _Generic (C11):
// + Estándar
// + Type-safe en compilación
// + Zero overhead (selección en compilación)
// - Verboso
// - Solo despacho por tipo, no por valor

// Macros simples:
// #define MAX(a, b) ((a) > (b) ? (a) : (b))
// + Simple
// - No type-safe (MAX(42, "hello") compila)
// - Evaluación múltiple

// typeof + macros (extensión GCC):
// #define max(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
// + Evita evaluación múltiple
// + Funciona con cualquier tipo
// - No es estándar (hasta C23 donde typeof se estandariza)

// void* genéricos:
// void* container_get(Container *c, size_t i);
// + Máxima flexibilidad
// - No type-safe (todo es void*)
// - Runtime overhead posible
```

---

## Ejercicios

### Ejercicio 1 — PRINT_VAR completo

```c
// Implementar un macro PRINT_VAR(x) que imprima:
// "nombre = valor" con el formato correcto para:
// int, unsigned, long, float, double, char, char*, _Bool.
// Usar _Generic para seleccionar el format specifier.
// Probar con variables de cada tipo.
```

### Ejercicio 2 — abs genérico

```c
// Implementar un macro abs_val(x) que llame a:
// - abs(x) para int
// - labs(x) para long
// - llabs(x) para long long
// - fabsf(x) para float
// - fabs(x) para double
// Usar _Generic. Probar con valores negativos de cada tipo.
// Verificar que abs_val(-3.14) retorna 3.14 (double, no int).
```

### Ejercicio 3 — Swap type-safe

```c
// Crear funciones swap_int, swap_double, swap_char.
// Crear un macro SWAP(a, b) que use _Generic para despachar
// a la función correcta según el tipo de a.
// Intentar llamar SWAP con tipos no soportados y verificar
// que da error de compilación (no error de runtime).
```
