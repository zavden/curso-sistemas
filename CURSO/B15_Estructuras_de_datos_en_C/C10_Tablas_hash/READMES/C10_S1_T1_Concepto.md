# Concepto de hashing

## Objetivo

Comprender la **tabla hash** como estructura de búsqueda con acceso
$O(1)$ promedio, y cómo transforma el problema de búsqueda en un
problema de **mapeo directo** de claves a posiciones:

- **Función hash**: convierte una clave en un índice entero.
- **Tabla**: array donde cada posición (bucket) almacena un
  elemento o una lista de elementos.
- **Colisión**: cuando dos claves distintas producen el mismo
  índice.
- Por qué $O(1)$ es promedio, no garantizado.
- Relación con las estructuras de búsqueda vistas en C09
  (BST, B-tree, trie).

---

## El problema de la búsqueda: resumen de C09

Hasta ahora tenemos estas complejidades de búsqueda:

| Estructura | Búsqueda | Inserción | Orden | Prefix |
|------------|:--------:|:---------:|:-----:|:------:|
| Array no ordenado | $O(n)$ | $O(1)$ | no | no |
| Array ordenado | $O(\log n)$ | $O(n)$ | sí | no |
| BST balanceado | $O(\log n)$ | $O(\log n)$ | sí | no |
| B-tree | $O(\log_t n)$ | $O(\log_t n)$ | sí | no |
| Trie | $O(m)$ | $O(m)$ | sí | sí |

Todas dependen de $n$ (número de elementos) o $m$ (longitud de
clave). ¿Es posible buscar en tiempo **constante**, independiente
de $n$ y $m$?

---

## La idea: acceso directo por índice

Un array permite acceso $O(1)$ por **posición**: `a[5]` es
instantáneo. Si pudiéramos convertir cualquier clave en una
posición de array, tendríamos búsqueda $O(1)$.

### El caso trivial: claves son enteros pequeños

Si las claves son enteros en el rango $[0, M)$ con $M$ razonable,
se usa un **array de acceso directo**:

```c
#define M 1000
int table[M];       // table[key] = value

table[42] = 100;    // insert(42, 100): O(1)
int v = table[42];  // search(42):      O(1)
```

Esto funciona perfectamente si $M$ es pequeño. Pero si las claves
son:
- Enteros de 64 bits: $M = 2^{64} = 1.8 \times 10^{19}$ — array
  imposible.
- Strings: el espacio de claves es infinito.
- Objetos complejos: no son directamente indices.

### La solución: función hash

Una **función hash** $h(k)$ convierte cualquier clave $k$ en un
entero en el rango $[0, m)$ donde $m$ es el tamaño de la tabla:

$$h: \text{Claves} \rightarrow \{0, 1, \ldots, m-1\}$$

```
clave "alice" → h("alice") = 3 → table[3] = valor
clave "bob"   → h("bob")   = 7 → table[7] = valor
clave 42      → h(42)      = 2 → table[2]  = valor
```

Ahora se puede usar un array de tamaño $m$ (razonable) para
almacenar cualquier tipo de clave.

---

## Anatomía de una tabla hash

```
Clave k → [Funcion hash h(k)] → indice i → [Tabla[i]] → valor

Ejemplo con m = 8:

h("alice") = 3    tabla[0]: (vacia)
h("bob")   = 7    tabla[1]: (vacia)
h("carol") = 1    tabla[2]: (vacia)
h("dave")  = 3    tabla[3]: "alice" → colision con "dave"!
                   tabla[4]: (vacia)
                   tabla[5]: (vacia)
                   tabla[6]: (vacia)
                   tabla[7]: "bob"
```

### Componentes

1. **Array de buckets**: `table[0..m-1]`, donde $m$ es la
   **capacidad** de la tabla.
2. **Función hash**: $h(k)$ que retorna un índice en $[0, m)$.
3. **Estrategia de colisión**: qué hacer cuando $h(k_1) = h(k_2)$
   con $k_1 \neq k_2$.
