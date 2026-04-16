# Hashing perfecto y universal

## Objetivo

Estudiar dos conceptos teóricos fundamentales que sustentan el
diseño de hash tables eficientes y seguras:

- **Hashing perfecto**: funciones hash sin colisiones para un
  conjunto **fijo y conocido** de claves. Búsqueda garantizada
  en $O(1)$ peor caso.
- **Hashing perfecto mínimo (MPHF)**: hashing perfecto que usa
  exactamente $n$ slots para $n$ claves.
- **Esquema FKS** (Fredman-Komlós-Szemerédi): construcción en
  dos niveles con espacio $O(n)$.
- **Hashing universal**: familias de funciones hash con
  garantías probabilísticas contra adversarios.
- **Hash aleatorizado contra DoS**: por qué SipHash y
  RandomState usan semillas aleatorias.
- **Cuckoo hashing**: esquema de dos tablas con $O(1)$ peor caso
  para búsqueda.
- Referencia: S01/T04 introdujo el concepto de hash perfecto
  brevemente; S01/T02 cubrió SipHash y DoS. Aquí formalizamos
  ambos conceptos.

---

## Hashing perfecto

### Definición

Una función hash $h: S \to \{0, \ldots, m-1\}$ es **perfecta**
para un conjunto $S$ si es inyectiva sobre $S$: para todo
$x, y \in S$ con $x \neq y$, $h(x) \neq h(y)$.

```
Conjunto S = {"lunes", "martes", "miércoles", "jueves",
              "viernes", "sábado", "domingo"}

Hash perfecto:
  h("lunes")     = 0
  h("martes")    = 1
  h("miércoles") = 2
  h("jueves")    = 3
  h("viernes")   = 4
  h("sábado")    = 5
  h("domingo")   = 6

  Tabla: [lunes, martes, miércoles, jueves, viernes, sábado, domingo]
  Búsqueda: O(1) peor caso — sin cadenas, sin sondeo.
```

### Requisito clave

El conjunto $S$ debe ser **conocido de antemano** y no cambiar.
Si se añade un nuevo elemento, la función hash perfecta puede
dejar de ser inyectiva. Esto limita el hashing perfecto a
**conjuntos estáticos**:

- Palabras reservadas de un lenguaje de programación.
- Nombres de meses, días, países, códigos de aeropuertos.
- Diccionarios de solo lectura (búsqueda sin inserción).
- Tablas de comandos de un CLI.

### Perfecto vs perfecto mínimo

| Tipo | Tabla | Espacio | Ejemplo |
|------|-------|---------|---------|
| Perfecto | $m \geq n$ slots (puede tener huecos) | $O(n)$ a $O(n^2)$ | $m = 20$ para $n = 7$ |
| Perfecto mínimo (MPHF) | Exactamente $m = n$ slots | $O(n)$ | $m = 7$ para $n = 7$ |

MPHF es más difícil de construir pero usa menos memoria. En la
práctica, se acepta que la tabla tenga algunos slots vacíos
(hashing perfecto no mínimo) a cambio de construcción más simple.

---

## Construcción: el esquema FKS

### La idea de dos niveles

Fredman, Komlós y Szemerédi (1984) demostraron que se puede
construir un hash perfecto con espacio $O(n)$ usando dos niveles
de hashing:

1. **Nivel 1**: una función hash $h_1$ distribuye las $n$ claves
   en $m_1$ buckets. Habrá colisiones.
2. **Nivel 2**: para cada bucket $i$ con $n_i$ claves, se usa
   una función hash **diferente** $h_{2,i}$ con una tabla de
   tamaño $m_{2,i} = n_i^2$. Con $m = n^2$, una función
   aleatoria de una familia universal es perfecta con
   probabilidad $\geq 1/2$ (birthday bound).

```
Nivel 1: h₁ distribuye en m₁ buckets
  bucket 0: {k₃}           → tabla nivel 2 de tamaño 1
  bucket 1: {k₁, k₅}       → tabla nivel 2 de tamaño 4
  bucket 2: {}              → vacío
  bucket 3: {k₂, k₄, k₆}  → tabla nivel 2 de tamaño 9

Nivel 2: cada bucket tiene su propia función hash perfecta
  bucket 1, m₂ = 4: h₂₁(k₁) = 0, h₂₁(k₅) = 3  (sin colisión)
  bucket 3, m₂ = 9: h₂₃(k₂) = 1, h₂₃(k₄) = 5, h₂₃(k₆) = 7
```

### Por qué funciona

Si un bucket tiene $n_i$ claves y la tabla de nivel 2 tiene
$m_{2,i} = n_i^2$ slots, la probabilidad de colisión (por birthday
bound) al elegir una función hash aleatoria de una familia
universal es:

$$P(\text{colisión}) \leq \frac{\binom{n_i}{2}}{m_{2,i}} = \frac{n_i(n_i - 1)/2}{n_i^2} < \frac{1}{2}$$

Así que con probabilidad $> 1/2$, una función aleatoria es
perfecta para ese bucket. Si no lo es, se elige otra. En promedio,
se necesitan $< 2$ intentos.

### Espacio total

El espacio total de las tablas de nivel 2 es:

$$\sum_{i=0}^{m_1 - 1} n_i^2$$

Para que esto sea $O(n)$, se elige $h_1$ de modo que la suma de
los cuadrados sea acotada. Usando una familia universal de
hash con $m_1 = n$:

$$E\left[\sum n_i^2\right] \leq n + \frac{n(n-1)}{m_1} \leq 2n$$

Si la suma excede $4n$ (baja probabilidad), se elige otra $h_1$.

### Implementación simplificada

```c
#define FAMILY_SIZE 1000003  // large prime for universal hash

typedef struct {
    int a, b, p;  // h(k) = ((a*k + b) % p) % m
    int m;        // table size
} UnivHash;

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    UnivHash hash;
    int size;     // number of keys
} Level2Table;

typedef struct {
    Level2Table *buckets;
    UnivHash h1;
    int num_buckets;
    int n;
} PerfectHashTable;
```

### Búsqueda: O(1) peor caso

```c
int pht_lookup(PerfectHashTable *t, int key) {
    // level 1: find bucket
    int b = universal_hash(&t->h1, key);

    // level 2: find slot within bucket
    Level2Table *l2 = &t->buckets[b];
    if (l2->size == 0) return -1;  // empty bucket

    int slot = universal_hash(&l2->hash, key);
    if (l2->occupied[slot] && l2->keys[slot] == key) {
        return l2->values[slot];
    }
    return -1;  // not found
}
```

Dos cálculos de hash + dos accesos a memoria = $O(1)$ **peor
caso** (no promedio).

---

## Hashing universal

### Motivación

Una función hash fija $h$ tiene un peor caso inevitable: existe
un conjunto de claves donde **todas** colisionan (por pigeonhole,
$|K| / m$ claves comparten al menos un bucket). Un adversario
puede elegir ese conjunto y degradar la tabla a $O(n)$.

La solución: elegir $h$ **aleatoriamente** de una familia de
funciones al momento de crear la tabla. El adversario elige las
claves **antes** de conocer qué función se eligió.

### Definición formal

