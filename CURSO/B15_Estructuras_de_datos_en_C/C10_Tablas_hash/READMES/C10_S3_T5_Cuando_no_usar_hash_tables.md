# T05 — Cuándo NO usar hash tables

## Objetivo

Identificar los escenarios donde una hash table **no es** la estructura de datos
óptima, comprender las alternativas (BST, B-tree, array, trie) y desarrollar
criterio para elegir la estructura correcta según los requisitos del problema.

---

## 1. La trampa del $O(1)$

Es tentador pensar que, con búsqueda $O(1)$ promedio, una hash table siempre
gana. Pero ese $O(1)$ tiene letra pequeña:

- Es **promedio**, no peor caso — el peor caso es $O(n)$ (todas las claves
  colisionan).
- Oculta una **constante** no despreciable: calcular el hash, manejar
  colisiones, posible rehash.
- Sacrifica **orden**: los elementos no tienen ninguna relación de posición
  entre sí.
- Requiere **claves hasheables**: no todo tipo de dato admite una función hash
  consistente con la igualdad.
- Consume **más memoria** que un array compacto: factor de carga típico
  $\alpha \le 0.75$ implica al menos un 25% de espacio desperdiciado.

Cuando alguna de estas limitaciones colisiona con un requisito del problema,
otra estructura es mejor.

---

## 2. Escenario 1 — Datos ordenados y consultas de rango

### El problema

Si necesitas responder preguntas como:

- ¿Cuáles claves están entre 100 y 500?
- ¿Cuál es la clave mínima/máxima?
- ¿Cuál es el sucesor/predecesor de $k$?
- Dame todas las claves en orden ascendente.

Una hash table **no puede** responder ninguna de estas de forma eficiente. Los
elementos están dispersos según el hash, no según su valor. Para obtener orden
habría que extraer todas las claves y ordenarlas: $O(n \log n)$.

### La alternativa: BST balanceado / B-tree

| Operación | Hash table | BST balanceado (AVL/Red-Black) | B-tree |
|-----------|:----------:|:------------------------------:|:------:|
| Búsqueda por clave | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ |
| Inserción | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ |
| Mínimo / Máximo | $O(n)$ | $O(\log n)$ | $O(1)$ amort |
| Sucesor / Predecesor | $O(n)$ | $O(\log n)$ | $O(\log n)$ |
| Rango $[a, b]$ | $O(n)$ | $O(\log n + k)$ | $O(\log n + k)$ |
| Recorrido in-order | $O(n \log n)$ | $O(n)$ | $O(n)$ |

Donde $k$ es el número de resultados en el rango.

En Rust, `BTreeMap<K, V>` ofrece todas estas operaciones:

```rust
use std::collections::BTreeMap;

let mut scores: BTreeMap<&str, u32> = BTreeMap::new();
scores.insert("Alice", 85);
scores.insert("Bob", 92);
scores.insert("Carol", 78);
scores.insert("Dave", 95);
scores.insert("Eve", 88);

// Range query: students scoring 80-90
for (name, score) in scores.iter().filter(|(_, &s)| s >= 80 && s <= 90) {
    println!("{name}: {score}");
}

// But BTreeMap is ordered by KEY, not value.
// For range on keys:
let range: Vec<_> = scores.range("Bob"..="Dave").collect();
```

### Cuándo la hash table sigue ganando

Si **nunca** necesitas orden y solo haces búsquedas puntuales (lookup, insert,
delete), la hash table es mejor: $O(1)$ vs $O(\log n)$. Para $n = 10^6$,
eso es $1$ vs $\sim 20$ comparaciones.

---

## 3. Escenario 2 — Colecciones pequeñas ($n < 50$)

### El problema

Para colecciones pequeñas, la constante oculta en el $O(1)$ de la hash table
puede superar al $O(n)$ de una búsqueda lineal en un array:

- **Calcular el hash** de una string de 20 caracteres requiere recorrer toda la
  string y hacer aritmética — mientras que una comparación directa puede abortar
  en el primer carácter diferente.
- **Localidad de caché**: un array de 50 elementos cabe en una o dos líneas de
  caché L1 (64 bytes cada una). Un hash table con buckets dispersos produce
  cache misses.
- **Overhead de memoria**: un `HashMap` en Rust tiene al menos ~48 bytes de
  overhead interno más los control bytes; para 5 elementos, esto puede ser 10x
  el tamaño de un `Vec`.

### El umbral empírico

No hay un número mágico universal, pero la regla práctica es:

| $n$ | Mejor estructura | Por qué |
|-----|:----------------:|---------|
| $< 16$ | Array / `Vec` + búsqueda lineal | Cabe en una línea de caché L1 |
| $16 - 50$ | Depende del tamaño de clave | Medir con benchmark |
| $> 50$ | Hash table (si no se necesita orden) | $O(1)$ amortiza el overhead |

El compilador de LLVM y el kernel de Linux usan arrays lineales para tablas de
símbolos locales y switch-case con pocos brazos, respectivamente.

### Ejemplo: `SmallVec` / array lineal vs HashMap

```rust
// For small key sets, Vec<(K,V)> beats HashMap
fn linear_lookup<'a>(table: &'a [(u32, &str)], key: u32) -> Option<&'a str> {
    table.iter()
        .find(|(k, _)| *k == key)
        .map(|(_, v)| *v)
}

let small_table = [(1, "one"), (2, "two"), (3, "three"), (4, "four")];
assert_eq!(linear_lookup(&small_table, 3), Some("three"));
```

---

## 4. Escenario 3 — Claves no hasheables

### El problema

Una función hash debe satisfacer el contrato:

$$a = b \implies \text{hash}(a) = \text{hash}(b)$$

Algunos tipos no pueden cumplir este contrato de forma útil:

#### Números de punto flotante

`f64` implementa `PartialEq` pero **no** `Eq` en Rust, porque `NaN != NaN`.
Tampoco implementa `Hash`. Intentar usar `f64` como clave de `HashMap` es un
error de compilación:

```rust
use std::collections::HashMap;

// ERROR: f64 does not implement Eq + Hash
// let mut map: HashMap<f64, String> = HashMap::new();
```

En C no hay protección del compilador, pero el problema es idéntico: si usas
`double` como clave, `0.1 + 0.2 != 0.3` por errores de redondeo, y el lookup
falla silenciosamente.

#### Objetos con igualdad aproximada

Vectores geométricos, colores con tolerancia, timestamps con precisión
variable — cualquier tipo donde la igualdad sea "cercano a" en lugar de
"exactamente igual" no es compatible con hashing.

#### Workarounds (con precaución)

1. **Discretizar**: convertir `f64` a entero escalado
   (`(x * 1000.0).round() as i64`), pero se pierde precisión.
2. **Wrapper con Ord**: usar `ordered_float::OrderedFloat<f64>` en Rust, que
   define `NaN` como igual a sí mismo y mayor que todo.
3. **Representación canónica**: normalizar antes de hashear (ej. reducir
   fracciones a forma irreducible).

### La alternativa: BTreeMap con Ord