4. **Factor de carga**: $\alpha = n/m$ donde $n$ es el número de
   elementos almacenados.

### Operaciones básicas

```
INSERT(table, key, value):
    i = h(key) % m
    almacenar (key, value) en table[i]     // con manejo de colision

SEARCH(table, key):
    i = h(key) % m
    buscar key en table[i]                 // puede haber multiples en i

DELETE(table, key):
    i = h(key) % m
    eliminar key de table[i]
```

Cada operación es $O(1)$ si `table[i]` contiene a lo sumo una
cantidad constante de elementos.

---

## Por qué $O(1)$ es promedio, no peor caso

### El principio del palomar (pigeonhole principle)

Si tenemos $n > m$ claves y $m$ buckets, al menos un bucket
contiene más de una clave. Pero incluso con $n < m$, las
colisiones son **inevitables** en la práctica.

**Birthday paradox**: con solo 23 personas, hay >50% de
probabilidad de que dos compartan cumpleaños (365 posibles). De
manera análoga, con $\sqrt{m}$ claves en $m$ buckets, la
probabilidad de colisión ya es ~50%.

Para $m = 1000$ buckets:
- Con $n = 32$ claves: ~50% de probabilidad de al menos una
  colisión.
- Con $n = 100$ claves: ~99.4% de probabilidad.
- Con $n = 500$ claves: colisiones casi seguras.

### Peor caso: $O(n)$

Si una función hash mala mapea todas las claves al mismo bucket,
todos los elementos están en una sola posición y la búsqueda
degenera a $O(n)$ (como una lista enlazada o un array lineal).

```
Funcion hash terrible: h(k) = 0 para toda k

tabla[0]: alice → bob → carol → dave → eve → ...
tabla[1]: (vacia)
tabla[2]: (vacia)
...

Buscar "eve": recorrer toda la lista en tabla[0] = O(n)
```

### Caso promedio: $O(1 + \alpha)$

Con una buena función hash (distribución uniforme) y factor de
carga $\alpha = n/m$ acotado (e.g., $\alpha < 0.75$), el número
esperado de elementos por bucket es $\alpha$. La búsqueda cuesta:

$$T_{\text{promedio}} = O(1 + \alpha) = O(1) \quad \text{si } \alpha = O(1)$$

El $1$ es el costo de calcular $h(k)$ y acceder a `table[i]`. El
$\alpha$ es el costo esperado de buscar dentro del bucket (que
tiene ~$\alpha$ elementos en promedio).

---

## Función hash: requisitos mínimos

Una función hash $h$ debe cumplir:

### 1. Determinismo

La misma clave siempre produce el mismo hash:

$$k_1 = k_2 \implies h(k_1) = h(k_2)$$

Sin esto, se inserta un elemento y después no se puede encontrar.

### 2. Uniformidad

Las claves se distribuyen lo más uniformemente posible entre los
$m$ buckets. Idealmente:

$$P[h(k) = i] = \frac{1}{m} \quad \forall i \in [0, m)$$

Sin esto, algunos buckets tienen muchos elementos y otros están
vacíos, degradando el rendimiento.

### 3. Eficiencia

Calcular $h(k)$ debe ser rápido — idealmente $O(1)$ para claves
de tamaño fijo, $O(m_k)$ para claves de longitud $m_k$ (no se
puede hacer mejor que leer toda la clave).

### Propiedad no requerida: inyectividad

$h(k_1) = h(k_2)$ **no implica** $k_1 = k_2$. Dos claves
distintas pueden (y van a) producir el mismo hash. Esto es una
**colisión**, y es inevitable cuando el espacio de claves es mayor
que $m$.

---

## Función hash simple para enteros

La más básica: **método de la división**.

$$h(k) = k \bmod m$$

```c
int hash_int(int key, int m) {
    return ((key % m) + m) % m;  // manejar negativos
}
```

El doble módulo maneja claves negativas: en C, `(-7) % 10` puede
ser $-7$, y necesitamos un resultado en $[0, m)$.

