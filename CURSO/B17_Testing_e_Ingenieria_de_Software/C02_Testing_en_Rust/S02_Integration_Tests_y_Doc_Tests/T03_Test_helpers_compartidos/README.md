# T03 - Test Helpers Compartidos

## Índice

1. [El problema: código duplicado entre tests](#1-el-problema-código-duplicado-entre-tests)
2. [Qué son los test helpers](#2-qué-son-los-test-helpers)
3. [tests/common/mod.rs: el patrón estándar](#3-testscommonmodrs-el-patrón-estándar)
4. [Anatomía de un módulo common completo](#4-anatomía-de-un-módulo-common-completo)
5. [Submódulos dentro de common/](#5-submódulos-dentro-de-common)
6. [Helpers para unit tests (dentro de src/)](#6-helpers-para-unit-tests-dentro-de-src)
7. [Fixtures: datos de prueba reutilizables](#7-fixtures-datos-de-prueba-reutilizables)
8. [Builders para datos de test complejos](#8-builders-para-datos-de-test-complejos)
9. [RAII fixtures: setup con cleanup automático](#9-raii-fixtures-setup-con-cleanup-automático)
10. [Custom assertions](#10-custom-assertions)
11. [Trait para test doubles: fakes y stubs](#11-trait-para-test-doubles-fakes-y-stubs)
12. [Archivos de fixtures externos](#12-archivos-de-fixtures-externos)
13. [Test utilities como crate interna](#13-test-utilities-como-crate-interna)
14. [Helpers compartidos entre unit e integration tests](#14-helpers-compartidos-entre-unit-e-integration-tests)
15. [Generadores de datos aleatorios para tests](#15-generadores-de-datos-aleatorios-para-tests)
16. [Macros como helpers de test](#16-macros-como-helpers-de-test)
17. [Organización según tamaño del proyecto](#17-organización-según-tamaño-del-proyecto)
18. [Comparación con C y Go](#18-comparación-con-c-y-go)
19. [Errores comunes](#19-errores-comunes)
20. [Ejemplo completo: test infrastructure para un servicio](#20-ejemplo-completo-test-infrastructure-para-un-servicio)
21. [Programa de práctica](#21-programa-de-práctica)
22. [Ejercicios](#22-ejercicios)

---

## 1. El problema: código duplicado entre tests

A medida que un proyecto crece, los tests empiezan a repetir patrones. Los primeros tests son simples y autocontenidos, pero después aparece la duplicación:

```rust
// tests/user_creation.rs
use mi_app::{Database, User};

#[test]
fn test_create_user() {
    // Setup repetitivo ─────────────────────────────────┐
    let db = Database::new_in_memory("test").unwrap();  //│
    db.run_migrations().unwrap();                        //│
    db.seed_defaults().unwrap();                         //│
    // ──────────────────────────────────────────────────┘
    let user = User::create(&db, "Alice", "alice@test.com").unwrap();
    assert_eq!(user.name(), "Alice");
}

#[test]
fn test_create_duplicate_user() {
    // Setup repetitivo (IDÉNTICO) ──────────────────────┐
    let db = Database::new_in_memory("test").unwrap();  //│
    db.run_migrations().unwrap();                        //│
    db.seed_defaults().unwrap();                         //│
    // ──────────────────────────────────────────────────┘
    User::create(&db, "Alice", "alice@test.com").unwrap();
    let result = User::create(&db, "Alice", "alice@test.com");
    assert!(result.is_err());
}
```

```rust
// tests/user_queries.rs
use mi_app::{Database, User};

#[test]
fn test_find_user() {
    // Setup repetitivo (IDÉNTICO, OTRO ARCHIVO) ────────┐
    let db = Database::new_in_memory("test").unwrap();  //│
    db.run_migrations().unwrap();                        //│
    db.seed_defaults().unwrap();                         //│
    // ──────────────────────────────────────────────────┘
    User::create(&db, "Alice", "alice@test.com").unwrap();
    let found = User::find_by_email(&db, "alice@test.com").unwrap();
    assert_eq!(found.name(), "Alice");
}
```

```
Problema de duplicación en tests:

  tests/user_creation.rs  ──┐
  tests/user_queries.rs   ──┤── Todos repiten las mismas 3 líneas de setup
  tests/user_deletion.rs  ──┤── Todos repiten los mismos datos de prueba
  tests/admin_tests.rs    ──┤── Todos repiten las mismas validaciones
  tests/auth_tests.rs     ──┘

  Consecuencias:
  ├─ Si cambia la firma de Database::new_in_memory → 5 archivos que corregir
  ├─ Si se agrega un paso de setup → 5 archivos que actualizar
  ├─ Si un dato de prueba cambia → buscar en todos los archivos
  └─ Inconsistencia: un archivo tiene setup actualizado, otros no
```

Los test helpers resuelven esto exactamente: extraen el código compartido a un lugar central.

---

## 2. Qué son los test helpers

Un test helper es cualquier código que **ayuda a escribir tests** pero **no es un test en sí mismo**. No tiene `#[test]`, no verifica nada directamente — su propósito es reducir la fricción de escribir tests claros y mantenibles:

```
Tipos de test helpers:

  ┌─────────────────────────────────────────────────────┐
  │                  Test Helpers                        │
  │                                                     │
  │  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  │
  │  │  Setup        │  │  Fixtures     │  │  Custom  │  │
  │  │  functions    │  │  (datos de    │  │  asserts │  │
  │  │              │  │   prueba)     │  │          │  │
  │  │  setup_db()  │  │  valid_user() │  │ assert_  │  │
  │  │  new_server()│  │  bad_input()  │  │ sorted() │  │
  │  └──────────────┘  └──────────────┘  └──────────┘  │
  │                                                     │
  │  ┌──────────────┐  ┌──────────────┐  ┌──────────┐  │
  │  │  Builders     │  │  RAII         │  │  Fakes/  │  │
  │  │  (datos      │  │  fixtures     │  │  Stubs   │  │
  │  │   complejos) │  │  (cleanup     │  │          │  │
  │  │              │  │   automático) │  │  FakeDb  │  │
  │  │ UserBuilder  │  │  TestDir      │  │ StubApi  │  │
  │  │  ::new()     │  │  TestServer   │  │          │  │
  │  │  .name("X")  │  │              │  │          │  │
  │  │  .build()    │  │              │  │          │  │
  │  └──────────────┘  └──────────────┘  └──────────┘  │
  │                                                     │
  │  ┌──────────────┐  ┌──────────────┐                │
  │  │  Macros       │  │  Generators  │                │
  │  │  de test      │  │  (datos      │                │
  │  │              │  │   aleatorios) │                │
  │  │ assert_      │  │              │                │
  │  │ approx_eq!() │  │ random_user()│                │
  │  └──────────────┘  └──────────────┘                │
  └─────────────────────────────────────────────────────┘
```

### Dónde viven los helpers

```
Helpers para integration tests:
  tests/
  ├── common/
  │   └── mod.rs          ← Aquí
  ├── test_a.rs           ← Usa: mod common;
  └── test_b.rs           ← Usa: mod common;

Helpers para unit tests:
  src/
  ├── lib.rs
  ├── module.rs
  └── test_helpers.rs     ← #[cfg(test)] pub(crate) helpers
                             O directamente dentro de mod tests {}

Helpers compartidos (unit + integration):
  Opción A: crate separada en workspace
  Opción B: #[cfg(test)] + pub(crate) en src/
            (solo funciona para unit tests)
```

---

## 3. tests/common/mod.rs: el patrón estándar

En T01 (Integration tests) vimos brevemente este patrón. Aquí lo profundizamos.

### Por qué `common/mod.rs` y no `common.rs`

```
Regla de Cargo para el directorio tests/:

  Archivos .rs directamente en tests/ → Se compilan como crates de test
  Archivos en subdirectorios de tests/ → NO se compilan automáticamente

  tests/
  ├── common.rs       ← ✗ Cargo lo compila como crate de test
  │                       (aparece como "Running tests/common.rs" con 0 tests)
  │
  ├── common/
  │   └── mod.rs      ← ✓ Cargo lo IGNORA (está en subdirectorio)
  │                       Solo se compila cuando otro archivo hace `mod common;`
  │
  ├── test_a.rs       ← Crate de test
  └── test_b.rs       ← Crate de test
```

### Uso básico

```rust
// tests/common/mod.rs
use mi_app::Database;

pub fn setup_db() -> Database {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    db.seed_defaults().unwrap();
    db
}
```

```rust
// tests/test_a.rs
mod common;  // ← Importa tests/common/mod.rs

use mi_app::User;

#[test]
fn test_something() {
    let db = common::setup_db();  // ← Usa el helper
    let user = User::create(&db, "Alice", "alice@test.com").unwrap();
    assert_eq!(user.name(), "Alice");
}
```

### Cómo funciona `mod common;`

```
Cuando un archivo de integration test dice `mod common;`:

  1. Rust busca `common/mod.rs` relativo al archivo actual
     (tests/test_a.rs busca tests/common/mod.rs)

  2. El contenido de common/mod.rs se incluye como un SUBMÓDULO
     del crate de test

  3. Los items pub de common son accesibles como common::item

  Diagrama de resolución:

  tests/test_a.rs          tests/common/mod.rs
  ┌──────────────────┐     ┌──────────────────────┐
  │ mod common;      │────→│ pub fn setup_db() {}  │
  │                  │     │ pub fn fixture() {}   │
  │ // Ahora:        │     └──────────────────────┘
  │ // common::      │
  │ //   setup_db()  │
  │ //   fixture()   │
  └──────────────────┘
```

### Cada archivo de test importa independientemente

```rust
// tests/test_a.rs
mod common;  // ← CADA archivo debe importar por separado

#[test]
fn test_a() {
    let db = common::setup_db();
    // ...
}
```

```rust
// tests/test_b.rs
mod common;  // ← No comparte el `mod common` de test_a.rs
             //   (son crates separados, cada uno hace su propia importación)

#[test]
fn test_b() {
    let db = common::setup_db();
    // ...
}
```

Esto no es duplicación — es una declaración de módulo. Cada crate de test necesita declarar que quiere usar el módulo `common`. El código real solo existe una vez en `common/mod.rs`.

### Warning de dead code

Un problema frecuente: si un helper solo se usa en **algunos** archivos de test, los archivos que no lo usan pueden generar warnings:

```
warning: function `special_helper` is never used
 --> tests/common/mod.rs:15:8
  |
15 | pub fn special_helper() { }
  |        ^^^^^^^^^^^^^^
```

Esto ocurre porque cada crate de test importa `mod common;` y compila todo `common/mod.rs`, pero quizás no usa todos los helpers. Solución:

```rust
// tests/common/mod.rs
#![allow(dead_code)]  // ← Silenciar warnings de helpers no usados

pub fn setup_db() -> Database { /* ... */ }
pub fn special_helper() { /* ... */ }  // No genera warning aunque no se use
```

---

## 4. Anatomía de un módulo common completo

Un módulo common bien estructurado para un proyecto de tamaño mediano:

```rust
// tests/common/mod.rs

#![allow(dead_code)]

use mi_app::{Database, User, Config, Role};
use std::collections::HashMap;

// ═══════════════════════════════════════
// SETUP FUNCTIONS
// ═══════════════════════════════════════

/// Base de datos vacía con migraciones aplicadas.
pub fn empty_db() -> Database {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    db
}

/// Base de datos con datos de seed (roles, configuración default).
pub fn seeded_db() -> Database {
    let db = empty_db();
    db.seed_defaults().unwrap();
    db
}

/// Base de datos con usuarios de prueba pre-cargados.
pub fn populated_db() -> Database {
    let db = seeded_db();
    for (name, email, role) in sample_users() {
        User::create(&db, name, email)
            .unwrap()
            .assign_role(&db, role)
            .unwrap();
    }
    db
}

/// Configuración de test con valores predecibles.
pub fn test_config() -> Config {
    Config::builder()
        .database_url("memory://test")
        .log_level("error")
        .max_retries(0)
        .timeout_secs(5)
        .build()
}

// ═══════════════════════════════════════
// FIXTURES (datos de prueba)
// ═══════════════════════════════════════

/// Usuarios de prueba estándar: (nombre, email, rol).
pub fn sample_users() -> Vec<(&'static str, &'static str, Role)> {
    vec![
        ("Alice", "alice@test.com", Role::Admin),
        ("Bob", "bob@test.com", Role::User),
        ("Charlie", "charlie@test.com", Role::User),
        ("Diana", "diana@test.com", Role::Moderator),
    ]
}

/// Un mapa de campo → valor para crear un usuario válido.
pub fn valid_user_data() -> HashMap<String, String> {
    let mut data = HashMap::new();
    data.insert("name".into(), "TestUser".into());
    data.insert("email".into(), "test@example.com".into());
    data.insert("age".into(), "25".into());
    data
}

/// Datos de usuario inválidos para probar validación.
pub fn invalid_user_data() -> HashMap<String, String> {
    let mut data = HashMap::new();
    data.insert("name".into(), "".into());
    data.insert("email".into(), "not-an-email".into());
    data.insert("age".into(), "-5".into());
    data
}

// ═══════════════════════════════════════
// CUSTOM ASSERTIONS
// ═══════════════════════════════════════

/// Verifica que un usuario tiene los campos esperados.
pub fn assert_user_matches(user: &User, name: &str, email: &str) {
    assert_eq!(
        user.name(), name,
        "Expected name '{}', got '{}'",
        name, user.name()
    );
    assert_eq!(
        user.email(), email,
        "Expected email '{}', got '{}'",
        email, user.email()
    );
}

/// Verifica que un Result es Err y el mensaje contiene el texto dado.
pub fn assert_error_contains<T: std::fmt::Debug>(
    result: &Result<T, String>,
    expected_text: &str,
) {
    match result {
        Ok(val) => panic!(
            "Expected error containing '{}', got Ok({:?})",
            expected_text, val
        ),
        Err(msg) => assert!(
            msg.contains(expected_text),
            "Error '{}' does not contain '{}'",
            msg, expected_text
        ),
    }
}

/// Verifica que una lista está ordenada.
pub fn assert_sorted<T: Ord + std::fmt::Debug>(items: &[T]) {
    for window in items.windows(2) {
        assert!(
            window[0] <= window[1],
            "Not sorted: {:?} > {:?}",
            window[0], window[1]
        );
    }
}

// ═══════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════

/// Genera un email único para evitar colisiones entre tests.
pub fn unique_email(prefix: &str) -> String {
    use std::sync::atomic::{AtomicU64, Ordering};
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let id = COUNTER.fetch_add(1, Ordering::SeqCst);
    format!("{}_{}_{}@test.com", prefix, std::process::id(), id)
}

/// Genera un nombre temporal único para recursos.
pub fn unique_name(prefix: &str) -> String {
    use std::sync::atomic::{AtomicU64, Ordering};
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let id = COUNTER.fetch_add(1, Ordering::SeqCst);
    format!("{}_{}_{}", prefix, std::process::id(), id)
}
```

```rust
// tests/user_tests.rs
mod common;

use mi_app::User;

#[test]
fn test_create_user() {
    let db = common::seeded_db();
    let email = common::unique_email("create");

    let user = User::create(&db, "TestUser", &email).unwrap();
    common::assert_user_matches(&user, "TestUser", &email);
}

#[test]
fn test_create_duplicate_fails() {
    let db = common::seeded_db();
    let email = common::unique_email("dup");

    User::create(&db, "First", &email).unwrap();
    let result = User::create(&db, "Second", &email);
    common::assert_error_contains(&result, "duplicate");
}

#[test]
fn test_list_users_sorted() {
    let db = common::populated_db();
    let users = User::list_all(&db).unwrap();
    let names: Vec<&str> = users.iter().map(|u| u.name()).collect();
    common::assert_sorted(&names);
}
```

---

## 5. Submódulos dentro de common/

Cuando `common/mod.rs` crece demasiado, se divide en submódulos temáticos:

### Estructura de submódulos

```
tests/
├── common/
│   ├── mod.rs            // Re-exporta submódulos
│   ├── db.rs             // Setup de base de datos
│   ├── fixtures.rs       // Datos de prueba
│   ├── assertions.rs     // Asserts personalizados
│   ├── builders.rs       // Builders para datos complejos
│   └── cleanup.rs        // RAII y cleanup
│
├── user_tests.rs
├── auth_tests.rs
└── report_tests.rs
```

### Implementación

```rust
// tests/common/mod.rs

#![allow(dead_code)]

pub mod db;
pub mod fixtures;
pub mod assertions;
pub mod builders;
pub mod cleanup;

// Re-exports de conveniencia para los helpers más usados
pub use db::{empty_db, seeded_db, populated_db};
pub use fixtures::{sample_users, valid_user_data};
pub use assertions::{assert_user_matches, assert_error_contains};
pub use builders::UserBuilder;
pub use cleanup::TestDir;
```

```rust
// tests/common/db.rs

use mi_app::Database;

pub fn empty_db() -> Database {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    db
}

pub fn seeded_db() -> Database {
    let db = empty_db();
    db.seed_defaults().unwrap();
    db
}

pub fn populated_db() -> Database {
    let db = seeded_db();
    for (name, email, role) in super::fixtures::sample_users() {
        mi_app::User::create(&db, name, email)
            .unwrap()
            .assign_role(&db, role)
            .unwrap();
    }
    db
}
```

```rust
// tests/common/fixtures.rs

use mi_app::Role;
use std::collections::HashMap;

pub fn sample_users() -> Vec<(&'static str, &'static str, Role)> {
    vec![
        ("Alice", "alice@test.com", Role::Admin),
        ("Bob", "bob@test.com", Role::User),
        ("Charlie", "charlie@test.com", Role::User),
    ]
}

pub fn valid_user_data() -> HashMap<String, String> {
    [
        ("name", "TestUser"),
        ("email", "test@example.com"),
        ("age", "25"),
    ]
    .iter()
    .map(|(k, v)| (k.to_string(), v.to_string()))
    .collect()
}

pub fn invalid_user_data() -> HashMap<String, String> {
    [
        ("name", ""),
        ("email", "bad"),
        ("age", "-1"),
    ]
    .iter()
    .map(|(k, v)| (k.to_string(), v.to_string()))
    .collect()
}

pub const LONG_STRING: &str = "a]";  // Se usaría: &"a".repeat(1000) en runtime

pub fn long_string(len: usize) -> String {
    "a".repeat(len)
}
```

```rust
// tests/common/assertions.rs

use mi_app::User;

pub fn assert_user_matches(user: &User, name: &str, email: &str) {
    assert_eq!(user.name(), name, "Name mismatch");
    assert_eq!(user.email(), email, "Email mismatch");
}

pub fn assert_error_contains<T: std::fmt::Debug>(
    result: &Result<T, String>,
    expected: &str,
) {
    match result {
        Ok(v) => panic!("Expected error containing '{}', got Ok({:?})", expected, v),
        Err(e) => assert!(
            e.contains(expected),
            "Error '{}' does not contain '{}'", e, expected
        ),
    }
}

pub fn assert_sorted<T: Ord + std::fmt::Debug>(items: &[T]) {
    for w in items.windows(2) {
        assert!(w[0] <= w[1], "Not sorted: {:?} > {:?}", w[0], w[1]);
    }
}

pub fn assert_contains_all<T: PartialEq + std::fmt::Debug>(
    haystack: &[T],
    needles: &[T],
) {
    for needle in needles {
        assert!(
            haystack.contains(needle),
            "{:?} not found in {:?}", needle, haystack
        );
    }
}
```

### Uso desde tests

```rust
// tests/user_tests.rs
mod common;

use mi_app::User;

#[test]
fn test_create() {
    let db = common::seeded_db();  // Re-export desde common/mod.rs
    // ...
}

#[test]
fn test_with_builder() {
    let db = common::seeded_db();
    let user_data = common::UserBuilder::new()
        .name("CustomUser")
        .email("custom@test.com")
        .build();
    // ...
}

#[test]
fn test_with_submodule_access() {
    // También se puede acceder al submódulo directamente
    let db = common::db::empty_db();
    let users = common::fixtures::sample_users();
    common::assertions::assert_sorted(&[1, 2, 3]);
}
```

---

## 6. Helpers para unit tests (dentro de src/)

Los unit tests viven en `src/` y no pueden importar `tests/common/mod.rs`. Necesitan sus propias soluciones para helpers compartidos:

### Patrón 1: Helpers dentro de `mod tests`

```rust
// src/parser.rs

pub fn parse(input: &str) -> Result<Ast, ParseError> {
    // ... implementación
    todo!()
}

#[cfg(test)]
mod tests {
    use super::*;

    // Helper local — solo disponible para tests de este módulo
    fn parse_ok(input: &str) -> Ast {
        parse(input).unwrap_or_else(|e| {
            panic!("Expected successful parse of '{}', got: {:?}", input, e)
        })
    }

    fn assert_ast_value(input: &str, expected: i32) {
        let ast = parse_ok(input);
        assert_eq!(
            ast.evaluate(), expected,
            "Parse('{}') evaluated to {}, expected {}",
            input, ast.evaluate(), expected
        );
    }

    #[test]
    fn test_simple_addition() {
        assert_ast_value("2 + 3", 5);
    }

    #[test]
    fn test_multiplication() {
        assert_ast_value("4 * 5", 20);
    }

    #[test]
    fn test_complex_expression() {
        assert_ast_value("(2 + 3) * 4", 20);
    }
}
```

### Patrón 2: Módulo de helpers compartido con #[cfg(test)]

```rust
// src/test_helpers.rs
//
// Este archivo completo solo se compila en modo test

#![cfg(test)]

use crate::{Database, User, Config};

pub fn test_db() -> Database {
    Database::new_in_memory("test").unwrap()
}

pub fn test_config() -> Config {
    Config::default()
}

pub fn test_user(db: &Database, name: &str) -> User {
    User::create(db, name, &format!("{}@test.com", name)).unwrap()
}
```

```rust
// src/lib.rs

pub mod database;
pub mod user;
pub mod config;

#[cfg(test)]
mod test_helpers;  // ← Solo existe en compilación de test
```

```rust
// src/user.rs

pub struct User { /* ... */ }

impl User {
    pub fn create(db: &Database, name: &str, email: &str) -> Result<Self, String> {
        // ...
        todo!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_helpers;  // ← Importa helpers del módulo compartido

    #[test]
    fn test_create_user() {
        let db = test_helpers::test_db();
        let user = test_helpers::test_user(&db, "Alice");
        assert_eq!(user.name(), "Alice");
    }
}
```

```rust
// src/database.rs

pub struct Database { /* ... */ }

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_helpers;  // ← Mismo módulo de helpers

    #[test]
    fn test_db_creation() {
        let db = test_helpers::test_db();
        assert!(db.is_connected());
    }
}
```

### Visibilidad de los helpers en src/

```
Helpers en src/test_helpers.rs:

  ┌──────────────────────────────────────────────┐
  │  #[cfg(test)]                                │
  │  mod test_helpers {                          │
  │      pub fn setup() { ... }                  │
  │  }                                           │
  │                                              │
  │  Accesible desde:                            │
  │    src/module_a.rs → crate::test_helpers ✓   │
  │    src/module_b.rs → crate::test_helpers ✓   │
  │    tests/test.rs   → NO ✗                    │
  │                    (es otro crate,            │
  │                     y #[cfg(test)] no         │
  │                     se activa para el .rlib   │
  │                     que los integration       │
  │                     tests enlazan)            │
  └──────────────────────────────────────────────┘
```

**Punto clave**: `#[cfg(test)]` en `src/` solo se activa cuando se compila el binario de unit tests. Los integration tests enlazan contra el `.rlib` compilado **sin** `cfg(test)`, así que no ven `test_helpers`.

---

## 7. Fixtures: datos de prueba reutilizables

Las fixtures son datos predefinidos que los tests usan como entrada. A diferencia del setup (que prepara el entorno), las fixtures son los **datos mismos**:

### Fixtures como funciones

```rust
// tests/common/mod.rs

use mi_app::{Order, Product, Address, PaymentMethod};

pub fn sample_product() -> Product {
    Product {
        id: 1,
        name: "Widget".to_string(),
        price_cents: 999,
        category: "gadgets".to_string(),
        in_stock: true,
    }
}

pub fn expensive_product() -> Product {
    Product {
        id: 2,
        name: "Luxury Item".to_string(),
        price_cents: 99999,
        category: "premium".to_string(),
        in_stock: true,
    }
}

pub fn out_of_stock_product() -> Product {
    Product {
        id: 3,
        name: "Rare Item".to_string(),
        price_cents: 4999,
        category: "collectibles".to_string(),
        in_stock: false,
    }
}

pub fn sample_address() -> Address {
    Address {
        street: "123 Test St".to_string(),
        city: "Testville".to_string(),
        state: "TS".to_string(),
        zip: "12345".to_string(),
        country: "US".to_string(),
    }
}

pub fn international_address() -> Address {
    Address {
        street: "456 Prueba Av".to_string(),
        city: "Madrid".to_string(),
        state: "".to_string(),
        zip: "28001".to_string(),
        country: "ES".to_string(),
    }
}

pub fn sample_order() -> Order {
    Order::new(
        vec![sample_product()],
        sample_address(),
        PaymentMethod::CreditCard,
    )
}

pub fn large_order() -> Order {
    let products = vec![
        sample_product(),
        expensive_product(),
        sample_product(),
        sample_product(),
    ];
    Order::new(products, sample_address(), PaymentMethod::CreditCard)
}
```

### Fixtures como constantes

Para datos simples, las constantes son más eficientes que funciones:

```rust
// tests/common/mod.rs

pub const VALID_EMAIL: &str = "user@example.com";
pub const INVALID_EMAIL: &str = "not-an-email";
pub const EMPTY_STRING: &str = "";
pub const LONG_NAME: &str = "A Very Long Name That Exceeds The Maximum Allowed Length For Names";

pub const MIN_AGE: u32 = 0;
pub const MAX_AGE: u32 = 150;
pub const TYPICAL_AGE: u32 = 30;

pub const VALID_PASSWORDS: &[&str] = &[
    "Str0ng!Pass",
    "An0ther@Valid",
    "C0mplex#123",
];

pub const WEAK_PASSWORDS: &[&str] = &[
    "123",
    "password",
    "abc",
    "",
];

// Para datos binarios
pub const SAMPLE_PNG_HEADER: &[u8] = &[0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
pub const SAMPLE_PDF_HEADER: &[u8] = b"%PDF-1.4";
```

```rust
// tests/validation_tests.rs
mod common;

use mi_app::validate_email;

#[test]
fn test_valid_email() {
    assert!(validate_email(common::VALID_EMAIL).is_ok());
}

#[test]
fn test_invalid_email() {
    assert!(validate_email(common::INVALID_EMAIL).is_err());
}

#[test]
fn test_all_weak_passwords_rejected() {
    for password in common::WEAK_PASSWORDS {
        assert!(
            mi_app::validate_password(password).is_err(),
            "Weak password '{}' should be rejected",
            password
        );
    }
}
```

### Fixtures parametrizadas

```rust
// tests/common/mod.rs

/// Casos de prueba para conversión de temperatura.
/// (celsius, fahrenheit, kelvin)
pub fn temperature_cases() -> Vec<(f64, f64, f64)> {
    vec![
        (0.0,    32.0,   273.15),   // Punto de congelación
        (100.0,  212.0,  373.15),   // Punto de ebullición
        (-40.0,  -40.0,  233.15),   // Punto de intersección C/F
        (37.0,   98.6,   310.15),   // Temperatura corporal
        (-273.15, -459.67, 0.0),    // Cero absoluto
    ]
}

/// Casos de prueba para parsing: (input, resultado_esperado).
pub fn parse_cases() -> Vec<(&'static str, Result<i32, &'static str>)> {
    vec![
        ("42",    Ok(42)),
        ("-17",   Ok(-17)),
        ("0",     Ok(0)),
        ("",      Err("empty")),
        ("abc",   Err("invalid")),
        ("99999999999", Err("overflow")),
    ]
}
```

```rust
// tests/temperature_tests.rs
mod common;

#[test]
fn test_celsius_to_fahrenheit() {
    for (celsius, fahrenheit, _kelvin) in common::temperature_cases() {
        let result = mi_app::c_to_f(celsius);
        assert!(
            (result - fahrenheit).abs() < 0.01,
            "c_to_f({}) = {}, expected {}",
            celsius, result, fahrenheit
        );
    }
}

#[test]
fn test_celsius_to_kelvin() {
    for (celsius, _fahrenheit, kelvin) in common::temperature_cases() {
        let result = mi_app::c_to_k(celsius);
        assert!(
            (result - kelvin).abs() < 0.01,
            "c_to_k({}) = {}, expected {}",
            celsius, result, kelvin
        );
    }
}
```

---

## 8. Builders para datos de test complejos

Cuando los datos de prueba tienen muchos campos, las fixtures fijas se vuelven rígidas. El patrón builder permite crear datos con valores default modificables:

### Builder básico

```rust
// tests/common/builders.rs

use mi_app::{User, Role, Address};

pub struct UserBuilder {
    name: String,
    email: String,
    age: u32,
    role: Role,
    active: bool,
    address: Option<Address>,
}

impl UserBuilder {
    pub fn new() -> Self {
        UserBuilder {
            name: "TestUser".to_string(),
            email: "test@example.com".to_string(),
            age: 25,
            role: Role::User,
            active: true,
            address: None,
        }
    }

    pub fn name(mut self, name: &str) -> Self {
        self.name = name.to_string();
        self
    }

    pub fn email(mut self, email: &str) -> Self {
        self.email = email.to_string();
        self
    }

    pub fn age(mut self, age: u32) -> Self {
        self.age = age;
        self
    }

    pub fn role(mut self, role: Role) -> Self {
        self.role = role;
        self
    }

    pub fn active(mut self, active: bool) -> Self {
        self.active = active;
        self
    }

    pub fn admin(self) -> Self {
        self.role(Role::Admin)
    }

    pub fn inactive(self) -> Self {
        self.active(false)
    }

    pub fn with_address(mut self, address: Address) -> Self {
        self.address = Some(address);
        self
    }

    pub fn build(self) -> User {
        let mut user = User::new(&self.name, &self.email, self.age);
        user.set_role(self.role);
        user.set_active(self.active);
        if let Some(addr) = self.address {
            user.set_address(addr);
        }
        user
    }
}
```

### Uso del builder

```rust
// tests/user_tests.rs
mod common;

use common::builders::UserBuilder;

#[test]
fn test_default_user() {
    // Solo especificas lo que importa para ESTE test
    let user = UserBuilder::new().build();
    assert_eq!(user.name(), "TestUser");
    assert!(user.is_active());
}

#[test]
fn test_admin_user() {
    let user = UserBuilder::new()
        .name("Admin")
        .admin()
        .build();
    assert_eq!(user.role(), mi_app::Role::Admin);
}

#[test]
fn test_inactive_user_cannot_login() {
    let user = UserBuilder::new()
        .inactive()
        .build();
    assert!(user.login("password").is_err());
}

#[test]
fn test_user_with_specific_age() {
    let user = UserBuilder::new()
        .age(17)
        .build();
    assert!(!user.can_vote());
}

#[test]
fn test_user_with_address() {
    let user = UserBuilder::new()
        .name("Alice")
        .with_address(common::fixtures::sample_address())
        .build();
    assert!(user.has_address());
}
```

### Builder con ID único automático

```rust
// tests/common/builders.rs

use std::sync::atomic::{AtomicU64, Ordering};

static USER_COUNTER: AtomicU64 = AtomicU64::new(1);

pub struct UserBuilder {
    id: Option<u64>,
    name: String,
    email: String,
    // ...
}

impl UserBuilder {
    pub fn new() -> Self {
        let n = USER_COUNTER.fetch_add(1, Ordering::SeqCst);
        UserBuilder {
            id: None,
            name: format!("User_{}", n),
            email: format!("user_{}@test.com", n),
            // ...
        }
    }

    // Si no se especifica ID, se auto-genera uno único
    pub fn build(self) -> User {
        let id = self.id.unwrap_or_else(|| {
            USER_COUNTER.fetch_add(1, Ordering::SeqCst)
        });
        User::with_id(id, &self.name, &self.email)
    }
}
```

Esto garantiza que cada test obtiene usuarios con emails únicos, evitando colisiones entre tests paralelos.

### Builder para múltiples objetos

```rust
// tests/common/builders.rs

pub struct OrderBuilder {
    customer: UserBuilder,
    products: Vec<ProductBuilder>,
    discount_percent: u32,
}

pub struct ProductBuilder {
    name: String,
    price_cents: u64,
    quantity: u32,
}

impl OrderBuilder {
    pub fn new() -> Self {
        OrderBuilder {
            customer: UserBuilder::new(),
            products: vec![ProductBuilder::default_widget()],
            discount_percent: 0,
        }
    }

    pub fn customer(mut self, customer: UserBuilder) -> Self {
        self.customer = customer;
        self
    }

    pub fn add_product(mut self, product: ProductBuilder) -> Self {
        self.products.push(product);
        self
    }

    pub fn with_products(mut self, products: Vec<ProductBuilder>) -> Self {
        self.products = products;
        self
    }

    pub fn discount(mut self, percent: u32) -> Self {
        self.discount_percent = percent;
        self
    }

    pub fn empty(mut self) -> Self {
        self.products.clear();
        self
    }

    pub fn build(self) -> Order {
        let customer = self.customer.build();
        let products: Vec<Product> = self.products
            .into_iter()
            .map(|pb| pb.build())
            .collect();

        let mut order = Order::new(customer, products);
        if self.discount_percent > 0 {
            order.apply_discount(self.discount_percent);
        }
        order
    }
}

impl ProductBuilder {
    pub fn default_widget() -> Self {
        ProductBuilder {
            name: "Widget".to_string(),
            price_cents: 999,
            quantity: 1,
        }
    }

    pub fn name(mut self, name: &str) -> Self {
        self.name = name.to_string();
        self
    }

    pub fn price(mut self, cents: u64) -> Self {
        self.price_cents = cents;
        self
    }

    pub fn quantity(mut self, qty: u32) -> Self {
        self.quantity = qty;
        self
    }

    pub fn build(self) -> Product {
        Product::new(&self.name, self.price_cents, self.quantity)
    }
}
```

```rust
// tests/order_tests.rs
mod common;
use common::builders::{OrderBuilder, ProductBuilder, UserBuilder};

#[test]
fn test_order_total() {
    let order = OrderBuilder::new()
        .add_product(ProductBuilder::default_widget().price(1000))
        .add_product(ProductBuilder::default_widget().price(2000))
        .build();

    assert_eq!(order.total_cents(), 3999);  // 999 + 1000 + 2000
}

#[test]
fn test_order_with_discount() {
    let order = OrderBuilder::new()
        .add_product(ProductBuilder::default_widget().price(10000))
        .discount(10)
        .build();

    assert_eq!(order.total_cents(), 9899);  // (999 + 10000) * 0.9
}

#[test]
fn test_empty_order() {
    let order = OrderBuilder::new().empty().build();
    assert_eq!(order.total_cents(), 0);
    assert!(order.products().is_empty());
}
```

---

## 9. RAII fixtures: setup con cleanup automático

El patrón RAII (Resource Acquisition Is Initialization) usa el trait `Drop` para garantizar cleanup automático, incluso si el test falla con panic:

### TestDir: directorio temporal con cleanup

```rust
// tests/common/cleanup.rs

use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU32, Ordering};

static DIR_COUNTER: AtomicU32 = AtomicU32::new(0);

/// Directorio temporal que se auto-elimina al salir de scope.
pub struct TestDir {
    path: PathBuf,
}

impl TestDir {
    /// Crea un directorio temporal único.
    pub fn new(prefix: &str) -> Self {
        let id = DIR_COUNTER.fetch_add(1, Ordering::SeqCst);
        let path = std::env::temp_dir()
            .join("rust_tests")
            .join(format!("{}_{}_{}",
                prefix,
                std::process::id(),
                id
            ));
        std::fs::create_dir_all(&path)
            .unwrap_or_else(|e| panic!("Cannot create test dir {:?}: {}", path, e));
        TestDir { path }
    }

    /// Path del directorio temporal.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Crea un archivo dentro del directorio.
    pub fn write_file(&self, name: &str, content: &str) -> PathBuf {
        let file_path = self.path.join(name);
        if let Some(parent) = file_path.parent() {
            std::fs::create_dir_all(parent).unwrap();
        }
        std::fs::write(&file_path, content).unwrap();
        file_path
    }

    /// Lee un archivo del directorio.
    pub fn read_file(&self, name: &str) -> String {
        std::fs::read_to_string(self.path.join(name))
            .unwrap_or_else(|e| panic!("Cannot read {:?}: {}", name, e))
    }

    /// Verifica si un archivo existe.
    pub fn exists(&self, name: &str) -> bool {
        self.path.join(name).exists()
    }

    /// Lista los archivos del directorio.
    pub fn list_files(&self) -> Vec<String> {
        std::fs::read_dir(&self.path)
            .unwrap()
            .filter_map(|entry| {
                let entry = entry.ok()?;
                if entry.file_type().ok()?.is_file() {
                    Some(entry.file_name().to_string_lossy().to_string())
                } else {
                    None
                }
            })
            .collect()
    }
}

impl Drop for TestDir {
    fn drop(&mut self) {
        // Ignora errores de cleanup — no queremos panic en un destructor
        let _ = std::fs::remove_dir_all(&self.path);
    }
}
```

```rust
// tests/file_tests.rs
mod common;

use common::cleanup::TestDir;

#[test]
fn test_process_csv_file() {
    let dir = TestDir::new("csv");

    // Crear archivo de entrada
    dir.write_file("input.csv", "name,age\nAlice,30\nBob,25");

    // Procesar
    mi_app::process_csv(
        &dir.path().join("input.csv"),
        &dir.path().join("output.json"),
    ).unwrap();

    // Verificar resultado
    let output = dir.read_file("output.json");
    assert!(output.contains("Alice"));
    assert!(output.contains("30"));
}   // ← dir se dropea aquí: elimina todo el directorio

#[test]
fn test_processing_error_does_not_leave_temp_files() {
    let dir = TestDir::new("error");

    dir.write_file("bad.csv", "not,a,valid\ncsv,,,,file");

    let result = mi_app::process_csv(
        &dir.path().join("bad.csv"),
        &dir.path().join("output.json"),
    );

    assert!(result.is_err());
    assert!(!dir.exists("output.json"));
}   // ← Cleanup automático incluso si assert! falla
```

### TestServer: servidor de test con shutdown automático

```rust
// tests/common/cleanup.rs

use std::net::TcpListener;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

/// Servidor de test que se detiene automáticamente al salir de scope.
pub struct TestServer {
    port: u16,
    shutdown: Arc<AtomicBool>,
    handle: Option<std::thread::JoinHandle<()>>,
}

impl TestServer {
    pub fn start() -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        let shutdown = Arc::new(AtomicBool::new(false));
        let shutdown_clone = Arc::clone(&shutdown);

        let handle = std::thread::spawn(move || {
            listener.set_nonblocking(true).unwrap();
            while !shutdown_clone.load(Ordering::SeqCst) {
                if let Ok((stream, _)) = listener.accept() {
                    mi_app::handle_connection(stream);
                }
                std::thread::sleep(std::time::Duration::from_millis(10));
            }
        });

        // Dar tiempo al servidor para arrancar
        std::thread::sleep(std::time::Duration::from_millis(50));

        TestServer {
            port,
            shutdown,
            handle: Some(handle),
        }
    }

    pub fn port(&self) -> u16 {
        self.port
    }

    pub fn url(&self) -> String {
        format!("http://127.0.0.1:{}", self.port)
    }
}

impl Drop for TestServer {
    fn drop(&mut self) {
        self.shutdown.store(true, Ordering::SeqCst);
        if let Some(handle) = self.handle.take() {
            let _ = handle.join();
        }
    }
}
```

```rust
// tests/api_tests.rs
mod common;
use common::cleanup::TestServer;

#[test]
fn test_health_check() {
    let server = TestServer::start();

    let response = mi_app::http_get(&format!("{}/health", server.url())).unwrap();
    assert_eq!(response.status(), 200);
    assert_eq!(response.body(), "OK");
}   // ← Servidor se detiene automáticamente

#[test]
fn test_not_found() {
    let server = TestServer::start();

    let response = mi_app::http_get(&format!("{}/nonexistent", server.url())).unwrap();
    assert_eq!(response.status(), 404);
}   // ← Shutdown automático
```

### Composición de RAII fixtures

```rust
// tests/common/cleanup.rs

/// Entorno de test completo con DB, directorio y config.
pub struct TestEnv {
    pub db: mi_app::Database,
    pub dir: TestDir,
    pub config: mi_app::Config,
}

impl TestEnv {
    pub fn new() -> Self {
        let dir = TestDir::new("env");

        let config = mi_app::Config::builder()
            .data_dir(dir.path().to_str().unwrap())
            .database_url("memory://test")
            .build();

        let db = mi_app::Database::connect(&config.database_url()).unwrap();
        db.run_migrations().unwrap();

        TestEnv { db, dir, config }
    }

    pub fn with_sample_data(self) -> Self {
        self.dir.write_file("sample.txt", "test data");
        for (name, email, _) in super::fixtures::sample_users() {
            mi_app::User::create(&self.db, name, email).unwrap();
        }
        self
    }
}

// No necesita impl Drop explícito:
// - db se dropea (cierra conexión)
// - dir se dropea (elimina directorio)
// - config no tiene recursos
// Rust ejecuta Drop en orden inverso de declaración (config, dir, db)
```

```
Orden de drop en TestEnv:

  TestEnv::drop() {
      1. config se dropea  (no hace nada especial)
      2. dir se dropea     (elimina directorio temporal)
      3. db se dropea      (cierra conexión, libera memoria)
  }

  El orden es inverso al de declaración en el struct.
  Esto es importante: el directorio se elimina antes de que
  la base de datos se cierre, lo cual está bien porque la DB
  es in-memory y no depende del directorio.
```

---

## 10. Custom assertions

Las assertions personalizadas hacen los tests más legibles y los mensajes de error más informativos:

### Assertions como funciones

```rust
// tests/common/assertions.rs

use mi_app::{User, Order, ValidationResult};

/// Verifica que dos floats son aproximadamente iguales.
pub fn assert_approx_eq(actual: f64, expected: f64, epsilon: f64) {
    assert!(
        (actual - expected).abs() < epsilon,
        "Values not approximately equal:\n  actual:   {}\n  expected: {}\n  diff:     {}\n  epsilon:  {}",
        actual, expected, (actual - expected).abs(), epsilon
    );
}

/// Verifica que un Result es Ok y retorna el valor.
pub fn assert_ok<T: std::fmt::Debug, E: std::fmt::Debug>(result: Result<T, E>) -> T {
    match result {
        Ok(val) => val,
        Err(e) => panic!("Expected Ok, got Err({:?})", e),
    }
}

/// Verifica que un Result es Err y retorna el error.
pub fn assert_err<T: std::fmt::Debug, E: std::fmt::Debug>(result: Result<T, E>) -> E {
    match result {
        Ok(val) => panic!("Expected Err, got Ok({:?})", val),
        Err(e) => e,
    }
}

/// Verifica que un Vec contiene exactamente los elementos esperados (sin importar orden).
pub fn assert_contains_exactly<T>(actual: &[T], expected: &[T])
where
    T: PartialEq + std::fmt::Debug + Ord + Clone,
{
    let mut actual_sorted = actual.to_vec();
    actual_sorted.sort();
    let mut expected_sorted = expected.to_vec();
    expected_sorted.sort();

    assert_eq!(
        actual_sorted, expected_sorted,
        "\nExpected (sorted): {:?}\nActual (sorted):   {:?}",
        expected_sorted, actual_sorted
    );
}

/// Verifica que un usuario tiene el rol esperado.
pub fn assert_user_role(user: &User, expected_role: mi_app::Role) {
    assert_eq!(
        user.role(), expected_role,
        "User '{}' has role {:?}, expected {:?}",
        user.name(), user.role(), expected_role
    );
}

/// Verifica que una validación falla en los campos esperados.
pub fn assert_validation_fails(
    result: &ValidationResult,
    expected_fields: &[&str],
) {
    assert!(
        !result.is_valid(),
        "Expected validation to fail, but it passed"
    );

    for field in expected_fields {
        assert!(
            result.has_error_for(field),
            "Expected validation error for field '{}', but none found.\nActual errors: {:?}",
            field,
            result.errors()
        );
    }

    // Verificar que no hay errores inesperados
    for error in result.errors() {
        assert!(
            expected_fields.contains(&error.field()),
            "Unexpected validation error for field '{}': {}",
            error.field(),
            error.message()
        );
    }
}

/// Verifica que una operación se completa dentro de un límite de tiempo.
pub fn assert_completes_within<F, R>(duration_ms: u64, operation: F) -> R
where
    F: FnOnce() -> R,
{
    let start = std::time::Instant::now();
    let result = operation();
    let elapsed = start.elapsed();

    assert!(
        elapsed.as_millis() < duration_ms as u128,
        "Operation took {}ms, expected < {}ms",
        elapsed.as_millis(),
        duration_ms
    );

    result
}
```

### Uso de custom assertions

```rust
// tests/order_tests.rs
mod common;
use common::assertions::*;

#[test]
fn test_order_total_calculation() {
    let order = common::builders::OrderBuilder::new()
        .add_product(common::builders::ProductBuilder::default_widget().price(1999))
        .build();

    // Mucho más legible que assert! con comparación manual
    assert_approx_eq(order.total_dollars(), 29.98, 0.01);
}

#[test]
fn test_create_user_returns_user() {
    let db = common::db::seeded_db();
    let result = mi_app::User::create(&db, "Alice", "alice@test.com");

    // Extrae el valor Ok o falla con mensaje claro
    let user = assert_ok(result);
    assert_eq!(user.name(), "Alice");
}

#[test]
fn test_validation_catches_bad_fields() {
    let result = mi_app::validate_user_data(&common::fixtures::invalid_user_data());

    // Verifica exactamente qué campos fallan
    assert_validation_fails(&result, &["name", "email", "age"]);
}

#[test]
fn test_search_is_fast() {
    let db = common::db::populated_db();

    let results = assert_completes_within(100, || {
        mi_app::search_users(&db, "Alice")
    });

    assert!(!results.is_empty());
}
```

### Mensajes de error comparados

```
assert! estándar con mala información:
  thread 'test_order' panicked at 'assertion failed'

assert_eq! un poco mejor:
  thread 'test_order' panicked at 'assertion `left == right` failed
    left: 29.98000001
    right: 29.98'

assert_approx_eq custom con contexto:
  thread 'test_order' panicked at 'Values not approximately equal:
    actual:   29.98000001
    expected: 29.98
    diff:     0.00000001
    epsilon:  0.01'

assert_validation_fails custom con contexto:
  thread 'test_validate' panicked at 'Expected validation error
    for field 'email', but none found.
    Actual errors: [FieldError { field: "name", message: "required" }]'
```

---

## 11. Trait para test doubles: fakes y stubs

Los test helpers pueden incluir implementaciones alternativas de traits para testing:

### Fake: implementación simplificada que funciona

```rust
// tests/common/fakes.rs

use mi_app::{EmailSender, EmailResult, Email};

/// Fake que registra emails enviados en vez de enviarlos realmente.
pub struct FakeEmailSender {
    sent: std::cell::RefCell<Vec<Email>>,
    should_fail: bool,
}

impl FakeEmailSender {
    pub fn new() -> Self {
        FakeEmailSender {
            sent: std::cell::RefCell::new(Vec::new()),
            should_fail: false,
        }
    }

    pub fn failing() -> Self {
        FakeEmailSender {
            sent: std::cell::RefCell::new(Vec::new()),
            should_fail: true,
        }
    }

    /// Retorna todos los emails "enviados".
    pub fn sent_emails(&self) -> Vec<Email> {
        self.sent.borrow().clone()
    }

    /// Retorna cuántos emails se enviaron.
    pub fn send_count(&self) -> usize {
        self.sent.borrow().len()
    }

    /// Verifica que se envió un email al destinatario dado.
    pub fn assert_sent_to(&self, recipient: &str) {
        let sent = self.sent.borrow();
        assert!(
            sent.iter().any(|e| e.to() == recipient),
            "No email sent to '{}'. Sent to: {:?}",
            recipient,
            sent.iter().map(|e| e.to()).collect::<Vec<_>>()
        );
    }

    /// Verifica que NO se envió ningún email.
    pub fn assert_no_emails_sent(&self) {
        let sent = self.sent.borrow();
        assert!(
            sent.is_empty(),
            "Expected no emails, but {} were sent: {:?}",
            sent.len(),
            sent.iter().map(|e| e.to()).collect::<Vec<_>>()
        );
    }
}

impl EmailSender for FakeEmailSender {
    fn send(&self, email: Email) -> EmailResult {
        if self.should_fail {
            Err("Simulated email failure".to_string())
        } else {
            self.sent.borrow_mut().push(email);
            Ok(())
        }
    }
}
```

```rust
// tests/notification_tests.rs
mod common;

use mi_app::NotificationService;
use common::fakes::FakeEmailSender;

#[test]
fn test_welcome_email_sent_on_registration() {
    let email_sender = FakeEmailSender::new();
    let service = NotificationService::new(&email_sender);

    service.on_user_registered("alice@test.com", "Alice").unwrap();

    email_sender.assert_sent_to("alice@test.com");
    assert_eq!(email_sender.send_count(), 1);

    let emails = email_sender.sent_emails();
    assert!(emails[0].subject().contains("Welcome"));
}

#[test]
fn test_registration_succeeds_even_if_email_fails() {
    let email_sender = FakeEmailSender::failing();
    let service = NotificationService::new(&email_sender);

    // El registro no debe fallar por un error de email
    let result = service.on_user_registered("bob@test.com", "Bob");
    assert!(result.is_ok());  // Error de email se logea pero no se propaga
}
```

### Stub: implementación mínima con respuesta predefinida

```rust
// tests/common/fakes.rs

use mi_app::{Clock, TimeProvider};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// Reloj que siempre retorna el mismo tiempo (para tests determinísticos).
pub struct StubClock {
    fixed_time: SystemTime,
}

impl StubClock {
    /// Crea un reloj fijado en un momento específico.
    pub fn fixed(year: u32, month: u32, day: u32) -> Self {
        let secs = calculate_epoch_secs(year, month, day);
        StubClock {
            fixed_time: UNIX_EPOCH + Duration::from_secs(secs),
        }
    }

    /// Crea un reloj fijado en epoch (1970-01-01).
    pub fn epoch() -> Self {
        StubClock {
            fixed_time: UNIX_EPOCH,
        }
    }

    /// Crea un reloj fijado en un timestamp específico.
    pub fn at_timestamp(secs: u64) -> Self {
        StubClock {
            fixed_time: UNIX_EPOCH + Duration::from_secs(secs),
        }
    }
}

impl TimeProvider for StubClock {
    fn now(&self) -> SystemTime {
        self.fixed_time  // Siempre el mismo tiempo
    }
}

fn calculate_epoch_secs(year: u32, month: u32, day: u32) -> u64 {
    // Simplificación — en producción usarías chrono
    let days_since_epoch = (year as u64 - 1970) * 365 + (month as u64 - 1) * 30 + day as u64;
    days_since_epoch * 86400
}
```

```rust
// tests/expiration_tests.rs
mod common;

use mi_app::Token;
use common::fakes::StubClock;

#[test]
fn test_token_not_expired_within_window() {
    let clock = StubClock::at_timestamp(1000);
    let token = Token::new_with_clock("secret", 3600, &clock);  // Expira en 1h

    // Avanzar el reloj 30 minutos (no expira)
    let later_clock = StubClock::at_timestamp(2800);
    assert!(!token.is_expired_at(&later_clock));
}

#[test]
fn test_token_expired_after_window() {
    let clock = StubClock::at_timestamp(1000);
    let token = Token::new_with_clock("secret", 3600, &clock);

    // Avanzar el reloj 2 horas (sí expira)
    let later_clock = StubClock::at_timestamp(8200);
    assert!(token.is_expired_at(&later_clock));
}
```

### Fake vs Stub vs Mock

```
Test Doubles en Rust:

  ┌─────────────┬──────────────────────────────────────────────┐
  │ Tipo        │ Qué hace                                     │
  ├─────────────┼──────────────────────────────────────────────┤
  │ Stub        │ Retorna respuestas predefinidas               │
  │             │ "Siempre retorna 42"                          │
  │             │ No tiene estado, no verifica nada             │
  │             │                                              │
  │             │ Ejemplo: StubClock que retorna tiempo fijo    │
  ├─────────────┼──────────────────────────────────────────────┤
  │ Fake        │ Implementación funcional simplificada         │
  │             │ Tiene comportamiento real pero simplificado   │
  │             │ Puede guardar estado para inspección          │
  │             │                                              │
  │             │ Ejemplo: FakeEmailSender que guarda           │
  │             │ en Vec en vez de enviar                       │
  ├─────────────┼──────────────────────────────────────────────┤
  │ Mock        │ Verifica que se llamó de forma específica     │
  │             │ Tiene expectations predefinidas               │
  │             │ Falla si no se cumplieron                     │
  │             │                                              │
  │             │ Ejemplo: se verá en S04 (Mocking en Rust)     │
  │             │ con mockall y automock                        │
  └─────────────┴──────────────────────────────────────────────┘

  Preferencia general:  Fake > Stub > Mock
  (más simple y mantenible a menos complejo)
```

---

## 12. Archivos de fixtures externos

Para datos de prueba complejos que no caben en código (JSON, CSV, XML, binarios), usa archivos de fixture:

### Estructura de directorios

```
mi_proyecto/
├── Cargo.toml
├── src/
│   └── lib.rs
└── tests/
    ├── fixtures/                ← Datos de prueba como archivos
    │   ├── valid_config.toml
    │   ├── malformed.json
    │   ├── sample_data.csv
    │   ├── large_input.txt
    │   └── images/
    │       ├── valid.png
    │       └── corrupt.png
    ├── common/
    │   └── mod.rs
    └── file_tests.rs
```

### Acceso a fixtures

```rust
// tests/common/mod.rs

use std::path::PathBuf;

/// Retorna la ruta al directorio de fixtures.
pub fn fixtures_dir() -> PathBuf {
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.push("tests");
    path.push("fixtures");
    path
}

/// Retorna la ruta a un fixture específico.
pub fn fixture_path(name: &str) -> PathBuf {
    let path = fixtures_dir().join(name);
    assert!(
        path.exists(),
        "Fixture file not found: {:?}",
        path
    );
    path
}

/// Lee el contenido de un fixture como string.
pub fn fixture_string(name: &str) -> String {
    std::fs::read_to_string(fixture_path(name))
        .unwrap_or_else(|e| panic!("Cannot read fixture '{}': {}", name, e))
}

/// Lee el contenido de un fixture como bytes.
pub fn fixture_bytes(name: &str) -> Vec<u8> {
    std::fs::read(fixture_path(name))
        .unwrap_or_else(|e| panic!("Cannot read fixture '{}': {}", name, e))
}
```

```rust
// tests/file_tests.rs
mod common;

#[test]
fn test_parse_valid_config() {
    let content = common::fixture_string("valid_config.toml");
    let config = mi_app::Config::from_toml(&content).unwrap();
    assert_eq!(config.name(), "test_project");
}

#[test]
fn test_malformed_json_returns_error() {
    let content = common::fixture_string("malformed.json");
    let result = mi_app::parse_json(&content);
    assert!(result.is_err());
}

#[test]
fn test_parse_csv_data() {
    let path = common::fixture_path("sample_data.csv");
    let records = mi_app::read_csv(&path).unwrap();
    assert!(records.len() > 0);
}

#[test]
fn test_detect_valid_png() {
    let bytes = common::fixture_bytes("images/valid.png");
    assert!(mi_app::is_valid_png(&bytes));
}

#[test]
fn test_detect_corrupt_image() {
    let bytes = common::fixture_bytes("images/corrupt.png");
    assert!(!mi_app::is_valid_png(&bytes));
}
```

### include_str! e include_bytes! para fixtures en compilación

```rust
// tests/common/mod.rs

/// Fixtures incrustadas en compilación (no necesitan archivos en runtime).
pub mod embedded {
    pub const VALID_JSON: &str = include_str!("../fixtures/valid_config.json");
    pub const SAMPLE_CSV: &str = include_str!("../fixtures/sample_data.csv");
    pub const PNG_HEADER: &[u8] = include_bytes!("../fixtures/images/valid.png");
}
```

```rust
// tests/parse_tests.rs
mod common;

#[test]
fn test_parse_embedded_json() {
    // No necesita leer del filesystem en runtime
    let config = mi_app::parse_json(common::embedded::VALID_JSON).unwrap();
    assert_eq!(config["name"], "test_project");
}
```

```
include_str!/include_bytes! vs std::fs::read:

  include_str!("file"):
    + Se resuelve en COMPILACIÓN (el contenido está en el binario)
    + No falla en runtime (si compila, funciona)
    + No necesita el archivo en runtime
    - Aumenta el tamaño del binario de test
    - Re-compilación si el archivo cambia
    - Solo paths relativos al archivo fuente

  std::fs::read("file"):
    + No aumenta el binario
    + Flexible (paths dinámicos)
    - Puede fallar en runtime si el archivo no existe
    - Necesita el archivo presente al ejecutar tests
    - Necesita CARGO_MANIFEST_DIR para paths absolutos

  Recomendación:
    Archivos pequeños (< 10 KB): include_str!/include_bytes!
    Archivos grandes (> 10 KB): std::fs::read con fixture_path()
```

---

## 13. Test utilities como crate interna

Para proyectos grandes con workspace, puedes crear una crate dedicada a utilidades de test:

### Estructura de workspace

```
mi_workspace/
├── Cargo.toml              ← Workspace root
├── crates/
│   ├── core/
│   │   ├── Cargo.toml
│   │   ├── src/lib.rs
│   │   └── tests/
│   │       ├── common/mod.rs      ← Helpers locales
│   │       └── core_tests.rs
│   │
│   ├── api/
│   │   ├── Cargo.toml
│   │   ├── src/lib.rs
│   │   └── tests/
│   │       ├── common/mod.rs      ← ¿Duplicar helpers? ✗
│   │       └── api_tests.rs
│   │
│   └── test_utils/                ← Crate compartida de test utilities
│       ├── Cargo.toml
│       └── src/lib.rs
```

### Configuración del workspace

```toml
# mi_workspace/Cargo.toml
[workspace]
members = [
    "crates/core",
    "crates/api",
    "crates/test_utils",
]
```

```toml
# crates/test_utils/Cargo.toml
[package]
name = "test_utils"
version = "0.1.0"
edition = "2021"

# No publicar — es solo para testing interno
publish = false

[dependencies]
mi_core = { path = "../core" }
```

```toml
# crates/core/Cargo.toml
[package]
name = "mi_core"
version = "0.1.0"
edition = "2021"

[dev-dependencies]
test_utils = { path = "../test_utils" }
```

```toml
# crates/api/Cargo.toml
[package]
name = "mi_api"
version = "0.1.0"
edition = "2021"

[dependencies]
mi_core = { path = "../core" }

[dev-dependencies]
test_utils = { path = "../test_utils" }
```

### La crate de test_utils

```rust
// crates/test_utils/src/lib.rs

//! Utilidades compartidas de testing para el workspace.

use mi_core::{Database, User, Config};

// ═══ Setup ═══

pub fn test_db() -> Database {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    db
}

pub fn test_config() -> Config {
    Config::test_defaults()
}

// ═══ Builders ═══

pub struct UserBuilder { /* ... */ }

impl UserBuilder {
    pub fn new() -> Self { /* ... */ todo!() }
    pub fn name(self, name: &str) -> Self { /* ... */ todo!() }
    pub fn build(self) -> User { /* ... */ todo!() }
}

// ═══ Assertions ═══

pub fn assert_ok<T: std::fmt::Debug, E: std::fmt::Debug>(result: Result<T, E>) -> T {
    match result {
        Ok(v) => v,
        Err(e) => panic!("Expected Ok, got Err({:?})", e),
    }
}

pub fn assert_err<T: std::fmt::Debug, E: std::fmt::Debug>(result: Result<T, E>) -> E {
    match result {
        Ok(v) => panic!("Expected Err, got Ok({:?})", v),
        Err(e) => e,
    }
}

// ═══ Fixtures ═══

pub fn sample_users() -> Vec<(&'static str, &'static str)> {
    vec![
        ("Alice", "alice@test.com"),
        ("Bob", "bob@test.com"),
    ]
}
```

### Uso desde múltiples crates

```rust
// crates/core/tests/user_tests.rs
use test_utils;  // ← Como cualquier dependencia

#[test]
fn test_create_user() {
    let db = test_utils::test_db();
    let user = test_utils::UserBuilder::new()
        .name("Alice")
        .build();

    let saved = mi_core::User::save(&db, user).unwrap();
    assert_eq!(saved.name(), "Alice");
}
```

```rust
// crates/api/tests/endpoint_tests.rs
use test_utils;  // ← Misma crate de utilidades, otra crate de consumidor

#[test]
fn test_api_list_users() {
    let db = test_utils::test_db();

    // Insertar datos de prueba
    for (name, email) in test_utils::sample_users() {
        mi_core::User::create(&db, name, email).unwrap();
    }

    let response = mi_api::handle_list_users(&db);
    assert_eq!(response.status(), 200);
}
```

### Ventajas de una crate de test_utils

```
Sin crate compartida:                Con crate compartida:

  crates/core/tests/common/           crates/test_utils/src/lib.rs
    mod.rs (setup_db, fixtures...)       (setup_db, fixtures,
                                          builders, assertions...)
  crates/api/tests/common/
    mod.rs (setup_db DUPLICADO,        crates/core/tests/
            fixtures DUPLICADOS...)       (usa test_utils)

  crates/auth/tests/common/           crates/api/tests/
    mod.rs (setup_db TRIPLICADO...)      (usa test_utils)

  Problema:                            crates/auth/tests/
    3 copias de los mismos helpers       (usa test_utils)
    Si cambia la API de Database,
    hay que actualizar 3 archivos      Ventaja:
                                         1 copia, N consumidores
```

---

## 14. Helpers compartidos entre unit e integration tests

Este es un problema común: quieres usar el **mismo helper** tanto en unit tests (en `src/`) como en integration tests (en `tests/`). Las soluciones:

### El problema

```
Unit tests (src/):           Integration tests (tests/):
  ├─ mod tests { }            ├─ mod common;
  ├─ use crate::test_helpers  ├─ common::setup_db()
  └─ test_helpers::setup()    └─ Necesitan los MISMOS helpers

  Pero:
  - src/test_helpers.rs (#[cfg(test)]) → Invisible para tests/
  - tests/common/mod.rs              → Invisible para src/

  ¿Cómo compartir?
```

### Solución 1: Duplicación aceptable (para helpers simples)

Si los helpers son 5-10 líneas, la duplicación es aceptable:

```rust
// src/parser.rs
#[cfg(test)]
mod tests {
    fn parse_ok(input: &str) -> Ast {  // Helper local
        super::parse(input).unwrap()
    }
    // ...
}
```

```rust
// tests/common/mod.rs
pub fn parse_ok(input: &str) -> mi_crate::Ast {  // Helper "duplicado"
    mi_crate::parse(input).unwrap()
}
```

### Solución 2: Módulo pub(crate) condicionalmente compilado

```rust
// src/lib.rs

// Módulo de helpers visible solo dentro de la crate, solo en modo test
#[cfg(test)]
pub mod testing {
    use crate::Database;

    pub fn setup_db() -> Database {
        Database::new_in_memory("test").unwrap()
    }
}
```

```rust
// src/user.rs — Unit test usa crate::testing
#[cfg(test)]
mod tests {
    use crate::testing;

    #[test]
    fn test_user() {
        let db = testing::setup_db();
        // ...
    }
}
```

Pero esto **no funciona** para integration tests porque `#[cfg(test)]` no se activa en el `.rlib`.

### Solución 3: Feature flag para testing

```toml
# Cargo.toml
[features]
default = []
test-support = []  # Feature que activa helpers de testing
```

```rust
// src/lib.rs

#[cfg(any(test, feature = "test-support"))]
pub mod testing {
    pub fn setup_db() -> crate::Database {
        crate::Database::new_in_memory("test").unwrap()
    }
}
```

```rust
// Unit test — cfg(test) se activa automáticamente
#[cfg(test)]
mod tests {
    use crate::testing;

    #[test]
    fn test_unit() {
        let db = testing::setup_db();
    }
}
```

```toml
# Cargo.toml (dev-dependencies para activar el feature en tests)
[dev-dependencies]
mi_crate = { path = ".", features = ["test-support"] }
```

Ahora los integration tests pueden usar `mi_crate::testing::setup_db()`.

```
Solución con feature flag:

  cargo build:
    features: []
    → testing module NO existe
    → Código de producción limpio

  cargo test --lib:
    features: [] + cfg(test) activo
    → testing module SÍ existe (por cfg(test))

  cargo test (integration):
    features: ["test-support"]  (por dev-dependencies)
    → testing module SÍ existe (por feature)
    → tests/ puede importar mi_crate::testing
```

### Solución 4: Crate separada en workspace (la más limpia)

Es la misma solución de la sección 13. Es la más limpia para proyectos grandes pero tiene el overhead de mantener una crate adicional.

### Tabla de decisión

| Situación | Solución recomendada |
|-----------|---------------------|
| Helpers simples (< 10 líneas) | Duplicar en `mod tests` y `tests/common/` |
| Helpers moderados, proyecto individual | Feature flag `test-support` |
| Workspace con múltiples crates | Crate `test_utils` compartida |
| Solo unit tests los necesitan | `#[cfg(test)] mod test_helpers` en `src/` |
| Solo integration tests los necesitan | `tests/common/mod.rs` |

---

## 15. Generadores de datos aleatorios para tests

A veces necesitas datos variados pero no te importa el valor específico. Los generadores de datos evitan fixtures estáticos y exponen edge cases:

### Generadores simples sin crate externa

```rust
// tests/common/generators.rs

use std::sync::atomic::{AtomicU64, Ordering};

static COUNTER: AtomicU64 = AtomicU64::new(0);

fn next_id() -> u64 {
    COUNTER.fetch_add(1, Ordering::SeqCst)
}

/// Genera un nombre único para cada invocación.
pub fn unique_name() -> String {
    format!("User_{}", next_id())
}

/// Genera un email único.
pub fn unique_email() -> String {
    format!("user_{}@test.com", next_id())
}

/// Genera un string de longitud específica.
pub fn string_of_length(len: usize) -> String {
    "a".repeat(len)
}

/// Genera un Vec<i32> con valores secuenciales.
pub fn sequential_ints(count: usize) -> Vec<i32> {
    (0..count as i32).collect()
}

/// Genera un Vec<i32> con valores en orden inverso.
pub fn reverse_ints(count: usize) -> Vec<i32> {
    (0..count as i32).rev().collect()
}

/// Genera un Vec<i32> con un patrón específico.
pub fn alternating_ints(count: usize) -> Vec<i32> {
    (0..count).map(|i| if i % 2 == 0 { i as i32 } else { -(i as i32) }).collect()
}

/// Genera datos de usuario con variaciones automáticas.
pub fn varied_users(count: usize) -> Vec<(String, String, u32)> {
    (0..count)
        .map(|i| {
            (
                format!("User_{}", i),
                format!("user_{}@test.com", i),
                18 + (i as u32 % 70),
            )
        })
        .collect()
}
```

### Generadores con rand (dev-dependency)

```toml
# Cargo.toml
[dev-dependencies]
rand = "0.8"
```

```rust
// tests/common/generators.rs

use rand::Rng;
use rand::distributions::Alphanumeric;

/// Genera un string aleatorio de longitud dada.
pub fn random_string(len: usize) -> String {
    rand::thread_rng()
        .sample_iter(&Alphanumeric)
        .take(len)
        .map(char::from)
        .collect()
}

/// Genera un email aleatorio válido.
pub fn random_email() -> String {
    format!("{}@{}.com", random_string(8), random_string(5))
}

/// Genera un vector de enteros aleatorios.
pub fn random_ints(count: usize, min: i32, max: i32) -> Vec<i32> {
    let mut rng = rand::thread_rng();
    (0..count).map(|_| rng.gen_range(min..=max)).collect()
}

/// Genera un HashMap con claves y valores aleatorios.
pub fn random_map(entries: usize) -> std::collections::HashMap<String, String> {
    (0..entries)
        .map(|_| (random_string(5), random_string(10)))
        .collect()
}
```

```rust
// tests/stress_tests.rs
mod common;

#[test]
fn test_sort_handles_random_data() {
    for _ in 0..100 {
        let mut data = common::generators::random_ints(100, -1000, 1000);
        mi_crate::sort(&mut data);

        // Verificar que está ordenado
        for window in data.windows(2) {
            assert!(window[0] <= window[1]);
        }
    }
}

#[test]
fn test_insert_many_unique_users() {
    let db = common::db::seeded_db();

    for _ in 0..50 {
        let name = common::generators::random_string(10);
        let email = common::generators::random_email();
        mi_crate::User::create(&db, &name, &email).unwrap();
    }

    let count = mi_crate::User::count(&db).unwrap();
    assert_eq!(count, 50);
}
```

---

## 16. Macros como helpers de test

Las macros pueden ser helpers de test poderosos, especialmente para patrones que se repiten con ligeras variaciones:

### Macro para tests parametrizados

```rust
// tests/common/mod.rs

/// Genera múltiples tests a partir de una tabla de casos.
#[macro_export]
macro_rules! test_cases {
    ($name:ident, $func:expr, [$(($input:expr, $expected:expr)),+ $(,)?]) => {
        mod $name {
            use super::*;
            $(
                paste::paste! {
                    #[test]
                    fn [< test_ $name _ $input:snake >]() {
                        let result = $func($input);
                        assert_eq!(result, $expected,
                            "{}({}) = {:?}, expected {:?}",
                            stringify!($func), $input, result, $expected
                        );
                    }
                }
            )+
        }
    };
}
```

Sin la macro `paste` (que requiere una dependencia), una versión más simple:

```rust
// tests/common/mod.rs

/// Ejecuta una función contra múltiples pares (input, expected).
#[macro_export]
macro_rules! assert_cases {
    ($func:expr, $( ($input:expr, $expected:expr) ),+ $(,)?) => {
        $(
            {
                let result = $func($input);
                assert_eq!(
                    result, $expected,
                    "\n  Failed: {}({:?})\n  Expected: {:?}\n  Got:      {:?}",
                    stringify!($func), $input, $expected, result
                );
            }
        )+
    };
}

/// Verifica que una función retorna Err para todos los inputs dados.
#[macro_export]
macro_rules! assert_all_err {
    ($func:expr, $( $input:expr ),+ $(,)?) => {
        $(
            assert!(
                $func($input).is_err(),
                "Expected Err for input {:?}, got Ok",
                $input
            );
        )+
    };
}

/// Verifica que una expresión hace panic.
#[macro_export]
macro_rules! assert_panics {
    ($expr:expr) => {
        assert!(
            std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| { $expr; })).is_err(),
            "Expected panic, but expression completed successfully"
        );
    };
    ($expr:expr, $msg:expr) => {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| { $expr; }));
        assert!(result.is_err(), "Expected panic with message '{}', but completed successfully", $msg);
    };
}
```

### Uso de macros de test

```rust
// tests/parsing_tests.rs
mod common;

use mi_crate::parse_number;

#[test]
fn test_valid_numbers() {
    assert_cases!(
        parse_number,
        ("42", Ok(42)),
        ("0", Ok(0)),
        ("-17", Ok(-17)),
        ("999", Ok(999)),
    );
}

#[test]
fn test_invalid_numbers() {
    assert_all_err!(
        parse_number,
        "",
        "abc",
        "12.34",
        "0xFF",
    );
}
```

### Macro para setup repetitivo

```rust
// tests/common/mod.rs

/// Crea un test con setup de base de datos.
#[macro_export]
macro_rules! db_test {
    ($name:ident, $body:expr) => {
        #[test]
        fn $name() {
            let db = common::db::seeded_db();
            $body(db);
        }
    };
}

/// Crea un test con setup completo (DB + config + dir temporal).
#[macro_export]
macro_rules! full_test {
    ($name:ident, $body:expr) => {
        #[test]
        fn $name() {
            let env = common::cleanup::TestEnv::new();
            $body(env);
        }
    };
}
```

```rust
// tests/db_tests.rs
mod common;

db_test!(test_insert_user, |db: mi_crate::Database| {
    let user = mi_crate::User::create(&db, "Alice", "alice@test.com").unwrap();
    assert_eq!(user.name(), "Alice");
});

db_test!(test_count_users, |db: mi_crate::Database| {
    mi_crate::User::create(&db, "Alice", "a@test.com").unwrap();
    mi_crate::User::create(&db, "Bob", "b@test.com").unwrap();
    assert_eq!(mi_crate::User::count(&db).unwrap(), 2);
});
```

### Cuándo usar macros vs funciones como helpers

```
Funciones como helpers:
  + Más fáciles de leer y debuggear
  + El IDE las entiende (autocompletado, go to definition)
  + Errores de compilación claros
  + Se pueden poner breakpoints

  Usar cuando:
    - El helper toma datos y retorna datos
    - El patrón es un cálculo o transformación
    - No necesitas generar código

Macros como helpers:
  + Pueden generar código (#[test], structs, impl)
  + Pueden aceptar patrones, tipos, expresiones
  + Reducen mucho boilerplate en tests repetitivos

  Usar cuando:
    - Necesitas generar múltiples funciones #[test]
    - El patrón involucra crear tests parametrizados
    - Necesitas expandir a código que no puede ser una función
    - El boilerplate es 5+ líneas idénticas entre tests

  En general: preferir funciones. Usar macros solo cuando
  la alternativa es copiar/pegar código estructural.
```

---

## 17. Organización según tamaño del proyecto

### Proyecto pequeño (1-3 archivos de test)

```
mi_crate/
├── src/
│   └── lib.rs            // Unit tests con mod tests {}
└── tests/
    └── integration.rs    // Unos pocos integration tests
                          // Helpers inline en el mismo archivo
```

```rust
// tests/integration.rs
use mi_crate::*;

// Helpers inline — no necesitan common/ para tan pocos tests
fn setup() -> Database {
    Database::new_in_memory("test").unwrap()
}

#[test]
fn test_a() { let db = setup(); /* ... */ }

#[test]
fn test_b() { let db = setup(); /* ... */ }
```

### Proyecto mediano (4-10 archivos de test)

```
mi_crate/
├── src/
│   ├── lib.rs
│   ├── auth.rs
│   ├── users.rs
│   └── reports.rs
└── tests/
    ├── common/
    │   └── mod.rs          // Helpers compartidos
    ├── auth_tests.rs
    ├── user_tests.rs
    ├── report_tests.rs
    └── integration.rs      // Tests de flujo completo
```

### Proyecto grande (10+ archivos de test, o workspace)

```
mi_workspace/
├── Cargo.toml
├── crates/
│   ├── test_utils/              ← Crate compartida de test utilities
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs           // Re-exports
│   │       ├── builders.rs      // Builders para datos complejos
│   │       ├── fakes.rs         // Fakes de servicios externos
│   │       ├── assertions.rs    // Custom assertions
│   │       ├── generators.rs    // Generadores de datos
│   │       └── fixtures.rs      // Datos estáticos de prueba
│   │
│   ├── core/
│   │   ├── Cargo.toml
│   │   ├── src/lib.rs
│   │   └── tests/
│   │       ├── common/
│   │       │   └── mod.rs       // Helpers locales + re-export test_utils
│   │       ├── model_tests.rs
│   │       └── service_tests.rs
│   │
│   └── api/
│       ├── Cargo.toml
│       ├── src/lib.rs
│       └── tests/
│           ├── common/
│           │   └── mod.rs       // Helpers locales + re-export test_utils
│           ├── endpoint_tests.rs
│           └── middleware_tests.rs
│
└── tests/                       ← Tests E2E del workspace completo
    ├── common/
    │   └── mod.rs
    └── e2e.rs
```

### Tabla de decisión de organización

| Indicador | Acción |
|-----------|--------|
| Setup duplicado en 2+ archivos | Extraer a `tests/common/mod.rs` |
| common/mod.rs > 200 líneas | Dividir en submódulos (`db.rs`, `fixtures.rs`, etc.) |
| Builders con > 5 campos | Crear módulo `builders.rs` en common/ |
| 3+ fakes/stubs | Crear módulo `fakes.rs` en common/ |
| 5+ custom assertions | Crear módulo `assertions.rs` en common/ |
| Workspace con 2+ crates que repiten helpers | Crear crate `test_utils` |
| Fixtures como archivos (JSON, CSV) | Crear `tests/fixtures/` |
| Tests parametrizados recurrentes | Considerar macros helper |

---

## 18. Comparación con C y Go

### C: Helpers manuales con Unity/CMocka

```c
/* tests/test_helpers.h — Header compartido */
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "unity.h"
#include "../src/database.h"
#include "../src/user.h"

/* Setup global (se llama antes de CADA test) */
static Database *test_db;

void setUp(void) {
    test_db = db_create_in_memory("test");
    db_run_migrations(test_db);
    db_seed_defaults(test_db);
}

void tearDown(void) {
    db_destroy(test_db);
    test_db = NULL;
}

/* Fixture functions */
User *create_test_user(const char *name, const char *email) {
    return user_create(test_db, name, email);
}

/* Custom assertion */
void assert_user_matches(User *user, const char *name, const char *email) {
    TEST_ASSERT_EQUAL_STRING(name, user->name);
    TEST_ASSERT_EQUAL_STRING(email, user->email);
}

#endif /* TEST_HELPERS_H */
```

```c
/* tests/test_users.c */
#include "test_helpers.h"

void test_create_user(void) {
    User *user = create_test_user("Alice", "alice@test.com");
    assert_user_matches(user, "Alice", "alice@test.com");
    user_free(user);  /* ← Cleanup MANUAL obligatorio */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_user);
    return UNITY_END();
}
```

### Go: Helpers con t.Helper() y testify

```go
// testutil/helpers.go — Paquete de helpers compartido
package testutil

import (
    "testing"
    "database/sql"
)

// SetupDB crea una base de datos de prueba.
// t.Helper() hace que los errores se reporten desde el caller.
func SetupDB(t *testing.T) *sql.DB {
    t.Helper()  // ← Los errores reportan la línea del TEST, no del helper

    db, err := sql.Open("sqlite3", ":memory:")
    if err != nil {
        t.Fatalf("Cannot create test DB: %v", err)
    }

    // t.Cleanup se ejecuta al final del test (como defer)
    t.Cleanup(func() {
        db.Close()
    })

    RunMigrations(t, db)
    return db
}

// AssertUserMatches verifica campos de un usuario.
func AssertUserMatches(t *testing.T, user *User, name, email string) {
    t.Helper()
    if user.Name != name {
        t.Errorf("Name: got %q, want %q", user.Name, name)
    }
    if user.Email != email {
        t.Errorf("Email: got %q, want %q", user.Email, email)
    }
}

// TestUser crea un usuario de prueba con valores default.
func TestUser(t *testing.T, db *sql.DB) *User {
    t.Helper()
    user, err := CreateUser(db, "TestUser", "test@test.com")
    if err != nil {
        t.Fatalf("Cannot create test user: %v", err)
    }
    return user
}
```

### Tabla comparativa

| Aspecto | C (Unity) | Go (testing) | Rust |
|---------|-----------|--------------|------|
| **Ubicación de helpers** | Headers `.h` en tests/ | Paquete `testutil` | `tests/common/mod.rs` |
| **Setup/teardown** | `setUp()/tearDown()` globales | `t.Cleanup()` por test | RAII con `Drop` |
| **Cleanup garantizado** | Solo si tearDown se ejecuta | `t.Cleanup` es defer | Drop se ejecuta siempre |
| **Cleanup en panic** | NO (programa aborta) | `t.Cleanup` sí corre | `Drop` sí corre (unwind) |
| **Fixtures** | Structs + malloc/free | Structs + GC | Builders + ownership |
| **Custom assertions** | Macros C `TEST_ASSERT_*` | `t.Errorf()/t.Fatalf()` | `assert!` + funciones |
| **Helpers marcan errores en caller** | No (macro muestra la línea del macro) | `t.Helper()` | `#[track_caller]` |
| **Compartir entre crates** | Headers compartidos | Paquetes Go | Crate `test_utils` |
| **Fake/stub** | Punteros a función | Interfaces | Traits |
| **Cleanup de archivos** | Manual (remove/unlink) | `os.RemoveAll` en Cleanup | `TestDir` con Drop |

### Patrón `#[track_caller]` en Rust (equivalente a `t.Helper()` de Go)

```rust
// tests/common/assertions.rs

/// Cuando esta assertion falla, el error apunta a la línea
/// del TEST que la llamó, no a la línea dentro de la assertion.
#[track_caller]
pub fn assert_ok<T: std::fmt::Debug, E: std::fmt::Debug>(
    result: Result<T, E>,
) -> T {
    match result {
        Ok(v) => v,
        Err(e) => panic!("Expected Ok, got Err({:?})", e),
    }
}
```

```
Sin #[track_caller]:
  thread 'test_foo' panicked at 'Expected Ok, got Err("bad")'
  --> tests/common/assertions.rs:8:9     ← Apunta al HELPER (inútil)

Con #[track_caller]:
  thread 'test_foo' panicked at 'Expected Ok, got Err("bad")'
  --> tests/user_tests.rs:15:16          ← Apunta al TEST (útil)
```

---

## 19. Errores comunes

### Error 1: Usar `tests/common.rs` en vez de `tests/common/mod.rs`

```
✗ tests/common.rs
  → Cargo lo compila como crate de test
  → Aparece "Running tests/common.rs: 0 tests" en output

✓ tests/common/mod.rs
  → Cargo ignora subdirectorios
  → Solo se compila cuando otro archivo hace `mod common;`
```

### Error 2: Olvidar `mod common;` en cada archivo de test

```rust
// tests/test_a.rs
mod common;  // ← Necesario en CADA archivo

// tests/test_b.rs
// mod common;  // ← OLVIDADO — common:: no existe aquí
use common::setup_db;  // ERROR: unresolved import `common`
```

Cada archivo de integration test es un crate separado. Cada uno necesita su propia declaración `mod common;`.

### Error 3: Hacer helpers privados en common/

```rust
// tests/common/mod.rs
fn setup_db() -> Database {  // ← Sin `pub`: privado al módulo
    Database::new_in_memory("test").unwrap()
}
```

```rust
// tests/test.rs
mod common;

#[test]
fn test() {
    let db = common::setup_db();  // ERROR: function `setup_db` is private
}
```

Los items en `common/mod.rs` deben ser `pub` para ser usados desde otros archivos.

### Error 4: Intentar importar `tests/common` desde `src/`

```rust
// src/lib.rs
mod tests_common;  // ✗ No funciona — src/ no puede ver tests/

#[cfg(test)]
mod tests {
    use crate::tests_common::setup;  // ✗ No existe
}
```

Los directorios `src/` y `tests/` son mundos separados. Los unit tests en `src/` NO pueden usar `tests/common/mod.rs`.

### Error 5: Helpers que hacen panic sin contexto

```rust
// ✗ MAL: el mensaje de error no dice QUÉ falló
pub fn setup_db() -> Database {
    Database::new_in_memory("test").unwrap()  // "called unwrap on Err"
}

// ✓ BIEN: contexto en caso de fallo
pub fn setup_db() -> Database {
    Database::new_in_memory("test")
        .unwrap_or_else(|e| panic!("Test setup failed: cannot create DB: {}", e))
}
```

### Error 6: Estado mutable compartido entre tests

```rust
// tests/common/mod.rs
use std::sync::Mutex;

// ✗ PELIGROSO: estado compartido entre tests que corren en paralelo
static SHARED_DB: Mutex<Option<Database>> = Mutex::new(None);

pub fn get_db() -> Database {
    let mut guard = SHARED_DB.lock().unwrap();
    if guard.is_none() {
        *guard = Some(Database::new_in_memory("shared").unwrap());
    }
    guard.as_ref().unwrap().clone()  // Todos los tests comparten la MISMA DB
}

// ✓ CORRECTO: cada test obtiene su propia instancia
pub fn setup_db() -> Database {
    Database::new_in_memory("test").unwrap()
}
```

### Error 7: Builders que producen datos inválidos

```rust
// ✗ MAL: Builder permite crear estados inválidos
pub struct UserBuilder {
    name: String,
    email: String,
}

impl UserBuilder {
    pub fn new() -> Self {
        UserBuilder {
            name: String::new(),     // ← ¡Vacío! ¿Es inválido?
            email: String::new(),    // ← ¡Vacío! ¿Es inválido?
        }
    }

    pub fn build(self) -> User {
        User::new(&self.name, &self.email)  // Puede fallar silenciosamente
    }
}

// ✓ BIEN: Builder con defaults válidos
pub struct UserBuilder {
    name: String,
    email: String,
}

impl UserBuilder {
    pub fn new() -> Self {
        UserBuilder {
            name: "TestUser".to_string(),
            email: "test@example.com".to_string(),
        }
    }
    // ...
}
```

### Error 8: Fixture files que no se incluyen en el repositorio

```
# .gitignore
*.csv          ← ¡Ignora los CSV de fixtures!

tests/
  fixtures/
    data.csv   ← No se commitea → CI falla
```

Los archivos de fixture deben estar commiteados. Asegúrate de que `.gitignore` no los excluya:

```
# .gitignore
*.csv

# Pero SÍ incluir fixtures de test
!tests/fixtures/*.csv
```

---

## 20. Ejemplo completo: test infrastructure para un servicio

Vamos a construir la infraestructura de test completa para un servicio de gestión de tareas (todo list).

### Estructura del proyecto

```
todo_service/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── task.rs
│   ├── store.rs
│   ├── filter.rs
│   └── export.rs
└── tests/
    ├── common/
    │   ├── mod.rs
    │   ├── builders.rs
    │   ├── fixtures.rs
    │   ├── assertions.rs
    │   ├── fakes.rs
    │   └── cleanup.rs
    ├── fixtures/
    │   ├── tasks.json
    │   └── export_expected.csv
    ├── task_tests.rs
    ├── store_tests.rs
    ├── filter_tests.rs
    └── export_tests.rs
```

### Código fuente (src/)

```rust
// src/lib.rs
pub mod task;
pub mod store;
pub mod filter;
pub mod export;

pub use task::{Task, Priority, Status};
pub use store::TaskStore;
pub use filter::{Filter, FilterBuilder};
pub use export::{Exporter, ExportFormat};
```

```rust
// src/task.rs

use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Priority {
    Low,
    Medium,
    High,
    Critical,
}

impl Priority {
    pub fn as_number(&self) -> u8 {
        match self {
            Priority::Low => 1,
            Priority::Medium => 2,
            Priority::High => 3,
            Priority::Critical => 4,
        }
    }
}

impl fmt::Display for Priority {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Priority::Low => write!(f, "low"),
            Priority::Medium => write!(f, "medium"),
            Priority::High => write!(f, "high"),
            Priority::Critical => write!(f, "critical"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Status {
    Todo,
    InProgress,
    Done,
    Cancelled,
}

impl fmt::Display for Status {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Status::Todo => write!(f, "todo"),
            Status::InProgress => write!(f, "in_progress"),
            Status::Done => write!(f, "done"),
            Status::Cancelled => write!(f, "cancelled"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Task {
    id: u64,
    title: String,
    description: String,
    priority: Priority,
    status: Status,
    tags: Vec<String>,
}

impl Task {
    pub fn new(id: u64, title: &str) -> Self {
        Task {
            id,
            title: title.to_string(),
            description: String::new(),
            priority: Priority::Medium,
            status: Status::Todo,
            tags: Vec::new(),
        }
    }

    pub fn id(&self) -> u64 { self.id }
    pub fn title(&self) -> &str { &self.title }
    pub fn description(&self) -> &str { &self.description }
    pub fn priority(&self) -> &Priority { &self.priority }
    pub fn status(&self) -> &Status { &self.status }
    pub fn tags(&self) -> &[String] { &self.tags }

    pub fn set_description(&mut self, desc: &str) { self.description = desc.to_string(); }
    pub fn set_priority(&mut self, p: Priority) { self.priority = p; }
    pub fn set_status(&mut self, s: Status) { self.status = s; }
    pub fn add_tag(&mut self, tag: &str) { self.tags.push(tag.to_string()); }

    pub fn is_open(&self) -> bool {
        matches!(self.status, Status::Todo | Status::InProgress)
    }

    pub fn is_high_priority(&self) -> bool {
        matches!(self.priority, Priority::High | Priority::Critical)
    }
}
```

```rust
// src/store.rs

use crate::task::Task;

pub struct TaskStore {
    tasks: Vec<Task>,
    next_id: u64,
}

impl TaskStore {
    pub fn new() -> Self {
        TaskStore {
            tasks: Vec::new(),
            next_id: 1,
        }
    }

    pub fn add(&mut self, title: &str) -> &Task {
        let task = Task::new(self.next_id, title);
        self.next_id += 1;
        self.tasks.push(task);
        self.tasks.last().unwrap()
    }

    pub fn get(&self, id: u64) -> Option<&Task> {
        self.tasks.iter().find(|t| t.id() == id)
    }

    pub fn get_mut(&mut self, id: u64) -> Option<&mut Task> {
        self.tasks.iter_mut().find(|t| t.id() == id)
    }

    pub fn remove(&mut self, id: u64) -> Option<Task> {
        let pos = self.tasks.iter().position(|t| t.id() == id)?;
        Some(self.tasks.remove(pos))
    }

    pub fn all(&self) -> &[Task] {
        &self.tasks
    }

    pub fn count(&self) -> usize {
        self.tasks.len()
    }

    pub fn clear(&mut self) {
        self.tasks.clear();
    }
}
```

```rust
// src/filter.rs

use crate::task::{Task, Priority, Status};

pub struct Filter {
    status: Option<Status>,
    priority: Option<Priority>,
    tag: Option<String>,
    title_contains: Option<String>,
}

impl Filter {
    pub fn matches(&self, task: &Task) -> bool {
        if let Some(ref status) = self.status {
            if task.status() != status {
                return false;
            }
        }
        if let Some(ref priority) = self.priority {
            if task.priority() != priority {
                return false;
            }
        }
        if let Some(ref tag) = self.tag {
            if !task.tags().contains(tag) {
                return false;
            }
        }
        if let Some(ref text) = self.title_contains {
            if !task.title().to_lowercase().contains(&text.to_lowercase()) {
                return false;
            }
        }
        true
    }

    pub fn apply<'a>(&self, tasks: &'a [Task]) -> Vec<&'a Task> {
        tasks.iter().filter(|t| self.matches(t)).collect()
    }
}

pub struct FilterBuilder {
    filter: Filter,
}

impl FilterBuilder {
    pub fn new() -> Self {
        FilterBuilder {
            filter: Filter {
                status: None,
                priority: None,
                tag: None,
                title_contains: None,
            },
        }
    }

    pub fn status(mut self, status: Status) -> Self {
        self.filter.status = Some(status);
        self
    }

    pub fn priority(mut self, priority: Priority) -> Self {
        self.filter.priority = Some(priority);
        self
    }

    pub fn tag(mut self, tag: &str) -> Self {
        self.filter.tag = Some(tag.to_string());
        self
    }

    pub fn title_contains(mut self, text: &str) -> Self {
        self.filter.title_contains = Some(text.to_string());
        self
    }

    pub fn build(self) -> Filter {
        self.filter
    }
}
```

```rust
// src/export.rs

use crate::task::Task;
use std::path::Path;

#[derive(Debug, Clone, Copy)]
pub enum ExportFormat {
    Csv,
    Json,
    Text,
}

pub struct Exporter;

impl Exporter {
    pub fn export_to_string(tasks: &[Task], format: ExportFormat) -> String {
        match format {
            ExportFormat::Csv => Self::to_csv(tasks),
            ExportFormat::Json => Self::to_json(tasks),
            ExportFormat::Text => Self::to_text(tasks),
        }
    }

    pub fn export_to_file(
        tasks: &[Task],
        format: ExportFormat,
        path: &Path,
    ) -> std::io::Result<()> {
        let content = Self::export_to_string(tasks, format);
        std::fs::write(path, content)
    }

    fn to_csv(tasks: &[Task]) -> String {
        let mut output = String::from("id,title,priority,status,tags\n");
        for task in tasks {
            output.push_str(&format!(
                "{},{},{},{},{}\n",
                task.id(),
                task.title(),
                task.priority(),
                task.status(),
                task.tags().join(";")
            ));
        }
        output
    }

    fn to_json(tasks: &[Task]) -> String {
        let mut output = String::from("[\n");
        for (i, task) in tasks.iter().enumerate() {
            if i > 0 {
                output.push_str(",\n");
            }
            output.push_str(&format!(
                "  {{\"id\": {}, \"title\": \"{}\", \"priority\": \"{}\", \"status\": \"{}\"}}",
                task.id(), task.title(), task.priority(), task.status()
            ));
        }
        output.push_str("\n]");
        output
    }

    fn to_text(tasks: &[Task]) -> String {
        let mut output = String::new();
        for task in tasks {
            let marker = if task.is_open() { "[ ]" } else { "[x]" };
            output.push_str(&format!(
                "{} [{}] {} ({})\n",
                marker,
                task.priority(),
                task.title(),
                task.status()
            ));
        }
        output
    }
}
```

### Test infrastructure completa (tests/common/)

```rust
// tests/common/mod.rs

#![allow(dead_code)]

pub mod builders;
pub mod fixtures;
pub mod assertions;
pub mod fakes;
pub mod cleanup;

// Re-exports de conveniencia
pub use builders::TaskBuilder;
pub use fixtures::{sample_tasks, populated_store};
pub use assertions::{assert_task_matches, assert_tasks_filtered};
pub use cleanup::TestDir;
```

```rust
// tests/common/builders.rs

use todo_service::{Task, Priority, Status};
use std::sync::atomic::{AtomicU64, Ordering};

static TASK_ID: AtomicU64 = AtomicU64::new(1000);

pub struct TaskBuilder {
    id: u64,
    title: String,
    description: String,
    priority: Priority,
    status: Status,
    tags: Vec<String>,
}

impl TaskBuilder {
    pub fn new() -> Self {
        let id = TASK_ID.fetch_add(1, Ordering::SeqCst);
        TaskBuilder {
            id,
            title: format!("Task_{}", id),
            description: String::new(),
            priority: Priority::Medium,
            status: Status::Todo,
            tags: Vec::new(),
        }
    }

    pub fn title(mut self, title: &str) -> Self {
        self.title = title.to_string();
        self
    }

    pub fn description(mut self, desc: &str) -> Self {
        self.description = desc.to_string();
        self
    }

    pub fn priority(mut self, p: Priority) -> Self {
        self.priority = p;
        self
    }

    pub fn status(mut self, s: Status) -> Self {
        self.status = s;
        self
    }

    pub fn tag(mut self, tag: &str) -> Self {
        self.tags.push(tag.to_string());
        self
    }

    pub fn tags(mut self, tags: &[&str]) -> Self {
        self.tags = tags.iter().map(|t| t.to_string()).collect();
        self
    }

    // Shortcuts semánticos
    pub fn high_priority(self) -> Self { self.priority(Priority::High) }
    pub fn critical(self) -> Self { self.priority(Priority::Critical) }
    pub fn low_priority(self) -> Self { self.priority(Priority::Low) }
    pub fn done(self) -> Self { self.status(Status::Done) }
    pub fn in_progress(self) -> Self { self.status(Status::InProgress) }
    pub fn cancelled(self) -> Self { self.status(Status::Cancelled) }

    pub fn build(self) -> Task {
        let mut task = Task::new(self.id, &self.title);
        task.set_description(&self.description);
        task.set_priority(self.priority);
        task.set_status(self.status);
        for tag in &self.tags {
            task.add_tag(tag);
        }
        task
    }
}
```

```rust
// tests/common/fixtures.rs

use todo_service::{Task, TaskStore, Priority, Status};
use super::builders::TaskBuilder;

/// Colección estándar de tareas para testing.
pub fn sample_tasks() -> Vec<Task> {
    vec![
        TaskBuilder::new()
            .title("Fix login bug")
            .priority(Priority::Critical)
            .tag("bug").tag("auth")
            .build(),
        TaskBuilder::new()
            .title("Add dark mode")
            .priority(Priority::Low)
            .tag("feature").tag("ui")
            .build(),
        TaskBuilder::new()
            .title("Update docs")
            .priority(Priority::Medium)
            .status(Status::InProgress)
            .tag("docs")
            .build(),
        TaskBuilder::new()
            .title("Refactor database layer")
            .priority(Priority::High)
            .status(Status::Done)
            .tag("refactor").tag("db")
            .build(),
        TaskBuilder::new()
            .title("Write unit tests")
            .priority(Priority::High)
            .tag("testing")
            .build(),
        TaskBuilder::new()
            .title("Old feature request")
            .priority(Priority::Low)
            .status(Status::Cancelled)
            .tag("feature")
            .build(),
    ]
}

/// TaskStore pre-poblado con las sample_tasks.
pub fn populated_store() -> TaskStore {
    let mut store = TaskStore::new();
    for task in sample_tasks() {
        let t = store.add(task.title());
        let id = t.id();
        if let Some(t) = store.get_mut(id) {
            t.set_priority(task.priority().clone());
            t.set_status(task.status().clone());
            for tag in task.tags() {
                t.add_tag(tag);
            }
        }
    }
    store
}

/// Genera N tareas con títulos secuenciales.
pub fn generate_tasks(count: usize) -> Vec<Task> {
    (0..count)
        .map(|i| {
            TaskBuilder::new()
                .title(&format!("Generated task {}", i))
                .build()
        })
        .collect()
}
```

```rust
// tests/common/assertions.rs

use todo_service::{Task, Priority, Status};

#[track_caller]
pub fn assert_task_matches(task: &Task, title: &str, priority: &Priority, status: &Status) {
    assert_eq!(task.title(), title, "Title mismatch");
    assert_eq!(task.priority(), priority, "Priority mismatch for '{}'", title);
    assert_eq!(task.status(), status, "Status mismatch for '{}'", title);
}

#[track_caller]
pub fn assert_tasks_filtered(tasks: &[&Task], expected_titles: &[&str]) {
    let actual: Vec<&str> = tasks.iter().map(|t| t.title()).collect();
    assert_eq!(
        actual.len(),
        expected_titles.len(),
        "Expected {} tasks, got {}.\nExpected: {:?}\nActual: {:?}",
        expected_titles.len(),
        actual.len(),
        expected_titles,
        actual
    );
    for title in expected_titles {
        assert!(
            actual.contains(title),
            "Expected task '{}' not found in {:?}",
            title,
            actual
        );
    }
}

#[track_caller]
pub fn assert_all_have_status(tasks: &[&Task], expected: &Status) {
    for task in tasks {
        assert_eq!(
            task.status(), expected,
            "Task '{}' has status {:?}, expected {:?}",
            task.title(), task.status(), expected
        );
    }
}

#[track_caller]
pub fn assert_all_have_tag(tasks: &[&Task], tag: &str) {
    for task in tasks {
        assert!(
            task.tags().iter().any(|t| t == tag),
            "Task '{}' does not have tag '{}'. Tags: {:?}",
            task.title(), tag, task.tags()
        );
    }
}

#[track_caller]
pub fn assert_sorted_by_priority(tasks: &[&Task]) {
    for window in tasks.windows(2) {
        assert!(
            window[0].priority().as_number() >= window[1].priority().as_number(),
            "Tasks not sorted by priority: '{}' ({}) before '{}' ({})",
            window[0].title(), window[0].priority(),
            window[1].title(), window[1].priority()
        );
    }
}
```

```rust
// tests/common/cleanup.rs

use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU32, Ordering};

static DIR_ID: AtomicU32 = AtomicU32::new(0);

pub struct TestDir {
    path: PathBuf,
}

impl TestDir {
    pub fn new(prefix: &str) -> Self {
        let id = DIR_ID.fetch_add(1, Ordering::SeqCst);
        let path = std::env::temp_dir()
            .join("todo_tests")
            .join(format!("{}_{}_{}", prefix, std::process::id(), id));
        std::fs::create_dir_all(&path).unwrap();
        TestDir { path }
    }

    pub fn path(&self) -> &Path { &self.path }

    pub fn file_path(&self, name: &str) -> PathBuf {
        self.path.join(name)
    }

    pub fn read_file(&self, name: &str) -> String {
        std::fs::read_to_string(self.path.join(name)).unwrap()
    }
}

impl Drop for TestDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.path);
    }
}
```

### Integration tests usando la infrastructure

```rust
// tests/task_tests.rs
mod common;

use todo_service::{Task, Priority, Status};
use common::TaskBuilder;

#[test]
fn test_new_task_defaults() {
    let task = TaskBuilder::new().title("Test").build();
    common::assert_task_matches(&task, "Test", &Priority::Medium, &Status::Todo);
    assert!(task.is_open());
    assert!(!task.is_high_priority());
}

#[test]
fn test_task_with_tags() {
    let task = TaskBuilder::new()
        .title("Tagged")
        .tags(&["rust", "testing"])
        .build();
    assert_eq!(task.tags().len(), 2);
    assert!(task.tags().contains(&"rust".to_string()));
}

#[test]
fn test_done_task_is_not_open() {
    let task = TaskBuilder::new().done().build();
    assert!(!task.is_open());
}

#[test]
fn test_critical_is_high_priority() {
    let task = TaskBuilder::new().critical().build();
    assert!(task.is_high_priority());
}
```

```rust
// tests/store_tests.rs
mod common;

use todo_service::TaskStore;

#[test]
fn test_add_and_retrieve() {
    let mut store = TaskStore::new();
    let task = store.add("New task");
    let id = task.id();

    let found = store.get(id).unwrap();
    assert_eq!(found.title(), "New task");
}

#[test]
fn test_remove_task() {
    let mut store = TaskStore::new();
    let task = store.add("To remove");
    let id = task.id();

    let removed = store.remove(id).unwrap();
    assert_eq!(removed.title(), "To remove");
    assert!(store.get(id).is_none());
}

#[test]
fn test_populated_store() {
    let store = common::populated_store();
    assert_eq!(store.count(), 6);
}

#[test]
fn test_clear_store() {
    let mut store = common::populated_store();
    assert!(store.count() > 0);
    store.clear();
    assert_eq!(store.count(), 0);
}
```

```rust
// tests/filter_tests.rs
mod common;

use todo_service::{FilterBuilder, Priority, Status};

#[test]
fn test_filter_by_status() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .status(Status::Todo)
        .build();

    let result = filter.apply(&tasks);
    common::assert_all_have_status(&result, &Status::Todo);
}

#[test]
fn test_filter_by_priority() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .priority(Priority::High)
        .build();

    let result = filter.apply(&tasks);
    for task in &result {
        assert_eq!(task.priority(), &Priority::High);
    }
}

#[test]
fn test_filter_by_tag() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .tag("bug")
        .build();

    let result = filter.apply(&tasks);
    common::assert_all_have_tag(&result, "bug");
}

#[test]
fn test_filter_by_title() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .title_contains("bug")
        .build();

    let result = filter.apply(&tasks);
    common::assert_tasks_filtered(&result, &["Fix login bug"]);
}

#[test]
fn test_combined_filter() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .status(Status::Todo)
        .priority(Priority::High)
        .build();

    let result = filter.apply(&tasks);
    common::assert_all_have_status(&result, &Status::Todo);
    for task in &result {
        assert_eq!(task.priority(), &Priority::High);
    }
}

#[test]
fn test_filter_no_matches() {
    let tasks = common::sample_tasks();
    let filter = FilterBuilder::new()
        .tag("nonexistent_tag")
        .build();

    let result = filter.apply(&tasks);
    assert!(result.is_empty());
}
```

```rust
// tests/export_tests.rs
mod common;

use todo_service::{Exporter, ExportFormat};

#[test]
fn test_export_csv() {
    let tasks = common::sample_tasks();
    let csv = Exporter::export_to_string(&tasks, ExportFormat::Csv);

    assert!(csv.starts_with("id,title,priority,status,tags\n"));
    assert!(csv.contains("Fix login bug"));
    assert!(csv.contains("critical"));
    // Contar líneas: header + N tasks
    assert_eq!(csv.lines().count(), tasks.len() + 1);
}

#[test]
fn test_export_json() {
    let tasks = common::sample_tasks();
    let json = Exporter::export_to_string(&tasks, ExportFormat::Json);

    assert!(json.starts_with("["));
    assert!(json.ends_with("]"));
    assert!(json.contains("\"title\": \"Fix login bug\""));
}

#[test]
fn test_export_text() {
    let tasks = common::sample_tasks();
    let text = Exporter::export_to_string(&tasks, ExportFormat::Text);

    // Tareas abiertas tienen [ ], cerradas [x]
    assert!(text.contains("[ ]"));
    assert!(text.contains("[x]"));
}

#[test]
fn test_export_empty() {
    let tasks: Vec<todo_service::Task> = vec![];
    let csv = Exporter::export_to_string(&tasks, ExportFormat::Csv);
    assert_eq!(csv, "id,title,priority,status,tags\n");
}

#[test]
fn test_export_to_file() {
    let dir = common::TestDir::new("export");
    let tasks = common::sample_tasks();

    Exporter::export_to_file(
        &tasks,
        ExportFormat::Csv,
        &dir.file_path("export.csv"),
    ).unwrap();

    let content = dir.read_file("export.csv");
    assert!(content.contains("Fix login bug"));
}
```

---

## 21. Programa de práctica

Implementa la **infraestructura de test completa** para una biblioteca de gestión de inventario (`inventory_lib`).

### Código fuente dado (ya implementado)

```rust
// src/lib.rs
pub mod item;
pub mod warehouse;
pub mod report;

pub use item::{Item, Category};
pub use warehouse::Warehouse;
pub use report::{InventoryReport, ReportFormat};
```

Asume que estos módulos implementan:

- `Item`: producto con `id`, `name`, `category`, `quantity`, `price_cents`
- `Category`: enum con `Electronics`, `Clothing`, `Food`, `Books`, `Other`
- `Warehouse`: almacén con métodos `add_item`, `remove_item`, `find_by_name`, `find_by_category`, `total_value`, `low_stock_items(threshold)`
- `InventoryReport`: generador de reportes con `generate(warehouse, format)`
- `ReportFormat`: enum con `Summary`, `Detailed`, `Csv`

### Requisitos de la infraestructura de test

Crea la siguiente estructura de helpers en `tests/common/`:

```
tests/
├── common/
│   ├── mod.rs              // Re-exports + #![allow(dead_code)]
│   ├── builders.rs         // ItemBuilder con defaults válidos
│   │                       // Al menos: new(), name(), category(), quantity(),
│   │                       // price(), out_of_stock(), expensive(), build()
│   │
│   ├── fixtures.rs         // Datos predefinidos:
│   │                       //   sample_items() → Vec<Item>  (al menos 8 items)
│   │                       //   populated_warehouse() → Warehouse con items
│   │                       //   empty_warehouse() → Warehouse vacío
│   │                       //   electronics_only() → Vec<Item> (solo electrónicos)
│   │                       //   mixed_stock() → Vec<Item> (algunos con qty=0)
│   │
│   ├── assertions.rs       // Al menos 5 custom assertions:
│   │                       //   assert_item_matches(item, name, category, qty)
│   │                       //   assert_total_value(warehouse, expected_cents)
│   │                       //   assert_all_in_stock(items)
│   │                       //   assert_none_in_stock(items)
│   │                       //   assert_warehouse_contains(warehouse, item_name)
│   │                       //   Todas con #[track_caller]
│   │
│   └── cleanup.rs          // TestDir para tests de exportación
│
├── item_tests.rs           // 6+ tests usando builders y assertions
├── warehouse_tests.rs      // 8+ tests usando fixtures y assertions
├── filter_tests.rs         // 5+ tests de find_by_category y find_by_name
├── report_tests.rs         // 4+ tests usando TestDir para export a archivo
└── fixtures/
    └── expected_summary.txt  // Output esperado para comparación
```

### Reglas

1. **Cada archivo de test debe usar `mod common;`** y al menos 2 helpers diferentes
2. **ItemBuilder debe generar IDs únicos automáticamente** (AtomicU64)
3. **Todas las custom assertions deben usar `#[track_caller]`**
4. **`populated_warehouse()` debe contener al menos 8 items** de 3+ categorías
5. **Al menos 1 test debe usar `TestDir`** para verificar exportación a archivo
6. **No acceder a campos internos** — solo API pública
7. **Los helpers deben producir mensajes de error descriptivos** (no `assert!(x)` desnudo)

---

## 22. Ejercicios

### Ejercicio 1: Diseño de module common

Dado este test con duplicación, refactorízalo extrayendo helpers a `tests/common/mod.rs`:

```rust
// tests/auth_tests.rs

use mi_app::{AuthService, User, Database, Config};

#[test]
fn test_login_success() {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    let config = Config::new();
    config.set("session_timeout", "3600");
    config.set("max_attempts", "3");
    let auth = AuthService::new(&db, &config);
    let user = User::create(&db, "alice", "alice@test.com", "Str0ng!Pass").unwrap();

    let session = auth.login("alice", "Str0ng!Pass").unwrap();
    assert!(session.is_valid());
    assert_eq!(session.user_id(), user.id());
}

#[test]
fn test_login_wrong_password() {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    let config = Config::new();
    config.set("session_timeout", "3600");
    config.set("max_attempts", "3");
    let auth = AuthService::new(&db, &config);
    User::create(&db, "bob", "bob@test.com", "Str0ng!Pass").unwrap();

    let result = auth.login("bob", "wrong_password");
    assert!(result.is_err());
}

#[test]
fn test_lockout_after_max_attempts() {
    let db = Database::new_in_memory("test").unwrap();
    db.run_migrations().unwrap();
    let config = Config::new();
    config.set("session_timeout", "3600");
    config.set("max_attempts", "3");
    let auth = AuthService::new(&db, &config);
    User::create(&db, "charlie", "charlie@test.com", "Str0ng!Pass").unwrap();

    for _ in 0..3 {
        let _ = auth.login("charlie", "wrong");
    }
    let result = auth.login("charlie", "Str0ng!Pass");
    assert!(result.is_err());
    assert!(format!("{:?}", result).contains("locked"));
}
```

**a)** Identifica qué código se repite y qué varía entre tests.

**b)** Diseña `tests/common/mod.rs` con: una función de setup, un helper para crear usuario, y una custom assertion.

**c)** Reescribe los 3 tests usando los helpers.

---

### Ejercicio 2: Builder completo

Diseña un builder para esta estructura:

```rust
pub struct HttpRequest {
    pub method: String,       // GET, POST, PUT, DELETE
    pub path: String,         // /api/users
    pub headers: HashMap<String, String>,
    pub query_params: HashMap<String, String>,
    pub body: Option<String>,
    pub authenticated: bool,
    pub api_key: Option<String>,
}
```

**a)** Implementa `HttpRequestBuilder` con defaults razonables (GET, path "/", sin body).

**b)** Añade shortcuts semánticos: `.get(path)`, `.post(path, body)`, `.put(path, body)`, `.delete(path)`.

**c)** Añade `.with_auth(api_key)` que establece `authenticated = true` y el api_key.

**d)** Añade `.header(key, value)` y `.query(key, value)`.

**e)** Escribe 5 tests que usen el builder mostrando diferentes combinaciones.

---

### Ejercicio 3: RAII fixture completa

Implementa un `TestDatabase` con estas características:

**a)** Constructor que crea una DB in-memory con nombre único (AtomicU32).

**b)** Método `seed(data)` que inserta datos iniciales.

**c)** Método `count(table)` que cuenta registros.

**d)** Método `query(sql)` que ejecuta una consulta.

**e)** `Drop` que logea el nombre de la DB eliminada (para debugging).

**f)** Escribe 3 tests que muestren creación, uso, y cleanup automático.

---

### Ejercicio 4: Análisis de organización de helpers

Dado este workspace, identifica problemas y propone mejoras:

```
mi_workspace/
├── crates/
│   ├── auth/
│   │   └── tests/
│   │       ├── common.rs           ← ¿Problema?
│   │       └── auth_tests.rs
│   ├── users/
│   │   └── tests/
│   │       ├── common/
│   │       │   └── mod.rs          ← setup_db() definida aquí
│   │       └── user_tests.rs
│   └── api/
│       └── tests/
│           ├── common/
│           │   └── mod.rs          ← setup_db() DUPLICADA aquí
│           └── api_tests.rs
```

**a)** Identifica 3 problemas en esta estructura.

**b)** ¿Cómo resolverías la duplicación de `setup_db()` entre `users` y `api`?

**c)** Dibuja la estructura corregida.

**d)** Si `auth` y `users` necesitan un `FakeEmailSender` compartido, ¿dónde lo pondrías?

---

## Navegación

- **Anterior**: [T02 - Doc tests](../T02_Doc_tests/README.md)
- **Siguiente**: [T04 - cargo test flags](../T04_Cargo_test_flags/README.md)

---

> **Sección 2: Integration Tests y Doc Tests** — Tópico 3 de 4 completado
>
> - T01: Integration tests — directorio tests/, crate separado, API pública, helpers compartidos ✓
> - T02: Doc tests — ` ``` ` en doc comments, compilación por rustdoc, no_run, ignore, compile_fail ✓
> - T03: Test helpers compartidos — common/mod.rs, fixtures, builders, RAII, custom assertions, fakes ✓
> - T04: cargo test flags (siguiente)
