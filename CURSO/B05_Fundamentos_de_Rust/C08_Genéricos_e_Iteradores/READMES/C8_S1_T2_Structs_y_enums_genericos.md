# T02 — Structs y enums genericos

## Structs genericos

Un struct generico parametriza uno o mas tipos, permitiendo
reutilizar la misma definicion para distintos tipos de datos:

```rust
// Sin genericos — duplicacion:
struct PointI32 { x: i32, y: i32 }
struct PointF64 { x: f64, y: f64 }

// Con genericos — una sola definicion:
struct Point<T> {
    x: T,
    y: T,
}

let int_point = Point { x: 5, y: 10 };       // Point<i32>
let float_point = Point { x: 1.0, y: 4.0 };  // Point<f64>
```

```rust
// T se infiere del uso — no necesitas anotaciones normalmente:
let p = Point { x: 3, y: 7 };       // Rust infiere Point<i32>
let p = Point { x: 1.5, y: 2.5 };   // Rust infiere Point<f64>

// Pero ambos campos deben ser del MISMO tipo T:
// let p = Point { x: 5, y: 4.0 };  // error: expected integer, found float
// x es i32, y seria f64 — pero T es uno solo
```

### Multiples parametros de tipo

```rust
// Si necesitas que x e y sean de tipos DISTINTOS:
struct Point<T, U> {
    x: T,
    y: U,
}

let p = Point { x: 5, y: 4.0 };        // Point<i32, f64>
let p = Point { x: "hello", y: true };  // Point<&str, bool>

// T y U PUEDEN ser el mismo tipo:
let p = Point { x: 1, y: 2 };          // Point<i32, i32>
```

```rust
// Struct con valores heterogeneos:
struct KeyValue<K, V> {
    key: K,
    value: V,
}

let kv = KeyValue { key: "name", value: String::from("Alice") };
let kv = KeyValue { key: 42, value: vec![1, 2, 3] };
```

### Struct generico con campos mixtos

```rust
// No todos los campos necesitan ser genericos:
struct Labeled<T> {
    label: String,  // siempre String
    value: T,       // generico
}

let item = Labeled { label: "count".to_string(), value: 42 };
let item = Labeled { label: "pi".to_string(), value: 3.14 };

// Struct con un generico y otros tipos fijos:
struct Response<T> {
    status: u16,
    message: String,
    data: T,
}

let r = Response { status: 200, message: "OK".into(), data: vec![1, 2, 3] };
let r = Response { status: 404, message: "Not found".into(), data: () };
```

## Enums genericos

Los enums genericos siguen la misma logica. Los dos ejemplos
mas importantes de Rust son genericos: `Option<T>` y `Result<T, E>`.

### Option\<T\> — un valor que puede no existir

```rust
// Definicion en la stdlib:
enum Option<T> {
    Some(T),
    None,
}

// Un solo parametro de tipo T:
let x: Option<i32> = Some(42);
let y: Option<i32> = None;

let name: Option<String> = Some("Alice".to_string());
let empty: Option<String> = None;

// T se infiere:
let x = Some(42);        // Option<i32>
let x = Some("hello");   // Option<&str>

// None necesita anotacion si no hay contexto:
let x: Option<i32> = None;
// let x = None;  // error: type annotations needed
```

```rust
// Option reemplaza null/nil/None de otros lenguajes.
// La diferencia: el compilador te OBLIGA a manejar el None.

fn find_user(id: u64) -> Option<String> {
    if id == 1 {
        Some("Alice".to_string())
    } else {
        None
    }
}

// DEBES manejar ambos casos:
match find_user(1) {
    Some(name) => println!("encontrado: {name}"),
    None       => println!("no encontrado"),
}

// if let para cuando solo te interesa Some:
if let Some(name) = find_user(1) {
    println!("{name}");
}
```

### Result\<T, E\> — exito o error

```rust
// Definicion en la stdlib:
enum Result<T, E> {
    Ok(T),
    Err(E),
}

// DOS parametros de tipo: T para exito, E para error:
let ok: Result<i32, String> = Ok(42);
let err: Result<i32, String> = Err("fallo".to_string());

// Uso tipico:
fn divide(a: f64, b: f64) -> Result<f64, String> {
    if b == 0.0 {
        Err("division por cero".to_string())
    } else {
        Ok(a / b)
    }
}

match divide(10.0, 3.0) {
    Ok(result) => println!("resultado: {result}"),
    Err(msg)   => eprintln!("error: {msg}"),
}
```

