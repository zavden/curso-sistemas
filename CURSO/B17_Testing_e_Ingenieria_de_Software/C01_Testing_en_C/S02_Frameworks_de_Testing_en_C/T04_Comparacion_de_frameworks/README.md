# Comparacion de frameworks — Unity vs CMocka vs Check vs MinUnit, criterios de eleccion

## 1. Introduccion

C no tiene un framework de testing estandar. A lo largo de los anos han surgido docenas de opciones, cada una con filosofias diferentes. Este topico compara las cuatro opciones mas relevantes: **Unity**, **CMocka**, **Check** y **MinUnit**, y establece criterios objetivos para elegir entre ellas.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                   PANORAMA DE FRAMEWORKS DE TESTING EN C                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐                            │
│  │ MinUnit  │  │  Unity  │  │  Check  │  │ CMocka  │                            │
│  │ ~3 LOC  │  │ ~4KLOC  │  │ ~15KLOC │  │ ~8KLOC  │                            │
│  │ header  │  │ source  │  │ library │  │ library │                            │
│  │ only    │  │ files   │  │ .so/.a  │  │ .so/.a  │                            │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘                            │
│       │            │            │            │                                   │
│       ▼            ▼            ▼            ▼                                   │
│   Minimalismo  Equilibrio  Robustez    Testing+Mocking                          │
│   Educativo    Practico    Produccion  Produccion                               │
│                                                                                  │
│  Complejidad creciente ──────────────────────────────────▶                      │
│  Funcionalidad creciente ────────────────────────────────▶                      │
│                                                                                  │
│  Otros frameworks notables (no cubiertos en detalle):                           │
│  • CUnit:       antiguo, interfaz compleja, menos mantenido                     │
│  • Criterion:   moderno, C/C++, buen output, menos adopcion                     │
│  • greatest:    header-only, inspirado en JUnit                                 │
│  • munit:       header-only con parametric + benchmarks                         │
│  • tau:         header-only, C/C++, moderno                                     │
│  • Snow:        header-only, BDD-style, usa __attribute__((constructor))        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Por que solo estos cuatro

| Framework | Razon de inclusion |
|-----------|--------------------|
| **MinUnit** | Representa el extremo minimalista: entender testing sin dependencias |
| **Unity** | El framework de asserts mas completo para C puro, parte del ecosistema ThrowTheSwitch |
| **Check** | El framework historico de referencia, con aislamiento por fork() nativo |
| **CMocka** | El unico que integra testing Y mocking en un solo paquete |

Juntos cubren todo el espectro de necesidades.

---

## 2. MinUnit — el framework en 3 lineas

### 2.1 Origen e historia

MinUnit fue creado por **Jera Design** como demostracion de que un framework de testing en C puede ser extremadamente simple. La version original (2002) tiene literalmente 3 lineas de macro:

```c
// minunit.h — version original (Jera Design, 2002)
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                               if (message) return message; } while (0)
extern int tests_run;
```

### 2.2 Version extendida moderna

La version original es demasiado basica para uso real. Una version extendida que mantiene la filosofia header-only:

```c
// minunit.h — version extendida
#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- Contadores ---
static int mu_tests_run = 0;
static int mu_tests_failed = 0;
static int mu_asserts = 0;

// --- Assertion base ---
#define mu_assert(message, test) do {                                   \
    mu_asserts++;                                                       \
    if (!(test)) {                                                      \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, message);    \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

// --- Assertions tipadas ---
#define mu_assert_int_eq(expected, actual) do {                         \
    mu_asserts++;                                                       \
    int _e = (expected), _a = (actual);                                \
    if (_e != _a) {                                                    \
        printf("  FAIL: %s:%d: expected %d, got %d\n",                \
               __FILE__, __LINE__, _e, _a);                            \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

#define mu_assert_double_eq(expected, actual, epsilon) do {             \
    mu_asserts++;                                                       \
    double _e = (expected), _a = (actual);                             \
    if (fabs(_e - _a) > (epsilon)) {                                   \
        printf("  FAIL: %s:%d: expected %.6f, got %.6f\n",            \
               __FILE__, __LINE__, _e, _a);                            \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

#define mu_assert_string_eq(expected, actual) do {                      \
    mu_asserts++;                                                       \
    const char *_e = (expected), *_a = (actual);                       \
    if (strcmp(_e, _a) != 0) {                                         \
        printf("  FAIL: %s:%d: expected \"%s\", got \"%s\"\n",        \
               __FILE__, __LINE__, _e, _a);                            \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

#define mu_assert_null(ptr) do {                                        \
    mu_asserts++;                                                       \
    if ((ptr) != NULL) {                                               \
        printf("  FAIL: %s:%d: expected NULL, got %p\n",              \
               __FILE__, __LINE__, (void *)(ptr));                     \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

#define mu_assert_not_null(ptr) do {                                    \
    mu_asserts++;                                                       \
    if ((ptr) == NULL) {                                               \
        printf("  FAIL: %s:%d: expected non-NULL, got NULL\n",        \
               __FILE__, __LINE__);                                    \
        mu_tests_failed++;                                              \
        return;                                                         \
    }                                                                   \
} while (0)

// --- Runner ---
#define mu_run_test(test) do {                                          \
    printf("  RUN:  %s\n", #test);                                     \
    test();                                                             \
    mu_tests_run++;                                                     \
    if (mu_tests_failed == 0) {                                        \
        printf("  PASS: %s\n", #test);                                 \
    }                                                                   \
} while (0)

// --- Resumen ---
#define mu_report() do {                                                \
    printf("\n%d tests, %d assertions, %d failures\n",                 \
           mu_tests_run, mu_asserts, mu_tests_failed);                 \
    return (mu_tests_failed != 0) ? 1 : 0;                            \
} while (0)

#endif // MINUNIT_H
```

### 2.3 Uso de MinUnit

```c
// test_stack.c
#include "minunit.h"
#include "stack.h"

// --- Tests ---

static void test_stack_new(void) {
    Stack *s = stack_new(10);
    mu_assert_not_null(s);
    mu_assert_int_eq(0, stack_size(s));
    mu_assert_int_eq(10, stack_capacity(s));
    stack_free(s);
}

static void test_stack_push_pop(void) {
    Stack *s = stack_new(10);
    stack_push(s, 42);
    stack_push(s, 99);
    mu_assert_int_eq(2, stack_size(s));
    mu_assert_int_eq(99, stack_pop(s));
    mu_assert_int_eq(42, stack_pop(s));
    mu_assert_int_eq(0, stack_size(s));
    stack_free(s);
}

static void test_stack_overflow(void) {
    Stack *s = stack_new(2);
    mu_assert(stack_push(s, 1) == true, "push 1 should succeed");
    mu_assert(stack_push(s, 2) == true, "push 2 should succeed");
    mu_assert(stack_push(s, 3) == false, "push 3 should fail (full)");
    stack_free(s);
}

int main(void) {
    printf("=== Stack Tests ===\n");
    mu_run_test(test_stack_new);
    mu_run_test(test_stack_push_pop);
    mu_run_test(test_stack_overflow);
    mu_report();
}
```

```
=== Stack Tests ===
  RUN:  test_stack_new
  PASS: test_stack_new
  RUN:  test_stack_push_pop
  PASS: test_stack_push_pop
  RUN:  test_stack_overflow
  PASS: test_stack_overflow

3 tests, 9 assertions, 0 failures
```

### 2.4 Limitaciones de MinUnit

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  LIMITACIONES DE MINUNIT                                                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ✗ No tiene setUp/tearDown                                                      │
│    → Hay que repetir la inicializacion en cada test                             │
│    → Si un assert falla, el cleanup no se ejecuta (memory leak)                │
│                                                                                  │
│  ✗ No tiene aislamiento entre tests                                             │
│    → Un crash (SEGFAULT) mata todo el runner                                    │
│    → No hay fork(), no hay setjmp/longjmp                                       │
│                                                                                  │
│  ✗ No tiene mocking                                                             │
│    → Imposible reemplazar dependencias                                          │
│                                                                                  │
│  ✗ No tiene output estructurado                                                │
│    → No genera XML/JUnit/TAP                                                    │
│    → No se integra nativamente con CI                                           │
│                                                                                  │
│  ✗ No tiene test discovery                                                      │
│    → Hay que listar cada test manualmente en main()                             │
│                                                                                  │
│  ✗ No tiene timeout ni retry                                                    │
│    → Un test que cuelga bloquea todo                                            │
│                                                                                  │
│  ✗ Variables static causan problemas multi-archivo                              │
│    → Si incluyes minunit.h en dos .c, los contadores se duplican               │
│    → Solucion: un solo archivo de test, o mover contadores a un .c             │
│                                                                                  │
│  Conclusion: MinUnit es excelente para aprender y para proyectos               │
│  muy pequenos. NO es suficiente para produccion.                                │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Unity — el framework de asserts

### 3.1 Resumen (cubierto en detalle en T01)

Unity es parte del ecosistema **ThrowTheSwitch.org** (Unity + CMock + Ceedling). Se distribuye como archivos fuente (no libreria compilada) y se enfoca en proporcionar la coleccion de assertion macros mas completa disponible para C.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  UNITY — RESUMEN                                                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Distribucion:  3 archivos (unity.c, unity.h, unity_internals.h)               │
│  Tamano:        ~4,000 LOC                                                      │
│  Licencia:      MIT                                                             │
│  Lenguaje:      C puro (C89 compatible)                                         │
│  Dependencias:  Ninguna                                                         │
│                                                                                  │
│  Puntos fuertes:                                                                │
│  • 80+ macros de assertion tipadas                                              │
│  • setUp/tearDown por archivo de test                                           │
│  • Facil de integrar (copiar 3 archivos)                                        │
│  • Output limpio y configurable                                                 │
│  • Excelente documentacion                                                      │
│  • Funciona en embedded (microcontroladores)                                    │
│  • Ecosistema: CMock (mocking), Ceedling (build+runner)                         │
│                                                                                  │
│  Puntos debiles:                                                                │
│  • setUp/tearDown global por archivo (no por grupo)                             │
│  • No tiene mocking integrado (necesita CMock separado)                         │
│  • No tiene aislamiento por fork()                                              │
│  • Sin test discovery automatico (necesita ruby scripts o main manual)          │
│  • No detecta leaks                                                             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Ejemplo tipico de Unity

```c
#include "unity.h"
#include "calculator.h"

static Calculator *calc;

void setUp(void) {
    calc = calculator_new();
}

void tearDown(void) {
    calculator_free(calc);
}

void test_add_positive_numbers(void) {
    TEST_ASSERT_EQUAL_INT(5, calculator_add(calc, 2, 3));
}

void test_add_negative_numbers(void) {
    TEST_ASSERT_EQUAL_INT(-5, calculator_add(calc, -2, -3));
}

void test_divide_by_zero_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(CALC_ERR_DIV_ZERO, calculator_divide(calc, 10, 0));
}

void test_memory_stores_last_result(void) {
    calculator_add(calc, 2, 3);
    TEST_ASSERT_EQUAL_INT(5, calculator_memory_recall(calc));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_positive_numbers);
    RUN_TEST(test_add_negative_numbers);
    RUN_TEST(test_divide_by_zero_returns_error);
    RUN_TEST(test_memory_stores_last_result);
    return UNITY_END();
}
```

### 3.3 Catalogo de macros de Unity (seleccion clave)

