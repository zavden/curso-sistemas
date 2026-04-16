# Comparación de algoritmos de ordenamiento

## Objetivo

Sintetizar todos los algoritmos de ordenamiento vistos en S02 y S03 en
una comparación integral:

- Tabla maestra: complejidad, estabilidad, espacio, cache, adaptividad.
- Rendimiento empírico con diferentes tipos de entrada y tamaños.
- Guía de decisión: qué algoritmo elegir según el contexto.
- Por qué los algoritmos de producción son **híbridos** (Timsort,
  introsort, pdqsort).

---

## Tabla maestra teórica

### Complejidad temporal

| Algoritmo | Mejor | Promedio | Peor |
|-----------|-------|----------|------|
| Bubble sort | $O(n)$ | $O(n^2)$ | $O(n^2)$ |
| Selection sort | $O(n^2)$ | $O(n^2)$ | $O(n^2)$ |
| Insertion sort | $O(n)$ | $O(n^2)$ | $O(n^2)$ |
| Shell sort (Ciura) | $O(n)$ | $O(n^{4/3})$ aprox | $O(n^{4/3})$ aprox |
| Merge sort | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Quicksort | $O(n \log n)$ | $O(n \log n)$ | $O(n^2)$ |
| Heapsort | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |

### Comparaciones exactas (promedio, datos aleatorios)

| Algoritmo | Comparaciones | Fórmula |
|-----------|--------------|---------|
| Insertion sort | $\sim n^2/4$ | $n(n-1)/4$ |
| Merge sort | $\sim n \log_2 n - 1.26n$ | Óptimo entre comparison sorts |
| Quicksort (Hoare) | $\sim 1.39\, n \log_2 n$ | $2n \ln n \approx 1.386\, n \log_2 n$ |
| Heapsort | $\sim 2n \log_2 n$ | $2n \lceil \log_2 n \rceil$ (peor) |

### Propiedades estructurales

| Algoritmo | Estable | In-place | Adaptativo | Recursivo | `malloc` |
|-----------|---------|----------|------------|-----------|----------|
| Bubble sort | Sí | Sí | Sí (con flag) | No | No |
| Selection sort | **No** | Sí | No | No | No |
| Insertion sort | Sí | Sí | **Sí** | No | No |
| Shell sort | **No** | Sí | Parcial | No | No |
| Merge sort | **Sí** | **No** ($O(n)$) | Con opt. | Sí | Sí |
| Quicksort | **No** | Sí ($O(\log n)$) | No | Sí | No |
| Heapsort | **No** | Sí | **No** | No* | No |

*Heapsort puede implementarse iterativamente.

### Espacio auxiliar

| Algoritmo | Espacio extra |
|-----------|--------------|
| Bubble, Selection, Insertion | $O(1)$ |
| Shell sort | $O(1)$ |
| Heapsort | $O(1)$ |
| Quicksort | $O(\log n)$ stack (con TCO) |
| Quicksort (sin TCO, peor) | $O(n)$ stack |
| Merge sort (top-down) | $O(n)$ buffer + $O(\log n)$ stack |
| Merge sort (bottom-up) | $O(n)$ buffer |

---

## El triángulo imposible

Se desean tres propiedades simultáneamente:

1. **Estable**: preservar orden relativo de iguales.
2. **In-place**: $O(1)$ espacio extra.
3. **$O(n \log n)$ garantizado**: peor caso eficiente.

Ningún algoritmo práctico tiene las tres:

```
                    Estable
                   /       \
          Merge sort       (Block merge sort)*
                 /           \
    O(n log n) ─── ??? ─── In-place
    garantizado \           /
             Heapsort    Quicksort
```

| Algoritmo | Estable | In-place | $O(n \log n)$ garantizado |
|-----------|---------|----------|--------------------------|
| Merge sort | **Sí** | No | **Sí** |
| Quicksort | No | **Sí** | No |
| Heapsort | No | **Sí** | **Sí** |
| Introsort | No | **Sí** | **Sí** |
| Timsort | **Sí** | No ($O(n)$) | **Sí** |

*Block merge sort (Grail sort) logra las tres, pero con constantes
enormes que lo hacen impracticable.

En la práctica se elige según qué propiedad sacrificar:
- Sacrificar espacio → **merge sort** / **Timsort** (estable + garantía).
- Sacrificar estabilidad → **introsort** / **pdqsort** (in-place + garantía).
- Sacrificar garantía → **quicksort** (in-place + muy rápido en promedio).

---

## Rendimiento práctico: cache

La complejidad asintótica no cuenta toda la historia. En hardware
moderno, la **localidad de cache** puede importar más que el conteo
de operaciones.

### Jerarquía de memoria

| Nivel | Tamaño típico | Latencia | Ancho de banda |
|-------|--------------|----------|---------------|
| Registros | 16 × 8 B | ~0.3 ns | — |
| L1 cache | 32-64 KB | ~1 ns | ~500 GB/s |
| L2 cache | 256-512 KB | ~3-5 ns | ~200 GB/s |
| L3 cache | 4-32 MB | ~10-15 ns | ~100 GB/s |
| RAM | 8-64 GB | ~50-100 ns | ~50 GB/s |

Un L3 miss (acceso a RAM) es **50-100x más lento** que un L1 hit.

### Patrón de acceso por algoritmo

| Algoritmo | Patrón | Comportamiento de cache |
|-----------|--------|------------------------|
| Insertion sort | Secuencial hacia atrás | **Excelente** — todo en L1 para $n$ pequeño |
| Merge sort | Secuencial (dos streams) | Bueno — pero buffer compite por cache |
| Quicksort | Secuencial (partición) | **Excelente** — prefetch automático |
| Heapsort | Exponencial (sift-down) | **Terrible** — misses en cada nivel |
| Shell sort | Stride variable | Regular — stride grande = cache misses |

### Mediciones empíricas (x86-64, n = 10^6, int)

| Algoritmo | L1 misses | L3 misses | Tiempo |
|-----------|----------|----------|--------|
| Quicksort (Hoare, m3) | ~3M | ~0.5M | ~65 ms |
| Merge sort (hybrid) | ~8M | ~1M | ~90 ms |
| Heapsort | ~40M | ~15M | ~130 ms |

Quicksort tiene ~5x menos L1 misses que merge sort y ~13x menos que
heapsort. Esto explica por qué quicksort gana pese a hacer más
comparaciones que merge sort.

---

## Rendimiento empírico por tipo de entrada

### Para n = 10000, datos aleatorios

| Algoritmo | Comparaciones | Tiempo relativo |
|-----------|--------------|----------------|
| Insertion sort | ~25M | 30x |
| Shell sort (Ciura) | ~190K | 2.5x |
| Merge sort (hybrid) | ~120K | 1.2x |
| Quicksort (Hoare, m3, cut) | ~170K | **1.0x** (base) |
| Heapsort | ~250K | 2.0x |

