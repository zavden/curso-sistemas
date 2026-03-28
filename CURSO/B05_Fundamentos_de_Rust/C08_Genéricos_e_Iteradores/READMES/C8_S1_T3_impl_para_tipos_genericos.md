# T03 — impl para tipos genericos

## Tres formas de implementar metodos

Cuando tienes un tipo generico, hay tres formas distintas de
agregar metodos, cada una con distinto alcance:

```rust
struct Wrapper<T> {
    inner: T,
}

// 1. impl<T> — metodos para CUALQUIER T
impl<T> Wrapper<T> {
    fn new(inner: T) -> Self {
        Wrapper { inner }
    }

    fn into_inner(self) -> T {
        self.inner
    }
}

// 2. impl Wrapper<i32> — metodos SOLO para i32
impl Wrapper<i32> {
    fn is_positive(&self) -> bool {
        self.inner > 0
    }
}

// 3. impl<T: Trait> — metodos para T que cumple un bound
impl<T: std::fmt::Display> Wrapper<T> {
    fn show(&self) {
        println!("Wrapper contiene: {}", self.inner);
    }
}
```

```rust
let w = Wrapper::new(42);
w.show();          // OK — i32 implementa Display
w.is_positive();   // OK — es Wrapper<i32>

let w = Wrapper::new("hello");
w.show();          // OK — &str implementa Display
// w.is_positive(); // error — no existe para Wrapper<&str>

let w = Wrapper::new(vec![1, 2, 3]);
// w.show();        // error — Vec<i32> no implementa Display (solo Debug)
w.into_inner();    // OK — into_inner existe para cualquier T
```

## impl\<T\> Foo\<T\> — metodos genericos

El `<T>` despues de `impl` declara el parametro de tipo para
todo el bloque. Es necesario para que Rust sepa que `T` en
`Foo<T>` es un parametro, no un tipo concreto llamado `T`:

```rust
struct Stack<T> {
    elements: Vec<T>,
}

// <T> despues de impl introduce T como parametro generico
impl<T> Stack<T> {
    fn new() -> Self {
        Stack { elements: Vec::new() }
    }

    fn push(&mut self, item: T) {
        self.elements.push(item);
    }

    fn pop(&mut self) -> Option<T> {
        self.elements.pop()
    }

    fn peek(&self) -> Option<&T> {
        self.elements.last()
    }

    fn is_empty(&self) -> bool {
        self.elements.is_empty()
    }

    fn len(&self) -> usize {
        self.elements.len()
    }
}

// Estos metodos existen para Stack<i32>, Stack<String>,
// Stack<Vec<bool>>, Stack<lo_que_sea>
let mut s = Stack::new();
s.push(1);
s.push(2);
assert_eq!(s.pop(), Some(2));
```

```text
¿Por que repetir <T>?

  impl<T> Stack<T> { ... }
       ^        ^
       |        └── "estoy implementando para Stack con parametro T"
       └── "T es un parametro generico que declaro aqui"

Si escribieras:
  impl Stack<T> { ... }
  → error: T no esta definido. Rust busca un tipo concreto llamado "T".

  impl Stack<i32> { ... }
  → OK: implementacion concreta solo para Stack<i32>.
```

## impl Foo\<ConcreteType\> — especializacion

Puedes implementar metodos que solo existen para un tipo concreto
especifico:

```rust
struct Matrix<T> {
    data: Vec<Vec<T>>,
    rows: usize,
    cols: usize,
}

// Para cualquier T:
impl<T> Matrix<T> {
    fn rows(&self) -> usize { self.rows }
    fn cols(&self) -> usize { self.cols }
}

// Solo para f64 — operaciones numericas:
impl Matrix<f64> {
    fn determinant(&self) -> f64 {
        // Solo tiene sentido para numeros
        todo!()
    }

    fn transpose(&self) -> Matrix<f64> {
        todo!()
    }
}

// Solo para String — operaciones de texto:
impl Matrix<String> {
    fn to_csv(&self) -> String {
        self.data.iter()
            .map(|row| row.join(","))
            .collect::<Vec<_>>()
            .join("\n")
    }
}
```

```rust
// Patron comun: metodos de conversion para tipos especificos
struct Id<T> {
    value: T,
}

impl<T> Id<T> {
    fn new(value: T) -> Self {
        Id { value }
    }
}

impl Id<String> {
    fn as_str(&self) -> &str {
        &self.value
    }
}

impl Id<u64> {
    fn is_zero(&self) -> bool {
        self.value == 0
    }

    fn next(&self) -> Id<u64> {
        Id { value: self.value + 1 }
    }
}
```

## impl con bounds condicionales

