# T02 — Builder en Rust

---

## 1. De C a Rust: method chaining natural

T01 mostro que en C el builder idiomatico es un config struct con
designated initializers. El method chaining era posible pero feo.
En Rust, el chaining es la forma natural:

```rust
// C (T01): config struct
// ServerConfig cfg = SERVER_CONFIG_DEFAULT;
// cfg.port = 443;
// cfg.use_tls = true;
// Server *s = server_build(&cfg);

// Rust: method chaining con consume-and-return
let server = ServerBuilder::new()
    .port(443)
    .tls("/cert.pem", "/key.pem")
    .workers(8)
    .build()?;
```

La diferencia no es solo estetica. Rust puede:
- **Consumir** el builder en `build()` (imposible reutilizar — previene bugs)
- **Retornar Result** con errores tipados
- **Garantizar** en compile time que campos obligatorios estan seteados (T03)
- **Drop** automatico si el builder se descarta sin llamar build

---

## 2. Patron basico: consume-and-return

### 2.1 El builder struct

```rust
struct ServerBuilder {
    host: String,
    port: u16,
    max_connections: usize,
    timeout_ms: u64,
    use_tls: bool,
    cert_path: Option<String>,
    key_path: Option<String>,
    log_level: LogLevel,
    log_file: Option<String>,
    num_workers: usize,
}

#[derive(Debug, Clone, Copy)]
enum LogLevel {
    None, Error, Warn, Info, Debug,
}
```

### 2.2 new() con defaults

```rust
impl ServerBuilder {
    fn new() -> Self {
        ServerBuilder {
            host: "0.0.0.0".into(),
            port: 8080,
            max_connections: 1024,
            timeout_ms: 30_000,
            use_tls: false,
            cert_path: None,
            key_path: None,
            log_level: LogLevel::Info,
            log_file: None,
            num_workers: 0,  // auto
        }
    }
}
```

### 2.3 Setters que consumen self

```rust
impl ServerBuilder {
    fn host(mut self, host: &str) -> Self {
        self.host = host.into();
        self
    }

    fn port(mut self, port: u16) -> Self {
        self.port = port;
        self
    }

    fn max_connections(mut self, n: usize) -> Self {
        self.max_connections = n;
        self
    }

    fn timeout(mut self, ms: u64) -> Self {
        self.timeout_ms = ms;
        self
    }

    fn tls(mut self, cert: &str, key: &str) -> Self {
        self.use_tls = true;
        self.cert_path = Some(cert.into());
        self.key_path = Some(key.into());
        self
    }

    fn log_level(mut self, level: LogLevel) -> Self {
        self.log_level = level;
        self
    }

    fn log_file(mut self, path: &str) -> Self {
        self.log_file = Some(path.into());
        self
    }

    fn workers(mut self, n: usize) -> Self {
        self.num_workers = n;
        self
    }
}
```

### 2.4 build() que consume el builder

```rust
#[derive(Debug)]
struct Server {
    host: String,
    port: u16,
    max_connections: usize,
    timeout_ms: u64,
    use_tls: bool,
    cert_path: Option<String>,
    key_path: Option<String>,
    log_level: LogLevel,
    log_file: Option<String>,
    num_workers: usize,
}

#[derive(Debug)]
enum BuildError {
    TlsMissingCert,
    TlsMissingKey,
    InvalidPort,
    InvalidWorkers(usize),
}

impl std::fmt::Display for BuildError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BuildError::TlsMissingCert =>
                write!(f, "TLS enabled but no cert_path"),
            BuildError::TlsMissingKey =>
                write!(f, "TLS enabled but no key_path"),
            BuildError::InvalidPort =>
                write!(f, "port must be > 0"),
            BuildError::InvalidWorkers(n) =>
                write!(f, "workers must be 0-256, got {n}"),
        }
    }
}

impl std::error::Error for BuildError {}

impl ServerBuilder {
    fn build(self) -> Result<Server, BuildError> {
        // Validacion
        if self.port == 0 {
            return Err(BuildError::InvalidPort);
        }
        if self.use_tls && self.cert_path.is_none() {
            return Err(BuildError::TlsMissingCert);
        }
        if self.use_tls && self.key_path.is_none() {
            return Err(BuildError::TlsMissingKey);
        }
        if self.num_workers > 256 {
            return Err(BuildError::InvalidWorkers(self.num_workers));
        }

        let num_workers = if self.num_workers == 0 {
            num_cpus()  // auto-detect
        } else {
            self.num_workers
        };

        Ok(Server {
            host: self.host,
            port: self.port,
            max_connections: self.max_connections,
            timeout_ms: self.timeout_ms,
            use_tls: self.use_tls,
            cert_path: self.cert_path,
            key_path: self.key_path,
            log_level: self.log_level,
            log_file: self.log_file,
            num_workers,
        })
    }
}

fn num_cpus() -> usize { 4 }  // simplificado
```

### 2.5 Uso

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Minimal
    let s1 = ServerBuilder::new().build()?;

    // Produccion
    let s2 = ServerBuilder::new()
        .port(443)
        .tls("/etc/cert.pem", "/etc/key.pem")
        .max_connections(10_000)
        .workers(8)
        .log_level(LogLevel::Warn)
        .log_file("/var/log/server.log")
        .build()?;

    // Desarrollo
    let s3 = ServerBuilder::new()
        .port(3000)
        .log_level(LogLevel::Debug)
        .build()?;

    // Error: TLS sin cert
    let err = ServerBuilder::new()
        .port(443)
        .build();
    // Ok — no TLS, no cert needed

    // Error real:
    let mut builder = ServerBuilder::new();
    builder.use_tls = true;  // campo publico en este ejemplo
    // builder.build() → Err(TlsMissingCert)

    Ok(())
}
```

---

## 3. Consume self vs &mut self

Hay dos estilos de builder en Rust:

### 3.1 Consume self (move semantics)

```rust
fn port(mut self, port: u16) -> Self {
    self.port = port;
    self
}

