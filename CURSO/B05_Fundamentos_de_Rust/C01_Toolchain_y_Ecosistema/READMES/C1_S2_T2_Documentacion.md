# T02 — Documentacion

## La cultura de documentacion en Rust

Rust trata la documentacion como ciudadano de primera clase.
La diferencia fundamental con otros lenguajes es que **la
documentacion es codigo**: se compila, se ejecuta como test,
y se distribuye junto con el crate.

```rust
// En Rust, la documentacion no es un archivo de texto suelto.
// Esta incrustada en el codigo fuente, usa Markdown,
// y los ejemplos que contiene se compilan y ejecutan
// como parte de cargo test.
//
// Si el codigo de ejemplo no compila → cargo test falla.
// Si el ejemplo no produce el resultado esperado → cargo test falla.
// Resultado: la documentacion NUNCA puede quedar desactualizada
// respecto al codigo real.
```

## cargo doc

`cargo doc` genera documentacion HTML a partir de los doc comments
del codigo fuente. Usa la herramienta `rustdoc` internamente:

```bash
# Generar documentacion del proyecto y todas sus dependencias:
cargo doc

# Generar y abrir en el navegador automaticamente:
cargo doc --open

# Generar solo la documentacion de tu crate (sin dependencias):
cargo doc --no-deps
# Mucho mas rapido. Los links a tipos de dependencias apuntan a docs.rs.

# Generar con documentacion de items privados (util durante desarrollo):
cargo doc --document-private-items
```

```bash
# La documentacion se genera en target/doc/:
#
# target/
# └── doc/
#     ├── mi_crate/           ← tu crate
#     │   ├── index.html      ← pagina principal
#     │   ├── fn.add.html     ← documentacion de la funcion add
#     │   └── struct.Widget.html
#     ├── serde/              ← dependencia (si no usaste --no-deps)
#     └── ...
#
# Puedes abrir target/doc/mi_crate/index.html directamente en el navegador.
```

```bash
# docs.rs — la plataforma de documentacion del ecosistema Rust.
# Cuando publicas un crate con cargo publish, docs.rs genera
# y hospeda la documentacion automaticamente.
#
# URL: https://docs.rs/nombre_del_crate
# Ejemplo: https://docs.rs/serde
#
# No necesitas hacer nada — docs.rs detecta la publicacion
# y ejecuta cargo doc sobre tu crate.
```

```bash
# rustup doc — documentacion local (funciona offline):

rustup doc            # pagina principal
rustup doc --std      # biblioteca estandar
rustup doc --book     # The Rust Programming Language (libro oficial)
rustup doc --reference  # Rust Reference
rustup doc --cargo    # documentacion de Cargo

# Abrir la pagina de un tipo especifico:
rustup doc std::collections::HashMap
rustup doc std::vec::Vec

# La documentacion se instala con el toolchain via rustup.
# Especialmente util sin conexion a internet.
```

## Doc comments — ///

Los comentarios de documentacion con `///` documentan el item
que aparece inmediatamente despues. Se usan para funciones,
structs, enums, traits, constantes, y cualquier item publico:

```rust
/// Adds two numbers together.
///
/// Takes two `i32` values and returns their sum.
///
/// # Examples
///
/// ```
/// let result = my_crate::add(2, 3);
/// assert_eq!(result, 5);
/// ```
///
/// # Panics
///
/// Panics if the result overflows.
pub fn add(a: i32, b: i32) -> i32 {
    a.checked_add(b).expect("overflow")
}

// Los /// son azucar sintactico para el atributo #[doc = "..."]:
// Las dos formas siguientes son equivalentes:

/// This is a doc comment.
pub fn foo() {}

#[doc = "This is a doc comment."]
pub fn bar() {}
```

```rust
// Los doc comments soportan Markdown completo:

/// # Heading Level 1
/// ## Heading Level 2
///
/// Paragraph with **bold**, *italic*, and `inline code`.
///
/// - Bullet list item 1
/// - Bullet list item 2
///
/// 1. Numbered list
/// 2. Second item
///
/// ```rust
/// let x = 42; // code block (compiled as doc test)
/// ```
///
/// | Column A | Column B |
/// |----------|----------|
/// | Cell 1   | Cell 2   |
///
/// [Link to docs](https://doc.rust-lang.org/)
pub fn documented_function() {}
```

```rust
// Secciones estandar en doc comments.
// Convenciones reconocidas por la comunidad y las herramientas:

