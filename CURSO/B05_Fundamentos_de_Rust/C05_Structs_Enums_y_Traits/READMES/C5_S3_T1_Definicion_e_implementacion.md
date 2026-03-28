# T01 — Definicion e implementacion

## Que es un trait

Un trait define comportamiento compartido. Declara un conjunto de
metodos que los tipos deben implementar. Es el mecanismo principal
de **polimorfismo** en Rust.

```rust
// Un trait se declara con la palabra clave trait.
// Dentro va la firma de los metodos que los tipos deben implementar.

trait Summary {
    fn summarize(&self) -> String;
}

// Summary es un contrato: cualquier tipo que implemente Summary
// debe proporcionar un metodo summarize que retorne un String.
// &self indica que el metodo toma una referencia inmutable al tipo.
```

```rust
// Comparacion con otros lenguajes:
//
// Java:  interface Summary { String summarize(); }
// Go:    type Summary interface { Summarize() string }
// Rust:  trait Summary { fn summarize(&self) -> String; }
//
// Diferencias clave:
// - Los traits pueden tener implementaciones default (como Java 8+).
// - No hay herencia de tipos, solo composicion via traits.
// - Se pueden implementar para tipos existentes (con restricciones).
```

## Implementar un trait

Para que un tipo cumpla con un trait, se usa `impl Trait for Type`.
Dentro del bloque se proporcionan las implementaciones de cada metodo:

```rust
trait Summary {
    fn summarize(&self) -> String;
}

struct Article {
    title: String,
    author: String,
    content: String,
}

struct Tweet {
    username: String,
    content: String,
}

// Implementar Summary para Article:
impl Summary for Article {
    fn summarize(&self) -> String {
        format!("{}, by {}", self.title, self.author)
    }
}

// Multiples tipos pueden implementar el mismo trait:
impl Summary for Tweet {
    fn summarize(&self) -> String {
        format!("@{}: {}", self.username, self.content)
    }
}

// La sintaxis es: impl <Trait> for <Type> { ... }
// Cada metodo debe coincidir EXACTAMENTE con la firma declarada en el trait.
```

```rust
// Uso:

fn main() {
    let article = Article {
        title: String::from("Rust 2024"),
        author: String::from("Jane Doe"),
        content: String::from("Rust continues to evolve..."),
    };

    let tweet = Tweet {
        username: String::from("rustlang"),
        content: String::from("Rust 1.80 is out!"),
    };

    println!("{}", article.summarize());  // Rust 2024, by Jane Doe
    println!("{}", tweet.summarize());    // @rustlang: Rust 1.80 is out!
}
```

```rust
// Error: si olvidas implementar un metodo requerido:

trait Summary {
    fn summarize(&self) -> String;
    fn author(&self) -> String;
}

impl Summary for Article {
    fn summarize(&self) -> String {
        format!("{}", self.title)
    }
    // Falta author() -> error de compilacion:
    // error[E0046]: not all trait items implemented, missing: `author`
}
```

## Metodos default

Un trait puede proporcionar una implementacion por defecto.
Los tipos pueden usar el default o reemplazarlo:

```rust
trait Summary {
    fn summarize(&self) -> String {
        String::from("(Read more...)")
    }
}

// summarize tiene un cuerpo — es un "default method".
// Los tipos que implementen Summary NO ESTAN OBLIGADOS
// a implementar summarize. Si no lo hacen, usan el default.

struct Article { title: String, content: String }
struct Announcement { message: String }

// Article sobreescribe el default:
impl Summary for Article {
    fn summarize(&self) -> String {
        format!("{}: {}...", self.title, &self.content[..30])
    }
}

// Announcement usa el default (bloque impl vacio):
impl Summary for Announcement {}

fn main() {
    let article = Article {
        title: String::from("Breaking News"),
        content: String::from("Something important happened today in the world"),
    };
    let ann = Announcement { message: String::from("Maintenance tonight") };

    println!("{}", article.summarize());  // Breaking News: Something important happened to...
    println!("{}", ann.summarize());      // (Read more...)
}
```

