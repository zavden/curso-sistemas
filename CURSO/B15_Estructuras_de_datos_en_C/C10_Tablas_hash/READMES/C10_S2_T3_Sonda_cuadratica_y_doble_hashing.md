# Sonda cuadrática y doble hashing

## Objetivo

Estudiar dos variantes de open addressing que reducen el
**clustering primario** de la sonda lineal:

- **Sonda cuadrática**: secuencia $h(k) + c_1 i + c_2 i^2$, saltos
  crecientes que dispersan los sondeos.
- **Doble hashing**: secuencia $h_1(k) + i \cdot h_2(k)$, paso
  dependiente de la clave que elimina el clustering secundario.
- Comparación de las tres variantes (lineal, cuadrática, doble).
- Restricciones sobre $m$ para garantizar cobertura completa.
- Referencia: T02 cubrió sonda lineal y clustering primario.

---

## Clustering primario vs secundario

### Recapitulación: clustering primario (sonda lineal)

Claves con **hashes diferentes** que caen en el mismo cluster
siguen la **misma secuencia** de sondeo a partir de ese punto.
Los clusters crecen y se fusionan:

```
Clave A: h=3 → probar 3, 4, 5, 6, 7, ...
Clave B: h=5 → probar 5, 6, 7, 8, ...
                      ^^^^^^^^
  B sigue la MISMA secuencia que A desde el slot 5
  → compiten por los mismos slots → cluster crece
```

### Clustering secundario

La sonda cuadrática elimina el clustering primario (claves con
hashes diferentes siguen secuencias diferentes), pero introduce
**clustering secundario**: claves con el **mismo hash** siguen
exactamente la misma secuencia de sondeo.

```
Clave A: h=3 → probar 3, 4, 7, 12, 19, ...
Clave B: h=3 → probar 3, 4, 7, 12, 19, ...  (identica!)
Clave C: h=5 → probar 5, 6, 9, 14, 21, ...  (diferente ✓)
```

A y B (mismo hash) compiten; C (hash diferente) no. Esto es
menos grave que el clustering primario porque requiere colisión
exacta de hash, no solo proximidad.

### Doble hashing: sin clustering

Con doble hashing, incluso claves con el mismo $h_1$ siguen
secuencias diferentes si tienen distinto $h_2$:

```
Clave A: h1=3, h2=5 → probar 3, 8, 13, 18, ...
Clave B: h1=3, h2=7 → probar 3, 10, 17, 24, ...  (diferente ✓)
Clave C: h1=5, h2=3 → probar 5, 8, 11, 14, ...   (diferente ✓)
```

Ningún par comparte secuencia completa salvo colisión en ambas
funciones hash (muy raro).

---

## Sonda cuadrática

### Secuencia de sondeo

$$s_i = (h(k) + c_1 \cdot i + c_2 \cdot i^2) \bmod m$$

La variante más común usa $c_1 = 0, c_2 = 1$:

$$s_i = (h(k) + i^2) \bmod m$$

O la variante alternante $c_1 = 1/2, c_2 = 1/2$ (que se
implementa como $h(k) + i(i+1)/2$ con aritmética entera):

$$s_i = \left(h(k) + \frac{i(i+1)}{2}\right) \bmod m$$

### Ejemplo

Con $h(k) = 3$ y $m = 16$, secuencia $s_i = (3 + i^2) \bmod 16$:

```
i=0: (3 + 0)  % 16 =  3
i=1: (3 + 1)  % 16 =  4
i=2: (3 + 4)  % 16 =  7
i=3: (3 + 9)  % 16 = 12
i=4: (3 + 16) % 16 =  3  ← cicla!
```

Con solo 4 sondeos visitamos los slots $\{3, 4, 7, 12\}$ y luego
el patrón se repite. Esto es un problema: **no se visitan todos
los slots**.

### Garantía de cobertura

Para que la sonda cuadrática visite todos los slots, se necesitan
restricciones sobre $m$:

**Opción 1**: $m$ primo y $s_i = h + i^2$. Se garantiza que se
visitan al menos $m/2$ slots distintos antes de ciclar. Si
$\alpha \leq 0.5$, siempre se encontrará un slot libre.

**Opción 2**: $m$ potencia de 2 y
$s_i = h + \frac{i(i+1)}{2}$. Esta variante triangular visita
**todos** los $m$ slots:

```
m = 8, h = 0, s_i = i(i+1)/2 mod 8:
  i=0: 0
  i=1: 1
  i=2: 3
  i=3: 6
  i=4: 2   (10 mod 8)
  i=5: 7   (15 mod 8)
  i=6: 5   (21 mod 8)
  i=7: 4   (28 mod 8)
  → visita {0,1,2,3,4,5,6,7} — todos!
```

