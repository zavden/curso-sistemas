# Radix sort

## Objetivo

Dominar radix sort como el algoritmo de ordenamiento lineal más
versátil en la práctica:

- Ordenar por **dígitos** (o bytes, o bits), del menos significativo
  al más significativo (**LSD**) o viceversa (**MSD**).
- Counting sort como subrutina estable en cada pasada.
- Complejidad $O(d \cdot (n + k))$ donde $d$ = número de dígitos y
  $k$ = base (rango del dígito).
- Para enteros de 32 bits con base 256: $O(4n)$ = $O(n)$ — más rápido
  que cualquier comparison sort.
- No basado en comparaciones: no le aplica el límite $\Omega(n \log n)$.

---

## LSD vs MSD

### LSD Radix Sort (Least Significant Digit first)

Procesar desde el dígito **menos significativo** hasta el más
significativo. Cada pasada ordena por un dígito usando un sort estable
(counting sort).

La estabilidad es crucial: cuando se ordena por el dígito $d_i$, el
orden producido por los dígitos anteriores $d_0, \ldots, d_{i-1}$ se
preserva para elementos con el mismo valor de $d_i$.

```
RADIX_SORT_LSD(arr, n, d, base):
    para digit de 0 a d-1:           // del menos al más significativo
        COUNTING_SORT_BY_DIGIT(arr, n, digit, base)
```

### MSD Radix Sort (Most Significant Digit first)

Procesar desde el dígito **más significativo**. Se divide el array en
$k$ buckets según el dígito más significativo, y se ordena
recursivamente cada bucket por el siguiente dígito.

```
RADIX_SORT_MSD(arr, lo, hi, digit, base):
    si digit < 0 o lo >= hi: retornar
    particionar arr[lo..hi] en base buckets según digit
    para cada bucket b:
        RADIX_SORT_MSD(arr, start_b, end_b, digit-1, base)
```

### Comparación

| Aspecto | LSD | MSD |
|---------|-----|-----|
| Dirección | Dígito menor → mayor | Dígito mayor → menor |
| Requiere sort estable | **Sí** (esencial) | No |
| Recursión | No | Sí |
| Short-circuit | No (siempre $d$ pasadas) | Sí (puede parar si bucket tiene 1 elemento) |
| Ideal para | Enteros de tamaño fijo | Strings de longitud variable |
| Paralelismo | Fácil (cada pasada es independiente) | Difícil (recursión en buckets) |

**LSD** es más simple y más usado para enteros. **MSD** es mejor para
strings (puede parar cuando el prefijo ya distingue los elementos).

---

## Traza detallada: LSD con base 10

Array: `[170, 45, 75, 90, 802, 24, 2, 66]`.

### Pasada 1: dígito de unidades (posición 0)

```
Extraer dígito de unidades (÷ 1 mod 10):
  170→0  45→5  75→5  90→0  802→2  24→4  2→2  66→6

Counting sort por este dígito:
  Bucket 0: [170, 90]
  Bucket 2: [802, 2]
  Bucket 4: [24]
  Bucket 5: [45, 75]
  Bucket 6: [66]

Resultado: [170, 90, 802, 2, 24, 45, 75, 66]
```

### Pasada 2: dígito de decenas (posición 1)

```
Extraer dígito de decenas (÷ 10 mod 10):
  170→7  90→9  802→0  2→0  24→2  45→4  75→7  66→6

Counting sort por este dígito:
  Bucket 0: [802, 2]      ← estabilidad: 802 antes de 2 (orden de pasada 1)
  Bucket 2: [24]
  Bucket 4: [45]
  Bucket 6: [66]
  Bucket 7: [170, 75]     ← estabilidad: 170 antes de 75
  Bucket 9: [90]

Resultado: [802, 2, 24, 45, 66, 170, 75, 90]
```

### Pasada 3: dígito de centenas (posición 2)

```
Extraer dígito de centenas (÷ 100 mod 10):
  802→8  2→0  24→0  45→0  66→0  170→1  75→0  90→0

Counting sort por este dígito:
  Bucket 0: [2, 24, 45, 66, 75, 90]  ← orden de pasada 2 preservado
  Bucket 1: [170]
  Bucket 8: [802]

Resultado: [2, 24, 45, 66, 75, 90, 170, 802]  ✓ Ordenado
```

### Por qué funciona

Después de la pasada $i$, los elementos están ordenados correctamente
respecto a los dígitos $0, 1, \ldots, i$. La estabilidad de counting
sort garantiza que cuando dos elementos tienen el mismo dígito $i + 1$,
su orden relativo (determinado por los dígitos $0, \ldots, i$) se
preserva.

---

## Elección de la base

La base $k$ (radix) determina el trade-off entre número de pasadas
y costo por pasada:

| Base | Pasadas ($d$) para 32 bits | Costo del counting sort | Total |
|------|--------------------------|------------------------|-------|
| 2 (bit a bit) | 32 | $O(n + 2) = O(n)$ | $O(32n)$ |
| 10 (decimal) | 10 | $O(n + 10) = O(n)$ | $O(10n)$ |
| 16 (nibble) | 8 | $O(n + 16) = O(n)$ | $O(8n)$ |
| 256 (byte) | 4 | $O(n + 256) = O(n)$ | $O(4n)$ |
| 65536 (16 bits) | 2 | $O(n + 65536)$ | $O(2n + 131072)$ |

