# HashMap en Rust

## Objetivo

Dominar `std::collections::HashMap` como la estructura de mapeo
clave-valor principal de Rust:

- **Arquitectura interna**: Swiss Table (hashbrown), grupos de 16,
  bytes de control, búsqueda SIMD.
- **Traits requeridos**: `Hash + Eq` para claves; por qué `PartialEq`
  no basta.
- **API completa**: construcción, inserción, acceso, eliminación,
  iteración, capacidad.
- **Entry API**: el patrón `entry().or_insert()` para evitar
  búsquedas dobles.
- **Hashers personalizados**: reemplazar SipHash por hashers más
  rápidos o especializados.
- **Ownership**: cómo HashMap interactúa con el sistema de
  ownership de Rust.
- Referencia: T02 de S01 cubrió `Hash` trait y SipHash a nivel de
  funciones hash. Aquí nos enfocamos en la API y el uso práctico.

---

## Arquitectura: Swiss Table (hashbrown)

Desde Rust 1.36 (2019), `HashMap` usa **hashbrown**, una
implementación de Swiss Table desarrollada por Google para
Abseil (C++) y portada a Rust. Es fundamentalmente diferente de
las tablas hash clásicas que estudiamos en S01-S02:

### Estructura de memoria

```
Array de control bytes (1 byte por slot):
  [H₇|H₇|E |H₇|H₇|D |H₇|E |H₇|H₇|H₇|H₇|E |H₇|H₇|E ]
   0   1  2  3  4  5  6  7  8  9  10 11 12 13 14 15

  H₇ = 7 bits altos del hash (0x00-0x7F) → slot OCCUPIED
  E  = 0xFF → slot EMPTY
  D  = 0x80 → slot DELETED (tombstone)

Array de slots (key-value pairs):
  [(k,v), (k,v), _, (k,v), (k,v), _, (k,v), _, ...]
```

Los control bytes forman **grupos de 16** que se examinan en
paralelo con una instrucción SIMD (SSE2 en x86, NEON en ARM, o
emulación por software). Para buscar una clave:

1. Calcular $h = \text{hash}(k)$.
2. Ir al grupo $g = h \bmod (\text{num\_groups})$.
3. Crear un vector SIMD con el byte $h_7$ (7 bits altos de $h$).
4. Comparar con los 16 control bytes del grupo → bitmask de
   matches.
5. Para cada match, comparar la clave completa.
6. Si no se encuentra y hay EMPTY en el grupo → no existe.
7. Si solo hay DELETED y OCCUPIED → probar siguiente grupo.

### Por qué es rápido

| Aspecto | Sonda lineal clásica | Swiss Table |
|---------|---------------------|-------------|
| Comparaciones por slot | 1 clave completa | 1 byte de control |
| Slots examinados en paralelo | 1 | 16 (SIMD) |
| Cache misses típicos | 1 por sondeo | 1 por grupo de 16 |
| Tombstone overhead | Alto (infla cadenas) | Bajo (filtrado por SIMD) |
| $\alpha_{\max}$ práctico | 0.70-0.75 | 0.875 (7/8) |

El secreto es que el 90%+ de las búsquedas se resuelven en el
**primer grupo** — sin acceder a las claves almacenadas. Solo
cuando el byte $h_7$ coincide se compara la clave completa.

---

## Traits requeridos: Hash + Eq

Para usar un tipo como clave de `HashMap`, debe implementar:

- `Hash`: para calcular el índice del bucket.
- `Eq`: para comparar claves cuando hay colisión de hash.

`PartialEq` no basta porque `HashMap` requiere que la igualdad
sea una **relación de equivalencia** (reflexiva, simétrica,
transitiva). `PartialEq` permite tipos donde `a != a` (como
`f64::NAN`), lo que rompería la búsqueda.

### El contrato Hash + Eq

Si `a == b` entonces `hash(a) == hash(b)`. La inversa no tiene
que cumplirse (colisiones son permitidas).

Violar este contrato produce comportamiento **lógicamente
incorrecto** (no unsafe, pero bugs silenciosos):

```rust
// BAD: Hash and Eq disagree
struct Bad {
    id: u32,
    name: String,
}

impl PartialEq for Bad {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id  // compare only id
    }
}
impl Eq for Bad {}

impl Hash for Bad {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.name.hash(state);  // hash only name — WRONG!
        // Two Bads with same id but different name:
        //   eq() says equal, but hash() gives different values
    }
}
```

### Tipos que implementan Hash

| Tipo | Hash | Notas |
|------|------|-------|
| `i32`, `u64`, etc. | Sí | Todos los enteros |
| `bool`, `char` | Sí | |
| `String`, `&str` | Sí | Hash del contenido UTF-8 |
| `Vec<T: Hash>` | Sí | Hash de todos los elementos |
| `(A, B)` tuples | Sí (si A, B: Hash) | Hash de cada campo |
| `f32`, `f64` | **No** | `NaN != NaN` viola Eq |
| `HashMap`, `HashSet` | **No** | No determinista (orden) |

---

## Construcción

```rust
use std::collections::HashMap;

// empty
let mut map: HashMap<String, i32> = HashMap::new();

// with pre-allocated capacity (avoids resizes)
let mut map: HashMap<String, i32> = HashMap::with_capacity(1000);

// from iterator of tuples
let map: HashMap<&str, i32> = vec![("a", 1), ("b", 2)].into_iter().collect();

// from arrays (Rust 1.56+)
let map = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);
```

`HashMap::new()` **no asigna memoria** hasta la primera inserción.
Esto es una optimización — crear un HashMap vacío cuesta cero.

---

## Inserción: insert, entry

### insert

```rust
let mut scores: HashMap<String, i32> = HashMap::new();

// insert returns Option<V> — the old value if key existed
let old = scores.insert("Alice".to_string(), 100);
assert_eq!(old, None);  // key was new

let old = scores.insert("Alice".to_string(), 95);
assert_eq!(old, Some(100));  // replaced, returns old value
```

`insert` siempre reemplaza. Si necesitas insertar **solo si la
clave no existe**, usa la Entry API.

