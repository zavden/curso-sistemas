# T01 — Singleton en C

---

## 1. Que es un Singleton

Un singleton garantiza que existe **exactamente una instancia** de un
tipo en todo el programa, y provee un punto de acceso global a ella.

```
  Programa
  ┌─────────────────────────────────────────────┐
  │                                             │
  │   modulo_a.c ──┐                            │
  │                 │    ┌──────────────────┐    │
  │   modulo_b.c ──┼───>│  Logger (unico)  │    │
  │                 │    └──────────────────┘    │
  │   modulo_c.c ──┘         ▲                  │
  │                          │                  │
  │   Todos acceden a la MISMA instancia        │
  └─────────────────────────────────────────────┘
```

Casos tipicos:
- Logger global
- Pool de conexiones a base de datos
- Configuracion de la aplicacion
- Cache compartido
- Gestor de hardware (GPU, audio device)

---

## 2. Singleton basico: variable static + funcion de acceso

### 2.1 Variable static en el .c

```c
/* logger.h */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} LogLevel;

typedef struct {
    FILE     *output;
    LogLevel  min_level;
    int       initialized;
} Logger;

/* Punto de acceso global */
Logger *logger_get_instance(void);

/* Operaciones */
void logger_set_level(Logger *log, LogLevel level);
void logger_set_output(Logger *log, FILE *fp);
void logger_write(Logger *log, LogLevel level, const char *fmt, ...);

#endif
```

```c
/* logger.c */
#include "logger.h"
#include <stdarg.h>
#include <time.h>

/* La instancia unica — static en el .c, invisible desde fuera */
static Logger instance = {
    .output      = NULL,  /* Se inicializa a stderr en get_instance */
    .min_level   = LOG_INFO,
    .initialized = 0,
};

Logger *logger_get_instance(void) {
    if (!instance.initialized) {
        instance.output      = stderr;
        instance.initialized = 1;
    }
    return &instance;
}

static const char *level_str(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
    }
    return "?";
}

void logger_set_level(Logger *log, LogLevel level) {
    log->min_level = level;
}

void logger_set_output(Logger *log, FILE *fp) {
    log->output = fp;
}

void logger_write(Logger *log, LogLevel level, const char *fmt, ...) {
    if (level < log->min_level) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    fprintf(log->output, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            level_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(log->output, fmt, args);
    va_end(args);

    fprintf(log->output, "\n");
    fflush(log->output);
}
```

```c
/* main.c */
#include "logger.h"

void module_a(void) {
    Logger *log = logger_get_instance();
    logger_write(log, LOG_INFO, "module_a initialized");
}

void module_b(void) {
    Logger *log = logger_get_instance();
    logger_write(log, LOG_DEBUG, "module_b doing work: %d", 42);
}

int main(void) {
    Logger *log = logger_get_instance();
    logger_set_level(log, LOG_DEBUG);

    module_a();
    module_b();

    logger_write(log, LOG_WARN, "shutting down");
    return 0;
}
```

### 2.2 Anatomia de la garantia

```
  logger.c
  ┌──────────────────────────────────────────┐
  │                                          │
  │  static Logger instance = { ... };       │ ← Una sola instancia
  │                                          │    en segment BSS/DATA
  │  Logger *logger_get_instance(void) {     │
  │      if (!instance.initialized) {        │ ← Lazy init
  │          instance.output = stderr;       │
  │          instance.initialized = 1;       │
  │      }                                   │
  │      return &instance;                   │ ← Siempre el mismo ptr
  │  }                                       │
  │                                          │
  └──────────────────────────────────────────┘

  main.c          module_a.c        module_b.c
  ┌────────┐      ┌────────┐        ┌────────┐
  │ log ───┼──┐   │ log ───┼──┐     │ log ───┼──┐
  └────────┘  │   └────────┘  │     └────────┘  │
              │               │                 │
              └───────┬───────┘                 │
                      └─────────────────────────┘
                                │
                      ┌─────────▼──────────┐
                      │  static instance   │  ← Misma direccion
                      └────────────────────┘
```

`static` en file scope hace que `instance` sea:
1. Unica — una sola copia en el programa
2. Privada — solo visible en logger.c (linkage interno)
3. Persistente — vive toda la ejecucion del programa

---

## 3. El problema del multithreading

### 3.1 Race condition en lazy init

```c
/* PELIGROSO con multiples threads */
Logger *logger_get_instance(void) {
    if (!instance.initialized) {      /* Thread A lee 0 */
        /* Context switch!  Thread B tambien lee 0 */
        instance.output = stderr;
        instance.initialized = 1;     /* Thread A escribe 1 */
        /* Thread B ya paso el check, inicializa de nuevo */
    }
    return &instance;
}
```

```
  Tiempo    Thread A                    Thread B
  ──────    ────────                    ────────
  t1        lee initialized == 0
  t2                                    lee initialized == 0
  t3        output = stderr
  t4        initialized = 1
  t5                                    output = stderr  ← re-init!
  t6                                    initialized = 1
```

En este ejemplo simple la re-inicializacion no causa dano visible,
pero si la inicializacion abriera un archivo o socket, tendriamos
un leak o corrupcion.

### 3.2 Solucion: pthread_once

`pthread_once` garantiza que una funcion se ejecuta **exactamente una
vez**, sin importar cuantos threads la invoquen simultaneamente:

```c
/* logger.c — version thread-safe */
#include "logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <time.h>

static Logger instance;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void logger_init(void) {
    instance.output      = stderr;
    instance.min_level   = LOG_INFO;
    instance.initialized = 1;
}

Logger *logger_get_instance(void) {
    pthread_once(&init_once, logger_init);
    return &instance;
}
```

```
  Tiempo    Thread A                    Thread B
  ──────    ────────                    ────────
  t1        pthread_once(&once, init)
  t2        → once == INIT, ejecuta     pthread_once(&once, init)
  t3          logger_init()             → once != INIT, espera...
  t4          once = DONE               ← desbloquea
  t5        return &instance            return &instance
```

### 3.3 Como funciona pthread_once internamente

```
  pthread_once_t once = PTHREAD_ONCE_INIT;

  pthread_once(&once, func):
  ┌─────────────────────────────────────────┐
  │  if (atomic_load(&once) == DONE)        │ ← Fast path (comun)
  │      return;                            │
  │                                         │
  │  lock(once.mutex);                      │ ← Slow path (primera vez)
  │  if (once != DONE) {                    │
  │      func();                            │
  │      atomic_store(&once, DONE);         │
  │  }                                      │
  │  unlock(once.mutex);                    │
  └─────────────────────────────────────────┘

  Despues de la primera llamada, el fast path (una lectura
  atomica) es todo lo que se ejecuta — sin lock.
```

---

## 4. Singleton con mutex para operaciones

La inicializacion thread-safe no es suficiente si multiples threads
escriben al logger simultaneamente (fprintf no es atomico entre
llamadas):

```c
/* logger.c — totalmente thread-safe */
#include "logger.h"
#include <pthread.h>
#include <stdarg.h>
#include <time.h>

typedef struct {
    FILE          *output;
    LogLevel       min_level;
    pthread_mutex_t mutex;
} LoggerInternal;

static LoggerInternal g_logger;
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

static void logger_init(void) {
    g_logger.output    = stderr;
    g_logger.min_level = LOG_INFO;
    pthread_mutex_init(&g_logger.mutex, NULL);
}

Logger *logger_get_instance(void) {
    pthread_once(&g_init_once, logger_init);
    return (Logger *)&g_logger;  /* Logger es la parte publica */
}

void logger_write(Logger *log, LogLevel level, const char *fmt, ...) {
    LoggerInternal *internal = (LoggerInternal *)log;

    if (level < internal->min_level) return;

    pthread_mutex_lock(&internal->mutex);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    fprintf(internal->output, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            level_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(internal->output, fmt, args);
    va_end(args);

    fprintf(internal->output, "\n");
    fflush(internal->output);

    pthread_mutex_unlock(&internal->mutex);
}
```

```
  Dos niveles de thread safety:
  ────────────────────────────

  1. Inicializacion (pthread_once):
     Garantiza que logger_init() se ejecuta UNA vez

  2. Operaciones (pthread_mutex):
     Garantiza que logger_write() no intercala output

  Sin (1): doble inicializacion, posible leak
  Sin (2): lineas mezcladas en output
  Ambos son necesarios en codigo multithreaded
```

---

## 5. Alternativa: singleton eager (sin lazy init)

Si la inicializacion no depende de parametros de runtime:

```c
/* logger.c — eager init */
static Logger instance = {
    .output    = NULL,    /* No podemos poner stderr aqui (no es constante) */
    .min_level = LOG_INFO,
};

/* Pero stderr no es compile-time constant en C... */
```

Problema: en C, los inicializadores static deben ser constantes
conocidas en compilacion. No puedes escribir `.output = stderr`
porque `stderr` es un macro que expande a una expresion no-constante
en la mayoria de implementaciones.

### 5.1 Solucion: constructor attribute (GCC/Clang)

```c
static Logger instance;

__attribute__((constructor))
static void logger_auto_init(void) {
    instance.output    = stderr;
    instance.min_level = LOG_INFO;
}

Logger *logger_get_instance(void) {
    /* Ya inicializado antes de main() */
    return &instance;
}
```

```
  Ciclo de vida con __attribute__((constructor)):
  ────────────────────────────────────────────────

  1. Cargador (ld.so) carga el programa
  2. Se ejecutan todas las funciones __constructor__
     → logger_auto_init() se ejecuta aqui
  3. Se llama a main()
  4. Cualquier llamada a logger_get_instance() retorna
     la instancia ya inicializada

  Problema: el orden entre constructors de diferentes TU
  (translation units) no esta definido.
```

**Advertencia**: `__attribute__((constructor))` es extension
GCC/Clang, no es standard C. `pthread_once` es POSIX y mas portable.

---

## 6. Patron opaque singleton

Para singletons mas complejos, combinar con opaque type (B04):

```c
/* db_pool.h — interfaz publica */
#ifndef DB_POOL_H
#define DB_POOL_H

#include <stddef.h>

/* Tipo opaco — caller no ve internos */
typedef struct DbPool DbPool;

/* Config para inicializar (una sola vez) */
typedef struct {
    const char *host;
    int         port;
    const char *database;
    const char *user;
    const char *password;
    size_t      pool_size;
} DbPoolConfig;

/* Inicializa el singleton (llamar una vez, antes de usar) */
int  db_pool_init(const DbPoolConfig *cfg);

/* Obtiene la instancia (despues de init) */
DbPool *db_pool_get_instance(void);

/* Operaciones */
struct DbConn *db_pool_acquire(DbPool *pool);
void           db_pool_release(DbPool *pool, struct DbConn *conn);

/* Cleanup al final del programa */
void db_pool_shutdown(void);

#endif
```

