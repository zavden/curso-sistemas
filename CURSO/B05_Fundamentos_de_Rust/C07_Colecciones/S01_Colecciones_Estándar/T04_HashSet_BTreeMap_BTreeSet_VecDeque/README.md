# T04 — HashSet, BTreeMap, BTreeSet, VecDeque

## Las otras colecciones de la stdlib

Ademas de `Vec`, `String` y `HashMap`, la stdlib tiene varias
colecciones mas. Cada una resuelve un problema distinto:

```text
Coleccion       Estructura      Orden          Busqueda    Uso tipico
─────────────────────────────────────────────────────────────────────
HashSet<T>      Tabla hash      No             O(1)        Conjuntos, dedup
BTreeMap<K,V>   B-Tree          Por clave      O(log n)    Map ordenado
BTreeSet<T>     B-Tree          Por valor      O(log n)    Set ordenado
VecDeque<T>     Ring buffer     Insercion      O(1)*       Cola, deque
BinaryHeap<T>   Heap binario    Max primero    O(log n)    Cola de prioridad
LinkedList<T>   Doble enlazada  Insercion      O(n)        Casi nunca usar
```

```rust
// Todas estan en std::collections:
use std::collections::{HashSet, BTreeMap, BTreeSet, VecDeque, BinaryHeap};
```

---

## HashSet\<T\>

Un `HashSet<T>` es un `HashMap<T, ()>` — solo almacena claves,
sin valores. Util para conjuntos: membresía rapida, unicidad,
operaciones de conjuntos.

```rust
use std::collections::HashSet;

// Crear
let mut fruits: HashSet<&str> = HashSet::new();
fruits.insert("apple");
fruits.insert("banana");
fruits.insert("apple");   // duplicado — no se agrega
assert_eq!(fruits.len(), 2);

// Desde iterador
let nums: HashSet<i32> = vec![1, 2, 3, 2, 1].into_iter().collect();
assert_eq!(nums.len(), 3);  // duplicados eliminados

// HashSet::from (Rust 1.56+)
let colors = HashSet::from(["red", "green", "blue"]);
```

```rust
// Operaciones basicas
let mut set = HashSet::from([1, 2, 3, 4, 5]);

assert!(set.contains(&3));    // O(1) — busqueda
assert!(!set.contains(&99));

set.insert(6);                // agrega
let was_present = set.remove(&3);  // quita, retorna bool
assert!(was_present);

set.retain(|&x| x % 2 == 0); // filtra in-place
// set = {2, 4, 6}
```

### Operaciones de conjuntos

```rust
use std::collections::HashSet;

let a: HashSet<i32> = [1, 2, 3, 4].into_iter().collect();
let b: HashSet<i32> = [3, 4, 5, 6].into_iter().collect();

// Union — todos los elementos de ambos
let union: HashSet<&i32> = a.union(&b).collect();
// {1, 2, 3, 4, 5, 6}

// Interseccion — solo los que estan en ambos
let inter: HashSet<&i32> = a.intersection(&b).collect();
// {3, 4}

// Diferencia — en a pero no en b
let diff: HashSet<&i32> = a.difference(&b).collect();
// {1, 2}

// Diferencia simetrica — en uno u otro pero no ambos
let sym: HashSet<&i32> = a.symmetric_difference(&b).collect();
// {1, 2, 5, 6}

// Tambien con operadores:
let union = &a | &b;         // union
let inter = &a & &b;         // interseccion
let diff  = &a - &b;         // diferencia
let sym   = &a ^ &b;         // diferencia simetrica
// Estos retornan HashSet<i32> (no referencias)

// Subconjunto / superconjunto
let small: HashSet<i32> = [1, 2].into_iter().collect();
assert!(small.is_subset(&a));
assert!(a.is_superset(&small));
assert!(a.is_disjoint(&HashSet::from([10, 20])));
```

### Cuando usar HashSet

