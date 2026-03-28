# T04 — Closures como parametros y retorno

## Closures como parametros: tres formas

Hay tres formas de aceptar una closure como parametro,
cada una con tradeoffs distintos:

```rust
// 1. Generico con trait bound (monomorphization)
fn apply_generic<F: Fn(i32) -> i32>(f: F, x: i32) -> i32 {
    f(x)
}

// 2. impl Trait (azucar para el generico)
fn apply_impl(f: impl Fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}

// 3. Trait object (dynamic dispatch)
fn apply_dyn(f: &dyn Fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}
```

```text
                    Generico <F: Fn>    impl Fn         &dyn Fn / Box<dyn Fn>
────────────────────────────────────────────────────────────────────────────
Dispatch            Estatico            Estatico        Dinamico
Inlining            Si                  Si              No
Tamaño binario      Crece por tipo      Crece por tipo  Una sola copia
Tipo concreto       Conocido            Conocido        Borrado (erased)
Almacenar en struct Si (con generico)   No              Si (con Box)
Multiples closures  Si (mismo tipo)     No              Si (tipos distintos)
Overhead runtime    Ninguno             Ninguno         Vtable indirection
```

## Generico con trait bound

La forma mas comun y eficiente. El compilador genera una
version especializada para cada closure:

```rust
fn map_vec<F: Fn(i32) -> i32>(v: &[i32], f: F) -> Vec<i32> {
    v.iter().map(|&x| f(x)).collect()
}

let doubled = map_vec(&[1, 2, 3], |x| x * 2);     // version 1
let incremented = map_vec(&[1, 2, 3], |x| x + 1);  // version 2
// Cada closure genera una version distinta de map_vec (monomorphization)
```

```rust
// Con where clause — mas legible con bounds complejos:
fn process<T, F, G>(items: Vec<T>, transform: F, filter: G) -> Vec<T>
where
    F: Fn(T) -> T,
    G: Fn(&T) -> bool,
{
    items.into_iter()
        .map(transform)
        .filter(filter)
        .collect()
}

// Elegir FnOnce, FnMut, o Fn segun necesidad:
fn call_once<F: FnOnce() -> String>(f: F) -> String {
    f()  // llama una sola vez
}

fn call_many<F: Fn() -> String>(f: F) -> String {
    format!("{} {}", f(), f())  // llama multiples veces
}

fn accumulate<F: FnMut(i32)>(mut f: F, items: &[i32]) {
    for &item in items {
        f(item);  // llama multiples veces, f puede mutar
    }
}
```

## impl Fn — azucar sintactico

`impl Fn` en posicion de parametro es exactamente equivalente
a un generico. Es azucar:

```rust
// Estas dos firmas son IDENTICAS:
fn foo(f: impl Fn(i32) -> i32) -> i32 { f(42) }
fn foo<F: Fn(i32) -> i32>(f: F) -> i32 { f(42) }

// impl Fn es mas conciso, pero el generico permite:
// 1. Nombrar el tipo (util si aparece multiples veces)
fn bar<F: Fn(i32) -> i32>(f: F) -> (i32, i32) {
    (f(1), f(2))  // F aparece una vez, pero se usa en la firma
}

// 2. Forzar que dos parametros sean la MISMA closure
fn same<F: Fn()>(a: F, b: F) { a(); b(); }
// Con impl: fn same(a: impl Fn(), b: impl Fn()) → pueden ser tipos distintos
```

```text
¿Cuando usar cual?

impl Fn   → una sola closure, conciso, la mayoria de los casos
<F: Fn>   → multiples apariciones del tipo, o necesitas turbofish
```

## &dyn Fn — trait objects

Cuando necesitas **borrar el tipo** de la closure (type erasure),
usas trait objects. Esto permite mezclar closures distintas:

```rust
// &dyn Fn — referencia a una closure (sin ownership)
fn apply(f: &dyn Fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}

let double = |x| x * 2;
let add_ten = |x| x + 10;

// Ambas closures pueden pasarse a la misma funcion
// con el mismo tipo de parametro:
println!("{}", apply(&double, 5));   // 10
println!("{}", apply(&add_ten, 5));  // 15

// Pueden estar en una coleccion:
let operations: Vec<&dyn Fn(i32) -> i32> = vec![&double, &add_ten];
for op in &operations {
    println!("{}", op(5));
}
```