/// Divides two numbers.
///
/// # Examples
///
/// ```
/// let result = my_crate::divide(10, 3);
/// assert_eq!(result, Ok(3));
/// ```
///
/// # Errors
///
/// Returns `Err(String)` if `divisor` is zero.
///
/// # Panics
///
/// This function does not panic.
pub fn divide(dividend: i32, divisor: i32) -> Result<i32, String> {
    if divisor == 0 {
        Err("division by zero".to_string())
    } else {
        Ok(dividend / divisor)
    }
}
```

```rust
// La seccion # Safety es obligatoria para funciones unsafe:

/// Dereferences a raw pointer to an `i32`.
///
/// # Safety
///
/// The caller must ensure that:
/// - `ptr` is non-null
/// - `ptr` is properly aligned for `i32`
/// - `ptr` points to a valid, initialized `i32`
/// - No other mutable reference to the same memory exists
pub unsafe fn deref_raw(ptr: *const i32) -> i32 {
    *ptr
}

// Si omites # Safety en una funcion unsafe publica,
// clippy emitira un warning: missing_safety_doc.
```

```rust
// Documentar structs, sus campos y metodos:

/// A rectangle defined by its width and height.
///
/// # Examples
///
/// ```
/// let rect = my_crate::Rectangle::new(10.0, 5.0);
/// assert_eq!(rect.area(), 50.0);
/// ```
pub struct Rectangle {
    /// The width of the rectangle. Must be non-negative.
    pub width: f64,
    /// The height of the rectangle. Must be non-negative.
    pub height: f64,
}

impl Rectangle {
    /// Creates a new `Rectangle` with the given dimensions.
    ///
    /// # Panics
    ///
    /// Panics if `width` or `height` is negative.
    pub fn new(width: f64, height: f64) -> Self {
        assert!(width >= 0.0, "width must be non-negative");
        assert!(height >= 0.0, "height must be non-negative");
        Rectangle { width, height }
    }

    /// Returns the area of the rectangle.
    pub fn area(&self) -> f64 {
        self.width * self.height
    }
}
```

```rust
// Documentar enums — cada variante puede tener su propia documentacion:

/// Represents a color in different color spaces.
pub enum Color {
    /// An RGB color with red, green, and blue components (0-255 each).
    Rgb(u8, u8, u8),
    /// A named color (e.g., "red", "blue").
    Named(String),
    /// A transparent (invisible) color.
    Transparent,
}
```

## Doc comments de modulo — //!

Los comentarios `//!` documentan el **item que los contiene**
(el modulo o crate donde estan escritos). Se colocan al inicio
del archivo, antes de cualquier item:

```rust
// src/lib.rs — documenta el crate entero.
// Es la pagina principal de la documentacion (index.html).

//! # My Library
//!
//! `my_library` provides utilities for working with geometric shapes.
//!
//! ## Quick Start
//!
//! ```
//! use my_library::Rectangle;
//!
//! let rect = Rectangle::new(10.0, 5.0);
//! println!("Area: {}", rect.area());
//! ```
//!
//! ## Features
//!
//! - Create rectangles, circles, and triangles
//! - Calculate area, perimeter, and intersections

pub mod shapes;
pub mod utils;
```

```rust
// En mod.rs o archivos de modulo — documenta ese modulo:

//! Geometric shape types and their operations.
//!
//! This module contains the core shape types:
//! - [`Rectangle`] — axis-aligned rectangles
//! - [`Circle`] — circles defined by center and radius

pub mod rectangle;
pub mod circle;
```

```rust
// Diferencia entre /// y //!:

/// This documents the NEXT item (the function below).
pub fn next_item() {}

//! This documents the ENCLOSING item (the module/crate this file belongs to).

// Es un error comun confundirlos.
// //! va al INICIO del archivo, antes de los items.
// /// va JUSTO ANTES del item que documenta.

// //! tambien es azucar sintactico para un atributo:
//! Module documentation.
// equivale a:
#![doc = "Module documentation."]

// #![...] (con !) es un atributo interno (inner attribute) — aplica al contenedor.
// #[...] (sin !) es un atributo externo (outer attribute) — aplica al item siguiente.
```

## Ejemplos en la documentacion que se compilan

Esta es la caracteristica mas importante del sistema de
documentacion de Rust. Los bloques de codigo en doc comments
se compilan y ejecutan como tests:

```rust
/// Multiplies a number by two.
///
/// ```
/// assert_eq!(my_crate::double(5), 10);
/// assert_eq!(my_crate::double(0), 0);
/// assert_eq!(my_crate::double(-3), -6);
/// ```
pub fn double(x: i32) -> i32 {
    x * 2
}

