# T02 — #define

> Sin erratas detectadas en el material fuente.

---

## 1. Object-like macros — Sustitución de texto puro

```c
// Sintaxis: #define NOMBRE valor
// El preprocesador reemplaza CADA ocurrencia de NOMBRE por valor.

#define PI          3.14159265358979
#define MAX_SIZE    1024
#define VERSION     "1.0.0"
#define DEBUG       1

double area = PI * r * r;        // → 3.14159... * r * r
char buf[MAX_SIZE];               // → char buf[1024]
printf("v%s\n", VERSION);        // → printf("v%s\n", "1.0.0")
```

### La trampa de precedencia

```c
// El macro es texto — NO entiende aritmética ni precedencia:

#define BAD  1 + 2
int x = BAD * 3;     // → int x = 1 + 2 * 3 = 1 + 6 = 7  (¡no 9!)

#define GOOD (1 + 2)
int y = GOOD * 3;    // → int y = (1 + 2) * 3 = 9  ✓

// REGLA: siempre envolver en paréntesis las expresiones en macros.
```

### Macros vacíos y flags

```c
// Macro sin valor (existe pero está vacío):
#define FEATURE_ENABLED
// Útil con #ifdef FEATURE_ENABLED → verdadero

// Definir desde línea de comandos:
// gcc -DDEBUG prog.c          → equivale a #define DEBUG 1
// gcc -DMAX=256 prog.c        → equivale a #define MAX 256
// gcc -DVERSION='"2.0"' prog.c → equivale a #define VERSION "2.0"
```

### Ver la expansión con gcc -E

```bash
# gcc -E ejecuta SOLO el preprocesador:
gcc -E programa.c | tail -20     # Ver las últimas 20 líneas
gcc -E -P programa.c             # Sin marcadores de línea (#)

# Ejemplo: un archivo de 8 líneas con #include <stdio.h>
# se expande a ~800 líneas (todo stdio.h copiado)
```

---

## 2. Function-like macros — Macros con parámetros

```c
// Sintaxis: #define NOMBRE(params) expresión
// ¡SIN espacio entre NOMBRE y el paréntesis!

#define SQUARE(x) ((x) * (x))           // ← Correcto
#define SQUARE (x) ((x) * (x))          // ← INCORRECTO: macro object-like
//              ^-- el espacio cambia el significado completamente
```

### La regla de oro de los paréntesis

```c
// 1. Cada parámetro entre paréntesis
// 2. La expresión completa entre paréntesis

// MAL — sin paréntesis en parámetros:
#define SQUARE(x) x * x
SQUARE(2 + 3)    // → 2 + 3 * 2 + 3 = 2 + 6 + 3 = 11  (¡no 25!)

// MAL — sin paréntesis en la expresión:
#define SQUARE(x) (x) * (x)
100 / SQUARE(5)  // → 100 / (5) * (5) = 20 * 5 = 100  (¡no 4!)

// BIEN — paréntesis en TODO:
#define SQUARE(x) ((x) * (x))
SQUARE(2 + 3)    // → ((2 + 3) * (2 + 3)) = 25  ✓
100 / SQUARE(5)  // → 100 / ((5) * (5)) = 100 / 25 = 4  ✓
```

### Macros utilitarios comunes

```c
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define ABS(x)          ((x) >= 0 ? (x) : -(x))
#define CLAMP(x, lo, hi) (MIN(MAX((x), (lo)), (hi)))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define UNUSED(x)       ((void)(x))
#define BIT(n)          (1u << (n))
#define IS_POWER_OF_TWO(n) ((n) > 0 && ((n) & ((n) - 1)) == 0)
```

---

## 3. Doble evaluación — El peligro de los efectos secundarios

```c
#define SQUARE(x) ((x) * (x))

// Con un valor simple: sin problemas
int a = SQUARE(5);        // → ((5) * (5)) = 25  ✓

// Con efectos secundarios: PELIGRO
int counter = 0;
int next(void) { return ++counter; }

int b = SQUARE(next());
// Se expande a: ((next()) * (next()))
// next() se llama DOS VECES: counter = 2, resultado = 1 * 2 = 2
// Con una función real: next() se llama una vez, counter = 1, resultado = 1

// La macro evalúa su argumento UNA VEZ POR CADA APARICIÓN.
// Una función evalúa su argumento UNA SOLA VEZ.
```

