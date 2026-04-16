# Quicksort optimizaciones

## Objetivo

Estudiar las tres optimizaciones fundamentales que transforman quicksort
de un algoritmo con peor caso $O(n^2)$ en la base de los algoritmos de
ordenamiento más rápidos en producción:

- **Cutoff a insertion sort**: cambiar a insertion sort para subarrays
  pequeños, eliminando overhead de recursión.
- **Partición de tres vías (Dutch National Flag)**: manejar
  eficientemente arrays con muchos elementos duplicados.
- **Quicksort aleatorizado**: eliminar la posibilidad de entradas
  adversariales determinísticas.
- Cómo se combinan en **introsort** y **pdqsort**.

---

## Optimización 1: cutoff a insertion sort

### El problema

Quicksort recursivo genera $2n - 1$ llamadas para $n$ elementos. La
mayoría son para subarrays muy pequeños: la mitad de las llamadas son
para subarrays de tamaño $\leq 1$, y el 75% son para tamaño $\leq 3$.
Cada llamada tiene overhead de stack frame (~20 ns).

### La solución

Cuando el subarray tiene $\leq T$ elementos (típicamente $T \in [8, 32]$),
usar insertion sort en lugar de continuar la recursión:

```c
void quicksort_cutoff(int *arr, int lo, int hi, int threshold) {
    if (hi - lo + 1 <= threshold) {
        insertion_sort_range(arr, lo, hi);
        return;
    }
    int p = partition(arr, lo, hi);
    quicksort_cutoff(arr, lo, p - 1, threshold);
    quicksort_cutoff(arr, p + 1, hi, threshold);
}
```

### Alternativa de Sedgewick

Robert Sedgewick propuso una variante más elegante: no hacer nada
para subarrays pequeños y ejecutar una sola pasada de insertion sort
sobre todo el array al final:

```c
void quicksort_sedgewick(int *arr, int lo, int hi, int threshold) {
    if (hi - lo + 1 <= threshold) return;  /* dejar desordenado */
    int p = partition(arr, lo, hi);
    quicksort_sedgewick(arr, lo, p - 1, threshold);
    quicksort_sedgewick(arr, p + 1, hi, threshold);
}

void sort(int *arr, int n) {
    quicksort_sedgewick(arr, 0, n - 1, 16);
    insertion_sort(arr, n);  /* una sola pasada final */
}
```

Esto funciona porque después de quicksort con cutoff, el array está
"casi ordenado": cada elemento está a distancia $\leq T$ de su posición
final. Insertion sort es $O(n)$ para ese caso (cada elemento se mueve a
lo sumo $T$ posiciones).

**Ventaja**: una sola llamada a insertion sort es más cache-friendly que
muchas llamadas pequeñas intercaladas con la recursión.

### Impacto empírico

| Threshold | Llamadas recursivas eliminadas | Speedup típico |
|-----------|-------------------------------|----------------|
| 1 (sin cutoff) | 0% | base |
| 8 | ~87% | ~10-15% |
| 16 | ~94% | ~15-20% |
| 32 | ~97% | ~15-25% |
| 64 | ~98% | ~10-20% (puede empeorar) |

El punto óptimo depende del hardware, pero 16 es el valor más común
en implementaciones de producción.

---

## Optimización 2: partición de tres vías (Dutch National Flag)

### El problema

Con la partición estándar (Lomuto o Hoare), los elementos **iguales al
pivote** se distribuyen arbitrariamente entre las dos particiones. Si
hay muchos duplicados, esto causa:

- **Lomuto**: todos los iguales van a la izquierda ($\leq$ pivot) →
  partición degenerada si la mayoría son iguales → $O(n^2)$.
- **Hoare**: los iguales se distribuyen entre ambos lados → mejor que
  Lomuto, pero los iguales se siguen comparando en recursiones
  posteriores.

Lo ideal: colocar todos los elementos iguales al pivote en sus
posiciones finales y **no recurrir sobre ellos**.

### Dutch National Flag (Dijkstra)

El nombre viene del problema de Dijkstra: ordenar un array de tres
colores (como la bandera holandesa: rojo, blanco, azul) en una sola
pasada. Aplicado a partición:

Dividir el array en **tres** zonas:

```
  lo          lt         i         gt          hi
   [  < pivot  |  == pivot  |  sin ver  |  > pivot  ]
```

- `arr[lo..lt-1]`: elementos $<$ pivot.
- `arr[lt..i-1]`: elementos $=$ pivot (ya en posición final).
- `arr[i..gt]`: elementos sin procesar.
- `arr[gt+1..hi]`: elementos $>$ pivot.

### Pseudocódigo

```
PARTITION_3WAY(arr, lo, hi):
    pivot = arr[lo]
    lt = lo          // frontera inferior de iguales
    gt = hi          // frontera superior de iguales
    i = lo + 1       // puntero de exploración

    mientras i <= gt:
        si arr[i] < pivot:
            swap(arr[lt], arr[i])
            lt++
            i++
        sino si arr[i] > pivot:
            swap(arr[i], arr[gt])
            gt--
            // NO incrementar i: el elemento swapeado no se ha visto
        sino:  // arr[i] == pivot
            i++

    // Resultado: arr[lo..lt-1] < pivot, arr[lt..gt] == pivot, arr[gt+1..hi] > pivot
    // Recurrir solo en [lo, lt-1] y [gt+1, hi]
```

### Traza detallada

Array: `[5, 3, 5, 8, 5, 2, 5, 1, 5]`, pivot = `arr[0]` = 5.

```
Estado inicial: lt=0, gt=8, i=1
                [5, 3, 5, 8, 5, 2, 5, 1, 5]
                 lt  i                    gt

i=1: arr[1]=3 < 5  → swap(arr[0],arr[1]), lt=1, i=2
                [3, 5, 5, 8, 5, 2, 5, 1, 5]
                    lt  i                 gt

i=2: arr[2]=5 == 5 → i=3
                [3, 5, 5, 8, 5, 2, 5, 1, 5]
                    lt     i              gt

i=3: arr[3]=8 > 5  → swap(arr[3],arr[8]), gt=7
                [3, 5, 5, 5, 5, 2, 5, 1, 8]
                    lt     i           gt

i=3: arr[3]=5 == 5 → i=4
                [3, 5, 5, 5, 5, 2, 5, 1, 8]
                    lt        i        gt

i=4: arr[4]=5 == 5 → i=5
                [3, 5, 5, 5, 5, 2, 5, 1, 8]
                    lt           i     gt

i=5: arr[5]=2 < 5  → swap(arr[1],arr[5]), lt=2, i=6
                [3, 2, 5, 5, 5, 5, 5, 1, 8]
                       lt           i  gt

i=6: arr[6]=5 == 5 → i=7
                [3, 2, 5, 5, 5, 5, 5, 1, 8]
                       lt              i
                                       gt

i=7: arr[7]=1 < 5  → swap(arr[2],arr[7]), lt=3, i=8
                [3, 2, 1, 5, 5, 5, 5, 5, 8]
                          lt              i
                                       gt

i=8 > gt=7 → TERMINAR

Resultado: [3, 2, 1 | 5, 5, 5, 5, 5 | 8]
            < 5       == 5 (final!)     > 5
Recurrir solo en [0,2] y [8,8]
```

