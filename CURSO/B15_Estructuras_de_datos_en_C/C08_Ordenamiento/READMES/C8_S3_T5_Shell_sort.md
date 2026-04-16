# Shell sort

## Objetivo

Dominar Shell sort como la generalización de insertion sort que rompe
la barrera cuadrática sin usar recursión ni memoria auxiliar:

- Insertion sort con **gaps decrecientes**: ordenar elementos separados
  por distancia $h$, reducir $h$ hasta 1.
- La secuencia de gaps determina la complejidad: desde $O(n^2)$ hasta
  $O(n^{4/3})$ o mejor.
- Secuencias clásicas: Shell, Knuth, Sedgewick, Ciura.
- In-place, no estable, no recursivo, sin `malloc`.
- Nicho práctico: sistemas embebidos y contextos donde la simplicidad
  de código importa más que la velocidad óptima.

---

## La idea

### El problema de insertion sort

Insertion sort mueve cada elemento **una posición a la vez**. Si el
elemento más pequeño está al final del array, necesita $n - 1$ shifts
para llegar al inicio. Esto produce el comportamiento $O(n^2)$.

### La solución de Shell

Donald Shell (1959) propuso: en lugar de comparar elementos adyacentes
(gap = 1), comparar elementos separados por un gap $h > 1$. Esto mueve
elementos **grandes distancias** rápidamente, eliminando muchas
inversiones de un solo paso.

Después de ordenar con gap $h$, el array está **$h$-sorted**: para todo
$i$, se cumple `arr[i] <= arr[i + h]`. Un array que es $h$-sorted y
luego se ordena con gap $k$ sigue siendo $h$-sorted (propiedad crucial).

Al llegar a gap = 1, se ejecuta un insertion sort estándar, pero sobre
un array que ya está "casi ordenado" por las pasadas anteriores,
resultando en $O(n)$ o cercano.

### Pseudocódigo

```
SHELL_SORT(arr, n, gaps[]):
    para cada gap h en gaps[] (de mayor a menor):
        // h-insertion sort: insertion sort con paso h
        para i de h a n-1:
            key = arr[i]
            j = i
            mientras j >= h y arr[j - h] > key:
                arr[j] = arr[j - h]
                j = j - h
            arr[j] = key
```

La pasada con gap = 1 es exactamente insertion sort. Las pasadas
anteriores preparan el array para que esta última pasada sea rápida.

---

## Traza detallada

Array: `[8, 3, 6, 1, 5, 9, 2, 7, 4]` ($n = 9$).

Usamos la secuencia de Shell: $\lfloor n/2 \rfloor, \lfloor n/4
\rfloor, \ldots, 1$ → gaps = [4, 2, 1].

### Pasada 1: gap = 4

Comparar elementos separados por 4 posiciones. Se forman 4 sub-secuencias
intercaladas:

```
Posiciones 0,4,8: valores [8, 5, 4]  → ordenar → [4, 5, 8]
Posiciones 1,5:   valores [3, 9]     → ordenar → [3, 9]
Posiciones 2,6:   valores [6, 2]     → ordenar → [2, 6]
Posiciones 3,7:   valores [1, 7]     → ordenar → [1, 7]

Resultado: [4, 3, 2, 1, 5, 9, 6, 7, 8]
            ↑        ↑        ↑
            0        4        8  (sub-secuencia 0: ahora ordenada)
```

### Pasada 2: gap = 2

Comparar elementos separados por 2 posiciones. Se forman 2 sub-secuencias:

```
Posiciones 0,2,4,6,8: valores [4, 2, 5, 6, 8] → ordenar → [2, 4, 5, 6, 8]
Posiciones 1,3,5,7:   valores [3, 1, 9, 7]     → ordenar → [1, 3, 7, 9]

Resultado: [2, 1, 4, 3, 5, 7, 6, 9, 8]
```

### Pasada 3: gap = 1 (insertion sort final)

```
Paso  key  Shifts  Array
────  ───  ──────  ──────────────────────
i=1:  1    1       [1, 2, 4, 3, 5, 7, 6, 9, 8]
i=2:  4    0       [1, 2, 4, 3, 5, 7, 6, 9, 8]
i=3:  3    1       [1, 2, 3, 4, 5, 7, 6, 9, 8]
i=4:  5    0       [1, 2, 3, 4, 5, 7, 6, 9, 8]
i=5:  7    0       [1, 2, 3, 4, 5, 7, 6, 9, 8]
i=6:  6    1       [1, 2, 3, 4, 5, 6, 7, 9, 8]
i=7:  9    0       [1, 2, 3, 4, 5, 6, 7, 9, 8]
i=8:  8    1       [1, 2, 3, 4, 5, 6, 7, 8, 9]

Total shifts en gap=1: solo 4 (vs ~20 que haría insertion sort puro)
```

Las pasadas con gaps 4 y 2 eliminaron la mayoría de las inversiones.
La pasada final con gap 1 solo necesita ajustes menores.

---

## Secuencias de gaps

La elección de la secuencia de gaps es lo más importante de Shell sort.
Una mala secuencia da $O(n^2)$; una buena da $O(n^{4/3})$ o mejor.

### Secuencia de Shell (1959)

$$h_k = \lfloor n / 2^k \rfloor$$

Secuencia: $n/2, n/4, n/8, \ldots, 1$.

- **Problema**: los gaps son todos pares excepto el último. Esto
  significa que elementos en posiciones pares nunca se comparan con
  elementos en posiciones impares hasta la última pasada (gap = 1).
  El array puede tener muchas inversiones al llegar a gap = 1.
- **Complejidad**: $\Theta(n^2)$ en el peor caso.

### Secuencia de Knuth (1973)

$$h_1 = 1, \quad h_{k+1} = 3h_k + 1$$

Secuencia: 1, 4, 13, 40, 121, 364, 1093, ...

