# T02 — Singleton en Rust

---

## 1. De C a Rust: el compilador no deja hacer lo facil

T01 mostro el singleton en C: `static` variable + `pthread_once`.
En Rust, intentar lo mismo no compila:

```rust
// INTENTO 1: static mutable — ERROR
static mut LOGGER: Logger = Logger::new();

fn get_instance() -> &'static Logger {
    unsafe { &LOGGER }  // Requiere unsafe SIEMPRE
}
```

Rust prohibe `static mut` accesible sin `unsafe` porque:
- Cualquier thread puede leer/escribir simultaneamente
- No hay barrera de memoria ni sincronizacion
- Es exactamente el problema que T01 resolvio con `pthread_once`

```
  C:                                Rust:
  ──                                ─────
  static Logger instance;           static mut LOGGER: Logger;
  → Compila, race condition         → Compila con unsafe, IGUAL race
    en runtime                        condition — unsafe no arregla nada

  pthread_once + mutex              → El compilador OBLIGA a usar
  → Programador debe recordar         tipos que garantizan safety
```

---

## 2. LazyLock: el reemplazo de pthread_once (std 1.80+)

`std::sync::LazyLock` es la forma idomiatica desde Rust 1.80:

```rust
use std::sync::LazyLock;

#[derive(Debug)]
struct Logger {
    min_level: LogLevel,
    // En un caso real: Mutex<File> para el output
}

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
enum LogLevel {
    Debug,
    Info,
    Warn,
    Error,
}

impl Logger {
    fn new() -> Self {
        eprintln!("[init] Logger created");
        Self { min_level: LogLevel::Info }
    }

    fn log(&self, level: LogLevel, msg: &str) {
        if level >= self.min_level {
            eprintln!("[{:?}] {}", level, msg);
        }
    }
}

// Singleton: se inicializa la primera vez que se accede
static LOGGER: LazyLock<Logger> = LazyLock::new(|| {
    Logger::new()
});

fn main() {
    // Primera llamada: ejecuta el closure, inicializa
    LOGGER.log(LogLevel::Info, "starting up");

    // Segunda llamada: retorna la instancia existente
    LOGGER.log(LogLevel::Warn, "something happened");
}
```

### 2.1 Que hace LazyLock internamente

```
  LazyLock<T> internamente:
  ┌───────────────────────────────────────────┐
  │  state: AtomicU8  (Uninit / Running / Done)│
  │  data:  UnsafeCell<MaybeUninit<T>>         │
  │  init:  Cell<Option<F>>  (el closure)      │
  └───────────────────────────────────────────┘

  Primera llamada (Thread A):
  1. atomic_load(state) == Uninit
  2. CAS(state, Uninit → Running)  → exito
  3. Ejecuta closure: init()
  4. Escribe resultado en data
  5. atomic_store(state, Done) con Release

  Llamadas siguientes (cualquier thread):
  1. atomic_load(state) == Done  → fast path
  2. Retorna &data

  Llamada concurrente (Thread B mientras A inicializa):
  1. atomic_load(state) == Running
  2. Espera (spin + yield) hasta Done
  3. Retorna &data

  Equivale a pthread_once + static variable,
  pero el compilador garantiza que no hay data race.
```

### 2.2 Comparacion con pthread_once de C

```
  pthread_once (C, T01):          LazyLock (Rust):
  ──────────────────────          ────────────────
  pthread_once_t once;            LazyLock::new(|| { ... })
  static Logger instance;
  void init(void) { ... }        ← closure captura todo

  logger_get_instance() {         // Acceso directo
    pthread_once(&once, init);    LOGGER.log(...)
    return &instance;             // Deref automatico
  }

  Manual: once + instance +       Un solo tipo, todo integrado
  funcion separada

  No acepta argumentos            Closure captura entorno
                                  (pero static no captura
                                  variables locales)
```

---

## 3. OnceLock: singleton con inicializacion separada

`LazyLock` define la inicializacion en el punto de declaracion.
`OnceLock` permite inicializar despues, con parametros:

```rust
use std::sync::OnceLock;

struct DbPool {
    host: String,
    pool_size: usize,
    // connections: Vec<Connection>, etc.
}

static DB_POOL: OnceLock<DbPool> = OnceLock::new();

fn db_pool_init(host: &str, pool_size: usize) {
    let pool = DbPool {
        host: host.to_string(),
        pool_size,
    };
    // set() retorna Err si ya fue inicializado
    DB_POOL.set(pool).expect("DbPool already initialized");
}

fn db_pool() -> &'static DbPool {
    DB_POOL.get().expect("DbPool not initialized — call db_pool_init first")
}

fn main() {
    // Inicializacion con parametros
    db_pool_init("localhost:5432", 10);

    // Uso posterior
    let pool = db_pool();
    println!("Connected to {} with {} connections", pool.host, pool.pool_size);
}
```

### 3.1 LazyLock vs OnceLock

