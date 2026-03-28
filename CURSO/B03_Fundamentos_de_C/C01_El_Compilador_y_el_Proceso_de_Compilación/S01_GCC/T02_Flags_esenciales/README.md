# T02 — Flags esenciales

## Por qué importan los flags

GCC tiene cientos de flags. Un programa puede compilar perfectamente
con `gcc main.c` pero tener bugs ocultos que los flags correctos
habrían detectado. Conocer los flags esenciales es la diferencia
entre descubrir los problemas en compilación o en producción:

```bash
# Compilación mínima (NO hacer esto):
gcc main.c -o main

# Compilación correcta para desarrollo:
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 main.c -o main

# Compilación para producción:
gcc -Wall -Wextra -std=c11 -O2 main.c -o main
```

## Flags de warnings

### -Wall — Warnings principales

```bash
gcc -Wall main.c -o main
```

A pesar del nombre, `-Wall` **no** activa todos los warnings. Activa
los más comunes y generalmente útiles:

```c
// Warnings que -Wall detecta:

// 1. Variable no usada:
int x = 5;  // warning: unused variable 'x'

// 2. Variable usada sin inicializar:
int y;
printf("%d\n", y);  // warning: 'y' is used uninitialized

// 3. Formato de printf incorrecto:
int n = 42;
printf("%s\n", n);  // warning: format '%s' expects 'char*', got 'int'

// 4. Comparación implícita con signo:
unsigned int u = 10;
int s = -1;
if (s < u) { }  // warning: comparison of integer expressions
                 // of different signedness

// 5. Switch sin default o sin cubrir todos los casos
// 6. Retorno implícito en función no-void
// 7. Paréntesis sugeridos para claridad
// ...y muchos más
```

### -Wextra — Warnings adicionales

```bash
gcc -Wall -Wextra main.c -o main
```

`-Wextra` agrega warnings que `-Wall` no incluye:

```c
// Warnings adicionales con -Wextra:

// 1. Parámetro no usado:
int process(int x, int y) {  // warning: unused parameter 'y'
    return x * 2;
}

// 2. Comparación siempre verdadera/falsa:
unsigned int u = 5;
if (u >= 0) { }  // warning: comparison of unsigned >= 0 is always true

// 3. Condición vacía:
if (x);   // warning: empty body in 'if' statement
    y++;

// 4. Missing field initializer:
struct Point { int x; int y; int z; };
struct Point p = { 1, 2 };  // warning: missing initializer for field 'z'
```

### -Wpedantic — Conformidad estricta al estándar

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 main.c -o main
```

`-Wpedantic` advierte sobre extensiones de GCC que no son parte del
estándar C:

```c
// Warnings con -Wpedantic:

// 1. Arrays de tamaño cero (extensión GNU):
int arr[0];  // warning: ISO C forbids zero-size array

// 2. Comentarios estilo C++ en modo C89:
// Este comentario da warning en -std=c89 -Wpedantic

// 3. Binary literals (extensión):
int b = 0b1010;  // warning: binary constants are a C23 feature or GCC extension

// 4. typeof (extensión GNU antes de C23):
typeof(x) y;  // warning: ISO C does not support 'typeof'
```

### -Werror — Tratar warnings como errores

```bash
gcc -Wall -Wextra -Werror main.c -o main
```

```c
// Con -Werror, la compilación FALLA si hay cualquier warning:
int x;  // ya no es un warning — es un ERROR de compilación
// error: unused variable 'x' [-Werror=unused-variable]

// Esto obliga a mantener código limpio.
// Útil en CI/CD para que nadie introduzca warnings.
```

```bash
# Tratar un warning específico como error:
gcc -Werror=return-type main.c -o main
# Solo falla si hay un warning de return type

