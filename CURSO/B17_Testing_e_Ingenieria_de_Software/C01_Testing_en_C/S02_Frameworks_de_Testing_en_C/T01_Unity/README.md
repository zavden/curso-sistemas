# Unity — instalacion, TEST_ASSERT macros, test runners, setUp/tearDown, integracion con Make

## 1. Introduccion

En la Seccion 1 construimos testing desde cero: macros caseras, goto cleanup, runners manuales, setjmp/longjmp. Ahora damos el salto a **Unity** — el framework de testing para C mas utilizado en la industria, especialmente en sistemas embebidos.

Unity no es el motor de videojuegos (ese es Unity3D/Unity Technologies). Unity Test Framework es un proyecto open-source creado por **ThrowTheSwitch.org**, especificamente diseñado para C puro (no C++). Es un solo archivo `.c` + un `.h` — no tiene dependencias externas, no necesita librerias compartidas, y funciona en cualquier plataforma que tenga un compilador de C.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         UNITY TEST FRAMEWORK                                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Autor:        ThrowTheSwitch.org (Mark VanderVoord et al.)                     │
│  Licencia:     MIT                                                              │
│  Repositorio:  https://github.com/ThrowTheSwitch/Unity                          │
│  Lenguaje:     C puro (C89 compatible, funciona con C99/C11/C17)                │
│  Archivos:     unity.c + unity.h + unity_internals.h (3 archivos)               │
│  Dependencias: Ninguna (ni siquiera la stdlib es obligatoria en embedded)       │
│                                                                                  │
│  Ecosistema ThrowTheSwitch:                                                     │
│  ┌───────────┐    ┌───────────┐    ┌───────────┐                                │
│  │  Unity    │    │  CMock    │    │  Ceedling │                                │
│  │ (testing) │◄───│ (mocking) │◄───│ (build)   │                                │
│  └───────────┘    └───────────┘    └───────────┘                                │
│  Unit tests       Mock generation  Build system                                 │
│  Assertions       from headers     Ruby-based                                   │
│  Test runner                       Integrates all                               │
│                                                                                  │
│  Popularidad:                                                                   │
│  • ~4000 stars en GitHub                                                        │
│  • Usado en embedded: STM32, ESP32, Arduino, automotive, medical devices        │
│  • Referencia en libros: "Test-Driven Development for Embedded C" (J. Grenning) │
│  • Alternativa principal a CMocka para testing en C puro                        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Por que Unity y no nuestras macros caseras

| Aspecto | Macros caseras (S01) | Unity |
|---------|---------------------|-------|
| **Assertions** | 3-5 macros basicas (CHECK, CHECK_EQ, CHECK_STR_EQ) | 80+ macros tipadas (INT, UINT, HEX, FLOAT, DOUBLE, STRING, ARRAY, MEMORY...) |
| **Mensajes de error** | "expected X, got Y" | Tipo, valor esperado, valor obtenido, archivo, linea, mensaje custom |
| **Aislamiento** | Manual (goto, setjmp, fork) | setjmp/longjmp interno — tearDown se llama siempre |
| **setUp/tearDown** | Manual (llamar al inicio/fin de cada test) | Automatico — Unity los llama por ti |
| **Test discovery** | Manual (listar en main) | Semiautomatico (generate_test_runner.rb o manual) |
| **Output formats** | printf custom | Formato estandarizado, parseable por CI/CD |
| **Portabilidad** | Depende de tus includes | Funciona en bare-metal sin stdlib |
| **Mantenimiento** | Tu lo mantienes | Comunidad activa, 10+ años de desarrollo |

### 1.2 Que NO es Unity

- **No es un framework de mocking** — para mocking usa CMock (del mismo ecosistema) o CMocka
- **No es un build system** — para eso esta Ceedling (Ruby-based) o Make/CMake
- **No tiene test discovery automatico** — necesitas generar el runner o escribirlo manualmente
- **No es para C++** — para C++ usa Google Test, Catch2, o doctest

---

## 2. Instalacion

### 2.1 Metodo 1: Copiar los archivos (recomendado para proyectos simples)

Unity son solo 3 archivos. La forma mas simple es copiarlos directamente a tu proyecto:

```bash
# Clonar el repositorio
$ git clone https://github.com/ThrowTheSwitch/Unity.git /tmp/unity

# Los 3 archivos que necesitas estan en src/
$ ls /tmp/unity/src/
unity.c  unity.h  unity_internals.h

# Copiarlos a tu proyecto
$ mkdir -p vendor/unity
$ cp /tmp/unity/src/unity.c vendor/unity/
$ cp /tmp/unity/src/unity.h vendor/unity/
$ cp /tmp/unity/src/unity_internals.h vendor/unity/
```

Estructura resultante:

```
proyecto/
├── src/
│   ├── math.c
│   ├── math.h
│   ├── stack.c
│   └── stack.h
├── tests/
│   ├── test_math.c
│   └── test_stack.c
├── vendor/
│   └── unity/
│       ├── unity.c
│       ├── unity.h
│       └── unity_internals.h
├── Makefile
└── README.md
```

### 2.2 Metodo 2: Git submodule

```bash
$ git submodule add https://github.com/ThrowTheSwitch/Unity.git vendor/unity
```

### 2.3 Metodo 3: Gestor de paquetes del sistema

```bash
# En algunas distribuciones (no todas)
$ sudo apt install libunity-dev    # Debian/Ubuntu (si esta disponible)

# Mas comun: usar el source directamente
```

### 2.4 Metodo 4: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0
)
FetchContent_MakeAvailable(unity)
```

### 2.5 Verificar la instalacion

```c
// test_sanity.c — verificar que Unity funciona
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_sanity(void) {
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sanity);
    return UNITY_END();
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -Ivendor/unity \
      -o test_sanity test_sanity.c vendor/unity/unity.c
$ ./test_sanity
test_sanity.c:8:test_sanity:PASS

-----------------------
1 Tests 0 Failures 0 Ignored
OK
```

Si ves este output, Unity esta funcionando.

---

## 3. Anatomia de un test con Unity

### 3.1 Estructura minima

Todo archivo de test con Unity necesita:

```c
#include "unity.h"

// 1. setUp — se llama ANTES de cada test
void setUp(void) {
    // Inicializar fixtures, allocar memoria, abrir archivos
}

// 2. tearDown — se llama DESPUES de cada test (incluso si falla)
void tearDown(void) {
    // Liberar memoria, cerrar archivos, limpiar estado
}

// 3. Tests — funciones void sin argumentos, nombre empieza con test_
void test_something(void) {
    TEST_ASSERT_EQUAL_INT(42, compute());
}

void test_another_thing(void) {
    TEST_ASSERT_TRUE(is_valid("input"));
}