```
  LazyLock<T>:                       OnceLock<T>:
  ────────────                       ────────────
  Closure definido en declaracion    Valor seteado despues
  static X: LazyLock<T> =           static X: OnceLock<T> =
    LazyLock::new(|| { ... });         OnceLock::new();
                                     X.set(value);
  No acepta config externa           Acepta config de runtime
  Siempre exitoso (closure corre)    set() puede fallar si ya init
  Equivale a pthread_once puro       Equivale a init() + get() de T01
  Ideal: logger, build info          Ideal: db pool, app config
```

### 3.2 OnceLock con get_or_init

Para combinar lazy init con un valor por defecto:

```rust
static CONFIG: OnceLock<AppConfig> = OnceLock::new();

fn config() -> &'static AppConfig {
    CONFIG.get_or_init(|| {
        // Se ejecuta solo si no fue seteado con set()
        AppConfig::default()
    })
}

fn main() {
    // Opcion A: usar defaults (get_or_init provee)
    let c = config();

    // Opcion B: setear antes de acceder
    CONFIG.set(AppConfig { debug: true, ..Default::default() }).ok();
    let c = config();
}
```

---

## 4. Singleton mutable: Mutex y RwLock

`LazyLock` y `OnceLock` dan `&T` — referencia inmutable. Para
un singleton que necesita modificarse:

### 4.1 Con Mutex (acceso exclusivo)

```rust
use std::sync::{LazyLock, Mutex};

#[derive(Debug)]
struct AppState {
    request_count: u64,
    last_error: Option<String>,
    cache: Vec<(String, String)>,
}

// LazyLock<Mutex<T>> — init lazy, acceso mutable sincronizado
static STATE: LazyLock<Mutex<AppState>> = LazyLock::new(|| {
    Mutex::new(AppState {
        request_count: 0,
        last_error: None,
        cache: Vec::new(),
    })
});

fn handle_request() {
    let mut state = STATE.lock().unwrap();
    state.request_count += 1;
    // mutex se libera al salir del scope (Drop)
}

fn record_error(msg: &str) {
    let mut state = STATE.lock().unwrap();
    state.last_error = Some(msg.to_string());
}

fn get_stats() -> (u64, Option<String>) {
    let state = STATE.lock().unwrap();
    (state.request_count, state.last_error.clone())
}
```

### 4.2 Con RwLock (lectores multiples, escritor exclusivo)

```rust
use std::sync::{LazyLock, RwLock};

#[derive(Debug)]
struct Config {
    debug: bool,
    max_retries: u32,
    api_url: String,
}

static CONFIG: LazyLock<RwLock<Config>> = LazyLock::new(|| {
    RwLock::new(Config {
        debug: false,
        max_retries: 3,
        api_url: "https://api.example.com".to_string(),
    })
});

fn is_debug() -> bool {
    // Multiples threads pueden leer simultaneamente
    CONFIG.read().unwrap().debug
}

fn set_debug(val: bool) {
    // Solo un thread puede escribir, bloquea lectores
    CONFIG.write().unwrap().debug = val;
}
```

### 4.3 Comparacion

```
  Patron                         Lectura    Escritura    Contention
  ──────                         ───────    ─────────    ──────────
  LazyLock<T>                    &T libre   imposible    ninguna
  LazyLock<Mutex<T>>             lock       lock         toda op
  LazyLock<RwLock<T>>            read lock  write lock   solo writes
  OnceLock<T>                    &T libre   solo set()   ninguna

  Elegir:
  - Solo lectura despues de init → LazyLock<T> o OnceLock<T>
  - Muchas lecturas, pocas escrituras → LazyLock<RwLock<T>>
  - Escrituras frecuentes → LazyLock<Mutex<T>>
```

---

## 5. AtomicT: singleton para valores simples

Para contadores o flags, los atomics evitan Mutex completamente:

```rust
use std::sync::atomic::{AtomicU64, AtomicBool, Ordering};

static REQUEST_COUNT: AtomicU64 = AtomicU64::new(0);
static SHUTDOWN_FLAG: AtomicBool = AtomicBool::new(false);

fn handle_request() {
    REQUEST_COUNT.fetch_add(1, Ordering::Relaxed);
}

fn request_count() -> u64 {
    REQUEST_COUNT.load(Ordering::Relaxed)
}

fn shutdown() {
    SHUTDOWN_FLAG.store(true, Ordering::Release);
}

fn should_shutdown() -> bool {
    SHUTDOWN_FLAG.load(Ordering::Acquire)
}
```

```
  Mutex<u64>:                      AtomicU64:
  ───────────                      ──────────
  lock() → incrementar → unlock    fetch_add (una instruccion)
  ~25ns con contention             ~5ns con contention
  Protege cualquier tipo           Solo tipos primitivos
  Puede deadlock                   No puede deadlock
```

No necesitan LazyLock: los atomics se inicializan con `const fn`.

---