**Opción 3**: $m$ primo de la forma $4k + 3$ y secuencia
$+1, -1, +4, -4, +9, -9, \ldots$ (alternando signos). Garantiza
cobertura completa.

### Implementación

```c
// quadratic probing: s_i = (h + i*(i+1)/2) mod m
// requires m = power of 2

int qp_insert(QuadTable *t, const char *key, int value) {
    unsigned int h = hash(key, t->m);

    for (int i = 0; i < t->m; i++) {
        int idx = (h + i * (i + 1) / 2) % t->m;

        if (t->state[idx] == EMPTY || t->state[idx] == DELETED) {
            t->keys[idx] = strdup(key);
            t->values[idx] = value;
            t->state[idx] = OCCUPIED;
            t->n++;
            return i + 1;  // probes
        }

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0) {
            t->values[idx] = value;
            return i + 1;
        }
    }
    return -1;  // full
}

int qp_search(QuadTable *t, const char *key, int *probes) {
    unsigned int h = hash(key, t->m);
    *probes = 0;

    for (int i = 0; i < t->m; i++) {
        int idx = (h + i * (i + 1) / 2) % t->m;
        (*probes)++;

        if (t->state[idx] == EMPTY)
            return 0;

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0)
            return 1;
    }
    return 0;
}
```

### Traza

Tabla $m = 16$, hashes $\{3, 3, 3, 3, 7, 7\}$, sonda
$h + i(i+1)/2$:

```
Insertar k1 (h=3): slot 3 libre → [3]=k1
  Probes: 1

Insertar k2 (h=3): slot 3 ocupado
  i=1: (3+1)%16 = 4 → libre → [4]=k2
  Probes: 2

Insertar k3 (h=3): slots 3, 4 ocupados
  i=2: (3+3)%16 = 6 → libre → [6]=k3
  Probes: 3

Insertar k4 (h=3): slots 3, 4, 6 ocupados
  i=3: (3+6)%16 = 9 → libre → [9]=k4
  Probes: 4

Insertar k5 (h=7): slot 7 libre → [7]=k5
  Probes: 1

Insertar k6 (h=7): slot 7 ocupado
  i=1: (7+1)%16 = 8 → libre → [8]=k6
  Probes: 2

Tabla final:
  [3]=k1  [4]=k2  [6]=k3  [7]=k5  [8]=k6  [9]=k4
```

Comparar con sonda lineal para los mismos hashes:

```
Sonda lineal:
  [3]=k1  [4]=k2  [5]=k3  [6]=k4  [7]=k5  [8]=k6
  → cluster contiguo de 6 slots!

Sonda cuadratica:
  [3]=k1  [4]=k2  [6]=k3  [7]=k5  [8]=k6  [9]=k4
  → los slots estan mas dispersos, sin cluster largo
```

---

## Doble hashing

### Secuencia de sondeo

$$s_i = (h_1(k) + i \cdot h_2(k)) \bmod m$$

donde $h_1$ y $h_2$ son dos funciones hash independientes.

### Requisito: $\gcd(h_2(k), m) = 1$

Para que la secuencia visite todos los $m$ slots, $h_2(k)$ debe
ser **coprimo** con $m$. Dos formas de garantizarlo:

1. **$m$ primo**: cualquier $h_2(k) \in [1, m-1]$ es coprimo con
   $m$. Solo hay que asegurar $h_2(k) \neq 0$.
2. **$m$ potencia de 2**: $h_2(k)$ debe ser impar.

### Funciones hash típicas

```c
unsigned int h1(const char *key, int m) {
    // primary hash: djb2
    unsigned int h = 5381;
    while (*key) h = h * 33 + (unsigned char)*key++;
    return h % m;
}

unsigned int h2(const char *key, int m) {
    // secondary hash: must never return 0
    unsigned int h = 0;
    while (*key) h = h * 31 + (unsigned char)*key++;
    return 1 + (h % (m - 1));  // range [1, m-1]
}
```

El truco `1 + (h % (m - 1))` garantiza que el resultado está en
$[1, m-1]$, nunca es 0.

Para enteros, una opción simple con $m$ primo:

```c
unsigned int h1_int(int k, int m) { return ((k % m) + m) % m; }
unsigned int h2_int(int k, int m) { return 1 + (((k / m) % (m - 1)) + (m - 1)) % (m - 1); }
```

### Implementación

