# T01 — Evolución de C

> **Errata del material base**
>
> | Ubicación | Error | Corrección |
> |-----------|-------|------------|
> | `README.md:277` | Dice *"GCC usa -std=gnu17 como default desde GCC 8"*. | El default de GCC cambió así: **GCC 5** (2015): gnu89 → gnu11. **GCC 14** (2024): gnu11 → gnu17. **GCC 15** (2025): gnu17 → gnu23. En GCC 8 (2018) el default seguía siendo `-std=gnu11`. |

---

## 1. Línea temporal

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

---

## 2. K&R C (1978) — El C original

Antes de cualquier estándar, K&R C era el "estándar de facto" definido por el libro de Kernighan y Ritchie:

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

---

## 3. C89/C90 (ANSI C) — Primera estandarización

ANSI estandarizó C en 1989 (C89). ISO lo adoptó en 1990 (C90). Son el mismo estándar.

```c
// C89 introdujo:

// 1. Prototipos de función — verificación de tipos:
int add(int a, int b);         // el compilador verifica los tipos

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
// 1. Variables solo al inicio del bloque:
int main(void) {
    int x = 5;
    int y = 10;
    x = x + y;
    int z = x;            // ERROR en C89 — declaración después de statement
    return 0;
}

// 2. Sin comentarios //
/* este es el único estilo en C89 */

// 3. Sin long long, stdint.h, _Bool/stdbool.h
// 4. Sin VLAs
// 5. Sin snprintf
```

---

## 4. C99 — Modernización

C99 fue la primera gran actualización:

```c
// 1. Declaraciones en cualquier parte del bloque:
for (int i = 0; i < 10; i++) {   // int i DENTRO del for
    int temp = arr[i];            // declaración después de código
}

// 2. Comentarios // en el estándar

// 3. long long (al menos 64 bits):
long long big = 9223372036854775807LL;

// 4. <stdint.h> — tipos de tamaño fijo:
#include <stdint.h>
int8_t   a;    // exactamente 8 bits
int32_t  c;    // exactamente 32 bits
uint32_t e;    // unsigned, exactamente 32 bits

// 5. <stdbool.h> — tipo bool:
#include <stdbool.h>
bool flag = true;          // _Bool + macros true/false

// 6. Variable-length arrays (VLAs):
void foo(int n) {
    int arr[n];            // tamaño determinado en runtime
}

// 7. Designated initializers:
struct Point { int x; int y; int z; };
struct Point p = { .x = 10, .z = 30 };   // .y = 0 implícito
int arr[5] = { [2] = 42, [4] = 99 };     // {0, 0, 42, 0, 99}

// 8. Compound literals:
struct Point p = (struct Point){ .x = 5, .y = 10 };

// 9. restrict:
void copy(int *restrict dst, const int *restrict src, int n);
// Promesa al compilador: dst y src no se solapan

// 10. Inline functions:
inline int square(int x) { return x * x; }

// 11. snprintf() — seguro contra buffer overflow:
char buf[20];
snprintf(buf, sizeof(buf), "x = %d", 42);

// 12. __func__, _Pragma, <math.h> mejorado
```

### VLAs — Controversia

```c
void foo(int n) {
    int arr[n];    // se alloca en el stack
    // ¿Qué pasa si n = 1000000?
    // Stack overflow — sin forma de detectar el error
}

// Por estas razones:
// - C11 hizo los VLAs opcionales (__STDC_NO_VLA__)
// - C23 mantiene los VLAs como opcionales
// - Muchos coding standards los prohíben (MISRA, CERT)
// - Linux kernel eliminó su uso (2018, v4.20)
// - Mejor usar malloc() para tamaños variables
```

---

## 5. C11 — Concurrencia y seguridad

```c
// 1. _Static_assert (verificación en compilación):
#include <assert.h>
_Static_assert(sizeof(int) == 4, "int must be 4 bytes");
// Si la condición es falsa, la compilación falla con el mensaje

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
print_val(42);        // printf("%d\n", 42)
print_val(3.14);      // printf("%f\n", 3.14)

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
size_t align = _Alignof(double);

// 8. <uchar.h> — UTF-16 y UTF-32:
char16_t c16 = u'A';
char32_t c32 = U'A';

// 9. Bounds-checking interfaces (Annex K) — opcional:
// gets_s, printf_s, strcpy_s — pocos compiladores lo implementan
```

---

## 6. C17 (C18) — Solo correcciones

