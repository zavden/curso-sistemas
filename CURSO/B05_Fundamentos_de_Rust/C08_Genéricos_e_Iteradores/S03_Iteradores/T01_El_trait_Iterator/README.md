# T01 — El trait Iterator

## Que es un iterador

Un iterador es un objeto que produce una secuencia de valores,
uno a la vez. En Rust, todo iterador implementa el trait `Iterator`:

```rust
// Definicion simplificada del trait (stdlib):
pub trait Iterator {
    type Item;  // tipo asociado — que produce el iterador

    fn next(&mut self) -> Option<Self::Item>;
    // Retorna Some(valor) si hay mas, None si termino.

    // ... mas de 70 metodos con implementacion default
    // (map, filter, collect, sum, count, etc.)
}
```

```rust
// Ejemplo basico: iterar sobre un Vec
let v = vec![10, 20, 30];
let mut iter = v.iter();  // crea un iterador

assert_eq!(iter.next(), Some(&10));  // primer elemento
assert_eq!(iter.next(), Some(&20));  // segundo
assert_eq!(iter.next(), Some(&30));  // tercero
assert_eq!(iter.next(), None);       // fin
assert_eq!(iter.next(), None);       // sigue dando None
```

```text
El protocolo es simple:
1. Llama next()
2. Si retorna Some(valor) → hay un elemento, puedes seguir
3. Si retorna None → se acabaron los elementos
4. Despues de None, next() sigue retornando None (fused)
```

## Item — el tipo asociado

`Item` define que tipo de valor produce el iterador. No es
un parametro generico sino un tipo asociado — cada iterador
tiene exactamente un tipo de Item:

```rust
let v = vec![1, 2, 3];

// .iter() → Iterator<Item = &i32>
let mut iter = v.iter();
let x: Option<&i32> = iter.next();  // referencias

// .iter_mut() → Iterator<Item = &mut i32>
let mut v = vec![1, 2, 3];
let mut iter = v.iter_mut();
let x: Option<&mut i32> = iter.next();  // referencias mutables

// .into_iter() → Iterator<Item = i32>
let v = vec![1, 2, 3];
let mut iter = v.into_iter();
let x: Option<i32> = iter.next();  // valores (consume el Vec)
```

```text
Los tres iteradores de Vec:

Metodo        Item      Ownership    Vec despues
──────────────────────────────────────────────────
.iter()       &T        Prestamo     Sigue vivo
.iter_mut()   &mut T    Prestamo mut Sigue vivo (mutado)
.into_iter()  T         Consume      Ya no existe
```

```rust
// Item puede ser cualquier tipo:
let s = "hello";
let mut chars = s.chars();  // Iterator<Item = char>
assert_eq!(chars.next(), Some('h'));

let text = "linea 1\nlinea 2\nlinea 3";
let mut lines = text.lines();  // Iterator<Item = &str>
assert_eq!(lines.next(), Some("linea 1"));

use std::collections::HashMap;
let map = HashMap::from([("a", 1), ("b", 2)]);
let mut iter = map.iter();  // Iterator<Item = (&str, &i32)>
// Item es una tupla
```

## for desugaring

El `for` loop es azucar sintactico para llamar `.into_iter()`
y luego `next()` repetidamente:

```rust
let v = vec![1, 2, 3];

// Esto:
for x in v {
    println!("{x}");
}

// Es equivalente a:
let v = vec![1, 2, 3];
let mut iter = v.into_iter();
loop {
    match iter.next() {
        Some(x) => println!("{x}"),
        None => break,
    }
}
```

```rust
// for in &v usa .iter() (prestamo):
let v = vec![1, 2, 3];
for x in &v {
    // x es &i32
    println!("{x}");
}
println!("{v:?}");  // v sigue viva

// for in &mut v usa .iter_mut() (prestamo mutable):
let mut v = vec![1, 2, 3];
for x in &mut v {
    *x *= 2;
}
assert_eq!(v, vec![2, 4, 6]);

// for in v usa .into_iter() (consume):
let v = vec![1, 2, 3];
for x in v {
    // x es i32
    println!("{x}");
}
// v ya no existe
```

## IntoIterator — el trait detras de for

Cualquier tipo que implemente `IntoIterator` puede usarse en
un `for` loop:

```rust
// Definicion simplificada:
pub trait IntoIterator {
    type Item;
    type IntoIter: Iterator<Item = Self::Item>;

    fn into_iter(self) -> Self::IntoIter;
}

// Todo Iterator implementa IntoIterator automaticamente
// (retornandose a si mismo).

// Vec implementa IntoIterator de tres formas:
// impl IntoIterator for Vec<T>      → into_iter() consume, Item = T
// impl IntoIterator for &Vec<T>     → into_iter() presta, Item = &T
// impl IntoIterator for &mut Vec<T> → into_iter() presta mut, Item = &mut T
```

```rust
// Tipos que implementan IntoIterator:
// Vec<T>, &[T], arrays [T; N], String (chars), &str (chars)
// HashMap, BTreeMap, HashSet, BTreeSet
// Range (1..10), Option<T>, Result<T, E>

// Option como iterador (0 o 1 elementos):
for x in Some(42) {
    println!("{x}");  // imprime 42
}
for x in None::<i32> {
    println!("{x}");  // no imprime nada
}
```

## Laziness — los iteradores son perezosos

Los iteradores en Rust son **lazy**: no hacen nada hasta que
los consumes. Crear un iterador y encadenar adaptadores no
ejecuta ningun codigo:

```rust
let v = vec![1, 2, 3, 4, 5];

// Esto NO ejecuta nada — solo construye una cadena de iteradores:
let iter = v.iter()
    .map(|x| {
        println!("mapeando {x}");  // NUNCA se imprime
        x * 2
    })
    .filter(|x| {
        println!("filtrando {x}");  // NUNCA se imprime
        x > &4
    });

// warning: unused `Filter` that must be used
// = note: iterators are lazy and do nothing unless consumed

// Recien al consumir (collect, for, sum, etc.) se ejecuta:
let result: Vec<i32> = iter.collect();
// Ahora SI se imprime:
// mapeando 1
// filtrando 2
// mapeando 2
// filtrando 4
// mapeando 3
// filtrando 6     ← este pasa el filtro
// mapeando 4
// filtrando 8     ← este pasa
// mapeando 5
// filtrando 10    ← este pasa
```

```text
¿Por que lazy?

1. Eficiencia: no se procesan elementos que no se necesitan.
   Con take(3), solo se procesan 3 elementos aunque el Vec tenga 1000.

2. Composicion: puedes encadenar operaciones sin crear
   colecciones intermedias. map().filter().take() es un solo
   recorrido, no tres Vec intermedios.

3. Iteradores infinitos: puedes representar secuencias sin fin
   y solo tomar lo que necesitas.
```

```rust
// Demostrar laziness con take:
let result: Vec<i32> = (1..)  // iterador INFINITO: 1, 2, 3, 4, ...
    .map(|x| x * x)           // 1, 4, 9, 16, 25, ...
    .filter(|&x| x % 2 == 0)  // 4, 16, 36, 64, ...
    .take(3)                   // solo los primeros 3
    .collect();
assert_eq!(result, vec![4, 16, 36]);
// Sin laziness, map sobre un iterador infinito nunca terminaria.
```

## Procesamiento elemento por elemento

Los iteradores procesan **un elemento a la vez** a traves de
toda la cadena, no una operacion a la vez sobre todos:

```rust
// NO es: primero map todo, luego filter todo
// SI es: cada elemento pasa por map+filter antes del siguiente

let v = vec![1, 2, 3];
let result: Vec<i32> = v.iter()
    .map(|x| {
        println!("  map({x})");
        x * 10
    })
    .filter(|x| {
        println!("  filter({x})");
        x > &15
    })
    .collect();

// Salida:
//   map(1)
//   filter(10)     ← 10 <= 15, descartado
//   map(2)
//   filter(20)     ← 20 > 15, pasa
//   map(3)
//   filter(30)     ← 30 > 15, pasa

// Cada elemento recorre toda la cadena antes del siguiente.
// Esto es mas eficiente que crear Vec intermedios.
```

## Crear iteradores

```rust
// Desde colecciones:
let v = vec![1, 2, 3];
let iter = v.iter();           // &i32
let iter = v.into_iter();     // i32 (consume v)

// Desde rangos:
let iter = 0..10;              // 0, 1, 2, ..., 9
let iter = 0..=10;             // 0, 1, 2, ..., 10
let iter = 'a'..='z';         // a, b, c, ..., z

// Funciones constructoras:
let iter = std::iter::empty::<i32>();     // iterador vacio
let iter = std::iter::once(42);           // un solo elemento
let iter = std::iter::repeat(0);          // infinito: 0, 0, 0, ...
let iter = std::iter::repeat_with(|| rand()); // infinito con closure

// successors — secuencia donde cada elemento depende del anterior:
let powers = std::iter::successors(Some(1_u64), |&prev| {
    prev.checked_mul(2)  // None cuando hay overflow → para
});
let v: Vec<u64> = powers.take(10).collect();
// [1, 2, 4, 8, 16, 32, 64, 128, 256, 512]

// from_fn — generador con closure:
let mut count = 0;
let counter = std::iter::from_fn(move || {
    count += 1;
    if count <= 5 { Some(count) } else { None }
});
let v: Vec<i32> = counter.collect();  // [1, 2, 3, 4, 5]
```

