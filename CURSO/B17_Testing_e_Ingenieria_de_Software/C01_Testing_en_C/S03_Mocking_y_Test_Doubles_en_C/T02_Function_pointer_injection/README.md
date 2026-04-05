# Function pointer injection — reemplazar dependencias via struct de function pointers (Strategy = DI natural en C)

## 1. El concepto

En el topico anterior vimos que para testear codigo con dependencias externas necesitamos **inyectar** test doubles. La tecnica de **function pointer injection** consiste en pasar las dependencias como **punteros a funcion**, de modo que el codigo bajo test llama a la dependencia a traves del puntero, y en tests puedes reemplazarlo con un stub/fake/spy/mock.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                   FUNCTION POINTER INJECTION                                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Sin inyeccion (acoplamiento directo):                                          │
│  ┌──────────────────────┐     ┌──────────────────┐                              │
│  │  user_service.c      │     │  database.c      │                              │
│  │                      │────▶│                  │                              │
│  │  db_insert_user(...) │     │  PostgreSQL code │                              │
│  │  ← llamada directa   │     │                  │                              │
│  └──────────────────────┘     └──────────────────┘                              │
│  El nombre "db_insert_user" esta hardcodeado. No puedes cambiarlo              │
│  sin recompilar o usar trucos del linker.                                       │
│                                                                                  │
│  Con function pointer injection:                                                │
│  ┌──────────────────────┐     ┌──────────────────┐                              │
│  │  user_service.c      │     │  database.c      │                              │
│  │                      │     │                  │                              │
│  │  deps->insert(...)   │──?──│  PostgreSQL code │                              │
│  │  ← via puntero       │     │                  │                              │
│  └──────────────────────┘     └──────────────────┘                              │
│         │                            ▲                                           │
│         │                            │                                           │
│         │     ┌──────────────────┐   │                                           │
│         └──?──│  stub/fake/mock  │   │                                           │
│               │  (en tests)      │   │                                           │
│               └──────────────────┘   │                                           │
│                                      │                                           │
│  deps->insert puede apuntar a la implementacion real O al test double.         │
│  El codigo de user_service.c NO cambia. Solo cambia a donde apunta deps.       │
│                                                                                  │
│  Esto es DEPENDENCY INJECTION en C.                                             │
│  Es el patron Strategy de GOF implementado sin clases.                          │
│  Es la forma natural de hacer DI cuando no tienes polimorfismo de objetos.     │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Relacion con patrones de diseño

| Patron OOP | Equivalente en C | Uso en testing |
|------------|-----------------|----------------|
| Strategy | Struct de function pointers | Inyectar test doubles |
| Dependency Injection | Pasar struct como parametro | Constructor injection |
| Interface | typedef de function pointer | Definir contrato |
| Abstract Factory | Function pointer que retorna struct | Crear implementaciones |
| Adapter | Wrapper function pointer | Adaptar API incompatible |

En lenguajes OOP, estas tecnicas requieren interfaces, clases abstractas, y frameworks de DI. En C, un `struct` con function pointers es todo lo que necesitas.

---

## 2. Function pointers en C — repaso rapido

### 2.1 Sintaxis basica

```c
// Declarar un puntero a funcion
int (*func_ptr)(int, int);

// Asignar
int add(int a, int b) { return a + b; }
func_ptr = add;      // sin &, el nombre de la funcion ya es un puntero
func_ptr = &add;     // equivalente, mas explicito

// Llamar
int result = func_ptr(2, 3);    // sin *
int result = (*func_ptr)(2, 3); // equivalente, mas explicito

// typedef para legibilidad
typedef int (*BinaryOp)(int, int);
BinaryOp op = add;
int result = op(2, 3);
```

### 2.2 Punteros a funcion con void*

```c
// Patron comun en C: pasar contexto generico con void*
typedef int (*Callback)(void *context, const char *data);

int my_handler(void *ctx, const char *data) {
    MyState *state = (MyState *)ctx;
    // usar state y data
    return 0;
}

// Registro:
register_callback(my_handler, &my_state);
```

### 2.3 Struct de function pointers = interfaz

```c
// Esto es lo mas parecido a una interfaz/trait que C tiene:
typedef struct {
    int (*open)(const char *path);
    int (*read)(char *buf, int size);
    int (*write)(const char *buf, int size);
    int (*close)(void);
} FileOps;

// "Implementacion" (como una clase que implementa una interfaz):
static int real_open(const char *path) { return open(path, O_RDONLY); }
static int real_read(char *buf, int size) { /* ... */ }
static int real_write(const char *buf, int size) { /* ... */ }
static int real_close(void) { /* ... */ }

FileOps real_file_ops = {
    .open  = real_open,
    .read  = real_read,
    .write = real_write,
    .close = real_close,
};

// Otra "implementacion" para tests:
FileOps fake_file_ops = {
    .open  = fake_open,
    .read  = fake_read,
    .write = fake_write,
    .close = fake_close,
};
```

---

## 3. Patron basico — struct de dependencias

### 3.1 El patron

```c
// Paso 1: Definir la "interfaz" como struct de function pointers
typedef struct {
    int (*find_user)(const char *email, UserRecord *out);
    int (*insert_user)(const UserRecord *user);
    int (*delete_user)(int user_id);
} DatabaseOps;

// Paso 2: El codigo bajo test recibe la interfaz como parametro
int user_service_create(const char *name, const char *email,
                        const DatabaseOps *db);

// Paso 3: Implementacion real
static int pg_find_user(const char *email, UserRecord *out) {
    // Query real a PostgreSQL
    PGresult *res = PQexec(conn, query);
    // ...
}
// ... (pg_insert_user, pg_delete_user)

const DatabaseOps postgres_db = {
    .find_user   = pg_find_user,
    .insert_user = pg_insert_user,
    .delete_user = pg_delete_user,
};

// Paso 4: En produccion
int main(void) {
    user_service_create("Alice", "alice@real.com", &postgres_db);
}

// Paso 5: En tests — inyectar stub/fake/spy/mock
static int stub_find(const char *email, UserRecord *out) {
    (void)email; (void)out;
    return -1;  // usuario no existe
}
static int stub_insert(const UserRecord *u) {
    (void)u;
    return 42;  // ID asignado
}

const DatabaseOps stub_db = {
    .find_user   = stub_find,
    .insert_user = stub_insert,
    .delete_user = NULL,  // no se usa en este test
};

static void test_create_user(void) {
    int id = user_service_create("Alice", "alice@test.com", &stub_db);
    assert(id == 42);
}
```

### 3.2 Implementacion del codigo bajo test

```c
// user_service.c
#include "user_service.h"
#include <string.h>
#include <stdio.h>

int user_service_create(const char *name, const char *email,
                        const DatabaseOps *db) {
    if (!name || !email || !db) return -1;

    // Verificar duplicado
    UserRecord existing;
    if (db->find_user(email, &existing) == 0) {
        return -1;  // ya existe
    }

    // Crear registro
    UserRecord new_user = {0};
    snprintf(new_user.name, sizeof(new_user.name), "%s", name);
    snprintf(new_user.email, sizeof(new_user.email), "%s", email);

    // Insertar via la interfaz
    return db->insert_user(&new_user);
}
```

El codigo NO sabe si `db->find_user` habla con PostgreSQL, con un array en memoria, o con un stub que retorna -1. **Ese es el punto.**

---

## 4. Ejemplo completo — servicio de cache con dependencias inyectadas

### 4.1 Interfaces (structs de function pointers)

