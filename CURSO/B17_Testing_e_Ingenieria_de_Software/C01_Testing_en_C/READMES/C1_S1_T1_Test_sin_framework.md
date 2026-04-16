# Test sin framework — assert.h, funciones main() de prueba, exit codes, por que C no tiene testing built-in

## 1. Introduccion

C es probablemente el unico lenguaje de sistemas ampliamente utilizado que **no incluye absolutamente ninguna infraestructura de testing** en su especificacion ni en su toolchain. No hay un comando `cc test`, no hay un atributo `#[test]`, no hay un modulo `testing` en la stdlib. Cuando quieres verificar que tu codigo C funciona, estas por tu cuenta — y eso es una decision de diseno, no un descuido.

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                TESTING EN LENGUAJES DE SISTEMAS — COMPARACION                       │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  Rust        Go           C              C++                                        │
│  ────        ──           ─              ───                                        │
│  #[test]     func Test*   ??? (nada)     ??? (nada)                                │
│  cargo test  go test      ???            ???                                        │
│  assert!     t.Error()    assert()       static_assert (C++11)                      │
│  assert_eq!  t.Fatal()    printf+exit    (frameworks externos)                      │
│  mod tests   _test.go     (manual)       (Google Test, Catch2)                      │
│                                                                                     │
│  Built-in:   Built-in:    Manual:        Manual:                                    │
│  - runner    - runner     - tu main()    - tu main()                                │
│  - asserts   - asserts    - assert.h     - assert macro                             │
│  - output    - output     - exit codes   - exit codes                               │
│  - filter    - filter     - printf       - printf/cout                              │
│  - coverage  - coverage   - (nada)       - (nada)                                   │
│                                                                                     │
│  ┌──────────────────────────────────────────────────────────────────────────┐        │
│  │   C te da UNA herramienta: assert() en <assert.h>                      │        │
│  │   Todo lo demas lo construyes tu o usas un framework externo           │        │
│  └──────────────────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Por que C no tiene testing built-in

La respuesta corta es **filosofia de diseno**. C fue creado en 1972 por Dennis Ritchie para escribir Unix. La filosofia era:

1. **Minimalismo radical**: el lenguaje debe ser pequeno. Lo que puedes hacer en una libreria no debe estar en el lenguaje.
2. **El programador sabe lo que hace**: no hay runtime checks (bounds checking, null checking, type checking en runtime). Si el programador quiere verificar algo, lo escribe el mismo.
3. **La stdlib es minima**: la C Standard Library tiene ~200 funciones. Compare con Go (~150 paquetes con miles de funciones) o Rust (stdlib + crates.io).
4. **Separacion de responsabilidades**: el compilador compila, el linker enlaza, el shell ejecuta. Testing no es responsabilidad de ninguno de estos.

La consecuencia practica:

```
Rust:  cargo test                     ← una herramienta, un comando
Go:    go test ./...                  ← una herramienta, un comando
C:     gcc -o test_math test_math.c   ← tu compilas un ejecutable
       ./test_math                    ← tu lo ejecutas
       echo $?                        ← tu verificas el resultado
```

En C, un test es **simplemente un programa** que:
- Ejecuta el codigo que quieres probar
- Verifica que los resultados son correctos
- Sale con codigo 0 si todo esta bien, distinto de 0 si algo fallo

No hay magia. No hay framework. Es un programa como cualquier otro.

### 1.2 Ventajas de no tener testing built-in

Puede parecer una desventaja, pero hay beneficios reales:

| Aspecto | Ventaja |
|---------|---------|
| **Transparencia total** | Cada test es un programa que puedes compilar, ejecutar, depurar con gdb, y analizar con Valgrind. No hay capas de abstraccion ocultas |
| **Sin dependencias** | Un test en C puro no necesita instalar nada — solo el compilador que ya tienes |
| **Portabilidad** | Funciona en cualquier plataforma que tenga un compilador C. No dependes de un runner especifico |
| **Control total** | Tu decides como organizar tests, como reportar resultados, como manejar setup/teardown |
| **Educativo** | Entiendes exactamente que es un test y como funciona, sin magia |

### 1.3 Desventajas reales

| Aspecto | Problema |
|---------|---------|
| **Boilerplate** | Cada test necesita su propio main(), compilacion, ejecucion |
| **Sin autodiscovery** | No hay forma automatica de encontrar y ejecutar todos los tests |
| **Sin reporte** | No hay output estandarizado (pass/fail, tiempo, nombre del test) |
| **Sin aislamiento** | Un assert que falla aborta todo el programa — no se ejecutan los demas tests |
| **Sin paralelismo** | Ejecutas un programa a la vez (a menos que uses Make para paralelizar) |

Estas desventajas son exactamente lo que resuelven frameworks como Unity, CMocka, o Check — pero antes de llegar a ellos, necesitas entender los fundamentos que este topico cubre.

---

## 2. assert.h — La unica herramienta de testing que C te da

### 2.1 Que es assert()

`assert()` es una macro definida en `<assert.h>` que verifica una expresion en runtime. Si la expresion es falsa (evalua a 0), el programa aborta con un mensaje de error. Si es verdadera (evalua a distinto de 0), no hace nada.

```c
#include <assert.h>

// Prototipo conceptual (es una macro, no una funcion):
// void assert(scalar expression);
```

La especificacion dice que `assert` debe:
1. Evaluar la expresion
2. Si es falsa: escribir un mensaje diagnostico en stderr y llamar a `abort()`
3. Si es verdadera: no hacer nada (no-op)

### 2.2 Anatomia de un assert

```c
// test_basic.c
#include <assert.h>
#include <stdio.h>

int sum(int a, int b) {
    return a + b;
}

int main(void) {
    // assert exitoso — no produce output, no hace nada visible
    assert(sum(2, 3) == 5);

    // assert fallido — aborta el programa con informacion util
    assert(sum(2, 3) == 6);  // BOOM

    // Esta linea NUNCA se ejecuta si el assert anterior falla
    printf("All tests passed!\n");

    return 0;
}
```

Compilar y ejecutar:

```bash
$ gcc -o test_basic test_basic.c
$ ./test_basic
test_basic: test_basic.c:14: main: Assertion `sum(2, 3) == 6' failed.
Aborted (core dumped)
$ echo $?
134    # 128 + 6 (SIGABRT = signal 6)
```

El mensaje de error de `assert` contiene:
- **Nombre del archivo**: `test_basic.c`
- **Numero de linea**: `14`
- **Nombre de la funcion**: `main`
- **La expresion textual que fallo**: `sum(2, 3) == 6`

Esto es posible porque `assert` es una **macro**, no una funcion. Las macros tienen acceso a `__FILE__`, `__LINE__`, y `__func__` (C99), lo que les permite incluir informacion del sitio de llamada.

### 2.3 Implementacion tipica de assert

La implementacion real varia segun el compilador/plataforma, pero conceptualmente:

```c
// Implementacion simplificada de <assert.h>
// (NO uses esto — usa el <assert.h> del sistema)

#ifdef NDEBUG
    #define assert(expr) ((void)0)
#else
    #define assert(expr) \
        ((expr) \
            ? ((void)0) \
            : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif
```

Puntos clave:

| Elemento | Significado |
|----------|-------------|
| `#expr` | El operador de stringify — convierte la expresion en string literal. `assert(x > 0)` produce `"x > 0"` |
| `__FILE__` | Macro predefinida — nombre del archivo fuente |
| `__LINE__` | Macro predefinida — numero de linea actual |
| `__func__` | Identificador predefinido (C99) — nombre de la funcion actual |
| `((void)0)` | No-op que evita warnings de "statement with no effect" |
| `NDEBUG` | Si esta definida, assert se convierte en no-op. **Esto es critico** |

### 2.4 El problema de NDEBUG — assert no es para produccion

```c
// Compilacion normal — asserts activos
$ gcc -o test_prog test_prog.c
$ ./test_prog
test_prog: test_prog.c:14: main: Assertion `result == 42' failed.

// Compilacion con NDEBUG — asserts ELIMINADOS completamente
$ gcc -DNDEBUG -o test_prog test_prog.c
$ ./test_prog
// (no output, no verificacion, el programa continua como si nada)
```

`NDEBUG` significa "No Debug". Cuando se define:
- **Todas las llamadas a assert() desaparecen** — se reemplazan por `((void)0)`
- Las expresiones dentro de assert **no se evaluan** — esto incluye efectos secundarios
- El binario resultante es mas rapido (sin branch para cada assert) y mas pequeno

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PELIGRO: EFECTOS SECUNDARIOS EN ASSERT          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  INCORRECTO:                                                        │
│  assert(read(fd, buf, 4096) > 0);   // con NDEBUG, read() no       │
│                                     // se ejecuta. El programa      │
│                                     // pierde la lectura.           │
│                                                                     │
│  assert(ptr = malloc(100));         // con NDEBUG, malloc() no      │
│                                     // se ejecuta. ptr es basura.   │
│                                                                     │
│  assert(i++ < limit);              // con NDEBUG, i nunca se        │
│                                     // incrementa. Loop infinito.   │
│                                                                     │
│  CORRECTO:                                                          │
│  ssize_t n = read(fd, buf, 4096);                                   │
│  assert(n > 0);                                                     │
│                                                                     │
│  ptr = malloc(100);                                                 │
│  assert(ptr != NULL);                                               │
│                                                                     │
│  i++;                                                               │
│  assert(i < limit);                                                 │
│                                                                     │
│  REGLA: assert() debe contener SOLO expresiones puras              │
│         (sin efectos secundarios, sin asignaciones, sin I/O)        │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.5 assert() vs validacion de errores — cuando usar cada uno

Esta es una distincion fundamental que muchos programadores confunden:

| Aspecto | assert() | Validacion (if + error handling) |
|---------|----------|----------------------------------|
| **Proposito** | Verificar invariantes del programador | Manejar condiciones que pueden ocurrir en runtime |
| **Quien causa el fallo** | Un bug en tu codigo | Input del usuario, condicion del sistema, caso borde |
| **Presente en produccion** | NO (eliminado con NDEBUG) | SI (siempre activo) |
| **Cuando falla** | El programa aborta inmediatamente | El programa maneja el error y continua |
| **Ejemplo** | `assert(idx < array_len)` | `if (fopen(path, "r") == NULL) { ... }` |

```c
// ASSERT — para cosas que NUNCA deberian pasar si el codigo es correcto
void process_node(Node *node) {
    assert(node != NULL);            // El caller DEBE pasar un nodo valido
    assert(node->type == NODE_EXPR); // Pre: solo se llama con nodos expresion
    assert(node->children_count > 0); // Invariante: las expresiones tienen hijos

    // ... procesar ...

    assert(result != NULL);          // Post: siempre produce un resultado
}

// VALIDACION — para cosas que PUEDEN pasar en uso normal
int parse_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {                 // El archivo puede no existir
        return -1;                   // Eso no es un bug, es una condicion normal
    }

    char buf[1024];
    if (fgets(buf, sizeof(buf), f) == NULL) {  // El archivo puede estar vacio
        fclose(f);
        return -2;
    }

    // ...
    fclose(f);
    return 0;
}
```

### 2.6 static_assert (C11) — Verificacion en tiempo de compilacion

C11 introdujo `_Static_assert` (y el alias `static_assert` via `<assert.h>`), que verifica condiciones **en tiempo de compilacion**:

```c
#include <assert.h>
#include <stdint.h>
#include <limits.h>

