# T04 — Compilacion condicional

> Sin erratas detectadas en el material fuente.

---

## 1 — #ifdef y #ifndef: existencia de macros

`#ifdef` verifica si un macro **existe**, sin importar su valor. `#ifndef`
verifica que **no** exista:

```c
#ifdef DEBUG
    printf("Debug: x = %d\n", x);
#endif

#ifndef RELEASE
    printf("Not a release build\n");
#endif
```

El punto clave: el **valor** del macro es irrelevante para `#ifdef`:

```c
#define FEATURE_A          /* definido, valor vacio */
#define FEATURE_B 0        /* definido, valor 0 */
#define FEATURE_C 1        /* definido, valor 1 */

#ifdef FEATURE_A     /* TRUE — existe */
#ifdef FEATURE_B     /* TRUE — existe (aunque vale 0) */
#ifdef FEATURE_C     /* TRUE — existe */

/* Para verificar el VALOR, usar #if: */
#if FEATURE_B        /* FALSE — vale 0 */
#if FEATURE_C        /* TRUE  — vale 1 */
```

Esta distincion es fundamental:
- `#ifdef` responde: "fue definido?"
- `#if` responde: "su valor es verdadero (no-cero)?"

Un `#define FOO 0` pasa `#ifdef` pero no `#if`.

---

## 2 — #if defined(): la forma flexible

`#ifdef` solo acepta **un** macro. `defined()` se puede usar dentro de `#if`
y combinar con operadores logicos:

```c
/* Esto NO se puede hacer con #ifdef: */
#if defined(FEATURE_A) && defined(FEATURE_B)
    /* ambas features activas */
#endif

#if defined(LINUX) || defined(MACOS) || defined(BSD)
    /* cualquier Unix */
#endif

/* #ifdef FEATURE_A && FEATURE_B ← no funciona como se espera */
/* GCC toma solo el primer identificador e ignora el resto con warning */
```

`defined()` es equivalente a `#ifdef` para un solo macro:

```c
#if defined(DEBUG)      /* equivale a #ifdef DEBUG */
#if !defined(NDEBUG)    /* equivale a #ifndef NDEBUG */
```

La ventaja aparece cuando necesitas expresiones complejas:

```c
#if defined(OPT_A) && defined(OPT_B)
    printf("Both options\n");
#elif defined(OPT_A)
    printf("Only OPT_A\n");
#elif defined(OPT_B)
    printf("Only OPT_B\n");
#else
    printf("Neither\n");
#endif
```

---

## 3 — #if, #elif, #else, #endif: cadenas condicionales

`#if` evalua **expresiones constantes enteras** en tiempo de preprocesamiento:

```c
#if LOG_LEVEL == 3
    printf("ERROR + WARN + INFO\n");
#elif LOG_LEVEL == 2
    printf("ERROR + WARN\n");
#elif LOG_LEVEL == 1
    printf("ERROR only\n");
#else
    printf("Logging: OFF\n");
#endif
```

Operadores permitidos en `#if`:
- Aritmeticos: `+`, `-`, `*`, `/`, `%`
- Comparacion: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logicos: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Ternario: `? :`
- `defined()`: verifica existencia de macro

**No** se puede usar: `sizeof`, casts, variables, llamadas a funciones. Solo
constantes enteras y macros que se expandan a constantes.

```c
/* Verificacion de version: */
#if (VERSION_MAJOR > 2) || (VERSION_MAJOR == 2 && VERSION_MINOR >= 5)
    /* version 2.5 o superior */
#endif

/* Validacion de rango: */
#ifndef BUFSIZE
#define BUFSIZE 256
#endif

#if BUFSIZE < 64
    #error "BUFSIZE too small"
#elif BUFSIZE <= 4096
    /* rango razonable */
#else
    /* buffer muy grande */
#endif
```

---

## 4 — gcc -D y -U: definir macros desde la linea de compilacion

`-D` define un macro antes de procesar el archivo, como si hubiera un `#define`
al inicio:

```bash
gcc -DDEBUG prog.c            # equivale a: #define DEBUG 1
gcc -DDEBUG=2 prog.c          # equivale a: #define DEBUG 2
gcc -DVERSION=\"1.0\" prog.c  # equivale a: #define VERSION "1.0"
gcc -UNDEBUG prog.c           # equivale a: #undef NDEBUG
```

`-D` sin valor asigna `1`. `-U` elimina una definicion (util para cancelar
macros que otros archivos definen).

Patron tipico — valores por defecto con `#ifndef`:

```c
/* Si no se pasa -DLEVEL=N, usar 0 por defecto: */
#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

/* Si no se pasa -DBUFSIZE=N, usar 256: */
#ifndef BUFSIZE
#define BUFSIZE 256
#endif
```

Esto permite que el usuario configure el build desde la linea de compilacion
sin modificar el codigo fuente. Los defaults se aplican cuando no se pasa
ningun `-D`.

---

## 5 — Macros no definidos en #if: la trampa silenciosa

Un macro **no definido** en `#if` silenciosamente vale **0**:

```c
/* MYSTERY_MACRO nunca fue definido: */
#if MYSTERY_MACRO    /* → #if 0 → FALSE */
    printf("active\n");
#else
    printf("inactive\n");   /* esta rama se ejecuta */
#endif
```

No hay error ni warning por defecto. Es imposible distinguir "definido con
valor 0" de "no definido" usando solo `#if`. El peligro: un typo como
`#if FETURE_X` (sin la A) pasa silenciosamente como 0.

Soluciones:

```c
/* 1. Ser explicito con defined(): */
#if defined(FEATURE_X) && FEATURE_X
    /* definido Y no-cero */
#elif defined(FEATURE_X)
    /* definido pero vale 0 */
#else
    /* no definido en absoluto */
#endif

/* 2. Usar -Wundef para detectar macros no definidos en #if: */
```

```bash
gcc -Wundef prog.c
# warning: "MYSTERY_MACRO" is not defined, evaluates to 0 [-Wundef]
```

`-Wundef` no esta incluido en `-Wall` ni `-Wextra`. Hay que habilitarlo
explicitamente. Es una buena practica para proyectos reales.

---

## 6 — NDEBUG y assert()

`assert()` (de `<assert.h>`) verifica una condicion en runtime y aborta el
programa si es falsa:

```c
#include <assert.h>

int divide(int a, int b) {
    assert(b != 0 && "divisor must not be zero");
    return a / b;
}
```

El truco `&& "mensaje"`: como el string literal es un puntero no-nulo (truthy),
no cambia la logica del assert, pero aparece en el mensaje de error:

```
prog: prog.c:4: divide: Assertion `b != 0 && "divisor must not be zero"' failed.
Aborted (core dumped)
```

**NDEBUG desactiva todos los asserts:**

```c
/* gcc -DNDEBUG prog.c */
/* Con NDEBUG definido ANTES de #include <assert.h>,
   assert(expr) se expande a ((void)0) — desaparece. */
```

La consecuencia critica — **nunca poner side effects en assert**:

```c
/* MAL: */
assert(++counter > 0);
/* Con NDEBUG, ++counter desaparece. El programa se comporta
   diferente en debug vs release. */

/* BIEN: separar el side effect: */
++counter;
assert(counter > 0);
/* Con NDEBUG, el ++counter sigue ejecutandose. */
```

`assert()` es para desarrollo y testing, no para validacion en produccion. En
produccion (con `-DNDEBUG`), las verificaciones desaparecen.

---

## 7 — Feature test macros

Las feature test macros desbloquean funciones adicionales en los headers del
sistema. Deben definirse **ANTES** de cualquier `#include`:

```c
/* ANTES de cualquier include: */
#define _POSIX_C_SOURCE 200809L   /* POSIX.1-2008 */
/* O: */
#define _GNU_SOURCE               /* extensiones GNU (incluye POSIX) */

#include <stdio.h>
#include <string.h>
```

`_POSIX_C_SOURCE` desbloquea funciones POSIX segun la version:
- `200112L` → POSIX.1-2001 (pthreads, readlink, etc.)
- `200809L` → POSIX.1-2008 (strnlen, getline, etc.)

