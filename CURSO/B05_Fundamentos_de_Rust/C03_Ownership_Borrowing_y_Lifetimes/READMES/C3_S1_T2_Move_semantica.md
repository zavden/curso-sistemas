# T02 — Move semantica

## Move por defecto

En Rust, la asignacion de tipos que no implementan `Copy` transfiere
la propiedad (ownership). Despues del move, la variable original
deja de ser valida. Esto previene doble liberacion de memoria.

```rust
fn main() {
    let s1 = String::from("hello");
    let s2 = s1;       // s1 se mueve a s2

    println!("{}", s2); // OK
    // println!("{}", s1); // ERROR de compilacion
}

// error[E0382]: borrow of moved value: `s1`
//  --> src/main.rs:5:20
//   |
// 2 |     let s1 = String::from("hello");
//   |         -- move occurs because `s1` has type `String`,
//   |            which does not implement the `Copy` trait
// 3 |     let s2 = s1;
//   |              -- value moved here
// 5 |     println!("{}", s1);
//   |                    ^^ value borrowed here after move
```

```rust
// El move ocurre en asignaciones, reasignaciones y bloques:
fn main() {
    let s1 = String::from("hello");
    let s2 = s1; // move — s1 invalidado

    let mut s3 = String::from("first");
    s3 = String::from("second"); // "first" se droppea, s3 posee "second"

    let s4;
    {
        let s5 = String::from("inner");
        s4 = s5; // move de s5 a s4
    }
    println!("{}, {}, {}", s2, s3, s4);
}
```

```rust
// Los enteros NO se mueven — se copian:
fn main() {
    let x = 42;
    let y = x;      // copia, no move
    println!("x = {}, y = {}", x, y); // ambos validos
}
```

## Por que String se mueve pero i32 se copia

Un `i32` vive enteramente en el stack (4 bytes). Copiar esos bytes
es rapido y seguro. Un `String` tiene datos en el stack (puntero,
longitud, capacidad) que apuntan a datos en el heap. Copiar solo
la parte del stack crearia dos punteros al mismo heap — al liberar
ambos se produciria un double free.

```text
i32 en memoria:

Stack
+-----------+
| x: 42     |   ← 4 bytes, todo en stack
+-----------+

Despues de let y = x:
+-----------+
| x: 42     |   ← copia independiente
| y: 42     |   ← copia independiente
+-----------+
```

```text
String en memoria (let s1 = String::from("hello")):

Stack                          Heap
+-------------------+          +---+---+---+---+---+
| s1                |          | h | e | l | l | o |
|   ptr --------+   |    +--→ +---+---+---+---+---+
|   len: 5      |   |    |
|   capacity: 5 |   |    |
+-------------------+    |

Si se copiaran solo los 24 bytes del stack:

Stack                          Heap
+-------------------+          +---+---+---+---+---+
| s1  ptr ------+   |    +--→ | h | e | l | l | o |
| s2  ptr ------+   |    +--→ +---+---+---+---+---+
+-------------------+
  DOS punteros al MISMO heap → double free al destruir ambos.

Lo que Rust hace (move):

Stack                          Heap
+-------------------+          +---+---+---+---+---+
| s1 [INVALIDADO]   |          | h | e | l | l | o |
+-------------------+     +--→ +---+---+---+---+---+
| s2                |     |
|   ptr --------+   |-----+
|   len: 5      |   |
|   capacity: 5 |   |
+-------------------+
  Solo s2 apunta al heap. Coste: copiar 24 bytes de stack. O(1).
```

## Move en llamadas a funciones

Pasar un valor a una funcion mueve la propiedad al parametro.
El caller pierde acceso. Retornar un valor mueve la propiedad
al caller.

```rust
fn takes_ownership(s: String) {
    println!("Inside: {}", s);
} // s se libera aqui

fn gives_ownership() -> String {
    let s = String::from("hello");
    s // move al caller — no se libera aqui
}

fn main() {
    let s = String::from("hello");
    takes_ownership(s);
    // println!("{}", s); // ERROR E0382: value used after being moved

    let s2 = gives_ownership();
    println!("{}", s2); // OK — main posee el String
}
```

