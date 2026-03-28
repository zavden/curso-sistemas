# T02 — Adaptadores

## Que son los adaptadores

Un adaptador es un metodo de `Iterator` que toma un iterador y
retorna **otro iterador** transformado. No consume elementos por
si mismo — solo construye una nueva capa de procesamiento lazy:

```rust
let v = vec![1, 2, 3, 4, 5];

// .map() y .filter() son adaptadores — retornan iteradores nuevos:
let iter = v.iter()
    .map(|x| x * 2)      // retorna Map<Iter<i32>, closure>
    .filter(|x| *x > 4); // retorna Filter<Map<...>, closure>

// Nada se ha ejecutado todavia.
// iter es un iterador compuesto, esperando ser consumido.
let result: Vec<i32> = iter.collect();  // AHORA se ejecuta
assert_eq!(result, vec![6, 8, 10]);
```

```text
Adaptador vs consumidor:

Adaptador:   Iterator → Iterator   (transforma, no ejecuta)
Consumidor:  Iterator → valor      (ejecuta toda la cadena)

Los adaptadores son lazy — construyen un pipeline.
Los consumidores disparan la ejecucion del pipeline.
```

## map — transformar cada elemento

`map` aplica una closure a cada elemento y produce los valores
transformados. Es el adaptador mas usado:

```rust
// Firma simplificada:
// fn map<B, F: FnMut(Self::Item) -> B>(self, f: F) -> Map<Self, F>

let nums = vec![1, 2, 3, 4];
let doubled: Vec<i32> = nums.iter().map(|x| x * 2).collect();
assert_eq!(doubled, vec![2, 4, 6, 8]);
```

```rust
// map cambia el tipo de Item:
let words = vec!["hello", "world"];
let lengths: Vec<usize> = words.iter().map(|w| w.len()).collect();
assert_eq!(lengths, vec![5, 5]);

// De &str a String:
let owned: Vec<String> = words.iter().map(|w| w.to_uppercase()).collect();
assert_eq!(owned, vec!["HELLO", "WORLD"]);
```

```rust
// map con structs:
struct User { name: String, age: u32 }

let users = vec![
    User { name: "Alice".into(), age: 30 },
    User { name: "Bob".into(), age: 25 },
];

let names: Vec<&str> = users.iter().map(|u| u.name.as_str()).collect();
assert_eq!(names, vec!["Alice", "Bob"]);

let ages: Vec<u32> = users.iter().map(|u| u.age).collect();
assert_eq!(ages, vec![30, 25]);
```

```rust
// map con indice — si necesitas el indice, usa enumerate + map:
let v = vec!["a", "b", "c"];
let indexed: Vec<String> = v.iter()
    .enumerate()
    .map(|(i, s)| format!("{i}: {s}"))
    .collect();
assert_eq!(indexed, vec!["0: a", "1: b", "2: c"]);
```

## filter — seleccionar elementos

`filter` retiene solo los elementos para los que la closure
retorna `true`. La closure recibe una **referencia** al elemento:

```rust
// Firma simplificada:
// fn filter<P: FnMut(&Self::Item) -> bool>(self, predicate: P) -> Filter<Self, P>
//                     ^ referencia al Item

let nums = vec![1, 2, 3, 4, 5, 6];
let evens: Vec<&i32> = nums.iter().filter(|x| **x % 2 == 0).collect();
assert_eq!(evens, vec![&2, &4, &6]);
//                 ^^^ iter() produce &i32, filter recibe &&i32
```

```rust
// La doble referencia con iter() + filter:
// iter() produce &i32
// filter recibe una referencia al Item → &(&i32) = &&i32
// Por eso necesitas ** para llegar al valor

// Alternativa 1: destructurar en el patron:
let evens: Vec<&i32> = nums.iter().filter(|&&x| x % 2 == 0).collect();

// Alternativa 2: usar into_iter() para evitar la doble referencia:
let nums = vec![1, 2, 3, 4, 5, 6];
let evens: Vec<i32> = nums.into_iter().filter(|x| x % 2 == 0).collect();
assert_eq!(evens, vec![2, 4, 6]);
// Nota: nums fue consumido por into_iter()
```