### Entry API

La Entry API resuelve el problema de la **búsqueda doble**:

```rust
// WITHOUT entry API — two lookups:
if !map.contains_key("key") {   // lookup 1
    map.insert("key", value);    // lookup 2
}

// WITH entry API — one lookup:
map.entry("key").or_insert(value);
```

El método `entry()` devuelve un `Entry` enum:

```rust
pub enum Entry<'a, K, V> {
    Occupied(OccupiedEntry<'a, K, V>),
    Vacant(VacantEntry<'a, K, V>),
}
```

#### Variantes de Entry

```rust
let mut word_count: HashMap<String, i32> = HashMap::new();
let text = "the cat sat on the mat the cat";

// or_insert: insert default if vacant
for word in text.split_whitespace() {
    let count = word_count.entry(word.to_string()).or_insert(0);
    *count += 1;
}
// {"the": 3, "cat": 2, "sat": 1, "on": 1, "mat": 1}

// or_insert_with: lazy computation (closure called only if vacant)
map.entry(key).or_insert_with(|| expensive_default());

// or_insert_with_key: closure receives the key
map.entry(key).or_insert_with_key(|k| k.len() as i32);

// or_default: uses Default trait
let counts: HashMap<String, Vec<i32>> = HashMap::new();
counts.entry("key".into()).or_default().push(42);

// and_modify: modify existing value
map.entry(key)
    .and_modify(|v| *v += 1)
    .or_insert(1);
```

#### Patrón: contador de frecuencias

```rust
fn word_frequencies(text: &str) -> HashMap<&str, usize> {
    let mut freq = HashMap::new();
    for word in text.split_whitespace() {
        *freq.entry(word).or_insert(0) += 1;
    }
    freq
}
```

Este es el uso más común de Entry API y el ejemplo canónico de
HashMap en Rust.

#### Patrón: agrupación (group by)

```rust
fn group_by_length(words: &[&str]) -> HashMap<usize, Vec<&str>> {
    let mut groups: HashMap<usize, Vec<&str>> = HashMap::new();
    for &word in words {
        groups.entry(word.len()).or_default().push(word);
    }
    groups
}
```

#### Patrón: cache / memoización

```rust
fn fibonacci_memo(n: u64, cache: &mut HashMap<u64, u64>) -> u64 {
    if n <= 1 { return n; }
    if let Some(&val) = cache.get(&n) {
        return val;
    }
    let result = fibonacci_memo(n - 1, cache)
               + fibonacci_memo(n - 2, cache);
    cache.insert(n, result);
    result
}
```

---

## Acceso: get, get_mut, indexación

```rust
let mut map = HashMap::from([("a", 1), ("b", 2)]);

// get: returns Option<&V>
let val: Option<&i32> = map.get("a");
assert_eq!(val, Some(&1));
assert_eq!(map.get("z"), None);

// get returns reference — doesn't move
let val = map.get("a").unwrap();  // &i32

// get_mut: returns Option<&mut V>
if let Some(val) = map.get_mut("a") {
    *val += 10;
}

// get_key_value: returns Option<(&K, &V)>
let pair = map.get_key_value("a");
assert_eq!(pair, Some((&"a", &11)));

// contains_key: returns bool
assert!(map.contains_key("a"));

// index operator []: panics if key missing
let val = map["a"];  // works, returns 11
// let val = map["z"];  // PANIC! use .get() instead
```

**Regla**: siempre preferir `get()` sobre `[]` excepto cuando
estás **seguro** de que la clave existe (por ejemplo, justo
después de `insert`).

---

## Eliminación: remove, remove_entry, retain

```rust
let mut map = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);

// remove: returns Option<V>
let removed = map.remove("a");
assert_eq!(removed, Some(1));
assert_eq!(map.remove("z"), None);

// remove_entry: returns Option<(K, V)> — gives back the key too
let pair = map.remove_entry("b");
assert_eq!(pair, Some(("b", 2)));

// retain: keep only entries matching predicate
map.insert("d", 4);
map.insert("e", 5);
map.retain(|_k, v| *v > 3);
// map = {"d": 4, "e": 5}
```

---

## Iteración

`HashMap` no garantiza orden de iteración. El orden puede cambiar
entre ejecuciones (debido a RandomState) y entre inserciones
(debido a resizes).

```rust
let map = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);

// iter: yields (&K, &V)
for (key, value) in &map {
    println!("{}: {}", key, value);
}

// iter_mut: yields (&K, &mut V) — keys are immutable!
let mut map = HashMap::from([("a", 1), ("b", 2)]);
for (_, value) in &mut map {
    *value *= 10;
}

// into_iter: consumes map, yields (K, V)
for (key, value) in map {
    // key and value are owned here
    println!("{}: {}", key, value);
}

// keys: yields &K
let keys: Vec<&&str> = map.keys().collect();

// values: yields &V
let sum: i32 = map.values().sum();

// values_mut: yields &mut V
for v in map.values_mut() {
    *v += 1;
}

// drain: removes all, yields (K, V) — like into_iter but borrows &mut
let pairs: Vec<_> = map.drain().collect();
// map is now empty but still allocated
```

---

## Capacidad y memoria

```rust
let mut map: HashMap<i32, i32> = HashMap::new();
assert_eq!(map.capacity(), 0);  // no allocation yet

map.insert(1, 1);
println!("cap after 1 insert: {}", map.capacity());  // ~3

// reserve: ensure space for N more elements without resize
map.reserve(100);

// shrink_to_fit: reduce to minimum capacity
map.shrink_to_fit();

// shrink_to: reduce to hold at least N
map.shrink_to(10);

// len and is_empty
assert_eq!(map.len(), 1);
assert!(!map.is_empty());

// clear: remove all elements, keep allocated memory
map.clear();
assert_eq!(map.len(), 0);
// capacity unchanged — memory still allocated
```

### Diferencia entre clear y drop

- `clear()`: elimina entradas pero **mantiene** la memoria.
  Útil si vas a re-llenar la tabla.
