# Merge sort

## Objetivo

Dominar merge sort como el algoritmo de ordenamiento eficiente más
predecible y entender sus propiedades fundamentales:

- **Divide and conquer**: dividir el array a la mitad, ordenar
  recursivamente, fusionar.
- $O(n \log n)$ en **todos** los casos (mejor, promedio y peor).
- **Estable**: preserva el orden relativo de elementos iguales.
- Costo: $O(n)$ de memoria auxiliar.
- Base teórica de Timsort y del ordenamiento externo.

---

## El principio: divide and conquer

Merge sort aplica el paradigma **divide and conquer** con tres pasos:

1. **Dividir**: partir el array en dos mitades (costo $O(1)$, solo
   calcular el punto medio).
2. **Conquistar**: ordenar recursivamente cada mitad.
3. **Combinar**: fusionar (merge) las dos mitades ordenadas en un
   solo array ordenado.

La clave es que fusionar dos arrays **ya ordenados** es una operación
$O(n)$: se recorren ambos con un puntero cada uno, y en cada paso se
elige el menor de los dos elementos apuntados.

### Pseudocódigo

```
MERGE_SORT(arr, lo, hi):
    si lo >= hi: retornar          // base: 0 o 1 elementos
    mid = lo + (hi - lo) / 2
    MERGE_SORT(arr, lo, mid)       // ordenar mitad izquierda
    MERGE_SORT(arr, mid+1, hi)     // ordenar mitad derecha
    MERGE(arr, lo, mid, hi)        // fusionar

MERGE(arr, lo, mid, hi):
    copiar arr[lo..hi] a tmp[lo..hi]
    i = lo, j = mid+1, k = lo
    mientras i <= mid y j <= hi:
        si tmp[i] <= tmp[j]:       // <= para estabilidad
            arr[k++] = tmp[i++]
        sino:
            arr[k++] = tmp[j++]
    copiar elementos restantes de tmp a arr
```

El detalle crucial es el `<=` en la comparación del merge: si usamos
`<`, los elementos iguales de la mitad derecha se colocarían antes que
los de la izquierda, rompiendo la estabilidad.

---

## Traza detallada

Array inicial: `[38, 27, 43, 3, 9, 82, 10]`

```
                    [38, 27, 43, 3, 9, 82, 10]
                   /                           \
            [38, 27, 43, 3]              [9, 82, 10]
           /               \            /           \
       [38, 27]          [43, 3]     [9, 82]       [10]
       /      \          /     \     /      \        |
     [38]    [27]      [43]   [3] [9]     [82]     [10]
       \      /          \     /     \      /        |
       [27, 38]          [3, 43]     [9, 82]       [10]
           \               /            \           /
            [3, 27, 38, 43]              [9, 10, 82]
                   \                           /
                    [3, 9, 10, 27, 38, 43, 82]
```

### Traza del merge final

Fusionando `[3, 27, 38, 43]` y `[9, 10, 82]`:

```
Paso  i  j   Comparación        Resultado parcial
────  ── ──  ──────────────     ──────────────────
  1   0  0   3 <= 9  → tomar 3    [3]
  2   1  0   27 > 9  → tomar 9    [3, 9]
  3   1  1   27 > 10 → tomar 10   [3, 9, 10]
  4   1  2   27 <= 82 → tomar 27  [3, 9, 10, 27]
  5   2  2   38 <= 82 → tomar 38  [3, 9, 10, 27, 38]
  6   3  2   43 <= 82 → tomar 43  [3, 9, 10, 27, 38, 43]
  7   ─  2   copiar restante 82   [3, 9, 10, 27, 38, 43, 82]
```

Total: 6 comparaciones para fusionar 7 elementos. En general, fusionar
dos arrays de tamaños $p$ y $q$ requiere entre $\max(p, q)$ y $p + q - 1$
comparaciones.

---

## Análisis de complejidad

### Recurrencia

$$T(n) = 2T\!\left(\frac{n}{2}\right) + \Theta(n)$$

El término $\Theta(n)$ es el merge. Por el **Teorema Maestro** (caso 2:
$a = 2$, $b = 2$, $f(n) = \Theta(n)$, $\log_b a = 1$):

$$T(n) = \Theta(n \log n)$$

### Conteo exacto de comparaciones

En cada nivel de la recursión hay $n$ elementos que participan en
merges. Hay $\lceil \log_2 n \rceil$ niveles. En cada nivel, el merge
hace entre $n/2$ y $n - 1$ comparaciones (por nivel):

| Caso | Comparaciones por nivel | Total |
|------|------------------------|-------|
| Mejor | $n/2$ (una mitad se agota primero) | $\frac{n}{2} \lceil \log_2 n \rceil$ |
| Peor | $n - 1$ (intercalado perfecto) | $(n-1) \lceil \log_2 n \rceil$ |
| Promedio | $\approx n \log_2 n - 1.26n$ | $n \log_2 n - 1.26n + O(\log n)$ |

Para $n = 1000$:
- Mejor: $\approx 5000$ comparaciones.
- Peor: $\approx 9990$ comparaciones.
- Promedio: $\approx 8710$ comparaciones.

### Comparaciones vs movimientos

El merge mueve exactamente $n$ elementos por nivel (cada elemento se
copia al buffer y de vuelta). Con $\log_2 n$ niveles, hay
$2n \log_2 n$ movimientos totales (copia al buffer + copia de vuelta).

### Espacio

Merge sort necesita un buffer auxiliar de $n$ elementos: $O(n)$ espacio
extra. Esto es su principal desventaja frente a quicksort y heapsort,
que son in-place (o casi in-place).