```rust
// Un metodo default puede llamar a otros metodos del mismo trait,
// incluso si esos otros metodos NO tienen default:

trait Summary {
    fn summarize_author(&self) -> String;   // required — sin default

    fn summarize(&self) -> String {         // default — usa summarize_author
        format!("(Read more from {}...)", self.summarize_author())
    }
}

struct Tweet { username: String, content: String }

impl Summary for Tweet {
    fn summarize_author(&self) -> String {
        format!("@{}", self.username)
    }
    // No implementa summarize — usa el default.
}

fn main() {
    let tweet = Tweet {
        username: String::from("rustlang"),
        content: String::from("New release!"),
    };
    println!("{}", tweet.summarize());  // (Read more from @rustlang...)
}
```

```rust
// Limitacion: un metodo default que fue sobreescrito NO puede llamar
// a la version default desde la implementacion. No hay "super" como en Java/C++.
```

## La regla del huerfano (orphan rule)

No se puede implementar un trait para un tipo de manera
completamente libre. Existe una restriccion fundamental:

```rust
// Regla: para escribir impl Trait for Type, al menos UNO
// de los dos (Trait o Type) debe estar definido en tu crate.
//
// Casos validos:
// - Tu trait + tipo externo:  impl MiTrait for Vec<i32>    OK
// - Trait externo + tu tipo:  impl Display for MiStruct    OK
// - Tu trait + tu tipo:       impl MiTrait for MiStruct    OK
//
// Caso invalido:
// - Trait externo + tipo externo:
//   impl Display for Vec<i32>                              ERROR
//   Ambos (Display y Vec) estan definidos en la libreria estandar.

// Por que existe: sin ella, dos crates distintos podrian implementar
// el mismo trait para el mismo tipo con comportamiento diferente.
// Cuando tu programa usa ambos crates, el compilador no sabria
// cual implementacion usar. Esto se llama el "problema de coherencia".
```

```rust
// Workaround: el patron newtype.
// Envolver el tipo externo en un struct propio:

use std::fmt;

struct Wrapper(Vec<String>);    // Wrapper es TU tipo

impl fmt::Display for Wrapper {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[{}]", self.0.join(", "))
    }
}

fn main() {
    let w = Wrapper(vec![String::from("alpha"), String::from("beta")]);
    println!("{}", w);  // [alpha, beta]
}

// self.0 accede al Vec interior. Desventaja: Wrapper no tiene los metodos
// de Vec directamente. Se exponen via Deref o delegando manualmente.
```

## Trait como parametro

Los traits permiten escribir funciones que aceptan cualquier tipo
que implemente un trait determinado:

```rust
// Sintaxis impl Trait (azucar sintactico):
fn notify(item: &impl Summary) {
    println!("Breaking: {}", item.summarize());
}

// notify acepta una referencia a CUALQUIER tipo que implemente Summary.
// Puede recibir &Article, &Tweet, o cualquier otro &T donde T: Summary.

// La forma desazucarada usa trait bounds con generics:
fn notify_verbose<T: Summary>(item: &T) {
    println!("Breaking: {}", item.summarize());
}

// Ambas formas son equivalentes.
```

```rust
// Cuando importa la diferencia:

// Con impl Trait, los dos parametros pueden ser de tipos DISTINTOS:
fn notify_two(a: &impl Summary, b: &impl Summary) {
    // a puede ser &Article, b puede ser &Tweet.
}

// Con generics, se puede forzar que ambos sean del MISMO tipo:
fn notify_same<T: Summary>(a: &T, b: &T) {
    // a y b DEBEN ser del mismo tipo concreto.
}
```

## Usar metodos de un trait — scope

Para llamar a un metodo de un trait, el trait debe estar
en scope. Esto es una fuente comun de confusion:

```rust
// archivo: main.rs
use article::Article;
// use summary::Summary;   // <-- comentado

fn main() {
    let a = Article { title: String::from("Hello") };
    println!("{}", a.summarize());
    // error[E0599]: no method named `summarize` found for struct `Article`
    //
    // El metodo EXISTE en Article, pero el compilador no lo encuentra
    // porque el trait Summary no esta en scope.
    // Solucion: agregar use summary::Summary;
}

// Regla: para llamar metodos de un trait, ese trait debe estar
// importado con use en el modulo donde se hace la llamada.
// Excepcion: traits del prelude (Clone, Copy, etc.) estan siempre en scope.

// Patron comun: re-exportar traits en un modulo prelude propio:
// pub mod prelude {
//     pub use crate::summary::Summary;
//     pub use crate::formatter::Formatter;
// }
// Uso: use my_crate::prelude::*;
```

