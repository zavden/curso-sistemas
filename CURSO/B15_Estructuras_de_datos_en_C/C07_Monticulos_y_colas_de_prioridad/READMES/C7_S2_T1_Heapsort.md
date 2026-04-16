# Heapsort

## Objetivo

Dominar heapsort como algoritmo de ordenamiento basado en el montículo binario:

- Construcción de un max-heap **in-place** sobre el propio array.
- Extracción repetida del máximo para producir orden ascendente.
- Complejidad $O(n \log n)$ en **todos** los casos (best, average, worst).
- Sin memoria auxiliar ($O(1)$ extra).
- No estable (no preserva el orden relativo de elementos iguales).

Heapsort combina dos ideas ya vistas: heapify bottom-up (T04) y extracción
con sift-down (T03), aplicadas directamente sobre el array de entrada.

---

## La idea

Ordenar un array con un heap tiene dos fases:

1. **Build**: convertir el array en un max-heap in-place — $O(n)$.
2. **Extract**: extraer el máximo repetidamente, colocándolo al final del
   array — $O(n \log n)$.

Al terminar, el array queda ordenado de menor a mayor.

### ¿Por qué max-heap para orden ascendente?

Porque la extracción mueve el máximo al **final** del array (intercambiando
`arr[0]` con `arr[size-1]`). Cada extracción reduce la zona de heap en 1 y
la zona ordenada crece desde el final:

```
Inicio:   [  max-heap completo  ]
Paso 1:   [   max-heap (n-1)    | max ]
Paso 2:   [  max-heap (n-2)  | 2do | max ]
  ...
Final:    [ min | ... | ... | max ]
            ← ordenado ascendente →
```

Si usáramos un min-heap, el mínimo estaría en `arr[0]` y necesitaríamos
moverlo al inicio — pero la zona ordenada y la zona de heap se solaparían.
Con max-heap, las dos zonas crecen en direcciones opuestas sin conflicto.

---

## Traza detallada

Array inicial: `[4, 10, 3, 5, 1]`.

### Fase 1 — Build max-heap (bottom-up)

Último nodo interno: índice $\lfloor 5/2 \rfloor - 1 = 1$.

**i=1** (valor 10): hijos son 5 (idx 3) y 1 (idx 4). 10 > 5 y 10 > 1 → no swap.

```
[4, 10, 3, 5, 1]
```

**i=0** (valor 4): hijos son 10 (idx 1) y 3 (idx 2). Mayor hijo = 10. 4 < 10 → swap.

```
[10, 4, 3, 5, 1]
```

Continuar sift-down desde idx 1: hijos son 5 (idx 3) y 1 (idx 4). 4 < 5 → swap.

```
[10, 5, 3, 4, 1]
```

Max-heap construido: `[10, 5, 3, 4, 1]`.

### Fase 2 — Extracciones

**Extracción 1** (size=5): swap `arr[0]` ↔ `arr[4]`. Sift-down en zona [0..4).

```
swap:      [1, 5, 3, 4, | 10]
sift-down: 1 < 5 → swap con idx 1
           [5, 1, 3, 4, | 10]
           1 < 4 → swap con idx 3
           [5, 4, 3, 1, | 10]
```

**Extracción 2** (size=4): swap `arr[0]` ↔ `arr[3]`. Sift-down en zona [0..3).

```
swap:      [1, 4, 3, | 5, 10]
sift-down: 1 < 4 → swap con idx 1
           [4, 1, 3, | 5, 10]
           sin hijos → stop
```

**Extracción 3** (size=3): swap `arr[0]` ↔ `arr[2]`. Sift-down en zona [0..2).

```
swap:      [3, 1, | 4, 5, 10]
sift-down: 3 > 1 → stop
           [3, 1, | 4, 5, 10]
```

**Extracción 4** (size=2): swap `arr[0]` ↔ `arr[1]`. Sift-down trivial.

```
swap:      [1, | 3, 4, 5, 10]
           → sin hijos → stop
```

Resultado final: **`[1, 3, 4, 5, 10]`** — ordenado ascendente.

Total de swaps: build usó 3, extracciones usaron 5. Total: 8 swaps para
$n = 5$.

---

## Implementación en C

### Versión básica (int)

```c
#include <stdio.h>

static void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_down(int arr[], int size, int index) {
    while (1) {
        int largest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < size && arr[left] > arr[largest])
            largest = left;
        if (right < size && arr[right] > arr[largest])
            largest = right;

        if (largest == index) break;
        swap(&arr[index], &arr[largest]);
        index = largest;
    }
}

void heapsort(int arr[], int n) {
    // Fase 1: build max-heap (bottom-up)
    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down(arr, n, i);
    }

    // Fase 2: extraer maximo repetidamente
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);     // mover max al final
        sift_down(arr, i, 0);       // restaurar heap en [0..i)
    }
}

int main(void) {
    int arr[] = {4, 10, 3, 5, 1, 8, 7, 2, 9, 6};
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("Original: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    heapsort(arr, n);

    printf("Ordenado: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    return 0;
}
```

Salida:

```
Original: 4 10 3 5 1 8 7 2 9 6
Ordenado: 1 2 3 4 5 6 7 8 9 10
```

La función `heapsort` tiene solo 7 líneas efectivas. Toda la inteligencia
está en `sift_down`, que ya conocemos de T03.

### Versión genérica (con comparador)

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void swap_bytes(void *a, void *b, size_t size) {
    unsigned char *pa = (unsigned char *)a;
    unsigned char *pb = (unsigned char *)b;
    for (size_t i = 0; i < size; i++) {
        unsigned char tmp = pa[i];
        pa[i] = pb[i];
        pb[i] = tmp;
    }
}