### Para n = 10000, datos ya ordenados

| Algoritmo | Comparaciones | Tiempo relativo |
|-----------|--------------|----------------|
| Insertion sort | ~10K ($O(n)$) | **0.05x** |
| Shell sort (Ciura) | ~80K | 1.0x |
| Merge sort (opt.) | ~10K ($O(n)$) | **0.08x** |
| Quicksort (m3, cut) | ~130K | 1.0x |
| Heapsort | ~250K | 2.0x |

### Para n = 10000, todos iguales

| Algoritmo | Comparaciones | Tiempo relativo |
|-----------|--------------|----------------|
| Insertion sort | ~10K ($O(n)$) | **0.05x** |
| Shell sort (Ciura) | ~80K | 1.0x |
| Merge sort (opt.) | ~10K ($O(n)$) | **0.08x** |
| Quicksort (3-way) | ~10K ($O(n)$) | **0.05x** |
| Quicksort (Lomuto) | ~50M ($O(n^2)$) | 600x |
| Heapsort | ~250K | 2.0x |

### Observaciones clave

1. **Quicksort Lomuto con todos iguales** es catastrófico: $O(n^2)$.
   Esto es por qué las implementaciones reales usan partición de
   tres vías o Hoare.

2. **Insertion sort y merge sort optimizado** son $O(n)$ para datos
   ordenados. Heapsort siempre hace $O(n \log n)$.

3. **Para datos aleatorios**, quicksort (Hoare + mediana de tres +
   cutoff) es el más rápido por su excelente cache behavior.

4. **Heapsort** es consistentemente el más lento de los $O(n \log n)$
   por su patrón de cache exponencial.

---

## Algoritmos de producción: los híbridos

Ningún algoritmo simple es el mejor en todos los escenarios. Los
algoritmos usados en producción combinan varios para cubrir todas las
situaciones.

### Timsort (estable)

**Usado en**: Python `list.sort`, Java `Arrays.sort` (objetos),
Rust `slice::sort`, Android.

| Componente | Rol |
|-----------|-----|
| Run detection | Encontrar subsecuencias ya ordenadas ($O(n)$) |
| Binary insertion sort | Extender runs cortos ($n < 32$-$64$) |
| Merge con galloping | Fusionar runs eficientemente |
| Run stack con invariantes | Garantizar $O(n \log n)$ merges |

Complejidad:
- Mejor: $O(n)$ (ya ordenado — un solo run).
- Promedio: $O(n \log n)$.
- Peor: $O(n \log n)$.
- Espacio: $O(n)$.
- Estable: **sí**.

### Introsort (no estable)

**Usado en**: GCC `std::sort`, MSVC `std::sort`, .NET `Array.Sort`.

| Componente | Rol |
|-----------|-----|
| Quicksort (mediana de 3) | Caso general, rápido |
| Heapsort | Fallback si profundidad $> 2 \lfloor \log_2 n \rfloor$ |
| Insertion sort | Subarrays con $n \leq 16$ |

Complejidad:
- Mejor: $O(n \log n)$.
- Promedio: $O(n \log n)$.
- Peor: $O(n \log n)$ (heapsort garantiza).
- Espacio: $O(\log n)$.
- Estable: **no**.

### pdqsort (no estable)

**Usado en**: Rust `slice::sort_unstable`, Boost.Sort, Go.

| Componente | Rol |
|-----------|-----|
| Quicksort (block partition) | Caso general, mejor branch prediction |
| Heapsort | Fallback si pivotes adversariales |
| Insertion sort | Subarrays pequeños ($n \leq 24$) |
| Run detection | $O(n)$ para datos ordenados |
| Dutch National Flag | $O(n)$ para pocos valores únicos |
| Shuffle parcial | Destruir patrones adversariales |

Complejidad:
- Mejor: $O(n)$ (ordenado, todos iguales).
- Promedio: $O(n \log n)$.
- Peor: $O(n \log n)$ (heapsort garantiza).
- Espacio: $O(\log n)$.
- Estable: **no**.

### Comparación de los híbridos

| Entrada | Timsort | Introsort | pdqsort |
|---------|---------|----------|---------|
| Aleatorio | 1.0x | 0.9x | **0.85x** |
| Ordenado | **~$O(n)$** | $O(n \log n)$ | **~$O(n)$** |
| Casi ordenado | **~$O(n)$** | $O(n \log n)$ | **~$O(n)$** |
| Reverso | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Pocos únicos | $O(n \log n)$ | $O(n \log n)$ | **~$O(n \log k)$** |
| Todos iguales | **$O(n)$** | $O(n \log n)$ | **$O(n)$** |
| Estable | **Sí** | No | No |
| Espacio | $O(n)$ | $O(\log n)$ | $O(\log n)$ |

pdqsort es el más versátil para sort no estable. Timsort es el mejor
para sort estable y datos con estructura (runs, datos parcialmente
ordenados).

---

## Guía de decisión

```
¿Necesitas estabilidad?
├── Sí
│   ├── ¿Tienes O(n) espacio extra?
│   │   ├── Sí → TIMSORT (merge sort + runs)
│   │   └── No → Insertion sort (n<50) o aceptar O(n) espacio
│   └── (No hay sort estable in-place práctico)
│
└── No
    ├── ¿Tienes malloc/recursión disponibles?
    │   ├── Sí → PDQSORT/INTROSORT
    │   └── No (embebido)
    │       ├── n < 50 → INSERTION SORT
    │       ├── n < 5000 → SHELL SORT (Ciura)
    │       └── n > 5000 → HEAPSORT (O(n log n) garantizado sin malloc/recursión)
    │
    ├── ¿Datos probablemente con estructura (casi ordenados, runs)?
    │   ├── Sí → TIMSORT (detecta y aprovecha)
    │   └── No → PDQSORT/INTROSORT
    │
    └── ¿Necesitas WCET predecible?
        ├── Sí → HEAPSORT (siempre exactamente O(n log n))
        └── No → PDQSORT
```

### Reglas simples

1. **Usa la función de biblioteca** (`qsort` en C, `.sort()` en Rust)
   a menos que tengas una razón concreta para no hacerlo. Están
   optimizadas y testeadas.

2. Si escribes tu propio sort:
   - **$n < 20$**: insertion sort.
   - **$n < 1000$ sin `malloc`**: Shell sort (Ciura).
   - **Necesitas estabilidad**: merge sort con cutoff a insertion sort.
   - **Necesitas velocidad máxima**: quicksort con mediana de tres +
     cutoff + three-way + depth limit (= introsort/pdqsort).

3. **Nunca uses bubble sort** en producción.

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
    long moves;     /* shifts, swaps×3, or copies */
} Metrics;

