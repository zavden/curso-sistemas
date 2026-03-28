# Counting sort

## Objetivo

Dominar counting sort como el primer algoritmo que rompe la barrera
$\Omega(n \log n)$ de los comparison sorts:

- No compara elementos entre sí — usa **indexación directa**.
- Requiere que los valores estén en un rango conocido $[0, k]$.
- Complejidad $O(n + k)$: lineal cuando $k = O(n)$.
- **Estable**: preserva el orden relativo de elementos iguales.
- Base fundamental de radix sort.

---

## Por qué es posible romper $\Omega(n \log n)$

El límite inferior $\Omega(n \log n)$ aplica solo a **comparison sorts**:
algoritmos que obtienen información sobre el orden únicamente comparando
pares de elementos. Cada comparación da 1 bit de información, y
necesitamos $\log_2(n!)$ bits para distinguir entre las $n!$
permutaciones posibles.

Counting sort no compara elementos. En su lugar, **cuenta** cuántas
veces aparece cada valor y usa esa información para colocar cada
elemento directamente en su posición final. Cada acceso a la tabla
de conteo da $\log_2(k)$ bits de información (el valor del elemento),
lo que permite romper la barrera logarítmica.

El costo es que necesitamos **saber el rango** de valores de antemano,
y el espacio extra es $O(k)$, que puede ser prohibitivo si $k$ es
grande.

---

## El algoritmo

### Versión simple (solo valores, sin estabilidad)

Para un array de enteros en $[0, k]$:

1. Crear un array `count[0..k]` inicializado a 0.
2. Recorrer el input: para cada `arr[i]`, incrementar `count[arr[i]]`.
3. Recorrer `count`: para cada valor $v$ con `count[v] > 0`, escribir
   $v$ en el output `count[v]` veces.

```
COUNTING_SORT_SIMPLE(arr, n, k):
    count[0..k] = {0}
    para i de 0 a n-1:
        count[arr[i]]++
    pos = 0
    para v de 0 a k:
        mientras count[v] > 0:
            arr[pos++] = v
            count[v]--
```

Esto funciona pero **no es estable** y **no funciona con datos
satelitales** (si los elementos son structs con una clave y otros
campos, la versión simple pierde los campos asociados).

### Versión estable (con datos satelitales)

La versión estable usa un paso adicional: convertir los conteos en
**prefijos acumulados** (prefix sum), que dan la posición final de
cada grupo de valores.

```
COUNTING_SORT_STABLE(arr, n, k):
    // Paso 1: contar ocurrencias
    count[0..k] = {0}
    para i de 0 a n-1:
        count[arr[i].key]++

    // Paso 2: prefix sum — count[v] = posición de inicio del grupo v+1
    para v de 1 a k:
        count[v] += count[v-1]

    // Paso 3: colocar elementos en output (recorrer de derecha a izquierda)
    output[0..n-1]
    para i de n-1 hasta 0:
        v = arr[i].key
        count[v]--
        output[count[v]] = arr[i]

    // Paso 4: copiar output a arr
    copiar output a arr
```

El recorrido de derecha a izquierda en el paso 3 garantiza
**estabilidad**: el último elemento con valor $v$ en el input se
coloca en la última posición del grupo $v$ en el output, preservando
el orden relativo.

---

## Traza detallada

Array: `[4, 2, 2, 8, 3, 3, 1]`, rango $k = 8$.

### Paso 1: contar

```
Recorrer: 4, 2, 2, 8, 3, 3, 1

count: [0, 1, 2, 2, 1, 0, 0, 0, 1]
        0  1  2  3  4  5  6  7  8
        ↑     ↑  ↑  ↑           ↑
        0×   1× 2× 2× 1×       1×
```

### Paso 2: prefix sum

```
count[0] = 0                        → 0 elementos antes del grupo 0
count[1] = 0 + 1 = 1               → 1 elemento antes del grupo 1
count[2] = 1 + 2 = 3               → 3 elementos antes del grupo 2
count[3] = 3 + 2 = 5               → 5 elementos antes del grupo 3
count[4] = 5 + 1 = 6               → 6 elementos antes del grupo 4
count[5] = 6 + 0 = 6
count[6] = 6 + 0 = 6
count[7] = 6 + 0 = 6
count[8] = 6 + 1 = 7               → 7 elementos en total

count: [0, 1, 3, 5, 6, 6, 6, 6, 7]
```

Después del prefix sum, `count[v]` indica cuántos elementos tienen
valor $\leq v$, o equivalentemente, la posición **después** del último
elemento del grupo $v$.

### Paso 3: colocar (derecha a izquierda)

