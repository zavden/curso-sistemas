# T02 — Estándar del curso (C11)

## Por qué C11

Este curso usa **C11** (`-std=c11`) como estándar base. La elección
no es arbitraria:

```
C89 — Demasiado viejo:
  Sin declaraciones en for, sin //, sin stdint.h, sin bool.
  Escribir C89 hoy es innecesariamente restrictivo.

C99 — Buena opción, pero le faltan cosas:
  Sin _Static_assert, sin _Generic, sin threads estándar,
  sin structs anónimos. Muchos proyectos modernos necesitan
  al menos C11.

C11 — El balance correcto:
  Tiene todo lo que necesitamos: static_assert, anonymous
  structs/unions, _Generic, threads.h, stdatomic.h.
  Soporte completo en GCC y Clang desde hace años.
  Es el estándar que más proyectos modernos usan como base.

C17 — Idéntico a C11 en la práctica:
  Solo correcciones editoriales, sin features nuevas.
  Usar -std=c17 o -std=c11 produce el mismo código.

C23 — Demasiado nuevo:
  Soporte parcial en compiladores (2024-2025).
  Features útiles pero no indispensables para aprender C.
  Señalaremos features de C23 cuando aparezcan.
```

```bash
# Flag para todo el curso:
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

## Features de C11 que usa el curso

### _Static_assert / static_assert

Verifica una condición en **tiempo de compilación**. Si la
condición es falsa, la compilación falla con un mensaje:

```c
#include <assert.h>    // para el macro static_assert

// Verificar supuestos sobre la plataforma:
static_assert(sizeof(int) >= 4, "necesitamos int de al menos 32 bits");
static_assert(sizeof(void *) == 8, "este código es para plataformas de 64 bits");

// Verificar supuestos sobre structs:
struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t data;
};
static_assert(sizeof(struct Packet) == 8, "Packet debe ser 8 bytes (sin padding)");
```

```c
// Diferencia con assert():
assert(x > 0);           // runtime — el programa aborta si falla
static_assert(X > 0, ""); // compilación — no compila si falla

// static_assert no genera código — es pura verificación en compilación.
// No tiene costo en runtime.

// Usar para:
// - Verificar tamaños de tipos
// - Verificar alineación
// - Verificar que una constante tiene un valor esperado
// - Detectar errores de portabilidad
```

```c
// _Static_assert es el keyword real de C11.
// static_assert es un macro definido en <assert.h>:
// #define static_assert _Static_assert
// En C23, static_assert será un keyword directo.
```

### Anonymous structs and unions

Permiten acceder a los miembros de un struct/union anidado
directamente, sin nombre intermedio:

```c
// Sin anonymous struct (C99):
struct Color_named {
    union {
        struct {
            uint8_t r, g, b, a;
        } components;               // tiene nombre
        uint32_t rgba;
    } color;                         // tiene nombre
};

struct Color_named c;
c.color.components.r = 255;         // acceso verboso
c.color.rgba = 0xFF0000FF;

// Con anonymous struct/union (C11):
struct Color {
    union {
        struct {
            uint8_t r, g, b, a;
        };                           // sin nombre
        uint32_t rgba;
    };                               // sin nombre
};

struct Color c;
c.r = 255;                          // acceso directo
c.rgba = 0xFF0000FF;                // mismo dato, diferente vista
```

```c
// Otro ejemplo — vector 3D con acceso dual:
struct Vec3 {
    union {
        struct { float x, y, z; };   // acceso por nombre
        float v[3];                   // acceso por índice
    };
};

struct Vec3 pos = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
printf("x=%f, v[0]=%f\n", pos.x, pos.v[0]);  // mismo valor
```

```c
// Restricción: los miembros anónimos no pueden tener tag:
struct Outer {
    struct { int a; int b; };     // OK — anónimo
    // struct Named { int c; };   // ERROR — tiene tag
};
```

### _Generic

Despacho por tipo en tiempo de compilación. Permite escribir
macros que se comportan diferente según el tipo del argumento:

```c
#include <stdio.h>
#include <math.h>

// Macro que elige la función correcta según el tipo:
#define abs_val(x) _Generic((x),    \
    int:       abs,                  \
    long:      labs,                 \
    long long: llabs,                \
    float:     fabsf,                \
    double:    fabs                   \
)(x)

int main(void) {
    int    a = abs_val(-5);          // llama a abs(-5)
    double b = abs_val(-3.14);       // llama a fabs(-3.14)
    float  c = abs_val(-2.0f);       // llama a fabsf(-2.0f)
    return 0;
}
```

```c
// _Generic evalúa el TIPO de la expresión, no el valor.
// La selección ocurre en compilación — sin overhead en runtime.

// Sintaxis:
_Generic(expresión,
    tipo1: resultado1,
    tipo2: resultado2,
    default: resultado_por_defecto
)

// La expresión NO se evalúa — solo se usa su tipo.
```

```c
// Ejemplo: print genérico
#define print_val(x) _Generic((x),     \
    int:           printf("%d\n", x),   \
    unsigned int:  printf("%u\n", x),   \
    long:          printf("%ld\n", x),  \
    double:        printf("%f\n", x),   \
    char*:         printf("%s\n", x),   \
    default:       printf("?\n")        \
)

int n = 42;
double d = 3.14;
char *s = "hello";
print_val(n);     // 42
print_val(d);     // 3.140000
print_val(s);     // hello
```

```c
// Ejemplo: type name como string
#define type_name(x) _Generic((x),     \
    int:           "int",               \
    double:        "double",            \
    char*:         "char*",             \
    float:         "float",             \
    default:       "unknown"            \
)