### ¿Cómo evitarlo?

```c
// Opción 1 — static inline (recomendada):
static inline int square(int x) {
    return x * x;
}
// Evalúa x una sola vez. Type-safe. Aparece en el debugger.

// Opción 2 — GCC statement expression (extensión, no estándar):
#define SQUARE(x) ({       \
    __typeof__(x) _x = (x); \
    _x * _x;               \
})
// Evalúa x una sola vez asignándolo a una variable temporal.
// Solo funciona en GCC/Clang.

// Regla: si el macro evalúa un argumento más de una vez,
// documentar el peligro o usar inline en su lugar.
```

---

## 4. Operador # (stringification)

El operador `#` convierte un argumento de macro en string literal:

```c
#define STRINGIFY(x) #x

printf("%s\n", STRINGIFY(hello));     // → "hello"
printf("%s\n", STRINGIFY(1 + 2));     // → "1 + 2"
printf("%s\n", STRINGIFY(foo bar));   // → "foo bar"
```

### El problema: # NO expande macros

```c
#define STRINGIFY(x) #x
#define VERSION 42

printf("%s\n", STRINGIFY(VERSION));   // → "VERSION"  (¡no "42"!)
// # convierte el token VERSION a string ANTES de expandirlo.
```

### La solución: XSTRINGIFY (doble nivel)

```c
#define STRINGIFY(x)  #x
#define XSTRINGIFY(x) STRINGIFY(x)

#define VERSION 42

STRINGIFY(VERSION)    // → #VERSION → "VERSION"
XSTRINGIFY(VERSION)  // → STRINGIFY(42) → #42 → "42"  ✓

// ¿Por qué funciona?
// XSTRINGIFY(VERSION)
//   → El preprocesador expande VERSION a 42 (primer nivel)
//   → STRINGIFY(42)
//   → #42 → "42" (segundo nivel)
```

### Uso práctico: macro ASSERT

```c
#define ASSERT(cond) do {                                    \
    if (!(cond)) {                                           \
        fprintf(stderr, "Assertion failed: %s\n"             \
                "  at %s:%d\n", #cond, __FILE__, __LINE__);  \
        abort();                                              \
    }                                                         \
} while (0)

ASSERT(x > 0);
// Si x = -5, imprime:
// Assertion failed: x > 0
//   at main.c:42
// #cond se expande a "x > 0" — el texto literal de la condición.
```

---

## 5. Operador ## (token pasting)

El operador `##` concatena dos tokens en uno:

```c
#define CONCAT(a, b)  a##b
#define MAKE_VAR(n)   var_##n

int CONCAT(my, Var) = 42;     // → int myVar = 42;
CONCAT(print, f)("hi\n");     // → printf("hi\n");

int MAKE_VAR(1) = 10;         // → int var_1 = 10;
int MAKE_VAR(2) = 20;         // → int var_2 = 20;
```

### Uso: generar funciones para múltiples tipos

```c
#define DECLARE_VECTOR(type)                                      \
    typedef struct {                                              \
        type *data;                                               \
        size_t len, cap;                                          \
    } vector_##type;                                              \
    vector_##type vector_##type##_create(void);                   \
    void vector_##type##_push(vector_##type *v, type val);        \
    void vector_##type##_free(vector_##type *v);

DECLARE_VECTOR(int)
// Genera:
// typedef struct { int *data; size_t len, cap; } vector_int;
// vector_int vector_int_create(void);
// void vector_int_push(vector_int *v, int val);
// void vector_int_free(vector_int *v);

DECLARE_VECTOR(double)
// Genera lo mismo para double.
```

### Regla de ##: no puede producir tokens inválidos

```c
#define BAD(a, b) a ## ## b    // ERROR: ## ## no es un token válido
#define BAD(a, b) a##           // ERROR: ## sin operando derecho
```

---

## 6. X-macros — Mantener datos sincronizados

El patrón X-macro define una lista de datos UNA sola vez y la usa para generar múltiples estructuras (enums, arrays de strings, switch cases):