// Cuando ejecutas cargo test, Rust extrae ese bloque de codigo,
// lo envuelve en una funcion main(), y lo compila como un programa
// independiente. Si compila y no hace panic → el test pasa.
```

```bash
# Ejecutar todos los tests (unitarios, integracion, y doc tests):
cargo test

# Ejecutar SOLO doc tests:
cargo test --doc

# La salida incluye una seccion Doc-tests:
# running 1 test
# test src/lib.rs - double (line 3) ... ok
# test result: ok. 1 passed; 0 failed; 0 ignored
```

```rust
// Si cambias la funcion (ej. x * 3 en vez de x * 2) y el doc test
// ya no es correcto, cargo test falla:
//
// test src/lib.rs - double (line 3) ... FAILED
//     'assertion `left == right` failed
//       left: 15
//      right: 10'
//
// La documentacion avisa de que el codigo cambio.
```

```rust
// Anotaciones especiales para bloques de codigo:

/// ```no_run
/// // Compila pero NO ejecuta.
/// // Util para ejemplos que acceden a la red o filesystem.
/// let data = std::fs::read_to_string("/etc/hostname").unwrap();
/// ```
///
/// ```ignore
/// // Ni compila ni ejecuta.
/// // Util para pseudocodigo o ejemplos incompletos.
/// let magic = do_something_undefined();
/// ```
///
/// ```compile_fail
/// // Verifica que el codigo NO compila.
/// // Util para demostrar errores de tipos.
/// let x: i32 = "not a number";
/// ```
///
/// ```should_panic
/// // Verifica que el codigo hace panic.
/// my_crate::divide(10, 0);
/// ```
pub fn annotations_example() {}
```

```rust
// Lineas ocultas con # (hash espacio).
// Se compilan pero no aparecen en la documentacion HTML.
// Util para setup, imports, y boilerplate que distrae:

/// Returns the larger of two values.
///
/// ```
/// # use my_crate::max;   // linea oculta — compilada pero no visible
/// #
/// let result = max(10, 20);
/// assert_eq!(result, 20);
/// ```
pub fn max(a: i32, b: i32) -> i32 {
    if a >= b { a } else { b }
}

// En la documentacion renderizada solo se ve:
//   let result = max(10, 20);
//   assert_eq!(result, 20);
// Pero al compilar, "use my_crate::max;" SI se incluye.

// Un main() oculto permite usar el operador ? en doc tests:

/// Parses a CSV line into fields.
///
/// ```
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let line = "Alice,30,Engineer";
/// let fields = my_crate::parse_csv_line(line)?;
/// assert_eq!(fields, vec!["Alice", "30", "Engineer"]);
/// # Ok(())
/// # }
/// ```
pub fn parse_csv_line(line: &str) -> Result<Vec<&str>, String> {
    Ok(line.split(',').collect())
}

// Sin el main() oculto, tendrias que usar .unwrap()
// en cada operacion falible.
```

```rust
// Cada bloque de codigo es un test independiente.
// Se ejecutan en paralelo, cada uno en su propio proceso.
// Variables y estado NO se comparten entre bloques.

/// ```
/// let x = 1;
/// assert_eq!(x, 1);
/// ```
///
/// ```
/// // x no existe aqui — es otro test.
/// let y = 2;
/// assert_eq!(y, 2);
/// ```
pub fn independent_tests() {}
```

## Links en la documentacion

Rustdoc soporta **intra-doc links**: enlaces a otros items del
codigo que se resuelven automaticamente a la URL correcta:

```rust
/// Adds an element to the [`Vec`].
///
/// This is similar to [`Vec::push`], but checks capacity first.
/// For hash-based storage, see [`std::collections::HashMap`].
///
/// See also the [`Widget`] struct and the [`process`] function.
pub fn add_element() {}

// Rustdoc resuelve estos enlaces al generar la documentacion:
// [`Vec`]          → link a std::vec::Vec
// [`Vec::push`]    → link al metodo Vec::push
// [`Widget`]       → link a Widget en el mismo crate
```

```rust
// Formas de intra-doc links:

/// - [`MyStruct`] — resuelve en el scope actual
/// - [`crate::module::MyStruct`] — path absoluto desde la raiz del crate
/// - [`super::other_module::Foo`] — path relativo al modulo padre
/// - [`MyStruct::method`] — metodo de un struct
/// - [`MyEnum::Variant`] — variante de un enum
/// - [link text][`MyStruct`] — texto personalizado con link a MyStruct
pub fn link_examples() {}

