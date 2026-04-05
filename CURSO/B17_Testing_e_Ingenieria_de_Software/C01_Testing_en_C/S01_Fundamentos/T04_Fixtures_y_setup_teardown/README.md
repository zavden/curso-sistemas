# Fixtures y setup/teardown — inicializacion compartida, cleanup con goto o funcion, simular constructores/destructores

## 1. Introduccion

En T02 introdujimos el concepto de helpers (`setup()`/`teardown()`) para reducir el boilerplate en los tests. Este topico profundiza en ese patron — las **fixtures** — y aborda el problema mas critico del testing en C: **garantizar la limpieza de recursos cuando un test falla a mitad de ejecucion**.

C no tiene constructores, destructores, RAII, `defer`, ni garbage collector. Si un test hace `malloc` y luego falla en un assert antes de llegar al `free`, tienes un memory leak. En un solo test eso es trivial, pero en una suite de 500 tests ejecutandose en el mismo proceso, los leaks se acumulan y pueden causar fallos espurios o enmascarar bugs reales.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│              EL PROBLEMA DEL CLEANUP EN TESTS EN C                              │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Rust:   fn test() {                     Go:    func Test(t *testing.T) {       │
│              let v = Vec::new();                    f := createTempFile()        │
│              // ... test ...                        t.Cleanup(func() {           │
│          }  // ← Drop llama free()                      os.Remove(f)            │
│             //   automaticamente                    })                           │
│                                                     // ... test ...              │
│  Java:   @After                                 }  // Cleanup se ejecuta         │
│          void tearDown() {                         // siempre, incluso si        │
│              db.close();                           // el test falla              │
│          }                                                                       │
│                                                                                  │
│  C:      void test() {                                                          │
│              int *p = malloc(100);                                               │
│              assert(p != NULL);       // Si esto falla...                       │
│              do_something(p);                                                    │
│              assert(result == 42);    // ...o esto falla...                     │
│              free(p);                 // ...free() NUNCA se ejecuta             │
│          }                            // → memory leak                          │
│                                                                                  │
│  Soluciones en C:                                                               │
│  1. Funciones setup()/teardown() llamadas por el runner                         │
│  2. Patron goto cleanup                                                         │
│  3. Macro wrapper que garantiza cleanup                                         │
│  4. fork() — cada test en su propio proceso (el OS limpia todo)                │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Que es una fixture

Una **fixture** (del ingles "accesorio fijo") es el **estado predefinido** que un test necesita para ejecutarse. Incluye:

- Objetos creados (structs, arrays, buffers)
- Recursos del sistema (archivos abiertos, sockets, memoria mapeada)
- Estado de la aplicacion (base de datos con datos de prueba, configuracion cargada)
- Condiciones del entorno (directorio temporal, variables de entorno)

| Termino | Significado |
|---------|-------------|
| **Fixture** | El conjunto de estado necesario para el test |
| **Setup** | La funcion que crea la fixture (inicializacion) |
| **Teardown** | La funcion que destruye la fixture (limpieza) |
| **Test context** / **Test environment** | El struct que contiene toda la fixture |

### 1.2 El ciclo de vida de un test con fixture

```
┌─────────────────────────────────────────────────────────────────┐
│                    CICLO DE VIDA                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Para CADA test:                                               │
│                                                                 │
│  ┌──────────┐     ┌──────────┐     ┌───────────┐              │
│  │  SETUP   │────→│   TEST   │────→│ TEARDOWN  │              │
│  │          │     │          │     │           │              │
│  │ malloc() │     │ Act      │     │ free()    │              │
│  │ fopen()  │     │ Assert   │     │ fclose()  │              │
│  │ connect()│     │          │     │ close()   │              │
│  │ mkdtemp()│     │          │     │ rmdir()   │              │
│  └──────────┘     └──────────┘     └───────────┘              │
│                        │                                        │
│                    ¿Que pasa si                                 │
│                    falla aqui?                                  │
│                        │                                        │
│                   ┌────▼────┐                                   │
│                   │ assert  │                                   │
│                   │ falla!  │                                   │
│                   └────┬────┘                                   │
│                        │                                        │
│           ┌────────────┼────────────┐                           │
│           │            │            │                           │
│     Con assert()  Con soft-    Con runner                       │
│     (abort)       assert       que llama                        │
│           │       (return)     teardown()                       │
│           │            │            │                           │
│     TEARDOWN      TEARDOWN     TEARDOWN                        │
│     NO se         NO se        SI se                           │
│     ejecuta       ejecuta*     ejecuta                         │
│                                                                 │
│  *A menos que uses goto cleanup o wrapper                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Setup y teardown como funciones

### 2.1 Patron basico — llamada explicita

El patron mas simple: dos funciones que el test llama al inicio y al final.

```c
// test_hash_table.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"
#include "test_helpers.h"

// ── Fixture ─────────────────────────────────────────────────

typedef struct {
    HashTable *ht;
    char *test_key;
    char *test_value;
} HtFixture;

static HtFixture setup(void) {
    HtFixture f = {0};
    f.ht = ht_new(16);
    f.test_key = strdup("test_key");
    f.test_value = strdup("test_value");
    return f;
}

static void teardown(HtFixture *f) {
    ht_free(f->ht);
    free(f->test_key);
    free(f->test_value);
    memset(f, 0, sizeof(*f));  // Poner a cero para detectar use-after-teardown
}

// ── Tests ───────────────────────────────────────────────────

void test_ht_insert_and_get(void) {
    HtFixture f = setup();

    ht_insert(f.ht, f.test_key, f.test_value);
    const char *val = ht_get(f.ht, f.test_key);

    ASSERT_STR_EQ(val, "test_value");

    teardown(&f);
}

void test_ht_count_after_insert(void) {
    HtFixture f = setup();

    ASSERT_EQ(ht_count(f.ht), 0);
    ht_insert(f.ht, f.test_key, f.test_value);
    ASSERT_EQ(ht_count(f.ht), 1);

    teardown(&f);
}

void test_ht_delete_existing_key(void) {
    HtFixture f = setup();

    ht_insert(f.ht, f.test_key, f.test_value);
    bool ok = ht_delete(f.ht, f.test_key);

    ASSERT_TRUE(ok);
    ASSERT(ht_get(f.ht, f.test_key) == NULL);

    teardown(&f);
}
```

**Problema**: si `ASSERT_STR_EQ` o `ASSERT_EQ` usa `return` para abortar el test en caso de fallo (como hacen la mayoria de los soft-assert que escribimos en T01-T02), el `teardown` **nunca se ejecuta**.

### 2.2 El leak silencioso

```c
void test_ht_insert_and_get(void) {
    HtFixture f = setup();          // malloc x3

    ht_insert(f.ht, f.test_key, f.test_value);
    const char *val = ht_get(f.ht, f.test_key);

    ASSERT_STR_EQ(val, "wrong!");   // ← FALLA, la macro hace return

    teardown(&f);                   // ← NUNCA SE EJECUTA
    // Leak: ht, test_key, test_value, mas toda la memoria interna de ht
}
```

Si ejecutas esta suite con Valgrind o ASan, veras los leaks. En una suite pequena no importa, pero en una suite grande los leaks acumulados pueden:

1. Agotar la memoria (OOM killer mata el proceso de test)
2. Causar que tests posteriores fallen por falta de memoria
3. Oscurecer leaks reales en el codigo de produccion (pierdes la señal entre el ruido)

---

## 3. Patron goto cleanup — el cleanup garantizado de C

### 3.1 El patron

`goto` tiene mala reputacion en la programacion general, pero en C tiene **un uso legitimo y ampliamente aceptado**: saltar a un bloque de cleanup al final de la funcion. El kernel de Linux lo usa extensivamente.

```c
void test_ht_insert_and_get(void) {
    HtFixture f = setup();
    int result = 0;  // 0 = pass, 1 = fail

    ht_insert(f.ht, f.test_key, f.test_value);
    const char *val = ht_get(f.ht, f.test_key);

    if (strcmp(val, "test_value") != 0) {
        fprintf(stderr, "  FAIL: expected 'test_value', got '%s'\n", val);
        result = 1;
        goto cleanup;
    }

    printf("  [PASS] ht_insert_and_get\n");

cleanup:
    teardown(&f);
    // result se podria usar para contabilizar fallos
}
```

### 3.2 Macros CHECK con goto

Podemos crear macros que combinen la verificacion y el salto a cleanup:

```c
// test_helpers_goto.h
#ifndef TEST_HELPERS_GOTO_H
#define TEST_HELPERS_GOTO_H

