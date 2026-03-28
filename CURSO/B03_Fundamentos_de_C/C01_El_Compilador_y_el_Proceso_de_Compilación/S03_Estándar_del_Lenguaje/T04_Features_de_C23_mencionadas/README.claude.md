# T04 — Features de C23 mencionadas

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| README.md:59–61 | El macro `PRINT` de ejemplo está funcionalmente roto. La expansión `printf("x=%d ", 42, "done\n")` NO imprime `"done\n"` — es un argumento extra ignorado por `printf`. Las cadenas no se concatenan porque no son literales adyacentes (hay `42` entre ellas). | Se omite este ejemplo del material consolidado. Los otros ejemplos de `__VA_OPT__` son correctos y suficientes. |

---

## 1. Rol de C23 en este curso

El curso usa **C11** (`-std=c11`). Sin embargo, C23 introduce features que vale la pena conocer:

1. Ya están disponibles como extensiones de GCC/Clang
2. Se adoptan rápidamente en proyectos nuevos
3. Algunas estandarizan lo que antes eran extensiones GNU

**Reglas del curso con C23**:
- El código del curso compila con `-std=c11`
- Features de C23 se muestran como alternativa cuando simplifican algo significativamente
- Nunca se requiere C23 para completar ejercicios

---

## 2. `__VA_OPT__` — Macros variádicas sin coma extra

### El problema

```c
// Macro variádica clásica:
#define LOG(fmt, ...) printf("[LOG] " fmt "\n", __VA_ARGS__)

LOG("x=%d y=%d", x, y);   // OK: printf("[LOG] x=%d y=%d\n", x, y)
LOG("hello");               // ERROR: printf("[LOG] hello\n", )
//                                                           ^ coma colgante
```

La solución pre-C23 era una extensión GNU:

```c
#define LOG(fmt, ...) printf("[LOG] " fmt "\n", ##__VA_ARGS__)
// ## elimina la coma si __VA_ARGS__ está vacío — funciona, pero no es estándar
```

### La solución de C23

```c
// __VA_OPT__(contenido) se expande a "contenido" SOLO si hay argumentos
// variádicos. Si no hay, desaparece completamente:

#define LOG(fmt, ...) printf("[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

LOG("x=%d y=%d", x, y);   // printf("[LOG] x=%d y=%d\n", x, y)
LOG("hello");               // printf("[LOG] hello\n")
//                           __VA_OPT__(,) desaparece → sin coma colgante
```

`__VA_OPT__` puede contener cualquier tokens, no solo coma:

```c
#define CALL(func, ...) func(__VA_OPT__(__VA_ARGS__))
CALL(foo, 1, 2, 3);    // foo(1, 2, 3)
CALL(bar);              // bar()
```

Ejemplo práctico — debug logging con archivo y línea:

```c
#define DEBUG(fmt, ...) \
    fprintf(stderr, "[%s:%d] " fmt "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

DEBUG("starting");                // [main.c:10] starting
DEBUG("value: %d", x);           // [main.c:11] value: 42
DEBUG("a=%d b=%d", a, b);        // [main.c:12] a=1 b=2
```

### Disponibilidad

```bash
# GCC: soportado desde GCC 8 (incluso sin -std=c23)
# Clang: soportado desde Clang 12

# Como extensión en modo C11:
gcc -std=gnu11 main.c -o main   # funciona
gcc -std=c11 -Wpedantic main.c  # warning: no es C11 estándar

# Como estándar en C23:
gcc -std=c23 main.c -o main     # estándar, sin warnings
```

---

## 3. `constexpr` — Constantes verdaderas en compilación

### El problema con `const`

```c
// const en C NO crea una "constant expression":
const int SIZE = 10;

// A nivel de archivo (file scope):
int arr[SIZE];           // ERROR — SIZE no es constant expression, y VLAs
                         // no se permiten en file scope

// Dentro de una función (block scope):
void f(void) {
    int arr[SIZE];       // Compila, pero es un VLA (variable-length array),
}                        // NO un array de tamaño fijo

// En case labels:
switch (x) {
    case SIZE: break;    // ERROR — SIZE no es constant expression
}
```

