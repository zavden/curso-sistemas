# Sonda lineal (linear probing)

## Objetivo

Implementar la estrategia de resolución de colisiones más simple
de **open addressing**: cuando el bucket calculado está ocupado,
probar el **siguiente** en secuencia circular hasta encontrar uno
libre:

- Secuencia de sondeo: $h(k), h(k)+1, h(k)+2, \ldots$ (mod $m$).
- Excelente **localidad de caché** — accesos secuenciales al array.
- Problema central: **clustering primario** — los clusters crecen
  y se fusionan, degradando el rendimiento.
- Análisis de complejidad con las fórmulas de Knuth.
- Eliminación con **tombstones** (preludio a S02/T04).
- Referencia: S02/T01 implementó chaining. Aquí empieza open
  addressing.

---

## La idea

En open addressing, todos los elementos se almacenan **dentro del
array**. No hay estructuras auxiliares (listas, árboles). Si
`table[h(k)]` está ocupado, se prueba `table[h(k)+1]`, luego
`table[h(k)+2]`, etc., dando la vuelta al array circularmente.

```
Insertar clave con h(k) = 3 en tabla de m=8:

  [_, _, _, X, X, _, _, _]
             ^  ^
             3  4  ← ambos ocupados

  Probar 5: libre → insertar en 5

  [_, _, _, X, X, k, _, _]
```

La secuencia de sondeo para sonda lineal es:

$$s_i = (h(k) + i) \bmod m \quad \text{para } i = 0, 1, 2, \ldots$$

---

## Estructura

```c
typedef struct {
    char **keys;     // array de punteros a claves
    int *values;     // array de valores
    char *state;     // EMPTY, OCCUPIED, DELETED (tombstone)
    int m;           // capacidad
    int n;           // elementos almacenados
} LinearTable;

enum { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };
```

Tres estados por slot:
- **EMPTY**: nunca ha sido usado. La búsqueda puede terminar aquí.
- **OCCUPIED**: contiene un par (clave, valor) activo.
- **DELETED** (tombstone): tenía un elemento que fue eliminado.
  La búsqueda debe **continuar** al encontrar un tombstone (el
  elemento buscado puede estar más adelante). La inserción puede
  reutilizar el slot.

---

## Operaciones

### Insertar

```c
int lp_insert(LinearTable *t, const char *key, int value) {
    if (t->n >= t->m * 3 / 4)  // alpha > 0.75
        return 0;  // tabla demasiado llena (deberia resize)

    unsigned int i = hash(key, t->m);
    int first_deleted = -1;

    for (int step = 0; step < t->m; step++) {
        int idx = (i + step) % t->m;

        if (t->state[idx] == EMPTY) {
            // encontramos un slot libre
            int target = (first_deleted >= 0) ? first_deleted : idx;
            t->keys[target] = strdup(key);
            t->values[target] = value;
            t->state[target] = OCCUPIED;
            t->n++;
            return 1;
        }

        if (t->state[idx] == DELETED) {
            if (first_deleted < 0) first_deleted = idx;
            continue;
        }

        // OCCUPIED: check for duplicate
        if (strcmp(t->keys[idx], key) == 0) {
            t->values[idx] = value;  // update
            return 1;
        }
    }
    return 0;  // tabla llena (no deberia llegar aqui)
}
```

Observaciones:
- Se registra el **primer tombstone** encontrado para reutilizarlo
  (optimización que reduce la fragmentación).
- Se verifica duplicados como en chaining.
- El guard `n >= m * 3/4` previene operar con $\alpha > 0.75$.

### Buscar

```c
int lp_search(LinearTable *t, const char *key, int *value) {
    unsigned int i = hash(key, t->m);

    for (int step = 0; step < t->m; step++) {
        int idx = (i + step) % t->m;

        if (t->state[idx] == EMPTY)
            return 0;  // no encontrado — cadena rota

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0) {
            if (value) *value = t->values[idx];
            return 1;
        }
        // si DELETED o clave diferente: seguir sondeando
    }
    return 0;
}
```

Regla crítica: la búsqueda **se detiene en EMPTY** (nunca hubo
nada ahí, la clave no puede estar más adelante) pero **continúa
sobre DELETED** (algo estuvo ahí y la clave pudo haber sido
desplazada más allá).

### Eliminar

```c
int lp_delete(LinearTable *t, const char *key) {
    unsigned int i = hash(key, t->m);

    for (int step = 0; step < t->m; step++) {
        int idx = (i + step) % t->m;

        if (t->state[idx] == EMPTY)
            return 0;

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0) {
            free(t->keys[idx]);
            t->keys[idx] = NULL;
            t->state[idx] = DELETED;  // tombstone
            t->n--;
            return 1;
        }
    }
    return 0;
}
```

