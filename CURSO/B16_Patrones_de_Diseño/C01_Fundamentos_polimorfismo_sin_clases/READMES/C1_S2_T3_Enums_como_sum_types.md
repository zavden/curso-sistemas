# T03 — Enums como sum types

---

## 1. El tercer mecanismo de polimorfismo

En T01 viste `dyn Trait`: polimorfismo abierto, extensible en runtime.
En T02 viste generics: polimorfismo estatico, zero-cost. Ahora vemos
el tercer mecanismo: **enums**, que ofrecen polimorfismo cerrado con
exhaustividad garantizada por el compilador.

```
  dyn Trait:   Abierto — cualquiera puede agregar un tipo nuevo
               Sin saber todas las variantes
               Dispatch dinamico (vtable)

  Generics:    Abierto — cualquier tipo que cumpla el bound
               Un tipo a la vez
               Dispatch estatico (monomorphization)

  Enum:        Cerrado — las variantes se definen una vez
               El compilador conoce TODAS las opciones
               Match exhaustivo, sin dispatch
```

### 1.1 Equivalencia con C

En S01/T02 viste la tagged union de C:

```c
// C: tagged union
typedef enum { TYPE_INT, TYPE_DOUBLE, TYPE_STRING } ValType;
typedef struct {
    ValType type;
    union { int i; double d; char *s; };
} Value;
```

El enum de Rust es la version type-safe de esto:

```rust
// Rust: enum = tagged union + type safety + exhaustividad
enum Value {
    Int(i32),
    Double(f64),
    Str(String),
}
```

La diferencia: en C puedes leer `v.d` cuando `v.type == TYPE_INT`
(UB silencioso). En Rust, `match` obliga a manejar cada variante
correctamente.

---

## 2. Enums con datos

### 2.1 Cada variante puede tener datos distintos

```rust
enum Shape {
    Circle { radius: f64 },
    Rect { width: f64, height: f64 },
    Triangle { a: f64, b: f64, c: f64 },
    Point,  // sin datos
}
```

Cada variante es un tipo distinto de dato, pero todos son `Shape`.
El compilador sabe exactamente cuantas variantes hay y que datos
tiene cada una.

### 2.2 Layout en memoria

El enum ocupa espacio para la **variante mas grande** + un discriminante
(tag):

```
  Shape en memoria:

  ┌──────────┬────────────────────────────┐
  │ tag (8B) │ datos (hasta 24B)          │
  └──────────┴────────────────────────────┘

  Circle:    │ tag=0  │ radius          │ padding    │
  Rect:      │ tag=1  │ width │ height  │ padding    │
  Triangle:  │ tag=2  │ a     │ b       │ c          │
  Point:     │ tag=3  │ (sin datos)     │ padding    │

  sizeof(Shape) = tag + sizeof(mayor variante)
                = 8 + 24 = 32 bytes (con alignment)
```

Comparacion con C:

```c
// C: el programador gestiona el tag manualmente
struct Shape {
    enum { CIRCLE, RECT, TRIANGLE, POINT } tag;  // 4 bytes
    union {
        struct { double radius; } circle;              // 8 bytes
        struct { double width, height; } rect;         // 16 bytes
        struct { double a, b, c; } triangle;           // 24 bytes
    };
};
// sizeof = 4 (tag) + 24 (mayor union) + padding = 32 bytes
```

Misma idea, mismo layout. La diferencia: en C puedes leer
`s.circle.radius` cuando `s.tag == RECT`. En Rust, `match` lo impide.

---

## 3. Pattern matching: match exhaustivo

### 3.1 Basico

```rust
fn area(shape: &Shape) -> f64 {
    match shape {
        Shape::Circle { radius } => {
            std::f64::consts::PI * radius.powi(2)
        }
        Shape::Rect { width, height } => {
            width * height
        }
        Shape::Triangle { a, b, c } => {
            let s = (a + b + c) / 2.0;
            (s * (s - a) * (s - b) * (s - c)).sqrt()
        }
        Shape::Point => 0.0,
    }
}
```

Si agregas una variante nueva (ej. `Ellipse { a: f64, b: f64 }`) y no
actualizas el match, el compilador da error:

```
error[E0004]: non-exhaustive patterns: `Shape::Ellipse { .. }` not covered
```

Esto es **imposible** con `dyn Trait` o con tagged unions en C. Solo
los enums dan esta garantia.

### 3.2 Destructuring profundo

```rust
enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>),
}

fn eval(expr: &Expr) -> f64 {
    match expr {
        Expr::Num(n) => *n,
        Expr::Add(a, b) => eval(a) + eval(b),
        Expr::Mul(a, b) => eval(a) * eval(b),
        Expr::Neg(e) => -eval(e),
    }
}

// 2 + 3 * 4
let expr = Expr::Add(
    Box::new(Expr::Num(2.0)),
    Box::new(Expr::Mul(
        Box::new(Expr::Num(3.0)),
        Box::new(Expr::Num(4.0)),
    )),
);

println!("{}", eval(&expr));  // 14.0
```