```c
// Paso 1: definir la lista UNA vez
#define COLORS(X) \
    X(RED)        \
    X(GREEN)      \
    X(BLUE)       \
    X(YELLOW)

// Paso 2: definir qué hacer con cada elemento
#define AS_ENUM(name)   name,
#define AS_STRING(name) #name,

// Paso 3: usar la lista para generar código
enum Color { COLORS(AS_ENUM) COLOR_COUNT };
// Expande a: enum Color { RED, GREEN, BLUE, YELLOW, COLOR_COUNT };

static const char *color_names[] = { COLORS(AS_STRING) };
// Expande a: ... = { "RED", "GREEN", "BLUE", "YELLOW", };
```

### Ventaja: agregar un color nuevo

```c
// Solo cambiar UN lugar:
#define COLORS(X) \
    X(RED)        \
    X(GREEN)      \
    X(BLUE)       \
    X(YELLOW)     \
    X(PURPLE)     // ← Agregar aquí

// El enum Y el array de strings se actualizan AUTOMÁTICAMENTE.
// Sin X-macros, hay que actualizar manualmente en 2+ lugares
// → riesgo de que queden desincronizados.
```

### X-macro con datos extra

```c
#define HTTP_CODES(X)          \
    X(200, OK)                 \
    X(404, NOT_FOUND)          \
    X(500, INTERNAL_ERROR)

#define AS_ENUM2(code, name)  HTTP_##name = code,
#define AS_CASE(code, name)   case code: return #name;

enum HttpCode { HTTP_CODES(AS_ENUM2) };
// → enum HttpCode { HTTP_OK = 200, HTTP_NOT_FOUND = 404, ... };

const char *http_name(int code) {
    switch (code) {
        HTTP_CODES(AS_CASE)
        // → case 200: return "OK";
        // → case 404: return "NOT_FOUND";
        // → case 500: return "INTERNAL_ERROR";
        default: return "UNKNOWN";
    }
}
```

---

## 7. Macros multi-línea y do-while(0)

### Continuación con \

```c
// El \ al final de la línea DEBE ser el último carácter
// (sin espacio ni tab después):

#define SWAP(a, b) do {           \
    __typeof__(a) _tmp = (a);     \
    (a) = (b);                    \
    (b) = _tmp;                   \
} while (0)
```

### ¿Por qué do { ... } while (0)?

```c
// Sin do-while(0), el macro se rompe con if/else:

#define BAD_SWAP(a, b)  \
    int _tmp = (a);     \
    (a) = (b);          \
    (b) = _tmp;

if (condition)
    BAD_SWAP(x, y);    // ← Solo la primera línea está dentro del if
else                    // ← ERROR: else sin if
    printf("no swap");

// Se expande a:
if (condition)
    int _tmp = (x);     // ← Solo esta línea en el if
    (x) = (y);          // ← Siempre se ejecuta
    (y) = _tmp;         // ← Siempre se ejecuta
else                    // ← Error de compilación: else sin if

// Con do-while(0):
if (condition)
    do { __typeof__(x) _tmp = (x); (x) = (y); (y) = _tmp; } while (0);
else
    printf("no swap");
// ← Funciona correctamente: todo dentro del do-while es una sola sentencia.
```

### ¿Por qué no simplemente { }?

```c
#define SWAP_BRACES(a, b) { int _tmp = (a); (a) = (b); (b) = _tmp; }

if (condition)
    SWAP_BRACES(x, y);   // ← El ; después de } crea una sentencia vacía
else                       // ← ERROR: else sin if (el ; rompió el if)
    ...

// do { ... } while (0) absorbe el ; final sin problemas.
```

---

## 8. #undef y -D — Redefinición y configuración

### #undef: eliminar un macro

```c
#define MODE "fast"
printf("Mode: %s\n", MODE);   // → "fast"

#undef MODE
// MODE ya no existe. Usarlo causaría error de compilación.
// #ifdef MODE es falso a partir de aquí.

// Patrón: #undef + #define para redefinir sin warning
#undef MODE
#define MODE "safe"
printf("Mode: %s\n", MODE);   // → "safe"

// Sin #undef previo:
// #define MODE "safe"
// warning: "MODE" redefined
```

### -D: definir macros desde la línea de comandos

```bash
# Enteros:
gcc -DMAX_SIZE=4096 programa.c

# Strings (ojo con las comillas del shell):
gcc -DVERSION='"2.0.0"' programa.c

# Flag sin valor (equivale a #define DEBUG 1):
gcc -DDEBUG programa.c

# Múltiples -D:
gcc -DDEBUG -DMAX=100 -DPLATFORM='"linux"' programa.c
```