#include <stdio.h>
#include <string.h>

static int _test_pass = 0;
static int _test_fail = 0;

// CHECK — si falla, salta a 'cleanup' (label que DEBE existir en la funcion)
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); \
            _test_fail++; \
            goto cleanup; \
        } \
        _test_pass++; \
    } while (0)

#define CHECK_EQ(got, expected) \
    do { \
        long _g = (long)(got), _e = (long)(expected); \
        if (_g != _e) { \
            fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                    __FILE__, __LINE__, _e, _g); \
            _test_fail++; \
            goto cleanup; \
        } \
        _test_pass++; \
    } while (0)

#define CHECK_STR_EQ(got, expected) \
    do { \
        const char *_g = (got), *_e = (expected); \
        if (_g == NULL || _e == NULL || strcmp(_g, _e) != 0) { \
            fprintf(stderr, "  FAIL [%s:%d] expected \"%s\", got \"%s\"\n", \
                    __FILE__, __LINE__, _e ? _e : "(null)", _g ? _g : "(null)"); \
            _test_fail++; \
            goto cleanup; \
        } \
        _test_pass++; \
    } while (0)

#define CHECK_TRUE(expr)  CHECK(expr)
#define CHECK_FALSE(expr) CHECK(!(expr))

#define RUN(fn) do { \
    int _bf = _test_fail; \
    fn(); \
    printf("  [%s] %s\n", (_test_fail == _bf) ? "PASS" : "FAIL", #fn); \
} while (0)

#define SUMMARY() \
    printf("\n─────────────────────────────────────────\n" \
           "Assertions: %d passed, %d failed\n", _test_pass, _test_fail)

#endif
```

Ahora los tests con cleanup garantizado:

```c
// test_hash_table.c
#include "test_helpers_goto.h"
#include "hash_table.h"
#include <stdlib.h>

typedef struct {
    HashTable *ht;
} Ctx;

static Ctx setup(void) {
    return (Ctx){ .ht = ht_new(16) };
}

static void teardown(Ctx *c) {
    ht_free(c->ht);
}

void test_ht_insert_new_key(void) {
    Ctx c = setup();

    CHECK_TRUE(ht_insert(c.ht, "name", "Alice"));
    CHECK_EQ(ht_count(c.ht), 1);
    CHECK_STR_EQ(ht_get(c.ht, "name"), "Alice");

cleanup:
    teardown(&c);
}

void test_ht_insert_duplicate_overwrites(void) {
    Ctx c = setup();

    ht_insert(c.ht, "name", "Alice");
    ht_insert(c.ht, "name", "Bob");

    CHECK_STR_EQ(ht_get(c.ht, "name"), "Bob");
    CHECK_EQ(ht_count(c.ht), 1);

cleanup:
    teardown(&c);
}

void test_ht_delete_returns_true_for_existing(void) {
    Ctx c = setup();

    ht_insert(c.ht, "key", "val");
    CHECK_TRUE(ht_delete(c.ht, "key"));
    CHECK(ht_get(c.ht, "key") == NULL);
    CHECK_EQ(ht_count(c.ht), 0);

cleanup:
    teardown(&c);
}

void test_ht_get_missing_key(void) {
    Ctx c = setup();

    CHECK(ht_get(c.ht, "ghost") == NULL);

cleanup:
    teardown(&c);
}

int main(void) {
    printf("=== Hash Table Tests ===\n\n");

    RUN(test_ht_insert_new_key);
    RUN(test_ht_insert_duplicate_overwrites);
    RUN(test_ht_delete_returns_true_for_existing);
    RUN(test_ht_get_missing_key);

    SUMMARY();
    return _test_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -g -fsanitize=address -o test_ht test_hash_table.c hash_table.c
$ ./test_ht
=== Hash Table Tests ===

  [PASS] test_ht_insert_new_key
  [PASS] test_ht_insert_duplicate_overwrites
  [PASS] test_ht_delete_returns_true_for_existing
  [PASS] test_ht_get_missing_key

─────────────────────────────────────────
Assertions: 9 passed, 0 failed
```

Si un CHECK falla, salta directamente a `cleanup:`, que ejecuta `teardown()`. **No hay leak posible** (asumiendo que teardown libera todo).

### 3.3 goto cleanup con multiples recursos

El patron se extiende naturalmente cuando la fixture tiene multiples recursos que se adquieren en secuencia:

```c
void test_process_file_and_store(void) {
    // Inicializar todo a NULL/0 para saber que liberar en cleanup
    FILE *input = NULL;
    FILE *output = NULL;
    char *buffer = NULL;
    Database *db = NULL;
    char *tmpdir = NULL;

    // ── Setup (adquisicion incremental) ──
    tmpdir = make_temp_dir();
    CHECK(tmpdir != NULL);

    char input_path[256], output_path[256];
    snprintf(input_path, sizeof(input_path), "%s/input.txt", tmpdir);
    snprintf(output_path, sizeof(output_path), "%s/output.txt", tmpdir);

    // Crear archivo de entrada con datos de test
    input = fopen(input_path, "w");
    CHECK(input != NULL);
    fprintf(input, "test data\n");
    fclose(input);
    input = NULL;  // Ya cerrado — no cerrar de nuevo en cleanup

    // Abrir para lectura
    input = fopen(input_path, "r");
    CHECK(input != NULL);

    output = fopen(output_path, "w");
    CHECK(output != NULL);

    buffer = malloc(4096);
    CHECK(buffer != NULL);

    db = db_open(":memory:");
    CHECK(db != NULL);

    // ── Act ──
    int result = process_file(input, output, buffer, 4096, db);

    // ── Assert ──
    CHECK_EQ(result, 0);
    // ... mas verificaciones ...

cleanup:
    // Liberar en orden inverso de adquisicion
    // Cada uno verifica NULL antes de liberar
    if (db) db_close(db);
    free(buffer);           // free(NULL) es safe en C
    if (output) fclose(output);
    if (input) fclose(input);
    if (tmpdir) {
        remove_temp_dir(tmpdir);
        free(tmpdir);
    }
}
```

Observa el patron:
1. **Inicializar todo a NULL** al inicio
2. **Verificar NULL en cleanup** antes de liberar (o aprovechar que `free(NULL)` es no-op)
3. **Liberar en orden inverso** de adquisicion
4. Si un recurso se "transforma" (archivo abierto para escritura, cerrado, reabierto para lectura), poner el puntero a NULL despues de cerrar para evitar double-close

### 3.4 goto cleanup con conteo incremental

Un patron mas sofisticado cuando los recursos son del mismo tipo:

```c
void test_multiple_connections(void) {
    int n_conns = 0;
    int conns[4] = {-1, -1, -1, -1};

    conns[0] = tcp_connect("host1", 8080);
    CHECK(conns[0] >= 0);
    n_conns = 1;

    conns[1] = tcp_connect("host2", 8080);
    CHECK(conns[1] >= 0);
    n_conns = 2;

    conns[2] = tcp_connect("host3", 8080);
    CHECK(conns[2] >= 0);
    n_conns = 3;

    // Act y assert...
    CHECK_EQ(send_to_all(conns, n_conns, "hello"), 0);

cleanup:
    for (int i = 0; i < n_conns; i++) {
        close(conns[i]);
    }
}
```

---

## 4. Runner con teardown automatico

### 4.1 El runner que llama teardown por ti

En vez de depender de que cada test tenga su label `cleanup:`, podemos hacer que el **runner** llame a teardown despues de cada test, incluso si el test falla:

```c
// test_runner_fixture.h
#ifndef TEST_RUNNER_FIXTURE_H
#define TEST_RUNNER_FIXTURE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

// ── Estado global del runner ────────────────────────────────
static int _pass = 0;
static int _fail = 0;
static jmp_buf _test_jmp;          // Para saltar de vuelta al runner en caso de fallo
static int _test_failed = 0;       // Flag: ¿el test actual fallo?

// ── Macros de assert (usan longjmp para abortar el test sin abortar el proceso) ──

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); \
            _fail++; \
            _test_failed = 1; \
            longjmp(_test_jmp, 1); \
        } \
        _pass++; \
    } while (0)

#define ASSERT_EQ(got, expected) \
    do { \
        long _g = (long)(got), _e = (long)(expected); \
        if (_g != _e) { \
            fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                    __FILE__, __LINE__, _e, _g); \
            _fail++; \
            _test_failed = 1; \
            longjmp(_test_jmp, 1); \
        } \
        _pass++; \
    } while (0)