```c
/* db_pool.c */
#include "db_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct DbConn {
    int  fd;
    int  in_use;
} DbConn;

struct DbPool {
    DbConn        *connections;
    size_t         pool_size;
    size_t         available;
    pthread_mutex_t mutex;
    char            host[256];
    int             port;
};

static struct DbPool *g_pool = NULL;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static DbPoolConfig   g_config;  /* Copia del config para init */

static void pool_do_init(void) {
    g_pool = calloc(1, sizeof(*g_pool));
    if (!g_pool) return;

    strncpy(g_pool->host, g_config.host, sizeof(g_pool->host) - 1);
    g_pool->port      = g_config.port;
    g_pool->pool_size = g_config.pool_size;
    g_pool->available = g_config.pool_size;

    g_pool->connections = calloc(g_config.pool_size, sizeof(DbConn));
    pthread_mutex_init(&g_pool->mutex, NULL);

    /* Crear conexiones reales... */
    for (size_t i = 0; i < g_config.pool_size; i++) {
        g_pool->connections[i].fd     = -1;  /* placeholder */
        g_pool->connections[i].in_use = 0;
    }
}

int db_pool_init(const DbPoolConfig *cfg) {
    /* Copiar config antes de pthread_once (que no acepta argumentos) */
    g_config = *cfg;
    pthread_once(&g_once, pool_do_init);
    return g_pool ? 0 : -1;
}

DbPool *db_pool_get_instance(void) {
    return g_pool;  /* NULL si init no fue llamado */
}

DbConn *db_pool_acquire(DbPool *pool) {
    pthread_mutex_lock(&pool->mutex);

    for (size_t i = 0; i < pool->pool_size; i++) {
        if (!pool->connections[i].in_use) {
            pool->connections[i].in_use = 1;
            pool->available--;
            pthread_mutex_unlock(&pool->mutex);
            return &pool->connections[i];
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return NULL;  /* Pool agotado */
}

void db_pool_release(DbPool *pool, DbConn *conn) {
    pthread_mutex_lock(&pool->mutex);
    conn->in_use = 0;
    pool->available++;
    pthread_mutex_unlock(&pool->mutex);
}

void db_pool_shutdown(void) {
    if (!g_pool) return;
    /* Cerrar conexiones, liberar memoria... */
    for (size_t i = 0; i < g_pool->pool_size; i++) {
        if (g_pool->connections[i].fd >= 0) {
            /* close(g_pool->connections[i].fd); */
        }
    }
    free(g_pool->connections);
    pthread_mutex_destroy(&g_pool->mutex);
    free(g_pool);
    g_pool = NULL;
}
```

```c
/* main.c */
#include "db_pool.h"
#include <stdio.h>

int main(void) {
    DbPoolConfig cfg = {
        .host      = "localhost",
        .port      = 5432,
        .database  = "myapp",
        .user      = "admin",
        .password  = "secret",
        .pool_size = 10,
    };

    if (db_pool_init(&cfg) != 0) {
        fprintf(stderr, "Failed to init db pool\n");
        return 1;
    }

    DbPool *pool = db_pool_get_instance();

    struct DbConn *conn = db_pool_acquire(pool);
    if (conn) {
        /* Usar conexion... */
        db_pool_release(pool, conn);
    }

    db_pool_shutdown();
    return 0;
}
```

### 6.1 Init separado vs lazy init

```
  Patron                   Ventaja                   Desventaja
  ──────                   ───────                   ──────────
  pthread_once dentro      Completamente lazy,       No puede recibir
  de get_instance()        caller no necesita init    parametros de config

  init() explicito +       Acepta config struct,     Caller debe llamar
  get_instance() separado  control del momento init   init() antes de usar

  Para singletons con config (DB, server), init() separado
  es casi siempre la mejor opcion.
```

---

## 7. Limitacion de pthread_once: no acepta argumentos

`pthread_once` llama una funcion `void (*)(void)` — sin parametros.
Para pasar configuracion, hay varias estrategias:

### 7.1 Variable global temporal (la que usamos arriba)

```c
static DbPoolConfig g_config;  /* Variable "staging" */

int db_pool_init(const DbPoolConfig *cfg) {
    g_config = *cfg;                   /* Copiar a staging */
    pthread_once(&g_once, pool_do_init); /* pool_do_init lee g_config */
    return g_pool ? 0 : -1;
}
```

Es thread-safe **si** `db_pool_init()` solo se llama una vez (que es
lo esperado para un singleton). Si dos threads llaman `db_pool_init`
con configs diferentes simultaneamente, uno podria pisar g_config antes
de que pthread_once lea. En la practica, init se llama en main() antes
de crear threads.

### 7.2 Patron init + get separados con mutex

```c
static DbPool    *g_pool = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

int db_pool_init(const DbPoolConfig *cfg) {
    pthread_mutex_lock(&g_mutex);

    if (g_pool != NULL) {
        /* Ya inicializado — ignorar o retornar error */
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    g_pool = calloc(1, sizeof(*g_pool));
    /* ... inicializar con cfg ... */

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

DbPool *db_pool_get_instance(void) {
    /* No necesita lock: despues de init, g_pool no cambia */
    return g_pool;
}
```