```c
// --- Booleanos ---
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);

// --- Enteros ---
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_EQUAL_INT8(expected, actual);
TEST_ASSERT_EQUAL_INT16(expected, actual);
TEST_ASSERT_EQUAL_INT32(expected, actual);
TEST_ASSERT_EQUAL_INT64(expected, actual);
TEST_ASSERT_EQUAL_UINT(expected, actual);
TEST_ASSERT_EQUAL_UINT8(expected, actual);
TEST_ASSERT_EQUAL_UINT16(expected, actual);
TEST_ASSERT_EQUAL_UINT32(expected, actual);
TEST_ASSERT_EQUAL_HEX(expected, actual);
TEST_ASSERT_EQUAL_HEX8(expected, actual);
TEST_ASSERT_EQUAL_HEX16(expected, actual);
TEST_ASSERT_EQUAL_HEX32(expected, actual);
TEST_ASSERT_BITS(mask, expected, actual);

// --- Comparacion ---
TEST_ASSERT_GREATER_THAN(threshold, actual);
TEST_ASSERT_LESS_THAN(threshold, actual);
TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual);
TEST_ASSERT_LESS_OR_EQUAL(threshold, actual);
TEST_ASSERT_INT_WITHIN(delta, expected, actual);

// --- Flotantes ---
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual);
TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual);
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_EQUAL_DOUBLE(expected, actual);
TEST_ASSERT_FLOAT_IS_INF(actual);
TEST_ASSERT_FLOAT_IS_NAN(actual);

// --- Strings ---
TEST_ASSERT_EQUAL_STRING(expected, actual);
TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len);

// --- Memoria ---
TEST_ASSERT_EQUAL_MEMORY(expected, actual, len);

// --- Arrays ---
TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements);
TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements);
TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, element_size, num_elements);
TEST_ASSERT_EACH_EQUAL_INT(expected, actual_array, num_elements);

// --- Control ---
TEST_FAIL();
TEST_FAIL_MESSAGE("reason");
TEST_PASS();
TEST_IGNORE();
TEST_IGNORE_MESSAGE("not implemented yet");

// --- Variante _MESSAGE (todos los assert tienen version _MESSAGE) ---
TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, "custom error message");
```

---

## 4. Check — el framework con aislamiento por fork()

### 4.1 Origen e historia

Check fue creado en 2001 por **Arien Malec** y es el framework de testing para C con mas historia en produccion. Su caracteristica definitoria es el **aislamiento por fork()**: cada test se ejecuta en un proceso hijo separado, de modo que un crash (SEGFAULT, abort) en un test NO mata al runner.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  CHECK — RESUMEN                                                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Sitio:         https://libcheck.github.io/check/                               │
│  Repositorio:   https://github.com/libcheck/check                              │
│  Licencia:      LGPL 2.1                                                        │
│  Lenguaje:      C puro (C99)                                                    │
│  Distribucion:  Libreria compartida (.so) y estatica (.a)                       │
│  Dependencias:  libm, librt (opcional), libsubunit (opcional)                   │
│  Tamano:        ~15,000 LOC                                                     │
│  Plataformas:   Linux, macOS, Windows (con limitaciones de fork)                │
│                                                                                  │
│  Usuarios conocidos:                                                            │
│  • GStreamer (multimedia framework)                                              │
│  • Expat (XML parser)                                                           │
│  • json-c                                                                       │
│  • libuv (versiones antiguas)                                                   │
│  • lighttpd                                                                     │
│                                                                                  │
│  ┌──────────────────────────────────────────────────────────────────┐            │
│  │  Arquitectura de Check:                                          │            │
│  │                                                                  │            │
│  │  Suite ──▶ TCase ──▶ Test function                              │            │
│  │                  ──▶ Test function                              │            │
│  │           TCase ──▶ Test function                              │            │
│  │                                                                  │            │
│  │  Suite = coleccion de TCases                                    │            │
│  │  TCase = coleccion de tests con shared setup/teardown           │            │
│  │  Test  = funcion individual, ejecutada en fork()                │            │
│  │                                                                  │            │
│  │  SRunner ejecuta una o mas Suites                               │            │
│  │  Cada test corre en su propio proceso (fork)                    │            │
│  └──────────────────────────────────────────────────────────────────┘            │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Instalacion

```bash
# Fedora/RHEL
sudo dnf install check check-devel

# Debian/Ubuntu
sudo apt install check libcheck-dev

# macOS (Homebrew)
brew install check

# Desde fuente
git clone https://github.com/libcheck/check.git
cd check
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .

# Verificar
pkg-config --modversion check
# 0.15.2
```

### 4.3 Anatomia de un test con Check

```c
// test_calculator.c
#include <check.h>
#include <stdlib.h>
#include "calculator.h"

// --- Fixture ---
static Calculator *calc;

static void setup(void) {
    calc = calculator_new();
}

static void teardown(void) {
    calculator_free(calc);
    calc = NULL;
}

// --- Tests ---
// START_TEST/END_TEST son macros que wrappean la funcion
// El argumento _i es un loop counter (para loop tests)

START_TEST(test_add_positive) {
    ck_assert_int_eq(calculator_add(calc, 2, 3), 5);
}
END_TEST

START_TEST(test_add_negative) {
    ck_assert_int_eq(calculator_add(calc, -2, -3), -5);
}
END_TEST

START_TEST(test_add_zero) {
    ck_assert_int_eq(calculator_add(calc, 0, 0), 0);
}
END_TEST

START_TEST(test_divide_normal) {
    ck_assert_int_eq(calculator_divide(calc, 10, 2), 5);
}
END_TEST

START_TEST(test_divide_by_zero) {
    // Este test CRASHEA (abort/signal)
    // Pero Check lo atrapa porque corre en fork()
    calculator_divide(calc, 10, 0);
    // Si llega aqui sin crash, el test pasa (puede ser un bug)
}
END_TEST

// --- Suite creation ---
static Suite *calculator_suite(void) {
    Suite *s = suite_create("Calculator");

    // TCase: arithmetic
    TCase *tc_arith = tcase_create("Arithmetic");
    tcase_add_checked_fixture(tc_arith, setup, teardown);
    tcase_add_test(tc_arith, test_add_positive);
    tcase_add_test(tc_arith, test_add_negative);
    tcase_add_test(tc_arith, test_add_zero);
    suite_add_tcase(s, tc_arith);

    // TCase: division
    TCase *tc_div = tcase_create("Division");
    tcase_add_checked_fixture(tc_div, setup, teardown);
    tcase_add_test(tc_div, test_divide_normal);
    tcase_add_test(tc_div, test_divide_by_zero);
    suite_add_tcase(s, tc_div);

    return s;
}

int main(void) {
    Suite *s = calculator_suite();
    SRunner *sr = srunner_create(s);

    // Opciones de output
    srunner_set_fork_status(sr, CK_FORK);       // Cada test en fork()
    srunner_set_log(sr, "test_results.log");      // Log a archivo
    srunner_set_xml(sr, "test_results.xml");      // XML output

    srunner_run_all(sr, CK_NORMAL);               // CK_VERBOSE para mas detalle

    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

### 4.4 Fork mode — la caracteristica distintiva

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  FORK MODE — COMO FUNCIONA                                                      │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  SRunner (proceso padre)                                                        │
│  │                                                                               │
│  ├── fork() ──▶ proceso hijo 1                                                  │
│  │              ├── setup()                                                      │
│  │              ├── test_add_positive()                                          │
│  │              ├── teardown()                                                   │
│  │              └── exit(0)  ← PASS                                             │
│  │              padre: waitpid() → exit code 0 → PASS                           │
│  │                                                                               │
│  ├── fork() ──▶ proceso hijo 2                                                  │
│  │              ├── setup()                                                      │
│  │              ├── test_divide_by_zero()                                        │
│  │              ├── SIGABRT (abort)                                              │
│  │              └── (muerto por signal)                                          │
│  │              padre: waitpid() → signal 6 → FAIL (con info del signal)        │
│  │                                                                               │
│  └── fork() ──▶ proceso hijo 3                                                  │
│                 ├── setup()                                                      │
│                 ├── test_buffer_overflow()                                       │
│                 ├── SIGSEGV (segfault)                                           │
│                 └── (muerto por signal)                                          │
│                 padre: waitpid() → signal 11 → FAIL (con info del signal)       │
│                                                                                  │
│  El runner padre NUNCA crashea. Siempre puede continuar                         │
│  al siguiente test y generar el reporte completo.                               │
│                                                                                  │
│  Comparacion:                                                                   │
│  • Unity/MinUnit: un SEGFAULT mata TODO el runner. Pierdes                      │
│    el reporte de los tests restantes.                                            │
│  • CMocka: no usa fork() por defecto, pero puede habilitarse                    │
│    manualmente (el usuario implementa fork() wrappers).                         │
│  • Check: fork() es el DEFAULT. Puedes desactivarlo con                         │
│    CK_NOFORK pero pierdes la ventaja principal.                                 │
│                                                                                  │
│  Desventaja del fork():                                                         │
│  • Mas lento (overhead de fork+exec por test)                                   │
│  • No funciona bien en Windows (no tiene fork() real)                           │
│  • No funciona en embedded (sin OS)                                             │
│  • Debugging mas dificil (no puedes usar gdb facilmente)                        │
│    → Solucion: CK_FORK=no en entorno, para debugging                           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 4.5 Assertions de Check

```c
// --- Core ---
ck_assert(expression);                     // Booleano
ck_assert_msg(expression, format, ...);    // Con mensaje personalizado

// --- Enteros ---
ck_assert_int_eq(X, Y);       // X == Y
ck_assert_int_ne(X, Y);       // X != Y
ck_assert_int_lt(X, Y);       // X < Y
ck_assert_int_le(X, Y);       // X <= Y
ck_assert_int_gt(X, Y);       // X > Y
ck_assert_int_ge(X, Y);       // X >= Y

// --- Unsigned ---
ck_assert_uint_eq(X, Y);
ck_assert_uint_ne(X, Y);
ck_assert_uint_lt(X, Y);
ck_assert_uint_le(X, Y);
ck_assert_uint_gt(X, Y);
ck_assert_uint_ge(X, Y);

// --- Strings ---
ck_assert_str_eq(X, Y);       // strcmp == 0
ck_assert_str_ne(X, Y);       // strcmp != 0
ck_assert_str_lt(X, Y);       // strcmp < 0
ck_assert_str_le(X, Y);       // strcmp <= 0
ck_assert_str_gt(X, Y);       // strcmp > 0
ck_assert_str_ge(X, Y);       // strcmp >= 0

// --- Punteros ---
ck_assert_ptr_eq(X, Y);       // X == Y
ck_assert_ptr_ne(X, Y);       // X != Y
ck_assert_ptr_null(X);        // X == NULL
ck_assert_ptr_nonnull(X);     // X != NULL

// --- Memoria ---
ck_assert_mem_eq(X, Y, LEN);  // memcmp == 0
ck_assert_mem_ne(X, Y, LEN);  // memcmp != 0

// --- Flotantes (Check >= 0.11.0) ---
ck_assert_float_eq(X, Y);
ck_assert_float_ne(X, Y);
ck_assert_float_lt(X, Y);
ck_assert_float_le(X, Y);
ck_assert_float_gt(X, Y);
ck_assert_float_ge(X, Y);
ck_assert_float_eq_tol(X, Y, TOL);  // |X - Y| < TOL
ck_assert_float_ne_tol(X, Y, TOL);
ck_assert_float_nan(X);
ck_assert_float_infinite(X);
ck_assert_float_finite(X);

ck_assert_double_eq(X, Y);
ck_assert_double_eq_tol(X, Y, TOL);
// ... (mismas variantes que float)

// --- Control ---
ck_abort();                    // Abortar test
ck_abort_msg(format, ...);     // Abortar con mensaje
```

### 4.6 Fixtures en Check — checked vs unchecked

Check tiene **dos tipos** de fixtures, algo que ningun otro framework ofrece:

```c
// CHECKED fixture: corre DENTRO del fork()
// → Si setup/teardown falla, solo ese test se marca como error
// → Es lo que normalmente quieres
tcase_add_checked_fixture(tc, setup, teardown);