#define ASSERT_STR_EQ(got, expected) \
    do { \
        const char *_g = (got), *_e = (expected); \
        if (_g == NULL || _e == NULL || strcmp(_g, _e) != 0) { \
            fprintf(stderr, "  FAIL [%s:%d] expected \"%s\", got \"%s\"\n", \
                    __FILE__, __LINE__, \
                    _e ? _e : "(null)", _g ? _g : "(null)"); \
            _fail++; \
            _test_failed = 1; \
            longjmp(_test_jmp, 1); \
        } \
        _pass++; \
    } while (0)

#define ASSERT_TRUE(e)  ASSERT(e)
#define ASSERT_FALSE(e) ASSERT(!(e))

// ── Runner con fixture ──────────────────────────────────────

// RUN_WITH_FIXTURE: ejecuta test con setup/teardown garantizado
//   - setup_fn: funcion que retorna void* (la fixture)
//   - test_fn:  funcion que recibe void* (la fixture)
//   - teardown_fn: funcion que recibe void* (la fixture)
//   - name: string con el nombre del test

typedef void *(*setup_fn_t)(void);
typedef void (*test_fn_t)(void *fixture);
typedef void (*teardown_fn_t)(void *fixture);

static void run_with_fixture(const char *name, setup_fn_t setup,
                              test_fn_t test, teardown_fn_t teardown) {
    _test_failed = 0;

    // 1. Setup
    void *fixture = setup ? setup() : NULL;

    // 2. Run test (con setjmp para capturar fallos)
    if (setjmp(_test_jmp) == 0) {
        // Ejecucion normal del test
        test(fixture);
    }
    // Si el test hizo longjmp, llegamos aqui con _test_failed = 1

    // 3. Teardown — SE EJECUTA SIEMPRE (la clave del patron)
    if (teardown) {
        teardown(fixture);
    }

    // 4. Reportar
    printf("  [%s] %s\n", _test_failed ? "FAIL" : "PASS", name);
}

#define RUN_F(setup, test_fn, teardown) \
    run_with_fixture(#test_fn, (setup_fn_t)(setup), \
                     (test_fn_t)(test_fn), (teardown_fn_t)(teardown))

#define SUMMARY() \
    printf("\n─────────────────────────────────────────\n" \
           "Assertions: %d passed, %d failed\n", _pass, _fail)

#endif
```

### 4.2 Uso del runner con fixture

```c
// test_database.c
#include "test_runner_fixture.h"
#include "database.h"

// ── Fixture ─────────────────────────────────────────────────

typedef struct {
    Database *db;
    char tmpfile[256];
} DbFixture;

static void *db_setup(void) {
    DbFixture *f = malloc(sizeof(DbFixture));
    snprintf(f->tmpfile, sizeof(f->tmpfile), "/tmp/test_db_%d.sqlite", getpid());
    f->db = db_open(f->tmpfile);
    db_exec(f->db, "CREATE TABLE users (id INTEGER, name TEXT)");
    db_exec(f->db, "INSERT INTO users VALUES (1, 'Alice')");
    db_exec(f->db, "INSERT INTO users VALUES (2, 'Bob')");
    return f;
}

static void db_teardown(void *ptr) {
    DbFixture *f = ptr;
    db_close(f->db);
    unlink(f->tmpfile);  // Borrar el archivo de la DB
    free(f);
}

// ── Tests ───────────────────────────────────────────────────

static void test_db_count_users(void *ptr) {
    DbFixture *f = ptr;
    ASSERT_EQ(db_count(f->db, "users"), 2);
}

static void test_db_find_user_by_id(void *ptr) {
    DbFixture *f = ptr;
    const char *name = db_find(f->db, "users", "id", "1");
    ASSERT_STR_EQ(name, "Alice");
}

static void test_db_insert_and_count(void *ptr) {
    DbFixture *f = ptr;
    db_exec(f->db, "INSERT INTO users VALUES (3, 'Charlie')");
    ASSERT_EQ(db_count(f->db, "users"), 3);
}

static void test_db_delete_user(void *ptr) {
    DbFixture *f = ptr;
    db_exec(f->db, "DELETE FROM users WHERE id = 1");
    ASSERT_EQ(db_count(f->db, "users"), 1);
    ASSERT(db_find(f->db, "users", "id", "1") == NULL);
}

// ── main ────────────────────────────────────────────────────

