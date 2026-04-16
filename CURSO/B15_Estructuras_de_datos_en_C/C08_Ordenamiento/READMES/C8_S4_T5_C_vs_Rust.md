# C vs Rust: ordenamiento en la práctica

## Objetivo

Confrontar directamente las interfaces de ordenamiento de C y Rust
para entender **por qué** Rust es consistentemente más rápido a pesar
de usar la misma familia de algoritmos:

- **`qsort`**: genericidad via `void *`, comparador como puntero a
  función — imposible de inlinear.
- **`.sort()` / `.sort_unstable()`**: genericidad via generics +
  monomorphization — comparador inlineado, código especializado por tipo.
- **Benchmark para $10^6$ elementos**: medición real de la diferencia.
- **Análisis de por qué**: indirección, inlining, tamaño de swap,
  pipeline del compilador.

---

## El problema fundamental: genericidad en tiempo de ejecución vs compilación

C y Rust resuelven el mismo problema — "ordenar un array de cualquier
tipo" — con estrategias opuestas:

| | C (`qsort`) | Rust (`.sort()`) |
|---|---|---|
| Mecanismo | Borrado de tipo (`void *`) | Generics (parámetro `T`) |
| Resolución | Runtime | Compilación |
| Código generado | Una sola copia para todos los tipos | Una copia por cada tipo `T` usado |
| Comparador | Puntero a función (indirecto) | Closure/trait (inlineado) |
| Cast | Manual, sin verificación | No necesario |
| Seguridad de tipos | Ninguna (UB silencioso) | Total (error de compilación) |

Esta diferencia de diseño tiene consecuencias directas en rendimiento.

---

## Anatomía de una comparación

### En C con qsort

Cuando `qsort` necesita comparar dos enteros, ocurre lo siguiente:

```
1. Calcular dirección:  ptr = (char *)base + i * size    (aritmética)
2. Preparar argumentos: cargar ptr_a y ptr_b en registros (mov)
3. Llamada indirecta:   call *compar                      (branch indirecto)
4. Frame setup:         push rbp; mov rbp, rsp            (prólogo)
5. Cast void *:         mov eax, [rdi]   ; *(int *)a      (load + interpretación)
6. Cast void *:         mov ecx, [rsi]   ; *(int *)b      (load + interpretación)
7. Comparar:            cmp eax, ecx                       (la comparación real)
8. Branch + return:     setl/setg, ret                     (resultado)
9. Frame teardown:      pop rbp                            (epílogo)
10. Usar resultado:     test eax, eax                      (interpretar retorno)
```

Son ~10 operaciones para una comparación que semánticamente es
`arr[i] < arr[j]`.

### En Rust con .sort()

Después de monomorphization e inlining:

```
1. Load:    mov eax, [rdi + i*4]    (acceso directo, tipo conocido)
2. Load:    mov ecx, [rdi + j*4]    (acceso directo)
3. Compare: cmp eax, ecx            (la comparación)
```

Son 3 operaciones. El compilador conoce el tipo, el tamaño, y la
operación de comparación. Todo está inlineado.

### Impacto cuantitativo

Para $n = 10^6$ elementos con $\sim n \log_2 n \approx 20 \times 10^6$
comparaciones:

- **C**: 20M comparaciones × ~10 operaciones = ~200M operaciones
  de comparación.
- **Rust**: 20M comparaciones × ~3 operaciones = ~60M operaciones.

La comparación es solo una parte del trabajo total (también hay swaps,
accesos a cache, etc.), pero es la parte donde la diferencia es más
dramática.

---

## Indirección: por qué el compilador no puede optimizar qsort

### El puntero a función es opaco

```c
void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
```

El compilador de C ve `compar` como un puntero cuyo valor se conocerá
en tiempo de ejecución. Incluso con optimización `-O3`, no puede:

1. **Inlinear** la función apuntada — no sabe cuál es.
2. **Especializar** el loop por tipo — no sabe el tipo.
3. **Eliminar el frame setup** — la llamada a función es real.
4. **Vectorizar** las comparaciones — no ve la operación concreta.

### Compilación separada

`qsort` vive en `libc.so`, compilada independientemente del programa
del usuario. Incluso si el compilador pudiera deducir que `compar`
siempre apunta a `cmp_int`, no tiene acceso al código de `qsort`
para sustituir la llamada.

**LTO (Link-Time Optimization)** podría ayudar en teoría, pero:

- La libc del sistema no participa en LTO (está precompilada).
- Incluso con una libc estática compilada con LTO, la indirección
  de `void *` impide la especialización por tipo.

### Monomorphization en Rust

```rust
// Código genérico en la stdlib:
pub fn sort(&mut self) where T: Ord { ... }

// El compilador genera para cada uso:
fn sort_i32(data: &mut [i32]) { ... }    // comparación: cmp i32
fn sort_string(data: &mut [String]) { ... } // comparación: strcmp inline
fn sort_student(data: &mut [Student]) { ... } // comparación: campo por campo
```

Cada instanciación es una función independiente donde:

- El tipo `T` es concreto — el compilador conoce el layout y tamaño.
- La closure/`.cmp()` está inlineada — no hay llamada indirecta.
- El swap es especializado — `swap::<i32>` mueve 4 bytes, no `size`
  bytes genéricos.
- El compilador puede **vectorizar**, **reordenar instrucciones**,
  **usar registros** óptimamente.

---

## El costo del swap genérico

### En C

`qsort` no conoce el tipo, solo el tamaño en bytes. Para intercambiar
dos elementos, debe copiar `size` bytes:

```c
/* Interno de qsort */
char tmp[size];                          /* buffer temporal */
memcpy(tmp,            base + i*size, size);   /* size bytes */
memcpy(base + i*size,  base + j*size, size);   /* size bytes */
memcpy(base + j*size,  tmp,           size);   /* size bytes */
```

Para un `int` (4 bytes), esto son 3 llamadas a `memcpy` de 4 bytes
cada una. Para un struct de 128 bytes, son 3 × 128 = 384 bytes
copiados.

El compilador no puede optimizar las llamadas a `memcpy` porque
`size` es un parámetro en runtime — no puede generar un simple
`mov` de 4 bytes para enteros.

### En Rust

El compilador conoce `T` y genera código especializado:

```rust
// Para i32: un swap de 4 bytes en registros
// mov eax, [arr + i*4]
// xchg eax, [arr + j*4]
// mov [arr + i*4], eax

// Para un struct grande: memcpy de tamaño conocido en compilación
// El compilador puede usar instrucciones SIMD si el tamaño lo permite
```

Para tipos pequeños (primitivos), el swap es una instrucción. Para
tipos grandes, el compilador genera `memcpy` con tamaño constante,
que puede ser optimizado a una secuencia de `movdqa` (SSE) o incluso
eliminado si puede reordenar los datos en registros.

---

## Predicción de branches

### Call indirecto en C

Cada llamada al comparador es un **branch indirecto**: `call [rax]`.
El procesador debe predecir el destino del salto. Para `qsort`, el
destino es siempre el mismo (el mismo comparador), así que el branch
predictor acumula confianza rápidamente.

Sin embargo, la penalización inicial y el costo fijo del call/ret
persisten. Cada comparación tiene ~5 ns de overhead de llamada
(variable según la microarquitectura).

### Branch directo en Rust

Con el comparador inlineado, la comparación genera un branch
condicional directo (`jl`, `jge`), que el predictor maneja con mayor
precisión. No hay overhead de call/ret.

Más importante: con la comparación inlineada, el compilador puede
aplicar **if-conversion** (transformar el branch en un `cmov`
condicional sin branch), eliminando la predicción por completo:

```asm
; Rust (inlineado, con cmov)
cmp     eax, ecx
cmovl   edx, esi    ; sin branch, sin misprediction posible
```

`qsort` no puede beneficiarse de esta optimización porque la
comparación está oculta tras la llamada a función.

---

## Benchmark: $10^6$ elementos

### Programa en C

```c
/* bench_c.c — Benchmark de qsort vs sort manual para int */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N 1000000
#define RUNS 5

int cmp_int(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

/* Quicksort manual para int (Hoare partition + median-of-three) */
static inline void swap(int *a, int *b)
{
    int t = *a; *a = *b; *b = t;
}

static int median_of_three(int *arr, int lo, int hi)
{
    int mid = lo + (hi - lo) / 2;
    if (arr[lo] > arr[mid]) swap(&arr[lo], &arr[mid]);
    if (arr[lo] > arr[hi])  swap(&arr[lo], &arr[hi]);
    if (arr[mid] > arr[hi]) swap(&arr[mid], &arr[hi]);
    return mid;
}

static int partition(int *arr, int lo, int hi)
{
    int m = median_of_three(arr, lo, hi);
    swap(&arr[m], &arr[lo]);
    int pivot = arr[lo];
    int i = lo, j = hi + 1;

    for (;;) {
        while (arr[++i] < pivot) if (i == hi) break;
        while (arr[--j] > pivot) if (j == lo) break;
        if (i >= j) break;
        swap(&arr[i], &arr[j]);
    }
    swap(&arr[lo], &arr[j]);
    return j;
}

static void manual_qsort(int *arr, int lo, int hi)
{
    if (hi - lo < 16) {
        /* Insertion sort para subarrays pequeños */
        for (int i = lo + 1; i <= hi; i++) {
            int key = arr[i];
            int j = i - 1;
            while (j >= lo && arr[j] > key) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }
    int p = partition(arr, lo, hi);
    manual_qsort(arr, lo, p - 1);
    manual_qsort(arr, p + 1, hi);
}

/* LCG determinista para reproducibilidad */
static unsigned int lcg_state = 42;
static int lcg_rand(void)
{
    lcg_state = lcg_state * 1103515245 + 12345;
    return (int)(lcg_state >> 1);
}

int main(void)
{
    int *original = malloc(N * sizeof(int));
    int *work     = malloc(N * sizeof(int));

    for (int i = 0; i < N; i++)
        original[i] = lcg_rand();

    /* Benchmark qsort */
    double qsort_total = 0;
    for (int r = 0; r < RUNS; r++) {
        memcpy(work, original, N * sizeof(int));
        clock_t t0 = clock();
        qsort(work, N, sizeof(int), cmp_int);
        clock_t t1 = clock();
        qsort_total += (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;
    }

    /* Benchmark sort manual */
    double manual_total = 0;
    for (int r = 0; r < RUNS; r++) {
        memcpy(work, original, N * sizeof(int));
        clock_t t0 = clock();
        manual_qsort(work, 0, N - 1);
        clock_t t1 = clock();
        manual_total += (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;
    }

    printf("=== C: %d elementos, promedio de %d runs ===\n", N, RUNS);
    printf("qsort (libc):     %7.2f ms\n", qsort_total / RUNS);
    printf("manual quicksort: %7.2f ms\n", manual_total / RUNS);
    printf("ratio qsort/manual: %.2fx\n", qsort_total / manual_total);

    free(original);
    free(work);
    return 0;
}
```