// 4. main — el runner
int main(void) {
    UNITY_BEGIN();           // Inicializar Unity
    RUN_TEST(test_something);
    RUN_TEST(test_another_thing);
    return UNITY_END();      // Imprimir resumen y retornar exit code
}
```

### 3.2 Flujo de ejecucion

```
┌─────────────────────────────────────────────────────────────────┐
│                FLUJO DE EJECUCION DE UNITY                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  main()                                                        │
│  │                                                              │
│  ├─ UNITY_BEGIN()          ← Resetear contadores                │
│  │                                                              │
│  ├─ RUN_TEST(test_something)                                   │
│  │   │                                                          │
│  │   ├─ setUp()            ← Inicializar fixture               │
│  │   │                                                          │
│  │   ├─ test_something()   ← Ejecutar test                     │
│  │   │   │                                                      │
│  │   │   ├─ TEST_ASSERT... PASS → continua                     │
│  │   │   │                                                      │
│  │   │   └─ TEST_ASSERT... FAIL → longjmp al runner            │
│  │   │                          (NO ejecuta el resto del test)  │
│  │   │                                                          │
│  │   └─ tearDown()         ← Limpiar (SIEMPRE se ejecuta)      │
│  │                                                              │
│  ├─ RUN_TEST(test_another_thing)                               │
│  │   │                                                          │
│  │   ├─ setUp()                                                │
│  │   ├─ test_another_thing()                                   │
│  │   └─ tearDown()                                             │
│  │                                                              │
│  └─ UNITY_END()            ← Imprimir resumen, retornar        │
│      │                        0 si todo OK, != 0 si hubo fallos │
│      └─ return al shell                                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Que hacen UNITY_BEGIN, RUN_TEST, y UNITY_END

| Macro | Que hace |
|-------|----------|
| `UNITY_BEGIN()` | Resetea contadores internos (tests ejecutados, fallos, ignorados). Debe llamarse una vez al inicio. |
| `RUN_TEST(test_fn)` | Llama a setUp(), ejecuta test_fn (con setjmp para capturar fallos), llama a tearDown(). Registra el resultado. |
| `UNITY_END()` | Imprime el resumen ("X Tests Y Failures Z Ignored"), retorna el numero de fallos (0 = todo OK). |

### 3.4 setUp y tearDown son obligatorios

Unity **requiere** que definas setUp y tearDown, incluso si estan vacias. Si no las defines, tendras un error de linkeo:

```
undefined reference to `setUp'
undefined reference to `tearDown'
```

Si no necesitas fixture, simplemente dejalas vacias:

```c
void setUp(void) {}
void tearDown(void) {}
```

---

## 4. TEST_ASSERT macros — la referencia completa

Unity tiene mas de 80 macros de asercion. Estan organizadas por tipo de dato.

### 4.1 Assertions booleanas y generales

```c
// Booleanas
TEST_ASSERT(condition);              // Falla si condition es 0 (false)
TEST_ASSERT_TRUE(condition);         // Igual que TEST_ASSERT
TEST_ASSERT_FALSE(condition);        // Falla si condition es != 0 (true)
TEST_ASSERT_NULL(pointer);           // Falla si pointer != NULL
TEST_ASSERT_NOT_NULL(pointer);       // Falla si pointer == NULL

// Ejemplo
void test_stack_new_returns_non_null(void) {
    Stack *s = stack_new(4);
    TEST_ASSERT_NOT_NULL(s);
    stack_free(s);
}

void test_stack_pop_empty_returns_false(void) {
    Stack *s = stack_new(4);
    int val;
    TEST_ASSERT_FALSE(stack_pop(s, &val));
    stack_free(s);
}
```

### 4.2 Assertions de enteros

```c
// Igualdad (con formato segun tipo)
TEST_ASSERT_EQUAL_INT(expected, actual);        // int
TEST_ASSERT_EQUAL_INT8(expected, actual);       // int8_t
TEST_ASSERT_EQUAL_INT16(expected, actual);      // int16_t
TEST_ASSERT_EQUAL_INT32(expected, actual);      // int32_t
TEST_ASSERT_EQUAL_INT64(expected, actual);      // int64_t
TEST_ASSERT_EQUAL_UINT(expected, actual);       // unsigned int
TEST_ASSERT_EQUAL_UINT8(expected, actual);      // uint8_t
TEST_ASSERT_EQUAL_UINT16(expected, actual);     // uint16_t
TEST_ASSERT_EQUAL_UINT32(expected, actual);     // uint32_t
TEST_ASSERT_EQUAL_UINT64(expected, actual);     // uint64_t
TEST_ASSERT_EQUAL_size_t(expected, actual);     // size_t
TEST_ASSERT_EQUAL_HEX(expected, actual);        // unsigned, display en hex
TEST_ASSERT_EQUAL_HEX8(expected, actual);       // uint8_t, display en hex
TEST_ASSERT_EQUAL_HEX16(expected, actual);      // uint16_t, display en hex
TEST_ASSERT_EQUAL_HEX32(expected, actual);      // uint32_t, display en hex

// Ejemplo
void test_stack_len_after_push(void) {
    Stack *s = stack_new(4);
    stack_push(s, 42);
    TEST_ASSERT_EQUAL_size_t(1, stack_len(s));  // size_t comparison
    stack_free(s);
}

void test_packet_header(void) {
    uint8_t header = build_header(0x0A, 0x03);
    TEST_ASSERT_EQUAL_HEX8(0xA3, header);  // Muestra 0xA3 vs 0x?? en hex
}
```

**Por que importa el tipo**: cuando un test falla, Unity formatea el mensaje segun el tipo. Un `EQUAL_INT` muestra decimales, un `EQUAL_HEX8` muestra hexadecimal con 2 digitos. Usar el tipo correcto hace que los mensajes de error sean mas utiles:

```
// Con EQUAL_INT:
test.c:15:test_packet:FAIL: Expected 163 Was 160
// ¿163? ¿160? No intuitivo para un byte de un protocolo binario.

// Con EQUAL_HEX8:
test.c:15:test_packet:FAIL: Expected 0xA3 Was 0xA0
// Mucho mas claro — el nibble bajo es 0 en vez de 3.
```

### 4.3 Assertions de comparacion

```c
// Mayor/menor que
TEST_ASSERT_GREATER_THAN(threshold, actual);        // actual > threshold
TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual);    // actual >= threshold
TEST_ASSERT_LESS_THAN(threshold, actual);           // actual < threshold
TEST_ASSERT_LESS_OR_EQUAL(threshold, actual);       // actual <= threshold
TEST_ASSERT_INT_WITHIN(delta, expected, actual);    // |actual - expected| <= delta

// Ejemplo
void test_temperature_in_range(void) {
    int temp = read_sensor();
    TEST_ASSERT_GREATER_OR_EQUAL(0, temp);     // temp >= 0
    TEST_ASSERT_LESS_THAN(100, temp);           // temp < 100
}

void test_adc_reading_within_tolerance(void) {
    int reading = adc_read(CHANNEL_0);
    TEST_ASSERT_INT_WITHIN(5, 512, reading);   // 507 <= reading <= 517
}
```

### 4.4 Assertions de flotantes

```c
// Igualdad con delta (epsilon)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual);
TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual);

// Igualdad exacta (raro — floating point no es exacto)
TEST_ASSERT_EQUAL_FLOAT(expected, actual);
TEST_ASSERT_EQUAL_DOUBLE(expected, actual);

// Especiales
TEST_ASSERT_FLOAT_IS_INF(value);              // +inf o -inf
TEST_ASSERT_FLOAT_IS_NOT_INF(value);
TEST_ASSERT_FLOAT_IS_NAN(value);              // NaN
TEST_ASSERT_FLOAT_IS_NOT_NAN(value);
TEST_ASSERT_FLOAT_IS_DETERMINATE(value);      // No es NaN ni inf

// Ejemplo
void test_circle_area(void) {
    double area = circle_area(5.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 78.54, area);  // pi * 25 ≈ 78.54
}

void test_division_by_zero_returns_inf(void) {
    float result = safe_divide(1.0f, 0.0f);
    TEST_ASSERT_FLOAT_IS_INF(result);
}
```

