# Medir rendimiento de algoritmos de ordenamiento

## Objetivo

Aprender a medir y comparar algoritmos de ordenamiento usando métricas
concretas:

- **Comparaciones** y **swaps**: métricas teóricas independientes del hardware.
- **Tiempo de ejecución**: `clock()` en C, `Instant` en Rust.
- **Espacio auxiliar**: memoria extra utilizada.
- **Cache-friendliness**: por qué el acceso secuencial gana al aleatorio.

Construiremos un **framework de benchmarking** reutilizable que permita
comparar cualquier par de algoritmos bajo condiciones controladas.

---

## Métricas teóricas: comparaciones y swaps

Contar comparaciones y swaps permite analizar el comportamiento de un
algoritmo independientemente del hardware. Dos máquinas distintas darán
tiempos diferentes, pero el mismo número de comparaciones.

### Instrumentar un algoritmo

La técnica es agregar un contador global que se incrementa en cada
comparación y cada swap:

```c
#include <stdio.h>

typedef struct {
    long comparisons;
    long swaps;
} Stats;

void insertion_sort(int arr[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0) {
            s->comparisons++;
            if (arr[j] > key) {
                arr[j + 1] = arr[j];
                s->swaps++;
                j--;
            } else {
                break;
            }
        }
        arr[j + 1] = key;
    }
}

void selection_sort(int arr[], int n, Stats *s) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            s->comparisons++;
            if (arr[j] < arr[min_idx])
                min_idx = j;
        }
        if (min_idx != i) {
            int tmp = arr[i]; arr[i] = arr[min_idx]; arr[min_idx] = tmp;
            s->swaps++;
        }
    }
}

void bubble_sort(int arr[], int n, Stats *s) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            s->comparisons++;
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j]; arr[j] = arr[j + 1]; arr[j + 1] = tmp;
                s->swaps++;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}
```

### Qué revela cada métrica

| Métrica | Qué mide | Quién gana |
|---------|----------|-----------|
| Comparaciones | Decisiones tomadas | Merge sort (~$n \log n$) |
| Swaps | Movimientos de datos | Selection sort ($O(n)$ swaps) |
| Comparaciones + swaps | Trabajo total | Depende del caso |

Selection sort siempre hace $O(n^2)$ comparaciones pero solo $O(n)$ swaps.
Si los swaps son caros (e.g., mover structs grandes), selection sort puede
ser preferible a bubble sort pese a tener la misma complejidad de
comparaciones.

---

## Medir tiempo en C: clock()

`clock()` mide el tiempo de CPU consumido por el proceso (no el tiempo de
pared). Es la forma estándar en C89/C99.

```c
#include <time.h>

double measure_sort(void (*sort_fn)(int[], int), int arr[], int n) {
    clock_t start = clock();
    sort_fn(arr, n);
    clock_t end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC;
}
```

### Problemas con clock()

1. **Resolución baja**: en muchos sistemas, `CLOCKS_PER_SEC = 1000000`
   (microsegundos). Para operaciones rápidas ($< 1$ ms), el resultado
   puede ser 0.
2. **Ruido**: otros procesos, interrupciones, y el garbage collector del
   OS afectan la medición.
3. **Optimización del compilador**: el compilador puede eliminar código
   que no tiene efecto observable.

### Soluciones

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 1. Repetir la medicion multiples veces
double measure_avg(void (*sort_fn)(int[], int),
                   int original[], int n, int repeats) {
    int *arr = malloc(n * sizeof(int));
    clock_t total = 0;

    for (int r = 0; r < repeats; r++) {
        memcpy(arr, original, n * sizeof(int));  // restaurar datos
        clock_t start = clock();
        sort_fn(arr, n);
        total += clock() - start;
    }

    free(arr);
    return (double)total / CLOCKS_PER_SEC / repeats;
}

// 2. Usar el resultado para evitar optimizacion
volatile int sink;  // el compilador no puede eliminar escrituras a volatile

void use_result(int arr[], int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) sum += arr[i];
    sink = sum;
}
```

### clock() vs time() vs clock_gettime()

| Función | Mide | Resolución | Portabilidad |
|---------|------|-----------|-------------|
| `clock()` | CPU time | ~1 µs | C estándar |
| `time()` | Wall time | 1 s | C estándar |
| `clock_gettime()` | Wall o CPU | ~1 ns | POSIX |
| `gettimeofday()` | Wall time | ~1 µs | POSIX (obsoleto) |

Para benchmarks de sorting, `clock()` es suficiente si $n \geq 1000$.
Para mediciones de alta precisión, usar `clock_gettime(CLOCK_MONOTONIC, ...)`.

---

## Medir tiempo en Rust: Instant

`std::time::Instant` es monotónico (nunca retrocede) y tiene resolución
de nanosegundos en la mayoría de plataformas.

```rust
use std::time::Instant;