En C11, las alternativas para constantes de compilación son:

```c
// 1. #define (sin tipo, sin scope — sustitución de texto):
#define SIZE 10

// 2. enum (solo enteros):
enum { SIZE = 10 };

// Ambos son constant expressions válidas:
int arr[SIZE];           // OK en file scope
case SIZE: break;        // OK en switch
```

### La solución de C23

```c
constexpr int SIZE = 10;
int arr[SIZE];                  // OK — array de tamaño fijo (no VLA)

constexpr double PI = 3.14159265358979;
constexpr int MAX_THREADS = 8;

// constexpr REQUIERE valor conocido en compilación:
int runtime_val = get_value();
constexpr int c = runtime_val;  // ERROR — no es constante
```

```c
// Diferencia clave const vs constexpr:
const int a = 10;
constexpr int b = 10;

int arr1[a];     // VLA (block scope) o ERROR (file scope)
int arr2[b];     // Array de tamaño fijo — OK en cualquier scope

switch (x) {
    case a: break;   // ERROR — no es constant expression
    case b: break;   // OK
}
```

### Restricciones de `constexpr` en C (vs C++)

```c
// C23 constexpr es MÁS LIMITADO que en C++:
// - Solo para variables (escalares, arrays, structs con inicializadores constantes)
// - El inicializador debe ser evaluable en compilación
// - NO hay funciones constexpr (a diferencia de C++)

constexpr int X = 5;            // OK
constexpr int Y = X + 3;        // OK — expresión constante
constexpr int arr[] = {1,2,3};  // OK — array

// NO válido en C23 (sí en C++):
// constexpr int square(int x) { return x * x; }
```

### Disponibilidad

```bash
# GCC 13+, Clang 17+ (con -std=c23)
gcc -std=c23 main.c -o main

# No hay extensión GNU equivalente — es puramente C23
# Alternativas en C11: #define o enum
```

---

## 4. `typeof` — Deducir el tipo de una expresión

### El problema

```c
int x = 42;
int y = x * 2;         // tienes que escribir "int" explícitamente

// Si cambias x a long, hay que cambiar y también.
// En macros genéricas, no sabes qué tipo tiene el argumento.
```

### `typeof` en C23

```c
int x = 42;
typeof(x) y = x * 2;           // y es int

double arr[10];
typeof(arr[0]) sum = 0.0;      // sum es double

// typeof NO evalúa la expresión — solo determina el tipo:
typeof(printf("hello")) ret;    // ret es int (tipo de retorno de printf)
                                 // "hello" NO se imprime
```

### `typeof_unqual` — tipo sin calificadores

```c
const int x = 42;
typeof(x) y = 10;              // y es const int → no se puede modificar
typeof_unqual(x) z = 10;       // z es int → modificable
z = 20;                         // OK
```

### Uso en macros type-safe

```c
// Macro MAX sin typeof — evalúa argumentos dos veces:
#define MAX_UNSAFE(a, b) ((a) > (b) ? (a) : (b))
int x = MAX_UNSAFE(i++, j++);    // BUG: i++ o j++ se ejecuta dos veces

// Macro MAX con typeof — segura:
#define MAX(a, b) ({            \
    typeof(a) _a = (a);         \
    typeof(b) _b = (b);         \
    _a > _b ? _a : _b;          \
})
int x = MAX(i++, j++);    // cada uno se evalúa exactamente una vez
// Nota: usa statement expressions (extensión GNU) + typeof (extensión GNU / C23)
```

```c
// SWAP genérico:
#define SWAP(a, b) do {         \
    typeof(a) _tmp = (a);       \
    (a) = (b);                   \
    (b) = _tmp;                  \
} while (0)

int x = 5, y = 10;
SWAP(x, y);              // funciona con cualquier tipo

double a = 3.14, b = 2.71;
SWAP(a, b);              // sin cambiar la macro
```

### Disponibilidad