- `drop(map)` (o salir del scope): libera toda la memoria.

---

## Ownership y borrowing

### Claves: ownership transferido

```rust
let mut map = HashMap::new();
let key = String::from("hello");

map.insert(key, 42);
// key is MOVED into the map — can't use it anymore
// println!("{}", key);  // ERROR: value used after move

// workaround 1: clone
let key2 = String::from("world");
map.insert(key2.clone(), 99);
println!("{}", key2);  // OK — cloned

// workaround 2: use &str keys
let mut map: HashMap<&str, i32> = HashMap::new();
map.insert("hello", 42);  // &str is Copy
```

### Lookup con tipos prestados

`HashMap<String, V>` permite buscar con `&str` gracias al trait
`Borrow`:

```rust
let mut map: HashMap<String, i32> = HashMap::new();
map.insert("hello".to_string(), 42);

// get accepts &str even though key is String
let val = map.get("hello");       // &str → Borrow<str> for String
assert_eq!(val, Some(&42));

// this works because String: Borrow<str>
```

Esta ergonomía evita crear un `String` temporal solo para buscar.

---

## Hashers personalizados

El hasher por defecto (`SipHash-1-3` via `RandomState`) prioriza
seguridad (resistencia a HashDoS) sobre velocidad. Para código
donde las claves no son adversariales (e.g., IDs numéricos
internos), un hasher más rápido puede mejorar el rendimiento
significativamente:

```rust
use std::hash::BuildHasherDefault;
use std::collections::HashMap;

// Example with FxHash (from rustc-hash crate)
// use rustc_hash::FxHashMap;
// let mut map: FxHashMap<i32, i32> = FxHashMap::default();

// Or manually specify hasher type
// use rustc_hash::FxBuildHasher;
// let mut map: HashMap<i32, i32, BuildHasherDefault<FxBuildHasher>>
//     = HashMap::with_hasher(BuildHasherDefault::default());
```

### Hashers populares

| Crate | Hasher | Velocidad | Seguridad | Uso ideal |
|-------|--------|-----------|-----------|-----------|
| (default) | SipHash-1-3 | media | alta | datos de usuario, redes |
| `rustc-hash` | FxHash | muy rápida | ninguna | compiladores, herramientas internas |
| `ahash` | AHash | muy rápida | media (keyed) | propósito general rápido |
| `fnv` | FNV-1a | rápida | ninguna | claves pequeñas |
| `twox-hash` | xxHash | muy rápida | ninguna | datos grandes |

`ahash` es notable porque es rápido **y** usa claves aleatorias
(similar a SipHash pero más rápido), ofreciendo un buen equilibrio.
La crate `hashbrown` expuesta por `std` puede usar `ahash` si se
compila con la feature correspondiente.

### Cuándo cambiar el hasher

- **Sí**: benchmarks muestran que hashing es el bottleneck;
  claves no provienen de input externo; código interno/compilador.
- **No**: claves vienen de la red/usuario (DoS); la diferencia de
  rendimiento es marginal; código de librería pública (dejar que
  el usuario elija).

---

## Patrones avanzados

### HashMap como grafo de adyacencia

```rust
fn build_graph(edges: &[(u32, u32)]) -> HashMap<u32, Vec<u32>> {
    let mut graph: HashMap<u32, Vec<u32>> = HashMap::new();
    for &(from, to) in edges {
        graph.entry(from).or_default().push(to);
        graph.entry(to).or_default().push(from);  // undirected
    }
    graph
}
```

### Two-sum con HashMap

```rust
fn two_sum(nums: &[i32], target: i32) -> Option<(usize, usize)> {
    let mut seen: HashMap<i32, usize> = HashMap::new();
    for (i, &num) in nums.iter().enumerate() {
        let complement = target - num;
        if let Some(&j) = seen.get(&complement) {
            return Some((j, i));
        }
        seen.insert(num, i);
    }
    None
}
```

### Invertir un HashMap

```rust
fn invert<K, V>(map: &HashMap<K, V>) -> HashMap<&V, &K>
where
    V: Eq + Hash,
    K: Eq + Hash,
{
    map.iter().map(|(k, v)| (v, k)).collect()
}
```

### Merge de dos HashMaps

```rust
fn merge(a: HashMap<String, i32>, b: HashMap<String, i32>) -> HashMap<String, i32> {
    let mut result = a;
    for (k, v) in b {
        result.entry(k)
            .and_modify(|existing| *existing += v)
            .or_insert(v);
    }
    result
}
```

---

## Programa completo en C

Este tópico es sobre HashMap de Rust. El programa en C implementa
una tabla hash genérica con API similar para ilustrar lo que
HashMap abstrae:

```c
// hashmap_api.c
// Hash table with string keys and generic API (mirrors Rust HashMap)
// Compile: gcc -O2 -o hashmap_api hashmap_api.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// Generic hash table (string keys, int values)
// ============================================================

#define LOAD_MAX 0.75
#define INITIAL_CAP 16

typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    int capacity;
    int count;
} StrHashMap;

static unsigned int djb2(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = hash * 33 + c;
    }
    return hash;
}

StrHashMap *shm_new(void) {
    StrHashMap *m = malloc(sizeof(StrHashMap));
    m->capacity = INITIAL_CAP;
    m->buckets = calloc(m->capacity, sizeof(Entry *));
    m->count = 0;
    return m;
}

StrHashMap *shm_with_capacity(int cap) {
    StrHashMap *m = malloc(sizeof(StrHashMap));
    m->capacity = cap;
    m->buckets = calloc(m->capacity, sizeof(Entry *));
    m->count = 0;
    return m;
}

void shm_free(StrHashMap *m) {
    for (int i = 0; i < m->capacity; i++) {
        Entry *e = m->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
}

static void shm_resize(StrHashMap *m, int new_cap);

// insert: returns old value if key existed, -1 if new
int shm_insert(StrHashMap *m, const char *key, int value) {
    if ((double)(m->count + 1) / m->capacity > LOAD_MAX) {
        shm_resize(m, m->capacity * 2);
    }

    unsigned int idx = djb2(key) % m->capacity;
    for (Entry *e = m->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            int old = e->value;
            e->value = value;
            return old;
        }
    }

    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = value;
    e->next = m->buckets[idx];
    m->buckets[idx] = e;
    m->count++;
    return -1;  // new entry
}

// get: returns pointer to value or NULL
int *shm_get(StrHashMap *m, const char *key) {
    unsigned int idx = djb2(key) % m->capacity;
    for (Entry *e = m->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            return &e->value;
        }
    }
    return NULL;
}

bool shm_contains(StrHashMap *m, const char *key) {
    return shm_get(m, key) != NULL;
}

// remove: returns removed value or -1
int shm_remove(StrHashMap *m, const char *key) {
    unsigned int idx = djb2(key) % m->capacity;
    Entry **ptr = &m->buckets[idx];
    while (*ptr) {
        if (strcmp((*ptr)->key, key) == 0) {
            Entry *found = *ptr;
            int val = found->value;
            *ptr = found->next;
            free(found->key);
            free(found);
            m->count--;
            return val;
        }
        ptr = &(*ptr)->next;
    }
    return -1;
}

// or_insert: insert only if key doesn't exist, return pointer to value
int *shm_or_insert(StrHashMap *m, const char *key, int default_val) {
    int *existing = shm_get(m, key);
    if (existing) return existing;
    shm_insert(m, key, default_val);
    return shm_get(m, key);
}

static void shm_resize(StrHashMap *m, int new_cap) {
    Entry **old = m->buckets;
    int old_cap = m->capacity;

    m->buckets = calloc(new_cap, sizeof(Entry *));
    m->capacity = new_cap;
    m->count = 0;

    for (int i = 0; i < old_cap; i++) {
        Entry *e = old[i];
        while (e) {
            Entry *next = e->next;
            unsigned int idx = djb2(e->key) % new_cap;
            e->next = m->buckets[idx];
            m->buckets[idx] = e;
            m->count++;
            e = next;
        }
    }
    free(old);
}

// iterate: call function for each entry
typedef void (*IterFn)(const char *key, int value, void *ctx);

void shm_iter(StrHashMap *m, IterFn fn, void *ctx) {
    for (int i = 0; i < m->capacity; i++) {
        for (Entry *e = m->buckets[i]; e; e = e->next) {
            fn(e->key, e->value, ctx);
        }
    }
}

// ============================================================
// Demos
// ============================================================

void print_entry(const char *key, int value, void *ctx) {
    (void)ctx;
    printf("    %s: %d\n", key, value);
}

void demo1_basic_api(void) {
    printf("=== Demo 1: basic API (insert, get, remove) ===\n");
    StrHashMap *m = shm_new();

    shm_insert(m, "Alice", 95);
    shm_insert(m, "Bob", 87);
    shm_insert(m, "Carol", 92);

    printf("  After 3 inserts: count=%d\n", m->count);

    int *val = shm_get(m, "Bob");
    printf("  get(\"Bob\") = %d\n", val ? *val : -1);

    int old = shm_insert(m, "Bob", 90);
    printf("  insert(\"Bob\", 90) replaced old=%d\n", old);

    int removed = shm_remove(m, "Carol");
    printf("  remove(\"Carol\") = %d, count=%d\n", removed, m->count);

    printf("  contains(\"Carol\") = %s\n",
           shm_contains(m, "Carol") ? "true" : "false");
    printf("\n");

    shm_free(m);
}

void demo2_or_insert(void) {
    printf("=== Demo 2: or_insert (entry API equivalent) ===\n");
    StrHashMap *m = shm_new();

    const char *words[] = {"the", "cat", "sat", "on", "the",
                           "mat", "the", "cat", NULL};

    for (int i = 0; words[i]; i++) {
        int *count = shm_or_insert(m, words[i], 0);
        (*count)++;
    }

    printf("  Word frequencies:\n");
    shm_iter(m, print_entry, NULL);
    printf("\n");

    shm_free(m);
}

void demo3_resize_trace(void) {
    printf("=== Demo 3: automatic resize ===\n");
    StrHashMap *m = shm_new();

    char key[16];
    for (int i = 0; i < 50; i++) {
        int old_cap = m->capacity;
        snprintf(key, sizeof(key), "key_%03d", i);
        shm_insert(m, key, i);
        if (m->capacity != old_cap) {
            printf("  insert #%d: resize %d -> %d (n=%d, alpha=%.3f)\n",
                   i + 1, old_cap, m->capacity, m->count,
                   (double)m->count / m->capacity);
        }
    }
    printf("  Final: count=%d, capacity=%d\n\n", m->count, m->capacity);

    shm_free(m);
}

void demo4_iteration(void) {
    printf("=== Demo 4: iteration ===\n");
    StrHashMap *m = shm_new();

    shm_insert(m, "red", 1);
    shm_insert(m, "green", 2);
    shm_insert(m, "blue", 3);
    shm_insert(m, "yellow", 4);
    shm_insert(m, "purple", 5);

    printf("  All entries (order not guaranteed):\n");
    shm_iter(m, print_entry, NULL);
    printf("\n");

    shm_free(m);
}

typedef struct {
    int sum;
    int count;
} SumCtx;

void sum_values(const char *key, int value, void *ctx) {
    (void)key;
    SumCtx *s = ctx;
    s->sum += value;
    s->count++;
}

void demo5_aggregate(void) {
    printf("=== Demo 5: aggregation with iteration ===\n");
    StrHashMap *m = shm_new();

    shm_insert(m, "math", 95);
    shm_insert(m, "physics", 88);
    shm_insert(m, "chemistry", 92);
    shm_insert(m, "biology", 85);

    SumCtx ctx = {0, 0};
    shm_iter(m, sum_values, &ctx);

    printf("  Subjects: %d, total: %d, average: %.1f\n\n",
           ctx.count, ctx.sum, (double)ctx.sum / ctx.count);

    shm_free(m);
}

void demo6_two_sum(void) {
    printf("=== Demo 6: two-sum problem ===\n");
    int nums[] = {2, 7, 11, 15, 1, 8};
    int n = 6;
    int target = 9;

    StrHashMap *m = shm_new();
    char key[16];
    bool found = false;

    for (int i = 0; i < n && !found; i++) {
        int complement = target - nums[i];
        snprintf(key, sizeof(key), "%d", complement);
        int *idx = shm_get(m, key);
        if (idx) {
            printf("  target=%d: nums[%d](%d) + nums[%d](%d) = %d\n",
                   target, *idx, complement, i, nums[i], target);
            found = true;
        }
        snprintf(key, sizeof(key), "%d", nums[i]);
        shm_insert(m, key, i);
    }

    shm_free(m);
    printf("\n");
}

int main(void) {
    demo1_basic_api();
    demo2_or_insert();
    demo3_resize_trace();
    demo4_iteration();
    demo5_aggregate();
    demo6_two_sum();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// hashmap_rust.rs
// Comprehensive HashMap usage in Rust
// Run: rustc hashmap_rust.rs && ./hashmap_rust

use std::collections::HashMap;
use std::hash::{Hash, Hasher, DefaultHasher};

// ============================================================
// Helper
// ============================================================

fn compute_hash<T: Hash>(val: &T) -> u64 {
    let mut hasher = DefaultHasher::new();
    val.hash(&mut hasher);
    hasher.finish()
}

// ============================================================
// Demos
// ============================================================

fn demo1_construction() {
    println!("=== Demo 1: construction methods ===");

    // new (zero allocation)
    let empty: HashMap<String, i32> = HashMap::new();
    println!("  new():           len={}, cap={}", empty.len(), empty.capacity());

    // with_capacity
    let prealloc: HashMap<String, i32> = HashMap::with_capacity(100);
    println!("  with_capacity(100): len={}, cap={}", prealloc.len(), prealloc.capacity());

    // from array
    let from_arr = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);
    println!("  from([...]): len={}", from_arr.len());

    // from iterator
    let from_iter: HashMap<i32, i32> = (0..5).map(|i| (i, i * i)).collect();
    println!("  collect():   len={}, entries: {:?}", from_iter.len(), from_iter);
    println!();
}

fn demo2_insert_and_update() {
    println!("=== Demo 2: insert, replace, entry API ===");
    let mut scores: HashMap<String, i32> = HashMap::new();

    // insert returns Option<V>
    let old = scores.insert("Alice".into(), 100);
    println!("  insert Alice=100: old={:?}", old);

    let old = scores.insert("Alice".into(), 95);
    println!("  insert Alice=95:  old={:?} (replaced)", old);

    // entry: or_insert
    scores.entry("Bob".into()).or_insert(88);
    println!("  entry Bob or_insert(88): Bob={}", scores["Bob"]);

    // entry: and_modify + or_insert
    scores.entry("Bob".into())
        .and_modify(|v| *v += 5)
        .or_insert(0);
    println!("  entry Bob and_modify(+5): Bob={}", scores["Bob"]);

    // entry: or_insert_with (lazy)
    scores.entry("Carol".into())
        .or_insert_with(|| {
            println!("    (computing default for Carol...)");
            92
        });
    println!("  Carol={}", scores["Carol"]);
    println!();
}

fn demo3_word_frequency() {
    println!("=== Demo 3: word frequency (classic entry pattern) ===");
    let text = "to be or not to be that is the question \
                to be is to know to know is to be";

    let mut freq: HashMap<&str, usize> = HashMap::new();
    for word in text.split_whitespace() {
        *freq.entry(word).or_insert(0) += 1;
    }

    // sort by frequency descending
    let mut sorted: Vec<_> = freq.iter().collect();
    sorted.sort_by(|a, b| b.1.cmp(a.1));

    println!("  Top words:");
    for (word, count) in sorted.iter().take(5) {
        println!("    {:>8}: {}", word, count);
    }
    println!();
}

fn demo4_access_patterns() {
    println!("=== Demo 4: access patterns ===");
    let mut map = HashMap::from([
        ("red", (255, 0, 0)),
        ("green", (0, 255, 0)),
        ("blue", (0, 0, 255)),
    ]);

    // get: Option<&V>
    println!("  get(\"red\"): {:?}", map.get("red"));
    println!("  get(\"pink\"): {:?}", map.get("pink"));

    // get_key_value
    println!("  get_key_value(\"blue\"): {:?}", map.get_key_value("blue"));

    // contains_key
    println!("  contains_key(\"green\"): {}", map.contains_key("green"));

    // get_mut
    if let Some(rgb) = map.get_mut("red") {
        *rgb = (200, 50, 50);
    }
    println!("  after mutating red: {:?}", map["red"]);

    // index [] — panics on missing key
    println!("  map[\"blue\"] = {:?}", map["blue"]);
    println!("  (map[\"missing\"] would panic!)\n");
}

fn demo5_removal_and_retain() {
    println!("=== Demo 5: remove, retain, drain ===");
    let mut map: HashMap<i32, &str> = HashMap::from([
        (1, "one"), (2, "two"), (3, "three"),
        (4, "four"), (5, "five"), (6, "six"),
    ]);
    println!("  initial: {:?}", map);

    // remove
    let removed = map.remove(&3);
    println!("  remove(3): {:?}, len={}", removed, map.len());

    // remove_entry
    let entry = map.remove_entry(&4);
    println!("  remove_entry(4): {:?}", entry);

    // retain: keep only even keys
    map.insert(3, "three");
    map.insert(4, "four");
    map.retain(|k, _| k % 2 == 0);
    println!("  retain(even keys): {:?}", map);

    // drain
    let drained: Vec<_> = map.drain().collect();
    println!("  drain(): got {:?}, map.len()={}", drained, map.len());
    println!();
}

fn demo6_iteration() {
    println!("=== Demo 6: iteration patterns ===");
    let map = HashMap::from([
        ("math", 95), ("physics", 88),
        ("chemistry", 92), ("biology", 85),
    ]);

    // values sum
    let total: i32 = map.values().sum();
    let avg = total as f64 / map.len() as f64;
    println!("  total={}, avg={:.1}", total, avg);

    // max by value
    let best = map.iter().max_by_key(|(_, &v)| v);
    println!("  best subject: {:?}", best);

    // filter and collect
    let passing: HashMap<&&str, &i32> = map.iter()
        .filter(|(_, &v)| v >= 90)
        .collect();
    println!("  passing (>=90): {:?}", passing);

    // keys
    let mut subjects: Vec<&&str> = map.keys().collect();
    subjects.sort();
    println!("  sorted subjects: {:?}", subjects);
    println!();
}

fn demo7_ownership() {
    println!("=== Demo 7: ownership and borrowing ===");

    // String keys: ownership transferred
    let mut map: HashMap<String, i32> = HashMap::new();
    let key = String::from("hello");
    map.insert(key, 42);
    // key is moved — can't use it
    // println!("{}", key);  // ERROR

    // lookup with &str (Borrow trait)
    let val = map.get("hello");  // &str works for String key
    println!("  get(\"hello\") on HashMap<String>: {:?}", val);

    // &str keys: Copy, no ownership issues
    let mut map2: HashMap<&str, i32> = HashMap::new();
    let key = "world";
    map2.insert(key, 99);
    println!("  key after insert: \"{}\" (still usable)", key);

    // custom type as key
    #[derive(Hash, Eq, PartialEq, Debug)]
    struct Point { x: i32, y: i32 }

    let mut grid: HashMap<Point, &str> = HashMap::new();
    grid.insert(Point { x: 0, y: 0 }, "origin");
    grid.insert(Point { x: 1, y: 0 }, "right");

    println!("  grid[origin] = {:?}", grid.get(&Point { x: 0, y: 0 }));
    println!();
}

fn demo8_advanced_patterns() {
    println!("=== Demo 8: advanced patterns ===");

    // group by
    let words = vec!["cat", "car", "bar", "bat", "can", "ban"];
    let mut by_first: HashMap<char, Vec<&str>> = HashMap::new();
    for w in &words {
        by_first.entry(w.chars().next().unwrap())
            .or_default()
            .push(w);
    }
    println!("  group by first letter: {:?}", by_first);

    // two-sum
    let nums = vec![2, 7, 11, 15, 1, 8];
    let target = 9;
    let mut seen: HashMap<i32, usize> = HashMap::new();
    for (i, &num) in nums.iter().enumerate() {
        if let Some(&j) = seen.get(&(target - num)) {
            println!("  two-sum({}): [{}, {}] → {} + {} = {}",
                     target, j, i, nums[j], num, target);
            break;
        }
        seen.insert(num, i);
    }

    // invert map
    let original = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);
    let inverted: HashMap<&i32, &&str> = original.iter()
        .map(|(k, v)| (v, k))
        .collect();
    println!("  inverted: {:?}", inverted);

    // merge with and_modify
    let mut a = HashMap::from([("x", 10), ("y", 20)]);
    let b = HashMap::from([("y", 5), ("z", 30)]);
    for (k, v) in b {
        a.entry(k).and_modify(|e| *e += v).or_insert(v);
    }
    println!("  merge(sum): {:?}", a);

    // cache / memoize
    let mut fib_cache: HashMap<u64, u64> = HashMap::new();
    fn fib(n: u64, cache: &mut HashMap<u64, u64>) -> u64 {
        if n <= 1 { return n; }
        if let Some(&v) = cache.get(&n) { return v; }
        let r = fib(n - 1, cache) + fib(n - 2, cache);
        cache.insert(n, r);
        r
    }
    println!("  fib(50) = {}", fib(50, &mut fib_cache));
    println!("  cache entries: {}", fib_cache.len());
    println!();
}

fn demo9_capacity_behavior() {
    println!("=== Demo 9: capacity and memory ===");

    let mut map: HashMap<i32, i32> = HashMap::new();
    println!("  new(): cap={}", map.capacity());

    map.insert(0, 0);
    println!("  after 1 insert: cap={}", map.capacity());

    for i in 1..100 {
        let old_cap = map.capacity();
        map.insert(i, i);
        if map.capacity() != old_cap {
            println!("  insert #{}: cap {} -> {}", i + 1, old_cap, map.capacity());
        }
    }

    println!("\n  Before shrink: len={}, cap={}", map.len(), map.capacity());
    for i in 0..90 {
        map.remove(&i);
    }
    println!("  After removing 90: len={}, cap={}", map.len(), map.capacity());

    map.shrink_to_fit();
    println!("  After shrink_to_fit: len={}, cap={}", map.len(), map.capacity());

    map.clear();
    println!("  After clear: len={}, cap={} (memory kept)", map.len(), map.capacity());
    println!();
}

fn demo10_hash_consistency() {
    println!("=== Demo 10: Hash + Eq contract ===");

    // demonstrate that equal values must have equal hashes
    let a = "hello".to_string();
    let b = "hello".to_string();

    println!("  a == b: {}", a == b);
    println!("  hash(a) = {}", compute_hash(&a));
    println!("  hash(b) = {}", compute_hash(&b));
    println!("  hash(a) == hash(b): {}", compute_hash(&a) == compute_hash(&b));

    // different values may have same hash (collision)
    // but equal values MUST have same hash

    // demonstrate with &str (Borrow trait in action)
    let hash_string = compute_hash(&"hello".to_string());
    let hash_str = compute_hash(&"hello");
    println!("\n  hash(String \"hello\") = {}", hash_string);
    println!("  hash(&str \"hello\")   = {}", hash_str);
    println!("  equal: {} (Borrow trait ensures this)", hash_string == hash_str);
    println!();
}

fn main() {
    demo1_construction();
    demo2_insert_and_update();
    demo3_word_frequency();
    demo4_access_patterns();
    demo5_removal_and_retain();
    demo6_iteration();
    demo7_ownership();
    demo8_advanced_patterns();
    demo9_capacity_behavior();
    demo10_hash_consistency();
}
```

