# Factor de carga

## Objetivo

Comprender el **factor de carga** $\alpha = n/m$ como la métrica
central que gobierna el rendimiento de una tabla hash, y cómo
determina cuándo redimensionar:

- Definición y significado geométrico.
- Relación precisa entre $\alpha$ y el número esperado de
  comparaciones por búsqueda.
- Umbrales distintos para **chaining** ($\alpha$ puede superar 1)
  y **open addressing** ($\alpha < 1$ obligatorio).
- Impacto en memoria vs rendimiento.
- Cuándo y cómo redimensionar (grow/shrink).

---

## Definición

$$\alpha = \frac{n}{m}$$

donde:
- $n$ = número de elementos almacenados.
- $m$ = número de buckets (capacidad de la tabla).

### Significado

$\alpha$ es el **número promedio de elementos por bucket**. Con
$n = 700$ elementos y $m = 1000$ buckets: $\alpha = 0.7$, es decir,
en promedio cada bucket tiene 0.7 elementos.

- $\alpha = 0$: tabla vacía.
- $\alpha = 0.5$: la mitad de los buckets están ocupados
  (en promedio).
- $\alpha = 1.0$: tantos elementos como buckets.
- $\alpha > 1.0$: más elementos que buckets — solo posible con
  chaining (varios elementos por bucket).

---

## $\alpha$ y rendimiento: chaining

Con **encadenamiento separado** (cada bucket es una lista
enlazada), la búsqueda tiene dos fases:
1. Calcular $h(k)$ y acceder a `table[i]`: $O(1)$.
2. Recorrer la lista en `table[i]`: $O(\text{longitud de lista})$.

Si la función hash es uniforme, la longitud esperada de cada lista
es $\alpha$.

### Número esperado de comparaciones

| Tipo de búsqueda | Comparaciones esperadas | Fórmula |
|------------------|:--------:|:--------:|
| **Exitosa** (la clave existe) | $1 + \frac{\alpha}{2}$ | recorrer media lista |
| **Fallida** (la clave no existe) | $\alpha$ | recorrer lista completa |

Derivación de la búsqueda exitosa: un elemento insertado en
posición $i$ de la lista (con $i$ entre 0 y longitud-1) requiere
$i + 1$ comparaciones para encontrarlo. El promedio sobre todos
los elementos de la lista es $(1 + 2 + \cdots + L) / L
= (L + 1) / 2$, y $E[L] = \alpha$.

### Tabla numérica

| $\alpha$ | Comparaciones (éxito) | Comparaciones (fracaso) | Estado |
|---------:|:--------:|:--------:|--------|
| 0.25 | 1.125 | 0.25 | infrautilizada |
| 0.50 | 1.25 | 0.50 | eficiente |
| 0.75 | 1.375 | 0.75 | buen equilibrio |
| 1.00 | 1.50 | 1.00 | llena |
| 2.00 | 2.00 | 2.00 | sobrecargada |
| 5.00 | 3.50 | 5.00 | degradada |
| 10.0 | 6.00 | 10.0 | inaceptable |

Observación: con chaining, $\alpha = 2$ aún es razonable (2
comparaciones promedio por búsqueda). $\alpha = 5$ ya empieza a
parecer búsqueda lineal en listas cortas.

---

## $\alpha$ y rendimiento: open addressing

Con **open addressing** (probing), todos los elementos se
almacenan directamente en el array. No hay listas — si un bucket
está ocupado, se busca el siguiente disponible según una secuencia
de sondeo.

Consecuencia inmediata: **$\alpha$ no puede superar 1**. Si
$n = m$, la tabla está completamente llena y no se puede insertar.

### Número esperado de sondeos (hash uniforme)

| Tipo de búsqueda | Sondeos esperados | Fórmula |
|------------------|:--------:|:--------:|
| **Exitosa** | $\frac{1}{\alpha} \ln \frac{1}{1 - \alpha}$ | armónica |
| **Fallida** | $\frac{1}{1 - \alpha}$ | geométrica |

Estas fórmulas asumen **hashing uniforme** (cada sondeo prueba un
bucket independiente). En la práctica, la sonda lineal tiene
**clustering primario** que empeora las cosas.

### Tabla numérica