### Elección de $m$

No todos los valores de $m$ son iguales:

- **$m$ potencia de 2** ($m = 2^k$): rápido (`key & (m-1)`), pero
  solo usa los $k$ bits menos significativos. Si las claves tienen
  patrones en los bits bajos, la distribución es mala.
- **$m$ primo**: distribuye mejor. Especialmente si las claves
  tienen patrones (multiplos de algún número).
- **$m$ primo lejos de potencias de 2**: ideal.

Ejemplo: con $m = 10$ y claves $\{10, 20, 30, 40, 50\}$:
$h = \{0, 0, 0, 0, 0\}$ — todas colisionan. Con $m = 7$:
$h = \{3, 6, 2, 5, 1\}$ — distribución perfecta.

---

## Función hash simple para strings

Para strings, se necesita combinar todos los caracteres en un solo
entero. La idea básica es tratar la cadena como un número en alguna
base:

$$h(s) = \left(\sum_{i=0}^{n-1} s[i] \cdot b^{n-1-i}\right) \bmod m$$

donde $b$ es una base (típicamente un primo como 31 o 37).

### Ejemplo: hash polinomial

```c
unsigned int hash_string(const char *s, int m) {
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h = h * 31 + (unsigned char)s[i];
    return h % m;
}
```

¿Por qué 31? Es primo, y $31 = 2^5 - 1$, así que el compilador
puede optimizar `h * 31` como `(h << 5) - h`.

### Función hash mala: solo sumar caracteres

```c
unsigned int bad_hash(const char *s, int m) {
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h += (unsigned char)s[i];
    return h % m;
}
```

Esto mapea todos los anagramas al mismo bucket: `"abc"`, `"bca"`,
`"cab"` todos producen el mismo hash. La posición del carácter no
influye — se pierde información.

---

## Tabla hash vs otras estructuras

| Operación | Hash table | Array ordenado | BST balanceado | Trie |
|-----------|:----------:|:--------------:|:--------------:|:----:|
| Búsqueda | $O(1)$ avg | $O(\log n)$ | $O(\log n)$ | $O(m)$ |
| Inserción | $O(1)$ avg | $O(n)$ | $O(\log n)$ | $O(m)$ |
| Eliminación | $O(1)$ avg | $O(n)$ | $O(\log n)$ | $O(m)$ |
| Mínimo/máximo | $O(n)$ | $O(1)$ | $O(\log n)$ | $O(?)$ |
| Rango $[a, b]$ | $O(n)$ | $O(\log n + k)$ | $O(\log n + k)$ | $O(m+k)$ |
| Orden | no | sí | sí | sí (lex) |
| Peor caso | $O(n)$ | $O(\log n)$ | $O(\log n)$ | $O(m)$ |

### La trampa del $O(1)$

El $O(1)$ de la tabla hash es **amortizado promedio**, con
supuestos:
1. Buena función hash (distribución uniforme).
2. Factor de carga acotado (redimensionar cuando $\alpha$ crece).
3. No hay adversario eligiendo claves maliciosamente (para
   protegerse: hash aleatorizado — ver C10/S03/T04).

El BST balanceado tiene $O(\log n)$ **garantizado** sin supuestos
adicionales. Para aplicaciones donde el peor caso importa
(sistemas de tiempo real), un BST puede ser preferible.

---

## Ejemplo conceptual: agenda telefónica

```
Problema: almacenar (nombre, telefono) y buscar por nombre.

Solucion 1 — Array no ordenado:
  Insertar: O(1) (agregar al final)
  Buscar:   O(n) (recorrer todo)

Solucion 2 — Array ordenado:
  Insertar: O(n) (shift para mantener orden)
  Buscar:   O(log n) (busqueda binaria)

Solucion 3 — BST balanceado:
  Insertar: O(m log n) (m = longitud del nombre)
  Buscar:   O(m log n)

Solucion 4 — Hash table:
  Insertar: O(m) promedio (calcular hash + insertar)
  Buscar:   O(m) promedio (calcular hash + buscar en bucket)
```