```rust
// filter con strings:
let words = vec!["hello", "hi", "world", "wow", "rust"];
let long: Vec<&&str> = words.iter().filter(|w| w.len() > 3).collect();
assert_eq!(long, vec![&"hello", &"world", &"rust"]);

// Mas limpio con into_iter:
let words = vec!["hello", "hi", "world", "wow", "rust"];
let long: Vec<&str> = words.into_iter().filter(|w| w.len() > 3).collect();
assert_eq!(long, vec!["hello", "world", "rust"]);
```

```rust
// filter_map — combina filter + map en un solo paso:
// La closure retorna Option<B>: Some(valor) lo incluye, None lo descarta.

let strings = vec!["1", "abc", "3", "def", "5"];
let nums: Vec<i32> = strings.iter()
    .filter_map(|s| s.parse::<i32>().ok())  // ok() convierte Result → Option
    .collect();
assert_eq!(nums, vec![1, 3, 5]);

// Equivalente sin filter_map (mas verboso):
let nums: Vec<i32> = strings.iter()
    .map(|s| s.parse::<i32>())
    .filter(|r| r.is_ok())
    .map(|r| r.unwrap())
    .collect();
```

## enumerate — agregar indices

`enumerate` envuelve cada elemento en una tupla `(indice, elemento)`.
El indice empieza en 0:

```rust
// Firma simplificada:
// fn enumerate(self) -> Enumerate<Self>
// Item cambia de T a (usize, T)

let colors = vec!["red", "green", "blue"];
let indexed: Vec<(usize, &&str)> = colors.iter().enumerate().collect();
assert_eq!(indexed, vec![(0, &"red"), (1, &"green"), (2, &"blue")]);
```

```rust
// Uso tipico: necesitar el indice en un loop
let words = vec!["hello", "world", "rust"];
for (i, word) in words.iter().enumerate() {
    println!("{i}: {word}");
}
// 0: hello
// 1: world
// 2: rust
```

```rust
// enumerate + filter — encontrar posiciones:
let v = vec![10, 20, 30, 40, 50];
let positions: Vec<usize> = v.iter()
    .enumerate()
    .filter(|(_, &val)| val > 25)
    .map(|(i, _)| i)
    .collect();
assert_eq!(positions, vec![2, 3, 4]);
```

```rust
// enumerate + map — transformar con contexto de posicion:
let letters = vec!['a', 'b', 'c'];
let result: Vec<String> = letters.iter()
    .enumerate()
    .map(|(i, ch)| format!("{}={}", i + 1, ch))
    .collect();
assert_eq!(result, vec!["1=a", "2=b", "3=c"]);
```

## zip — combinar dos iteradores

`zip` toma dos iteradores y produce tuplas emparejando
elementos por posicion. Se detiene cuando el mas corto se agota:

```rust
// Firma simplificada:
// fn zip<U: IntoIterator>(self, other: U) -> Zip<Self, U::IntoIter>

let names = vec!["Alice", "Bob", "Carol"];
let ages = vec![30, 25, 35];

let pairs: Vec<(&&str, &i32)> = names.iter().zip(ages.iter()).collect();
assert_eq!(pairs, vec![(&"Alice", &30), (&"Bob", &25), (&"Carol", &35)]);
```

```rust
// zip se detiene en el iterador mas corto:
let a = vec![1, 2, 3, 4, 5];
let b = vec!["x", "y", "z"];
let zipped: Vec<(&i32, &&str)> = a.iter().zip(b.iter()).collect();
assert_eq!(zipped.len(), 3);  // solo 3 pares (el mas corto)
```

```rust
// zip con rangos:
let words = vec!["hello", "world", "rust"];
let numbered: Vec<(i32, &&str)> = (1..).zip(words.iter()).collect();
assert_eq!(numbered, vec![(1, &"hello"), (2, &"world"), (3, &"rust")]);
// (1..) es infinito, pero zip para cuando words se agota
```

```rust
// zip para construir HashMap:
use std::collections::HashMap;

let keys = vec!["name", "city", "age"];
let values = vec!["Alice", "Paris", "30"];

let map: HashMap<&str, &str> = keys.into_iter().zip(values).collect();
assert_eq!(map["name"], "Alice");
assert_eq!(map["city"], "Paris");
```

