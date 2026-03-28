# T02 — Inline

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 169 | Dice que `SQUARE("hello")` "compila (sustitución de texto) → basura". En realidad `(("hello") * ("hello"))` es un **error de compilación**: el operador `*` requiere operandos aritméticos, y `char *` no lo es. | Un mejor ejemplo de falta de type-safety en macros sería `SQUARE(3.5f)` que compila y produce un `float`, mientras que `square(3.5f)` con parámetro `int` trunca a `3` y retorna `9` en vez de `12.25`. O bien `#define ABS(x) ((x) >= 0 ? (x) : -(x))` con `ABS("text")` que sí compila con warning pero produce basura. |

---

## 1. Qué es `inline`

`inline` es una **sugerencia** al compilador para que reemplace la llamada a una función por el cuerpo de esa función directamente en el punto de llamada (call site):

```c
// Sin inline — se genera una llamada:
//   push argumentos, call square, recuperar resultado, pop
int square(int x) { return x * x; }

// Con inline — el compilador PUEDE reemplazar por:
//   int result = 5 * 5;
//   Sin overhead de llamada (push/call/pop/ret)
inline int square(int x) { return x * x; }

int result = square(5);
```

La palabra clave es **sugerencia**. El compilador puede:

1. **Hacer inline sin que lo pidas** — con `-O2`, funciones pequeñas se expanden automáticamente.
2. **Ignorar tu `inline`** — si la función es grande, el compilador decide que no vale la pena.
3. **Hacer inline en unas llamadas y no en otras** — según el contexto de cada call site.

Con `-O0` (sin optimización), el compilador **ignora** `inline` completamente y genera `call` normales. Solo con optimización (`-O1` o superior) la sugerencia tiene efecto práctico.

---

## 2. Semántica de `inline` en C

Las reglas de `inline` en C (C99+) son más complicadas que en C++ y generan confusión frecuente. Hay tres variantes con semánticas distintas:

### `inline` (sin `static` ni `extern`)

```c
// En un .c:
inline int square(int x) { return x * x; }
```

Esto crea una **definición inline** que NO genera una versión externa de la función. Si el compilador decide no expandir la función inline, el enlazador buscará una definición externa — y no la encontrará:

```
undefined reference to `square'
```

Para que funcione, necesitas proporcionar una **instanciación externa** en exactamente un `.c`:

```c
// utils.h — definición inline (visible para todos):
inline int square(int x) { return x * x; }

// utils.c — instanciación externa:
#include "utils.h"
extern inline int square(int x);
// Sin cuerpo: le dice al compilador "genera el código aquí"
```

Esto es correcto pero innecesariamente complicado.

### `static inline` — La forma idiomática

```c
static inline int max(int a, int b) {
    return a > b ? a : b;
}
```

- **`static`**: la función tiene enlace interno (local al translation unit).
- **`inline`**: sugerencia de expansión.

Si el compilador expande inline → no queda código de la función.
Si no expande → genera una copia local (no visible para otros archivos).

**No hay problemas de enlace nunca.** Esta es la convención del kernel de Linux y de la mayoría de proyectos C.

Se define en el **header**, no en un `.c`:

```c
// math_utils.h
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

static inline int min(int a, int b) {
    return a < b ? a : b;
}