| $\alpha$ | Sondeos éxito | Sondeos fracaso | Estado |
|---------:|:--------:|:--------:|--------|
| 0.25 | 1.15 | 1.33 | infrautilizada |
| 0.50 | 1.39 | 2.00 | eficiente |
| 0.70 | 1.72 | 3.33 | buen equilibrio |
| 0.80 | 2.01 | 5.00 | cargada |
| 0.90 | 2.56 | 10.0 | pesada |
| 0.95 | 3.15 | 20.0 | crítica |
| 0.99 | 4.65 | 100.0 | inaceptable |

La degradación es **no lineal**: pasar de $\alpha = 0.7$ a
$\alpha = 0.9$ triplica los sondeos de búsqueda fallida (de 3.3
a 10). Pasar de 0.9 a 0.99 lo lleva a 100.

### Con sonda lineal (peor que uniforme)

La sonda lineal sufre **clustering primario**: las cadenas de
buckets ocupados crecen y se fusionan, creando clusters que
empeoran las búsquedas. Los sondeos esperados reales con sonda
lineal son:

| $\alpha$ | Sondeos éxito (lineal) | Sondeos fracaso (lineal) |
|---------:|:--------:|:--------:|
| 0.50 | 1.50 | 2.50 |
| 0.70 | 2.17 | 6.06 |
| 0.80 | 3.00 | 13.0 |
| 0.90 | 5.50 | 50.5 |
| 0.95 | 10.5 | 200.5 |

Fórmulas de Knuth para sonda lineal:

$$\text{Éxito} \approx \frac{1}{2}\left(1 + \frac{1}{(1-\alpha)^2}\right)$$

$$\text{Fracaso} \approx \frac{1}{2}\left(1 + \frac{1}{(1-\alpha)^2}\right)$$

La primera es una simplificación; la fórmula exacta es más
compleja. Lo importante: con $\alpha = 0.9$ y sonda lineal, una
búsqueda fallida puede probar 50 buckets — esto ya no es $O(1)$.

---

## Chaining vs open addressing: umbrales

| Aspecto | Chaining | Open addressing |
|---------|:--------:|:---------------:|
| $\alpha$ posible | $[0, \infty)$ | $[0, 1)$ |
| $\alpha$ típico | 0.75 – 2.0 | 0.5 – 0.75 |
| Umbral de resize | $\alpha > 1.0$ – 2.0 | $\alpha > 0.7$ – 0.75 |
| Memoria extra | punteros de lista por nodo | ninguna |
| Cache locality | mala (nodos dispersos) | buena (array contiguo) |
| Peor caso real | listas largas pero funcional | clustering catastrófico |

### ¿Por qué el umbral de open addressing es tan bajo?

Porque la degradación es **exponencial**, no lineal. La diferencia
entre $\alpha = 0.7$ (3.3 sondeos fracaso) y $\alpha = 0.9$
(10 sondeos) no es un 29% de incremento en $\alpha$ sino un
**200% de incremento en sondeos**.

### Umbrales en implementaciones reales

| Implementación | Tipo | $\alpha$ máximo | $\alpha$ tras resize |
|---------------|------|----------:|----------:|
| Java HashMap | chaining | 0.75 | 0.375 (×2) |
| Python dict | open (probing) | 0.667 | 0.333 (×2) |
| Rust HashMap | open (Swiss Table) | 0.875 | ~0.44 (×2) |
| Go map | chaining (buckets de 8) | 6.5 (por bucket) | ~3.25 |
| C++ unordered_map | chaining | 1.0 | 0.5 (×2) |
| Redis dict | chaining | 1.0 | 0.5 (×2) |

Go es peculiar: usa "buckets de 8" (cada bucket almacena hasta 8
pares inline antes de overflow), así que $\alpha = 6.5$ significa
que cada bucket tiene ~6.5 elementos, pero 8 caben sin overflow.

Rust usa un umbral alto (0.875) porque Swiss Table tiene una
estrategia de probing muy eficiente con SIMD.

---

## Visualización del efecto de $\alpha$

### $\alpha = 0.3$ (muy dispersa)

