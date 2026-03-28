# T02 — Estándar del curso (C11)

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| README.md línea 272 | "glibc no implementó threads.h hasta 2023" | glibc añadió soporte de C11 threads (`threads.h`) en la **versión 2.28 (agosto 2018)**, no en 2023. musl lo tuvo antes aún. La confusión puede venir de que el soporte se consideró maduro/estable más tarde, pero la implementación existe desde 2018. |

---

## 1. Por qué C11

Este curso usa **C11** (`-std=c11`) como estándar base:

| Estándar | Veredicto | Razón |
|----------|-----------|-------|
| **C89** | Demasiado viejo | Sin declaraciones en `for`, sin `//`, sin `stdint.h`, sin `bool` |
| **C99** | Bueno, pero incompleto | Sin `_Static_assert`, sin `_Generic`, sin threads estándar, sin structs anónimos |
| **C11** | **El balance correcto** | `static_assert`, anonymous structs/unions, `_Generic`, `threads.h`, `stdatomic.h`. Soporte completo en GCC y Clang |
| **C17** | Idéntico a C11 | Solo correcciones editoriales. `-std=c17` y `-std=c11` producen el mismo código |
| **C23** | Demasiado nuevo | Soporte parcial en compiladores (2024–2025). Features útiles pero no indispensables para aprender C |

```bash
# Flag base para todo el curso:
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

---

## 2. `_Static_assert` / `static_assert`

Verifica una condición en **tiempo de compilación**. Si la condición es falsa, la compilación falla con el mensaje indicado:

```c
#include <assert.h>    // define el macro: static_assert → _Static_assert

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

Diferencia fundamental con `assert()`:

| | `assert()` | `static_assert()` |
|---|---|---|
| **Momento** | Runtime | Compilación |
| **Si falla** | `abort()` en ejecución | Error de compilación |
| **Genera código** | Sí (a menos que se defina `NDEBUG`) | No — cero costo en runtime |
| **Operandos** | Cualquier expresión | Solo expresiones constantes (evaluables en compilación) |

```c
// _Static_assert es el keyword real de C11.
// static_assert es un macro definido en <assert.h>:
//   #define static_assert _Static_assert
// En C23, static_assert pasa a ser un keyword directo (sin necesidad del header).
```

---

## 3. Anonymous structs y unions

Permiten acceder a miembros de un struct/union anidado **directamente**, sin nombre intermedio:

```c
// Sin anonymous (C99) — acceso verboso:
struct Color_named {
    union {
        struct {
            uint8_t r, g, b, a;
        } components;               // tiene nombre
        uint32_t rgba;
    } color;                         // tiene nombre
};
struct Color_named c;
c.color.components.r = 255;         // 3 niveles

// Con anonymous (C11) — acceso directo:
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

Patrón útil — **acceso dual** (por nombre y por índice):

```c
struct Vec3 {
    union {
        struct { float x, y, z; };   // acceso por nombre
        float v[3];                   // acceso por índice
    };
};

struct Vec3 pos = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
printf("x=%f, v[0]=%f\n", pos.x, pos.v[0]);  // mismo valor
```

**Restricción**: los miembros anónimos no pueden tener tag (nombre de tipo):

```c
struct Outer {
    struct { int a; int b; };     // OK — anónimo
    // struct Named { int c; };   // ERROR — tiene tag
};
```

---

## 4. `_Generic` — Despacho por tipo en compilación

Selecciona una expresión según el **tipo** del primer argumento. La selección ocurre en compilación (cero overhead en runtime):

```c
// Sintaxis:
_Generic(expresión,
    tipo1: resultado1,
    tipo2: resultado2,
    default: resultado_por_defecto
)
// La expresión NO se evalúa — solo se inspecciona su tipo.
```

Ejemplo clásico — `abs` genérico:

```c
#include <stdlib.h>
#include <math.h>

#define abs_val(x) _Generic((x),    \
    int:       abs,                  \
    long:      labs,                 \
    long long: llabs,                \
    float:     fabsf,                \
    double:    fabs                   \
)(x)

