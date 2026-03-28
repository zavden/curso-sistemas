# T01 — Box\<T\>

## Que es Box\<T\>

`Box<T>` es el smart pointer mas simple de Rust. Almacena un valor
de tipo `T` en el **heap** en lugar del stack, y cuando el `Box`
sale de scope, libera automaticamente la memoria del heap:

```rust
// En el stack:
let x: i32 = 42;         // 4 bytes en el stack

// En el heap:
let b: Box<i32> = Box::new(42);  // 8 bytes en el stack (puntero)
                                  // + 4 bytes en el heap (el i32)

println!("{b}");  // 42 — Box implementa Deref, se comporta como &T
```

```text
Stack vs Heap con Box:

  let x = 42;
  let b = Box::new(42);

  Stack:                     Heap:
  ┌──────────────────┐
  │ x: 42            │
  ├──────────────────┤       ┌──────┐
  │ b: ptr ──────────│──────▶│  42  │
  └──────────────────┘       └──────┘

  Cuando b sale de scope:
  1. Drop libera la memoria del heap (el 42)
  2. El puntero en el stack se libera normalmente
```

```rust
// Box implementa Deref — puedes usarlo como una referencia:
let b = Box::new(String::from("hello"));
println!("length: {}", b.len());  // deref automatico a &String
let s: &str = &b;                 // deref coercion: &Box<String> → &String → &str

// Box implementa DerefMut — tambien acceso mutable:
let mut b = Box::new(vec![1, 2, 3]);
b.push(4);  // deref mutable automatico a &mut Vec<i32>
assert_eq!(*b, vec![1, 2, 3, 4]);
```

---

## Cuando usar Box\<T\>

Box no es necesario para la mayoria de los casos — Rust almacena
en el stack por defecto, y eso es mas rapido. Hay 3 razones
principales para usar Box:

### Razon 1 — Tipos de tamano desconocido en compilacion

```rust
// Los trait objects no tienen tamano conocido (son ?Sized):
// let x: dyn std::fmt::Display = 42;  // error: doesn't have a size known at compile-time

// Box les da un tamano fijo (el tamano del puntero):
let x: Box<dyn std::fmt::Display> = Box::new(42);
let y: Box<dyn std::fmt::Display> = Box::new("hello");
println!("{x}, {y}");  // 42, hello

// Esto habilita colecciones heterogeneas:
let items: Vec<Box<dyn std::fmt::Display>> = vec![
    Box::new(42),
    Box::new("hello"),
    Box::new(3.14),
];
for item in &items {
    println!("{item}");
}
```

### Razon 2 — Tipos recursivos

```rust
// Este enum no compila — tamano infinito:
// enum List {
//     Cons(i32, List),  // error: recursive type has infinite size
//     Nil,
// }
//
// Cons contiene un List, que contiene otro List, que contiene otro...
// El compilador no puede calcular el tamano.

// Box rompe la recursion — tamano conocido (1 puntero):
enum List {
    Cons(i32, Box<List>),  // Box<List> tiene tamano fijo: 8 bytes (puntero)
    Nil,
}

// Ahora el compilador sabe el tamano:
// Cons = i32 (4) + padding (4) + Box<List> (8) = 16 bytes
// Nil = 0 bytes de datos
// List = max(Cons, Nil) + discriminante = 16 + algun padding

use List::{Cons, Nil};
let list = Cons(1, Box::new(Cons(2, Box::new(Cons(3, Box::new(Nil))))));
```

### Razon 3 — Transferir ownership de datos grandes sin copiar

```rust
// Mover datos grandes en el stack implica copiar bytes:
let big_array = [0u8; 1_000_000];  // 1 MB en el stack
let moved = big_array;              // copia 1 MB de stack a stack
// (el compilador puede optimizar esto, pero no siempre)

// Box mueve solo 8 bytes (el puntero):
let big_box = Box::new([0u8; 1_000_000]);  // 1 MB en el heap
let moved = big_box;                        // mueve solo 8 bytes (el puntero)
// Los datos en el heap no se copian
```

---

## Tipos recursivos en detalle

### Lista enlazada

```rust
#[derive(Debug)]
enum List<T> {
    Cons(T, Box<List<T>>),
    Nil,
}

impl<T> List<T> {
    fn new() -> Self {
        List::Nil
    }

    fn push(self, val: T) -> Self {
        List::Cons(val, Box::new(self))
    }

    fn len(&self) -> usize {
        match self {
            List::Nil => 0,
            List::Cons(_, rest) => 1 + rest.len(),
        }
    }
}

impl<T: std::fmt::Display> std::fmt::Display for List<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            List::Nil => write!(f, "Nil"),
            List::Cons(val, rest) => write!(f, "{val} → {rest}"),
        }
    }
}

fn main() {
    let list = List::new().push(3).push(2).push(1);
    println!("{list}");   // 1 → 2 → 3 → Nil
    println!("len: {}", list.len());  // 3
}
```