```rust
// zip para operaciones paralelas sobre dos colecciones:
let prices = vec![10.0, 20.0, 30.0];
let quantities = vec![2, 5, 1];

let totals: Vec<f64> = prices.iter()
    .zip(quantities.iter())
    .map(|(price, qty)| price * (*qty as f64))
    .collect();
assert_eq!(totals, vec![20.0, 100.0, 30.0]);
```

```rust
// unzip — la operacion inversa de zip:
let pairs = vec![(1, 'a'), (2, 'b'), (3, 'c')];
let (nums, chars): (Vec<i32>, Vec<char>) = pairs.into_iter().unzip();
assert_eq!(nums, vec![1, 2, 3]);
assert_eq!(chars, vec!['a', 'b', 'c']);
```

## take y skip — recortar iteradores

`take(n)` toma solo los primeros `n` elementos.
`skip(n)` descarta los primeros `n` y produce el resto:

```rust
// take — primeros n elementos:
let v = vec![10, 20, 30, 40, 50];
let first_three: Vec<&i32> = v.iter().take(3).collect();
assert_eq!(first_three, vec![&10, &20, &30]);

// skip — saltar los primeros n elementos:
let last_two: Vec<&i32> = v.iter().skip(3).collect();
assert_eq!(last_two, vec![&40, &50]);
```

```rust
// take con iteradores infinitos — esencial para limitar:
let squares: Vec<i32> = (1..)
    .map(|x| x * x)
    .take(5)
    .collect();
assert_eq!(squares, vec![1, 4, 9, 16, 25]);

// Sin take, (1..).map(...).collect() nunca terminaria.
```

```rust
// skip + take — una "ventana" o paginacion:
let data = vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

// Pagina 2, tamano de pagina 3 (indices 3..6):
let page: Vec<&i32> = data.iter().skip(3).take(3).collect();
assert_eq!(page, vec![&3, &4, &5]);

// Pagina 3:
let page: Vec<&i32> = data.iter().skip(6).take(3).collect();
assert_eq!(page, vec![&6, &7, &8]);
```

```rust
// take_while — tomar mientras la condicion sea true:
let v = vec![1, 2, 3, 4, 5, 1, 2];
let prefix: Vec<&i32> = v.iter().take_while(|&&x| x < 4).collect();
assert_eq!(prefix, vec![&1, &2, &3]);
// Se detiene en el primer 4 — NO continua con el 1, 2 final.

// skip_while — saltar mientras la condicion sea true:
let suffix: Vec<&i32> = v.iter().skip_while(|&&x| x < 4).collect();
assert_eq!(suffix, vec![&4, &5, &1, &2]);
// Salta 1, 2, 3 (condicion true), luego produce TODO el resto.
```

```text
take_while vs filter:

take_while se DETIENE en el primer false — es un corte de prefijo.
filter SIGUE buscando elementos que cumplan — recorre todo.

  v = [1, 2, 5, 3, 1]
  take_while(|x| x < 4) → [1, 2]         (para en 5)
  filter(|x| x < 4)     → [1, 2, 3, 1]   (sigue buscando)
```

## chain — concatenar iteradores

`chain` une dos iteradores en secuencia. Primero produce todos
los elementos del primero, luego todos los del segundo:

```rust
// Firma simplificada:
// fn chain<U: IntoIterator<Item = Self::Item>>(self, other: U) -> Chain<Self, U>

let a = vec![1, 2, 3];
let b = vec![4, 5, 6];
let all: Vec<&i32> = a.iter().chain(b.iter()).collect();
assert_eq!(all, vec![&1, &2, &3, &4, &5, &6]);
```

```rust
// chain con tipos distintos de iterador (mismo Item):
let arr = [10, 20];
let vec = vec![30, 40];
let range = 50..=60;

let combined: Vec<i32> = arr.into_iter()
    .chain(vec.into_iter())
    .chain(range)
    .collect();
assert_eq!(combined, vec![10, 20, 30, 40, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60]);
```

```rust
// chain para agregar un elemento al principio o final:
let v = vec![2, 3, 4];

// Agregar al principio:
let with_first: Vec<i32> = std::iter::once(1).chain(v.iter().copied()).collect();
assert_eq!(with_first, vec![1, 2, 3, 4]);

// Agregar al final:
let with_last: Vec<i32> = v.iter().copied().chain(std::iter::once(5)).collect();
assert_eq!(with_last, vec![2, 3, 4, 5]);
```