fn build(self) -> Result<Server, BuildError> { ... }
```

```
  Ventajas:
  - Despues de build(), el builder NO EXISTE (moved)
  - Imposible reutilizar un builder consumido
  - Cada setter retorna ownership → chaining natural

  Desventajas:
  - No puedes reutilizar el builder para multiples objetos
  - Cada setter mueve el builder (puede ser confuso)
```

```rust
let s = ServerBuilder::new()
    .port(443)
    .build()?;

// ServerBuilder fue consumido por build() — no existe mas
```

### 3.2 &mut self (borrow)

```rust
fn port(&mut self, port: u16) -> &mut Self {
    self.port = port;
    self
}

fn build(&self) -> Result<Server, BuildError> { ... }
```

```
  Ventajas:
  - El builder es reutilizable (no se consume)
  - Puede crear multiples objetos del mismo builder

  Desventajas:
  - No previene usar builder despues de build (puede ser bug)
  - El chaining requiere una variable para el builder
```

```rust
let mut b = ServerBuilder::new();
b.port(443).workers(8);

let s1 = b.build()?;  // builder sigue vivo

b.port(9090);
let s2 = b.build()?;  // segundo objeto, distinto port
```

### 3.3 Comparacion

```
  Consume self:          &mut self:
  ─────────────          ───────────
  Builder::new()         let mut b = Builder::new();
    .port(443)           b.port(443);
    .build()             b.build()

  Builder desaparece     Builder reutilizable
  Previene mal uso       Permite templates
  Idiomatico para        Idiomatico para
  objetos unicos         objetos repetidos

  Usa consume self cuando: el builder es de un solo uso
  Usa &mut self cuando: necesitas crear variantes similares
```

La mayoria de builders en el ecosistema Rust usan **consume self**
(Command, reqwest::RequestBuilder, etc.).

---

## 4. Builder con Default trait

```rust
#[derive(Debug)]
struct ServerBuilder {
    host: String,
    port: u16,
    max_connections: usize,
    timeout_ms: u64,
    use_tls: bool,
    cert_path: Option<String>,
    key_path: Option<String>,
    log_level: LogLevel,
    log_file: Option<String>,
    num_workers: usize,
}

impl Default for ServerBuilder {
    fn default() -> Self {
        ServerBuilder {
            host: "0.0.0.0".into(),
            port: 8080,
            max_connections: 1024,
            timeout_ms: 30_000,
            use_tls: false,
            cert_path: None,
            key_path: None,
            log_level: LogLevel::Info,
            log_file: None,
            num_workers: 0,
        }
    }
}

impl ServerBuilder {
    fn new() -> Self {
        Self::default()
    }
}
```

Con `Default`, tambien puedes usar struct update syntax:

```rust
let builder = ServerBuilder {
    port: 443,
    use_tls: true,
    cert_path: Some("/cert.pem".into()),
    key_path: Some("/key.pem".into()),
    ..ServerBuilder::default()
};
```

Esto es el equivalente Rust de designated initializers en C:

```
  C (T01):                              Rust:
  ──────                                ─────
  ServerConfig cfg = DEFAULT;           let b = ServerBuilder {
  cfg.port = 443;                           port: 443,
  cfg.use_tls = true;                      use_tls: true,
                                            ..Default::default()
                                        };
```

---

## 5. Builder separado del producto

Un patron comun: el builder y el producto son structs distintos.
El builder tiene campos `Option<T>` para rastrear que se seteo;
el producto tiene campos no-opcionales.

```rust
// El PRODUCTO: todos los campos son concretos
#[derive(Debug)]
struct DatabaseConfig {
    host: String,
    port: u16,
    name: String,
    user: String,
    password: String,
    pool_size: usize,
    timeout_ms: u64,
    ssl: bool,
}

// El BUILDER: campos opcionales para rastrear estado
#[derive(Debug, Default)]
struct DatabaseConfigBuilder {
    host: Option<String>,
    port: Option<u16>,
    name: Option<String>,
    user: Option<String>,
    password: Option<String>,
    pool_size: Option<usize>,
    timeout_ms: Option<u64>,
    ssl: Option<bool>,
}

impl DatabaseConfigBuilder {
    fn new() -> Self { Self::default() }

    fn host(mut self, host: &str) -> Self {
        self.host = Some(host.into());
        self
    }

    fn port(mut self, port: u16) -> Self {
        self.port = Some(port);
        self
    }

    fn name(mut self, name: &str) -> Self {
        self.name = Some(name.into());
        self
    }

    fn user(mut self, user: &str) -> Self {
        self.user = Some(user.into());
        self
    }

    fn password(mut self, password: &str) -> Self {
        self.password = Some(password.into());
        self
    }

    fn pool_size(mut self, n: usize) -> Self {
        self.pool_size = Some(n);
        self
    }

    fn timeout(mut self, ms: u64) -> Self {
        self.timeout_ms = Some(ms);
        self
    }

    fn ssl(mut self, enabled: bool) -> Self {
        self.ssl = Some(enabled);
        self
    }

    fn build(self) -> Result<DatabaseConfig, String> {
        Ok(DatabaseConfig {
            host: self.host.unwrap_or_else(|| "localhost".into()),
            port: self.port.unwrap_or(5432),
            name: self.name.ok_or("database name is required")?,
            user: self.user.ok_or("user is required")?,
            password: self.password.ok_or("password is required")?,
            pool_size: self.pool_size.unwrap_or(10),
            timeout_ms: self.timeout_ms.unwrap_or(5000),
            ssl: self.ssl.unwrap_or(false),
        })
    }
}
```

```rust
// Campos obligatorios: name, user, password (sin default)
// Campos opcionales: host, port, pool_size, timeout, ssl (con default)