```bash
# Como extensión GNU: GCC y Clang siempre lo soportaron
gcc -std=gnu11 main.c -o main   # typeof disponible

# Como estándar C23:
gcc -std=c23 main.c -o main     # typeof es estándar

# Con -std=c11 -Wpedantic:
# warning: ISO C does not support 'typeof'

# Alternativa en C11 con GCC/Clang:
# __typeof__(x) — variante con __ prefix, siempre disponible sin warning
```

---

## 5. Otras features de C23 notables

| Feature | Ejemplo | Antes de C23 |
|---------|---------|-------------|
| `nullptr` | `int *p = nullptr;` | `NULL`, `(void *)0`, `0` |
| `bool` keyword | `bool flag = true;` | `#include <stdbool.h>` |
| Atributos `[[]]` | `[[nodiscard]] int f();` | `__attribute__((warn_unused_result))` |
| Binary literals | `int mask = 0b11110000;` | Extensión GNU o hex (`0xF0`) |
| Digit separators | `int m = 1'000'000;` | Sin alternativa |
| `#embed` | `const unsigned char d[] = { #embed "file.bin" };` | `xxd -i file.bin` |
| `static_assert` sin msg | `static_assert(sizeof(int) == 4);` | Requería mensaje string |
| `#elifdef` | `#elifdef B` | `#elif defined(B)` |
| `auto` deducción | `auto x = 42;` | Declarar tipo explícitamente |

Eliminaciones en C23:
- **K&R function definitions** eliminadas (ya obsoletas desde C89)
- **Trigraphs** eliminados

---

## 6. Tabla resumen de disponibilidad

| Feature | Estándar | Extensión GNU | GCC desde | Clang desde |
|---------|----------|---------------|-----------|-------------|
| `__VA_OPT__` | C23 | Sí | GCC 8 | Clang 12 |
| `constexpr` | C23 | No | GCC 13 | Clang 17 |
| `typeof` | C23 | Sí (`__typeof__`) | Siempre | Siempre |
| `typeof_unqual` | C23 | No | GCC 13 | Clang 16 |
| `nullptr` | C23 | No | GCC 13 | Clang 17 |
| `bool` keyword | C23 | No | GCC 13 | Clang 15 |
| `[[atributos]]` | C23 | `__attribute__` | GCC 11 | Clang 13 |
| Binary literals | C23 | Sí | GCC 4.3 | Clang 3.0 |
| Digit separators | C23 | No | GCC 12 | Clang 16 |

---

## 7. Política del curso con C23

```
Cuando en el curso aparezca una feature de C23, se indicará:

    "typeof es una extensión GNU estandarizada en C23.
     En este curso (C11): usar __typeof__ con -std=gnu11
     o un approach alternativo."

Reglas:
1. El código del curso compila con -std=c11
2. Features de C23 se muestran como alternativa cuando simplifican algo
3. Nunca se requiere C23 para completar los ejercicios
```

---

## Ejercicios

### Ejercicio 1 — `__VA_OPT__` básico

```c
// va_opt_basic.c
#include <stdio.h>

#define LOG(fmt, ...) \
    printf("[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

int main(void) {
    LOG("server starting");
    LOG("listening on port %d", 8080);
    LOG("connection from %s:%d", "192.168.1.1", 4567);
    return 0;
}
```

```bash
# Compilar como extensión GNU:
gcc -std=gnu11 va_opt_basic.c -o va_opt_basic && ./va_opt_basic

# Intentar con C11 estricto + pedantic:
gcc -std=c11 -Wpedantic va_opt_basic.c -o va_opt_basic 2>&1

rm -f va_opt_basic
```

**Predicción**: ¿compila con `-std=gnu11`? ¿Qué warning da con `-std=c11 -Wpedantic`?

<details><summary>Respuesta</summary>

Con `-std=gnu11`: compila y funciona:

```
[LOG] server starting
[LOG] listening on port 8080
[LOG] connection from 192.168.1.1:4567
```

Con `-std=c11 -Wpedantic`: produce un warning:

```
warning: __VA_OPT__ is not available until C23
```

El programa sigue compilando (es un warning, no error), pero indica que `__VA_OPT__` no es C11 estándar.

</details>

### Ejercicio 2 — `__VA_OPT__` vs `##__VA_ARGS__`