### Patrón: defaults configurables

```c
// En el código fuente:
#ifndef MAX_SIZE
#define MAX_SIZE 1024    // Valor por defecto
#endif

#ifndef BUILD_MODE
#define BUILD_MODE "debug"
#endif

// Si se compila con -DMAX_SIZE=4096, usa 4096.
// Si se compila sin -D, usa 1024.
// El #ifndef evita que se redefina si ya viene de -D.
```

### -U: eliminar un macro desde la línea de comandos

```bash
# Desactivar un macro definido en el código:
gcc -UDEBUG programa.c
# Equivale a #undef DEBUG al inicio del archivo.
```

---

## 9. Macros vs const vs enum vs inline

| Mecanismo | Tipo | Scope | Debugger | Compilación | Uso ideal |
|-----------|------|-------|----------|-------------|-----------|
| `#define PI 3.14` | Sin tipo | Global (preprocesador) | No visible | Sustitución de texto | Strings, expresiones condicionales |
| `const double PI = 3.14` | Tipado | Bloque/archivo | Visible | Verificación de tipos | Flotantes, constantes con tipo |
| `enum { MAX = 100 }` | int | Bloque/archivo | Visible | Constante de compilación | Enteros, case labels, array sizes |
| `static inline` | Tipado | Archivo (con header) | Visible | Como función | Funciones pequeñas, reemplaza macros |

### Constantes enteras: enum > #define

```c
// #define:
#define MAX_SIZE 1024
// Sin tipo, sin scope, no aparece en debugger, conflicto de nombres posible

// enum:
enum { MAX_SIZE = 1024 };
// Tipo int, scope de bloque, aparece en debugger, constante de compilación
// Se puede usar en case labels y tamaños de arrays
// ¡PREFERIR enum para constantes enteras!
```

### Funciones pequeñas: inline > macro

```c
// Macro:
#define SQUARE(x) ((x) * (x))
// Doble evaluación, sin tipo, difícil de depurar

// Inline:
static inline int square(int x) { return x * x; }
// Evaluación única, tipado, aparece en debugger, sin sorpresas
// ¡PREFERIR inline para funciones!
```

### Cuándo SÍ usar macros (no hay alternativa)

```c
// 1. Stringification (#):
#define ASSERT(cond) ... #cond ...
// No se puede con inline — necesitas el texto del código fuente

// 2. Token pasting (##):
#define DECLARE_VECTOR(type) struct vector_##type { ... }
// No se puede con inline — necesitas generar identificadores

// 3. __FILE__ y __LINE__ del sitio de llamada:
#define LOG(msg) printf("[%s:%d] %s\n", __FILE__, __LINE__, msg)
// Si fuera inline, __FILE__/__LINE__ serían del header, no del .c

// 4. Compilación condicional:
#define DEBUG_PRINT(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

// 5. Genéricos con _Generic (próximo tópico)
```

---

## 10. Comparación con Rust

| Concepto C | Rust | Diferencia |
|-----------|------|-----------|
| `#define PI 3.14` | `const PI: f64 = 3.14;` | Rust: tipado, scoped, evaluado en compilación |
| `#define MAX(a,b) ((a)>(b)?(a):(b))` | Genéricos: `fn max<T: Ord>(a: T, b: T) -> T` | Rust: sin doble evaluación, tipado |
| `#define STRINGIFY(x) #x` | `stringify!(x)` macro built-in | Macro de compilación, no preprocessor |
| `#define CONCAT(a,b) a##b` | `paste!` crate + `concat_idents!` | Macros procedurales |
| X-macros | `#[derive(...)]` + macros procedurales | Generación de código en compilación |
| `gcc -DDEBUG` | `cfg!(debug_assertions)` | Compilación condicional con features |
| `#undef` + `#define` | No existe (const es inmutable) | No se puede redefinir |

```rust
// Rust tiene DOS sistemas de macros (ninguno es un preprocesador de texto):

// 1. Macros declarativos (macro_rules!):
macro_rules! max {
    ($a:expr, $b:expr) => {
        {
            let a = $a;    // Evalúa una sola vez (sin doble evaluación)
            let b = $b;
            if a > b { a } else { b }
        }
    };
}

// 2. Macros procedurales (derive, attribute):
#[derive(Debug, Clone)]   // Genera código automáticamente
struct Point {
    x: f64,
    y: f64,
}

// Ventajas sobre C:
// - No hay sustitución de texto → sin trampas de precedencia
// - Evaluación higiénica → sin conflicto de nombres de variables
// - Type-safe → errores en compilación, no en runtime
// - No hay doble evaluación accidental
// - Aparecen en el debugger con --pretty=expanded
```