let cfg = DatabaseConfigBuilder::new()
    .name("myapp")
    .user("admin")
    .password("secret")
    .pool_size(20)
    .build()?;

// Error: falta user
let err = DatabaseConfigBuilder::new()
    .name("myapp")
    .password("secret")
    .build();
// Err("user is required")
```

```
  Builder con Option<T>:
  ┌─────────────────────────┐
  │ host: Option<String>    │  → unwrap_or("localhost")
  │ port: Option<u16>       │  → unwrap_or(5432)
  │ name: Option<String>    │  → ok_or("required")? ← OBLIGATORIO
  │ user: Option<String>    │  → ok_or("required")? ← OBLIGATORIO
  │ password: Option<String>│  → ok_or("required")? ← OBLIGATORIO
  │ pool_size: Option<usize>│  → unwrap_or(10)
  └─────────────────────────┘

  En build():
  - unwrap_or(default) → campo opcional con default
  - ok_or("error")?    → campo obligatorio, error si falta
```

---

## 6. Builder en la stdlib y el ecosistema

### 6.1 std::process::Command

```rust
use std::process::Command;

let output = Command::new("ls")
    .arg("-la")
    .arg("/tmp")
    .env("LANG", "C")
    .current_dir("/")
    .output()?;
```

`Command` usa &mut self (reutilizable):

```rust
let mut cmd = Command::new("gcc");
cmd.arg("-O2").arg("-Wall");

// Reutilizar para compilar distintos archivos
cmd.arg("main.c");
let r1 = cmd.output()?;

// Ojo: "main.c" sigue en los args
// Command es acumulativo, no limpia entre usos
```

### 6.2 reqwest (HTTP client)

```rust
let response = reqwest::Client::new()
    .get("https://api.example.com/users")
    .header("Authorization", "Bearer token")
    .timeout(std::time::Duration::from_secs(10))
    .send()
    .await?;
```

### 6.3 tokio::runtime::Builder

```rust
let runtime = tokio::runtime::Builder::new_multi_thread()
    .worker_threads(4)
    .enable_io()
    .enable_time()
    .build()?;
```

### 6.4 Patron comun

```
  Todos siguen el mismo patron:

  Objeto::builder()     ← o ::new() que retorna builder
      .campo1(valor)    ← setters que retornan Self o &mut Self
      .campo2(valor)
      .build()          ← consume y retorna Result<T, E>
          │
          ▼
  Producto inmutable (no tiene setters)
```

---

## 7. Builder con validacion rica

```rust
#[derive(Debug)]
enum ConfigError {
    MissingField(&'static str),
    InvalidValue { field: &'static str, message: String },
    IncompatibleOptions { a: &'static str, b: &'static str, reason: String },
}

impl std::fmt::Display for ConfigError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigError::MissingField(name) =>
                write!(f, "missing required field: {name}"),
            ConfigError::InvalidValue { field, message } =>
                write!(f, "invalid {field}: {message}"),
            ConfigError::IncompatibleOptions { a, b, reason } =>
                write!(f, "{a} and {b} are incompatible: {reason}"),
        }
    }
}

impl std::error::Error for ConfigError {}

impl ServerBuilder {
    fn build(self) -> Result<Server, ConfigError> {
        // Validacion de campos individuales
        if self.port == 0 {
            return Err(ConfigError::InvalidValue {
                field: "port",
                message: "must be > 0".into(),
            });
        }

        if self.num_workers > 256 {
            return Err(ConfigError::InvalidValue {
                field: "workers",
                message: format!("must be 0-256, got {}", self.num_workers),
            });
        }

        // Validacion de combinaciones
        if self.use_tls && self.cert_path.is_none() {
            return Err(ConfigError::MissingField("cert_path (required when TLS enabled)"));
        }

        if self.use_tls && self.key_path.is_none() {
            return Err(ConfigError::MissingField("key_path (required when TLS enabled)"));
        }

        // Transformaciones
        let num_workers = if self.num_workers == 0 {
            num_cpus()
        } else {
            self.num_workers
        };

        Ok(Server {
            host: self.host,
            port: self.port,
            max_connections: self.max_connections,
            timeout_ms: self.timeout_ms,
            use_tls: self.use_tls,
            cert_path: self.cert_path,
            key_path: self.key_path,
            log_level: self.log_level,
            log_file: self.log_file,
            num_workers,
        })
    }
}
```

```
  Comparacion C vs Rust de error handling:

  C:    if (!s) { /* pero cual fue el error? */ }
        NULL no dice nada. Necesitas error code separado.

  Rust: match result {
            Err(ConfigError::MissingField(f)) => ...,
            Err(ConfigError::InvalidValue { field, message }) => ...,
            Err(ConfigError::IncompatibleOptions { .. }) => ...,
            Ok(server) => ...,
        }
        El error dice EXACTAMENTE que fallo y por que.
```

---

## 8. Builder con conditional methods

Metodos que setean un valor solo si se cumple una condicion:

```rust
impl ServerBuilder {
    /// Setear solo si la condicion es true
    fn port_if(self, condition: bool, port: u16) -> Self {
        if condition { self.port(port) } else { self }
    }

    /// Setear desde un Option (si es Some)
    fn maybe_port(self, port: Option<u16>) -> Self {
        match port {
            Some(p) => self.port(p),
            None => self,
        }
    }

    /// Aplicar una closure de configuracion
    fn configure(self, f: impl FnOnce(Self) -> Self) -> Self {
        f(self)
    }
}
```

```rust
// Uso con condiciones
let is_prod = std::env::var("ENV").as_deref() == Ok("production");