Se usa el mayor $h$ tal que $h < n/3$.

- **Ventaja**: los gaps no son múltiplos entre sí, lo que evita el
  problema de Shell.
- **Complejidad**: $O(n^{3/2})$.
- **Práctica**: fácil de calcular, buen rendimiento general.

### Secuencia de Sedgewick (1986)

$$h_k = \begin{cases} 9 \cdot (2^k - 2^{k/2}) + 1 & \text{si } k \text{ par} \\ 8 \cdot 2^k - 6 \cdot 2^{(k+1)/2} + 1 & \text{si } k \text{ impar} \end{cases}$$

Secuencia: 1, 5, 19, 41, 109, 209, 505, 929, 2161, ...

- **Complejidad**: $O(n^{4/3})$.
- **Práctica**: mejor que Knuth para $n$ grande.

### Secuencia de Tokuda (1992)

$$h_1 = 1, \quad h_{k+1} = \lceil (9h_k + 4) / 4 \rceil \quad \text{equivalente a} \quad h_k = \lceil (9^k - 4^k) / (5 \cdot 4^{k-1}) \rceil$$

Secuencia: 1, 4, 9, 20, 46, 103, 233, 525, 1182, ...

### Secuencia de Ciura (2001)

Determinada empíricamente:

$$1, 4, 10, 23, 57, 132, 301, 701, 1750, \ldots$$

(Después de 701, se extiende multiplicando por ~2.25.)

- **La mejor secuencia conocida empíricamente** para la mayoría de
  tamaños de entrada.
- No tiene fórmula cerrada — es resultado de optimización numérica.

### Comparación

| Secuencia | Complejidad (peor caso) | Complejidad (promedio) | Gaps para $n = 1000$ |
|-----------|------------------------|----------------------|---------------------|
| Shell | $\Theta(n^2)$ | $O(n^{3/2})$ | 500, 250, 125, 62, 31, 15, 7, 3, 1 |
| Knuth | $O(n^{3/2})$ | $O(n^{5/4})$ aprox | 364, 121, 40, 13, 4, 1 |
| Sedgewick | $O(n^{4/3})$ | $O(n^{7/6})$ aprox | 505, 209, 109, 41, 19, 5, 1 |
| Ciura | $O(?)$ (no probado) | Mejor empírico | 701, 301, 132, 57, 23, 10, 4, 1 |

---

## Análisis de complejidad

### Por qué funciona: eliminación de inversiones a distancia

Una inversión es un par $(i, j)$ con $i < j$ y `arr[i] > arr[j]`.
Insertion sort elimina exactamente una inversión por shift. Con $k$
inversiones, insertion sort hace $k$ shifts.

Shell sort con gap $h$ elimina inversiones entre elementos separados
por distancia $h$. Una pasada con gap $h$ hace $O(n)$ operaciones y
puede eliminar $O(n \cdot h)$ inversiones (inversiones a distancias
mayores a $h$).

Después de las pasadas con gaps grandes, quedan solo inversiones de
distancia corta. La pasada final con gap 1 tiene pocas inversiones
que resolver.

### Análisis de la secuencia de Knuth

Con gaps $1, 4, 13, 40, \ldots$, después de la pasada con gap $h_k$,
cada elemento está a lo sumo a distancia $h_k$ de su posición final.
Cuando gap $h_{k-1} = (h_k - 1) / 3$, la pasada siguiente hace a lo
sumo $O(h_k)$ shifts por elemento.

Total: $O(n) \times$ (número de gaps) = $O(n \cdot \log_{3} n) =
O(n^{3/2})$ para Knuth. El argumento formal usa que arrays que son
simultáneamente $h$-sorted y $k$-sorted con $\gcd(h, k) = 1$ tienen
a lo sumo $O(hk)$ inversiones restantes.

### Complejidad exacta: un problema abierto

La complejidad exacta de Shell sort **no se conoce** para la mayoría
de secuencias de gaps. Es uno de los problemas abiertos más antiguos
en análisis de algoritmos. Se conjetura que ninguna secuencia da
$O(n \log n)$, pero no está probado.

Los mejores resultados conocidos:
- Pratt (1971): gaps = todos los $2^p \cdot 3^q < n$. Complejidad
  $O(n \log^2 n)$, pero con demasiados gaps (~$\log^2 n$).
- Cota inferior general: $\Omega(n \log^2 n / \log \log n)$ para
  cualquier secuencia (Cypher, 1993).

---

## Propiedades

### In-place

Shell sort solo usa $O(1)$ espacio extra (una variable `key` y un
índice `j`). No necesita recursión ni memoria auxiliar.

### No estable

Shell sort no es estable. Las pasadas con gap > 1 mueven elementos a
larga distancia, pasando por encima de elementos iguales.

```
Input: [(2,'a'), (1,'b'), (2,'c'), (1,'d')]
Gap 2: compara posiciones 0,2 y 1,3
  pos 0,2: (2,'a'),(2,'c') → no swap (==)
  pos 1,3: (1,'b'),(1,'d') → no swap (==)
  [sin cambio]
Gap 1: insertion sort
  [(1,'b'), (1,'d'), (2,'a'), (2,'c')]
  Los 1's: 'b' antes de 'd' ✓
  Los 2's: 'a' antes de 'c' ✓ (estable en este caso)
```

Pero con otra entrada:

```
Input: [(3,'a'), (1,'b'), (3,'c'), (2,'d')]
Gap 2: pos 0,2: (3,'a'),(3,'c') → no swap
       pos 1,3: (1,'b'),(2,'d') → no swap
Gap 1: insertion sort
  i=1: 1 < 3 → shift → [(1,'b'), (3,'a'), (3,'c'), (2,'d')]
  i=3: 2 < 3 → shift → [(1,'b'), (2,'d'), (3,'a'), (3,'c')]
  3's mantienen orden ✓ — pero es por suerte.
```

