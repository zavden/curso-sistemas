# Heapsort en contexto

## Objetivo

Analizar heapsort **dentro del panorama de los algoritmos eficientes**,
complementando la cobertura detallada en C07/S02/T01:

- Revisar el algoritmo: build max-heap $O(n)$ + extract-max $O(n \log n)$.
- $O(n \log n)$ **garantizado** (mejor, promedio y peor) — único entre
  los tres grandes junto con merge sort.
- In-place ($O(1)$ extra) — único entre los $O(n \log n)$ garantizados.
- **Cache-unfriendly**: por qué es 2-3x más lento que quicksort pese a
  tener misma complejidad asintótica.
- Su rol en introsort como fallback de seguridad.

> **Referencia**: La implementación completa del heap, sift-down, heapify
> bottom-up y la traza paso a paso están en
> `C07_Monticulos_y_colas_de_prioridad/S02_Aplicaciones_del_heap/T01_Heapsort/README.md`.
> Este tópico se enfoca en la comparación y el análisis práctico.

---

## Repaso del algoritmo

```
HEAPSORT(arr, n):
    // Fase 1: Build max-heap — O(n)
    para i desde n/2-1 hasta 0:
        SIFT_DOWN(arr, i, n)

    // Fase 2: Extract — O(n log n)
    para i desde n-1 hasta 1:
        swap(arr[0], arr[i])       // máximo va al final
        SIFT_DOWN(arr, 0, i)       // restaurar heap en arr[0..i-1]
```

### Por qué max-heap para orden ascendente

Cada extracción mueve el máximo actual a la **última** posición de la
zona de heap. La zona de heap se encoge desde la derecha y la zona
ordenada crece desde la derecha:

```
[──── heap ────|── ordenado ──]
       ↓ extract
[─── heap ───|─── ordenado ───]
```

Si se usara min-heap, el mínimo estaría en `arr[0]` y necesitaríamos
espacio extra o trucos complicados para no sobrescribirlo.

---

## Complejidad exacta

### Comparaciones

| Caso | Comparaciones | Razón |
|------|--------------|-------|
| Build | $\leq 2n$ | Heapify bottom-up lineal |
| Extract (peor) | $2n \log_2 n$ | Cada sift-down: $2 \lfloor \log_2 k \rfloor$ cmp (2 por nivel: hijos + padre) |
| Extract (promedio) | $\approx 2n \log_2 n - O(n)$ | Casi siempre baja hasta el fondo |
| **Total (peor)** | $\leq 2n \log_2 n + 2n$ | |

Las $2n \log_2 n$ comparaciones del extract son ~44% más que las
$\approx 1.39\, n \log_2 n$ de quicksort promedio. Heapsort hace más
comparaciones **y** cada comparación es más lenta (cache misses).

### Optimización de Floyd (bottom-up heapsort)

Robert Floyd observó que en la fase de extracción, el elemento recién
colocado en la raíz casi siempre baja hasta el fondo. Su optimización:

1. **Bajar sin comparar con la raíz**: simplemente seguir al hijo mayor
   hasta llegar al fondo (solo $\lfloor \log_2 k \rfloor$ comparaciones
   en lugar de $2 \lfloor \log_2 k \rfloor$).
2. **Subir buscando la posición correcta**: una vez en el fondo,
   burbujear hacia arriba hasta la posición correcta (típicamente 1-2
   pasos).

Esto reduce las comparaciones a $\sim n \log_2 n + O(n)$, comparable
con merge sort. Pero el beneficio práctico es limitado porque el cuello
de botella de heapsort es el cache, no las comparaciones.

### Movimientos (swaps)

Cada extracción hace 1 swap + los swaps del sift-down. En el peor caso,
sift-down baja $\lfloor \log_2 k \rfloor$ niveles = $\lfloor \log_2 k
\rfloor$ swaps.

Total: $\sum_{k=1}^{n-1} (1 + \lfloor \log_2 k \rfloor) \approx
n \log_2 n$ swaps.

Cada swap mueve 3 valores (via temp). Total de escrituras: $\sim 3n
\log_2 n$. Comparado con merge sort ($\sim 2n \log_2 n$ copias) y
quicksort ($\sim 0.7 \cdot 3n \log_2 n \approx 2n \log_2 n$
escrituras de swaps), heapsort hace el mayor número de escrituras.

---

## El problema del cache

### Por qué heapsort es lento en la práctica

Heapsort tiene la misma complejidad asintótica que quicksort y merge
sort, pero es consistentemente 2-3x más lento en hardware moderno. La
razón es la **localidad de cache**.

### Patrón de acceso del sift-down

En un heap almacenado en array, los hijos del nodo $i$ están en
posiciones $2i + 1$ y $2i + 2$. Un sift-down desde la raíz accede a:

```
Nivel 0: posición 0
Nivel 1: posición 1 o 2
Nivel 2: posición 3, 4, 5, o 6
Nivel 3: posición 7-14
Nivel 4: posición 15-30
...
Nivel k: posiciones 2^k - 1 a 2^(k+1) - 2
```

Los saltos entre niveles **duplican** la distancia en memoria. Para un
array de $n = 10^6$ enteros ($4$ MB), los primeros niveles caben en L1
(32 KB), pero:

| Nivel | Rango de posiciones | Distancia en bytes | Cache |
|-------|--------------------|--------------------|-------|
| 0-12 | 0 - 8191 | 32 KB | L1 hit |
| 13 | 8192 - 16383 | 32 KB | L1 miss, L2 hit |
| 14-16 | 16384 - 131071 | 512 KB | L2 miss, L3 hit |
| 17-19 | 131072 - 1048575 | 4 MB | L3 miss, RAM |

Cada sift-down durante la fase de extracción recorre todos los niveles,
así que los niveles profundos siempre causan **cache misses**. Con
latencia L3 de ~10 ns y RAM de ~50-100 ns, un sift-down de profundidad
20 puede tomar 200-400 ns solo en latencia de cache.