## 6. Por que singleton es un anti-pattern en Rust

### 6.1 Estado global vs ownership

```
  Filosofia de Rust:
  ─────────────────
  Cada dato tiene UN owner claro
  Las referencias son prestamos temporales
  El compilador verifica que no hay conflictos

  Singleton global:
  ─────────────────
  Owner: nadie (o todos)
  Referencias: cualquiera, en cualquier momento
  Conflictos: Mutex/RwLock en runtime, no compile time
```

Un singleton esquiva el sistema de ownership:

```rust
// Con ownership: el compilador verifica todo
fn process(db: &DbPool, logger: &Logger) {
    // db y logger tienen lifetime conocido
    // El compilador sabe que no se destruyen durante process()
}

// Con singleton: el compilador no puede verificar nada
fn process() {
    let db = DB_POOL.get().unwrap();      // Puede panic
    let logger = &*LOGGER;                // Siempre ok (LazyLock)
    // ¿db esta inicializado? Solo sabes en runtime
}
```

### 6.2 Testabilidad

```rust
// Con singleton: test contaminado
#[test]
fn test_handle_request() {
    handle_request();  // Modifica STATE global
    let (count, _) = get_stats();
    assert_eq!(count, 1);  // ¿O 5? Otro test ya lo incremento?
}

// Los tests de Rust corren en paralelo por defecto.
// Un test modifica el singleton → afecta a otro test.
// Resultado: tests flaky que pasan solos pero fallan juntos.
```

```rust
// Con dependency injection: test aislado
fn handle_request(state: &Mutex<AppState>) {
    let mut s = state.lock().unwrap();
    s.request_count += 1;
}

#[test]
fn test_handle_request() {
    // Cada test tiene su propio estado
    let state = Mutex::new(AppState {
        request_count: 0,
        last_error: None,
        cache: Vec::new(),
    });
    handle_request(&state);
    assert_eq!(state.lock().unwrap().request_count, 1);  // Siempre 1
}
```

### 6.3 Acoplamiento oculto (mismo problema que en C)

```rust
// La firma miente: dice que solo necesita Order
fn process_order(order: &Order) -> Result<(), OrderError> {
    let db = DB_POOL.get().unwrap();
    let cache = &*CACHE;
    let logger = &*LOGGER;
    // Tres dependencias ocultas
    todo!()
}

// La firma dice la verdad
fn process_order(
    order: &Order,
    db: &DbPool,
    cache: &Cache,
    logger: &Logger,
) -> Result<(), OrderError> {
    // Imposible llamar sin proveer las dependencias
    todo!()
}
```

---

## 7. Alternativa idiomatica: pasar estado como parametro

### 7.1 Struct de contexto

```rust
struct AppContext {
    db: DbPool,
    cache: Cache,
    logger: Logger,
}

impl AppContext {
    fn new(config: &AppConfig) -> Result<Self, AppError> {
        Ok(Self {
            db: DbPool::connect(&config.db_url, config.pool_size)?,
            cache: Cache::new(config.cache_size),
            logger: Logger::new(config.log_level),
        })
    }
}

fn handle_request(ctx: &AppContext, req: &Request) -> Response {
    ctx.logger.info(&format!("Handling {}", req.path));
    let data = ctx.db.query("SELECT ...")?;
    ctx.cache.put(&req.path, &data);
    Response::ok(data)
}

fn main() -> Result<(), AppError> {
    let config = AppConfig::from_file("app.toml")?;
    let ctx = AppContext::new(&config)?;

    // Pasar ctx a cada handler
    for req in server.incoming() {
        let resp = handle_request(&ctx, &req);
        req.respond(resp);
    }
    Ok(())
}
```

### 7.2 Con Arc para multithreading

```rust
use std::sync::Arc;

fn main() -> Result<(), AppError> {
    let config = AppConfig::from_file("app.toml")?;
    let ctx = Arc::new(AppContext::new(&config)?);

    let mut handles = vec![];
    for _ in 0..4 {
        let ctx = Arc::clone(&ctx);
        handles.push(std::thread::spawn(move || {
            // Cada thread tiene su propio Arc, comparten el AppContext
            worker_loop(&ctx);
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
    Ok(())
}
```

```
  Singleton global:              Arc<AppContext>:
  ────────────────               ───────────────
  static CONTEXT: LazyLock<...>  let ctx = Arc::new(...)
  Acceso desde cualquier lado    Acceso solo donde se pasa ctx
  Init en cualquier momento      Init explicito en main
  Lifetime = programa entero     Lifetime = ultimo Arc<> vivo
  Dificil de testear             Facil: cada test crea su ctx
  Un solo "entorno"              Multiples: prod, test, staging
```

---

## 8. Cuando singleton SI es aceptable en Rust

A pesar de los problemas, hay casos validos:

### 8.1 Logger global (tracing, log)