```rust
// chain util para procesar multiples fuentes:
fn process_all_names(admins: &[String], users: &[String]) {
    for name in admins.iter().chain(users.iter()) {
        println!("Processing: {name}");
    }
    // Un solo loop, sin duplicar logica
}
```

## flatten — aplanar iteradores anidados

`flatten` toma un iterador de iteradores (o de colecciones) y
produce los elementos internos en secuencia:

```rust
// Firma simplificada:
// fn flatten(self) -> Flatten<Self>
// donde Self::Item: IntoIterator

let nested = vec![vec![1, 2], vec![3, 4], vec![5]];
let flat: Vec<&i32> = nested.iter().flatten().collect();
assert_eq!(flat, vec![&1, &2, &3, &4, &5]);

// Con into_iter:
let nested = vec![vec![1, 2], vec![3, 4], vec![5]];
let flat: Vec<i32> = nested.into_iter().flatten().collect();
assert_eq!(flat, vec![1, 2, 3, 4, 5]);
```

```rust
// flatten con Option — elimina los None y extrae los Some:
let data = vec![Some(1), None, Some(3), None, Some(5)];
let values: Vec<i32> = data.into_iter().flatten().collect();
assert_eq!(values, vec![1, 3, 5]);
// Option implementa IntoIterator:
//   Some(x) → iterador de 1 elemento
//   None    → iterador vacio
```

```rust
// flatten con Result — extrae los Ok, descarta los Err:
let results: Vec<Result<i32, &str>> = vec![Ok(1), Err("bad"), Ok(3)];
let oks: Vec<i32> = results.into_iter().flatten().collect();
assert_eq!(oks, vec![1, 3]);
```

```rust
// flat_map = map + flatten — el combo mas comun:
let sentences = vec!["hello world", "foo bar baz"];
let words: Vec<&str> = sentences.iter()
    .flat_map(|s| s.split_whitespace())
    .collect();
assert_eq!(words, vec!["hello", "world", "foo", "bar", "baz"]);

// Equivalente sin flat_map:
let words: Vec<&str> = sentences.iter()
    .map(|s| s.split_whitespace())
    .flatten()
    .collect();
```

```rust
// flat_map para one-to-many:
let v = vec![1, 2, 3];
let repeated: Vec<i32> = v.iter()
    .flat_map(|&x| vec![x, x * 10])
    .collect();
assert_eq!(repeated, vec![1, 10, 2, 20, 3, 30]);
// Cada elemento genera dos valores.
```

```rust
// flatten con profundidad 1 — solo un nivel:
let deep = vec![vec![vec![1, 2], vec![3]], vec![vec![4]]];
let one_level: Vec<Vec<i32>> = deep.into_iter().flatten().collect();
assert_eq!(one_level, vec![vec![1, 2], vec![3], vec![4]]);
// Solo aplano un nivel. Para aplanar completamente: .flatten().flatten()
// o .flat_map(|v| v.into_iter().flatten())
```

## Otros adaptadores utiles

### peekable — mirar sin consumir

```rust
// peekable() envuelve el iterador para permitir peek():
let v = vec![1, 2, 3];
let mut iter = v.iter().peekable();

// peek() retorna Option<&&i32> sin avanzar:
assert_eq!(iter.peek(), Some(&&1));
assert_eq!(iter.peek(), Some(&&1));  // sigue en el mismo lugar

assert_eq!(iter.next(), Some(&1));   // ahora si avanza
assert_eq!(iter.peek(), Some(&&2));
```

```rust
// Uso tipico: parsing — decidir segun el proximo elemento
let tokens = vec!["if", "(", "x", ")", "{", "}"];
let mut iter = tokens.iter().peekable();

while let Some(&token) = iter.peek() {
    if *token == "if" {
        println!("encontre if-statement");
        iter.next();  // consumir "if"
    } else {
        iter.next();  // consumir otro token
    }
}
```

### inspect — debug sin alterar

```rust
// inspect ejecuta una closure por cada elemento sin modificarlo.
// Util para depuracion:
let v = vec![1, 2, 3, 4, 5];
let result: Vec<i32> = v.iter()
    .inspect(|x| println!("antes de filter: {x}"))
    .filter(|&&x| x > 2)
    .inspect(|x| println!("despues de filter: {x}"))
    .copied()
    .collect();
// antes de filter: 1
// antes de filter: 2
// antes de filter: 3
// despues de filter: 3
// antes de filter: 4
// despues de filter: 4
// antes de filter: 5
// despues de filter: 5
assert_eq!(result, vec![3, 4, 5]);
```