`_GNU_SOURCE` es un superset: activa extensiones GNU **mas** todas las POSIX.
Por eso, definir `_GNU_SOURCE` automaticamente hace disponible todo lo de
`_POSIX_C_SOURCE >= 200809L`.

```c
#ifdef _GNU_SOURCE
    /* strcasestr() — busqueda case-insensitive, extension GNU */
    const char *found = strcasestr("Hello, World!", "world");
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
    /* strnlen() — POSIX.1-2008 */
    size_t len = strnlen("Hello", 100);
#endif
```

Si defines `_GNU_SOURCE` o `_POSIX_C_SOURCE` **despues** de los includes, no
tienen efecto — los headers ya se procesaron sin ver esas definiciones.

---

## 8 — Deteccion de plataforma, compilador y estandar

El compilador predefine macros que permiten detectar el entorno:

**Plataforma:**

```c
#if defined(__linux__)
    #define PLATFORM "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM "macOS"
#elif defined(_WIN32)
    #define PLATFORM "Windows"
#elif defined(__FreeBSD__)
    #define PLATFORM "FreeBSD"
#else
    #error "Unsupported platform"
#endif
```

**Compilador:**

```c
#if defined(__GNUC__) && !defined(__clang__)
    /* GCC (Clang tambien define __GNUC__, por eso se excluye) */
    #define COMPILER "GCC"
    #define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__clang__)
    #define COMPILER "Clang"
#elif defined(_MSC_VER)
    #define COMPILER "MSVC"
#endif
```

Nota: Clang define `__GNUC__` por compatibilidad. Para distinguir GCC de Clang,
siempre verificar `!defined(__clang__)`.

**Estandar de C:**

```c
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

**Arquitectura:**

```c
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH "ARM64"
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH "x86"
#endif
```

Los macros con prefijo `_M_` son de MSVC. Los macros con `__` son de GCC/Clang.

---

## 9 — #error, #warning, y #if 0

**#error** detiene la compilacion con un mensaje:

```c
#ifndef BUFFER_SIZE
    #error "BUFFER_SIZE must be defined. Use -DBUFFER_SIZE=N"
#endif

#if BUFFER_SIZE < 64
    #error "BUFFER_SIZE must be at least 64"
#endif
```

Util para validar la configuracion del build en tiempo de compilacion (no en
runtime). Si falta un macro requerido, el programador ve un mensaje claro en
vez de errores confusos.

**#warning** emite un aviso pero la compilacion continua:

```c
#ifdef USE_OLD_API
    #warning "USE_OLD_API is deprecated — migrate to the new API"
#endif
```

`#warning` es extension GCC/Clang, estandarizada en C23. Con `-std=c11
-Wpedantic`, GCC emite un warning adicional sobre la directiva misma.

**#if 0** deshabilita bloques de codigo:

```c
#if 0
    /* Este codigo NO se compila.
       A diferencia de /* ... */, puede contener
       comentarios anidados y directivas: */
    #ifdef FOO
    printf("esto no existe\n");
    #endif
#endif
```

Los comentarios `/* */` no se pueden anidar — el primer `*/` cierra el
comentario exterior. `#if 0` es la forma segura de deshabilitar bloques
grandes que pueden contener comentarios.

Patron para alternar entre implementaciones:

```c
#if 1    /* cambiar a 0 para usar la version alternativa */
    int compute(int x) { return x * x; }
#else
    int compute(int x) { return x * x + 1; }
#endif
```

---

## 10 — Compilacion condicional vs runtime: cuando usar cada una

| Criterio | Compilacion condicional | Runtime |
|----------|------------------------|---------|
| Overhead | Cero — codigo eliminado | Branch en cada ejecucion |
| Flexibilidad | Requiere recompilar | Configurable en ejecucion |
| Tipos/structs | Puede cambiar la definicion | No puede cambiar un struct |
| Diagnostico | `gcc -E` para verificar | Debugger, logs |
| Caso de uso | Plataforma, compilador, build type | Configuracion de usuario |