Mas simple que `pthread_once` cuando la inicializacion requiere
parametros. El lock solo se toma durante init.

---

## 8. Destruccion del singleton

### 8.1 El problema del orden de destruccion

```
  Programa termina:
  ┌────────────────────────────────────────────┐
  │  1. main() retorna                         │
  │  2. Se ejecutan funciones atexit()         │
  │  3. Se destruyen objetos static            │
  │  4. Se liberan recursos del proceso        │
  └────────────────────────────────────────────┘

  Problema: si singleton A usa singleton B,
  y B se destruye antes que A...
```

### 8.2 Solucion: atexit() para cleanup

```c
static void pool_do_init(void) {
    g_pool = calloc(1, sizeof(*g_pool));
    /* ... inicializar ... */

    /* Registrar cleanup al salir */
    atexit(db_pool_shutdown);
}
```

`atexit` ejecuta las funciones en orden LIFO (ultimo registrado,
primero ejecutado). Si A se inicializa antes que B:

```
  Init:     A → B       (A se registra primero en atexit)
  Cleanup:  B → A       (LIFO: B se limpia primero)

  Esto es correcto si A depende de B:
  B se crea despues de A, se destruye antes que A.
```

### 8.3 Estrategia: no destruir

Muchos singletons en C simplemente **no se destruyen**:

```c
/* El OS libera toda la memoria del proceso al terminar.
   Si no hay side effects (archivos que cerrar, sockets
   que desconectar), no destruir es valido. */
```

Razones para esta estrategia:
- Simplicidad — evita bugs de orden de destruccion
- El OS limpia memoria, file descriptors, sockets al terminar
- Herramientas como Valgrind reportan "still reachable" (no es leak)

Razones para destruir explicitamente:
- Flush de buffers (datos no escritos a disco)
- Desconexion limpia de base de datos (liberar locks)
- Recursos compartidos entre procesos (shared memory, semaforos)

---

## 9. Variantes comunes

### 9.1 Singleton con enum como key (multiples instancias nombradas)

No es singleton puro, pero es un patron comun:

```c
/* cache.h */
typedef enum {
    CACHE_USER,
    CACHE_SESSION,
    CACHE_PRODUCT,
    CACHE_COUNT,  /* Sentinel: cantidad de caches */
} CacheId;

typedef struct Cache Cache;

Cache *cache_get(CacheId id);
void   cache_put(Cache *c, const char *key, const void *val, size_t size);
void  *cache_get_val(Cache *c, const char *key, size_t *size);
```

```c
/* cache.c */
#include "cache.h"
#include <pthread.h>
#include <stdlib.h>

struct Cache {
    /* ... hash map interno ... */
    pthread_mutex_t mutex;
};

static Cache g_caches[CACHE_COUNT];
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

static void caches_init(void) {
    for (int i = 0; i < CACHE_COUNT; i++) {
        pthread_mutex_init(&g_caches[i].mutex, NULL);
        /* ... init hash map ... */
    }
}

Cache *cache_get(CacheId id) {
    pthread_once(&g_init_once, caches_init);
    if (id < 0 || id >= CACHE_COUNT) return NULL;
    return &g_caches[id];
}
```

### 9.2 Singleton con referencia contada (raro en C)

Para singletons que se comparten entre subsistemas con ciclos
de vida independientes:

```c
static int g_ref_count = 0;
static AudioEngine *g_engine = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

AudioEngine *audio_engine_acquire(void) {
    pthread_mutex_lock(&g_mutex);
    if (g_ref_count == 0) {
        g_engine = audio_engine_create();
    }
    g_ref_count++;
    pthread_mutex_unlock(&g_mutex);
    return g_engine;
}

void audio_engine_release(void) {
    pthread_mutex_lock(&g_mutex);
    g_ref_count--;
    if (g_ref_count == 0) {
        audio_engine_destroy(g_engine);
        g_engine = NULL;
    }
    pthread_mutex_unlock(&g_mutex);
}
```

---

## 10. Por que singleton es controversial

### 10.1 Problemas del patron

```
  Problema                    Consecuencia
  ────────                    ────────────
  Estado global mutable       Cualquier funcion puede modificarlo
  Acoplamiento oculto         Dependencia no visible en la firma
  Dificil de testear          No puedes inyectar un mock
  Orden de inicializacion     Dependencias entre singletons
  Concurrencia                Necesita sincronizacion
  Lifetime ambiguo            ¿Quien lo destruye? ¿Cuando?
```

### 10.2 Acoplamiento oculto

```c
/* La firma NO revela que usa el singleton */
int process_order(const Order *order) {
    Logger *log = logger_get_instance();      /* Dependencia oculta */
    DbPool *db  = db_pool_get_instance();     /* Dependencia oculta */
    Cache  *c   = cache_get(CACHE_PRODUCT);   /* Dependencia oculta */

    /* Si alguno no esta inicializado → crash */
    logger_write(log, LOG_INFO, "Processing order %d", order->id);
    /* ... */
}

/* Comparar con dependencia explicita: */
int process_order(const Order *order, Logger *log, DbPool *db, Cache *cache) {
    /* Todas las dependencias visibles en la firma */
    /* El caller es responsable de proveerlas */
    /* En tests, puedo pasar mocks */
}
```

