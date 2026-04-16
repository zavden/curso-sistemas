# Cuándo usar ordenamientos cuadráticos

## Objetivo

Sintetizar los criterios prácticos para elegir un algoritmo cuadrático
sobre uno eficiente:

- Para arrays pequeños ($n < \sim 50$), los cuadráticos son **más rápidos**
  que los $O(n \log n)$ por constantes más bajas y mejor cache.
- Insertion sort es el ganador absoluto entre los cuadráticos: adaptativo,
  estable, in-place, mínimo overhead.
- Es la subrutina base de Timsort ($n < 64$) e introsort ($n < 16$).
- Cuándo selection sort o bubble sort pueden justificarse (casi nunca).

---

## El argumento del crossover

### Por qué "más eficiente" no siempre es más rápido

Un algoritmo $O(n \log n)$ como merge sort tiene la forma:

$$T_{merge}(n) = c_1 \cdot n \log_2 n$$

mientras que insertion sort tiene:

$$T_{ins}(n) = c_2 \cdot n^2$$

donde $c_1 > c_2$ porque merge sort tiene más overhead por operación:
llamadas recursivas, asignación de memoria auxiliar, copia de datos entre
buffers, y más instrucciones por comparación.

El **crossover point** es el $n_0$ donde ambas curvas se cruzan:

$$c_2 \cdot n_0^2 = c_1 \cdot n_0 \log_2 n_0$$

$$n_0 = \frac{c_1}{c_2} \cdot \log_2 n_0$$

Para $n < n_0$, insertion sort es más rápido. En hardware moderno,
$n_0$ típicamente cae entre 20 y 64 dependiendo del tipo de dato, la
arquitectura y el algoritmo eficiente contra el que se compara.

### Factores que influyen en el crossover

| Factor | Favorece cuadrático | Favorece eficiente |
|--------|--------------------|--------------------|
| Overhead de recursión | Llamadas recursivas cuestan ~20 ns cada una | Irrelevante para $n$ grande |
| Predicción de ramas | Loop simple del cuadrático es predecible | Recursión dificulta predicción |
| Cache L1 | Array pequeño cabe entero en L1 (32-64 KB) | Merge sort necesita buffer auxiliar |
| Inline | `insertion_sort` se puede inlinear | `merge_sort` recursivo no |
| Elementos grandes | Más costoso mover datos (shifts) | Merge sort mueve igual |
| Datos casi ordenados | Insertion sort: $O(n)$ | Merge sort: $O(n \log n)$ siempre |

### La constante oculta

Las mediciones empíricas típicas en x86-64 con `int` muestran:

| Algoritmo | Costo por comparación+acción | Razón |
|-----------|------------------------------|-------|
| Insertion sort | ~2-3 ns | Un shift = 1 escritura |
| Merge sort | ~5-8 ns | Copia a buffer + merge + copia de vuelta |
| Quicksort | ~3-5 ns | Swap = 3 escrituras, pero buen cache |

Estas constantes significan que para $n = 20$, insertion sort hace
~$20^2/2 = 200$ operaciones a 2 ns = 400 ns, mientras que merge sort
hace ~$20 \cdot \log_2 20 \approx 87$ operaciones a 6 ns = 522 ns.
Insertion sort gana.

---

## Insertion sort: el ganador práctico

### Por qué no bubble sort ni selection sort

La comparación entre los tres cuadráticos deja claro quién domina:

| Criterio | Bubble | Selection | Insertion |
|----------|--------|-----------|-----------|
| Mejor caso | $O(n)$ con bandera | $O(n^2)$ siempre | $O(n)$ natural |
| Adaptativo | Sí (con bandera) | No | **Sí** |
| Estable | Sí | **No** | **Sí** |
| Escrituras (peor) | $3 \cdot n^2/2$ (swaps) | $3n$ (swaps) | $n^2/2$ (shifts) |
| Escrituras (mejor) | $0$ | $3n$ | $0$ |
| Cache | Bueno (adyacente) | Malo (scan largo) | **Excelente** (secuencial) |
| Overhead | Flag check extra | Scan mínimo completo | Mínimo |

Insertion sort combina las ventajas de ambos sin sus desventajas:
- Es adaptativo como bubble sort (mejor que bubble: no necesita bandera).
- Hace menos escrituras que bubble sort en el peor caso ($n^2/2$ shifts
  vs $3n^2/2$ writes de swaps).
- Es estable, a diferencia de selection sort.
- Es más cache-friendly que selection sort.

La **única** ventaja de selection sort es el mínimo número de swaps
($n - 1$), útil cuando un swap es extremadamente costoso (Flash memory,
structs de 1 KB). Pero ese caso es raro.

Bubble sort no tiene ninguna ventaja práctica sobre insertion sort.
Knuth lo resume: "*the bubble sort seems to have nothing to recommend
it, except a catchy name*".

### Insertion sort como subrutina

Los algoritmos de ordenamiento más usados en producción usan insertion
sort internamente:

**Timsort** (Python `sort`, Java `Arrays.sort` para objetos, Rust `sort`):
1. Divide el array en "runs" (subsecuencias ya ordenadas).
2. Los runs cortos ($< 64$ elementos, o $< 32$ en algunas implementaciones)
   se extienden usando **binary insertion sort**.
3. Los runs se fusionan con un merge adaptativo.