Se puede asignar el buffer **una sola vez** antes de la primera llamada
y pasarlo como parámetro, en lugar de asignarlo en cada llamada
recursiva. Esto reduce la presión sobre el allocator.

| Recurso | Complejidad |
|---------|-------------|
| Comparaciones (peor) | $O(n \log n)$ |
| Comparaciones (mejor) | $O(n \log n)$ |
| Movimientos | $\Theta(2n \log n)$ |
| Espacio extra | $\Theta(n)$ |
| Llamadas recursivas | $2n - 1$ |
| Profundidad de stack | $O(\log n)$ |

---

## Estabilidad de merge sort

Merge sort es **estable** siempre que el merge use `<=` (no `<`) al
comparar elementos de la mitad izquierda con los de la derecha. Esto
garantiza que cuando dos elementos son iguales, el de la mitad izquierda
(que tenía índice original menor) se coloca primero.

### Demostración con pares (valor, índice original)

```
Array: [(5,'a'), (3,'b'), (5,'c'), (1,'d')]

División:
  Izq: [(5,'a'), (3,'b')]    Der: [(5,'c'), (1,'d')]

Recursión izquierda:
  [(3,'b'), (5,'a')]          — 3 < 5, 'b' antes de 'a' ✓

Recursión derecha:
  [(1,'d'), (5,'c')]          — 1 < 5, 'd' antes de 'c' ✓

Merge final:
  i→(3,'b')  j→(1,'d')  →  1 < 3  →  tomar (1,'d')
  i→(3,'b')  j→(5,'c')  →  3 < 5  →  tomar (3,'b')
  i→(5,'a')  j→(5,'c')  →  5 <= 5 →  tomar (5,'a')  ← estabilidad
  i→fin      j→(5,'c')  →  tomar (5,'c')

Resultado: [(1,'d'), (3,'b'), (5,'a'), (5,'c')]
           Los dos 5 mantienen su orden original: 'a' antes de 'c' ✓
```

Si hubiéramos usado `<` en lugar de `<=`, cuando ambos valen 5, se
tomaría el de la derecha `(5,'c')` primero, rompiendo la estabilidad.

---

## Optimizaciones

### 1. Evitar merge si ya está ordenado

Si el último elemento de la mitad izquierda es menor o igual al
primer elemento de la mitad derecha, las dos mitades ya forman una
secuencia ordenada — no hace falta fusionar.

```c
if (arr[mid] <= arr[mid + 1]) return;  /* already sorted */
```

Esto convierte merge sort en $O(n)$ para datos ya ordenados (cada nivel
hace una comparación y retorna).

### 2. Alternar dirección de copia

En lugar de copiar siempre de `arr` a `tmp` y de vuelta, alternar la
dirección en cada nivel de recursión: en niveles pares se fusiona de
`arr` a `tmp`, en niveles impares de `tmp` a `arr`. Esto elimina la
mitad de las copias.

### 3. Cutoff a insertion sort

Para subarrays pequeños ($n < 16$ a $64$), usar insertion sort en lugar
de continuar la recursión. Esto elimina las llamadas recursivas en los
niveles más profundos y aprovecha la ventaja de insertion sort para
$n$ pequeño (analizado en T04 de S02).

### 4. Natural merge sort

En lugar de dividir siempre a la mitad, detectar **runs** naturales
(subsecuencias ya ordenadas) y fusionarlos. Para datos parcialmente
ordenados, esto puede reducir significativamente el trabajo. Timsort
es la versión sofisticada de esta idea.

---

## Merge sort bottom-up (iterativo)

La versión recursiva (top-down) divide hasta arrays de 1 elemento y
luego fusiona hacia arriba. La versión iterativa (bottom-up) hace lo
inverso: comienza fusionando pares de 1, luego pares de 2, de 4, etc.

```
Array: [38, 27, 43, 3, 9, 82, 10]

Pasada 1 (width=1): merge pares de 1
  [27,38] [3,43] [9,82] [10]

Pasada 2 (width=2): merge pares de 2
  [3,27,38,43] [9,10,82]

Pasada 3 (width=4): merge pares de 4
  [3,9,10,27,38,43,82]
```

Ventajas del bottom-up:
- Sin recursión (no usa stack de llamadas).
- Mismo número de comparaciones y movimientos.
- Más fácil de adaptar para **ordenamiento externo** (archivos en disco).

Desventaja: no se puede aplicar la optimización de "ya ordenado" tan
fácilmente en cada nivel.

---

## Merge sort para ordenamiento externo

Cuando los datos no caben en RAM, merge sort es el algoritmo de
elección. El procedimiento:

1. **Fase de runs**: leer bloques de $M$ elementos en memoria (donde
   $M$ es lo que cabe en RAM), ordenar cada bloque con quicksort o
   similar, escribir de vuelta a disco como un "run" ordenado.

2. **Fase de merge**: fusionar $k$ runs simultáneamente usando un
   **min-heap** de $k$ elementos. En cada paso, extraer el mínimo del
   heap (se sabe de qué run viene), escribirlo a la salida, y leer
   el siguiente elemento de ese run al heap.

Con $N$ elementos totales y $M$ elementos en RAM, se generan
$\lceil N/M \rceil$ runs iniciales. Si se fusionan $k$ runs a la vez
(k-way merge), el número de pasadas es $\lceil \log_k(N/M) \rceil$.

El cuello de botella es el I/O de disco, no las comparaciones. Merge
sort minimiza las lecturas/escrituras secuenciales, que es lo que los
discos (especialmente HDDs) hacen eficientemente.

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
    long moves;
} Metrics;

/* ── Merge (shared) ──────────────────────────────────────── */