int x = 42;
printf("x is %s\n", type_name(x));    // "x is int"
```

### threads.h — Threads en el estándar

C11 incorporó threads al estándar. Antes de C11, los threads
dependían de la plataforma (POSIX pthreads, Windows threads):

```c
#include <threads.h>
#include <stdio.h>

int worker(void *arg) {
    int id = *(int *)arg;
    printf("Thread %d running\n", id);
    return 0;
}

int main(void) {
    thrd_t t;
    int id = 1;

    thrd_create(&t, worker, &id);
    thrd_join(t, NULL);

    return 0;
}
```

```c
// API de threads.h:

// Threads:
thrd_create(&t, func, arg);   // crear thread
thrd_join(t, &result);         // esperar a que termine
thrd_detach(t);                // liberar sin esperar
thrd_current();                // ID del thread actual
thrd_sleep(&duration, NULL);   // dormir
thrd_yield();                  // ceder el CPU

// Mutexes:
mtx_init(&m, mtx_plain);      // crear mutex
mtx_lock(&m);                 // bloquear
mtx_unlock(&m);                // desbloquear
mtx_destroy(&m);               // destruir

// Condition variables:
cnd_init(&c);                  // crear
cnd_wait(&c, &m);             // esperar
cnd_signal(&c);                // despertar uno
cnd_broadcast(&c);             // despertar todos

// Thread-local storage:
_Thread_local int counter;     // cada thread tiene su copia
```

```c
// Nota: en la práctica, muchos proyectos usan pthreads directamente
// porque:
// 1. pthreads tiene más features (barriers, rwlocks, scheduling)
// 2. glibc no implementó threads.h hasta 2023 (musl sí lo tiene)
// 3. El código existente ya usa pthreads
//
// Pero threads.h es el estándar y es portable.
// En este curso lo mencionamos como referencia.
```

### stdatomic.h — Operaciones atómicas

```c
#include <stdatomic.h>

// Variables atómicas — accesos seguros sin mutex:
_Atomic int counter = 0;

// Operaciones atómicas:
atomic_store(&counter, 42);
int val = atomic_load(&counter);
atomic_fetch_add(&counter, 1);     // counter++ atómico
atomic_fetch_sub(&counter, 1);     // counter-- atómico

// Compare-and-swap:
int expected = 5;
atomic_compare_exchange_strong(&counter, &expected, 10);
// Si counter == 5, lo cambia a 10. Si no, expected = counter actual.
```

```c
// Memory orders (control de visibilidad entre threads):
atomic_store_explicit(&counter, 42, memory_order_release);
int val = atomic_load_explicit(&counter, memory_order_acquire);

// Los memory orders son un tema avanzado.
// Para empezar, las operaciones sin _explicit usan
// memory_order_seq_cst (el más seguro pero más lento).
```

### _Alignas y _Alignof

```c
#include <stdalign.h>    // macros alignas y alignof

// alignof — consultar la alineación de un tipo:
printf("Alineación de int: %zu\n", alignof(int));       // típicamente 4
printf("Alineación de double: %zu\n", alignof(double));  // típicamente 8

// alignas — forzar alineación:
alignas(16) int data[4];     // alineado a 16 bytes (útil para SIMD)
alignas(64) char cache_line[64];  // alineado a línea de caché
```

### _Noreturn

```c
#include <stdnoreturn.h>    // macro noreturn

// Indica que una función NUNCA retorna:
noreturn void fatal_error(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

// El compilador puede optimizar sabiendo que el código
// después de la llamada nunca se ejecuta.
// También genera warning si la función intenta retornar.
```

## Lo que C11 NO tiene

```c
// Features que NO están en C11 (vienen en C23 o no existen):

// No hay typeof estándar:
// typeof(x) y;           // extensión GNU, no C11

// No hay constexpr:
// constexpr int MAX = 100; // C23

// No hay nullptr:
// int *p = nullptr;       // C23, en C11 usar NULL

// No hay atributos [[]]:
// [[nodiscard]] int f();  // C23, en C11 usar __attribute__((warn_unused_result))

// No hay binary literals:
// int x = 0b1010;         // extensión GNU o C23

// No hay digit separators:
// int x = 1'000'000;      // C23

// Todas estas features se señalan cuando aparezcan en el curso.
```

## Flag de compilación del curso

```bash
# Para todo el curso, este es el set de flags recomendado:

# Desarrollo:
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined main.c -o main

# -Wall -Wextra -Wpedantic  → máximos warnings
# -std=c11                   → C11 estricto (sin extensiones GNU)
# -g                         → información de debug
# -O0                        → sin optimización (para GDB)
# -fsanitize=...             → detección de errores en runtime

# Sin sanitizers (compilación rápida):
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

---

## Ejercicios

### Ejercicio 1 — static_assert

```c
// Agregar static_asserts para verificar:
// 1. sizeof(char) == 1
// 2. sizeof(int) >= sizeof(short)
// 3. sizeof(long) >= sizeof(int)
// ¿Compila en tu sistema?

#include <assert.h>

// tus static_asserts aquí

int main(void) {
    return 0;
}
```

### Ejercicio 2 — _Generic

```c
// Crear un macro max_val(a, b) usando _Generic que funcione
// con int, long y double.
// Tip: necesitas funciones auxiliares para cada tipo.
```

### Ejercicio 3 — Anonymous structs

```c
// Crear un struct "Rect" con un anonymous struct para
// posición (x, y) y un anonymous struct para dimensiones
// (width, height). Verificar que puedes acceder a
// rect.x, rect.y, rect.width, rect.height directamente.
```