### Comparación de patrones de acceso

| Algoritmo | Patrón de acceso | Cache behavior |
|-----------|-----------------|----------------|
| Quicksort | Secuencial (partición recorre linealmente) | Excelente — prefetch automático |
| Merge sort | Secuencial (merge recorre dos arrays linealmente) | Bueno — pero buffer extra compite por cache |
| Heapsort | Exponencial (sift-down salta $2^k$) | **Terrible** — cada nivel es un cache miss |
| Insertion sort | Secuencial (shift recorre hacia atrás) | Excelente — todo en L1 para $n$ pequeño |

### Medición empírica de cache misses

En x86-64 con `perf stat`:

```
Quicksort  (n=10^6):  ~3M  L1 misses,  ~0.5M L3 misses
Merge sort (n=10^6):  ~8M  L1 misses,  ~1M   L3 misses
Heapsort   (n=10^6):  ~40M L1 misses,  ~15M  L3 misses
```

Heapsort tiene ~10x más L1 misses que quicksort. Cada miss agrega
~4-10 ns de latencia, lo que domina completamente el costo de las
comparaciones (~1-2 ns cada una).

---

## Heapsort vs quicksort vs merge sort

### Tabla comparativa completa

| Propiedad | Heapsort | Quicksort | Merge sort |
|-----------|---------|-----------|------------|
| Peor caso | $O(n \log n)$ | $O(n^2)$ | $O(n \log n)$ |
| Promedio | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Mejor caso | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$* |
| Comparaciones (promedio) | $\sim 2n \log_2 n$ | $\sim 1.39\, n \log_2 n$ | $\sim n \log_2 n$ |
| Espacio extra | $O(1)$ | $O(\log n)$ stack | $O(n)$ |
| Estable | **No** | **No** | **Sí** |
| In-place | **Sí** | **Sí** | **No** |
| Cache | Terrible | **Excelente** | Bueno |
| Adaptativo | **No** | No (sí pdqsort) | Con optimización |
| Tiempo real ($n = 10^6$) | ~130 ms | ~65 ms | ~90 ms |

*Merge sort con la optimización `arr[mid] <= arr[mid+1]` es $O(n)$
para datos ya ordenados.

### El "triángulo imposible" revisitado

Se desean tres propiedades: estable + in-place + $O(n \log n)$
garantizado. Ningún algoritmo práctico tiene las tres:

| Algoritmo | Estable | In-place | $O(n \log n)$ garantizado |
|-----------|---------|----------|--------------------------|
| Merge sort | Si | **No** | Si |
| Heapsort | **No** | Si | Si |
| Quicksort | **No** | Si | **No** |

Heapsort sacrifica estabilidad (y rendimiento práctico por cache) para
obtener in-place + garantía.

---

## Cuándo usar heapsort

### 1. Fallback en introsort

El uso principal de heapsort en la práctica no es como algoritmo
primario, sino como **red de seguridad** en introsort:

```
si depth_limit == 0:
    heapsort(arr, lo, hi)    // garantía O(n log n)
    retornar
```

Esto convierte quicksort de $O(n^2)$ peor caso a $O(n \log n)$
garantizado, con overhead mínimo (heapsort rara vez se activa).

### 2. Memoria estrictamente limitada

Cuando no se puede asignar $O(n)$ memoria extra (merge sort) y se
necesita garantía $O(n \log n)$ (quicksort no la da):

- Sistemas embebidos sin heap dinámico.
- Kernels de sistema operativo durante la inicialización temprana.
- Contextos donde `malloc` no está disponible o es prohibitivo.

### 3. Partial sort (los $k$ menores)

Heapsort se puede detener después de $k$ extracciones para obtener los
$k$ elementos menores en $O(n + k \log n)$. Para $k \ll n$, esto es
mucho mejor que ordenar todo:

```c
/* Partial sort: find k smallest elements */
void partial_heapsort(int *arr, int n, int k) {
    /* Build min-heap */
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down_min(arr, i, n);

    /* Extract k smallest */
    for (int i = 0; i < k; i++) {
        /* arr[0] is the current minimum */
        swap(&arr[0], &arr[n - 1 - i]);
        sift_down_min(arr, 0, n - 1 - i);
    }
}
```

Complejidad: $O(n)$ para build + $O(k \log n)$ para extracciones.
Para los 100 menores de $10^6$: $O(10^6 + 100 \cdot 20) = O(10^6)$
vs $O(10^6 \cdot 20) = O(2 \times 10^7)$ para ordenar todo.

`std::partial_sort` en C++ usa exactamente este enfoque.

### 4. Worst-case execution time (WCET) predecible

Heapsort siempre hace exactamente el mismo número de operaciones
independientemente de la entrada. Para sistemas de tiempo real duro
donde se necesita WCET predecible, heapsort es más confiable que
quicksort (que varía entre $O(n \log n)$ y $O(n^2)$) y merge sort
(que puede variar en número de merges con natural merge sort).

---

## Heapsort no es adaptativo

### El problema fundamental

Heapsort **destruye** cualquier orden preexistente durante la fase de
build. La operación heapify reorganiza el array según la propiedad de
heap, que no tiene relación con el orden ascendente/descendente.

```
Entrada ya ordenada: [1, 2, 3, 4, 5, 6, 7]
Después de build max-heap: [7, 5, 6, 4, 2, 3, 1]
→ Todo el orden original se perdió
```

Después de build, la fase de extract tiene que hacer el mismo trabajo
que para cualquier otra entrada. No hay forma de "detectar" que la
entrada estaba casi ordenada.

### Contraste con los otros eficientes

| Input | Quicksort (pdqsort) | Merge sort (optimizado) | Heapsort |
|-------|--------------------|-----------------------|---------|
| Sorted | $O(n)$ | $O(n)$ | $O(n \log n)$ |
| Nearly sorted | $O(n)$ | $O(n)$ - $O(n \log n)$ | $O(n \log n)$ |
| Random | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Reversed | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| All equal | $O(n)$ (DNF) | $O(n)$ | $O(n \log n)$ |