// UNCHECKED fixture: corre FUERA del fork() (en el padre)
// → Si falla, TODOS los tests del TCase se abortan
// → Util para setup costoso (ej: inicializar base de datos una sola vez)
tcase_add_unchecked_fixture(tc, global_setup, global_teardown);
```

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  CHECKED vs UNCHECKED FIXTURE                                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  CHECKED (dentro del fork):                                                     │
│  ┌───────────────────────────────────────┐                                      │
│  │ proceso padre (SRunner)               │                                      │
│  │  │                                     │                                      │
│  │  ├─ fork() ──▶ hijo 1                 │                                      │
│  │  │             setup()     ← checked  │                                      │
│  │  │             test_A()               │                                      │
│  │  │             teardown()  ← checked  │                                      │
│  │  │                                     │                                      │
│  │  ├─ fork() ──▶ hijo 2                 │                                      │
│  │  │             setup()     ← checked  │                                      │
│  │  │             test_B()               │                                      │
│  │  │             teardown()  ← checked  │                                      │
│  │  │                                     │                                      │
│  └──┴────────────────────────────────────┘                                      │
│  Aislamiento total: cada test tiene su propio setup/teardown                    │
│                                                                                  │
│  UNCHECKED (fuera del fork):                                                    │
│  ┌───────────────────────────────────────┐                                      │
│  │ proceso padre (SRunner)               │                                      │
│  │  global_setup()  ← unchecked (padre)  │                                      │
│  │  │                                     │                                      │
│  │  ├─ fork() ──▶ hijo 1                 │                                      │
│  │  │             test_A()               │                                      │
│  │  │                                     │                                      │
│  │  ├─ fork() ──▶ hijo 2                 │                                      │
│  │  │             test_B()               │                                      │
│  │  │                                     │                                      │
│  │  global_teardown() ← unchecked        │                                      │
│  └──┴────────────────────────────────────┘                                      │
│  Setup una vez, teardown una vez. Tests leen pero no modifican.                 │
│                                                                                  │
│  Combinable: puedes tener AMBOS tipos en el mismo TCase.                        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 4.7 Loop tests — tests parametrizados nativos

Check tiene soporte nativo para tests parametrizados via loop:

```c
START_TEST(test_factorial_values) {
    int inputs[]   = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int expected[] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};

    // _i es el loop counter, inyectado por tcase_add_loop_test
    ck_assert_int_eq(factorial(inputs[_i]), expected[_i]);
}
END_TEST

// En la suite:
tcase_add_loop_test(tc, test_factorial_values, 0, 11);
// Ejecuta test_factorial_values con _i = 0, 1, 2, ..., 10
// Cada iteracion es un test SEPARADO (en su propio fork)
```

### 4.8 Timeout

```c
// Check tiene timeout por TCase
TCase *tc = tcase_create("TimeoutTests");
tcase_set_timeout(tc, 5);  // 5 segundos por test
// Default: 4 segundos
// 0 = sin timeout (peligroso)

// Variable de entorno override:
// CK_DEFAULT_TIMEOUT=10  (aplica a todos los TCases sin timeout explicito)
```

### 4.9 Output

```c
SRunner *sr = srunner_create(s);

// Log a archivo de texto
srunner_set_log(sr, "test_results.log");

// XML output
srunner_set_xml(sr, "test_results.xml");

// TAP output (Test Anything Protocol)
srunner_set_tap(sr, "test_results.tap");

// Subunit output (para subunit2junitxml)
// Requiere compilar Check con --enable-subunit

// Nivel de verbosidad en srunner_run_all():
srunner_run_all(sr, CK_SILENT);    // Solo conteo final
srunner_run_all(sr, CK_MINIMAL);   // + tests fallados
srunner_run_all(sr, CK_NORMAL);    // + resumen (default)
srunner_run_all(sr, CK_VERBOSE);   // + cada test individual
srunner_run_all(sr, CK_ENV);       // Leer de env CK_VERBOSITY
```

### 4.10 Compilacion con Check

```bash
# Obtener flags con pkg-config
gcc -o test_calc test_calculator.c calculator.c \
    $(pkg-config --cflags --libs check) -lm

# Sin pkg-config
gcc -o test_calc test_calculator.c calculator.c -lcheck -lsubunit -lrt -lm -lpthread

# Con CMake
# find_package(check) no existe oficialmente, pero pkg-config funciona:
```

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

add_executable(test_calc tests/test_calculator.c)
target_link_libraries(test_calc PRIVATE calculator_lib ${CHECK_LIBRARIES})
target_include_directories(test_calc PRIVATE ${CHECK_INCLUDE_DIRS})
target_compile_options(test_calc PRIVATE ${CHECK_CFLAGS_OTHER})
```

### 4.11 Multiples suites

```c
// test_main.c — runner que combina suites de diferentes archivos

// Declaradas en otros archivos:
extern Suite *calculator_suite(void);
extern Suite *string_utils_suite(void);
extern Suite *io_suite(void);

int main(void) {
    SRunner *sr = srunner_create(calculator_suite());
    srunner_add_suite(sr, string_utils_suite());
    srunner_add_suite(sr, io_suite());

    srunner_set_xml(sr, "test_results.xml");
    srunner_run_all(sr, CK_NORMAL);

    int nfail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

### 4.12 Selective test execution

```c
// Desde codigo:
srunner_run(sr, "Calculator", "Arithmetic", CK_VERBOSE);
// Solo la suite "Calculator", TCase "Arithmetic"

// Desde variables de entorno:
// CK_RUN_SUITE=Calculator
// CK_RUN_CASE=Arithmetic
```

```bash
CK_RUN_SUITE=Calculator CK_RUN_CASE=Division ./test_runner
```

---

## 5. CMocka — testing + mocking integrado

### 5.1 Resumen (cubierto en detalle en T02)

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  CMOCKA — RESUMEN                                                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Distribucion:  Libreria (.so/.a) via cmake                                     │
│  Tamano:        ~8,000 LOC                                                      │
│  Licencia:      Apache 2.0                                                      │
│  Lenguaje:      C puro (C99)                                                    │
│  Dependencias:  Ninguna en runtime                                              │
│                                                                                  │
│  Puntos fuertes:                                                                │
│  • Testing + mocking en un solo paquete                                         │
│  • Mocking via --wrap del linker (no modifica codigo de produccion)             │
│  • will_return/expect_value: verificacion de parametros de mocks                │
│  • Deteccion de memory leaks integrada (test_malloc/test_free)                  │
│  • Group setup/teardown (ademas de per-test)                                    │
│  • void **state: fixture como parametro del test                                │
│  • Output TAP, XML, subunit                                                     │
│  • Usado en proyectos grandes (Samba, libssh, OpenVPN)                          │
│                                                                                  │
│  Puntos debiles:                                                                │
│  • Include order especifico y fragil (stdarg → stddef → setjmp → cmocka)       │
│  • API menos intuitiva que Unity para principiantes                             │
│  • Mocking con --wrap requiere entender el linker                               │
│  • Menos assertions tipadas que Unity                                           │
│  • No tiene fork() nativo (un crash mata el runner)                             │
│  • Documentacion menos completa que Unity o Check                               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Ejemplo tipico de CMocka

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "calculator.h"

static int setup(void **state) {
    Calculator *calc = calculator_new();
    if (!calc) return -1;
    *state = calc;
    return 0;
}

static int teardown(void **state) {
    calculator_free(*state);
    return 0;
}

static void test_add(void **state) {
    Calculator *calc = *state;
    assert_int_equal(calculator_add(calc, 2, 3), 5);
}

static void test_subtract(void **state) {
    Calculator *calc = *state;
    assert_int_equal(calculator_subtract(calc, 5, 3), 2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_add, setup, teardown),
        cmocka_unit_test_setup_teardown(test_subtract, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 5.3 Mocking con CMocka (unico entre los cuatro)

```c
// --- Codigo de produccion ---
// database.h
int db_query(const char *sql, char *result, int result_size);

// --- Mock ---
int __wrap_db_query(const char *sql, char *result, int result_size) {
    check_expected(sql);
    const char *mock_result = mock_ptr_type(const char *);
    strncpy(result, mock_result, result_size);
    return mock_type(int);
}

// --- Test ---
static void test_fetch_user_name(void **state) {
    expect_string(__wrap_db_query, sql, "SELECT name FROM users WHERE id=1");
    will_return(__wrap_db_query, "Alice");  // mock_result
    will_return(__wrap_db_query, 0);        // return code

    char name[64];
    int rc = fetch_user_name(1, name, sizeof(name));
    assert_int_equal(rc, 0);
    assert_string_equal(name, "Alice");
}

// Compilacion con --wrap:
// gcc -o test_user test_user.c user_service.c mock_db.c -lcmocka
//     -Wl,--wrap=db_query
```

---

## 6. Tabla de comparacion exhaustiva

### 6.1 Caracteristicas fundamentales

| Caracteristica | MinUnit | Unity | Check | CMocka |
|----------------|---------|-------|-------|--------|
| **Tipo** | Header-only (macros) | Source files (3 archivos) | Libreria compilada | Libreria compilada |
| **Tamano** | ~50 LOC | ~4,000 LOC | ~15,000 LOC | ~8,000 LOC |
| **Licencia** | MIT | MIT | LGPL 2.1 | Apache 2.0 |
| **C standard** | C89 | C89 | C99 | C99 |
| **Dependencias** | Ninguna | Ninguna | libm, librt, libpthread | Ninguna |
| **Instalacion** | Copiar 1 header | Copiar 3 archivos | apt/dnf + pkg-config | apt/dnf o FetchContent |
| **Complejidad de setup** | Trivial | Baja | Media | Media |

### 6.2 Assertions

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Numero de assertions** | ~6 (custom) | 80+ | ~40 | ~20 |
| **Booleanas** | Si | Si | Si | Si |
| **Enteros tipados** | Basica | int8/16/32/64, uint, hex | int, uint | int |
| **Flotantes** | Manual | float, double, WITHIN, NaN, INF | float, double, tol | No nativa |
| **Strings** | strcmp manual | Si (con longitud) | Si (con comparacion) | Si |
| **Memoria** | No | Si (MEMORY, ARRAY) | Si (mem_eq) | Si (memory_equal) |
| **Arrays** | No | Si (INT_ARRAY, etc) | No | No |
| **Punteros** | NULL check | Si | Si (eq, ne, null) | Si (null, non_null) |
| **Rango** | No | WITHIN, GREATER, LESS | lt, le, gt, ge | in_range, not_in_range |
| **Mensaje custom** | Si | Si (_MESSAGE variants) | Si (_msg variants) | No (solo assert_true con mensaje) |
| **Hex output** | No | Si (HEX8/16/32) | No | No |
| **Bits** | No | Si (BITS) | No | No |

### 6.3 Fixtures y lifecycle

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **setUp/tearDown** | No | Si (1 par por archivo) | Si (por TCase) | Si (por test o grupo) |
| **Multiples fixtures** | No | Workarounds | Multiples TCases | Multiples grupos |
| **Group setup/teardown** | No | No | Si (unchecked fixture) | Si (group setup/teardown) |
| **State parameter** | No | No (globals) | No (globals) | Si (void **state) |
| **Checked vs unchecked** | N/A | N/A | Si (unico) | N/A |
| **Cleanup on assert fail** | No (leak) | Si (tearDown siempre) | Si (fork protege) | Si (setjmp/longjmp) |

### 6.4 Aislamiento y robustez

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Aislamiento entre tests** | Ninguno | Ninguno (setjmp/longjmp) | fork() por test | setjmp/longjmp |
| **Sobrevive a SEGFAULT** | No | No | **Si** | No |
| **Sobrevive a abort()** | No | No | **Si** | No |
| **Sobrevive a exit()** | No | No | **Si** | No |
| **Leak detection** | No | No | No | **Si** (test_malloc) |
| **Test timeout** | No | No | **Si** (tcase_set_timeout) | No |
| **Signal handling** | No | No | **Si** (via waitpid) | No |

### 6.5 Mocking

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Mocking integrado** | No | No | No | **Si** |
| **Mock via --wrap** | N/A | N/A | N/A | Si |
| **will_return** | N/A | N/A | N/A | Si |
| **expect_value** | N/A | N/A | N/A | Si |
| **Parameter checking** | N/A | N/A | N/A | Si |
| **Ecosistema mock** | N/A | CMock (separado) | N/A | Integrado |

### 6.6 Runner y output

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Test runner** | main() manual | main() manual (o ruby gen) | SRunner (programatico) | cmocka_run_group_tests |
| **Auto-discovery** | No | Via ruby script | No | No |
| **Output texto** | Custom printf | Estandarizado | Si (5 niveles) | Si |
| **Output XML** | No | No nativo | **Si** | **Si** |
| **Output TAP** | No | No | **Si** | **Si** |
| **Output JUnit** | No | No | Si (via XML) | Si (via XML) |
| **Selective run** | No | No | Si (suite/case) | No nativo |
| **Loop tests** | No | No | **Si** (_i counter) | No |
| **Test count** | Manual | UNITY_BEGIN/END | srunner_ntests_run | Return value |

### 6.7 Integracion con build systems

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Make** | Trivial | Facil | Media (pkg-config) | Media |
| **CMake** | Trivial | FetchContent | pkg_check_modules | FetchContent |
| **CTest** | Si | Si | Si | Si |
| **pkg-config** | No | No | **Si** | **Si** |
| **find_package** | No | No | No oficial | No oficial |
| **FetchContent** | No necesita | Si (GitHub) | Posible | Si (GitLab) |

### 6.8 Plataformas y portabilidad

| Aspecto | MinUnit | Unity | Check | CMocka |
|---------|---------|-------|-------|--------|
| **Linux** | Si | Si | Si | Si |
| **macOS** | Si | Si | Si | Si |
| **Windows** | Si | Si | Parcial (sin fork) | Si |
| **Embedded** | **Si** | **Si** | No (requiere fork/OS) | Parcial |
| **Cross-compile** | Facil | Facil | Dificil | Posible |

---

## 7. Comparacion en codigo — el mismo test en los cuatro frameworks

Para hacer la comparacion concreta, implementamos el **mismo test** (una pila/stack) en los cuatro frameworks.

### 7.1 Codigo bajo test (compartido)

```c
// stack.h
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

