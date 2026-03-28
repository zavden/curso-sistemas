# T03 — Iteradores

## Que es un iterador

Un iterador es un objeto que produce una secuencia de valores,
uno a la vez. En Rust, todos los iteradores implementan el trait
`Iterator` de la biblioteca estandar:

```rust
// Definicion simplificada del trait Iterator:
trait Iterator {
    type Item;                              // tipo de cada elemento
    fn next(&mut self) -> Option<Self::Item>; // produce el siguiente, o None
}

// type Item — el tipo asociado que define que produce el iterador.
// Para un iterador sobre Vec<i32>, Item es i32 (o &i32, o &mut i32,
// dependiendo de como se creo el iterador).
//
// next() — avanza el iterador y devuelve:
//   Some(valor) si hay un elemento disponible
//   None        si la secuencia termino
//
// &mut self — el iterador necesita mutar su estado interno
// (lleva la cuenta de en que posicion esta).
```

```rust
// Los iteradores son lazy (perezosos):
// No hacen nada hasta que alguien llama next().
// Producen valores bajo demanda, uno por uno.

let v = vec![10, 20, 30];
let iter = v.iter();    // no se ha iterado nada todavia
// Hasta que alguien llame next(), el iterador no avanza.
```

## Tres formas de iterar una coleccion

Rust ofrece tres metodos para crear un iterador a partir de
una coleccion. La diferencia es **que tipo de acceso** dan
sobre cada elemento:

```rust
// .iter() — prestamo inmutable: &T
// La coleccion NO se consume. Se puede usar despues del loop.

let names = vec![String::from("Alice"), String::from("Bob")];

for name in names.iter() {
    println!("{}", name);   // name es &String
}

println!("Total: {}", names.len());  // OK — names sigue valido
```

```rust
// .iter_mut() — prestamo mutable: &mut T
// La coleccion NO se consume, pero se pueden modificar los elementos.

let mut scores = vec![80, 90, 75];

for score in scores.iter_mut() {
    *score += 5;  // score es &mut i32, dereferencia para modificar
}

println!("{:?}", scores);  // [85, 95, 80]
```

```rust
// .into_iter() — toma ownership: T
// La coleccion SE CONSUME. No se puede usar despues.

let words = vec![String::from("hello"), String::from("world")];

for word in words.into_iter() {
    println!("{}", word);   // word es String (no &String)
}

// words ya no existe. Esta linea no compila:
// println!("{:?}", words);
// error[E0382]: borrow of moved value: `words`
```

```rust
// Resumen comparativo:
//
// Metodo        Tipo de item    Coleccion despues
// ──────────    ─────────────   ─────────────────
// .iter()       &T              intacta (prestamo inmutable)
// .iter_mut()   &mut T          intacta (prestamo mutable)
// .into_iter()  T               consumida (ya no existe)
//
// Regla practica:
//   - Solo necesitas leer?          → .iter()
//   - Necesitas modificar?          → .iter_mut()
//   - Ya no necesitas la coleccion? → .into_iter()
```

## for y el trait IntoIterator

El bucle `for` de Rust no trabaja directamente con `Iterator`.
Trabaja con `IntoIterator`, un trait que convierte algo en un
iterador:

```rust
// Cuando escribes:
for x in collection {
    // ...
}

// El compilador lo transforma en:
// let mut iter = collection.into_iter();
// while let Some(x) = iter.next() {
//     // ...
// }
//
// Es decir, for llama .into_iter() sobre lo que le pases.
```

```rust
// Lo que pasa depende de si pasas la coleccion directamente,
// una referencia, o una referencia mutable:
//
// for x in v         → llama v.into_iter()          → x es i32 (consume v)
// for x in &v        → llama (&v).into_iter()       → x es &i32
// for x in &mut v    → llama (&mut v).into_iter()   → x es &mut i32
//
// Esto funciona porque la biblioteca estandar implementa:
//
//   impl<'a, T> IntoIterator for &'a Vec<T> {
//       type Item = &'a T;
//       fn into_iter(self) -> slice::Iter<'a, T> { self.iter() }
//   }
//
// Es decir, IntoIterator para &Vec<T> delega a .iter().
// Y IntoIterator para &mut Vec<T> delega a .iter_mut().
```