int main(void) {
    printf("=== Database Tests ===\n\n");

    RUN_F(db_setup, test_db_count_users, db_teardown);
    RUN_F(db_setup, test_db_find_user_by_id, db_teardown);
    RUN_F(db_setup, test_db_insert_and_count, db_teardown);
    RUN_F(db_setup, test_db_delete_user, db_teardown);

    SUMMARY();
    return _fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

Puntos clave:

1. **Setup se llama antes de CADA test** — cada test tiene una base de datos fresca con los mismos datos iniciales.
2. **Teardown se ejecuta SIEMPRE** — incluso si el test falla con ASSERT (gracias a setjmp/longjmp).
3. **Los tests son independientes** — test_db_insert_and_count inserta un usuario, pero test_db_delete_user ve la base de datos original (sin el insert del test anterior) porque tiene su propio setup.

### 4.3 setjmp/longjmp — como funciona

`setjmp`/`longjmp` son el mecanismo de C para **saltos no locales** — el equivalente primitivo de las excepciones:

```c
#include <setjmp.h>

jmp_buf env;

void dangerous_function(void) {
    // longjmp "salta" de vuelta a donde se hizo setjmp
    // El segundo argumento (1) es el valor que setjmp retorna
    longjmp(env, 1);
    // Esta linea NUNCA se ejecuta
    printf("unreachable\n");
}

int main(void) {
    // setjmp retorna 0 la primera vez (cuando se "guarda el checkpoint")
    if (setjmp(env) == 0) {
        // Primera vez — ejecucion normal
        printf("Before jump\n");
        dangerous_function();
        printf("After function (never reached)\n");
    } else {
        // Llegamos aqui via longjmp
        printf("Jumped back! (recovery)\n");
    }

    printf("Continuing normally\n");
    return 0;
}
```

```
Output:
Before jump
Jumped back! (recovery)
Continuing normally
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    setjmp/longjmp EN TESTING                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  run_with_fixture():                                                       │
│      fixture = setup()                                                     │
│      if (setjmp(env) == 0) {    ← "checkpoint" guardado                   │
│          test(fixture)          ← ejecutar el test                         │
│              │                                                             │
│              ├── ASSERT pasa → continua                                    │
│              │                                                             │
│              └── ASSERT falla → longjmp(env, 1)                           │
│                                    │                                       │
│                                    ▼                                       │
│      } else {                  ← llega aqui via longjmp                   │
│          // test fallo                                                     │
│      }                                                                     │
│      teardown(fixture)         ← SE EJECUTA EN AMBOS CASOS               │
│                                                                             │
│  Analogia:                                                                 │
│  setjmp    = try {                                                         │
│  longjmp   = throw                                                         │
│  el else   = catch                                                         │
│  teardown  = finally                                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Advertencias sobre setjmp/longjmp**:

| Peligro | Descripcion |
|---------|-------------|
| No llama destructores | En C++ (o C con cleanup handlers), los destructores de objetos locales entre el setjmp y el longjmp NO se ejecutan |
| Variables locales volatiles | Variables locales modificadas entre setjmp y longjmp necesitan ser `volatile` para garantizar que conservan su valor despues del longjmp |
| No cruza threads | El jmp_buf es por thread — no puedes longjmp a un setjmp de otro thread |
| Stack unwinding parcial | longjmp "desembobina" el stack, pero no libera memoria allocada entre setjmp y longjmp |

Para testing estas advertencias son mayormente irrelevantes — el patron es: setjmp en el runner, longjmp en la macro de assert, teardown despues de ambos caminos. Simple y efectivo.

---

## 5. Multiples fixtures — composicion

### 5.1 Fixture con multiples componentes

A veces un test necesita multiples subsistemas:

```c
typedef struct {
    Database *db;
    Cache *cache;
    Logger *logger;
    char tmpdir[256];
} ServiceFixture;

static void *service_setup(void) {
    ServiceFixture *f = calloc(1, sizeof(ServiceFixture));

    // Crear directorio temporal
    char template[] = "/tmp/test_XXXXXX";
    char *dir = mkdtemp(template);
    strncpy(f->tmpdir, dir, sizeof(f->tmpdir) - 1);

    // Crear cada componente
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/test.db", f->tmpdir);
    f->db = db_open(db_path);

    f->cache = cache_new(64);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/test.log", f->tmpdir);
    f->logger = logger_new(log_path);

    // Insertar datos de prueba
    db_exec(f->db, "CREATE TABLE items (id INT, name TEXT, price INT)");
    db_exec(f->db, "INSERT INTO items VALUES (1, 'Widget', 999)");
    db_exec(f->db, "INSERT INTO items VALUES (2, 'Gadget', 1999)");

    return f;
}

static void service_teardown(void *ptr) {
    ServiceFixture *f = ptr;

    // Liberar en orden inverso de creacion
    logger_free(f->logger);
    cache_free(f->cache);
    db_close(f->db);

    // Limpiar directorio temporal
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", f->tmpdir);
    system(cmd);

    free(f);
}
```

### 5.2 Fixtures reutilizables entre archivos de test

Si multiples archivos de test necesitan la misma fixture, puedes ponerla en un header compartido:

```c
// test_fixtures.h
#ifndef TEST_FIXTURES_H
#define TEST_FIXTURES_H

#include "database.h"
#include "cache.h"

// ── Database fixture ────────────────────────────────────────

typedef struct {
    Database *db;
    char tmpfile[256];
} DbFixture;

static inline void *db_fixture_setup(void) {
    DbFixture *f = calloc(1, sizeof(DbFixture));
    snprintf(f->tmpfile, sizeof(f->tmpfile), "/tmp/test_%d.db", getpid());
    f->db = db_open(f->tmpfile);
    return f;
}

static inline void db_fixture_teardown(void *ptr) {
    DbFixture *f = ptr;
    if (f) {
        db_close(f->db);
        unlink(f->tmpfile);
        free(f);
    }
}

// ── Cache fixture ───────────────────────────────────────────

typedef struct {
    Cache *cache;
} CacheFixture;

static inline void *cache_fixture_setup(void) {
    CacheFixture *f = calloc(1, sizeof(CacheFixture));
    f->cache = cache_new(32);
    return f;
}

static inline void cache_fixture_teardown(void *ptr) {
    CacheFixture *f = ptr;
    if (f) {
        cache_free(f->cache);
        free(f);
    }
}

#endif
```

Nota: las funciones son `static inline` para evitar "unused function" warnings si un archivo de test incluye el header pero no usa todas las fixtures.

### 5.3 Fixture con datos parametricos

A veces quieres la misma fixture pero con diferentes datos iniciales:

```c
typedef struct {
    HashTable *ht;
    size_t initial_capacity;
    size_t num_entries;
} HtFixtureParam;

static void *ht_setup_with_params(size_t capacity, size_t entries) {
    HtFixtureParam *f = calloc(1, sizeof(HtFixtureParam));
    f->initial_capacity = capacity;
    f->num_entries = entries;
    f->ht = ht_new(capacity);

    for (size_t i = 0; i < entries; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key_%zu", i);
        snprintf(val, sizeof(val), "val_%zu", i);
        ht_insert(f->ht, key, strdup(val));
    }

    return f;
}

// Wrappers para el runner (que espera funciones sin argumentos)
static void *setup_small_empty(void)  { return ht_setup_with_params(4, 0); }
static void *setup_small_full(void)   { return ht_setup_with_params(4, 3); }
static void *setup_large_sparse(void) { return ht_setup_with_params(1024, 10); }
static void *setup_large_dense(void)  { return ht_setup_with_params(16, 100); }

static void ht_param_teardown(void *ptr) {
    HtFixtureParam *f = ptr;
    // Liberar los valores strdup'd
    // ... (iterar la tabla y free cada valor) ...
    ht_free(f->ht);
    free(f);
}

// Ahora puedes testear el mismo comportamiento con diferentes configuraciones:
static void test_ht_get_returns_inserted_value(void *ptr) {
    HtFixtureParam *f = ptr;
    ht_insert(f->ht, "new_key", "new_val");
    ASSERT_STR_EQ(ht_get(f->ht, "new_key"), "new_val");
}

int main(void) {
    printf("-- small empty table --\n");
    RUN_F(setup_small_empty, test_ht_get_returns_inserted_value, ht_param_teardown);

    printf("-- small full table (triggers resize) --\n");
    RUN_F(setup_small_full, test_ht_get_returns_inserted_value, ht_param_teardown);

    printf("-- large sparse table --\n");
    RUN_F(setup_large_sparse, test_ht_get_returns_inserted_value, ht_param_teardown);

    printf("-- large dense table --\n");
    RUN_F(setup_large_dense, test_ht_get_returns_inserted_value, ht_param_teardown);

    SUMMARY();
    return 0;
}
```

---

## 6. Simular constructores y destructores

### 6.1 El patron constructor/destructor en C

C no tiene constructores ni destructores, pero la convencion `modulo_new()`/`modulo_free()` cumple la misma funcion. Para testing, este patron es esencial — todo recurso debe tener un "destructor" claro:

```c
// ── Patron constructor/destructor bien hecho ─────────────────

// Constructor: alloca e inicializa. Retorna NULL si falla.
Stack *stack_new(size_t initial_cap) {
    Stack *s = malloc(sizeof(Stack));
    if (s == NULL) return NULL;

    s->data = malloc(initial_cap * sizeof(int));
    if (s->data == NULL) {
        free(s);          // Limpiar lo que ya se alloco
        return NULL;
    }

    s->len = 0;
    s->cap = initial_cap;
    return s;
}

// Destructor: libera todo. Acepta NULL gracefully.
void stack_free(Stack *s) {
    if (s == NULL) return;   // Importante: free(NULL) es safe, pero nuestro
                             // destructor debe serlo tambien
    free(s->data);
    free(s);
}
```

### 6.2 Patron init/deinit (sin malloc)

A veces no quieres allocar en el heap — quieres inicializar un struct en el stack:

```c
// Variante: init/deinit (el caller alloca, nosotros inicializamos)

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} Stack;

// Init: inicializa un struct ya existente
int stack_init(Stack *s, size_t initial_cap) {
    if (s == NULL) return -1;

    s->data = malloc(initial_cap * sizeof(int));
    if (s->data == NULL) return -1;

    s->len = 0;
    s->cap = initial_cap;
    return 0;
}

// Deinit: libera los recursos internos (pero NO el struct mismo)
void stack_deinit(Stack *s) {
    if (s == NULL) return;
    free(s->data);
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}
```

```c
// Uso en tests:

void test_stack_push(void) {
    Stack s;
    int rc = stack_init(&s, 4);  // En el stack, no en el heap
    CHECK_EQ(rc, 0);

    stack_push(&s, 42);
    CHECK_EQ(stack_len(&s), 1);

cleanup:
    stack_deinit(&s);  // Solo libera los internos, no &s (que esta en el stack)
}
```

Ventaja: no hay `malloc`/`free` del struct mismo, asi que aunque `stack_deinit` no se ejecute, la unica perdida es `s.data` (no el struct completo).

### 6.3 Jerarquia de recursos — destruccion en cascada

Cuando un objeto "posee" otros objetos, el destructor debe destruir recursivamente:

```c
typedef struct {
    char *name;
    int *grades;
    size_t num_grades;
} Student;

typedef struct {
    Student *students;
    size_t count;
    size_t capacity;
    char *school_name;
} Classroom;

// Destructor de Student
void student_free(Student *s) {
    if (s == NULL) return;
    free(s->name);
    free(s->grades);
    // NO free(s) — el Student puede estar dentro de un array
}

// Destructor de Classroom — destruye todo recursivamente
void classroom_free(Classroom *c) {
    if (c == NULL) return;

    // Destruir cada estudiante
    for (size_t i = 0; i < c->count; i++) {
        student_free(&c->students[i]);
    }

    // Destruir el array de estudiantes
    free(c->students);

    // Destruir el nombre
    free(c->school_name);

    // Destruir el classroom
    free(c);
}
```

```c
// En el test, solo necesitas llamar al destructor del "raiz":
void test_classroom_add_student(void) {
    Classroom *c = classroom_new("Test School", 10);

    Student s = { .name = strdup("Alice"), .grades = NULL, .num_grades = 0 };
    classroom_add(c, &s);

    CHECK_EQ(c->count, 1);
    CHECK_STR_EQ(c->students[0].name, "Alice");

cleanup:
    classroom_free(c);  // Libera todo: classroom, students, names, grades
}
```

### 6.4 atexit — cleanup global de ultimo recurso

`atexit()` registra funciones que se ejecutan cuando el programa termina (via `exit()` o `return` desde `main()`). Se puede usar como red de seguridad:

```c
// test_with_atexit.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estado global de test que necesita limpieza
static char *global_tmpdir = NULL;