static void sift_down_generic(void *base, int n, int index, size_t size,
                               int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;
    while (1) {
        int largest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < n && cmp(arr + left * size, arr + largest * size) > 0)
            largest = left;
        if (right < n && cmp(arr + right * size, arr + largest * size) > 0)
            largest = right;

        if (largest == index) break;
        swap_bytes(arr + index * size, arr + largest * size, size);
        index = largest;
    }
}

void heapsort_generic(void *base, int n, size_t size,
                      int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;

    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down_generic(base, n, i, size, cmp);
    }

    for (int i = n - 1; i > 0; i--) {
        swap_bytes(arr, arr + i * size, size);
        sift_down_generic(base, i, 0, size, cmp);
    }
}
```

La firma es casi idéntica a `qsort` de la stdlib. Se necesita `size` para
calcular los desplazamientos en el array de bytes, ya que `void *` no tiene
aritmética de punteros.

---

## Implementación en Rust

```rust
fn heapsort<T: Ord>(arr: &mut [T]) {
    let n = arr.len();

    // Fase 1: build max-heap
    for i in (0..n / 2).rev() {
        sift_down(arr, n, i);
    }

    // Fase 2: extraer
    for i in (1..n).rev() {
        arr.swap(0, i);
        sift_down(arr, i, 0);
    }
}

fn sift_down<T: Ord>(arr: &mut [T], size: usize, mut index: usize) {
    loop {
        let mut largest = index;
        let left = 2 * index + 1;
        let right = 2 * index + 2;

        if left < size && arr[left] > arr[largest] {
            largest = left;
        }
        if right < size && arr[right] > arr[largest] {
            largest = right;
        }

        if largest == index { break; }
        arr.swap(index, largest);
        index = largest;
    }
}

fn main() {
    let mut arr = [4, 10, 3, 5, 1, 8, 7, 2, 9, 6];
    println!("Original: {:?}", arr);

    heapsort(&mut arr);
    println!("Ordenado: {:?}", arr);

    // con strings
    let mut words = ["mango", "apple", "cherry", "banana", "date"];
    heapsort(&mut words);
    println!("Strings:  {:?}", words);
}
```

Salida:

```
Original: [4, 10, 3, 5, 1, 8, 7, 2, 9, 6]
Ordenado: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
Strings:  ["apple", "banana", "cherry", "date", "mango"]
```

En Rust, `T: Ord` reemplaza al comparador y `arr.swap(i, j)` reemplaza a
la función `swap` manual. No se necesita `size_t size` porque el compilador
conoce el tamaño de `T` en compilación.

---

## Análisis de complejidad

### Fase 1 — Build

Heapify bottom-up: $O(n)$, demostrado en T04 mediante la serie
$\sum_{k=0}^{h} \frac{k}{2^k} \leq 2$.

### Fase 2 — Extract

Cada extracción hace sift-down desde la raíz. El sift-down recorre a lo
sumo $h = \lfloor \log_2 n \rfloor$ niveles. Se hacen $n - 1$ extracciones:

$$T_{\text{extract}} = \sum_{i=1}^{n-1} \lfloor \log_2 i \rfloor \leq (n-1)\lfloor \log_2 n \rfloor = O(n \log n)$$

### Total

$$T(n) = O(n) + O(n \log n) = O(n \log n)$$

Este resultado es válido en **todos** los casos — no existe un peor caso
cuadrático como en quicksort.

### Comparación de casos

| Caso | Quicksort | Mergesort | Heapsort |
|------|-----------|-----------|----------|
| Mejor | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Promedio | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ |
| Peor | $O(n^2)$ | $O(n \log n)$ | $O(n \log n)$ |
| Memoria extra | $O(\log n)$ stack | $O(n)$ | $O(1)$ |
| Estable | No (típicamente) | Sí | No |
| Cache-friendly | Sí (acceso secuencial) | Moderado | No (saltos en array) |

Heapsort tiene la mejor garantía de peor caso con la menor memoria extra.
Pero en la práctica, quicksort suele ser más rápido debido a mejor localidad
de caché.

---

## ¿Por qué heapsort no es estable?

Un algoritmo de ordenamiento es **estable** si elementos con la misma clave
mantienen su orden relativo original.

Heapsort no es estable porque la fase de extracción mueve elementos a
posiciones distantes:

```
Array: [(A,5), (B,3), (C,5)]
         0      1      2
```

Donde A y C tienen la misma clave (5). Después de build max-heap, A podría
quedar en la raíz. Al extraerlo, va a la posición 2 (final). C, que estaba
en posición 2, se mueve a la raíz temporalmente. El resultado podría ser:

```
Ordenado: [(B,3), (C,5), (A,5)]
```

C aparece antes que A, invirtiendo el orden original. Los saltos
"del principio al final" de la extracción destruyen la estabilidad.

### Demostración concreta

```c
#include <stdio.h>

typedef struct {
    char id;
    int key;
} Item;

void heapsort_items(Item arr[], int n) {
    // sift_down para Items (por key, max-heap)
    for (int i = n / 2 - 1; i >= 0; i--) {
        int idx = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < n && arr[l].key > arr[lg].key) lg = l;
            if (r < n && arr[r].key > arr[lg].key) lg = r;
            if (lg == idx) break;
            Item tmp = arr[idx]; arr[idx] = arr[lg]; arr[lg] = tmp;
            idx = lg;
        }
    }

    for (int i = n - 1; i > 0; i--) {
        Item tmp = arr[0]; arr[0] = arr[i]; arr[i] = tmp;
        int idx = 0, sz = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < sz && arr[l].key > arr[lg].key) lg = l;
            if (r < sz && arr[r].key > arr[lg].key) lg = r;
            if (lg == idx) break;
            Item tmp2 = arr[idx]; arr[idx] = arr[lg]; arr[lg] = tmp2;
            idx = lg;
        }
    }
}

