# T04 — Compilación condicional

## #ifdef y #ifndef

Las directivas de compilación condicional permiten incluir o
excluir código según macros definidas:

```c
// #ifdef — si la macro ESTÁ definida:
#ifdef DEBUG
    printf("Debug: x = %d\n", x);
#endif

// #ifndef — si la macro NO está definida:
#ifndef NDEBUG
    assert(x > 0);
#endif

// Equivalen a:
#if defined(DEBUG)
    printf("Debug: x = %d\n", x);
#endif

#if !defined(NDEBUG)
    assert(x > 0);
#endif
```

```c
// El valor del macro NO importa para #ifdef — solo si existe:
#define FOO          // definido (valor vacío)
#define BAR 0        // definido (valor 0)
#define BAZ 1        // definido (valor 1)

#ifdef FOO           // TRUE — FOO existe
#ifdef BAR           // TRUE — BAR existe (aunque vale 0)
#ifdef BAZ           // TRUE

// Para verificar el VALOR, usar #if:
#if BAR              // FALSE — BAR vale 0
#if BAZ              // TRUE  — BAZ vale 1
```

```c
// Definir macros desde la línea de compilación:
// gcc -DDEBUG prog.c            → #define DEBUG 1
// gcc -DDEBUG=2 prog.c          → #define DEBUG 2
// gcc -DVERSION=\"1.0\" prog.c  → #define VERSION "1.0"
// gcc -UNDEBUG prog.c           → #undef NDEBUG

// Uso típico para habilitar debug:
#ifdef DEBUG
    #define LOG(fmt, ...) fprintf(stderr, "[DBG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG(fmt, ...) ((void)0)
#endif

// Compilar con: gcc -DDEBUG prog.c   → logging activo
// Compilar con: gcc prog.c           → logging eliminado
```

## #if, #elif, #else, #endif

`#if` evalúa expresiones constantes enteras en tiempo
de preprocesamiento:

```c
#if LEVEL == 1
    // código para nivel 1
#elif LEVEL == 2
    // código para nivel 2
#elif LEVEL == 3
    // código para nivel 3
#else
    // nivel por defecto
#endif

// Si LEVEL no está definido, se trata como 0:
// #if LEVEL == 1  →  #if 0 == 1  →  FALSE
```

```c
// Operadores permitidos en #if:
// Aritméticos: +, -, *, /, %
// Comparación: ==, !=, <, >, <=, >=
// Lógicos: &&, ||, !
// Bitwise: &, |, ^, ~, <<, >>
// Ternario: ? :
// defined(): verifica si un macro existe

#if (VERSION_MAJOR > 2) || (VERSION_MAJOR == 2 && VERSION_MINOR >= 5)
    // versión 2.5 o superior
#endif

#if defined(LINUX) && !defined(ANDROID)
    // Linux de escritorio, no Android
#endif

// NO se puede usar: sizeof, casts, variables, llamadas a función.
// Solo constantes enteras y macros (que se expanden a constantes).
```

```c
// defined() vs #ifdef:
// #ifdef solo acepta UN macro. defined() se puede combinar:

// Esto NO se puede hacer con #ifdef:
#if defined(FEATURE_A) && defined(FEATURE_B)
    // ambas features activas
#endif

#if defined(LINUX) || defined(MACOS) || defined(BSD)
    // cualquier Unix
#endif

// #ifdef FEATURE_A && FEATURE_B   ← ERROR de sintaxis
```

## Patrones comunes

### Debug condicional

```c
// Nivel de debug configurable:
// gcc -DDEBUG_LEVEL=2 prog.c

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif

#if DEBUG_LEVEL >= 1
    #define LOG_ERROR(fmt, ...) \
        fprintf(stderr, "[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 2
    #define LOG_WARN(fmt, ...) \
        fprintf(stderr, "[WRN] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#if DEBUG_LEVEL >= 3
    #define LOG_DEBUG(fmt, ...) \
        fprintf(stderr, "[DBG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif
```