Una familia $\mathcal{H} = \{h: U \to \{0, \ldots, m-1\}\}$ es
**universal** (o 2-universal) si para todo par de claves distintas
$x \neq y$:

$$P_{h \in \mathcal{H}}[h(x) = h(y)] \leq \frac{1}{m}$$

Es decir, la probabilidad de colisión entre dos claves
cualesquiera es como máximo $1/m$ — la misma que una función
perfectamente aleatoria.

### Familia de Carter-Wegman

La familia universal más conocida para claves enteras:

$$h_{a,b}(k) = ((a \cdot k + b) \bmod p) \bmod m$$

donde:
- $p$ es un primo mayor que todas las claves ($p > \max(U)$).
- $a \in \{1, \ldots, p-1\}$ (no puede ser 0).
- $b \in \{0, \ldots, p-1\}$.
- $a$ y $b$ se eligen al azar al crear la tabla.

### Demostración de universalidad

Para $x \neq y$, la transformación $ax + b \bmod p$ y
$ay + b \bmod p$ produce valores distintos (porque $a \neq 0$ y
$p$ es primo, la multiplicación módulo primo es inyectiva).

Sea $r = (ax + b) \bmod p$ y $s = (ay + b) \bmod p$. Como
$r \neq s$ y ambos son uniformes en $\{0, \ldots, p-1\}$, la
probabilidad de que $r \bmod m = s \bmod m$ es a lo sumo
$\lceil p/m \rceil / p \leq 1/m + 1/p \approx 1/m$ para $p \gg m$.

### Garantía para hash tables

Con una función universal y chaining:

$$E[\text{longitud de cadena}] = 1 + \alpha$$

Esto es independiente de la distribución de claves. Incluso si un
adversario elige las peores claves posibles, el rendimiento
**esperado** (sobre la elección aleatoria de $h$) es $O(1 + \alpha)$.

### Familias k-independientes

| Familia | Garantía | Uso |
|---------|----------|-----|
| Universal (2-independiente) | $P[h(x) = h(y)] \leq 1/m$ | Chaining: $O(1 + \alpha)$ esperado |
| 5-independiente | Cotas más fuertes en varianza | Linear probing: $O(1)$ esperado |
| $O(\log n)$-independiente | Concentración exponencial | Cuckoo hashing |
| Totalmente aleatorio | Óptimo teórico | Impractical (necesita $O(|U|)$ espacio) |

Sonda lineal necesita **5-independencia** para garantizar $O(1)$
esperado (resultado de Pagh, Pagh y Ružić, 2009). Con solo
2-independencia, el peor caso de sonda lineal puede ser
$\Theta(\sqrt{n})$.

---

## Hash aleatorizado contra DoS

### El ataque HashDoS

En 2011, Koza y Rizzo demostraron que enviando miles de claves
diseñadas para colisionar en la función hash de un servidor web,
un atacante podía convertir las tablas hash internas (usadas para
parámetros HTTP, headers, etc.) de $O(1)$ a $O(n)$, causando
**denegación de servicio** con poco tráfico.

```
Servidor web con HashMap<String, String> para HTTP params:

Normal: 1000 params, O(1) cada uno → O(1000) total
Ataque: 1000 params que colisionan → O(1000) por búsqueda
        → O(1000 × 1000) = O(10⁶) total

Un solo request malicioso bloquea el servidor por segundos.
```

### La defensa: semilla aleatoria

La solución es usar una función hash con **semilla aleatoria por
instancia** de la tabla:

```rust
// Cada HashMap tiene su propia semilla aleatoria
let map1 = HashMap::new();  // seed = random_1
let map2 = HashMap::new();  // seed = random_2

// El atacante no puede predecir las colisiones porque
// no conoce las semillas
```

En Rust, `HashMap` usa `RandomState` que genera dos claves
aleatorias de 64 bits al crear cada instancia. Estas se pasan a
SipHash como semilla.

### SipHash: diseñado para defensa

SipHash (Aumasson y Bernstein, 2012) fue diseñado específicamente
como hash keyed (con clave/semilla) resistente a ataques:

| Propiedad | SipHash | djb2 / FNV-1a |
|-----------|---------|---------------|
| Keyed (con semilla) | Sí (128 bits de clave) | No |
| Resistente a ataques | Sí (PRF-like) | No |
| Velocidad | Media (~1 GB/s) | Rápida (~3 GB/s) |
| Uso en hash tables | Sí (default en Rust, Python) | Solo si no hay adversarios |

### ¿Cuándo no necesitas hash aleatorizado?

- Claves que **nunca** vienen de input externo (IDs internos,
  enums, contadores).
- Código que no está expuesto a la red.
- Benchmarks y herramientas internas.

En estos casos, un hasher sin semilla (FxHash, ahash) es más
rápido y seguro de usar.

---

## Cuckoo hashing

### La idea

Cuckoo hashing (Pagh y Rodler, 2001) usa **dos tablas** (o dos
funciones hash sobre una tabla) para lograr búsqueda $O(1)$ peor
caso sin hashing perfecto estático:

- Cada clave puede estar en exactamente una de dos posiciones:
  $h_1(k)$ o $h_2(k)$.
- Búsqueda: verificar ambas posiciones → $O(1)$ peor caso.
- Inserción: si ambas posiciones están ocupadas, **desplazar**
  (como el cuco que empuja huevos del nido) y reubicar el
  elemento desplazado.

```
Tabla 1 (T₁):     Tabla 2 (T₂):
  [_, A, _, C]       [_, _, B, _]

  h₁(A)=1, h₂(A)=2 → A en T₁[1] ✓
  h₁(B)=1, h₂(B)=2 → B en T₂[2] ✓ (T₁[1] ocupado por A)
  h₁(C)=3, h₂(C)=0 → C en T₁[3] ✓

Buscar B: T₁[h₁(B)] = T₁[1] = A ≠ B, T₂[h₂(B)] = T₂[2] = B ✓
  Siempre 2 lookups máximo.
```

### Inserción con desplazamiento

```c
bool cuckoo_insert(CuckooTable *t, int key, int value) {
    int cur_key = key;
    int cur_val = value;

    for (int i = 0; i < MAX_KICKS; i++) {
        // try table 1
        int idx1 = hash1(cur_key) % t->capacity;
        if (!t->occupied1[idx1]) {
            t->keys1[idx1] = cur_key;
            t->vals1[idx1] = cur_val;
            t->occupied1[idx1] = true;
            t->count++;
            return true;
        }

        // evict from table 1
        int tmp_k = t->keys1[idx1];
        int tmp_v = t->vals1[idx1];
        t->keys1[idx1] = cur_key;
        t->vals1[idx1] = cur_val;
        cur_key = tmp_k;
        cur_val = tmp_v;

        // try table 2 with evicted key
        int idx2 = hash2(cur_key) % t->capacity;
        if (!t->occupied2[idx2]) {
            t->keys2[idx2] = cur_key;
            t->vals2[idx2] = cur_val;
            t->occupied2[idx2] = true;
            t->count++;
            return true;
        }

        // evict from table 2
        tmp_k = t->keys2[idx2];
        tmp_v = t->vals2[idx2];
        t->keys2[idx2] = cur_key;
        t->vals2[idx2] = cur_val;
        cur_key = tmp_k;
        cur_val = tmp_v;
    }

    // too many kicks: rehash with new functions
    return false;  // trigger rehash
}
```