C17 no agregó **ninguna feature nueva**. Solo corrigió defectos y ambigüedades del texto de C11:

```c
// C17 = C11 con correcciones editoriales.
// No hay nada que "solo funcione en C17 y no en C11".

// En la práctica:
// gcc -std=c17 y gcc -std=c11 producen el mismo código.
// La diferencia es el valor de __STDC_VERSION__:
// C11: __STDC_VERSION__ == 201112L
// C17: __STDC_VERSION__ == 201710L
```

---

## 7. C23 — La revisión más reciente

C23 es la revisión más grande desde C99. Incorpora muchas ideas de C++:

```c
// 1. constexpr (diferente a C++):
constexpr int MAX = 100;
// Similar a const pero evaluado en compilación. Más restringido que en C++.

// 2. typeof y typeof_unqual:
int x = 42;
typeof(x) y = 10;           // y es int
typeof_unqual(x) z = 20;    // z es int (sin const/volatile)
// Antes era extensión GNU, ahora es estándar

// 3. nullptr (reemplaza NULL):
int *p = nullptr;            // tipo nullptr_t, no (void*)0

// 4. true y false como keywords:
bool flag = true;            // bool, true, false ya no necesitan stdbool.h

// 5. auto para deducción de tipo:
auto x = 42;                 // x es int (limitado vs C++)

// 6. __VA_OPT__ en macros variádicas:
#define LOG(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
LOG("hello");                // printf("hello") — sin coma extra
LOG("x=%d", 42);             // printf("x=%d", 42)

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

// 12. static_assert sin mensaje:
static_assert(sizeof(int) == 4);   // sin segundo argumento

// 13. Eliminaciones:
// - Trigraphs eliminados
// - K&R function definitions eliminadas
// - gets() eliminada (ya deprecated desde C11)
```

---

## 8. Tabla comparativa

| Feature | C89 | C99 | C11 | C17 | C23 |
|---------|-----|-----|-----|-----|-----|
| `//` comentarios | No | Sí | Sí | Sí | Sí |
| Variables en `for` | No | Sí | Sí | Sí | Sí |
| `long long` | No | Sí | Sí | Sí | Sí |
| `stdint.h` | No | Sí | Sí | Sí | Sí |
| `bool` (`stdbool.h`) | No | Sí | Sí | Sí | keyword |
| VLAs | No | Obligatorio | Opcional | Opcional | Opcional |
| Designated init | No | Sí | Sí | Sí | Sí |
| `restrict` | No | Sí | Sí | Sí | Sí |
| `_Static_assert` | No | No | Sí | Sí | sin msg ok |
| `_Generic` | No | No | Sí | Sí | Sí |
| `threads.h` | No | No | Sí | Sí | Sí |
| `stdatomic.h` | No | No | Sí | Sí | Sí |
| `_Noreturn` | No | No | Sí | Sí | `[[noreturn]]` |
| `typeof` | No | No | No | No | Sí |
| `constexpr` | No | No | No | No | Sí |
| `nullptr` | No | No | No | No | Sí |
| Binary literals | No | No | No | No | Sí |
| Atributos `[[]]` | No | No | No | No | Sí |

---

## 9. __STDC_VERSION__

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

### Default de GCC por versión

```
GCC 1-4:  -std=gnu89  (K&R + extensiones)
GCC 5-13: -std=gnu11  (C11 + extensiones GNU)
GCC 14:   -std=gnu17  (C17 + extensiones GNU)
GCC 15+:  -std=gnu23  (C23 + extensiones GNU)
```

Nota: `gnu*` = estándar C + extensiones GNU. `-std=c11` es C11 estricto (sin extensiones).

---

## Ejercicios

### Ejercicio 1 — Identificar el estándar mínimo

¿Cuál es el estándar **mínimo** necesario para compilar cada fragmento?

```c
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

**Predicción**: Antes de mirar la respuesta, anota tu respuesta para cada uno:

<details><summary>Respuestas</summary>

| Fragmento | Estándar mínimo | Razón |
|-----------|----------------|-------|
| A | **C99** | Declaración de variable dentro de `for` — no existe en C89 |
| B | **C11** | `_Static_assert` fue introducido en C11 |
| C | **C23** | `typeof` era extensión GNU; se estandarizó en C23 |
| D | **C89** | `const` fue introducido en C89 (ANSI C) |
| E | **C99** | VLAs fueron introducidos en C99 (opcionales desde C11) |

Nota sobre C: si usas `-std=gnu11`, `typeof` funciona como extensión GNU. Pero como feature **estándar**, requiere C23.

</details>

---

### Ejercicio 2 — __STDC_VERSION__ con distintos estándares

Crea un archivo `version.c`:

```c
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