fn measure_sort<F: FnMut(&mut [i32])>(mut sort_fn: F, data: &[i32], repeats: u32) -> f64 {
    let mut total = std::time::Duration::ZERO;

    for _ in 0..repeats {
        let mut arr = data.to_vec();
        let start = Instant::now();
        sort_fn(&mut arr);
        total += start.elapsed();
    }

    total.as_secs_f64() / repeats as f64
}

fn insertion_sort(arr: &mut [i32]) {
    for i in 1..arr.len() {
        let key = arr[i];
        let mut j = i;
        while j > 0 && arr[j - 1] > key {
            arr[j] = arr[j - 1];
            j -= 1;
        }
        arr[j] = key;
    }
}

fn main() {
    let data: Vec<i32> = (0..10000).map(|i| (i * 7 + 13) % 10000).collect();

    let t = measure_sort(insertion_sort, &data, 10);
    println!("Insertion sort (n=10000): {:.4}s", t);

    let t = measure_sort(|arr| arr.sort_unstable(), &data, 10);
    println!("sort_unstable (n=10000):  {:.4}s", t);
}
```

### Ventajas de Instant sobre clock()

- **Monotónico**: no se ve afectado por cambios de reloj del sistema.
- **Resolución**: nanosegundos vs microsegundos.
- **Tipo seguro**: `Duration` evita errores de conversión.
- **No desborda**: `Duration` es 64 bits internamente.

---

## Cache-friendliness

La jerarquía de caché tiene un impacto enorme en el rendimiento real de
los algoritmos de ordenamiento — a menudo mayor que el número de
comparaciones.

### La jerarquía de memoria

| Nivel | Tamaño típico | Latencia | Ancho de banda |
|-------|--------------|----------|---------------|
| Registros | ~1 KB | ~0.3 ns | — |
| L1 cache | 32-64 KB | ~1 ns | ~100 GB/s |
| L2 cache | 256 KB-1 MB | ~4 ns | ~50 GB/s |
| L3 cache | 4-32 MB | ~10 ns | ~30 GB/s |
| RAM | 8-64 GB | ~60 ns | ~20 GB/s |

Un cache miss a RAM cuesta ~60× más que un hit en L1. Los algoritmos que
acceden a la memoria secuencialmente generan pocos cache misses; los que
saltan aleatoriamente generan muchos.

### Patrón de acceso por algoritmo

| Algoritmo | Patrón de acceso | Cache-friendliness |
|-----------|------------------|-------------------|
| Insertion sort | Secuencial (shift de vecinos) | Excelente |
| Merge sort | Secuencial (merge de dos mitades) | Bueno |
| Quicksort | Secuencial (partición) | Excelente |
| Heapsort | Saltos padre↔hijo (exponenciales) | Pobre |
| Shell sort | Saltos por gap (decrecientes) | Moderado |

Heapsort accede a posiciones $i$, $2i+1$, $4i+3$, ... — los saltos crecen
exponencialmente, causando cache misses en cada nivel del sift-down. Esta
es la principal razón de que heapsort sea ~2× más lento que quicksort en
la práctica, pese a tener mejor complejidad teórica en el peor caso.

### Medir cache misses

En Linux, `perf stat` mide cache misses a nivel hardware:

```bash
perf stat -e cache-misses,cache-references ./sort_benchmark
```

Resultado típico para $n = 10^6$ enteros:

```
Quicksort:  cache-misses: 150,000   (0.3%)
Heapsort:   cache-misses: 2,500,000 (15.0%)
Merge sort: cache-misses: 800,000   (2.0%)
```

Heapsort tiene 17× más cache misses que quicksort — esto explica su
rendimiento inferior en la práctica.

---

## Tipos de entrada

El rendimiento de un algoritmo puede variar dramáticamente según el patrón
de los datos de entrada. Un benchmark completo debe probar múltiples
escenarios:

```c
typedef enum {
    INPUT_RANDOM,
    INPUT_SORTED,
    INPUT_REVERSE,
    INPUT_NEARLY_SORTED,
    INPUT_FEW_UNIQUE,
    INPUT_PIPE_ORGAN,     // 1,2,...,n,...,2,1
} InputType;