let server = ServerBuilder::new()
    .port_if(is_prod, 443)
    .maybe_port(env_port())
    .configure(|b| {
        if is_prod {
            b.tls("/cert.pem", "/key.pem")
             .workers(8)
             .log_level(LogLevel::Warn)
        } else {
            b.log_level(LogLevel::Debug)
        }
    })
    .build()?;

fn env_port() -> Option<u16> {
    std::env::var("PORT").ok()?.parse().ok()
}
```

```
  configure() es un "builder de builders":
  Pasa el builder a una closure que puede hacer
  N operaciones condicionalmente y retornarlo.

  Esto es imposible con config struct en C:
  no puedes pasar un bloque de logica como parametro.
```

---

## 9. Derive macro: #[derive(Builder)]

El boilerplate de un builder es mecanico: para cada campo, un
setter que toma self y retorna self. El crate `derive_builder`
lo genera automaticamente:

```rust
// Cargo.toml: derive_builder = "0.20"
use derive_builder::Builder;

#[derive(Builder, Debug)]
#[builder(setter(into))]
struct Server {
    #[builder(default = r#""0.0.0.0".to_string()"#)]
    host: String,

    #[builder(default = "8080")]
    port: u16,

    #[builder(default = "1024")]
    max_connections: usize,

    #[builder(default = "30_000")]
    timeout_ms: u64,

    #[builder(default = "false")]
    use_tls: bool,

    #[builder(default)]
    cert_path: Option<String>,

    #[builder(default)]
    key_path: Option<String>,

    #[builder(default = "LogLevel::Info")]
    log_level: LogLevel,

    #[builder(default)]
    log_file: Option<String>,

    #[builder(default = "0")]
    num_workers: usize,
}
```

```rust
// El macro genera ServerBuilder automaticamente:
let server = ServerBuilder::default()
    .port(443u16)
    .use_tls(true)
    .cert_path(Some("/cert.pem".to_string()))
    .build()?;

// El builder usa &mut self por defecto (reutilizable)
```

```
  A mano: ~60 lineas de builder (struct + Default + setters + build)
  derive: ~20 lineas de anotaciones

  Pro: menos boilerplate, menos bugs, consistente
  Con: magia (el builder no es visible en el codigo fuente),
       mensajes de error pueden ser confusos,
       menos control sobre validacion custom
```

### 9.1 Validacion con derive_builder

```rust
#[derive(Builder, Debug)]
#[builder(build_fn(validate = "Self::validate"))]
struct Server {
    // ... campos ...
}

impl ServerBuilder {
    fn validate(&self) -> Result<(), String> {
        if self.use_tls == Some(true) && self.cert_path.is_none() {
            return Err("TLS requires cert_path".into());
        }
        Ok(())
    }
}
```

---

## 10. Comparacion completa: C vs Rust

```
  Aspecto               C (T01)                  Rust (T02)
  ───────               ───────                  ──────────
  Forma idiomatica      Config struct            Method chaining
  Defaults              Macro / funcion          Default trait / new()
  Setters               cfg.field = val          .field(val) → Self
  Chaining              Feo (nested calls)       Natural (. syntax)
  Build                 server_build(&cfg)       builder.build()
  Errores               NULL o error code        Result<T, BuildError>
  Consumo               cfg sigue viva           builder moved (o &mut)
  Reuso                 Siempre (struct vive)    Solo si &mut self
  Campos obligatorios   Runtime (NULL check)     Runtime (Option) o
                                                  compile time (T03)
  Inmutabilidad         Convencion / opaque      Default (sin mut)
  Generacion            X-macros                 #[derive(Builder)]
  Conditional           if/else antes de build   .configure(|b| ...)
  Ownership             Manual (strdup)          Move o Clone
  Destruccion           Manual                   Drop automatico
```

```
  Lo que Rust agrega sobre C:
  1. Move semantics: builder consumido no puede reutilizarse (previene bugs)
  2. Result<T, E>: errores ricos, forzados por compilador
  3. Closures: conditional building (.configure())
  4. derive macros: genera builder automaticamente
  5. Typestate (T03): campos obligatorios verificados en compile time
```

---

## Errores comunes

### Error 1 — Usar builder despues de build (consume self)

```rust
let builder = ServerBuilder::new().port(443);
let s1 = builder.build()?;
let s2 = builder.build()?;
//       ^^^^^^^ error: use of moved value: `builder`
```

`build(self)` consume el builder. Para reutilizar: usa `&self`
o clona el builder antes de build.

```rust
let builder = ServerBuilder::new().port(443);
let s1 = builder.clone().build()?;  // clona antes de consumir
let s2 = builder.build()?;          // ahora si consume
```

### Error 2 — Olvidar llamar build

```rust
fn setup() -> Result<Server, BuildError> {
    ServerBuilder::new()
        .port(443)
        .tls("/cert.pem", "/key.pem");
    // BUG: el builder se descarta sin llamar build()
    // Rust no avisa (el builder simplemente se dropea)

    // Falta: .build()?
    todo!()
}
```

Rust no puede detectar que olvidaste `build()`. Un lint custom
o `#[must_use]` en el builder puede ayudar:

```rust
#[must_use = "builder does nothing unless .build() is called"]
struct ServerBuilder { ... }
```

### Error 3 — Setter que no retorna self

```rust
impl ServerBuilder {
    fn port(mut self, port: u16) {  // BUG: no retorna Self
        self.port = port;
        // self se dropea aqui
    }
}

// No compila si intentas chainear:
// ServerBuilder::new().port(443).build()
//                     ^^^^^^^^^ method not found on `()`
```

Todo setter debe retornar `Self` (o `&mut Self`) para permitir
chaining.

### Error 4 — Confundir builder con producto

```rust
// El builder NO es el producto
let builder = ServerBuilder::new().port(443);
// builder.start();  // ERROR: ServerBuilder no tiene start()

// Necesitas build() primero
let server = builder.build()?;
server.start();  // OK: Server tiene start()
```

---