```c
// cache_service.h
#ifndef CACHE_SERVICE_H
#define CACHE_SERVICE_H

#include <stdbool.h>
#include <time.h>

// --- Interfaz: Storage backend ---
typedef struct {
    int (*get)(void *ctx, const char *key, char *value, int value_size);
    int (*set)(void *ctx, const char *key, const char *value, int ttl_seconds);
    int (*del)(void *ctx, const char *key);
    bool (*exists)(void *ctx, const char *key);
} StorageOps;

// --- Interfaz: Clock ---
typedef struct {
    time_t (*now)(void *ctx);
} ClockOps;

// --- Interfaz: Metrics ---
typedef struct {
    void (*increment)(void *ctx, const char *metric_name);
    void (*record_timing)(void *ctx, const char *metric_name, double ms);
} MetricsOps;

// --- Interfaz: Logger ---
typedef struct {
    void (*log_debug)(void *ctx, const char *msg);
    void (*log_info)(void *ctx, const char *msg);
    void (*log_error)(void *ctx, const char *msg);
} LoggerOps;

// --- Servicio de cache ---
typedef struct {
    const StorageOps *storage;
    void *storage_ctx;
    const ClockOps *clock;
    void *clock_ctx;
    const MetricsOps *metrics;
    void *metrics_ctx;
    const LoggerOps *logger;
    void *logger_ctx;
    int default_ttl;
} CacheService;

// Constructor
CacheService cache_service_new(
    const StorageOps *storage, void *storage_ctx,
    const ClockOps *clock, void *clock_ctx,
    const MetricsOps *metrics, void *metrics_ctx,
    const LoggerOps *logger, void *logger_ctx,
    int default_ttl
);

// Operaciones del servicio
int cache_get(CacheService *cs, const char *key, char *value, int value_size);
int cache_set(CacheService *cs, const char *key, const char *value);
int cache_set_with_ttl(CacheService *cs, const char *key, const char *value,
                       int ttl_seconds);
int cache_delete(CacheService *cs, const char *key);
int cache_get_or_compute(CacheService *cs, const char *key,
                         char *value, int value_size,
                         int (*compute)(const char *key, char *value, int size));

#endif
```

### 4.2 Implementacion del servicio

```c
// cache_service.c
#include "cache_service.h"
#include <stdio.h>
#include <string.h>

CacheService cache_service_new(
    const StorageOps *storage, void *storage_ctx,
    const ClockOps *clock, void *clock_ctx,
    const MetricsOps *metrics, void *metrics_ctx,
    const LoggerOps *logger, void *logger_ctx,
    int default_ttl)
{
    return (CacheService){
        .storage = storage, .storage_ctx = storage_ctx,
        .clock = clock, .clock_ctx = clock_ctx,
        .metrics = metrics, .metrics_ctx = metrics_ctx,
        .logger = logger, .logger_ctx = logger_ctx,
        .default_ttl = default_ttl,
    };
}

int cache_get(CacheService *cs, const char *key, char *value, int value_size) {
    if (!cs || !key || !value) return -1;

    time_t start = cs->clock->now(cs->clock_ctx);

    int result = cs->storage->get(cs->storage_ctx, key, value, value_size);

    time_t end = cs->clock->now(cs->clock_ctx);
    double elapsed_ms = difftime(end, start) * 1000.0;

    if (result == 0) {
        cs->metrics->increment(cs->metrics_ctx, "cache.hit");
        cs->metrics->record_timing(cs->metrics_ctx, "cache.get.ms", elapsed_ms);
        cs->logger->log_debug(cs->logger_ctx, "cache HIT");
    } else {
        cs->metrics->increment(cs->metrics_ctx, "cache.miss");
        cs->logger->log_debug(cs->logger_ctx, "cache MISS");
    }

    return result;
}

int cache_set(CacheService *cs, const char *key, const char *value) {
    return cache_set_with_ttl(cs, key, value, cs->default_ttl);
}

int cache_set_with_ttl(CacheService *cs, const char *key, const char *value,
                       int ttl_seconds) {
    if (!cs || !key || !value) return -1;
    if (ttl_seconds <= 0) ttl_seconds = cs->default_ttl;

    int result = cs->storage->set(cs->storage_ctx, key, value, ttl_seconds);

    if (result == 0) {
        cs->metrics->increment(cs->metrics_ctx, "cache.set");
        cs->logger->log_debug(cs->logger_ctx, "cache SET");
    } else {
        cs->metrics->increment(cs->metrics_ctx, "cache.set.error");
        cs->logger->log_error(cs->logger_ctx, "cache SET failed");
    }

    return result;
}

int cache_delete(CacheService *cs, const char *key) {
    if (!cs || !key) return -1;

    int result = cs->storage->del(cs->storage_ctx, key);
    cs->metrics->increment(cs->metrics_ctx, "cache.delete");
    return result;
}

int cache_get_or_compute(CacheService *cs, const char *key,
                         char *value, int value_size,
                         int (*compute)(const char *key, char *value, int size)) {
    if (!cs || !key || !value || !compute) return -1;

    // Intentar obtener del cache
    if (cache_get(cs, key, value, value_size) == 0) {
        return 0;  // cache hit
    }

    // Cache miss: computar el valor
    cs->logger->log_info(cs->logger_ctx, "computing value for cache miss");
    int rc = compute(key, value, value_size);
    if (rc != 0) {
        cs->logger->log_error(cs->logger_ctx, "compute failed");
        return rc;
    }

    // Guardar en cache para la proxima vez
    cache_set(cs, key, value);

    return 0;
}
```

### 4.3 Tests con doubles inyectados