static void do_merge(int *arr, int *tmp, int lo, int mid, int hi, Metrics *m) {
    memcpy(tmp + lo, arr + lo, (hi - lo + 1) * sizeof(int));
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        m->comparisons++;
        if (tmp[i] <= tmp[j]) {
            arr[k++] = tmp[i++];
        } else {
            arr[k++] = tmp[j++];
        }
        m->moves++;
    }
    while (i <= mid) { arr[k++] = tmp[i++]; m->moves++; }
    while (j <= hi)  { arr[k++] = tmp[j++]; m->moves++; }
}

/* ── Top-down merge sort ─────────────────────────────────── */

static void merge_sort_td(int *arr, int *tmp, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_td(arr, tmp, lo, mid, m);
    merge_sort_td(arr, tmp, mid + 1, hi, m);
    do_merge(arr, tmp, lo, mid, hi, m);
}

void merge_sort_topdown(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->moves = 0;
    int *tmp = malloc(n * sizeof(int));
    merge_sort_td(arr, tmp, 0, n - 1, m);
    free(tmp);
}

/* ── Top-down with "already sorted" optimization ─────────── */

static void merge_sort_opt_r(int *arr, int *tmp, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_opt_r(arr, tmp, lo, mid, m);
    merge_sort_opt_r(arr, tmp, mid + 1, hi, m);
    m->comparisons++;
    if (arr[mid] <= arr[mid + 1]) return;  /* already sorted */
    do_merge(arr, tmp, lo, mid, hi, m);
}

void merge_sort_optimized(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->moves = 0;
    int *tmp = malloc(n * sizeof(int));
    merge_sort_opt_r(arr, tmp, 0, n - 1, m);
    free(tmp);
}

/* ── Top-down with insertion sort cutoff ─────────────────── */

static void insertion_sub(int *arr, int lo, int hi, Metrics *m) {
    for (int i = lo + 1; i <= hi; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= lo && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j];
            m->moves++;
            j--;
        }
        arr[j + 1] = key;
        m->moves++;
    }
}

static void merge_sort_hyb_r(int *arr, int *tmp, int lo, int hi,
                               int threshold, Metrics *m) {
    if (hi - lo + 1 <= threshold) {
        insertion_sub(arr, lo, hi, m);
        return;
    }
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_hyb_r(arr, tmp, lo, mid, threshold, m);
    merge_sort_hyb_r(arr, tmp, mid + 1, hi, threshold, m);
    m->comparisons++;
    if (arr[mid] <= arr[mid + 1]) return;
    do_merge(arr, tmp, lo, mid, hi, m);
}

void merge_sort_hybrid(int *arr, int n, int threshold, Metrics *m) {
    m->comparisons = 0;
    m->moves = 0;
    int *tmp = malloc(n * sizeof(int));
    merge_sort_hyb_r(arr, tmp, 0, n - 1, threshold, m);
    free(tmp);
}

/* ── Bottom-up merge sort ────────────────────────────────── */

void merge_sort_bottomup(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->moves = 0;
    int *tmp = malloc(n * sizeof(int));

    for (int width = 1; width < n; width *= 2) {
        for (int lo = 0; lo < n; lo += 2 * width) {
            int mid = lo + width - 1;
            int hi  = lo + 2 * width - 1;
            if (mid >= n) break;       /* no right half */
            if (hi >= n) hi = n - 1;   /* clamp */
            do_merge(arr, tmp, lo, mid, hi, m);
        }
    }

    free(tmp);
}

/* ── Natural merge sort ──────────────────────────────────── */

void merge_sort_natural(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->moves = 0;
    int *tmp = malloc(n * sizeof(int));

    /* Collect runs, merge pairwise until one run remains */
    int *runs = malloc((n + 1) * sizeof(int));
    int done = 0;

    while (!done) {
        /* Find natural runs */
        int num_runs = 0;
        int i = 0;
        while (i < n) {
            runs[num_runs++] = i;
            i++;
            while (i < n) {
                m->comparisons++;
                if (arr[i - 1] > arr[i]) break;
                i++;
            }
        }
        runs[num_runs] = n;  /* sentinel */

        if (num_runs <= 1) {
            done = 1;
            break;
        }

        /* Merge adjacent runs */
        for (int r = 0; r + 1 < num_runs; r += 2) {
            int lo  = runs[r];
            int mid = runs[r + 1] - 1;
            int hi  = (r + 2 < num_runs) ? runs[r + 2] - 1 : n - 1;
            do_merge(arr, tmp, lo, mid, hi, m);
        }
    }

    free(runs);
    free(tmp);
}

/* ── Generic merge sort (like qsort interface) ───────────── */

typedef int (*CmpFunc)(const void *, const void *);

static void generic_merge(void *arr, void *tmp, int lo, int mid, int hi,
                          size_t size, CmpFunc cmp) {
    char *a = (char *)arr;
    char *t = (char *)tmp;
    memcpy(t + lo * size, a + lo * size, (hi - lo + 1) * size);

    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        if (cmp(t + i * size, t + j * size) <= 0) {
            memcpy(a + k * size, t + i * size, size);
            i++;
        } else {
            memcpy(a + k * size, t + j * size, size);
            j++;
        }
        k++;
    }
    while (i <= mid) {
        memcpy(a + k * size, t + i * size, size);
        i++; k++;
    }
    while (j <= hi) {
        memcpy(a + k * size, t + j * size, size);
        j++; k++;
    }
}

static void generic_merge_sort_r(void *arr, void *tmp, int lo, int hi,
                                  size_t size, CmpFunc cmp) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    generic_merge_sort_r(arr, tmp, lo, mid, size, cmp);
    generic_merge_sort_r(arr, tmp, mid + 1, hi, size, cmp);
    generic_merge(arr, tmp, lo, mid, hi, size, cmp);
}