Con la secuencia de Shell ($n/2$):

```
Input: [(5,'a'), (3,'b'), (5,'c'), (3,'d')]
Gap 2: pos 0,2: (5,'a'),(5,'c') → no swap
       pos 1,3: (3,'b'),(3,'d') → no swap
Gap 1: [(3,'b'), (3,'d'), (5,'a'), (5,'c')] ✓ estable aquí

Input: [(5,'a'), (5,'b'), (3,'c'), (3,'d')]
Gap 2: pos 0,2: (5,'a'),(3,'c') → swap → [(3,'c'), (5,'b'), (5,'a'), (3,'d')]
       pos 1,3: (5,'b'),(3,'d') → swap → [(3,'c'), (3,'d'), (5,'a'), (5,'b')]
Gap 1: ya ordenado → [(3,'c'), (3,'d'), (5,'a'), (5,'b')]
  3's: 'c' antes de 'd' pero original era 'c' antes de 'd' ✓
  5's: 'a' antes de 'b' ✓ — estable aquí también (suerte)
```

La estabilidad se puede romper con entradas más complejas. En general,
Shell sort no garantiza estabilidad.

### No recursivo

Shell sort es completamente iterativo: tres loops anidados (gaps,
posición de inicio, shifts). No necesita stack de recursión. Esto lo
hace ideal para sistemas con stack limitado.

### Adaptativo

Como la pasada final es insertion sort, Shell sort hereda su
adaptividad parcial: si el array ya está casi ordenado, la pasada
final hace $O(n)$ y las pasadas anteriores hacen muy poco trabajo
(los sub-arrays ya están $h$-sorted).

---

## Shell sort vs insertion sort vs los eficientes

| Propiedad | Insertion sort | Shell sort | Quicksort | Merge sort |
|-----------|---------------|------------|-----------|------------|
| Peor caso | $O(n^2)$ | $O(n^{3/2})$-$O(n^{4/3})$ | $O(n^2)$ | $O(n \log n)$ |
| Promedio | $O(n^2)$ | $O(n^{5/4})$-$O(n^{7/6})$ | $O(n \log n)$ | $O(n \log n)$ |
| Espacio | $O(1)$ | $O(1)$ | $O(\log n)$ | $O(n)$ |
| Estable | Sí | No | No | Sí |
| Recursión | No | No | Sí | Sí |
| Código | ~10 líneas | ~15 líneas | ~30 líneas | ~40 líneas |
| `malloc` | No | No | No | Sí |

Shell sort ocupa un nicho intermedio: mucho mejor que insertion sort
para $n$ mediano, más simple que quicksort/merge sort, y no necesita
recursión ni `malloc`.

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
    long shifts;
    int  num_passes;
} Metrics;

/* ── Shell sort with configurable gap sequence ───────────── */

void shell_sort_gaps(int *arr, int n, const int *gaps, int num_gaps, Metrics *m) {
    m->comparisons = 0;
    m->shifts = 0;
    m->num_passes = 0;

    for (int g = 0; g < num_gaps; g++) {
        int h = gaps[g];
        if (h >= n) continue;
        m->num_passes++;

        for (int i = h; i < n; i++) {
            int key = arr[i];
            int j = i;
            while (j >= h && (m->comparisons++, arr[j - h] > key)) {
                arr[j] = arr[j - h];
                m->shifts++;
                j -= h;
            }
            arr[j] = key;
        }
    }
}

/* ── Gap sequence generators ─────────────────────────────── */

int gen_shell_gaps(int n, int *gaps) {
    int count = 0;
    for (int h = n / 2; h >= 1; h /= 2)
        gaps[count++] = h;
    return count;
}

int gen_knuth_gaps(int n, int *gaps) {
    /* Generate forward: 1, 4, 13, 40, 121, ... */
    int temp[64];
    int count = 0;
    int h = 1;
    while (h < n / 3) {
        temp[count++] = h;
        h = 3 * h + 1;
    }
    temp[count++] = h;
    /* Reverse into gaps[] */
    for (int i = 0; i < count; i++)
        gaps[i] = temp[count - 1 - i];
    return count;
}

int gen_sedgewick_gaps(int n, int *gaps) {
    int temp[64];
    int count = 0;
    for (int k = 0; ; k++) {
        long h;
        if (k % 2 == 0)
            h = 9L * ((1L << k) - (1L << (k / 2))) + 1;
        else
            h = 8L * (1L << k) - 6L * (1L << ((k + 1) / 2)) + 1;
        if (h <= 0) h = 1;
        if (h >= n) break;
        temp[count++] = (int)h;
    }
    if (count == 0 || temp[0] != 1) {
        /* Ensure 1 is included */
        for (int i = count; i > 0; i--) temp[i] = temp[i - 1];
        temp[0] = 1;
        count++;
    }
    /* Reverse */
    for (int i = 0; i < count; i++)
        gaps[i] = temp[count - 1 - i];
    return count;
}

int gen_ciura_gaps(int n, int *gaps) {
    int ciura[] = {1, 4, 10, 23, 57, 132, 301, 701};
    int base_count = 8;

    /* Extend beyond 701 with factor ~2.25 */
    int temp[64];
    int count = 0;
    for (int i = 0; i < base_count && ciura[i] < n; i++)
        temp[count++] = ciura[i];
    int h = 701;
    while (1) {
        h = (int)(h * 2.25);
        if (h >= n) break;
        temp[count++] = h;
    }
    /* Reverse */
    for (int i = 0; i < count; i++)
        gaps[i] = temp[count - 1 - i];
    return count;
}

/* ── Insertion sort for comparison ───────────────────────── */

void insertion_sort(int *arr, int n, Metrics *m) {
    m->comparisons = 0;
    m->shifts = 0;
    m->num_passes = 1;
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && (m->comparisons++, arr[j] > key)) {
            arr[j + 1] = arr[j];
            m->shifts++;
            j--;
        }
        arr[j + 1] = key;
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
        int tmp = a[x]; a[x] = a[y]; a[y] = tmp;
    }
}

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* ── Benchmark ───────────────────────────────────────────── */