static void swap_int(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

/* ── Algorithms ──────────────────────────────────────────── */

void insertion_sort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    for (int i = 1; i < n; i++) {
        int key = a[i]; int j = i - 1;
        while (j >= 0 && (m->comparisons++, a[j] > key)) {
            a[j + 1] = a[j]; m->moves++; j--;
        }
        a[j + 1] = key; m->moves++;
    }
}

void selection_sort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    for (int i = 0; i < n - 1; i++) {
        int mi = i;
        for (int j = i + 1; j < n; j++) {
            m->comparisons++;
            if (a[j] < a[mi]) mi = j;
        }
        if (mi != i) { swap_int(&a[i], &a[mi]); m->moves += 3; }
    }
}

void bubble_sort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    for (int i = 0; i < n - 1; i++) {
        int sw = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            m->comparisons++;
            if (a[j] > a[j + 1]) { swap_int(&a[j], &a[j + 1]); m->moves += 3; sw = 1; }
        }
        if (!sw) break;
    }
}

void shell_sort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    int ciura[] = {701, 301, 132, 57, 23, 10, 4, 1};
    for (int g = 0; g < 8; g++) {
        int h = ciura[g];
        if (h >= n) continue;
        for (int i = h; i < n; i++) {
            int key = a[i]; int j = i;
            while (j >= h && (m->comparisons++, a[j - h] > key)) {
                a[j] = a[j - h]; m->moves++; j -= h;
            }
            a[j] = key; m->moves++;
        }
    }
}

/* Merge sort (hybrid with cutoff) */
static void ms_ins(int *a, int lo, int hi, Metrics *m) {
    for (int i = lo + 1; i <= hi; i++) {
        int key = a[i]; int j = i - 1;
        while (j >= lo && (m->comparisons++, a[j] > key)) {
            a[j + 1] = a[j]; m->moves++; j--;
        }
        a[j + 1] = key; m->moves++;
    }
}

static void ms_merge(int *a, int *t, int lo, int mid, int hi, Metrics *m) {
    memcpy(t + lo, a + lo, (hi - lo + 1) * sizeof(int));
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        m->comparisons++;
        if (t[i] <= t[j]) a[k++] = t[i++]; else a[k++] = t[j++];
        m->moves++;
    }
    while (i <= mid) { a[k++] = t[i++]; m->moves++; }
    while (j <= hi)  { a[k++] = t[j++]; m->moves++; }
}

static void ms_r(int *a, int *t, int lo, int hi, Metrics *m) {
    if (hi - lo + 1 <= 16) { ms_ins(a, lo, hi, m); return; }
    int mid = lo + (hi - lo) / 2;
    ms_r(a, t, lo, mid, m);
    ms_r(a, t, mid + 1, hi, m);
    m->comparisons++;
    if (a[mid] <= a[mid + 1]) return;
    ms_merge(a, t, lo, mid, hi, m);
}

void merge_sort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    int *t = malloc(n * sizeof(int));
    ms_r(a, t, 0, n - 1, m);
    free(t);
}

/* Quicksort (Hoare + median3 + cutoff + 3-way) */
static void qs_3way(int *a, int lo, int hi, int depth, Metrics *m);

static void qs_hs_sift(int *a, int lo, int i, int n, Metrics *m) {
    int v = a[lo + i];
    while (2 * i + 1 < n) {
        int c = 2 * i + 1;
        if (c + 1 < n) { m->comparisons++; if (a[lo+c+1] > a[lo+c]) c++; }
        m->comparisons++;
        if (a[lo + c] <= v) break;
        a[lo + i] = a[lo + c]; m->moves++; i = c;
    }
    a[lo + i] = v;
}

static void qs_heapsort(int *a, int lo, int hi, Metrics *m) {
    int n = hi - lo + 1;
    for (int i = n / 2 - 1; i >= 0; i--) qs_hs_sift(a, lo, i, n, m);
    for (int i = n - 1; i > 0; i--) {
        swap_int(&a[lo], &a[lo + i]); m->moves += 3;
        qs_hs_sift(a, lo, 0, i, m);
    }
}

static int qs_floor_log2(int n) { int k = 0; while (n > 1) { n /= 2; k++; } return k; }

static void qs_3way(int *a, int lo, int hi, int depth, Metrics *m) {
    if (hi - lo + 1 <= 16) { ms_ins(a, lo, hi, m); return; }
    if (depth == 0) { qs_heapsort(a, lo, hi, m); return; }

    /* Three-way partition */
    int pivot = a[lo]; int lt = lo, gt = hi, i = lo + 1;
    while (i <= gt) {
        m->comparisons++;
        if (a[i] < pivot) { swap_int(&a[lt], &a[i]); m->moves += 3; lt++; i++; }
        else if (a[i] > pivot) { m->comparisons++; swap_int(&a[i], &a[gt]); m->moves += 3; gt--; }
        else i++;
    }
    if (lt > lo) qs_3way(a, lo, lt - 1, depth - 1, m);
    qs_3way(a, gt + 1, hi, depth - 1, m);
}

void quicksort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    qs_3way(a, 0, n - 1, 2 * qs_floor_log2(n), m);
}

/* Heapsort standalone */
void heapsort(int *a, int n, Metrics *m) {
    m->comparisons = 0; m->moves = 0;
    for (int i = n / 2 - 1; i >= 0; i--) qs_hs_sift(a, 0, i, n, m);
    for (int i = n - 1; i > 0; i--) {
        swap_int(&a[0], &a[i]); m->moves += 3;
        qs_hs_sift(a, 0, 0, i, m);
    }
}

/* ── Input generators ────────────────────────────────────── */

void fill_random(int *a, int n) { for (int i = 0; i < n; i++) a[i] = rand() % (n * 10); }
void fill_sorted(int *a, int n) { for (int i = 0; i < n; i++) a[i] = i; }
void fill_reversed(int *a, int n) { for (int i = 0; i < n; i++) a[i] = n - i; }
void fill_nearly_sorted(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    for (int k = 0; k < n / 20; k++) { int x = rand()%n, y = rand()%n; swap_int(&a[x], &a[y]); }
}
void fill_few_unique(int *a, int n) { for (int i = 0; i < n; i++) a[i] = rand() % 10; }
void fill_pipe_organ(int *a, int n) {
    for (int i = 0; i < n / 2; i++) a[i] = i;
    for (int i = n / 2; i < n; i++) a[i] = n - i;
}

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

/* ── Benchmark ───────────────────────────────────────────── */

typedef void (*SortFn)(int *, int, Metrics *);

typedef struct { const char *name; SortFn fn; int max_n; } SortEntry;

