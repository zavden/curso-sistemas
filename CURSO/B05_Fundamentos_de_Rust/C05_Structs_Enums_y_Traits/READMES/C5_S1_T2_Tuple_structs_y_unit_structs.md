# T02 — Tuple structs y unit structs

## Tuple structs

Una tuple struct es una struct cuyos campos no tienen nombre.
Es una **tupla con nombre**: tiene identidad de tipo propia,
pero los campos se acceden por posicion, no por nombre.

```rust
struct Color(u8, u8, u8);
struct Point(f64, f64);

fn main() {
    let red = Color(255, 0, 0);
    let origin = Point(0.0, 0.0);

    // Acceso por indice:
    println!("R: {}", red.0);       // 255
    println!("G: {}", red.1);       // 0
    println!("x: {}", origin.0);    // 0.0

    // Destructuring con let:
    let Color(r, g, b) = red;
    println!("RGB: ({}, {}, {})", r, g, b);

    // Destructuring parcial:
    let Color(r, _, _) = red;
    println!("Solo rojo: {}", r);

    // Destructuring en match:
    match origin {
        Point(x, y) if x == 0.0 && y == 0.0 => println!("Origin"),
        Point(x, y) => println!("({}, {})", x, y),
    }
}
```

```rust
// Destructuring en parametros de funcion:
struct Dimensions(f64, f64);

fn area(Dimensions(w, h): Dimensions) -> f64 {
    w * h
}

fn main() {
    println!("Area: {}", area(Dimensions(10.0, 5.0)));  // 50.0
}
```

## Newtype pattern

El newtype pattern consiste en envolver un **unico valor** en una
tuple struct de un solo campo. Crea un tipo distinto con significado
semantico propio, sin costo en tiempo de ejecucion.

```rust
struct Meters(f64);
struct Seconds(f64);

fn calculate_speed(distance: Meters, time: Seconds) -> f64 {
    distance.0 / time.0
}

fn main() {
    let d = Meters(100.0);
    let t = Seconds(9.58);

    // Tipos distintos — el compilador impide mezclarlos:
    // let wrong: Meters = t;               // ERROR: expected Meters, found Seconds
    // calculate_speed(t, d);               // ERROR: argumentos invertidos

    println!("Speed: {:.2}", calculate_speed(d, t));
}
```

```rust
// Zero-cost abstraction: mismo layout en memoria que el tipo interior.
use std::mem::size_of;

struct Meters(f64);

fn main() {
    println!("f64:    {} bytes", size_of::<f64>());     // 8
    println!("Meters: {} bytes", size_of::<Meters>());  // 8
    // El newtype no agrega ningun byte.
    // El compilador elimina la struct y trabaja directamente con f64.
}
```

```rust
// Implementar operaciones para el newtype:
use std::ops::Add;

#[derive(Debug, Clone, Copy)]
struct Meters(f64);

impl Add for Meters {
    type Output = Meters;
    fn add(self, other: Meters) -> Meters {
        Meters(self.0 + other.0)
    }
}

fn main() {
    let total = Meters(10.0) + Meters(32.195);
    println!("{:?}", total);  // Meters(42.195)
}
```

## Newtype para implementar traits en tipos externos

La regla de orfandad (orphan rule) dice que solo puedes implementar
un trait para un tipo si **al menos uno de los dos** esta definido
en tu crate. El newtype pattern permite sortear esta restriccion.

```rust
// Problema: Display y Vec<String> son externos a nuestro crate.
// impl fmt::Display for Vec<String> { ... }
// ERROR: orphan rule — ni Display ni Vec estan en nuestro crate.

// Solucion: envolver en un newtype.
use std::fmt;

struct WordList(Vec<String>);

impl fmt::Display for WordList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[{}]", self.0.join(", "))
    }
}

fn main() {
    let words = WordList(vec![
        String::from("hello"),
        String::from("world"),
    ]);
    println!("{}", words);  // [hello, world]
}
```

```rust
// Inconveniente: pierdes acceso directo a metodos del tipo interior.
// Solucion: implementar Deref.
use std::ops::Deref;

struct WordList(Vec<String>);

impl Deref for WordList {
    type Target = Vec<String>;
    fn deref(&self) -> &Vec<String> {
        &self.0
    }
}

fn main() {
    let words = WordList(vec![String::from("hello"), String::from("world")]);
    // Ahora metodos de Vec son accesibles directamente:
    println!("Length: {}", words.len());   // 2
    println!("First: {}", words[0]);       // hello
    // Para acceso mutable, implementar DerefMut tambien.
}
```

## Unit structs

Una unit struct es una struct sin campos. No contiene datos
y ocupa **cero bytes** en memoria. Se define con punto y coma.

```rust
use std::mem::size_of;

struct Marker;
struct Signal;

fn main() {
    let _m = Marker;    // sin parentesis ni llaves
    let _s = Signal;

    println!("Marker: {} bytes", size_of::<Marker>());  // 0
    println!("Signal: {} bytes", size_of::<Signal>());   // 0

    // Tamano cero, pero son tipos distintos:
    // una funcion que espera Marker no acepta Signal.
}
```

## Unit structs como marker types y en traits

El uso principal de unit structs es como **marker types**: tipos
sin datos que codifican informacion en el sistema de tipos. El
compilador verifica las restricciones en tiempo de compilacion,
sin ningun costo en runtime.