Los bounds en `impl<T: Trait>` hacen que los metodos solo existan
cuando T satisface las condiciones:

```rust
struct Container<T> {
    items: Vec<T>,
}

// Para cualquier T:
impl<T> Container<T> {
    fn new() -> Self {
        Container { items: Vec::new() }
    }

    fn push(&mut self, item: T) {
        self.items.push(item);
    }

    fn len(&self) -> usize {
        self.items.len()
    }
}

// Solo si T implementa Display:
impl<T: std::fmt::Display> Container<T> {
    fn print_all(&self) {
        for item in &self.items {
            println!("{item}");
        }
    }
}

// Solo si T implementa PartialOrd:
impl<T: PartialOrd> Container<T> {
    fn max(&self) -> Option<&T> {
        self.items.iter().max_by(|a, b| a.partial_cmp(b).unwrap())
    }
}

// Solo si T implementa Clone + Default:
impl<T: Clone + Default> Container<T> {
    fn resize(&mut self, new_len: usize) {
        self.items.resize(new_len, T::default());
    }
}
```

```rust
let mut c = Container::new();
c.push(3);
c.push(1);
c.push(4);

c.print_all();      // OK — i32: Display ✓
c.max();             // OK — i32: PartialOrd ✓
c.resize(5);         // OK — i32: Clone + Default ✓

// Con un tipo que no implementa Display:
struct Opaque;
let mut c = Container::new();
c.push(Opaque);
c.len();            // OK — no requiere bounds
// c.print_all();   // error: Opaque no implementa Display
// c.max();         // error: Opaque no implementa PartialOrd
```

```text
Este patron es MUY comun en la stdlib. Por ejemplo, Vec<T>:
  - push/pop/len existen para cualquier T
  - sort() solo existe si T: Ord
  - dedup() solo existe si T: PartialEq
  - contains(&val) solo existe si T: PartialEq
```

## Multiples bloques impl

Un tipo puede tener **cualquier cantidad** de bloques impl,
cada uno con bounds distintos:

```rust
struct Pair<T> {
    first: T,
    second: T,
}

// Bloque 1: sin bounds
impl<T> Pair<T> {
    fn new(first: T, second: T) -> Self {
        Pair { first, second }
    }
}

// Bloque 2: con Display
impl<T: std::fmt::Display> Pair<T> {
    fn display(&self) {
        println!("({}, {})", self.first, self.second);
    }
}

// Bloque 3: con PartialOrd + Display
impl<T: PartialOrd + std::fmt::Display> Pair<T> {
    fn larger(&self) -> &T {
        if self.first >= self.second {
            &self.first
        } else {
            &self.second
        }
    }
}

// Bloque 4: solo para f64
impl Pair<f64> {
    fn average(&self) -> f64 {
        (self.first + self.second) / 2.0
    }
}
```

## Blanket implementations

Una blanket implementation implementa un trait para **todos los
tipos que satisfacen ciertos bounds**. Es una de las herramientas
mas poderosas del sistema de tipos de Rust:

```rust
// La stdlib tiene esta blanket impl:
// impl<T: Display> ToString for T {
//     fn to_string(&self) -> String {
//         format!("{self}")
//     }
// }

// Esto significa: cualquier tipo que implemente Display
// automaticamente tiene .to_string()

let s = 42.to_string();        // "42" — i32: Display ✓
let s = 3.14.to_string();     // "3.14" — f64: Display ✓
let s = true.to_string();     // "true" — bool: Display ✓

// Tu struct custom tambien:
struct Color { r: u8, g: u8, b: u8 }

impl std::fmt::Display for Color {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "#{:02X}{:02X}{:02X}", self.r, self.g, self.b)
    }
}

let c = Color { r: 255, g: 128, b: 0 };
let s = c.to_string();  // "#FF8000" — gratis por la blanket impl
```

### Blanket impls en la stdlib

```rust
// 1. Display → ToString (ya visto arriba)
// impl<T: Display> ToString for T

// 2. From → Into
// impl<T, U> Into<U> for T where U: From<T>
// Si implementas From<A> for B, automaticamente tienes A.into() → B

struct Meters(f64);

impl From<f64> for Meters {
    fn from(val: f64) -> Self {
        Meters(val)
    }
}

let m: Meters = 5.0.into();           // Into via blanket impl
let m: Meters = Meters::from(5.0);    // From directo

// 3. Borrow y AsRef
// impl<T> Borrow<T> for T { ... }
// impl<T> AsRef<T> for T { ... }
// Cualquier T puede prestarse como si mismo

// 4. Iterator → muchos adaptadores
// Si implementas Iterator, obtienes gratis: map, filter,
// enumerate, zip, take, skip, collect, sum, count, etc.
// Todos son blanket impls sobre el trait Iterator.
```