typedef int (*GapGenFn)(int n, int *gaps);

typedef struct {
    const char *name;
    GapGenFn gen;
} GapSeq;

void bench_gap(const char *seq_name, int *original, int n, GapGenFn gen) {
    int *working = malloc(n * sizeof(int));
    memcpy(working, original, n * sizeof(int));

    int gaps[64];
    int num_gaps = gen(n, gaps);

    Metrics m;
    clock_t start = clock();
    shell_sort_gaps(working, n, gaps, num_gaps, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-16s %3d passes %10ld cmp %10ld shifts %8.0f us  %s\n",
           seq_name, m.num_passes, m.comparisons, m.shifts, us,
           is_sorted(working, n) ? "OK" : "FAIL");
    free(working);
}

void bench_insertion(int *original, int n) {
    int *working = malloc(n * sizeof(int));
    memcpy(working, original, n * sizeof(int));

    Metrics m;
    clock_t start = clock();
    insertion_sort(working, n, &m);
    clock_t end = clock();
    double us = (double)(end - start) / CLOCKS_PER_SEC * 1e6;

    printf("  %-16s %3d passes %10ld cmp %10ld shifts %8.0f us  %s\n",
           "Insertion sort", m.num_passes, m.comparisons, m.shifts, us,
           is_sorted(working, n) ? "OK" : "FAIL");
    free(working);
}

/* ── Demo 1: Trace ───────────────────────────────────────── */

void demo_trace(void) {
    printf("=== Demo 1: Shell sort trace (Knuth gaps) ===\n\n");

    int arr[] = {8, 3, 6, 1, 5, 9, 2, 7, 4};
    int n = 9;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n\n");

    /* Knuth gaps for n=9: 4, 1 */
    int gaps[] = {4, 1};
    int num_gaps = 2;

    for (int g = 0; g < num_gaps; g++) {
        int h = gaps[g];
        printf("--- Gap = %d ---\n", h);

        for (int i = h; i < n; i++) {
            int key = arr[i];
            int j = i;
            int moved = 0;
            while (j >= h && arr[j - h] > key) {
                arr[j] = arr[j - h];
                j -= h;
                moved = 1;
            }
            arr[j] = key;

            if (moved || g == num_gaps - 1) {
                printf("  i=%d key=%d: ", i, key);
                for (int k = 0; k < n; k++) printf("%d ", arr[k]);
                if (moved) printf("(shifted)");
                printf("\n");
            }
        }
        printf("  After gap %d: ", h);
        for (int k = 0; k < n; k++) printf("%d ", arr[k]);
        printf("\n\n");
    }
}

/* ── Demo 2: Gap sequences printed ───────────────────────── */

void demo_gaps(int n) {
    printf("=== Demo 2: Gap sequences for n = %d ===\n\n", n);

    int gaps[64];
    int count;

    GapSeq seqs[] = {
        {"Shell",    gen_shell_gaps},
        {"Knuth",    gen_knuth_gaps},
        {"Sedgewick", gen_sedgewick_gaps},
        {"Ciura",    gen_ciura_gaps},
    };

    for (int s = 0; s < 4; s++) {
        count = seqs[s].gen(n, gaps);
        printf("  %-10s (%2d gaps): ", seqs[s].name, count);
        for (int i = 0; i < count; i++) printf("%d ", gaps[i]);
        printf("\n");
    }
    printf("\n");
}

/* ── Demo 3: Sequence comparison ─────────────────────────── */

void demo_comparison(int n) {
    printf("=== Demo 3: Gap sequence comparison, n = %d ===\n\n", n);

    typedef struct { const char *name; void (*fill)(int *, int); } Input;
    Input inputs[] = {
        {"Random       ", fill_random},
        {"Sorted       ", fill_sorted},
        {"Reversed     ", fill_reversed},
        {"Nearly sorted", fill_nearly_sorted},
    };

    GapSeq seqs[] = {
        {"Shell",     gen_shell_gaps},
        {"Knuth",     gen_knuth_gaps},
        {"Sedgewick",  gen_sedgewick_gaps},
        {"Ciura",     gen_ciura_gaps},
    };

    for (int t = 0; t < 4; t++) {
        int *original = malloc(n * sizeof(int));
        inputs[t].fill(original, n);
        printf("--- %s ---\n", inputs[t].name);

        for (int s = 0; s < 4; s++)
            bench_gap(seqs[s].name, original, n, seqs[s].gen);
        if (n <= 10000)
            bench_insertion(original, n);
        printf("\n");
        free(original);
    }
}

/* ── Demo 4: Shell's sequence worst case ─────────────────── */

void demo_shell_worst_case(void) {
    printf("=== Demo 4: Shell's sequence worst case ===\n\n");

    /* Construct worst case: elements at even positions are large,
       odd positions are small. Shell gaps (n/2, n/4, ...) are all
       even, so evens and odds never interact until gap=1. */
    int n = 16;
    int arr[16];
    /* Interleave two sorted halves: [1,9,2,10,3,11,4,12,5,13,6,14,7,15,8,16] */
    for (int i = 0; i < n; i++) {
        if (i % 2 == 0) arr[i] = i / 2 + 1;
        else arr[i] = n / 2 + i / 2 + 1;
    }

    printf("Input (designed for Shell's weakness): ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n\n");

    /* Shell sequence */
    int gaps_shell[10];
    int ng_shell = gen_shell_gaps(n, gaps_shell);
    int *w1 = malloc(n * sizeof(int));
    memcpy(w1, arr, n * sizeof(int));
    Metrics m1;
    shell_sort_gaps(w1, n, gaps_shell, ng_shell, &m1);
    printf("  Shell sequence:  %ld cmp, %ld shifts\n", m1.comparisons, m1.shifts);

    /* Knuth sequence */
    int gaps_knuth[10];
    int ng_knuth = gen_knuth_gaps(n, gaps_knuth);
    int *w2 = malloc(n * sizeof(int));
    memcpy(w2, arr, n * sizeof(int));
    Metrics m2;
    shell_sort_gaps(w2, n, gaps_knuth, ng_knuth, &m2);
    printf("  Knuth sequence:  %ld cmp, %ld shifts\n", m2.comparisons, m2.shifts);

    printf("  Shell does %.1fx more work on this input.\n\n",
           m1.shifts > 0 && m2.shifts > 0 ?
           (double)m1.shifts / m2.shifts : 0.0);

    free(w1);
    free(w2);
}

