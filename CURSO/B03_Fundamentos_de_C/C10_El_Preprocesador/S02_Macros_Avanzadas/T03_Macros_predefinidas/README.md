# T03 — Macros predefinidas

## Macros estándar (todos los compiladores C)

El estándar C define macros que el compilador debe proveer
sin necesidad de `#define`:

```c
#include <stdio.h>

int main(void) {
    // __FILE__ — nombre del archivo fuente actual:
    printf("File: %s\n", __FILE__);      // → "main.c"

    // __LINE__ — número de línea actual:
    printf("Line: %d\n", __LINE__);      // → 7

    // __DATE__ — fecha de compilación ("Mmm dd yyyy"):
    printf("Date: %s\n", __DATE__);      // → "Mar 17 2026"

    // __TIME__ — hora de compilación ("hh:mm:ss"):
    printf("Time: %s\n", __TIME__);      // → "14:30:05"

    // __STDC__ — 1 si el compilador cumple el estándar C:
    printf("STDC: %d\n", __STDC__);      // → 1

    // __STDC_VERSION__ — versión del estándar:
    printf("Version: %ld\n", __STDC_VERSION__);
    // 199901L = C99
    // 201112L = C11
    // 201710L = C17
    // 202311L = C23

    return 0;
}
```

```c
// __func__ (C99) — nombre de la función actual:
// Nota: __func__ no es un macro sino una variable local implícita.
// Se comporta como: static const char __func__[] = "function_name";

void process_data(int *data) {
    printf("Entering %s\n", __func__);   // → "process_data"
}

// __func__ NO se puede concatenar con # ni ## porque no es macro:
// #define LOG(msg) printf("%s: %s\n", __func__, msg)   // OK — uso normal
// #define TAG __func__ ## _tag                          // ERROR — no es macro
```

## __FILE__ y __LINE__ en macros

El uso más poderoso de `__FILE__` y `__LINE__` es dentro
de macros — se expanden en el lugar de la llamada, no donde
se definió el macro:

```c
// Si se usan directamente en una función:
void log_msg(const char *msg) {
    printf("%s:%d: %s\n", __FILE__, __LINE__, msg);
    // Siempre imprime la línea de ESTA función, no del caller.
}

// Con un macro, se captura la ubicación del caller:
#define LOG(msg) printf("%s:%d: %s\n", __FILE__, __LINE__, msg)

LOG("error");    // → printf("main.c", 42, "error")
// __FILE__ y __LINE__ se expanden donde se usa LOG.
```

```c
// Patrón: macro que pasa ubicación a una función:

#define MALLOC(size) \
    malloc_tracked(size, __FILE__, __LINE__)

void *malloc_tracked(size_t size, const char *file, int line) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "%s:%d: malloc(%zu) failed\n", file, line, size);
        abort();
    }
    // Opcionalmente registrar la asignación para detectar leaks:
    track_alloc(p, size, file, line);
    return p;
}

// Uso:
int *arr = MALLOC(100 * sizeof(int));
// Si falla, el error muestra la línea del caller, no de malloc_tracked.
```

### Assert con ubicación

```c
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, \
            "Assertion failed: %s\n" \
            "  File: %s\n" \
            "  Line: %d\n" \
            "  Function: %s\n", \
            #cond, __FILE__, __LINE__, __func__); \
        abort(); \
    } \
} while (0)

// El assert estándar de <assert.h> usa el mismo mecanismo:
// assert(x > 0);
// Si falla, imprime algo como:
// prog: main.c:42: main: Assertion `x > 0' failed.
```

### Error tracking

```c
// Macro para retornar errores con contexto:
#define RETURN_ERROR(code, fmt, ...) do { \
    fprintf(stderr, "[ERROR] %s:%d %s(): " fmt "\n", \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    return (code); \
} while (0)

int open_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        RETURN_ERROR(-1, "cannot open %s: %s", path, strerror(errno));
    }
    // ...
    return 0;
}
// Si falla:
// [ERROR] config.c:15 open_config(): cannot open /etc/app.conf: No such file
```

## __DATE__ y __TIME__ — Build timestamp

```c
// Incrustar información de build en el binario:

#define BUILD_INFO "Built on " __DATE__ " at " __TIME__

void print_version(void) {
    printf("MyApp v1.0.0\n");
    printf("%s\n", BUILD_INFO);
    // → "Built on Mar 17 2026 at 14:30:05"
}
```

```c
// Problema: __DATE__ y __TIME__ hacen builds no-reproducibles.
// Cada compilación produce un binario diferente (diferente timestamp).
// Esto rompe build caching y verificación de reproducibilidad.

// Soluciones:
// 1. No usar __DATE__/__TIME__ en producción.
// 2. GCC/Clang: -Wdate-time da warning si se usan.
// 3. Definir macros de fecha externamente:
//    gcc -DBUILD_DATE=\"$(date +%Y-%m-%d)\" prog.c
// 4. SOURCE_DATE_EPOCH (convención para builds reproducibles):
//    export SOURCE_DATE_EPOCH=$(date +%s)
//    Algunos compiladores respetan esta variable.
```

## __STDC_VERSION__ — Detectar estándar

```c
// Usar features condicionalmente según el estándar disponible:

#if __STDC_VERSION__ >= 202311L
    // C23: typeof, auto, #embed, constexpr, etc.
    #define HAS_TYPEOF 1
    #define HAS_AUTO 1
#endif

#if __STDC_VERSION__ >= 201112L
    // C11: _Generic, _Static_assert, _Alignof, etc.
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    // Fallback para C99:
    #define STATIC_ASSERT(cond, msg) \
        typedef char static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