**Base 256** (un byte por pasada) es el punto óptimo para la mayoría
de casos:
- Solo 4 pasadas para 32 bits, 8 para 64 bits.
- El array `count` tiene 256 entradas (1 KB) — cabe en L1 cache.
- Extraer un byte es una operación trivial: `(x >> (8*d)) & 0xFF`.

Base 65536 hace solo 2 pasadas pero el array `count` tiene 65536
entradas (256 KB) — puede no caber en L1, degradando el rendimiento.

---

## Manejo de enteros con signo

Los enteros con signo en complemento a dos tienen el bit más
significativo invertido respecto al orden numérico: los negativos
tienen el MSB en 1, los positivos en 0. Pero en orden numérico,
los negativos son menores.

### Solución 1: flip del MSB

Antes de ordenar, invertir el bit de signo de cada elemento:
`x ^= 0x80000000`. Esto convierte negativos (MSB=1) en valores
con MSB=0 y positivos (MSB=0) en MSB=1, restaurando el orden correcto
para radix sort. Después de ordenar, invertir de nuevo.

### Solución 2: tratar la última pasada como signed

En la última pasada (byte más significativo), tratar los valores
128-255 (negativos) como menores que 0-127 (positivos).

---

## Radix sort para floats

Los floats IEEE 754 tienen una propiedad útil: para floats positivos,
la representación en bits preserva el orden numérico. Para floats
negativos, el orden está invertido.

Transformación para radix sort:
1. Si positivo (MSB=0): invertir el MSB → `x ^= 0x80000000`.
2. Si negativo (MSB=1): invertir todos los bits → `x = ~x`.

Después de ordenar, invertir la transformación.

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── LSD Radix sort — base 10 (didactic) ────────────────── */

void radix_sort_base10(int *arr, int n) {
    int *output = malloc(n * sizeof(int));
    int max_val = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max_val) max_val = arr[i];

    for (int exp = 1; max_val / exp > 0; exp *= 10) {
        int count[10] = {0};

        for (int i = 0; i < n; i++)
            count[(arr[i] / exp) % 10]++;

        for (int v = 1; v < 10; v++)
            count[v] += count[v - 1];

        for (int i = n - 1; i >= 0; i--) {
            int digit = (arr[i] / exp) % 10;
            count[digit]--;
            output[count[digit]] = arr[i];
        }
        memcpy(arr, output, n * sizeof(int));
    }
    free(output);
}

/* ── LSD Radix sort — base 256 (practical) ──────────────── */

void radix_sort_base256(unsigned int *arr, int n) {
    unsigned int *output = malloc(n * sizeof(unsigned int));

    for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
        int shift = byte_idx * 8;
        int count[256] = {0};

        for (int i = 0; i < n; i++)
            count[(arr[i] >> shift) & 0xFF]++;

        for (int v = 1; v < 256; v++)
            count[v] += count[v - 1];

        for (int i = n - 1; i >= 0; i--) {
            int byte_val = (arr[i] >> shift) & 0xFF;
            count[byte_val]--;
            output[count[byte_val]] = arr[i];
        }
        memcpy(arr, output, n * sizeof(unsigned int));
    }
    free(output);
}

/* ── LSD Radix sort — signed int (flip MSB) ─────────────── */

void radix_sort_signed(int *arr, int n) {
    unsigned int *ua = (unsigned int *)arr;

    /* Flip sign bit so negative < positive in unsigned order */
    for (int i = 0; i < n; i++)
        ua[i] ^= 0x80000000u;

    radix_sort_base256(ua, n);

    /* Flip back */
    for (int i = 0; i < n; i++)
        ua[i] ^= 0x80000000u;
}

/* ── LSD Radix sort — skip pass optimization ─────────────── */

void radix_sort_optimized(unsigned int *arr, int n) {
    unsigned int *output = malloc(n * sizeof(unsigned int));

    for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
        int shift = byte_idx * 8;
        int count[256] = {0};

        for (int i = 0; i < n; i++)
            count[(arr[i] >> shift) & 0xFF]++;

        /* If all elements have the same byte, skip this pass */
        int skip = 0;
        for (int v = 0; v < 256; v++) {
            if (count[v] == n) { skip = 1; break; }
        }
        if (skip) continue;

        for (int v = 1; v < 256; v++)
            count[v] += count[v - 1];

        for (int i = n - 1; i >= 0; i--) {
            int byte_val = (arr[i] >> shift) & 0xFF;
            count[byte_val]--;
            output[count[byte_val]] = arr[i];
        }
        memcpy(arr, output, n * sizeof(unsigned int));
    }
    free(output);
}

/* ── MSD Radix sort (recursive) ──────────────────────────── */