static void cleanup_tmpdir(void) {
    if (global_tmpdir) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", global_tmpdir);
        system(cmd);
        free(global_tmpdir);
        global_tmpdir = NULL;
        fprintf(stderr, "[cleanup] temp dir removed\n");
    }
}

int main(void) {
    // Registrar cleanup — se ejecuta al salir del programa
    // INCLUSO si un assert() llama a abort()... NO. abort() NO ejecuta atexit.
    // atexit solo funciona con exit() o return desde main().
    atexit(cleanup_tmpdir);

    char template[] = "/tmp/test_XXXXXX";
    global_tmpdir = strdup(mkdtemp(template));
    printf("Using tmpdir: %s\n", global_tmpdir);

    // ... tests ...
    // Si el programa sale normalmente (return 0 o exit(0/1)),
    // cleanup_tmpdir se ejecuta.
    // Si el programa aborta (assert failure → abort() → SIGABRT),
    // cleanup_tmpdir NO se ejecuta.

    return 0;
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│           ¿CUANDO SE EJECUTA atexit?                                │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  return desde main()        → SI                                    │
│  exit(code)                 → SI                                    │
│  abort()                    → NO  (assert failure usa abort)        │
│  _Exit(code) / _exit(code)  → NO                                   │
│  signal no manejado (SEGV)  → NO                                   │
│  SIGKILL                    → NO  (imposible interceptar)           │
│                                                                      │
│  Conclusion: atexit NO es confiable para tests que usan assert()   │
│  Mejor: usar setjmp/longjmp o goto cleanup                         │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 7. fork() — aislamiento total

### 7.1 El aislamiento perfecto

La solucion mas robusta para cleanup es ejecutar cada test en un **proceso hijo**. Cuando el hijo termina (normal o por crash), el kernel libera **toda** su memoria, cierra **todos** sus file descriptors, y elimina **todos** sus recursos. No hay leaks posibles.

```c
// test_runner_fork.h
#ifndef TEST_RUNNER_FORK_H
#define TEST_RUNNER_FORK_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

static int _total_pass = 0;
static int _total_fail = 0;

typedef void (*test_fn_void)(void);

static void run_test_forked(const char *name, test_fn_void fn) {
    fflush(stdout);
    fflush(stderr);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        _total_fail++;
        return;
    }

    if (pid == 0) {
        // ── Proceso hijo: ejecutar el test ──
        fn();
        _exit(EXIT_SUCCESS);
        // Si el test crashea (segfault, assert fail), el hijo muere
        // y el padre lo detecta via waitpid
    }

    // ── Proceso padre: esperar al hijo ──
    int status;
    waitpid(pid, &status, 0);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0
                   + (end.tv_nsec - start.tv_nsec) / 1e6;

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("  [PASS] %-45s (%.1f ms)\n", name, elapsed);
        _total_pass++;
    } else if (WIFEXITED(status)) {
        printf("  [FAIL] %-45s (exit %d, %.1f ms)\n",
               name, WEXITSTATUS(status), elapsed);
        _total_fail++;
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        const char *sig_name = "unknown";
        switch (sig) {
            case 6:  sig_name = "SIGABRT (assert failure)"; break;
            case 11: sig_name = "SIGSEGV (segfault)"; break;
            case 8:  sig_name = "SIGFPE (division by zero)"; break;
        }
        printf("  [CRASH] %-43s (%s, %.1f ms)\n", name, sig_name, elapsed);
        _total_fail++;
    }
}

#define RUN_FORKED(fn) run_test_forked(#fn, fn)

#define SUMMARY_FORKED() \
    printf("\n─────────────────────────────────────────\n" \
           "%d passed, %d failed, %d total\n", \
           _total_pass, _total_fail, _total_pass + _total_fail)

#endif
```

### 7.2 Uso

```c
// test_risky.c
#include "test_runner_fork.h"
#include <assert.h>
#include <string.h>

void test_normal(void) {
    // Test normal — pasa
    int x = 2 + 3;
    assert(x == 5);
}

void test_assertion_failure(void) {
    // Fallo de assert — el hijo aborta, pero el padre sigue
    assert(1 == 2);
}

void test_segfault(void) {
    // Segfault — el hijo muere con SIGSEGV, el padre sigue
    int *p = NULL;
    *p = 42;
}

void test_with_leak(void) {
    // Memory leak — NO importa, el proceso hijo muere y el OS limpia todo
    char *leaked = malloc(1000000);
    strcpy(leaked, "never freed");
    assert(strlen(leaked) == 11);
    // Sin free(leaked) — pero no importa en un hijo fork'd
}

void test_with_temp_file(void) {
    // Archivo temporal — el fd se cierra automaticamente al morir el hijo
    // (pero el archivo persiste en disco — esto requiere cleanup manual)
    FILE *f = tmpfile();  // tmpfile() se borra automaticamente
    fprintf(f, "test data");
    rewind(f);
    char buf[64];
    fgets(buf, sizeof(buf), f);
    assert(strcmp(buf, "test data") == 0);
    // Sin fclose(f) — el OS lo cierra al morir el hijo
    // tmpfile() tambien borra el archivo automaticamente
}

int main(void) {
    printf("=== Forked Test Runner ===\n\n");

    RUN_FORKED(test_normal);
    RUN_FORKED(test_assertion_failure);
    RUN_FORKED(test_segfault);
    RUN_FORKED(test_with_leak);
    RUN_FORKED(test_with_temp_file);

    SUMMARY_FORKED();
    return _total_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -g -o test_risky test_risky.c
$ ./test_risky
=== Forked Test Runner ===

  [PASS] test_normal                                (0.3 ms)
  [CRASH] test_assertion_failure                    (SIGABRT (assert failure), 0.4 ms)
  [CRASH] test_segfault                             (SIGSEGV (segfault), 0.2 ms)
  [PASS] test_with_leak                             (0.3 ms)
  [PASS] test_with_temp_file                        (0.4 ms)

─────────────────────────────────────────
3 passed, 2 failed, 5 total
```

Observa:
- `test_assertion_failure` crashea con SIGABRT pero **no mata** al runner
- `test_segfault` crashea con SIGSEGV pero **no mata** al runner
- `test_with_leak` tiene un memory leak pero **no importa** — el OS limpia al morir el hijo
- Todos los tests se ejecutan, incluso despues de crashes

### 7.3 fork vs setjmp/longjmp — cuando usar cada uno

| Aspecto | setjmp/longjmp | fork() |
|---------|----------------|--------|
| **Overhead** | Minimo (un salto) | Alto (crear proceso, ~0.1-1ms) |
| **Aislamiento** | Parcial (mismo address space) | Total (procesos separados) |
| **Cleanup** | Manual (teardown) | Automatico (OS limpia todo) |
| **Crashes (segfault)** | Mata todo el runner | Solo mata el hijo |
| **Comunicacion** | Variables compartidas | Pipes o exit codes |
| **Portabilidad** | C estandar | POSIX (Linux, macOS, BSDs) |
| **ASan/Valgrind** | Funciona normal | Puede tener quirks (ASan + fork) |

Recomendacion:
- **Usa setjmp/longjmp** para tests rapidos que no crashean (unit tests puros)
- **Usa fork()** para tests que pueden crashear, tests de integracion, o cuando quieres aislamiento total

---

## 8. Comparacion con frameworks reales

### 8.1 Como lo hacen Unity, CMocka y Check

Los frameworks de testing en C formalizan estos patrones:

**Unity (setUp/tearDown globales):**

```c
// En Unity, setUp y tearDown son funciones con nombres fijos
// que se llaman automaticamente antes/despues de cada test

static HashTable *ht;

void setUp(void) {
    ht = ht_new(16);
}

void tearDown(void) {
    ht_free(ht);
}

void test_insert(void) {
    ht_insert(ht, "key", "val");
    TEST_ASSERT_EQUAL_STRING("val", ht_get(ht, "key"));
    // tearDown se ejecuta automaticamente despues
}

void test_count(void) {
    // setUp se ejecuto: ht es fresco
    TEST_ASSERT_EQUAL(0, ht_count(ht));
    ht_insert(ht, "key", "val");
    TEST_ASSERT_EQUAL(1, ht_count(ht));
    // tearDown se ejecuta automaticamente despues
}
```

**CMocka (state pasado como parametro):**

```c
// CMocka usa el patron de fixture pasada como parametro
// Muy similar a nuestro RUN_F

static int ht_setup(void **state) {
    *state = ht_new(16);
    return 0;  // 0 = setup exitoso
}

static int ht_teardown(void **state) {
    ht_free(*state);
    return 0;
}

static void test_insert(void **state) {
    HashTable *ht = *state;
    ht_insert(ht, "key", "val");
    assert_string_equal(ht_get(ht, "key"), "val");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_insert, ht_setup, ht_teardown),
        cmocka_unit_test_setup_teardown(test_count, ht_setup, ht_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

**Check (fork por defecto):**

```c
// Check ejecuta cada test en un proceso hijo por defecto (fork)

START_TEST(test_insert) {
    HashTable *ht = ht_new(16);
    ht_insert(ht, "key", "val");
    ck_assert_str_eq(ht_get(ht, "key"), "val");
    ht_free(ht);
    // Si no haces free, no importa — fork()
}
END_TEST

Suite *ht_suite(void) {
    Suite *s = suite_create("HashTable");
    TCase *tc = tcase_create("basic");
    tcase_add_test(tc, test_insert);
    suite_add_tcase(s, tc);
    return s;
}
```

### 8.2 Tabla comparativa

| Aspecto | Manual (nuestro) | Unity | CMocka | Check |
|---------|------------------|-------|--------|-------|
| **Setup/Teardown** | Funciones llamadas explicitamente | setUp()/tearDown() automaticos | setup/teardown pasados como parametro | Fixture con setup/teardown |
| **Scope de fixture** | Por test o global | Por test (un setUp/tearDown global por archivo) | Por test o por grupo | Por TCase (grupo de tests) |
| **Aislamiento** | goto, setjmp, o fork (manual) | setjmp (longjmp en caso de fallo) | setjmp | fork (por defecto) |
| **Parametros al test** | void* o struct | Globals | void **state | Globals |
| **Cleanup garantizado** | Solo con goto/setjmp/fork | Si (interno) | Si (interno) | Si (fork + OS cleanup) |
| **Overhead** | ~0 (macros) | Bajo | Bajo | Alto (fork por test) |

---

## 9. Comparacion con Rust y Go

### 9.1 Fixtures en tres lenguajes

**C (manual):**

```c
typedef struct {
    Vec *vec;
    int *test_data;
    size_t data_len;
} VecFixture;

static VecFixture setup(void) {
    VecFixture f = {0};
    f.vec = vec_new(16);
    f.test_data = malloc(5 * sizeof(int));
    f.data_len = 5;
    for (int i = 0; i < 5; i++) {
        f.test_data[i] = (i + 1) * 10;
        vec_push(f.vec, f.test_data[i]);
    }
    return f;
}

static void teardown(VecFixture *f) {
    vec_free(f->vec);
    free(f->test_data);
}

void test_vec_contains_all(void) {
    VecFixture f = setup();

    for (size_t i = 0; i < f.data_len; i++) {
        CHECK_EQ(vec_get(f.vec, i), f.test_data[i]);
    }

cleanup:
    teardown(&f);
}
```

**Rust (RAII + closures):**

```rust
fn make_test_vec() -> Vec<i32> {
    vec![10, 20, 30, 40, 50]
}

#[test]
fn test_vec_contains_all() {
    let v = make_test_vec();  // "setup"

    let expected = [10, 20, 30, 40, 50];
    for (i, &exp) in expected.iter().enumerate() {
        assert_eq!(v[i], exp);
    }

    // "teardown": Drop se ejecuta automaticamente aqui
    // Vec::drop libera el heap buffer
    // Incluso si assert_eq! panics, Drop se ejecuta (stack unwinding)
}
```

**Go (t.Cleanup):**

```go
func setupVec(t *testing.T) *Vec {
    t.Helper()
    v := NewVec(16)
    for _, n := range []int{10, 20, 30, 40, 50} {
        v.Push(n)
    }
    t.Cleanup(func() {
        v.Free()  // Se ejecuta SIEMPRE, incluso si el test falla
    })
    return v
}

func TestVecContainsAll(t *testing.T) {
    v := setupVec(t)

    expected := []int{10, 20, 30, 40, 50}
    for i, exp := range expected {
        if got := v.Get(i); got != exp {
            t.Errorf("Get(%d) = %d, want %d", i, got, exp)
        }
    }
    // Cleanup se ejecuta automaticamente al salir del test
}
```

### 9.2 Diferencias fundamentales

| Aspecto | C | Rust | Go |
|---------|---|------|----|
| **Cleanup automatico** | No — debes llamar a teardown manualmente (o usar goto/setjmp/fork) | Si — Drop trait (RAII) libera recursos automaticamente | Parcial — GC para memoria, t.Cleanup para recursos |
| **Cleanup en fallo** | No garantizado (assert → abort → sin cleanup) | Si — stack unwinding ejecuta Drop incluso en panic | Si — t.Cleanup se ejecuta siempre |
| **Shared state** | Structs + punteros, gestion manual | Builders, closures, Arc/Mutex | Structs, closures |
| **Fixture scope** | Manual (por test, por grupo, global) | Por test (el scope de la variable) | Por test (t.Cleanup), por paquete (TestMain) |
| **Recursos del OS** | Debes cerrar archivos, sockets, memoria | RAII cierra automaticamente (File, TcpStream) | defer/Cleanup cierra automaticamente |

---

## 10. Fixtures globales — setup/teardown de suite

### 10.1 Cuando necesitas una fixture que se inicializa una sola vez

A veces, crear la fixture para cada test es demasiado costoso:

- Conectar a una base de datos (100ms)
- Cargar un archivo de configuracion grande (50ms)
- Compilar un parser grammar (200ms)
- Crear un pool de threads

En estos casos, creas la fixture **una vez** para toda la suite y la compartes entre tests:

```c
// test_with_global_fixture.c

#include <stdio.h>
#include <stdlib.h>
#include "database.h"
#include "test_helpers_goto.h"

// ── Fixture global (una por suite) ──────────────────────────

static Database *g_db = NULL;

static int suite_setup(void) {
    g_db = db_open("test.db");
    if (g_db == NULL) {
        fprintf(stderr, "FATAL: cannot open test database\n");
        return -1;
    }

    // Crear schema
    if (db_exec(g_db, "CREATE TABLE IF NOT EXISTS users "
                      "(id INTEGER PRIMARY KEY, name TEXT)") != 0) {
        db_close(g_db);
        return -1;
    }

    return 0;
}

static void suite_teardown(void) {
    if (g_db) {
        db_close(g_db);
        g_db = NULL;
    }
    unlink("test.db");
}

// ── Per-test fixture (limpia los datos entre tests) ─────────

static void per_test_setup(void) {
    // Limpiar datos de tests anteriores, pero NO recrear la tabla
    db_exec(g_db, "DELETE FROM users");
    // Insertar datos base
    db_exec(g_db, "INSERT INTO users VALUES (1, 'Alice')");
    db_exec(g_db, "INSERT INTO users VALUES (2, 'Bob')");
}

// ── Tests ───────────────────────────────────────────────────

void test_count_initial_users(void) {
    per_test_setup();
    CHECK_EQ(db_count(g_db, "users"), 2);
cleanup:
    return;
}

void test_insert_user(void) {
    per_test_setup();
    db_exec(g_db, "INSERT INTO users VALUES (3, 'Charlie')");
    CHECK_EQ(db_count(g_db, "users"), 3);
cleanup:
    return;
}

void test_delete_user(void) {
    per_test_setup();
    db_exec(g_db, "DELETE FROM users WHERE id = 1");
    CHECK_EQ(db_count(g_db, "users"), 1);
cleanup:
    return;
}

// ── main ────────────────────────────────────────────────────

int main(void) {
    // Suite setup — una vez
    if (suite_setup() != 0) {
        fprintf(stderr, "Suite setup failed!\n");
        return EXIT_FAILURE;
    }

    printf("=== Database Tests ===\n\n");

    RUN(test_count_initial_users);
    RUN(test_insert_user);
    RUN(test_delete_user);

    SUMMARY();

    // Suite teardown — una vez
    suite_teardown();

    return _test_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

El patron es:
- `suite_setup()` en main, antes de todos los tests
- `per_test_setup()` al inicio de cada test (reset de datos)
- `suite_teardown()` en main, despues de todos los tests

### 10.2 Cuidado con el estado compartido

Cuando usas fixtures globales, los tests **comparten estado**. Esto viola el principio de independencia (T02). Mitigaciones:

1. **Reset entre tests**: `per_test_setup()` restaura el estado a una linea base conocida
2. **Tests read-only**: si los tests solo leen la fixture global (no la modifican), son naturalmente independientes
3. **Documentar la dependencia**: si la fixture global es necesaria, documenta claramente que tests la usan y por que

```
┌──────────────────────────────────────────────────────────────────────┐
│               FIXTURE SCOPE — TRADE-OFFS                            │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Per-test fixture (recomendado):                                    │
│  • Cada test crea y destruye su fixture                             │
│  • Independencia total                                              │
│  • Mas lento si la creacion es costosa                              │
│                                                                      │
│  Per-suite fixture (cuando es necesario):                           │
│  • Una fixture compartida para toda la suite                        │
│  • Necesita reset entre tests                                       │
│  • Mas rapido para recursos costosos (DB, red, compilacion)         │
│  • Riesgo de dependencia entre tests                                │
│                                                                      │
│  Hibrido (lo mas comun en la practica):                             │
│  • Global: conexion a DB, pool de threads                           │
│  • Per-test: datos en la DB, estado de la aplicacion                │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejemplo completo — File processor con fixtures

### 11.1 El modulo bajo test

```c
// file_processor.h
#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <stddef.h>

typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
} LineBuffer;

// Crea un buffer de lineas
LineBuffer *lb_new(size_t initial_cap);

// Libera el buffer y todas las lineas
void lb_free(LineBuffer *lb);

// Lee un archivo linea por linea y almacena en el buffer
int lb_load_file(LineBuffer *lb, const char *path);

// Filtra lineas que contienen el substring dado
// Retorna un NUEVO LineBuffer con las lineas filtradas (copias)
LineBuffer *lb_filter(const LineBuffer *lb, const char *substring);

// Cuenta lineas no vacias
size_t lb_count_nonempty(const LineBuffer *lb);

// Busca una linea que empiece con prefix, retorna su indice o -1
int lb_find_prefix(const LineBuffer *lb, const char *prefix);

#endif
```

### 11.2 Los tests con fixtures

```c
// test_file_processor.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "file_processor.h"

// ═══════════════════════════════════════════════════════════════
// Framework con goto cleanup
// ═══════════════════════════════════════════════════════════════

static int _pass = 0, _fail = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); \
        _fail++; goto cleanup; \
    } _pass++; \
} while (0)