Para 10000 contactos con nombres de ~10 caracteres:
- Array: ~5000 comparaciones de strings promedio.
- BST: ~14 comparaciones de strings × 10 chars = ~140 ops.
- Hash: 1 cálculo de hash (10 ops) + ~1 comparación = ~20 ops.

---

## Estructura mínima en C

```c
#define TABLE_SIZE 101  // primo

typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;  // chaining para colisiones
} Entry;

typedef struct {
    Entry *buckets[TABLE_SIZE];
    int count;
} HashTable;
```

Con encadenamiento separado (chaining): cada bucket es una lista
enlazada de pares (clave, valor). Las secciones S02 y S03 cubren
las implementaciones en detalle.

---

## Programa completo en C

```c
// hash_concept.c — hash table concept demonstration
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- hash functions ---

unsigned int hash_bad(const char *s, int m) {
    // bad: just sum characters (anagrams collide)
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h += (unsigned char)s[i];
    return h % m;
}

unsigned int hash_good(const char *s, int m) {
    // good: polynomial hash with base 31
    unsigned int h = 0;
    for (int i = 0; s[i]; i++)
        h = h * 31 + (unsigned char)s[i];
    return h % m;
}

unsigned int hash_int(int key, int m) {
    return ((key % m) + m) % m;
}

// --- simple chaining hash table ---

#define M 11  // small prime for demonstration

typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry *buckets[M];
    int count;
} HashTable;

HashTable *ht_create(void) {
    return calloc(1, sizeof(HashTable));
}

void ht_insert(HashTable *ht, const char *key, int value) {
    unsigned int i = hash_good(key, M);

    // check if key exists
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;  // update
            return;
        }
    }

    // new entry
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = value;
    e->next = ht->buckets[i];
    ht->buckets[i] = e;
    ht->count++;
}

int ht_search(HashTable *ht, const char *key, int *value) {
    unsigned int i = hash_good(key, M);
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (value) *value = e->value;
            return 1;
        }
    }
    return 0;
}

void ht_print(HashTable *ht) {
    for (int i = 0; i < M; i++) {
        printf("  [%2d]: ", i);
        for (Entry *e = ht->buckets[i]; e; e = e->next) {
            printf("(%s, %d) ", e->key, e->value);
            if (e->next) printf("-> ");
        }
        printf("\n");
    }
}

void ht_free(HashTable *ht) {
    for (int i = 0; i < M; i++) {
        Entry *e = ht->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht);
}

// --- demos ---

int main(void) {
    // demo 1: hash function output
    printf("=== Demo 1: hash function comparison ===\n");
    const char *words[] = {"hello", "world", "abc", "bca", "cab",
                           "dog", "god", "cat", "act", "tac"};
    int nwords = 10;

    printf("%-8s  bad(%%11)  good(%%11)\n", "word");
    printf("-------  --------  ---------\n");
    for (int i = 0; i < nwords; i++) {
        printf("%-8s  %5u     %5u\n", words[i],
               hash_bad(words[i], M), hash_good(words[i], M));
    }
    printf("\nNote: anagrams (abc/bca/cab, dog/god, cat/act/tac)\n");
    printf("  bad hash:  same bucket for anagrams\n");
    printf("  good hash: different buckets\n\n");

    // demo 2: basic hash table operations
    printf("=== Demo 2: hash table operations ===\n");
    HashTable *ht = ht_create();

    ht_insert(ht, "alice", 1234);
    ht_insert(ht, "bob", 5678);
    ht_insert(ht, "carol", 9012);
    ht_insert(ht, "dave", 3456);
    ht_insert(ht, "eve", 7890);

    printf("Table after 5 insertions (m=%d):\n", M);
    ht_print(ht);

    int val;
    printf("\nSearches:\n");
    if (ht_search(ht, "carol", &val))
        printf("  search(carol) = %d\n", val);
    if (ht_search(ht, "eve", &val))
        printf("  search(eve)   = %d\n", val);
    if (!ht_search(ht, "frank", &val))
        printf("  search(frank) = NOT FOUND\n");

    printf("\nLoad factor: alpha = %d/%d = %.2f\n\n",
           ht->count, M, (double)ht->count / M);

    // demo 3: collision demonstration
    printf("=== Demo 3: collisions ===\n");
    HashTable *ht2 = ht_create();

    // insert words that hash to same bucket
    printf("Inserting into table of size %d:\n", M);
    const char *many[] = {"alpha", "beta", "gamma", "delta", "epsilon",
                          "zeta", "eta", "theta", "iota", "kappa",
                          "lambda", "mu"};
    for (int i = 0; i < 12; i++) {
        unsigned int h = hash_good(many[i], M);
        printf("  h(\"%s\") = %u\n", many[i], h);
        ht_insert(ht2, many[i], i);
    }

    printf("\nTable (alpha = %.2f):\n", 12.0 / M);
    ht_print(ht2);

    // count max chain length
    int max_chain = 0;
    for (int i = 0; i < M; i++) {
        int len = 0;
        for (Entry *e = ht2->buckets[i]; e; e = e->next)
            len++;
        if (len > max_chain) max_chain = len;
    }
    printf("\nMax chain length: %d\n", max_chain);
    printf("Ideal (uniform): %.1f per bucket\n\n", 12.0 / M);

    // demo 4: integer hash
    printf("=== Demo 4: integer hash ===\n");
    printf("hash_int with m = 7:\n");
    int keys[] = {0, 7, 14, 21, 3, 10, -5, 100};
    for (int i = 0; i < 8; i++) {
        printf("  h(%4d) = %u\n", keys[i], hash_int(keys[i], 7));
    }
    printf("Note: multiples of 7 all map to 0 (bad if m divides keys)\n\n");

    // demo 5: table size matters
    printf("=== Demo 5: table size effect ===\n");
    int sizes[] = {8, 10, 11, 16, 17};
    const char *test_words[] = {"one", "two", "three", "four", "five",
                                "six", "seven", "eight", "nine", "ten"};
    for (int s = 0; s < 5; s++) {
        int buckets_used = 0;
        int bucket_count[64] = {0};
        for (int i = 0; i < 10; i++) {
            unsigned int h = hash_good(test_words[i], sizes[s]);
            bucket_count[h]++;
        }
        for (int i = 0; i < sizes[s]; i++) {
            if (bucket_count[i] > 0) buckets_used++;
        }
        int max_b = 0;
        for (int i = 0; i < sizes[s]; i++) {
            if (bucket_count[i] > max_b) max_b = bucket_count[i];
        }
        printf("  m=%2d: buckets used=%d/%d, max chain=%d, alpha=%.2f\n",
               sizes[s], buckets_used, sizes[s], max_b, 10.0 / sizes[s]);
    }
    printf("Primes (11, 17) tend to distribute better\n\n");

    // demo 6: O(1) vs O(n) illustration
    printf("=== Demo 6: access pattern ===\n");
    printf("Searching for 'carol' in different structures:\n");
    printf("  Array (unsorted, 5 elements): up to 5 comparisons\n");
    printf("  Array (sorted, 5 elements):   ~3 comparisons (binary search)\n");
    printf("  BST (balanced, 5 elements):   ~3 comparisons\n");
    printf("  Hash table:  1 hash + 1 comparison = O(1)\n\n");

    printf("With 1,000,000 elements:\n");
    printf("  Array (unsorted): up to 1,000,000 comparisons\n");
    printf("  Array (sorted):   ~20 comparisons\n");
    printf("  BST (balanced):   ~20 comparisons\n");
    printf("  Hash table:       1 hash + ~1 comparison = O(1)\n");
    printf("  (assuming good hash and alpha < 0.75)\n");

    ht_free(ht);
    ht_free(ht2);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -o hash_concept hash_concept.c
./hash_concept
```

