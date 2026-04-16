# CMocka — assert_int_equal, expect_*, will_return, test groups, por que elegir CMocka sobre Unity

## 1. Introduccion

CMocka es un framework de testing y **mocking** para C. A diferencia de Unity (que se enfoca en assertions y runner), CMocka integra testing Y mocking en un solo paquete. Es la evolucion de **cmockery** (creado por Google), mantenido por la comunidad desde 2013.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                            CMOCKA                                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Origen:       cmockery (Google, 2008) → fork como cmocka (2013)                │
│  Sitio:        https://cmocka.org                                               │
│  Repositorio:  https://gitlab.com/cmocka/cmocka                                │
│  Licencia:     Apache 2.0                                                       │
│  Lenguaje:     C puro (C99)                                                     │
│  Distribucion: Libreria compartida (.so/.dylib) o estatica (.a)                 │
│  Dependencias: Ninguna en runtime (solo el compilador y cmake para build)       │
│                                                                                  │
│  ┌────────────────────────────────────────────────────────────────────┐          │
│  │  Unity:  testing = assertions + setUp/tearDown + runner           │          │
│  │  CMocka: testing = assertions + setUp/tearDown + runner           │          │
│  │                  + MOCKING (expect_*, will_return, --wrap)        │          │
│  │                  + GROUP SETUP/TEARDOWN                            │          │
│  │                  + STATE parameter (fixture pasada al test)       │          │
│  └────────────────────────────────────────────────────────────────────┘          │
│                                                                                  │
│  Usuarios conocidos:                                                            │
│  • Samba (networking suite — uno de los proyectos C mas grandes del mundo)      │
│  • libssh (libreria SSH en C)                                                   │
│  • OpenVPN                                                                      │
│  • systemd (usa cmocka para algunos tests)                                      │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 CMocka vs Unity — diferencias fundamentales

| Aspecto | Unity | CMocka |
|---------|-------|--------|
| **Enfoque principal** | Assertions | Assertions + Mocking |
| **Distribucion** | 3 archivos fuente (copiar a vendor/) | Libreria compilada (.so/.a) o build con cmake |
| **Fixture** | setUp/tearDown globales (no reciben parametros) | setUp/tearDown reciben `void **state` (la fixture) |
| **Scope de fixture** | Per-test (un par global por archivo) | Per-test Y per-group (setup de suite) |
| **Mocking** | No (necesita CMock aparte) | Si — expect_*, will_return, --wrap |
| **Output format** | Custom (file:line:test:PASS/FAIL) | TAP, subunit, XML (parseable por CI) |
| **Aislamiento** | setjmp/longjmp | setjmp/longjmp + signal handlers |
| **Leak detection** | No | Si — detecta memory leaks de malloc/calloc en tests |
| **Plataformas** | Cualquiera con compilador C | POSIX + Windows (necesita cmake para build) |
| **Embedded** | Excelente (sin dependencias, bare-metal) | Posible pero menos comun (necesita stdlib) |
| **Tamaño** | ~3000 lineas | ~5000 lineas |

### 1.2 Cuando elegir CMocka sobre Unity

Elige **CMocka** cuando:
- Necesitas **mocking** (reemplazar funciones en link-time con `--wrap`)
- Necesitas **fixtures con estado** (void **state en vez de globals)
- Necesitas **group setup/teardown** (inicializar una vez para toda la suite)
- Necesitas **leak detection** built-in
- Tu proyecto ya usa **CMake** (CMocka se integra nativamente)
- Tu proyecto es de **aplicaciones/servidores** (no embedded)

Elige **Unity** cuando:
- Trabajas en **embedded** (bare-metal, sin stdlib)
- Quieres **cero dependencias** externas (solo copiar 3 archivos)
- No necesitas mocking
- El proyecto usa **Make** puro (sin CMake)

---

## 2. Instalacion

### 2.1 Desde el gestor de paquetes (recomendado en Linux)

```bash
# Debian/Ubuntu
$ sudo apt install libcmocka-dev

# Fedora
$ sudo dnf install libcmocka-devel

# Arch
$ sudo pacman -S cmocka

# macOS
$ brew install cmocka
```

Despues de instalar:

```bash
# Verificar instalacion
$ pkg-config --cflags --libs cmocka
-lcmocka

# Los headers estan en:
$ ls /usr/include/cmocka*.h
/usr/include/cmocka.h
/usr/include/cmocka_pbc.h    # Pre/post conditions (opcional)
```

### 2.2 Compilar desde fuente

```bash
$ git clone https://gitlab.com/cmocka/cmocka.git
$ cd cmocka
$ mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
$ make
$ sudo make install
```

### 2.3 CMake FetchContent (sin instalar globalmente)

```cmake
include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG cmocka-1.1.7
)
set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)
```

### 2.4 Verificar la instalacion

```c
// test_sanity.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void test_sanity(void **state) {
    (void)state;  // No usado
    assert_true(1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sanity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

```bash
$ gcc -o test_sanity test_sanity.c -lcmocka
$ ./test_sanity
[==========] Running 1 test(s).
[ RUN      ] test_sanity
[       OK ] test_sanity
[==========] 1 test(s) run.
[  PASSED  ] 1 test(s).
```

**Nota sobre los includes**: CMocka requiere que incluyas `<stdarg.h>`, `<stddef.h>`, y `<setjmp.h>` **antes** de `<cmocka.h>`. Esto es porque cmocka.h usa tipos definidos en estos headers (va_list, size_t, jmp_buf) pero no los incluye internamente para maximizar la portabilidad.

```c
// ORDEN OBLIGATORIO de includes para CMocka
#include <stdarg.h>    // 1. va_list
#include <stddef.h>    // 2. size_t, NULL
#include <setjmp.h>    // 3. jmp_buf
#include <cmocka.h>    // 4. CMocka (ultimo)
```

---

## 3. Anatomia de un test con CMocka

### 3.1 Estructura basica

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

// Los tests reciben void **state (la fixture)
static void test_something(void **state) {
    (void)state;  // Cast a void si no se usa (evita warning)
    assert_int_equal(2 + 2, 4);
}

static void test_another(void **state) {
    (void)state;
    assert_string_equal("hello", "hello");
}

int main(void) {
    // Array de tests
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_something),
        cmocka_unit_test(test_another),
    };

    // Ejecutar — NULL, NULL = sin group setup/teardown
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 3.2 La firma de los tests: void **state

A diferencia de Unity (donde los tests son `void test(void)`), en CMocka todos los tests reciben `void **state`. Este parametro es la **fixture**:

```
┌────────────────────────────────────────────────────────────────────────┐
│              void **state — EL PARAMETRO FIXTURE                      │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Unity:                                                               │
│  ┌──────────────┐                                                     │
│  │ static Stack *s;  ← global, compartida por todos los tests        │
│  │                                                                    │
│  │ void setUp() { s = stack_new(4); }                                │
│  │ void test() { stack_push(s, 42); }  ← accede a la global         │
│  │ void tearDown() { stack_free(s); }                                │
│  └──────────────┘                                                     │
│                                                                        │
│  CMocka:                                                              │
│  ┌──────────────┐                                                     │
│  │ int setup(void **state) {                                         │
│  │     *state = stack_new(4);  ← fixture almacenada en state         │
│  │     return 0;                                                      │
│  │ }                                                                  │
│  │                                                                    │
│  │ void test(void **state) {                                         │
│  │     Stack *s = *state;      ← fixture recuperada de state         │
│  │     stack_push(s, 42);                                             │
│  │ }                                                                  │
│  │                                                                    │
│  │ int teardown(void **state) {                                      │
│  │     stack_free(*state);     ← liberada via state                  │
│  │     return 0;                                                      │
│  │ }                                                                  │
│  └──────────────┘                                                     │
│                                                                        │
│  Ventaja de CMocka: no hay variable global, la fixture es explícita   │
│  y cada test puede tener su propia fixture diferente                  │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Registrar tests — cmocka_unit_test y variantes