```rust
// El crate `tracing` usa un subscriber global
use tracing::{info, warn};
use tracing_subscriber;

fn main() {
    // Init una vez
    tracing_subscriber::fmt::init();

    // Uso desde cualquier lugar con macros
    info!("starting up");
    warn!("something happened");
}

// Internamente, tracing usa un singleton (OnceLock).
// Justificacion: loggear debe ser posible desde
// CUALQUIER parte del codigo sin pasar &logger.
// El costo de DI para logging es desproporcionado.
```

### 8.2 Allocator global

```rust
// El allocator global ES un singleton por definicion
#[global_allocator]
static ALLOC: jemallocator::Jemalloc = jemallocator::Jemalloc;

// Solo puede haber uno. No tiene sentido inyectarlo.
```

### 8.3 Config read-only

```rust
use std::sync::LazyLock;

static CONFIG: LazyLock<AppConfig> = LazyLock::new(|| {
    AppConfig::from_env().expect("Invalid configuration")
});

// Si la config nunca cambia despues de init, y se lee
// desde muchos lugares, un singleton inmutable es practico.
// No necesita Mutex (inmutable), no afecta tests
// (cada test puede setear env vars diferentes... en teoria).
```

### 8.4 Regla practica

```
  ┌──────────────────────────────────────────────────────┐
  │  Singleton aceptable cuando:                         │
  │                                                      │
  │  1. El recurso es genuinamente global                │
  │     (logging, allocator, signal handlers)             │
  │                                                      │
  │  2. Es inmutable despues de init                     │
  │     (config, build info, feature flags)              │
  │                                                      │
  │  3. La alternativa (DI) es desproporcionadamente     │
  │     costosa (pasar &logger a 200 funciones)          │
  │                                                      │
  │  Si necesita mutarse: preferir Arc<Mutex<T>>         │
  │  pasado como parametro, no singleton global.         │
  └──────────────────────────────────────────────────────┘
```

---

## 9. Comparacion completa: C vs Rust

```
  Mecanismo C (T01)              Equivalente Rust
  ──────────────────             ────────────────
  static var + pthread_once      LazyLock<T>
  static var + init() separado   OnceLock<T> con set()/get()
  static var + mutex ops         LazyLock<Mutex<T>>
  static var + rwlock ops        LazyLock<RwLock<T>>
  atomic counter                 AtomicU64/AtomicBool
  __attribute__((constructor))   No equivalente directo
  atexit(cleanup)                Drop (automatico al final)

  Lo que Rust agrega:
  ────────────────────
  • static mut requiere unsafe → no puedes "olvidar" la sync
  • LazyLock/OnceLock son Send + Sync → thread safe por tipo
  • Mutex<T> protege el dato, no una seccion critica
    → imposible acceder sin lockear
  • Drop automatico → no necesitas atexit manual
```

---

## 10. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| `static mut` sin sincronizacion | Data race: UB incluso con `unsafe` | `LazyLock`, `OnceLock`, o `Mutex` |
| `LazyLock` para singleton con config | El closure no puede capturar config de runtime | `OnceLock` con `set()` |
| `OnceLock.get().unwrap()` sin verificar init | Panic si `set()` no fue llamado | `get_or_init()` con default |
| Singleton mutable sin Mutex | `LazyLock<T>` da `&T`, no `&mut T` | `LazyLock<Mutex<T>>` o `LazyLock<RwLock<T>>` |
| Tests con singleton compartido | Tests paralelos contaminan estado | DI con estado local por test |
| Mutex para flag/contador simple | Overhead innecesario | `AtomicBool`, `AtomicU64` |
| Singleton para todo | Acoplamiento oculto, dificulta testing | DI con `Arc<AppContext>` |
| Panic en closure de LazyLock | LazyLock queda envenenado, todos los accesos posteriores panic | Validar datos antes del closure |

---

## 11. Ejercicios

### Ejercicio 1 — LazyLock basico

Implementa un singleton `BuildInfo` con LazyLock que almacene
version, commit hash, y fecha de build como `&'static str`.

**Prediccion**: ¿necesita Mutex? ¿Por que o por que no?

<details>
<summary>Respuesta</summary>

```rust
use std::sync::LazyLock;

struct BuildInfo {
    version: &'static str,
    commit: &'static str,
    date: &'static str,
}

static BUILD_INFO: LazyLock<BuildInfo> = LazyLock::new(|| {
    BuildInfo {
        version: env!("CARGO_PKG_VERSION"),
        commit: option_env!("BUILD_COMMIT").unwrap_or("unknown"),
        date: option_env!("BUILD_DATE").unwrap_or("unknown"),
    }
});

fn main() {
    println!("Version: {}", BUILD_INFO.version);
    println!("Commit:  {}", BUILD_INFO.commit);
    println!("Date:    {}", BUILD_INFO.date);
}
```