Si la inserción entra en un ciclo (más de `MAX_KICKS`
desplazamientos), se eligen nuevas funciones hash y se reinserta
todo.

### Análisis

| Operación | Complejidad |
|-----------|-------------|
| Búsqueda | $O(1)$ **peor caso** |
| Eliminación | $O(1)$ **peor caso** (sin tombstones) |
| Inserción | $O(1)$ **amortizado** (esperado) |
| Rehash (por ciclo) | $O(n)$ — pero raro si $\alpha < 0.5$ |

El factor de carga máximo es $\approx 0.5$ para dos tablas (cada
tabla se llena a lo sumo al 50%). Con $d$ funciones hash ($d > 2$)
o buckets de tamaño $> 1$, se puede llegar a $\alpha > 0.9$.

### Cuckoo hashing vs open addressing

| Criterio | Cuckoo | Sonda lineal | Swiss Table |
|----------|--------|-------------|-------------|
| Búsqueda peor caso | $O(1)$ | $O(n)$ | $O(n)$ teórico |
| Búsqueda promedio | 2 lookups | $1/(1-\alpha)$ | ~1 grupo SIMD |
| $\alpha_{\max}$ | ~0.5 | ~0.7-0.9 | 0.875 |
| Cache locality | Mala (2 posiciones aleatorias) | Excelente | Buena |
| Uso real | Filtros, hardware, GPUs | General | Rust, Abseil |

Cuckoo hashing se usa más en **hardware** (switches de red,
FPGAs) y en filtros probabilísticos (cuckoo filter) que en
software general.

---

## Herramientas: gperf

**gperf** (GNU perfect hash function generator) es una herramienta
de línea de comandos que genera código C para una función hash
perfecta dado un conjunto de strings:

```bash
# input file: keywords.gperf
%{
#include <string.h>
%}
%%
if
else
while
for
return
int
void
char
%%

# generate
gperf keywords.gperf > keywords.c
```

gperf genera una función `in_word_set(str, len)` que devuelve un
puntero al keyword si existe, o NULL. La búsqueda es $O(1)$ —
una operación de hash y una comparación de string.

Uso típico: lexers de compiladores (GCC usa gperf para keywords
de C/C++), parsers de protocolos (HTTP headers), lookup de
comandos.

---

## Programa completo en C