void generate_input(int arr[], int n, InputType type) {
    switch (type) {
        case INPUT_RANDOM:
            for (int i = 0; i < n; i++) arr[i] = rand();
            break;
        case INPUT_SORTED:
            for (int i = 0; i < n; i++) arr[i] = i;
            break;
        case INPUT_REVERSE:
            for (int i = 0; i < n; i++) arr[i] = n - i;
            break;
        case INPUT_NEARLY_SORTED:
            for (int i = 0; i < n; i++) arr[i] = i;
            // perturbar ~5% de los elementos
            for (int i = 0; i < n / 20; i++) {
                int a = rand() % n, b = rand() % n;
                int tmp = arr[a]; arr[a] = arr[b]; arr[b] = tmp;
            }
            break;
        case INPUT_FEW_UNIQUE:
            for (int i = 0; i < n; i++) arr[i] = rand() % 10;
            break;
        case INPUT_PIPE_ORGAN:
            for (int i = 0; i < n / 2; i++) arr[i] = i;
            for (int i = n / 2; i < n; i++) arr[i] = n - i;
            break;
    }
}
```

### Por qué importa cada tipo

| Tipo | Qué revela |
|------|-----------|
| Random | Caso promedio "honesto" |
| Sorted | Mejor caso de adaptativos, peor caso de quicksort con pivote fijo |
| Reverse | Peor caso de insertion sort y bubble sort |
| Nearly sorted | Ventaja de algoritmos adaptativos |
| Few unique | Estrés para quicksort (muchos iguales → partición degenerada) |
| Pipe organ | Estrés para detección de runs en Timsort |

---

## Framework de benchmarking completo

### En C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ======== Sorts instrumentados ========

typedef struct { long cmp; long swp; } Stats;

void isort(int a[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int k = a[i]; int j = i - 1;
        while (j >= 0) { s->cmp++; if (a[j] > k) { a[j+1] = a[j]; s->swp++; j--; } else break; }
        a[j+1] = k;
    }
}

void ssort(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++) {
        int m = i;
        for (int j = i+1; j < n; j++) { s->cmp++; if (a[j] < a[m]) m = j; }
        if (m != i) { int t = a[i]; a[i] = a[m]; a[m] = t; s->swp++; }
    }
}

void bsort(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++) {
        int sw = 0;
        for (int j = 0; j < n-1-i; j++) {
            s->cmp++;
            if (a[j] > a[j+1]) { int t = a[j]; a[j] = a[j+1]; a[j+1] = t; s->swp++; sw = 1; }
        }
        if (!sw) break;
    }
}

// merge sort
static long ms_cmp;
void ms_merge(int a[], int t[], int l, int m, int r) {
    int i = l, j = m+1, k = l;
    while (i <= m && j <= r) { ms_cmp++; if (a[i] <= a[j]) t[k++] = a[i++]; else t[k++] = a[j++]; }
    while (i <= m) t[k++] = a[i++];
    while (j <= r) t[k++] = a[j++];
    for (int x = l; x <= r; x++) a[x] = t[x];
}
void ms_rec(int a[], int t[], int l, int r) {
    if (l >= r) return;
    int m = l + (r-l)/2;
    ms_rec(a, t, l, m); ms_rec(a, t, m+1, r);
    ms_merge(a, t, l, m, r);
}

// quicksort
static long qs_cmp;
int qs_part(int a[], int lo, int hi) {
    int piv = a[lo + (hi-lo)/2];
    while (lo <= hi) {
        while (a[lo] < piv) { qs_cmp++; lo++; }
        while (a[hi] > piv) { qs_cmp++; hi--; }
        qs_cmp += 2;  // las dos condiciones que fallan
        if (lo <= hi) { int t = a[lo]; a[lo] = a[hi]; a[hi] = t; lo++; hi--; }
    }
    return lo;
}
void qs_rec(int a[], int lo, int hi) {
    if (lo >= hi) return;
    int p = qs_part(a, lo, hi);
    qs_rec(a, lo, p-1); qs_rec(a, p, hi);
}

// ======== Generadores ========

void gen_random(int a[], int n)  { for (int i = 0; i < n; i++) a[i] = rand(); }
void gen_sorted(int a[], int n)  { for (int i = 0; i < n; i++) a[i] = i; }
void gen_reverse(int a[], int n) { for (int i = 0; i < n; i++) a[i] = n - i; }
void gen_nearly(int a[], int n)  {
    for (int i = 0; i < n; i++) a[i] = i;
    for (int i = 0; i < n/20; i++) { int x = rand()%n, y = rand()%n; int t=a[x]; a[x]=a[y]; a[y]=t; }
}

// ======== Benchmark ========

typedef void (*GenFunc)(int[], int);
typedef void (*SortStats)(int[], int, Stats*);

void bench_quadratic(const char *name, SortStats fn, int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    Stats s = {0, 0};

    clock_t t0 = clock();
    fn(arr, n, &s);
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-15s %8.4fs  cmp=%-10ld swp=%-10ld\n", name, elapsed, s.cmp, s.swp);
    free(arr);
}

void bench_merge(int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    int *tmp = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    ms_cmp = 0;

    clock_t t0 = clock();
    ms_rec(arr, tmp, 0, n - 1);
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-15s %8.4fs  cmp=%-10ld\n", "merge sort", elapsed, ms_cmp);
    free(arr); free(tmp);
}

void bench_quick(int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    qs_cmp = 0;

    clock_t t0 = clock();
    qs_rec(arr, 0, n - 1);
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-15s %8.4fs  cmp=%-10ld\n", "quicksort", elapsed, qs_cmp);
    free(arr);
}

void run_benchmark(const char *input_name, GenFunc gen, int n) {
    int *orig = malloc(n * sizeof(int));
    gen(orig, n);

    printf("\n--- %s (n=%d) ---\n", input_name, n);

    if (n <= 50000) {
        bench_quadratic("insertion", isort, orig, n);
        bench_quadratic("selection", ssort, orig, n);
        bench_quadratic("bubble",    bsort, orig, n);
    }
    bench_merge(orig, n);
    bench_quick(orig, n);

    free(orig);
}

int main(void) {
    srand(42);

    int n = 10000;
    run_benchmark("Random",        gen_random,  n);
    run_benchmark("Sorted",        gen_sorted,  n);
    run_benchmark("Reverse",       gen_reverse, n);
    run_benchmark("Nearly sorted", gen_nearly,  n);

    printf("\n--- Escalamiento (random) ---\n");
    for (int sz = 1000; sz <= 100000; sz *= 10) {
        int *arr = malloc(sz * sizeof(int));
        gen_random(arr, sz);
        printf("\nn=%d:\n", sz);
        bench_merge(arr, sz);
        bench_quick(arr, sz);
        if (sz <= 10000) bench_quadratic("insertion", isort, arr, sz);
        free(arr);
    }

    return 0;
}
```