typedef struct Stack Stack;

Stack *stack_new(int capacity);
void   stack_free(Stack *s);
bool   stack_push(Stack *s, int value);
int    stack_pop(Stack *s);      // -1 si vacia
int    stack_peek(Stack *s);     // -1 si vacia
int    stack_size(Stack *s);
bool   stack_is_empty(Stack *s);
bool   stack_is_full(Stack *s);

#endif
```

```c
// stack.c
#include "stack.h"
#include <stdlib.h>

struct Stack {
    int *data;
    int top;
    int capacity;
};

Stack *stack_new(int capacity) {
    if (capacity <= 0) return NULL;
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;
    s->data = malloc(sizeof(int) * capacity);
    if (!s->data) { free(s); return NULL; }
    s->top = -1;
    s->capacity = capacity;
    return s;
}

void stack_free(Stack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

bool stack_push(Stack *s, int value) {
    if (!s || s->top >= s->capacity - 1) return false;
    s->data[++s->top] = value;
    return true;
}

int stack_pop(Stack *s) {
    if (!s || s->top < 0) return -1;
    return s->data[s->top--];
}

int stack_peek(Stack *s) {
    if (!s || s->top < 0) return -1;
    return s->data[s->top];
}

int stack_size(Stack *s) {
    return s ? s->top + 1 : 0;
}

bool stack_is_empty(Stack *s) {
    return !s || s->top < 0;
}

bool stack_is_full(Stack *s) {
    return s && s->top >= s->capacity - 1;
}
```

### 7.2 MinUnit

```c
// test_stack_minunit.c
#include "minunit.h"
#include "stack.h"

static void test_new_stack_is_empty(void) {
    Stack *s = stack_new(10);
    mu_assert_not_null(s);
    mu_assert("new stack should be empty", stack_is_empty(s));
    mu_assert_int_eq(0, stack_size(s));
    stack_free(s);
}

static void test_push_increases_size(void) {
    Stack *s = stack_new(10);
    stack_push(s, 42);
    mu_assert_int_eq(1, stack_size(s));
    mu_assert("stack should not be empty after push", !stack_is_empty(s));
    stack_free(s);
}

static void test_pop_returns_last_pushed(void) {
    Stack *s = stack_new(10);
    stack_push(s, 1);
    stack_push(s, 2);
    stack_push(s, 3);
    mu_assert_int_eq(3, stack_pop(s));
    mu_assert_int_eq(2, stack_pop(s));
    mu_assert_int_eq(1, stack_pop(s));
    stack_free(s);
}

static void test_pop_empty_returns_negative_one(void) {
    Stack *s = stack_new(10);
    mu_assert_int_eq(-1, stack_pop(s));
    stack_free(s);
}

static void test_push_full_returns_false(void) {
    Stack *s = stack_new(2);
    mu_assert("push 1", stack_push(s, 1));
    mu_assert("push 2", stack_push(s, 2));
    mu_assert("push 3 should fail", !stack_push(s, 3));
    stack_free(s);
}

static void test_peek_does_not_remove(void) {
    Stack *s = stack_new(10);
    stack_push(s, 42);
    mu_assert_int_eq(42, stack_peek(s));
    mu_assert_int_eq(42, stack_peek(s));  // sigue ahi
    mu_assert_int_eq(1, stack_size(s));
    stack_free(s);
}

static void test_new_with_zero_capacity_returns_null(void) {
    mu_assert_null(stack_new(0));
    mu_assert_null(stack_new(-5));
}

int main(void) {
    printf("=== Stack Tests (MinUnit) ===\n");
    mu_run_test(test_new_stack_is_empty);
    mu_run_test(test_push_increases_size);
    mu_run_test(test_pop_returns_last_pushed);
    mu_run_test(test_pop_empty_returns_negative_one);
    mu_run_test(test_push_full_returns_false);
    mu_run_test(test_peek_does_not_remove);
    mu_run_test(test_new_with_zero_capacity_returns_null);
    mu_report();
}

// Lineas de framework: 0 (header incluido)
// Lineas de test:       ~60
// Boilerplate:          Bajo
// Cleanup seguro:       No (si assert falla, stack_free no se llama)
```

### 7.3 Unity

```c
// test_stack_unity.c
#include "unity.h"
#include "stack.h"

static Stack *s;

void setUp(void) {
    s = stack_new(10);
}

void tearDown(void) {
    stack_free(s);
    s = NULL;
}

void test_new_stack_is_empty(void) {
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(stack_is_empty(s));
    TEST_ASSERT_EQUAL_INT(0, stack_size(s));
}

void test_push_increases_size(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL_INT(1, stack_size(s));
    TEST_ASSERT_FALSE(stack_is_empty(s));
}

void test_pop_returns_last_pushed(void) {
    stack_push(s, 1);
    stack_push(s, 2);
    stack_push(s, 3);
    TEST_ASSERT_EQUAL_INT(3, stack_pop(s));
    TEST_ASSERT_EQUAL_INT(2, stack_pop(s));
    TEST_ASSERT_EQUAL_INT(1, stack_pop(s));
}

void test_pop_empty_returns_negative_one(void) {
    TEST_ASSERT_EQUAL_INT(-1, stack_pop(s));
}

void test_push_full_returns_false(void) {
    // Necesitamos stack de capacidad 2, pero setUp crea de 10
    // Workaround: free y recrear
    stack_free(s);
    s = stack_new(2);
    TEST_ASSERT_TRUE(stack_push(s, 1));
    TEST_ASSERT_TRUE(stack_push(s, 2));
    TEST_ASSERT_FALSE(stack_push(s, 3));
}

void test_peek_does_not_remove(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL_INT(42, stack_peek(s));
    TEST_ASSERT_EQUAL_INT(42, stack_peek(s));
    TEST_ASSERT_EQUAL_INT(1, stack_size(s));
}

void test_new_with_zero_capacity_returns_null(void) {
    TEST_ASSERT_NULL(stack_new(0));
    TEST_ASSERT_NULL(stack_new(-5));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_new_stack_is_empty);
    RUN_TEST(test_push_increases_size);
    RUN_TEST(test_pop_returns_last_pushed);
    RUN_TEST(test_pop_empty_returns_negative_one);
    RUN_TEST(test_push_full_returns_false);
    RUN_TEST(test_peek_does_not_remove);
    RUN_TEST(test_new_with_zero_capacity_returns_null);
    return UNITY_END();
}

// Lineas de framework: 3 archivos a incluir en build
// Lineas de test:       ~55
// Boilerplate:          Medio (setUp/tearDown + UNITY_BEGIN/END + RUN_TEST)
// Cleanup seguro:       Si (tearDown se ejecuta siempre)
// Limitacion:           setUp crea stack(10), test_push_full necesita stack(2)
//                       → workaround feo (free + recrear)
```

### 7.4 Check

```c
// test_stack_check.c
#include <check.h>
#include <stdlib.h>
#include "stack.h"

static Stack *s;

static void setup(void) {
    s = stack_new(10);
}

static void teardown(void) {
    stack_free(s);
    s = NULL;
}

START_TEST(test_new_stack_is_empty) {
    ck_assert_ptr_nonnull(s);
    ck_assert(stack_is_empty(s));
    ck_assert_int_eq(stack_size(s), 0);
}
END_TEST

START_TEST(test_push_increases_size) {
    stack_push(s, 42);
    ck_assert_int_eq(stack_size(s), 1);
    ck_assert(!stack_is_empty(s));
}
END_TEST

START_TEST(test_pop_returns_last_pushed) {
    stack_push(s, 1);
    stack_push(s, 2);
    stack_push(s, 3);
    ck_assert_int_eq(stack_pop(s), 3);
    ck_assert_int_eq(stack_pop(s), 2);
    ck_assert_int_eq(stack_pop(s), 1);
}
END_TEST

START_TEST(test_pop_empty_returns_negative_one) {
    ck_assert_int_eq(stack_pop(s), -1);
}
END_TEST

START_TEST(test_peek_does_not_remove) {
    stack_push(s, 42);
    ck_assert_int_eq(stack_peek(s), 42);
    ck_assert_int_eq(stack_peek(s), 42);
    ck_assert_int_eq(stack_size(s), 1);
}
END_TEST

// Segundo TCase con diferente fixture (stack de capacidad 2)
static Stack *small_s;

static void setup_small(void) {
    small_s = stack_new(2);
}

static void teardown_small(void) {
    stack_free(small_s);
    small_s = NULL;
}

START_TEST(test_push_full_returns_false) {
    ck_assert(stack_push(small_s, 1));
    ck_assert(stack_push(small_s, 2));
    ck_assert(!stack_push(small_s, 3));
}
END_TEST

START_TEST(test_new_with_zero_capacity_returns_null) {
    ck_assert_ptr_null(stack_new(0));
    ck_assert_ptr_null(stack_new(-5));
}
END_TEST

static Suite *stack_suite(void) {
    Suite *s = suite_create("Stack");

    TCase *tc_basic = tcase_create("Basic");
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_new_stack_is_empty);
    tcase_add_test(tc_basic, test_push_increases_size);
    tcase_add_test(tc_basic, test_pop_returns_last_pushed);
    tcase_add_test(tc_basic, test_pop_empty_returns_negative_one);
    tcase_add_test(tc_basic, test_peek_does_not_remove);
    suite_add_tcase(s, tc_basic);

    TCase *tc_limits = tcase_create("Limits");
    tcase_add_checked_fixture(tc_limits, setup_small, teardown_small);
    tcase_add_test(tc_limits, test_push_full_returns_false);
    // test_new no necesita fixture, pero Check requiere que este en un TCase
    tcase_add_test(tc_limits, test_new_with_zero_capacity_returns_null);
    suite_add_tcase(s, tc_limits);

    return s;
}

