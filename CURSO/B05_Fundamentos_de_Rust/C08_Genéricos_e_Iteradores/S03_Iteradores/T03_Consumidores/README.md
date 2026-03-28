# T03 — Consumidores

## Que es un consumidor

Un consumidor es un metodo de `Iterator` que **ejecuta** el iterador
y produce un valor final. A diferencia de los adaptadores (que retornan
otro iterador), los consumidores disparan toda la cadena lazy:

```rust
let v = vec![1, 2, 3, 4, 5];

// Adaptadores: construyen el pipeline (lazy, no ejecutan nada)
let pipeline = v.iter()
    .filter(|&&x| x > 2)
    .map(|x| x * 10);

// Consumidor: ejecuta todo el pipeline y produce un valor
let result: Vec<&i32> = v.iter().filter(|&&x| x > 2).collect();
let total: i32 = v.iter().sum();
let n: usize = v.iter().count();
```

```text
Pipeline completo:

  iterador → adaptador → adaptador → CONSUMIDOR → valor
  (fuente)   (lazy)       (lazy)      (ejecuta)    (resultado)

Sin consumidor, la cadena nunca se ejecuta.
El compilador emite un warning si construyes un pipeline sin consumirlo.
```

---

## collect — acumular en una coleccion

`collect` es el consumidor mas versatil. Consume el iterador y
construye una coleccion. El tipo de coleccion se infiere de la
anotacion de tipo o del turbofish:

```rust
// Firma simplificada:
// fn collect<B: FromIterator<Self::Item>>(self) -> B

let v = vec![1, 2, 3, 4, 5];

// Anotacion de tipo — la forma mas comun:
let doubled: Vec<i32> = v.iter().map(|x| x * 2).collect();
assert_eq!(doubled, vec![2, 4, 6, 8, 10]);

// Turbofish — cuando no hay variable con tipo:
let doubled = v.iter().map(|x| x * 2).collect::<Vec<i32>>();

// Tipo parcial con _ — collect infiere el Item:
let doubled: Vec<_> = v.iter().map(|x| x * 2).collect();
```

### collect en distintas colecciones

```rust
use std::collections::{HashMap, HashSet, BTreeSet, VecDeque, LinkedList};

let v = vec![1, 2, 3, 2, 1];

// Vec:
let as_vec: Vec<&i32> = v.iter().collect();

// HashSet (elimina duplicados):
let as_set: HashSet<&i32> = v.iter().collect();
assert_eq!(as_set.len(), 3);  // {1, 2, 3}

// BTreeSet (ordenado, sin duplicados):
let as_sorted: BTreeSet<&i32> = v.iter().collect();

// VecDeque:
let as_deque: VecDeque<&i32> = v.iter().collect();

// String desde chars:
let hello: String = vec!['h', 'e', 'l', 'l', 'o'].into_iter().collect();
assert_eq!(hello, "hello");

// String desde &str:
let words = vec!["hello", " ", "world"];
let sentence: String = words.into_iter().collect();
assert_eq!(sentence, "hello world");
```

### collect en HashMap

```rust
use std::collections::HashMap;

// Desde iterador de tuplas (clave, valor):
let keys = vec!["name", "city"];
let values = vec!["Alice", "Paris"];
let map: HashMap<&str, &str> = keys.into_iter().zip(values).collect();
assert_eq!(map["name"], "Alice");

// Desde enumerate (indice como clave):
let fruits = vec!["apple", "banana", "cherry"];
let indexed: HashMap<usize, &&str> = fruits.iter().enumerate().collect();
assert_eq!(indexed[&0], &"apple");

// Agrupar con fold (collect no agrupa directamente):
let words = vec!["hello", "hi", "world", "wonder"];
let mut by_letter: HashMap<char, Vec<&str>> = HashMap::new();
for w in &words {
    by_letter.entry(w.chars().next().unwrap()).or_default().push(w);
}
// by_letter = {'h': ["hello", "hi"], 'w': ["world", "wonder"]}
```

### collect con Result — cortocircuito en errores