static void msd_radix_r(int *arr, int *tmp, int lo, int hi, int shift) {
    if (lo >= hi || shift < 0) return;

    int count[256] = {0};
    int n = hi - lo + 1;

    for (int i = lo; i <= hi; i++)
        count[(arr[i] >> shift) & 0xFF]++;

    /* Prefix sum */
    int starts[256];
    starts[0] = 0;
    for (int v = 1; v < 256; v++)
        starts[v] = starts[v - 1] + count[v - 1];

    /* Copy positions for placing */
    int pos[256];
    memcpy(pos, starts, sizeof(pos));

    /* Place into tmp */
    for (int i = lo; i <= hi; i++) {
        int byte_val = (arr[i] >> shift) & 0xFF;
        tmp[lo + pos[byte_val]] = arr[i];
        pos[byte_val]++;
    }
    memcpy(arr + lo, tmp + lo, n * sizeof(int));

    /* Recurse on each bucket */
    for (int v = 0; v < 256; v++) {
        int blo = lo + starts[v];
        int bhi = blo + count[v] - 1;
        if (count[v] > 1)
            msd_radix_r(arr, tmp, blo, bhi, shift - 8);
    }
}

void radix_sort_msd(int *arr, int n) {
    int *tmp = malloc(n * sizeof(int));

    /* Flip sign bit for correct signed ordering */
    unsigned int *ua = (unsigned int *)arr;
    for (int i = 0; i < n; i++) ua[i] ^= 0x80000000u;

    msd_radix_r(arr, tmp, 0, n - 1, 24);

    for (int i = 0; i < n; i++) ua[i] ^= 0x80000000u;

    free(tmp);
}

/* ── Helpers ─────────────────────────────────────────────── */

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;
    return 1;
}

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ── Demo 1: Base 10 trace ───────────────────────────────── */

void demo_trace(void) {
    printf("=== Demo 1: LSD Radix sort trace (base 10) ===\n\n");

    int arr[] = {170, 45, 75, 90, 802, 24, 2, 66};
    int n = 8;

    printf("Input: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n\n");

    int output[8];
    int max_val = 802;
    int pass = 0;

    for (int exp = 1; max_val / exp > 0; exp *= 10) {
        pass++;
        int count[10] = {0};

        printf("--- Pass %d: digit position %d (÷%d mod 10) ---\n", pass, pass - 1, exp);
        printf("  Digits: ");
        for (int i = 0; i < n; i++) printf("%d→%d  ", arr[i], (arr[i] / exp) % 10);
        printf("\n");

        for (int i = 0; i < n; i++) count[(arr[i] / exp) % 10]++;

        printf("  Count:  ");
        for (int v = 0; v < 10; v++) if (count[v]) printf("[%d]=%d ", v, count[v]);
        printf("\n");

        for (int v = 1; v < 10; v++) count[v] += count[v - 1];
        for (int i = n - 1; i >= 0; i--) {
            int d = (arr[i] / exp) % 10;
            count[d]--;
            output[count[d]] = arr[i];
        }
        memcpy(arr, output, n * sizeof(int));

        printf("  Result: ");
        for (int i = 0; i < n; i++) printf("%d ", arr[i]);
        printf("\n\n");
    }
}

/* ── Demo 2: Base 256 performance ────────────────────────── */

void demo_base256(int n) {
    printf("=== Demo 2: Radix sort base 256 vs qsort, n=%d ===\n\n", n);

    int *orig = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) orig[i] = rand();

    /* Radix base 256 (unsigned) */
    {
        unsigned int *w = malloc(n * sizeof(unsigned int));
        for (int i = 0; i < n; i++) w[i] = (unsigned int)orig[i];
        clock_t s = clock();
        radix_sort_base256(w, n);
        clock_t e = clock();
        double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;
        /* Verify */
        int ok = 1;
        for (int i = 1; i < n; i++) if (w[i-1] > w[i]) { ok = 0; break; }
        printf("  Radix base 256:  %10.0f us  %s\n", us, ok ? "OK" : "FAIL");
        free(w);
    }

    /* Radix signed */
    {
        int *w = malloc(n * sizeof(int));
        memcpy(w, orig, n * sizeof(int));
        /* Add some negatives */
        for (int i = 0; i < n / 2; i++) w[i] = -w[i];
        clock_t s = clock();
        radix_sort_signed(w, n);
        clock_t e = clock();
        double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;
        printf("  Radix signed:    %10.0f us  %s\n", us, is_sorted(w, n) ? "OK" : "FAIL");
        free(w);
    }

    /* qsort */
    {
        int *w = malloc(n * sizeof(int));
        memcpy(w, orig, n * sizeof(int));
        clock_t s = clock();
        qsort(w, n, sizeof(int), cmp_int);
        clock_t e = clock();
        double us = (double)(e - s) / CLOCKS_PER_SEC * 1e6;
        printf("  qsort:           %10.0f us  %s\n", us, is_sorted(w, n) ? "OK" : "FAIL");
        free(w);
    }

    printf("\n");
    free(orig);
}

/* ── Demo 3: Base comparison ─────────────────────────────── */