### Portabilidad entre plataformas

```c
// Macros predefinidos por el compilador según la plataforma:

#if defined(__linux__)
    #include <unistd.h>
    #define PLATFORM "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <unistd.h>
    #define PLATFORM "macOS"
#elif defined(_WIN32)
    #include <windows.h>
    #define PLATFORM "Windows"
#elif defined(__FreeBSD__)
    #include <unistd.h>
    #define PLATFORM "FreeBSD"
#else
    #error "Unsupported platform"
#endif

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}
```

```c
// Detectar compilador:
#if defined(__GNUC__) && !defined(__clang__)
    #define COMPILER "GCC"
    #define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__clang__)
    #define COMPILER "Clang"
    #define CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(_MSC_VER)
    #define COMPILER "MSVC"
#endif

// Detectar estándar de C:
#if __STDC_VERSION__ >= 202311L
    #define C_STANDARD "C23"
#elif __STDC_VERSION__ >= 201710L
    #define C_STANDARD "C17"
#elif __STDC_VERSION__ >= 201112L
    #define C_STANDARD "C11"
#elif __STDC_VERSION__ >= 199901L
    #define C_STANDARD "C99"
#else
    #define C_STANDARD "C89/C90"
#endif
```

```c
// Detectar arquitectura:
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH "x86_64"
    #define ARCH_64BIT 1
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH "x86"
    #define ARCH_64BIT 0
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH "ARM64"
    #define ARCH_64BIT 1
#elif defined(__arm__) || defined(_M_ARM)
    #define ARCH "ARM"
    #define ARCH_64BIT 0
#else
    #define ARCH "unknown"
#endif
```

### Feature toggles

```c
// Habilitar/deshabilitar funcionalidad:
// gcc -DUSE_SSL -DUSE_LOGGING prog.c

typedef struct Config {
    int port;
#ifdef USE_SSL
    char *cert_path;
    char *key_path;
#endif
} Config;

void init(Config *cfg) {
    cfg->port = 8080;
#ifdef USE_SSL
    cfg->cert_path = "/etc/ssl/cert.pem";
    cfg->key_path = "/etc/ssl/key.pem";
    init_ssl(cfg->cert_path, cfg->key_path);
#endif

#ifdef USE_LOGGING
    init_logging("app.log");
    LOG("Server starting on port %d", cfg->port);
#endif
}
```

### Include guard (repaso del patrón)

```c
// El uso más fundamental de compilación condicional:
#ifndef MYHEADER_H
#define MYHEADER_H

// contenido del header...

#endif // MYHEADER_H
```

## #error y #warning

```c
// #error — detiene la compilación con un mensaje:
#if !defined(__linux__) && !defined(__APPLE__)
    #error "This library only supports Linux and macOS"
#endif

#if BUFFER_SIZE < 64
    #error "BUFFER_SIZE must be at least 64"
#endif

// Verificar que se definieron las macros necesarias:
#ifndef API_KEY
    #error "Define API_KEY with -DAPI_KEY=... at compile time"
#endif
```

```c
// #warning — emite un warning pero continúa compilando:
// (extensión de GCC/Clang, estandarizada en C23)
#ifdef USE_OLD_API
    #warning "USE_OLD_API is deprecated — migrate to new API"
#endif

#if CACHE_SIZE > 1048576
    #warning "CACHE_SIZE > 1 MB may cause memory issues"
#endif

// En C23, #warning es estándar.
// En C11/C17 con GCC/Clang, funciona como extensión.
// MSVC usa: #pragma message("warning text")
```

## #if 0 — Comentar bloques de código