```rust
// Ejemplo practico — las tres formas con for:

let mut data = vec![10, 20, 30];

// Leer sin consumir (equivale a data.iter()):
for val in &data {
    println!("Valor: {}", val);   // val es &i32
}

// Modificar sin consumir (equivale a data.iter_mut()):
for val in &mut data {
    *val *= 2;                     // val es &mut i32
}
// data ahora es [20, 40, 60]

// Consumir (equivale a data.into_iter()):
for val in data {
    println!("Consumido: {}", val); // val es i32
}
// data ya no existe
```

## Iteracion manual con next()

Se puede usar un iterador directamente llamando `next()`,
sin el azucar sintactico del bucle `for`:

```rust
let v = vec![10, 20, 30];
let mut iter = v.iter();   // mut porque next() toma &mut self

assert_eq!(iter.next(), Some(&10));
assert_eq!(iter.next(), Some(&20));
assert_eq!(iter.next(), Some(&30));
assert_eq!(iter.next(), None);       // secuencia agotada
assert_eq!(iter.next(), None);       // sigue devolviendo None
```

```rust
// Equivalente manual de un for loop usando while let:

let v = vec!["alpha", "beta", "gamma"];
let mut iter = v.iter();

while let Some(item) = iter.next() {
    println!("{}", item);
}
// Produce exactamente la misma salida que: for item in &v { ... }
```

```rust
// Cuando usar next() manualmente en vez de for:

// 1. Procesar el primer elemento de forma distinta:
let mut args = std::env::args();
let program_name = args.next().unwrap();  // primer argumento
for arg in args {
    println!("Argumento: {}", arg);       // resto de argumentos
}

// 2. Avanzar condicionalmente (saltar elementos):
let data = vec![1, 2, 0, 3, 4];
let mut iter = data.iter();
while let Some(&val) = iter.next() {
    if val == 0 {
        let _ = iter.next();  // saltar el siguiente despues del cero
        continue;
    }
    println!("{}", val);
}
// Imprime: 1, 2, 4 (el 3 se salto porque seguia al 0)
```

## Rangos como iteradores

Los rangos de Rust implementan `Iterator` directamente.
No son azucar sintactico ni un caso especial del for:

```rust
// Rango exclusivo: start..end (no incluye end)
for i in 0..5 {
    print!("{} ", i);
}
// 0 1 2 3 4

// Rango inclusivo: start..=end (incluye end)
for i in 1..=5 {
    print!("{} ", i);
}
// 1 2 3 4 5
```

```rust
// Funcionan con cualquier tipo que implemente Step: enteros y char.

for c in 'a'..='f' {
    print!("{} ", c);
}
// a b c d e f

// Como son iteradores, se pueden usar con metodos de Iterator:
let sum: i32 = (1..=100).sum();
println!("Suma 1 a 100: {}", sum);  // 5050
```

```rust
// Rangos vacios: si start >= end, no produce valores.
for i in 5..5 {
    println!("Nunca se ejecuta: {}", i);
}

// Rangos descendentes no funcionan directamente:
for i in 10..0 {
    println!("Nunca se ejecuta: {}", i);
}

// Para iterar en reversa se usa .rev():
for i in (0..10).rev() {
    print!("{} ", i);
}
// 9 8 7 6 5 4 3 2 1 0
```

## Metodos comunes de Iterator (introduccion)

El trait `Iterator` provee decenas de metodos. Aqui se
presentan los mas usados en combinacion con bucles.
La cobertura completa de adaptadores y consumidores
esta en C08 (Genericos e Iteradores).

```rust
// .enumerate() — agrega un indice a cada elemento.

let fruits = vec!["apple", "banana", "cherry"];

for (i, fruit) in fruits.iter().enumerate() {
    println!("{}: {}", i, fruit);
}
// 0: apple
// 1: banana
// 2: cherry
```

```rust
// .zip() — combina dos iteradores en uno de tuplas.
// Se detiene cuando cualquiera de los dos se agota.

let names = vec!["Alice", "Bob", "Carol"];
let ages = vec![30, 25, 35];

for (name, age) in names.iter().zip(ages.iter()) {
    println!("{} tiene {} anios", name, age);
}
```

