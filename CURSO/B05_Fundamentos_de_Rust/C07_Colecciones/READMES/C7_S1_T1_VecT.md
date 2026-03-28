# T01 — Vec\<T\>

## Que es Vec

`Vec<T>` es un array dinamico: crece y decrece en tiempo de
ejecucion, almacena sus elementos contiguos en el heap, y es
la coleccion mas usada en Rust.

```
// Vec es un buffer contiguo en el heap:
//
//  Stack                    Heap
// ┌──────────┐             ┌───┬───┬───┬───┬───┬───┐
// │ ptr ──────────────────▶│ 1 │ 2 │ 3 │   │   │   │
// │ len: 3   │             └───┴───┴───┴───┴───┴───┘
// │ cap: 6   │              ▲ elementos ▲  ▲ espacio ▲
// └──────────┘              └───────────┘  └ libre ──┘
//
// ptr — puntero al inicio del buffer en el heap
// len — cuantos elementos hay actualmente
// cap — cuantos elementos caben antes de realocar
```

```rust
// Un Vec<T> en el stack siempre ocupa 3 usize (24 bytes en 64-bit):
use std::mem::size_of;
assert_eq!(size_of::<Vec<i32>>(), 24);   // ptr + len + cap
assert_eq!(size_of::<Vec<u8>>(), 24);    // mismo tamaño sin importar T
assert_eq!(size_of::<Vec<String>>(), 24);
```

## Crear un Vec

```rust
// 1. Vec::new() — vacio, sin alocar heap
let mut v: Vec<i32> = Vec::new();
// len = 0, cap = 0, no se aloca heap hasta el primer push

// 2. vec![] — macro, la forma mas comun
let v = vec![1, 2, 3];          // Vec<i32> con 3 elementos
let v = vec![0; 10];            // Vec<i32> con 10 ceros
let v = vec![String::new(); 5]; // Vec<String> con 5 strings vacios
// Nota: vec![val; n] requiere que val implemente Clone

// 3. Vec::with_capacity() — reserva espacio sin elementos
let mut v: Vec<i32> = Vec::with_capacity(100);
assert_eq!(v.len(), 0);    // vacio
assert_eq!(v.capacity(), 100);  // pero puede recibir 100 sin realocar

// 4. collect() — desde un iterador
let v: Vec<i32> = (1..=5).collect();  // [1, 2, 3, 4, 5]
let v: Vec<char> = "hello".chars().collect();  // ['h', 'e', 'l', 'l', 'o']
```

```text
¿Cuando usar with_capacity?
- Cuando sabes (o estimas) cuantos elementos tendras
- Evita realocaciones innecesarias
- Ejemplo: leer lineas de un archivo → estimas con file_size / avg_line_len
```

## push, pop y modificacion

```rust
let mut v = Vec::new();

// push — agrega al final, O(1) amortizado
v.push(10);
v.push(20);
v.push(30);
// v = [10, 20, 30]

// pop — quita el ultimo, retorna Option<T>
let last = v.pop();      // Some(30)
let last = v.pop();      // Some(20)
let last = v.pop();      // Some(10)
let last = v.pop();      // None — vacio
```

```rust
let mut v = vec![1, 2, 3, 4, 5];

// insert — inserta en posicion, desplaza el resto, O(n)
v.insert(0, 99);   // [99, 1, 2, 3, 4, 5]
v.insert(3, 88);   // [99, 1, 2, 88, 3, 4, 5]

// remove — quita en posicion, desplaza el resto, O(n)
let removed = v.remove(0);  // removed = 99, v = [1, 2, 88, 3, 4, 5]

// swap_remove — quita en posicion, reemplaza con el ultimo, O(1)
// NO preserva el orden
let mut v = vec![10, 20, 30, 40, 50];
let removed = v.swap_remove(1);  // removed = 20, v = [10, 50, 30, 40]
//                                              el ultimo (50) tomo el lugar de 20
```