Usar compilacion condicional cuando:
- El codigo no debe existir en ciertos builds (debug, release)
- Depende de la plataforma o el compilador
- Afecta tipos o structs (no se pueden cambiar en runtime)
- El overhead importa (hot paths)

Usar runtime cuando:
- El usuario puede cambiar la configuracion sin recompilar
- El mismo binario debe funcionar en multiples modos

**Comparacion con Rust:**

Rust no tiene preprocesador. La compilacion condicional se hace con el atributo
`#[cfg(...)]` y el macro `cfg!()`:

```rust
// Equivalente a #ifdef en C:
#[cfg(target_os = "linux")]
fn platform_init() {
    println!("Linux init");
}

#[cfg(target_os = "windows")]
fn platform_init() {
    println!("Windows init");
}

// Equivalente a #if defined(FEATURE_A) && defined(FEATURE_B):
#[cfg(all(feature = "ssl", feature = "logging"))]
fn secure_log() { /* ... */ }

// Equivalente a #if defined(X) || defined(Y):
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
fn simd_optimize() { /* ... */ }

// Runtime check (equivalente a if debug_enabled):
if cfg!(debug_assertions) {
    println!("Debug mode");
}
```

Diferencias clave:
- `#[cfg]` opera sobre **items** (funciones, structs, impl), no sobre lineas
  arbitrarias — el codigo siempre tiene estructura valida
- Las features se declaran en `Cargo.toml`, no con `-D` — son tipadas y
  documentadas
- `cfg!(...)` permite checks en runtime sin overhead (el compilador lo resuelve
  y lo reemplaza por `true` o `false`)
- No hay trampa del "macro no definido vale 0" — una feature inexistente es
  un error de compilacion

---

## Ejercicios

### Ejercicio 1 — Existencia vs valor

```c
// Definir tres macros dentro del archivo:
//   #define OPT_A          (vacio)
//   #define OPT_B 0
//   #define OPT_C 1
//
// Para cada uno, imprimir:
// 1. Si #ifdef es TRUE o FALSE
// 2. Si #if es TRUE o FALSE
// Predecir antes de compilar.
```

<details><summary>Prediccion</summary>

| Macro | `#ifdef` | `#if` |
|-------|----------|-------|
| `OPT_A` (vacio) | TRUE (existe) | TRUE — un macro vacio en `#if` se trata como 0... **pero no:** `#if OPT_A` con OPT_A vacio es un error. Depende del compilador. GCC lo trata como 0 con warning |
| `OPT_B 0` | TRUE (existe) | FALSE (vale 0) |
| `OPT_C 1` | TRUE (existe) | TRUE (vale 1) |

Para `OPT_A` vacio: `#if OPT_A` expande a `#if ` (nada), que GCC trata como `#if 0` pero puede emitir warning. `#ifdef OPT_A` es TRUE sin ambiguedad. Esta es la trampa: un macro definido sin valor pasa `#ifdef` pero su comportamiento en `#if` es problematico.

</details>

### Ejercicio 2 — Sistema de logging con #if/#elif

```c
// Implementar un sistema de logging controlado por -DLOG_LEVEL=N:
//   0 = off, 1 = ERROR, 2 = +WARN, 3 = +INFO, 4 = +DEBUG
//
// Default a 1 si no se define.
// Cada macro LOG_X debe imprimir [NIVEL] archivo:linea: mensaje
// usando __FILE__ y __LINE__.
//
// Compilar con: gcc prog.c
//               gcc -DLOG_LEVEL=3 prog.c
//               gcc -DLOG_LEVEL=0 prog.c
// Verificar que cada nivel muestra solo los mensajes esperados.
```

<details><summary>Prediccion</summary>

```c
#ifndef LOG_LEVEL
#define LOG_LEVEL 1
#endif

#if LOG_LEVEL >= 1
    #define LOG_ERROR(fmt, ...) \
        fprintf(stderr, "[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 2
    #define LOG_WARN(fmt, ...) \
        fprintf(stderr, "[WRN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif
/* ... mismo patron para INFO (>=3) y DEBUG (>=4) */
```