---

## Programa completo en Rust

```rust
// hash_concept.rs — hash table concept: hashing, collisions, comparison
use std::collections::HashMap;
use std::hash::{Hash, Hasher, DefaultHasher};

// --- custom hash functions for demonstration ---

fn hash_bad(s: &str, m: usize) -> usize {
    // bad: just sum bytes (anagrams collide)
    let sum: u32 = s.bytes().map(|b| b as u32).sum();
    (sum as usize) % m
}

fn hash_good(s: &str, m: usize) -> usize {
    // good: polynomial with base 31
    let mut h: u64 = 0;
    for b in s.bytes() {
        h = h.wrapping_mul(31).wrapping_add(b as u64);
    }
    (h as usize) % m
}

fn std_hash<T: Hash>(val: &T) -> u64 {
    let mut hasher = DefaultHasher::new();
    val.hash(&mut hasher);
    hasher.finish()
}

fn main() {
    // demo 1: hash function comparison
    println!("=== Demo 1: hash function comparison ===");
    let words = ["hello", "world", "abc", "bca", "cab",
                 "dog", "god", "cat", "act", "tac"];
    let m = 11;

    println!("{:<8}  bad(%{})  good(%{})  std", m, m);
    println!("-------  --------  ---------  ---");
    for w in &words {
        println!("{:<8}  {:>5}     {:>5}      {:>5}",
                 w, hash_bad(w, m), hash_good(w, m),
                 (std_hash(w) as usize) % m);
    }
    println!("\nAnagrams (abc/bca/cab): bad gives same, good/std differ\n");

    // demo 2: HashMap basic usage
    println!("=== Demo 2: HashMap basics ===");
    let mut phone_book: HashMap<&str, &str> = HashMap::new();

    phone_book.insert("alice", "555-0101");
    phone_book.insert("bob", "555-0102");
    phone_book.insert("carol", "555-0103");
    phone_book.insert("dave", "555-0104");
    phone_book.insert("eve", "555-0105");

    println!("Phone book: {:?}", phone_book);
    println!("search(carol): {:?}", phone_book.get("carol"));
    println!("search(frank): {:?}", phone_book.get("frank"));
    println!("contains(bob): {}", phone_book.contains_key("bob"));
    println!("len: {}", phone_book.len());
    println!();

    // demo 3: collision visualization
    println!("=== Demo 3: collision visualization ===");
    let many = ["alpha", "beta", "gamma", "delta", "epsilon",
                "zeta", "eta", "theta", "iota", "kappa",
                "lambda", "mu"];

    let mut buckets: Vec<Vec<&str>> = vec![vec![]; m];
    for &w in &many {
        let h = hash_good(w, m);
        println!("  h(\"{}\") = {}", w, h);
        buckets[h].push(w);
    }

    println!("\nBucket distribution (m={}):", m);
    for (i, bucket) in buckets.iter().enumerate() {
        if !bucket.is_empty() {
            println!("  [{}]: {:?}", i, bucket);
        }
    }
    let max_chain = buckets.iter().map(|b| b.len()).max().unwrap_or(0);
    println!("Max chain: {}", max_chain);
    println!("Load factor: {}/{} = {:.2}\n", many.len(), m,
             many.len() as f64 / m as f64);

    // demo 4: hash of different types
    println!("=== Demo 4: hashing different types ===");
    println!("hash(42i32)   = {}", std_hash(&42i32));
    println!("hash(42i64)   = {}", std_hash(&42i64));
    println!("hash(\"hello\") = {}", std_hash(&"hello"));
    println!("hash((1, 2))  = {}", std_hash(&(1, 2)));
    println!("hash(true)    = {}", std_hash(&true));

    // f64 does NOT implement Hash (NaN != NaN violates Eq)
    // println!("hash(3.14f64) = {}", std_hash(&3.14f64)); // compile error!
    println!("\nNote: f64 does NOT implement Hash (NaN != NaN breaks Eq)\n");

    // demo 5: HashMap vs manual simulation
    println!("=== Demo 5: what HashMap does internally ===");
    let key = "carol";
    let hash_val = std_hash(&key);
    println!("key: \"{}\"", key);
    println!("hash value: {} (u64)", hash_val);
    println!("if capacity=16: bucket = {} % 16 = {}", hash_val, hash_val % 16);
    println!("if capacity=32: bucket = {} % 32 = {}", hash_val, hash_val % 32);
    println!("(actual HashMap uses more sophisticated bucket selection)\n");

    // demo 6: O(1) average demonstration
    println!("=== Demo 6: O(1) average — scaling ===");
    for &n in &[100, 1_000, 10_000, 100_000, 1_000_000] {
        let mut map: HashMap<i32, i32> = HashMap::new();
        for i in 0..n {
            map.insert(i, i * 10);
        }

        // search for a few keys
        let found = map.get(&(n / 2));
        let capacity = map.capacity();
        let load = n as f64 / capacity as f64;

        println!("  n={:>9}: capacity={:>9}, alpha={:.2}, get({})={:?}",
                 n, capacity, load, n / 2, found.map(|_| "found"));
    }
    println!("Note: HashMap automatically resizes to keep alpha low\n");

    // demo 7: what hash tables CAN'T do
    println!("=== Demo 7: limitations ===");
    let map: HashMap<i32, &str> = [
        (10, "a"), (5, "b"), (20, "c"), (3, "d"), (15, "e"),
    ].into_iter().collect();

    println!("HashMap: {:?}", map);
    println!("Iteration order: NOT sorted (implementation-dependent)");
    print!("  Keys: ");
    for &k in map.keys() {
        print!("{} ", k);
    }
    println!("\n");

    println!("Operations hash tables can't do efficiently:");
    println!("  - Find minimum/maximum: O(n) not O(1)");
    println!("  - Range query [10..20]: O(n) not O(log n + k)");
    println!("  - Ordered iteration: O(n log n) with sort");
    println!("  - Prefix search: O(n) not O(m + k)");
    println!("  → Use BTreeMap, sorted Vec, or Trie for these");
}
```

