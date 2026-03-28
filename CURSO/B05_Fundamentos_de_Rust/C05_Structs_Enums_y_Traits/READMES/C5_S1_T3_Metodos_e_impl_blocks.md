# T03 — Metodos e impl blocks

## Que es un impl block

Un `impl` block asocia funciones y metodos a un tipo. En Rust no
hay clases: la definicion de datos (struct/enum) y la definicion
de comportamiento (impl) estan separadas:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

impl Rectangle {
    // Todo lo que esta aqui dentro pertenece a Rectangle.
    // Puede ser:
    //   - metodos (reciben self en alguna forma)
    //   - funciones asociadas (no reciben self)
}

// La struct define los datos. El impl define el comportamiento.
// Se puede tener multiples impl blocks para el mismo tipo.
// No se puede tener impl para un tipo definido en otro crate.
```

## Metodos con &self

Un metodo recibe `&self` como primer parametro. Toma prestada la
instancia de forma inmutable: puede leer campos pero no modificarlos.
Es la forma mas comun:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

impl Rectangle {
    fn area(&self) -> f64 {
        self.width * self.height
    }

    fn perimeter(&self) -> f64 {
        2.0 * (self.width + self.height)
    }

    fn is_square(&self) -> bool {
        (self.width - self.height).abs() < f64::EPSILON
    }
}

fn main() {
    let rect = Rectangle { width: 3.0, height: 4.0 };

    println!("Area: {}", rect.area());           // 12.0
    println!("Perimeter: {}", rect.perimeter());  // 14.0
    println!("Square? {}", rect.is_square());     // false

    // rect sigue valido despues de cada llamada.
    // &self solo toma un prestamo inmutable.
    let a2 = rect.area();  // Se puede llamar otra vez.
}
```

```rust
// &self es azucar sintactica para self: &Self.
// Equivalentes: fn area(&self), fn area(self: &Self),
//               fn area(self: &Rectangle).
// &self es la forma idiomatica. Las otras se usan en
// casos avanzados (self: Arc<Self>, self: Box<Self>).
```

## Metodos con &mut self

Un metodo recibe `&mut self` cuando necesita modificar campos.
La instancia debe estar declarada con `let mut`:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

impl Rectangle {
    fn area(&self) -> f64 {
        self.width * self.height
    }

    fn scale(&mut self, factor: f64) {
        self.width *= factor;
        self.height *= factor;
    }

    fn set_width(&mut self, width: f64) {
        self.width = width;
    }
}

fn main() {
    let mut rect = Rectangle { width: 3.0, height: 4.0 };
    println!("Area: {}", rect.area());  // 12.0

    rect.scale(2.0);
    println!("Area: {}", rect.area());  // 48.0

    rect.set_width(10.0);
    println!("Area: {}", rect.area());  // 80.0
}
```

```rust
// Si la instancia no es mut, el compilador rechaza la llamada:
fn main() {
    let rect = Rectangle { width: 3.0, height: 4.0 };
    // rect.scale(2.0);
    // Error: cannot borrow `rect` as mutable, as it is not
    //        declared as mutable
}
```

## Metodos con self (por valor)

Un metodo recibe `self` (sin referencia) cuando toma ownership.
Despues de la llamada, la instancia ya no es valida:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

impl Rectangle {
    fn into_square(self) -> Rectangle {
        let side = self.width.max(self.height);
        Rectangle { width: side, height: side }
    }

    fn into_description(self) -> String {
        format!("Rectangle({}x{})", self.width, self.height)
    }
}

fn main() {
    let rect = Rectangle { width: 3.0, height: 4.0 };
    let desc = rect.into_description();
    println!("{}", desc);  // Rectangle(3x4)

    // rect ya no es valido. into_description() consumio el valor.
    // println!("{}", rect.width);
    // Error: borrow of moved value: `rect`
}
```

