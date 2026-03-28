# T03 — Macros predefinidas

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 183 | `typedef char static_assert_##__LINE__[...]` — el operador `##` **impide** la expansión de sus operandos, así que `__LINE__` se pega literalmente como texto. Todas las invocaciones generan el mismo nombre `static_assert___LINE__` y colisionan. | Se necesita doble indirección: `#define CONCAT_(a,b) a##b` / `#define CONCAT(a,b) CONCAT_(a,b)` y usar `CONCAT(static_assert_, __LINE__)`. Esto es el mismo patrón que `counter_demo.c` usa para `UNIQUE_NAME`. |

---

## 1. Macros estándar del compilador C

El estándar C garantiza que todo compilador conforme define estas macros
automáticamente, sin necesidad de `#include` ni `#define`:

| Macro | Tipo | Descripción |
|-------|------|-------------|
| `__FILE__` | `const char[]` | Nombre del archivo fuente actual |
| `__LINE__` | `int` | Número de línea donde aparece |
| `__DATE__` | `const char[]` | Fecha de compilación (`"Mmm dd yyyy"`) |
| `__TIME__` | `const char[]` | Hora de compilación (`"hh:mm:ss"`) |
| `__STDC__` | `int` | 1 si el compilador cumple el estándar C |
| `__STDC_VERSION__` | `long` | Versión del estándar (p.ej. `201112L` = C11) |
| `__STDC_HOSTED__` | `int` | 1 si es entorno hosted (con OS y libc completa) |

Estas macros se expanden en el lugar donde aparecen. `__LINE__` en la línea
19 del fuente vale 19, no importa si está dentro de `main` o de otra función.

```c
#include <stdio.h>

int main(void) {
    printf("File: %s\n", __FILE__);            // "main.c"
    printf("Line: %d\n", __LINE__);            // 5
    printf("Date: %s\n", __DATE__);            // "Mar 26 2026"
    printf("Time: %s\n", __TIME__);            // "14:30:05"
    printf("STDC: %d\n", __STDC__);            // 1
    printf("Version: %ld\n", __STDC_VERSION__);// 201112 con -std=c11
    return 0;
}
```

**Detalle importante**: `__FILE__` se expande a la cadena exacta que se pasó a
`gcc` como argumento. Si compilas con `gcc main.c`, da `"main.c"`. Si compilas
con `gcc ./src/main.c`, da `"./src/main.c"`. Esto afecta qué ven los usuarios
en mensajes de error y logs.

---

## 2. `__func__` — variable implícita, no macro

`__func__` (C99) parece un macro predefinido, pero técnicamente es una
**variable local implícita** que el compilador inyecta en cada función:

```c
// El compilador genera algo equivalente a:
static const char __func__[] = "nombre_de_la_funcion";
```

Esto tiene consecuencias prácticas:

```c
void process_data(int *data) {
    printf("Entering %s\n", __func__);   // "process_data"
}

int main(void) {
    printf("In %s\n", __func__);          // "main"
    return 0;
}
```

**No se puede usar con `#` ni `##`** porque esos operadores solo funcionan
con parámetros de macros, y `__func__` no es un macro:

```c
// Esto funciona — uso normal como valor:
#define LOG(msg) printf("%s: %s\n", __func__, msg)

// Esto NO funciona — ## necesita un token de macro:
#define TAG __func__ ## _tag   // ERROR: __func__ no es un macro
```

En contraste, `__FILE__` y `__LINE__` **sí** son macros y pueden usarse con
`#` y `##` (aunque rara vez se necesita).

---

## 3. `__FILE__` y `__LINE__` en macros: captura del punto de llamada

Este es el uso más poderoso de las macros predefinidas. Cuando `__FILE__` y
`__LINE__` aparecen dentro de la definición de un macro, se expanden **donde
se invoca el macro**, no donde se definió:

```c
// En una FUNCIÓN, __LINE__ siempre muestra la misma línea:
void log_msg(const char *msg) {
    printf("%s:%d: %s\n", __FILE__, __LINE__, msg);
    // Siempre imprime la línea de ESTA función, inútil para el caller.
}

// En un MACRO, __LINE__ se expande en el punto de invocación:
#define LOG(msg) printf("%s:%d: %s\n", __FILE__, __LINE__, msg)

// En main.c, línea 42:
LOG("error");    // → printf("main.c", 42, "error")
```

**Patrón clave**: macro que captura ubicación y delega a función:

```c
#define MALLOC(size) \
    malloc_tracked(size, __FILE__, __LINE__)

void *malloc_tracked(size_t size, const char *file, int line) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "%s:%d: malloc(%zu) failed\n", file, line, size);
        abort();
    }
    return p;
}

// Uso en main.c línea 50:
int *arr = MALLOC(100 * sizeof(int));
// Si falla → "main.c:50: malloc(400) failed"
// NO "tracker.c:6: malloc(400) failed"
```

Este patrón es la razón por la que `assert()`, los sistemas de logging y los
memory trackers de C son **macros** y no funciones.

---

## 4. Patrones de logging y assert con ubicación

### Sistema de logging por niveles

```c
#define LOG(level, fmt, ...) \
    fprintf(stderr, "[%s] %s:%d %s(): " fmt "\n", \
            level, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_INFO(fmt, ...)  LOG("INFO",  fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG("WARN",  fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG("ERROR", fmt, __VA_ARGS__)
```

Nota: estas macros requieren **al menos un argumento variádico** porque C11
estándar exige al menos un argumento para `...`. Para permitir cero argumentos
se usaría `##__VA_ARGS__` (extensión GNU) o `__VA_OPT__` (C23).

### Assert personalizado

```c
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, \
            "Assertion failed: %s\n" \
            "  File:     %s\n" \
            "  Line:     %d\n" \
            "  Function: %s\n", \
            #cond, __FILE__, __LINE__, __func__); \
        abort(); \
    } \
} while (0)
```

- `#cond` stringifica la expresión: `ASSERT(x > 0)` produce `"x > 0"`.
- `abort()` envía SIGABRT → código de salida 134 (128 + 6).
- El `assert()` de `<assert.h>` usa exactamente este mecanismo internamente.

### Error tracking con retorno

```c
#define RETURN_ERROR(code, fmt, ...) do { \
    LOG_ERROR(fmt, __VA_ARGS__); \
    return (code); \
} while (0)

int open_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        RETURN_ERROR(-1, "cannot open '%s': %s", path, strerror(errno));
    }
    // ...
}
// → [ERROR] config.c:5 open_config(): cannot open '/etc/app.conf': No such file
```

---

## 5. `__DATE__` y `__TIME__`: timestamps y reproducibilidad

Concatenación de string literals para build info:

```c
#define BUILD_INFO "Built on " __DATE__ " at " __TIME__
// → "Built on Mar 26 2026 at 14:30:05"
```

Esto funciona porque C concatena automáticamente string literals adyacentes
(`"abc" "def"` → `"abcdef"`).

### El problema de reproducibilidad

Cada compilación produce un binario diferente porque el timestamp cambia.
Esto rompe:
- **Build caching**: el binario nunca coincide con el cache.
- **Verificación**: `sha256sum` da un hash diferente cada vez.
- **Distribuciones**: no se puede verificar que el binario corresponde al fuente.

### Soluciones

```c
// 1. Warning del compilador:
//    gcc -Wdate-time prog.c
//    → warning: macro "__DATE__" might prevent reproducible builds

// 2. Definir timestamps externamente:
//    gcc -DBUILD_DATE=\"$(date +%Y-%m-%d)\" prog.c

// 3. SOURCE_DATE_EPOCH (convención para builds reproducibles):
//    export SOURCE_DATE_EPOCH=$(date +%s)
//    Algunos compiladores respetan esta variable y la usan en lugar
//    del reloj del sistema para __DATE__ y __TIME__.

// 4. No usar __DATE__/__TIME__ en producción — la opción más simple.
```

---

## 6. `__STDC_VERSION__`: detección del estándar y fallbacks

| Valor | Estándar |
|-------|----------|
| No definido | C89/C90 |
| `199901L` | C99 |
| `201112L` | C11 |
| `201710L` | C17 |
| `202311L` | C23 |

Permite escribir código que se adapta al estándar disponible:

```c
#if __STDC_VERSION__ >= 201112L
    // C11+: usar _Static_assert nativo
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    // Fallback para C99: typedef con array de tamaño negativo
    // IMPORTANTE: necesita doble indirección para expandir __LINE__
    #define CONCAT_(a, b) a ## b
    #define CONCAT(a, b)  CONCAT_(a, b)
    #define STATIC_ASSERT(cond, msg) \
        typedef char CONCAT(static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif
```

