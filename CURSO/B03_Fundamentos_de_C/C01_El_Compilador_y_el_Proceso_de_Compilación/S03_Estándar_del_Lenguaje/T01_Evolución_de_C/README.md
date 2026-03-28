# T01 — Evolución de C

## La estandarización de C

C no es un lenguaje diseñado de una vez — ha evolucionado a lo
largo de décadas. Cada revisión del estándar agrega features,
corrige ambigüedades y adapta el lenguaje a hardware moderno:

```
1972    Dennis Ritchie crea C en Bell Labs
1978    "The C Programming Language" (K&R C) — primer libro
1989    ANSI C (C89) — primera estandarización
1990    ISO adopta C89 como C90 (idéntico a C89)
1999    C99 — modernización significativa
2011    C11 — concurrencia, seguridad, features genéricas
2018    C17 — solo correcciones (sin features nuevas)
2024    C23 — la revisión más reciente
```

## K&R C (1978) — El C original

Antes de cualquier estándar, K&R C era el "estándar de facto"
definido por el libro de Kernighan y Ritchie:

```c
// K&R C — sintaxis antigua:

// Declaración de función sin prototipos:
int add(a, b)
    int a;
    int b;
{
    return a + b;
}

// Sin void — paréntesis vacíos significaban "cualquier argumento":
int main()       // acepta cualquier cosa
{
    printf("hello\n");   // sin #include, "funcionaba" de todos modos
}

// El compilador no verificaba tipos de argumentos.
// Los errores se descubrían en runtime (o nunca).
```

## C89/C90 (ANSI C) — La primera estandarización

ANSI estandarizó C en 1989 (C89). ISO lo adoptó en 1990 (C90).
Son el mismo estándar:

```c
// C89 introdujo:

// 1. Prototipos de función — verificación de tipos:
int add(int a, int b);         // el compilador verifica los tipos
// vs K&R:
int add();                     // sin verificación

// 2. void:
void do_nothing(void);         // void = no retorna / no acepta args
int main(void)                 // void explícito = sin argumentos

// 3. const y volatile:
const int MAX = 100;           // no modificable
volatile int hw_reg;           // no optimizar accesos

// 4. enum:
enum Color { RED, GREEN, BLUE };

// 5. Bibliotecas estándar formalizadas:
// stdio.h, stdlib.h, string.h, math.h, etc.

// 6. Trigraphs (obsoletos, eliminados en C23):
// ??= era equivalente a #
```

### Limitaciones de C89

```c
// C89 tiene restricciones que hoy parecen arcaicas:

// 1. Variables solo al inicio del bloque:
int main(void) {
    int x = 5;            // OK — inicio del bloque
    int y = 10;           // OK — inicio del bloque
    x = x + y;
    int z = x;            // ERROR en C89 — declaración después de statement
    return 0;
}

// 2. Sin comentarios //:
/* este es el único estilo de comentario en C89 */
// este estilo es una extensión, no parte de C89

// 3. Sin long long:
long long x;              // no existe en C89

// 4. Sin stdint.h:
int32_t x;                // no existe en C89

// 5. Sin _Bool/stdbool.h:
_Bool flag;               // no existe en C89

// 6. Sin VLAs:
int n = 10;
int arr[n];               // no existe en C89

// 7. Sin snprintf:
snprintf(buf, size, ...); // no existe en C89
```

## C99 — Modernización

C99 fue la primera gran actualización. Agregó muchas features
que los programadores llevaban años pidiendo:

```c
// 1. Declaraciones en cualquier parte del bloque:
for (int i = 0; i < 10; i++) {   // int i DENTRO del for
    int temp = arr[i];            // declaración después de código
    // ...
}

// 2. Comentarios //:
// Ahora esto es válido en el estándar

// 3. long long (al menos 64 bits):
long long big = 9223372036854775807LL;

// 4. <stdint.h> — tipos de tamaño fijo:
#include <stdint.h>
int8_t   a;    // exactamente 8 bits
int16_t  b;    // exactamente 16 bits
int32_t  c;    // exactamente 32 bits
int64_t  d;    // exactamente 64 bits
uint32_t e;    // unsigned, exactamente 32 bits

// 5. <stdbool.h> — tipo bool:
#include <stdbool.h>
bool flag = true;          // _Bool + macros true/false

// 6. Variable-length arrays (VLAs):
void foo(int n) {
    int arr[n];            // tamaño determinado en runtime
    // ...
}

// 7. Designated initializers:
struct Point { int x; int y; int z; };
struct Point p = { .x = 10, .z = 30 };   // .y = 0 implícito

int arr[5] = { [2] = 42, [4] = 99 };     // arr = {0, 0, 42, 0, 99}

// 8. Compound literals:
struct Point p = (struct Point){ .x = 5, .y = 10 };

// 9. restrict:
void copy(int *restrict dst, const int *restrict src, int n);
// Promesa al compilador: dst y src no se solapan

// 10. Inline functions:
inline int square(int x) { return x * x; }

// 11. <math.h> mejorado:
// isnan(), isinf(), round(), remainder()...

// 12. snprintf():
char buf[20];
snprintf(buf, sizeof(buf), "x = %d", 42);  // seguro contra overflow

// 13. // en el estándar, __func__, _Pragma
```