#define CHECK_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                __FILE__, __LINE__, _b, _a); \
        _fail++; goto cleanup; \
    } _pass++; \
} while (0)

#define CHECK_STR_EQ(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  FAIL [%s:%d] expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, _b ? _b : "(null)", _a ? _a : "(null)"); \
        _fail++; goto cleanup; \
    } _pass++; \
} while (0)

#define CHECK_TRUE(e)  CHECK(e)
#define CHECK_FALSE(e) CHECK(!(e))
#define CHECK_NULL(e)  CHECK((e) == NULL)
#define CHECK_NOT_NULL(e) CHECK((e) != NULL)

#define RUN(fn) do { \
    int _bf = _fail; fn(); \
    printf("  [%s] %s\n", (_fail == _bf) ? "PASS" : "FAIL", #fn); \
} while (0)

// ═══════════════════════════════════════════════════════════════
// Fixture de filesystem
// ═══════════════════════════════════════════════════════════════

typedef struct {
    char dir[256];
    char file_path[512];
} FsFixture;

static FsFixture fs_setup(const char *content) {
    FsFixture f = {0};

    // Crear directorio temporal
    char template[] = "/tmp/test_fp_XXXXXX";
    char *dir = mkdtemp(template);
    strncpy(f.dir, dir, sizeof(f.dir) - 1);

    // Crear archivo de test
    snprintf(f.file_path, sizeof(f.file_path), "%s/input.txt", f.dir);
    FILE *fp = fopen(f.file_path, "w");
    if (fp && content) {
        fputs(content, fp);
        fclose(fp);
    }

    return f;
}

