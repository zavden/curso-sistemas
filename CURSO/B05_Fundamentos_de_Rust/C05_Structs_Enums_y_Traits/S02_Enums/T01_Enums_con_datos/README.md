# T01 — Enums con datos

## Que son los enums en Rust

Un enum (enumeracion) define un tipo que puede ser **una de varias
variantes**. A diferencia de C, donde los enums son simples enteros,
en Rust cada variante puede contener datos de tipos distintos. Esto
convierte a los enums de Rust en **tipos de datos algebraicos**
(algebraic data types), equivalentes a las tagged unions de otros
lenguajes pero con seguridad garantizada en tiempo de compilacion.

```rust
// Un enum define un tipo con multiples variantes posibles.
// Un valor del tipo solo puede ser UNA variante a la vez.

enum Direction {
    North,
    South,
    East,
    West,
}

let direction = Direction::North;
// direction es de tipo Direction, con valor North.
```

## Enums basicos (tipo C)

Las variantes sin datos se llaman **unit variants**. Representan
un conjunto finito de opciones sin informacion adicional.

```rust
#[derive(Debug, PartialEq, Clone, Copy)]
enum Color {
    Red,
    Green,
    Blue,
}

fn main() {
    let c = Color::Green;

    println!("{:?}", c);  // Green          (Debug)
    assert!(c == Color::Green);             // (PartialEq)

    let c2 = c;  // c sigue valido — Copy
    println!("{:?} {:?}", c, c2);  // Green Green
}
```

```rust
// match es la forma principal de inspeccionar un enum.
// El compilador exige cubrir TODAS las variantes.

fn color_hex(c: Color) -> &'static str {
    match c {
        Color::Red   => "#FF0000",
        Color::Green => "#00FF00",
        Color::Blue  => "#0000FF",
    }
    // Si olvidas una variante:
    // error: non-exhaustive patterns: `Blue` not covered
}
```

```rust
// Asignar valores enteros explicitos
// (util para interoperar con C o protocolos binarios):

#[repr(u16)]
enum HttpStatus {
    Ok = 200,
    NotFound = 404,
    InternalError = 500,
}

fn main() {
    let s = HttpStatus::Ok;
    println!("Codigo: {}", s as u16);  // Codigo: 200
}
```

## Enums con datos de tupla

Cuando los datos se especifican por posicion (sin nombres de campo),
se llaman **tuple variants**. Cada variante puede llevar datos de
tipos y cantidades distintas. Esto es lo que otros lenguajes llaman
"tagged union" o "sum type".

```rust
#[derive(Debug)]
enum Shape {
    Circle(f64),              // un campo: radio
    Rectangle(f64, f64),      // dos campos: ancho, alto
    Triangle(f64, f64, f64),  // tres campos: lados a, b, c
}

fn main() {
    let s1 = Shape::Circle(5.0);
    let s2 = Shape::Rectangle(4.0, 3.0);
    println!("{:?}", s1);  // Circle(5.0)
    println!("{:?}", s2);  // Rectangle(4.0, 3.0)
}
```

```rust
// Cada variante actua como funcion constructora:
// Shape::Circle es de tipo fn(f64) -> Shape.

let radii = vec![1.0, 2.0, 3.0];
let circles: Vec<Shape> = radii.into_iter().map(Shape::Circle).collect();
// [Circle(1.0), Circle(2.0), Circle(3.0)]
```

```rust
// Extraer datos con match:

fn area(shape: &Shape) -> f64 {
    match shape {
        Shape::Circle(r) => std::f64::consts::PI * r * r,
        Shape::Rectangle(w, h) => w * h,
        Shape::Triangle(a, b, c) => {
            let s = (a + b + c) / 2.0;
            (s * (s - a) * (s - b) * (s - c)).sqrt()
        }
    }
}

fn main() {
    println!("{:.2}", area(&Shape::Circle(5.0)));       // 78.54
    println!("{:.2}", area(&Shape::Rectangle(4.0, 3.0))); // 12.00
}
```

## Enums con campos nombrados

Las variantes pueden tener campos con nombre, como una struct.
Se llaman **struct variants** y son utiles cuando el significado
de cada campo no es obvio por su tipo.

```rust
#[derive(Debug)]
enum Shape {
    Circle { radius: f64 },
    Rectangle { width: f64, height: f64 },
}

fn main() {
    let c = Shape::Circle { radius: 5.0 };
    let r = Shape::Rectangle { width: 4.0, height: 3.0 };
    println!("{:?}", c);  // Circle { radius: 5.0 }
}
```