### VLAs — Controversia

```c
// Los VLAs de C99 parecen útiles pero tienen problemas:

void foo(int n) {
    int arr[n];    // se alloca en el stack
    // ¿Qué pasa si n = 1000000?
    // Stack overflow — sin forma de detectar el error
    // No hay verificación de tamaño
}

// Por estas razones:
// - C11 hizo los VLAs opcionales
// - C23 los hizo opcionales de nuevo
// - Muchos coding standards los prohíben (MISRA, CERT)
// - Linux kernel no los usa
// - Mejor usar malloc() para tamaños variables
```

## C11 — Concurrencia y seguridad

C11 agregó soporte para threads, operaciones atómicas, mejoras
de seguridad y features genéricas:

```c
// 1. _Static_assert (verificación en compilación):
#include <assert.h>
_Static_assert(sizeof(int) == 4, "int must be 4 bytes");
// Si la condición es falsa, la compilación falla con el mensaje

// En C11 con <assert.h>:
static_assert(sizeof(int) >= 4, "need at least 32-bit ints");

// 2. Anonymous structs and unions:
struct Vector3 {
    union {
        struct { float x, y, z; };    // anónimo — acceso directo
        float v[3];
    };
};
struct Vector3 vec;
vec.x = 1.0f;        // acceso directo, no vec.unnamed.x
vec.v[0] = 1.0f;     // mismo dato

// 3. _Generic (despacho por tipo):
#define print_val(x) _Generic((x),    \
    int:    printf("%d\n", x),         \
    double: printf("%f\n", x),         \
    char*:  printf("%s\n", x)          \
)

int n = 42;
double d = 3.14;
print_val(n);         // llama a printf("%d\n", n)
print_val(d);         // llama a printf("%f\n", d)

// 4. <threads.h> — threads en el estándar:
#include <threads.h>
thrd_t thread;
mtx_t mutex;
// thrd_create(), thrd_join(), mtx_lock(), mtx_unlock()

// 5. <stdatomic.h> — operaciones atómicas:
#include <stdatomic.h>
_Atomic int counter = 0;
atomic_fetch_add(&counter, 1);    // incremento atómico

// 6. _Noreturn:
_Noreturn void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

// 7. _Alignas y _Alignof:
_Alignas(16) int aligned_array[4];   // alineado a 16 bytes
size_t align = _Alignof(double);      // alineación de double

// 8. <uchar.h> — UTF-16 y UTF-32:
char16_t c16 = u'A';
char32_t c32 = U'A';

// 9. Bounds-checking interfaces (Annex K) — opcional:
// gets_s, printf_s, strcpy_s...
// Pocos compiladores lo implementan
```

## C17 (C18) — Solo correcciones

C17 no agregó **ninguna feature nueva**. Solo corrigió defectos
y ambigüedades del texto de C11:

```c
// C17 = C11 con correcciones editoriales.
// No hay nada que "solo funcione en C17 y no en C11".
// Es más una errata oficial que una nueva versión.

// En la práctica:
// gcc -std=c17 y gcc -std=c11 producen el mismo código.
// La diferencia es el valor de __STDC_VERSION__:
// C11: __STDC_VERSION__ == 201112L
// C17: __STDC_VERSION__ == 201710L

// GCC usa -std=gnu17 como default desde GCC 8.
```

## C23 — La revisión más reciente

C23 es la revisión más grande desde C99. Incorpora muchas ideas
de C++ y moderniza significativamente el lenguaje:

```c
// 1. constexpr (diferente a C++):
constexpr int MAX = 100;
// Similar a const pero evaluado en compilación
// Más restringido que en C++

// 2. typeof y typeof_unqual:
int x = 42;
typeof(x) y = 10;           // y es int
typeof_unqual(x) z = 20;    // z es int (sin const/volatile)
// Antes era extensión GNU, ahora es estándar

// 3. nullptr (reemplaza NULL):
int *p = nullptr;            // tipo nullptr_t, no (void*)0

// 4. true y false como keywords:
// Ya no necesitan stdbool.h
bool flag = true;            // bool, true, false son keywords

// 5. auto para deducción de tipo (como C++):
auto x = 42;                 // x es int
// Limitado comparado con C++

// 6. __VA_OPT__ en macros variádicas:
#define LOG(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
LOG("hello");                // printf("hello")
LOG("x=%d", 42);             // printf("x=%d", 42)
// Resuelve el problema de la coma extra con ##__VA_ARGS__

// 7. Atributos estándar:
[[nodiscard]] int compute(void);    // warning si se ignora el resultado
[[maybe_unused]] int x;             // no warning por no usarla
[[deprecated]] void old_func(void); // warning si se usa
[[noreturn]] void die(void);        // reemplaza _Noreturn

// 8. Binary literals:
int mask = 0b11110000;       // antes era extensión GNU

// 9. Digit separators:
int million = 1'000'000;     // legibilidad

// 10. #embed — incluir datos binarios:
const unsigned char icon[] = {
    #embed "icon.png"
};

// 11. #elifdef, #elifndef:
#ifdef A
// ...
#elifdef B                   // antes: #elif defined(B)
// ...
#endif

// 12. static_assert sin mensaje (como C++):
static_assert(sizeof(int) == 4);   // sin segundo argumento

// 13. Eliminaciones:
// - Trigraphs eliminados
// - K&R function definitions eliminadas
// - gets() eliminada (ya deprecated desde C11)
```

## Línea temporal

```
      K&R      C89/C90         C99              C11           C17    C23
  ───┬──────────┬──────────────┬────────────────┬─────────────┬──────┬───
     1978      1989           1999             2011          2018   2024

  Antes de    Primera        Gran             Threads,      Solo   Modernización
  estándar    estandarización modernización   atomics,      fixes  mayor, ideas
              prototipos,    int en for,      _Generic,            de C++
              const, void    stdint, VLA,     static_assert
                             bool, //
```

## Tabla comparativa

| Feature | C89 | C99 | C11 | C17 | C23 |
|---|---|---|---|---|---|
| // comentarios | No | Sí | Sí | Sí | Sí |
| Variables en for | No | Sí | Sí | Sí | Sí |
| long long | No | Sí | Sí | Sí | Sí |
| stdint.h | No | Sí | Sí | Sí | Sí |
| bool (stdbool.h) | No | Sí | Sí | Sí | keyword |
| VLAs | No | Obligatorio | Opcional | Opcional | Opcional |
| Designated init | No | Sí | Sí | Sí | Sí |
| restrict | No | Sí | Sí | Sí | Sí |
| _Static_assert | No | No | Sí | Sí | sin msg ok |
| _Generic | No | No | Sí | Sí | Sí |
| threads.h | No | No | Sí | Sí | Sí |
| stdatomic.h | No | No | Sí | Sí | Sí |
| _Noreturn | No | No | Sí | Sí | [[noreturn]] |
| typeof | No | No | No | No | Sí |
| constexpr | No | No | No | No | Sí |
| nullptr | No | No | No | No | Sí |
| Binary literals | No | No | No | No | Sí |
| Attributes [[]] | No | No | No | No | Sí |

## __STDC_VERSION__

```c
// Cada estándar define una macro para identificarse:

#if defined(__STDC_VERSION__)
    #if __STDC_VERSION__ >= 202311L
        // C23
    #elif __STDC_VERSION__ >= 201710L
        // C17
    #elif __STDC_VERSION__ >= 201112L
        // C11
    #elif __STDC_VERSION__ >= 199901L
        // C99
    #endif
#else
    // C89/C90 (no define __STDC_VERSION__)
#endif

// Útil para código portable que adapta su comportamiento
// según el estándar disponible.
```

---

## Ejercicios

### Ejercicio 1 — Identificar el estándar

```c
// ¿Cuál es el estándar MÍNIMO necesario para compilar
// cada uno de estos fragmentos?

// Fragmento A:
for (int i = 0; i < 10; i++) { }

// Fragmento B:
_Static_assert(sizeof(int) == 4, "need 32-bit int");

// Fragmento C:
typeof(42) x = 100;

// Fragmento D:
const int MAX = 100;

// Fragmento E:
int arr[n];  // n es un parámetro de función
```

### Ejercicio 2 — __STDC_VERSION__

```c
// Compilar este programa con -std=c89, -std=c99, -std=c11, -std=c17
// ¿Qué imprime en cada caso?

#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("STDC_VERSION: %ld\n", __STDC_VERSION__);
#else
    printf("Pre-C99 (no __STDC_VERSION__)\n");
#endif
    return 0;
}
```

### Ejercicio 3 — Compatibilidad

```c
// Reescribir este código C99 para que compile en C89.
// ¿Qué pierdes?

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

int main(void) {
    bool found = false;
    for (int32_t i = 0; i < 100; i++) {
        if (i == 42) {
            found = true;
            // encontrado
        }
    }
    printf("%d\n", found);
    return 0;
}
```