void demo_base_comparison(int n) {
    printf("=== Demo 3: Different bases, n=%d ===\n\n", n);
    printf("%8s %6s %10s\n", "Base", "Passes", "Time(us)");
    printf("%8s %6s %10s\n", "────────", "──────", "──────────");

    unsigned int *orig = malloc(n * sizeof(unsigned int));
    for (int i = 0; i < n; i++) orig[i] = (unsigned int)rand();

    /* Base 2 (bit by bit) — only do first few for timing */
    {
        unsigned int *w = malloc(n * sizeof(unsigned int));
        unsigned int *out = malloc(n * sizeof(unsigned int));
        memcpy(w, orig, n * sizeof(unsigned int));
        clock_t s = clock();
        for (int bit = 0; bit < 32; bit++) {
            int count[2] = {0};
            for (int i = 0; i < n; i++) count[(w[i] >> bit) & 1]++;
            count[1] += count[0];
            for (int i = n - 1; i >= 0; i--) {
                int b = (w[i] >> bit) & 1;
                count[b]--;
                out[count[b]] = w[i];
            }
            memcpy(w, out, n * sizeof(unsigned int));
        }
        clock_t e = clock();
        printf("%8d %6d %10.0f\n", 2, 32, (double)(e-s)/CLOCKS_PER_SEC*1e6);
        free(w); free(out);
    }

    /* Base 16 (nibble) */
    {
        unsigned int *w = malloc(n * sizeof(unsigned int));
        unsigned int *out = malloc(n * sizeof(unsigned int));
        memcpy(w, orig, n * sizeof(unsigned int));
        clock_t s = clock();
        for (int nib = 0; nib < 8; nib++) {
            int shift = nib * 4;
            int count[16] = {0};
            for (int i = 0; i < n; i++) count[(w[i] >> shift) & 0xF]++;
            for (int v = 1; v < 16; v++) count[v] += count[v-1];
            for (int i = n-1; i >= 0; i--) {
                int d = (w[i] >> shift) & 0xF;
                count[d]--;
                out[count[d]] = w[i];
            }
            memcpy(w, out, n * sizeof(unsigned int));
        }
        clock_t e = clock();
        printf("%8d %6d %10.0f\n", 16, 8, (double)(e-s)/CLOCKS_PER_SEC*1e6);
        free(w); free(out);
    }

    /* Base 256 (byte) */
    {
        unsigned int *w = malloc(n * sizeof(unsigned int));
        memcpy(w, orig, n * sizeof(unsigned int));
        clock_t s = clock();
        radix_sort_base256(w, n);
        clock_t e = clock();
        printf("%8d %6d %10.0f\n", 256, 4, (double)(e-s)/CLOCKS_PER_SEC*1e6);
        free(w);
    }

    /* Base 65536 (16-bit) */
    {
        unsigned int *w = malloc(n * sizeof(unsigned int));
        unsigned int *out = malloc(n * sizeof(unsigned int));
        memcpy(w, orig, n * sizeof(unsigned int));
        clock_t s = clock();
        for (int half = 0; half < 2; half++) {
            int shift = half * 16;
            int count[65536] = {0};
            for (int i = 0; i < n; i++) count[(w[i] >> shift) & 0xFFFF]++;
            for (int v = 1; v < 65536; v++) count[v] += count[v-1];
            for (int i = n-1; i >= 0; i--) {
                int d = (w[i] >> shift) & 0xFFFF;
                count[d]--;
                out[count[d]] = w[i];
            }
            memcpy(w, out, n * sizeof(unsigned int));
        }
        clock_t e = clock();
        printf("%8d %6d %10.0f\n", 65536, 2, (double)(e-s)/CLOCKS_PER_SEC*1e6);
        free(w); free(out);
    }

    printf("\n");
    free(orig);
}

/* ── Demo 4: LSD vs MSD ─────────────────────────────────── */

void demo_lsd_vs_msd(int n) {
    printf("=== Demo 4: LSD vs MSD, n=%d ===\n\n", n);

    int *orig = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) orig[i] = rand() % 1000000;

    int *w1 = malloc(n * sizeof(int));
    int *w2 = malloc(n * sizeof(int));
    memcpy(w1, orig, n * sizeof(int));
    memcpy(w2, orig, n * sizeof(int));

    clock_t s1 = clock();
    radix_sort_signed(w1, n);
    clock_t e1 = clock();

    clock_t s2 = clock();
    radix_sort_msd(w2, n);
    clock_t e2 = clock();

    double us1 = (double)(e1-s1)/CLOCKS_PER_SEC*1e6;
    double us2 = (double)(e2-s2)/CLOCKS_PER_SEC*1e6;

    printf("  LSD (base 256): %10.0f us  %s\n", us1, is_sorted(w1, n) ? "OK" : "FAIL");
    printf("  MSD (base 256): %10.0f us  %s\n", us2, is_sorted(w2, n) ? "OK" : "FAIL");
    printf("\n");

    free(orig); free(w1); free(w2);
}

/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║          Radix Sort — Complete Analysis           ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    demo_trace();
    demo_base_comparison(100000);
    demo_base256(100000);
    demo_base256(1000000);
    demo_lsd_vs_msd(100000);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -o radix_sort radix_sort.c
./radix_sort
```

---

## Programa completo en Rust

```rust
use std::time::Instant;

/* ── LSD Radix sort — base 10 (didactic) ────────────────── */

fn radix_sort_base10(arr: &mut [u32]) {
    if arr.is_empty() { return; }
    let max_val = *arr.iter().max().unwrap();
    let mut output = vec![0u32; arr.len()];
    let mut exp = 1u32;

    while max_val / exp > 0 {
        let mut count = [0usize; 10];
        for &x in arr.iter() {
            count[((x / exp) % 10) as usize] += 1;
        }
        for v in 1..10 {
            count[v] += count[v - 1];
        }
        for i in (0..arr.len()).rev() {
            let digit = ((arr[i] / exp) % 10) as usize;
            count[digit] -= 1;
            output[count[digit]] = arr[i];
        }
        arr.copy_from_slice(&output);
        exp *= 10;
    }
}

