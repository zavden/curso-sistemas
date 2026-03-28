# T01 — Variadic macros

## Erratas detectadas

| Ubicacion | Error | Correccion |
|-----------|-------|------------|
| `README.md:26` | El comentario de la expansion de `DEBUG("value = %d", 42)` muestra `"[DBG main.c:10] value = %d\n"` como si `%s` y `%d` del prefijo ya estuvieran resueltos | La expansion real del preprocesador es `fprintf(stderr, "[DBG %s:%d] " "value = %d" "\n", "main.c", 10, 42)` — los `%s` y `%d` siguen en el format string y se resuelven en runtime por `fprintf`, no por el preprocesador |

---

## 1 — __VA_ARGS__ basico

Los macros variadicos (C99) aceptan un numero variable de argumentos usando
`...` en la lista de parametros y `__VA_ARGS__` en la expansion:

```c
#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", __VA_ARGS__)

LOG("x = %d", x);
/* → fprintf(stderr, "[LOG] " "x = %d" "\n", x)
   Los string literals se concatenan en compilacion:
   → fprintf(stderr, "[LOG] x = %d\n", x) */

LOG("a=%d b=%d", a, b);
/* → fprintf(stderr, "[LOG] " "a=%d b=%d" "\n", a, b) */
```

La mecanica:
- `...` captura **todos** los argumentos despues del ultimo parametro fijo
- `__VA_ARGS__` se reemplaza por esos argumentos, separados por comas
- `"[LOG] " fmt "\n"` — tres string literals adyacentes se concatenan en
  compilacion (no por el preprocesador)

Se pueden tener multiples parametros fijos antes de `...`:

```c
#define ERROR(code, fmt, ...) \
    fprintf(stderr, "[ERR %d] " fmt "\n", code, __VA_ARGS__)

ERROR(404, "Not found: %s", path);
/* → fprintf(stderr, "[ERR %d] Not found: %s\n", 404, path) */
```

Un macro sin parametro fijo — todo es variadic:

```c
#define PRINT_ALL(...) printf(__VA_ARGS__)

PRINT_ALL("x = %d\n", x);    /* → printf("x = %d\n", x) */
PRINT_ALL("hello\n");         /* → printf("hello\n") */
```

---

## 2 — El problema de cero argumentos variadicos

El bug mas frecuente con macros variadicos: si no hay argumentos despues del
parametro fijo, queda una coma huerfana:

```c
#define LOG(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)

LOG("starting");
/* Expansion: fprintf(stderr, "starting" "\n", )
                                               ^ coma + nada = ERROR */
```

El error de compilacion:

```
error: expected expression before ')' token
```

Esto ocurre porque `__VA_ARGS__` se expande a **nada**, pero la coma que lo
precede permanece. El preprocesador no sabe que debe eliminarla.

En C99/C11 estricto, un macro variadico requiere **al menos un argumento
variadico**. `LOG("hello")` con cero argumentos variadicos es tecnicamente no
conforme al estandar.

---

## 3 — ##__VA_ARGS__: extension GCC

La solucion de facto: anteponer `##` a `__VA_ARGS__`. Si no hay argumentos
variadicos, `##` elimina la coma precedente:

```c
#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", ##__VA_ARGS__)

LOG("starting");
/* __VA_ARGS__ vacio → ## elimina la coma →
   fprintf(stderr, "[LOG] " "starting" "\n")   ← sin coma, OK */

LOG("x = %d", x);
/* __VA_ARGS__ = x → la coma permanece →
   fprintf(stderr, "[LOG] " "x = %d" "\n", x)   ← normal */
```

**No es estandar C99/C11**, pero es soportado por:
- GCC (con o sin `-pedantic` en versiones recientes)
- Clang (sin warnings incluso con `-pedantic`)

Es tan ubicuo que se considera de facto universal. Para proyectos que necesitan
portabilidad estricta a compiladores exoticos, existe `__VA_OPT__` (seccion 4).

---

## 4 — __VA_OPT__: la solucion estandar (C23)

C23 introdujo `__VA_OPT__(contenido)`:
- Si hay argumentos variadicos → se expande a `contenido`
- Si no hay argumentos → se expande a **nada**

```c
#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

LOG("starting");
/* __VA_OPT__(,) → nada, __VA_ARGS__ → nada →
   fprintf(stderr, "[LOG] " "starting" "\n") */

LOG("x = %d", x);
/* __VA_OPT__(,) → ",", __VA_ARGS__ → x →
   fprintf(stderr, "[LOG] " "x = %d" "\n", x) */
```