### 10.3 Dificil de testear

```c
/* Test con singleton: no puedo aislar */
void test_process_order(void) {
    /* Necesito inicializar el logger REAL */
    /* Necesito una DB REAL (o al menos db_pool_init) */
    /* El test depende de estado global */
    /* Si otro test modifico el singleton, puede fallar */

    Order o = { .id = 1 };
    int result = process_order(&o);
    assert(result == 0);
}

/* Test con dependency injection: puedo mockear */
void test_process_order(void) {
    Logger mock_log = { .output = fopen("/dev/null", "w"), .min_level = LOG_DEBUG };
    DbPool *mock_db = create_mock_pool();
    Cache  *mock_cache = create_mock_cache();

    Order o = { .id = 1 };
    int result = process_order(&o, &mock_log, mock_db, mock_cache);
    assert(result == 0);
}
```

### 10.4 Cuando singleton es aceptable

A pesar de los problemas, singleton es aceptable cuando:

```
  ✓ El recurso es genuinamente unico (hardware, archivo de log)
  ✓ Pasar la dependencia a traves de 15 niveles de call stack
    seria impractico (especialmente en C sin closures)
  ✓ El singleton es read-only despues de la inicializacion
  ✓ Es un detalle de implementacion, no parte de la API publica
  ✓ La alternativa (dependency injection manual en C) es peor
    que el problema
```

En C, la ausencia de constructors/destructors automaticos y
closures hace que la inyeccion de dependencias sea mas costosa
que en lenguajes con mas abstracciones. El singleton es una
herramienta pragmatica, no un ideal.

---

## 11. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Lazy init sin pthread_once | Race condition en multithreading | `pthread_once` o mutex |
| pthread_once + funcion con parametros | `pthread_once` no pasa args | Variable staging o mutex manual |
| Acceder sin verificar init | `get_instance` retorna NULL | Check en get o init obligatorio |
| Destruir con dependencias activas | Otro singleton aun lo usa | `atexit` con LIFO order |
| Singleton para todo | Estado global incontrolado | Solo para recursos genuinamente unicos |
| Olvidar mutex en operaciones | Lazy init es safe, pero write no lo es | Mutex para toda operacion mutable |
| `static` en header | Cada TU tiene su propia copia | `static` en el .c, no en el .h |
| Testear codigo con singletons | No puedes inyectar mock | Pasar como parametro o usar callback |

---

## 12. Ejercicios

### Ejercicio 1 — Singleton basico

Implementa un singleton `AppConfig` que almacene:
- `const char *app_name`
- `int debug_mode`
- `int max_retries`

Con `config_init()` y `config_get_instance()`.

**Prediccion**: ¿que pasa si llamas `config_get_instance()` antes
de `config_init()`?

<details>
<summary>Respuesta</summary>

```c
/* app_config.h */
typedef struct AppConfig AppConfig;

int         config_init(const char *name, int debug, int retries);
AppConfig  *config_get_instance(void);
const char *config_app_name(const AppConfig *c);
int         config_debug_mode(const AppConfig *c);
int         config_max_retries(const AppConfig *c);
```

```c
/* app_config.c */
#include "app_config.h"
#include <string.h>
#include <stdlib.h>

struct AppConfig {
    char app_name[128];
    int  debug_mode;
    int  max_retries;
};

static AppConfig *g_config = NULL;

int config_init(const char *name, int debug, int retries) {
    if (g_config) return -1;  /* Ya inicializado */

    g_config = calloc(1, sizeof(*g_config));
    if (!g_config) return -1;

    strncpy(g_config->app_name, name, sizeof(g_config->app_name) - 1);
    g_config->debug_mode  = debug;
    g_config->max_retries = retries;
    return 0;
}

AppConfig *config_get_instance(void) {
    return g_config;  /* NULL si no se llamo init */
}

const char *config_app_name(const AppConfig *c) { return c->app_name; }
int config_debug_mode(const AppConfig *c)       { return c->debug_mode; }
int config_max_retries(const AppConfig *c)      { return c->max_retries; }
```

Si llamas `config_get_instance()` antes de `config_init()`, retorna
NULL. El caller debe hacer `if (!config) { /* error */ }` o el
programa segfaults al desreferenciar.

Opcion mas segura: que `config_get_instance()` use `assert(g_config)`
o imprima error y llame `abort()`.

</details>

---

### Ejercicio 2 — Thread safety con pthread_once

Agrega thread safety al ejercicio 1 usando `pthread_once`.

**Prediccion**: ¿como pasas los parametros (name, debug, retries)
a la funcion de `pthread_once` si esta no acepta argumentos?

<details>
<summary>Respuesta</summary>

Usando la variable staging:

```c
#include <pthread.h>

struct InitParams {
    const char *name;
    int         debug;
    int         retries;
};

static AppConfig   *g_config = NULL;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static struct InitParams g_params;

static void do_init(void) {
    g_config = calloc(1, sizeof(*g_config));
    if (!g_config) return;

    strncpy(g_config->app_name, g_params.name,
            sizeof(g_config->app_name) - 1);
    g_config->debug_mode  = g_params.debug;
    g_config->max_retries = g_params.retries;
}

int config_init(const char *name, int debug, int retries) {
    g_params = (struct InitParams){ name, debug, retries };
    pthread_once(&g_once, do_init);
    return g_config ? 0 : -1;
}
```