```
i=6: arr[6]=1 → count[1]=1, count[1]-- → 0, output[0] = 1
i=5: arr[5]=3 → count[3]=5, count[3]-- → 4, output[4] = 3
i=4: arr[4]=3 → count[3]=4, count[3]-- → 3, output[3] = 3
i=3: arr[3]=8 → count[8]=7, count[8]-- → 6, output[6] = 8
i=2: arr[2]=2 → count[2]=3, count[2]-- → 2, output[2] = 2
i=1: arr[1]=2 → count[2]=2, count[2]-- → 1, output[1] = 2
i=0: arr[0]=4 → count[4]=6, count[4]-- → 5, output[5] = 4

Output: [1, 2, 2, 3, 3, 4, 8]
```

### Estabilidad verificada

Si los dos 2's fueran `(2,'a')` en posición 1 y `(2,'b')` en posición
2, el recorrido de derecha a izquierda procesa primero `(2,'b')` (i=2)
y lo coloca en output[2], luego `(2,'a')` (i=1) y lo coloca en
output[1]. Resultado: `(2,'a')` antes de `(2,'b')` — orden original
preservado.

---

## Análisis de complejidad

| Paso | Operaciones | Accesos a memoria |
|------|------------|-------------------|
| Inicializar `count` | $O(k)$ | $k$ escrituras |
| Contar | $O(n)$ | $n$ lecturas + $n$ read-modify-write |
| Prefix sum | $O(k)$ | $k$ read-modify-write |
| Colocar | $O(n)$ | $n$ lecturas + $n$ escrituras |
| Copiar | $O(n)$ | $n$ lecturas + $n$ escrituras |

**Total**: $O(n + k)$ tiempo, $O(n + k)$ espacio extra.

### Cuándo es eficiente

- $k = O(n)$: counting sort es $O(n)$ — lineal, óptimo.
- $k = O(n^2)$: counting sort es $O(n^2)$ — peor que quicksort.
- $k = O(2^{32})$: counting sort necesita 4 GB de espacio — inviable.

**Regla práctica**: counting sort vale la pena cuando $k \leq c \cdot n$
para una constante $c$ razonable (típicamente $c \leq 100$).

### Comparación con comparison sorts

| Algoritmo | Tiempo | Espacio extra | Restricción |
|-----------|--------|--------------|-------------|
| Merge sort | $O(n \log n)$ | $O(n)$ | Ninguna |
| Quicksort | $O(n \log n)$ promedio | $O(\log n)$ | Ninguna |
| Counting sort | $O(n + k)$ | $O(n + k)$ | Rango $[0, k]$ conocido |

Para $k = 100$ y $n = 10^6$: counting sort hace $\sim 10^6$
operaciones vs quicksort $\sim 2 \times 10^7$ — una diferencia de 20x.

---

## Manejo de rangos negativos

Si los valores están en $[\text{min}, \text{max}]$ con $\text{min} < 0$,
se desplaza el rango:

```c
int offset = -min_val;
int k = max_val - min_val;
/* Usar arr[i] + offset como índice en count */
count[arr[i] + offset]++;
```

Esto permite ordenar cualquier rango $[\text{min}, \text{max}]$ con
$k = \text{max} - \text{min}$.

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Counting sort (simple, in-place, not for satellites) ── */

void counting_sort_simple(int *arr, int n, int k) {
    int *count = calloc(k + 1, sizeof(int));
    for (int i = 0; i < n; i++)
        count[arr[i]]++;
    int pos = 0;
    for (int v = 0; v <= k; v++)
        for (int c = 0; c < count[v]; c++)
            arr[pos++] = v;
    free(count);
}

/* ── Counting sort (stable, with output buffer) ──────────── */

void counting_sort_stable(int *arr, int n, int k) {
    int *count  = calloc(k + 1, sizeof(int));
    int *output = malloc(n * sizeof(int));

    /* Count */
    for (int i = 0; i < n; i++)
        count[arr[i]]++;

    /* Prefix sum */
    for (int v = 1; v <= k; v++)
        count[v] += count[v - 1];

    /* Place (right to left for stability) */
    for (int i = n - 1; i >= 0; i--) {
        count[arr[i]]--;
        output[count[arr[i]]] = arr[i];
    }

    memcpy(arr, output, n * sizeof(int));
    free(count);
    free(output);
}

/* ── Counting sort for structs (stable, by key) ──────────── */

typedef struct {
    int key;
    char tag;
} Record;

