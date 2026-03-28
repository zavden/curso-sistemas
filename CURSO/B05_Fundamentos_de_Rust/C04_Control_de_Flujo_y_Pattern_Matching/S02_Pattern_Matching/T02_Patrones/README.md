# T02 — Patrones

## Que es un patron

Un patron es una plantilla que Rust compara contra un valor.
Si el valor encaja en la plantilla, Rust ejecuta el codigo
asociado y **extrae** (bind) las partes del valor que interesan.
Los patrones aparecen en `match`, `if let`, `while let`, `let`,
`for` y en parametros de funciones.

```rust
let x = 42;
match x {
    0 => println!("zero"),         // patron literal
    1..=9 => println!("single"),   // patron de rango
    n => println!("{n}"),          // patron variable (captura)
}

// Patrones tambien aparecen en let, if let y parametros:
let (a, b) = (10, 20);
if let Some(val) = some_option { println!("{val}"); }
fn print_point(&(x, y): &(i32, i32)) { println!("({x}, {y})"); }
```

## Patrones literales

Un patron literal compara el valor contra una constante exacta.
Funciona con enteros, chars, bools y string slices:

```rust
let x = 3;
match x {
    1 => println!("one"),
    2 => println!("two"),
    3 => println!("three"),   // coincide
    _ => println!("other"),
}

let c = 'b';
match c {
    'a' => println!("first letter"),
    'z' => println!("last letter"),
    _ => println!("something else"),  // coincide
}

let active = true;
match active {
    true => println!("active"),       // coincide
    false => println!("inactive"),
}
// Con bools no necesitas _ porque true/false cubren todo.
```

```rust
let greeting: &str = "hello";
match greeting {
    "hello" => println!("hi there"),  // coincide
    "bye" => println!("goodbye"),
    _ => println!("unknown"),
}
// Solo funciona con &str, no con String.
// Para String: match s.as_str() { ... }

let temp: i32 = -5;
match temp {
    0 => println!("freezing"),
    -10 => println!("very cold"),
    -5 => println!("cold"),           // coincide — negativos OK
    _ => println!("other"),
}
```

## Patrones de variable

Cuando un patron es un nombre simple (sin path, sin literal),
captura el valor y lo vincula a esa variable dentro del brazo:

```rust
let x = 42;
match x {
    0 => println!("zero"),
    n => println!("got: {n}"),   // n captura 42
}
// Imprime: got: 42
```

```rust
// Shadowing: la variable del patron oculta variables externas.
let x = 10;
match x {
    x => println!("x is {x}"),
    // Este x es una NUEVA variable que captura el valor.
    // Siempre coincide, como _ pero con un nombre.
}
println!("outer x is {x}"); // el x externo sigue intacto
```

```rust
// Un patron variable siempre coincide — error logico comun:
let expected = 5;
let actual = 7;
match actual {
    expected => println!("match: {expected}"),
    // NO compara contra expected = 5, crea una nueva variable.
}
// Imprime: match: 7

// Para comparar contra una variable, usa un guard:
match actual {
    n if n == expected => println!("match"),
    n => println!("no match: {n}"),
}
// Imprime: no match: 7
```

## Wildcard _

El patron `_` coincide con cualquier valor sin vincularlo
a ninguna variable. Es el "descarte":

```rust
let x = 42;
match x {
    1 => println!("one"),
    _ => println!("something else"),  // catch-all
}

// _ en posiciones especificas:
let point = (3, 7, 11);
match point {
    (0, 0, 0) => println!("origin"),
    (x, _, _) => println!("x = {x}"),  // descarta y, z
}
```

```rust
// .. ignora multiples campos restantes:
let tuple = (1, 2, 3, 4, 5);
match tuple {
    (first, ..) => println!("{first}"),       // 1
    // (first, .., last) tambien funciona.
}
// .. solo se puede usar UNA VEZ en un patron.

// _x vincula pero suprime el warning de variable no usada.
// IMPORTANTE: _x SI mueve ownership. _ NO mueve.
let s = String::from("hello");
if let Some(_s) = Some(s) {}   // _s mueve s
// s ya no es valido.

let s2 = String::from("world");
if let Some(_) = Some(s2) {}   // _ NO mueve s2
println!("{s2}");              // OK — s2 sigue valido
```