`BTreeMap` requiere `Ord`, no `Hash`. Para tipos que tienen orden total pero
no hash natural, un BST es la opción correcta:

```rust
use ordered_float::OrderedFloat;
use std::collections::BTreeMap;

let mut temps: BTreeMap<OrderedFloat<f64>, &str> = BTreeMap::new();
temps.insert(OrderedFloat(36.5), "normal");
temps.insert(OrderedFloat(38.0), "fever");
temps.insert(OrderedFloat(40.0), "high_fever");

// Range query: temperatures between 37 and 39
for (temp, label) in temps.range(OrderedFloat(37.0)..OrderedFloat(39.0)) {
    println!("{temp}: {label}");
}
```

---

## 5. Escenario 4 — Recorrido ordenado frecuente

### El problema

Si tu aplicación necesita iterar sobre los elementos en orden con frecuencia
(ej. imprimir un directorio ordenado, generar un reporte ordenado por fecha,
serializar en orden determinista), una hash table te obliga a:

1. Extraer todas las claves: $O(n)$.
2. Ordenarlas: $O(n \log n)$.
3. Acceder por clave: $O(n) \times O(1) = O(n)$.

Total: $O(n \log n)$ cada vez que necesitas el recorrido.

Un `BTreeMap` da recorrido in-order en $O(n)$ directamente, sin paso de
ordenación.

### Cuándo importa

| Patrón de uso | Mejor estructura |
|---------------|:----------------:|
| 1000 inserciones, luego 1 recorrido ordenado | Hash table + sort al final |
| Inserciones intercaladas con recorridos frecuentes | `BTreeMap` / BST |
| Solo recorridos, nunca búsqueda por clave | Array ordenado / `Vec` + sort |

### Orden de inserción

Si necesitas mantener el **orden de inserción** (no el orden de las claves),
ni hash table ni BST sirven directamente. Opciones:

- **`IndexMap`** (crate `indexmap` en Rust): hash table que mantiene orden de
  inserción, iteración en $O(n)$ en orden de inserción, búsqueda $O(1)$.
- **`LinkedHashMap`**: hash table + lista doblemente enlazada (Java
  `LinkedHashMap`, crate `linked-hash-map` en Rust).
- En C: hash table con array auxiliar de punteros en orden de inserción.

---

## 6. Escenario 5 — Garantías de peor caso

### El problema

En sistemas de tiempo real, bases de datos, y sistemas operativos, el **peor
caso** importa más que el promedio:

| Estructura | Búsqueda promedio | Búsqueda peor caso |
|-----------|:-----------------:|:------------------:|
| Hash table (chaining) | $O(1)$ | $O(n)$ |
| Hash table (open addr.) | $O(1)$ | $O(n)$ |
| AVL tree | $O(\log n)$ | $O(\log n)$ |
| Red-Black tree | $O(\log n)$ | $O(\log n)$ |
| B-tree | $O(\log n)$ | $O(\log n)$ |

Un atacante que conozca la función hash puede forzar todas las claves al mismo
bucket — el ataque **HashDoS** que vimos en T04. Aunque SipHash mitiga esto,
no lo elimina al 100%.

Para sistemas donde un spike de latencia es inaceptable:

- **Kernel de Linux**: usa Red-Black trees para el scheduler de procesos (CFS)
  y para el mapeo de memoria virtual (`vm_area_struct`).
- **Bases de datos**: usan B-trees para índices en disco porque $O(\log n)$
  con branching factor alto significa pocas lecturas de disco.
- **Sistemas embebidos**: a menudo prohiben `malloc`/`realloc`, lo que hace
  imposible el redimensionamiento de hash tables.

### Cuckoo hashing como compromiso

Como vimos en T04, cuckoo hashing ofrece $O(1)$ **peor caso** en búsqueda
(a costa de inserción más compleja). Es un punto intermedio interesante cuando
necesitas lookup determinista pero toleras inserciones más lentas.

---

## 7. Escenario 6 — Memoria limitada

### El problema

Una hash table con $\alpha \le 0.75$ desperdicia al menos el 25% de la memoria
asignada. Además:

- **Control bytes**: SwissTable usa 1 byte extra por slot.
- **Punteros de bucket**: chaining almacena un puntero por nodo de lista
  (8 bytes en 64-bit).
- **Crecimiento en potencias de 2**: al redimensionar de $m$ a $2m$, el pico
  de memoria es $3m$ slots (viejo + nuevo durante rehash).

Para $n = 10^6$ enteros de 4 bytes:

| Estructura | Memoria aproximada |
|-----------|:------------------:|
| Array compacto | 4 MB |
| `Vec` sorted | 4 MB + overhead mínimo |
| `HashMap<u32, ()>` | ~18-24 MB (con control bytes, padding, $\alpha$) |
| `BTreeMap<u32, ()>` | ~40-56 MB (nodos con punteros) |

Cuando la memoria es escasa (sistemas embebidos, WASM con memoria limitada,
procesamiento de datos masivos), un array ordenado con búsqueda binaria puede
ser la opción más eficiente en espacio:

```c
// Sorted array with binary search: O(log n) lookup, O(1) memory per element
int binary_search(const int *arr, int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == key) return mid;
        if (arr[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;  // not found
}
```

### Bloom filter como complemento

Si solo necesitas saber "¿está $x$ en el conjunto?" con cierta tolerancia a
falsos positivos, un **Bloom filter** usa $\sim 10$ bits por elemento
(vs ~64+ bits por elemento en un `HashSet`). Se usa extensivamente en bases de
datos (LevelDB, Cassandra) para evitar lecturas innecesarias de disco.

---

## 8. Escenario 7 — Prefijos y búsqueda parcial

### El problema

Si necesitas:

- Autocompletado: "todas las palabras que empiezan con `pre`".
- Búsqueda por prefijo: "todos los archivos en `/usr/local/`".
- Longest prefix match: routing tables de red.

Una hash table requiere la clave **exacta**. No puede buscar por prefijo sin
iterar todos los elementos.

### La alternativa: Trie (prefix tree)

Un trie almacena strings carácter por carácter, compartiendo prefijos comunes:

```
        (root)
       /      \
      c        d
     / \        \
    a   o        o
    |   |        |
    t   d        g
        |
        e
```

| Operación | Hash table | Trie |
|-----------|:----------:|:----:|
| Búsqueda exacta | $O(1)$ avg ($O(L)$ hash) | $O(L)$ |
| Búsqueda por prefijo | $O(n)$ | $O(L + k)$ |
| Autocompletado (top-$k$) | $O(n \log k)$ | $O(L + k)$ |
| Longest prefix match | $O(L \cdot m)$ intentos | $O(L)$ |

Donde $L$ es la longitud de la clave y $k$ el número de resultados.

En Rust no hay trie en la biblioteca estándar, pero crates como `radix_trie`
y `cedarwood` ofrecen implementaciones eficientes. En C, se implementa como
árbol de punteros o array compacto (PATRICIA trie).

---

## 9. Matriz de decisión completa