Los parametros se copian a `g_params` antes de `pthread_once`.
Esto es safe si `config_init()` se llama desde un solo thread
(tipicamente `main`). Si multiples threads pudieran llamar
`config_init` simultaneamente, el que "gana" `pthread_once`
usara el `g_params` que estaba en ese momento — podria ser
el del otro thread. En la practica, init se llama una vez en main.

</details>

---

### Ejercicio 3 — Singleton con atexit

Agrega cleanup automatico al DbPool del ejemplo de la seccion 6.

**Prediccion**: ¿en que orden se ejecutan los atexit handlers si
el logger se inicializa antes que el pool?

<details>
<summary>Respuesta</summary>

```c
static void pool_do_init(void) {
    g_pool = calloc(1, sizeof(*g_pool));
    /* ... */
    atexit(db_pool_shutdown);
}

/* Si en main(): */
int main(void) {
    logger_get_instance();  /* atexit(logger_shutdown) → pos 1 */
    db_pool_init(&cfg);     /* atexit(pool_shutdown) → pos 2 */
    /* ... */
    return 0;
}

/*
  atexit es LIFO:
  1. db_pool_shutdown()   — registrado segundo, ejecuta primero
  2. logger_shutdown()    — registrado primero, ejecuta segundo

  Esto es CORRECTO si db_pool_shutdown() usa logger:
  - Pool se cierra primero (puede loggear su cierre)
  - Logger se cierra despues (disponible durante pool shutdown)
*/
```

Si el orden fuera al reves (logger se cierra primero), el pool
no podria loggear durante su shutdown. LIFO resuelve esto
naturalmente: lo que se creo primero es lo que se necesita
disponible mas tiempo.

</details>

---

### Ejercicio 4 — Singleton vs dependency injection

Refactoriza esta funcion para que no use el singleton directamente:

```c
int send_email(const char *to, const char *subject, const char *body) {
    Logger *log = logger_get_instance();
    SmtpPool *smtp = smtp_pool_get_instance();
    logger_write(log, LOG_INFO, "Sending to %s", to);
    SmtpConn *conn = smtp_pool_acquire(smtp);
    /* ... enviar ... */
    smtp_pool_release(smtp, conn);
    return 0;
}
```

**Prediccion**: ¿cuantos parametros necesitas agregar? ¿Es mejor
o peor que el singleton?

<details>
<summary>Respuesta</summary>

```c
/* Dependencias explicitas */
int send_email(const char *to, const char *subject, const char *body,
               Logger *log, SmtpPool *smtp) {
    logger_write(log, LOG_INFO, "Sending to %s", to);
    SmtpConn *conn = smtp_pool_acquire(smtp);
    /* ... enviar ... */
    smtp_pool_release(smtp, conn);
    return 0;
}
```

Dos parametros mas. **Ventajas**:
- Testeable: puedo pasar un mock logger y mock smtp
- Explicito: la firma dice exactamente que necesita
- Desacoplado: no depende de la existencia de singletons

**Desventajas**:
- Mas verboso en el call site
- Si `send_email` se llama desde 20 funciones, todas necesitan
  recibir `log` y `smtp` y pasarlos
- En C sin closures, propagar contexto por 5+ niveles es tedioso

**Compromiso comun**: usar un struct "contexto" que agrupe las
dependencias:

```c
typedef struct {
    Logger   *log;
    SmtpPool *smtp;
    DbPool   *db;
} AppContext;

int send_email(const char *to, const char *subject, const char *body,
               const AppContext *ctx) {
    logger_write(ctx->log, LOG_INFO, "Sending to %s", to);
    /* ... */
}
```

Un solo parametro extra que agrupa todo el "entorno".

</details>

---

### Ejercicio 5 — Orden de inicializacion

Tienes tres singletons con estas dependencias:

```
  Cache depende de Logger (para loggear evictions)
  DbPool depende de Logger (para loggear queries)
  Cache depende de DbPool (para warming desde DB)
```

**Prediccion**: ¿en que orden debes inicializar? ¿Que pasa si
el orden es incorrecto?

<details>
<summary>Respuesta</summary>

Orden correcto (respetando dependencias):

```
  1. Logger     — no depende de nadie
  2. DbPool     — depende de Logger (ya listo)
  3. Cache      — depende de Logger y DbPool (ambos listos)

  Grafo de dependencias:
  Logger ← DbPool ← Cache
       ↑─────────────┘
```

```c
int main(void) {
    /* Orden correcto */
    logger_init();            /* 1. Logger primero */
    db_pool_init(&db_cfg);    /* 2. DbPool usa Logger */
    cache_init(&cache_cfg);   /* 3. Cache usa Logger y DbPool */

    /* ... programa ... */

    /* Destruccion inversa (atexit LIFO lo hace automatico) */
    /* 1. cache_shutdown   — registrado tercero, ejecuta primero  */
    /* 2. db_pool_shutdown — registrado segundo, ejecuta segundo  */
    /* 3. logger_shutdown  — registrado primero, ejecuta tercero  */
    return 0;
}
```