```c
// #if 0 es la forma más segura de deshabilitar código:

#if 0
    // Este código NO se compila.
    // A diferencia de /* ... */, se puede anidar:
    /* comentario normal */
    #ifdef FOO
    printf("esto no existe\n");
    #endif
#endif

// Los comentarios /* */ NO se pueden anidar:
/*
    /* esto es un error */    ← el primer */ cierra el comentario
    printf("esto SÍ se compila");
*/

// #if 0 es la herramienta correcta para deshabilitar
// bloques grandes que pueden contener comentarios.
```

```c
// Patrón para alternar entre dos implementaciones:
#if 1    // cambiar a 0 para usar la versión alternativa
    // Implementación A (activa)
    int compute(int x) { return x * x; }
#else
    // Implementación B (inactiva)
    int compute(int x) { return x * x + 1; }
#endif
```

## Compilación condicional vs runtime

```c
// COMPILACIÓN CONDICIONAL — código eliminado en compilación:
#ifdef DEBUG
    validate_state();    // no existe en release — cero overhead
#endif

// RUNTIME — código siempre presente, se evalúa en ejecución:
if (debug_enabled) {
    validate_state();    // siempre compilado — branch en runtime
}

// Usar compilación condicional cuando:
// - El código no debe existir en ciertos builds
// - Depende de la plataforma o el compilador
// - Afecta tipos o structs (no se pueden cambiar en runtime)
// - El overhead importa (hot paths)

// Usar runtime cuando:
// - El usuario puede cambiar la configuración sin recompilar
// - El mismo binario debe funcionar en múltiples modos
// - La lógica es simple (un if no cuesta nada)
```

## Errores comunes

```c
// 1. Olvidar #endif:
#ifdef DEBUG
    printf("debug\n");
// ERROR: #endif faltante → error de compilación confuso
// "unterminated #ifdef"

// 2. #elif vs #else if:
#if X == 1
    // ...
#else if X == 2    // MAL — esto es #else seguido de "if X == 2"
    // ...          //   que es código normal dentro del #else
#endif

#if X == 1
    // ...
#elif X == 2       // BIEN — #elif es la directiva correcta
    // ...
#endif

// 3. Macro no definido en #if (silenciosamente vale 0):
#if FEATURE_X    // si FEATURE_X no está definido → vale 0 → FALSE
    // ...       // no hay error ni warning → bug silencioso
#endif

// Mejor: ser explícito:
#if defined(FEATURE_X) && FEATURE_X
    // ...
#endif

// O con -Wundef (GCC/Clang) para detectar macros no definidos en #if.
```

---

## Ejercicios

### Ejercicio 1 — Sistema de logging multi-nivel

```c
// Implementar un sistema con 4 niveles: ERROR, WARN, INFO, DEBUG.
// Controlado por -DLOG_LEVEL=N (0=off, 1=error, 2=warn, 3=info, 4=debug).
// Cada macro LOG_X debe imprimir: [NIVEL] archivo:línea: mensaje
// Si LOG_LEVEL no está definido, default a 1 (solo errores).
// Probar compilando con distintos niveles y verificar qué se imprime.
```

### Ejercicio 2 — Portabilidad

```c
// Escribir un programa que imprima:
// - Plataforma (Linux/macOS/Windows/unknown)
// - Compilador y versión
// - Arquitectura (x86_64/ARM64/etc.)
// - Estándar de C (__STDC_VERSION__)
// - Tamaño de puntero (sizeof(void*))
// Usar compilación condicional para cada detección.
// Compilar y verificar que la detección es correcta.
```

### Ejercicio 3 — Feature flags

```c
// Crear un programa con 3 features opcionales:
// - USE_COLOR: printf con colores ANSI
// - USE_VERBOSE: mensajes detallados
// - USE_TIMING: medir tiempo con clock_gettime
// Cada feature agrega funcionalidad cuando se compila con -DUSE_X.
// El programa debe funcionar con cualquier combinación de features.
// Probar: gcc prog.c, gcc -DUSE_COLOR -DUSE_TIMING prog.c, etc.
```
