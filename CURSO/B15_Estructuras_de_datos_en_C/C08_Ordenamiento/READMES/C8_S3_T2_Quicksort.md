# Quicksort

## Objetivo

Dominar quicksort como el algoritmo de ordenamiento más rápido en la
práctica para datos en RAM:

- **Partición** con pivote: reorganizar el array para que todos los
  menores queden a la izquierda y todos los mayores a la derecha.
- $O(n \log n)$ en promedio, $O(n^2)$ en el peor caso.
- In-place (solo $O(\log n)$ de stack), no estable.
- Estrategias de pivote: primero, último, mediana de tres, aleatorio.
- Por qué es más rápido que merge sort en la práctica pese a tener
  peor caso peor.

---

## El principio: particionar y conquistar

Quicksort también es divide and conquer, pero invierte la estructura
respecto a merge sort:

| | Merge sort | Quicksort |
|---|-----------|-----------|
| Dividir | Trivial ($O(1)$, por la mitad) | **Costoso** ($O(n)$, partición) |
| Conquistar | Recursión en ambas mitades | Recursión en ambas particiones |
| Combinar | **Costoso** ($O(n)$, merge) | Trivial ($O(0)$, nada) |

En merge sort, el trabajo está en **combinar**. En quicksort, el trabajo
está en **dividir**. Después de la partición, los elementos ya están en
su lado correcto — no hay paso de combinación.

### Pseudocódigo

```
QUICKSORT(arr, lo, hi):
    si lo >= hi: retornar
    p = PARTITION(arr, lo, hi)     // p = posición final del pivote
    QUICKSORT(arr, lo, p - 1)     // ordenar lado izquierdo
    QUICKSORT(arr, p + 1, hi)     // ordenar lado derecho

PARTITION(arr, lo, hi):            // esquema Lomuto
    pivot = arr[hi]                // pivote = último elemento
    i = lo - 1                     // frontera de "menores"
    para j de lo a hi-1:
        si arr[j] <= pivot:
            i++
            swap(arr[i], arr[j])
        // si arr[j] > pivot: no hacer nada (queda a la derecha)
    swap(arr[i+1], arr[hi])        // colocar pivote en su posición
    retornar i + 1
```

### Invariante de la partición Lomuto

Durante la ejecución de `PARTITION`, el array se divide en cuatro
zonas:

```
 lo              i        j              hi
  [  <= pivot  |  > pivot  |  sin ver  | pivot ]
```

- `arr[lo..i]`: elementos $\leq$ pivot (ya procesados, lado izquierdo).
- `arr[i+1..j-1]`: elementos $>$ pivot (ya procesados, lado derecho).
- `arr[j..hi-1]`: elementos sin procesar.
- `arr[hi]`: el pivote.

Al terminar ($j = hi$), se intercambia `arr[i+1]` con `arr[hi]` para
colocar el pivote entre las dos particiones.

---

## Traza detallada

Array inicial: `[3, 7, 8, 5, 2, 1, 9, 5, 4]`, pivote = último.

### Partición del array completo

```
Pivote = arr[8] = 4
i = -1, recorrer j de 0 a 7:

j=0: arr[0]=3 <= 4  → i=0, swap(arr[0],arr[0]) → [3, 7, 8, 5, 2, 1, 9, 5, 4]
j=1: arr[1]=7 > 4   → nada
j=2: arr[2]=8 > 4   → nada
j=3: arr[3]=5 > 4   → nada
j=4: arr[4]=2 <= 4  → i=1, swap(arr[1],arr[4]) → [3, 2, 8, 5, 7, 1, 9, 5, 4]
j=5: arr[5]=1 <= 4  → i=2, swap(arr[2],arr[5]) → [3, 2, 1, 5, 7, 8, 9, 5, 4]
j=6: arr[6]=9 > 4   → nada
j=7: arr[7]=5 > 4   → nada

Colocar pivote: swap(arr[3], arr[8]) → [3, 2, 1, 4, 7, 8, 9, 5, 5]
                                              ↑
                                         pivote en posición 3

Resultado: [3, 2, 1 | 4 | 7, 8, 9, 5, 5]
            ≤ 4       =     > 4
```

El pivote 4 queda en su **posición final definitiva** (índice 3). Ahora
se aplica quicksort recursivamente a `[3, 2, 1]` y `[7, 8, 9, 5, 5]`.

### Árbol de recursión completo

```
                    [3,7,8,5,2,1,9,5,4]
                    pivot=4, pos=3
                   /                   \
           [3,2,1]                    [7,8,9,5,5]
           pivot=1, pos=0             pivot=5, pos=5
          /       \                  /             \
        []      [3,2]           [5]              [9,8,7]
               pivot=2,pos=1                   pivot=7,pos=7
              /         \                     /           \
           [2]         [3]                 [7]           [9,8]
                                                       pivot=8,pos=8
                                                      /         \
                                                   [8]          [9]
```

---

## Partición de Hoare

La partición de Lomuto es más fácil de entender, pero la partición de
Hoare es más eficiente: hace aproximadamente **la mitad de swaps** que
Lomuto en promedio.

### Pseudocódigo de Hoare

```
HOARE_PARTITION(arr, lo, hi):
    pivot = arr[lo]                // pivote = primer elemento
    i = lo - 1
    j = hi + 1
    repetir:
        hacer: i++ hasta arr[i] >= pivot
        hacer: j-- hasta arr[j] <= pivot
        si i >= j: retornar j
        swap(arr[i], arr[j])
```

### Diferencias clave