## Patrones de rango

Los rangos inclusivos `..=` funcionan como patron para
enteros y chars:

```rust
let x = 3;
match x {
    1..=5 => println!("one through five"),   // coincide con 1,2,3,4,5
    6..=10 => println!("six through ten"),
    _ => println!("something else"),
}

let c = 'k';
match c {
    'a'..='j' => println!("first ten letters"),
    'k'..='z' => println!("rest of lowercase"),  // coincide
    'A'..='Z' => println!("uppercase"),
    _ => println!("other"),
}
```

```rust
let score = 85;
match score {
    0..=59 => println!("F"),
    60..=69 => println!("D"),
    70..=79 => println!("C"),
    80..=89 => println!("B"),    // coincide
    90..=100 => println!("A"),
    _ => println!("invalid"),
}
// Rangos exclusivos (0..5) existen en nightly pero no en stable.
// En stable, usar ..= (inclusivo) solamente.
```

## Patrones or

El operador `|` permite que un brazo coincida con multiples
patrones alternativos:

```rust
let x = 2;
match x {
    1 | 2 | 3 => println!("one, two, or three"),  // coincide
    4 | 5 => println!("four or five"),
    _ => println!("other"),
}

// Combinar con rangos:
let c = '\n';
match c {
    ' ' | '\t' | '\n' | '\r' => println!("whitespace"),
    'a'..='z' | 'A'..='Z' => println!("letter"),
    '0'..='9' => println!("digit"),
    _ => println!("other"),
}
```

```rust
// Todas las alternativas deben vincular las mismas variables:
let pair = (1, true);
match pair {
    (1, b) | (2, b) => println!("1 or 2, b = {b}"),
    // b se vincula en AMBAS alternativas — OK.
    _ => println!("other"),
}

// Esto NO compila:
// (1, b) | (2, _) => ...
// Error: b no se vincula en la segunda alternativa.
```

## Patrones de tupla

Desestructurar tuplas extrae sus componentes directamente:

```rust
let point = (3, 7);
match point {
    (0, 0) => println!("origin"),
    (x, 0) => println!("on x-axis at {x}"),
    (0, y) => println!("on y-axis at {y}"),
    (x, y) => println!("point at ({x}, {y})"),  // coincide
}

// Desestructurar en let:
let (x, y, z) = (10, 20, 30);
let (_, second, _) = (1, 2, 3);  // ignorar componentes
let (first, ..) = (1, 2, 3, 4);  // ignorar el resto

// En parametros de funcion:
fn distance((x, y): (f64, f64)) -> f64 {
    (x * x + y * y).sqrt()
}

// Tuplas anidadas:
let ((a, b), (c, d)) = ((1, 2), (3, 4));
```

## Patrones de struct

Desestructurar structs extrae los campos por nombre:

```rust
struct Point { x: i32, y: i32 }

let p = Point { x: 5, y: 10 };

// Forma completa:
let Point { x: px, y: py } = p;

// Shorthand (nombre de variable = nombre de campo):
let Point { x, y } = p;
// Point { x, y } es equivalente a Point { x: x, y: y }.
```

```rust
struct Point { x: i32, y: i32 }
let p = Point { x: 0, y: 7 };

match p {
    Point { x: 0, y } => println!("on y-axis at y = {y}"),
    // x: 0 es patron literal — solo coincide si x es 0.
    Point { x, y: 0 } => println!("on x-axis at x = {x}"),
    Point { x, y } => println!("({x}, {y})"),
}
// Imprime: on y-axis at y = 7

// .. ignora los campos restantes:
struct Point3D { x: i32, y: i32, z: i32 }
let p = Point3D { x: 1, y: 2, z: 3 };
let Point3D { x, .. } = p;  // ignora y, z

match p {
    Point3D { z: 0, .. } => println!("on the xy plane"),
    Point3D { x, y, .. } => println!("x={x}, y={y}"),
}
```