```rust
let mut v = vec![1, 2, 3, 4, 5];

// clear — vacia sin desalocar
v.clear();
assert_eq!(v.len(), 0);
assert!(v.capacity() >= 5);  // el buffer sigue alocado

// truncate — recorta a n elementos
let mut v = vec![1, 2, 3, 4, 5];
v.truncate(3);   // [1, 2, 3] — dropea 4 y 5

// retain — filtra in-place
let mut v = vec![1, 2, 3, 4, 5, 6];
v.retain(|&x| x % 2 == 0);  // [2, 4, 6]

// dedup — elimina duplicados CONSECUTIVOS (el vec debe estar ordenado)
let mut v = vec![1, 1, 2, 3, 3, 3, 4];
v.dedup();  // [1, 2, 3, 4]

// extend — agrega multiples elementos desde un iterador
let mut v = vec![1, 2];
v.extend([3, 4, 5]);            // [1, 2, 3, 4, 5]
v.extend(10..=12);              // [1, 2, 3, 4, 5, 10, 11, 12]
v.extend_from_slice(&[99, 100]); // agrega desde un slice
```

## Indexado y acceso

```rust
let v = vec![10, 20, 30, 40, 50];

// Indexado con [] — panic si esta fuera de rango
let first = v[0];    // 10
let third = v[2];    // 30
// let bad = v[10];  // panic: index out of bounds

// .get() — retorna Option, sin panic
let first = v.get(0);    // Some(&10)
let bad   = v.get(10);   // None

// first, last
let f = v.first();  // Some(&10)
let l = v.last();   // Some(&50)
let empty: Vec<i32> = vec![];
assert_eq!(empty.first(), None);
```

```rust
// Acceso mutable
let mut v = vec![10, 20, 30];

v[1] = 99;                  // v = [10, 99, 30]

if let Some(last) = v.last_mut() {
    *last = 0;               // v = [10, 99, 0]
}

// get_mut — acceso mutable con bounds check
if let Some(elem) = v.get_mut(0) {
    *elem += 1000;           // v = [1010, 99, 0]
}
```

```rust
// Slicing — obtener un &[T] de parte del Vec
let v = vec![10, 20, 30, 40, 50];

let slice: &[i32] = &v[1..4];   // [20, 30, 40]
let first_two = &v[..2];        // [10, 20]
let from_third = &v[2..];       // [30, 40, 50]
let all = &v[..];               // [10, 20, 30, 40, 50]

// Los rangos fuera de limites causan panic:
// let bad = &v[3..10];  // panic
```

```text
Regla practica:
  v[i]     → cuando SABES que el indice es valido (panic si no)
  v.get(i) → cuando el indice PUEDE ser invalido (retorna Option)

En loops, prefiere iteradores sobre indices:
  BIEN: for x in &v { ... }
  OK:   for i in 0..v.len() { let x = v[i]; ... }
  El iterador es mas idiomatico, seguro, y a veces mas rapido.
```

## Iteracion

```rust
let v = vec![10, 20, 30];

// &v — prestamo inmutable, no consume el Vec
for val in &v {
    println!("{val}");  // val es &i32
}
// v sigue disponible aqui

// &mut v — prestamo mutable, modifica in-place
let mut v = vec![10, 20, 30];
for val in &mut v {
    *val *= 2;  // val es &mut i32
}
// v = [20, 40, 60]

// v (sin &) — consume el Vec, mueve los elementos
let v = vec![String::from("a"), String::from("b")];
for s in v {
    println!("{s}");  // s es String (no &String)
}
// v ya no existe — fue consumido
```

```rust
// Equivalencia explicita:
// for x in &v      →  for x in v.iter()
// for x in &mut v  →  for x in v.iter_mut()
// for x in v       →  for x in v.into_iter()
```

```rust
// Iterar con indice
let v = vec!["a", "b", "c"];
for (i, val) in v.iter().enumerate() {
    println!("{i}: {val}");
}

// Iterar en reversa
for val in v.iter().rev() {
    println!("{val}");  // c, b, a
}

// windows y chunks
let v = vec![1, 2, 3, 4, 5];
for w in v.windows(3) {
    println!("{w:?}");  // [1,2,3], [2,3,4], [3,4,5]
}
for c in v.chunks(2) {
    println!("{c:?}");  // [1,2], [3,4], [5]
}
```

## Capacidad vs longitud

```rust
let mut v = Vec::new();

println!("len: {}, cap: {}", v.len(), v.capacity());
// len: 0, cap: 0  — no se ha alocado nada

v.push(1);
println!("len: {}, cap: {}", v.len(), v.capacity());
// len: 1, cap: 4  — primera alocacion (tipicamente 4)

v.push(2);
v.push(3);
v.push(4);
println!("len: {}, cap: {}", v.len(), v.capacity());
// len: 4, cap: 4  — lleno

v.push(5);
println!("len: {}, cap: {}", v.len(), v.capacity());
// len: 5, cap: 8  — realocar: duplico la capacidad
```