```rust
// Box<dyn Fn> — closure en el heap (con ownership)
fn make_operations() -> Vec<Box<dyn Fn(i32) -> i32>> {
    vec![
        Box::new(|x| x * 2),
        Box::new(|x| x + 10),
        Box::new(|x| x * x),
    ]
}

let ops = make_operations();
for op in &ops {
    println!("{}", op(5));
}
// 10, 15, 25
```

```text
¿Cuando usar trait objects para closures?

✓ Colecciones de closures distintas (Vec<Box<dyn Fn>>)
✓ Closures almacenadas en structs con tipo erasure
✓ APIs que aceptan callbacks registrados dinamicamente
✓ Plugin systems, event handlers
✓ Reducir tamaño de binario (una sola version, no monomorphization)

✗ Performance critica (vtable overhead, no inlining)
✗ Casos simples donde un generico basta
```

## Boxing closures

`Box<dyn Fn>` es la forma mas comun de almacenar closures con
ownership cuando no puedes usar genericos:

```rust
// Almacenar closures en un struct:
struct EventHandler {
    on_click: Box<dyn Fn(i32, i32)>,
    on_hover: Box<dyn Fn(i32, i32)>,
}

impl EventHandler {
    fn new(
        on_click: impl Fn(i32, i32) + 'static,
        on_hover: impl Fn(i32, i32) + 'static,
    ) -> Self {
        EventHandler {
            on_click: Box::new(on_click),
            on_hover: Box::new(on_hover),
        }
    }

    fn click(&self, x: i32, y: i32) {
        (self.on_click)(x, y);
    }

    fn hover(&self, x: i32, y: i32) {
        (self.on_hover)(x, y);
    }
}

let handler = EventHandler::new(
    |x, y| println!("click at ({x}, {y})"),
    |x, y| println!("hover at ({x}, {y})"),
);
handler.click(10, 20);
handler.hover(30, 40);
```

```rust
// Mapa de callbacks:
use std::collections::HashMap;

struct EventBus {
    handlers: HashMap<String, Vec<Box<dyn Fn(&str)>>>,
}

impl EventBus {
    fn new() -> Self {
        EventBus { handlers: HashMap::new() }
    }

    fn on(&mut self, event: &str, handler: impl Fn(&str) + 'static) {
        self.handlers
            .entry(event.to_string())
            .or_default()
            .push(Box::new(handler));
    }

    fn emit(&self, event: &str, data: &str) {
        if let Some(handlers) = self.handlers.get(event) {
            for handler in handlers {
                handler(data);
            }
        }
    }
}

let mut bus = EventBus::new();
bus.on("login", |user| println!("Welcome, {user}!"));
bus.on("login", |user| println!("Logging access for {user}"));
bus.on("error", |msg| eprintln!("ERROR: {msg}"));

bus.emit("login", "Alice");
// Welcome, Alice!
// Logging access for Alice
bus.emit("error", "disk full");
// ERROR: disk full
```

## Retornar closures

Las closures no tienen un tipo nombrable — cada una tiene un
tipo anonimo unico. Para retornarlas hay dos opciones:

### impl Fn — retorno con tipo opaco

```rust
// impl Fn en posicion de retorno: el tipo concreto es opaco
fn make_adder(n: i32) -> impl Fn(i32) -> i32 {
    move |x| x + n
}

let add5 = make_adder(5);
assert_eq!(add5(10), 15);

// Funciona porque el compilador conoce el tipo concreto
// aunque el llamador no lo vea (tipo opaco).

// LIMITACION: solo puedes retornar UN tipo concreto.
fn make_op(multiply: bool) -> impl Fn(i32) -> i32 {
    if multiply {
        |x| x * 2
    } else {
        |x| x + 10
    }
    // error: `if` and `else` have incompatible types
    // Cada closure es un tipo distinto → no puede ser el mismo impl Fn
}
```

### Box\<dyn Fn\> — retorno con tipo borrado

```rust
// Box<dyn Fn> permite retornar closures de distintos tipos:
fn make_op(multiply: bool) -> Box<dyn Fn(i32) -> i32> {
    if multiply {
        Box::new(|x| x * 2)
    } else {
        Box::new(|x| x + 10)
    }
    // OK — ambas ramas retornan Box<dyn Fn>, tipo borrado
}

let op = make_op(true);
assert_eq!(op(5), 10);

let op = make_op(false);
assert_eq!(op(5), 15);
```