- Sin flags: `LOG_LEVEL=1` → solo `LOG_ERROR` activo
- Con `-DLOG_LEVEL=3`: ERROR, WARN, INFO activos; DEBUG inactivo
- Con `-DLOG_LEVEL=0`: todos inactivos → `((void)0)`, cero overhead
- `##__VA_ARGS__` es extension GCC que elimina la coma si no hay argumentos variadic

</details>

### Ejercicio 3 — defined() combinado con && y ||

```c
// Crear un programa que detecte combinaciones de features:
//   USE_SSL, USE_LOGGING, USE_CACHE
//
// Usando defined() con operadores logicos (NO #ifdef):
// 1. Si las tres estan activas: "Full configuration"
// 2. Si SSL + LOGGING: "Secure with logging"
// 3. Si solo SSL: "Secure mode"
// 4. Si ninguna: "Minimal configuration"
//
// Compilar con distintas combinaciones de -DUSE_X y verificar.
```

<details><summary>Prediccion</summary>

```c
#if defined(USE_SSL) && defined(USE_LOGGING) && defined(USE_CACHE)
    printf("Full configuration\n");
#elif defined(USE_SSL) && defined(USE_LOGGING)
    printf("Secure with logging\n");
#elif defined(USE_SSL)
    printf("Secure mode\n");
#elif !defined(USE_SSL) && !defined(USE_LOGGING) && !defined(USE_CACHE)
    printf("Minimal configuration\n");
#else
    printf("Custom configuration\n");
#endif
```

- `gcc prog.c` → "Minimal configuration"
- `gcc -DUSE_SSL prog.c` → "Secure mode"
- `gcc -DUSE_SSL -DUSE_LOGGING prog.c` → "Secure with logging"
- `gcc -DUSE_SSL -DUSE_LOGGING -DUSE_CACHE prog.c` → "Full configuration"
- `gcc -DUSE_LOGGING prog.c` → cae en el `#else` (Custom configuration)

El orden de los `#elif` importa: las combinaciones mas especificas deben ir primero.

</details>

### Ejercicio 4 — Detectar macros no definidos con -Wundef

```c
// 1. Escribir un programa con:
//    #if FEATURE_X
//        printf("Feature X active\n");
//    #endif
//    donde FEATURE_X nunca se define.
//
// 2. Compilar con gcc -Wall -Wextra y verificar que NO hay warning
// 3. Compilar con gcc -Wall -Wextra -Wundef y observar el warning
// 4. Corregir usando #if defined(FEATURE_X) && FEATURE_X
// 5. Verificar que con la correccion ya no hay warning con -Wundef
```

<details><summary>Prediccion</summary>

- Paso 2: `-Wall -Wextra` NO incluye `-Wundef` → sin warning. `#if FEATURE_X` vale 0 silenciosamente
- Paso 3: `-Wundef` produce: `warning: "FEATURE_X" is not defined, evaluates to 0 [-Wundef]`
- Paso 4: `#if defined(FEATURE_X) && FEATURE_X` — `defined(FEATURE_X)` es false → cortocircuito, `FEATURE_X` nunca se evalua → sin warning
- Leccion: `-Wundef` detecta usos de macros no definidos en `#if`. `defined()` evita el problema porque no evalua el macro como entero

</details>

### Ejercicio 5 — NDEBUG y assert con side effects

```c
// 1. Escribir una funcion divide(a, b) con assert(b != 0)
// 2. Compilar SIN -DNDEBUG, llamar divide(10, 0), observar abort
// 3. Compilar CON -DNDEBUG, observar que la division por cero
//    crashea con SIGFPE en vez de un mensaje legible
// 4. Demostrar el antipatron: assert(++counter > 0)
//    - Sin NDEBUG: counter se incrementa
//    - Con NDEBUG: counter NO se incrementa
// 5. Corregir separando: ++counter; assert(counter > 0);
```

<details><summary>Prediccion</summary>