```c
// test_cache_service.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cache_service.h"

// =====================================================================
// DUMMY — Logger que no hace nada
// =====================================================================
static void dummy_log(void *ctx, const char *msg) {
    (void)ctx; (void)msg;
}

static const LoggerOps dummy_logger = {
    .log_debug = dummy_log,
    .log_info  = dummy_log,
    .log_error = dummy_log,
};

// =====================================================================
// STUB — Clock que retorna valor fijo
// =====================================================================
typedef struct {
    time_t current;
} StubClock;

static time_t stub_clock_now(void *ctx) {
    StubClock *c = (StubClock *)ctx;
    return c->current;
}

static const ClockOps stub_clock_ops = {
    .now = stub_clock_now,
};

// =====================================================================
// FAKE — Storage en memoria (hash map simplificado)
// =====================================================================
#define FAKE_STORAGE_MAX 64

typedef struct {
    char key[128];
    char value[256];
    int ttl;
    bool occupied;
} FakeEntry;

typedef struct {
    FakeEntry entries[FAKE_STORAGE_MAX];
    int count;
} FakeStorage;

static void fake_storage_init(FakeStorage *fs) {
    memset(fs, 0, sizeof(*fs));
}

static FakeEntry *fake_storage_find(FakeStorage *fs, const char *key) {
    for (int i = 0; i < FAKE_STORAGE_MAX; i++) {
        if (fs->entries[i].occupied && strcmp(fs->entries[i].key, key) == 0) {
            return &fs->entries[i];
        }
    }
    return NULL;
}

static int fake_get(void *ctx, const char *key, char *value, int value_size) {
    FakeStorage *fs = (FakeStorage *)ctx;
    FakeEntry *e = fake_storage_find(fs, key);
    if (!e) return -1;
    snprintf(value, value_size, "%s", e->value);
    return 0;
}

static int fake_set(void *ctx, const char *key, const char *value, int ttl) {
    FakeStorage *fs = (FakeStorage *)ctx;

    // Actualizar si existe
    FakeEntry *e = fake_storage_find(fs, key);
    if (e) {
        snprintf(e->value, sizeof(e->value), "%s", value);
        e->ttl = ttl;
        return 0;
    }

    // Insertar nuevo
    for (int i = 0; i < FAKE_STORAGE_MAX; i++) {
        if (!fs->entries[i].occupied) {
            fs->entries[i].occupied = true;
            snprintf(fs->entries[i].key, sizeof(fs->entries[i].key), "%s", key);
            snprintf(fs->entries[i].value, sizeof(fs->entries[i].value),
                     "%s", value);
            fs->entries[i].ttl = ttl;
            fs->count++;
            return 0;
        }
    }
    return -1;  // lleno
}

static int fake_del(void *ctx, const char *key) {
    FakeStorage *fs = (FakeStorage *)ctx;
    FakeEntry *e = fake_storage_find(fs, key);
    if (!e) return -1;
    e->occupied = false;
    fs->count--;
    return 0;
}

static bool fake_exists(void *ctx, const char *key) {
    FakeStorage *fs = (FakeStorage *)ctx;
    return fake_storage_find(fs, key) != NULL;
}

static const StorageOps fake_storage_ops = {
    .get    = fake_get,
    .set    = fake_set,
    .del    = fake_del,
    .exists = fake_exists,
};

// =====================================================================
// SPY — Metrics que graba las llamadas
// =====================================================================
#define SPY_MAX_METRICS 64

typedef struct {
    char name[64];
    int count;
    double last_value;
} MetricRecord;

typedef struct {
    MetricRecord records[SPY_MAX_METRICS];
    int record_count;
    int total_calls;
} MetricsSpy;

static void metrics_spy_init(MetricsSpy *spy) {
    memset(spy, 0, sizeof(*spy));
}

static MetricRecord *metrics_spy_find(MetricsSpy *spy, const char *name) {
    for (int i = 0; i < spy->record_count; i++) {
        if (strcmp(spy->records[i].name, name) == 0) {
            return &spy->records[i];
        }
    }
    return NULL;
}

static void spy_increment(void *ctx, const char *name) {
    MetricsSpy *spy = (MetricsSpy *)ctx;
    spy->total_calls++;

    MetricRecord *r = metrics_spy_find(spy, name);
    if (r) {
        r->count++;
    } else if (spy->record_count < SPY_MAX_METRICS) {
        r = &spy->records[spy->record_count++];
        snprintf(r->name, sizeof(r->name), "%s", name);
        r->count = 1;
    }
}

static void spy_record_timing(void *ctx, const char *name, double ms) {
    MetricsSpy *spy = (MetricsSpy *)ctx;
    spy->total_calls++;

    MetricRecord *r = metrics_spy_find(spy, name);
    if (r) {
        r->count++;
        r->last_value = ms;
    } else if (spy->record_count < SPY_MAX_METRICS) {
        r = &spy->records[spy->record_count++];
        snprintf(r->name, sizeof(r->name), "%s", name);
        r->count = 1;
        r->last_value = ms;
    }
}

static int metrics_spy_get_count(MetricsSpy *spy, const char *name) {
    MetricRecord *r = metrics_spy_find(spy, name);
    return r ? r->count : 0;
}

static const MetricsOps spy_metrics_ops = {
    .increment     = spy_increment,
    .record_timing = spy_record_timing,
};

// =====================================================================
// TESTS
// =====================================================================

static FakeStorage storage;
static StubClock clock_state;
static MetricsSpy metrics_spy;

static CacheService create_test_service(void) {
    fake_storage_init(&storage);
    clock_state.current = 1000;
    metrics_spy_init(&metrics_spy);

    return cache_service_new(
        &fake_storage_ops, &storage,
        &stub_clock_ops, &clock_state,
        &spy_metrics_ops, &metrics_spy,
        &dummy_logger, NULL,
        3600  // default TTL: 1 hora
    );
}

// --- Test: cache miss ---
static void test_get_cache_miss(void) {
    CacheService cs = create_test_service();

    char value[256];
    int rc = cache_get(&cs, "nonexistent", value, sizeof(value));

    assert(rc == -1);
    assert(metrics_spy_get_count(&metrics_spy, "cache.miss") == 1);
    assert(metrics_spy_get_count(&metrics_spy, "cache.hit") == 0);

    printf("  PASS: get cache miss\n");
}

// --- Test: set + get = hit ---
static void test_set_then_get_is_hit(void) {
    CacheService cs = create_test_service();

    int rc = cache_set(&cs, "user:1", "Alice");
    assert(rc == 0);

    char value[256];
    rc = cache_get(&cs, "user:1", value, sizeof(value));
    assert(rc == 0);
    assert(strcmp(value, "Alice") == 0);
    assert(metrics_spy_get_count(&metrics_spy, "cache.hit") == 1);
    assert(metrics_spy_get_count(&metrics_spy, "cache.set") == 1);

    printf("  PASS: set then get is hit\n");
}

// --- Test: overwrite ---
static void test_set_overwrites_value(void) {
    CacheService cs = create_test_service();

    cache_set(&cs, "key", "value1");
    cache_set(&cs, "key", "value2");

    char value[256];
    cache_get(&cs, "key", value, sizeof(value));
    assert(strcmp(value, "value2") == 0);
    assert(metrics_spy_get_count(&metrics_spy, "cache.set") == 2);

    printf("  PASS: set overwrites value\n");
}

// --- Test: delete ---
static void test_delete_removes_key(void) {
    CacheService cs = create_test_service();

    cache_set(&cs, "key", "value");
    int rc = cache_delete(&cs, "key");
    assert(rc == 0);

    char value[256];
    rc = cache_get(&cs, "key", value, sizeof(value));
    assert(rc == -1);  // ya no existe
    assert(metrics_spy_get_count(&metrics_spy, "cache.delete") == 1);

    printf("  PASS: delete removes key\n");
}

// --- Test: get_or_compute (cache miss → compute → set) ---
static int compute_factorial(const char *key, char *value, int size) {
    // Ignorar key, calcular factorial de 5
    snprintf(value, size, "120");
    return 0;
}

static void test_get_or_compute_miss(void) {
    CacheService cs = create_test_service();

    char value[256];
    int rc = cache_get_or_compute(&cs, "factorial:5", value, sizeof(value),
                                  compute_factorial);
    assert(rc == 0);
    assert(strcmp(value, "120") == 0);

    // Verificar que se guardo en cache
    char cached[256];
    rc = cache_get(&cs, "factorial:5", cached, sizeof(cached));
    assert(rc == 0);
    assert(strcmp(cached, "120") == 0);

    // Metricas: 1 miss (primer get) + 1 set + 1 hit (segundo get)
    assert(metrics_spy_get_count(&metrics_spy, "cache.miss") == 1);
    assert(metrics_spy_get_count(&metrics_spy, "cache.set") == 1);
    assert(metrics_spy_get_count(&metrics_spy, "cache.hit") == 1);

    printf("  PASS: get_or_compute cache miss\n");
}

// --- Test: get_or_compute (cache hit → no compute) ---
static int compute_should_not_be_called(const char *key, char *value, int size) {
    (void)key; (void)value; (void)size;
    assert(0 && "compute should not be called on cache hit!");
    return -1;
}

static void test_get_or_compute_hit(void) {
    CacheService cs = create_test_service();

    // Pre-poblar el cache
    cache_set(&cs, "factorial:5", "120");
    metrics_spy_init(&metrics_spy);  // resetear metricas

    char value[256];
    int rc = cache_get_or_compute(&cs, "factorial:5", value, sizeof(value),
                                  compute_should_not_be_called);
    assert(rc == 0);
    assert(strcmp(value, "120") == 0);
    assert(metrics_spy_get_count(&metrics_spy, "cache.hit") == 1);

    printf("  PASS: get_or_compute cache hit (compute not called)\n");
}

// --- Test: compute failure ---
static int compute_fails(const char *key, char *value, int size) {
    (void)key; (void)value; (void)size;
    return -1;  // error
}

static void test_get_or_compute_failure(void) {
    CacheService cs = create_test_service();

    char value[256] = "";
    int rc = cache_get_or_compute(&cs, "failing", value, sizeof(value),
                                  compute_fails);
    assert(rc == -1);

    // Verificar que NO se guardo en cache
    char check[256];
    rc = cache_get(&cs, "failing", check, sizeof(check));
    assert(rc == -1);

    printf("  PASS: get_or_compute compute failure\n");
}

// --- Test: NULL handling ---
static void test_null_parameters(void) {
    CacheService cs = create_test_service();

    assert(cache_get(&cs, NULL, NULL, 0) == -1);
    assert(cache_set(&cs, NULL, NULL) == -1);
    assert(cache_delete(&cs, NULL) == -1);
    assert(cache_get_or_compute(&cs, NULL, NULL, 0, NULL) == -1);

    printf("  PASS: null parameters handled\n");
}

int main(void) {
    printf("=== CacheService tests (function pointer injection) ===\n");
    test_get_cache_miss();
    test_set_then_get_is_hit();
    test_set_overwrites_value();
    test_delete_removes_key();
    test_get_or_compute_miss();
    test_get_or_compute_hit();
    test_get_or_compute_failure();
    test_null_parameters();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 4.4 Diagrama de inyeccion

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  PRODUCCION                          TESTS                                      │
│                                                                                  │
│  ┌──────────────┐                    ┌──────────────┐                           │
│  │ RedisStorage  │                    │ FakeStorage  │                           │
│  │ redis_get()   │                    │ fake_get()   │                           │
│  │ redis_set()   │                    │ (array)      │                           │
│  └──────┬───────┘                    └──────┬───────┘                           │
│         │                                    │                                   │
│         ├──── StorageOps ────────────────────┤                                   │
│         │     .get = ?                       │                                   │
│         │     .set = ?                       │                                   │
│         │                                    │                                   │
│  ┌──────┴───────────────────────────────────┴───────┐                           │
│  │              cache_service.c                      │                           │
│  │    cs->storage->get(cs->storage_ctx, key, ...)    │                           │
│  │    ← NO sabe si habla con Redis o con un array    │                           │
│  └───────────────────────────────────────────────────┘                           │
│                                                                                  │
│  Lo mismo para clock, metrics, logger:                                          │
│  Produccion: reloj real, Datadog, syslog                                        │
│  Tests: stub clock, spy metrics, dummy logger                                   │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. El parametro void *ctx — contexto opaco

### 5.1 Por que void*

En C no hay `this`. Cuando tienes un struct de function pointers, las funciones no saben sobre que datos operar a menos que se los pases. El patron `void *ctx` resuelve esto:

```c
// Sin ctx: la funcion necesita acceso a un global
int global_count = 0;
static void increment(const char *name) {
    global_count++;  // MAL: estado global
}

