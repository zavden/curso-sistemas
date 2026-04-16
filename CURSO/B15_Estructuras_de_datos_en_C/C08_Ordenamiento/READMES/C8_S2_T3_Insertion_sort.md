# Insertion sort

## Objetivo

Dominar insertion sort como el mejor algoritmo cuadrático y entender por
qué es la base de algoritmos modernos como Timsort e introsort:

- Insertar cada elemento en su posición correcta dentro de la porción
  ya ordenada.
- **Adaptativo**: $O(n)$ para datos casi ordenados, $O(n + k)$ donde $k$
  es el número de inversiones.
- Estable, in-place, con excelente rendimiento para arrays pequeños.
- Usado como subrutina en Timsort ($n < 64$) y en introsort ($n < 16$).

---

## El algoritmo

Insertion sort mantiene una porción ordenada al inicio del array. En cada
paso, toma el siguiente elemento de la porción no ordenada y lo **inserta**
en la posición correcta dentro de la porción ordenada, desplazando
elementos mayores una posición a la derecha.

Es el mismo procedimiento que usa la mayoría de la gente al ordenar una
mano de cartas: tomar una carta, buscar su lugar entre las ya ordenadas,
insertar.

### Invariante

Después de la iteración $i$, `arr[0..i]` está ordenado (pero no
necesariamente en sus posiciones finales — pueden moverse cuando se
insertan elementos posteriores más pequeños).

### Pseudocódigo

```
INSERTION_SORT(arr, n):
    para i de 1 a n-1:
        key = arr[i]
        j = i - 1
        mientras j >= 0 y arr[j] > key:
            arr[j+1] = arr[j]      // shift a la derecha
            j--
        arr[j+1] = key             // insertar en posicion correcta
```

---

## Traza detallada

Array: `[5, 2, 4, 6, 1, 3]`.

### i=1: insertar 2

```
Porción ordenada: [5]    Key: 2
j=0: 5 > 2? sí → shift: [5, 5, 4, 6, 1, 3]
j=-1: fin del loop
Insertar key en j+1=0: [2, 5, 4, 6, 1, 3]
                        ↑────↑ ordenado
```

### i=2: insertar 4

```
Porción ordenada: [2, 5]    Key: 4
j=1: 5 > 4? sí → shift: [2, 5, 5, 6, 1, 3]
j=0: 2 > 4? no → break
Insertar key en j+1=1: [2, 4, 5, 6, 1, 3]
                        ↑────────↑ ordenado
```

### i=3: insertar 6

```
Porción ordenada: [2, 4, 5]    Key: 6
j=2: 5 > 6? no → break
Insertar key en j+1=3: [2, 4, 5, 6, 1, 3]  (sin cambio)
                        ↑───────────↑ ordenado
```

### i=4: insertar 1

```
Porción ordenada: [2, 4, 5, 6]    Key: 1
j=3: 6 > 1? sí → shift
j=2: 5 > 1? sí → shift
j=1: 4 > 1? sí → shift
j=0: 2 > 1? sí → shift → [2, 2, 4, 5, 6, 3]
j=-1: fin del loop
Insertar key en j+1=0: [1, 2, 4, 5, 6, 3]
                        ↑──────────────↑ ordenado
```

### i=5: insertar 3

```
Porción ordenada: [1, 2, 4, 5, 6]    Key: 3
j=4: 6 > 3? sí → shift
j=3: 5 > 3? sí → shift
j=2: 4 > 3? sí → shift
j=1: 2 > 3? no → break
Insertar key en j+1=2: [1, 2, 3, 4, 5, 6]
                        ↑─────────────────↑ ordenado
```

Resultado: **`[1, 2, 3, 4, 5, 6]`**. 5 iteraciones, 11 shifts, 11
comparaciones.

---

## Implementación en C

### Versión básica

```c
#include <stdio.h>

void insertion_sort(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}
```

El `>` estricto (no `>=`) garantiza estabilidad: elementos iguales no se
mueven entre sí.

### Versión instrumentada

```c
typedef struct { long cmp; long shifts; } Stats;

void insertion_sort_stats(int arr[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0) {
            s->cmp++;
            if (arr[j] > key) {
                arr[j + 1] = arr[j];
                s->shifts++;
                j--;
            } else {
                break;
            }
        }
        arr[j + 1] = key;
    }
}
```