**Por qué la doble indirección**: el operador `##` impide que sus operandos
se expandan como macros. Si escribes `static_assert_##__LINE__`, obtienes
literalmente `static_assert___LINE__` — la cadena de texto `__LINE__`, no
el número de línea. Con `CONCAT(static_assert_, __LINE__)`, primero se
expande `__LINE__` a (por ejemplo) `42`, y luego `CONCAT_(static_assert_, 42)`
produce `static_assert_42`. Este patrón de doble indirección es necesario
siempre que se use `##` con un argumento que debe expandirse primero.

Otros fallbacks comunes:

```c
// _Noreturn (C11) vs __attribute__((noreturn)) (GCC)
#if __STDC_VERSION__ >= 201112L
    #define NORETURN _Noreturn
#elif defined(__GNUC__)
    #define NORETURN __attribute__((noreturn))
#else
    #define NORETURN
#endif

// _Thread_local (C11) vs __thread (GCC)
#if __STDC_VERSION__ >= 201112L
    #define THREAD_LOCAL _Thread_local
#elif defined(__GNUC__)
    #define THREAD_LOCAL __thread
#else
    #define THREAD_LOCAL  // no-op: sin soporte
#endif
```

---

## 7. Extensiones GCC/Clang: `__GNUC__`, `__SIZEOF_*__`, `__BYTE_ORDER__`

Estas macros **no son estándar** pero son ubicuas en código Unix/Linux:

```c
// Versión del compilador:
#ifdef __GNUC__
    #define GCC_VERSION \
        (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #if GCC_VERSION >= 40800
        // Usar features de GCC 4.8+
    #endif
#endif
```

**Cuidado**: Clang también define `__GNUC__` para compatibilidad. Para
distinguirlos, verifica `__clang__` **primero**:

```c
#if defined(__clang__)
    // Clang (también define __GNUC__)
#elif defined(__GNUC__)
    // GCC genuino
#endif
```

Detección de plataforma y arquitectura:

```c
// Tamaño de tipos — conocido en compilación, sin sizeof():
#if __SIZEOF_POINTER__ == 8
    #define ARCH_64BIT 1
#elif __SIZEOF_POINTER__ == 4
    #define ARCH_32BIT 1
#endif

// Endianness:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define IS_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define IS_BIG_ENDIAN 1
#endif

// Plataforma:
// __linux__     → Linux
// __APPLE__     → macOS/iOS
// _WIN32        → Windows (32 y 64 bits)
// __x86_64__    → x86-64
// __aarch64__   → ARM64
```

### `__STDC_HOSTED__`

Distingue entornos **hosted** (con OS completo) de **freestanding** (kernels,
embedded, bootloaders):

```c
#if __STDC_HOSTED__ == 1
    // Hosted: main(), <stdio.h>, <stdlib.h>, etc. disponibles
#else
    // Freestanding: solo <stddef.h>, <stdint.h>, <float.h>,
    // <limits.h>, <stdarg.h>, <stdbool.h>, <stdnoreturn.h>
    // No hay printf, malloc, fopen, etc.
#endif
```

---

## 8. `__COUNTER__` y generación de identificadores únicos

`__COUNTER__` (extensión GCC/Clang) es un entero que empieza en 0 y se
incrementa con **cada uso** en la unidad de traducción:

```c
printf("%d\n", __COUNTER__);  // 0
printf("%d\n", __COUNTER__);  // 1
printf("%d\n", __COUNTER__);  // 2
```

Es global al translation unit — cualquier macro que use `__COUNTER__`
consume el siguiente valor, sin importar cuál macro sea.

### Generación de nombres únicos

Para usarlo con `##` (token pasting), necesita doble indirección:

```c
#define CONCAT_INNER(a, b)  a ## b
#define CONCAT(a, b)        CONCAT_INNER(a, b)
#define UNIQUE_NAME(prefix) CONCAT(prefix, __COUNTER__)

int UNIQUE_NAME(tmp_) = 100;   // int tmp_0 = 100;
int UNIQUE_NAME(tmp_) = 200;   // int tmp_1 = 200;
// Sin colisión: cada variable tiene un nombre diferente.
```

Es la misma doble indirección que `__LINE__` necesita con `##`. La razón
es la misma: `##` impide la expansión de macros en sus operandos.

### Tracing con numeración automática