// Con ctx: la funcion recibe su estado como parametro
typedef struct { int count; } MetricsState;

static void increment(void *ctx, const char *name) {
    MetricsState *state = (MetricsState *)ctx;
    state->count++;  // BIEN: estado local
}
```

### 5.2 Ventajas del void *ctx

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  void *ctx — POR QUE ES IMPORTANTE                                              │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  1. EVITA ESTADO GLOBAL                                                         │
│     Sin ctx, cada implementacion necesita globals.                              │
│     Los globals rompen el aislamiento entre tests.                              │
│                                                                                  │
│  2. PERMITE MULTIPLES INSTANCIAS                                                │
│     Puedes tener dos CacheService con storage contexts diferentes.              │
│     Con globals, solo puedes tener uno.                                         │
│                                                                                  │
│  3. TYPE-SAFE (dentro de cada implementacion)                                   │
│     El cast a (FakeStorage *) es inseguro,                                      │
│     pero solo ocurre en UN lugar, dentro de fake_get().                         │
│                                                                                  │
│  4. EQUIVALENTE A self/this DE OOP                                              │
│     C:     func(void *ctx, ...)     ctx = (MyType *)ctx;                        │
│     Python: def method(self, ...):                                               │
│     Rust:   fn method(&self, ...)                                                │
│     Go:     func (s *MyType) method(...)                                         │
│                                                                                  │
│  Alternativa sin void*: puntero tipado en el struct                             │
│  typedef struct {                                                               │
│      int (*get)(struct StorageImpl *self, ...);                                 │
│  } StorageOps;                                                                  │
│  Pero esto requiere conocer el tipo concreto → pierde la genericidad.           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Patron con ctx empaquetado en el struct

Variante donde el ctx esta dentro del struct de operaciones:

```c
// Variante: ctx dentro del struct
typedef struct {
    void *ctx;
    int (*get)(void *ctx, const char *key, char *value, int size);
    int (*set)(void *ctx, const char *key, const char *value, int ttl);
    int (*del)(void *ctx, const char *key);
} Storage;

// Constructor para produccion:
Storage storage_redis_new(const char *host, int port) {
    RedisCtx *r = redis_connect(host, port);
    return (Storage){
        .ctx = r,
        .get = redis_get,
        .set = redis_set,
        .del = redis_del,
    };
}

// Constructor para tests:
Storage storage_fake_new(void) {
    FakeStorage *fs = calloc(1, sizeof(FakeStorage));
    return (Storage){
        .ctx = fs,
        .get = fake_get,
        .set = fake_set,
        .del = fake_del,
    };
}

// El usuario llama:
int rc = storage.get(storage.ctx, "key", value, sizeof(value));

// Convenience macro para ocultar el ctx:
#define STORAGE_GET(s, key, val, size) ((s)->get((s)->ctx, key, val, size))
int rc = STORAGE_GET(&storage, "key", value, sizeof(value));
```

---

## 6. Variantes del patron

### 6.1 Puntero a funcion individual (sin struct)

Cuando solo hay UNA dependencia, no necesitas un struct:

```c
// Una sola dependencia: time
typedef time_t (*GetTimeFn)(void);

int token_create(const char *user_id, int ttl, GetTimeFn get_time) {
    time_t now = get_time();
    // ...
}

// Produccion:
time_t real_time(void) { return time(NULL); }
token_create("user1", 3600, real_time);

// Test:
time_t stub_time(void) { return 1000; }
token_create("user1", 3600, stub_time);
```

### 6.2 Global mutable (patron menos puro pero comun)

Cuando no puedes cambiar la API para agregar parametros:

```c
// Global que apunta a la implementacion actual
static const StorageOps *current_storage = NULL;
static void *current_storage_ctx = NULL;

void storage_set_impl(const StorageOps *ops, void *ctx) {
    current_storage = ops;
    current_storage_ctx = ctx;
}

// El codigo usa el global
int cache_get(const char *key, char *value, int size) {
    return current_storage->get(current_storage_ctx, key, value, size);
}

// Produccion:
storage_set_impl(&redis_ops, redis_ctx);