### 4.5 Assertions de strings

```c
TEST_ASSERT_EQUAL_STRING(expected, actual);
TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len);  // Solo primeros len chars

// Ejemplo
void test_format_name(void) {
    char buf[64];
    format_name(buf, sizeof(buf), "John", "Doe");
    TEST_ASSERT_EQUAL_STRING("Doe, John", buf);
}

void test_prefix_match(void) {
    const char *line = "ERROR: disk full";
    TEST_ASSERT_EQUAL_STRING_LEN("ERROR", line, 5);
}
```

### 4.6 Assertions de memoria

```c
TEST_ASSERT_EQUAL_MEMORY(expected, actual, len);   // memcmp de len bytes

// Ejemplo
void test_serialize_packet(void) {
    uint8_t expected[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t buf[4];
    serialize_packet(buf, &packet);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, 4);
}
```

### 4.7 Assertions de arrays

```c
// Arrays de enteros
TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements);
TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements);
TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, actual, num_elements);
// ... (existe para todos los tipos de entero)

// Arrays de strings
TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements);

// Arrays de punteros
TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num_elements);

// Ejemplo
void test_sort(void) {
    int input[] = {5, 3, 1, 4, 2};
    int expected[] = {1, 2, 3, 4, 5};

    sort(input, 5);

    TEST_ASSERT_EQUAL_INT_ARRAY(expected, input, 5);
}

void test_tokenize(void) {
    char *tokens[4];
    int count = tokenize("a,b,c", ',', tokens, 4);

    TEST_ASSERT_EQUAL_INT(3, count);

    const char *expected[] = {"a", "b", "c"};
    TEST_ASSERT_EQUAL_STRING_ARRAY(expected, tokens, 3);
}
```

Cuando un array assertion falla, Unity muestra **que elemento** fallo:

```
test.c:12:test_sort:FAIL: Element 2 Expected 3 Was 4
```

### 4.8 Assertions con mensaje custom

Todas las macros tienen una variante `_MESSAGE` que acepta un string adicional:

```c
TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, "mensaje");
TEST_ASSERT_TRUE_MESSAGE(condition, "mensaje");
TEST_ASSERT_NULL_MESSAGE(ptr, "mensaje");
// ... etc para todas las macros

// Ejemplo
void test_config_parse(void) {
    Config cfg;
    int rc = config_parse("test.ini", &cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "config_parse should succeed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8080, cfg.port,
        "default port should be 8080");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("localhost", cfg.host,
        "default host should be localhost");
}
```

Output en caso de fallo:

```
test.c:6:test_config_parse:FAIL: default port should be 8080. Expected 8080 Was 3000
```

### 4.9 TEST_IGNORE y TEST_PASS/TEST_FAIL

```c
// Ignorar un test (placeholder, WIP, deshabilitado temporalmente)
void test_feature_not_implemented_yet(void) {
    TEST_IGNORE();                     // Marca como "ignored" y salta
}

void test_skip_on_ci(void) {
    if (getenv("CI") != NULL) {
        TEST_IGNORE_MESSAGE("Skipped in CI - requires hardware");
    }
    // ... test que necesita hardware real ...
}

// Forzar PASS o FAIL incondicional
void test_always_passes(void) {
    TEST_PASS();                       // Marca como passed
}

void test_known_broken(void) {
    TEST_FAIL_MESSAGE("BUG-123: fix pending, do not remove this test");
}
```

```bash
$ ./test_suite
test.c:5:test_feature_not_implemented_yet:IGNORE
test.c:9:test_skip_on_ci:IGNORE: Skipped in CI - requires hardware
test.c:14:test_always_passes:PASS
test.c:18:test_known_broken:FAIL: BUG-123: fix pending, do not remove this test

-----------------------
4 Tests 1 Failures 2 Ignored
FAIL
```

### 4.10 Tabla de referencia rapida

| Que verificar | Macro |
|---------------|-------|
| Condicion booleana | `TEST_ASSERT_TRUE(x)` / `TEST_ASSERT_FALSE(x)` |
| Puntero NULL | `TEST_ASSERT_NULL(p)` / `TEST_ASSERT_NOT_NULL(p)` |
| Enteros iguales | `TEST_ASSERT_EQUAL_INT(exp, act)` |
| Enteros sin signo | `TEST_ASSERT_EQUAL_UINT(exp, act)` |
| Hex | `TEST_ASSERT_EQUAL_HEX8(exp, act)` |
| size_t | `TEST_ASSERT_EQUAL_size_t(exp, act)` |
| Rango de entero | `TEST_ASSERT_INT_WITHIN(delta, exp, act)` |
| Mayor/menor | `TEST_ASSERT_GREATER_THAN(threshold, act)` |
| Float con epsilon | `TEST_ASSERT_FLOAT_WITHIN(delta, exp, act)` |
| String | `TEST_ASSERT_EQUAL_STRING(exp, act)` |
| Memoria (bytes) | `TEST_ASSERT_EQUAL_MEMORY(exp, act, len)` |
| Array de enteros | `TEST_ASSERT_EQUAL_INT_ARRAY(exp, act, n)` |
| Ignorar test | `TEST_IGNORE()` / `TEST_IGNORE_MESSAGE("why")` |
| Fallo forzado | `TEST_FAIL_MESSAGE("reason")` |

---

## 5. setUp y tearDown — fixtures en Unity

### 5.1 Patron basico

```c
#include "unity.h"
#include "stack.h"

// Variable global para la fixture
// (Unity no tiene mecanismo para pasar parametros a setUp/test/tearDown)
static Stack *s = NULL;

void setUp(void) {
    s = stack_new(4);
}

void tearDown(void) {
    stack_free(s);
    s = NULL;
}

void test_stack_is_empty_initially(void) {
    TEST_ASSERT_TRUE(stack_is_empty(s));
    TEST_ASSERT_EQUAL_size_t(0, stack_len(s));
}

void test_stack_push_makes_non_empty(void) {
    stack_push(s, 42);
    TEST_ASSERT_FALSE(stack_is_empty(s));
}

void test_stack_push_pop_lifo(void) {
    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);

    int val;
    stack_pop(s, &val);
    TEST_ASSERT_EQUAL_INT(30, val);

    stack_pop(s, &val);
    TEST_ASSERT_EQUAL_INT(20, val);

    stack_pop(s, &val);
    TEST_ASSERT_EQUAL_INT(10, val);
}

void test_stack_pop_empty_returns_false(void) {
    int val;
    TEST_ASSERT_FALSE(stack_pop(s, &val));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stack_is_empty_initially);
    RUN_TEST(test_stack_push_makes_non_empty);
    RUN_TEST(test_stack_push_pop_lifo);
    RUN_TEST(test_stack_pop_empty_returns_false);
    return UNITY_END();
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -Ivendor/unity -Isrc \
      -o test_stack tests/test_stack.c src/stack.c vendor/unity/unity.c
$ ./test_stack
test_stack.c:14:test_stack_is_empty_initially:PASS
test_stack.c:19:test_stack_push_makes_non_empty:PASS
test_stack.c:23:test_stack_push_pop_lifo:PASS
test_stack.c:37:test_stack_pop_empty_returns_false:PASS

-----------------------
4 Tests 0 Failures 0 Ignored
OK
```

### 5.2 El orden exacto de setUp/tearDown