## Multiples traits en un mismo tipo

Un tipo puede implementar tantos traits como sea necesario.
Cada implementacion va en su propio bloque `impl`:

```rust
trait Summary { fn summarize(&self) -> String; }
trait Printable { fn print(&self); }

struct Article { title: String, content: String }

impl Summary for Article {
    fn summarize(&self) -> String {
        format!("{}: {}...", self.title, &self.content[..20])
    }
}

impl Printable for Article {
    fn print(&self) { println!("[Article] {}", self.title); }
}

// Tambien metodos inherentes (sin trait):
impl Article {
    fn new(title: &str, content: &str) -> Self {
        Article { title: String::from(title), content: String::from(content) }
    }
}

// Article tiene metodos de Summary, Printable, y metodos inherentes.
```

```rust
// Cuando dos traits definen un metodo con el mismo nombre:

trait Pilot { fn fly(&self); }
trait Wizard { fn fly(&self); }

struct Human;

impl Pilot for Human {
    fn fly(&self) { println!("This is your captain speaking."); }
}

impl Wizard for Human {
    fn fly(&self) { println!("Levitation!"); }
}

impl Human {
    fn fly(&self) { println!("Waving arms furiously."); }
}

fn main() {
    let person = Human;
    person.fly();           // metodo inherente: "Waving arms furiously."
    Pilot::fly(&person);    // "This is your captain speaking."
    Wizard::fly(&person);   // "Levitation!"

    // Rust prioriza el metodo inherente.
    // Para una version especifica: Trait::method(&instance).
}
```

```rust
// Requerir multiples traits en un parametro con +:
fn process(item: &(impl Summary + Printable)) {
    println!("{}", item.summarize());
    item.print();
}

// Con where clause (mas legible para muchos bounds):
fn process_verbose<T>(item: &T)
where T: Summary + Printable,
{
    println!("{}", item.summarize());
}
```

## Supertraits

Un trait puede requerir que los tipos que lo implementen
tambien implementen otro trait:

```rust
use std::fmt;

// PrettyPrint requiere que el tipo implemente Display.
// Sintaxis: trait SubTrait: SuperTrait

trait PrettyPrint: fmt::Display {
    fn pretty_print(&self) {
        println!("=== {} ===", self);   // {} funciona porque Self: Display
    }
}

struct Point { x: f64, y: f64 }

// Primero implementar Display (el supertrait):
impl fmt::Display for Point {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({}, {})", self.x, self.y)
    }
}

// Luego implementar PrettyPrint:
impl PrettyPrint for Point {}

fn main() {
    let p = Point { x: 3.0, y: 4.0 };
    p.pretty_print();   // === (3, 4) ===
}
```

```rust
// Multiples supertraits:
trait Serialize: Clone + std::fmt::Debug {
    fn serialize(&self) -> String;
}

// Un tipo que implemente Serialize debe implementar TAMBIEN Clone y Debug.

#[derive(Clone, Debug)]
struct Config { name: String, value: i32 }

impl Serialize for Config {
    fn serialize(&self) -> String {
        format!("{}={}", self.name, self.value)
    }
}

// Cadenas: trait A {} / trait B: A {} / trait C: B {}
// Para implementar C, el tipo debe implementar A, B, y C.
```

## Tipos asociados vs generics en traits (introduccion)

Los traits pueden parametrizarse de dos formas: con tipos
asociados o con genericos. Ambos sirven para distintos propositos:

```rust
// Tipo asociado: se define DENTRO del trait con type.
// El implementador elige el tipo concreto UNA vez.

trait Iterator {
    type Item;                      // tipo asociado
    fn next(&mut self) -> Option<Self::Item>;
}

struct Counter { count: u32 }

impl Iterator for Counter {
    type Item = u32;                // Counter produce u32

    fn next(&mut self) -> Option<Self::Item> {
        self.count += 1;
        if self.count <= 5 { Some(self.count) } else { None }
    }
}

// Con tipos asociados, Counter implementa Iterator UNA SOLA VEZ.
```