// Tests:
storage_set_impl(&fake_ops, &fake_storage);
```

**Cuidado**: este patron tiene los problemas de estado global (no thread-safe, dificil de paralelizar tests). Preferir la inyeccion por parametro cuando sea posible.

### 6.3 Struct con datos + operaciones (OOP en C)

El patron mas completo, similar a una vtable de C++:

```c
// "Clase base" con vtable
typedef struct Database Database;

typedef struct {
    int (*find_user)(Database *self, const char *email, UserRecord *out);
    int (*insert_user)(Database *self, const UserRecord *user);
    void (*destroy)(Database *self);
} DatabaseVTable;

struct Database {
    const DatabaseVTable *vtable;
    // Subtipos agregan sus propios datos despues
};

// "Clase derivada" — PostgreSQL
typedef struct {
    Database base;         // DEBE ser el primer miembro
    PGconn *connection;
    char schema[64];
} PostgresDatabase;

static int pg_find(Database *self, const char *email, UserRecord *out) {
    PostgresDatabase *pg = (PostgresDatabase *)self;
    // usar pg->connection para hacer query
    return 0;
}

static int pg_insert(Database *self, const UserRecord *user) {
    PostgresDatabase *pg = (PostgresDatabase *)self;
    // INSERT INTO ...
    return 42;
}

static void pg_destroy(Database *self) {
    PostgresDatabase *pg = (PostgresDatabase *)self;
    PQfinish(pg->connection);
    free(pg);
}

static const DatabaseVTable pg_vtable = {
    .find_user   = pg_find,
    .insert_user = pg_insert,
    .destroy     = pg_destroy,
};

Database *postgres_database_new(const char *conninfo) {
    PostgresDatabase *pg = calloc(1, sizeof(PostgresDatabase));
    pg->base.vtable = &pg_vtable;
    pg->connection = PQconnectdb(conninfo);
    return (Database *)pg;
}

// "Clase derivada" — Fake para tests
typedef struct {
    Database base;
    UserRecord records[100];
    int count;
    int next_id;
} FakeDatabase;

static int fake_find(Database *self, const char *email, UserRecord *out) {
    FakeDatabase *fdb = (FakeDatabase *)self;
    for (int i = 0; i < fdb->count; i++) {
        if (strcmp(fdb->records[i].email, email) == 0) {
            if (out) *out = fdb->records[i];
            return 0;
        }
    }
    return -1;
}

static int fake_insert(Database *self, const UserRecord *user) {
    FakeDatabase *fdb = (FakeDatabase *)self;
    if (fdb->count >= 100) return -1;
    fdb->records[fdb->count] = *user;
    fdb->records[fdb->count].id = ++fdb->next_id;
    fdb->count++;
    return fdb->next_id;
}

static void fake_destroy(Database *self) {
    free(self);
}

static const DatabaseVTable fake_vtable = {
    .find_user   = fake_find,
    .insert_user = fake_insert,
    .destroy     = fake_destroy,
};

Database *fake_database_new(void) {
    FakeDatabase *fdb = calloc(1, sizeof(FakeDatabase));
    fdb->base.vtable = &fake_vtable;
    fdb->next_id = 0;
    return (Database *)fdb;
}

// --- El codigo bajo test usa la interfaz generica ---
int user_service_create(Database *db, const char *name, const char *email) {
    UserRecord existing;
    if (db->vtable->find_user(db, email, &existing) == 0) {
        return -1;
    }
    UserRecord new_user = {0};
    snprintf(new_user.name, sizeof(new_user.name), "%s", name);
    snprintf(new_user.email, sizeof(new_user.email), "%s", email);
    return db->vtable->insert_user(db, &new_user);
}

// --- Produccion ---
Database *db = postgres_database_new("host=localhost dbname=myapp");
user_service_create(db, "Alice", "alice@real.com");
db->vtable->destroy(db);

// --- Test ---
Database *db = fake_database_new();
int id = user_service_create(db, "Alice", "alice@test.com");
assert(id == 1);
db->vtable->destroy(db);
```

Este patron es exactamente como funciona el polimorfismo en C++ (vtable), GObject (GNOME), y el VFS de Linux.

---

## 7. Ejemplos reales — donde se usa este patron

### 7.1 Linux kernel — struct file_operations

```c
// El kernel de Linux usa EXACTAMENTE este patron para el VFS:
struct file_operations {
    struct module *owner;
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    int (*ioctl)(struct inode *, struct file *, unsigned int, unsigned long);
    // ... muchos mas
};

// ext4, btrfs, NFS, cada filesystem implementa sus propias funciones
// y las registra en un struct file_operations.
// El VFS llama file->f_op->read() sin saber que filesystem hay debajo.