No se puede simplemente poner el slot en EMPTY porque rompería
las cadenas de sondeo de claves insertadas después. Ejemplo:

```
Insertar A en slot 3, B en slot 4 (colision con A):
  [_, _, _, A, B, _, _, _]

Si eliminamos A poniendo EMPTY:
  [_, _, _, _, B, _, _, _]

Buscar B: h(B)=3, slot 3 EMPTY → "no encontrado"!
  ¡B esta en slot 4 pero la busqueda se detuvo!

Con tombstone:
  [_, _, _, D, B, _, _, _]  (D = DELETED)

Buscar B: h(B)=3, slot 3 DELETED → continuar, slot 4 → B encontrado ✓
```

---

## Traza completa

Tabla $m = 11$, insertar claves con hashes:
$h = \{3, 0, 3, 5, 3, 0, 8, 5, 10\}$ para claves
$\{a, b, c, d, e, f, g, h, i\}$:

```
Insertar a: h=3 → slot 3 libre → [3]=a
  [_, _, _, a, _, _, _, _, _, _, _]

Insertar b: h=0 → slot 0 libre → [0]=b
  [b, _, _, a, _, _, _, _, _, _, _]

Insertar c: h=3 → slot 3 ocupado(a) → probar 4 → libre → [4]=c
  [b, _, _, a, c, _, _, _, _, _, _]

Insertar d: h=5 → slot 5 libre → [5]=d
  [b, _, _, a, c, d, _, _, _, _, _]

Insertar e: h=3 → slot 3(a), 4(c), 5(d) ocupados → probar 6 → libre → [6]=e
  [b, _, _, a, c, d, e, _, _, _, _]
  Probes: 4 (3→4→5→6)

Insertar f: h=0 → slot 0(b) ocupado → probar 1 → libre → [1]=f
  [b, f, _, a, c, d, e, _, _, _, _]

Insertar g: h=8 → slot 8 libre → [8]=g
  [b, f, _, a, c, d, e, _, g, _, _]

Insertar h: h=5 → slot 5(d), 6(e) ocupados → probar 7 → libre → [7]=h
  [b, f, _, a, c, d, e, h, g, _, _]

Insertar i: h=10 → slot 10 libre → [10]=i
  [b, f, _, a, c, d, e, h, g, _, i]

alpha = 9/11 = 0.818
```

Observación: las claves con $h = 3$ (a, c, e) formaron un cluster
que se extendió a los slots 3-6. La clave $h$ (hash=5) fue
empujada a slot 7 por el cluster.

**Buscar e** ($h = 3$):

```
Probar slot 3: a ≠ e → siguiente
Probar slot 4: c ≠ e → siguiente
Probar slot 5: d ≠ e → siguiente
Probar slot 6: e == e → ENCONTRADO
Sondeos: 4
```

**Buscar z** ($h = 3$, no existe):

```
Probar slot 3: a ≠ z → siguiente
Probar slot 4: c ≠ z → siguiente
Probar slot 5: d ≠ z → siguiente
Probar slot 6: e ≠ z → siguiente
Probar slot 7: h ≠ z → siguiente
Probar slot 8: g ≠ z → siguiente
Probar slot 9: EMPTY → NO ENCONTRADO
Sondeos: 7!
```

La búsqueda fallida recorre todo el cluster más allá del punto
de hash — esto es el efecto del clustering primario.

---

## Clustering primario

### El fenómeno

Los clusters (secuencias contiguas de slots ocupados) tienen una
propiedad perversa: **cuanto más grande es un cluster, más rápido
crece**.

Un cluster de longitud $L$ "captura" inserciones destinadas a
cualquiera de los $L$ slots del cluster más el slot justo después
(porque todas esas inserciones terminan al final del cluster).
La probabilidad de que el cluster crezca es proporcional a
$L + 1$:

$$P(\text{cluster crece}) = \frac{L + 1}{m}$$

```
Cluster de longitud 4:

  [_, _, X, X, X, X, _, _]
         |  |  |  |  ^
         |  |  |  |  |
         cualquier insercion con h ∈ {2,3,4,5} termina aqui
         mas h=6 tambien (slot 6 es "justo despues")

  → 5 de m posiciones alimentan este cluster
```

### Fusión de clusters

Cuando dos clusters adyacentes se fusionan (una inserción llena
el hueco entre ellos), el resultado es un cluster cuyo tamaño es
la suma de ambos más uno. Esto crea un **efecto bola de nieve**:

```
Antes:
  [X, X, X, _, X, X, _, _, _]
   cluster A     cluster B

Insertar con h=3 (slot 3 es el hueco):
  [X, X, X, X, X, X, _, _, _]
   cluster AB (longitud 6!)

Ahora 7 de m posiciones alimentan este cluster
```

### Impacto empírico

La longitud del cluster más largo con sonda lineal:

| $\alpha$ | Cluster esperado más largo ($m = 10000$) |
|---------:|:--------:|
| 0.50 | ~8-12 |
| 0.70 | ~20-30 |
| 0.80 | ~40-60 |
| 0.90 | ~150-200 |
| 0.95 | ~400-600 |

A $\alpha = 0.9$, el cluster más largo tiene ~200 slots. Una
búsqueda fallida que caiga en ese cluster recorre los 200.

---

## Fórmulas de Knuth

Knuth derivó las fórmulas exactas para el número esperado de
sondeos con sonda lineal y hash uniforme:

### Búsqueda exitosa

$$E[\text{sondeos}] \approx \frac{1}{2}\left(1 + \frac{1}{1 - \alpha}\right)$$

### Búsqueda fallida

$$E[\text{sondeos}] \approx \frac{1}{2}\left(1 + \frac{1}{(1 - \alpha)^2}\right)$$

### Tabla numérica

| $\alpha$ | Sondeos éxito | Sondeos fracaso | Factor vs chaining |
|---------:|:--------:|:--------:|:--------:|
| 0.10 | 1.06 | 1.12 | ~1x |
| 0.25 | 1.17 | 1.39 | ~1x |
| 0.50 | 1.50 | 2.50 | ~2.5x miss |
| 0.70 | 2.17 | 6.06 | ~6x miss |
| 0.75 | 2.50 | 8.50 | ~8.5x miss |
| 0.80 | 3.00 | 13.0 | ~13x miss |
| 0.90 | 5.50 | 50.5 | ~50x miss |
| 0.95 | 10.5 | 200.5 | ~200x miss |

La degradación en búsqueda fallida es **cuadrática en**
$1/(1-\alpha)$. Esto explica por qué el umbral de resize debe
ser bajo (~0.7-0.75) — la diferencia entre $\alpha = 0.75$ (8.5
sondeos) y $\alpha = 0.90$ (50.5 sondeos) es un factor de 6x.

---

## Sonda lineal vs chaining: por qué sonda lineal puede ganar

A pesar del clustering, la sonda lineal es **más rápida en tiempo
real** que chaining en muchos escenarios. La razón: localidad de
caché.

### Acceso a memoria

```
Chaining (buscar en bucket de longitud 3):
  acceso 1: table[i]         (array, en cache)
  acceso 2: node1            (heap, posiblemente cache miss)
  acceso 3: node1->next      (heap, probablemente cache miss)
  acceso 4: node2            (heap, probablemente cache miss)
  acceso 5: node2->next      (heap, probablemente cache miss)
  acceso 6: node3            (heap, probablemente cache miss)
  Total: ~5 cache misses

Sonda lineal (buscar con 3 sondeos):
  acceso 1: table[i]         (array, en cache)
  acceso 2: table[i+1]       (array, MISMA linea de cache!)
  acceso 3: table[i+2]       (array, misma o siguiente linea)
  Total: ~1 cache miss
```

Una línea de caché típica (64 bytes) contiene 8 punteros de 8
bytes o 16 enteros de 4 bytes. Los sondeos consecutivos en sonda
lineal acceden a la **misma línea de caché**, mientras que cada
nodo de la lista enlazada es un cache miss probable.

### Punto de cruce

Para elementos pequeños (int, punteros) y $\alpha \leq 0.7$, la
sonda lineal es típicamente **2-3x más rápida** que chaining en
tiempo real a pesar de hacer más comparaciones:

```
Chaining con alpha=0.7:
  ~1.35 comparaciones, ~1.35 cache misses (+ 1 para el array)
  Tiempo: ~1.35 * 100ns = ~135ns

Sonda lineal con alpha=0.7:
  ~2.17 comparaciones, ~1 cache miss (todo en cache line)
  Tiempo: ~1 * 100ns + 1.17 * 5ns = ~106ns
```

Los ~5 ns extras por comparación en cache (vs ~100 ns por cache
miss) hacen que más sondeos con menos misses gane.

---

## Programa completo en C