void generic_merge_sort(void *arr, int n, size_t size, CmpFunc cmp) {
    void *tmp = malloc(n * size);
    generic_merge_sort_r(arr, tmp, 0, n - 1, size, cmp);
    free(tmp);
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
    int perturbs = n / 20;
    if (perturbs < 1) perturbs = 1;
    for (int k = 0; k < perturbs; k++) {
        int a = rand() % n, b = rand() % n;
        int tmp = arr[a]; arr[a] = arr[b]; arr[b] = tmp;
    }
}

void fill_reversed(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = n - i;
}

/* ── Verification ────────────────────────────────────────── */

int is_sorted(const int *arr, int n) {
    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) return 0;
    }
    return 1;
}

/* ── Benchmark ───────────────────────────────────────────── */

typedef void (*SortFn)(int *, int, Metrics *);

void benchmark(const char *label, int *original, int n,
               void (*sort)(int *, int, Metrics *)) {
    int *working = malloc(n * sizeof(int));
    memcpy(working, original, n * sizeof(int));
    Metrics m = {0, 0};

    clock_t start = clock();
    sort(working, n, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-25s %10ld cmp  %10ld mov  %8.0f us  %s\n",
           label, m.comparisons, m.moves, us,
           is_sorted(working, n) ? "OK" : "FAIL");

    free(working);
}

void benchmark_hybrid(const char *label, int *original, int n, int threshold) {
    int *working = malloc(n * sizeof(int));
    memcpy(working, original, n * sizeof(int));
    Metrics m = {0, 0};

    clock_t start = clock();
    merge_sort_hybrid(working, n, threshold, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-25s %10ld cmp  %10ld mov  %8.0f us  %s\n",
           label, m.comparisons, m.moves, us,
           is_sorted(working, n) ? "OK" : "FAIL");

    free(working);
}

/* ── Demo 1: Trace ───────────────────────────────────────── */

void demo_trace(void) {
    printf("=== Demo 1: Merge sort trace ===\n\n");

    int arr[] = {38, 27, 43, 3, 9, 82, 10};
    int n = 7;

    printf("Input:  ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    Metrics m = {0, 0};
    merge_sort_topdown(arr, n, &m);

    printf("Output: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
    printf("Comparisons: %ld, Moves: %ld\n\n", m.comparisons, m.moves);
}

/* ── Demo 2: Stability ───────────────────────────────────── */

typedef struct {
    int value;
    char tag;
} TaggedItem;

int cmp_tagged(const void *a, const void *b) {
    return ((TaggedItem *)a)->value - ((TaggedItem *)b)->value;
}

void demo_stability(void) {
    printf("=== Demo 2: Stability ===\n\n");

    TaggedItem items[] = {
        {5, 'a'}, {3, 'b'}, {5, 'c'}, {1, 'd'}, {3, 'e'}, {5, 'f'}
    };
    int n = 6;

    printf("Input:  ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", items[i].value, items[i].tag);
    printf("\n");

    generic_merge_sort(items, n, sizeof(TaggedItem), cmp_tagged);

    printf("Output: ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", items[i].value, items[i].tag);
    printf("\n");

    /* Check stability */
    int stable = 1;
    for (int i = 0; i < n - 1; i++) {
        if (items[i].value == items[i + 1].value &&
            items[i].tag > items[i + 1].tag) {
            stable = 0;
            break;
        }
    }
    printf("Stable: %s\n\n", stable ? "YES" : "NO");
}

/* ── Demo 3: All variants comparison ─────────────────────── */

void demo_variants(int n) {
    printf("=== Demo 3: Merge sort variants, n = %d ===\n\n", n);

    typedef struct {
        const char *name;
        void (*fill)(int *, int);
    } InputType;

    InputType inputs[] = {
        {"Random",        fill_random},
        {"Sorted",        fill_sorted},
        {"Nearly sorted", fill_nearly_sorted},
        {"Reversed",      fill_reversed},
    };

    for (int t = 0; t < 4; t++) {
        int *original = malloc(n * sizeof(int));
        inputs[t].fill(original, n);
        printf("--- %s ---\n", inputs[t].name);

        benchmark("Top-down (basic)",     original, n, merge_sort_topdown);
        benchmark("Top-down (optimized)", original, n, merge_sort_optimized);
        benchmark_hybrid("Hybrid (T=16)", original, n, 16);
        benchmark("Bottom-up",            original, n, merge_sort_bottomup);
        benchmark("Natural",              original, n, merge_sort_natural);
        printf("\n");

        free(original);
    }
}

/* ── Demo 4: Best vs worst case comparison counts ────────── */

void demo_comparison_counts(void) {
    printf("=== Demo 4: Comparison counts vs theoretical bounds ===\n\n");
    printf("%8s %12s %12s %12s %12s\n",
           "n", "Best(meas)", "Best(theo)", "Worst(meas)", "Worst(theo)");
    printf("%8s %12s %12s %12s %12s\n",
           "────────", "────────────", "────────────", "────────────", "────────────");

    int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};
    for (int si = 0; si < 8; si++) {
        int n = sizes[si];
        int *arr = malloc(n * sizeof(int));

        /* Best case: already sorted */
        for (int i = 0; i < n; i++) arr[i] = i;
        Metrics m_best = {0, 0};
        merge_sort_topdown(arr, n, &m_best);

        /* Worst case: interleaved to force maximum comparisons
           Use reversed data as a proxy (not exact worst but close) */
        for (int i = 0; i < n; i++) arr[i] = n - i;
        Metrics m_worst = {0, 0};
        merge_sort_topdown(arr, n, &m_worst);

        int log2n = 0;
        { int x = n; while (x > 1) { x /= 2; log2n++; } }
        long best_theo  = (long)n / 2 * log2n;
        long worst_theo = (long)(n - 1) * log2n;

        printf("%8d %12ld %12ld %12ld %12ld\n",
               n, m_best.comparisons, best_theo,
               m_worst.comparisons, worst_theo);

        free(arr);
    }
    printf("\n");
}

/* ── Demo 5: Bottom-up trace ─────────────────────────────── */

void demo_bottomup_trace(void) {
    printf("=== Demo 5: Bottom-up merge sort trace ===\n\n");

    int arr[] = {38, 27, 43, 3, 9, 82, 10, 1};
    int n = 8;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n\n");

    int *tmp = malloc(n * sizeof(int));
    Metrics m = {0, 0};
    int pass = 0;

    for (int width = 1; width < n; width *= 2) {
        pass++;
        for (int lo = 0; lo < n; lo += 2 * width) {
            int mid = lo + width - 1;
            int hi  = lo + 2 * width - 1;
            if (mid >= n) break;
            if (hi >= n) hi = n - 1;
            do_merge(arr, tmp, lo, mid, hi, &m);
        }
        printf("Pass %d (width=%d): ", pass, width);
        for (int i = 0; i < n; i++) printf("%d ", arr[i]);
        printf("\n");
    }

    printf("\nComparisons: %ld, Moves: %ld\n\n", m.comparisons, m.moves);
    free(tmp);
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║          Merge Sort — Complete Analysis          ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    demo_trace();
    demo_stability();
    demo_bottomup_trace();
    demo_comparison_counts();
    demo_variants(10000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o merge_sort merge_sort.c
./merge_sort
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

/* ── Merge (shared) ──────────────────────────────────────── */

fn do_merge(arr: &mut [i32], tmp: &mut [i32], lo: usize, mid: usize, hi: usize, m: &mut Metrics) {
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
        m.moves += 1;
        k += 1;
    }
    while i <= mid {
        arr[k] = tmp[i];
        m.moves += 1;
        i += 1;
        k += 1;
    }
    while j <= hi {
        arr[k] = tmp[j];
        m.moves += 1;
        j += 1;
        k += 1;
    }
}

/* ── Top-down merge sort ─────────────────────────────────── */

fn merge_sort_td(arr: &mut [i32], tmp: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    let mid = lo + (hi - lo) / 2;
    merge_sort_td(arr, tmp, lo, mid, m);
    merge_sort_td(arr, tmp, mid + 1, hi, m);
    do_merge(arr, tmp, lo, mid, hi, m);
}

fn merge_sort_topdown(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 {
        return;
    }
    let mut tmp = vec![0i32; n];
    merge_sort_td(arr, &mut tmp, 0, n - 1, m);
}

/* ── Top-down with "already sorted" optimization ─────────── */

fn merge_sort_opt_r(arr: &mut [i32], tmp: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    let mid = lo + (hi - lo) / 2;
    merge_sort_opt_r(arr, tmp, lo, mid, m);
    merge_sort_opt_r(arr, tmp, mid + 1, hi, m);
    m.comparisons += 1;
    if arr[mid] <= arr[mid + 1] {
        return;
    }
    do_merge(arr, tmp, lo, mid, hi, m);
}

fn merge_sort_optimized(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 {
        return;
    }
    let mut tmp = vec![0i32; n];
    merge_sort_opt_r(arr, &mut tmp, 0, n - 1, m);
}

/* ── Hybrid with insertion sort cutoff ───────────────────── */

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
            m.moves += 1;
            j -= 1;
        }
        arr[j] = key;
        m.moves += 1;
    }
}