```rust
// Convencion de nombres para metodos que consumen self:
//   into_*  — transforma el tipo en otro
//   unwrap  — extrae el valor interior
//
// self por valor permite mover campos fuera de la struct
// (imposible con &self):
struct FileHandle { path: String, buffer: Vec<u8> }

impl FileHandle {
    fn into_buffer(self) -> Vec<u8> {
        self.buffer  // Mueve buffer fuera. Requiere ownership.
    }
}
```

## El tipo Self

`Self` (S mayuscula) es un alias del tipo que se implementa.
Dentro de `impl Rectangle`, `Self` equivale a `Rectangle`:

```rust
struct Point {
    x: f64,
    y: f64,
}

impl Point {
    fn origin() -> Self {
        Self { x: 0.0, y: 0.0 }  // Equivale a Point { x: 0.0, y: 0.0 }
    }

    fn new(x: f64, y: f64) -> Self {
        Self { x, y }
    }

    fn distance_to(&self, other: &Self) -> f64 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        (dx * dx + dy * dy).sqrt()
    }
}

fn main() {
    let origin = Point::origin();
    let p = Point::new(3.0, 4.0);
    println!("Distance: {}", origin.distance_to(&p));  // 5.0
}

// self (minuscula) = la instancia actual (el valor).
// Self (mayuscula) = el tipo actual.
// Ventaja de Self: si renombras la struct, el cuerpo del impl
// no necesita cambios.
```

## Funciones asociadas (sin self)

Una funcion dentro de un `impl` que **no** recibe `self` es una
funcion asociada. Se llama con `Tipo::funcion()`, no con punto:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

impl Rectangle {
    // Funciones asociadas (sin self): se llaman con ::
    fn new(width: f64, height: f64) -> Self {
        Self { width, height }
    }

    fn square(side: f64) -> Self {
        Self { width: side, height: side }
    }

    fn from_corners(x1: f64, y1: f64, x2: f64, y2: f64) -> Self {
        Self {
            width: (x2 - x1).abs(),
            height: (y2 - y1).abs(),
        }
    }

    // Metodo (con &self): se llama con .
    fn area(&self) -> f64 {
        self.width * self.height
    }
}

fn main() {
    let r1 = Rectangle::new(3.0, 4.0);       // :: sintaxis
    let r2 = Rectangle::square(5.0);          // :: sintaxis
    println!("{}", r1.area());                 // .  sintaxis (12.0)
    println!("{}", r2.area());                 // .  sintaxis (25.0)
}
```

```rust
// Convenciones para constructores:
//   new     — constructor principal
//   from_*  — construir desde otro tipo
//   with_*  — construir con configuracion opcional
//
// new no es palabra reservada. Es convencion de la comunidad.
// Ejemplos stdlib: String::from("hello"), Vec::with_capacity(100).
```

## Resolucion de metodos y auto-ref

Cuando escribes `rect.area()`, el compilador agrega las referencias
necesarias automaticamente. No necesitas escribir `(&rect).area()`:

```rust
fn main() {
    let mut rect = Rectangle { width: 3.0, height: 4.0 };

    // Equivalentes:
    let a1 = rect.area();
    let a2 = (&rect).area();       // Explicito, innecesario.

    // Equivalentes:
    rect.scale(2.0);
    (&mut rect).scale(2.0);        // Explicito, innecesario.
}

// El compilador prueba en orden:
//   1. T       (self por valor)
//   2. &T      (&self)
//   3. &mut T  (&mut self)
//   4. *T      (si T implementa Deref)
//   ... y asi sucesivamente (cadena de Deref).
//
// Esto permite llamar metodos de str directamente sobre String:
//   let s = String::from("hello");
//   s.len();  // String -> Deref -> str -> len()
//
// Elimina la ambiguedad . vs -> de C:
//   C:    rect.area  vs  rect->area
//   Rust: rect.area()    (siempre)
```

## Multiples impl blocks

Un tipo puede tener multiples `impl` blocks. El compilador
los fusiona:

```rust
struct Rectangle {
    width: f64,
    height: f64,
}