## Patrones de enum

Cada variante de un enum se desestructura segun su forma
(unit, tuple, struct):

```rust
enum Message {
    Quit,
    Echo(String),
    Move { x: i32, y: i32 },
    Color(u8, u8, u8),
}

let msg = Message::Move { x: 10, y: 20 };
match msg {
    Message::Quit => println!("quit"),
    Message::Echo(text) => println!("echo: {text}"),
    Message::Move { x, y } => println!("move to ({x}, {y})"),
    Message::Color(r, g, b) => println!("color: ({r}, {g}, {b})"),
}
// Imprime: move to (10, 20)
```

```rust
// Option<T> y Result<T, E> son enums estandar:
let value: Option<i32> = Some(42);
match value {
    Some(n) if n > 0 => println!("positive: {n}"),
    Some(n) => println!("non-positive: {n}"),
    None => println!("nothing"),
}

let result: Result<i32, String> = Ok(100);
match result {
    Ok(val) => println!("success: {val}"),
    Err(e) => println!("error: {e}"),
}

// Patrones anidados:
let nested: Option<Option<i32>> = Some(Some(7));
match nested {
    Some(Some(n)) => println!("inner: {n}"),
    Some(None) => println!("outer Some, inner None"),
    None => println!("nothing"),
}
```

```rust
// Enum con calculo:
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
    Triangle { base: f64, height: f64 },
}

fn area(shape: &Shape) -> f64 {
    match shape {
        Shape::Circle(r) => std::f64::consts::PI * r * r,
        Shape::Rectangle(w, h) => w * h,
        Shape::Triangle { base, height } => 0.5 * base * height,
    }
}
```

## ref y ref mut

`ref` en un patron toma una referencia al valor en vez de
moverlo o copiarlo. `ref mut` toma una referencia mutable:

```rust
// ref en match — evita mover el valor fuera del contenedor:
let data = Some(String::from("hello"));
match data {
    Some(ref s) => println!("got: {s}"),
    // s es &String — no mueve el String fuera de data.
    None => println!("nothing"),
}
println!("{:?}", data); // data sigue valido: Some("hello")

// ref mut para modificar in-place:
let mut data = Some(String::from("hello"));
match data {
    Some(ref mut s) => s.push_str(" world"),
    None => {}
}
println!("{:?}", data); // Some("hello world")
```

```rust
// Desde Rust 2018, ref se inserta automaticamente cuando
// haces match sobre una referencia (match ergonomics):

let data = Some(String::from("hello"));

// Rust 2015 — necesitas ref:
match data {
    Some(ref s) => println!("{s}"),
    None => {}
}

// Rust 2018+ — el compilador lo infiere:
match &data {
    Some(s) => println!("{s}"),   // s es automaticamente &String
    None => {}
}
// ref explicito sigue siendo necesario cuando NO
// tienes una referencia al valor original.
```

## @ bindings

El operador `@` vincula un valor a una variable **y al mismo
tiempo** lo compara contra un patron:

```rust
let x = 5;
match x {
    n @ 1..=12 => println!("month {n}"),
    // n captura el valor (5) Y verifica que este en 1..=12.
    // Sin @, el rango coincide pero no da nombre al valor.
    n @ 13..=31 => println!("day {n}"),
    other => println!("out of range: {other}"),
}
// Imprime: month 5
```

```rust
// @ con enum — capturar el valor completo y desestructurar:
let msg = Some(42);
match msg {
    whole @ Some(n) if n > 0 => {
        println!("whole: {:?}", whole); // Some(42)
        println!("inner: {n}");         // 42
    }
    _ => {}
}

// @ con struct patterns:
struct Request { method: String, version: u8 }
let req = Request { method: String::from("GET"), version: 2 };
match req {
    Request { version: v @ 1..=2, ref method } => {
        println!("{method} (HTTP/{v})");
    }
    Request { version, .. } => println!("unsupported: {version}"),
}
// Imprime: GET (HTTP/2)

// @ con or patterns — parentesis obligatorios:
let x = 15;
match x {
    n @ (1 | 5 | 10 | 15) => println!("notable: {n}"),
    n => println!("other: {n}"),
}
```