```text
Estrategia de crecimiento:
- Vec::new() no aloca hasta el primer push
- Cuando se llena, realoca al doble (aproximadamente)
- La realocacion copia TODOS los elementos al nuevo buffer
- El buffer viejo se libera
- Por eso push es O(1) AMORTIZADO — la mayoria O(1), pero
  ocasionalmente O(n) cuando realoca

Secuencia tipica de capacidad:
  0 → 4 → 8 → 16 → 32 → 64 → 128 → ...
  (la stdlib no garantiza esta secuencia exacta)
```

```rust
// Controlar la capacidad manualmente:

// reserve — asegura que caben al menos n mas
let mut v = vec![1, 2, 3];
v.reserve(100);  // cap >= len + 100 = 103
// Puede alocar MAS de 103 para evitar realocaciones frecuentes

// reserve_exact — intenta alocar exactamente lo necesario
v.reserve_exact(100);  // cap >= 103 (puede ser exacto)

// shrink_to_fit — reduce la capacidad al minimo
let mut v = Vec::with_capacity(1000);
v.push(1);
v.push(2);
v.shrink_to_fit();
// cap probablemente ahora es 2 (o cercano)

// shrink_to — reduce a un minimo especificado
v.shrink_to(10);  // cap >= max(len, 10)
```

```rust
// Por que importa la capacidad — benchmark mental:
fn bad_collect(n: usize) -> Vec<i32> {
    let mut v = Vec::new();  // empieza con cap 0
    for i in 0..n {
        v.push(i as i32);   // realoca multiples veces
    }
    v
    // Para n = 1000: ~10 realocaciones (0→4→8→16→...→1024)
    // Cada realocacion copia todos los elementos existentes
}

fn good_collect(n: usize) -> Vec<i32> {
    let mut v = Vec::with_capacity(n);  // una sola alocacion
    for i in 0..n {
        v.push(i as i32);  // nunca realoca
    }
    v
    // Para n = 1000: 0 realocaciones
}

// Mejor aun — usar collect:
fn best_collect(n: usize) -> Vec<i32> {
    (0..n as i32).collect()
    // collect usa size_hint() del iterador para pre-alocar
}
```

## Layout en memoria

```rust
// Vec<i32> con 3 elementos, capacidad 4:
//
// Stack (24 bytes):
// ┌────────────────────┐
// │ ptr:  0x7f8a...    │  8 bytes — puntero al heap
// │ len:  3            │  8 bytes — elementos actuales
// │ cap:  4            │  8 bytes — capacidad total
// └────────────────────┘
//
// Heap (16 bytes = 4 * size_of::<i32>()):
// ┌─────┬─────┬─────┬─────┐
// │  10 │  20 │  30 │  ?? │
// └─────┴─────┴─────┴─────┘
//  [0]    [1]    [2]   [3] ← espacio libre (no inicializado)

// Los elementos son contiguos — excelente para cache locality
// El acceso por indice es O(1) — aritmetica de punteros:
//   &v[i] == ptr + i * size_of::<T>()
```

```rust
// Vec<String> — cada elemento es un String (tambien ptr+len+cap):
//
// Stack:
// ┌──────────┐
// │ ptr ─────────────┐
// │ len: 2   │       │
// │ cap: 2   │       │
// └──────────┘       │
//                    ▼
// Heap (buffer del Vec):
// ┌──────────────────┬──────────────────┐
// │ String { ptr,    │ String { ptr,    │
// │   len, cap }     │   len, cap }     │
// └────┬─────────────┴────┬─────────────┘
//      │                  │
//      ▼                  ▼
// Heap (buffer del String 1): Heap (buffer del String 2):
// ┌───────────┐          ┌──────────┐
// │ "hello"   │          │ "world"  │
// └───────────┘          └──────────┘
//
// Dos niveles de indireccion para Vec<String>
```

```rust
// Vec vacio — no aloca heap:
let v: Vec<i32> = Vec::new();
// ptr apunta a un valor especial (dangling aligned pointer)
// len = 0, cap = 0
// No hay alocacion de heap — completamente gratis
```

## Sorting