No necesita Mutex. Los campos son `&'static str` (inmutables) y
nunca se modifican despues de la inicializacion. `LazyLock` ya
garantiza que la inicializacion es thread-safe. Multiples threads
pueden leer `BUILD_INFO.version` simultaneamente sin sincronizacion.

Es el singleton ideal: init once, read many, never mutate.

</details>

---

### Ejercicio 2 — OnceLock con configuracion

Implementa un singleton `DbPool` usando OnceLock que se
inicializa con host y pool_size de runtime.

**Prediccion**: ¿que pasa si alguien llama `db_pool()` antes
de `db_pool_init()`? ¿Y si llaman `db_pool_init()` dos veces?

<details>
<summary>Respuesta</summary>

```rust
use std::sync::OnceLock;

struct DbPool {
    host: String,
    pool_size: usize,
}

impl DbPool {
    fn query(&self, sql: &str) -> String {
        format!("[{}] Executing: {}", self.host, sql)
    }
}

static DB_POOL: OnceLock<DbPool> = OnceLock::new();

fn db_pool_init(host: &str, pool_size: usize) -> Result<(), &'static str> {
    DB_POOL.set(DbPool {
        host: host.to_string(),
        pool_size,
    }).map_err(|_| "DbPool already initialized")
}

fn db_pool() -> &'static DbPool {
    DB_POOL.get().expect("DbPool not initialized")
}

fn main() {
    // Init con parametros de runtime
    db_pool_init("postgres://localhost:5432", 10).unwrap();

    // Uso
    println!("{}", db_pool().query("SELECT 1"));

    // Segunda init: error
    let result = db_pool_init("other-host", 5);
    assert!(result.is_err());
}
```

- `db_pool()` antes de `db_pool_init()`: `get()` retorna `None`,
  `expect()` hace panic con el mensaje dado.
- `db_pool_init()` dos veces: `set()` retorna `Err(value)` con
  el valor que no pudo insertar. El primer valor permanece.

Alternativa mas segura con `get_or_init`:

```rust
fn db_pool() -> &'static DbPool {
    DB_POOL.get_or_init(|| {
        // Fallback si nadie llamo init
        DbPool { host: "localhost:5432".to_string(), pool_size: 5 }
    })
}
```

</details>

---

### Ejercicio 3 — Singleton mutable con Mutex

Implementa un contador de requests global thread-safe.

**Prediccion**: ¿es mejor `LazyLock<Mutex<u64>>` o `AtomicU64`?

<details>
<summary>Respuesta</summary>

`AtomicU64` es mejor para un simple contador:

```rust
use std::sync::atomic::{AtomicU64, Ordering};

static REQUEST_COUNT: AtomicU64 = AtomicU64::new(0);

fn increment() {
    REQUEST_COUNT.fetch_add(1, Ordering::Relaxed);
}

fn count() -> u64 {
    REQUEST_COUNT.load(Ordering::Relaxed)
}

fn main() {
    let handles: Vec<_> = (0..4).map(|_| {
        std::thread::spawn(|| {
            for _ in 0..1000 {
                increment();
            }
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    println!("Total: {}", count());  // 4000
}
```

`Mutex<u64>` para comparar:

```rust
use std::sync::{LazyLock, Mutex};

static REQUEST_COUNT: LazyLock<Mutex<u64>> = LazyLock::new(|| Mutex::new(0));

fn increment() {
    *REQUEST_COUNT.lock().unwrap() += 1;
}
```

Diferencias:
- `AtomicU64`: una instruccion CPU, ~5ns, no puede panic
- `Mutex<u64>`: lock + increment + unlock, ~25ns, puede poison

Para un solo contador → `AtomicU64`. Para estado compuesto
(contador + ultimo error + timestamp) → `Mutex<AppState>`.

</details>

---

### Ejercicio 4 — Singleton vs dependency injection

Refactoriza este codigo de singleton a DI:

```rust
use std::sync::LazyLock;

static CACHE: LazyLock<Mutex<HashMap<String, String>>> =
    LazyLock::new(|| Mutex::new(HashMap::new()));

fn get_user(id: u64) -> Option<String> {
    let cache = CACHE.lock().unwrap();
    cache.get(&id.to_string()).cloned()
}

fn set_user(id: u64, name: &str) {
    let mut cache = CACHE.lock().unwrap();
    cache.insert(id.to_string(), name.to_string());
}
```

**Prediccion**: ¿cuantos parametros extra necesitan las funciones?
¿Que tipo tiene el parametro?

<details>
<summary>Respuesta</summary>

Un parametro extra: `&Mutex<HashMap<String, String>>` (o un
type alias):