### Programa en Rust

```rust
// bench_rust.rs — Benchmark de sort vs sort_unstable
use std::time::Instant;

const N: usize = 1_000_000;
const RUNS: usize = 5;

fn lcg_rand(state: &mut u32) -> i32 {
    *state = state.wrapping_mul(1103515245).wrapping_add(12345);
    (*state >> 1) as i32
}

fn benchmark<F: FnMut(&mut Vec<i32>)>(
    label: &str,
    original: &[i32],
    mut f: F,
) -> f64 {
    let mut total = 0.0;
    for _ in 0..RUNS {
        let mut work = original.to_vec();
        let start = Instant::now();
        f(&mut work);
        total += start.elapsed().as_secs_f64() * 1000.0;
    }
    let avg = total / RUNS as f64;
    println!("  {:<30} {:>7.2} ms", label, avg);
    avg
}

fn main() {
    let mut state: u32 = 42;
    let original: Vec<i32> = (0..N).map(|_| lcg_rand(&mut state)).collect();

    println!("=== Rust: {} elementos, promedio de {} runs ===\n", N, RUNS);

    println!("--- Datos aleatorios ---");
    let t_sort = benchmark("sort (Timsort, estable)", &original, |v| v.sort());
    let t_unstable = benchmark("sort_unstable (pdqsort)", &original, |v| v.sort_unstable());

    println!("  ratio sort/sort_unstable:     {:.2}x", t_sort / t_unstable);

    /* Datos ya ordenados */
    let sorted: Vec<i32> = {
        let mut v = original.clone();
        v.sort();
        v
    };

    println!("\n--- Datos ordenados ---");
    benchmark("sort (Timsort)", &sorted, |v| v.sort());
    benchmark("sort_unstable (pdqsort)", &sorted, |v| v.sort_unstable());

    /* Datos casi-ordenados (1% perturbados) */
    let mut almost = sorted.clone();
    let mut s: u32 = 7;
    for i in (0..N).step_by(100) {
        s = s.wrapping_mul(1103515245).wrapping_add(12345);
        let j = i + (s as usize % 10).min(N - i - 1);
        almost.swap(i, j);
    }

    println!("\n--- Datos casi-ordenados ---");
    benchmark("sort (Timsort)", &almost, |v| v.sort());
    benchmark("sort_unstable (pdqsort)", &almost, |v| v.sort_unstable());

    /* Datos con muchos duplicados */
    let duplicated: Vec<i32> = original.iter().map(|x| x % 100).collect();

    println!("\n--- Muchos duplicados (mod 100) ---");
    benchmark("sort (Timsort)", &duplicated, |v| v.sort());
    benchmark("sort_unstable (pdqsort)", &duplicated, |v| v.sort_unstable());
}
```

### Compilar y ejecutar

```bash
# C
gcc -O2 -std=c11 -o bench_c bench_c.c
./bench_c

# Rust
rustc -O -o bench_rust bench_rust.rs
./bench_rust
```

### Resultados típicos (x86-64, Linux)

```
=== C: 1000000 elementos, promedio de 5 runs ===
qsort (libc):       72.50 ms
manual quicksort:   28.30 ms
ratio qsort/manual: 2.56x

=== Rust: 1000000 elementos, promedio de 5 runs ===

--- Datos aleatorios ---
  sort (Timsort, estable)          62.00 ms
  sort_unstable (pdqsort)          38.00 ms
  ratio sort/sort_unstable:     1.63x

--- Datos ordenados ---
  sort (Timsort)                    1.50 ms
  sort_unstable (pdqsort)           1.80 ms

--- Datos casi-ordenados ---
  sort (Timsort)                    5.20 ms
  sort_unstable (pdqsort)           7.00 ms

--- Muchos duplicados (mod 100) ---
  sort (Timsort)                   25.00 ms
  sort_unstable (pdqsort)          10.50 ms
```

**Nota**: los valores exactos varían según CPU, versión del compilador,
y versión de libc. Las **proporciones** entre las implementaciones
son lo relevante.

---

## Análisis de los resultados

### C: qsort vs manual

| Métrica | `qsort` | Manual QS |
|---------|---------|-----------|
| Aleatorio | ~70 ms | ~28 ms |
| Ratio | 2.5x más lento | baseline |