**Introsort** (C++ `std::sort`, .NET `Array.Sort`):
1. Comienza con quicksort.
2. Si la profundidad de recursión excede $2 \lfloor \log_2 n \rfloor$,
   cambia a heapsort para garantizar $O(n \log n)$.
3. Cuando la partición reduce el subarray a $n < 16$ (o 32 según la
   implementación), cambia a **insertion sort**.

**pdqsort** (Rust `sort_unstable`, Boost.Sort):
1. Pattern-defeating quicksort: detecta patrones y elige estrategia.
2. Usa insertion sort para particiones pequeñas ($n \leq 24$).
3. Cambia a heapsort si detecta pivotes adversariales.

En todos los casos, insertion sort se elige por la misma razón: para
$n$ pequeño es el más rápido, y como el array grande se va dividiendo
recursivamente, eventualmente todas las piezas son pequeñas.

---

## El threshold: cómo elegir el valor de corte

### Metodología empírica

El valor óptimo del threshold $T$ (donde se cambia de quicksort/mergesort
a insertion sort) depende del hardware. Se determina experimentalmente:

1. Implementar el algoritmo híbrido con $T$ configurable.
2. Medir tiempo para valores de $T$ en $\{4, 8, 12, 16, 20, 24, 32, 48, 64\}$.
3. Probar con diferentes tipos de entrada (aleatorio, casi ordenado, etc.).
4. Elegir el $T$ que minimiza el tiempo promedio.

### Valores típicos en implementaciones reales

| Implementación | Algoritmo | Threshold | Tipo |
|----------------|-----------|-----------|------|
| glibc `qsort` | Quicksort | 4 | `size_t` genérico |
| GCC `std::sort` | Introsort | 16 | Templateado |
| MSVC `std::sort` | Introsort | 32 | Templateado |
| CPython `list.sort` | Timsort | 64 | Objetos Python |
| Java `Arrays.sort` (primitivos) | Dual-pivot QS | 47 | Primitivos |
| Java `Arrays.sort` (objetos) | Timsort | 32 | Objetos |
| Rust `sort_unstable` | pdqsort | 20-24 | Monomorphizado |
| Rust `sort` | Timsort variant | 20 | Monomorphizado |

El rango 16-64 cubre casi todas las implementaciones. La variación
se debe a:
- **Lenguajes con indirección** (Python, Java objetos) usan thresholds
  más altos porque la comparación es más cara (virtual dispatch).
- **Lenguajes sin indirección** (C++ templates, Rust generics) usan
  thresholds más bajos porque el merge/quicksort inlinea las
  comparaciones.

---

## Cuándo usar cada cuadrático: guía de decisión

### Flujo de decisión

```
¿n < ~50?
├── Sí
│   ├── ¿Datos probablemente casi ordenados?
│   │   ├── Sí → INSERTION SORT (O(n), mejor caso posible)
│   │   └── No → INSERTION SORT (aún el mejor cuadrático)
│   └── ¿Swaps extremadamente caros? (Flash, struct > 1KB, write-limited HW)
│       ├── Sí → SELECTION SORT (mínimo n-1 swaps)
│       └── No → INSERTION SORT
├── No
│   └── Usar algoritmo O(n log n) con cutoff a insertion sort
│
¿Bubble sort?
└── Solo para enseñanza. Nunca en producción.
```

### Los nichos restantes de selection sort

Selection sort se justifica en exactamente estos casos:

1. **Memoria Flash / EEPROM**: Cada escritura desgasta la celda. Selection
   sort minimiza escrituras ($n - 1$ swaps = $3(n-1)$ writes). Insertion
   sort puede hacer $O(n^2)$ shifts en el peor caso.

2. **Structs enormes sin indirección**: Si cada swap mueve 1 KB de datos
   y no se puede usar punteros, minimizar swaps importa. Pero la solución
   real es ordenar un array de índices.

3. **Sistemas WCET (Worst Case Execution Time)**: En sistemas de tiempo
   real duro, selection sort tiene rendimiento perfectamente predecible:
   siempre exactamente $n(n-1)/2$ comparaciones y $n-1$ swaps. No hay
   varianza entre mejor y peor caso.

Fuera de estos nichos, insertion sort domina.

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Metrics ─────────────────────────────────────────────── */

typedef struct {
    long comparisons;
    long writes;  /* shifts or swap-writes */
} Metrics;

/* ── Sorting algorithms ──────────────────────────────────── */

void insertion_sort(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->writes = 0;
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j];
            m->writes++;
            j--;
        }
        arr[j + 1] = key;
        m->writes++;
    }
}

void selection_sort(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->writes = 0;
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            m->comparisons++;
            if (arr[j] < arr[min_idx]) min_idx = j;
        }
        if (min_idx != i) {
            int tmp = arr[i];
            arr[i] = arr[min_idx];
            arr[min_idx] = tmp;
            m->writes += 3;
        }
    }
}

void bubble_sort(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->writes = 0;
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            m->comparisons++;
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                m->writes += 3;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

/* ── Merge sort with insertion sort cutoff ───────────────── */

static void merge(int *arr, int *tmp, int lo, int mid, int hi, Metrics *m) {
    memcpy(tmp + lo, arr + lo, (hi - lo + 1) * sizeof(int));
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        m->comparisons++;
        if (tmp[i] <= tmp[j]) {
            arr[k++] = tmp[i++];
        } else {
            arr[k++] = tmp[j++];
        }
        m->writes++;
    }
    while (i <= mid) { arr[k++] = tmp[i++]; m->writes++; }
    while (j <= hi)  { arr[k++] = tmp[j++]; m->writes++; }
}

