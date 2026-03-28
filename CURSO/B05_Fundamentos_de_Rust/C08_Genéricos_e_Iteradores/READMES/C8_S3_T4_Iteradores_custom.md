# T04 — Iteradores custom

## Por que crear iteradores propios

La libreria estandar provee iteradores para Vec, HashMap, Range, etc.
Pero cuando creas tus propios tipos, necesitas implementar `Iterator`
para que sean compatibles con `for`, adaptadores y consumidores:

```rust
// Tu tipo custom:
struct Countdown { from: u32 }

// Con Iterator implementado, puedes hacer:
for n in Countdown { from: 5 } {
    println!("{n}");  // 5, 4, 3, 2, 1
}

// Y encadenar adaptadores:
let sum: u32 = Countdown { from: 10 }
    .filter(|&n| n % 2 == 0)
    .sum();
// 10 + 8 + 6 + 4 + 2 = 30
```

---

## Implementar el trait Iterator

El trait `Iterator` requiere una sola cosa: definir `Item` y
el metodo `next()`. Todo lo demas (map, filter, sum, count, etc.)
viene gratis como metodos por defecto:

```rust
// El trait Iterator (simplificado):
trait Iterator {
    type Item;                        // tipo de cada elemento
    fn next(&mut self) -> Option<Self::Item>;  // el unico metodo requerido

    // ~75 metodos por defecto: map, filter, sum, count, etc.
    // Todos se implementan en terminos de next().
}
```

```rust
// Ejemplo: Countdown que cuenta hacia atras desde un numero

struct Countdown {
    current: u32,
}

impl Countdown {
    fn new(from: u32) -> Self {
        Countdown { current: from }
    }
}

impl Iterator for Countdown {
    type Item = u32;

    fn next(&mut self) -> Option<u32> {
        if self.current == 0 {
            None                    // iterador agotado
        } else {
            let val = self.current;
            self.current -= 1;
            Some(val)               // producir el valor actual
        }
    }
}

fn main() {
    // Usar en for:
    for n in Countdown::new(5) {
        print!("{n} ");
    }
    // 5 4 3 2 1

    // Usar con adaptadores:
    let evens: Vec<u32> = Countdown::new(10)
        .filter(|n| n % 2 == 0)
        .collect();
    assert_eq!(evens, vec![10, 8, 6, 4, 2]);
}
```

```text
El contrato de next():

  Some(valor)  →  hay un elemento, el iterador avanza
  None         →  el iterador se agoto, no hay mas elementos

  Una vez que retorna None, DEBE seguir retornando None.
  (Esto se llama "fused iterator" — ver mas abajo)
```

---

## Patron: estado mutable en el iterador

El iterador necesita mantener estado entre llamadas a `next()`.
Ese estado vive en los campos del struct:

```rust
// Iterador que genera la secuencia de Fibonacci:
struct Fibonacci {
    a: u64,
    b: u64,
}

impl Fibonacci {
    fn new() -> Self {
        Fibonacci { a: 0, b: 1 }
    }
}

impl Iterator for Fibonacci {
    type Item = u64;

    fn next(&mut self) -> Option<u64> {
        let current = self.a;
        let next = self.a + self.b;
        self.a = self.b;
        self.b = next;
        Some(current)  // iterador infinito — nunca retorna None
    }
}

fn main() {
    // Fibonacci es infinito, asi que necesitas take:
    let first_10: Vec<u64> = Fibonacci::new().take(10).collect();
    assert_eq!(first_10, vec![0, 1, 1, 2, 3, 5, 8, 13, 21, 34]);

    // Primer Fibonacci mayor que 1000:
    let big = Fibonacci::new().find(|&n| n > 1000);
    assert_eq!(big, Some(1597));
}
```

```rust
// Iterador sobre un rango con paso configurable:
struct StepRange {
    current: i32,
    end: i32,
    step: i32,
}

impl StepRange {
    fn new(start: i32, end: i32, step: i32) -> Self {
        assert!(step != 0, "step cannot be zero");
        StepRange { current: start, end, step }
    }
}

impl Iterator for StepRange {
    type Item = i32;

    fn next(&mut self) -> Option<i32> {
        if (self.step > 0 && self.current >= self.end)
            || (self.step < 0 && self.current <= self.end)
        {
            None
        } else {
            let val = self.current;
            self.current += self.step;
            Some(val)
        }
    }
}

fn main() {
    let v: Vec<i32> = StepRange::new(0, 20, 3).collect();
    assert_eq!(v, vec![0, 3, 6, 9, 12, 15, 18]);

    let v: Vec<i32> = StepRange::new(10, 0, -2).collect();
    assert_eq!(v, vec![10, 8, 6, 4, 2]);
}
```