El sort manual es ~2.5x más rápido que `qsort` para enteros. Esta
diferencia se debe **casi exclusivamente** al overhead de indirección:

1. ~20M llamadas indirectas al comparador eliminadas.
2. Swap de 4 bytes en registros vs `memcpy` genérico.
3. Comparación inlineada permite `cmov` y reordenamiento de
   instrucciones.

### Rust: sort vs sort_unstable

| Input | `sort` | `sort_unstable` | Ratio |
|-------|--------|-----------------|-------|
| Aleatorio | ~62 ms | ~38 ms | 1.6x |
| Ordenado | ~1.5 ms | ~1.8 ms | 0.8x |
| Casi-ordenado | ~5 ms | ~7 ms | 0.7x |
| Duplicados | ~25 ms | ~10.5 ms | 2.4x |

- **Aleatorio**: `sort_unstable` gana — pdqsort es más rápido que
  Timsort para datos sin estructura, y no necesita alloc.
- **Ordenado/casi-ordenado**: `sort` gana — Timsort detecta runs
  naturales con overhead mínimo.
- **Duplicados**: `sort_unstable` gana por mucho — pdqsort usa
  Dutch National Flag que maneja duplicados en $O(n)$.

### C qsort vs Rust sort_unstable (la comparación que importa)

| | C `qsort` | Rust `sort_unstable` |
|---|---|---|
| Aleatorio | ~70 ms | ~38 ms |
| Ratio | **1.8x más lento** | baseline |

Rust `sort_unstable` es ~1.8x más rápido que C `qsort` para enteros
aleatorios. Y es comparable en velocidad al quicksort manual de C
(~38 vs ~28 ms), con la diferencia de que el código Rust es una sola
línea: `v.sort_unstable()`.

El sort manual en C es aún más rápido (~28 ms) porque:

1. Hoare partition con median-of-three está optimizado para este
   input específico.
2. No tiene los features defensivos de pdqsort (detección de
   adversarios, fallback a heapsort).

Pero nadie escribe un sort manual para cada tipo en producción. La
comparación relevante es **stdlib vs stdlib**: `qsort` vs
`.sort_unstable()`.

---

## Mapa de trade-offs

```
                    Ergonomía
                       ↑
                       |
   Rust .sort()  ●     |     ● Rust .sort_unstable()
   (estable,           |       (rápido,
    adaptativo)        |        genérico)
                       |
   ─────────────────── + ──────────────────→ Velocidad
                       |
   C qsort ●           |     ● C sort manual
   (genérico,          |       (rápido,
    portable)          |        no genérico)
                       |
```

- **C `qsort`**: genérico y portable, pero lento por indirección.
- **C sort manual**: rápido, pero hay que reescribirlo para cada tipo.
- **Rust `.sort()`**: genérico, estable, adaptativo — buen balance.
- **Rust `.sort_unstable()`**: genérico y rápido — lo mejor de ambos
  mundos para cuando la estabilidad no importa.

Rust elimina el dilema de C: no hay que elegir entre genericidad y
rendimiento. Monomorphization da ambos.

---

## Más allá de la velocidad: seguridad

El rendimiento es la diferencia más visible, pero la **seguridad de
tipos** es la diferencia más importante en código de producción:

### Errores imposibles en Rust

```c
/* C — todos compilan, todos son bugs silenciosos: */
double arr[] = {3.14, 1.41};
qsort(arr, 2, sizeof(int), cmp_int);       /* 1. sizeof incorrecto */
qsort(arr, 2, sizeof(double), cmp_int);    /* 2. comparador de tipo equivocado */

int *p = malloc(n * sizeof(int));
qsort(p, n, sizeof(p), cmp_int);           /* 3. sizeof(puntero) vs sizeof(int) */
```

```rust
// Rust — ninguno de estos es posible:

let mut arr = vec![3.14_f64, 1.41];
arr.sort();
// ERROR de compilación: f64 no implementa Ord

// No hay sizeof incorrecto — el compilador conoce el tipo
// No hay cast incorrecto — no hay void *
// No hay comparador de tipo equivocado — la closure tiene tipos concretos
```

### El bug de overflow es imposible en Rust

```c
/* C — bug silencioso: */
int cmp(const void *a, const void *b) {
    return *(int *)a - *(int *)b;  /* overflow para valores extremos */
}
```

```rust
// Rust — .cmp() retorna Ordering, no int:
arr.sort_by(|a, b| a.cmp(b));  // nunca puede desbordar
```

En Rust, `.cmp()` retorna un enum `Ordering` (`Less`/`Equal`/`Greater`),
no un entero. No hay oportunidad de overflow porque no hay resta.

---

## Tabla comparativa completa