```c
#define TRACE(msg) \
    printf("[trace #%d] %s:%d: %s\n", \
           __COUNTER__, __FILE__, __LINE__, msg)

TRACE("start");    // [trace #0] main.c:10: start
TRACE("process");  // [trace #1] main.c:11: process
TRACE("done");     // [trace #2] main.c:12: done
```

**Limitación**: `__COUNTER__` no es estándar. Con `-Wpedantic` da warning.
`__LINE__` es una alternativa estándar para nombres únicos, aunque no
garantiza unicidad si dos macros se invocan en la misma línea.

---

## 9. `__attribute__` y macros de portabilidad

Los `__attribute__` de GCC/Clang no son macros predefinidas, pero se usan
junto con ellas para crear APIs portables:

```c
// noreturn — función que nunca retorna:
__attribute__((noreturn))
void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}

// format(printf, N, M) — el compilador verifica los argumentos:
__attribute__((format(printf, 1, 2)))
void log_msg(const char *fmt, ...) { /* ... */ }

log_msg("%d\n", "hello");
// WARNING: format '%d' expects int, not char*

// deprecated — aviso al usar la función:
__attribute__((deprecated("use new_func instead")))
void old_func(void) { }

// unused — suprime warning de variable no usada:
__attribute__((unused))
int debug_counter = 0;
```

### Macros de portabilidad

Envolver `__attribute__` en macros que se vacían en compiladores no-GCC:

```c
#ifdef __GNUC__
    #define NORETURN       __attribute__((noreturn))
    #define PRINTF_FMT(a,b) __attribute__((format(printf, a, b)))
    #define DEPRECATED(msg) __attribute__((deprecated(msg)))
    #define UNUSED          __attribute__((unused))
#else
    #define NORETURN
    #define PRINTF_FMT(a,b)
    #define DEPRECATED(msg)
    #define UNUSED
#endif

// Uso portable:
NORETURN void die(const char *msg);
PRINTF_FMT(1, 2) void log_msg(const char *fmt, ...);
```

En compiladores no-GCC, las macros se expanden a nada y el código compila
sin error, solo pierde las verificaciones extras.

---

## 10. `gcc -dM -E`: explorar las macros predefinidas

El comando `gcc -dM -E - < /dev/null` lista **todas** las macros predefinidas
del compilador sin necesidad de un archivo fuente:

```bash
# Contar cuántas hay:
gcc -std=c11 -dM -E - < /dev/null | wc -l
# ~250 macros

# Ver macros del estándar C:
gcc -std=c11 -dM -E - < /dev/null | grep "^#define __STDC"
# __STDC__ 1
# __STDC_HOSTED__ 1
# __STDC_VERSION__ 201112L
# ...

# Ver macros de plataforma y arquitectura:
gcc -std=c11 -dM -E - < /dev/null | grep -E "__(linux|x86_64|SIZEOF|BYTE_ORDER|GNUC)" | sort

# Comparar macros entre estándares:
diff <(gcc -std=c99 -dM -E - < /dev/null | sort) \
     <(gcc -std=c11 -dM -E - < /dev/null | sort)
# __STDC_VERSION__ cambia, C11 puede definir macros adicionales
```

Desglose del comando:
- `-dM`: dump macros (solo mostrar `#define`, no el código preprocesado)
- `-E`: solo preprocesar (no compilar)
- `- < /dev/null`: leer de stdin (vacío) — muestra solo macros predefinidas,
  sin las que vendrían de archivos de include.

### Comparación con Rust

En Rust, la información equivalente se obtiene con `cfg`:

```rust
// Rust usa cfg en lugar de macros predefinidas:
#[cfg(target_os = "linux")]
fn platform_specific() { }

#[cfg(target_pointer_width = "64")]
fn arch_specific() { }

// file!(), line!(), column!() son macros procedurales (no texto):
println!("{}:{}", file!(), line!());

// env!() lee variables de entorno en compilación:
const VERSION: &str = env!("CARGO_PKG_VERSION");
```

La diferencia fundamental: en C las macros predefinidas son texto que se
sustituye antes de la compilación. En Rust, `cfg`, `file!()`, `line!()` son
parte del sistema de tipos y se verifican en compilación.

---

## Ejercicios

### Ejercicio 1 — Macros estándar básicas

Escribe un programa que imprima `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`,
`__STDC__` y `__STDC_VERSION__`.