### Salida esperada

```
--- Random (n=10000) ---
  insertion       0.0200s  cmp=24950000   swp=24940000
  selection       0.0150s  cmp=49995000   swp=9990
  bubble          0.0500s  cmp=49900000   swp=24950000
  merge sort      0.0005s  cmp=120500
  quicksort       0.0004s  cmp=155000

--- Sorted (n=10000) ---
  insertion       0.0000s  cmp=9999       swp=0
  selection       0.0150s  cmp=49995000   swp=0
  bubble          0.0000s  cmp=9999       swp=0
  merge sort      0.0004s  cmp=69000
  quicksort       0.0003s  cmp=140000

--- Reverse (n=10000) ---
  insertion       0.0400s  cmp=49995000   swp=49995000
  selection       0.0150s  cmp=49995000   swp=5000
  bubble          0.0800s  cmp=49995000   swp=49995000
  merge sort      0.0005s  cmp=120500
  quicksort       0.0003s  cmp=140000

--- Nearly sorted (n=10000) ---
  insertion       0.0010s  cmp=500000     swp=490000
  selection       0.0150s  cmp=49995000   swp=9500
  bubble          0.0020s  cmp=1000000    swp=490000
  merge sort      0.0005s  cmp=110000
  quicksort       0.0003s  cmp=140000

--- Escalamiento (random) ---

n=1000:
  merge sort      0.0000s  cmp=8700
  quicksort       0.0000s  cmp=12000
  insertion       0.0002s  cmp=250000     swp=249000

n=10000:
  merge sort      0.0005s  cmp=120500
  quicksort       0.0004s  cmp=155000
  insertion       0.0200s  cmp=24950000   swp=24940000

n=100000:
  merge sort      0.0060s  cmp=1600000
  quicksort       0.0050s  cmp=2000000
```

### Observaciones clave

1. **Insertion sort + sorted**: $O(n)$ — 9999 comparaciones, 0 swaps.
   Adaptativo en acción.
2. **Selection sort**: siempre ~50M comparaciones pero solo ~10K swaps.
   Comparaciones constantes independientemente del input.
3. **Bubble sort + reverse**: el peor caso — 50M comparaciones Y 50M swaps.
   El peor algoritmo de los tres cuadráticos.
4. **Merge vs quick**: quicksort hace ~30% más comparaciones pero es igual
   o más rápido en tiempo. La ventaja de caché compensa.
5. **Escalamiento**: los cuadráticos escalan 100× al ir de 1K a 10K;
   merge y quick escalan ~13×. Confirmación de $O(n^2)$ vs $O(n \log n)$.

---

## Framework en Rust