static void insertion_sub(int *arr, int lo, int hi, Metrics *m) {
    for (int i = lo + 1; i <= hi; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= lo && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j];
            m->writes++;
            j--;
        }
        arr[j + 1] = key;
        m->writes++;
    }
}

static void merge_sort_hybrid(int *arr, int *tmp, int lo, int hi,
                               int threshold, Metrics *m) {
    if (hi - lo + 1 <= threshold) {
        insertion_sub(arr, lo, hi, m);
        return;
    }
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_hybrid(arr, tmp, lo, mid, threshold, m);
    merge_sort_hybrid(arr, tmp, mid + 1, hi, threshold, m);
    merge(arr, tmp, lo, mid, hi, m);
}

/* ── Pure merge sort (threshold = 1) ─────────────────────── */

static void merge_sort_pure(int *arr, int *tmp, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_pure(arr, tmp, lo, mid, m);
    merge_sort_pure(arr, tmp, mid + 1, hi, m);
    merge(arr, tmp, lo, mid, hi, m);
}

/* ── Input generators ────────────────────────────────────── */

void fill_random(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = rand() % (n * 10);
}

void fill_sorted(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
}

void fill_nearly_sorted(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
    /* Perturb ~5% of elements */
    int perturbs = n / 20;
    if (perturbs < 1) perturbs = 1;
    for (int k = 0; k < perturbs; k++) {
        int i = rand() % n;
        int j = rand() % n;
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

void fill_reversed(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = n - i;
}

/* ── Benchmarking ────────────────────────────────────────── */

typedef void (*SortFn)(int *, int, Metrics *);

typedef struct {
    const char *name;
    SortFn fn;
} SortEntry;

void benchmark_quadratics(int n) {
    SortEntry sorts[] = {
        {"Insertion", insertion_sort},
        {"Selection", selection_sort},
        {"Bubble   ", bubble_sort},
    };
    int num_sorts = 3;

    typedef struct {
        const char *name;
        void (*fill)(int *, int);
    } InputType;

    InputType inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Nearly sorted", fill_nearly_sorted},
        {"Reversed     ", fill_reversed},
    };
    int num_inputs = 4;

    int *original = malloc(n * sizeof(int));
    int *working  = malloc(n * sizeof(int));

    printf("\n=== Quadratic sorts comparison: n = %d ===\n\n", n);
    printf("%-14s %-10s %12s %12s %10s\n",
           "Input", "Algorithm", "Comparisons", "Writes", "Time(us)");
    printf("%-14s %-10s %12s %12s %10s\n",
           "──────────────", "──────────", "────────────", "────────────", "──────────");

    for (int t = 0; t < num_inputs; t++) {
        inputs[t].fill(original, n);

        for (int s = 0; s < num_sorts; s++) {
            memcpy(working, original, n * sizeof(int));
            Metrics m;

            clock_t start = clock();
            sorts[s].fn(working, n, &m);
            clock_t end = clock();
            double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

            printf("%-14s %-10s %12ld %12ld %10.0f\n",
                   inputs[t].name, sorts[s].name, m.comparisons, m.writes, us);
        }
        printf("\n");
    }

    free(original);
    free(working);
}

/* ── Threshold finder ────────────────────────────────────── */

void find_optimal_threshold(int n) {
    int thresholds[] = {1, 4, 8, 12, 16, 20, 24, 32, 48, 64};
    int num_t = sizeof(thresholds) / sizeof(thresholds[0]);
    int trials = 5;

    int *original = malloc(n * sizeof(int));
    int *working  = malloc(n * sizeof(int));
    int *tmp      = malloc(n * sizeof(int));

    printf("\n=== Optimal threshold for merge sort + insertion sort: n = %d ===\n\n", n);
    printf("%10s %12s %12s %12s\n",
           "Threshold", "Comparisons", "Writes", "Time(us)");
    printf("%10s %12s %12s %12s\n",
           "──────────", "────────────", "────────────", "────────────");

    double best_time = 1e18;
    int best_t = 1;

    for (int ti = 0; ti < num_t; ti++) {
        long total_cmp = 0, total_wr = 0;
        double total_us = 0;

        for (int trial = 0; trial < trials; trial++) {
            fill_random(original, n);
            memcpy(working, original, n * sizeof(int));
            Metrics m = {0, 0};

            clock_t start = clock();
            merge_sort_hybrid(working, tmp, 0, n - 1, thresholds[ti], &m);
            clock_t end = clock();

            total_cmp += m.comparisons;
            total_wr  += m.writes;
            total_us  += (double)(end - start) / CLOCKS_PER_SEC * 1e6;
        }

        double avg_us = total_us / trials;
        printf("%10d %12ld %12ld %12.0f%s\n",
               thresholds[ti], total_cmp / trials, total_wr / trials,
               avg_us, avg_us <= best_time ? " <--" : "");

        if (avg_us < best_time) {
            best_time = avg_us;
            best_t = thresholds[ti];
        }
    }

    printf("\nBest threshold: %d (%.0f us average)\n", best_t, best_time);

    free(original);
    free(working);
    free(tmp);
}

/* ── Demo: hybrid vs pure merge sort ─────────────────────── */

void demo_hybrid_vs_pure(int n) {
    int *original = malloc(n * sizeof(int));
    int *working  = malloc(n * sizeof(int));
    int *tmp      = malloc(n * sizeof(int));
    int trials = 5;

    printf("\n=== Hybrid merge sort (T=16) vs Pure merge sort: n = %d ===\n\n", n);

    typedef struct {
        const char *name;
        void (*fill)(int *, int);
    } InputType;

    InputType inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Nearly sorted", fill_nearly_sorted},
        {"Reversed     ", fill_reversed},
    };
    int num_inputs = 4;

    printf("%-14s %-22s %12s %12s %10s\n",
           "Input", "Algorithm", "Comparisons", "Writes", "Time(us)");
    printf("%-14s %-22s %12s %12s %10s\n",
           "──────────────", "──────────────────────", "────────────",
           "────────────", "──────────");

    for (int t = 0; t < num_inputs; t++) {
        /* Pure merge sort */
        long cmp_p = 0, wr_p = 0;
        double us_p = 0;
        for (int trial = 0; trial < trials; trial++) {
            inputs[t].fill(original, n);
            memcpy(working, original, n * sizeof(int));
            Metrics m = {0, 0};
            clock_t start = clock();
            merge_sort_pure(working, tmp, 0, n - 1, &m);
            clock_t end = clock();
            cmp_p += m.comparisons;
            wr_p  += m.writes;
            us_p  += (double)(end - start) / CLOCKS_PER_SEC * 1e6;
        }

        /* Hybrid (threshold = 16) */
        long cmp_h = 0, wr_h = 0;
        double us_h = 0;
        for (int trial = 0; trial < trials; trial++) {
            inputs[t].fill(original, n);
            memcpy(working, original, n * sizeof(int));
            Metrics m = {0, 0};
            clock_t start = clock();
            merge_sort_hybrid(working, tmp, 0, n - 1, 16, &m);
            clock_t end = clock();
            cmp_h += m.comparisons;
            wr_h  += m.writes;
            us_h  += (double)(end - start) / CLOCKS_PER_SEC * 1e6;
        }

        printf("%-14s %-22s %12ld %12ld %10.0f\n",
               inputs[t].name, "Pure merge sort", cmp_p / trials, wr_p / trials,
               us_p / trials);
        printf("%-14s %-22s %12ld %12ld %10.0f\n",
               inputs[t].name, "Hybrid (T=16)", cmp_h / trials, wr_h / trials,
               us_h / trials);
        if (us_p > 0) {
            printf("%-14s Speedup: %.2fx\n", "", us_p / us_h);
        }
        printf("\n");
    }

    free(original);
    free(working);
    free(tmp);
}