```
Para RUN_TEST(test_A):
  1. setUp()      ← s = stack_new(4)
  2. test_A()     ← usa s
  3. tearDown()   ← stack_free(s); s = NULL

Para RUN_TEST(test_B):
  1. setUp()      ← s = stack_new(4) — NUEVA instancia
  2. test_B()     ← usa s (fresco, no afectado por test_A)
  3. tearDown()   ← stack_free(s); s = NULL
```

Esto garantiza que cada test tiene un stack **nuevo y vacio**. Si test_A hace push, eso no afecta a test_B.

### 5.3 tearDown se ejecuta incluso si el test falla

```c
void test_that_will_fail(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL_INT(99, stack_len(s));  // FALLA — len es 1, no 99
    // Esta linea nunca se ejecuta (Unity hace longjmp)
    stack_push(s, 100);
}

// Pero tearDown SI se ejecuta — stack_free(s) se llama
// No hay memory leak
```

Internamente, RUN_TEST hace algo como:

```c
// Pseudocodigo de RUN_TEST (simplificado)
#define RUN_TEST(func) \
    do { \
        setUp(); \
        if (setjmp(unity_jmp_buf) == 0) { \
            func(); \
        } \
        tearDown(); /* SIEMPRE */ \
        print_result(); \
    } while (0)
```

### 5.4 Multiples fixtures — el problema de Unity

Unity tiene **un solo par** setUp/tearDown por archivo. Si necesitas diferentes fixtures para diferentes grupos de tests, tienes varias opciones:

**Opcion A: Archivos separados por fixture**

```
tests/
├── test_stack_basic.c      ← setUp crea stack vacio
├── test_stack_populated.c  ← setUp crea stack con datos
└── test_stack_stress.c     ← setUp crea stack grande
```

**Opcion B: Variable global que indica la configuracion**

```c
static Stack *s = NULL;
static int setup_mode = 0;  // 0 = empty, 1 = populated

void setUp(void) {
    if (setup_mode == 0) {
        s = stack_new(4);
    } else {
        s = stack_new(4);
        stack_push(s, 10);
        stack_push(s, 20);
        stack_push(s, 30);
    }
}

void tearDown(void) {
    stack_free(s);
    s = NULL;
}

// Tests con stack vacio
void test_empty_is_empty(void) {
    TEST_ASSERT_TRUE(stack_is_empty(s));
}

// Tests con stack pre-poblado
void test_populated_len(void) {
    TEST_ASSERT_EQUAL_size_t(3, stack_len(s));
}

int main(void) {
    UNITY_BEGIN();

    setup_mode = 0;  // empty stack
    RUN_TEST(test_empty_is_empty);

    setup_mode = 1;  // populated stack
    RUN_TEST(test_populated_len);

    return UNITY_END();
}
```

Esto funciona pero es fragil — si olvidas cambiar `setup_mode`, el test usa la fixture incorrecta. La opcion A (archivos separados) es mas limpia.

**Opcion C: Crear la fixture dentro del test (ignorar setUp)**

```c
void setUp(void) {}   // Vacio
void tearDown(void) {} // Vacio

void test_stack_with_specific_setup(void) {
    // Fixture local — no usa setUp/tearDown
    Stack *s = stack_new(4);
    stack_push(s, 42);

    TEST_ASSERT_EQUAL_INT(42, stack_peek(s));

    stack_free(s);
    // Riesgo: si TEST_ASSERT falla, stack_free no se llama
    // Pero Unity no leakea mucho en tests cortos
}
```

---

## 6. Test runners

### 6.1 Runner manual (lo mas comun)

El runner manual es simplemente el `main()` que escribes tu:

```c
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a);
    RUN_TEST(test_b);
    RUN_TEST(test_c);
    return UNITY_END();
}
```

Desventaja: cada vez que agregas un test, debes agregarlo a main(). Si olvidas, el test existe pero nunca se ejecuta.

### 6.2 Runner generado automaticamente

Unity incluye un script Ruby (`generate_test_runner.rb`) que parsea tu archivo de test y genera el runner:

```bash
# Generar el runner
$ ruby vendor/unity/auto/generate_test_runner.rb tests/test_stack.c tests/test_stack_runner.c
```

El script busca funciones que empiecen con `test_` o `spec_` y genera un main() que las llama.

Si no quieres depender de Ruby, puedes usar un script de shell simple:

```bash
#!/bin/bash
# generate_runner.sh — genera runner basico sin Ruby
FILE=$1
OUTPUT=${2:-${FILE%.c}_runner.c}

echo '#include "unity.h"' > "$OUTPUT"
echo "" >> "$OUTPUT"

# Extraer prototipos de test functions
grep -E '^void (test_|spec_)' "$FILE" | sed 's/{$/;/' >> "$OUTPUT"
echo "" >> "$OUTPUT"

# Extraer setUp y tearDown
echo 'extern void setUp(void);' >> "$OUTPUT"
echo 'extern void tearDown(void);' >> "$OUTPUT"
echo "" >> "$OUTPUT"

echo 'int main(void) {' >> "$OUTPUT"
echo '    UNITY_BEGIN();' >> "$OUTPUT"

# Generar RUN_TEST para cada test
grep -oE '(test_|spec_)[a-zA-Z0-9_]+' "$FILE" | sort -u | while read -r name; do
    echo "    RUN_TEST($name);" >> "$OUTPUT"
done

echo '    return UNITY_END();' >> "$OUTPUT"
echo '}' >> "$OUTPUT"

echo "Generated $OUTPUT"
```

```bash
$ bash generate_runner.sh tests/test_stack.c
Generated tests/test_stack_runner.c
```

### 6.3 Patron: test file sin main, runner separado

```c
// tests/test_stack.c — NO tiene main()
#include "unity.h"
#include "stack.h"

static Stack *s;

void setUp(void) { s = stack_new(4); }
void tearDown(void) { stack_free(s); s = NULL; }

void test_stack_push(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL_size_t(1, stack_len(s));
}

void test_stack_pop_lifo(void) {
    stack_push(s, 10);
    stack_push(s, 20);
    int val;
    stack_pop(s, &val);
    TEST_ASSERT_EQUAL_INT(20, val);
}
```

```c
// tests/test_stack_runner.c — generado o escrito manualmente
#include "unity.h"

extern void setUp(void);
extern void tearDown(void);
extern void test_stack_push(void);
extern void test_stack_pop_lifo(void);

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stack_push);
    RUN_TEST(test_stack_pop_lifo);
    return UNITY_END();
}
```

```bash
$ gcc -Ivendor/unity -Isrc -o test_stack \
      tests/test_stack.c tests/test_stack_runner.c \
      src/stack.c vendor/unity/unity.c
```

---

## 7. Integracion con Make

### 7.1 Makefile basico

```makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -g
SRCDIR   = src
TESTDIR  = tests
UNITY    = vendor/unity
BUILDDIR = build

# Includes
INCLUDES = -I$(SRCDIR) -I$(UNITY)

# Archivos fuente de produccion (sin main.c)
PROD_SRC = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
PROD_OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(PROD_SRC))

# Unity
UNITY_OBJ = $(BUILDDIR)/unity.o

# Tests — cada test_*.c produce un ejecutable
TEST_SRC  = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/test_%.c, $(BUILDDIR)/test_%, $(TEST_SRC))

# ── Reglas ───────────────────────────────────────────────────

.PHONY: all test test-verbose clean

all: $(BUILDDIR)/program

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Produccion
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILDDIR)/program: $(SRCDIR)/main.c $(PROD_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Unity object
$(BUILDDIR)/unity.o: $(UNITY)/unity.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(UNITY) -c -o $@ $<

# Tests
$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(PROD_OBJ) $(UNITY_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# ── Test runners ─────────────────────────────────────────────

test: $(TEST_BINS)
	@pass=0; fail=0; ignore=0; \
	for t in $(TEST_BINS); do \
		name=$$(basename $$t); \
		output=$$(./$$t 2>&1); \
		rc=$$?; \
		if [ $$rc -eq 0 ]; then \
			printf "  \033[32m[PASS]\033[0m %s\n" "$$name"; \
			pass=$$((pass + 1)); \
		else \
			printf "  \033[31m[FAIL]\033[0m %s\n" "$$name"; \
			echo "$$output" | grep FAIL; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Suites: $$pass passed, $$fail failed"; \
	[ $$fail -eq 0 ]

test-verbose: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "═══ $$(basename $$t) ═══"; \
		./$$t; \
		echo ""; \
	done

# Test con sanitizers
test-asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
test-asan: clean test

test-ubsan: CFLAGS += -fsanitize=undefined
test-ubsan: clean test

clean:
	rm -rf $(BUILDDIR)
```

```bash
$ make test
  [PASS] test_stack
  [PASS] test_math
  [FAIL] test_parser

Suites: 2 passed, 1 failed

$ make test-verbose
═══ test_stack ═══
test_stack.c:14:test_stack_is_empty_initially:PASS
test_stack.c:19:test_stack_push_makes_non_empty:PASS
test_stack.c:23:test_stack_push_pop_lifo:PASS
test_stack.c:37:test_stack_pop_empty:PASS

-----------------------
4 Tests 0 Failures 0 Ignored
OK

═══ test_math ═══
...

═══ test_parser ═══
test_parser.c:28:test_parse_invalid:FAIL: Expected 'error' Was NULL
...
```

### 7.2 Makefile con generacion automatica de runners

```makefile
# Regla para generar runner automaticamente
$(BUILDDIR)/%_runner.c: $(TESTDIR)/%.c | $(BUILDDIR)
	@echo "Generating runner for $<"
	@echo '#include "unity.h"' > $@
	@grep -E '^void (test_|spec_)' $< | sed 's/ *{.*/;/' >> $@
	@echo 'extern void setUp(void);' >> $@
	@echo 'extern void tearDown(void);' >> $@
	@echo 'int main(void) {' >> $@
	@echo '    UNITY_BEGIN();' >> $@
	@grep -oE '(test_|spec_)[a-zA-Z0-9_]+' $< | sort -u | \
		while read name; do echo "    RUN_TEST($$name);"; done >> $@
	@echo '    return UNITY_END();' >> $@
	@echo '}' >> $@

# Test binary: link test source + generated runner + production + unity
$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(BUILDDIR)/test_%_runner.c $(PROD_OBJ) $(UNITY_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^
```

---

## 8. Configuracion de Unity

### 8.1 unity_config.h

Unity se puede configurar creando un archivo `unity_config.h` y compilando con `-DUNITY_INCLUDE_CONFIG_H`:

```c
// unity_config.h
#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

// Soporte para float y double (por defecto habilitado, deshabilitar en embedded)
// #define UNITY_EXCLUDE_FLOAT
// #define UNITY_EXCLUDE_DOUBLE

// Soporte para int64 (por defecto habilitado)
// #define UNITY_EXCLUDE_STDINT_H

// Tamano del tipo int por defecto para comparaciones
// #define UNITY_INT_WIDTH 32

// Soporte para imprimir en color (ANSI escape codes)
// Util para terminales
// #define UNITY_OUTPUT_COLOR

// Custom output function (para embedded sin stdout)
// #define UNITY_OUTPUT_CHAR(c) uart_putchar(c)

// Formato del output
// #define UNITY_OUTPUT_FLUSH() fflush(stdout)

#endif
```

```bash
# Compilar con config personalizada
$ gcc -DUNITY_INCLUDE_CONFIG_H -Iconfig/ -Ivendor/unity ...
```

### 8.2 Opciones utiles para development

```c
// Para desktop/Linux — habilitar color
#define UNITY_OUTPUT_COLOR

// Esto produce output como:
//   test.c:5:test_something:PASS     ← en verde
//   test.c:8:test_other:FAIL: ...    ← en rojo
//   test.c:11:test_skip:IGNORE       ← en amarillo
```

---

## 9. Ejemplo completo — Hash Table con Unity

### 9.1 El modulo bajo test

```c
// src/hash_table.h
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTable HashTable;

HashTable  *ht_new(size_t initial_capacity);
void        ht_free(HashTable *ht);
bool        ht_set(HashTable *ht, const char *key, const char *value);
const char *ht_get(const HashTable *ht, const char *key);
bool        ht_delete(HashTable *ht, const char *key);
bool        ht_contains(const HashTable *ht, const char *key);
size_t      ht_count(const HashTable *ht);

#endif
```

### 9.2 Los tests