Compilar y ejecutar:

```bash
rustc hash_concept.rs && ./hash_concept
```

---

## Errores frecuentes

1. **No manejar negativos en el módulo**: en C,
   `(-7) % 10` es $-7$ o $3$ dependiendo de la implementación
   (C99: resultado tiene el signo del dividendo). Usar
   `((key % m) + m) % m` para garantizar resultado positivo.
2. **Usar $m$ potencia de 2 con hash de división**: `key % 16`
   solo usa los 4 bits bajos. Si las claves tienen patrones
   (pares, múltiplos), la distribución es terrible.
3. **Hash de strings sumando caracteres**: los anagramas colisionan
   siempre. Usar hash polinomial con base (31, 37, etc.).
4. **Confundir $O(1)$ promedio con $O(1)$ garantizado**: una tabla
   hash puede degradar a $O(n)$ con mala distribución o sin
   redimensionamiento.
5. **Olvidar comparar la clave completa**: después de encontrar el
   bucket con $h(k)$, hay que comparar la clave almacenada con la
   clave buscada. Solo el hash no es suficiente — diferentes claves
   pueden tener el mismo hash.

---

## Ejercicios

### Ejercicio 1 — Hash a mano

Calcula $h(k) = k \bmod 11$ para las claves
$\{0, 11, 22, 5, 16, 27, 3, 14, 25, 7\}$. ¿Cuáles colisionan?
¿En cuántos buckets distintos caen las 10 claves?