`__VA_OPT__` puede contener cualquier texto, no solo comas:

```c
#define DEBUG(fmt, ...) \
    fprintf(stderr, "[DBG %s:%d] " fmt "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

DEBUG("entering main");
/* Los args fijos __FILE__, __LINE__ siempre estan.
   __VA_OPT__(,) → nada (sin args extra).
   → fprintf(stderr, "[DBG %s:%d] entering main\n", "file.c", 26) */

DEBUG("x = %d", x);
/* __VA_OPT__(,) → ","
   → fprintf(stderr, "[DBG %s:%d] x = %d\n", "file.c", 27, x) */
```

Compilacion:

```bash
gcc -std=c2x -Wall -Wextra va_opt_demo.c -o va_opt_demo
# O: gcc -std=gnu2x ...
# Con -std=c11: warning: __VA_OPT__ is not available until C23
```

GCC 8+ y Clang 6+ soportan `__VA_OPT__` con `-std=c2x` o `-std=gnu2x`.

---

## 5 — Solucion sin parametro fijo

Mover `fmt` dentro de `__VA_ARGS__` elimina el problema porque no hay coma
entre un parametro fijo y los variadicos:

```c
#define LOG(...) fprintf(stderr, __VA_ARGS__)

LOG("starting\n");           /* → fprintf(stderr, "starting\n") */
LOG("x = %d\n", x);         /* → fprintf(stderr, "x = %d\n", x) */
```

**Desventaja**: no se puede agregar prefijo o sufijo alrededor de `fmt` porque
el format string esta mezclado con los argumentos dentro de `__VA_ARGS__`:

```c
/* No se puede hacer esto: */
#define LOG(...) fprintf(stderr, "[LOG] " __VA_ARGS__ "\n")
/* __VA_ARGS__ = "x = %d", x
   → fprintf(stderr, "[LOG] " "x = %d", x "\n")
                                          ^ x no es string literal → error */
```

Comparacion:

| Enfoque | Prefijo/sufijo | Cero args | Estandar |
|---------|---------------|-----------|----------|
| `fmt, ...` + `__VA_ARGS__` | Si | Error con cero args | C99 |
| `fmt, ...` + `##__VA_ARGS__` | Si | Funciona | GCC extension |
| `fmt, ...` + `__VA_OPT__(,)` | Si | Funciona | C23 |
| `...` sin fijo | No | Funciona siempre | C99 |

---

## 6 — Logging con archivo, linea y funcion

El patron mas comun de macros variadicos — sistema de logging con contexto:

```c
/* Siempre activo: */
#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

/* Condicional por nivel: */
#if LOG_LEVEL >= 2
    #define LOG_WARN(fmt, ...) \
        fprintf(stderr, "[WARN]  %s:%d: " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 4
    #define LOG_DEBUG(fmt, ...) \
        fprintf(stderr, "[DEBUG] %s:%d %s(): " fmt "\n", \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif
```

Por que `((void)0)` para macros deshabilitados:
- Es una expresion valida en cualquier contexto (if/else, coma, etc.)
- No genera codigo ejecutable
- Los argumentos del macro no se evaluan (desaparecen con la expansion)

Uso tipico:

```bash
gcc -DLOG_LEVEL=1 prog.c    # solo errores
gcc -DLOG_LEVEL=3 prog.c    # error + warn + info
gcc -DLOG_LEVEL=4 prog.c    # todo incluido debug
```

Nota: `__func__` no es un macro del preprocesador — es una variable implicita
(`static const char[]`) que el compilador define dentro de cada funcion. Por eso
solo se puede usar en la expansion (no en el format string del preprocesador).

---

## 7 — ASSERT variadico con do-while(0)

Un assert personalizado que combina stringify, variadic args, y do-while(0):

```c
#define ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED: %s\n" \
                "  at %s:%d (%s)\n" \
                "  " fmt "\n", \
                #cond, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        abort(); \
    } \
} while (0)

ASSERT(ptr != NULL, "allocation of %zu bytes failed", size);
/* Si falla:
   ASSERT FAILED: ptr != NULL
     at main.c:42 (process_data)
     allocation of 1024 bytes failed
*/
```

Elementos combinados:
- `#cond` → stringify de la condicion (aparece como texto en el mensaje)
- `__FILE__`, `__LINE__`, `__func__` → contexto del punto de llamada
- `##__VA_ARGS__` → argumentos del mensaje formateado
- `do { ... } while (0)` → seguro en if/else
- `abort()` → terminacion inmediata con core dump para debugging