Heapsort es el **único** de los tres que siempre hace $O(n \log n)$
sin importar la entrada. Esto es tanto su fortaleza (predecible) como
su debilidad (no puede aprovechar orden existente).

---

## Heapsort no es estable

### Por qué

La fase de extracción intercambia `arr[0]` (raíz del heap) con el
último elemento de la zona de heap. Este swap es de **larga distancia**:
mueve un elemento desde el inicio hasta el final del array, pasando
por encima de elementos con el mismo valor.

### Demostración

```
Input: [(3,'a'), (3,'b'), (1,'c'), (3,'d')]

Build max-heap (por valor):
  [(3,'d'), (3,'b'), (1,'c'), (3,'a')]     — ya se reordenaron los 3's

Extract max: swap arr[0] con arr[3]:
  [(3,'a'), (3,'b'), (1,'c') | (3,'d')]

Sift down: (3,'a') > (1,'c') y (3,'a') >= (3,'b'), no swap
  [(3,'a'), (3,'b'), (1,'c') | (3,'d')]

Extract max: swap arr[0] con arr[2]:
  [(1,'c'), (3,'b') | (3,'a'), (3,'d')]

Sift down: (3,'b') > (1,'c'), swap:
  [(3,'b'), (1,'c') | (3,'a'), (3,'d')]

Extract max: swap arr[0] con arr[1]:
  [(1,'c') | (3,'b'), (3,'a'), (3,'d')]

Resultado: [(1,'c'), (3,'b'), (3,'a'), (3,'d')]
Orden original de 3's: a, b, d
Orden final:           b, a, d  ← INESTABLE
```

No hay forma práctica de hacer heapsort estable sin $O(n)$ espacio
extra (que anula la ventaja de ser in-place).

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
    long cache_jumps;  /* sum of distances between consecutive accesses */
} Metrics;

static void swap_int(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

/* ── Heapsort ────────────────────────────────────────────── */

static void sift_down(int *arr, int i, int n, Metrics *m) {
    int val = arr[i];
    int last_pos = i;
    while (2 * i + 1 < n) {
        int child = 2 * i + 1;
        if (child + 1 < n) {
            m->comparisons++;
            m->cache_jumps += abs(child + 1 - child);
            if (arr[child + 1] > arr[child]) child++;
        }
        m->comparisons++;
        m->cache_jumps += abs(child - i);
        if (arr[child] <= val) break;
        arr[i] = arr[child];
        m->swaps++;
        last_pos = child;
        i = child;
    }
    arr[i] = val;
}

void heapsort(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->swaps = 0;
    m->cache_jumps = 0;

    /* Build max-heap */
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, i, n, m);

    /* Extract */
    for (int i = n - 1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        m->swaps++;
        m->cache_jumps += i;  /* swap distance */
        sift_down(arr, 0, i, m);
    }
}

/* ── Floyd's bottom-up heapsort ──────────────────────────── */

static void sift_down_floyd(int *arr, int i, int n, Metrics *m) {
    int val = arr[i];

    /* Phase 1: descend to leaf without comparing with val */
    int pos = i;
    while (2 * pos + 1 < n) {
        int child = 2 * pos + 1;
        if (child + 1 < n) {
            m->comparisons++;
            if (arr[child + 1] > arr[child]) child++;
        }
        arr[pos] = arr[child];
        m->swaps++;
        pos = child;
    }

    /* Phase 2: bubble up from leaf to correct position */
    while (pos > i) {
        int parent = (pos - 1) / 2;
        m->comparisons++;
        if (arr[parent] >= val) {
            arr[pos] = val;
            return;
        }
        arr[pos] = arr[parent];
        m->swaps++;
        pos = parent;
    }
    arr[pos] = val;
}

void heapsort_floyd(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->swaps = 0;
    m->cache_jumps = 0;

    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, i, n, m);

    for (int i = n - 1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        m->swaps++;
        sift_down_floyd(arr, 0, i, m);
    }
}

/* ── Quicksort (Hoare + median3) for comparison ──────────── */

static int qs_partition(int *arr, int lo, int hi, Metrics *m) {
    int mid = lo + (hi - lo) / 2;
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

static void insertion_range(int *arr, int lo, int hi, Metrics *m) {
    for (int i = lo + 1; i <= hi; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= lo && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j]; m->swaps++; j--;
        }
        arr[j + 1] = key;
    }
}

void quicksort_m3(int *arr, int lo, int hi, Metrics *m) {
    if (hi - lo + 1 <= 16) {
        insertion_range(arr, lo, hi, m);
        return;
    }
    int p = qs_partition(arr, lo, hi, m);
    quicksort_m3(arr, lo, p, m);
    quicksort_m3(arr, p + 1, hi, m);
}

void quicksort_wrapper(int *arr, int n, Metrics *m) {
    m->comparisons = 0; m->swaps = 0; m->cache_jumps = 0;
    quicksort_m3(arr, 0, n - 1, m);
}

/* ── Merge sort for comparison ───────────────────────────── */

static void do_merge(int *arr, int *tmp, int lo, int mid, int hi, Metrics *m) {
    memcpy(tmp + lo, arr + lo, (hi - lo + 1) * sizeof(int));
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        m->comparisons++;
        if (tmp[i] <= tmp[j]) arr[k++] = tmp[i++];
        else                   arr[k++] = tmp[j++];
        m->swaps++;
    }
    while (i <= mid) { arr[k++] = tmp[i++]; m->swaps++; }
    while (j <= hi)  { arr[k++] = tmp[j++]; m->swaps++; }
}