### copied y cloned — convertir referencias a valores

```rust
// copied() — convierte &T a T (requiere T: Copy):
let v = vec![1, 2, 3];
let owned: Vec<i32> = v.iter().copied().collect();
//   v.iter() produce &i32
//   .copied() convierte a i32

// cloned() — convierte &T a T (requiere T: Clone):
let v = vec![String::from("a"), String::from("b")];
let owned: Vec<String> = v.iter().cloned().collect();
//   v.iter() produce &String
//   .cloned() llama .clone() en cada elemento
```

```rust
// copied vs cloned:
// copied → requiere Copy, es gratis (memcpy de bits)
// cloned → requiere Clone, puede ser costoso (allocations)
//
// Prefiere copied para i32, f64, bool, char, &str, etc.
// Usa cloned solo cuando necesites clonar tipos non-Copy.
```

### step_by — saltar elementos a intervalos

```rust
let v: Vec<i32> = (0..20).step_by(3).collect();
assert_eq!(v, vec![0, 3, 6, 9, 12, 15, 18]);

let v: Vec<i32> = (1..=10).step_by(2).collect();
assert_eq!(v, vec![1, 3, 5, 7, 9]);
```

### rev — invertir (solo para DoubleEndedIterator)

```rust
let v = vec![1, 2, 3, 4, 5];
let reversed: Vec<&i32> = v.iter().rev().collect();
assert_eq!(reversed, vec![&5, &4, &3, &2, &1]);

// rev requiere DoubleEndedIterator — no todos los iteradores lo tienen.
// Vec, slice, Range, String::chars() si lo tienen.
// HashMap::iter() NO lo tiene (no hay orden definido).
```

## Encadenamiento de adaptadores

La potencia real de los adaptadores esta en encadenarlos.
Cada encadenamiento construye un pipeline lazy que se ejecuta
en un solo recorrido:

```rust
let data = vec![
    ("Alice", 85),
    ("Bob", 92),
    ("Carol", 78),
    ("Dave", 95),
    ("Eve", 88),
];

// Top 3 estudiantes con nota > 80, formateados:
let honor_roll: Vec<String> = data.iter()
    .filter(|(_, score)| *score > 80)
    .enumerate()
    .map(|(i, (name, score))| format!("{}. {} ({})", i + 1, name, score))
    .take(3)
    .collect();

assert_eq!(honor_roll, vec![
    "1. Alice (85)",
    "2. Bob (92)",
    "3. Dave (95)",
]);
```

```rust
// Procesamiento de texto — contar palabras por linea:
let text = "hello world\nfoo bar baz\nrust";
let word_counts: Vec<(usize, usize)> = text.lines()
    .map(|line| line.split_whitespace().count())
    .enumerate()
    .collect();
assert_eq!(word_counts, vec![(0, 2), (1, 3), (2, 1)]);
```

```rust
// Pipeline complejo — procesar logs:
let logs = vec![
    "[INFO] server started",
    "[ERROR] connection refused",
    "[INFO] request received",
    "[ERROR] timeout",
    "[WARN] slow query",
    "[ERROR] disk full",
];

let errors: Vec<&str> = logs.iter()
    .filter(|line| line.starts_with("[ERROR]"))
    .map(|line| &line[8..])  // quitar "[ERROR] "
    .collect();
assert_eq!(errors, vec!["connection refused", "timeout", "disk full"]);
```

```rust
// Interleave con zip + flat_map:
let a = vec![1, 3, 5];
let b = vec![2, 4, 6];

let interleaved: Vec<i32> = a.iter()
    .zip(b.iter())
    .flat_map(|(&x, &y)| vec![x, y])
    .collect();
assert_eq!(interleaved, vec![1, 2, 3, 4, 5, 6]);
```

## Performance — zero-cost abstractions

Los adaptadores en Rust son abstracciones de costo cero.
El compilador optimiza las cadenas de iteradores al mismo
codigo que escribirias con loops manuales:

```rust
// Esta cadena de iteradores:
let sum: i32 = (1..=100)
    .filter(|x| x % 2 == 0)
    .map(|x| x * x)
    .sum();

// Compila al mismo codigo maquina que:
let mut sum = 0;
for x in 1..=100 {
    if x % 2 == 0 {
        sum += x * x;
    }
}

// No hay allocations intermedias.
// No hay dispatch dinamico.
// Monomorphization genera codigo especializado.
```

```text
¿Por que zero-cost?

1. Monomorphization: cada combinacion de adaptadores genera
   codigo especializado en compilacion. Map<Filter<Iter<i32>>>
   es un tipo concreto con un next() optimizado.

2. Sin colecciones intermedias: map().filter() no crea Vec
   temporales. Cada elemento fluye por toda la cadena.

3. Inlining: el compilador inserta las closures directamente
   en el loop, eliminando las llamadas a funcion.

4. LLVM optimiza el resultado como un loop simple.
```

## Errores comunes

```rust
// ERROR 1: olvidar que los adaptadores son lazy
let v = vec![1, 2, 3];
v.iter().map(|x| {
    println!("{x}");  // NUNCA se ejecuta
    x * 2
});
// warning: unused `Map` that must be used
// Solucion: consumir con .collect(), .for_each(), .count(), etc.

// Si solo quieres el efecto secundario:
v.iter().for_each(|x| println!("{x}"));
// O simplemente un for loop:
for x in &v { println!("{x}"); }
```

```rust
// ERROR 2: doble referencia en filter
let v = vec![1, 2, 3, 4, 5];

// iter() produce &i32, filter pasa &&i32:
// v.iter().filter(|x| x % 2 == 0)  // error: % no definido para &&i32

// Soluciones:
v.iter().filter(|x| **x % 2 == 0);   // deref manual
v.iter().filter(|&&x| x % 2 == 0);   // destructurar patron
v.iter().filter(|x| *x % 2 == 0);    // un deref si el metodo acepta &i32
v.iter().copied().filter(|x| x % 2 == 0);  // copiar antes
```

```rust
// ERROR 3: confundir map + flatten con flat_map
let v = vec![vec![1, 2], vec![3, 4]];

// Esto NO compila — map no aplana automaticamente:
// let flat: Vec<i32> = v.iter().map(|inner| inner.iter()).collect();
// error: collect() no puede convertir un iterador de iteradores a Vec<i32>

// Solucion 1: flatten despues de map
let flat: Vec<&i32> = v.iter().map(|inner| inner.iter()).flatten().collect();

// Solucion 2: flat_map (idiomatico)
let flat: Vec<&i32> = v.iter().flat_map(|inner| inner.iter()).collect();
```

```rust
// ERROR 4: take_while vs filter (comportamiento diferente)
let v = vec![1, 3, 5, 2, 7, 9];

// take_while PARA en el primer false:
let result: Vec<&i32> = v.iter().take_while(|&&x| x < 5).collect();
assert_eq!(result, vec![&1, &3]);
// Se detuvo en 5, NO incluyo 2 ni 7

// filter recorre TODO:
let result: Vec<&i32> = v.iter().filter(|&&x| x < 5).collect();
assert_eq!(result, vec![&1, &3, &2]);
// Incluye 2 porque filter sigue buscando
```

```rust
// ERROR 5: zip silenciosamente descarta elementos sobrantes
let a = vec![1, 2, 3, 4, 5];
let b = vec!["x", "y"];
let zipped: Vec<_> = a.iter().zip(b.iter()).collect();
assert_eq!(zipped.len(), 2);  // perdimos los elementos 3, 4, 5

// Si necesitas saber si los iteradores tienen distinta longitud,
// usa itertools::zip_longest o verifica manualmente.
```

```rust
// ERROR 6: encadenar demasiados adaptadores — legibilidad
// Malo:
let result: Vec<String> = data.iter()
    .filter(|x| x.len() > 3)
    .map(|x| x.to_uppercase())
    .enumerate()
    .filter(|(i, _)| i % 2 == 0)
    .map(|(_, s)| s)
    .chain(std::iter::once("EXTRA".to_string()))
    .take(5)
    .collect();

// Mejor: partir en pasos intermedios con nombres descriptivos
let long_words = data.iter().filter(|x| x.len() > 3);
let uppercased = long_words.map(|x| x.to_uppercase());
let every_other = uppercased.enumerate()
    .filter(|(i, _)| i % 2 == 0)
    .map(|(_, s)| s);
let result: Vec<String> = every_other
    .chain(std::iter::once("EXTRA".to_string()))
    .take(5)
    .collect();
// Los intermedios siguen siendo lazy — no hay costo extra.
```