// Esto es EXACTAMENTE function pointer injection a escala industrial.
```

### 7.2 SQLite — VFS (Virtual File System)

```c
// SQLite usa function pointers para abstraer el OS:
struct sqlite3_vfs {
    int iVersion;
    int szOsFile;
    int mxPathname;
    // ...
    int (*xOpen)(sqlite3_vfs*, const char *zName, sqlite3_file*, int flags, int *);
    int (*xDelete)(sqlite3_vfs*, const char *zName, int syncDir);
    int (*xAccess)(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
    int (*xFullPathname)(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
    // ...
};

// En tests de SQLite, se registra un VFS fake que opera en memoria.
```

### 7.3 cURL — callbacks

```c
// cURL usa function pointers para callbacks:
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_write_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &my_buffer);

// En tests, reemplazas el callback por uno que graba los datos.
```

---

## 8. Ventajas y desventajas

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  FUNCTION POINTER INJECTION — PROS Y CONTRAS                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  VENTAJAS:                                                                      │
│  ✓ Independiente del linker (no necesita --wrap ni tricks)                      │
│  ✓ 100% portable (funciona en cualquier compilador C)                           │
│  ✓ Permite cambiar la implementacion en RUNTIME (no solo en compile-time)       │
│  ✓ Soporta multiples instancias con diferentes implementaciones                │
│  ✓ Hace explicitas las dependencias (estan en la firma de la funcion)           │
│  ✓ Facil de combinar: stub para una dependencia, fake para otra                │
│  ✓ Testable sin herramientas especiales                                         │
│  ✓ Patron usado extensamente en proyectos reales (Linux, SQLite, cURL)          │
│                                                                                  │
│  DESVENTAJAS:                                                                   │
│  ✗ INVASIVO: necesitas cambiar la API del codigo bajo test                     │
│    (agregar parametros para las dependencias)                                   │
│  ✗ Overhead de indirection: llamada a traves de puntero es mas lenta           │
│    que llamada directa (~1-3 ns por llamada, negligible en la mayoria)          │
│  ✗ Mas verbose: structs, typedef, ctx everywhere                               │
│  ✗ Perdida de type-safety por void* (cast manual)                              │
│  ✗ El compilador no puede inline a traves de function pointers                 │
│  ✗ Code navigation (IDE) es peor: "go to definition" lleva al typedef,        │
│    no a la implementacion                                                       │
│  ✗ Si la interfaz cambia, hay que actualizar TODAS las implementaciones         │
│    (no hay error de compilacion si olvidas un campo del struct)                 │
│                                                                                  │
│  CUANDO USAR:                                                                   │
│  • Cuando puedes diseñar la API desde el principio                              │
│  • Cuando la dependencia puede cambiar en runtime (plugins)                     │
│  • Cuando la portabilidad importa (no depender de --wrap de GNU ld)            │
│  • Cuando quieres hacer explicitas las dependencias                             │
│                                                                                  │
│  CUANDO NO USAR:                                                                │
│  • Codigo legacy donde no puedes cambiar la API                                │
│    → Usar link-time substitution o --wrap en su lugar                           │
│  • Funciones de la standard library (malloc, time, fopen)                       │
│    → No puedes agregar function pointer para time()                             │
│  • Cuando el overhead de indirection importa (hot paths en embedded)            │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Comparacion con los otros mecanismos de inyeccion

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  FUNCTION POINTERS vs LINK-TIME vs --WRAP                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Criterio            │ Function Pointer │ Link-time subst │ --wrap (linker)     │
│  ────────────────────┼──────────────────┼─────────────────┼─────────────────    │
│  Cambia la API       │ SI               │ NO              │ NO                  │
│  Cambia el build     │ NO               │ SI (.o files)   │ SI (-Wl,--wrap)     │
│  Runtime-switchable  │ SI               │ NO              │ NO                  │
│  Multiples instancias│ SI               │ NO              │ NO                  │
│  Portable            │ SI               │ SI              │ Solo GNU ld         │
│  Standard library    │ NO (wrappers)    │ Dificil         │ SI                  │
│  Overhead runtime    │ ~1-3 ns/call     │ 0               │ 0                   │
│  Legacy-friendly     │ NO               │ SI              │ SI                  │
│  IDE navigation      │ Peor             │ Normal          │ Normal              │
│  Hace deps explicitas│ SI               │ NO              │ NO                  │
│                                                                                  │
│  Regla practica:                                                                │
│  • Codigo nuevo → Function pointers (diseña para testability)                   │
│  • Codigo existente → Link-time substitution o --wrap                           │
│  • Standard library → --wrap o wrapper functions                                │
│  • Embedded portable → Function pointers (no depender del linker)               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Comparacion con Rust y Go

### 10.1 Rust — traits

```rust
// Rust usa traits, que son la version type-safe de structs de function pointers.
// El compilador verifica en COMPILE TIME que la implementacion es completa.

trait Storage {
    fn get(&self, key: &str) -> Option<String>;
    fn set(&mut self, key: &str, value: &str, ttl: u32) -> Result<(), Error>;
    fn delete(&mut self, key: &str) -> Result<(), Error>;
}

// Implementacion real
struct RedisStorage { /* ... */ }
impl Storage for RedisStorage {
    fn get(&self, key: &str) -> Option<String> { /* ... */ }
    fn set(&mut self, key: &str, value: &str, ttl: u32) -> Result<(), Error> { /* ... */ }
    fn delete(&mut self, key: &str) -> Result<(), Error> { /* ... */ }
}

// Fake para tests
struct FakeStorage {
    data: HashMap<String, String>,
}
impl Storage for FakeStorage {
    fn get(&self, key: &str) -> Option<String> {
        self.data.get(key).cloned()
    }
    fn set(&mut self, key: &str, value: &str, _ttl: u32) -> Result<(), Error> {
        self.data.insert(key.to_string(), value.to_string());
        Ok(())
    }
    fn delete(&mut self, key: &str) -> Result<(), Error> {
        self.data.remove(key);
        Ok(())
    }
}

// Servicio generico sobre el trait
struct CacheService<S: Storage> {
    storage: S,
}

impl<S: Storage> CacheService<S> {
    fn get(&self, key: &str) -> Option<String> {
        self.storage.get(key)
    }
}

// Test
#[test]
fn test_cache_miss() {
    let service = CacheService { storage: FakeStorage::new() };
    assert_eq!(service.get("missing"), None);
}
```

**Diferencia clave con C**:
- Rust verifica que el fake implementa TODOS los metodos del trait en compile time
- En C, si olvidas un campo en el struct, es NULL → crash en runtime
- Rust no necesita void* (generics con monomorphization)
- Rust puede hacer dispatch estatico (generico) O dinamico (`dyn Trait`)

### 10.2 Go — interfaces implicitas

```go
// Go: las interfaces son implicitas. Si un struct tiene los metodos,
// automaticamente "implementa" la interfaz. No necesita "implements".

type Storage interface {
    Get(key string) (string, error)
    Set(key string, value string, ttl int) error
    Delete(key string) error
}

// Implementacion real
type RedisStorage struct { /* ... */ }
func (r *RedisStorage) Get(key string) (string, error) { /* ... */ }
func (r *RedisStorage) Set(key string, value string, ttl int) error { /* ... */ }
func (r *RedisStorage) Delete(key string) error { /* ... */ }

// Fake
type FakeStorage struct {
    data map[string]string
}
func (f *FakeStorage) Get(key string) (string, error) {
    v, ok := f.data[key]
    if !ok { return "", errors.New("not found") }
    return v, nil
}
func (f *FakeStorage) Set(key string, value string, ttl int) error {
    f.data[key] = value
    return nil
}
func (f *FakeStorage) Delete(key string) error {
    delete(f.data, key)
    return nil
}

// Servicio
type CacheService struct {
    storage Storage  // interfaz, no tipo concreto
}

func (cs *CacheService) Get(key string) (string, error) {
    return cs.storage.Get(key)
}

// Test
func TestCacheMiss(t *testing.T) {
    fake := &FakeStorage{data: make(map[string]string)}
    service := &CacheService{storage: fake}
    _, err := service.Get("missing")
    if err == nil {
        t.Error("expected error for missing key")
    }
}
```

**Diferencia clave con C**:
- Go interfaces son implicitas: no necesitas declarar "FakeStorage implements Storage"
- Go verifica en compile time (al asignar a la variable de interfaz)
- No hay void* ni casts manuales
- Las interfaces de Go son similares a structs de function pointers pero type-safe

### 10.3 Tabla comparativa

| Aspecto | C (function pointers) | Rust (traits) | Go (interfaces) |
|---------|----------------------|---------------|-----------------|
| Declaracion | `typedef struct { int (*f)(...); }` | `trait Name { fn f(...); }` | `type Name interface { F(...) }` |
| Implementacion | Asignar function pointers al struct | `impl Trait for Type` | Tener los metodos (implicito) |
| Type safety | Pobre (void*, sin verificacion) | Excelente (compile time) | Buena (compile time) |
| Dispatch | Siempre dinamico (via puntero) | Estatico (generics) o dinamico (dyn) | Siempre dinamico (interface) |
| Verificacion completa | No (campo NULL = crash) | Si (compile error) | Si (compile error) |
| Context/self | void* manual | &self automatico | receiver automatico |
| Overhead | ~1-3 ns/call | 0 (static) o ~1 ns (dyn) | ~1-3 ns/call |

---

## 11. Errores comunes

### 11.1 Olvidar inicializar un campo del struct

```c
// BUG: campo del no inicializado → NULL → SEGFAULT
const StorageOps incomplete_ops = {
    .get = fake_get,
    .set = fake_set,
    // .del NO ASIGNADO → es NULL
    // .exists NO ASIGNADO → es NULL
};

// Si cache_delete() llama a ops->del(), SEGFAULT!
```

Solucion: siempre usar designated initializers y verificar en runtime:

```c
// Verificacion defensiva en el constructor
CacheService cache_service_new(const StorageOps *storage, ...) {
    assert(storage != NULL);
    assert(storage->get != NULL);
    assert(storage->set != NULL);
    assert(storage->del != NULL);
    assert(storage->exists != NULL);
    // ...
}
```

### 11.2 Mezclar ctx de diferentes doubles

```c
// BUG: pasar el ctx de un double a otro
CacheService cs = cache_service_new(
    &fake_storage_ops, &metrics_spy,    // ERROR: ctx equivocado!
    &stub_clock_ops, &clock_state,
    &spy_metrics_ops, &storage,         // ERROR: ctx cruzado!
    &dummy_logger, NULL,
    3600
);
// fake_get() hara cast de MetricsSpy* a FakeStorage* → corrupcion

// SOLUCION: ser meticuloso con el orden, o empaquetar ctx en el struct de ops
```

### 11.3 Function pointer invalidado (dangling)

```c
// BUG: function pointer apunta a funcion de un .so descargado
void *handle = dlopen("plugin.so", RTLD_NOW);
PluginOps *ops = dlsym(handle, "plugin_ops");
storage_set_impl(ops, NULL);

dlclose(handle);  // plugin descargado

cache_get(&cs, "key", buf, sizeof(buf));
// ops->get apunta a memoria liberada → SEGFAULT o corrupcion
```

### 11.4 No resetear state entre tests

```c
// BUG: fake_storage tiene datos del test anterior
static void test_1(void) {
    CacheService cs = create_test_service();  // fake_storage_init() dentro
    cache_set(&cs, "key", "from_test_1");
    // ... asserts
}

static void test_2(void) {
    // Si create_test_service() no llama fake_storage_init():
    CacheService cs = create_test_service();  // BUG: storage tiene "key" de test_1
    char value[256];
    int rc = cache_get(&cs, "key", value, sizeof(value));
    assert(rc == -1);  // FALLA! key existe del test anterior
}
// SOLUCION: siempre inicializar en setup
```

### 11.5 Interfaz demasiado grande

```c
// ANTI-PATRON: struct con 20 function pointers
typedef struct {
    int (*create_user)(...);
    int (*find_user_by_email)(...);
    int (*find_user_by_id)(...);
    int (*update_user)(...);
    int (*delete_user)(...);
    int (*list_users)(...);
    int (*count_users)(...);
    int (*create_post)(...);
    int (*find_post)(...);
    int (*update_post)(...);
    int (*delete_post)(...);
    int (*list_posts_by_user)(...);
    int (*create_comment)(...);
    // ... 15 mas
} DatabaseOps;

// Problema: cada test double necesita implementar las 20+ funciones,
// aunque solo use 2 de ellas. Esto es el Interface Segregation Principle
// (la I de SOLID).

// SOLUCION: separar en interfaces pequenas
typedef struct {
    int (*find)(const char *email, UserRecord *out);
    int (*insert)(const UserRecord *user);
    int (*delete)(int user_id);
} UserRepoOps;

typedef struct {
    int (*find)(int post_id, PostRecord *out);
    int (*insert)(const PostRecord *post);
    int (*list_by_user)(int user_id, PostRecord *out, int max);
} PostRepoOps;

// Cada modulo solo depende de la interfaz que necesita
int user_service_create(const char *name, const UserRepoOps *repo);
int post_service_create(const char *title, const PostRepoOps *repo);
```

---

## 12. Patrones avanzados

### 12.1 Composicion de interfaces

```c
// Componer multiples interfaces en una "super-interfaz"
typedef struct {
    const UserRepoOps *users;
    void *users_ctx;
    const PostRepoOps *posts;
    void *posts_ctx;
    const EmailOps *email;
    void *email_ctx;
} ServiceDeps;

// El servicio recibe todas sus dependencias en un struct
int create_user_and_welcome(const char *name, const char *email,
                            const ServiceDeps *deps) {
    UserRecord u = {0};
    snprintf(u.name, sizeof(u.name), "%s", name);
    snprintf(u.email, sizeof(u.email), "%s", email);

    int id = deps->users->insert(deps->users_ctx, &u);
    if (id < 0) return -1;

    deps->email->send(deps->email_ctx, email, "Welcome!", "Hello!");
    return id;
}

// En tests: mezclar diferentes tipos de doubles
ServiceDeps test_deps = {
    .users = &fake_user_ops, .users_ctx = &fake_user_db,
    .posts = &stub_post_ops, .posts_ctx = NULL,       // dummy/stub
    .email = &spy_email_ops, .email_ctx = &email_spy,  // spy
};
```

### 12.2 Factory functions para tests

```c
// Helper que crea un ServiceDeps pre-configurado para tests
ServiceDeps create_test_deps(void) {
    static FakeUserDB user_db;
    static EmailSpy email_spy;

    fake_user_db_init(&user_db);
    email_spy_init(&email_spy);

    return (ServiceDeps){
        .users = &fake_user_ops, .users_ctx = &user_db,
        .posts = &stub_post_ops, .posts_ctx = NULL,
        .email = &spy_email_ops, .email_ctx = &email_spy,
    };
}

// Variante: solo override lo que necesitas
ServiceDeps create_test_deps_with_failing_email(void) {
    ServiceDeps deps = create_test_deps();
    // Solo cambiar el email
    static EmailSpy failing_spy;
    email_spy_init(&failing_spy);
    failing_spy.return_value = -1;
    deps.email = &spy_email_ops;
    deps.email_ctx = &failing_spy;
    return deps;
}
```

### 12.3 Dispatch table dinamica

```c
// Para tests parametrizados: tabla de implementaciones
typedef struct {
    const char *name;
    const StorageOps *ops;
    void *(*create_ctx)(void);
    void (*destroy_ctx)(void *);
} StorageTestCase;

// Testear contra multiples implementaciones
StorageTestCase storage_implementations[] = {
    {"fake_memory", &fake_storage_ops, fake_storage_create, fake_storage_destroy},
    {"fake_file",   &file_storage_ops, file_storage_create, file_storage_destroy},
};

void test_set_get_roundtrip(void) {
    for (int i = 0; i < 2; i++) {
        StorageTestCase *tc = &storage_implementations[i];
        void *ctx = tc->create_ctx();

        int rc = tc->ops->set(ctx, "key", "value", 3600);
        assert(rc == 0);

        char value[256];
        rc = tc->ops->get(ctx, "key", value, sizeof(value));
        assert(rc == 0);
        assert(strcmp(value, "value") == 0);

        tc->destroy_ctx(ctx);
        printf("  PASS: set_get_roundtrip [%s]\n", tc->name);
    }
}
```

### 12.4 Spy que tambien delega al real (partial mock)

```c
// Spy que intercepta llamadas pero tambien ejecuta la real
typedef struct {
    const StorageOps *real_ops;
    void *real_ctx;
    int get_call_count;
    int set_call_count;
    char last_get_key[128];
    char last_set_key[128];
} StorageSpyWrapper;

static int spy_wrapper_get(void *ctx, const char *key, char *value, int size) {
    StorageSpyWrapper *spy = (StorageSpyWrapper *)ctx;
    spy->get_call_count++;
    snprintf(spy->last_get_key, sizeof(spy->last_get_key), "%s", key);

    // DELEGAR a la implementacion real
    return spy->real_ops->get(spy->real_ctx, key, value, size);
}

static int spy_wrapper_set(void *ctx, const char *key, const char *value,
                           int ttl) {
    StorageSpyWrapper *spy = (StorageSpyWrapper *)ctx;
    spy->set_call_count++;
    snprintf(spy->last_set_key, sizeof(spy->last_set_key), "%s", key);

    // DELEGAR a la implementacion real
    return spy->real_ops->set(spy->real_ctx, key, value, ttl);
}

static const StorageOps spy_wrapper_ops = {
    .get = spy_wrapper_get,
    .set = spy_wrapper_set,
    // ...
};

// Uso: observar las llamadas sin cambiar el comportamiento
StorageSpyWrapper spy = {
    .real_ops = &fake_storage_ops,
    .real_ctx = &fake_storage,
};

CacheService cs = cache_service_new(&spy_wrapper_ops, &spy, ...);
cache_get(&cs, "key", buf, sizeof(buf));

assert(spy.get_call_count == 1);
assert(strcmp(spy.last_get_key, "key") == 0);
// Y la operacion real tambien se ejecuto (el fake fue consultado)
```

---

## 13. Verificacion de interfaz completa — macro helper

Un problema comun es olvidar campos del struct. Podemos crear una macro que lo detecte:

```c
// Helper para verificar que todos los campos estan asignados
#define VERIFY_OPS(ops, type) do {                                          \
    type _zero = {0};                                                       \
    const char *_ptr = (const char *)&(ops);                                \
    const char *_zptr = (const char *)&_zero;                               \
    for (size_t _i = 0; _i < sizeof(type); _i++) {                         \
        if (_ptr[_i] != _zptr[_i]) goto _not_zero_##__LINE__;              \
    }                                                                       \
    fprintf(stderr, "WARNING: %s has all-zero fields\n", #ops);            \
    _not_zero_##__LINE__: ;                                                 \
} while(0)

// Mejor: verificar campo por campo
#define VERIFY_STORAGE_OPS(ops) do {                                        \
    assert((ops).get != NULL && "StorageOps.get is NULL");                  \
    assert((ops).set != NULL && "StorageOps.set is NULL");                  \
    assert((ops).del != NULL && "StorageOps.del is NULL");                  \
    assert((ops).exists != NULL && "StorageOps.exists is NULL");            \
} while(0)

