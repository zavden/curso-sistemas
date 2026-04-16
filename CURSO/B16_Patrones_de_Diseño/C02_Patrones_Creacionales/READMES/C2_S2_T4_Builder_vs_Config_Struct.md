# T04 — Builder vs Config Struct

---

## 1. Dos soluciones al mismo problema

T01-T03 presentaron tres mecanismos para construir objetos complejos:

```
  Config struct (C/Rust)     Builder (Rust)        Typestate (Rust)
  ─────────────────────      ──────────────        ────────────────
  Campos publicos            Campos privados       Campos en tipo generico
  Default + override         Setters + build()     Setters + build()
  Sin validacion forzosa     Validacion en build    Validacion en compile
  Inmediato                  Paso a paso            Paso a paso
  Simple                     Moderado               Complejo
```

La pregunta central de este topico: **cuando basta un config struct con
Default, y cuando el builder aporta valor real?**

---

## 2. El config struct: la opcion mas simple

### 2.1 En C (designated initializers)

```c
/* server_config.h */
typedef struct {
    const char *host;        /* "0.0.0.0" */
    int         port;        /* 8080 */
    int         max_conn;    /* 1024 */
    int         timeout_ms;  /* 5000 */
    bool        use_tls;     /* false */
    const char *cert_path;   /* NULL */
} ServerConfig;

static const ServerConfig SERVER_CONFIG_DEFAULT = {
    .host       = "0.0.0.0",
    .port       = 8080,
    .max_conn   = 1024,
    .timeout_ms = 5000,
    .use_tls    = false,
    .cert_path  = NULL,
};

/* Uso: */
ServerConfig cfg = SERVER_CONFIG_DEFAULT;
cfg.port    = 443;
cfg.use_tls = true;
```

### 2.2 En Rust (Default trait)

```rust
#[derive(Debug, Clone)]
pub struct ServerConfig {
    pub host: String,
    pub port: u16,
    pub max_conn: usize,
    pub timeout_ms: u64,
    pub use_tls: bool,
    pub cert_path: Option<String>,
}

impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            host: "0.0.0.0".to_string(),
            port: 8080,
            max_conn: 1024,
            timeout_ms: 5000,
            use_tls: false,
            cert_path: None,
        }
    }
}

// Uso con struct update syntax:
let cfg = ServerConfig {
    port: 443,
    use_tls: true,
    ..Default::default()
};
```

### 2.3 Fortalezas del config struct

```
  ✓ Cero boilerplate         — no hay builder separado
  ✓ Campos visibles          — el IDE muestra todos los campos
  ✓ Facil de serializar      — #[derive(Serialize, Deserialize)]
  ✓ Facil de clonar/copiar   — modificar una copia es natural
  ✓ Familiar                 — cualquier programador lo entiende
  ✓ Composicion directa      — un config puede contener otro config
```

---

## 3. El builder: cuando agrega valor

### 3.1 Caso 1 — Validacion cruzada entre campos

Con un config struct, nada impide estados invalidos:

```rust
// Config struct: compila, pero es invalido
let cfg = ServerConfig {
    use_tls: true,
    cert_path: None,       // TLS sin certificado!
    ..Default::default()
};
// El error se descubre al usar cfg, no al crearlo
```

Un builder puede rechazarlo en `build()`:

```rust
impl ServerBuilder {
    pub fn build(self) -> Result<ServerConfig, ConfigError> {
        if self.use_tls && self.cert_path.is_none() {
            return Err(ConfigError::TlsWithoutCert);
        }
        if self.port == 0 {
            return Err(ConfigError::InvalidPort);
        }
        if self.timeout_ms < 100 {
            return Err(ConfigError::TimeoutTooLow(self.timeout_ms));
        }
        Ok(ServerConfig {
            host: self.host.unwrap_or_else(|| "0.0.0.0".to_string()),
            port: self.port,
            max_conn: self.max_conn,
            timeout_ms: self.timeout_ms,
            use_tls: self.use_tls,
            cert_path: self.cert_path,
        })
    }
}
```

**Regla**: si la validez de un campo depende de otro campo,
un builder o constructor con validacion es mejor que un config struct
que permite estados invalidos silenciosamente.

### 3.2 Caso 2 — Campos privados e inmutabilidad post-construccion

```rust
// Config struct: campos publicos = mutables siempre
let mut cfg = ServerConfig { ..Default::default() };
cfg.port = 443;
// ... 200 lineas despues ...
cfg.port = 0;  // Alguien lo modifica por error

// Builder: producto inmutable
let server = ServerBuilder::new()
    .port(443)
    .build()?;
// server.port es privado, no se puede modificar
// Solo accesible via server.port() getter
```