```text
Layout en memoria de List::Cons(1, Box::new(Cons(2, Box::new(Nil)))):

Stack:
┌────────────────────────┐
│ list: Cons              │
│   val: 1                │
│   next: ptr ─────────┐  │
└──────────────────────│──┘
                       ▼
Heap:              ┌────────────────────┐
                   │ Cons                │
                   │   val: 2            │
                   │   next: ptr ─────┐  │
                   └──────────────────│──┘
                                     ▼
                   ┌─────────────────┐
                   │ Nil              │
                   └─────────────────┘
```

### Arbol binario

```rust
#[derive(Debug)]
struct Tree<T> {
    value: T,
    left: Option<Box<Tree<T>>>,
    right: Option<Box<Tree<T>>>,
}

impl<T> Tree<T> {
    fn leaf(value: T) -> Self {
        Tree { value, left: None, right: None }
    }

    fn node(value: T, left: Tree<T>, right: Tree<T>) -> Self {
        Tree {
            value,
            left: Some(Box::new(left)),
            right: Some(Box::new(right)),
        }
    }
}

impl<T: std::fmt::Display> Tree<T> {
    fn depth(&self) -> usize {
        let left_depth = self.left.as_ref().map_or(0, |l| l.depth());
        let right_depth = self.right.as_ref().map_or(0, |r| r.depth());
        1 + left_depth.max(right_depth)
    }

    fn print_inorder(&self) {
        if let Some(left) = &self.left {
            left.print_inorder();
        }
        print!("{} ", self.value);
        if let Some(right) = &self.right {
            right.print_inorder();
        }
    }
}

fn main() {
    //       4
    //      / \
    //     2   6
    //    / \ / \
    //   1  3 5  7
    let tree = Tree::node(
        4,
        Tree::node(2, Tree::leaf(1), Tree::leaf(3)),
        Tree::node(6, Tree::leaf(5), Tree::leaf(7)),
    );

    tree.print_inorder();  // 1 2 3 4 5 6 7
    println!();
    println!("depth: {}", tree.depth());  // 3
}
```

```text
¿Por que Option<Box<Tree<T>>> en lugar de Box<Option<Tree<T>>>?

  Option<Box<Tree<T>>>:
  - None = sin hijo (0 bytes extras gracias a niche optimization)
  - Some(Box<Tree<T>>) = tiene hijo en el heap
  - El Box solo existe cuando hay un hijo

  Box<Option<Tree<T>>>:
  - Siempre aloca en el heap, incluso para None
  - Desperdicia memoria

  Regla: Option va FUERA de Box, no dentro.
```

### Expresiones aritmeticas (AST)

```rust
// Un arbol de sintaxis abstracta (AST) para expresiones:
#[derive(Debug)]
enum Expr {
    Num(f64),
    Add(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>),
}

impl Expr {
    fn eval(&self) -> f64 {
        match self {
            Expr::Num(n) => *n,
            Expr::Add(a, b) => a.eval() + b.eval(),
            Expr::Mul(a, b) => a.eval() * b.eval(),
            Expr::Neg(e) => -e.eval(),
        }
    }
}

// Helpers para construir expresiones sin escribir Box::new:
fn num(n: f64) -> Expr { Expr::Num(n) }
fn add(a: Expr, b: Expr) -> Expr { Expr::Add(Box::new(a), Box::new(b)) }
fn mul(a: Expr, b: Expr) -> Expr { Expr::Mul(Box::new(a), Box::new(b)) }
fn neg(e: Expr) -> Expr { Expr::Neg(Box::new(e)) }

fn main() {
    // (2 + 3) * -(4)
    let expr = mul(add(num(2.0), num(3.0)), neg(num(4.0)));
    println!("{}", expr.eval());  // -20.0
}
```

---

## Box y ownership

Box tiene las mismas reglas de ownership que cualquier otro tipo
en Rust — un solo dueño, se mueve al asignar:

```rust
let b = Box::new(42);
let c = b;        // b se mueve a c
// println!("{b}");  // error: value borrowed after move

// Drop es automatico:
{
    let b = Box::new(String::from("hello"));
    // b vive aqui
}  // b sale de scope → Drop libera el String en el heap

// Box::into_inner — extraer el valor del heap (Rust 1.XX+):
let b = Box::new(42);
let val: i32 = *b;  // desreferenciar copia el valor (i32 es Copy)
assert_eq!(val, 42);

// Para tipos non-Copy, desreferenciar mueve:
let b = Box::new(String::from("hello"));
let s: String = *b;  // mueve el String fuera del Box
// b ya no es valido
assert_eq!(s, "hello");
```