```c
// perfect_universal.c
// Perfect hashing (FKS-style), universal hashing, cuckoo hashing
// Compile: gcc -O2 -o perfect_universal perfect_universal.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// ============================================================
// Universal hash family: h(k) = ((a*k + b) % p) % m
// ============================================================

#define PRIME 1000003  // large prime

typedef struct {
    long long a, b;
    int m;
} UnivHash;

UnivHash uh_random(int m) {
    UnivHash h;
    h.a = 1 + rand() % (PRIME - 1);  // a in [1, p-1]
    h.b = rand() % PRIME;             // b in [0, p-1]
    h.m = m;
    return h;
}

int uh_eval(UnivHash *h, int key) {
    long long r = ((h->a * (long long)key + h->b) % PRIME + PRIME) % PRIME;
    return (int)(r % h->m);
}

// ============================================================
// Perfect hash table (FKS two-level)
// ============================================================

typedef struct {
    int *keys;
    int *values;
    bool *occupied;
    UnivHash hash;
    int table_size;
    int count;
} Level2;

typedef struct {
    Level2 *buckets;
    UnivHash h1;
    int num_buckets;
    int total_keys;
} PerfectHT;

// build a perfect hash table for a fixed set of (key, value) pairs
PerfectHT *pht_build(int *keys, int *values, int n) {
    PerfectHT *t = malloc(sizeof(PerfectHT));
    t->num_buckets = n;
    t->total_keys = n;
    t->buckets = calloc(n, sizeof(Level2));

    // level 1: find h1 such that sum of ni^2 <= 4n
    int *bucket_sizes = calloc(n, sizeof(int));
    int **bucket_keys = malloc(n * sizeof(int *));
    int **bucket_vals = malloc(n * sizeof(int *));

    int attempts = 0;
    while (1) {
        attempts++;
        t->h1 = uh_random(n);
        memset(bucket_sizes, 0, n * sizeof(int));

        // count bucket sizes
        for (int i = 0; i < n; i++) {
            int b = uh_eval(&t->h1, keys[i]);
            bucket_sizes[b]++;
        }

        // check sum of squares
        long long sum_sq = 0;
        for (int i = 0; i < n; i++) {
            sum_sq += (long long)bucket_sizes[i] * bucket_sizes[i];
        }

        if (sum_sq <= 4 * n) break;  // good h1
        if (attempts > 100) break;   // fallback
    }

    // distribute keys into buckets
    for (int i = 0; i < n; i++) {
        bucket_keys[i] = malloc(bucket_sizes[i] * sizeof(int));
        bucket_vals[i] = malloc(bucket_sizes[i] * sizeof(int));
        bucket_sizes[i] = 0;  // reset as index
    }

    for (int i = 0; i < n; i++) {
        int b = uh_eval(&t->h1, keys[i]);
        bucket_keys[b][bucket_sizes[b]] = keys[i];
        bucket_vals[b][bucket_sizes[b]] = values[i];
        bucket_sizes[b]++;
    }

    // level 2: for each bucket, find perfect hash
    for (int b = 0; b < n; b++) {
        int ni = bucket_sizes[b];
        t->buckets[b].count = ni;

        if (ni == 0) {
            t->buckets[b].table_size = 0;
            t->buckets[b].keys = NULL;
            t->buckets[b].values = NULL;
            t->buckets[b].occupied = NULL;
            continue;
        }

        int m2 = ni * ni;
        if (m2 == 0) m2 = 1;
        t->buckets[b].table_size = m2;
        t->buckets[b].keys = calloc(m2, sizeof(int));
        t->buckets[b].values = calloc(m2, sizeof(int));
        t->buckets[b].occupied = calloc(m2, sizeof(bool));

        // try random h2 until no collisions
        int h2_attempts = 0;
        while (1) {
            h2_attempts++;
            t->buckets[b].hash = uh_random(m2);
            memset(t->buckets[b].occupied, 0, m2 * sizeof(bool));

            bool collision = false;
            for (int i = 0; i < ni; i++) {
                int slot = uh_eval(&t->buckets[b].hash, bucket_keys[b][i]);
                if (t->buckets[b].occupied[slot]) {
                    collision = true;
                    break;
                }
                t->buckets[b].occupied[slot] = true;
                t->buckets[b].keys[slot] = bucket_keys[b][i];
                t->buckets[b].values[slot] = bucket_vals[b][i];
            }

            if (!collision) break;
            if (h2_attempts > 100) break;
        }
    }

    // cleanup temp
    for (int i = 0; i < n; i++) {
        free(bucket_keys[i]);
        free(bucket_vals[i]);
    }
    free(bucket_keys);
    free(bucket_vals);
    free(bucket_sizes);

    return t;
}

int pht_lookup(PerfectHT *t, int key) {
    int b = uh_eval(&t->h1, key);
    Level2 *l2 = &t->buckets[b];
    if (l2->count == 0) return -1;

    int slot = uh_eval(&l2->hash, key);
    if (slot < l2->table_size && l2->occupied[slot] && l2->keys[slot] == key) {
        return l2->values[slot];
    }
    return -1;
}

void pht_free(PerfectHT *t) {
    for (int i = 0; i < t->num_buckets; i++) {
        free(t->buckets[i].keys);
        free(t->buckets[i].values);
        free(t->buckets[i].occupied);
    }
    free(t->buckets);
    free(t);
}

// ============================================================
// Cuckoo hash table
// ============================================================

#define MAX_KICKS 50

typedef struct {
    int *keys1, *keys2;
    int *vals1, *vals2;
    bool *occ1, *occ2;
    int capacity;
    int count;
    UnivHash h1, h2;
} CuckooHT;

CuckooHT *cht_create(int capacity) {
    CuckooHT *t = malloc(sizeof(CuckooHT));
    t->capacity = capacity;
    t->count = 0;
    t->keys1 = calloc(capacity, sizeof(int));
    t->keys2 = calloc(capacity, sizeof(int));
    t->vals1 = calloc(capacity, sizeof(int));
    t->vals2 = calloc(capacity, sizeof(int));
    t->occ1 = calloc(capacity, sizeof(bool));
    t->occ2 = calloc(capacity, sizeof(bool));
    t->h1 = uh_random(capacity);
    t->h2 = uh_random(capacity);
    return t;
}

void cht_free(CuckooHT *t) {
    free(t->keys1); free(t->keys2);
    free(t->vals1); free(t->vals2);
    free(t->occ1); free(t->occ2);
    free(t);
}

int cht_lookup(CuckooHT *t, int key) {
    int i1 = uh_eval(&t->h1, key);
    if (t->occ1[i1] && t->keys1[i1] == key) return t->vals1[i1];

    int i2 = uh_eval(&t->h2, key);
    if (t->occ2[i2] && t->keys2[i2] == key) return t->vals2[i2];

    return -1;
}

bool cht_insert(CuckooHT *t, int key, int value) {
    // check if exists
    int i1 = uh_eval(&t->h1, key);
    if (t->occ1[i1] && t->keys1[i1] == key) {
        t->vals1[i1] = value; return false;
    }
    int i2 = uh_eval(&t->h2, key);
    if (t->occ2[i2] && t->keys2[i2] == key) {
        t->vals2[i2] = value; return false;
    }

    int ck = key, cv = value;
    for (int kick = 0; kick < MAX_KICKS; kick++) {
        // try table 1
        i1 = uh_eval(&t->h1, ck);
        if (!t->occ1[i1]) {
            t->keys1[i1] = ck; t->vals1[i1] = cv;
            t->occ1[i1] = true; t->count++;
            return true;
        }
        // swap with occupant of table 1
        int tk = t->keys1[i1], tv = t->vals1[i1];
        t->keys1[i1] = ck; t->vals1[i1] = cv;
        ck = tk; cv = tv;

        // try table 2
        i2 = uh_eval(&t->h2, ck);
        if (!t->occ2[i2]) {
            t->keys2[i2] = ck; t->vals2[i2] = cv;
            t->occ2[i2] = true; t->count++;
            return true;
        }
        // swap with occupant of table 2
        tk = t->keys2[i2]; tv = t->vals2[i2];
        t->keys2[i2] = ck; t->vals2[i2] = cv;
        ck = tk; cv = tv;
    }

    // cycle detected — need rehash (not implemented here)
    printf("    (cuckoo cycle! would rehash in production)\n");
    return false;
}

bool cht_remove(CuckooHT *t, int key) {
    int i1 = uh_eval(&t->h1, key);
    if (t->occ1[i1] && t->keys1[i1] == key) {
        t->occ1[i1] = false; t->count--;
        return true;
    }
    int i2 = uh_eval(&t->h2, key);
    if (t->occ2[i2] && t->keys2[i2] == key) {
        t->occ2[i2] = false; t->count--;
        return true;
    }
    return false;
}

// ============================================================
// Demos
// ============================================================

void demo1_universal_hash(void) {
    printf("=== Demo 1: universal hash family ===\n");
    srand(42);

    int m = 10;
    UnivHash h1 = uh_random(m);
    UnivHash h2 = uh_random(m);

    printf("  h1: a=%lld, b=%lld, m=%d\n", h1.a, h1.b, m);
    printf("  h2: a=%lld, b=%lld, m=%d\n", h2.a, h2.b, m);

    printf("\n  key   h1(key)  h2(key)\n");
    for (int k = 0; k < 15; k++) {
        printf("  %3d     %d       %d\n", k, uh_eval(&h1, k), uh_eval(&h2, k));
    }

    // test collision probability
    int collisions = 0;
    int trials = 10000;
    for (int t = 0; t < trials; t++) {
        UnivHash h = uh_random(m);
        if (uh_eval(&h, 5) == uh_eval(&h, 17)) {
            collisions++;
        }
    }
    printf("\n  P[h(5) = h(17)] over %d random h: %.4f (expected ~%.4f)\n\n",
           trials, (double)collisions / trials, 1.0 / m);
}

void demo2_perfect_hash(void) {
    printf("=== Demo 2: perfect hash table (FKS) ===\n");
    srand(time(NULL));

    int keys[] = {15, 42, 7, 88, 23, 56, 91, 34, 67, 12};
    int vals[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int n = 10;

    PerfectHT *t = pht_build(keys, vals, n);

    printf("  Built perfect hash for %d keys\n", n);

    // verify all keys
    int found = 0;
    for (int i = 0; i < n; i++) {
        int v = pht_lookup(t, keys[i]);
        if (v == vals[i]) found++;
    }
    printf("  Verified: %d/%d keys found correctly\n", found, n);

    // test non-existent keys
    int false_pos = 0;
    for (int k = 100; k < 200; k++) {
        if (pht_lookup(t, k) != -1) false_pos++;
    }
    printf("  False positives (100 non-keys): %d\n", false_pos);

    // space usage
    int total_slots = 0;
    for (int i = 0; i < t->num_buckets; i++) {
        total_slots += t->buckets[i].table_size;
    }
    printf("  Level-2 total slots: %d (%.1fx n)\n\n",
           total_slots, (double)total_slots / n);

    pht_free(t);
}

void demo3_cuckoo(void) {
    printf("=== Demo 3: cuckoo hashing ===\n");
    srand(42);

    CuckooHT *t = cht_create(23);  // prime for better distribution

    printf("  Inserting 10 elements:\n");
    int keys[] = {5, 28, 19, 15, 20, 33, 12, 17, 10, 44};
    for (int i = 0; i < 10; i++) {
        bool ok = cht_insert(t, keys[i], keys[i] * 10);
        printf("    insert(%d): %s\n", keys[i], ok ? "ok" : "FAILED");
    }

    printf("\n  Lookups (2 accesses max each):\n");
    for (int i = 0; i < 10; i++) {
        int v = cht_lookup(t, keys[i]);
        printf("    lookup(%d) = %d\n", keys[i], v);
    }

    printf("    lookup(99) = %d (not found)\n", cht_lookup(t, 99));

    // delete
    cht_remove(t, 19);
    printf("\n  After remove(19): lookup(19) = %d\n\n",
           cht_lookup(t, 19));

    cht_free(t);
}

void demo4_collision_probability(void) {
    printf("=== Demo 4: universal hash collision probability ===\n");
    srand(42);

    int ms[] = {10, 100, 1000};
    int trials = 100000;

    for (int mi = 0; mi < 3; mi++) {
        int m = ms[mi];
        int collisions = 0;
        for (int t = 0; t < trials; t++) {
            UnivHash h = uh_random(m);
            // two random distinct keys
            int x = rand() % 10000;
            int y = x + 1 + rand() % 9999;
            if (uh_eval(&h, x) == uh_eval(&h, y)) {
                collisions++;
            }
        }
        printf("  m=%4d: P[collision] = %.5f (expected <= %.5f)\n",
               m, (double)collisions / trials, 1.0 / m);
    }
    printf("\n");
}

void demo5_cuckoo_load(void) {
    printf("=== Demo 5: cuckoo hashing load factor ===\n");
    srand(42);

    int cap = 101;  // prime
    CuckooHT *t = cht_create(cap);

    int inserted = 0;
    for (int i = 0; i < cap; i++) {
        if (cht_insert(t, i * 7 + 3, i)) {
            inserted++;
        } else {
            break;
        }
    }

    double alpha = (double)inserted / (2 * cap);  // two tables
    printf("  capacity per table: %d\n", cap);
    printf("  inserted: %d\n", inserted);
    printf("  alpha (total): %.3f\n", alpha);
    printf("  (cuckoo typically fails around alpha ~0.5)\n\n");

    cht_free(t);
}

void demo6_perfect_vs_regular(void) {
    printf("=== Demo 6: perfect hash vs regular (lookup cost) ===\n");
    srand(42);

    int n = 100;
    int *keys = malloc(n * sizeof(int));
    int *vals = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
        keys[i] = rand() % 100000;
        vals[i] = i;
    }

    PerfectHT *pht = pht_build(keys, vals, n);

    // perfect hash: always exactly 2 hash computations
    int pht_found = 0;
    for (int i = 0; i < n; i++) {
        if (pht_lookup(pht, keys[i]) == vals[i]) pht_found++;
    }
    printf("  Perfect hash: %d/%d found (always 2 hashes per lookup)\n",
           pht_found, n);

    int total_l2_slots = 0;
    for (int i = 0; i < pht->num_buckets; i++) {
        total_l2_slots += pht->buckets[i].table_size;
    }
    printf("  Space: %d level-2 slots for %d keys (%.1fx)\n",
           total_l2_slots, n, (double)total_l2_slots / n);
    printf("  Trade-off: more space, guaranteed O(1) worst case\n\n");

    free(keys);
    free(vals);
    pht_free(pht);
}

int main(void) {
    demo1_universal_hash();
    demo2_perfect_hash();
    demo3_cuckoo();
    demo4_collision_probability();
    demo5_cuckoo_load();
    demo6_perfect_vs_regular();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// perfect_universal.rs
// Universal hashing, perfect hashing concepts, cuckoo hashing
// Run: rustc -O perfect_universal.rs && ./perfect_universal

use std::collections::HashMap;

// ============================================================
// Universal hash family
// ============================================================

const PRIME: u64 = 1_000_003;

struct UnivHash {
    a: u64,
    b: u64,
    m: u64,
}

impl UnivHash {
    fn random(m: u64, rng: &mut SimpleRng) -> Self {
        Self {
            a: 1 + rng.next() % (PRIME - 1),
            b: rng.next() % PRIME,
            m,
        }
    }

    fn eval(&self, key: i64) -> usize {
        let k = ((key % PRIME as i64) + PRIME as i64) as u64;
        let r = (self.a.wrapping_mul(k).wrapping_add(self.b)) % PRIME;
        (r % self.m) as usize
    }
}

// simple LCG for reproducibility
struct SimpleRng(u64);

impl SimpleRng {
    fn new(seed: u64) -> Self { Self(seed) }
    fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1);
        self.0 >> 33
    }
}

// ============================================================
// Cuckoo hash table
// ============================================================

struct CuckooTable {
    keys1: Vec<i64>,
    vals1: Vec<i64>,
    occ1: Vec<bool>,
    keys2: Vec<i64>,
    vals2: Vec<i64>,
    occ2: Vec<bool>,
    h1: UnivHash,
    h2: UnivHash,
    capacity: usize,
    count: usize,
}

impl CuckooTable {
    fn new(capacity: usize, rng: &mut SimpleRng) -> Self {
        Self {
            keys1: vec![0; capacity],
            vals1: vec![0; capacity],
            occ1: vec![false; capacity],
            keys2: vec![0; capacity],
            vals2: vec![0; capacity],
            occ2: vec![false; capacity],
            h1: UnivHash::random(capacity as u64, rng),
            h2: UnivHash::random(capacity as u64, rng),
            capacity,
            count: 0,
        }
    }

    fn lookup(&self, key: i64) -> Option<i64> {
        let i1 = self.h1.eval(key);
        if self.occ1[i1] && self.keys1[i1] == key {
            return Some(self.vals1[i1]);
        }
        let i2 = self.h2.eval(key);
        if self.occ2[i2] && self.keys2[i2] == key {
            return Some(self.vals2[i2]);
        }
        None
    }

    fn insert(&mut self, key: i64, value: i64) -> bool {
        // check existing
        let i1 = self.h1.eval(key);
        if self.occ1[i1] && self.keys1[i1] == key {
            self.vals1[i1] = value;
            return false;
        }
        let i2 = self.h2.eval(key);
        if self.occ2[i2] && self.keys2[i2] == key {
            self.vals2[i2] = value;
            return false;
        }

        let mut ck = key;
        let mut cv = value;
        let max_kicks = 50;

        for _ in 0..max_kicks {
            let i1 = self.h1.eval(ck);
            if !self.occ1[i1] {
                self.keys1[i1] = ck;
                self.vals1[i1] = cv;
                self.occ1[i1] = true;
                self.count += 1;
                return true;
            }
            std::mem::swap(&mut ck, &mut self.keys1[i1]);
            std::mem::swap(&mut cv, &mut self.vals1[i1]);

            let i2 = self.h2.eval(ck);
            if !self.occ2[i2] {
                self.keys2[i2] = ck;
                self.vals2[i2] = cv;
                self.occ2[i2] = true;
                self.count += 1;
                return true;
            }
            std::mem::swap(&mut ck, &mut self.keys2[i2]);
            std::mem::swap(&mut cv, &mut self.vals2[i2]);
        }
        false // cycle — need rehash
    }

    fn remove(&mut self, key: i64) -> bool {
        let i1 = self.h1.eval(key);
        if self.occ1[i1] && self.keys1[i1] == key {
            self.occ1[i1] = false;
            self.count -= 1;
            return true;
        }
        let i2 = self.h2.eval(key);
        if self.occ2[i2] && self.keys2[i2] == key {
            self.occ2[i2] = false;
            self.count -= 1;
            return true;
        }
        false
    }
}

// ============================================================
// Demos
// ============================================================

fn demo1_universal_hash() {
    println!("=== Demo 1: universal hash family ===");
    let mut rng = SimpleRng::new(42);

    let m = 10u64;
    let h1 = UnivHash::random(m, &mut rng);
    let h2 = UnivHash::random(m, &mut rng);

    println!("  h1: a={}, b={}, m={}", h1.a, h1.b, m);
    println!("  h2: a={}, b={}, m={}\n", h2.a, h2.b, m);

    println!("  {:>5} {:>8} {:>8}", "key", "h1(key)", "h2(key)");
    for k in 0..15i64 {
        println!("  {:>5} {:>8} {:>8}", k, h1.eval(k), h2.eval(k));
    }

    // test collision probability
    let trials = 100_000;
    let mut collisions = 0;
    for _ in 0..trials {
        let h = UnivHash::random(m, &mut rng);
        if h.eval(5) == h.eval(17) {
            collisions += 1;
        }
    }
    println!("\n  P[h(5)=h(17)] = {:.4} (expected ~{:.4})\n",
             collisions as f64 / trials as f64, 1.0 / m as f64);
}

fn demo2_collision_probability() {
    println!("=== Demo 2: universality verification ===");
    let mut rng = SimpleRng::new(123);
    let trials = 100_000;

    for &m in &[10u64, 100, 1000] {
        let mut collisions = 0;
        for _ in 0..trials {
            let h = UnivHash::random(m, &mut rng);
            let x = (rng.next() % 10000) as i64;
            let y = x + 1 + (rng.next() % 9999) as i64;
            if h.eval(x) == h.eval(y) {
                collisions += 1;
            }
        }
        println!("  m={:>5}: P[collision] = {:.5} (expected <= {:.5})",
                 m, collisions as f64 / trials as f64, 1.0 / m as f64);
    }
    println!();
}

fn demo3_cuckoo_basic() {
    println!("=== Demo 3: cuckoo hashing ===");
    let mut rng = SimpleRng::new(42);
    let mut t = CuckooTable::new(23, &mut rng);

    let keys = [5, 28, 19, 15, 20, 33, 12, 17, 10, 44];
    println!("  Inserting:");
    for &k in &keys {
        let ok = t.insert(k, k * 10);
        println!("    insert({}) = {}", k, if ok { "new" } else { "exists" });
    }

    println!("\n  Lookups (max 2 accesses):");
    for &k in &keys {
        println!("    lookup({}) = {:?}", k, t.lookup(k));
    }
    println!("    lookup(99) = {:?}", t.lookup(99));

    t.remove(19);
    println!("\n  After remove(19): lookup(19) = {:?}\n", t.lookup(19));
}

fn demo4_cuckoo_load() {
    println!("=== Demo 4: cuckoo load factor limit ===");
    let mut rng = SimpleRng::new(42);
    let cap = 101;
    let mut t = CuckooTable::new(cap, &mut rng);

    let mut inserted = 0;
    for i in 0..cap {
        if t.insert((i as i64) * 7 + 3, i as i64) {
            inserted += 1;
        } else {
            println!("  Failed at element #{}", i);
            break;
        }
    }

    let alpha = inserted as f64 / (2 * cap) as f64;
    println!("  capacity/table: {}", cap);
    println!("  inserted: {}", inserted);
    println!("  alpha (over 2 tables): {:.3}", alpha);
    println!("  (cuckoo typically saturates around alpha ~0.5)\n");
}

fn demo5_rust_randomstate() {
    println!("=== Demo 5: Rust's RandomState (anti-DoS) ===");

    // each HashMap has its own random seed
    let map1: HashMap<i32, i32> = HashMap::new();
    let map2: HashMap<i32, i32> = HashMap::new();

    // demonstrate different internal hashing
    use std::hash::{BuildHasher, Hasher};
    let s1 = map1.hasher();
    let s2 = map2.hasher();

    let mut h1 = s1.build_hasher();
    let mut h2 = s2.build_hasher();
    h1.write_i32(42);
    h2.write_i32(42);

    println!("  map1.hash(42) = {}", h1.finish());
    println!("  map2.hash(42) = {}", h2.finish());
    println!("  (different seeds → different hashes → DoS-resistant)");
    println!("  Attacker can't pre-compute colliding keys\n");
}

fn demo6_perfect_hash_concept() {
    println!("=== Demo 6: perfect hashing concept ===");

    // simulate: build a minimal perfect hash for known keywords
    let keywords = vec!["if", "else", "while", "for", "return",
                        "int", "void", "char", "struct", "enum"];

    // brute-force: find (a, b) such that h(k) = (a*simple_hash(k)+b) % n
    // maps all keywords to distinct slots
    let n = keywords.len();

    fn simple_hash(s: &str) -> u64 {
        let mut h = 0u64;
        for b in s.bytes() {
            h = h.wrapping_mul(31).wrapping_add(b as u64);
        }
        h
    }

    let mut rng = SimpleRng::new(42);
    let mut best_a = 1u64;
    let mut best_b = 0u64;
    let mut found = false;

    for _ in 0..10000 {
        let a = 1 + rng.next() % 999;
        let b = rng.next() % 999;

        let mut slots = vec![false; n];
        let mut collision = false;
        for &kw in &keywords {
            let slot = ((a.wrapping_mul(simple_hash(kw)).wrapping_add(b)) % n as u64) as usize;
            if slots[slot] {
                collision = true;
                break;
            }
            slots[slot] = true;
        }

        if !collision {
            best_a = a;
            best_b = b;
            found = true;
            break;
        }
    }

    if found {
        println!("  Found perfect hash for {} keywords:", n);
        println!("  h(k) = ({} * hash(k) + {}) % {}\n", best_a, best_b, n);
        for &kw in &keywords {
            let slot = ((best_a.wrapping_mul(simple_hash(kw)).wrapping_add(best_b)) % n as u64) as usize;
            println!("    h(\"{:>6}\") = {}", kw, slot);
        }
        println!("\n  All slots unique — O(1) worst-case lookup!");
    } else {
        println!("  (could not find perfect hash in 10000 attempts)");
    }
    println!();
}

fn demo7_gperf_equivalent() {
    println!("=== Demo 7: static lookup table (gperf-like) ===");

    // in production, gperf generates this kind of code
    // here we show the concept with a hand-crafted table
    let http_methods: HashMap<&str, u8> = HashMap::from([
        ("GET", 0), ("POST", 1), ("PUT", 2), ("DELETE", 3),
        ("PATCH", 4), ("HEAD", 5), ("OPTIONS", 6),
    ]);

    let requests = ["GET", "POST", "DELETE", "UNKNOWN", "PUT"];
    for req in &requests {
        match http_methods.get(req) {
            Some(code) => println!("  {} → method code {}", req, code),
            None => println!("  {} → unknown method", req),
        }
    }
    println!("  (gperf would generate O(1) worst-case C code for this)\n");
}

fn main() {
    demo1_universal_hash();
    demo2_collision_probability();
    demo3_cuckoo_basic();
    demo4_cuckoo_load();
    demo5_rust_randomstate();
    demo6_perfect_hash_concept();
    demo7_gperf_equivalent();
}
```