---

## Ejercicios

### Ejercicio 1 — Contador de frecuencia de caracteres

Escribe una función que reciba un `&str` y devuelva un
`HashMap<char, usize>` con la frecuencia de cada carácter
(ignorando espacios). Prueba con `"abracadabra"`.

<details><summary>¿Cuántas entradas tiene el resultado?</summary>

`"abracadabra"` tiene las letras a, b, r, c, d.
Frecuencias: a=5, b=2, r=2, c=1, d=1.
El HashMap tiene **5 entradas**.

</details>

<details><summary>¿Qué patrón de Entry API usas?</summary>

```rust
fn char_freq(s: &str) -> HashMap<char, usize> {
    let mut freq = HashMap::new();
    for c in s.chars().filter(|c| !c.is_whitespace()) {
        *freq.entry(c).or_insert(0) += 1;
    }
    freq
}
```

`entry(c).or_insert(0)` seguido de `+= 1` — el patrón clásico de
contador.

</details>

---

### Ejercicio 2 — Entry API: and_modify vs or_insert

¿Cuál es la diferencia entre estas dos formas de contar?

```rust
// Forma A
*map.entry(key).or_insert(0) += 1;

// Forma B
map.entry(key)
    .and_modify(|v| *v += 1)
    .or_insert(1);
```