// Falla si la plataforma no cumple nuestras suposiciones
static_assert(sizeof(int) >= 4, "Need at least 32-bit int");
static_assert(CHAR_BIT == 8, "Char must be 8 bits");
static_assert(sizeof(void *) == 8, "Only 64-bit platforms supported");

// Util para verificar layout de structs (networking, archivos binarios)
typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
} Packet;

static_assert(sizeof(Packet) == 8, "Packet must be exactly 8 bytes (no padding)");

// Verificar que un enum cabe en el tipo esperado
typedef enum { A, B, C, STATE_COUNT } State;
static_assert(STATE_COUNT <= UINT8_MAX, "State must fit in uint8_t");
```

```bash
# Si falla, es un error de compilacion — no de ejecucion
$ gcc -std=c11 -o prog prog.c
prog.c:5:1: error: static assertion failed: "Need at least 32-bit int"
    5 | static_assert(sizeof(int) >= 4, "Need at least 32-bit int");
      | ^~~~~~~~~~~~~
```

| Caracteristica | assert() (runtime) | static_assert (compile-time) |
|----------------|--------------------|-----------------------------|
| Cuando se evalua | Al ejecutar el programa | Al compilar |
| Donde puede aparecer | Dentro de funciones | En cualquier scope (global, funcion, struct) |
| Que puede verificar | Cualquier expresion | Solo expresiones constantes (conocidas en compilacion) |
| Efecto de NDEBUG | Se desactiva | No le afecta — siempre activo |
| Disponible desde | C89 | C11 |

---

## 3. Funciones main() de prueba — El patron basico

### 3.1 El test mas simple posible

Un test en C sin framework es un programa con `main()` que ejecuta codigo, verifica resultados, y reporta:

```c
// math.h — el modulo a testear
#ifndef MATH_H
#define MATH_H

int sum(int a, int b);
int multiply(int a, int b);
int factorial(int n);
int gcd(int a, int b);

#endif
```

```c
// math.c — implementacion
#include "math.h"

int sum(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}
```

```c
// test_math.c — el test (un programa independiente)
#include <assert.h>
#include <stdio.h>
#include "math.h"

int main(void) {
    // Test sum
    assert(sum(2, 3) == 5);
    assert(sum(0, 0) == 0);
    assert(sum(-1, 1) == 0);
    assert(sum(-5, -3) == -8);

    // Test multiply
    assert(multiply(3, 4) == 12);
    assert(multiply(0, 100) == 0);
    assert(multiply(-2, 3) == -6);
    assert(multiply(-2, -3) == 6);

    // Test factorial
    assert(factorial(0) == 1);
    assert(factorial(1) == 1);
    assert(factorial(5) == 120);
    assert(factorial(10) == 3628800);

    // Test gcd
    assert(gcd(12, 8) == 4);
    assert(gcd(17, 13) == 1);  // primos entre si
    assert(gcd(100, 0) == 100);
    assert(gcd(0, 100) == 100);

    printf("All tests passed!\n");
    return 0;
}
```

```bash
$ gcc -Wall -Wextra -o test_math test_math.c math.c
$ ./test_math
All tests passed!
$ echo $?
0
```

### 3.2 El problema del assert duro — un fallo mata todo

```c
// test_math_v2.c — que pasa cuando un test falla?
#include <assert.h>
#include <stdio.h>
#include "math.h"

int main(void) {
    printf("Testing sum...\n");
    assert(sum(2, 3) == 5);       // PASS
    printf("  sum: basic OK\n");

    assert(sum(2, 3) == 999);     // FAIL — abort() aqui
    printf("  sum: otro OK\n");   // nunca se ejecuta

    printf("Testing multiply...\n"); // nunca se ejecuta
    assert(multiply(3, 4) == 12);    // nunca se ejecuta

    printf("All tests passed!\n");   // nunca se ejecuta
    return 0;
}
```

```bash
$ gcc -Wall -Wextra -o test_math_v2 test_math_v2.c math.c
$ ./test_math_v2
Testing sum...
  sum: basic OK
test_math_v2: test_math_v2.c:10: main: Assertion `sum(2, 3) == 999' failed.
Aborted (core dumped)
```

El primer assert que falla mata el proceso completo. No sabes si los demas tests pasan o fallan. Esto es un problema serio cuando tienes decenas de verificaciones.

### 3.3 Patron soft-assert — continuar despues de un fallo

La solucion clasica es reemplazar `assert()` con verificaciones que reportan el fallo pero **no abortan**:

```c
// test_framework_mini.h — un "micro-framework" casero
#ifndef TEST_FRAMEWORK_MINI_H
#define TEST_FRAMEWORK_MINI_H

#include <stdio.h>

static int test_failures = 0;
static int test_count = 0;

#define TEST_ASSERT(expr) \
    do { \
        test_count++; \
        if (!(expr)) { \
            fprintf(stderr, \
                "FAIL: %s:%d: %s: assertion '%s' failed\n", \
                __FILE__, __LINE__, __func__, #expr); \
            test_failures++; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        test_count++; \
        if ((a) != (b)) { \
            fprintf(stderr, \
                "FAIL: %s:%d: %s: expected %d, got %d\n", \
                __FILE__, __LINE__, __func__, (int)(b), (int)(a)); \
            test_failures++; \
        } \
    } while (0)

#define TEST_SUMMARY() \
    do { \
        if (test_failures == 0) { \
            printf("\n=== ALL %d TESTS PASSED ===\n", test_count); \
        } else { \
            fprintf(stderr, \
                "\n=== %d of %d TESTS FAILED ===\n", \
                test_failures, test_count); \
        } \
    } while (0)

#define TEST_RESULT() (test_failures == 0 ? 0 : 1)

#endif
```

Notas sobre la implementacion:

- **`do { ... } while (0)`**: es el patron estandar para macros multi-statement en C. Sin esto, `if (cond) TEST_ASSERT(x);` se romperia porque el `if` solo capturaria la primera statement de la macro.
- **`#expr`**: stringify operator — convierte el argumento de la macro en string literal.
- **Variables `static`**: `test_failures` y `test_count` son `static` para evitar conflictos si se incluye en multiples translation units (aunque normalmente un test es un solo archivo).

```c
// test_math_v3.c — usando el micro-framework
#include "test_framework_mini.h"
#include "math.h"

void test_sum(void) {
    TEST_ASSERT_EQ(sum(2, 3), 5);
    TEST_ASSERT_EQ(sum(0, 0), 0);
    TEST_ASSERT_EQ(sum(-1, 1), 0);
    TEST_ASSERT_EQ(sum(2, 3), 999);   // Falla pero continua
    TEST_ASSERT_EQ(sum(-5, -3), -8);
}

void test_multiply(void) {
    TEST_ASSERT_EQ(multiply(3, 4), 12);
    TEST_ASSERT_EQ(multiply(0, 100), 0);
    TEST_ASSERT_EQ(multiply(-2, 3), -6);
}

void test_factorial(void) {
    TEST_ASSERT_EQ(factorial(0), 1);
    TEST_ASSERT_EQ(factorial(1), 1);
    TEST_ASSERT_EQ(factorial(5), 120);
    TEST_ASSERT_EQ(factorial(10), 3628800);
}

int main(void) {
    test_sum();
    test_multiply();
    test_factorial();

    TEST_SUMMARY();
    return TEST_RESULT();
}
```

```bash
$ gcc -Wall -Wextra -o test_math_v3 test_math_v3.c math.c
$ ./test_math_v3
FAIL: test_math_v3.c:9: test_sum: expected 999, got 5

=== 1 of 12 TESTS FAILED ===
$ echo $?
1
```

Ahora **todas** las verificaciones se ejecutan, y al final tienes un resumen completo. El exit code es 1 porque hubo al menos un fallo.

### 3.4 Patron de funciones de test con reporte

Un nivel mas arriba — cada funcion de test reporta su nombre y resultado:

```c
// test_runner_basic.h
#ifndef TEST_RUNNER_BASIC_H
#define TEST_RUNNER_BASIC_H

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef int (*test_fn)(void);

typedef struct {
    const char *name;
    test_fn fn;
} TestCase;

static int total_pass = 0;
static int total_fail = 0;

// Cada funcion de test retorna 0 si pasa, != 0 si falla
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, \
                "  FAIL: %s:%d: '%s'\n", \
                __FILE__, __LINE__, #expr); \
            return 1; \
        } \
    } while (0)

#define CHECK_EQ(got, expected) \
    do { \
        long _got = (long)(got); \
        long _exp = (long)(expected); \
        if (_got != _exp) { \
            fprintf(stderr, \
                "  FAIL: %s:%d: expected %ld, got %ld\n", \
                __FILE__, __LINE__, _exp, _got); \
            return 1; \
        } \
    } while (0)

#define CHECK_STR_EQ(got, expected) \
    do { \
        const char *_got = (got); \
        const char *_exp = (expected); \
        if (strcmp(_got, _exp) != 0) { \
            fprintf(stderr, \
                "  FAIL: %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, _exp, _got); \
            return 1; \
        } \
    } while (0)

static void run_tests(TestCase *tests, int count) {
    printf("Running %d tests:\n\n", count);

    for (int i = 0; i < count; i++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int result = tests[i].fn();

        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0
                          + (end.tv_nsec - start.tv_nsec) / 1e6;

        if (result == 0) {
            printf("  [PASS] %-40s (%.3f ms)\n", tests[i].name, elapsed_ms);
            total_pass++;
        } else {
            printf("  [FAIL] %-40s (%.3f ms)\n", tests[i].name, elapsed_ms);
            total_fail++;
        }
    }

    printf("\n─────────────────────────────────────────\n");
    printf("Results: %d passed, %d failed, %d total\n",
           total_pass, total_fail, total_pass + total_fail);

    if (total_fail > 0) {
        printf("FAILED\n");
    } else {
        printf("OK\n");
    }
}

#define RUN_TESTS(...) \
    do { \
        TestCase _tests[] = { __VA_ARGS__ }; \
        int _count = sizeof(_tests) / sizeof(_tests[0]); \
        run_tests(_tests, _count); \
        return total_fail > 0 ? 1 : 0; \
    } while (0)

#define TEST(name) { #name, name }

#endif
```