```rust
// Si el iterador produce Result<T, E>, collect puede producir
// Result<Vec<T>, E> — se detiene en el primer error:

let strings = vec!["1", "2", "3"];
let nums: Result<Vec<i32>, _> = strings.iter()
    .map(|s| s.parse::<i32>())
    .collect();
assert_eq!(nums, Ok(vec![1, 2, 3]));

// Con un error:
let strings = vec!["1", "abc", "3"];
let nums: Result<Vec<i32>, _> = strings.iter()
    .map(|s| s.parse::<i32>())
    .collect();
assert!(nums.is_err());
// Se detuvo en "abc" — no intento parsear "3"
```

```rust
// Tambien funciona con Option:
let maybes = vec![Some(1), Some(2), Some(3)];
let all: Option<Vec<i32>> = maybes.into_iter().collect();
assert_eq!(all, Some(vec![1, 2, 3]));

let maybes = vec![Some(1), None, Some(3)];
let all: Option<Vec<i32>> = maybes.into_iter().collect();
assert_eq!(all, None);  // un None invalida todo
```

### unzip — collect que separa pares

```rust
// unzip es un caso especial de collect que separa tuplas:
let pairs = vec![(1, 'a'), (2, 'b'), (3, 'c')];
let (nums, chars): (Vec<i32>, Vec<char>) = pairs.into_iter().unzip();
assert_eq!(nums, vec![1, 2, 3]);
assert_eq!(chars, vec!['a', 'b', 'c']);
```

---

## sum y product — acumular aritmeticamente

```rust
// sum() requiere que Item implemente std::iter::Sum
let total: i32 = vec![1, 2, 3, 4, 5].iter().sum();
assert_eq!(total, 15);

// Con adaptadores:
let even_sum: i32 = (1..=10).filter(|x| x % 2 == 0).sum();
assert_eq!(even_sum, 30);  // 2 + 4 + 6 + 8 + 10

// product() requiere que Item implemente std::iter::Product
let factorial: i64 = (1..=5).product();
assert_eq!(factorial, 120);  // 1 * 2 * 3 * 4 * 5

// Cuidado con overflow:
let big: i32 = (1..=20).product();  // overflow en debug → panic!
// Solucion: usar i64 o u64
let big: i64 = (1..=20).product();
assert_eq!(big, 2_432_902_008_176_640_000);
```

```rust
// sum con floats:
let avg = {
    let data = vec![85.0, 92.0, 78.0, 95.0, 88.0];
    let sum: f64 = data.iter().sum();
    sum / data.len() as f64
};
assert!((avg - 87.6).abs() < f64::EPSILON);

// sum con referencias — iter() produce &i32, sum acepta &i32:
let v = vec![1, 2, 3];
let total: i32 = v.iter().sum();  // sum de &i32 → i32
assert_eq!(total, 6);
// No necesitas .copied() antes de sum — Sum esta implementado para &i32
```

---

## count y len

```rust
// count() consume el iterador y cuenta los elementos:
let n = vec![1, 2, 3, 4, 5].iter().count();
assert_eq!(n, 5);

// count es util despues de filtrar:
let evens = (1..=100).filter(|x| x % 2 == 0).count();
assert_eq!(evens, 50);

// Si solo necesitas el total sin filtrar, usa .len():
let v = vec![1, 2, 3];
assert_eq!(v.len(), 3);     // O(1), no consume nada
assert_eq!(v.iter().count(), 3);  // O(n), consume el iterador
// .len() es preferible cuando esta disponible.
```

```rust
// count con adaptadores:
let long_words = vec!["hello", "hi", "world", "a", "rust"]
    .iter()
    .filter(|w| w.len() > 2)
    .count();
assert_eq!(long_words, 3);  // "hello", "world", "rust"
```

---

## any y all — predicados booleanos

`any` retorna `true` si **al menos un** elemento cumple el predicado.
`all` retorna `true` si **todos** los elementos cumplen el predicado.
Ambos hacen cortocircuito — se detienen en cuanto saben la respuesta:

```rust
let v = vec![1, 2, 3, 4, 5];

// any: ¿existe algun par?
assert!(v.iter().any(|&x| x % 2 == 0));   // true (2 existe)

// all: ¿todos son positivos?
assert!(v.iter().all(|&x| x > 0));         // true

// all: ¿todos son menores que 4?
assert!(!v.iter().all(|&x| x < 4));        // false (4 y 5 fallan)
```