| Aspecto | C `qsort` | Rust `.sort()` | Rust `.sort_unstable()` |
|---------|-----------|----------------|-------------------------|
| **Algoritmo** | Depende de libc | Timsort | pdqsort |
| **Estable** | No garantizado | Sí | No |
| **Complejidad** | Depende de libc | $O(n \log n)$ siempre | $O(n \log n)$ siempre |
| **Espacio** | Depende de libc | $O(n)$ | $O(\log n)$ |
| **Adaptativo** | Depende de libc | Sí ($O(n)$ si ordenado) | Parcial |
| **Comparador** | Puntero a función | Closure `FnMut` | Closure `FnMut` |
| **Inlining** | Imposible | Sí | Sí |
| **Tipo seguro** | No (`void *`) | Sí (generics) | Sí |
| **Perf aleatorio** | ~70 ms ($10^6$ int) | ~62 ms | ~38 ms |
| **Perf ordenado** | ~20 ms | ~1.5 ms | ~1.8 ms |
| **Multi-campo** | Cadena de `if` con cast | `.then()` / tuplas | `.then()` / tuplas |
| **Estado en cmp** | Global o `qsort_r` (no estándar) | Closure captura | Closure captura |
| **Error de tipo** | UB silencioso | Error de compilación | Error de compilación |
| **Binario** | Una copia de qsort | Una copia por tipo `T` | Una copia por tipo `T` |
| **Requisitos** | `<stdlib.h>` (siempre disponible) | `std` (siempre disponible) | `std` |

---

## qsort_r: la extensión para estado

El estándar C no ofrece forma de pasar estado al comparador. Las
extensiones `qsort_r` (POSIX) y `qsort_s` (C11 Annex K) añaden un
parámetro extra:

```c
/* GNU/Linux (glibc) */
void qsort_r(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *, void *),
             void *arg);

/* BSD/macOS — ¡arg en posición diferente! */
void qsort_r(void *base, size_t nmemb, size_t size,
             void *arg,
             int (*compar)(void *, const void *, const void *));
```

Nótese que GNU y BSD tienen **firmas incompatibles** — la posición
de `arg` difiere. Código que usa `qsort_r` no es portable entre
Linux y macOS sin `#ifdef`.

En Rust, las closures resuelven este problema de forma natural:

```rust
let multiplier = 3;
nums.sort_by(|a, b| (a * multiplier).cmp(&(b * multiplier)));
// multiplier capturado por la closure — sin argumento extra, sin extensión
```

---

## Cuándo cada enfoque es apropiado

### Usar qsort en C cuando

- Se trabaja en C puro sin alternativa (kernel, firmware, legacy).
- El rendimiento del sort no es crítico (arrays pequeños, operación
  infrecuente).
- Se necesita portabilidad absoluta (todo compilador C tiene `qsort`).
- El tipo ya tiene un comparador probado (reutilización).

### Usar sort manual en C cuando

- El sort es un cuello de botella medido con profiling.
- El tipo es fijo y conocido (siempre `int`, siempre `struct X`).
- Se necesita control fino del algoritmo (cutoffs, pivote, etc.).
- En sistemas embebidos con restricciones de stack/heap.

### Usar .sort() / .sort_unstable() en Rust cuando

- Siempre. No hay razón para escribir un sort manual en Rust.
- La stdlib da rendimiento comparable al sort manual de C, con
  ergonomía de `qsort` y seguridad de tipos total.
- Elegir entre `sort` (estable, adaptativo) y `sort_unstable`
  (más rápido para datos aleatorios) según la necesidad.

---

## Programa integrado: C y Rust lado a lado

### Tarea: ordenar estudiantes por GPA descendente, nombre ascendente

#### En C

```c
/* sort_students_c.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[32];
    double gpa;
} Student;

int cmp_student(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    /* GPA descendente */
    if (sa->gpa > sb->gpa) return -1;
    if (sa->gpa < sb->gpa) return  1;

    /* Empate: nombre ascendente */
    return strcmp(sa->name, sb->name);
}

int main(void)
{
    Student students[] = {
        {"Carlos",  3.50},
        {"Ana",     3.90},
        {"Beatriz", 3.50},
        {"David",   3.90},
        {"Elena",   3.20},
    };
    size_t n = sizeof(students) / sizeof(students[0]);

    qsort(students, n, sizeof(students[0]), cmp_student);

    printf("%-10s  GPA\n", "Nombre");
    printf("──────────  ────\n");
    for (size_t i = 0; i < n; i++)
        printf("%-10s  %.2f\n", students[i].name, students[i].gpa);

    return 0;
}
```

#### En Rust

```rust
// sort_students_rust.rs

#[derive(Debug)]
struct Student {
    name: String,
    gpa: f64,
}

fn main() {
    let mut students = vec![
        Student { name: "Carlos".into(),  gpa: 3.50 },
        Student { name: "Ana".into(),     gpa: 3.90 },
        Student { name: "Beatriz".into(), gpa: 3.50 },
        Student { name: "David".into(),   gpa: 3.90 },
        Student { name: "Elena".into(),   gpa: 3.20 },
    ];

    students.sort_by(|a, b| {
        b.gpa.total_cmp(&a.gpa)
            .then_with(|| a.name.cmp(&b.name))
    });

    println!("{:<10}  GPA", "Nombre");
    println!("──────────  ────");
    for s in &students {
        println!("{:<10}  {:.2}", s.name, s.gpa);
    }
}
```

#### Salida (idéntica en ambos)