fn merge_sort_hyb_r(
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
    merge_sort_hyb_r(arr, tmp, lo, mid, threshold, m);
    merge_sort_hyb_r(arr, tmp, mid + 1, hi, threshold, m);
    m.comparisons += 1;
    if arr[mid] <= arr[mid + 1] {
        return;
    }
    do_merge(arr, tmp, lo, mid, hi, m);
}

fn merge_sort_hybrid(arr: &mut [i32], threshold: usize, m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 {
        return;
    }
    let mut tmp = vec![0i32; n];
    merge_sort_hyb_r(arr, &mut tmp, 0, n - 1, threshold, m);
}

/* ── Bottom-up merge sort ────────────────────────────────── */

fn merge_sort_bottomup(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 {
        return;
    }
    let mut tmp = vec![0i32; n];
    let mut width = 1;
    while width < n {
        let mut lo = 0;
        while lo < n {
            let mid = lo + width - 1;
            let hi = (lo + 2 * width - 1).min(n - 1);
            if mid >= n {
                break;
            }
            do_merge(arr, &mut tmp, lo, mid, hi, m);
            lo += 2 * width;
        }
        width *= 2;
    }
}

/* ── Natural merge sort ──────────────────────────────────── */

fn merge_sort_natural(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 {
        return;
    }
    let mut tmp = vec![0i32; n];

    loop {
        /* Find natural runs */
        let mut runs = Vec::new();
        let mut i = 0;
        while i < n {
            runs.push(i);
            i += 1;
            while i < n {
                m.comparisons += 1;
                if arr[i - 1] > arr[i] {
                    break;
                }
                i += 1;
            }
        }

        if runs.len() <= 1 {
            break;
        }

        /* Merge adjacent runs */
        let mut r = 0;
        while r + 1 < runs.len() {
            let lo = runs[r];
            let mid = runs[r + 1] - 1;
            let hi = if r + 2 < runs.len() {
                runs[r + 2] - 1
            } else {
                n - 1
            };
            do_merge(arr, &mut tmp, lo, mid, hi, m);
            r += 2;
        }
    }
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n)
        .map(|i| {
            let mut h = DefaultHasher::new();
            (i as u64 ^ 0xCAFE_BABE).hash(&mut h);
            (h.finish() % (n as u64 * 10)) as i32
        })
        .collect()
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