```c
// Sin fixture
cmocka_unit_test(test_fn)

// Con setup y teardown per-test
cmocka_unit_test_setup_teardown(test_fn, setup_fn, teardown_fn)

// Con setup per-test (sin teardown)
cmocka_unit_test_setup(test_fn, setup_fn)

// Con state inicial (sin setup function, pasar un puntero directamente)
cmocka_unit_test_prestate(test_fn, initial_state_ptr)

// Con setup, teardown, y state inicial
cmocka_unit_test_prestate_setup_teardown(test_fn, setup_fn, teardown_fn, state_ptr)
```

### 3.4 Ejecutar tests — cmocka_run_group_tests

```c
int cmocka_run_group_tests(
    const struct CMUnitTest tests[],  // Array de tests
    CMFixtureFunction group_setup,    // Setup de GRUPO (se ejecuta UNA vez antes de todos)
    CMFixtureFunction group_teardown  // Teardown de GRUPO (se ejecuta UNA vez despues de todos)
);
// Retorna: numero de fallos (0 = todo OK)
```

---

## 4. Assertions de CMocka

### 4.1 Assertions basicas

```c
// Booleanas
assert_true(expression);
assert_false(expression);

// Punteros
assert_null(ptr);
assert_non_null(ptr);

// Igualdad generica (compara como uintmax_t)
assert_int_equal(expected, actual);
assert_int_not_equal(val1, val2);

// Ejemplo
static void test_stack_new(void **state) {
    (void)state;
    Stack *s = stack_new(4);
    assert_non_null(s);
    assert_int_equal(0, stack_len(s));
    assert_true(stack_is_empty(s));
    stack_free(s);
}
```

### 4.2 Comparacion de enteros

```c
// Comparaciones con formato
assert_int_equal(expected, actual);       // int (con signo)
assert_int_not_equal(a, b);

// Rango
assert_in_range(value, min, max);         // min <= value <= max
assert_not_in_range(value, min, max);     // value < min || value > max

// Ejemplo
static void test_clamp(void **state) {
    (void)state;
    assert_int_equal(0, clamp(-10, 0, 100));
    assert_int_equal(100, clamp(200, 0, 100));
    assert_int_equal(50, clamp(50, 0, 100));

    // Verificar que el resultado esta en rango
    int result = clamp(50, 0, 100);
    assert_in_range(result, 0, 100);
}
```

### 4.3 Comparacion de strings

```c
assert_string_equal(expected, actual);
assert_string_not_equal(a, b);

// Ejemplo
static void test_format_greeting(void **state) {
    (void)state;
    char buf[64];
    format_greeting(buf, sizeof(buf), "Alice");
    assert_string_equal("Hello, Alice!", buf);
}
```

### 4.4 Comparacion de memoria

```c
assert_memory_equal(expected, actual, size);
assert_memory_not_equal(a, b, size);

// Ejemplo
static void test_serialize(void **state) {
    (void)state;
    uint8_t expected[] = {0x01, 0x00, 0x0A, 0xFF};
    uint8_t buf[4];
    Packet p = { .type = 1, .flags = 0, .length = 0x0AFF };
    serialize_packet(&p, buf);
    assert_memory_equal(expected, buf, 4);
}
```

### 4.5 assert_return_code — para funciones que retornan errno

```c
// Verifica que rc == 0 (exito) o, si falla, muestra errno
assert_return_code(return_code, errno_value);

// Ejemplo
static void test_open_file(void **state) {
    (void)state;
    int fd = open("/tmp/test_file", O_CREAT | O_WRONLY, 0644);
    assert_return_code(fd, errno);  // Si fd < 0, muestra el strerror(errno)
    if (fd >= 0) close(fd);
    unlink("/tmp/test_file");
}
```

Output si falla:

```
[  ERROR   ] --- 0x-1 != 0x0 (Permission denied)
[   LINE   ] --- test.c:8: error: Failure!
```

### 4.6 fail() y skip()

```c
// Fallo incondicional
fail();
fail_msg("reason: %s", reason);

// Skip (test ignorado, equivalente a TEST_IGNORE de Unity)
skip();

// Ejemplo
static void test_feature_todo(void **state) {
    (void)state;
    skip();  // Este test no se ejecuta, aparece como [  SKIPPED ]
}

static void test_requires_root(void **state) {
    (void)state;
    if (getuid() != 0) {
        skip();  // Skip si no somos root
    }
    // ... test que necesita root ...
}
```

### 4.7 Tabla de referencia rapida

| Que verificar | CMocka | Unity equivalente |
|---------------|--------|-------------------|
| Condicion verdadera | `assert_true(x)` | `TEST_ASSERT_TRUE(x)` |
| Condicion falsa | `assert_false(x)` | `TEST_ASSERT_FALSE(x)` |
| Puntero NULL | `assert_null(p)` | `TEST_ASSERT_NULL(p)` |
| Puntero no NULL | `assert_non_null(p)` | `TEST_ASSERT_NOT_NULL(p)` |
| Enteros iguales | `assert_int_equal(e, a)` | `TEST_ASSERT_EQUAL_INT(e, a)` |
| Enteros diferentes | `assert_int_not_equal(a, b)` | (no tiene directo) |
| En rango | `assert_in_range(v, min, max)` | `TEST_ASSERT_INT_WITHIN(d, e, a)` |
| Strings iguales | `assert_string_equal(e, a)` | `TEST_ASSERT_EQUAL_STRING(e, a)` |
| Memoria igual | `assert_memory_equal(e, a, n)` | `TEST_ASSERT_EQUAL_MEMORY(e, a, n)` |
| Return code | `assert_return_code(rc, errno)` | (no tiene) |
| Fallo forzado | `fail_msg("why")` | `TEST_FAIL_MESSAGE("why")` |
| Skip | `skip()` | `TEST_IGNORE()` |

CMocka tiene **menos variantes** que Unity (no hay EQUAL_INT8, EQUAL_HEX16, etc.), pero sus assertions cubren el 95% de los casos. Para embedded con registros de hardware de tamaño especifico, Unity tiene ventaja.