```c
int dh_insert(DHTable *t, const char *key, int value) {
    unsigned int hash1 = h1(key, t->m);
    unsigned int hash2 = h2(key, t->m);

    for (int i = 0; i < t->m; i++) {
        int idx = (hash1 + i * hash2) % t->m;

        if (t->state[idx] == EMPTY || t->state[idx] == DELETED) {
            t->keys[idx] = strdup(key);
            t->values[idx] = value;
            t->state[idx] = OCCUPIED;
            t->n++;
            return i + 1;
        }

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0) {
            t->values[idx] = value;
            return i + 1;
        }
    }
    return -1;
}

int dh_search(DHTable *t, const char *key, int *probes) {
    unsigned int hash1 = h1(key, t->m);
    unsigned int hash2 = h2(key, t->m);
    *probes = 0;

    for (int i = 0; i < t->m; i++) {
        int idx = (hash1 + i * hash2) % t->m;
        (*probes)++;

        if (t->state[idx] == EMPTY)
            return 0;

        if (t->state[idx] == OCCUPIED &&
            strcmp(t->keys[idx], key) == 0)
            return 1;
    }
    return 0;
}
```

### Traza

Tabla $m = 11$ (primo), claves con $h_1 = \{3, 3, 3, 3, 7, 7\}$,
$h_2 = \{5, 3, 7, 2, 4, 6\}$:

```
k1: h1=3, h2=5 → slot 3 libre → [3]=k1

k2: h1=3, h2=3 → slot 3 ocupado
  i=1: (3+3)%11 = 6 → libre → [6]=k2

k3: h1=3, h2=7 → slot 3 ocupado
  i=1: (3+7)%11 = 10 → libre → [10]=k3

k4: h1=3, h2=2 → slot 3 ocupado
  i=1: (3+2)%11 = 5 → libre → [5]=k4

k5: h1=7, h2=4 → slot 7 libre → [7]=k5

k6: h1=7, h2=6 → slot 7 ocupado
  i=1: (7+6)%11 = 2 → libre → [2]=k6

Tabla: [_, _, k6, k1, _, k4, k2, k5, _, _, k3]
```

Todas las claves con $h_1 = 3$ fueron a slots diferentes (3, 6,
10, 5) porque cada una tenía un $h_2$ distinto. Sin clustering
alguno.

---

## Comparación de las tres variantes

### Sondeos esperados (búsqueda fallida, hash uniforme)

| $\alpha$ | Lineal | Cuadrática | Doble | Chaining |
|---------:|:------:|:----------:|:-----:|:--------:|
| 0.50 | 2.50 | 2.19 | 2.00 | 0.50 |
| 0.70 | 6.06 | 3.68 | 3.33 | 0.70 |
| 0.80 | 13.0 | 5.81 | 5.00 | 0.80 |
| 0.90 | 50.5 | 11.4 | 10.0 | 0.90 |
| 0.95 | 200.5 | 22.0 | 20.0 | 0.95 |

### Sondeos esperados (búsqueda exitosa, hash uniforme)

| $\alpha$ | Lineal | Cuadrática | Doble | Chaining |
|---------:|:------:|:----------:|:-----:|:--------:|
| 0.50 | 1.50 | 1.44 | 1.39 | 1.25 |
| 0.70 | 2.17 | 1.85 | 1.72 | 1.35 |
| 0.80 | 3.00 | 2.21 | 2.01 | 1.40 |
| 0.90 | 5.50 | 2.85 | 2.56 | 1.45 |

Las fórmulas para doble hashing (que se aproxima al hashing
uniforme ideal):

$$\text{Miss}: \frac{1}{1-\alpha} \qquad \text{Hit}: \frac{1}{\alpha} \ln \frac{1}{1-\alpha}$$

### Trade-offs

| Aspecto | Lineal | Cuadrática | Doble |
|---------|:------:|:----------:|:-----:|
| Cache locality | excelente | buena | mala |
| Clustering primario | sí | no | no |
| Clustering secundario | sí | sí | no |
| Costo por sondeo | 1 hash + 1 add | 1 hash + 1 mul | 2 hashes + 1 mul |
| Restricción en $m$ | ninguna | primo o $2^k$ | primo (ideal) |
| Complejidad de código | baja | baja | media |

### ¿Cuál elegir?

- **Sonda lineal**: si $\alpha \leq 0.7$ y el rendimiento de
  caché importa (la mayoría de los casos en RAM).
- **Sonda cuadrática**: si $\alpha$ puede llegar a 0.8-0.9 y se
  quiere evitar el clustering primario sin el costo de dos hashes.
- **Doble hashing**: si se necesita la distribución más uniforme
  posible y el costo de calcular $h_2$ no es problema (claves
  largas donde el hash ya domina).

En la práctica, la sonda lineal con buen hash (como Swiss Table)
domina — la localidad de caché compensa el clustering si se
mantiene $\alpha$ bajo.

---

## Programa completo en C