int    a = abs_val(-5);          // → abs(-5)
double b = abs_val(-3.14);       // → fabs(-3.14)
float  c = abs_val(-2.0f);       // → fabsf(-2.0f)
```

Ejemplo — `type_name` como string:

```c
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

Ejemplo — `print_val` genérico:

```c
#define print_val(x) _Generic((x),     \
    int:           printf("%d\n", x),   \
    unsigned int:  printf("%u\n", x),   \
    long:          printf("%ld\n", x),  \
    double:        printf("%f\n", x),   \
    char*:         printf("%s\n", x),   \
    default:       printf("?\n")        \
)
```

---

## 5. `threads.h` — Threads en el estándar

C11 incorporó threads al estándar. Antes, los threads dependían de la plataforma (POSIX `pthreads`, Windows threads):

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

### API completa de `threads.h`

```c
// --- Threads ---
thrd_create(&t, func, arg);    // crear thread
thrd_join(t, &result);          // esperar a que termine
thrd_detach(t);                 // liberar sin esperar
thrd_current();                 // ID del thread actual
thrd_sleep(&duration, NULL);    // dormir
thrd_yield();                   // ceder el CPU

// --- Mutexes ---
mtx_init(&m, mtx_plain);       // crear mutex
mtx_lock(&m);                  // bloquear
mtx_unlock(&m);                // desbloquear
mtx_destroy(&m);               // destruir

// --- Condition variables ---
cnd_init(&c);                   // crear
cnd_wait(&c, &m);              // esperar (libera mutex, bloquea, re-adquiere)
cnd_signal(&c);                 // despertar un thread
cnd_broadcast(&c);              // despertar todos

// --- Thread-local storage ---
_Thread_local int counter;      // cada thread tiene su propia copia
```

### `threads.h` vs `pthreads` en la práctica

En la práctica, muchos proyectos siguen usando `pthreads` directamente porque:

1. **pthreads tiene más features**: barriers, rwlocks, scheduling policies, cancelación
2. **Soporte histórico**: glibc añadió `threads.h` en la versión 2.28 (2018); musl lo tuvo antes. Todo el código existente ya usaba pthreads
3. **Portabilidad paradójica**: `pthreads` está disponible en más sistemas que `threads.h`

`threads.h` es el estándar ISO y es la opción correcta para código portable. En este curso lo mencionamos como referencia.

---

## 6. `stdatomic.h` — Operaciones atómicas

Variables atómicas permiten accesos seguros entre threads sin mutex:

```c
#include <stdatomic.h>

_Atomic int counter = 0;

// Operaciones básicas:
atomic_store(&counter, 42);
int val = atomic_load(&counter);
atomic_fetch_add(&counter, 1);     // counter++ atómico
atomic_fetch_sub(&counter, 1);     // counter-- atómico
```

### Compare-and-swap (CAS)

```c
int expected = 5;
atomic_compare_exchange_strong(&counter, &expected, 10);
// Si counter == 5 → lo cambia a 10, retorna true
// Si counter != 5 → expected = valor actual de counter, retorna false
```

### Memory orders

```c
atomic_store_explicit(&counter, 42, memory_order_release);
int val = atomic_load_explicit(&counter, memory_order_acquire);
```

Los memory orders controlan la visibilidad de operaciones entre threads. Es un tema avanzado. Las operaciones sin `_explicit` usan `memory_order_seq_cst` (el más seguro pero más lento — correcto para empezar).

| Order | Garantía |
|-------|----------|
| `memory_order_relaxed` | Solo atomicidad, sin orden |
| `memory_order_acquire` | Lee ven escrituras previas del que hizo release |
| `memory_order_release` | Escrituras visibles para quien haga acquire |
| `memory_order_seq_cst` | Orden total global (el default) |

---

## 7. `_Alignas` y `_Alignof`