```rust
use std::time::Instant;

// ======== Sorts instrumentados ========

struct Stats { cmp: u64, swp: u64 }

fn insertion_sort_stats(arr: &mut [i32]) -> Stats {
    let mut s = Stats { cmp: 0, swp: 0 };
    for i in 1..arr.len() {
        let key = arr[i];
        let mut j = i;
        while j > 0 {
            s.cmp += 1;
            if arr[j - 1] > key {
                arr[j] = arr[j - 1];
                s.swp += 1;
                j -= 1;
            } else {
                break;
            }
        }
        arr[j] = key;
    }
    s
}

// ======== Generadores ========

fn gen_random(n: usize) -> Vec<i32> {
    (0..n).map(|i| ((i as i32).wrapping_mul(1103515245).wrapping_add(12345)) % n as i32).collect()
}

fn gen_sorted(n: usize) -> Vec<i32> { (0..n as i32).collect() }
fn gen_reverse(n: usize) -> Vec<i32> { (0..n as i32).rev().collect() }

fn gen_nearly(n: usize) -> Vec<i32> {
    let mut v: Vec<i32> = (0..n as i32).collect();
    for i in (0..n).step_by(20) {
        let j = (i + 7) % n;
        v.swap(i, j);
    }
    v
}

// ======== Benchmark ========

fn bench<F: FnMut(&mut [i32])>(name: &str, mut f: F, data: &[i32], repeats: u32) {
    let mut total = std::time::Duration::ZERO;
    for _ in 0..repeats {
        let mut arr = data.to_vec();
        let start = Instant::now();
        f(&mut arr);
        total += start.elapsed();
    }
    let avg = total.as_secs_f64() / repeats as f64;
    println!("  {:<20} {:.6}s", name, avg);
}

fn main() {
    let n = 10_000;

    for (name, data) in [
        ("Random", gen_random(n)),
        ("Sorted", gen_sorted(n)),
        ("Reverse", gen_reverse(n)),
        ("Nearly sorted", gen_nearly(n)),
    ] {
        println!("\n--- {} (n={}) ---", name, n);

        // insertion sort con stats (una vez)
        let mut arr = data.clone();
        let s = insertion_sort_stats(&mut arr);
        println!("  insertion (stats): cmp={}, swp={}", s.cmp, s.swp);

        bench("sort (Timsort)", |a| a.sort(), &data, 10);
        bench("sort_unstable", |a| a.sort_unstable(), &data, 10);
    }

    println!("\n--- Escalamiento sort_unstable ---");
    for &sz in &[1_000, 10_000, 100_000, 1_000_000] {
        let data = gen_random(sz);
        bench(&format!("n={}", sz), |a| a.sort_unstable(), &data, 5);
    }
}
```

Salida típica:

```
--- Random (n=10000) ---
  insertion (stats): cmp=24832401, swp=24822402
  sort (Timsort)     0.000350s
  sort_unstable      0.000280s

--- Sorted (n=10000) ---
  insertion (stats): cmp=9999, swp=0
  sort (Timsort)     0.000015s
  sort_unstable      0.000020s

--- Reverse (n=10000) ---
  insertion (stats): cmp=49995000, swp=49995000
  sort (Timsort)     0.000020s
  sort_unstable      0.000025s

--- Nearly sorted (n=10000) ---
  insertion (stats): cmp=25400, swp=15400
  sort (Timsort)     0.000030s
  sort_unstable      0.000050s

--- Escalamiento sort_unstable ---
  n=1000               0.000025s
  n=10000              0.000280s
  n=100000             0.003500s
  n=1000000            0.042000s
```

Notar que `sort` (Timsort) es más rápido que `sort_unstable` para datos
ya ordenados y en reversa — Timsort es adaptativo y detecta runs.

---

## Errores comunes en benchmarking

### 1. No restaurar los datos entre ejecuciones

```c
// INCORRECTO: la segunda medicion ordena un array ya ordenado
sort_fn(arr, n);
// medir...
sort_fn(arr, n);  // arr ya esta ordenado — resultado invalido

// CORRECTO: copiar los datos originales antes de cada ejecucion
memcpy(arr, original, n * sizeof(int));
sort_fn(arr, n);
```

### 2. Optimización del compilador elimina el sort

```c
// INCORRECTO: el compilador puede ver que arr no se usa despues del sort
int arr[1000];
// ...
sort(arr, 1000);
// fin del programa — el compilador elimina sort()

// CORRECTO: usar el resultado
volatile int sink;
sort(arr, 1000);
sink = arr[0];  // fuerza que sort() no sea eliminado
```

### 3. Medir con n demasiado pequeño

Para $n < 100$, el overhead de `clock()` puede dominar el tiempo medido.
Reglas:

- $n < 100$: no medir tiempo, contar operaciones.
- $100 \leq n \leq 1000$: repetir al menos 1000 veces.
- $n > 10000$: una sola ejecución suele ser suficiente.

### 4. No compilar con optimización