```
  Config struct:                    Builder:
  ───────────────                   ────────
  Campos pub                        Campos privados en producto
  Mutable despues de crear          Inmutable despues de build()
  Cualquiera modifica               Solo el builder puede setear
```

**Regla**: si el objeto no debe modificarse despues de la creacion,
el builder produce un producto sellado. El config struct queda
abierto.

### 3.3 Caso 3 — Construccion con efectos secundarios

```rust
// Builder que ejecuta logica en cada paso
impl DatabaseBuilder {
    pub fn host(mut self, host: &str) -> Self {
        // Resuelve DNS al setear, no al build()
        self.resolved_addr = dns_resolve(host);
        self.host = host.to_string();
        self
    }

    pub fn credentials(mut self, user: &str, pass: &str) -> Self {
        // Hashea la password al setear
        self.pass_hash = hash_password(pass);
        self.user = user.to_string();
        self
    }

    pub fn build(self) -> Result<Database, DbError> {
        // resolved_addr y pass_hash ya estan listos
        Database::connect(self.resolved_addr, &self.user, &self.pass_hash)
    }
}
```

Un config struct no puede ejecutar logica al asignar campos.

**Regla**: si la construccion requiere transformaciones intermedias
(parseo, resolucion, validacion progresiva), un builder las encapsula.

### 3.4 Caso 4 — API publica de una libreria

```rust
// Libreria v1:
pub struct Config {
    pub port: u16,
    pub host: String,
}

// Libreria v2: necesito agregar un campo
pub struct Config {
    pub port: u16,
    pub host: String,
    pub max_conn: usize,  // NUEVO — rompe todos los callers!
}
// Todo codigo que usaba Config { port: 8080, host: "..." }
// ahora no compila: "missing field max_conn"
```

Soluciones:

```rust
// Opcion A: #[non_exhaustive]
#[non_exhaustive]
pub struct Config {
    pub port: u16,
    pub host: String,
}
// Fuerza uso de ..Default::default() — funciona pero es parcial

// Opcion B: Builder
pub struct ConfigBuilder { /* campos privados */ }
// Agregar campos al builder no rompe callers existentes
// Solo callers que necesitan el nuevo campo lo usan
```

**Regla**: en una API publica, el builder protege contra breaking
changes al agregar campos. `#[non_exhaustive]` ayuda pero no
reemplaza la validacion.

---

## 4. Matriz de decision

```
  Criterio                          Config struct   Builder
  ─────────────────────────────     ─────────────   ───────
  Todos los campos independientes        ✓             ○
  Validacion cruzada entre campos        ✗             ✓
  Producto debe ser inmutable            ✗             ✓
  Logica en asignacion de campos         ✗             ✓
  API publica de libreria                △             ✓
  Serializacion directa                  ✓             ○
  Pocos campos (< 5)                     ✓             ✗
  Muchos campos (> 10)                   ○             ✓
  Config anidado (config de config)      ✓             ○
  Prototipo rapido                       ✓             ✗

  ✓ = mejor opcion    ○ = funciona    ✗ = mala opcion    △ = depende
```

### 4.1 Flowchart de decision

```
  Necesito construir un objeto con multiples parametros?
  │
  ├── Solo 2-3 campos → parametros directos en fn new()
  │
  ├── 4+ campos →
  │   │
  │   ├── Todos los campos son independientes?
  │   │   ├── SI → Los campos tienen restricciones cruzadas?
  │   │   │        ├── NO → Config struct con Default
  │   │   │        └── SI → Builder con validacion
  │   │   │
  │   │   └── NO (dependen entre si) → Builder
  │   │
  │   ├── El producto debe ser inmutable post-creacion?
  │   │   ├── NO → Config struct puede bastar
  │   │   └── SI → Builder
  │   │
  │   ├── Es API publica de libreria?
  │   │   ├── NO → Config struct probablemente basta
  │   │   └── SI → Builder (protege compatibilidad)
  │   │
  │   └── Necesito validacion en compile time?
  │       ├── NO → Builder con Result
  │       └── SI → Typestate Builder (T03)
```

---

## 5. Patrones hibridos

### 5.1 Config struct que alimenta un builder

```rust
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ServerConfig {
    pub host: String,
    pub port: u16,
    pub max_conn: usize,
    pub use_tls: bool,
    pub cert_path: Option<String>,
}

pub struct Server { /* campos privados */ }

impl Server {
    pub fn from_config(cfg: &ServerConfig) -> Result<Self, ServerError> {
        // Validacion aqui
        if cfg.use_tls && cfg.cert_path.is_none() {
            return Err(ServerError::TlsWithoutCert);
        }
        if cfg.port == 0 {
            return Err(ServerError::InvalidPort);
        }
        Ok(Self {
            host: cfg.host.clone(),
            port: cfg.port,
            max_conn: cfg.max_conn,
            tls: if cfg.use_tls {
                Some(TlsContext::new(cfg.cert_path.as_deref().unwrap())?)
            } else {
                None
            },
        })
    }
}

// Config serializable + constructor validado
// Lo mejor de ambos mundos
let cfg: ServerConfig = serde_json::from_str(&json)?;
let server = Server::from_config(&cfg)?;
```