Sin NDEBUG:
```
prog: prog.c:5: divide: Assertion `b != 0' failed.
Aborted (core dumped)
```
El assert intercepta la division por cero con un mensaje claro.

Con NDEBUG:
```
Floating point exception (core dumped)
```
El assert desaparecio → la division por cero ocurre → SIGFPE sin explicacion.

Antipatron `assert(++counter > 0)`:
- Sin NDEBUG: `assert` se ejecuta, `++counter` ocurre, counter = 1
- Con NDEBUG: `assert(...)` → `((void)0)`, `++counter` desaparece, counter = 0
- El programa se comporta diferente en debug vs release → bug sutil

</details>

### Ejercicio 6 — Feature test macros: desbloquear strnlen

```c
// 1. Escribir un programa que use strnlen() de <string.h>
// 2. Compilar con gcc -std=c11 -Wall -Wextra -Wpedantic
//    sin feature macros → debe fallar o dar warning implicito
// 3. Agregar #define _POSIX_C_SOURCE 200809L ANTES de los includes
//    y recompilar → ahora strnlen esta disponible
// 4. Reemplazar por _GNU_SOURCE y verificar que tambien funciona
//    (porque _GNU_SOURCE es superset de POSIX)
// 5. Usar gcc -E para verificar que strnlen aparece declarado
//    en la salida del preprocesador
```

<details><summary>Prediccion</summary>

- Sin feature macros: con `-std=c11`, el sistema solo expone funciones del estandar C11. `strnlen` es POSIX, no C11 → warning implicito o error
- Con `_POSIX_C_SOURCE 200809L`: los headers exponen `strnlen` → compila limpio
- Con `_GNU_SOURCE`: superset de POSIX → tambien expone `strnlen`, mas extensiones GNU como `strcasestr`
- `gcc -E prog.c | grep strnlen` mostrara la declaracion de `strnlen` solo cuando el feature macro esta activo

</details>

### Ejercicio 7 — #error para validar configuracion de build

```c
// Crear un programa que requiera dos macros desde la linea de compilacion:
//   APP_NAME (string) y APP_PORT (entero >= 1024)
//
// Usar #error para:
// 1. Detener si APP_NAME no esta definido
// 2. Detener si APP_PORT no esta definido
// 3. Detener si APP_PORT < 1024 (puertos privilegiados)
//
// Compilar con distintas combinaciones y verificar los mensajes.
// Compilar correctamente con:
//   gcc -DAPP_NAME=\"myapp\" -DAPP_PORT=8080 prog.c
```

<details><summary>Prediccion</summary>

```c
#ifndef APP_NAME
    #error "Define APP_NAME with -DAPP_NAME=\\\"name\\\""
#endif

#ifndef APP_PORT
    #error "Define APP_PORT with -DAPP_PORT=N (N >= 1024)"
#endif

#if APP_PORT < 1024
    #error "APP_PORT must be >= 1024 (privileged ports not allowed)"
#endif
```

- `gcc prog.c` → error: "Define APP_NAME..."
- `gcc -DAPP_NAME=\"x\" prog.c` → error: "Define APP_PORT..."
- `gcc -DAPP_NAME=\"x\" -DAPP_PORT=80 prog.c` → error: "APP_PORT must be >= 1024"
- `gcc -DAPP_NAME=\"myapp\" -DAPP_PORT=8080 prog.c` → compila y ejecuta

Nota: los strings desde `-D` requieren escaping: `-DAPP_NAME=\"myapp\"` para que el preprocesador vea `#define APP_NAME "myapp"`.

</details>

### Ejercicio 8 — Deteccion de plataforma y compilador

```c
// Escribir un programa que imprima:
// 1. Plataforma (Linux/macOS/Windows/unknown)
// 2. Compilador y version (GCC X.Y.Z / Clang X.Y.Z / MSVC)
// 3. Estandar de C (C89/C99/C11/C17/C23)
// 4. Arquitectura (x86_64/ARM64/x86/unknown)
// 5. sizeof(void*) y sizeof(long) para verificar el modelo de datos
//
// Compilar con -std=c11 y con -std=c17 para verificar que
// la deteccion del estandar cambia.
```