Compara las dos formas de resolver la coma colgante:

```c
// va_opt_compare.c
#include <stdio.h>

// Forma GNU (extensión, pre-C23):
#define LOG_GNU(fmt, ...) printf("[GNU] " fmt "\n", ##__VA_ARGS__)

// Forma C23:
#define LOG_C23(fmt, ...) printf("[C23] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

int main(void) {
    // Con argumentos:
    LOG_GNU("x=%d", 42);
    LOG_C23("x=%d", 42);

    // Sin argumentos:
    LOG_GNU("hello");
    LOG_C23("hello");

    return 0;
}
```

```bash
gcc -std=gnu11 va_opt_compare.c -o va_opt_compare && ./va_opt_compare
rm -f va_opt_compare
```

**Predicción**: ¿producen la misma salida? ¿Cuál es más portable?

<details><summary>Respuesta</summary>

Producen exactamente la **misma salida**:

```
[GNU] x=42
[C23] x=42
[GNU] hello
[C23] hello
```

Ambas resuelven el problema de la coma colgante. La diferencia:
- `##__VA_ARGS__` es extensión GNU — funciona en GCC y Clang, pero no es estándar
- `__VA_OPT__` es C23 estándar — la opción correcta para código portable hacia el futuro

En C11, ninguna de las dos es estándar. `##__VA_ARGS__` tiene la ventaja de ser una extensión más antigua y ampliamente soportada.

</details>

### Ejercicio 3 — `constexpr` vs `const` vs `#define`

```c
// const_compare.c
#include <stdio.h>

// Tres formas de "constante":
#define DEF_SIZE 10
enum { ENUM_SIZE = 10 };
const int CONST_SIZE = 10;

// Usarlas en contextos que requieren constant expressions:
int arr_def[DEF_SIZE];         // ¿compila?
int arr_enum[ENUM_SIZE];       // ¿compila?
// int arr_const[CONST_SIZE];  // ¿compila en file scope?

int main(void) {
    int arr_local[CONST_SIZE]; // ¿compila en block scope?

    printf("sizeof(arr_def)   = %zu (%zu elementos)\n",
           sizeof(arr_def), sizeof(arr_def) / sizeof(arr_def[0]));
    printf("sizeof(arr_enum)  = %zu (%zu elementos)\n",
           sizeof(arr_enum), sizeof(arr_enum) / sizeof(arr_enum[0]));
    printf("sizeof(arr_local) = %zu (%zu elementos)\n",
           sizeof(arr_local), sizeof(arr_local) / sizeof(arr_local[0]));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_compare.c -o const_compare && ./const_compare
rm -f const_compare
```

**Predicción**: ¿`arr_local` (con `const int` en block scope) compila? Si sí, ¿es un array de tamaño fijo o un VLA?

<details><summary>Respuesta</summary>

Compila, pero `arr_local` es un **VLA** (variable-length array), no un array de tamaño fijo. `CONST_SIZE` no es una constant expression en C11 — es una variable de solo lectura. En block scope, C11 permite VLAs (aunque son opcionales).

```
sizeof(arr_def)   = 40 (10 elementos)
sizeof(arr_enum)  = 40 (10 elementos)
sizeof(arr_local) = 40 (10 elementos)
```

Los tamaños son iguales, pero el tipo subyacente es diferente: `arr_def` y `arr_enum` son arrays de tamaño fijo (constant expression), `arr_local` es un VLA que se evalúa en runtime.

Si descomentas `int arr_const[CONST_SIZE];` a nivel de file scope, GCC da error: VLAs no se permiten en file scope.

En C23, `constexpr int SIZE = 10;` resolvería esto: sería una constant expression verdadera usable en cualquier contexto.

</details>

### Ejercicio 4 — `typeof` en macros