Compila con cada estándar:

```bash
gcc -std=c89 version.c -o v89 && ./v89
gcc -std=c99 version.c -o v99 && ./v99
gcc -std=c11 version.c -o v11 && ./v11
gcc -std=c17 version.c -o v17 && ./v17
gcc -std=c23 version.c -o v23 && ./v23
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué imprimirá cada uno? ¿Qué pasa con -std=c89?</summary>

```
-std=c89: Pre-C99 (no __STDC_VERSION__)
-std=c99: STDC_VERSION: 199901
-std=c11: STDC_VERSION: 201112
-std=c17: STDC_VERSION: 201710
-std=c23: STDC_VERSION: 202311
```

C89 no define `__STDC_VERSION__` — la macro no existía aún. Fue introducida en el amendment de 1994 a C89 (C94/C95, con valor `199409L`), y luego cada estándar posterior define su propio valor.

Nota: con `-std=c89`, GCC emitirá un warning sobre los comentarios `//` en el `#else` porque no son válidos en C89. Los comentarios `//` requieren C99.

</details>

Limpieza:

```bash
rm -f version.c v89 v99 v11 v17 v23
```

---

### Ejercicio 3 — Reescribir C99 como C89

Reescribe este código C99 para que compile con `-std=c89 -pedantic`:

```c
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

**Predicción**: Antes de reescribir, identifica qué necesita cambiar:

<details><summary>¿Cuántos cambios son necesarios? ¿Qué se pierde?</summary>

**5 cambios**:

```c
#include <stdio.h>
/* sin stdbool.h ni stdint.h — no existen en C89 */

int main(void) {
    int found = 0;                 /* bool → int, false → 0 */
    long i;                        /* int32_t → long, declarar al inicio */
    for (i = 0; i < 100; i++) {   /* no declarar int en for */
        if (i == 42) {
            found = 1;             /* true → 1 */
            /* encontrado */       /* // → comentario estilo C89 */
        }
    }
    printf("%d\n", found);
    return 0;
}
```

| Cambio | C99 | C89 | Qué se pierde |
|--------|-----|-----|---------------|
| `stdbool.h` | `bool`, `true`, `false` | `int`, `1`, `0` | Semántica: no queda claro que es un flag booleano |
| `stdint.h` | `int32_t` (exactamente 32 bits) | `long` (al menos 32 bits) | Garantía de tamaño exacto |
| Variable en `for` | `for (int i = ...)` | Declarar antes del `for` | Scope limitado (i visible fuera del for) |
| Comentarios `//` | Válidos | No válidos | Conveniencia |

El código C89 funciona igual pero pierde **expresividad** y **garantías de tipo**.

</details>

---

### Ejercicio 4 — C11 features: _Static_assert

¿Qué pasa al compilar cada uno?

```bash
echo '
#include <assert.h>
static_assert(sizeof(int) == 4, "int must be 4 bytes");
int main(void) { return 0; }
' | gcc -std=c11 -x c -

echo '
#include <assert.h>
static_assert(sizeof(char) == 2, "char must be 2 bytes");
int main(void) { return 0; }
' | gcc -std=c11 -x c -
```

**Predicción**: Antes de compilar, responde:

<details><summary>¿Cuál compila y cuál falla? ¿El error es en runtime o en compilación?</summary>

- **Primero**: **compila** — `sizeof(int) == 4` es verdadero en esta plataforma (x86_64 Linux).
- **Segundo**: **error de compilación** — `sizeof(char)` es siempre 1 por definición del estándar (§6.5.3.4). La condición es falsa, y GCC produce:

```
error: static assertion failed: "char must be 2 bytes"
```

El error es en **compilación**, no en runtime. Esa es la ventaja de `_Static_assert` / `static_assert`: verifica condiciones en tiempo de compilación, sin coste en runtime. Útil para verificar suposiciones sobre tamaños de tipos, alignment, endianness, etc.

</details>

---

### Ejercicio 5 — _Generic: despacho por tipo

¿Qué imprimirá este programa?