```
Nombre      GPA
──────────  ────
Ana         3.90
David       3.90
Beatriz     3.50
Carlos      3.50
Elena       3.20
```

#### Análisis lado a lado

| | C | Rust |
|---|---|---|
| Líneas del comparador | 10 | 3 |
| Castings manuales | 2 (`const Student *`) | 0 |
| Posibilidad de bug de tipo | Sí | No |
| Manejo de `double` | `<`/`>` (ignora NaN) | `total_cmp` (maneja NaN) |
| Llamada | `qsort(arr, n, sizeof(...), cmp)` | `arr.sort_by(\|a, b\| ...)` |
| sizeof explícito | Sí | No |
| Inlining del comparador | No | Sí |

---

## Ejercicios

### Ejercicio 1 — Predecir overhead

Para un array de $10^6$ enteros con ~20M comparaciones, estima
cuántas llamadas indirectas a función realiza `qsort` y cuántas
realiza `.sort_unstable()`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

- **C `qsort`**: ~20 millones de llamadas indirectas. Cada
  comparación es una llamada `call *compar` a través de puntero
  a función.

- **Rust `.sort_unstable()`**: **cero** llamadas indirectas. La
  closure está inlineada en el código del sort después de
  monomorphization. Cada comparación es una instrucción `cmp`
  directa.

Cada llamada indirecta tiene un overhead de ~3-5 ns (setup de frame,
argumentos, return). Para 20M llamadas:

$20 \times 10^6 \times 4 \text{ ns} = 80 \text{ ms}$ de overhead puro

Esto explica gran parte de la diferencia de ~30-40 ms observada en
los benchmarks (el resto se amortiza por branch prediction y caching
del target address).

</details>

### Ejercicio 2 — sizeof bug en C

¿Qué resultado produce este código en una plataforma de 64 bits?

```c
double arr[] = {3.14, 1.41, 2.72, 0.58};
qsort(arr, 4, sizeof(float), cmp_double);
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

`sizeof(float) = 4`, pero `sizeof(double) = 8`. `qsort` cree que
cada elemento ocupa 4 bytes y avanza de 4 en 4 bytes.

En el primer paso, `qsort` pasa al comparador un puntero a los
primeros 4 bytes de `arr[0]` y un puntero a los últimos 4 bytes
de `arr[0]` (que interpreta como el "segundo elemento"). El
comparador castea estos 4 bytes como `double`, leyendo 8 bytes desde
una posición no alineada — comportamiento indefinido.

El resultado es basura: lecturas desalineadas, valores sin sentido,
posible segfault en arquitecturas que no toleran accesos desalineados
(ARM estricto).

En Rust este error es imposible — no hay `sizeof` que pasar:

```rust
let mut arr = vec![3.14_f64, 1.41, 2.72, 0.58];
arr.sort_by(|a, b| a.total_cmp(b));
// El compilador conoce que T = f64, usa el stride correcto
```

</details>

### Ejercicio 3 — Monomorphization: tamaño del binario

Si un programa Rust ordena `Vec<i32>`, `Vec<String>`, y `Vec<f64>`,
¿cuántas copias del código de sort genera el compilador? ¿Cuál es el
trade-off?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

El compilador genera **3 copias** del algoritmo de sort:

1. `sort::<i32>` — comparación con `i32::cmp`, swap de 4 bytes.
2. `sort::<String>` — comparación con `String::cmp` (delegando a
   `str::cmp`), swap de 24 bytes (size_of::<String>() = 24 en
   64-bit: ptr + len + cap).
3. `sort::<f64>` — si se usa `sort_by` con `total_cmp`.

Trade-off:

| | C `qsort` | Rust `.sort()` |
|---|---|---|
| Código en binario | 1 copia | N copias (una por tipo) |
| Tamaño del binario | Menor | Mayor |
| Velocidad | Menor (indirección) | Mayor (especializado) |

Para un programa que ordena 10 tipos diferentes, Rust genera 10
copias del sort. Con Timsort siendo ~500-1000 líneas de código
máquina, esto añade ~5-10 KB al binario por tipo. En la mayoría
de aplicaciones, este costo es insignificante.

En sistemas embebidos con ROM muy limitada (< 64 KB), este code
bloat puede ser relevante, y es uno de los pocos escenarios donde
la aproximación de C (una sola copia genérica) tiene ventaja.

</details>

### Ejercicio 4 — Portabilidad de qsort

Un programa depende de que `qsort` sea estable. Funciona en Linux
(glibc) pero falla en Alpine (musl). Explica por qué y cómo
solucionarlo de forma portable.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Por qué falla**: glibc implementa `qsort` con merge sort (estable).
musl implementa `qsort` con smoothsort (no estable). El estándar C
no requiere estabilidad, así que ambas son correctas.

El programa dependía de un detalle de implementación, no del
estándar.

**Soluciones portables**:

1. **Decorar con índice**: añadir el índice original como campo
   secundario en el comparador:

   ```c
   typedef struct { int value; size_t original_idx; } Decorated;

   int cmp_stable(const void *a, const void *b) {
       const Decorated *da = a, *db = b;
       if (da->value != db->value)
           return (da->value < db->value) ? -1 : 1;
       /* Empate: preservar orden original */
       return (da->original_idx < db->original_idx) ? -1 : 1;
   }
   ```

   Esto simula estabilidad con $O(n)$ espacio extra.

2. **Implementar merge sort propio**: usar un merge sort estable
   en lugar de `qsort`.

3. **Usar Rust**: `.sort()` es estable por especificación, no por
   coincidencia de implementación.

</details>

### Ejercicio 5 — Swap de struct grande

Para un struct de 256 bytes, ¿cuántos bytes copia `qsort` por
cada swap? ¿Y Rust `.sort()`? ¿Cómo se puede reducir el costo
en C?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**C `qsort`**: cada swap copia $3 \times 256 = 768$ bytes
(tmp ← a, a ← b, b ← tmp) usando `memcpy` con tamaño runtime.

**Rust `.sort()`**: misma cantidad de bytes copiados ($3 \times 256$),
pero con `memcpy` de **tamaño conocido en compilación**, permitiendo
al compilador usar instrucciones SIMD (AVX-256 copia 32 bytes por
instrucción → 8 instrucciones vs un loop genérico).

**Reducir el costo en C**: ordenar un array de **punteros** en lugar
del array de structs:

```c
typedef struct { char data[256]; int key; } Big;

