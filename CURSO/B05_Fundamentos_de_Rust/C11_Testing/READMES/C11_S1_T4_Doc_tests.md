# Doc tests: ejemplos en documentación que se ejecutan como tests

## Índice
1. [Qué son los doc tests](#1-qué-son-los-doc-tests)
2. [Tu primer doc test](#2-tu-primer-doc-test)
3. [Cómo se ejecutan](#3-cómo-se-ejecutan)
4. [El main invisible](#4-el-main-invisible)
5. [Ocultar líneas con #](#5-ocultar-líneas-con-)
6. [Atributos: ignore, no_run, compile_fail, should_panic](#6-atributos-ignore-no_run-compile_fail-should_panic)
7. [Doc tests con Result y ?](#7-doc-tests-con-result-y-)
8. [Doc tests para structs, enums y traits](#8-doc-tests-para-structs-enums-y-traits)
9. [Doc tests en módulos y crate-level](#9-doc-tests-en-módulos-y-crate-level)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué son los doc tests

Los **doc tests** son ejemplos de código dentro de la documentación (`///` o `//!`) que Rust **compila y ejecuta como tests**. Cumplen doble propósito:

1. **Documentación**: muestran al usuario cómo usar tu API.
2. **Verificación**: garantizan que los ejemplos realmente funcionan.

```rust
/// Adds two numbers together.
///
/// # Examples
///
/// ```
/// let result = mi_crate::add(2, 3);
/// assert_eq!(result, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

Ese bloque de código entre ` ``` ` se compila y ejecuta con `cargo test`. Si el ejemplo deja de funcionar (por un cambio en la API), el test falla — los docs nunca se desactualizan.

---

## 2. Tu primer doc test

```rust
/// Reverses a string.
///
/// # Examples
///
/// ```
/// let reversed = mi_crate::reverse("hello");
/// assert_eq!(reversed, "olleh");
/// ```
///
/// Empty strings are handled correctly:
///
/// ```
/// let reversed = mi_crate::reverse("");
/// assert_eq!(reversed, "");
/// ```
pub fn reverse(s: &str) -> String {
    s.chars().rev().collect()
}
```

```bash
$ cargo test
   Doc-tests mi_crate

running 2 tests
test src/lib.rs - reverse (line 5) ... ok
test src/lib.rs - reverse (line 12) ... ok

test result: ok. 2 passed; 0 failed
```

Cada bloque ` ``` ` es un test independiente. Cargo los identifica por archivo y número de línea.

### Reglas básicas

- Los doc tests se escriben en comentarios `///` (items) o `//!` (módulos/crate).
- El código va entre ` ``` ` y ` ``` `.
- Cada bloque es un programa completo e independiente.
- Se compilan como un **crate externo** que depende de tu library.
- Solo funcionan en **library crates** (`lib.rs`).

---

## 3. Cómo se ejecutan

Cada bloque de código se compila como un mini-programa independiente:

```rust
/// ```
/// let x = 2 + 3;
/// assert_eq!(x, 5);
/// ```
pub fn something() {}
```

Rust transforma internamente el bloque en:

```rust
// Generado automáticamente por cargo test:
extern crate mi_crate;

fn main() {
    let x = 2 + 3;
    assert_eq!(x, 5);
}
```

Por eso:
- Necesitas `mi_crate::` para acceder a tus items (es un crate externo).
- Puedes usar `use mi_crate::something;` para importar.
- No tienes acceso a funciones privadas.

### Output de cargo test

`cargo test` ejecuta tres categorías separadas:

```bash
$ cargo test

     Running unittests src/lib.rs        ← unit tests (#[test])
running 3 tests ...

     Running tests/integration.rs        ← integration tests
running 2 tests ...

   Doc-tests mi_crate                    ← doc tests
running 4 tests ...
```

Para ejecutar solo doc tests:

```bash
cargo test --doc
```

---

## 4. El main invisible

Cada doc test se envuelve automáticamente en `fn main() { ... }`. Pero si tu código ya contiene `fn main`, Rust lo usa directamente:

### Sin main (lo más común)

```rust
/// ```
/// let x = mi_crate::add(1, 2);
/// assert_eq!(x, 3);
/// ```
```

Se convierte en:

```rust
fn main() {
    let x = mi_crate::add(1, 2);
    assert_eq!(x, 3);
}
```

### Con main explícito

```rust
/// ```
/// fn main() {
///     let x = mi_crate::add(1, 2);
///     assert_eq!(x, 3);
/// }
/// ```
```

Rust detecta el `fn main` y **no** envuelve en otro. Útil cuando necesitas `async fn main` o atributos:

```rust
/// ```
/// #[tokio::main]
/// async fn main() {
///     let result = mi_crate::fetch("url").await;
///     assert!(result.is_ok());
/// }
/// ```
```

---

## 5. Ocultar líneas con #

Las líneas que empiezan con `# ` (hash + espacio) se **ejecutan pero no se muestran** en la documentación renderizada. Esto permite incluir setup necesario sin ensuciar el ejemplo:

```rust
/// Processes a configuration file.
///
/// # Examples
///
/// ```
/// # use std::fs;
/// # use std::io::Write;
/// # let dir = tempfile::tempdir().unwrap();
/// # let path = dir.path().join("config.toml");
/// # fs::write(&path, "port = 8080").unwrap();
/// let config = mi_crate::load_config(path.to_str().unwrap()).unwrap();
/// assert_eq!(config.port, 8080);
/// ```
pub fn load_config(path: &str) -> Result<Config, Error> {
    // ...
}
```

Lo que el usuario ve en `cargo doc`:

```rust
let config = mi_crate::load_config(path.to_str().unwrap()).unwrap();
assert_eq!(config.port, 8080);
```

Lo que se ejecuta como test incluye todas las líneas con `#`:

```rust
use std::fs;
use std::io::Write;
let dir = tempfile::tempdir().unwrap();
let path = dir.path().join("config.toml");
fs::write(&path, "port = 8080").unwrap();
let config = mi_crate::load_config(path.to_str().unwrap()).unwrap();
assert_eq!(config.port, 8080);
```

### Usos comunes de #

```rust
/// ```
/// # // Imports que el lector no necesita ver:
/// # use mi_crate::Parser;
/// #
/// # // Setup que distrae del punto del ejemplo:
/// # let input = "sample data for testing";
/// #
/// let parser = Parser::new();
/// let result = parser.parse(input);
/// assert!(result.is_ok());
/// ```
```

### Solo # al inicio de la línea

```rust
/// ```
/// # use mi_crate::process;
/// let result = process("hello");
/// assert_eq!(result, "HELLO");  // los # en comments normales se muestran
/// ```
```

---

## 6. Atributos: ignore, no_run, compile_fail, should_panic

Puedes anotar el bloque de código con atributos después de los backticks:

### ignore: no compilar ni ejecutar

```rust
/// ```ignore
/// // Este código no se compila ni ejecuta
/// // Útil para pseudocódigo o ejemplos incompletos
/// let connection = Database::connect("production-url");
/// connection.drop_all_tables(); // ¡no queremos ejecutar esto!
/// ```
```

`cargo test` reporta: `test ... - ignored`

### no_run: compilar pero no ejecutar

```rust
/// ```no_run
/// // Se compila (verifica que es código válido) pero no se ejecuta
/// // Útil para código con side effects (red, filesystem, exit)
/// use std::process;
/// let output = process::Command::new("rm")
///     .arg("-rf")
///     .arg("/tmp/test")
///     .output()
///     .unwrap();
/// ```
```

Verifica que el código **compila** sin ejecutarlo. Perfecto para:
- Código que requiere red o servicios externos.
- Código con `process::exit()`.
- Ejemplos que modifican el filesystem.
- Código que tarda mucho.

### compile_fail: verificar que NO compila

```rust
/// This type cannot be cloned.
///
/// ```compile_fail
/// let a = mi_crate::UniqueHandle::new();
/// let b = a.clone();  // ERROR: UniqueHandle doesn't implement Clone
/// ```
pub struct UniqueHandle {
    // ...
}
```

El test **pasa si el código falla al compilar**. Útil para documentar restricciones del sistema de tipos:

```rust
/// References cannot outlive the data they point to:
///
/// ```compile_fail
/// let reference;
/// {
///     let data = String::from("hello");
///     reference = &data;
/// }
/// println!("{}", reference);  // ERROR: data dropped
/// ```
```

### should_panic: esperar panic

```rust
/// Panics if the index is out of bounds.
///
/// ```should_panic
/// mi_crate::safe_get(&[1, 2, 3], 10);
/// ```
pub fn safe_get(slice: &[i32], index: usize) -> i32 {
    // ...
}
```

### Combinar atributos con lenguaje

```rust
/// ```rust,no_run
/// // Explícitamente marcado como Rust + no_run
/// ```

/// ```rust,ignore
/// // Explícitamente Rust + ignorado
/// ```

/// ```text
/// Esto no es código — es texto plano.
/// No se compila ni ejecuta.
/// ```

/// ```python
/// # Esto es Python, no Rust.
/// # Cargo lo ignora completamente.
/// print("hello")
/// ```
```

Solo los bloques marcados como `rust` (o sin lenguaje, que es Rust por defecto) se ejecutan como doc tests.

---

## 7. Doc tests con Result y ?

Usar `unwrap()` en cada línea es ruidoso. Puedes usar `?` con un main que retorna Result:

### Patrón con main explícito

```rust
/// ```
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let number: i32 = "42".parse()?;
/// let config = mi_crate::Config::from_str("port=8080")?;
/// assert_eq!(config.port, 8080);
/// # Ok(())
/// # }
/// ```
```

El usuario ve:

```rust
let number: i32 = "42".parse()?;
let config = mi_crate::Config::from_str("port=8080")?;
assert_eq!(config.port, 8080);
```

Limpio y sin unwrap. Las líneas de `fn main` y `Ok(())` están ocultas con `#`.

### Patrón simplificado (Rust reciente)

Si tu doc test contiene `?` pero no tiene `fn main`, Rust automáticamente lo envuelve en un main que retorna `Result<(), impl Debug>`:

```rust
/// ```
/// # use mi_crate::Config;
/// let config = Config::from_str("port=8080")?;
/// assert_eq!(config.port, 8080);
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
```

La última línea `Ok::<(), Box<dyn std::error::Error>>(())` le dice al compilador el tipo de `Result`. Es necesaria porque Rust necesita inferir el tipo de retorno de main.

---

## 8. Doc tests para structs, enums y traits

### Struct

```rust
/// A 2D point in space.
///
/// # Examples
///
/// ```
/// use mi_crate::Point;
///
/// let p = Point::new(3.0, 4.0);
/// assert_eq!(p.distance_to_origin(), 5.0);
/// ```
pub struct Point {
    pub x: f64,
    pub y: f64,
}

impl Point {
    /// Creates a new point.
    ///
    /// ```
    /// use mi_crate::Point;
    ///
    /// let origin = Point::new(0.0, 0.0);
    /// assert_eq!(origin.x, 0.0);
    /// ```
    pub fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    /// Calculates the distance from this point to the origin.
    ///
    /// ```
    /// use mi_crate::Point;
    ///
    /// let p = Point::new(3.0, 4.0);
    /// assert!((p.distance_to_origin() - 5.0).abs() < f64::EPSILON);
    /// ```
    pub fn distance_to_origin(&self) -> f64 {
        (self.x * self.x + self.y * self.y).sqrt()
    }
}
```

### Enum

```rust
/// Represents a color.
///
/// # Examples
///
/// ```
/// use mi_crate::Color;
///
/// let c = Color::from_hex("#FF0000").unwrap();
/// assert_eq!(c, Color::Rgb(255, 0, 0));
/// ```
#[derive(Debug, PartialEq)]
pub enum Color {
    Rgb(u8, u8, u8),
    Named(String),
}
```

### Trait

```rust
/// A trait for types that can be serialized to a simple string format.
///
/// # Examples
///
/// ```
/// use mi_crate::SimpleSerialize;
///
/// struct Point { x: i32, y: i32 }
///
/// impl SimpleSerialize for Point {
///     fn serialize(&self) -> String {
///         format!("{},{}", self.x, self.y)
///     }
/// }
///
/// let p = Point { x: 1, y: 2 };
/// assert_eq!(p.serialize(), "1,2");
/// ```
pub trait SimpleSerialize {
    fn serialize(&self) -> String;
}
```

Los doc tests en traits son especialmente valiosos porque muestran **cómo implementar** el trait.

---

## 9. Doc tests en módulos y crate-level

### Documentación del crate (//!)

```rust
// src/lib.rs

//! # Mi Crate
//!
//! A library for doing useful things.
//!
//! ## Quick Start
//!
//! ```
//! use mi_crate::{Parser, evaluate};
//!
//! let ast = Parser::new().parse("2 + 3 * 4").unwrap();
//! let result = evaluate(&ast);
//! assert_eq!(result, 14);
//! ```
//!
//! ## Features
//!
//! - Fast parsing
//! - Extensible evaluation

pub mod parser;
pub mod evaluator;
```

Los doc tests del crate-level (`//!`) son la mejor introducción a tu library — muestran el flujo completo.

### Documentación de módulos

```rust
/// Parsing utilities for mathematical expressions.
///
/// # Examples
///
/// ```
/// use mi_crate::parser::Parser;
///
/// let parser = Parser::new();
/// let ast = parser.parse("1 + 2").unwrap();
/// assert_eq!(ast.to_string(), "(+ 1 2)");
/// ```
pub mod parser {
    // ...
}
```

---

## 10. Errores comunes

### Error 1: Olvidar que el doc test es un crate externo

```rust
/// ```
/// let x = add(2, 3);  // ERROR: cannot find function `add`
/// assert_eq!(x, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

Necesitas calificar con el nombre del crate o usar `use`:

```rust
/// ```
/// let x = mi_crate::add(2, 3);
/// // o:
/// // use mi_crate::add;
/// // let x = add(2, 3);
/// assert_eq!(x, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

### Error 2: Doc test en función privada

```rust
/// This won't run as a test because the function is private.
///
/// ```
/// mi_crate::internal_helper();
/// ```
fn internal_helper() { }
// Los doc tests de items privados no se ejecutan con cargo test
// (el item no es visible desde fuera del crate)
```

`cargo test --doc` solo ejecuta doc tests de items **públicos**. Documentar items privados es válido para `cargo doc --document-private-items`, pero los tests no se ejecutan.

### Error 3: Doc test que no tiene asserts

```rust
/// ```
/// let result = mi_crate::process("data");
/// // Sin assert — solo verifica que no hay panic
/// // ¿El resultado es correcto? No lo sabemos
/// ```
```

Un doc test sin assert solo verifica que el código compila y no hace panic. Añade asserts para verificar el resultado:

```rust
/// ```
/// let result = mi_crate::process("data");
/// assert_eq!(result, "PROCESSED: data");
/// ```
```

### Error 4: Bloque de código no-Rust ejecutado como test

```rust
/// Example JSON output:
///
/// ```
/// {
///     "name": "Alice",
///     "age": 30
/// }
/// ```
```

Este bloque de "JSON" se interpreta como Rust y falla al compilar. Marca el lenguaje:

```rust
/// Example JSON output:
///
/// ```json
/// {
///     "name": "Alice",
///     "age": 30
/// }
/// ```
```

O usa `text`:

```rust
/// ```text
/// No es código Rust
/// ```
```

### Error 5: Doc tests en binary crates

```rust
// src/main.rs
/// ```
/// let x = 42;
/// ```
fn main() {
    // Los doc tests no se ejecutan en binary crates
    // Solo funcionan con lib.rs
}
```

Si necesitas doc tests, crea un `lib.rs`.

---

## 11. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                       DOC TESTS CHEATSHEET                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESCRIBIR                                                          ║
║  /// texto                      Doc comment para items             ║
║  //! texto                      Doc comment para módulo/crate      ║
║  /// ```                        Inicio de doc test                 ║
║  /// código rust aquí            Código que se compila y ejecuta   ║
║  /// ```                        Fin de doc test                    ║
║                                                                    ║
║  Cada bloque ``` es un test independiente                          ║
║  Se compila como crate externo → necesita mi_crate::              ║
║  Se envuelve en fn main() automáticamente                          ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  OCULTAR LÍNEAS                                                    ║
║  /// # use mi_crate::Foo;       Se ejecuta pero no se muestra     ║
║  /// # fn main() -> Result...   Main oculto para usar ?            ║
║  /// # Ok(())                   Retorno oculto                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ATRIBUTOS                                                         ║
║  ```                            Normal: compila + ejecuta          ║
║  ```ignore                      No compila ni ejecuta              ║
║  ```no_run                      Compila pero no ejecuta            ║
║  ```compile_fail                Pasa si NO compila                 ║
║  ```should_panic                Pasa si hace panic                 ║
║  ```text                        No es Rust, se ignora              ║
║  ```json / ```python / etc.     No es Rust, se ignora              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EJECUTAR                                                          ║
║  cargo test                     Todos (incluye doc tests)          ║
║  cargo test --doc               Solo doc tests                     ║
║  cargo doc --open               Ver la documentación generada      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  USAR ? EN DOC TESTS                                               ║
║  /// ```                                                           ║
║  /// # fn main() -> Result<(), Box<dyn std::error::Error>> {       ║
║  /// let val: i32 = "42".parse()?;                                 ║
║  /// # Ok(())                                                      ║
║  /// # }                                                           ║
║  /// ```                                                           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  SECCIONES CONVENCIONALES EN DOCS                                  ║
║  # Examples         Cómo usar la función/tipo                      ║
║  # Panics           Cuándo hace panic                              ║
║  # Errors           Qué errores puede retornar                     ║
║  # Safety           Para funciones unsafe                          ║
║                                                                    ║
║  REGLAS                                                            ║
║  ✓ Solo en library crates (lib.rs)                                 ║
║  ✓ Solo items pub generan doc tests ejecutables                    ║
║  ✓ Incluir assert para verificar resultados                        ║
║  ✓ Marcar código no-Rust con ```json, ```text, etc.                ║
║  ✓ Usar # para ocultar setup del lector                            ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Añadir doc tests a una library

Añade documentación con doc tests ejecutables a cada item público:

```rust
// src/lib.rs — añade /// con ejemplos a cada item

pub fn celsius_to_fahrenheit(celsius: f64) -> f64 {
    celsius * 9.0 / 5.0 + 32.0
}

pub fn fahrenheit_to_celsius(fahrenheit: f64) -> f64 {
    (fahrenheit - 32.0) * 5.0 / 9.0
}

pub struct Temperature {
    celsius: f64,
}

impl Temperature {
    pub fn from_celsius(c: f64) -> Self {
        Temperature { celsius: c }
    }

    pub fn from_fahrenheit(f: f64) -> Self {
        Temperature {
            celsius: fahrenheit_to_celsius(f),
        }
    }

    pub fn celsius(&self) -> f64 {
        self.celsius
    }

    pub fn fahrenheit(&self) -> f64 {
        celsius_to_fahrenheit(self.celsius)
    }
}
```

**Requisitos:**
1. Doc test para `celsius_to_fahrenheit` (agua hierve: 100°C = 212°F).
2. Doc test para `fahrenheit_to_celsius` (congelación: 32°F = 0°C).
3. Doc test para `Temperature::from_celsius` que muestre creación y lectura.
4. Doc test para `Temperature::from_fahrenheit` con conversión ida y vuelta.
5. Doc test en el crate-level (`//!`) mostrando el flujo completo.
6. Usa `#` para ocultar imports donde sea apropiado.
7. Verifica que todos pasan con `cargo test --doc`.

### Ejercicio 2: Atributos de doc tests

Para cada caso, decide qué atributo usar y escribe el doc test:

```rust
// A) Función que se conecta a una base de datos (no queremos conectar en tests)
pub fn connect(url: &str) -> Connection {
    // ...
}
// ¿Qué atributo: ignore, no_run, o normal?

// B) Tipo que NO debe implementar Clone (queremos documentar que clonar falla)
pub struct FileHandle {
    fd: i32,
}
// Escribe un doc test que muestre que clone() no compila

// C) Función que hace panic con input inválido (documentar el panic)
pub fn parse_positive(s: &str) -> u32 {
    let n: i32 = s.parse().expect("not a number");
    assert!(n > 0, "must be positive, got {}", n);
    n as u32
}
// Escribe doc tests para: uso normal + caso de panic

// D) Ejemplo que muestra output pero no necesita verificación
pub fn print_banner() {
    println!("╔════════════════╗");
    println!("║  MY APP v1.0   ║");
    println!("╚════════════════╝");
}
// ¿Qué atributo usarías?
```

### Ejercicio 3: Doc tests como documentación completa

Escribe documentación completa (con secciones convencionales) para esta función, incluyendo múltiples doc tests:

```rust
pub fn search(haystack: &str, needle: &str, case_sensitive: bool) -> Vec<usize> {
    if needle.is_empty() {
        return Vec::new();
    }
    let (h, n) = if case_sensitive {
        (haystack.to_string(), needle.to_string())
    } else {
        (haystack.to_lowercase(), needle.to_lowercase())
    };
    h.match_indices(&n)
        .map(|(i, _)| i)
        .collect()
}
```

**Tu documentación debe incluir:**
1. Descripción de qué hace la función (1-2 líneas).
2. Sección `# Arguments` describiendo cada parámetro.
3. Sección `# Returns` describiendo el valor de retorno.
4. Sección `# Examples` con al menos 3 doc tests:
   - Búsqueda case-sensitive que encuentra resultados.
   - Búsqueda case-insensitive.
   - Búsqueda sin resultados (retorna vec vacío).
5. Un doc test que muestre el caso de `needle` vacío.
6. Verifica que todos los doc tests pasan con `cargo test --doc`.

---

> **Nota**: los doc tests son una de las características más valoradas de Rust en el ecosistema. Mientras que en otros lenguajes los ejemplos de documentación se desactualizan silenciosamente, en Rust cada ejemplo es un test que se ejecuta en CI. Esto crea un ciclo virtuoso: los mantenedores confían en los ejemplos porque se verifican automáticamente, y los usuarios confían en la documentación porque saben que los ejemplos funcionan. Cuando publiques un crate, prioriza buenos doc tests — son la primera impresión de tu API.