## Errores comunes

```rust
// ERROR 1: olvidar consumir el iterador
let v = vec![1, 2, 3];
v.iter().map(|x| x * 2);
// warning: unused `Map` — no hace nada sin collect/for/etc.
// Solucion: let result: Vec<_> = v.iter().map(|x| x * 2).collect();

// ERROR 2: usar el iterador despues de consumirlo
let v = vec![1, 2, 3];
let iter = v.into_iter();
let sum: i32 = iter.sum();     // consume iter
// let count = iter.count();   // error: iter ya fue consumido

// ERROR 3: confundir iter() con into_iter()
let v = vec![String::from("a"), String::from("b")];
for s in v.iter() {
    // s es &String — no puedes moverlo
}
for s in v {
    // s es String — v fue consumido
}

// ERROR 4: next() requiere &mut self
let v = vec![1, 2, 3];
let iter = v.iter();
// iter.next();  // error: iter no es mut
let mut iter = v.iter();  // mut necesario para llamar next()
iter.next();  // OK

// ERROR 5: iterar y modificar a la vez
let mut v = vec![1, 2, 3];
// for x in &v {
//     v.push(*x);  // error: borrow compartido + mutable
// }
// Solucion: recolectar primero, luego modificar
let new_items: Vec<i32> = v.iter().map(|&x| x * 2).collect();
v.extend(new_items);
```

## Cheatsheet

```text
Trait Iterator:
  type Item                    tipo que produce
  fn next(&mut self) -> Option<Item>  siguiente elemento

Crear iteradores:
  v.iter()                     &T (prestamo)
  v.iter_mut()                 &mut T (prestamo mutable)
  v.into_iter()                T (consume)
  0..10                        rango exclusivo
  0..=10                       rango inclusivo
  iter::once(val)              un elemento
  iter::repeat(val)            infinito
  iter::empty()                vacio
  iter::from_fn(|| ...)        generador

for desugaring:
  for x in v       →  v.into_iter() + loop { next() }
  for x in &v      →  (&v).into_iter() = v.iter()
  for x in &mut v  →  (&mut v).into_iter() = v.iter_mut()

Laziness:
  - Adapters (map, filter, take) no ejecutan nada
  - Consumers (collect, sum, for_each, count) disparan la ejecucion
  - Procesamiento elemento por elemento (no paso por paso)
  - Permite iteradores infinitos con take/take_while
```

---

## Ejercicios

### Ejercicio 1 — next() manual

```rust
// Crea un iterador sobre [10, 20, 30, 40] y llama next()
// manualmente hasta que retorne None.
//
// Predice cada valor de retorno ANTES de ejecutar:
//   next() → ???
//   next() → ???
//   next() → ???
//   next() → ???
//   next() → ???
//
// ¿Cuantas veces puedes llamar next() antes del primer None?
// ¿Que retorna si llamas next() una vez mas despues de None?
```

### Ejercicio 2 — Laziness

```rust
// Sin ejecutar, predice la salida de este codigo:
let v = vec![1, 2, 3, 4, 5];
let result: Vec<i32> = v.iter()
    .map(|x| { println!("map: {x}"); x * 2 })
    .filter(|x| { println!("filter: {x}"); *x > 6 })
    .take(1)
    .collect();

// Preguntas:
// a) ¿Cuantas veces se imprime "map: ..."?
// b) ¿Cuantas veces se imprime "filter: ..."?
// c) ¿Cual es el valor de result?
// d) ¿Se procesa el elemento 5? ¿Por que si/no?
//
// Ejecuta y verifica.
```

### Ejercicio 3 — iter vs into_iter

```rust
// Dado: let words = vec!["hello".to_string(), "world".to_string()];
//
// Escribe tres loops:
// a) Con &words — imprime cada palabra, words sigue disponible despues
// b) Con &mut words — convierte cada palabra a mayusculas in-place
// c) Con words (sin &) — mueve cada palabra a un nuevo Vec<String> en reversa
//
// Para cada uno, predice: ¿el tipo de la variable del loop?
//                         ¿words sigue disponible despues?
```