```rust
use std::collections::HashMap;
use std::sync::Mutex;

type UserCache = Mutex<HashMap<String, String>>;

fn get_user(cache: &UserCache, id: u64) -> Option<String> {
    let c = cache.lock().unwrap();
    c.get(&id.to_string()).cloned()
}

fn set_user(cache: &UserCache, id: u64, name: &str) {
    let mut c = cache.lock().unwrap();
    c.insert(id.to_string(), name.to_string());
}

fn main() {
    let cache = Mutex::new(HashMap::new());

    set_user(&cache, 1, "Alice");
    set_user(&cache, 2, "Bob");

    assert_eq!(get_user(&cache, 1), Some("Alice".to_string()));
    assert_eq!(get_user(&cache, 3), None);
}

#[test]
fn test_get_user() {
    // Cada test tiene su propio cache — aislado
    let cache = Mutex::new(HashMap::new());
    set_user(&cache, 42, "Test User");
    assert_eq!(get_user(&cache, 42), Some("Test User".to_string()));
    assert_eq!(get_user(&cache, 99), None);
}
```

Un parametro extra (`&UserCache`). La ventaja es que en tests
cada test crea su propio `HashMap` — sin contaminacion entre tests.

Para multithreading, se pasa `Arc<UserCache>`:

```rust
let cache = Arc::new(Mutex::new(HashMap::new()));
let c2 = Arc::clone(&cache);
std::thread::spawn(move || {
    set_user(&c2, 1, "Alice");
});
```

</details>

---

### Ejercicio 5 — RwLock singleton

Implementa un singleton de feature flags con `RwLock`:
- `is_enabled(flag_name) -> bool` (lectura concurrente)
- `set_flag(flag_name, enabled)` (escritura exclusiva)

**Prediccion**: ¿por que RwLock y no Mutex?

<details>
<summary>Respuesta</summary>

```rust
use std::collections::HashMap;
use std::sync::{LazyLock, RwLock};

type FeatureFlags = RwLock<HashMap<String, bool>>;

static FLAGS: LazyLock<FeatureFlags> = LazyLock::new(|| {
    let mut map = HashMap::new();
    map.insert("dark_mode".to_string(), false);
    map.insert("new_checkout".to_string(), false);
    map.insert("beta_api".to_string(), true);
    RwLock::new(map)
});

fn is_enabled(flag: &str) -> bool {
    let flags = FLAGS.read().unwrap();
    flags.get(flag).copied().unwrap_or(false)
}

fn set_flag(flag: &str, enabled: bool) {
    let mut flags = FLAGS.write().unwrap();
    flags.insert(flag.to_string(), enabled);
}

fn main() {
    // Multiples threads pueden leer simultaneamente
    assert!(!is_enabled("dark_mode"));
    assert!(is_enabled("beta_api"));

    // Solo uno puede escribir
    set_flag("dark_mode", true);
    assert!(is_enabled("dark_mode"));
}
```

RwLock en vez de Mutex porque:
- `is_enabled` se llama en cada request (miles/segundo)
- `set_flag` se llama raramente (admin cambia un flag)
- Con Mutex: cada `is_enabled` bloquea a todos los demas
- Con RwLock: 100 threads leen simultaneamente, solo se
  bloquean cuando alguien escribe

```
  Patron de acceso tipico:        Mutex:      RwLock:
  1000 reads/sec + 1 write/sec    1001 locks  1000 reads paralelas
                                              + 1 write lock
```

</details>

---

### Ejercicio 6 — Comparar con C

T01 mostro el patron `pthread_once + mutex` para un logger en C.
Implementa el equivalente en Rust.

**Prediccion**: ¿cuantas lineas menos necesitas? ¿Que errores
de T01 son imposibles en Rust?

<details>
<summary>Respuesta</summary>

```rust
use std::io::Write;
use std::sync::{LazyLock, Mutex};

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
enum LogLevel { Debug, Info, Warn, Error }

struct Logger {
    output: Mutex<Box<dyn Write + Send>>,
    min_level: LogLevel,
}

static LOGGER: LazyLock<Logger> = LazyLock::new(|| {
    Logger {
        output: Mutex::new(Box::new(std::io::stderr())),
        min_level: LogLevel::Info,
    }
});

impl Logger {
    fn log(&self, level: LogLevel, msg: &str) {
        if level >= self.min_level {
            let mut out = self.output.lock().unwrap();
            writeln!(out, "[{:?}] {}", level, msg).ok();
        }
    }
}

fn main() {
    LOGGER.log(LogLevel::Info, "starting");
    LOGGER.log(LogLevel::Debug, "skipped (below min_level)");
    LOGGER.log(LogLevel::Error, "something broke");
}
```

**Lineas**: ~25 en Rust vs ~60 en C (sin contar header).

**Errores de T01 imposibles en Rust**:
1. Olvidar `pthread_once` → race condition: imposible, `LazyLock`
   maneja la sincronizacion automaticamente
2. Olvidar mutex en `logger_write` → output intercalado:
   `Mutex<Box<dyn Write>>` obliga a lockear para escribir
3. Olvidar `pthread_mutex_destroy` → leak: `Drop` lo hace
4. `static` en header crea copias: Rust no tiene headers,
   `static` es siempre unico