### Versión con búsqueda binaria

La posición de inserción se puede encontrar con búsqueda binaria en
$O(\log i)$ en lugar de $O(i)$ comparaciones:

```c
void insertion_sort_binary(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];

        // busqueda binaria en arr[0..i-1]
        int lo = 0, hi = i;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            if (arr[mid] > key)  // > estricto para estabilidad
                hi = mid;
            else
                lo = mid + 1;
        }

        // shift desde lo hasta i
        for (int j = i; j > lo; j--)
            arr[j] = arr[j - 1];
        arr[lo] = key;
    }
}
```

Esto reduce las comparaciones a $O(n \log n)$ pero los shifts siguen
siendo $O(n^2)$. En la práctica no es más rápido porque los shifts (no
las comparaciones) dominan el tiempo para ints.

La búsqueda binaria **sí** ayuda cuando las comparaciones son costosas
(e.g., comparar strings largos) pero los movimientos son baratos (e.g.,
mover punteros).

### Versión genérica

```c
#include <string.h>
#include <stdlib.h>

void insertion_sort_generic(void *base, int n, size_t size,
                            int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;
    char *key = malloc(size);

    for (int i = 1; i < n; i++) {
        memcpy(key, arr + i * size, size);
        int j = i - 1;

        while (j >= 0 && cmp(arr + j * size, key) > 0) {
            memcpy(arr + (j + 1) * size, arr + j * size, size);
            j--;
        }
        memcpy(arr + (j + 1) * size, key, size);
    }

    free(key);
}
```

---

## Implementación en Rust

```rust
fn insertion_sort<T: Ord>(arr: &mut [T]) {
    for i in 1..arr.len() {
        let mut j = i;
        while j > 0 && arr[j - 1] > arr[j] {
            arr.swap(j - 1, j);
            j -= 1;
        }
    }
}
```

Esta versión usa swaps en lugar de shifts porque Rust no permite tener
una copia temporal de `T` sin `Clone`. Es más idiomática en Rust, aunque
hace 3 escrituras por movimiento en lugar de 1.

### Versión con shifts (requiere unsafe o Clone)

```rust
fn insertion_sort_shift<T: Ord + Clone>(arr: &mut [T]) {
    for i in 1..arr.len() {
        let key = arr[i].clone();
        let mut j = i;
        while j > 0 && arr[j - 1] > key {
            arr[j] = arr[j - 1].clone();
            j -= 1;
        }
        arr[j] = key;
    }
}
```

Para tipos `Copy` (como `i32`), el compilador optimiza los `clone()` a
simples copias. Para tipos que no son `Copy` (como `String`), los clones
tienen costo — la versión con swaps puede ser preferible.

### Versión genérica con closure

```rust
fn insertion_sort_by<T, F>(arr: &mut [T], less: F)
where F: Fn(&T, &T) -> bool
{
    for i in 1..arr.len() {
        let mut j = i;
        while j > 0 && less(&arr[j], &arr[j - 1]) {
            arr.swap(j - 1, j);
            j -= 1;
        }
    }
}

fn main() {
    let mut v = vec![(3, "c"), (1, "a"), (2, "b"), (1, "d")];
    insertion_sort_by(&mut v, |a, b| a.0 < b.0);
    println!("{:?}", v);
    // [(1, "a"), (1, "d"), (2, "b"), (3, "c")]  — estable: "a" antes de "d"
}
```

---

## Análisis de complejidad

### Caso por caso

| Caso | Comparaciones | Shifts | Ejemplo |
|------|--------------|--------|---------|
| Mejor | $n - 1$ | 0 | Array ordenado |
| Peor | $n(n-1)/2$ | $n(n-1)/2$ | Array inverso |
| Promedio | $n(n-1)/4$ | $n(n-1)/4$ | Array aleatorio |

### Relación con inversiones

Una **inversión** es un par $(i, j)$ con $i < j$ y $arr[i] > arr[j]$.
Insertion sort hace exactamente **una comparación y un shift por cada
inversión**, más $n - 1$ comparaciones del loop externo.

$$T(n) = n - 1 + \text{inversiones}$$