| Criterio | Hash table | BST (AVL/RB) | B-tree | Array ordenado | Trie |
|----------|:----------:|:------------:|:------:|:--------------:|:----:|
| Búsqueda puntual | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ | $O(L)$ |
| Inserción | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ | $O(n)$ | $O(L)$ |
| Eliminación | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ | $O(n)$ | $O(L)$ |
| Rango $[a,b]$ | $O(n)$ | $O(\log n + k)$ | $O(\log n + k)$ | $O(\log n + k)$ | N/A |
| Min / Max | $O(n)$ | $O(\log n)$ | $O(1)$ | $O(1)$ | — |
| Orden | No | Sí | Sí | Sí | Lexicográfico |
| Peor caso búsqueda | $O(n)$ | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ | $O(L)$ |
| Memoria por elem. | Alta | Media | Media | Mínima | Alta (punteros) |
| Caché-friendly | Medio* | Bajo | Alto | Muy alto | Bajo |
| Prefijos | No | No | No | Sí (con esfuerzo) | Sí |

*SwissTable es bastante cache-friendly gracias a los grupos de 16.

### Árbol de decisión rápido

```
¿Necesitas orden o rangos?
├─ Sí → ¿Muchas inserciones/eliminaciones?
│       ├─ Sí → BTreeMap (Rust) / Red-Black tree (C)
│       └─ No → Array ordenado + binary search
└─ No → ¿n > 50?
         ├─ Sí → ¿Claves hasheables?
         │       ├─ Sí → HashMap / HashSet
         │       └─ No → BTreeMap con Ord
         └─ No → Vec / array + búsqueda lineal
```

---

## 10. Antipatrones comunes

### Antipatrón 1: HashMap para contadores con pocas categorías

```rust
// BAD: HashMap overhead for 3-5 categories
let mut counts: HashMap<&str, u32> = HashMap::new();
for item in &items {
    *counts.entry(item.category).or_insert(0) += 1;
}

// BETTER: enum + array
#[derive(Clone, Copy)]
enum Category { A, B, C }

let mut counts = [0u32; 3];
for item in &items {
    counts[item.category as usize] += 1;
}
```

### Antipatrón 2: HashMap como sparse array con claves densas

```rust
// BAD: keys are 0..n, just use a Vec!
let mut map: HashMap<usize, f64> = HashMap::new();
for i in 0..1000 {
    map.insert(i, compute(i));
}

// BETTER: direct indexing
let mut values: Vec<f64> = (0..1000).map(compute).collect();
```

### Antipatrón 3: HashSet para membership test en datos estáticos y ordenados

```rust
// BAD: building a HashSet just to check membership
let valid_codes: HashSet<u32> = vec![100, 200, 301, 302, 404, 500].into_iter().collect();
if valid_codes.contains(&code) { /* ... */ }

// BETTER: sorted slice + binary_search (or even match for few values)
let valid_codes = [100, 200, 301, 302, 404, 500]; // already sorted
if valid_codes.binary_search(&code).is_ok() { /* ... */ }

// BEST for very few values: pattern match
match code {
    100 | 200 | 301 | 302 | 404 | 500 => { /* ... */ }
    _ => {}
}
```

### Antipatrón 4: HashMap cuando el orden de inserción importa

```rust
// BAD: iterating HashMap gives unpredictable order
let mut config: HashMap<String, String> = HashMap::new();
config.insert("host".into(), "localhost".into());
config.insert("port".into(), "8080".into());
config.insert("db".into(), "mydb".into());
// Iteration order: could be any permutation!

// BETTER: use IndexMap for insertion-order iteration
// use indexmap::IndexMap;
// let mut config: IndexMap<String, String> = IndexMap::new();
```

---

## 11. Programa completo en C