Cada rama del match desempaca los datos de la variante y los liga a
variables. El compilador verifica que accedes a los datos correctos
para cada variante.

### 3.3 Guards y patterns compuestos

```rust
fn classify(shape: &Shape) -> &str {
    match shape {
        Shape::Circle { radius } if *radius > 100.0 => "huge circle",
        Shape::Circle { .. } => "circle",
        Shape::Rect { width, height } if width == height => "square",
        Shape::Rect { .. } => "rectangle",
        Shape::Triangle { a, b, c } if a == b && b == c => "equilateral",
        Shape::Triangle { .. } => "triangle",
        Shape::Point => "point",
    }
}
```

Los guards (`if condition`) refinan el match sin romper exhaustividad.

---

## 4. Implementar metodos en enums

A diferencia de `dyn Trait` (donde cada tipo implementa su propia
version), en un enum los metodos van en un solo `impl`:

```rust
impl Shape {
    fn area(&self) -> f64 {
        match self {
            Shape::Circle { radius } => {
                std::f64::consts::PI * radius.powi(2)
            }
            Shape::Rect { width, height } => width * height,
            Shape::Triangle { a, b, c } => {
                let s = (a + b + c) / 2.0;
                (s * (s - a) * (s - b) * (s - c)).sqrt()
            }
            Shape::Point => 0.0,
        }
    }

    fn name(&self) -> &str {
        match self {
            Shape::Circle { .. } => "Circle",
            Shape::Rect { .. } => "Rect",
            Shape::Triangle { .. } => "Triangle",
            Shape::Point => "Point",
        }
    }

    fn is_degenerate(&self) -> bool {
        self.area() == 0.0
    }
}
```

### 4.1 Enums implementan traits

```rust
impl std::fmt::Display for Shape {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Shape::Circle { radius } =>
                write!(f, "Circle(r={:.1})", radius),
            Shape::Rect { width, height } =>
                write!(f, "Rect({:.1}x{:.1})", width, height),
            Shape::Triangle { a, b, c } =>
                write!(f, "Triangle({:.1},{:.1},{:.1})", a, b, c),
            Shape::Point => write!(f, "Point"),
        }
    }
}

let s = Shape::Circle { radius: 5.0 };
println!("{}", s);  // Circle(r=5.0)
```

---

## 5. Enum vs dyn Trait vs generics

### 5.1 Tabla comparativa

| Aspecto | Enum | dyn Trait | Generics |
|---------|------|-----------|----------|
| Variantes conocidas | Si (cerrado) | No (abierto) | No (abierto) |
| Exhaustividad en match | Si | No | No aplica |
| Agregar variante | Recompila todo | Sin cambios | Sin cambios |
| Agregar operacion | Sin cambios | Recompila tipos | Depende |
| Heterogeneo en Vec | Si: `Vec<Shape>` | Si: `Vec<Box<dyn T>>` | No |
| Stack allocation | Si | Requiere Box para owned | Si |
| Dispatch | Match (branch) | Vtable (indirection) | Inline (zero-cost) |
| Tamano por valor | Mayor variante + tag | Solo datos | Solo datos |

### 5.2 El dilema de la extension

Existe una tension fundamental entre agregar variantes y agregar
operaciones. Se conoce como el **Expression Problem**:

```
                    Facil agregar        Facil agregar
                    variante nueva       operacion nueva
  ────────────      ──────────────       ────────────────
  Enum               NO (recompilar)      SI (nuevo match)
  dyn Trait           SI (nuevo tipo)      NO (cambiar trait = rompe todo)
```

- **Enum**: facil agregar un metodo nuevo (escribes otro `match`), pero
  agregar una variante obliga a actualizar todos los `match` existentes.
- **dyn Trait**: facil agregar un tipo nuevo (implementas el trait), pero
  agregar un metodo al trait rompe todas las implementaciones.

Elige enum cuando las variantes son estables (ej. tipos de token en un
parser). Elige `dyn Trait` cuando los tipos crecen (ej. plugins).

---

## 6. Patrones comunes con enums

### 6.1 Command / Message

```rust
enum Command {
    Quit,
    Echo(String),
    Move { x: i32, y: i32 },
    Color(u8, u8, u8),
}

fn execute(cmd: &Command) {
    match cmd {
        Command::Quit => std::process::exit(0),
        Command::Echo(msg) => println!("{}", msg),
        Command::Move { x, y } => println!("Moving to ({}, {})", x, y),
        Command::Color(r, g, b) => println!("Color: #{:02x}{:02x}{:02x}", r, g, b),
    }
}
```

### 6.2 AST (Abstract Syntax Tree)