- Array ordenado: 0 inversiones → $T = n - 1 = O(n)$.
- Array inverso: $n(n-1)/2$ inversiones → $T = n(n-1)/2 + n - 1 = O(n^2)$.
- Array aleatorio: $n(n-1)/4$ inversiones esperadas → $T = O(n^2)$.
- Array casi ordenado ($k$ inversiones): $T = O(n + k)$.

### Demostración: O(n) para datos ordenados

```
[1, 2, 3, 4, 5]

i=1: key=2, j=0. arr[0]=1 > 2? no → 0 shifts. 1 comparación.
i=2: key=3, j=1. arr[1]=2 > 3? no → 0 shifts. 1 comparación.
i=3: key=4, j=2. arr[2]=3 > 4? no → 0 shifts. 1 comparación.
i=4: key=5, j=3. arr[3]=4 > 5? no → 0 shifts. 1 comparación.

Total: 4 comparaciones, 0 shifts = O(n).
```

Cada iteración del loop externo hace exactamente 1 comparación y termina.

### Demostración: O(n²) para datos inversos

```
[5, 4, 3, 2, 1]

i=1: key=4. Shift 5. 1 shift, 1 cmp.           → [4, 5, 3, 2, 1]
i=2: key=3. Shift 5, 4. 2 shifts, 2 cmps.      → [3, 4, 5, 2, 1]
i=3: key=2. Shift 5, 4, 3. 3 shifts, 3 cmps.   → [2, 3, 4, 5, 1]
i=4: key=1. Shift 5, 4, 3, 2. 4 shifts, 4 cmps.→ [1, 2, 3, 4, 5]

Total: 1+2+3+4 = 10 shifts = n(n-1)/2.
```

---

## ¿Por qué insertion sort es el mejor cuadrático?

### vs Bubble sort

| Aspecto | Insertion sort | Bubble sort |
|---------|---------------|-------------|
| Escrituras por movimiento | 1 (shift) | 3 (swap) |
| Adaptividad | $O(n + k)$ exacto | $O(n + k)$ pero con "tortugas" |
| Mejor caso | $O(n)$ | $O(n)$ (con bandera) |
| Rendimiento práctico | ~2× más rápido | Referencia |
| Uso en algoritmos reales | Timsort, introsort | Ninguno |

### vs Selection sort

| Aspecto | Insertion sort | Selection sort |
|---------|---------------|----------------|
| Comparaciones (mejor) | $n - 1$ | $n(n-1)/2$ |
| Comparaciones (peor) | $n(n-1)/2$ | $n(n-1)/2$ |
| Shifts/swaps (peor) | $n(n-1)/2$ shifts | $n - 1$ swaps |
| Adaptativo | Sí | No |
| Estable | Sí | No |

Insertion sort gana en todo excepto número de swaps. Selection sort solo
es preferible cuando los movimientos de datos son extraordinariamente
costosos.

---

## Insertion sort como subrutina

### En Timsort

Timsort (Python, Java, Rust `sort`) usa insertion sort para runs
menores a un umbral (típicamente 32-64 elementos). La razón es que para
arrays pequeños, la constante de $O(n^2)$ es menor que la de $O(n \log n)$:

```
n = 32:
  Insertion sort: ~32² / 4 = 256 operaciones
  Merge sort:     ~32 × log₂(32) = 160 operaciones + overhead de merge

  Pero insertion sort tiene menor constante (sin recursión, sin
  alocación de buffer), así que en tiempo real es más rápido.
```

### En introsort/pdqsort

Introsort (C++ `std::sort`, Rust `sort_unstable`) empieza con quicksort.
Cuando el tamaño del subarray baja de un umbral (~16), cambia a insertion
sort:

```c
void introsort_impl(int arr[], int lo, int hi, int depth_limit) {
    if (hi - lo < 16) {
        insertion_sort(arr + lo, hi - lo + 1);  // subarray pequeno
        return;
    }
    if (depth_limit == 0) {
        heapsort(arr + lo, hi - lo + 1);        // demasiada recursion
        return;
    }
    int p = partition(arr, lo, hi);
    introsort_impl(arr, lo, p - 1, depth_limit - 1);
    introsort_impl(arr, p + 1, hi, depth_limit - 1);
}
```