```
Tabla m=10, n=3:
  [0]: ■
  [1]:
  [2]:
  [3]:
  [4]: ■
  [5]:
  [6]:
  [7]:
  [8]: ■
  [9]:

Comparaciones promedio: ~1.0 (casi siempre un solo acceso)
Memoria desperdiciada: 70% de los buckets vacios
```

### $\alpha = 0.7$ (equilibrada)

```
Tabla m=10, n=7:
  [0]: ■
  [1]: ■ ■     (colision)
  [2]:
  [3]: ■
  [4]: ■
  [5]:
  [6]: ■
  [7]:
  [8]: ■
  [9]:

Comparaciones promedio: ~1.35 (pocas colisiones)
Memoria desperdiciada: 30%
```

### $\alpha = 2.0$ (sobrecargada, solo chaining)

```
Tabla m=10, n=20:
  [0]: ■ ■
  [1]: ■ ■ ■
  [2]: ■ ■
  [3]: ■
  [4]: ■ ■ ■ ■
  [5]: ■ ■
  [6]: ■
  [7]: ■ ■
  [8]: ■ ■
  [9]: ■

Comparaciones promedio: ~2.0 (recorrer listas de 2)
Sin memoria desperdiciada, pero mas lenta
```

---

## Trade-off memoria vs rendimiento

$\alpha$ bajo (tabla grande) = rápido pero desperdicia memoria.
$\alpha$ alto (tabla pequeña) = lento pero eficiente en memoria.

```
rendimiento
  (sondeos)
     |
  10 |                              /
     |                           /
   5 |                        /
     |                     /
   3 |                  .
   2 |              .
   1 |  . . . . .
     +-----+-----+-----+-----+--→ alpha
     0    0.25  0.50  0.75  1.0
```

Para la mayoría de las aplicaciones, $\alpha \in [0.5, 0.75]$ es
el punto óptimo: búsqueda rápida (~1.5 sondeos) con uso razonable
de memoria (~50-75% de ocupación).

---

## Cuándo redimensionar

### Crecer (grow)

Cuando $\alpha$ supera el umbral máximo:

```
Chaining:  si alpha > 1.0 (o 0.75 para ser conservador)
Open addr: si alpha > 0.7 (o 0.75 dependiendo del scheme)
```