// Geometria
impl Rectangle {
    fn area(&self) -> f64 { self.width * self.height }
    fn perimeter(&self) -> f64 { 2.0 * (self.width + self.height) }
}

// Constructores
impl Rectangle {
    fn new(width: f64, height: f64) -> Self { Self { width, height } }
    fn square(side: f64) -> Self { Self { width: side, height: side } }
}

fn main() {
    let r = Rectangle::new(3.0, 4.0);
    println!("{}", r.area());  // 12.0
    // Metodos de ambos impl blocks estan disponibles.
}
```

```rust
// Razones para dividir en multiples impl blocks:
//
// 1. Traits (cada trait requiere su propio impl):
//    impl Display for Rectangle { ... }
//    impl Debug for Rectangle { ... }
//
// 2. Compilacion condicional:
//    #[cfg(feature = "serde")]
//    impl Rectangle {
//        fn to_json(&self) -> String { ... }
//    }
//
// 3. Organizacion por funcionalidad.
//
// 4. Generics con distintos bounds:
//    impl<T> Container<T> {
//        fn len(&self) -> usize { ... }      // Para todo T
//    }
//    impl<T: Display> Container<T> {
//        fn print(&self) { ... }             // Solo si T: Display
//    }
```

## Metodos en enums

Los `impl` blocks tambien funcionan con enums:

```rust
enum Shape {
    Circle(f64),
    Rectangle { width: f64, height: f64 },
}

impl Shape {
    fn area(&self) -> f64 {
        match self {
            Self::Circle(r) => std::f64::consts::PI * r * r,
            Self::Rectangle { width, height } => width * height,
        }
    }

    fn description(&self) -> String {
        match self {
            Self::Circle(r) => format!("Circle(r={})", r),
            Self::Rectangle { width, height } => {
                format!("Rect({}x{})", width, height)
            }
        }
    }

    // Funcion asociada:
    fn unit_circle() -> Self { Self::Circle(1.0) }
}

fn main() {
    let shapes = vec![
        Shape::Circle(5.0),
        Shape::Rectangle { width: 3.0, height: 4.0 },
        Shape::unit_circle(),
    ];
    for s in &shapes {
        println!("{}: area = {:.2}", s.description(), s.area());
    }
}
```

## Getters y encapsulacion

Los campos son privados por defecto fuera del modulo. Se usan
getters para exponer datos de forma controlada:

```rust
pub struct User {
    name: String,
    email: String,
    login_count: u64,
}

impl User {
    pub fn new(name: String, email: String) -> Self {
        Self { name, email, login_count: 0 }
    }

    pub fn name(&self) -> &str { &self.name }
    pub fn email(&self) -> &str { &self.email }
    pub fn login_count(&self) -> u64 { self.login_count }

    pub fn record_login(&mut self) {
        self.login_count += 1;
    }
}
```

```rust
// Convenciones para getters en Rust:
//
// Nombre del getter = nombre del campo (sin prefijo get_):
//   fn name(&self) -> &str     ← idiomatico
//   fn get_name(&self) -> &str ← no idiomatico
//
// Tipos Copy (numeros, bool): retornar por valor.
//   fn width(&self) -> f64 { self.width }
//
// Tipos String/Vec: retornar referencia.
//   fn name(&self) -> &str { &self.name }
//   fn items(&self) -> &[Item] { &self.items }
```

## Patron builder

El patron builder construye structs paso a paso cuando tienen
muchos campos opcionales:

```rust
struct Server {
    host: String,
    port: u16,
    max_connections: u32,
    timeout_secs: u64,
}

struct ServerBuilder {
    host: String,
    port: u16,
    max_connections: Option<u32>,
    timeout_secs: Option<u64>,
}

impl ServerBuilder {
    fn new(host: String, port: u16) -> Self {
        Self { host, port, max_connections: None, timeout_secs: None }
    }

    fn max_connections(mut self, n: u32) -> Self {
        self.max_connections = Some(n);
        self  // Retorna Self para encadenar.
    }

    fn timeout_secs(mut self, secs: u64) -> Self {
        self.timeout_secs = Some(secs);
        self
    }