static void merge_sort_r(int *arr, int *tmp, int lo, int hi, Metrics *m) {
    if (hi - lo + 1 <= 16) { insertion_range(arr, lo, hi, m); return; }
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_r(arr, tmp, lo, mid, m);
    merge_sort_r(arr, tmp, mid + 1, hi, m);
    m->comparisons++;
    if (arr[mid] <= arr[mid + 1]) return;
    do_merge(arr, tmp, lo, mid, hi, m);
}

void mergesort_wrapper(int *arr, int n, Metrics *m) {
    m->comparisons = 0; m->swaps = 0; m->cache_jumps = 0;
    int *tmp = malloc(n * sizeof(int));
    merge_sort_r(arr, tmp, 0, n - 1, m);
    free(tmp);
}

/* ── Partial sort (k smallest) ───────────────────────────── */

static void sift_down_max(int *arr, int i, int n) {
    int val = arr[i];
    while (2 * i + 1 < n) {
        int c = 2 * i + 1;
        if (c + 1 < n && arr[c + 1] > arr[c]) c++;
        if (arr[c] <= val) break;
        arr[i] = arr[c];
        i = c;
    }
    arr[i] = val;
}

void partial_sort(int *arr, int n, int k) {
    /* Build max-heap of size k from first k elements */
    for (int i = k / 2 - 1; i >= 0; i--)
        sift_down_max(arr, i, k);

    /* For each remaining element, if smaller than max, replace */
    for (int i = k; i < n; i++) {
        if (arr[i] < arr[0]) {
            arr[0] = arr[i];
            sift_down_max(arr, 0, k);
        }
    }
    /* arr[0..k-1] contains the k smallest (in heap order) */
    /* Sort them if needed */
    for (int i = k - 1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        sift_down_max(arr, 0, i);
    }
}

/* ── Input generators ────────────────────────────────────── */

void fill_random(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = rand() % (n * 10);
}
void fill_sorted(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = i;
}
void fill_reversed(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = n - i;
}
void fill_nearly_sorted(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    for (int k = 0; k < n / 20; k++) {
        int x = rand() % n, y = rand() % n;
        swap_int(&a[x], &a[y]);
    }
}
void fill_all_equal(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = 42;
}

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* ── Benchmark ───────────────────────────────────────────── */

typedef void (*SortFn)(int *, int, Metrics *);

void bench(const char *name, SortFn fn, int *original, int n) {
    int *w = malloc(n * sizeof(int));
    memcpy(w, original, n * sizeof(int));
    Metrics m = {0, 0, 0};

    clock_t start = clock();
    fn(w, n, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-22s %10ld cmp  %10ld swp  %8.0f us  %s\n",
           name, m.comparisons, m.swaps, us,
           is_sorted(w, n) ? "OK" : "FAIL");
    free(w);
}

/* ── Demo 1: Three-way comparison ────────────────────────── */

void demo_comparison(int n) {
    printf("=== Demo 1: Heapsort vs Quicksort vs Merge sort, n = %d ===\n\n", n);

    typedef struct { const char *name; void (*fill)(int *, int); } Input;
    Input inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Reversed     ", fill_reversed},
        {"Nearly sorted", fill_nearly_sorted},
        {"All equal    ", fill_all_equal},
    };

    for (int t = 0; t < 5; t++) {
        int *original = malloc(n * sizeof(int));
        inputs[t].fill(original, n);
        printf("--- %s ---\n", inputs[t].name);
        bench("Heapsort", heapsort, original, n);
        bench("Heapsort (Floyd)", heapsort_floyd, original, n);
        bench("Quicksort (m3+cut)", quicksort_wrapper, original, n);
        bench("Merge sort (hybrid)", mergesort_wrapper, original, n);
        printf("\n");
        free(original);
    }
}

/* ── Demo 2: Heapsort destroys order ─────────────────────── */