```c
/*
 * when_not_hash.c
 *
 * Demonstrates scenarios where hash tables are NOT the best choice
 * and compares with alternative data structures.
 *
 * Compile: gcc -O2 -o when_not_hash when_not_hash.c -lm
 * Run:     ./when_not_hash
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ========================== SORTED ARRAY ========================== */

typedef struct {
    int *data;
    int size;
    int capacity;
} SortedArray;

SortedArray sa_create(int capacity) {
    SortedArray sa;
    sa.data = malloc(capacity * sizeof(int));
    sa.size = 0;
    sa.capacity = capacity;
    return sa;
}

void sa_free(SortedArray *sa) {
    free(sa->data);
    sa->data = NULL;
    sa->size = sa->capacity = 0;
}

/* Binary search: returns index or -1 */
int sa_find(const SortedArray *sa, int key) {
    int lo = 0, hi = sa->size - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (sa->data[mid] == key) return mid;
        if (sa->data[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* Insert maintaining sorted order (O(n) shift) */
void sa_insert(SortedArray *sa, int key) {
    if (sa->size == sa->capacity) {
        sa->capacity *= 2;
        sa->data = realloc(sa->data, sa->capacity * sizeof(int));
    }
    /* Find insertion point */
    int i = sa->size - 1;
    while (i >= 0 && sa->data[i] > key) {
        sa->data[i + 1] = sa->data[i];
        i--;
    }
    sa->data[i + 1] = key;
    sa->size++;
}

/* Range query: count elements in [lo, hi] */
int sa_range_count(const SortedArray *sa, int lo_val, int hi_val) {
    /* Find first >= lo_val */
    int lo = 0, hi = sa->size;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (sa->data[mid] < lo_val) lo = mid + 1;
        else hi = mid;
    }
    int start = lo;

    /* Find first > hi_val */
    lo = start;
    hi = sa->size;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (sa->data[mid] <= hi_val) lo = mid + 1;
        else hi = mid;
    }
    int end = lo;

    return end - start;
}

/* ======================== SIMPLE HASH SET ========================= */

#define HT_SIZE 16384
#define HT_EMPTY -1

typedef struct {
    int *buckets;
    int size;
    int capacity;
} SimpleHashSet;

SimpleHashSet hs_create(int capacity) {
    SimpleHashSet hs;
    hs.capacity = capacity;
    hs.size = 0;
    hs.buckets = malloc(capacity * sizeof(int));
    for (int i = 0; i < capacity; i++)
        hs.buckets[i] = HT_EMPTY;
    return hs;
}

void hs_free(SimpleHashSet *hs) {
    free(hs->buckets);
    hs->buckets = NULL;
    hs->size = hs->capacity = 0;
}

static unsigned int hs_hash(int key, int cap) {
    unsigned int h = (unsigned int)key;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h % (unsigned int)cap;
}

int hs_insert(SimpleHashSet *hs, int key) {
    unsigned int idx = hs_hash(key, hs->capacity);
    for (int i = 0; i < hs->capacity; i++) {
        unsigned int pos = (idx + i) % (unsigned int)hs->capacity;
        if (hs->buckets[pos] == HT_EMPTY) {
            hs->buckets[pos] = key;
            hs->size++;
            return 1;
        }
        if (hs->buckets[pos] == key) return 0;  /* duplicate */
    }
    return 0;  /* full */
}

int hs_contains(const SimpleHashSet *hs, int key) {
    unsigned int idx = hs_hash(key, hs->capacity);
    for (int i = 0; i < hs->capacity; i++) {
        unsigned int pos = (idx + i) % (unsigned int)hs->capacity;
        if (hs->buckets[pos] == HT_EMPTY) return 0;
        if (hs->buckets[pos] == key) return 1;
    }
    return 0;
}

/* Range "query" on hash set: must scan everything */
int hs_range_count(const SimpleHashSet *hs, int lo, int hi) {
    int count = 0;
    for (int i = 0; i < hs->capacity; i++) {
        if (hs->buckets[i] != HT_EMPTY &&
            hs->buckets[i] >= lo && hs->buckets[i] <= hi)
            count++;
    }
    return count;
}

/* ========================= SIMPLE BST ============================= */

typedef struct BSTNode {
    int key;
    struct BSTNode *left, *right;
} BSTNode;

BSTNode *bst_insert(BSTNode *root, int key) {
    if (!root) {
        BSTNode *node = malloc(sizeof(BSTNode));
        node->key = key;
        node->left = node->right = NULL;
        return node;
    }
    if (key < root->key) root->left = bst_insert(root->left, key);
    else if (key > root->key) root->right = bst_insert(root->right, key);
    return root;
}

int bst_find(const BSTNode *root, int key) {
    while (root) {
        if (key == root->key) return 1;
        root = key < root->key ? root->left : root->right;
    }
    return 0;
}

int bst_range_count(const BSTNode *root, int lo, int hi) {
    if (!root) return 0;
    if (root->key < lo) return bst_range_count(root->right, lo, hi);
    if (root->key > hi) return bst_range_count(root->left, lo, hi);
    return 1 + bst_range_count(root->left, lo, hi)
             + bst_range_count(root->right, lo, hi);
}

void bst_inorder(const BSTNode *root, int *out, int *idx) {
    if (!root) return;
    bst_inorder(root->left, out, idx);
    out[(*idx)++] = root->key;
    bst_inorder(root->right, out, idx);
}

void bst_free(BSTNode *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

/* ====================== TIMING HELPERS ============================ */

static double time_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1e6;
}

/* ============================ DEMOS =============================== */

/*
 * Demo 1: Range query — sorted array vs hash set
 * Shows that hash set must scan all buckets for range queries.
 */
void demo_range_query(void) {
    printf("=== Demo 1: Range Query — Sorted Array vs Hash Set ===\n\n");

    const int N = 10000;
    SortedArray sa = sa_create(N);
    SimpleHashSet hs = hs_create(N * 2);

    srand(42);
    for (int i = 0; i < N; i++) {
        int val = rand() % 100000;
        sa_insert(&sa, val);
        hs_insert(&hs, val);
    }

    int lo = 25000, hi = 75000;
    struct timespec t0, t1;
    int count;

    /* Sorted array range query */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < 1000; i++)
        count = sa_range_count(&sa, lo, hi);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Sorted array: range [%d, %d] → %d elements\n", lo, hi, count);
    printf("  1000 queries: %.3f ms\n", time_ms(t0, t1));

    /* Hash set range "query" (full scan) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < 1000; i++)
        count = hs_range_count(&hs, lo, hi);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Hash set:      range [%d, %d] → %d elements\n", lo, hi, count);
    printf("  1000 queries: %.3f ms (full scan!)\n\n", time_ms(t0, t1));

    sa_free(&sa);
    hs_free(&hs);
}

/*
 * Demo 2: Small collection — linear search vs hash lookup
 * For n < ~30, linear search on array beats hash table.
 */
void demo_small_collection(void) {
    printf("=== Demo 2: Small Collection — Array vs Hash ===\n\n");

    const int sizes[] = {5, 10, 20, 50, 100, 500};
    const int num_sizes = 6;
    const int LOOKUPS = 100000;

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        int *arr = malloc(n * sizeof(int));
        SimpleHashSet hs = hs_create(n * 4);

        for (int i = 0; i < n; i++) {
            arr[i] = i * 7 + 3;
            hs_insert(&hs, arr[i]);
        }

        struct timespec t0, t1;
        volatile int found;

        /* Linear search */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int q = 0; q < LOOKUPS; q++) {
            int key = arr[q % n];
            found = 0;
            for (int i = 0; i < n; i++) {
                if (arr[i] == key) { found = 1; break; }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double linear_ms = time_ms(t0, t1);

        /* Hash lookup */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int q = 0; q < LOOKUPS; q++) {
            int key = arr[q % n];
            found = hs_contains(&hs, key);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double hash_ms = time_ms(t0, t1);

        printf("n = %3d: linear = %.3f ms, hash = %.3f ms → %s wins\n",
               n, linear_ms, hash_ms,
               linear_ms < hash_ms ? "linear" : "hash");

        free(arr);
        hs_free(&hs);
    }
    printf("\n");
}

/*
 * Demo 3: Ordered traversal — BST in-order vs hash + sort
 * BST gives O(n) ordered traversal; hash table needs O(n log n).
 */
void demo_ordered_traversal(void) {
    printf("=== Demo 3: Ordered Traversal — BST vs Hash + Sort ===\n\n");

    const int N = 50000;
    BSTNode *bst = NULL;
    SimpleHashSet hs = hs_create(N * 2);
    int *shuffled = malloc(N * sizeof(int));

    /* Generate shuffled data */
    for (int i = 0; i < N; i++) shuffled[i] = i;
    srand(123);
    for (int i = N - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = shuffled[i];
        shuffled[i] = shuffled[j];
        shuffled[j] = tmp;
    }

    /* Insert into both structures */
    for (int i = 0; i < N; i++) {
        bst = bst_insert(bst, shuffled[i]);
        hs_insert(&hs, shuffled[i]);
    }

    struct timespec t0, t1;
    int *output = malloc(N * sizeof(int));

    /* BST in-order traversal */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int idx = 0;
    bst_inorder(bst, output, &idx);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("BST in-order (%d elements): %.3f ms\n", idx, time_ms(t0, t1));

    /* Hash: extract + sort */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int count = 0;
    for (int i = 0; i < hs.capacity; i++) {
        if (hs.buckets[i] != HT_EMPTY)
            output[count++] = hs.buckets[i];
    }
    /* qsort */
    int cmp_int(const void *a, const void *b) {
        return (*(int *)a - *(int *)b);
    }
    qsort(output, count, sizeof(int), cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Hash extract+sort (%d elements): %.3f ms\n\n", count, time_ms(t0, t1));

    free(output);
    free(shuffled);
    bst_free(bst);
    hs_free(&hs);
}

/*
 * Demo 4: Memory comparison
 * Shows bytes per element for array vs hash set.
 */
void demo_memory_comparison(void) {
    printf("=== Demo 4: Memory Comparison ===\n\n");

    const int sizes[] = {100, 1000, 10000, 100000};
    const int num_sizes = 4;

    printf("%-10s  %-18s  %-18s  %-10s\n",
           "n", "Array (bytes)", "HashSet (bytes)", "Ratio");
    printf("%-10s  %-18s  %-18s  %-10s\n",
           "---", "---", "---", "---");

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        size_t array_bytes = n * sizeof(int);

        /* Hash set with load factor ~ 0.5 needs 2n slots */
        int ht_cap = n * 2;
        size_t hash_bytes = ht_cap * sizeof(int) + sizeof(SimpleHashSet);

        printf("%-10d  %-18zu  %-18zu  %.1fx\n",
               n, array_bytes, hash_bytes, (double)hash_bytes / array_bytes);
    }
    printf("\nNote: real HashMap has additional overhead (control bytes, metadata)\n\n");
}

/*
 * Demo 5: Point lookup — hash table's strength
 * Shows where hash table genuinely wins: many random lookups.
 */
void demo_point_lookup(void) {
    printf("=== Demo 5: Point Lookup — Where Hash Tables Shine ===\n\n");

    const int N = 100000;
    const int QUERIES = 100000;

    /* Build sorted array */
    SortedArray sa = sa_create(N);
    for (int i = 0; i < N; i++)
        sa_insert(&sa, i * 3);  /* insert in order */

    /* Build hash set */
    SimpleHashSet hs = hs_create(N * 2);
    for (int i = 0; i < N; i++)
        hs_insert(&hs, i * 3);

    /* Random query keys (mix of hits and misses) */
    int *queries = malloc(QUERIES * sizeof(int));
    srand(99);
    for (int i = 0; i < QUERIES; i++)
        queries[i] = rand() % (N * 3);

    struct timespec t0, t1;
    volatile int found;

    /* Sorted array + binary search */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int q = 0; q < QUERIES; q++)
        found = sa_find(&sa, queries[q]) >= 0;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Binary search (%d queries on %d elements): %.3f ms\n",
           QUERIES, N, time_ms(t0, t1));

    /* Hash lookup */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int q = 0; q < QUERIES; q++)
        found = hs_contains(&hs, queries[q]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("Hash lookup   (%d queries on %d elements): %.3f ms\n\n",
           QUERIES, N, time_ms(t0, t1));

    free(queries);
    sa_free(&sa);
    hs_free(&hs);
}

/*
 * Demo 6: Enum array vs HashMap for categorical data
 * When keys are a small fixed set, array indexing is fastest.
 */
void demo_categorical_data(void) {
    printf("=== Demo 6: Categorical Data — Enum Array vs Hash ===\n\n");

    /* Simulate counting HTTP status categories */
    enum StatusCat { INFO, SUCCESS, REDIRECT, CLIENT_ERR, SERVER_ERR, NUM_CATS };
    const char *cat_names[] = {
        "1xx Info", "2xx Success", "3xx Redirect",
        "4xx Client Error", "5xx Server Error"
    };

    const int N = 1000000;
    int *events = malloc(N * sizeof(int));
    srand(77);
    for (int i = 0; i < N; i++)
        events[i] = rand() % NUM_CATS;

    struct timespec t0, t1;

    /* Array-indexed counting */
    int counts_arr[NUM_CATS] = {0};
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++)
        counts_arr[events[i]]++;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double arr_ms = time_ms(t0, t1);

    /* Hash-based counting (overkill for 5 categories) */
    /* Simulate with a hash table mapping int → count */
    SimpleHashSet hs = hs_create(32);
    int hash_counts[NUM_CATS] = {0};
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) {
        hash_counts[events[i]]++;  /* simplified; real hash map would be slower */
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double hash_ms = time_ms(t0, t1);

    printf("Counting %d events across %d categories:\n", N, NUM_CATS);
    printf("  Array-indexed: %.3f ms\n", arr_ms);
    printf("  Hash-based:    %.3f ms (simulated; real overhead is higher)\n\n", hash_ms);

    for (int i = 0; i < NUM_CATS; i++)
        printf("  %s: %d\n", cat_names[i], counts_arr[i]);

    printf("\nTakeaway: for few fixed categories, enum + array = best.\n\n");

    free(events);
    hs_free(&hs);
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  When NOT to Use Hash Tables — C Demonstrations     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_range_query();
    demo_small_collection();
    demo_ordered_traversal();
    demo_memory_comparison();
    demo_point_lookup();
    demo_categorical_data();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Summary: choose the right tool for the job!        ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 12. Programa completo en Rust

```rust
/*
 * when_not_hash.rs
 *
 * Demonstrates scenarios where hash tables are NOT the best choice.
 *
 * Run: cargo run --release
 *
 * Cargo.toml dependencies:
 * [dependencies]
 * ordered-float = "4"
 * indexmap = "2"
 */