/* ── Demo: crossover point ───────────────────────────────── */

void demo_crossover(void) {
    printf("\n=== Crossover point: insertion sort vs merge sort ===\n\n");
    printf("%6s %12s %12s   %s\n", "n", "Ins(us)", "Merge(us)", "Winner");
    printf("%6s %12s %12s   %s\n", "──────", "────────────", "────────────", "──────");

    int sizes[] = {5, 10, 15, 20, 30, 40, 50, 75, 100, 200, 500};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    int trials = 50;

    for (int si = 0; si < num_sizes; si++) {
        int n = sizes[si];
        int *original = malloc(n * sizeof(int));
        int *working  = malloc(n * sizeof(int));
        int *tmp      = malloc(n * sizeof(int));

        double ins_us = 0, merge_us = 0;

        for (int trial = 0; trial < trials; trial++) {
            fill_random(original, n);
            Metrics m;

            memcpy(working, original, n * sizeof(int));
            clock_t start = clock();
            insertion_sort(working, n, &m);
            clock_t end = clock();
            ins_us += (double)(end - start) / CLOCKS_PER_SEC * 1e6;

            memcpy(working, original, n * sizeof(int));
            m = (Metrics){0, 0};
            start = clock();
            merge_sort_pure(working, tmp, 0, n - 1, &m);
            end = clock();
            merge_us += (double)(end - start) / CLOCKS_PER_SEC * 1e6;
        }

        ins_us   /= trials;
        merge_us /= trials;

        printf("%6d %12.1f %12.1f   %s\n", n, ins_us, merge_us,
               ins_us < merge_us ? "Insertion" : "Merge");

        free(original);
        free(working);
        free(tmp);
    }
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   When to use quadratic sorts — Empirical analysis  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    /* 1. Compare the three quadratics */
    benchmark_quadratics(100);
    benchmark_quadratics(1000);

    /* 2. Find crossover point */
    demo_crossover();

    /* 3. Find optimal threshold for hybrid sort */
    find_optimal_threshold(10000);
    find_optimal_threshold(100000);

    /* 4. Hybrid vs pure merge sort */
    demo_hybrid_vs_pure(10000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o when_quadratic when_quadratic.c -lm
./when_quadratic
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── Metrics ─────────────────────────────────────────────── */

#[derive(Default, Clone)]
struct Metrics {
    comparisons: u64,
    writes: u64,
}

/* ── Sorting algorithms ──────────────────────────────────── */

fn insertion_sort(arr: &mut [i32], m: &mut Metrics) {
    for i in 1..arr.len() {
        let key = arr[i];
        let mut j = i;
        while j > 0 {
            m.comparisons += 1;
            if arr[j - 1] <= key {
                break;
            }
            arr[j] = arr[j - 1];
            m.writes += 1;
            j -= 1;
        }
        arr[j] = key;
        m.writes += 1;
    }
}

fn selection_sort(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut min_idx = i;
        for j in (i + 1)..n {
            m.comparisons += 1;
            if arr[j] < arr[min_idx] {
                min_idx = j;
            }
        }
        if min_idx != i {
            arr.swap(i, min_idx);
            m.writes += 3;
        }
    }
}

fn bubble_sort(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut swapped = false;
        for j in 0..(n - 1 - i) {
            m.comparisons += 1;
            if arr[j] > arr[j + 1] {
                arr.swap(j, j + 1);
                m.writes += 3;
                swapped = true;
            }
        }
        if !swapped {
            break;
        }
    }
}