```c
#include <stdio.h>

int main(void) {
    printf("File:    %s\n", __FILE__);
    printf("Line:    %d\n", __LINE__);
    printf("Date:    %s\n", __DATE__);
    printf("Time:    %s\n", __TIME__);
    printf("STDC:    %d\n", __STDC__);
    printf("Version: %ld\n", __STDC_VERSION__);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
./ex01
```

<details><summary>Predicción</summary>

```
File:    ex01.c
Line:    5
Date:    <fecha de hoy>
Time:    <hora de compilación>
STDC:    1
Version: 201112
```

- `__FILE__` muestra `ex01.c` (el nombre exacto pasado a gcc).
- `__LINE__` vale 5 porque es la quinta línea del archivo.
- `__STDC_VERSION__` con `-std=c11` vale `201112` (el sufijo `L` es parte
  del tipo `long`, no se imprime).

</details>

---

### Ejercicio 2 — `__func__` en diferentes funciones

Escribe tres funciones (`alpha`, `beta`, `gamma_func`) que cada una imprima
su nombre con `__func__` y su número de línea con `__LINE__`. Llama a las
tres desde `main` (que también imprime su propio `__func__`).

```c
#include <stdio.h>

static void alpha(void) {
    printf("  Function: %-12s  Line: %d\n", __func__, __LINE__);
}

static void beta(void) {
    printf("  Function: %-12s  Line: %d\n", __func__, __LINE__);
}

static void gamma_func(void) {
    printf("  Function: %-12s  Line: %d\n", __func__, __LINE__);
}

int main(void) {
    printf("  Function: %-12s  Line: %d\n", __func__, __LINE__);
    alpha();
    beta();
    gamma_func();
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
./ex02
```

<details><summary>Predicción</summary>

```
  Function: main          Line: 17
  Function: alpha         Line: 4
  Function: beta          Line: 8
  Function: gamma_func    Line: 12
```

- `__func__` cambia según la función donde se encuentra — es una variable
  local implícita, no un macro global.
- `__LINE__` muestra la línea del archivo donde aparece el printf, dentro
  de cada función respectiva.
- Nótese que las funciones se llaman en orden (alpha, beta, gamma_func)
  pero los números de línea corresponden a donde están **definidas**, no
  al orden de llamada.

</details>

---

### Ejercicio 3 — Macro LOG con ubicación del caller

Define un macro `LOG(msg)` que imprima `[archivo:línea] función(): mensaje`.
Úsalo dentro de `main` y dentro de una función helper. Verifica que la
ubicación reportada es la del **caller** (donde se invoca el macro).

```c
#include <stdio.h>

#define LOG(msg) \
    printf("[%s:%d] %s(): %s\n", __FILE__, __LINE__, __func__, msg)

static void helper(void) {
    LOG("inside helper");
}

int main(void) {
    LOG("start of main");
    helper();
    LOG("end of main");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
./ex03
```

<details><summary>Predicción</summary>

```
[ex03.c:11] main(): start of main
[ex03.c:7] helper(): inside helper
[ex03.c:13] main(): end of main
```

- Cada invocación de `LOG` muestra el archivo, línea y función **donde se
  escribe el macro**, no donde se definió (línea 4).
- `helper()` reporta línea 7 y función `helper`, no la línea 12 de main
  que la llamó — porque `__LINE__` y `__func__` se expanden dentro de
  `helper`, no en `main`.
- Si `LOG` fuera una función en vez de macro, todas las líneas mostrarían
  la misma ubicación (la de la función log).

</details>

---

### Ejercicio 4 — Custom ASSERT con stringificación

Implementa un macro `MY_ASSERT(cond)` que al fallar imprima la condición
como texto (usando `#cond`), el archivo, línea y función. Pruébalo con una
condición que pase y una que falle.

```c
#include <stdio.h>
#include <stdlib.h>

#define MY_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, \
            "Assertion failed: %s\n" \
            "  File:     %s\n" \
            "  Line:     %d\n" \
            "  Function: %s\n", \
            #cond, __FILE__, __LINE__, __func__); \
        abort(); \
    } \
} while (0)

int main(void) {
    int x = 42;
    MY_ASSERT(x > 0);    // pasa
    printf("First assert passed.\n");

    MY_ASSERT(x < 0);    // falla
    printf("This line is never reached.\n");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
./ex04 2>&1
echo "Exit code: $?"
```

<details><summary>Predicción</summary>

```
First assert passed.
Assertion failed: x < 0
  File:     ex04.c
  Line:     21
  Function: main
```

Seguido de un exit code de **134** (128 + SIGABRT = 128 + 6).