<details><summary>Prediccion</summary>

En tu sistema (Fedora 43, GCC, x86_64):
- Plataforma: `Linux` (via `__linux__`)
- Compilador: `GCC X.Y.Z` (via `__GNUC__`, `__GNUC_MINOR__`, `__GNUC_PATCHLEVEL__`)
- Con `-std=c11`: `C11` (`__STDC_VERSION__ == 201112L`)
- Con `-std=c17`: `C17` (`__STDC_VERSION__ == 201710L`)
- Arquitectura: `x86_64` (via `__x86_64__`)
- `sizeof(void*) = 8`, `sizeof(long) = 8` (modelo LP64)

Nota: Clang define `__GNUC__` por compatibilidad. Por eso se verifica `!defined(__clang__)` antes de declarar GCC.

</details>

### Ejercicio 9 — #if 0 para deshabilitar codigo con comentarios

```c
// 1. Escribir un bloque de codigo con comentarios /* */ dentro
// 2. Intentar deshabilitarlo con /* ... */ exterior
//    → error: los comentarios no se anidan
// 3. Deshabilitarlo con #if 0 ... #endif → funciona
// 4. Agregar directivas del preprocesador (#define, #ifdef) dentro
//    del bloque #if 0 y verificar que tambien se ignoran
// 5. Usar el patron #if 1/#else para alternar entre dos
//    implementaciones de una misma funcion
```

<details><summary>Prediccion</summary>

```c
/* Intento con comentarios anidados: */
/*
    int x = 42; /* este es un comentario interno */
    printf("%d\n", x);   ← ESTO SE COMPILA — el */ cerro el comentario
*/                        ← ERROR: */ sin /* correspondiente

/* Con #if 0: */
#if 0
    int x = 42; /* comentario interno — sin problema */
    #define SOMETHING 123  /* directivas ignoradas */
    #ifdef ANYTHING
    printf("nada de esto existe\n");
    #endif
#endif
/* Todo el bloque desaparece limpiamente */

/* Patron de alternancia: */
#if 1
    int compute(int x) { return x * 2; }    /* version A (activa) */
#else
    int compute(int x) { return x * 3; }    /* version B (inactiva) */
#endif
/* Cambiar 1 a 0 para activar la version B */
```

</details>

### Ejercicio 10 — Feature flags con struct condicional

```c
// Crear una struct Config con campos condicionales:
//   - port (siempre presente)
//   - cert_path, key_path (solo con -DUSE_SSL)
//   - log_file (solo con -DUSE_LOGGING)
//
// Escribir una funcion print_config que imprima todos los
// campos activos.
//
// Compilar de 4 formas y verificar que sizeof(Config) cambia:
//   gcc prog.c
//   gcc -DUSE_SSL prog.c
//   gcc -DUSE_LOGGING prog.c
//   gcc -DUSE_SSL -DUSE_LOGGING prog.c
```

<details><summary>Prediccion</summary>

```c
typedef struct Config {
    int port;
#ifdef USE_SSL
    const char *cert_path;
    const char *key_path;
#endif
#ifdef USE_LOGGING
    const char *log_file;
#endif
} Config;
```

Sizeof en x86_64 (LP64, puntero = 8 bytes):
- Sin flags: `sizeof(Config) = 4` (solo `int port`, con padding posiblemente 4 u 8)
- Con SSL: `+16 bytes` (dos punteros)
- Con LOGGING: `+8 bytes` (un puntero)
- Con ambos: `+24 bytes` (tres punteros)

El tamaño exacto depende del padding/alignment. Con `port` (4 bytes) seguido de punteros (8 bytes), habra 4 bytes de padding despues de `port`. Asi:
- Sin flags: 4 bytes (o 4 con posible tail padding)
- Con SSL: 4 + 4(pad) + 8 + 8 = 24
- Con LOGGING: 4 + 4(pad) + 8 = 16
- Con ambos: 4 + 4(pad) + 8 + 8 + 8 = 32

La compilacion condicional cambia la **definicion misma** del struct — esto es imposible con runtime `if`.

</details>
