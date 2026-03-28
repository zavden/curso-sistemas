# T02 — _Generic

## Erratas detectadas

| Ubicacion | Error | Correccion |
|-----------|-------|------------|
| `README.md:58` | El comentario dice `_Generic(cx, int: "int", const int: "const int") // → "const int"`, pero en GCC el resultado es `"int"`. La conversion lvalue del controlling expression elimina los calificadores top-level (`const`, `volatile`) segun el estandar C11 (DR 423) | El resultado correcto en GCC es `"int"`, no `"const int"`. El texto posterior (lineas 59-61) advierte que varia entre compiladores, pero el comentario de la linea 58 es incorrecto para GCC |

---

## 1 — Que es _Generic

`_Generic` es una expresion de seleccion por tipo introducida en C11. Elige
una expresion segun el tipo de un argumento, en **tiempo de compilacion**:

```c
#define print_val(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x),     \
    char:   printf("'%c'\n", x),   \
    char*:  printf("%s\n", x)      \
)

print_val(42);        /* tipo int    → printf("%d\n", 42) */
print_val(3.14);      /* tipo double → printf("%f\n", 3.14) */
print_val("hello");   /* tipo char*  → printf("%s\n", "hello") */
```

No hay overhead en runtime: el compilador descarta todos los branches excepto
el seleccionado. Es la forma estandar de simular function overloading en C.

---

## 2 — Sintaxis y reglas

```c
_Generic(expresion_control,
    tipo1: resultado1,
    tipo2: resultado2,
    default: resultado_default
)
```

Reglas fundamentales:

1. **La expresion de control NO se evalua** — solo se usa su tipo
2. Se selecciona **un** resultado cuyo tipo coincida
3. Solo el resultado seleccionado se compila (los demas se descartan)
4. Si ningun tipo coincide y hay `default`, se usa `default`
5. Si ningun tipo coincide y **no** hay `default` → error de compilacion

**Tipos de literales en C** (importan para _Generic):

| Literal | Tipo |
|---------|------|
| `42` | `int` |
| `42L` | `long` |
| `42UL` | `unsigned long` |
| `42LL` | `long long` |
| `3.14` | `double` (NO float) |
| `3.14f` | `float` |
| `3.14L` | `long double` |
| `'A'` | `int` (NO char — en C++ si seria char) |
| `"hello"` | decae a `char*` |

**Array decay**: los arrays decaen a punteros en la expresion de control.
`char arr[10]` se ve como `char*`, no como `char[10]`.

**Calificadores top-level**: GCC aplica conversion lvalue al controlling
expression, que elimina `const` y `volatile`. Asi, `const int cx` se ve
como `int` en `_Generic`. No depender de qualifiers en `_Generic` para
portabilidad.

---

## 3 — Despacho a funciones por tipo

El patron idiomatico: `_Generic` selecciona la **funcion** y los parentesis
finales la **invocan**:

```c
#include <math.h>

#define cbrt(x) _Generic((x), \
    float:       cbrtf,       \
    double:      cbrt,        \
    long double: cbrtl        \
)(x)

float  a = cbrt(27.0f);   /* → cbrtf(27.0f) */
double b = cbrt(27.0);    /* → cbrt(27.0) — la funcion, no el macro */
```

Nota: el macro `cbrt` tiene el mismo nombre que la funcion `cbrt`. Esto
funciona porque C prohibe la re-expansion recursiva — si un macro aparece en
su propia expansion, no se vuelve a expandir. Asi `cbrt` en el branch `double`
refiere a la funcion de `<math.h>`.

Este es el mecanismo interno de `<tgmath.h>` (C11): las funciones
type-generic (`sqrt`, `sin`, `cos`, etc.) usan `_Generic` para despachar a
`sqrtf`/`sqrt`/`sqrtl` segun el tipo del argumento.

---

## 4 — type_name: obtener el nombre del tipo como string

Un macro que retorna el nombre del tipo como string literal:

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