```c
#include <stdio.h>

#define type_name(x) _Generic((x), \
    int: "int",                     \
    float: "float",                 \
    double: "double",               \
    char*: "char*",                 \
    default: "other"                \
)

int main(void) {
    int a = 42;
    float b = 3.14f;
    double c = 2.71;
    char *s = "hello";
    long d = 100L;

    printf("a: %s\n", type_name(a));
    printf("b: %s\n", type_name(b));
    printf("c: %s\n", type_name(c));
    printf("s: %s\n", type_name(s));
    printf("d: %s\n", type_name(d));

    return 0;
}
```

Compílalo con `-std=c11`:

```bash
gcc -std=c11 -o generic generic.c && ./generic
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué tipo se selecciona para cada variable? ¿Qué pasa con d (long)?</summary>

```
a: int
b: float
c: double
s: char*
d: other
```

`_Generic` selecciona la rama que coincide con el **tipo de la expresión** en compilación:
- `a` (int) → `"int"`
- `b` (float) → `"float"`
- `c` (double) → `"double"`
- `s` (char*) → `"char*"`
- `d` (long) → **`"other"`** — no hay rama para `long`, cae en `default`

`_Generic` es resolución en **compilación** (no runtime). No hay overhead. Es la forma de C de hacer "sobrecarga de funciones" según el tipo — algo que C++ hace con templates y overloading.

</details>

Limpieza:

```bash
rm -f generic generic.c
```

---

### Ejercicio 6 — Compilar con estándar equivocado

¿Qué errores produce compilar código C99 con `-std=c89 -pedantic`?

```bash
echo '
#include <stdio.h>
int main(void) {
    for (int i = 0; i < 5; i++) {
        printf("%d\n", i);
    }
    return 0;
}
' | gcc -std=c89 -pedantic -x c - -o test89
```

**Predicción**: Antes de compilar, responde:

<details><summary>¿El compilador producirá error o warning? ¿Se generará el binario?</summary>

GCC produce **warnings** (no errores) con `-pedantic`:

```
warning: ISO C90 forbids mixed declarations and code
```

El binario **sí se genera** porque `-pedantic` emite warnings, no errores. Para que falle la compilación, se necesita `-pedantic-errors` o `-Werror`:

```bash
# Esto SÍ falla:
echo '...' | gcc -std=c89 -pedantic-errors -x c - -o test89
```

Lección: `-std=c89` le dice a GCC qué estándar usar, y `-pedantic` le dice que advierta sobre extensiones no conformes. Sin `-pedantic`, GCC acepta silenciosamente extensiones GNU incluso con `-std=c89`.

</details>

Limpieza:

```bash
rm -f test89
```

---

### Ejercicio 7 — VLAs: funcionar y fallar

```c
#include <stdio.h>

void print_sum(int n) {
    int arr[n];
    for (int i = 0; i < n; i++) arr[i] = i + 1;
    int sum = 0;
    for (int i = 0; i < n; i++) sum += arr[i];
    printf("sum(1..%d) = %d\n", n, sum);
}

int main(void) {
    print_sum(10);           // funciona
    print_sum(100);          // funciona
    // print_sum(10000000);  // stack overflow probable
    return 0;
}
```

Compila con `-std=c99` y con `-std=c11`:

```bash
gcc -std=c99 -o vla vla.c && ./vla
gcc -std=c11 -o vla11 vla.c && ./vla11
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Ambos compilan? ¿Qué pasa si descomentaras print_sum(10000000)?</summary>

**Ambos compilan y producen la misma salida**:
```
sum(1..10) = 55
sum(1..100) = 5050
```

En C99, los VLAs son obligatorios. En C11, son opcionales — pero GCC los soporta de todos modos (el compilador define `__STDC_NO_VLA__` solo si NO los soporta).

Si descomentaras `print_sum(10000000)`:
- `int arr[10000000]` intenta alocar ~40 MB en el stack
- El stack típico es 8 MB (`ulimit -s`)
- **Stack overflow** → Segmentation fault
- No hay forma de verificar si la alocación del VLA tuvo éxito
- Por eso la recomendación es usar `malloc` para tamaños variables

</details>

Limpieza:

```bash
rm -f vla vla11 vla.c
```

---

### Ejercicio 8 — C23: typeof

Compila con `-std=c23` (o `-std=gnu23`):

```bash
echo '
#include <stdio.h>

int main(void) {
    int x = 42;
    typeof(x) y = x * 2;
    const int cx = 10;
    typeof(cx) cy = 20;
    typeof_unqual(cx) z = 30;

    y = 100;    // OK?
    // cy = 50; // OK?
    z = 60;     // OK?

    printf("y=%d z=%d\n", y, z);
    return 0;
}
' | gcc -std=c23 -Wall -x c - -o typeof_test && ./typeof_test
```