---

## 8 — Despacho por numero de argumentos

Un truco avanzado: contar argumentos variadicos y despachar a macros distintos,
simulando overloading de funciones:

```c
/* Paso 1: contar argumentos */
#define COUNT_ARGS(...) COUNT_ARGS_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define COUNT_ARGS_(_1, _2, _3, _4, _5, N, ...) N

COUNT_ARGS(a)           /* → 1 */
COUNT_ARGS(a, b)        /* → 2 */
COUNT_ARGS(a, b, c)     /* → 3 */
```

Como funciona: los argumentos se colocan en posiciones fijas y `N` captura el
numero correcto. Con `COUNT_ARGS(a, b)`:
- `_1=a, _2=b, _3=5, _4=4, _5=3, N=2` → retorna `2`

```c
/* Paso 2: concatenar nombre + cuenta */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)

#define OVERLOAD(name, ...) \
    CONCAT(name, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

/* Paso 3: definir variantes por aridad */
typedef struct { int x, y, z; } Vec3;

#define VEC1(x)       (Vec3){x, 0, 0}
#define VEC2(x, y)    (Vec3){x, y, 0}
#define VEC3(x, y, z) (Vec3){x, y, z}

#define VEC(...) OVERLOAD(VEC, __VA_ARGS__)

VEC(1)        /* → VEC1(1)       → (Vec3){1, 0, 0} */
VEC(1, 2)     /* → VEC2(1, 2)    → (Vec3){1, 2, 0} */
VEC(1, 2, 3)  /* → VEC3(1, 2, 3) → (Vec3){1, 2, 3} */
```

Este patron es fragil y dificil de depurar. Usar con precaucion y siempre
verificar con `gcc -E`.

---

## 9 — Cuidados con macros variadicos

**Al menos un argumento es requerido en C99/C11:**

```c
LOG("hello")  /* Sin args variadicos → no conforme al estandar */
/* Usar ##__VA_ARGS__ o __VA_OPT__ para manejarlo */
```

**Comas dentro de argumentos:**

```c
LOG("coords: (%d, %d)", x, y);   /* OK — 2 args variadicos */
/* Las comas en string literals no cuentan como separadores */
/* Pero comas en expresiones sin parentesis si: */
LOG("result", (struct){.a=1, .b=2});  /* ERROR — el compilador ve 3 args */
/* Solucion: usar parentesis extra o variable temporal */
```

**__VA_ARGS__ solo dentro de macros variadicos:**

```c
#define FOO(x) printf(__VA_ARGS__)    /* ERROR — FOO no tiene ... */
```

**Stringify de __VA_ARGS__:**

```c
#define STRINGIFY_ARGS(...) #__VA_ARGS__

STRINGIFY_ARGS(a, b, c)    /* → "a, b, c" */
STRINGIFY_ARGS(x + 1)      /* → "x + 1" */
```

`#__VA_ARGS__` convierte todos los argumentos (con comas) en un solo string
literal. Util para debugging y logging.

**Nombres para __VA_ARGS__ (extension GCC):**

```c
/* Extension no estandar — NO usar: */
#define LOG(fmt, args...) fprintf(stderr, fmt, args)
/* Preferir __VA_ARGS__ que es C99 estandar */
```

---

## 10 — Comparacion con Rust

Rust tiene macros variadicos con un enfoque fundamentalmente distinto:

```rust
// macro_rules! usa patrones para capturar argumentos variadicos:
macro_rules! log {
    // Patron con formato + argumentos
    ($fmt:expr $(, $arg:expr)*) => {
        eprintln!(concat!("[LOG] ", $fmt), $($arg),*)
    };
    // Solo formato, sin argumentos extra
    ($fmt:expr) => {
        eprintln!(concat!("[LOG] ", $fmt))
    };
}

log!("starting");           // patron 2: eprintln!("[LOG] starting")
log!("x = {}", x);          // patron 1: eprintln!("[LOG] x = {}", x)
```

Diferencias clave:
- **Sin problema de coma**: los patrones de Rust manejan 0 o mas argumentos
  nativamente con `$(, $arg:expr)*` — la coma es parte del patron repetitivo
- **Tipo-seguro**: `eprintln!` verifica los format specifiers en compilacion
  (`{}` vs `%d`) — un tipo incorrecto es un error, no undefined behavior