void counting_sort_records(Record *arr, int n, int k) {
    int *count     = calloc(k + 1, sizeof(int));
    Record *output = malloc(n * sizeof(Record));

    for (int i = 0; i < n; i++)
        count[arr[i].key]++;

    for (int v = 1; v <= k; v++)
        count[v] += count[v - 1];

    for (int i = n - 1; i >= 0; i--) {
        count[arr[i].key]--;
        output[count[arr[i].key]] = arr[i];
    }

    memcpy(arr, output, n * sizeof(Record));
    free(count);
    free(output);
}

/* ── Counting sort with negative range ───────────────────── */

void counting_sort_range(int *arr, int n, int min_val, int max_val) {
    int k = max_val - min_val;
    int *count  = calloc(k + 1, sizeof(int));
    int *output = malloc(n * sizeof(int));

    for (int i = 0; i < n; i++)
        count[arr[i] - min_val]++;

    for (int v = 1; v <= k; v++)
        count[v] += count[v - 1];

    for (int i = n - 1; i >= 0; i--) {
        int idx = arr[i] - min_val;
        count[idx]--;
        output[count[idx]] = arr[i];
    }

    memcpy(arr, output, n * sizeof(int));
    free(count);
    free(output);
}

/* ── Counting sort by digit (for radix sort) ─────────────── */

void counting_sort_digit(int *arr, int n, int exp) {
    int *output = malloc(n * sizeof(int));
    int count[10] = {0};

    for (int i = 0; i < n; i++)
        count[(arr[i] / exp) % 10]++;

    for (int v = 1; v < 10; v++)
        count[v] += count[v - 1];

    for (int i = n - 1; i >= 0; i--) {
        int digit = (arr[i] / exp) % 10;
        count[digit]--;
        output[count[digit]] = arr[i];
    }

    memcpy(arr, output, n * sizeof(int));
    free(output);
}

/* ── Input helpers ───────────────────────────────────────── */

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* ── Demo 1: Trace ───────────────────────────────────────── */

void demo_trace(void) {
    printf("=== Demo 1: Counting sort trace ===\n\n");

    int arr[] = {4, 2, 2, 8, 3, 3, 1};
    int n = 7, k = 8;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\nRange: [0, %d]\n\n", k);

    /* Step 1: count */
    int count[9] = {0};
    for (int i = 0; i < n; i++) count[arr[i]]++;

    printf("Step 1 — Count:\n  ");
    for (int v = 0; v <= k; v++) printf("count[%d]=%d ", v, count[v]);
    printf("\n\n");

    /* Step 2: prefix sum */
    for (int v = 1; v <= k; v++) count[v] += count[v - 1];

    printf("Step 2 — Prefix sum:\n  ");
    for (int v = 0; v <= k; v++) printf("count[%d]=%d ", v, count[v]);
    printf("\n\n");

    /* Step 3: place */
    int output[7];
    printf("Step 3 — Place (right to left):\n");
    for (int i = n - 1; i >= 0; i--) {
        int v = arr[i];
        count[v]--;
        output[count[v]] = arr[i];
        printf("  i=%d: arr[%d]=%d → count[%d]=%d → output[%d]=%d\n",
               i, i, v, v, count[v], count[v], v);
    }

    printf("\nOutput: ");
    for (int i = 0; i < n; i++) printf("%d ", output[i]);
    printf("\n\n");
}

/* ── Demo 2: Stability ──────────────────────────────────── */

void demo_stability(void) {
    printf("=== Demo 2: Stability with satellite data ===\n\n");

    Record items[] = {
        {3,'a'}, {1,'b'}, {3,'c'}, {2,'d'}, {1,'e'}, {3,'f'}, {2,'g'}
    };
    int n = 7, k = 3;

    printf("Input:  ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", items[i].key, items[i].tag);
    printf("\n");

    counting_sort_records(items, n, k);

    printf("Output: ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", items[i].key, items[i].tag);
    printf("\n");

    /* Verify stability */
    int stable = 1;
    for (int i = 1; i < n; i++) {
        if (items[i].key == items[i-1].key &&
            items[i].tag < items[i-1].tag) {
            stable = 0; break;
        }
    }
    printf("Stable: %s\n", stable ? "YES" : "NO");
    printf("  1's: b,e (original order preserved)\n");
    printf("  2's: d,g (original order preserved)\n");
    printf("  3's: a,c,f (original order preserved)\n\n");
}

/* ── Demo 3: Simple vs stable ────────────────────────────── */