---

## Iterar sobre los datos de un struct

El caso mas comun es crear un iterador que recorra los datos
internos de tu tipo. Hay varias estrategias:

### Estrategia 1 — Iterador que envuelve un indice

```rust
struct Matrix {
    data: Vec<Vec<f64>>,
    rows: usize,
    cols: usize,
}

// Iterador que recorre todos los elementos fila por fila:
struct MatrixIter<'a> {
    matrix: &'a Matrix,
    row: usize,
    col: usize,
}

impl<'a> Iterator for MatrixIter<'a> {
    type Item = &'a f64;

    fn next(&mut self) -> Option<&'a f64> {
        if self.row >= self.matrix.rows {
            return None;
        }
        let val = &self.matrix.data[self.row][self.col];
        self.col += 1;
        if self.col >= self.matrix.cols {
            self.col = 0;
            self.row += 1;
        }
        Some(val)
    }
}

impl Matrix {
    fn new(data: Vec<Vec<f64>>) -> Self {
        let rows = data.len();
        let cols = if rows > 0 { data[0].len() } else { 0 };
        Matrix { data, rows, cols }
    }

    fn iter(&self) -> MatrixIter {
        MatrixIter { matrix: self, row: 0, col: 0 }
    }
}

fn main() {
    let m = Matrix::new(vec![
        vec![1.0, 2.0, 3.0],
        vec![4.0, 5.0, 6.0],
    ]);

    let all: Vec<&f64> = m.iter().collect();
    assert_eq!(all, vec![&1.0, &2.0, &3.0, &4.0, &5.0, &6.0]);

    let sum: f64 = m.iter().sum();
    assert_eq!(sum, 21.0);
}
```

### Estrategia 2 — Delegar al iterador interno

```rust
// Si tu tipo envuelve una coleccion, puedes delegar directamente:

struct TodoList {
    items: Vec<String>,
}

impl TodoList {
    fn new() -> Self {
        TodoList { items: Vec::new() }
    }

    fn add(&mut self, item: &str) {
        self.items.push(item.to_string());
    }

    // Delegar al iterador de Vec:
    fn iter(&self) -> std::slice::Iter<'_, String> {
        self.items.iter()
    }
}

fn main() {
    let mut list = TodoList::new();
    list.add("buy milk");
    list.add("write code");

    // Funciona porque iter() retorna un Iterator:
    for item in list.iter() {
        println!("- {item}");
    }

    let count = list.iter().count();
    assert_eq!(count, 2);
}
```

### Estrategia 3 — Iterador que consume (into_iter)

```rust
// Iterador que toma ownership de los datos:

struct Deck {
    cards: Vec<String>,
}

// El iterador de consumo:
struct DeckIntoIter {
    iter: std::vec::IntoIter<String>,
}

impl Iterator for DeckIntoIter {
    type Item = String;

    fn next(&mut self) -> Option<String> {
        self.iter.next()  // delegar a IntoIter de Vec
    }
}

impl IntoIterator for Deck {
    type Item = String;
    type IntoIter = DeckIntoIter;

    fn into_iter(self) -> DeckIntoIter {
        DeckIntoIter { iter: self.cards.into_iter() }
    }
}

fn main() {
    let deck = Deck {
        cards: vec!["Ace".into(), "King".into(), "Queen".into()],
    };

    // for consume el deck:
    for card in deck {
        println!("{card}");
    }
    // deck ya no existe aqui
}
```

---

## IntoIterator — habilitar el for loop

El `for` loop en Rust no usa `Iterator` directamente — usa
`IntoIterator`. Cualquier tipo que implemente `IntoIterator`
puede usarse en un `for`:

```rust
// Lo que for realmente hace:
// for x in collection { ... }
//
// Se desugarea a:
// let mut iter = collection.into_iter();
// while let Some(x) = iter.next() { ... }
```