### ¿Por qué funciona para subarrays pequeños?

Después de que quicksort ha particionado el array repetidamente, los
subarrays de tamaño $< 16$ están **casi en su lugar** — cada elemento
está a lo sumo a ~16 posiciones de su posición final. Insertion sort
es $O(n \times 16)$ = $O(n)$ para este caso.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { long cmp; long shifts; } Stats;

// ======== Variantes ========

void isort_basic(int arr[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0) {
            s->cmp++;
            if (arr[j] > key) { arr[j+1] = arr[j]; s->shifts++; j--; }
            else break;
        }
        arr[j+1] = key;
    }
}

void isort_binary(int arr[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int lo = 0, hi = i;
        while (lo < hi) {
            s->cmp++;
            int mid = lo + (hi - lo) / 2;
            if (arr[mid] > key) hi = mid;
            else lo = mid + 1;
        }
        for (int j = i; j > lo; j--) {
            arr[j] = arr[j-1];
            s->shifts++;
        }
        arr[lo] = key;
    }
}

// ======== Generadores ========

void gen_random(int a[], int n)  { for (int i = 0; i < n; i++) a[i] = rand(); }
void gen_sorted(int a[], int n)  { for (int i = 0; i < n; i++) a[i] = i; }
void gen_reverse(int a[], int n) { for (int i = 0; i < n; i++) a[i] = n - i; }
void gen_nearly(int a[], int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    for (int i = 0; i < n/100; i++) {
        int a_ = rand()%n, b_ = rand()%n;
        int t = a[a_]; a[a_] = a[b_]; a[b_] = t;
    }
}
void gen_few_swaps(int a[], int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    // solo 5 pares fuera de lugar
    for (int i = 0; i < 5; i++) {
        int x = rand()%n, y = rand()%n;
        int t = a[x]; a[x] = a[y]; a[y] = t;
    }
}

// ======== Benchmark ========

typedef void (*ISortFn)(int[], int, Stats*);

void bench(const char *sort_name, ISortFn fn,
           const char *input_name, int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    Stats s = {0, 0};

    clock_t t0 = clock();
    fn(arr, n, &s);
    double t = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-12s %-14s %8.5fs  cmp=%-10ld shifts=%-10ld\n",
           sort_name, input_name, t, s.cmp, s.shifts);
    free(arr);
}

int main(void) {
    srand(42);
    int n = 10000;

    printf("=== Insertion sort (n=%d) ===\n\n", n);

    struct { const char *name; void (*gen)(int[], int); } inputs[] = {
        {"random",    gen_random},
        {"sorted",    gen_sorted},
        {"reverse",   gen_reverse},
        {"nearly",    gen_nearly},
        {"few_swaps", gen_few_swaps},
    };

    for (int i = 0; i < 5; i++) {
        int *arr = malloc(n * sizeof(int));
        inputs[i].gen(arr, n);
        bench("linear",  isort_basic,  inputs[i].name, arr, n);
        bench("binary",  isort_binary, inputs[i].name, arr, n);
        free(arr);
    }

    printf("\n=== Escalamiento para datos casi ordenados ===\n\n");
    for (int sz = 1000; sz <= 100000; sz *= 10) {
        int *arr = malloc(sz * sizeof(int));
        gen_few_swaps(arr, sz);
        Stats s = {0, 0};
        int *copy = malloc(sz * sizeof(int));
        memcpy(copy, arr, sz * sizeof(int));

        clock_t t0 = clock();
        isort_basic(copy, sz, &s);
        double t = (double)(clock() - t0) / CLOCKS_PER_SEC;

        printf("  n=%6d: %8.5fs  cmp=%-8ld shifts=%-6ld  (%.1f cmp/n)\n",
               sz, t, s.cmp, s.shifts, (double)s.cmp / sz);
        free(arr); free(copy);
    }

    return 0;
}
```

### Salida esperada

```
=== Insertion sort (n=10000) ===

  linear       random         0.02000s  cmp=25000000   shifts=24990000
  binary       random         0.01800s  cmp=120000     shifts=24990000
  linear       sorted         0.00002s  cmp=9999       shifts=0
  binary       sorted         0.00005s  cmp=30000      shifts=0
  linear       reverse        0.04000s  cmp=49995000   shifts=49995000
  binary       reverse        0.03500s  cmp=130000     shifts=49995000
  linear       nearly         0.00100s  cmp=110000     shifts=100000
  binary       nearly         0.00100s  cmp=45000      shifts=100000
  linear       few_swaps      0.00003s  cmp=10010      shifts=10
  binary       few_swaps      0.00005s  cmp=30000      shifts=10