void demo_simple_vs_stable(int n, int k) {
    printf("=== Demo 3: Simple vs Stable, n=%d, k=%d ===\n\n", n, k);

    int *arr = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) arr[i] = rand() % (k + 1);

    int *w1 = malloc(n * sizeof(int));
    int *w2 = malloc(n * sizeof(int));
    memcpy(w1, arr, n * sizeof(int));
    memcpy(w2, arr, n * sizeof(int));

    clock_t s1 = clock();
    counting_sort_simple(w1, n, k);
    clock_t e1 = clock();

    clock_t s2 = clock();
    counting_sort_stable(w2, n, k);
    clock_t e2 = clock();

    double us1 = (double)(e1 - s1) / CLOCKS_PER_SEC * 1e6;
    double us2 = (double)(e2 - s2) / CLOCKS_PER_SEC * 1e6;

    printf("  Simple: %8.0f us  %s\n", us1, is_sorted(w1, n) ? "OK" : "FAIL");
    printf("  Stable: %8.0f us  %s\n", us2, is_sorted(w2, n) ? "OK" : "FAIL");
    printf("  Simple is faster (no output buffer copy)\n\n");

    free(arr); free(w1); free(w2);
}

/* ── Demo 4: Counting sort vs qsort ─────────────────────── */

int cmp_int(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

void demo_vs_qsort(int n, int k) {
    printf("=== Demo 4: Counting sort vs qsort, n=%d, k=%d ===\n\n", n, k);

    int *arr = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) arr[i] = rand() % (k + 1);

    int *w1 = malloc(n * sizeof(int));
    int *w2 = malloc(n * sizeof(int));
    memcpy(w1, arr, n * sizeof(int));
    memcpy(w2, arr, n * sizeof(int));

    clock_t s1 = clock();
    counting_sort_stable(w1, n, k);
    clock_t e1 = clock();

    clock_t s2 = clock();
    qsort(w2, n, sizeof(int), cmp_int);
    clock_t e2 = clock();

    double us1 = (double)(e1 - s1) / CLOCKS_PER_SEC * 1e6;
    double us2 = (double)(e2 - s2) / CLOCKS_PER_SEC * 1e6;

    printf("  Counting sort: %10.0f us  %s\n", us1, is_sorted(w1, n) ? "OK" : "FAIL");
    printf("  qsort:         %10.0f us  %s\n", us2, is_sorted(w2, n) ? "OK" : "FAIL");
    if (us1 > 0)
        printf("  Ratio: qsort is %.1fx slower\n", us2 / us1);
    printf("\n");

    free(arr); free(w1); free(w2);
}

/* ── Demo 5: Impact of k ────────────────────────────────── */

void demo_impact_of_k(int n) {
    printf("=== Demo 5: Impact of k on counting sort, n=%d ===\n\n", n);
    printf("%10s %10s %10s\n", "k", "Time(us)", "k/n ratio");
    printf("%10s %10s %10s\n", "──────────", "──────────", "──────────");

    int ks[] = {10, 100, 1000, 10000, 100000, 1000000};
    for (int ki = 0; ki < 6; ki++) {
        int k = ks[ki];
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand() % (k + 1);

        clock_t s = clock();
        counting_sort_stable(arr, n, k);
        clock_t e = clock();
        double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;

        printf("%10d %10.0f %10.1f\n", k, us, (double)k / n);
        free(arr);
    }
    printf("\n");
}

/* ── Demo 6: Negative range ──────────────────────────────── */