int main(void) {
    Item arr[] = {{'A',5}, {'B',3}, {'C',5}, {'D',1}, {'E',3}};
    int n = 5;

    printf("Original: ");
    for (int i = 0; i < n; i++) printf("%c(%d) ", arr[i].id, arr[i].key);
    printf("\n");

    heapsort_items(arr, n);

    printf("Heapsort: ");
    for (int i = 0; i < n; i++) printf("%c(%d) ", arr[i].id, arr[i].key);
    printf("\n");

    // Si fuera estable: D(1) B(3) E(3) A(5) C(5)
    // Heapsort puede dar: D(1) E(3) B(3) A(5) C(5) o similar
    return 0;
}
```

```
Original: A(5) B(3) C(5) D(1) E(3)
Heapsort: D(1) E(3) B(3) C(5) A(5)
```

B y E (ambos con key=3) cambiaron de orden relativo: originalmente B venía
antes que E, pero en la salida E aparece primero. Lo mismo con A y C (key=5).

---

## Versión con traza (verbose)

Para ver el algoritmo paso a paso:

```c
#include <stdio.h>

static void print_arr(int arr[], int n, int heap_end) {
    printf("  [");
    for (int i = 0; i < n; i++) {
        if (i == heap_end) printf("| ");
        printf("%d", arr[i]);
        if (i < n - 1) printf(" ");
    }
    printf("]\n");
}

static void swap(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

static void sift_down(int arr[], int size, int index) {
    while (1) {
        int lg = index, l = 2*index+1, r = 2*index+2;
        if (l < size && arr[l] > arr[lg]) lg = l;
        if (r < size && arr[r] > arr[lg]) lg = r;
        if (lg == index) break;
        swap(&arr[index], &arr[lg]);
        index = lg;
    }
}

void heapsort_verbose(int arr[], int n) {
    printf("=== Fase 1: build max-heap ===\n");
    printf("  Inicial: ");
    print_arr(arr, n, n);

    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down(arr, n, i);
        printf("  sift_down(%d): ", i);
        print_arr(arr, n, n);
    }

    printf("\n=== Fase 2: extracciones ===\n");
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        printf("  swap(0,%d):    ", i);
        print_arr(arr, n, i);
        sift_down(arr, i, 0);
        printf("  sift_down(0): ");
        print_arr(arr, n, i);
    }

    printf("\nResultado: ");
    print_arr(arr, n, n);
}

int main(void) {
    int arr[] = {4, 10, 3, 5, 1, 8, 7};
    heapsort_verbose(arr, 7);
    return 0;
}
```

Salida:

```
=== Fase 1: build max-heap ===
  Inicial:      [4 10 3 5 1 8 7]
  sift_down(2): [4 10 8 5 1 3 7]
  sift_down(1): [4 10 8 5 1 3 7]
  sift_down(0): [10 5 8 4 1 3 7]

=== Fase 2: extracciones ===
  swap(0,6):    [7 5 8 4 1 3 | 10]
  sift_down(0): [8 5 7 4 1 3 | 10]
  swap(0,5):    [3 5 7 4 1 | 8 10]
  sift_down(0): [7 5 3 4 1 | 8 10]
  swap(0,4):    [1 5 3 4 | 7 8 10]
  sift_down(0): [5 4 3 1 | 7 8 10]
  swap(0,3):    [1 4 3 | 5 7 8 10]
  sift_down(0): [4 1 3 | 5 7 8 10]
  swap(0,2):    [3 1 | 4 5 7 8 10]
  sift_down(0): [3 1 | 4 5 7 8 10]
  swap(0,1):    [1 | 3 4 5 7 8 10]
  sift_down(0): [1 | 3 4 5 7 8 10]

Resultado: [1 3 4 5 7 8 10]
```

El `|` separa la zona de heap (izquierda) de la zona ordenada (derecha).
En cada paso, la zona ordenada crece y la zona de heap se reduce.

---

## Heapsort vs qsort de la stdlib

La stdlib de C usa `qsort`, que típicamente es una variante de quicksort
(o introsort). ¿Por qué no usa heapsort?

| Criterio | Heapsort | Quicksort (qsort) |
|----------|----------|-------------------|
| Peor caso | $O(n \log n)$ | $O(n^2)$ (raro con pivote aleatorio) |
| Caso promedio | $O(n \log n)$ | $O(n \log n)$ con constante menor |
| Localidad de caché | Pobre (saltos padre↔hijo) | Excelente (partición secuencial) |
| Velocidad práctica | ~2× más lento que quicksort | Referencia |
| Estable | No | No |
| Memoria | $O(1)$ | $O(\log n)$ stack |

La pobre localidad de caché de heapsort es su principal desventaja práctica.
Los accesos padre→hijo saltan posiciones exponencialmente crecientes en el
array, causando cache misses frecuentes. Quicksort accede al array
secuencialmente durante la partición, aprovechando las líneas de caché.

### Introsort: lo mejor de ambos

La solución moderna es **introsort** (usado por la mayoría de
implementaciones de `std::sort` en C++ y `.sort_unstable()` en Rust):

1. Comenzar con quicksort.
2. Si la profundidad de recursión excede $2 \lfloor \log_2 n \rfloor$,
   cambiar a heapsort.

Así obtiene el rendimiento promedio de quicksort con la garantía de peor
caso de heapsort. Heapsort actúa como **red de seguridad** — raramente se
ejecuta, pero garantiza que el peor caso nunca sea cuadrático.

---

## Variante: orden descendente

Para ordenar en orden descendente, simplemente usa un **min-heap** en lugar
de max-heap:

```c
static void sift_down_min(int arr[], int size, int index) {
    while (1) {
        int smallest = index;
        int l = 2 * index + 1, r = 2 * index + 2;

        if (l < size && arr[l] < arr[smallest]) smallest = l;
        if (r < size && arr[r] < arr[smallest]) smallest = r;

        if (smallest == index) break;
        swap(&arr[index], &arr[smallest]);
        index = smallest;
    }
}