```text
✓ Verificar si un elemento existe (membresía) — O(1)
✓ Eliminar duplicados de una coleccion
✓ Operaciones de conjuntos (union, interseccion, diferencia)
✓ Tracking de "ya visitado" (grafos, crawlers, dedup)

✗ NO si necesitas orden — usa BTreeSet
✗ NO si necesitas el valor asociado — usa HashMap
```

```rust
// Patron: dedup preservando orden (primer aparicion)
fn dedup_ordered<T: Eq + std::hash::Hash + Clone>(v: &[T]) -> Vec<T> {
    let mut seen = HashSet::new();
    let mut result = Vec::new();
    for item in v {
        if seen.insert(item) {  // insert retorna true si es nuevo
            result.push(item.clone());
        }
    }
    result
}

let v = vec![3, 1, 2, 1, 3, 4, 2];
assert_eq!(dedup_ordered(&v), vec![3, 1, 2, 4]);
```

---

## BTreeMap\<K, V\>

Un `BTreeMap<K, V>` es como `HashMap` pero mantiene las claves
**ordenadas**. Internamente usa un B-Tree (no un arbol binario):

```rust
use std::collections::BTreeMap;

let mut map = BTreeMap::new();
map.insert("cherry", 3);
map.insert("apple", 1);
map.insert("banana", 2);

// Iteracion — SIEMPRE en orden de clave
for (k, v) in &map {
    println!("{k}: {v}");
}
// apple: 1
// banana: 2
// cherry: 3
```

```rust
// La API es casi identica a HashMap:
let mut map = BTreeMap::new();
map.insert(3, "three");
map.insert(1, "one");
map.insert(2, "two");

// get, insert, remove, entry — igual que HashMap
assert_eq!(map.get(&2), Some(&"two"));

map.entry(4).or_insert("four");

// EXTRA: operaciones por rango (HashMap no tiene esto)
// range — iterador sobre un rango de claves
for (k, v) in map.range(2..=3) {
    println!("{k}: {v}");
}
// 2: two
// 3: three

// range con Unbounded
use std::ops::Bound;
for (k, v) in map.range(2..) {
    println!("{k}: {v}");  // 2, 3, 4
}

// first / last (Rust 1.66+)
let (first_key, _) = map.first_key_value().unwrap();
let (last_key, _) = map.last_key_value().unwrap();
assert_eq!(*first_key, 1);
assert_eq!(*last_key, 4);

// pop_first / pop_last — quitar el minimo/maximo
let (k, v) = map.pop_first().unwrap();  // (1, "one")
```

### K necesita Ord (no Hash)

```rust
use std::collections::BTreeMap;

// BTreeMap requiere Ord, NO Hash:
let mut map: BTreeMap<f64, &str> = BTreeMap::new();
// ERROR: f64 no implementa Ord (por NaN)

// Pero funciona con tipos que implementan Ord:
let mut map: BTreeMap<i32, &str> = BTreeMap::new();       // OK
let mut map: BTreeMap<String, i32> = BTreeMap::new();     // OK
let mut map: BTreeMap<(i32, i32), &str> = BTreeMap::new(); // OK

// Struct custom:
#[derive(Eq, PartialEq, Ord, PartialOrd)]
struct Priority(u8);
let mut map: BTreeMap<Priority, String> = BTreeMap::new(); // OK
```

### Cuando usar BTreeMap vs HashMap

```text
                    HashMap                BTreeMap
──────────────────────────────────────────────────────────
Busqueda            O(1) promedio          O(log n)
Insercion           O(1) amortizado        O(log n)
Orden               No                     Si (por clave)
range queries       No                     Si
min/max clave       O(n)                   O(log n)
K requiere          Hash + Eq              Ord
Memoria             Menos predecible       Mas compacta
Cache               Mejor (contiguo)       Peor (arbol)

Usa HashMap cuando:
  ✓ Solo necesitas buscar/insertar/eliminar
  ✓ No importa el orden
  ✓ Rendimiento importa (O(1) vs O(log n))

Usa BTreeMap cuando:
  ✓ Necesitas iterar en orden de clave
  ✓ Necesitas queries por rango (range)
  ✓ Necesitas min/max eficiente
  ✓ K no implementa Hash (pero si Ord)
```