```rust
// Box como parametro de funcion:
fn process(data: Box<Vec<i32>>) {
    println!("processing {} items", data.len());
}  // data se libera aqui

let v = Box::new(vec![1, 2, 3]);
process(v);
// v ya no es valido — fue movido a process

// Generalmente es mejor recibir &Vec<i32> o &[i32] que Box<Vec<i32>>,
// a menos que la funcion necesite tomar ownership del heap allocation.
```

---

## Box::leak — escapar del lifetime

```rust
// Box::leak convierte Box<T> en &'static mut T.
// La memoria nunca se libera — se "escapa" del sistema de ownership:

let s: &'static str = Box::leak(Box::new(String::from("forever")));
// s vive para siempre — la memoria del String nunca se libera

// Uso legitimo: datos que viven toda la vida del programa
// (configuracion global, lazy_static, etc.)

// CUIDADO: cada llamada a Box::leak es un memory leak intencional.
// Solo usalo cuando realmente necesitas 'static lifetime.
```

---

## Box vs referencias vs otros smart pointers

```text
¿Cuando usar que?

  &T / &mut T:
  - No tienes ownership, solo prestas
  - El dato vive en otro lugar (stack o heap de otro owner)
  - Es la opcion por defecto

  Box<T>:
  - Ownership exclusivo de un dato en el heap
  - Un solo dueño, como T pero en el heap
  - Necesario para tipos recursivos y trait objects

  Rc<T>:
  - Multiples dueños (shared ownership)
  - Single-thread
  - (siguiente topico)

  Arc<T>:
  - Multiples dueños, multi-thread
  - (siguiente topico)

Regla de decision:
  1. ¿Necesitas ownership? Si no → &T
  2. ¿El tipo cabe en el stack? Si → T directamente
  3. ¿Tipo recursivo o trait object? → Box<T>
  4. ¿Necesitas compartir ownership? → Rc<T> o Arc<T>
  5. En cualquier otro caso → T en el stack
```

---

## Errores comunes

```rust
// ERROR 1: usar Box innecesariamente
// MAL — Box no aporta nada aqui:
fn sum(numbers: Box<Vec<i32>>) -> i32 {
    numbers.iter().sum()
}

// BIEN — recibir referencia o slice:
fn sum(numbers: &[i32]) -> i32 {
    numbers.iter().sum()
}

// Box solo es necesario cuando necesitas ownership en el heap.
// Si solo lees los datos, usa &T.
```

```rust
// ERROR 2: olvidar Box en tipos recursivos
// enum Tree { Node(i32, Tree, Tree), Leaf }
// error[E0072]: recursive type `Tree` has infinite size
//
// Solucion: envolver la recursion en Box:
enum Tree { Node(i32, Box<Tree>, Box<Tree>), Leaf }
```

```rust
// ERROR 3: Box<Option<T>> en vez de Option<Box<T>>
struct Node {
    value: i32,
    // MAL: siempre aloca en el heap, incluso para None:
    // next: Box<Option<Node>>,

    // BIEN: solo aloca si hay un nodo:
    next: Option<Box<Node>>,
    // Bonus: Option<Box<T>> tiene niche optimization →
    // None ocupa 0 bytes extra (usa el null pointer como discriminante)
}
```

```rust
// ERROR 4: confundir Box::new con allocation en el stack
let v = vec![0u8; 10_000_000];     // 10 MB en el STACK, luego se mueve
let b = Box::new(vec![0u8; 10_000_000]);

// En ambos casos, Vec almacena sus datos en el heap internamente.
// Box<Vec<T>> pone el STRUCT Vec (ptr + len + cap = 24 bytes) en el heap.
// Los datos del Vec ya estan en el heap con o sin Box.
//
// Box<Vec<T>> rara vez tiene sentido — Vec ya es un "smart pointer" al heap.
// Usa Vec<T> directamente.
//
// Donde Box SI importa: Box<[u8; 10_000_000]> — el ARRAY va al heap.
// Sin Box, el array de 10 MB estaria en el stack (stack overflow).
```