```c
#include <stdalign.h>    // macros alignas y alignof (C11)

// alignof — consultar la alineación requerida de un tipo:
printf("Alineación de int:    %zu\n", alignof(int));       // típicamente 4
printf("Alineación de double: %zu\n", alignof(double));    // típicamente 8

// alignas — forzar alineación mínima:
alignas(16) int data[4];          // alineado a 16 bytes (útil para SIMD)
alignas(64) char cache_line[64];  // alineado a línea de caché
```

Caso de uso principal: **SIMD** (SSE/AVX requieren alineación a 16/32 bytes) y **optimización de caché** (evitar false sharing alineando a 64 bytes).

---

## 8. `_Noreturn`

```c
#include <stdnoreturn.h>    // macro noreturn (C11)

noreturn void fatal_error(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}
```

El compilador usa esta información para:
- **Optimizar**: el código después de la llamada nunca se ejecuta, puede eliminarse
- **Advertir**: genera warning si la función intenta retornar con `return`

Funciones estándar que son `_Noreturn`: `exit()`, `abort()`, `_Exit()`, `quick_exit()`, `thrd_exit()`.

> En C23, `_Noreturn` y `<stdnoreturn.h>` se deprecan en favor del atributo `[[noreturn]]`.

---

## 9. Lo que C11 NO tiene

Estas features **no** existen en C11 — son de C23 o extensiones de compilador:

| Feature | En C11 | Alternativa C11 | Disponible en |
|---------|--------|-----------------|---------------|
| `typeof(x)` | No | `__typeof__(x)` (extensión GNU) | C23 |
| `constexpr` | No | `#define` o `enum` para constantes enteras | C23 |
| `nullptr` | No | `NULL` (que es `(void *)0` o `0`) | C23 |
| `[[atributos]]` | No | `__attribute__((...))` (extensión GNU) | C23 |
| `0b1010` (binary literals) | No | `0xA` (hexadecimal) | C23 / extensión GNU |
| `1'000'000` (digit separators) | No | No hay alternativa | C23 |
| `auto` (type deduction) | No | Declarar el tipo explícitamente | C23 |
| `#embed` | No | `xxd -i` para generar arrays | C23 |

---

## 10. Flags de compilación del curso

```bash
# --- Desarrollo (máxima detección de errores) ---
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined main.c -o main

# -Wall -Wextra -Wpedantic  → máximos warnings
# -std=c11                   → C11 estricto (sin extensiones GNU)
# -g                         → información de debug (para GDB)
# -O0                        → sin optimización (para GDB)
# -fsanitize=...             → detección de errores en runtime

# --- Sin sanitizers (compilación rápida, para GDB) ---
gcc -Wall -Wextra -Wpedantic -std=c11 -g main.c -o main
```

Nota: `-std=c11` (sin `gnu`) desactiva extensiones GNU. Si un ejercicio necesita una extensión (como `__typeof__`), se indicará explícitamente.

---

## Ejercicios

### Ejercicio 1 — `static_assert` básico

Escribe un programa que use `static_assert` para verificar tres propiedades del sistema:

```c
// static_assert_basic.c
#include <assert.h>
#include <stdint.h>

static_assert(sizeof(char) == 1, "char debe ser 1 byte");
static_assert(sizeof(int) >= sizeof(short), "int debe ser >= short");
static_assert(sizeof(long) >= sizeof(int), "long debe ser >= int");

int main(void) {
    // Agrega un static_assert que FALLE:
    // static_assert(sizeof(int) == 8, "int no es 8 bytes");
    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 static_assert_basic.c -o static_assert_basic && ./static_assert_basic
```

**Predicción**: ¿qué pasa si descomentas la línea del `static_assert` que falla?

<details><summary>Respuesta</summary>

El programa **no compila**. GCC muestra un error como:

```
static_assert_basic.c:8: error: static assertion failed: "int no es 8 bytes"
```

`static_assert` actúa en compilación — no hay ejecutable, no hay runtime. En la mayoría de plataformas modernas (x86-64), `sizeof(int) == 4`, no 8.

</details>

### Ejercicio 2 — `static_assert` sobre struct

Usa `static_assert` para verificar que un struct tiene el tamaño esperado (sin padding inesperado):

