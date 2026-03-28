# cargo-expand: ver código generado por macros

## Índice

1. [¿Qué es cargo-expand?](#qué-es-cargo-expand)
2. [Instalar y ejecutar](#instalar-y-ejecutar)
3. [Expandir derive macros](#expandir-derive-macros)
4. [Expandir macros declarativas (macro_rules!)](#expandir-macros-declarativas)
5. [Expandir módulos y archivos específicos](#expandir-módulos-y-archivos-específicos)
6. [Expandir macros procedurales](#expandir-macros-procedurales)
7. [Casos de uso: debugging de macros](#casos-de-uso-debugging-de-macros)
8. [Alternativas a cargo-expand](#alternativas-a-cargo-expand)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es cargo-expand?

`cargo-expand` es una herramienta que muestra el código Rust **después** de expandir todas las macros. Te permite ver exactamente qué código generan `#[derive(...)]`, `macro_rules!`, `println!`, `vec!`, y cualquier macro procedural.

```
┌──────────────────────────────────────────────────────┐
│        Lo que escribes vs lo que compila Rust        │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Tu código:                                          │
│  ┌──────────────────────────────────┐               │
│  │ #[derive(Debug, Clone)]          │               │
│  │ struct Point { x: f64, y: f64 }  │               │
│  └──────────────────────────────────┘               │
│          │                                           │
│          │ expansión de macros                       │
│          ▼                                           │
│  Código expandido (~50 líneas):                      │
│  ┌──────────────────────────────────┐               │
│  │ struct Point { x: f64, y: f64 }  │               │
│  │ impl ::core::fmt::Debug for Point│               │
│  │ {                                 │               │
│  │   fn fmt(&self, f: ...) { ... }   │               │
│  │ }                                 │               │
│  │ impl ::core::clone::Clone for ... │               │
│  │ { ... }                           │               │
│  └──────────────────────────────────┘               │
│                                                      │
│  cargo-expand te muestra este código expandido       │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### ¿Cuándo lo necesitas?

- **Debugging**: una macro genera código que no compila y no entiendes el error.
- **Aprendizaje**: quieres entender qué hace `#[derive(Debug)]` o `vec![]` internamente.
- **Desarrollo de macros**: estás escribiendo una macro y quieres verificar su output.
- **Auditoría**: quieres verificar qué código genera una macro de un crate externo.

---

## Instalar y ejecutar

### Instalación

```bash
# Instalar cargo-expand
cargo install cargo-expand

# Requiere nightly (para la flag -Zunpretty=expanded)
rustup toolchain install nightly
```

### Ejecución básica

```bash
# Expandir todo el crate (lib.rs o main.rs)
cargo expand

# Expandir usando nightly explícitamente
cargo +nightly expand

# Expandir un módulo específico
cargo expand module_name

# Expandir un item específico
cargo expand module_name::function_name

# Expandir tests
cargo expand --tests

# Expandir un ejemplo
cargo expand --example my_example
```

### Ejemplo rápido

```rust
// src/main.rs
fn main() {
    let v = vec![1, 2, 3];
    println!("vector: {:?}", v);
}
```

```bash
$ cargo expand
```

```rust
#![feature(prelude_import)]
#[prelude_import]
use std::prelude::rust_2021::*;
#[macro_use]
extern crate std;
fn main() {
    let v = <[_]>::into_vec(
        #[rustc_box]
        ::alloc::boxed::Box::new([1, 2, 3]),
    );
    {
        ::std::io::_print(
            format_args!("vector: {0:?}\n", v),
        );
    };
}
```

Ahora ves qué hacen `vec![]` y `println!` internamente:
- `vec![1, 2, 3]` → crea un `Box<[i32; 3]>` y lo convierte a `Vec`.
- `println!(...)` → `::std::io::_print(format_args!(...))`.

---

## Expandir derive macros

### #[derive(Debug)]

```rust
#[derive(Debug)]
struct User {
    name: String,
    age: u32,
    active: bool,
}
```

```bash
$ cargo expand
```

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

impl ::core::fmt::Debug for User {
    #[inline]
    fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
        ::core::fmt::Formatter::debug_struct_field3_finish(
            f,
            "User",
            "name", &self.name,
            "age", &self.age,
            "active", &&self.active,
        )
    }
}
```

### #[derive(Clone)]

```rust
#[derive(Clone)]
struct Config {
    name: String,
    values: Vec<i32>,
}
```

Expandido:

```rust
impl ::core::clone::Clone for Config {
    #[inline]
    fn clone(&self) -> Config {
        Config {
            name: ::core::clone::Clone::clone(&self.name),
            values: ::core::clone::Clone::clone(&self.values),
        }
    }
}
```

### #[derive(PartialEq, Eq)]

```rust
#[derive(PartialEq, Eq)]
enum Color {
    Red,
    Green,
    Blue,
}
```

Expandido:

```rust
impl ::core::marker::StructuralPartialEq for Color {}

impl ::core::cmp::PartialEq for Color {
    #[inline]
    fn eq(&self, other: &Color) -> bool {
        let __self_tag = ::core::intrinsics::discriminant_value(self);
        let __arg1_tag = ::core::intrinsics::discriminant_value(other);
        __self_tag == __arg1_tag
    }
}

impl ::core::cmp::Eq for Color {
    #[inline]
    fn assert_receiver_is_total_eq(&self) {}
}
```

> **Predicción**: ¿Qué genera `#[derive(Default)]` para un struct donde todos los campos tienen `Default`? Genera un `impl Default` que llama `Default::default()` para cada campo.

### Múltiples derives a la vez

```rust
#[derive(Debug, Clone, PartialEq, Hash)]
struct Point {
    x: f64,
    y: f64,
}
```

```bash
$ cargo expand
# Genera implementaciones separadas para cada trait:
# impl Debug for Point { ... }
# impl Clone for Point { ... }
# impl PartialEq for Point { ... }
# impl Hash for Point { ... }
```

---

## Expandir macros declarativas

### macro_rules! simples

```rust
macro_rules! say_hello {
    ($name:expr) => {
        println!("Hello, {}!", $name);
    };
}

fn main() {
    say_hello!("world");
}
```

Expandido:

```rust
fn main() {
    {
        ::std::io::_print(format_args!("Hello, {0}!\n", "world"));
    };
}
```

### vec! con diferentes formas

```rust
fn main() {
    let a = vec![1, 2, 3];           // lista de elementos
    let b = vec![0; 10];              // repetición
    let c = vec!["hello".to_string()]; // un elemento con expresión
}
```

Expandido:

```rust
fn main() {
    let a = <[_]>::into_vec(
        #[rustc_box]
        ::alloc::boxed::Box::new([1, 2, 3]),
    );
    let b = ::alloc::vec::from_elem(0, 10);
    let c = <[_]>::into_vec(
        #[rustc_box]
        ::alloc::boxed::Box::new(["hello".to_string()]),
    );
}
```

`vec![elem; count]` usa una función diferente (`from_elem`) que `vec![a, b, c]` (`Box::new([...]).into_vec()`).

### assert! y sus variantes

```rust
fn main() {
    let x = 42;
    assert!(x > 0);
    assert_eq!(x, 42);
    assert_ne!(x, 0);
}
```

Expandido:

```rust
fn main() {
    let x = 42;

    if !(x > 0) {
        ::core::panicking::panic("assertion failed: x > 0");
    }

    match (&x, &42) {
        (left_val, right_val) => {
            if !(*left_val == *right_val) {
                let kind = ::core::panicking::AssertKind::Eq;
                ::core::panicking::assert_failed(
                    kind, &*left_val, &*right_val, ::core::option::Option::None,
                );
            }
        }
    };

    match (&x, &0) {
        (left_val, right_val) => {
            if !(*left_val != *right_val) {
                let kind = ::core::panicking::AssertKind::Ne;
                ::core::panicking::assert_failed(
                    kind, &*left_val, &*right_val, ::core::option::Option::None,
                );
            }
        }
    };
}
```

Observa que `assert_eq!` y `assert_ne!` usan `match` para evaluar cada lado **una sola vez** y almacenar los valores para el mensaje de error.

---

## Expandir módulos y archivos específicos

### Expandir un módulo

```rust
// src/lib.rs
mod math;
mod utils;
```

```bash
# Solo expandir el módulo math
cargo expand math

# Solo expandir una función dentro de math
cargo expand math::calculate
```

### Expandir tests de un módulo

```rust
// src/lib.rs
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic() {
        assert_eq!(2 + 2, 4);
    }
}
```

```bash
$ cargo expand --tests tests
```

```rust
mod tests {
    use super::*;

    #[test]
    fn test_basic() {
        match (&(2 + 2), &4) {
            (left_val, right_val) => {
                if !(*left_val == *right_val) {
                    // ... assert_failed ...
                }
            }
        };
    }
}
```

El atributo `#[test]` se expande a la maquinaria del test runner (no visible directamente con expand, pero las macros dentro del test sí se expanden).

---

## Expandir macros procedurales

Las macros procedurales (proc macros) de crates externos también se expanden.

### serde: #[derive(Serialize, Deserialize)]

```toml
[dependencies]
serde = { version = "1", features = ["derive"] }
```

```rust
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug)]
struct Config {
    name: String,
    port: u16,
    debug: bool,
}
```

```bash
$ cargo expand
```

El output muestra la implementación completa de `Serialize` y `Deserialize` — puede ser cientos de líneas para un struct simple. Esto es útil para entender:

- Cómo serde mapea campos a claves JSON.
- Qué visitor patterns usa para deserializar.
- Cómo maneja `Option`, `Vec`, tipos anidados.

### thiserror: #[derive(Error)]

```toml
[dependencies]
thiserror = "2"
```

```rust
use thiserror::Error;

#[derive(Error, Debug)]
enum AppError {
    #[error("file not found: {0}")]
    NotFound(String),

    #[error("parse error at line {line}: {message}")]
    ParseError { line: u32, message: String },

    #[error(transparent)]
    IoError(#[from] std::io::Error),
}
```

```bash
$ cargo expand
```

Verás que `thiserror` genera:
- `impl Display for AppError` con los mensajes formateados.
- `impl Error for AppError` con `source()` para la cadena de errores.
- `impl From<std::io::Error> for AppError` por el `#[from]`.

### clap: #[derive(Parser)]

```toml
[dependencies]
clap = { version = "4", features = ["derive"] }
```

```rust
use clap::Parser;

#[derive(Parser, Debug)]
#[command(name = "myapp")]
struct Args {
    /// Input file path
    #[arg(short, long)]
    input: String,

    /// Verbosity level
    #[arg(short, long, default_value_t = 0)]
    verbose: u8,
}
```

`cargo expand` mostrará toda la maquinaria de parsing de argumentos que clap genera — útil para entender cómo mapea argumentos CLI a campos del struct.

---

## Casos de uso: debugging de macros

### Caso 1: error de compilación dentro de una macro

```rust
macro_rules! make_getter {
    ($name:ident, $field:ident, $type:ty) => {
        fn $name(&self) -> $type {
            self.$field  // ¿Y si el campo no existe?
        }
    };
}

struct Data {
    value: i32,
}

impl Data {
    make_getter!(get_value, value, i32);
    make_getter!(get_name, name, String);  // 'name' no existe → error críptico
}
```

El error del compilador apunta a la macro, no al uso. Con cargo-expand ves el código generado y el error se vuelve obvio:

```bash
$ cargo expand
```

```rust
impl Data {
    fn get_value(&self) -> i32 {
        self.value   // ✓ existe
    }
    fn get_name(&self) -> String {
        self.name    // ✗ Data no tiene campo 'name'
    }
}
```

### Caso 2: entender qué genera un derive externo

```rust
#[derive(Debug)]
enum State {
    Loading { progress: f32 },
    Ready(String),
    Error { code: u32, message: String },
}
```

¿Cómo implementa `Debug` un enum con variantes distintas? `cargo expand` lo muestra:

```rust
impl ::core::fmt::Debug for State {
    fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
        match self {
            State::Loading { progress: __self_0 } => {
                ::core::fmt::Formatter::debug_struct_field1_finish(
                    f, "Loading", "progress", &__self_0,
                )
            }
            State::Ready(__self_0) => {
                ::core::fmt::Formatter::debug_tuple_field1_finish(
                    f, "Ready", &__self_0,
                )
            }
            State::Error { code: __self_0, message: __self_1 } => {
                ::core::fmt::Formatter::debug_struct_field2_finish(
                    f, "Error", "code", __self_0, "message", &__self_1,
                )
            }
        }
    }
}
```

### Caso 3: verificar que tu macro genera código correcto

```rust
macro_rules! impl_from_str {
    ($type:ty, $parse_fn:expr) => {
        impl std::str::FromStr for $type {
            type Err = String;

            fn from_str(s: &str) -> Result<Self, Self::Err> {
                $parse_fn(s).map_err(|e| format!("parse error: {}", e))
            }
        }
    };
}

struct Celsius(f64);

impl_from_str!(Celsius, |s: &str| -> Result<Celsius, std::num::ParseFloatError> {
    s.parse::<f64>().map(Celsius)
});
```

`cargo expand` te muestra si la macro generó el `impl FromStr` correctamente, con los tipos y traits resueltos.

---

## Alternativas a cargo-expand

### rustc -Zunpretty=expanded (directo)

```bash
# cargo-expand usa esto internamente
rustc +nightly -Zunpretty=expanded src/main.rs

# Ventaja: no necesita instalar cargo-expand
# Desventaja: no resuelve dependencias automáticamente
```

### rust-analyzer: Expand Macro en el IDE

En VS Code con rust-analyzer:
1. Pon el cursor sobre una invocación de macro.
2. Abre la paleta de comandos (Ctrl+Shift+P).
3. Busca "Rust Analyzer: Expand Macro Recursively".
4. Se abre una ventana con el código expandido.

```
┌──────────────────────────────────────────────────┐
│  VS Code: Expand Macro                           │
│                                                  │
│  Cursor en: vec![1, 2, 3]                        │
│  Ctrl+Shift+P → "Expand Macro Recursively"       │
│                                                  │
│  Resultado en panel:                             │
│  <[_]>::into_vec(Box::new([1, 2, 3]))            │
│                                                  │
│  Ventaja: inline, sin salir del editor           │
│  Desventaja: a veces menos completo que expand    │
└──────────────────────────────────────────────────┘
```

### cargo-expand con colores

```bash
# Por defecto, cargo-expand intenta colorear con rustfmt
# Si no está instalado:
rustup component add rustfmt

# Sin colores (para redireccionar a archivo):
cargo expand --color=never > expanded.rs

# Guardar a archivo para comparar
cargo expand > before.rs
# ... hacer cambios a la macro ...
cargo expand > after.rs
diff before.rs after.rs
```

---

## Errores comunes

### 1. No tener nightly instalado

```bash
$ cargo expand
error: cargo-expand requires a nightly toolchain

# Solución:
$ rustup toolchain install nightly
$ cargo +nightly expand
# O:
$ cargo expand  # usa nightly automáticamente si está instalado
```

### 2. Output demasiado grande para leer

```bash
# Un crate con muchos derives genera miles de líneas
$ cargo expand | wc -l
5432  # imposible de leer

# Solución: expandir solo el módulo o item que te interesa
$ cargo expand my_module
$ cargo expand my_module::MyStruct

# O buscar un item específico en el output
$ cargo expand | grep -A 20 "impl Debug for MyStruct"
```

### 3. Confundir código expandido con código que debes escribir

```bash
# cargo-expand muestra código generado por el compilador.
# NO es código que tú debas escribir o mantener.
# Es una herramienta de INSPECCIÓN, no de generación.

# Si ves:
impl ::core::fmt::Debug for MyStruct { ... }
# No copies esto a tu código. Usa #[derive(Debug)] en su lugar.
```

### 4. Macros que generan macros (expansión parcial)

```rust
macro_rules! outer {
    ($name:ident) => {
        macro_rules! inner {
            () => { stringify!($name) };
        }
    };
}

outer!(hello);
```

`cargo expand` expande `outer!` pero puede dejar `inner!` sin expandir si no se invoca. Solo se expanden las macros que se **usan**.

### 5. No usar --tests para ver expansión de macros de test

```rust
#[cfg(test)]
mod tests {
    #[test]
    fn my_test() {
        assert_eq!(1 + 1, 2);
    }
}
```

```bash
# Sin --tests, el módulo #[cfg(test)] no se incluye
$ cargo expand
# El módulo tests no aparece

# Con --tests:
$ cargo expand --tests
# Ahora sí aparece el módulo tests con assert_eq! expandido
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║            CARGO-EXPAND — REFERENCIA RÁPIDA              ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  INSTALAR                                                 ║
║  ────────                                                 ║
║  cargo install cargo-expand                               ║
║  rustup toolchain install nightly                         ║
║                                                           ║
║  EJECUTAR                                                 ║
║  ────────                                                 ║
║  cargo expand                    todo el crate            ║
║  cargo expand modulo             un módulo                ║
║  cargo expand modulo::item       un item específico       ║
║  cargo expand --tests            incluir tests            ║
║  cargo expand --example name     un ejemplo               ║
║                                                           ║
║  SALIDA                                                   ║
║  ──────                                                   ║
║  cargo expand > expanded.rs      guardar a archivo        ║
║  cargo expand | grep -A 20 "impl Debug"   buscar item    ║
║  cargo expand --color=never      sin colores              ║
║                                                           ║
║  QUÉ EXPANDE                                              ║
║  ────────────                                             ║
║  #[derive(Debug, Clone, ...)]    derive macros            ║
║  vec![], println![], assert![]   macros del std           ║
║  macro_rules! invocaciones       macros declarativas      ║
║  #[derive(Serialize, Parser)]    proc macros externas     ║
║  #[tokio::main]                  attribute macros          ║
║                                                           ║
║  ALTERNATIVAS                                             ║
║  ────────────                                             ║
║  rust-analyzer "Expand Macro"    en el IDE (inline)       ║
║  rustc +nightly -Zunpretty=expanded    sin cargo-expand  ║
║                                                           ║
║  CASOS DE USO                                             ║
║  ────────────                                             ║
║  Error críptico de macro    → ver el código generado      ║
║  Entender un derive         → inspeccionar la impl       ║
║  Desarrollar macros         → verificar el output        ║
║  Auditar crate externo      → ver qué genera             ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Explorar derive macros estándar

Crea un struct y aplica diferentes combinaciones de derives. Usa `cargo expand` para responder estas preguntas:

```rust
#[derive(Debug, Clone, PartialEq, Eq, Hash, Default)]
struct Student {
    name: String,
    grades: Vec<u8>,
    graduated: bool,
}

#[derive(Debug, Clone, PartialEq)]
enum Grade {
    A,
    B,
    C,
    F,
    Numeric(u8),
}
```

Preguntas a responder con `cargo expand`:

1. ¿Cómo implementa `Debug` un struct con 3 campos? ¿Usa `debug_struct`?
2. ¿Cómo implementa `Clone` un struct con `Vec`? ¿Clona cada campo individualmente?
3. ¿Cómo implementa `PartialEq` un enum con variantes con datos vs sin datos?
4. ¿Qué genera `Default` para cada campo? ¿`String::default()` o `"".to_string()`?
5. ¿Cómo implementa `Hash` un struct? ¿Hashea cada campo por separado?

**Pregunta de reflexión**: si implementaras `Debug` manualmente para `Student`, ¿escribirías algo diferente a lo que genera derive? ¿Cuándo tiene sentido implementar manualmente en vez de usar derive?

---

### Ejercicio 2: Debugging de una macro con error

Esta macro tiene un bug sutil. Usa `cargo expand` para encontrarlo:

```rust
macro_rules! builder {
    ($name:ident { $($field:ident : $type:ty),* $(,)? }) => {
        struct $name {
            $($field: $type,)*
        }

        impl $name {
            fn new() -> Self {
                $name {
                    $($field: Default::default(),)*
                }
            }

            $(
                fn $field(mut self, value: $type) -> Self {
                    self.$field = value;
                    self
                }
            )*
        }
    };
}

builder!(Config {
    host: String,
    port: u16,
    debug: bool,
});

fn main() {
    let config = Config::new()
        .host("localhost".to_string())
        .port(8080)
        .debug(true);

    // ¿Compila? Si no, usa cargo expand para ver qué genera
    // y encuentra el problema
    println!("host: {}", config.host);
}
```

Pasos:
1. Intenta compilar. ¿Funciona?
2. Ejecuta `cargo expand` y busca la `impl Config`.
3. Verifica que todos los métodos builder están generados correctamente.
4. Si hay un bug (ej: conflicto de nombres, tipo incorrecto), identifícalo en el código expandido.
5. Arregla la macro.

**Pregunta de reflexión**: ¿Qué pasaría si un campo se llamara `new`? ¿La macro generaría un método `new` builder que conflictuaría con el constructor? ¿Cómo lo evitarías?

---

### Ejercicio 3: Comparar implementaciones derive vs manual

Implementa `Display` manualmente y `Debug` con derive para el mismo tipo. Usa `cargo expand` para comparar:

```rust
use std::fmt;

#[derive(Debug, Clone)]
struct Color {
    r: u8,
    g: u8,
    b: u8,
    name: Option<String>,
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.name {
            Some(name) => write!(f, "{}(#{:02X}{:02X}{:02X})", name, self.r, self.g, self.b),
            None => write!(f, "#{:02X}{:02X}{:02X}", self.r, self.g, self.b),
        }
    }
}

fn main() {
    let red = Color { r: 255, g: 0, b: 0, name: Some("Red".to_string()) };
    let custom = Color { r: 128, g: 64, b: 32, name: None };

    // Debug (derive):
    println!("{:?}", red);    // Color { r: 255, g: 0, b: 0, name: Some("Red") }
    // Display (manual):
    println!("{}", red);      // Red(#FF0000)
    println!("{}", custom);   // #804020
}
```

Usa `cargo expand` para:
1. Ver la implementación generada de `Debug` y `Clone`.
2. Comparar tu `impl Display` manual con el `impl Debug` generado.
3. ¿Cuántas líneas genera `#[derive(Debug)]`? ¿Cuántas escribiste para `Display`?
4. Intenta añadir `#[derive(PartialEq)]` y observa cómo compara `Option<String>`.

**Pregunta de reflexión**: el derive de `Debug` produce output como `Color { r: 255, g: 0, b: 0, name: Some("Red") }`. ¿Es útil para debugging pero no para mostrar al usuario? ¿Por qué Rust separa `Debug` y `Display` en dos traits distintos?
