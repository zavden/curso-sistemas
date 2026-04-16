# HashSet en Rust

## Objetivo

Dominar `std::collections::HashSet` como la estructura de
**conjunto** (set) principal de Rust:

- **Relación con HashMap**: `HashSet<T>` es internamente un
  `HashMap<T, ()>` — misma arquitectura Swiss Table, sin valores.
- **Operaciones de conjuntos**: unión, intersección, diferencia,
  diferencia simétrica.
- **Predicados de conjuntos**: subconjunto, superconjunto,
  disjuntos.
- **API práctica**: construcción, inserción, pertenencia,
  eliminación, iteración.
- **Patrones de uso**: deduplicación, membership testing,
  búsqueda de duplicados, tracking de elementos visitados.
- Referencia: T01 cubrió HashMap en profundidad. Aquí nos
  enfocamos en la semántica de conjuntos y los métodos propios
  de HashSet.

---

## HashSet = HashMap sin valores

Internamente, `HashSet<T>` es un wrapper alrededor de
`HashMap<T, ()>`:

```rust
// Definición simplificada en la biblioteca estándar:
pub struct HashSet<T, S = RandomState> {
    base: HashMap<T, (), S>,
}
```

Esto significa que:
- Usa la misma Swiss Table (hashbrown) por debajo.
- Los elementos deben implementar `Hash + Eq`.
- El rendimiento es idéntico a HashMap: $O(1)$ promedio para
  insert, contains y remove.
- La memoria por elemento es ligeramente menor que HashMap porque
  no se almacena un valor (solo la clave + 1 byte de control).

El tipo `()` (unit) tiene tamaño 0 en Rust, así que el compilador
optimiza: no se reserva espacio para "valores" en el array de
slots.

---

## Construcción

```rust
use std::collections::HashSet;

// empty
let mut set: HashSet<i32> = HashSet::new();

// with pre-allocated capacity
let mut set: HashSet<String> = HashSet::with_capacity(1000);

// from array (Rust 1.56+)
let set = HashSet::from([1, 2, 3, 4, 5]);

// from iterator
let set: HashSet<i32> = (1..=10).collect();

// from Vec (deduplica automáticamente)
let vec = vec![1, 2, 3, 2, 1, 4];
let set: HashSet<i32> = vec.into_iter().collect();
// set = {1, 2, 3, 4} — duplicados eliminados
```

Al igual que HashMap, `HashSet::new()` no asigna memoria hasta
la primera inserción.

---

## Inserción y pertenencia

```rust
let mut set = HashSet::new();

// insert: returns bool (true if element was new)
assert!(set.insert(42));        // new → true
assert!(!set.insert(42));       // duplicate → false

// contains: returns bool
assert!(set.contains(&42));
assert!(!set.contains(&99));

// get: returns Option<&T> — the actual stored reference
let val: Option<&i32> = set.get(&42);
assert_eq!(val, Some(&42));

// len, is_empty
assert_eq!(set.len(), 1);
assert!(!set.is_empty());
```

La diferencia clave con HashMap: `insert` devuelve `bool` (no
`Option<V>`), porque no hay valor que reemplazar — el elemento
ya está o no está.

### El patrón insert + check

`insert` devuelve `true` si el elemento era nuevo. Esto es
perfecto para detectar duplicados en una sola operación:

```rust
let mut seen = HashSet::new();
let items = vec![1, 2, 3, 2, 4, 3, 5];

for &item in &items {
    if !seen.insert(item) {
        println!("  duplicate: {}", item);
    }
}
// prints: duplicate: 2, duplicate: 3
```

---

## Eliminación

```rust
let mut set = HashSet::from([1, 2, 3, 4, 5]);

// remove: returns bool
assert!(set.remove(&3));        // found and removed → true
assert!(!set.remove(&99));      // not found → false

// take: removes and returns the value (owned)
let taken: Option<i32> = set.take(&4);
assert_eq!(taken, Some(4));

// retain: keep only elements matching predicate
set.retain(|&x| x % 2 == 0);
// set = {2}

// clear: remove all
set.clear();
assert!(set.is_empty());

// drain: remove all, yielding owned values
let mut set = HashSet::from([1, 2, 3]);
let drained: Vec<i32> = set.drain().collect();
// set is empty, drained has the elements
```

`take` es útil cuando necesitas el elemento **con ownership** de
vuelta (por ejemplo, para transferirlo a otra estructura).

---

## Operaciones de conjuntos

Estas son las operaciones que distinguen a HashSet de un simple
contenedor. Cada una devuelve un **iterador lazy** — no crea un
nuevo HashSet a menos que lo recojas con `collect()`.

### Unión: $A \cup B$

Todos los elementos que están en $A$ **o** en $B$ (o ambos):

```rust
let a = HashSet::from([1, 2, 3, 4]);
let b = HashSet::from([3, 4, 5, 6]);

// lazy iterator
for x in a.union(&b) {
    print!("{} ", x);
}
// 1 2 3 4 5 6 (order not guaranteed)

// collect into new HashSet
let union: HashSet<&i32> = a.union(&b).collect();

// alternative: bitwise OR operator
let union: HashSet<i32> = &a | &b;
```

### Intersección: $A \cap B$

Elementos que están en $A$ **y** en $B$:

```rust
let inter: HashSet<&i32> = a.intersection(&b).collect();
// {3, 4}

// operator
let inter: HashSet<i32> = &a & &b;
```

La intersección itera sobre el conjunto **más pequeño** y verifica
cada elemento en el más grande — optimización automática $O(\min(|A|, |B|))$.

### Diferencia: $A \setminus B$

Elementos que están en $A$ pero **no** en $B$:

```rust
let diff: HashSet<&i32> = a.difference(&b).collect();
// {1, 2}

// operator
let diff: HashSet<i32> = &a - &b;
```

### Diferencia simétrica: $A \triangle B$

Elementos que están en $A$ **o** en $B$ pero **no en ambos**:

```rust
let sym: HashSet<&i32> = a.symmetric_difference(&b).collect();
// {1, 2, 5, 6}

// operator
let sym: HashSet<i32> = &a ^ &b;
```

### Resumen de operadores

| Operación | Método | Operador | Resultado |
|-----------|--------|----------|-----------|
| Unión $A \cup B$ | `a.union(&b)` | `&a \| &b` | en $A$ o $B$ |
| Intersección $A \cap B$ | `a.intersection(&b)` | `&a & &b` | en $A$ y $B$ |
| Diferencia $A \setminus B$ | `a.difference(&b)` | `&a - &b` | en $A$, no en $B$ |
| Dif. simétrica $A \triangle B$ | `a.symmetric_difference(&b)` | `&a ^ &b` | en $A$ xor $B$ |

Los operadores (`|`, `&`, `-`, `^`) devuelven un `HashSet<T>`
nuevo (owned). Los métodos devuelven iteradores lazy sobre
referencias.

---

## Predicados de conjuntos

```rust
let a = HashSet::from([1, 2, 3]);
let b = HashSet::from([1, 2, 3, 4, 5]);
let c = HashSet::from([6, 7]);

// is_subset: A ⊆ B — every element of A is in B
assert!(a.is_subset(&b));
assert!(!b.is_subset(&a));

// is_superset: B ⊇ A — B contains every element of A
assert!(b.is_superset(&a));

// is_disjoint: A ∩ C = ∅ — no common elements
assert!(a.is_disjoint(&c));
assert!(!a.is_disjoint(&b));
```

Complejidad de los predicados:

| Predicado | Complejidad |
|-----------|-------------|
| `is_subset(other)` | $O(\|self\|)$ — itera self, verifica en other |
| `is_superset(other)` | $O(\|other\|)$ — equivale a `other.is_subset(self)` |
| `is_disjoint(other)` | $O(\min(\|self\|, \|other\|))$ — itera el menor |

---

## Iteración

```rust
let set = HashSet::from([10, 20, 30, 40]);

// iter: yields &T
for &val in &set {
    print!("{} ", val);
}

// into_iter: consumes set, yields T
for val in set {
    // val is owned
}

// No iter_mut — elements are immutable!
// (mutating an element could change its hash, breaking the set)
```

**No existe `iter_mut` para HashSet**. Si pudieras mutar un
elemento in-place, su hash podría cambiar, dejándolo en el bucket
equivocado. Para modificar elementos: remove, modify, re-insert.

---

## Complejidad de las operaciones de conjuntos

Sea $n = |A|$ y $m = |B|$:

| Operación | Complejidad | Notas |
|-----------|-------------|-------|
| `union` | $O(n + m)$ | Itera ambos, skip duplicados |
| `intersection` | $O(\min(n, m))$ | Itera el menor, lookup en el mayor |
| `difference` | $O(n)$ | Itera $A$, lookup en $B$ |
| `symmetric_difference` | $O(n + m)$ | Itera ambos |
| `is_subset` | $O(n)$ | Itera $A$, lookup en $B$ |
| `is_disjoint` | $O(\min(n, m))$ | Itera el menor |

Comparación con alternativas:

| Implementación | Union | Intersección | Membership |
|----------------|-------|--------------|------------|
| HashSet | $O(n + m)$ | $O(\min(n,m))$ | $O(1)$ |
| BTreeSet (ordenado) | $O(n + m)$ merge | $O(n + m)$ merge | $O(\log n)$ |
| Vec sorted | $O(n + m)$ merge | $O(n + m)$ merge | $O(\log n)$ binaria |
| Vec unsorted | $O(nm)$ | $O(nm)$ | $O(n)$ lineal |
| Bitset (universo pequeño) | $O(U/64)$ OR | $O(U/64)$ AND | $O(1)$ |

HashSet gana para membership e intersección cuando los conjuntos
son de tamaños muy diferentes. BTreeSet gana cuando se necesita
iteración ordenada o range queries.

---

## Patrones de uso

### Deduplicación

```rust
fn deduplicate<T: Hash + Eq + Clone>(items: &[T]) -> Vec<T> {
    let mut seen = HashSet::new();
    let mut result = Vec::new();
    for item in items {
        if seen.insert(item) {
            result.push(item.clone());
        }
    }
    result
}

// preserva el orden de primera aparición
let v = deduplicate(&[3, 1, 4, 1, 5, 9, 2, 6, 5]);
// [3, 1, 4, 5, 9, 2, 6]
```

Si el orden no importa, la forma más simple es:

```rust
let unique: HashSet<i32> = vec![3, 1, 4, 1, 5].into_iter().collect();
```

### Detección de ciclos

```rust
fn has_cycle(edges: &[(usize, usize)], start: usize) -> bool {
    let mut visited = HashSet::new();
    let mut stack = vec![start];

    while let Some(node) = stack.pop() {
        if !visited.insert(node) {
            return true;  // already visited → cycle
        }
        for &(from, to) in edges {
            if from == node {
                stack.push(to);
            }
        }
    }
    false
}
```

### Encontrar el primer duplicado

```rust
fn first_duplicate(items: &[i32]) -> Option<i32> {
    let mut seen = HashSet::new();
    for &item in items {
        if !seen.insert(item) {
            return Some(item);
        }
    }
    None
}
```

