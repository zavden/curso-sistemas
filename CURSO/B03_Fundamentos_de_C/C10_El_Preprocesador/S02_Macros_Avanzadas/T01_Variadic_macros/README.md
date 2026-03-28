# T01 — Variadic macros

## __VA_ARGS__

Los macros variádicos aceptan un número variable de argumentos,
como `printf`. Se introdujeron en C99:

```c
// Sintaxis: ... en la lista de parámetros, __VA_ARGS__ en la expansión.

#define LOG(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)

LOG("x = %d", x);           // → fprintf(stderr, "x = %d" "\n", x)
LOG("a=%d b=%d", a, b);     // → fprintf(stderr, "a=%d b=%d" "\n", a, b)
```

```c
// Los ... capturan TODOS los argumentos después del último parámetro fijo.
// __VA_ARGS__ se expande a esos argumentos, separados por comas.

#define DEBUG(fmt, ...) \
    fprintf(stderr, "[DBG %s:%d] " fmt "\n", \
            __FILE__, __LINE__, __VA_ARGS__)

DEBUG("value = %d", 42);
// → fprintf(stderr, "[DBG main.c:10] value = %d\n", "main.c", 10, 42)

// Se puede tener parámetros fijos antes de ...:
#define ERROR(code, fmt, ...) \
    fprintf(stderr, "[ERR %d] " fmt "\n", code, __VA_ARGS__)

ERROR(404, "Not found: %s", path);
```

## El problema de cero argumentos variádicos

```c
// Problema: si no se pasan argumentos variádicos, queda una coma extra:

#define LOG(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)

LOG("starting");
// Se expande a: fprintf(stderr, "starting" "\n", )
//                                                ^ coma seguida de nada → ERROR

// Esto es un problema frecuente:
// LOG("hello") no compila en C99/C11 estricto.
```

### Solución 1: extensión GCC — ##__VA_ARGS__

```c
// ## antes de __VA_ARGS__ elimina la coma si no hay argumentos:
#define LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

LOG("starting");           // → fprintf(stderr, "starting" "\n")  ← OK, sin coma
LOG("x = %d", x);         // → fprintf(stderr, "x = %d" "\n", x) ← OK

// ## es una extensión de GCC, también soportada por Clang.
// No es estándar C99/C11, pero es tan común que es de facto estándar.
// GCC: funciona con -std=c99 y -pedantic da warning.
// Clang: funciona sin warnings incluso con -pedantic.
```

### Solución 2: C23 — __VA_OPT__

```c
// C23 introdujo __VA_OPT__(content):
// Se expande a 'content' SI hay argumentos variádicos.
// Se expande a NADA si no hay argumentos.

#define LOG(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__)

LOG("starting");           // → fprintf(stderr, "starting" "\n")
//  __VA_OPT__(,) → nada (no hay args variádicos)
//  __VA_ARGS__ → nada

LOG("x = %d", x);         // → fprintf(stderr, "x = %d" "\n", x)
//  __VA_OPT__(,) → ","
//  __VA_ARGS__ → x
```

```c
// __VA_OPT__ puede contener cualquier texto, no solo comas:

#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG] " fmt __VA_OPT__(": " __VA_ARGS__) "\n")

LOG("done");               // → fprintf(stderr, "[LOG] done\n")
LOG("error %d", code);     // → fprintf(stderr, "[LOG] error: " "error %d", code "\n")
//   ↑ este ejemplo es solo ilustrativo — el uso correcto es:

#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

// __VA_OPT__ es estándar C23.
// GCC 8+ y Clang 6+ lo soportan con -std=c2x o -std=gnu2x.
// Para C11/C17, usar ##__VA_ARGS__ (extensión GCC).
```

### Solución 3: solo un parámetro variádico

```c
// Mover fmt dentro de __VA_ARGS__:
#define LOG(...) fprintf(stderr, __VA_ARGS__)

LOG("starting\n");             // → fprintf(stderr, "starting\n")
LOG("x = %d\n", x);           // → fprintf(stderr, "x = %d\n", x)

// No hay coma problemática porque no hay parámetro fijo.
// Desventaja: no se puede agregar prefijo/sufijo alrededor de fmt.
```

## Patrones de uso

### Logging con archivo y línea

```c
// Patrón más común — macro wrapper para printf/fprintf:

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)

#ifdef DEBUG
    #define LOG_DEBUG(fmt, ...) \
        fprintf(stderr, "[DEBUG] %s:%d %s(): " fmt "\n", \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

// Uso:
LOG_ERROR("failed to open %s: %s", path, strerror(errno));
LOG_INFO("server started on port %d", port);
LOG_DEBUG("entering function with x=%d", x);
```

### Assert personalizado

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
// Si falla:
// ASSERT FAILED: ptr != NULL
//   at main.c:42 (process_data)
//   allocation of 1024 bytes failed
```

### Wrapper para funciones con contexto

```c
// Envolver funciones del sistema con logging automático:

#define SAFE_MALLOC(size, ...) \
    safe_malloc_impl(size, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

void *safe_malloc_impl(size_t size, const char *file, int line, ...) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "%s:%d: malloc(%zu) failed\n", file, line, size);
        abort();
    }
    return p;
}
```

### Wrapper para ejecutar y medir

```c
#define TIMED(label, ...) do { \
    struct timespec _start, _end; \
    clock_gettime(CLOCK_MONOTONIC, &_start); \
    __VA_ARGS__; \
    clock_gettime(CLOCK_MONOTONIC, &_end); \
    double _elapsed = (_end.tv_sec - _start.tv_sec) \
                    + (_end.tv_nsec - _start.tv_nsec) / 1e9; \
    fprintf(stderr, "%s: %.6f s\n", label, _elapsed); \
} while (0)

// Uso:
TIMED("sort", qsort(arr, n, sizeof(int), cmp));
TIMED("search", result = bsearch(&key, arr, n, sizeof(int), cmp));
```

## Macros variádicos que despachan por número de argumentos

```c
// Contar el número de argumentos variádicos:
#define COUNT_ARGS(...) COUNT_ARGS_(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define COUNT_ARGS_(_1, _2, _3, _4, _5, N, ...) N

COUNT_ARGS(a)           // → 1
COUNT_ARGS(a, b)        // → 2
COUNT_ARGS(a, b, c)     // → 3
// Funciona hasta el límite que se implemente (aquí 5).
```

```c
// Despachar a diferentes macros según el número de argumentos:
#define OVERLOAD(name, ...) \
    CONCAT(name, COUNT_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define CONCAT(a, b) CONCAT_(a, b)
#define CONCAT_(a, b) a##b

#define VEC1(x)       (Vec3){x, 0, 0}
#define VEC2(x, y)    (Vec3){x, y, 0}
#define VEC3(x, y, z) (Vec3){x, y, z}

#define VEC(...) OVERLOAD(VEC, __VA_ARGS__)

VEC(1)        // → VEC1(1)       → (Vec3){1, 0, 0}
VEC(1, 2)     // → VEC2(1, 2)    → (Vec3){1, 2, 0}
VEC(1, 2, 3)  // → VEC3(1, 2, 3) → (Vec3){1, 2, 3}

// Este patrón simula overloading de funciones en C.
// Es frágil y difícil de depurar — usar con precaución.
```

## Cuidados con macros variádicos

```c
// 1. Al menos un argumento variádico es requerido en C99/C11:
//    LOG("hello") sin args variádicos es técnicamente UB en C99.
//    Usar ##__VA_ARGS__ o __VA_OPT__ para manejarlo.

// 2. Las comas dentro de argumentos pueden confundir:
LOG("coords: (%d, %d)", x, y);    // OK — 2 args variádicos
// Pero si un argumento contiene template syntax de C++ o macros
// con comas, puede haber problemas.

// 3. __VA_ARGS__ no se puede usar fuera de un macro variádico:
#define FOO(x) printf(__VA_ARGS__)    // ERROR — FOO no tiene ...

// 4. No se puede nombrar __VA_ARGS__:
// Algunos compiladores soportan nombres como extensión:
// #define LOG(fmt, args...) fprintf(stderr, fmt, args)  // GCC extension
// Pero no es estándar — preferir __VA_ARGS__.

// 5. Stringify de __VA_ARGS__:
#define STRINGIFY_ARGS(...) #__VA_ARGS__
STRINGIFY_ARGS(a, b, c)    // → "a, b, c"
```

---

## Ejercicios

### Ejercicio 1 — Sistema de logging completo

```c
// Crear macros LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG que:
// - Impriman [NIVEL] archivo:línea: mensaje
// - Acepten formato variádico como printf
// - Funcionen sin argumentos extra: LOG_INFO("starting")
// - Se deshabiliten según LOG_LEVEL compilado con -DLOG_LEVEL=N
// Usar ##__VA_ARGS__ para compatibilidad.
```

### Ejercicio 2 — ASSERT variádico

```c
// Crear un macro ASSERT(cond, ...) que:
// - Si cond falla, imprima la condición como texto (#cond)
// - Si hay argumentos extra, los imprima como mensaje formateado
// - Si NO hay argumentos extra, imprima solo la condición
// - Incluya __FILE__, __LINE__, __func__
// Usar __VA_OPT__ o ##__VA_ARGS__ para el caso sin mensaje.
// Probar: ASSERT(x > 0) y ASSERT(x > 0, "x was %d", x)
```

### Ejercicio 3 — Macro de constructor con overloading

```c
// Definir un struct Point { int x, y, z; }
// Crear un macro POINT(...) que:
// - POINT() → (Point){0, 0, 0}
// - POINT(x) → (Point){x, 0, 0}
// - POINT(x, y) → (Point){x, y, 0}
// - POINT(x, y, z) → (Point){x, y, z}
// Usar COUNT_ARGS para despachar a POINT0, POINT1, POINT2, POINT3.
```