static void fs_teardown(FsFixture *f) {
    if (f->dir[0] != '\0') {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", f->dir);
        system(cmd);
    }
}

// ═══════════════════════════════════════════════════════════════
// Fixture de LineBuffer con datos pre-cargados
// ═══════════════════════════════════════════════════════════════

typedef struct {
    LineBuffer *lb;
    FsFixture fs;
} LbFixture;

static LbFixture lb_setup_with_content(const char *content) {
    LbFixture f = {0};
    f.fs = fs_setup(content);
    f.lb = lb_new(8);
    if (content) {
        lb_load_file(f.lb, f.fs.file_path);
    }
    return f;
}

static void lb_teardown(LbFixture *f) {
    lb_free(f->lb);
    fs_teardown(&f->fs);
}

// ═══════════════════════════════════════════════════════════════
// Tests: Creacion
// ═══════════════════════════════════════════════════════════════

void test_lb_new_creates_empty_buffer(void) {
    LineBuffer *lb = lb_new(4);

    CHECK_NOT_NULL(lb);
    CHECK_EQ(lb->count, 0);

cleanup:
    lb_free(lb);
}

// ═══════════════════════════════════════════════════════════════
// Tests: lb_load_file
// ═══════════════════════════════════════════════════════════════

void test_lb_load_file_reads_lines(void) {
    LbFixture f = lb_setup_with_content("line one\nline two\nline three\n");

    CHECK_EQ(f.lb->count, 3);
    CHECK_STR_EQ(f.lb->lines[0], "line one");
    CHECK_STR_EQ(f.lb->lines[1], "line two");
    CHECK_STR_EQ(f.lb->lines[2], "line three");

cleanup:
    lb_teardown(&f);
}

void test_lb_load_file_empty(void) {
    LbFixture f = lb_setup_with_content("");

    CHECK_EQ(f.lb->count, 0);

cleanup:
    lb_teardown(&f);
}

void test_lb_load_file_nonexistent(void) {
    LineBuffer *lb = lb_new(4);

    int rc = lb_load_file(lb, "/tmp/nonexistent_test_file_12345.txt");
    CHECK(rc != 0);  // Debe retornar error
    CHECK_EQ(lb->count, 0);

cleanup:
    lb_free(lb);
}

void test_lb_load_file_single_line_no_newline(void) {
    LbFixture f = lb_setup_with_content("single line");

    CHECK_EQ(f.lb->count, 1);
    CHECK_STR_EQ(f.lb->lines[0], "single line");

cleanup:
    lb_teardown(&f);
}

// ═══════════════════════════════════════════════════════════════
// Tests: lb_filter
// ═══════════════════════════════════════════════════════════════

void test_lb_filter_matching_lines(void) {
    LbFixture f = lb_setup_with_content(
        "error: disk full\n"
        "info: started\n"
        "error: network timeout\n"
        "info: shutdown\n"
    );
    LineBuffer *filtered = NULL;

    filtered = lb_filter(f.lb, "error");
    CHECK_NOT_NULL(filtered);
    CHECK_EQ(filtered->count, 2);
    CHECK_STR_EQ(filtered->lines[0], "error: disk full");
    CHECK_STR_EQ(filtered->lines[1], "error: network timeout");

cleanup:
    lb_free(filtered);
    lb_teardown(&f);
}

void test_lb_filter_no_matches(void) {
    LbFixture f = lb_setup_with_content("aaa\nbbb\nccc\n");
    LineBuffer *filtered = NULL;

    filtered = lb_filter(f.lb, "zzz");
    CHECK_NOT_NULL(filtered);
    CHECK_EQ(filtered->count, 0);

cleanup:
    lb_free(filtered);
    lb_teardown(&f);
}

void test_lb_filter_all_match(void) {
    LbFixture f = lb_setup_with_content("abc\nabc\nabc\n");
    LineBuffer *filtered = NULL;

    filtered = lb_filter(f.lb, "abc");
    CHECK_NOT_NULL(filtered);
    CHECK_EQ(filtered->count, 3);

cleanup:
    lb_free(filtered);
    lb_teardown(&f);
}

// ═══════════════════════════════════════════════════════════════
// Tests: lb_count_nonempty
// ═══════════════════════════════════════════════════════════════

void test_lb_count_nonempty_mixed(void) {
    LbFixture f = lb_setup_with_content("hello\n\nworld\n\n\nlast\n");

    CHECK_EQ(lb_count_nonempty(f.lb), 3);  // "hello", "world", "last"

cleanup:
    lb_teardown(&f);
}

// ═══════════════════════════════════════════════════════════════
// Tests: lb_find_prefix
// ═══════════════════════════════════════════════════════════════

void test_lb_find_prefix_found(void) {
    LbFixture f = lb_setup_with_content(
        "name=Alice\n"
        "age=30\n"
        "city=NYC\n"
    );

    CHECK_EQ(lb_find_prefix(f.lb, "age="), 1);

cleanup:
    lb_teardown(&f);
}

