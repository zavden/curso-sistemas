# T04 - Organización de Tests en Rust

## Índice

1. [El problema de organizar tests](#1-el-problema-de-organizar-tests)
2. [Tests junto al código: el patrón inline](#2-tests-junto-al-código-el-patrón-inline)
3. [El módulo tests: convención y estructura](#3-el-módulo-tests-convención-y-estructura)
4. [#[cfg(test)]: compilación condicional](#4-cfgtest-compilación-condicional)
5. [use super::*: acceso al módulo padre](#5-use-super-acceso-al-módulo-padre)
6. [Archivo tests.rs separado: cuándo y cómo](#6-archivo-testsrs-separado-cuándo-y-cómo)
7. [Organización en proyectos multi-archivo](#7-organización-en-proyectos-multi-archivo)
8. [Organización en bibliotecas vs binarios](#8-organización-en-bibliotecas-vs-binarios)
9. [Imports condicionales con #[cfg(test)]](#9-imports-condicionales-con-cfgtest)
10. [Helpers y utilidades de test](#10-helpers-y-utilidades-de-test)
11. [Fixtures y datos de prueba](#11-fixtures-y-datos-de-prueba)
12. [Test modules anidados](#12-test-modules-anidados)
13. [Visibilidad y pub(crate) para testing](#13-visibilidad-y-pubcrate-para-testing)
14. [Organización de tests según tipo](#14-organización-de-tests-según-tipo)
15. [Patrones de nombrado](#15-patrones-de-nombrado)
16. [Comparación con C y Go](#16-comparación-con-c-y-go)
17. [Errores comunes](#17-errores-comunes)
18. [Ejemplo completo: biblioteca de colecciones](#18-ejemplo-completo-biblioteca-de-colecciones)
19. [Programa de práctica](#19-programa-de-práctica)
20. [Ejercicios](#20-ejercicios)

---

## 1. El problema de organizar tests

En C, la separación entre código de producción y código de test es obligatoria: los tests viven en archivos separados, usan un framework externo, y requieren Makefiles dedicados. Esto crea fricción:

```
Proyecto C típico:
src/
  stack.c          // Implementación
  stack.h          // Interfaz pública
tests/
  test_stack.c     // Tests (archivo separado, otro directorio)
  unity.c          // Framework (dependencia externa)
  unity.h
Makefile           // Reglas para compilar tests separadamente
```

Rust toma la decisión opuesta: los tests **viven junto al código que prueban**, dentro del mismo archivo. Esta decisión tiene consecuencias profundas:

```
Proyecto Rust típico:
src/
  lib.rs           // Implementación + tests en el mismo archivo
  stack.rs         // Implementación + tests en el mismo archivo
Cargo.toml         // No necesita configuración extra para tests
```

### ¿Por qué junto al código?

```
┌─────────────────────────────────────────────────┐
│         Argumentos a favor (inline)             │
├─────────────────────────────────────────────────┤
│ 1. Proximidad: ves el código y sus tests juntos │
│ 2. Refactoring: mover código mueve sus tests    │
│ 3. Contexto: no hay que buscar en otro archivo  │
│ 4. Descubrimiento: todo dev ve que hay tests    │
│ 5. Acceso: tests pueden probar funciones priv.  │
│ 6. Sin config: cargo test funciona sin setup    │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│        Argumentos en contra (separado)          │
├─────────────────────────────────────────────────┤
│ 1. Archivos largos: 500 líneas impl + 500 test │
│ 2. Ruido visual: distrae al leer el código      │
│ 3. Ratio: puede haber más test que código       │
│ 4. Navegación: scroll largo en archivos densos  │
└─────────────────────────────────────────────────┘
```

La comunidad Rust ha convergido en un consenso: **tests junto al código para unit tests**, archivos separados para integration tests. Este tópico explora todas las variantes y cuándo usar cada una.

### Mapa de las tres zonas de testing en Rust

```
mi_proyecto/
├── src/
│   ├── lib.rs          ─── Zone 1: Unit tests (inline, #[cfg(test)])
│   ├── parser.rs       ─── Zone 1: Unit tests (inline, #[cfg(test)])
│   └── utils.rs        ─── Zone 1: Unit tests (inline, #[cfg(test)])
├── tests/
│   ├── integration.rs  ─── Zone 2: Integration tests (crate separado)
│   └── common/
│       └── mod.rs      ─── Zone 2: Helpers compartidos
└── src/lib.rs          ─── Zone 3: Doc tests (en /// comments)
     /// ```
     /// let x = mi_crate::foo();
     /// ```

cargo test ejecuta las 3 zonas automáticamente:
  Running unittests src/lib.rs        ← Zone 1
  Running tests/integration.rs       ← Zone 2
  Doc-tests mi_crate                 ← Zone 3
```

---

## 2. Tests junto al código: el patrón inline

El patrón estándar de Rust coloca los tests al final del mismo archivo que contiene la implementación:

```rust
// src/math.rs

/// Calcula el factorial de n.
/// Panics si n > 20 (overflow en u64).
pub fn factorial(n: u64) -> u64 {
    match n {
        0 | 1 => 1,
        _ => n * factorial(n - 1),
    }
}

/// Calcula combinaciones C(n, k) = n! / (k! * (n-k)!)
pub fn combinations(n: u64, k: u64) -> u64 {
    if k > n {
        return 0;
    }
    factorial(n) / (factorial(k) * factorial(n - k))
}

// ──────────────── Tests al final del archivo ────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn factorial_zero() {
        assert_eq!(factorial(0), 1);
    }

    #[test]
    fn factorial_one() {
        assert_eq!(factorial(1), 1);
    }

    #[test]
    fn factorial_five() {
        assert_eq!(factorial(5), 120);
    }

    #[test]
    fn combinations_basic() {
        assert_eq!(combinations(5, 2), 10);
    }

    #[test]
    fn combinations_k_greater_than_n() {
        assert_eq!(combinations(3, 5), 0);
    }

    #[test]
    fn combinations_k_equals_n() {
        assert_eq!(combinations(5, 5), 1);
    }
}
```

### Anatomía del patrón

```
src/math.rs
┌──────────────────────────────────────┐
│  Imports (use std::...)              │  ← Código de producción
│                                      │
│  pub fn factorial(n: u64) -> u64 {   │
│      ...                             │
│  }                                   │
│                                      │
│  pub fn combinations(...) -> u64 {   │
│      ...                             │
│  }                                   │
├──────────────────────────────────────┤
│  #[cfg(test)]     ← BARRERA         │  ← Solo existe en cargo test
│  mod tests {                         │
│      use super::*;                   │
│                                      │
│      #[test]                         │
│      fn factorial_zero() { ... }     │
│                                      │
│      #[test]                         │
│      fn combinations_basic() { ... } │
│  }                                   │
└──────────────────────────────────────┘

cargo build  →  compila solo la parte superior
cargo test   →  compila TODO (incluyendo mod tests)
```

### Reglas del patrón inline

| Regla | Detalle |
|-------|---------|
| Posición | `mod tests` va **al final** del archivo |
| Atributo | Siempre `#[cfg(test)]` en el módulo |
| Import | `use super::*` para acceder al código del padre |
| Nombre | El módulo se llama `tests` por convención |
| Separador | Comentario visual separando implementación de tests |
| Un módulo | Solo **un** `mod tests` por archivo |

### ¿Qué pasa si no se usa #[cfg(test)]?

```rust
// SIN #[cfg(test)] — INCORRECTO
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
```

Consecuencias:

```
1. El módulo tests se compila en cargo build (producción)
2. use super::* importa todo en un módulo que existe en el binario
3. El compilador puede emitir warnings por código sin usar
4. El binario final es más grande innecesariamente
5. Dependencias solo de test (como tempfile) no compilarían

warning: function `it_works` is never used
 --> src/math.rs:20:8
  |
20|     fn it_works() {
  |        ^^^^^^^^
```

---

## 3. El módulo tests: convención y estructura

### ¿Por qué `mod tests` y no funciones sueltas?

Se podría marcar funciones con `#[test]` sin meterlas en un módulo:

```rust
// Funciones test sueltas — FUNCIONA pero no es idiomático
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

#[test]
fn test_add() {
    assert_eq!(add(1, 2), 3);
}
```

Esto compila y ejecuta, pero tiene problemas:

```
┌─────────────────────────────────────────────────────┐
│       Funciones sueltas vs mod tests                │
├──────────────────┬──────────────────────────────────┤
│ Sueltas          │ mod tests                        │
├──────────────────┼──────────────────────────────────┤
│ Sin namespace    │ Namespace tests::               │
│ Se compilan      │ #[cfg(test)] las excluye         │
│  siempre         │  de producción                   │
│ No pueden tener  │ Pueden tener helpers             │
│  helpers privados│  privados al módulo              │
│ Mezclan nombres  │ Nombres limpios:                 │
│  en el namespace │  tests::add_positive             │
│  del módulo      │                                  │
│ No convencional  │ Convención universal de Rust     │
└──────────────────┴──────────────────────────────────┘
```

### El namespace `tests::`

Cuando usas `mod tests`, cada función test tiene un path completo:

```
cargo test
running 3 tests
test math::tests::factorial_zero ... ok
test math::tests::factorial_five ... ok
test math::tests::combinations_basic ... ok
     ^^^^  ^^^^^  ^^^^^^^^^^^^^^^^^
     módulo  mod    función test
     padre   tests
```

Este namespace permite:

```bash
# Ejecutar solo tests de un módulo específico
cargo test math::tests

# Ejecutar un test específico
cargo test math::tests::factorial_zero

# Ejecutar tests que contengan "combination" en cualquier módulo
cargo test combination
```

### Estructura interna del módulo tests

El módulo `tests` es un módulo Rust normal, lo que significa que puede contener:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // ── Constantes de test ──────────────────────────
    const EPSILON: f64 = 1e-10;
    const TEST_INPUT: &str = "hello world";

    // ── Structs auxiliares ──────────────────────────
    struct TestCase {
        input: i32,
        expected: i32,
    }

    // ── Funciones helper (no #[test]) ───────────────
    fn create_test_data() -> Vec<TestCase> {
        vec![
            TestCase { input: 0, expected: 1 },
            TestCase { input: 1, expected: 1 },
            TestCase { input: 5, expected: 120 },
        ]
    }

    fn assert_approx_eq(a: f64, b: f64) {
        assert!(
            (a - b).abs() < EPSILON,
            "Expected {} ≈ {}, diff = {}",
            a, b, (a - b).abs()
        );
    }

    // ── Tests ───────────────────────────────────────
    #[test]
    fn factorial_table() {
        for tc in create_test_data() {
            assert_eq!(
                factorial(tc.input as u64),
                tc.expected as u64,
                "factorial({}) should be {}",
                tc.input, tc.expected
            );
        }
    }

    #[test]
    fn sqrt_approximation() {
        assert_approx_eq(2.0_f64.sqrt(), 1.41421356237);
    }
}
```

### El módulo tests como scope de privacidad

Todo lo definido dentro de `mod tests` es privado al módulo de tests:

```rust
pub fn public_api() -> i32 { 42 }

fn internal_helper() -> i32 { 7 }

#[cfg(test)]
mod tests {
    use super::*;

    // Esta función solo existe en tests, no contamina el módulo padre
    fn setup_test_environment() -> Vec<i32> {
        vec![1, 2, 3, 4, 5]
    }

    // Esta constante solo existe en tests
    const MAX_TEST_SIZE: usize = 1000;

    #[test]
    fn test_with_setup() {
        let data = setup_test_environment();
        assert_eq!(data.len(), 5);
    }

    #[test]
    fn test_internal() {
        // Acceso a funciones privadas del módulo padre
        assert_eq!(internal_helper(), 7);
    }
}

// setup_test_environment y MAX_TEST_SIZE NO existen aquí
// No contaminan el namespace del módulo
```

---

## 4. #[cfg(test)]: compilación condicional

### ¿Qué es cfg?

`cfg` (configuration) es el sistema de compilación condicional de Rust. Permite incluir o excluir código basándose en condiciones evaluadas en tiempo de compilación:

```rust
// Código que solo existe en Linux
#[cfg(target_os = "linux")]
fn linux_only() { /* ... */ }

// Código que solo existe en modo debug
#[cfg(debug_assertions)]
fn debug_only() { /* ... */ }

// Código que solo existe cuando se ejecutan tests
#[cfg(test)]
mod tests { /* ... */ }
```

### El predicado `test`

El predicado `test` es `true` cuando se compila con `cargo test` (que internamente pasa `--test` al compilador):

```
┌──────────────────────────────────────────────┐
│                cargo build                    │
│                                              │
│  rustc src/lib.rs                            │
│    cfg(test) = false                         │
│    → mod tests se IGNORA completamente       │
│    → no se compila, no existe en el binario  │
└──────────────────────────────────────────────┘

┌──────────────────────────────────────────────┐
│                cargo test                     │
│                                              │
│  rustc --test src/lib.rs                     │
│    cfg(test) = true                          │
│    → mod tests se COMPILA                    │
│    → funciones #[test] se registran          │
│    → se genera un main() que ejecuta tests   │
└──────────────────────────────────────────────┘
```

### Granularidad de #[cfg(test)]

`#[cfg(test)]` puede aplicarse a diferentes niveles:

```rust
// ── Nivel 1: Módulo completo (lo más común) ────────
#[cfg(test)]
mod tests {
    // Todo este módulo solo existe en cargo test
}

// ── Nivel 2: Función individual ────────────────────
#[cfg(test)]
fn test_helper() -> String {
    "solo para tests".to_string()
}

// ── Nivel 3: Implementación (impl block) ───────────
pub struct Database {
    connection: String,
}

impl Database {
    pub fn new(conn: &str) -> Self {
        Database { connection: conn.to_string() }
    }

    pub fn query(&self, sql: &str) -> Vec<String> {
        // implementación real
        vec![]
    }
}

#[cfg(test)]
impl Database {
    /// Constructor simplificado solo disponible en tests
    fn mock(data: Vec<String>) -> Self {
        Database { connection: "mock".to_string() }
    }

    /// Método de inspección solo disponible en tests
    fn connection_string(&self) -> &str {
        &self.connection
    }
}

// ── Nivel 4: Campo de struct (raro pero posible) ───
pub struct Config {
    pub host: String,
    pub port: u16,
    #[cfg(test)]
    pub test_mode: bool,  // Solo existe en tests
}
```

### cfg(test) vs cfg(debug_assertions)

Una confusión frecuente:

```
┌──────────────────┬──────────────────────────────────────┐
│ Predicado        │ Cuándo es true                       │
├──────────────────┼──────────────────────────────────────┤
│ cfg(test)        │ cargo test (compilación de tests)    │
│                  │ NUNCA en cargo build ni cargo run    │
├──────────────────┼──────────────────────────────────────┤
│ cfg(debug_asser  │ cargo build (modo debug, por defecto)│
│  tions)          │ cargo test (que usa debug)           │
│                  │ NO en cargo build --release          │
├──────────────────┼──────────────────────────────────────┤
│ cfg(not(test))   │ cargo build, cargo run               │
│                  │ NO en cargo test                     │
└──────────────────┴──────────────────────────────────────┘
```

```rust
// debug_assert! usa cfg(debug_assertions) internamente
pub fn divide(a: f64, b: f64) -> f64 {
    debug_assert!(b != 0.0, "division by zero"); // Solo en debug
    a / b
}

// Esto es diferente de cfg(test):
#[cfg(test)]
fn this_only_exists_in_test_binary() {}

#[cfg(debug_assertions)]
fn this_exists_in_debug_build_AND_test() {}
```

### Verificar que cfg(test) funciona

```rust
pub fn am_i_testing() -> bool {
    cfg!(test) // Macro que retorna bool en runtime
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn verify_test_mode() {
        assert!(am_i_testing()); // true en cargo test
    }
}

// En cargo run:
// am_i_testing() retorna false
```

Nota: `cfg!()` (con `!`, macro) retorna un `bool` en runtime. `#[cfg()]` (sin `!`, atributo) elimina código en compilación. Son mecanismos diferentes:

```rust
// #[cfg(test)] — elimina el código completamente
#[cfg(test)]
fn only_in_tests() {} // No existe en el binario de producción

// cfg!(test) — compila siempre, evalúa a true/false
fn works_everywhere() -> bool {
    if cfg!(test) {
        true  // Este branch existe pero retorna false en producción
    } else {
        false
    }
}
```

---

## 5. use super::*: acceso al módulo padre

### El sistema de módulos de Rust

Para entender `use super::*` hay que entender cómo funciona `mod tests` dentro de un archivo:

```
src/math.rs
┌─────────────────────────────────┐
│ módulo math (implícito)         │
│                                 │
│  pub fn factorial(n: u64) {...} │
│  fn helper() {...}              │  ← super (módulo padre)
│  struct Internal {...}          │
│                                 │
│  ┌────────────────────────────┐ │
│  │ mod tests (hijo)           │ │
│  │                            │ │
│  │  use super::*;  ──────────►│─│── importa factorial, helper, Internal
│  │                            │ │
│  │  #[test]                   │ │
│  │  fn test_factorial() {...} │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘
```

`mod tests` es un módulo hijo del módulo donde se define. `super` se refiere al módulo padre. `use super::*` importa todo lo que el módulo padre tiene en scope.

### ¿Qué importa `use super::*`?

```rust
// src/calculator.rs

use std::collections::HashMap;  // (1) Import del padre

pub const PI: f64 = 3.14159;   // (2) Constante pública
const E: f64 = 2.71828;        // (3) Constante privada

pub struct Calculator {         // (4) Struct público
    history: Vec<f64>,
}

struct InternalState {          // (5) Struct privado
    cache: HashMap<String, f64>,
}

pub fn add(a: f64, b: f64) -> f64 { a + b }  // (6) Función pública
fn validate(x: f64) -> bool { x.is_finite() } // (7) Función privada

pub enum Operation {            // (8) Enum público
    Add, Sub, Mul, Div,
}

type Result = std::result::Result<f64, String>; // (9) Type alias

pub trait Computable {          // (10) Trait público
    fn compute(&self) -> f64;
}

#[cfg(test)]
mod tests {
    use super::*;
    // Después de use super::*; están disponibles:
    //
    // (1) HashMap (re-importado desde el padre)     ✓
    // (2) PI                                         ✓
    // (3) E  (privada, pero hijo tiene acceso)       ✓
    // (4) Calculator                                 ✓
    // (5) InternalState (privada, pero accesible)    ✓
    // (6) add                                        ✓
    // (7) validate (privada, pero accesible)         ✓
    // (8) Operation, Operation::Add, etc.            ✓
    // (9) Result (el type alias)                     ✓
    // (10) Computable                                ✓

    #[test]
    fn test_private_access() {
        // Podemos usar funciones privadas
        assert!(validate(1.0));
        assert!(!validate(f64::NAN));

        // Podemos crear structs privados
        let state = InternalState {
            cache: HashMap::new(),
        };

        // Podemos usar constantes privadas
        assert!((E - 2.71828).abs() < 1e-10);
    }
}
```

### use super::* vs imports selectivos

```rust
#[cfg(test)]
mod tests {
    // Opción 1: Importar todo (lo más común)
    use super::*;

    // Opción 2: Importar selectivamente
    // use super::{factorial, combinations};

    // Opción 3: Usar paths completos
    // sin ningún use, referencia directa:
    // super::factorial(5)
}
```

Comparación:

```
┌──────────────────┬───────────────────────────────────────┐
│ Estilo           │ Cuándo usarlo                         │
├──────────────────┼───────────────────────────────────────┤
│ use super::*     │ Por defecto. El módulo padre es       │
│                  │ pequeño/mediano. Quieres acceso a     │
│                  │ todo sin pensar.                      │
├──────────────────┼───────────────────────────────────────┤
│ use super::{a,b} │ Módulo padre muy grande. Quieres      │
│                  │ documentar qué usas. Hay conflictos   │
│                  │ de nombres.                           │
├──────────────────┼───────────────────────────────────────┤
│ super::func()    │ Una o dos referencias. No vale la     │
│                  │ pena un use.                          │
└──────────────────┴───────────────────────────────────────┘
```

### Conflictos de nombres con use super::*

Un caso donde `use super::*` puede causar problemas:

```rust
// src/parser.rs

pub type Result<T> = std::result::Result<T, ParseError>;  // Type alias

pub struct Error {  // Nombre genérico
    message: String,
}

#[cfg(test)]
mod tests {
    use super::*;
    // Ahora "Result" se refiere a super::Result, no std::result::Result
    // Ahora "Error" se refiere a super::Error, no std::error::Error

    // Si necesitas el Result estándar:
    use std::result::Result as StdResult;

    #[test]
    fn test_with_std_result() -> StdResult<(), Box<dyn std::error::Error>> {
        // Usar StdResult para evitar ambigüedad
        let parsed: Result<i32> = Ok(42); // Este es super::Result
        assert_eq!(parsed?, 42);
        Ok(())
    }

    // Alternativa: importar selectivamente
    // use super::{parse, ParseError};  // No importar Result ni Error
}
```

### Acceso a módulos hermanos

`use super::*` solo importa del padre directo. Para acceder a módulos hermanos:

```rust
// src/lib.rs
pub mod parser;
pub mod lexer;

// src/parser.rs
use crate::lexer::Token;  // Acceso a módulo hermano via crate root

pub fn parse(tokens: &[Token]) -> Ast { /* ... */ }

#[cfg(test)]
mod tests {
    use super::*;
    // parse está disponible via super::*
    // Token está disponible porque parser.rs ya lo importó
    // con use crate::lexer::Token

    // Si necesitas algo más de lexer:
    use crate::lexer::Lexer;

    #[test]
    fn test_parse() {
        let lexer = Lexer::new("1 + 2");
        let tokens = lexer.tokenize();
        let ast = parse(&tokens);
        // ...
    }
}
```

---

## 6. Archivo tests.rs separado: cuándo y cómo

### El problema de archivos muy largos

Cuando un módulo tiene mucha implementación y muchos tests, el archivo puede volverse excesivamente largo:

```
src/parser.rs
├── Imports ..................  15 líneas
├── Tipos ...................  80 líneas
├── impl Parser .............  350 líneas
├── impl Display ............  40 líneas
├── Funciones helper ........  60 líneas
├── ─── separador ─────────
├── mod tests ...............  600 líneas   ← ¡Más test que código!
└── Total: 1145 líneas
```

### Solución: archivo tests.rs separado dentro de src/

Rust permite declarar módulos en archivos separados. El módulo `tests` no es excepción:

```
Estructura con tests en archivo separado:

src/
├── parser.rs           // Solo implementación (545 líneas)
└── parser/
    └── tests.rs        // Solo tests (600 líneas)
```

O equivalentemente con la convención antigua:

```
src/
├── parser/
│   ├── mod.rs          // Implementación
│   └── tests.rs        // Tests
```

### Implementación paso a paso

**Paso 1: Reorganizar de archivo a directorio**

El archivo `src/parser.rs` se convierte en `src/parser/mod.rs` (o se usa la convención moderna con `src/parser.rs` + `src/parser/`):

```
ANTES:                      DESPUÉS (convención moderna):
src/                        src/
├── lib.rs                  ├── lib.rs
└── parser.rs               ├── parser.rs        ← implementación
                            └── parser/
                                └── tests.rs     ← tests
```

**Paso 2: Mover tests al archivo separado**

```rust
// src/parser.rs — Solo implementación
pub struct Parser {
    input: String,
    position: usize,
}

impl Parser {
    pub fn new(input: &str) -> Self {
        Parser {
            input: input.to_string(),
            position: 0,
        }
    }

    pub fn parse(&mut self) -> Result<Ast, ParseError> {
        // ... implementación completa
        todo!()
    }

    fn peek(&self) -> Option<char> {
        self.input.chars().nth(self.position)
    }

    fn advance(&mut self) {
        self.position += 1;
    }
}

// Declarar el módulo tests como archivo separado
#[cfg(test)]
mod tests;   // ← Rust busca parser/tests.rs
```

```rust
// src/parser/tests.rs — Solo tests
use super::*;  // Importa de parser.rs (el módulo padre)

#[test]
fn parse_empty() {
    let mut parser = Parser::new("");
    assert!(parser.parse().is_err());
}

#[test]
fn parse_number() {
    let mut parser = Parser::new("42");
    let ast = parser.parse().unwrap();
    assert_eq!(ast, Ast::Number(42));
}

#[test]
fn peek_returns_first_char() {
    let parser = Parser::new("hello");
    assert_eq!(parser.peek(), Some('h'));
}

#[test]
fn advance_moves_position() {
    let mut parser = Parser::new("ab");
    parser.advance();
    assert_eq!(parser.peek(), Some('b'));
}

// ... 600 líneas de tests
```

### Detalles importantes del archivo separado

```
┌─────────────────────────────────────────────────────────┐
│                   REGLAS CLAVE                           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. NO pongas #[cfg(test)] en tests.rs                  │
│     Ya está declarado en parser.rs:                     │
│     #[cfg(test)] mod tests;                             │
│     El archivo entero es condicional.                   │
│                                                         │
│  2. NO pongas mod tests { } en tests.rs                 │
│     El archivo YA ES el módulo tests.                   │
│     Envolver en mod tests { } crearía tests::tests.     │
│                                                         │
│  3. SÍ pon use super::* en tests.rs                     │
│     super se refiere al módulo padre (parser).          │
│                                                         │
│  4. SÍ pon #[test] en cada función de test              │
│     Igual que si estuvieran inline.                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Error frecuente: doble wrapping

```rust
// src/parser/tests.rs — INCORRECTO ✗
#[cfg(test)]        // ← REDUNDANTE: ya está en parser.rs
mod tests {         // ← INCORRECTO: crea parser::tests::tests
    use super::*;   // ← Ahora super es parser::tests, no parser

    #[test]
    fn test_something() {
        // Parser no está en scope porque super es el módulo equivocado
    }
}
```

```rust
// src/parser/tests.rs — CORRECTO ✓
use super::*;  // super = parser (el módulo padre)

#[test]
fn test_something() {
    let p = Parser::new("test");
    // Parser está en scope correctamente
}
```

### ¿Cuándo usar archivo separado?

```
┌──────────────────┬────────────────────────────────────────┐
│ Criterio         │ Recomendación                          │
├──────────────────┼────────────────────────────────────────┤
│ Tests < 100 lín. │ Inline (junto al código)               │
│ Tests 100-300    │ Depende de preferencia del equipo      │
│ Tests > 300 lín. │ Considerar archivo separado            │
│ Archivo total    │ Si supera ~500 lín, separar mejora     │
│  > 500 líneas    │  la navegación                         │
│ Muchos helpers   │ Archivo separado para aislar helpers   │
│ Tests simples    │ Inline siempre (la proximidad ayuda)   │
└──────────────────┴────────────────────────────────────────┘
```

---

## 7. Organización en proyectos multi-archivo

### Proyecto real con múltiples módulos

```
mi_proyecto/
├── Cargo.toml
├── src/
│   ├── lib.rs          // Raíz del crate
│   ├── config.rs       // Módulo de configuración
│   ├── parser.rs       // Módulo de parsing
│   ├── evaluator.rs    // Módulo de evaluación
│   └── error.rs        // Tipos de error
└── tests/
    └── integration.rs  // Tests de integración (S02)
```

Cada módulo tiene sus propios unit tests inline:

```rust
// src/lib.rs
pub mod config;
pub mod parser;
pub mod evaluator;
pub mod error;

/// Re-exports para la API pública
pub use config::Config;
pub use parser::Parser;
pub use evaluator::evaluate;
pub use error::Error;
```

```rust
// src/config.rs
use std::collections::HashMap;

pub struct Config {
    values: HashMap<String, String>,
}

impl Config {
    pub fn new() -> Self {
        Config { values: HashMap::new() }
    }

    pub fn set(&mut self, key: &str, value: &str) {
        self.values.insert(key.to_string(), value.to_string());
    }

    pub fn get(&self, key: &str) -> Option<&str> {
        self.values.get(key).map(|s| s.as_str())
    }

    fn validate_key(key: &str) -> bool {
        !key.is_empty() && key.chars().all(|c| c.is_alphanumeric() || c == '_')
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_config_is_empty() {
        let config = Config::new();
        assert_eq!(config.get("anything"), None);
    }

    #[test]
    fn set_and_get() {
        let mut config = Config::new();
        config.set("host", "localhost");
        assert_eq!(config.get("host"), Some("localhost"));
    }

    #[test]
    fn validate_key_accepts_alphanumeric() {
        assert!(Config::validate_key("my_key_123"));
    }

    #[test]
    fn validate_key_rejects_empty() {
        assert!(!Config::validate_key(""));
    }

    #[test]
    fn validate_key_rejects_special_chars() {
        assert!(!Config::validate_key("my-key"));
        assert!(!Config::validate_key("my key"));
    }
}
```

```rust
// src/error.rs
use std::fmt;

#[derive(Debug, PartialEq)]
pub enum Error {
    ParseError(String),
    EvalError(String),
    ConfigError(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::ParseError(msg) => write!(f, "Parse error: {}", msg),
            Error::EvalError(msg) => write!(f, "Eval error: {}", msg),
            Error::ConfigError(msg) => write!(f, "Config error: {}", msg),
        }
    }
}

impl std::error::Error for Error {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_parse_error() {
        let err = Error::ParseError("unexpected token".to_string());
        assert_eq!(format!("{}", err), "Parse error: unexpected token");
    }

    #[test]
    fn display_eval_error() {
        let err = Error::EvalError("division by zero".to_string());
        assert_eq!(format!("{}", err), "Eval error: division by zero");
    }

    #[test]
    fn errors_are_comparable() {
        let e1 = Error::ParseError("x".to_string());
        let e2 = Error::ParseError("x".to_string());
        let e3 = Error::ParseError("y".to_string());
        assert_eq!(e1, e2);
        assert_ne!(e1, e3);
    }
}
```

### Ejecución selectiva por módulo

```bash
# Todos los unit tests
cargo test --lib

# Solo tests del módulo config
cargo test config::tests

# Solo tests del módulo error
cargo test error::tests

# Tests que contengan "parse" en cualquier módulo
cargo test parse

# Listar todos los tests sin ejecutarlos
cargo test --lib -- --list
```

Salida de `cargo test --lib`:

```
running 3 tests
test config::tests::new_config_is_empty ... ok
test config::tests::set_and_get ... ok
test config::tests::validate_key_accepts_alphanumeric ... ok

running 3 tests
test error::tests::display_parse_error ... ok
test error::tests::display_eval_error ... ok
test error::tests::errors_are_comparable ... ok

...
```

### El archivo lib.rs: ¿tests aquí también?

`lib.rs` puede tener sus propios tests, típicamente para probar re-exports o funciones de nivel raíz:

```rust
// src/lib.rs
pub mod config;
pub mod parser;

pub use config::Config;

/// Función de conveniencia en la raíz del crate
pub fn quick_parse(input: &str) -> Result<i32, String> {
    input.trim().parse::<i32>().map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn quick_parse_valid() {
        assert_eq!(quick_parse("42"), Ok(42));
    }

    #[test]
    fn quick_parse_with_whitespace() {
        assert_eq!(quick_parse("  42  "), Ok(42));
    }

    #[test]
    fn quick_parse_invalid() {
        assert!(quick_parse("abc").is_err());
    }

    #[test]
    fn config_reexport_works() {
        // Verificar que el re-export funciona
        let _ = Config::new();
    }
}
```

---

## 8. Organización en bibliotecas vs binarios

### Biblioteca (lib crate)

```
mi_lib/
├── Cargo.toml
├── src/
│   ├── lib.rs          // Punto de entrada de la biblioteca
│   ├── module_a.rs     // Módulo A con tests inline
│   └── module_b.rs     // Módulo B con tests inline
└── tests/
    └── api_test.rs     // Integration tests (prueban API pública)
```

```rust
// src/lib.rs
pub mod module_a;
pub mod module_b;

// Los unit tests prueban implementación interna
// Los integration tests (tests/) prueban la API pública
```

### Binario (bin crate)

```
mi_app/
├── Cargo.toml
├── src/
│   ├── main.rs         // Punto de entrada (mínimo)
│   └── lib.rs          // Lógica real (testeable)
└── tests/
    └── cli_test.rs     // Integration tests
```

El patrón recomendado es minimizar `main.rs` y poner la lógica en `lib.rs`:

```rust
// src/main.rs — Mínimo, difícil de testear directamente
use mi_app::Config;
use mi_app::run;

fn main() {
    let config = Config::from_args();
    if let Err(e) = run(&config) {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
```

```rust
// src/lib.rs — Toda la lógica, fácilmente testeable
pub struct Config {
    pub input: String,
    pub verbose: bool,
}

impl Config {
    pub fn from_args() -> Self {
        // parsing de argumentos
        Config {
            input: String::new(),
            verbose: false,
        }
    }
}

pub fn run(config: &Config) -> Result<(), Box<dyn std::error::Error>> {
    // lógica principal
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn run_with_default_config() {
        let config = Config {
            input: "test".to_string(),
            verbose: false,
        };
        assert!(run(&config).is_ok());
    }
}
```

### ¿Por qué no testear main.rs directamente?

```
┌─────────────────────────────────────────────────────────┐
│ main.rs es difícil de testear porque:                   │
│                                                         │
│ 1. fn main() no retorna un valor testeable              │
│ 2. process::exit() termina el proceso de test           │
│ 3. Los argumentos de CLI son globales                   │
│ 4. stdout/stderr son difíciles de capturar              │
│ 5. No se puede importar main() desde tests              │
│                                                         │
│ Solución:                                               │
│ main.rs = thin wrapper (parsing + delegación)           │
│ lib.rs  = lógica real (funciones puras, testeables)     │
└─────────────────────────────────────────────────────────┘
```

### Proyecto mixto (lib + bin)

```toml
# Cargo.toml
[package]
name = "mi_proyecto"
version = "0.1.0"
edition = "2021"

# Automáticamente detecta:
# src/lib.rs → biblioteca
# src/main.rs → binario
```

```
mi_proyecto/
├── src/
│   ├── lib.rs          // Biblioteca: use mi_proyecto::*
│   ├── main.rs         // Binario: use mi_proyecto::*
│   ├── core.rs         // Módulo con tests inline
│   └── utils.rs        // Módulo con tests inline
└── tests/
    └── integration.rs  // use mi_proyecto::*
```

```rust
// src/main.rs
// main.rs puede usar la biblioteca como cualquier otro consumidor
use mi_proyecto::run;

fn main() {
    run();
}
```

```bash
# Ejecutar tests de la biblioteca
cargo test --lib

# Ejecutar tests del binario (si main.rs tuviera #[test])
cargo test --bin mi_proyecto

# Ejecutar todo
cargo test
```

---

## 9. Imports condicionales con #[cfg(test)]

### Dependencias solo para tests

Rust permite declarar dependencias que solo se usan en tests en `Cargo.toml`:

```toml
# Cargo.toml
[package]
name = "mi_proyecto"
version = "0.1.0"
edition = "2021"

[dependencies]
serde = "1.0"          # Dependencia de producción

[dev-dependencies]
tempfile = "3.0"       # Solo en cargo test y cargo bench
pretty_assertions = "1.0"
```

```rust
// src/file_handler.rs

use std::fs;
use std::path::Path;

pub fn read_config(path: &Path) -> Result<String, std::io::Error> {
    fs::read_to_string(path)
}

pub fn write_config(path: &Path, content: &str) -> Result<(), std::io::Error> {
    fs::write(path, content)
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;   // dev-dependency: solo existe en tests
    use std::io::Write;

    #[test]
    fn read_config_from_file() {
        // tempfile crea un archivo temporal que se borra al salir del scope
        let mut file = NamedTempFile::new().unwrap();
        writeln!(file, "host=localhost").unwrap();

        let content = read_config(file.path()).unwrap();
        assert!(content.contains("host=localhost"));
    }

    #[test]
    fn write_and_read_roundtrip() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("config.txt");

        write_config(&path, "port=8080").unwrap();
        let content = read_config(&path).unwrap();

        assert_eq!(content, "port=8080");
    }
}
```

### Imports condicionales en el cuerpo del módulo

A veces necesitas imports que solo existen en tests, fuera del módulo `tests`:

```rust
// src/serializer.rs

use serde::Serialize;  // Dependencia de producción

// Import condicional: pretty_assertions solo en tests
#[cfg(test)]
use pretty_assertions::assert_eq;  // Reemplaza el assert_eq! estándar

pub fn serialize<T: Serialize>(value: &T) -> String {
    serde_json::to_string(value).unwrap()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn serialize_struct() {
        #[derive(Serialize)]
        struct Point { x: i32, y: i32 }

        let result = serialize(&Point { x: 1, y: 2 });
        // Si falla, pretty_assertions muestra un diff coloreado
        assert_eq!(result, r#"{"x":1,"y":2}"#);
    }
}
```

### #[cfg(test)] en bloques impl para soporte de testing

```rust
pub struct Database {
    data: Vec<Record>,
    connected: bool,
}

impl Database {
    pub fn new() -> Self {
        Database {
            data: Vec::new(),
            connected: false,
        }
    }

    pub fn connect(&mut self) -> Result<(), String> {
        // ... conexión real
        self.connected = true;
        Ok(())
    }

    pub fn insert(&mut self, record: Record) -> Result<(), String> {
        if !self.connected {
            return Err("Not connected".to_string());
        }
        self.data.push(record);
        Ok(())
    }
}

// Métodos que solo existen en tests
#[cfg(test)]
impl Database {
    /// Crea una Database pre-conectada con datos de prueba
    fn with_test_data(records: Vec<Record>) -> Self {
        Database {
            data: records,
            connected: true,  // Simular conexión sin IO real
        }
    }

    /// Inspeccionar estado interno (no expuesto en producción)
    fn record_count(&self) -> usize {
        self.data.len()
    }

    /// Verificar si contiene un record específico
    fn contains(&self, id: u64) -> bool {
        self.data.iter().any(|r| r.id == id)
    }
}

#[derive(Debug, PartialEq)]
pub struct Record {
    pub id: u64,
    pub value: String,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn with_test_data_creates_connected_db() {
        let db = Database::with_test_data(vec![
            Record { id: 1, value: "one".to_string() },
            Record { id: 2, value: "two".to_string() },
        ]);
        // Estos métodos solo existen en tests
        assert_eq!(db.record_count(), 2);
        assert!(db.contains(1));
        assert!(!db.contains(99));
    }

    #[test]
    fn insert_requires_connection() {
        let mut db = Database::new();
        let record = Record { id: 1, value: "test".to_string() };
        assert!(db.insert(record).is_err());
    }
}
```

### Patrón: trait condicional para testing

```rust
pub struct HttpClient {
    base_url: String,
}

impl HttpClient {
    pub fn new(base_url: &str) -> Self {
        HttpClient { base_url: base_url.to_string() }
    }

    pub fn get(&self, path: &str) -> Result<String, String> {
        // HTTP request real...
        Ok(format!("response from {}{}", self.base_url, path))
    }
}

// Trait que solo existe en tests para inspección
#[cfg(test)]
trait TestInspectable {
    fn base_url(&self) -> &str;
    fn is_local(&self) -> bool;
}

#[cfg(test)]
impl TestInspectable for HttpClient {
    fn base_url(&self) -> &str {
        &self.base_url
    }

    fn is_local(&self) -> bool {
        self.base_url.contains("localhost") || self.base_url.contains("127.0.0.1")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn client_base_url() {
        let client = HttpClient::new("http://localhost:8080");
        assert_eq!(client.base_url(), "http://localhost:8080");
        assert!(client.is_local());
    }
}
```

---

## 10. Helpers y utilidades de test

### Funciones helper dentro de mod tests

```rust
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

    pub fn set(&mut self, row: usize, col: usize, val: f64) {
        self.data[row][col] = val;
    }

    pub fn get(&self, row: usize, col: usize) -> f64 {
        self.data[row][col]
    }

    pub fn transpose(&self) -> Matrix {
        let mut result = Matrix::new(self.cols, self.rows);
        for i in 0..self.rows {
            for j in 0..self.cols {
                result.set(j, i, self.get(i, j));
            }
        }
        result
    }

    pub fn multiply(&self, other: &Matrix) -> Result<Matrix, String> {
        if self.cols != other.rows {
            return Err(format!(
                "Cannot multiply {}x{} by {}x{}",
                self.rows, self.cols, other.rows, other.cols
            ));
        }
        let mut result = Matrix::new(self.rows, other.cols);
        for i in 0..self.rows {
            for j in 0..other.cols {
                let mut sum = 0.0;
                for k in 0..self.cols {
                    sum += self.get(i, k) * other.get(k, j);
                }
                result.set(i, j, sum);
            }
        }
        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Helpers ─────────────────────────────────────────────

    const EPSILON: f64 = 1e-10;

    /// Crea una matriz desde una slice 2D
    fn matrix_from(data: &[&[f64]]) -> Matrix {
        let rows = data.len();
        let cols = if rows > 0 { data[0].len() } else { 0 };
        let mut m = Matrix::new(rows, cols);
        for (i, row) in data.iter().enumerate() {
            for (j, &val) in row.iter().enumerate() {
                m.set(i, j, val);
            }
        }
        m
    }

    /// Crea una matriz identidad nxn
    fn identity(n: usize) -> Matrix {
        let mut m = Matrix::new(n, n);
        for i in 0..n {
            m.set(i, i, 1.0);
        }
        m
    }

    /// Verifica que dos matrices son aproximadamente iguales
    fn assert_matrix_eq(a: &Matrix, b: &Matrix) {
        assert_eq!(a.rows, b.rows, "Row count differs");
        assert_eq!(a.cols, b.cols, "Col count differs");
        for i in 0..a.rows {
            for j in 0..a.cols {
                assert!(
                    (a.get(i, j) - b.get(i, j)).abs() < EPSILON,
                    "Matrices differ at [{},{}]: {} vs {}",
                    i, j, a.get(i, j), b.get(i, j)
                );
            }
        }
    }

    // ── Tests usando los helpers ────────────────────────────

    #[test]
    fn transpose_2x3() {
        let m = matrix_from(&[
            &[1.0, 2.0, 3.0],
            &[4.0, 5.0, 6.0],
        ]);
        let t = m.transpose();
        let expected = matrix_from(&[
            &[1.0, 4.0],
            &[2.0, 5.0],
            &[3.0, 6.0],
        ]);
        assert_matrix_eq(&t, &expected);
    }

    #[test]
    fn transpose_twice_is_identity_operation() {
        let m = matrix_from(&[
            &[1.0, 2.0],
            &[3.0, 4.0],
        ]);
        assert_matrix_eq(&m, &m.transpose().transpose());
    }

    #[test]
    fn multiply_by_identity() {
        let m = matrix_from(&[
            &[1.0, 2.0],
            &[3.0, 4.0],
        ]);
        let id = identity(2);
        let result = m.multiply(&id).unwrap();
        assert_matrix_eq(&m, &result);
    }

    #[test]
    fn multiply_dimension_mismatch() {
        let a = Matrix::new(2, 3);
        let b = Matrix::new(2, 2);
        assert!(a.multiply(&b).is_err());
    }
}
```

### Struct con Drop como fixture (RAII)

```rust
use std::fs;
use std::path::{Path, PathBuf};

pub fn process_file(path: &Path) -> Result<usize, std::io::Error> {
    let content = fs::read_to_string(path)?;
    Ok(content.lines().count())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    /// Fixture que crea un directorio temporal y lo limpia al salir del scope
    struct TestDir {
        path: PathBuf,
    }

    impl TestDir {
        fn new(name: &str) -> Self {
            let path = std::env::temp_dir().join(format!("rust_test_{}", name));
            fs::create_dir_all(&path).unwrap();
            TestDir { path }
        }

        fn file(&self, name: &str, content: &str) -> PathBuf {
            let file_path = self.path.join(name);
            let mut f = fs::File::create(&file_path).unwrap();
            write!(f, "{}", content).unwrap();
            file_path
        }

        fn path(&self) -> &Path {
            &self.path
        }
    }

    impl Drop for TestDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.path);
        }
    }

    #[test]
    fn process_file_counts_lines() {
        let dir = TestDir::new("count_lines");
        let file = dir.file("test.txt", "line1\nline2\nline3\n");

        assert_eq!(process_file(&file).unwrap(), 3);
        // dir se destruye aquí → directorio temporal eliminado
    }

    #[test]
    fn process_file_empty() {
        let dir = TestDir::new("empty_file");
        let file = dir.file("empty.txt", "");

        assert_eq!(process_file(&file).unwrap(), 0);
    }

    #[test]
    fn process_file_not_found() {
        let result = process_file(Path::new("/nonexistent/path"));
        assert!(result.is_err());
    }
}
```

---

## 11. Fixtures y datos de prueba

### Constantes como datos de prueba

```rust
pub fn validate_email(email: &str) -> bool {
    let parts: Vec<&str> = email.split('@').collect();
    if parts.len() != 2 {
        return false;
    }
    let (local, domain) = (parts[0], parts[1]);
    !local.is_empty() && !domain.is_empty() && domain.contains('.')
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Datos de prueba como constantes ─────────────────────

    const VALID_EMAILS: &[&str] = &[
        "user@example.com",
        "first.last@domain.org",
        "user+tag@sub.domain.co",
        "a@b.c",
    ];

    const INVALID_EMAILS: &[&str] = &[
        "",
        "nodomain",
        "@nolocal.com",
        "noat",
        "user@",
        "user@nodot",
        "user@@double.com",
        "user@do@main.com",
    ];

    #[test]
    fn valid_emails_pass() {
        for email in VALID_EMAILS {
            assert!(
                validate_email(email),
                "'{}' should be valid", email
            );
        }
    }

    #[test]
    fn invalid_emails_fail() {
        for email in INVALID_EMAILS {
            assert!(
                !validate_email(email),
                "'{}' should be invalid", email
            );
        }
    }
}
```

### Builder pattern para datos complejos

```rust
pub struct User {
    pub name: String,
    pub email: String,
    pub age: u32,
    pub active: bool,
    pub roles: Vec<String>,
}

impl User {
    pub fn is_admin(&self) -> bool {
        self.roles.contains(&"admin".to_string())
    }

    pub fn can_login(&self) -> bool {
        self.active && self.age >= 18
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Builder solo para tests
    struct UserBuilder {
        name: String,
        email: String,
        age: u32,
        active: bool,
        roles: Vec<String>,
    }

    impl UserBuilder {
        fn new() -> Self {
            UserBuilder {
                name: "Test User".to_string(),
                email: "test@example.com".to_string(),
                age: 25,
                active: true,
                roles: vec![],
            }
        }

        fn name(mut self, name: &str) -> Self {
            self.name = name.to_string();
            self
        }

        fn age(mut self, age: u32) -> Self {
            self.age = age;
            self
        }

        fn active(mut self, active: bool) -> Self {
            self.active = active;
            self
        }

        fn role(mut self, role: &str) -> Self {
            self.roles.push(role.to_string());
            self
        }

        fn build(self) -> User {
            User {
                name: self.name,
                email: self.email,
                age: self.age,
                active: self.active,
                roles: self.roles,
            }
        }
    }

    #[test]
    fn default_user_can_login() {
        let user = UserBuilder::new().build();
        assert!(user.can_login());
    }

    #[test]
    fn minor_cannot_login() {
        let user = UserBuilder::new().age(16).build();
        assert!(!user.can_login());
    }

    #[test]
    fn inactive_cannot_login() {
        let user = UserBuilder::new().active(false).build();
        assert!(!user.can_login());
    }

    #[test]
    fn admin_role() {
        let user = UserBuilder::new().role("admin").build();
        assert!(user.is_admin());
    }

    #[test]
    fn no_roles_not_admin() {
        let user = UserBuilder::new().build();
        assert!(!user.is_admin());
    }

    #[test]
    fn other_role_not_admin() {
        let user = UserBuilder::new().role("editor").build();
        assert!(!user.is_admin());
    }
}
```

### Archivos de datos como fixtures

Para tests que requieren datos externos (JSON, CSV, etc.):

```
mi_proyecto/
├── src/
│   └── parser.rs
└── tests/
    └── fixtures/
        ├── valid_input.json
        ├── malformed.json
        └── large_dataset.csv
```

```rust
// src/parser.rs
use std::path::Path;

pub fn parse_json_file(path: &Path) -> Result<serde_json::Value, String> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| format!("Cannot read file: {}", e))?;
    serde_json::from_str(&content)
        .map_err(|e| format!("Invalid JSON: {}", e))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    /// Retorna el path al directorio de fixtures
    fn fixture_path(name: &str) -> PathBuf {
        // CARGO_MANIFEST_DIR apunta a la raíz del proyecto
        let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        path.push("tests");
        path.push("fixtures");
        path.push(name);
        path
    }

    #[test]
    fn parse_valid_json() {
        let path = fixture_path("valid_input.json");
        let result = parse_json_file(&path);
        assert!(result.is_ok());
    }

    #[test]
    fn parse_malformed_json() {
        let path = fixture_path("malformed.json");
        let result = parse_json_file(&path);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Invalid JSON"));
    }
}
```

### include_str! y include_bytes! para datos embebidos

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // Incluir contenido de archivo en tiempo de compilación
    const TEST_CSV: &str = include_str!("../tests/fixtures/data.csv");
    const TEST_BINARY: &[u8] = include_bytes!("../tests/fixtures/image.png");

    #[test]
    fn parse_embedded_csv() {
        let lines: Vec<&str> = TEST_CSV.lines().collect();
        assert!(lines.len() > 0);
    }
}
```

Nota: `include_str!` y `include_bytes!` embeben el contenido en el binario de test. Para archivos grandes, es mejor leerlos en runtime con `fixture_path()`.

---

## 12. Test modules anidados

### Submódulos dentro de tests

Para organizar muchos tests, se pueden crear submódulos dentro de `mod tests`:

```rust
pub struct Calculator {
    memory: f64,
}

impl Calculator {
    pub fn new() -> Self {
        Calculator { memory: 0.0 }
    }

    pub fn add(&self, a: f64, b: f64) -> f64 { a + b }
    pub fn subtract(&self, a: f64, b: f64) -> f64 { a - b }
    pub fn multiply(&self, a: f64, b: f64) -> f64 { a * b }
    pub fn divide(&self, a: f64, b: f64) -> Result<f64, String> {
        if b == 0.0 {
            Err("Division by zero".to_string())
        } else {
            Ok(a / b)
        }
    }

    pub fn store(&mut self, value: f64) { self.memory = value; }
    pub fn recall(&self) -> f64 { self.memory }
    pub fn clear(&mut self) { self.memory = 0.0; }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Submódulo para operaciones aritméticas
    mod arithmetic {
        use super::*;

        #[test]
        fn add_positive() {
            let calc = Calculator::new();
            assert_eq!(calc.add(2.0, 3.0), 5.0);
        }

        #[test]
        fn add_negative() {
            let calc = Calculator::new();
            assert_eq!(calc.add(-1.0, -2.0), -3.0);
        }

        #[test]
        fn subtract_basic() {
            let calc = Calculator::new();
            assert_eq!(calc.subtract(5.0, 3.0), 2.0);
        }

        #[test]
        fn multiply_by_zero() {
            let calc = Calculator::new();
            assert_eq!(calc.multiply(42.0, 0.0), 0.0);
        }

        #[test]
        fn divide_basic() {
            let calc = Calculator::new();
            assert_eq!(calc.divide(10.0, 2.0).unwrap(), 5.0);
        }

        #[test]
        fn divide_by_zero() {
            let calc = Calculator::new();
            assert!(calc.divide(1.0, 0.0).is_err());
        }
    }

    // Submódulo para operaciones de memoria
    mod memory {
        use super::*;

        #[test]
        fn initial_memory_is_zero() {
            let calc = Calculator::new();
            assert_eq!(calc.recall(), 0.0);
        }

        #[test]
        fn store_and_recall() {
            let mut calc = Calculator::new();
            calc.store(42.0);
            assert_eq!(calc.recall(), 42.0);
        }

        #[test]
        fn clear_resets_memory() {
            let mut calc = Calculator::new();
            calc.store(42.0);
            calc.clear();
            assert_eq!(calc.recall(), 0.0);
        }

        #[test]
        fn store_overwrites() {
            let mut calc = Calculator::new();
            calc.store(1.0);
            calc.store(2.0);
            assert_eq!(calc.recall(), 2.0);
        }
    }
}
```

### Ejecución selectiva con submódulos

```bash
# Todos los tests
cargo test

# Solo tests de arithmetic
cargo test tests::arithmetic

# Solo tests de memory
cargo test tests::memory

# Test específico
cargo test tests::arithmetic::divide_by_zero

# Listar estructura
cargo test -- --list
```

Salida de `cargo test -- --list`:

```
calculator::tests::arithmetic::add_positive: test
calculator::tests::arithmetic::add_negative: test
calculator::tests::arithmetic::subtract_basic: test
calculator::tests::arithmetic::multiply_by_zero: test
calculator::tests::arithmetic::divide_basic: test
calculator::tests::arithmetic::divide_by_zero: test
calculator::tests::memory::initial_memory_is_zero: test
calculator::tests::memory::store_and_recall: test
calculator::tests::memory::clear_resets_memory: test
calculator::tests::memory::store_overwrites: test
```

### use super::* en submódulos anidados

Cuando se anidan submódulos, `super` se refiere al padre inmediato:

```
calculator.rs
└── mod tests
    ├── use super::*  →  importa de calculator.rs
    │
    ├── mod arithmetic
    │   └── use super::*  →  importa de mod tests
    │       (que incluye todo de calculator.rs via re-export)
    │
    └── mod memory
        └── use super::*  →  importa de mod tests
```

El `use super::*` dentro de `mod arithmetic` importa todo lo que `mod tests` tiene en scope, que a su vez importó todo de `calculator` con su propio `use super::*`. Es transitivo.

### ¿Cuándo usar submódulos?

```
┌──────────────────────────────────────────────────────────┐
│ Submódulos en tests: cuándo sí, cuándo no               │
├──────────────────────┬───────────────────────────────────┤
│ Usar cuando          │ No usar cuando                    │
├──────────────────────┼───────────────────────────────────┤
│ > 15-20 tests        │ < 10 tests                        │
│ Grupos lógicos       │ Tests misceláneos                 │
│  claros              │                                   │
│ Quieres ejecutar     │ Siempre ejecutas todo             │
│  grupos selectivos   │                                   │
│ Diferentes categorías│ Todos prueban lo mismo            │
│  (happy path, edge   │                                   │
│  cases, errors)      │                                   │
│ Documentar la        │ Los nombres de test son           │
│  estructura de tests │  suficientes                      │
└──────────────────────┴───────────────────────────────────┘
```

---

## 13. Visibilidad y pub(crate) para testing

### El acceso privilegiado de tests en Rust

En Rust, los unit tests (dentro de `mod tests` en el mismo archivo) tienen acceso a funciones privadas del módulo padre. Esto es una decisión de diseño deliberada:

```rust
// src/crypto.rs

/// API pública
pub fn hash(data: &[u8]) -> Vec<u8> {
    let padded = pad_to_block_size(data);
    let rounds = compute_rounds(&padded);
    finalize(&rounds)
}

/// Funciones privadas — NO accesibles desde fuera del módulo
fn pad_to_block_size(data: &[u8]) -> Vec<u8> {
    let block_size = 64;
    let mut padded = data.to_vec();
    let padding_needed = block_size - (padded.len() % block_size);
    padded.extend(vec![0u8; padding_needed]);
    padded
}

fn compute_rounds(data: &[u8]) -> Vec<u8> {
    // ... transformaciones internas
    data.to_vec()
}

fn finalize(data: &[u8]) -> Vec<u8> {
    data.to_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    // ✓ Podemos testear cada función privada individualmente
    #[test]
    fn pad_to_block_size_exact_block() {
        let data = vec![0u8; 64];
        let padded = pad_to_block_size(&data);
        assert_eq!(padded.len() % 64, 0);
    }

    #[test]
    fn pad_to_block_size_partial() {
        let data = vec![0u8; 10];
        let padded = pad_to_block_size(&data);
        assert_eq!(padded.len(), 64); // Se rellena hasta 64
    }

    #[test]
    fn hash_produces_output() {
        let result = hash(b"hello");
        assert!(!result.is_empty());
    }
}
```

### ¿Por qué Rust permite testear funciones privadas?

```
┌──────────────────────────────────────────────────────────┐
│ Filosofía de Rust sobre tests de funciones privadas:     │
│                                                          │
│ 1. Los unit tests prueban la IMPLEMENTACIÓN              │
│    → Las funciones privadas SON implementación           │
│    → Tiene sentido probarlas directamente                │
│                                                          │
│ 2. Los integration tests prueban la API PÚBLICA          │
│    → Viven en tests/ (crate separado)                    │
│    → Solo ven pub fn                                     │
│                                                          │
│ 3. El módulo tests es HIJO del módulo que prueba         │
│    → En Rust, los hijos ven todo del padre               │
│    → Es acceso legítimo, no un hack                      │
│                                                          │
│ Contraste con C:                                         │
│ En C, probar funciones static requiere hacks:            │
│    #include "file.c"  (incluir implementación)           │
│    Eliminar static con macros                            │
│    Exponer con funciones wrapper                         │
└──────────────────────────────────────────────────────────┘
```

### pub(crate): visibilidad intermedia

Cuando un módulo necesita exponer funciones a otros módulos del mismo crate (pero no a usuarios externos), se usa `pub(crate)`:

```rust
// src/parser.rs

/// API pública: visible para todos
pub fn parse(input: &str) -> Result<Ast, ParseError> {
    let tokens = tokenize(input)?;
    parse_expression(&tokens, 0).map(|(ast, _)| ast)
}

/// pub(crate): visible dentro del crate pero no para usuarios externos
pub(crate) fn tokenize(input: &str) -> Result<Vec<Token>, ParseError> {
    // ... puede ser llamado por otros módulos del crate
    // ... pero no por código que haga `use mi_crate::parser::tokenize`
    todo!()
}

/// Privada: solo visible en parser.rs (y sus tests)
fn parse_expression(tokens: &[Token], pos: usize) -> Result<(Ast, usize), ParseError> {
    todo!()
}
```

```
Niveles de visibilidad y quién puede testear qué:

┌────────────────┬────────────┬──────────────┬──────────────┐
│ Visibilidad    │ Unit tests │ Otros módulos│ Integration  │
│                │ (inline)   │ del crate    │ tests (ext.) │
├────────────────┼────────────┼──────────────┼──────────────┤
│ fn private()   │     ✓      │      ✗       │      ✗       │
│ pub(crate) fn  │     ✓      │      ✓       │      ✗       │
│ pub(super) fn  │     ✓      │  Solo padre  │      ✗       │
│ pub fn         │     ✓      │      ✓       │      ✓       │
└────────────────┴────────────┴──────────────┴──────────────┘
```

### pub(crate) como herramienta de diseño para testing

A veces se usa `pub(crate)` específicamente para permitir testing desde otros módulos:

```rust
// src/engine.rs
pub(crate) struct EngineState {
    pub(crate) phase: Phase,
    pub(crate) cycle_count: u64,
}

pub(crate) enum Phase {
    Init,
    Running,
    Shutdown,
}

pub struct Engine {
    state: EngineState,
}

impl Engine {
    pub fn new() -> Self {
        Engine {
            state: EngineState {
                phase: Phase::Init,
                cycle_count: 0,
            },
        }
    }

    pub fn run(&mut self) {
        self.state.phase = Phase::Running;
        self.state.cycle_count += 1;
    }

    // pub(crate): otros módulos del crate pueden inspeccionar
    pub(crate) fn state(&self) -> &EngineState {
        &self.state
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn engine_starts_in_init() {
        let engine = Engine::new();
        assert!(matches!(engine.state().phase, Phase::Init));
    }

    #[test]
    fn run_transitions_to_running() {
        let mut engine = Engine::new();
        engine.run();
        assert!(matches!(engine.state().phase, Phase::Running));
        assert_eq!(engine.state().cycle_count, 1);
    }
}
```

```rust
// src/monitor.rs — Otro módulo del crate puede acceder a pub(crate)
use crate::engine::{Engine, Phase};

pub fn is_engine_active(engine: &Engine) -> bool {
    matches!(engine.state().phase, Phase::Running)
    //       ^^^^^^^^ pub(crate) → accesible desde monitor
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn monitor_detects_running() {
        let mut engine = Engine::new();
        assert!(!is_engine_active(&engine));
        engine.run();
        assert!(is_engine_active(&engine));
    }
}
```

---

## 14. Organización de tests según tipo

### Clasificación de tests por propósito

```
┌─────────────────────────────────────────────────────────────┐
│              Dónde va cada tipo de test                      │
├─────────────────┬──────────────────┬────────────────────────┤
│ Tipo            │ Ubicación        │ Qué prueba             │
├─────────────────┼──────────────────┼────────────────────────┤
│ Unit test       │ src/module.rs    │ Una función/método     │
│ (funcionalidad) │ mod tests { }   │ individualmente        │
├─────────────────┼──────────────────┼────────────────────────┤
│ Unit test       │ src/module.rs    │ Inputs extremos,       │
│ (edge cases)    │ mod tests { }   │ valores límite         │
├─────────────────┼──────────────────┼────────────────────────┤
│ Unit test       │ src/module.rs    │ Errores esperados,     │
│ (errores)       │ mod tests { }   │ inputs inválidos       │
├─────────────────┼──────────────────┼────────────────────────┤
│ Integration     │ tests/*.rs       │ Módulos colaborando,   │
│ test            │                  │ API pública            │
├─────────────────┼──────────────────┼────────────────────────┤
│ Doc test        │ /// ``` en       │ Que los ejemplos de    │
│                 │ src/module.rs    │ docs compilen y pasen  │
├─────────────────┼──────────────────┼────────────────────────┤
│ Benchmark       │ benches/*.rs     │ Rendimiento            │
├─────────────────┼──────────────────┼────────────────────────┤
│ Property test   │ src/module.rs    │ Invariantes con        │
│                 │ mod tests { }   │ inputs aleatorios       │
└─────────────────┴──────────────────┴────────────────────────┘
```

### Ejemplo: un módulo con tests organizados por tipo

```rust
// src/sorted_vec.rs

/// Vector que mantiene sus elementos ordenados
pub struct SortedVec<T: Ord> {
    data: Vec<T>,
}

impl<T: Ord> SortedVec<T> {
    pub fn new() -> Self {
        SortedVec { data: Vec::new() }
    }

    pub fn insert(&mut self, value: T) {
        let pos = self.data.binary_search(&value).unwrap_or_else(|p| p);
        self.data.insert(pos, value);
    }

    pub fn contains(&self, value: &T) -> bool {
        self.data.binary_search(value).is_ok()
    }

    pub fn remove(&mut self, value: &T) -> bool {
        if let Ok(pos) = self.data.binary_search(value) {
            self.data.remove(pos);
            true
        } else {
            false
        }
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn as_slice(&self) -> &[T] {
        &self.data
    }

    pub fn min(&self) -> Option<&T> {
        self.data.first()
    }

    pub fn max(&self) -> Option<&T> {
        self.data.last()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ═══════════════════════════════════════════════
    // Funcionalidad básica
    // ═══════════════════════════════════════════════

    #[test]
    fn new_is_empty() {
        let sv: SortedVec<i32> = SortedVec::new();
        assert!(sv.is_empty());
        assert_eq!(sv.len(), 0);
    }

    #[test]
    fn insert_maintains_order() {
        let mut sv = SortedVec::new();
        sv.insert(3);
        sv.insert(1);
        sv.insert(2);
        assert_eq!(sv.as_slice(), &[1, 2, 3]);
    }

    #[test]
    fn contains_finds_element() {
        let mut sv = SortedVec::new();
        sv.insert(42);
        assert!(sv.contains(&42));
    }

    #[test]
    fn contains_does_not_find_missing() {
        let mut sv = SortedVec::new();
        sv.insert(1);
        assert!(!sv.contains(&99));
    }

    #[test]
    fn remove_existing() {
        let mut sv = SortedVec::new();
        sv.insert(1);
        sv.insert(2);
        sv.insert(3);
        assert!(sv.remove(&2));
        assert_eq!(sv.as_slice(), &[1, 3]);
    }

    #[test]
    fn remove_nonexistent() {
        let mut sv = SortedVec::new();
        sv.insert(1);
        assert!(!sv.remove(&99));
    }

    // ═══════════════════════════════════════════════
    // Casos extremos (edge cases)
    // ═══════════════════════════════════════════════

    #[test]
    fn insert_duplicates() {
        let mut sv = SortedVec::new();
        sv.insert(1);
        sv.insert(1);
        sv.insert(1);
        assert_eq!(sv.len(), 3);
        assert_eq!(sv.as_slice(), &[1, 1, 1]);
    }

    #[test]
    fn insert_in_descending_order() {
        let mut sv = SortedVec::new();
        for i in (0..10).rev() {
            sv.insert(i);
        }
        let expected: Vec<i32> = (0..10).collect();
        assert_eq!(sv.as_slice(), expected.as_slice());
    }

    #[test]
    fn insert_already_sorted() {
        let mut sv = SortedVec::new();
        for i in 0..10 {
            sv.insert(i);
        }
        let expected: Vec<i32> = (0..10).collect();
        assert_eq!(sv.as_slice(), expected.as_slice());
    }

    #[test]
    fn single_element() {
        let mut sv = SortedVec::new();
        sv.insert(42);
        assert_eq!(sv.min(), Some(&42));
        assert_eq!(sv.max(), Some(&42));
        assert_eq!(sv.len(), 1);
    }

    // ═══════════════════════════════════════════════
    // Operaciones con empty
    // ═══════════════════════════════════════════════

    #[test]
    fn min_of_empty() {
        let sv: SortedVec<i32> = SortedVec::new();
        assert_eq!(sv.min(), None);
    }

    #[test]
    fn max_of_empty() {
        let sv: SortedVec<i32> = SortedVec::new();
        assert_eq!(sv.max(), None);
    }

    #[test]
    fn remove_from_empty() {
        let mut sv: SortedVec<i32> = SortedVec::new();
        assert!(!sv.remove(&1));
    }

    #[test]
    fn contains_in_empty() {
        let sv: SortedVec<i32> = SortedVec::new();
        assert!(!sv.contains(&1));
    }

    // ═══════════════════════════════════════════════
    // Min y Max
    // ═══════════════════════════════════════════════

    #[test]
    fn min_after_insertions() {
        let mut sv = SortedVec::new();
        sv.insert(5);
        sv.insert(3);
        sv.insert(7);
        assert_eq!(sv.min(), Some(&3));
    }

    #[test]
    fn max_after_insertions() {
        let mut sv = SortedVec::new();
        sv.insert(5);
        sv.insert(3);
        sv.insert(7);
        assert_eq!(sv.max(), Some(&7));
    }

    // ═══════════════════════════════════════════════
    // Tipos genéricos
    // ═══════════════════════════════════════════════

    #[test]
    fn works_with_strings() {
        let mut sv = SortedVec::new();
        sv.insert("banana".to_string());
        sv.insert("apple".to_string());
        sv.insert("cherry".to_string());
        assert_eq!(
            sv.as_slice(),
            &["apple".to_string(), "banana".to_string(), "cherry".to_string()]
        );
    }

    #[test]
    fn works_with_chars() {
        let mut sv = SortedVec::new();
        sv.insert('c');
        sv.insert('a');
        sv.insert('b');
        assert_eq!(sv.as_slice(), &['a', 'b', 'c']);
    }
}
```

---

## 15. Patrones de nombrado

### Convenciones de nombre para funciones test

```
┌────────────────────────────────────────────────────────────┐
│              Patrones de nombrado de tests                  │
├──────────────────┬─────────────────────────────────────────┤
│ Patrón           │ Ejemplo                                 │
├──────────────────┼─────────────────────────────────────────┤
│ accion_condicion │ insert_maintains_order                  │
│                  │ parse_returns_error_on_empty            │
│                  │ divide_by_zero_returns_err              │
├──────────────────┼─────────────────────────────────────────┤
│ sujeto_accion    │ empty_vec_has_zero_len                  │
│                  │ sorted_vec_preserves_order              │
│                  │ config_loads_from_file                  │
├──────────────────┼─────────────────────────────────────────┤
│ when_then        │ when_empty_returns_none                 │
│                  │ when_duplicate_preserves_both           │
├──────────────────┼─────────────────────────────────────────┤
│ should_behavior  │ should_reject_negative                  │
│                  │ should_sort_on_insert                   │
├──────────────────┼─────────────────────────────────────────┤
│ descriptivo      │ two_plus_two_equals_four                │
│ literal          │ empty_string_is_not_valid_email         │
└──────────────────┴─────────────────────────────────────────┘
```

### Lo que importa en los nombres

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // ✗ MALO: no dice qué verifica
    #[test]
    fn test1() { }

    // ✗ MALO: repite "test" (ya está en un módulo tests)
    #[test]
    fn test_add() { }

    // ✗ MALO: demasiado genérico
    #[test]
    fn it_works() { }

    // ✓ BUENO: describe el escenario y resultado esperado
    #[test]
    fn add_two_positive_numbers() { }

    // ✓ BUENO: describe el edge case
    #[test]
    fn add_with_overflow_wraps() { }

    // ✓ BUENO: describe el caso de error
    #[test]
    fn parse_empty_string_returns_error() { }
}
```

### Nombres largos son aceptables

En Rust (y en testing en general), los nombres de test pueden ser largos. La claridad es más importante que la brevedad:

```rust
#[test]
fn removing_last_element_from_sorted_vec_updates_max() {
    let mut sv = SortedVec::new();
    sv.insert(1);
    sv.insert(5);
    sv.insert(3);
    sv.remove(&5);
    assert_eq!(sv.max(), Some(&3));
}
```

Cuando falla, el nombre largo actúa como documentación:

```
test sorted_vec::tests::removing_last_element_from_sorted_vec_updates_max ... FAILED
```

Sabes exactamente qué escenario falló sin leer el código del test.

---

## 16. Comparación con C y Go

### C: separación obligatoria

```c
/* En C, no hay forma estándar de incluir tests en el mismo archivo */

/* === src/stack.c === */
#include "stack.h"
#include <stdlib.h>

struct Stack {
    int *data;
    size_t top;
    size_t capacity;
};

Stack *stack_new(size_t cap) {
    Stack *s = malloc(sizeof(Stack));
    s->data = malloc(cap * sizeof(int));
    s->top = 0;
    s->capacity = cap;
    return s;
}

/* === tests/test_stack.c === (archivo separado obligatorio) */
#include "unity.h"
#include "../src/stack.h"

void test_stack_new(void) {
    Stack *s = stack_new(10);
    TEST_ASSERT_NOT_NULL(s);
    stack_free(s);
}

/* Para probar funciones static:
   - Hack: #include "../src/stack.c"  (incluir .c, no .h)
   - O: eliminar static
   - O: wrapper functions
*/
```

### Go: convención de archivo separado (mismo paquete)

```go
// stack.go
package stack

type Stack struct {
    data []int
}

func New() *Stack {
    return &Stack{data: make([]int, 0)}
}

func (s *Stack) Push(val int) {
    s.data = append(s.data, val)
}

func (s *Stack) isEmpty() bool {  // minúscula = privada al paquete
    return len(s.data) == 0
}
```

```go
// stack_test.go — mismo directorio, mismo paquete
package stack

import "testing"

func TestNew(t *testing.T) {
    s := New()
    if !s.isEmpty() {  // Acceso a función privada (mismo paquete)
        t.Error("new stack should be empty")
    }
}
```

### Tabla comparativa completa

```
┌──────────────────┬──────────────────┬──────────────────┬──────────────────┐
│ Aspecto          │ C                │ Go               │ Rust             │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Ubicación tests  │ Archivo separado │ Archivo separado │ Mismo archivo    │
│                  │ (tests/test_*.c) │ (*_test.go)      │ (mod tests)      │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Acceso privado   │ Hack: incluir .c │ Mismo paquete:   │ use super::*     │
│                  │ o quitar static  │ sí. Otro: _test  │ (acceso total)   │
│                  │                  │ con package x    │                  │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Compilación      │ Manual (Make)    │ `go test`        │ `cargo test`     │
│ condicional      │ Ifdefs           │ Archivos *_test  │ #[cfg(test)]     │
│                  │                  │ excluidos auto.  │ excluye de build │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Framework        │ Externo (Unity,  │ Built-in         │ Built-in         │
│                  │ Check, cmocka)   │ testing package  │ #[test] + assert │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Organización     │ Por directorio   │ Por archivo      │ Por módulo       │
│                  │ (tests/)         │ (*_test.go)      │ (mod tests {})   │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Integration tests│ Manual           │ TestMain +       │ tests/ directorio│
│                  │                  │ build tags       │ (crate separado) │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Separar tests    │ Siempre separado │ Siempre separado │ Opcional: archivo│
│ de producción    │ (no hay opción)  │ (*_test.go)      │ tests.rs separado│
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Namespacing      │ Prefijos manuales│ func Test...     │ mod::tests::name │
│                  │ test_stack_push  │ por convención   │ automático       │
├──────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Binario de prod. │ No incluye tests │ No incluye tests │ No incluye tests │
│                  │ (compilación sep)│ (*_test excluido) │ (cfg(test) false)│
└──────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

### Equivalencia de patrones

```
C:                          Go:                         Rust:
tests/test_foo.c            foo_test.go                 // En foo.rs:
#include "../src/foo.c"     package foo                 #[cfg(test)]
// (hack para privadas)     // mismo paquete = privadas mod tests {
                                                            use super::*;
void test_bar(void) {       func TestBar(t *testing.T){ 
    TEST_ASSERT_EQUAL(      if got != want {                #[test]
        expected, actual);      t.Errorf(...)               fn bar() {
}                           }                                   assert_eq!(...);
                                                            }
                                                        }
```

---

## 17. Errores comunes

### Error 1: Olvidar #[cfg(test)]

```rust
// ✗ INCORRECTO
mod tests {
    use super::*;
    #[test]
    fn it_works() { assert!(true); }
}
```

**Síntoma**: Warning `function is never used` en `cargo build`. El módulo tests y sus imports se compilan en producción.

**Solución**:

```rust
// ✓ CORRECTO
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn it_works() { assert!(true); }
}
```

### Error 2: Doble #[cfg(test)] en archivo separado

```rust
// src/parser.rs
#[cfg(test)]
mod tests;

// src/parser/tests.rs
#[cfg(test)]  // ← REDUNDANTE y confuso
mod tests {   // ← INCORRECTO: crea tests::tests
    use super::*;
    // super ahora es parser::tests, no parser
}
```

**Síntoma**: `use super::*` no importa las funciones del parser. Error de compilación: `cannot find function`.

**Solución**:

```rust
// src/parser/tests.rs — SIN wrapping adicional
use super::*;

#[test]
fn test_parse() {
    // Parser está en scope correctamente
}
```

### Error 3: Múltiples mod tests en un archivo

```rust
// ✗ INCORRECTO
pub fn add(a: i32, b: i32) -> i32 { a + b }

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn add_works() { assert_eq!(add(1, 2), 3); }
}

pub fn multiply(a: i32, b: i32) -> i32 { a * b }

#[cfg(test)]
mod tests {  // ← ERROR: nombre duplicado
    use super::*;
    #[test]
    fn multiply_works() { assert_eq!(multiply(2, 3), 6); }
}
```

**Síntoma**: `error[E0428]: the name 'tests' is defined multiple times`

**Solución**: Un solo `mod tests` al final del archivo:

```rust
pub fn add(a: i32, b: i32) -> i32 { a + b }
pub fn multiply(a: i32, b: i32) -> i32 { a * b }

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn add_works() { assert_eq!(add(1, 2), 3); }

    #[test]
    fn multiply_works() { assert_eq!(multiply(2, 3), 6); }
}
```

### Error 4: Usar dev-dependencies sin #[cfg(test)]

```rust
// ✗ INCORRECTO
use tempfile::NamedTempFile;  // ← Error en cargo build

pub fn process() { /* ... */ }

#[cfg(test)]
mod tests {
    use super::*;
    // tempfile es dev-dependency, no existe en cargo build
}
```

**Síntoma**: `error[E0432]: unresolved import 'tempfile'` en `cargo build` (no en `cargo test`).

**Solución**:

```rust
pub fn process() { /* ... */ }

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;  // ← Dentro de cfg(test)

    #[test]
    fn test_process() {
        let tmp = NamedTempFile::new().unwrap();
        // ...
    }
}
```

### Error 5: Tests en main.rs sin lib.rs

```rust
// src/main.rs — difícil de testear
fn calculate(x: i32) -> i32 {
    x * 2
}

fn main() {
    println!("{}", calculate(21));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_calculate() {
        assert_eq!(calculate(21), 42);
    }
}
```

Esto funciona técnicamente, pero tiene limitaciones:

```
┌────────────────────────────────────────────────────────────┐
│ Problemas de tests en main.rs:                             │
│                                                            │
│ 1. Integration tests (tests/) no pueden importar main.rs  │
│ 2. La lógica no es reutilizable como biblioteca            │
│ 3. No se puede hacer `use mi_crate::calculate` desde      │
│    otros crates                                            │
│ 4. Mezcla lógica de entrada (CLI) con lógica de negocio    │
│                                                            │
│ Solución: mover lógica a lib.rs, main.rs solo delega       │
└────────────────────────────────────────────────────────────┘
```

### Error 6: Confundir unit tests e integration tests

```rust
// ✗ INCORRECTO: integration test en src/
// src/lib.rs
pub mod parser;

#[cfg(test)]
mod tests {
    // Esto NO es un integration test aunque pruebe múltiples módulos
    use crate::parser::Parser;

    #[test]
    fn full_pipeline() {
        // Este test tiene acceso a internos de todos los módulos
        // Es un unit test que prueba la interacción
    }
}
```

```rust
// ✓ Integration test real: en tests/
// tests/full_pipeline.rs
use mi_crate::Parser;  // Solo API pública

#[test]
fn full_pipeline() {
    // Este test SOLO ve la API pública
    // Es un verdadero integration test
}
```

### Error 7: Olvidar use super::* con imports selectivos

```rust
#[cfg(test)]
mod tests {
    // ✗ Olvidó importar
    #[test]
    fn test_add() {
        assert_eq!(add(1, 2), 3);  // error: cannot find function `add`
    }
}
```

**Solución**: Siempre incluir `use super::*;` o imports selectivos.

---

## 18. Ejemplo completo: biblioteca de colecciones

Una biblioteca con múltiples módulos, cada uno con tests organizados correctamente:

```toml
# Cargo.toml
[package]
name = "collections"
version = "0.1.0"
edition = "2021"

[dev-dependencies]
# Sin dependencias externas de test para este ejemplo
```

```rust
// src/lib.rs
pub mod stack;
pub mod queue;
pub mod deque;

// Re-exports
pub use stack::Stack;
pub use queue::Queue;
pub use deque::Deque;

/// Trait común para todas las colecciones
pub trait Collection {
    type Item;

    fn len(&self) -> usize;
    fn is_empty(&self) -> bool {
        self.len() == 0
    }
    fn clear(&mut self);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reexports_are_accessible() {
        let _stack: Stack<i32> = Stack::new();
        let _queue: Queue<i32> = Queue::new();
        let _deque: Deque<i32> = Deque::new();
    }
}
```

```rust
// src/stack.rs
use crate::Collection;

/// Stack LIFO (Last In, First Out)
pub struct Stack<T> {
    data: Vec<T>,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { data: Vec::new() }
    }

    pub fn with_capacity(capacity: usize) -> Self {
        Stack { data: Vec::with_capacity(capacity) }
    }

    pub fn push(&mut self, value: T) {
        self.data.push(value);
    }

    pub fn pop(&mut self) -> Option<T> {
        self.data.pop()
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.last()
    }

    /// Capacidad actual del buffer interno
    pub(crate) fn capacity(&self) -> usize {
        self.data.capacity()
    }
}

impl<T> Collection for Stack<T> {
    type Item = T;

    fn len(&self) -> usize {
        self.data.len()
    }

    fn clear(&mut self) {
        self.data.clear();
    }
}

// Implementación de Display para tipos que implementan Debug
impl<T: std::fmt::Debug> std::fmt::Display for Stack<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Stack[")?;
        for (i, item) in self.data.iter().enumerate() {
            if i > 0 {
                write!(f, ", ")?;
            }
            write!(f, "{:?}", item)?;
        }
        write!(f, "]")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Helpers ─────────────────────────────────────────

    /// Crea un Stack con los elementos dados (el último es el top)
    fn stack_of(items: &[i32]) -> Stack<i32> {
        let mut s = Stack::new();
        for &item in items {
            s.push(item);
        }
        s
    }

    // ── Construcción ────────────────────────────────────

    #[test]
    fn new_stack_is_empty() {
        let s: Stack<i32> = Stack::new();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
    }

    #[test]
    fn with_capacity_preallocates() {
        let s: Stack<i32> = Stack::with_capacity(100);
        assert!(s.is_empty());
        assert!(s.capacity() >= 100);
    }

    // ── Push y Pop ──────────────────────────────────────

    #[test]
    fn push_increases_len() {
        let mut s = Stack::new();
        s.push(1);
        assert_eq!(s.len(), 1);
        s.push(2);
        assert_eq!(s.len(), 2);
    }

    #[test]
    fn pop_returns_last_pushed() {
        let mut s = stack_of(&[1, 2, 3]);
        assert_eq!(s.pop(), Some(3));
        assert_eq!(s.pop(), Some(2));
        assert_eq!(s.pop(), Some(1));
    }

    #[test]
    fn pop_empty_returns_none() {
        let mut s: Stack<i32> = Stack::new();
        assert_eq!(s.pop(), None);
    }

    #[test]
    fn pop_decreases_len() {
        let mut s = stack_of(&[1, 2, 3]);
        assert_eq!(s.len(), 3);
        s.pop();
        assert_eq!(s.len(), 2);
    }

    // ── Peek ────────────────────────────────────────────

    #[test]
    fn peek_returns_top_without_removing() {
        let s = stack_of(&[1, 2, 3]);
        assert_eq!(s.peek(), Some(&3));
        assert_eq!(s.len(), 3); // No cambió
    }

    #[test]
    fn peek_empty_returns_none() {
        let s: Stack<i32> = Stack::new();
        assert_eq!(s.peek(), None);
    }

    // ── Collection trait ────────────────────────────────

    #[test]
    fn clear_empties_stack() {
        let mut s = stack_of(&[1, 2, 3]);
        s.clear();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert_eq!(s.pop(), None);
    }

    #[test]
    fn is_empty_after_pop_all() {
        let mut s = stack_of(&[1]);
        assert!(!s.is_empty());
        s.pop();
        assert!(s.is_empty());
    }

    // ── Display ─────────────────────────────────────────

    #[test]
    fn display_empty() {
        let s: Stack<i32> = Stack::new();
        assert_eq!(format!("{}", s), "Stack[]");
    }

    #[test]
    fn display_with_elements() {
        let s = stack_of(&[1, 2, 3]);
        assert_eq!(format!("{}", s), "Stack[1, 2, 3]");
    }

    // ── LIFO property ───────────────────────────────────

    #[test]
    fn lifo_order_maintained() {
        let mut s = Stack::new();
        let input = vec![10, 20, 30, 40, 50];

        for &val in &input {
            s.push(val);
        }

        let mut output = Vec::new();
        while let Some(val) = s.pop() {
            output.push(val);
        }

        // LIFO: output debe ser el reverso del input
        let expected: Vec<i32> = input.into_iter().rev().collect();
        assert_eq!(output, expected);
    }
}
```

```rust
// src/queue.rs
use crate::Collection;
use std::collections::VecDeque;

/// Queue FIFO (First In, First Out)
pub struct Queue<T> {
    data: VecDeque<T>,
}

impl<T> Queue<T> {
    pub fn new() -> Self {
        Queue { data: VecDeque::new() }
    }

    pub fn enqueue(&mut self, value: T) {
        self.data.push_back(value);
    }

    pub fn dequeue(&mut self) -> Option<T> {
        self.data.pop_front()
    }

    pub fn front(&self) -> Option<&T> {
        self.data.front()
    }

    pub fn back(&self) -> Option<&T> {
        self.data.back()
    }
}

impl<T> Collection for Queue<T> {
    type Item = T;

    fn len(&self) -> usize {
        self.data.len()
    }

    fn clear(&mut self) {
        self.data.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn queue_of(items: &[i32]) -> Queue<i32> {
        let mut q = Queue::new();
        for &item in items {
            q.enqueue(item);
        }
        q
    }

    // ── Construcción ────────────────────────────────────

    #[test]
    fn new_queue_is_empty() {
        let q: Queue<i32> = Queue::new();
        assert!(q.is_empty());
    }

    // ── Enqueue y Dequeue ───────────────────────────────

    #[test]
    fn enqueue_increases_len() {
        let mut q = Queue::new();
        q.enqueue(1);
        assert_eq!(q.len(), 1);
        q.enqueue(2);
        assert_eq!(q.len(), 2);
    }

    #[test]
    fn dequeue_returns_first_enqueued() {
        let mut q = queue_of(&[1, 2, 3]);
        assert_eq!(q.dequeue(), Some(1));
        assert_eq!(q.dequeue(), Some(2));
        assert_eq!(q.dequeue(), Some(3));
    }

    #[test]
    fn dequeue_empty_returns_none() {
        let mut q: Queue<i32> = Queue::new();
        assert_eq!(q.dequeue(), None);
    }

    // ── Front y Back ────────────────────────────────────

    #[test]
    fn front_returns_first_without_removing() {
        let q = queue_of(&[10, 20, 30]);
        assert_eq!(q.front(), Some(&10));
        assert_eq!(q.len(), 3);
    }

    #[test]
    fn back_returns_last() {
        let q = queue_of(&[10, 20, 30]);
        assert_eq!(q.back(), Some(&30));
    }

    #[test]
    fn front_and_back_single_element() {
        let q = queue_of(&[42]);
        assert_eq!(q.front(), Some(&42));
        assert_eq!(q.back(), Some(&42));
    }

    // ── Collection trait ────────────────────────────────

    #[test]
    fn clear_empties_queue() {
        let mut q = queue_of(&[1, 2, 3]);
        q.clear();
        assert!(q.is_empty());
        assert_eq!(q.dequeue(), None);
    }

    // ── FIFO property ───────────────────────────────────

    #[test]
    fn fifo_order_maintained() {
        let mut q = Queue::new();
        let input = vec![10, 20, 30, 40, 50];

        for &val in &input {
            q.enqueue(val);
        }

        let mut output = Vec::new();
        while let Some(val) = q.dequeue() {
            output.push(val);
        }

        // FIFO: output debe ser igual al input
        assert_eq!(output, input);
    }
}
```

```rust
// src/deque.rs
use crate::Collection;
use std::collections::VecDeque;

/// Double-Ended Queue: push y pop por ambos extremos
pub struct Deque<T> {
    data: VecDeque<T>,
}

impl<T> Deque<T> {
    pub fn new() -> Self {
        Deque { data: VecDeque::new() }
    }

    pub fn push_front(&mut self, value: T) {
        self.data.push_front(value);
    }

    pub fn push_back(&mut self, value: T) {
        self.data.push_back(value);
    }

    pub fn pop_front(&mut self) -> Option<T> {
        self.data.pop_front()
    }

    pub fn pop_back(&mut self) -> Option<T> {
        self.data.pop_back()
    }

    pub fn front(&self) -> Option<&T> {
        self.data.front()
    }

    pub fn back(&self) -> Option<&T> {
        self.data.back()
    }
}

impl<T> Collection for Deque<T> {
    type Item = T;

    fn len(&self) -> usize {
        self.data.len()
    }

    fn clear(&mut self) {
        self.data.clear();
    }
}

// impl solo disponible en tests para inspección
#[cfg(test)]
impl<T: Clone> Deque<T> {
    /// Convierte el deque a un Vec para fácil comparación en tests
    fn to_vec(&self) -> Vec<T> {
        self.data.iter().cloned().collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Push/Pop front ──────────────────────────────────

    #[test]
    fn push_front_pop_front_is_lifo() {
        let mut d = Deque::new();
        d.push_front(1);
        d.push_front(2);
        d.push_front(3);
        assert_eq!(d.pop_front(), Some(3));
        assert_eq!(d.pop_front(), Some(2));
        assert_eq!(d.pop_front(), Some(1));
    }

    // ── Push/Pop back ───────────────────────────────────

    #[test]
    fn push_back_pop_back_is_lifo() {
        let mut d = Deque::new();
        d.push_back(1);
        d.push_back(2);
        d.push_back(3);
        assert_eq!(d.pop_back(), Some(3));
        assert_eq!(d.pop_back(), Some(2));
        assert_eq!(d.pop_back(), Some(1));
    }

    // ── Push front, Pop back (FIFO) ─────────────────────

    #[test]
    fn push_front_pop_back_is_fifo() {
        let mut d = Deque::new();
        d.push_front(1);
        d.push_front(2);
        d.push_front(3);
        assert_eq!(d.pop_back(), Some(1));
        assert_eq!(d.pop_back(), Some(2));
        assert_eq!(d.pop_back(), Some(3));
    }

    // ── Mixto ───────────────────────────────────────────

    #[test]
    fn mixed_operations() {
        let mut d = Deque::new();
        d.push_back(1);     // [1]
        d.push_back(2);     // [1, 2]
        d.push_front(0);    // [0, 1, 2]
        d.push_back(3);     // [0, 1, 2, 3]
        d.push_front(-1);   // [-1, 0, 1, 2, 3]

        assert_eq!(d.to_vec(), vec![-1, 0, 1, 2, 3]);
        assert_eq!(d.front(), Some(&-1));
        assert_eq!(d.back(), Some(&3));
    }

    // ── Vacío ───────────────────────────────────────────

    #[test]
    fn pop_front_empty() {
        let mut d: Deque<i32> = Deque::new();
        assert_eq!(d.pop_front(), None);
    }

    #[test]
    fn pop_back_empty() {
        let mut d: Deque<i32> = Deque::new();
        assert_eq!(d.pop_back(), None);
    }

    // ── Collection trait ────────────────────────────────

    #[test]
    fn len_tracks_operations() {
        let mut d = Deque::new();
        assert_eq!(d.len(), 0);
        d.push_front(1);
        assert_eq!(d.len(), 1);
        d.push_back(2);
        assert_eq!(d.len(), 2);
        d.pop_front();
        assert_eq!(d.len(), 1);
        d.pop_back();
        assert_eq!(d.len(), 0);
    }

    #[test]
    fn clear_empties_deque() {
        let mut d = Deque::new();
        d.push_back(1);
        d.push_back(2);
        d.clear();
        assert!(d.is_empty());
        assert_eq!(d.front(), None);
        assert_eq!(d.back(), None);
    }

    // ── Uso como Stack (solo back) ──────────────────────

    #[test]
    fn deque_as_stack() {
        let mut d = Deque::new();
        d.push_back(1);
        d.push_back(2);
        d.push_back(3);
        assert_eq!(d.pop_back(), Some(3)); // LIFO
        assert_eq!(d.pop_back(), Some(2));
        assert_eq!(d.pop_back(), Some(1));
    }

    // ── Uso como Queue (push_back, pop_front) ───────────

    #[test]
    fn deque_as_queue() {
        let mut d = Deque::new();
        d.push_back(1);
        d.push_back(2);
        d.push_back(3);
        assert_eq!(d.pop_front(), Some(1)); // FIFO
        assert_eq!(d.pop_front(), Some(2));
        assert_eq!(d.pop_front(), Some(3));
    }
}
```

### Salida de cargo test

```
$ cargo test
   Compiling collections v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
     Running unittests src/lib.rs

running 1 test
test tests::reexports_are_accessible ... ok

test result: ok. 1 passed; 0 failed; 0 ignored

     Running unittests src/lib.rs (stack module)

running 13 tests
test stack::tests::new_stack_is_empty ... ok
test stack::tests::with_capacity_preallocates ... ok
test stack::tests::push_increases_len ... ok
test stack::tests::pop_returns_last_pushed ... ok
test stack::tests::pop_empty_returns_none ... ok
test stack::tests::pop_decreases_len ... ok
test stack::tests::peek_returns_top_without_removing ... ok
test stack::tests::peek_empty_returns_none ... ok
test stack::tests::clear_empties_stack ... ok
test stack::tests::is_empty_after_pop_all ... ok
test stack::tests::display_empty ... ok
test stack::tests::display_with_elements ... ok
test stack::tests::lifo_order_maintained ... ok

running 9 tests
test queue::tests::new_queue_is_empty ... ok
test queue::tests::enqueue_increases_len ... ok
test queue::tests::dequeue_returns_first_enqueued ... ok
test queue::tests::dequeue_empty_returns_none ... ok
test queue::tests::front_returns_first_without_removing ... ok
test queue::tests::back_returns_last ... ok
test queue::tests::front_and_back_single_element ... ok
test queue::tests::clear_empties_queue ... ok
test queue::tests::fifo_order_maintained ... ok

running 10 tests
test deque::tests::push_front_pop_front_is_lifo ... ok
test deque::tests::push_back_pop_back_is_lifo ... ok
test deque::tests::push_front_pop_back_is_fifo ... ok
test deque::tests::mixed_operations ... ok
test deque::tests::pop_front_empty ... ok
test deque::tests::pop_back_empty ... ok
test deque::tests::len_tracks_operations ... ok
test deque::tests::clear_empties_deque ... ok
test deque::tests::deque_as_stack ... ok
test deque::tests::deque_as_queue ... ok

test result: ok. 33 passed; 0 failed; 0 ignored
```

### Ejecución selectiva

```bash
# Solo el módulo stack
cargo test stack::tests
# running 13 tests ... ok

# Solo tests del deque que contengan "fifo"
cargo test deque::tests::fifo
# running 1 test
# test deque::tests::push_front_pop_back_is_fifo ... ok

# Solo tests que contengan "empty"
cargo test empty
# running 6 tests (de stack, queue y deque)

# Tests de Collection (clear, is_empty)
cargo test clear
# running 3 tests (uno por módulo)
```

---

## 19. Programa de práctica

Implementa un módulo `ring_buffer` — un buffer circular de tamaño fijo:

```rust
// src/ring_buffer.rs

/// Buffer circular de tamaño fijo.
///
/// Cuando el buffer está lleno, las nuevas escrituras sobrescriben
/// los datos más antiguos.
///
/// Ejemplo de operación con capacidad 3:
///
///   push(A) → [A, _, _]  head=0, len=1
///   push(B) → [A, B, _]  head=0, len=2
///   push(C) → [A, B, C]  head=0, len=3 (LLENO)
///   push(D) → [D, B, C]  head=1, len=3 (A sobrescrito)
///   pop()   → B          [D, _, C]  head=2, len=2
///   pop()   → C          [D, _, _]  head=0, len=1
///   pop()   → D          [_, _, _]  head=0, len=0
pub struct RingBuffer<T> {
    // Usa Vec<Option<T>> internamente
    // Campos sugeridos: data, head, tail, len, capacity
    todo!()
}

impl<T> RingBuffer<T> {
    /// Crea un RingBuffer con la capacidad dada.
    /// Panics si capacity es 0.
    pub fn new(capacity: usize) -> Self { todo!() }

    /// Inserta un elemento. Si está lleno, sobrescribe el más antiguo.
    /// Retorna Some(old) si sobrescribió, None si había espacio.
    pub fn push(&mut self, value: T) -> Option<T> { todo!() }

    /// Extrae el elemento más antiguo. None si está vacío.
    pub fn pop(&mut self) -> Option<T> { todo!() }

    /// Referencia al elemento más antiguo sin extraerlo.
    pub fn peek(&self) -> Option<&T> { todo!() }

    /// Número de elementos actualmente en el buffer.
    pub fn len(&self) -> usize { todo!() }

    /// True si no hay elementos.
    pub fn is_empty(&self) -> bool { todo!() }

    /// True si len == capacity.
    pub fn is_full(&self) -> bool { todo!() }

    /// La capacidad máxima del buffer.
    pub fn capacity(&self) -> usize { todo!() }

    /// Vacía el buffer.
    pub fn clear(&mut self) { todo!() }
}
```

**Requisitos de organización de tests**:

1. Todos los tests en un `#[cfg(test)] mod tests` al final del archivo
2. Al menos 3 funciones helper:
   - `ring_of(cap, items)` — crea un RingBuffer y le hace push de los items
   - `drain(rb)` — extrae todos los elementos en un Vec
   - `assert_contents(rb, expected)` — verifica que los elementos del buffer son los esperados
3. Tests organizados con submódulos:
   - `mod construction` — new, capacity
   - `mod push_pop` — push, pop, peek
   - `mod overflow` — comportamiento cuando está lleno
   - `mod clear` — clear, is_empty
4. Al menos 20 tests
5. Al menos un test que verifique `#[should_panic]` para `new(0)`
6. Nombrado descriptivo (sin prefijo `test_`)

---

## 20. Ejercicios

### Ejercicio 1: Inline vs separado

Dado un archivo `src/tokenizer.rs` con 200 líneas de implementación y 450 líneas de tests:

**a)** ¿Recomendarías tests inline o en archivo separado? Justifica.

**b)** Si decides separar, escribe la línea exacta que va en `tokenizer.rs` para declarar el módulo, y las primeras líneas de `tokenizer/tests.rs`. Indica qué **NO** debe ir en `tests.rs`.

**c)** Si el tokenizer usa `tempfile` como dev-dependency, ¿dónde va el `use tempfile::*;`? Muestra las dos opciones válidas y cuál es preferible.

### Ejercicio 2: Visibilidad y acceso

Dado este código:

```rust
// src/auth.rs
pub struct Session { /* ... */ }

pub(crate) fn validate_token(token: &str) -> bool { todo!() }
fn hash_password(password: &str) -> String { todo!() }

// src/middleware.rs
// ¿Puede acceder a validate_token? ¿Y a hash_password?

// tests/integration.rs
// ¿Puede acceder a validate_token? ¿Y a Session?
```

**a)** Completa la tabla de accesibilidad para cada función/tipo desde cada ubicación (auth::tests, middleware, tests/integration.rs).

**b)** Si `middleware.rs` necesita llamar a `hash_password`, ¿cuáles son las tres opciones y cuál es la más idiomática?

**c)** Escribe un bloque `#[cfg(test)] impl Session` que añada un método `mock()` solo disponible en tests.

### Ejercicio 3: Organización multi-módulo

Diseña la estructura de archivos y tests para un crate llamado `minidb` con:

- `storage.rs` — lectura/escritura a disco
- `index.rs` — índice B-tree en memoria
- `query.rs` — parsing y ejecución de queries simples
- `engine.rs` — coordina storage + index + query

**a)** Dibuja el árbol de archivos completo incluyendo `lib.rs`, `Cargo.toml`, y el directorio `tests/`.

**b)** Para cada módulo, indica si los tests deberían ser inline o en archivo separado, justificando por el tipo de lógica.

**c)** Escribe el `lib.rs` completo con las declaraciones de módulos y re-exports.

**d)** Escribe un `#[cfg(test)] impl` para `engine.rs` que añada un constructor `Engine::test_engine()` que use datos en memoria en vez de disco.

### Ejercicio 4: Migración de C a Rust

Dado este proyecto en C:

```
c_project/
├── src/
│   ├── list.c
│   ├── list.h
│   ├── sort.c
│   └── sort.h
├── tests/
│   ├── test_list.c
│   ├── test_sort.c
│   └── test_runner.c
├── vendor/
│   └── unity/
│       ├── unity.c
│       └── unity.h
└── Makefile
```

**a)** Diseña la estructura equivalente en Rust. ¿Cuántos archivos de test necesitas? ¿Se necesita un framework externo?

**b)** Traduce este fragmento de test de C a Rust idiomático, incluyendo la organización correcta:

```c
// tests/test_list.c
#include "unity.h"
#include "../src/list.h"

void test_list_append(void) {
    List *l = list_new();
    list_append(l, 42);
    TEST_ASSERT_EQUAL(1, list_len(l));
    TEST_ASSERT_EQUAL(42, list_get(l, 0));
    list_free(l);
}
```

**c)** ¿Qué reemplaza a `list_free(l)` en la versión Rust? ¿Dónde queda el cleanup del test?

---

## Navegación

- **Anterior**: [T03 - Tests con Result](../T03_Tests_con_Result/README.md)
- **Siguiente**: [S02/T01 - Integration tests](../../S02_Integration_Tests_y_Doc_Tests/T01_Integration_tests/README.md)

---

> **Fin de la Sección 1: Testing Built-in**
>
> Esta sección cubrió los fundamentos del sistema de testing integrado de Rust:
> - T01: `#[test]`, `assert!`, `assert_eq!`, `assert_ne!`, `mod tests`, `#[cfg(test)]`
> - T02: `#[should_panic]`, `expected`, `catch_unwind`, cuándo usar vs Result
> - T03: `Result<(), E>` en tests, operador `?`, `Box<dyn Error>`, anyhow
> - T04: Organización — inline vs separado, `use super::*`, `#[cfg(test)]` para imports, visibilidad
>
> Con estos fundamentos, la Sección 2 aborda testing más allá del módulo: integration tests, doc tests, y herramientas de `cargo test`.
