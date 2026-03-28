# T01 — match

## Que es match

`match` es la construccion de pattern matching de Rust. Toma un valor
y lo compara contra una serie de patrones. El primer patron que
coincida ejecuta su expresion asociada. Es similar al `switch` de C,
pero con diferencias fundamentales: es exhaustivo (obliga a cubrir
todos los casos), no tiene fall-through y es una expresion que
retorna un valor.

```rust
// Sintaxis basica:
// match value {
//     pattern => expression,
// }
//
// Cada linea dentro del match se llama "arm" (brazo).
// Los arms se separan con comas.
// La coma del ultimo arm es opcional (pero recomendada por estilo).

let number = 3;

match number {
    1 => println!("one"),
    2 => println!("two"),
    3 => println!("three"),
    _ => println!("something else"),
}
// three
```

```rust
// Cuando un arm tiene multiples lineas, se usan llaves:

let x = 2;

match x {
    1 => {
        println!("found one");
        println!("it's the first number");
    },
    2 => {
        println!("found two");
        println!("it's the second number");
    },
    _ => println!("other"),
}
// found two
// it's the second number
```

## Exhaustividad

El compilador de Rust exige que un `match` cubra **todos** los
valores posibles del tipo. Si falta un caso, el codigo no compila:

```rust
// ESTO NO COMPILA — falta cubrir valores:
let x: u8 = 5;

match x {
    0 => println!("zero"),
    1 => println!("one"),
}
// error[E0004]: non-exhaustive patterns: `2_u8..=u8::MAX` not covered
//
// u8 tiene 256 valores posibles (0..=255).
// Solo cubrimos 0 y 1 — faltan 254 casos.
```

```rust
// Solucion: usar _ (wildcard) para capturar todo lo demas.
// _ coincide con cualquier valor y descarta el dato.

let x: u8 = 5;

match x {
    0 => println!("zero"),
    1 => println!("one"),
    _ => println!("other"),
}
// other

// Alternativa: una variable captura el valor (como _ pero con binding):
match x {
    0 => println!("zero"),
    1 => println!("one"),
    other => println!("got: {}", other),
}
// got: 5
```

```rust
// Por que importa la exhaustividad:
//
// 1. Si agregas una variante nueva a un enum, el compilador marca
//    TODOS los match que no la cubren. Imposible olvidarse.
//
// 2. Comparacion con switch en C:
//    - switch NO es exhaustivo. Si falta un case, bug silencioso.
//    - Si olvidas break, hay fall-through accidental.
//    - En Rust no existe fall-through y match obliga cobertura total.
//
// 3. El wildcard _ debe ir como ultimo arm.
//    Si lo pones antes, los arms posteriores son inalcanzables.
```

## match con enums

Los enums son el caso de uso principal de `match`. Cada variante
del enum se convierte en un patron:

```rust
// match con Option<T> y Result<T, E>:

let maybe: Option<i32> = Some(42);

match maybe {
    Some(x) => println!("got: {}", x),
    None => println!("nothing"),
}
// got: 42

let result: Result<i32, String> = Ok(100);

match result {
    Ok(val) => println!("success: {}", val),
    Err(e) => println!("error: {}", e),
}
// success: 100
```

```rust
// match con enums propios:

enum Direction {
    North,
    South,
    East,
    West,
}

let dir = Direction::North;

match dir {
    Direction::North => println!("going north"),
    Direction::South => println!("going south"),
    Direction::East => println!("going east"),
    Direction::West => println!("going west"),
}
// going north
//
// No necesitamos _ porque cubrimos las 4 variantes.
// Si agregaramos Direction::NorthEast al enum,
// este match dejaria de compilar.
```

```rust
// Enums con datos asociados:

enum Message {
    Quit,
    Echo(String),
    Move { x: i32, y: i32 },
    Color(u8, u8, u8),
}

let msg = Message::Move { x: 10, y: 20 };

match msg {
    Message::Quit => println!("quit"),
    Message::Echo(text) => println!("echo: {}", text),
    Message::Move { x, y } => println!("move to ({}, {})", x, y),
    Message::Color(r, g, b) => println!("color: ({}, {}, {})", r, g, b),
}
// move to (10, 20)
//
// Cada variante se destructura segun su forma:
// - Quit: sin datos
// - Echo(text): variante tuple con un String
// - Move { x, y }: variante struct con campos nombrados
// - Color(r, g, b): variante tuple con tres u8
```