```c
// typeof_macros.c
#include <stdio.h>

// SWAP genérico usando typeof (extensión GNU):
#define SWAP(a, b) do {         \
    __typeof__(a) _tmp = (a);   \
    (a) = (b);                   \
    (b) = _tmp;                  \
} while (0)

int main(void) {
    int x = 10, y = 20;
    printf("Antes: x=%d, y=%d\n", x, y);
    SWAP(x, y);
    printf("Después: x=%d, y=%d\n", x, y);

    double a = 3.14, b = 2.71;
    printf("Antes: a=%.2f, b=%.2f\n", a, b);
    SWAP(a, b);
    printf("Después: a=%.2f, b=%.2f\n", a, b);

    // ¿Funciona con punteros?
    const char *s1 = "hello", *s2 = "world";
    printf("Antes: s1=%s, s2=%s\n", s1, s2);
    SWAP(s1, s2);
    printf("Después: s1=%s, s2=%s\n", s1, s2);

    return 0;
}
```

```bash
# Con __typeof__ (funciona en C11, sin warning):
gcc -std=c11 -Wall -Wextra -Wpedantic typeof_macros.c -o typeof_macros && ./typeof_macros

rm -f typeof_macros
```

**Predicción**: ¿`SWAP` funciona con los tres tipos (int, double, punteros)? ¿`__typeof__` produce warning con `-Wpedantic`?

<details><summary>Respuesta</summary>

Funciona con los tres tipos:

```
Antes: x=10, y=20
Después: x=20, y=10
Antes: a=3.14, b=2.71
Después: a=2.71, b=3.14
Antes: s1=hello, s2=world
Después: s1=world, s2=hello
```

`__typeof__` (con doble underscore) **no produce warning** con `-Wpedantic`. Los identificadores con prefijo `__` están reservados para la implementación, así que el compilador los acepta sin avisar. `typeof` (sin underscores) sí produce warning con `-std=c11 -Wpedantic`. Por eso, en C11 se usa `__typeof__` para evitar warnings.

</details>

### Ejercicio 5 — `typeof_unqual` vs `typeof`

```c
// typeof_qual.c
#include <stdio.h>

int main(void) {
    const int x = 42;

    // typeof preserva calificadores:
    typeof(x) a = 10;       // a es const int
    // a = 20;               // ERROR: a es const

    // typeof_unqual elimina calificadores:
    typeof_unqual(x) b = 10;  // b es int
    b = 20;                     // OK

    printf("a=%d, b=%d\n", a, b);
    return 0;
}
```

```bash
# Requiere C23:
gcc -std=c23 typeof_qual.c -o typeof_qual && ./typeof_qual

# Intentar con C11:
gcc -std=c11 -Wpedantic typeof_qual.c -o typeof_qual 2>&1

rm -f typeof_qual
```

**Predicción**: ¿compila con `-std=c23`? ¿Y con `-std=c11`? ¿Qué pasa si descomentas `a = 20`?

<details><summary>Respuesta</summary>

- Con `-std=c23`: compila sin problemas. `a=10, b=20`.
- Con `-std=c11`: error — `typeof_unqual` no existe ni como extensión GNU (es puramente C23). `typeof` sí funcionaría como extensión, pero `typeof_unqual` no.
- Si descomentas `a = 20`: error de compilación `assignment of read-only variable 'a'` porque `typeof(const int)` preserva el calificador `const`.

`typeof_unqual` resuelve un problema real: cuando quieres una variable temporal del mismo tipo base pero sin restricciones de `const`/`volatile` — típico en macros genéricas.

</details>

### Ejercicio 6 — `__VA_OPT__` para macro DEBUG condicional

```c
// debug_macro.c
#include <stdio.h>

#ifdef DEBUG
#define DBG(fmt, ...) \
    fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
#else
#define DBG(fmt, ...) ((void)0)
#endif

int main(void) {
    int x = 42;
    DBG("starting main");
    DBG("x = %d", x);
    DBG("x * 2 = %d, x * 3 = %d", x * 2, x * 3);
    printf("result: %d\n", x);
    return 0;
}
```

```bash
# Sin DEBUG (las macros desaparecen):
gcc -std=gnu11 debug_macro.c -o debug_off && ./debug_off
echo "---"

# Con DEBUG (las macros imprimen):
gcc -std=gnu11 -DDEBUG debug_macro.c -o debug_on && ./debug_on

rm -f debug_off debug_on
```