**Filosofía**: en C, los macros son la herramienta multiusos por defecto porque el lenguaje no tiene genéricos, metaprogramación, ni evaluación constante avanzada. En Rust, los macros son el último recurso — `const`, `fn`, genéricos y traits cubren la mayoría de los casos.

---

## Ejercicios

### Ejercicio 1 — Trampa de precedencia

```c
// Definir estas macros SIN paréntesis:
//   #define DOUBLE(x)  x + x
//   #define HALF(x)    x / 2
//   #define NEGATE(x)  -x
//
// Probar con expresiones compuestas:
//   DOUBLE(3) * 2       → ¿Resultado?
//   HALF(10 + 2)        → ¿Resultado?
//   NEGATE(5 - 3)       → ¿Resultado?
//   100 / DOUBLE(5)     → ¿Resultado?
//
// Verificar con gcc -E. Luego corregir con paréntesis y comparar.
```

<details><summary>Predicción</summary>

Sin paréntesis:
- `DOUBLE(3) * 2` → `3 + 3 * 2` = `3 + 6` = **9** (no 12)
- `HALF(10 + 2)` → `10 + 2 / 2` = `10 + 1` = **11** (no 6)
- `NEGATE(5 - 3)` → `-5 - 3` = **-8** (no -2)
- `100 / DOUBLE(5)` → `100 / 5 + 5` = `20 + 5` = **25** (no 10)

Con paréntesis correctos: `((x) + (x))`, `((x) / 2)`, `(-(x))`:
- `DOUBLE(3) * 2` → `((3) + (3)) * 2` = 12 ✓
- `HALF(10 + 2)` → `((10 + 2) / 2)` = 6 ✓
- `NEGATE(5 - 3)` → `(-(5 - 3))` = -2 ✓
- `100 / DOUBLE(5)` → `100 / ((5) + (5))` = 10 ✓

</details>

### Ejercicio 2 — Doble evaluación en acción

```c
// Definir:
//   #define MAX(a, b) ((a) > (b) ? (a) : (b))
//
// Crear una función con efecto secundario:
//   int get_value(const char *name) {
//       static int calls = 0;
//       calls++;
//       printf("  get_value(%s) called [%d times total]\n", name, calls);
//       return atoi(name + 1);  // "A5" → 5
//   }
//
// Ejecutar: int m = MAX(get_value("A5"), get_value("B3"));
// ¿Cuántas veces se llama get_value? ¿Cuál es el resultado?
//
// Comparar con una función inline max(int a, int b).
```

<details><summary>Predicción</summary>

`MAX(get_value("A5"), get_value("B3"))` expande a:
```c
((get_value("A5")) > (get_value("B3")) ? (get_value("A5")) : (get_value("B3")))
```

`get_value` se llama **3 veces**, no 2:
1. `get_value("A5")` → 5 (para la comparación)
2. `get_value("B3")` → 3 (para la comparación)
3. 5 > 3 → verdadero → `get_value("A5")` → 5 (para el resultado)

Total: 3 llamadas. El resultado es 5 (correcto), pero se ejecutó una llamada extra.

Con `static inline int max(int a, int b)`: solo 2 llamadas a `get_value` (una por argumento). Sin evaluación extra.

Si ambos valores fueran iguales, el branch `?:` tomaría el segundo operando, llamando `get_value("B3")` una segunda vez → también 3 llamadas.

</details>

### Ejercicio 3 — STRINGIFY vs XSTRINGIFY

```c
// Definir:
//   #define STRINGIFY(x)  #x
//   #define XSTRINGIFY(x) STRINGIFY(x)
//   #define APP_NAME  MyApp
//   #define APP_VER   3
//
// Predecir la salida de:
//   printf("%s\n", STRINGIFY(APP_NAME));
//   printf("%s\n", XSTRINGIFY(APP_NAME));
//   printf("%s\n", STRINGIFY(APP_VER + 1));
//   printf("%s\n", XSTRINGIFY(APP_VER + 1));
//   printf("%s\n", STRINGIFY(APP_VER));
//   printf("%s\n", XSTRINGIFY(APP_VER));
//
// Verificar con gcc -E.
```