```text
¿Cuando usar cual?

impl Fn en retorno:
  ✓ Siempre retornas la MISMA closure (un solo tipo concreto)
  ✓ Sin overhead de heap allocation
  ✓ El compilador puede hacer inline

Box<dyn Fn> en retorno:
  ✓ Retornas closures DISTINTAS segun condiciones
  ✓ Almacenas en colecciones o structs
  ✗ Heap allocation (Box)
  ✗ Dynamic dispatch (vtable, sin inline)
```

## fn pointer vs Fn traits

```rust
// fn pointer: tipo simple, sin capturas
fn double(x: i32) -> i32 { x * 2 }

// fn pointer como parametro:
fn apply_fn_ptr(f: fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}

apply_fn_ptr(double, 5);     // OK — funcion nombrada
apply_fn_ptr(|x| x + 1, 5); // OK — closure sin capturas

let y = 10;
// apply_fn_ptr(|x| x + y, 5);  // error: closure captura y
// fn pointers NO pueden tener capturas.
```

```text
Jerarquia completa:

fn(i32) -> i32          Puntero a funcion. Sin capturas. Tamaño fijo.
                        Implementa Fn + FnMut + FnOnce + Copy.

impl Fn(i32) -> i32     Closure con tipo conocido. Monomorphization.
                        Puede capturar variables.

&dyn Fn(i32) -> i32     Referencia a closure. Dynamic dispatch.
                        Sin ownership. El llamador posee la closure.

Box<dyn Fn(i32) -> i32> Closure en el heap. Dynamic dispatch.
                        Con ownership. Almacenable en structs/colecciones.
```

## Closures y lifetimes

```rust
// Closure que toma referencia — lifetime se infiere:
fn find_first<'a>(items: &'a [i32], predicate: impl Fn(&i32) -> bool) -> Option<&'a i32> {
    items.iter().find(|x| predicate(x))
}

let nums = vec![1, 2, 3, 4, 5];
let first_even = find_first(&nums, |x| x % 2 == 0);
assert_eq!(first_even, Some(&2));
```

```rust
// Box<dyn Fn> y lifetimes — por defecto es 'static:
struct Processor {
    transform: Box<dyn Fn(i32) -> i32>,
    // equivalente a: Box<dyn Fn(i32) -> i32 + 'static>
    // La closure no puede capturar referencias no-'static
}

// Si necesitas capturar referencias con lifetime limitado:
struct ProcessorWithLifetime<'a> {
    transform: Box<dyn Fn(i32) -> i32 + 'a>,
    // Ahora puede capturar referencias con lifetime 'a
}

fn make_processor(offset: &i32) -> ProcessorWithLifetime<'_> {
    ProcessorWithLifetime {
        transform: Box::new(move |x| x + offset),
    }
}
```

## Ejemplo integrador

```rust
/// Builder de queries con closures encadenables.
struct QueryBuilder<T> {
    data: Vec<T>,
    filters: Vec<Box<dyn Fn(&T) -> bool>>,
    transform: Option<Box<dyn Fn(T) -> T>>,
    limit: Option<usize>,
}

impl<T: 'static> QueryBuilder<T> {
    fn new(data: Vec<T>) -> Self {
        QueryBuilder {
            data,
            filters: Vec::new(),
            transform: None,
            limit: None,
        }
    }

    fn filter(mut self, f: impl Fn(&T) -> bool + 'static) -> Self {
        self.filters.push(Box::new(f));
        self
    }

    fn map(mut self, f: impl Fn(T) -> T + 'static) -> Self {
        self.transform = Some(Box::new(f));
        self
    }

    fn limit(mut self, n: usize) -> Self {
        self.limit = Some(n);
        self
    }

    fn execute(self) -> Vec<T> {
        let mut result: Vec<T> = self.data.into_iter()
            .filter(|item| {
                self.filters.iter().all(|f| f(item))
            })
            .collect();

        if let Some(transform) = self.transform {
            result = result.into_iter().map(|x| transform(x)).collect();
        }

        if let Some(limit) = self.limit {
            result.truncate(limit);
        }

        result
    }
}

fn main() {
    let numbers: Vec<i32> = (1..=20).collect();

    let result = QueryBuilder::new(numbers)
        .filter(|&x| x % 2 == 0)      // pares
        .filter(|&x| x > 5)            // mayores que 5
        .map(|x| x * 10)               // multiplicar por 10
        .limit(3)                       // solo 3 resultados
        .execute();

    assert_eq!(result, vec![60, 80, 100]);
}
```

## Errores comunes