### 5.2 Builder que acepta un config base

```rust
impl ServerBuilder {
    /// Arranca desde un config existente (ej: leido de archivo)
    pub fn from_config(cfg: ServerConfig) -> Self {
        Self {
            host: Some(cfg.host),
            port: cfg.port,
            max_conn: cfg.max_conn,
            use_tls: cfg.use_tls,
            cert_path: cfg.cert_path,
        }
    }

    /// Arranca de cero con defaults
    pub fn new() -> Self {
        Self::from_config(ServerConfig::default())
    }

    // ... setters normales ...
}

// Caso tipico: config de archivo + overrides programaticos
let base: ServerConfig = load_from_file("server.toml")?;
let server = ServerBuilder::from_config(base)
    .port(9090)        // Override del archivo
    .use_tls(true)     // Override adicional
    .build()?;
```

### 5.3 Config struct con metodo validate

Punto intermedio: config struct publico con validacion explicita:

```rust
impl ServerConfig {
    pub fn validate(&self) -> Result<(), Vec<ConfigError>> {
        let mut errors = Vec::new();

        if self.port == 0 {
            errors.push(ConfigError::InvalidPort);
        }
        if self.use_tls && self.cert_path.is_none() {
            errors.push(ConfigError::TlsWithoutCert);
        }
        if self.max_conn == 0 {
            errors.push(ConfigError::InvalidMaxConn);
        }

        if errors.is_empty() { Ok(()) } else { Err(errors) }
    }
}

// Uso:
let cfg = ServerConfig {
    port: 443,
    use_tls: true,
    cert_path: Some("/etc/ssl/cert.pem".into()),
    ..Default::default()
};
cfg.validate()?;
let server = Server::from_validated(cfg);  // Asume que ya fue validado
```

**Desventaja**: nada obliga a llamar `validate()` antes de usar el config.

---

## 6. Comparacion en C

En C no hay method chaining comodo, asi que la decision es diferente:

```
  Mecanismo C                       Equivalente Rust
  ───────────                       ────────────────
  Config struct + defaults          Config struct + Default
  Config struct + validate()        Config struct + validate()
  Config struct + build()           Builder (hibrido)
  Function chaining (raro)          Builder con consume-and-return
```

### 6.1 El patron canonico en C: config → build → opaque

```c
/* config.h — publico, serializable */
typedef struct {
    const char *host;
    int         port;
    int         max_conn;
    bool        use_tls;
    const char *cert_path;
} ServerConfig;

static const ServerConfig SERVER_CONFIG_DEFAULT = { /* ... */ };

/* server.h — opaque */
typedef struct Server Server;

/* Funcion "build": valida config y crea server */
Server *server_create(const ServerConfig *cfg, ServerError *err);
void    server_destroy(Server *s);

/* server.c — privado */
struct Server {
    char host[256];
    int  port;
    int  max_conn;
    /* TLS handle interno — no expuesto en config */
    SSL_CTX *ssl_ctx;
};

Server *server_create(const ServerConfig *cfg, ServerError *err) {
    if (cfg->port <= 0 || cfg->port > 65535) {
        *err = SERVER_ERR_INVALID_PORT;
        return NULL;
    }
    if (cfg->use_tls && cfg->cert_path == NULL) {
        *err = SERVER_ERR_TLS_NO_CERT;
        return NULL;
    }

    Server *s = calloc(1, sizeof(*s));
    if (!s) { *err = SERVER_ERR_ALLOC; return NULL; }

    strncpy(s->host, cfg->host, sizeof(s->host) - 1);
    s->port     = cfg->port;
    s->max_conn = cfg->max_conn;

    if (cfg->use_tls) {
        s->ssl_ctx = setup_tls(cfg->cert_path);
        if (!s->ssl_ctx) {
            free(s);
            *err = SERVER_ERR_TLS_INIT;
            return NULL;
        }
    }
    *err = SERVER_ERR_OK;
    return s;
}
```

Este es el patron mas comun en C de produccion (sqlite3_open con
sqlite3_config, libcurl con CURLOPT_*, etc.). Es el equivalente
funcional del hibrido "config + builder" de Rust.

---

## 7. Casos reales: que eligieron los proyectos conocidos