/* ── Merge sort (pure and hybrid) ────────────────────────── */

fn merge(arr: &mut [i32], tmp: &mut [i32], lo: usize, mid: usize, hi: usize, m: &mut Metrics) {
    tmp[lo..=hi].copy_from_slice(&arr[lo..=hi]);
    let (mut i, mut j, mut k) = (lo, mid + 1, lo);
    while i <= mid && j <= hi {
        m.comparisons += 1;
        if tmp[i] <= tmp[j] {
            arr[k] = tmp[i];
            i += 1;
        } else {
            arr[k] = tmp[j];
            j += 1;
        }
        m.writes += 1;
        k += 1;
    }
    while i <= mid {
        arr[k] = tmp[i];
        m.writes += 1;
        i += 1;
        k += 1;
    }
    while j <= hi {
        arr[k] = tmp[j];
        m.writes += 1;
        j += 1;
        k += 1;
    }
}

fn merge_sort_pure(arr: &mut [i32], tmp: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    let mid = lo + (hi - lo) / 2;
    merge_sort_pure(arr, tmp, lo, mid, m);
    merge_sort_pure(arr, tmp, mid + 1, hi, m);
    merge(arr, tmp, lo, mid, hi, m);
}

fn insertion_sub(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    for i in (lo + 1)..=hi {
        let key = arr[i];
        let mut j = i;
        while j > lo {
            m.comparisons += 1;
            if arr[j - 1] <= key {
                break;
            }
            arr[j] = arr[j - 1];
            m.writes += 1;
            j -= 1;
        }
        arr[j] = key;
        m.writes += 1;
    }
}

fn merge_sort_hybrid(
    arr: &mut [i32], tmp: &mut [i32], lo: usize, hi: usize,
    threshold: usize, m: &mut Metrics,
) {
    if hi - lo + 1 <= threshold {
        insertion_sub(arr, lo, hi, m);
        return;
    }
    if lo >= hi {
        return;
    }
    let mid = lo + (hi - lo) / 2;
    merge_sort_hybrid(arr, tmp, lo, mid, threshold, m);
    merge_sort_hybrid(arr, tmp, mid + 1, hi, threshold, m);
    merge(arr, tmp, lo, mid, hi, m);
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    let mut v = Vec::with_capacity(n);
    for i in 0..n {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xDEAD_BEEF_u64.wrapping_mul(n as u64)).hash(&mut h);
        v.push((h.finish() % (n as u64 * 10)) as i32);
    }
    v
}

fn fill_sorted(n: usize) -> Vec<i32> {
    (0..n as i32).collect()
}

fn fill_nearly_sorted(n: usize) -> Vec<i32> {
    let mut v: Vec<i32> = (0..n as i32).collect();
    let perturbs = (n / 20).max(1);
    for k in 0..perturbs {
        let i = (k * 7 + 13) % n;
        let j = (k * 11 + 3) % n;
        v.swap(i, j);
    }
    v
}

fn fill_reversed(n: usize) -> Vec<i32> {
    (0..n as i32).rev().collect()
}

/* ── Benchmarking ────────────────────────────────────────── */

fn benchmark_quadratics(n: usize) {
    println!("\n=== Quadratic sorts comparison: n = {} ===\n", n);
    println!(
        "{:<14} {:<10} {:>12} {:>12} {:>10}",
        "Input", "Algorithm", "Comparisons", "Writes", "Time(us)"
    );
    println!(
        "{:<14} {:<10} {:>12} {:>12} {:>10}",
        "──────────────", "──────────", "────────────", "────────────", "──────────"
    );

    let input_names = ["Random", "Sorted", "Nearly sorted", "Reversed"];
    let generators: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_sorted, fill_nearly_sorted, fill_reversed,
    ];
    let sort_names = ["Insertion", "Selection", "Bubble"];

    type SortFn = fn(&mut [i32], &mut Metrics);
    let sorts: Vec<SortFn> = vec![insertion_sort, selection_sort, bubble_sort];

    for (ti, gen) in generators.iter().enumerate() {
        let original = gen(n);
        for (si, sort_fn) in sorts.iter().enumerate() {
            let mut working = original.clone();
            let mut m = Metrics::default();

            let start = Instant::now();
            sort_fn(&mut working, &mut m);
            let elapsed = start.elapsed().as_micros();

            println!(
                "{:<14} {:<10} {:>12} {:>12} {:>10}",
                input_names[ti], sort_names[si], m.comparisons, m.writes, elapsed
            );
        }
        println!();
    }
}