```rust
enum Expr {
    Literal(f64),
    Var(String),
    BinOp { op: Op, left: Box<Expr>, right: Box<Expr> },
    UnaryOp { op: UnaryOp, expr: Box<Expr> },
    Call { name: String, args: Vec<Expr> },
}

enum Op { Add, Sub, Mul, Div }
enum UnaryOp { Neg, Not }
```

Los ASTs son el caso de uso ideal para enums: las variantes son fijas
(definidas por la gramatica) y las operaciones crecen (eval, print,
optimize, typecheck).

### 6.3 State machine

```rust
enum ConnectionState {
    Idle,
    Connecting { attempt: u32 },
    Connected { session_id: u64 },
    Error { message: String, retries: u32 },
}

impl ConnectionState {
    fn handle_event(self, event: Event) -> ConnectionState {
        match (self, event) {
            (ConnectionState::Idle, Event::Connect) => {
                ConnectionState::Connecting { attempt: 1 }
            }
            (ConnectionState::Connecting { attempt }, Event::Success(id)) => {
                ConnectionState::Connected { session_id: id }
            }
            (ConnectionState::Connecting { attempt }, Event::Failure(msg)) => {
                if attempt < 3 {
                    ConnectionState::Connecting { attempt: attempt + 1 }
                } else {
                    ConnectionState::Error { message: msg, retries: attempt }
                }
            }
            (ConnectionState::Connected { .. }, Event::Disconnect) => {
                ConnectionState::Idle
            }
            (state, _) => state,  // ignorar eventos no validos
        }
    }
}
```

El match sobre `(state, event)` es una **tabla de transiciones**
verificada por el compilador. Si agregas un estado nuevo, el compilador
te obliga a definir que pasa con cada evento.

Compara con la dispatch table de C (S01/T01):

```c
// C: tabla de handlers, indices manuales, sin verificacion
StateHandler handlers[STATE_COUNT] = { ... };
handlers[current](event);  // si olvidas un estado: UB
```

### 6.4 Error types

```rust
enum AppError {
    Io(std::io::Error),
    Parse(std::num::ParseIntError),
    NotFound { path: String },
    Custom(String),
}

impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            AppError::Io(e) => write!(f, "IO error: {}", e),
            AppError::Parse(e) => write!(f, "Parse error: {}", e),
            AppError::NotFound { path } => write!(f, "Not found: {}", path),
            AppError::Custom(msg) => write!(f, "{}", msg),
        }
    }
}
```

---

## 7. Enums recursivos y Box

Un enum no puede contenerse a si mismo directamente — sizeof seria
infinito:

```rust
// NO COMPILA: tamano infinito
// enum List {
//     Cons(i32, List),  // List contiene List contiene List...
//     Nil,
// }

// Box rompe la recursion: puntero de tamano fijo (8 bytes)
enum List {
    Cons(i32, Box<List>),
    Nil,
}
```

```
  Sin Box:                        Con Box:

  List                            List
  ┌──────────────────┐            ┌─────────┬──────────┐
  │ i32 │ List       │            │ i32 (4) │ Box (8)  │  = 16 bytes
  │     │ ┌────────┐ │            └─────────┴──────┬───┘
  │     │ │i32│List │ │                            │
  │     │ │   │ ...│ │                            ▼
  │     │ └────────┘ │            ┌─────────┬──────────┐
  └──────────────────┘            │ i32 (4) │ Box (8)  │  (heap)
  sizeof = infinito               └─────────┴──────┬───┘
                                                   │
                                                   ▼
                                              ┌─────┐
                                              │ Nil │  (heap)
                                              └─────┘
```

```rust
fn list_sum(list: &List) -> i32 {
    match list {
        List::Nil => 0,
        List::Cons(val, rest) => val + list_sum(rest),
    }
}

let list = List::Cons(1,
    Box::new(List::Cons(2,
        Box::new(List::Cons(3,
            Box::new(List::Nil))))));

println!("{}", list_sum(&list));  // 6
```

---

## 8. Niche optimization

Rust optimiza el layout de enums cuando es posible:

### 8.1 Option con punteros

```rust
// Option<Box<T>> ocupa 8 bytes, NO 16
// Porque Box<T> nunca es null → null representa None

use std::mem::size_of;

assert_eq!(size_of::<Box<i32>>(),         8);
assert_eq!(size_of::<Option<Box<i32>>>(),  8);  // mismo tamano
```

El compilador usa el valor imposible (null) como discriminante para
`None`. Esto se llama **niche optimization**.

### 8.2 Otros nichos

```rust
// NonZeroU32: nunca es 0 → None = 0
use std::num::NonZeroU32;
assert_eq!(size_of::<Option<NonZeroU32>>(), 4);  // == size_of::<u32>()

// bool: solo usa 0 y 1 → None puede ser 2
assert_eq!(size_of::<Option<bool>>(), 1);  // == size_of::<bool>()

// char: no todos los u32 son chars validos → nicho disponible
assert_eq!(size_of::<Option<char>>(), 4);  // == size_of::<char>()
```