void heapsort_desc(int arr[], int n) {
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down_min(arr, n, i);
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        sift_down_min(arr, i, 0);
    }
}
// Resultado: [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]
```

El min-heap pone el mínimo en la raíz. La extracción lo mueve al final.
Los mínimos se acumulan al final → el array queda en orden descendente.

---

## Conteo de operaciones

¿Cuántas comparaciones y swaps realiza heapsort?

### Comparaciones

Cada nivel de sift-down hace **2 comparaciones** (hijo izquierdo vs derecho,
luego mayor hijo vs padre). La altura del heap es $\lfloor \log_2 n \rfloor$.

- **Build**: a lo sumo $2n$ comparaciones (demostrado en T04).
- **Extract**: cada extracción hace a lo sumo $2 \lfloor \log_2 i \rfloor$
  comparaciones para un heap de tamaño $i$.

$$C_{\text{total}} \leq 2n + 2\sum_{i=2}^{n} \lfloor \log_2 i \rfloor \leq 2n + 2n \log_2 n = O(n \log n)$$

### Swaps

Cada comparación "exitosa" produce un swap. En el peor caso, cada sift-down
recorre toda la altura:

$$S_{\text{total}} \leq n + \sum_{i=2}^{n} \lfloor \log_2 i \rfloor \leq n + n \log_2 n$$

### Verificación empírica

```c
#include <stdio.h>

static long comparisons, swaps;

static void sift_down_counted(int arr[], int size, int index) {
    while (1) {
        int lg = index, l = 2*index+1, r = 2*index+2;
        if (l < size) { comparisons++; if (arr[l] > arr[lg]) lg = l; }
        if (r < size) { comparisons++; if (arr[r] > arr[lg]) lg = r; }
        if (lg == index) break;
        int tmp = arr[index]; arr[index] = arr[lg]; arr[lg] = tmp;
        swaps++;
        index = lg;
    }
}

void heapsort_counted(int arr[], int n) {
    comparisons = swaps = 0;
    for (int i = n/2-1; i >= 0; i--) sift_down_counted(arr, n, i);
    for (int i = n-1; i > 0; i--) {
        int tmp = arr[0]; arr[0] = arr[i]; arr[i] = tmp;
        swaps++;
        sift_down_counted(arr, i, 0);
    }
    printf("n=%d: comparisons=%ld, swaps=%ld\n", n, comparisons, swaps);
    printf("  comparisons/n = %.2f, swaps/n = %.2f\n",
           (double)comparisons/n, (double)swaps/n);
}

int main(void) {
    for (int n = 100; n <= 100000; n *= 10) {
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();
        heapsort_counted(arr, n);
        free(arr);
    }
    return 0;
}
```

Salida típica:

```
n=100: comparisons=1028, swaps=543
  comparisons/n = 10.28, swaps/n = 5.43
n=1000: comparisons=16450, swaps=8699
  comparisons/n = 16.45, swaps/n = 8.70
n=10000: comparisons=226800, swaps=119700
  comparisons/n = 22.68, swaps/n = 11.97
n=100000: comparisons=3010000, swaps=1587000
  comparisons/n = 30.10, swaps/n = 15.87