### Filtrar elementos comunes entre colecciones

```rust
fn common_words(text1: &str, text2: &str) -> HashSet<&str> {
    let words1: HashSet<&str> = text1.split_whitespace().collect();
    let words2: HashSet<&str> = text2.split_whitespace().collect();
    &words1 & &words2
}
```

### Tracking de estados visitados (BFS/DFS)

```rust
use std::collections::VecDeque;

fn bfs_levels(
    graph: &HashMap<i32, Vec<i32>>,
    start: i32,
) -> Vec<Vec<i32>> {
    let mut visited = HashSet::new();
    let mut queue = VecDeque::new();
    let mut levels = Vec::new();

    visited.insert(start);
    queue.push_back(start);

    while !queue.is_empty() {
        let mut level = Vec::new();
        for _ in 0..queue.len() {
            let node = queue.pop_front().unwrap();
            level.push(node);
            if let Some(neighbors) = graph.get(&node) {
                for &next in neighbors {
                    if visited.insert(next) {
                        queue.push_back(next);
                    }
                }
            }
        }
        levels.push(level);
    }
    levels
}
```

---

## Programa completo en C

```c
// hashset.c
// Set implementation and set operations in C (mirrors Rust HashSet)
// Compile: gcc -O2 -o hashset hashset.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// Integer hash set (open addressing, linear probing)
// ============================================================

#define LOAD_MAX 0.75
#define INITIAL_CAP 16
#define EMPTY_VAL  0x7FFFFFFF  // sentinel for empty
#define DELETED_VAL 0x7FFFFFFE // sentinel for deleted

typedef struct {
    int *data;
    char *state;  // 0=empty, 1=occupied, 2=deleted
    int capacity;
    int count;
} IntSet;

IntSet *is_create(int capacity) {
    IntSet *s = malloc(sizeof(IntSet));
    s->data = calloc(capacity, sizeof(int));
    s->state = calloc(capacity, sizeof(char));
    s->capacity = capacity;
    s->count = 0;
    return s;
}

void is_free(IntSet *s) {
    free(s->data);
    free(s->state);
    free(s);
}

static int is_hash(int key, int cap) {
    unsigned int h = (unsigned int)key;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h % cap;
}

static void is_resize(IntSet *s, int new_cap);

bool is_insert(IntSet *s, int val) {
    if ((double)(s->count + 1) / s->capacity > LOAD_MAX) {
        is_resize(s, s->capacity * 2);
    }

    int idx = is_hash(val, s->capacity);
    for (int i = 0; i < s->capacity; i++) {
        if (s->state[idx] == 0) {
            s->data[idx] = val;
            s->state[idx] = 1;
            s->count++;
            return true;
        }
        if (s->state[idx] == 1 && s->data[idx] == val) {
            return false;  // already exists
        }
        idx = (idx + 1) % s->capacity;
    }
    return false;
}

bool is_contains(IntSet *s, int val) {
    int idx = is_hash(val, s->capacity);
    for (int i = 0; i < s->capacity; i++) {
        if (s->state[idx] == 0) return false;
        if (s->state[idx] == 1 && s->data[idx] == val) return true;
        idx = (idx + 1) % s->capacity;
    }
    return false;
}

bool is_remove(IntSet *s, int val) {
    int idx = is_hash(val, s->capacity);
    for (int i = 0; i < s->capacity; i++) {
        if (s->state[idx] == 0) return false;
        if (s->state[idx] == 1 && s->data[idx] == val) {
            s->state[idx] = 2;
            s->count--;
            return true;
        }
        idx = (idx + 1) % s->capacity;
    }
    return false;
}

static void is_resize(IntSet *s, int new_cap) {
    int *old_data = s->data;
    char *old_state = s->state;
    int old_cap = s->capacity;

    s->data = calloc(new_cap, sizeof(int));
    s->state = calloc(new_cap, sizeof(char));
    s->capacity = new_cap;
    s->count = 0;

    for (int i = 0; i < old_cap; i++) {
        if (old_state[i] == 1) {
            is_insert(s, old_data[i]);
        }
    }
    free(old_data);
    free(old_state);
}

void is_print(IntSet *s, const char *label) {
    printf("  %s = {", label);
    bool first = true;
    for (int i = 0; i < s->capacity; i++) {
        if (s->state[i] == 1) {
            if (!first) printf(", ");
            printf("%d", s->data[i]);
            first = false;
        }
    }
    printf("} (n=%d)\n", s->count);
}

// set operations: return new set

IntSet *is_union(IntSet *a, IntSet *b) {
    IntSet *result = is_create(INITIAL_CAP);
    for (int i = 0; i < a->capacity; i++) {
        if (a->state[i] == 1) is_insert(result, a->data[i]);
    }
    for (int i = 0; i < b->capacity; i++) {
        if (b->state[i] == 1) is_insert(result, b->data[i]);
    }
    return result;
}

IntSet *is_intersection(IntSet *a, IntSet *b) {
    IntSet *result = is_create(INITIAL_CAP);
    // iterate smaller, lookup in larger
    IntSet *small = (a->count <= b->count) ? a : b;
    IntSet *large = (a->count <= b->count) ? b : a;
    for (int i = 0; i < small->capacity; i++) {
        if (small->state[i] == 1 && is_contains(large, small->data[i])) {
            is_insert(result, small->data[i]);
        }
    }
    return result;
}

IntSet *is_difference(IntSet *a, IntSet *b) {
    IntSet *result = is_create(INITIAL_CAP);
    for (int i = 0; i < a->capacity; i++) {
        if (a->state[i] == 1 && !is_contains(b, a->data[i])) {
            is_insert(result, a->data[i]);
        }
    }
    return result;
}

IntSet *is_symmetric_difference(IntSet *a, IntSet *b) {
    IntSet *result = is_create(INITIAL_CAP);
    for (int i = 0; i < a->capacity; i++) {
        if (a->state[i] == 1 && !is_contains(b, a->data[i])) {
            is_insert(result, a->data[i]);
        }
    }
    for (int i = 0; i < b->capacity; i++) {
        if (b->state[i] == 1 && !is_contains(a, b->data[i])) {
            is_insert(result, b->data[i]);
        }
    }
    return result;
}

bool is_subset(IntSet *a, IntSet *b) {
    for (int i = 0; i < a->capacity; i++) {
        if (a->state[i] == 1 && !is_contains(b, a->data[i])) {
            return false;
        }
    }
    return true;
}

bool is_disjoint(IntSet *a, IntSet *b) {
    IntSet *small = (a->count <= b->count) ? a : b;
    IntSet *large = (a->count <= b->count) ? b : a;
    for (int i = 0; i < small->capacity; i++) {
        if (small->state[i] == 1 && is_contains(large, small->data[i])) {
            return false;
        }
    }
    return true;
}

// ============================================================
// Demos
// ============================================================

void demo1_basic_api(void) {
    printf("=== Demo 1: basic set API ===\n");
    IntSet *s = is_create(INITIAL_CAP);

    printf("  insert(42): %s\n", is_insert(s, 42) ? "new" : "dup");
    printf("  insert(17): %s\n", is_insert(s, 17) ? "new" : "dup");
    printf("  insert(42): %s\n", is_insert(s, 42) ? "new" : "dup");
    printf("  insert(99): %s\n", is_insert(s, 99) ? "new" : "dup");

    printf("  contains(42): %s\n", is_contains(s, 42) ? "yes" : "no");
    printf("  contains(55): %s\n", is_contains(s, 55) ? "yes" : "no");

    printf("  remove(17): %s\n", is_remove(s, 17) ? "yes" : "no");
    printf("  remove(17): %s\n", is_remove(s, 17) ? "yes" : "no");

    is_print(s, "final");
    printf("\n");
    is_free(s);
}

void demo2_set_operations(void) {
    printf("=== Demo 2: set operations ===\n");
    IntSet *a = is_create(INITIAL_CAP);
    IntSet *b = is_create(INITIAL_CAP);

    int a_vals[] = {1, 2, 3, 4, 5};
    int b_vals[] = {3, 4, 5, 6, 7};

    for (int i = 0; i < 5; i++) is_insert(a, a_vals[i]);
    for (int i = 0; i < 5; i++) is_insert(b, b_vals[i]);

    is_print(a, "A");
    is_print(b, "B");

    IntSet *u = is_union(a, b);
    is_print(u, "A ∪ B");

    IntSet *inter = is_intersection(a, b);
    is_print(inter, "A ∩ B");

    IntSet *diff = is_difference(a, b);
    is_print(diff, "A \\ B");

    IntSet *sym = is_symmetric_difference(a, b);
    is_print(sym, "A △ B");

    printf("\n");
    is_free(a); is_free(b);
    is_free(u); is_free(inter); is_free(diff); is_free(sym);
}

void demo3_predicates(void) {
    printf("=== Demo 3: set predicates ===\n");
    IntSet *a = is_create(INITIAL_CAP);
    IntSet *b = is_create(INITIAL_CAP);
    IntSet *c = is_create(INITIAL_CAP);

    for (int i = 1; i <= 3; i++) is_insert(a, i);
    for (int i = 1; i <= 5; i++) is_insert(b, i);
    for (int i = 6; i <= 8; i++) is_insert(c, i);

    is_print(a, "A"); is_print(b, "B"); is_print(c, "C");

    printf("  A ⊆ B: %s\n", is_subset(a, b) ? "yes" : "no");
    printf("  B ⊆ A: %s\n", is_subset(b, a) ? "yes" : "no");
    printf("  A disjoint C: %s\n", is_disjoint(a, c) ? "yes" : "no");
    printf("  A disjoint B: %s\n", is_disjoint(a, b) ? "yes" : "no");
    printf("\n");

    is_free(a); is_free(b); is_free(c);
}

void demo4_deduplication(void) {
    printf("=== Demo 4: deduplication ===\n");
    int data[] = {5, 3, 8, 3, 2, 5, 9, 1, 8, 7, 2, 4, 1, 6, 9};
    int n = 15;

    IntSet *s = is_create(INITIAL_CAP);
    int unique = 0;
    int dups = 0;

    for (int i = 0; i < n; i++) {
        if (is_insert(s, data[i])) {
            unique++;
        } else {
            dups++;
        }
    }

    printf("  total=%d, unique=%d, duplicates=%d\n", n, unique, dups);
    is_print(s, "unique");
    printf("\n");
    is_free(s);
}

void demo5_two_array_intersection(void) {
    printf("=== Demo 5: find common elements of two arrays ===\n");
    int arr1[] = {1, 4, 7, 2, 9, 5, 3};
    int arr2[] = {3, 8, 5, 1, 6, 10, 7};
    int n1 = 7, n2 = 7;

    IntSet *s1 = is_create(INITIAL_CAP);
    for (int i = 0; i < n1; i++) is_insert(s1, arr1[i]);

    printf("  common elements: ");
    for (int i = 0; i < n2; i++) {
        if (is_contains(s1, arr2[i])) {
            printf("%d ", arr2[i]);
        }
    }
    printf("\n\n");
    is_free(s1);
}

void demo6_missing_element(void) {
    printf("=== Demo 6: find missing elements ===\n");
    // find numbers 1..10 not in the array
    int present[] = {2, 5, 7, 1, 9, 3};
    int n = 6;

    IntSet *s = is_create(INITIAL_CAP);
    for (int i = 0; i < n; i++) is_insert(s, present[i]);

    printf("  present: ");
    for (int i = 0; i < n; i++) printf("%d ", present[i]);
    printf("\n  missing from 1..10: ");
    for (int i = 1; i <= 10; i++) {
        if (!is_contains(s, i)) printf("%d ", i);
    }
    printf("\n\n");
    is_free(s);
}

int main(void) {
    demo1_basic_api();
    demo2_set_operations();
    demo3_predicates();
    demo4_deduplication();
    demo5_two_array_intersection();
    demo6_missing_element();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// hashset_rust.rs
// Comprehensive HashSet usage in Rust
// Run: rustc hashset_rust.rs && ./hashset_rust

use std::collections::{HashMap, HashSet};

fn demo1_construction() {
    println!("=== Demo 1: construction ===");

    let empty: HashSet<i32> = HashSet::new();
    println!("  new():          len={}, cap={}", empty.len(), empty.capacity());

    let prealloc: HashSet<i32> = HashSet::with_capacity(100);
    println!("  with_cap(100):  len={}, cap={}", prealloc.len(), prealloc.capacity());

    let from_arr = HashSet::from([1, 2, 3, 4, 5]);
    println!("  from([1..5]):   len={}", from_arr.len());

    let from_range: HashSet<i32> = (1..=10).collect();
    println!("  collect(1..10): len={}", from_range.len());

    // deduplication via collect
    let with_dups = vec![1, 2, 3, 2, 1, 4, 3, 5];
    let deduped: HashSet<i32> = with_dups.into_iter().collect();
    println!("  deduped [1,2,3,2,1,4,3,5]: {:?}\n", deduped);
}

fn demo2_insert_contains_remove() {
    println!("=== Demo 2: insert, contains, remove ===");
    let mut set = HashSet::new();

    println!("  insert(42): {}", set.insert(42));   // true (new)
    println!("  insert(17): {}", set.insert(17));   // true
    println!("  insert(42): {}", set.insert(42));   // false (dup)

    println!("  contains(42): {}", set.contains(&42));  // true
    println!("  contains(99): {}", set.contains(&99));  // false

    println!("  remove(17): {}", set.remove(&17));   // true
    println!("  remove(17): {}", set.remove(&17));   // false

    println!("  take(42): {:?}", set.take(&42));     // Some(42)
    println!("  len: {}\n", set.len());
}

fn demo3_set_operations() {
    println!("=== Demo 3: set operations ===");
    let a: HashSet<i32> = HashSet::from([1, 2, 3, 4, 5]);
    let b: HashSet<i32> = HashSet::from([3, 4, 5, 6, 7]);

    println!("  A = {:?}", a);
    println!("  B = {:?}", b);

    // union
    let union: HashSet<i32> = &a | &b;
    println!("  A ∪ B = {:?}", union);

    // intersection
    let inter: HashSet<i32> = &a & &b;
    println!("  A ∩ B = {:?}", inter);

    // difference
    let diff: HashSet<i32> = &a - &b;
    println!("  A \\ B = {:?}", diff);

    // symmetric difference
    let sym: HashSet<i32> = &a ^ &b;
    println!("  A △ B = {:?}", sym);

    // using methods (lazy iterators)
    let inter_count = a.intersection(&b).count();
    println!("\n  |A ∩ B| = {} (counted lazily, no allocation)\n", inter_count);
}

fn demo4_predicates() {
    println!("=== Demo 4: set predicates ===");
    let a = HashSet::from([1, 2, 3]);
    let b = HashSet::from([1, 2, 3, 4, 5]);
    let c = HashSet::from([6, 7, 8]);

    println!("  A = {:?}", a);
    println!("  B = {:?}", b);
    println!("  C = {:?}", c);

    println!("  A ⊆ B: {}", a.is_subset(&b));       // true
    println!("  B ⊆ A: {}", b.is_subset(&a));       // false
    println!("  B ⊇ A: {}", b.is_superset(&a));     // true
    println!("  A disjoint C: {}", a.is_disjoint(&c)); // true
    println!("  A disjoint B: {}", a.is_disjoint(&b)); // false
    println!();
}

fn demo5_deduplication_patterns() {
    println!("=== Demo 5: deduplication patterns ===");

    // simple dedup (loses order)
    let data = vec![5, 3, 8, 3, 2, 5, 9, 1, 8];
    let unique: HashSet<i32> = data.iter().cloned().collect();
    println!("  unordered dedup: {:?}", unique);

    // order-preserving dedup
    let mut seen = HashSet::new();
    let ordered: Vec<i32> = data.into_iter()
        .filter(|x| seen.insert(*x))
        .collect();
    println!("  ordered dedup:   {:?}", ordered);

    // find first duplicate
    let items = vec![1, 2, 3, 4, 2, 5];
    let mut check = HashSet::new();
    let first_dup = items.iter().find(|&&x| !check.insert(x));
    println!("  first duplicate: {:?}", first_dup);
    println!();
}

fn demo6_practical_patterns() {
    println!("=== Demo 6: practical patterns ===");

    // common words between two texts
    let text1 = "the quick brown fox jumps over the lazy dog";
    let text2 = "the lazy cat sleeps on the brown mat";

    let words1: HashSet<&str> = text1.split_whitespace().collect();
    let words2: HashSet<&str> = text2.split_whitespace().collect();

    let common: HashSet<&str> = &words1 & &words2;
    let only1: HashSet<&str> = &words1 - &words2;
    let only2: HashSet<&str> = &words2 - &words1;

    println!("  common words: {:?}", common);
    println!("  only in text1: {:?}", only1);
    println!("  only in text2: {:?}", only2);

    // missing numbers: find 1..=n not in array
    let present: HashSet<i32> = vec![2, 5, 7, 1, 9, 3].into_iter().collect();
    let full: HashSet<i32> = (1..=10).collect();
    let missing: HashSet<i32> = &full - &present;
    println!("  missing from 1..10: {:?}", missing);
    println!();
}

fn demo7_with_custom_types() {
    println!("=== Demo 7: custom types in HashSet ===");

    #[derive(Hash, Eq, PartialEq, Debug)]
    struct Point { x: i32, y: i32 }

    let mut visited: HashSet<Point> = HashSet::new();

    let path = vec![
        Point { x: 0, y: 0 },
        Point { x: 1, y: 0 },
        Point { x: 1, y: 1 },
        Point { x: 0, y: 0 },  // revisit
    ];

    for p in path {
        if !visited.insert(p) {
            println!("  revisited a point!");
        }
    }
    println!("  unique points visited: {}", visited.len());

    // with String keys
    let mut tags: HashSet<String> = HashSet::new();
    tags.insert("rust".to_string());
    tags.insert("systems".to_string());

    // lookup with &str (Borrow trait)
    assert!(tags.contains("rust"));  // &str works for String set
    println!("  tags.contains(\"rust\"): true (Borrow trait)\n");
}

fn demo8_retain_and_drain() {
    println!("=== Demo 8: retain and drain ===");

    let mut nums: HashSet<i32> = (1..=20).collect();
    println!("  initial: {} elements", nums.len());

    // retain only primes (simple check)
    nums.retain(|&n| {
        if n < 2 { return false; }
        (2..=(n as f64).sqrt() as i32).all(|d| n % d != 0)
    });
    let mut primes: Vec<&i32> = nums.iter().collect();
    primes.sort();
    println!("  primes in 1..20: {:?}", primes);

    // drain
    let drained: Vec<i32> = nums.drain().collect();
    println!("  drained {} primes, set.len()={}", drained.len(), nums.len());
    println!();
}

fn demo9_set_algebra_chain() {
    println!("=== Demo 9: chained set algebra ===");

    let students_math = HashSet::from(["Alice", "Bob", "Carol", "Dave"]);
    let students_physics = HashSet::from(["Bob", "Carol", "Eve", "Frank"]);
    let students_cs = HashSet::from(["Alice", "Carol", "Eve", "Grace"]);

    // students in all three
    let all_three: HashSet<&&str> = students_math.intersection(&students_physics)
        .filter(|s| students_cs.contains(*s))
        .collect();
    println!("  in all three: {:?}", all_three);

    // students in exactly one subject
    let in_math_only = &(&students_math - &students_physics) - &students_cs;
    let in_phys_only = &(&students_physics - &students_math) - &students_cs;
    let in_cs_only = &(&students_cs - &students_math) - &students_physics;

    println!("  math only:    {:?}", in_math_only);
    println!("  physics only: {:?}", in_phys_only);
    println!("  CS only:      {:?}", in_cs_only);

    // total unique students
    let all: HashSet<&str> = &(&students_math | &students_physics) | &students_cs;
    println!("  total unique:  {}", all.len());
    println!();
}

fn demo10_hashset_vs_vec() {
    println!("=== Demo 10: HashSet vs Vec for membership ===");

    let n = 10_000;
    let set: HashSet<i32> = (0..n).collect();
    let vec: Vec<i32> = (0..n).collect();

    // measure lookup time
    let start = std::time::Instant::now();
    let mut found_set = 0;
    for i in 0..n {
        if set.contains(&i) { found_set += 1; }
    }
    let set_time = start.elapsed();

    let start = std::time::Instant::now();
    let mut found_vec = 0;
    for i in 0..n {
        if vec.contains(&i) { found_vec += 1; }
    }
    let vec_time = start.elapsed();

    println!("  n = {}", n);
    println!("  HashSet: {:?} ({} found)", set_time, found_set);
    println!("  Vec:     {:?} ({} found)", vec_time, found_vec);
    println!("  Vec/Set ratio: {:.0}x slower",
             vec_time.as_nanos() as f64 / set_time.as_nanos() as f64);
    println!();
}

fn main() {
    demo1_construction();
    demo2_insert_contains_remove();
    demo3_set_operations();
    demo4_predicates();
    demo5_deduplication_patterns();
    demo6_practical_patterns();
    demo7_with_custom_types();
    demo8_retain_and_drain();
    demo9_set_algebra_chain();
    demo10_hashset_vs_vec();
}
```