```c
// static_assert_struct.c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct NetworkHeader {
    uint8_t  version;    // 1 byte
    uint8_t  type;       // 1 byte
    uint16_t length;     // 2 bytes
    uint32_t sequence;   // 4 bytes
};

static_assert(sizeof(struct NetworkHeader) == 8, "NetworkHeader debe ser 8 bytes");

struct Bad {
    char   a;       // 1 byte
    double b;       // 8 bytes
};

int main(void) {
    printf("sizeof(NetworkHeader) = %zu\n", sizeof(struct NetworkHeader));
    printf("sizeof(Bad) = %zu\n", sizeof(struct Bad));
    // ¿Por qué sizeof(Bad) no es 9?
    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 static_assert_struct.c -o static_assert_struct && ./static_assert_struct
```

**Predicción**: ¿cuánto vale `sizeof(struct Bad)`? ¿Por qué no es 9?

<details><summary>Respuesta</summary>

`sizeof(struct Bad)` es **16**, no 9. El compilador inserta 7 bytes de **padding** entre `a` (1 byte) y `b` (8 bytes) para alinear `b` a 8 bytes (su alineación natural). Layout:

```
Offset 0:   a      (1 byte)
Offset 1-7: padding (7 bytes)
Offset 8:   b      (8 bytes)
Total: 16 bytes
```

`NetworkHeader` no tiene padding porque los campos están ordenados de manera que cada uno cae naturalmente alineado: `uint8_t` (alineación 1), `uint16_t` (alineación 2, offset 2 ✓), `uint32_t` (alineación 4, offset 4 ✓).

</details>

### Ejercicio 3 — Anonymous struct/union

Crea un struct `Rect` con acceso directo a posición y dimensiones:

```c
// anonymous_rect.c
#include <stdio.h>

struct Rect {
    union {
        struct { float x, y; };
        float pos[2];
    };
    union {
        struct { float width, height; };
        float size[2];
    };
};

int main(void) {
    struct Rect r = { .x = 10.0f, .y = 20.0f, .width = 100.0f, .height = 50.0f };

    // Acceso por nombre:
    printf("Posición: (%.1f, %.1f)\n", r.x, r.y);
    printf("Tamaño: %.1f x %.1f\n", r.width, r.height);

    // Acceso por array:
    printf("pos[0]=%.1f, pos[1]=%.1f\n", r.pos[0], r.pos[1]);
    printf("size[0]=%.1f, size[1]=%.1f\n", r.size[0], r.size[1]);

    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 anonymous_rect.c -o anonymous_rect && ./anonymous_rect
```

**Predicción**: ¿`r.x` y `r.pos[0]` imprimen el mismo valor? ¿Por qué?

<details><summary>Respuesta</summary>

Sí, imprimen el mismo valor (`10.0`). El `union` anónimo hace que `x` y `pos[0]` ocupen **la misma memoria**. Un union almacena todos sus miembros en la misma dirección; el struct anónimo `{ float x, y; }` y el array `float pos[2]` comparten los mismos bytes. Salida:

```
Posición: (10.0, 20.0)
Tamaño: 100.0 x 50.0
pos[0]=10.0, pos[1]=20.0
size[0]=100.0, size[1]=50.0
```

</details>

### Ejercicio 4 — `_Generic` para `type_name`

Implementa un macro `type_name(x)` que retorne el nombre del tipo como string:

```c
// generic_typename.c
#include <stdio.h>

#define type_name(x) _Generic((x),     \
    int:            "int",              \
    unsigned int:   "unsigned int",     \
    long:           "long",             \
    float:          "float",            \
    double:         "double",           \
    char*:          "char*",            \
    const char*:    "const char*",      \
    default:        "unknown"           \
)

int main(void) {
    int a = 42;
    double b = 3.14;
    const char *c = "hello";
    char *d = (char[]){"world"};
    float e = 1.0f;
    short s = 5;

    printf("a: %s\n", type_name(a));
    printf("b: %s\n", type_name(b));
    printf("c: %s\n", type_name(c));
    printf("d: %s\n", type_name(d));
    printf("e: %s\n", type_name(e));
    printf("s: %s\n", type_name(s));

    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 generic_typename.c -o generic_typename && ./generic_typename
```