void bench(const char *sname, SortFn fn, int *orig, int n) {
    int *w = malloc(n * sizeof(int));
    memcpy(w, orig, n * sizeof(int));
    Metrics m;
    clock_t s = clock();
    fn(w, n, &m);
    clock_t e = clock();
    double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;
    printf("  %-18s %12ld cmp %12ld mov %10.0f us  %s\n",
           sname, m.comparisons, m.moves, us,
           is_sorted(w, n) ? "OK" : "FAIL");
    free(w);
}

/* ── Demo 1: Full comparison table ───────────────────────── */

void demo_full(int n) {
    printf("=== Full comparison: n = %d ===\n\n", n);

    typedef struct { const char *name; void (*fill)(int *, int); } Input;
    Input inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Reversed     ", fill_reversed},
        {"Nearly sorted", fill_nearly_sorted},
        {"Few unique   ", fill_few_unique},
        {"Pipe organ   ", fill_pipe_organ},
    };

    SortEntry sorts[] = {
        {"Bubble sort",    bubble_sort,    5000},
        {"Selection sort", selection_sort, 10000},
        {"Insertion sort", insertion_sort, 10000},
        {"Shell sort",     shell_sort,     0},
        {"Merge sort",     merge_sort,     0},
        {"Quicksort",      quicksort,      0},
        {"Heapsort",       heapsort,       0},
    };
    int ns = 7;

    for (int t = 0; t < 6; t++) {
        int *orig = malloc(n * sizeof(int));
        inputs[t].fill(orig, n);
        printf("--- %s ---\n", inputs[t].name);

        for (int s = 0; s < ns; s++) {
            if (sorts[s].max_n > 0 && n > sorts[s].max_n) {
                printf("  %-18s (skipped: too slow for n=%d)\n", sorts[s].name, n);
                continue;
            }
            bench(sorts[s].name, sorts[s].fn, orig, n);
        }
        printf("\n");
        free(orig);
    }
}

/* ── Demo 2: Scaling behavior ────────────────────────────── */

void demo_scaling(void) {
    printf("=== Scaling: time(us) for random data ===\n\n");
    printf("%8s %10s %10s %10s %10s %10s\n",
           "n", "Insertion", "Shell", "Merge", "Quick", "Heap");
    printf("%8s %10s %10s %10s %10s %10s\n",
           "────────", "──────────", "──────────",
           "──────────", "──────────", "──────────");

    int sizes[] = {100, 500, 1000, 5000, 10000, 50000};
    SortFn fns[] = {insertion_sort, shell_sort, merge_sort, quicksort, heapsort};
    const char *names[] = {"ins", "shell", "merge", "quick", "heap"};

    for (int si = 0; si < 6; si++) {
        int n = sizes[si];
        int *orig = malloc(n * sizeof(int));
        fill_random(orig, n);
        printf("%8d", n);

        for (int f = 0; f < 5; f++) {
            if (f == 0 && n > 10000) { printf(" %10s", "skip"); continue; }
            int *w = malloc(n * sizeof(int));
            memcpy(w, orig, n * sizeof(int));
            Metrics m;
            clock_t s = clock();
            fns[f](w, n, &m);
            clock_t e = clock();
            double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;
            printf(" %10.0f", us);
            free(w);
        }
        printf("\n");
        free(orig);
    }
    printf("\n");
}

/* ── Demo 3: Stability check ────────────────────────────── */

typedef struct { int value; int orig_idx; } Pair;

int cmp_pair(const Pair *a, const Pair *b) { return a->value - b->value; }

void check_stability(const char *name, Pair *sorted, int n) {
    int stable = 1;
    for (int i = 1; i < n; i++) {
        if (sorted[i].value == sorted[i-1].value &&
            sorted[i].orig_idx < sorted[i-1].orig_idx) {
            stable = 0; break;
        }
    }
    printf("  %-18s Stable: %s\n", name, stable ? "YES" : "NO");
}

void demo_stability(void) {
    printf("=== Stability test (10 values, n=100) ===\n\n");
    int n = 100;
    Pair orig[100];
    for (int i = 0; i < n; i++) { orig[i].value = i % 10; orig[i].orig_idx = i; }

    /* Insertion sort */
    {
        Pair w[100]; memcpy(w, orig, sizeof(w));
        for (int i = 1; i < n; i++) {
            Pair key = w[i]; int j = i - 1;
            while (j >= 0 && w[j].value > key.value) { w[j+1] = w[j]; j--; }
            w[j+1] = key;
        }
        check_stability("Insertion sort", w, n);
    }

    /* Selection sort */
    {
        Pair w[100]; memcpy(w, orig, sizeof(w));
        for (int i = 0; i < n - 1; i++) {
            int mi = i;
            for (int j = i + 1; j < n; j++) if (w[j].value < w[mi].value) mi = j;
            if (mi != i) { Pair t = w[i]; w[i] = w[mi]; w[mi] = t; }
        }
        check_stability("Selection sort", w, n);
    }

    /* Merge sort (via qsort is not guaranteed stable, do manual) */
    /* Use stdlib sort as Timsort proxy if available — simplify: manual merge */
    {
        Pair w[100]; memcpy(w, orig, sizeof(w));
        /* Simple merge sort for Pair */
        Pair tmp[100];
        for (int width = 1; width < n; width *= 2) {
            for (int lo = 0; lo < n; lo += 2 * width) {
                int mid = lo + width - 1;
                int hi = lo + 2 * width - 1;
                if (mid >= n) break;
                if (hi >= n) hi = n - 1;
                memcpy(tmp + lo, w + lo, (hi - lo + 1) * sizeof(Pair));
                int a = lo, b = mid + 1, k = lo;
                while (a <= mid && b <= hi) {
                    if (tmp[a].value <= tmp[b].value) w[k++] = tmp[a++];
                    else w[k++] = tmp[b++];
                }
                while (a <= mid) w[k++] = tmp[a++];
                while (b <= hi)  w[k++] = tmp[b++];
            }
        }
        check_stability("Merge sort", w, n);
    }

    /* Quicksort (Lomuto) */
    {
        Pair w[100]; memcpy(w, orig, sizeof(w));
        /* Simple Lomuto quicksort */
        void qs_pair(Pair *a, int lo, int hi);
        qs_pair(w, 0, n - 1);
        check_stability("Quicksort", w, n);
    }

    /* Heapsort */
    {
        Pair w[100]; memcpy(w, orig, sizeof(w));
        for (int i = n/2 - 1; i >= 0; i--) {
            int pos = i; Pair v = w[pos];
            while (2*pos+1 < n) {
                int c = 2*pos+1;
                if (c+1 < n && w[c+1].value > w[c].value) c++;
                if (w[c].value <= v.value) break;
                w[pos] = w[c]; pos = c;
            }
            w[pos] = v;
        }
        for (int i = n-1; i > 0; i--) {
            Pair t = w[0]; w[0] = w[i]; w[i] = t;
            int pos = 0; Pair v = w[0];
            while (2*pos+1 < i) {
                int c = 2*pos+1;
                if (c+1 < i && w[c+1].value > w[c].value) c++;
                if (w[c].value <= v.value) break;
                w[pos] = w[c]; pos = c;
            }
            w[pos] = v;
        }
        check_stability("Heapsort", w, n);
    }
    printf("\n");
}