```c
// test_math_v4.c — usando el runner
#include "test_runner_basic.h"
#include "math.h"

int test_sum_basic(void) {
    CHECK_EQ(sum(2, 3), 5);
    CHECK_EQ(sum(0, 0), 0);
    CHECK_EQ(sum(-1, 1), 0);
    return 0;
}

int test_sum_negative(void) {
    CHECK_EQ(sum(-5, -3), -8);
    CHECK_EQ(sum(-100, 50), -50);
    return 0;
}

int test_sum_overflow(void) {
    // INT_MAX + 1 es UB en C — pero lo documentamos
    // Este test verifica que entendemos el comportamiento
    CHECK_EQ(sum(2147483647, 0), 2147483647);
    // sum(2147483647, 1) seria UB — NO lo testeamos
    return 0;
}

int test_multiply_basic(void) {
    CHECK_EQ(multiply(3, 4), 12);
    CHECK_EQ(multiply(0, 100), 0);
    CHECK_EQ(multiply(1, 42), 42);
    return 0;
}

int test_multiply_negative(void) {
    CHECK_EQ(multiply(-2, 3), -6);
    CHECK_EQ(multiply(-2, -3), 6);
    return 0;
}

int test_factorial_base_cases(void) {
    CHECK_EQ(factorial(0), 1);
    CHECK_EQ(factorial(1), 1);
    return 0;
}

int test_factorial_normal(void) {
    CHECK_EQ(factorial(5), 120);
    CHECK_EQ(factorial(10), 3628800);
    return 0;
}

int test_gcd_coprime(void) {
    CHECK_EQ(gcd(17, 13), 1);
    CHECK_EQ(gcd(7, 11), 1);
    return 0;
}

int test_gcd_common_factors(void) {
    CHECK_EQ(gcd(12, 8), 4);
    CHECK_EQ(gcd(100, 75), 25);
    CHECK_EQ(gcd(48, 36), 12);
    return 0;
}

int test_gcd_with_zero(void) {
    CHECK_EQ(gcd(100, 0), 100);
    CHECK_EQ(gcd(0, 100), 100);
    return 0;
}

int main(void) {
    RUN_TESTS(
        TEST(test_sum_basic),
        TEST(test_sum_negative),
        TEST(test_sum_overflow),
        TEST(test_multiply_basic),
        TEST(test_multiply_negative),
        TEST(test_factorial_base_cases),
        TEST(test_factorial_normal),
        TEST(test_gcd_coprime),
        TEST(test_gcd_common_factors),
        TEST(test_gcd_with_zero)
    );
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -o test_math_v4 test_math_v4.c math.c
$ ./test_math_v4
Running 10 tests:

  [PASS] test_sum_basic                          (0.001 ms)
  [PASS] test_sum_negative                       (0.000 ms)
  [PASS] test_sum_overflow                       (0.000 ms)
  [PASS] test_multiply_basic                     (0.000 ms)
  [PASS] test_multiply_negative                  (0.001 ms)
  [PASS] test_factorial_base_cases               (0.000 ms)
  [PASS] test_factorial_normal                    (0.000 ms)
  [PASS] test_gcd_coprime                        (0.000 ms)
  [PASS] test_gcd_common_factors                 (0.000 ms)
  [PASS] test_gcd_with_zero                      (0.000 ms)

─────────────────────────────────────────
Results: 10 passed, 0 failed, 10 total
OK
$ echo $?
0
```

Esto es esencialmente lo que hacen Unity y CMocka por dentro — pero con miles de lineas de macros adicionales para manejar tipos, formateo, aislamiento, etc.

---

## 4. Convencion de exit codes

### 4.1 Exit codes en Unix/POSIX

En Unix, cada proceso termina con un **exit code** (tambien llamado exit status o return code). Es un entero entre 0 y 255 (el kernel solo preserva los 8 bits bajos):

```
┌────────────────────────────────────────────────────────────────────────┐
│                     EXIT CODES EN TESTING                             │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Codigo    Significado              Quien lo usa                       │
│  ──────    ─────────────            ─────────────                      │
│  0         Exito / todos los        Shell, Make, CI, scripts           │
│            tests pasaron                                               │
│                                                                        │
│  1         Fallo generico /         Convencion universal               │
│            al menos un test fallo                                      │
│                                                                        │
│  2         Error de uso             Programas que parsean args          │
│            (argumentos invalidos)   (getopt, argparse)                  │
│                                                                        │
│  77        Test skipped             GNU Automake / Autotools            │
│            (no aplica en esta       (convencion, no estandar)           │
│            plataforma)                                                  │
│                                                                        │
│  99        Hard error               GNU Automake                       │
│            (entorno roto, no es     (parar toda la suite)               │
│            un test failure)                                             │
│                                                                        │
│  126       Permiso denegado         Shell (chmod -x)                    │
│  127       Comando no encontrado    Shell (PATH)                        │
│  128+N     Matado por signal N      Kernel                              │
│            134 = 128+6 = SIGABRT    (assert fallo)                     │
│            139 = 128+11 = SIGSEGV   (segfault)                         │
│            137 = 128+9 = SIGKILL    (killed, OOM killer)               │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Por que importan los exit codes para testing

Los exit codes son la **interfaz** entre tu test y el mundo exterior. Make, CMake, CI systems, shell scripts — todos leen el exit code para decidir si el test paso o fallo:

```makefile
# Makefile — Make aborta si un comando retorna != 0
test: test_math
	./test_math          # Si retorna 0: Make continua
	                     # Si retorna != 0: Make aborta con error
```

```bash
# Shell — && solo ejecuta el siguiente comando si el anterior retorno 0
./test_math && echo "Tests passed" || echo "Tests FAILED"

# CI (GitHub Actions) — un exit code != 0 marca el step como fallido
# .github/workflows/test.yml
# steps:
#   - run: ./test_math    # exit 0 = green check, exit != 0 = red X
```

```bash
# Script de test runner
#!/bin/bash
FAILURES=0
for test in test_*; do
    ./$test
    if [ $? -ne 0 ]; then
        FAILURES=$((FAILURES + 1))
    fi
done

echo "Failed: $FAILURES"
exit $FAILURES  # 0 si todos pasaron
```

### 4.3 Return vs exit vs abort vs _Exit

Hay cuatro formas de terminar un programa en C, y cada una tiene implicaciones diferentes:

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // para _exit/_Exit en algunos sistemas
#include <assert.h>

// 1. return desde main()
// - Ejecuta atexit() handlers
// - Flush de stdout/stderr (fclose de streams abiertos)
// - Destructores de variables con storage duration (C11 threads)
int main(void) {
    return 0;  // Equivalente a exit(0)
}

// 2. exit(code)
// - Igual que return desde main()
// - Se puede llamar desde CUALQUIER funcion (no solo main)
void some_function(void) {
    if (critical_error) {
        exit(1);  // Termina el programa desde aqui
    }
}

// 3. abort()
// - Genera SIGABRT (signal 6)
// - NO ejecuta atexit() handlers
// - NO flush de streams
// - Puede generar core dump (si esta habilitado: ulimit -c unlimited)
// - Es lo que assert() llama cuando falla
void die(void) {
    abort();  // Exit code = 134 (128 + 6)
}

// 4. _Exit(code) / _exit(code)
// - Terminacion INMEDIATA
// - NO ejecuta atexit() handlers
// - NO flush de streams
// - NO genera signal
// - Util en child process despues de fork()
void child_process(void) {
    _Exit(0);  // Salir sin tocar los buffers del padre
}
```

```
┌─────────────────────────────────────────────────────────────────────────┐
│               TERMINACION DE PROGRAMA EN C                             │
├──────────────┬──────────┬────────┬────────┬────────────────────────────┤
│              │ return 0 │ exit() │ abort()│ _Exit()                    │
├──────────────┼──────────┼────────┼────────┼────────────────────────────┤
│ atexit()     │   SI     │   SI   │   NO   │   NO                      │
│ flush stdio  │   SI     │   SI   │   NO   │   NO                      │
│ signal       │   NO     │   NO   │ SIGABRT│   NO                      │
│ core dump    │   NO     │   NO   │   SI*  │   NO                      │
│ exit code    │ argumento│ argum. │  134   │ argumento                  │
│ desde main() │   SI     │   SI   │   SI   │   SI                      │
│ desde func() │   NO     │   SI   │   SI   │   SI                      │
├──────────────┴──────────┴────────┴────────┴────────────────────────────┤
│ *core dump: depende de ulimit -c y configuracion del sistema          │
└───────────────────────────────────────────────────────────────────────┘
```

### 4.4 Patron recomendado para tests

```c
// test_mi_modulo.c
#include <stdio.h>
#include <stdlib.h>

// ... includes del modulo a testear ...

int main(void) {
    int failures = 0;

    // Test 1
    if (mi_funcion(42) != expected) {
        fprintf(stderr, "FAIL: mi_funcion(42) returned wrong value\n");
        failures++;
    }

    // Test 2
    // ...

    // Reporte final
    if (failures > 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return EXIT_FAILURE;  // = 1 (definido en <stdlib.h>)
    }

    printf("All tests passed\n");
    return EXIT_SUCCESS;  // = 0 (definido en <stdlib.h>)
}
```