---

## Ejercicios

### Ejercicio 1 — Familia universal: verificar propiedad

Genera 10000 funciones hash aleatorias de la familia
Carter-Wegman con $m = 100$. Para cada par fijo $(x, y) = (42, 137)$,
cuenta cuántas veces $h(42) = h(137)$.

<details><summary>¿Qué proporción esperas?</summary>

$P[h(x) = h(y)] \leq 1/m = 1/100 = 0.01$. De 10000 funciones,
esperamos ~100 colisiones (1%).

La universalidad garantiza que esta proporción no exceda $1/m$
para **cualquier** par de claves, no solo $(42, 137)$.

</details>

<details><summary>¿Qué pasa si usas a=0?</summary>

Con $a = 0$: $h_{0,b}(k) = b \bmod m$ — una constante
independiente de $k$. Todas las claves tienen el mismo hash.
$P[\text{colisión}] = 1$, violando la propiedad universal.

Por eso la familia Carter-Wegman requiere $a \in \{1, \ldots, p-1\}$
(nunca 0).

</details>

---

### Ejercicio 2 — FKS: calcular espacio

Un hash perfecto FKS se construye para $n = 10$ claves. El nivel 1
produce los siguientes tamaños de bucket:
$n_0 = 3, n_1 = 0, n_2 = 2, n_3 = 1, n_4 = 0, n_5 = 4,
n_6 = 0, n_7 = 0, n_8 = 0, n_9 = 0$.