int main(void) {
    Suite *s = stack_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int nfail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Lineas de framework: libreria a instalar
// Lineas de test:       ~80
// Boilerplate:          Alto (Suite, TCase, SRunner, START_TEST/END_TEST)
// Cleanup seguro:       Si (fork aísla todo)
// Ventaja:              Multiples fixtures naturales (multiples TCases)
//                       Un crash no mata el runner
```

### 7.5 CMocka

```c
// test_stack_cmocka.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "stack.h"

static int setup(void **state) {
    Stack *s = stack_new(10);
    if (!s) return -1;
    *state = s;
    return 0;
}

static int teardown(void **state) {
    stack_free(*state);
    return 0;
}

static int setup_small(void **state) {
    Stack *s = stack_new(2);
    if (!s) return -1;
    *state = s;
    return 0;
}

static void test_new_stack_is_empty(void **state) {
    Stack *s = *state;
    assert_non_null(s);
    assert_true(stack_is_empty(s));
    assert_int_equal(stack_size(s), 0);
}

static void test_push_increases_size(void **state) {
    Stack *s = *state;
    stack_push(s, 42);
    assert_int_equal(stack_size(s), 1);
    assert_false(stack_is_empty(s));
}

static void test_pop_returns_last_pushed(void **state) {
    Stack *s = *state;
    stack_push(s, 1);
    stack_push(s, 2);
    stack_push(s, 3);
    assert_int_equal(stack_pop(s), 3);
    assert_int_equal(stack_pop(s), 2);
    assert_int_equal(stack_pop(s), 1);
}

static void test_pop_empty_returns_negative_one(void **state) {
    Stack *s = *state;
    assert_int_equal(stack_pop(s), -1);
}

static void test_push_full_returns_false(void **state) {
    Stack *s = *state;  // stack de capacidad 2
    assert_true(stack_push(s, 1));
    assert_true(stack_push(s, 2));
    assert_false(stack_push(s, 3));
}

static void test_peek_does_not_remove(void **state) {
    Stack *s = *state;
    stack_push(s, 42);
    assert_int_equal(stack_peek(s), 42);
    assert_int_equal(stack_peek(s), 42);
    assert_int_equal(stack_size(s), 1);
}

static void test_new_with_zero_capacity_returns_null(void **state) {
    (void)state;
    assert_null(stack_new(0));
    assert_null(stack_new(-5));
}

int main(void) {
    const struct CMUnitTest basic_tests[] = {
        cmocka_unit_test_setup_teardown(test_new_stack_is_empty, setup, teardown),
        cmocka_unit_test_setup_teardown(test_push_increases_size, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pop_returns_last_pushed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pop_empty_returns_negative_one, setup, teardown),
        cmocka_unit_test_setup_teardown(test_peek_does_not_remove, setup, teardown),
    };

    const struct CMUnitTest limit_tests[] = {
        cmocka_unit_test_setup_teardown(test_push_full_returns_false, setup_small, teardown),
        cmocka_unit_test(test_new_with_zero_capacity_returns_null),
    };

    int rc = 0;
    rc += cmocka_run_group_tests(basic_tests, NULL, NULL);
    rc += cmocka_run_group_tests(limit_tests, NULL, NULL);
    return rc;
}

// Lineas de framework: libreria a instalar
// Lineas de test:       ~75
// Boilerplate:          Medio-alto (includes, void **state, CMUnitTest array)
// Cleanup seguro:       Si (teardown se ejecuta siempre)
// Ventaja:              Fixture como parametro (void **state), no globals
//                       setup_small da stack de cap 2 → limpio
//                       Leak detection automatica (si usas test_malloc)
```

### 7.6 Comparacion visual del codigo

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  COMPARACION VISUAL — EL MISMO TEST EN LOS 4 FRAMEWORKS                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Lineas de codigo del test (sin contar codigo bajo test):                       │
│                                                                                  │
│  MinUnit ██████████████████████████████░░░░░░░░░░░░░░░  60 LOC                  │
│  Unity   ████████████████████████████░░░░░░░░░░░░░░░░░  55 LOC                  │
│  CMocka  ███████████████████████████████████████░░░░░░░  75 LOC                  │
│  Check   ████████████████████████████████████████████░░  80 LOC                  │
│                                                                                  │
│  Boilerplate (setup del framework, no logica de test):                           │
│                                                                                  │
│  MinUnit ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   5 LOC                  │
│  Unity   ██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  12 LOC                  │
│  CMocka  ████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  18 LOC                  │
│  Check   ████████████████████████░░░░░░░░░░░░░░░░░░░░░  25 LOC                  │
│                                                                                  │
│  Fixture flexibility (1=rigido, 5=flexible):                                    │
│                                                                                  │
│  MinUnit ★░░░░  1 (no tiene)                                                    │
│  Unity   ★★░░░  2 (1 par global por archivo)                                    │
│  CMocka  ★★★★░  4 (per-test + group, void **state)                              │
│  Check   ★★★★★  5 (per-test + group, checked + unchecked, multiples TCases)     │
│                                                                                  │
│  Safety (sobrevive a crashes):                                                  │
│                                                                                  │
│  MinUnit ★░░░░  1 (un crash mata todo)                                          │
│  Unity   ★★░░░  2 (setjmp protege de assert fail, no de SEGFAULT)              │
│  CMocka  ★★░░░  2 (setjmp protege de assert fail, no de SEGFAULT)              │
│  Check   ★★★★★  5 (fork aísla absolutamente todo)                               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Criterios de eleccion

### 8.1 Arbol de decision

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     ARBOL DE DECISION                                            │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ¿Necesitas mocking?                                                            │
│  ├── SI ──▶ CMocka (o Unity + CMock si ya usas Unity)                           │
│  │                                                                               │
│  └── NO ──▶ ¿Los tests pueden crashear (SEGFAULT, abort)?                       │
│             ├── SI / PROBABLEMENTE ──▶ Check (fork aísla crashes)               │
│             │                                                                    │
│             └── NO ──▶ ¿Cuantos assertions tipados necesitas?                   │
│                        ├── MUCHOS (float, arrays, hex, bits)                    │
│                        │   ──▶ Unity (80+ macros)                               │
│                        │                                                         │
│                        └── POCOS (int, string, null)                            │
│                            ──▶ ¿Es un proyecto educativo o muy pequeno?         │
│                                ├── SI ──▶ MinUnit o assert.h                    │
│                                └── NO ──▶ Unity (mejor inversion a futuro)      │
│                                                                                  │
│  ¿Es embedded / microcontrolador?                                               │
│  ──▶ Unity (funciona sin OS, C89)                                               │
│  ──▶ MinUnit (aun mas simple)                                                   │
│  ──▶ NO Check (necesita fork/OS)                                                │
│  ──▶ NO CMocka (necesita --wrap, difícil en embedded)                           │
│                                                                                  │
│  ¿Windows es target principal?                                                  │
│  ──▶ Unity (mejor soporte Windows)                                              │
│  ──▶ CMocka (funciona en Windows)                                               │
│  ──▶ EVITAR Check (fork() problemático en Windows)                              │
│                                                                                  │
│  ¿Es un proyecto grande con muchos contribuidores?                              │
│  ──▶ CMocka o Check (mas estructura, mas disciplina)                            │
│  ──▶ No MinUnit (demasiado informal)                                            │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Tabla de escenarios concretos

| Escenario | Recomendacion | Por que |
|-----------|---------------|---------|
| Proyecto personal pequeno | MinUnit o assert.h | Cero dependencias, aprender conceptos |
| Libreria open source | Unity | Equilibrio funcionalidad/simplicidad, facil para contribuidores |
| Aplicacion de red/IO | CMocka | Mocking integrado para sockets, archivos, etc. |
| Parser/compilador | Check | Los parsers suelen crashear durante desarrollo; fork protege |
| Firmware embedded | Unity | Funciona sin OS, C89, pequeno footprint |
| Sistema critico con CI | Check + CMocka | Check para robustez, CMocka para mocking de hardware |
| Prototipo rapido | MinUnit | Agrega un header y empeza a testear |
| Proyecto con dependencias externas | CMocka | --wrap permite mockear sin modificar las dependencias |
| Equipo grande, proyecto corporativo | Check | Mas estructura (Suite/TCase), output estandar, timeout |
| Enseñanza/curso | MinUnit → Unity | MinUnit para entender que es un test, Unity para produccion |

### 8.3 Decision por prioridad

**Si tu prioridad es...**

| Prioridad | Mejor opcion |
|-----------|-------------|
| Minimo setup | MinUnit |
| Mas assertions tipados | Unity |
| Mocking | CMocka |
| Crash safety | Check |
| Portabilidad (embedded) | Unity |
| Output para CI | Check o CMocka |
| Leak detection | CMocka |
| Documentacion | Unity |
| Comunidad activa | Unity |
| Madurez en produccion | Check |

---

## 9. Licencias — implicaciones practicas

Las licencias de los frameworks tienen implicaciones reales para tu proyecto:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  LICENCIAS                                                                      │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  MinUnit:  MIT                                                                  │
│  ──▶ Sin restricciones. Puedes hacer lo que quieras.                            │
│                                                                                  │
│  Unity:    MIT                                                                  │
│  ──▶ Sin restricciones. Puedes incluirlo en software propietario.               │
│                                                                                  │
│  CMocka:   Apache 2.0                                                           │
│  ──▶ Permisiva. Requiere atribucion y notificacion de cambios.                  │
│     Puedes usar en software propietario.                                        │
│  ──▶ Incluye proteccion de patentes.                                            │
│                                                                                  │
│  Check:    LGPL 2.1                                                             │
│  ──▶ CUIDADO. Si linkeas estaticamente, tu software debe ser LGPL.             │
│     Si linkeas dinamicamente (.so), puedes mantenerlo propietario.              │
│  ──▶ Esto es irrelevante para tests (no se distribuyen con el producto).        │
│     Pero si tu build system mezcla test y produccion, revisa.                   │
│                                                                                  │
│  En la practica: para TESTING, la licencia raramente importa porque             │
│  el framework solo esta en los tests, que no se distribuyen al usuario          │
│  final. Pero si incluyes el framework en tu source tree (como Unity),           │
│  la licencia puede afectar la licencia de tu repositorio.                       │
│                                                                                  │
│  Para proyectos open source:                                                    │
│  • MIT/Apache: compatible con cualquier licencia                                │
│  • LGPL: compatible con GPL, problematico con MIT/BSD si linkeas estatico       │
│                                                                                  │
│  Para proyectos propietarios:                                                   │
│  • MIT/Apache: sin problemas                                                    │
│  • LGPL: linkear dinamicamente (.so) para evitar la obligacion copyleft         │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Rendimiento — overhead por framework

El overhead del framework de testing es generalmente despreciable comparado con la logica del test. Pero en embedded o con miles de tests, puede importar:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  BENCHMARKS APROXIMADOS (1000 tests triviales, Linux x86_64)                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  MinUnit   ██░░░░░░░░░░░░░░░░░░  ~0.002s (todo en un proceso)                  │
│  Unity     ███░░░░░░░░░░░░░░░░░  ~0.005s (todo en un proceso)                  │
│  CMocka    ████░░░░░░░░░░░░░░░░  ~0.010s (setjmp overhead)                     │
│  Check     ██████████████████░░  ~0.500s (fork+waitpid por test)               │
│                                                                                  │
│  Notas:                                                                         │
│  • El fork() de Check tiene un costo real (~0.5ms por test)                    │
│  • Con 10,000 tests, Check tarda ~5s solo en fork overhead                     │
│  • Unity/CMocka/MinUnit: el overhead es imperceptible                          │
│  • En embedded: Unity/MinUnit son las unicas opciones viables                  │
│  • El tiempo de los tests reales domina en proyectos reales                    │
│                                                                                  │
│  Solucion para Check lento:                                                     │
│  • CK_FORK=no para desarrollo (rapido, sin aislamiento)                        │
│  • CK_FORK=yes para CI (lento, con aislamiento)                               │
│  • Agrupar tests relacionados en menos ejecutables                             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 11. Migracion entre frameworks

### 11.1 MinUnit → Unity

La migracion mas comun cuando un proyecto crece:

```c
// MinUnit:
mu_assert("msg", condition);
mu_assert_int_eq(expected, actual);
mu_assert_null(ptr);
mu_run_test(test_func);
mu_report();

// Unity equivalente:
TEST_ASSERT_MESSAGE(condition, "msg");      // o TEST_ASSERT_TRUE
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_NULL(ptr);
RUN_TEST(test_func);
return UNITY_END();

// Pasos de migracion:
// 1. Agregar unity.c, unity.h, unity_internals.h al proyecto
// 2. Reemplazar #include "minunit.h" → #include "unity.h"
// 3. Convertir assertions (tabla abajo)
// 4. Extraer setup comun a setUp/tearDown
// 5. Cambiar main: mu_report() → UNITY_BEGIN/UNITY_END
// 6. Cambiar mu_run_test → RUN_TEST
```

| MinUnit | Unity |
|---------|-------|
| `mu_assert(msg, expr)` | `TEST_ASSERT_MESSAGE(expr, msg)` (nota: argumentos invertidos!) |
| `mu_assert_int_eq(e, a)` | `TEST_ASSERT_EQUAL_INT(e, a)` |
| `mu_assert_double_eq(e, a, eps)` | `TEST_ASSERT_DOUBLE_WITHIN(eps, e, a)` |
| `mu_assert_string_eq(e, a)` | `TEST_ASSERT_EQUAL_STRING(e, a)` |
| `mu_assert_null(p)` | `TEST_ASSERT_NULL(p)` |
| `mu_assert_not_null(p)` | `TEST_ASSERT_NOT_NULL(p)` |
| `mu_run_test(f)` | `RUN_TEST(f)` |
| `mu_report()` | `return UNITY_END();` |

### 11.2 Unity → CMocka

Cuando necesitas mocking:

```c
// Unity:
void setUp(void) { calc = calculator_new(); }
void tearDown(void) { calculator_free(calc); }
void test_add(void) { TEST_ASSERT_EQUAL_INT(5, calculator_add(calc, 2, 3)); }
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add);
    return UNITY_END();
}