```rust
// Cortocircuito — no recorre todo:
let v = vec![1, 2, 3, 4, 5];

// any se detiene en el primer true:
let found = v.iter().any(|&x| {
    println!("checking {x}");
    x == 3
});
// Imprime: checking 1, checking 2, checking 3
// NO imprime 4 ni 5

// all se detiene en el primer false:
let all_small = v.iter().all(|&x| {
    println!("checking {x}");
    x < 3
});
// Imprime: checking 1, checking 2, checking 3
// Se detiene en 3 (retorna false)
```

```rust
// any y all con iteradores vacios:
let empty: Vec<i32> = vec![];
assert!(!empty.iter().any(|&x| x > 0));   // false (ningun elemento)
assert!(empty.iter().all(|&x| x > 0));    // true (vacuously true)
// "todos los elementos de un conjunto vacio cumplen cualquier cosa"
```

```rust
// Casos de uso practicos:
let passwords = vec!["abc", "password123!", "short"];

// ¿Alguna contrasena tiene mas de 10 caracteres?
let has_long = passwords.iter().any(|p| p.len() > 10);
assert!(has_long);

// ¿Todas tienen al menos 3 caracteres?
let all_min = passwords.iter().all(|p| p.len() >= 3);
assert!(all_min);

// Verificar que un slice no esta vacio y todos son positivos:
let values = vec![1, 2, 3];
let valid = !values.is_empty() && values.iter().all(|&v| v > 0);
assert!(valid);
```

```rust
// any vs contains:
let v = vec![1, 2, 3, 4, 5];

// Para buscar un valor exacto, contains es mas legible:
assert!(v.contains(&3));

// any es para condiciones mas complejas:
assert!(v.iter().any(|&x| x > 3 && x % 2 == 0));  // 4 cumple
```

---

## find y position — buscar elementos

`find` retorna el **primer elemento** que cumple el predicado.
`position` retorna el **indice** del primer elemento que cumple:

```rust
let v = vec![1, 2, 3, 4, 5];

// find retorna Option<&Item>:
let first_even = v.iter().find(|&&x| x % 2 == 0);
assert_eq!(first_even, Some(&&2));
// Doble referencia: iter() → &i32, find recibe &&i32, retorna Option<&&i32>

// Con into_iter (sin doble referencia):
let v = vec![1, 2, 3, 4, 5];
let first_even = v.into_iter().find(|&x| x % 2 == 0);
assert_eq!(first_even, Some(2));
```

```rust
// position retorna Option<usize>:
let v = vec![10, 20, 30, 40, 50];
let pos = v.iter().position(|&x| x == 30);
assert_eq!(pos, Some(2));

let pos = v.iter().position(|&x| x == 99);
assert_eq!(pos, None);

// rposition — busca desde el final:
let v = vec![1, 2, 3, 2, 1];
let last_two = v.iter().rposition(|&x| x == 2);
assert_eq!(last_two, Some(3));  // indice 3 (el segundo 2)
```

```rust
// find_map — combina find + map:
let strings = vec!["foo", "42", "bar", "7"];
let first_num: Option<i32> = strings.iter()
    .find_map(|s| s.parse::<i32>().ok());
assert_eq!(first_num, Some(42));

// Equivalente sin find_map:
let first_num = strings.iter()
    .filter_map(|s| s.parse::<i32>().ok())
    .next();  // next() es un consumidor minimo
assert_eq!(first_num, Some(42));
```

```rust
// find con structs:
struct User { name: String, age: u32 }

let users = vec![
    User { name: "Alice".into(), age: 30 },
    User { name: "Bob".into(), age: 17 },
    User { name: "Carol".into(), age: 25 },
];

let minor = users.iter().find(|u| u.age < 18);
assert_eq!(minor.unwrap().name, "Bob");

// position con structs:
let pos = users.iter().position(|u| u.name == "Carol");
assert_eq!(pos, Some(2));
```

---

## min, max y extremos