### Escribir tus propias blanket impls

```rust
trait Describable {
    fn describe(&self) -> String;
}

// Blanket impl: cualquier tipo que sea Debug + Display es Describable
impl<T: std::fmt::Debug + std::fmt::Display> Describable for T {
    fn describe(&self) -> String {
        format!("display: {self}, debug: {self:?}")
    }
}

// Ahora todos los tipos con Debug + Display tienen describe():
println!("{}", 42.describe());
// "display: 42, debug: 42"

println!("{}", "hello".describe());
// "display: hello, debug: \"hello\""
```

```rust
// Blanket impl para referencias:
trait Summary {
    fn summary(&self) -> String;
}

struct Article { title: String }

impl Summary for Article {
    fn summary(&self) -> String {
        self.title.clone()
    }
}

// Blanket: si T implementa Summary, &T tambien:
// (Esto ya lo hace la stdlib para muchos traits, pero es un ejemplo)
// impl<T: Summary> Summary for &T {
//     fn summary(&self) -> String {
//         (**self).summary()
//     }
// }
```

```text
Reglas de blanket impls:
- Solo puedes escribir blanket impls para traits que TU definiste
  (regla del orfano / coherence)
- No puedes implementar un trait externo para "todos los T"
- Las blanket impls tienen prioridad baja — una impl concreta
  siempre gana sobre una blanket impl
```

## impl Trait para tipos genericos (trait impls)

Ademas de metodos inherentes, puedes implementar traits para
tipos genericos:

```rust
use std::fmt;

struct Pair<T> {
    first: T,
    second: T,
}

// Display para Pair<T> cuando T: Display
impl<T: fmt::Display> fmt::Display for Pair<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "({}, {})", self.first, self.second)
    }
}

// Clone para Pair<T> cuando T: Clone
impl<T: Clone> Clone for Pair<T> {
    fn clone(&self) -> Self {
        Pair {
            first: self.first.clone(),
            second: self.second.clone(),
        }
    }
}

// O mas facil con derive:
#[derive(Debug, Clone, PartialEq)]
struct Pair2<T> {
    first: T,
    second: T,
}
// derive genera automaticamente:
// impl<T: Debug> Debug for Pair2<T>
// impl<T: Clone> Clone for Pair2<T>
// impl<T: PartialEq> PartialEq for Pair2<T>
```

## Ejemplo integrador

```rust
use std::fmt;

/// Coleccion que mantiene un historial de los ultimos N elementos.
struct RingBuffer<T> {
    data: Vec<Option<T>>,
    head: usize,
    len: usize,
}

// Para cualquier T:
impl<T> RingBuffer<T> {
    fn new(capacity: usize) -> Self {
        let mut data = Vec::with_capacity(capacity);
        for _ in 0..capacity {
            data.push(None);
        }
        RingBuffer { data, head: 0, len: 0 }
    }

    fn push(&mut self, item: T) {
        let cap = self.data.len();
        self.data[self.head] = Some(item);
        self.head = (self.head + 1) % cap;
        if self.len < cap {
            self.len += 1;
        }
    }

    fn len(&self) -> usize {
        self.len
    }

    fn capacity(&self) -> usize {
        self.data.len()
    }
}

// Solo si T: Clone — poder obtener los elementos como Vec:
impl<T: Clone> RingBuffer<T> {
    fn to_vec(&self) -> Vec<T> {
        let cap = self.data.len();
        let start = if self.len < cap {
            0
        } else {
            self.head
        };
        let mut result = Vec::with_capacity(self.len);
        for i in 0..self.len {
            let idx = (start + i) % cap;
            if let Some(ref item) = self.data[idx] {
                result.push(item.clone());
            }
        }
        result
    }
}

// Solo si T: Display — poder imprimir:
impl<T: fmt::Display> fmt::Display for RingBuffer<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "RingBuffer[")?;
        let cap = self.data.len();
        let start = if self.len < cap { 0 } else { self.head };
        for i in 0..self.len {
            if i > 0 { write!(f, ", ")?; }
            let idx = (start + i) % cap;
            if let Some(ref item) = self.data[idx] {
                write!(f, "{item}")?;
            }
        }
        write!(f, "]")
    }
}

// Solo para RingBuffer<f64> — estadisticas:
impl RingBuffer<f64> {
    fn average(&self) -> f64 {
        if self.len == 0 { return 0.0; }
        let sum: f64 = self.data.iter()
            .filter_map(|x| x.as_ref())
            .sum();
        sum / self.len as f64
    }
}

fn main() {
    let mut buf = RingBuffer::new(3);
    buf.push(1.0);
    buf.push(2.0);
    buf.push(3.0);
    println!("{buf}");           // RingBuffer[1, 2, 3]
    println!("avg: {}", buf.average());  // avg: 2.0

    buf.push(4.0);  // sobreescribe el 1.0
    println!("{buf}");           // RingBuffer[2, 3, 4]
    println!("{:?}", buf.to_vec()); // [2.0, 3.0, 4.0]
}
```