```rust
// Match con campos nombrados — se desestructura como una struct:

fn area(shape: &Shape) -> f64 {
    match shape {
        Shape::Circle { radius: r } => std::f64::consts::PI * r * r,
        Shape::Rectangle { width: w, height: h } => w * h,
    }
}

// Ignorar campos con ..:
fn is_wide(shape: &Shape) -> bool {
    match shape {
        Shape::Rectangle { width, .. } => *width > 10.0,
        _ => false,
    }
}
```

## Variantes mixtas

Un enum puede mezclar variantes unit, tupla y con campos nombrados
en la misma definicion. Este patron es muy comun en Rust.

```rust
#[derive(Debug)]
enum Message {
    Quit,                       // unit variant
    Move { x: i32, y: i32 },   // struct variant
    Write(String),              // tuple variant (1 campo)
    ChangeColor(u8, u8, u8),   // tuple variant (3 campos)
}

fn process(msg: &Message) {
    match msg {
        Message::Quit => println!("Saliendo"),
        Message::Move { x, y } => println!("Mover a ({}, {})", x, y),
        Message::Write(text) => println!("Texto: {}", text),
        Message::ChangeColor(r, g, b) => {
            println!("Color: #{:02X}{:02X}{:02X}", r, g, b)
        }
    }
}

fn main() {
    let msgs = vec![
        Message::Quit,
        Message::Move { x: 10, y: 20 },
        Message::Write(String::from("hello")),
        Message::ChangeColor(255, 128, 0),
    ];
    for m in &msgs { process(m); }
}
```

## Construccion y pattern matching

Ademas de `match`, Rust ofrece `if let`, `let-else`, `while let`
y guards para trabajar con enums.

```rust
#[derive(Debug)]
enum Expr {
    Literal(f64),
    Add(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>),
}

// if let — cuando solo importa una variante:
fn get_literal(expr: &Expr) -> Option<f64> {
    if let Expr::Literal(val) = expr {
        Some(*val)
    } else {
        None
    }
}

// let-else — extraer o salir temprano (Rust 1.65+):
fn must_be_literal(expr: &Expr) -> f64 {
    let Expr::Literal(val) = expr else {
        panic!("Se esperaba un literal");
    };
    *val
}
```

```rust
// Guards en match — condiciones adicionales:

fn classify(expr: &Expr) -> &str {
    match expr {
        Expr::Literal(v) if *v == 0.0 => "zero",
        Expr::Literal(v) if *v > 0.0  => "positive",
        Expr::Literal(_)               => "negative",
        Expr::Add(_, _)                => "addition",
        Expr::Neg(_)                   => "negation",
    }
}
```

```rust
// matches! — atajo para if let que retorna bool:

let e = Expr::Literal(5.0);
let is_lit = matches!(e, Expr::Literal(_));        // true
let is_pos = matches!(e, Expr::Literal(v) if v > 0.0); // true
```

## Metodos en enums

Los enums pueden tener bloques `impl` con metodos y funciones
asociadas, exactamente igual que las structs.

```rust
#[derive(Debug)]
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
}

impl Shape {
    // Funciones asociadas (constructores):
    fn unit_circle() -> Self { Shape::Circle(1.0) }
    fn unit_square() -> Self { Shape::Rectangle(1.0, 1.0) }

    // Metodo con &self:
    fn area(&self) -> f64 {
        match self {
            Shape::Circle(r) => std::f64::consts::PI * r * r,
            Shape::Rectangle(w, h) => w * h,
        }
    }

    fn perimeter(&self) -> f64 {
        match self {
            Shape::Circle(r) => 2.0 * std::f64::consts::PI * r,
            Shape::Rectangle(w, h) => 2.0 * (w + h),
        }
    }

    // Metodo que consume self:
    fn scale(self, factor: f64) -> Self {
        match self {
            Shape::Circle(r) => Shape::Circle(r * factor),
            Shape::Rectangle(w, h) => Shape::Rectangle(w * factor, h * factor),
        }
    }

    fn is_circle(&self) -> bool {
        matches!(self, Shape::Circle(_))
    }
}

fn main() {
    let c = Shape::unit_circle();
    println!("Area: {:.4}", c.area());           // 3.1416
    println!("Perimetro: {:.4}", c.perimeter()); // 6.2832

    let big = c.scale(10.0);
    println!("Area: {:.2}", big.area());         // 314.16
    println!("Circulo: {}", big.is_circle());    // true
}
```