// Para desambiguar cuando un nombre se refiere a varios items:
/// See [`struct@Widget`] (not the module).
/// See [`mod@utils`] (not the function).
/// See [`macro@my_macro`] (not the function with the same name).
// Prefijos: struct@, enum@, trait@, type@, mod@, fn@, macro@, value@, field@
```

```rust
// Aliases de busqueda — para que los usuarios encuentren
// tu item con terminos alternativos:

/// Searches for elements matching a predicate.
#[doc(alias = "find")]
#[doc(alias = "search")]
pub fn query<T>(items: &[T], predicate: impl Fn(&T) -> bool) -> Option<&T> {
    items.iter().find(|item| predicate(item))
}

// En la documentacion generada, buscar "find" o "search"
// tambien encontrara esta funcion.
```

## Atributos de documentacion

```rust
// #[doc(hidden)] — oculta un item de la documentacion.
// El item sigue siendo publico y usable, pero no aparece en docs.
// Util para funciones publicas por razones tecnicas (macros, etc.)
// que no son parte de la API destinada al usuario.

#[doc(hidden)]
pub fn internal_helper() {}
```

```rust
// #[doc = include_str!(...)] — incluir documentacion desde un archivo externo.
// En lib.rs:
#![doc = include_str!("../README.md")]

// Inserta el contenido del README.md como documentacion del crate.
// Ventaja: el README se muestra en crates.io Y en docs.rs,
// sin duplicar contenido. Si editas el README, docs se actualiza.
```

```rust
// Lints de documentacion — warn y deny.
// Estos atributos van al inicio de lib.rs (inner attributes):

#![warn(missing_docs)]   // warning si un item publico no tiene documentacion
#![deny(missing_docs)]   // ERROR de compilacion si falta documentacion

//! My documented crate.

/// A documented function.
pub fn documented() {}

pub fn undocumented() {}
// Con warn: warning: missing documentation for a function
// Con deny: la compilacion FALLA. Obliga a documentar todo lo publico.

// Marcar items como deprecated:
/// Does something old.
#[deprecated(since = "2.0.0", note = "Use `new_function` instead")]
pub fn old_function() {}
// En docs aparece con etiqueta [Deprecated].
```

## Buenas practicas

```rust
// 1. Documentar TODOS los items publicos.
//    Usar #![warn(missing_docs)] en librerias.
//
// 2. Siempre incluir # Examples. Un ejemplo vale mas que tres
//    parrafos de explicacion. Cubrir el caso normal y edge cases.
//
// 3. La documentacion del crate (//! en lib.rs) debe funcionar
//    como una guia de inicio rapido.
//
// 4. Usar include_str! para mantener README.md y lib.rs sincronizados:
//    #![doc = include_str!("../README.md")]
//
// 5. Documentar errores posibles (# Errors) en funciones con Result.
//
// 6. Documentar condiciones de panic (# Panics).
//    Si una funcion puede hacer panic, el usuario DEBE saberlo.
//
// 7. Usar las anotaciones de code blocks apropiadamente:
//    ``` para ejemplos normales, ```no_run para I/O y red,
//    ```compile_fail para demostrar errores de tipos,
//    ```should_panic para verificar panics esperados.
```

## Resumen

```bash
# Comandos principales:
cargo doc                          # genera HTML (crate + dependencias)
cargo doc --open                   # genera y abre en navegador
cargo doc --no-deps                # solo tu crate
cargo doc --document-private-items # incluye items privados
cargo test --doc                   # ejecuta solo doc tests
rustup doc --std                   # docs offline de la biblioteca estandar
rustup doc --book                  # The Rust Programming Language offline
```

```rust
// Sintaxis de doc comments:
/// Item doc comment     — documenta el item SIGUIENTE
//! Module doc comment   — documenta el item que lo CONTIENE

/// ```                  — compila y ejecuta (doc test)
/// ```no_run            — compila pero no ejecuta
/// ```ignore            — ni compila ni ejecuta
/// ```compile_fail      — verifica que NO compila
/// ```should_panic      — verifica que hace panic
/// # hidden line        — se compila pero no se muestra en docs
/// [`Type`]             — intra-doc link a Type