static inline int clamp(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

#endif
```

El compilador necesita **ver el cuerpo** de la función para poder expandirla. Si solo ve un prototipo, no puede hacer inline. Por eso la definición completa va en el header.

### `extern inline`

```c
// En un .h:
inline int square(int x) { return x * x; }

// En exactamente un .c:
extern inline int square(int x);
```

La línea `extern inline` (sin cuerpo) fuerza la emisión de una definición externa en ese translation unit. Otros archivos que no hagan inline usarán esa copia.

En la práctica, `static inline` es más simple y cubre el 99% de los casos. `extern inline` solo se justifica si necesitas una **única copia** de la función en el binario (por ejemplo, si tomas su dirección en múltiples translation units y quieres que todos los punteros sean iguales).

---

## 3. `static inline` en headers: múltiples translation units

Cuando dos archivos `.c` incluyen el mismo header con funciones `static inline`:

```c
// math_inline.h
static inline int min(int a, int b) { return a < b ? a : b; }

// caller_a.c
#include "math_inline.h"
void test_a(void) { printf("%d\n", min(3, 7)); }

// caller_b.c
#include "math_inline.h"
void test_b(void) { printf("%d\n", min(5, 2)); }
```

**No hay error de "multiple definition"** al enlazar. `static` hace que cada translation unit tenga su propia copia local, invisible para los demás.

Se puede verificar con `nm`:

```bash
gcc -c -O0 caller_a.c caller_b.c

nm caller_a.o
# ... t min       ← 't' minúscula = símbolo local
nm caller_b.o
# ... t min       ← otra copia local, sin conflicto
```

Con `-O2`, los símbolos `min` desaparecen completamente — el compilador expandió inline y eliminó las copias sobrantes:

```bash
gcc -c -O2 caller_a.c caller_b.c

nm caller_a.o
# Solo test_from_a y printf — min ya no existe
```

Este es el comportamiento ideal: cero overhead en el binario final.

---

## 4. `inline` vs macros

Históricamente, las macros de preprocesador (`#define`) se usaban para lograr expansión inline. Las funciones `inline` son superiores en casi todos los aspectos:

### Type-safety

```c
#define DOUBLE(x)  ((x) * 2)
static inline int double_it(int x) { return x * 2; }

DOUBLE(3.7f);       // Compila: produce float 7.4
double_it(3.7f);    // Compila: trunca a 3, retorna 6
                    // El tipo int del parámetro hace la conversión explícita
```

Con macros, no hay verificación de tipos — la sustitución textual acepta cualquier cosa que compile sintácticamente con los operadores usados.

### Evaluación única

```c
#define SQUARE(x) ((x) * (x))
static inline int square(int x) { return x * x; }

int i = 5;
SQUARE(i++);    // ((i++) * (i++)) → i se incrementa DOS veces → UB
square(i++);    // i++ se evalúa UNA vez (5), se pasa la copia → retorna 25
```

Este es el peligro más grave de las macros: los argumentos con efectos secundarios se evalúan múltiples veces.

### Scope y depuración

- Las macros no tienen scope — contaminan el namespace global y pueden causar conflictos inesperados.
- Las funciones inline tienen scope normal y aparecen en el debugger (si no fueron expandidas).

### Cuándo siguen siendo necesarias las macros

```c
// 1. Genericidad de tipos (antes de C11 _Generic):
#define MAX(a, b) ((a) > (b) ? (a) : (b))  // Funciona con int, float, etc.

// 2. Stringify y concatenación:
#define STRINGIFY(x) #x
#define CONCAT(a, b) a##b

// 3. Compilación condicional:
#ifdef DEBUG
#define LOG(msg) fprintf(stderr, "%s\n", msg)
#else
#define LOG(msg) ((void)0)
#endif
```

---

## 5. El compilador como optimizador

### Inlining automático con `-O2`

Con `-O2`, el compilador hace inline automáticamente sin necesidad de la palabra clave `inline`:

```c
// Sin "inline" explícito:
int tiny_add(int a, int b) { return a + b; }

int medium_work(int x) {
    int sum = 0;
    for (int i = 0; i < x; i++) {
        sum += i * i;
        if (sum > 1000) sum = sum % 100;
    }
    return sum;
}
```

Con `-O2 -S`:
- `tiny_add` desaparece de las llamadas (inlined automáticamente).
- `medium_work` conserva su `call` — demasiado grande para expandir.

El criterio del compilador considera: tamaño de la función, frecuencia de llamada, presión de registros, y efecto en el instruction cache.

### Atributos GCC para controlar inlining

```c
// Forzar inline — error de compilación si no puede:
__attribute__((always_inline))
static inline int critical_op(int x) { return x * 2 + 1; }

// Prohibir inline — útil para stack traces legibles:
__attribute__((noinline))
int debug_function(int x) { return x; }
```

`always_inline` solo se justifica en paths extremadamente críticos (kernel, ISRs). En código normal, confía en el compilador.

### Link-Time Optimization (LTO)

Sin LTO, el compilador solo puede hacer inline dentro de un mismo translation unit (un `.c` con sus `#include`). Con LTO, puede hacer inline **entre archivos**:

```bash
gcc -flto -O2 file1.c file2.c -o program
```

LTO retrasa parte de la optimización hasta el enlace, cuando el compilador ve todo el programa junto. Esto permite inlining cross-file, eliminación de código muerto global, y mejor propagación de constantes.

---

## 6. `static inline` vs símbolo regular: efecto en el binario

La diferencia clave entre una función regular y `static inline` se manifiesta cuando el compilador hace inline con `-O2`:

| Aspecto | Función regular | `static inline` |
|---------|----------------|------------------|
| Tras inline con `-O2` | La etiqueta y el código permanecen (enlace externo) | Desaparece completamente si todas las llamadas fueron inlined |
| Símbolo en `nm` | `T square` (global) | No aparece |
| Tamaño del assembly | Mayor (conserva copia "por si acaso") | Menor (eliminada por completo) |
| Uso entre archivos | Otros `.c` pueden enlazar con ella | Solo visible en su translation unit |

Esta es la razón por la que `static inline` en headers es la forma correcta: si el compilador inlinea todo, no queda residuo en el binario.

---

## 7. Comparación con Rust

Rust tiene un atributo `#[inline]` con semántica diferente:

```rust
// Sugerencia (equivalente a inline en C):
#[inline]
fn square(x: i32) -> i32 { x * x }

// Forzar (equivalente a always_inline):
#[inline(always)]
fn critical(x: i32) -> i32 { x * 2 }

// Prohibir (equivalente a noinline):
#[inline(never)]
fn debug_fn(x: i32) -> i32 { x }
```

Diferencias clave:

- En Rust, `#[inline]` es necesario para permitir inlining **entre crates** (equivalente a inline entre librerías). Sin `#[inline]`, el compilador solo inlinea dentro del mismo crate.
- Rust no tiene el problema de `static inline` vs `extern inline` — el sistema de módulos maneja la visibilidad automáticamente.
- Las macros de Rust (`macro_rules!`) son higiénicas y operan sobre el AST, no sobre texto, por lo que no tienen los problemas de evaluación múltiple de las macros de C.

---

## Ejercicios

### Ejercicio 1 — `static inline` en header: min/max/abs

Crea un header `utils_inline.h` con tres funciones `static inline`:
- `int min_of(int a, int b)` — retorna el menor
- `int max_of(int a, int b)` — retorna el mayor
- `int abs_of(int x)` — retorna el valor absoluto

Incluye el header desde `main.c` y prueba con valores positivos, negativos y cero.

```c
// utils_inline.h
#ifndef UTILS_INLINE_H
#define UTILS_INLINE_H

static inline int min_of(int a, int b) {
    return a < b ? a : b;
}

static inline int max_of(int a, int b) {
    return a > b ? a : b;
}

static inline int abs_of(int x) {
    return x >= 0 ? x : -x;
}

#endif
```

```c
// main.c
#include <stdio.h>
#include "utils_inline.h"

int main(void) {
    printf("min_of(3, 7) = %d\n", min_of(3, 7));
    printf("max_of(3, 7) = %d\n", max_of(3, 7));
    printf("abs_of(-5)   = %d\n", abs_of(-5));
    printf("abs_of(0)    = %d\n", abs_of(0));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -o ex1
./ex1
```

<details><summary>Predicción</summary>

```
min_of(3, 7) = 3
max_of(3, 7) = 7
abs_of(-5)   = 5
abs_of(0)    = 0
```

Las funciones `static inline` definidas en el header se incluyen en el translation unit de `main.c`. Funcionan como funciones normales. El compilador puede expandirlas o no dependiendo del nivel de optimización — el resultado es el mismo en ambos casos.

</details>

---

### Ejercicio 2 — Doble evaluación: macro vs inline

Demuestra el peligro de la doble evaluación en macros comparando `SQUARE(x)` (macro) con `square(x)` (inline):

```c
#include <stdio.h>

#define SQUARE(x) ((x) * (x))

static inline int square(int x) {
    return x * x;
}

int main(void) {
    int a = 3;
    int b = 3;

    int macro_result = SQUARE(a++);
    int func_result  = square(b++);

    printf("SQUARE(a++): result = %d, a = %d\n", macro_result, a);
    printf("square(b++): result = %d, b = %d\n", func_result, b);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
./ex2
```

<details><summary>Predicción</summary>

`SQUARE(a++)` se expande a `((a++) * (a++))`: `a` se incrementa **dos veces** y el resultado depende del orden de evaluación (comportamiento indefinido en C — `a` se modifica dos veces sin sequence point). En la práctica con GCC, típicamente produce `a = 5` y `result = 9` o `12`, pero **cualquier resultado es válido** porque es UB.

`square(b++)` evalúa `b++` **una vez** (pasa `3` como copia), luego incrementa `b`. Resultado definido: `result = 9`, `b = 4`.

```
SQUARE(a++): result = 9, a = 5     (o cualquier valor — UB)
square(b++): result = 9, b = 4     (siempre determinista)
```

Moraleja: las funciones inline evalúan sus argumentos exactamente una vez, como cualquier función. Las macros son sustitución textual.

</details>

---

### Ejercicio 3 — Assembly con `-O0` vs `-O2`

Compara el assembly generado para una función pequeña con y sin optimización:

```c
// tiny.c
#include <stdio.h>

static inline int double_it(int x) {
    return x * 2;
}

int main(void) {
    int result = double_it(21);
    printf("double_it(21) = %d\n", result);
    return 0;
}
```

```bash
gcc -std=c11 -S -O0 tiny.c -o tiny_O0.s
gcc -std=c11 -S -O2 tiny.c -o tiny_O2.s

echo "=== -O0: calls ==="
grep "call" tiny_O0.s

echo "=== -O2: calls ==="
grep "call" tiny_O2.s

echo "=== -O0: double_it label ==="
grep "double_it" tiny_O0.s

echo "=== -O2: double_it label ==="
grep "double_it" tiny_O2.s
```

<details><summary>Predicción</summary>

Con `-O0`:
```
call    double_it     ← Se genera la llamada
call    printf
```
La etiqueta `double_it:` aparece en el assembly. El compilador ignora `inline` sin optimización.

Con `-O2`:
```
call    printf        ← Solo printf, double_it desapareció
```
No hay etiqueta `double_it` en el assembly. El compilador:
1. Expandió `double_it(21)` como `21 * 2 = 42` (constant folding).
2. Como es `static` y todas las llamadas fueron inlined, eliminó la función por completo.

Es probable que con `-O2` ni siquiera aparezca la multiplicación: el compilador calcula `42` en tiempo de compilación y lo pasa directamente a `printf`.

</details>

---

### Ejercicio 4 — Símbolos con `nm`: `static inline` vs regular

Compara los símbolos generados por una función regular y una `static inline`:

```c
// regular.c
int compute(int x) { return x * x + 1; }
int main(void) { return compute(5); }

// sinline.c
static inline int compute(int x) { return x * x + 1; }
int main(void) { return compute(5); }
```

```bash
gcc -std=c11 -c -O0 regular.c -o regular.o
gcc -std=c11 -c -O0 sinline.c -o sinline.o

echo "=== regular.o (-O0) ==="
nm regular.o

echo "=== sinline.o (-O0) ==="
nm sinline.o

gcc -std=c11 -c -O2 regular.c -o regular_O2.o
gcc -std=c11 -c -O2 sinline.c -o sinline_O2.o

echo "=== regular.o (-O2) ==="
nm regular_O2.o

echo "=== sinline.o (-O2) ==="
nm sinline_O2.o
```

<details><summary>Predicción</summary>

Con `-O0`:
```
=== regular.o ===
... T compute        ← 'T' mayúscula = símbolo global
... T main

=== sinline.o ===
... t compute        ← 't' minúscula = símbolo local (por static)
... T main
```

Con `-O2`:
```
=== regular.o ===
... T compute        ← Sigue presente (enlace externo, otros pueden necesitarla)
... T main

=== sinline.o ===
... T main           ← compute desapareció completamente
```

La función regular mantiene su símbolo global incluso tras ser inlined, porque otro translation unit podría referenciarla. La `static inline` desaparece porque `static` garantiza que nadie más la referencia, y al estar inlined, no queda ninguna llamada.

</details>

---

### Ejercicio 5 — `inline` sin `static`: el error de enlace

Demuestra el problema de usar `inline` solo (sin `static`) cuando el compilador no puede hacer inline:

```c
// problem.c
#include <stdio.h>

inline int triple(int x) {
    return x * 3;
}

int main(void) {
    printf("triple(7) = %d\n", triple(7));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 problem.c -o problem
```

<details><summary>Predicción</summary>

Con `-O0`, el compilador no hace inline. La definición `inline int triple(...)` no genera una versión externa. El compilador emite `call triple` pero no hay símbolo `triple` enlazable:

```
undefined reference to `triple'
```

Para corregirlo, hay tres opciones:

1. **`static inline`** (recomendada): `static inline int triple(int x) { return x * 3; }`
2. **Agregar instanciación externa**: añadir `extern inline int triple(int x);` en el mismo archivo.
3. **Quitar `inline`**: `int triple(int x) { return x * 3; }` — función normal.

Nota: algunos compiladores (como GCC con extensiones GNU por defecto, sin `-std=c11`) tratan `inline` con semántica GNU diferente y podrían no producir este error. Con `-std=c11` (C estándar), el error es consistente.

</details>

---

### Ejercicio 6 — `always_inline` y `noinline`

Usa atributos GCC para forzar y prohibir inlining:

```c
#include <stdio.h>

__attribute__((always_inline))
static inline int forced_inline(int x) {
    return x * x;
}

__attribute__((noinline))
int never_inline(int x) {
    return x + 1;
}

int main(void) {
    printf("forced: %d\n", forced_inline(10));
    printf("never:  %d\n", never_inline(10));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -S -O2 ex6.c -o ex6.s
grep "call" ex6.s
```

<details><summary>Predicción</summary>

```
call    never_inline
call    printf
call    printf
```

- `forced_inline` NO aparece en las llamadas — `always_inline` garantiza la expansión incluso si el compilador no lo haría normalmente.
- `never_inline` SÍ aparece como `call` — `noinline` prohíbe la expansión incluso para una función trivial que normalmente sería inlined con `-O2`.

`always_inline` se usa en código de kernel para funciones que **deben** expandirse (por razones de rendimiento o contexto de ejecución). `noinline` se usa para funciones que necesitan aparecer en stack traces o profilers.

</details>

---

### Ejercicio 7 — Función grande: el compilador dice no

Verifica que el compilador no inlinea funciones grandes ni con `inline` explícito:

```c
#include <stdio.h>

static inline int big_function(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        result += i * i;
        if (result > 10000) result = result % 1000;
        for (int j = 0; j < i; j++) {
            result += j;
            if (result > 5000) result /= 2;
        }
    }
    for (int k = n; k > 0; k--) {
        result ^= k;
        result += k * 3;
        if (result < 0) result = -result;
    }
    return result;
}

int main(void) {
    printf("big_function(50) = %d\n", big_function(50));
    return 0;
}
```

```bash
gcc -std=c11 -S -O2 ex7.c -o ex7.s
grep "call" ex7.s
```

<details><summary>Predicción</summary>

```
call    big_function     ← No fue inlined a pesar de "inline"
call    printf
```

Aunque `big_function` está marcada como `static inline`, el compilador con `-O2` decide **no expandirla** porque el cuerpo es demasiado grande. El costo de duplicar todo ese código (presión en instruction cache, aumento del tamaño del binario) supera el beneficio de eliminar el `call`.

Sin embargo, como es `static` y se llama una sola vez, **algunos compiladores podrían** inlinearla de todas formas (una sola llamada = no hay duplicación). El resultado depende de la heurística específica de la versión de GCC.

Si se usara `__attribute__((always_inline))`, el compilador estaría **obligado** a expandirla.

</details>

---

### Ejercicio 8 — Múltiples translation units con `static inline`

Crea un proyecto multi-archivo que use funciones `static inline` desde un header compartido:

```c
// string_utils.h
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

static inline size_t str_len(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static inline int str_equal(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

#endif
```

```c
// module_a.c
#include <stdio.h>
#include "string_utils.h"

void report_length(const char *s) {
    printf("Length of \"%s\" = %zu\n", s, str_len(s));
}
```

```c
// module_b.c
#include <stdio.h>
#include "string_utils.h"

void check_equal(const char *a, const char *b) {
    printf("\"%s\" == \"%s\"? %s\n", a, b,
           str_equal(a, b) ? "yes" : "no");
}
```

```c
// main_ex8.c
#include <stdio.h>

void report_length(const char *s);
void check_equal(const char *a, const char *b);

int main(void) {
    report_length("hello");
    report_length("");
    check_equal("abc", "abc");
    check_equal("abc", "xyz");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic module_a.c module_b.c main_ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
Length of "hello" = 5
Length of "" = 0
"abc" == "abc"? yes
"abc" == "xyz"? no
```

Ambos `module_a.c` y `module_b.c` incluyen `string_utils.h`. Cada uno obtiene su propia copia local de `str_len` y `str_equal` (por el `static`). No hay conflicto de enlace. Con `-O2`, esas copias se inlinearían y desaparecerían del binario.

Nota: `module_a.c` usa `str_len` pero no `str_equal`, y viceversa. Con `-O2`, el compilador **elimina las funciones no usadas** en cada translation unit — otra ventaja de `static inline`.

</details>

---

### Ejercicio 9 — Macro con efectos secundarios vs inline

Escribe un logging macro y su equivalente inline para mostrar un peligro real:

```c
#include <stdio.h>

int call_count = 0;

int get_value(void) {
    call_count++;
    return 42;
}

#define LOG_VALUE(val) printf("Value: %d, Double: %d\n", (val), (val))

static inline void log_value(int val) {
    printf("Value: %d, Double: %d\n", val, val);
}

int main(void) {
    call_count = 0;
    LOG_VALUE(get_value());
    printf("Calls after macro: %d\n\n", call_count);

    call_count = 0;
    log_value(get_value());
    printf("Calls after inline: %d\n", call_count);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
./ex9
```

<details><summary>Predicción</summary>

```
Value: 42, Double: 42
Calls after macro: 2

Value: 42, Double: 42
Calls after inline: 1
```

`LOG_VALUE(get_value())` se expande a `printf("...", (get_value()), (get_value()))` — `get_value()` se llama **dos veces**, incrementando `call_count` a 2.

`log_value(get_value())` evalúa `get_value()` **una vez**, pasa la copia `42`, e imprime el mismo valor dos veces. `call_count` es 1.

Ambos imprimen `42, 42` (los valores numéricos coinciden porque `get_value` siempre retorna 42), pero la macro ejecuta el efecto secundario (incremento del contador) dos veces. Con funciones que tienen efectos secundarios reales (I/O, asignación de memoria, cambio de estado), esto es un bug grave.

</details>

---

### Ejercicio 10 — Tabla de dispatch con `static inline` helpers

Combina punteros a función (tópico anterior) con `static inline` para crear una calculadora modular:

```c
#include <stdio.h>

static inline int safe_add(int a, int b) { return a + b; }
static inline int safe_sub(int a, int b) { return a - b; }
static inline int safe_mul(int a, int b) { return a * b; }
static inline int safe_div(int a, int b) { return b != 0 ? a / b : 0; }

typedef int (*BinaryOp)(int, int);

int main(void) {
    BinaryOp ops[] = { safe_add, safe_sub, safe_mul, safe_div };
    const char *names[] = { "add", "sub", "mul", "div" };

    int x = 20, y = 4;

    for (int i = 0; i < 4; i++) {
        printf("%s(%d, %d) = %d\n", names[i], x, y, ops[i](x, y));
    }

    // Test division by zero:
    printf("div(%d, 0) = %d\n", x, safe_div(x, 0));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
add(20, 4) = 24
sub(20, 4) = 16
mul(20, 4) = 80
div(20, 4) = 5
div(20, 0) = 0
```

Las funciones `static inline` se usan de dos formas:
1. **A través de punteros en `ops[]`**: aquí el compilador **no puede** hacer inline porque las funciones se llaman indirectamente (`ops[i](x, y)`). Necesita una dirección real, así que genera copias de las funciones.
2. **Directamente** (`safe_div(x, 0)`): aquí el compilador **sí puede** hacer inline con `-O2`.

Este es un detalle importante: tomar la dirección de una función `static inline` fuerza al compilador a generar una copia real, anulando parcialmente el beneficio de inline. El compilador es lo suficientemente inteligente para generar la copia solo cuando se necesita su dirección.

</details>