// CMocka equivalente:
static int setup(void **state) {
    *state = calculator_new();
    return *state ? 0 : -1;
}
static int teardown(void **state) {
    calculator_free(*state);
    return 0;
}
static void test_add(void **state) {
    Calculator *calc = *state;
    assert_int_equal(calculator_add(calc, 2, 3), 5);
}
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_add, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

| Unity | CMocka |
|-------|--------|
| `TEST_ASSERT_TRUE(x)` | `assert_true(x)` |
| `TEST_ASSERT_FALSE(x)` | `assert_false(x)` |
| `TEST_ASSERT_EQUAL_INT(e, a)` | `assert_int_equal(a, e)` (argumentos invertidos!) |
| `TEST_ASSERT_NOT_EQUAL(e, a)` | `assert_int_not_equal(a, e)` |
| `TEST_ASSERT_NULL(p)` | `assert_null(p)` |
| `TEST_ASSERT_NOT_NULL(p)` | `assert_non_null(p)` |
| `TEST_ASSERT_EQUAL_STRING(e, a)` | `assert_string_equal(a, e)` |
| `TEST_ASSERT_EQUAL_MEMORY(e, a, n)` | `assert_memory_equal(a, e, n)` |
| `TEST_FAIL_MESSAGE(msg)` | `fail_msg("%s", msg)` |
| `TEST_IGNORE()` | `skip()` |

**Cuidado con el orden de argumentos**: Unity usa `(expected, actual)`, CMocka usa `(actual, expected)` en la mayoria de assertions. Esto es una fuente comun de confusion en migraciones.

### 11.3 Unity/CMocka → Check

Cuando necesitas aislamiento por fork():

```c
// Unity test:
void test_parse_null(void) {
    // Esto puede causar SEGFAULT si parser no valida NULL
    TEST_ASSERT_NULL(parse(NULL));
}

// Check equivalente (seguro contra SEGFAULT):
START_TEST(test_parse_null) {
    ck_assert_ptr_null(parse(NULL));
    // Si parse(NULL) causa SEGFAULT, Check lo atrapa via fork
    // El runner sobrevive y reporta el test como FAILED
}
END_TEST
```

| Unity | Check |
|-------|-------|
| `TEST_ASSERT_TRUE(x)` | `ck_assert(x)` |
| `TEST_ASSERT_EQUAL_INT(e, a)` | `ck_assert_int_eq(a, e)` |
| `TEST_ASSERT_NOT_EQUAL(e, a)` | `ck_assert_int_ne(a, e)` |
| `TEST_ASSERT_GREATER_THAN(t, a)` | `ck_assert_int_gt(a, t)` |
| `TEST_ASSERT_NULL(p)` | `ck_assert_ptr_null(p)` |
| `TEST_ASSERT_NOT_NULL(p)` | `ck_assert_ptr_nonnull(p)` |
| `TEST_ASSERT_EQUAL_STRING(e, a)` | `ck_assert_str_eq(a, e)` |
| `TEST_ASSERT_EQUAL_MEMORY(e, a, n)` | `ck_assert_mem_eq(a, e, n)` |
| `TEST_FAIL_MESSAGE(msg)` | `ck_abort_msg(msg)` |

---

## 12. Uso combinado — multiples frameworks en un proyecto

En proyectos grandes, es posible (y a veces deseable) usar multiples frameworks:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  ESTRATEGIA DE MULTIPLES FRAMEWORKS                                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Proyecto grande:                                                               │
│  ├── tests/unit/           ← Unity (assertions claras, facil de escribir)       │
│  ├── tests/mock/           ← CMocka (necesita mocking de dependencias)          │
│  ├── tests/crash/          ← Check (tests que pueden crashear)                  │
│  └── tests/smoke/          ← assert.h (tests ultra-simples)                    │
│                                                                                  │
│  CTest como meta-runner:                                                        │
│  • CTest no sabe ni le importa que framework usa cada test                     │
│  • Cada ejecutable de test es un test CTest independiente                      │
│  • Labels para separar: "unit", "mock", "crash", "smoke"                       │
│                                                                                  │
│  Ejemplo CMakeLists.txt:                                                        │
│                                                                                  │
│  # Tests con Unity                                                              │
│  add_executable(test_math tests/unit/test_math.c)                              │
│  target_link_libraries(test_math PRIVATE mylib unity)                           │
│  add_test(NAME unit.math COMMAND test_math)                                    │
│  set_tests_properties(unit.math PROPERTIES LABELS "unit")                      │
│                                                                                  │
│  # Tests con CMocka (mocking)                                                   │
│  add_executable(test_network tests/mock/test_network.c)                        │
│  target_link_libraries(test_network PRIVATE mylib cmocka-static)               │
│  target_link_options(test_network PRIVATE -Wl,--wrap=connect)                  │
│  add_test(NAME mock.network COMMAND test_network)                              │
│  set_tests_properties(mock.network PROPERTIES LABELS "mock")                   │
│                                                                                  │
│  # Tests con Check (crash-prone)                                                │
│  add_executable(test_parser tests/crash/test_parser.c)                         │
│  target_link_libraries(test_parser PRIVATE mylib ${CHECK_LIBRARIES})           │
│  add_test(NAME crash.parser COMMAND test_parser)                               │
│  set_tests_properties(crash.parser PROPERTIES LABELS "crash")                  │
│                                                                                  │
│  Ejecucion selectiva:                                                           │
│  ctest -L "unit"   # Solo tests con Unity                                      │
│  ctest -L "mock"   # Solo tests con CMocka                                     │
│  ctest -L "crash"  # Solo tests con Check                                      │
│  ctest             # Todo                                                       │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Comparacion con Rust y Go

### 13.1 Rust testing

Rust tiene testing **integrado en el lenguaje y en cargo**. No existe la decision de "que framework usar" para la funcionalidad basica:

```rust
// Rust: todo esta integrado
#[cfg(test)]
mod tests {
    use super::*;

    // Equivalente a setUp: cada test crea su propio estado
    fn setup() -> Stack {
        Stack::new(10)
    }

    #[test]
    fn test_push_pop() {
        let mut s = setup();
        s.push(42);
        assert_eq!(s.pop(), Some(42));
    }

    #[test]
    #[should_panic(expected = "empty")]
    fn test_pop_empty_panics() {
        // Equivalente a WILL_FAIL de CTest
        let mut s = setup();
        s.pop().expect("empty");  // panic! → test pasa
    }

    #[test]
    fn test_is_empty() {
        let s = setup();
        assert!(s.is_empty());
    }
}

// cargo test                    → todos los tests
// cargo test test_push          → filtro por nombre
// cargo test -- --test-threads 1   → sin paralelismo
// cargo test -- --ignored       → solo tests #[ignore]
```

