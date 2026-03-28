# Features: compilación condicional y dependencias opcionales

## Índice
1. [Qué son las features](#1-qué-son-las-features)
2. [Declarar features en Cargo.toml](#2-declarar-features-en-cargotoml)
3. [default features](#3-default-features)
4. [Usar features en el código con cfg](#4-usar-features-en-el-código-con-cfg)
5. [Dependencias opcionales como features](#5-dependencias-opcionales-como-features)
6. [Features que habilitan features de dependencias](#6-features-que-habilitan-features-de-dependencias)
7. [Compilación condicional avanzada con cfg](#7-compilación-condicional-avanzada-con-cfg)
8. [Features en workspaces](#8-features-en-workspaces)
9. [Activar features como consumidor](#9-activar-features-como-consumidor)
10. [Feature unification](#10-feature-unification)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué son las features

Las **features** son flags de compilación condicional que permiten habilitar o deshabilitar partes de un crate. Con features puedes:

- Incluir funcionalidad extra solo cuando se necesita.
- Hacer dependencias opcionales (no se compilan si no se activan).
- Reducir tiempos de compilación y tamaño del binario.
- Ofrecer variantes de tu crate para distintos contextos.

### Ejemplo del ecosistema

```toml
# serde con derive habilitado:
[dependencies]
serde = { version = "1.0", features = ["derive"] }

# tokio con runtime completo:
[dependencies]
tokio = { version = "1.35", features = ["full"] }

# tokio con solo lo mínimo:
[dependencies]
tokio = { version = "1.35", features = ["rt", "macros"] }

# reqwest con soporte JSON:
[dependencies]
reqwest = { version = "0.11", features = ["json", "cookies"] }
```

Sin features, estos crates compilan solo su funcionalidad base — sin macros derive, sin runtime multi-thread, sin parsing JSON.

---

## 2. Declarar features en Cargo.toml

### Sintaxis básica

```toml
# Cargo.toml
[package]
name = "mi_crate"
version = "0.1.0"
edition = "2021"

[features]
# nombre = [lista de features o dependencias que habilita]
json = []              # feature vacía (solo activa cfg)
logging = []           # feature vacía
xml = []
full = ["json", "xml", "logging"]  # habilita otras features
```

Cada feature es un nombre que puedes usar en el código con `#[cfg(feature = "nombre")]`.

### Features como grupos

Una feature puede habilitar otras features:

```toml
[features]
# Features individuales:
http1 = []
http2 = []
tls = []
cookies = []
json = []

# Features compuestas:
default-tls = ["tls"]
full = ["http1", "http2", "tls", "cookies", "json"]
```

---

## 3. default features

La feature especial `default` define qué se activa cuando el consumidor no especifica features:

```toml
[features]
default = ["json", "logging"]
json = []
logging = []
xml = []
```

### Comportamiento del consumidor

```toml
# Activa default (json + logging):
[dependencies]
mi_crate = "1.0"

# Activa default + xml:
[dependencies]
mi_crate = { version = "1.0", features = ["xml"] }

# Desactiva default, solo xml:
[dependencies]
mi_crate = { version = "1.0", default-features = false, features = ["xml"] }

# Sin nada — ni default ni extras:
[dependencies]
mi_crate = { version = "1.0", default-features = false }
```

### Diseñar defaults

```toml
# BUENA PRÁCTICA: default incluye lo que la mayoría necesita
[features]
default = ["std"]     # soporte estándar por defecto
std = []              # usa std (heap, I/O, threads)
alloc = []            # usa alloc sin std completo
# Sin default ni std: #![no_std], sin heap

# BUENA PRÁCTICA: default conservador
[features]
default = []          # nada por defecto, el usuario opta-in
derive = []
json = []
```

La elección depende de tu audiencia. Si la mayoría de usuarios necesita una feature, ponla en default. Si es una capacidad especializada, déjala opt-in.

---

## 4. Usar features en el código con cfg

### #[cfg(feature = "...")]

El atributo `#[cfg(...)]` incluye o excluye código según las features activas:

```rust
// Función que solo existe si la feature "json" está habilitada
#[cfg(feature = "json")]
pub fn parse_json(input: &str) -> serde_json::Value {
    serde_json::from_str(input).unwrap()
}

// Función siempre disponible
pub fn parse_text(input: &str) -> Vec<&str> {
    input.lines().collect()
}
```

### Módulos condicionales

```rust
// src/lib.rs

pub mod core;  // siempre disponible

#[cfg(feature = "json")]
pub mod json;  // solo si feature "json" está activa

#[cfg(feature = "xml")]
pub mod xml;

#[cfg(feature = "logging")]
mod logger;
```

### Bloques cfg en funciones

```rust
pub fn process(data: &str) -> String {
    let result = data.to_uppercase();

    #[cfg(feature = "logging")]
    {
        println!("[LOG] Processed {} bytes", data.len());
    }

    result
}
```

### cfg_attr: atributos condicionales

```rust
// Derivar Serialize solo si la feature "serde" está activa
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Config {
    pub name: String,
    pub port: u16,
}
```

Esto es extremadamente común — permite que tu tipo sea serializable **opcionalmente** sin forzar la dependencia de serde.

### cfg! macro en expresiones

```rust
pub fn backend_name() -> &'static str {
    if cfg!(feature = "postgres") {
        "PostgreSQL"
    } else if cfg!(feature = "sqlite") {
        "SQLite"
    } else {
        "in-memory"
    }
}
```

A diferencia de `#[cfg(...)]`, la macro `cfg!()` no elimina código — evalúa a `true` o `false` en compilación. Ambas ramas deben compilar (los tipos deben existir), pero el compilador optimiza la rama muerta.

---

## 5. Dependencias opcionales como features

Una dependencia marcada como `optional = true` se convierte automáticamente en una feature:

```toml
[dependencies]
serde = { version = "1.0", optional = true }
serde_json = { version = "1.0", optional = true }
tokio = { version = "1.35", optional = true }

[features]
default = []
# "serde", "serde_json", "tokio" son features automáticas
# Activar la feature = compilar la dependencia
json = ["serde", "serde_json"]   # habilita ambas dependencias
async = ["tokio"]
```

### Usar dependencias opcionales en el código

```rust
// Solo disponible si la feature/dependencia "serde" está activa
#[cfg(feature = "serde")]
use serde::{Serialize, Deserialize};

pub struct User {
    pub name: String,
    pub age: u32,
}

// Implementar Serialize solo si serde está disponible
#[cfg(feature = "serde")]
impl Serialize for User {
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        use serde::ser::SerializeStruct;
        let mut s = serializer.serialize_struct("User", 2)?;
        s.serialize_field("name", &self.name)?;
        s.serialize_field("age", &self.age)?;
        s.end()
    }
}
```

### Patrón con cfg_attr y derive (más común)

```rust
#[derive(Debug, Clone)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct User {
    pub name: String,
    pub age: u32,
}
```

### dep: namespace (Rust 1.60+)

Si no quieres que el nombre de la dependencia sea una feature directamente, usa el namespace `dep:`:

```toml
[dependencies]
serde_lib = { package = "serde", version = "1.0", optional = true }

[features]
# Sin dep: → "serde_lib" sería una feature implícita
# Con dep: → controlas el nombre de la feature
serialization = ["dep:serde_lib"]
```

`dep:` evita que la dependencia sea una feature implícita — solo la feature explícita `serialization` la habilita:

```toml
[dependencies]
serde = { version = "1.0", optional = true, features = ["derive"] }

[features]
# Sin dep: → el usuario puede hacer features = ["serde"] (nombre de dependencia)
# Con dep: → el usuario debe hacer features = ["serialize"] (tu nombre de feature)
serialize = ["dep:serde"]
```

---

## 6. Features que habilitan features de dependencias

Puedes activar features de una dependencia condicionalmente:

```toml
[dependencies]
tokio = { version = "1.35", optional = true }

[features]
default = ["runtime"]
runtime = ["tokio/rt", "tokio/macros"]        # activa tokio con rt + macros
runtime-multi = ["tokio/rt-multi-thread", "tokio/macros"]
full = ["tokio/full"]
```

La sintaxis `dependencia/feature` activa una feature específica de la dependencia:

```toml
[dependencies]
serde = { version = "1.0" }  # no opcional, siempre incluida

[features]
derive = ["serde/derive"]    # activa la feature "derive" de serde
# serde siempre se compila, pero derive solo con esta feature
```

### Combinar con dependencias opcionales

```toml
[dependencies]
serde = { version = "1.0", optional = true }
serde_json = { version = "1.0", optional = true }

[features]
json = ["dep:serde", "serde/derive", "dep:serde_json"]
# Activa serde (dependencia opcional), su feature derive, y serde_json
```

---

## 7. Compilación condicional avanzada con cfg

### cfg más allá de features

`cfg` no se limita a features. Puede evaluar otras condiciones de compilación:

```rust
// Por sistema operativo
#[cfg(target_os = "linux")]
fn get_hostname() -> String {
    std::fs::read_to_string("/etc/hostname").unwrap().trim().to_string()
}

#[cfg(target_os = "windows")]
fn get_hostname() -> String {
    std::env::var("COMPUTERNAME").unwrap_or_default()
}

#[cfg(target_os = "macos")]
fn get_hostname() -> String {
    String::from("mac") // simplificado
}
```

### Condiciones disponibles

```rust
// Sistema operativo
#[cfg(target_os = "linux")]
#[cfg(target_os = "windows")]
#[cfg(target_os = "macos")]

// Familia de SO
#[cfg(unix)]        // Linux, macOS, BSDs
#[cfg(windows)]

// Arquitectura
#[cfg(target_arch = "x86_64")]
#[cfg(target_arch = "aarch64")]
#[cfg(target_arch = "wasm32")]

// Puntero size
#[cfg(target_pointer_width = "64")]
#[cfg(target_pointer_width = "32")]

// Endianness
#[cfg(target_endian = "little")]
#[cfg(target_endian = "big")]

// Tests
#[cfg(test)]        // solo en cargo test

// Debug assertions
#[cfg(debug_assertions)]    // debug build
#[cfg(not(debug_assertions))]  // release build
```

### Operadores lógicos

```rust
// NOT
#[cfg(not(feature = "json"))]
fn no_json_fallback() { }

// AND (all)
#[cfg(all(feature = "json", feature = "async"))]
fn json_async() { }

// OR (any)
#[cfg(any(target_os = "linux", target_os = "macos"))]
fn unix_like() { }

// Combinaciones
#[cfg(all(
    unix,
    not(target_os = "macos"),
    feature = "system-allocator"
))]
fn linux_specific() { }
```

### cfg para tests

```rust
// Módulo de tests — solo se compila con cargo test
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_something() {
        assert_eq!(2 + 2, 4);
    }
}

// Código solo para tests (helpers, mocks)
#[cfg(test)]
pub(crate) fn mock_database() -> MockDb {
    MockDb::new()
}
```

### cfg para debug vs release

```rust
fn process(data: &[u8]) -> Vec<u8> {
    #[cfg(debug_assertions)]
    {
        // Solo en debug: verificaciones costosas
        assert!(data.len() < 10_000_000, "Data too large");
        eprintln!("Processing {} bytes", data.len());
    }

    // Lógica real (siempre)
    data.to_vec()
}
```

---

## 8. Features en workspaces

### workspace.features no existe

A diferencia de `[workspace.dependencies]`, **no** existe `[workspace.features]`. Cada crate define sus propias features.

### Patrón: features que propagan a dependencias internas

```toml
# crates/api/Cargo.toml
[dependencies]
core = { workspace = true }

[features]
default = ["json"]
json = ["core/json"]       # propaga la feature "json" al crate core
logging = ["core/logging"]
```

```toml
# crates/core/Cargo.toml
[dependencies]
serde = { version = "1.0", optional = true }
serde_json = { version = "1.0", optional = true }

[features]
json = ["dep:serde", "serde/derive", "dep:serde_json"]
logging = []
```

### Compilar con features desde la raíz

```bash
# Activar feature de un miembro específico:
cargo build -p api --features json

# Activar feature de un crate desde la raíz del workspace:
cargo build -p core --features "json,logging"

# NO funciona: --features sin -p activa features del default-member
cargo build --features json  # ¿de qué crate?
```

---

## 9. Activar features como consumidor

### En Cargo.toml

```toml
# Feature por defecto (lo que el autor eligió):
[dependencies]
mi_crate = "1.0"

# Añadir features extra:
[dependencies]
mi_crate = { version = "1.0", features = ["json", "async"] }

# Sin defaults, solo features específicas:
[dependencies]
mi_crate = { version = "1.0", default-features = false, features = ["json"] }

# Sin nada:
[dependencies]
mi_crate = { version = "1.0", default-features = false }
```

### Con cargo en CLI

```bash
# Compilar con features específicas:
cargo build --features "json,logging"

# Sin features por defecto:
cargo build --no-default-features

# Sin defaults + features específicas:
cargo build --no-default-features --features json

# Todas las features:
cargo build --all-features
```

### Verificar que tu crate compila con distintas combinaciones

```bash
# Buena práctica: verificar varias combinaciones
cargo check                                    # defaults
cargo check --no-default-features              # mínimo
cargo check --all-features                     # máximo
cargo check --no-default-features --features json  # solo json
```

Si tu CI no verifica estas combinaciones, es fácil que una feature rota pase desapercibida.

---

## 10. Feature unification

### Qué es

Cuando múltiples crates en tu grafo de dependencias dependen del mismo crate con distintas features, Cargo **unifica** las features — activa la unión de todas las features solicitadas:

```text
mi_app
├── crate_a (depende de serde con features = ["derive"])
└── crate_b (depende de serde sin features extra)

Resultado: serde se compila UNA vez con features = ["derive"]
```

### Implicación: las features deben ser aditivas

Las features deben **solo añadir** funcionalidad, nunca cambiar o eliminar comportamiento existente:

```toml
# CORRECTO: aditiva
[features]
json = []     # añade soporte JSON
xml = []      # añade soporte XML
# json + xml juntos → ambos soportados

# INCORRECTO: mutuamente excluyente
[features]
backend_postgres = []
backend_sqlite = []
# ¿Qué pasa si otro crate activa postgres y yo activo sqlite?
# Ambas se activan → conflicto
```

```rust
// INCORRECTO: feature que cambia comportamiento
#[cfg(feature = "verbose")]
fn log(msg: &str) { println!("[VERBOSE] {}", msg); }

#[cfg(not(feature = "verbose"))]
fn log(msg: &str) { /* silencio */ }
// Si una dependencia activa "verbose", TODAS las partes del programa lo usan
// Eso puede ser inesperado para el consumidor
```

### La regla de oro

> Las features son una **unión**, nunca una selección exclusiva. Diseña cada feature para que activarla junto con cualquier otra combinación siga siendo válido.

Si necesitas opciones mutuamente excluyentes, usa la selección en runtime (enum, configuración) en lugar de features:

```rust
// En vez de features mutuamente excluyentes:
pub enum Backend {
    Postgres,
    Sqlite,
    InMemory,
}

pub fn connect(backend: Backend) -> Connection {
    match backend {
        Backend::Postgres => postgres_connect(),
        Backend::Sqlite => sqlite_connect(),
        Backend::InMemory => memory_connect(),
    }
}
```

---

## 11. Errores comunes

### Error 1: Feature que rompe la compilación de otra

```toml
[features]
use_foo = []
use_bar = []
```

```rust
#[cfg(feature = "use_foo")]
type Handler = FooHandler;

#[cfg(feature = "use_bar")]
type Handler = BarHandler;  // CONFLICTO si ambas features activas

#[cfg(not(any(feature = "use_foo", feature = "use_bar")))]
type Handler = DefaultHandler;
```

Si alguien activa `use_foo` y `use_bar` simultáneamente, hay dos definiciones de `Handler`. Las features deben ser compatibles entre sí.

**Solución**: usar un solo type alias con prioridad o cambiar a selección en runtime.

### Error 2: Código que no compila sin default features

```toml
[features]
default = ["std"]
std = []
```

```rust
pub fn process() -> Vec<u8> {  // Vec requiere alloc/std
    vec![1, 2, 3]
}
```

Si alguien usa `default-features = false`, `Vec` no está disponible en `#![no_std]` sin `alloc`. Siempre verifica:

```bash
cargo check --no-default-features
```

### Error 3: Feature que no se guarda condicionalmente

```rust
// Cargo.toml
// [dependencies]
// serde = { version = "1.0", optional = true }

// INCORRECTO: usa serde sin verificar la feature
use serde::Serialize;  // ERROR si serde no está activado

// CORRECTO: condicional
#[cfg(feature = "serde")]
use serde::Serialize;
```

Si la dependencia es `optional`, todo el código que la usa debe estar bajo `#[cfg(feature = "nombre")]`.

### Error 4: Olvidar propagar features a sub-dependencias

```toml
# mi_crate/Cargo.toml
[dependencies]
mi_core = { path = "../core" }

[features]
json = []   # ← no propaga a mi_core
```

```rust
// mi_crate/src/lib.rs
#[cfg(feature = "json")]
pub fn parse() {
    mi_core::json::parse("{}");  // ERROR: mi_core no tiene feature json activa
}
```

**Solución**: propagar la feature:

```toml
[features]
json = ["mi_core/json"]   # activa json en mi_core también
```

### Error 5: Demasiadas features granulares

```toml
# EXCESIVO: feature por cada función
[features]
parse_json = []
parse_xml = []
parse_yaml = []
parse_toml = []
format_json = []
format_xml = []
serialize_json = []
# 20 features más...
```

Demasiadas features crean una explosión combinatoria que es imposible de testear. Agrupa funcionalidad relacionada:

```toml
# MEJOR: features por capacidad
[features]
json = []     # parse + format + serialize JSON
xml = []      # parse + format + serialize XML
yaml = []
```

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                       FEATURES CHEATSHEET                          ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DECLARAR FEATURES                                                 ║
║  [features]                                                        ║
║  default = ["json"]              Features activadas por defecto    ║
║  json = []                       Feature simple (solo cfg flag)    ║
║  full = ["json", "xml"]          Feature que habilita otras        ║
║  serde = ["dep:serde"]           Habilita dependencia opcional     ║
║  derive = ["serde/derive"]       Activa feature de dependencia     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DEPENDENCIAS OPCIONALES                                           ║
║  [dependencies]                                                    ║
║  serde = { version = "1.0", optional = true }                      ║
║  → crea feature implícita "serde"                                  ║
║  → con dep: en [features]: serialize = ["dep:serde"]               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  USAR EN CÓDIGO                                                    ║
║  #[cfg(feature = "json")]             Incluir/excluir item         ║
║  #[cfg(not(feature = "json"))]        Negación                     ║
║  #[cfg(all(feature = "a", unix))]     AND                          ║
║  #[cfg(any(feature = "a", test))]     OR                           ║
║  #[cfg_attr(feature = "s", derive(Serialize))]  Atributo cond.    ║
║  if cfg!(feature = "json") { }        Expresión booleana           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  OTROS cfg                                                         ║
║  #[cfg(test)]                    Solo en cargo test                ║
║  #[cfg(debug_assertions)]       Solo en debug build                ║
║  #[cfg(unix)] / #[cfg(windows)] Familia de SO                      ║
║  #[cfg(target_os = "linux")]     SO específico                     ║
║  #[cfg(target_arch = "x86_64")]  Arquitectura                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONSUMIR FEATURES                                                 ║
║  mi_crate = { version = "1.0", features = ["json"] }              ║
║  mi_crate = { ..., default-features = false }  Desactivar default  ║
║  cargo build --features "json,xml"             Desde CLI            ║
║  cargo build --no-default-features             Sin defaults         ║
║  cargo build --all-features                    Todas                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS CLAVE                                                      ║
║  1. Features deben ser ADITIVAS (nunca mutuamente excluyentes)     ║
║  2. Feature unification: Cargo activa la unión de todas            ║
║  3. default-features = false es la forma de desactivar defaults    ║
║  4. Verificar: cargo check con --no-default-features               ║
║     y --all-features                                               ║
║  5. Código con dependencia optional → todo bajo #[cfg(feature)]    ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Diseñar features para una library

Tienes una library de logging. Diseña las features y escribe el `Cargo.toml` y el código condicional:

**Requisitos funcionales:**
- Log a stdout (siempre disponible, base mínima)
- Log a archivo (opcional, requiere `std::fs`)
- Log con colores en terminal (opcional, usa crate `colored`)
- Log en formato JSON (opcional, usa `serde` + `serde_json`)
- Log con timestamps (opcional, usa crate `chrono`)

```toml
# Escribe el Cargo.toml con [features] y [dependencies]
```

```rust
// Escribe lib.rs con el código condicional
pub fn log(level: &str, message: &str) {
    // Base: siempre disponible
    // + timestamp si feature "timestamps"
    // + colores si feature "colors"
    // + formato JSON si feature "json"
}

// Solo si feature "file":
// pub fn log_to_file(path: &str, level: &str, message: &str) { ... }
```

**Preguntas:**
1. ¿Qué pondrías en `default`?
2. ¿Crearías una feature `full`? ¿Qué incluiría?
3. ¿`colored` y `json` son compatibles entre sí? (¿qué significa log con colores en JSON?)

### Ejercicio 2: cfg_attr con serde

Haz que estos tipos sean serializables **opcionalmente** sin forzar la dependencia de serde:

```rust
pub struct Config {
    pub host: String,
    pub port: u16,
    pub debug: bool,
}

pub enum LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Trace,
}

pub struct ServerStatus {
    pub uptime_secs: u64,
    pub connections: u32,
    pub config: Config,
}
```

**Tareas:**
1. Escribe el `[features]` y `[dependencies]` en Cargo.toml.
2. Añade `#[cfg_attr(...)]` a cada tipo para derivar `Serialize` y `Deserialize` condicionalmente.
3. Añade una función `pub fn to_json(status: &ServerStatus) -> String` que solo exista con la feature `json`.
4. Verifica mentalmente que compila con `--no-default-features` y con `--features json`.

### Ejercicio 3: Analizar features de un crate real

Analiza las features de `tokio` (sin necesidad de instalar nada, solo razonando):

```toml
# Simplificación de las features de tokio:
[features]
default = []
full = ["io-util", "io-std", "net", "time", "rt", "rt-multi-thread",
        "macros", "sync", "signal", "fs"]

rt = []                    # runtime single-thread
rt-multi-thread = ["rt"]   # runtime multi-thread (requiere rt)
macros = ["tokio-macros"]  # #[tokio::main] y #[tokio::test]
net = []                   # TCP, UDP, Unix sockets
io-util = []               # AsyncReadExt, AsyncWriteExt
io-std = []                # Stdin, Stdout, Stderr async
time = []                  # sleep, timeout, interval
sync = []                  # Mutex, RwLock, mpsc, watch, etc.
signal = ["net"]           # ctrl_c, signal handling
fs = []                    # operaciones de filesystem async

[dependencies]
tokio-macros = { version = "...", optional = true }
```

**Preguntas:**
1. ¿Por qué `default = []` (vacío)? ¿Qué compila con `tokio = "1.35"` sin features?
2. ¿Por qué `rt-multi-thread` requiere `rt`?
3. ¿Por qué `signal` requiere `net`?
4. Si solo necesitas `#[tokio::main]` con `async fn main` y `tokio::spawn`, ¿cuáles son las features mínimas?
5. ¿Son todas estas features aditivas? ¿Puede haber conflicto si alguien activa `rt` y otro `rt-multi-thread`?
6. ¿Por qué `features = ["full"]` existe si podrías listar todas las features individualmente?

---

> **Nota**: las features son el mecanismo que permite al ecosistema Rust mantener crates potentes sin forzar dependencias pesadas. Crates como tokio, serde, y reqwest serían mucho más pesados sin features. Al diseñar tu crate, piensa en features como un contrato: cada feature es una promesa de que esa funcionalidad estará disponible y será compatible con todas las demás. Menos features bien diseñadas es mejor que muchas features granulares.