---

## BTreeSet\<T\>

`BTreeSet<T>` es a `BTreeMap<T, ()>` lo que `HashSet<T>` es a
`HashMap<T, ()>`. Un conjunto ordenado:

```rust
use std::collections::BTreeSet;

let mut set = BTreeSet::new();
set.insert(5);
set.insert(2);
set.insert(8);
set.insert(1);

// Iteracion en orden
for val in &set {
    print!("{val} ");
}
// 1 2 5 8

// range
for val in set.range(2..=5) {
    print!("{val} ");
}
// 2 5

// first / last
assert_eq!(set.first(), Some(&1));
assert_eq!(set.last(), Some(&8));

// pop_first / pop_last
let min = set.pop_first();  // Some(1)
let max = set.pop_last();   // Some(8)
```

```rust
// Operaciones de conjuntos — igual que HashSet
let a: BTreeSet<i32> = [1, 2, 3, 4].into_iter().collect();
let b: BTreeSet<i32> = [3, 4, 5, 6].into_iter().collect();

let union: BTreeSet<i32> = a.union(&b).cloned().collect();
// {1, 2, 3, 4, 5, 6} — en orden

let inter: BTreeSet<i32> = a.intersection(&b).cloned().collect();
// {3, 4}

// Tambien con operadores &, |, -, ^
let diff = &a - &b;  // {1, 2}
```

```text
Usa BTreeSet cuando:
  ✓ Necesitas un set con iteracion ordenada
  ✓ Necesitas min/max eficiente
  ✓ Necesitas queries por rango
  ✓ T no implementa Hash pero si Ord
```

---

## VecDeque\<T\>

`VecDeque<T>` es un double-ended queue implementado como ring
buffer. Permite push/pop eficiente en **ambos extremos**:

```text
VecDeque internamente (ring buffer):

Buffer:  [ _ | 3 | 4 | 5 | 1 | 2 | _ | _ ]
           ↑               ↑
          tail             head

Logicamente: [1, 2, 3, 4, 5]
El head y tail "envuelven" el array cuando llegan al final.
Push front y push back son O(1) amortizado.
```

```rust
use std::collections::VecDeque;

let mut deque = VecDeque::new();

// push/pop en ambos extremos — O(1)
deque.push_back(1);    // [1]
deque.push_back(2);    // [1, 2]
deque.push_front(0);   // [0, 1, 2]
deque.push_back(3);    // [0, 1, 2, 3]

let front = deque.pop_front();  // Some(0), deque = [1, 2, 3]
let back  = deque.pop_back();   // Some(3), deque = [1, 2]
```

```rust
// Acceso por indice — O(1)
let deque = VecDeque::from([10, 20, 30, 40]);
assert_eq!(deque[0], 10);
assert_eq!(deque[3], 40);
assert_eq!(deque.front(), Some(&10));
assert_eq!(deque.back(), Some(&40));

// get — con bounds check (Option)
assert_eq!(deque.get(1), Some(&20));
assert_eq!(deque.get(99), None);
```

```rust
// Iteracion
let deque = VecDeque::from([1, 2, 3, 4]);
for val in &deque {
    print!("{val} ");  // 1 2 3 4
}

// Convertir a Vec (consume VecDeque)
let vec: Vec<i32> = deque.into();

// Convertir desde Vec
let vec = vec![1, 2, 3];
let deque: VecDeque<i32> = vec.into();

// make_contiguous — garantiza que el ring buffer sea contiguo
let mut deque = VecDeque::from([1, 2, 3]);
let slice: &[i32] = deque.make_contiguous();
```

### VecDeque como cola (FIFO)

```rust
use std::collections::VecDeque;

// Cola: push_back (encolar), pop_front (desencolar)
let mut queue = VecDeque::new();

queue.push_back("task 1");
queue.push_back("task 2");
queue.push_back("task 3");

while let Some(task) = queue.pop_front() {
    println!("procesando: {task}");
}
// procesando: task 1
// procesando: task 2
// procesando: task 3
```