```rust
let v = vec![3, 1, 4, 1, 5, 9, 2, 6];

// min y max retornan Option (None si el iterador esta vacio):
assert_eq!(v.iter().min(), Some(&1));
assert_eq!(v.iter().max(), Some(&9));

// Iterador vacio:
let empty: Vec<i32> = vec![];
assert_eq!(empty.iter().min(), None);
```

```rust
// min_by y max_by — comparador custom:
let v = vec![-5, 3, -1, 4, -2];

// Maximo por valor absoluto:
let max_abs = v.iter().max_by(|a, b| a.abs().cmp(&b.abs()));
assert_eq!(max_abs, Some(&-5));

// Para floats (no implementan Ord):
let floats = vec![1.5, 0.3, 2.7, 1.1];
let max = floats.iter().max_by(|a, b| a.partial_cmp(b).unwrap());
assert_eq!(max, Some(&2.7));
// Cuidado: partial_cmp con NaN retorna None → unwrap panic
// Alternativa segura:
let max = floats.iter()
    .max_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
```

```rust
// min_by_key y max_by_key — comparar por una clave:
struct Student { name: &'static str, grade: u32 }

let students = vec![
    Student { name: "Alice", grade: 85 },
    Student { name: "Bob", grade: 92 },
    Student { name: "Carol", grade: 78 },
];

let best = students.iter().max_by_key(|s| s.grade);
assert_eq!(best.unwrap().name, "Bob");

let worst = students.iter().min_by_key(|s| s.grade);
assert_eq!(worst.unwrap().name, "Carol");

// min_by_key con strings (por longitud):
let words = vec!["hello", "hi", "world"];
let shortest = words.iter().min_by_key(|w| w.len());
assert_eq!(shortest, Some(&"hi"));
```

---

## fold y reduce — acumulacion general

`fold` es el consumidor mas general — todo lo demas (sum, count, any,
all, max, min) se puede expresar con fold:

```rust
// Firma:
// fn fold<B, F>(self, init: B, f: F) -> B
// donde F: FnMut(B, Self::Item) -> B

// init es el valor inicial del acumulador.
// f recibe (acumulador, elemento) y retorna el nuevo acumulador.

let v = vec![1, 2, 3, 4, 5];

// sum con fold:
let total = v.iter().fold(0, |acc, &x| acc + x);
assert_eq!(total, 15);

// product con fold:
let product = v.iter().fold(1, |acc, &x| acc * x);
assert_eq!(product, 120);

// count con fold:
let count = v.iter().fold(0, |acc, _| acc + 1);
assert_eq!(count, 5);
```

```rust
// fold puede cambiar el tipo — el acumulador no necesita ser del mismo tipo:
let words = vec!["hello", "world", "rust"];

// Construir un String (acumulador = String, item = &str):
let sentence = words.iter().fold(String::new(), |mut acc, &w| {
    if !acc.is_empty() {
        acc.push(' ');
    }
    acc.push_str(w);
    acc
});
assert_eq!(sentence, "hello world rust");
```

```rust
// fold para contar ocurrencias:
let text = "hello world";
let vowels = text.chars().fold(0, |count, ch| {
    if "aeiou".contains(ch) { count + 1 } else { count }
});
assert_eq!(vowels, 3);  // e, o, o
```

```rust
// fold para construir estructuras complejas:
use std::collections::HashMap;

let words = vec!["hello", "world", "hello", "rust", "world", "hello"];
let freq: HashMap<&str, usize> = words.iter()
    .fold(HashMap::new(), |mut map, &word| {
        *map.entry(word).or_insert(0) += 1;
        map
    });
assert_eq!(freq["hello"], 3);
assert_eq!(freq["world"], 2);
assert_eq!(freq["rust"], 1);
```

### reduce — fold sin valor inicial

```rust
// reduce usa el primer elemento como valor inicial.
// Retorna Option (None si el iterador esta vacio).

let v = vec![1, 2, 3, 4, 5];

// sum con reduce:
let total = v.iter().copied().reduce(|acc, x| acc + x);
assert_eq!(total, Some(15));

// max con reduce:
let max = v.iter().copied().reduce(|a, b| if a > b { a } else { b });
assert_eq!(max, Some(5));

// Iterador vacio:
let empty: Vec<i32> = vec![];
let result = empty.into_iter().reduce(|acc, x| acc + x);
assert_eq!(result, None);
```