```c
// tests/test_hash_table.c
#include "unity.h"
#include "hash_table.h"

static HashTable *ht;

void setUp(void) {
    ht = ht_new(16);
}

void tearDown(void) {
    ht_free(ht);
    ht = NULL;
}

// ── Creation ────────────────────────────────────────────────

void test_ht_new_creates_empty_table(void) {
    TEST_ASSERT_NOT_NULL(ht);
    TEST_ASSERT_EQUAL_size_t(0, ht_count(ht));
}

// ── Set / Get ───────────────────────────────────────────────

void test_ht_set_and_get_single(void) {
    TEST_ASSERT_TRUE(ht_set(ht, "name", "Alice"));
    TEST_ASSERT_EQUAL_STRING("Alice", ht_get(ht, "name"));
}

void test_ht_set_overwrites_existing_key(void) {
    ht_set(ht, "name", "Alice");
    ht_set(ht, "name", "Bob");

    TEST_ASSERT_EQUAL_STRING("Bob", ht_get(ht, "name"));
    TEST_ASSERT_EQUAL_size_t(1, ht_count(ht));
}

void test_ht_set_multiple_keys(void) {
    ht_set(ht, "name", "Alice");
    ht_set(ht, "age", "30");
    ht_set(ht, "city", "NYC");

    TEST_ASSERT_EQUAL_size_t(3, ht_count(ht));
    TEST_ASSERT_EQUAL_STRING("Alice", ht_get(ht, "name"));
    TEST_ASSERT_EQUAL_STRING("30", ht_get(ht, "age"));
    TEST_ASSERT_EQUAL_STRING("NYC", ht_get(ht, "city"));
}

void test_ht_get_missing_key_returns_null(void) {
    TEST_ASSERT_NULL(ht_get(ht, "nonexistent"));
}

// ── Contains ────────────────────────────────────────────────

void test_ht_contains_existing_key(void) {
    ht_set(ht, "key", "val");
    TEST_ASSERT_TRUE(ht_contains(ht, "key"));
}

void test_ht_contains_missing_key(void) {
    TEST_ASSERT_FALSE(ht_contains(ht, "ghost"));
}

// ── Delete ──────────────────────────────────────────────────

void test_ht_delete_existing_key(void) {
    ht_set(ht, "key", "val");
    TEST_ASSERT_TRUE(ht_delete(ht, "key"));
    TEST_ASSERT_NULL(ht_get(ht, "key"));
    TEST_ASSERT_EQUAL_size_t(0, ht_count(ht));
}

void test_ht_delete_missing_key_returns_false(void) {
    TEST_ASSERT_FALSE(ht_delete(ht, "ghost"));
}

void test_ht_delete_then_reinsert(void) {
    ht_set(ht, "key", "first");
    ht_delete(ht, "key");
    ht_set(ht, "key", "second");

    TEST_ASSERT_EQUAL_STRING("second", ht_get(ht, "key"));
    TEST_ASSERT_EQUAL_size_t(1, ht_count(ht));
}

// ── Stress ──────────────────────────────────────────────────

void test_ht_many_entries_triggers_resize(void) {
    char key[32], val[32];

    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        TEST_ASSERT_TRUE(ht_set(ht, key, val));
    }

    TEST_ASSERT_EQUAL_size_t(100, ht_count(ht));

    // Verify all entries survived resize
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(val, ht_get(ht, key), key);
    }
}

// ── Edge cases ──────────────────────────────────────────────

void test_ht_empty_string_key(void) {
    ht_set(ht, "", "empty_key");
    TEST_ASSERT_EQUAL_STRING("empty_key", ht_get(ht, ""));
}

void test_ht_empty_string_value(void) {
    ht_set(ht, "key", "");
    TEST_ASSERT_EQUAL_STRING("", ht_get(ht, "key"));
}

void test_ht_long_key(void) {
    char long_key[256];
    memset(long_key, 'A', 255);
    long_key[255] = '\0';

    ht_set(ht, long_key, "long_key_value");
    TEST_ASSERT_EQUAL_STRING("long_key_value", ht_get(ht, long_key));
}

// ── Null safety ─────────────────────────────────────────────

void test_ht_null_operations(void) {
    TEST_ASSERT_FALSE(ht_set(NULL, "key", "val"));
    TEST_ASSERT_NULL(ht_get(NULL, "key"));
    TEST_ASSERT_FALSE(ht_delete(NULL, "key"));
    TEST_ASSERT_FALSE(ht_contains(NULL, "key"));
    TEST_ASSERT_EQUAL_size_t(0, ht_count(NULL));
}

// ── Runner ──────────────────────────────────────────────────

int main(void) {
    UNITY_BEGIN();

    // Creation
    RUN_TEST(test_ht_new_creates_empty_table);

    // Set / Get
    RUN_TEST(test_ht_set_and_get_single);
    RUN_TEST(test_ht_set_overwrites_existing_key);
    RUN_TEST(test_ht_set_multiple_keys);
    RUN_TEST(test_ht_get_missing_key_returns_null);

    // Contains
    RUN_TEST(test_ht_contains_existing_key);
    RUN_TEST(test_ht_contains_missing_key);

    // Delete
    RUN_TEST(test_ht_delete_existing_key);
    RUN_TEST(test_ht_delete_missing_key_returns_false);
    RUN_TEST(test_ht_delete_then_reinsert);

    // Stress
    RUN_TEST(test_ht_many_entries_triggers_resize);

    // Edge cases
    RUN_TEST(test_ht_empty_string_key);
    RUN_TEST(test_ht_empty_string_value);
    RUN_TEST(test_ht_long_key);

    // Null safety
    RUN_TEST(test_ht_null_operations);

    return UNITY_END();
}
```

```bash
$ make test-verbose
═══ test_hash_table ═══
test_hash_table.c:18:test_ht_new_creates_empty_table:PASS
test_hash_table.c:24:test_ht_set_and_get_single:PASS
test_hash_table.c:29:test_ht_set_overwrites_existing_key:PASS
test_hash_table.c:36:test_ht_set_multiple_keys:PASS
test_hash_table.c:46:test_ht_get_missing_key_returns_null:PASS
test_hash_table.c:51:test_ht_contains_existing_key:PASS
test_hash_table.c:56:test_ht_contains_missing_key:PASS
test_hash_table.c:61:test_ht_delete_existing_key:PASS
test_hash_table.c:68:test_ht_delete_missing_key_returns_false:PASS
test_hash_table.c:72:test_ht_delete_then_reinsert:PASS
test_hash_table.c:82:test_ht_many_entries_triggers_resize:PASS
test_hash_table.c:98:test_ht_empty_string_key:PASS
test_hash_table.c:103:test_ht_empty_string_value:PASS
test_hash_table.c:108:test_ht_long_key:PASS
test_hash_table.c:116:test_ht_null_operations:PASS

-----------------------
15 Tests 0 Failures 0 Ignored
OK
```

---

## 10. Comparacion con testing casero (S01)

### 10.1 El mismo test — antes y despues

**Antes (macros caseras de S01):**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

static int _pass = 0, _fail = 0;

#define CHECK_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                __FILE__, __LINE__, _b, _a); \
        _fail++; goto cleanup; \
    } _pass++; \
} while (0)