```c
// probing_variants.c — linear, quadratic, and double hashing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

typedef struct {
    int *keys;     // integer keys for simplicity
    char *state;
    int m, n;
} ProbeTable;

// --- hash functions ---

unsigned int hash1(int key, int m) {
    return ((unsigned int)(key * 2654435769U)) % m;
}

unsigned int hash2(int key, int m) {
    // must return [1, m-1], m should be prime
    return 1 + (((unsigned int)(key * 1103515245U)) % (m - 1));
}

// --- create/free ---

ProbeTable *pt_create(int m) {
    ProbeTable *t = malloc(sizeof(ProbeTable));
    t->m = m; t->n = 0;
    t->keys = calloc(m, sizeof(int));
    t->state = calloc(m, sizeof(char));
    return t;
}

void pt_free(ProbeTable *t) {
    free(t->keys); free(t->state); free(t);
}

// --- insert/search with probe function ---

typedef int (*ProbeFn)(int h1, int h2, int i, int m);

int probe_linear(int h1, int h2, int i, int m) {
    (void)h2;
    return (h1 + i) % m;
}

int probe_quadratic(int h1, int h2, int i, int m) {
    (void)h2;
    return (h1 + i * (i + 1) / 2) % m;
}

int probe_double(int h1, int h2, int i, int m) {
    return (h1 + i * h2) % m;
}

int pt_insert(ProbeTable *t, int key, ProbeFn probe) {
    int h1 = hash1(key, t->m);
    int h2 = hash2(key, t->m);

    for (int i = 0; i < t->m; i++) {
        int idx = probe(h1, h2, i, t->m);
        if (t->state[idx] != OCCUPIED) {
            t->keys[idx] = key;
            t->state[idx] = OCCUPIED;
            t->n++;
            return i + 1;
        }
        if (t->keys[idx] == key) return i + 1;
    }
    return -1;
}

int pt_search(ProbeTable *t, int key, ProbeFn probe) {
    int h1 = hash1(key, t->m);
    int h2 = hash2(key, t->m);

    for (int i = 0; i < t->m; i++) {
        int idx = probe(h1, h2, i, t->m);
        if (t->state[idx] == EMPTY) return i + 1;
        if (t->state[idx] == OCCUPIED && t->keys[idx] == key) return i + 1;
    }
    return t->m;
}

// --- demos ---

int main(void) {
    srand(42);

    // demo 1: trace comparison
    printf("=== Demo 1: insertion trace — same keys, 3 methods ===\n");
    int keys[] = {10, 22, 31, 4, 15, 28, 17, 88, 59, 73};
    int nkeys = 10;
    int m = 17;  // prime, works for all methods

    const char *names[] = {"Linear", "Quadratic", "Double"};
    ProbeFn fns[] = {probe_linear, probe_quadratic, probe_double};

    for (int f = 0; f < 3; f++) {
        ProbeTable *t = pt_create(m);
        int total_probes = 0;
        printf("  %s (m=%d):\n", names[f], m);

        for (int i = 0; i < nkeys; i++) {
            int probes = pt_insert(t, keys[i], fns[f]);
            total_probes += probes;
            if (probes > 1) {
                printf("    insert %2d: h1=%2d, %d probe(s)\n",
                       keys[i], hash1(keys[i], m), probes);
            }
        }
        printf("    Total probes: %d (avg %.2f)\n\n",
               total_probes, (double)total_probes / nkeys);

        // show table
        printf("    Table: [");
        for (int i = 0; i < m; i++) {
            if (t->state[i] == OCCUPIED) printf("%d", t->keys[i]);
            else printf("_");
            if (i < m - 1) printf(",");
        }
        printf("]\n\n");

        pt_free(t);
    }

    // demo 2: probes vs alpha
    printf("=== Demo 2: probes vs alpha ===\n");
    printf("  %6s  %8s  %8s  %8s  %8s  %8s  %8s\n",
           "alpha", "lin_hit", "lin_miss", "quad_hit", "quad_miss",
           "dbl_hit", "dbl_miss");

    int big_m = 10007;  // prime
    for (double alpha = 0.1; alpha <= 0.91; alpha += 0.1) {
        int n = (int)(alpha * big_m);

        double results[3][2] = {{0}};  // [method][hit/miss]

        for (int f = 0; f < 3; f++) {
            ProbeTable *t = pt_create(big_m);
            for (int i = 0; i < n; i++)
                pt_insert(t, rand() % 10000000, fns[f]);

            // hit
            double total_hit = 0;
            int hits = 0;
            for (int i = 0; i < big_m && hits < 500; i++) {
                if (t->state[i] == OCCUPIED) {
                    total_hit += pt_search(t, t->keys[i], fns[f]);
                    hits++;
                }
            }
            results[f][0] = total_hit / hits;

            // miss
            double total_miss = 0;
            for (int i = 0; i < 500; i++)
                total_miss += pt_search(t, -(i + 1), fns[f]);
            results[f][1] = total_miss / 500;

            pt_free(t);
        }

        printf("  %6.2f  %8.2f  %8.2f  %8.2f  %8.2f  %8.2f  %8.2f\n",
               alpha,
               results[0][0], results[0][1],
               results[1][0], results[1][1],
               results[2][0], results[2][1]);
    }
    printf("\n");

    // demo 3: cluster analysis
    printf("=== Demo 3: cluster analysis at alpha=0.7 ===\n");
    int vis_m = 60;
    int vis_n = (int)(0.7 * vis_m);

    for (int f = 0; f < 3; f++) {
        // use prime m for all
        ProbeTable *t = pt_create(61);  // prime close to 60
        srand(999);
        for (int i = 0; i < vis_n; i++)
            pt_insert(t, rand() % 100000, fns[f]);

        printf("  %s:\n    ", names[f]);
        for (int i = 0; i < 61; i++)
            printf("%c", t->state[i] == OCCUPIED ? '#' : '.');
        printf("\n");

        // count clusters
        int max_c = 0, nc = 0, cur = 0;
        for (int i = 0; i < 61; i++) {
            if (t->state[i] == OCCUPIED) { cur++; }
            else {
                if (cur > 0) { nc++; if (cur > max_c) max_c = cur; cur = 0; }
            }
        }
        if (cur > 0) { nc++; if (cur > max_c) max_c = cur; }
        printf("    clusters: %d, max: %d\n\n", nc, max_c);

        pt_free(t);
    }

    // demo 4: quadratic coverage test
    printf("=== Demo 4: quadratic probing coverage ===\n");
    // with m = power of 2, triangular numbers cover all slots
    int m_pow2 = 16;
    printf("  m=%d (power of 2), h=0, s_i = i(i+1)/2:\n    ", m_pow2);
    int visited[16] = {0};
    for (int i = 0; i < m_pow2; i++) {
        int idx = (i * (i + 1) / 2) % m_pow2;
        visited[idx] = 1;
        printf("%d ", idx);
    }
    int count = 0;
    for (int i = 0; i < m_pow2; i++) count += visited[i];
    printf("\n    Distinct slots visited: %d/%d\n\n", count, m_pow2);

    // with m = 13 (prime), i^2 covers only some slots
    int m_prime = 13;
    printf("  m=%d (prime), h=0, s_i = i^2:\n    ", m_prime);
    int visited2[13] = {0};
    for (int i = 0; i < m_prime; i++) {
        int idx = (i * i) % m_prime;
        visited2[idx] = 1;
        printf("%d ", idx);
    }
    count = 0;
    for (int i = 0; i < m_prime; i++) count += visited2[i];
    printf("\n    Distinct slots visited: %d/%d\n\n", count, m_prime);

    // demo 5: double hashing h2 values
    printf("=== Demo 5: double hashing — h2 diversity ===\n");
    printf("  m=%d (prime), h2 values for keys 0..19:\n", 17);
    for (int k = 0; k < 20; k++) {
        printf("    key=%2d: h1=%2d, h2=%2d\n", k, hash1(k, 17), hash2(k, 17));
    }
    printf("  All h2 in [1, %d] — guarantees full coverage\n\n", 16);

    // demo 6: theory comparison
    printf("=== Demo 6: theoretical probe counts ===\n");
    printf("  %6s  %8s  %8s  %8s  %8s\n",
           "alpha", "linear", "uniform", "chaining", "improvement");
    for (double a = 0.1; a <= 0.95; a += 0.1) {
        double lin = 0.5 * (1 + 1.0 / ((1 - a) * (1 - a)));
        double uni = 1.0 / (1 - a);
        double cha = a;
        printf("  %6.2f  %8.2f  %8.2f  %8.2f  %8.1fx\n",
               a, lin, uni, cha, lin / uni);
    }
    printf("  (uniform ≈ double hashing; improvement = linear/uniform)\n");

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o probing_variants probing_variants.c -lm
./probing_variants
```