// Uso en el constructor:
CacheService cache_service_new(const StorageOps *storage, void *ctx, ...) {
    VERIFY_STORAGE_OPS(*storage);
    // ...
}
```

---

## 14. Programa de practica — HTTP server con dependencias inyectadas

### 14.1 Requisitos

Implementa un mini HTTP request handler con todas las dependencias inyectadas via function pointers:

```c
// http_handler.h

// --- Interfaces ---

// Base de datos de rutas/contenido
typedef struct {
    int (*get_page)(void *ctx, const char *path, char *content, int size);
    int (*log_visit)(void *ctx, const char *path, const char *ip);
} ContentOps;

// Autenticacion
typedef struct {
    int (*validate_token)(void *ctx, const char *token, char *user_id, int size);
    int (*is_admin)(void *ctx, const char *user_id);
} AuthOps;

// Rate limiting
typedef struct {
    int (*check_rate)(void *ctx, const char *ip, int max_per_minute);
    void (*record_request)(void *ctx, const char *ip);
} RateLimitOps;

// Logger
typedef struct {
    void (*log)(void *ctx, const char *level, const char *msg);
} LogOps;

// --- Request/Response ---
typedef struct {
    char method[8];
    char path[256];
    char ip[46];
    char auth_token[128];
} HTTPRequest;

