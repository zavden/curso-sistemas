# Tests de integración: carpeta tests/, scope y #[ignore]

## Índice
1. [Qué son los tests de integración](#1-qué-son-los-tests-de-integración)
2. [La carpeta tests/](#2-la-carpeta-tests)
3. [Scope: solo la API pública](#3-scope-solo-la-api-pública)
4. [Múltiples archivos de test](#4-múltiples-archivos-de-test)
5. [Módulos compartidos entre tests](#5-módulos-compartidos-entre-tests)
6. [#[ignore]: tests lentos y condicionales](#6-ignore-tests-lentos-y-condicionales)
7. [Setup y teardown en integración](#7-setup-y-teardown-en-integración)
8. [Tests de integración en binary crates](#8-tests-de-integración-en-binary-crates)
9. [Tests de integración en workspaces](#9-tests-de-integración-en-workspaces)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué son los tests de integración

Los tests de integración testean tu library **desde la perspectiva de un consumidor externo**. Solo ven la API pública — como si fueran otro crate que depende del tuyo:

| | Unit tests | Tests de integración |
|---|---|---|
| Ubicación | `src/`, junto al código | `tests/`, directorio separado |
| Acceso | Items públicos y privados | Solo items públicos (`pub`) |
| Perspectiva | Interna — detalles de implementación | Externa — API pública |
| `#[cfg(test)]` | Necesario | No necesario (solo se compilan con `cargo test`) |
| Importación | `use super::*` | `use mi_crate::...` |
| Propósito | Verificar componentes aislados | Verificar que los componentes trabajan juntos |

### Cuándo usar cada uno

- **Unit tests**: lógica interna compleja, funciones privadas, casos edge de componentes individuales.
- **Integration tests**: flujos completos, interacción entre módulos, verificar que la API pública se comporta como prometes.

---

## 2. La carpeta tests/

Los tests de integración viven en el directorio `tests/` en la raíz del proyecto (al mismo nivel que `src/`):

```text
mi_proyecto/
├── Cargo.toml
├── src/
│   └── lib.rs
└── tests/
    ├── basic.rs
    ├── advanced.rs
    └── regression.rs
```

Cada archivo `.rs` en `tests/` es un **crate independiente**. No necesita `#[cfg(test)]` ni `mod tests` — todo el archivo es test:

```rust
// tests/basic.rs
use mi_proyecto::add;

#[test]
fn test_add_from_outside() {
    assert_eq!(add(2, 3), 5);
}

#[test]
fn test_add_large_numbers() {
    assert_eq!(add(1_000_000, 2_000_000), 3_000_000);
}
```

```rust
// src/lib.rs
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

### Ejecutar

```bash
# Todos los tests (unit + integration + doc):
cargo test

# Solo los tests de integración:
cargo test --test basic         # solo tests/basic.rs
cargo test --test advanced      # solo tests/advanced.rs

# Un test específico dentro de un archivo:
cargo test --test basic test_add_from_outside
```

### Output de cargo test

```bash
$ cargo test
   Compiling mi_proyecto v0.1.0
    Finished `test` profile

     Running unittests src/lib.rs
running 2 tests
test tests::test_internal ... ok
test tests::test_private ... ok

     Running tests/basic.rs
running 2 tests
test test_add_from_outside ... ok
test test_add_large_numbers ... ok

     Running tests/advanced.rs
running 1 test
test test_complex_flow ... ok

   Doc-tests mi_proyecto
running 0 tests

test result: ok. 5 passed; 0 failed
```

Cargo separa claramente los tres tipos de tests en el output.

---

## 3. Scope: solo la API pública

Los tests de integración son crates independientes — solo ven lo que cualquier usuario externo vería:

```rust
// src/lib.rs
pub fn public_api(input: &str) -> String {
    let cleaned = internal_clean(input);
    internal_transform(&cleaned)
}

fn internal_clean(s: &str) -> String {
    s.trim().to_lowercase()
}

fn internal_transform(s: &str) -> String {
    s.to_uppercase()
}

pub struct Config {
    pub name: String,
    secret_key: String,  // campo privado
}

impl Config {
    pub fn new(name: &str) -> Self {
        Config {
            name: name.to_string(),
            secret_key: String::from("default"),
        }
    }
}
```

```rust
// tests/api_test.rs
use mi_proyecto::{public_api, Config};

#[test]
fn test_public_api() {
    // OK: public_api es pub
    let result = public_api("  Hello  ");
    assert_eq!(result, "HELLO");
}

#[test]
fn test_config_creation() {
    // OK: Config y Config::new son pub
    let cfg = Config::new("test");
    assert_eq!(cfg.name, "test");

    // ERROR: secret_key es privado
    // assert_eq!(cfg.secret_key, "default");
}

// ERROR: internal_clean no es pub
// fn test_internal() {
//     let result = mi_proyecto::internal_clean("test");
// }
```

### Esto es una ventaja

Si reorganizas la implementación interna (renombras funciones privadas, cambias estructuras internas), los tests de integración **no se rompen** — solo verifican la API pública que prometiste mantener.

---

## 4. Múltiples archivos de test

Cada archivo en `tests/` se compila como un crate separado. Organiza por funcionalidad o por área:

```text
tests/
├── parsing.rs          ← tests de parsing
├── evaluation.rs       ← tests de evaluación
├── error_handling.rs   ← tests de manejo de errores
└── regression.rs       ← tests de bugs corregidos
```

```rust
// tests/parsing.rs
use mi_calculadora::parse;

#[test]
fn test_parse_addition() {
    let ast = parse("2 + 3").unwrap();
    assert_eq!(ast.to_string(), "(+ 2 3)");
}

#[test]
fn test_parse_invalid() {
    let result = parse("+ + +");
    assert!(result.is_err());
}
```

```rust
// tests/evaluation.rs
use mi_calculadora::{parse, evaluate};

#[test]
fn test_evaluate_addition() {
    let ast = parse("2 + 3").unwrap();
    let result = evaluate(&ast).unwrap();
    assert_eq!(result, 5);
}

#[test]
fn test_evaluate_complex() {
    let ast = parse("(2 + 3) * 4").unwrap();
    let result = evaluate(&ast).unwrap();
    assert_eq!(result, 20);
}
```

```rust
// tests/regression.rs — bugs que corregiste
use mi_calculadora::{parse, evaluate};

#[test]
fn test_issue_42_division_by_zero() {
    // Bug #42: dividir por cero causaba panic en vez de Error
    let ast = parse("10 / 0").unwrap();
    let result = evaluate(&ast);
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("division by zero"));
}

#[test]
fn test_issue_57_negative_numbers() {
    // Bug #57: parser no aceptaba números negativos
    let ast = parse("-5 + 3").unwrap();
    let result = evaluate(&ast).unwrap();
    assert_eq!(result, -2);
}
```

### Ventajas de múltiples archivos

- Cada archivo se compila **en paralelo** (más rápido).
- `cargo test --test parsing` ejecuta solo un subconjunto.
- La organización documenta qué área cubre cada grupo de tests.

---

## 5. Módulos compartidos entre tests

Si varios archivos de test necesitan helpers comunes (funciones, datos de prueba), **no** puedes simplemente crear un archivo en `tests/` porque cada archivo `.rs` se trata como un crate de test independiente.

### El problema

```text
tests/
├── helpers.rs    ← esto se compila como su propio test crate
├── parsing.rs    ← no puede hacer `mod helpers;`
└── evaluation.rs
```

`helpers.rs` sería compilado como un test crate (Cargo lo ejecutaría y reportaría "0 tests"). No es un módulo compartido.

### La solución: subdirectorio con mod.rs

```text
tests/
├── common/
│   └── mod.rs       ← módulo compartido (NO un test crate)
├── parsing.rs
└── evaluation.rs
```

```rust
// tests/common/mod.rs
use mi_calculadora::{parse, evaluate};

pub fn parse_and_eval(input: &str) -> i64 {
    let ast = parse(input).unwrap();
    evaluate(&ast).unwrap()
}

pub fn assert_eval_eq(input: &str, expected: i64) {
    let result = parse_and_eval(input);
    assert_eq!(result, expected, "eval('{}') = {}, expected {}", input, result, expected);
}

pub fn sample_expressions() -> Vec<(&'static str, i64)> {
    vec![
        ("1 + 1", 2),
        ("10 - 3", 7),
        ("4 * 5", 20),
        ("15 / 3", 5),
    ]
}
```

```rust
// tests/parsing.rs
mod common;

use mi_calculadora::parse;

#[test]
fn test_all_sample_expressions_parse() {
    for (expr, _) in common::sample_expressions() {
        assert!(parse(expr).is_ok(), "Failed to parse: {}", expr);
    }
}
```

```rust
// tests/evaluation.rs
mod common;

#[test]
fn test_all_sample_expressions_evaluate() {
    for (expr, expected) in common::sample_expressions() {
        common::assert_eval_eq(expr, expected);
    }
}

#[test]
fn test_complex_expression() {
    common::assert_eval_eq("(2 + 3) * (4 - 1)", 15);
}
```

### ¿Por qué el subdirectorio?

Cargo trata cada archivo `.rs` directamente en `tests/` como un test crate. Pero los archivos dentro de **subdirectorios** no reciben este tratamiento — son módulos normales que puedes importar con `mod`.

```text
tests/
├── helpers.rs          ← Cargo lo ejecuta como test crate (NO queremos esto)
├── common/
│   └── mod.rs          ← Cargo lo IGNORA (no está directamente en tests/)
├── parsing.rs          ← Test crate: puede hacer `mod common;`
└── evaluation.rs       ← Test crate: puede hacer `mod common;`
```

---

## 6. #[ignore]: tests lentos y condicionales

### Marcar tests como ignorados

```rust
#[test]
#[ignore]
fn test_heavy_computation() {
    // Este test tarda 30 segundos
    let result = compute_primes_up_to(10_000_000);
    assert!(result.len() > 600_000);
}

#[test]
#[ignore = "requires database connection"]
fn test_database_query() {
    let conn = Database::connect("postgres://localhost/test").unwrap();
    let users = conn.query("SELECT * FROM users").unwrap();
    assert!(!users.is_empty());
}
```

### Ejecutar tests ignorados

```bash
# Normal: ejecuta solo tests NO ignorados
cargo test
# test test_heavy_computation ... ignored
# test test_database_query ... ignored

# Solo los ignorados:
cargo test -- --ignored

# Todos (incluyendo ignorados):
cargo test -- --include-ignored
```

### Cuándo usar #[ignore]

```rust
// 1. Tests que requieren infraestructura externa
#[test]
#[ignore = "requires Redis running on localhost:6379"]
fn test_redis_cache() { /* ... */ }

// 2. Tests muy lentos
#[test]
#[ignore = "takes ~60 seconds"]
fn test_full_simulation() { /* ... */ }

// 3. Tests que solo funcionan en ciertos entornos
#[test]
#[ignore = "requires root privileges"]
fn test_bind_port_80() { /* ... */ }

// 4. Tests de performance/stress
#[test]
#[ignore = "stress test, run manually"]
fn test_handle_million_requests() { /* ... */ }
```

### Patrón CI: ejecutar ignored en un paso separado

```yaml
# .github/workflows/test.yml
jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - run: cargo test  # tests rápidos

  integration-tests:
    runs-on: ubuntu-latest
    services:
      postgres: ...
      redis: ...
    steps:
      - run: cargo test -- --ignored  # tests que requieren infra
```

---

## 7. Setup y teardown en integración

### RAII para cleanup automático

```rust
// tests/file_operations.rs
use std::fs;
use std::path::Path;

fn create_test_file(name: &str, content: &str) -> String {
    let path = format!("/tmp/test_{}", name);
    fs::write(&path, content).unwrap();
    path
}

fn cleanup(path: &str) {
    let _ = fs::remove_file(path);  // ignorar error si no existe
}

#[test]
fn test_read_file() {
    let path = create_test_file("read", "hello world");

    let result = mi_proyecto::read_and_process(&path);
    assert_eq!(result.unwrap(), "HELLO WORLD");

    cleanup(&path);
}
```

### Mejor: usar tempfile para cleanup automático

```rust
// tests/file_operations.rs
use tempfile::NamedTempFile;
use std::io::Write;

#[test]
fn test_process_file() {
    // Crea archivo temporal que se borra automáticamente al salir del scope
    let mut file = NamedTempFile::new().unwrap();
    writeln!(file, "line 1").unwrap();
    writeln!(file, "line 2").unwrap();

    let result = mi_proyecto::count_lines(file.path()).unwrap();
    assert_eq!(result, 2);
    // file se dropea aquí → archivo temporal borrado
}
```

```toml
# Cargo.toml
[dev-dependencies]
tempfile = "3"
```

### Struct guard para recursos

```rust
// tests/common/mod.rs
pub struct TestServer {
    pub url: String,
    handle: std::thread::JoinHandle<()>,
}

impl TestServer {
    pub fn start() -> Self {
        let (tx, rx) = std::sync::mpsc::channel();
        let handle = std::thread::spawn(move || {
            // Iniciar servidor de prueba
            let url = "http://127.0.0.1:0"; // puerto aleatorio
            tx.send(url.to_string()).unwrap();
            // ... servir requests
        });
        let url = rx.recv().unwrap();
        TestServer { url, handle }
    }
}

impl Drop for TestServer {
    fn drop(&mut self) {
        // Cleanup: detener el servidor
    }
}
```

```rust
// tests/api_tests.rs
mod common;

#[test]
fn test_api_endpoint() {
    let server = common::TestServer::start();

    let response = mi_proyecto::client::get(&format!("{}/health", server.url));
    assert_eq!(response.status(), 200);

    // server se dropea → cleanup automático
}
```

---

## 8. Tests de integración en binary crates

Los tests de integración **solo funcionan con library crates** (`lib.rs`). No puedes testear un binary crate (`main.rs`) desde `tests/`:

```rust
// Si solo tienes src/main.rs:
// tests/test.rs
// use mi_app::something;  // ERROR: no hay library crate para importar
```

### La solución: patrón thin binary

Mueve la lógica a `lib.rs` y haz que `main.rs` sea un wrapper delgado:

```rust
// src/lib.rs — toda la lógica
pub fn run(args: &[String]) -> Result<String, Box<dyn std::error::Error>> {
    if args.len() < 2 {
        return Err("Usage: program <input>".into());
    }
    let input = &args[1];
    Ok(format!("Processed: {}", input.to_uppercase()))
}
```

```rust
// src/main.rs — solo punto de entrada
fn main() {
    let args: Vec<String> = std::env::args().collect();
    match mi_proyecto::run(&args) {
        Ok(output) => println!("{}", output),
        Err(e) => {
            eprintln!("Error: {}", e);
            std::process::exit(1);
        }
    }
}
```

```rust
// tests/cli_test.rs — ahora puedes testear la lógica
use mi_proyecto::run;

#[test]
fn test_run_with_input() {
    let args = vec!["program".to_string(), "hello".to_string()];
    let result = run(&args).unwrap();
    assert_eq!(result, "Processed: HELLO");
}

#[test]
fn test_run_without_input() {
    let args = vec!["program".to_string()];
    let result = run(&args);
    assert!(result.is_err());
}
```

### Alternativa: testear el binario como proceso

Si necesitas testear el binario real (con stdin/stdout/exit codes):

```rust
// tests/binary_test.rs
use std::process::Command;

#[test]
fn test_binary_output() {
    let output = Command::new("cargo")
        .args(["run", "--", "hello"])
        .output()
        .expect("Failed to execute");

    assert!(output.status.success());
    let stdout = String::from_utf8(output.stdout).unwrap();
    assert!(stdout.contains("Processed: HELLO"));
}

#[test]
fn test_binary_error() {
    let output = Command::new("cargo")
        .args(["run"])
        .output()
        .expect("Failed to execute");

    assert!(!output.status.success());
}
```

El crate `assert_cmd` simplifica este patrón:

```toml
[dev-dependencies]
assert_cmd = "2"
predicates = "3"
```

```rust
use assert_cmd::Command;
use predicates::prelude::*;

#[test]
fn test_with_assert_cmd() {
    Command::cargo_bin("mi_proyecto")
        .unwrap()
        .arg("hello")
        .assert()
        .success()
        .stdout(predicate::str::contains("HELLO"));
}
```

---

## 9. Tests de integración en workspaces

En un workspace, cada crate miembro tiene su propio directorio `tests/`:

```text
mi_workspace/
├── Cargo.toml
├── crates/
│   ├── core/
│   │   ├── src/lib.rs
│   │   └── tests/
│   │       └── core_test.rs    ← tests de core
│   ├── api/
│   │   ├── src/main.rs
│   │   └── tests/
│   │       └── api_test.rs     ← tests de api (si tiene lib.rs)
│   └── shared/
│       ├── src/lib.rs
│       └── tests/
│           └── shared_test.rs  ← tests de shared
```

### Ejecutar tests

```bash
# Todos los tests de todo el workspace:
cargo test --workspace

# Tests de un miembro específico:
cargo test -p core

# Solo tests de integración de un miembro:
cargo test -p core --test core_test

# Tests de integración del miembro, con un filtro:
cargo test -p core --test core_test test_parse
```

### Tests de integración que dependen de múltiples crates

```rust
// crates/api/tests/integration.rs
use api::create_app;
use core::models::User;
use shared::test_utils;

#[test]
fn test_full_flow() {
    let app = create_app();
    let user = User::new("Alice", "alice@example.com");
    // Testear un flujo que involucra múltiples crates
}
```

Para que esto funcione, `api/Cargo.toml` debe listar `core` y `shared` como dependencias (o dev-dependencies):

```toml
# crates/api/Cargo.toml
[dependencies]
core = { path = "../core" }

[dev-dependencies]
shared = { path = "../shared" }
```

---

## 10. Errores comunes

### Error 1: Crear tests de integración para un binary-only crate

```text
mi_app/
├── src/
│   └── main.rs        ← solo binary, sin lib.rs
└── tests/
    └── test.rs        ← no puede importar nada del crate
```

```rust
// tests/test.rs
use mi_app::process;  // ERROR: can't find crate `mi_app`
```

**Solución**: crea `lib.rs` con la lógica y deja `main.rs` como wrapper. Ver sección 8.

### Error 2: Poner helpers como archivo directo en tests/

```text
tests/
├── helpers.rs     ← Cargo lo trata como test crate
├── test_a.rs
└── test_b.rs
```

```bash
$ cargo test
     Running tests/helpers.rs
running 0 tests  # "ejecuta" helpers como tests (confuso)
```

**Solución**: usar subdirectorio `tests/common/mod.rs`. Ver sección 5.

### Error 3: Intentar acceder a funciones privadas

```rust
// tests/internal.rs
use mi_crate::internal_function;
// ERROR: function `internal_function` is private
```

Los tests de integración **no pueden** ver items privados. Eso es por diseño — testean la API pública.

**Solución**: si necesitas testear algo privado, usa unit tests (dentro de `src/`, con `#[cfg(test)]`).

### Error 4: Confundir dev-dependencies con dependencies

```toml
# Cargo.toml
[dev-dependencies]
tempfile = "3"       # disponible en tests e integration tests
serde_json = "1.0"   # disponible en tests

[dependencies]
serde = "1.0"        # disponible siempre
```

Las `[dev-dependencies]` solo están disponibles para tests y benchmarks, no para el código en `src/`. Si intentas usar `tempfile` en `lib.rs`, no compilará.

### Error 5: Tests de integración que no son independientes

```rust
// tests/sequential.rs

#[test]
fn test_step_1_create() {
    // Crea un archivo
    std::fs::write("/tmp/test_state", "created").unwrap();
}

#[test]
fn test_step_2_read() {
    // PELIGRO: asume que test_step_1 ya se ejecutó
    let content = std::fs::read_to_string("/tmp/test_state").unwrap();
    assert_eq!(content, "created");
}
```

Los tests se ejecutan en paralelo y sin orden garantizado. Cada test debe ser autónomo.

---

## 11. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                 TESTS DE INTEGRACIÓN CHEATSHEET                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA                                                        ║
║  proyecto/                                                         ║
║  ├── src/lib.rs               Código del crate                     ║
║  └── tests/                                                        ║
║      ├── test_a.rs            Cada .rs = crate de test separado    ║
║      ├── test_b.rs            Solo ve API pública (pub)            ║
║      └── common/                                                   ║
║          └── mod.rs           Helpers compartidos (no es test)      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESCRIBIR                                                          ║
║  use mi_crate::item;          Importar como crate externo          ║
║  #[test]                      Marcar función como test             ║
║  fn test_name() { ... }       Sin #[cfg(test)], sin mod tests      ║
║  mod common;                  Importar helpers compartidos         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EJECUTAR                                                          ║
║  cargo test                    Todos (unit + integration + doc)    ║
║  cargo test --test archivo     Solo un archivo de integración      ║
║  cargo test --test arch nombre Filtrar dentro de un archivo        ║
║  cargo test -- --ignored       Solo tests con #[ignore]            ║
║  cargo test -- --include-ignored  Todos incluyendo ignorados       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  #[ignore]                                                         ║
║  #[ignore]                     Saltar este test                    ║
║  #[ignore = "reason"]          Con motivo documentado              ║
║  cargo test -- --ignored       Ejecutar solo los ignorados         ║
║  Usos: tests lentos, requiere DB, requiere root, stress tests     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  HELPERS COMPARTIDOS                                               ║
║  tests/common/mod.rs           Módulo compartido                   ║
║  (NO tests/helpers.rs)         .rs directo = otro test crate       ║
║  mod common; en cada test      Importar el módulo                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DEPENDENCIAS DE TEST                                              ║
║  [dev-dependencies]            Solo para tests y benchmarks        ║
║  tempfile, assert_cmd          Crates comunes para testing         ║
║  predicates                    Assertions expresivas               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS                                                            ║
║  ✓ Solo library crates (necesitas lib.rs)                          ║
║  ✓ Solo API pública visible                                        ║
║  ✓ Cada test independiente y autónomo                              ║
║  ✓ RAII para cleanup (tempfile, Drop guards)                       ║
║  ✗ No acceso a funciones privadas                                  ║
║  ✗ No depender del orden de ejecución                              ║
║  ✗ No poner helpers como .rs directo en tests/                     ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Crear tests de integración

Dada esta library, crea tests de integración en `tests/`:

```rust
// src/lib.rs
pub struct TodoList {
    items: Vec<Todo>,
}

pub struct Todo {
    pub title: String,
    pub done: bool,
}

impl TodoList {
    pub fn new() -> Self {
        TodoList { items: Vec::new() }
    }

    pub fn add(&mut self, title: &str) {
        self.items.push(Todo {
            title: title.to_string(),
            done: false,
        });
    }

    pub fn complete(&mut self, index: usize) -> Result<(), String> {
        match self.items.get_mut(index) {
            Some(todo) => {
                todo.done = true;
                Ok(())
            }
            None => Err(format!("No todo at index {}", index)),
        }
    }

    pub fn pending(&self) -> Vec<&Todo> {
        self.items.iter().filter(|t| !t.done).collect()
    }

    pub fn all(&self) -> &[Todo] {
        &self.items
    }

    pub fn len(&self) -> usize {
        self.items.len()
    }

    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }
}
```

**Estructura esperada:**

```text
tests/
├── common/
│   └── mod.rs          ← helper: crea un TodoList con datos de prueba
├── basic.rs            ← tests de operaciones básicas (add, len, is_empty)
└── completion.rs       ← tests de complete, pending, flujos completos
```

**Escribe al menos 8 tests que cubran:**
1. Lista vacía inicial
2. Añadir items
3. Completar un item existente
4. Completar un índice inválido (error)
5. Filtrar pendientes
6. Flujo completo: crear → añadir 3 → completar 1 → verificar pending
7. Usar el helper de `common/mod.rs` en ambos archivos de test
8. Un test marcado con `#[ignore]`

### Ejercicio 2: Refactorizar binary a testable

Este programa solo tiene `main.rs`. Refactorízalo para que sea testable con integration tests:

```rust
// src/main.rs (estado actual)
use std::collections::HashMap;

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 {
        eprintln!("Usage: wordcount <text>");
        std::process::exit(1);
    }

    let text = &args[1];
    let mut counts: HashMap<&str, usize> = HashMap::new();

    for word in text.split_whitespace() {
        *counts.entry(word).or_insert(0) += 1;
    }

    let mut sorted: Vec<_> = counts.into_iter().collect();
    sorted.sort_by(|a, b| b.1.cmp(&a.1));

    for (word, count) in &sorted {
        println!("{}: {}", word, count);
    }
}
```

**Tareas:**
1. Crea `lib.rs` con funciones `count_words` y `format_counts`.
2. Simplifica `main.rs` para que solo parsee args y llame a la library.
3. Crea `tests/wordcount.rs` con tests de integración.
4. ¿Necesitas `dev-dependencies` para estos tests?

### Ejercicio 3: Tests con #[ignore] y CI

Diseña una estrategia de testing para un proyecto con estos tipos de tests:

```text
Categoría A: Tests unitarios rápidos (<1s cada uno) — 50 tests
Categoría B: Tests de integración que necesitan filesystem — 10 tests
Categoría C: Tests de integración que necesitan PostgreSQL — 5 tests
Categoría D: Tests de stress/performance que tardan >30s — 3 tests
```

**Preguntas:**
1. ¿Qué categorías marcarías con `#[ignore]`?
2. ¿Cómo organizarías los archivos en `tests/`?
3. Escribe los comandos `cargo test` para ejecutar:
   - Solo tests rápidos (A + B)
   - Solo tests que necesitan DB (C)
   - Todo incluyendo stress tests (A + B + C + D)
4. Escribe un workflow de CI (pseudocódigo) con dos jobs: "fast" y "full".
5. ¿Usarías `#[ignore]` o archivos separados para distinguir categorías?

---

> **Nota**: los tests de integración son tu garantía de que la API pública funciona como prometes. Mientras los unit tests verifican los engranajes internos, los integration tests verifican que la máquina completa produce el resultado correcto. Juntos forman una red de seguridad que te permite refactorizar con confianza — si los tests de integración pasan, los usuarios de tu crate no notarán cambios internos.