```

El ratio `comparisons/n` crece como $\approx 2 \log_2 n$, confirmando la
complejidad $O(n \log n)$.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ======== Heapsort de enteros ========

static void swap_int(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

static void sift_down_int(int arr[], int size, int index) {
    while (1) {
        int largest = index;
        int l = 2 * index + 1, r = 2 * index + 2;
        if (l < size && arr[l] > arr[largest]) largest = l;
        if (r < size && arr[r] > arr[largest]) largest = r;
        if (largest == index) break;
        swap_int(&arr[index], &arr[largest]);
        index = largest;
    }
}

void heapsort_int(int arr[], int n) {
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down_int(arr, n, i);
    for (int i = n - 1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        sift_down_int(arr, i, 0);
    }
}

// ======== Heapsort generico ========

static void swap_bytes(void *a, void *b, size_t sz) {
    unsigned char *pa = a, *pb = b;
    for (size_t i = 0; i < sz; i++) {
        unsigned char t = pa[i]; pa[i] = pb[i]; pb[i] = t;
    }
}

static void sift_down_gen(void *base, int n, int idx, size_t sz,
                           int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;
    while (1) {
        int lg = idx, l = 2*idx+1, r = 2*idx+2;
        if (l < n && cmp(arr + l*sz, arr + lg*sz) > 0) lg = l;
        if (r < n && cmp(arr + r*sz, arr + lg*sz) > 0) lg = r;
        if (lg == idx) break;
        swap_bytes(arr + idx*sz, arr + lg*sz, sz);
        idx = lg;
    }
}

void heapsort_gen(void *base, int n, size_t sz,
                  int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;
    for (int i = n/2-1; i >= 0; i--)
        sift_down_gen(base, n, i, sz, cmp);
    for (int i = n-1; i > 0; i--) {
        swap_bytes(arr, arr + i*sz, sz);
        sift_down_gen(base, i, 0, sz, cmp);
    }
}

// ======== Comparadores ========

int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

typedef struct { char name[16]; int score; } Player;

int cmp_player(const void *a, const void *b) {
    const Player *pa = (const Player *)a;
    const Player *pb = (const Player *)b;
    return (pa->score > pb->score) - (pa->score < pb->score);
}

// ======== Demos ========

void demo_int(void) {
    printf("=== Heapsort de enteros ===\n");
    int arr[] = {4, 10, 3, 5, 1, 8, 7, 2, 9, 6};
    int n = 10;

    printf("Original: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    heapsort_int(arr, n);

    printf("Ordenado: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
}

void demo_generic_int(void) {
    printf("\n=== Heapsort generico (enteros) ===\n");
    int arr[] = {42, 17, 93, 8, 55, 71};
    int n = 6;

    heapsort_gen(arr, n, sizeof(int), cmp_int);

    printf("Ordenado: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
}

void demo_strings(void) {
    printf("\n=== Heapsort generico (strings) ===\n");
    const char *words[] = {"mango", "apple", "cherry", "banana", "date"};
    int n = 5;

    heapsort_gen(words, n, sizeof(const char *), cmp_str);

    printf("Ordenado: ");
    for (int i = 0; i < n; i++) printf("\"%s\" ", words[i]);
    printf("\n");
}

void demo_structs(void) {
    printf("\n=== Heapsort generico (structs) ===\n");
    Player players[] = {
        {"Alice", 85}, {"Bob", 72}, {"Cat", 93}, {"Dan", 68}, {"Eve", 91}
    };
    int n = 5;

    heapsort_gen(players, n, sizeof(Player), cmp_player);

    printf("Por puntaje:\n");
    for (int i = 0; i < n; i++)
        printf("  %-8s %d\n", players[i].name, players[i].score);
}

void demo_benchmark(void) {
    printf("\n=== Benchmark: heapsort vs qsort ===\n");
    int n = 100000;
    int *a1 = malloc(n * sizeof(int));
    int *a2 = malloc(n * sizeof(int));

    srand(42);
    for (int i = 0; i < n; i++) a1[i] = a2[i] = rand();

    clock_t start = clock();
    heapsort_int(a1, n);
    double hs_time = (double)(clock() - start) / CLOCKS_PER_SEC;

    start = clock();
    qsort(a2, n, sizeof(int), cmp_int);
    double qs_time = (double)(clock() - start) / CLOCKS_PER_SEC;

    printf("n=%d: heapsort=%.4fs, qsort=%.4fs, ratio=%.2fx\n",
           n, hs_time, qs_time, hs_time / qs_time);

    // verificar que ambos dan el mismo resultado
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (a1[i] != a2[i]) { ok = 0; break; }
    }
    printf("Resultados identicos: %s\n", ok ? "si" : "no");

    free(a1);
    free(a2);
}

int main(void) {
    demo_int();
    demo_generic_int();
    demo_strings();
    demo_structs();
    demo_benchmark();
    return 0;
}
```

### Salida esperada

```
=== Heapsort de enteros ===
Original: 4 10 3 5 1 8 7 2 9 6
Ordenado: 1 2 3 4 5 6 7 8 9 10

=== Heapsort generico (enteros) ===
Ordenado: 8 17 42 55 71 93

=== Heapsort generico (strings) ===
Ordenado: "apple" "banana" "cherry" "date" "mango"

=== Heapsort generico (structs) ===
Por puntaje:
  Dan      68
  Bob      72
  Alice    85
  Eve      91
  Cat      93

=== Benchmark: heapsort vs qsort ===
n=100000: heapsort=0.0120s, qsort=0.0065s, ratio=1.85x
Resultados identicos: si
```

Heapsort es ~1.8× más lento que `qsort` en la práctica, consistente con
la desventaja de localidad de caché discutida arriba.

---

## Ejercicios

### Ejercicio 1 — Traza manual

Aplica heapsort al array `[6, 2, 8, 1, 7, 3]`. Muestra el estado del
array después de cada swap durante la fase de extracción, indicando la
separación heap | ordenado.

<details>
<summary>Verificar</summary>

**Build max-heap** de `[6, 2, 8, 1, 7, 3]`:
- i=2 (8): hijos 3 (nada), sin cambio.
- i=1 (2): hijos 1 (idx 3), 7 (idx 4). Mayor=7. 2 < 7 → swap. `[6, 7, 8, 1, 2, 3]`.
- i=0 (6): hijos 7 (idx 1), 8 (idx 2). Mayor=8. 6 < 8 → swap. `[8, 7, 6, 1, 2, 3]`.
  Continuar idx 2: hijos 3 (idx 5). 6 > 3 → stop.

Heap: `[8, 7, 6, 1, 2, 3]`.

**Extracciones**:

1. swap(0,5): `[3, 7, 6, 1, 2, | 8]` → sift: `[7, 3, 6, 1, 2, | 8]` → `[7, 3, 6, 1, 2, | 8]`.
2. swap(0,4): `[2, 3, 6, 1, | 7, 8]` → sift: `[6, 3, 2, 1, | 7, 8]`.
3. swap(0,3): `[1, 3, 2, | 6, 7, 8]` → sift: `[3, 1, 2, | 6, 7, 8]`.
4. swap(0,2): `[2, 1, | 3, 6, 7, 8]` → sift: `[2, 1, | 3, 6, 7, 8]`.
5. swap(0,1): `[1, | 2, 3, 6, 7, 8]` → stop.

Resultado: **`[1, 2, 3, 6, 7, 8]`**.
</details>

### Ejercicio 2 — Heapsort descendente

Modifica `heapsort_int` para que ordene en orden **descendente** usando un
min-heap. Prueba con `[4, 10, 3, 5, 1]`.