- **Higienico**: las variables internas del macro no colisionan con el caller
- **Pattern matching**: se pueden tener multiples patrones para diferentes
  aridades, sin trucos como `COUNT_ARGS`
- **concat!** opera en compilacion sobre string literals, similar a la
  concatenacion de C pero explicito

```rust
// Equivalente al sistema de logging con niveles:
macro_rules! log_debug {
    ($($arg:tt)*) => {
        if cfg!(debug_assertions) {
            eprintln!("[DEBUG] {}:{}: {}", file!(), line!(),
                      format_args!($($arg)*));
        }
    };
}

// file!() y line!() son macros built-in (como __FILE__/__LINE__)
// cfg!(debug_assertions) se resuelve en compilacion (como #ifdef DEBUG)
```

---

## Ejercicios

### Ejercicio 1 — LOG basico con __VA_ARGS__

```c
// Definir un macro LOG(fmt, ...) que:
// - Imprima a stderr con prefijo "[LOG] " y sufijo "\n"
// - Usar __VA_ARGS__ estandar (sin ##)
//
// 1. Probar: LOG("x = %d", 42)  → debe funcionar
// 2. Probar: LOG("starting")    → debe fallar (coma extra)
// 3. Usar gcc -E para ver la expansion de ambos casos
```

<details><summary>Prediccion</summary>

```c
#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", __VA_ARGS__)
```

- `LOG("x = %d", 42)` → `fprintf(stderr, "[LOG] " "x = %d" "\n", 42)` → compila y muestra `[LOG] x = 42`
- `LOG("starting")` → `fprintf(stderr, "[LOG] " "starting" "\n", )` → error: `expected expression before ')' token`
- `gcc -E` mostrara la coma huerfana en el segundo caso

</details>

### Ejercicio 2 — Resolver con ##__VA_ARGS__

```c
// Tomar el macro LOG del ejercicio 1 y agregar ## antes de __VA_ARGS__
// 1. Verificar que LOG("starting") ahora compila
// 2. Verificar que LOG("x = %d", 42) sigue funcionando
// 3. Compilar con -std=c11 -Wpedantic y observar si hay warnings
// 4. Usar gcc -E para ver como ## elimino la coma
```

<details><summary>Prediccion</summary>

```c
#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", ##__VA_ARGS__)
```

- `LOG("starting")` → `fprintf(stderr, "[LOG] " "starting" "\n")` — `##` elimino la coma. Compila OK
- `LOG("x = %d", 42)` → `fprintf(stderr, "[LOG] " "x = %d" "\n", 42)` — funciona normal
- Con `-Wpedantic`: en GCC recientes (13+), probablemente no hay warning. En versiones anteriores: `warning: ISO C99 requires at least one argument for "..."`
- `gcc -E` para `LOG("starting")`: no aparece coma antes del `)` — confirmacion visual

</details>

### Ejercicio 3 — __VA_OPT__ de C23

```c
// Reescribir LOG usando __VA_OPT__(,) en vez de ##__VA_ARGS__
// 1. Compilar con -std=c2x -Wall -Wextra -Wpedantic
// 2. Probar con y sin argumentos variadicos
// 3. Intentar compilar con -std=c11 y observar el warning/error
// 4. Comparar la expansion de gcc -E con la version ##
```

<details><summary>Prediccion</summary>

```c
#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
```

- Con `-std=c2x`: compila limpio. `LOG("starting")` → `__VA_OPT__(,)` se expande a nada → sin coma
- Con `-std=c11`: warning `__VA_OPT__ is not available until C23`
- `gcc -E -std=c2x`: `LOG("starting")` → `fprintf(stderr, "[LOG] " "starting" "\n")` (identico a la version `##`)
- `gcc -E -std=c2x`: `LOG("x = %d", 42)` → `fprintf(stderr, "[LOG] " "x = %d" "\n", 42)`

La expansion final es identica a `##__VA_ARGS__`, pero `__VA_OPT__` es estandar C23.

</details>

### Ejercicio 4 — DEBUG con __FILE__ y __LINE__

```c
// Crear un macro DEBUG(fmt, ...) que imprima:
//   [DBG archivo:linea] mensaje
// usando __FILE__ y __LINE__ como argumentos de fprintf (no en el
// format string del preprocesador).
//
// 1. Probar: DEBUG("entering main")
// 2. Probar: DEBUG("x = %d", x)
// 3. Usar gcc -E para verificar que __FILE__ se reemplazo por
//    el nombre del archivo y __LINE__ por el numero de linea
```

<details><summary>Prediccion</summary>