void qs_pair(Pair *a, int lo, int hi) {
    if (lo >= hi) return;
    Pair pivot = a[hi]; int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (a[j].value <= pivot.value) {
            i++; Pair t = a[i]; a[i] = a[j]; a[j] = t;
        }
    }
    Pair t = a[i+1]; a[i+1] = a[hi]; a[hi] = t;
    int p = i + 1;
    if (p > 0) qs_pair(a, lo, p - 1);
    qs_pair(a, p + 1, hi);
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   Sorting Algorithms — Comprehensive Comparison       ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    demo_stability();
    demo_scaling();
    demo_full(1000);
    demo_full(10000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o sort_comparison sort_comparison.c
./sort_comparison
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── Metrics ─────────────────────────────────────────────── */

#[derive(Default, Clone)]
struct Metrics {
    comparisons: u64,
    moves: u64,
}

/* ── Sorting algorithms ──────────────────────────────────── */

fn insertion_sort(a: &mut [i32], m: &mut Metrics) {
    for i in 1..a.len() {
        let key = a[i]; let mut j = i;
        while j > 0 { m.comparisons += 1; if a[j-1] <= key { break; } a[j] = a[j-1]; m.moves += 1; j -= 1; }
        a[j] = key; m.moves += 1;
    }
}

fn selection_sort(a: &mut [i32], m: &mut Metrics) {
    let n = a.len();
    for i in 0..n.saturating_sub(1) {
        let mut mi = i;
        for j in (i+1)..n { m.comparisons += 1; if a[j] < a[mi] { mi = j; } }
        if mi != i { a.swap(i, mi); m.moves += 3; }
    }
}

fn bubble_sort(a: &mut [i32], m: &mut Metrics) {
    let n = a.len();
    for i in 0..n.saturating_sub(1) {
        let mut sw = false;
        for j in 0..(n-1-i) {
            m.comparisons += 1;
            if a[j] > a[j+1] { a.swap(j, j+1); m.moves += 3; sw = true; }
        }
        if !sw { break; }
    }
}

fn shell_sort(a: &mut [i32], m: &mut Metrics) {
    let ciura = [701, 301, 132, 57, 23, 10, 4, 1];
    let n = a.len();
    for &h in &ciura {
        if h >= n { continue; }
        for i in h..n {
            let key = a[i]; let mut j = i;
            while j >= h { m.comparisons += 1; if a[j-h] <= key { break; } a[j] = a[j-h]; m.moves += 1; j -= h; }
            a[j] = key; m.moves += 1;
        }
    }
}

fn ms_ins(a: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    for i in (lo+1)..=hi {
        let key = a[i]; let mut j = i;
        while j > lo { m.comparisons += 1; if a[j-1] <= key { break; } a[j] = a[j-1]; m.moves += 1; j -= 1; }
        a[j] = key; m.moves += 1;
    }
}

fn ms_merge(a: &mut [i32], t: &mut [i32], lo: usize, mid: usize, hi: usize, m: &mut Metrics) {
    t[lo..=hi].copy_from_slice(&a[lo..=hi]);
    let (mut i, mut j, mut k) = (lo, mid+1, lo);
    while i <= mid && j <= hi {
        m.comparisons += 1;
        if t[i] <= t[j] { a[k] = t[i]; i += 1; } else { a[k] = t[j]; j += 1; }
        m.moves += 1; k += 1;
    }
    while i <= mid { a[k] = t[i]; m.moves += 1; i += 1; k += 1; }
    while j <= hi  { a[k] = t[j]; m.moves += 1; j += 1; k += 1; }
}

fn ms_r(a: &mut [i32], t: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if hi - lo + 1 <= 16 { ms_ins(a, lo, hi, m); return; }
    let mid = lo + (hi - lo) / 2;
    ms_r(a, t, lo, mid, m);
    ms_r(a, t, mid+1, hi, m);
    m.comparisons += 1;
    if a[mid] <= a[mid+1] { return; }
    ms_merge(a, t, lo, mid, hi, m);
}

fn merge_sort(a: &mut [i32], m: &mut Metrics) {
    let n = a.len(); if n <= 1 { return; }
    let mut t = vec![0i32; n];
    ms_r(a, &mut t, 0, n-1, m);
}

fn hs_sift(a: &mut [i32], lo: usize, mut i: usize, n: usize, m: &mut Metrics) {
    let v = a[lo+i];
    while 2*i+1 < n {
        let mut c = 2*i+1;
        if c+1 < n { m.comparisons += 1; if a[lo+c+1] > a[lo+c] { c += 1; } }
        m.comparisons += 1;
        if a[lo+c] <= v { break; }
        a[lo+i] = a[lo+c]; m.moves += 1; i = c;
    }
    a[lo+i] = v;
}

fn hs_range(a: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    let n = hi - lo + 1;
    for i in (0..n/2).rev() { hs_sift(a, lo, i, n, m); }
    for i in (1..n).rev() { a.swap(lo, lo+i); m.moves += 3; hs_sift(a, lo, 0, i, m); }
}

fn heapsort(a: &mut [i32], m: &mut Metrics) {
    let n = a.len(); if n <= 1 { return; }
    hs_range(a, 0, n-1, m);
}

fn floor_log2(n: usize) -> usize { let mut k = 0; let mut x = n; while x > 1 { x /= 2; k += 1; } k }

fn qs_3way(a: &mut [i32], lo: usize, hi: usize, depth: usize, m: &mut Metrics) {
    if hi - lo + 1 <= 16 { ms_ins(a, lo, hi, m); return; }
    if depth == 0 { hs_range(a, lo, hi, m); return; }
    let pivot = a[lo]; let mut lt = lo; let mut gt = hi; let mut i = lo + 1;
    while i <= gt {
        m.comparisons += 1;
        if a[i] < pivot { a.swap(lt, i); m.moves += 3; lt += 1; i += 1; }
        else if a[i] > pivot { m.comparisons += 1; a.swap(i, gt); m.moves += 3; gt -= 1; }
        else { i += 1; }
    }
    if lt > lo { qs_3way(a, lo, lt-1, depth-1, m); }
    qs_3way(a, gt+1, hi, depth-1, m);
}

fn quicksort(a: &mut [i32], m: &mut Metrics) {
    let n = a.len(); if n <= 1 { return; }
    qs_3way(a, 0, n-1, 2 * floor_log2(n), m);
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| { let mut h = DefaultHasher::new(); (i as u64 ^ 0xC0FFEE).hash(&mut h); (h.finish() % (n as u64 * 10)) as i32 }).collect()
}
fn fill_sorted(n: usize) -> Vec<i32> { (0..n as i32).collect() }
fn fill_reversed(n: usize) -> Vec<i32> { (0..n as i32).rev().collect() }
fn fill_nearly_sorted(n: usize) -> Vec<i32> {
    let mut v: Vec<i32> = (0..n as i32).collect();
    let p = (n/20).max(1);
    for k in 0..p { let i = (k*7+13)%n; let j = (k*11+3)%n; v.swap(i,j); }
    v
}
fn fill_few_unique(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| { let mut h = DefaultHasher::new(); i.hash(&mut h); (h.finish() % 10) as i32 }).collect()
}
fn fill_pipe_organ(n: usize) -> Vec<i32> {
    (0..n).map(|i| if i < n/2 { i as i32 } else { (n - i) as i32 }).collect()
}