**Predicción**: Antes de compilar, responde:

<details><summary>¿Qué tipo tienen y, cy y z? ¿Cuáles asignaciones son válidas?</summary>

- `typeof(x)` donde x es `int` → **`y` es `int`** → `y = 100` es **válido**
- `typeof(cx)` donde cx es `const int` → **`cy` es `const int`** → `cy = 50` sería **error** (asignar a const)
- `typeof_unqual(cx)` → **`z` es `int`** (quita el `const`) → `z = 60` es **válido**

Salida: `y=100 z=60`

`typeof_unqual` es útil cuando necesitas una variable del mismo tipo base pero sin los qualifiers (`const`, `volatile`). Sin `typeof_unqual`, tendrías que conocer el tipo base para escribirlo explícitamente.

</details>

Limpieza:

```bash
rm -f typeof_test
```

---

### Ejercicio 9 — C23: nullptr vs NULL

```bash
echo '
#include <stdio.h>
#include <stddef.h>

int foo(int x)    { return 1; }
int foo_ptr(int *p) { return 2; }

int main(void) {
    // NULL es (void*)0 o 0 — ambiguo
    // nullptr es de tipo nullptr_t — solo puntero

    int *p1 = NULL;
    int *p2 = nullptr;

    printf("p1 == nullptr: %d\n", p1 == nullptr);
    printf("p2 == NULL: %d\n", p2 == nullptr);
    printf("nullptr == 0: %d\n", nullptr == 0);

    return 0;
}
' | gcc -std=c23 -Wall -x c - -o nullptr_test && ./nullptr_test
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué imprimirá? ¿Por qué nullptr es preferible a NULL?</summary>

```
p1 == nullptr: 1
p2 == NULL: 1
nullptr == 0: 1
```

Todas las comparaciones son verdaderas: `NULL`, `nullptr` y `0` representan el puntero nulo.

`nullptr` es preferible a `NULL` porque:
- `NULL` puede ser `(void*)0` o simplemente `0` (un entero) según la implementación
- En C++, `NULL` como `0` puede hacer que `foo(NULL)` llame a `foo(int)` en vez de `foo(int*)` — ambigüedad
- `nullptr` tiene tipo `nullptr_t` — es inequívocamente un puntero, no un entero
- En C23, `nullptr` resuelve la misma ambigüedad para el futuro

</details>

Limpieza:

```bash
rm -f nullptr_test
```

---

### Ejercicio 10 — Verificar el default de tu GCC

Sin especificar `-std`, ¿qué estándar usa tu GCC por defecto?

```bash
echo '
#include <stdio.h>
int main(void) {
#ifdef __STDC_VERSION__
    long ver = __STDC_VERSION__;
    printf("__STDC_VERSION__ = %ld\n", ver);
    if      (ver >= 202311L) printf("Default: C23 (gnu23)\n");
    else if (ver >= 201710L) printf("Default: C17 (gnu17)\n");
    else if (ver >= 201112L) printf("Default: C11 (gnu11)\n");
    else if (ver >= 199901L) printf("Default: C99 (gnu99)\n");
    else                     printf("Default: pre-C99\n");
#else
    printf("Default: C89/C90 (gnu89)\n");
#endif
    return 0;
}
' | gcc -x c - -o detect_std && ./detect_std
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué estándar usará por defecto tu GCC?</summary>

En este sistema (GCC 15.2.1, Fedora 43):

```
__STDC_VERSION__ = 202311
Default: C23 (gnu23)
```

GCC 15 usa `-std=gnu23` por defecto — el más reciente. La progresión histórica:

| GCC | Default | Estándar |
|-----|---------|----------|
| 1-4 | `-std=gnu89` | C89 + extensiones GNU |
| 5-13 | `-std=gnu11` | C11 + extensiones GNU |
| 14 | `-std=gnu17` | C17 + extensiones GNU |
| 15+ | `-std=gnu23` | C23 + extensiones GNU |

Nota: `gnu*` = estándar + extensiones GNU (typeof antes de C23, statement expressions, nested functions, etc.). Para estándar estricto sin extensiones, usar `-std=c11`, `-std=c17`, etc.

Para el curso, `-std=c11` es la recomendación: ampliamente soportado, con todas las features modernas necesarias.

</details>

Limpieza:

```bash
rm -f detect_std
```