Los cinco 5's ya están en sus posiciones finales. Sin tres vías,
quicksort seguiría comparándolos en recursiones futuras.

### Impacto

| Distribución de datos | Partición estándar | Tres vías |
|-----------------------|-------------------|-----------|
| Todos distintos | $O(n \log n)$ | $O(n \log n)$ (overhead mínimo) |
| Pocos valores únicos ($k$) | $O(n \log n)$ a $O(n^2)$ | $O(n \log k)$ |
| Todos iguales | $O(n^2)$ (Lomuto) | $O(n)$ (una pasada) |

Para datos del mundo real (edades, calificaciones, categorías), muchos
elementos son iguales. La partición de tres vías convierte estos casos
de problemáticos a óptimos.

---

## Optimización 3: quicksort aleatorizado

### El problema

Con pivote determinístico (primero, último, mediana de tres), un
adversario puede construir una entrada que siempre produzca
particiones degeneradas:

- Pivote fijo: datos ordenados o reversos → $O(n^2)$.
- Mediana de tres: existe el "median-of-three killer" (Musser, 1997)
  que fuerza $O(n^2)$.

### La solución

Elegir el pivote **uniformemente al azar**:

```c
int rand_idx = lo + rand() % (hi - lo + 1);
swap(&arr[rand_idx], &arr[hi]);
/* Continuar con partición normal usando arr[hi] como pivote */
```

### Análisis probabilístico

Para cualquier entrada fija, el quicksort aleatorizado tiene:
- **Esperanza**: $1.39\, n \log_2 n$ comparaciones.
- **Varianza**: $O(n^2)$ — la desviación estándar es $O(n)$, así que
  las fluctuaciones son pequeñas relativas al total.
- **Probabilidad de $> 4n \ln n$ comparaciones**: $< 1/n$ —
  exponencialmente improbable.

No existe entrada adversarial para quicksort aleatorizado. El peor
caso $O(n^2)$ solo ocurre si el generador de números aleatorios
produce una secuencia patológica, con probabilidad $1/n!$.

### Costo del azar

`rand()` en C es barato (~10 ns), pero tiene problemas:
- `RAND_MAX` puede ser solo 32767 en algunos sistemas.
- No es thread-safe (estado global).
- Calidad del generador puede ser pobre.

Alternativas:
- **xorshift**: generador rápido de buena calidad, ~2 ns.
- **Hash del índice**: `rand_idx = lo + hash(lo ^ hi) % (hi - lo + 1)` —
  determinístico pero impredecible para un adversario que no conoce
  la función hash.

---

## Optimización 4: tail call optimization

### El problema

En el peor caso, quicksort tiene profundidad de recursión $O(n)$.
Cada frame de stack usa ~64-128 bytes. Para $n = 10^6$, esto puede
causar stack overflow (~64-128 MB de stack).

### La solución

Recurrir solo en la partición más pequeña y usar un loop para la
más grande:

```c
void quicksort_tailcall(int *arr, int lo, int hi) {
    while (lo < hi) {
        int p = partition(arr, lo, hi);
        if (p - lo < hi - p) {
            quicksort_tailcall(arr, lo, p - 1);
            lo = p + 1;  /* "recurrir" en la mayor via loop */
        } else {
            quicksort_tailcall(arr, p + 1, hi);
            hi = p - 1;  /* "recurrir" en la mayor via loop */
        }
    }
}
```

La partición más pequeña tiene a lo sumo $n/2$ elementos. En el
siguiente nivel, a lo sumo $n/4$, etc. La profundidad máxima de
recursión real es siempre $\leq \lfloor \log_2 n \rfloor$, incluso
con particiones perfectamente degeneradas.

---

## Introsort: lo mejor de tres mundos

Introsort (Musser, 1997) combina las tres optimizaciones con una
cuarta idea: **cambiar a heapsort si la recursión se profundiza
demasiado**.

### Algoritmo

```
INTROSORT(arr, lo, hi, depth_limit):
    si hi - lo + 1 <= THRESHOLD:       // Opt 1: cutoff
        INSERTION_SORT(arr, lo, hi)
        retornar
    si depth_limit == 0:                // Fallback a heapsort
        HEAPSORT(arr, lo, hi)
        retornar
    pivot = MEDIAN_OF_THREE(arr, lo, hi)
    p = PARTITION(arr, lo, hi, pivot)
    INTROSORT(arr, lo, p-1, depth_limit - 1)
    INTROSORT(arr, p+1, hi, depth_limit - 1)

SORT(arr, n):
    INTROSORT(arr, 0, n-1, 2 * floor(log2(n)))
```

### Propiedades

| Propiedad | Valor |
|-----------|-------|
| Peor caso | $O(n \log n)$ garantizado (heapsort fallback) |
| Promedio | $O(n \log n)$ (quicksort dominante) |
| Espacio | $O(\log n)$ |
| Estable | No |
| Overhead vs quicksort puro | ~1-2% (rara vez se activa heapsort) |

Introsort es `std::sort` en GCC/Clang C++ y la base de muchas
implementaciones de `qsort`.

---

## pdqsort: pattern-defeating quicksort

pdqsort (Orson Peters, 2021) es la evolución moderna de introsort,
usado en Rust `sort_unstable` y Boost.Sort. Añade detección de
patrones:

### Mejoras sobre introsort

1. **Detección de datos ordenados**: si después de la partición un lado
   tiene 0 elementos, el pivote era un extremo → probable datos ya
   ordenados. Intenta una pasada de insertion sort en la otra partición.
   Si no hay swaps, el subarray ya estaba ordenado.