---

## Ejercicios

### Ejercicio 1 — Operaciones con diagramas de Venn

Dados $A = \{1, 2, 3, 4, 5\}$ y $B = \{4, 5, 6, 7, 8\}$, calcula
mentalmente y luego verifica con código:
a) $A \cup B$
b) $A \cap B$
c) $A \setminus B$
d) $B \setminus A$
e) $A \triangle B$

<details><summary>a) $A \cup B$</summary>

$\{1, 2, 3, 4, 5, 6, 7, 8\}$ — 8 elementos.

Todos los de $A$ más los de $B$ que no están en $A$ (6, 7, 8).

</details>

<details><summary>b-e) Resto de operaciones</summary>

b) $A \cap B = \{4, 5\}$ — 2 elementos.

c) $A \setminus B = \{1, 2, 3\}$ — en $A$ pero no en $B$.

d) $B \setminus A = \{6, 7, 8\}$ — en $B$ pero no en $A$.

e) $A \triangle B = \{1, 2, 3, 6, 7, 8\}$ — equivale a
$(A \setminus B) \cup (B \setminus A)$, o $(A \cup B) \setminus (A \cap B)$.

Nota: $|A \cup B| = |A| + |B| - |A \cap B| = 5 + 5 - 2 = 8$ ✓

</details>