```rust
// ERROR 5: intentar clonar Box sin que T sea Clone
let b = Box::new(vec![1, 2, 3]);
let c = b.clone();  // OK — Vec<i32> implementa Clone

struct NoCopy { data: String }
let b = Box::new(NoCopy { data: "hi".into() });
// let c = b.clone();  // error: NoCopy doesn't implement Clone

// Solucion: derivar Clone en NoCopy:
#[derive(Clone)]
struct WithClone { data: String }
let b = Box::new(WithClone { data: "hi".into() });
let c = b.clone();  // OK
```

---

## Cheatsheet

```text
Crear:
  Box::new(value)          aloca value en el heap
  Box::new([0u8; 1024])    array grande en el heap (evita stack overflow)

Acceder:
  *b                       desreferenciar (mover o copiar el valor)
  &*b  o  &b               obtener &T (via Deref)
  b.method()               deref automatico llama T::method()

Mover:
  let c = b;               mueve el Box (solo copia el puntero)
  *b                       mueve el valor FUERA del Box (non-Copy)

Convertir:
  Box::into_raw(b)         → *mut T (raw pointer, tu manejas la memoria)
  unsafe { Box::from_raw(ptr) }  → Box<T> (retoma ownership)
  Box::leak(b)             → &'static mut T (nunca se libera)

Usos principales:
  1. Tipos recursivos:     enum List { Cons(T, Box<List>), Nil }
  2. Trait objects:         Box<dyn Trait>
  3. Datos grandes:        Box<[u8; 1_000_000]>
  4. Transferir ownership: fn consume(data: Box<Config>) { ... }

Layout en memoria:
  Box<T> en el stack = 1 puntero (8 bytes en 64-bit)
  Datos de T en el heap = size_of::<T>() bytes

Drop:
  Automatico al salir de scope.
  Primero drop(T), luego libera la memoria del heap.

Niche optimization:
  Option<Box<T>> tiene el mismo tamano que Box<T>
  (None usa el null pointer como discriminante)
```

---

## Ejercicios

### Ejercicio 1 — Lista enlazada

```rust
// Implementa una lista enlazada con las siguientes operaciones:
//
// enum List<T> { Cons(T, Box<List<T>>), Nil }
//
// a) fn from_vec(v: Vec<T>) -> List<T>
//    Convierte un Vec en una List. El primer elemento del Vec
//    debe ser el primer elemento de la lista.
//    Pista: itera el Vec en reversa y usa push.
//
// b) fn to_vec(&self) -> Vec<&T>
//    Convierte la lista en un Vec de referencias.
//
// c) fn map<U, F: Fn(&T) -> U>(&self, f: F) -> List<U>
//    Crea una nueva lista aplicando f a cada elemento.
//
// d) Verifica:
//    let list = List::from_vec(vec![1, 2, 3]);
//    let doubled = list.map(|x| x * 2);
//    assert_eq!(doubled.to_vec(), vec![&2, &4, &6]);
```

### Ejercicio 2 — Arbol binario de busqueda

```rust
// Implementa un BST (Binary Search Tree) basico:
//
// struct BST<T: Ord> {
//     root: Option<Box<Node<T>>>,
// }
// struct Node<T> {
//     value: T,
//     left: Option<Box<Node<T>>>,
//     right: Option<Box<Node<T>>>,
// }
//
// a) fn insert(&mut self, value: T)
//    Inserta un valor en la posicion correcta.
//
// b) fn contains(&self, value: &T) -> bool
//    Retorna true si el valor existe en el arbol.
//
// c) fn inorder(&self) -> Vec<&T>
//    Retorna los elementos en orden ascendente.
//
// d) Verifica:
//    let mut bst = BST::new();
//    for x in [5, 3, 7, 1, 4, 6, 8] { bst.insert(x); }
//    assert_eq!(bst.inorder(), vec![&1, &3, &4, &5, &6, &7, &8]);
//    assert!(bst.contains(&4));
//    assert!(!bst.contains(&2));
```

### Ejercicio 3 — Calculadora con AST

```rust
// Extiende el ejemplo del AST de expresiones:
//
// enum Expr {
//     Num(f64),
//     Add(Box<Expr>, Box<Expr>),
//     Mul(Box<Expr>, Box<Expr>),
//     Neg(Box<Expr>),
//     Div(Box<Expr>, Box<Expr>),  // NUEVO
// }
//
// a) Agrega la variante Div y modifica eval() para soportarla.
//    ¿Que pasa si el divisor es 0? Cambia eval() para retornar
//    Result<f64, String> en lugar de f64.
//
// b) Implementa Display para Expr, que imprima con parentesis:
//    Add(Num(2), Mul(Num(3), Num(4))) → "(2 + (3 * 4))"
//
// c) Construye y evalua: (10 + 5) / (3 - (-2))
//    ¿Cual es el resultado?
```
