# Macros del ecosistema: cómo leer vec![], println![], assert_eq![]

## Índice
1. [Por qué leer macros ajenas](#1-por-qué-leer-macros-ajenas)
2. [vec![]](#2-vec)
3. [println![] y format![]](#3-println-y-format)
4. [assert!, assert_eq!, assert_ne!](#4-assert-assert_eq-assert_ne)
5. [dbg![]](#5-dbg)
6. [todo!, unimplemented!, unreachable!](#6-todo-unimplemented-unreachable)
7. [cfg! y compile_error!](#7-cfg-y-compile_error)
8. [include_str! e include_bytes!](#8-include_str-e-include_bytes)
9. [Expandir macros con cargo-expand](#9-expandir-macros-con-cargo-expand)
10. [Macros procedurales: derive, attribute, function-like](#10-macros-procedurales-derive-attribute-function-like)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Por qué leer macros ajenas

Usas macros todo el tiempo en Rust — `vec!`, `println!`, `assert_eq!`, `#[derive(Debug)]`, `#[tokio::main]`. Entender **cómo funcionan internamente** te permite:

- Predecir su comportamiento en casos edge.
- Entender los errores de compilación que generan.
- Saber cuándo una macro es la herramienta correcta.
- Escribir mejores macros propias.

Este tópico descompone las macros más usadas del lenguaje y del ecosistema.

---

## 2. vec![]

`vec!` es probablemente la primera macro que todo rustacean usa. Crea un `Vec<T>` con elementos iniciales:

### Uso

```rust
let a = vec![1, 2, 3];           // Vec<i32> con 3 elementos
let b = vec![0; 10];             // Vec<i32> con 10 ceros
let c: Vec<String> = vec![];     // Vec vacío
```

### Implementación (simplificada)

```rust
// La implementación real en std es algo así:
macro_rules! vec {
    // Caso 1: vacío
    () => {
        Vec::new()
    };
    // Caso 2: repetir un elemento N veces
    ($elem:expr; $n:expr) => {
        // vec![0; 10] → crea 10 copias de 0
        {
            let mut v = Vec::with_capacity($n);
            v.resize($n, $elem);  // requiere Clone
            v
        }
        // En realidad usa: <[_]>::into_vec(Box::new([$elem; $n]))
        // o un mecanismo más eficiente
    };
    // Caso 3: lista de elementos
    ($($x:expr),+ $(,)?) => {
        // vec![1, 2, 3] → crea Vec con estos elementos
        {
            let mut v = Vec::new();
            $( v.push($x); )+
            v
        }
        // En realidad usa: <[_]>::into_vec(Box::new([$($x),+]))
        // que es más eficiente (una sola allocación)
    };
}
```

### Lo que la implementación real hace diferente

La std usa `<[_]>::into_vec(Box::new([...]))` en lugar de push repetido, lo que:
- Hace **una sola allocación** del tamaño exacto (no múltiples grows).
- Es más eficiente en runtime.
- El `[_]` le dice al compilador que infiera el tipo del array.

### Detalles importantes

```rust
// vec![expr; n] requiere que expr implemente Clone
let v = vec![String::from("hi"); 3];
// Crea un String, luego lo clona 2 veces → 3 Strings

// vec![expr1, expr2, ...] NO requiere Clone
let v = vec![String::from("a"), String::from("b")];
// Cada expresión se evalúa independientemente

// Ambos aceptan trailing comma
let v = vec![1, 2, 3,];  // OK
```

---

## 3. println![] y format![]

### La familia format

Rust tiene varias macros de formateo que comparten la misma sintaxis:

```rust
format!("Hello, {}!", name)        // → String
print!("Hello, {}!", name)         // → stdout, sin newline
println!("Hello, {}!", name)       // → stdout, con newline
eprint!("Error: {}", msg)          // → stderr, sin newline
eprintln!("Error: {}", msg)        // → stderr, con newline
write!(buf, "Data: {}", val)       // → impl Write
writeln!(buf, "Data: {}", val)     // → impl Write + newline
```

### Cómo funciona el format string

El format string es **verificado en compilación** — no es un string normal:

```rust
// El compilador verifica en compilación:
println!("{} + {} = {}", 1, 2, 3);  // OK: 3 placeholders, 3 args
// println!("{} + {}", 1, 2, 3);    // ERROR: 2 placeholders, 3 args
// println!("{} + {} = {}", 1, 2);  // ERROR: 3 placeholders, 2 args
```

Esto es posible porque `println!` es una macro — puede analizar el string literal en compilación. Una función normal no podría hacerlo.

### Formato interno: los traits de fmt

Cada placeholder `{}` llama a un trait específico:

```rust
println!("{}", val);    // Display    → fmt::Display::fmt
println!("{:?}", val);  // Debug      → fmt::Debug::fmt
println!("{:#?}", val); // Debug pretty → fmt::Debug::fmt (con formato)
println!("{:b}", val);  // Binary     → fmt::Binary::fmt
println!("{:o}", val);  // Octal      → fmt::Octal::fmt
println!("{:x}", val);  // Hex lower  → fmt::LowerHex::fmt
println!("{:X}", val);  // Hex upper  → fmt::UpperHex::fmt
println!("{:e}", val);  // Sci lower  → fmt::LowerExp::fmt
println!("{:p}", &val); // Pointer    → fmt::Pointer::fmt
```

### Opciones de formato

```rust
// Ancho y alineación
println!("{:>10}", "right");    // "     right"
println!("{:<10}", "left");     // "left      "
println!("{:^10}", "center");   // "  center  "
println!("{:*>10}", "fill");    // "******fill"

// Precisión
println!("{:.2}", 3.14159);     // "3.14"
println!("{:.5}", "hello world"); // "hello"

// Ancho + precisión
println!("{:10.2}", 3.14159);   // "      3.14"

// Con nombre
println!("{name} is {age}", name = "Alice", age = 30);

// Captura directa de variables (Rust 1.58+)
let name = "Alice";
let age = 30;
println!("{name} is {age}");    // captura las variables directamente

// Posicional
println!("{0} + {0} = {1}", 5, 10);  // "5 + 5 = 10"

// Cero-padding
println!("{:05}", 42);          // "00042"
println!("{:08b}", 42);         // "00101010"

// Signo
println!("{:+}", 42);           // "+42"
println!("{:+}", -42);          // "-42"
```

### La expansión real

`println!("Hello, {}!", name)` se expande aproximadamente a:

```rust
{
    ::std::io::_print(
        ::core::fmt::Arguments::new_v1(
            &["Hello, ", "!\n"],        // fragmentos literales
            &[::core::fmt::ArgumentV1::new_display(&name)],  // argumentos
        )
    );
}
```

Los fragmentos literales y los argumentos se almacenan por separado, y `Arguments` los intercala al escribir. Esto evita allocaciones — no se crea un `String` intermedio.

---

## 4. assert!, assert_eq!, assert_ne!

### assert!

```rust
assert!(condition);
assert!(condition, "message");
assert!(condition, "formatted {}", message);
```

Expansión conceptual:

```rust
// assert!(x > 0) se expande a algo como:
if !x > 0 {
    panic!("assertion failed: x > 0");
}

// assert!(x > 0, "x must be positive, got {}", x) →
if !x > 0 {
    panic!("x must be positive, got {}", x);
}
```

### assert_eq! y assert_ne!

```rust
assert_eq!(left, right);
assert_ne!(left, right);
```

La diferencia clave con `assert!` es que al fallar, **muestran ambos valores**:

```rust
assert_eq!(2 + 2, 5);
// panic: assertion `left == right` failed
//   left: 4
//  right: 5
```

Expansión conceptual:

```rust
// assert_eq!(a, b) se expande a algo como:
match (&a, &b) {
    (left_val, right_val) => {
        if !(*left_val == *right_val) {
            panic!(
                "assertion `left == right` failed\n  left: {:?}\n right: {:?}",
                left_val, right_val
            );
        }
    }
}
```

### Por qué son macros y no funciones

1. **Evaluación lazy del mensaje**: `assert!(cond, "expensive {}", compute())` — `compute()` solo se ejecuta si la aserción falla.
2. **Mostrar la expresión original**: el mensaje incluye `"assertion failed: x > 0"` con el código fuente.
3. **Requiere tanto `PartialEq` como `Debug`**: `assert_eq!` necesita comparar (PartialEq) Y mostrar (Debug) — una función necesitaría ambos bounds explícitamente.
4. **Se eliminan en release**: `debug_assert!`, `debug_assert_eq!` y `debug_assert_ne!` solo existen en debug builds:

```rust
debug_assert!(expensive_check());  // solo en debug, eliminado en release
```

---

## 5. dbg![]

`dbg!` es una macro de depuración que imprime la expresión, su valor, y la ubicación en el código:

```rust
let x = 5;
let y = dbg!(x * 2);  // imprime a stderr Y retorna el valor
// [src/main.rs:3:9] x * 2 = 10
// y = 10
```

### Expansión conceptual

```rust
// dbg!(expr) se expande a algo como:
match expr {
    val => {
        eprintln!(
            "[{}:{}:{}] {} = {:#?}",
            file!(),
            line!(),
            column!(),
            stringify!(expr),
            &val
        );
        val  // retorna el valor (move semantics)
    }
}
```

### Características importantes

```rust
// Retorna el valor — se puede encadenar:
let result = dbg!(dbg!(2) + dbg!(3));
// [src/main.rs:1] 2 = 2
// [src/main.rs:1] 3 = 3
// [src/main.rs:1] dbg!(2) + dbg!(3) = 5

// Imprime a stderr, no stdout:
// Puedes redirigir stdout sin perder los mensajes de dbg!

// Múltiples argumentos (retorna tupla):
let (a, b) = dbg!(1, 2);
// [src/main.rs:1] 1 = 1
// [src/main.rs:1] 2 = 2

// Sin argumentos (útil como "llegué aquí"):
dbg!();
// [src/main.rs:1]

// TOMA OWNERSHIP — cuidado con tipos no Copy:
let s = String::from("hello");
let s = dbg!(s);  // mueve s, lo retorna
// println!("{}", s);  // OK, dbg! retornó s
```

### dbg! vs println!

```rust
// println! necesita Display, no retorna valor:
println!("{}", x);  // necesita Display, no retorna

// dbg! usa Debug, retorna el valor:
let y = dbg!(x);   // necesita Debug, retorna x

// dbg! muestra el nombre de la expresión automáticamente
// println! necesitas escribir el contexto manualmente
```

---

## 6. todo!, unimplemented!, unreachable!

### todo!

Placeholder para código no implementado aún. Siempre hace panic:

```rust
fn parse_config(path: &str) -> Config {
    todo!()  // panic: "not yet implemented"
}

fn process(data: &[u8]) -> Vec<u8> {
    todo!("implement data processing for format v2")
    // panic: "not yet implemented: implement data processing for format v2"
}
```

`todo!()` tiene tipo `!` (never type), así que satisface cualquier tipo de retorno. Es la forma idiomática de dejar funciones pendientes mientras desarrollas:

```rust
trait Handler {
    fn handle(&self, req: Request) -> Response;
}

impl Handler for MyHandler {
    fn handle(&self, req: Request) -> Response {
        todo!()  // compila, permite trabajar en otra parte del código
    }
}
```

### unimplemented!

Similar a `todo!` pero con una connotación distinta — indica algo que **no se planea** implementar:

```rust
fn serialize(&self) -> Vec<u8> {
    unimplemented!("XML serialization is not supported")
}
```

En la práctica, `todo!` y `unimplemented!` generan el mismo panic. La diferencia es semántica:
- `todo!()` → "aún no lo hice, pero lo haré"
- `unimplemented!()` → "no voy a implementar esto (conscientemente)"

### unreachable!

Indica código que **nunca debería ejecutarse**. Si se ejecuta, hay un bug:

```rust
fn classify(n: i32) -> &'static str {
    if n > 0 {
        "positive"
    } else if n < 0 {
        "negative"
    } else if n == 0 {
        "zero"
    } else {
        unreachable!()  // matemáticamente imposible llegar aquí
    }
}

// Más útil en matches:
fn process(value: u8) -> &'static str {
    match value {
        0..=127 => "low",
        128..=255 => "high",
        // u8 cubre 0..=255 completamente, pero si el compilador no lo sabe:
        // _ => unreachable!()
    }
}
```

La versión `debug_assert!` existe como `unreachable!` solo compila en debug:

```rust
// unsafe_unreachable! no existe, pero sí:
unsafe { std::hint::unreachable_unchecked() }
// UB si se alcanza — solo para optimización extrema, casi nunca lo necesitas
```

---

## 7. cfg! y compile_error!

### cfg!

La macro `cfg!` evalúa condiciones de compilación como una expresión booleana:

```rust
fn main() {
    if cfg!(target_os = "linux") {
        println!("Running on Linux");
    } else if cfg!(target_os = "windows") {
        println!("Running on Windows");
    }

    if cfg!(debug_assertions) {
        println!("Debug mode");
    }

    let threaded = cfg!(feature = "multi-thread");
    println!("Multi-threaded: {}", threaded);
}
```

**Diferencia con `#[cfg(...)]`**: el atributo `#[cfg]` **elimina** código. La macro `cfg!()` lo mantiene pero evalúa a `true`/`false`. Con `cfg!()`, ambas ramas deben compilar:

```rust
// #[cfg] elimina código:
#[cfg(feature = "json")]
fn parse_json() { ... }  // no existe sin la feature

// cfg!() no elimina nada:
if cfg!(feature = "json") {
    parse_json();  // DEBE compilar incluso sin la feature
                   // ERROR si parse_json no existe
}
```

### compile_error!

Fuerza un error de compilación con un mensaje personalizado. Útil dentro de macros y `cfg`:

```rust
#[cfg(not(any(feature = "postgres", feature = "sqlite")))]
compile_error!("At least one database backend must be enabled. Use --features postgres or --features sqlite");
```

Dentro de una macro:

```rust
macro_rules! validate {
    (positive $val:expr) => { /* ok */ };
    (negative $val:expr) => { /* ok */ };
    ($other:tt $val:expr) => {
        compile_error!(concat!("Unknown validator: ", stringify!($other)));
    };
}

validate!(positive 42);   // OK
// validate!(unknown 42); // ERROR de compilación: "Unknown validator: unknown"
```

---

## 8. include_str! e include_bytes!

### include_str!

Incluye el contenido de un archivo como `&'static str` en compilación:

```rust
// Incluye el archivo completo como string en el binario
const SQL_SCHEMA: &str = include_str!("schema.sql");
const README: &str = include_str!("../README.md");

fn main() {
    println!("{}", SQL_SCHEMA);
}
```

La ruta es **relativa al archivo** donde se invoca la macro, no al directorio de trabajo.

### include_bytes!

Lo mismo pero como `&'static [u8; N]`:

```rust
const LOGO: &[u8] = include_bytes!("logo.png");
const CERT: &[u8] = include_bytes!("cert.pem");

fn main() {
    println!("Logo size: {} bytes", LOGO.len());
}
```

### include!

Incluye un archivo Rust como código (raramente usado, pero existe):

```rust
// generated_code.rs (generado por build script):
// [1, 2, 3, 4, 5]

const DATA: &[i32] = &include!("generated_code.rs");
```

### Cuándo usar cada uno

```rust
// include_str! → templates, SQL, configuración embebida
const TEMPLATE: &str = include_str!("template.html");

// include_bytes! → assets binarios (imágenes, certificados, wasm)
const WASM: &[u8] = include_bytes!("module.wasm");

// Ninguno → si el archivo es grande o cambia frecuentemente
// Mejor leer en runtime con std::fs::read
```

---

## 9. Expandir macros con cargo-expand

`cargo-expand` es la herramienta clave para entender qué hace una macro. Muestra el código **después** de la expansión de macros:

### Instalar

```bash
cargo install cargo-expand
# Requiere nightly toolchain:
rustup install nightly
```

### Usar

```bash
# Expandir todo el crate:
cargo expand

# Expandir un módulo específico:
cargo expand models

# Expandir una función:
cargo expand main

# Con colores:
cargo expand --theme monokai
```

### Ejemplo práctico

```rust
// src/main.rs
fn main() {
    let v = vec![1, 2, 3];
    println!("Vector: {:?}", v);
    dbg!(&v);
    assert_eq!(v.len(), 3);
}
```

```bash
cargo expand
```

Produce algo como:

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
            format_args!("Vector: {0:?}\n", v),
        );
    };
    match &v {
        tmp => {
            ::std::io::_eprint(
                format_args!(
                    "[{0}:{1}:{2}] {3} = {4:#?}\n",
                    "src/main.rs", 4u32, 5u32, "&v", &tmp,
                ),
            );
            tmp
        }
    };
    match (&v.len(), &3) {
        (left_val, right_val) => {
            if !(*left_val == *right_val) {
                let kind = ::core::panicking::AssertKind::Eq;
                ::core::panicking::assert_failed(
                    kind, &*left_val, &*right_val, ::core::option::Option::None,
                );
            }
        }
    };
}
```

### Expandir derives

```rust
#[derive(Debug, Clone)]
struct Point {
    x: f64,
    y: f64,
}
```

`cargo expand` muestra las implementaciones generadas de `Debug` y `Clone`:

```rust
struct Point {
    x: f64,
    y: f64,
}

#[automatically_derived]
impl ::core::fmt::Debug for Point {
    fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
        ::core::fmt::Formatter::debug_struct_field2_finish(
            f, "Point", "x", &self.x, "y", &&self.y,
        )
    }
}

#[automatically_derived]
impl ::core::clone::Clone for Point {
    fn clone(&self) -> Point {
        Point {
            x: ::core::clone::Clone::clone(&self.x),
            y: ::core::clone::Clone::clone(&self.y),
        }
    }
}
```

### Alternativa sin cargo-expand

Si no puedes instalar nightly, puedes usar el playground de Rust:
1. Escribe tu código en https://play.rust-lang.org
2. En "Tools" → "Expand macros"

---

## 10. Macros procedurales: derive, attribute, function-like

Además de `macro_rules!` (declarativas), Rust tiene **macros procedurales** — código Rust que genera código Rust en compilación. No las escribirás ahora, pero las usas constantemente:

### Derive macros

```rust
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
struct User {
    name: String,
    age: u32,
}
```

Cada `derive` es una macro procedural que genera una `impl` automática. `Debug`, `Clone`, `PartialEq` son de std. `Serialize`, `Deserialize` son del crate `serde`.

### Attribute macros

```rust
#[tokio::main]
async fn main() {
    println!("Hello from async!");
}
```

`#[tokio::main]` es una attribute macro que transforma tu función. Se expande a:

```rust
fn main() {
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap()
        .block_on(async {
            println!("Hello from async!");
        })
}
```

Otros ejemplos:

```rust
#[test]                      // marca función como test
#[cfg(test)]                 // compilación condicional
#[allow(unused)]             // suprime warnings
#[tokio::test]               // test async con tokio runtime
#[actix_web::get("/")]       // route handler en actix-web
```

### Function-like procedural macros

Parecen `macro_rules!` pero son más poderosas — pueden analizar y transformar tokens arbitrarios:

```rust
// sqlx — verifica queries SQL en compilación
let users = sqlx::query!("SELECT id, name FROM users WHERE age > $1", 18)
    .fetch_all(&pool)
    .await?;

// html! de yew — JSX-like en Rust
html! {
    <div class="container">
        <h1>{ "Hello" }</h1>
        <p>{ format!("Count: {}", count) }</p>
    </div>
}
```

### Diferencias clave

| | `macro_rules!` | Procedural macros |
|---|---|---|
| Se define en | Cualquier crate | Crate separado con `proc-macro = true` |
| Input | Patrones de tokens | Stream de tokens completo |
| Poder | Pattern matching | Código Rust arbitrario |
| Complejidad | Media | Alta |
| Ejemplos | `vec!`, `println!` | `#[derive(Debug)]`, `#[tokio::main]` |

Para este curso, solo necesitas **usar** macros procedurales, no escribirlas. Escribirlas requiere conocer el crate `proc_macro` y parsers como `syn` y `quote`.

---

## 11. Errores comunes

### Error 1: No entender los errores de macros

```rust
let v = vec![1, "two", 3];
// ERROR: expected integer, found `&str`
// La ubicación del error apunta a vec![], no a "two"
```

Los errores dentro de macros pueden ser confusos porque el compilador muestra la **ubicación de la macro**, no del código expandido. **Solución**: usa `cargo expand` para ver el código real y localizar el problema.

### Error 2: Asumir que println! crea un String

```rust
// println! imprime directamente, NO crea un String
let result = println!("hello");  // result es () — unit type
// Para crear un String, usa format!:
let result = format!("hello {}", name);  // result es String
```

### Error 3: Olvidar que dbg! toma ownership

```rust
let s = String::from("hello");
dbg!(s);
// println!("{}", s);  // ERROR: s fue movido a dbg!

// Solución: pasar referencia
let s = String::from("hello");
dbg!(&s);
println!("{}", s);  // OK
```

### Error 4: assert_eq! con tipos que no implementan Debug

```rust
struct Point { x: f64, y: f64 }

impl PartialEq for Point {
    fn eq(&self, other: &Self) -> bool {
        self.x == other.x && self.y == other.y
    }
}

let a = Point { x: 1.0, y: 2.0 };
let b = Point { x: 1.0, y: 2.0 };

// ERROR: `Point` doesn't implement `Debug`
// assert_eq! necesita Debug para mostrar los valores si falla
assert_eq!(a, b);
```

**Solución**: derivar `Debug` o usar `assert!` con comparación manual:

```rust
#[derive(Debug, PartialEq)]
struct Point { x: f64, y: f64 }
// Ahora assert_eq! funciona
```

### Error 5: Confundir macro declarativa con procedural

```rust
// Esto es una macro DECLARATIVA (macro_rules!):
macro_rules! my_macro { ... }

// Esto es una macro PROCEDURAL (derive):
#[derive(MyCustomDerive)]

// No puedes escribir un derive con macro_rules!
// Los derives requieren un crate proc-macro separado
```

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                  MACROS DEL ECOSISTEMA CHEATSHEET                  ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR DATOS                                                       ║
║  vec![1, 2, 3]              Vec con elementos                      ║
║  vec![0; n]                 Vec con n copias (requiere Clone)       ║
║  format!("x={}", x)        String formateado                      ║
║  concat!("a", "b")         &str concatenado en compilación         ║
║  stringify!(expr)           Expresión → &str literal               ║
║  include_str!("f.txt")     Archivo → &'static str                  ║
║  include_bytes!("f.bin")   Archivo → &'static [u8]                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  OUTPUT                                                            ║
║  print!("...")              stdout, sin newline                     ║
║  println!("...")            stdout, con newline                     ║
║  eprint!("...")             stderr, sin newline                     ║
║  eprintln!("...")           stderr, con newline                     ║
║  write!(buf, "...")         impl Write                              ║
║  writeln!(buf, "...")       impl Write + newline                    ║
║  dbg!(expr)                 stderr + file:line + retorna valor     ║
║                                                                    ║
║  FORMAT SPECIFIERS                                                 ║
║  {}    Display    {:?}  Debug     {:#?}  Debug pretty              ║
║  {:b}  Binary     {:o}  Octal     {:x}   Hex lower                 ║
║  {:X}  Hex upper  {:e}  Sci       {:p}   Pointer                   ║
║  {:>10} right-pad  {:<10} left-pad {:^10} center                   ║
║  {:.2}  precision  {:05}  zero-pad {:+}   sign                     ║
║  {name} named      {0}   positional                                ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ASSERTIONS                                                        ║
║  assert!(cond)              Panic si false                         ║
║  assert!(cond, "msg {}", x) Panic con mensaje                     ║
║  assert_eq!(a, b)           Panic si a ≠ b (muestra ambos)        ║
║  assert_ne!(a, b)           Panic si a = b (muestra ambos)         ║
║  debug_assert!()            Solo en debug builds                   ║
║  debug_assert_eq/ne!()      Solo en debug builds                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONTROL                                                           ║
║  todo!()                    "Not yet implemented" (panic)          ║
║  todo!("msg")               Con mensaje                            ║
║  unimplemented!()           "Not implemented" (panic)              ║
║  unreachable!()             "Unreachable code" (panic, indica bug) ║
║  panic!("msg")              Panic incondicional                    ║
║  compile_error!("msg")      Error de compilación forzado           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMPILACIÓN CONDICIONAL                                           ║
║  cfg!(feature = "x")        → bool en runtime                      ║
║  #[cfg(feature = "x")]      Elimina código si no aplica            ║
║  #[cfg_attr(cond, attr)]    Atributo condicional                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MACROS PROCEDURALES (se usan, no se escriben aquí)                ║
║  #[derive(Debug, Clone)]    Genera impl automática                 ║
║  #[tokio::main]             Transforma fn en runtime async         ║
║  #[test]                    Marca función como test                ║
║  sqlx::query!("SQL")        Verifica SQL en compilación            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  HERRAMIENTAS                                                      ║
║  cargo expand                Ver expansión de macros               ║
║  cargo expand modulo         Expandir módulo específico            ║
║  play.rust-lang.org Tools    Expandir online                       ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Predecir la expansión

Sin ejecutar el código, predice qué imprime cada línea. Luego verifica ejecutando:

```rust
fn main() {
    // 1. format! vs println!
    let a = format!("x = {}", 42);
    let b = println!("x = {}", 42);
    println!("a = {:?}, b = {:?}", a, b);

    // 2. dbg! retorna el valor
    let x = dbg!(2 + 3) * dbg!(4);
    println!("x = {}", x);

    // 3. Formateo con opciones
    println!("{:>10}", "right");
    println!("{:0>5}", 42);
    println!("{:#010x}", 255);
    println!("{:.3}", std::f64::consts::PI);

    // 4. stringify!
    println!("{}", stringify!(2 + 3 * 4));
    println!("{}", 2 + 3 * 4);

    // 5. assert_eq! cuando pasa
    let result = assert_eq!(1 + 1, 2);
    println!("assert result: {:?}", result);

    // 6. vec! dos formas
    let v1 = vec![1, 2, 3];
    let v2 = vec![0; 3];
    println!("{:?} vs {:?}", v1, v2);

    // 7. cfg!
    println!("debug: {}", cfg!(debug_assertions));
    println!("test: {}", cfg!(test));
}
```

**Preguntas para cada caso:**
1. ¿Cuál es el tipo de `a`? ¿Y de `b`?
2. ¿En qué orden se imprimen los mensajes de `dbg!`?
3. ¿`stringify!` evalúa la expresión?
4. ¿`assert_eq!` retorna algo?

### Ejercicio 2: Leer una macro real

Analiza esta macro simplificada basada en `env_logger`:

```rust
macro_rules! log {
    // log!(Level::Info, "message")
    ($level:expr, $($arg:tt)+) => {
        if $level <= crate::max_level() {
            let msg = format!($($arg)+);
            eprintln!(
                "[{}] {}: {}",
                $level,
                module_path!(),
                msg
            );
        }
    };
}

macro_rules! info {
    ($($arg:tt)+) => {
        log!(Level::Info, $($arg)+)
    };
}

macro_rules! warn {
    ($($arg:tt)+) => {
        log!(Level::Warn, $($arg)+)
    };
}
```

**Preguntas:**
1. ¿Por qué `$($arg:tt)+` y no `$msg:expr`?
2. ¿Qué hace `module_path!()`?
3. ¿Por qué `format!($($arg)+)` funciona para pasar los args a format?
4. ¿Qué pasa si `max_level()` retorna un nivel bajo — se compila el `format!`? ¿Se ejecuta?
5. Si quisieras añadir `error!` y `debug!`, ¿qué escribirías?
6. ¿Podrías reemplazar estas tres macros con una sola macro + un enum?

### Ejercicio 3: cargo-expand en la práctica

Escribe este código en un proyecto de prueba y usa `cargo expand` (o el playground) para ver la expansión:

```rust
#[derive(Debug, Clone, PartialEq)]
struct Color {
    r: u8,
    g: u8,
    b: u8,
}

fn main() {
    let c = Color { r: 255, g: 128, b: 0 };
    let c2 = c.clone();
    assert_eq!(c, c2);
    dbg!(&c);
    println!("Color: {:?}", c);
}
```

**Tareas:**
1. Identifica la implementación generada de `Debug` — ¿usa `debug_struct`?
2. Identifica la implementación de `Clone` — ¿clona campo por campo?
3. Identifica la implementación de `PartialEq` — ¿compara campo por campo?
4. ¿Cuántas líneas de código genera `#[derive(Debug, Clone, PartialEq)]` para un struct de 3 campos?
5. ¿Qué pasaría si un campo fuera un tipo que no implementa `Debug`? ¿Cuándo aparece el error — en la macro o en el código generado?

---

> **Nota**: no necesitas memorizar las expansiones exactas de cada macro. Lo importante es entender el **propósito** de cada una, qué traits requiere, y cómo usar `cargo expand` cuando necesites ver los detalles. Con el tiempo, leerás `vec!`, `println!`, `assert_eq!` tan naturalmente como una llamada a función — sabiendo que detrás hay metaprogramación, pero sin necesitar pensar en ello.