```rust
// Si Iterator usara generics:

trait GenericIterator<T> {
    fn next(&mut self) -> Option<T>;
}

// Un tipo PODRIA implementarlo multiples veces:
// impl GenericIterator<u32> for Counter { ... }
// impl GenericIterator<String> for Counter { ... }
// Esto rara vez tiene sentido para un iterador.

// Regla general:
// - Tipo asociado: cuando solo hay UNA implementacion logica por tipo.
//   Ejemplos: Iterator::Item, Deref::Target, Add::Output
// - Generico: cuando un tipo puede implementar el trait multiples veces.
//   Ejemplo: From<T> — impl From<String>, From<i32>, From<&str>
```

```rust
// Ejemplo con Add (combina ambos: Rhs es generico, Output es asociado):

use std::ops::Add;

#[derive(Debug)]
struct Point { x: f64, y: f64 }

impl Add for Point {
    type Output = Point;

    fn add(self, other: Point) -> Point {
        Point { x: self.x + other.x, y: self.y + other.y }
    }
}

fn main() {
    let p1 = Point { x: 1.0, y: 2.0 };
    let p2 = Point { x: 3.0, y: 4.0 };
    let p3 = p1 + p2;
    println!("{:?}", p3);  // Point { x: 4.0, y: 6.0 }
}
```

---

## Ejercicios

### Ejercicio 1 — Definir e implementar un trait

```rust
// Definir un trait llamado Describable con un metodo:
//   fn describe(&self) -> String;
//
// Crear dos structs:
//   - Book { title: String, pages: u32 }
//   - Movie { title: String, duration_minutes: u32 }
//
// Implementar Describable para ambos:
//   - Book: "Book: <title> (<pages> pages)"
//   - Movie: "Movie: <title> (<duration> min)"
//
// Escribir una funcion:
//   fn print_description(item: &impl Describable)
// que imprima la descripcion.
//
// Verificar:
//   cargo run
//   # Book: The Rust Book (560 pages)
//   # Movie: Blade Runner (117 min)
```

### Ejercicio 2 — Metodos default y sobreescritura

```rust
// Definir un trait Logger con:
//   fn level(&self) -> &str;               // required
//   fn log(&self, message: &str) {         // default
//       println!("[{}] {}", self.level(), message);
//   }
//
// Crear dos structs:
//   - ConsoleLogger (usa el default de log)
//   - FileLogger { path: String } (sobreescribe log para incluir el path)
//
// Implementar Logger para ambos:
//   - ConsoleLogger: level() retorna "INFO"
//   - FileLogger: level() retorna "FILE", log() imprime "[FILE -> <path>] <message>"
//
// Verificar:
//   let console = ConsoleLogger;
//   console.log("System started");       // [INFO] System started
//
//   let file = FileLogger { path: String::from("/var/log/app.log") };
//   file.log("Error occurred");          // [FILE -> /var/log/app.log] Error occurred
```

### Ejercicio 3 — Orphan rule y newtype

```rust
// Intentar implementar Display para Vec<String> directamente.
// Observar el error del compilador.
//
// Luego aplicar el patron newtype:
//   - Crear struct StringList(Vec<String>)
//   - Implementar Display para StringList
//     formato: elementos separados por " | "
//   - Agregar un metodo push(&mut self, item: String)
//     que delegue al Vec interno
//
// Verificar:
//   let mut list = StringList(vec![String::from("alpha"), String::from("beta")]);
//   list.push(String::from("gamma"));
//   println!("{}", list);   // alpha | beta | gamma
```

### Ejercicio 4 — Supertraits

```rust
// Definir un trait Labeled que requiera Display como supertrait:
//   trait Labeled: std::fmt::Display {
//       fn label(&self) -> String {
//           format!("Label: {}", self)
//       }
//   }
//
// Crear un struct Product { name: String, price: f64 }.
// Implementar Display para Product: "<name> ($<price>)"
// Implementar Labeled para Product (usando el default).
//
// Verificar:
//   let p = Product { name: String::from("Widget"), price: 9.99 };
//   println!("{}", p);          // Widget ($9.99)
//   println!("{}", p.label());  // Label: Widget ($9.99)
```