---

### Ejercicio 2 — Complejidad de operaciones

¿Cuál es la complejidad de `a.intersection(&b).count()` si
$|A| = 100$ y $|B| = 1000000$?

<details><summary>¿Es O(100) o O(1000000)?</summary>

$O(100)$. Rust optimiza iterando sobre el conjunto **más pequeño**
y verificando pertenencia en el más grande. Cada lookup en $B$ es
$O(1)$ amortizado, así que el total es $O(\min(|A|, |B|)) = O(100)$.

Si se iterara sobre $B$, sería $O(1000000)$ — 10000× más lento.
La optimización es automática en la implementación de la
biblioteca estándar.

</details>

<details><summary>¿Y `&a - &b` (diferencia)?</summary>

$O(|A|) = O(100)$. La diferencia siempre itera sobre el primer
operando (A) y verifica en el segundo (B). No se puede optimizar
más porque necesitamos verificar **cada** elemento de A.

</details>

---

### Ejercicio 3 — Deduplicar preservando orden

Implementa `dedup_ordered` que reciba un `Vec<i32>` y devuelva
un `Vec<i32>` sin duplicados, **preservando el orden de primera
aparición**. ¿Por qué `collect()` a HashSet no sirve aquí?

<details><summary>¿Por qué HashSet pierde el orden?</summary>