    fn build(self) -> Server {
        Server {
            host: self.host,
            port: self.port,
            max_connections: self.max_connections.unwrap_or(100),
            timeout_secs: self.timeout_secs.unwrap_or(30),
        }
    }
}

fn main() {
    let server = ServerBuilder::new("localhost".to_string(), 8080)
        .max_connections(500)
        .timeout_secs(60)
        .build();

    println!("{}:{}, max={}", server.host, server.port,
        server.max_connections);
}

// Alternativa: setters con &mut self (retornan &mut Self).
// Mas eficiente pero requiere let mut en el builder.
```

## Ejemplo completo

Funciones asociadas, &self, &mut self y self por valor en una
sola struct:

```rust
struct BankAccount {
    holder: String,
    balance: f64,
}

impl BankAccount {
    fn new(holder: String, initial: f64) -> Self {
        Self { holder, balance: initial }
    }

    fn holder(&self) -> &str { &self.holder }
    fn balance(&self) -> f64 { self.balance }

    fn deposit(&mut self, amount: f64) {
        self.balance += amount;
    }

    fn withdraw(&mut self, amount: f64) -> Result<(), String> {
        if amount > self.balance {
            return Err("Insufficient funds".to_string());
        }
        self.balance -= amount;
        Ok(())
    }

    fn close(self) -> f64 {
        println!("Closing account for {}", self.holder);
        self.balance  // Consume la cuenta, retorna saldo.
    }
}

fn main() {
    let mut acct = BankAccount::new("Alice".to_string(), 1000.0);
    acct.deposit(500.0);
    acct.withdraw(200.0).unwrap();
    println!("{}: {:.2}", acct.holder(), acct.balance()); // Alice: 1300.00
    let remaining = acct.close();
    println!("Returned: {:.2}", remaining);               // 1300.00
    // acct ya no es valido.
}
```

---

## Ejercicios

### Ejercicio 1 --- Struct con metodos basicos

```rust
// Crear una struct Circle con un campo radius: f64.
// Implementar:
//   - fn new(radius: f64) -> Self
//   - fn area(&self) -> f64
//   - fn circumference(&self) -> f64
//   - fn scale(&mut self, factor: f64)
//   - fn into_diameter(self) -> f64  (consume el circulo)
//
// En main:
//   1. Crear un circulo con radio 5.0 usando Circle::new
//   2. Imprimir area y circunferencia
//   3. Escalar por factor 2.0, imprimir area otra vez
//   4. Consumir con into_diameter e imprimir el resultado
//   5. Verificar que usar el circulo despues da error de compilacion
```

### Ejercicio 2 --- Enum con metodos

```rust
// Crear un enum TrafficLight con variantes Red, Yellow, Green.
// Implementar:
//   - fn next(&self) -> Self
//     (Red -> Green, Green -> Yellow, Yellow -> Red)
//   - fn duration_secs(&self) -> u32
//     (Red: 60, Yellow: 5, Green: 45)
//   - fn is_stop(&self) -> bool
//     (true para Red y Yellow)
//   - fn from_name(s: &str) -> Option<Self>
//     (funcion asociada: "red" -> Some(Red), etc.)
//
// En main, simular 6 cambios de luz empezando desde Red,
// imprimiendo la variante y duracion en cada paso.
```

### Ejercicio 3 --- Builder pattern

```rust
// Crear una struct Email con campos:
//   from: String, to: String, subject: String,
//   body: String, cc: Vec<String>, priority: u8
//
// Crear un EmailBuilder que:
//   - Reciba from y to como obligatorios en new()
//   - Tenga setters encadenables para subject, body, cc, priority
//   - cc se acumula (cada llamada agrega un destinatario)
//   - build() retorne Email con defaults:
//     subject: "(no subject)", body: "", cc: [], priority: 3
//
// En main, construir dos emails:
//   1. Uno con todos los campos y dos CC
//   2. Uno solo con from/to (verificar defaults)
```