# Desactivar un warning específico:
gcc -Wall -Wno-unused-variable main.c -o main
# Activa todo de -Wall EXCEPTO unused-variable
```

## Selección de estándar (-std)

```bash
# Especificar qué estándar de C usar:
gcc -std=c89 main.c      # C89/C90 (ANSI C)
gcc -std=c99 main.c      # C99
gcc -std=c11 main.c      # C11 (recomendado para este curso)
gcc -std=c17 main.c      # C17 (solo correcciones sobre C11)
gcc -std=c23 main.c      # C23 (el más reciente, soporte parcial)
```

### -std=c11 vs -std=gnu11

```bash
# -std=c11: estándar C11 estricto
# -std=gnu11: C11 + extensiones GNU (default de GCC)

gcc main.c -o main           # usa -std=gnu17 por defecto
gcc -std=c11 main.c -o main  # C11 estricto, sin extensiones
gcc -std=gnu11 main.c -o main # C11 con extensiones GNU
```

```c
// Extensiones GNU que -std=gnu11 permite pero -std=c11 no:

// 1. Nested functions:
void foo(void) {
    void bar(void) { }  // solo con gnu, no en estándar
}

// 2. Statement expressions:
int x = ({ int y = 5; y * 2; });  // extensión GNU

// 3. typeof (antes de C23):
typeof(x) copy = x;  // extensión GNU, estandarizado en C23

// 4. Atributos con __attribute__:
void die(void) __attribute__((noreturn));  // extensión GNU
// En C estándar: _Noreturn void die(void);
```

```bash
# Para este curso: -std=c11
# Razón: C11 tiene las features que necesitamos
# (static_assert, anonymous structs, _Generic, threads.h)
# sin depender de extensiones de compilador.

# -Wpedantic combinado con -std=c11 advierte si usas extensiones.
```

## Flags de debug (-g)

```bash
# Incluir información de depuración:
gcc -g main.c -o main

# Ahora GDB puede mostrar nombres de variables, líneas de código, etc.
gdb ./main
# (gdb) break main
# (gdb) run
# (gdb) print x    ← funciona porque -g incluye los nombres

# Sin -g:
gcc main.c -o main
gdb ./main
# (gdb) print x    ← "No symbol table is loaded"
```

```bash
# Niveles de debug:
gcc -g0 main.c    # sin información de debug (default sin -g)
gcc -g1 main.c    # mínima (líneas y funciones, no variables)
gcc -g  main.c    # estándar (= -g2, lo normal)
gcc -g3 main.c    # máxima (incluye macros expandidas)
```

```bash
# -g es compatible con -O, pero la optimización puede
# hacer que el debug sea confuso (variables eliminadas,
# código reordenado):

gcc -g -O0 main.c -o main    # debug sin optimización (ideal para GDB)
gcc -g -O2 main.c -o main    # debug con optimización (puede ser confuso)
```

## Flags de optimización (-O)

```bash
# Niveles de optimización:
gcc -O0 main.c -o main    # sin optimización (default)
gcc -O1 main.c -o main    # optimización básica
gcc -O2 main.c -o main    # optimización estándar (producción)
gcc -O3 main.c -o main    # agresiva (puede ser contraproducente)
gcc -Os main.c -o main    # optimizar tamaño (en lugar de velocidad)
gcc -Ofast main.c -o main # -O3 + flags no conformes al estándar
```

### Qué hace cada nivel

```bash
# -O0 (default):
# - Sin optimización
# - Compilación más rápida
# - Ideal para debug (código corresponde 1:1 con el fuente)

# -O1:
# - Optimizaciones simples (eliminar código muerto, simplificar)
# - Sin aumento significativo de tiempo de compilación

# -O2 (RECOMENDADO para producción):
# - La mayoría de optimizaciones que no sacrifican correctitud
# - Inlining de funciones pequeñas
# - Loop unrolling limitado
# - Eliminación de subexpresiones comunes
# - Buen balance entre velocidad de ejecución y de compilación

# -O3:
# - Todo de -O2 más optimizaciones agresivas
# - Vectorización automática (SIMD)
# - Loop unrolling agresivo
# - Puede generar binarios MÁS GRANDES
# - Puede ser MÁS LENTO que -O2 (cache misses por código grande)

# -Os:
# - Optimiza para tamaño del binario
# - Útil en sistemas embebidos
# - Suele ser rápido también (código pequeño = menos cache misses)

