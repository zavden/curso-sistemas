# T02 — Inline

## Qué es inline

`inline` es una **sugerencia** al compilador para que reemplace
la llamada a función por el cuerpo de la función directamente
en el caller:

```c
// Sin inline:
int square(int x) { return x * x; }

int result = square(5);
// Se genera una llamada: push 5, call square, pop resultado

// Con inline:
inline int square(int x) { return x * x; }

int result = square(5);
// El compilador PUEDE reemplazar por: int result = 5 * 5;
// Sin overhead de llamada (push, call, pop, return)
```

```c
// IMPORTANTE: inline es una SUGERENCIA, no una orden.
// El compilador puede:
// 1. Hacer inline aunque no lo pidas (con -O2)
// 2. NO hacer inline aunque lo pidas (si la función es grande)
// 3. Hacer inline en algunas llamadas y no en otras

// En la práctica, con -O2 el compilador decide mejor que
// el programador cuándo hacer inline.
```

## Semántica de inline en C

Las reglas de inline en C son más complicadas de lo que parecen
y diferentes a las de C++:

```c
// Definición inline en un .c:
inline int square(int x) { return x * x; }

// En C, esto NO genera una versión "externa" de la función.
// Si el compilador decide no hacer inline, el enlazador no
// encontrará square → undefined reference.

// Para garantizar que existe una versión externa:
// OPCIÓN 1: agregar una definición extern en exactamente un .c:
extern inline int square(int x);
// (sin cuerpo — le dice al compilador "genera el código aquí")

// OPCIÓN 2: definir como static inline (lo más común):
static inline int square(int x) { return x * x; }
```

### static inline — La forma correcta

```c
// static inline es la forma recomendada en C:
static inline int max(int a, int b) {
    return a > b ? a : b;
}

// static: la función es local al archivo (translation unit)
// inline: sugerencia de hacer inline

// Ventajas:
// - Funciona siempre (static genera una copia si no hace inline)
// - No hay problemas de enlace
// - Es la convención usada en el kernel de Linux y la mayoría
//   de proyectos C

// Se define en el HEADER, no en un .c:
// utils.h
#ifndef UTILS_H
#define UTILS_H

static inline int min(int a, int b) {
    return a < b ? a : b;
}

#endif
```

```c
// ¿Por qué en el header?
// Porque el compilador necesita ver el CUERPO de la función
// para poder hacer inline. Si solo ve un prototipo, no puede.
//
// Con static, cada .c que incluye el header tiene su propia
// copia de la función. Si el compilador hace inline, la copia
// no genera código extra. Si no hace inline, se genera una
// copia local por archivo (generalmente el enlazador las
// deduplica o el compilador las elimina si están inlined).
```

### extern inline — Para C estricto

```c
// Si quieres una sola copia visible externamente (no static):

// utils.h — definición inline:
inline int square(int x) { return x * x; }

// utils.c — instanciación externa:
#include "utils.h"
extern inline int square(int x);
// Esta línea genera el código de square en utils.o
// Si el compilador decide no hacer inline en algún caller,
// enlaza con esta versión

// En la práctica, esto es innecesariamente complicado.
// Usar static inline es más simple y funciona igual.
```

## Cuándo usar inline

```c
// USAR inline para funciones:
// - Muy pequeñas (1-5 líneas)
// - Llamadas frecuentemente
// - En paths críticos de rendimiento

static inline int abs_val(int x) {
    return x >= 0 ? x : -x;
}

static inline void swap_int(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline int clamp(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
```

```c
// NO usar inline para funciones:
// - Grandes (el código expandido puede ser peor para el cache)
// - Recursivas (no se puede expandir infinitamente)
// - Con loops complejos
// - Que raramente se llaman

// El compilador con -O2 hace inline automáticamente
// cuando es beneficioso. Agregar inline manualmente
// es generalmente innecesario excepto para funciones
// pequeñas en headers.
```

## inline vs macros

```c
// Históricamente, las macros se usaban donde hoy usaríamos inline:

// Macro:
#define SQUARE(x) ((x) * (x))

// inline:
static inline int square(int x) { return x * x; }

// La función inline es MEJOR:

// 1. Tipo seguro:
SQUARE("hello");        // compila (sustitución de texto) → basura
square("hello");        // ERROR de compilación

// 2. Evaluación única:
SQUARE(i++);            // ((i++) * (i++)) → evalúa i++ DOS veces → UB
square(i++);            // i++ se evalúa una vez, se pasa la copia

// 3. Scope correcto:
// Las macros no tienen scope — pueden causar conflictos de nombres
// Las funciones inline sí tienen scope

// 4. Depuración:
// Las macros no aparecen en el debugger (son sustitución de texto)
// Las funciones inline sí aparecen (si no fueron inlined)

// Usar macros solo cuando no puedes usar una función:
// - Macros que necesitan operar con tipos genéricos
// - Macros que usan # (stringify) o ## (concatenación)
// - Macros para compilación condicional
```

## El compilador es mejor que tú

```c
// Con -O2, GCC/Clang hacen inline automáticamente:
// - Funciones pequeñas
// - Funciones llamadas una sola vez
// - Funciones en el mismo translation unit
// - Funciones marcadas con __attribute__((always_inline))

// También pueden hacer inline entre archivos con LTO:
// gcc -flto file1.c file2.c -o program
// LTO (Link-Time Optimization) permite inline entre .c files

// __attribute__((always_inline)) — forzar inline:
__attribute__((always_inline))
static inline int critical_path(int x) {
    return x * 2 + 1;
}
// El compilador DEBE hacer inline (error si no puede)

// __attribute__((noinline)) — prohibir inline:
__attribute__((noinline))
int debug_function(int x) {
    return x;
}
// Útil para funciones que quieres ver en stack traces
```

---

## Ejercicios

### Ejercicio 1 — static inline en header

```c
// Crear un header math_inline.h con funciones static inline:
// - min(int, int)
// - max(int, int)
// - clamp(int, int lo, int hi)
// Incluir desde main.c y verificar que funcionan.
```

### Ejercicio 2 — inline vs macro

```c
// Crear una macro DOUBLE(x) y una función inline double_it(int x).
// Mostrar la diferencia con: DOUBLE(i++) vs double_it(i++)
// ¿Qué valor tiene i después de cada uno?
```

### Ejercicio 3 — Verificar inline con -S

```bash
# Compilar una función small y una función large con
# -O2 -S y verificar en el assembly:
# ¿La función small fue inlined? ¿Y la large?
```