<details><summary>Predicción</summary>

- `STRINGIFY(APP_NAME)` → `#APP_NAME` → `"APP_NAME"` (sin expandir)
- `XSTRINGIFY(APP_NAME)` → `STRINGIFY(MyApp)` → `#MyApp` → `"MyApp"` (expandido)
- `STRINGIFY(APP_VER + 1)` → `#APP_VER + 1` → `"APP_VER + 1"` (todo literal, sin expandir)
- `XSTRINGIFY(APP_VER + 1)` → `STRINGIFY(3 + 1)` → `#3 + 1` → `"3 + 1"` (APP_VER expandido a 3, pero +1 queda como texto, NO se calcula)
- `STRINGIFY(APP_VER)` → `"APP_VER"`
- `XSTRINGIFY(APP_VER)` → `STRINGIFY(3)` → `"3"`

El preprocesador NO hace aritmética. `3 + 1` queda como string literal, no se convierte en `4`.

</details>

### Ejercicio 4 — Token pasting para generar código

```c
// Usar ## para crear un "type-safe container" de enteros:
//
// #define DECLARE_STACK(type) ...
//
// Que genere:
//   struct stack_int { int *data; int top; int cap; };
//   struct stack_int stack_int_create(int cap);
//   void stack_int_push(struct stack_int *s, int val);
//   int stack_int_pop(struct stack_int *s);
//
// Implementar DECLARE_STACK(int) y DECLARE_STACK(double).
// Verificar la expansión con gcc -E.
//
// Pregunta: ¿funciona DECLARE_STACK(unsigned int)? ¿Por qué?
```

<details><summary>Predicción</summary>

`DECLARE_STACK(int)` genera correctamente `stack_int`, `stack_int_create`, etc.

`DECLARE_STACK(unsigned int)` **no funciona** porque `##` concatena tokens: `stack_##unsigned int` produce `stack_unsigned` seguido del token `int` separado. El resultado sería un error de sintaxis.

Solución: usar un typedef primero: `typedef unsigned int uint;` y luego `DECLARE_STACK(uint)`.

Esta es una limitación fundamental de `##`: solo funciona con tokens simples (una sola "palabra"). Tipos compuestos como `unsigned int`, `long long`, `struct Point` no funcionan directamente.

</details>

### Ejercicio 5 — X-macro con switch

```c
// Definir un X-macro para errores:
//   #define ERRORS(X)      \
//       X(ERR_NONE, 0)     \
//       X(ERR_FILE, 1)     \
//       X(ERR_MEMORY, 2)   \
//       X(ERR_PARSE, 3)    \
//       X(ERR_NETWORK, 4)
//
// Generar:
//   a) enum Error { ERR_NONE = 0, ERR_FILE = 1, ... }
//   b) const char *error_name(enum Error e) con switch
//   c) const char *error_names[] = { "ERR_NONE", "ERR_FILE", ... }
//
// Agregar ERR_TIMEOUT al X-macro y verificar que las tres
// generaciones se actualizan automáticamente sin tocar más código.
```

<details><summary>Predicción</summary>

Las tres generaciones usan macros diferentes sobre la misma lista:

```c
#define AS_ENUM(name, val)   name = val,
#define AS_CASE(name, val)   case name: return #name;
#define AS_STRING(name, val) [val] = #name,

enum Error { ERRORS(AS_ENUM) };
const char *error_names[] = { ERRORS(AS_STRING) };
const char *error_name(enum Error e) {
    switch (e) { ERRORS(AS_CASE) default: return "UNKNOWN"; }
}
```

Al agregar `X(ERR_TIMEOUT, 5)` a `ERRORS`, las tres generaciones se actualizan automáticamente:
- El enum incluye `ERR_TIMEOUT = 5`
- El switch incluye `case ERR_TIMEOUT: return "ERR_TIMEOUT"`
- El array incluye `[5] = "ERR_TIMEOUT"`

Sin X-macros habría que editar 3 lugares manualmente — alto riesgo de olvido.

</details>

### Ejercicio 6 — PRINT_VAR genérico