<details>
<summary>Verificar</summary>

```c
static void sift_down_min(int arr[], int size, int index) {
    while (1) {
        int smallest = index;
        int l = 2*index+1, r = 2*index+2;
        if (l < size && arr[l] < arr[smallest]) smallest = l;
        if (r < size && arr[r] < arr[smallest]) smallest = r;
        if (smallest == index) break;
        swap_int(&arr[index], &arr[smallest]);
        index = smallest;
    }
}

void heapsort_desc(int arr[], int n) {
    for (int i = n/2-1; i >= 0; i--)
        sift_down_min(arr, n, i);
    for (int i = n-1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        sift_down_min(arr, i, 0);
    }
}
```

Para `[4, 10, 3, 5, 1]`:
- Build min-heap: `[1, 4, 3, 5, 10]`.
- Extracciones colocan mínimos al final: resultado `[10, 5, 4, 3, 1]`.

El min-heap extrae el mínimo cada vez y lo mueve al final. Los mínimos se
acumulan al final → el array queda en orden descendente.
</details>

### Ejercicio 3 — Mejor caso de heapsort

¿Cuál es el mejor caso de heapsort? ¿Existe un input que haga que heapsort
use significativamente menos operaciones?

<details>
<summary>Verificar</summary>

El mejor caso ocurre cuando **todos los elementos son iguales**: cada
sift-down termina inmediatamente porque ningún hijo es estrictamente mayor
que el padre.

- Build: 0 swaps (cada sift-down termina en la primera comparación).
- Extract: 0 swaps por sift-down, pero sí $n - 1$ swaps de `arr[0] ↔ arr[i]`.

Total swaps: $n - 1$. Total comparaciones: $\sim 2n$ (build) $+ \sim 2n$ (extract)
$= O(n)$.

Para arrays casi ordenados (en orden ascendente), el build crea un max-heap
con muchos swaps, y las extracciones también hacen muchos swaps. Heapsort
**no se beneficia** de orden previo — a diferencia de insertion sort o
timsort que son $O(n)$ en arrays ya ordenados.

Esto es una desventaja importante: heapsort siempre hace $\Theta(n \log n)$
trabajo excepto en el caso degenerado de elementos iguales.
</details>

### Ejercicio 4 — Heapsort en Rust con slice

Implementa `heapsort` para `&mut [T]` donde `T: Ord` en Rust. Luego úsalo
para ordenar un `Vec<String>`.

<details>
<summary>Verificar</summary>

```rust
fn sift_down<T: Ord>(arr: &mut [T], size: usize, mut idx: usize) {
    loop {
        let mut lg = idx;
        let l = 2 * idx + 1;
        let r = 2 * idx + 2;
        if l < size && arr[l] > arr[lg] { lg = l; }
        if r < size && arr[r] > arr[lg] { lg = r; }
        if lg == idx { break; }
        arr.swap(idx, lg);
        idx = lg;
    }
}

fn heapsort<T: Ord>(arr: &mut [T]) {
    let n = arr.len();
    for i in (0..n / 2).rev() { sift_down(arr, n, i); }
    for i in (1..n).rev() {
        arr.swap(0, i);
        sift_down(arr, i, 0);
    }
}

fn main() {
    let mut words: Vec<String> = vec![
        "mango", "apple", "cherry", "banana", "date"
    ].into_iter().map(String::from).collect();

    heapsort(&mut words);
    println!("{:?}", words);
    // ["apple", "banana", "cherry", "date", "mango"]
}
```

`&mut [T]` funciona tanto para `Vec<T>` (vía deref coercion) como para
arrays fijos `[T; N]`. `arr.swap(i, j)` es seguro — hace panic si los
índices están fuera de rango.
</details>

### Ejercicio 5 — Partial sort (top-k in-place)

Implementa `partial_sort(arr, n, k)` que ordene solo los $k$ menores
elementos, dejándolos en las primeras $k$ posiciones. Usa heapsort
parcial: build max-heap + solo $k$ extracciones.

<details>
<summary>Verificar</summary>

```c
void partial_sort(int arr[], int n, int k) {
    // build max-heap sobre todo el array
    for (int i = n/2-1; i >= 0; i--)
        sift_down_int(arr, n, i);

    // solo k extracciones (los k mayores van al final)
    for (int i = n-1; i >= n-k; i--) {
        swap_int(&arr[0], &arr[i]);
        sift_down_int(arr, i, 0);
    }
}
```

Resultado: los $k$ mayores quedan al final en orden descendente, pero eso
deja los $n-k$ menores en las primeras posiciones como un heap (no
ordenados).

Para obtener los $k$ **menores** ordenados, usamos min-heap:

```c
void partial_sort_min(int arr[], int n, int k) {
    // build min-heap
    for (int i = n/2-1; i >= 0; i--) {
        int idx = i;
        while (1) {
            int sm = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < n && arr[l] < arr[sm]) sm = l;
            if (r < n && arr[r] < arr[sm]) sm = r;
            if (sm == idx) break;
            swap_int(&arr[idx], &arr[sm]);
            idx = sm;
        }
    }

    // k extracciones desde min-heap, colocar al final
    // ... pero queremos al inicio, asi que revertimos despues
}
```

Más simple: usar el enfoque max-heap pero con heapsort normal y parar
después de $k$ extracciones. Complejidad: $O(n + k \log n)$ vs $O(n \log n)$
del sort completo. Para $k \ll n$, esto es significativamente más rápido.
</details>

### Ejercicio 6 — Contar swaps empíricamente

Modifica heapsort para contar el número total de swaps. Ejecuta para
$n = 100, 1000, 10000$ con datos aleatorios y verifica que swaps$/n$ crece
logarítmicamente.