## Tamano en memoria

El tamano de un enum es el de su **variante mas grande** mas un
**discriminante** (tag) que indica cual variante esta activa.

```rust
use std::mem::size_of;

enum Small {
    A,       // 0 bytes de datos
    B(u8),   // 1 byte de datos
}
// Tamano: 1 (dato) + 1 (discriminante) = 2 bytes

enum Medium {
    X(u8),   // 1 byte
    Y(u32),  // 4 bytes
    Z,       // 0 bytes
}
// Tamano: 4 (u32, el mas grande) + alineacion + discriminante = 8 bytes

fn main() {
    println!("Small: {}", size_of::<Small>());   // 2
    println!("Medium: {}", size_of::<Medium>());  // 8
}
```

```rust
// Niche optimization (optimizacion de nicho):
// El compilador detecta valores "imposibles" en un tipo
// y los usa como discriminante, ahorrando espacio.

use std::mem::size_of;

fn main() {
    // &T nunca es null → Option usa null para None:
    println!("&u32:         {}", size_of::<&u32>());          // 8
    println!("Option<&u32>: {}", size_of::<Option<&u32>>());  // 8
    // Mismo tamano! Lo mismo aplica a Box<T>.

    // Sin nicho, se necesita espacio extra para el tag:
    println!("Option<u32>: {}", size_of::<Option<u32>>());  // 8
    // u32 puede ser cualquier valor — no hay nicho disponible.

    // Implicacion: Option<&T> y Option<Box<T>> son "gratis"
    // en memoria. Rust usa Option en vez de null sin penalidad.
}
```

## Comparacion con C

```rust
// En C hay dos mecanismos separados: enum y union.

// C enum → solo alias para enteros, sin datos:
// enum color { RED, GREEN, BLUE };
// c = 42;  // legal, sin error. No hay exhaustividad en switch.

// C union → misma memoria para distintos tipos, sin tag:
// union data { int i; float f; };
// d.f = 3.14; printf("%d\n", d.i);  // comportamiento indefinido

// Tagged union manual en C → struct con enum tag + union:
// Problemas: nada impide leer el campo equivocado,
// el compilador no verifica exhaustividad,
// el programador debe sincronizar tag y datos.

// En Rust, un solo enum resuelve todo con seguridad:
enum Shape {
    Circle(f64),
    Rectangle { w: f64, h: f64 },
}

match s {
    Shape::Circle(r) => println!("r={}", r),
    Shape::Rectangle { w, h } => println!("{}x{}", w, h),
    // Si se agrega Triangle y se olvida: ERROR DE COMPILACION.
}
```

## Enums recursivos

Un enum puede referirse a si mismo. Como el compilador necesita
conocer el tamano en tiempo de compilacion, se necesita
**indirection** con `Box<T>` (puntero al heap, tamano fijo de
8 bytes en 64-bit).

```rust
// Sin Box, el compilador no puede calcular el tamano:
// enum List {
//     Cons(i32, List),  // ERROR: tamano infinito
//     Nil,
// }
// error[E0072]: recursive type `List` has infinite size

// Solucion: Box<T> rompe la recursion con un puntero.
#[derive(Debug)]
enum List {
    Cons(i32, Box<List>),
    Nil,
}

fn main() {
    let list = List::Cons(1,
        Box::new(List::Cons(2,
            Box::new(List::Cons(3,
                Box::new(List::Nil)
            ))
        ))
    );
    println!("{:?}", list);  // Cons(1, Cons(2, Cons(3, Nil)))

    // Tamano: i32(4) + Box(8) + discriminante + padding = 16 bytes
    println!("List: {} bytes", std::mem::size_of::<List>());  // 16
}
```

```rust
// Metodos recursivos sobre enums recursivos:

impl List {
    fn new() -> Self { List::Nil }

    fn prepend(self, value: i32) -> Self {
        List::Cons(value, Box::new(self))
    }

    fn len(&self) -> usize {
        match self {
            List::Nil => 0,
            List::Cons(_, rest) => 1 + rest.len(),
        }
    }

    fn sum(&self) -> i32 {
        match self {
            List::Nil => 0,
            List::Cons(val, rest) => val + rest.sum(),
        }
    }
}

fn main() {
    let list = List::new().prepend(3).prepend(2).prepend(1);
    println!("Len: {}", list.len());  // 3
    println!("Sum: {}", list.sum());  // 6
}
```