<details><summary>Predice antes de ejecutar</summary>

Los múltiplos de 11 ($0, 11, 22$) tienen el mismo hash. ¿Qué
otros pares colisionan? ¿Cuántos buckets de los 11 quedan vacíos?

</details>

### Ejercicio 2 — Hash de strings a mano

Usa la función `h = (h * 31 + c) % 7` para calcular el hash de
`"abc"` y `"bca"`. Muestra cada paso.

<details><summary>Predice antes de ejecutar</summary>

¿Producen el mismo hash? ¿Y si usaras la función mala (solo sumar
caracteres)?

</details>

### Ejercicio 3 — Birthday paradox

Para una tabla de $m = 365$ buckets, calcula la probabilidad de
al menos una colisión con $n = 10, 20, 23, 30, 50$ claves
(asumiendo hash uniforme). Fórmula:
$P(\text{colisión}) = 1 - \prod_{i=0}^{n-1} \frac{m-i}{m}$.

<details><summary>Predice antes de ejecutar</summary>

¿Con cuántas claves la probabilidad supera 50%? ¿Y 90%? ¿Es
sorprendente lo bajo que es el número?

</details>

### Ejercicio 4 — Distribución empírica

Inserta 1000 strings aleatorios de longitud 8 (a-z) en una tabla
de $m = 127$ (primo) y $m = 128$ (potencia de 2) usando
`hash_good`. Para cada $m$, reporta: buckets vacíos, longitud
máxima de cadena, y longitud promedio.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál $m$ tendrá mejor distribución? ¿La diferencia será
significativa para `hash_good` (que ya mezcla bien los bits)?