`EXIT_SUCCESS` y `EXIT_FAILURE` estan definidos en `<stdlib.h>`:
- `EXIT_SUCCESS` = 0 (en todas las implementaciones conocidas)
- `EXIT_FAILURE` = 1 (en la mayoria; POSIX garantiza que es != 0)

Son preferibles a los literales 0 y 1 porque:
1. Expresan intencion (exito vs fallo) en lugar de un numero magico
2. Son portables — en teoria, un sistema podria usar valores diferentes (VMS usaba valores pares para exito e impares para error)

---

## 5. Organizacion de archivos de test

### 5.1 Convencion de nombrado

No hay un estandar en C, pero las convenciones mas comunes son:

```
proyecto/
├── src/
│   ├── math.c
│   ├── math.h
│   ├── string_utils.c
│   ├── string_utils.h
│   ├── hash_table.c
│   └── hash_table.h
├── tests/
│   ├── test_math.c          ← test_ + nombre del modulo
│   ├── test_string_utils.c
│   └── test_hash_table.c
├── Makefile
└── README.md
```

Convenciones comunes:

| Convencion | Ejemplo | Usado por |
|------------|---------|-----------|
| `test_modulo.c` | `test_math.c` | Linux kernel, muchos proyectos FOSS |
| `modulo_test.c` | `math_test.c` | Google (estilo Go: `_test.go`) |
| `t_modulo.c` | `t_math.c` | Algunos proyectos BSD |
| `check_modulo.c` | `check_math.c` | Proyectos que usan Check framework |

La convencion `test_modulo.c` es la mas comun en C. Usaremos esa.

### 5.2 Un main() por archivo de test vs un main() central

Hay dos enfoques y cada uno tiene sus trade-offs:

**Enfoque 1: Un main() por archivo de test** (recomendado para empezar)

```
tests/
├── test_math.c       ← tiene su propio main()
├── test_strings.c    ← tiene su propio main()
└── test_hashtable.c  ← tiene su propio main()
```

```makefile
# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src

TESTS = test_math test_strings test_hashtable

all: $(TESTS)

test_math: test_math.c ../src/math.c
	$(CC) $(CFLAGS) -o $@ $^

test_strings: test_strings.c ../src/string_utils.c
	$(CC) $(CFLAGS) -o $@ $^

test_hashtable: test_hashtable.c ../src/hash_table.c
	$(CC) $(CFLAGS) -o $@ $^

test: $(TESTS)
	@failures=0; \
	for t in $(TESTS); do \
		echo "--- Running $$t ---"; \
		./$$t; \
		if [ $$? -ne 0 ]; then \
			failures=$$((failures + 1)); \
		fi; \
	done; \
	echo ""; \
	if [ $$failures -ne 0 ]; then \
		echo "$$failures test suite(s) FAILED"; \
		exit 1; \
	else \
		echo "All test suites passed"; \
	fi

clean:
	rm -f $(TESTS)

.PHONY: all test clean
```

```bash
$ make test
--- Running test_math ---
All tests passed!
--- Running test_strings ---
All tests passed!
--- Running test_hashtable ---
FAIL: test_hashtable.c:45: lookup returned NULL for existing key

1 test suite(s) FAILED
$ echo $?
2  # make retorna 2 cuando un comando falla
```

Ventajas:
- Cada test es independiente — un crash (segfault) solo afecta a ese test
- Puedes ejecutar un solo test: `./test_math`
- Cada test se puede compilar con flags diferentes (sanitizers, optimization)
- Facil de paralelizar: `make -j4 test`

**Enfoque 2: Un main() central que llama a todos los tests**

```
tests/
├── test_main.c       ← el unico main(), importa y llama todo
├── test_math.c       ← funciones de test, sin main()
├── test_strings.c    ← funciones de test, sin main()
└── test_hashtable.c  ← funciones de test, sin main()
```

```c
// test_math.h — declaraciones de las funciones de test
#ifndef TEST_MATH_H
#define TEST_MATH_H

int test_sum_basic(void);
int test_sum_negative(void);
int test_multiply_basic(void);
int test_factorial(void);

#endif
```

```c
// test_main.c — el runner central
#include <stdio.h>
#include "test_runner_basic.h"  // el header con RUN_TESTS, TEST, CHECK, etc.
#include "test_math.h"
#include "test_strings.h"
#include "test_hashtable.h"

int main(void) {
    RUN_TESTS(
        // Math tests
        TEST(test_sum_basic),
        TEST(test_sum_negative),
        TEST(test_multiply_basic),
        TEST(test_factorial),

        // String tests
        TEST(test_strlen_basic),
        TEST(test_strcpy_safe),

        // Hashtable tests
        TEST(test_ht_insert),
        TEST(test_ht_lookup),
        TEST(test_ht_delete)
    );
}
```

Ventajas:
- Un solo ejecutable — mas facil de distribuir y ejecutar
- Reporte unificado (total de tests, total de fallos)
- Mas parecido a como funcionan los frameworks reales

Desventajas:
- Un crash (segfault, stack overflow) mata toda la suite
- Todos los tests comparten el mismo address space — un memory leak en un test puede afectar a otro
- Mas complejo de compilar (necesitas linkear todo junto)

### 5.3 Compilar tests con sanitizers

Los sanitizers de GCC/Clang son **imprescindibles** para testing en C. Detectan bugs que los tests por si solos no encuentran:

```makefile
# Makefile con targets de sanitizers
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Tests normales
test: test_math
	./test_math

# Tests con AddressSanitizer (buffer overflow, use-after-free, double-free)
test-asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
test-asan: test_math
	./test_math

# Tests con UndefinedBehaviorSanitizer (signed overflow, null deref, etc.)
test-ubsan: CFLAGS += -fsanitize=undefined
test-ubsan: test_math
	./test_math

# Tests con ambos
test-sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
test-sanitize: test_math
	./test_math

# Tests con ThreadSanitizer (data races — incompatible con ASan)
test-tsan: CC = gcc
test-tsan: CFLAGS += -fsanitize=thread
test-tsan: test_math
	./test_math

test_math: test_math.c math.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: test test-asan test-ubsan test-sanitize test-tsan
```

```bash
$ make test-sanitize
$ ./test_math
# Si hay un buffer overflow oculto que el test no verifica directamente,
# ASan lo encontrara y abortara con un reporte detallado
```

---

## 6. Ejemplo completo — Testing de una estructura de datos

Hasta ahora los ejemplos han sido funciones puras simples. Veamos algo mas realista: una pila (stack) dinamica con manejo de memoria.

### 6.1 La implementacion a testear

```c
// stack.h
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} Stack;

// Crea una pila con capacidad inicial
Stack *stack_new(size_t initial_cap);

// Libera la pila y su memoria interna
void stack_free(Stack *s);

// Agrega un elemento al tope
bool stack_push(Stack *s, int value);

// Remueve y retorna el elemento del tope
// Retorna true si habia un elemento, false si la pila estaba vacia
bool stack_pop(Stack *s, int *out);

// Mira el elemento del tope sin removerlo
bool stack_peek(const Stack *s, int *out);

// Retorna la cantidad de elementos
size_t stack_len(const Stack *s);

// Retorna true si la pila esta vacia
bool stack_is_empty(const Stack *s);

#endif
```

```c
// stack.c
#include "stack.h"
#include <stdlib.h>
#include <string.h>

Stack *stack_new(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 4;

    Stack *s = malloc(sizeof(Stack));
    if (s == NULL) return NULL;

    s->data = malloc(initial_cap * sizeof(int));
    if (s->data == NULL) {
        free(s);
        return NULL;
    }

    s->len = 0;
    s->cap = initial_cap;
    return s;
}

void stack_free(Stack *s) {
    if (s == NULL) return;
    free(s->data);
    free(s);
}

bool stack_push(Stack *s, int value) {
    if (s == NULL) return false;

    if (s->len == s->cap) {
        size_t new_cap = s->cap * 2;
        int *new_data = realloc(s->data, new_cap * sizeof(int));
        if (new_data == NULL) return false;
        s->data = new_data;
        s->cap = new_cap;
    }

    s->data[s->len] = value;
    s->len++;
    return true;
}

bool stack_pop(Stack *s, int *out) {
    if (s == NULL || s->len == 0) return false;

    s->len--;
    if (out != NULL) {
        *out = s->data[s->len];
    }
    return true;
}

bool stack_peek(const Stack *s, int *out) {
    if (s == NULL || s->len == 0) return false;
    if (out != NULL) {
        *out = s->data[s->len - 1];
    }
    return true;
}

size_t stack_len(const Stack *s) {
    if (s == NULL) return 0;
    return s->len;
}

bool stack_is_empty(const Stack *s) {
    return s == NULL || s->len == 0;
}
```

### 6.2 El archivo de tests