```bash
# INCORRECTO: sin optimizacion, el codigo es 3-10x mas lento
gcc sort_bench.c -o bench

# CORRECTO: usar -O2 para benchmarks realistas
gcc -O2 sort_bench.c -o bench

# En Rust:
cargo run --release
```

Sin `-O2`, las comparaciones y el benchmarking no reflejan el rendimiento
real del algoritmo — reflejan el overhead del código no optimizado.

---

## Ejercicios

### Ejercicio 1 — Contar comparaciones y swaps

Instrumenta insertion sort, selection sort y bubble sort con contadores.
Ejecuta para $n = 100$ con datos aleatorios. ¿Cuál hace más comparaciones?
¿Cuál hace más swaps?

<details>
<summary>Verificar</summary>

Para $n = 100$ aleatorio (valores típicos):

| Algoritmo | Comparaciones | Swaps |
|-----------|--------------|-------|
| Insertion sort | ~2500 | ~2500 |
| Selection sort | 4950 | ~95 |
| Bubble sort | ~4900 | ~2500 |

- **Más comparaciones**: selection sort y bubble sort (~4950, siempre $n(n-1)/2$).
- **Más swaps**: insertion sort y bubble sort (~2500 cada uno).
- **Menos swaps**: selection sort (~95, solo $O(n)$ swaps).

Selection sort tiene comparaciones constantes pero pocos swaps. Si los
elementos son structs grandes (swap caro), selection sort puede ganar en
tiempo real pese a tener las mismas comparaciones que bubble sort.
</details>

### Ejercicio 2 — Medir con clock()

Mide el tiempo de insertion sort para $n = 1000, 5000, 10000$ con datos
aleatorios. Verifica que el tiempo crece cuadráticamente (el ratio de
tiempos debería ser $\sim 25$× de 1000 a 5000 y $\sim 100$× de 1000 a
10000).

<details>
<summary>Verificar</summary>

```c
int main(void) {
    srand(42);
    for (int n = 1000; n <= 10000; n += (n < 5000 ? 4000 : 5000)) {
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();

        clock_t start = clock();
        // repetir para precision
        int repeats = 100000 / n;
        int *copy = malloc(n * sizeof(int));
        for (int r = 0; r < repeats; r++) {
            memcpy(copy, arr, n * sizeof(int));
            isort(copy, n, &(Stats){0,0});
        }
        double t = (double)(clock()-start)/CLOCKS_PER_SEC/repeats;

        printf("n=%5d: %.6fs\n", n, t);
        free(arr); free(copy);
    }
}
```

```
n= 1000: 0.000200s
n= 5000: 0.005000s  (25x → n^2 confirmado)
n=10000: 0.020000s  (100x → n^2 confirmado)
```

El ratio 0.02/0.0002 = 100 = $(10000/1000)^2$ confirma $O(n^2)$.
</details>

### Ejercicio 3 — Instant en Rust

Mide `sort` vs `sort_unstable` para $n = 10^5$ con datos aleatorios,
ordenados y en reversa. ¿En qué caso `sort` gana?

<details>
<summary>Verificar</summary>

```rust
use std::time::Instant;

fn bench(name: &str, data: &[i32]) {
    let mut a = data.to_vec();
    let t1 = Instant::now();
    a.sort();
    let d1 = t1.elapsed();

    let mut a = data.to_vec();
    let t2 = Instant::now();
    a.sort_unstable();
    let d2 = t2.elapsed();

    println!("{:15} sort={:?}  unstable={:?}", name, d1, d2);
}

fn main() {
    let n = 100_000;
    let random: Vec<i32> = (0..n).map(|i| (i*7+13) % n).collect();
    let sorted: Vec<i32> = (0..n).collect();
    let reverse: Vec<i32> = (0..n).rev().collect();

    bench("Random", &random);
    bench("Sorted", &sorted);
    bench("Reverse", &reverse);
}
```

```
Random          sort=3.5ms   unstable=2.8ms
Sorted          sort=0.1ms   unstable=0.2ms
Reverse         sort=0.2ms   unstable=0.3ms
```

`sort` (Timsort) gana en **sorted** y **reverse** porque detecta runs y
los fusiona sin trabajo. `sort_unstable` (pdqsort) gana en **random**
porque no tiene el overhead de mantener estabilidad.
</details>

### Ejercicio 4 — Generador de inputs

Implementa el generador `INPUT_FEW_UNIQUE` (valores en rango $[0, 9]$) y
mide cómo afecta a quicksort vs merge sort.

<details>
<summary>Verificar</summary>

```c
void gen_few_unique(int a[], int n) {
    for (int i = 0; i < n; i++) a[i] = rand() % 10;
}
```