typedef struct {
    int status_code;
    char body[4096];
    char content_type[64];
} HTTPResponse;

// --- Handler ---
typedef struct {
    const ContentOps *content;  void *content_ctx;
    const AuthOps *auth;        void *auth_ctx;
    const RateLimitOps *rate;   void *rate_ctx;
    const LogOps *log;          void *log_ctx;
    int rate_limit_per_minute;
} HTTPHandler;

HTTPHandler http_handler_new(
    const ContentOps *content, void *content_ctx,
    const AuthOps *auth, void *auth_ctx,
    const RateLimitOps *rate, void *rate_ctx,
    const LogOps *log, void *log_ctx,
    int rate_limit
);

HTTPResponse http_handle_request(HTTPHandler *h, const HTTPRequest *req);
```

### 14.2 Lo que debes implementar

1. **http_handler.c** — logica del handler:
   - Verificar rate limit (429 Too Many Requests)
   - Si la ruta requiere auth (`/admin/*`), validar token (401/403)
   - Obtener contenido de la "base de datos" (404 si no existe)
   - Registrar la visita
   - Retornar 200 con el contenido

2. **Tests con doubles** (15+ tests):

| Dependencia | Tipo de double | Tests |
|-------------|---------------|-------|
| LogOps | DUMMY | Todos (no verificamos logs) |
| RateLimitOps | STUB | Rate limit exceeded, rate limit OK |
| ContentOps | FAKE | Pagina existe, no existe, visitas registradas |
| AuthOps | SPY | Token validado, admin verificado, argumentos correctos |
| ContentOps.log_visit | SPY | Visita registrada con IP y path correctos |

3. **CMakeLists.txt** con CTest, labels (unit, auth, ratelimit, content)

---

## 15. Ejercicios

### Ejercicio 1: Interfaz minima

Refactoriza el siguiente codigo para usar function pointer injection. El objetivo es poder testear `process_order` sin una base de datos real ni un servicio de email real:

```c
// ANTES (acoplamiento directo):
int process_order(int user_id, int product_id, int quantity) {
    Product p = db_get_product(product_id);         // DB real
    if (p.stock < quantity) return -1;

    db_update_stock(product_id, p.stock - quantity); // DB real
    int order_id = db_create_order(user_id, product_id, quantity); // DB real

    char msg[256];
    snprintf(msg, sizeof(msg), "Order #%d confirmed", order_id);
    send_email(user_id, "Order Confirmation", msg);  // Email real

    return order_id;
}
```

Define las interfaces (structs de function pointers), modifica `process_order`, y escribe tests con stubs/fakes.

### Ejercicio 2: Spy wrapper

Implementa un spy wrapper (patron de la seccion 12.4) para la interfaz `StorageOps`. El spy debe:
- Contar llamadas a cada operacion (get, set, del)
- Registrar los keys usados en las ultimas 10 llamadas
- Medir el tiempo de cada llamada (usando clock())
- Delegar todas las llamadas a una implementacion real (fake)

Escribe tests que usen el spy wrapper para verificar que `cache_get_or_compute`:
- Llama a get exactamente 1 vez si hay cache hit
- Llama a get 1 vez + set 1 vez si hay cache miss
- Los keys usados son correctos

### Ejercicio 3: Plugin system

Diseña un sistema de plugins usando function pointers:

```c
typedef struct {
    const char *name;
    const char *version;
    int (*init)(void *config);
    int (*process)(const char *input, char *output, int output_size);
    void (*shutdown)(void);
} Plugin;
```

Implementa 3 "plugins" (uppercase, reverse, rot13) y un plugin_manager que los carga. Escribe tests usando fakes de plugins para verificar que plugin_manager:
- Inicializa los plugins en orden
- Despacha al plugin correcto
- Maneja errores de init/process

### Ejercicio 4: Comparar approaches

Implementa el mismo test (cache miss → compute → set) usando tres tecnicas:
1. Function pointer injection (este topico)
2. Link-time substitution (compilar con stub.o)
3. Preprocessor macro (#define cache_backend_get stub_get)

Documenta: lineas de codigo, complejidad del Makefile, y que tan facil es cambiar un solo double sin afectar otros.

---

Navegacion:

- Anterior: [T01 — Que es un test double](../T01_Que_es_un_test_double/README.md)
- Siguiente: [T03 — Link-time substitution](../T03_Link_time_substitution/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Proximo topico: T03 — Link-time substitution*