```rust
// IntoIterator tiene 3 formas idomiaticas para colecciones:

let v = vec![1, 2, 3];

// 1. for x in &v     →  IntoIterator para &Vec<T>  →  produce &T
for x in &v {
    // x: &i32 (prestamo compartido)
}

// 2. for x in &mut v →  IntoIterator para &mut Vec<T>  →  produce &mut T
let mut v = vec![1, 2, 3];
for x in &mut v {
    *x *= 2;  // x: &mut i32 (prestamo mutable)
}

// 3. for x in v      →  IntoIterator para Vec<T>  →  produce T
for x in v {
    // x: i32 (ownership, v se consume)
}
```

### Implementar IntoIterator para tu tipo

```rust
// Para que tu tipo soporte las 3 formas de for,
// implementas IntoIterator 3 veces:

struct Playlist {
    songs: Vec<String>,
}

// --- Forma 1: &Playlist (iteracion por referencia) ---

impl<'a> IntoIterator for &'a Playlist {
    type Item = &'a String;
    type IntoIter = std::slice::Iter<'a, String>;

    fn into_iter(self) -> std::slice::Iter<'a, String> {
        self.songs.iter()
    }
}

// --- Forma 2: &mut Playlist (iteracion mutable) ---

impl<'a> IntoIterator for &'a mut Playlist {
    type Item = &'a mut String;
    type IntoIter = std::slice::IterMut<'a, String>;

    fn into_iter(self) -> std::slice::IterMut<'a, String> {
        self.songs.iter_mut()
    }
}

// --- Forma 3: Playlist (iteracion con ownership) ---

impl IntoIterator for Playlist {
    type Item = String;
    type IntoIter = std::vec::IntoIter<String>;

    fn into_iter(self) -> std::vec::IntoIter<String> {
        self.songs.into_iter()
    }
}

fn main() {
    let mut playlist = Playlist {
        songs: vec!["Song A".into(), "Song B".into(), "Song C".into()],
    };

    // &Playlist:
    for song in &playlist {
        println!("listening: {song}");
    }

    // &mut Playlist:
    for song in &mut playlist {
        *song = song.to_uppercase();
    }

    // Playlist (consume):
    for song in playlist {
        println!("owned: {song}");
    }
    // playlist ya no existe
}
```

### Relacion entre iter() e IntoIterator

```rust
// Convencion en Rust:
//
// Si tu tipo implementa IntoIterator para &T,
// tambien deberias proveer un metodo iter():
//
//   impl MyType {
//       fn iter(&self) -> SomeIterator { ... }
//   }
//
// iter() y (&my_value).into_iter() producen lo mismo.
// iter() existe por ergonomia — es mas comodo que &my_value.

let v = vec![1, 2, 3];
// Estos son equivalentes:
let a: Vec<&i32> = v.iter().collect();
let b: Vec<&i32> = (&v).into_iter().collect();
assert_eq!(a, b);
```

---

## Iteradores infinitos

Un iterador que nunca retorna `None` es infinito. Debes usar
`take`, `take_while`, `find`, o similar para limitar:

```rust
// Generador de numeros naturales:
struct Naturals {
    current: u64,
}

impl Naturals {
    fn from(start: u64) -> Self {
        Naturals { current: start }
    }
}

impl Iterator for Naturals {
    type Item = u64;

    fn next(&mut self) -> Option<u64> {
        let val = self.current;
        self.current += 1;
        Some(val)  // siempre Some — nunca se agota
    }
}

fn main() {
    // NUNCA hacer .collect() en un iterador infinito sin take:
    // let v: Vec<u64> = Naturals::from(0).collect();  // loop infinito

    // Correcto:
    let first_5: Vec<u64> = Naturals::from(1).take(5).collect();
    assert_eq!(first_5, vec![1, 2, 3, 4, 5]);

    // Primer cuadrado perfecto > 1000:
    let big_square = Naturals::from(1)
        .map(|n| n * n)
        .find(|&sq| sq > 1000);
    assert_eq!(big_square, Some(1024));  // 32*32
}
```