En C, `Option<Box<T>>` seria un struct de puntero + tag = 16 bytes.
Rust lo reduce a 8 bytes sin overhead.

---

## 9. if let, while let, let else

### 9.1 if let: match de una sola variante

Cuando solo te interesa una variante:

```rust
let shape = Shape::Circle { radius: 5.0 };

// match completo (verboso para una sola variante)
match &shape {
    Shape::Circle { radius } => println!("r = {}", radius),
    _ => {}
}

// if let (conciso)
if let Shape::Circle { radius } = &shape {
    println!("r = {}", radius);
}
```

### 9.2 while let: loop sobre variantes

```rust
fn drain_list(mut list: List) {
    while let List::Cons(val, rest) = list {
        println!("{}", val);
        list = *rest;
    }
}
```

### 9.3 let else: early return si no match

```rust
fn process_circle(shape: &Shape) -> f64 {
    let Shape::Circle { radius } = shape else {
        println!("Not a circle, skipping");
        return 0.0;
    };
    std::f64::consts::PI * radius.powi(2)
}
```

`let else` es util para validar una variante al inicio de una funcion
y salir temprano si no coincide.

---

## 10. Cuando enum, cuando dyn, cuando generics

```
  ¿Conoces TODAS las variantes?
    │
    ├─ SI  → ¿Cambian frecuentemente?
    │        │
    │        ├─ NO  → Enum  (exhaustividad, stack, sin indirection)
    │        │        Ej: tokens, AST, errores, comandos, estados
    │        │
    │        └─ SI  → Enum posible, pero cada cambio recompila
    │                 Considera dyn Trait si el cambio es frecuente
    │
    └─ NO  → ¿El usuario/plugin puede agregar tipos?
             │
             ├─ SI  → dyn Trait  (abierto, extensible)
             │        Ej: plugins, callbacks, middleware
             │
             └─ NO  → Generics  (compile time, zero-cost)
                      Ej: contenedores, algoritmos, utilidades
```

### Resumen en una frase

- **Enum**: "se exactamente que opciones hay y quiero que el compilador
  me fuerce a manejar todas"
- **dyn Trait**: "no se cuantos tipos habra y quiero que cualquiera
  pueda agregar uno nuevo"
- **Generics**: "funciona con cualquier tipo que cumpla las
  restricciones, y quiero zero-cost"

---

## Errores comunes

### Error 1 — Match no exhaustivo

```rust
enum Dir { North, South, East, West }

fn to_str(d: &Dir) -> &str {
    match d {
        Dir::North => "N",
        Dir::South => "S",
        // ERROR: East y West no cubiertos
    }
}
```

El compilador lista las variantes faltantes. Usa `_ => ...` como
catch-all solo si realmente quieres ignorar las demas.

### Error 2 — Wildcard prematuro

```rust
fn describe(d: &Dir) -> &str {
    match d {
        Dir::North => "north",
        _ => "other",  // catch-all
    }
}

// Si agregas Dir::NorthEast, el wildcard lo absorbe silenciosamente
// No hay error de compilacion — el bug se esconde
```

Prefiere listar todas las variantes explicitamente. Usa `_` solo cuando
realmente no importa.

### Error 3 — Enum recursivo sin Box

```rust
enum Tree {
    Leaf(i32),
    Node(Tree, Tree),  // ERROR: tamano infinito
}
// error[E0072]: recursive type has infinite size
```

Fix: `Node(Box<Tree>, Box<Tree>)`.

### Error 4 — Mover valor en match sin referencia

```rust
let shape = Shape::Circle { radius: 5.0 };

match shape {  // shape se MUEVE al match
    Shape::Circle { radius } => println!("{}", radius),
    _ => {}
}

// println!("{:?}", shape);  // ERROR: value used after move
```

Fix: `match &shape` para hacer match por referencia.

---

## Ejercicios

### Ejercicio 1 — Calculadora con enum

Define un enum `Expr` con variantes `Num(f64)`, `Add(Box<Expr>,
Box<Expr>)`, `Sub(Box<Expr>, Box<Expr>)`, `Mul(Box<Expr>, Box<Expr>)`.
Implementa `eval(&self) -> f64` y `Display`.

Construye y evalua la expresion `(3 + 4) * (2 - 1)`.

**Prediccion**: Cuantas allocations (Box::new) necesitas para esa
expresion?

<details><summary>Respuesta</summary>