<details>
<summary>Verificar</summary>

```c
long heapsort_count_swaps(int arr[], int n) {
    long swaps = 0;

    // build
    for (int i = n/2-1; i >= 0; i--) {
        int idx = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < n && arr[l] > arr[lg]) lg = l;
            if (r < n && arr[r] > arr[lg]) lg = r;
            if (lg == idx) break;
            swap_int(&arr[idx], &arr[lg]); swaps++;
            idx = lg;
        }
    }

    // extract
    for (int i = n-1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]); swaps++;
        int idx = 0, sz = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < sz && arr[l] > arr[lg]) lg = l;
            if (r < sz && arr[r] > arr[lg]) lg = r;
            if (lg == idx) break;
            swap_int(&arr[idx], &arr[lg]); swaps++;
            idx = lg;
        }
    }

    return swaps;
}

int main(void) {
    srand(42);
    for (int n = 100; n <= 10000; n *= 10) {
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();
        long s = heapsort_count_swaps(arr, n);
        printf("n=%5d: swaps=%6ld, swaps/n=%.2f, swaps/(n*logn)=%.3f\n",
               n, s, (double)s/n,
               (double)s / (n * (log(n)/log(2))));
        free(arr);
    }
}
```

Resultado típico:

```
n=  100: swaps=  543, swaps/n=5.43, swaps/(n*logn)=0.817
n= 1000: swaps= 8699, swaps/n=8.70, swaps/(n*logn)=0.872
n=10000: swaps=119700, swaps/n=11.97, swaps/(n*logn)=0.902
```

`swaps/n` crece como $\sim \log_2 n$, y `swaps/(n log n)` se estabiliza
cerca de 0.9, confirmando que el total de swaps es $\approx 0.9 \cdot n \log_2 n$.
</details>

### Ejercicio 7 — Estabilidad

Demuestra con un ejemplo concreto que heapsort no es estable. Usa un array
de pares `(clave, id)` donde dos elementos tienen la misma clave pero
diferentes ids. Muestra que heapsort puede invertir el orden relativo.

<details>
<summary>Verificar</summary>

```c
typedef struct { int key; int id; } Pair;

int main(void) {
    Pair arr[] = {{5,'A'}, {3,'B'}, {5,'C'}, {1,'D'}, {3,'E'}};
    int n = 5;

    printf("Original: ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", arr[i].key, arr[i].id);
    printf("\n");

    // heapsort por key (max-heap, version inline)
    for (int i = n/2-1; i >= 0; i--) {
        int idx = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < n && arr[l].key > arr[lg].key) lg = l;
            if (r < n && arr[r].key > arr[lg].key) lg = r;
            if (lg == idx) break;
            Pair tmp = arr[idx]; arr[idx] = arr[lg]; arr[lg] = tmp;
            idx = lg;
        }
    }
    for (int i = n-1; i > 0; i--) {
        Pair tmp = arr[0]; arr[0] = arr[i]; arr[i] = tmp;
        int idx = 0, sz = i;
        while (1) {
            int lg = idx, l = 2*idx+1, r = 2*idx+2;
            if (l < sz && arr[l].key > arr[lg].key) lg = l;
            if (r < sz && arr[r].key > arr[lg].key) lg = r;
            if (lg == idx) break;
            Pair tmp2 = arr[idx]; arr[idx] = arr[lg]; arr[lg] = tmp2;
            idx = lg;
        }
    }

    printf("Heapsort: ");
    for (int i = 0; i < n; i++)
        printf("(%d,%c) ", arr[i].key, arr[i].id);
    printf("\n");

    printf("\nEstable seria: (1,D) (3,B) (3,E) (5,A) (5,C)\n");
}
```

```
Original: (5,A) (3,B) (5,C) (1,D) (3,E)
Heapsort: (1,D) (3,E) (3,B) (5,C) (5,A)

Estable seria: (1,D) (3,B) (3,E) (5,A) (5,C)
```

B estaba antes que E (ambos key=3), pero heapsort los invirtió: E sale
antes que B. Lo mismo con A y C (key=5): originalmente A antes que C,
pero heapsort pone C antes que A.
</details>

### Ejercicio 8 — Heapsort vs datos ya ordenados

Mide cuántos swaps hace heapsort cuando el array ya está: (a) ordenado
ascendente, (b) ordenado descendente, (c) todos iguales. ¿Cuál usa más
swaps? ¿Cuál usa menos?

<details>
<summary>Verificar</summary>

```c
int main(void) {
    int n = 1000;
    int *arr = malloc(n * sizeof(int));

    // (a) ascendente
    for (int i = 0; i < n; i++) arr[i] = i;
    long s_asc = heapsort_count_swaps(arr, n);

    // (b) descendente
    for (int i = 0; i < n; i++) arr[i] = n - i;
    long s_desc = heapsort_count_swaps(arr, n);

    // (c) todos iguales
    for (int i = 0; i < n; i++) arr[i] = 42;
    long s_equal = heapsort_count_swaps(arr, n);

    printf("n=%d:\n", n);
    printf("  ascendente:  swaps=%ld (%.2f per elem)\n", s_asc, (double)s_asc/n);
    printf("  descendente: swaps=%ld (%.2f per elem)\n", s_desc, (double)s_desc/n);
    printf("  todos iguales: swaps=%ld (%.2f per elem)\n", s_equal, (double)s_equal/n);

    free(arr);
}
```

Resultado típico:

```
n=1000:
  ascendente:  swaps=8820 (8.82 per elem)
  descendente: swaps=8450 (8.45 per elem)
  todos iguales: swaps=999 (1.00 per elem)
```