/* ── Demo 5: h-sorted property ───────────────────────────── */

void demo_h_sorted(void) {
    printf("=== Demo 5: h-sorted property preservation ===\n\n");

    int arr[] = {8, 3, 6, 1, 5, 9, 2, 7, 4, 0};
    int n = 10;

    printf("Original:   ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    /* Sort with gap 4 */
    for (int i = 4; i < n; i++) {
        int key = arr[i];
        int j = i;
        while (j >= 4 && arr[j - 4] > key) {
            arr[j] = arr[j - 4]; j -= 4;
        }
        arr[j] = key;
    }
    printf("After h=4:  ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);

    /* Verify 4-sorted */
    int h4 = 1;
    for (int i = 0; i + 4 < n; i++)
        if (arr[i] > arr[i + 4]) { h4 = 0; break; }
    printf(" [4-sorted: %s]\n", h4 ? "YES" : "NO");

    /* Sort with gap 2 */
    for (int i = 2; i < n; i++) {
        int key = arr[i];
        int j = i;
        while (j >= 2 && arr[j - 2] > key) {
            arr[j] = arr[j - 2]; j -= 2;
        }
        arr[j] = key;
    }
    printf("After h=2:  ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);

    /* Verify still 4-sorted AND 2-sorted */
    h4 = 1;
    for (int i = 0; i + 4 < n; i++)
        if (arr[i] > arr[i + 4]) { h4 = 0; break; }
    int h2 = 1;
    for (int i = 0; i + 2 < n; i++)
        if (arr[i] > arr[i + 2]) { h2 = 0; break; }
    printf(" [4-sorted: %s, 2-sorted: %s]\n", h4 ? "YES" : "NO", h2 ? "YES" : "NO");

    printf("Key property: h=2 sort preserved the h=4 sort!\n\n");
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║          Shell Sort — Complete Analysis           ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    demo_trace();
    demo_gaps(1000);
    demo_gaps(100000);
    demo_h_sorted();
    demo_shell_worst_case();
    demo_comparison(1000);
    demo_comparison(10000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o shell_sort shell_sort.c -lm
./shell_sort
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── Metrics ─────────────────────────────────────────────── */

#[derive(Default, Clone)]
struct Metrics {
    comparisons: u64,
    shifts: u64,
    num_passes: u32,
}

/* ── Shell sort with configurable gaps ───────────────────── */

fn shell_sort(arr: &mut [i32], gaps: &[usize], m: &mut Metrics) {
    let n = arr.len();
    for &h in gaps {
        if h >= n { continue; }
        m.num_passes += 1;
        for i in h..n {
            let key = arr[i];
            let mut j = i;
            while j >= h {
                m.comparisons += 1;
                if arr[j - h] <= key { break; }
                arr[j] = arr[j - h];
                m.shifts += 1;
                j -= h;
            }
            arr[j] = key;
        }
    }
}

/* ── Gap generators ──────────────────────────────────────── */

fn gen_shell_gaps(n: usize) -> Vec<usize> {
    let mut gaps = Vec::new();
    let mut h = n / 2;
    while h >= 1 {
        gaps.push(h);
        h /= 2;
    }
    gaps
}

fn gen_knuth_gaps(n: usize) -> Vec<usize> {
    let mut forward = Vec::new();
    let mut h = 1usize;
    while h < n / 3 {
        forward.push(h);
        h = 3 * h + 1;
    }
    forward.push(h);
    forward.reverse();
    forward
}

fn gen_sedgewick_gaps(n: usize) -> Vec<usize> {
    let mut forward = Vec::new();
    for k in 0u32.. {
        let h: i64 = if k % 2 == 0 {
            9 * ((1i64 << k) - (1i64 << (k / 2))) + 1
        } else {
            8 * (1i64 << k) - 6 * (1i64 << ((k + 1) / 2)) + 1
        };
        if h <= 0 { continue; }
        if h as usize >= n { break; }
        forward.push(h as usize);
    }
    if forward.is_empty() || forward[0] != 1 {
        forward.insert(0, 1);
    }
    forward.reverse();
    forward
}

fn gen_ciura_gaps(n: usize) -> Vec<usize> {
    let base = [1, 4, 10, 23, 57, 132, 301, 701];
    let mut forward: Vec<usize> = base.iter().copied().filter(|&g| g < n).collect();
    let mut h = 701usize;
    loop {
        h = (h as f64 * 2.25) as usize;
        if h >= n { break; }
        forward.push(h);
    }
    forward.reverse();
    forward
}

/* ── Insertion sort for comparison ───────────────────────── */

fn insertion_sort(arr: &mut [i32], m: &mut Metrics) {
    m.num_passes = 1;
    for i in 1..arr.len() {
        let key = arr[i];
        let mut j = i;
        while j > 0 {
            m.comparisons += 1;
            if arr[j - 1] <= key { break; }
            arr[j] = arr[j - 1];
            m.shifts += 1;
            j -= 1;
        }
        arr[j] = key;
    }
}

/* ── Input generators ────────────────────────────────────── */

fn fill_random(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xBADC0FFE).hash(&mut h);
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

fn is_sorted(a: &[i32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }

/* ── Benchmark ───────────────────────────────────────────── */

fn bench(name: &str, original: &[i32], gaps: &[usize]) {
    let mut w = original.to_vec();
    let mut m = Metrics::default();
    let start = Instant::now();
    shell_sort(&mut w, gaps, &mut m);
    let elapsed = start.elapsed().as_micros();
    let ok = if is_sorted(&w) { "OK" } else { "FAIL" };
    println!("  {:<16} {:>3} passes {:>10} cmp {:>10} shifts {:>8} us  {}",
        name, m.num_passes, m.comparisons, m.shifts, elapsed, ok);
}

fn bench_insertion(original: &[i32]) {
    let mut w = original.to_vec();
    let mut m = Metrics::default();
    let start = Instant::now();
    insertion_sort(&mut w, &mut m);
    let elapsed = start.elapsed().as_micros();
    let ok = if is_sorted(&w) { "OK" } else { "FAIL" };
    println!("  {:<16} {:>3} passes {:>10} cmp {:>10} shifts {:>8} us  {}",
        "Insertion sort", m.num_passes, m.comparisons, m.shifts, elapsed, ok);
}

/* ── Demo 1: Trace ───────────────────────────────────────── */

fn demo_trace() {
    println!("=== Demo 1: Shell sort trace (Knuth gaps) ===\n");

    let original = vec![8, 3, 6, 1, 5, 9, 2, 7, 4];
    let n = original.len();
    println!("Input: {:?}\n", original);

    let mut arr = original.clone();
    let gaps_list = [4usize, 1];

    for &h in &gaps_list {
        println!("--- Gap = {} ---", h);
        for i in h..n {
            let key = arr[i];
            let mut j = i;
            let mut moved = false;
            while j >= h && arr[j - h] > key {
                arr[j] = arr[j - h];
                j -= h;
                moved = true;
            }
            arr[j] = key;
            if moved {
                println!("  i={} key={}: {:?}", i, key, arr);
            }
        }
        println!("  After gap {}: {:?}\n", h, arr);
    }
}

/* ── Demo 2: Gap sequences ───────────────────────────────── */

fn demo_gaps(n: usize) {
    println!("=== Gap sequences for n = {} ===\n", n);

    let seqs: Vec<(&str, Vec<usize>)> = vec![
        ("Shell",     gen_shell_gaps(n)),
        ("Knuth",     gen_knuth_gaps(n)),
        ("Sedgewick",  gen_sedgewick_gaps(n)),
        ("Ciura",     gen_ciura_gaps(n)),
    ];

    for (name, gaps) in &seqs {
        println!("  {:<10} ({:>2} gaps): {:?}", name, gaps.len(), gaps);
    }
    println!();
}

/* ── Demo 3: Sequence comparison ─────────────────────────── */

fn demo_comparison(n: usize) {
    println!("=== Gap sequence comparison, n = {} ===\n", n);

    let names = ["Random", "Sorted", "Reversed", "Nearly sorted"];
    let gens: Vec<fn(usize) -> Vec<i32>> = vec![
        fill_random, fill_sorted, fill_reversed, fill_nearly_sorted,
    ];

    let seq_names = ["Shell", "Knuth", "Sedgewick", "Ciura"];
    let seq_gaps: Vec<Vec<usize>> = vec![
        gen_shell_gaps(n), gen_knuth_gaps(n),
        gen_sedgewick_gaps(n), gen_ciura_gaps(n),
    ];

    for (ti, gen) in gens.iter().enumerate() {
        let original = gen(n);
        println!("--- {} ---", names[ti]);
        for (si, gaps) in seq_gaps.iter().enumerate() {
            bench(seq_names[si], &original, gaps);
        }
        if n <= 10000 {
            bench_insertion(&original);
        }
        println!();
    }
}

/* ── Demo 4: h-sorted preservation ───────────────────────── */

fn demo_h_sorted() {
    println!("=== h-sorted property preservation ===\n");

    let mut arr = vec![8, 3, 6, 1, 5, 9, 2, 7, 4, 0];
    let n = arr.len();
    println!("Original:  {:?}", arr);

    /* Sort with gap 4 */
    for i in 4..n {
        let key = arr[i];
        let mut j = i;
        while j >= 4 && arr[j - 4] > key { arr[j] = arr[j - 4]; j -= 4; }
        arr[j] = key;
    }
    let h4_ok = (0..n - 4).all(|i| arr[i] <= arr[i + 4]);
    println!("After h=4: {:?} [4-sorted: {}]", arr, if h4_ok { "YES" } else { "NO" });

    /* Sort with gap 2 */
    for i in 2..n {
        let key = arr[i];
        let mut j = i;
        while j >= 2 && arr[j - 2] > key { arr[j] = arr[j - 2]; j -= 2; }
        arr[j] = key;
    }
    let h4_still = (0..n - 4).all(|i| arr[i] <= arr[i + 4]);
    let h2_ok = (0..n - 2).all(|i| arr[i] <= arr[i + 2]);
    println!("After h=2: {:?} [4-sorted: {}, 2-sorted: {}]",
        arr, if h4_still { "YES" } else { "NO" }, if h2_ok { "YES" } else { "NO" });
    println!("Key property: h=2 sort preserved the h=4 sort!\n");
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════╗");
    println!("║          Shell Sort — Complete Analysis           ║");
    println!("╚══════════════════════════════════════════════════╝\n");

    demo_trace();
    demo_gaps(1000);
    demo_gaps(100000);
    demo_h_sorted();
    demo_comparison(1000);
    demo_comparison(10000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o shell_sort_rs shell_sort.rs
./shell_sort_rs
```

---

## Nicho práctico de Shell sort

### Cuándo elegir Shell sort

1. **Sistemas embebidos sin `malloc`**: Shell sort no necesita memoria
   dinámica ni recursión. En microcontroladores con 2-8 KB de RAM y
   sin heap, es la mejor opción después de insertion sort.

2. **Código mínimo**: ~15 líneas vs ~40+ de merge sort o quicksort.
   En contextos donde el tamaño del binario importa (bootloaders,
   firmware), Shell sort es ideal.

3. **$n$ mediano (50-5000)**: demasiado grande para insertion sort,
   pero no lo suficiente para justificar la complejidad de quicksort
   o merge sort.

4. **uClibc y BusyBox**: la implementación de `qsort` en uClibc (usada
   en muchos sistemas embebidos Linux) usa Shell sort con secuencia de
   Ciura por su simplicidad y tamaño de código mínimo.

### Cuándo NO elegir Shell sort

- $n > 10000$: quicksort o merge sort son significativamente mejores.
- Se necesita estabilidad: usar merge sort.
- Se necesita $O(n \log n)$ garantizado: usar merge sort o heapsort.
- Se necesita el máximo rendimiento: usar introsort/pdqsort/Timsort.

---

## Ejercicios

### Ejercicio 1 — Shell vs Knuth vs Sedgewick vs Ciura

Compara las cuatro secuencias de gaps para $n = 10000$ con datos
aleatorios. Cuenta comparaciones, shifts y tiempo.

<details><summary>Predicción</summary>

Para $n = 10000$ aleatorio:

| Secuencia | Comparaciones | Shifts | Pasadas |
|-----------|--------------|--------|---------|
| Shell | ~400K | ~300K | 13 |
| Knuth | ~250K | ~180K | 8 |
| Sedgewick | ~200K | ~140K | 7 |
| Ciura | ~190K | ~130K | 8 |

Ciura y Sedgewick deberían empatar o Ciura ganar ligeramente. Shell
será ~2x peor que las mejores secuencias. Knuth será intermedio.

Insertion sort para comparación: ~$10000^2/4 = 25M$ comparaciones
— 100x más que las mejores secuencias de Shell sort.

</details>

### Ejercicio 2 — Caso patológico de Shell's sequence

Construye una entrada que sea especialmente mala para la secuencia de
Shell (todos los gaps pares excepto 1). Compara con Knuth.

<details><summary>Predicción</summary>

Entrada: intercalar dos secuencias ordenadas en posiciones pares e
impares. Ejemplo para $n = 16$:
`[1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15, 8, 16]`

Con Shell (gaps 8, 4, 2, 1): los gaps 8, 4, 2 solo comparan posiciones
pares con pares e impares con impares (porque todos son pares). Las dos
sub-secuencias intercaladas ya están ordenadas internamente, así que
las pasadas con gaps > 1 no hacen nada útil. La pasada con gap = 1
tiene que hacer todo el trabajo: ~$n^2/4 = 64$ shifts.

Con Knuth (gaps 13, 4, 1): gap 4 mezcla pares e impares (4 no es
divisor de 2), eliminando las inversiones cruzadas. La pasada final
hace muy poco trabajo: ~$n$ shifts.

Shell: ~$n^2/4$ shifts. Knuth: ~$n$ shifts. Ratio: ~$n/4 = 4$ para
$n = 16$.

</details>

### Ejercicio 3 — Shell sort vs insertion sort: crossover

Encuentra el $n$ donde Shell sort (Ciura) empieza a ser más rápido que
insertion sort para datos aleatorios.

<details><summary>Predicción</summary>

Para $n$ muy pequeño (< 10), insertion sort gana por menor overhead
(Shell sort evalúa múltiples gaps incluso si hace poco trabajo con
cada uno).

El crossover debería estar alrededor de $n = 20-30$. Para $n = 25$:
- Insertion sort: ~$25^2/4 = 156$ shifts.
- Shell sort (Ciura, gaps 23, 10, 4, 1): ~80-100 shifts + overhead
  de múltiples pasadas.

En tiempo real, el crossover puede ser ~$n = 50$ por el overhead de
los loops extra de Shell sort. Para $n > 100$, Shell sort es
claramente superior.

</details>

### Ejercicio 4 — Propiedad h-sorted

Verifica empíricamente que un array que es 4-sorted sigue siendo
4-sorted después de aplicar una pasada con gap 2. Prueba con 100
arrays aleatorios de $n = 1000$.

<details><summary>Predicción</summary>

La propiedad se debe cumplir en el 100% de los casos. Es un teorema,
no una observación empírica: si un array es $h$-sorted y se ordena
con gap $k$, sigue siendo $h$-sorted.

La demostración informal: el gap-$k$ sort solo intercambia elementos
separados por distancia $k$. Si `arr[i] <= arr[i+h]` antes del
gap-$k$ sort, los únicos intercambios que podrían violar esta
propiedad son los que mueven `arr[i]` hacia arriba o `arr[i+h]` hacia
abajo. Pero como $h$ y $k$ son independientes, los sub-arrays que ve
el gap-$k$ sort son ortogonales a la relación $h$-sorted.

Los 100 arrays deberían dar 100% de preservación.

</details>

### Ejercicio 5 — Complejidad empírica

Mide las comparaciones de Shell sort (Knuth) para
$n \in \{100, 500, 1000, 5000, 10000, 50000\}$ con datos aleatorios.
Calcula el exponente $\alpha$ asumiendo $T(n) = c \cdot n^\alpha$.

<details><summary>Predicción</summary>

Para Knuth, la complejidad teórica es $O(n^{3/2})$, es decir
$\alpha = 1.5$.

Usando dos puntos: $\alpha = \log(T(n_2)/T(n_1)) / \log(n_2/n_1)$.

Valores esperados:
- $n = 1000$ → ~20000 cmp, $n = 10000$ → ~600000 cmp.
- $\alpha = \log(600000/20000) / \log(10) = \log(30) / 1 \approx 1.48$.

El exponente medido debería estar entre 1.3 y 1.5 para Knuth, y entre
1.2 y 1.4 para Ciura/Sedgewick (que tienen complejidad $O(n^{4/3})$
con $\alpha \approx 1.33$).

</details>

### Ejercicio 6 — Secuencia de Pratt

Implementa Shell sort con la secuencia de Pratt: todos los números
de la forma $2^p \cdot 3^q$ menores que $n$, en orden descendente.
Compara comparaciones con Knuth para $n = 10000$.

<details><summary>Predicción</summary>

La secuencia de Pratt tiene complejidad $O(n \log^2 n)$, que es
mejor asintóticamente que Knuth ($O(n^{3/2})$). Para $n = 10000$:
- Knuth: ~$10000^{1.5} \approx 1M$ operaciones.
- Pratt: ~$10000 \times 14^2 \approx 2M$ operaciones.

Paradójicamente, Pratt puede ser **más lento** para $n = 10000$ porque
tiene muchos más gaps (~$\log^2 n \approx 196$ gaps vs ~8 de Knuth),
y cada pasada tiene overhead fijo. El overhead de tantas pasadas
domina para $n$ práctico.

Pratt es teóricamente superior pero prácticamente inferior para
tamaños de entrada reales ($n < 10^7$).

</details>

### Ejercicio 7 — Shell sort para nearly sorted

Compara Shell sort (Ciura) e insertion sort para datos casi ordenados
(5% perturbados) con $n = 10000$.

<details><summary>Predicción</summary>

Para datos casi ordenados, insertion sort tiene
$O(n + k)$ donde $k$ es el número de inversiones.
Con 5% perturbados: ~$n/20 = 500$ elementos fuera de lugar, cada uno
a distancia ~$n/20$, así que $k \approx 500 \times 500/2 = 125000$
inversiones.

Insertion sort: ~$n + k = 10000 + 125000 = 135000$ comparaciones.

Shell sort: las pasadas con gaps grandes eliminan las inversiones de
distancia larga rápidamente. Pero tiene overhead de múltiples pasadas
sobre el array completo. Total: ~$80000-100000$ comparaciones.

Ambos son comparables. Para datos **muy** casi ordenados (1% perturbados),
insertion sort gana por su menor overhead. Para datos moderadamente
perturbados (10%+), Shell sort gana.

</details>

### Ejercicio 8 — Shell sort genérico

Implementa Shell sort genérico en C (interfaz `void *`, como `qsort`)
y en Rust (genérico con `T: Ord`). Ordena un array de strings.

<details><summary>Predicción</summary>

La versión genérica en C usa `memcpy` para shifts (en lugar de
asignación directa) y un comparador `CmpFunc`. El overhead de
`memcpy` + indirección hace que Shell sort genérico sea ~2-3x más
lento que la versión para `int`.

En Rust, la versión genérica con `T: Ord` se monomorphiza: el
compilador genera código especializado para cada tipo. No hay
overhead de indirección. La versión genérica es igual de rápida que
la versión específica.

Para strings en C: cada comparación llama `strcmp` (~50 ns) y cada
shift hace `memcpy` de un puntero (~8 bytes, rápido). El costo total
está dominado por las comparaciones, no los shifts.

</details>

### Ejercicio 9 — Diseñar tu propia secuencia de gaps

Experimenta con secuencias personalizadas. Prueba:
a) Fibonacci: 1, 1, 2, 3, 5, 8, 13, 21, 34, ...
b) Potencias de 3: 1, 3, 9, 27, 81, ...
c) Primos: 1, 2, 3, 5, 7, 11, 13, ...

Compara con Ciura para $n = 10000$.

<details><summary>Predicción</summary>

a) **Fibonacci**: ratio entre gaps consecutivos es $\phi \approx 1.618$
(vs ~2.25 para Ciura). Los gaps decrecen más lentamente → más pasadas
pero cada una es más efectiva. Rendimiento: probablemente similar a
Knuth, ligeramente peor que Ciura.

b) **Potencias de 3**: ratio = 3. Gaps decrecen más rápido → menos
pasadas pero cada una elimina menos inversiones. El problema es que
$\gcd(9, 3) = 3 \neq 1$, lo que es malo (los sub-arrays no se
mezclan entre pasadas). Rendimiento: probablemente peor que Shell.

c) **Primos**: los gaps son coprimos entre sí (buen para la propiedad
de mezcla), pero están muy juntos para valores pequeños, lo que genera
muchas pasadas innecesarias. Rendimiento: probablemente peor que Knuth.

Ninguna debería superar a Ciura — esa secuencia fue optimizada
numéricamente contra millones de entradas.

</details>

### Ejercicio 10 — Shell sort en sistema embebido simulado

Simula un "microcontrolador" con restricciones: sin `malloc`, sin
recursión, stack máximo de 256 bytes. Implementa Shell sort que
ordene un array de hasta 1000 elementos. Verifica que funciona
correctamente y mide el tiempo.

<details><summary>Predicción</summary>

Shell sort cumple todas las restricciones naturalmente:
- Sin `malloc`: solo usa una variable `key` (4 bytes) y un índice `j`.
- Sin recursión: tres loops anidados, todo iterativo.
- Stack: solo las variables locales del loop (~20 bytes total).

Para $n = 1000$ con secuencia de Ciura: ~8 pasadas, ~20000 comparaciones.
Con un "procesador" a 16 MHz (típico de ARM Cortex-M0), cada
comparación + shift toma ~10 ciclos. Total: ~200000 ciclos = ~12.5 ms.

Quicksort y merge sort no cumplen las restricciones:
- Quicksort: recursión usa $O(\log n)$ stack frames, ~$10 \times 40$
  bytes = 400 bytes para $n = 1000$ — excede 256 bytes.
- Merge sort: necesita $O(n)$ memoria auxiliar = 4000 bytes.

Shell sort es la mejor opción en este escenario.

</details>