```rust
// Result con tipos de error de la stdlib:
use std::fs;
use std::io;
use std::num::ParseIntError;

fn read_number(path: &str) -> Result<i64, io::Error> {
    let content = fs::read_to_string(path)?;
    // Nota: parse() retorna Result<i64, ParseIntError>, no io::Error
    // Necesitarias conversion de error (tema de manejo de errores)
    Ok(content.trim().parse().map_err(|e| {
        io::Error::new(io::ErrorKind::InvalidData, e)
    })?)
}
```

### Enums genericos propios

```rust
// Enum generico con multiples variantes:
enum Shape<T> {
    Circle(T),           // radio
    Rectangle(T, T),     // ancho, alto
    Triangle(T, T, T),   // tres lados
}

let c = Shape::Circle(5.0_f64);
let r = Shape::Rectangle(3, 4);

// Enum que modela un estado con datos:
enum LoadState<T> {
    NotStarted,
    Loading { progress: f32 },
    Done(T),
    Failed(String),
}

let state: LoadState<Vec<u8>> = LoadState::Loading { progress: 0.75 };
let state: LoadState<Vec<u8>> = LoadState::Done(vec![1, 2, 3]);
```

```rust
// Enum generico para un arbol binario:
enum Tree<T> {
    Leaf(T),
    Node {
        value: T,
        left: Box<Tree<T>>,
        right: Box<Tree<T>>,
    },
}

let tree = Tree::Node {
    value: 1,
    left: Box::new(Tree::Leaf(2)),
    right: Box::new(Tree::Node {
        value: 3,
        left: Box::new(Tree::Leaf(4)),
        right: Box::new(Tree::Leaf(5)),
    }),
};
// Box es necesario porque Tree es recursivo (tamaño infinito sin indireccion)
```

## Metodos en tipos genericos

```rust
// impl con parametros genericos:
struct Point<T> {
    x: T,
    y: T,
}

// Metodos para CUALQUIER T:
impl<T> Point<T> {
    fn new(x: T, y: T) -> Self {
        Point { x, y }
    }

    fn x(&self) -> &T {
        &self.x
    }

    fn y(&self) -> &T {
        &self.y
    }
}

// Metodos solo para tipos especificos:
impl Point<f64> {
    fn distance_to_origin(&self) -> f64 {
        (self.x * self.x + self.y * self.y).sqrt()
    }
}

let p = Point::new(3.0, 4.0);
println!("{}", p.distance_to_origin());  // 5.0

let p = Point::new(3, 4);
// p.distance_to_origin();  // error: no existe para Point<i32>
```

```rust
// Metodos con bounds:
impl<T: std::fmt::Display> Point<T> {
    fn describe(&self) -> String {
        format!("({}, {})", self.x, self.y)
    }
}

// Solo disponible si T implementa Display:
let p = Point::new(1, 2);
println!("{}", p.describe());  // (1, 2)
```

```rust
// Metodos con parametros genericos ADICIONALES:
impl<T> Point<T> {
    fn mixup<U>(self, other: Point<U>) -> Point<T, U>
    where
        // Nota: esto requiere que Point acepte 2 type params
        // Redefiniria Point como Point<T, U>
    {
        todo!()
    }
}

// Ejemplo real de la stdlib — map en Option:
// impl<T> Option<T> {
//     fn map<U, F: FnOnce(T) -> U>(self, f: F) -> Option<U>
//     El metodo introduce un nuevo tipo U distinto de T
// }
let x: Option<i32> = Some(42);
let y: Option<String> = x.map(|n| format!("valor: {n}"));
// T = i32, U = String — el metodo convierte Option<i32> → Option<String>
```

## Layout en memoria

```rust
// Los genericos no agregan overhead en memoria.
// Point<i32> tiene exactamente el tamaño de dos i32:
use std::mem::size_of;

assert_eq!(size_of::<Point<i32>>(), 8);    // 4 + 4
assert_eq!(size_of::<Point<f64>>(), 16);   // 8 + 8
assert_eq!(size_of::<Point<u8>>(), 2);     // 1 + 1

// Option<T> tiene optimizacion de nicho para ciertos tipos:
assert_eq!(size_of::<Option<&i32>>(), 8);      // mismo que &i32
assert_eq!(size_of::<&i32>(), 8);              // 8 bytes (puntero)
// Option<&T> usa el valor null como None — zero-cost!

assert_eq!(size_of::<Option<i32>>(), 8);       // 4 (i32) + 4 (discriminante)
// Para i32 no hay nicho — necesita espacio extra para el discriminante

// Result<T, E> tiene tamaño = max(T, E) + discriminante (+ padding)
assert_eq!(size_of::<Result<i32, i32>>(), 8);  // 4 + 4 (discriminante)
```