```rust
// Patron tedioso: tomar y devolver para "prestar":
fn calculate_length(s: String) -> (String, usize) {
    let length = s.len();
    (s, length) // devolver el String junto con el resultado
}

fn main() {
    let s1 = String::from("hello");
    let (s2, len) = calculate_length(s1);
    println!("'{}' has length {}", s2, len);
}
// Esto funciona pero es incomodo.
// Para eso existen las referencias (&T) — siguiente seccion.
```

```rust
// Los tipos Copy no se mueven al pasar a funciones:
fn takes_i32(x: i32) {
    println!("Inside: {}", x);
}

fn main() {
    let x = 42;
    takes_i32(x);
    println!("Still valid: {}", x); // OK — i32 se copio
}
```

## Move en colecciones

Las colecciones toman propiedad de los valores insertados.
Insertar mueve al interior; extraer mueve hacia fuera.

```rust
fn main() {
    let s = String::from("hello");
    let mut v = Vec::new();

    v.push(s); // s se mueve al Vec
    // println!("{}", s); // ERROR: s se movio

    // Acceder por indice da referencia, no move:
    println!("{}", v[0]); // OK — v[0] es &String

    // let s2 = v[0];     // ERROR: cannot move out of index

    // Metodos que mueven fuera:
    let last = v.pop();       // Option<String> — mueve fuera
    println!("{:?}", last);
}
```

```rust
fn main() {
    let mut v = vec![
        String::from("one"),
        String::from("two"),
        String::from("three"),
    ];

    let removed = v.remove(0);     // mueve "one" fuera, O(n)
    let swapped = v.swap_remove(0); // mueve "two" fuera, O(1), cambia orden
    println!("{}, {}", removed, swapped);
    println!("{:?}", v); // ["three"]
}
```

```rust
// Moves parciales en structs:
struct Person {
    name: String,
    age: u32,
}

fn main() {
    let person = Person {
        name: String::from("Alice"),
        age: 30,
    };

    let name = person.name;     // move parcial
    // println!("{}", person.name);  // ERROR: name fue movido
    println!("{}", person.age);      // OK — u32 es Copy
    println!("{}", name);

    // Para evitar moves parciales, usar referencia:
    let p2 = Person { name: String::from("Bob"), age: 25 };
    let name_ref = &p2.name;
    println!("{}, {}", name_ref, p2.age); // OK — p2 intacta
}
```

## El trait Copy

`Copy` es un trait marcador: le dice al compilador que el tipo
se puede duplicar copiando bits. La copia es **implicita** — no
existe un metodo `.copy()`.

```rust
fn main() {
    // Todos estos tipos implementan Copy:
    let a: i32 = 1;   let b = a;   // copia
    let x: f64 = 3.14; let y = x;  // copia
    let t = true;      let f = t;   // copia
    let c = 'R';       let d = c;   // copia

    // Referencias inmutables son Copy:
    let s = String::from("hello");
    let r1 = &s;
    let r2 = r1; // copia de la referencia (no del String)
    println!("{}, {}", r1, r2);

    println!("{}, {}, {}, {}, {}, {}", a, b, x, y, t, c);
}
```

```rust
// Tuplas y arrays de tipos Copy son Copy:
fn main() {
    let point = (3, 4);
    let point2 = point; // copia
    println!("{:?}, {:?}", point, point2); // ambos validos

    let arr = [1, 2, 3];
    let arr2 = arr; // copia
    println!("{:?}, {:?}", arr, arr2);

    // Pero si contienen un tipo no-Copy, NO son Copy:
    let mixed = (1, String::from("x"));
    let mixed2 = mixed; // MOVE
    // println!("{:?}", mixed); // ERROR
    println!("{:?}", mixed2);
}
```

## El trait Clone

`Clone` permite crear una copia **explicita** llamando `.clone()`.
A diferencia de `Copy`, puede hacer copias profundas que involucran
asignacion de heap.

```rust
fn main() {
    let s1 = String::from("hello");
    let s2 = s1.clone(); // deep copy — duplica datos del heap
    println!("s1 = {}, s2 = {}", s1, s2); // ambos validos
}
```

