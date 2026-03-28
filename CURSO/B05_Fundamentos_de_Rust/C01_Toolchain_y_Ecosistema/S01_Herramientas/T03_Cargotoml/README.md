# T03 -- Cargo.toml

## Que es Cargo.toml

Cargo.toml es el archivo **manifiesto** de todo proyecto Rust. Define
el nombre del proyecto, su version, sus dependencias, perfiles de
compilacion y configuraciones opcionales. Cargo lee este archivo para
saber como compilar, testear y publicar el proyecto.

```toml
# Un Cargo.toml minimo:
[package]
name = "my_project"
version = "0.1.0"
edition = "2021"
```

```bash
# Cargo busca Cargo.toml en el directorio actual:
cargo build

# Equivalencias con otros ecosistemas:
# Cargo.toml ≈ package.json (Node.js)
# Cargo.toml ≈ pyproject.toml (Python)
# Cargo.toml ≈ pom.xml (Java/Maven)
# Cargo.toml ≈ Makefile + pkg-config (C/C++)
```

El formato es **TOML** (Tom's Obvious Minimal Language), disenado
para ser legible por humanos. Usa pares `clave = "valor"`, secciones
entre `[corchetes]` y tablas anidadas:

```toml
# Sintaxis basica de TOML:
name = "my_project"                # cadena
opt-level = 3                      # numero
debug = true                       # booleano
authors = ["Alice", "Bob"]         # array
serde = { version = "1.0" }        # tabla inline

[package]                          # tabla (seccion)
name = "my_project"

[profile.dev]                      # tabla anidada
opt-level = 0
```

## La seccion [package]

Contiene los metadatos del proyecto. Los tres campos esenciales:

```toml
[package]
# name — nombre del crate. Solo letras, numeros, - y _.
# Este nombre se usa en crates.io si publicas el crate.
name = "my_project"

# version — version del proyecto, siguiendo SemVer (major.minor.patch).
# 0.1.0 indica que el proyecto aun no tiene API estable.
version = "0.1.0"

# edition — edicion de Rust a usar.
# Valores: "2015", "2018", "2021", "2024".
# cargo init crea proyectos con la edicion mas reciente estable.
edition = "2021"
```

### Que es la edicion (edition)

La edicion **no** es la version del compilador. Es un mecanismo para
introducir cambios de sintaxis y semantica sin romper codigo existente:

```toml
# "2015" — primera edicion. Sin async/await, sin NLL.
# "2018" — async/await, NLL, dyn Trait obligatorio.
# "2021" — closures con capturas disjuntas, prefijo dep: en features.
# "2024" — unsafe_op_in_unsafe_fn como error, gen blocks.
```

```rust
// Punto clave: codigo de diferentes ediciones INTEROPERA.
// Un crate con edition = "2015" puede depender de uno con "2021"
// y viceversa. La edicion es por-crate, no por-proyecto.
// El compilador es el mismo; la edicion solo cambia como
// interpreta la sintaxis de ESE crate.
```

```bash
# Migrar entre ediciones:
cargo fix --edition
# Aplica correcciones automaticas. Luego cambias edition en Cargo.toml.
```

### Campos opcionales de metadatos

```toml
[package]
name = "my_project"
version = "0.1.0"
edition = "2021"

authors = ["Alice <alice@example.com>"]    # autores del crate
description = "A fast HTTP client"          # obligatoria para publicar
license = "MIT OR Apache-2.0"              # identificador SPDX
repository = "https://github.com/user/my_project"
homepage = "https://my_project.example.com"
documentation = "https://docs.rs/my_project"
readme = "README.md"
keywords = ["http", "client", "async"]     # hasta 5 para crates.io
categories = ["web-programming::http-client"]
exclude = ["tests/fixtures/*", "benches/"] # archivos a excluir al publicar

# rust-version — Minimum Supported Rust Version (MSRV).
# Cargo rechaza compilar si el compilador es mas viejo.
rust-version = "1.70.0"
```

```bash
# Si tu compilador es 1.65.0 y el crate requiere 1.70.0:
cargo build
# error: package `my_project v0.1.0` cannot be built because it
# requires rustc 1.70.0 or newer
```

## La seccion [dependencies]

Las dependencias son otros crates que tu proyecto usa. Cargo las
descarga de crates.io, las compila y las enlaza automaticamente:

```toml
[dependencies]
serde = "1.0"
```

```bash
cargo build
# Downloading crates ...
# Downloaded serde v1.0.197
# Compiling serde v1.0.197
# Compiling my_project v0.1.0
```

### Requisitos de version

El numero de version es un **requisito**, no una version exacta.
Cargo busca la version mas alta que lo cumpla:

```toml
[dependencies]
# Caret (^) — valor por defecto. Compatible con SemVer.
# "1.0" es lo mismo que "^1.0".
serde = "1.0"          # >= 1.0.0, < 2.0.0
serde = "^1.2.3"       # >= 1.2.3, < 2.0.0
serde = "^0.2.3"       # >= 0.2.3, < 0.3.0  (regla especial para 0.x)
serde = "^0.0.3"       # >= 0.0.3, < 0.0.4  (regla especial para 0.0.x)

# Tilde (~) — solo permite cambios en el ultimo componente.
serde = "~1.0"         # >= 1.0.0, < 1.1.0
serde = "~1.0.3"       # >= 1.0.3, < 1.1.0

# Exacto (=) — solo esa version.
serde = "=1.0.197"

# Rango — expresiones arbitrarias.
serde = ">=1.0, <2.0"

# Wildcard (*) — cualquier version. EVITAR en produccion.
serde = "*"
serde = "1.*"
```

### SemVer y las reglas especiales de 0.x

```toml
# Versiones >= 1.0.0 — SemVer estandar:
#   major: cambios incompatibles
#   minor: nuevas funcionalidades compatibles
#   patch: correcciones de bugs
#
# Versiones 0.x.y — el minor actua como major:
#   0.1.0 → 0.2.0 puede romper la API
#   0.1.0 → 0.1.1 solo patches
#
# Versiones 0.0.x — cada patch puede romper:
#   0.0.1 → 0.0.2 puede romper la API
#
# Consecuencia practica:
[dependencies]
tokio = "1.0"     # seguro: cualquier 1.x.y
rand = "0.8"      # en practica: cualquier 0.8.x (no sube a 0.9)
```

### Dependencias con features y opcionales

```toml
[dependencies]
# Activar features especificas:
serde = { version = "1.0", features = ["derive"] }

# Desactivar features por defecto:
serde_json = { version = "1.0", default-features = false }

# Combinar:
tokio = { version = "1.0", default-features = false, features = ["rt", "macros"] }

# Dependencia opcional — no se compila a menos que se active:
serde_json = { version = "1.0", optional = true }
```

### Dependencias por ruta (path)

```toml
[dependencies]
# Apuntar a un crate local:
my_lib = { path = "../my_lib" }

# Combinar path con version (para publicar):
my_lib = { version = "0.5.0", path = "../my_lib" }
# En desarrollo local usa path. Al publicar, el path se ignora.
```

### Dependencias por Git

```toml
[dependencies]
my_lib = { git = "https://github.com/user/my_lib" }
my_lib = { git = "https://github.com/user/my_lib", branch = "main" }
my_lib = { git = "https://github.com/user/my_lib", tag = "v1.0.0" }
my_lib = { git = "https://github.com/user/my_lib", rev = "a1b2c3d" }
```

### Renombrar dependencias

```toml
[dependencies]
# El nombre a la izquierda es como lo usas en tu codigo.
# "package" indica el nombre real en crates.io.
my_serde = { package = "serde", version = "1.0" }

# Uso tipico: dos versiones del mismo crate.
serde_v1 = { package = "serde", version = "1.0" }
serde_v2 = { package = "serde", version = "2.0" }
```

```rust
// En tu codigo usas el nombre renombrado:
use my_serde::Deserialize;
```

### [dev-dependencies] y [build-dependencies]

```toml
[dev-dependencies]
# Solo para tests, ejemplos y benchmarks.
# NO se incluyen en el binario final.
assert_cmd = "2.0"
tempfile = "3.0"
mockall = "0.12"

[build-dependencies]
# Solo para el script build.rs (se ejecuta ANTES de compilar tu crate).
cc = "1.0"            # compilar codigo C/C++
bindgen = "0.69"       # generar bindings FFI
pkg-config = "0.3"     # buscar librerias del sistema
```

```rust
// build.rs — se compila y ejecuta antes de tu crate.
fn main() {
    cc::Build::new()
        .file("src/helper.c")
        .compile("helper");
}
```

## La seccion [features]

Los features son **flags opcionales de compilacion**. Permiten
activar o desactivar funcionalidad en tiempo de compilacion.
Son aditivos: activar un feature nunca debe romper la compilacion.

```toml
[features]
# Feature por defecto — se activa automaticamente:
default = ["std"]

# Features simples (lista vacia si no dependen de nada):
std = []
alloc = []

# Features que activan dependencias opcionales (dep: prefix):
json = ["dep:serde_json"]

# Features que activan otros features:
full = ["json", "xml", "yaml"]

# Activar features de una dependencia:
serialization = ["dep:serde", "serde/derive"]
# serde/derive = feature "derive" del crate "serde"

[dependencies]
serde = { version = "1.0", optional = true }
serde_json = { version = "1.0", optional = true }
```

```bash
cargo build --features "json"                         # activar feature
cargo build --features "json xml"                     # multiples features
cargo build --no-default-features                     # sin defaults
cargo build --no-default-features --features "json"   # solo json
cargo build --all-features                            # todos
```

### Compilacion condicional con features

```rust
// Compilar modulo solo si el feature "json" esta activo:
#[cfg(feature = "json")]
pub mod json_support {
    pub fn parse(input: &str) -> serde_json::Result<serde_json::Value> {
        serde_json::from_str(input)
    }
}

// Alternativas segun feature:
#[cfg(feature = "std")]
pub fn get_time() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs()
}

#[cfg(not(feature = "std"))]
pub fn get_time() -> u64 { 0 }

// cfg! como expresion booleana:
fn init() {
    if cfg!(feature = "logging") {
        println!("Logging enabled");
    }
}

// Aplicar derive condicionalmente:
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Config {
    pub name: String,
    pub value: i32,
}
```

### El prefijo dep:

Antes de la edicion 2021, las dependencias opcionales creaban
automaticamente un feature con el mismo nombre. El prefijo `dep:`
elimina esa ambiguedad:

```toml
# Sin dep: (estilo antiguo):
[features]
default = ["serde_json"]   # es un feature o una dependencia?

# Con dep: (estilo moderno, recomendado):
[features]
json = ["dep:serde_json"]  # claro: activa la dependencia
default = ["json"]          # claro: activa el feature
```

## Las secciones [profile.*]

Los perfiles controlan **como** compila Cargo. Cada perfil define
opciones del compilador (optimizacion, debug info, LTO, etc.):

```toml
# Perfiles predefinidos:
# dev     → cargo build, cargo run
# release → cargo build --release
# test    → cargo test (hereda de dev)
# bench   → cargo bench (hereda de release)
```

### [profile.dev] y [profile.release]

```toml
# Valores por defecto de dev (prioriza velocidad de compilacion):
[profile.dev]
opt-level = 0          # sin optimizacion
debug = true           # debug info completa (DWARF)
debug-assertions = true
overflow-checks = true # panic en overflow de enteros
lto = false
codegen-units = 256    # maximo paralelismo de compilacion
panic = "unwind"
incremental = true
strip = "none"

# Valores por defecto de release (prioriza rendimiento):
[profile.release]
opt-level = 3          # optimizacion maxima
debug = false          # sin debug info
debug-assertions = false
overflow-checks = false
lto = false
codegen-units = 16     # menos unidades = mejor optimizacion
panic = "unwind"
incremental = false
strip = "none"
```

### Opciones detalladas de perfil

```toml
[profile.release]
# opt-level:
#   0: sin optimizacion     1: basicas
#   2: mayoria               3: todas (vectorizacion agresiva)
#   "s": optimizar tamano   "z": tamano agresivo (menor que "s")
opt-level = 3

# debug:
#   false/0: sin info      1/"line-tables-only": solo tablas de lineas
#   true/2: debug completa
debug = false

# lto (Link-Time Optimization):
#   false: sin LTO
#   true/"fat": LTO completa (lento de compilar, binario mas rapido)
#   "thin": LTO parcial (buen balance)
lto = "thin"

# codegen-units: particiones para compilacion paralela.
#   1: maxima optimizacion    256: maximo paralelismo
codegen-units = 1

# panic: "unwind" (permite catch_unwind) o "abort" (binario mas pequeno)
panic = "abort"

# overflow-checks: true = panic en overflow, false = wrap silencioso
overflow-checks = true

# strip: "none", "debuginfo", o "symbols" (binario mas pequeno)
strip = "symbols"
```

### Optimizar dependencias en dev

Patron comun: tu codigo sin optimizacion (debug rapido) pero
dependencias con optimizacion (para que no sean lentas):

```toml
[profile.dev]
opt-level = 0

# Todas las dependencias con opt-level 2:
[profile.dev.package."*"]
opt-level = 2

# O una dependencia especifica:
[profile.dev.package.serde_json]
opt-level = 3

# Util con crates pesados (image, regex, serde_json)
# que en debug pueden ser 10-100x mas lentos.
```

### Perfiles personalizados

```toml
# Perfil personalizado para CI:
[profile.ci]
inherits = "release"
debug = 1              # solo tablas de lineas
strip = "none"
lto = "thin"

# Perfil para profiling:
[profile.profiling]
inherits = "release"
debug = true           # debug info completa para el profiler
strip = "none"
```

```bash
cargo build --profile ci
cargo build --profile profiling

# Directorio de salida = nombre del perfil:
# target/ci/my_project
# target/profiling/my_project
# (excepto dev → target/debug y release → target/release)
```

## Cargo.lock

`Cargo.lock` es generado automaticamente por Cargo. Contiene las
**versiones exactas** de todas las dependencias (directas y
transitivas):

```toml
# Fragmento de Cargo.lock (no editar manualmente):
[[package]]
name = "serde"
version = "1.0.197"
source = "registry+https://github.com/rust-lang/crates.io-index"
checksum = "3fb1c873e1b9b056a4dc4c0c198b24c3ffa059243875b..."
```

```bash
# Cuando hacer commit de Cargo.lock:
#
# Binarios (aplicaciones): SI — garantiza builds reproducibles.
# Bibliotecas (crates publicados): historicamente NO, pero desde 2023
#   la recomendacion oficial es SI, para reproducibilidad en CI.
#   (El Cargo.lock de una libreria NO se usa por los crates que dependen de ella.)
```

```bash
# cargo update — actualizar dependencias dentro de sus requisitos:
cargo update                          # todas las dependencias
cargo update serde                    # solo serde
cargo update serde --precise 1.0.195  # a version especifica
cargo update --dry-run                # ver sin ejecutar
```

## Otras secciones utiles

### [workspace]

Agrupa multiples crates bajo un mismo Cargo.lock y directorio
`target/`:

```toml
# Cargo.toml raiz:
[workspace]
members = ["crates/my_app", "crates/my_lib", "crates/my_utils"]

[workspace.dependencies]
serde = { version = "1.0", features = ["derive"] }
tokio = { version = "1.0", features = ["full"] }
```

```toml
# Cargo.toml de un miembro:
[package]
name = "my_app"
version = "0.1.0"
edition = "2021"

[dependencies]
serde = { workspace = true }   # hereda version y features del workspace
tokio = { workspace = true }
```

### [[bin]], [lib] y [[example]]

```toml
# [lib] — configurar el crate como biblioteca.
[lib]
name = "my_lib"
path = "src/lib.rs"
crate-type = ["lib"]   # "lib", "dylib", "staticlib", "cdylib", "rlib"

# [[bin]] — binarios (doble corchete: puede haber varios).
[[bin]]
name = "server"
path = "src/bin/server.rs"

[[bin]]
name = "client"
path = "src/bin/client.rs"

# [[example]], [[test]], [[bench]] — analogo.
[[bench]]
name = "my_bench"
path = "benches/my_bench.rs"
harness = false        # deshabilitar el harness de test
```

```bash
cargo run --bin server
cargo run --example basic_usage

# Sin [[bin]], Cargo detecta binarios automaticamente:
# src/main.rs       → binario con nombre del paquete
# src/bin/server.rs → binario "server"
```

### [patch]

Sustituir una dependencia globalmente en el grafo de dependencias:

```toml
[patch.crates-io]
serde = { path = "../serde" }          # version local
serde = { git = "https://github.com/my-fork/serde", branch = "fix-bug" }
# [replace] esta deprecado. Usar [patch].
```

```bash
# Caso de uso: encontraste un bug en una dependencia.
# 1. Clonar el crate, hacer el fix localmente.
# 2. Agregar [patch.crates-io] apuntando a tu copia.
# 3. Compilar y testear. Enviar PR al upstream.
# 4. Cuando upstream publique la correccion, quitar el patch.
```

## Ejemplo completo de Cargo.toml

```toml
[package]
name = "my_web_server"
version = "0.3.0"
edition = "2021"
authors = ["Alice <alice@example.com>"]
description = "A lightweight HTTP server"
license = "MIT OR Apache-2.0"
repository = "https://github.com/alice/my_web_server"
rust-version = "1.70.0"

[dependencies]
tokio = { version = "1.0", features = ["full"] }
hyper = { version = "1.0", features = ["server", "http1"] }
serde = { version = "1.0", features = ["derive"] }
serde_json = { version = "1.0", optional = true }
tracing = "0.1"

[dev-dependencies]
assert_cmd = "2.0"
tempfile = "3.0"

[build-dependencies]
cc = "1.0"

[features]
default = ["json"]
json = ["dep:serde_json"]
full = ["json"]

[profile.dev]
opt-level = 0

[profile.dev.package."*"]
opt-level = 2

[profile.release]
opt-level = 3
lto = "thin"
codegen-units = 1
strip = "symbols"
panic = "abort"
```

```bash
# Verificar que el Cargo.toml es valido:
cargo check

# Ver el arbol de dependencias completo:
cargo tree

# Ver features activos:
cargo tree -e features
```

---

## Ejercicios

### Ejercicio 1 -- Cargo.toml basico

```toml
# Crear un proyecto nuevo con cargo init y modificar su Cargo.toml:
#
# 1. Nombre: "greet_cli"
# 2. Version: "0.1.0"
# 3. Edition: "2021"
# 4. Agregar dependency: clap = { version = "4.0", features = ["derive"] }
# 5. Compilar con cargo build y verificar que descarga clap.
# 6. Ejecutar cargo tree y observar las dependencias transitivas.
# 7. Examinar Cargo.lock — buscar la version exacta de clap resuelta.
#
# Verificar:
#   cargo check    → compila sin errores
#   cargo tree     → muestra clap y sus dependencias
```

### Ejercicio 2 -- Requisitos de version

```toml
# Sin ejecutar nada, predecir que rango de versiones acepta cada requisito.
# Luego verificar con cargo en un proyecto de prueba.
#
# 1. serde = "1.0"           → rango: ???
# 2. serde = "^0.9.3"        → rango: ???
# 3. serde = "~1.2.3"        → rango: ???
# 4. serde = "=1.0.100"      → rango: ???
# 5. rand = "0.8"             → rango: ???
# 6. rand = "^0.0.5"          → rango: ???
#
# Respuestas esperadas:
# 1. >= 1.0.0, < 2.0.0
# 2. >= 0.9.3, < 0.10.0
# 3. >= 1.2.3, < 1.3.0
# 4. exactamente 1.0.100
# 5. >= 0.8.0, < 0.9.0
# 6. >= 0.0.5, < 0.0.6
```

### Ejercicio 3 -- Features

```toml
# Crear un crate de libreria con features:
#
# 1. cargo init --lib feature_demo
# 2. Definir en Cargo.toml:
#    [features]
#    default = ["greeting"]
#    greeting = []
#    farewell = []
#    full = ["greeting", "farewell"]
#
# 3. En src/lib.rs, implementar:
#    - #[cfg(feature = "greeting")] pub fn hello() que imprime "Hello!"
#    - #[cfg(feature = "farewell")] pub fn goodbye() que imprime "Goodbye!"
#    - pub fn available_features() que usa cfg! para reportar cuales estan activas.
#
# 4. Crear examples/demo.rs que llame a las funciones disponibles.
#
# Verificar:
#   cargo run --example demo                          → solo hello (default)
#   cargo run --example demo --no-default-features    → ninguna funcion
#   cargo run --example demo --features "farewell"    → hello y goodbye
#   cargo run --example demo --all-features           → hello y goodbye
```

### Ejercicio 4 -- Perfiles de compilacion

```bash
# Crear un proyecto que haga calculo intensivo (ej: calcular primos
# hasta 100000 con un bucle ingenuo) y comparar tiempos:
#
# 1. cargo build && time ./target/debug/my_project
# 2. cargo build --release && time ./target/release/my_project
#
# 3. Agregar al Cargo.toml:
#    [profile.release]
#    lto = "fat"
#    codegen-units = 1
#    panic = "abort"
#    strip = "symbols"
#
# 4. Recompilar con --release y comparar:
#    - Tiempo de compilacion (mas lento con lto = "fat")
#    - Tamano del binario (ls -lh target/release/my_project)
#    - Tiempo de ejecucion
#
# 5. Comparar tamanos de binario con y sin strip = "symbols".
#
# Preguntas:
#   - Cuanto mas rapido es release vs debug? (tipicamente 10-100x)
#   - Cuanto reduce strip el tamano del binario?
#   - Cuanto tarda mas la compilacion con lto = "fat"?
```

### Ejercicio 5 -- Workspace con dependencias path y dev

```toml
# Crear un workspace con dos crates:
#
# 1. Estructura:
#    my_workspace/
#      Cargo.toml        ← [workspace] con members
#      crates/
#        my_math/
#          Cargo.toml    ← libreria con fn add(a: i32, b: i32) -> i32
#          src/lib.rs
#        my_app/
#          Cargo.toml    ← binario que usa my_math
#          src/main.rs
#
# 2. my_app depende de my_math via path:
#    my_math = { path = "../my_math" }
#
# 3. Agregar dev-dependency a my_math:
#    [dev-dependencies]
#    assert_cmd = "2.0"
#
# Verificar:
#   cargo build (desde la raiz) → compila ambos crates
#   cargo test (desde la raiz)  → ejecuta tests de ambos
#   cargo tree                  → muestra la relacion entre crates
#   Que assert_cmd NO aparece como dependencia de my_app.
```