### Cuando usar VecDeque vs Vec

```text
                    Vec                 VecDeque
──────────────────────────────────────────────────
push_back           O(1) amort.         O(1) amort.
pop_back            O(1)                O(1)
push_front          O(n) — shift all    O(1) amort.
pop_front           O(n) — shift all    O(1)
Indexado            O(1)                O(1)
Memoria contigua    Siempre             No siempre (ring)
Deref a &[T]        Si                  No (make_contiguous)
Cache friendly      Mas                 Menos

Usa Vec cuando:
  ✓ Solo operas al final (push/pop back)
  ✓ Necesitas &[T] frecuentemente
  ✓ Es la coleccion por defecto

Usa VecDeque cuando:
  ✓ Necesitas push/pop al FRENTE (cola FIFO)
  ✓ Implementas un buffer circular
  ✓ Necesitas operaciones eficientes en ambos extremos
```

---

## BinaryHeap\<T\>

`BinaryHeap<T>` es una cola de prioridad: el elemento **maximo**
siempre esta al frente. Push y pop son O(log n):

```rust
use std::collections::BinaryHeap;

let mut heap = BinaryHeap::new();
heap.push(3);
heap.push(1);
heap.push(5);
heap.push(2);

// peek — ver el maximo sin quitarlo
assert_eq!(heap.peek(), Some(&5));

// pop — quitar el maximo
assert_eq!(heap.pop(), Some(5));
assert_eq!(heap.pop(), Some(3));
assert_eq!(heap.pop(), Some(2));
assert_eq!(heap.pop(), Some(1));
assert_eq!(heap.pop(), None);
```

```rust
// Min-heap — usar std::cmp::Reverse
use std::cmp::Reverse;
use std::collections::BinaryHeap;

let mut min_heap = BinaryHeap::new();
min_heap.push(Reverse(3));
min_heap.push(Reverse(1));
min_heap.push(Reverse(5));

assert_eq!(min_heap.pop(), Some(Reverse(1)));  // minimo primero
assert_eq!(min_heap.pop(), Some(Reverse(3)));
assert_eq!(min_heap.pop(), Some(Reverse(5)));
```

```rust
// Desde Vec — heapify en O(n)
let v = vec![4, 1, 7, 3, 9];
let heap = BinaryHeap::from(v);

// Convertir a Vec ordenado
let sorted: Vec<i32> = heap.into_sorted_vec();
assert_eq!(sorted, vec![1, 3, 4, 7, 9]);
```

```text
Usa BinaryHeap cuando:
  ✓ Necesitas acceso rapido al maximo (o minimo con Reverse)
  ✓ Cola de prioridad (tasks, scheduling, Dijkstra)
  ✓ Top-K problemas (los K mas grandes/pequeños)

T debe implementar Ord.
```

---

## LinkedList\<T\> — casi nunca usarla

```rust
use std::collections::LinkedList;

let mut list = LinkedList::new();
list.push_back(1);
list.push_front(0);
list.push_back(2);
// [0, 1, 2]

let front = list.pop_front();  // Some(0)
let back = list.pop_back();    // Some(2)
```

```text
¿Por que casi nunca usar LinkedList?

- Mal cache locality (cada nodo esta en una posicion distinta del heap)
- Vec y VecDeque son casi siempre mas rapidos en la practica
- Indexado es O(n)
- Mas overhead de memoria (cada nodo tiene 2 punteros extra)

Los unicos casos donde puede tener sentido:
- Split/merge O(1) de dos listas (append)
- Cursor API para insercion/eliminacion en el medio durante iteracion
- Interop con algoritmos que requieren estabilidad de punteros

En la practica, si crees que necesitas LinkedList, probablemente
quieres VecDeque o Vec.
```

---

## Arbol de decision