---

## 5. Fixtures con void **state

### 5.1 Setup y teardown per-test

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include "stack.h"

// Setup: crea la fixture y la almacena en *state
static int stack_setup(void **state) {
    Stack *s = stack_new(4);
    if (s == NULL) return -1;  // Setup fallo
    *state = s;
    return 0;  // 0 = exito
}

// Teardown: libera la fixture desde *state
static int stack_teardown(void **state) {
    stack_free(*state);
    return 0;
}

// Tests: recuperan la fixture de *state
static void test_stack_is_empty(void **state) {
    Stack *s = *state;
    assert_true(stack_is_empty(s));
    assert_int_equal(0, stack_len(s));
}

static void test_stack_push(void **state) {
    Stack *s = *state;
    stack_push(s, 42);
    assert_false(stack_is_empty(s));
    assert_int_equal(1, stack_len(s));
}

static void test_stack_pop_lifo(void **state) {
    Stack *s = *state;
    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);

    int val;
    stack_pop(s, &val);
    assert_int_equal(30, val);

    stack_pop(s, &val);
    assert_int_equal(20, val);

    stack_pop(s, &val);
    assert_int_equal(10, val);

    assert_true(stack_is_empty(s));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_stack_is_empty, stack_setup, stack_teardown),
        cmocka_unit_test_setup_teardown(test_stack_push, stack_setup, stack_teardown),
        cmocka_unit_test_setup_teardown(test_stack_pop_lifo, stack_setup, stack_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

```bash
$ gcc -o test_stack test_stack.c stack.c -lcmocka
$ ./test_stack
[==========] Running 3 test(s).
[ RUN      ] test_stack_is_empty
[       OK ] test_stack_is_empty
[ RUN      ] test_stack_push
[       OK ] test_stack_push
[ RUN      ] test_stack_pop_lifo
[       OK ] test_stack_pop_lifo
[==========] 3 test(s) run.
[  PASSED  ] 3 test(s).
```

### 5.2 Fixture compleja con struct

```c
typedef struct {
    HashTable *ht;
    char *tmpdir;
    FILE *logfile;
} TestContext;

static int ctx_setup(void **state) {
    TestContext *ctx = calloc(1, sizeof(TestContext));
    if (ctx == NULL) return -1;

    ctx->ht = ht_new(16);
    if (ctx->ht == NULL) { free(ctx); return -1; }

    char template[] = "/tmp/test_XXXXXX";
    ctx->tmpdir = strdup(mkdtemp(template));
    if (ctx->tmpdir == NULL) { ht_free(ctx->ht); free(ctx); return -1; }

    char logpath[512];
    snprintf(logpath, sizeof(logpath), "%s/test.log", ctx->tmpdir);
    ctx->logfile = fopen(logpath, "w");
    if (ctx->logfile == NULL) {
        free(ctx->tmpdir); ht_free(ctx->ht); free(ctx);
        return -1;
    }

    // Pre-cargar datos
    ht_set(ctx->ht, "name", "Alice");
    ht_set(ctx->ht, "age", "30");

    *state = ctx;
    return 0;
}

static int ctx_teardown(void **state) {
    TestContext *ctx = *state;
    if (ctx == NULL) return 0;

    if (ctx->logfile) fclose(ctx->logfile);

    if (ctx->tmpdir) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", ctx->tmpdir);
        system(cmd);
        free(ctx->tmpdir);
    }

    ht_free(ctx->ht);
    free(ctx);
    return 0;
}

static void test_ht_has_preloaded_data(void **state) {
    TestContext *ctx = *state;
    assert_int_equal(2, ht_count(ctx->ht));
    assert_string_equal("Alice", ht_get(ctx->ht, "name"));
    assert_string_equal("30", ht_get(ctx->ht, "age"));
}

static void test_ht_insert_new_key(void **state) {
    TestContext *ctx = *state;
    ht_set(ctx->ht, "city", "NYC");
    assert_int_equal(3, ht_count(ctx->ht));
    assert_string_equal("NYC", ht_get(ctx->ht, "city"));
}
```

### 5.3 Group setup/teardown — fixture de suite

CMocka distingue dos niveles de fixture:

```
┌──────────────────────────────────────────────────────────────────────┐
│              NIVELES DE FIXTURE EN CMOCKA                           │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  cmocka_run_group_tests(tests, GROUP_SETUP, GROUP_TEARDOWN)         │
│  │                                                                   │
│  ├─ GROUP_SETUP         ← Una vez, antes de todos los tests         │
│  │                         (ej: conectar a DB, crear tablas)        │
│  │                                                                   │
│  ├─ Test 1:                                                          │
│  │   ├─ per-test setup  ← Antes de este test                       │
│  │   ├─ test function                                               │
│  │   └─ per-test teardown ← Despues de este test                   │
│  │                                                                   │
│  ├─ Test 2:                                                          │
│  │   ├─ per-test setup                                              │
│  │   ├─ test function                                               │
│  │   └─ per-test teardown                                           │
│  │                                                                   │
│  ├─ Test 3:             ← Sin per-test setup/teardown               │
│  │   └─ test function   (usa cmocka_unit_test() simple)             │
│  │                                                                   │
│  └─ GROUP_TEARDOWN      ← Una vez, despues de todos los tests       │
│                            (ej: cerrar DB, borrar tablas)           │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

```c
// El state del GROUP se comparte entre todos los tests
// Los tests que no tienen per-test setup heredan el state del grupo

// Group setup: crear base de datos (costoso, una sola vez)
static int group_setup(void **state) {
    Database *db = db_open(":memory:");
    db_exec(db, "CREATE TABLE users (id INT, name TEXT)");
    *state = db;
    return 0;
}

// Group teardown: cerrar base de datos
static int group_teardown(void **state) {
    db_close(*state);
    return 0;
}

// Per-test setup: limpiar datos (barato, cada test)
static int per_test_setup(void **state) {
    Database *db = *state;
    db_exec(db, "DELETE FROM users");
    db_exec(db, "INSERT INTO users VALUES (1, 'Alice')");
    db_exec(db, "INSERT INTO users VALUES (2, 'Bob')");
    return 0;
}

static void test_count_users(void **state) {
    Database *db = *state;
    assert_int_equal(2, db_count(db, "users"));
}

static void test_find_user(void **state) {
    Database *db = *state;
    assert_string_equal("Alice", db_find_name(db, 1));
}

static void test_insert_user(void **state) {
    Database *db = *state;
    db_exec(db, "INSERT INTO users VALUES (3, 'Charlie')");
    assert_int_equal(3, db_count(db, "users"));
    // Nota: per_test_setup limpiara esto para el siguiente test
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_count_users, per_test_setup),
        cmocka_unit_test_setup(test_find_user, per_test_setup),
        cmocka_unit_test_setup(test_insert_user, per_test_setup),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
```

Esto es exactamente el patron que describimos manualmente en S01/T04 (fixtures globales con reset per-test), pero formalizado por CMocka.

---

## 6. Mocking con CMocka — expect_* y will_return

### 6.1 Que es mocking en CMocka

CMocka permite **reemplazar funciones reales** por versiones mock en link-time usando el flag `--wrap` del linker. Cuando mockeas una funcion, puedes:

1. **Definir que debe retornar** (`will_return`)
2. **Verificar que fue llamada con los parametros correctos** (`expect_*`)
3. **Verificar cuantas veces fue llamada**

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MOCKING CON --wrap                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Codigo de produccion:                                                  │
│  ┌──────────────────────────────────────────────┐                       │
│  │  int read_sensor(int channel) {              │                       │
│  │      // Lee hardware real                    │                       │
│  │      return ioctl(fd, READ_ADC, channel);    │                       │
│  │  }                                           │                       │
│  └──────────────────────────────────────────────┘                       │
│                                                                          │
│  Compilacion normal:                                                    │
│  gcc -o program main.c sensor.c                                         │
│  → read_sensor llama al hardware real                                   │
│                                                                          │
│  Compilacion con mock (--wrap):                                         │
│  gcc -Wl,--wrap=read_sensor -o test test.c sensor.c -lcmocka           │
│  → Todas las llamadas a read_sensor van a __wrap_read_sensor            │
│  → La funcion original se renombra a __real_read_sensor                 │
│                                                                          │
│  En el test:                                                            │
│  ┌──────────────────────────────────────────────┐                       │
│  │  int __wrap_read_sensor(int channel) {       │                       │
│  │      check_expected(channel);  // Verificar  │                       │
│  │      return mock();            // Retornar   │                       │
│  │  }                             // valor fake │                       │
│  └──────────────────────────────────────────────┘                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.2 will_return — definir el valor de retorno del mock

```c
// En el test, ANTES de llamar al codigo que usa la funcion mockeada:
will_return(function_name, return_value);

// Para multiples llamadas:
will_return(function_name, first_return);
will_return(function_name, second_return);
will_return(function_name, third_return);

// Para que siempre retorne lo mismo:
will_return_always(function_name, value);

// Para que retorne un valor N veces:
will_return_count(function_name, value, count);
```

### 6.3 Ejemplo completo de mocking

Supongamos que tenemos un servicio que lee de un sensor:

```c
// sensor.h
int read_sensor(int channel);

// temperature.h / temperature.c
#include "sensor.h"

typedef enum {
    TEMP_OK,
    TEMP_TOO_HOT,
    TEMP_TOO_COLD,
    TEMP_SENSOR_ERROR
} TempStatus;

TempStatus check_temperature(int channel) {
    int raw = read_sensor(channel);  // ← Esta es la dependencia que queremos mockear

    if (raw < 0) return TEMP_SENSOR_ERROR;
    if (raw < 10) return TEMP_TOO_COLD;
    if (raw > 40) return TEMP_TOO_HOT;
    return TEMP_OK;
}
```

```c
// test_temperature.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "temperature.h"

// ── Mock de read_sensor ─────────────────────────────────────

// Esta funcion reemplaza a read_sensor() gracias a --wrap
int __wrap_read_sensor(int channel) {
    check_expected(channel);     // Verificar que se llamo con el channel esperado
    return mock_type(int);       // Retornar el valor configurado con will_return
}

// ── Tests ───────────────────────────────────────────────────

static void test_temp_ok(void **state) {
    (void)state;

    // Configurar expectativas:
    // "cuando llamen a read_sensor, espero channel=0, retorna 25"
    expect_value(read_sensor, channel, 0);
    will_return(read_sensor, 25);

    TempStatus status = check_temperature(0);

    assert_int_equal(TEMP_OK, status);
}

static void test_temp_too_hot(void **state) {
    (void)state;

    expect_value(read_sensor, channel, 0);
    will_return(read_sensor, 45);  // 45 > 40

    assert_int_equal(TEMP_TOO_HOT, check_temperature(0));
}

static void test_temp_too_cold(void **state) {
    (void)state;

    expect_value(read_sensor, channel, 1);
    will_return(read_sensor, 5);   // 5 < 10

    assert_int_equal(TEMP_TOO_COLD, check_temperature(1));
}

static void test_temp_sensor_error(void **state) {
    (void)state;

    expect_value(read_sensor, channel, 0);
    will_return(read_sensor, -1);  // Error del sensor

    assert_int_equal(TEMP_SENSOR_ERROR, check_temperature(0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_temp_ok),
        cmocka_unit_test(test_temp_too_hot),
        cmocka_unit_test(test_temp_too_cold),
        cmocka_unit_test(test_temp_sensor_error),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

```bash
# CLAVE: compilar con --wrap para redirigir read_sensor a __wrap_read_sensor
$ gcc -Wl,--wrap=read_sensor \
      -o test_temp test_temperature.c temperature.c -lcmocka
$ ./test_temp
[==========] Running 4 test(s).
[ RUN      ] test_temp_ok
[       OK ] test_temp_ok
[ RUN      ] test_temp_too_hot
[       OK ] test_temp_too_hot
[ RUN      ] test_temp_too_cold
[       OK ] test_temp_too_cold
[ RUN      ] test_temp_sensor_error
[       OK ] test_temp_sensor_error
[==========] 4 test(s) run.
[  PASSED  ] 4 test(s).
```

### 6.4 expect_* — verificar parametros de la llamada

```c
// Verificar que un parametro tiene un valor exacto
expect_value(function_name, param_name, expected_value);

// Verificar que un parametro NO tiene un valor
expect_not_value(function_name, param_name, unexpected_value);

// Verificar que un parametro esta en un rango
expect_in_range(function_name, param_name, min, max);

// Verificar que un string es igual
expect_string(function_name, param_name, expected_string);

// Verificar que un puntero apunta a la memoria esperada
expect_memory(function_name, param_name, expected_data, size);

// Aceptar CUALQUIER valor (no verificar)
expect_any(function_name, param_name);

// Verificar siempre (para todas las llamadas)
expect_any_always(function_name, param_name);

// Verificar N veces
expect_any_count(function_name, param_name, count);
```

### 6.5 Ejemplo avanzado — mock de filesystem

```c
// file_utils.h
int read_config_value(const char *path, const char *key, char *value, size_t value_len);

// file_utils.c
#include <stdio.h>
#include <string.h>

// Esta funcion abre un archivo, busca key=value, y retorna el valor
int read_config_value(const char *path, const char *key, char *value, size_t value_len) {
    FILE *f = fopen(path, "r");        // ← queremos mockear fopen
    if (f == NULL) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {  // ← queremos mockear fgets
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) {
            // Quitar newline
            char *nl = strchr(eq + 1, '\n');
            if (nl) *nl = '\0';
            strncpy(value, eq + 1, value_len - 1);
            value[value_len - 1] = '\0';
            fclose(f);                               // ← queremos mockear fclose
            return 0;
        }
    }

    fclose(f);
    return -1;  // Key not found
}
```

```c
// test_file_utils.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

// Declarar el prototipo del codigo bajo test
int read_config_value(const char *path, const char *key, char *value, size_t value_len);

// ── Mocks ───────────────────────────────────────────────────

FILE *__wrap_fopen(const char *path, const char *mode) {
    check_expected(path);
    check_expected(mode);
    return mock_ptr_type(FILE *);
}

char *__wrap_fgets(char *s, int size, FILE *stream) {
    (void)size;
    (void)stream;
    char *line = mock_ptr_type(char *);
    if (line == NULL) return NULL;
    strcpy(s, line);
    return s;
}

int __wrap_fclose(FILE *stream) {
    (void)stream;
    return 0;
}

// ── Tests ───────────────────────────────────────────────────

static void test_read_config_key_found(void **state) {
    (void)state;

    // Configurar fopen: espera "/etc/app.conf" en modo "r", retorna fake FILE*
    expect_string(fopen, path, "/etc/app.conf");
    expect_string(fopen, mode, "r");
    will_return(fopen, (FILE *)0x1234);  // Puntero fake (no se usa realmente)

    // Configurar fgets: retorna lineas del "archivo" una por una
    will_return(fgets, "# comment\n");        // Primera llamada
    will_return(fgets, "host=localhost\n");     // Segunda llamada
    will_return(fgets, "port=8080\n");          // Tercera llamada
    // read_config_value encontrara "port" y dejara de leer

    char value[64];
    int rc = read_config_value("/etc/app.conf", "port", value, sizeof(value));

    assert_int_equal(0, rc);
    assert_string_equal("8080", value);
}

static void test_read_config_file_not_found(void **state) {
    (void)state;

    expect_string(fopen, path, "/no/existe");
    expect_string(fopen, mode, "r");
    will_return(fopen, NULL);  // fopen falla

    char value[64];
    int rc = read_config_value("/no/existe", "key", value, sizeof(value));

    assert_int_equal(-1, rc);
}

static void test_read_config_key_not_found(void **state) {
    (void)state;

    expect_string(fopen, path, "/etc/app.conf");
    expect_string(fopen, mode, "r");
    will_return(fopen, (FILE *)0x1234);

    will_return(fgets, "host=localhost\n");
    will_return(fgets, "port=8080\n");
    will_return(fgets, NULL);  // EOF — key "missing" no esta

    char value[64];
    int rc = read_config_value("/etc/app.conf", "missing", value, sizeof(value));

    assert_int_equal(-1, rc);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_read_config_key_found),
        cmocka_unit_test(test_read_config_file_not_found),
        cmocka_unit_test(test_read_config_key_not_found),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

```bash
$ gcc -Wl,--wrap=fopen,--wrap=fgets,--wrap=fclose \
      -o test_file_utils test_file_utils.c file_utils.c -lcmocka
$ ./test_file_utils
[==========] Running 3 test(s).
[ RUN      ] test_read_config_key_found
[       OK ] test_read_config_key_found
[ RUN      ] test_read_config_file_not_found
[       OK ] test_read_config_file_not_found
[ RUN      ] test_read_config_key_not_found
[       OK ] test_read_config_key_not_found
[==========] 3 test(s) run.
[  PASSED  ] 3 test(s).
```

### 6.6 mock() vs mock_type() vs mock_ptr_type()

```c
// mock() — retorna LargestIntegralType (uintmax_t)
// Usar para int, bool, enum, etc.
int __wrap_read_sensor(int ch) {
    return mock();  // Retorna como uintmax_t, se castea a int
}

// mock_type(type) — retorna con cast explicito
int __wrap_get_count(void) {
    return mock_type(int);
}

// mock_ptr_type(type) — retorna un puntero
char *__wrap_get_name(int id) {
    return mock_ptr_type(char *);
}

FILE *__wrap_fopen(const char *path, const char *mode) {
    return mock_ptr_type(FILE *);
}
```

---

## 7. Leak detection

### 7.1 CMocka detecta leaks automaticamente

CMocka redefine `malloc`, `calloc`, `realloc`, y `free` dentro de los tests. Si un test alloca memoria y no la libera, CMocka lo detecta:

```c
static void test_with_leak(void **state) {
    (void)state;
    char *p = test_malloc(100);  // Usar test_malloc en vez de malloc
    strcpy(p, "hello");
    assert_string_equal("hello", p);
    // ¡No llamamos test_free(p)! → CMocka lo detecta
}
```

```bash
$ ./test_leak
[==========] Running 1 test(s).
[ RUN      ] test_with_leak
[  ERROR   ] --- Blocks allocated...
                  test.c:4: note: block 0x... (100 bytes)
[   LINE   ] --- test.c:4: error: Failure!
[  FAILED  ] test_with_leak
[==========] 1 test(s) run.
[  FAILED  ] 1 test(s).
```

### 7.2 test_malloc vs malloc

Para que CMocka detecte leaks, debes usar las funciones de CMocka para allocation:

```c
// Funciones de CMocka (rastrean allocations)
void *test_malloc(size_t size);
void *test_calloc(size_t nmemb, size_t size);
void *test_realloc(void *ptr, size_t size);
void  test_free(void *ptr);

// Si quieres que TODO tu codigo use las versiones rastreadas,
// puedes redefinir las macros ANTES de incluir tu codigo:
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#include "my_module.c"  // Ahora malloc/free dentro de my_module son rastreados
```

### 7.3 Compilar con soporte de leak detection

```bash
# Opcion 1: usar test_malloc directamente en los tests
$ gcc -o test test.c -lcmocka

# Opcion 2: redefinir malloc globalmente via preprocesador
$ gcc -Dmalloc=test_malloc -Dfree=test_free \
      -Dcalloc=test_calloc -Drealloc=test_realloc \
      -o test test.c my_module.c -lcmocka
```

---

## 8. Test groups — organizar suites

### 8.1 Multiples grupos en un solo ejecutable

```c
// test_all.c — multiples grupos de tests

// ── Stack tests ─────────────────────────────────────────────

static int stack_setup(void **state) {
    *state = stack_new(4);
    return 0;
}

static int stack_teardown(void **state) {
    stack_free(*state);
    return 0;
}

static void test_stack_push(void **state) {
    Stack *s = *state;
    stack_push(s, 42);
    assert_int_equal(1, stack_len(s));
}

static void test_stack_pop(void **state) {
    Stack *s = *state;
    stack_push(s, 42);
    int val;
    stack_pop(s, &val);
    assert_int_equal(42, val);
}

// ── Queue tests ─────────────────────────────────────────────

static int queue_setup(void **state) {
    *state = queue_new(4);
    return 0;
}

static int queue_teardown(void **state) {
    queue_free(*state);
    return 0;
}

static void test_queue_enqueue(void **state) {
    Queue *q = *state;
    queue_enqueue(q, 42);
    assert_int_equal(1, queue_len(q));
}

static void test_queue_fifo(void **state) {
    Queue *q = *state;
    queue_enqueue(q, 10);
    queue_enqueue(q, 20);
    int val;
    queue_dequeue(q, &val);
    assert_int_equal(10, val);  // FIFO: first in, first out
}

// ── main — ejecutar ambos grupos ────────────────────────────

int main(void) {
    int failures = 0;

    // Grupo 1: Stack
    const struct CMUnitTest stack_tests[] = {
        cmocka_unit_test_setup_teardown(test_stack_push, stack_setup, stack_teardown),
        cmocka_unit_test_setup_teardown(test_stack_pop, stack_setup, stack_teardown),
    };
    failures += cmocka_run_group_tests(stack_tests, NULL, NULL);

    // Grupo 2: Queue
    const struct CMUnitTest queue_tests[] = {
        cmocka_unit_test_setup_teardown(test_queue_enqueue, queue_setup, queue_teardown),
        cmocka_unit_test_setup_teardown(test_queue_fifo, queue_setup, queue_teardown),
    };
    failures += cmocka_run_group_tests(queue_tests, NULL, NULL);

    return failures;
}
```

```bash
$ ./test_all
[==========] Running 2 test(s).
[ RUN      ] test_stack_push
[       OK ] test_stack_push
[ RUN      ] test_stack_pop
[       OK ] test_stack_pop
[==========] 2 test(s) run.
[  PASSED  ] 2 test(s).
[==========] Running 2 test(s).
[ RUN      ] test_queue_enqueue
[       OK ] test_queue_enqueue
[ RUN      ] test_queue_fifo
[       OK ] test_queue_fifo
[==========] 2 test(s) run.
[  PASSED  ] 2 test(s).
```

### 8.2 Nombrar grupos

CMocka 1.1+ permite nombrar los grupos para mejor output:

```c
// cmocka_run_group_tests_name
failures += cmocka_run_group_tests_name("Stack", stack_tests, NULL, NULL);
failures += cmocka_run_group_tests_name("Queue", queue_tests, NULL, NULL);
```

---

## 9. Output formats

### 9.1 Formatos disponibles

CMocka soporta multiples formatos de output configurables via variable de entorno:

```bash
# Formato por defecto (human-readable)
$ ./test_suite
[==========] Running 3 test(s).
[ RUN      ] test_something
[       OK ] test_something
...

# TAP (Test Anything Protocol — parseable por CI)
$ CMOCKA_MESSAGE_OUTPUT=TAP ./test_suite
1..3
ok 1 - test_something
ok 2 - test_another
not ok 3 - test_broken

# Subunit (para integracion con testrepository)
$ CMOCKA_MESSAGE_OUTPUT=SUBUNIT ./test_suite

# XML (JUnit-compatible — para Jenkins, GitLab CI, etc.)
$ CMOCKA_XML_FILE=results.xml ./test_suite
$ cat results.xml
<?xml version="1.0" encoding="UTF-8" ?>
<testsuites>
  <testsuite name="tests" tests="3" failures="1" ...>
    <testcase name="test_something" .../>
    <testcase name="test_another" .../>
    <testcase name="test_broken" ...>
      <failure>...</failure>
    </testcase>
  </testsuite>
</testsuites>
```

### 9.2 Integracion con CI

```yaml
# .github/workflows/test.yml
steps:
  - run: |
      CMOCKA_XML_FILE=test-results.xml ./test_suite
  - uses: actions/upload-artifact@v3
    with:
      name: test-results
      path: test-results.xml
```

---

## 10. Integracion con Make y CMake

### 10.1 Makefile

```makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g
LDFLAGS  = -lcmocka
SRCDIR   = src
TESTDIR  = tests
BUILDDIR = build

PROD_SRC = $(filter-out $(SRCDIR)/main.c, $(wildcard $(SRCDIR)/*.c))
PROD_OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(PROD_SRC))

TEST_SRC  = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/test_%.c, $(BUILDDIR)/test_%, $(TEST_SRC))

.PHONY: all test test-verbose test-tap clean

all: $(BUILDDIR)/program

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/program: $(SRCDIR)/main.c $(PROD_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Tests sin mocking
$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(PROD_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^ $(LDFLAGS)

# Tests CON mocking (necesitan --wrap)
# Ejemplo: test_temperature necesita --wrap=read_sensor
$(BUILDDIR)/test_temperature: $(TESTDIR)/test_temperature.c $(PROD_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -Wl,--wrap=read_sensor -o $@ $^ $(LDFLAGS)

test: $(TEST_BINS)
	@pass=0; fail=0; \
	for t in $(TEST_BINS); do \
		name=$$(basename $$t); \
		./$$t > /dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			printf "  \033[32m[PASS]\033[0m %s\n" "$$name"; \
			pass=$$((pass + 1)); \
		else \
			printf "  \033[31m[FAIL]\033[0m %s\n" "$$name"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; echo "$$pass passed, $$fail failed"; [ $$fail -eq 0 ]

test-verbose: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "═══ $$(basename $$t) ═══"; \
		./$$t; echo ""; \
	done

test-tap: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		CMOCKA_MESSAGE_OUTPUT=TAP ./$$t; \
	done

clean:
	rm -rf $(BUILDDIR)
```

### 10.2 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(mi_proyecto C)

set(CMAKE_C_STANDARD 11)

# Buscar CMocka instalado en el sistema
find_package(cmocka REQUIRED)

# Libreria de produccion
add_library(mylib src/stack.c src/hash_table.c src/temperature.c)
target_include_directories(mylib PUBLIC src)

# Ejecutable principal
add_executable(program src/main.c)
target_link_libraries(program mylib)

# ── Tests ──────────────────────────────────────────────────
enable_testing()

# Test sin mocking
function(add_cmocka_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} mylib cmocka::cmocka)
    target_include_directories(${test_name} PRIVATE src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_cmocka_test(test_stack tests/test_stack.c)
add_cmocka_test(test_hash_table tests/test_hash_table.c)

# Test CON mocking (--wrap)
add_executable(test_temperature tests/test_temperature.c)
target_link_libraries(test_temperature mylib cmocka::cmocka)
target_link_options(test_temperature PRIVATE "LINKER:--wrap=read_sensor")
add_test(NAME test_temperature COMMAND test_temperature)
```

```bash
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
$ ctest --output-on-failure
```

---

## 11. Comparacion con Rust y Go

### 11.1 Mocking — el mismo concepto en tres lenguajes

**C con CMocka (--wrap):**

```c
// Mock: reemplazar read_sensor en link-time
int __wrap_read_sensor(int ch) {
    check_expected(ch);
    return mock_type(int);
}

static void test_temp_ok(void **state) {
    (void)state;
    expect_value(read_sensor, channel, 0);
    will_return(read_sensor, 25);

    assert_int_equal(TEMP_OK, check_temperature(0));
}

// Compilar: gcc -Wl,--wrap=read_sensor ...
```

**Rust con mockall:**

```rust
#[cfg_attr(test, mockall::automock)]
trait Sensor {
    fn read(&self, channel: u32) -> i32;
}

fn check_temperature(sensor: &dyn Sensor, channel: u32) -> TempStatus {
    let raw = sensor.read(channel);
    // ...
}

#[test]
fn test_temp_ok() {
    let mut mock = MockSensor::new();
    mock.expect_read()
        .with(eq(0))
        .returning(|_| 25);

    assert_eq!(TempStatus::Ok, check_temperature(&mock, 0));
}
```

**Go (interface + mock manual):**

```go
type Sensor interface {
    Read(channel int) int
}

type mockSensor struct {
    returnValue int
}

func (m *mockSensor) Read(channel int) int {
    return m.returnValue
}

func TestTempOk(t *testing.T) {
    mock := &mockSensor{returnValue: 25}
    status := CheckTemperature(mock, 0)
    if status != TempOK {
        t.Errorf("got %v, want TempOK", status)
    }
}
```

### 11.2 Tabla comparativa

| Aspecto | CMocka (C) | mockall (Rust) | Go mock manual |
|---------|-----------|---------------|----------------|
| **Mecanismo** | Link-time wrapping (`--wrap`) | Trait + generacion de codigo | Interface + struct manual |
| **Invasividad** | Baja — no modifica el codigo de produccion | Media — requiere trait (buen diseno de todas formas) | Baja — requiere interface (buen diseno de todas formas) |
| **Verificacion de parametros** | `expect_value`, `expect_string`, etc. | `.with(eq(...))` | Manual (`if param != expected`) |
| **Valor de retorno** | `will_return` | `.returning(closure)` | Campo en el mock struct |
| **Verificacion de llamadas** | Automatica (falla si no se consume expect) | `.times(n)` | Manual (contador) |
| **Requiere compilacion especial** | Si (`-Wl,--wrap=...`) | No | No |

---

## 12. Ejemplo completo — Cache con mocking

### 12.1 El modulo bajo test

```c
// cache.h
#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include <time.h>

typedef struct Cache Cache;

Cache      *cache_new(int max_entries, int ttl_seconds);
void        cache_free(Cache *c);
bool        cache_set(Cache *c, const char *key, const char *value);
const char *cache_get(Cache *c, const char *key);  // NULL si no existe o expiro
bool        cache_delete(Cache *c, const char *key);
int         cache_count(const Cache *c);
void        cache_purge_expired(Cache *c);

#endif
```

```c
// cache.c
#include "cache.h"
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    char *key;
    char *value;
    time_t expires_at;
} Entry;

struct Cache {
    Entry *entries;
    int count;
    int capacity;
    int ttl;
};

Cache *cache_new(int max_entries, int ttl_seconds) {
    Cache *c = calloc(1, sizeof(Cache));
    if (!c) return NULL;
    c->entries = calloc(max_entries, sizeof(Entry));
    if (!c->entries) { free(c); return NULL; }
    c->capacity = max_entries;
    c->ttl = ttl_seconds;
    return c;
}

void cache_free(Cache *c) {
    if (!c) return;
    for (int i = 0; i < c->count; i++) {
        free(c->entries[i].key);
        free(c->entries[i].value);
    }
    free(c->entries);
    free(c);
}

bool cache_set(Cache *c, const char *key, const char *value) {
    if (!c || !key || !value) return false;
    if (c->count >= c->capacity) return false;

    // Buscar si ya existe
    for (int i = 0; i < c->count; i++) {
        if (strcmp(c->entries[i].key, key) == 0) {
            free(c->entries[i].value);
            c->entries[i].value = strdup(value);
            c->entries[i].expires_at = time(NULL) + c->ttl;  // ← dependencia de time()
            return true;
        }
    }

    c->entries[c->count].key = strdup(key);
    c->entries[c->count].value = strdup(value);
    c->entries[c->count].expires_at = time(NULL) + c->ttl;   // ← dependencia de time()
    c->count++;
    return true;
}

const char *cache_get(Cache *c, const char *key) {
    if (!c || !key) return NULL;
    for (int i = 0; i < c->count; i++) {
        if (strcmp(c->entries[i].key, key) == 0) {
            if (time(NULL) > c->entries[i].expires_at) {     // ← dependencia de time()
                return NULL;  // Expirado
            }
            return c->entries[i].value;
        }
    }
    return NULL;
}

int cache_count(const Cache *c) {
    return c ? c->count : 0;
}
```

### 12.2 Los tests con mock de time()

```c
// test_cache.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "cache.h"

// ── Mock de time() ──────────────────────────────────────────

time_t __wrap_time(time_t *tloc) {
    time_t fake = mock_type(time_t);
    if (tloc) *tloc = fake;
    return fake;
}

// ── Fixture ─────────────────────────────────────────────────

static int cache_setup(void **state) {
    Cache *c = cache_new(10, 60);  // 10 entries, 60s TTL
    assert_non_null(c);
    *state = c;
    return 0;
}

static int cache_teardown(void **state) {
    cache_free(*state);
    return 0;
}

// ── Tests ───────────────────────────────────────────────────

static void test_cache_set_and_get(void **state) {
    Cache *c = *state;

    // time() se llama en cache_set
    will_return(__wrap_time, 1000);

    cache_set(c, "key", "value");

    // time() se llama en cache_get (para verificar expiracion)
    will_return(__wrap_time, 1030);  // 30s despues — dentro del TTL de 60s

    assert_string_equal("value", cache_get(c, "key"));
}

static void test_cache_entry_expires(void **state) {
    Cache *c = *state;

    // Set a tiempo 1000 (expira a 1060)
    will_return(__wrap_time, 1000);
    cache_set(c, "key", "value");

    // Get a tiempo 1061 — expirado
    will_return(__wrap_time, 1061);
    assert_null(cache_get(c, "key"));
}

static void test_cache_entry_valid_at_boundary(void **state) {
    Cache *c = *state;

    // Set a tiempo 1000 (expira a 1060)
    will_return(__wrap_time, 1000);
    cache_set(c, "key", "value");

    // Get a tiempo 1060 — justo en el limite (no expirado, uses >)
    will_return(__wrap_time, 1060);
    assert_string_equal("value", cache_get(c, "key"));
}

static void test_cache_overwrite_refreshes_ttl(void **state) {
    Cache *c = *state;

    // Primera escritura a t=1000 (expira t=1060)
    will_return(__wrap_time, 1000);
    cache_set(c, "key", "first");

    // Sobreescritura a t=1050 (nuevo expiry t=1110)
    will_return(__wrap_time, 1050);
    cache_set(c, "key", "second");

    // Lectura a t=1080 — el primer TTL habria expirado, pero la sobreescritura lo renovo
    will_return(__wrap_time, 1080);
    assert_string_equal("second", cache_get(c, "key"));
}

static void test_cache_get_missing_key(void **state) {
    Cache *c = *state;

    // No se llama a time() porque la key no existe (el loop no encuentra match)
    assert_null(cache_get(c, "ghost"));
}

static void test_cache_count(void **state) {
    Cache *c = *state;

    assert_int_equal(0, cache_count(c));

    will_return(__wrap_time, 1000);
    cache_set(c, "a", "1");
    will_return(__wrap_time, 1000);
    cache_set(c, "b", "2");

    assert_int_equal(2, cache_count(c));
}

// ── Runner ──────────────────────────────────────────────────

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_cache_set_and_get, cache_setup, cache_teardown),
        cmocka_unit_test_setup_teardown(test_cache_entry_expires, cache_setup, cache_teardown),
        cmocka_unit_test_setup_teardown(test_cache_entry_valid_at_boundary, cache_setup, cache_teardown),
        cmocka_unit_test_setup_teardown(test_cache_overwrite_refreshes_ttl, cache_setup, cache_teardown),
        cmocka_unit_test_setup_teardown(test_cache_get_missing_key, cache_setup, cache_teardown),
        cmocka_unit_test_setup_teardown(test_cache_count, cache_setup, cache_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -g \
      -Wl,--wrap=time \
      -o test_cache test_cache.c cache.c -lcmocka
$ ./test_cache
[==========] Running 6 test(s).
[ RUN      ] test_cache_set_and_get
[       OK ] test_cache_set_and_get
[ RUN      ] test_cache_entry_expires
[       OK ] test_cache_entry_expires
[ RUN      ] test_cache_entry_valid_at_boundary
[       OK ] test_cache_entry_valid_at_boundary
[ RUN      ] test_cache_overwrite_refreshes_ttl
[       OK ] test_cache_overwrite_refreshes_ttl
[ RUN      ] test_cache_get_missing_key
[       OK ] test_cache_get_missing_key
[ RUN      ] test_cache_count
[       OK ] test_cache_count
[==========] 6 test(s) run.
[  PASSED  ] 6 test(s).
```

Sin el mock de `time()`, tendrias que usar `sleep(61)` para verificar expiracion. Con el mock, cada test es instantaneo.

---

## 13. Programa de practica

### Objetivo

Escribir tests con CMocka para un **HTTP client simplificado** que depende de funciones de socket.

### Especificacion

```c
// http_client.h
typedef struct {
    int status_code;
    char *body;
    size_t body_len;
} HttpResponse;

// Hace un GET request. Retorna 0 en exito, -1 en error.
int http_get(const char *host, int port, const char *path, HttpResponse *resp);
void http_response_free(HttpResponse *resp);
```

```c
// http_client.c — usa socket(), connect(), send(), recv() internamente
#include <sys/socket.h>
#include <netdb.h>
int http_get(const char *host, int port, const char *path, HttpResponse *resp) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);  // ← mockear
    if (fd < 0) return -1;

    // ... connect(), send(), recv() ...          ← mockear

    close(fd);                                    // ← mockear
    return 0;
}
```

### Tests que escribir con CMocka + --wrap

1. **test_http_get_success** — mock socket/connect/send/recv para retornar "HTTP/1.1 200 OK\r\n\r\nHello"
2. **test_http_get_socket_fails** — mock socket retorna -1, verificar que http_get retorna -1
3. **test_http_get_connect_fails** — mock connect retorna -1
4. **test_http_get_404** — mock recv retorna "HTTP/1.1 404 Not Found\r\n\r\n"
5. **test_http_get_timeout** — mock recv retorna -1 con errno=ETIMEDOUT

Compilar con: `gcc -Wl,--wrap=socket,--wrap=connect,--wrap=send,--wrap=recv,--wrap=close ...`

---

## 14. Ejercicios

### Ejercicio 1: Migrar tests de Unity a CMocka

Toma los tests del hash table de S02/T01 (Unity) y reescribelos con CMocka. Usa `void **state` para la fixture en vez de variables globales. Usa group setup/teardown si tiene sentido.

### Ejercicio 2: Mock de malloc

Escribe tests para una funcion `Buffer *buffer_new(size_t size)` que usa malloc internamente. Usa `--wrap=malloc` para simular fallo de malloc y verificar que `buffer_new` retorna NULL sin crashear.

### Ejercicio 3: Test con TAP output

Escribe una suite de tests CMocka para alguna estructura de datos y ejecutala con `CMOCKA_MESSAGE_OUTPUT=TAP`. Parsea el output TAP con un script bash que cuente los ok/not ok.

### Ejercicio 4: Leak detection

Escribe un test que deliberadamente tiene un memory leak (usa `test_malloc` sin `test_free`) y verifica que CMocka lo detecta. Luego corrige el leak y verifica que el test pasa.

---

## 15. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        CMOCKA — MAPA MENTAL                                  │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ¿Que es?                                                                     │
│  └─ Framework de testing + mocking para C (evolucion de cmockery de Google)   │
│                                                                                │
│  Instalacion:                                                                 │
│  └─ apt install libcmocka-dev (o compilar desde fuente con cmake)             │
│                                                                                │
│  Estructura:                                                                  │
│  ├─ #include <stdarg.h> + <stddef.h> + <setjmp.h> + <cmocka.h>              │
│  ├─ Tests reciben void **state (fixture)                                     │
│  ├─ cmocka_unit_test_setup_teardown(test, setup, teardown)                   │
│  └─ cmocka_run_group_tests(tests, group_setup, group_teardown)               │
│                                                                                │
│  Assertions:                                                                  │
│  ├─ assert_true/false, assert_null/non_null                                  │
│  ├─ assert_int_equal/not_equal                                               │
│  ├─ assert_string_equal, assert_memory_equal                                 │
│  ├─ assert_in_range, assert_return_code                                      │
│  └─ skip(), fail_msg()                                                       │
│                                                                                │
│  Fixtures:                                                                    │
│  ├─ Per-test: setup/teardown reciben void **state                            │
│  ├─ Per-group: group_setup/group_teardown en cmocka_run_group_tests          │
│  └─ La fixture se pasa como parametro, no como variable global               │
│                                                                                │
│  Mocking:                                                                     │
│  ├─ --wrap flag: gcc -Wl,--wrap=func_name                                   │
│  ├─ __wrap_func_name reemplaza a func_name                                   │
│  ├─ will_return(func, value) — definir retorno del mock                      │
│  ├─ expect_value(func, param, value) — verificar parametros                  │
│  ├─ check_expected(param) — dentro del mock, consumir la expectativa         │
│  └─ mock_type(type) — dentro del mock, obtener el valor de will_return       │
│                                                                                │
│  Extras:                                                                      │
│  ├─ Leak detection (test_malloc/test_free)                                   │
│  ├─ Output: default, TAP, subunit, XML (JUnit)                              │
│  └─ Integracion con CMake (find_package) y Make (-lcmocka)                   │
│                                                                                │
│  vs Unity:                                                                    │
│  ├─ CMocka: mocking built-in, fixtures con state, group setup, leak detect   │
│  └─ Unity: mas assertions tipadas, mejor para embedded, cero dependencias    │
│                                                                                │
│  Proximo topico: T03 — CTest con CMake                                       │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: T01 — Unity | Siguiente: T03 — CTest con CMake →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 2 — Frameworks de Testing en C > Topico 2 — CMocka*