```c
// test_stack.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"

// ─── Micro-framework inline ──────────────────────────────────────────

static int test_pass = 0;
static int test_fail = 0;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); \
            test_fail++; \
            return; \
        } \
        test_pass++; \
    } while (0)

#define ASSERT_EQ(got, expected) \
    do { \
        long _g = (long)(got); \
        long _e = (long)(expected); \
        if (_g != _e) { \
            fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                    __FILE__, __LINE__, _e, _g); \
            test_fail++; \
            return; \
        } \
        test_pass++; \
    } while (0)

#define ASSERT_TRUE(expr)  ASSERT(expr)
#define ASSERT_FALSE(expr) ASSERT(!(expr))

#define RUN(test_func) \
    do { \
        int _before = test_fail; \
        test_func(); \
        if (test_fail == _before) { \
            printf("  [PASS] %s\n", #test_func); \
        } else { \
            printf("  [FAIL] %s\n", #test_func); \
        } \
    } while (0)

// ─── Tests ───────────────────────────────────────────────────────────

void test_stack_new_creates_empty_stack(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);
    ASSERT_EQ(stack_len(s), 0);
    ASSERT_TRUE(stack_is_empty(s));
    stack_free(s);
}

void test_stack_new_with_zero_cap(void) {
    // Capacidad 0 debe usar un default (4)
    Stack *s = stack_new(0);
    ASSERT(s != NULL);
    ASSERT_EQ(stack_len(s), 0);
    stack_free(s);
}

void test_stack_push_single(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    ASSERT_TRUE(stack_push(s, 42));
    ASSERT_EQ(stack_len(s), 1);
    ASSERT_FALSE(stack_is_empty(s));

    stack_free(s);
}

void test_stack_push_multiple(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(stack_push(s, i * 10));
    }
    ASSERT_EQ(stack_len(s), 10);

    stack_free(s);
}

void test_stack_push_triggers_realloc(void) {
    // Capacidad inicial 2, push 100 elementos — debe hacer multiples realloc
    Stack *s = stack_new(2);
    ASSERT(s != NULL);

    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(stack_push(s, i));
    }
    ASSERT_EQ(stack_len(s), 100);

    // Verificar que los datos se preservaron despues de los reallocs
    int val;
    for (int i = 99; i >= 0; i--) {
        ASSERT_TRUE(stack_pop(s, &val));
        ASSERT_EQ(val, i);
    }

    stack_free(s);
}

void test_stack_pop_empty_returns_false(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    int val = -1;
    ASSERT_FALSE(stack_pop(s, &val));
    ASSERT_EQ(val, -1);  // val no debe haber sido modificado

    stack_free(s);
}

void test_stack_pop_lifo_order(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);

    int val;
    ASSERT_TRUE(stack_pop(s, &val));
    ASSERT_EQ(val, 30);  // Last In = 30

    ASSERT_TRUE(stack_pop(s, &val));
    ASSERT_EQ(val, 20);

    ASSERT_TRUE(stack_pop(s, &val));
    ASSERT_EQ(val, 10);  // First In = 10

    ASSERT_FALSE(stack_pop(s, &val));  // Ahora esta vacia

    stack_free(s);
}

void test_stack_pop_with_null_out(void) {
    // pop con out=NULL debe funcionar (descartar el valor)
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    stack_push(s, 42);
    ASSERT_TRUE(stack_pop(s, NULL));  // descartar
    ASSERT_EQ(stack_len(s), 0);

    stack_free(s);
}

void test_stack_peek(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    stack_push(s, 10);
    stack_push(s, 20);

    int val;
    ASSERT_TRUE(stack_peek(s, &val));
    ASSERT_EQ(val, 20);           // peek retorna el tope
    ASSERT_EQ(stack_len(s), 2);   // peek NO remueve

    stack_free(s);
}

void test_stack_peek_empty_returns_false(void) {
    Stack *s = stack_new(4);
    ASSERT(s != NULL);

    int val = -1;
    ASSERT_FALSE(stack_peek(s, &val));
    ASSERT_EQ(val, -1);

    stack_free(s);
}

void test_stack_push_pop_interleaved(void) {
    Stack *s = stack_new(2);
    ASSERT(s != NULL);

    // Push 3, pop 2, push 2, pop all
    stack_push(s, 1);
    stack_push(s, 2);
    stack_push(s, 3);
    ASSERT_EQ(stack_len(s), 3);

    int val;
    stack_pop(s, &val);  // 3
    ASSERT_EQ(val, 3);
    stack_pop(s, &val);  // 2
    ASSERT_EQ(val, 2);
    ASSERT_EQ(stack_len(s), 1);

    stack_push(s, 40);
    stack_push(s, 50);
    ASSERT_EQ(stack_len(s), 3);

    stack_pop(s, &val);
    ASSERT_EQ(val, 50);
    stack_pop(s, &val);
    ASSERT_EQ(val, 40);
    stack_pop(s, &val);
    ASSERT_EQ(val, 1);

    ASSERT_TRUE(stack_is_empty(s));

    stack_free(s);
}

void test_stack_null_safety(void) {
    // Todas las funciones deben manejar NULL sin crash
    ASSERT_FALSE(stack_push(NULL, 42));
    ASSERT_FALSE(stack_pop(NULL, NULL));
    ASSERT_FALSE(stack_peek(NULL, NULL));
    ASSERT_EQ(stack_len(NULL), 0);
    ASSERT_TRUE(stack_is_empty(NULL));
    stack_free(NULL);  // No debe crashear
}

// ─── main ────────────────────────────────────────────────────────────

int main(void) {
    printf("=== Stack Tests ===\n\n");

    RUN(test_stack_new_creates_empty_stack);
    RUN(test_stack_new_with_zero_cap);
    RUN(test_stack_push_single);
    RUN(test_stack_push_multiple);
    RUN(test_stack_push_triggers_realloc);
    RUN(test_stack_pop_empty_returns_false);
    RUN(test_stack_pop_lifo_order);
    RUN(test_stack_pop_with_null_out);
    RUN(test_stack_peek);
    RUN(test_stack_peek_empty_returns_false);
    RUN(test_stack_push_pop_interleaved);
    RUN(test_stack_null_safety);

    printf("\n─────────────────────────────────────────\n");
    printf("Assertions: %d passed, %d failed\n", test_pass, test_fail);

    if (test_fail > 0) {
        printf("FAILED\n");
        return EXIT_FAILURE;
    }

    printf("OK\n");
    return EXIT_SUCCESS;
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -g -o test_stack test_stack.c stack.c
$ ./test_stack
=== Stack Tests ===

  [PASS] test_stack_new_creates_empty_stack
  [PASS] test_stack_new_with_zero_cap
  [PASS] test_stack_push_single
  [PASS] test_stack_push_multiple
  [PASS] test_stack_push_triggers_realloc
  [PASS] test_stack_pop_empty_returns_false
  [PASS] test_stack_pop_lifo_order
  [PASS] test_stack_pop_with_null_out
  [PASS] test_stack_peek
  [PASS] test_stack_peek_empty_returns_false
  [PASS] test_stack_push_pop_interleaved
  [PASS] test_stack_null_safety

─────────────────────────────────────────
Assertions: 48 passed, 0 failed
OK

# Con sanitizers
$ gcc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined -o test_stack test_stack.c stack.c
$ ./test_stack
# Misma salida — ASan no encontro problemas de memoria
```

### 6.3 Que cubre este test y que NO cubre

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        COVERAGE MENTAL                                       │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ✓ Cubierto por los tests:                                                  │
│  ─────────────────────────                                                   │
│  • Creacion con capacidad normal y con 0                                    │
│  • Push simple y multiples (mas alla de la capacidad)                       │
│  • Pop en orden LIFO                                                        │
│  • Pop en pila vacia                                                        │
│  • Pop con out=NULL (descartar valor)                                       │
│  • Peek sin remover                                                         │
│  • Peek en pila vacia                                                       │
│  • Operaciones entrelazadas push/pop                                        │
│  • Null safety en todas las funciones                                       │
│  • Realloc por crecimiento (cap 2 → 4 → 8 → ... → 128)                    │
│                                                                              │
│  ✗ NO cubierto (necesitaria mas infraestructura):                           │
│  ──────────────────────────────────────────────────                          │
│  • Fallo de malloc/realloc (necesita mocking de malloc)                     │
│  • Comportamiento con SIZE_MAX elementos (impractico)                       │
│  • Thread safety (no es thread-safe por diseno)                             │
│  • Rendimiento (eso es benchmarking, otro topico)                           │
│  • Memory leaks (lo detecta Valgrind/ASan, no el test)                     │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Patrones avanzados de testing manual

### 7.1 Test con datos de archivo

A veces necesitas probar con inputs grandes o datos reales. Un patron comun es leer test data de archivos:

```c
// test_parser.c — test con datos de archivo
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// Helper: leer archivo completo a un buffer
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, len, f);
    buf[read] = '\0';
    fclose(f);

    if (out_len) *out_len = read;
    return buf;
}

int test_parse_valid_json(void) {
    char *input = read_file("testdata/valid.json", NULL);
    if (input == NULL) {
        fprintf(stderr, "  SKIP: testdata/valid.json not found\n");
        return 77;  // Convencion Automake: 77 = skip
    }

    ParseResult result = parse(input);
    free(input);

    if (result.error != NULL) {
        fprintf(stderr, "  FAIL: parse error: %s\n", result.error);
        parse_result_free(&result);
        return 1;
    }

    if (result.node_count != 42) {
        fprintf(stderr, "  FAIL: expected 42 nodes, got %zu\n", result.node_count);
        parse_result_free(&result);
        return 1;
    }

    parse_result_free(&result);
    return 0;
}

int test_parse_invalid_json(void) {
    char *input = read_file("testdata/invalid.json", NULL);
    if (input == NULL) {
        fprintf(stderr, "  SKIP: testdata/invalid.json not found\n");
        return 77;
    }

    ParseResult result = parse(input);
    free(input);

    // Un input invalido DEBE producir un error
    if (result.error == NULL) {
        fprintf(stderr, "  FAIL: expected parse error for invalid input\n");
        parse_result_free(&result);
        return 1;
    }

    parse_result_free(&result);
    return 0;
}
```

Estructura de directorios:

```
tests/
├── test_parser.c
└── testdata/
    ├── valid.json
    ├── invalid.json
    ├── empty.json
    ├── large.json        ← archivo grande para stress testing
    └── unicode.json      ← caracteres multibyte
```

### 7.2 Test con captura de stdout/stderr

Para testear codigo que imprime a stdout/stderr, puedes redirigir los file descriptors:

```c
// test_logger.c — capturar output
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logger.h"

// Capturar stdout en un buffer
typedef struct {
    char *buf;
    size_t len;
} CapturedOutput;

static CapturedOutput capture_stdout(void (*fn)(void)) {
    // Crear un pipe
    int pipefd[2];
    pipe(pipefd);

    // Guardar stdout original
    int saved_stdout = dup(STDOUT_FILENO);

    // Redirigir stdout al pipe
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    // Ejecutar la funcion
    fn();
    fflush(stdout);

    // Restaurar stdout
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    // Leer del pipe
    char buf[4096];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);

    CapturedOutput out = {0};
    if (n > 0) {
        buf[n] = '\0';
        out.buf = strdup(buf);
        out.len = n;
    }
    return out;
}

// Ejemplo de test
static void call_log_info(void) {
    log_info("hello %s", "world");
}

int test_log_info_format(void) {
    CapturedOutput out = capture_stdout(call_log_info);

    if (out.buf == NULL) {
        fprintf(stderr, "  FAIL: no output captured\n");
        return 1;
    }

    // Verificar que el output contiene lo esperado
    if (strstr(out.buf, "[INFO]") == NULL) {
        fprintf(stderr, "  FAIL: missing [INFO] prefix: '%s'\n", out.buf);
        free(out.buf);
        return 1;
    }

    if (strstr(out.buf, "hello world") == NULL) {
        fprintf(stderr, "  FAIL: missing message: '%s'\n", out.buf);
        free(out.buf);
        return 1;
    }

    free(out.buf);
    return 0;
}
```