## Destructuring en match

Los patrones de match pueden extraer datos de estructuras
compuestas: tuplas, structs, enums anidados:

```rust
// Destructuring de tuplas:

let point = (3, 7);

match point {
    (0, 0) => println!("origin"),
    (x, 0) => println!("on x-axis at {}", x),
    (0, y) => println!("on y-axis at {}", y),
    (x, y) => println!("at ({}, {})", x, y),
}
// at (3, 7)
```

```rust
// Destructuring de structs:

struct Point { x: i32, y: i32 }

let p = Point { x: 0, y: 5 };

match p {
    Point { x: 0, y } => println!("on y-axis at y={}", y),
    Point { x, y: 0 } => println!("on x-axis at x={}", x),
    Point { x, y } => println!("at ({}, {})", x, y),
}
// on y-axis at y=5
//
// Point { x: 0, y } — x debe ser 0, y se captura.
// Point { x, y } — shorthand: captura ambos campos por nombre.
```

```rust
// Destructuring anidado — multiples niveles en un solo patron:

let nested: Option<Result<i32, String>> = Some(Ok(42));

match nested {
    Some(Ok(val)) => println!("success: {}", val),
    Some(Err(e)) => println!("error: {}", e),
    None => println!("nothing"),
}
// success: 42
```

```rust
// .. para ignorar campos restantes en structs:

struct Config {
    debug: bool,
    verbose: bool,
    port: u16,
    host: String,
}

let cfg = Config {
    debug: true, verbose: false,
    port: 8080, host: String::from("localhost"),
};

match cfg {
    Config { debug: true, port, .. } => println!("debug on port {}", port),
    Config { port, .. } => println!("running on port {}", port),
}
// debug on port 8080
```

## Match guards

Un match guard es una condicion booleana adicional despues del
patron, introducida con `if`. El arm solo coincide si el patron
matchea **y** la condicion es verdadera:

```rust
let num = Some(4);

match num {
    Some(x) if x < 0 => println!("{} is negative", x),
    Some(x) if x == 0 => println!("zero"),
    Some(x) => println!("{} is positive", x),
    None => println!("nothing"),
}
// 4 is positive
```

```rust
// El guard NO afecta la exhaustividad.
// El compilador no analiza la logica de los guards.

let x: i32 = 5;

// ESTO NO COMPILA:
// match x {
//     n if n < 0 => println!("negative"),
//     n if n >= 0 => println!("non-negative"),
// }
// error: non-exhaustive patterns
//
// Solucion: arm final sin guard:
match x {
    n if n < 0 => println!("negative"),
    n => println!("{} is non-negative", n),
}
// 5 is non-negative
```

```rust
// Guard con variables externas:

let target = 7;
let value = 7;

match value {
    x if x == target => println!("hit the target!"),
    x => println!("{} is not the target", x),
}
// hit the target!

// Guard combinado con destructuring de enum:
enum Temperature {
    Celsius(f64),
    Fahrenheit(f64),
}

let temp = Temperature::Celsius(35.0);

match temp {
    Temperature::Celsius(t) if t > 40.0 => println!("extremely hot"),
    Temperature::Celsius(t) if t > 30.0 => println!("hot: {}C", t),
    Temperature::Celsius(t) => println!("{}C", t),
    Temperature::Fahrenheit(t) if t > 100.0 => println!("hot: {}F", t),
    Temperature::Fahrenheit(t) => println!("{}F", t),
}
// hot: 35C
```

## Multiples patrones

El operador `|` permite combinar varios patrones en un solo arm.
El arm coincide si **cualquiera** de los patrones matchea:

```rust
let x = 3;

match x {
    1 | 2 | 3 => println!("small"),
    4 | 5 | 6 => println!("medium"),
    _ => println!("large"),
}
// small

// Con enums — cubrir variantes relacionadas:
enum Suit { Hearts, Diamonds, Clubs, Spades }

let card = Suit::Hearts;

match card {
    Suit::Hearts | Suit::Diamonds => println!("red"),
    Suit::Clubs | Suit::Spades => println!("black"),
}
// red
```

```rust
// | con bindings — todas las alternativas deben
// bindear las mismas variables con los mismos tipos:

enum Command {
    Start(String),
    Resume(String),
    Stop,
}

let cmd = Command::Start(String::from("server"));

match cmd {
    Command::Start(name) | Command::Resume(name) => {
        println!("activating: {}", name);
    },
    Command::Stop => println!("stopping"),
}
// activating: server
```

```rust
// Rangos inclusivos con ..=

let score: u8 = 85;

match score {
    0..=59 => println!("fail"),
    60..=69 => println!("D"),
    70..=79 => println!("C"),
    80..=89 => println!("B"),
    90..=100 => println!("A"),
    101..=255 => println!("invalid score"),
}
// B
//
// Todos los arms juntos cubren 0..=255 — exhaustivo para u8.
```

## match como expresion

`match` es una expresion: retorna un valor. Todos los arms deben
retornar el mismo tipo:

```rust
let status = 404;

let msg = match status {
    200 => "ok",
    301 => "moved permanently",
    404 => "not found",
    500 => "internal server error",
    _ => "unknown",
};

println!("{}: {}", status, msg);
// 404: not found
//
// Nota el ; despues de }; — match es una expresion dentro de let.
```

```rust
// Todos los arms deben retornar el mismo tipo:

// ESTO NO COMPILA:
// let val = match x {
//     1 => 10,         // i32
//     2 => "hello",    // &str  <-- tipo diferente
//     _ => 0,          // i32
// };
// error: match arms have incompatible types

// match en distintos contextos:
let x = 3;

println!("{}", match x {
    1 => "one",
    2 => "two",
    _ => "other",
});
// other
```

## Matching con referencias

Cuando el valor matcheado es una referencia, los patrones
interactuan con el sistema de ownership:

```rust
let value = 42;
let reference = &value;

// Los patrones matchean contra &T:
match reference {
    &42 => println!("forty-two"),
    &0 => println!("zero"),
    _ => println!("something else"),
}
// forty-two

// Dereferencing para matchear el valor interno:
match *reference {
    42 => println!("forty-two"),
    0 => println!("zero"),
    _ => println!("something else"),
}
// forty-two
```

```rust
// match sobre String sin mover el ownership — usar as_str():

let name = String::from("Alice");

match name.as_str() {
    "Alice" => println!("found Alice"),
    "Bob" => println!("found Bob"),
    _ => println!("unknown"),
}
// found Alice
// name.as_str() retorna &str. Si hicieramos match name { ... }
// moveriamos el String.

// ref en patrones — crea referencia en vez de mover:
let value = String::from("hello");

match value {
    ref s if s.len() > 3 => println!("long: {}", s),
    ref s => println!("short: {}", s),
}
// long: hello
// Despues del match, value sigue siendo accesible.
```

## Patrones comunes

Patrones frecuentes en codigo Rust real:

```rust
// Rangos de enteros y caracteres:

let http_status = 404;
let category = match http_status {
    200..=299 => "success",
    300..=399 => "redirection",
    400..=499 => "client error",
    500..=599 => "server error",
    _ => "unknown",
};
println!("{}: {}", http_status, category);
// 404: client error

let c = 'G';
match c {
    'a'..='z' => println!("lowercase"),
    'A'..='Z' => println!("uppercase"),
    '0'..='9' => println!("digit"),
    _ => println!("other"),
}
// uppercase
```

```rust
// Matching en tuplas — comparar multiples valores a la vez:

match (true, false) {
    (true, true) => println!("both true"),
    (true, false) => println!("first true, second false"),
    (false, true) => println!("first false, second true"),
    (false, false) => println!("both false"),
}
// first true, second false
```