## Ejercicios

### Ejercicio 1 — Builder basico

Implementa un builder para `HttpRequest` con estos campos:

- `method` (GET/POST/PUT/DELETE) — obligatorio
- `url` (String) — obligatorio
- `headers` (Vec de pares) — opcional, acumulativo
- `body` (Option<String>) — opcional
- `timeout_ms` (u64) — default 30000

Usa consume-self pattern.

**Prediccion**: Como manejas los campos obligatorios (method, url)
si el builder empieza vacio?

<details><summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy)]
enum Method { Get, Post, Put, Delete }

#[derive(Debug)]
struct HttpRequest {
    method: Method,
    url: String,
    headers: Vec<(String, String)>,
    body: Option<String>,
    timeout_ms: u64,
}

struct HttpRequestBuilder {
    method: Method,
    url: String,
    headers: Vec<(String, String)>,
    body: Option<String>,
    timeout_ms: u64,
}

impl HttpRequestBuilder {
    // method y url son obligatorios → se pasan en new()
    fn new(method: Method, url: &str) -> Self {
        HttpRequestBuilder {
            method,
            url: url.to_string(),
            headers: Vec::new(),
            body: None,
            timeout_ms: 30_000,
        }
    }

    fn header(mut self, key: &str, value: &str) -> Self {
        self.headers.push((key.into(), value.into()));
        self
    }

    fn body(mut self, body: &str) -> Self {
        self.body = Some(body.into());
        self
    }

    fn timeout(mut self, ms: u64) -> Self {
        self.timeout_ms = ms;
        self
    }