`HashSet` no mantiene orden de inserción. Al convertir de HashSet
a Vec, el orden depende de los hashes internos (esencialmente
aleatorio). Para preservar orden se necesita un `Vec` o
`IndexSet` (de la crate `indexmap`).

</details>

<details><summary>Implementación</summary>

```rust
fn dedup_ordered(v: Vec<i32>) -> Vec<i32> {
    let mut seen = HashSet::new();
    v.into_iter().filter(|x| seen.insert(*x)).collect()
}
```

El truco: `seen.insert(x)` devuelve `true` si es nuevo. `filter`
mantiene solo los `true` → primera aparición de cada elemento.
Complejidad: $O(n)$.

</details>

---

### Ejercicio 4 — Verificar propiedad de conjuntos

Demuestra empíricamente que para cualquier $A$, $B$:

$$|A \cup B| = |A| + |B| - |A \cap B|$$

Genera 1000 pares de conjuntos aleatorios y verifica.

<details><summary>¿Siempre se cumple?</summary>

Sí, siempre. Es la **inclusión-exclusión** para dos conjuntos. La
unión cuenta cada elemento una vez; si lo contamos desde $|A|$ y
$|B|$, los que están en ambos se cuentan dos veces, así que
restamos $|A \cap B|$.

También se cumple:
$|A \triangle B| = |A \cup B| - |A \cap B| = |A| + |B| - 2|A \cap B|$