**Predicción**: ¿qué imprime cada versión? ¿`DBG("starting main")` (sin args extra) funciona?

<details><summary>Respuesta</summary>

Sin `-DDEBUG`:
```
result: 42
```

Con `-DDEBUG`:
```
[DEBUG debug_macro.c:12] starting main
[DEBUG debug_macro.c:13] x = 42
[DEBUG debug_macro.c:14] x * 2 = 84, x * 3 = 126
result: 42
```

Sí, `DBG("starting main")` funciona correctamente porque `__VA_OPT__(,)` desaparece cuando no hay argumentos variádicos, evitando la coma colgante. Este es el patrón estándar para macros de debug condicional.

</details>

### Ejercicio 7 — Alternativas C11 para código C23

Reescribe este código C23 para que compile en C11 estricto:

```c
// c23_original.c (NO compila con -std=c11)
#include <stdio.h>

constexpr int SIZE = 5;
typeof(SIZE) arr[SIZE];

int main(void) {
    for (auto i = 0; i < SIZE; i++) {
        arr[i] = i * i;
    }
    for (auto i = 0; i < SIZE; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }
    return 0;
}
```

```c
// c11_equivalent.c — tu versión C11:
#include <stdio.h>

// constexpr → ???
// typeof → ???
// auto → ???

int main(void) {
    // ...
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic c11_equivalent.c -o c11_eq && ./c11_eq
rm -f c11_eq
```

**Predicción**: ¿qué reemplaza a `constexpr`, `typeof` y `auto` en C11?

<details><summary>Respuesta</summary>

```c
// c11_equivalent.c
#include <stdio.h>

enum { SIZE = 5 };         // constexpr → enum (constant expression)
int arr[SIZE];             // typeof(SIZE) → int (tipo explícito)

int main(void) {
    for (int i = 0; i < SIZE; i++) {   // auto → int (tipo explícito)
        arr[i] = i * i;
    }
    for (int i = 0; i < SIZE; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }
    return 0;
}
```

Reemplazos:
- `constexpr int` → `enum { ... }` (para enteros) o `#define` (cualquier tipo)
- `typeof(x)` → escribir el tipo explícitamente
- `auto` → escribir el tipo explícitamente

`enum` es la mejor alternativa para constantes enteras en C11 porque es una verdadera constant expression con tipo y scope.

</details>

### Ejercicio 8 — `nullptr` vs `NULL` vs `0`

```c
// nullptr_compare.c
#include <stdio.h>
#include <stddef.h>

void take_int(int x)    { printf("int: %d\n", x); }
void take_ptr(int *p)   { printf("ptr: %p\n", (void *)p); }

// _Generic no puede distinguir NULL de 0 en C11:
#define show(x) _Generic((x), \
    int:   "int",              \
    int *: "int *",            \
    default: "other"           \
)

int main(void) {
    printf("NULL is: %s\n", show(NULL));
    printf("0 is: %s\n", show(0));

    // En C11, NULL puede ser ((void *)0) o 0 — depende de la implementación.
    // Esto afecta qué rama de _Generic se selecciona.

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic nullptr_compare.c -o nullptr_compare && ./nullptr_compare
rm -f nullptr_compare
```

**Predicción**: ¿`NULL` es detectado como `int` o como `int *` por `_Generic`?

<details><summary>Respuesta</summary>

Depende de la implementación. En glibc (Linux):
- `NULL` se define como `((void *)0)` → tipo `void *`, que no está en la lista del `_Generic` → cae en `default`
- `0` tiene tipo `int` → selecciona `"int"`

Salida típica:
```
NULL is: other
0 is: int
```

`NULL` no es `int *` — es `void *` o `int`, según la implementación. `_Generic` no convierte tipos implícitamente. En C23, `nullptr` tiene tipo `nullptr_t`, un tipo dedicado para punteros nulos que resuelve esta ambigüedad.

</details>

### Ejercicio 9 — Atributos C23 vs `__attribute__`