5. Acceder antes de init → NULL deref: `LazyLock` inicializa
   automaticamente, no hay estado "no inicializado"

</details>

---

### Ejercicio 7 — Test con singleton

Este test es flaky. Explica por que y arreglalo:

```rust
static COUNTER: AtomicU64 = AtomicU64::new(0);

fn process_item() {
    COUNTER.fetch_add(1, Ordering::Relaxed);
}

#[test]
fn test_process_one_item() {
    process_item();
    assert_eq!(COUNTER.load(Ordering::Relaxed), 1);
}

#[test]
fn test_process_three_items() {
    process_item();
    process_item();
    process_item();
    assert_eq!(COUNTER.load(Ordering::Relaxed), 3);
}
```

**Prediccion**: ¿por que falla intermitentemente?

<details>
<summary>Respuesta</summary>

Los tests corren en paralelo y comparten `COUNTER`:

```
  Escenario 1 (test_one primero):
  test_one: process → COUNTER = 1, assert 1 ✓
  test_three: process x3 → COUNTER = 4, assert 3 ✗ FAIL

  Escenario 2 (test_three primero):
  test_three: process x3 → COUNTER = 3, assert 3 ✓
  test_one: process → COUNTER = 4, assert 1 ✗ FAIL

  Escenario 3 (intercalados):
  Cualquier combinacion — impredecible
```

**Solucion**: dependency injection — no usar singleton en tests:

```rust
use std::sync::atomic::{AtomicU64, Ordering};

fn process_item(counter: &AtomicU64) {
    counter.fetch_add(1, Ordering::Relaxed);
}

#[test]
fn test_process_one_item() {
    let counter = AtomicU64::new(0);
    process_item(&counter);
    assert_eq!(counter.load(Ordering::Relaxed), 1);
}

#[test]
fn test_process_three_items() {
    let counter = AtomicU64::new(0);
    process_item(&counter);
    process_item(&counter);
    process_item(&counter);
    assert_eq!(counter.load(Ordering::Relaxed), 3);
}
```

Cada test tiene su propio `AtomicU64` → aislamiento total →
tests siempre pasan, en cualquier orden, en paralelo.

</details>

---

### Ejercicio 8 — Contexto con Arc

Convierte estos tres singletons en un `AppContext` con `Arc`:

```rust
static DB: LazyLock<DbPool> = LazyLock::new(|| DbPool::connect("localhost"));
static CACHE: LazyLock<Mutex<Cache>> = LazyLock::new(|| Mutex::new(Cache::new(1024)));
static LOGGER: LazyLock<Logger> = LazyLock::new(|| Logger::new(LogLevel::Info));
```

**Prediccion**: ¿el AppContext necesita Mutex? ¿O solo el Cache?

<details>
<summary>Respuesta</summary>

Solo el Cache necesita Mutex (es el unico que muta):

```rust
use std::sync::{Arc, Mutex};

struct AppContext {
    db: DbPool,               // Inmutable (connection pool maneja sync)
    cache: Mutex<Cache>,       // Mutable → necesita Mutex
    logger: Logger,            // Inmutable (o tiene Mutex interno)
}

impl AppContext {
    fn new(db_url: &str, cache_size: usize, log_level: LogLevel) -> Self {
        Self {
            db: DbPool::connect(db_url),
            cache: Mutex::new(Cache::new(cache_size)),
            logger: Logger::new(log_level),
        }
    }
}

fn handle_request(ctx: &AppContext, req: &Request) -> Response {
    ctx.logger.info(&format!("Handling {}", req.path));

    // Cache necesita lock
    {
        let cache = ctx.cache.lock().unwrap();
        if let Some(cached) = cache.get(&req.path) {
            return Response::ok(cached);
        }
    }  // Lock liberado

    let data = ctx.db.query(&req.query);

    {
        let mut cache = ctx.cache.lock().unwrap();
        cache.put(&req.path, &data);
    }

    Response::ok(&data)
}

fn main() {
    let ctx = Arc::new(AppContext::new("localhost:5432", 1024, LogLevel::Info));

    let handles: Vec<_> = (0..4).map(|_| {
        let ctx = Arc::clone(&ctx);
        std::thread::spawn(move || {
            // Cada worker tiene Arc al mismo contexto
            worker_loop(&ctx);
        })
    }).collect();

    for h in handles { h.join().unwrap(); }
    // ctx se destruye aqui (ultimo Arc)
    // Drop de DbPool cierra conexiones
    // Drop de Cache libera memoria
}
```

`AppContext` no necesita Mutex porque:
- `db` es inmutable (pools internos ya son thread-safe)
- `logger` es inmutable (o internamente sincronizado)
- Solo `cache` muta → solo cache tiene Mutex

`Arc` permite compartir el contexto entre threads.
Cuando el ultimo `Arc` se destruye, `Drop` se ejecuta
automaticamente — equivalente a `atexit` de C pero
determinista.

</details>

---

### Ejercicio 9 — Singleton que inicializa desde env