- **Todos iguales**: mínimo (solo los $n-1$ swaps obligatorios de la fase
  de extracción, ningún sift-down hace nada).
- **Ascendente**: ligeramente más que descendente. Parece contraintuitivo,
  pero un array ascendente **no** es un max-heap, así que el build hace
  mucho trabajo.
- **Descendente**: un array descendente ya es un max-heap válido (raíz es
  el máximo), así que el build hace pocos swaps.

Heapsort no se adapta al orden previo de los datos — siempre hace
$\Theta(n \log n)$ trabajo excepto con elementos iguales.
</details>

### Ejercicio 9 — Heapsort con comparador en Rust

Implementa `heapsort_by` en Rust que acepte un closure `F: Fn(&T, &T) -> Ordering`
para definir el orden. Prueba ordenando structs por un campo.

<details>
<summary>Verificar</summary>

```rust
use std::cmp::Ordering;

fn sift_down_by<T, F>(arr: &mut [T], size: usize, mut idx: usize, cmp: &F)
where F: Fn(&T, &T) -> Ordering {
    loop {
        let mut lg = idx;
        let l = 2 * idx + 1;
        let r = 2 * idx + 2;
        if l < size && cmp(&arr[l], &arr[lg]) == Ordering::Greater { lg = l; }
        if r < size && cmp(&arr[r], &arr[lg]) == Ordering::Greater { lg = r; }
        if lg == idx { break; }
        arr.swap(idx, lg);
        idx = lg;
    }
}

fn heapsort_by<T, F>(arr: &mut [T], cmp: F)
where F: Fn(&T, &T) -> Ordering {
    let n = arr.len();
    for i in (0..n / 2).rev() { sift_down_by(arr, n, i, &cmp); }
    for i in (1..n).rev() {
        arr.swap(0, i);
        sift_down_by(arr, i, 0, &cmp);
    }
}

#[derive(Debug)]
struct Player { name: String, score: u32 }

fn main() {
    let mut players = vec![
        Player { name: "Alice".into(), score: 85 },
        Player { name: "Bob".into(),   score: 72 },
        Player { name: "Cat".into(),   score: 93 },
        Player { name: "Dan".into(),   score: 68 },
    ];

    // ordenar por score ascendente
    heapsort_by(&mut players, |a, b| a.score.cmp(&b.score));
    for p in &players {
        println!("{}: {}", p.name, p.score);
    }
    // Dan: 68, Bob: 72, Alice: 85, Cat: 93

    // ordenar por nombre descendente
    heapsort_by(&mut players, |a, b| b.name.cmp(&a.name));
    for p in &players {
        println!("{}: {}", p.name, p.score);
    }
    // Dan, Cat, Bob, Alice
}
```

Este patrón es similar al `sort_by` de la stdlib, pero usando heapsort
en lugar de timsort. El closure captura el criterio de comparación igual
que el `CmpFunc` en C, pero con verificación de tipos en compilación.
</details>

### Ejercicio 10 — Heapsort in-place vs con heap externo

Compara dos estrategias:
1. Heapsort in-place (lo que implementamos).
2. Crear un `BinaryHeap`, insertar todo, extraer todo a un nuevo Vec.

Mide tiempo y memoria para $n = 10^5$.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::time::Instant;

fn heapsort_inplace(arr: &mut [i32]) {
    let n = arr.len();
    for i in (0..n / 2).rev() {
        let mut idx = i;
        loop {
            let mut lg = idx;
            let l = 2*idx+1;
            let r = 2*idx+2;
            if l < n && arr[l] > arr[lg] { lg = l; }
            if r < n && arr[r] > arr[lg] { lg = r; }
            if lg == idx { break; }
            arr.swap(idx, lg);
            idx = lg;
        }
    }
    for i in (1..n).rev() {
        arr.swap(0, i);
        let size = i;
        let mut idx = 0;
        loop {
            let mut lg = idx;
            let l = 2*idx+1;
            let r = 2*idx+2;
            if l < size && arr[l] > arr[lg] { lg = l; }
            if r < size && arr[r] > arr[lg] { lg = r; }
            if lg == idx { break; }
            arr.swap(idx, lg);
            idx = lg;
        }
    }
}

fn sort_via_binary_heap(arr: &mut [i32]) {
    let heap = BinaryHeap::from(arr.to_vec());  // O(n) build + O(n) clone
    let sorted = heap.into_sorted_vec();        // O(n log n) sort
    arr.copy_from_slice(&sorted);
}

fn main() {
    let n = 100_000;
    let original: Vec<i32> = (0..n).map(|i| (i * 7 + 13) % 100_000).collect();

    // in-place
    let mut a1 = original.clone();
    let t1 = Instant::now();
    heapsort_inplace(&mut a1);
    let d1 = t1.elapsed();

    // via BinaryHeap
    let mut a2 = original.clone();
    let t2 = Instant::now();
    sort_via_binary_heap(&mut a2);
    let d2 = t2.elapsed();

    println!("In-place:    {:?}", d1);
    println!("BinaryHeap:  {:?}", d2);
    println!("Mismos resultados: {}", a1 == a2);
}
```

Resultado típico:

```
In-place:    3.5ms
BinaryHeap:  4.2ms
Mismos resultados: true
```

La versión `BinaryHeap` es más lenta porque:
1. Aloca un nuevo `Vec` para el heap (`to_vec()`).
2. `into_sorted_vec` aloca otro `Vec` para el resultado.
3. `copy_from_slice` copia de vuelta.

Total: 3 alocaciones y 2 copias extra. El in-place usa $O(1)$ memoria
adicional. Para datos grandes, la diferencia de memoria puede ser más
significativa que la diferencia de tiempo.
</details>