type_name(42)         /* → "int" */
type_name(3.14f)      /* → "float" */
type_name('A')        /* → "int" (literal char es int en C) */
type_name("hello")    /* → "char*" (array decay) */
type_name((bool)1)    /* → "bool" */
```

Util para debugging — saber exactamente que tipo ve el compilador:

```c
printf("Type of x: %s\n", type_name(some_expression));
```

---

## 5 — PRINT_VAR: stringify + _Generic

Combinar `#` (stringify del preprocesador) con `_Generic` para imprimir
nombre y valor de cualquier variable con el formato correcto:

```c
#define PRINT_VAR(var) _Generic((var), \
    int:                printf(#var " = %d\n",    var), \
    unsigned int:       printf(#var " = %u\n",    var), \
    long:               printf(#var " = %ld\n",   var), \
    double:             printf(#var " = %f\n",    var), \
    float:              printf(#var " = %f\n",    var), \
    char:               printf(#var " = '%c'\n",  var), \
    char*:              printf(#var " = \"%s\"\n", var), \
    default:            printf(#var " = (unknown type)\n") \
)

int count = 42;
double pi = 3.14;
char *name = "Alice";

PRINT_VAR(count);    /* → count = 42 */
PRINT_VAR(pi);       /* → pi = 3.140000 */
PRINT_VAR(name);     /* → name = "Alice" */
```

Como funciona:
1. El preprocesador evalua `#var` primero → produce `"count"`, `"pi"`, etc.
2. La concatenacion de string literals une `"count"` con `" = %d\n"`
3. `_Generic` selecciona el branch correcto segun el tipo de `var`
4. Todo se resuelve en compilacion — cero overhead

---

## 6 — abs y max genericos

**abs_val** — despacha a la funcion de valor absoluto correcta:

```c
#include <stdlib.h>  /* abs, labs, llabs */
#include <math.h>    /* fabsf, fabs */

#define abs_val(x) _Generic((x), \
    int:         abs,             \
    long:        labs,            \
    long long:   llabs,           \
    float:       fabsf,           \
    double:      fabs             \
)(x)

abs_val(-42)     /* → abs(-42) = 42 */
abs_val(-3.14)   /* → fabs(-3.14) = 3.14 (NO truncado a int) */
abs_val(-2.71f)  /* → fabsf(-2.71f) = 2.71 */
```

**max type-safe** — requiere funciones auxiliares:

```c
static inline int    max_int(int a, int b)       { return a > b ? a : b; }
static inline double max_dbl(double a, double b) { return a > b ? a : b; }
static inline long   max_lng(long a, long b)     { return a > b ? a : b; }

#define max(a, b) _Generic((a), \
    int:    max_int,            \
    double: max_dbl,            \
    long:   max_lng             \
)(a, b)

max(10, 25)      /* → max_int(10, 25) = 25 */
max(1.5, 2.7)    /* → max_dbl(1.5, 2.7) = 2.7 */
```

Limitacion: `max` despacha solo por el tipo del **primer** argumento.
`max(10, 2.5)` llamaria `max_int(10, 2.5)` → el `2.5` se trunca a `int`.
Compilar con `-Wextra` para detectar conversiones implicitas.

---

## 7 — Clausula default vs error estricto

**Con default** — catch-all para tipos no listados:

```c
#define describe_type(x) _Generic((x), \
    int:     "integer",                 \
    double:  "floating-point",          \
    char*:   "string",                  \
    default: "unsupported type"         \
)

describe_type(42)      /* → "integer" */
describe_type(3.14f)   /* → "unsupported type" (float no listado) */
```

**Sin default** — error de compilacion para tipos no soportados:

```c
#define print_num(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x)      \
)

float f = 1.0f;
print_num(f);   /* ERROR: '_Generic' selector of type 'float'
                   is not compatible with any association */
```

Cuando usar cada uno:

| Situacion | Estrategia |
|-----------|-----------|
| API publica, flexibilidad | `default` con mensaje generico |
| Seguridad de tipo estricta | Sin `default` — fuerza error de compilacion |
| Debugging | `default` que imprime tipo con `type_name` |

La ventaja de omitir `default`: si alguien pasa un tipo inesperado, el error
es en **compilacion** (inmediato y claro), no en runtime (sutil y tardio).