Si el orden es incorrecto (ej: Cache antes que DbPool):
- `cache_init` intenta `db_pool_get_instance()` → retorna NULL
- Cache segfaults al intentar warming desde DB
- O peor: si usa lazy init, DbPool se inicializa con config
  por defecto/incompleta

Si usas `pthread_once` puro (sin init separado), el orden se
resuelve automaticamente al primer acceso — pero sin control
de la configuracion.

</details>

---

### Ejercicio 6 — Static en header vs .c

¿Que esta mal en este codigo?

```c
/* counter.h */
static int g_count = 0;

static int counter_increment(void) {
    return ++g_count;
}
```

```c
/* a.c */
#include "counter.h"
void func_a(void) { printf("a: %d\n", counter_increment()); }
```

```c
/* b.c */
#include "counter.h"
void func_b(void) { printf("b: %d\n", counter_increment()); }
```

**Prediccion**: si llamas `func_a()` y luego `func_b()`, ¿que
imprime cada una?

<details>
<summary>Respuesta</summary>

Imprime:
```
a: 1
b: 1    ← NO 2!
```

El error es que `static` en un header crea una **copia separada**
en cada translation unit (.c) que incluye el header:

```
  Despues del preprocesador:

  a.c:                          b.c:
  ┌──────────────────────┐      ┌──────────────────────┐
  │ static int g_count=0 │      │ static int g_count=0 │
  │ static int counter_  │      │ static int counter_  │
  │   increment(void)    │      │   increment(void)    │
  │ { return ++g_count;} │      │ { return ++g_count;} │
  │                      │      │                      │
  │ void func_a(void) {  │      │ void func_b(void) {  │
  │   counter_increment()│      │   counter_increment()│
  │ }                    │      │ }                    │
  └──────────────────────┘      └──────────────────────┘
       ↑ su propio g_count          ↑ su propio g_count
```

Cada .c tiene su propia variable `g_count` — no es singleton!

**Solucion**: la variable y la funcion deben estar en un .c,
con solo declaraciones `extern` en el .h:

```c
/* counter.h */
int counter_increment(void);  /* Declaracion */

/* counter.c */
static int g_count = 0;       /* UNA copia, privada a counter.c */

int counter_increment(void) { /* Definicion */
    return ++g_count;
}
```

</details>

---

### Ejercicio 7 — Singleton con mutex manual

Implementa `logger_get_instance()` usando mutex en vez de
`pthread_once`. Debe ser thread-safe.

**Prediccion**: ¿por que el pattern "double-checked locking"
con un simple `if` no es suficiente?

<details>
<summary>Respuesta</summary>

```c
static Logger    *g_logger = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

Logger *logger_get_instance(void) {
    /* Double-checked locking (correcto con atomic) */
    if (__atomic_load_n(&g_logger, __ATOMIC_ACQUIRE) == NULL) {
        pthread_mutex_lock(&g_mutex);
        if (g_logger == NULL) {
            Logger *tmp = calloc(1, sizeof(*tmp));
            tmp->output    = stderr;
            tmp->min_level = LOG_INFO;
            /* Barrera: asegurar que tmp esta completamente
               inicializado ANTES de ser visible */
            __atomic_store_n(&g_logger, tmp, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&g_mutex);
    }
    return g_logger;
}
```

**¿Por que `if (g_logger == NULL)` simple no basta?**

Sin atomics/barreras, el compilador o CPU puede reordenar:

```
  Thread A:                        Thread B:
  1. g_logger = malloc(...)        1. lee g_logger → no-NULL
  2. g_logger->output = stderr     2. usa g_logger->output
                                      → basura! (step 2 de A
                                        aun no se ejecuto)
```

La CPU puede hacer visible la asignacion del puntero (step 1)
antes de que los campos esten escritos (step 2). El
`__ATOMIC_RELEASE` en store asegura que todo lo escrito antes
es visible para quien haga `__ATOMIC_ACQUIRE` en el load.

`pthread_once` maneja todo esto internamente — por eso es
preferible al double-checked locking manual.

</details>

---

### Ejercicio 8 — Singleton read-only

Disena un singleton `BuildInfo` que se inicializa una vez y
nunca se modifica. Almacena: version, commit hash, build date.

**Prediccion**: ¿este singleton necesita mutex para las operaciones
de lectura? ¿Por que?

<details>
<summary>Respuesta</summary>

```c
/* build_info.h */
typedef struct BuildInfo BuildInfo;

void             build_info_init(void);
const BuildInfo *build_info_get(void);
const char      *build_info_version(const BuildInfo *b);
const char      *build_info_commit(const BuildInfo *b);
const char      *build_info_date(const BuildInfo *b);
```

```c
/* build_info.c */
#include "build_info.h"
#include <pthread.h>

struct BuildInfo {
    char version[32];
    char commit[41];  /* SHA-1 hex */
    char date[32];
};

static BuildInfo g_info;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static void do_init(void) {
    /* Estos macros los define el build system (-D flags) */
    #ifndef BUILD_VERSION
    #define BUILD_VERSION "unknown"
    #endif
    #ifndef BUILD_COMMIT
    #define BUILD_COMMIT "unknown"
    #endif
    #ifndef BUILD_DATE
    #define BUILD_DATE __DATE__
    #endif

    snprintf(g_info.version, sizeof(g_info.version), "%s", BUILD_VERSION);
    snprintf(g_info.commit, sizeof(g_info.commit), "%s", BUILD_COMMIT);
    snprintf(g_info.date, sizeof(g_info.date), "%s", BUILD_DATE);
}

void build_info_init(void) {
    pthread_once(&g_once, do_init);
}

const BuildInfo *build_info_get(void) {
    pthread_once(&g_once, do_init);
    return &g_info;
}

const char *build_info_version(const BuildInfo *b) { return b->version; }
const char *build_info_commit(const BuildInfo *b)  { return b->commit; }
const char *build_info_date(const BuildInfo *b)    { return b->date; }
```