```rust
// fold vs reduce:
//
// fold(init, f):
//   - Siempre retorna B (nunca None)
//   - Puede cambiar de tipo (B ≠ Item)
//   - init es el valor para iterador vacio
//
// reduce(f):
//   - Retorna Option<Item> (None si vacio)
//   - Mismo tipo que Item
//   - Usa el primer elemento como init
//
// Usa fold cuando:
//   - Necesitas un valor inicial diferente (ej: 1 para product)
//   - El resultado es de tipo distinto al Item
//
// Usa reduce cuando:
//   - El resultado es del mismo tipo que Item
//   - Quieres manejar explicitamente el caso vacio con Option
```

---

## for_each — efecto secundario por elemento

```rust
// for_each consume el iterador ejecutando una closure por cada elemento.
// Es la alternativa funcional al for loop.

let v = vec![1, 2, 3];

// Con for loop:
for x in &v {
    println!("{x}");
}

// Con for_each:
v.iter().for_each(|x| println!("{x}"));

// La diferencia principal: for_each acepta una closure,
// lo que lo hace combinable con adaptadores:
(1..=5)
    .filter(|x| x % 2 == 0)
    .for_each(|x| println!("even: {x}"));
```

```rust
// ¿Cuando usar for_each vs for?
//
// for loop:
//   - Mas idiomatico en Rust para la mayoria de casos
//   - Soporta break, continue, return (de la funcion que lo contiene)
//   - Mas legible para logica compleja
//
// for_each:
//   - Util al final de una cadena de adaptadores (evita collect innecesario)
//   - No soporta break/continue/return
//   - En iteradores paralelos (rayon) tiene ventaja de performance

// Ejemplo donde for_each es mas limpio:
vec!["Alice", "Bob", "Carol"]
    .iter()
    .enumerate()
    .filter(|(_, name)| name.len() > 3)
    .for_each(|(i, name)| println!("{i}: {name}"));

// vs el equivalente con for:
for (i, name) in vec!["Alice", "Bob", "Carol"]
    .iter()
    .enumerate()
    .filter(|(_, name)| name.len() > 3)
{
    println!("{i}: {name}");
}
```

---

## last y nth — acceso por posicion

```rust
// last() consume todo el iterador y retorna el ultimo elemento:
let v = vec![1, 2, 3, 4, 5];
assert_eq!(v.iter().last(), Some(&5));

// Para Vec, es mas eficiente usar .last() del slice:
assert_eq!(v.last(), Some(&5));  // O(1), no consume

// last despues de filtrar:
let last_even = (1..=10).filter(|x| x % 2 == 0).last();
assert_eq!(last_even, Some(10));
```

```rust
// nth(n) retorna el elemento en la posicion n (0-indexed):
let v = vec![10, 20, 30, 40, 50];
let mut iter = v.iter();

assert_eq!(iter.nth(0), Some(&10));  // consume los primeros 0+1 = 1 elementos
assert_eq!(iter.nth(1), Some(&30));  // consume 1+1 = 2 mas (salta 20, retorna 30)
assert_eq!(iter.nth(0), Some(&40));  // el siguiente elemento

// nth CONSUME los elementos anteriores — no es acceso aleatorio O(1).
// Para acceso directo por indice, usa v[n] en vez de iter.nth(n).
```

---

## Comparacion de consumidores