- La primera aserción (`x > 0` con x=42) pasa sin problema.
- La segunda (`x < 0` con x=42) falla: `#cond` produce `"x < 0"`.
- `abort()` envía SIGABRT al proceso, terminando con código 134.
- El printf de "This line is never reached" nunca se ejecuta.

</details>

---

### Ejercicio 5 — RETURN_ERROR con logging

Crea un macro `RETURN_ERROR(code, fmt, ...)` que loguee un error con archivo,
línea y función, y luego retorne `code`. Úsalo en una función que intente
abrir un archivo inexistente.

```c
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define RETURN_ERROR(code, fmt, ...) do { \
    fprintf(stderr, "[ERROR] %s:%d %s(): " fmt "\n", \
            __FILE__, __LINE__, __func__, __VA_ARGS__); \
    return (code); \
} while (0)

static int load_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        RETURN_ERROR(-1, "cannot open '%s': %s", path, strerror(errno));
    }
    printf("Config loaded from %s\n", path);
    fclose(f);
    return 0;
}

int main(void) {
    int rc = load_config("/nonexistent/file.txt");
    printf("load_config returned: %d\n", rc);

    rc = load_config("/dev/null");
    printf("load_config returned: %d\n", rc);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
./ex05 2>&1
```

<details><summary>Predicción</summary>

```
[ERROR] ex05.c:14 load_config(): cannot open '/nonexistent/file.txt': No such file or directory
load_config returned: -1
Config loaded from /dev/null
load_config returned: 0
```

- La primera llamada falla: `fopen` retorna NULL, `strerror(errno)` da
  "No such file or directory". `RETURN_ERROR` loguea a stderr y retorna -1.
- La línea reportada (14) es donde se invoca `RETURN_ERROR` dentro de
  `load_config`, no donde se definió el macro.
- La segunda llamada con `/dev/null` tiene éxito (es un archivo válido),
  imprime "Config loaded" y retorna 0.

</details>

---

### Ejercicio 6 — Build info embebido

Escribe un programa que detecte e imprima: compilador y versión (GCC o
Clang), estándar C, plataforma, arquitectura, tamaño de puntero, y si es
build debug o release (basado en `NDEBUG`).

```c
#include <stdio.h>

#define BUILD_TIMESTAMP "Built on " __DATE__ " at " __TIME__

int main(void) {
    printf("=== Build Info ===\n\n");

#if defined(__clang__)
    printf("Compiler: Clang %d.%d.%d\n",
           __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("Compiler: GCC %d.%d.%d\n",
           __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    printf("Compiler: unknown\n");
#endif

#if __STDC_VERSION__ >= 201710L
    printf("Standard: C17\n");
#elif __STDC_VERSION__ >= 201112L
    printf("Standard: C11\n");
#elif __STDC_VERSION__ >= 199901L
    printf("Standard: C99\n");
#else
    printf("Standard: C89/C90\n");
#endif

#if defined(__linux__)
    printf("Platform: Linux\n");
#elif defined(__APPLE__)
    printf("Platform: macOS\n");
#elif defined(_WIN32)
    printf("Platform: Windows\n");
#endif

#ifdef __SIZEOF_POINTER__
    printf("Pointer:  %d bytes (%d-bit)\n",
           __SIZEOF_POINTER__, __SIZEOF_POINTER__ * 8);
#endif

#ifdef NDEBUG
    printf("Build:    release\n");
#else
    printf("Build:    debug\n");
#endif

    printf("\n%s\n", BUILD_TIMESTAMP);
    return 0;
}
```

Compila dos veces y compara:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
./ex06

gcc -std=c11 -Wall -Wextra -Wpedantic -DNDEBUG ex06.c -o ex06_release
./ex06_release | grep "Build:"
```

<details><summary>Predicción</summary>

Primera ejecución (sin NDEBUG):
```
=== Build Info ===

Compiler: GCC <version>
Standard: C11
Platform: Linux
Pointer:  8 bytes (64-bit)
Build:    debug

Built on <fecha> at <hora>
```

Segunda ejecución (con `-DNDEBUG`):
```
Build:    release
```

- Clang se verifica antes que GCC porque Clang define ambos `__clang__` y
  `__GNUC__`. Si no se hiciera este orden, Clang se reportaría como GCC.
- Con `-std=c11`, `__STDC_VERSION__` es `201112L`, que pasa `>= 201112L`
  pero no `>= 201710L`, así que imprime "C11".
- En Linux x86-64, el puntero es de 8 bytes.
- `-DNDEBUG` equivale a `#define NDEBUG` al inicio del archivo.