use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::time::Instant;

// ── Demo 1: Range queries — BTreeMap vs HashMap ──────────────────────

fn demo_range_query() {
    println!("=== Demo 1: Range Query — BTreeMap vs HashMap ===\n");

    let data: Vec<(i32, &str)> = vec![
        (10, "alpha"), (25, "beta"), (30, "gamma"), (42, "delta"),
        (55, "epsilon"), (67, "zeta"), (73, "eta"), (88, "theta"),
        (91, "iota"), (99, "kappa"),
    ];

    let btree: BTreeMap<i32, &str> = data.iter().cloned().collect();
    let hash: HashMap<i32, &str> = data.iter().cloned().collect();

    // BTreeMap: direct range query
    println!("BTreeMap range [30, 80]:");
    for (k, v) in btree.range(30..=80) {
        println!("  {k} → {v}");
    }

    // HashMap: must iterate everything and filter
    println!("\nHashMap range [30, 80] (requires full scan):");
    let mut range_results: Vec<_> = hash.iter()
        .filter(|(&k, _)| k >= 30 && k <= 80)
        .collect();
    range_results.sort_by_key(|(&k, _)| k);
    for (k, v) in &range_results {
        println!("  {k} → {v}");
    }

    // Min/max
    println!("\nBTreeMap min: {:?}", btree.iter().next());
    println!("BTreeMap max: {:?}", btree.iter().next_back());
    println!("HashMap min: requires O(n) scan");
    println!();
}

// ── Demo 2: Small collections — Vec vs HashSet ──────────────────────

fn demo_small_collection() {
    println!("=== Demo 2: Small Collection — Vec vs HashSet ===\n");

    let sizes = [5, 10, 25, 50, 100, 500, 1000];
    let queries = 500_000;

    for &n in &sizes {
        let vec_data: Vec<u64> = (0..n).map(|i| i * 7 + 3).collect();
        let set_data: HashSet<u64> = vec_data.iter().cloned().collect();

        // Vec linear search
        let start = Instant::now();
        let mut found_v = 0u64;
        for q in 0..queries {
            let key = vec_data[(q % n) as usize];
            if vec_data.contains(&key) {
                found_v += 1;
            }
        }
        let vec_time = start.elapsed();

        // HashSet lookup
        let start = Instant::now();
        let mut found_h = 0u64;
        for q in 0..queries {
            let key = vec_data[(q % n) as usize];
            if set_data.contains(&key) {
                found_h += 1;
            }
        }
        let hash_time = start.elapsed();

        let winner = if vec_time < hash_time { "Vec" } else { "HashSet" };
        println!(
            "n = {:4}: Vec = {:>8.3} ms, HashSet = {:>8.3} ms → {winner} wins",
            n,
            vec_time.as_secs_f64() * 1000.0,
            hash_time.as_secs_f64() * 1000.0,
        );
        // Prevent optimization
        assert_eq!(found_v, found_h);
    }
    println!();
}