```c
#define DEBUG(fmt, ...) \
    fprintf(stderr, "[DBG %s:%d] " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)
```

- `DEBUG("entering main")` en linea 15 de `prog.c`:
  → `fprintf(stderr, "[DBG %s:%d] " "entering main" "\n", "prog.c", 15)`
  → Salida: `[DBG prog.c:15] entering main`
- `DEBUG("x = %d", x)` en linea 16:
  → `fprintf(stderr, "[DBG %s:%d] " "x = %d" "\n", "prog.c", 16, x)`
  → Salida: `[DBG prog.c:16] x = 42`
- `gcc -E` mostrara `"prog.c"` y `16` como valores concretos (el preprocesador los resuelve)

Nota: `__FILE__` y `__LINE__` se expanden al punto de llamada del macro, no a donde se definio. Por eso los macros son preferibles a funciones inline para logging con contexto.

</details>

### Ejercicio 5 — Sistema de logging con 4 niveles

```c
// Implementar LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG controlados
// por -DLOG_LEVEL=N (default 3):
//   1=ERROR, 2=+WARN, 3=+INFO, 4=+DEBUG
//
// LOG_ERROR: siempre activo, con archivo:linea
// LOG_DEBUG: con archivo:linea:funcion (__func__)
// Macros deshabilitados: ((void)0)
//
// Compilar 3 veces con LEVEL=1, LEVEL=3, LEVEL=4 y comparar salidas.
```

<details><summary>Prediccion</summary>

Con `LOG_LEVEL=1`: solo `[ERROR]` aparece. WARN, INFO, DEBUG son `((void)0)`.
Con `LOG_LEVEL=3`: ERROR, WARN, INFO aparecen. DEBUG es `((void)0)`.
Con `LOG_LEVEL=4`: todos aparecen. DEBUG muestra archivo, linea y nombre de funcion.

Clave: `((void)0)` no genera codigo ni evalua argumentos. El binario compilado con `LOG_LEVEL=1` es mas pequeno y rapido que con `LOG_LEVEL=4`.

</details>

### Ejercicio 6 — Separar stderr de stdout

```c
// 1. Escribir un programa que use LOG (a stderr) y printf (a stdout)
// 2. Ejecutar normalmente: ambas salidas aparecen mezcladas
// 3. Ejecutar con 2>/dev/null: solo aparece stdout
// 4. Ejecutar con 2>&1 1>/dev/null: solo aparece stderr
// 5. Redirigir stderr a un archivo: ./prog 2>log.txt
//    Verificar que log.txt contiene solo los mensajes de LOG
```

<details><summary>Prediccion</summary>

```c
LOG("this goes to stderr");
printf("this goes to stdout\n");
```

- `./prog`: ambas lineas aparecen en la terminal (entrelazadas)
- `./prog 2>/dev/null`: solo `this goes to stdout`
- `./prog 2>&1 1>/dev/null`: solo `[LOG] this goes to stderr`
- `./prog 2>log.txt`: stdout en terminal, stderr en `log.txt`

`fprintf(stderr, ...)` escribe al file descriptor 2 (stderr).
`printf(...)` escribe al file descriptor 1 (stdout).
Las redirecciones de shell separan los dos streams.

</details>

### Ejercicio 7 — ASSERT variadico

```c
// Crear ASSERT(cond, fmt, ...) que:
// - Imprima la condicion como texto (#cond)
// - Imprima archivo, linea, funcion
// - Imprima mensaje formateado con los argumentos extra
// - Llame abort()
// - Use do-while(0)
//
// Probar:
//   ASSERT(x > 0, "x was %d", x);
//   ASSERT(ptr != NULL, "malloc failed for %zu bytes", n);
```

<details><summary>Prediccion</summary>

```c
#define ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED: %s\n" \
                "  at %s:%d (%s)\n" \
                "  " fmt "\n", \
                #cond, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        abort(); \
    } \
} while (0)
```

`ASSERT(x > 0, "x was %d", x)` con `x = -5`:
```
ASSERT FAILED: x > 0
  at main.c:20 (main)
  x was -5
Aborted (core dumped)
```

`#cond` produce el string `"x > 0"`. `##__VA_ARGS__` pasa `x` como argumento de `%d`. `abort()` genera core dump para analisis con gdb.

</details>

### Ejercicio 8 — COUNT_ARGS y despacho