```c
// Implementar un macro PRINT_VAR(x) que imprima:
//   "nombre = valor"
// Soportando int, double, char*, y char con _Generic:
//
// int n = 42;
// PRINT_VAR(n);       // → "n = 42"
//
// double pi = 3.14;
// PRINT_VAR(pi);      // → "pi = 3.140000"
//
// char *s = "hello";
// PRINT_VAR(s);       // → "s = hello"
//
// Pista: usar #x para el nombre y _Generic para el format specifier.
```

<details><summary>Predicción</summary>

```c
#define FMT(x) _Generic((x), \
    int: "%d",                \
    double: "%f",             \
    char*: "%s",              \
    default: "%p")

#define PRINT_VAR(x) printf(#x " = " FMT(x) "\n", (x))
```

Espera, eso no compila directamente porque `printf` necesita un format string literal concatenado. La concatenación de string literals funciona solo en compilación, pero `_Generic` se resuelve en compilación también.

Alternativa más segura:
```c
#define PRINT_VAR(x) printf("%s = ", #x), \
    _Generic((x), \
        int: printf("%d\n", (x)), \
        double: printf("%f\n", (x)), \
        char*: printf("%s\n", (x)))
```

Salida:
- `PRINT_VAR(n)` → `n = 42`
- `PRINT_VAR(pi)` → `pi = 3.140000`
- `PRINT_VAR(s)` → `s = hello`

</details>

### Ejercicio 7 — do-while(0) vs llaves

```c
// Definir LOG en DOS versiones:
//
//   #define LOG_BAD(msg)  { printf("[LOG] %s\n", msg); }
//   #define LOG_GOOD(msg) do { printf("[LOG] %s\n", msg); } while (0)
//
// Probar ambas en:
//   if (error)
//       LOG_XXX("error occurred");
//   else
//       printf("all ok\n");
//
// ¿Cuál compila? ¿Cuál falla? Verificar con gcc -E.
```

<details><summary>Predicción</summary>

**LOG_BAD**: falla. Se expande a:
```c
if (error)
    { printf("[LOG] %s\n", "error occurred"); };
else
    printf("all ok\n");
```
El `;` después de `}` crea una sentencia vacía que termina el `if`. El `else` queda huérfano → `error: 'else' without a previous 'if'`.

**LOG_GOOD**: compila. Se expande a:
```c
if (error)
    do { printf("[LOG] %s\n", "error occurred"); } while (0);
else
    printf("all ok\n");
```
El `while (0);` absorbe el `;` correctamente. El `do-while` es una sola sentencia que funciona con `if/else`.

</details>

### Ejercicio 8 — -D para configuración de build

```c
// Crear un programa con estos defaults configurables:
//   #ifndef LOG_LEVEL
//   #define LOG_LEVEL 1    // 0=off, 1=error, 2=warn, 3=info, 4=debug
//   #endif
//
//   #ifndef OUTPUT_FILE
//   #define OUTPUT_FILE "output.txt"
//   #endif
//
// Implementar macros LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG
// que solo impriman si LOG_LEVEL es suficiente.
//
// Compilar con:
//   gcc prog.c -o prog                          (defaults)
//   gcc -DLOG_LEVEL=4 prog.c -o prog_debug      (todo activado)
//   gcc -DLOG_LEVEL=0 prog.c -o prog_silent     (nada)
//   gcc -DOUTPUT_FILE='"result.dat"' prog.c      (archivo custom)
```

<details><summary>Predicción</summary>

```c
#define LOG_ERROR(msg) do { if (LOG_LEVEL >= 1) fprintf(stderr, "[ERR] %s\n", msg); } while(0)
#define LOG_WARN(msg)  do { if (LOG_LEVEL >= 2) fprintf(stderr, "[WRN] %s\n", msg); } while(0)
#define LOG_INFO(msg)  do { if (LOG_LEVEL >= 3) fprintf(stderr, "[INF] %s\n", msg); } while(0)
#define LOG_DEBUG(msg) do { if (LOG_LEVEL >= 4) fprintf(stderr, "[DBG] %s\n", msg); } while(0)
```

Con `-DLOG_LEVEL=4`: todas las macros imprimen. Con `-DLOG_LEVEL=0`: ninguna imprime. El compilador optimiza `if (0 >= 1)` eliminando el código muerto.