## Cheatsheet

```text
Adaptadores principales:
  .map(|x| ...)          transforma cada elemento
  .filter(|x| ...)       selecciona elementos (recibe &Item)
  .filter_map(|x| ...)   map + filter con Option
  .flat_map(|x| ...)     map + flatten (one-to-many)
  .flatten()             aplana un nivel de anidamiento

Posicion y orden:
  .enumerate()           agrega indice: (usize, Item)
  .zip(other)            empareja con otro iterador: (A, B)
  .unzip()               separa pares en dos colecciones
  .rev()                 invierte el orden (DoubleEndedIterator)

Recortar:
  .take(n)               solo los primeros n elementos
  .skip(n)               salta los primeros n
  .take_while(|x| ...)   toma mientras condicion sea true
  .skip_while(|x| ...)   salta mientras condicion sea true
  .step_by(n)            cada n-esimo elemento

Combinar:
  .chain(other)          concatena dos iteradores en secuencia

Conversion de referencias:
  .copied()              &T → T (requiere Copy)
  .cloned()              &T → T (requiere Clone)

Debug y utilidad:
  .inspect(|x| ...)      ejecuta efecto sin alterar elementos
  .peekable()            permite peek() sin consumir

Reglas clave:
  - Todos son LAZY — necesitan un consumidor para ejecutar
  - Se pueden encadenar sin costo — zero-cost abstractions
  - Procesamiento elemento por elemento, no paso por paso
  - filter recibe &Item (doble ref con iter())
  - take_while PARA en el primer false; filter sigue buscando
  - zip se detiene en el iterador mas corto
  - flatten solo aplana un nivel
```

---

## Ejercicios

### Ejercicio 1 — Cadena de adaptadores

```rust
// Dado:
let data = vec![
    ("Alice", vec![85, 92, 78]),
    ("Bob", vec![90, 88, 95]),
    ("Carol", vec![70, 65, 80]),
    ("Dave", vec![98, 100, 97]),
];

// Usando adaptadores de iterador (sin loops for), produce:
//
// a) Un Vec<(&str, f64)> con el nombre y promedio de notas de cada
//    estudiante. (pista: map)
//
// b) Un Vec<&str> con los nombres de estudiantes cuyo promedio > 85.
//    (pista: filter + map, o filter_map)
//
// c) Un Vec<i32> con TODAS las notas de TODOS los estudiantes
//    en un solo Vec aplanado. (pista: flat_map)
//
// d) La nota mas alta entre todos los estudiantes.
//    (pista: flat_map + max)
```

### Ejercicio 2 — take_while vs filter

```rust
// Dado:
let temps = vec![22, 25, 28, 31, 35, 30, 27, 24];

// Predice el resultado de cada uno ANTES de ejecutar:
//
// a) let a: Vec<&i32> = temps.iter().take_while(|&&t| t < 30).collect();
//
// b) let b: Vec<&i32> = temps.iter().filter(|&&t| t < 30).collect();
//
// c) let c: Vec<&i32> = temps.iter().skip_while(|&&t| t < 30).collect();
//
// Preguntas:
//   ¿Cuantos elementos tiene cada resultado?
//   ¿Por que a y b son distintos si la condicion es la misma?
//   ¿Que elementos estan en b pero no en a?
```

### Ejercicio 3 — zip y enumerate

```rust
// Dado:
let products = vec!["laptop", "mouse", "keyboard", "monitor"];
let prices = vec![999.99, 29.99, 79.99, 349.99];

// a) Usa zip para crear un Vec<(&str, f64)> de pares (producto, precio).
//
// b) Usa enumerate para crear un Vec<String> con formato:
//    "1. laptop - $999.99"
//    "2. mouse - $29.99"
//    ...
//
// c) Usa zip + filter + map para obtener un Vec<&str> con los
//    nombres de productos que cuestan mas de $100.
//
// d) ¿Que pasa si prices tiene menos elementos que products?
//    Pruebalo con prices = vec![999.99, 29.99] y verifica.
```