fn find_optimal_threshold(n: usize) {
    let thresholds = [1, 4, 8, 12, 16, 20, 24, 32, 48, 64];
    let trials = 5;

    println!(
        "\n=== Optimal threshold for merge sort + insertion sort: n = {} ===\n",
        n
    );
    println!(
        "{:>10} {:>12} {:>12} {:>12}",
        "Threshold", "Comparisons", "Writes", "Time(us)"
    );
    println!(
        "{:>10} {:>12} {:>12} {:>12}",
        "──────────", "────────────", "────────────", "────────────"
    );

    let mut best_time = u128::MAX;
    let mut best_t = 1;

    for &threshold in &thresholds {
        let mut total_cmp = 0u64;
        let mut total_wr = 0u64;
        let mut total_us = 0u128;

        for trial in 0..trials {
            let original = fill_random(n + trial); // vary slightly
            let mut working = original[..n].to_vec();
            let mut tmp = vec![0i32; n];
            let mut m = Metrics::default();

            let start = Instant::now();
            merge_sort_hybrid(&mut working, &mut tmp, 0, n - 1, threshold, &mut m);
            let elapsed = start.elapsed().as_micros();

            total_cmp += m.comparisons;
            total_wr += m.writes;
            total_us += elapsed;
        }

        let avg_us = total_us / trials as u128;
        let marker = if avg_us <= best_time { " <--" } else { "" };
        println!(
            "{:>10} {:>12} {:>12} {:>12}{}",
            threshold,
            total_cmp / trials as u64,
            total_wr / trials as u64,
            avg_us,
            marker
        );

        if avg_us < best_time {
            best_time = avg_us;
            best_t = threshold;
        }
    }
    println!("\nBest threshold: {} ({} us average)", best_t, best_time);
}