/* ── Verification ────────────────────────────────────────── */

fn is_sorted(arr: &[i32]) -> bool {
    arr.windows(2).all(|w| w[0] <= w[1])
}

/* ── Benchmark helper ────────────────────────────────────── */

fn bench<F>(label: &str, original: &[i32], mut sort_fn: F)
where
    F: FnMut(&mut [i32], &mut Metrics),
{
    let mut working = original.to_vec();
    let mut m = Metrics::default();

    let start = Instant::now();
    sort_fn(&mut working, &mut m);
    let elapsed = start.elapsed().as_micros();

    let ok = if is_sorted(&working) { "OK" } else { "FAIL" };
    println!(
        "  {:<25} {:>10} cmp  {:>10} mov  {:>8} us  {}",
        label, m.comparisons, m.moves, elapsed, ok
    );
}

/* ── Demo 1: Trace ───────────────────────────────────────── */

fn demo_trace() {
    println!("=== Demo 1: Merge sort trace ===\n");

    let mut arr = vec![38, 27, 43, 3, 9, 82, 10];
    print!("Input:  ");
    for x in &arr {
        print!("{} ", x);
    }
    println!();

    let mut m = Metrics::default();
    merge_sort_topdown(&mut arr, &mut m);

    print!("Output: ");
    for x in &arr {
        print!("{} ", x);
    }
    println!();
    println!("Comparisons: {}, Moves: {}\n", m.comparisons, m.moves);
}

/* ── Demo 2: Stability ───────────────────────────────────── */

fn demo_stability() {
    println!("=== Demo 2: Stability ===\n");

    let mut items: Vec<(i32, char)> = vec![
        (5, 'a'), (3, 'b'), (5, 'c'), (1, 'd'), (3, 'e'), (5, 'f'),
    ];

    print!("Input:  ");
    for &(v, t) in &items {
        print!("({},{}) ", v, t);
    }
    println!();

    /* Merge sort on Vec<(i32, char)> using sort_by (Timsort, stable) */
    items.sort_by(|a, b| a.0.cmp(&b.0));

    print!("Output: ");
    for &(v, t) in &items {
        print!("({},{}) ", v, t);
    }
    println!();

    let stable = items
        .windows(2)
        .all(|w| w[0].0 != w[1].0 || w[0].1 < w[1].1);
    println!("Stable: {}\n", if stable { "YES" } else { "NO" });
}

/* ── Demo 3: Bottom-up trace ─────────────────────────────── */

fn demo_bottomup_trace() {
    println!("=== Demo 3: Bottom-up merge sort trace ===\n");

    let mut arr = vec![38, 27, 43, 3, 9, 82, 10, 1];
    let n = arr.len();

    print!("Input: ");
    for x in &arr {
        print!("{} ", x);
    }
    println!("\n");

    let mut tmp = vec![0i32; n];
    let mut m = Metrics::default();
    let mut pass = 0;
    let mut width = 1;

    while width < n {
        pass += 1;
        let mut lo = 0;
        while lo < n {
            let mid = lo + width - 1;
            let hi = (lo + 2 * width - 1).min(n - 1);
            if mid >= n {
                break;
            }
            do_merge(&mut arr, &mut tmp, lo, mid, hi, &mut m);
            lo += 2 * width;
        }
        print!("Pass {} (width={}): ", pass, width);
        for x in &arr {
            print!("{} ", x);
        }
        println!();
        width *= 2;
    }
    println!("\nComparisons: {}, Moves: {}\n", m.comparisons, m.moves);
}

/* ── Demo 4: Comparison counts vs theoretical ────────────── */

fn demo_comparison_counts() {
    println!("=== Demo 4: Comparison counts vs theoretical bounds ===\n");
    println!(
        "{:>8} {:>12} {:>12} {:>12} {:>12}",
        "n", "Best(meas)", "Best(theo)", "Worst(meas)", "Worst(theo)"
    );
    println!(
        "{:>8} {:>12} {:>12} {:>12} {:>12}",
        "────────", "────────────", "────────────", "────────────", "────────────"
    );

    for &n in &[8, 16, 32, 64, 128, 256, 512, 1024] {
        /* Best case: sorted */
        let mut arr: Vec<i32> = (0..n).collect();
        let mut m_best = Metrics::default();
        merge_sort_topdown(&mut arr, &mut m_best);

        /* Worst-ish case: reversed */
        let mut arr: Vec<i32> = (0..n).rev().collect();
        let mut m_worst = Metrics::default();
        merge_sort_topdown(&mut arr, &mut m_worst);

        let log2n = (n as f64).log2() as u64;
        let best_theo = (n as u64) / 2 * log2n;
        let worst_theo = (n as u64 - 1) * log2n;

        println!(
            "{:>8} {:>12} {:>12} {:>12} {:>12}",
            n, m_best.comparisons, best_theo, m_worst.comparisons, worst_theo
        );
    }
    println!();
}

/* ── Demo 5: All variants comparison ─────────────────────── */