### 7.1 Config struct gana

| Proyecto | Patron | Por que |
|---|---|---|
| serde_json::from_str | No builder | Deserializacion directa al tipo |
| toml::Value | Config struct | Datos sin restricciones cruzadas |
| clap (v2) | Struct con derive | `#[derive(Parser)]` genera todo |
| env_logger | Config struct | `Builder::new().filter_level().init()` ← en realidad es builder, pero era config struct en versiones tempranas |

### 7.2 Builder gana

| Proyecto | Patron | Por que |
|---|---|---|
| reqwest::Client | Builder | Validacion de TLS, timeouts cruzados, Connection pooling |
| tokio::runtime | Builder | Config compleja: threads, thread names, hooks |
| std::process::Command | Builder | Cada arg/env muta estado acumulativo |
| hyper::Server | Builder | Bind + configuracion incremental |
| regex::RegexBuilder | Builder | Flags que interactuan (case, multiline, unicode) |

### 7.3 Hibrido gana

| Proyecto | Patron | Por que |
|---|---|---|
| tracing_subscriber | Config + Builder | `EnvFilter` es config, `fmt::Layer` tiene builder |
| SQLite | Config struct + open() | `sqlite3_config()` + `sqlite3_open_v2()` |
| libcurl | Handle + CURLOPT | `curl_easy_init()` + setopt + perform |

---

## 8. Anti-patrones

### 8.1 Builder para 2 campos

```rust
// ANTI-PATRON: builder innecesario
struct PointBuilder {
    x: Option<f64>,
    y: Option<f64>,
}
impl PointBuilder {
    fn new() -> Self { Self { x: None, y: None } }
    fn x(mut self, x: f64) -> Self { self.x = Some(x); self }
    fn y(mut self, y: f64) -> Self { self.y = Some(y); self }
    fn build(self) -> Point {
        Point { x: self.x.unwrap(), y: self.y.unwrap() }
    }
}

// CORRECTO: constructor directo
impl Point {
    fn new(x: f64, y: f64) -> Self { Self { x, y } }
}
```

Regla practica: si **todos** los campos son obligatorios y son pocos,
`fn new()` con parametros es mejor. El builder brilla cuando hay
campos opcionales o validacion.

### 8.2 Config struct sin Default para muchos campos

```rust
// ANTI-PATRON: 10 campos y todos obligatorios en construccion
let cfg = ServerConfig {
    host: "0.0.0.0".to_string(),
    port: 8080,
    max_conn: 1024,
    timeout_ms: 5000,
    use_tls: false,
    cert_path: None,
    key_path: None,
    log_level: LogLevel::Info,
    worker_threads: 4,
    keep_alive: true,
};
// Caller debe especificar TODOS los campos cada vez
```

Si no implementas `Default`, el config struct pierde su mayor
ventaja: cambiar solo lo que difiere.

### 8.3 Typestate para config simple

```rust
// ANTI-PATRON: typestate para logger config
struct LoggerBuilder<Host, Level> { ... }
// 2 type parameters, PhantomData, generics...
// Para algo que validate() resuelve en 3 lineas

// CORRECTO: builder simple con Result
impl LoggerBuilder {
    fn build(self) -> Result<Logger, LoggerError> {
        let host = self.host.ok_or(LoggerError::MissingHost)?;
        let level = self.level.unwrap_or(Level::Info);
        Ok(Logger { host, level })
    }
}
```

Typestate se justifica cuando:
- Hay protocolo secuencial (pasos que deben ocurrir en orden)
- El costo de un error es alto (seguridad, corrupcion de datos)
- La API la usaran muchos equipos y el compiler error ahorra tiempo

### 8.4 Builder que expone el producto mutable

```rust
// ANTI-PATRON: el builder no protege nada
impl ServerBuilder {
    pub fn build(self) -> Server {
        Server {
            pub host: self.host,     // publico!
            pub port: self.port,     // publico!
        }
    }
}
// El caller puede modificar server.port despues
// ¿Para que era el builder entonces?
```

Si el producto tiene todos los campos publicos, el builder no
aporta encapsulacion. Valida usar un config struct directamente.

---

## 9. Resumen: reglas de decision

```
  ┌────────────────────────────────────────────────────┐
  │              ¿Que mecanismo elegir?                 │
  │                                                    │
  │  1. ¿Pocos campos, todos obligatorios?             │
  │     → fn new(a, b, c)                              │
  │                                                    │
  │  2. ¿Muchos campos, todos independientes,          │
  │     sin validacion cruzada?                        │
  │     → Config struct + Default                      │
  │                                                    │
  │  3. ¿Validacion cruzada o producto inmutable       │
  │     o API publica?                                 │
  │     → Builder con Result                           │
  │                                                    │
  │  4. ¿Protocolo secuencial o seguridad critica?     │
  │     → Typestate Builder                            │
  │                                                    │
  │  5. ¿Config de archivo + overrides programaticos?  │
  │     → Hibrido: Config struct + Builder/from_config │
  │                                                    │
  │  6. ¿En C?                                         │
  │     → Config struct + build function + opaque type │
  └────────────────────────────────────────────────────┘
```