#define CHECK_STR_EQ(a, b) do { \
    if (strcmp((a),(b)) != 0) { \
        fprintf(stderr, "  FAIL [%s:%d] expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, (b), (a)); \
        _fail++; goto cleanup; \
    } _pass++; \
} while (0)

// ... 20 lineas mas de macros ...

void test_ht_set_and_get(void) {
    HashTable *ht = ht_new(16);

    ht_set(ht, "name", "Alice");
    CHECK_STR_EQ(ht_get(ht, "name"), "Alice");

cleanup:
    ht_free(ht);
}

int main(void) {
    printf("=== Hash Table ===\n");
    // ... listar cada test manualmente ...
    // ... macro RUN personalizada ...
    // ... SUMMARY personalizada ...
}
```

**Despues (Unity):**

```c
#include "unity.h"
#include "hash_table.h"

static HashTable *ht;

void setUp(void) { ht = ht_new(16); }
void tearDown(void) { ht_free(ht); ht = NULL; }

void test_ht_set_and_get(void) {
    ht_set(ht, "name", "Alice");
    TEST_ASSERT_EQUAL_STRING("Alice", ht_get(ht, "name"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ht_set_and_get);
    return UNITY_END();
}
```

### 10.2 Lo que ganas con Unity

| Aspecto | Casero | Unity |
|---------|--------|-------|
| Lineas de boilerplate | ~40-60 (macros, runner, summary) | 0 (todo esta en unity.h) |
| Cleanup | goto cleanup o manual | tearDown automatico |
| Mensaje de error | "expected X, got Y" | Tipo-especifico + archivo:linea:test_name |
| Formato de output | Custom, no estandar | Estandarizado, parseable por CI |
| Arrays | No (necesitas loop) | TEST_ASSERT_EQUAL_INT_ARRAY |
| Floats | Manual (epsilon) | TEST_ASSERT_FLOAT_WITHIN |
| Strings | strcmp manual | TEST_ASSERT_EQUAL_STRING |
| Memoria | memcmp manual | TEST_ASSERT_EQUAL_MEMORY |
| Test ignorado | No hay concepto | TEST_IGNORE / TEST_IGNORE_MESSAGE |

### 10.3 Lo que pierdes con Unity

| Aspecto | Detalle |
|---------|---------|
| **Control total** | No puedes modificar como Unity reporta fallos (sin recompilar unity.c) |
| **Un solo setUp/tearDown** | Si necesitas fixtures diferentes, necesitas archivos separados |
| **Dependencia externa** | Aunque son solo 3 archivos, es codigo que no escribiste tu |
| **Tipo de assert** | Tienes que elegir la macro correcta (EQUAL_INT vs EQUAL_UINT vs EQUAL_HEX) — con macros caseras usabas long para todo |

---

## 11. Comparacion con Rust y Go

### 11.1 El mismo test suite en tres lenguajes

**C con Unity:**

```c
#include "unity.h"
#include "hash_table.h"

static HashTable *ht;

void setUp(void) { ht = ht_new(16); }
void tearDown(void) { ht_free(ht); ht = NULL; }

void test_set_and_get(void) {
    ht_set(ht, "key", "value");
    TEST_ASSERT_EQUAL_STRING("value", ht_get(ht, "key"));
}

void test_overwrite(void) {
    ht_set(ht, "key", "first");
    ht_set(ht, "key", "second");
    TEST_ASSERT_EQUAL_STRING("second", ht_get(ht, "key"));
    TEST_ASSERT_EQUAL_size_t(1, ht_count(ht));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_set_and_get);
    RUN_TEST(test_overwrite);
    return UNITY_END();
}
```

**Rust (stdlib):**

```rust
use std::collections::HashMap;

#[cfg(test)]
mod tests {
    use super::*;

    fn setup() -> HashMap<String, String> {
        HashMap::new()
    }

    #[test]
    fn test_set_and_get() {
        let mut ht = setup();
        ht.insert("key".to_string(), "value".to_string());
        assert_eq!(ht.get("key").unwrap(), "value");
    }

    #[test]
    fn test_overwrite() {
        let mut ht = setup();
        ht.insert("key".to_string(), "first".to_string());
        ht.insert("key".to_string(), "second".to_string());
        assert_eq!(ht.get("key").unwrap(), "second");
        assert_eq!(ht.len(), 1);
    }
}
```

**Go (stdlib):**

```go
package hashtable

import "testing"

func TestSetAndGet(t *testing.T) {
    ht := New(16)
    defer ht.Free()

    ht.Set("key", "value")
    if got := ht.Get("key"); got != "value" {
        t.Errorf("Get(\"key\") = %q, want \"value\"", got)
    }
}

func TestOverwrite(t *testing.T) {
    ht := New(16)
    defer ht.Free()

    ht.Set("key", "first")
    ht.Set("key", "second")
    if got := ht.Get("key"); got != "second" {
        t.Errorf("Get(\"key\") = %q, want \"second\"", got)
    }
    if got := ht.Count(); got != 1 {
        t.Errorf("Count() = %d, want 1", got)
    }
}
```

### 11.2 Tabla comparativa de frameworks

| Aspecto | Unity (C) | Rust stdlib | Go stdlib |
|---------|-----------|-------------|-----------|
| **Instalacion** | Copiar 3 archivos | Nada (built-in) | Nada (built-in) |
| **setUp/tearDown** | setUp()/tearDown() globales | Funciones helper | t.Cleanup() / defer |
| **Assertions** | 80+ macros tipadas | assert!, assert_eq!, assert_ne! | if/t.Errorf (no assertions) |
| **Test discovery** | Manual o generado | Automatico (#[test]) | Automatico (Test*) |
| **Output** | file:line:test:PASS/FAIL | ok/FAILED + panic message | ok/FAIL + error message |
| **Subtests** | No | Closures informales | t.Run() |
| **Parallelism** | No (un proceso) | Por defecto (threads) | t.Parallel() |
| **Cleanup en fallo** | tearDown (setjmp/longjmp) | Drop (RAII) | defer / t.Cleanup() |

---

## 12. Patrones avanzados

### 12.1 Test parametrico (table-driven) con Unity

Unity no tiene soporte nativo para table-driven tests, pero puedes simularlo con un loop y TEST_ASSERT_MESSAGE:

```c
#include "unity.h"
#include "math_utils.h"

void setUp(void) {}
void tearDown(void) {}

// Table-driven test
void test_clamp_table(void) {
    struct {
        const char *name;
        int value;
        int min;
        int max;
        int expected;
    } cases[] = {
        {"below min",    -10, 0, 100,   0},
        {"above max",    200, 0, 100, 100},
        {"at min",         0, 0, 100,   0},
        {"at max",       100, 0, 100, 100},
        {"in range",      50, 0, 100,  50},
        {"negative range",-5,-10, -1,  -5},
        {"single value",  42, 42, 42,  42},
    };
    size_t num_cases = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < num_cases; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "case '%s': clamp(%d, %d, %d)",
                 cases[i].name, cases[i].value, cases[i].min, cases[i].max);

        int result = clamp(cases[i].value, cases[i].min, cases[i].max);
        TEST_ASSERT_EQUAL_INT_MESSAGE(cases[i].expected, result, msg);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clamp_table);
    return UNITY_END();
}
```

Output si un caso falla:

```
test_math.c:28:test_clamp_table:FAIL: case 'above max': clamp(200, 0, 100). Expected 100 Was 200
```

### 12.2 Tests con archivos temporales

```c
#include "unity.h"
#include "config_parser.h"
#include <stdio.h>
#include <unistd.h>

static char tmpfile_path[256];

void setUp(void) {
    snprintf(tmpfile_path, sizeof(tmpfile_path), "/tmp/test_cfg_%d.ini", getpid());
}

void tearDown(void) {
    unlink(tmpfile_path);  // Borrar el archivo temporal
}

// Helper: escribir contenido al archivo temporal
static void write_test_file(const char *content) {
    FILE *f = fopen(tmpfile_path, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to create temp file");
    fputs(content, f);
    fclose(f);
}

void test_parse_basic_config(void) {
    write_test_file("host=localhost\nport=8080\n");

    Config cfg;
    int rc = config_parse(tmpfile_path, &cfg);

    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("localhost", cfg.host);
    TEST_ASSERT_EQUAL_INT(8080, cfg.port);
}

void test_parse_empty_file(void) {
    write_test_file("");

    Config cfg;
    int rc = config_parse(tmpfile_path, &cfg);

    TEST_ASSERT_EQUAL_INT(0, rc);
    // Valores por defecto
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", cfg.host);
    TEST_ASSERT_EQUAL_INT(80, cfg.port);
}

void test_parse_nonexistent_file(void) {
    int rc = config_parse("/tmp/no_existe_12345.ini", NULL);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_basic_config);
    RUN_TEST(test_parse_empty_file);
    RUN_TEST(test_parse_nonexistent_file);
    return UNITY_END();
}
```

### 12.3 Tests con stdout capturado

```c
#include "unity.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int saved_stdout;
static int pipe_fd[2];
static char captured[4096];

void setUp(void) {
    // Redirigir stdout a un pipe
    pipe(pipe_fd);
    saved_stdout = dup(STDOUT_FILENO);
    dup2(pipe_fd[1], STDOUT_FILENO);
}

void tearDown(void) {
    // Restaurar stdout
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    close(pipe_fd[1]);

    // Leer lo que se capturo
    ssize_t n = read(pipe_fd[0], captured, sizeof(captured) - 1);
    if (n > 0) captured[n] = '\0';
    else captured[0] = '\0';
    close(pipe_fd[0]);
}