```c
// attributes.c
#include <stdio.h>
#include <stdlib.h>

// Forma GNU (C11):
__attribute__((noreturn))
void die_gnu(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

__attribute__((warn_unused_result))
int compute_gnu(int x) {
    return x * x + 1;
}

__attribute__((unused))
static void helper_gnu(void) {
    // función no usada — sin warning
}

int main(void) {
    int result = compute_gnu(5);
    printf("result = %d\n", result);

    // Descomentar para ver warning de warn_unused_result:
    // compute_gnu(10);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic attributes.c -o attributes && ./attributes
rm -f attributes
```

**Predicción**: ¿`__attribute__` produce warning con `-Wpedantic`? ¿Qué pasa si descomentas `compute_gnu(10);`?

<details><summary>Respuesta</summary>

`__attribute__` **no produce warning** con `-Wpedantic`. Aunque no es C estándar, los identificadores con `__` prefix están reservados para la implementación, así que el compilador los acepta sin avisar (igual que `__typeof__`).

```
result = 26
```

Si descomentas `compute_gnu(10);` (sin usar el resultado):
```
warning: ignoring return value of 'compute_gnu', declared with attribute 'warn_unused_result'
```

En C23, los equivalentes serían:
- `__attribute__((noreturn))` → `[[noreturn]]`
- `__attribute__((warn_unused_result))` → `[[nodiscard]]`
- `__attribute__((unused))` → `[[maybe_unused]]`

</details>

### Ejercicio 10 — Verificar soporte de C23 en tu GCC

```c
// c23_check.c
#include <stdio.h>

int main(void) {
    printf("=== Soporte de C23 ===\n");
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);

#if __STDC_VERSION__ >= 202311L
    printf("C23 disponible\n\n");

    // Probar constexpr:
    constexpr int N = 5;
    printf("constexpr N = %d\n", N);

    // Probar typeof:
    typeof(N) arr[N];
    for (typeof(N) i = 0; i < N; i++) arr[i] = i * i;
    printf("arr = {");
    for (int i = 0; i < N; i++) printf("%s%d", i ? ", " : "", arr[i]);
    printf("}\n");

    // Probar static_assert sin mensaje:
    static_assert(sizeof(int) >= 4);

    // Probar bool como keyword:
    bool flag = true;
    printf("bool flag = %s\n", flag ? "true" : "false");

    // Probar nullptr:
    int *p = nullptr;
    printf("nullptr: p = %p\n", (void *)p);

#else
    printf("C23 NO disponible con estos flags\n");
    printf("Usar -std=c23 para habilitar\n");
#endif

    return 0;
}
```

```bash
echo "=== Con -std=c11 ==="
gcc -std=c11 c23_check.c -o c23_check && ./c23_check
echo ""

echo "=== Con -std=c23 ==="
gcc -std=c23 c23_check.c -o c23_check && ./c23_check
echo ""

echo "=== Con default (sin -std) ==="
gcc c23_check.c -o c23_check && ./c23_check

rm -f c23_check
```

**Predicción**: ¿el default de GCC 15 (sin `-std`) activa C23? ¿Todas las features C23 funcionan?

<details><summary>Respuesta</summary>

```
=== Con -std=c11 ===
__STDC_VERSION__ = 201112L
C23 NO disponible con estos flags
Usar -std=c23 para habilitar

=== Con -std=c23 ===
__STDC_VERSION__ = 202311L
C23 disponible

constexpr N = 5
arr = {0, 1, 4, 9, 16}
bool flag = true
nullptr: p = (nil)

=== Con default (sin -std) ===
__STDC_VERSION__ = 202311L
C23 disponible

constexpr N = 5
arr = {0, 1, 4, 9, 16}
bool flag = true
nullptr: p = (nil)
```

Sí, el default de GCC 15 es `gnu23`, que incluye todas las features de C23 más extensiones GNU. `__STDC_VERSION__ = 202311L` lo confirma. Todas las features probadas (`constexpr`, `typeof`, `static_assert` sin mensaje, `bool` keyword, `nullptr`) funcionan tanto con `-std=c23` como con el default.

Pero recuerda: el curso usa `-std=c11` para enseñar C portable. Conocer C23 es útil, pero no se requiere.

</details>