---

## 8 — Limitaciones de _Generic

**1. Solo despacha por UN argumento:**

```c
/* max(a, b) solo revisa tipo de a, no de b */
#define max(a, b) _Generic((a), int: max_int, double: max_dbl)(a, b)

/* Para despachar por dos argumentos → anidamiento (feo): */
#define add(a, b) _Generic((a), \
    int:    _Generic((b), int: add_ii, double: add_id), \
    double: _Generic((b), int: add_di, double: add_dd)  \
)(a, b)
/* 2 tipos × 2 argumentos = 4 funciones. No escala. */
```

**2. Todos los branches se parsean:**

El compilador parsea todos los branches aunque solo compile uno. Las
expresiones deben ser sintacticamente validas para todos los tipos:

```c
/* Esto falla porque printf("%d", x) no es valido si x es char*: */
#define show(x) _Generic((x), \
    int:   printf("%d", x),   \
    char*: printf("%s", x)    \
)
/* Funciona en GCC/Clang porque solo COMPILAN el branch seleccionado,
   pero el estandar no lo garantiza. */
```

**3. Structs no escalan:**

```c
struct Point { int x, y; };
struct Vec3  { float x, y, z; };

_Generic(val, struct Point: ..., struct Vec3: ...)
/* Cada struct es un tipo distinto. Con muchos structs, la lista
   de asociaciones crece sin limite. */
```

**4. Tipos con calificadores (portabilidad):**

GCC elimina top-level `const`/`volatile` del controlling expression.
Clang historicamente los mantenia. No depender de calificadores en `_Generic`.

---

## 9 — Combinacion con variadic macros

`_Generic` se puede combinar con variadic macros y conteo de argumentos para
crear interfaces genericas que aceptan multiples valores de tipos diferentes:

```c
/* Paso 1: imprimir un valor con formato correcto */
#define print_one(x) _Generic((x), \
    int:    printf("%d", x),       \
    double: printf("%f", x),       \
    char*:  printf("\"%s\"", x),   \
    long:   printf("%ld", x),      \
    float:  printf("%f", x)        \
)

/* Paso 2: contar argumentos */
#define NARGS_IMPL(_1, _2, _3, N, ...) N
#define NARGS(...)  NARGS_IMPL(__VA_ARGS__, 3, 2, 1)

/* Paso 3: despachar por aridad */
#define CONCAT(a, b) a##b
#define DISPATCH(name, n) CONCAT(name, n)
#define PRINT_VALS(...) \
    DISPATCH(PRINT_VALS_, NARGS(__VA_ARGS__))(__VA_ARGS__)

#define PRINT_VALS_2(a, b) \
    do { print_one(a); printf(", "); print_one(b); \
         printf("\n"); } while(0)

#define PRINT_VALS_3(a, b, c) \
    do { print_one(a); printf(", "); print_one(b); \
         printf(", "); print_one(c); printf("\n"); } while(0)
```

Uso:

```c
PRINT_VALS(42, "hello");          /* → 42, "hello" */
PRINT_VALS(3.14, 100);            /* → 3.140000, 100 */
PRINT_VALS(42, 3.14, "world");    /* → 42, 3.140000, "world" */
```

Cada argumento se formatea independientemente segun su tipo. El conteo de
argumentos selecciona `PRINT_VALS_2` o `PRINT_VALS_3` automaticamente.

---

## 10 — Comparacion con Rust

Rust no tiene `_Generic` — tiene generics reales con traits:

```rust
// En Rust, los generics se parametrizan por tipo:
fn max<T: PartialOrd>(a: T, b: T) -> T {
    if a > b { a } else { b }
}

// UNA funcion sirve para int, float, cualquier tipo con PartialOrd.
// En C, necesitarias max_int, max_dbl, max_lng, etc.

let m1 = max(3, 5);        // T = i32
let m2 = max(1.5, 2.7);    // T = f64
// max(3, 2.5) → ERROR: tipos mixtos no permitidos sin conversion
```

Diferencias clave:

| Aspecto | `_Generic` (C11) | Generics (Rust) |
|---------|-----------------|-----------------|
| Mecanismo | Seleccion entre funciones existentes | Monomorphization: genera codigo por tipo |
| Tipo-seguro | Si (compilacion) | Si (compilacion + traits) |
| Flexibilidad | Lista finita de tipos | Cualquier tipo que implemente el trait |
| Verbosidad | Alta — una funcion por tipo | Baja — una funcion generica |
| Multi-argumento | Solo por un argumento | Multiples parametros de tipo |
| Overhead | Zero (compilacion) | Zero (monomorphization) |

```rust
// Display trait — equivalente a PRINT_VAR con _Generic:
fn print_var<T: std::fmt::Display>(name: &str, val: &T) {
    println!("{name} = {val}");
}
// Funciona con CUALQUIER tipo que implemente Display.
// En C, tendrias que listar cada tipo en _Generic.

// El trait system de Rust unifica lo que en C requiere:
// - _Generic (despacho por tipo)
// - void* (genericidad)
// - macros (generacion de codigo)
```

---

## Ejercicios

### Ejercicio 1 — print_val basico

```c
// Definir un macro print_val(x) con _Generic que imprima:
// - int: formato %d
// - double: formato %f
// - char: formato '%c'
// - char*: formato %s
//
// Probar con variables Y con literales directos.
// Predecir: que branch selecciona print_val('A')?
// Y print_val(2.71)?
```

<details><summary>Prediccion</summary>

- `print_val(42)` → branch `int` → `printf("%d\n", 42)` → `42`
- `print_val(3.14)` → branch `double` → `printf("%f\n", 3.14)` → `3.140000`
- `print_val('A')` → branch `int` (NO char — los character literals son `int` en C) → `printf("%d\n", 65)` → `65`
- `print_val(2.71)` → branch `double` (sin sufijo `f` = double) → `printf("%f\n", 2.71)` → `2.710000`
- Para que `'A'` use el branch `char`, necesitas: `char c = 'A'; print_val(c);`

</details>

### Ejercicio 2 — type_name para todos los tipos basicos

```c
// Definir type_name(x) que retorne el nombre del tipo como string
// para al menos 10 tipos: _Bool, char, int, unsigned int, long,
// float, double, char*, void*, y un default.
//
// Probar: type_name(42), type_name(3.14f), type_name("hi"),
//         type_name('Z'), type_name((void*)0)
```

<details><summary>Prediccion</summary>

- `type_name(42)` → `"int"` (literal entero sin sufijo)
- `type_name(3.14f)` → `"float"` (sufijo `f`)
- `type_name(3.14)` → `"double"` (sin sufijo = double)
- `type_name("hi")` → `"char*"` (array decay)
- `type_name('Z')` → `"int"` (character literal es int en C, no char)
- `type_name((void*)0)` → `"void*"` (cast explicito)
- `type_name(42UL)` → `"unsigned long"` (sufijo UL)

</details>

### Ejercicio 3 — PRINT_VAR con stringify

```c
// Definir PRINT_VAR(var) que imprima "nombre = valor" usando
// #var para el nombre y _Generic para el formato.
// Soportar: int, double, char, char*, unsigned int.
//
// Probar con:
//   int count = 42;
//   double pi = 3.14;
//   char *msg = "hello";
//   unsigned int flags = 0xFF;
```

<details><summary>Prediccion</summary>

```c
PRINT_VAR(count);    /* → "count = 42" */
PRINT_VAR(pi);       /* → "pi = 3.140000" */
PRINT_VAR(msg);      /* → "msg = \"hello\"" */
PRINT_VAR(flags);    /* → "flags = 255" (0xFF = 255 decimal, %u) */
```

El preprocesador resuelve `#var` → `"count"`, `"pi"`, etc. Luego `_Generic` selecciona el `printf` con el format specifier correcto. La concatenacion `#var " = %d\n"` produce `"count" " = %d\n"` → `"count = %d\n"`.

</details>

### Ejercicio 4 — abs_val generico