</details>

---

### Ejercicio 7 — STATIC_ASSERT con fallback correcto

Implementa un `STATIC_ASSERT` que use `_Static_assert` en C11+ y un fallback
con typedef para C99. El fallback **debe** usar doble indirección para que
`__LINE__` se expanda correctamente. Prueba con ambos estándares.

```c
#include <stdio.h>

/* Double indirection for token pasting with __LINE__ */
#define CONCAT_(a, b) a ## b
#define CONCAT(a, b)  CONCAT_(a, b)

#if __STDC_VERSION__ >= 201112L
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    /* Fallback: typedef array with negative size triggers compile error */
    #define STATIC_ASSERT(cond, msg) \
        typedef char CONCAT(static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* These should all pass: */
STATIC_ASSERT(sizeof(int) >= 2, "int must be at least 2 bytes");
STATIC_ASSERT(sizeof(char) == 1, "char is always 1 byte");
STATIC_ASSERT(sizeof(long) >= sizeof(int), "long >= int");

int main(void) {
    STATIC_ASSERT(sizeof(void *) >= 4, "need at least 32-bit pointers");
    printf("All static assertions passed.\n");
    printf("Compiled with C standard: %ld\n", __STDC_VERSION__);
    return 0;
}
```

```bash
# Con C11 (usa _Static_assert nativo):
gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
./ex07

# Con C99 (usa el fallback con typedef):
gcc -std=c99 -Wall -Wextra -Wpedantic ex07.c -o ex07_c99
./ex07_c99
```

<details><summary>Predicción</summary>

Ambas compilaciones pasan sin error:
```
All static assertions passed.
Compiled with C standard: 201112
```
(Con C99: `199901` en lugar de `201112`.)

- En C11, `_Static_assert(sizeof(int) >= 2, "...")` es nativo del lenguaje.
- En C99, el fallback genera (por ejemplo):
  `typedef char static_assert_17[1];` — si la condición es verdadera,
  tamaño 1, válido. Si fuera falsa, tamaño -1, error de compilación.
- Sin la doble indirección (`CONCAT` → `CONCAT_`), el nombre sería
  `static_assert___LINE__` para TODAS las invocaciones, causando
  "conflicting types" en la segunda invocación. La doble indirección
  produce `static_assert_17`, `static_assert_18`, etc.
- Para verificar que falla, cambia una condición: `sizeof(int) >= 16`
  debería dar error en compilación.

</details>

---

### Ejercicio 8 — `__COUNTER__` para tracing y nombres únicos

Usa `__COUNTER__` para crear un macro `TRACE(msg)` que numere cada punto
de traza, y un macro `UNIQUE_VAR(prefix)` que genere variables sin colisión.

```c
#include <stdio.h>

#define CONCAT_INNER(a, b)   a ## b
#define CONCAT(a, b)         CONCAT_INNER(a, b)
#define UNIQUE_VAR(prefix)   CONCAT(prefix, __COUNTER__)

#define TRACE(msg) \
    printf("[trace #%d] %s:%d: %s\n", __COUNTER__, __FILE__, __LINE__, msg)

int main(void) {
    TRACE("program start");
    TRACE("initializing");
    TRACE("ready");

    int UNIQUE_VAR(val_) = 10;
    int UNIQUE_VAR(val_) = 20;
    int UNIQUE_VAR(val_) = 30;

    printf("\nval_3=%d, val_4=%d, val_5=%d\n", val_3, val_4, val_5);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra counter_ex.c -o counter_ex
./counter_ex
```

(Nota: se omite `-Wpedantic` porque `__COUNTER__` es extensión GCC/Clang.)

<details><summary>Predicción</summary>

```
[trace #0] counter_ex.c:11: program start
[trace #1] counter_ex.c:12: initializing
[trace #2] counter_ex.c:13: ready

val_3=10, val_4=20, val_5=30
```

- `__COUNTER__` es global al translation unit. Las tres llamadas a `TRACE`
  consumen los valores 0, 1, 2.
- Las tres llamadas a `UNIQUE_VAR(val_)` continúan desde 3: generan
  `val_3`, `val_4`, `val_5`.