// ── Demo 3: Ordered traversal — BTreeMap vs HashMap + sort ──────────

fn demo_ordered_traversal() {
    println!("=== Demo 3: Ordered Traversal — BTreeMap vs HashMap ===\n");

    let n = 100_000;
    let mut btree = BTreeMap::new();
    let mut hash = HashMap::new();

    // Insert shuffled data
    let mut keys: Vec<i64> = (0..n).collect();
    // Simple shuffle
    for i in (1..keys.len()).rev() {
        let j = (i as u64 * 2654435761 % keys.len() as u64) as usize;
        keys.swap(i, j);
    }
    for &k in &keys {
        btree.insert(k, k * 2);
        hash.insert(k, k * 2);
    }

    // BTreeMap: already ordered
    let start = Instant::now();
    let ordered_bt: Vec<_> = btree.iter().collect();
    let bt_time = start.elapsed();

    // HashMap: extract + sort
    let start = Instant::now();
    let mut ordered_hm: Vec<_> = hash.iter().collect();
    ordered_hm.sort_by_key(|(&k, _)| k);
    let hm_time = start.elapsed();

    println!("BTreeMap iteration (already ordered): {:.3} ms",
             bt_time.as_secs_f64() * 1000.0);
    println!("HashMap extract + sort:               {:.3} ms",
             hm_time.as_secs_f64() * 1000.0);
    println!("Both produced {} elements", ordered_bt.len());
    assert_eq!(ordered_bt.len(), ordered_hm.len());
    println!();
}

// ── Demo 4: Non-hashable keys — f64 with BTreeMap ───────────────────

fn demo_non_hashable() {
    println!("=== Demo 4: Non-Hashable Keys — f64 with BTreeMap ===\n");

    // f64 does NOT implement Hash, so HashMap<f64, _> won't compile.
    // BTreeMap works because f64... also doesn't implement Ord!
    // We need OrderedFloat or a wrapper.

    // Workaround 1: discretize
    let mut temp_map: BTreeMap<i64, &str> = BTreeMap::new();
    let discretize = |f: f64| -> i64 { (f * 100.0).round() as i64 };

    temp_map.insert(discretize(36.5), "normal");
    temp_map.insert(discretize(37.2), "low_fever");
    temp_map.insert(discretize(38.5), "fever");
    temp_map.insert(discretize(40.1), "high_fever");

    println!("Temperatures (discretized to centidegrees):");
    for (key, label) in &temp_map {
        println!("  {:.2}°C → {label}", *key as f64 / 100.0);
    }

    // Range query on temperatures: 37.0 to 39.0
    println!("\nRange [37.0, 39.0]:");
    for (key, label) in temp_map.range(discretize(37.0)..=discretize(39.0)) {
        println!("  {:.2}°C → {label}", *key as f64 / 100.0);
    }

    // Workaround 2: show the compilation error
    println!("\n// HashMap<f64, _> → compile error:");
    println!("// error[E0277]: the trait bound `f64: Hash` is not satisfied");
    println!("// error[E0277]: the trait bound `f64: Eq` is not satisfied");
    println!();
}

// ── Demo 5: Memory comparison ───────────────────────────────────────

fn demo_memory() {
    println!("=== Demo 5: Memory Comparison ===\n");

    let sizes = [100, 1_000, 10_000, 100_000];

    println!("{:<10} {:<20} {:<20} {:<20} {:<10}",
             "n", "Vec<i32>", "HashSet<i32>", "BTreeSet<i32>", "Ratio H/V");

    for &n in &sizes {
        let vec_bytes = n * std::mem::size_of::<i32>();

        // HashSet: capacity is at least n/0.875 rounded to power of 2,
        // plus 1 control byte per slot, plus metadata
        let hs_cap = (n as f64 / 0.875).ceil() as usize;
        let hs_cap_pow2 = hs_cap.next_power_of_two();
        let hs_bytes = hs_cap_pow2 * (std::mem::size_of::<i32>() + 1) // data + control
            + 3 * std::mem::size_of::<usize>(); // struct fields

        // BTreeSet: each node has up to 11 keys + 12 children pointers + metadata
        // Rough estimate: ~64 bytes per element
        let bt_bytes = n * 64;

        let ratio = hs_bytes as f64 / vec_bytes as f64;
        println!("{:<10} {:<20} {:<20} {:<20} {:.1}x",
                 n,
                 format!("{} B", vec_bytes),
                 format!("~{} B", hs_bytes),
                 format!("~{} B", bt_bytes),
                 ratio);
    }
    println!("\nVec (sorted) + binary_search is the most memory-efficient.");
    println!();
}

// ── Demo 6: Point lookup — where HashMap wins ───────────────────────

fn demo_point_lookup() {
    println!("=== Demo 6: Point Lookup — Where HashMap Wins ===\n");

    let n: usize = 100_000;
    let queries = 500_000;

    // Build structures
    let mut sorted_vec: Vec<u64> = (0..n as u64).map(|i| i * 3).collect();
    sorted_vec.sort(); // already sorted, but be explicit
    let hash_set: HashSet<u64> = sorted_vec.iter().cloned().collect();
    let btree_set: BTreeSet<u64> = sorted_vec.iter().cloned().collect();

    // Generate random query keys
    let query_keys: Vec<u64> = (0..queries)
        .map(|i| ((i as u64 * 2654435761) % (n as u64 * 3)))
        .collect();

    // Binary search on sorted Vec
    let start = Instant::now();
    let mut found_bs = 0u64;
    for &key in &query_keys {
        if sorted_vec.binary_search(&key).is_ok() {
            found_bs += 1;
        }
    }
    let bs_time = start.elapsed();

    // HashSet lookup
    let start = Instant::now();
    let mut found_hs = 0u64;
    for &key in &query_keys {
        if hash_set.contains(&key) {
            found_hs += 1;
        }
    }
    let hs_time = start.elapsed();

    // BTreeSet lookup
    let start = Instant::now();
    let mut found_bt = 0u64;
    for &key in &query_keys {
        if btree_set.contains(&key) {
            found_bt += 1;
        }
    }
    let bt_time = start.elapsed();

    println!("{queries} lookups on {n} elements:");
    println!("  Vec binary_search: {:.3} ms", bs_time.as_secs_f64() * 1000.0);
    println!("  HashSet:           {:.3} ms", hs_time.as_secs_f64() * 1000.0);
    println!("  BTreeSet:          {:.3} ms", bt_time.as_secs_f64() * 1000.0);

    assert_eq!(found_bs, found_hs);
    assert_eq!(found_bs, found_bt);
    println!("  All found {} hits\n", found_bs);
}

// ── Demo 7: Antipattern showcase ────────────────────────────────────