```rust
use std::fmt;

enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),
    Sub(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
}

impl Expr {
    fn eval(&self) -> f64 {
        match self {
            Expr::Num(n) => *n,
            Expr::Add(a, b) => a.eval() + b.eval(),
            Expr::Sub(a, b) => a.eval() - b.eval(),
            Expr::Mul(a, b) => a.eval() * b.eval(),
        }
    }
}

impl fmt::Display for Expr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Expr::Num(n) => write!(f, "{}", n),
            Expr::Add(a, b) => write!(f, "({} + {})", a, b),
            Expr::Sub(a, b) => write!(f, "({} - {})", a, b),
            Expr::Mul(a, b) => write!(f, "({} * {})", a, b),
        }
    }
}

fn main() {
    // (3 + 4) * (2 - 1)
    let expr = Expr::Mul(
        Box::new(Expr::Add(
            Box::new(Expr::Num(3.0)),
            Box::new(Expr::Num(4.0)),
        )),
        Box::new(Expr::Sub(
            Box::new(Expr::Num(2.0)),
            Box::new(Expr::Num(1.0)),
        )),
    );

    println!("{} = {}", expr, expr.eval());
    // ((3 + 4) * (2 - 1)) = 7
}
```

Allocations: **6 Box::new**.
- 4 para los Num (hojas)
- 1 para el Add
- 1 para el Sub
- El Mul raiz vive en stack (no necesita Box)

En C, este arbol se construiria con 6 `malloc` equivalentes. La
diferencia: en Rust, `drop` libera todo automaticamente al salir de
scope. En C, necesitas un `tree_destroy` recursivo.

</details>

---

### Ejercicio 2 — State machine con enum

Implementa un semaforo (traffic light) como state machine con enum:

```rust
enum Light { Red, Yellow, Green }
```

Implementa `next(&self) -> Light` que cicle entre estados, y
`duration(&self) -> u32` que retorne los segundos de cada estado.

**Prediccion**: Si agregas `FlashingRed` como variante, en cuantos
lugares del codigo te obliga el compilador a agregar un caso?

<details><summary>Respuesta</summary>

```rust
enum Light {
    Red,
    Yellow,
    Green,
}

impl Light {
    fn next(&self) -> Light {
        match self {
            Light::Red => Light::Green,
            Light::Green => Light::Yellow,
            Light::Yellow => Light::Red,
        }
    }

    fn duration(&self) -> u32 {
        match self {
            Light::Red => 30,
            Light::Green => 25,
            Light::Yellow => 5,
        }
    }

    fn label(&self) -> &str {
        match self {
            Light::Red => "STOP",
            Light::Green => "GO",
            Light::Yellow => "CAUTION",
        }
    }
}

fn main() {
    let mut light = Light::Red;
    for _ in 0..6 {
        println!("{}: {}s", light.label(), light.duration());
        light = light.next();
    }
}
```

Salida:

```
STOP: 30s
GO: 25s
CAUTION: 5s
STOP: 30s
GO: 25s
CAUTION: 5s
```

Si agregas `FlashingRed`, el compilador te obliga a agregar un caso en
**3 match blocks**: `next`, `duration`, y `label`. No puedes compilar
hasta que manejes todos. Con dyn Trait o con C, olvidar uno seria un
bug silencioso.

</details>

---

### Ejercicio 3 — JSON simplificado

Define un enum `Json` que represente valores JSON:

```
null, bool, number, string, array, object
```

Implementa `Display` para serializar a JSON valido.

**Prediccion**: Cuales variantes necesitan `Box` o `Vec`? Por que
`String` no necesita Box?

<details><summary>Respuesta</summary>

```rust
use std::collections::HashMap;
use std::fmt;

enum Json {
    Null,
    Bool(bool),
    Number(f64),
    Str(String),
    Array(Vec<Json>),
    Object(HashMap<String, Json>),
}

impl fmt::Display for Json {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Json::Null => write!(f, "null"),
            Json::Bool(b) => write!(f, "{}", b),
            Json::Number(n) => write!(f, "{}", n),
            Json::Str(s) => write!(f, "\"{}\"", s),
            Json::Array(arr) => {
                write!(f, "[")?;
                for (i, item) in arr.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "{}", item)?;
                }
                write!(f, "]")
            }
            Json::Object(map) => {
                write!(f, "{{")?;
                for (i, (key, val)) in map.iter().enumerate() {
                    if i > 0 { write!(f, ", ")?; }
                    write!(f, "\"{}\": {}", key, val)?;
                }
                write!(f, "}}")
            }
        }
    }
}

fn main() {
    let data = Json::Object({
        let mut m = HashMap::new();
        m.insert("name".into(), Json::Str("Ana".into()));
        m.insert("age".into(), Json::Number(30.0));
        m.insert("active".into(), Json::Bool(true));
        m.insert("scores".into(), Json::Array(vec![
            Json::Number(95.0),
            Json::Number(87.0),
        ]));
        m
    });

    println!("{}", data);
    // {"name": "Ana", "age": 30, "active": true, "scores": [95, 87]}
}
```

`Vec<Json>` y `HashMap<String, Json>` ya almacenan datos en heap
internamente — no necesitan `Box` extra. `String` tampoco necesita
`Box` porque String ya es un puntero a heap (3 words: ptr, len, cap).