```rust
// .map() — transforma cada elemento (lazy, no hace nada hasta que se consume).

let numbers = vec![1, 2, 3, 4];

for n in numbers.iter().map(|x| x * 2) {
    print!("{} ", n);
}
// 2 4 6 8

// O recolectar en un Vec con .collect():
let doubled: Vec<i32> = numbers.iter().map(|x| x * 2).collect();
println!("{:?}", doubled);  // [2, 4, 6, 8]
```

```rust
// .filter() — conserva solo los elementos que cumplen una condicion.

let values = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

for v in values.iter().filter(|&&x| x > 5) {
    print!("{} ", v);
}
// 6 7 8 9 10

// Nota sobre &&x: values.iter() produce &i32.
// filter recibe &(&i32), es decir &&i32.
// El patron &&x desenvuelve ambas referencias.
```

```rust
// .collect() — consume un iterador y construye una coleccion.
// Necesita saber el tipo de destino (anotacion de tipo o turbofish).

let data = vec![1, 2, 3, 4, 5];

let evens: Vec<&i32> = data.iter().filter(|&&x| x % 2 == 0).collect();
let evens = data.iter().filter(|&&x| x % 2 == 0).collect::<Vec<_>>();
// Ambas formas producen [2, 4]
```

```rust
// Encadenamiento — los metodos de iterador se componen:

let words = vec!["hello", "world", "foo", "bar", "baz"];

let result: Vec<String> = words
    .iter()
    .filter(|w| w.len() > 3)
    .map(|w| w.to_uppercase())
    .enumerate()
    .map(|(i, w)| format!("{}: {}", i, w))
    .collect();

println!("{:?}", result);  // ["0: HELLO", "1: WORLD"]
```

## Iteradores vs indexacion

En Rust, iterar con `for x in &v` es idiomatico. Acceder
por indice con `v[i]` generalmente no lo es:

```rust
let data = vec![10, 20, 30, 40, 50];

// Idiomatico — iterador:
for val in &data {
    println!("{}", val);
}

// No idiomatico — indexacion:
for i in 0..data.len() {
    println!("{}", data[i]);
}
```

```rust
// Razon 1: seguridad.
// data[i] hace bounds checking en cada acceso. Si i >= data.len(),
// el programa entra en panic. Los iteradores no pueden salirse de rango:
// next() simplemente devuelve None.

// Razon 2: optimizacion.
// Con iteradores, el compilador puede demostrar que todos los accesos
// estan dentro de limites y ELIMINAR los bounds checks. Con data[i],
// a veces no puede probar que i < data.len() y mantiene el check.
// Esto afecta la capacidad de vectorizar (SIMD) loops de alto rendimiento.

// Razon 3: claridad de intencion.
// for val in &data dice "quiero leer cada elemento".
// Si necesitas el indice, usa enumerate():
for (i, val) in data.iter().enumerate() {
    println!("Posicion {}: valor {}", i, val);
}
```

```rust
// Razon 4: generalidad.
// Los iteradores funcionan con cualquier coleccion que implemente
// IntoIterator: Vec, HashMap, BTreeSet, archivos, etc.
// La indexacion solo funciona con colecciones de acceso posicional.

use std::collections::HashMap;
let mut map = HashMap::new();
map.insert("a", 1);
map.insert("b", 2);

for (key, val) in &map {
    println!("{}: {}", key, val);   // funciona
}
// map[0] no tiene sentido — HashMap no tiene indices numericos.
```

## Implicaciones de ownership

La eleccion entre `iter()`, `iter_mut()` e `into_iter()`
tiene consecuencias directas sobre el ownership de la coleccion:

```rust
// Caso 1: necesitas la coleccion despues del loop → .iter() o &

fn print_all(items: &Vec<String>) {
    for item in items {
        println!("{}", item);   // item es &String
    }
}

let names = vec![String::from("Alice"), String::from("Bob")];
print_all(&names);
println!("Todavia tengo {} nombres", names.len());  // OK
```

```rust
// Caso 2: necesitas modificar elementos → .iter_mut() o &mut

fn double_all(values: &mut Vec<i32>) {
    for val in values.iter_mut() {
        *val *= 2;
    }
}

let mut nums = vec![1, 2, 3];
double_all(&mut nums);
println!("{:?}", nums);  // [2, 4, 6]
```