=== Escalamiento para datos casi ordenados ===

  n=  1000:  0.00000s  cmp=1005     shifts=5      (1.0 cmp/n)
  n= 10000:  0.00003s  cmp=10010    shifts=10     (1.0 cmp/n)
  n=100000:  0.00020s  cmp=100010   shifts=10     (1.0 cmp/n)
```

Observaciones:

- **binary + random**: reduce comparaciones de 25M a 120K (208×) pero los
  shifts siguen iguales → solo ~10% más rápido.
- **linear + sorted**: 9999 comparaciones, 0 shifts — $O(n)$ confirmado.
- **binary + sorted**: 30K comparaciones — paradójicamente más lento que
  linear para datos ordenados, porque la búsqueda binaria siempre hace
  $\lceil \log_2 i \rceil$ comparaciones, mientras la búsqueda lineal
  hace solo 1.
- **few_swaps**: `cmp/n = 1.0` → exactamente $n + k$ operaciones para
  $k = 5$ inversiones. Escala linealmente incluso para $n = 10^5$.

---

## Ejercicios

### Ejercicio 1 — Traza manual

Aplica insertion sort a `[7, 3, 5, 2, 4]`. Muestra el array después de
cada inserción. ¿Cuántos shifts en total?

<details>
<summary>Verificar</summary>

**i=1**: key=3. Shift 7. `[3, 7, 5, 2, 4]`. 1 shift.

**i=2**: key=5. Shift 7. `[3, 5, 7, 2, 4]`. 1 shift.

**i=3**: key=2. Shift 7, 5, 3. `[2, 3, 5, 7, 4]`. 3 shifts.

**i=4**: key=4. Shift 7, 5. `[2, 3, 4, 5, 7]`. 2 shifts.

Total: **7 shifts**, 9 comparaciones.

El array tiene 7 inversiones: (7,3), (7,5), (7,2), (7,4), (3,2), (5,2),
(5,4). Shifts = inversiones = 7. Confirmado.
</details>

### Ejercicio 2 — Contar inversiones

Verifica que el número de shifts de insertion sort para `[4, 3, 1, 5, 2]`
es exactamente igual al número de inversiones.

<details>
<summary>Verificar</summary>

**Inversiones** de `[4, 3, 1, 5, 2]`:
- (4,3), (4,1), (4,2): 3
- (3,1), (3,2): 2
- (5,2): 1

Total: **6 inversiones**.

**Insertion sort**:
- i=1: key=3. 4>3 → shift. 1 shift. `[3, 4, 1, 5, 2]`.
- i=2: key=1. 4>1 → shift, 3>1 → shift. 2 shifts. `[1, 3, 4, 5, 2]`.
- i=3: key=5. 4>5? no. 0 shifts. `[1, 3, 4, 5, 2]`.
- i=4: key=2. 5>2 → shift, 4>2 → shift, 3>2 → shift. 3 shifts. `[1, 2, 3, 4, 5]`.

Total: 1+2+0+3 = **6 shifts** = 6 inversiones. Confirmado.
</details>

### Ejercicio 3 — Búsqueda binaria vs lineal

¿Para qué tipo de datos la versión con búsqueda binaria es genuinamente
más rápida? Explica con un ejemplo.

<details>
<summary>Verificar</summary>

La búsqueda binaria ayuda cuando **las comparaciones son caras pero los
movimientos son baratos**:

```c
// Ordenar array de punteros a strings largos
const char *strings[] = {
    "zzzzzzzz... (1000 chars)",
    "aaaaaaaa... (1000 chars)",
    // ...
};
```

- **Comparación**: `strcmp` recorre hasta 1000 bytes → costosa.
- **Movimiento**: mover un `const char *` (8 bytes) → barata.

Con búsqueda lineal: $O(n^2)$ comparaciones de strings largos.
Con búsqueda binaria: $O(n \log n)$ comparaciones de strings largos.
Los shifts siguen siendo $O(n^2)$ pero mueven punteros de 8 bytes.

Para `int[]`, ambas versiones mueven 4 bytes y comparan 4 bytes — la
comparación es tan barata como el movimiento, así que la búsqueda binaria
no ayuda (y puede ser peor por el overhead de los cálculos de `mid`).
</details>

### Ejercicio 4 — Estabilidad

Demuestra que insertion sort es estable. ¿Qué pasaría si cambiamos `>`
por `>=` en la comparación?

<details>
<summary>Verificar</summary>

**Con `>`** (estable): cuando `arr[j] == key`, la condición `arr[j] > key`
es falsa → break. El key se inserta **después** de los elementos iguales.
Esto preserva el orden original: si A y B tienen la misma clave y A estaba
antes, A permanece antes.

```
[A(3), B(3), C(1)]