2. **Shuffle contra adversarios**: si se detectan demasiadas
   particiones malas seguidas, hace un shuffle aleatorio parcial de
   algunos elementos antes de continuar, destruyendo el patrón
   adversarial.

3. **Partición de bloques (block partition)**: en lugar de comparar
   elemento por elemento, procesa bloques de 64 elementos, almacenando
   los índices a swapear en un buffer. Esto mejora la predicción de
   ramas: las comparaciones se hacen en un bloque sin branching, y
   luego los swaps se ejecutan secuencialmente.

4. **Detección de pocos valores únicos**: si la partición produce
   muchos iguales, cambia a Dutch National Flag.

### Rendimiento comparativo

| Input | Introsort | pdqsort | Razón |
|-------|----------|---------|-------|
| Aleatorio | 1.0x | 1.0-1.1x | Block partition mejora branch prediction |
| Ordenado | 1.0x | ~$O(n)$ | Detección de runs |
| Casi ordenado | 1.0x | ~$O(n)$ | Insertion sort detecta |
| Pocos únicos | 1.0x | 2-3x más rápido | DNF automático |
| Adversarial | 1.0x | 1.0x | Shuffle + heapsort fallback |

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
    long swaps;
    long max_depth;
    long current_depth;
    long heapsort_calls;
    long insertion_calls;
} Metrics;

static void swap_int(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

/* ── Insertion sort (range) ──────────────────────────────── */

static void insertion_sort_range(int *arr, int lo, int hi, Metrics *m) {
    m->insertion_calls++;
    for (int i = lo + 1; i <= hi; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= lo && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j];
            m->swaps++;
            j--;
        }
        arr[j + 1] = key;
    }
}

/* ── Heapsort (range) ────────────────────────────────────── */

static void sift_down(int *arr, int lo, int i, int n, Metrics *m) {
    int val = arr[lo + i];
    while (2 * i + 1 < n) {
        int child = 2 * i + 1;
        if (child + 1 < n) {
            m->comparisons++;
            if (arr[lo + child + 1] > arr[lo + child]) child++;
        }
        m->comparisons++;
        if (arr[lo + child] <= val) break;
        arr[lo + i] = arr[lo + child];
        m->swaps++;
        i = child;
    }
    arr[lo + i] = val;
}

static void heapsort_range(int *arr, int lo, int hi, Metrics *m) {
    m->heapsort_calls++;
    int n = hi - lo + 1;
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, lo, i, n, m);
    for (int i = n - 1; i > 0; i--) {
        swap_int(&arr[lo], &arr[lo + i]);
        m->swaps++;
        sift_down(arr, lo, 0, i, m);
    }
}

/* ── Lomuto partition ────────────────────────────────────── */

static int partition_lomuto(int *arr, int lo, int hi, Metrics *m) {
    int pivot = arr[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        m->comparisons++;
        if (arr[j] <= pivot) {
            i++;
            swap_int(&arr[i], &arr[j]);
            m->swaps++;
        }
    }
    swap_int(&arr[i + 1], &arr[hi]);
    m->swaps++;
    return i + 1;
}

/* ── Hoare partition ─────────────────────────────────────── */

static int partition_hoare(int *arr, int lo, int hi, Metrics *m) {
    int mid = lo + (hi - lo) / 2;
    /* Median of three */
    if (arr[lo] > arr[mid]) swap_int(&arr[lo], &arr[mid]);
    if (arr[lo] > arr[hi])  swap_int(&arr[lo], &arr[hi]);
    if (arr[mid] > arr[hi]) swap_int(&arr[mid], &arr[hi]);
    int pivot = arr[mid];

    int i = lo - 1, j = hi + 1;
    for (;;) {
        do { i++; m->comparisons++; } while (arr[i] < pivot);
        do { j--; m->comparisons++; } while (arr[j] > pivot);
        if (i >= j) return j;
        swap_int(&arr[i], &arr[j]);
        m->swaps++;
    }
}

/* ── Three-way partition (Dutch National Flag) ───────────── */

static void partition_3way(int *arr, int lo, int hi,
                            int *lt_out, int *gt_out, Metrics *m) {
    int pivot = arr[lo];
    int lt = lo, gt = hi, i = lo + 1;

    while (i <= gt) {
        m->comparisons++;
        if (arr[i] < pivot) {
            swap_int(&arr[lt], &arr[i]);
            m->swaps++;
            lt++;
            i++;
        } else if (arr[i] > pivot) {
            m->comparisons++;  /* second comparison */
            swap_int(&arr[i], &arr[gt]);
            m->swaps++;
            gt--;
        } else {
            i++;
        }
    }
    *lt_out = lt;
    *gt_out = gt;
}

/* ── Variant 1: basic quicksort (no optimizations) ───────── */

void qs_basic(int *arr, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int p = partition_lomuto(arr, lo, hi, m);
    if (p > 0) qs_basic(arr, lo, p - 1, m);
    qs_basic(arr, p + 1, hi, m);

    m->current_depth--;
}

/* ── Variant 2: quicksort + cutoff ───────────────────────── */

#define CUTOFF 16