Solo necesitas `Box` cuando una variante contiene el **mismo enum
directamente** (recursion). `Vec<Json>` no es recursion directa — Vec
es un indirection a traves del heap.

</details>

---

### Ejercicio 4 — Enum vs dyn Trait: mismo problema

Implementa un sistema de formas geometricas de **dos formas**:

A) Con enum + match
B) Con trait + dyn

Para ambos: `area()`, `name()`, y una funcion `total_area` sobre un
slice.

**Prediccion**: Ahora agrega un metodo `perimeter()`. En cual version
es mas facil?. Luego agrega un tipo `Ellipse`. En cual es mas facil?

<details><summary>Respuesta</summary>

**Version A: Enum**

```rust
enum Shape {
    Circle(f64),
    Rect(f64, f64),
}

impl Shape {
    fn area(&self) -> f64 {
        match self {
            Shape::Circle(r) => std::f64::consts::PI * r.powi(2),
            Shape::Rect(w, h) => w * h,
        }
    }
    fn name(&self) -> &str {
        match self {
            Shape::Circle(_) => "Circle",
            Shape::Rect(_, _) => "Rect",
        }
    }
}

fn total_area_enum(shapes: &[Shape]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

**Version B: dyn Trait**

```rust
trait ShapeTrait {
    fn area(&self) -> f64;
    fn name(&self) -> &str;
}

struct CircleT(f64);
struct RectT(f64, f64);

impl ShapeTrait for CircleT {
    fn area(&self) -> f64 { std::f64::consts::PI * self.0.powi(2) }
    fn name(&self) -> &str { "Circle" }
}

impl ShapeTrait for RectT {
    fn area(&self) -> f64 { self.0 * self.1 }
    fn name(&self) -> &str { "Rect" }
}