```c
// 1. Implementar COUNT_ARGS(...) que cuente hasta 5 argumentos
// 2. Verificar: COUNT_ARGS(a) → 1, COUNT_ARGS(a,b,c) → 3
// 3. Usar printf para imprimir el resultado de COUNT_ARGS
// 4. Bonus: implementar POINT(...) que despache a POINT1/POINT2/POINT3
//    segun el numero de argumentos
// Pista: COUNT_ARGS_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
```

<details><summary>Prediccion</summary>

```c
#define COUNT_ARGS(...) COUNT_ARGS_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define COUNT_ARGS_(_1, _2, _3, _4, _5, N, ...) N
```

Traza para `COUNT_ARGS(a, b, c)`:
- `COUNT_ARGS_(a, b, c, 5, 4, 3, 2, 1, 0)`
- `_1=a, _2=b, _3=c, _4=5, _5=4, N=3` → retorna `3`

Traza para `COUNT_ARGS(a)`:
- `COUNT_ARGS_(a, 5, 4, 3, 2, 1, 0)`
- `_1=a, _2=5, _3=4, _4=3, _5=2, N=1` → retorna `1`

Limitacion: `COUNT_ARGS()` sin argumentos no funciona correctamente (retornaria 1 porque `__VA_ARGS__` vacio cuenta como un argumento vacio).

</details>

### Ejercicio 9 — Stringify de __VA_ARGS__

```c
// 1. Definir SHOW(...) que imprima la expresion como texto Y su valor:
//    SHOW(x + 1)  → "x + 1 = 43"
//
// 2. Definir SHOW_ARGS(...) que convierta todos los argumentos a string:
//    SHOW_ARGS(a, b, c)  → imprime "a, b, c"
//
// Pistas: #__VA_ARGS__ stringify todos los args como un string.
//         Para SHOW de una expresion int: printf("%s = %d\n", #__VA_ARGS__, (__VA_ARGS__))
```

<details><summary>Prediccion</summary>

```c
#define SHOW(...) printf("%s = %d\n", #__VA_ARGS__, (__VA_ARGS__))
#define SHOW_ARGS(...) printf("args: %s\n", #__VA_ARGS__)
```

- `SHOW(x + 1)` con `x=42`:
  - `#__VA_ARGS__` → `"x + 1"`
  - `(__VA_ARGS__)` → `(x + 1)` → `43`
  - Salida: `x + 1 = 43`

- `SHOW_ARGS(a, b, c)`:
  - `#__VA_ARGS__` → `"a, b, c"` (todas las comas se incluyen en el string)
  - Salida: `args: a, b, c`

Nota: `SHOW` solo funciona con expresiones enteras (`%d`). Para multiples tipos habria que usar `_Generic` o macros separados.

</details>

### Ejercicio 10 — Wrapper TIMED para medir ejecucion

```c
// Crear un macro TIMED(label, ...) que:
// 1. Tome timestamp con clock_gettime(CLOCK_MONOTONIC, ...)
// 2. Ejecute __VA_ARGS__ (el codigo a medir)
// 3. Tome segundo timestamp
// 4. Imprima: "label: X.XXXXXX s"
//
// Usar do-while(0) para seguridad.
// #include <time.h> para clock_gettime.
//
// Probar con un loop que sume 10 millones de numeros.
```

<details><summary>Prediccion</summary>

```c
#define TIMED(label, ...) do { \
    struct timespec _t0, _t1; \
    clock_gettime(CLOCK_MONOTONIC, &_t0); \
    __VA_ARGS__; \
    clock_gettime(CLOCK_MONOTONIC, &_t1); \
    double _elapsed = (_t1.tv_sec - _t0.tv_sec) \
                    + (_t1.tv_nsec - _t0.tv_nsec) / 1e9; \
    fprintf(stderr, "%s: %.6f s\n", label, _elapsed); \
} while (0)
```

Uso:
```c
long sum = 0;
TIMED("sum 10M", for (int i = 0; i < 10000000; i++) sum += i);
```

- `__VA_ARGS__` = `for (int i = 0; i < 10000000; i++) sum += i` — se inserta entre los dos `clock_gettime`
- Salida: `sum 10M: 0.023456 s` (tiempo variable)
- Nota: las variables `_t0`, `_t1`, `_elapsed` tienen prefijo `_` para reducir riesgo de colision con el caller, pero la colision es posible (ver T03 sobre variables temporales en macros)

Compilar con: `gcc -std=c11 -Wall -Wextra prog.c -o prog` (no necesita `-lrt` en Linux moderno; `clock_gettime` esta en libc).

</details>
