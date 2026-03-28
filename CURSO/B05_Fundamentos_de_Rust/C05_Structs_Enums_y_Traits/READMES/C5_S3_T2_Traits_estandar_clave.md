# T02 — Traits estandar clave

## Por que importan los traits estandar

La libreria estandar de Rust define un conjunto de traits que el
compilador y el ecosistema reconocen de forma especial. Implementar
estos traits en tus tipos les da capacidades concretas: poder
imprimirse, compararse, copiarse, usarse como claves de mapas,
convertirse entre si. Sin ellos, tus structs y enums son opacos
para el resto del codigo.

```rust
// Sin traits estandar, un struct no puede hacer casi nada:
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let p = Point { x: 1.0, y: 2.0 };

    // println!("{}", p);    // Error: Point doesn't implement Display
    // println!("{:?}", p);  // Error: Point doesn't implement Debug
    // let q = p.clone();    // Error: Point doesn't implement Clone
    // if p == p {}           // Error: Point doesn't implement PartialEq
}
```

```rust
// Con traits estandar, el tipo se integra con todo el lenguaje:
#[derive(Debug, Clone, Copy, PartialEq)]
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let p = Point { x: 1.0, y: 2.0 };

    println!("{:?}", p);         // Point { x: 1.0, y: 2.0 }
    let q = p;                   // Copy: copia implicita
    let r = p.clone();           // Clone: copia explicita
    assert_eq!(p, q);            // PartialEq: comparacion
}
```

---

## Tabla de contenidos