Implementa un singleton `Config` que lee de variables de entorno
usando `OnceLock` con `get_or_init`.

**Prediccion**: ¿que ventaja tiene `get_or_init` sobre `set` + `get`
para este caso?

<details>
<summary>Respuesta</summary>

```rust
use std::sync::OnceLock;

#[derive(Debug)]
struct Config {
    database_url: String,
    port: u16,
    debug: bool,
}

static CONFIG: OnceLock<Config> = OnceLock::new();

fn config() -> &'static Config {
    CONFIG.get_or_init(|| {
        Config {
            database_url: std::env::var("DATABASE_URL")
                .unwrap_or_else(|_| "sqlite://local.db".to_string()),
            port: std::env::var("PORT")
                .ok()
                .and_then(|p| p.parse().ok())
                .unwrap_or(8080),
            debug: std::env::var("DEBUG")
                .map(|v| v == "1" || v == "true")
                .unwrap_or(false),
        }
    })
}

/// Permite override en tests o main
fn config_override(cfg: Config) -> Result<(), Config> {
    CONFIG.set(cfg)
}

fn main() {
    // Opcion A: leer de env automaticamente
    println!("Port: {}", config().port);

    // Opcion B: override antes del primer acceso
    // config_override(Config { port: 9090, .. });
    // println!("Port: {}", config().port);
}
```

**Ventaja de `get_or_init` sobre `set` + `get`**:

Con `set` + `get`:
```rust
fn config() -> &'static Config {
    // ¿Que pasa si nadie llamo set()?
    CONFIG.get().expect("Config not initialized")  // PANIC
}
```

Con `get_or_init`:
```rust
fn config() -> &'static Config {
    CONFIG.get_or_init(|| Config::from_env())
    // Si set() fue llamado: retorna ese valor
    // Si no: inicializa con from_env()
    // NUNCA panic
}
```

`get_or_init` combina lo mejor: permite override con `set()`,
pero tiene fallback si nadie override. No necesitas preocuparte
por el orden de inicializacion.

</details>

---

### Ejercicio 10 — Analisis: tracing como singleton bien hecho

El crate `tracing` usa un singleton global (subscriber).
Analiza este patron:

```rust
use tracing::{info, span, Level};
use tracing_subscriber::fmt;

fn main() {
    // Init global (singleton)
    fmt::init();

    // Uso desde cualquier lugar — sin pasar &logger
    let span = span!(Level::INFO, "my_span");
    let _enter = span.enter();

    info!("this is inside the span");
    do_work();
}

fn do_work() {
    info!("no need to pass logger");  // Accede al singleton
}
```

**Prediccion**: ¿por que tracing usa singleton en vez de DI?
Identifica al menos 3 razones.

<details>
<summary>Respuesta</summary>

**1. Ubicuidad**: logging se usa en TODAS partes — domain logic,
error handlers, middleware, libraries de terceros. Pasar `&Logger`
a cada funcion contaminaria todas las firmas del programa:

```rust
// Sin singleton: CADA funcion necesita el parametro
fn parse_config(logger: &Logger, path: &str) -> Config { ... }
fn connect_db(logger: &Logger, url: &str) -> Pool { ... }
fn handle_request(logger: &Logger, req: Request) -> Response { ... }
// Y cada libreria de terceros tambien...
```

**2. Composabilidad de librerias**: una libreria que loggea (como
`hyper` o `sqlx`) no puede saber que logger usa la aplicacion.
Con un singleton dispatcher, la libreria emite eventos y la
aplicacion decide como procesarlos:

```rust
// hyper (libreria) — emite sin saber quien escucha
tracing::debug!("received HTTP request");

// Tu app — decide el subscriber
tracing_subscriber::fmt()
    .with_max_level(Level::INFO)  // Filtrar debug de hyper
    .init();
```

**3. Overhead minimo cuando desactivado**: el singleton dispatcher
comprueba si hay subscriber con una lectura atomica. Si no hay,
los macros `info!()` etc. se resuelven a un branch que no hace
nada. Pasar `&Logger` por parametro no puede ser "gratis" porque
el parametro siempre existe.

**4. Thread safety por diseno**: el subscriber global usa
`OnceLock` internamente. Una vez seteado, es inmutable (las
llamadas `info!()` son lecturas concurrentes sin lock). El
subscriber internamente puede tener estado mutable propio
(con su propia sincronizacion).

**5. Ergonomia de macros**: `info!("msg")` es posible porque el
macro accede al singleton internamente. Con DI seria
`logger.info("msg")` — viable pero no componible con el
sistema de spans y layers de tracing.

**Conclusion**: logging es el caso canonico donde singleton
se justifica. El costo de DI (contaminar toda firma, forzar
a librerias a aceptar un logger) supera con creces el costo
del singleton (estado global, dificil de testear — aunque
tracing resuelve testing con `tracing_test::subscriber`).

</details>