Big items[N];
Big *ptrs[N];

for (int i = 0; i < N; i++) ptrs[i] = &items[i];

int cmp_ptr(const void *a, const void *b) {
    const Big *pa = *(const Big **)a;
    const Big *pb = *(const Big **)b;
    return (pa->key < pb->key) ? -1 : (pa->key > pb->key);
}

qsort(ptrs, N, sizeof(Big *), cmp_ptr);
/* Cada swap mueve 8 bytes (un puntero), no 256 */
```

En Rust, el mismo patrón:

```rust
let mut indices: Vec<usize> = (0..items.len()).collect();
indices.sort_by_key(|&i| items[i].key);
// Swap de usize (8 bytes) en lugar de 256 bytes
```

</details>

### Ejercicio 6 — Closure vs puntero a función

Explica por qué esta closure de Rust puede inlinearse pero el
puntero a función de C no:

```rust
arr.sort_by(|a, b| a.cmp(b));
```

```c
qsort(arr, n, sizeof(int), cmp_int);
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Rust**: la closure `|a, b| a.cmp(b)` tiene un tipo único y anónimo
generado por el compilador. El método `sort_by` es genérico sobre
`F: FnMut(&T, &T) -> Ordering`. Cuando se instancia para esta
closure específica, el compilador conoce **exactamente** qué código
ejecutar y puede inlinearlo directamente en el loop del sort.

Es como si `sort_by` fuera una plantilla/template que se expande con
el cuerpo de la closure pegado dentro.

**C**: `compar` es un `int (*)(const void *, const void *)` — un
puntero cuyo valor es la **dirección** de `cmp_int`. El compilador
ve:

```c
result = compar(ptr_a, ptr_b);  /* call [rax] */
```

Incluso si el análisis de flujo determina que `compar` siempre
apunta a `cmp_int`, `qsort` está en una biblioteca compartida
compilada por separado — el compilador no puede modificar su código.

Además, en C un puntero a función podría cambiar en runtime (no
es `const`), lo que impide optimizaciones incluso dentro de la
misma unidad de compilación.

La diferencia fundamental: en Rust el tipo de la closure **codifica
la función**, en C el puntero **codifica la dirección**.

</details>

### Ejercicio 7 — qsort_r portabilidad

Escribe una versión de `qsort_r` que funcione tanto en Linux como
en macOS, usando `#ifdef`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
#include <stdlib.h>

typedef int (*cmp_ctx_fn)(const void *, const void *, void *);

#ifdef __APPLE__
/* macOS: arg va antes del comparador, y el comparador recibe arg primero */
struct wrapper_ctx {
    cmp_ctx_fn real_cmp;
    void *real_arg;
};

static int wrapper(void *ctx, const void *a, const void *b) {
    struct wrapper_ctx *w = ctx;
    return w->real_cmp(a, b, w->real_arg);
}

void portable_qsort_r(void *base, size_t n, size_t size,
                       cmp_ctx_fn cmp, void *arg) {
    struct wrapper_ctx ctx = { cmp, arg };
    qsort_r(base, n, size, &ctx, wrapper);
}

#elif defined(__GLIBC__)
/* Linux/glibc: arg va al final */
void portable_qsort_r(void *base, size_t n, size_t size,
                       cmp_ctx_fn cmp, void *arg) {
    qsort_r(base, n, size, cmp, arg);
}

#else
/* Fallback: variable global (no thread-safe) */
static cmp_ctx_fn g_cmp;
static void *g_arg;

static int global_wrapper(const void *a, const void *b) {
    return g_cmp(a, b, g_arg);
}