```c
// Definir abs_val(x) que despache a:
//   int → abs,  long → labs,  float → fabsf,  double → fabs
// Necesitas: #include <stdlib.h> y #include <math.h>
// Compilar con: gcc -std=c11 -Wall -Wextra prog.c -o prog -lm
//
// Probar: abs_val(-42), abs_val(-3.14), abs_val(-2.71f)
// Predecir: abs_val(-3.14) retorna 3.14 o 3?
```

<details><summary>Prediccion</summary>

- `abs_val(-42)` → `abs(-42)` → `42` (int)
- `abs_val(-3.14)` → `fabs(-3.14)` → `3.14` (double, NO truncado)
- `abs_val(-2.71f)` → `fabsf(-2.71f)` → `2.71` (float)
- `abs_val(-100000L)` → `labs(-100000L)` → `100000` (long)

Si en vez de `_Generic` usaras `abs()` directamente: `abs(-3.14)` truncaria a int → retornaria `3`. Con `_Generic`, el despacho a `fabs` preserva el valor decimal.

Patron: `_Generic((x), tipo: func)(x)` — los parentesis `(x)` invocan la funcion seleccionada.

</details>

### Ejercicio 5 — max type-safe con funciones auxiliares

```c
// 1. Definir max_int, max_dbl, max_lng como funciones static inline
// 2. Definir max(a, b) con _Generic por tipo de a
// 3. Probar: max(10, 25), max(1.5, 2.7), max(100L, -50L)
// 4. Pensar: que pasa con max(10, 2.5)?
//    (despacha por int, pero b es double)
```

<details><summary>Prediccion</summary>

- `max(10, 25)` → `max_int(10, 25)` → `25`
- `max(1.5, 2.7)` → `max_dbl(1.5, 2.7)` → `2.7`
- `max(100L, -50L)` → `max_lng(100, -50)` → `100`
- `max(10, 2.5)` → `max_int(10, 2.5)` → `2.5` se convierte implicitamente a `int` → `max_int(10, 2)` → `10`. Warning con `-Wextra`: conversion de double a int.

`_Generic` solo despacha por el primer argumento. Para seguridad con tipos mixtos, se necesitarian `_Generic` anidados (impractico) o verificar manualmente que ambos argumentos tengan el mismo tipo.

</details>

### Ejercicio 6 — default como catch-all vs error estricto

```c
// 1. Definir describe(x) CON default:
//    int → "integer", double → "float", char* → "string",
//    default → "other"
// 2. Probar con int, double, char*, float, long, struct
// 3. Quitar default y recompilar con float → observar el error
// 4. Comparar los mensajes de error: que informacion da el
//    compilador sobre el tipo que fallo?
```

<details><summary>Prediccion</summary>

Con `default`:
- `describe(42)` → `"integer"`
- `describe(3.14)` → `"float"`
- `describe(3.14f)` → `"other"` (float no listado, cae en default)
- `describe(42L)` → `"other"` (long no listado)
- Compila sin problemas, pero los tipos no soportados pasan silenciosamente

Sin `default`:
```
error: '_Generic' selector of type 'float' is not compatible
       with any association
```
- Error de compilacion claro, con el tipo exacto que fallo
- Mejor para APIs donde solo ciertos tipos son validos

</details>

### Ejercicio 7 — Evitar truncamiento con _Generic

```c
// Demostrar el problema de truncamiento sin _Generic:
//   int r = abs(-3.14);    // → 3 (truncado!)
//
// Resolver con abs_val(-3.14) que despacha a fabs.
// Compilar y comparar los dos resultados.
// Bonus: que retorna abs(-2147483648)?
// (pista: overflow de int en complemento a dos)
```

<details><summary>Prediccion</summary>

- `abs(-3.14)` → la conversion de -3.14 a int da -3, luego `abs(-3)` = 3. Se pierde la parte decimal.
- `abs_val(-3.14)` → `fabs(-3.14)` = 3.14. Tipo correcto, sin truncamiento.
- `abs(-2147483648)`: en complemento a dos de 32 bits, `-2147483648` es `INT_MIN`. `abs(INT_MIN)` es undefined behavior porque `2147483648` no cabe en `int` (INT_MAX = 2147483647). GCC puede retornar `-2147483648` (el mismo valor negativo) o comportamiento arbitrario.