```rust
// La libreria estandar tiene constructores para iteradores infinitos:

// Repetir un valor:
let fives: Vec<i32> = std::iter::repeat(5).take(3).collect();
assert_eq!(fives, vec![5, 5, 5]);

// Repetir con closure:
let mut counter = 0;
let ids: Vec<i32> = std::iter::repeat_with(|| {
    counter += 1;
    counter
}).take(4).collect();
assert_eq!(ids, vec![1, 2, 3, 4]);

// Generar con funcion (from_fn):
let mut n = 0;
let powers: Vec<u32> = std::iter::from_fn(|| {
    n += 1;
    Some(2_u32.pow(n))
}).take(5).collect();
assert_eq!(powers, vec![2, 4, 8, 16, 32]);

// Sucesores (successors):
let powers_of_2: Vec<u32> = std::iter::successors(Some(1), |&prev| {
    Some(prev * 2)
}).take(6).collect();
assert_eq!(powers_of_2, vec![1, 2, 4, 8, 16, 32]);
```

---

## size_hint y ExactSizeIterator

```rust
// size_hint() da una estimacion del numero de elementos restantes:
// fn size_hint(&self) -> (usize, Option<usize>)
//   (lower_bound, upper_bound_or_none)

let v = vec![1, 2, 3, 4, 5];
let iter = v.iter();
assert_eq!(iter.size_hint(), (5, Some(5)));  // exactamente 5

let filtered = v.iter().filter(|&&x| x > 2);
assert_eq!(filtered.size_hint(), (0, Some(5)));  // entre 0 y 5
// filter no sabe cuantos pasaran sin recorrerlos

// size_hint es usado por collect para pre-allocar capacidad:
// Si el hint dice (5, Some(5)), collect crea un Vec con capacity 5.
```

```rust
// Implementar size_hint en tu iterador (opcional pero recomendado):
struct Countdown { current: u32 }

impl Iterator for Countdown {
    type Item = u32;

    fn next(&mut self) -> Option<u32> {
        if self.current == 0 { None }
        else {
            let val = self.current;
            self.current -= 1;
            Some(val)
        }
    }

    // Override del metodo por defecto:
    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.current as usize;
        (remaining, Some(remaining))  // sabemos exactamente cuantos quedan
    }
}

// Si size_hint es exacto, puedes implementar ExactSizeIterator:
impl ExactSizeIterator for Countdown {
    fn len(&self) -> usize {
        self.current as usize
    }
}
```

---

## DoubleEndedIterator

Si tu iterador puede producir elementos desde ambos extremos,
implementa `DoubleEndedIterator` para habilitar `.rev()` y
`.next_back()`:

```rust
struct Range {
    start: i32,
    end: i32,  // exclusivo
}

impl Iterator for Range {
    type Item = i32;

    fn next(&mut self) -> Option<i32> {
        if self.start >= self.end {
            None
        } else {
            let val = self.start;
            self.start += 1;
            Some(val)
        }
    }
}

impl DoubleEndedIterator for Range {
    fn next_back(&mut self) -> Option<i32> {
        if self.start >= self.end {
            None
        } else {
            self.end -= 1;
            Some(self.end)
        }
    }
}

fn main() {
    // Hacia adelante:
    let forward: Vec<i32> = Range { start: 1, end: 6 }.collect();
    assert_eq!(forward, vec![1, 2, 3, 4, 5]);

    // Hacia atras con rev():
    let backward: Vec<i32> = Range { start: 1, end: 6 }.rev().collect();
    assert_eq!(backward, vec![5, 4, 3, 2, 1]);

    // next y next_back intercalados:
    let mut r = Range { start: 1, end: 6 };
    assert_eq!(r.next(), Some(1));
    assert_eq!(r.next_back(), Some(5));
    assert_eq!(r.next(), Some(2));
    assert_eq!(r.next_back(), Some(4));
    assert_eq!(r.next(), Some(3));
    assert_eq!(r.next(), None);      // se cruzaron
    assert_eq!(r.next_back(), None);
}
```

---

## Ejemplo completo — lista enlazada iterable