```c
// linear_probing.c — hash table with linear probing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

typedef struct {
    char **keys;
    int *values;
    char *state;
    int m;
    int n;
} LinearTable;

unsigned int djb2(const char *s, int m) {
    unsigned int h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % m;
}

LinearTable *lp_create(int m) {
    LinearTable *t = malloc(sizeof(LinearTable));
    t->m = m;
    t->n = 0;
    t->keys = calloc(m, sizeof(char *));
    t->values = calloc(m, sizeof(int));
    t->state = calloc(m, sizeof(char));
    return t;
}

int lp_insert(LinearTable *t, const char *key, int value) {
    unsigned int h = djb2(key, t->m);
    int first_deleted = -1;

    for (int step = 0; step < t->m; step++) {
        int i = (h + step) % t->m;

        if (t->state[i] == EMPTY) {
            int target = (first_deleted >= 0) ? first_deleted : i;
            t->keys[target] = strdup(key);
            t->values[target] = value;
            t->state[target] = OCCUPIED;
            t->n++;
            return step + 1;  // probes used
        }

        if (t->state[i] == DELETED) {
            if (first_deleted < 0) first_deleted = i;
            continue;
        }

        if (strcmp(t->keys[i], key) == 0) {
            t->values[i] = value;
            return step + 1;
        }
    }
    return -1;  // full
}

int lp_search(LinearTable *t, const char *key, int *value, int *probes) {
    unsigned int h = djb2(key, t->m);
    *probes = 0;

    for (int step = 0; step < t->m; step++) {
        int i = (h + step) % t->m;
        (*probes)++;

        if (t->state[i] == EMPTY)
            return 0;

        if (t->state[i] == OCCUPIED && strcmp(t->keys[i], key) == 0) {
            if (value) *value = t->values[i];
            return 1;
        }
    }
    return 0;
}

int lp_delete(LinearTable *t, const char *key) {
    unsigned int h = djb2(key, t->m);

    for (int step = 0; step < t->m; step++) {
        int i = (h + step) % t->m;

        if (t->state[i] == EMPTY)
            return 0;

        if (t->state[i] == OCCUPIED && strcmp(t->keys[i], key) == 0) {
            free(t->keys[i]);
            t->keys[i] = NULL;
            t->state[i] = DELETED;
            t->n--;
            return 1;
        }
    }
    return 0;
}

void lp_print(LinearTable *t) {
    for (int i = 0; i < t->m; i++) {
        printf("  [%2d]: ", i);
        switch (t->state[i]) {
            case EMPTY:    printf("_\n"); break;
            case OCCUPIED: printf("(%s, %d)\n", t->keys[i], t->values[i]); break;
            case DELETED:  printf("[DELETED]\n"); break;
        }
    }
}

void lp_free(LinearTable *t) {
    for (int i = 0; i < t->m; i++)
        if (t->state[i] == OCCUPIED) free(t->keys[i]);
    free(t->keys);
    free(t->values);
    free(t->state);
    free(t);
}

// --- cluster analysis ---

void analyze_clusters(LinearTable *t) {
    int max_cluster = 0, num_clusters = 0, cur = 0;
    int total_cluster_len = 0;

    for (int i = 0; i < t->m; i++) {
        if (t->state[i] == OCCUPIED) {
            cur++;
        } else {
            if (cur > 0) {
                num_clusters++;
                total_cluster_len += cur;
                if (cur > max_cluster) max_cluster = cur;
                cur = 0;
            }
        }
    }
    if (cur > 0) {
        num_clusters++;
        total_cluster_len += cur;
        if (cur > max_cluster) max_cluster = cur;
    }

    printf("  clusters: %d, max length: %d, avg length: %.1f\n",
           num_clusters, max_cluster,
           num_clusters > 0 ? (double)total_cluster_len / num_clusters : 0);
}

// --- demos ---

int main(void) {
    srand(42);

    // demo 1: step-by-step insertion
    printf("=== Demo 1: insertion trace ===\n");
    LinearTable *t = lp_create(11);

    const char *names[] = {"alice","bob","carol","dave","eve",
                           "frank","grace","heidi"};
    for (int i = 0; i < 8; i++) {
        int probes = lp_insert(t, names[i], (i + 1) * 100);
        unsigned int h = djb2(names[i], 11);
        printf("  insert \"%s\": h=%u, %d probe(s)\n", names[i], h, probes);
    }
    printf("\nTable (m=11, n=%d, alpha=%.2f):\n", t->n, (double)t->n / t->m);
    lp_print(t);
    analyze_clusters(t);
    printf("\n");

    // demo 2: search
    printf("=== Demo 2: search ===\n");
    const char *queries[] = {"alice", "eve", "grace", "xyz", "bob"};
    for (int i = 0; i < 5; i++) {
        int val, probes;
        int found = lp_search(t, queries[i], &val, &probes);
        printf("  search(\"%s\"): %s, %d probe(s)\n",
               queries[i], found ? "FOUND" : "NOT FOUND", probes);
    }
    printf("\n");

    // demo 3: delete and tombstone
    printf("=== Demo 3: delete and tombstone ===\n");
    printf("  Delete \"carol\":\n");
    lp_delete(t, "carol");
    lp_print(t);

    int probes_val;
    int found = lp_search(t, "dave", NULL, &probes_val);
    printf("  search(\"dave\") after deleting carol: %s, %d probe(s)\n",
           found ? "FOUND" : "NOT FOUND", probes_val);
    printf("  (tombstone preserves the probe chain)\n\n");

    lp_free(t);

    // demo 4: clustering visualization
    printf("=== Demo 4: clustering at different alpha ===\n");
    for (double target = 0.3; target <= 0.9; target += 0.2) {
        int m = 40;
        int n = (int)(target * m);
        LinearTable *ct = lp_create(m);
        char buf[32];

        for (int i = 0; i < n; i++) {
            snprintf(buf, sizeof(buf), "k%d", rand() % 100000);
            lp_insert(ct, buf, i);
        }

        printf("  alpha=%.1f: ", target);
        for (int i = 0; i < m; i++)
            printf("%c", ct->state[i] == OCCUPIED ? '#' : '.');
        printf("\n");
        printf("  ");
        analyze_clusters(ct);
        printf("\n");

        lp_free(ct);
    }

    // demo 5: probes vs alpha (empirical vs Knuth)
    printf("=== Demo 5: probes vs alpha ===\n");
    printf("  %6s  %8s  %8s  %8s  %8s\n",
           "alpha", "hit_emp", "hit_knuth", "miss_emp", "miss_knuth");

    for (double alpha = 0.1; alpha <= 0.95; alpha += 0.1) {
        int m = 10007;  // prime
        int n = (int)(alpha * m);
        LinearTable *at = lp_create(m);
        char buf[32];

        for (int i = 0; i < n; i++) {
            snprintf(buf, sizeof(buf), "key_%d", rand() % 10000000);
            lp_insert(at, buf, i);
        }

        // hit
        double total_hit = 0;
        int hits = 0;
        for (int i = 0; i < m && hits < 500; i++) {
            if (at->state[i] == OCCUPIED) {
                int p;
                lp_search(at, at->keys[i], NULL, &p);
                total_hit += p;
                hits++;
            }
        }

        // miss
        double total_miss = 0;
        for (int i = 0; i < 500; i++) {
            snprintf(buf, sizeof(buf), "miss_%d", i);
            int p;
            lp_search(at, buf, NULL, &p);
            total_miss += p;
        }

        double a = (double)at->n / at->m;
        double knuth_hit = 0.5 * (1 + 1.0 / (1 - a));
        double knuth_miss = 0.5 * (1 + 1.0 / ((1 - a) * (1 - a)));

        printf("  %6.2f  %8.2f  %8.2f  %8.2f  %8.2f\n",
               a, total_hit / hits, knuth_hit,
               total_miss / 500, knuth_miss);

        lp_free(at);
    }
    printf("\n");

    // demo 6: tombstone accumulation
    printf("=== Demo 6: tombstone accumulation ===\n");
    LinearTable *tomb = lp_create(1009);
    char buf[32];

    // insert 700
    for (int i = 0; i < 700; i++) {
        snprintf(buf, sizeof(buf), "w%d", i);
        lp_insert(tomb, buf, i);
    }

    // delete 500
    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "w%d", i);
        lp_delete(tomb, buf);
    }

    // measure search performance
    double total = 0;
    for (int i = 500; i < 700; i++) {
        int p;
        snprintf(buf, sizeof(buf), "w%d", i);
        lp_search(tomb, buf, NULL, &p);
        total += p;
    }
    printf("  Inserted 700, deleted 500 (200 remain, 500 tombstones)\n");
    printf("  n=%d, alpha(effective)=%.2f\n", tomb->n, (double)tomb->n / tomb->m);
    printf("  Avg probes for existing keys: %.2f\n", total / 200);
    printf("  (tombstones inflate probe chains!)\n");

    int occupied = 0, deleted = 0, empty = 0;
    for (int i = 0; i < tomb->m; i++) {
        if (tomb->state[i] == OCCUPIED) occupied++;
        else if (tomb->state[i] == DELETED) deleted++;
        else empty++;
    }
    printf("  Slots: %d occupied, %d deleted, %d empty\n",
           occupied, deleted, empty);

    lp_free(tomb);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o linear_probing linear_probing.c
./linear_probing
```