---

## 10. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Builder para 2 campos obligatorios | Boilerplate sin beneficio | `fn new(a, b)` |
| Config struct sin Default | Callers repiten todos los campos | `impl Default` siempre |
| Config struct con validacion cruzada | Estados invalidos silenciosos | Builder o `validate()` |
| Typestate para config simple | Complejidad desproporcionada | Builder con `Result` |
| Builder con producto pub | No encapsula nada | Config struct o campos privados |
| Config struct en API publica sin `#[non_exhaustive]` | Agregar campo rompe callers | `#[non_exhaustive]` o builder |
| Hibrido sin validacion | Config de archivo puede ser invalido | `from_config` debe validar |
| Ignorar patrones existentes en el proyecto | Inconsistencia | Seguir la convencion del codebase |

---

## 11. Ejercicios

### Ejercicio 1 — Elegir mecanismo

Dado este tipo:

```rust
struct Color {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
}
```

**Prediccion**: ¿que mecanismo usarias (new, config struct, builder)?
¿Por que?

<details>
<summary>Respuesta</summary>

`fn new(r, g, b, a)` — solo 4 campos, todos obligatorios, todos
independientes, sin validacion cruzada. Un builder seria anti-patron
8.1. Un config struct no aporta nada con solo 4 campos del mismo tipo.

```rust
impl Color {
    pub fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    pub fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }
}
```

Named constructors (`rgb`, `rgba`, `from_hex`) son mejor que builder aqui.

</details>

---

### Ejercicio 2 — Config struct adecuado

```rust
struct LogConfig {
    level: Level,
    output: Output,
    color: bool,
    timestamp: bool,
    module_filter: Option<String>,
}
```

**Prediccion**: ¿config struct con Default basta? ¿Hay validacion
cruzada que justifique un builder?

<details>
<summary>Respuesta</summary>

Config struct con Default basta. Los campos son independientes:
- `level` no depende de `output`
- `color` no depende de `timestamp`
- `module_filter` es opcional

```rust
impl Default for LogConfig {
    fn default() -> Self {
        Self {
            level: Level::Info,
            output: Output::Stderr,
            color: true,
            timestamp: true,
            module_filter: None,
        }
    }
}

let cfg = LogConfig {
    level: Level::Debug,
    color: false,
    ..Default::default()
};
```

No hay validacion cruzada, no es API publica, no necesita inmutabilidad.
Builder seria overengineering.

</details>

---

### Ejercicio 3 — Validacion cruzada

```rust
struct EmailConfig {
    smtp_host: String,
    smtp_port: u16,
    use_tls: bool,
    username: Option<String>,
    password: Option<String>,
    from_address: String,
}
```

**Prediccion**: ¿config struct o builder? Identifica las restricciones
cruzadas.

<details>
<summary>Respuesta</summary>

Builder, porque hay al menos dos restricciones cruzadas:

1. Si `use_tls` es true, el port debe ser 465 o 587 (o al menos no 25)
2. Si `username` es Some, `password` debe ser Some (y viceversa)

```rust
impl EmailConfigBuilder {
    pub fn build(self) -> Result<EmailConfig, EmailError> {
        // Restriccion 1: TLS + port
        if self.use_tls && self.port == Some(25) {
            return Err(EmailError::PlaintextPort);
        }

        // Restriccion 2: auth completa
        match (&self.username, &self.password) {
            (Some(_), None) => return Err(EmailError::MissingPassword),
            (None, Some(_)) => return Err(EmailError::MissingUsername),
            _ => {}
        }

        Ok(EmailConfig {
            smtp_host: self.smtp_host.ok_or(EmailError::MissingHost)?,
            smtp_port: self.port.unwrap_or(if self.use_tls { 587 } else { 25 }),
            use_tls: self.use_tls,
            username: self.username,
            password: self.password,
            from_address: self.from_address.ok_or(EmailError::MissingFrom)?,
        })
    }
}
```

El config struct permitiria `use_tls: true, port: 25, username: Some("x"),
password: None` — todo invalido pero compila sin warning.

</details>

---

### Ejercicio 4 — Hibrido config + builder

Implementa un patron hibrido donde:
1. `AppConfig` se lee de un archivo TOML
2. Un builder permite overrides programaticos
3. `build()` valida todo