## Errores comunes

```rust
// ERROR 1: olvidar <T> en impl cuando T es parametro
struct Box<T> { val: T }
// impl Box<T> { }      // error: cannot find type T
impl<T> Box<T> { }      // correcto

// ERROR 2: bounds en el struct en vez del impl
// MAL — fuerza Display en todas partes:
struct Logger<T: std::fmt::Display> { value: T }
// Ahora no puedes crear Logger<Vec<i32>> porque Vec no es Display

// BIEN — bound solo donde se necesita:
struct Logger<T> { value: T }
impl<T: std::fmt::Display> Logger<T> {
    fn log(&self) { println!("{}", self.value); }
}

// ERROR 3: conflicto entre blanket impl e impl concreta
trait Greet {
    fn greet(&self) -> String;
}

impl<T: std::fmt::Display> Greet for T {
    fn greet(&self) -> String {
        format!("Hello, {self}!")
    }
}

// impl Greet for String {  // error: conflicting implementations
//     fn greet(&self) -> String { format!("Hi, {self}!") }
// }
// String implementa Display, asi que la blanket impl ya cubre String.

// ERROR 4: confundir metodos de impl<T> con impl concreto
struct Data<T> { val: T }

impl<T> Data<T> {
    fn get(&self) -> &T { &self.val }  // existe para todo T
}

impl Data<i32> {
    fn double(&self) -> i32 { self.val * 2 }  // solo i32
}

let d = Data { val: "hello" };
d.get();      // OK
// d.double(); // error: no method named `double` for Data<&str>
```

## Cheatsheet

```text
impl<T> Foo<T> { ... }
  → metodos para CUALQUIER T
  → el caso mas general

impl Foo<ConcreteType> { ... }
  → metodos solo para ese tipo concreto
  → especializacion

impl<T: Bound> Foo<T> { ... }
  → metodos solo para T que cumple el bound
  → disponibilidad condicional

Blanket impl:
  impl<T: TraitA> TraitB for T { ... }
  → implementa TraitB para todos los T que tienen TraitA
  → solo para traits propios (orphan rule)
  → stdlib: Display→ToString, From→Into, Iterator→adaptadores

Derive genera impl condicionales:
  #[derive(Clone)]
  struct Foo<T> { val: T }
  → impl<T: Clone> Clone for Foo<T>

Buena practica:
  - Bounds en impl, NO en struct
  - Multiples bloques impl con distintos bounds
  - impl concreto para metodos tipo-especificos
```

---

## Ejercicios

### Ejercicio 1 — Capas de impl

```rust
// Define struct NumericPair<T> { a: T, b: T }
//
// Implementa en bloques separados:
// 1. impl<T> — new(a, b), swap(self) -> Self
// 2. impl<T: PartialOrd> — min(&self) -> &T, max(&self) -> &T
// 3. impl<T: Display> — to_string(&self) -> String
// 4. impl NumericPair<f64> — average(&self) -> f64
// 5. impl NumericPair<i32> — sum(&self) -> i32
//
// Prueba cada bloque y verifica que los metodos
// solo existen donde los bounds lo permiten.
```

### Ejercicio 2 — Blanket impl

```rust
// Define un trait Printable con fn print(&self);
//
// Escribe una blanket impl: todo tipo que sea Display es Printable,
// y print() llama println!("{self}").
//
// Verifica que funciona con i32, String, &str.
// Luego intenta escribir una impl concreta de Printable para i32.
// Predice: ¿compila? ¿Por que?
```

### Ejercicio 3 — Cache generico

```rust
// Implementa struct Cache<K, V> que wrappea un HashMap<K, V>.
//
// Bloques impl:
// 1. impl<K: Hash + Eq, V> — new(), insert(k, v), get(&k) -> Option<&V>
// 2. impl<K: Hash + Eq, V: Clone> — get_or_insert(k, default: V) -> &V
// 3. impl<K: Hash + Eq + Display, V: Display> — dump() que imprime todo
// 4. impl Cache<String, i32> — sum_values() -> i32
//
// Prueba con Cache<String, i32> y Cache<u64, Vec<String>>.
// ¿Cuales metodos estan disponibles para cada uno?
```