# -Ofast:
# - -O3 + optimizaciones que violan el estándar IEEE 754
# - Puede reordenar operaciones de punto flotante
# - NO usar a menos que sepas exactamente qué implica
```

```c
// Ejemplo: qué optimiza -O2

// Código original:
int sum(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) {
        total += i;
    }
    return total;
}

// Con -O2, GCC puede convertir esto en:
// return n * (n + 1) / 2;
// (si detecta el patrón de suma aritmética)
```

## Otros flags útiles

### Paths de headers y bibliotecas

```bash
# Agregar un directorio a la búsqueda de headers:
gcc -I./include main.c -o main
# Ahora #include "myheader.h" busca en ./include/

# Agregar un directorio a la búsqueda de bibliotecas:
gcc main.c -L./lib -lmylib -o main
# Busca libmylib.so o libmylib.a en ./lib/
```

### Definir macros desde la línea de comandos

```bash
# Definir una macro:
gcc -DDEBUG main.c -o main
# Equivalente a poner #define DEBUG al inicio del archivo

gcc -DVERSION=2 main.c -o main
# Equivalente a #define VERSION 2

# Útil para compilación condicional:
gcc -DNDEBUG main.c -o main    # deshabilita assert()
```

### Sanitizers

```bash
# AddressSanitizer (detecta errores de memoria):
gcc -fsanitize=address -g main.c -o main

# UndefinedBehaviorSanitizer (detecta UB):
gcc -fsanitize=undefined -g main.c -o main

# Ambos:
gcc -fsanitize=address,undefined -g main.c -o main
```

## Sets recomendados

```bash
# DESARROLLO (debug, máxima detección de errores):
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined main.c -o main

# TESTING (warnings + optimización básica):
gcc -Wall -Wextra -Werror -std=c11 -g -O1 main.c -o main

# PRODUCCIÓN:
gcc -Wall -Wextra -std=c11 -O2 -DNDEBUG main.c -o main

# ESTE CURSO:
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

## Tabla de flags

| Flag | Categoría | Qué hace |
|---|---|---|
| -Wall | Warnings | Warnings principales |
| -Wextra | Warnings | Warnings adicionales |
| -Wpedantic | Warnings | Conformidad estricta al estándar |
| -Werror | Warnings | Warnings son errores |
| -Wno-xxx | Warnings | Desactivar warning específico |
| -std=c11 | Estándar | Compilar según C11 |
| -std=gnu11 | Estándar | C11 + extensiones GNU |
| -g | Debug | Incluir info de depuración |
| -O0 | Optimización | Sin optimización |
| -O2 | Optimización | Optimización de producción |
| -O3 | Optimización | Optimización agresiva |
| -Os | Optimización | Optimizar tamaño |
| -I | Paths | Directorio de headers |
| -L | Paths | Directorio de bibliotecas |
| -l | Enlace | Enlazar con biblioteca |
| -D | Preprocesador | Definir macro |
| -fsanitize | Sanitizers | Detección en runtime |

---

## Ejercicios

### Ejercicio 1 — Warnings

```c
// Compilar este código con diferentes niveles de warnings:
// gcc main.c
// gcc -Wall main.c
// gcc -Wall -Wextra main.c
// ¿Cuántos warnings aparecen en cada caso?

#include <stdio.h>

int process(int x, int unused_param) {
    int y;
    unsigned int u = 10;
    int s = -1;

    if (s < u) {
        printf("%s\n", x);
    }

    return y;
}

int main(void) {
    process(5, 0);
}
```

### Ejercicio 2 — Optimización

```bash
# Compilar un loop simple con -O0, -O2 y -O3
# Generar assembly con -S para cada nivel
# Comparar el número de instrucciones
```

### Ejercicio 3 — Estándar

```c
// Este código compila con -std=gnu11 pero no con -std=c11 -Wpedantic.
// ¿Por qué?
#include <stdio.h>

int main(void) {
    typeof(42) x = 100;
    printf("%d\n", x);
    return 0;
}
```