**Prediccion**: ¿cuantos structs necesitas? ¿Donde va cada validacion?

<details>
<summary>Respuesta</summary>

Dos structs: `AppConfig` (serializable) y `App` (producto validado).

```rust
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct AppConfig {
    pub database_url: String,
    pub port: u16,
    pub workers: usize,
    pub log_level: String,
}

pub struct App {
    db_pool: DbPool,       // Privado — construido en build
    port: u16,
    workers: usize,
    log_level: Level,
}

pub struct AppBuilder {
    config: AppConfig,
}

impl AppBuilder {
    pub fn from_file(path: &str) -> Result<Self, ConfigError> {
        let text = std::fs::read_to_string(path)?;
        let config: AppConfig = toml::from_str(&text)?;
        Ok(Self { config })
    }

    pub fn port(mut self, port: u16) -> Self {
        self.config.port = port;
        self
    }

    pub fn workers(mut self, n: usize) -> Self {
        self.config.workers = n;
        self
    }

    pub fn build(self) -> Result<App, AppError> {
        let log_level = self.config.log_level.parse::<Level>()
            .map_err(|_| AppError::InvalidLogLevel)?;

        if self.config.workers == 0 {
            return Err(AppError::ZeroWorkers);
        }

        let db_pool = DbPool::connect(&self.config.database_url)?;

        Ok(App {
            db_pool,
            port: self.config.port,
            workers: self.config.workers,
            log_level,
        })
    }
}

// Uso:
let app = AppBuilder::from_file("app.toml")?
    .port(9090)      // Override del archivo
    .build()?;
```

La validacion va en `build()` porque necesita ver el estado final
(despues de todos los overrides).

</details>

---

### Ejercicio 5 — Migrar config a builder

Este codigo usa config struct pero tiene bugs de validacion:

```rust
fn create_connection(cfg: &DbConfig) -> Connection {
    // BUG: si pool_size es 0, panic en runtime
    // BUG: si host esta vacio, timeout silencioso
    // BUG: si ssl y ssl_cert es None, handshake falla
    Connection::new(&cfg.host, cfg.port, cfg.pool_size, cfg.ssl, cfg.ssl_cert.as_deref())
}
```

**Prediccion**: reescribe esto con un builder que prevenga los 3 bugs.
¿Donde va cada validacion?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug)]
pub enum DbConfigError {
    EmptyHost,
    ZeroPoolSize,
    SslWithoutCert,
}

pub struct DbConfigBuilder {
    host: Option<String>,
    port: u16,
    pool_size: usize,
    ssl: bool,
    ssl_cert: Option<String>,
}

impl DbConfigBuilder {
    pub fn new() -> Self {
        Self {
            host: None,
            port: 5432,
            pool_size: 10,
            ssl: false,
            ssl_cert: None,
        }
    }

    pub fn host(mut self, host: &str) -> Self {
        self.host = Some(host.to_string());
        self
    }

    pub fn port(mut self, port: u16) -> Self {
        self.port = port;
        self
    }

    pub fn pool_size(mut self, size: usize) -> Self {
        self.pool_size = size;
        self
    }

    pub fn ssl(mut self, cert_path: &str) -> Self {
        self.ssl = true;
        self.ssl_cert = Some(cert_path.to_string());
        self
    }

    pub fn build(self) -> Result<Connection, DbConfigError> {
        let host = self.host.ok_or(DbConfigError::EmptyHost)?;
        if host.is_empty() {
            return Err(DbConfigError::EmptyHost);
        }
        if self.pool_size == 0 {
            return Err(DbConfigError::ZeroPoolSize);
        }
        if self.ssl && self.ssl_cert.is_none() {
            return Err(DbConfigError::SslWithoutCert);
        }
        Ok(Connection::new(
            &host, self.port, self.pool_size,
            self.ssl, self.ssl_cert.as_deref(),
        ))
    }
}
```

Truco clave: `ssl()` toma el cert_path como parametro, haciendo
imposible activar SSL sin certificado (elimina la restriccion cruzada
por diseno de la API, no por validacion).

</details>

---

### Ejercicio 6 — Decision en C

```c
typedef struct {
    const char *filename;
    const char *mode;       /* "r", "w", "a" */
    size_t      buffer_size;
    bool        sync_writes;
    int         permissions; /* solo si mode contiene 'w' */
} FileConfig;
```

**Prediccion**: ¿config struct + `file_open()` basta, o necesita
builder? ¿Hay restriccion cruzada?

<details>
<summary>Respuesta</summary>

Config struct + `file_open()` con validacion basta. La restriccion
cruzada (permissions solo relevante en modo escritura) es menor y
se resuelve bien en la funcion de creacion:

```c
static const FileConfig FILE_CONFIG_DEFAULT = {
    .filename     = NULL,
    .mode         = "r",
    .buffer_size  = 4096,
    .sync_writes  = false,
    .permissions  = 0644,
};