a) ¿Cuál es $\sum n_i^2$?
b) ¿Es aceptable ($ \leq 4n$)?
c) ¿Cuántos slots de nivel 2 se usan en total?

<details><summary>a) Suma de cuadrados</summary>

$3^2 + 0 + 2^2 + 1^2 + 0 + 4^2 + 0 + 0 + 0 + 0 = 9 + 4 + 1 + 16 = 30$.

</details>

<details><summary>b) y c) ¿Es aceptable?</summary>

$4n = 40$. Como $30 \leq 40$, **sí es aceptable**. La función
$h_1$ es válida.

Slots de nivel 2: $9 + 0 + 4 + 1 + 0 + 16 + 0 + 0 + 0 + 0 = 30$.
Para 10 claves, se usan 30 slots → $3\times$ el mínimo. Un hash
perfecto mínimo usaría 10 slots, pero la construcción FKS
necesita $n_i^2$ por bucket para garantizar que el nivel 2 sea
perfecto.

</details>

---

### Ejercicio 3 — Cuckoo: traza de inserción

Tabla cuckoo con capacidad 5 por tabla, $h_1(k) = k \bmod 5$,
$h_2(k) = (k/5) \bmod 5$. Inserta las claves 3, 8, 13, 18, 23.

<details><summary>¿Dónde termina cada clave?</summary>