```rust
// Maquina de estados con unit structs como markers.
struct Disconnected;
struct Connected;

struct Connection<State> {
    address: String,
    _state: State,
}

impl Connection<Disconnected> {
    fn new(addr: &str) -> Connection<Disconnected> {
        Connection { address: String::from(addr), _state: Disconnected }
    }
    fn connect(self) -> Connection<Connected> {
        println!("Connected to {}", self.address);
        Connection { address: self.address, _state: Connected }
    }
}

impl Connection<Connected> {
    fn send(&self, data: &str) {
        println!("Sending '{}' to {}", data, self.address);
    }
    fn disconnect(self) -> Connection<Disconnected> {
        Connection { address: self.address, _state: Disconnected }
    }
}

fn main() {
    let conn = Connection::new("192.168.1.1");
    // conn.send("hello");  // ERROR: send no existe para Connection<Disconnected>

    let conn = conn.connect();
    conn.send("hello");     // OK: Connection<Connected> tiene send()

    let _conn = conn.disconnect();
    // conn ya fue consumido — no se puede usar.
}
// Disconnected y Connected no contienen datos. Su unico proposito
// es distinguir estados. Todo se verifica en compilacion.
```

```rust
// Unit structs como selectores de comportamiento via traits:
trait Formatter {
    fn format(&self, value: f64) -> String;
}

struct Decimal;
struct Scientific;
struct Percentage;

impl Formatter for Decimal {
    fn format(&self, value: f64) -> String { format!("{:.2}", value) }
}
impl Formatter for Scientific {
    fn format(&self, value: f64) -> String { format!("{:.4e}", value) }
}
impl Formatter for Percentage {
    fn format(&self, value: f64) -> String { format!("{:.1}%", value * 100.0) }
}

fn print_value(value: f64, fmt: &dyn Formatter) {
    println!("{}", fmt.format(value));
}

fn main() {
    let val = 0.12345;
    print_value(val, &Decimal);      // 0.12
    print_value(val, &Scientific);   // 1.2345e-1
    print_value(val, &Percentage);   // 12.3%
    // No almacenan nada — son selectores de comportamiento.
}
```

## Comparacion: named structs vs tuple structs vs unit structs

```rust
// 1. Named struct — campos con nombre.
//    Uso: cuando los nombres clarifican el significado.
struct User { name: String, age: u32, active: bool }

// 2. Tuple struct — campos por posicion.
//    Uso: cuando el contexto es obvio, o para crear newtypes.
struct Color(u8, u8, u8);
struct Meters(f64);

// 3. Unit struct — sin campos.
//    Uso: markers, selectores de tipo, implementar traits sin datos.
struct Marker;
```

```rust
// Guia de decision:
//
// Sin campos?         → Unit struct:  struct Marker;
// Un solo campo?      → Newtype:      struct UserId(u64);
// Pocos campos obvios → Tuple struct: struct Point(f64, f64);
// Campos con nombre   → Named struct: struct Config { port: u16, host: String }
//
// Regla general: si dudas entre tuple struct y named struct,
// elige named struct. Los nombres hacen el codigo mas legible.
```

```rust
// Resumen de sintaxis:
//
// Definicion:
//   struct Named { field: Type }       // named
//   struct Tuple(Type, Type);          // tuple
//   struct Unit;                       // unit
//
// Construccion:
//   Named { field: value }
//   Tuple(value, value)
//   Unit
//
// Acceso:
//   named.field            // por nombre
//   tuple.0, tuple.1       // por indice
//   (unit no tiene campos)
//
// Destructuring:
//   let Named { field } = named;
//   let Tuple(a, b) = tuple;
//
// Tamano en memoria:
//   Named/Tuple → suma de campos + padding
//   Unit        → 0 bytes (zero-sized type)
//   Newtype     → mismo tamano que el tipo interior
```

---

## Ejercicios

### Ejercicio 1 — Newtypes para unidades fisicas

```rust
// Definir: Meters(f64), Seconds(f64), MetersPerSecond(f64).
//
// Implementar:
//   fn calculate_speed(distance: Meters, time: Seconds) -> MetersPerSecond
//   Display para cada uno: "42.20 m", "9.58 s", "4.40 m/s"
//   Add para Meters (sumar dos distancias).
//
// Verificar:
//   1. calculate_speed(Meters(100.0), Seconds(9.58)) imprime correctamente.
//   2. Meters(10.0) + Meters(32.195) produce Meters(42.195).
//   3. calculate_speed(Seconds(1.0), Meters(1.0)) NO compila.
```

### Ejercicio 2 — State machine con unit structs

```rust
// Modelar un documento con estados: Draft -> Review -> Approved -> Published.
//
// struct Document<State> { title: String, content: String, _state: State }
// Unit structs: Draft, Review, Approved, Published.
//
// Metodos por estado:
//   Document<Draft>     → edit(&mut self, &str), submit(self) -> Document<Review>
//   Document<Review>    → approve(self) -> Document<Approved>, reject(self) -> Document<Draft>
//   Document<Approved>  → publish(self) -> Document<Published>
//   Document<Published> → content(&self) -> &str
//
// Verificar:
//   1. Document<Draft> puede llamar edit() y submit().
//   2. Document<Draft> NO puede llamar publish() (error de compilacion).
//   3. Document<Published> NO puede llamar edit() (error de compilacion).
```

### Ejercicio 3 — Newtype con orphan rule

```rust
// Crear SortedVec(Vec<i32>) que mantiene el vector siempre ordenado.
//
// Implementar:
//   SortedVec::new() -> SortedVec
//   SortedVec::insert(&mut self, i32) — inserta manteniendo orden
//   SortedVec::contains(&self, &i32) -> bool — busqueda binaria
//   Display para SortedVec — elementos separados por ", "
//
// Verificar:
//   1. Insertar 5, 2, 8, 1, 9 → Display muestra "1, 2, 5, 8, 9".
//   2. contains(&5) retorna true, contains(&3) retorna false.
//   3. println!("{}", sorted_vec) funciona gracias al newtype
//      (orphan rule impide impl Display for Vec<i32> directamente).
```