fn demo_antipatterns() {
    println!("=== Demo 7: Antipattern Showcase ===\n");

    // Antipattern 1: HashMap for dense integer keys
    println!("1) Dense integer keys (0..100):");
    let mut map: HashMap<usize, f64> = HashMap::new();
    let mut vec: Vec<f64> = Vec::with_capacity(100);
    for i in 0..100 {
        map.insert(i, (i as f64).sqrt());
        vec.push((i as f64).sqrt());
    }
    println!("   HashMap: {} entries, ~{} bytes capacity overhead",
             map.len(), map.capacity() * (std::mem::size_of::<usize>() + std::mem::size_of::<f64>() + 1));
    println!("   Vec:     {} entries, ~{} bytes",
             vec.len(), vec.len() * std::mem::size_of::<f64>());
    println!("   → Use Vec when keys are 0..n\n");

    // Antipattern 2: HashSet for small static membership test
    println!("2) Small static membership test:");
    let codes_set: HashSet<u16> = [200, 201, 204, 301, 302].iter().cloned().collect();
    let codes_arr: [u16; 5] = [200, 201, 204, 301, 302];

    let start = Instant::now();
    let mut found_s = 0u32;
    for _ in 0..1_000_000 {
        for code in 100..600u16 {
            if codes_set.contains(&code) { found_s += 1; }
        }
    }
    let set_time = start.elapsed();

    let start = Instant::now();
    let mut found_a = 0u32;
    for _ in 0..1_000_000 {
        for code in 100..600u16 {
            if codes_arr.contains(&code) { found_a += 1; }
        }
    }
    let arr_time = start.elapsed();

    println!("   HashSet: {:.3} ms", set_time.as_secs_f64() * 1000.0);
    println!("   Array:   {:.3} ms", arr_time.as_secs_f64() * 1000.0);
    println!("   Found: set={found_s}, arr={found_a}");
    println!("   → For 5 elements, array.contains() can match or beat HashSet\n");

    // Antipattern 3: HashMap when you need insertion order
    println!("3) Insertion order:");
    let mut config: HashMap<&str, &str> = HashMap::new();
    config.insert("host", "localhost");
    config.insert("port", "8080");
    config.insert("database", "mydb");
    config.insert("user", "admin");
    config.insert("timeout", "30");
    print!("   HashMap iteration order: ");
    for (k, _) in &config {
        print!("{k} ");
    }
    println!("← unpredictable!");
    println!("   → Use IndexMap for deterministic insertion-order iteration\n");
}

// ── Demo 8: Prefix search — where tries/BTreeMap win ────────────────