Con pocos valores únicos:
- **Quicksort con pivote fijo**: puede degenerar si el pivote se repite
  mucho. La partición pone todos los iguales de un lado, creando
  particiones desbalanceadas → $O(n^2)$.
- **Quicksort 3-way** (Dutch National Flag): maneja duplicados
  eficientemente. Los iguales al pivote se excluyen de ambas
  recursiones → $O(n)$ si todos son iguales.
- **Merge sort**: no se ve afectado — siempre $O(n \log n)$.

Este es el caso que motivó la partición de 3 vías y que pdqsort maneja
con detección de patrones.
</details>

### Ejercicio 5 — Cache experiment

Explica por qué quicksort es ~2× más rápido que heapsort para
$n = 10^6$ pese a que heapsort tiene peor caso garantizado $O(n \log n)$.

<details>
<summary>Verificar</summary>

La razón principal es la **localidad de caché**:

**Quicksort** — la partición recorre el array secuencialmente con dos
punteros (uno desde la izquierda, otro desde la derecha). Cada puntero
avanza una posición por paso. Esto genera un patrón de acceso
perfectamente secuencial → los datos se leen en líneas de caché completas
(64 bytes = 16 ints), y casi cada acceso es un hit.

**Heapsort** — sift-down accede a posiciones $i$, $2i+1$, $4i+3$, $8i+7$, ...
Los saltos se duplican en cada nivel. Para un heap de $10^6$ elementos
(altura 20), los últimos niveles saltan miles de posiciones, cada salto
a una línea de caché diferente → cache miss en cada comparación.

Para $n = 10^6$ ints (4 MB):
- L1 (64 KB): los últimos ~14 niveles de heapsort no caben en L1.
- L2 (256 KB): los últimos ~12 niveles no caben.
- Quicksort: las dos mitades de la partición caben en L2 después del
  primer nivel de recursión.

Resultado: heapsort tiene ~15% cache miss rate vs ~0.3% de quicksort.
Cada miss cuesta ~60 ns (acceso a RAM) vs ~1 ns (hit L1). Esto explica
el factor 2× de diferencia en tiempo real.
</details>

### Ejercicio 6 — Verificar escalamiento

Mide merge sort para $n = 10^3, 10^4, 10^5$. Calcula el ratio
$t(10n) / t(n)$ y verifica que se acerca a ~13 (porque
$10 \log 10 / 1 \log 1 \approx 10 \times 1.3 = 13$).

<details>
<summary>Verificar</summary>

```c
int main(void) {
    srand(42);
    double prev_t = 0;
    for (int n = 1000; n <= 100000; n *= 10) {
        int *arr = malloc(n * sizeof(int));
        int *tmp = malloc(n * sizeof(int));
        gen_random(arr, n);

        clock_t t0 = clock();
        int reps = 1000000 / n;
        for (int r = 0; r < reps; r++) {
            gen_random(arr, n);
            ms_rec(arr, tmp, 0, n-1);
        }
        double t = (double)(clock()-t0)/CLOCKS_PER_SEC/reps;

        printf("n=%6d: %.6fs", n, t);
        if (prev_t > 0) printf("  ratio=%.1f", t/prev_t);
        printf("\n");
        prev_t = t;
        free(arr); free(tmp);
    }
}
```

```
n=  1000: 0.000050s
n= 10000: 0.000600s  ratio=12.0
n=100000: 0.008000s  ratio=13.3
```

El ratio ~13× por cada factor 10 en $n$ confirma $O(n \log n)$: si fuera
$O(n^2)$, el ratio sería 100×; si fuera $O(n)$, sería 10×. El factor
$\log$ agrega ~30% extra por década.
</details>

### Ejercicio 7 — Benchmark en Rust con closures

Escribe un benchmark genérico en Rust que acepte cualquier sort como
closure y mida tiempo + verificación de correctitud.

<details>
<summary>Verificar</summary>

```rust
use std::time::Instant;

fn benchmark<F: FnMut(&mut [i32])>(
    name: &str,
    mut sort_fn: F,
    data: &[i32],
    repeats: u32,
) {
    let mut total = std::time::Duration::ZERO;

    for _ in 0..repeats {
        let mut arr = data.to_vec();
        let start = Instant::now();
        sort_fn(&mut arr);
        total += start.elapsed();

        // verificar correctitud
        for w in arr.windows(2) {
            assert!(w[0] <= w[1], "sort produced incorrect result");
        }
    }

    let avg = total.as_secs_f64() / repeats as f64;
    println!("  {:<25} {:.6}s", name, avg);
}

fn main() {
    let n = 50_000;
    let data: Vec<i32> = (0..n as i32).map(|i| (i * 31337) % n as i32).collect();

    println!("n = {}", n);
    benchmark("sort (Timsort)", |a| a.sort(), &data, 10);
    benchmark("sort_unstable (pdqsort)", |a| a.sort_unstable(), &data, 10);
    benchmark("sort_by(reverse)", |a| a.sort_by(|x, y| y.cmp(x)), &data, 10);
}
```