---

## Programa completo en Rust

```rust
// probing_variants.rs — linear, quadratic, double hashing comparison
use std::collections::HashMap;

// --- probe table with configurable strategy ---

#[derive(Clone, PartialEq)]
enum State { Empty, Occupied, Deleted }

enum ProbeStrategy { Linear, Quadratic, Double }

struct ProbeTable {
    keys: Vec<i32>,
    state: Vec<State>,
    m: usize,
    n: usize,
    strategy: ProbeStrategy,
}

fn hash1(key: i32, m: usize) -> usize {
    ((key as u32).wrapping_mul(2654435769)) as usize % m
}

fn hash2(key: i32, m: usize) -> usize {
    1 + ((key as u32).wrapping_mul(1103515245)) as usize % (m - 1)
}

impl ProbeTable {
    fn new(m: usize, strategy: ProbeStrategy) -> Self {
        ProbeTable {
            keys: vec![0; m],
            state: vec![State::Empty; m],
            m, n: 0, strategy,
        }
    }

    fn probe(&self, h1: usize, h2: usize, i: usize) -> usize {
        match self.strategy {
            ProbeStrategy::Linear => (h1 + i) % self.m,
            ProbeStrategy::Quadratic => (h1 + i * (i + 1) / 2) % self.m,
            ProbeStrategy::Double => (h1 + i * h2) % self.m,
        }
    }

    fn insert(&mut self, key: i32) -> usize {
        let h1 = hash1(key, self.m);
        let h2 = hash2(key, self.m);

        for i in 0..self.m {
            let idx = self.probe(h1, h2, i);
            match self.state[idx] {
                State::Empty | State::Deleted => {
                    self.keys[idx] = key;
                    self.state[idx] = State::Occupied;
                    self.n += 1;
                    return i + 1;
                }
                State::Occupied if self.keys[idx] == key => {
                    return i + 1;
                }
                _ => {}
            }
        }
        0
    }

    fn search(&self, key: i32) -> usize {
        let h1 = hash1(key, self.m);
        let h2 = hash2(key, self.m);

        for i in 0..self.m {
            let idx = self.probe(h1, h2, i);
            match self.state[idx] {
                State::Empty => return i + 1,
                State::Occupied if self.keys[idx] == key => return i + 1,
                _ => {}
            }
        }
        self.m
    }

    fn alpha(&self) -> f64 { self.n as f64 / self.m as f64 }

    fn cluster_info(&self) -> (usize, usize) {
        let mut max_c = 0; let mut nc = 0; let mut cur = 0;
        for i in 0..self.m {
            if self.state[i] == State::Occupied { cur += 1; }
            else {
                if cur > 0 { nc += 1; if cur > max_c { max_c = cur; } cur = 0; }
            }
        }
        if cur > 0 { nc += 1; if cur > max_c { max_c = cur; } }
        (nc, max_c)
    }
}

fn main() {
    // demo 1: insertion trace
    println!("=== Demo 1: insertion trace ===");
    let keys = [10, 22, 31, 4, 15, 28, 17, 88, 59, 73];
    let m = 17;

    let strats: Vec<(&str, ProbeStrategy)> = vec![
        ("Linear", ProbeStrategy::Linear),
        ("Quadratic", ProbeStrategy::Quadratic),
        ("Double", ProbeStrategy::Double),
    ];

    for (name, strat) in &strats {
        let mut t = ProbeTable::new(m, match strat {
            ProbeStrategy::Linear => ProbeStrategy::Linear,
            ProbeStrategy::Quadratic => ProbeStrategy::Quadratic,
            ProbeStrategy::Double => ProbeStrategy::Double,
        });
        // reconstruct strategy since we can't copy enum
        let mut t = ProbeTable::new(m, match name {
            &"Linear" => ProbeStrategy::Linear,
            &"Quadratic" => ProbeStrategy::Quadratic,
            _ => ProbeStrategy::Double,
        });

        let mut total = 0;
        for &k in &keys {
            let p = t.insert(k);
            total += p;
            if p > 1 {
                println!("  {}: insert {} (h1={}): {} probes",
                         name, k, hash1(k, m), p);
            }
        }
        println!("  {}: total probes={}, avg={:.2}\n",
                 name, total, total as f64 / keys.len() as f64);
    }

    // demo 2: probes vs alpha
    println!("=== Demo 2: probes vs alpha (miss) ===");
    println!("  {:>6} {:>8} {:>8} {:>8}", "alpha", "linear", "quadr", "double");

    let big_m = 10007;
    for alpha_pct in (10..=90).step_by(10) {
        let alpha = alpha_pct as f64 / 100.0;
        let n = (alpha * big_m as f64) as usize;

        let mut results = [0.0f64; 3];
        let strategy_names = ["linear", "quadratic", "double"];

        for (s, _) in strategy_names.iter().enumerate() {
            let strat = match s {
                0 => ProbeStrategy::Linear,
                1 => ProbeStrategy::Quadratic,
                _ => ProbeStrategy::Double,
            };
            let mut t = ProbeTable::new(big_m, strat);
            let mut rng = (alpha_pct as u64) * 999983;

            for _ in 0..n {
                rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
                t.insert((rng >> 16) as i32);
            }

            let mut total_miss = 0usize;
            for i in 0..500 {
                total_miss += t.search(-(i + 1));
            }
            results[s] = total_miss as f64 / 500.0;
        }

        println!("  {:>6.2} {:>8.2} {:>8.2} {:>8.2}",
                 alpha, results[0], results[1], results[2]);
    }
    println!();

    // demo 3: cluster visualization
    println!("=== Demo 3: cluster visualization (alpha=0.7) ===");
    let vis_m = 61;
    let vis_n = (0.7 * vis_m as f64) as usize;

    for &(name, make_strat) in &[
        ("Linear", ProbeStrategy::Linear as fn() -> _ ),
    ] {
        // inline to avoid closure complexity
    }
    // simplified approach
    for name in &["Linear", "Quadratic", "Double"] {
        let strat = match *name {
            "Linear" => ProbeStrategy::Linear,
            "Quadratic" => ProbeStrategy::Quadratic,
            _ => ProbeStrategy::Double,
        };
        let mut t = ProbeTable::new(vis_m, strat);
        let mut rng: u64 = 999;

        for _ in 0..vis_n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            t.insert((rng >> 16) as i32);
        }

        let vis: String = t.state.iter()
            .map(|s| if *s == State::Occupied { '#' } else { '.' })
            .collect();
        let (nc, mc) = t.cluster_info();
        println!("  {:>10}: {} clusters={}, max={}", name, vis, nc, mc);
    }
    println!();

    // demo 4: quadratic coverage
    println!("=== Demo 4: quadratic coverage ===");
    let m16 = 16;
    print!("  m=16, h=0, triangular: ");
    let mut visited = vec![false; m16];
    for i in 0..m16 {
        let idx = (i * (i + 1) / 2) % m16;
        visited[idx] = true;
        print!("{} ", idx);
    }
    println!("\n  Distinct: {}/{}\n", visited.iter().filter(|&&v| v).count(), m16);

    // demo 5: theory vs empirical
    println!("=== Demo 5: theoretical probes (miss) ===");
    println!("  {:>6} {:>10} {:>10} {:>10}", "alpha", "linear", "double/unif", "chaining");
    for alpha_pct in (10..=90).step_by(10) {
        let a = alpha_pct as f64 / 100.0;
        let lin = 0.5 * (1.0 + 1.0 / ((1.0 - a) * (1.0 - a)));
        let dbl = 1.0 / (1.0 - a);
        let cha = a;
        println!("  {:>6.2} {:>10.2} {:>10.2} {:>10.2}", a, lin, dbl, cha);
    }
    println!();

    // demo 6: HashMap — Rust uses open addressing (Swiss Table)
    println!("=== Demo 6: Rust HashMap internals ===");
    let mut hm: HashMap<i32, i32> = HashMap::new();
    for i in 0..10000 {
        hm.insert(i, i * 10);
    }
    println!("  HashMap: len={}, capacity={}", hm.len(), hm.capacity());
    println!("  alpha={:.3}", hm.len() as f64 / hm.capacity() as f64);
    println!("  Uses Swiss Table (SIMD-accelerated open addressing)");
    println!("  Not linear/quadratic/double — uses control bytes + SIMD");
    println!("  Groups of 16 slots with 1-byte metadata each");
    println!("  Achieves ~0.875 max load factor with good performance");
}
```