fn demo_crossover() {
    println!("\n=== Crossover point: insertion sort vs merge sort ===\n");
    println!(
        "{:>6} {:>12} {:>12}   {}",
        "n", "Ins(us)", "Merge(us)", "Winner"
    );
    println!(
        "{:>6} {:>12} {:>12}   {}",
        "──────", "────────────", "────────────", "──────"
    );

    let sizes = [5, 10, 15, 20, 30, 40, 50, 75, 100, 200, 500];
    let trials = 50usize;

    for &n in &sizes {
        let mut ins_us = 0u128;
        let mut merge_us = 0u128;

        for t in 0..trials {
            let original = fill_random(n + t);
            let data: Vec<i32> = original[..n].to_vec();

            let mut working = data.clone();
            let mut m = Metrics::default();
            let start = Instant::now();
            insertion_sort(&mut working, &mut m);
            ins_us += start.elapsed().as_nanos();

            let mut working = data.clone();
            let mut tmp = vec![0i32; n];
            let mut m = Metrics::default();
            let start = Instant::now();
            merge_sort_pure(&mut working, &mut tmp, 0, n - 1, &mut m);
            merge_us += start.elapsed().as_nanos();
        }

        let ins_avg = ins_us as f64 / trials as f64 / 1000.0;
        let merge_avg = merge_us as f64 / trials as f64 / 1000.0;
        let winner = if ins_avg < merge_avg { "Insertion" } else { "Merge" };
        println!(
            "{:>6} {:>12.1} {:>12.1}   {}",
            n, ins_avg, merge_avg, winner
        );
    }
}

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║   When to use quadratic sorts — Empirical analysis  ║");
    println!("╚══════════════════════════════════════════════════════╝");

    benchmark_quadratics(100);
    benchmark_quadratics(1000);
    demo_crossover();
    find_optimal_threshold(10_000);
    find_optimal_threshold(100_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o when_quadratic_rs when_quadratic.rs
./when_quadratic_rs
```

---

## Análisis detallado: por qué insertion sort domina para n pequeño

### Localidad de cache

Insertion sort accede a memoria de forma estrictamente secuencial hacia
la izquierda desde la posición actual. Para un array de 20 `int` (80
bytes), todo el array cabe en una **línea de cache** de L1 (64 bytes la
línea, 80 bytes = 2 líneas). Cada acceso es un **cache hit**.

Merge sort, incluso para 20 elementos, necesita:
- Un buffer auxiliar de 20 `int` (80 bytes más).
- Copiar datos al buffer y de vuelta.
- Los accesos alternan entre el array original y el buffer, lo que
  puede causar **conflictos de cache** si ambos mapean a la misma línea.

### Predicción de ramas

Los procesadores modernos predicen el resultado de saltos condicionales.
El loop interno de insertion sort (`while arr[j] > key`) tiene un patrón
predecible: mayormente "continuar" (los elementos se desplazan) hasta
un punto donde "parar" (se encontró la posición). El predictor de ramas
alcanza alta precisión (~90%).

Merge sort tiene dos fuentes de impredecibilidad:
- La comparación en el merge (`if tmp[i] <= tmp[j]`) es esencialmente
  aleatoria — el predictor acierta ~50%.
- Las llamadas recursivas generan saltos indirectos que el BTB (Branch
  Target Buffer) puede no predecir bien.

### Overhead de llamadas

Para $n = 16$, merge sort puro hace $2 \cdot 16 - 1 = 31$ llamadas
recursivas. Cada llamada implica:
- Push de registros al stack (~8 registros = 64 bytes).
- Ajuste del stack pointer.
- Salto al cuerpo de la función.
- Pop al retornar.

Costo estimado: ~15-20 ns por llamada × 31 = ~500 ns solo en overhead
de llamadas. Insertion sort para 16 elementos toma ~400 ns en total.

---

## Casos de estudio reales

### Caso 1: glibc qsort — threshold = 4

La implementación de `qsort` en glibc usa un threshold muy bajo (4)
porque `qsort` trabaja con `void *` y `size_t` — cada swap hace
`memcpy` de `size` bytes. Para structs grandes, el swap cuadrático
puede ser costoso, así que el threshold se mantiene bajo.

### Caso 2: GCC std::sort — threshold = 16

`std::sort` en GCC (introsort) usa 16. Como las comparaciones y
movimientos se compilan directamente (templates, no indirección),
el overhead de merge/quicksort es bajo, y el crossover con insertion
sort ocurre pronto. El valor 16 es conservador pero robusto.

### Caso 3: CPython list.sort — threshold = 64

Timsort en CPython usa 64 porque cada comparación invoca
`PyObject_RichCompareBool`, que es una llamada virtual con overhead de
~100 ns. Con comparaciones tan caras, minimizar su número importa más
que minimizar movimientos. Binary insertion sort reduce comparaciones
a $O(n \log n)$ en el run inicial, lo que compensa el mayor threshold.

### Caso 4: sistemas embebidos

En microcontroladores sin cache (ARM Cortex-M0), la ventaja de cache
de insertion sort desaparece, pero su simplicidad (pocas instrucciones,
sin recursión, sin `malloc`) sigue siendo ventajosa. Para $n < 20$,
insertion sort es la elección estándar en firmware.

---

## Ejercicios

### Ejercicio 1 — Crossover empírico

Encuentra el crossover point exacto en tu máquina entre insertion sort
y merge sort para datos aleatorios. Prueba $n = 1, 2, 3, \ldots, 100$.

<details><summary>Predicción</summary>

Antes de ejecutar: Estima en qué valor de $n$ merge sort empezará a
ganar consistentemente. Piensa en tu hardware — ¿es un procesador de
alto rendimiento o un sistema embebido?

Típicamente, el crossover está entre $n = 20$ y $n = 40$ en hardware
moderno con datos aleatorios y tipo `int`.

</details>

### Ejercicio 2 — Crossover para datos casi ordenados

Repite el ejercicio 1 pero con datos casi ordenados (5% perturbados).
¿El crossover cambia?

<details><summary>Predicción</summary>

Para datos casi ordenados, insertion sort es $O(n)$ (solo hace ~5% de
$n$ shifts). Merge sort sigue siendo $O(n \log n)$ incluso para datos
ordenados. El crossover se moverá **significativamente a la derecha**
— quizás hasta $n = 200$ o más. Esto es exactamente por qué Timsort
busca runs naturales: convierte el problema en muchos subarrays casi
ordenados donde insertion sort brilla.

</details>

### Ejercicio 3 — Threshold óptimo con structs grandes

Modifica el programa para ordenar structs de 256 bytes (array de chars
con un campo clave `int`). ¿Cómo cambia el threshold óptimo?

<details><summary>Predicción</summary>

Con structs de 256 bytes, cada shift en insertion sort mueve 256 bytes
(un `memcpy`). En el peor caso cuadrático, eso son $n^2/2$ movimientos
de 256 bytes cada uno. El threshold óptimo debería **bajar** — quizás a
8-12. Este es el principio detrás del threshold bajo de glibc (4):
`qsort` no sabe el tamaño del elemento, así que asume que puede ser
grande.

Alternativa: ordenar un array de punteros/índices y reordenar al final.

</details>

### Ejercicio 4 — Selection sort para escrituras mínimas

Implementa un programa que cuenta las escrituras a memoria (no
lecturas) de insertion sort vs selection sort para $n = 100$ con
datos aleatorios. Verifica que selection sort hace exactamente
$3(n - 1)$ escrituras en swaps.

<details><summary>Predicción</summary>

Selection sort: $3(n-1) = 297$ escrituras (3 por swap × 99 swaps,
asumiendo que nunca `min_idx == i`; en la práctica algunos swaps se
evitan, así que será $\leq 297$).

Insertion sort: cada shift es 1 escritura + 1 escritura final para
colocar `key`. Peor caso (reversed): $n(n-1)/2 + n - 1 = 4950 + 99 =
5049$ escrituras. Promedio (random): $\approx n^2/4 + n \approx 2600$
escrituras.

Selection sort gana en escrituras por un factor de ~10-17x, pero es
más lento en tiempo porque las escrituras no son el cuello de botella
en hardware moderno (los reads dominan). Solo importa en Flash/EEPROM
donde las escrituras son órdenes de magnitud más lentas.

</details>

### Ejercicio 5 — Binary insertion sort como en Timsort

Implementa binary insertion sort: usa búsqueda binaria para encontrar
la posición de inserción, luego `memmove` para desplazar el bloque.
Compara comparaciones y tiempo vs insertion sort lineal para
$n = 64$ con datos aleatorios.

<details><summary>Predicción</summary>

Binary insertion sort hará $O(n \log n) \approx 64 \times 6 = 384$
comparaciones vs $O(n^2/4) \approx 1024$ de la versión lineal.

Pero los shifts son los mismos: $O(n^2/4)$ movimientos en ambos casos.
El tiempo podría ser similar o incluso peor para la versión binaria
debido al overhead de la búsqueda binaria y el `memmove` (que no puede
hacer early termination como el loop del insertion sort lineal).

Para tipos donde la comparación es cara (strings, objetos con `cmp`
virtual), binary insertion sort gana. Para `int`, es un empate o
pierde ligeramente.

</details>

### Ejercicio 6 — Hybrid quicksort

Implementa quicksort con cutoff a insertion sort. Encuentra el
threshold óptimo para $n = 100000$ con datos aleatorios. Compara
con el threshold óptimo de merge sort.

<details><summary>Predicción</summary>

El threshold óptimo para quicksort suele ser menor que para merge sort
(típicamente 8-20 vs 16-32). Razón: quicksort tiene menos overhead por
llamada que merge sort (no necesita buffer auxiliar ni copia), así que
el punto donde insertion sort supera al quicksort viene antes.

El speedup del hybrid vs pure quicksort será del 10-20%, menor que el
speedup hybrid vs pure merge sort, porque quicksort ya es cache-friendly
y tiene poco overhead por llamada.

</details>

### Ejercicio 7 — Costo real de comparaciones vs movimientos

Crea un tipo `HeavyItem` (struct con 1024 bytes de payload y un campo
`int key`). Ordena 50 elementos con insertion sort vs selection sort.
Mide el tiempo real (no solo conteos).

<details><summary>Predicción</summary>

Con 1024 bytes por movimiento:
- Insertion sort peor caso: $50^2/4 \approx 625$ shifts de 1024 bytes
  cada uno = ~640 KB de datos movidos.
- Selection sort: 49 swaps × 3 × 1024 = ~150 KB de datos movidos.

Selection sort debería ser **significativamente más rápido** para este
caso (quizás 3-4x). Este es uno de los pocos casos reales donde
selection sort justifica su existencia.

Pero la solución correcta es ordenar un array de punteros
(`HeavyItem **`) — ambos algoritmos hacen ~mismo número de movimientos
de 8 bytes (un puntero), y insertion sort gana por ser adaptativo.

</details>

### Ejercicio 8 — Timsort simplificado

Implementa un "mini-Timsort":
1. Divide el array en bloques de 32 elementos.
2. Ordena cada bloque con insertion sort.
3. Fusiona bloques adyacentes con merge.

Compara con merge sort puro para $n = 10000$ con datos casi ordenados.

<details><summary>Predicción</summary>

Para datos casi ordenados, cada bloque de 32 ya estará "casi ordenado",
por lo que insertion sort hará ~$O(32)$ trabajo por bloque en lugar de
$O(32^2)$. El paso de merge procesa $10000/32 \approx 312$ bloques.

El mini-Timsort debería ser **notablemente más rápido** que merge sort
puro para datos casi ordenados (quizás 2-3x), y **comparable** para
datos aleatorios. Esto demuestra por qué Timsort domina en el mundo
real: la mayoría de datos reales tienen algún grado de orden previo.

</details>

### Ejercicio 9 — Shell sort como punto medio

Implementa Shell sort con la secuencia de Knuth ($h = 3h + 1$: 1, 4,
13, 40, 121, ...) y compara con insertion sort y merge sort para
$n = 1000$. ¿Dónde cae Shell sort en la tabla de rendimiento?

<details><summary>Predicción</summary>

Shell sort con secuencia de Knuth tiene complejidad $O(n^{3/2})$.
Para $n = 1000$: $1000^{3/2} = 31623$ operaciones (aproximado).
Insertion sort: $1000^2/4 = 250000$. Merge sort: $1000 \cdot 10 = 10000$.

Shell sort estará **entre** insertion sort y merge sort — mucho mejor
que insertion sort para $n$ mediano, pero peor que merge sort. Es una
opción razonable cuando necesitas algo mejor que cuadrático pero no
quieres la complejidad de implementar merge sort o quicksort (ej:
embedded systems sin `malloc`).

</details>

### Ejercicio 10 — Resumen empírico: tabla de decisión

Ejecuta todos los algoritmos de esta sección (bubble, selection,
insertion, merge, hybrid merge) para $n \in \{10, 50, 100, 500, 1000,
5000, 10000\}$ con datos aleatorios. Construye una tabla que muestre
cuál es el más rápido para cada $n$.

<details><summary>Predicción</summary>

La tabla debería mostrar un patrón claro:

| $n$ | Más rápido |
|-----|-----------|
| 10 | Insertion sort |
| 50 | Insertion sort (apenas) |
| 100 | Hybrid merge o insertion (empate) |
| 500 | Hybrid merge sort |
| 1000 | Hybrid merge sort |
| 5000 | Hybrid merge sort |
| 10000 | Hybrid merge sort |

Observaciones esperadas:
- Bubble sort nunca gana para ningún $n$.
- Selection sort nunca gana (comparable a insertion para aleatorio,
  peor para casi-ordenado).
- Insertion sort puro gana hasta $n \approx 50-100$.
- Hybrid merge sort gana para $n > 100$ y **siempre** supera a merge
  sort puro, confirmando que el cutoff a insertion sort es una
  optimización universal.

</details>