</details>

### Ejercicio 5 — Degradación con hash mala

Inserta las palabras `{"abc", "bca", "cab", "bac", "acb", "cba"}`
(todos anagramas de "abc") en una tabla de $m = 11$ usando
`hash_bad` y `hash_good`. Muestra la estructura de la tabla en
cada caso.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas colisiones produce `hash_bad`? ¿Y `hash_good`?
¿La búsqueda más cara con `hash_bad` equivale a qué estructura?

</details>

### Ejercicio 6 — Tabla de acceso directo

Implementa una tabla de acceso directo para claves en $[0, 999]$.
Compara el tiempo de 10000 búsquedas con una tabla hash de $m = 127$.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será más rápida? ¿Cuánta memoria usa cada una?
¿Hay alguna razón para preferir la tabla hash si las claves
siempre están en $[0, 999]$?

</details>

### Ejercicio 7 — HashMap en Rust: conteo de palabras

Usa `HashMap<String, u32>` para contar la frecuencia de cada
palabra en un texto. Imprime las 10 palabras más frecuentes.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál es la forma idiomática en Rust para incrementar un contador
en un HashMap? ¿Qué pasa si la clave no existe?

</details>

### Ejercicio 8 — Hash de tipos compuestos en Rust

Define un struct `Point { x: i32, y: i32 }` y úsalo como clave de
un `HashMap`. Implementa `Hash` y `Eq` manualmente. Luego hazlo
con `#[derive(Hash, Eq, PartialEq)]` y verifica que ambos producen
el mismo comportamiento.

<details><summary>Predice antes de ejecutar</summary>

¿Qué pasa si implementas `Hash` sin implementar `Eq`? ¿Compila?
¿Y si dos puntos iguales producen hashes diferentes?

</details>

### Ejercicio 9 — Comparación de tiempos

Para $n = 10^5$ enteros aleatorios, mide el tiempo de inserción y
búsqueda en: (a) `Vec` no ordenado con búsqueda lineal, (b) `Vec`
ordenado con `binary_search`, (c) `BTreeMap`, (d) `HashMap`.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será el ranking de velocidad para búsqueda? ¿Y para
inserción? ¿El Vec ordenado será competitivo en alguna operación?

</details>

### Ejercicio 10 — Limitaciones de la tabla hash

Dado un `HashMap<i32, String>` con 1000 entradas, implementa:
1. `find_min()` — encontrar la clave mínima.
2. `range_query(lo, hi)` — todas las claves en $[lo, hi]$.
3. `ordered_keys()` — todas las claves ordenadas.

Mide el costo de cada operación y compara con `BTreeMap`.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál es la complejidad de cada operación con HashMap vs BTreeMap?
¿En qué factor es más lento el HashMap para `ordered_keys`?

</details>