void demo_negative_range(void) {
    printf("=== Demo 6: Negative range ===\n\n");

    int arr[] = {-3, 5, -1, 0, 3, -2, 4, -3, 1};
    int n = 9;
    int min_val = -3, max_val = 5;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\nRange: [%d, %d]\n", min_val, max_val);

    counting_sort_range(arr, n, min_val, max_val);

    printf("Output: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n%s\n\n", is_sorted(arr, n) ? "OK" : "FAIL");
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║        Counting Sort — Complete Analysis         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    demo_trace();
    demo_stability();
    demo_negative_range();
    demo_simple_vs_stable(100000, 100);
    demo_vs_qsort(100000, 100);
    demo_vs_qsort(1000000, 1000);
    demo_impact_of_k(100000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o counting_sort counting_sort.c
./counting_sort
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── Counting sort (simple, in-place) ────────────────────── */

fn counting_sort_simple(arr: &mut [i32], k: usize) {
    let mut count = vec![0u32; k + 1];
    for &x in arr.iter() {
        count[x as usize] += 1;
    }
    let mut pos = 0;
    for v in 0..=k {
        for _ in 0..count[v] {
            arr[pos] = v as i32;
            pos += 1;
        }
    }
}

/* ── Counting sort (stable) ──────────────────────────────── */

fn counting_sort_stable(arr: &mut [i32], k: usize) {
    let mut count = vec![0usize; k + 1];
    for &x in arr.iter() {
        count[x as usize] += 1;
    }
    for v in 1..=k {
        count[v] += count[v - 1];
    }
    let mut output = vec![0i32; arr.len()];
    for i in (0..arr.len()).rev() {
        let v = arr[i] as usize;
        count[v] -= 1;
        output[count[v]] = arr[i];
    }
    arr.copy_from_slice(&output);
}

/* ── Counting sort for records (stable, by key) ──────────── */

#[derive(Clone, Debug)]
struct Record {
    key: i32,
    tag: char,
}

fn counting_sort_records(arr: &mut [Record], k: usize) {
    let mut count = vec![0usize; k + 1];
    for r in arr.iter() {
        count[r.key as usize] += 1;
    }
    for v in 1..=k {
        count[v] += count[v - 1];
    }
    let mut output = vec![Record { key: 0, tag: ' ' }; arr.len()];
    for i in (0..arr.len()).rev() {
        let v = arr[i].key as usize;
        count[v] -= 1;
        output[count[v]] = arr[i].clone();
    }
    arr.clone_from_slice(&output);
}

/* ── Counting sort with negative range ───────────────────── */

fn counting_sort_range(arr: &mut [i32], min_val: i32, max_val: i32) {
    let k = (max_val - min_val) as usize;
    let mut count = vec![0usize; k + 1];
    for &x in arr.iter() {
        count[(x - min_val) as usize] += 1;
    }
    for v in 1..=k {
        count[v] += count[v - 1];
    }
    let mut output = vec![0i32; arr.len()];
    for i in (0..arr.len()).rev() {
        let idx = (arr[i] - min_val) as usize;
        count[idx] -= 1;
        output[count[idx]] = arr[i];
    }
    arr.copy_from_slice(&output);
}

/* ── Input helpers ───────────────────────────────────────── */

fn fill_random(n: usize, k: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xABCD1234).hash(&mut h);
        (h.finish() % (k as u64 + 1)) as i32
    }).collect()
}