</details>

<details><summary>Código de verificación</summary>

```rust
use rand::Rng;
let mut rng = rand::thread_rng();

for _ in 0..1000 {
    let a: HashSet<i32> = (0..rng.gen_range(0..50))
        .map(|_| rng.gen_range(0..100)).collect();
    let b: HashSet<i32> = (0..rng.gen_range(0..50))
        .map(|_| rng.gen_range(0..100)).collect();

    let union = (&a | &b).len();
    let inter = (&a & &b).len();
    assert_eq!(union, a.len() + b.len() - inter);
}
```

</details>

---

### Ejercicio 5 — is_subset eficiente

¿Por qué `a.is_subset(&b)` verifica primero `a.len() <= b.len()`
antes de iterar?

<details><summary>¿Qué optimiza esa verificación?</summary>

Si $|A| > |B|$, es **imposible** que $A \subseteq B$ — un
conjunto no puede ser subconjunto de uno más pequeño. El check
`len()` es $O(1)$ y evita la iteración $O(|A|)$ completa.

Es un **early return** extremadamente barato que filtra un caso
trivial.

</details>

<details><summary>¿Qué tan frecuente es ese caso?</summary>

Muy frecuente en la práctica. Si estás verificando si un conjunto
de permisos del usuario ($|A| = 10$) es subconjunto de permisos
requeridos ($|B| = 3$), el check falla inmediatamente sin
comparar ningún elemento.

</details>

---

### Ejercicio 6 — HashSet vs BTreeSet

Implementa la misma operación de intersección con `HashSet` y
`BTreeSet`. Para $A = \{1..1000\}$ y $B = \{500..1500\}$, mide
el tiempo de 10000 intersecciones.

<details><summary>¿Cuál es más rápido para intersección?</summary>

Para este caso ($|A| = 1000$, $|B| = 1000$), `HashSet` es
típicamente 2-5× más rápido. La intersección de HashSet itera
sobre el menor ($O(500)$ lookups de $O(1)$), mientras que BTreeSet
hace merge de dos iteradores ordenados ($O(|A| + |B|) = O(2000)$).

Pero BTreeSet gana cuando los conjuntos están en memoria contigua
y el merge aprovecha prefetching secuencial.

</details>

<details><summary>¿Cuándo preferir BTreeSet?</summary>

- Necesitas iteración **ordenada** (e.g., imprimir en orden).
- Necesitas **range queries** (`range(10..20)`).
- Los conjuntos son pequeños ($< 20$ elementos).
- Necesitas un set de `f64` (BTreeSet requiere `Ord`, no `Hash`).