---

## Programa completo en Rust

```rust
// linear_probing.rs — hash table with linear probing
use std::collections::HashMap;

// --- manual linear probing table ---

#[derive(Clone, PartialEq)]
enum SlotState {
    Empty,
    Occupied,
    Deleted,
}

struct LinearTable {
    keys: Vec<Option<String>>,
    values: Vec<i32>,
    state: Vec<SlotState>,
    m: usize,
    n: usize,
}

impl LinearTable {
    fn new(capacity: usize) -> Self {
        LinearTable {
            keys: vec![None; capacity],
            values: vec![0; capacity],
            state: vec![SlotState::Empty; capacity],
            m: capacity,
            n: 0,
        }
    }

    fn hash(&self, key: &str) -> usize {
        let mut h: u32 = 5381;
        for b in key.bytes() {
            h = h.wrapping_mul(33).wrapping_add(b as u32);
        }
        h as usize % self.m
    }

    fn insert(&mut self, key: &str, value: i32) -> usize {
        let h = self.hash(key);
        let mut first_deleted: Option<usize> = None;

        for step in 0..self.m {
            let i = (h + step) % self.m;

            match self.state[i] {
                SlotState::Empty => {
                    let target = first_deleted.unwrap_or(i);
                    self.keys[target] = Some(key.to_string());
                    self.values[target] = value;
                    self.state[target] = SlotState::Occupied;
                    self.n += 1;
                    return step + 1;
                }
                SlotState::Deleted => {
                    if first_deleted.is_none() {
                        first_deleted = Some(i);
                    }
                }
                SlotState::Occupied => {
                    if self.keys[i].as_deref() == Some(key) {
                        self.values[i] = value;
                        return step + 1;
                    }
                }
            }
        }
        0  // full
    }

    fn search(&self, key: &str) -> (bool, usize) {
        let h = self.hash(key);

        for step in 0..self.m {
            let i = (h + step) % self.m;
            match self.state[i] {
                SlotState::Empty => return (false, step + 1),
                SlotState::Occupied if self.keys[i].as_deref() == Some(key) => {
                    return (true, step + 1);
                }
                _ => {} // Deleted or different key: continue
            }
        }
        (false, self.m)
    }

    fn delete(&mut self, key: &str) -> bool {
        let h = self.hash(key);

        for step in 0..self.m {
            let i = (h + step) % self.m;
            match self.state[i] {
                SlotState::Empty => return false,
                SlotState::Occupied if self.keys[i].as_deref() == Some(key) => {
                    self.keys[i] = None;
                    self.state[i] = SlotState::Deleted;
                    self.n -= 1;
                    return true;
                }
                _ => {}
            }
        }
        false
    }

    fn alpha(&self) -> f64 {
        self.n as f64 / self.m as f64
    }

    fn print(&self) {
        for i in 0..self.m {
            match &self.state[i] {
                SlotState::Empty => println!("  [{:>2}]: _", i),
                SlotState::Occupied => println!("  [{:>2}]: ({}, {})", i,
                    self.keys[i].as_deref().unwrap_or("?"), self.values[i]),
                SlotState::Deleted => println!("  [{:>2}]: [DELETED]", i),
            }
        }
    }

    fn cluster_info(&self) -> (usize, usize, f64) {
        let mut max_c = 0usize;
        let mut num_c = 0usize;
        let mut total = 0usize;
        let mut cur = 0usize;

        for i in 0..self.m {
            if self.state[i] == SlotState::Occupied {
                cur += 1;
            } else {
                if cur > 0 {
                    num_c += 1;
                    total += cur;
                    if cur > max_c { max_c = cur; }
                    cur = 0;
                }
            }
        }
        if cur > 0 {
            num_c += 1;
            total += cur;
            if cur > max_c { max_c = cur; }
        }
        let avg = if num_c > 0 { total as f64 / num_c as f64 } else { 0.0 };
        (num_c, max_c, avg)
    }
}

fn main() {
    // demo 1: insertion trace
    println!("=== Demo 1: insertion trace ===");
    let mut table = LinearTable::new(11);

    let names = ["alice", "bob", "carol", "dave", "eve", "frank", "grace", "heidi"];
    for (i, &name) in names.iter().enumerate() {
        let h = table.hash(name);
        let probes = table.insert(name, (i as i32 + 1) * 100);
        println!("  insert \"{}\": h={}, {} probe(s)", name, h, probes);
    }

    println!("\nTable (m=11, n={}, alpha={:.2}):", table.n, table.alpha());
    table.print();
    let (nc, mc, ac) = table.cluster_info();
    println!("  clusters: {}, max: {}, avg: {:.1}\n", nc, mc, ac);

    // demo 2: search
    println!("=== Demo 2: search ===");
    for &q in &["alice", "eve", "grace", "xyz", "bob"] {
        let (found, probes) = table.search(q);
        println!("  search(\"{}\"): {}, {} probe(s)",
                 q, if found { "FOUND" } else { "NOT FOUND" }, probes);
    }
    println!();

    // demo 3: delete with tombstone
    println!("=== Demo 3: delete and tombstone ===");
    table.delete("carol");
    println!("  After deleting \"carol\":");
    table.print();

    let (found, probes) = table.search("dave");
    println!("  search(\"dave\"): {}, {} probe(s)",
             if found { "FOUND" } else { "NOT FOUND" }, probes);
    println!("  (tombstone preserves probe chain)\n");

    // demo 4: clustering visualization
    println!("=== Demo 4: clustering at different alpha ===");
    let mut rng: u64 = 42;

    for &target in &[0.3, 0.5, 0.7, 0.9] {
        let m = 50;
        let n = (target * m as f64) as usize;
        let mut ct = LinearTable::new(m);

        for i in 0..n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            let key = format!("k{}", (rng >> 16) % 100000);
            ct.insert(&key, i as i32);
        }

        let vis: String = ct.state.iter()
            .map(|s| if *s == SlotState::Occupied { '#' } else { '.' })
            .collect();
        let (nc, mc, _) = ct.cluster_info();
        println!("  alpha={:.1}: {} (clusters={}, max={})",
                 target, vis, nc, mc);
    }
    println!();

    // demo 5: probes vs alpha (empirical vs Knuth)
    println!("=== Demo 5: probes vs alpha ===");
    println!("  {:>6} {:>8} {:>8} {:>8} {:>8}",
             "alpha", "hit_emp", "hit_th", "miss_emp", "miss_th");

    for &alpha in &[0.1, 0.2, 0.3, 0.5, 0.7, 0.8, 0.9] {
        let m = 10007;
        let n = (alpha * m as f64) as usize;
        let mut lt = LinearTable::new(m);
        rng = alpha.to_bits();

        for i in 0..n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            let key = format!("key_{}", (rng >> 16) as u32);
            lt.insert(&key, i as i32);
        }

        // hit
        let mut total_hit = 0usize;
        let mut hits = 0;
        for i in 0..m {
            if lt.state[i] == SlotState::Occupied {
                let (_, p) = lt.search(lt.keys[i].as_deref().unwrap());
                total_hit += p;
                hits += 1;
                if hits >= 500 { break; }
            }
        }

        // miss
        let mut total_miss = 0usize;
        for i in 0..500 {
            let key = format!("miss_{}", i);
            let (_, p) = lt.search(&key);
            total_miss += p;
        }

        let a = lt.alpha();
        let th_hit = 0.5 * (1.0 + 1.0 / (1.0 - a));
        let th_miss = 0.5 * (1.0 + 1.0 / ((1.0 - a) * (1.0 - a)));

        println!("  {:>6.2} {:>8.2} {:>8.2} {:>8.2} {:>8.2}",
                 a, total_hit as f64 / hits as f64, th_hit,
                 total_miss as f64 / 500.0, th_miss);
    }
    println!();

    // demo 6: tombstone effect
    println!("=== Demo 6: tombstone accumulation ===");
    let mut tomb = LinearTable::new(1009);

    for i in 0..700 {
        tomb.insert(&format!("w{}", i), i as i32);
    }
    for i in 0..500 {
        tomb.delete(&format!("w{}", i));
    }

    let mut total = 0usize;
    for i in 500..700 {
        let (_, p) = tomb.search(&format!("w{}", i));
        total += p;
    }

    let occupied = tomb.state.iter().filter(|s| **s == SlotState::Occupied).count();
    let deleted = tomb.state.iter().filter(|s| **s == SlotState::Deleted).count();
    let empty = tomb.state.iter().filter(|s| **s == SlotState::Empty).count();

    println!("  Inserted 700, deleted 500 (200 remain)");
    println!("  Slots: {} occupied, {} deleted, {} empty", occupied, deleted, empty);
    println!("  Avg probes for existing keys: {:.2}", total as f64 / 200.0);
    println!("  (tombstones force longer probe chains)\n");

    // demo 7: HashMap comparison
    println!("=== Demo 7: Rust HashMap (Swiss Table) ===");
    let mut hm: HashMap<String, i32> = HashMap::new();
    for i in 0..10000 {
        hm.insert(format!("key_{}", i), i);
    }
    println!("  HashMap: len={}, capacity={}, alpha={:.3}",
             hm.len(), hm.capacity(),
             hm.len() as f64 / hm.capacity() as f64);
    println!("  Uses open addressing (Swiss Table) internally");
    println!("  No tombstones — uses SIMD-accelerated probing");
    println!("  Handles clustering via better probe sequences");
}
```