### 7.3 Test con fork() para aislar crashes

Cuando necesitas verificar que cierto input causa un crash (segfault, abort), puedes usar `fork()` para ejecutar el codigo peligroso en un proceso hijo:

```c
// test_crash_safety.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parser.h"

// Ejecuta fn en un proceso hijo y retorna el exit status
static int run_in_child(void (*fn)(void)) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Proceso hijo — ejecutar la funcion
        fn();
        _Exit(EXIT_SUCCESS);  // Si llega aqui, no crasheo
    }

    // Proceso padre — esperar al hijo
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);  // Exit code del hijo
    }
    if (WIFSIGNALED(status)) {
        return -WTERMSIG(status);    // Negativo = signal number
    }
    return -1;
}

// Test: parse(NULL) debe causar SIGSEGV o ser manejado gracefully
static void call_parse_null(void) {
    parse(NULL);  // Puede crashear — esta en un hijo, no importa
}

int test_parse_null_does_not_crash(void) {
    int result = run_in_child(call_parse_null);

    if (result < 0) {
        fprintf(stderr, "  FAIL: parse(NULL) crashed with signal %d\n", -result);
        return 1;
    }

    return 0;
}

// Test: verificar que un assert falla (exit via SIGABRT)
static void call_with_invalid_state(void) {
    Parser p = {0};
    p.state = -1;  // Estado invalido — deberia triggerar assert
    parser_step(&p);
}

int test_invalid_state_triggers_abort(void) {
    int result = run_in_child(call_with_invalid_state);

    // Esperamos SIGABRT (signal 6) → result = -6
    if (result != -6) {
        fprintf(stderr, "  FAIL: expected SIGABRT, got %d\n", result);
        return 1;
    }

    printf("  (correctly aborted with SIGABRT)\n");
    return 0;
}
```

Este patron es extremadamente util porque:
1. El crash en el hijo **no mata** al padre (el test runner continua)
2. Puedes verificar **que tipo de signal** causo el crash
3. Puedes testear que tu codigo **aborta correctamente** cuando recibe input invalido (verificar los asserts de produccion)

---

## 8. Integracion con Make

### 8.1 Makefile completo para testing

```makefile
# Makefile — proyecto con tests
CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -g
SRCDIR   = src
TESTDIR  = tests
BUILDDIR = build

# Archivos fuente (excluir main.c del proyecto para tests)
SRCS     = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
OBJS     = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))

# Test sources — cada uno produce un ejecutable
TEST_SRCS = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/%.c, $(BUILDDIR)/%, $(TEST_SRCS))

# ─── Targets principales ─────────────────────────────────────────────

.PHONY: all test test-verbose test-sanitize clean

all: $(BUILDDIR)/program

# Compilar el programa principal
$(BUILDDIR)/program: $(SRCDIR)/main.c $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compilar objetos
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

# Compilar tests — cada test linkea con los objetos necesarios
$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ─── Test targets ────────────────────────────────────────────────────

# Ejecutar todos los tests (output minimo)
test: $(TEST_BINS)
	@echo "Running $(words $(TEST_BINS)) test suite(s)..."
	@echo ""
	@pass=0; fail=0; skip=0; \
	for t in $(TEST_BINS); do \
		name=$$(basename $$t); \
		$$t > /dev/null 2>&1; \
		rc=$$?; \
		if [ $$rc -eq 0 ]; then \
			echo "  [PASS] $$name"; \
			pass=$$((pass + 1)); \
		elif [ $$rc -eq 77 ]; then \
			echo "  [SKIP] $$name"; \
			skip=$$((skip + 1)); \
		else \
			echo "  [FAIL] $$name (exit code $$rc)"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Results: $$pass passed, $$fail failed, $$skip skipped"; \
	if [ $$fail -ne 0 ]; then exit 1; fi

# Ejecutar con output completo (verbose)
test-verbose: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "═══ $$(basename $$t) ═══"; \
		$$t; \
		echo ""; \
	done

# Ejecutar con sanitizers
test-sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
test-sanitize: clean $(TEST_BINS)
	@echo "Running tests with ASan + UBSan..."
	@for t in $(TEST_BINS); do \
		echo "--- $$(basename $$t) ---"; \
		$$t || exit 1; \
	done

# Ejecutar con Valgrind (alternativa a sanitizers)
test-valgrind: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "--- $$(basename $$t) ---"; \
		valgrind --leak-check=full --error-exitcode=1 $$t || exit 1; \
	done

clean:
	rm -rf $(BUILDDIR)
```

```bash
# Uso tipico
$ make test
Running 3 test suite(s)...

  [PASS] test_math
  [PASS] test_stack
  [FAIL] test_parser (exit code 1)

Results: 2 passed, 1 failed, 0 skipped
make: *** [Makefile:42: test] Error 1

# Verbose para ver detalles del fallo
$ make test-verbose
═══ test_math ═══
All tests passed!

═══ test_stack ═══
  [PASS] test_stack_new_creates_empty_stack
  [PASS] test_stack_push_single
  ...
OK

═══ test_parser ═══
  [PASS] test_parse_valid
  [FAIL] test_parse_empty: expected error, got success
FAILED

# Con sanitizers
$ make test-sanitize
# ... si hay un buffer overflow oculto, ASan lo reporta aqui
```