void demo_destroys_order(void) {
    printf("=== Demo 2: Heapsort destroys existing order ===\n\n");

    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int n = 10;

    printf("Input (sorted):    ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    /* Build max-heap only */
    for (int i = n / 2 - 1; i >= 0; i--) {
        int val = arr[i];
        int pos = i;
        while (2 * pos + 1 < n) {
            int c = 2 * pos + 1;
            if (c + 1 < n && arr[c + 1] > arr[c]) c++;
            if (arr[c] <= val) break;
            arr[pos] = arr[c];
            pos = c;
        }
        arr[pos] = val;
    }

    printf("After build heap:  ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
    printf("All order destroyed — heapsort cannot be adaptive.\n\n");
}

/* ── Demo 3: Stability violation ─────────────────────────── */

typedef struct { int value; char tag; } Tagged;

void demo_stability(void) {
    printf("=== Demo 3: Heapsort stability violation ===\n\n");

    Tagged items[] = {
        {3,'a'}, {1,'b'}, {3,'c'}, {2,'d'}, {3,'e'}, {1,'f'}
    };
    int n = 6;

    printf("Input:  ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", items[i].value, items[i].tag);
    printf("\n");

    /* Manual heapsort on Tagged by value */
    #define TAG_SWAP(a, b) { Tagged t = a; a = b; b = t; }

    /* Build max-heap */
    for (int i = n / 2 - 1; i >= 0; i--) {
        int pos = i;
        Tagged val = items[pos];
        while (2 * pos + 1 < n) {
            int c = 2 * pos + 1;
            if (c + 1 < n && items[c + 1].value > items[c].value) c++;
            if (items[c].value <= val.value) break;
            items[pos] = items[c];
            pos = c;
        }
        items[pos] = val;
    }

    /* Extract */
    for (int i = n - 1; i > 0; i--) {
        TAG_SWAP(items[0], items[i]);
        int pos = 0;
        Tagged val = items[0];
        while (2 * pos + 1 < i) {
            int c = 2 * pos + 1;
            if (c + 1 < i && items[c + 1].value > items[c].value) c++;
            if (items[c].value <= val.value) break;
            items[pos] = items[c];
            pos = c;
        }
        items[pos] = val;
    }

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
    printf("Stable: %s\n\n", stable ? "YES" : "NO (expected)");
}

/* ── Demo 4: Partial sort ────────────────────────────────── */

void demo_partial_sort(void) {
    printf("=== Demo 4: Partial sort — find k smallest ===\n\n");

    int n = 20, k = 5;
    int arr[20];
    fill_random(arr, n);

    printf("Input (n=%d): ", n);
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    partial_sort(arr, n, k);

    printf("After partial_sort(k=%d): ", k);
    for (int i = 0; i < n; i++) {
        if (i == k) printf("| ");
        printf("%d ", arr[i]);
    }
    printf("\n");
    printf("First %d elements are the %d smallest, sorted.\n\n", k, k);
}

/* ── Demo 5: Cache access pattern ────────────────────────── */

void demo_cache_pattern(void) {
    printf("=== Demo 5: Sift-down access positions (n=1024) ===\n\n");
    printf("Sift-down from root visits:\n");

    int n = 1024;
    int pos = 0;
    printf("  Level  Position  Distance(bytes)\n");
    printf("  ─────  ────────  ───────────────\n");
    int level = 0;
    while (2 * pos + 1 < n) {
        int child = 2 * pos + 1;
        int dist = (child - pos) * (int)sizeof(int);
        printf("  %5d  %8d  %15d", level, pos, dist);
        if (dist <= 64)        printf("  (same cache line)\n");
        else if (dist <= 32768) printf("  (L1)\n");
        else if (dist <= 262144) printf("  (L2)\n");
        else                    printf("  (L3/RAM)\n");
        pos = child;
        level++;
    }
    printf("\n  Total levels: %d, final position: %d\n", level, pos);
    printf("  Each extract does this traversal — exponential jumps!\n\n");
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        Heapsort in Context — Analysis                ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_destroys_order();
    demo_stability();
    demo_cache_pattern();
    demo_partial_sort();
    demo_comparison(10000);
    demo_comparison(100000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o heapsort_ctx heapsort_ctx.c
./heapsort_ctx
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
}

/* ── Heapsort ────────────────────────────────────────────── */

fn sift_down(arr: &mut [i32], mut i: usize, n: usize, m: &mut Metrics) {
    let val = arr[i];
    while 2 * i + 1 < n {
        let mut child = 2 * i + 1;
        if child + 1 < n {
            m.comparisons += 1;
            if arr[child + 1] > arr[child] { child += 1; }
        }
        m.comparisons += 1;
        if arr[child] <= val { break; }
        arr[i] = arr[child];
        m.swaps += 1;
        i = child;
    }
    arr[i] = val;
}

fn heapsort(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 { return; }

    for i in (0..n / 2).rev() {
        sift_down(arr, i, n, m);
    }
    for i in (1..n).rev() {
        arr.swap(0, i);
        m.swaps += 1;
        sift_down(arr, 0, i, m);
    }
}

/* ── Floyd's bottom-up heapsort ──────────────────────────── */

fn sift_down_floyd(arr: &mut [i32], start: usize, n: usize, m: &mut Metrics) {
    let val = arr[start];
    let mut pos = start;

    /* Descend to leaf */
    while 2 * pos + 1 < n {
        let mut child = 2 * pos + 1;
        if child + 1 < n {
            m.comparisons += 1;
            if arr[child + 1] > arr[child] { child += 1; }
        }
        arr[pos] = arr[child];
        m.swaps += 1;
        pos = child;
    }

    /* Bubble up */
    while pos > start {
        let parent = (pos - 1) / 2;
        m.comparisons += 1;
        if arr[parent] >= val {
            arr[pos] = val;
            return;
        }
        arr[pos] = arr[parent];
        m.swaps += 1;
        pos = parent;
    }
    arr[pos] = val;
}

fn heapsort_floyd(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 { return; }

    for i in (0..n / 2).rev() {
        sift_down(arr, i, n, m);
    }
    for i in (1..n).rev() {
        arr.swap(0, i);
        m.swaps += 1;
        sift_down_floyd(arr, 0, i, m);
    }
}

/* ── Quicksort (median3 + cutoff) for comparison ─────────── */

fn insertion_range(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
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

fn qs_partition(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) -> usize {
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

fn quicksort_r(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if hi - lo + 1 <= 16 {
        insertion_range(arr, lo, hi, m);
        return;
    }
    let p = qs_partition(arr, lo, hi, m);
    quicksort_r(arr, lo, p, m);
    quicksort_r(arr, p + 1, hi, m);
}

fn quicksort_wrapper(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 { return; }
    quicksort_r(arr, 0, n - 1, m);
}

/* ── Merge sort for comparison ───────────────────────────── */

fn do_merge(arr: &mut [i32], tmp: &mut [i32], lo: usize, mid: usize, hi: usize, m: &mut Metrics) {
    tmp[lo..=hi].copy_from_slice(&arr[lo..=hi]);
    let (mut i, mut j, mut k) = (lo, mid + 1, lo);
    while i <= mid && j <= hi {
        m.comparisons += 1;
        if tmp[i] <= tmp[j] { arr[k] = tmp[i]; i += 1; }
        else                 { arr[k] = tmp[j]; j += 1; }
        m.swaps += 1;
        k += 1;
    }
    while i <= mid { arr[k] = tmp[i]; m.swaps += 1; i += 1; k += 1; }
    while j <= hi  { arr[k] = tmp[j]; m.swaps += 1; j += 1; k += 1; }
}

fn merge_sort_r(arr: &mut [i32], tmp: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if hi - lo + 1 <= 16 { insertion_range(arr, lo, hi, m); return; }
    if lo >= hi { return; }
    let mid = lo + (hi - lo) / 2;
    merge_sort_r(arr, tmp, lo, mid, m);
    merge_sort_r(arr, tmp, mid + 1, hi, m);
    m.comparisons += 1;
    if arr[mid] <= arr[mid + 1] { return; }
    do_merge(arr, tmp, lo, mid, hi, m);
}

fn mergesort_wrapper(arr: &mut [i32], m: &mut Metrics) {
    let n = arr.len();
    if n <= 1 { return; }
    let mut tmp = vec![0i32; n];
    merge_sort_r(arr, &mut tmp, 0, n - 1, m);
}

/* ── Partial sort ────────────────────────────────────────── */

fn partial_sort(arr: &mut [i32], k: usize) {
    let n = arr.len();
    if k == 0 || k > n { return; }

    /* Build max-heap of first k elements */
    for i in (0..k / 2).rev() {
        let mut pos = i;
        let val = arr[pos];
        while 2 * pos + 1 < k {
            let mut c = 2 * pos + 1;
            if c + 1 < k && arr[c + 1] > arr[c] { c += 1; }
            if arr[c] <= val { break; }
            arr[pos] = arr[c];
            pos = c;
        }
        arr[pos] = val;
    }

    /* Replace root if smaller */
    for i in k..n {
        if arr[i] < arr[0] {
            arr[0] = arr[i];
            let mut pos = 0;
            let val = arr[0];
            while 2 * pos + 1 < k {
                let mut c = 2 * pos + 1;
                if c + 1 < k && arr[c + 1] > arr[c] { c += 1; }
                if arr[c] <= val { break; }
                arr[pos] = arr[c];
                pos = c;
            }
            arr[pos] = val;
        }
    }

    /* Sort the k elements */
    for i in (1..k).rev() {
        arr.swap(0, i);
        let mut pos = 0;
        let val = arr[0];
        while 2 * pos + 1 < i {
            let mut c = 2 * pos + 1;
            if c + 1 < i && arr[c + 1] > arr[c] { c += 1; }
            if arr[c] <= val { break; }
            arr[pos] = arr[c];
            pos = c;
        }
        arr[pos] = val;
    }
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xFACE_FEED).hash(&mut h);
        (h.finish() % (n as u64 * 10)) as i32
    }).collect()
}
fn fill_sorted(n: usize) -> Vec<i32> { (0..n as i32).collect() }
fn fill_reversed(n: usize) -> Vec<i32> { (0..n as i32).rev().collect() }
fn fill_nearly_sorted(n: usize) -> Vec<i32> {
    let mut v: Vec<i32> = (0..n as i32).collect();
    let p = (n / 20).max(1);
    for k in 0..p { let i = (k * 7 + 13) % n; let j = (k * 11 + 3) % n; v.swap(i, j); }
    v
}
fn fill_all_equal(n: usize) -> Vec<i32> { vec![42; n] }

fn is_sorted(a: &[i32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }

/* ── Benchmark helper ────────────────────────────────────── */

fn bench(name: &str, original: &[i32], sort_fn: fn(&mut [i32], &mut Metrics)) {
    let mut w = original.to_vec();
    let mut m = Metrics::default();
    let start = Instant::now();
    sort_fn(&mut w, &mut m);
    let elapsed = start.elapsed().as_micros();
    let ok = if is_sorted(&w) { "OK" } else { "FAIL" };
    println!("  {:<22} {:>10} cmp  {:>10} swp  {:>8} us  {}",
        name, m.comparisons, m.swaps, elapsed, ok);
}

/* ── Demo 1: Three-way comparison ────────────────────────── */

fn demo_comparison(n: usize) {
    println!("=== Heapsort vs Quicksort vs Merge sort, n = {} ===\n", n);

    let names = ["Random", "Sorted", "Reversed", "Nearly sorted", "All equal"];
    let gens: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_sorted, fill_reversed, fill_nearly_sorted, fill_all_equal,
    ];

    for (ti, gen) in gens.iter().enumerate() {
        let original = gen(n);
        println!("--- {} ---", names[ti]);
        bench("Heapsort", &original, heapsort);
        bench("Heapsort (Floyd)", &original, heapsort_floyd);
        bench("Quicksort (m3+cut)", &original, quicksort_wrapper);
        bench("Merge sort (hybrid)", &original, mergesort_wrapper);
        println!();
    }
}

/* ── Demo 2: Order destruction ───────────────────────────── */

fn demo_destroys_order() {
    println!("=== Heapsort destroys existing order ===\n");
    let mut arr: Vec<i32> = (1..=10).collect();
    println!("Input (sorted):   {:?}", arr);

    let n = arr.len();
    for i in (0..n / 2).rev() {
        let mut pos = i;
        let val = arr[pos];
        while 2 * pos + 1 < n {
            let mut c = 2 * pos + 1;
            if c + 1 < n && arr[c + 1] > arr[c] { c += 1; }
            if arr[c] <= val { break; }
            arr[pos] = arr[c];
            pos = c;
        }
        arr[pos] = val;
    }
    println!("After build heap: {:?}", arr);
    println!("All original order destroyed.\n");
}

/* ── Demo 3: Stability violation ─────────────────────────── */

fn demo_stability() {
    println!("=== Heapsort stability violation ===\n");

    let mut items: Vec<(i32, char)> = vec![
        (3,'a'), (1,'b'), (3,'c'), (2,'d'), (3,'e'), (1,'f'),
    ];
    let n = items.len();

    println!("Input:  {:?}", items);

    /* Build max-heap by value */
    for i in (0..n / 2).rev() {
        let mut pos = i;
        let val = items[pos];
        while 2 * pos + 1 < n {
            let mut c = 2 * pos + 1;
            if c + 1 < n && items[c + 1].0 > items[c].0 { c += 1; }
            if items[c].0 <= val.0 { break; }
            items[pos] = items[c];
            pos = c;
        }
        items[pos] = val;
    }

    /* Extract */
    for i in (1..n).rev() {
        items.swap(0, i);
        let mut pos = 0;
        let val = items[0];
        while 2 * pos + 1 < i {
            let mut c = 2 * pos + 1;
            if c + 1 < i && items[c + 1].0 > items[c].0 { c += 1; }
            if items[c].0 <= val.0 { break; }
            items[pos] = items[c];
            pos = c;
        }
        items[pos] = val;
    }

    println!("Output: {:?}", items);
    let stable = items.windows(2).all(|w| w[0].0 != w[1].0 || w[0].1 < w[1].1);
    println!("Stable: {} (expected: NO)\n", if stable { "YES" } else { "NO" });
}

/* ── Demo 4: Partial sort ────────────────────────────────── */

fn demo_partial_sort() {
    println!("=== Partial sort — find k smallest ===\n");

    let mut arr = fill_random(20);
    let k = 5;
    println!("Input (n=20): {:?}", arr);
    partial_sort(&mut arr, k);
    println!("After partial_sort(k={}): {:?}", k, &arr[..k]);
    println!("First {} are the {} smallest, sorted.\n", k, k);
}

/* ── Demo 5: Cache access pattern ────────────────────────── */

fn demo_cache_pattern() {
    println!("=== Sift-down access positions (n=1024) ===\n");
    println!("  Level  Position  Distance(bytes)");
    println!("  ─────  ────────  ───────────────");

    let n = 1024;
    let mut pos = 0usize;
    let mut level = 0;
    while 2 * pos + 1 < n {
        let child = 2 * pos + 1;
        let dist = (child - pos) * std::mem::size_of::<i32>();
        let cache = if dist <= 64 { "(same cache line)" }
            else if dist <= 32768 { "(L1)" }
            else if dist <= 262144 { "(L2)" }
            else { "(L3/RAM)" };
        println!("  {:>5}  {:>8}  {:>15}  {}", level, pos, dist, cache);
        pos = child;
        level += 1;
    }
    println!("\n  Total levels: {}, exponential jumps cause cache misses.\n", level);
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║        Heapsort in Context — Analysis                ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_destroys_order();
    demo_stability();
    demo_cache_pattern();
    demo_partial_sort();
    demo_comparison(10_000);
    demo_comparison(100_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o heapsort_ctx_rs heapsort_ctx.rs
./heapsort_ctx_rs
```

---

## Ejercicios

### Ejercicio 1 — Heapsort vs quicksort: conteo de comparaciones

Para $n = 100000$ con datos aleatorios, cuenta las comparaciones de
heapsort, quicksort (mediana de tres) y merge sort. Verifica que
heapsort hace ~$2n \log_2 n$, quicksort ~$1.39\, n \log_2 n$ y merge
sort ~$n \log_2 n$.

<details><summary>Predicción</summary>

Para $n = 100000$ ($\log_2 n \approx 16.6$):
- Heapsort: $\sim 2 \times 100000 \times 16.6 = 3.32M$ comparaciones.
- Quicksort: $\sim 1.39 \times 100000 \times 16.6 = 2.31M$ comparaciones.
- Merge sort: $\sim 100000 \times 16.6 = 1.66M$ comparaciones.

Ratio: heapsort/merge $\approx 2.0$, heapsort/quicksort $\approx 1.44$.
Heapsort hace el mayor número de comparaciones porque sift-down necesita
2 comparaciones por nivel (comparar los dos hijos + comparar ganador
con padre).

</details>

### Ejercicio 2 — Floyd's bottom-up heapsort

Implementa la optimización de Floyd y compara comparaciones con
heapsort estándar para $n = 100000$.

<details><summary>Predicción</summary>

Heapsort estándar: $\sim 2n \log_2 n \approx 3.32M$ comparaciones
(2 por nivel: hijos + padre).

Floyd's bottom-up: $\sim n \log_2 n + O(n) \approx 1.76M$ comparaciones
(1 por nivel en la bajada + ~1-2 en la subida). Reducción de ~47%.

Sin embargo, el tiempo real podría mejorar solo ~10-15% porque:
- La bajada hace los mismos accesos a memoria (mismos cache misses).
- La subida agrega accesos adicionales (aunque pocos).
- El cuello de botella sigue siendo el cache, no las comparaciones.

</details>

### Ejercicio 3 — Destrucción de orden

Crea un array casi ordenado (95% en orden) de $n = 1000$. Muestra el
array antes y después de build max-heap. Cuenta cuántas posiciones
conservan su valor original.

<details><summary>Predicción</summary>

Después de build max-heap, el array está completamente reorganizado
según la propiedad de heap (padre $\geq$ hijos). Para un array casi
ordenado ascendente, build max-heap moverá los elementos grandes hacia
la raíz y los pequeños hacia las hojas — esencialmente invirtiendo la
tendencia.

De los 1000 elementos, se espera que solo ~5-10% estén en su posición
original (los que por coincidencia ya satisfacen la propiedad de heap
en su posición). Esto contrasta con quicksort/merge sort optimizados,
que pueden beneficiarse del orden existente.

</details>

### Ejercicio 4 — Partial sort vs full sort

Compara el tiempo de `partial_sort(arr, n, k)` vs `heapsort(arr, n)`
seguido de tomar los primeros $k$, para $n = 100000$ y
$k \in \{10, 100, 1000, 10000\}$.

<details><summary>Predicción</summary>

| $k$ | Partial sort | Full heapsort | Ratio |
|-----|-------------|---------------|-------|
| 10 | $O(n + 10 \log n) \approx 100166$ | $O(n \log n) \approx 1.66M$ | ~16x más rápido |
| 100 | $O(n + 100 \log n) \approx 101660$ | $1.66M$ | ~16x |
| 1000 | $O(n + 1000 \log n) \approx 116600$ | $1.66M$ | ~14x |
| 10000 | $O(n + 10000 \log n) \approx 266000$ | $1.66M$ | ~6x |

Partial sort es significativamente más rápido para $k \ll n$. Para
$k = n$, partial sort degenera en heapsort. La ventaja es proporcional
a $n/k$.

</details>

### Ejercicio 5 — Heapsort en introsort: frecuencia del fallback

Implementa introsort con contadores. Para $n = 100000$ con datos
aleatorios, ¿cuántas veces se activa el fallback a heapsort? Repite
100 veces.

<details><summary>Predicción</summary>

Con mediana de tres y `depth_limit = 2 \lfloor \log_2 n \rfloor = 34`,
y datos **aleatorios**, la profundidad esperada de quicksort es ~$2
\log_2 n \approx 33$, que está justo en el límite.

En la práctica, el fallback se activa en ~0-5% de las ejecuciones para
datos aleatorios (la mediana de tres produce particiones suficientemente
buenas). De las 100 ejecuciones, se espera que ~0-5 activen heapsort,
y cuando lo hacen, es en subarrays pequeños (1000-5000 elementos).

Para datos patológicos (sorted con pivote fijo), el fallback se
activaría en el 100% de las ejecuciones.

</details>

### Ejercicio 6 — Cache misses simulados

Instrumenta heapsort y quicksort con un contador de "saltos largos"
(accesos a posiciones > 64 bytes de distancia del acceso anterior).
Compara para $n = 100000$.

<details><summary>Predicción</summary>

**Quicksort**: la partición recorre el array secuencialmente. Cada
acceso está a 4 bytes del anterior (un `int`). Una línea de cache
es 64 bytes = 16 ints. Saltos largos: solo al cambiar de subarray
en la recursión. Estimado: ~$n / 16 \times \log_2 n \approx 100000$
saltos largos.

**Heapsort**: cada sift-down hace saltos exponenciales. De los ~$\log_2
n = 17$ niveles, los primeros ~4 están dentro de la misma línea de
cache, los siguientes ~13 son saltos largos. Con $n$ extracciones:
$\sim 13n = 1.3M$ saltos largos.

Heapsort tiene ~13x más saltos largos, lo que se traduce directamente
en más cache misses y mayor latencia total.

</details>

### Ejercicio 7 — Ternary heapsort

Implementa heapsort con un heap ternario (3 hijos por nodo en lugar
de 2). Los hijos de $i$ están en $3i + 1$, $3i + 2$, $3i + 3$.
Compara profundidad y comparaciones con heapsort binario.

<details><summary>Predicción</summary>

Heap ternario tiene profundidad $\log_3 n$ vs $\log_2 n$. Para
$n = 100000$: $\log_3 n \approx 10.5$ vs $\log_2 n \approx 16.6$.
Menos niveles = menos swaps en sift-down.

Pero cada nivel necesita 3 comparaciones (encontrar el máximo de 3
hijos + comparar con padre = 3 cmp) vs 2 para binario. Total
comparaciones: $3 \times 10.5 \approx 31.5$ vs $2 \times 16.6 \approx
33.2$ por sift-down — ligeramente menos.

El beneficio real es que el heap ternario tiene **mejor localidad de
cache**: los 3 hijos están en posiciones consecutivas en memoria (vs
2 para binario), y la menor profundidad significa menos saltos totales.
Speedup esperado: ~5-15%.

</details>

### Ejercicio 8 — Heapsort para strings

Implementa heapsort para un array de strings (usando `strcmp`). Compara
con `qsort` para $n = 10000$ strings aleatorias de longitud 10-50.

<details><summary>Predicción</summary>

Para strings, la comparación es $O(L)$ donde $L$ es la longitud del
string. El costo relativo de comparación vs movimiento cambia:
- Comparación: ~50-100 ns (recorrer caracteres).
- Movimiento: ~8 ns (mover un puntero de 8 bytes).

Con comparaciones caras, el mayor número de comparaciones de heapsort
($2n \log_2 n$ vs $1.39\, n \log_2 n$) se siente más. Heapsort será
~40-50% más lento que quicksort, no solo ~30% como con ints.

Además, los strings están dispersos en memoria (heap-allocated), así
que incluso quicksort tiene mala localidad de cache para el contenido
de los strings. La ventaja de cache de quicksort se reduce.

</details>

### Ejercicio 9 — Heapsort estable con índices

Haz heapsort estable augmentando cada elemento con su índice original
y usando `(value, index)` como clave de comparación. Mide el overhead
vs heapsort normal.

<details><summary>Predicción</summary>

Al comparar por `(value, index)`, dos elementos con el mismo valor se
ordenan por su índice original — preservando la estabilidad.

Overhead:
- Cada comparación ahora compara un `int` y potencialmente un segundo
  `int` (el índice, cuando los valores son iguales). Para datos con
  muchos duplicados, esto agrega ~30-50% comparaciones extra.
- Cada elemento ocupa 8 bytes en lugar de 4 (o 16 con padding).
  Peor localidad de cache.

Speedup negativo: ~20-40% más lento que heapsort normal.

Conclusión: si necesitas estabilidad + $O(n \log n)$ garantizado,
usa merge sort. Hacer heapsort estable elimina su ventaja (in-place +
rápido) sin ganar nada.

</details>

### Ejercicio 10 — Perfil de heapsort para distintos tamaños

Mide el tiempo de heapsort, quicksort y merge sort para
$n \in \{10^3, 10^4, 10^5, 10^6\}$ con datos aleatorios. Calcula
el ratio heapsort/quicksort para cada $n$.

<details><summary>Predicción</summary>

| $n$ | Heapsort (ms) | Quicksort (ms) | Ratio |
|-----|--------------|----------------|-------|
| $10^3$ | ~0.05 | ~0.03 | ~1.7x |
| $10^4$ | ~0.6 | ~0.3 | ~2.0x |
| $10^5$ | ~8 | ~4 | ~2.0x |
| $10^6$ | ~130 | ~65 | ~2.0x |

El ratio debería ser **estable** alrededor de 2.0x para todos los
tamaños, porque la desventaja de heapsort (cache misses) es proporcional
a $n \log n$ igual que su trabajo computacional.

Para $n$ muy grande ($> 10^7$), el ratio puede crecer ligeramente
porque los niveles profundos del heap caen fuera de L3 cache (latencia
de RAM: ~100 ns vs L3: ~10 ns).

</details>
