# T01 - Integration Tests en Rust

## Índice

1. [El problema: tests que cruzan la frontera del módulo](#1-el-problema-tests-que-cruzan-la-frontera-del-módulo)
2. [Qué son los integration tests](#2-qué-son-los-integration-tests)
3. [El directorio tests/](#3-el-directorio-tests)
4. [Cada archivo es un crate separado](#4-cada-archivo-es-un-crate-separado)
5. [use mi_crate:: — acceso exclusivo a la API pública](#5-use-mi_crate--acceso-exclusivo-a-la-api-pública)
6. [Anatomía de un integration test](#6-anatomía-de-un-integration-test)
7. [Ciclo de compilación: cómo cargo compila integration tests](#7-ciclo-de-compilación-cómo-cargo-compila-integration-tests)
8. [Múltiples archivos de integración](#8-múltiples-archivos-de-integración)
9. [Submódulos compartidos: tests/common/mod.rs](#9-submódulos-compartidos-testscommonmodrs)
10. [API pública vs internals: la barrera de visibilidad](#10-api-pública-vs-internals-la-barrera-de-visibilidad)
11. [Setup y teardown en integration tests](#11-setup-y-teardown-en-integration-tests)
12. [Integration tests para binary crates](#12-integration-tests-para-binary-crates)
13. [Integration tests con dependencias externas](#13-integration-tests-con-dependencias-externas)
14. [Organización por feature o funcionalidad](#14-organización-por-feature-o-funcionalidad)
15. [Integration tests vs unit tests: criterios de elección](#15-integration-tests-vs-unit-tests-criterios-de-elección)
16. [Filtrado y ejecución selectiva](#16-filtrado-y-ejecución-selectiva)
17. [Patrones avanzados de integration testing](#17-patrones-avanzados-de-integration-testing)
18. [Comparación con C y Go](#18-comparación-con-c-y-go)
19. [Errores comunes](#19-errores-comunes)
20. [Ejemplo completo: biblioteca de validación de datos](#20-ejemplo-completo-biblioteca-de-validación-de-datos)
21. [Programa de práctica](#21-programa-de-práctica)
22. [Ejercicios](#22-ejercicios)

---

## 1. El problema: tests que cruzan la frontera del módulo

En T04 (Organización) vimos que los unit tests viven **dentro** del módulo que prueban. Esto les da superpoderes: acceso a funciones privadas, a structs internos, a todo lo que `use super::*` importe. Pero precisamente esa intimidad es un problema:

```
Problema de los unit tests aislados:

  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
  │  parser.rs   │    │ validator.rs │    │  engine.rs   │
  │             │    │             │    │             │
  │  mod tests  │    │  mod tests  │    │  mod tests  │
  │  ✓ 15 pass  │    │  ✓ 12 pass  │    │  ✓ 20 pass  │
  └─────────────┘    └─────────────┘    └─────────────┘
         │                  │                  │
         └──────────────────┼──────────────────┘
                            │
                    ¿Funcionan juntos?
                    ¿La API pública es coherente?
                    ¿Los contratos se respetan?
                            │
                         ??? ← Sin respuesta
```

Cada módulo funciona perfectamente **en aislamiento**, pero eso no garantiza que:

- El parser produzca output que el validator acepte
- El validator pase datos en el formato que el engine espera
- La API pública exponga la funcionalidad correcta
- Los tipos que cruzan fronteras de módulo sean compatibles
- El flujo completo (input → parse → validate → process → output) funcione

Los unit tests prueban **implementación interna**. Los integration tests prueban **comportamiento externo** — usan tu biblioteca exactamente como lo haría un usuario real.

```
Perspectiva del unit test vs integration test:

  Unit test (dentro del módulo):
  ┌─────────────────────────────────┐
  │  mod parser {                   │
  │      fn tokenize(s: &str)  ←── │── Acceso directo a fn privada
  │      fn parse_expr(tokens) ←── │── Acceso directo a fn privada
  │      pub fn parse(s: &str) ←── │── También acceso a fn pública
  │                                 │
  │      mod tests {                │
  │          use super::*;     ←── │── Ve TODO el módulo
  │          #[test]                │
  │          fn test_tokenize() {}  │
  │      }                          │
  │  }                              │
  └─────────────────────────────────┘

  Integration test (fuera de la crate):
  ┌─────────────────────────────────┐
  │  // tests/parsing.rs            │
  │                                 │
  │  use mi_crate::parser::parse;   │── Solo ve lo que es pub
  │                                 │
  │  #[test]                        │
  │  fn test_parse_expression() {   │
  │      let ast = parse("2 + 3");  │── Usa la API como un usuario
  │      // ...                     │
  │  }                              │
  └─────────────────────────────────┘
                │
                │ NO puede ver: tokenize(), parse_expr(),
                │               structs privados, constantes internas
```

Esta restricción no es una limitación — es exactamente el punto. Si tu API pública no permite probar algo, probablemente necesitas rediseñar la API, no saltar la barrera.

---

## 2. Qué son los integration tests

Un integration test en Rust es un test que:

1. **Vive fuera de `src/`** — en el directorio `tests/` de la raíz del proyecto
2. **Es un crate separado** — se compila como un binario independiente
3. **Solo ve la API pública** — usa `use mi_crate::` como cualquier dependencia externa
4. **No necesita `#[cfg(test)]`** — el directorio `tests/` solo se compila con `cargo test`
5. **No necesita `mod tests`** — cada archivo es ya un crate de test

```
Mapa mental — Las 3 zonas de testing en Rust:

  ┌─────────────────────────────────────────────────────┐
  │                    Tu Proyecto                       │
  │                                                     │
  │  src/                          tests/                │
  │  ┌──────────────────┐         ┌──────────────────┐  │
  │  │  ZONA 1: Unit    │         │  ZONA 2:         │  │
  │  │  #[cfg(test)]    │         │  Integration     │  │
  │  │  mod tests { }   │         │                  │  │
  │  │  Ve: todo        │         │  Ve: solo pub    │  │
  │  │  Granularidad:   │         │  Granularidad:   │  │
  │  │  función/módulo  │         │  API/flujo       │  │
  │  └──────────────────┘         └──────────────────┘  │
  │                                                     │
  │  /// Doc comments              Cargo.toml            │
  │  ┌──────────────────┐         ┌──────────────────┐  │
  │  │  ZONA 3: Doc     │         │  [dev-            │  │
  │  │  tests           │         │   dependencies]  │  │
  │  │  ```rust         │         │  Solo para       │  │
  │  │  // ejemplo      │         │  test/bench      │  │
  │  │  ```             │         └──────────────────┘  │
  │  │  Ve: solo pub    │                               │
  │  └──────────────────┘                               │
  └─────────────────────────────────────────────────────┘
```

La zona 2 (Integration) es el tema de este tópico. Veremos doc tests (zona 3) en T02.

---

## 3. El directorio tests/

El directorio `tests/` es **mágico** para Cargo. No necesitas declararlo en `Cargo.toml` ni configurar nada. Cargo lo detecta automáticamente:

```
mi_proyecto/
├── Cargo.toml
├── src/
│   ├── lib.rs          // Tu biblioteca
│   ├── parser.rs
│   └── validator.rs
└── tests/              ← Cargo busca este directorio automáticamente
    ├── parsing.rs      ← Cada .rs es un integration test separado
    ├── validation.rs
    └── full_flow.rs
```

### Reglas del directorio tests/

| Regla | Detalle |
|-------|---------|
| **Ubicación** | Debe estar en la raíz del proyecto, al mismo nivel que `src/` y `Cargo.toml` |
| **Nombre** | Exactamente `tests` (no `test`, no `Tests`, no `testing`) |
| **Detección** | Automática — Cargo lo busca sin configuración |
| **Compilación** | Solo con `cargo test`, nunca con `cargo build` |
| **Cada archivo .rs** | Se compila como un crate binario independiente |
| **Subdirectorios** | Los archivos en subdirectorios NO son crates de test automáticamente |
| **Cargo.toml** | No necesita entries — solo `[dev-dependencies]` para dependencias de test |

### Qué Cargo hace con tests/

Cuando ejecutas `cargo test`, Cargo realiza **tres fases** de compilación:

```
cargo test ejecuta en este orden:

  Fase 1: Unit tests
  ┌─────────────────────────────────────┐
  │  Compila src/ con cfg(test) activo  │
  │  Ejecuta todos los #[test] en src/  │
  │  Reporta: "running X tests"         │
  └──────────────────┬──────────────────┘
                     │
  Fase 2: Integration tests
  ┌─────────────────────────────────────┐
  │  Para CADA archivo en tests/:       │
  │    1. Compila como crate separado   │
  │    2. Enlaza contra tu lib          │
  │    3. Ejecuta sus #[test]           │
  │  Reporta por archivo:              │
  │    "test result: tests/parsing.rs"  │
  │    "test result: tests/validation"  │
  └──────────────────┬──────────────────┘
                     │
  Fase 3: Doc tests
  ┌─────────────────────────────────────┐
  │  Extrae bloques ```rust de docs     │
  │  Compila y ejecuta cada uno         │
  │  Reporta: "Doc-tests mi_crate"     │
  └─────────────────────────────────────┘
```

### Output de cargo test con integration tests

```
$ cargo test

   Compiling mi_proyecto v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
     Running unittests src/lib.rs (target/debug/deps/mi_proyecto-a1b2c3d4)

running 15 tests
test parser::tests::test_tokenize ... ok
test parser::tests::test_parse_number ... ok
test validator::tests::test_validate_range ... ok
...
test result: ok. 15 passed; 0 failed; 0 ignored

     Running tests/full_flow.rs (target/debug/deps/full_flow-e5f6g7h8)

running 3 tests
test test_complete_pipeline ... ok
test test_error_propagation ... ok
test test_empty_input ... ok
test result: ok. 3 passed; 0 failed; 0 ignored

     Running tests/parsing.rs (target/debug/deps/parsing-i9j0k1l2)

running 5 tests
test test_parse_simple_expression ... ok
test test_parse_nested_expression ... ok
test test_parse_invalid_syntax ... ok
test test_parse_empty_string ... ok
test test_parse_unicode ... ok
test result: ok. 5 passed; 0 failed; 0 ignored

     Running tests/validation.rs (target/debug/deps/validation-m3n4o5p6)

running 4 tests
test test_validate_email ... ok
test test_validate_age ... ok
test test_validate_name ... ok
test test_validate_combined ... ok
test result: ok. 4 passed; 0 failed; 0 ignored

   Doc-tests mi_proyecto

running 2 tests
test src/lib.rs - parse (line 5) ... ok
test src/parser.rs - parser::parse (line 12) ... ok
test result: ok. 2 passed; 0 failed; 0 ignored
```

Observa: cada archivo de `tests/` aparece como una **sección separada** en el output, con su propio binario (`target/debug/deps/full_flow-e5f6g7h8`).

---

## 4. Cada archivo es un crate separado

Esta es la idea más importante de los integration tests y la fuente de la mayoría de confusiones. Cada archivo `.rs` directamente dentro de `tests/` se compila como un **crate binario completamente independiente**.

### Qué significa "crate separado"

```
Cuando escribes tests/parsing.rs, Cargo hace esto internamente:

  1. Crea un binario temporal:
     ┌──────────────────────────────────┐
     │  // Pseudo-código de lo que      │
     │  // Cargo genera internamente    │
     │                                  │
     │  extern crate mi_proyecto;      │
     │                                  │
     │  // Tu código de tests/parsing  │
     │  // se inserta aquí             │
     │                                  │
     │  fn main() {                     │
     │      // test harness generado   │
     │      // ejecuta todos los #[test]│
     │  }                               │
     └──────────────────────────────────┘

  2. Compila este binario enlazándolo contra libmi_proyecto.rlib

  3. Ejecuta el binario resultante
```

### Consecuencias de ser un crate separado

| Consecuencia | Implicación práctica |
|-------------|---------------------|
| **Tiene su propio scope** | No comparte `use` ni imports con otros archivos de tests/ |
| **No ve privados** | Solo accede a items marcados `pub` en tu crate |
| **Compilación independiente** | Cada archivo se compila por separado (más lento, más aislado) |
| **No necesita mod tests** | El archivo entero ya es un contexto de test |
| **No necesita #[cfg(test)]** | Nunca se compila en builds normales |
| **Tiene su propio main()** | Cargo genera el test harness automáticamente |
| **Puede tener módulos propios** | Puede declarar `mod common;` para compartir código |

### Demostración: independencia entre archivos

```rust
// tests/alpha.rs
use mi_proyecto::Widget;

// Esta variable existe SOLO en el crate alpha
const TEST_NAME: &str = "alpha";

#[test]
fn test_widget_creation() {
    let w = Widget::new(TEST_NAME);
    assert!(w.is_valid());
}
```

```rust
// tests/beta.rs
use mi_proyecto::Widget;

// Esta variable NO conflictúa con la de alpha.rs
// Son crates completamente separados
const TEST_NAME: &str = "beta";

#[test]
fn test_widget_display() {
    let w = Widget::new(TEST_NAME);
    assert_eq!(format!("{}", w), "Widget(beta)");
}
```

No hay conflicto entre `TEST_NAME` en alpha y beta porque **nunca se compilan juntos**. Son dos programas separados que casualmente prueban la misma biblioteca.

### Contraste con unit tests

```
Unit tests: UN binario para todo src/

  src/lib.rs ──┐
  src/a.rs ────┤     ┌────────────────────┐
  src/b.rs ────┼────→│  1 binario de test  │
  src/c.rs ────┤     │  con TODOS los      │
  mod tests ───┘     │  #[test] de src/    │
                     └────────────────────┘

Integration tests: UN binario POR archivo

  tests/x.rs ────→  ┌──────────────────┐
                     │  binario x       │
                     └──────────────────┘
  tests/y.rs ────→  ┌──────────────────┐
                     │  binario y       │
                     └──────────────────┘
  tests/z.rs ────→  ┌──────────────────┐
                     │  binario z       │
                     └──────────────────┘
```

Esto tiene implicaciones en tiempo de compilación: si tienes 20 archivos en `tests/`, Cargo compila **20 binarios separados**. En proyectos grandes, esto puede ser significativo (veremos cómo mitigarlo en la sección 14).

---

## 5. use mi_crate:: — acceso exclusivo a la API pública

Desde un integration test, tu crate es una **dependencia externa**. Accedes a ella exactamente como lo haría cualquier usuario de tu biblioteca:

```rust
// tests/api_tests.rs

// Importar desde tu crate — usa el nombre del paquete definido
// en [package] name = "mi_proyecto" de Cargo.toml
use mi_proyecto::parse;
use mi_proyecto::validate;
use mi_proyecto::Config;

#[test]
fn test_parse_and_validate() {
    let config = Config::default();
    let data = parse("input data").unwrap();
    let result = validate(&data, &config);
    assert!(result.is_ok());
}
```

### El nombre del crate

El nombre que usas en `use` viene del campo `name` en `Cargo.toml`:

```toml
[package]
name = "mi-proyecto"    # ← Este nombre, con guiones reemplazados por _
version = "0.1.0"
edition = "2021"
```

```rust
// tests/example.rs

// Los guiones (-) se convierten en underscores (_)
use mi_proyecto::something;  // ← "mi-proyecto" → mi_proyecto
```

**Regla de conversión**: Cargo reemplaza automáticamente `-` por `_` en los nombres de crate. `mi-proyecto` se convierte en `mi_proyecto` en imports.

### Qué puedes y qué no puedes importar

```rust
// src/lib.rs
pub mod parser {
    pub fn parse(input: &str) -> Result<Ast, ParseError> { /* ... */ }

    fn tokenize(input: &str) -> Vec<Token> { /* ... */ }  // privada

    pub struct Ast {
        pub root: Node,
        nodes: Vec<Node>,  // campo privado
    }

    pub struct ParseError {
        pub message: String,
        pub line: usize,
    }

    struct Token {     // struct privado
        kind: TokenKind,
        span: Span,
    }

    enum TokenKind {   // enum privado
        Number,
        Operator,
    }
}
```

```rust
// tests/parsing_tests.rs
use mi_proyecto::parser::parse;       // ✓ función pub
use mi_proyecto::parser::Ast;         // ✓ struct pub
use mi_proyecto::parser::ParseError;  // ✓ struct pub

// Todas estas fallan:
// use mi_proyecto::parser::tokenize;   // ✗ función privada
// use mi_proyecto::parser::Token;      // ✗ struct privado
// use mi_proyecto::parser::TokenKind;  // ✗ enum privado

#[test]
fn test_parse_produces_ast() {
    let ast = parse("2 + 3").unwrap();
    assert_eq!(ast.root.value(), 5);  // ✓ campo pub de Ast

    // ast.nodes  ← ✗ ERROR: campo privado de Ast
}
```

### Mapa de accesibilidad completo

```
Desde un integration test, puedes acceder a:

  ┌─────────────────────────────────────────────────────────┐
  │                    Tu crate (src/)                       │
  │                                                         │
  │  pub fn foo()          ✓ Accesible                      │
  │  pub struct Bar        ✓ Accesible                      │
  │    pub field           ✓ Accesible                      │
  │    private_field       ✗ NO accesible                   │
  │  pub enum Baz          ✓ Accesible                      │
  │  pub trait Qux         ✓ Accesible                      │
  │  pub type Alias        ✓ Accesible                      │
  │  pub const X           ✓ Accesible                      │
  │  pub static Y          ✓ Accesible                      │
  │                                                         │
  │  fn private()          ✗ NO accesible                   │
  │  struct Internal       ✗ NO accesible                   │
  │  pub(crate) fn semi()  ✗ NO accesible                   │
  │  pub(super) fn up()    ✗ NO accesible                   │
  │                                                         │
  │  pub mod sub {                                          │
  │    pub fn visible()    ✓ Accesible via mi_crate::sub::  │
  │    fn hidden()         ✗ NO accesible                   │
  │  }                                                      │
  │                                                         │
  │  mod private_mod {     ✗ Módulo entero NO accesible     │
  │    pub fn inside()     ✗ pub dentro de mod privado = NO │
  │  }                                                      │
  └─────────────────────────────────────────────────────────┘
```

**Punto crucial**: `pub(crate)` **no** es visible desde integration tests. Solo `pub` (sin restricciones) es visible. Esto es porque los integration tests son crates separados — `pub(crate)` solo se aplica dentro de la crate que lo declara.

---

## 6. Anatomía de un integration test

Un integration test es un archivo `.rs` ordinario. No tiene nada "especial" en su sintaxis — la magia está en **dónde vive** (directorio `tests/`):

```rust
// tests/calculator.rs
//
// Este archivo completo es un integration test.
// Se compila como un crate independiente.
// No necesita mod tests ni #[cfg(test)].

// Imports desde tu crate (como si fuera una dependencia externa)
use calculator_lib::{Calculator, Operation, CalcError};

// Imports de la biblioteca estándar o dev-dependencies
use std::f64::consts::PI;

// Constantes y helpers locales a este archivo de test
const EPSILON: f64 = 1e-10;

fn approx_eq(a: f64, b: f64) -> bool {
    (a - b).abs() < EPSILON
}

// Los tests — directamente con #[test], sin wrapper mod tests
#[test]
fn test_basic_addition() {
    let mut calc = Calculator::new();
    calc.push(2.0);
    calc.push(3.0);
    let result = calc.execute(Operation::Add).unwrap();
    assert_eq!(result, 5.0);
}

#[test]
fn test_division_by_zero() {
    let mut calc = Calculator::new();
    calc.push(10.0);
    calc.push(0.0);
    let err = calc.execute(Operation::Div).unwrap_err();
    assert_eq!(err, CalcError::DivisionByZero);
}

#[test]
fn test_trigonometric_operations() {
    let mut calc = Calculator::new();
    calc.push(PI / 2.0);
    let result = calc.execute(Operation::Sin).unwrap();
    assert!(approx_eq(result, 1.0));
}

#[test]
fn test_chained_operations() {
    let mut calc = Calculator::new();
    // (2 + 3) * 4 = 20
    calc.push(2.0);
    calc.push(3.0);
    calc.execute(Operation::Add).unwrap();
    calc.push(4.0);
    let result = calc.execute(Operation::Mul).unwrap();
    assert_eq!(result, 20.0);
}
```

### Lo que NO necesitas (y por qué)

```rust
// tests/example.rs

// NO necesitas esto:
// #[cfg(test)]        ← Innecesario: tests/ SOLO se compila en test
// mod tests {         ← Innecesario: el archivo entero es ya test
//     use super::*;   ← No hay "super" — este es el crate raíz
// }

// SÍ necesitas esto:
use mi_crate::Thing;   // Import explícito de tu crate

#[test]                // Directamente, sin wrapper
fn test_thing() {
    let t = Thing::new();
    assert!(t.works());
}
```

### Comparación lado a lado: unit test vs integration test

```rust
// ═══════════════════════════════════
// UNIT TEST (dentro de src/thing.rs)
// ═══════════════════════════════════

pub struct Thing {
    value: i32,       // campo privado
}

impl Thing {
    pub fn new() -> Self {
        Thing { value: 0 }
    }

    pub fn get(&self) -> i32 {
        self.value
    }

    fn internal_process(&mut self) {  // función privada
        self.value *= 2;
    }
}

#[cfg(test)]           // ← Necesario: evita compilar en release
mod tests {            // ← Necesario: namespace + scope
    use super::*;      // ← Necesario: importa del módulo padre

    #[test]
    fn test_internal() {
        let mut t = Thing::new();
        t.internal_process();      // ✓ Puede llamar función privada
        assert_eq!(t.value, 0);    // ✓ Puede leer campo privado
    }
}

// ═══════════════════════════════════════
// INTEGRATION TEST (tests/thing_test.rs)
// ═══════════════════════════════════════

use mi_crate::Thing;   // ← Import explícito, solo pub

#[test]                // ← Directo, sin wrappers
fn test_external() {
    let t = Thing::new();        // ✓ Constructor es pub
    assert_eq!(t.get(), 0);      // ✓ Getter es pub

    // t.internal_process();     // ✗ Función privada
    // t.value                   // ✗ Campo privado
}
```

---

## 7. Ciclo de compilación: cómo cargo compila integration tests

Entender el ciclo de compilación explica el rendimiento y las peculiaridades de los integration tests.

### Paso a paso detallado

```
cargo test (ciclo completo):

  Paso 1: Compilar la biblioteca
  ┌────────────────────────────────────────────────┐
  │  rustc src/lib.rs → libmi_proyecto.rlib        │
  │  (compilación normal, sin cfg(test))           │
  │                                                │
  │  Este .rlib es lo que los integration tests    │
  │  van a enlazar. Es la versión "real" de tu     │
  │  crate — exactamente lo que publicarías.       │
  └─────────────────────┬──────────────────────────┘
                        │
  Paso 2: Compilar el binario de unit tests
  ┌────────────────────────────────────────────────┐
  │  rustc src/lib.rs --cfg test                   │
  │  → target/debug/deps/mi_proyecto-HASH          │
  │                                                │
  │  Esta compilación SÍ incluye #[cfg(test)]      │
  │  Es un binario separado del .rlib              │
  └─────────────────────┬──────────────────────────┘
                        │
  Paso 3: Para CADA archivo en tests/:
  ┌────────────────────────────────────────────────┐
  │  tests/alpha.rs:                               │
  │    rustc tests/alpha.rs --extern mi_proyecto   │
  │    → target/debug/deps/alpha-HASH              │
  │                                                │
  │  tests/beta.rs:                                │
  │    rustc tests/beta.rs --extern mi_proyecto    │
  │    → target/debug/deps/beta-HASH               │
  │                                                │
  │  tests/gamma.rs:                               │
  │    rustc tests/gamma.rs --extern mi_proyecto   │
  │    → target/debug/deps/gamma-HASH              │
  │  ...                                           │
  └─────────────────────┬──────────────────────────┘
                        │
  Paso 4: Ejecutar todos los binarios
  ┌────────────────────────────────────────────────┐
  │  ./mi_proyecto-HASH          (unit tests)      │
  │  ./alpha-HASH                (integration)     │
  │  ./beta-HASH                 (integration)     │
  │  ./gamma-HASH                (integration)     │
  │  rustdoc --test src/lib.rs   (doc tests)       │
  └────────────────────────────────────────────────┘
```

### Punto clave: tu crate se compila DOS veces

```
Compilación 1 (para integration tests):
  src/lib.rs  ──rustc──→  libmi_proyecto.rlib
                          (sin cfg(test))
                          (versión "publicada")

Compilación 2 (para unit tests):
  src/lib.rs  ──rustc --cfg test──→  mi_proyecto-HASH (binario test)
                                      (con cfg(test))
                                      (incluye mod tests {})
```

La compilación 1 **no** incluye nada marcado con `#[cfg(test)]`. Esto confirma que los integration tests prueban exactamente el mismo código que tus usuarios ejecutarán.

### Implicación en rendimiento

```
Proyecto con N archivos de integration test:

  Compilaciones totales: 1 (librería) + 1 (unit tests) + N (integration tests)
  Binarios generados:    1 (unit test) + N (integration test)

  Si N = 1:   3 compilaciones, 2 binarios  ← rápido
  Si N = 5:   7 compilaciones, 6 binarios  ← aceptable
  Si N = 20:  22 compilaciones, 21 binarios ← lento
  Si N = 50:  52 compilaciones, 51 binarios ← MUY lento

  Estrategia de mitigación: un solo archivo que re-exporta módulos
  (lo veremos en la sección 14)
```

---

## 8. Múltiples archivos de integración

En proyectos reales, organizas los integration tests en múltiples archivos según funcionalidad:

### Organización por funcionalidad

```
tests/
├── api_creation.rs        // Tests de crear recursos via API
├── api_querying.rs        // Tests de consultar recursos
├── api_modification.rs    // Tests de modificar recursos
├── api_deletion.rs        // Tests de eliminar recursos
├── error_handling.rs      // Tests de manejo de errores
└── edge_cases.rs          // Tests de casos límite
```

### Ejemplo concreto: biblioteca de colecciones

```
mi_colecciones/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── stack.rs
│   ├── queue.rs
│   └── sorted_vec.rs
└── tests/
    ├── stack_tests.rs         // Integration tests para Stack
    ├── queue_tests.rs         // Integration tests para Queue
    ├── sorted_vec_tests.rs    // Integration tests para SortedVec
    ├── interop.rs             // Tests de interoperabilidad entre colecciones
    └── common/
        └── mod.rs             // Helpers compartidos (NO es un test)
```

```rust
// tests/stack_tests.rs
use mi_colecciones::Stack;

#[test]
fn test_push_and_pop() {
    let mut s = Stack::new();
    s.push(1);
    s.push(2);
    s.push(3);
    assert_eq!(s.pop(), Some(3));
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert_eq!(s.pop(), None);
}

#[test]
fn test_peek_does_not_remove() {
    let mut s = Stack::new();
    s.push(42);
    assert_eq!(s.peek(), Some(&42));
    assert_eq!(s.peek(), Some(&42));  // Sigue ahí
    assert_eq!(s.len(), 1);
}

#[test]
fn test_stack_is_lifo() {
    let mut s = Stack::new();
    for i in 0..100 {
        s.push(i);
    }
    for i in (0..100).rev() {
        assert_eq!(s.pop(), Some(i));
    }
}
```

```rust
// tests/queue_tests.rs
use mi_colecciones::Queue;

#[test]
fn test_enqueue_and_dequeue() {
    let mut q = Queue::new();
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    assert_eq!(q.dequeue(), Some(1));  // FIFO: primero entra, primero sale
    assert_eq!(q.dequeue(), Some(2));
    assert_eq!(q.dequeue(), Some(3));
    assert_eq!(q.dequeue(), None);
}

#[test]
fn test_queue_is_fifo() {
    let mut q = Queue::new();
    for i in 0..100 {
        q.enqueue(i);
    }
    for i in 0..100 {
        assert_eq!(q.dequeue(), Some(i));  // Sale en el mismo orden
    }
}
```

```rust
// tests/interop.rs — Tests que cruzan módulos
use mi_colecciones::{Stack, Queue, SortedVec};

#[test]
fn test_stack_to_queue_transfer() {
    // Transferir elementos de stack a queue invierte el orden
    let mut stack = Stack::new();
    stack.push(1);
    stack.push(2);
    stack.push(3);

    let mut queue = Queue::new();
    while let Some(val) = stack.pop() {
        queue.enqueue(val);
    }

    // Stack era LIFO → 3, 2, 1
    // Queue es FIFO  → sale 3, 2, 1
    assert_eq!(queue.dequeue(), Some(3));
    assert_eq!(queue.dequeue(), Some(2));
    assert_eq!(queue.dequeue(), Some(1));
}

#[test]
fn test_sorted_vec_from_stack() {
    let mut stack = Stack::new();
    stack.push(5);
    stack.push(1);
    stack.push(3);

    let mut sorted = SortedVec::new();
    while let Some(val) = stack.pop() {
        sorted.insert(val);
    }

    assert_eq!(sorted.as_slice(), &[1, 3, 5]);
}

#[test]
fn test_all_collections_empty_behavior() {
    // Todas las colecciones vacías deben comportarse consistentemente
    let stack: Stack<i32> = Stack::new();
    let queue: Queue<i32> = Queue::new();
    let sorted: SortedVec<i32> = SortedVec::new();

    assert!(stack.is_empty());
    assert!(queue.is_empty());
    assert!(sorted.is_empty());

    assert_eq!(stack.len(), 0);
    assert_eq!(queue.len(), 0);
    assert_eq!(sorted.len(), 0);
}
```

### Output con múltiples archivos

```
$ cargo test

     Running unittests src/lib.rs (target/debug/deps/mi_colecciones-abc123)
running 12 tests
...
test result: ok. 12 passed; 0 failed

     Running tests/interop.rs (target/debug/deps/interop-def456)
running 3 tests
test test_stack_to_queue_transfer ... ok
test test_sorted_vec_from_stack ... ok
test test_all_collections_empty_behavior ... ok
test result: ok. 3 passed; 0 failed

     Running tests/queue_tests.rs (target/debug/deps/queue_tests-ghi789)
running 2 tests
test test_enqueue_and_dequeue ... ok
test test_queue_is_fifo ... ok
test result: ok. 2 passed; 0 failed

     Running tests/sorted_vec_tests.rs (target/debug/deps/sorted_vec_tests-jkl012)
running 4 tests
...
test result: ok. 4 passed; 0 failed

     Running tests/stack_tests.rs (target/debug/deps/stack_tests-mno345)
running 3 tests
test test_push_and_pop ... ok
test test_peek_does_not_remove ... ok
test test_stack_is_lifo ... ok
test result: ok. 3 passed; 0 failed
```

---

## 9. Submódulos compartidos: tests/common/mod.rs

Cuando múltiples archivos de test necesitan compartir helpers, fixtures o utilidades, el patrón es crear un **submódulo compartido**:

### El problema

```rust
// tests/alpha.rs
fn setup_database() -> TestDb {    // Helper duplicado
    TestDb::new("test.db")
}

#[test]
fn test_alpha() {
    let db = setup_database();
    // ...
}
```

```rust
// tests/beta.rs
fn setup_database() -> TestDb {    // ¡Código duplicado!
    TestDb::new("test.db")
}

#[test]
fn test_beta() {
    let db = setup_database();
    // ...
}
```

### La solución: tests/common/mod.rs

```
tests/
├── common/
│   └── mod.rs          ← Helpers compartidos (NO es un test)
├── alpha.rs
└── beta.rs
```

**Regla crítica**: El archivo debe ser `common/mod.rs`, NO `common.rs`. Si usas `common.rs`, Cargo lo tratará como un archivo de integration test y aparecerá en el output como:

```
     Running tests/common.rs (target/debug/deps/common-xyz789)
running 0 tests
test result: ok. 0 passed; 0 failed    ← Sección vacía inútil
```

Con `common/mod.rs`, Cargo **ignora** los subdirectorios — no los compila como crates de test.

### Por qué mod.rs y no common.rs

```
Regla de Cargo para tests/:

  tests/
  ├── foo.rs          ← SÍ compilado como crate de test
  ├── bar.rs          ← SÍ compilado como crate de test
  ├── baz.rs          ← SÍ compilado como crate de test
  │
  ├── common.rs       ← SÍ compilado (¡NO queremos esto!)
  │
  └── helpers/        ← Subdirectorio: IGNORADO por Cargo
      └── mod.rs      ← No compilado como test (correcto)
      └── setup.rs    ← No compilado como test (correcto)
```

Cargo solo compila como integration tests los archivos `.rs` **directamente** en `tests/`. Los archivos en subdirectorios son ignorados y están disponibles como módulos si otro archivo los importa con `mod`.

### Implementación completa

```rust
// tests/common/mod.rs

use mi_proyecto::Database;
use std::sync::atomic::{AtomicU32, Ordering};

// Contador global para nombres de DB únicos (evita colisiones entre tests)
static DB_COUNTER: AtomicU32 = AtomicU32::new(0);

/// Crea una base de datos temporal para testing.
/// Cada invocación produce un nombre único.
pub fn setup_test_db() -> Database {
    let id = DB_COUNTER.fetch_add(1, Ordering::SeqCst);
    let name = format!("test_db_{}", id);
    let db = Database::new_in_memory(&name).unwrap();
    db.run_migrations().unwrap();
    db.seed_test_data().unwrap();
    db
}

/// Datos de prueba estándar
pub fn sample_users() -> Vec<(&'static str, &'static str, u32)> {
    vec![
        ("Alice", "alice@example.com", 30),
        ("Bob", "bob@example.com", 25),
        ("Charlie", "charlie@example.com", 35),
    ]
}

/// Verifica que un resultado tiene exactamente N elementos
pub fn assert_count<T>(items: &[T], expected: usize) {
    assert_eq!(
        items.len(),
        expected,
        "Expected {} items, got {}",
        expected,
        items.len()
    );
}

/// Limpia recursos después de un test
pub fn cleanup_temp_files(prefix: &str) {
    let temp_dir = std::env::temp_dir();
    if let Ok(entries) = std::fs::read_dir(&temp_dir) {
        for entry in entries.flatten() {
            if entry
                .file_name()
                .to_string_lossy()
                .starts_with(prefix)
            {
                let _ = std::fs::remove_file(entry.path());
            }
        }
    }
}
```

```rust
// tests/user_creation.rs

mod common;  // ← Importa tests/common/mod.rs

use mi_proyecto::{User, UserStore};

#[test]
fn test_create_single_user() {
    let db = common::setup_test_db();
    let store = UserStore::new(db);

    let user = store.create("Alice", "alice@example.com", 30).unwrap();
    assert_eq!(user.name(), "Alice");
    assert_eq!(user.email(), "alice@example.com");
}

#[test]
fn test_create_multiple_users() {
    let db = common::setup_test_db();
    let store = UserStore::new(db);

    for (name, email, age) in common::sample_users() {
        store.create(name, email, age).unwrap();
    }

    let all_users = store.list_all().unwrap();
    common::assert_count(&all_users, 3);
}
```

```rust
// tests/user_queries.rs

mod common;  // ← Cada archivo debe importar por separado

use mi_proyecto::UserStore;

#[test]
fn test_find_by_email() {
    let db = common::setup_test_db();
    let store = UserStore::new(db);

    // Crear usuarios de prueba
    for (name, email, age) in common::sample_users() {
        store.create(name, email, age).unwrap();
    }

    let user = store.find_by_email("bob@example.com").unwrap().unwrap();
    assert_eq!(user.name(), "Bob");
}

#[test]
fn test_find_nonexistent_returns_none() {
    let db = common::setup_test_db();
    let store = UserStore::new(db);

    let result = store.find_by_email("nobody@example.com").unwrap();
    assert!(result.is_none());
}
```

### Módulo common con múltiples archivos

Para helpers más complejos, puedes estructurar `common/` con submódulos:

```
tests/
├── common/
│   ├── mod.rs          // Re-exporta submódulos
│   ├── db.rs           // Helpers de base de datos
│   ├── fixtures.rs     // Datos de prueba
│   └── assertions.rs   // Asserts personalizados
├── alpha.rs
└── beta.rs
```

```rust
// tests/common/mod.rs
pub mod db;
pub mod fixtures;
pub mod assertions;
```

```rust
// tests/common/db.rs
use mi_proyecto::Database;

pub fn setup_test_db() -> Database {
    Database::new_in_memory("test").unwrap()
}

pub fn setup_populated_db() -> Database {
    let db = setup_test_db();
    db.run_migrations().unwrap();
    db.seed_test_data().unwrap();
    db
}
```

```rust
// tests/common/fixtures.rs
use mi_proyecto::User;

pub fn admin_user() -> User {
    User::new("admin", "admin@example.com", 40)
}

pub fn guest_user() -> User {
    User::new("guest", "guest@example.com", 20)
}

pub fn sample_users() -> Vec<User> {
    vec![admin_user(), guest_user()]
}
```

```rust
// tests/common/assertions.rs
use mi_proyecto::User;

pub fn assert_user_valid(user: &User) {
    assert!(!user.name().is_empty(), "User name cannot be empty");
    assert!(user.email().contains('@'), "Invalid email: {}", user.email());
    assert!(user.age() > 0, "Age must be positive, got {}", user.age());
}

pub fn assert_users_sorted_by_name(users: &[User]) {
    for window in users.windows(2) {
        assert!(
            window[0].name() <= window[1].name(),
            "Users not sorted: '{}' > '{}'",
            window[0].name(),
            window[1].name()
        );
    }
}
```

```rust
// tests/admin_tests.rs
mod common;

use mi_proyecto::UserStore;

#[test]
fn test_admin_has_valid_fields() {
    let admin = common::fixtures::admin_user();
    common::assertions::assert_user_valid(&admin);
}

#[test]
fn test_users_sorted_after_insert() {
    let db = common::db::setup_populated_db();
    let store = UserStore::new(db);

    for user in common::fixtures::sample_users() {
        store.insert(user).unwrap();
    }

    let users = store.list_sorted_by_name().unwrap();
    common::assertions::assert_users_sorted_by_name(&users);
}
```

---

## 10. API pública vs internals: la barrera de visibilidad

Los integration tests fuerzan una reflexión profunda sobre el diseño de tu API. Si no puedes probar algo desde fuera, hay tres posibilidades:

### Caso 1: La funcionalidad debería ser pública

```rust
// src/lib.rs — Antes (API insuficiente)
pub struct Matrix {
    data: Vec<Vec<f64>>,
    rows: usize,
    cols: usize,
}

impl Matrix {
    pub fn new(rows: usize, cols: usize) -> Self {
        Matrix {
            data: vec![vec![0.0; cols]; rows],
            rows,
            cols,
        }
    }

    pub fn multiply(&self, other: &Matrix) -> Matrix {
        // ... implementación
        # todo!()
    }

    // ¡No hay forma de inspeccionar el resultado!
    // Un integration test no puede verificar los valores
}
```

```rust
// tests/matrix_tests.rs — Problema
use mi_crate::Matrix;

#[test]
fn test_multiply() {
    let a = Matrix::new(2, 2);
    let b = Matrix::new(2, 2);
    let c = a.multiply(&b);

    // ¿Cómo verifico que c tiene los valores correctos?
    // No puedo acceder a c.data (privado)
    // No hay getter público
    // ¡El test no puede verificar nada útil!
}
```

**Solución**: Añadir API pública que permita inspección:

```rust
// src/lib.rs — Después (API completa)
impl Matrix {
    pub fn new(rows: usize, cols: usize) -> Self { /* ... */ }
    pub fn multiply(&self, other: &Matrix) -> Matrix { /* ... */ }

    // Añadir accesores públicos necesarios:
    pub fn get(&self, row: usize, col: usize) -> f64 {
        self.data[row][col]
    }

    pub fn set(&mut self, row: usize, col: usize, value: f64) {
        self.data[row][col] = value;
    }

    pub fn rows(&self) -> usize { self.rows }
    pub fn cols(&self) -> usize { self.cols }
}
```

```rust
// tests/matrix_tests.rs — Ahora funciona
use mi_crate::Matrix;

#[test]
fn test_identity_multiply() {
    let mut a = Matrix::new(2, 2);
    a.set(0, 0, 1.0);
    a.set(1, 1, 1.0);  // Matriz identidad

    let mut b = Matrix::new(2, 2);
    b.set(0, 0, 5.0);
    b.set(0, 1, 3.0);
    b.set(1, 0, 2.0);
    b.set(1, 1, 7.0);

    let c = a.multiply(&b);

    // Ahora podemos verificar cada elemento
    assert_eq!(c.get(0, 0), 5.0);
    assert_eq!(c.get(0, 1), 3.0);
    assert_eq!(c.get(1, 0), 2.0);
    assert_eq!(c.get(1, 1), 7.0);
}
```

### Caso 2: El detalle es genuinamente interno

```rust
// src/lib.rs
pub struct HashMap<K, V> {
    buckets: Vec<Vec<(K, V)>>,  // Implementación interna
    len: usize,
    load_factor: f64,           // Umbral de rehash
}

impl<K: Hash + Eq, V> HashMap<K, V> {
    pub fn insert(&mut self, key: K, value: V) { /* ... */ }
    pub fn get(&self, key: &K) -> Option<&V> { /* ... */ }
    pub fn len(&self) -> usize { self.len }
}
```

No necesitas exponer `buckets` ni `load_factor` públicamente. El número de buckets es un detalle de implementación. Un integration test debe probar **comportamiento**:

```rust
// tests/hashmap_tests.rs
use mi_crate::HashMap;

#[test]
fn test_insert_and_retrieve() {
    let mut map = HashMap::new();
    map.insert("key", "value");
    assert_eq!(map.get(&"key"), Some(&"value"));
}

#[test]
fn test_handles_many_elements() {
    // Si el rehashing funciona, esto no crashea ni pierde datos
    let mut map = HashMap::new();
    for i in 0..10_000 {
        map.insert(i, i * 2);
    }
    assert_eq!(map.len(), 10_000);
    for i in 0..10_000 {
        assert_eq!(map.get(&i), Some(&(i * 2)));
    }
}

// El test verifica que el rehashing funciona
// sin necesidad de inspeccionar los buckets internos
```

### Caso 3: Necesitas un test hook controlado

A veces necesitas verificar comportamiento interno sin exponerlo en la API pública. El patrón es usar `pub(crate)` y testarlo en **unit tests**, no en integration tests:

```rust
// src/cache.rs
pub struct Cache<T> {
    entries: Vec<Option<T>>,
    hits: u64,           // Estadística interna
    misses: u64,         // Estadística interna
}

impl<T> Cache<T> {
    pub fn get(&self, index: usize) -> Option<&T> { /* ... */ }
    pub fn insert(&mut self, index: usize, value: T) { /* ... */ }

    // Solo para testing interno, no para usuarios
    pub(crate) fn hit_rate(&self) -> f64 {
        let total = self.hits + self.misses;
        if total == 0 { 0.0 } else { self.hits as f64 / total as f64 }
    }
}

// Unit test puede acceder a hit_rate()
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hit_rate_accuracy() {
        let mut cache = Cache::new(10);
        cache.insert(0, "hello");
        cache.get(0);  // hit
        cache.get(0);  // hit
        cache.get(1);  // miss
        assert!((cache.hit_rate() - 0.6667).abs() < 0.01);
    }
}
```

```rust
// tests/cache_tests.rs
use mi_crate::Cache;

#[test]
fn test_cache_stores_and_retrieves() {
    let mut cache = Cache::new(10);
    cache.insert(0, "hello");
    assert_eq!(cache.get(0), Some(&"hello"));
}

// cache.hit_rate() ← No accesible (pub(crate))
// Eso está bien — el integration test prueba comportamiento,
// no estadísticas internas
```

### Diagrama de decisión

```
¿Necesito probar X desde un integration test?
│
├─ X es comportamiento observable por el usuario
│  ├─ ¿Hay API pública para verificarlo? → SÍ → Usa la API existente
│  └─ ¿No hay forma de verificar? → Añade getter/método público
│
├─ X es detalle de implementación
│  ├─ ¿Afecta comportamiento observable? → Testea el comportamiento, no el detalle
│  └─ ¿Es puramente interno? → Testea con unit test (pub(crate) si necesario)
│
└─ X es un efecto secundario (archivo, log, red)
   └─ Diseña la API para que el efecto sea verificable
      (retorna Result, acepta Writer genérico, etc.)
```

---

## 11. Setup y teardown en integration tests

A diferencia de frameworks como JUnit (Java) o pytest (Python), Rust no tiene `@Before`/`@After` ni fixtures automáticas. Pero el sistema de ownership y `Drop` proporciona un mecanismo más robusto:

### Patrón 1: Función de setup simple

```rust
// tests/common/mod.rs

pub struct TestContext {
    pub db: Database,
    pub temp_dir: std::path::PathBuf,
    pub config: Config,
}

pub fn setup() -> TestContext {
    let temp_dir = std::env::temp_dir().join(format!(
        "test_{}",
        std::process::id()
    ));
    std::fs::create_dir_all(&temp_dir).unwrap();

    TestContext {
        db: Database::new_in_memory("test").unwrap(),
        temp_dir,
        config: Config::test_defaults(),
    }
}
```

```rust
// tests/feature_tests.rs
mod common;

#[test]
fn test_feature_a() {
    let ctx = common::setup();  // Setup explícito
    // ... usar ctx.db, ctx.temp_dir, ctx.config
}   // ctx se dropea aquí, Drop limpia recursos automáticamente

#[test]
fn test_feature_b() {
    let ctx = common::setup();  // Cada test tiene su propio contexto
    // ...
}
```

### Patrón 2: RAII con Drop para cleanup automático

```rust
// tests/common/mod.rs
use std::path::{Path, PathBuf};

pub struct TestDir {
    path: PathBuf,
}

impl TestDir {
    pub fn new(name: &str) -> Self {
        let path = std::env::temp_dir()
            .join("rust_integration_tests")
            .join(name);
        std::fs::create_dir_all(&path).unwrap();
        TestDir { path }
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn create_file(&self, name: &str, contents: &str) -> PathBuf {
        let file_path = self.path.join(name);
        if let Some(parent) = file_path.parent() {
            std::fs::create_dir_all(parent).unwrap();
        }
        std::fs::write(&file_path, contents).unwrap();
        file_path
    }

    pub fn read_file(&self, name: &str) -> String {
        std::fs::read_to_string(self.path.join(name)).unwrap()
    }

    pub fn file_exists(&self, name: &str) -> bool {
        self.path.join(name).exists()
    }
}

impl Drop for TestDir {
    fn drop(&mut self) {
        // Cleanup automático — se ejecuta cuando el test termina
        // (ya sea por éxito, fallo, o panic)
        let _ = std::fs::remove_dir_all(&self.path);
    }
}
```

```rust
// tests/file_processing.rs
mod common;

use mi_crate::FileProcessor;

#[test]
fn test_process_csv() {
    let dir = common::TestDir::new("csv_test");

    // Crear archivo de prueba
    dir.create_file("input.csv", "name,age\nAlice,30\nBob,25");

    // Procesar
    let processor = FileProcessor::new(dir.path());
    processor.process("input.csv", "output.json").unwrap();

    // Verificar resultado
    let output = dir.read_file("output.json");
    assert!(output.contains("Alice"));
    assert!(output.contains("30"));

}   // ← TestDir::drop() se ejecuta aquí
    //   Elimina TODO el directorio temporal
    //   Funciona incluso si el test falla con panic

#[test]
fn test_process_missing_file() {
    let dir = common::TestDir::new("missing_test");

    let processor = FileProcessor::new(dir.path());
    let result = processor.process("nonexistent.csv", "output.json");

    assert!(result.is_err());
    assert!(!dir.file_exists("output.json"));
}   // ← Cleanup automático
```

### Patrón 3: Builder para setup configurable

```rust
// tests/common/mod.rs

pub struct TestEnvBuilder {
    with_db: bool,
    with_files: Vec<(String, String)>,
    config_overrides: Vec<(String, String)>,
}

pub struct TestEnv {
    pub db: Option<Database>,
    pub dir: TestDir,
    pub config: Config,
}

impl TestEnvBuilder {
    pub fn new() -> Self {
        TestEnvBuilder {
            with_db: false,
            with_files: Vec::new(),
            config_overrides: Vec::new(),
        }
    }

    pub fn with_database(mut self) -> Self {
        self.with_db = true;
        self
    }

    pub fn with_file(mut self, name: &str, content: &str) -> Self {
        self.with_files.push((name.to_string(), content.to_string()));
        self
    }

    pub fn with_config(mut self, key: &str, value: &str) -> Self {
        self.config_overrides.push((key.to_string(), value.to_string()));
        self
    }

    pub fn build(self) -> TestEnv {
        let dir = TestDir::new(&format!("env_{}", std::process::id()));

        for (name, content) in &self.with_files {
            dir.create_file(name, content);
        }

        let mut config = Config::test_defaults();
        for (key, value) in &self.config_overrides {
            config.set(key, value);
        }

        let db = if self.with_db {
            Some(Database::new_in_memory("test").unwrap())
        } else {
            None
        };

        TestEnv { db, dir, config }
    }
}
```

```rust
// tests/complex_tests.rs
mod common;
use common::TestEnvBuilder;

#[test]
fn test_import_with_custom_config() {
    let env = TestEnvBuilder::new()
        .with_database()
        .with_file("data.csv", "a,b,c\n1,2,3")
        .with_config("delimiter", ",")
        .with_config("skip_header", "true")
        .build();

    let importer = mi_crate::Importer::new(
        env.db.as_ref().unwrap(),
        &env.config,
    );
    importer.import(env.dir.path().join("data.csv")).unwrap();

    // Verificar importación
    let rows = env.db.as_ref().unwrap().count_rows("data").unwrap();
    assert_eq!(rows, 1);  // 1 fila de datos (header omitido)
}
```

### Comparación con otros lenguajes

```
                Setup/Teardown

  JUnit (Java):
    @Before void setUp() { ... }
    @After  void tearDown() { ... }     ← Explícito, manual

  pytest (Python):
    @pytest.fixture
    def db():
        conn = connect()
        yield conn
        conn.close()                    ← Generator-based

  Google Test (C++):
    void SetUp() override { ... }
    void TearDown() override { ... }    ← Herencia de TestFixture

  Rust:
    let ctx = setup();
    // ... test ...
    // Drop::drop() se ejecuta automáticamente
    //
    // Ventajas:
    //  - No se puede olvidar el cleanup
    //  - Funciona con panic (unwind ejecuta Drop)
    //  - Composable (varios RAII en un test)
    //  - Sin framework ni macros especiales
```

---

## 12. Integration tests para binary crates

Esta es una restricción importante: **no puedes escribir integration tests para binary crates** (`src/main.rs` sin `src/lib.rs`).

### El problema

```
Proyecto solo-binario:

  mi_app/
  ├── Cargo.toml
  ├── src/
  │   └── main.rs        ← Solo binario, no hay lib.rs
  └── tests/
      └── test_app.rs    ← ¿Qué importo?
```

```rust
// tests/test_app.rs

use mi_app::???;  // ERROR: mi_app es un binario, no una librería
                   // No hay crate que importar
```

El error será:

```
error[E0432]: unresolved import `mi_app`
 --> tests/test_app.rs:1:5
  |
1 | use mi_app::run;
  |     ^^^^^^ use of undeclared crate or module `mi_app`
```

### Por qué no funciona

Los integration tests usan `use mi_crate::` para importar tu código. Esto requiere que tu proyecto produzca una **librería** (`.rlib`). Un proyecto que solo tiene `main.rs` produce un **binario** — no hay `.rlib` para enlazar.

```
Proyecto con lib.rs:
  cargo build → libmi_crate.rlib   ← Los integration tests enlazan contra esto
                mi_crate (binario)

Proyecto solo main.rs:
  cargo build → mi_crate (binario) ← NO hay .rlib
                                       Los integration tests no pueden enlazar
```

### La solución: patrón thin main

El patrón estándar en Rust es tener un `main.rs` minimalista que delega a `lib.rs`:

```
mi_app/
├── Cargo.toml
├── src/
│   ├── lib.rs           ← Toda la lógica aquí
│   └── main.rs          ← Solo el punto de entrada
└── tests/
    └── integration.rs   ← Importa desde lib.rs
```

```rust
// src/lib.rs — Toda la lógica va aquí

pub struct App {
    config: Config,
}

pub struct Config {
    pub verbose: bool,
    pub output_path: String,
}

impl Config {
    pub fn from_args(args: &[String]) -> Result<Config, String> {
        if args.len() < 2 {
            return Err("Usage: mi_app <output_path>".to_string());
        }
        Ok(Config {
            verbose: args.contains(&"--verbose".to_string()),
            output_path: args[1].clone(),
        })
    }
}

impl App {
    pub fn new(config: Config) -> Self {
        App { config }
    }

    pub fn run(&self) -> Result<(), Box<dyn std::error::Error>> {
        if self.config.verbose {
            eprintln!("Processing...");
        }
        // ... lógica de la aplicación
        std::fs::write(&self.config.output_path, "result")?;
        Ok(())
    }
}
```

```rust
// src/main.rs — Mínimo posible

use mi_app::{App, Config};
use std::process;

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let config = Config::from_args(&args).unwrap_or_else(|err| {
        eprintln!("Error: {}", err);
        process::exit(1);
    });

    let app = App::new(config);
    if let Err(e) = app.run() {
        eprintln!("Application error: {}", e);
        process::exit(1);
    }
}
```

```rust
// tests/integration.rs — Ahora puedes importar desde lib.rs

use mi_app::{App, Config};

#[test]
fn test_app_creates_output_file() {
    let dir = std::env::temp_dir().join("test_app_output");
    std::fs::create_dir_all(&dir).unwrap();
    let output = dir.join("result.txt");

    let config = Config {
        verbose: false,
        output_path: output.to_string_lossy().to_string(),
    };

    let app = App::new(config);
    app.run().unwrap();

    assert!(output.exists());
    let contents = std::fs::read_to_string(&output).unwrap();
    assert_eq!(contents, "result");

    // Cleanup
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn test_config_parsing() {
    let args = vec![
        "mi_app".to_string(),
        "/tmp/output".to_string(),
        "--verbose".to_string(),
    ];
    let config = Config::from_args(&args).unwrap();
    assert!(config.verbose);
    assert_eq!(config.output_path, "/tmp/output");
}

#[test]
fn test_config_missing_args() {
    let args = vec!["mi_app".to_string()];
    let result = Config::from_args(&args);
    assert!(result.is_err());
}
```

### Patrón para proyectos con lib + multiple bins

```toml
# Cargo.toml
[package]
name = "mi_toolkit"
version = "0.1.0"

[lib]
name = "mi_toolkit"
path = "src/lib.rs"

[[bin]]
name = "tool_a"
path = "src/bin/tool_a.rs"

[[bin]]
name = "tool_b"
path = "src/bin/tool_b.rs"
```

```
mi_toolkit/
├── Cargo.toml
├── src/
│   ├── lib.rs              ← Lógica compartida
│   ├── bin/
│   │   ├── tool_a.rs       ← Binario A (thin main)
│   │   └── tool_b.rs       ← Binario B (thin main)
│   ├── tool_a_logic.rs     ← Lógica específica de tool_a
│   └── tool_b_logic.rs     ← Lógica específica de tool_b
└── tests/
    ├── tool_a_tests.rs     ← Integration tests de tool_a
    ├── tool_b_tests.rs     ← Integration tests de tool_b
    └── shared_tests.rs     ← Tests de funcionalidad compartida
```

```rust
// src/bin/tool_a.rs — Thin main
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if let Err(e) = mi_toolkit::tool_a_logic::run(&args) {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
```

### Alternativa: test del binario como proceso

Cuando necesitas probar el binario **como proceso** (argumentos de CLI, exit codes, stderr/stdout), puedes usar `std::process::Command`:

```rust
// tests/cli_tests.rs
use std::process::Command;

#[test]
fn test_binary_with_no_args_exits_with_error() {
    let output = Command::new("cargo")
        .args(["run", "--", /* sin argumentos */])
        .output()
        .expect("Failed to execute");

    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains("Usage:"));
}

#[test]
fn test_binary_help_flag() {
    // Compilar primero, luego ejecutar el binario directamente
    let status = Command::new("cargo")
        .args(["build"])
        .status()
        .expect("Failed to build");
    assert!(status.success());

    let output = Command::new("target/debug/mi_app")
        .arg("--help")
        .output()
        .expect("Failed to execute binary");

    assert!(output.status.success());
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("Usage:"));
    assert!(stdout.contains("Options:"));
}

#[test]
fn test_binary_processes_file() {
    let temp = std::env::temp_dir().join("cli_test_output.txt");

    let output = Command::new("cargo")
        .args(["run", "--", &temp.to_string_lossy()])
        .output()
        .expect("Failed to execute");

    assert!(output.status.success());
    assert!(temp.exists());

    // Cleanup
    let _ = std::fs::remove_file(&temp);
}
```

---

## 13. Integration tests con dependencias externas

Los integration tests frecuentemente necesitan interactuar con recursos externos: archivos, directorios, variables de entorno, puertos de red. Aquí están los patrones para manejar cada caso:

### Archivos y directorios

```rust
// tests/file_tests.rs
use std::path::PathBuf;

/// Obtiene la ruta al directorio de fixtures del proyecto
fn fixtures_dir() -> PathBuf {
    // CARGO_MANIFEST_DIR apunta a la raíz del proyecto
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.push("tests");
    path.push("fixtures");
    path
}

#[test]
fn test_parse_sample_config() {
    let config_path = fixtures_dir().join("sample_config.toml");
    let config = mi_crate::Config::from_file(&config_path).unwrap();
    assert_eq!(config.name(), "test_project");
    assert_eq!(config.version(), "1.0.0");
}

#[test]
fn test_parse_large_csv() {
    let csv_path = fixtures_dir().join("large_dataset.csv");
    let records = mi_crate::parse_csv(&csv_path).unwrap();
    assert_eq!(records.len(), 1000);
    assert_eq!(records[0].name, "Alice");
}
```

```
Estructura con fixtures:

  mi_proyecto/
  ├── src/
  │   └── lib.rs
  └── tests/
      ├── fixtures/              ← Datos de prueba
      │   ├── sample_config.toml
      │   ├── large_dataset.csv
      │   ├── valid_input.json
      │   └── malformed.xml
      ├── common/
      │   └── mod.rs
      └── file_tests.rs
```

### Variables de entorno

```rust
// tests/env_tests.rs

use std::sync::Mutex;

// Las variables de entorno son estado global compartido.
// Los tests que las modifican deben ser cuidadosos con
// ejecución paralela.

// Opción 1: Usar un mutex para serializar tests de env
static ENV_LOCK: Mutex<()> = Mutex::new(());

#[test]
fn test_reads_api_key_from_env() {
    let _guard = ENV_LOCK.lock().unwrap();

    // Guardar valor original
    let original = std::env::var("API_KEY").ok();

    // Establecer para el test
    std::env::set_var("API_KEY", "test_key_12345");

    let config = mi_crate::Config::from_env().unwrap();
    assert_eq!(config.api_key(), "test_key_12345");

    // Restaurar
    match original {
        Some(val) => std::env::set_var("API_KEY", val),
        None => std::env::remove_var("API_KEY"),
    }
}

// Opción 2: Diseñar la API para no depender de env directamente
#[test]
fn test_config_with_explicit_values() {
    // Mejor diseño: pasar valores explícitamente
    let config = mi_crate::Config::builder()
        .api_key("test_key_12345")
        .timeout(30)
        .build()
        .unwrap();

    assert_eq!(config.api_key(), "test_key_12345");
    // No necesita tocar variables de entorno
}
```

### Puertos de red y servicios

```rust
// tests/server_tests.rs
use std::net::TcpListener;

/// Encuentra un puerto libre para testing
fn free_port() -> u16 {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    listener.local_addr().unwrap().port()
}

#[test]
fn test_server_responds_to_health_check() {
    let port = free_port();

    // Iniciar servidor en un thread
    let handle = std::thread::spawn(move || {
        mi_crate::Server::new(port).run_once()
    });

    // Dar tiempo al servidor para arrancar
    std::thread::sleep(std::time::Duration::from_millis(100));

    // Conectar como cliente
    let response = mi_crate::Client::new(&format!("http://127.0.0.1:{}", port))
        .get("/health")
        .unwrap();

    assert_eq!(response.status(), 200);
    assert_eq!(response.body(), "OK");

    handle.join().unwrap();
}
```

### Bases de datos temporales

```rust
// tests/common/mod.rs
use mi_crate::Database;

pub struct TestDatabase {
    db: Database,
    name: String,
}

impl TestDatabase {
    pub fn new() -> Self {
        let name = format!(
            "test_db_{}_{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        );

        let db = Database::new_in_memory(&name).unwrap();
        db.run_migrations().unwrap();

        TestDatabase { db, name }
    }

    pub fn db(&self) -> &Database {
        &self.db
    }

    pub fn seed(&self, data: &[(&str, &str, u32)]) {
        for (name, email, age) in data {
            self.db.insert_user(name, email, *age).unwrap();
        }
    }
}

impl Drop for TestDatabase {
    fn drop(&mut self) {
        // SQLite in-memory se limpia solo, pero si fuera on-disk:
        let path = format!("/tmp/{}.db", self.name);
        let _ = std::fs::remove_file(path);
    }
}
```

---

## 14. Organización por feature o funcionalidad

En proyectos grandes, la cantidad de archivos de integration test puede afectar significativamente el tiempo de compilación. Hay dos estrategias:

### Estrategia 1: Un archivo por feature (muchos crates)

```
tests/
├── auth.rs              ← Crate 1
├── users.rs             ← Crate 2
├── products.rs          ← Crate 3
├── orders.rs            ← Crate 4
├── payments.rs          ← Crate 5
├── notifications.rs     ← Crate 6
└── reports.rs           ← Crate 7

→ 7 compilaciones separadas + 1 librería + 1 unit tests = 9 compilaciones
→ Cada archivo es un binario independiente
→ Máximo aislamiento pero más lento
```

### Estrategia 2: Un archivo que importa módulos (un solo crate)

```
tests/
├── integration.rs       ← UN solo crate que importa submódulos
└── integration/
    ├── auth.rs
    ├── users.rs
    ├── products.rs
    ├── orders.rs
    ├── payments.rs
    ├── notifications.rs
    └── reports.rs

→ 1 compilación de integration tests + 1 librería + 1 unit tests = 3 compilaciones
→ Todos los tests comparten un binario
→ Más rápido pero menos aislado
```

```rust
// tests/integration.rs
// Este archivo declara submódulos — todos se compilan en UN binario

mod integration;  // ← busca tests/integration/mod.rs o tests/integration/*.rs
```

Pero **cuidado**: esto requiere un `mod.rs` en el subdirectorio:

```rust
// tests/integration/mod.rs
mod auth;
mod users;
mod products;
mod orders;
mod payments;
mod notifications;
mod reports;
```

```rust
// tests/integration/auth.rs
use mi_crate::Auth;

#[test]
fn test_login_success() {
    let auth = Auth::new();
    let token = auth.login("admin", "password").unwrap();
    assert!(!token.is_expired());
}

#[test]
fn test_login_bad_password() {
    let auth = Auth::new();
    let result = auth.login("admin", "wrong");
    assert!(result.is_err());
}
```

```rust
// tests/integration/users.rs
use mi_crate::UserService;

#[test]
fn test_create_user() {
    let service = UserService::new_for_testing();
    let user = service.create("Alice", "alice@example.com").unwrap();
    assert_eq!(user.name(), "Alice");
}
```

### Comparación de estrategias

```
Estrategia 1: Muchos archivos (1 crate por archivo)

  Ventajas:
    + Cada test file es completamente independiente
    + Fallos en uno no afectan a otros
    + Cargo puede ejecutar en paralelo (por archivo)
    + Fácil de ejecutar un solo archivo: cargo test --test auth

  Desventajas:
    - N compilaciones separadas (lento en proyectos grandes)
    - Cada crate re-enlaza contra tu librería
    - Más uso de disco en target/

  Cuándo usar:
    → Proyecto mediano (< 15 archivos de integration test)
    → Cuando la independencia importa más que la velocidad
    → CI con caché de compilación

  ─────────────────────────────────────────────────

Estrategia 2: Un archivo + submódulos (1 crate total)

  Ventajas:
    + Solo 1 compilación de integration tests
    + Significativamente más rápido en proyectos grandes
    + Menos uso de disco

  Desventajas:
    - Un panic en un test puede afectar otros (mismo proceso)
    - No puedes ejecutar un solo módulo fácilmente
    - Todos los módulos comparten namespace

  Cuándo usar:
    → Proyecto grande (> 20 archivos de integration test)
    → Cuando el tiempo de compilación es un bottleneck
    → Tests que no tienen efectos secundarios conflictivos
```

### Estrategia 3: Híbrida

```
tests/
├── smoke.rs               ← Crate separado: tests rápidos esenciales
├── integration.rs          ← Crate unificado: tests funcionales
├── integration/
│   ├── mod.rs
│   ├── auth.rs
│   ├── users.rs
│   └── products.rs
├── performance.rs          ← Crate separado: tests lentos
└── common/
    └── mod.rs              ← Helpers compartidos
```

```rust
// tests/smoke.rs — Tests rápidos que siempre ejecutas
use mi_crate::App;

#[test]
fn test_app_starts() {
    let app = App::new_test();
    assert!(app.is_healthy());
}

#[test]
fn test_basic_crud() {
    let app = App::new_test();
    let id = app.create("test").unwrap();
    let item = app.read(id).unwrap();
    assert_eq!(item.name(), "test");
}
```

```rust
// tests/performance.rs — Tests lentos, ejecutados selectivamente
use mi_crate::Processor;
use std::time::Instant;

#[test]
#[ignore]  // Solo se ejecuta con cargo test -- --ignored
fn test_processes_10k_records_under_1_second() {
    let processor = Processor::new();
    let data = (0..10_000).map(|i| format!("record_{}", i)).collect::<Vec<_>>();

    let start = Instant::now();
    processor.process_batch(&data).unwrap();
    let elapsed = start.elapsed();

    assert!(
        elapsed.as_secs_f64() < 1.0,
        "Processing took {:.2}s, expected < 1s",
        elapsed.as_secs_f64()
    );
}
```

---

## 15. Integration tests vs unit tests: criterios de elección

### Tabla de decisión

| Criterio | Unit test | Integration test |
|----------|-----------|-----------------|
| **Qué prueba** | Una función/módulo aislado | Flujo completo o API pública |
| **Visibilidad** | Todo (incluyendo privado) | Solo `pub` |
| **Velocidad** | Muy rápido | Más lento (compilación extra) |
| **Ubicación** | `src/`, dentro de `mod tests` | `tests/` directorio |
| **Necesita `#[cfg(test)]`** | Sí | No |
| **Necesita `mod tests`** | Sí | No |
| **Necesita `use super::*`** | Sí | No (usa `use mi_crate::`) |
| **Perspectiva** | Desarrollador del módulo | Usuario de la biblioteca |
| **Fallo indica** | Bug en implementación interna | Bug en API o en integración |
| **Refactoring** | Se rompen frecuentemente | Solo se rompen si la API cambia |
| **Cantidad típica** | Muchos (5-50 por módulo) | Pocos (1-10 por feature) |

### Pirámide de testing en Rust

```
                    ╱╲
                   ╱  ╲
                  ╱ E2E╲          Pocos, lentos, frágiles
                 ╱ tests ╲        (binario completo, CLI, red)
                ╱──────────╲
               ╱            ╲
              ╱ Integration   ╲    Moderados, velocidad media
             ╱  tests          ╲   (API pública, flujos, tests/)
            ╱────────────────────╲
           ╱                      ╲
          ╱     Unit tests          ╲  Muchos, rápidos, estables
         ╱      (mod tests {})       ╲ (funciones individuales, src/)
        ╱──────────────────────────────╲

  Regla general:
    70% unit tests   → Prueban lógica interna, rápidos de ejecutar
    20% integration  → Prueban API pública, flujos entre módulos
    10% E2E          → Prueban el sistema completo
```

### Cuándo usar cada uno — Ejemplos concretos

```rust
// ═══════════════════════════════════════════
// CASO 1: Algoritmo interno → UNIT TEST
// ═══════════════════════════════════════════

// src/sort.rs
fn merge(left: &[i32], right: &[i32]) -> Vec<i32> {
    // Función interna del merge sort
    let mut result = Vec::with_capacity(left.len() + right.len());
    let (mut i, mut j) = (0, 0);
    while i < left.len() && j < right.len() {
        if left[i] <= right[j] {
            result.push(left[i]);
            i += 1;
        } else {
            result.push(right[j]);
            j += 1;
        }
    }
    result.extend_from_slice(&left[i..]);
    result.extend_from_slice(&right[j..]);
    result
}

pub fn merge_sort(data: &mut [i32]) {
    // ... usa merge() internamente
}

#[cfg(test)]
mod tests {
    use super::*;

    // ✓ Unit test: prueba la función interna merge()
    #[test]
    fn test_merge_two_sorted_arrays() {
        assert_eq!(merge(&[1, 3, 5], &[2, 4, 6]), vec![1, 2, 3, 4, 5, 6]);
    }

    #[test]
    fn test_merge_empty_left() {
        assert_eq!(merge(&[], &[1, 2, 3]), vec![1, 2, 3]);
    }
}
```

```rust
// ═══════════════════════════════════════════════
// CASO 2: API pública correcta → INTEGRATION TEST
// ═══════════════════════════════════════════════

// tests/sorting_api.rs
use mi_crate::merge_sort;

#[test]
fn test_sort_random_array() {
    let mut data = vec![5, 3, 8, 1, 9, 2, 7, 4, 6];
    merge_sort(&mut data);
    assert_eq!(data, vec![1, 2, 3, 4, 5, 6, 7, 8, 9]);
}

#[test]
fn test_sort_already_sorted() {
    let mut data = vec![1, 2, 3, 4, 5];
    merge_sort(&mut data);
    assert_eq!(data, vec![1, 2, 3, 4, 5]);
}

#[test]
fn test_sort_reverse_sorted() {
    let mut data = vec![5, 4, 3, 2, 1];
    merge_sort(&mut data);
    assert_eq!(data, vec![1, 2, 3, 4, 5]);
}

#[test]
fn test_sort_empty() {
    let mut data: Vec<i32> = vec![];
    merge_sort(&mut data);
    assert!(data.is_empty());
}

#[test]
fn test_sort_single_element() {
    let mut data = vec![42];
    merge_sort(&mut data);
    assert_eq!(data, vec![42]);
}

#[test]
fn test_sort_duplicates() {
    let mut data = vec![3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5];
    merge_sort(&mut data);
    assert_eq!(data, vec![1, 1, 2, 3, 3, 4, 5, 5, 5, 6, 9]);
}
```

```rust
// ═══════════════════════════════════════════════
// CASO 3: Flujo entre módulos → INTEGRATION TEST
// ═══════════════════════════════════════════════

// tests/pipeline.rs
use mi_crate::{Parser, Validator, Transformer, Output};

#[test]
fn test_complete_pipeline() {
    let input = r#"{"name": "Alice", "age": 30}"#;

    // Paso 1: Parse
    let parsed = Parser::new().parse(input).unwrap();

    // Paso 2: Validate
    let validated = Validator::strict().validate(&parsed).unwrap();

    // Paso 3: Transform
    let transformed = Transformer::new()
        .add_timestamp()
        .normalize_names()
        .transform(&validated)
        .unwrap();

    // Paso 4: Output
    let output = Output::to_string(&transformed).unwrap();

    // Verificar resultado final
    assert!(output.contains("ALICE"));  // normalize_names() uppercased
    assert!(output.contains("timestamp"));  // add_timestamp() added field
}
```

---

## 16. Filtrado y ejecución selectiva

Cargo ofrece múltiples formas de ejecutar subconjuntos de tests:

### Ejecutar solo integration tests

```bash
# Solo integration tests (todos los archivos en tests/)
$ cargo test --test '*'

# Solo un archivo específico de integration tests
$ cargo test --test stack_tests

# Solo integration tests que contienen "push" en el nombre
$ cargo test --test stack_tests push

# Múltiples archivos
$ cargo test --test stack_tests --test queue_tests
```

### Excluir integration tests

```bash
# Solo unit tests (de src/)
$ cargo test --lib

# Solo doc tests
$ cargo test --doc

# Unit tests + doc tests (sin integration)
$ cargo test --lib --doc
```

### Filtrar por nombre de función

```bash
# Tests cuyo nombre contiene "creation"
$ cargo test creation

# Tests cuyo nombre contiene "error"
$ cargo test error

# Tests en el módulo "auth" (de integration tests)
$ cargo test --test integration auth
```

### Tabla de flags de filtrado

| Flag | Efecto |
|------|--------|
| `--lib` | Solo unit tests de `src/lib.rs` |
| `--test NAME` | Solo el archivo `tests/NAME.rs` |
| `--test '*'` | Todos los integration tests |
| `--doc` | Solo doc tests |
| `--bin NAME` | Solo tests del binario NAME |
| `--bins` | Tests de todos los binarios |
| `FILTER` | Tests cuyo nombre contiene FILTER |
| `-- --ignored` | Solo tests marcados `#[ignore]` |
| `-- --include-ignored` | Tests normales + ignored |
| `-- --exact` | Match exacto del nombre (no substring) |

### Ejecución durante desarrollo

```bash
# Workflow típico de desarrollo:

# 1. Mientras escribes código, ejecuta solo unit tests (rápido)
$ cargo test --lib

# 2. Cuando un módulo está listo, ejecuta sus integration tests
$ cargo test --test auth_tests

# 3. Antes de commit, ejecuta todo
$ cargo test

# 4. En CI, ejecuta todo incluyendo tests lentos
$ cargo test -- --include-ignored
```

### Ejemplo de output filtrado

```
$ cargo test --test stack_tests push

   Compiling mi_colecciones v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
     Running tests/stack_tests.rs (target/debug/deps/stack_tests-abc123)

running 2 tests
test test_push_and_pop ... ok
test test_push_multiple ... ok
test result: ok. 2 passed; 0 failed; 0 ignored; 3 filtered out

```

El `3 filtered out` indica que hay 3 tests más en el archivo que no coinciden con el filtro "push".

---

## 17. Patrones avanzados de integration testing

### Patrón 1: Test de migración de datos

```rust
// tests/migration.rs
use mi_crate::{Database, Migration};

#[test]
fn test_v1_to_v2_migration() {
    // Crear base de datos con esquema v1
    let db = Database::new_in_memory("migration_test").unwrap();
    db.execute("CREATE TABLE users (name TEXT, age INTEGER)").unwrap();
    db.execute("INSERT INTO users VALUES ('Alice', 30)").unwrap();

    // Aplicar migración v2
    let migration = Migration::v1_to_v2();
    migration.apply(&db).unwrap();

    // Verificar que los datos se preservaron en el nuevo esquema
    let users = db.query("SELECT name, age, email FROM users").unwrap();
    assert_eq!(users.len(), 1);
    assert_eq!(users[0].get::<String>("name"), "Alice");
    assert_eq!(users[0].get::<i32>("age"), 30);
    assert_eq!(users[0].get::<Option<String>>("email"), None);  // Nueva columna, default NULL
}

#[test]
fn test_migration_is_idempotent() {
    let db = Database::new_in_memory("idempotent_test").unwrap();
    db.execute("CREATE TABLE users (name TEXT, age INTEGER)").unwrap();

    let migration = Migration::v1_to_v2();

    // Aplicar dos veces no debe fallar
    migration.apply(&db).unwrap();
    migration.apply(&db).unwrap();  // Segunda vez: debe ser no-op
}
```

### Patrón 2: Test de round-trip (serialización)

```rust
// tests/serialization.rs
use mi_crate::{Config, Format};

#[test]
fn test_json_round_trip() {
    let original = Config::builder()
        .name("test")
        .timeout(30)
        .retries(3)
        .tags(vec!["a".into(), "b".into()])
        .build();

    // Serializar → deserializar debe producir el mismo valor
    let json = original.to_format(Format::Json).unwrap();
    let restored = Config::from_str(&json, Format::Json).unwrap();

    assert_eq!(original, restored);
}

#[test]
fn test_toml_round_trip() {
    let original = Config::builder()
        .name("production")
        .timeout(60)
        .retries(5)
        .build();

    let toml = original.to_format(Format::Toml).unwrap();
    let restored = Config::from_str(&toml, Format::Toml).unwrap();

    assert_eq!(original, restored);
}

#[test]
fn test_cross_format_compatibility() {
    let original = Config::builder()
        .name("cross")
        .timeout(10)
        .build();

    // JSON → TOML → JSON debe preservar datos
    let json1 = original.to_format(Format::Json).unwrap();
    let from_json = Config::from_str(&json1, Format::Json).unwrap();
    let toml = from_json.to_format(Format::Toml).unwrap();
    let from_toml = Config::from_str(&toml, Format::Toml).unwrap();
    let json2 = from_toml.to_format(Format::Json).unwrap();

    assert_eq!(json1, json2);
}
```

### Patrón 3: Test de backwards compatibility

```rust
// tests/compatibility.rs
use mi_crate::Config;

// Fixtures con formatos de versiones anteriores
const V1_CONFIG: &str = r#"
{
    "name": "legacy",
    "timeout_ms": 5000
}
"#;

const V2_CONFIG: &str = r#"
{
    "name": "current",
    "timeout": { "secs": 5, "nanos": 0 }
}
"#;

#[test]
fn test_reads_v1_format() {
    // La versión actual debe poder leer el formato v1
    let config = Config::from_json(V1_CONFIG).unwrap();
    assert_eq!(config.name(), "legacy");
    assert_eq!(config.timeout_secs(), 5);
}

#[test]
fn test_reads_v2_format() {
    let config = Config::from_json(V2_CONFIG).unwrap();
    assert_eq!(config.name(), "current");
    assert_eq!(config.timeout_secs(), 5);
}

#[test]
fn test_writes_current_format() {
    // Nuevos configs siempre se escriben en formato v2
    let config = Config::new("test", 10);
    let json = config.to_json().unwrap();
    assert!(json.contains(r#""timeout""#));
    assert!(!json.contains("timeout_ms"));
}
```

### Patrón 4: Test de error handling end-to-end

```rust
// tests/error_handling.rs
use mi_crate::{Processor, ProcessError};

#[test]
fn test_error_chain_preserved() {
    let processor = Processor::new();

    // Input que causa error en una capa profunda
    let result = processor.process("invalid://data");

    let err = result.unwrap_err();

    // Verificar que la cadena de errores es informativa
    let error_string = format!("{}", err);
    assert!(error_string.contains("process failed"));

    // Verificar la causa raíz
    let source = err.source().unwrap();
    assert!(format!("{}", source).contains("invalid protocol"));
}

#[test]
fn test_partial_failure_rollback() {
    let processor = Processor::new();

    // Batch con un elemento inválido en el medio
    let items = vec!["good1", "good2", "BAD", "good3"];
    let result = processor.process_batch(&items);

    // Debe fallar indicando cuál falló
    let err = result.unwrap_err();
    match err {
        ProcessError::BatchPartialFailure { succeeded, failed_index, .. } => {
            assert_eq!(succeeded, 2);       // 2 procesados antes del fallo
            assert_eq!(failed_index, 2);    // Índice del elemento malo
        }
        other => panic!("Expected BatchPartialFailure, got {:?}", other),
    }
}
```

### Patrón 5: Test de concurrencia

```rust
// tests/concurrency.rs
use mi_crate::SharedCounter;
use std::sync::Arc;
use std::thread;

#[test]
fn test_concurrent_increments() {
    let counter = Arc::new(SharedCounter::new());
    let mut handles = Vec::new();

    // 10 threads, cada uno incrementa 1000 veces
    for _ in 0..10 {
        let counter = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..1000 {
                counter.increment();
            }
        }));
    }

    for handle in handles {
        handle.join().unwrap();
    }

    // Si la sincronización funciona, el resultado es exactamente 10_000
    assert_eq!(counter.value(), 10_000);
}

#[test]
fn test_concurrent_read_write() {
    let map = Arc::new(mi_crate::ConcurrentMap::new());
    let mut handles = Vec::new();

    // Writers
    for i in 0..5 {
        let map = Arc::clone(&map);
        handles.push(thread::spawn(move || {
            for j in 0..100 {
                map.insert(format!("key_{}_{}", i, j), i * 100 + j);
            }
        }));
    }

    // Readers (ejecutándose concurrentemente con writers)
    for _ in 0..5 {
        let map = Arc::clone(&map);
        handles.push(thread::spawn(move || {
            for _ in 0..100 {
                // Las lecturas no deben panic ni retornar datos corruptos
                let _ = map.get("key_0_0");
                let _ = map.len();
            }
        }));
    }

    for handle in handles {
        handle.join().unwrap();
    }

    assert_eq!(map.len(), 500);  // 5 writers × 100 keys
}
```

---

## 18. Comparación con C y Go

### Tabla comparativa

| Aspecto | C | Go | Rust |
|---------|---|-----|------|
| **Ubicación** | `tests/` separado + framework | `*_test.go` junto al código | `tests/` directorio separado |
| **Dependencia** | Framework externo (Unity, CMocka) | Paquete `testing` built-in | Built-in, sin framework |
| **Visibilidad** | Ve todo (no hay restricción) | Ve todo (mismo paquete) o solo exportado (`_test` package) | Solo `pub` |
| **Compilación** | Makefile separado | `go test` automático | `cargo test` automático |
| **Cada archivo** | Necesita main() o runner | Parte del paquete | Crate independiente |
| **Helpers** | Headers compartidos | `testutil_test.go` | `tests/common/mod.rs` |
| **Descubrimiento** | Manual (registrar tests) | Automático (`Test*`) | Automático (`#[test]`) |
| **Parallel** | Manual (fork/threads) | `t.Parallel()` | Automático (por defecto) |

### Equivalencias de patrones

```c
/* ═══ C: Integration test con Unity ═══ */

// tests/test_integration.c
#include "unity.h"
#include "../include/mylib.h"  // Incluye TODA la interfaz

void setUp(void) {
    // Inicialización antes de cada test
}

void tearDown(void) {
    // Limpieza después de cada test
}

void test_full_pipeline(void) {
    MyLib_Context *ctx = mylib_create();
    mylib_configure(ctx, "test_mode");

    int result = mylib_process(ctx, "input data");
    TEST_ASSERT_EQUAL(0, result);

    char *output = mylib_get_output(ctx);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_EQUAL_STRING("expected", output);

    mylib_destroy(ctx);  // Cleanup manual obligatorio
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_pipeline);
    return UNITY_END();
}
```

```go
// ═══ Go: Integration test ═══

// integration_test.go (en un paquete _test separado para solo ver exportados)
package mylib_test  // ← _test = solo ve lo exportado

import (
    "testing"
    "myproject/mylib"  // Importa como usuario externo
)

func TestFullPipeline(t *testing.T) {
    ctx := mylib.Create()
    defer ctx.Destroy()  // defer = cleanup automático

    ctx.Configure("test_mode")

    result, err := ctx.Process("input data")
    if err != nil {
        t.Fatalf("Process failed: %v", err)
    }

    if result.Output != "expected" {
        t.Errorf("got %q, want %q", result.Output, "expected")
    }
}

// Helpers en el mismo paquete de test
func setupTestDB(t *testing.T) *TestDB {
    t.Helper()
    db := NewTestDB()
    t.Cleanup(func() { db.Close() })
    return db
}
```

```rust
// ═══ Rust: Integration test ═══

// tests/full_pipeline.rs
use mi_crate::{Context, Config};

// Sin setUp/tearDown — RAII se encarga

#[test]
fn test_full_pipeline() {
    let ctx = Context::new();  // No necesita destroy — Drop lo hace
    let config = Config::new("test_mode");

    let result = ctx.process("input data", &config).unwrap();
    assert_eq!(result.output(), "expected");
}   // ← ctx se dropea aquí automáticamente

// Helper con RAII
fn setup_test_db() -> TestDb {
    let db = TestDb::new_in_memory();
    db.run_migrations().unwrap();
    db  // Caller es dueño; Drop limpiará cuando se salga de scope
}
```

### Diferencias filosóficas clave

```
C:
  - Sin restricción de visibilidad → los tests pueden ver todo
  - Esto significa que es RESPONSABILIDAD del programador no probar internals
  - El test puede accidentalmente depender de detalles de implementación
  - Si cambias un struct interno, tests de integración se rompen

Go:
  - DOS modos de visibilidad:
    package mylib      → ve todo (equivalente a unit test)
    package mylib_test → solo ve exportados (equivalente a integration test)
  - Ambos modos pueden estar en el mismo directorio
  - Decisión explícita del programador qué modo usar

Rust:
  - Separación FORZADA por el sistema de crates:
    src/mod tests  → ve todo (unit test)
    tests/*.rs     → solo ve pub (integration test)
  - No puedes "hacer trampa" — el compilador lo impide
  - La barrera es arquitectural, no convencional
```

---

## 19. Errores comunes

### Error 1: Poner `mod tests` dentro de un integration test

```rust
// tests/my_test.rs

// ✗ ERROR CONCEPTUAL: mod tests es para UNIT tests
use mi_crate::Widget;

#[cfg(test)]           // Innecesario — tests/ siempre es test
mod tests {            // Innecesario — el archivo ya es un crate de test
    use super::*;      // Solo importa Widget del scope del archivo

    #[test]
    fn test_widget() {
        let w = Widget::new();
        assert!(w.is_valid());
    }
}
```

```rust
// tests/my_test.rs

// ✓ CORRECTO: directamente
use mi_crate::Widget;

#[test]
fn test_widget() {
    let w = Widget::new();
    assert!(w.is_valid());
}
```

El wrapper `mod tests` no causa un error de compilación, pero es innecesario y confuso. Si lo usas, los nombres de tus tests serán `tests::test_widget` en lugar de `test_widget`.

### Error 2: Usar `tests/common.rs` en vez de `tests/common/mod.rs`

```
✗ INCORRECTO:
  tests/
  ├── common.rs          ← Cargo lo compila como test
  └── my_test.rs

  Output:
  Running tests/common.rs
  running 0 tests                ← Sección vacía inútil
  test result: ok. 0 passed

✓ CORRECTO:
  tests/
  ├── common/
  │   └── mod.rs          ← Cargo lo ignora (subdirectorio)
  └── my_test.rs
```

### Error 3: Intentar importar items `pub(crate)` desde integration tests

```rust
// src/lib.rs
pub(crate) fn internal_helper() -> i32 { 42 }
```

```rust
// tests/test.rs
use mi_crate::internal_helper;  // ✗ ERROR: not accessible

// Error del compilador:
// error[E0603]: function `internal_helper` is private
//  --> tests/test.rs:1:16
//   |
// 1 | use mi_crate::internal_helper;
//   |                ^^^^^^^^^^^^^^^ private function
```

`pub(crate)` solo es visible dentro de la crate que lo declara. Los integration tests son crates separados.

### Error 4: Olvidar que cada archivo de test es independiente

```rust
// tests/setup.rs
pub fn init() -> TestDb {     // ← Esto define init en el crate 'setup'
    TestDb::new()
}

// tests/my_test.rs
use setup::init;  // ✗ ERROR: 'setup' no es una dependencia

#[test]
fn test_something() {
    let db = init();
    // ...
}
```

Los archivos de test NO pueden importarse entre sí. Usa `tests/common/mod.rs` para código compartido:

```rust
// tests/common/mod.rs
pub fn init() -> TestDb { TestDb::new() }

// tests/my_test.rs
mod common;

#[test]
fn test_something() {
    let db = common::init();
}
```

### Error 5: Integration tests para proyecto sin lib.rs

```
✗ INCORRECTO:
  mi_app/
  ├── Cargo.toml
  ├── src/
  │   └── main.rs        ← Solo binario
  └── tests/
      └── test.rs        ← No puede importar nada

✓ CORRECTO:
  mi_app/
  ├── Cargo.toml
  ├── src/
  │   ├── lib.rs         ← Lógica aquí
  │   └── main.rs        ← Solo punto de entrada (thin main)
  └── tests/
      └── test.rs        ← Importa desde lib.rs
```

### Error 6: Asumir que los tests comparten estado

```rust
// tests/shared_state.rs

// ✗ PELIGROSO: estado estático mutable
static mut COUNTER: i32 = 0;

#[test]
fn test_increment() {
    unsafe { COUNTER += 1; }       // Race condition con otros tests
    unsafe { assert_eq!(COUNTER, 1); }  // ¡Puede fallar!
}

#[test]
fn test_increment_again() {
    unsafe { COUNTER += 1; }
    unsafe { assert_eq!(COUNTER, 1); }  // ¡Depende del orden!
}
```

Los tests se ejecutan en paralelo por defecto. Cada test debe ser independiente:

```rust
// tests/independent.rs

#[test]
fn test_increment() {
    let mut counter = 0;       // Cada test tiene su propia variable
    counter += 1;
    assert_eq!(counter, 1);    // ✓ Siempre funciona
}

#[test]
fn test_increment_again() {
    let mut counter = 0;       // Independiente del otro test
    counter += 1;
    assert_eq!(counter, 1);    // ✓ Siempre funciona
}
```

### Error 7: No limpiar recursos en tests que crean archivos

```rust
// tests/file_test.rs

#[test]
fn test_write_file() {
    std::fs::write("/tmp/test_output.txt", "data").unwrap();
    let content = std::fs::read_to_string("/tmp/test_output.txt").unwrap();
    assert_eq!(content, "data");
    // ✗ Archivo queda en /tmp/ después del test
    // Si el test falla antes del cleanup, el archivo persiste
}

// ✓ CORRECTO: usar RAII para cleanup
#[test]
fn test_write_file_safe() {
    struct TempFile(std::path::PathBuf);
    impl Drop for TempFile {
        fn drop(&mut self) {
            let _ = std::fs::remove_file(&self.0);
        }
    }

    let temp = TempFile(std::env::temp_dir().join("test_output_safe.txt"));
    std::fs::write(&temp.0, "data").unwrap();
    let content = std::fs::read_to_string(&temp.0).unwrap();
    assert_eq!(content, "data");
}   // ← TempFile::drop() elimina el archivo, incluso si assert falla
```

### Error 8: Confundir `--test` con filtro de nombre

```bash
# ✗ Estos NO son lo mismo:

$ cargo test auth           # Ejecuta TODOS los tests cuyo nombre contiene "auth"
                            # (unit + integration + doc)

$ cargo test --test auth    # Ejecuta solo el archivo tests/auth.rs
                            # (solo ese integration test)

# Combinación:
$ cargo test --test auth login  # Ejecuta tests en tests/auth.rs
                                 # cuyo nombre contiene "login"
```

---

## 20. Ejemplo completo: biblioteca de validación de datos

Vamos a construir un ejemplo completo que demuestre todos los conceptos de integration testing: estructura de proyecto, API pública, múltiples archivos de test, helpers compartidos, y tests que verifican flujos completos.

### Estructura del proyecto

```
data_validator/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── rules.rs
│   ├── validator.rs
│   └── report.rs
└── tests/
    ├── common/
    │   └── mod.rs
    ├── rule_tests.rs
    ├── validation_flow.rs
    ├── report_tests.rs
    └── error_tests.rs
```

### Código fuente (src/)

```rust
// src/lib.rs

pub mod rules;
pub mod validator;
pub mod report;

// Re-exportar tipos principales para conveniencia
pub use rules::{Rule, RuleSet};
pub use validator::{Validator, ValidationResult, FieldError};
pub use report::{Report, ReportFormat};
```

```rust
// src/rules.rs

use std::fmt;

/// Una regla de validación individual
#[derive(Debug, Clone)]
pub struct Rule {
    field: String,
    kind: RuleKind,
    message: String,
}

#[derive(Debug, Clone)]
enum RuleKind {
    Required,
    MinLength(usize),
    MaxLength(usize),
    Pattern(String),  // Regex simplificado: solo soporta prefijos/sufijos
    Range(i64, i64),
    Custom(String),   // Nombre de la validación custom
}

impl Rule {
    /// Campo requerido (no vacío)
    pub fn required(field: &str) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::Required,
            message: format!("{} is required", field),
        }
    }

    /// Longitud mínima
    pub fn min_length(field: &str, min: usize) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::MinLength(min),
            message: format!("{} must be at least {} characters", field, min),
        }
    }

    /// Longitud máxima
    pub fn max_length(field: &str, max: usize) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::MaxLength(max),
            message: format!("{} must be at most {} characters", field, max),
        }
    }

    /// Debe contener @ (simplificación para emails)
    pub fn email_format(field: &str) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::Pattern("@".to_string()),
            message: format!("{} must be a valid email", field),
        }
    }

    /// Valor numérico en rango
    pub fn range(field: &str, min: i64, max: i64) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::Range(min, max),
            message: format!("{} must be between {} and {}", field, min, max),
        }
    }

    /// Validación custom por nombre
    pub fn custom(field: &str, name: &str, message: &str) -> Self {
        Rule {
            field: field.to_string(),
            kind: RuleKind::Custom(name.to_string()),
            message: message.to_string(),
        }
    }

    pub fn field(&self) -> &str {
        &self.field
    }

    pub fn message(&self) -> &str {
        &self.message
    }

    /// Valida un valor contra esta regla
    pub(crate) fn validate(&self, value: Option<&str>) -> bool {
        match &self.kind {
            RuleKind::Required => {
                value.map_or(false, |v| !v.trim().is_empty())
            }
            RuleKind::MinLength(min) => {
                value.map_or(true, |v| v.len() >= *min)
            }
            RuleKind::MaxLength(max) => {
                value.map_or(true, |v| v.len() <= *max)
            }
            RuleKind::Pattern(pat) => {
                value.map_or(true, |v| v.contains(pat))
            }
            RuleKind::Range(min, max) => {
                value.map_or(true, |v| {
                    v.parse::<i64>()
                        .map_or(false, |n| n >= *min && n <= *max)
                })
            }
            RuleKind::Custom(_) => true, // Custom rules handled externally
        }
    }
}

impl fmt::Display for Rule {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Rule({}: {})", self.field, self.message)
    }
}

/// Conjunto de reglas para validar un tipo de dato
#[derive(Debug, Clone)]
pub struct RuleSet {
    name: String,
    rules: Vec<Rule>,
}

impl RuleSet {
    pub fn new(name: &str) -> Self {
        RuleSet {
            name: name.to_string(),
            rules: Vec::new(),
        }
    }

    pub fn add(mut self, rule: Rule) -> Self {
        self.rules.push(rule);
        self
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn rules(&self) -> &[Rule] {
        &self.rules
    }

    pub fn rule_count(&self) -> usize {
        self.rules.len()
    }
}
```

```rust
// src/validator.rs

use crate::rules::RuleSet;
use std::collections::HashMap;
use std::fmt;

/// Resultado de validar un conjunto de datos
#[derive(Debug)]
pub struct ValidationResult {
    valid: bool,
    errors: Vec<FieldError>,
    fields_checked: usize,
}

impl ValidationResult {
    pub fn is_valid(&self) -> bool {
        self.valid
    }

    pub fn errors(&self) -> &[FieldError] {
        &self.errors
    }

    pub fn error_count(&self) -> usize {
        self.errors.len()
    }

    pub fn fields_checked(&self) -> usize {
        self.fields_checked
    }

    pub fn errors_for_field(&self, field: &str) -> Vec<&FieldError> {
        self.errors.iter().filter(|e| e.field == field).collect()
    }

    pub fn has_error_for(&self, field: &str) -> bool {
        self.errors.iter().any(|e| e.field == field)
    }
}

/// Error de validación para un campo específico
#[derive(Debug, Clone)]
pub struct FieldError {
    field: String,
    message: String,
    value: Option<String>,
}

impl FieldError {
    pub fn field(&self) -> &str {
        &self.field
    }

    pub fn message(&self) -> &str {
        &self.message
    }

    pub fn value(&self) -> Option<&str> {
        self.value.as_deref()
    }
}

impl fmt::Display for FieldError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.value {
            Some(v) => write!(f, "{}: {} (got: '{}')", self.field, self.message, v),
            None => write!(f, "{}: {} (no value)", self.field, self.message),
        }
    }
}

/// Validador que aplica reglas a datos
pub struct Validator {
    rule_set: RuleSet,
    strict: bool,
}

impl Validator {
    pub fn new(rule_set: RuleSet) -> Self {
        Validator {
            rule_set,
            strict: false,
        }
    }

    /// En modo estricto, falla al encontrar el primer error
    pub fn strict(mut self) -> Self {
        self.strict = true;
        self
    }

    /// Valida un mapa de campo → valor
    pub fn validate(&self, data: &HashMap<String, String>) -> ValidationResult {
        let mut errors = Vec::new();
        let mut fields_checked = 0;

        for rule in self.rule_set.rules() {
            fields_checked += 1;
            let value = data.get(rule.field()).map(|s| s.as_str());

            if !rule.validate(value) {
                let error = FieldError {
                    field: rule.field().to_string(),
                    message: rule.message().to_string(),
                    value: value.map(|s| s.to_string()),
                };

                if self.strict {
                    return ValidationResult {
                        valid: false,
                        errors: vec![error],
                        fields_checked,
                    };
                }

                errors.push(error);
            }
        }

        ValidationResult {
            valid: errors.is_empty(),
            errors,
            fields_checked,
        }
    }

    /// Valida una lista de registros
    pub fn validate_batch(
        &self,
        records: &[HashMap<String, String>],
    ) -> Vec<(usize, ValidationResult)> {
        records
            .iter()
            .enumerate()
            .map(|(i, record)| (i, self.validate(record)))
            .filter(|(_, result)| !result.is_valid())
            .collect()
    }
}
```

```rust
// src/report.rs

use crate::validator::ValidationResult;
use std::fmt;

/// Formato de reporte
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ReportFormat {
    Text,
    Csv,
    Json,
}

/// Reporte de validación
pub struct Report {
    title: String,
    results: Vec<(String, ValidationResult)>,
}

impl Report {
    pub fn new(title: &str) -> Self {
        Report {
            title: title.to_string(),
            results: Vec::new(),
        }
    }

    pub fn add_result(&mut self, label: &str, result: ValidationResult) {
        self.results.push((label.to_string(), result));
    }

    pub fn total_errors(&self) -> usize {
        self.results.iter().map(|(_, r)| r.error_count()).sum()
    }

    pub fn total_records(&self) -> usize {
        self.results.len()
    }

    pub fn all_valid(&self) -> bool {
        self.results.iter().all(|(_, r)| r.is_valid())
    }

    pub fn render(&self, format: ReportFormat) -> String {
        match format {
            ReportFormat::Text => self.render_text(),
            ReportFormat::Csv => self.render_csv(),
            ReportFormat::Json => self.render_json(),
        }
    }

    fn render_text(&self) -> String {
        let mut output = format!("=== {} ===\n", self.title);
        output.push_str(&format!(
            "Records: {} | Errors: {}\n\n",
            self.total_records(),
            self.total_errors()
        ));

        for (label, result) in &self.results {
            if result.is_valid() {
                output.push_str(&format!("[OK] {}\n", label));
            } else {
                output.push_str(&format!("[FAIL] {}\n", label));
                for error in result.errors() {
                    output.push_str(&format!("  - {}\n", error));
                }
            }
        }

        output
    }

    fn render_csv(&self) -> String {
        let mut output = String::from("record,field,message,value\n");

        for (label, result) in &self.results {
            for error in result.errors() {
                output.push_str(&format!(
                    "{},{},{},{}\n",
                    label,
                    error.field(),
                    error.message(),
                    error.value().unwrap_or("")
                ));
            }
        }

        output
    }

    fn render_json(&self) -> String {
        let mut output = String::from("{\n");
        output.push_str(&format!("  \"title\": \"{}\",\n", self.title));
        output.push_str(&format!(
            "  \"total_errors\": {},\n",
            self.total_errors()
        ));
        output.push_str("  \"results\": [\n");

        let mut first = true;
        for (label, result) in &self.results {
            if !first {
                output.push_str(",\n");
            }
            first = false;

            output.push_str("    {\n");
            output.push_str(&format!("      \"label\": \"{}\",\n", label));
            output.push_str(&format!(
                "      \"valid\": {},\n",
                result.is_valid()
            ));
            output.push_str("      \"errors\": [");

            let errors: Vec<String> = result
                .errors()
                .iter()
                .map(|e| format!("\"{}\"", e))
                .collect();
            output.push_str(&errors.join(", "));
            output.push_str("]\n");
            output.push_str("    }");
        }

        output.push_str("\n  ]\n}");
        output
    }
}

impl fmt::Display for Report {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.render(ReportFormat::Text))
    }
}
```

### Helpers compartidos (tests/common/)

```rust
// tests/common/mod.rs

use std::collections::HashMap;
use data_validator::{Rule, RuleSet};

/// Crea un HashMap a partir de pares clave-valor
pub fn make_record(fields: &[(&str, &str)]) -> HashMap<String, String> {
    fields
        .iter()
        .map(|(k, v)| (k.to_string(), v.to_string()))
        .collect()
}

/// RuleSet predefinido para validar usuarios
pub fn user_rules() -> RuleSet {
    RuleSet::new("user")
        .add(Rule::required("name"))
        .add(Rule::min_length("name", 2))
        .add(Rule::max_length("name", 50))
        .add(Rule::required("email"))
        .add(Rule::email_format("email"))
        .add(Rule::required("age"))
        .add(Rule::range("age", 0, 150))
}

/// Registro de usuario válido
pub fn valid_user() -> HashMap<String, String> {
    make_record(&[
        ("name", "Alice Smith"),
        ("email", "alice@example.com"),
        ("age", "30"),
    ])
}

/// Registro de usuario con todos los campos inválidos
pub fn invalid_user() -> HashMap<String, String> {
    make_record(&[
        ("name", ""),           // Requerido, vacío
        ("email", "not-email"), // Sin @
        ("age", "200"),         // Fuera de rango
    ])
}

/// Genera N registros de usuario válidos
pub fn generate_valid_users(count: usize) -> Vec<HashMap<String, String>> {
    (0..count)
        .map(|i| {
            make_record(&[
                ("name", &format!("User_{}", i)),
                ("email", &format!("user{}@test.com", i)),
                ("age", &format!("{}", 20 + (i % 60))),
            ])
        })
        .collect()
}
```

### Integration tests

```rust
// tests/rule_tests.rs
//
// Prueba la construcción y composición de reglas via API pública

use data_validator::{Rule, RuleSet};

#[test]
fn test_create_rule_set() {
    let rules = RuleSet::new("product")
        .add(Rule::required("name"))
        .add(Rule::min_length("name", 3))
        .add(Rule::range("price", 0, 100_000));

    assert_eq!(rules.name(), "product");
    assert_eq!(rules.rule_count(), 3);
}

#[test]
fn test_rule_set_is_composable() {
    // Puedes construir un RuleSet incrementalmente
    let mut rules = RuleSet::new("address");
    rules = rules.add(Rule::required("street"));
    rules = rules.add(Rule::required("city"));
    rules = rules.add(Rule::required("country"));
    rules = rules.add(Rule::min_length("zip", 5));
    rules = rules.add(Rule::max_length("zip", 10));

    assert_eq!(rules.rule_count(), 5);
}

#[test]
fn test_rule_display() {
    let rule = Rule::required("email");
    let display = format!("{}", rule);
    assert!(display.contains("email"));
    assert!(display.contains("required"));
}

#[test]
fn test_empty_rule_set() {
    let rules = RuleSet::new("empty");
    assert_eq!(rules.rule_count(), 0);
    assert_eq!(rules.name(), "empty");
}

#[test]
fn test_rule_preserves_field_name() {
    let rules = vec![
        Rule::required("name"),
        Rule::email_format("contact_email"),
        Rule::range("quantity", 1, 100),
    ];

    assert_eq!(rules[0].field(), "name");
    assert_eq!(rules[1].field(), "contact_email");
    assert_eq!(rules[2].field(), "quantity");
}

#[test]
fn test_rule_preserves_message() {
    let rule = Rule::min_length("password", 8);
    assert!(rule.message().contains("8"));
    assert!(rule.message().contains("password"));
}
```

```rust
// tests/validation_flow.rs
//
// Prueba flujos completos de validación

mod common;

use data_validator::{Validator, Rule, RuleSet};

#[test]
fn test_valid_record_passes() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);
    let record = common::valid_user();

    let result = validator.validate(&record);

    assert!(result.is_valid());
    assert_eq!(result.error_count(), 0);
}

#[test]
fn test_missing_required_field() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);
    let record = common::make_record(&[
        // "name" ausente
        ("email", "alice@example.com"),
        ("age", "30"),
    ]);

    let result = validator.validate(&record);

    assert!(!result.is_valid());
    assert!(result.has_error_for("name"));
}

#[test]
fn test_multiple_errors_collected() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);
    let record = common::invalid_user();

    let result = validator.validate(&record);

    assert!(!result.is_valid());
    assert!(result.error_count() >= 2);  // Al menos name y email fallan
    assert!(result.has_error_for("name"));
    assert!(result.has_error_for("email"));
}

#[test]
fn test_strict_mode_stops_at_first_error() {
    let rules = common::user_rules();
    let validator = Validator::new(rules).strict();
    let record = common::invalid_user();

    let result = validator.validate(&record);

    assert!(!result.is_valid());
    assert_eq!(result.error_count(), 1);  // Solo el primer error
}

#[test]
fn test_error_contains_field_value() {
    let rules = RuleSet::new("test")
        .add(Rule::email_format("email"));
    let validator = Validator::new(rules);
    let record = common::make_record(&[("email", "invalid")]);

    let result = validator.validate(&record);
    let errors = result.errors_for_field("email");

    assert_eq!(errors.len(), 1);
    assert_eq!(errors[0].value(), Some("invalid"));
}

#[test]
fn test_batch_validation() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);

    let records = vec![
        common::valid_user(),                                        // OK
        common::make_record(&[                                       // Fallo
            ("name", "A"),  // Muy corto
            ("email", "a@b.com"),
            ("age", "25"),
        ]),
        common::valid_user(),                                        // OK
        common::make_record(&[                                       // Fallo
            ("name", "Bob"),
            ("email", "no-arroba"),  // Email inválido
            ("age", "30"),
        ]),
    ];

    let failures = validator.validate_batch(&records);

    // Solo los registros inválidos aparecen
    assert_eq!(failures.len(), 2);
    assert_eq!(failures[0].0, 1);  // Índice del primer fallo
    assert_eq!(failures[1].0, 3);  // Índice del segundo fallo
}

#[test]
fn test_batch_all_valid() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);
    let records = common::generate_valid_users(100);

    let failures = validator.validate_batch(&records);
    assert!(failures.is_empty());
}

#[test]
fn test_fields_checked_count() {
    let rules = RuleSet::new("test")
        .add(Rule::required("a"))
        .add(Rule::required("b"))
        .add(Rule::required("c"));
    let validator = Validator::new(rules);

    let record = common::make_record(&[("a", "1"), ("b", "2"), ("c", "3")]);
    let result = validator.validate(&record);

    assert_eq!(result.fields_checked(), 3);
}

#[test]
fn test_empty_string_fails_required() {
    let rules = RuleSet::new("test")
        .add(Rule::required("field"));
    let validator = Validator::new(rules);

    // String vacío
    let record = common::make_record(&[("field", "")]);
    assert!(!validator.validate(&record).is_valid());

    // String con solo espacios
    let record = common::make_record(&[("field", "   ")]);
    assert!(!validator.validate(&record).is_valid());
}

#[test]
fn test_min_length_boundary() {
    let rules = RuleSet::new("test")
        .add(Rule::min_length("name", 3));
    let validator = Validator::new(rules);

    // Exactamente 3 → pasa
    let record = common::make_record(&[("name", "abc")]);
    assert!(validator.validate(&record).is_valid());

    // 2 → falla
    let record = common::make_record(&[("name", "ab")]);
    assert!(!validator.validate(&record).is_valid());

    // 4 → pasa
    let record = common::make_record(&[("name", "abcd")]);
    assert!(validator.validate(&record).is_valid());
}

#[test]
fn test_range_boundaries() {
    let rules = RuleSet::new("test")
        .add(Rule::range("value", 1, 10));
    let validator = Validator::new(rules);

    // Extremos inclusivos
    let record = common::make_record(&[("value", "1")]);
    assert!(validator.validate(&record).is_valid());

    let record = common::make_record(&[("value", "10")]);
    assert!(validator.validate(&record).is_valid());

    // Fuera de rango
    let record = common::make_record(&[("value", "0")]);
    assert!(!validator.validate(&record).is_valid());

    let record = common::make_record(&[("value", "11")]);
    assert!(!validator.validate(&record).is_valid());
}

#[test]
fn test_non_numeric_value_for_range() {
    let rules = RuleSet::new("test")
        .add(Rule::range("age", 0, 100));
    let validator = Validator::new(rules);

    let record = common::make_record(&[("age", "not_a_number")]);
    assert!(!validator.validate(&record).is_valid());
}
```

```rust
// tests/report_tests.rs
//
// Prueba la generación de reportes en distintos formatos

mod common;

use data_validator::{Validator, Report, ReportFormat};

fn create_report_with_results() -> Report {
    let rules = common::user_rules();
    let validator = Validator::new(rules);

    let mut report = Report::new("User Validation Report");

    // Registro válido
    let valid = common::valid_user();
    report.add_result("record_1", validator.validate(&valid));

    // Registro inválido
    let invalid = common::invalid_user();
    report.add_result("record_2", validator.validate(&invalid));

    // Otro válido
    let valid2 = common::make_record(&[
        ("name", "Bob Jones"),
        ("email", "bob@test.com"),
        ("age", "45"),
    ]);
    report.add_result("record_3", validator.validate(&valid2));

    report
}

#[test]
fn test_report_counts() {
    let report = create_report_with_results();

    assert_eq!(report.total_records(), 3);
    assert!(!report.all_valid());  // record_2 tiene errores
    assert!(report.total_errors() > 0);
}

#[test]
fn test_text_report_format() {
    let report = create_report_with_results();
    let text = report.render(ReportFormat::Text);

    // Título presente
    assert!(text.contains("User Validation Report"));

    // Registros reportados
    assert!(text.contains("[OK] record_1"));
    assert!(text.contains("[FAIL] record_2"));
    assert!(text.contains("[OK] record_3"));

    // Errores detallados para record_2
    assert!(text.contains("name"));
}

#[test]
fn test_csv_report_format() {
    let report = create_report_with_results();
    let csv = report.render(ReportFormat::Csv);

    // Header
    assert!(csv.starts_with("record,field,message,value\n"));

    // Solo registros con errores aparecen
    assert!(csv.contains("record_2"));
    assert!(!csv.contains("record_1"));  // record_1 es válido
    assert!(!csv.contains("record_3"));  // record_3 es válido
}

#[test]
fn test_json_report_format() {
    let report = create_report_with_results();
    let json = report.render(ReportFormat::Json);

    // Estructura JSON básica
    assert!(json.contains("\"title\": \"User Validation Report\""));
    assert!(json.contains("\"total_errors\":"));
    assert!(json.contains("\"results\":"));
    assert!(json.contains("\"valid\": true"));
    assert!(json.contains("\"valid\": false"));
}

#[test]
fn test_display_uses_text_format() {
    let report = create_report_with_results();
    let display = format!("{}", report);
    let text = report.render(ReportFormat::Text);

    assert_eq!(display, text);
}

#[test]
fn test_empty_report() {
    let report = Report::new("Empty");

    assert_eq!(report.total_records(), 0);
    assert_eq!(report.total_errors(), 0);
    assert!(report.all_valid());

    let text = report.render(ReportFormat::Text);
    assert!(text.contains("Empty"));
    assert!(text.contains("Records: 0"));
}

#[test]
fn test_report_all_valid() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);
    let mut report = Report::new("All Valid");

    for user in common::generate_valid_users(5) {
        report.add_result("user", validator.validate(&user));
    }

    assert!(report.all_valid());
    assert_eq!(report.total_errors(), 0);
    assert_eq!(report.total_records(), 5);
}
```

```rust
// tests/error_tests.rs
//
// Prueba el manejo de errores y casos límite

mod common;

use data_validator::{Validator, Rule, RuleSet, FieldError};

#[test]
fn test_error_display_with_value() {
    let rules = RuleSet::new("test")
        .add(Rule::email_format("email"));
    let validator = Validator::new(rules);

    let record = common::make_record(&[("email", "bad")]);
    let result = validator.validate(&record);
    let error = &result.errors()[0];

    let display = format!("{}", error);
    assert!(display.contains("email"));
    assert!(display.contains("bad"));
}

#[test]
fn test_error_display_without_value() {
    let rules = RuleSet::new("test")
        .add(Rule::required("missing_field"));
    let validator = Validator::new(rules);

    let record = common::make_record(&[]);  // Campo ausente
    let result = validator.validate(&record);
    let error = &result.errors()[0];

    let display = format!("{}", error);
    assert!(display.contains("missing_field"));
    assert!(display.contains("no value"));
}

#[test]
fn test_errors_for_specific_field() {
    let rules = RuleSet::new("test")
        .add(Rule::required("name"))
        .add(Rule::min_length("name", 5))
        .add(Rule::required("email"))
        .add(Rule::email_format("email"));
    let validator = Validator::new(rules);

    let record = common::make_record(&[
        ("name", "AB"),        // Falla min_length
        ("email", "invalid"),  // Falla email_format
    ]);
    let result = validator.validate(&record);

    let name_errors = result.errors_for_field("name");
    let email_errors = result.errors_for_field("email");

    assert_eq!(name_errors.len(), 1);   // Solo min_length falla (no required)
    assert_eq!(email_errors.len(), 1);  // Solo email_format falla
}

#[test]
fn test_unicode_values() {
    let rules = RuleSet::new("test")
        .add(Rule::required("name"))
        .add(Rule::min_length("name", 2));
    let validator = Validator::new(rules);

    let record = common::make_record(&[("name", "日本語")]);
    let result = validator.validate(&record);

    assert!(result.is_valid());
}

#[test]
fn test_very_long_value() {
    let rules = RuleSet::new("test")
        .add(Rule::max_length("data", 10));
    let validator = Validator::new(rules);

    let long_value = "x".repeat(10_000);
    let record = common::make_record(&[("data", &long_value)]);
    let result = validator.validate(&record);

    assert!(!result.is_valid());
}

#[test]
fn test_validate_empty_data() {
    let rules = common::user_rules();
    let validator = Validator::new(rules);

    let record = common::make_record(&[]);  // Sin datos

    let result = validator.validate(&record);
    assert!(!result.is_valid());
    // Todos los campos requeridos fallan
    assert!(result.has_error_for("name"));
    assert!(result.has_error_for("email"));
    assert!(result.has_error_for("age"));
}

#[test]
fn test_validate_with_no_rules() {
    let rules = RuleSet::new("empty");
    let validator = Validator::new(rules);

    let record = common::make_record(&[("anything", "whatever")]);
    let result = validator.validate(&record);

    assert!(result.is_valid());
    assert_eq!(result.fields_checked(), 0);
}

#[test]
fn test_negative_range_values() {
    let rules = RuleSet::new("test")
        .add(Rule::range("temperature", -40, 50));
    let validator = Validator::new(rules);

    let record = common::make_record(&[("temperature", "-20")]);
    assert!(validator.validate(&record).is_valid());

    let record = common::make_record(&[("temperature", "-41")]);
    assert!(!validator.validate(&record).is_valid());
}
```

### Output completo de cargo test

```
$ cargo test

   Compiling data_validator v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
     Running unittests src/lib.rs (target/debug/deps/data_validator-abc123)

running 0 tests
test result: ok. 0 passed; 0 failed; 0 ignored

     Running tests/error_tests.rs (target/debug/deps/error_tests-def456)

running 9 tests
test test_error_display_with_value ... ok
test test_error_display_without_value ... ok
test test_errors_for_specific_field ... ok
test test_negative_range_values ... ok
test test_unicode_values ... ok
test test_validate_empty_data ... ok
test test_validate_with_no_rules ... ok
test test_very_long_value ... ok
test result: ok. 9 passed; 0 failed; 0 ignored

     Running tests/report_tests.rs (target/debug/deps/report_tests-ghi789)

running 7 tests
test test_csv_report_format ... ok
test test_display_uses_text_format ... ok
test test_empty_report ... ok
test test_json_report_format ... ok
test test_report_all_valid ... ok
test test_report_counts ... ok
test test_text_report_format ... ok
test result: ok. 7 passed; 0 failed; 0 ignored

     Running tests/rule_tests.rs (target/debug/deps/rule_tests-jkl012)

running 6 tests
test test_create_rule_set ... ok
test test_empty_rule_set ... ok
test test_rule_display ... ok
test test_rule_preserves_field_name ... ok
test test_rule_preserves_message ... ok
test test_rule_set_is_composable ... ok
test result: ok. 6 passed; 0 failed; 0 ignored

     Running tests/validation_flow.rs (target/debug/deps/validation_flow-mno345)

running 13 tests
test test_batch_all_valid ... ok
test test_batch_validation ... ok
test test_empty_string_fails_required ... ok
test test_error_contains_field_value ... ok
test test_fields_checked_count ... ok
test test_min_length_boundary ... ok
test test_missing_required_field ... ok
test test_multiple_errors_collected ... ok
test test_non_numeric_value_for_range ... ok
test test_range_boundaries ... ok
test test_strict_mode_stops_at_first_error ... ok
test test_valid_record_passes ... ok
test result: ok. 13 passed; 0 failed; 0 ignored

   Doc-tests data_validator
running 0 tests
test result: ok. 0 passed; 0 failed; 0 ignored
```

### Ejecución selectiva

```bash
# Solo tests de reglas
$ cargo test --test rule_tests

# Solo tests de validación que contengan "batch"
$ cargo test --test validation_flow batch

# Solo tests de reportes
$ cargo test --test report_tests

# Todo excepto integration tests
$ cargo test --lib
```

---

## 21. Programa de práctica

Implementa un **sistema de archivos virtual en memoria** (`VirtualFS`) con integration tests completos.

### Requisitos de la biblioteca (src/)

```rust
// src/lib.rs — API pública que debes implementar

pub struct VirtualFS { /* ... */ }

pub struct FileInfo {
    pub name: String,
    pub size: usize,
    pub is_dir: bool,
}

#[derive(Debug, PartialEq)]
pub enum FsError {
    NotFound(String),
    AlreadyExists(String),
    NotADirectory(String),
    NotAFile(String),
    InvalidPath(String),
    DirectoryNotEmpty(String),
}

impl VirtualFS {
    /// Crea un FS vacío con solo el directorio raíz "/"
    pub fn new() -> Self { todo!() }

    /// Crea un directorio (falla si ya existe o si el padre no existe)
    pub fn mkdir(&mut self, path: &str) -> Result<(), FsError> { todo!() }

    /// Crea o sobrescribe un archivo con contenido
    pub fn write_file(&mut self, path: &str, content: &str) -> Result<(), FsError> { todo!() }

    /// Lee el contenido de un archivo
    pub fn read_file(&self, path: &str) -> Result<String, FsError> { todo!() }

    /// Elimina un archivo
    pub fn remove_file(&mut self, path: &str) -> Result<(), FsError> { todo!() }

    /// Elimina un directorio vacío
    pub fn rmdir(&mut self, path: &str) -> Result<(), FsError> { todo!() }

    /// Lista el contenido de un directorio
    pub fn list(&self, path: &str) -> Result<Vec<FileInfo>, FsError> { todo!() }

    /// Verifica si un path existe
    pub fn exists(&self, path: &str) -> bool { todo!() }

    /// Obtiene información de un path
    pub fn info(&self, path: &str) -> Result<FileInfo, FsError> { todo!() }

    /// Cuenta total de archivos (no directorios)
    pub fn file_count(&self) -> usize { todo!() }

    /// Cuenta total de directorios
    pub fn dir_count(&self) -> usize { todo!() }
}

impl std::fmt::Display for FsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { todo!() }
}
```

### Requisitos de los integration tests (tests/)

Debes crear **4 archivos de integration test** y **1 módulo de helpers**:

```
tests/
├── common/
│   └── mod.rs          // Al menos 3 helpers:
│                       //   - populated_fs() → VirtualFS con estructura predefinida
│                       //   - assert_file_content(fs, path, expected_content)
│                       //   - assert_not_exists(fs, path)
│
├── basic_ops.rs        // Al menos 8 tests:
│                       //   - Crear directorio
│                       //   - Crear archivo
│                       //   - Leer archivo
│                       //   - Eliminar archivo
│                       //   - Eliminar directorio
│                       //   - Listar directorio
│                       //   - Verificar exists
│                       //   - Verificar info
│
├── error_cases.rs      // Al menos 8 tests:
│                       //   - mkdir en directorio inexistente
│                       //   - mkdir duplicado
│                       //   - read_file de archivo inexistente
│                       //   - read_file de directorio
│                       //   - rmdir de directorio no vacío
│                       //   - rmdir de la raíz
│                       //   - remove_file de directorio
│                       //   - write_file con path inválido
│
├── workflows.rs        // Al menos 6 tests:
│                       //   - Crear estructura de directorios anidados
│                       //   - Sobrescribir archivo existente
│                       //   - Crear y eliminar múltiples archivos
│                       //   - Verificar conteo de archivos y directorios
│                       //   - Listar directorio con mezcla de archivos y subdirs
│                       //   - Estructura compleja (3+ niveles de profundidad)
│
└── display_tests.rs    // Al menos 4 tests:
                        //   - Display de cada variante de FsError
                        //   - FileInfo muestra nombre y tamaño correctos
                        //   - Verificar que los mensajes de error son informativos
                        //   - Error contiene el path que causó el problema
```

### Reglas adicionales

1. **Ningún test debe usar `mod tests` ni `#[cfg(test)]`** — son integration tests
2. **Cada archivo debe importar `mod common;`** y usar al menos un helper
3. **No acceder a campos internos** de `VirtualFS` — solo API pública
4. **Al menos 2 tests deben usar `#[should_panic]`** (por ejemplo, unwrap de un error esperado)
5. **Los helpers deben retornar un `VirtualFS` preconfigurado** con la siguiente estructura:

```
/
├── docs/
│   ├── readme.txt      (contenido: "Hello, World!")
│   └── notes/
│       └── todo.txt    (contenido: "Buy milk")
├── src/
│   ├── main.rs         (contenido: "fn main() {}")
│   └── lib.rs          (contenido: "pub fn hello() {}")
└── empty_dir/
```

---

## 22. Ejercicios

### Ejercicio 1: Análisis de estructura de tests

Dado el siguiente output de `cargo test`, responde las preguntas:

```
     Running unittests src/lib.rs (target/debug/deps/myapp-a1b2c3)
running 12 tests
test parser::tests::test_tokenize ... ok
test parser::tests::test_parse ... ok
test engine::tests::test_process ... ok
...
test result: ok. 12 passed; 0 failed

     Running tests/api_tests.rs (target/debug/deps/api_tests-d4e5f6)
running 5 tests
test test_create_resource ... ok
test test_get_resource ... ok
test test_update_resource ... ok
test test_delete_resource ... ok
test test_list_resources ... ok
test result: ok. 5 passed; 0 failed

     Running tests/integration.rs (target/debug/deps/integration-g7h8i9)
running 8 tests
test auth::test_login ... ok
test auth::test_logout ... ok
test data::test_import ... ok
test data::test_export ... ok
test data::test_transform ... ok
test errors::test_not_found ... ok
test errors::test_permission_denied ... ok
test errors::test_timeout ... ok
test result: ok. 8 passed; 0 failed

   Doc-tests myapp
running 3 tests
...
test result: ok. 3 passed; 0 failed
```

**a)** ¿Cuántos binarios de test se generaron? ¿Cuáles son?

**b)** `tests/integration.rs` tiene tests con nombres como `auth::test_login`. ¿Qué estructura de archivos produce esto? Dibuja el árbol de directorios de `tests/`.

**c)** Si solo quisieras ejecutar los tests de `tests/api_tests.rs`, ¿qué comando usarías?

**d)** Si quisieras ejecutar solo los tests de `auth` dentro de `tests/integration.rs`, ¿qué comando usarías?

**e)** ¿Cuántas veces se compiló `src/lib.rs` en total? ¿Por qué?

---

### Ejercicio 2: Diseño de API para testabilidad

Tienes esta estructura que NO es testable desde integration tests:

```rust
// src/lib.rs
pub struct Encryptor {
    key: [u8; 32],
    rounds: u32,
}

impl Encryptor {
    pub fn new(password: &str) -> Self {
        let key = derive_key(password);
        Encryptor { key, rounds: 10 }
    }

    pub fn encrypt(&self, data: &[u8]) -> Vec<u8> {
        let mut result = data.to_vec();
        for _ in 0..self.rounds {
            for byte in &mut result {
                *byte ^= self.key[0];  // Simplificación
            }
        }
        result
    }
}

fn derive_key(password: &str) -> [u8; 32] {
    let mut key = [0u8; 32];
    for (i, byte) in password.bytes().enumerate() {
        key[i % 32] ^= byte;
    }
    key
}
```

**a)** ¿Por qué un integration test no puede verificar que `encrypt()` produce el resultado correcto? ¿Qué falta en la API?

**b)** Añade los métodos públicos mínimos necesarios para que un integration test pueda verificar:
- Que encriptar y desencriptar produce el dato original (round-trip)
- Que dos Encryptors con la misma contraseña producen el mismo resultado
- Que encriptar con diferente contraseña produce resultado diferente

**c)** Escribe los 3 integration tests mencionados en (b).

**d)** ¿Debería `derive_key` ser pública? Argumenta por qué sí o por qué no.

---

### Ejercicio 3: Migración de tests de C a Rust

Traduce este archivo de integration tests de C a Rust. Mantén la misma cobertura pero usa patrones idiomáticos de Rust:

```c
// tests/test_stack.c
#include "unity.h"
#include "../src/stack.h"

static Stack *s;

void setUp(void) {
    s = stack_new(10);  // Capacidad inicial 10
}

void tearDown(void) {
    stack_free(s);
    s = NULL;
}

void test_new_stack_is_empty(void) {
    TEST_ASSERT_TRUE(stack_is_empty(s));
    TEST_ASSERT_EQUAL(0, stack_size(s));
}

void test_push_increases_size(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL(1, stack_size(s));
    stack_push(s, 99);
    TEST_ASSERT_EQUAL(2, stack_size(s));
}

void test_pop_returns_last_pushed(void) {
    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);
    TEST_ASSERT_EQUAL(30, stack_pop(s));
    TEST_ASSERT_EQUAL(20, stack_pop(s));
    TEST_ASSERT_EQUAL(10, stack_pop(s));
}

void test_peek_does_not_remove(void) {
    stack_push(s, 42);
    TEST_ASSERT_EQUAL(42, stack_peek(s));
    TEST_ASSERT_EQUAL(42, stack_peek(s));
    TEST_ASSERT_EQUAL(1, stack_size(s));
}

void test_grows_beyond_initial_capacity(void) {
    for (int i = 0; i < 100; i++) {
        stack_push(s, i);
    }
    TEST_ASSERT_EQUAL(100, stack_size(s));
    TEST_ASSERT_EQUAL(99, stack_peek(s));
}

void test_pop_empty_returns_error(void) {
    int result = stack_pop(s);
    TEST_ASSERT_EQUAL(STACK_UNDERFLOW, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_new_stack_is_empty);
    RUN_TEST(test_push_increases_size);
    RUN_TEST(test_pop_returns_last_pushed);
    RUN_TEST(test_peek_does_not_remove);
    RUN_TEST(test_grows_beyond_initial_capacity);
    RUN_TEST(test_pop_empty_returns_error);
    return UNITY_END();
}
```

**a)** Escribe el archivo `tests/stack_integration.rs` equivalente. ¿Necesitas setUp/tearDown? ¿Qué los reemplaza?

**b)** Escribe el archivo `tests/common/mod.rs` con un helper que cree un stack pre-llenado.

**c)** Identifica 3 diferencias fundamentales entre la versión C y la versión Rust de estos tests.

---

### Ejercicio 4: Diagnóstico de errores

Para cada fragmento, identifica el error y escribe la versión corregida:

**Fragmento A:**
```rust
// tests/test.rs
use mi_crate::internal::helper;  // helper es pub(crate)

#[test]
fn test_helper() {
    assert_eq!(helper(), 42);
}
```

**Fragmento B:**
```rust
// tests/alpha.rs
mod common;  // tests/common/mod.rs

pub fn shared_setup() -> Database {
    common::setup_db()
}
```

```rust
// tests/beta.rs
use alpha::shared_setup;  // Intenta importar de otro test file

#[test]
fn test_something() {
    let db = shared_setup();
}
```

**Fragmento C:**
```rust
// tests/test.rs
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_something() {
        let w = Widget::new();
        assert!(w.is_valid());
    }
}
```

**Fragmento D:**
```
// Estructura de archivos:
tests/
├── common.rs        ← Helpers
└── feature_test.rs  ← Tests
```

```rust
// tests/feature_test.rs
mod common;  // Intenta importar helpers

#[test]
fn test_feature() {
    let ctx = common::setup();
}
```

---

## Navegación

- **Anterior**: [S01/T04 - Organización](../../S01_Testing_Built_in/T04_Organizacion/README.md)
- **Siguiente**: [T02 - Doc tests](../T02_Doc_tests/README.md)

---

> **Sección 2: Integration Tests y Doc Tests** — Tópico 1 de 4 completado
>
> - T01: Integration tests — directorio tests/, crate separado, API pública, helpers compartidos ✓
> - T02: Doc tests (siguiente)
> - T03: Test helpers compartidos
> - T04: cargo test flags