```rust
let mut v = vec![3, 1, 4, 1, 5, 9, 2, 6];

// sort — ordenar (estable), requiere Ord
v.sort();  // [1, 1, 2, 3, 4, 5, 6, 9]

// sort_unstable — mas rapido, no preserva orden de iguales
v.sort_unstable();

// sort_by — ordenar con comparador custom
let mut names = vec!["Charlie", "Alice", "Bob"];
names.sort_by(|a, b| a.len().cmp(&b.len()));
// ["Bob", "Alice", "Charlie"] — por longitud

// sort_by_key — ordenar por una clave extraida
let mut v = vec![(3, "c"), (1, "a"), (2, "b")];
v.sort_by_key(|&(n, _)| n);  // [(1, "a"), (2, "b"), (3, "c")]

// Reverse sort
let mut v = vec![1, 5, 3, 2, 4];
v.sort_by(|a, b| b.cmp(a));  // [5, 4, 3, 2, 1]
// O:
v.sort_unstable();
v.reverse();
```

```rust
// Ordenar floats — f64 no implementa Ord (por NaN)
let mut v = vec![3.1, 1.2, 2.5];

// NO COMPILA:
// v.sort();  // error: f64 doesn't implement Ord

// Solucion: sort_by con partial_cmp + unwrap
v.sort_by(|a, b| a.partial_cmp(b).unwrap());

// Mas robusto — total_cmp (desde Rust 1.62):
v.sort_by(|a, b| a.total_cmp(b));
// total_cmp da un orden total (NaN tiene una posicion definida)
```

## Busqueda

```rust
let v = vec![10, 20, 30, 40, 50];

// contains — busqueda lineal O(n)
assert!(v.contains(&30));
assert!(!v.contains(&99));

// binary_search — busqueda binaria O(log n), requiere vec ordenado
match v.binary_search(&30) {
    Ok(index) => println!("encontrado en indice {index}"),  // 2
    Err(index) => println!("no encontrado, se insertaria en {index}"),
}

// position — indice del primer elemento que cumple condicion
let pos = v.iter().position(|&x| x > 25);  // Some(2) — el 30

// find — primer elemento que cumple condicion
let found = v.iter().find(|&&x| x > 25);  // Some(&30)
```

## Vec y ownership

```rust
// Vec es dueño de sus elementos — los dropea al salir de scope
{
    let v = vec![String::from("a"), String::from("b")];
    // v es dueño de ambos Strings
}
// Aqui se dropean: primero los Strings, luego el buffer del Vec

// Mover elementos fuera del Vec:
let mut v = vec![String::from("hello")];
let s = v.remove(0);  // s es dueño de "hello", v esta vacio
let s = v.pop();       // Option<String> — mueve el ultimo

// swap_remove para mover por indice en O(1):
let mut v = vec![String::from("a"), String::from("b"), String::from("c")];
let b = v.swap_remove(1);  // b = "b", v = ["a", "c"]

// drain — extraer un rango, moviendo ownership
let mut v = vec![1, 2, 3, 4, 5];
let drained: Vec<i32> = v.drain(1..3).collect();  // [2, 3]
// v = [1, 4, 5]

// split_off — partir en dos Vecs
let mut v = vec![1, 2, 3, 4, 5];
let tail = v.split_off(3);  // tail = [4, 5], v = [1, 2, 3]
```

## Vec\<T\> como &[T] — Deref coercion

```rust
// Vec<T> implementa Deref<Target = [T]>
// Esto significa que &Vec<T> se convierte automaticamente a &[T]

fn sum(slice: &[i32]) -> i32 {
    slice.iter().sum()
}

let v = vec![1, 2, 3];
let total = sum(&v);     // &Vec<i32> → &[i32] automaticamente
// No necesitas escribir sum(&v[..]) — la coercion es implicita

// Por eso las funciones deben aceptar &[T], no &Vec<T>:
fn bad(v: &Vec<i32>) -> i32 { v.iter().sum() }  // demasiado restrictivo
fn good(s: &[i32]) -> i32 { s.iter().sum() }    // acepta Vec, array, slice
```

```text
Regla: en parametros de funciones, prefiere &[T] sobre &Vec<T>.
  &[T] acepta: &Vec<T>, &[T; N], &slice
  &Vec<T> solo acepta: &Vec<T>
```

## Errores comunes