Compilar y ejecutar:

```bash
rustc linear_probing.rs && ./linear_probing
```

---

## Errores frecuentes

1. **Poner EMPTY en vez de DELETED al eliminar**: destruye las
   cadenas de sondeo. Claves insertadas después del elemento
   eliminado se vuelven inaccesibles.
2. **No detenerse en EMPTY al buscar**: recorrer toda la tabla
   cuando la cadena ya terminó. Esto convierte cada búsqueda
   fallida en $O(m)$.
3. **Detenerse en DELETED al buscar**: tratar DELETED como EMPTY
   y retornar "no encontrado". La clave puede estar más adelante
   en la cadena.
4. **Operar con $\alpha > 0.9$**: el rendimiento se degrada
   exponencialmente. A $\alpha = 0.95$, cada búsqueda fallida
   toma ~200 sondeos. Redimensionar a $\alpha > 0.75$ como máximo.
5. **Acumular tombstones sin limpiar**: después de muchas
   inserciones y eliminaciones, los tombstones inflan las cadenas
   de sondeo. Periodicamente hacer rehash (reinsertar todos los
   elementos en una tabla limpia) para eliminar tombstones.

---

## Ejercicios

### Ejercicio 1 — Traza de inserción

Inserta las claves con hashes $h = \{3, 7, 3, 3, 7, 0, 8, 3\}$
en una tabla de $m = 11$ con sonda lineal. Dibuja la tabla después
de cada inserción y cuenta los sondeos totales.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos sondeos necesita la última clave ($h = 3$)? ¿Un cluster
se forma alrededor del slot 3?