Para membership testing puro con $n > 50$, HashSet casi siempre
gana.

</details>

---

### Ejercicio 7 — Problema: encontrar pares con diferencia k

Dado un array de enteros y un valor $k$, encuentra todos los pares
$(a, b)$ donde $b - a = k$. Usa HashSet para lograr $O(n)$.

<details><summary>¿Cómo usar HashSet para O(n)?</summary>

```rust
fn pairs_with_diff(nums: &[i32], k: i32) -> Vec<(i32, i32)> {
    let set: HashSet<i32> = nums.iter().cloned().collect();
    let mut result = Vec::new();
    for &num in nums {
        if set.contains(&(num + k)) {
            result.push((num, num + k));
        }
    }
    result
}
```

Para cada elemento $a$, verificamos si $a + k$ existe en el set.
El lookup es $O(1)$, y hacemos $n$ lookups → $O(n)$ total.

Sin HashSet, la alternativa es ordenar + búsqueda binaria
($O(n \log n)$) o fuerza bruta ($O(n^2)$).

</details>

<details><summary>¿Cómo evitar pares duplicados?</summary>

Si el array tiene duplicados, el resultado puede tener pares
repetidos. Solución: recoger en un `HashSet<(i32, i32)>`:

```rust
let result: HashSet<(i32, i32)> = nums.iter()
    .filter(|&&n| set.contains(&(n + k)))
    .map(|&n| (n, n + k))
    .collect();
```

</details>

---

### Ejercicio 8 — Álgebra de conjuntos: tres conjuntos

Dados estudiantes inscritos en tres materias:
- Math: {Alice, Bob, Carol, Dave}
- Physics: {Bob, Carol, Eve}
- CS: {Carol, Dave, Eve, Frank}

Calcula:
a) Inscritos en las tres materias.
b) Inscritos en exactamente una materia.
c) Inscritos en al menos dos materias.

<details><summary>a) Las tres materias</summary>

$M \cap P \cap C = \{\text{Carol}\}$ — solo Carol está en las tres.

</details>

<details><summary>b) Exactamente una materia</summary>

- Solo Math: $M \setminus (P \cup C) = \{\text{Alice}\}$
- Solo Physics: $P \setminus (M \cup C) = \emptyset$
- Solo CS: $C \setminus (M \cup P) = \{\text{Frank}\}$

Total: {Alice, Frank} — 2 estudiantes.

</details>

<details><summary>c) Al menos dos materias</summary>

Los que están en $\geq 2$: Bob (M+P), Carol (M+P+C), Dave (M+C),
Eve (P+C).

Total: {Bob, Carol, Dave, Eve} — 4 estudiantes.

Se puede calcular como: todos $-$ los de exactamente una =
$6 - 2 = 4$.

</details>

---

### Ejercicio 9 — No iter_mut: por qué

Explica por qué HashSet no tiene `iter_mut()` pero HashMap sí
tiene `values_mut()`. ¿Qué se rompería si pudiera mutar los
elementos in-place?

<details><summary>¿Qué se rompería?</summary>

Si se pudiera mutar un elemento del set, su **hash podría
cambiar**. El elemento estaría almacenado en un bucket que
corresponde al hash viejo, pero su hash nuevo apuntaría a un
bucket diferente.

Resultado: `contains()` reportaría `false` para un elemento que
**sí está** en el set. O peor, dos elementos "iguales" podrían
coexistir si uno fue mutado para ser igual al otro.

HashMap tiene `values_mut()` porque mutar un **valor** no afecta
el hash (que solo depende de la clave). Las claves del HashMap
tampoco son mutables.

</details>

<details><summary>¿Cómo modificar un elemento entonces?</summary>

Remove, modify, re-insert:

```rust
if let Some(mut item) = set.take(&old_value) {
    item = modify(item);
    set.insert(item);
}
```

`take` devuelve el elemento owned, permitiendo modificarlo fuera
del set. `insert` lo coloca en el bucket correcto con el hash
nuevo.

</details>

---

### Ejercicio 10 — Rendimiento: cuándo NO usar HashSet

Para cada escenario, ¿es HashSet la mejor opción?

a) Verificar si un número está en un rango $[1, 1000000]$.
b) Mantener un conjunto de 5 flags booleanos.
c) Encontrar el mínimo de un conjunto dinámico.
d) Verificar unicidad en un stream de 10 millones de strings.

<details><summary>a) Rango numérico</summary>

**No**. Basta con `n >= 1 && n <= 1000000`. Un HashSet con un
millón de elementos usaría ~20 MB para un check que es una
comparación de dos enteros.

</details>

<details><summary>b) 5 flags</summary>

**No**. Con 5 elementos, un array o bitfield es más eficiente.
Un `u8` con bits individuales ocupa 1 byte. Un HashSet tiene
overhead mínimo de ~100 bytes (metadata + array de control).

</details>

<details><summary>c) Mínimo dinámico</summary>

**No**. HashSet no ofrece `min()` eficiente — requiere iterar
todos los elementos $O(n)$. Usar `BTreeSet` ($O(\log n)$ para
`first()`) o un min-heap.

</details>

<details><summary>d) 10M strings</summary>

**Sí**. Este es el caso ideal para HashSet: membership testing
en un conjunto grande. 10M lookups de $O(1)$ cada uno. La
alternativa (ordenar + binaria) sería $O(n \log n)$ para ordenar
y luego $O(\log n)$ por lookup — más lento.

Consideración: si la memoria es limitada, un **bloom filter**
podría ser mejor (falsos positivos permitidos, ~1.5 bytes/elemento
vs ~50+ bytes/elemento del HashSet).

</details>