| Caracteristica | Frameworks C | Rust built-in |
|----------------|-------------|---------------|
| Assertions | Depende del framework | `assert!`, `assert_eq!`, `assert_ne!` + crates |
| Fixtures | Manual (setUp/tearDown, void **state) | Constructor + `Drop` (RAII) |
| Crash isolation | Solo Check (fork) | Cada test en su propio thread (catch_unwind) |
| Mocking | Solo CMocka (--wrap) | mockall crate, interfaces |
| Coverage | gcov/lcov separado | `cargo tarpaulin` o nightly `--coverage` |
| Discovery | Manual o ruby scripts | Automatico (atributo #[test]) |
| Build integration | Separado (CTest, Make) | Integrado (cargo test) |

### 13.2 Go testing

Go tiene su propio framework integrado, similar a Rust pero con filosofia diferente:

```go
// Go: testing integrado
package stack

import "testing"

func TestPushPop(t *testing.T) {
    s := NewStack(10)
    s.Push(42)
    got := s.Pop()
    if got != 42 {
        t.Errorf("Pop() = %d, want 42", got)
    }
}

func TestIsEmpty(t *testing.T) {
    s := NewStack(10)
    if !s.IsEmpty() {
        t.Error("new stack should be empty")
    }
}

// Subtests (como TCases en Check):
func TestStack(t *testing.T) {
    t.Run("push increases size", func(t *testing.T) {
        s := NewStack(10)
        s.Push(1)
        if s.Size() != 1 {
            t.Errorf("size = %d, want 1", s.Size())
        }
    })
    t.Run("pop empty returns error", func(t *testing.T) {
        s := NewStack(10)
        _, err := s.Pop()
        if err == nil {
            t.Error("expected error for pop on empty stack")
        }
    })
}

// Table-driven tests (patron idomiatico Go):
func TestFactorial(t *testing.T) {
    tests := []struct {
        input    int
        expected int
    }{
        {0, 1}, {1, 1}, {5, 120}, {10, 3628800},
    }
    for _, tt := range tests {
        t.Run(fmt.Sprintf("factorial(%d)", tt.input), func(t *testing.T) {
            got := Factorial(tt.input)
            if got != tt.expected {
                t.Errorf("Factorial(%d) = %d, want %d",
                         tt.input, got, tt.expected)
            }
        })
    }
}
```

| Caracteristica | Frameworks C | Go built-in |
|----------------|-------------|-------------|
| Assertions | Framework-specific macros | `t.Error`, `t.Fatal` (sin assert library) |
| Subtests | Check TCases, CMocka groups | `t.Run()` nativo |
| Table-driven | CMake parametrizados, manual | Idomiatico (slice de structs) |
| Fixtures | setUp/tearDown | `TestMain(m *testing.M)` |
| Cleanup | tearDown, defer en Go | `t.Cleanup(func)` |
| Parallelism | CTest -j N | `t.Parallel()` |
| Benchmarks | No integrado | `func Benchmark*(b *testing.B)` |
| Mocking | CMocka --wrap | Interfaces (DI natural) |

### 13.3 La leccion fundamental

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  POR QUE C TIENE TANTOS FRAMEWORKS Y RUST/GO SOLO UNO                          │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  C (1972):                                                                      │
│  • No tiene testing en el lenguaje                                              │
│  • No tiene package manager                                                     │
│  • No tiene sistema de build estandar                                           │
│  • No tiene macros hygienicas ni metaprogramacion                               │
│  → Cada proyecto reinventa testing. Decenas de frameworks.                      │
│                                                                                  │
│  Rust (2015):                                                                   │
│  • #[test] es parte del lenguaje                                                │
│  • cargo test integrado en el package manager                                   │
│  • Macros procedurales para extender (mockall, proptest)                        │
│  → Un solo framework base. Crates para funcionalidad extra.                     │
│                                                                                  │
│  Go (2009):                                                                     │
│  • testing package en la standard library                                       │
│  • go test integrado en el toolchain                                            │
│  • Filosofia de "una forma de hacerlo"                                          │
│  → Un solo framework. Punto.                                                    │
│                                                                                  │
│  Moraleja: La fragmentacion de frameworks en C no es un defecto de los         │
│  frameworks. Es una consecuencia de que C no ofrece la infraestructura         │
│  que lenguajes modernos integran de serie. Aprender a elegir y usar            │
│  frameworks C es una habilidad necesaria que Rust/Go eliminaron.               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Otros frameworks notables

Aunque la comparacion principal es Unity vs CMocka vs Check vs MinUnit, vale la pena conocer otros frameworks que pueden aparecer en proyectos reales:

### 14.1 CUnit

```
Sitio:    http://cunit.sourceforge.net/
Licencia: LGPL 2.0
Estado:   Poco mantenido (ultimo release significativo ~2014)

Estructura: Registry → Suite → Test (similar a Check pero sin fork)
API verbose: CU_add_test(pSuite, "test_name", test_function)

Problema principal:
• API compleja y verbosa comparada con alternativas modernas
• Poca actividad de mantenimiento
• No agrega nada que Unity/Check/CMocka no tengan

Uso recomendado: Solo si ya lo usas. No empezar proyectos nuevos con CUnit.
```

### 14.2 Criterion

```
Sitio:    https://github.com/Snaipe/Criterion
Licencia: MIT
Estado:   Activo

Caracteristicas unicas:
• Funciona con C Y C++
• fork() por test (como Check)
• Output bonito (colores, Unicode)
• Parametric tests nativos
• Assertions con operadores: cr_assert(x == 5)
• Theories (property-based testing ligero)
• Output TAP, XML, JSON

Problema:
• Requiere C11 o C++11
• Menos adopcion que Unity/Check/CMocka
• Mas pesado que Unity

Ejemplo:
```

```c
#include <criterion/criterion.h>

Test(stack, push_pop) {
    Stack *s = stack_new(10);
    stack_push(s, 42);
    cr_assert_eq(stack_pop(s), 42);
    stack_free(s);
}

Test(stack, empty_pop, .signal = SIGABRT) {
    // Espera que stack_pop en vacio cause abort
    Stack *s = stack_new(10);
    stack_pop(s);  // abort → test pasa
    stack_free(s);
}

// Parametric:
ParameterizedTestParameters(factorial, values) {
    static int params[] = {0, 1, 2, 3, 4, 5};
    return cr_make_param_array(int, params, 6);
}

ParameterizedTest(int *n, factorial, values) {
    int expected[] = {1, 1, 2, 6, 24, 120};
    cr_assert_eq(factorial(*n), expected[*n]);
}
```

### 14.3 greatest

```
Sitio:    https://github.com/silentbicycle/greatest
Licencia: ISC
Estado:   Estable, mantenido

Caracteristicas:
• Header-only (1 archivo)
• ~1,500 LOC
• Inspirado en JUnit
• Suites y setup/teardown
• Output TAP
• Shuffle tests
• Mas funcional que MinUnit, menos que Unity

Es una buena opcion intermedia entre MinUnit y Unity.
```

```c
#include "greatest.h"

TEST test_push(void) {
    Stack *s = stack_new(10);
    ASSERT(stack_push(s, 42));
    ASSERT_EQ(1, stack_size(s));
    stack_free(s);
    PASS();
}

SUITE(stack_suite) {
    RUN_TEST(test_push);
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(stack_suite);
    GREATEST_MAIN_END();
}
```

### 14.4 munit

```
Sitio:    https://nemequ.github.io/munit/
Licencia: MIT
Estado:   Estable pero poco activo

Caracteristicas unicas:
• Header-only (1 archivo, ~2,000 LOC)
• Tests parametrizados nativos
• Benchmarking integrado (MUNIT_ENABLE_TIMING)
• Test suites con setup/teardown
• Random seed para reproducibilidad
• Buena API

Problema: Mantenimiento esporadico.
```

### 14.5 Tabla comparativa extendida

| Framework | Tipo | Tamano | Fork | Mock | Parametric | Header-only |
|-----------|------|--------|------|------|------------|-------------|
| MinUnit | Macro | ~50 LOC | No | No | No | Si |
| greatest | Header | ~1,500 LOC | No | No | No | Si |
| munit | Header | ~2,000 LOC | No | No | Si | Si |
| Unity | Source | ~4,000 LOC | No | No (CMock sep.) | No | No |
| CMocka | Library | ~8,000 LOC | No | Si | No | No |
| Check | Library | ~15,000 LOC | Si | No | Si (loop) | No |
| Criterion | Library | ~10,000 LOC | Si | No | Si | No |
| CUnit | Library | ~5,000 LOC | No | No | No | No |

---

## 15. Ejemplo completo — el mismo proyecto con cada framework

Para consolidar la comparacion, mostramos el **CMakeLists.txt** de un proyecto que compila el mismo test con los cuatro frameworks, usando CTest como meta-runner:

### 15.1 Estructura

```
comparison/
├── CMakeLists.txt
├── include/
│   └── calculator.h
├── src/
│   └── calculator.c
├── tests/
│   ├── CMakeLists.txt
│   ├── minunit.h
│   ├── test_calc_minunit.c
│   ├── test_calc_unity.c
│   ├── test_calc_check.c
│   └── test_calc_cmocka.c
└── external/
    └── (Unity via FetchContent)
```

### 15.2 Codigo bajo test

```c
// include/calculator.h
#ifndef CALCULATOR_H
#define CALCULATOR_H

typedef enum {
    CALC_OK = 0,
    CALC_ERR_OVERFLOW,
    CALC_ERR_DIV_ZERO,
    CALC_ERR_INVALID,
} CalcError;

typedef struct {
    int last_result;
    CalcError last_error;
} Calculator;

Calculator calculator_init(void);
int calc_add(Calculator *c, int a, int b);
int calc_subtract(Calculator *c, int a, int b);
int calc_multiply(Calculator *c, int a, int b);
int calc_divide(Calculator *c, int a, int b);
CalcError calc_last_error(const Calculator *c);

#endif
```

```c
// src/calculator.c
#include "calculator.h"
#include <limits.h>

Calculator calculator_init(void) {
    return (Calculator){.last_result = 0, .last_error = CALC_OK};
}

int calc_add(Calculator *c, int a, int b) {
    // Check overflow
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        c->last_error = CALC_ERR_OVERFLOW;
        return 0;
    }
    c->last_result = a + b;
    c->last_error = CALC_OK;
    return c->last_result;
}

int calc_subtract(Calculator *c, int a, int b) {
    if ((b < 0 && a > INT_MAX + b) || (b > 0 && a < INT_MIN + b)) {
        c->last_error = CALC_ERR_OVERFLOW;
        return 0;
    }
    c->last_result = a - b;
    c->last_error = CALC_OK;
    return c->last_result;
}

int calc_multiply(Calculator *c, int a, int b) {
    if (a != 0 && b != 0) {
        if ((a > 0 && b > 0 && a > INT_MAX / b) ||
            (a < 0 && b < 0 && a < INT_MAX / b) ||
            (a > 0 && b < 0 && b < INT_MIN / a) ||
            (a < 0 && b > 0 && a < INT_MIN / b)) {
            c->last_error = CALC_ERR_OVERFLOW;
            return 0;
        }
    }
    c->last_result = a * b;
    c->last_error = CALC_OK;
    return c->last_result;
}

int calc_divide(Calculator *c, int a, int b) {
    if (b == 0) {
        c->last_error = CALC_ERR_DIV_ZERO;
        return 0;
    }
    if (a == INT_MIN && b == -1) {
        c->last_error = CALC_ERR_OVERFLOW;
        return 0;
    }
    c->last_result = a / b;
    c->last_error = CALC_OK;
    return c->last_result;
}

CalcError calc_last_error(const Calculator *c) {
    return c ? c->last_error : CALC_ERR_INVALID;
}
```

### 15.3 Test con MinUnit

```c
// tests/test_calc_minunit.c
#include "minunit.h"
#include "calculator.h"

static void test_add_basic(void) {
    Calculator c = calculator_init();
    mu_assert_int_eq(5, calc_add(&c, 2, 3));
    mu_assert_int_eq(0, calc_last_error(&c));
}

static void test_add_negative(void) {
    Calculator c = calculator_init();
    mu_assert_int_eq(-5, calc_add(&c, -2, -3));
}

static void test_subtract_basic(void) {
    Calculator c = calculator_init();
    mu_assert_int_eq(1, calc_subtract(&c, 3, 2));
}

static void test_multiply_basic(void) {
    Calculator c = calculator_init();
    mu_assert_int_eq(6, calc_multiply(&c, 2, 3));
}

static void test_divide_basic(void) {
    Calculator c = calculator_init();
    mu_assert_int_eq(5, calc_divide(&c, 10, 2));
}

static void test_divide_by_zero(void) {
    Calculator c = calculator_init();
    calc_divide(&c, 10, 0);
    mu_assert_int_eq(CALC_ERR_DIV_ZERO, calc_last_error(&c));
}

static void test_add_overflow(void) {
    Calculator c = calculator_init();
    calc_add(&c, INT_MAX, 1);
    mu_assert_int_eq(CALC_ERR_OVERFLOW, calc_last_error(&c));
}

int main(void) {
    printf("=== Calculator Tests (MinUnit) ===\n");
    mu_run_test(test_add_basic);
    mu_run_test(test_add_negative);
    mu_run_test(test_subtract_basic);
    mu_run_test(test_multiply_basic);
    mu_run_test(test_divide_basic);
    mu_run_test(test_divide_by_zero);
    mu_run_test(test_add_overflow);
    mu_report();
}
```

### 15.4 Test con Unity

```c
// tests/test_calc_unity.c
#include "unity.h"
#include "calculator.h"
#include <limits.h>

static Calculator calc;

void setUp(void) {
    calc = calculator_init();
}

void tearDown(void) {
    // Calculator es stack-allocated, nada que limpiar
}

void test_add_basic(void) {
    TEST_ASSERT_EQUAL_INT(5, calc_add(&calc, 2, 3));
    TEST_ASSERT_EQUAL_INT(CALC_OK, calc_last_error(&calc));
}

void test_add_negative(void) {
    TEST_ASSERT_EQUAL_INT(-5, calc_add(&calc, -2, -3));
}

void test_subtract_basic(void) {
    TEST_ASSERT_EQUAL_INT(1, calc_subtract(&calc, 3, 2));
}

void test_multiply_basic(void) {
    TEST_ASSERT_EQUAL_INT(6, calc_multiply(&calc, 2, 3));
}

void test_divide_basic(void) {
    TEST_ASSERT_EQUAL_INT(5, calc_divide(&calc, 10, 2));
}

void test_divide_by_zero(void) {
    calc_divide(&calc, 10, 0);
    TEST_ASSERT_EQUAL_INT(CALC_ERR_DIV_ZERO, calc_last_error(&calc));
}

void test_add_overflow(void) {
    calc_add(&calc, INT_MAX, 1);
    TEST_ASSERT_EQUAL_INT(CALC_ERR_OVERFLOW, calc_last_error(&calc));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_basic);
    RUN_TEST(test_add_negative);
    RUN_TEST(test_subtract_basic);
    RUN_TEST(test_multiply_basic);
    RUN_TEST(test_divide_basic);
    RUN_TEST(test_divide_by_zero);
    RUN_TEST(test_add_overflow);
    return UNITY_END();
}
```

### 15.5 Test con Check

```c
// tests/test_calc_check.c
#include <check.h>
#include <stdlib.h>
#include <limits.h>
#include "calculator.h"

static Calculator calc;

static void setup(void) {
    calc = calculator_init();
}

START_TEST(test_add_basic) {
    ck_assert_int_eq(calc_add(&calc, 2, 3), 5);
    ck_assert_int_eq(calc_last_error(&calc), CALC_OK);
}
END_TEST

START_TEST(test_add_negative) {
    ck_assert_int_eq(calc_add(&calc, -2, -3), -5);
}
END_TEST

START_TEST(test_subtract_basic) {
    ck_assert_int_eq(calc_subtract(&calc, 3, 2), 1);
}
END_TEST

START_TEST(test_multiply_basic) {
    ck_assert_int_eq(calc_multiply(&calc, 2, 3), 6);
}
END_TEST

START_TEST(test_divide_basic) {
    ck_assert_int_eq(calc_divide(&calc, 10, 2), 5);
}
END_TEST

START_TEST(test_divide_by_zero) {
    calc_divide(&calc, 10, 0);
    ck_assert_int_eq(calc_last_error(&calc), CALC_ERR_DIV_ZERO);
}
END_TEST

START_TEST(test_add_overflow) {
    calc_add(&calc, INT_MAX, 1);
    ck_assert_int_eq(calc_last_error(&calc), CALC_ERR_OVERFLOW);
}
END_TEST

static Suite *calculator_suite(void) {
    Suite *s = suite_create("Calculator");

    TCase *tc = tcase_create("Core");
    tcase_add_checked_fixture(tc, setup, NULL);
    tcase_add_test(tc, test_add_basic);
    tcase_add_test(tc, test_add_negative);
    tcase_add_test(tc, test_subtract_basic);
    tcase_add_test(tc, test_multiply_basic);
    tcase_add_test(tc, test_divide_basic);
    tcase_add_test(tc, test_divide_by_zero);
    tcase_add_test(tc, test_add_overflow);
    suite_add_tcase(s, tc);

    return s;
}

int main(void) {
    Suite *s = calculator_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int nfail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nfail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

### 15.6 Test con CMocka

```c
// tests/test_calc_cmocka.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <limits.h>
#include <cmocka.h>
#include "calculator.h"

static int setup(void **state) {
    Calculator *c = test_malloc(sizeof(Calculator));
    *c = calculator_init();
    *state = c;
    return 0;
}

static int teardown(void **state) {
    test_free(*state);
    return 0;
}

static void test_add_basic(void **state) {
    Calculator *c = *state;
    assert_int_equal(calc_add(c, 2, 3), 5);
    assert_int_equal(calc_last_error(c), CALC_OK);
}

static void test_add_negative(void **state) {
    Calculator *c = *state;
    assert_int_equal(calc_add(c, -2, -3), -5);
}

static void test_subtract_basic(void **state) {
    Calculator *c = *state;
    assert_int_equal(calc_subtract(c, 3, 2), 1);
}

static void test_multiply_basic(void **state) {
    Calculator *c = *state;
    assert_int_equal(calc_multiply(c, 2, 3), 6);
}

static void test_divide_basic(void **state) {
    Calculator *c = *state;
    assert_int_equal(calc_divide(c, 10, 2), 5);
}

static void test_divide_by_zero(void **state) {
    Calculator *c = *state;
    calc_divide(c, 10, 0);
    assert_int_equal(calc_last_error(c), CALC_ERR_DIV_ZERO);
}

static void test_add_overflow(void **state) {
    Calculator *c = *state;
    calc_add(c, INT_MAX, 1);
    assert_int_equal(calc_last_error(c), CALC_ERR_OVERFLOW);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_add_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_add_negative, setup, teardown),
        cmocka_unit_test_setup_teardown(test_subtract_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_multiply_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_divide_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_divide_by_zero, setup, teardown),
        cmocka_unit_test_setup_teardown(test_add_overflow, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 15.7 CMakeLists.txt unificado

```cmake
cmake_minimum_required(VERSION 3.14)
project(framework_comparison C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Libreria bajo test ---
add_library(calculator src/calculator.c)
target_include_directories(calculator PUBLIC include)

# --- Testing ---
enable_testing()

# === MinUnit (header-only, siempre disponible) ===
add_executable(test_calc_minunit tests/test_calc_minunit.c)
target_link_libraries(test_calc_minunit PRIVATE calculator)
target_include_directories(test_calc_minunit PRIVATE tests)  # para minunit.h
add_test(NAME minunit.calculator COMMAND test_calc_minunit)
set_tests_properties(minunit.calculator PROPERTIES LABELS "minunit;unit")

# === Unity (via FetchContent) ===
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

add_executable(test_calc_unity tests/test_calc_unity.c)
target_link_libraries(test_calc_unity PRIVATE calculator unity)
add_test(NAME unity.calculator COMMAND test_calc_unity)
set_tests_properties(unity.calculator PROPERTIES LABELS "unity;unit")

# === Check (via pkg-config, si instalado) ===
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CHECK QUIET check)
endif()

if(CHECK_FOUND)
    add_executable(test_calc_check tests/test_calc_check.c)
    target_link_libraries(test_calc_check PRIVATE calculator ${CHECK_LIBRARIES})
    target_include_directories(test_calc_check PRIVATE ${CHECK_INCLUDE_DIRS})
    add_test(NAME check.calculator COMMAND test_calc_check)
    set_tests_properties(check.calculator PROPERTIES LABELS "check;unit")
    message(STATUS "Check found: ${CHECK_VERSION}")
else()
    message(STATUS "Check not found, skipping Check tests")
endif()

# === CMocka (via FetchContent) ===
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG        cmocka-1.1.7
)
set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)

add_executable(test_calc_cmocka tests/test_calc_cmocka.c)
target_link_libraries(test_calc_cmocka PRIVATE calculator cmocka-static)
add_test(NAME cmocka.calculator COMMAND test_calc_cmocka)
set_tests_properties(cmocka.calculator PROPERTIES LABELS "cmocka;unit")
```

```bash
$ mkdir build && cd build && cmake ..
-- Check found: 0.15.2
-- Configuring done

$ cmake --build . -j$(nproc)

$ ctest --output-on-failure -j$(nproc)
1/4 Test #1: minunit.calculator ...............   Passed    0.00 sec
2/4 Test #2: unity.calculator .................   Passed    0.00 sec
3/4 Test #3: check.calculator .................   Passed    0.01 sec
4/4 Test #4: cmocka.calculator ................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 4

# Filtrar por framework:
$ ctest -L "check"    # Solo el test de Check
$ ctest -L "unity"    # Solo el test de Unity
$ ctest -L "cmocka"   # Solo el test de CMocka
$ ctest -L "minunit"  # Solo el test de MinUnit
```

---

## 16. Guia de adopcion rapida

### 16.1 Si empiezas de cero

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  RECOMENDACION PARA NUEVOS PROYECTOS                                            │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Proyecto personal / aprendizaje:                                               │
│  ──▶ Empezar con MinUnit o assert.h                                             │
│  ──▶ Migrar a Unity cuando necesites mas estructura                             │
│                                                                                  │
│  Libreria open source:                                                          │
│  ──▶ Unity (facil para contribuidores, buenas assertions, MIT)                  │
│  ──▶ CTest como runner si usas CMake                                            │
│                                                                                  │
│  Aplicacion con dependencias externas:                                          │
│  ──▶ CMocka (necesitaras mocking mas temprano que tarde)                        │
│                                                                                  │
│  Sistema critico / parser / networking:                                         │
│  ──▶ Check (fork protege contra crashes)                                        │
│  ──▶ CMocka para los tests de mocking                                           │
│                                                                                  │
│  Embedded / microcontrolador:                                                   │
│  ──▶ Unity (C89, sin dependencias de OS)                                        │
│  ──▶ Ceedling para build automation                                             │
│                                                                                  │
│  Si solo puedes aprender UNO:                                                   │
│  ──▶ Unity. Es el mas versatil y el conocimiento se transfiere bien.            │
│                                                                                  │
│  Si puedes aprender DOS:                                                        │
│  ──▶ Unity + CMocka. Cubren el 95% de los casos de uso.                        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 16.2 Checklist de adopcion

```
[ ] 1. Elegir framework basado en criterios (seccion 8)
[ ] 2. Instalar / incluir en el proyecto
[ ] 3. Crear primer test (un solo assert, verificar que compila y ejecuta)
[ ] 4. Integrar con build system (Make o CMake)
[ ] 5. Crear 3-5 tests para un modulo existente
[ ] 6. Agregar setUp/tearDown si hay estado compartido
[ ] 7. Configurar CTest o Make target para `make test`
[ ] 8. Agregar a CI (GitHub Actions, GitLab CI)
[ ] 9. Documentar en README como correr los tests
[ ] 10. Agregar sanitizers (ASan, UBSan) al pipeline de tests
```

---

## 17. Programa de practica — evaluador de frameworks

### 17.1 Requisitos

Implementa un sistema de **key-value store** con las siguientes operaciones:

```c
// kvstore.h
typedef struct KVStore KVStore;

KVStore *kvstore_new(int capacity);
void kvstore_free(KVStore *store);
bool kvstore_set(KVStore *store, const char *key, const char *value);
const char *kvstore_get(const KVStore *store, const char *key);
bool kvstore_delete(KVStore *store, const char *key);
int kvstore_count(const KVStore *store);
bool kvstore_has(const KVStore *store, const char *key);
int kvstore_save(const KVStore *store, const char *filepath);
KVStore *kvstore_load(const char *filepath);
```

### 17.2 Lo que debes hacer

Escribe los **mismos tests** para kvstore usando **los cuatro frameworks** (MinUnit, Unity, Check, CMocka), siguiendo el patron de la seccion 15.

Tests minimos por framework (10 tests):

1. `test_new_store_is_empty` — count == 0
2. `test_set_and_get` — set "key"="value", get "key" == "value"
3. `test_set_overwrite` — set "k"="v1", set "k"="v2", get "k" == "v2"
4. `test_get_nonexistent_returns_null` — get "nope" == NULL
5. `test_delete_existing` — set + delete, has == false
6. `test_delete_nonexistent_returns_false`
7. `test_has_existing` — set, has == true
8. `test_has_nonexistent` — has == false
9. `test_capacity_limit` — llenar store, siguiente set retorna false
10. `test_save_and_load` — set 3 keys, save, load, verificar

Despues de implementar los cuatro archivos de test, documenta en un comentario al final de cada archivo:

```c
/*
 * Observaciones sobre [framework]:
 * - Lineas de boilerplate: NN
 * - Lineas de logica de test: NN
 * - ¿El cleanup se ejecuta si un assert falla? SI/NO
 * - ¿Puedo tener fixtures diferentes por test? SI/NO
 * - Dificultad subjetiva (1-5): N
 * - Lo que mas me gusto: ...
 * - Lo que menos me gusto: ...
 */
```

### 17.3 CMakeLists.txt

Crea un CMakeLists.txt como el de la seccion 15.7 que compile los cuatro tests y los registre en CTest con labels por framework. Verifica:

```bash
ctest -N                  # Debe mostrar 4 tests
ctest --output-on-failure # Debe pasar todo
ctest -L "minunit"        # Solo MinUnit
ctest -L "unity"          # Solo Unity
ctest -L "check"          # Solo Check (si instalado)
ctest -L "cmocka"         # Solo CMocka
```

---

## 18. Ejercicios

### Ejercicio 1: Migracion MinUnit → Unity

Toma el archivo `test_calc_minunit.c` de la seccion 15.3 y migralo manualmente a Unity. Documenta cada cambio que haces y por que. Compara los mensajes de error cuando un assert falla: ejecuta ambas versiones con un valor incorrecto forzado y compara la utilidad del output de error.

### Ejercicio 2: Check loop tests

Implementa tests parametrizados para una funcion `int collatz_steps(int n)` (numero de pasos para llegar a 1 en la conjetura de Collatz) usando `tcase_add_loop_test` de Check. La tabla de valores debe incluir al menos 10 entradas. Compara la ergonomia con hacer lo mismo en CMocka (sin loop nativo) y en Unity.

### Ejercicio 3: Evaluacion de crash safety

Crea un programa que deliberadamente hace:
1. Acceso a puntero NULL (SIGSEGV)
2. Division por cero (SIGFPE)
3. Escritura fuera de buffer (detectado por ASan)

Escribe un test para cada caso en los cuatro frameworks. Documenta cual framework:
- Sobrevive al crash y ejecuta los tests restantes
- Muere en el primer crash
- Detecta el error por output (FAIL_REGULAR_EXPRESSION con ASan)

### Ejercicio 4: Decision justificada

Elige un proyecto personal o imaginario (ej: servidor HTTP en C, compilador de un lenguaje, driver de dispositivo, libreria de graficos). Escribe un documento de 1 pagina justificando que framework(s) usarias, basandote en:
- Tipo de proyecto (libreria, aplicacion, embedded)
- Plataformas target
- Necesidad de mocking
- Probabilidad de crashes durante desarrollo
- Requisitos de CI
- Licencia del proyecto

---

Navegacion:

- Anterior: [T03 — CTest con CMake](../T03_CTest_con_CMake/README.md)
- Siguiente: [S03/T01 — Que es un test double](../../S03_Mocking_y_Test_Doubles_en_C/T01_Que_es_un_test_double/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Fin de la Seccion 2 — Frameworks de Testing en C*

*Proximo topico: S03/T01 — Que es un test double*