fn is_sorted(a: &[i32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }

/* ── Benchmark ───────────────────────────────────────────── */

fn bench(name: &str, orig: &[i32], sort_fn: fn(&mut [i32], &mut Metrics)) {
    let mut w = orig.to_vec();
    let mut m = Metrics::default();
    let start = Instant::now();
    sort_fn(&mut w, &mut m);
    let elapsed = start.elapsed().as_micros();
    let ok = if is_sorted(&w) { "OK" } else { "FAIL" };
    println!("  {:<18} {:>12} cmp {:>12} mov {:>10} us  {}",
        name, m.comparisons, m.moves, elapsed, ok);
}

/* ── Demo 1: Full comparison ─────────────────────────────── */

fn demo_full(n: usize) {
    println!("=== Full comparison: n = {} ===\n", n);

    let names = ["Random", "Sorted", "Reversed", "Nearly sorted", "Few unique", "Pipe organ"];
    let gens: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_sorted, fill_reversed, fill_nearly_sorted, fill_few_unique, fill_pipe_organ,
    ];

    let sort_names = ["Bubble sort", "Selection sort", "Insertion sort",
                      "Shell sort", "Merge sort", "Quicksort", "Heapsort"];
    let sort_fns: Vec<fn(&mut [i32], &mut Metrics)> = vec![
        bubble_sort, selection_sort, insertion_sort,
        shell_sort, merge_sort, quicksort, heapsort,
    ];
    let max_n: Vec<usize> = vec![5000, 10000, 10000, 0, 0, 0, 0];

    for (ti, gen) in gens.iter().enumerate() {
        let orig = gen(n);
        println!("--- {} ---", names[ti]);
        for (si, &sfn) in sort_fns.iter().enumerate() {
            if max_n[si] > 0 && n > max_n[si] {
                println!("  {:<18} (skipped: too slow)", sort_names[si]);
                continue;
            }
            bench(sort_names[si], &orig, sfn);
        }
        println!();
    }
}

/* ── Demo 2: Scaling ─────────────────────────────────────── */

fn demo_scaling() {
    println!("=== Scaling: time(us) for random data ===\n");
    println!("{:>8} {:>10} {:>10} {:>10} {:>10} {:>10}",
        "n", "Insertion", "Shell", "Merge", "Quick", "Heap");
    println!("{:>8} {:>10} {:>10} {:>10} {:>10} {:>10}",
        "────────", "──────────", "──────────", "──────────", "──────────", "──────────");

    let sort_fns: Vec<fn(&mut [i32], &mut Metrics)> = vec![
        insertion_sort, shell_sort, merge_sort, quicksort, heapsort,
    ];

    for &n in &[100, 500, 1000, 5000, 10000, 50000] {
        let orig = fill_random(n);
        print!("{:>8}", n);
        for (fi, &sfn) in sort_fns.iter().enumerate() {
            if fi == 0 && n > 10000 { print!(" {:>10}", "skip"); continue; }
            let mut w = orig.clone();
            let mut m = Metrics::default();
            let start = Instant::now();
            sfn(&mut w, &mut m);
            let elapsed = start.elapsed().as_micros();
            print!(" {:>10}", elapsed);
        }
        println!();
    }
    println!();
}

/* ── Demo 3: Stability ──────────────────────────────────── */

fn demo_stability() {
    println!("=== Stability test ===\n");

    let n = 100;
    let orig: Vec<(i32, usize)> = (0..n).map(|i| (i as i32 % 10, i)).collect();

    /* Insertion sort (stable) */
    {
        let mut w = orig.clone();
        for i in 1..n {
            let key = w[i]; let mut j = i;
            while j > 0 && w[j-1].0 > key.0 { w[j] = w[j-1]; j -= 1; }
            w[j] = key;
        }
        let stable = w.windows(2).all(|p| p[0].0 != p[1].0 || p[0].1 < p[1].1);
        println!("  Insertion sort     Stable: {}", if stable { "YES" } else { "NO" });
    }

    /* Merge sort (stable via .sort_by) */
    {
        let mut w = orig.clone();
        w.sort_by(|a, b| a.0.cmp(&b.0));
        let stable = w.windows(2).all(|p| p[0].0 != p[1].0 || p[0].1 < p[1].1);
        println!("  Merge sort (std)   Stable: {}", if stable { "YES" } else { "NO" });
    }

    /* sort_unstable (pdqsort, not stable) */
    {
        let mut w = orig.clone();
        w.sort_unstable_by(|a, b| a.0.cmp(&b.0));
        let stable = w.windows(2).all(|p| p[0].0 != p[1].0 || p[0].1 < p[1].1);
        println!("  sort_unstable      Stable: {}", if stable { "YES" } else { "NO" });
    }

    /* Selection sort (not stable) */
    {
        let mut w = orig.clone();
        for i in 0..n-1 {
            let mut mi = i;
            for j in (i+1)..n { if w[j].0 < w[mi].0 { mi = j; } }
            if mi != i { w.swap(i, mi); }
        }
        let stable = w.windows(2).all(|p| p[0].0 != p[1].0 || p[0].1 < p[1].1);
        println!("  Selection sort     Stable: {}", if stable { "YES" } else { "NO" });
    }

    /* Heapsort (not stable) */
    {
        let mut w = orig.clone();
        let n = w.len();
        for i in (0..n/2).rev() {
            let mut pos = i; let v = w[pos];
            while 2*pos+1 < n {
                let mut c = 2*pos+1;
                if c+1 < n && w[c+1].0 > w[c].0 { c += 1; }
                if w[c].0 <= v.0 { break; }
                w[pos] = w[c]; pos = c;
            }
            w[pos] = v;
        }
        for i in (1..n).rev() {
            w.swap(0, i);
            let mut pos = 0; let v = w[0];
            while 2*pos+1 < i {
                let mut c = 2*pos+1;
                if c+1 < i && w[c+1].0 > w[c].0 { c += 1; }
                if w[c].0 <= v.0 { break; }
                w[pos] = w[c]; pos = c;
            }
            w[pos] = v;
        }
        let stable = w.windows(2).all(|p| p[0].0 != p[1].0 || p[0].1 < p[1].1);
        println!("  Heapsort           Stable: {}", if stable { "YES" } else { "NO" });
    }
    println!();
}