**¿Necesita mutex para lecturas?** NO.

Despues de `pthread_once`, los datos nunca cambian. Multiples
threads pueden leer simultaneamente sin sincronizacion porque:
1. `pthread_once` incluye una barrera de memoria — todo lo escrito
   en `do_init` es visible para todos los threads despues
2. No hay escrituras posteriores — no hay data race posible
3. Lecturas concurrentes de datos inmutables son siempre safe

Este es el singleton "ideal": init once, read many, never mutate.
No necesita mutex en operaciones, y `const BuildInfo *` en la
API hace explicito que es read-only.

</details>

---

### Ejercicio 9 — Singleton configurable en tests

Disena un mecanismo para que en tests puedas reemplazar el
logger singleton con un mock:

**Prediccion**: ¿se puede hacer sin modificar la interfaz publica?

<details>
<summary>Respuesta</summary>

Si, usando un puntero a funcion o flag de test:

```c
/* logger.h — interfaz igual */
Logger *logger_get_instance(void);

/* logger_internal.h — solo para tests */
#ifdef TESTING
void logger_override_instance(Logger *mock);
void logger_restore_instance(void);
#endif
```

```c
/* logger.c */
static Logger  g_real_logger;
static Logger *g_instance = NULL;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

#ifdef TESTING
static Logger *g_override = NULL;

void logger_override_instance(Logger *mock) {
    g_override = mock;
}

void logger_restore_instance(void) {
    g_override = NULL;
}
#endif

static void do_init(void) {
    g_real_logger.output    = stderr;
    g_real_logger.min_level = LOG_INFO;
    g_instance = &g_real_logger;
}

Logger *logger_get_instance(void) {
    pthread_once(&g_once, do_init);

    #ifdef TESTING
    if (g_override) return g_override;
    #endif

    return g_instance;
}
```

```c
/* test_email.c */
#define TESTING
#include "logger_internal.h"

void test_send_email(void) {
    /* Mock logger que escribe a buffer */
    char buffer[4096] = {0};
    FILE *mem = fmemopen(buffer, sizeof(buffer), "w");
    Logger mock = { .output = mem, .min_level = LOG_DEBUG };

    logger_override_instance(&mock);

    send_email("user@test.com", "Test", "Body");

    logger_restore_instance();
    fclose(mem);

    /* Verificar que se loggeo correctamente */
    assert(strstr(buffer, "Sending to user@test.com") != NULL);
}
```

El `#ifdef TESTING` asegura que el override no existe en
produccion — zero overhead. Pero requiere compilar con `-DTESTING`
para los tests.

Alternativa sin ifdef: usar un function pointer:

```c
static Logger *(*g_factory)(void) = default_logger_factory;

void logger_set_factory(Logger *(*factory)(void)) {
    g_factory = factory;
}

Logger *logger_get_instance(void) {
    return g_factory();
}
```

</details>

---

### Ejercicio 10 — Analisis: por que no constructor con args

`pthread_once` llama `void (*)(void)`. POSIX podria haber definido
`void (*)(void *arg)` con un argumento extra.

**Prediccion**: ¿por que crees que POSIX eligio no tener argumento?
¿Que problemas causaria si lo tuviera?

<details>
<summary>Respuesta</summary>

Si la firma fuera `pthread_once(once_t *, void (*)(void*), void *arg)`:

**Problema 1 — ¿Que arg se usa?**
```c
/* Thread A */
pthread_once(&once, init, config_a);

/* Thread B (simultaneo) */
pthread_once(&once, init, config_b);

/* Solo uno ejecuta init(). ¿Con config_a o config_b?
   El "perdedor" espera, pero su arg se ignora silenciosamente.
   Esto es confuso y propenso a bugs. */
```

**Problema 2 — Semantica ambigua**
```c
/* ¿init debe liberar arg? ¿O el caller lo libera? */
/* Si solo un thread ejecuta init, ¿los otros threads
   necesitan liberar su arg que nunca se uso? */
```

**Problema 3 — Simplicidad del contrato**
La semantica de "ejecutar exactamente una vez" es simple y clara
con `void (*)(void)`. Agregar un argumento complica la semantica
porque multiples callers pueden pasar argumentos diferentes, y
solo uno "gana".

**Solucion de POSIX**: el caller usa variables de file scope
(que son naturales en C) para pasar datos al init function:

```c
static Config g_config;         /* Variable compartida */
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static void init(void) {
    /* Lee g_config — setada por quien llamo primero */
    use(g_config);
}

void setup(const Config *cfg) {
    g_config = *cfg;
    pthread_once(&g_once, init);
}
```

Es mas verboso, pero la semantica es clara: el dato esta en
una variable conocida, no en un argumento que podria ser
diferente segun quien llame.

</details>