## Ejemplo completo

Combina enums basicos, con datos, variantes mixtas, metodos
y recursion en un evaluador de expresiones aritmeticas.

```rust
#[derive(Debug, Clone, Copy)]
enum Op { Add, Sub, Mul, Div }

#[derive(Debug)]
enum Expr {
    Num(f64),                                           // literal
    BinOp { op: Op, left: Box<Expr>, right: Box<Expr> }, // operacion
}

impl Expr {
    fn eval(&self) -> Result<f64, String> {
        match self {
            Expr::Num(v) => Ok(*v),
            Expr::BinOp { op, left, right } => {
                let l = left.eval()?;
                let r = right.eval()?;
                match op {
                    Op::Add => Ok(l + r),
                    Op::Sub => Ok(l - r),
                    Op::Mul => Ok(l * r),
                    Op::Div if r == 0.0 => Err(String::from("Division por cero")),
                    Op::Div => Ok(l / r),
                }
            }
        }
    }
}

fn main() {
    // (3 + 4) * 2
    let expr = Expr::BinOp {
        op: Op::Mul,
        left: Box::new(Expr::BinOp {
            op: Op::Add,
            left: Box::new(Expr::Num(3.0)),
            right: Box::new(Expr::Num(4.0)),
        }),
        right: Box::new(Expr::Num(2.0)),
    };

    match expr.eval() {
        Ok(v) => println!("= {}", v),       // = 14
        Err(e) => println!("Error: {}", e),
    }
}
```

---

## Ejercicios

### Ejercicio 1 — Enum basico y match

```rust
// Definir un enum Season con cuatro variantes: Spring, Summer, Autumn, Winter.
// Derivar Debug y PartialEq.
//
// Implementar:
//   fn temperature_range(season: &Season) -> (i32, i32)
//     Retorna (min, max) en grados Celsius tipicos.
//     Spring: (10, 22), Summer: (25, 40), Autumn: (8, 20), Winter: (-5, 10)
//
//   fn next(season: &Season) -> Season
//     Retorna la siguiente estacion (Winter -> Spring).
//
// En main:
//   1. Crear un array con las cuatro estaciones.
//   2. Iterar e imprimir cada estacion con su rango de temperatura.
//   3. Verificar que next de Winter es Spring (usar assert_eq!).
```

### Ejercicio 2 — Enum con datos y metodos

```rust
// Definir un enum Currency con variantes:
//   Dollar(f64)
//   Euro(f64)
//   Yen(f64)
//   Bitcoin(f64)
//
// Implementar en un bloque impl:
//   fn to_usd(&self) -> f64
//     Convertir a dolares usando tasas fijas:
//     Dollar: 1.0, Euro: 1.08, Yen: 0.0067, Bitcoin: 65000.0
//
//   fn display(&self) -> String
//     Formatear: "$100.00", "E50.00", "Y10000", "B0.50000000"
//     (Bitcoin con 8 decimales, Yen sin decimales, otros con 2)
//
// En main:
//   Crear un Vec<Currency> con al menos 4 valores.
//   Calcular el total en USD.
//   Imprimir cada moneda con su equivalente en USD.
```

### Ejercicio 3 — Enum recursivo

```rust
// Implementar una calculadora con un enum recursivo:
//
// enum Calc {
//     Num(f64),
//     Add(Box<Calc>, Box<Calc>),
//     Mul(Box<Calc>, Box<Calc>),
//     Div(Box<Calc>, Box<Calc>),
// }
//
// Implementar:
//   fn eval(&self) -> Result<f64, String>
//     Evaluar recursivamente. Div debe retornar Err si el divisor es 0.
//
//   fn count_ops(&self) -> usize
//     Contar cuantas operaciones (Add, Mul, Div) hay en el arbol.
//
//   fn depth(&self) -> usize
//     Profundidad del arbol (Num tiene profundidad 1).
//
// En main:
//   Construir la expresion (10 + 5) * (3 + 2)
//   Evaluar e imprimir el resultado (75).
//   Imprimir numero de operaciones (3) y profundidad (3).
//
//   Construir una expresion con division por cero.
//   Verificar que eval retorna Err.
```