</details>

### Ejercicio 8 — _Generic anidado para dos argumentos

```c
// Implementar add(a, b) que despache por tipo de AMBOS argumentos:
//   int+int → add_ii, int+double → add_id,
//   double+int → add_di, double+double → add_dd
//
// Usar _Generic anidados.
// Probar: add(1, 2), add(1, 2.5), add(1.5, 2), add(1.5, 2.5)
// Observar lo verboso que es — por que _Generic no escala
// para multi-dispatch.
```

<details><summary>Prediccion</summary>

```c
static inline int    add_ii(int a, int b)       { return a + b; }
static inline double add_id(int a, double b)    { return a + b; }
static inline double add_di(double a, int b)    { return a + b; }
static inline double add_dd(double a, double b) { return a + b; }

#define add(a, b) _Generic((a), \
    int:    _Generic((b), int: add_ii, double: add_id), \
    double: _Generic((b), int: add_di, double: add_dd)  \
)(a, b)
```

- `add(1, 2)` → `add_ii(1, 2)` → `3`
- `add(1, 2.5)` → `add_id(1, 2.5)` → `3.5`
- `add(1.5, 2)` → `add_di(1.5, 2)` → `3.5`
- `add(1.5, 2.5)` → `add_dd(1.5, 2.5)` → `4.0`

2 tipos × 2 argumentos = 4 funciones + 4 asociaciones anidadas. Con 3 tipos serian 9 funciones. N tipos × M argumentos = N^M combinaciones. No escala.

</details>

### Ejercicio 9 — PRINT_VALS con _Generic + variadic

```c
// 1. Definir print_one(x) con _Generic para int, double, char*
// 2. Definir PRINT_VALS_2(a, b) y PRINT_VALS_3(a, b, c)
//    que impriman valores separados por comas
// 3. Usar NARGS + DISPATCH para seleccionar automaticamente
// 4. Probar: PRINT_VALS(42, "hello")
//            PRINT_VALS(3.14, 100, "world")
```

<details><summary>Prediccion</summary>

- `PRINT_VALS(42, "hello")` → `NARGS` cuenta 2 → `PRINT_VALS_2(42, "hello")` → `print_one(42)` [int: %d] + `print_one("hello")` [char*: "%s"] → `42, "hello"`
- `PRINT_VALS(3.14, 100, "world")` → `NARGS` cuenta 3 → `PRINT_VALS_3(3.14, 100, "world")` → `3.140000, 100, "world"`

Cada argumento se formatea independientemente. Tipos mixtos en una sola llamada sin problemas.

</details>

### Ejercicio 10 — Contenedor generico con _Generic

```c
// 1. Definir IntArray y DblArray (struct con data, len, cap)
// 2. Implementar int_push, dbl_push, int_get, dbl_get
// 3. Definir array_push(arr, val) con _Generic por tipo de val
// 4. Definir array_get(arr, i) con _Generic por tipo de *arr->data
// 5. Probar que pasar un tipo incorrecto produce error de
//    compilacion (no error de runtime)
```

<details><summary>Prediccion</summary>

```c
typedef struct { int *data; size_t len, cap; } IntArray;
typedef struct { double *data; size_t len, cap; } DblArray;

#define array_push(arr, val) _Generic((val), \
    int:    int_push,                        \
    double: dbl_push                         \
)(arr, val)

#define array_get(arr, i) _Generic(*(arr)->data, \
    int:    int_get,                              \
    double: dbl_get                               \
)(arr, i)
```

- `array_push(&ints, 42)` → `int_push(&ints, 42)` ✓
- `array_push(&dbls, 3.14)` → `dbl_push(&dbls, 3.14)` ✓
- `array_push(&ints, "hello")` → error: `_Generic` selector of type `char*` not compatible

El truco en `array_get`: `*(arr)->data` desreferencia el puntero `data` para obtener el tipo del elemento (`int` o `double`). La expresion no se evalua — solo se usa su tipo.

</details>