void test_log_info_writes_to_stdout(void) {
    log_info("hello world");  // Esto va a stdout (que es nuestro pipe)

    // Forzar flush para que los datos lleguen al pipe
    fflush(stdout);
    // Leer parcial (tearDown leera el resto)
    // Nota: la verificacion real se hace en el patron tear-then-check

    // Necesitamos un patron ligeramente diferente para verificar
    // despues de capturar. Una alternativa mas limpia:
}

// Patron mas limpio: funcion helper que captura
static char *capture_stdout(void (*fn)(void)) {
    int pfd[2];
    pipe(pfd);
    int saved = dup(STDOUT_FILENO);
    dup2(pfd[1], STDOUT_FILENO);

    fn();
    fflush(stdout);

    dup2(saved, STDOUT_FILENO);
    close(saved);
    close(pfd[1]);

    static char buf[4096];
    ssize_t n = read(pfd[0], buf, sizeof(buf) - 1);
    close(pfd[0]);
    buf[n > 0 ? n : 0] = '\0';
    return buf;
}

static void call_log_hello(void) {
    log_info("hello");
}

void test_log_info_format(void) {
    char *output = capture_stdout(call_log_hello);
    TEST_ASSERT_NOT_NULL(strstr(output, "[INFO]"));
    TEST_ASSERT_NOT_NULL(strstr(output, "hello"));
}
```

---

## 13. Programa de practica

### Objetivo

Escribir una **suite completa de tests con Unity** para un modulo `dynamic_array` (vector dinamico de strings).

### Especificacion del modulo

```c
// dynamic_array.h
#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct DynArray DynArray;

DynArray   *da_new(size_t initial_cap);
void        da_free(DynArray *da);

bool        da_push(DynArray *da, const char *str);    // Copia el string
const char *da_get(const DynArray *da, size_t index);
bool        da_set(DynArray *da, size_t index, const char *str);
bool        da_remove(DynArray *da, size_t index);
bool        da_insert(DynArray *da, size_t index, const char *str);

size_t      da_len(const DynArray *da);
bool        da_is_empty(const DynArray *da);
int         da_find(const DynArray *da, const char *str);  // -1 si no esta
bool        da_contains(const DynArray *da, const char *str);

void        da_sort(DynArray *da);           // Orden lexicografico
void        da_reverse(DynArray *da);
void        da_clear(DynArray *da);          // Vacia sin destruir

#endif
```

### Tests que escribir

```c
// tests/test_dynamic_array.c
#include "unity.h"
#include "dynamic_array.h"

static DynArray *da;

void setUp(void) { da = da_new(4); }
void tearDown(void) { da_free(da); da = NULL; }

// Escribir al menos estos tests:
// test_da_new_creates_empty
// test_da_push_single
// test_da_push_copies_string (modificar el original, verificar que da no cambia)
// test_da_push_beyond_capacity (push 100 strings con capacidad 4)
// test_da_get_valid_index
// test_da_get_invalid_index_returns_null
// test_da_set_overwrites
// test_da_remove_middle_shifts_elements
// test_da_remove_invalid_index_returns_false
// test_da_insert_middle_shifts_right
// test_da_find_existing_returns_index
// test_da_find_missing_returns_negative
// test_da_contains_true_and_false
// test_da_sort_alphabetical
// test_da_reverse_three_elements
// test_da_reverse_then_reverse_restores_original
// test_da_clear_makes_empty_and_reusable
// test_da_null_safety
// test_da_empty_string_push
// test_da_table_driven_push_get (table-driven con 5+ casos)
```

---

## 14. Ejercicios

### Ejercicio 1: Migrar tests caseros a Unity

Toma los tests del ring buffer (S01/T01) o la linked list (S01/T02) y reescribelos usando Unity. Usa setUp/tearDown para la fixture y reemplaza tus macros CHECK con TEST_ASSERT_*.

### Ejercicio 2: Test parametrico

Escribe un test table-driven con Unity para una funcion `int roman_to_int(const char *roman)` que convierte numeros romanos a enteros. La tabla debe incluir al menos 10 casos: I, V, X, L, C, D, M, IV, XIV, MCMXCIV (1994).

### Ejercicio 3: Tests con archivos temporales

Escribe tests con Unity para un modulo `csv_reader` que lee archivos CSV. Usa setUp para crear un archivo temporal con datos de prueba y tearDown para borrarlo. Tests minimos:
- CSV con 3 columnas y 5 filas
- CSV vacio
- CSV con campos que contienen comas (entre comillas)
- Archivo que no existe

### Ejercicio 4: Multiples suites con Makefile

Crea un proyecto con 3 archivos de test Unity (`test_stack.c`, `test_queue.c`, `test_deque.c`), cada uno con su propio setUp/tearDown, y un Makefile que:
- Compila cada suite como un ejecutable separado
- Tiene un target `test` que ejecuta las 3 suites
- Tiene un target `test-asan` con sanitizers
- Muestra un resumen final con cuantas suites pasaron/fallaron

---

## 15. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                       UNITY — MAPA MENTAL                                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ¿Que es?                                                                     │
│  └─ Framework de testing para C puro (3 archivos, sin dependencias)           │
│                                                                                │
│  Instalacion:                                                                 │
│  └─ Copiar unity.c + unity.h + unity_internals.h a vendor/unity/              │
│                                                                                │
│  Estructura de un test:                                                       │
│  ├─ #include "unity.h"                                                        │
│  ├─ void setUp(void) { ... }     ← obligatorio, antes de cada test           │
│  ├─ void tearDown(void) { ... }  ← obligatorio, despues de cada test         │
│  ├─ void test_xxx(void) { ... }  ← tests con TEST_ASSERT_*                   │
│  └─ int main(void) { UNITY_BEGIN(); RUN_TEST(...); return UNITY_END(); }      │
│                                                                                │
│  Assertions (80+):                                                            │
│  ├─ Booleanas: TEST_ASSERT_TRUE, FALSE, NULL, NOT_NULL                       │
│  ├─ Enteros:   TEST_ASSERT_EQUAL_INT, UINT, HEX, INT8..INT64                │
│  ├─ Rango:     TEST_ASSERT_GREATER_THAN, LESS_THAN, INT_WITHIN              │
│  ├─ Float:     TEST_ASSERT_FLOAT_WITHIN, IS_INF, IS_NAN                     │
│  ├─ Strings:   TEST_ASSERT_EQUAL_STRING, STRING_LEN                         │
│  ├─ Memoria:   TEST_ASSERT_EQUAL_MEMORY                                     │
│  ├─ Arrays:    TEST_ASSERT_EQUAL_INT_ARRAY, STRING_ARRAY                    │
│  └─ Todas tienen variante _MESSAGE(exp, act, "msg")                         │
│                                                                                │
│  Fixtures:                                                                    │
│  ├─ Un solo par setUp/tearDown por archivo                                   │
│  ├─ tearDown se ejecuta SIEMPRE (setjmp/longjmp interno)                    │
│  ├─ Para multiples fixtures: archivos separados                              │
│  └─ Variables globales static para pasar estado test ↔ fixture              │
│                                                                                │
│  Makefile:                                                                    │
│  ├─ Compilar: gcc -Ivendor/unity test.c src.c vendor/unity/unity.c           │
│  ├─ Target test: ejecutar todos los test_*                                   │
│  └─ Target test-asan: recompilar con sanitizers                              │
│                                                                                │
│  Proximo topico: T02 — CMocka                                                │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: S01/T04 — Fixtures y setup/teardown | Siguiente: T02 — CMocka →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 2 — Frameworks de Testing en C > Topico 1 — Unity*