1. [Display](#display)
2. [Debug](#debug)
3. [Clone](#clone)
4. [Copy](#copy)
5. [PartialEq y Eq](#partialeq-y-eq)
6. [PartialOrd y Ord](#partialord-y-ord)
7. [Hash](#hash)
8. [Errores comunes](#errores-comunes)
9. [Tabla resumen y cheatsheet](#tabla-resumen-y-cheatsheet)

---

## Display

`fmt::Display` controla como se muestra un tipo al usuario final.
Es lo que usa el formato `{}` en `println!`, `format!`, `write!`
y cualquier macro de formateo. Display **no se puede derivar** con
`#[derive()]` — siempre hay que implementarlo manualmente porque
Rust no puede saber cual es la representacion "bonita" que
quieres para tu tipo.

```rust
use std::fmt;

struct Color {
    r: u8,
    g: u8,
    b: u8,
}

// Implementacion manual de Display:
impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // write! escribe en el formatter, NO en stdout.
        // Retorna fmt::Result (alias de Result<(), fmt::Error>).
        write!(f, "#{:02X}{:02X}{:02X}", self.r, self.g, self.b)
    }
}

fn main() {
    let red = Color { r: 255, g: 0, b: 0 };

    println!("{}", red);             // #FF0000
    println!("Color: {}", red);      // Color: #FF0000

    // format! retorna un String:
    let s = format!("{}", red);
    assert_eq!(s, "#FF0000");
}
```

### El trait Display completo

```rust
// Definicion en la libreria estandar:
// pub trait Display {
//     fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result;
// }
//
// Solo un metodo obligatorio: fmt.
// f es un buffer de escritura con metodos auxiliares.
```

### Formatter y opciones de alineacion

```rust
use std::fmt;

struct Label {
    name: String,
    value: i32,
}

impl fmt::Display for Label {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // El formatter respeta las opciones que el llamador pasa.
        // Por ejemplo, {:<20} pide alineacion a la izquierda con ancho 20.

        // Podemos usar el metodo pad() para que nuestro tipo
        // respete width, alignment y fill:
        let text = format!("{}: {}", self.name, self.value);

        // Si el caller pide ancho, pad se encarga:
        f.pad(&text)
    }
}

fn main() {
    let label = Label { name: "score".to_string(), value: 42 };

    println!("[{}]", label);         // [score: 42]
    println!("[{:<20}]", label);     // [score: 42            ]
    println!("[{:>20}]", label);     // [            score: 42]
    println!("[{:^20}]", label);     // [     score: 42       ]
    println!("[{:*^20}]", label);    // [*****score: 42*******]
}
```

### Display para enums

```rust
use std::fmt;

enum Suit {
    Hearts,
    Diamonds,
    Clubs,
    Spades,
}

impl fmt::Display for Suit {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // match para cada variante:
        match self {
            Suit::Hearts   => write!(f, "Hearts"),
            Suit::Diamonds => write!(f, "Diamonds"),
            Suit::Clubs    => write!(f, "Clubs"),
            Suit::Spades   => write!(f, "Spades"),
        }
    }
}

fn main() {
    let s = Suit::Hearts;
    println!("{}", s);   // Hearts
}
```

### Display para tipos con campos que implementan Display

```rust
use std::fmt;

struct Coordinate {
    lat: f64,
    lon: f64,
}

impl fmt::Display for Coordinate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Podemos usar {} dentro de write! porque f64 implementa Display:
        write!(f, "({}, {})", self.lat, self.lon)
    }
}

fn main() {
    let c = Coordinate { lat: 40.4168, lon: -3.7038 };
    println!("{}", c);   // (40.4168, -3.7038)
}
```

### Relacion con ToString

```rust
// Implementar Display te da ToString GRATIS.
// La libreria estandar tiene un blanket impl:
//
// impl<T: Display> ToString for T {
//     fn to_string(&self) -> String {
//         format!("{}", self)
//     }
// }
//
// Por eso NUNCA implementes ToString manualmente. Implementa Display.

use std::fmt;

struct Temperature {
    celsius: f64,
}

impl fmt::Display for Temperature {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:.1}°C", self.celsius)
    }
}

fn main() {
    let t = Temperature { celsius: 36.6 };

    // to_string() viene gratis de Display:
    let s: String = t.to_string();
    assert_eq!(s, "36.6°C");

    // Equivalente a:
    let s2 = format!("{}", t);
    assert_eq!(s, s2);
}
```

### Display para colecciones

```rust
use std::fmt;

struct Matrix {
    rows: Vec<Vec<f64>>,
}

impl fmt::Display for Matrix {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // El ? propaga errores de escritura:
        for (i, row) in self.rows.iter().enumerate() {
            if i > 0 {
                writeln!(f)?;  // nueva linea entre filas
            }
            write!(f, "[")?;
            for (j, val) in row.iter().enumerate() {
                if j > 0 {
                    write!(f, ", ")?;
                }
                write!(f, "{:6.2}", val)?;
            }
            write!(f, "]")?;
        }
        Ok(())
    }
}

fn main() {
    let m = Matrix {
        rows: vec![
            vec![1.0, 2.5, 3.14],
            vec![4.0, 5.5, 6.28],
        ],
    };
    println!("{}", m);
    // [  1.00,   2.50,   3.14]
    // [  4.00,   5.50,   6.28]
}
```

---

## Debug

`fmt::Debug` produce una representacion orientada al **programador**,
no al usuario final. Es lo que usa `{:?}` en macros de formateo.
A diferencia de Display, Debug **si se puede derivar** con
`#[derive(Debug)]`, y esa es la forma mas comun de obtenerlo.

### Derive basico

```rust
#[derive(Debug)]
struct Packet {
    src: String,
    dst: String,
    payload: Vec<u8>,
    ttl: u8,
}

fn main() {
    let p = Packet {
        src: "192.168.1.1".to_string(),
        dst: "10.0.0.1".to_string(),
        payload: vec![0xDE, 0xAD, 0xBE, 0xEF],
        ttl: 64,
    };

    // {:?} — formato compacto en una linea:
    println!("{:?}", p);
    // Packet { src: "192.168.1.1", dst: "10.0.0.1", payload: [222, 173, 190, 239], ttl: 64 }

    // {:#?} — pretty-print, con indentacion:
    println!("{:#?}", p);
    // Packet {
    //     src: "192.168.1.1",
    //     dst: "10.0.0.1",
    //     payload: [
    //         222,
    //         173,
    //         190,
    //         239,
    //     ],
    //     ttl: 64,
    // }
}
```

### dbg! macro

```rust
#[derive(Debug)]
struct Config {
    port: u16,
    host: String,
}

fn main() {
    let cfg = Config {
        port: 8080,
        host: "localhost".to_string(),
    };

    // dbg! imprime el archivo, linea, expresion y su valor.
    // Retorna ownership del valor (lo mueve o copia).
    dbg!(&cfg);
    // [src/main.rs:13:5] &cfg = Config {
    //     port: 8080,
    //     host: "localhost",
    // }

    // dbg! es util en cadenas de expresiones:
    let port = dbg!(cfg.port + 1);  // imprime y retorna 8081
    // [src/main.rs:19:16] cfg.port + 1 = 8081
}
```

### Implementacion manual de Debug

```rust
use std::fmt;

struct Secret {
    username: String,
    password: String,
}

// Implementacion manual para OCULTAR datos sensibles:
impl fmt::Debug for Secret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Secret")
            .field("username", &self.username)
            .field("password", &"[REDACTED]")  // no mostrar la clave real
            .finish()
    }
}

fn main() {
    let s = Secret {
        username: "admin".to_string(),
        password: "super_secret_123".to_string(),
    };
    println!("{:?}", s);
    // Secret { username: "admin", password: "[REDACTED]" }
}
```

### Helpers del Formatter para Debug manual

```rust
use std::fmt;

// debug_struct — para structs con campos nombrados:
struct Point { x: f64, y: f64 }

impl fmt::Debug for Point {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Point")
            .field("x", &self.x)
            .field("y", &self.y)
            .finish()
    }
}

// debug_tuple — para tuple structs:
struct Pair(i32, i32);

impl fmt::Debug for Pair {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Pair")
            .field(&self.0)
            .field(&self.1)
            .finish()
    }
}

// debug_list — para colecciones tipo lista:
struct IdList(Vec<u32>);

impl fmt::Debug for IdList {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_list()
            .entries(self.0.iter())
            .finish()
    }
}

// debug_map — para colecciones tipo mapa:
use std::collections::HashMap;
struct Registry(HashMap<String, u32>);

impl fmt::Debug for Registry {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_map()
            .entries(self.0.iter())
            .finish()
    }
}

fn main() {
    let p = Point { x: 1.0, y: 2.0 };
    println!("{:?}", p);    // Point { x: 1.0, y: 2.0 }
    println!("{:#?}", p);
    // Point {
    //     x: 1.0,
    //     y: 2.0,
    // }

    let pair = Pair(10, 20);
    println!("{:?}", pair);  // Pair(10, 20)

    let ids = IdList(vec![1, 2, 3]);
    println!("{:?}", ids);   // [1, 2, 3]

    let mut map = HashMap::new();
    map.insert("alice".to_string(), 100);
    let reg = Registry(map);
    println!("{:?}", reg);   // {"alice": 100}
}
```

### Alternate flag (#) en Debug manual

```rust
use std::fmt;

struct HexByte(u8);

impl fmt::Debug for HexByte {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // f.alternate() es true cuando se usa {:#?}
        if f.alternate() {
            write!(f, "0x{:02X}", self.0)
        } else {
            write!(f, "{}", self.0)
        }
    }
}

fn main() {
    let b = HexByte(255);
    println!("{:?}", b);    // 255
    println!("{:#?}", b);   // 0xFF
}
```

### Diferencia entre Display y Debug

```rust
use std::fmt;

struct Ipv4 {
    octets: [u8; 4],
}

// Display: para el usuario final
impl fmt::Display for Ipv4 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}.{}", self.octets[0], self.octets[1],
               self.octets[2], self.octets[3])
    }
}

// Debug: para el programador
impl fmt::Debug for Ipv4 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Ipv4")
            .field("octets", &self.octets)
            .finish()
    }
}

fn main() {
    let ip = Ipv4 { octets: [192, 168, 1, 1] };

    println!("{}", ip);      // 192.168.1.1          (Display — limpio)
    println!("{:?}", ip);    // Ipv4 { octets: [192, 168, 1, 1] }  (Debug — info tecnica)
}
```

---

## Clone

El trait `Clone` indica que un tipo puede crear una **copia profunda**
(deep copy) de si mismo. Llamar `.clone()` produce un valor nuevo,
independiente del original. Es una operacion explicita y puede ser
costosa (por ejemplo, clonar un `Vec<String>` clona cada String
y el vector completo).

### Derive y uso basico

```rust
#[derive(Clone, Debug)]
struct Config {
    name: String,
    values: Vec<i32>,
}

fn main() {
    let original = Config {
        name: "test".to_string(),
        values: vec![1, 2, 3],
    };

    // clone() crea una copia independiente:
    let mut copy = original.clone();
    copy.name = "modified".to_string();
    copy.values.push(4);

    println!("{:?}", original);  // Config { name: "test", values: [1, 2, 3] }
    println!("{:?}", copy);      // Config { name: "modified", values: [1, 2, 3, 4] }
    // El original no se vio afectado.
}
```

### El trait Clone completo

```rust
// Definicion en la libreria estandar:
// pub trait Clone: Sized {
//     fn clone(&self) -> Self;
//
//     fn clone_from(&mut self, source: &Self) {
//         *self = source.clone();
//     }
// }
//
// clone() es obligatorio.
// clone_from() tiene implementacion default pero se puede optimizar.
```

### clone_from — optimizacion para reutilizar buffers

```rust
fn main() {
    let source = "a very long string that causes allocation".to_string();
    let mut dest = String::with_capacity(100);  // ya tiene buffer grande

    // clone() descarta el buffer existente de dest y crea uno nuevo.
    // clone_from() puede reutilizar el buffer existente si tiene capacidad:
    dest.clone_from(&source);
    // Mas eficiente: reutiliza la memoria ya asignada en dest.

    assert_eq!(dest, source);
    // dest mantiene su capacidad original (100) si era suficiente.
}
```

```rust
// clone_from es importante para Vec tambien:
fn main() {
    let source = vec![1, 2, 3];
    let mut dest = Vec::with_capacity(1000);
    dest.push(0);

    // En vez de: dest = source.clone();  // descarta el buffer de 1000
    dest.clone_from(&source);             // reutiliza el buffer de 1000
    assert_eq!(dest, vec![1, 2, 3]);
}
```

### Implementacion manual de Clone

```rust
#[derive(Debug)]
struct Tree {
    value: i32,
    children: Vec<Tree>,
    // Imaginemos que tiene un campo que no queremos clonar tal cual:
    id: u64,
}

static mut COUNTER: u64 = 0;

fn next_id() -> u64 {
    // En codigo real usarias AtomicU64 o similar.
    unsafe {
        COUNTER += 1;
        COUNTER
    }
}

impl Clone for Tree {
    fn clone(&self) -> Self {
        Tree {
            value: self.value,
            children: self.children.clone(),  // clone recursivo
            id: next_id(),  // el clon obtiene su propio id unico
        }
    }
}

fn main() {
    let tree = Tree {
        value: 1,
        children: vec![
            Tree { value: 2, children: vec![], id: next_id() },
            Tree { value: 3, children: vec![], id: next_id() },
        ],
        id: next_id(),
    };

    let cloned = tree.clone();
    // cloned tiene los mismos valores pero ids diferentes.
}
```

### Cuando Clone no se puede derivar

```rust
// derive(Clone) requiere que TODOS los campos sean Clone.

// Esto NO compila:
// #[derive(Clone)]
// struct Handle {
//     file: std::fs::File,  // File no implementa Clone
// }
// Error: the trait `Clone` is not implemented for `File`

// Solucion: implementar manualmente o redisenar el struct.
```

### Clone y performance

```rust
#[derive(Clone, Debug)]
struct BigData {
    data: Vec<u8>,
}

fn process(data: BigData) {
    println!("Processing {} bytes", data.data.len());
}

fn main() {
    let big = BigData { data: vec![0u8; 1_000_000] };

    // CUIDADO: clone() aqui copia 1MB de datos:
    process(big.clone());  // copia costosa
    process(big);           // move: sin copia, pero big ya no es usable

    // Preferir &BigData cuando no necesites ownership:
    // fn process(data: &BigData) { ... }
}
```

---

## Copy

`Copy` es un **marker trait** (no tiene metodos). Indica que un tipo
puede copiarse simplemente copiando sus bytes en memoria (memcpy).
Cuando un tipo es `Copy`, las asignaciones y paso de argumentos
hacen copia implicita en vez de mover.

### Diferencia fundamental con Clone

```rust
#[derive(Debug, Clone, Copy)]
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let a = Point { x: 1.0, y: 2.0 };

    // Con Copy, la asignacion COPIA en vez de mover:
    let b = a;   // copia implicita (Copy)
    let c = a;   // a sigue siendo valido — fue copiado, no movido

    println!("{:?} {:?} {:?}", a, b, c);
    // Point { x: 1.0, y: 2.0 } Point { x: 1.0, y: 2.0 } Point { x: 1.0, y: 2.0 }
}
```

```rust
#[derive(Debug, Clone)]
struct Name {
    value: String,
}

fn main() {
    let a = Name { value: "Alice".to_string() };

    // Sin Copy, la asignacion MUEVE:
    let b = a;  // a se movio a b
    // println!("{:?}", a);  // Error: value borrowed here after move

    // Hay que clonar explicitamente:
    let c = b.clone();
    println!("{:?} {:?}", b, c);
}
```

### Requisitos para implementar Copy

```rust
// Copy requiere Clone como supertrait:
// pub trait Copy: Clone { }
//
// Reglas para que un tipo pueda ser Copy:
//
// 1. TODOS sus campos deben ser Copy.
// 2. No debe implementar Drop.
// 3. El tipo no puede contener datos en el heap (String, Vec, Box, etc.).

// Tipos primitivos que son Copy:
// - Todos los enteros: i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, isize, usize
// - Todos los flotantes: f32, f64
// - bool
// - char
// - Referencias inmutables: &T (para cualquier T)
// - Punteros crudos: *const T, *mut T
// - Tuplas si todos sus elementos son Copy: (i32, f64, bool)
// - Arrays si su tipo elemento es Copy: [i32; 5]
// - Tipo unit: ()

// Tipos que NO son Copy:
// - String (contiene heap data)
// - Vec<T> (contiene heap data)
// - Box<T> (contiene heap data)
// - &mut T (solo puede haber una referencia mutable)
// - Cualquier struct con campos no-Copy
// - Cualquier tipo que implemente Drop
```

### Por que Copy y Drop son incompatibles

```rust
// Si un tipo implementa Drop, Rust necesita ejecutar drop() al salir de scope.
// Si tambien fuera Copy, se duplicarian los datos y drop() se llamaria
// multiples veces sobre la misma memoria — double free.
//
// Por eso el compilador prohibe:
//
// #[derive(Clone, Copy)]
// struct Bad {
//     data: String,  // Error: String no es Copy
// }
//
// Incluso si todos los campos fueran Copy, si el struct tiene Drop:
//
// #[derive(Clone, Copy)]
// struct AlsoBad {
//     x: i32,
// }
// impl Drop for AlsoBad {
//     fn drop(&mut self) { println!("dropped"); }
// }
// Error: the trait `Copy` cannot be implemented for this type;
// the type has a destructor
```

### Implementar Copy: derive vs manual

```rust
// La forma mas comun — derive:
#[derive(Debug, Clone, Copy, PartialEq)]
struct Vec2 {
    x: f32,
    y: f32,
}

// Manual — no hay metodos que implementar, pero igual se puede:
struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}

impl Clone for Vec3 {
    fn clone(&self) -> Self {
        *self  // desreferenciar un Copy type lo copia
    }
}

impl Copy for Vec3 {}
// No hay bloque impl porque Copy no tiene metodos.
```

### Semantica de Copy en funciones

```rust
#[derive(Debug, Clone, Copy)]
struct Score(u32);

fn double(s: Score) -> Score {
    Score(s.0 * 2)
}

fn main() {
    let s = Score(50);

    // s se copia al pasarlo a la funcion — sigue siendo usable:
    let d = double(s);
    println!("Original: {:?}, Doubled: {:?}", s, d);
    // Original: Score(50), Doubled: Score(100)

    // Sin Copy, s se moveria y no podrias usarlo despues.
}
```

### Cuando NO derivar Copy (aunque se pueda)

```rust
// A veces un tipo PODRIA ser Copy pero no deberia serlo:

// 1. Tipos que representan ownership logica:
// Un FileDescriptor es solo un i32, pero copiarlo no duplica el recurso.
struct FileDescriptor(i32);
// Si fuera Copy, podrias "copiar" el descriptor y cerrar el archivo
// desde dos sitios, causando bugs sutiles.

// 2. Tipos que podrian crecer en el futuro:
// Si hoy tu struct tiene solo campos Copy pero manana podrias
// agregar un String, hacer Copy ahora romperia tu API publica.
// Quitar Copy de un tipo es un breaking change.

// 3. Tipos grandes:
// Copy copia bytes implicitamente. Un [u8; 4096] es Copy,
// pero copiarlo implicitamente en cada asignacion es costoso.
// Mejor exigir .clone() explicito para que el programador
// sea consciente del costo.
```

---

## PartialEq y Eq

Estos traits controlan la comparacion de igualdad (`==` y `!=`).

### PartialEq

`PartialEq` implementa una **relacion de equivalencia parcial**.
"Parcial" significa que puede haber valores que **no son iguales
a si mismos** (el caso clasico: `NaN` en flotantes).

```rust
#[derive(Debug, PartialEq)]
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let a = Point { x: 1.0, y: 2.0 };
    let b = Point { x: 1.0, y: 2.0 };
    let c = Point { x: 3.0, y: 4.0 };

    assert!(a == b);     // true: mismos valores
    assert!(a != c);     // true: valores distintos

    // El problema con floats y NaN:
    let nan = f64::NAN;
    assert!(nan != nan);             // true! NaN no es igual a si mismo
    assert!(!(nan == nan));          // true! confirma lo anterior

    let p = Point { x: f64::NAN, y: 0.0 };
    assert!(p != p);  // true! porque NaN != NaN
}
```

### El trait PartialEq

```rust
// Definicion simplificada:
// pub trait PartialEq<Rhs = Self>
// where
//     Rhs: ?Sized,
// {
//     fn eq(&self, other: &Rhs) -> bool;
//
//     fn ne(&self, other: &Rhs) -> bool {
//         !self.eq(other)
//     }
// }
//
// Solo eq() es obligatorio. ne() tiene implementacion default.
// Rhs puede ser otro tipo (comparacion entre tipos distintos).
```

### Implementacion manual de PartialEq

```rust
#[derive(Debug)]
struct CaseInsensitive {
    value: String,
}

impl PartialEq for CaseInsensitive {
    fn eq(&self, other: &Self) -> bool {
        // Comparar sin importar mayusculas/minusculas:
        self.value.to_lowercase() == other.value.to_lowercase()
    }
}

fn main() {
    let a = CaseInsensitive { value: "Hello".to_string() };
    let b = CaseInsensitive { value: "HELLO".to_string() };
    let c = CaseInsensitive { value: "world".to_string() };

    assert!(a == b);    // true: "hello" == "hello"
    assert!(a != c);    // true: "hello" != "world"
}
```

### PartialEq entre tipos distintos

```rust
struct Meters(f64);
struct Centimeters(f64);

impl PartialEq<Centimeters> for Meters {
    fn eq(&self, other: &Centimeters) -> bool {
        (self.0 * 100.0 - other.0).abs() < f64::EPSILON
    }
}

impl PartialEq<Meters> for Centimeters {
    fn eq(&self, other: &Meters) -> bool {
        (self.0 - other.0 * 100.0).abs() < f64::EPSILON
    }
}

fn main() {
    let m = Meters(1.0);
    let cm = Centimeters(100.0);

    assert!(m == cm);   // 1 metro == 100 centimetros
    assert!(cm == m);   // simetrico
}
```

### Eq

`Eq` es un **marker trait** que indica que la igualdad es una
**relacion de equivalencia total**: reflexiva, simetrica y
transitiva. Es decir, para todo `x`, se cumple que `x == x`.

```rust
// Definicion:
// pub trait Eq: PartialEq<Self> { }
//
// No tiene metodos propios. Es una promesa del programador:
// "la igualdad de mi tipo siempre es reflexiva".

// Tipos que implementan Eq: enteros, bool, char, String, Vec<T> (si T: Eq)
// Tipos que NO implementan Eq: f32, f64 (porque NaN != NaN)

#[derive(Debug, PartialEq, Eq)]
struct UserId(u64);

fn main() {
    let a = UserId(42);
    let b = UserId(42);

    assert!(a == b);
    assert!(a == a);  // reflexivo — garantizado por Eq
}
```

### Por que Eq importa

```rust
use std::collections::HashMap;

// HashMap requiere que las claves implementen Eq (ademas de Hash).
// La razon: si una clave no fuera igual a si misma (como NaN),
// nunca podrias encontrar el valor que insertaste.

#[derive(Debug, PartialEq, Eq, Hash)]
struct Key {
    id: u32,
    name: String,
}

fn main() {
    let mut map = HashMap::new();
    map.insert(Key { id: 1, name: "one".to_string() }, 100);

    let lookup = Key { id: 1, name: "one".to_string() };
    assert_eq!(map.get(&lookup), Some(&100));
}
```

```rust
// Esto NO compila — f64 no implementa Eq:
// use std::collections::HashMap;
// let mut map: HashMap<f64, String> = HashMap::new();
// Error: the trait `Eq` is not implemented for `f64`
```

### PartialEq con derive — que hace exactamente

```rust
// derive(PartialEq) compara campo por campo, en orden de declaracion.
// Todos los campos deben ser PartialEq.

#[derive(PartialEq)]
struct Record {
    id: u32,        // u32: PartialEq ✓
    name: String,   // String: PartialEq ✓
    score: f64,     // f64: PartialEq ✓ (pero NO Eq por NaN)
}

// El derive genera algo equivalente a:
// impl PartialEq for Record {
//     fn eq(&self, other: &Self) -> bool {
//         self.id == other.id
//             && self.name == other.name
//             && self.score == other.score
//     }
// }

// Para enums, derive(PartialEq) compara variante y datos:
#[derive(PartialEq, Debug)]
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
}

fn main() {
    assert!(Shape::Circle(1.0) == Shape::Circle(1.0));
    assert!(Shape::Circle(1.0) != Shape::Rectangle(1.0, 1.0));
}
```

### Comparacion parcial por campos selectos

```rust
#[derive(Debug)]
struct Employee {
    id: u32,
    name: String,
    last_login: u64,  // no queremos que esto afecte la igualdad
}

impl PartialEq for Employee {
    fn eq(&self, other: &Self) -> bool {
        // Solo comparar por id — dos Employee con el mismo id son "iguales"
        // aunque last_login difiera:
        self.id == other.id
    }
}

impl Eq for Employee {}

fn main() {
    let a = Employee { id: 1, name: "Alice".to_string(), last_login: 1000 };
    let b = Employee { id: 1, name: "Alice".to_string(), last_login: 2000 };
    assert!(a == b);  // true: mismo id
}
```

---

## PartialOrd y Ord

Estos traits permiten ordenar valores usando `<`, `>`, `<=`, `>=`
y metodos como `.min()`, `.max()`, `.clamp()`, `.sort()`.

### PartialOrd

`PartialOrd` define un **ordenamiento parcial**: la comparacion
puede retornar `None` si dos valores no son comparables (como
NaN en flotantes).

```rust
// Definicion:
// pub trait PartialOrd<Rhs = Self>: PartialEq<Rhs>
// where
//     Rhs: ?Sized,
// {
//     fn partial_cmp(&self, other: &Rhs) -> Option<Ordering>;
//
//     fn lt(&self, other: &Rhs) -> bool { ... }
//     fn le(&self, other: &Rhs) -> bool { ... }
//     fn gt(&self, other: &Rhs) -> bool { ... }
//     fn ge(&self, other: &Rhs) -> bool { ... }
// }
//
// Solo partial_cmp() es obligatorio.
// lt, le, gt, ge se derivan automaticamente de partial_cmp.
```

```rust
use std::cmp::Ordering;

fn main() {
    // Ordering tiene tres variantes:
    // Ordering::Less    — self < other
    // Ordering::Equal   — self == other
    // Ordering::Greater — self > other

    // Con PartialOrd, partial_cmp retorna Option<Ordering>:
    let a: f64 = 1.0;
    let b: f64 = 2.0;
    assert_eq!(a.partial_cmp(&b), Some(Ordering::Less));
    assert_eq!(b.partial_cmp(&a), Some(Ordering::Greater));
    assert_eq!(a.partial_cmp(&a), Some(Ordering::Equal));

    // NaN no se puede comparar:
    let nan = f64::NAN;
    assert_eq!(nan.partial_cmp(&1.0), None);  // no se puede ordenar
    assert_eq!(nan.partial_cmp(&nan), None);   // ni consigo mismo
    assert!(!(nan < 1.0));   // false
    assert!(!(nan > 1.0));   // false
    assert!(!(nan == 1.0));  // false
    // NaN no es menor, mayor ni igual a nada.
}
```

### Derive de PartialOrd

```rust
// derive(PartialOrd) compara campo por campo en orden de declaracion.
// El primer campo que difiere determina el resultado.

#[derive(Debug, PartialEq, PartialOrd)]
struct Version {
    major: u32,
    minor: u32,
    patch: u32,
}

fn main() {
    let v1 = Version { major: 1, minor: 2, patch: 3 };
    let v2 = Version { major: 1, minor: 3, patch: 0 };
    let v3 = Version { major: 2, minor: 0, patch: 0 };

    assert!(v1 < v2);   // 1.2.3 < 1.3.0 (minor difiere primero)
    assert!(v2 < v3);   // 1.3.0 < 2.0.0 (major difiere primero)
    assert!(v1 < v3);   // transitivo

    // OJO: el orden de los campos en el struct importa para derive.
    // Si major estuviera despues de minor, la comparacion seria incorrecta.
}
```

### Implementacion manual de PartialOrd

```rust
use std::cmp::Ordering;

#[derive(Debug, PartialEq)]
struct Temperature {
    kelvin: f64,
}

impl PartialOrd for Temperature {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        // Delegar al partial_cmp de f64:
        self.kelvin.partial_cmp(&other.kelvin)
    }
}

fn main() {
    let boiling = Temperature { kelvin: 373.15 };
    let freezing = Temperature { kelvin: 273.15 };

    assert!(boiling > freezing);
    assert!(freezing < boiling);
}
```

### Ord

`Ord` define un **ordenamiento total**: todos los valores se
pueden comparar y no hay "incomparables". Requiere `Eq`
(equivalencia total) como prerequisito.

```rust
// Definicion:
// pub trait Ord: Eq + PartialOrd<Self> {
//     fn cmp(&self, other: &Self) -> Ordering;
//
//     fn max(self, other: Self) -> Self { ... }
//     fn min(self, other: Self) -> Self { ... }
//     fn clamp(self, min: Self, max: Self) -> Self { ... }
// }
//
// Solo cmp() es obligatorio.
// Retorna Ordering directamente (no Option).
```

```rust
use std::cmp::Ordering;

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
struct Priority(u8);

fn main() {
    let low = Priority(1);
    let high = Priority(10);

    assert_eq!(low.cmp(&high), Ordering::Less);
    assert_eq!(high.cmp(&low), Ordering::Greater);

    // max y min:
    assert_eq!(low.max(high), Priority(10));
    // Nota: max/min consumen los valores (toman self, no &self).
}
```

### Ord es necesario para sort

```rust
// Vec::sort() requiere Ord. Vec::sort_by() acepta un closure.

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
struct Score {
    points: u32,
    name: String,
}

fn main() {
    let mut scores = vec![
        Score { points: 50, name: "Bob".to_string() },
        Score { points: 90, name: "Alice".to_string() },
        Score { points: 50, name: "Alice".to_string() },
    ];

    scores.sort();
    // Ordena por points primero, luego por name (orden de campos):
    // Score { points: 50, name: "Alice" }
    // Score { points: 50, name: "Bob" }
    // Score { points: 90, name: "Alice" }

    for s in &scores {
        println!("{:?}", s);
    }
}
```

### Implementacion manual de Ord

```rust
use std::cmp::Ordering;

#[derive(Debug, Eq, PartialEq)]
struct Task {
    priority: u8,   // 0 = mas urgente
    name: String,
}

// Ordenar por prioridad (menor numero = mas urgente):
impl Ord for Task {
    fn cmp(&self, other: &Self) -> Ordering {
        // Comparar por priority, y en caso de empate por name:
        self.priority.cmp(&other.priority)
            .then_with(|| self.name.cmp(&other.name))
    }
}

// PartialOrd debe ser consistente con Ord:
impl PartialOrd for Task {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

fn main() {
    let mut tasks = vec![
        Task { priority: 2, name: "tests".to_string() },
        Task { priority: 0, name: "hotfix".to_string() },
        Task { priority: 1, name: "review".to_string() },
        Task { priority: 1, name: "build".to_string() },
    ];

    tasks.sort();
    for t in &tasks {
        println!("[{}] {}", t.priority, t.name);
    }
    // [0] hotfix
    // [1] build
    // [1] review
    // [2] tests
}
```

### Ordenar floats: el problema y las soluciones

```rust
fn main() {
    let mut values = vec![3.0_f64, 1.0, f64::NAN, 2.0];

    // values.sort();
    // Error: f64 no implementa Ord (por NaN).

    // Solucion 1: sort_by con partial_cmp y un fallback:
    values.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
    println!("{:?}", values);  // [1.0, 2.0, 3.0, NaN]

    // Solucion 2: sort_by con total_cmp (Rust 1.62+):
    // total_cmp trata NaN como mayor que todo:
    values.sort_by(f64::total_cmp);
    println!("{:?}", values);  // [1.0, 2.0, 3.0, NaN]
}
```

### Relacion entre PartialOrd, Ord, PartialEq, Eq

```rust
// Jerarquia de requisitos:
//
//   PartialEq          (base: ==, !=)
//       |
//       ├── Eq          (marker: reflexividad garantizada)
//       |
//       └── PartialOrd  (base: <, >, <=, >=, partial_cmp)
//               |
//               └── Ord  (requiere Eq + PartialOrd: cmp, sort)
//
// Para derivar Ord, necesitas TODOS los anteriores:
// #[derive(PartialEq, Eq, PartialOrd, Ord)]
//
// Consistencia obligatoria:
// - Si a.cmp(b) == Equal, entonces a == b (Ord consistente con Eq).
// - Si a.partial_cmp(b) == Some(Less), entonces a < b.
// - partial_cmp debe retornar Some(cmp(a, b)) cuando Ord esta implementado.
```

### Orden inverso con Reverse

```rust
use std::cmp::Reverse;

fn main() {
    let mut numbers = vec![3, 1, 4, 1, 5, 9];

    // Orden descendente usando Reverse:
    numbers.sort_by_key(|&x| Reverse(x));
    println!("{:?}", numbers);  // [9, 5, 4, 3, 1, 1]

    // Equivalente sin Reverse:
    numbers.sort_by(|a, b| b.cmp(a));
    println!("{:?}", numbers);  // [9, 5, 4, 3, 1, 1]
}
```

### BTreeMap requiere Ord

```rust
use std::collections::BTreeMap;

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
struct Name(String);

fn main() {
    let mut map = BTreeMap::new();
    map.insert(Name("Charlie".to_string()), 3);
    map.insert(Name("Alice".to_string()), 1);
    map.insert(Name("Bob".to_string()), 2);

    // BTreeMap mantiene las claves ordenadas:
    for (name, val) in &map {
        println!("{:?} -> {}", name, val);
    }
    // Name("Alice") -> 1
    // Name("Bob") -> 2
    // Name("Charlie") -> 3
}
```

---

## Hash

El trait `Hash` permite calcular un **hash** (valor numerico
resumido) de un valor. Es necesario para usar un tipo como clave
en `HashMap` y `HashSet`.

### Uso basico con derive

```rust
use std::collections::{HashMap, HashSet};

#[derive(Debug, PartialEq, Eq, Hash)]
struct StudentId(u32);

fn main() {
    // Como clave de HashMap:
    let mut grades: HashMap<StudentId, f64> = HashMap::new();
    grades.insert(StudentId(1001), 9.5);
    grades.insert(StudentId(1002), 8.0);

    assert_eq!(grades.get(&StudentId(1001)), Some(&9.5));

    // En un HashSet:
    let mut enrolled: HashSet<StudentId> = HashSet::new();
    enrolled.insert(StudentId(1001));
    enrolled.insert(StudentId(1002));
    enrolled.insert(StudentId(1001));  // duplicado, no se agrega

    assert_eq!(enrolled.len(), 2);
}
```

### El trait Hash

```rust
// Definicion simplificada:
// pub trait Hash {
//     fn hash<H: Hasher>(&self, state: &mut H);
//
//     fn hash_slice<H: Hasher>(data: &[Self], state: &mut H)
//     where
//         Self: Sized,
//     {
//         // implementacion default
//     }
// }
//
// hash() alimenta el Hasher con los bytes relevantes del valor.
// El Hasher acumula los datos y produce un u64 al final.
```

### Consistencia con Eq — la regla fundamental

```rust
// REGLA CRITICA:
// Si a == b (segun Eq), entonces hash(a) == hash(b).
//
// Es decir: valores iguales DEBEN producir el mismo hash.
// (Valores distintos PUEDEN producir el mismo hash — es una colision.)
//
// Si violas esta regla, HashMap/HashSet se rompen silenciosamente:
// podrias insertar un valor y no encontrarlo al buscarlo.

// Ejemplo de violacion (NO hacer esto):
// impl PartialEq for Bad {
//     fn eq(&self, other: &Self) -> bool { self.id == other.id }
// }
// impl Hash for Bad {
//     fn hash<H: Hasher>(&self, state: &mut H) {
//         self.name.hash(state);  // MAL: usa name, no id
//     }
// }
// Dos Bad con mismo id pero distinto name serian "iguales" segun Eq
// pero tendrian distinto hash. HashMap no los encontraria.
```

### Implementacion manual de Hash

```rust
use std::hash::{Hash, Hasher};

#[derive(Debug)]
struct Document {
    id: u64,
    title: String,
    content: String,     // campo pesado
    cached_size: usize,  // cache, no afecta identidad
}

// Igualdad solo por id:
impl PartialEq for Document {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl Eq for Document {}

// Hash DEBE ser consistente con Eq:
// Como Eq solo compara id, Hash solo debe usar id.
impl Hash for Document {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
        // NO incluir title, content ni cached_size.
    }
}

use std::collections::HashMap;

fn main() {
    let mut index: HashMap<Document, Vec<String>> = HashMap::new();

    let doc = Document {
        id: 1,
        title: "Rust Guide".to_string(),
        content: "...".to_string(),
        cached_size: 1024,
    };

    index.insert(doc, vec!["rust".to_string(), "programming".to_string()]);

    // Buscar con un Document que tiene el mismo id pero distinto cache:
    let lookup = Document {
        id: 1,
        title: "Rust Guide".to_string(),
        content: "...".to_string(),
        cached_size: 0,  // distinto cache, mismo id → mismo hash
    };

    assert!(index.contains_key(&lookup));  // funciona correctamente
}
```

### Hash para tipos con flotantes

```rust
// f32 y f64 NO implementan Hash (porque tampoco implementan Eq).
// NaN romperia la regla de consistencia.

// Si necesitas hashear un struct con floats, tienes opciones:

// Opcion 1: Wrapper que convierte a bits
use std::hash::{Hash, Hasher};

#[derive(Debug, Clone, Copy)]
struct OrderedFloat(f64);

impl PartialEq for OrderedFloat {
    fn eq(&self, other: &Self) -> bool {
        self.0.to_bits() == other.0.to_bits()
    }
}

impl Eq for OrderedFloat {}

impl Hash for OrderedFloat {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.to_bits().hash(state);
    }
}

// Opcion 2: usar el crate `ordered-float` del ecosistema.
// Es la solucion recomendada en produccion.
```

### Hasher personalizado

```rust
use std::collections::HashMap;
use std::hash::{BuildHasherDefault, Hasher};

// Rust usa SipHash por defecto (resistente a DoS).
// Para casos donde no necesitas esa proteccion, puedes usar
// hashers mas rapidos.

// Ejemplo conceptual: un hasher de identidad para u64:
#[derive(Default)]
struct IdentityHasher(u64);

impl Hasher for IdentityHasher {
    fn write(&mut self, bytes: &[u8]) {
        // Solo toma los primeros 8 bytes como u64:
        let mut buf = [0u8; 8];
        let len = bytes.len().min(8);
        buf[..len].copy_from_slice(&bytes[..len]);
        self.0 = u64::from_ne_bytes(buf);
    }

    fn finish(&self) -> u64 {
        self.0
    }
}

type IdentityBuildHasher = BuildHasherDefault<IdentityHasher>;

fn main() {
    // HashMap con hasher personalizado:
    let mut map: HashMap<u64, String, IdentityBuildHasher> = HashMap::default();
    map.insert(42, "answer".to_string());
    assert_eq!(map.get(&42), Some(&"answer".to_string()));

    // En la practica, usa crates como `ahash` o `fnv` en vez de
    // escribir tu propio hasher.
}
```

### Hash con derive — que genera

```rust
// derive(Hash) llama hash() en cada campo, en orden de declaracion.
// Todos los campos deben implementar Hash.

#[derive(Hash, PartialEq, Eq)]
struct Connection {
    src_ip: [u8; 4],
    dst_ip: [u8; 4],
    src_port: u16,
    dst_port: u16,
    protocol: u8,
}

// El derive genera algo equivalente a:
// impl Hash for Connection {
//     fn hash<H: Hasher>(&self, state: &mut H) {
//         self.src_ip.hash(state);
//         self.dst_ip.hash(state);
//         self.src_port.hash(state);
//         self.dst_port.hash(state);
//         self.protocol.hash(state);
//     }
// }
```

### Ejemplo completo: tipo como clave de mapa

```rust
use std::collections::HashMap;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct Endpoint {
    host: String,
    port: u16,
}

#[derive(Debug)]
struct ConnectionPool {
    pools: HashMap<Endpoint, Vec<String>>,  // conexiones por endpoint
}

impl ConnectionPool {
    fn new() -> Self {
        ConnectionPool {
            pools: HashMap::new(),
        }
    }

    fn add_connection(&mut self, endpoint: Endpoint, conn: String) {
        self.pools
            .entry(endpoint)
            .or_insert_with(Vec::new)
            .push(conn);
    }

    fn count(&self, endpoint: &Endpoint) -> usize {
        self.pools
            .get(endpoint)
            .map_or(0, |conns| conns.len())
    }
}

fn main() {
    let mut pool = ConnectionPool::new();

    let ep = Endpoint {
        host: "db.example.com".to_string(),
        port: 5432,
    };

    pool.add_connection(ep.clone(), "conn_1".to_string());
    pool.add_connection(ep.clone(), "conn_2".to_string());

    assert_eq!(pool.count(&ep), 2);
    println!("{:#?}", pool);
}
```

---

## Errores comunes

### Error 1 — Derivar Display (no se puede)

```rust
// INCORRECTO:
// #[derive(Display)]  // Error: cannot find derive macro `Display`
// struct Foo(i32);

// CORRECTO: implementar manualmente.
use std::fmt;

struct Foo(i32);

impl fmt::Display for Foo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Foo({})", self.0)
    }
}
```

### Error 2 — Derivar Copy sin Clone

```rust
// Copy requiere Clone como supertrait.

// INCORRECTO:
// #[derive(Copy)]
// struct Bar(i32);
// Error: the trait `Clone` is not implemented for `Bar`

// CORRECTO:
#[derive(Clone, Copy)]
struct Bar(i32);
```

### Error 3 — Derivar Eq sin PartialEq

```rust
// Eq requiere PartialEq.

// INCORRECTO:
// #[derive(Eq)]
// struct Baz(i32);
// Error: the trait `PartialEq` is not implemented for `Baz`

// CORRECTO:
#[derive(PartialEq, Eq)]
struct Baz(i32);
```

### Error 4 — Derivar Ord sin la cadena completa

```rust
// Ord requiere Eq + PartialOrd.
// Eq requiere PartialEq.

// INCORRECTO — falta PartialEq:
// #[derive(Eq, Ord, PartialOrd)]
// struct Qux(i32);

// CORRECTO — toda la cadena:
#[derive(PartialEq, Eq, PartialOrd, Ord)]
struct Qux(i32);
```

### Error 5 — Hash inconsistente con Eq

```rust
use std::collections::HashMap;
use std::hash::{Hash, Hasher};

#[derive(Debug)]
struct Player {
    id: u32,
    name: String,
    score: u32,
}

// Eq compara por id:
impl PartialEq for Player {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}
impl Eq for Player {}

// MAL: Hash usa name en vez de id:
// impl Hash for Player {
//     fn hash<H: Hasher>(&self, state: &mut H) {
//         self.name.hash(state);  // BUG: inconsistente con Eq
//     }
// }

// BIEN: Hash usa id, consistente con Eq:
impl Hash for Player {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}

fn main() {
    let mut map = HashMap::new();
    let p1 = Player { id: 1, name: "Alice".to_string(), score: 100 };
    map.insert(p1, "team_a");

    // Buscar con mismo id pero distinto name — debe funcionar:
    let lookup = Player { id: 1, name: "alice".to_string(), score: 0 };
    assert_eq!(map.get(&lookup), Some(&"team_a"));  // OK con Hash consistente
}
```

### Error 6 — Copy en tipos que no deberian serlo

```rust
// Un tipo que "podria" ser Copy pero no deberia:

// MAL — Semaphore es solo un usize, pero copiarlo es un bug logico:
// #[derive(Clone, Copy)]
// struct Semaphore { permits: usize }
// Si copias un Semaphore, dos partes del codigo creen tener los mismos permisos.

// BIEN — Sin Copy, forzamos transferencia explicita de ownership:
#[derive(Clone)]
struct Semaphore { permits: usize }
```

### Error 7 — Orden incorrecto de campos con derive(Ord)

```rust
// derive(Ord) compara en orden de declaracion de campos.
// Si el orden de los campos no es el orden de prioridad deseado,
// el resultado es incorrecto.

// MAL — queremos ordenar por score descendente, luego por name:
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
struct Entry {
    name: String,   // se compara PRIMERO (no es lo que queremos)
    score: u32,     // se compara SEGUNDO
}

// BIEN — reordenar campos o implementar Ord manualmente:
#[derive(Debug, PartialEq, Eq)]
struct EntryFixed {
    name: String,
    score: u32,
}

impl Ord for EntryFixed {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        other.score.cmp(&self.score)          // descendente por score
            .then_with(|| self.name.cmp(&other.name))  // ascendente por name
    }
}

impl PartialOrd for EntryFixed {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
```

### Error 8 — Usar floats como clave de HashMap

```rust
// f64 no implementa Eq ni Hash, asi que no puede ser clave de HashMap.

// INCORRECTO:
// use std::collections::HashMap;
// let mut map: HashMap<f64, String> = HashMap::new();
// Error: the trait bound `f64: Eq` is not satisfied

// SOLUCION: usar un wrapper (ver seccion de Hash) o el crate ordered-float.
```

### Error 9 — Clonar cuando se puede pasar por referencia

```rust
// Patron comun de ineficiencia:
fn process_data(data: Vec<u8>) {
    println!("Processing {} bytes", data.len());
}

fn main() {
    let data = vec![0u8; 1_000_000];

    // MAL — clone innecesario si solo necesitas leer:
    // process_data(data.clone());

    // BIEN — pasar por referencia:
    fn process_data_ref(data: &[u8]) {
        println!("Processing {} bytes", data.len());
    }
    process_data_ref(&data);
}
```

---

## Tabla resumen y cheatsheet

### Tabla de traits: que hace cada uno

```text
Trait        | Operador/Uso              | Se puede derivar | Requisitos
-------------|---------------------------|-------------------|-------------------
Display      | {}, to_string()           | NO                | (ninguno)
Debug        | {:?}, {:#?}, dbg!()       | SI                | campos: Debug
Clone        | .clone(), .clone_from()   | SI                | campos: Clone
Copy         | asignacion copia, no move | SI                | Clone + campos Copy + no Drop
PartialEq    | ==, !=                    | SI                | campos: PartialEq
Eq           | (marker: x == x)          | SI                | PartialEq
PartialOrd   | <, >, <=, >=             | SI                | PartialEq + campos: PartialOrd
Ord          | sort(), cmp(), min/max    | SI                | Eq + PartialOrd
Hash         | HashMap key, HashSet      | SI                | campos: Hash
```

### Cadena de dependencias entre traits

```text
                    PartialEq
                   /         \
                 Eq        PartialOrd
                   \         /
                     Ord

Clone ← Copy

Display → ToString (blanket impl)

Hash ← debe ser consistente con → Eq
```

### Cuando derivar vs implementar manualmente

```text
Derivar (#[derive(...)]) cuando:
  - Quieres comparar/hashear TODOS los campos.
  - El comportamiento default (campo a campo) es correcto.
  - No necesitas ocultar datos (como en Debug para passwords).

Implementar manualmente cuando:
  - Solo algunos campos determinan la identidad (ej: solo el id).
  - Necesitas logica especial (ej: comparacion case-insensitive).
  - Quieres ocultar campos sensibles en Debug.
  - Display siempre es manual (Rust no asume formato).
  - El orden de comparacion no coincide con el orden de campos.
```

### Derive combinaciones comunes

```text
Caso de uso                              | Derives recomendados
-----------------------------------------|---------------------------------------
Tipo simple para debugging               | Debug
Tipo de dato inmutable con igualdad      | Debug, PartialEq, Eq
Tipo de dato que sera clave de HashMap   | Debug, PartialEq, Eq, Hash, Clone
Tipo de dato que necesita ordenarse      | Debug, PartialEq, Eq, PartialOrd, Ord
Tipo numerico pequeno (coordenadas)      | Debug, Clone, Copy, PartialEq
Struct de config copiable                | Debug, Clone, Copy, PartialEq, Eq
Enum de estados                          | Debug, Clone, Copy, PartialEq, Eq, Hash
Clave completa para BTreeMap             | Debug, Clone, PartialEq, Eq, PartialOrd, Ord
```

### Que traits implementan los tipos primitivos

```text
Tipo     | Display | Debug | Clone | Copy | PartialEq | Eq | PartialOrd | Ord | Hash
---------|---------|-------|-------|------|-----------|----|------------|-----|-----
i32      |   ✓    |   ✓   |   ✓  |  ✓   |     ✓     | ✓  |     ✓     |  ✓  |  ✓
f64      |   ✓    |   ✓   |   ✓  |  ✓   |     ✓     | ✗  |     ✓     |  ✗  |  ✗
bool     |   ✓    |   ✓   |   ✓  |  ✓   |     ✓     | ✓  |     ✓     |  ✓  |  ✓
char     |   ✓    |   ✓   |   ✓  |  ✓   |     ✓     | ✓  |     ✓     |  ✓  |  ✓
&str     |   ✓    |   ✓   |   ✗  |  ✓   |     ✓     | ✓  |     ✓     |  ✓  |  ✓
String   |   ✓    |   ✓   |   ✓  |  ✗   |     ✓     | ✓  |     ✓     |  ✓  |  ✓
Vec<T>   |   ✗    |   ✓*  |   ✓* |  ✗   |     ✓*    | ✓* |     ✓*    |  ✓* |  ✓*
```

```text
* = requiere que T implemente el mismo trait.

Notas sobre f64:
- No implementa Eq (NaN != NaN).
- No implementa Ord (NaN no se puede ordenar).
- No implementa Hash (seria inconsistente sin Eq).
- SI implementa PartialEq y PartialOrd (retornan false/None para NaN).
```

### Cheatsheet rapido de sintaxis

```rust
// --- Derivar todo lo comun ---
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct Id(u64);

// --- Display siempre es manual ---
use std::fmt;
impl fmt::Display for Id {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "ID-{}", self.0)
    }
}

// --- Clone manual ---
impl Clone for MyType {
    fn clone(&self) -> Self { /* ... */ todo!() }
}

// --- Copy es un marker (cuerpo vacio) ---
impl Copy for MyType {}

// --- PartialEq manual ---
impl PartialEq for MyType {
    fn eq(&self, other: &Self) -> bool { /* ... */ todo!() }
}

// --- Eq es un marker ---
impl Eq for MyType {}

// --- PartialOrd manual ---
impl PartialOrd for MyType {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))  // delegar a Ord si existe
    }
}

// --- Ord manual ---
impl Ord for MyType {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering { /* ... */ todo!() }
}

// --- Hash manual ---
use std::hash::{Hash, Hasher};
impl Hash for MyType {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // hashear los campos relevantes (consistente con Eq)
        todo!()
    }
}

struct MyType;
```

---

## Ejercicios

### Ejercicio 1 — Implementar Display, Debug, Clone, PartialEq

```rust
// Crear un struct `Color` con campos r, g, b (u8).
//
// 1. Derivar Debug y Clone.
// 2. Implementar Display para que muestre "#RRGGBB" en hexadecimal.
// 3. Implementar PartialEq manualmente para que dos colores sean
//    iguales si su diferencia por canal es menor a 5.
//    (ej: Color(100,100,100) == Color(103,102,101))
// 4. Verificar que Display y Debug producen salidas distintas.
// 5. Verificar que clone produce una copia independiente.
//
// Prediccion antes de ejecutar:
//   - println!("{}", Color{r:255, g:0, b:128}) → ?
//   - Color{r:0, g:0, b:0} == Color{r:4, g:4, b:4} → ?
//   - Color{r:0, g:0, b:0} == Color{r:5, g:0, b:0} → ?
```

### Ejercicio 2 — Copy vs Clone en la practica

```rust
// Crear dos structs:
//   - `Pixel` con campos x: u32, y: u32, color: u32 → derivar Copy + Clone
//   - `Image` con campos width: u32, height: u32, pixels: Vec<Pixel> → solo Clone
//
// 1. Crear un Pixel, asignarlo a otra variable, verificar que el original sigue vivo.
// 2. Crear un Image, intentar asignarlo sin clone → verificar error de compilacion.
// 3. Clonar el Image, modificar el clone, verificar que el original no cambio.
//
// Prediccion: ¿por que Image no puede ser Copy?
```

### Ejercicio 3 — HashMap con tipo custom como clave

```rust
// Crear un struct `Coordinate` con lat: i32 y lon: i32 (enteros
// para evitar el problema con floats).
//
// 1. Derivar los traits necesarios para usarlo como clave de HashMap.
// 2. Crear un HashMap<Coordinate, String> con varias ciudades.
// 3. Buscar una ciudad por coordenada.
// 4. Implementar Display para que muestre "(lat, lon)".
//
// Prediccion: ¿cuales son los traits minimos necesarios para
// usarlo como clave de HashMap? ¿Y de BTreeMap?
```