- Si se usara `__LINE__` en vez de `__COUNTER__`, los nombres dependerían
  del número de línea (como `val_14`, `val_15`, `val_16`) — funciona,
  pero falla si dos macros se invocan en la misma línea.

</details>

---

### Ejercicio 9 — `__attribute__` portables

Crea un header con macros de portabilidad para `noreturn`, `format(printf)`,
`deprecated` y `unused`. Úsalas en funciones y verifica que GCC da los
warnings esperados.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* --- Portability macros --- */
#ifdef __GNUC__
    #define NORETURN       __attribute__((noreturn))
    #define PRINTF_FMT(a,b) __attribute__((format(printf, a, b)))
    #define DEPRECATED(msg) __attribute__((deprecated(msg)))
    #define UNUSED          __attribute__((unused))
#else
    #define NORETURN
    #define PRINTF_FMT(a,b)
    #define DEPRECATED(msg)
    #define UNUSED
#endif

/* --- Functions using the macros --- */

NORETURN void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}

PRINTF_FMT(1, 2)
void log_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

DEPRECATED("use new_api() instead")
void old_api(void) {
    printf("old API called\n");
}

void new_api(void) {
    printf("new API called\n");
}

int main(void) {
    UNUSED int debug_flag = 0;

    log_msg("Starting with value %d\n", 42);

    /* Descomentar para ver warnings: */
    /* log_msg("bad format %d\n", "hello");  // format mismatch */
    /* old_api();                             // deprecated warning */

    new_api();
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
./ex09 2>&1
```

<details><summary>Predicción</summary>

Compilación: sin warnings (las líneas con format mismatch y deprecated
están comentadas, y `debug_flag` tiene `UNUSED`).

Salida:
```
Starting with value 42
new API called
```

- `log_msg` imprime a stderr: "Starting with value 42\n" — se ve en la
  salida gracias a `2>&1`.
- Si descomentaras `log_msg("bad format %d\n", "hello")`, GCC daría:
  `warning: format '%d' expects 'int', but argument 2 has type 'char *'`
  — esto es `format(printf, 1, 2)` en acción.
- Si descomentaras `old_api()`, GCC daría:
  `warning: 'old_api' is deprecated: use new_api() instead`.
- Sin `UNUSED`, `debug_flag` daría `warning: unused variable`.
- En un compilador no-GCC, las macros se expandirían a nada, las funciones
  compilarían sin verificaciones extras, y `debug_flag` daría warning.

</details>

---

### Ejercicio 10 — `gcc -dM -E`: explorar macros predefinidas

Usa `gcc -dM -E` para responder estas preguntas sobre tu sistema. No
escribas código C — solo comandos de shell.

```bash
# a) ¿Cuántas macros predefinidas tiene tu GCC con -std=c11?
gcc -std=c11 -dM -E - < /dev/null | wc -l

# b) ¿Qué valor tiene __STDC_VERSION__ con -std=c11 vs -std=c99?
gcc -std=c11 -dM -E - < /dev/null | grep __STDC_VERSION__
gcc -std=c99 -dM -E - < /dev/null | grep __STDC_VERSION__

# c) ¿Tu sistema es little-endian o big-endian?
gcc -dM -E - < /dev/null | grep BYTE_ORDER

# d) ¿Cuántos bytes tiene un long double en tu sistema?
gcc -dM -E - < /dev/null | grep SIZEOF_LONG_DOUBLE

# e) ¿Qué macros cambian entre -std=c99 y -std=c11?
diff <(gcc -std=c99 -dM -E - < /dev/null | sort) \
     <(gcc -std=c11 -dM -E - < /dev/null | sort)
```

<details><summary>Predicción</summary>

a) Aproximadamente **250** macros (el número exacto varía por versión de GCC).

b)
```
#define __STDC_VERSION__ 201112L
#define __STDC_VERSION__ 199901L
```

c) En x86-64:
```
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
```
(x86 y x86-64 son siempre little-endian.)

d) En x86-64 Linux:
```
#define __SIZEOF_LONG_DOUBLE__ 16
```
(16 bytes = 128 bits, aunque solo 80 bits se usan para el valor;
el resto es padding de alineación.)

e) Principales diferencias:
- `__STDC_VERSION__` cambia de `199901L` a `201112L`.
- C11 puede definir macros adicionales como `__STDC_NO_THREADS__` (si no
  soporta threads) o `__STDC_NO_ATOMICS__`.
- Las macros de plataforma y arquitectura no cambian entre estándares.

</details>