fn demo_variants(n: usize) {
    println!("=== Demo 5: Merge sort variants, n = {} ===\n", n);

    let input_names = ["Random", "Sorted", "Nearly sorted", "Reversed"];
    let generators: Vec<fn(usize) -> Vec<i32>> =
        vec![fill_random, fill_sorted, fill_nearly_sorted, fill_reversed];

    for (ti, gen) in generators.iter().enumerate() {
        let original = gen(n);
        println!("--- {} ---", input_names[ti]);

        bench("Top-down (basic)", &original, |arr, m| {
            merge_sort_topdown(arr, m);
        });
        bench("Top-down (optimized)", &original, |arr, m| {
            merge_sort_optimized(arr, m);
        });
        bench("Hybrid (T=16)", &original, |arr, m| {
            merge_sort_hybrid(arr, 16, m);
        });
        bench("Bottom-up", &original, |arr, m| {
            merge_sort_bottomup(arr, m);
        });
        bench("Natural", &original, |arr, m| {
            merge_sort_natural(arr, m);
        });
        println!();
    }
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════╗");
    println!("║          Merge Sort — Complete Analysis          ║");
    println!("╚══════════════════════════════════════════════════╝\n");

    demo_trace();
    demo_stability();
    demo_bottomup_trace();
    demo_comparison_counts();
    demo_variants(10_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o merge_sort_rs merge_sort.rs
./merge_sort_rs
```

---

## Merge sort vs otros algoritmos

| Propiedad | Merge sort | Quicksort | Heapsort |
|-----------|-----------|-----------|----------|
| Peor caso | $O(n \log n)$ | $O(n^2)$ | $O(n \log n)$ |
| Promedio | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Estable | **Sí** | No | No |
| In-place | No ($O(n)$ extra) | **Sí** ($O(\log n)$ stack) | **Sí** |
| Cache | Regular | **Excelente** | Malo |
| Adaptativo | Con optimización | No (sí con introsort) | No |
| Externo | **Excelente** | Malo | Malo |

Merge sort gana cuando se necesita:
- **Estabilidad garantizada** (ordenar registros por múltiples campos).
- **Peor caso garantizado** (sistemas de tiempo real).
- **Ordenamiento externo** (datos que no caben en RAM).
- **Paralelismo** (las dos mitades se pueden ordenar en paralelo).

Quicksort gana en rendimiento práctico para datos en RAM por su
excelente localidad de cache.

---

## Ejercicios

### Ejercicio 1 — Conteo de comparaciones: mejor vs peor caso

Ordena arrays de tamaño $n = 1024$ ya ordenados (mejor caso) y con
intercalado perfecto (peor caso). Compara los conteos medidos con
las fórmulas teóricas.

<details><summary>Predicción</summary>

Para $n = 1024$ ($\log_2 n = 10$):
- Mejor caso teórico: $n/2 \cdot \log_2 n = 512 \cdot 10 = 5120$
  comparaciones.
- Peor caso teórico: $(n-1) \cdot \log_2 n = 1023 \cdot 10 = 10230$
  comparaciones.

Los valores medidos deberían estar dentro de un 5-10% de estos valores
teóricos. La diferencia viene de que el teórico asume divisiones exactas
en potencias de 2, y nuestro merge cuenta comparaciones ligeramente
diferente según la implementación.

</details>

### Ejercicio 2 — Top-down vs bottom-up

Compara top-down y bottom-up para $n = 100000$ con datos aleatorios,
ordenados y reversos. ¿Cuál es más rápido?

<details><summary>Predicción</summary>

Para datos **aleatorios**: rendimiento casi idéntico (mismas
comparaciones y movimientos, solo difiere la estructura de control).
Bottom-up puede ser ligeramente más rápido por no tener overhead de
recursión.

Para datos **ordenados**: top-down con la optimización `arr[mid] <=
arr[mid+1]` será mucho más rápido ($O(n)$ comparaciones, 0 merges).
Bottom-up no tiene esta optimización y hará el trabajo completo.

Para datos **reversos**: rendimiento similar, ambos hacen el mismo
número de merges.

</details>

### Ejercicio 3 — Natural merge sort con runs reales

Genera un array de 10000 elementos formado por 50 runs ordenados de
200 elementos cada uno (concatenar 50 secuencias `[0..199]` permutadas).
Compara natural merge sort vs top-down estándar.

<details><summary>Predicción</summary>

Natural merge sort detectará los 50 runs de longitud ~200 y los
fusionará en $\lceil \log_2 50 \rceil = 6$ pasadas, cada una procesando
10000 elementos. Total: ~60000 comparaciones.

Top-down estándar no sabe de los runs y divide por la mitad, generando
$\log_2 10000 \approx 14$ niveles de recursión. Total: ~130000
comparaciones.

Natural merge sort debería ser **~2x más rápido** para esta entrada.
Este es el principio detrás de Timsort: en datos del mundo real
(logs, registros parcialmente ordenados), los runs naturales son
comunes.

</details>

### Ejercicio 4 — Espacio auxiliar: una asignación vs muchas

Implementa dos versiones: una que asigna `tmp` en cada llamada
recursiva y otra que lo asigna una sola vez y lo pasa como parámetro.
Compara tiempo y número de `malloc`/`free` para $n = 100000$.

<details><summary>Predicción</summary>

La versión con `malloc` por llamada hará $2n - 1 \approx 200000$
asignaciones y liberaciones. Cada `malloc`/`free` cuesta ~50-100 ns.
Overhead adicional: ~10-20 ms.

La versión con una sola asignación hace 1 `malloc` y 1 `free`.
Overhead: ~0 ms.

La diferencia será notable: la versión con muchas asignaciones será
~20-50% más lenta. Lección: nunca asignar memoria auxiliar dentro de
la recursión.

</details>

### Ejercicio 5 — Merge sort para listas enlazadas

Merge sort es ideal para listas enlazadas porque:
- Dividir a la mitad es $O(n)$ (buscar el nodo medio).
- Merge es $O(n)$ sin espacio extra (re-enlazar nodos).
- No hay acceso random necesario (a diferencia de quicksort).

Implementa merge sort para una lista enlazada simple de `int`.
¿Cuál es la complejidad de espacio total?

<details><summary>Predicción</summary>

Complejidad temporal: $O(n \log n)$ — misma que para arrays.

Complejidad de espacio extra: $O(\log n)$ — solo el stack de recursión.
No se necesita buffer auxiliar porque el merge re-enlaza nodos existentes
sin copiar datos. Esta es la gran ventaja sobre arrays: merge sort para
listas enlazadas es **in-place** (modulo stack).

La implementación necesita una función `find_middle` que use la técnica
slow/fast pointer (tortuga y liebre) para encontrar el nodo medio en
$O(n)$.

</details>

### Ejercicio 6 — Estabilidad empírica

Crea un array de 1000 pares `(value, original_index)` donde `value`
tiene solo 10 valores posibles (0-9). Ordena con merge sort y verifica
que para cada grupo de `value` iguales, los `original_index` están en
orden ascendente.

<details><summary>Predicción</summary>

Merge sort con `<=` en la comparación garantiza estabilidad. Con 10
valores posibles y 1000 elementos, cada grupo tiene ~100 elementos.
Dentro de cada grupo, los índices originales deben estar en orden
ascendente.

Si cambias `<=` por `<` en el merge, la estabilidad se rompe: los
elementos iguales de la mitad derecha se colocarán antes que los de
la izquierda. Prueba ambas versiones para ver la diferencia.

</details>

### Ejercicio 7 — k-way merge sort

Implementa un merge sort que divide en $k = 4$ partes en lugar de 2.
Usa un min-heap para el merge de las 4 partes. Compara con el merge
sort binario estándar.

<details><summary>Predicción</summary>

4-way merge sort tiene $\log_4 n$ niveles de recursión en lugar de
$\log_2 n$ (la mitad). Pero cada merge de 4 vías necesita un heap, y
cada extracción del heap cuesta $O(\log 4) = O(1)$ (constante para
$k = 4$, pero con mayor overhead por operación).

El resultado neto: 4-way merge sort hace **menos pasadas** sobre los
datos (mejor para cache y disco), pero cada pasada es ligeramente más
cara. Para datos en RAM, probablemente sea comparable al binario. Para
ordenamiento externo en disco, gana porque minimiza lecturas/escrituras
secuenciales.

Para $n = 100000$: $\log_4 n \approx 8.3$ vs $\log_2 n \approx 16.6$
niveles.

</details>

### Ejercicio 8 — Inversiones con merge sort

El número de inversiones (pares $i < j$ donde $arr[i] > arr[j]$) se
puede contar durante merge sort: cada vez que se toma un elemento de
la mitad derecha, contribuye `(mid - i + 1)` inversiones. Implementa
esta cuenta y verifica con datos de prueba.

<details><summary>Predicción</summary>

Para un array ya ordenado: 0 inversiones.
Para un array reverso de $n$ elementos: $n(n-1)/2$ inversiones
(cada par es una inversión).
Para datos aleatorios de $n$ elementos: $\approx n(n-1)/4$ inversiones
(la mitad del máximo).

Contar inversiones con merge sort es $O(n \log n)$, mucho mejor que
el enfoque directo de $O(n^2)$ (comparar todos los pares). Es una
aplicación clásica de merge sort en algoritmia competitiva.

Para $n = 1000$ reverso: $1000 \times 999 / 2 = 499500$ inversiones.

</details>

### Ejercicio 9 — Merge sort paralelo (conceptual)

Merge sort se paraleliza naturalmente: las dos llamadas recursivas
son independientes. En Rust, usa `std::thread::scope` para paralelizar
los dos subcalls cuando $n > 10000$. Mide el speedup vs secuencial.

<details><summary>Predicción</summary>

Con 2 threads, el speedup teórico máximo es 2x, pero en la práctica:
- El primer nivel de recursión divide en 2 tareas paralelas.
- Los niveles siguientes también pueden paralelizarse, pero el overhead
  de crear threads limita la ganancia.
- El merge sigue siendo secuencial (es difícil paralelizarlo
  eficientemente).

Speedup esperado: ~1.5-1.8x con 2 cores, ~2.5-3.5x con 4 cores.
El merge se convierte en el cuello de botella (Ley de Amdahl).

Para $n = 1000000$, la versión paralela debería ser notablemente más
rápida que la secuencial. Para $n < 10000$, el overhead de threads
elimina la ganancia.

</details>

### Ejercicio 10 — Merge sort externo simulado

Simula un merge sort externo:
1. Divide un array de 100000 elementos en "bloques" de 1000.
2. Ordena cada bloque con `qsort`.
3. Fusiona todos los bloques usando un min-heap de tamaño $k = 100$.

Cuenta el número de "lecturas de disco" (accesos a bloque) y compáralo
con un merge sort interno estándar.

<details><summary>Predicción</summary>

Fase de runs: 100 bloques × 1000 elementos. Cada bloque se lee una vez,
se ordena, se escribe una vez. Total: 200 operaciones de bloque.

Fase de merge: con un 100-way merge, se necesita una sola pasada para
fusionar los 100 runs. Cada run se lee secuencialmente una vez más.
Total: 100 operaciones de bloque.

Total de I/O: ~300 operaciones de bloque = 3 pasadas sobre los datos.

Un merge sort binario necesitaría $\log_2 100 \approx 7$ pasadas
de merge, cada una leyendo y escribiendo todos los datos. Total: ~14
pasadas = 1400 operaciones de bloque.

El k-way merge reduce el I/O por un factor de ~5x. En disco mecánico,
donde cada pasada cuesta segundos, esta diferencia es crucial.

</details>