fn demo_prefix_search() {
    println!("=== Demo 8: Prefix Search — BTreeMap vs HashMap ===\n");

    let words = vec![
        "apple", "application", "apply", "apt", "banana", "band",
        "bandana", "bar", "bark", "barn", "cat", "car", "card",
        "care", "careful", "cargo", "carpet",
    ];

    let btree: BTreeSet<&str> = words.iter().cloned().collect();
    let hashset: HashSet<&str> = words.iter().cloned().collect();

    let prefix = "car";
    println!("Words starting with \"{prefix}\":\n");

    // BTreeMap: range from prefix to prefix+1 (lexicographic)
    println!("  BTreeSet (efficient range scan):");
    let upper = format!("{}~", prefix); // '~' > all lowercase letters
    for word in btree.range::<str, _>(prefix..upper.as_str()) {
        if word.starts_with(prefix) {
            println!("    {word}");
        }
    }

    // HashSet: must check every element
    println!("\n  HashSet (full scan required):");
    let mut prefix_results: Vec<_> = hashset.iter()
        .filter(|w| w.starts_with(prefix))
        .collect();
    prefix_results.sort();
    for word in prefix_results {
        println!("    {word}");
    }

    println!("\n  BTreeSet: O(log n + k) for prefix search");
    println!("  HashSet:  O(n) — must scan everything\n");
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  When NOT to Use Hash Tables — Rust Demonstrations  ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_range_query();
    demo_small_collection();
    demo_ordered_traversal();
    demo_non_hashable();
    demo_memory();
    demo_point_lookup();
    demo_antipatterns();
    demo_prefix_search();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Conclusion: HashMap is great — but not for         ║");
    println!("║  everything. Know your alternatives!                ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 13. Ejercicios

### Ejercicio 1 — Range query benchmark

Implementa un benchmark que compare `BTreeMap::range()` vs `HashMap` filtrado
para consultas de rango sobre $n = 10^5$ enteros con 1000 consultas aleatorias.

<details><summary>¿Qué factor de ventaja esperas para BTreeMap?</summary>

`BTreeMap::range()` es $O(\log n + k)$ donde $k$ es el tamaño del resultado.
Para un rango que cubre el 10% de los datos: $\log(10^5) + 10^4 \approx 10^4$
operaciones. `HashMap` filtrado es siempre $O(n) = 10^5$. Factor esperado:
~10x más rápido para `BTreeMap` en este caso.

</details>

### Ejercicio 2 — Umbral de crossover

Escribe un programa que mida el punto exacto donde `HashSet::contains` supera
a `Vec::contains` en tu máquina. Varía $n$ de 1 a 200 en incrementos de 5.
Grafica los resultados.

<details><summary>¿Alrededor de qué n esperas el crossover?</summary>

Típicamente entre $n = 20$ y $n = 50$, dependiendo del tamaño de la clave y
la arquitectura. Para claves `u64`, el crossover suele estar alrededor de
$n \approx 25$. Para `String`, puede ser mayor porque el hash de una string
es costoso comparado con `==` que puede abortar temprano.

</details>

### Ejercicio 3 — Float keys con discretización

Crea un `HashMap<i64, Vec<String>>` que agrupe mediciones de temperatura por
rango de 0.5°C (discretizando `f64` a `i64` con factor 2). Inserta 1000
mediciones aleatorias en `[35.0, 42.0]` y responde: ¿cuántas mediciones hay
entre 37.0°C y 38.0°C?

<details><summary>¿Cuántos buckets de 0.5°C cubren el rango [37.0, 38.0]?</summary>

El rango [37.0, 38.0] en unidades discretizadas (×2) es [74, 76]. Los buckets
son 74 (37.0-37.5) y 75 (37.5-38.0), es decir, 2 buckets. Las mediciones
con valor exacto 38.0 caen en el bucket 76 (38.0-38.5), que queda fuera del
rango estricto si se usa `<` en lugar de `<=`.

</details>

### Ejercicio 4 — Ordered iteration count

Dado un programa que inserta $n$ elementos y luego itera en orden $k$ veces,
¿para qué relación $k/n$ es más eficiente `BTreeMap` (iteración directa) que
`HashMap` + sort? Escribe la fórmula y verifica empíricamente.

<details><summary>¿Cuál es la relación teórica?</summary>

`BTreeMap`: inserción $O(n \log n)$ + iteración $O(kn)$.
`HashMap`: inserción $O(n)$ + cada iteración $O(n \log n)$ (extract + sort).
Total: $n \log n + kn$ vs $n + kn\log n$.
BTreeMap gana cuando $n \log n + kn < n + kn\log n$, es decir,
$n(\log n - 1) < kn(\log n - 1)$, simplificando: $k > 1$.
Basta con que necesites más de **una** iteración ordenada para que BTreeMap
sea preferible (asumiendo que la constante de BST y sort son similares).

</details>

### Ejercicio 5 — Prefix autocomplete

Implementa un autocompletado simple usando `BTreeSet<String>`. Dado un prefijo,
retorna las primeras 10 palabras que lo completan. Carga un diccionario de al
menos 10000 palabras y compara con un `HashSet` que filtra con `starts_with`.

<details><summary>¿Cuál es la complejidad de cada enfoque?</summary>

`BTreeSet::range()` + `take(10)`: $O(\log n + 10)$ — excelente.
`HashSet::iter().filter()`: $O(n)$ — debe revisar todas las palabras.
Para $n = 10^4$: ~14 vs ~10000 operaciones, factor ~700x teórico.
En la práctica el factor es menor porque las constantes difieren, pero
`BTreeSet` será significativamente más rápido para este caso de uso.

</details>

### Ejercicio 6 — Memory profiling

Escribe un programa que mida la memoria real (usando
`std::alloc::GlobalAlloc` con un contador) de `Vec<u32>` sorted,
`HashSet<u32>`, y `BTreeSet<u32>` para $n \in \{100, 1000, 10000\}$.
¿Cuántos bytes por elemento usa cada estructura?

<details><summary>¿Qué bytes/elemento esperas para cada estructura?</summary>

- `Vec<u32>`: ~4 bytes/elem (solo los datos, más overhead fijo del Vec).
- `HashSet<u32>`: ~9-13 bytes/elem (4 bytes dato + 1 byte control +
  espacio vacío por $\alpha \le 0.875$, redondeado a potencia de 2).
- `BTreeSet<u32>`: ~40-64 bytes/elem (nodos B-tree con punteros hijos,
  metadata de longitud, alineación). BTreeSet es la más costosa en memoria.

</details>

### Ejercicio 7 — Worst-case latency

Crea un programa que mida la latencia del **peor caso** (máximo de $10^6$
lookups individuales) para `HashMap` vs `BTreeMap`. Usa `Instant::now()`
alrededor de cada lookup individual.

<details><summary>¿Cuál estructura tendrá peor caso más alto?</summary>

`HashMap` tendrá picos de latencia más altos por dos razones:
1. Un rehash durante un insert causa un spike de $O(n)$.
2. Colisiones extremas (poco probables con SipHash pero posibles) causan
   cadenas largas. `BTreeMap` tiene $O(\log n)$ garantizado: ~20 comparaciones
   para $n = 10^6$, sin spikes. El peor caso de HashMap puede ser 100-1000x
   más lento que su promedio durante un rehash.

</details>

### Ejercicio 8 — Decision matrix

Dado un sistema de reservas de hotel que necesita: (a) buscar reservas por
ID, (b) listar reservas por fecha en orden, (c) encontrar la reserva más
próxima a una fecha dada. ¿Qué estructura(s) de datos usarías y por qué?

<details><summary>¿Cuál es la solución recomendada?</summary>

Usar **dos** estructuras:
1. `HashMap<ReservationId, Reservation>` — para búsqueda por ID en $O(1)$.
2. `BTreeMap<DateTime, Vec<ReservationId>>` — para consultas por rango de
   fecha en $O(\log n + k)$ y para encontrar la reserva más próxima con
   `range()` buscando antes y después de la fecha objetivo.

Una sola estructura no cubre ambos requisitos eficientemente. Este patrón
de "índice primario + índice secundario" es exactamente lo que hacen las
bases de datos: una tabla con múltiples índices.

</details>

### Ejercicio 9 — Sparse vs dense

Tienes un tablero de juego de $1000 \times 1000$ donde solo ~100 casillas
están ocupadas. Compara `HashMap<(u16, u16), Piece>` vs
`Vec<Vec<Option<Piece>>>` (matriz 2D). ¿Cuál es mejor en tiempo? ¿Y en
memoria?

<details><summary>¿Cuál gana en cada métrica?</summary>

**Memoria**: HashMap gana ampliamente. La matriz 2D necesita
$10^6 \times \text{size\_of::<Option<Piece>>}$ bytes (al menos 1-2 MB
incluso si `Piece` es pequeño). El HashMap con 100 entradas usa ~2-4 KB.
**Tiempo de acceso**: la matriz 2D gana — acceso directo `board[y][x]` es
$O(1)$ con una multiplicación + offset, sin hash ni colisiones. Pero la
diferencia es pequeña para 100 accesos. **Iteración sobre piezas**: HashMap
gana — itera solo 100 entradas vs escanear $10^6$ celdas.
**Veredicto**: para tablero sparse (0.01% ocupado), HashMap es claramente
mejor. Si la ocupación fuera >10%, la matriz sería competitiva.

</details>

### Ejercicio 10 — Implementa la estructura correcta

Para cada escenario, implementa la solución **sin** usar hash tables:
1. Agenda telefónica ordenada por apellido (usa `BTreeMap`).
2. Top-5 scores en un juego que se actualiza frecuentemente (usa `BinaryHeap`
   o `Vec` sorted).
3. Cola de prioridad de tareas con deadline (usa `BTreeMap<DateTime, Task>`).

<details><summary>¿Por qué BinaryHeap no sirve para (3)?</summary>

`BinaryHeap` no permite extraer un elemento arbitrario ni hacer range
queries por deadline. Solo permite ver/extraer el máximo (o mínimo con
`Reverse`). Para "todas las tareas con deadline antes de X" necesitas
`BTreeMap::range(..x)`, que es $O(\log n + k)$. Con `BinaryHeap` tendrías
que vaciar el heap completo para encontrar todos los vencidos: $O(n \log n)$.
Para (2), `BinaryHeap` sí funciona si solo necesitas los top-5: inserta
todo y haz 5 `pop()` — pero un `Vec` ordenado por inserción es más simple
para conjuntos pequeños.

</details>

---

## 14. Resumen

Las hash tables son una herramienta poderosa pero no universal. La clave para
elegir correctamente es responder estas preguntas:

1. **¿Necesito orden?** → BST / B-tree.
2. **¿Son pocos elementos?** → Array / Vec.
3. **¿Las claves son hasheables?** → Si no, BTreeMap con Ord.
4. **¿Necesito garantías de peor caso?** → BST balanceado.
5. **¿La memoria es crítica?** → Array ordenado + binary search.
6. **¿Necesito búsqueda por prefijo?** → Trie / BTreeSet.
7. **¿Las claves son un rango denso de enteros?** → Array directo.

No existe una estructura de datos perfecta para todo — y reconocer cuándo **no**
usar una hash table es tan importante como saber usarla.

> *"When all you have is a hammer, everything looks like a nail."*
> — Abraham Maslow (attributed)

Con esto concluimos el **Capítulo 10 — Tablas hash**.