```text
Consumidores que producen colecciones:
  .collect()           → Vec, HashMap, HashSet, String, Result<Vec>, etc.
  .unzip()             → (ColA, ColB) desde iterador de tuplas

Consumidores aritmeticos:
  .sum()               → suma de todos los elementos
  .product()           → producto de todos los elementos

Consumidores de conteo:
  .count()             → numero de elementos (O(n), consume el iterador)

Consumidores booleanos (con cortocircuito):
  .any(|x| ...)        → true si al menos uno cumple
  .all(|x| ...)        → true si todos cumplen

Consumidores de busqueda (con cortocircuito):
  .find(|x| ...)       → Option<&Item>, primer elemento que cumple
  .find_map(|x| ...)   → Option<B>, primer Some de la closure
  .position(|x| ...)   → Option<usize>, indice del primero que cumple
  .rposition(|x| ...)  → Option<usize>, indice del ultimo que cumple

Consumidores de extremos:
  .min()               → Option<&Item>, minimo (requiere Ord)
  .max()               → Option<&Item>, maximo (requiere Ord)
  .min_by(|a,b| ...)   → con comparador custom
  .max_by(|a,b| ...)   → con comparador custom
  .min_by_key(|x| ...) → minimo por clave
  .max_by_key(|x| ...) → maximo por clave

Consumidores de acumulacion general:
  .fold(init, |acc,x| ...)   → acumula con valor inicial, retorna B
  .reduce(|acc,x| ...)       → acumula sin init, retorna Option<Item>

Consumidores de posicion:
  .last()              → Option<Item>, ultimo elemento
  .nth(n)              → Option<Item>, elemento en posicion n

Consumidores de efecto:
  .for_each(|x| ...)   → ejecuta closure por cada elemento

Regla de oro:
  - Todos consumen el iterador (no puedes reutilizarlo despues)
  - any, all, find, position hacen cortocircuito (eficientes)
  - count, last, sum, fold recorren todo el iterador
  - collect necesita anotacion de tipo (no sabe a que coleccion convertir)
```

---

## Errores comunes

```rust
// ERROR 1: olvidar que collect necesita anotacion de tipo
let v = vec![1, 2, 3];
// let result = v.iter().collect();
// error: type annotations needed
//        cannot infer type of the type parameter `B`

// Solucion — cualquiera de estas formas:
let result: Vec<&i32> = v.iter().collect();
let result = v.iter().collect::<Vec<&i32>>();
let result: Vec<_> = v.iter().collect();  // infiere &i32
```

```rust
// ERROR 2: usar sum sin especificar el tipo del resultado
let v = vec![1, 2, 3];
// let total = v.iter().sum();
// error: type annotations needed
//        cannot infer type of the type parameter `S`

// Solucion:
let total: i32 = v.iter().sum();
let total = v.iter().sum::<i32>();
```

```rust
// ERROR 3: intentar usar el iterador despues de consumirlo
let v = vec![1, 2, 3];
let mut iter = v.iter();
let total: i32 = iter.sum();   // consume iter
// let first = iter.next();    // error: value used after move

// Solucion: crear un nuevo iterador
let first = v.iter().next();
```

```rust
// ERROR 4: confundir find con filter
let v = vec![1, 2, 3, 4, 5];

// find retorna Option<&Item> — el PRIMER match:
let first_even = v.iter().find(|&&x| x % 2 == 0);
assert_eq!(first_even, Some(&&2));

// filter retorna un iterador — TODOS los matches:
let all_evens: Vec<&i32> = v.iter().filter(|&&x| x % 2 == 0).collect();
assert_eq!(all_evens, vec![&2, &4]);
```

```rust
// ERROR 5: fold con el valor inicial incorrecto
let v = vec![2, 3, 4];

// Product con init = 0 → siempre da 0:
let wrong = v.iter().fold(0, |acc, &x| acc * x);
assert_eq!(wrong, 0);  // 0 * 2 * 3 * 4 = 0

// init correcto para product es 1:
let correct = v.iter().fold(1, |acc, &x| acc * x);
assert_eq!(correct, 24);

// init correcto para sum es 0:
let sum = v.iter().fold(0, |acc, &x| acc + x);
assert_eq!(sum, 9);
```

```rust
// ERROR 6: min/max con floats — f64 no implementa Ord
let floats = vec![1.5, 0.3, 2.7];
// let max = floats.iter().max();
// error: the trait `Ord` is not implemented for `f64`

// Solucion: usar max_by con partial_cmp:
let max = floats.iter()
    .max_by(|a, b| a.partial_cmp(b).unwrap());
assert_eq!(max, Some(&2.7));

// O convertir a tipo ordenable si son enteros disfrazados:
let prices_cents: Vec<i64> = vec![150, 30, 270];  // centavos
let max = prices_cents.iter().max();
assert_eq!(max, Some(&270));
```