| Aspecto | Lomuto | Hoare |
|---------|--------|-------|
| Pivote | Último elemento | Primer elemento (o cualquiera) |
| Punteros | 1 (`j` avanza, `i` marca frontera) | 2 (convergen desde extremos) |
| Swaps (aleatorio) | $\approx n/3$ | $\approx n/6$ |
| Retorno | Posición exacta del pivote | Punto de partición (pivote puede estar en cualquier lado) |
| Recursión después | `(lo, p-1)` y `(p+1, hi)` | `(lo, j)` y `(j+1, hi)` |
| Complejidad de código | Más simple | Más sutil (off-by-one errors) |

### Traza de Hoare

Array: `[3, 7, 8, 5, 2, 1, 9, 5, 4]`, pivote = `arr[0]` = 3.

```
i empieza en -1, j empieza en 9

Iteración 1:
  i avanza: i=0, arr[0]=3 >= 3 → para
  j retrocede: j=8, arr[8]=4 > 3 → continúa
               j=7, arr[7]=5 > 3 → continúa
               j=6, arr[6]=9 > 3 → continúa
               j=5, arr[5]=1 <= 3 → para
  i(0) < j(5): swap(arr[0], arr[5]) → [1, 7, 8, 5, 2, 3, 9, 5, 4]

Iteración 2:
  i avanza: i=1, arr[1]=7 >= 3 → para
  j retrocede: j=4, arr[4]=2 <= 3 → para
  i(1) < j(4): swap(arr[1], arr[4]) → [1, 2, 8, 5, 7, 3, 9, 5, 4]

Iteración 3:
  i avanza: i=2, arr[2]=8 >= 3 → para
  j retrocede: j=3, arr[3]=5 > 3 → continúa
               j=2, arr[2]=8 > 3 → continúa
               j=1, arr[1]=2 <= 3 → para
  i(2) >= j(1): retornar j=1

Partición: [1, 2 | 8, 5, 7, 3, 9, 5, 4]
Recursión: quicksort(0,1) y quicksort(2,8)
```

Hoare hizo 2 swaps vs los 3 que haría Lomuto. Para arrays grandes,
la diferencia se acumula.

### Por qué Hoare con pivote = primer elemento y datos ordenados es desastroso

Si el array está ordenado `[1, 2, 3, 4, 5]` y el pivote es `arr[lo] = 1`:
- `i` avanza a posición 0 (1 >= 1).
- `j` retrocede a posición 0 (todos son >= 1, baja hasta encontrar 1).
- La partición es (0, 0) y (1, 4) — partición completamente
  desbalanceada.

Esto produce $n - 1$ niveles de recursión, cada uno con $O(n)$ trabajo:
$O(n^2)$ total. Lo mismo pasa con Lomuto cuando el pivote es el máximo
o mínimo.

---

## Estrategias de selección de pivote

La elección del pivote determina la calidad de la partición y, por
tanto, el rendimiento. El pivote ideal divide el array en dos mitades
iguales, pero encontrar la mediana exacta cuesta $O(n)$ adicional.

### Pivote fijo (primero o último)

```c
int pivot = arr[hi];  /* Lomuto: último */
int pivot = arr[lo];  /* Hoare: primero */
```

- **Ventaja**: simple, sin overhead.
- **Desventaja**: $O(n^2)$ para datos ordenados o reversos, que son
  entradas comunes en la práctica.

### Pivote aleatorio

```c
int rand_idx = lo + rand() % (hi - lo + 1);
swap(&arr[rand_idx], &arr[hi]);
int pivot = arr[hi];
```

- **Ventaja**: $O(n^2)$ solo con probabilidad $1/n!$ (prácticamente
  imposible). El caso esperado es siempre $O(n \log n)$.
- **Desventaja**: `rand()` tiene overhead (~10 ns por llamada) y
  necesita una buena fuente de aleatoriedad.

### Mediana de tres

```c
int mid = lo + (hi - lo) / 2;
/* Ordenar arr[lo], arr[mid], arr[hi] y usar arr[mid] como pivote */
if (arr[lo] > arr[mid]) swap(&arr[lo], &arr[mid]);
if (arr[lo] > arr[hi])  swap(&arr[lo], &arr[hi]);
if (arr[mid] > arr[hi]) swap(&arr[mid], &arr[hi]);
swap(&arr[mid], &arr[hi - 1]);  /* mover pivote junto al final */
int pivot = arr[hi - 1];
```

Se elige la mediana de `arr[lo]`, `arr[mid]`, `arr[hi]`:
- **Ventaja**: elimina el peor caso para datos ordenados, reversos y
  pipe organ. Particiones más balanceadas en promedio.
- **Desventaja**: sigue siendo posible construir entradas adversariales
  (aunque es difícil en la práctica). 3 comparaciones extra.

### Mediana de medianas (Ninther)

Introsort en GCC usa "ninther": toma 9 muestras equiespaciadas, calcula
la mediana de cada grupo de 3, y luego la mediana de esas 3 medianas.
Esto es más robusto que mediana de tres pero con más overhead.

### Comparación

| Estrategia | Overhead | Peor caso | Datos ordenados |
|------------|----------|-----------|-----------------|
| Fijo (último) | 0 | $O(n^2)$ | $O(n^2)$ |
| Aleatorio | ~10 ns/call | $O(n^2)$ con prob $\approx 0$ | $O(n \log n)$ esperado |
| Mediana de 3 | 3 cmp + 2-3 swaps | $O(n^2)$ (adversarial) | $O(n \log n)$ |
| Ninther | ~12 cmp | $O(n^2)$ (muy difícil) | $O(n \log n)$ |

---

## Análisis de complejidad

### Caso promedio: $O(n \log n)$

Si cada partición divide el array en una fracción constante (no
necesariamente la mitad), el número de niveles es $O(\log n)$ y cada
nivel procesa $O(n)$ elementos en total.

Formalmente, si el pivote cae en la posición $k$ (con $k$ uniformemente
distribuido en $[0, n-1]$), la recurrencia es:

$$T(n) = \frac{1}{n} \sum_{k=0}^{n-1} \left[ T(k) + T(n-1-k) \right] + cn$$

Resolviendo: $T(n) \approx 2n \ln n \approx 1.39\, n \log_2 n$.

Quicksort hace ~39% más comparaciones que merge sort en promedio, pero
es más rápido en la práctica por mejor localidad de cache.

### Peor caso: $O(n^2)$

Ocurre cuando cada partición produce una partición de tamaño 0 y otra
de tamaño $n-1$ (el pivote es siempre el mínimo o el máximo):

$$T(n) = T(n-1) + T(0) + cn = T(n-1) + cn$$

$$T(n) = c \sum_{k=1}^{n} k = \frac{cn(n+1)}{2} = O(n^2)$$

Escenarios que causan peor caso con pivote fijo:
- Array ya ordenado (pivote = último = máximo).
- Array reverso (pivote = último = mínimo).
- Todos los elementos iguales (con Lomuto sin manejo especial).

### Mejor caso: $O(n \log n)$

Cada partición divide exactamente a la mitad:

$$T(n) = 2T(n/2) + cn \implies T(n) = cn \log_2 n$$

### Espacio

Quicksort es in-place (no necesita buffer auxiliar), pero usa stack
para la recursión:
- Caso promedio: $O(\log n)$ profundidad de stack.
- Peor caso: $O(n)$ profundidad de stack (particiones degeneradas).

Con la optimización **tail call** (recurrir primero en la partición
más pequeña), el stack se limita a $O(\log n)$ incluso en el peor caso.

| Caso | Comparaciones | Swaps | Stack |
|------|--------------|-------|-------|
| Mejor | $n \log_2 n$ | $\approx 0.3\, n \log_2 n$ | $O(\log n)$ |
| Promedio | $1.39\, n \log_2 n$ | $\approx 0.7\, n \log_2 n$ | $O(\log n)$ |
| Peor | $n^2/2$ | $n^2/4$ | $O(n)$ |

---

## Por qué quicksort es más rápido que merge sort en la práctica

A pesar de hacer ~39% más comparaciones en promedio y tener peor caso
$O(n^2)$, quicksort es consistentemente más rápido que merge sort para
datos en RAM. Las razones:

### 1. Localidad de cache

La partición recorre el array **secuencialmente** con dos punteros
que convergen. Todos los accesos son a posiciones cercanas, lo que
maximiza los cache hits en L1/L2.

Merge sort necesita un buffer auxiliar y alterna entre el array
original y el buffer. Estos dos arrays pueden competir por las mismas
líneas de cache, causando conflictos.

### 2. Sin asignación de memoria

Quicksort es in-place. Merge sort necesita `malloc` de $O(n)$ bytes.
El `malloc` tiene overhead propio, y el buffer puede no estar en cache.

### 3. Operaciones más simples

Cada iteración de la partición hace: una comparación + posiblemente un
swap. No hay copia a buffer intermedio. El compilador genera código
más compacto que cabe mejor en la cache de instrucciones.

### 4. Predicción de ramas

En Lomuto, la comparación `arr[j] <= pivot` tiene ~50% de probabilidad
de ser verdadera para datos aleatorios, similar a merge sort. Pero en
Hoare, los dos bucles internos (`while arr[i] < pivot` y
`while arr[j] > pivot`) suelen iterar varias veces antes de parar,
lo que el predictor de ramas maneja bien.

### Medición empírica (aproximada)

| Algoritmo | $n = 10^6$ aleatorio (ms) | Razón principal |
|-----------|--------------------------|-----------------|
| Quicksort (Hoare, mediana 3) | ~65 | Cache + in-place |
| Merge sort (optimizado) | ~90 | Buffer auxiliar |
| Heapsort | ~130 | Cache terrible |

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
} Metrics;