## Errores comunes

```rust
// ERROR 1: olvidar <T> en el impl
struct Wrapper<T> {
    inner: T,
}

// impl Wrapper<T> {  // error: T no definido
impl<T> Wrapper<T> {  // correcto: declarar T en impl<T>
    fn get(&self) -> &T {
        &self.inner
    }
}

// ERROR 2: bounds en el struct (generalmente innecesario)
// MAL:
struct Container<T: Clone> {
    data: T,
}
// Esto fuerza Clone en TODOS los usos de Container,
// incluso cuando no necesitas clonar.

// BIEN — bounds en el impl, no en el struct:
struct Container<T> {
    data: T,
}

impl<T: Clone> Container<T> {
    fn duplicate(&self) -> Self {
        Container { data: self.data.clone() }
    }
}
// Ahora Container<T> existe para cualquier T,
// pero duplicate() solo esta disponible si T: Clone.

// ERROR 3: confundir parametros del struct con los del metodo
struct Pair<T> {
    first: T,
    second: T,
}

impl<T> Pair<T> {
    // U es un parametro del METODO, no del struct
    fn replace_first<U>(self, new_first: U) -> (U, T) {
        (new_first, self.second)
    }
}

// ERROR 4: T sin bounds intentando operar
struct Stats<T> {
    values: Vec<T>,
}

impl<T> Stats<T> {
    fn sum(&self) -> T {
        // self.values.iter().sum()  // error: T no implementa Sum
        todo!()
    }
}
// Solucion: impl<T: std::iter::Sum + Copy> Stats<T>
```

## Cheatsheet

```text
Struct generico:
  struct Foo<T> { field: T }            un parametro
  struct Foo<T, U> { a: T, b: U }      multiples
  struct Foo<T> { label: String, val: T } mixto

Enum generico:
  enum Foo<T> { A(T), B }              un parametro
  enum Foo<T, E> { Ok(T), Err(E) }     multiples
  enum Foo<T> { Leaf(T), Node { val: T, children: Vec<Foo<T>> } }

Metodos:
  impl<T> Foo<T> { ... }               para cualquier T
  impl Foo<i32> { ... }                solo para i32
  impl<T: Trait> Foo<T> { ... }        para T con bound

stdlib:
  Option<T>     → Some(T) | None
  Result<T, E>  → Ok(T) | Err(E)
  Vec<T>        → array dinamico
  HashMap<K, V> → tabla hash

Memoria:
  - Sin overhead (monomorphization)
  - Option<&T> = mismo tamaño que &T (niche optimization)
  - Bounds en impl, no en struct (regla idiomatica)
```

---

## Ejercicios

### Ejercicio 1 — Struct generico

```rust
// Define un struct Pair<T> con campos first: T y second: T.
// Implementa:
//   fn new(first: T, second: T) -> Self
//   fn swap(self) -> Pair<T>   — retorna con los campos invertidos
//   fn contains(&self, value: &T) -> bool  — true si first o second == value
//       (necesita bound T: PartialEq)
//
// Prueba con Pair<i32>, Pair<String>, Pair<&str>.
// Predice: ¿compila Pair::new(1, "hello")? ¿Por que?
```

### Ejercicio 2 — Enum generico

```rust
// Define un enum Either<L, R> con variantes Left(L) y Right(R).
// Implementa:
//   fn is_left(&self) -> bool
//   fn is_right(&self) -> bool
//   fn left(self) -> Option<L>   — retorna Some si es Left, None si Right
//   fn right(self) -> Option<R>  — retorna Some si es Right, None si Left
//   fn map_left<U>(self, f: impl FnOnce(L) -> U) -> Either<U, R>
//
// Prueba:
//   let e: Either<i32, String> = Either::Left(42);
//   assert!(e.is_left());
//   assert_eq!(e.left(), Some(42));
```

### Ejercicio 3 — Tamaños en memoria

```rust
// Usa std::mem::size_of para verificar el tamaño de:
//   Point<u8>    — predice antes de ejecutar
//   Point<i32>
//   Point<f64>
//   Option<i32>
//   Option<&str>
//   Option<Box<i32>>
//   Result<u8, u8>
//   Result<i32, String>
//
// ¿Cuales tienen niche optimization?
// ¿Cuales son mas grandes de lo que esperabas?
```
