# Flags esenciales

> Sin erratas detectadas en el material base.

---

## Indice

1. [Por que importan los flags](#1-por-que-importan-los-flags)
2. [Flags de warnings](#2-flags-de-warnings)
3. [Seleccion de estandar (-std)](#3-seleccion-de-estandar--std)
4. [Flags de debug (-g)](#4-flags-de-debug--g)
5. [Flags de optimizacion (-O)](#5-flags-de-optimizacion--o)
6. [Otros flags utiles](#6-otros-flags-utiles)
7. [Sets recomendados](#7-sets-recomendados)
8. [Tabla de flags](#8-tabla-de-flags)
9. [Ejercicios](#9-ejercicios)

---

## 1. Por que importan los flags

GCC tiene cientos de flags. Un programa puede compilar perfectamente con `gcc main.c` pero tener bugs ocultos que los flags correctos habrian detectado:

```bash
# Minimal compilation (DON'T do this):
gcc main.c -o main

# Correct compilation for development:
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 main.c -o main

# Production compilation:
gcc -Wall -Wextra -std=c11 -O2 main.c -o main
```

---

## 2. Flags de warnings

### -Wall — Warnings principales

A pesar del nombre, `-Wall` **no** activa todos los warnings. Activa los mas comunes y generalmente utiles:

```c
// Warnings that -Wall detects:

// 1. Unused variable:
int x = 5;  // warning: unused variable 'x'

// 2. Uninitialized variable:
int y;
printf("%d\n", y);  // warning: 'y' is used uninitialized

// 3. Wrong printf format:
int n = 42;
printf("%s\n", n);  // warning: format '%s' expects 'char*', got 'int'

// 4. Signed/unsigned comparison:
unsigned int u = 10;
int s = -1;
if (s < u) { }  // warning: comparison of integer expressions
                 // of different signedness

// 5. Switch without default or missing cases
// 6. Implicit return in non-void function
// 7. Suggested parentheses for clarity
// ...and many more
```

### -Wextra — Warnings adicionales

`-Wextra` agrega warnings que `-Wall` no incluye:

```c
// Additional warnings with -Wextra:

// 1. Unused parameter:
int process(int x, int y) {  // warning: unused parameter 'y'
    return x * 2;
}

// 2. Always-true/false comparison:
unsigned int u = 5;
if (u >= 0) { }  // warning: comparison of unsigned >= 0 is always true

// 3. Empty body:
if (x);   // warning: empty body in 'if' statement
    y++;

// 4. Missing field initializer:
struct Point { int x; int y; int z; };
struct Point p = { 1, 2 };  // warning: missing initializer for field 'z'
```

### -Wpedantic — Conformidad estricta al estandar

`-Wpedantic` advierte sobre extensiones de GCC que no son parte del estandar C:

```c
// Warnings with -Wpedantic:

// 1. Zero-size arrays (GNU extension):
int arr[0];  // warning: ISO C forbids zero-size array

// 2. Statement expressions (GNU extension):
int x = ({ int y = 5; y * 2; });  // warning: ISO C forbids braced-groups

// 3. Binary literals (GNU extension, standardized in C23):
int b = 0b1010;  // warning: binary constants are a C23 feature or GCC extension

// 4. typeof (GNU extension before C23):
typeof(x) y;  // warning: ISO C does not support 'typeof'
```

### -Werror — Tratar warnings como errores

```bash
gcc -Wall -Wextra -Werror main.c -o main
# Compilation FAILS if there are any warnings
# Useful in CI/CD to enforce clean code

# Treat a specific warning as error:
gcc -Werror=return-type main.c -o main

# Disable a specific warning:
gcc -Wall -Wno-unused-variable main.c -o main
# Activates everything in -Wall EXCEPT unused-variable
```

---

## 3. Seleccion de estandar (-std)

```bash
# Specify which C standard to use:
gcc -std=c89 main.c      # C89/C90 (ANSI C)
gcc -std=c99 main.c      # C99
gcc -std=c11 main.c      # C11 (recommended for this course)
gcc -std=c17 main.c      # C17 (only corrections over C11)
gcc -std=c23 main.c      # C23 (most recent, partial support)
```

### -std=c11 vs -std=gnu11

```bash
# -std=c11: strict C11 standard
# -std=gnu11: C11 + GNU extensions (GCC's default behavior)

gcc main.c -o main           # uses -std=gnu17 by default (modern GCC)
gcc -std=c11 main.c -o main  # strict C11, no extensions
gcc -std=gnu11 main.c -o main # C11 with GNU extensions
```

Extensiones GNU que `-std=gnu11` permite pero `-std=c11` no:

```c
// 1. Nested functions:
void foo(void) {
    void bar(void) { }  // only with gnu, not in standard
}

// 2. Statement expressions:
int x = ({ int y = 5; y * 2; });  // GNU extension

// 3. typeof (before C23):
typeof(x) copy = x;  // GNU extension, standardized in C23

// 4. Attributes with __attribute__:
void die(void) __attribute__((noreturn));  // GNU extension
// Standard C11 equivalent: _Noreturn void die(void);
```

Para este curso: **`-std=c11`**. C11 tiene las features necesarias (`_Static_assert`, anonymous structs, `_Generic`, `threads.h`) sin depender de extensiones de compilador. `-Wpedantic` combinado con `-std=c11` advierte si usas extensiones.

---

## 4. Flags de debug (-g)

```bash
# Include debug information:
gcc -g main.c -o main

# Now GDB can show variable names, source lines, etc.
gdb ./main
# (gdb) break main
# (gdb) run
# (gdb) print x    <- works because -g includes the names

# Without -g:
gcc main.c -o main
gdb ./main
# (gdb) print x    <- "No symbol table is loaded"
```

### Niveles de debug

```bash
gcc -g0 main.c    # no debug info (default without -g)
gcc -g1 main.c    # minimal (lines and functions, not variables)
gcc -g  main.c    # standard (= -g2, the normal level)
gcc -g3 main.c    # maximum (includes expanded macros)
```

### -g es compatible con -O

```bash
gcc -g -O0 main.c -o main    # debug without optimization (ideal for GDB)
gcc -g -O2 main.c -o main    # debug with optimization (can be confusing)
```

Con optimizacion, el debug puede ser confuso: variables eliminadas, codigo reordenado, lineas que "saltan" de forma inesperada en el debugger. Para depurar, `-g -O0` es lo ideal.

---

## 5. Flags de optimizacion (-O)

```bash
gcc -O0 main.c -o main    # no optimization (default)
gcc -O1 main.c -o main    # basic optimization
gcc -O2 main.c -o main    # standard optimization (production)
gcc -O3 main.c -o main    # aggressive (can be counterproductive)
gcc -Os main.c -o main    # optimize for size (instead of speed)
gcc -Ofast main.c -o main # -O3 + non-standard-conforming flags
```

### Que hace cada nivel

| Nivel | Caracteristicas | Uso |
|-------|----------------|-----|
| `-O0` | Sin optimizacion. Compilacion rapida. Codigo 1:1 con fuente | Debug con GDB |
| `-O1` | Optimizaciones simples (eliminar dead code, simplificar) | Rara vez usado directamente |
| `-O2` | Inlining, loop unrolling limitado, eliminacion de subexpresiones comunes | **Produccion** |
| `-O3` | Todo de -O2 + vectorizacion SIMD, loop unrolling agresivo. Binarios mas grandes, puede ser mas lento que -O2 (cache misses) | Solo si se mide mejora |
| `-Os` | Optimiza tamano del binario. Suele ser rapido tambien (menos cache misses) | Sistemas embebidos |
| `-Ofast` | -O3 + viola IEEE 754 (reordena punto flotante). **No usar** sin saber las implicaciones | Casi nunca |

### Ejemplo de optimizacion

```c
// Original code:
int sum(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) {
        total += i;
    }
    return total;
}

// With -O2, GCC can convert this to:
// return n * (n + 1) / 2;
// (if it detects the arithmetic sum pattern)
```

---

## 6. Otros flags utiles

### Paths de headers y bibliotecas

```bash
# Add a directory to header search:
gcc -I./include main.c -o main
# Now #include "myheader.h" looks in ./include/

# Add a directory to library search:
gcc main.c -L./lib -lmylib -o main
# Looks for libmylib.so or libmylib.a in ./lib/
```

### Definir macros desde la linea de comandos

```bash
# Define a macro:
gcc -DDEBUG main.c -o main
# Equivalent to #define DEBUG at the start of the file

gcc -DVERSION=2 main.c -o main
# Equivalent to #define VERSION 2

# Useful: disable assert()
gcc -DNDEBUG main.c -o main
```

### Sanitizers

```bash
# AddressSanitizer (detects memory errors):
gcc -fsanitize=address -g main.c -o main

# UndefinedBehaviorSanitizer (detects UB):
gcc -fsanitize=undefined -g main.c -o main

# Both:
gcc -fsanitize=address,undefined -g main.c -o main
```

Los sanitizers agregan instrumentacion al binario que detecta errores en runtime: buffer overflow, use-after-free, signed integer overflow, null dereference, etc. Anaden overhead de rendimiento y dependencias de bibliotecas adicionales (libasan, libubsan), asi que solo se usan en desarrollo.

---

## 7. Sets recomendados

```bash
# DEVELOPMENT (debug, maximum error detection):
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined main.c -o main

# TESTING (warnings + basic optimization):
gcc -Wall -Wextra -Werror -std=c11 -g -O1 main.c -o main

# PRODUCTION:
gcc -Wall -Wextra -std=c11 -O2 -DNDEBUG main.c -o main

# THIS COURSE:
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

---

## 8. Tabla de flags

| Flag | Categoria | Que hace |
|---|---|---|
| `-Wall` | Warnings | Warnings principales |
| `-Wextra` | Warnings | Warnings adicionales |
| `-Wpedantic` | Warnings | Conformidad estricta al estandar |
| `-Werror` | Warnings | Warnings son errores |
| `-Wno-xxx` | Warnings | Desactivar warning especifico |
| `-std=c11` | Estandar | Compilar segun C11 |
| `-std=gnu11` | Estandar | C11 + extensiones GNU |
| `-g` | Debug | Incluir info de depuracion |
| `-O0` | Optimizacion | Sin optimizacion |
| `-O2` | Optimizacion | Optimizacion de produccion |
| `-O3` | Optimizacion | Optimizacion agresiva |
| `-Os` | Optimizacion | Optimizar tamano |
| `-I` | Paths | Directorio de headers |
| `-L` | Paths | Directorio de bibliotecas |
| `-l` | Enlace | Enlazar con biblioteca |
| `-D` | Preprocesador | Definir macro |
| `-fsanitize` | Sanitizers | Deteccion en runtime |

---

## 9. Ejercicios

Los archivos fuente del laboratorio estan en `labs/`. Usa ese directorio como base.

### Ejercicio 1: Warnings incrementales con warnings.c

Usando `labs/warnings.c` (tiene problemas deliberados: variable no usada, parametro no usado, variable sin inicializar, formato incorrecto, comparacion signed/unsigned, initializer incompleto).

1. Compila sin flags de warning: `gcc warnings.c -o warnings_none 2>&1`

**Prediccion**: cuantos warnings aparecen sin flags explicitos?

<details><summary>Respuesta</summary>

Al menos **1-2 warnings**. GCC sin flags no es completamente silencioso: emite algunos warnings criticos por defecto, como el formato de printf incorrecto (`%s` con un `int`). Pero no reporta variables no usadas ni parametros no usados.

La cantidad exacta depende de la version de GCC — versiones mas recientes emiten mas warnings por defecto.

</details>

2. Compila con `-Wall`: `gcc -Wall warnings.c -o warnings_wall 2>&1`

**Prediccion**: cuantos warnings nuevos agrega `-Wall`?

<details><summary>Respuesta</summary>

`-Wall` agrega warnings como:
- `unused variable 'unused_var'` — variable declarada pero nunca usada
- `'y' is used uninitialized` — variable sin valor inicial usada en el return
- `format '%s' expects 'char*', got 'int'` — tipo incorrecto en printf

El total sube a **~3-4 warnings**. `-Wall` no detecta parametros no usados ni missing initializers de struct — esos son de `-Wextra`.

</details>

---

### Ejercicio 2: -Wextra y -Wpedantic

Continuando con `labs/warnings.c`:

1. Compila con `-Wall -Wextra`: `gcc -Wall -Wextra warnings.c -o warnings_wextra 2>&1`

**Prediccion**: que warnings nuevos aparecen que `-Wall` solo no detectaba?

<details><summary>Respuesta</summary>

`-Wextra` agrega:
- `unused parameter 'unused_param'` — parametro en la firma pero nunca usado
- `comparison of integer expressions of different signedness` — `s < u` compara signed con unsigned
- `missing initializer for field 'depth'` — `struct Config cfg = { 640, 480 }` omite el tercer campo

El total sube a **~5-7 warnings**. Cada nivel agrega deteccion de problemas mas sutiles.

</details>

2. Compila con `-Wall -Wextra -Wpedantic -std=c11`: `gcc -Wall -Wextra -Wpedantic -std=c11 warnings.c -o warnings_pedantic 2>&1`

**Prediccion**: agrega `-Wpedantic` algun warning nuevo para este archivo?

<details><summary>Respuesta</summary>

**No** — `warnings.c` no usa extensiones GNU (no tiene zero-length arrays, statement expressions, ni binary literals). `-Wpedantic` solo advierte sobre codigo que no cumple el estandar; si el codigo ya es C11 valido, no agrega nada. En el ejercicio 4 veras su efecto real con extensiones.

</details>

---

### Ejercicio 3: -Werror y -Wno-xxx

1. Compila con `-Wall -Wextra -Werror`: `gcc -Wall -Wextra -Werror warnings.c -o warnings_werror 2>&1`

**Prediccion**: se genera el binario `warnings_werror`?

<details><summary>Respuesta</summary>

**No**. `-Werror` convierte cada warning en un error de compilacion. Como `warnings.c` tiene multiples warnings, la compilacion falla y no se genera ningun binario. Puedes verificar: `ls warnings_werror` dara "No such file or directory".

Esto es util en CI/CD para garantizar que nadie introduzca codigo con warnings en el repositorio.

</details>

2. Compila suprimiendo un warning especifico: `gcc -Wall -Wextra -Wno-unused-variable warnings.c -o warnings_suppress 2>&1`

**Prediccion**: el warning de `unused_var` aparece? Los demas siguen?

<details><summary>Respuesta</summary>

- `unused_var`: **no aparece** — `-Wno-unused-variable` lo desactiva especificamente
- Todos los demas warnings: **siguen apareciendo**

`-Wno-xxx` desactiva un warning individual sin afectar los demas. Util cuando un warning especifico es un falso positivo en tu codigo.

</details>

3. Limpieza: `rm -f warnings_none warnings_wall warnings_wextra warnings_pedantic warnings_werror warnings_suppress`

---

### Ejercicio 4: Extensiones GNU y -Wpedantic

Usando `labs/standard.c` (usa zero-length array, statement expression, binary literal):

1. Compila con `-std=gnu11`: `gcc -std=gnu11 -Wall -Wextra standard.c -o standard_gnu 2>&1 && ./standard_gnu`

**Prediccion**: hay warnings? Cual es la salida?

<details><summary>Respuesta</summary>

**Sin warnings** — `-std=gnu11` acepta extensiones GNU sin quejarse. Salida:

```
size of flex = 4
y    = 201
mask = 0xF0
```

- `sizeof(struct flex)` = 4 bytes: solo el campo `int n`, el array `data[0]` no ocupa espacio
- `y` = 201: `({ int tmp = 100 * 2; tmp + 1; })` = 201
- `mask` = 0xF0: `0b11110000` = 240 = 0xF0

</details>

2. Compila con `-std=c11 -Wpedantic`: `gcc -std=c11 -Wpedantic -Wall -Wextra standard.c -o standard_strict 2>&1`

**Prediccion**: cuantos warnings y sobre que?

<details><summary>Respuesta</summary>

**3 warnings**, uno por cada extension GNU:
- `ISO C forbids zero-size array 'data'`
- `ISO C forbids braced-groups within expressions` (statement expression)
- `binary constants are a C23 feature or a GCC extension` (literal binario)

El programa **compila** (son warnings, no errores) y produce la misma salida. Pero los warnings indican que este codigo no es portable a otros compiladores.

</details>

3. Limpieza: `rm -f standard_gnu standard_strict`

---

### Ejercicio 5: Flags de debug — efecto en tamano

Usando `labs/optimize_cmp.c`:

```bash
gcc -g -O0 optimize_cmp.c -o optimize_debug
gcc -O2 optimize_cmp.c -o optimize_fast
```

1. Compara tamanos: `ls -l optimize_debug optimize_fast`

**Prediccion**: cual es mas grande y por que?

<details><summary>Respuesta</summary>

**`optimize_debug` es significativamente mas grande** (tipicamente 2-5x). `-g` incluye tablas de simbolos, mapeo de lineas fuente-a-binario, nombres de variables, tipos, y otra metadata que GDB necesita para depurar. `-O0` ademas genera codigo verboso sin compactar.

`optimize_fast` tiene codigo optimizado (mas compacto) y sin informacion de debug.

</details>

2. Verifica con `file`: `file optimize_debug optimize_fast`

**Prediccion**: que diferencia clave muestra `file`?

<details><summary>Respuesta</summary>

- `optimize_debug`: incluye `with debug_info` (o `not stripped`)
- `optimize_fast`: dice `stripped` o no menciona debug info

La presencia de "debug_info" es lo que hace el binario mas grande. `strip optimize_debug` eliminaria la info de debug, reduciendo el tamano, pero perdiendo la capacidad de depurar con GDB.

</details>

3. Limpieza: `rm -f optimize_debug optimize_fast`

---

### Ejercicio 6: Optimizacion — assembly de un loop

```bash
gcc -S -O0 optimize_cmp.c -o optimize_O0.s
gcc -S -O2 optimize_cmp.c -o optimize_O2.s
```

1. Compara lineas: `wc -l optimize_O0.s optimize_O2.s`

**Prediccion**: el assembly de -O0 tendra ____ veces mas lineas que el de -O2.

<details><summary>Respuesta</summary>

Tipicamente **1.5-3x mas lineas** con `-O0`. Sin optimizacion, el compilador genera codigo que carga cada variable de la pila, opera, y guarda el resultado de vuelta. Con `-O2`, usa registros, elimina accesos redundantes a memoria, y puede transformar el loop drasticamente.

</details>

2. Busca la funcion `sum_to_n` en ambos:
   ```bash
   grep -A 30 "sum_to_n:" optimize_O0.s | head -35
   grep -A 30 "sum_to_n:" optimize_O2.s | head -35
   ```

**Prediccion**: con -O0 veras un loop con accesos a pila en cada iteracion. Con -O2, que podria haber hecho el optimizador?

<details><summary>Respuesta</summary>

Con `-O0`: un loop completo con `movl` para cargar `total` e `i` de la pila, `addl` para sumar, `movl` para guardar de vuelta, y un salto condicional. Muchos accesos a memoria innecesarios.

Con `-O2`: el compilador puede haber:
- Usado registros exclusivamente (sin tocar la pila en el loop)
- Desenrollado parcialmente el loop (loop unrolling)
- O incluso reemplazado todo el loop con la formula `n * (n + 1) / 2` si detecto el patron de suma aritmetica

La diferencia visual es dramatica y muestra por que las optimizaciones importan.

</details>

3. Limpieza: `rm -f optimize_O0.s optimize_O2.s`

---

### Ejercicio 7: Sets de desarrollo vs produccion

```bash
# Development set
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined \
    optimize_cmp.c -o optimize_dev

# Production set
gcc -Wall -Wextra -std=c11 -O2 -DNDEBUG \
    optimize_cmp.c -o optimize_prod
```

1. Compara tamanos: `ls -l optimize_dev optimize_prod`

**Prediccion**: cual es mas grande y por cuanto aproximadamente?

<details><summary>Respuesta</summary>

**`optimize_dev` es mucho mas grande** (puede ser 5-10x o mas). Combina tres factores que aumentan el tamano:
- `-g`: informacion de debug
- `-O0`: codigo sin compactar
- `-fsanitize=address,undefined`: instrumentacion de sanitizers (checks en cada acceso a memoria, cada operacion aritmetica, etc.)

`optimize_prod` tiene codigo optimizado (-O2), sin debug info, sin sanitizers.

</details>

2. Compara dependencias: `ldd optimize_dev` y `ldd optimize_prod`

**Prediccion**: el binario de desarrollo depende de bibliotecas extra? Cuales?

<details><summary>Respuesta</summary>

**Si**. `optimize_dev` depende de:
- `libasan.so` (AddressSanitizer runtime)
- `libubsan.so` (UndefinedBehaviorSanitizer runtime)

Ademas de las habituales `libc.so.6` y `ld-linux-x86-64.so.2`.

`optimize_prod` solo depende de `libc.so.6` y el enlazador dinamico. Los sanitizers solo se usan en desarrollo porque anaden overhead significativo de rendimiento.

</details>

3. Verifica que ambos producen el mismo resultado: `./optimize_dev && ./optimize_prod`

4. Limpieza: `rm -f optimize_dev optimize_prod`

---

### Ejercicio 8: -DDEBUG y compilacion condicional

Crea un archivo temporal para este ejercicio:

```bash
cat > /tmp/conditional.c << 'EOF'
#include <stdio.h>

int main(void) {
    int x = 42;

#ifdef DEBUG
    printf("[DEBUG] x = %d\n", x);
#endif

    printf("Result: %d\n", x * 2);
    return 0;
}
EOF
```

1. Compila sin `-DDEBUG`: `gcc -std=c11 /tmp/conditional.c -o /tmp/cond_normal && /tmp/cond_normal`

**Prediccion**: cuantas lineas de salida?

<details><summary>Respuesta</summary>

**1 linea**: solo `Result: 84`. El bloque `#ifdef DEBUG` no se compila porque `DEBUG` no esta definido. El preprocesador elimina todo el codigo entre `#ifdef DEBUG` y `#endif`.

</details>

2. Compila con `-DDEBUG`: `gcc -std=c11 -DDEBUG /tmp/conditional.c -o /tmp/cond_debug && /tmp/cond_debug`

**Prediccion**: cuantas lineas de salida ahora?

<details><summary>Respuesta</summary>

**2 lineas**:
```
[DEBUG] x = 42
Result: 84
```

`-DDEBUG` es equivalente a poner `#define DEBUG` al inicio del archivo. Ahora `#ifdef DEBUG` es verdadero y el printf de debug se incluye en la compilacion.

Esto permite tener codigo de depuracion que solo se activa cuando compilas con ese flag, sin impactar la version de produccion.

</details>

3. Limpieza: `rm -f /tmp/conditional.c /tmp/cond_normal /tmp/cond_debug`

---

### Ejercicio 9: -I para paths de headers

```bash
mkdir -p /tmp/myinc
cat > /tmp/myinc/greet.h << 'EOF'
#ifndef GREET_H
#define GREET_H
#define GREETING "Hello from custom header"
#endif
EOF

cat > /tmp/test_include.c << 'EOF'
#include <stdio.h>
#include "greet.h"

int main(void) {
    printf("%s\n", GREETING);
    return 0;
}
EOF
```

1. Compila sin `-I`: `gcc -std=c11 /tmp/test_include.c -o /tmp/test_inc 2>&1`

**Prediccion**: que error da?

<details><summary>Respuesta</summary>

`fatal error: greet.h: No such file or directory`

GCC busca `"greet.h"` primero en el directorio del archivo fuente (`/tmp/`) y luego en los paths del sistema. Como `greet.h` esta en `/tmp/myinc/`, no lo encuentra.

</details>

2. Compila con `-I/tmp/myinc`: `gcc -std=c11 -I/tmp/myinc /tmp/test_include.c -o /tmp/test_inc && /tmp/test_inc`

**Prediccion**: la salida sera ____

<details><summary>Respuesta</summary>

`Hello from custom header`

`-I/tmp/myinc` agrega ese directorio a la lista de busqueda de headers. Ahora cuando GCC ve `#include "greet.h"`, lo busca (entre otros sitios) en `/tmp/myinc/` y lo encuentra.

`-I` es esencial en proyectos con multiples directorios: `-Isrc/include -Ilib/include` etc.

</details>

3. Limpieza: `rm -rf /tmp/myinc /tmp/test_include.c /tmp/test_inc`

---

### Ejercicio 10: Sanitizers en accion

```bash
cat > /tmp/buggy.c << 'EOF'
#include <stdio.h>

int main(void) {
    int arr[5] = {1, 2, 3, 4, 5};

    // Bug: out-of-bounds access
    int val = arr[10];
    printf("val = %d\n", val);

    // Bug: signed integer overflow
    int big = 2147483647;  // INT_MAX
    int overflow = big + 1;
    printf("overflow = %d\n", overflow);

    return 0;
}
EOF
```

1. Compila sin sanitizer y ejecuta: `gcc -std=c11 -O0 /tmp/buggy.c -o /tmp/buggy_normal && /tmp/buggy_normal`

**Prediccion**: el programa crashea o imprime algo?

<details><summary>Respuesta</summary>

Probablemente **imprime valores sin crashear**. El acceso fuera de limites (`arr[10]`) lee basura del stack pero no necesariamente causa un segfault — depende de la disposicion de la memoria. El integer overflow produce un valor negativo (-2147483648) en la mayoria de sistemas, pero es **comportamiento indefinido** — el compilador puede hacer cualquier cosa.

El programa "funciona" pero tiene dos bugs que podrian causar problemas impredecibles en codigo real.

</details>

2. Compila con sanitizers: `gcc -std=c11 -g -O0 -fsanitize=address,undefined /tmp/buggy.c -o /tmp/buggy_sanitized && /tmp/buggy_sanitized`

**Prediccion**: que reportan los sanitizers?

<details><summary>Respuesta</summary>

Los sanitizers detectan ambos bugs y reportan mensajes detallados:

- **AddressSanitizer**: `stack-buffer-overflow on address ...` al acceder `arr[10]`. Muestra la linea exacta del codigo fuente (gracias a `-g`), el tamano del array, y que direccion se accedio fuera de limites.

- **UndefinedBehaviorSanitizer**: `signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`. Muestra la operacion exacta que causa el overflow.

El programa puede abortar despues del primer error detectado. Esta es la razon por la que los sanitizers son indispensables en desarrollo: detectan bugs que de otra forma pasarian desapercibidos.

</details>

3. Limpieza: `rm -f /tmp/buggy.c /tmp/buggy_normal /tmp/buggy_sanitized`