void test_lb_find_prefix_not_found(void) {
    LbFixture f = lb_setup_with_content("aaa\nbbb\nccc\n");

    CHECK_EQ(lb_find_prefix(f.lb, "zzz"), -1);

cleanup:
    lb_teardown(&f);
}

// ═══════════════════════════════════════════════════════════════
// Tests: Null safety
// ═══════════════════════════════════════════════════════════════

void test_lb_null_safety(void) {
    CHECK_EQ(lb_count_nonempty(NULL), 0);
    CHECK_EQ(lb_find_prefix(NULL, "x"), -1);
    CHECK_NULL(lb_filter(NULL, "x"));
    lb_free(NULL);  // No crash

cleanup:
    return;
}

// ═══════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════

int main(void) {
    printf("=== File Processor Tests ===\n\n");

    printf("-- Creation --\n");
    RUN(test_lb_new_creates_empty_buffer);

    printf("\n-- load_file --\n");
    RUN(test_lb_load_file_reads_lines);
    RUN(test_lb_load_file_empty);
    RUN(test_lb_load_file_nonexistent);
    RUN(test_lb_load_file_single_line_no_newline);

    printf("\n-- filter --\n");
    RUN(test_lb_filter_matching_lines);
    RUN(test_lb_filter_no_matches);
    RUN(test_lb_filter_all_match);

    printf("\n-- count_nonempty --\n");
    RUN(test_lb_count_nonempty_mixed);

    printf("\n-- find_prefix --\n");
    RUN(test_lb_find_prefix_found);
    RUN(test_lb_find_prefix_not_found);

    printf("\n-- null safety --\n");
    RUN(test_lb_null_safety);

    printf("\n═════════════════════════════════════════\n");
    printf("Assertions: %d passed, %d failed\n", _pass, _fail);
    return _fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

Observa como las fixtures se componen:
- `FsFixture` maneja directorio temporal + archivo
- `LbFixture` contiene un `FsFixture` + un `LineBuffer` cargado
- `lb_teardown` llama a `lb_free` y luego `fs_teardown` — limpieza en cascada
- Cada test que crea un `LineBuffer` adicional (como `filtered`) lo libera en su propio `cleanup:`

---

## 12. Programa de practica

### Objetivo

Implementar y testear un **key-value store basado en archivo** — un modulo que persiste pares clave-valor en un archivo de texto y los carga al iniciar.

### Especificacion

```c
// kvstore.h
#ifndef KVSTORE_H
#define KVSTORE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct KVStore KVStore;

// Abre o crea un key-value store en el path dado
KVStore *kvs_open(const char *path);

// Cierra el store y libera recursos (flush automatico)
void kvs_close(KVStore *store);

// Operaciones CRUD
bool        kvs_set(KVStore *store, const char *key, const char *value);
const char *kvs_get(const KVStore *store, const char *key);
bool        kvs_delete(KVStore *store, const char *key);
bool        kvs_exists(const KVStore *store, const char *key);

// Persistencia
int kvs_save(KVStore *store);  // Escribe a disco
int kvs_load(KVStore *store);  // Lee de disco (sobreescribe datos en memoria)

// Consultas
size_t kvs_count(const KVStore *store);

#endif
```

### Fixture necesaria

```c
typedef struct {
    KVStore *store;
    char tmpdir[256];
    char filepath[512];
} KvsFixture;

// Setup: crea dir temporal, abre store
// Teardown: cierra store, borra dir temporal
```

### Tests que escribir

| Grupo | Test | Fixture |
|-------|------|---------|
| Creacion | `test_kvs_open_creates_store` | FsFixture (dir temporal) |
| Set/Get | `test_kvs_set_then_get`, `test_kvs_set_overwrites`, `test_kvs_get_missing_returns_null` | KvsFixture (store abierto) |
| Delete | `test_kvs_delete_existing`, `test_kvs_delete_nonexistent` | KvsFixture |
| Persistencia | `test_kvs_save_and_reload`, `test_kvs_load_preserves_all_data` | KvsFixture (set datos, save, close, reopen, verify) |
| Edge cases | `test_kvs_empty_key`, `test_kvs_empty_value`, `test_kvs_value_with_equals_sign` | KvsFixture |
| Null safety | `test_kvs_null_safety` | Ninguna |

Cada test debe usar `goto cleanup` con `teardown` garantizado. Verificar con `-fsanitize=address` que no hay leaks.

---

## 13. Ejercicios

### Ejercicio 1: Migrar tests sin cleanup a goto cleanup

Toma los tests del ring buffer (T01) o de la linked list (T02) y reescribelos usando el patron `CHECK`/`goto cleanup`. Verifica con Valgrind que no hay leaks cuando un test falla.

### Ejercicio 2: Runner con setjmp/longjmp

Implementa un test runner que use setjmp/longjmp (como el de la seccion 4) y usalo para ejecutar tests de alguna estructura de datos. Verifica que el teardown se ejecuta incluso cuando un ASSERT falla.

### Ejercicio 3: Runner con fork

Implementa un test runner que use fork (como el de la seccion 7) y usalo para ejecutar tests que incluyan un test que deliberadamente hace segfault y otro que deliberadamente hace double-free. Verifica que el runner sobrevive y reporta los crashes correctamente.

### Ejercicio 4: Fixture con base de datos SQLite

Si tienes SQLite instalado (`sudo apt install libsqlite3-dev`), implementa una fixture que:
1. Crea una base de datos en `/tmp/test_XXXXXX/test.db`
2. Crea una tabla `items (id INTEGER, name TEXT, price REAL)`
3. Inserta 3 registros de prueba
4. Cada test recibe la fixture, opera sobre la DB, y el teardown borra todo

Compilar: `gcc -lsqlite3 ...`

---

## 14. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│            FIXTURES Y SETUP/TEARDOWN — MAPA MENTAL                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  Problema: C no tiene RAII, Drop, GC, defer, ni finally                      │
│  → Si un test falla a mitad, el cleanup no se ejecuta                        │
│  → Memory leaks, file descriptors abiertos, archivos temporales              │
│                                                                                │
│  Soluciones:                                                                  │
│  ├─ goto cleanup ← mas comun, kernel Linux lo usa, simple y efectivo         │
│  │   CHECK(expr) → falla → goto cleanup → teardown siempre                  │
│  │                                                                            │
│  ├─ setjmp/longjmp ← runner que captura fallos y llama teardown              │
│  │   ASSERT → longjmp → runner → teardown → siguiente test                  │
│  │                                                                            │
│  ├─ fork() ← aislamiento total, el OS limpia todo                           │
│  │   fork → hijo ejecuta test → hijo muere → padre continua                 │
│  │   No hay leaks posibles (el kernel libera todo)                           │
│  │                                                                            │
│  └─ atexit ← solo para exit/return, NO para abort/signals                    │
│                                                                                │
│  Fixture = estado predefinido para el test                                    │
│  ├─ Struct con todos los recursos necesarios                                  │
│  ├─ setup(): crea e inicializa                                                │
│  ├─ teardown(): libera y limpia                                               │
│  └─ Per-test (recomendado) o per-suite (para recursos costosos)              │
│                                                                                │
│  Constructores/Destructores en C:                                             │
│  ├─ modulo_new() / modulo_free() ← heap (devuelve puntero)                  │
│  ├─ modulo_init() / modulo_deinit() ← stack (inicializa struct existente)    │
│  ├─ Destructor debe aceptar NULL gracefully                                  │
│  └─ Destruccion en cascada: el raiz destruye todo lo que posee               │
│                                                                                │
│  Comparacion:                                                                 │
│  ├─ C: manual (goto, setjmp, fork)                                           │
│  ├─ Rust: automatico (RAII / Drop, incluso en panic)                         │
│  ├─ Go: t.Cleanup() (registrar funcion, se ejecuta siempre)                 │
│  ├─ Unity: setUp()/tearDown() automaticos                                    │
│  ├─ CMocka: setup/teardown pasados como parametro                            │
│  └─ Check: fork() por defecto                                                │
│                                                                                │
│  Fin de la Seccion 1 — Fundamentos                                           │
│  Proxima seccion: S02 — Frameworks de Testing en C (Unity, CMocka, CTest)    │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: T03 — Compilacion condicional para tests | Siguiente: S02/T01 — Unity →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 1 — Fundamentos > Topico 4 — Fixtures y setup/teardown*

*Fin de la Seccion 1 — Fundamentos*