```text
¿Que necesitas?

Secuencia (orden por insercion):
├── Solo push/pop al final → Vec
└── Push/pop en ambos extremos → VecDeque

Clave-Valor (map):
├── Orden no importa → HashMap
├── Necesito iteracion ordenada por clave → BTreeMap
└── Necesito queries por rango → BTreeMap

Conjunto (set, unicidad):
├── Orden no importa → HashSet
├── Necesito iteracion ordenada → BTreeSet
└── Necesito queries por rango → BTreeSet

Cola de prioridad (max/min eficiente):
└── BinaryHeap (con Reverse para min-heap)

Lista enlazada:
└── Casi nunca. Usa Vec o VecDeque.
```

```text
Resumen de requisitos para T/K:

Coleccion       T/K necesita
──────────────────────────────
Vec<T>          nada
VecDeque<T>     nada
HashMap<K,V>    K: Hash + Eq
HashSet<T>      T: Hash + Eq
BTreeMap<K,V>   K: Ord
BTreeSet<T>     T: Ord
BinaryHeap<T>   T: Ord
LinkedList<T>   nada
```

## Errores comunes

```rust
// ERROR 1: usar HashMap cuando necesitas orden
use std::collections::HashMap;
let mut map = HashMap::new();
map.insert(1, "a");
map.insert(2, "b");
map.insert(3, "c");
// for (k,v) in &map { ... } — el orden NO es 1,2,3
// Solucion: BTreeMap si necesitas orden por clave

// ERROR 2: usar BTreeMap cuando no necesitas orden
// BTreeMap es O(log n) vs HashMap O(1)
// Si no necesitas orden, HashMap es mas rapido

// ERROR 3: usar LinkedList "porque es O(1) para insertar"
// En la practica, Vec con insert/remove en el medio es
// mas rapido gracias al cache, hasta miles de elementos

// ERROR 4: BinaryHeap para min — olvidar Reverse
use std::collections::BinaryHeap;
let mut heap = BinaryHeap::from([3, 1, 5]);
assert_eq!(heap.pop(), Some(5));  // MAXIMO, no minimo
// Solucion: wrap con Reverse para min-heap

// ERROR 5: asumir que HashSet preserva orden de insercion
use std::collections::HashSet;
let set: HashSet<i32> = [3, 1, 2].into_iter().collect();
// La iteracion NO sera necesariamente 3, 1, 2
// Solucion: BTreeSet para orden, o indexmap para orden de insercion
```

---

## Ejercicios

### Ejercicio 1 — Operaciones de conjuntos

```rust
// Dados dos vectores de nombres de estudiantes:
//   clase_a = ["Alice", "Bob", "Carol", "Dave"]
//   clase_b = ["Bob", "Dave", "Eve", "Frank"]
//
// Usando HashSet, calcula e imprime:
// 1. Estudiantes en ambas clases (interseccion)
// 2. Todos los estudiantes sin repetir (union)
// 3. Solo en clase_a (diferencia)
// 4. En una sola clase (diferencia simetrica)
//
// Predice los resultados antes de ejecutar.
```

### Ejercicio 2 — Cola de tareas con prioridad

```rust
// Implementa una cola de tareas con prioridad usando BinaryHeap.
//
// struct Task { priority: u8, name: String }
//
// Implementa Ord para que mayor prioridad salga primero.
// Si la prioridad es igual, ordena por nombre alfabeticamente.
//
// Crea 5 tareas con distintas prioridades, agregalas al heap,
// y saca una por una mostrando el orden.
//
// Bonus: ¿como harias una min-heap (menor prioridad primero)?
```

### Ejercicio 3 — Elegir la coleccion correcta

```rust
// Para cada escenario, elige la coleccion mas apropiada y justifica:
//
// a) Almacenar los IDs de usuarios que ya visitaron una pagina
//    para no mostrar el banner dos veces
//
// b) Un buffer de mensajes donde agregas al final y procesas
//    desde el inicio (FIFO)
//
// c) Un leaderboard de puntuaciones donde necesitas iterar
//    del mayor al menor puntaje
//
// d) Un cache de resultados de funcion (memoization) donde
//    buscas por input y retornas output
//
// e) Encontrar los 10 elementos mas grandes de un stream
//    de millones de numeros
//
// Predice y luego verifica con la tabla de decision de este tema.
```