void qs_cutoff(int *arr, int lo, int hi, Metrics *m) {
    if (hi - lo + 1 <= CUTOFF) {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int p = partition_hoare(arr, lo, hi, m);
    qs_cutoff(arr, lo, p, m);
    qs_cutoff(arr, p + 1, hi, m);

    m->current_depth--;
}

/* ── Variant 3: quicksort + 3-way ────────────────────────── */

void qs_3way(int *arr, int lo, int hi, Metrics *m) {
    if (hi <= lo) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int lt, gt;
    partition_3way(arr, lo, hi, &lt, &gt, m);
    if (lt > 0) qs_3way(arr, lo, lt - 1, m);
    qs_3way(arr, gt + 1, hi, m);

    m->current_depth--;
}

/* ── Variant 4: quicksort + 3-way + cutoff ───────────────── */

void qs_3way_cutoff(int *arr, int lo, int hi, Metrics *m) {
    if (hi - lo + 1 <= CUTOFF) {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int lt, gt;
    partition_3way(arr, lo, hi, &lt, &gt, m);
    if (lt > 0) qs_3way_cutoff(arr, lo, lt - 1, m);
    qs_3way_cutoff(arr, gt + 1, hi, m);

    m->current_depth--;
}

/* ── Variant 5: introsort ────────────────────────────────── */

static int floor_log2(int n) {
    int k = 0;
    while (n > 1) { n /= 2; k++; }
    return k;
}

void introsort_r(int *arr, int lo, int hi, int depth_limit, Metrics *m) {
    if (hi - lo + 1 <= CUTOFF) {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    if (depth_limit == 0) {
        heapsort_range(arr, lo, hi, m);
        return;
    }

    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int p = partition_hoare(arr, lo, hi, m);
    introsort_r(arr, lo, p, depth_limit - 1, m);
    introsort_r(arr, p + 1, hi, depth_limit - 1, m);

    m->current_depth--;
}

void introsort(int *arr, int lo, int hi, Metrics *m) {
    int depth_limit = 2 * floor_log2(hi - lo + 1);
    introsort_r(arr, lo, hi, depth_limit, m);
}

/* ── Variant 6: introsort + 3-way ────────────────────────── */

void introsort_3way_r(int *arr, int lo, int hi, int depth_limit, Metrics *m) {
    if (hi - lo + 1 <= CUTOFF) {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    if (depth_limit == 0) {
        heapsort_range(arr, lo, hi, m);
        return;
    }

    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int lt, gt;
    partition_3way(arr, lo, hi, &lt, &gt, m);
    if (lt > 0) introsort_3way_r(arr, lo, lt - 1, depth_limit - 1, m);
    introsort_3way_r(arr, gt + 1, hi, depth_limit - 1, m);

    m->current_depth--;
}

void introsort_3way(int *arr, int lo, int hi, Metrics *m) {
    int depth_limit = 2 * floor_log2(hi - lo + 1);
    introsort_3way_r(arr, lo, hi, depth_limit, m);
}

/* ── Input generators ────────────────────────────────────── */

void fill_random(int *a, int n)   { for (int i = 0; i < n; i++) a[i] = rand() % (n * 10); }
void fill_sorted(int *a, int n)   { for (int i = 0; i < n; i++) a[i] = i; }
void fill_reversed(int *a, int n) { for (int i = 0; i < n; i++) a[i] = n - i; }
void fill_all_equal(int *a, int n){ for (int i = 0; i < n; i++) a[i] = 42; }
void fill_few_unique(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = rand() % 10;
}
void fill_nearly_sorted(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    for (int k = 0; k < n / 20; k++) {
        int x = rand() % n, y = rand() % n;
        swap_int(&a[x], &a[y]);
    }
}

/* ── Verification ────────────────────────────────────────── */

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* ── Benchmark harness ───────────────────────────────────── */

typedef void (*SortFn2)(int *, int, int, Metrics *);

void bench(const char *name, SortFn2 fn, int *original, int n) {
    int *w = malloc(n * sizeof(int));
    memcpy(w, original, n * sizeof(int));
    Metrics m = {0};

    clock_t start = clock();
    fn(w, 0, n - 1, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-24s %10ld cmp %8ld swp  d=%3ld  hs=%ld is=%ld  %8.0f us %s\n",
           name, m.comparisons, m.swaps, m.max_depth,
           m.heapsort_calls, m.insertion_calls,
           us, is_sorted(w, n) ? "OK" : "FAIL");

    free(w);
}

/* ── Demo 1: Three-way partition trace ───────────────────── */

void demo_3way_trace(void) {
    printf("=== Demo 1: Three-way partition trace ===\n\n");

    int arr[] = {5, 3, 5, 8, 5, 2, 5, 1, 5};
    int n = 9;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\nPivot = %d\n\n", arr[0]);

    int pivot = arr[0];
    int lt = 0, gt = n - 1, i = 1;

    while (i <= gt) {
        if (arr[i] < pivot) {
            swap_int(&arr[lt], &arr[i]);
            printf("i=%d: arr[%d]=%d < %d → swap with lt=%d → [", i, i, arr[lt], pivot, lt);
            lt++; i++;
        } else if (arr[i] > pivot) {
            swap_int(&arr[i], &arr[gt]);
            printf("i=%d: arr[%d]=%d > %d → swap with gt=%d → [", i, i, arr[i], pivot, gt);
            gt--;
        } else {
            printf("i=%d: arr[%d]=%d == %d → skip             → [", i, i, arr[i], pivot);
            i++;
        }
        for (int k = 0; k < n; k++) printf("%d%s", arr[k], k < n - 1 ? "," : "");
        printf("]\n");
    }

    printf("\nResult: ");
    for (int k = 0; k < n; k++) {
        if (k == lt) printf("| ");
        printf("%d ", arr[k]);
        if (k == gt) printf("| ");
    }
    printf("\n        lt=%d gt=%d → elements [lt..gt] are in final position\n\n", lt, gt);
}

/* ── Demo 2: Cutoff impact ───────────────────────────────── */

void demo_cutoff_impact(int n) {
    printf("=== Demo 2: Cutoff to insertion sort impact, n = %d ===\n\n", n);

    int *original = malloc(n * sizeof(int));
    fill_random(original, n);

    int thresholds[] = {1, 4, 8, 12, 16, 20, 32, 48, 64};
    int nt = sizeof(thresholds) / sizeof(thresholds[0]);

    printf("%10s %10s %10s %10s %10s\n",
           "Threshold", "Cmp", "Swaps", "Depth", "Time(us)");
    printf("%10s %10s %10s %10s %10s\n",
           "──────────", "──────────", "──────────", "──────────", "──────────");

    for (int ti = 0; ti < nt; ti++) {
        int t = thresholds[ti];
        int *w = malloc(n * sizeof(int));
        memcpy(w, original, n * sizeof(int));
        Metrics m = {0};

        clock_t start = clock();
        /* Inline variant with configurable threshold */
        int depth_limit = 2 * floor_log2(n);
        /* Use a manual stack to avoid rewriting introsort_r */
        /* Simpler: just call introsort_r with modified CUTOFF behavior */
        /* For this demo, we use the 3way+cutoff with variable threshold */
        {
            /* Quick inline introsort with variable cutoff */
            void qs_var(int *a, int lo, int hi, int cut, Metrics *met);
            qs_var(w, 0, n - 1, t, &m);
        }
        clock_t end = clock();
        double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

        printf("%10d %10ld %10ld %10ld %10.0f  %s\n",
               t, m.comparisons, m.swaps, m.max_depth, us,
               is_sorted(w, n) ? "OK" : "FAIL");
        free(w);
    }
    free(original);
    printf("\n");
}

/* Variable-cutoff quicksort for demo */
void qs_var(int *arr, int lo, int hi, int cut, Metrics *m) {
    if (hi - lo + 1 <= cut) {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;
    int p = partition_hoare(arr, lo, hi, m);
    qs_var(arr, lo, p, cut, m);
    qs_var(arr, p + 1, hi, cut, m);
    m->current_depth--;
}

/* ── Demo 3: Three-way vs standard for duplicates ────────── */

void demo_3way_vs_standard(int n) {
    printf("=== Demo 3: Three-way vs standard partition, n = %d ===\n\n", n);

    typedef struct {
        const char *name;
        void (*fill)(int *, int);
    } InputType;

    InputType inputs[] = {
        {"Random       ", fill_random},
        {"All equal    ", fill_all_equal},
        {"Few unique   ", fill_few_unique},
        {"Sorted       ", fill_sorted},
        {"Nearly sorted", fill_nearly_sorted},
    };

    for (int t = 0; t < 5; t++) {
        int *original = malloc(n * sizeof(int));
        inputs[t].fill(original, n);
        printf("--- %s ---\n", inputs[t].name);

        /* Skip basic for pathological cases */
        if (inputs[t].fill != fill_all_equal &&
            inputs[t].fill != fill_sorted) {
            bench("Basic (Lomuto)", qs_basic, original, n);
        } else {
            printf("  %-24s (skipped: O(n^2) risk)\n", "Basic (Lomuto)");
        }
        bench("Cutoff (T=16)", qs_cutoff, original, n);
        bench("3-way", qs_3way, original, n);
        bench("3-way + cutoff", qs_3way_cutoff, original, n);
        bench("Introsort", introsort, original, n);
        bench("Introsort + 3-way", introsort_3way, original, n);
        printf("\n");

        free(original);
    }
}

/* ── Demo 4: Introsort heapsort fallback ─────────────────── */

void demo_introsort_fallback(void) {
    printf("=== Demo 4: Introsort heapsort fallback ===\n\n");

    /* Create adversarial input for median-of-three */
    int sizes[] = {1000, 5000, 10000};
    for (int si = 0; si < 3; si++) {
        int n = sizes[si];
        int *arr = malloc(n * sizeof(int));

        /* Sorted input triggers fallback with Lomuto-style partition */
        fill_sorted(arr, n);

        printf("n = %d, sorted input:\n", n);
        bench("Introsort", introsort, arr, n);
        bench("Introsort + 3-way", introsort_3way, arr, n);
        printf("\n");

        free(arr);
    }
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     Quicksort Optimizations — Complete Analysis         ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    demo_3way_trace();
    demo_cutoff_impact(10000);
    demo_3way_vs_standard(10000);
    demo_introsort_fallback();

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o qs_opt qs_opt.c
./qs_opt
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── Metrics ─────────────────────────────────────────────── */

#[derive(Default, Clone)]
struct Metrics {
    comparisons: u64,
    swaps: u64,
    max_depth: u64,
    current_depth: u64,
    heapsort_calls: u64,
    insertion_calls: u64,
}

impl Metrics {
    fn enter(&mut self) {
        self.current_depth += 1;
        if self.current_depth > self.max_depth {
            self.max_depth = self.current_depth;
        }
    }
    fn leave(&mut self) {
        self.current_depth -= 1;
    }
}

/* ── Insertion sort (range) ──────────────────────────────── */

fn insertion_sort_range(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    m.insertion_calls += 1;
    for i in (lo + 1)..=hi {
        let key = arr[i];
        let mut j = i;
        while j > lo {
            m.comparisons += 1;
            if arr[j - 1] <= key { break; }
            arr[j] = arr[j - 1];
            m.swaps += 1;
            j -= 1;
        }
        arr[j] = key;
    }
}

/* ── Heapsort (range) ────────────────────────────────────── */

fn sift_down(arr: &mut [i32], lo: usize, i: usize, n: usize, m: &mut Metrics) {
    let val = arr[lo + i];
    let mut pos = i;
    while 2 * pos + 1 < n {
        let mut child = 2 * pos + 1;
        if child + 1 < n {
            m.comparisons += 1;
            if arr[lo + child + 1] > arr[lo + child] { child += 1; }
        }
        m.comparisons += 1;
        if arr[lo + child] <= val { break; }
        arr[lo + pos] = arr[lo + child];
        m.swaps += 1;
        pos = child;
    }
    arr[lo + pos] = val;
}

fn heapsort_range(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    m.heapsort_calls += 1;
    let n = hi - lo + 1;
    for i in (0..n / 2).rev() {
        sift_down(arr, lo, i, n, m);
    }
    for i in (1..n).rev() {
        arr.swap(lo, lo + i);
        m.swaps += 1;
        sift_down(arr, lo, 0, i, m);
    }
}

/* ── Hoare partition with median of three ────────────────── */

fn partition_hoare(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) -> usize {
    let mid = lo + (hi - lo) / 2;
    if arr[lo] > arr[mid] { arr.swap(lo, mid); }
    if arr[lo] > arr[hi]  { arr.swap(lo, hi); }
    if arr[mid] > arr[hi] { arr.swap(mid, hi); }
    let pivot = arr[mid];

    let mut i = lo;
    let mut j = hi;
    loop {
        while { m.comparisons += 1; arr[i] < pivot } { i += 1; }
        while { m.comparisons += 1; arr[j] > pivot } { j -= 1; }
        if i >= j { return j; }
        arr.swap(i, j);
        m.swaps += 1;
        i += 1;
        j -= 1;
    }
}

/* ── Three-way partition (DNF) ───────────────────────────── */

fn partition_3way(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) -> (usize, usize) {
    let pivot = arr[lo];
    let mut lt = lo;
    let mut gt = hi;
    let mut i = lo + 1;

    while i <= gt {
        m.comparisons += 1;
        if arr[i] < pivot {
            arr.swap(lt, i);
            m.swaps += 1;
            lt += 1;
            i += 1;
        } else if arr[i] > pivot {
            m.comparisons += 1;
            arr.swap(i, gt);
            m.swaps += 1;
            gt -= 1;
        } else {
            i += 1;
        }
    }
    (lt, gt)
}

/* ── Variant 1: quicksort + cutoff ───────────────────────── */

const CUTOFF: usize = 16;

fn qs_cutoff(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if hi - lo + 1 <= CUTOFF {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m.enter();
    let p = partition_hoare(arr, lo, hi, m);
    qs_cutoff(arr, lo, p, m);
    qs_cutoff(arr, p + 1, hi, m);
    m.leave();
}

/* ── Variant 2: 3-way + cutoff ───────────────────────────── */

fn qs_3way_cutoff(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if hi - lo + 1 <= CUTOFF {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m.enter();
    let (lt, gt) = partition_3way(arr, lo, hi, m);
    if lt > 0 {
        qs_3way_cutoff(arr, lo, lt - 1, m);
    }
    qs_3way_cutoff(arr, gt + 1, hi, m);
    m.leave();
}

/* ── Variant 3: introsort ────────────────────────────────── */

fn floor_log2(n: usize) -> u64 {
    let mut k = 0u64;
    let mut x = n;
    while x > 1 { x /= 2; k += 1; }
    k
}

fn introsort_r(arr: &mut [i32], lo: usize, hi: usize, depth_limit: u64, m: &mut Metrics) {
    if hi - lo + 1 <= CUTOFF {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    if depth_limit == 0 {
        heapsort_range(arr, lo, hi, m);
        return;
    }
    m.enter();
    let p = partition_hoare(arr, lo, hi, m);
    introsort_r(arr, lo, p, depth_limit - 1, m);
    introsort_r(arr, p + 1, hi, depth_limit - 1, m);
    m.leave();
}

fn introsort(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    let depth_limit = 2 * floor_log2(hi - lo + 1);
    introsort_r(arr, lo, hi, depth_limit, m);
}

/* ── Variant 4: introsort + 3-way ────────────────────────── */

fn introsort_3way_r(arr: &mut [i32], lo: usize, hi: usize, depth_limit: u64, m: &mut Metrics) {
    if hi - lo + 1 <= CUTOFF {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    if depth_limit == 0 {
        heapsort_range(arr, lo, hi, m);
        return;
    }
    m.enter();
    let (lt, gt) = partition_3way(arr, lo, hi, m);
    if lt > 0 {
        introsort_3way_r(arr, lo, lt - 1, depth_limit - 1, m);
    }
    introsort_3way_r(arr, gt + 1, hi, depth_limit - 1, m);
    m.leave();
}

fn introsort_3way(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    let depth_limit = 2 * floor_log2(hi - lo + 1);
    introsort_3way_r(arr, lo, hi, depth_limit, m);
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xBEEF_CAFE).hash(&mut h);
        (h.finish() % (n as u64 * 10)) as i32
    }).collect()
}
fn fill_sorted(n: usize) -> Vec<i32> { (0..n as i32).collect() }
fn fill_reversed(n: usize) -> Vec<i32> { (0..n as i32).rev().collect() }
fn fill_all_equal(n: usize) -> Vec<i32> { vec![42; n] }
fn fill_few_unique(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        i.hash(&mut h);
        (h.finish() % 10) as i32
    }).collect()
}
fn fill_nearly_sorted(n: usize) -> Vec<i32> {
    let mut v: Vec<i32> = (0..n as i32).collect();
    let p = (n / 20).max(1);
    for k in 0..p { let i = (k * 7 + 13) % n; let j = (k * 11 + 3) % n; v.swap(i, j); }
    v
}

/* ── Verification ────────────────────────────────────────── */

fn is_sorted(arr: &[i32]) -> bool {
    arr.windows(2).all(|w| w[0] <= w[1])
}

/* ── Benchmark helper ────────────────────────────────────── */

fn bench(name: &str, original: &[i32], sort_fn: fn(&mut [i32], usize, usize, &mut Metrics)) {
    let mut working = original.to_vec();
    let n = working.len();
    if n == 0 { return; }
    let mut m = Metrics::default();

    let start = Instant::now();
    sort_fn(&mut working, 0, n - 1, &mut m);
    let elapsed = start.elapsed().as_micros();

    let ok = if is_sorted(&working) { "OK" } else { "FAIL" };
    println!(
        "  {:<24} {:>10} cmp {:>8} swp  d={:>3}  hs={} is={}  {:>8} us {}",
        name, m.comparisons, m.swaps, m.max_depth,
        m.heapsort_calls, m.insertion_calls, elapsed, ok
    );
}

/* ── Demo 1: Three-way partition trace ───────────────────── */

fn demo_3way_trace() {
    println!("=== Demo 1: Three-way partition trace ===\n");

    let mut arr = vec![5, 3, 5, 8, 5, 2, 5, 1, 5];
    let n = arr.len();
    let pivot = arr[0];

    println!("Input: {:?}", arr);
    println!("Pivot = {}\n", pivot);

    let mut lt = 0usize;
    let mut gt = n - 1;
    let mut i = 1usize;

    while i <= gt {
        if arr[i] < pivot {
            arr.swap(lt, i);
            println!("i={}: {} < {} → swap lt={} → {:?}", i, arr[lt], pivot, lt, arr);
            lt += 1;
            i += 1;
        } else if arr[i] > pivot {
            arr.swap(i, gt);
            println!("i={}: {} > {} → swap gt={} → {:?}", i, arr[i], pivot, gt, arr);
            gt -= 1;
        } else {
            println!("i={}: {} == {} → skip       → {:?}", i, arr[i], pivot, arr);
            i += 1;
        }
    }
    println!("\nlt={}, gt={} → {:?}\n", lt, gt, arr);
}

/* ── Demo 2: Variable cutoff ────────────────────────────── */

fn qs_var(arr: &mut [i32], lo: usize, hi: usize, cut: usize, m: &mut Metrics) {
    if hi - lo + 1 <= cut {
        insertion_sort_range(arr, lo, hi, m);
        return;
    }
    m.enter();
    let p = partition_hoare(arr, lo, hi, m);
    qs_var(arr, lo, p, cut, m);
    qs_var(arr, p + 1, hi, cut, m);
    m.leave();
}

fn demo_cutoff_impact(n: usize) {
    println!("=== Demo 2: Cutoff impact, n = {} ===\n", n);

    let original = fill_random(n);
    let thresholds = [1, 4, 8, 12, 16, 20, 32, 48, 64];

    println!("{:>10} {:>10} {:>10} {:>10} {:>10}",
        "Threshold", "Cmp", "Swaps", "Depth", "Time(us)");
    println!("{:>10} {:>10} {:>10} {:>10} {:>10}",
        "──────────", "──────────", "──────────", "──────────", "──────────");

    for &t in &thresholds {
        let mut working = original.clone();
        let mut m = Metrics::default();

        let start = Instant::now();
        qs_var(&mut working, 0, n - 1, t, &mut m);
        let elapsed = start.elapsed().as_micros();

        let ok = if is_sorted(&working) { "OK" } else { "FAIL" };
        println!("{:>10} {:>10} {:>10} {:>10} {:>10}  {}",
            t, m.comparisons, m.swaps, m.max_depth, elapsed, ok);
    }
    println!();
}

/* ── Demo 3: All variants comparison ─────────────────────── */

fn demo_variants(n: usize) {
    println!("=== Demo 3: All quicksort variants, n = {} ===\n", n);

    let input_names = ["Random", "All equal", "Few unique", "Sorted", "Nearly sorted"];
    let generators: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_all_equal, fill_few_unique, fill_sorted, fill_nearly_sorted,
    ];

    for (ti, gen) in generators.iter().enumerate() {
        let original = gen(n);
        println!("--- {} ---", input_names[ti]);

        bench("Cutoff (T=16)", &original, qs_cutoff);
        bench("3-way + cutoff", &original, qs_3way_cutoff);
        bench("Introsort", &original, introsort);
        bench("Introsort + 3-way", &original, introsort_3way);
        println!();
    }
}

/* ── Demo 4: Heapsort fallback demonstration ─────────────── */

fn demo_heapsort_fallback() {
    println!("=== Demo 4: Introsort heapsort fallback ===\n");

    for &n in &[1000, 5000, 10000] {
        let original = fill_sorted(n);
        println!("n = {}, sorted input:", n);
        bench("Introsort", &original, introsort);
        bench("Introsort + 3-way", &original, introsort_3way);
        println!();
    }
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════════════╗");
    println!("║     Quicksort Optimizations — Complete Analysis         ║");
    println!("╚══════════════════════════════════════════════════════════╝\n");

    demo_3way_trace();
    demo_cutoff_impact(10_000);
    demo_variants(10_000);
    demo_heapsort_fallback();
}
```

### Compilar y ejecutar

```bash
rustc -O -o qs_opt_rs qs_opt.rs
./qs_opt_rs
```

---

## Resumen: de quicksort básico a pdqsort

| Generación | Algoritmo | Peor caso | Duplicados | Ordenado | Adversarial |
|------------|-----------|-----------|------------|----------|-------------|
| 1960 | Quicksort básico | $O(n^2)$ | $O(n^2)$ | $O(n^2)$ | Trivial |
| 1993 | + Mediana de 3 | $O(n^2)$ raro | $O(n^2)$ | $O(n \log n)$ | Posible |
| 1997 | + Introsort | $O(n \log n)$ | $O(n^2)$ → HS | $O(n \log n)$ | $O(n \log n)$ |
| 2009 | + Three-way | $O(n \log n)$ | $O(n)$ | $O(n \log n)$ | $O(n \log n)$ |
| 2021 | pdqsort | $O(n \log n)$ | $O(n)$ | $O(n)$ | $O(n \log n)$ |

Cada generación resuelve una debilidad de la anterior. pdqsort es
el estado del arte actual y es lo que Rust usa internamente para
`sort_unstable`.

---

## Ejercicios

### Ejercicio 1 — Cutoff óptimo empírico

Encuentra el cutoff óptimo para quicksort (Hoare + mediana de tres)
en tu máquina, probando $T \in \{1, 4, 8, 12, 16, 20, 24, 32, 48, 64\}$
con $n = 100000$ aleatorio.

<details><summary>Predicción</summary>

El cutoff óptimo típicamente está entre 12 y 24. Con $T = 1$ (sin
cutoff), el overhead de recursión domina para subarrays de 2-3
elementos. Con $T = 64$, insertion sort hace demasiado trabajo
cuadrático ($64^2/4 = 1024$ operaciones por subarray, vs ~400 de
quicksort).

El speedup del cutoff óptimo vs sin cutoff ($T = 1$) debería ser
15-25%. Las comparaciones aumentan ligeramente (insertion sort hace
más comparaciones que quicksort para el mismo subarray), pero el tiempo
total baja por eliminación de overhead.

</details>

### Ejercicio 2 — Dutch National Flag: todos iguales

Ordena un array de $n = 100000$ donde todos los elementos son 42.
Compara partición estándar (Lomuto) vs tres vías. Mide comparaciones
y tiempo.

<details><summary>Predicción</summary>

**Lomuto estándar**: cada partición produce $(n-1, 0)$ → $O(n^2)$.
Para $n = 100000$: $\approx 5 \times 10^9$ comparaciones. Probablemente
tarde varios segundos o cause stack overflow.

**Tres vías**: una sola pasada detecta que todos son iguales al pivote.
$lt = 0$, $gt = n-1$, ningún subarray para recursión. Total:
$n - 1 = 99999$ comparaciones, $0$ swaps. Tiempo: microsegundos.

Ratio: Lomuto es ~50000x más lento. Este es el caso más dramático de
la ventaja de tres vías.

</details>

### Ejercicio 3 — Pocos valores únicos

Genera un array de $n = 100000$ con solo 5 valores posibles (0-4).
Compara quicksort estándar vs tres vías.

<details><summary>Predicción</summary>

Con 5 valores únicos, cada valor aparece ~20000 veces.

**Estándar**: cada partición puede ser desbalanceada si muchos
elementos son iguales al pivote. Complejidad: $O(n \log n)$ con Hoare,
pero con constante alta porque los iguales se siguen procesando.
Estimado: ~$2 \times 10^6$ comparaciones.

**Tres vías**: el primer nivel separa un valor ($\sim 20000$ iguales)
y recurre en los otros. Después de 5 niveles de recursión (uno por
valor único), todo está ordenado. Comparaciones: $\sim 5n = 500000$.

Tres vías debería ser ~3-4x más rápido. La ventaja crece con más
duplicados y decrece con más valores únicos.

</details>

### Ejercicio 4 — Sedgewick vs cutoff directo

Implementa la variante de Sedgewick (dejar desordenado + insertion
sort final) y compárala con cutoff directo para $n = 100000$.

<details><summary>Predicción</summary>

Ambas deberían tener rendimiento muy similar porque hacen la misma
cantidad de trabajo total. La diferencia está en la localidad de cache:

**Cutoff directo**: cada subarray pequeño se ordena inmediatamente
cuando se alcanza en la recursión. Buen cache si el subarray ya está
en L1.

**Sedgewick**: una sola pasada de insertion sort sobre todo el array
al final. El array grande probablemente no cabe en L1, pero la pasada
es secuencial (buena localidad espacial).

En la práctica, la diferencia es < 5%. El cutoff directo suele ganar
ligeramente en hardware moderno porque los subarrays suelen estar en
L1 cuando se alcanzan durante la recursión.

</details>

### Ejercicio 5 — Introsort: forzar el fallback a heapsort

Crea una entrada que fuerce introsort a activar el fallback a heapsort.
Verifica que `heapsort_calls > 0` y que el resultado sigue siendo
correcto.

<details><summary>Predicción</summary>

Para forzar el fallback, necesitamos particiones consistentemente malas
que agoten el `depth_limit = 2 \lfloor \log_2 n \rfloor$. Con mediana
de tres, datos ordenados producen particiones perfectas (la mediana de
tres es exactamente la mediana), así que NO activa el fallback.

Opciones para forzar el fallback:
1. Usar pivote fijo (sin mediana de tres) con datos ordenados.
2. Construir un "median-of-three killer" (Musser, 1997).
3. Artificialmente reducir el `depth_limit` a un valor bajo (ej: 3).

Con `depth_limit = 3` y $n = 10000$: después de 3 niveles de recursión
(8 subarrays de ~1250 cada uno), todos los subarrays restantes usarán
heapsort. Se esperan ~8 llamadas a heapsort.

</details>

### Ejercicio 6 — Tail call optimization: profundidad de stack

Implementa quicksort con tail call optimization (recurrir en la
partición más pequeña, loop en la más grande). Mide profundidad
máxima de stack para datos ordenados con $n = 10000$.

<details><summary>Predicción</summary>

**Sin TCO**: datos ordenados con pivote fijo producen profundidad
$n - 1 = 9999$.

**Con TCO**: cada recursión es en la partición más pequeña, que tiene
a lo sumo $n/2$ elementos. Profundidad máxima: $\lfloor \log_2 n
\rfloor = 13$. Incluso con particiones perfectamente degeneradas
(0, n-1), la recursión es en la de tamaño 0 y el loop avanza sobre
la de $n - 1$, así que la profundidad es 1 en ese extremo.

Con mediana de tres + TCO y datos ordenados: profundidad ~$\log_2 n
= 13$, esencialmente igual que sin TCO (porque mediana de tres ya
produce buenas particiones).

</details>

### Ejercicio 7 — Dual-pivot quicksort

Implementa quicksort con dos pivotes (como Java `Arrays.sort` para
primitivos). Divide el array en tres particiones:
$< p_1$, $p_1 \leq x \leq p_2$, $> p_2$. Compara con single-pivot.

<details><summary>Predicción</summary>

Dual-pivot quicksort (Yaroslavskiy, 2009) hace ~$1.9n \ln n$
comparaciones en promedio (más que single-pivot: $2n \ln n \approx
1.39n \log_2 n$), pero hace ~$0.6n \ln n$ swaps (menos que
single-pivot: $0.33n \ln n$ por swap, pero más movimientos totales).

En la práctica, dual-pivot es ~5-10% más rápido que single-pivot para
datos aleatorios en hardware moderno, principalmente por mejor
utilización de cache (divide en 3 partes en lugar de 2, reduciendo
niveles de recursión de $\log_2 n$ a $\log_3 n$).

Para $n = 100000$: single-pivot ~$2.3 \times 10^6$ comparaciones,
dual-pivot ~$3.2 \times 10^6$ comparaciones pero menos pasadas totales.

</details>

### Ejercicio 8 — Tres vías: conteo de comparaciones extra

Para datos con todos valores distintos, la partición de tres vías hace
más comparaciones que la estándar (dos comparaciones por elemento vs
una). Mide este overhead para $n = 100000$ aleatorio.

<details><summary>Predicción</summary>

Con todos los valores distintos:
- **Partición estándar** (Hoare): $\sim n$ comparaciones por nivel.
- **Tres vías**: cada elemento necesita una comparación `< pivot` y
  posiblemente una `> pivot`. En promedio, ~$1.5n$ comparaciones por
  nivel (la mitad de los elementos son `< pivot` y entran sin segunda
  comparación, la otra mitad necesitan la segunda).

Overhead: ~50% más comparaciones. Para $n = 100000$: estándar ~$1.5M$
comparaciones, tres vías ~$2.2M$.

Este overhead se justifica **solo** cuando hay duplicados significativos.
Para datos con todos distintos, la partición estándar es mejor. Las
implementaciones modernas (pdqsort) detectan si hay duplicados y eligen
la estrategia apropiada.

</details>

### Ejercicio 9 — Block partition simplificada

Implementa una partición por bloques simplificada: procesa 8 elementos
a la vez, almacenando los índices a swapear en arrays locales, y luego
ejecuta los swaps. Compara con Lomuto estándar.

<details><summary>Predicción</summary>

La partición por bloques reduce los branch mispredictions. En Lomuto
estándar, cada comparación `arr[j] <= pivot` tiene ~50% de probabilidad
→ ~50% de mispredictions (~5 ns cada uno).

Con bloques de 8: las 8 comparaciones se ejecutan sin branching
(usando operaciones condicionales o masks), almacenando los resultados.
Luego los swaps se ejecutan secuencialmente. Esto convierte 8
branches impredecibles en 0 branches.

Speedup esperado: ~20-40% para datos aleatorios de tipo `int`. El
beneficio es mayor para tipos más pequeños (donde la comparación es
barata y el branch domina) y menor para tipos con comparación cara.

Para $n = 100000$: Lomuto ~80 ms, block partition ~55 ms.

</details>

### Ejercicio 10 — Quicksort adaptativo casero

Implementa un quicksort que antes de particionar, verifica si el
subarray ya está ordenado (una pasada de $n$ comparaciones). Si sí,
retorna inmediatamente. Mide el impacto para datos casi ordenados
vs datos aleatorios.

<details><summary>Predicción</summary>

**Datos casi ordenados**: la verificación detecta muchos subarrays ya
ordenados y evita la partición. Si el array tiene ~5% de perturbaciones,
la mayoría de los subarrays en los niveles profundos de la recursión
estarán ordenados. Speedup: ~2-4x vs quicksort sin verificación.

**Datos aleatorios**: la verificación casi nunca detecta un subarray
ordenado (la probabilidad de que $k$ elementos aleatorios estén
ordenados es $1/k!$). El overhead de la pasada extra es ~$n$ comparaciones
por nivel, duplicando el trabajo. Slowdown: ~40-60% vs quicksort sin
verificación.

Conclusión: esta optimización solo vale para datos que se espera estén
casi ordenados. Timsort hace algo similar pero más sofisticado
(detecta runs parciales, no solo subarrays completamente ordenados).

</details>