```rust
// Option<Result<T, E>> aplanado:

fn parse_and_double(input: Option<&str>) -> String {
    let parsed = input.map(|s| s.parse::<i32>());
    match parsed {
        Some(Ok(n)) => format!("{}", n * 2),
        Some(Err(e)) => format!("parse error: {}", e),
        None => String::from("no input"),
    }
}

println!("{}", parse_and_double(Some("21")));   // 42
println!("{}", parse_and_double(Some("abc")));  // parse error: ...
println!("{}", parse_and_double(None));         // no input
```

```rust
// Binding con @ — capturar el valor mientras se compara:

let age: u8 = 25;

match age {
    0 => println!("newborn"),
    n @ 1..=12 => println!("child, age {}", n),
    n @ 13..=17 => println!("teenager, age {}", n),
    n @ 18..=64 => println!("adult, age {}", n),
    n @ 65..=255 => println!("senior, age {}", n),
}
// adult, age 25
//
// n @ 1..=12: si el valor esta en 1..=12, capturalo en n.
```

```rust
// Match dentro de impl — transformar enum a otros tipos:

enum LogLevel { Error, Warn, Info, Debug }

impl LogLevel {
    fn as_str(&self) -> &str {
        match self {
            LogLevel::Error => "ERROR",
            LogLevel::Warn => "WARN",
            LogLevel::Info => "INFO",
            LogLevel::Debug => "DEBUG",
        }
    }
}

let level = LogLevel::Warn;
println!("[{}]", level.as_str());
// [WARN]
```

```rust
// Match en un loop — procesar secuencia de acciones:

enum Action { Add(i32), Remove(i32), Clear }

let actions = vec![Action::Add(5), Action::Add(3), Action::Remove(5)];
let mut items: Vec<i32> = Vec::new();

for action in actions {
    match action {
        Action::Add(x) => items.push(x),
        Action::Remove(x) => { items.retain(|&i| i != x); },
        Action::Clear => items.clear(),
    }
}
println!("{:?}", items);
// [3]
```

---

## Ejercicios

### Ejercicio 1 — match basico con enum

```rust
// Definir un enum Coin con las variantes:
//   Penny, Nickel, Dime, Quarter
//
// Implementar una funcion value_in_cents(coin: Coin) -> u32
// que use match para retornar el valor de cada moneda:
//   Penny => 1, Nickel => 5, Dime => 10, Quarter => 25
//
// En main, crear un Vec<Coin> con varias monedas,
// iterar y acumular el total.
//
// Verificar:
//   [Penny, Quarter, Dime, Nickel, Quarter] => total 66
```

### Ejercicio 2 — destructuring y guards

```rust
// Definir un struct Point { x: f64, y: f64 }
//
// Escribir una funcion classify_point(p: &Point) -> &str que use
// match con destructuring y guards para clasificar:
//
//   - (0.0, 0.0) => "origin"
//   - x == 0 => "y-axis"
//   - y == 0 => "x-axis"
//   - x > 0 y y > 0 => "quadrant I"
//   - x < 0 y y > 0 => "quadrant II"
//   - x < 0 y y < 0 => "quadrant III"
//   - x > 0 y y < 0 => "quadrant IV"
//
// Probar con al menos un punto en cada cuadrante y en cada eje.
```

### Ejercicio 3 — match anidado con Option y Result

```rust
// Escribir una funcion safe_divide(a: f64, b: f64) -> Result<f64, String>
// que retorne Err si b es 0.
//
// Escribir una funcion process(input: Option<(f64, f64)>) -> String
// que use match para manejar:
//   - None => "no input"
//   - Some((a, b)) => llamar safe_divide y hacer match sobre el Result:
//       - Ok(val) => formatear el resultado con 2 decimales
//       - Err(msg) => retornar el mensaje de error
//
// Probar con:
//   process(Some((10.0, 3.0)))  => "3.33"
//   process(Some((10.0, 0.0)))  => "division by zero" (o similar)
//   process(None)               => "no input"
```