## Patrones de slice y array

Los patrones de slice desestructuran arrays y slices por posicion:

```rust
let arr = [1, 2, 3];
match arr {
    [1, _, _] => println!("starts with 1"),
    [_, _, 3] => println!("ends with 3"),
    _ => println!("other"),
}

// Con slices (longitud variable):
let slice: &[i32] = &[1, 2, 3, 4, 5];
match slice {
    [] => println!("empty"),
    [single] => println!("one: {single}"),
    [first, .., last] => println!("{first}..{last}"),
}
// Imprime: 1..5
```

```rust
// rest @ .. captura los elementos restantes:
let numbers: &[i32] = &[1, 2, 3, 4, 5];
match numbers {
    [first, rest @ ..] => {
        println!("head: {first}");       // 1
        println!("tail: {:?}", rest);    // [2, 3, 4, 5]
    }
    [] => println!("empty"),
}

// Literales y variables combinados:
let data: &[i32] = &[0, 1, 2];
match data {
    [0, rest @ ..] => println!("starts with 0, rest: {:?}", rest),
    [1, ..] => println!("starts with 1"),
    _ => println!("other"),
}
// Imprime: starts with 0, rest: [1, 2]

// Arrays (tamanio fijo) no necesitan catch-all:
let arr: [i32; 3] = [1, 2, 3];
match arr {
    [a, b, c] => println!("{a} {b} {c}"),
}
// Slices (longitud variable) SI necesitan cubrir otros tamanios.
```

## Match ergonomics

Desde Rust 2018, el compilador inserta automaticamente `ref`
cuando haces match sobre una referencia:

```rust
let data: Option<String> = Some(String::from("hello"));

// Match sobre &data — sin mover:
match &data {
    Some(s) => println!("{s}"),   // s es &String (auto-ref)
    None => println!("none"),
}
println!("{:?}", data); // data sigue valida

// Tambien con &mut:
let mut nums = Some(vec![1, 2, 3]);
match &mut nums {
    Some(v) => v.push(4),   // v es &mut Vec<i32> (auto-ref-mut)
    None => {}
}
println!("{:?}", nums); // Some([1, 2, 3, 4])
```

```rust
// La regla:
// &T matched against pattern P  → variables become &T
// &mut T against pattern P      → variables become &mut T
// Esto elimina la mayoria de usos explicitos de ref.

// Con structs:
struct Config { name: String, value: i32 }
let cfg = Config { name: String::from("timeout"), value: 30 };
match &cfg {
    Config { name, value } => println!("{name} = {value}"),
    // name es &String, value es &i32.
}
println!("{}", cfg.name); // cfg sigue valido
```

## Irrefutable vs refutable

Un patron **irrefutable** siempre coincide con cualquier valor
posible del tipo. Un patron **refutable** puede fallar:

```rust
// Irrefutable — siempre coincide:
let x = 5;
let (a, b) = (1, 2);

// Refutable — puede no coincidir:
// let Some(x) = None::<i32>;
// Error: "refutable pattern in local binding"
```

```rust
// Donde se acepta cada tipo:
//
// Solo irrefutable:
//   let <pattern> = expr;
//   for <pattern> in iter { }
//   fn foo(<pattern>: Type)
//
// Acepta refutable:
//   match expr { <pattern> => ... }
//   if let <pattern> = expr { }
//   while let <pattern> = expr { }

let val: Option<i32> = Some(42);
if let Some(x) = val { println!("{x}"); }       // refutable — OK

let mut stack = vec![1, 2, 3];
while let Some(top) = stack.pop() { println!("{top}"); }
// Imprime: 3, 2, 1 — termina cuando pop() devuelve None.
```