#[doc(hidden)]           // ocultar de la documentacion
#[doc(alias = "find")]   // alias de busqueda
#[doc = include_str!("../README.md")]  // incluir archivo externo
#![warn(missing_docs)]   // warn si falta documentacion
#![deny(missing_docs)]   // error si falta documentacion
```

---

## Ejercicios

### Ejercicio 1 — Documentar una funcion basica

```rust
// Crear un proyecto con cargo new --lib doc_practice
// En src/lib.rs, escribir una funcion publica clamp que limite
// un valor a un rango [min, max].
//
// Requisitos:
//   1. Agregar #![warn(missing_docs)] al inicio de lib.rs
//   2. Agregar documentacion del crate con //! (titulo y descripcion breve)
//   3. Documentar la funcion con ///
//   4. Incluir las secciones: descripcion, # Examples, # Panics
//   5. El ejemplo debe usar assert_eq! para verificar el resultado
//   6. La funcion debe hacer panic si min > max
//
// Verificar:
//   cargo doc --open          → ver la documentacion en el navegador
//   cargo test --doc          → el ejemplo debe pasar
//   cargo test                → todos los tests deben pasar
//
// Firma de la funcion:
//   pub fn clamp(value: i32, min: i32, max: i32) -> i32
```

### Ejercicio 2 — Struct con documentacion completa

```rust
// En el mismo proyecto, agregar un struct Temperature con:
//   - Un campo privado celsius: f64
//   - Metodo new(celsius: f64) -> Self
//   - Metodo to_fahrenheit(&self) -> f64
//   - Metodo from_fahrenheit(f: f64) -> Self
//   - Metodo is_freezing(&self) -> bool
//
// Requisitos:
//   1. Documentar el struct y CADA metodo con ///
//   2. Incluir # Examples en cada metodo con assert!
//      (usar tolerancia para comparar floats: (a - b).abs() < f64::EPSILON)
//   3. new() debe hacer panic si celsius < -273.15 (cero absoluto)
//      → documentar esto en # Panics
//
// Verificar:
//   cargo doc --open          → navegar a Temperature y ver toda la documentacion
//   cargo test --doc          → todos los doc tests deben pasar
//   Quitar la documentacion de un metodo → verificar que warn(missing_docs) avisa
```

### Ejercicio 3 — Doc tests con anotaciones

```rust
// Agregar las siguientes funciones al proyecto y documentarlas
// con los tipos de code blocks indicados:
//
// 1. pub fn read_config(path: &str) -> Result<String, std::io::Error>
//    → Documentar con un ejemplo ```no_run (accede al filesystem)
//
// 2. pub fn parse_positive(s: &str) -> Result<u32, String>
//    → Documentar con un ejemplo normal que use ? (ocultar el main con #)
//    → Agregar un bloque ```compile_fail que demuestre que
//      no se puede pasar un numero negativo literal a u32
//
// 3. pub fn divide(a: i32, b: i32) -> i32
//    → Documentar con un ejemplo normal
//    → Agregar un bloque ```should_panic que demuestre la division por cero
//
// Verificar:
//   cargo test --doc → todos los doc tests deben pasar
//   Los compile_fail deben reportarse como "ok" (verifican que NO compila)
//   Los should_panic deben reportarse como "ok" (verifican el panic)
```

### Ejercicio 4 — Documentacion del crate y links

```rust
// Reorganizar el proyecto:
//   src/lib.rs      → documentacion del crate con //!, re-exports
//   src/math.rs     → mover clamp y divide aqui, con //! de modulo
//   src/temp.rs     → mover Temperature aqui, con //! de modulo
//   src/io.rs       → mover read_config y parse_positive aqui
//
// Requisitos:
//   1. La documentacion del crate (//! en lib.rs) debe incluir:
//      - Titulo y descripcion
//      - ## Modules — describir cada modulo con links: [`math`], [`temp`], [`io`]
//      - ## Quick Start — un ejemplo basico que use un item del crate
//   2. Usar intra-doc links en las funciones para referenciar tipos relacionados:
//      - En divide, enlazar a [`math::clamp`]
//      - En Temperature::new, enlazar a [`Temperature::from_fahrenheit`]
//   3. Agregar #[doc(alias = "limit")] a clamp
//   4. Agregar #[doc(alias = "celsius")] y #[doc(alias = "fahrenheit")] a Temperature
//
// Verificar:
//   cargo doc --open → navegar por los modulos, verificar que los links funcionan
//   cargo test --doc → todo pasa
//   Buscar "limit" en la documentacion → debe encontrar clamp
```