**Predicción**: ¿qué imprime para `s` (un `short`)? ¿Por qué?

<details><summary>Respuesta</summary>

Imprime `"unknown"` para `s`. `_Generic` hace coincidencia **exacta** de tipos — no hay promoción implícita. `short` no está en la lista, así que cae en `default`. Si quisiéramos detectar `short`, habría que añadir `short: "short"` explícitamente.

```
a: int
b: double
c: const char*
d: char*
e: float
s: unknown
```

Nota: un string literal `"hello"` tiene tipo `const char*`, no `char*`. Por eso se necesitan ambas entradas en el `_Generic`.

</details>

### Ejercicio 5 — `_Generic` con funciones

Crea un macro `abs_val(x)` que despache a la función correcta según el tipo:

```c
// generic_abs.c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define abs_val(x) _Generic((x),    \
    int:       abs,                  \
    long:      labs,                 \
    long long: llabs,                \
    float:     fabsf,                \
    double:    fabs                   \
)(x)

int main(void) {
    int    a = -5;
    double b = -3.14;
    float  c = -2.5f;
    long   d = -1000000L;

    printf("abs_val(%d)   = %d\n", a, abs_val(a));
    printf("abs_val(%.2f) = %.2f\n", b, abs_val(b));
    printf("abs_val(%.1f) = %.1f\n", c, abs_val(c));
    printf("abs_val(%ld)  = %ld\n", d, abs_val(d));

    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 generic_abs.c -o generic_abs -lm && ./generic_abs
```

**Predicción**: ¿qué pasaría si llamaras `abs_val((unsigned int)5)`?

<details><summary>Respuesta</summary>

Error de compilación. `unsigned int` no tiene entrada en el `_Generic` y no hay `default`. El compilador dice algo como:

```
error: '_Generic' selector of type 'unsigned int' is not compatible with any association
```

Para solucionarlo, habría que agregar `unsigned int: (unsigned int)` con una función helper, o añadir `default: abs` como fallback (aunque no sería correcto para todos los tipos).

</details>

### Ejercicio 6 — Explorar `alignof` y `alignas`

```c
// alignment.c
#include <stdio.h>
#include <stdalign.h>
#include <stdint.h>

int main(void) {
    printf("=== Alineación natural de tipos ===\n");
    printf("char:     %zu\n", alignof(char));
    printf("short:    %zu\n", alignof(short));
    printf("int:      %zu\n", alignof(int));
    printf("long:     %zu\n", alignof(long));
    printf("float:    %zu\n", alignof(float));
    printf("double:   %zu\n", alignof(double));
    printf("void*:    %zu\n", alignof(void *));
    printf("int64_t:  %zu\n", alignof(int64_t));

    printf("\n=== Alineación forzada ===\n");
    alignas(16) int a;
    alignas(32) char b[32];
    int c;

    printf("&a (alignas 16): %p → mod 16 = %zu\n",
           (void *)&a, (size_t)&a % 16);
    printf("&b (alignas 32): %p → mod 32 = %zu\n",
           (void *)&b, (size_t)&b % 32);
    printf("&c (natural):    %p → mod 4  = %zu\n",
           (void *)&c, (size_t)&c % 4);

    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 alignment.c -o alignment && ./alignment
```

**Predicción**: ¿cuál es la alineación de `char`? ¿Y de `double`? ¿El residuo de `&a % 16` será 0?

<details><summary>Respuesta</summary>

- `alignof(char)` = **1** (siempre, por definición del estándar)
- `alignof(double)` = **8** en x86-64 (el tipo ocupa 8 bytes y debe estar alineado a 8)
- `&a % 16` = **0** — `alignas(16)` garantiza que la dirección de `a` es múltiplo de 16

Salida típica en x86-64:

```
char:     1
short:    2
int:      4
long:     8
float:    4
double:   8
void*:    8
int64_t:  8

&a (alignas 16): 0x7ffd...0 → mod 16 = 0
&b (alignas 32): 0x7ffd...0 → mod 32 = 0
&c (natural):    0x7ffd...c → mod 4  = 0
```

Las direcciones exactas varían, pero los residuos siempre son 0.

</details>

### Ejercicio 7 — `_Noreturn` y warnings

```c
// noreturn_demo.c
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

int safe_divide(int a, int b) {
    if (b == 0) {
        die("división por cero");
    }
    return a / b;
}

// ¿Qué pasa si quitas el exit(1) de die()?
// noreturn void bad_die(const char *msg) {
//     fprintf(stderr, "FATAL: %s\n", msg);
//     // sin exit — ¡la función retorna!
// }

int main(void) {
    printf("10 / 3 = %d\n", safe_divide(10, 3));
    printf("10 / 0 = %d\n", safe_divide(10, 0));  // nunca llega al printf
    return 0;
}
```

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 noreturn_demo.c -o noreturn_demo && ./noreturn_demo
```

**Predicción**: ¿se ejecuta el segundo `printf`? ¿Qué warning da GCC si descomentas `bad_die`?

<details><summary>Respuesta</summary>

El segundo `printf` **nunca se ejecuta**. `safe_divide(10, 0)` llama a `die()`, que ejecuta `exit(1)`. Salida:

```
10 / 3 = 3
FATAL: división por cero
```

Si descomentas `bad_die` (una función `noreturn` que sí retorna), GCC emite:

```
warning: 'noreturn' function does return [-Winvalid-noreturn]
```

Y si `bad_die` realmente se ejecuta, el comportamiento es **indefinido** — la función marcada `noreturn` retornó, y el compilador puede haber eliminado código que venía después de la llamada.

</details>

### Ejercicio 8 — Lo que C11 NO tiene

Intenta compilar código con features de C23 usando `-std=c11` y observa los errores:

```c
// not_c11.c
#include <stdio.h>

int main(void) {
    // Intento 1: binary literal
    // int mask = 0b11110000;

    // Intento 2: typeof (extensión GNU)
    // int x = 42;
    // typeof(x) y = 100;

    // Intento 3: auto deduction (C23)
    // auto z = 3.14;

    printf("Descomenta las líneas una por una y recompila con -std=c11\n");
    return 0;
}
```

```bash
# Probar cada línea descomentada:
gcc -Wall -Wextra -Wpedantic -std=c11 not_c11.c -o not_c11

# Luego probar con gnu11 (permite extensiones):
gcc -Wall -Wextra -Wpedantic -std=gnu11 not_c11.c -o not_c11

# Y con c23 (todo debería compilar):
gcc -Wall -Wextra -std=c23 not_c11.c -o not_c11
```

**Predicción**: ¿cuáles de las 3 líneas compilan con `-std=gnu11`? ¿Y con `-std=c23`?

<details><summary>Respuesta</summary>

| Feature | `-std=c11` | `-std=gnu11` | `-std=c23` |
|---------|-----------|-------------|-----------|
| `0b11110000` | Error | **Compila** (extensión GNU) | **Compila** |
| `typeof(x)` | Error | **Compila** (extensión GNU) | **Compila** |
| `auto z = 3.14` | Error | Error (no es extensión GNU) | **Compila** |

Con `-std=c11` (C estricto), ninguna compila. Con `-std=gnu11`, los binary literals y `typeof` funcionan porque son extensiones GNU. `auto` como deducción de tipo es exclusivo de C23 — en C11/C99 `auto` existe pero significa "storage class automática" (que es el default, así que nadie lo usa).

</details>

### Ejercicio 9 — Comparar `-std=c11` vs `-std=gnu11`

```c
// c11_vs_gnu11.c
#include <stdio.h>