### 8.2 Integracion con CMake (CTest)

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(mi_proyecto C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libreria con el codigo a testear
add_library(mylib
    src/math.c
    src/stack.c
    src/parser.c
)
target_include_directories(mylib PUBLIC src)

# Ejecutable principal
add_executable(program src/main.c)
target_link_libraries(program mylib)

# ─── Tests ──────────────────────────────────────────────────────
enable_testing()

# Funcion helper para agregar tests
function(add_unit_test test_name)
    add_executable(${test_name} tests/${test_name}.c)
    target_link_libraries(${test_name} mylib)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_unit_test(test_math)
add_unit_test(test_stack)
add_unit_test(test_parser)
```

```bash
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
$ ctest --output-on-failure
Test project /home/user/project/build
    Start 1: test_math
1/3 Test #1: test_math ................   Passed    0.01 sec
    Start 2: test_stack
2/3 Test #2: test_stack ...............   Passed    0.01 sec
    Start 3: test_parser
3/3 Test #3: test_parser ..............***Failed    0.01 sec
  FAIL [test_parser.c:28] expected error, got success

67% tests passed, 1 tests failed out of 3
```

CTest ya te da:
- Autodiscovery de tests (los registrados con `add_test`)
- Output filtrado (solo muestra detalles de tests que fallan)
- Paralelismo: `ctest -j4`
- Timeout: `set_tests_properties(test_name PROPERTIES TIMEOUT 10)`
- Labels: agrupar tests por categoria

---

## 9. Comparacion con Rust y Go

### 9.1 El mismo test en tres lenguajes

**C — manual, sin framework:**

```c
// test_sum.c
#include <stdio.h>
#include <stdlib.h>

int sum(int a, int b) { return a + b; }

int main(void) {
    int failures = 0;

    if (sum(2, 3) != 5) {
        fprintf(stderr, "FAIL: sum(2,3) != 5\n");
        failures++;
    }
    if (sum(0, 0) != 0) {
        fprintf(stderr, "FAIL: sum(0,0) != 0\n");
        failures++;
    }
    if (sum(-1, 1) != 0) {
        fprintf(stderr, "FAIL: sum(-1,1) != 0\n");
        failures++;
    }

    if (failures > 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("All tests passed\n");
    return EXIT_SUCCESS;
}
```

```bash
gcc -o test_sum test_sum.c && ./test_sum
```

**Rust — built-in:**

```rust
fn sum(a: i32, b: i32) -> i32 { a + b }

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sum_basic() {
        assert_eq!(sum(2, 3), 5);
    }

    #[test]
    fn test_sum_zero() {
        assert_eq!(sum(0, 0), 0);
    }

    #[test]
    fn test_sum_negative() {
        assert_eq!(sum(-1, 1), 0);
    }
}
```

```bash
cargo test
```

**Go — built-in:**

```go
package math

import "testing"

func Sum(a, b int) int { return a + b }

func TestSumBasic(t *testing.T) {
    if got := Sum(2, 3); got != 5 {
        t.Errorf("Sum(2,3) = %d, want 5", got)
    }
}

func TestSumZero(t *testing.T) {
    if got := Sum(0, 0); got != 0 {
        t.Errorf("Sum(0,0) = %d, want 0", got)
    }
}

func TestSumNegative(t *testing.T) {
    if got := Sum(-1, 1); got != 0 {
        t.Errorf("Sum(-1,1) = %d, want 0", got)
    }
}
```

```bash
go test
```

### 9.2 Tabla comparativa

| Aspecto | C (sin framework) | Rust | Go |
|---------|-------------------|------|-----|
| **Donde viven los tests** | Archivos separados (`test_*.c`) | Mismo archivo (`#[cfg(test)] mod tests`) o `tests/` | Archivos separados (`*_test.go`, mismo paquete) |
| **Como se ejecutan** | Compilar + ejecutar manualmente | `cargo test` | `go test` |
| **Autodiscovery** | No — tienes que listar cada test | Si — `#[test]` | Si — `Test*` |
| **Aislamiento** | Un fallo mata el proceso | Cada test en su propio thread, panic atrapado | Cada test en su propia goroutine |
| **Assertions** | `assert()` (hard) o macros custom (soft) | `assert!`, `assert_eq!`, `assert_ne!` | `t.Error()`, `t.Fatal()`, `t.Errorf()` |
| **Setup/Teardown** | Manual (funciones llamadas explicitamente) | No built-in (closures, Drop trait) | `t.Cleanup()`, `TestMain` |
| **Subtests** | No built-in (funciones separadas) | Closures dentro de `#[test]` | `t.Run("name", func)` |
| **Test paralelo** | Un proceso por test (Make -j) | Por defecto (threads) | `t.Parallel()` |
| **Cobertura** | `gcov`/`lcov` (separado) | `cargo tarpaulin` / `llvm-cov` | `go test -cover` |
| **Benchmarks** | Manual (`clock_gettime`) | `#[bench]` / Criterion | `testing.B` |
| **Fuzzing** | AFL++, libFuzzer (externo) | `cargo fuzz` | `testing.F` (Go 1.18+) |
| **Memory safety** | ASan/Valgrind (separado) | El compilador (borrow checker) | GC (no aplica) |

### 9.3 Lo que C te obliga a aprender

La ausencia de testing built-in en C tiene un efecto pedagogico profundo. Al escribir tests manuales en C, aprendes:

1. **Que es realmente un test**: un programa que ejecuta codigo y verifica resultados. No hay magia — es codigo que prueba otro codigo.

2. **Exit codes**: la interfaz fundamental entre procesos en Unix. Cada programa que ejecutas retorna un numero, y ese numero tiene significado.

3. **Process model**: un test es un proceso. Si crashea, el kernel lo mata y reporta el signal. Esto es lo mismo que pasa cuando tu servidor web recibe un SIGSEGV.

4. **Build systems**: compilar tests requiere entender como linkear, que archivos incluir, como organizar targets. Esto es conocimiento transferible a cualquier proyecto C.

5. **Macros de C**: para construir assertions decentes necesitas dominar `#define`, stringify (`#`), `__FILE__`, `__LINE__`, `do { ... } while (0)`, y las trampas de la evaluacion multiple.

Cuando llegas a frameworks como Unity o CMocka, ya entiendes todo lo que hacen por dentro — porque lo construiste tu.

---

## 10. Errores comunes y antipatrones

### 10.1 Testear con printf + verificacion visual

```c
// ANTIPATRON — depende de que un humano lea la salida
int main(void) {
    printf("sum(2,3) = %d\n", sum(2, 3));
    printf("sum(0,0) = %d\n", sum(0, 0));
    printf("factorial(5) = %d\n", factorial(5));
    return 0;
    // ¿Paso? ¿Fallo? Nadie sabe — tienes que leer cada linea
    // y comparar mentalmente con el valor esperado.
    // Esto no es un test. Es un programa de diagnostico.
}
```

```c
// CORRECTO — el programa verifica automaticamente
int main(void) {
    assert(sum(2, 3) == 5);
    assert(sum(0, 0) == 0);
    assert(factorial(5) == 120);
    printf("All tests passed\n");
    return 0;
    // Si el programa imprime "All tests passed" Y retorna 0,
    // sabemos que todo esta bien. Sin intervencion humana.
}
```

La diferencia es **automatizabilidad**. Un test con printf requiere un humano. Un test con assert puede ejecutarse en CI, en Make, en un script — sin que nadie lo lea.

### 10.2 Usar assert en el codigo de produccion como error handling

```c
// INCORRECTO — assert no es para validar input del usuario
int parse_port(const char *str) {
    assert(str != NULL);          // ← con NDEBUG, desaparece
    int port = atoi(str);
    assert(port > 0 && port < 65536);  // ← con NDEBUG, desaparece
    return port;
}
// En produccion con NDEBUG:
//   parse_port(NULL) → segfault
//   parse_port("99999") → retorna 99999, puerto invalido
```

```c
// CORRECTO — validar input explicitamente
int parse_port(const char *str, int *out_port) {
    if (str == NULL) return -1;

    char *end;
    long port = strtol(str, &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        return -1;
    }

    *out_port = (int)port;
    return 0;
}
// Funciona igual con o sin NDEBUG — siempre valida
```

### 10.3 No testear edge cases

```c
// INSUFICIENTE — solo el happy path
void test_stack(void) {
    Stack *s = stack_new(10);
    stack_push(s, 42);
    int val;
    stack_pop(s, &val);
    assert(val == 42);
    stack_free(s);
    // ¿Que pasa con stack vacia? ¿Con NULL? ¿Con capacidad 0?
    // ¿Con 10 millones de elementos? ¿Con push/pop alternados?
}
```

Checklist de edge cases para testing en C:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                    CHECKLIST DE EDGE CASES EN C                             │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Punteros:                                                                  │
│  □ NULL como argumento                                                      │
│  □ Puntero a buffer vacio (longitud 0)                                     │
│  □ Buffer de exactamente 1 byte                                            │
│                                                                              │
│  Enteros:                                                                   │
│  □ 0, 1, -1                                                                │
│  □ INT_MAX, INT_MIN                                                        │
│  □ Overflow (INT_MAX + 1 es UB — documentar, no testear)                   │
│                                                                              │
│  Strings:                                                                   │
│  □ String vacia ("")                                                        │
│  □ String de un caracter ("x")                                             │
│  □ String sin null terminator (¡cuidado! es UB leer mas alla)              │
│  □ String con caracteres especiales (\n, \0 embedded, UTF-8)               │
│                                                                              │
│  Arrays/buffers:                                                            │
│  □ Vacio (longitud 0)                                                      │
│  □ Un elemento                                                             │
│  □ Exactamente lleno (longitud == capacidad)                               │
│  □ Mas alla de la capacidad (debe triggerar realloc o error)               │
│                                                                              │
│  Memoria:                                                                   │
│  □ malloc retornando NULL (si puedes mockear)                              │
│  □ Double free (debe ser imposible por diseno, verificar con ASan)         │
│  □ Use after free (verificar con ASan)                                     │
│                                                                              │
│  Archivos:                                                                  │
│  □ Archivo no existe                                                        │
│  □ Archivo vacio                                                            │
│  □ Archivo sin permisos de lectura                                         │
│  □ Path demasiado largo (PATH_MAX)                                         │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 10.4 No usar flags de compilacion estrictos

```makefile
# MINIMO — no detecta nada
CFLAGS = -o test

# RECOMENDADO — para desarrollo y tests
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g

# PARANOICO — para CI y tests exhaustivos
CFLAGS = -Wall -Wextra -Wpedantic -Werror -std=c11 -g \
         -Wshadow -Wconversion -Wdouble-promotion \
         -Wformat=2 -Wnull-dereference -Wuninitialized \
         -fstack-protector-strong
```

| Flag | Que detecta |
|------|-------------|
| `-Wall` | La mayoria de warnings utiles (variables no usadas, comparaciones sospechosas, etc.) |
| `-Wextra` | Warnings adicionales (parametros no usados, comparacion signed/unsigned, etc.) |
| `-Wpedantic` | Rechazar extensiones de GNU — solo C estandar |
| `-Werror` | Tratar warnings como errores — nada pasa sin ser revisado |
| `-Wshadow` | Variable local que oculta (shadows) otra variable |
| `-Wconversion` | Conversiones implicitas que pueden perder datos (int → char) |
| `-std=c11` | Usar C11 (static_assert, aligned_alloc, _Generic) |
| `-g` | Incluir info de debug (gdb, Valgrind, ASan pueden dar lineas de codigo) |

---

## 11. Programa de practica

### Objetivo

Implementar y testear un **buffer circular** (ring buffer) — una estructura de datos usada en networking, audio, logs, y comunicacion entre productor/consumidor.

### Especificacion

```c
// ring_buffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t cap;      // capacidad total
    size_t head;     // indice de lectura
    size_t tail;     // indice de escritura
    size_t len;      // elementos actuales
} RingBuffer;

// Crea un ring buffer con capacidad dada (debe ser > 0)
RingBuffer *rb_new(size_t capacity);

// Libera el ring buffer
void rb_free(RingBuffer *rb);

// Escribe un byte. Retorna false si esta lleno.
bool rb_write(RingBuffer *rb, uint8_t byte);

// Lee un byte. Retorna false si esta vacio.
bool rb_read(RingBuffer *rb, uint8_t *out);

// Escribe multiples bytes. Retorna la cantidad realmente escrita.
size_t rb_write_bulk(RingBuffer *rb, const uint8_t *data, size_t len);

// Lee multiples bytes. Retorna la cantidad realmente leida.
size_t rb_read_bulk(RingBuffer *rb, uint8_t *out, size_t len);

// Consultas
size_t rb_len(const RingBuffer *rb);
size_t rb_available(const RingBuffer *rb);  // espacio libre
bool rb_is_empty(const RingBuffer *rb);
bool rb_is_full(const RingBuffer *rb);

// Reset — vacia el buffer sin liberar memoria
void rb_reset(RingBuffer *rb);

#endif
```

### Que testear

1. **Creacion**: `rb_new(0)` debe retornar NULL o usar default; `rb_new(N)` debe crear buffer vacio con cap=N
2. **Write/Read basico**: escribir un byte, leerlo, verificar que es el mismo
3. **FIFO order**: escribir [1,2,3], leer → debe ser 1, luego 2, luego 3
4. **Buffer lleno**: escribir mas bytes de los que caben → `rb_write` retorna false, los datos existentes no se corrompen
5. **Buffer vacio**: leer de buffer vacio → `rb_read` retorna false
6. **Wrap-around**: escribir hasta llenar, leer la mitad, escribir mas — el tail debe "dar la vuelta" al inicio del array
7. **Bulk operations**: `rb_write_bulk` y `rb_read_bulk` con cantidades parciales (pedir mas de lo que cabe/hay)
8. **Reset**: llenar, resetear, verificar que esta vacio y se puede volver a usar
9. **Null safety**: todas las funciones con NULL
10. **Edge case**: buffer de capacidad 1

### Patron de test sugerido

```c
// test_ring_buffer.c — esqueleto
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ring_buffer.h"

static int pass_count = 0;
static int fail_count = 0;

#define ASSERT(expr) /* ... como en los ejemplos anteriores ... */
#define ASSERT_EQ(a, b) /* ... */
#define ASSERT_TRUE(expr) /* ... */
#define ASSERT_FALSE(expr) /* ... */
#define RUN(fn) /* ... */

void test_rb_create(void) {
    RingBuffer *rb = rb_new(8);
    ASSERT(rb != NULL);
    ASSERT_EQ(rb_len(rb), 0);
    ASSERT_EQ(rb_available(rb), 8);
    ASSERT_TRUE(rb_is_empty(rb));
    ASSERT_FALSE(rb_is_full(rb));
    rb_free(rb);
}

void test_rb_write_read_single(void) {
    RingBuffer *rb = rb_new(4);
    ASSERT(rb != NULL);

    ASSERT_TRUE(rb_write(rb, 0xAB));
    ASSERT_EQ(rb_len(rb), 1);

    uint8_t val;
    ASSERT_TRUE(rb_read(rb, &val));
    ASSERT_EQ(val, 0xAB);
    ASSERT_TRUE(rb_is_empty(rb));

    rb_free(rb);
}

void test_rb_fifo_order(void) {
    RingBuffer *rb = rb_new(8);
    ASSERT(rb != NULL);

    rb_write(rb, 10);
    rb_write(rb, 20);
    rb_write(rb, 30);

    uint8_t val;
    rb_read(rb, &val); ASSERT_EQ(val, 10);
    rb_read(rb, &val); ASSERT_EQ(val, 20);
    rb_read(rb, &val); ASSERT_EQ(val, 30);

    rb_free(rb);
}

void test_rb_full(void) {
    RingBuffer *rb = rb_new(3);
    ASSERT(rb != NULL);

    ASSERT_TRUE(rb_write(rb, 1));
    ASSERT_TRUE(rb_write(rb, 2));
    ASSERT_TRUE(rb_write(rb, 3));
    ASSERT_TRUE(rb_is_full(rb));
    ASSERT_FALSE(rb_write(rb, 4));  // Rechazado — lleno

    // Verificar que los datos no se corrompieron
    uint8_t val;
    rb_read(rb, &val); ASSERT_EQ(val, 1);
    rb_read(rb, &val); ASSERT_EQ(val, 2);
    rb_read(rb, &val); ASSERT_EQ(val, 3);

    rb_free(rb);
}

void test_rb_wraparound(void) {
    RingBuffer *rb = rb_new(4);
    ASSERT(rb != NULL);

    // Llenar completamente
    rb_write(rb, 'A');
    rb_write(rb, 'B');
    rb_write(rb, 'C');
    rb_write(rb, 'D');

    // Leer 2 — libera espacio al inicio
    uint8_t val;
    rb_read(rb, &val); ASSERT_EQ(val, 'A');
    rb_read(rb, &val); ASSERT_EQ(val, 'B');

    // Escribir 2 mas — tail wraps around
    ASSERT_TRUE(rb_write(rb, 'E'));
    ASSERT_TRUE(rb_write(rb, 'F'));
    ASSERT_TRUE(rb_is_full(rb));

    // Verificar orden FIFO completo
    rb_read(rb, &val); ASSERT_EQ(val, 'C');
    rb_read(rb, &val); ASSERT_EQ(val, 'D');
    rb_read(rb, &val); ASSERT_EQ(val, 'E');
    rb_read(rb, &val); ASSERT_EQ(val, 'F');
    ASSERT_TRUE(rb_is_empty(rb));

    rb_free(rb);
}

void test_rb_bulk_operations(void) {
    RingBuffer *rb = rb_new(8);
    ASSERT(rb != NULL);

    uint8_t data[] = {1, 2, 3, 4, 5};
    size_t written = rb_write_bulk(rb, data, 5);
    ASSERT_EQ(written, 5);
    ASSERT_EQ(rb_len(rb), 5);

    uint8_t out[8] = {0};
    size_t read_count = rb_read_bulk(rb, out, 8);  // Pedir 8, solo hay 5
    ASSERT_EQ(read_count, 5);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(out[i], data[i]);
    }

    rb_free(rb);
}

void test_rb_capacity_one(void) {
    RingBuffer *rb = rb_new(1);
    ASSERT(rb != NULL);

    ASSERT_TRUE(rb_write(rb, 99));
    ASSERT_TRUE(rb_is_full(rb));
    ASSERT_FALSE(rb_write(rb, 100));

    uint8_t val;
    ASSERT_TRUE(rb_read(rb, &val));
    ASSERT_EQ(val, 99);
    ASSERT_TRUE(rb_is_empty(rb));

    rb_free(rb);
}

void test_rb_null_safety(void) {
    ASSERT_FALSE(rb_write(NULL, 42));
    ASSERT_FALSE(rb_read(NULL, NULL));
    ASSERT_EQ(rb_len(NULL), 0);
    ASSERT_EQ(rb_available(NULL), 0);
    ASSERT_TRUE(rb_is_empty(NULL));
    ASSERT_TRUE(rb_is_full(NULL));  // o FALSE — depende de tu convencion
    rb_free(NULL);
    rb_reset(NULL);
}

void test_rb_reset(void) {
    RingBuffer *rb = rb_new(4);
    ASSERT(rb != NULL);

    rb_write(rb, 1);
    rb_write(rb, 2);
    rb_write(rb, 3);
    ASSERT_EQ(rb_len(rb), 3);

    rb_reset(rb);
    ASSERT_TRUE(rb_is_empty(rb));
    ASSERT_EQ(rb_len(rb), 0);
    ASSERT_EQ(rb_available(rb), 4);

    // Debe poder reutilizarse despues de reset
    ASSERT_TRUE(rb_write(rb, 10));
    uint8_t val;
    ASSERT_TRUE(rb_read(rb, &val));
    ASSERT_EQ(val, 10);

    rb_free(rb);
}

int main(void) {
    printf("=== Ring Buffer Tests ===\n\n");

    RUN(test_rb_create);
    RUN(test_rb_write_read_single);
    RUN(test_rb_fifo_order);
    RUN(test_rb_full);
    RUN(test_rb_wraparound);
    RUN(test_rb_bulk_operations);
    RUN(test_rb_capacity_one);
    RUN(test_rb_null_safety);
    RUN(test_rb_reset);

    printf("\n─────────────────────────────────────────\n");
    printf("Assertions: %d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

Compilar y ejecutar:

```bash
$ gcc -Wall -Wextra -std=c11 -g -o test_rb test_ring_buffer.c ring_buffer.c
$ ./test_rb

# Con sanitizers (imprescindible — el ring buffer usa aritmetica de indices)
$ gcc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined \
      -o test_rb test_ring_buffer.c ring_buffer.c
$ ./test_rb

# Con Valgrind
$ valgrind --leak-check=full ./test_rb
```

---

## 12. Ejercicios

### Ejercicio 1: Implementar el ring buffer

Implementa `ring_buffer.c` segun la especificacion de `ring_buffer.h` de la seccion 11. El wrap-around se implementa con operacion modulo: `index = (index + 1) % cap`.

Verificaciones clave:
- Los tests de la seccion 11 deben pasar todos
- Ejecutar con `-fsanitize=address,undefined` no debe reportar errores
- `valgrind --leak-check=full` no debe reportar leaks

### Ejercicio 2: Agregar un micro-framework

Crea tu propio header `test_helpers.h` con al menos estas macros:
- `ASSERT(expr)` — falla con archivo, linea, y expresion
- `ASSERT_EQ(got, expected)` — falla mostrando ambos valores (formateados como `long`)
- `ASSERT_STR_EQ(got, expected)` — falla mostrando ambos strings
- `ASSERT_MEM_EQ(got, expected, len)` — compara bloques de memoria con `memcmp`
- `RUN(fn)` — ejecuta una funcion de test y reporta PASS/FAIL
- Contadores globales de pass/fail
- Macro o funcion `SUMMARY()` que imprime el resumen final

### Ejercicio 3: Testear una funcion de string

Escribe una funcion `char *str_trim(const char *s)` que retorna una copia del string sin espacios al inicio ni al final (la copia es allocada con `malloc`, el caller debe hacer `free`). Luego escribe tests para:

- String normal con espacios: `"  hello world  "` → `"hello world"`
- Solo espacios: `"    "` → `""`
- String vacia: `""` → `""`
- Sin espacios: `"hello"` → `"hello"`
- Tabs y newlines: `"\t\nhello\t\n"` → `"hello"`
- NULL input: `str_trim(NULL)` → `NULL`
- Un solo caracter: `" x "` → `"x"`

### Ejercicio 4: Test con fork para verificar asserts

Escribe un test que verifique que tu implementacion de ring buffer aborta (SIGABRT via assert) cuando se intenta crear con capacidad 0 (si decidiste usar assert para eso en lugar de retornar NULL). Usa el patron de `fork()` + `waitpid()` de la seccion 7.3 para ejecutar la operacion en un proceso hijo y verificar el signal de terminacion.

---

## 13. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                  TEST SIN FRAMEWORK EN C — MAPA MENTAL                        │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ¿Por que no tiene testing?                                                   │
│  └─ Filosofia: minimalismo, el programador decide, la stdlib es minima        │
│                                                                                │
│  ¿Que te da C?                                                                │
│  ├─ assert.h → assert() (runtime, eliminable con NDEBUG)                     │
│  ├─ assert.h → static_assert (C11, compile-time, siempre activo)             │
│  └─ Exit codes → 0=exito, 1=fallo, 128+N=signal                             │
│                                                                                │
│  ¿Como se estructura un test?                                                 │
│  ├─ Un main() que ejecuta codigo y verifica resultados                        │
│  ├─ Exit code 0 si pasa, != 0 si falla                                       │
│  ├─ Soft-asserts (macros custom) para no abortar al primer fallo             │
│  └─ Un archivo test_*.c por modulo, compilado como ejecutable independiente  │
│                                                                                │
│  ¿Como se integra con el build?                                              │
│  ├─ Make: target "test" que compila y ejecuta todos los test_*               │
│  ├─ CMake: CTest con add_test()                                              │
│  └─ Sanitizers: -fsanitize=address,undefined para bugs de memoria            │
│                                                                                │
│  Patrones avanzados:                                                          │
│  ├─ Test data de archivo (testdata/)                                         │
│  ├─ Captura de stdout/stderr (dup/dup2/pipe)                                 │
│  └─ fork() para aislar crashes (SIGSEGV, SIGABRT)                            │
│                                                                                │
│  Que NO hacer:                                                                │
│  ├─ printf sin verificacion automatica                                       │
│  ├─ assert() para validar input de usuario                                   │
│  ├─ Efectos secundarios dentro de assert()                                   │
│  └─ Compilar tests sin -Wall -Wextra -g                                      │
│                                                                                │
│  Proximo topico: T02 — Arrange-Act-Assert, nombrado, un test = una cosa      │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: (Inicio del capitulo) | Siguiente: T02 — Estructura de un test →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 1 — Fundamentos > Topico 1 — Test sin framework*
