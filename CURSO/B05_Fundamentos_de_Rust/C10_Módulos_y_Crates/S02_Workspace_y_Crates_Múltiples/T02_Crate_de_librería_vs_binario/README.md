# Crate de librería vs binario: lib.rs vs main.rs

## Índice
1. [Los dos tipos de crate](#1-los-dos-tipos-de-crate)
2. [Binary crate: main.rs](#2-binary-crate-mainrs)
3. [Library crate: lib.rs](#3-library-crate-librs)
4. [Ambos en un mismo paquete](#4-ambos-en-un-mismo-paquete)
5. [Múltiples binarios con src/bin/](#5-múltiples-binarios-con-srcbin)
6. [Cuándo separar lib de bin](#6-cuándo-separar-lib-de-bin)
7. [Publicar en crates.io](#7-publicar-en-cratesio)
8. [Patrones de organización](#8-patrones-de-organización)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Los dos tipos de crate

En Rust, un **crate** es la unidad mínima de compilación. Existen exactamente dos tipos:

| | Binary crate | Library crate |
|---|---|---|
| Punto de entrada | `fn main()` | Sin `main` |
| Archivo raíz | `src/main.rs` | `src/lib.rs` |
| Produce | Ejecutable | `.rlib` (para que otros lo usen) |
| Cantidad por paquete | 0 o más | 0 o 1 |
| Se ejecuta con | `cargo run` | No se ejecuta directamente |
| Lo usan | Usuarios finales | Otros crates (dependencias) |

Un **paquete** (definido por un `Cargo.toml`) puede contener:
- Solo un binary crate
- Solo un library crate
- Un library crate + uno o más binary crates
- Múltiples binary crates (sin library)

Pero **como máximo un library crate**.

---

## 2. Binary crate: main.rs

Un binary crate produce un ejecutable. Su raíz es `src/main.rs` y debe contener `fn main()`:

```rust
// src/main.rs
fn main() {
    println!("Hello, world!");
}
```

### Crear un binary crate

```bash
cargo new mi_app          # binary por defecto
cargo init mi_app          # binary por defecto
cargo new mi_app --bin     # explícitamente binary
```

Genera:

```text
mi_app/
├── Cargo.toml
└── src/
    └── main.rs
```

```toml
# Cargo.toml
[package]
name = "mi_app"
version = "0.1.0"
edition = "2021"
```

### Módulos en un binary crate

```text
src/
├── main.rs       ← crate root, declara módulos
├── config.rs
├── handlers.rs
└── handlers/
    ├── users.rs
    └── products.rs
```

```rust
// src/main.rs
mod config;
mod handlers;

fn main() {
    let cfg = config::load();
    handlers::users::list(&cfg);
}
```

### Limitaciones de un binary-only crate

El código dentro de un binary crate **no puede ser reutilizado** por otros crates:

```rust
// mi_app/src/main.rs
pub fn useful_function() -> i32 { 42 }

fn main() {
    println!("{}", useful_function());
}
```

```toml
# otro_crate/Cargo.toml
[dependencies]
mi_app = { path = "../mi_app" }  # ← esto NO funciona como esperarías
```

```rust
// otro_crate/src/main.rs
// ERROR: no puedes importar funciones de un binary crate
// use mi_app::useful_function;
```

Un binary crate no expone API — solo produce un ejecutable. Para compartir código, necesitas un library crate.

---

## 3. Library crate: lib.rs

Un library crate expone una API pública que otros crates pueden usar. Su raíz es `src/lib.rs`:

```rust
// src/lib.rs
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

pub struct Calculator {
    history: Vec<i32>,
}

impl Calculator {
    pub fn new() -> Self {
        Calculator { history: Vec::new() }
    }

    pub fn compute(&mut self, value: i32) -> i32 {
        self.history.push(value);
        value
    }
}
```

### Crear un library crate

```bash
cargo new mi_lib --lib
```

Genera:

```text
mi_lib/
├── Cargo.toml
└── src/
    └── lib.rs
```

### Usar una library desde otro crate

```toml
# otro_crate/Cargo.toml
[dependencies]
mi_lib = { path = "../mi_lib" }
```

```rust
// otro_crate/src/main.rs
use mi_lib::{add, Calculator};

fn main() {
    println!("{}", add(2, 3));

    let mut calc = Calculator::new();
    calc.compute(42);
}
```

### El nombre del crate en código

El nombre usado en `use` es el `name` del `Cargo.toml`, con guiones convertidos a guiones bajos:

```toml
[package]
name = "my-cool-lib"   # guiones en Cargo.toml
```

```rust
// En código Rust, los guiones se convierten a underscores:
use my_cool_lib::something;
```

### Tests integrados en la library

Una ventaja importante de `lib.rs` es que los **tests de integración** (`tests/`) solo funcionan con library crates:

```text
mi_lib/
├── Cargo.toml
├── src/
│   └── lib.rs
└── tests/
    └── integration_test.rs   ← solo funciona si hay lib.rs
```

```rust
// tests/integration_test.rs
use mi_lib::add;

#[test]
fn test_add() {
    assert_eq!(add(2, 3), 5);
}
```

Los tests de integración tratan tu library como un consumidor externo — solo ven la API pública.

---

## 4. Ambos en un mismo paquete

Un paquete puede tener **lib.rs y main.rs simultáneamente**. Esta es una de las configuraciones más útiles:

```text
mi_proyecto/
├── Cargo.toml
└── src/
    ├── lib.rs      ← library crate: API pública, lógica central
    └── main.rs     ← binary crate: punto de entrada, CLI wrapper
```

### Cómo interactúan

Son **dos crates separados** dentro del mismo paquete. El binary accede a la library como si fuera una dependencia externa:

```rust
// src/lib.rs
pub mod parser {
    pub fn parse(input: &str) -> Vec<&str> {
        input.split_whitespace().collect()
    }
}

pub mod evaluator {
    pub fn eval(tokens: &[&str]) -> i32 {
        tokens.len() as i32
    }
}
```

```rust
// src/main.rs
// Accede a la library usando el nombre del paquete
use mi_proyecto::parser;
use mi_proyecto::evaluator;

fn main() {
    let tokens = parser::parse("hello world");
    let result = evaluator::eval(&tokens);
    println!("Result: {}", result);
}
```

### Lo que main.rs NO puede hacer

```rust
// src/main.rs

// ERROR: main.rs no puede declarar módulos que lib.rs ya declara
// mod parser;  // ERROR si parser.rs ya está vinculado a lib.rs

// ERROR: main.rs no puede acceder a items privados de lib.rs
// use mi_proyecto::internal_helper;  // ERROR si no es pub

// CORRECTO: acceder vía el nombre del paquete
use mi_proyecto::parser::parse;
```

### main.rs con sus propios módulos

`main.rs` puede tener módulos exclusivos que no son parte de la library:

```text
src/
├── lib.rs        ← library (lógica compartible)
├── main.rs       ← binary
├── cli.rs        ← módulo exclusivo del binary
└── parser.rs     ← módulo de la library (declarado en lib.rs)
```

```rust
// src/main.rs
mod cli;  // módulo exclusivo del binary, no parte de la library

use mi_proyecto::parser;

fn main() {
    let args = cli::parse_args();
    let result = parser::parse(&args.input);
    println!("{:?}", result);
}
```

```rust
// src/cli.rs
pub struct Args {
    pub input: String,
}

pub fn parse_args() -> Args {
    let args: Vec<String> = std::env::args().collect();
    Args {
        input: args.get(1).cloned().unwrap_or_default(),
    }
}
```

**Cuidado**: si tanto `lib.rs` como `main.rs` intentan declarar `mod cli;`, habrá conflicto. Cada módulo pertenece a exactamente un crate.

---

## 5. Múltiples binarios con src/bin/

Un paquete puede tener múltiples ejecutables. Cada archivo en `src/bin/` se convierte en un binary crate independiente:

```text
mi_proyecto/
├── Cargo.toml
├── src/
│   ├── lib.rs         ← library compartida
│   ├── main.rs        ← binary principal: cargo run
│   └── bin/
│       ├── server.rs  ← cargo run --bin server
│       ├── worker.rs  ← cargo run --bin worker
│       └── migrate.rs ← cargo run --bin migrate
```

```rust
// src/bin/server.rs
use mi_proyecto::config;

fn main() {
    let cfg = config::load();
    println!("Starting server on port {}", cfg.port);
}
```

```rust
// src/bin/worker.rs
use mi_proyecto::jobs;

fn main() {
    println!("Processing background jobs...");
    jobs::process_all();
}
```

### Ejecutar binarios específicos

```bash
cargo run                   # ejecuta src/main.rs (el default)
cargo run --bin server      # ejecuta src/bin/server.rs
cargo run --bin worker      # ejecuta src/bin/worker.rs
cargo run --bin migrate     # ejecuta src/bin/migrate.rs
```

### Binarios con múltiples archivos

Si un binario necesita sus propios módulos, usa un directorio:

```text
src/bin/
├── server/
│   ├── main.rs          ← punto de entrada
│   ├── routes.rs        ← módulo del binary
│   └── middleware.rs    ← módulo del binary
└── worker.rs            ← binary simple (un archivo)
```

```rust
// src/bin/server/main.rs
mod routes;
mod middleware;

use mi_proyecto::config;

fn main() {
    let cfg = config::load();
    middleware::setup();
    routes::start(cfg.port);
}
```

### Declarar binarios en Cargo.toml

También puedes declarar binarios explícitamente, útil para configurar nombres o paths no estándar:

```toml
# Cargo.toml
[package]
name = "mi_proyecto"
version = "0.1.0"
edition = "2021"

[[bin]]
name = "mi-servidor"
path = "src/server_main.rs"

[[bin]]
name = "mi-cli"
path = "src/cli_main.rs"
```

Nota la sintaxis `[[bin]]` con doble corchete — es un array de tablas en TOML.

---

## 6. Cuándo separar lib de bin

### Patrón recomendado: thin binary

El binary crate debería ser un "wrapper" delgado sobre la library:

```rust
// src/lib.rs — toda la lógica va aquí
pub mod config;
pub mod server;
pub mod database;

pub fn run(cfg: config::Config) -> Result<(), Box<dyn std::error::Error>> {
    let db = database::connect(&cfg.database_url)?;
    server::start(cfg.port, db)?;
    Ok(())
}
```

```rust
// src/main.rs — solo parseo de args y llamada a la library
use mi_app::config::Config;

fn main() {
    let config = Config::from_env();

    if let Err(e) = mi_app::run(config) {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
```

### Ventajas de este patrón

1. **Testeable**: la lógica en `lib.rs` se puede testear con unit tests e integration tests. `main()` es casi imposible de testear directamente.
2. **Reutilizable**: otros crates pueden usar tu library sin el CLI.
3. **Documentable**: `cargo doc` genera documentación útil para la library; un binary sin lib no genera docs interesantes.
4. **Benchmarkable**: los benchmarks (`benches/`) solo pueden testear library crates.

### Cuándo NO necesitas lib.rs

Para scripts simples o herramientas internas que nunca serán reutilizadas:

```rust
// src/main.rs — un script simple, lib.rs sería overhead
fn main() {
    let files: Vec<_> = std::fs::read_dir(".")
        .unwrap()
        .filter_map(|e| e.ok())
        .collect();

    for file in files {
        println!("{}", file.path().display());
    }
}
```

Si tu `main.rs` tiene menos de ~100 líneas y no será reutilizado, un library crate es innecesario.

### Señales de que necesitas lib.rs

- Tu `main.rs` supera las ~200 líneas de lógica (no solo setup/CLI).
- Quieres tests de integración (`tests/` directory).
- Quieres benchmarks (`benches/` directory).
- Otro crate (o futuro crate) reutilizará esta lógica.
- Quieres generar documentación (`cargo doc`).

---

## 7. Publicar en crates.io

### Library crate

Las library crates se publican para que otros las usen como dependencia:

```toml
# Cargo.toml
[package]
name = "mi_lib"
version = "1.0.0"
edition = "2021"
description = "A useful library"
license = "MIT"
repository = "https://github.com/user/mi_lib"
```

```bash
cargo publish    # publica la library en crates.io
```

Los usuarios la agregan a su `Cargo.toml`:

```toml
[dependencies]
mi_lib = "1.0"
```

### Binary crate

Los binary crates también se pueden publicar. Los usuarios los instalan como herramientas:

```bash
cargo install mi_app     # descarga, compila e instala el binario
mi_app --help             # ahora disponible en $PATH
```

### Paquete con ambos

Si publicas un paquete con lib.rs + main.rs:
- `cargo install mi_paquete` instala el binario.
- Otros crates pueden usarlo como dependencia (usan la library).

```toml
# Cargo.toml
[package]
name = "ripgrep"
version = "14.0.0"

# ripgrep tiene lib.rs (library para reutilizar) + main.rs (el CLI)
# cargo install ripgrep → instala el binario rg
# [dependencies] ripgrep = "14" → usa la library
```

---

## 8. Patrones de organización

### Patrón 1: Library pura

```text
src/
└── lib.rs          ← solo library, sin ejecutable
```

Para: crates que son dependencias (serde, rand, tokio).

### Patrón 2: Binary simple

```text
src/
└── main.rs         ← solo ejecutable, sin library
```

Para: scripts, herramientas internas, prototipos rápidos.

### Patrón 3: Library + thin binary (recomendado)

```text
src/
├── lib.rs          ← toda la lógica
└── main.rs         ← wrapper delgado
```

Para: CLI tools, servidores, cualquier aplicación no trivial.

### Patrón 4: Library + múltiples binarios

```text
src/
├── lib.rs          ← lógica compartida
├── main.rs         ← binary principal
└── bin/
    ├── tool_a.rs   ← herramienta auxiliar
    └── tool_b.rs   ← otra herramienta
```

Para: suites de herramientas, aplicaciones con múltiples puntos de entrada.

### Patrón 5: Workspace (proyecto grande)

```text
crates/
├── core/src/lib.rs
├── api/src/main.rs
├── cli/src/main.rs
└── shared/src/lib.rs
```

Para: proyectos con múltiples binarios y libraries interdependientes.

### Árbol de decisión

```text
¿Tu código será reutilizado por otros crates?
├── NO y es simple → solo main.rs (Patrón 2)
├── NO pero es complejo → lib.rs + main.rs (Patrón 3)
├── SÍ, solo como library → solo lib.rs (Patrón 1)
├── SÍ, library + CLI → lib.rs + main.rs (Patrón 3)
└── Múltiples componentes → workspace (Patrón 5)
```

---

## 9. Errores comunes

### Error 1: Declarar módulos en main.rs que pertenecen a lib.rs

```text
src/
├── lib.rs       ← declara: mod parser;
├── main.rs      ← declara: mod parser;  ← ERROR
└── parser.rs
```

```
error[E0583]: file not found for module `parser`
```

Un archivo `.rs` solo puede pertenecer a un crate. Si `lib.rs` declara `mod parser;`, `main.rs` no puede hacerlo. En `main.rs`, accede vía la library:

```rust
// src/main.rs
use mi_proyecto::parser;  // a través de la library, no mod directo
```

### Error 2: Intentar cargo test en un binary-only crate sin tests

```rust
// src/main.rs
fn add(a: i32, b: i32) -> i32 { a + b }

fn main() {
    println!("{}", add(2, 3));
}
```

```text
tests/
└── test_add.rs
```

```rust
// tests/test_add.rs
// ERROR: no se puede importar desde un binary crate
use mi_app::add;  // no compila
```

Los tests de integración (`tests/`) solo funcionan con library crates. **Solución**: mover `add` a `lib.rs` y usar `use mi_app::add` en los tests.

Para unit tests dentro del binary, usa `#[cfg(test)]` inline:

```rust
// src/main.rs
fn add(a: i32, b: i32) -> i32 { a + b }

fn main() {
    println!("{}", add(2, 3));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
    }
}
```

### Error 3: Olvidar que main.rs y lib.rs son crates distintos

```rust
// src/lib.rs
pub fn public_fn() {}
fn private_fn() {}

static mut COUNTER: i32 = 0;
```

```rust
// src/main.rs
fn main() {
    mi_proyecto::public_fn();  // OK

    // ERROR: private_fn no es visible (otro crate)
    // mi_proyecto::private_fn();

    // COUNTER de lib.rs y COUNTER de main.rs son distintas
    // Son dos crates separados con su propia memoria estática
}
```

### Error 4: Nombre de paquete con guiones al importar

```toml
[package]
name = "my-cool-tool"
```

```rust
// src/bin/helper.rs
// ERROR: guiones no son válidos en identificadores Rust
// use my-cool-tool::something;

// CORRECTO: guiones se convierten a underscores
use my_cool_tool::something;
```

### Error 5: No poner [[bin]] para paths no estándar

```text
src/
├── lib.rs
├── server.rs       ← ¿es un módulo o un binario?
└── cli.rs          ← ¿es un módulo o un binario?
```

Sin configuración explícita, `server.rs` y `cli.rs` serán tratados como módulos (si `lib.rs` o `main.rs` los declara con `mod`), no como binarios. Para que sean binarios:

```toml
[[bin]]
name = "server"
path = "src/server.rs"

[[bin]]
name = "cli"
path = "src/cli.rs"
```

O mejor, usar la convención estándar `src/bin/`:

```text
src/
├── lib.rs
└── bin/
    ├── server.rs     ← automáticamente un binary crate
    └── cli.rs        ← automáticamente un binary crate
```

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║               LIBRARY vs BINARY CRATE CHEATSHEET                   ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  BINARY CRATE                                                      ║
║  src/main.rs        → cargo run         → produce ejecutable       ║
║  Tiene fn main()                                                   ║
║  No se puede importar desde otros crates                           ║
║                                                                    ║
║  LIBRARY CRATE                                                     ║
║  src/lib.rs         → cargo build       → produce .rlib            ║
║  No tiene fn main()                                                ║
║  Se importa con: use nombre_crate::item;                           ║
║  Soporta tests de integración (tests/) y benchmarks (benches/)     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  AMBOS EN UN PAQUETE                                               ║
║  src/lib.rs    ← lógica (el "cerebro")                             ║
║  src/main.rs   ← wrapper delgado (el "interruptor")               ║
║                                                                    ║
║  main.rs accede a lib.rs como crate externo:                       ║
║    use nombre_paquete::modulo;                                     ║
║  main.rs NO puede hacer mod de módulos de lib.rs                   ║
║  main.rs SÍ puede tener sus propios mod exclusivos                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MÚLTIPLES BINARIOS                                                ║
║  src/bin/name.rs         → cargo run --bin name                    ║
║  src/bin/name/main.rs    → binary con módulos propios              ║
║  [[bin]] en Cargo.toml   → paths no estándar                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR                                                             ║
║  cargo new app           → binary (main.rs)                        ║
║  cargo new lib --lib     → library (lib.rs)                        ║
║  Añadir lib.rs a binary  → ambos coexisten automáticamente         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CUÁNDO SEPARAR                                                    ║
║  < 100 líneas, no reutilizable   → solo main.rs                   ║
║  > 200 líneas o testeable        → lib.rs + main.rs               ║
║  Dependencia para otros          → solo lib.rs                     ║
║  Múltiples ejecutables           → workspace o src/bin/            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  NOMBRES                                                           ║
║  Cargo.toml: name = "my-cool-lib"                                  ║
║  En código:  use my_cool_lib::item;  (guiones → underscores)       ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Refactorizar binary a lib + bin

Este programa funciona pero toda la lógica está en `main.rs`. Refactorízalo para que tenga `lib.rs` (lógica) y `main.rs` (solo punto de entrada):

```rust
// src/main.rs (estado actual — todo junto)
use std::collections::HashMap;

fn count_words(text: &str) -> HashMap<&str, usize> {
    let mut counts = HashMap::new();
    for word in text.split_whitespace() {
        *counts.entry(word).or_insert(0) += 1;
    }
    counts
}

fn top_words(counts: &HashMap<&str, usize>, n: usize) -> Vec<(&str, usize)> {
    let mut sorted: Vec<_> = counts.iter().map(|(&k, &v)| (k, v)).collect();
    sorted.sort_by(|a, b| b.1.cmp(&a.1));
    sorted.truncate(n);
    sorted
}

fn format_report(words: &[(&str, usize)]) -> String {
    words
        .iter()
        .enumerate()
        .map(|(i, (word, count))| format!("{}. {} ({})", i + 1, word, count))
        .collect::<Vec<_>>()
        .join("\n")
}

fn main() {
    let text = "the cat sat on the mat the cat";
    let counts = count_words(text);
    let top = top_words(&counts, 3);
    println!("{}", format_report(&top));
}
```

**Tareas:**
1. Mueve `count_words`, `top_words`, y `format_report` a `lib.rs` con visibilidad apropiada.
2. `main.rs` solo debe parsear entrada y llamar a la library.
3. Escribe un test de integración en `tests/integration.rs` que verifique `count_words`.
4. ¿Qué funciones necesitan `pub` y cuáles podrían ser `pub(crate)`?

### Ejercicio 2: Múltiples binarios

Crea un paquete con una library y tres binarios que la compartan:

```text
text_tools/
├── Cargo.toml
├── src/
│   ├── lib.rs           ← funciones compartidas
│   └── bin/
│       ├── uppercase.rs  ← convierte stdin a mayúsculas
│       ├── wordcount.rs  ← cuenta palabras de stdin
│       └── reverse.rs    ← invierte cada línea de stdin
```

**Requisitos:**
1. `lib.rs` expone: `to_uppercase(s: &str) -> String`, `count_words(s: &str) -> usize`, `reverse_line(s: &str) -> String`.
2. Cada binario lee de stdin línea por línea, aplica su transformación e imprime el resultado.
3. Verifica que puedes ejecutar cada uno: `echo "hello world" | cargo run --bin uppercase`.

**Preguntas:**
1. ¿Los tres binarios comparten el mismo `Cargo.lock`?
2. Si añades una dependencia en `Cargo.toml`, ¿está disponible para los tres binarios?
3. ¿Podría `uppercase.rs` declarar `mod helper;` con un archivo `src/bin/helper.rs`? ¿Cómo?

### Ejercicio 3: Decidir la estructura

Para cada escenario, decide qué estructura usarías y justifica:

```text
Escenario A:
  Un formateador de JSON que lee stdin y produce stdout formateado.
  50 líneas de código, herramienta personal, no será publicado.

Escenario B:
  Una library de parsing de CSV con soporte para distintos delimitadores,
  encodings, y streaming. Se publicará en crates.io. También incluye
  una herramienta CLI para convertir CSV a JSON.

Escenario C:
  Un sistema con: servidor HTTP, worker de background, CLI de admin,
  y lógica de negocio compartida entre los tres.

Escenario D:
  Un bot de Discord que responde comandos. 200 líneas,
  no será reutilizado por nadie más, pero quieres testear
  la lógica de procesamiento de comandos.
```

Para cada uno indica:
1. ¿Solo `main.rs`, solo `lib.rs`, ambos, o workspace?
2. ¿Tendrías `src/bin/`? ¿Cuántos binarios?
3. ¿Dónde pondrías los tests?
4. ¿Publicarías algo en crates.io?

---

> **Nota**: la separación lib/bin es una de las decisiones arquitectónicas más impactantes en un proyecto Rust. La regla práctica es simple: si tu código hace algo útil más allá de parsear argumentos y formatear output, ese "algo útil" debería estar en `lib.rs`. Esto facilita testing, documentación, reutilización, y mantiene `main.rs` como un punto de entrada limpio y mínimo.