    fn build(self) -> Result<HttpRequest, String> {
        if self.url.is_empty() {
            return Err("url cannot be empty".into());
        }
        if matches!(self.method, Method::Post | Method::Put)
            && self.body.is_none()
        {
            return Err("POST/PUT require a body".into());
        }
        Ok(HttpRequest {
            method: self.method,
            url: self.url,
            headers: self.headers,
            body: self.body,
            timeout_ms: self.timeout_ms,
        })
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let req = HttpRequestBuilder::new(Method::Post, "https://api.example.com")
        .header("Content-Type", "application/json")
        .header("Authorization", "Bearer token")
        .body(r#"{"name": "Alice"}"#)
        .timeout(5000)
        .build()?;

    println!("{:?}", req);
    Ok(())
}
```

Los campos obligatorios (method, url) se pasan como parametros
de `new()`. Asi es imposible crear un builder sin ellos. Los
opcionales se setean con metodos de chaining.

</details>

---

### Ejercicio 2 — Builder &mut self (reutilizable)

Reescribe el HttpRequestBuilder usando `&mut self` para crear
multiples requests similares (ej. mismas headers, distinto URL).

**Prediccion**: Como evitas que los headers se acumulen entre
builds?

<details><summary>Respuesta</summary>

```rust
struct HttpRequestBuilder {
    method: Method,
    url: String,
    base_headers: Vec<(String, String)>,
    extra_headers: Vec<(String, String)>,
    body: Option<String>,
    timeout_ms: u64,
}

impl HttpRequestBuilder {
    fn new(method: Method, url: &str) -> Self {
        HttpRequestBuilder {
            method,
            url: url.to_string(),
            base_headers: Vec::new(),
            extra_headers: Vec::new(),
            body: None,
            timeout_ms: 30_000,
        }
    }

    fn base_header(&mut self, key: &str, value: &str) -> &mut Self {
        self.base_headers.push((key.into(), value.into()));
        self
    }

    fn header(&mut self, key: &str, value: &str) -> &mut Self {
        self.extra_headers.push((key.into(), value.into()));
        self
    }

    fn url(&mut self, url: &str) -> &mut Self {
        self.url = url.into();
        self
    }

    fn body(&mut self, body: &str) -> &mut Self {
        self.body = Some(body.into());
        self
    }

    fn build(&mut self) -> Result<HttpRequest, String> {
        let mut all_headers = self.base_headers.clone();
        all_headers.extend(self.extra_headers.drain(..));
        // drain: toma extra_headers y los vacia para el proximo build

        let req = HttpRequest {
            method: self.method,
            url: self.url.clone(),
            headers: all_headers,
            body: self.body.take(),  // take: mueve body, deja None
            timeout_ms: self.timeout_ms,
        };
        Ok(req)
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut b = HttpRequestBuilder::new(Method::Get, "https://api.example.com");
    b.base_header("Authorization", "Bearer token");

    // Request 1
    b.url("https://api.example.com/users");
    let r1 = b.build()?;

    // Request 2 (reutiliza base_headers, sin extra)
    b.url("https://api.example.com/posts");
    let r2 = b.build()?;

    // Ambas tienen Authorization header
    println!("{:?}", r1.headers);
    println!("{:?}", r2.headers);
    Ok(())
}
```

Para evitar acumulacion: separar `base_headers` (persistentes)
de `extra_headers` (se vacian con `drain` en cada build). Asi
los headers base se comparten entre requests y los extras son
per-request.

`body.take()` mueve el body al request y deja `None` en el
builder — el siguiente build no tendra body a menos que lo setees.

</details>

---

### Ejercicio 3 — Builder con acumuladores

Implementa un builder para `Query` donde `where_clause` y
`order_by` son acumulativos (cada llamada agrega, no reemplaza):

```rust
let q = QueryBuilder::new("users")
    .select("name")
    .select("age")
    .where_clause("age > 18")
    .where_clause("active = true")
    .order_by("name", Asc)
    .limit(10)
    .build();
// SELECT name, age FROM users WHERE age > 18 AND active = true
// ORDER BY name ASC LIMIT 10
```

**Prediccion**: `select` es acumulativo — como manejas el default?

<details><summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy)]
enum SortOrder { Asc, Desc }

#[derive(Debug)]
struct Query {
    sql: String,
}

struct QueryBuilder {
    table: String,
    columns: Vec<String>,
    conditions: Vec<String>,
    order: Vec<(String, SortOrder)>,
    limit: Option<usize>,
}

impl QueryBuilder {
    fn new(table: &str) -> Self {
        QueryBuilder {
            table: table.into(),
            columns: Vec::new(),
            conditions: Vec::new(),
            order: Vec::new(),
            limit: None,
        }
    }

    fn select(mut self, column: &str) -> Self {
        self.columns.push(column.into());
        self
    }

    fn where_clause(mut self, condition: &str) -> Self {
        self.conditions.push(condition.into());
        self
    }

    fn order_by(mut self, column: &str, dir: SortOrder) -> Self {
        self.order.push((column.into(), dir));
        self
    }

    fn limit(mut self, n: usize) -> Self {
        self.limit = Some(n);
        self
    }

    fn build(self) -> Query {
        let columns = if self.columns.is_empty() {
            "*".to_string()
        } else {
            self.columns.join(", ")
        };

        let mut sql = format!("SELECT {} FROM {}", columns, self.table);

        if !self.conditions.is_empty() {
            sql.push_str(" WHERE ");
            sql.push_str(&self.conditions.join(" AND "));
        }

        if !self.order.is_empty() {
            sql.push_str(" ORDER BY ");
            let parts: Vec<String> = self.order.iter()
                .map(|(col, dir)| {
                    format!("{} {}", col,
                            match dir { SortOrder::Asc => "ASC",
                                        SortOrder::Desc => "DESC" })
                })
                .collect();
            sql.push_str(&parts.join(", "));
        }

        if let Some(n) = self.limit {
            sql.push_str(&format!(" LIMIT {}", n));
        }

        Query { sql }
    }
}

fn main() {
    let q = QueryBuilder::new("users")
        .select("name")
        .select("age")
        .where_clause("age > 18")
        .where_clause("active = true")
        .order_by("name", SortOrder::Asc)
        .limit(10)
        .build();

    println!("{}", q.sql);
    // SELECT name, age FROM users WHERE age > 18 AND active = true
    // ORDER BY name ASC LIMIT 10
}
```

Default de `select`: si `columns` esta vacio al build, se usa
`*` (SELECT *). Asi `QueryBuilder::new("users").build()` produce
`SELECT * FROM users`. Cada `.select()` agrega al Vec.

</details>

---

### Ejercicio 4 — Builder con configure()

Agrega un metodo `configure` al ServerBuilder que tome un closure:

```rust
fn configure(self, f: impl FnOnce(Self) -> Self) -> Self
```

Usalo para aplicar configuracion condicional (produccion vs
desarrollo) sin romper el chaining.

**Prediccion**: Por que FnOnce y no Fn?

<details><summary>Respuesta</summary>

```rust
impl ServerBuilder {
    fn configure(self, f: impl FnOnce(Self) -> Self) -> Self {
        f(self)
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let is_prod = std::env::var("ENV").as_deref() == Ok("production");

    let server = ServerBuilder::new()
        .configure(|b| {
            if is_prod {
                b.port(443)
                 .tls("/etc/cert.pem", "/etc/key.pem")
                 .workers(8)
                 .log_level(LogLevel::Warn)
            } else {
                b.port(3000)
                 .log_level(LogLevel::Debug)
            }
        })
        .max_connections(500)  // despues de configure, sigue chaining
        .build()?;

    Ok(())
}
```

`FnOnce` porque el builder se consume (move) al entrar al
closure y se retorna. `Fn` requeriria `&self` (no puede mover).
`FnMut` requeriria `&mut self` (no es el patron consume-self).

Como el builder usa `fn port(mut self, ...) -> Self` (consume
self), el closure necesita ownership del builder para llamar
los setters. Solo `FnOnce` puede tomar ownership de sus
capturas y del argumento.

</details>

---

### Ejercicio 5 — Builder from environment

Implementa un metodo `from_env` que lea configuracion de
variables de entorno:

```rust
let server = ServerBuilder::new()
    .from_env()       // lee SERVER_PORT, SERVER_HOST, etc.
    .port(9090)       // override programatico (prioridad sobre env)
    .build()?;
```

**Prediccion**: from_env debe ir antes o despues de los setters
programaticos?

<details><summary>Respuesta</summary>

```rust
impl ServerBuilder {
    fn from_env(mut self) -> Self {
        if let Ok(host) = std::env::var("SERVER_HOST") {
            self.host = host;
        }

        if let Ok(port) = std::env::var("SERVER_PORT") {
            if let Ok(p) = port.parse::<u16>() {
                self.port = p;
            }
        }

        if let Ok(workers) = std::env::var("SERVER_WORKERS") {
            if let Ok(n) = workers.parse::<usize>() {
                self.num_workers = n;
            }
        }

        if let Ok(level) = std::env::var("SERVER_LOG_LEVEL") {
            self.log_level = match level.to_lowercase().as_str() {
                "none"  => LogLevel::None,
                "error" => LogLevel::Error,
                "warn"  => LogLevel::Warn,
                "info"  => LogLevel::Info,
                "debug" => LogLevel::Debug,
                _ => self.log_level,
            };
        }

        if let Ok(tls) = std::env::var("SERVER_TLS") {
            if tls == "true" || tls == "1" {
                self.use_tls = true;
            }
        }

        if let Ok(cert) = std::env::var("SERVER_CERT") {
            self.cert_path = Some(cert);
        }

        if let Ok(key) = std::env::var("SERVER_KEY") {
            self.key_path = Some(key);
        }

        self
    }
}
```

`from_env()` debe ir **antes** de los setters programaticos:

```rust
ServerBuilder::new()    // 1. defaults del codigo
    .from_env()         // 2. override desde env vars
    .port(9090)         // 3. override programatico (maxima prioridad)
    .build()?
```

Orden de prioridad: programatico > env > defaults. Si `from_env`
fuera despues de `.port(9090)`, el env var sobreescribiria el
port programatico — incorrecto.

Este patron de layered configuration es comun en aplicaciones
reales: defaults → config file → env vars → CLI args → code.

</details>

---

### Ejercicio 6 — #[must_use] y builder safety

Agrega `#[must_use]` al builder y al Result de build. Demuestra
que Rust avisa si:
1. Se crea un builder sin llamar build()
2. Se llama build() sin usar el Result

**Prediccion**: `#[must_use]` es un error o un warning?

<details><summary>Respuesta</summary>

```rust
#[must_use = "builder does nothing unless .build() is called"]
struct ServerBuilder { /* ... */ }

impl ServerBuilder {
    #[must_use = "build() returns Result that must be handled"]
    fn build(self) -> Result<Server, BuildError> { /* ... */ }
}

fn example1() {
    ServerBuilder::new()
        .port(443);
    // warning: unused `ServerBuilder` that must be used
    // = note: builder does nothing unless .build() is called
}

fn example2() {
    ServerBuilder::new()
        .port(443)
        .build();
    // warning: unused `Result` that must be used
    // = note: build() returns Result that must be handled
}

fn example3() -> Result<(), BuildError> {
    let _server = ServerBuilder::new()
        .port(443)
        .build()?;
    // OK: Result handled with ?, Server stored in _server
    Ok(())
}
```

`#[must_use]` produce un **warning**, no un error. Con
`#[deny(unused_must_use)]` se convierte en error.

Esto no existe en C: si olvidas llamar `server_build(&cfg)`, C
no avisa nada. Tampoco avisa si ignoras el NULL de retorno.

</details>

---

### Ejercicio 7 — Builder generico

Escribe un builder generico para `Pool<T>` que funcione con
cualquier tipo:

```rust
let pool: Pool<DatabaseConn> = PoolBuilder::new()
    .min_size(5)
    .max_size(20)
    .timeout(Duration::from_secs(30))
    .build()?;
```

**Prediccion**: Necesitas bounds en T para el builder?

<details><summary>Respuesta</summary>

```rust
use std::time::Duration;
use std::marker::PhantomData;

#[derive(Debug)]
struct Pool<T> {
    min_size: usize,
    max_size: usize,
    timeout: Duration,
    _phantom: PhantomData<T>,
}

struct PoolBuilder<T> {
    min_size: usize,
    max_size: usize,
    timeout: Duration,
    _phantom: PhantomData<T>,
}

impl<T> PoolBuilder<T> {
    fn new() -> Self {
        PoolBuilder {
            min_size: 1,
            max_size: 10,
            timeout: Duration::from_secs(30),
            _phantom: PhantomData,
        }
    }

    fn min_size(mut self, n: usize) -> Self {
        self.min_size = n;
        self
    }

    fn max_size(mut self, n: usize) -> Self {
        self.max_size = n;
        self
    }

    fn timeout(mut self, t: Duration) -> Self {
        self.timeout = t;
        self
    }

    fn build(self) -> Result<Pool<T>, String> {
        if self.min_size > self.max_size {
            return Err(format!(
                "min_size ({}) > max_size ({})",
                self.min_size, self.max_size
            ));
        }
        Ok(Pool {
            min_size: self.min_size,
            max_size: self.max_size,
            timeout: self.timeout,
            _phantom: PhantomData,
        })
    }
}

struct DbConn;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let pool: Pool<DbConn> = PoolBuilder::new()
        .min_size(5)
        .max_size(20)
        .timeout(Duration::from_secs(10))
        .build()?;

    println!("{:?}", pool);
    Ok(())
}
```

No necesitas bounds en `T` para el builder en si. `PhantomData<T>`
le dice al compilador "este builder esta asociado al tipo T" sin
requerir un valor de T.

Si quisieras que `build()` cree instancias reales, necesitarias
un bound como `T: Default` o un factory function:

```rust
fn build_with(self, factory: impl Fn() -> T) -> Result<Pool<T>, String>
```

Pero para un pool, la creacion de items es lazy (on-demand), asi
que el builder solo configura limites.

</details>

---

### Ejercicio 8 — Comparar with/without builder

Escribe la misma configuracion de Server de tres formas:

1. Constructor con todos los parametros
2. Config struct (estilo C)
3. Builder pattern

Cuenta lineas y evalua legibilidad de cada una.

<details><summary>Respuesta</summary>

```rust
// 1. Constructor con todos los parametros (8 lineas, ilegible)
let s1 = Server::new(
    "0.0.0.0", 443, 10_000, 30_000,
    true, Some("/cert.pem"), Some("/key.pem"),
    LogLevel::Warn, Some("/var/log/app.log"), 8,
);
// ¿Que es 10_000? ¿Y 30_000? ¿Y 8? Imposible saber sin docs.

// 2. Config struct (7 lineas, legible)
let cfg = ServerConfig {
    port: 443,
    max_connections: 10_000,
    use_tls: true,
    cert_path: Some("/cert.pem".into()),
    key_path: Some("/key.pem".into()),
    workers: 8,
    ..ServerConfig::default()
};
let s2 = Server::from_config(cfg)?;

// 3. Builder (9 lineas, legible + validacion)
let s3 = ServerBuilder::new()
    .port(443)
    .max_connections(10_000)
    .tls("/cert.pem", "/key.pem")
    .workers(8)
    .log_level(LogLevel::Warn)
    .log_file("/var/log/app.log")
    .build()?;
```

| Aspecto | Constructor | Config struct | Builder |
|---------|------------|--------------|---------|
| Lineas | 4 | 7 | 9 |
| Legibilidad | Baja | Alta | Alta |
| Defaults | No | Si (..default()) | Si (new()) |
| Validacion | En constructor | En from_config | En build() |
| Chaining | No | No | Si |
| Extensible | Rompe API | Compatible | Compatible |
| Metodos helpers | No | No | Si (.tls() setea 3 campos) |

El builder es ligeramente mas largo pero el mas legible y
extensible. `.tls(cert, key)` setea use_tls + cert + key en una
sola llamada — imposible con config struct.

</details>

---

### Ejercicio 9 — Builder que produce distintos tipos

Diseña un builder que produce `HttpServer` o `HttpsServer`
dependiendo de si se llamo `.tls()`:

```rust
let http = ServerBuilder::new().port(80).build_http();
let https = ServerBuilder::new().port(443).tls(c, k).build_https()?;
```

**Prediccion**: Puede un solo `build()` retornar dos tipos
distintos?

<details><summary>Respuesta</summary>

```rust
struct HttpServer {
    host: String,
    port: u16,
}

struct HttpsServer {
    host: String,
    port: u16,
    cert_path: String,
    key_path: String,
}

struct ServerBuilder {
    host: String,
    port: u16,
    cert_path: Option<String>,
    key_path: Option<String>,
}

impl ServerBuilder {
    fn new() -> Self {
        ServerBuilder {
            host: "0.0.0.0".into(),
            port: 8080,
            cert_path: None,
            key_path: None,
        }
    }

    fn host(mut self, host: &str) -> Self {
        self.host = host.into(); self
    }

    fn port(mut self, port: u16) -> Self {
        self.port = port; self
    }

    fn tls(mut self, cert: &str, key: &str) -> Self {
        self.cert_path = Some(cert.into());
        self.key_path = Some(key.into());
        self
    }

    fn build_http(self) -> HttpServer {
        HttpServer { host: self.host, port: self.port }
    }

    fn build_https(self) -> Result<HttpsServer, String> {
        let cert = self.cert_path.ok_or("cert required for HTTPS")?;
        let key = self.key_path.ok_or("key required for HTTPS")?;
        Ok(HttpsServer {
            host: self.host,
            port: self.port,
            cert_path: cert,
            key_path: key,
        })
    }

    // O un solo build con enum:
    fn build(self) -> AnyServer {
        if self.cert_path.is_some() && self.key_path.is_some() {
            AnyServer::Https(self.build_https().unwrap())
        } else {
            AnyServer::Http(self.build_http())
        }
    }
}

enum AnyServer {
    Http(HttpServer),
    Https(HttpsServer),
}
```

Un solo `build()` no puede retornar dos tipos distintos
directamente (cada funcion tiene un tipo de retorno fijo). Las
opciones son:
1. Metodos de build separados (`build_http`, `build_https`)
2. Un enum (`AnyServer`) que contiene ambos
3. Un trait object (`Box<dyn Server>`)
4. Typestate builder (T03) que cambia el tipo al llamar `.tls()`

La opcion 1 es la mas clara cuando los tipos resultantes son
fundamentalmente diferentes.

</details>

---

### Ejercicio 10 — Reflexion: builder en Rust

Responde sin ver la respuesta:

1. Cual es la diferencia clave entre consume-self y &mut self
   para setters?

2. El derive_builder crate genera el builder automaticamente.
   Que sacrificas al usarlo?

3. Si tu builder tiene 3 campos obligatorios y 10 opcionales,
   como lo estructurias? (pista: parametros de new vs setters)

4. Nombra una situacion donde un builder es overkill y un
   simple `struct { ... }` con `Default` seria suficiente.

<details><summary>Respuesta</summary>

**1. Consume-self vs &mut self:**

- `fn port(mut self, ...) -> Self`: el builder se **mueve** en
  cada setter. Despues de `build(self)`, el builder no existe.
  Previene reutilizacion accidental.

- `fn port(&mut self, ...) -> &mut Self`: el builder se **presta**.
  Sigue existiendo despues de `build(&self)`. Permite crear
  multiples objetos del mismo builder (template).

Usa consume-self para objetos unicos (el 90% de los casos).
Usa &mut self cuando necesitas un builder-template.

**2. Sacrificio con derive_builder:**

- Control: no puedes escribir logica custom en los setters
  (ej. `.tls()` que setea 3 campos a la vez).
- Validacion: limitada a una funcion `validate()` al final.
  No puedes rechazar combinaciones incrementalmente.
- Claridad: el builder es generado — no visible en el codigo.
  Nuevos desarrolladores no ven la API hasta leer los docs.
- Estilo: usa `&mut self` por defecto, no consume-self.

Vale la pena cuando: >10 campos, sin logica custom, muchos
builders similares. No vale la pena cuando: pocos campos o
setters con logica especial.

**3. 3 obligatorios + 10 opcionales:**

Los 3 obligatorios van como parametros de `new()`:
```rust
fn new(host: &str, user: &str, password: &str) -> Self
```

Los 10 opcionales van como setters con defaults:
```rust
.port(5432)
.pool_size(10)
.timeout(5000)
// etc.
```

Asi es imposible crear un builder sin los campos obligatorios.
Los opcionales se setean solo si se necesita un valor distinto
del default.

**4. Builder overkill:**

Si todos los campos tienen valores razonables y no hay
validacion cruzada:

```rust
#[derive(Debug, Default)]
struct Margins {
    top: f64,
    right: f64,
    bottom: f64,
    left: f64,
}

// Uso: simple y directo
let m = Margins { top: 10.0, ..Default::default() };
```

No necesitas builder si:
- Pocos campos (< 5)
- Todos opcionales con defaults claros
- Sin validacion cruzada
- Sin logica en los setters
- Los campos son publicos y simples

Un builder agrega complejidad. Solo vale la pena cuando esa
complejidad resuelve un problema real (muchos parametros,
validacion, campos privados, API publica).

</details>