---

## Cheatsheet

```text
Consumidores principales:
  .collect::<Tipo>()   acumula en coleccion (Vec, HashMap, String, etc.)
  .sum::<Tipo>()       suma de elementos (necesita tipo)
  .product::<Tipo>()   producto de elementos
  .count()             numero de elementos

Busqueda y predicados:
  .any(|x| ...)        ¿al menos uno cumple? (cortocircuito)
  .all(|x| ...)        ¿todos cumplen? (cortocircuito)
  .find(|x| ...)       primer elemento que cumple → Option
  .find_map(|x| ...)   primer Some de la closure → Option
  .position(|x| ...)   indice del primero que cumple → Option

Extremos:
  .min() / .max()             por Ord natural
  .min_by() / .max_by()       con comparador custom
  .min_by_key() / .max_by_key()  por clave extraida

Acumulacion general:
  .fold(init, |acc,x| ...)    con valor inicial, puede cambiar tipo
  .reduce(|acc,x| ...)        sin init, retorna Option

Acceso y efecto:
  .last()                     ultimo elemento
  .nth(n)                     elemento en posicion n
  .for_each(|x| ...)          ejecutar efecto por elemento
  .unzip()                    separar tuplas en dos colecciones

Reglas clave:
  - Todo consumidor consume el iterador (no reutilizable)
  - collect y sum necesitan tipo (anotacion o turbofish)
  - collect con Result<Vec<T>, E> hace cortocircuito en errores
  - fold es el mas general — todo lo demas es un caso especial
  - Para buscar un valor exacto, .contains() es mas simple que .any()
  - any/all con iterador vacio: any → false, all → true
```

---

## Ejercicios

### Ejercicio 1 — collect y sum

```rust
// Dado:
let scores = vec![
    ("Alice", vec![85, 92, 78]),
    ("Bob", vec![90, 88, 95]),
    ("Carol", vec![70, 65, 80]),
    ("Dave", vec![98, 100, 97]),
];

// a) Usa fold o map+collect para crear un HashMap<&str, f64>
//    donde el valor es el promedio de notas de cada estudiante.
//
// b) Usa filter + count para contar cuantos estudiantes tienen
//    promedio mayor a 85.
//
// c) Usa max_by_key para encontrar el estudiante con el promedio
//    mas alto. ¿Quien es?
//
// d) Usa flat_map + sum para calcular la suma total de TODAS
//    las notas de todos los estudiantes.
```

### Ejercicio 2 — fold vs reduce

```rust
// a) Implementa una funcion que reciba un &[i32] y retorne el
//    maximo valor usando fold. ¿Que valor inicial usas?
//    ¿Que pasa si el slice esta vacio?
//
// b) Reimplementa la misma funcion usando reduce.
//    ¿Cual maneja mejor el caso vacio? ¿Por que?
//
// c) Usa fold para convertir un Vec<&str> en un String
//    separado por comas: ["a", "b", "c"] → "a, b, c"
//    (sin coma al final)
//
// d) Usa fold para implementar .any() manualmente:
//    dado un &[i32], retorna true si algun elemento es negativo.
//    ¿Tu implementacion hace cortocircuito como .any()?
```

### Ejercicio 3 — collect con Result

```rust
// Dado:
let inputs = vec!["10", "20", "abc", "40", "xyz"];

// a) Usa map + collect para intentar parsear todos a i32.
//    El resultado debe ser Result<Vec<i32>, _>.
//    ¿Que valor tiene? ¿En cual se detuvo?
//
// b) Usa filter_map para obtener un Vec<i32> con solo los
//    que se parsearon correctamente (ignorando errores).
//    ¿Cuantos elementos tiene?
//
// c) Usa partition para separar en dos Vecs: los que son
//    numeros validos y los que no.
//    Pista: iter().partition(|s| s.parse::<i32>().is_ok())
//
// d) Predice: si cambias "abc" por "30" en inputs,
//    ¿que retorna el collect de Result<Vec<i32>, _> del punto a?
```