Compilar y ejecutar:

```bash
rustc probing_variants.rs && ./probing_variants
```

---

## Errores frecuentes

1. **$h_2(k) = 0$ en doble hashing**: el paso es 0, así que se
   prueba el mismo slot infinitamente. Siempre garantizar
   $h_2(k) \geq 1$ (e.g., `1 + hash % (m-1)`).
2. **$\gcd(h_2, m) > 1$**: la secuencia no visita todos los slots.
   Usar $m$ primo o forzar $h_2$ impar si $m = 2^k$.
3. **Sonda cuadrática con $m$ incorrecto**: con $m$ no primo y
   $s_i = i^2$, la secuencia puede ciclar sin visitar todos los
   slots. Con $m = 16$ y $s_i = i^2$: slots visitados =
   $\{0, 1, 4, 9, 0, 9, 4, 1, \ldots\}$ — solo 4 distintos.
4. **Usar la misma función hash para $h_1$ y $h_2$**: si
   $h_2 = h_1$, la secuencia es lineal con paso $h_1$. Se necesitan
   funciones **independientes** (diferentes constantes/bases).
5. **Olvidar tombstones**: las tres variantes de open addressing
   necesitan tombstones para eliminación. El error es el mismo que
   en sonda lineal (T02).

---

## Ejercicios