<details><summary>¿Son equivalentes?</summary>

Sí, son funcionalmente equivalentes. Ambas insertan 1 si la clave
es nueva, o incrementan si ya existe.

Forma A: `or_insert(0)` devuelve `&mut V` (0 si nuevo), luego `+= 1`.
Forma B: `and_modify` incrementa si existe, `or_insert(1)` inserta
1 si es nuevo.

Forma A es más concisa. Forma B es más explícita sobre la
intención (diferentes acciones para cada caso).

</details>

<details><summary>¿Cuándo preferir Forma B?</summary>

Cuando la acción para clave existente es **diferente** de la acción
para clave nueva. Por ejemplo:

```rust
map.entry(key)
    .and_modify(|v| v.push(item.clone()))
    .or_insert_with(|| vec![item]);
```

Aquí, si existe se hace `push`, si no existe se crea un `Vec`
nuevo. Forma A no puede expresar esto naturalmente.

</details>

---

### Ejercicio 3 — Borrow trait y búsqueda eficiente

Dado un `HashMap<String, i32>`, ¿cuál de estas búsquedas
compila?

```rust
let map: HashMap<String, i32> = HashMap::new();
let a = map.get("hello");              // A
let b = map.get(&"hello".to_string()); // B
let c = map.get(&String::from("hello")); // C
let s = String::from("hello");
let d = map.get(&s);                   // D
```