La clave es `assert!` después de cada sort — un benchmark que no verifica
correctitud puede medir un algoritmo incorrecto que es "rápido" porque no
hace nada.
</details>

### Ejercicio 8 — Comparar swaps: selection vs insertion

Para $n = 1000$, mide los swaps de selection sort vs insertion sort para
datos aleatorios. ¿Cuál hace menos? ¿Por qué?

<details>
<summary>Verificar</summary>

Resultados típicos:

| Algoritmo | Swaps (n=1000) |
|-----------|---------------|
| Selection sort | ~999 |
| Insertion sort | ~250000 |

Selection sort hace **exactamente** $n - 1$ swaps en el peor caso (un swap
por iteración del loop externo). Insertion sort hace un swap (shift) por
cada inversión: ~$n^2/4$ en promedio.

¿Por qué selection sort hace tan pocos? Porque cada swap coloca un elemento
en su posición **final** — no hay movimientos intermedios. Insertion sort
hace muchos movimientos pequeños para "abrir espacio" al insertar cada
elemento.

Si el swap es costoso (e.g., mover un struct de 1 KB), selection sort puede
ser 250× más rápido que insertion sort para $n = 1000$, pese a hacer las
mismas comparaciones.
</details>

### Ejercicio 9 — Datos casi ordenados

Genera un array de $n = 10000$ donde solo 10 elementos están fuera de
lugar. Mide insertion sort, merge sort y quicksort. ¿Cuál gana?

<details>
<summary>Verificar</summary>

```c
void gen_almost(int a[], int n) {
    for (int i = 0; i < n; i++) a[i] = i;
    // mover 10 elementos aleatorios
    for (int i = 0; i < 10; i++) {
        int from = rand() % n, to = rand() % n;
        int tmp = a[from]; a[from] = a[to]; a[to] = tmp;
    }
}
```

Resultados (n=10000):

| Algoritmo | Comparaciones | Tiempo |
|-----------|--------------|--------|
| Insertion sort | ~10020 | 0.00002s |
| Merge sort | ~120000 | 0.00050s |
| Quicksort | ~140000 | 0.00040s |

Insertion sort gana por un factor de 10-25×. Con solo 10 inversiones
($k = 10$), su complejidad es $O(n + k) = O(10010)$ vs $O(n \log n) \approx 130000$ de merge/quick.

Esta es la razón por la que Timsort e introsort usan insertion sort como
subrutina para subarrays pequeños o casi ordenados.
</details>

### Ejercicio 10 — Tabla completa

Ejecuta el framework completo para $n = 10000$ con los 4 tipos de input
(random, sorted, reverse, nearly). Produce una tabla con comparaciones y
tiempos para insertion, selection, bubble, merge y quicksort.

<details>
<summary>Verificar</summary>

| Input | Algoritmo | Comparaciones | Swaps | Tiempo |
|-------|-----------|--------------|-------|--------|
| **Random** | Insertion | ~25M | ~25M | 0.020s |
| | Selection | 50M | 10K | 0.015s |
| | Bubble | 50M | ~25M | 0.050s |
| | Merge | 120K | — | 0.0005s |
| | Quicksort | 155K | — | 0.0004s |
| **Sorted** | Insertion | 10K | 0 | 0.00002s |
| | Selection | 50M | 0 | 0.015s |
| | Bubble | 10K | 0 | 0.00002s |
| | Merge | 69K | — | 0.0004s |
| | Quicksort | 140K | — | 0.0003s |
| **Reverse** | Insertion | 50M | 50M | 0.040s |
| | Selection | 50M | 5K | 0.015s |
| | Bubble | 50M | 50M | 0.080s |
| | Merge | 120K | — | 0.0005s |
| | Quicksort | 140K | — | 0.0003s |
| **Nearly** | Insertion | 500K | 490K | 0.001s |
| | Selection | 50M | 10K | 0.015s |
| | Bubble | 1M | 490K | 0.002s |
| | Merge | 110K | — | 0.0005s |
| | Quicksort | 140K | — | 0.0003s |

Patrones:
- **Selection**: siempre 50M comparaciones, insensible al input.
- **Insertion + sorted**: mejor caso, 10K comparaciones — 5000× menos que random.
- **Bubble + reverse**: peor caso — 50M de todo.
- **Merge y quick**: siempre ~100-160K comparaciones, insensibles al input
  (merge levemente menos que quick).
- **Nearly sorted**: insertion y bubble se benefician enormemente; los demás no.
</details>