```rust
// Una lista enlazada simple con soporte completo de iteracion:

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

    fn iter(&self) -> ListIter<T> {
        ListIter { current: self }
    }
}

// Iterador por referencia:
struct ListIter<'a, T> {
    current: &'a List<T>,
}

impl<'a, T> Iterator for ListIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        match self.current {
            List::Cons(val, next) => {
                self.current = next;
                Some(val)
            }
            List::Nil => None,
        }
    }
}

// IntoIterator para &List<T>:
impl<'a, T> IntoIterator for &'a List<T> {
    type Item = &'a T;
    type IntoIter = ListIter<'a, T>;

    fn into_iter(self) -> ListIter<'a, T> {
        self.iter()
    }
}

fn main() {
    let list = List::new().push(3).push(2).push(1);

    // Con iter():
    let v: Vec<&i32> = list.iter().collect();
    assert_eq!(v, vec![&1, &2, &3]);

    // Con for (usa IntoIterator):
    for val in &list {
        print!("{val} ");
    }
    // 1 2 3

    // Con adaptadores:
    let sum: i32 = list.iter().sum();
    assert_eq!(sum, 6);
}
```

---

## Errores comunes

```rust
// ERROR 1: olvidar que next() toma &mut self
struct MyIter { val: i32 }

impl Iterator for MyIter {
    type Item = i32;

    // fn next(&self) → error: method `next` has an incompatible type
    // Debe ser &mut self porque next() muta el estado del iterador

    fn next(&mut self) -> Option<i32> {
        if self.val > 0 {
            self.val -= 1;
            Some(self.val + 1)
        } else {
            None
        }
    }
}
```

```rust
// ERROR 2: no retornar None despues de agotar el iterador
struct Bad {
    items: Vec<i32>,
    pos: usize,
}

impl Iterator for Bad {
    type Item = i32;

    fn next(&mut self) -> Option<i32> {
        // MAL: si pos pasa del final, items[pos] hace panic
        // let val = self.items[self.pos];
        // self.pos += 1;
        // Some(val)

        // BIEN: verificar limites
        if self.pos >= self.items.len() {
            None
        } else {
            let val = self.items[self.pos];
            self.pos += 1;
            Some(val)
        }
    }
}
```

```rust
// ERROR 3: implementar IntoIterator pero olvidar iter()
struct MyCollection { data: Vec<i32> }

// Implementas IntoIterator...
impl IntoIterator for MyCollection {
    type Item = i32;
    type IntoIter = std::vec::IntoIter<i32>;
    fn into_iter(self) -> std::vec::IntoIter<i32> {
        self.data.into_iter()
    }
}

// ...pero el usuario espera poder hacer:
// let c = MyCollection { data: vec![1, 2, 3] };
// for x in &c { ... }  // error: IntoIterator no implementado para &MyCollection

// Solucion: tambien implementar para &MyCollection:
impl<'a> IntoIterator for &'a MyCollection {
    type Item = &'a i32;
    type IntoIter = std::slice::Iter<'a, i32>;
    fn into_iter(self) -> std::slice::Iter<'a, i32> {
        self.data.iter()
    }
}

// Y proveer el metodo iter() por conveniencia:
impl MyCollection {
    fn iter(&self) -> std::slice::Iter<'_, i32> {
        self.data.iter()
    }
}
```

```rust
// ERROR 4: collect en iterador infinito sin take
struct Forever;

impl Iterator for Forever {
    type Item = i32;
    fn next(&mut self) -> Option<i32> { Some(42) }
}

// let v: Vec<i32> = Forever.collect();  // loop infinito, consume toda la RAM

// Solucion: siempre limitar iteradores infinitos:
let v: Vec<i32> = Forever.take(5).collect();
assert_eq!(v, vec![42, 42, 42, 42, 42]);
```

```rust
// ERROR 5: lifetime incorrecto en iterador por referencia
struct Data { values: Vec<i32> }

// El iterador necesita un lifetime que conecte con Data:
struct DataIter<'a> {
    data: &'a Data,  // referencia a Data
    pos: usize,
}

impl<'a> Iterator for DataIter<'a> {
    type Item = &'a i32;  // el Item vive tanto como Data, no como DataIter

    fn next(&mut self) -> Option<&'a i32> {
        if self.pos < self.data.values.len() {
            let val = &self.data.values[self.pos];
            self.pos += 1;
            Some(val)
        } else {
            None
        }
    }
}

// El lifetime 'a conecta:  Data <--'a-- DataIter <--'a-- &Item
// El Item (&i32) vive mientras Data exista, no mientras DataIter exista.
```