fn total_area_dyn(shapes: &[Box<dyn ShapeTrait>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

**Agregar `perimeter()` — enum es mas facil:**

Enum: agregar un metodo nuevo con un match. No cambias nada existente.

dyn Trait: agregar al trait rompe todas las implementaciones — cada
struct debe implementar el nuevo metodo.

**Agregar `Ellipse` — dyn Trait es mas facil:**

dyn Trait: crear un struct nuevo + impl. Ningun codigo existente cambia.

Enum: agregar variante + actualizar TODOS los match existentes (area,
name, y cualquier otro metodo).

Este es el Expression Problem en accion.

</details>

---

### Ejercicio 5 — Niche optimization

Sin ejecutar, predice el tamano de cada tipo:

```rust
use std::mem::size_of;

enum A { X, Y, Z }

enum B { One(u32), Two(u32) }

enum C { Some(Box<i32>), None }

enum D { Val(bool) }

enum E {}  // zero variantes
```

| Tipo | `size_of` | Razon |
|------|-----------|-------|
| `A` | ? | |
| `B` | ? | |
| `C` | ? | |
| `Option<A>` | ? | |
| `Option<C>` | ? | |
| `D` | ? | |

<details><summary>Respuesta</summary>

| Tipo | `size_of` | Razon |
|------|-----------|-------|
| `A` | 1 | 3 variantes sin datos — cabe en 1 byte |
| `B` | 8 | tag (4B) + u32 (4B) — ambas variantes misma forma |
| `C` | 8 | Niche: Box nunca es null → None = null, sin tag extra |
| `Option<A>` | 1 | A usa valores 0,1,2 → None puede ser 3, cabe en 1 byte |
| `Option<C>` | 8 | C ya optimiza con niche; Option<C> es como Option<Box> |
| `D` | 1 | Una variante con bool — tag no necesario |

Notas:
- `A` sin datos: solo necesita distinguir 3 valores → 1 byte basta
- `B` con datos: necesita tag + espacio para u32
- `C` con Box: niche optimization — sizeof == sizeof(Box)
- `E` (zero variantes): es el tipo `!` (never). `size_of::<E>()` = 0,
  pero no puedes crear un valor de tipo E

</details>

---

### Ejercicio 6 — Traducir tagged union de C

Traduce esta tagged union de C (de S01/T02) a un enum de Rust:

```c
typedef enum { TYPE_INT, TYPE_DOUBLE, TYPE_STRING } ValType;
typedef struct {
    ValType type;
    union {
        int    as_int;
        double as_double;
        char  *as_string;
    };
} Value;

void print_value(const Value *v) {
    switch (v->type) {
        case TYPE_INT:    printf("%d\n", v->as_int); break;
        case TYPE_DOUBLE: printf("%.2f\n", v->as_double); break;
        case TYPE_STRING: printf("%s\n", v->as_string); break;
    }
}
```

**Prediccion**: Que errores posibles en C desaparecen en Rust?

<details><summary>Respuesta</summary>

```rust
enum Value {
    Int(i32),
    Double(f64),
    Str(String),
}

impl std::fmt::Display for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Value::Int(n) => write!(f, "{}", n),
            Value::Double(d) => write!(f, "{:.2}", d),
            Value::Str(s) => write!(f, "{}", s),
        }
    }
}

fn main() {
    let values = vec![
        Value::Int(42),
        Value::Double(3.14),
        Value::Str("hello".into()),
    ];

    for v in &values {
        println!("{}", v);
    }
}
```

Errores de C que desaparecen:

1. **Leer el campo incorrecto**: en C puedes leer `v.as_double` cuando
   `v.type == TYPE_INT`. En Rust, `match` solo te da acceso al campo
   de la variante que coincidio.

2. **Olvidar un case en switch**: en C, el compilador puede dar warning
   con `-Wswitch`, pero no es error. En Rust, es error de compilacion.

3. **Olvidar break en switch**: en C, sin `break` se ejecuta el
   siguiente case (fall-through). En Rust, no existe fall-through.

4. **Memory management del string**: en C, `char *as_string` es un
   puntero manual — debes saber quien lo libera. En Rust, `String` se
   libera automaticamente con el enum.

5. **Valor del tag inconsistente**: en C, puedes setear
   `v.type = TYPE_INT; v.as_string = "hello"` — tag y dato
   inconsistentes. En Rust, es imposible.

</details>

---

### Ejercicio 7 — Enum con metodos constructores

Crea un enum `Color` con variantes `Rgb(u8, u8, u8)`, `Hsl(f64, f64,
f64)`, y `Named(&'static str)`. Implementa metodos constructores
idiomaticos y un metodo `to_css(&self) -> String`.

**Prediccion**: Los constructores `Color::red()`, `Color::blue()` —
son funciones asociadas o metodos? Necesitan `&self`?

<details><summary>Respuesta</summary>

```rust
enum Color {
    Rgb(u8, u8, u8),
    Hsl(f64, f64, f64),
    Named(&'static str),
}

impl Color {
    // Funciones asociadas (no metodos — sin self)
    fn red() -> Self { Color::Rgb(255, 0, 0) }
    fn green() -> Self { Color::Rgb(0, 255, 0) }
    fn blue() -> Self { Color::Rgb(0, 0, 255) }
    fn black() -> Self { Color::Named("black") }
    fn white() -> Self { Color::Named("white") }

    fn from_hex(hex: u32) -> Self {
        Color::Rgb(
            ((hex >> 16) & 0xFF) as u8,
            ((hex >> 8) & 0xFF) as u8,
            (hex & 0xFF) as u8,
        )
    }

    // Metodo (con &self)
    fn to_css(&self) -> String {
        match self {
            Color::Rgb(r, g, b) => format!("rgb({}, {}, {})", r, g, b),
            Color::Hsl(h, s, l) => format!("hsl({}, {}%, {}%)", h, s, l),
            Color::Named(name) => name.to_string(),
        }
    }

    fn is_grayscale(&self) -> bool {
        match self {
            Color::Rgb(r, g, b) => r == g && g == b,
            Color::Named(n) => *n == "black" || *n == "white" || *n == "gray",
            _ => false,
        }
    }
}

fn main() {
    let colors = vec![
        Color::red(),
        Color::from_hex(0xFF8800),
        Color::Hsl(200.0, 80.0, 50.0),
        Color::black(),
    ];

    for c in &colors {
        println!("{}", c.to_css());
    }
    // rgb(255, 0, 0)
    // rgb(255, 136, 0)
    // hsl(200, 80%, 50%)
    // black
}
```

`Color::red()` es una **funcion asociada** (no metodo). No recibe
`&self` — es el equivalente de un static method en C++/Java o una
funcion asociada sin receiver en Rust. Se llama con `Color::red()`,
no con `c.red()`.

</details>

---

### Ejercicio 8 — Option y Result como enums

`Option<T>` y `Result<T, E>` son enums de la stdlib. Sin mirar la
documentacion, escribe su definicion y los metodos mas usados.

**Prediccion**: `Option<T>` tiene cuantas variantes? `Result<T, E>`?

<details><summary>Respuesta</summary>

```rust
// La definicion real en la stdlib:
enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

`Option`: 2 variantes (Some, None).
`Result`: 2 variantes (Ok, Err).

Metodos principales (simplificados):

```rust
impl<T> Option<T> {
    fn is_some(&self) -> bool;
    fn is_none(&self) -> bool;
    fn unwrap(self) -> T;              // panic si None
    fn unwrap_or(self, default: T) -> T;
    fn map<U>(self, f: impl FnOnce(T) -> U) -> Option<U>;
    fn and_then<U>(self, f: impl FnOnce(T) -> Option<U>) -> Option<U>;
}

impl<T, E> Result<T, E> {
    fn is_ok(&self) -> bool;
    fn is_err(&self) -> bool;
    fn unwrap(self) -> T;              // panic si Err
    fn map<U>(self, f: impl FnOnce(T) -> U) -> Result<U, E>;
    fn map_err<F>(self, f: impl FnOnce(E) -> F) -> Result<T, F>;
}
```

Estos son los enums mas usados en Rust. Son la alternativa type-safe a:
- `Option`: puntero NULL en C (`NULL` = None, valor valido = Some)
- `Result`: error codes en C (retorno < 0 = error, >= 0 = exito)

La ventaja: el compilador te obliga a manejar el caso de error/None.
En C, puedes ignorar el return code y el compilador no dice nada.

</details>

---

### Ejercicio 9 — Encontrar errores

Cada fragmento tiene un error. Identificalo:

```rust
// Error A
enum List {
    Cons(i32, List),
    Nil,
}

// Error B
enum Token { Number(f64), Plus, Minus }

fn eval(t: &Token) -> f64 {
    match t {
        Token::Number(n) => *n,
        Token::Plus => 0.0,
    }
}

// Error C
let val = Some(42);
let x = val.unwrap();
let y = val.unwrap();  // ???
```

**Prediccion**: Cual es error de tamano, cual es exhaustividad, y cual
es ownership?

<details><summary>Respuesta</summary>

**Error A: Tamano infinito (recursion sin Box).**

`Cons(i32, List)` contiene un `List` directo — sizeof es infinito.

```
error[E0072]: recursive type `List` has infinite size
```

Fix: `Cons(i32, Box<List>)`.

**Error B: Match no exhaustivo.**

`Token::Minus` no esta cubierto.

```
error[E0004]: non-exhaustive patterns: `Token::Minus` not covered
```

Fix: agregar `Token::Minus => 0.0` o `_ => 0.0`.

**Error C: Depende del tipo — NO es error.**

`Some(42)` es `Option<i32>`. `i32` implementa `Copy`, asi que
`val.unwrap()` no mueve `val` — lo copia. Ambas lineas funcionan.

Si fuera `Some(String::from("hello"))`, **si seria error**: `String`
no es `Copy`, y el primer `unwrap()` mueve el valor fuera del `Option`.
El segundo `unwrap()` daria:

```
error[E0382]: use of moved value: `val`
```

La distincion Copy vs Move es clave para predecir estos errores.

</details>

---

### Ejercicio 10 — Reflexion: enums como herramienta de diseno

Responde sin ver la respuesta:

1. Los enums de Rust no existen en C de forma type-safe. Las tagged
   unions de C son lo mas cercano. Cual es la diferencia mas importante
   para la correctitud del programa?

2. Un parser de un lenguaje de programacion define tokens como enum.
   Por que es mejor enum que dyn Trait aqui?

3. Si un enum tiene 100 variantes y 20 metodos (cada uno con match de
   100 brazos), es esto un code smell? Que alternativa propones?

<details><summary>Respuesta</summary>

**1. Diferencia mas importante:**

**Exhaustividad verificada por el compilador.** En C, agregar un valor
nuevo al `enum` de tag no genera ningun error — todos los `switch`
existentes lo ignoran silenciosamente (o caen en `default`). En Rust,
agregar una variante al enum genera error de compilacion en cada
`match` que no la cubre.

Esto transforma bugs silenciosos en runtime (C) en errores de
compilacion (Rust). El costo es la recompilacion; el beneficio es
la garantia de que nada se te escapo.

**2. Tokens como enum:**

Un parser tiene un conjunto **fijo y conocido** de tokens (definido por
la gramatica). No hay "tokens nuevos" en runtime. Ademas:

- Necesitas match exhaustivo para cada etapa (lexer, parser,
  evaluator). Un token no manejado es un bug del parser.
- Los tokens viven en stack (sin allocation). Con dyn Trait, cada token
  seria una Box — millones de allocations para un archivo grande.
- Los tokens se copian y comparan frecuentemente. Un enum de tokens es
  Copy (si los datos lo permiten). Un `dyn Trait` no es Copy.

**3. 100 variantes × 20 metodos:**

Si, es un code smell. 2000 brazos de match son inmantenibles. Opciones:

- **Agrupar variantes** en sub-enums:
  ```rust
  enum Expr { Literal(LiteralExpr), Binary(BinaryExpr), ... }
  enum LiteralExpr { Int(i64), Float(f64), Str(String), Bool(bool) }
  ```
  Cada sub-enum tiene sus propios metodos, reduciendo la explosion
  combinatoria.

- **Visitor pattern**: una funcion por variante, agrupadas en un trait.
  El match se escribe una vez (en `accept`) y las operaciones se
  implementan en visitors separados.

- **Reconsiderar la abstraccion**: si hay 100 variantes, quizas
  algunas comparten suficiente estructura para ser un trait + tipos
  concretos en vez de un enum monolitico.

La regla: si cada variante tiene comportamiento muy distinto, dyn Trait
puede ser mejor. Si comparten estructura y el match tiene patrones
repetitivos, sub-enums o visitors ayudan.

</details>