```rust
// Caso 3: no necesitas la coleccion despues → .into_iter()

fn consume_and_count(items: Vec<String>) -> usize {
    let mut total_len = 0;
    for item in items {
        total_len += item.len();  // item es String (ownership transferido)
    }
    total_len
}

let words = vec![String::from("hello"), String::from("world")];
let len = consume_and_count(words);
// println!("{:?}", words);  // no compila — words fue movido
```

```rust
// Caso 4: into_iter con tipos Copy.
// Para tipos que implementan Copy (i32, f64, bool, char, etc.),
// los valores individuales se copian, pero el Vec se consume igual.

let numbers = vec![1, 2, 3];
for n in numbers.into_iter() {
    println!("{}", n);   // n es i32
}
// numbers fue consumido. Si lo necesitas despues, usa &numbers.
```

```rust
// Errores comunes de ownership con iteradores:

// Error 1: usar la coleccion despues de into_iter
let v = vec![1, 2, 3];
for x in v {
    println!("{}", x);
}
// println!("{:?}", v);  // error: value used after move
// Solucion: usar &v si necesitas v despues.

// Error 2: intentar modificar con iter() en vez de iter_mut()
let mut v = vec![1, 2, 3];
for x in v.iter() {
    // *x += 1;  // error: cannot assign to `*x`, behind a `&` reference
}
// Solucion: usar iter_mut()
for x in v.iter_mut() {
    *x += 1;  // OK
}
```

---

## Ejercicios

### Ejercicio 1 — iter, iter_mut, into_iter

```rust
// Crear un Vec<String> con 4 ciudades.
//
// 1. Usar .iter() para imprimir cada ciudad con su longitud en caracteres.
//    Verificar que el Vec sigue disponible despues del loop.
//
// 2. Usar .iter_mut() para convertir cada ciudad a mayusculas in-place
//    (pista: *city = city.to_uppercase()).
//    Verificar que el Vec ahora contiene las ciudades en mayusculas.
//
// 3. Usar .into_iter() para mover cada ciudad a un nuevo Vec que solo
//    contenga ciudades con mas de 5 caracteres.
//    Verificar que el Vec original ya no es accesible (si descomentas
//    un println! del Vec original, debe dar error de compilacion).
```

### Ejercicio 2 — for con referencias

```rust
// Dado:
// let mut values = vec![10, 20, 30, 40, 50];
//
// Sin usar .iter(), .iter_mut() ni .into_iter() explicitamente
// (usar solo la sintaxis de for con &, &mut, o sin prefijo):
//
// 1. Imprimir cada valor (for v in &values)
// 2. Multiplicar cada valor por 3 (for v in &mut values)
// 3. Calcular la suma moviendo los valores (for v in values)
//
// Para cada caso, anotar en un comentario que tipo tiene
// la variable del loop (v) y si values es usable despues.
```

### Ejercicio 3 — Iteracion manual vs for

```rust
// Dado un Vec<i32> con los valores [1, 0, 2, 0, 0, 3, 4, 0, 5]:
//
// Escribir un loop con while let y next() que:
//   - Imprima cada valor distinto de cero
//   - Cuando encuentre un cero, salte (skip) el siguiente elemento
//     llamando next() una vez extra
//
// Resultado esperado: 1, 2, 3, 5
// (Verificar la logica con papel antes de escribir el codigo.)
//
// Pista: usar iter.next() dentro del cuerpo del while let para
// "consumir" el elemento que se quiere saltar.
```

### Ejercicio 4 — Rangos e iteradores combinados

```rust
// 1. Usar un rango para calcular la suma de los numeros del 1 al 50
//    que sean divisibles por 3 o por 5.
//    (Pista: (1..=50).filter(...).sum())
//
// 2. Usar .enumerate() y un rango de caracteres ('a'..='z') para
//    imprimir cada letra con su posicion numerica:
//    "0: a", "1: b", ..., "25: z"
//
// 3. Usar .zip() para combinar (0..5) con ['a', 'b', 'c', 'd', 'e']
//    e imprimir "0-a", "1-b", etc.
//
// 4. Iterar de 10 a 0 (descendente) usando .rev().
//    Imprimir una cuenta regresiva.
```