Con `-DLOG_LEVEL=0`, el código de las macros desactivadas no se ejecuta pero SÍ se compila (el compilador verifica sintaxis). Esto es mejor que `#if LOG_LEVEL >= 1` (que eliminaría el código del preprocesador y podría ocultar errores de compilación).

</details>

### Ejercicio 9 — ARRAY_SIZE seguro

```c
// ARRAY_SIZE(arr) = sizeof(arr) / sizeof(arr[0])
// funciona con arrays, pero NO detecta punteros:
//
// int arr[10];
// ARRAY_SIZE(arr)    → 10 ✓
//
// int *ptr = arr;
// ARRAY_SIZE(ptr)    → sizeof(int*)/sizeof(int) = 2 en 64-bit ✗
//
// Implementar ARRAY_SIZE_SAFE que cause error de compilación
// si se usa con un puntero en vez de un array.
//
// Pista: sizeof(*&(arr)) == sizeof(arr) solo si arr es un array.
// O usar: (sizeof(arr) / sizeof((arr)[0])) + 0*sizeof(struct { ... })
```

<details><summary>Predicción</summary>

Una versión segura para GCC/Clang:

```c
#define ARRAY_SIZE_SAFE(arr)                           \
    (sizeof(arr) / sizeof((arr)[0])                    \
     + 0 * sizeof(struct {                             \
         _Static_assert(                               \
             !__builtin_types_compatible_p(             \
                 __typeof__(arr), __typeof__(&(arr)[0]) \
             ), "ARRAY_SIZE used on pointer");          \
     }))
```

O más simple con `_Static_assert` separado:

```c
#define ARRAY_SIZE_SAFE(arr) \
    (sizeof(arr) / sizeof((arr)[0]))

// Antes de usar:
_Static_assert(sizeof(arr) != sizeof(void*) || sizeof(arr[0]) == sizeof(void*),
               "Possible pointer passed to ARRAY_SIZE");
```

Para `int arr[10]`: `sizeof(arr)` = 40, `sizeof(&arr[0])` = 8. `__typeof__(arr)` es `int[10]`, `__typeof__(&arr[0])` es `int*`. No son compatibles → assert pasa.

Para `int *ptr`: `__typeof__(ptr)` es `int*`, `__typeof__(&ptr[0])` es `int*`. Son compatibles → assert falla con error de compilación.

</details>

### Ejercicio 10 — Macro DEFER para cleanup

```c
// Implementar un macro DEFER que ejecute una expresión al final
// del scope actual (simulando defer de Go/Zig):
//
// void process(const char *path) {
//     FILE *f = fopen(path, "r");
//     DEFER(fclose(f));           // Se ejecuta al salir de la función
//
//     int fd = open("tmp", O_RDWR | O_CREAT, 0644);
//     DEFER(close(fd));           // Se ejecuta al salir
//
//     // ... usar f y fd ...
//     // Al retornar, close(fd) y fclose(f) se ejecutan en orden inverso
// }
//
// Pista: usar __attribute__((cleanup)) de GCC.
// Generar nombres únicos con ## y __COUNTER__.
```

<details><summary>Predicción</summary>

`__attribute__((cleanup(func)))` llama a `func(&var)` cuando `var` sale del scope. Combinando con macros:

```c
#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#define DEFER(expr)                                    \
    void CONCAT(_defer_fn_, __COUNTER__)(int *_) {     \
        (void)_; expr;                                 \
    }                                                  \
    int CONCAT(_defer_var_, __COUNTER__)               \
        __attribute__((cleanup(CONCAT(_defer_fn_, __COUNTER__)))) = 0

// Problema: esto no funciona directamente porque no puedes definir
// funciones dentro de funciones en C estándar.
```

Alternativa funcional con bloques GCC:

```c
#define DEFER(expr) \
    __attribute__((cleanup(_defer_cleanup))) int CONCAT(_d_, __COUNTER__) = 0; \
    // Requiere que _defer_cleanup sea genérico...
```

En la práctica, `__attribute__((cleanup))` se usa con funciones específicas:
```c
static void close_file(FILE **fp) { if (*fp) fclose(*fp); }
#define AUTOCLOSE __attribute__((cleanup(close_file)))

AUTOCLOSE FILE *f = fopen("data.txt", "r");
// f se cierra automáticamente al salir del scope
```

`__COUNTER__` genera un entero único por cada expansión (0, 1, 2...), evitando colisiones de nombres.

</details>