### Ejercicio 1 — Traza cuadrática

Inserta claves con hashes $h = \{5, 5, 5, 5, 5\}$ en una tabla de
$m = 16$ (potencia de 2) con sonda $h + i(i+1)/2$. Dibuja la
tabla y cuenta los sondeos totales.

<details><summary>Predice antes de ejecutar</summary>

¿La quinta clave (5to h=5) necesita 5 sondeos? ¿Los slots
visitados son contiguos o dispersos?

</details>

### Ejercicio 2 — Traza de doble hashing

Con $m = 13$ (primo), inserta claves con
$(h_1, h_2) = \{(3,5), (3,7), (3,2), (10,3), (10,5)\}$. Dibuja la
tabla final y verifica que no hay clusters.

<details><summary>Predice antes de ejecutar</summary>

¿Las dos claves con $h_1=3$ caen en slots adyacentes? ¿Las dos
con $h_1=10$?

</details>

### Ejercicio 3 — Cobertura cuadrática

Para $m = 12$ (no primo, no potencia de 2) y $h = 0$, lista los
slots visitados por $s_i = i^2 \bmod 12$ para $i = 0, \ldots, 11$.
¿Cuántos slots distintos se visitan? Repite con $m = 13$ (primo).

<details><summary>Predice antes de ejecutar</summary>