</details>

### Ejercicio 2 — Traza de eliminación con tombstone

Partiendo de la tabla del ejercicio 1, elimina la segunda clave
insertada (la de $h = 7$). Luego busca la quinta clave ($h = 7$).
¿La encuentra? ¿Qué pasaría si hubieras puesto EMPTY en vez de
DELETED?

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos sondeos necesita la búsqueda con tombstone? ¿Y sin él
(con EMPTY en vez de DELETED)?

</details>

### Ejercicio 3 — Verificar fórmulas de Knuth

Implementa sonda lineal con $m = 10007$ (primo). Para
$\alpha = 0.3, 0.5, 0.7, 0.8, 0.9$, mide empíricamente los
sondeos promedio de búsqueda exitosa y fallida. Compara con las
fórmulas de Knuth.

<details><summary>Predice antes de ejecutar</summary>

¿La medición empírica estará dentro del 10% de la fórmula?
¿En qué $\alpha$ habrá más discrepancia?

</details>

### Ejercicio 4 — Cluster más largo

Para $m = 10000$ y $\alpha = 0.5, 0.7, 0.9$, mide la longitud
del cluster más largo en 10 experimentos. Reporta promedio y
máximo.

<details><summary>Predice antes de ejecutar</summary>

¿El cluster más largo a $\alpha = 0.7$ será ~10, ~30 o ~100?
¿A $\alpha = 0.9$?