<details><summary>¿Cuáles compilan?</summary>

**Todas** compilan. Pero A es la más eficiente:
- A: `&str` directamente — sin alocación.
- B: crea un `String` temporal en el heap — alocación innecesaria.
- C: igual que B — alocación innecesaria.
- D: usa referencia a `String` existente — sin alocación extra.

El trait `Borrow<str>` implementado por `String` permite que A
funcione. **Siempre usar la forma A** para búsquedas con claves
`String`.

</details>

<details><summary>¿Cómo funciona internamente?</summary>

`HashMap::get<Q>(&self, k: &Q)` donde `K: Borrow<Q>` y `Q: Hash + Eq`.

Para `K = String`, `Q = str`: `String: Borrow<str>` está
implementado. El hash de `&str` y `String` son idénticos (ambos
hashean los bytes UTF-8), satisfaciendo el contrato Borrow.

</details>

---

### Ejercicio 4 — HashMap como grafo

Construye un grafo dirigido con `HashMap<&str, Vec<&str>>` para:

```
A → B, C
B → D
C → D, E
D → E
```

Implementa una función `reachable(graph, start)` que devuelva
todos los nodos alcanzables desde `start` usando BFS.

<details><summary>¿Cuántos nodos son alcanzables desde A?</summary>

Desde A: A → B, C → D, E → (D→E ya visitado).
Alcanzables: {A, B, C, D, E} — **5 nodos** (todos).

Desde B: B → D → E.
Alcanzables: {B, D, E} — **3 nodos**.

</details>

<details><summary>¿Cómo usar HashMap en el BFS?</summary>

```rust
use std::collections::{HashMap, HashSet, VecDeque};

fn reachable<'a>(
    graph: &HashMap<&'a str, Vec<&'a str>>,
    start: &'a str,
) -> HashSet<&'a str> {
    let mut visited = HashSet::new();
    let mut queue = VecDeque::new();
    visited.insert(start);
    queue.push_back(start);
    while let Some(node) = queue.pop_front() {
        if let Some(neighbors) = graph.get(node) {
            for &next in neighbors {
                if visited.insert(next) {
                    queue.push_back(next);
                }
            }
        }
    }
    visited
}
```

`graph.get(node)` devuelve `Option<&Vec<&str>>` — si el nodo no
tiene aristas de salida (no está como clave), devuelve `None`.

</details>

---

### Ejercicio 5 — Tipo personalizado como clave

Crea un struct `Color { r: u8, g: u8, b: u8 }` y úsalo como
clave de un `HashMap<Color, &str>`. ¿Qué traits necesitas derivar?

<details><summary>¿Qué traits se necesitan?</summary>

```rust
#[derive(Hash, Eq, PartialEq)]
struct Color { r: u8, g: u8, b: u8 }
```

Mínimo: `Hash + Eq`. `Eq` requiere `PartialEq`, así que se
derivan ambos. `Debug` es opcional pero útil para imprimir.

</details>

<details><summary>¿Qué pasa si solo derivas Hash + PartialEq (sin Eq)?</summary>

No compila. El error es:

```
the trait bound `Color: Eq` is not satisfied
```

`HashMap` requiere `Eq` (no solo `PartialEq`) porque necesita
garantía de que la igualdad es una relación de equivalencia
(reflexiva: `a == a` siempre). `PartialEq` permite tipos como
`f64` donde `NaN != NaN`.