#if __STDC_VERSION__ >= 199901L
    // C99: VLAs, // comments, mixed declarations, etc.
    #define HAS_VLA 1
#endif
```

## Macros predefinidas de GCC/Clang

```c
// Estas no son estándar pero son muy comunes en código Unix/Linux:

// __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ — versión de GCC:
#ifdef __GNUC__
    #define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #if GCC_VERSION >= 40800
        // GCC 4.8+ features
    #endif
#endif

// __clang__ — detectar Clang (nota: Clang también define __GNUC__):
#if defined(__clang__)
    // Clang
#elif defined(__GNUC__)
    // GCC (no Clang)
#endif

// __SIZEOF_POINTER__ — tamaño de puntero (4 u 8):
#if __SIZEOF_POINTER__ == 8
    #define ARCH_64BIT 1
#endif

// __BYTE_ORDER__ — endianness:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define IS_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define IS_BIG_ENDIAN 1
#endif

// __COUNTER__ — contador único que se incrementa con cada uso:
#define UNIQUE_ID __COUNTER__
int CONCAT(var_, __COUNTER__) = 1;   // var_0
int CONCAT(var_, __COUNTER__) = 2;   // var_1
// Útil para generar nombres únicos sin colisión.
```

## __attribute__ (extensión GCC/Clang)

```c
// No son macros predefinidas, pero se usan junto con ellas:

// Función que nunca retorna:
__attribute__((noreturn))
void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}

// Función con formato tipo printf (el compilador verifica los args):
__attribute__((format(printf, 1, 2)))
void log_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

log_msg("%d\n", "hello");   // WARNING: format '%d' expects int, not char*

// Función deprecated:
__attribute__((deprecated("use new_func instead")))
void old_func(void) { /* ... */ }

// Variable no usada (suprime warning):
__attribute__((unused))
int debug_counter = 0;

// Hacer portable con macros:
#ifdef __GNUC__
    #define NORETURN    __attribute__((noreturn))
    #define PRINTF_FMT(a, b) __attribute__((format(printf, a, b)))
    #define DEPRECATED(msg) __attribute__((deprecated(msg)))
    #define UNUSED     __attribute__((unused))
#else
    #define NORETURN
    #define PRINTF_FMT(a, b)
    #define DEPRECATED(msg)
    #define UNUSED
#endif
```

## Macro predefinida para el estándar: __STDC_HOSTED__

```c
// __STDC_HOSTED__ — indica si es un entorno hosted (con OS):
// 1 = hosted (tiene main, stdlib, stdio, etc.)
// 0 = freestanding (kernels, embedded, bootloaders)

#if __STDC_HOSTED__ == 0
    // Freestanding: solo <stddef.h>, <stdint.h>, <float.h>,
    // <limits.h>, <stdarg.h>, <stdbool.h>, <stdnoreturn.h>
    // No hay printf, malloc, fopen, etc.
#endif
```

## Resumen de macros predefinidas

```c
// Estándar C (todos los compiladores):
// __FILE__           — nombre del archivo ("main.c")
// __LINE__           — número de línea (42)
// __DATE__           — fecha de compilación ("Mar 17 2026")
// __TIME__           — hora de compilación ("14:30:05")
// __STDC__           — 1 si es compilador C estándar
// __STDC_VERSION__   — versión del estándar (201112L = C11)
// __STDC_HOSTED__    — 1 si es entorno hosted

// C99:
// __func__           — nombre de la función actual (no es macro)

// GCC/Clang:
// __GNUC__           — versión major de GCC
// __clang__          — definido si es Clang
// __COUNTER__        — contador auto-incrementable
// __SIZEOF_POINTER__ — tamaño de puntero en bytes
// __BYTE_ORDER__     — endianness
// __PRETTY_FUNCTION__ — nombre de función con tipos (C++)

// Plataforma (GCC/Clang):
// __linux__          — Linux
// __APPLE__          — macOS/iOS
// _WIN32             — Windows
// __x86_64__         — arquitectura x86-64
// __aarch64__        — arquitectura ARM64
```

---

## Ejercicios

### Ejercicio 1 — Build info

```c
// Crear una función print_build_info() que imprima:
// - Compilador y versión (GCC o Clang, con número de versión)
// - Estándar C (__STDC_VERSION__ formateado como "C11", "C17", etc.)
// - Plataforma y arquitectura
// - Fecha y hora de compilación
// - Si es build debug o release (verificar si NDEBUG está definido)
// Compilar y verificar que la información es correcta.
```

### Ejercicio 2 — Memory tracker

```c
// Crear macros MALLOC(size), FREE(ptr) que:
// - MALLOC llame a una función que registre: puntero, tamaño, archivo, línea
// - FREE marque la asignación como liberada
// - Al final del programa, una función report_leaks() imprima
//   las asignaciones no liberadas con su ubicación original
// Usar __FILE__ y __LINE__ para capturar la ubicación del caller.
```

### Ejercicio 3 — Portabilidad con fallbacks

```c
// Crear un header "compat.h" que defina:
// - STATIC_ASSERT(cond, msg) — _Static_assert en C11+, fallback en C99
// - NORETURN — _Noreturn en C11+, __attribute__((noreturn)) en GCC
// - THREAD_LOCAL — _Thread_local en C11+, __thread en GCC
// - ALIGNAS(n) — _Alignas en C11+, __attribute__((aligned(n))) en GCC
// Probar con gcc -std=c99 y gcc -std=c11 para verificar los fallbacks.
```