File *file_open(const FileConfig *cfg, FileError *err) {
    if (cfg->filename == NULL) {
        *err = FILE_ERR_NO_FILENAME;
        return NULL;
    }

    /* Restriccion cruzada: ignorar permissions en lectura */
    int perms = 0;
    if (strchr(cfg->mode, 'w') || strchr(cfg->mode, 'a')) {
        perms = cfg->permissions;
    }

    /* sync_writes solo tiene sentido en escritura */
    if (cfg->sync_writes && !strchr(cfg->mode, 'w')) {
        *err = FILE_ERR_SYNC_READ;
        return NULL;
    }

    // ... abrir archivo ...
}
```

En C, el patron config struct + build function es casi siempre
suficiente. Solo se justifica algo mas complejo si la construccion
es incremental (como `curl_easy_setopt` con decenas de opciones
que interactuan).

</details>

---

### Ejercicio 7 — #[non_exhaustive] vs builder

Una libreria publica expone:

```rust
#[derive(Default)]
pub struct RenderConfig {
    pub width: u32,
    pub height: u32,
    pub antialiasing: bool,
}
```

En v2 necesitas agregar `pub dpi: f32`.

**Prediccion**: ¿que pasa sin `#[non_exhaustive]`? ¿Con
`#[non_exhaustive]`? ¿Con builder? ¿Cual es mejor?

<details>
<summary>Respuesta</summary>

**Sin `#[non_exhaustive]`** — breaking change:
```rust
// Caller en v1:
let cfg = RenderConfig { width: 800, height: 600, antialiasing: true };
// En v2: ERROR — missing field `dpi`
```

**Con `#[non_exhaustive]`**:
```rust
#[non_exhaustive]
#[derive(Default)]
pub struct RenderConfig {
    pub width: u32,
    pub height: u32,
    pub antialiasing: bool,
    pub dpi: f32,  // v2
}

// Caller debe usar:
let cfg = RenderConfig {
    width: 800,
    height: 600,
    antialiasing: true,
    ..Default::default()  // Cubre campos futuros
};
// Funciona, pero caller NO puede pattern-match exhaustivamente
```

**Con builder**:
```rust
let cfg = RenderConfigBuilder::new()
    .width(800)
    .height(600)
    .antialiasing(true)
    .build();
// v2: .dpi(2.0) es opcional, default = 1.0
// Cero impacto en callers existentes
```

**Mejor opcion**: depende. Si no hay validacion cruzada,
`#[non_exhaustive]` + Default es mas simple. Si hay validacion
o la API es grande, builder es mas robusto. Muchos proyectos
usan ambos: `#[non_exhaustive]` como safety net + builder como
API primaria.

</details>

---

### Ejercicio 8 — Typestate vs builder: decision

Tienes un protocolo de conexion SSH:

```
  1. Especificar host (obligatorio)
  2. Especificar port (opcional, default 22)
  3. Autenticar: password O key (obligatorio, uno u otro)
  4. Abrir canal (solo despues de autenticar)
  5. Ejecutar comando (solo despues de abrir canal)
```

**Prediccion**: ¿builder normal con Result, o typestate? Justifica.

<details>
<summary>Respuesta</summary>

**Typestate**, porque hay un protocolo secuencial:
- No puedes abrir canal sin autenticarte
- No puedes ejecutar sin canal
- El orden importa (no es solo "setear campos")

```rust
// Estados del protocolo
struct Disconnected;
struct Connected;
struct Authenticated;
struct ChannelOpen;

struct SshSession<State> {
    host: String,
    port: u16,
    // ... internos ...
    _state: PhantomData<State>,
}

impl SshSession<Disconnected> {
    pub fn connect(host: &str, port: u16) -> Result<SshSession<Connected>, SshError> {
        // TCP connect
        todo!()
    }
}

impl SshSession<Connected> {
    pub fn auth_password(self, user: &str, pass: &str)
        -> Result<SshSession<Authenticated>, SshError> { todo!() }

    pub fn auth_key(self, user: &str, key_path: &str)
        -> Result<SshSession<Authenticated>, SshError> { todo!() }
}

impl SshSession<Authenticated> {
    pub fn open_channel(self)
        -> Result<SshSession<ChannelOpen>, SshError> { todo!() }
}

impl SshSession<ChannelOpen> {
    pub fn exec(self, cmd: &str) -> Result<Output, SshError> { todo!() }
}
```

Un builder normal permitiria `.exec()` antes de `.auth()` — el
error seria en runtime. Typestate lo hace imposible en compile time.