```text
Despues de s2 = s1.clone():

Stack                          Heap
+-------------------+          +---+---+---+---+---+
| s1  ptr ----------|----+--→ | h | e | l | l | o |
|   len: 5          |    |     +---+---+---+---+---+
+-------------------+    |
                         |
+-------------------+    |     +---+---+---+---+---+
| s2  ptr ----------|----+--→ | h | e | l | l | o |  ← nueva asignacion
|   len: 5          |          +---+---+---+---+---+
+-------------------+
  Cada String tiene su propio bloque de heap.
```

```rust
// Clone puede ser costoso:
fn main() {
    let big = "x".repeat(1_000_000);
    let _big_clone = big.clone(); // ~1 MB de asignacion + copia

    let v: Vec<String> = (0..1000).map(|i| format!("item_{}", i)).collect();
    let _v_clone = v.clone(); // 1000 asignaciones de heap individuales
}
```

```rust
// Derivar Clone automaticamente:
#[derive(Clone)]
struct Config {
    name: String,
    max_retries: u32,
}

fn main() {
    let c1 = Config { name: String::from("prod"), max_retries: 3 };
    let c2 = c1.clone(); // clona cada campo
    println!("{}, {}", c1.name, c2.name);
}

// #[derive(Clone)] genera una implementacion que llama .clone()
// en cada campo. Si algun campo no implementa Clone, no compila.
```

## Copy vs Clone

`Copy` es un subtrait de `Clone`: todo tipo `Copy` debe ser `Clone`,
pero no al reves.

```text
+-------------------+-----------------------------+-----------------------------+
| Caracteristica    | Copy                        | Clone                       |
+-------------------+-----------------------------+-----------------------------+
| Invocacion        | Implicita (automatica)      | Explicita (.clone())        |
| Coste             | Barato (bitwise copy)       | Puede ser costoso           |
| Memoria           | Solo stack                  | Puede involucrar heap       |
| Semantica         | Copia bit a bit             | Puede tener logica custom   |
| Restriccion       | No puede tener Drop         | Puede tener Drop            |
+-------------------+-----------------------------+-----------------------------+
```

```rust
// Derivar ambos para structs con campos solo Copy:
#[derive(Copy, Clone, Debug)]
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let p1 = Point { x: 1.0, y: 2.0 };
    let p2 = p1;          // copia implicita (Copy)
    let p3 = p1.clone();  // tambien funciona
    println!("{:?}, {:?}, {:?}", p1, p2, p3); // todos validos
}
```

```rust
// Si un campo no es Copy, el struct tampoco puede serlo:
#[derive(Clone, Debug)]
struct NamedPoint {
    x: f64,
    y: f64,
    label: String, // String no es Copy → NamedPoint no puede ser Copy
}

fn main() {
    let p1 = NamedPoint { x: 1.0, y: 2.0, label: String::from("A") };
    let p2 = p1.clone(); // OK — Clone funciona
    let p3 = p1;          // MOVE — no hay Copy
    // println!("{:?}", p1); // ERROR
    println!("{:?}, {:?}", p2, p3);
}
```

## Cuando un tipo no puede ser Copy

Un tipo no puede implementar `Copy` en cuatro situaciones:

```rust
// 1. Tiene campos no-Copy (String, Vec, Box, HashMap, etc.):
//
// #[derive(Copy, Clone)]
// struct User { name: String, age: u32 }
//
// error[E0204]: the trait `Copy` may not be implemented for this type
//   |     name: String,
//   |     ------------- this field does not implement `Copy`
```

```rust
// 2. Implementa Drop (Copy y Drop son mutuamente excluyentes):
struct Resource { id: u32 }

impl Drop for Resource {
    fn drop(&mut self) {
        println!("Liberando recurso {}", self.id);
    }
}

// impl Copy for Resource {} // ERROR:
// error[E0184]: the trait `Copy` may not be implemented for this type;
// the type has a destructor

fn main() {
    let r1 = Resource { id: 1 };
    let r2 = r1; // MOVE — si fuera Copy, drop() se llamaria dos veces
    println!("r2 id: {}", r2.id);
}
```

```rust
// 3. &mut T no es Copy (seria violar la regla de una sola referencia mutable):
fn main() {
    let mut x = 42;
    let r1 = &mut x;
    let r2 = r1; // MOVE de la referencia mutable
    // println!("{}", r1); // ERROR: r1 se movio
    println!("{}", r2);
}
```