---

## Cheatsheet

```text
Implementar Iterator:
  struct MyIter { /* estado */ }
  impl Iterator for MyIter {
      type Item = T;
      fn next(&mut self) -> Option<T> { ... }
  }

Habilitar for loop (IntoIterator):
  impl IntoIterator for MyType       → for x in val    (consume)
  impl IntoIterator for &MyType      → for x in &val   (referencia)
  impl IntoIterator for &mut MyType  → for x in &mut val (mutable)

Convencion de metodos:
  .iter()      → iterador por &T      (no consume)
  .iter_mut()  → iterador por &mut T  (no consume)
  .into_iter() → iterador por T       (consume)

Traits opcionales para tu iterador:
  size_hint()             → estimacion de elementos restantes
  ExactSizeIterator       → .len() exacto
  DoubleEndedIterator     → .rev() y .next_back()

Constructores de la stdlib para iteradores custom:
  std::iter::once(val)         → iterador de 1 elemento
  std::iter::empty()           → iterador de 0 elementos
  std::iter::repeat(val)       → infinito, repite un valor
  std::iter::repeat_with(f)    → infinito, repite con closure
  std::iter::from_fn(f)        → desde closure que retorna Option
  std::iter::successors(s, f)  → cada elemento depende del anterior

Reglas clave:
  - Solo necesitas implementar next() — todo lo demas viene gratis
  - next() retorna Some(val) o None, nunca panic
  - Despues de retornar None, debe seguir retornando None
  - El iterador toma &mut self — muta su estado interno
  - Para iterar por referencia, el Item es &'a T (con lifetime)
  - Iteradores infinitos SIEMPRE requieren take/find/take_while
```

---

## Ejercicios

### Ejercicio 1 — Iterador basico

```rust
// Implementa un iterador Repeat<T> que repita un valor
// exactamente N veces:
//
// let v: Vec<&str> = Repeat::new("hello", 3).collect();
// assert_eq!(v, vec!["hello", "hello", "hello"]);
//
// a) Define el struct Repeat<T> con los campos necesarios.
//    ¿Que trait bound necesita T? (pista: el valor se repite)
//
// b) Implementa Iterator para Repeat<T>.
//    ¿Que retorna next() cuando se agotaron las repeticiones?
//
// c) Implementa ExactSizeIterator. ¿Que retorna len()?
//
// d) Verifica que funciona con adaptadores:
//    Repeat::new(1, 5).map(|x| x * 2).sum::<i32>() == 10
```

### Ejercicio 2 — IntoIterator

```rust
// Dado:
struct Grid {
    cells: Vec<Vec<i32>>,
}

// a) Implementa un metodo iter() que retorne un iterador
//    sobre todos los valores de la grilla, fila por fila.
//    (pista: usa flat_map o flatten delegando al iterador de Vec)
//
// b) Implementa IntoIterator para &Grid, de forma que puedas
//    escribir: for cell in &grid { ... }
//
// c) Verifica que funciona:
//    let grid = Grid { cells: vec![vec![1,2], vec![3,4]] };
//    let sum: i32 = grid.iter().sum();
//    assert_eq!(sum, 10);
//
// d) ¿Por que es mas dificil implementar IntoIterator para
//    Grid (con ownership)? Intentalo y observa el tipo de
//    IntoIter que necesitas.
```

### Ejercicio 3 — Iterador infinito con logica

```rust
// Implementa un iterador Collatz que genere la secuencia de
// Collatz a partir de un numero n:
//   - Si n es par: siguiente = n / 2
//   - Si n es impar: siguiente = 3n + 1
//   - La secuencia termina cuando n llega a 1
//
// Ejemplo: Collatz(6) → 6, 3, 10, 5, 16, 8, 4, 2, 1
//
// a) Define el struct e implementa Iterator.
//    ¿Debe retornar None despues del 1, o incluir el 1?
//
// b) let seq: Vec<u64> = Collatz::new(6).collect();
//    assert_eq!(seq, vec![6, 3, 10, 5, 16, 8, 4, 2, 1]);
//
// c) ¿Cuantos pasos toma Collatz(27)? Usa .count()
//
// d) ¿Cual es el valor maximo alcanzado por Collatz(27)?
//    Usa .max()
```