void portable_qsort_r(void *base, size_t n, size_t size,
                       cmp_ctx_fn cmp, void *arg) {
    g_cmp = cmp;
    g_arg = arg;
    qsort(base, n, size, global_wrapper);
}
#endif
```

Este tipo de portabilidad es la razón por la que muchos proyectos C
implementan su propio sort. En Rust, las closures capturan estado
naturalmente — no existe el problema.

</details>

### Ejercicio 8 — Adaptatividad

Explica por qué `sort` (Timsort) de Rust es mucho más rápido que
`qsort` de glibc para datos ya ordenados, a pesar de que glibc
también usa merge sort.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Aunque glibc usa merge sort, su implementación es un **merge sort
clásico** que siempre divide el array por la mitad y siempre hace
el merge completo. Para $n = 10^6$ datos ya ordenados:

- glibc: $O(n \log n)$ comparaciones — divide y mergea todos los
  niveles, aunque cada merge descubre que los datos ya están en
  orden. Cada comparación pasa por la indirección de `void *`.

- Timsort de Rust: detecta que todo el array es un solo **run**
  ascendente en un solo pase lineal ($n - 1$ comparaciones). No
  hay merge, no hay recursión, no hay allocación.

Resultado típico para $10^6$ enteros ordenados:

```
glibc qsort:       ~20 ms  (n log n comparaciones indirectas)
Rust sort:          ~1.5 ms (n comparaciones directas, inlineadas)
```

La diferencia es ~13x, compuesta por:

1. **Adaptatividad**: $n$ comparaciones vs $n \log n$ (~20x menos).
2. **Inlining**: cada comparación es una instrucción vs una llamada.

Esta es la ventaja de Timsort sobre merge sort clásico: no solo es
$O(n \log n)$ en el peor caso, sino que es $O(n)$ para datos con
estructura — y la mayoría de datos reales tienen algún grado de
orden preexistente.

</details>

### Ejercicio 9 — Comparador incorrecto: consecuencias

¿Qué pasa en C y en Rust si el comparador no es transitivo?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**En C**: comportamiento indefinido. El estándar dice que el
comparador debe definir un orden total. Si no lo hace:

- `qsort` puede acceder fuera del array (buffer overflow).
- `qsort` puede entrar en un bucle infinito.
- `qsort` puede producir un resultado silenciosamente incorrecto.
- En algunos casos, puede corromper memoria.

Ejemplo: un comparador que usa `<` para `double` sin manejar `NaN`
viola transitividad cuando `NaN` está presente.

**En Rust**: la documentación dice que el comparador debe definir un
orden total consistente con `Eq`. Si no lo hace:

- El sort **no** produce UB (no hay unsafe en la interfaz pública).
- El resultado puede ser incorrecto (orden no especificado).
- El sort siempre termina y nunca accede fuera del slice.

La diferencia es fundamental: en C, un comparador buggy puede
comprometer la **seguridad** del programa (buffer overflow
explotable). En Rust, solo puede comprometer la **corrección** del
resultado.

Rust logra esto usando `unsafe` internamente con invariantes que
no dependen de la corrección del comparador para la seguridad de
memoria.

</details>

### Ejercicio 10 — Diseño: ¿por qué C no adoptó monomorphization?

Explica las razones históricas y técnicas por las que C usa `void *`
en lugar de generar código especializado como Rust.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Razones históricas**:

1. **Época**: C se diseñó en 1972 para el PDP-11 con 64 KB de
   memoria. Generar múltiples copias de funciones era impensable.
2. **Modelo de compilación**: C compila cada archivo `.c`
   independientemente. No hay mecanismo para instanciar templates
   cross-file (C++ los añadió 13 años después).
3. **Filosofía**: C es un "ensamblador portable". `void *` refleja
   directamente cómo funciona la máquina: un puntero es un puntero.

**Razones técnicas**:

1. **Sin templates/generics**: C no tiene sistema de tipos
   paramétricos. `void *` es la única forma de polimorfismo.
2. **ABI estable**: `qsort` tiene una firma fija en la libc. Cambiar
   a genéricos rompería la ABI de todos los programas existentes.
3. **Bibliotecas compartidas**: la libc se carga en runtime. No se
   puede generar código especializado para cada programa que la usa.

**C++ adoptó templates** (1990) y tiene `std::sort` con inlining
completo — el mismo enfoque que Rust. Pero C++ mantiene
retrocompatibilidad con C, así que `qsort` sigue disponible.

**Rust aprendió de ambos**: tomó los generics con monomorphization
de C++ (sin la complejidad de templates), y los hizo seguros con
el sistema de traits. El resultado es la ergonomía de `qsort` con
la velocidad de sort manual.

</details>

---

## Resumen

| Dimensión | C `qsort` | Rust `.sort_unstable()` | Factor |
|-----------|-----------|-------------------------|--------|
| Aleatorio $10^6$ int | ~70 ms | ~38 ms | 1.8x |
| Ordenado $10^6$ int | ~20 ms | ~1.8 ms | 11x |
| Llamadas indirectas | ~20M | 0 | ∞ |
| Bugs de tipo posibles | Sí (UB) | No (compilación) | — |
| Líneas de comparador | ~10 | ~1 | 10x |
| Portabilidad de estabilidad | No | Sí | — |
| Estado en comparador | `qsort_r` (no portable) | Closure (estándar) | — |
| Copias del sort en binario | 1 | N (por tipo) | Trade-off |