```text
Resumen — tipos Copy vs no-Copy:

Copy (copia implicita):              No-Copy (se mueven):
  i8..i128, u8..u128, isize, usize    String
  f32, f64                             Vec<T>
  bool                                 Box<T>
  char                                 HashMap<K, V>, HashSet<T>
  &T (referencias inmutables)          Rc<T>, Arc<T>
  (T1, T2, ..) si todos Ti son Copy    &mut T
  [T; N] si T es Copy                  Cualquier struct con campo no-Copy
                                       Cualquier tipo con Drop
```

```rust
// Patrones comunes con move:

// 1. Clonar para usar un valor en varios lugares:
fn process(s: String) { println!("{}", s); }

fn main() {
    let s = String::from("data");
    process(s.clone()); // clone para la primera llamada
    process(s);         // move en la ultima — ya no necesitamos s
}

// 2. Move en match — extraer de Option/Result:
fn main() {
    let opt: Option<String> = Some(String::from("hello"));

    match opt {
        Some(s) => println!("Got: {}", s), // s toma propiedad
        None => println!("Nothing"),
    }
    // opt ya no es valido

    // Para no mover, usar referencia:
    let opt2: Option<String> = Some(String::from("world"));
    match &opt2 {
        Some(s) => println!("Got: {}", s), // s es &String
        None => println!("Nothing"),
    }
    println!("opt2 still valid: {:?}", opt2); // OK
}

// 3. into_iter() consume la coleccion; iter() no:
fn main() {
    let names = vec![String::from("Alice"), String::from("Bob")];
    for name in names.into_iter() { // move de cada elemento
        println!("{}", name);
    }
    // names ya no existe

    let names2 = vec![String::from("Carol"), String::from("Dave")];
    for name in names2.iter() { // &String, no move
        println!("{}", name);
    }
    println!("{:?}", names2); // OK
}
```

---

## Ejercicios

### Ejercicio 1 — Identificar moves y copies

```rust
// Para cada linea marcada, indicar si ocurre MOVE o COPY.
// Indicar cuales variables son validas al final.
// Verificar compilando (descomentar los println!).

fn main() {
    let a: i32 = 10;
    let b = a;              // (1) move o copy?

    let s1 = String::from("hello");
    let s2 = s1;            // (2) move o copy?

    let t = (1, 2.0, true);
    let t2 = t;             // (3) move o copy?

    let mixed = (1, String::from("x"));
    let mixed2 = mixed;     // (4) move o copy?

    let r = &s2;
    let r2 = r;             // (5) move o copy?

    // Descomentar para verificar:
    // println!("{}", a);
    // println!("{}", b);
    // println!("{}", s1);      // compila?
    // println!("{}", s2);
    // println!("{:?}", t);
    // println!("{:?}", t2);
    // println!("{:?}", mixed);  // compila?
    // println!("{:?}", mixed2);
    // println!("{}", r);
    // println!("{}", r2);
}
```

### Ejercicio 2 — Corregir errores de move

```rust
// Este codigo tiene 3 errores E0382 (use after move).
// Corregir cada uno de manera diferente:
//   - Uno con .clone()
//   - Uno reestructurando el orden
//   - Uno cambiando la funcion para tomar referencia

fn print_string(s: String) {
    println!("{}", s);
}

fn main() {
    let name = String::from("Rust");
    let greeting = String::from("Hello");

    // Error 1:
    print_string(name);
    println!("Name was: {}", name);

    // Error 2:
    let g1 = greeting;
    let g2 = greeting;
    println!("{}, {}", g1, g2);

    // Error 3:
    let msg = String::from("tick");
    for _ in 0..3 {
        print_string(msg);
    }
}
```

### Ejercicio 3 — Implementar Copy y Clone

```rust
// 1. Crear struct Color (r, g, b: u8). Derivar Copy y Clone.
//    Verificar que la asignacion copia, no mueve.
//
// 2. Crear struct Palette (colors: Vec<Color>). Derivar solo Clone.
//    Verificar que la asignacion mueve y .clone() copia.
//
// 3. Intentar derivar Copy en Palette. Leer el error del compilador
//    y explicar por que no es posible.
//
// 4. Crear struct Counter (count: u32). Implementar Drop (que imprima
//    "Counter dropped"). Intentar derivar Copy. Observar el error
//    y explicar por que Copy y Drop son incompatibles.
```