/* ── LSD Radix sort — base 256 (practical) ──────────────── */

fn radix_sort_base256(arr: &mut [u32]) {
    let n = arr.len();
    if n <= 1 { return; }
    let mut output = vec![0u32; n];

    for byte_idx in 0..4u32 {
        let shift = byte_idx * 8;
        let mut count = [0usize; 256];

        for &x in arr.iter() {
            count[((x >> shift) & 0xFF) as usize] += 1;
        }

        /* Skip if all same byte */
        let mut skip = false;
        for &c in &count {
            if c == n { skip = true; break; }
        }
        if skip { continue; }

        for v in 1..256 {
            count[v] += count[v - 1];
        }
        for i in (0..n).rev() {
            let byte_val = ((arr[i] >> shift) & 0xFF) as usize;
            count[byte_val] -= 1;
            output[count[byte_val]] = arr[i];
        }
        arr.copy_from_slice(&output);
    }
}

/* ── LSD Radix sort — signed i32 ────────────────────────── */

fn radix_sort_signed(arr: &mut [i32]) {
    let n = arr.len();
    if n <= 1 { return; }

    /* Reinterpret as u32, flip sign bit */
    let ua: &mut [u32] = unsafe {
        std::slice::from_raw_parts_mut(arr.as_mut_ptr() as *mut u32, n)
    };
    for x in ua.iter_mut() { *x ^= 0x80000000; }

    radix_sort_base256(ua);

    for x in ua.iter_mut() { *x ^= 0x80000000; }
}

/* ── Safe signed version (no unsafe) ─────────────────────── */

fn radix_sort_signed_safe(arr: &mut [i32]) {
    let n = arr.len();
    if n <= 1 { return; }
    let mut output = vec![0i32; n];

    for byte_idx in 0..4u32 {
        let shift = byte_idx * 8;
        let mut count = [0usize; 256];

        for &x in arr.iter() {
            /* Convert to unsigned-like ordering by flipping sign bit */
            let ux = (x as u32) ^ 0x80000000;
            count[((ux >> shift) & 0xFF) as usize] += 1;
        }

        let mut skip = false;
        for &c in &count { if c == n { skip = true; break; } }
        if skip { continue; }

        for v in 1..256 { count[v] += count[v - 1]; }

        for i in (0..n).rev() {
            let ux = (arr[i] as u32) ^ 0x80000000;
            let byte_val = ((ux >> shift) & 0xFF) as usize;
            count[byte_val] -= 1;
            output[count[byte_val]] = arr[i];
        }
        arr.copy_from_slice(&output);
    }
}

/* ── Input helpers ───────────────────────────────────────── */

fn fill_random_u32(n: usize) -> Vec<u32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xFEED_FACE).hash(&mut h);
        h.finish() as u32
    }).collect()
}

fn fill_random_i32(n: usize) -> Vec<i32> {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    (0..n).map(|i| {
        let mut h = DefaultHasher::new();
        (i as u64 ^ 0xCAFE_BABE).hash(&mut h);
        h.finish() as i32
    }).collect()
}