</details>

### Ejercicio 5 — Tombstone cleanup

Implementa `lp_rehash(LinearTable *t)` que crea una tabla nueva
del mismo tamaño, reinserta todos los OCCUPIED, y elimina todos
los tombstones. Mide el rendimiento de búsqueda antes y después
del rehash en una tabla con 500 tombstones.

<details><summary>Predice antes de ejecutar</summary>

¿Cuánto mejoran los sondeos promedio después de eliminar 500
tombstones? ¿Vale la pena el costo $O(n)$ del rehash?

</details>

### Ejercicio 6 — Eliminación sin tombstone (backward shift)

Implementa eliminación sin tombstones usando **backward shift
delete**: al eliminar un elemento, mover los siguientes elementos
del cluster hacia atrás para llenar el hueco (solo si su hash
natural los colocaría antes). Compara el rendimiento con tombstones.

<details><summary>Predice antes de ejecutar</summary>

¿Es más complejo de implementar? ¿Cuántos elementos se mueven
en promedio al eliminar?

</details>

### Ejercicio 7 — Sonda lineal con step > 1

Modifica la secuencia de sondeo a $h(k), h(k)+c, h(k)+2c, \ldots$
con $c = 3$. ¿Esto reduce el clustering? ¿Visita todos los
buckets si $\gcd(c, m) > 1$?

<details><summary>Predice antes de ejecutar</summary>

Con $c = 3$ y $m = 12$, ¿qué slots se visitan? ¿Se visitan los
12 buckets? ¿Qué pasa con $m = 11$ (primo)?

</details>

### Ejercicio 8 — Robin Hood hashing

Implementa la variante **Robin Hood**: al insertar, si el nuevo
elemento tiene un "desplazamiento" (distancia desde su hash
natural) mayor que el del elemento actual, intercambiarlos.
Esto iguala los desplazamientos y reduce la varianza de sondeos.

<details><summary>Predice antes de ejecutar</summary>

¿Robin Hood reduce el peor caso de búsqueda? ¿Cuánto mejora
el promedio vs sonda lineal estándar?

</details>

### Ejercicio 9 — Sonda lineal vs chaining: benchmark

Para $10^5$ strings de longitud 10, mide el tiempo de inserción y
búsqueda (exitosa y fallida) con sonda lineal ($\alpha = 0.7$) vs
chaining ($\alpha = 0.7$). Usa `clock()` en C.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será más rápida en tiempo real? Pista: sonda lineal tiene
mejor cache locality pero más sondeos.

</details>

### Ejercicio 10 — Tabla de enteros (sin strings)

Implementa sonda lineal para claves `int` (sin `strcmp`, sin
`strdup`). Mide los sondeos para $10^6$ inserciones con
$\alpha = 0.7$. Compara con la versión de strings.

<details><summary>Predice antes de ejecutar</summary>

¿Cuánto más rápida es la versión de enteros? ¿La distribución
de sondeos es diferente?

</details>