- Insert 3: $h_1(3)=3$, T1[3] libre → T1[3]=3.
- Insert 8: $h_1(8)=3$, T1[3] ocupado(3) → swap: T1[3]=8,
  reubicar 3. $h_2(3)=0$, T2[0] libre → T2[0]=3.
- Insert 13: $h_1(13)=3$, T1[3] ocupado(8) → swap: T1[3]=13,
  reubicar 8. $h_2(8)=1$, T2[1] libre → T2[1]=8.
- Insert 18: $h_1(18)=3$, T1[3] ocupado(13) → swap: T1[3]=18,
  reubicar 13. $h_2(13)=2$, T2[2] libre → T2[2]=13.
- Insert 23: $h_1(23)=3$, T1[3] ocupado(18) → swap: T1[3]=23,
  reubicar 18. $h_2(18)=3$, T2[3] libre → T2[3]=18.

Resultado: T1 = [_, _, _, 23, _], T2 = [3, 8, 13, 18, _].

</details>

<details><summary>¿Cuántos desplazamientos en total?</summary>

Inserts 1-2: 0, 1 desplazamientos. Inserts 3-5: 1, 1, 1.
Total: 4 desplazamientos para 5 inserciones. Todas las claves
tienen $h_1 = 3$, causando desplazamiento en cadena.

Búsqueda de cualquier clave: máximo 2 accesos (T1 + T2).

</details>

---

### Ejercicio 4 — ¿Por qué no usar hashing perfecto siempre?

Si el hashing perfecto garantiza $O(1)$ peor caso, ¿por qué no se
usa en todas las hash tables?

<details><summary>¿Cuáles son las limitaciones?</summary>

1. **Conjunto fijo**: las claves deben conocerse de antemano. No
   se puede insertar una clave nueva sin reconstruir toda la
   función hash.
2. **Costo de construcción**: construir un hash perfecto FKS es
   $O(n)$ esperado, pero con constantes grandes (múltiples
   intentos de funciones aleatorias).
3. **Espacio extra**: FKS usa $\sim 2n-4n$ slots vs $n/0.75
   \approx 1.33n$ de una tabla normal.
4. **No soporta eliminación**: eliminar una clave puede dejar un
   hueco que confunde la búsqueda (similar al problema de EMPTY
   en open addressing, pero peor).

</details>

<details><summary>¿Cuándo sí usarlo?</summary>

- **Diccionarios inmutables**: keywords de un lenguaje, códigos
  de país, tablas de protocolo.
- **Compiladores**: gperf para keywords de C/C++.
- **Bases de datos**: índices de columnas con dominio fijo.
- **Networking**: tablas de routing con conjuntos estáticos de
  prefijos.

La regla: si el conjunto nunca cambia y la búsqueda es
performance-critical, considerar hashing perfecto.

</details>

---

### Ejercicio 5 — SipHash vs djb2: benchmark

Implementa ambas funciones hash para strings y compara la velocidad
hasheando 100000 strings de longitud variable (5-50 chars).

<details><summary>¿Cuál es más rápido?</summary>

djb2 es típicamente **2-3× más rápida** que SipHash. djb2 hace
una multiplicación + suma por byte. SipHash hace mezcla compleja
(rotaciones, XORs, sumas) diseñada para ser
criptográficamente robusta.

Para strings cortos (< 8 bytes), la diferencia es menor porque
SipHash procesa por bloques de 8 bytes.

</details>

<details><summary>¿Cuándo vale la pena la velocidad de djb2?</summary>

Solo cuando:
- Las claves no vienen de input externo (no hay riesgo de DoS).
- El hashing es el bottleneck medido (no asumido).
- Se acepta la pérdida de seguridad.

En la práctica, la diferencia de velocidad rara vez importa
porque el hashing es una fracción pequeña del tiempo total de
insert/lookup (la comparación de claves y accesos a caché suelen
dominar).

</details>

---

### Ejercicio 6 — Cuckoo filter vs bloom filter

Un **cuckoo filter** es una estructura probabilística inspirada
en cuckoo hashing. Compara con bloom filter:

| Criterio | Bloom filter | Cuckoo filter |
|----------|-------------|---------------|
| Falsos positivos | Sí | Sí |
| Falsos negativos | No | No |
| Eliminación | No (sin counting) | Sí |
| Bits por elemento | $\sim 10$ para 1% FP | $\sim 12$ para 1% FP |

<details><summary>¿Cuándo elegir cuckoo filter?</summary>

Cuando necesitas **eliminación**. Bloom filter estándar no
soporta delete (un bit puede pertenecer a múltiples elementos).
Counting Bloom filter sí, pero usa 4× más memoria.