Con $m = 12$, ¿se visitan los 12 slots o menos? ¿Con $m = 13$?

</details>

### Ejercicio 4 — Comparar clustering empíricamente

Para $m = 10007$ y $\alpha = 0.7$, inserta las mismas claves con
las 3 estrategias. Mide: (a) cluster más largo, (b) número de
clusters, (c) sondeos promedio hit/miss.

<details><summary>Predice antes de ejecutar</summary>

¿El cluster más largo con sonda lineal será ~5x, ~10x o ~20x
mayor que con cuadrática?

</details>

### Ejercicio 5 — $h_2$ problemática

Implementa doble hashing con $m = 10$ (no primo) y
$h_2(k) = k \bmod 10$. Inserta claves $\{10, 20, 30, 40, 50\}$
(todas con $h_2 = 0$). ¿Qué pasa?

<details><summary>Predice antes de ejecutar</summary>

¿Las inserciones entran en loop infinito? ¿Qué corrección se
necesita?

</details>

### Ejercicio 6 — Benchmark: 3 estrategias

Para $n = 10^5$ enteros y $\alpha = 0.5, 0.7$, mide el tiempo de
inserción + 10000 búsquedas con las 3 estrategias. ¿La localidad
de caché de la sonda lineal compensa su clustering?

<details><summary>Predice antes de ejecutar</summary>

¿A $\alpha = 0.5$, cuál será más rápida en tiempo real (no en
sondeos)? ¿Cambia la respuesta a $\alpha = 0.7$?

</details>

### Ejercicio 7 — Cuadrática: $\alpha$ máximo seguro

Con $m$ primo y $s_i = i^2$, determina experimentalmente el
$\alpha$ máximo al que se puede llenar la tabla sin que ninguna
inserción falle (en 100 experimentos con claves aleatorias).

<details><summary>Predice antes de ejecutar</summary>

¿Es ~0.5, ~0.7 o ~0.9? ¿Coincide con la garantía teórica de
$m/2$ slots visitados?

</details>

### Ejercicio 8 — Swiss Table simplificado

Investiga cómo funciona Swiss Table (usado por Rust HashMap):
control bytes de 7 bits, grupos de 16, SIMD matching. Implementa
una versión simplificada con un byte de metadata por slot (en vez
de 3 estados, usa el byte alto del hash para comparación rápida).

<details><summary>Predice antes de ejecutar</summary>

¿El byte de metadata reduce comparaciones de claves? ¿Por qué
es más eficiente que verificar la clave completa en cada sondeo?

</details>

### Ejercicio 9 — Costo de $h_2$

Mide el tiempo de calcular $h_1$ sola vs $h_1 + h_2$ para $10^6$
strings de longitud 10. ¿El costo extra de $h_2$ justifica la
mejor distribución?

<details><summary>Predice antes de ejecutar</summary>

¿$h_2$ duplica el tiempo de hashing? ¿El ahorro en sondeos
compensa?

</details>

### Ejercicio 10 — Híbrido: lineal con fallback cuadrático

Implementa una estrategia que use sonda lineal para los primeros
4 sondeos (aprovechando cache locality) y luego cambie a sonda
cuadrática. Compara con lineal puro y cuadrático puro.

<details><summary>Predice antes de ejecutar</summary>

¿El híbrido combina lo mejor de ambos (cache + dispersión)?
¿O agrega complejidad sin beneficio claro?

</details>