static void swap_int(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

/* ── Lomuto partition ────────────────────────────────────── */

int lomuto_partition(int *arr, int lo, int hi, Metrics *m) {
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

int hoare_partition(int *arr, int lo, int hi, Metrics *m) {
    int pivot = arr[lo + (hi - lo) / 2];
    int i = lo - 1;
    int j = hi + 1;
    for (;;) {
        do { i++; m->comparisons++; } while (arr[i] < pivot);
        do { j--; m->comparisons++; } while (arr[j] > pivot);
        if (i >= j) return j;
        swap_int(&arr[i], &arr[j]);
        m->swaps++;
    }
}

/* ── Quicksort with Lomuto ───────────────────────────────── */

void quicksort_lomuto(int *arr, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int p = lomuto_partition(arr, lo, hi, m);
    quicksort_lomuto(arr, lo, p - 1, m);
    quicksort_lomuto(arr, p + 1, hi, m);

    m->current_depth--;
}

/* ── Quicksort with Hoare ────────────────────────────────── */

void quicksort_hoare(int *arr, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int p = hoare_partition(arr, lo, hi, m);
    quicksort_hoare(arr, lo, p, m);
    quicksort_hoare(arr, p + 1, hi, m);

    m->current_depth--;
}

/* ── Quicksort with random pivot (Lomuto) ────────────────── */

void quicksort_random(int *arr, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    int rand_idx = lo + rand() % (hi - lo + 1);
    swap_int(&arr[rand_idx], &arr[hi]);

    int p = lomuto_partition(arr, lo, hi, m);
    quicksort_random(arr, lo, p - 1, m);
    quicksort_random(arr, p + 1, hi, m);

    m->current_depth--;
}

/* ── Quicksort with median-of-three (Hoare) ──────────────── */

void quicksort_median3(int *arr, int lo, int hi, Metrics *m) {
    if (lo >= hi) return;
    m->current_depth++;
    if (m->current_depth > m->max_depth) m->max_depth = m->current_depth;

    /* Median of three: lo, mid, hi */
    int mid = lo + (hi - lo) / 2;
    if (arr[lo] > arr[mid]) swap_int(&arr[lo], &arr[mid]);
    if (arr[lo] > arr[hi])  swap_int(&arr[lo], &arr[hi]);
    if (arr[mid] > arr[hi]) swap_int(&arr[mid], &arr[hi]);
    /* Now arr[lo] <= arr[mid] <= arr[hi] */
    /* Use arr[mid] as pivot via Hoare */
    int pivot = arr[mid];

    int i = lo - 1;
    int j = hi + 1;
    for (;;) {
        do { i++; m->comparisons++; } while (arr[i] < pivot);
        do { j--; m->comparisons++; } while (arr[j] > pivot);
        if (i >= j) break;
        swap_int(&arr[i], &arr[j]);
        m->swaps++;
    }

    quicksort_median3(arr, lo, j, m);
    quicksort_median3(arr, j + 1, hi, m);

    m->current_depth--;
}

/* ── Wrappers ────────────────────────────────────────────── */

typedef void (*QSortFn)(int *, int, int, Metrics *);

typedef struct {
    const char *name;
    QSortFn fn;
} QSortEntry;

void run_sort(const char *name, QSortFn fn, int *original, int n) {
    int *working = malloc(n * sizeof(int));
    memcpy(working, original, n * sizeof(int));
    Metrics m = {0, 0, 0, 0};

    clock_t start = clock();
    fn(working, 0, n - 1, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    /* Verify */
    int ok = 1;
    for (int i = 1; i < n; i++) {
        if (working[i - 1] > working[i]) { ok = 0; break; }
    }

    printf("  %-22s %10ld cmp  %8ld swp  depth=%3ld  %8.0f us  %s\n",
           name, m.comparisons, m.swaps, m.max_depth, us,
           ok ? "OK" : "FAIL");

    free(working);
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
void fill_few_unique(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = rand() % 10;
}

/* ── Demo 1: Lomuto trace ────────────────────────────────── */

void demo_lomuto_trace(void) {
    printf("=== Demo 1: Lomuto partition trace ===\n\n");

    int arr[] = {3, 7, 8, 5, 2, 1, 9, 5, 4};
    int n = 9;

    printf("Input:   ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\nPivot = arr[%d] = %d\n\n", n - 1, arr[n - 1]);

    int pivot = arr[n - 1];
    int idx = -1;

    for (int j = 0; j < n - 1; j++) {
        if (arr[j] <= pivot) {
            idx++;
            swap_int(&arr[idx], &arr[j]);
            printf("j=%d: arr[%d]=%d <= %d → i=%d, swap → [",
                   j, j, arr[idx], pivot, idx);
        } else {
            printf("j=%d: arr[%d]=%d >  %d → skip        → [",
                   j, j, arr[j], pivot);
        }
        for (int k = 0; k < n; k++) printf("%d%s", arr[k], k < n - 1 ? "," : "");
        printf("]\n");
    }

    swap_int(&arr[idx + 1], &arr[n - 1]);
    printf("\nPlace pivot: swap(arr[%d], arr[%d])\n", idx + 1, n - 1);
    printf("Result:  ");
    for (int i = 0; i < n; i++) {
        if (i == idx + 1) printf("[%d] ", arr[i]);
        else printf("%d ", arr[i]);
    }
    printf("\n         pivot at index %d\n\n", idx + 1);
}

/* ── Demo 2: Hoare trace ─────────────────────────────────── */

void demo_hoare_trace(void) {
    printf("=== Demo 2: Hoare partition trace ===\n\n");

    int arr[] = {3, 7, 8, 5, 2, 1, 9, 5, 4};
    int n = 9;

    printf("Input:   ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    int pivot = arr[n / 2];
    printf("\nPivot = arr[%d] = %d\n\n", n / 2, pivot);

    int i = -1, j = n;
    int iter = 0;
    for (;;) {
        do { i++; } while (arr[i] < pivot);
        do { j--; } while (arr[j] > pivot);
        iter++;
        if (i >= j) {
            printf("Iter %d: i=%d, j=%d → i >= j → partition point = %d\n", iter, i, j, j);
            break;
        }
        printf("Iter %d: i=%d (arr[%d]=%d), j=%d (arr[%d]=%d) → swap → ",
               iter, i, i, arr[i], j, j, arr[j]);
        swap_int(&arr[i], &arr[j]);
        printf("[");
        for (int k = 0; k < n; k++) printf("%d%s", arr[k], k < n - 1 ? "," : "");
        printf("]\n");
    }

    printf("\nResult:  ");
    for (int k = 0; k < n; k++) {
        if (k == j + 1) printf("| ");
        printf("%d ", arr[k]);
    }
    printf("\n\n");
}

/* ── Demo 3: Pivot strategies comparison ─────────────────── */

void demo_pivot_strategies(int n) {
    printf("=== Demo 3: Pivot strategies, n = %d ===\n\n", n);

    typedef struct {
        const char *name;
        void (*fill)(int *, int);
    } InputType;

    InputType inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Reversed     ", fill_reversed},
        {"Nearly sorted", fill_nearly_sorted},
        {"All equal    ", fill_all_equal},
        {"Few unique   ", fill_few_unique},
    };
    int num_inputs = 6;

    QSortEntry sorts[] = {
        {"Lomuto (last)",      quicksort_lomuto},
        {"Hoare (middle)",     quicksort_hoare},
        {"Random pivot",       quicksort_random},
        {"Median of three",    quicksort_median3},
    };
    int num_sorts = 4;

    for (int t = 0; t < num_inputs; t++) {
        int *original = malloc(n * sizeof(int));
        inputs[t].fill(original, n);
        printf("--- %s ---\n", inputs[t].name);

        for (int s = 0; s < num_sorts; s++) {
            /* Skip Lomuto on sorted/reversed/equal for large n (stack overflow) */
            if (n > 5000 && s == 0 &&
                (inputs[t].fill == fill_sorted ||
                 inputs[t].fill == fill_reversed ||
                 inputs[t].fill == fill_all_equal)) {
                printf("  %-22s (skipped: O(n^2) and stack overflow risk)\n",
                       sorts[s].name);
                continue;
            }
            run_sort(sorts[s].name, sorts[s].fn, original, n);
        }
        printf("\n");

        free(original);
    }
}

/* ── Demo 4: Recursion depth analysis ────────────────────── */

void demo_depth(void) {
    printf("=== Demo 4: Recursion depth for different inputs ===\n\n");
    printf("%8s  %-22s %6s %10s\n", "n", "Input", "Depth", "log2(n)");
    printf("%8s  %-22s %6s %10s\n", "────────", "──────────────────────",
           "──────", "──────────");

    int sizes[] = {100, 1000, 10000};
    const char *input_names[] = {"Random", "Sorted"};

    for (int si = 0; si < 3; si++) {
        int n = sizes[si];
        int *arr = malloc(n * sizeof(int));

        for (int t = 0; t < 2; t++) {
            if (t == 0) fill_random(arr, n);
            else fill_sorted(arr, n);

            int *working = malloc(n * sizeof(int));
            memcpy(working, arr, n * sizeof(int));
            Metrics m = {0, 0, 0, 0};

            /* Use median-of-three for sorted to avoid stack overflow */
            if (t == 0)
                quicksort_lomuto(working, 0, n - 1, &m);
            else
                quicksort_median3(working, 0, n - 1, &m);

            int log2n = 0;
            { int x = n; while (x > 1) { x /= 2; log2n++; } }
            printf("%8d  %-22s %6ld %10d\n", n,
                   t == 0 ? "Random (Lomuto)" : "Sorted (median3)",
                   m.max_depth, log2n);

            free(working);
        }
        free(arr);
    }
    printf("\n");
}

/* ── Demo 5: Lomuto vs Hoare swap count ──────────────────── */

void demo_swap_count(void) {
    printf("=== Demo 5: Lomuto vs Hoare — swap counts ===\n\n");
    printf("%8s %12s %12s %8s\n", "n", "Lomuto", "Hoare", "Ratio");
    printf("%8s %12s %12s %8s\n",
           "────────", "────────────", "────────────", "────────");

    int sizes[] = {100, 500, 1000, 5000, 10000};
    for (int si = 0; si < 5; si++) {
        int n = sizes[si];
        int *original = malloc(n * sizeof(int));
        fill_random(original, n);

        int *w1 = malloc(n * sizeof(int));
        int *w2 = malloc(n * sizeof(int));
        memcpy(w1, original, n * sizeof(int));
        memcpy(w2, original, n * sizeof(int));

        Metrics m1 = {0, 0, 0, 0};
        Metrics m2 = {0, 0, 0, 0};

        quicksort_lomuto(w1, 0, n - 1, &m1);
        quicksort_hoare(w2, 0, n - 1, &m2);

        printf("%8d %12ld %12ld %7.2fx\n",
               n, m1.swaps, m2.swaps,
               m2.swaps > 0 ? (double)m1.swaps / m2.swaps : 0.0);

        free(original);
        free(w1);
        free(w2);
    }
    printf("\n");
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║        Quicksort — Complete Analysis             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    demo_lomuto_trace();
    demo_hoare_trace();
    demo_swap_count();
    demo_depth();
    demo_pivot_strategies(10000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o quicksort quicksort.c
./quicksort
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

/* ── Lomuto partition ────────────────────────────────────── */

fn lomuto_partition(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) -> usize {
    let pivot = arr[hi];
    let mut i = lo;
    for j in lo..hi {
        m.comparisons += 1;
        if arr[j] <= pivot {
            arr.swap(i, j);
            m.swaps += 1;
            i += 1;
        }
    }
    arr.swap(i, hi);
    m.swaps += 1;
    i
}

/* ── Hoare partition ─────────────────────────────────────── */

fn hoare_partition(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) -> usize {
    let pivot = arr[lo + (hi - lo) / 2];
    let mut i = lo;
    let mut j = hi;

    loop {
        while { m.comparisons += 1; arr[i] < pivot } { i += 1; }
        while { m.comparisons += 1; arr[j] > pivot } { j -= 1; }
        if i >= j {
            return j;
        }
        arr.swap(i, j);
        m.swaps += 1;
        i += 1;
        j -= 1;
    }
}

/* ── Quicksort variants ─────────────────────────────────── */

fn quicksort_lomuto(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    m.enter();
    let p = lomuto_partition(arr, lo, hi, m);
    if p > 0 {
        quicksort_lomuto(arr, lo, p - 1, m);
    }
    quicksort_lomuto(arr, p + 1, hi, m);
    m.leave();
}

fn quicksort_hoare(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    m.enter();
    let p = hoare_partition(arr, lo, hi, m);
    quicksort_hoare(arr, lo, p, m);
    quicksort_hoare(arr, p + 1, hi, m);
    m.leave();
}

fn quicksort_random(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    m.enter();

    /* Simple deterministic "random" based on positions */
    let rand_idx = lo + ((hi.wrapping_mul(2654435761)) % (hi - lo + 1));
    arr.swap(rand_idx.min(hi), hi);

    let p = lomuto_partition(arr, lo, hi, m);
    if p > 0 {
        quicksort_random(arr, lo, p - 1, m);
    }
    quicksort_random(arr, p + 1, hi, m);
    m.leave();
}

fn quicksort_median3(arr: &mut [i32], lo: usize, hi: usize, m: &mut Metrics) {
    if lo >= hi {
        return;
    }
    m.enter();

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
        if i >= j { break; }
        arr.swap(i, j);
        m.swaps += 1;
        i += 1;
        j -= 1;
    }

    quicksort_median3(arr, lo, j, m);
    quicksort_median3(arr, j + 1, hi, m);
    m.leave();
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n)
        .map(|i| {
            let mut h = DefaultHasher::new();
            (i as u64 ^ 0xDEAD_BEEF).hash(&mut h);
            (h.finish() % (n as u64 * 10)) as i32
        })
        .collect()
}

fn fill_sorted(n: usize) -> Vec<i32> {
    (0..n as i32).collect()
}

fn fill_reversed(n: usize) -> Vec<i32> {
    (0..n as i32).rev().collect()
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

fn fill_all_equal(n: usize) -> Vec<i32> {
    vec![42; n]
}

fn fill_few_unique(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n)
        .map(|i| {
            let mut h = DefaultHasher::new();
            i.hash(&mut h);
            (h.finish() % 10) as i32
        })
        .collect()
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
        "  {:<22} {:>10} cmp  {:>8} swp  depth={:>3}  {:>8} us  {}",
        name, m.comparisons, m.swaps, m.max_depth, elapsed, ok
    );
}

/* ── Demo 1: Lomuto trace ────────────────────────────────── */

fn demo_lomuto_trace() {
    println!("=== Demo 1: Lomuto partition trace ===\n");

    let mut arr = vec![3, 7, 8, 5, 2, 1, 9, 5, 4];
    let n = arr.len();
    let pivot = arr[n - 1];

    println!("Input:   {:?}", arr);
    println!("Pivot = arr[{}] = {}\n", n - 1, pivot);

    let mut i: usize = 0;
    for j in 0..n - 1 {
        if arr[j] <= pivot {
            arr.swap(i, j);
            println!("j={}: arr[{}]={} <= {} → i={}, swap → {:?}", j, j, arr[i], pivot, i, arr);
            i += 1;
        } else {
            println!("j={}: arr[{}]={} >  {} → skip        → {:?}", j, j, arr[j], pivot, arr);
        }
    }
    arr.swap(i, n - 1);
    println!("\nPlace pivot: swap(arr[{}], arr[{}])", i, n - 1);
    println!("Result:  {:?}", arr);
    println!("         pivot at index {}\n", i);
}

/* ── Demo 2: Swap count comparison ───────────────────────── */

fn demo_swap_count() {
    println!("=== Demo 2: Lomuto vs Hoare — swap counts ===\n");
    println!("{:>8} {:>12} {:>12} {:>8}", "n", "Lomuto", "Hoare", "Ratio");
    println!(
        "{:>8} {:>12} {:>12} {:>8}",
        "────────", "────────────", "────────────", "────────"
    );

    for &n in &[100, 500, 1000, 5000, 10000] {
        let original = fill_random(n);

        let mut w1 = original.clone();
        let mut m1 = Metrics::default();
        quicksort_lomuto(&mut w1, 0, n - 1, &mut m1);

        let mut w2 = original.clone();
        let mut m2 = Metrics::default();
        quicksort_hoare(&mut w2, 0, n - 1, &mut m2);

        let ratio = if m2.swaps > 0 {
            m1.swaps as f64 / m2.swaps as f64
        } else {
            0.0
        };
        println!(
            "{:>8} {:>12} {:>12} {:>7.2}x",
            n, m1.swaps, m2.swaps, ratio
        );
    }
    println!();
}

/* ── Demo 3: Pivot strategies ────────────────────────────── */

fn demo_pivot_strategies(n: usize) {
    println!("=== Demo 3: Pivot strategies, n = {} ===\n", n);

    let input_names = [
        "Random", "Sorted", "Reversed", "Nearly sorted",
        "All equal", "Few unique",
    ];
    let generators: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_sorted, fill_reversed, fill_nearly_sorted,
        fill_all_equal, fill_few_unique,
    ];

    let sort_names = ["Lomuto (last)", "Hoare (middle)", "Random pivot", "Median of three"];
    let sort_fns: Vec<fn(&mut [i32], usize, usize, &mut Metrics)> = vec![
        quicksort_lomuto, quicksort_hoare, quicksort_random, quicksort_median3,
    ];

    for (ti, gen) in generators.iter().enumerate() {
        let original = gen(n);
        println!("--- {} ---", input_names[ti]);

        for (si, &sort_fn) in sort_fns.iter().enumerate() {
            /* Skip Lomuto on pathological inputs for large n */
            if n > 5000 && si == 0
                && (ti == 1 || ti == 2 || ti == 4)
            {
                println!("  {:<22} (skipped: O(n^2) risk)", sort_names[si]);
                continue;
            }
            bench(sort_names[si], &original, sort_fn);
        }
        println!();
    }
}

/* ── Demo 4: Depth analysis ──────────────────────────────── */

fn demo_depth() {
    println!("=== Demo 4: Recursion depth ===\n");
    println!(
        "{:>8}  {:<22} {:>6} {:>10}",
        "n", "Input+Strategy", "Depth", "log2(n)"
    );
    println!(
        "{:>8}  {:<22} {:>6} {:>10}",
        "────────", "──────────────────────", "──────", "──────────"
    );

    for &n in &[100, 1000, 10000] {
        /* Random input, Lomuto */
        let original = fill_random(n);
        let mut w = original.clone();
        let mut m = Metrics::default();
        quicksort_lomuto(&mut w, 0, n - 1, &mut m);
        let log2n = (n as f64).log2() as u64;
        println!(
            "{:>8}  {:<22} {:>6} {:>10}",
            n, "Random (Lomuto)", m.max_depth, log2n
        );

        /* Sorted input, median3 */
        let original = fill_sorted(n);
        let mut w = original.clone();
        let mut m = Metrics::default();
        quicksort_median3(&mut w, 0, n - 1, &mut m);
        println!(
            "{:>8}  {:<22} {:>6} {:>10}",
            n, "Sorted (median3)", m.max_depth, log2n
        );
    }
    println!();
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════╗");
    println!("║        Quicksort — Complete Analysis             ║");
    println!("╚══════════════════════════════════════════════════╝\n");

    demo_lomuto_trace();
    demo_swap_count();
    demo_depth();
    demo_pivot_strategies(10_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o quicksort_rs quicksort.rs
./quicksort_rs
```

---

## Quicksort no es estable

Quicksort no preserva el orden relativo de elementos iguales. La
partición mueve elementos a larga distancia con swaps, rompiendo el
orden original.

### Demostración

```
Input:  [(5,'a'), (3,'b'), (5,'c'), (1,'d')]
Pivot = (1,'d') [Lomuto, último]

j=0: (5,'a') > (1,'d')  → skip
j=1: (3,'b') > (1,'d')  → skip
j=2: (5,'c') > (1,'d')  → skip
Place pivot: swap(arr[0], arr[3])

Result: [(1,'d'), (3,'b'), (5,'c'), (5,'a')]
                                ↑ 'c' antes de 'a' — orden original era 'a','c'
                                ¡INESTABLE!
```

Un solo swap en la partición puede intercambiar el orden de dos
elementos iguales que estaban separados. No hay forma práctica de hacer
quicksort estable sin $O(n)$ espacio extra (que anula su ventaja
principal sobre merge sort).

---

## Ejercicios

### Ejercicio 1 — Lomuto vs Hoare: conteo de swaps

Para $n = 10000$ con datos aleatorios, cuenta los swaps de Lomuto
y Hoare. ¿Cuál es la razón?

<details><summary>Predicción</summary>

Lomuto hace ~$n/3$ swaps por nivel de partición porque cada elemento
con probabilidad $1/2$ de ser $\leq$ pivot se swapea (pero muchos swaps
son redundantes: swap consigo mismo cuando `i == j`).

Hoare hace ~$n/6$ swaps por nivel porque solo swapea cuando dos
elementos están en el lado incorrecto simultáneamente.

Razón esperada: Lomuto/Hoare $\approx 2\text{-}3\times$. Con
$\log_2(10000) \approx 13$ niveles, Lomuto hará ~40000-50000 swaps
totales y Hoare ~15000-25000.

</details>

### Ejercicio 2 — Peor caso empírico

Crea un array ordenado de $n = 1000$ y ordénalo con quicksort Lomuto
(pivote = último). Cuenta comparaciones y verifica que son $\approx
n^2/2 = 500000$.

<details><summary>Predicción</summary>

Con array ordenado y pivote = último, cada partición produce una
partición de tamaño 0 (derecha vacía) y $n - 1$ (izquierda completa).
La profundidad de recursión es $n - 1 = 999$.

Comparaciones: $\sum_{k=1}^{999} k = 999 \times 1000/2 = 499500$.
Swaps: en cada nivel, todos los elementos son $\leq$ pivote, así que
hay $n - 1$ swaps en el primer nivel, $n - 2$ en el segundo, etc.
Total swaps: también ~499500.

**Advertencia**: con $n = 1000$, la profundidad de recursión de 999
está cerca del límite de stack en muchos sistemas (~8 KB por frame ×
1000 = ~8 MB). Para $n > 5000$, probablemente cause stack overflow.

</details>

### Ejercicio 3 — Mediana de tres elimina el peor caso simple

Repite el ejercicio 2 pero con mediana de tres. Verifica que las
comparaciones bajan a $\approx 1.39 \cdot n \log_2 n$ y la profundidad
a $\approx \log_2 n$.

<details><summary>Predicción</summary>

Para datos ordenados con mediana de tres, el pivote siempre es la
mediana real del subarray (porque los tres candidatos — primero, medio,
último — están en orden y la mediana es el elemento medio).

Esto produce particiones perfectamente balanceadas: profundidad
$\log_2(1000) \approx 10$, comparaciones $\approx 1000 \times 10 =
10000$. Mucho mejor que los 499500 de Lomuto.

Pero cuidado: mediana de tres no elimina **todos** los peores casos.
Existen secuencias adversariales construidas específicamente para
mediana de tres (median-of-three killer).

</details>

### Ejercicio 4 — Todos los elementos iguales

Ordena un array de $n = 1000$ donde todos los elementos son 42. Prueba
con Lomuto y con Hoare. ¿Cuál se degrada?

<details><summary>Predicción</summary>

**Lomuto**: todos los elementos son $\leq$ pivote, así que `i` avanza
en cada iteración. La partición produce particiones de tamaño $(n-1, 0)$
— caso degenerado, $O(n^2)$.

**Hoare**: `i` avanza hasta encontrar un elemento $\geq$ pivote (el
primero), `j` retrocede hasta encontrar uno $\leq$ pivote (el último).
Se encuentran en el medio: partición balanceada $(n/2, n/2)$. Hoare
maneja todos-iguales correctamente por defecto.

Lomuto: ~499500 comparaciones, profundidad ~999.
Hoare: ~10000 comparaciones, profundidad ~10.

Este es un caso donde Lomuto falla completamente y Hoare funciona bien.
La solución general para Lomuto es la partición de tres vías (Dutch
National Flag), que se verá en T03.

</details>

### Ejercicio 5 — Profundidad de recursión vs log n

Mide la profundidad máxima de recursión para quicksort con mediana de
tres y datos aleatorios, con $n = 100, 1000, 10000, 100000$. Grafica
(o tabula) profundidad vs $\log_2 n$.

<details><summary>Predicción</summary>

La profundidad esperada con buenas particiones es $\sim c \cdot \log_2 n$
donde $c \approx 2$ (porque las particiones no son perfectas, solo
buenas en promedio).

| $n$ | $\log_2 n$ | Profundidad esperada |
|-----|-----------|---------------------|
| 100 | 6.6 | ~12-15 |
| 1000 | 10.0 | ~18-25 |
| 10000 | 13.3 | ~25-35 |
| 100000 | 16.6 | ~32-45 |

La relación es lineal en $\log n$, confirmando que el caso promedio
tiene $O(\log n)$ profundidad.

</details>

### Ejercicio 6 — Quicksort vs merge sort empírico

Compara quicksort (mediana de tres, Hoare) vs merge sort (híbrido
T=16) para $n = 100000$ con datos aleatorios. Mide tiempo, comparaciones
y swaps/moves.

<details><summary>Predicción</summary>

Quicksort hará más comparaciones (~$1.39 \cdot n \log_2 n \approx
2.3M$) que merge sort (~$n \log_2 n - 1.26n \approx 1.5M$).

Pero quicksort será **más rápido en tiempo** (~30-40% más rápido)
porque:
- No asigna memoria auxiliar.
- Mejor localidad de cache (partición secuencial).
- Menos movimientos de datos totales.

Quicksort: ~60-80 ms. Merge sort: ~90-120 ms.
Ratio de comparaciones: merge sort gana ~1.5x.
Ratio de tiempo: quicksort gana ~1.3-1.5x.

</details>

### Ejercicio 7 — Estabilidad rota

Crea un array de 20 pares `(value, original_index)` con valores
repetidos. Ordena con quicksort y verifica que los `original_index`
NO están en orden dentro de cada grupo.

<details><summary>Predicción</summary>

Con pocas repeticiones, quicksort reorganizará los elementos iguales
de forma impredecible. Por ejemplo, con valores {1, 2, 3} repetidos:

```
Input:  (2,0)(1,1)(3,2)(2,3)(1,4)(2,5)(3,6)(1,7)
Output: (1,7)(1,4)(1,1)(2,5)(2,0)(2,3)(3,6)(3,2)  ← ejemplo
```

Dentro del grupo de 2's, los índices originales podrían ser [5,0,3]
en lugar de [0,3,5]. El swap durante la partición mueve elementos a
larga distancia sin considerar el orden original.

Para verificar: dentro de cada grupo de valores iguales, comprobar si
los índices originales están en orden creciente. Deberían NO estarlo
en al menos un grupo.

</details>

### Ejercicio 8 — Pivote aleatorio: distribución de profundidades

Ejecuta quicksort con pivote aleatorio 100 veces sobre el mismo array
de $n = 10000$ (aleatorio). Registra la profundidad máxima en cada
ejecución. ¿Cuál es la media y desviación estándar?

<details><summary>Predicción</summary>

Con pivote aleatorio, la profundidad máxima sigue una distribución
concentrada alrededor de $c \cdot \log_2 n \approx 2 \times 13.3 \approx 27$.

Media esperada: ~25-30.
Desviación estándar: ~3-5.
Rango: ~20-40 (muy raro ver > 50).

La probabilidad de profundidad $> 4 \log_2 n \approx 53$ es
exponencialmente pequeña. Esto es lo que introsort explota: si la
profundidad excede $2 \lfloor \log_2 n \rfloor$, algo está mal y
cambia a heapsort.

</details>

### Ejercicio 9 — Partición como operación independiente

Usa una sola llamada a `partition` (sin recursión) para encontrar el
$k$-ésimo elemento más pequeño (quickselect). Implementa quickselect
para encontrar la mediana de un array de $n = 10001$ elementos.

<details><summary>Predicción</summary>

Quickselect usa la misma partición que quicksort, pero solo recurre en
**un** lado (el que contiene la posición $k$). Su complejidad esperada
es $O(n)$:

$$T(n) = T(n/2) + cn = O(n)$$

Para encontrar la mediana ($k = 5000$), quickselect hará en promedio
$2n = 20002$ comparaciones. Quicksort haría $1.39 \cdot n \log_2 n
\approx 185000$ comparaciones para ordenar todo.

Quickselect es ~10x más eficiente que ordenar cuando solo se necesita
un elemento. Es la base de `std::nth_element` en C++.

</details>

### Ejercicio 10 — Quicksort tail-call optimizado

Implementa quicksort con optimización tail-call: siempre recurre
primero en la partición más pequeña y usa un loop para la más grande.
Verifica que la profundidad de stack nunca excede $\log_2 n$ incluso
con datos ordenados (usando mediana de tres).

<details><summary>Predicción</summary>

Sin tail-call optimization, datos adversariales producen profundidad
$O(n)$. Con tail-call optimization:

```c
while (lo < hi) {
    int p = partition(arr, lo, hi);
    if (p - lo < hi - p) {
        quicksort(arr, lo, p - 1);  /* recurse on smaller */
        lo = p + 1;                 /* loop on larger */
    } else {
        quicksort(arr, p + 1, hi);  /* recurse on smaller */
        hi = p - 1;                 /* loop on larger */
    }
}
```

La partición más pequeña tiene a lo sumo $n/2$ elementos, así que
la profundidad de recursión es $\leq \log_2 n$.

Para $n = 10000$: profundidad máxima $\leq 14$, vs potencialmente
$10000$ sin la optimización. El número de comparaciones no cambia,
solo el uso de stack.

</details>