Cuckoo filter almacena fingerprints (hashes parciales) en una
tabla cuckoo, y eliminar un fingerprint es directo.

Para aplicaciones de solo insert + lookup (sin delete), Bloom
filter es más simple y ligeramente más eficiente en espacio.

</details>

<details><summary>¿Cómo funciona la eliminación en cuckoo filter?</summary>

Cada slot almacena un fingerprint $f(x)$ de la clave $x$.
Para eliminar $x$: calcular $f(x)$, buscar en las dos posiciones
posibles, y borrar el slot que contiene $f(x)$.

Riesgo: si dos claves diferentes tienen el mismo fingerprint y
la misma posición, eliminar una puede "eliminar" la otra
(falso negativo). Esto se mitiga con fingerprints largos
(más bits = menor probabilidad de colisión).

</details>

---

### Ejercicio 7 — k-independencia y sonda lineal

¿Por qué sonda lineal necesita 5-independencia para
$O(1)$ esperado, mientras que chaining solo necesita
2-independencia?

<details><summary>¿Cuál es la diferencia?</summary>

En **chaining**, cada clave va a un bucket independientemente de
las demás (la cadena es una lista enlazada). La longitud esperada
del bucket solo depende de pares de claves que colisionan →
2-independencia basta.

En **sonda lineal**, la posición final de una clave depende de
**todas las claves anteriores** en el cluster. Un cluster de
longitud $L$ afecta a todas las claves con hash en esas $L$
posiciones. El análisis de clusters requiere controlar
correlaciones entre 5 o más claves simultáneamente →
5-independencia.

Con solo 2-independencia, sonda lineal puede degradar a
$O(\sqrt{n})$ esperado (no $O(n)$, pero tampoco $O(1)$).

</details>

<details><summary>¿Cómo lograr 5-independencia?</summary>

Usando un polinomio de grado 4 módulo primo:

$$h(k) = (a_4 k^4 + a_3 k^3 + a_2 k^2 + a_1 k + a_0) \bmod p \bmod m$$

con coeficientes $a_0, \ldots, a_4$ aleatorios. Esto es más
costoso que Carter-Wegman (grado 1) pero garantiza las
propiedades necesarias.

En la práctica, funciones como SipHash, MurmurHash3 y xxHash no
son formalmente 5-independientes, pero se comportan como si lo
fueran. Tabular hashing (Zobrist) es formalmente 3-independiente
y funciona bien para sonda lineal en la práctica.

</details>

---

### Ejercicio 8 — Hash perfecto para HTTP methods

Los métodos HTTP estándar son: GET, HEAD, POST, PUT, DELETE,
CONNECT, OPTIONS, TRACE, PATCH (9 strings). Encuentra
manualmente una función hash perfecta simple de la forma
$h(s) = \text{len}(s) + s[k]$ para algún carácter $k$.

<details><summary>¿Funciona h(s) = len(s)?</summary>

GET(3), HEAD(4), POST(4), PUT(3), DELETE(6), CONNECT(7),
OPTIONS(7), TRACE(5), PATCH(5).

Colisiones: GET/PUT(3), HEAD/POST(4), TRACE/PATCH(5),
CONNECT/OPTIONS(7). No es perfecta.

</details>

<details><summary>¿Hay una combinación que funcione?</summary>

Probemos $h(s) = s[\text{len}-1] \bmod 9$ (último carácter):

GET→T(84%9=3), HEAD→D(68%9=5), POST→T(3, colisión), ...

No funciona. Probemos $h(s) = (s[0] + \text{len}) \bmod 9$:

GET→G(71)+3=74%9=2, HEAD→H(72)+4=76%9=4, POST→P(80)+4=84%9=3,
PUT→P(80)+3=83%9=2 — colisión con GET.

En la práctica, gperf encuentra automáticamente la combinación
correcta. Para 9 strings, prueba combinaciones de posiciones de
caracteres y offsets hasta encontrar una inyectiva. El espacio de
búsqueda es pequeño.

</details>

---

### Ejercicio 9 — RandomState: demostrar protección

Crea dos `HashMap<String, i32>` en Rust y muestra que la misma
clave produce hashes diferentes en cada mapa.

<details><summary>¿Cómo acceder al hash interno?</summary>

```rust
use std::hash::{BuildHasher, Hasher};

let map1: HashMap<String, i32> = HashMap::new();
let map2: HashMap<String, i32> = HashMap::new();

let s1 = map1.hasher();
let s2 = map2.hasher();

let mut h1 = s1.build_hasher();
let mut h2 = s2.build_hasher();
h1.write(b"attack_key");
h2.write(b"attack_key");

println!("map1: {}", h1.finish());  // e.g., 12345678
println!("map2: {}", h2.finish());  // e.g., 87654321
// different! attacker can't predict which keys collide
```

</details>

<details><summary>¿Qué pasaría sin RandomState?</summary>

Sin semilla aleatoria (como djb2 fijo), el atacante puede
precalcular miles de strings que colisionan. Todas las instancias
de HashMap usarían la misma función hash, así que un ataque
contra una sirve contra todas.

Con RandomState, cada HashMap es inmune a ataques precalculados.
El atacante necesitaría conocer la semilla de la instancia
específica, lo cual requiere acceso al espacio de memoria del
proceso.

</details>

---

### Ejercicio 10 — Diseño: elegir la estrategia correcta

Para cada escenario, elige la mejor estrategia de hashing:

a) Tabla de opcodes de un procesador (256 opcodes fijos).
b) Cache web con claves URL de usuarios.
c) Base de datos en memoria con millones de registros.
d) Switch de red que enruta paquetes por dirección MAC.

<details><summary>a) Opcodes</summary>

**Array directo** (ni siquiera hash table). 256 opcodes caben
en un array de 256 posiciones indexado por el byte del opcode.
$O(1)$ peor caso, cero overhead.

Si fueran más opcodes o no consecutivos: **hash perfecto** (gperf).
Conjunto fijo, solo lectura, búsqueda máxima velocidad.

</details>

<details><summary>b) Cache web</summary>

**HashMap con SipHash** (Rust default) o equivalente con semilla
aleatoria. Las URLs vienen de usuarios (potencialmente
adversarios), así que se necesita hash resistente a DoS.
El conjunto es dinámico (inserciones y eliminaciones constantes),
así que hashing perfecto no aplica.

</details>

<details><summary>c) Base de datos en memoria</summary>

**Hash table con Swiss Table o Robin Hood** para búsquedas rápidas
por clave primaria. Si las claves son enteras secuenciales, un
hasher rápido (FxHash) es suficiente. Si se necesitan range
queries, combinar con un B-tree index.

Si el dataset es estático (read-only después de cargar), considerar
**hashing perfecto mínimo** (MPHF) para mínimo espacio y máxima
velocidad.

</details>

<details><summary>d) Switch de red</summary>

**Cuckoo hashing en hardware** (FPGA/ASIC). La búsqueda de
dirección MAC necesita $O(1)$ **peor caso** (no amortizado) porque
cada paquete tiene deadline en nanosegundos. Cuckoo hashing
garantiza máximo 2 accesos a SRAM. Sonda lineal no sirve porque
el peor caso es $O(n)$.

</details>