```rust
// ERROR 1: modificar un Vec mientras se itera
let mut v = vec![1, 2, 3, 4, 5];
// for val in &v {
//     if *val > 3 {
//         v.push(*val * 2);  // error: cannot borrow as mutable
//     }
// }
// Solucion: usar retain, o recolectar los nuevos y luego extend
let extras: Vec<i32> = v.iter().filter(|&&x| x > 3).map(|&x| x * 2).collect();
v.extend(extras);

// ERROR 2: invalidar referencias por realocacion
let mut v = vec![1, 2, 3];
let first = &v[0];   // referencia al primer elemento
// v.push(4);         // error: push puede realocar, invalidando first
// println!("{first}");
// El borrow checker lo impide. En C, esto seria un use-after-free.

// ERROR 3: indexar con valor que puede estar fuera de rango
fn get_element(v: &[i32], index: usize) -> i32 {
    v[index]  // panic si index >= v.len()
}
// Solucion: usar .get()
fn get_element_safe(v: &[i32], index: usize) -> Option<&i32> {
    v.get(index)
}

// ERROR 4: clonar un Vec cuando solo necesitas una referencia
fn process(data: Vec<i32>) { /* ... */ }
let v = vec![1, 2, 3];
process(v.clone());  // copia innecesaria si process solo lee
// Solucion:
fn process_better(data: &[i32]) { /* ... */ }
process_better(&v);  // sin copia

// ERROR 5: collect sin type annotation
let v = (1..5).collect();
// error: type annotations needed
// Solucion:
let v: Vec<i32> = (1..5).collect();
// O con turbofish:
let v = (1..5).collect::<Vec<i32>>();
```

## Cheatsheet

```text
Crear:
  Vec::new()               vacio, sin alocar
  vec![1, 2, 3]            con elementos
  vec![0; n]               n copias de un valor
  Vec::with_capacity(n)    pre-alocar espacio
  iter.collect()           desde iterador

Agregar/quitar:
  push(val)                al final, O(1) amort.
  pop()                    del final, O(1)
  insert(i, val)           en posicion, O(n)
  remove(i)                de posicion, O(n)
  swap_remove(i)           rapido pero desordenado, O(1)
  extend(iter)             agregar multiples
  drain(range)             extraer rango
  retain(|x| cond)         filtrar in-place
  clear()                  vaciar (sin desalocar)

Acceso:
  v[i]                     por indice (panic)
  v.get(i)                 por indice (Option)
  v.first() / v.last()     primero / ultimo
  &v[a..b]                 slice

Iterar:
  for x in &v              prestamo (&T)
  for x in &mut v          mutable (&mut T)
  for x in v               consume (T)

Capacidad:
  v.len()                  elementos actuales
  v.capacity()             espacio alocado
  v.is_empty()             len == 0
  v.reserve(n)             asegurar n mas
  v.shrink_to_fit()        reducir al minimo

Ordenar/buscar:
  v.sort()                 estable, Ord
  v.sort_unstable()        mas rapido
  v.sort_by(|a,b| ...)     comparador
  v.contains(&val)         lineal
  v.binary_search(&val)    binaria (ord.)
```

---

## Ejercicios

### Ejercicio 1 — Capacidad y realocaciones

```rust
// Crea un Vec vacio y haz push de 20 elementos (1..=20).
// Despues de cada push, imprime len y capacity.
//
// Predice ANTES de ejecutar:
// - ¿En que pushes cambia la capacidad?
// - ¿Cuantas realocaciones hay en total?
// - ¿Cual es la capacidad final?
//
// Luego repite con Vec::with_capacity(20) — ¿cuantas realocaciones?
```

### Ejercicio 2 — Operaciones sobre Vec

```rust
// Dado: let mut v = vec![5, 3, 8, 1, 9, 2, 7, 4, 6];
//
// Sin usar variables auxiliares (encadena operaciones):
// 1. Elimina todos los numeros pares (retain)
// 2. Ordena de mayor a menor
// 3. Toma los primeros 3 elementos (truncate)
//
// Predice el resultado antes de ejecutar.
// ¿Cual es el Vec final?
```

### Ejercicio 3 — Vec como stack

```rust
// Implementa una calculadora de notacion polaca inversa (RPN):
//   "3 4 + 2 *" → (3 + 4) * 2 = 14
//
// Usa un Vec<f64> como stack:
//   - Numeros → push al stack
//   - Operadores (+, -, *, /) → pop dos operandos, push resultado
//
// fn rpn(expr: &str) -> Result<f64, String>
//
// Prueba con:
//   "3 4 +"       → 7.0
//   "5 1 2 + 4 * + 3 -" → 14.0
//   "4 0 /"       → error (division por cero) o Infinity
//   "+"           → error (stack underflow)
```