i=2: key=C(1). Shift B(3), A(3). → [C(1), A(3), B(3)]
A sigue antes que B — estable.
```

**Con `>=`** (no estable): cuando `arr[j] == key`, la condición
`arr[j] >= key` es verdadera → shift. El key se inserta **antes** de los
elementos iguales, invirtiendo su orden.

```
[A(3), B(3)]

i=1: key=B(3). arr[0]=A(3) >= B(3)? sí → shift. → [B(3), A(3)]
B ahora está antes que A — inestable.
```
</details>

### Ejercicio 5 — Insertion sort para subarrays

Implementa `insertion_sort_range(arr, lo, hi)` que ordene `arr[lo..hi]`
(inclusive). Esto es lo que Timsort e introsort usan como subrutina.

<details>
<summary>Verificar</summary>

```c
void insertion_sort_range(int arr[], int lo, int hi) {
    for (int i = lo + 1; i <= hi; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= lo && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}
```

En Rust:

```rust
fn insertion_sort_range<T: Ord>(arr: &mut [T], lo: usize, hi: usize) {
    for i in (lo + 1)..=hi {
        let mut j = i;
        while j > lo && arr[j - 1] > arr[j] {
            arr.swap(j - 1, j);
            j -= 1;
        }
    }
}
```

O más idiomático, usando slices:

```rust
fn insertion_sort_slice<T: Ord>(arr: &mut [T]) {
    for i in 1..arr.len() {
        let mut j = i;
        while j > 0 && arr[j - 1] > arr[j] {
            arr.swap(j - 1, j);
            j -= 1;
        }
    }
}

// Uso: insertion_sort_slice(&mut arr[lo..=hi]);
```
</details>

### Ejercicio 6 — Benchmark: insertion sort vs qsort para n pequeño

Mide insertion sort vs `qsort` de stdlib para $n = 8, 16, 32, 64$. ¿A
partir de qué $n$ gana `qsort`?

<details>
<summary>Verificar</summary>

```c
#include <time.h>

int cmp_int(const void *a, const void *b) {
    return *(int *)a - *(int *)b;
}

int main(void) {
    srand(42);
    int repeats = 1000000;

    printf("%-6s %-12s %-12s\n", "n", "insertion", "qsort");
    for (int n = 8; n <= 64; n *= 2) {
        int *arr = malloc(n * sizeof(int));
        int *copy = malloc(n * sizeof(int));

        // generar datos aleatorios
        for (int i = 0; i < n; i++) arr[i] = rand() % 1000;

        clock_t t0 = clock();
        for (int r = 0; r < repeats; r++) {
            memcpy(copy, arr, n * sizeof(int));
            insertion_sort(copy, n);
        }
        double t_ins = (double)(clock()-t0)/CLOCKS_PER_SEC;

        t0 = clock();
        for (int r = 0; r < repeats; r++) {
            memcpy(copy, arr, n * sizeof(int));
            qsort(copy, n, sizeof(int), cmp_int);
        }
        double t_qs = (double)(clock()-t0)/CLOCKS_PER_SEC;

        printf("n=%-4d %-12.4fs %-12.4fs  ratio=%.2f\n",
               n, t_ins, t_qs, t_qs / t_ins);
        free(arr); free(copy);
    }
}
```

Resultado típico:

```
n      insertion    qsort
n=8    0.0800s      0.1500s       ratio=1.88
n=16   0.1200s      0.2200s       ratio=1.83
n=32   0.2500s      0.3500s       ratio=1.40
n=64   0.8000s      0.6000s       ratio=0.75
```

Insertion sort gana para $n \leq 32$ (1.4-1.9× más rápido). `qsort`
empieza a ganar alrededor de $n = 50$-$64$.

Esto coincide con los umbrales usados en la práctica: Timsort usa 32-64,
introsort usa 16-24.
</details>

### Ejercicio 7 — Sentinel optimization

La versión básica tiene dos condiciones en el while: `j >= 0` y
`arr[j] > key`. Elimina la primera usando un **centinela**: colocar el
mínimo del array en posición 0 antes de empezar.

<details>
<summary>Verificar</summary>

```c
void insertion_sort_sentinel(int arr[], int n) {
    // fase 1: mover el minimo a posicion 0
    int min_idx = 0;
    for (int i = 1; i < n; i++)
        if (arr[i] < arr[min_idx]) min_idx = i;
    int tmp = arr[0]; arr[0] = arr[min_idx]; arr[min_idx] = tmp;

    // fase 2: insertion sort sin bound check
    for (int i = 2; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (arr[j] > key) {  // sin j >= 0: arr[0] es el minimo, actua de centinela
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}
```

El centinela `arr[0]` garantiza que `arr[j] > key` se vuelve falso antes
de que `j` sea negativo (porque `arr[0]` es el mínimo global → nunca es
mayor que `key`).

Esto elimina una comparación del loop interno (la más ejecutada del
algoritmo). En la práctica, mejora ~5-15% para datos aleatorios. Es la
optimización que usa la implementación de insertion sort en glibc.

Nota: esta optimización **rompe la estabilidad** del primer elemento (el
mínimo original no queda en su posición relativa). Para mantener
estabilidad, se puede usar un shift en lugar de swap para mover el mínimo.
</details>

### Ejercicio 8 — Insertion sort en Rust: sort vs sort_unstable para n pequeño

Mide `sort`, `sort_unstable` e insertion sort manual para $n = 10$ en Rust.
¿Cuál es más rápido?

<details>
<summary>Verificar</summary>

```rust
use std::time::Instant;

fn isort(arr: &mut [i32]) {
    for i in 1..arr.len() {
        let mut j = i;
        while j > 0 && arr[j-1] > arr[j] { arr.swap(j-1, j); j -= 1; }
    }
}

fn main() {
    let data: Vec<i32> = vec![9, 3, 7, 1, 5, 8, 2, 6, 4, 0];
    let reps = 10_000_000u32;

    let t = Instant::now();
    for _ in 0..reps { let mut a = data.clone(); isort(&mut a); }
    println!("insertion:     {:?}", t.elapsed() / reps);

    let t = Instant::now();
    for _ in 0..reps { let mut a = data.clone(); a.sort(); }
    println!("sort:          {:?}", t.elapsed() / reps);

    let t = Instant::now();
    for _ in 0..reps { let mut a = data.clone(); a.sort_unstable(); }
    println!("sort_unstable: {:?}", t.elapsed() / reps);
}
```

```
insertion:     25ns
sort:          35ns
sort_unstable: 30ns
```

Insertion sort manual gana para $n = 10$ porque no tiene el overhead de
detección de runs (Timsort) ni de selección de pivote (pdqsort). Para
arrays de 10 elementos, esa maquinaria inicial es más costosa que los
~25 shifts que insertion sort hace en promedio.
</details>

### Ejercicio 9 — Adaptividad empírica

Genera arrays con exactamente $k$ inversiones para $k = 0, 10, 100, 1000$
y $n = 10000$. Verifica que el tiempo de insertion sort crece linealmente
con $k$.

<details>
<summary>Verificar</summary>

```c
void gen_k_inversions(int arr[], int n, int k) {
    // empezar con array ordenado
    for (int i = 0; i < n; i++) arr[i] = i;
    // crear exactamente k inversiones intercambiando pares adyacentes
    // (cada swap adyacente crea exactamente 1 inversion)
    srand(42);
    for (int i = 0; i < k; i++) {
        int pos = rand() % (n - 1);
        if (arr[pos] < arr[pos + 1]) {  // solo si crea una inversion
            int t = arr[pos]; arr[pos] = arr[pos+1]; arr[pos+1] = t;
        } else {
            i--;  // reintentar
        }
    }
}

int main(void) {
    int n = 10000;
    for (int k = 0; k <= 10000; k = (k == 0 ? 10 : k * 10)) {
        int *arr = malloc(n * sizeof(int));
        gen_k_inversions(arr, n, k);

        Stats s = {0, 0};
        clock_t t0 = clock();
        isort_basic(arr, n, &s);
        double t = (double)(clock()-t0)/CLOCKS_PER_SEC;

        printf("k=%5d: %8.5fs  shifts=%-8ld  cmp=%-8ld  cmp/n=%.1f\n",
               k, t, s.shifts, s.cmp, (double)s.cmp / n);
        free(arr);
    }
}
```

```
k=    0: 0.00002s  shifts=0        cmp=9999      cmp/n=1.0
k=   10: 0.00002s  shifts=10       cmp=10009     cmp/n=1.0
k=  100: 0.00003s  shifts=100      cmp=10099     cmp/n=1.0
k= 1000: 0.00010s  shifts=1000     cmp=10999     cmp/n=1.1
k=10000: 0.00080s  shifts=10000    cmp=19999     cmp/n=2.0
```

`cmp` $\approx n + k$ para todos los valores de $k$, confirmando la
complejidad $O(n + k)$. El tiempo crece linealmente con $k$.
</details>

### Ejercicio 10 — Insertion sort vs merge sort: punto de cruce

Determina empíricamente el $n$ donde merge sort empieza a ser más rápido
que insertion sort para datos aleatorios.

<details>
<summary>Verificar</summary>

```c
// merge sort simple
void merge(int a[], int t[], int l, int m, int r) {
    int i = l, j = m+1, k = l;
    while (i <= m && j <= r) { if (a[i] <= a[j]) t[k++] = a[i++]; else t[k++] = a[j++]; }
    while (i <= m) t[k++] = a[i++];
    while (j <= r) t[k++] = a[j++];
    for (int x = l; x <= r; x++) a[x] = t[x];
}
void msort(int a[], int t[], int l, int r) {
    if (l >= r) return;
    int m = l+(r-l)/2;
    msort(a, t, l, m); msort(a, t, m+1, r);
    merge(a, t, l, m, r);
}

int main(void) {
    srand(42);
    int repeats = 100000;

    for (int n = 10; n <= 200; n += 10) {
        int *arr = malloc(n * sizeof(int));
        int *tmp = malloc(n * sizeof(int));
        int *copy = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();

        clock_t t0 = clock();
        for (int r = 0; r < repeats; r++) {
            memcpy(copy, arr, n*sizeof(int));
            insertion_sort(copy, n);
        }
        double t_ins = (double)(clock()-t0)/CLOCKS_PER_SEC;

        t0 = clock();
        for (int r = 0; r < repeats; r++) {
            memcpy(copy, arr, n*sizeof(int));
            msort(copy, tmp, 0, n-1);
        }
        double t_ms = (double)(clock()-t0)/CLOCKS_PER_SEC;

        printf("n=%3d: insertion=%.4f  merge=%.4f  %s\n",
               n, t_ins, t_ms, t_ins < t_ms ? "INS" : "MERGE");
        free(arr); free(tmp); free(copy);
    }
}
```

```
n= 10: insertion=0.0050  merge=0.0090  INS
n= 20: insertion=0.0120  merge=0.0140  INS
n= 30: insertion=0.0250  merge=0.0180  MERGE
n= 40: insertion=0.0400  merge=0.0220  MERGE
n= 50: insertion=0.0600  merge=0.0260  MERGE
```

El punto de cruce está alrededor de $n \approx 25$-$35$. Esto depende
del hardware y del compilador, pero coincide con los umbrales empíricos
usados en Timsort (32) e introsort (16-24).

Para $n < 30$, la menor constante de insertion sort (sin recursión, sin
alocación de buffer) compensa su peor complejidad asintótica.
</details>