```rust
// ERROR 1: retornar closures distintas con impl Fn
fn make(flag: bool) -> impl Fn() {
    if flag {
        || println!("a")
    } else {
        || println!("b")
    }
    // error: incompatible types
}
// Solucion: Box<dyn Fn()>

// ERROR 2: olvidar 'static en Box<dyn Fn>
fn store(f: Box<dyn Fn()>) { }
fn caller() {
    let local = String::from("hi");
    // store(Box::new(|| println!("{local}")));
    // error: `local` does not live long enough
    // Box<dyn Fn()> implica 'static por defecto
}
// Solucion: move || ... (para que la closure sea dueña)

// ERROR 3: olvidar mut en parametro FnMut
fn run(f: impl FnMut()) {
    // f();  // error: f is not mutable
}
fn run(mut f: impl FnMut()) {
    f();  // OK
}

// ERROR 4: confundir fn pointer con Fn trait
fn takes_fn(f: fn() -> i32) -> i32 { f() }
let x = 5;
// takes_fn(|| x + 1);  // error: closure captures x
// Solucion: usar impl Fn() -> i32

// ERROR 5: llamar closure en parentesis dentro de struct
struct S { f: Box<dyn Fn() -> i32> }
let s = S { f: Box::new(|| 42) };
// let val = s.f();  // error: no method named `f`
let val = (s.f)();   // OK — parentesis para desambiguar
```

## Cheatsheet

```text
Parametros:
  f: impl Fn(A) -> B           azucar, estatico, simple
  f: F  where F: Fn(A) -> B    generico, estatico, nombrable
  f: &dyn Fn(A) -> B           referencia, dinamico
  f: Box<dyn Fn(A) -> B>       heap, dinamico, ownership
  f: fn(A) -> B                puntero, sin capturas

Retorno:
  -> impl Fn(A) -> B           tipo opaco (un solo tipo concreto)
  -> Box<dyn Fn(A) -> B>       tipo borrado (multiples tipos)

Almacenar en struct:
  field: F (generico)           struct Foo<F: Fn()> { f: F }
  field: Box<dyn Fn()>          sin generico, heap

Elegir bound:
  Una llamada                → FnOnce (mas general)
  Multiples + muta           → FnMut
  Multiples + inmutable      → Fn (mas restrictivo)

Elegir forma:
  Performance                → generico / impl Fn
  Coleccion heterogenea      → Vec<Box<dyn Fn>>
  Retornar multiples tipos   → Box<dyn Fn>
  Sin capturas               → fn pointer
```

---

## Ejercicios

### Ejercicio 1 — Las tres formas

```rust
// Escribe la MISMA funcion apply_twice en tres versiones:
// a) Con generico:   fn apply_twice_gen<F: Fn(i32) -> i32>(f: F, x: i32) -> i32
// b) Con impl:       fn apply_twice_impl(f: impl Fn(i32) -> i32, x: i32) -> i32
// c) Con dyn:        fn apply_twice_dyn(f: &dyn Fn(i32) -> i32, x: i32) -> i32
//
// Cada una aplica f dos veces: f(f(x))
//
// Prueba las tres con |x| x + 1 y x = 0. Resultado esperado: 2.
// ¿Cual puedes pasar a un Vec de closures distintas?
```

### Ejercicio 2 — Retornar closures

```rust
// Implementa fn make_operation(op: &str) -> Box<dyn Fn(f64, f64) -> f64>
// que retorne la operacion correspondiente:
//   "add" → suma
//   "sub" → resta
//   "mul" → multiplicacion
//   "div" → division (retorna f64::NAN si divisor es 0)
//   otro  → panic
//
// Prueba:
//   let add = make_operation("add");
//   assert_eq!(add(3.0, 4.0), 7.0);
//   let div = make_operation("div");
//   assert!(div(1.0, 0.0).is_nan());
```

### Ejercicio 3 — Middleware pattern

```rust
// Implementa un sistema de middleware donde cada middleware
// es una closure que transforma un String.
//
// struct Pipeline {
//     steps: Vec<Box<dyn Fn(String) -> String>>,
// }
//
// impl Pipeline:
//   fn new() -> Self
//   fn add(mut self, step: impl Fn(String) -> String + 'static) -> Self
//   fn execute(self, input: String) -> String
//
// Ejemplo:
//   Pipeline::new()
//       .add(|s| s.trim().to_string())
//       .add(|s| s.to_uppercase())
//       .add(|s| format!("[{s}]"))
//       .execute("  hello world  ".to_string())
//   // → "[HELLO WORLD]"
```