int main(void) {
    // Este programa compila con ambos estándares.
    // La diferencia está en qué extensiones están disponibles.

    printf("__STDC_VERSION__ = %ld\n", __STDC_VERSION__);

    #ifdef __GNUC__
        printf("Compilador: GCC %d.%d.%d\n",
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    #endif

    #ifdef __STRICT_ANSI__
        printf("Modo estricto: extensiones GNU deshabilitadas\n");
    #else
        printf("Modo GNU: extensiones GNU habilitadas\n");
    #endif

    return 0;
}
```

```bash
echo "=== Con -std=c11 ==="
gcc -Wall -Wextra -Wpedantic -std=c11 c11_vs_gnu11.c -o test_c11 && ./test_c11

echo ""
echo "=== Con -std=gnu11 ==="
gcc -Wall -Wextra -Wpedantic -std=gnu11 c11_vs_gnu11.c -o test_gnu11 && ./test_gnu11
```

**Predicción**: ¿`__STDC_VERSION__` cambia entre `-std=c11` y `-std=gnu11`? ¿Qué macro se define/no define?

<details><summary>Respuesta</summary>

`__STDC_VERSION__` es **el mismo** en ambos: `201112L` (C11). La diferencia es:

- Con `-std=c11`: se define `__STRICT_ANSI__`, extensiones GNU deshabilitadas
- Con `-std=gnu11`: `__STRICT_ANSI__` **NO** se define, extensiones GNU habilitadas

```
=== Con -std=c11 ===
__STDC_VERSION__ = 201112
Compilador: GCC 15.2.1
Modo estricto: extensiones GNU deshabilitadas

=== Con -std=gnu11 ===
__STDC_VERSION__ = 201112
Compilador: GCC 15.2.1
Modo GNU: extensiones GNU habilitadas
```

`-std=c11` y `-std=gnu11` usan el mismo estándar C11, pero `gnu11` permite extensiones como `typeof`, `__attribute__`, statement expressions `({...})`, binary literals, etc. El curso usa `-std=c11` para asegurarse de escribir C portable.

</details>

### Ejercicio 10 — Compilación con los flags del curso

Compila un programa con errores intencionales usando los flags recomendados y observa qué detecta cada capa:

```c
// course_flags.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static_assert(sizeof(int) >= 4, "need 32-bit int");

int sum_array(int *arr, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) {    // Bug: off-by-one (i <= n)
        total += arr[i];
    }
    return total;
}

int main(void) {
    int data[] = {10, 20, 30, 40, 50};
    int n = 5;

    printf("Sum = %d\n", sum_array(data, n));

    // Bug 2: signed overflow intencional
    int big = 2147483647;  // INT_MAX
    printf("big + 1 = %d\n", big + 1);

    return 0;
}
```

```bash
# Paso 1: compilar SIN sanitizers — ¿detecta algo?
gcc -Wall -Wextra -Wpedantic -std=c11 -g course_flags.c -o course_flags && ./course_flags

echo "---"

# Paso 2: compilar CON sanitizers — ¿qué más detecta?
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O0 \
    -fsanitize=address,undefined course_flags.c -o course_flags_san && ./course_flags_san
```

**Predicción**: ¿el paso 1 muestra warnings o errores? ¿El paso 2 detecta los bugs en runtime?

<details><summary>Respuesta</summary>

**Paso 1** (sin sanitizers): el compilador probablemente **no emite warnings** sobre el off-by-one ni el overflow — estos son errores lógicos, no sintácticos. El programa ejecuta y produce resultados silenciosamente incorrectos:

```
Sum = <valor con basura del byte extra>
big + 1 = -2147483648
```

El overflow produce wrapping a negativo (en la práctica, aunque técnicamente es UB).

**Paso 2** (con sanitizers): los sanitizers detectan ambos bugs en runtime:
- **ASan** detecta el acceso fuera de límites (`arr[5]` en un array de 5 elementos): `stack-buffer-overflow`
- **UBSan** detecta el signed integer overflow: `signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`

El programa aborta en el primer error detectado. Esto demuestra por qué los flags completos del curso (`-fsanitize=address,undefined`) son necesarios durante el desarrollo.

</details>