**Cuando builder normal basta**: si los campos son independientes
(como ServerConfig). **Cuando typestate**: si hay protocolo (pasos
que deben ocurrir en orden). SSH es protocolo → typestate.

</details>

---

### Ejercicio 9 — Refactorizar a hibrido

Este codigo tiene un builder que seria mejor como hibrido:

```rust
let cache = CacheBuilder::new()
    .max_size(1024)
    .ttl_seconds(300)
    .eviction_policy(EvictionPolicy::Lru)
    .shard_count(16)
    .build()?;
```

El problema: estos valores vienen de un archivo de configuracion,
pero el builder no acepta un config struct.

**Prediccion**: refactoriza para que el builder acepte un
`CacheConfig` como base.

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct CacheConfig {
    pub max_size: usize,
    pub ttl_seconds: u64,
    pub eviction_policy: EvictionPolicy,
    pub shard_count: usize,
}

pub struct CacheBuilder {
    max_size: usize,
    ttl_seconds: u64,
    eviction_policy: EvictionPolicy,
    shard_count: usize,
}

impl CacheBuilder {
    /// Desde cero con defaults razonables
    pub fn new() -> Self {
        Self::from_config(CacheConfig::default())
    }

    /// Desde un config existente (archivo, env, etc.)
    pub fn from_config(cfg: CacheConfig) -> Self {
        Self {
            max_size: cfg.max_size,
            ttl_seconds: cfg.ttl_seconds,
            eviction_policy: cfg.eviction_policy,
            shard_count: cfg.shard_count,
        }
    }

    pub fn max_size(mut self, size: usize) -> Self {
        self.max_size = size;
        self
    }

    pub fn ttl_seconds(mut self, ttl: u64) -> Self {
        self.ttl_seconds = ttl;
        self
    }

    // ... otros setters ...

    pub fn build(self) -> Result<Cache, CacheError> {
        if self.max_size == 0 {
            return Err(CacheError::ZeroSize);
        }
        if self.shard_count == 0 || !self.shard_count.is_power_of_two() {
            return Err(CacheError::InvalidShards);
        }
        Ok(Cache::new(self.max_size, self.ttl_seconds,
                       self.eviction_policy, self.shard_count))
    }
}

// Uso tipico: config de archivo + override
let base: CacheConfig = toml::from_str(&fs::read_to_string("cache.toml")?)?;
let cache = CacheBuilder::from_config(base)
    .max_size(2048)  // Override para este entorno
    .build()?;
```

El patron `from_config` + setters permite:
- Configuracion declarativa (archivo)
- Overrides programaticos (codigo)
- Validacion centralizada en `build()`

</details>

---

### Ejercicio 10 — Analisis de codigo real

Analiza este fragmento de `tokio`:

```rust
// tokio::runtime::Builder
let rt = tokio::runtime::Builder::new_multi_thread()
    .worker_threads(4)
    .thread_name("my-worker")
    .thread_stack_size(3 * 1024 * 1024)
    .on_thread_start(|| println!("thread started"))
    .enable_all()
    .build()
    .unwrap();
```

**Prediccion**: ¿por que tokio usa builder y no config struct?
Identifica al menos 3 razones de las que vimos en este topico.

<details>
<summary>Respuesta</summary>

Razones por las que tokio usa builder:

**1. Validacion cruzada**:
- `worker_threads` solo tiene sentido con `new_multi_thread()`,
  no con `new_current_thread()`
- `thread_stack_size` tiene minimo del OS
- `enable_io()` + `enable_time()` vs `enable_all()` interactuan

**2. Producto inmutable**:
- Una vez construido, el Runtime no permite cambiar la cantidad
  de workers ni el stack size. El builder produce un producto sellado.

**3. API publica de libreria**:
- Tokio es una de las librerias mas usadas. Agregar opciones
  (como `on_thread_park`, `on_thread_unpark`) no rompe callers
  existentes gracias al builder.

**4. Logica en asignacion**:
- `on_thread_start` toma un closure — no es un valor plano que
  se pueda meter en un config struct serializable
- `enable_all()` ejecuta logica (activa todos los drivers)

**5. Diferentes constructores**:
- `new_multi_thread()` vs `new_current_thread()` configuran
  estado interno diferente. Un config struct necesitaria un
  `RuntimeKind` enum, pero entonces ¿que pasa si seteas
  `worker_threads` en `CurrentThread`?

El builder resuelve esto con el tipo de retorno:
ambos retornan `Builder`, pero `build()` valida la coherencia.

Un config struct requeriria `RuntimeConfig` + `RuntimeKind` +
validacion manual — mas complejo y error-prone que el builder.

</details>