</details>

---

### Ejercicio 6 — Merge de mapas con diferentes estrategias

Implementa tres funciones de merge para `HashMap<String, i32>`:
- `merge_sum`: sumar valores de claves comunes.
- `merge_max`: quedarse con el mayor de los dos valores.
- `merge_first`: quedarse con el valor del primer mapa.

<details><summary>¿Cómo implementar merge_max?</summary>

```rust
fn merge_max(a: &mut HashMap<String, i32>, b: HashMap<String, i32>) {
    for (k, v) in b {
        a.entry(k)
            .and_modify(|e| *e = (*e).max(v))
            .or_insert(v);
    }
}
```

`and_modify` actualiza solo si la clave existe. Si el valor
existente es mayor, se queda; si `v` es mayor, se reemplaza.
`or_insert` maneja claves que solo están en `b`.

</details>

<details><summary>¿Cómo implementar merge_first?</summary>

```rust
fn merge_first(a: &mut HashMap<String, i32>, b: HashMap<String, i32>) {
    for (k, v) in b {
        a.entry(k).or_insert(v);
    }
}
```

Solo `or_insert` — si la clave ya existe en `a`, no se modifica.
Esta es la variante más simple.

</details>

---

### Ejercicio 7 — Rendimiento: with_capacity vs new

Mide el tiempo de insertar 100000 pares `(i, i*i)` en
`HashMap::new()` vs `HashMap::with_capacity(100000)`. Usa
`std::time::Instant`.

<details><summary>¿Cuánto más rápido es with_capacity?</summary>

Típicamente 1.5-2× más rápido. La diferencia viene de:
- `new()`: ~17 resizes, ~200000 reinserciones totales.
- `with_capacity(100000)`: 0 resizes, 0 reinserciones.

El speedup es mayor con claves grandes (más costoso copiar
durante rehash) y menor con claves pequeñas como `i32`.

</details>

<details><summary>¿Vale la pena siempre pre-alocar?</summary>

No. Pre-alocar desperdiciaría memoria si el tamaño final es mucho
menor que el estimado. Usar `with_capacity` solo cuando:
- Se conoce (o se puede estimar razonablemente) el tamaño final.
- El rendimiento de la inserción es crítico.
- La memoria sobrante es aceptable si la estimación es inexacta.

Para la mayoría de usos, `HashMap::new()` es suficiente.

</details>

---

### Ejercicio 8 — retain como filtro in-place

Dado un `HashMap<String, Vec<i32>>` donde las claves son nombres
de estudiantes y los valores son sus calificaciones, usa `retain`
para eliminar estudiantes cuyo promedio sea menor a 70.

<details><summary>¿Cómo escribir el predicado?</summary>

```rust
students.retain(|_name, grades| {
    let avg = grades.iter().sum::<i32>() as f64 / grades.len() as f64;
    avg >= 70.0
});
```

`retain` recibe `|&K, &mut V| -> bool`. Se queda con las entradas
donde el predicado devuelve `true`.

</details>

<details><summary>¿Por qué retain y no filter + collect?</summary>

`retain` modifica **in-place** — no crea un nuevo HashMap. Esto
es más eficiente en memoria y CPU:
- Sin nueva alocación.
- Sin rehash de los elementos que se quedan.
- Los elementos eliminados se dropean inmediatamente.

`filter().collect()` crearía un HashMap nuevo, hasheando cada
clave de nuevo. Para $n$ entradas, eso es $O(n)$ hashes extra.

</details>

---

### Ejercicio 9 — HashMap vs Vec para lookup

Compara el tiempo de búsqueda de 10000 claves en:
a) `Vec<(String, i32)>` con búsqueda lineal.
b) `HashMap<String, i32>`.

<details><summary>¿Cuál es más rápido con n=100?</summary>

Con $n = 100$ elementos, el `Vec` puede ser **más rápido** o
comparable. La búsqueda lineal recorre ~50 elementos en promedio
(hit) o 100 (miss). Pero el acceso es secuencial en memoria
(cache-friendly), y no hay costo de hashing.

HashMap calcula un hash (decenas de ns para SipHash) + 1-2
comparaciones. Para claves pequeñas (`i32`), el costo de hash puede
dominar.

Breakeven típico: $n \approx 20-50$ dependiendo del tipo de clave
y la CPU.

</details>

<details><summary>¿Y con n=10000?</summary>

Con $n = 10000$, HashMap es **~100-500× más rápido**.
Vec busca ~5000 elementos en promedio (hit). HashMap hace 1 hash +
1-2 comparaciones. La diferencia es dramática.

Regla práctica: usar HashMap para $n > 20$ si las búsquedas son
frecuentes.

</details>

---

### Ejercicio 10 — Entry API con or_insert_with_key

Implementa un sistema de IDs auto-incrementales: dado un
`Vec<&str>` de nombres, asigna un ID único a cada nombre
(empezando en 1). Los nombres repetidos deben recibir el mismo
ID. Usa `or_insert_with_key`.

<details><summary>¿Cómo funciona or_insert_with_key?</summary>

`or_insert_with_key` pasa la **clave** al closure. Útil cuando el
valor por defecto depende de la clave:

```rust
let mut ids: HashMap<String, usize> = HashMap::new();
let mut next_id = 1;

let names = vec!["Alice", "Bob", "Alice", "Carol", "Bob"];

for name in names {
    let id = ids.entry(name.to_string())
        .or_insert_with_key(|_key| {
            let id = next_id;
            next_id += 1;
            id
        });
    println!("{} → ID {}", name, id);
}
// Alice → 1, Bob → 2, Alice → 1, Carol → 3, Bob → 2
```

El closure solo se ejecuta si la clave es nueva. Para claves
existentes, devuelve la referencia al valor existente sin llamar
al closure.

</details>

<details><summary>¿Cuántas veces se ejecuta el closure?</summary>

3 veces — una por cada nombre **único**: Alice, Bob, Carol.
Las repeticiones de Alice y Bob no ejecutan el closure (la clave
ya existe en el mapa).

</details>