Se duplica el tamaño de la tabla ($m' = 2m$) y se reinsertan
todos los elementos (**rehash**). El costo del rehash es $O(n)$,
pero se amortiza sobre las $n/2$ inserciones previas: costo
**amortizado** $O(1)$ por inserción. El redimensionamiento se cubre
en detalle en S02/T05.

### Encoger (shrink)

Cuando $\alpha$ cae por debajo de un umbral mínimo (e.g.,
$\alpha < 0.1$ o $\alpha < 0.25$), se puede reducir el tamaño
para liberar memoria. Menos común — muchas implementaciones
(incluyendo Rust `HashMap`) no encogen automáticamente.

¿Por qué no encoger siempre? Porque el rehash tiene costo $O(n)$,
y si los elementos se re-insertan pronto, se desperdicia trabajo.
Solo vale la pena si la tabla permanecerá pequeña por un tiempo.

### Factor de crecimiento

$\times 2$ es lo más común (simple, eficiente en potencias de 2).
Algunos sistemas usan $\times 1.5$ para menor pico de memoria.

| Factor | Ventaja | Desventaja |
|--------|---------|------------|
| $\times 2$ | simple, $m$ se mantiene potencia de 2 | pico de memoria alto ($3m$ temporalmente) |
| $\times 1.5$ | menor pico de memoria | $m$ no es potencia de 2 |
| $\times 4$ | menos resizes | desperdicio grande tras resize |

---

## Programa completo en C

```c
// load_factor.c — load factor effects on hash table performance
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// --- chaining hash table ---

typedef struct Entry {
    int key;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    int m;       // capacity
    int n;       // count
} ChainTable;

ChainTable *chain_create(int m) {
    ChainTable *t = malloc(sizeof(ChainTable));
    t->m = m;
    t->n = 0;
    t->buckets = calloc(m, sizeof(Entry *));
    return t;
}

void chain_insert(ChainTable *t, int key) {
    int i = ((key % t->m) + t->m) % t->m;
    Entry *e = malloc(sizeof(Entry));
    e->key = key;
    e->next = t->buckets[i];
    t->buckets[i] = e;
    t->n++;
}

// search returning number of comparisons
int chain_search(ChainTable *t, int key, int *comps) {
    int i = ((key % t->m) + t->m) % t->m;
    *comps = 0;
    for (Entry *e = t->buckets[i]; e; e = e->next) {
        (*comps)++;
        if (e->key == key) return 1;
    }
    return 0;
}

void chain_free(ChainTable *t) {
    for (int i = 0; i < t->m; i++) {
        Entry *e = t->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e);
            e = next;
        }
    }
    free(t->buckets);
    free(t);
}

// --- open addressing (linear probing) ---

typedef struct {
    int *keys;
    int *occupied;
    int m;
    int n;
} OpenTable;

OpenTable *open_create(int m) {
    OpenTable *t = malloc(sizeof(OpenTable));
    t->m = m;
    t->n = 0;
    t->keys = calloc(m, sizeof(int));
    t->occupied = calloc(m, sizeof(int));
    return t;
}

int open_insert(OpenTable *t, int key) {
    if (t->n >= t->m) return 0;  // full
    int i = ((key % t->m) + t->m) % t->m;
    int probes = 0;
    while (t->occupied[i]) {
        if (t->keys[i] == key) return 1;  // already exists
        i = (i + 1) % t->m;
        probes++;
    }
    t->keys[i] = key;
    t->occupied[i] = 1;
    t->n++;
    return 1;
}

int open_search(OpenTable *t, int key, int *probes) {
    int i = ((key % t->m) + t->m) % t->m;
    *probes = 1;
    while (t->occupied[i]) {
        if (t->keys[i] == key) return 1;
        i = (i + 1) % t->m;
        (*probes)++;
        if (*probes > t->m) return 0;
    }
    return 0;
}

void open_free(OpenTable *t) {
    free(t->keys);
    free(t->occupied);
    free(t);
}

// --- demos ---

int main(void) {
    srand(42);

    // demo 1: chaining — alpha vs comparisons
    printf("=== Demo 1: chaining — alpha vs comparisons ===\n");
    printf("%6s  %8s  %8s  %8s  %8s\n",
           "alpha", "n", "m", "avg_hit", "avg_miss");

    int m_chain = 1000;
    for (int target_n = 250; target_n <= 5000; target_n += 250) {
        ChainTable *t = chain_create(m_chain);

        // insert target_n random keys
        for (int i = 0; i < target_n; i++)
            chain_insert(t, rand() % 1000000);

        // measure hit (search for inserted keys)
        double total_hit = 0;
        int hits = 0;
        Entry **sample = t->buckets;
        for (int i = 0; i < m_chain && hits < 500; i++) {
            for (Entry *e = sample[i]; e && hits < 500; e = e->next) {
                int c;
                chain_search(t, e->key, &c);
                total_hit += c;
                hits++;
            }
        }

        // measure miss (search for non-existent keys)
        double total_miss = 0;
        for (int i = 0; i < 500; i++) {
            int c;
            chain_search(t, -(i + 1), &c);  // negative keys never inserted
            total_miss += c;
        }

        printf("%6.2f  %8d  %8d  %8.2f  %8.2f\n",
               (double)t->n / t->m, t->n, t->m,
               total_hit / hits, total_miss / 500);

        chain_free(t);
    }
    printf("\n");

    // demo 2: open addressing — alpha vs probes
    printf("=== Demo 2: open addressing (linear) — alpha vs probes ===\n");
    printf("%6s  %8s  %8s  %8s  %8s  %8s  %8s\n",
           "alpha", "n", "m", "avg_hit", "avg_miss",
           "theory_h", "theory_m");

    int m_open = 10000;
    double alphas[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95};

    for (int a = 0; a < 10; a++) {
        int target_n = (int)(alphas[a] * m_open);
        OpenTable *t = open_create(m_open);

        // insert using random keys
        int inserted = 0;
        while (inserted < target_n) {
            if (open_insert(t, rand() % 10000000))
                inserted++;
        }

        // measure hit
        double total_hit = 0;
        int hits = 0;
        for (int i = 0; i < m_open && hits < 500; i++) {
            if (t->occupied[i]) {
                int p;
                open_search(t, t->keys[i], &p);
                total_hit += p;
                hits++;
            }
        }

        // measure miss
        double total_miss = 0;
        for (int i = 0; i < 500; i++) {
            int p;
            open_search(t, -(i + 1), &p);
            total_miss += p;
        }

        double al = alphas[a];
        double theory_hit = 0.5 * (1 + 1.0 / ((1 - al) * (1 - al)));
        double theory_miss = 0.5 * (1 + 1.0 / ((1 - al) * (1 - al)));
        // Knuth's formula for linear probing (approx)

        printf("%6.2f  %8d  %8d  %8.2f  %8.2f  %8.2f  %8.2f\n",
               al, t->n, t->m,
               total_hit / hits, total_miss / 500,
               theory_hit, theory_miss);

        open_free(t);
    }
    printf("\n");

    // demo 3: visualize bucket distribution
    printf("=== Demo 3: bucket distribution at different alpha ===\n");

    for (double target_alpha = 0.3; target_alpha <= 2.1; target_alpha += 0.7) {
        int small_m = 20;
        int small_n = (int)(target_alpha * small_m);
        ChainTable *t = chain_create(small_m);
        for (int i = 0; i < small_n; i++)
            chain_insert(t, rand() % 10000);

        printf("alpha = %.1f (n=%d, m=%d):\n", target_alpha, small_n, small_m);
        for (int i = 0; i < small_m; i++) {
            printf("  [%2d]: ", i);
            int count = 0;
            for (Entry *e = t->buckets[i]; e; e = e->next) {
                printf("#");
                count++;
            }
            if (count == 0) printf("-");
            printf("\n");
        }
        printf("\n");
        chain_free(t);
    }

    // demo 4: open addressing degradation
    printf("=== Demo 4: open addressing — fill to 99%% ===\n");
    int small_open = 100;
    OpenTable *ot = open_create(small_open);

    int milestones[] = {50, 70, 80, 90, 95, 99};
    int mi = 0;
    for (int i = 0; i < 99; i++) {
        open_insert(ot, rand() % 1000000);

        if (mi < 6 && ot->n == milestones[mi]) {
            // measure miss
            double total = 0;
            for (int j = 0; j < 100; j++) {
                int p;
                open_search(ot, -(j + 1), &p);
                total += p;
            }
            printf("  alpha=%.2f: avg miss probes = %.1f\n",
                   (double)ot->n / ot->m, total / 100);
            mi++;
        }
    }
    printf("  (notice exponential growth past alpha=0.8)\n\n");

    // demo 5: max chain length
    printf("=== Demo 5: max chain length vs alpha ===\n");
    printf("%6s  %8s  %8s\n", "alpha", "avg_len", "max_len");

    for (double al = 0.5; al <= 5.0; al += 0.5) {
        int m5 = 1000;
        int n5 = (int)(al * m5);
        ChainTable *t = chain_create(m5);
        for (int i = 0; i < n5; i++)
            chain_insert(t, rand() % 10000000);

        int max_len = 0;
        for (int i = 0; i < m5; i++) {
            int len = 0;
            for (Entry *e = t->buckets[i]; e; e = e->next) len++;
            if (len > max_len) max_len = len;
        }

        printf("%6.1f  %8.2f  %8d\n", al, al, max_len);
        chain_free(t);
    }

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o load_factor load_factor.c -lm
./load_factor
```

---

## Programa completo en Rust

```rust
// load_factor.rs — load factor effects and HashMap behavior
use std::collections::HashMap;

fn main() {
    // demo 1: HashMap capacity and load factor
    println!("=== Demo 1: HashMap auto-resizing ===");
    let mut map: HashMap<i32, i32> = HashMap::new();

    println!("{:>6} {:>10} {:>8}", "n", "capacity", "alpha");
    for i in 0..100 {
        map.insert(i, i * 10);
        if (i + 1) % 10 == 0 || i < 5 {
            let cap = map.capacity();
            let alpha = map.len() as f64 / cap as f64;
            println!("{:>6} {:>10} {:>8.3}", map.len(), cap, alpha);
        }
    }
    println!("Note: HashMap grows automatically to keep alpha low\n");

    // demo 2: with_capacity pre-allocation
    println!("=== Demo 2: with_capacity ===");
    let mut pre: HashMap<i32, i32> = HashMap::with_capacity(1000);
    println!("with_capacity(1000): actual capacity = {}", pre.capacity());
    for i in 0..1000 {
        pre.insert(i, i);
    }
    println!("after 1000 inserts:  capacity = {}", pre.capacity());
    println!("  (no resize if pre-allocated correctly)\n");

    // demo 3: shrink_to_fit
    println!("=== Demo 3: shrink_to_fit ===");
    let mut shrinkable: HashMap<i32, i32> = HashMap::new();
    for i in 0..1000 {
        shrinkable.insert(i, i);
    }
    println!("after 1000 inserts: len={}, cap={}",
             shrinkable.len(), shrinkable.capacity());

    // remove most elements
    for i in 0..900 {
        shrinkable.remove(&i);
    }
    println!("after removing 900:  len={}, cap={} (unchanged!)",
             shrinkable.len(), shrinkable.capacity());

    shrinkable.shrink_to_fit();
    println!("after shrink_to_fit: len={}, cap={}",
             shrinkable.len(), shrinkable.capacity());
    println!();

    // demo 4: simulated chaining — alpha vs comparisons
    println!("=== Demo 4: simulated chaining — alpha vs comparisons ===");
    println!("{:>6} {:>8} {:>10} {:>10} {:>10} {:>10}",
             "alpha", "n", "avg_hit", "avg_miss", "th_hit", "th_miss");

    let m = 1000usize;
    for &n in &[250, 500, 750, 1000, 1500, 2000, 3000, 5000] {
        let mut buckets: Vec<Vec<i32>> = vec![vec![]; m];
        let mut rng = n as u64 * 2654435769;

        for _ in 0..n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            let key = (rng >> 16) as i32;
            let idx = (key.unsigned_abs() as usize) % m;
            buckets[idx].push(key);
        }

        // measure hit: search for existing keys
        let mut total_hit = 0u64;
        let mut hits = 0;
        for bucket in &buckets {
            for (pos, &key) in bucket.iter().enumerate() {
                let idx = (key.unsigned_abs() as usize) % m;
                // comparisons = position in chain + 1
                let comps = buckets[idx].iter()
                    .position(|&k| k == key)
                    .map(|p| p + 1)
                    .unwrap_or(bucket.len());
                total_hit += comps as u64;
                hits += 1;
                if hits >= 500 { break; }
            }
            if hits >= 500 { break; }
        }

        // measure miss
        let mut total_miss = 0u64;
        for i in 0..500 {
            let key = -(i + 1);
            let idx = (key.unsigned_abs() as usize) % m;
            total_miss += buckets[idx].len() as u64;
        }

        let alpha = n as f64 / m as f64;
        let th_hit = 1.0 + alpha / 2.0;
        let th_miss = alpha;

        println!("{:>6.2} {:>8} {:>10.2} {:>10.2} {:>10.2} {:>10.2}",
                 alpha, n,
                 total_hit as f64 / hits as f64,
                 total_miss as f64 / 500.0,
                 th_hit, th_miss);
    }
    println!();

    // demo 5: simulated open addressing — alpha vs probes
    println!("=== Demo 5: simulated open addressing (linear) ===");
    println!("{:>6} {:>8} {:>10} {:>10}",
             "alpha", "n", "avg_hit", "avg_miss");

    let m_open = 10000usize;
    for &alpha_target in &[0.1, 0.3, 0.5, 0.7, 0.8, 0.9, 0.95] {
        let n = (alpha_target * m_open as f64) as usize;
        let mut table: Vec<Option<i32>> = vec![None; m_open];
        let mut rng = (n as u64).wrapping_mul(2654435769);

        let mut inserted = 0;
        while inserted < n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            let key = (rng >> 16) as i32;
            let mut idx = (key.unsigned_abs() as usize) % m_open;
            loop {
                match table[idx] {
                    None => {
                        table[idx] = Some(key);
                        inserted += 1;
                        break;
                    }
                    Some(k) if k == key => break,
                    _ => idx = (idx + 1) % m_open,
                }
            }
        }

        // measure hit
        let mut total_hit = 0u64;
        let mut hits = 0;
        for i in 0..m_open {
            if let Some(key) = table[i] {
                let mut idx = (key.unsigned_abs() as usize) % m_open;
                let mut probes = 1u64;
                while table[idx] != Some(key) {
                    idx = (idx + 1) % m_open;
                    probes += 1;
                }
                total_hit += probes;
                hits += 1;
                if hits >= 500 { break; }
            }
        }

        // measure miss
        let mut total_miss = 0u64;
        for i in 0..500i32 {
            let key = -(i + 1);
            let mut idx = (key.unsigned_abs() as usize) % m_open;
            let mut probes = 1u64;
            while table[idx].is_some() {
                idx = (idx + 1) % m_open;
                probes += 1;
            }
            total_miss += probes;
        }

        println!("{:>6.2} {:>8} {:>10.2} {:>10.2}",
                 alpha_target, n,
                 total_hit as f64 / hits as f64,
                 total_miss as f64 / 500.0);
    }
    println!("  (notice exponential growth past alpha=0.8)\n");

    // demo 6: memory trade-off
    println!("=== Demo 6: memory vs speed trade-off ===");
    println!("{:>6} {:>12} {:>12}", "alpha", "memory(KB)", "probes_miss");
    let n_elements = 10000;
    let entry_size = 16; // bytes per entry (key + value)
    for &alpha in &[0.25, 0.50, 0.75, 1.0, 2.0, 5.0] {
        let m_needed = (n_elements as f64 / alpha) as usize;
        let memory_kb = (m_needed * 8 + n_elements * entry_size) / 1024; // pointers + entries
        let probes = if alpha <= 1.0 {
            // open addressing approximation
            1.0 / (1.0 - alpha.min(0.99))
        } else {
            alpha // chaining
        };
        println!("{:>6.2} {:>12} {:>12.1}", alpha, memory_kb, probes);
    }
    println!("  alpha=0.5: fast but ~2x memory");
    println!("  alpha=2.0: compact but ~2x slower");
}
```

Compilar y ejecutar:

```bash
rustc load_factor.rs && ./load_factor
```

---

## Errores frecuentes

1. **No redimensionar**: insertar sin verificar $\alpha$ hace que
   la tabla degrade a $O(n)$ gradualmente. El programa no falla —
   simplemente se vuelve lento sin aviso.
2. **Umbral de open addressing demasiado alto**: usar $\alpha > 0.9$
   con sonda lineal es catastrófico (50+ sondeos por miss). Mantener
   $\alpha \leq 0.75$.
3. **Confundir $n$ con $m$**: $n$ es el número de elementos
   (crece con cada inserción), $m$ es la capacidad (fija hasta el
   resize). $\alpha = n/m$, no $m/n$.
4. **No pre-alocar con `with_capacity`**: si se conoce el número
   aproximado de elementos, pre-alocar evita múltiples resizes
   costosos.
5. **Asumir que HashMap no encoge automáticamente**: en Rust,
   `HashMap` no reduce su capacidad al eliminar elementos. Hay que
   llamar `shrink_to_fit()` explícitamente si se quiere recuperar
   memoria.

---

## Ejercicios

### Ejercicio 1 — Calcular $\alpha$

Una tabla hash tiene $m = 127$ buckets y contiene 89 elementos.
¿Cuál es $\alpha$? ¿Cuántas comparaciones promedio esperarías para
búsqueda exitosa y fallida con chaining?

<details><summary>Predice antes de ejecutar</summary>

¿$\alpha$ está por debajo o por encima de 0.75? ¿Debería
redimensionar?

</details>

### Ejercicio 2 — Open addressing: punto de quiebre

Implementa una tabla hash con sonda lineal de $m = 1000$. Inserta
claves de 1 en 1 y mide el promedio de sondeos de búsqueda fallida
cada 50 inserciones. Grafica (o imprime) sondeos vs $\alpha$.
¿En qué $\alpha$ los sondeos superan 5? ¿Y 10?

<details><summary>Predice antes de ejecutar</summary>

Con la fórmula $1/(1-\alpha)^2$ / 2, ¿a qué $\alpha$ se llega a
5 sondeos? ¿Coincide con tu medición empírica?

</details>

### Ejercicio 3 — Chaining vs open: $\alpha = 0.9$

Para $n = 9000$ y $m = 10000$ ($\alpha = 0.9$), inserta las mismas
claves en una tabla con chaining y otra con sonda lineal. Mide el
promedio de comparaciones/sondeos para 1000 búsquedas exitosas y
1000 fallidas.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas veces más lenta es la sonda lineal que chaining a
$\alpha = 0.9$? ¿La diferencia es 2x, 5x o 10x?

</details>

### Ejercicio 4 — Crecimiento amortizado

Implementa una tabla hash que empiece con $m = 4$ y duplique cuando
$\alpha > 0.75$. Inserta 10000 elementos y cuenta: (a) número total
de resizes, (b) número total de reinserciones por resize,
(c) costo amortizado por inserción.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos resizes ocurrirán para llegar de $m=4$ a $m$ suficiente
para 10000 elementos? ¿El costo total de reinserciones es $O(n)$
o $O(n \log n)$?

</details>

### Ejercicio 5 — HashMap capacity en Rust

Crea un `HashMap<i32, i32>` vacío e inserta 1, 2, 4, 8, ..., 1024
elementos. Después de cada potencia de 2, imprime `len()`,
`capacity()` y $\alpha$. ¿En qué puntos ocurre un resize?

<details><summary>Predice antes de ejecutar</summary>

¿`capacity()` siempre es potencia de 2? ¿`HashMap` usa $\alpha$
máximo de 0.75 o algún otro valor?

</details>

### Ejercicio 6 — Longitud máxima de cadena

Para una tabla de chaining con $m = 1000$ y $\alpha = 1.0$ (1000
claves), calcula teóricamente y mide empíricamente la longitud
máxima de cadena esperada. Compara con $\alpha = 0.5$ y
$\alpha = 2.0$.

<details><summary>Predice antes de ejecutar</summary>

La longitud máxima esperada es $\Theta(\log n / \log \log n)$ con
hash uniforme. Para $n = 1000$, ¿es ~3, ~5, ~7 o ~10?

</details>

### Ejercicio 7 — Pre-allocate benchmark

Mide el tiempo de insertar $10^6$ pares en un `HashMap::new()` vs
`HashMap::with_capacity(1_000_000)`. ¿Cuánto más rápido es el
pre-alocado?

<details><summary>Predice antes de ejecutar</summary>

¿El pre-alocado será 1.1x, 1.5x o 2x más rápido? ¿Cuántos
resizes evita?

</details>

### Ejercicio 8 — shrink_to_fit

Crea un `HashMap` con 10000 elementos, elimina 9900, y mide la
memoria (via `capacity()`) antes y después de `shrink_to_fit()`.
¿Cuánta memoria se recupera?

<details><summary>Predice antes de ejecutar</summary>

¿`shrink_to_fit` reduce la capacidad a exactamente 100, o deja
algo de margen? ¿Qué $\alpha$ queda después del shrink?

</details>

### Ejercicio 9 — $\alpha$ óptimo empírico

Para sonda lineal con $m = 10000$, mide el throughput total
(inserciones + búsquedas por segundo) para $\alpha$ fijo entre
0.3 y 0.95. ¿Cuál $\alpha$ maximiza el throughput considerando
que menor $\alpha$ desperdicia memoria (y por tanto cache)?

<details><summary>Predice antes de ejecutar</summary>

¿El $\alpha$ óptimo será ~0.5, ~0.7 o ~0.8? ¿Depende de si
domina la inserción o la búsqueda?

</details>

### Ejercicio 10 — Tabla comparativa final

Para $n = 10000$ claves enteras aleatorias, implementa y compara:
- Chaining con $\alpha = 0.75$, 1.0, 2.0.
- Sonda lineal con $\alpha = 0.5$, 0.7, 0.9.

Mide: tiempo de inserción total, promedio de sondeos/comparaciones
por búsqueda (exitosa y fallida), memoria usada.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál combinación (tipo + $\alpha$) será la más rápida en
búsqueda? ¿Y la más eficiente en memoria? ¿Hay un ganador claro?

</details>