fn is_sorted(a: &[i32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }

/* ── Demo 1: Trace ───────────────────────────────────────── */

fn demo_trace() {
    println!("=== Demo 1: Counting sort trace ===\n");

    let arr = vec![4, 2, 2, 8, 3, 3, 1];
    let n = arr.len();
    let k = 8;

    println!("Input: {:?}", arr);
    println!("Range: [0, {}]\n", k);

    /* Step 1 */
    let mut count = vec![0usize; k + 1];
    for &x in &arr { count[x as usize] += 1; }
    println!("Step 1 — Count: {:?}\n", count);

    /* Step 2 */
    for v in 1..=k { count[v] += count[v - 1]; }
    println!("Step 2 — Prefix sum: {:?}\n", count);

    /* Step 3 */
    let mut output = vec![0i32; n];
    println!("Step 3 — Place (right to left):");
    for i in (0..n).rev() {
        let v = arr[i] as usize;
        count[v] -= 1;
        output[count[v]] = arr[i];
        println!("  i={}: arr[{}]={} → count[{}]={} → output[{}]={}",
            i, i, arr[i], v, count[v], count[v], arr[i]);
    }
    println!("\nOutput: {:?}\n", output);
}

/* ── Demo 2: Stability ──────────────────────────────────── */

fn demo_stability() {
    println!("=== Demo 2: Stability with satellite data ===\n");

    let mut items = vec![
        Record { key: 3, tag: 'a' },
        Record { key: 1, tag: 'b' },
        Record { key: 3, tag: 'c' },
        Record { key: 2, tag: 'd' },
        Record { key: 1, tag: 'e' },
        Record { key: 3, tag: 'f' },
        Record { key: 2, tag: 'g' },
    ];

    print!("Input:  ");
    for r in &items { print!("({},{}) ", r.key, r.tag); }
    println!();

    counting_sort_records(&mut items, 3);

    print!("Output: ");
    for r in &items { print!("({},{}) ", r.key, r.tag); }
    println!();

    let stable = items.windows(2).all(|w|
        w[0].key != w[1].key || w[0].tag < w[1].tag
    );
    println!("Stable: {}\n", if stable { "YES" } else { "NO" });
}

/* ── Demo 3: vs sort_unstable ────────────────────────────── */

fn demo_vs_stdlib(n: usize, k: usize) {
    println!("=== Counting sort vs stdlib, n={}, k={} ===\n", n, k);

    let orig = fill_random(n, k);

    /* Counting sort */
    let mut w1 = orig.clone();
    let start = Instant::now();
    counting_sort_stable(&mut w1, k);
    let t1 = start.elapsed().as_micros();

    /* sort_unstable */
    let mut w2 = orig.clone();
    let start = Instant::now();
    w2.sort_unstable();
    let t2 = start.elapsed().as_micros();

    /* sort (stable) */
    let mut w3 = orig.clone();
    let start = Instant::now();
    w3.sort();
    let t3 = start.elapsed().as_micros();

    println!("  Counting sort:   {:>10} us  {}", t1, if is_sorted(&w1) { "OK" } else { "FAIL" });
    println!("  sort_unstable:   {:>10} us  {}", t2, if is_sorted(&w2) { "OK" } else { "FAIL" });
    println!("  sort (stable):   {:>10} us  {}", t3, if is_sorted(&w3) { "OK" } else { "FAIL" });
    if t1 > 0 {
        println!("  sort_unstable is {:.1}x slower than counting sort", t2 as f64 / t1 as f64);
    }
    println!();
}

/* ── Demo 4: Impact of k ────────────────────────────────── */

fn demo_impact_of_k(n: usize) {
    println!("=== Impact of k on counting sort, n={} ===\n", n);
    println!("{:>10} {:>10} {:>10}", "k", "Time(us)", "k/n");
    println!("{:>10} {:>10} {:>10}", "──────────", "──────────", "──────────");

    for &k in &[10, 100, 1000, 10000, 100000, 1000000] {
        let mut arr = fill_random(n, k);
        let start = Instant::now();
        counting_sort_stable(&mut arr, k);
        let elapsed = start.elapsed().as_micros();
        println!("{:>10} {:>10} {:>10.1}", k, elapsed, k as f64 / n as f64);
    }
    println!();
}

/* ── Demo 5: Negative range ──────────────────────────────── */

fn demo_negative_range() {
    println!("=== Negative range ===\n");
    let mut arr = vec![-3, 5, -1, 0, 3, -2, 4, -3, 1];
    println!("Input:  {:?}", arr);
    counting_sort_range(&mut arr, -3, 5);
    println!("Output: {:?}", arr);
    println!("{}\n", if is_sorted(&arr) { "OK" } else { "FAIL" });
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════╗");
    println!("║        Counting Sort — Complete Analysis         ║");
    println!("╚══════════════════════════════════════════════════╝\n");

    demo_trace();
    demo_stability();
    demo_negative_range();
    demo_vs_stdlib(100_000, 100);
    demo_vs_stdlib(1_000_000, 1000);
    demo_impact_of_k(100_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o counting_sort_rs counting_sort.rs
./counting_sort_rs
```

---

## Aplicaciones de counting sort

### 1. Subrutina de radix sort

Radix sort ordena por dígitos, y cada pasada por dígito es un counting
sort con $k = 9$ (base 10) o $k = 255$ (base 256). La estabilidad de
counting sort es **esencial**: cada pasada debe preservar el orden
producido por las pasadas anteriores.

### 2. Suffix array construction

Algoritmos como SA-IS y DC3/skew para construir suffix arrays usan
counting sort como primitiva fundamental, ya que los caracteres del
alfabeto tienen un rango pequeño y conocido.

### 3. Histogramas y frecuencias

El paso 1 de counting sort (contar ocurrencias) es exactamente un
histograma. Muchos algoritmos de procesamiento de imágenes, estadística
y compresión usan esta operación.

### 4. Bucket sort

Bucket sort es una generalización de counting sort: en lugar de un
bucket por valor exacto, se usan buckets para rangos de valores. Cada
bucket se ordena internamente con insertion sort o similar.

---

## Ejercicios

### Ejercicio 1 — Traza manual

Aplica counting sort estable a `[3, 1, 4, 1, 5, 9, 2, 6, 5, 3]` con
$k = 9$. Muestra los arrays `count` después de cada paso.

<details><summary>Predicción</summary>

Paso 1 — Contar: `count = [0, 2, 1, 2, 1, 2, 1, 0, 0, 1]`
(valor 1 aparece 2 veces, valor 3 aparece 2 veces, etc.)

Paso 2 — Prefix sum: `count = [0, 2, 3, 5, 6, 8, 9, 9, 9, 10]`
(2 elementos $\leq 1$, 3 $\leq 2$, 5 $\leq 3$, etc.)

Paso 3 — Colocar (derecha a izquierda):
- i=9: 3 → output[4], count[3]=4
- i=8: 5 → output[7], count[5]=7
- i=7: 6 → output[8], count[6]=8
- i=6: 2 → output[2], count[2]=2
- i=5: 9 → output[9], count[9]=9
- i=4: 5 → output[6], count[5]=6
- i=3: 1 → output[1], count[1]=1
- i=2: 4 → output[5], count[4]=5
- i=1: 1 → output[0], count[1]=0
- i=0: 3 → output[3], count[3]=3

Output: `[1, 1, 2, 3, 3, 4, 5, 5, 6, 9]`

</details>

### Ejercicio 2 — Estabilidad: izquierda vs derecha

Modifica counting sort para recorrer de izquierda a derecha en el paso
3 (i de 0 a n-1). ¿El resultado sigue siendo correcto? ¿Es estable?

<details><summary>Predicción</summary>

El resultado será **correcto** (los mismos elementos en las mismas
posiciones del grupo), pero **no estable**. Al recorrer de izquierda
a derecha, el primer elemento con valor $v$ se coloca en la última
posición disponible del grupo $v$ (porque `count[v]` apunta al final
del grupo y decrementamos). El segundo se coloca antes. Esto invierte
el orden relativo dentro de cada grupo.

Para verificar: con input `[(2,'a'), (2,'b')]`, el recorrido LTR
produciría `[(2,'b'), (2,'a')]` — orden invertido. RTL produce
`[(2,'a'), (2,'b')]` — orden preservado.

La estabilidad es crucial para radix sort: si counting sort no fuera
estable, radix sort no funcionaría.

</details>

### Ejercicio 3 — Counting sort vs qsort para diferentes k

Mide el tiempo de counting sort y `qsort` para $n = 10^6$ y
$k \in \{10, 100, 1000, 10^4, 10^5, 10^6, 10^7\}$. ¿En qué $k$
counting sort deja de ser más rápido?

<details><summary>Predicción</summary>

Counting sort es $O(n + k)$, qsort es $O(n \log n)$. Para $n = 10^6$,
$n \log_2 n \approx 2 \times 10^7$.

| $k$ | Counting sort ops | vs $n \log n$ | ¿Gana? |
|-----|------------------|---------------|--------|
| 10 | $\sim 10^6$ | 20x menos | **Sí** |
| 100 | $\sim 10^6$ | 20x menos | **Sí** |
| 1000 | $\sim 10^6$ | 20x menos | **Sí** |
| $10^4$ | $\sim 10^6$ | 20x menos | **Sí** |
| $10^5$ | $\sim 1.1 \times 10^6$ | 18x menos | **Sí** |
| $10^6$ | $\sim 2 \times 10^6$ | 10x menos | **Sí** |
| $10^7$ | $\sim 1.1 \times 10^7$ | 2x menos | Apenas |

Counting sort debería ganar hasta $k \approx 10^7$. Después de eso,
la inicialización de `count` ($O(k)$) domina. Además, para $k > 10^6$,
el array `count` no cabe en L2 cache (4 MB), degradando el rendimiento.

En la práctica, counting sort pierde ventaja alrededor de $k = 10^6$
por cache effects, no por operaciones.

</details>

### Ejercicio 4 — Counting sort para bytes

Implementa counting sort para un array de `unsigned char` (rango
$[0, 255]$, $k = 255$). Ordena 10 millones de bytes y compara con
`qsort`.

<details><summary>Predicción</summary>

Con $k = 255$, el array `count` tiene 256 entradas (1 KB) — cabe
completamente en L1 cache. Counting sort será extremadamente rápido:
una pasada de lectura + una pasada de escritura.

Para $n = 10^7$: counting sort ~5-10 ms, qsort ~500-800 ms.
Counting sort será **50-100x más rápido**.

Este es el caso ideal de counting sort: $k$ muy pequeño, $n$ muy
grande. Es exactamente lo que se usa en la práctica para ordenar
píxeles de imágenes por intensidad.

</details>

### Ejercicio 5 — Counting sort como histograma

Usa el paso 1 de counting sort (solo contar, sin ordenar) para
calcular la distribución de frecuencias de caracteres ASCII en un
archivo de texto. ¿Cuál es la complejidad?

<details><summary>Predicción</summary>

La complejidad es $O(n)$ donde $n$ es el tamaño del archivo, con
$O(k) = O(128) = O(1)$ espacio extra (128 posibles caracteres ASCII).

Este es un problema resuelto en una sola pasada, sin necesidad de
ordenar. Es la base de muchos algoritmos:
- **Huffman coding**: necesita frecuencias para construir el árbol.
- **Detección de idioma**: frecuencias de letras identifican idiomas.
- **Análisis de texto**: detectar caracteres inválidos, contar
  vocales, etc.

La pasada de conteo es bandwidth-limited: el cuello de botella es
leer bytes del archivo, no procesarlos.

</details>

### Ejercicio 6 — Counting sort bidireccional

Implementa una versión que ordena en orden descendente. Solo necesitas
cambiar la dirección de una iteración.

<details><summary>Predicción</summary>

Para orden descendente, hay dos opciones:

1. **Invertir el prefix sum**: acumular de derecha a izquierda.
   `count[k-1] += count[k]`, `count[k-2] += count[k-1]`, etc.
   Luego colocar normalmente.

2. **Colocar de izquierda a derecha**: en el paso 3, recorrer de
   $i = 0$ a $n - 1$ en lugar de $n - 1$ a $0$, y usar el prefix
   sum invertido.

La forma más simple: hacer counting sort normal (ascendente) y luego
invertir el array en $O(n)$. Total sigue siendo $O(n + k)$.

</details>

### Ejercicio 7 — Espacio: count in-place

¿Es posible hacer counting sort sin el array `output` (in-place)?
Analiza por qué es difícil y qué se sacrifica.

<details><summary>Predicción</summary>

La versión simple (solo valores, sin satelitales) **sí** puede ser
in-place: el paso 3 simplemente escribe los valores en orden usando
los conteos. No necesita `output`.

La versión estable con datos satelitales **no** puede ser fácilmente
in-place porque necesita colocar registros completos en posiciones
calculadas. Si se escribe directamente en `arr`, se sobreescriben
registros que aún no se han procesado.

Se puede hacer in-place con un algoritmo de "cycling" (seguir las
posiciones como una permutación), pero es complejo ($O(n + k)$ tiempo
pero con constante alta) y pierde la estabilidad.

Conclusión: el array `output` de $O(n)$ es el precio de la estabilidad.
Es el mismo espacio que merge sort usa.

</details>

### Ejercicio 8 — Counting sort para strings por primer carácter

Ordena un array de strings por su primer carácter usando counting sort
($k = 127$). ¿Es estable? ¿Es un sort completo?

<details><summary>Predicción</summary>

Counting sort por primer carácter es estable (si se implementa
correctamente con recorrido RTL). Las strings que empiezan con la
misma letra preservan su orden relativo.

**No** es un sort completo: strings con el mismo primer carácter no
están ordenadas entre sí. Para completar el sort, se necesita ordenar
recursivamente dentro de cada grupo por el segundo carácter, luego el
tercero, etc. Esto es exactamente **MSD radix sort** para strings.

Complejidad del primer paso: $O(n + 128) = O(n)$. Complejidad del
sort completo (MSD radix sobre todas las posiciones): $O(n \cdot L)$
donde $L$ es la longitud promedio de las strings.

</details>

### Ejercicio 9 — Parallel counting sort

El paso 1 (contar) se puede paralelizar: cada thread cuenta su
porción del array en un `count` local, luego se suman los conteos.
Implementa esta versión en Rust con 2 threads.

<details><summary>Predicción</summary>

Con 2 threads, cada uno procesa $n/2$ elementos. El paso de conteo
se reduce de $O(n)$ a $O(n/2)$, pero hay overhead de:
- Crear threads (~10 us).
- Sumar los dos arrays `count` ($O(k)$).

Para $n = 10^7$ y $k = 100$: el conteo secuencial toma ~10 ms.
Con 2 threads: ~6-7 ms (speedup ~1.5x, no 2x por overhead y
contención de memoria).

El paso 3 (colocar) es más difícil de paralelizar porque los threads
escriben en posiciones que dependen del prefix sum global. Se puede
particionar el output por valor: thread 1 coloca valores $[0, k/2]$,
thread 2 coloca $[k/2+1, k]$.

</details>

### Ejercicio 10 — Counting sort como building block

Implementa un sort para datos tipo `(edad, nombre)` donde `edad`
está en $[0, 120]$: primero haz counting sort por edad, luego dentro
de cada grupo de edad, ordena los nombres con `strcmp` (insertion sort
ya que cada grupo es pequeño).

<details><summary>Predicción</summary>

Paso 1: counting sort por edad — $O(n + 120) = O(n)$. Estable, así
que personas con la misma edad mantienen su orden original.

Paso 2: para cada grupo de edad, ordenar por nombre. Si hay $n_v$
personas con edad $v$, insertion sort toma $O(n_v^2)$. Con distribución
uniforme, $n_v \approx n/120$.

Total: $O(n) + \sum_{v=0}^{120} O(n_v^2) \approx O(n) + 120 \times
O((n/120)^2) = O(n^2/120)$. Para $n = 12000$: $O(12000 + 1200000/120)
= O(12000 + 10000) = O(22000)$ — mucho mejor que $O(n^2) = O(144M)$
o incluso $O(n \log n) = O(160K)$.

Este es el principio de **bucket sort**: dividir por un criterio
barato (counting sort) y refinar dentro de cada bucket.

</details>