/* ── Demo 4: stdlib comparison ───────────────────────────── */

fn demo_stdlib(n: usize) {
    println!("=== stdlib sort vs manual, n = {} ===\n", n);

    let orig = fill_random(n);

    /* std sort (Timsort) */
    {
        let mut w = orig.clone();
        let start = Instant::now();
        w.sort();
        let elapsed = start.elapsed().as_micros();
        println!("  {:<18} {:>10} us  {}", ".sort() (Timsort)", elapsed,
            if is_sorted(&w) { "OK" } else { "FAIL" });
    }

    /* std sort_unstable (pdqsort) */
    {
        let mut w = orig.clone();
        let start = Instant::now();
        w.sort_unstable();
        let elapsed = start.elapsed().as_micros();
        println!("  {:<18} {:>10} us  {}", ".sort_unstable()", elapsed,
            if is_sorted(&w) { "OK" } else { "FAIL" });
    }

    /* Our quicksort */
    {
        let mut w = orig.clone();
        let mut m = Metrics::default();
        let start = Instant::now();
        quicksort(&mut w, &mut m);
        let elapsed = start.elapsed().as_micros();
        println!("  {:<18} {:>10} us  {}", "Our quicksort", elapsed,
            if is_sorted(&w) { "OK" } else { "FAIL" });
    }

    /* Our merge sort */
    {
        let mut w = orig.clone();
        let mut m = Metrics::default();
        let start = Instant::now();
        merge_sort(&mut w, &mut m);
        let elapsed = start.elapsed().as_micros();
        println!("  {:<18} {:>10} us  {}", "Our merge sort", elapsed,
            if is_sorted(&w) { "OK" } else { "FAIL" });
    }
    println!();
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔════════════════════════════════════════════════════════╗");
    println!("║   Sorting Algorithms — Comprehensive Comparison       ║");
    println!("╚════════════════════════════════════════════════════════╝\n");

    demo_stability();
    demo_scaling();
    demo_full(1000);
    demo_full(10000);
    demo_stdlib(100_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o sort_cmp_rs sort_comparison.rs
./sort_cmp_rs
```

---

## Ejercicios

### Ejercicio 1 — Tabla empírica completa

Ejecuta los 7 algoritmos (bubble, selection, insertion, shell, merge,
quick, heap) para $n = 1000$ con las 6 entradas (random, sorted,
reversed, nearly sorted, few unique, pipe organ). Construye la tabla
de comparaciones y tiempo.

<details><summary>Predicción</summary>

La tabla debería confirmar las predicciones teóricas:

| Input | Ganador (cmp) | Ganador (tiempo) |
|-------|--------------|-----------------|
| Random | Merge sort | Quicksort |
| Sorted | Insertion sort ($n$) | Insertion sort |
| Reversed | Merge sort | Quicksort |
| Nearly sorted | Insertion sort (~$n$) | Insertion sort |
| Few unique | Quicksort 3-way | Quicksort 3-way |
| Pipe organ | Merge sort | Quicksort |

Observaciones clave:
- Merge sort siempre gana en comparaciones (tiene el mínimo teórico).
- Quicksort siempre gana en tiempo (mejor cache).
- Los cuadráticos solo ganan para datos ya ordenados y $n$ pequeño.
- Heapsort nunca gana en ninguna categoría.

</details>

### Ejercicio 2 — Crossover empírico entre clases

Encuentra los valores de $n$ donde:
a) Shell sort supera a insertion sort.
b) Merge sort supera a Shell sort.
c) Quicksort supera a merge sort.

<details><summary>Predicción</summary>

a) **Insertion vs Shell**: crossover ~$n = 30$-$50$. Por debajo, insertion
   sort gana por menor overhead. Por encima, la diferencia crece
   rápidamente ($n^2$ vs $n^{4/3}$).

b) **Shell vs Merge**: crossover ~$n = 500$-$2000$. Shell sort
   ($n^{4/3}$) supera a merge sort ($n \log n$) para $n$ pequeño por
   no tener overhead de recursión ni `malloc`. Para $n$ grande, merge
   sort gana.

c) **Merge vs Quick**: quicksort es más rápido para **todo** $n$
   (excepto datos ordenados). No hay crossover — quicksort tiene mejor
   constante por cache. Sin embargo, merge sort hace menos comparaciones
   para todo $n$.

</details>

### Ejercicio 3 — Estabilidad: verificación exhaustiva

Genera 1000 arrays aleatorios de $n = 50$ con valores en $[0, 5]$.
Para cada array, ordena con los 7 algoritmos y verifica estabilidad.
¿Cuántos arrays violan estabilidad para cada algoritmo?

<details><summary>Predicción</summary>

| Algoritmo | Arrays estables / 1000 |
|-----------|----------------------|
| Bubble sort | 1000/1000 (siempre estable) |
| Insertion sort | 1000/1000 (siempre estable) |
| Merge sort | 1000/1000 (siempre estable) |
| Selection sort | ~200-400/1000 (inestable por swaps largos) |
| Shell sort | ~300-500/1000 (inestable por gaps) |
| Quicksort | ~100-300/1000 (inestable por partición) |
| Heapsort | ~50-200/1000 (muy inestable) |

Con valores en $[0,5]$ y $n = 50$, hay ~10 elementos por valor. La
probabilidad de que un algoritmo inestable preserve accidentalmente el
orden es baja cuando hay muchos duplicados.

</details>

### Ejercicio 4 — Rendimiento con structs grandes

Ordena un array de structs de 256 bytes (con clave `int`) usando cada
algoritmo. ¿Cómo cambia el ranking de rendimiento?

<details><summary>Predicción</summary>

Con structs de 256 bytes, los **movimientos** (shifts, swaps, copias)
se vuelven dominantes (cada uno mueve 256 bytes en lugar de 4).

Nuevo ranking (tiempo):
1. **Selection sort** puede subir (minimiza swaps: $n - 1$).
2. **Merge sort** baja (copia $2n \log n \times 256$ bytes).
3. **Quicksort** mantiene posición (swaps son caros, pero son pocos).
4. **Insertion sort** baja mucho (shifts de 256 bytes).

La solución correcta: **no mover los structs**. Crear un array de
índices/punteros, ordenar los índices (4-8 bytes), y reordenar al
final. Esto restaura el ranking original.

</details>

### Ejercicio 5 — Comparaciones caras: strings

Ordena 10000 strings aleatorias de longitud 50. Compara los algoritmos
por tiempo. ¿Cambia el ranking?

<details><summary>Predicción</summary>

Con comparaciones de strings (~50 ns cada una vs ~1 ns para ints), el
número de comparaciones importa más que los movimientos (mover un
puntero = 8 ns).

Nuevo ranking por comparaciones:
1. **Merge sort** (mínimo de comparaciones: $n \log_2 n$).
2. **Quicksort** ($1.39\, n \log_2 n$ comparaciones).
3. **Heapsort** ($2n \log_2 n$ — la desventaja se amplifica).

Merge sort podría ganar a quicksort en tiempo para strings porque la
ventaja de cache de quicksort se reduce (las strings están dispersas
en el heap) y la ventaja en comparaciones de merge sort se amplifica.

</details>

### Ejercicio 6 — stdlib vs manual

Compara tu implementación manual de quicksort con `qsort()` de C y
`.sort_unstable()` de Rust para $n = 10^5$ y $n = 10^6$.

<details><summary>Predicción</summary>

Las implementaciones de stdlib deberían ser ~20-50% más rápidas que
las manuales porque:
- `qsort` de glibc usa introsort optimizado con años de tuning.
- `.sort_unstable()` de Rust usa pdqsort con block partition y
  detección de patrones.
- Ambos están compilados con las mejores optimizaciones del compilador.

Para $n = 10^6$:
- Manual quicksort: ~70 ms.
- `qsort`: ~50-60 ms.
- `.sort_unstable()`: ~40-50 ms (monomorphization elimina indirección).

Rust `.sort_unstable()` > C `qsort` porque `qsort` usa indirección
vía puntero a función (no inlineable), mientras Rust monomorphiza.

</details>

### Ejercicio 7 — Adaptividad cuantificada

Para datos con $p$% de perturbaciones ($p \in \{0, 1, 5, 10, 25, 50, 100\}$),
mide el tiempo de cada algoritmo. Grafica tiempo vs $p$ para
$n = 10000$.

<details><summary>Predicción</summary>

| $p$% | Insertion | Merge (opt) | Quick (pdq) | Heapsort |
|------|----------|-------------|-------------|---------|
| 0 | **$O(n)$** | **$O(n)$** | $O(n \log n)$ | $O(n \log n)$ |
| 1 | ~$O(n)$ | ~$O(n)$ | $O(n \log n)$ | $O(n \log n)$ |
| 5 | $O(n^{1.5})$ | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| 10 | $O(n^{1.7})$ | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| 50 | $O(n^2)$ | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| 100 | $O(n^2)$ | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |

Insertion sort es el más sensible a $p$: varía de $O(n)$ a $O(n^2)$.
Merge sort y quicksort (pdqsort) son adaptativos para $p$ bajo.
Heapsort es completamente constante — la línea es plana.

</details>

### Ejercicio 8 — Peor caso garantizado vs probabilístico

Implementa un generador de "entrada adversarial" para quicksort con
pivote = último elemento (fácil: datos ordenados). Muestra que
introsort (quicksort + heapsort fallback) maneja este caso en
$O(n \log n)$.

<details><summary>Predicción</summary>

Quicksort puro con pivote fijo y datos ordenados:
$n^2/2 = 50M$ comparaciones para $n = 10000$. Tiempo: ~100 ms.
Profundidad: $n - 1 = 9999$ (probable stack overflow para $n > 20000$).

Introsort con `depth_limit = 2 \lfloor \log_2 n \rfloor = 26$:
Las primeras ~26 particiones son degeneradas, luego heapsort toma el
control. Total: ~$26n + n \log_2 n \approx 400K$ comparaciones.
Tiempo: ~3 ms. Profundidad: 26.

Ratio: quicksort puro es ~125x más lento. Introsort paga un overhead
mínimo (~5% vs quicksort puro para datos aleatorios) para garantizar
que el peor caso nunca ocurra.

</details>

### Ejercicio 9 — Sort multidimensional: estabilidad en acción

Crea un array de registros `{nombre, edad, salario}`. Ordena primero
por salario (estable), luego por edad (estable), luego por nombre
(estable). El resultado debe estar ordenado por (nombre, edad, salario)
como clave compuesta.

<details><summary>Predicción</summary>

Con un sort estable, ordenar por criterios sucesivos en orden inverso
de prioridad produce un sort multi-campo correcto:

1. Sort por salario: registros ordenados por salario.
2. Sort por edad (estable): dentro de cada grupo de edad, el orden
   por salario se preserva.
3. Sort por nombre (estable): dentro de cada nombre, el orden por
   (edad, salario) se preserva.

Resultado: ordenado por (nombre, edad, salario) — correcto.

Con un sort **inestable**, el paso 3 rompería el orden por (edad,
salario) dentro de cada nombre. La alternativa es usar un comparador
compuesto `cmp(a,b) = cmp(a.nombre, b.nombre) || cmp(a.edad, b.edad)
|| cmp(a.salario, b.salario)`, que funciona con cualquier sort pero
requiere una sola pasada.

</details>

### Ejercicio 10 — El algoritmo "perfecto"

Si pudieras diseñar el algoritmo de ordenamiento ideal, ¿qué
propiedades tendría? Implementa un "super sort" que combine:
- Detección de runs (como Timsort) para datos casi ordenados.
- Three-way partition (como pdqsort) para duplicados.
- Insertion sort para subarrays pequeños.
- Heapsort como fallback.

<details><summary>Predicción</summary>

El "super sort" es esencialmente lo que pdqsort ya implementa.

Algoritmo:
1. Si $n \leq 24$: insertion sort.
2. Verificar si el array ya está ordenado: pasada $O(n)$.
   Si sí, retornar.
3. Elegir pivote (mediana de tres o ninther).
4. Si la partición es muy desbalanceada: incrementar bad_count.
   Si bad_count > 0: shuffle parcial para destruir el patrón.
   Si bad_count > $\log_2 n$: heapsort fallback.
5. Si muchos iguales al pivote: three-way partition.
6. Recurrir en la más pequeña (TCO), loop en la más grande.

Rendimiento esperado:
- Aleatorio: $O(n \log n)$, ~10% más rápido que quicksort estándar
  (block partition).
- Ordenado: $O(n)$ (detección de run).
- Todos iguales: $O(n)$ (three-way).
- Adversarial: $O(n \log n)$ (shuffle + heapsort).

Este es literalmente pdqsort. No se puede hacer mucho mejor sin
sacrificar generalidad.

</details>