fn is_sorted_i32(a: &[i32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }
fn is_sorted_u32(a: &[u32]) -> bool { a.windows(2).all(|w| w[0] <= w[1]) }

/* ── Demo 1: Base 10 trace ───────────────────────────────── */

fn demo_trace() {
    println!("=== Demo 1: LSD Radix sort trace (base 10) ===\n");

    let mut arr: Vec<u32> = vec![170, 45, 75, 90, 802, 24, 2, 66];
    println!("Input: {:?}\n", arr);

    let max_val = *arr.iter().max().unwrap();
    let mut output = vec![0u32; arr.len()];
    let mut exp = 1u32;
    let mut pass = 0;

    while max_val / exp > 0 {
        pass += 1;
        let mut count = [0usize; 10];

        println!("--- Pass {}: digit position {} (÷{} mod 10) ---", pass, pass - 1, exp);
        print!("  Digits: ");
        for &x in &arr { print!("{}→{}  ", x, (x / exp) % 10); }
        println!();

        for &x in &arr { count[((x / exp) % 10) as usize] += 1; }
        for v in 1..10 { count[v] += count[v - 1]; }
        for i in (0..arr.len()).rev() {
            let d = ((arr[i] / exp) % 10) as usize;
            count[d] -= 1;
            output[count[d]] = arr[i];
        }
        arr.copy_from_slice(&output);
        println!("  Result: {:?}\n", arr);
        exp *= 10;
    }
}

/* ── Demo 2: Base 256 vs stdlib ──────────────────────────── */

fn demo_vs_stdlib(n: usize) {
    println!("=== Demo 2: Radix sort vs stdlib, n={} ===\n", n);

    /* Unsigned */
    {
        let orig = fill_random_u32(n);
        let mut w1 = orig.clone();
        let start = Instant::now();
        radix_sort_base256(&mut w1);
        let t1 = start.elapsed().as_micros();

        let mut w2 = orig.clone();
        let start = Instant::now();
        w2.sort_unstable();
        let t2 = start.elapsed().as_micros();

        println!("  u32 radix 256:   {:>10} us  {}", t1, if is_sorted_u32(&w1) { "OK" } else { "FAIL" });
        println!("  u32 sort_unst:   {:>10} us  {}", t2, if is_sorted_u32(&w2) { "OK" } else { "FAIL" });
    }

    /* Signed */
    {
        let orig = fill_random_i32(n);
        let mut w1 = orig.clone();
        let start = Instant::now();
        radix_sort_signed_safe(&mut w1);
        let t1 = start.elapsed().as_micros();

        let mut w2 = orig.clone();
        let start = Instant::now();
        w2.sort_unstable();
        let t2 = start.elapsed().as_micros();

        println!("  i32 radix 256:   {:>10} us  {}", t1, if is_sorted_i32(&w1) { "OK" } else { "FAIL" });
        println!("  i32 sort_unst:   {:>10} us  {}", t2, if is_sorted_i32(&w2) { "OK" } else { "FAIL" });
    }
    println!();
}

/* ── Demo 3: Different bases ─────────────────────────────── */

fn demo_bases(n: usize) {
    println!("=== Demo 3: Different bases, n={} ===\n", n);
    println!("{:>8} {:>6} {:>10}", "Base", "Passes", "Time(us)");
    println!("{:>8} {:>6} {:>10}", "────────", "──────", "──────────");

    let orig = fill_random_u32(n);

    /* Base 2 */
    {
        let mut w = orig.clone();
        let mut out = vec![0u32; n];
        let start = Instant::now();
        for bit in 0..32u32 {
            let mut count = [0usize; 2];
            for &x in &w { count[((x >> bit) & 1) as usize] += 1; }
            count[1] += count[0];
            for i in (0..n).rev() {
                let b = ((w[i] >> bit) & 1) as usize;
                count[b] -= 1;
                out[count[b]] = w[i];
            }
            w.copy_from_slice(&out);
        }
        let elapsed = start.elapsed().as_micros();
        println!("{:>8} {:>6} {:>10}", 2, 32, elapsed);
    }

    /* Base 16 */
    {
        let mut w = orig.clone();
        let mut out = vec![0u32; n];
        let start = Instant::now();
        for nib in 0..8u32 {
            let shift = nib * 4;
            let mut count = [0usize; 16];
            for &x in &w { count[((x >> shift) & 0xF) as usize] += 1; }
            for v in 1..16 { count[v] += count[v-1]; }
            for i in (0..n).rev() {
                let d = ((w[i] >> shift) & 0xF) as usize;
                count[d] -= 1;
                out[count[d]] = w[i];
            }
            w.copy_from_slice(&out);
        }
        let elapsed = start.elapsed().as_micros();
        println!("{:>8} {:>6} {:>10}", 16, 8, elapsed);
    }

    /* Base 256 */
    {
        let mut w = orig.clone();
        let start = Instant::now();
        radix_sort_base256(&mut w);
        let elapsed = start.elapsed().as_micros();
        println!("{:>8} {:>6} {:>10}", 256, 4, elapsed);
    }

    /* Base 65536 */
    {
        let mut w = orig.clone();
        let mut out = vec![0u32; n];
        let start = Instant::now();
        for half in 0..2u32 {
            let shift = half * 16;
            let mut count = vec![0usize; 65536];
            for &x in &w { count[((x >> shift) & 0xFFFF) as usize] += 1; }
            for v in 1..65536 { count[v] += count[v-1]; }
            for i in (0..n).rev() {
                let d = ((w[i] >> shift) & 0xFFFF) as usize;
                count[d] -= 1;
                out[count[d]] = w[i];
            }
            w.copy_from_slice(&out);
        }
        let elapsed = start.elapsed().as_micros();
        println!("{:>8} {:>6} {:>10}", 65536, 2, elapsed);
    }
    println!();
}

/* ── Demo 4: Signed with negatives ───────────────────────── */

fn demo_signed() {
    println!("=== Demo 4: Signed integers ===\n");
    let mut arr = vec![-42, 17, -3, 0, 100, -100, 55, -1, 8, -77];
    println!("Input:  {:?}", arr);
    radix_sort_signed_safe(&mut arr);
    println!("Output: {:?}", arr);
    println!("{}\n", if is_sorted_i32(&arr) { "OK" } else { "FAIL" });
}

/* ── Main ────────────────────────────────────────────────── */

fn main() {
    println!("╔══════════════════════════════════════════════════╗");
    println!("║          Radix Sort — Complete Analysis           ║");
    println!("╚══════════════════════════════════════════════════╝\n");

    demo_trace();
    demo_signed();
    demo_bases(100_000);
    demo_vs_stdlib(100_000);
    demo_vs_stdlib(1_000_000);
}
```

### Compilar y ejecutar

```bash
rustc -O -o radix_sort_rs radix_sort.rs
./radix_sort_rs
```

---

## Radix sort vs comparison sorts

| Propiedad | Radix sort (LSD, base 256) | Quicksort (pdqsort) | Merge sort |
|-----------|--------------------------|--------------------|-----------|
| Tiempo (32-bit int) | $O(4n)$ | $O(1.39\, n \log_2 n)$ | $O(n \log_2 n)$ |
| Para $n = 10^6$ | $\sim 4 \times 10^6$ ops | $\sim 2.8 \times 10^7$ ops | $\sim 2 \times 10^7$ ops |
| Espacio | $O(n + 256)$ | $O(\log n)$ | $O(n)$ |
| Estable | **Sí** (LSD) | No | **Sí** |
| Aplicable a | Enteros, floats, strings fijas | Cualquier tipo comparable | Cualquier tipo comparable |
| Cache | 4 pasadas secuenciales | 1 pasada con partición | Buffer + merge |

Para enteros de 32 bits con $n > 10000$, radix sort base 256 es
generalmente **2-4x más rápido** que quicksort. La ventaja crece
con $n$ porque radix sort es estrictamente $O(n)$ mientras quicksort
es $O(n \log n)$.

La limitación: radix sort solo funciona con datos que tienen una
representación como secuencia de dígitos/bytes. No funciona para
tipos con comparadores arbitrarios.

---

## Ejercicios

### Ejercicio 1 — Traza base 10

Aplica LSD radix sort (base 10) a `[329, 457, 657, 839, 436, 720, 355]`.
Muestra el array después de cada pasada.

<details><summary>Predicción</summary>

Pasada 1 (unidades): dígitos = 9,7,7,9,6,0,5
Resultado: `[720, 355, 436, 457, 657, 329, 839]`

Pasada 2 (decenas): dígitos = 2,5,3,5,5,2,3
Resultado: `[720, 329, 436, 839, 355, 457, 657]`

Pasada 3 (centenas): dígitos = 7,3,4,8,3,4,6
Resultado: `[329, 355, 436, 457, 657, 720, 839]` ✓

3 pasadas porque el máximo (839) tiene 3 dígitos.

</details>

### Ejercicio 2 — Base 256 vs base 10

Compara radix sort base 10 y base 256 para $n = 10^6$ enteros de
32 bits aleatorios. Mide pasadas y tiempo.

<details><summary>Predicción</summary>

Base 10: máximo $\sim 2^{32} = 4.29 \times 10^9$ tiene 10 dígitos →
10 pasadas. Cada pasada: counting sort con $k = 9$ → muy rápido pero
son 10 pasadas.

Base 256: 4 bytes → 4 pasadas. Cada pasada: counting sort con
$k = 255$ → un poco más de trabajo por pasada pero muchas menos
pasadas.

Base 256 debería ser **~2-3x más rápido** que base 10. Las 4 pasadas
con $k = 255$ son más eficientes que 10 pasadas con $k = 9$ porque
el overhead fijo por pasada (recorrer $n$ elementos, copiar resultados)
domina sobre el costo del counting sort.

</details>

### Ejercicio 3 — Skip pass optimization

Implementa la optimización de "saltar pasada": si todos los elementos
tienen el mismo byte en la posición $d$, no es necesario hacer la
pasada. Mide el impacto para datos en $[0, 255]$ (solo el byte 0
varía).

<details><summary>Predicción</summary>

Para datos en $[0, 255]$: bytes 1, 2, 3 son siempre 0. Con la
optimización, solo se ejecuta 1 pasada (byte 0) en lugar de 4.
Speedup: ~3.5x (las 3 pasadas innecesarias son puro overhead).

Para datos en $[0, 65535]$: bytes 2, 3 son siempre 0. Solo 2 pasadas.
Speedup: ~1.8x.

Para datos aleatorios de 32 bits: los 4 bytes varían, ninguna pasada
se salta. Speedup: 0% (solo overhead de verificar si se puede saltar).

La verificación cuesta $O(k)$ por pasada (recorrer `count` buscando
si algún bucket tiene $n$ elementos), lo cual es negligible.

</details>

### Ejercicio 4 — Radix sort para signed int

Implementa radix sort para `int` (con negativos) usando la técnica
del flip del MSB. Verifica con arrays que mezclan positivos y
negativos.

<details><summary>Predicción</summary>

Test: `[-5, 3, -1, 0, 7, -3, 2]`.
Después de flip MSB: los negativos (MSB=1) se convierten en MSB=0
(valores pequeños) y los positivos (MSB=0) se convierten en MSB=1
(valores grandes).

Radix sort ordena por representación unsigned. Después de reflip,
el orden es correcto: `[-5, -3, -1, 0, 2, 3, 7]`.

El flip solo afecta 1 bit y se hace en $O(n)$ antes y después del
sort. No cambia la complejidad ni el rendimiento de forma medible.

</details>

### Ejercicio 5 — Radix sort vs qsort para diferentes n

Compara radix sort (base 256) vs `qsort` para
$n \in \{10^3, 10^4, 10^5, 10^6, 10^7\}$. ¿En qué $n$ radix sort
empieza a ganar?

<details><summary>Predicción</summary>

| $n$ | Radix (us) | qsort (us) | Ganador |
|-----|-----------|------------|---------|
| $10^3$ | ~10 | ~30 | Radix |
| $10^4$ | ~80 | ~400 | Radix |
| $10^5$ | ~800 | ~5000 | Radix |
| $10^6$ | ~8000 | ~60000 | Radix (~7x) |
| $10^7$ | ~80000 | ~700000 | Radix (~9x) |

Radix sort debería ganar para **todo** $n$ con enteros de 32 bits,
pero la ventaja crece con $n$ porque radix es $O(n)$ y qsort es
$O(n \log n)$. Para $n = 10^7$, $\log_2 n \approx 23$, así que
qsort hace ~23x más "unidades de trabajo" que radix sort, pero cada
pasada de radix sort tiene más overhead, así que la ventaja real es
~8-10x.

Para $n$ muy pequeño ($< 100$), `qsort` con insertion sort cutoff
puede ganar por menor overhead de setup.

</details>

### Ejercicio 6 — MSD radix sort para strings

Implementa MSD radix sort para strings ASCII. Cada "dígito" es un
carácter (base 128). Compara con `qsort` + `strcmp`.

<details><summary>Predicción</summary>

MSD radix sort para strings tiene complejidad $O(n \cdot L)$ donde
$L$ es la longitud media de las strings. `qsort` + `strcmp` tiene
$O(n \log n \cdot L)$ — el factor $\log n$ extra.

Para $n = 100000$ strings de longitud ~20:
- MSD radix: ~$100000 \times 20 = 2M$ operaciones de byte.
- qsort+strcmp: ~$100000 \times 17 \times 20 = 34M$ operaciones.

MSD radix debería ser ~10x más rápido. Pero MSD radix tiene overhead
de recursión (un nivel por carácter, hasta $L$ niveles) y mala
localidad de cache para strings cortas dispersas.

En la práctica, MSD radix sort gana para colecciones grandes de
strings (ej: suffix array construction) pero no para colecciones
pequeñas.

</details>

### Ejercicio 7 — Radix sort para 64-bit integers

Adapta radix sort base 256 para `uint64_t` (8 pasadas en lugar de 4).
¿Sigue siendo más rápido que quicksort?

<details><summary>Predicción</summary>

Con 8 pasadas, radix sort hace ~$8n$ operaciones. Quicksort para
$n = 10^6$ hace ~$1.39 \times 10^6 \times 20 = 2.8 \times 10^7$
operaciones ($\log_2 10^6 \approx 20$).

Ratio: $8n / 1.39n \log_2 n = 8 / (1.39 \times 20) = 8/27.8 = 0.29$.
Radix sort hace ~3.5x menos operaciones.

Pero con 8 pasadas, la constante de overhead es mayor (8 recorridos
completos del array, 8 copias). En la práctica, radix sort de 64 bits
es **~1.5-2x más rápido** que quicksort para $n > 10^5$, pero la
ventaja es menor que para 32 bits.

Para $n < 10^4$, quicksort puede ganar porque el overhead de 8 pasadas
domina.

</details>

### Ejercicio 8 — Estabilidad de LSD en acción

Ordena un array de pares `(apellido_hash, nombre_hash)` donde los
hashes están en $[0, 999]$. Usa radix sort base 10 y verifica que
las personas con el mismo apellido mantienen el orden por nombre.

<details><summary>Predicción</summary>

LSD radix sort procesa primero el dígito menos significativo de
`nombre_hash`, luego los siguientes, y finalmente los dígitos de
`apellido_hash`. La estabilidad de counting sort garantiza que al
terminar de ordenar por apellido, los nombres dentro de cada grupo
de apellido están en orden.

Esto es equivalente a hacer sort estable por nombre y luego sort
estable por apellido — el mismo principio que el sort multi-campo
del ejercicio 9 de T06.

Para verificar: dentro de cada grupo con el mismo `apellido_hash`,
los `nombre_hash` deben estar en orden ascendente.

</details>

### Ejercicio 9 — Counting sort como caso especial

Demuestra que counting sort es radix sort con $d = 1$ (un solo dígito).
Implementa una función genérica que reciba $d$ y se comporte como
counting sort cuando $d = 1$.

<details><summary>Predicción</summary>

Counting sort para valores en $[0, k]$ es exactamente LSD radix sort
con base $k + 1$ y $d = 1$: una sola pasada de counting sort que
ordena por el único "dígito" (el valor completo).

La función genérica:
```c
void sort(int *arr, int n, int base, int digits) {
    for (int d = 0; d < digits; d++)
        counting_sort_by_digit(arr, n, d, base);
}
```
- `sort(arr, n, k+1, 1)` = counting sort con rango $[0, k]$.
- `sort(arr, n, 10, 3)` = radix sort base 10 para 3 dígitos.
- `sort(arr, n, 256, 4)` = radix sort base 256 para 32 bits.

</details>

### Ejercicio 10 — Radix sort vs pdqsort escalabilidad

Mide radix sort (base 256) y `.sort_unstable()` de Rust para
$n = 10^4, 10^5, 10^6, 10^7$ y calcula el ratio de tiempo.
Grafica ratio vs $n$.

<details><summary>Predicción</summary>

| $n$ | Radix/pdqsort ratio | Razón |
|-----|--------------------|----|
| $10^4$ | ~0.5-0.8 | Radix empieza a ganar |
| $10^5$ | ~0.3-0.5 | Ventaja clara |
| $10^6$ | ~0.2-0.3 | ~3-5x más rápido |
| $10^7$ | ~0.15-0.25 | ~4-7x más rápido |

El ratio debería **decrecer** (radix se vuelve relativamente más
rápido) al crecer $n$, porque radix es $O(n)$ y pdqsort es
$O(n \log n)$. Para cada duplicación de $n$, pdqsort crece un poco
más que linealmente mientras radix crece linealmente.

La pendiente del ratio converge a $c / \log_2 n$ para alguna
constante $c$, acercándose asintóticamente a 0 pero muy lentamente.

</details>