```rust
// let-else (Rust 1.65+) permite refutables en let
// con una rama de divergencia:
fn process(input: &str) {
    let Some((key, value)) = input.split_once('=') else {
        println!("invalid");
        return; // DEBE divergir (return, break, panic, etc.)
    };
    println!("{key} = {value}");
}

process("timeout=30");  // timeout = 30
process("invalid");     // invalid
```

## Combinando patrones

En la practica, los patrones se combinan en expresiones
complejas que desestructuran, filtran y capturan al mismo
tiempo:

```rust
enum Event {
    Click { x: i32, y: i32, button: u8 },
    Key(char),
    Resize(u32, u32),
    Quit,
}

fn handle(event: &Event) {
    match event {
        Event::Click { x, y, button: 1 } if *x >= 0 && *y >= 0 => {
            println!("left click at ({x}, {y})");
        }
        Event::Key('q') | Event::Quit => println!("quitting"),
        Event::Key(c @ 'a'..='z') => println!("lowercase: {c}"),
        _ => println!("unhandled"),
    }
}
```

```rust
// Desestructuracion profunda con multiples niveles:
struct Db { connection: Option<Connection> }
struct Connection { host: String, port: u16 }

let db = Db {
    connection: Some(Connection {
        host: String::from("localhost"), port: 5432,
    }),
};

match &db {
    Db { connection: Some(Connection { host, port: p @ 1024.. }) } => {
        println!("connected to {host}:{p}");
    }
    Db { connection: Some(Connection { port, .. }) } => {
        println!("privileged port: {port}");
    }
    Db { connection: None } => println!("not connected"),
}
// Imprime: connected to localhost:5432
```

---

## Ejercicios

### Ejercicio 1 — Clasificador con patrones literales y rangos

```rust
// Escribir una funcion classify(n: i32) -> &'static str que use match con:
//   - Literales para 0 -> "zero"
//   - Rango negativo -100..=-1 -> "negative"
//   - Rango 1..=100 -> "positive"
//   - _ -> "out of range"
//
// Agregar una segunda funcion grade(score: u8) -> char:
//   0..=59 -> 'F', 60..=69 -> 'D', 70..=79 -> 'C',
//   80..=89 -> 'B', 90..=100 -> 'A', _ -> '?'
//
// Verificar:
//   classify(0)    == "zero"
//   classify(-50)  == "negative"
//   classify(200)  == "out of range"
//   grade(85)      == 'B'
//   grade(101)     == '?'
```

### Ejercicio 2 — Desestructuracion de structs y enums

```rust
// Definir:
//   struct Point { x: f64, y: f64 }
//   enum Shape {
//       Circle { center: Point, radius: f64 },
//       Rectangle { top_left: Point, bottom_right: Point },
//       Line(Point, Point),
//   }
//
// Escribir fn describe(shape: &Shape) que use match para imprimir:
//   Circle en el origen con radius=1 -> "unit circle at origin"
//   Circle general -> "circle at (x,y) r=radius"
//   Rectangle con top_left en origen -> "rectangle from origin to (x,y)"
//   Line -> "line from (x1,y1) to (x2,y2)"
//
// Usar: patrones de struct, literales en campos, guards, ..
// Verificar con al menos 4 shapes distintas.
```

### Ejercicio 3 — Slice patterns y @ bindings

```rust
// Escribir fn parse_command(args: &[&str]) -> String que use match con
// slice patterns:
//   [] -> "no command"
//   ["help"] -> "showing help"
//   ["get", url] -> "fetching {url}"
//   ["set", key, value] -> "setting {key} = {value}"
//   [cmd, rest @ ..] -> "unknown command {cmd} with {n} args"
//       donde n = rest.len()
//
// Escribir fn categorize(val: i32) -> String que use @ bindings:
//   n @ 1..=9 -> "single digit: {n}"
//   n @ (10 | 20 | 30) -> "tens: {n}"
//   n @ 100.. -> "large: {n}"
//   n -> "other: {n}"
//
// Verificar:
//   parse_command(&["get", "https://example.com"])
//   parse_command(&["deploy", "prod", "--force", "--yes"])
//   categorize(5), categorize(20), categorize(999), categorize(-3)
```
