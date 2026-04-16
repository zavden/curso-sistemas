# Búsqueda por interpolación

## Objetivo

Dominar la búsqueda por interpolación, que estima la posición del
valor buscado en función de su magnitud:

- **Idea**: en lugar de ir siempre al medio, estimar dónde
  "debería estar" el valor, como al buscar en un diccionario.
- Complejidad $O(\log \log n)$ para datos **uniformemente
  distribuidos**.
- Peor caso $O(n)$ para distribuciones adversariales.
- Cuándo usarla y cuándo preferir búsqueda binaria.

---

## La intuición

Cuando buscamos una palabra en un diccionario físico, no abrimos
por la mitad cada vez. Si buscamos "zapato", abrimos cerca del final.
Si buscamos "ángulo", abrimos cerca del inicio. Estimamos la posición
proporcionalmente al valor.

La búsqueda binaria ignora los **valores** de los elementos — siempre
divide por la mitad, sin importar si el buscado es mucho mayor o
mucho menor que el punto medio. La búsqueda por interpolación usa
los valores para hacer una estimación más inteligente.

---

## El algoritmo

### Fórmula de interpolación

En la búsqueda binaria, el punto medio es:

$$\text{mid} = \text{lo} + \frac{\text{hi} - \text{lo}}{2}$$

En la búsqueda por interpolación, reemplazamos la fracción $\frac{1}{2}$
por una **proporción** basada en el valor buscado:

$$\text{pos} = \text{lo} + \left\lfloor \frac{(\text{target} - \text{arr[lo]}) \times (\text{hi} - \text{lo})}{\text{arr[hi]} - \text{arr[lo]}} \right\rfloor$$

Esta fórmula asume que los valores están distribuidos linealmente
entre `arr[lo]` y `arr[hi]`. Si `target` está al 30% del rango de
valores, la posición estimada estará al 30% del rango de índices.

### Pseudocódigo

```
INTERPOLATION_SEARCH(arr, n, target):
    lo = 0
    hi = n - 1

    mientras lo <= hi Y arr[lo] <= target <= arr[hi]:
        si arr[lo] == arr[hi]:
            si arr[lo] == target: retornar lo
            sino: retornar -1

        pos = lo + ((target - arr[lo]) * (hi - lo)) / (arr[hi] - arr[lo])

        si arr[pos] == target:
            retornar pos
        si arr[pos] < target:
            lo = pos + 1
        sino:
            hi = pos - 1

    retornar -1
```

### Condiciones de guarda

Las condiciones extra respecto a la búsqueda binaria son esenciales:

1. **`arr[lo] <= target <= arr[hi]`**: si el target está fuera del
   rango, no puede estar en `arr[lo..hi]`. Terminar inmediatamente.

2. **`arr[lo] == arr[hi]`**: si los extremos son iguales, la fórmula
   tiene denominador 0 (división por cero). Verificar si el valor
   coincide.

Sin estas guardas, la fórmula puede producir un índice negativo o
mayor que `n`, causando acceso fuera del array.

---

## Traza detallada

### Datos uniformes

Buscando `44` en `[10, 20, 30, 40, 50, 60, 70, 80, 90, 100]`
($n = 10$, distribución perfectamente uniforme):

```
Paso 1: lo=0, hi=9
  arr[lo]=10, arr[hi]=100, target=44
  pos = 0 + (44-10)*(9-0)/(100-10)
      = 0 + 34*9/90
      = 0 + 306/90
      = 0 + 3 = 3
  arr[3]=40 < 44 → lo=4

Paso 2: lo=4, hi=9
  arr[lo]=50, arr[hi]=100, target=44
  arr[lo]=50 > target=44 → condición arr[lo] <= target falla
  → retornar -1

Total: 2 pasos (no encontrado, correcto)
```

Buscando `50`:

```
Paso 1: lo=0, hi=9
  arr[lo]=10, arr[hi]=100, target=50
  pos = 0 + (50-10)*(9-0)/(100-10)
      = 0 + 40*9/90
      = 0 + 4 = 4
  arr[4]=50 == 50 → encontrado en posición 4

Total: 1 paso ✓
```

Con búsqueda binaria, encontrar 50 requeriría:

```
mid=4, arr[4]=50 == 50 → encontrado (1 paso, coincidencia)
```

En este caso ambas dan 1 paso, pero para `target=90`:

```
Interpolación:
  pos = 0 + (90-10)*9/90 = 0 + 720/90 = 8
  arr[8]=90 → encontrado en 1 paso

Binaria:
  mid=4, arr[4]=50 < 90 → lo=5
  mid=7, arr[7]=80 < 90 → lo=8
  mid=8, arr[8]=90 → encontrado en 3 pasos
```

La interpolación "salta" directamente a la posición correcta cuando
los datos son uniformes.

### Datos no uniformes

Buscando `100` en `[1, 2, 3, 4, 5, 6, 7, 8, 9, 100]`
(distribución muy sesgada):

```
Paso 1: lo=0, hi=9
  arr[lo]=1, arr[hi]=100, target=100
  pos = 0 + (100-1)*(9-0)/(100-1) = 0 + 99*9/99 = 9
  arr[9]=100 == 100 → encontrado en 1 paso ✓
```

Buscando `5`:

```
Paso 1: lo=0, hi=9
  arr[lo]=1, arr[hi]=100, target=5
  pos = 0 + (5-1)*9/99 = 0 + 36/99 = 0
  arr[0]=1 < 5 → lo=1

Paso 2: lo=1, hi=9
  arr[lo]=2, arr[hi]=100, target=5
  pos = 1 + (5-2)*8/98 = 1 + 24/98 = 1
  arr[1]=2 < 5 → lo=2

Paso 3: lo=2, hi=9
  pos = 2 + (5-3)*7/97 = 2 + 14/97 = 2
  arr[2]=3 < 5 → lo=3

Paso 4: lo=3, hi=9
  pos = 3 + (5-4)*6/96 = 3 + 6/96 = 3
  arr[3]=4 < 5 → lo=4

Paso 5: lo=4, hi=9
  pos = 4 + (5-5)*5/95 = 4 + 0 = 4
  arr[4]=5 == 5 → encontrado
```

5 pasos — peor que búsqueda binaria (que tomaría ~3). El valor
extremo `100` sesga la estimación hacia el inicio, haciendo que el
algoritmo avance de uno en uno, degenerando a $O(n)$.

---

## Análisis de complejidad

### Caso promedio: $O(\log \log n)$

Para datos **uniformemente distribuidos**, la interpolación estima
la posición con un error proporcional a $\sqrt{n}$. Esto significa
que cada paso reduce el espacio de búsqueda de $n$ a $\sqrt{n}$:

$$n \to \sqrt{n} \to \sqrt{\sqrt{n}} \to \cdots$$

El número de pasos hasta llegar a 1 es el número de veces que se
puede tomar la raíz cuadrada de $n$:

$$\underbrace{\sqrt{\sqrt{\cdots\sqrt{n}}}}_{k \text{ veces}} = 1
\implies n^{1/2^k} = 1 \implies k = \log_2(\log_2 n)$$

Esto es $O(\log \log n)$. Para $n = 10^6$:

- Binaria: $\log_2(10^6) \approx 20$ pasos.
- Interpolación: $\log_2(\log_2(10^6)) \approx \log_2(20) \approx 4.3$ pasos.

La mejora es dramática para arrays grandes.

### Peor caso: $O(n)$

Para distribuciones adversariales (como el ejemplo con `[1,2,...,9,100]`),
la estimación es consistentemente incorrecta y el algoritmo avanza
de una posición a la vez, como búsqueda secuencial.

Ejemplo extremo: array con $n - 1$ valores iguales a 1 y un valor
$M$ al final. Para buscar 1, la fórmula siempre estima `pos = lo`,
y el algoritmo degrada a búsqueda secuencial.

### Costo por paso

Cada paso de interpolación es más costoso que uno de búsqueda binaria:

| Operación | Binaria | Interpolación |
|-----------|---------|---------------|
| Cálculo de posición | 1 suma, 1 shift | 1 resta, 1 multiplicación, 1 división |
| Comparaciones | 1-2 | 1-2 + verificaciones de guarda |
| Costo total por paso | ~3 ns | ~5-8 ns |

La división entera es la operación más costosa (~20-30 ciclos en x86
vs ~1 ciclo para shift). Esto hace que la interpolación necesite
significativamente **menos** pasos para compensar el mayor costo
por paso.

El punto de cruce: la interpolación solo es más rápida que la binaria
cuando $n$ es suficientemente grande (típicamente $n > 10^4$ a $10^5$)
y los datos son suficientemente uniformes.

---

## Variante: búsqueda por interpolación-secuencial

Una mejora práctica: cuando la interpolación acota el rango a un
subarray pequeño (digamos < 16 elementos), cambiar a búsqueda
**secuencial** en lugar de seguir interpolando. Esto evita el costo
de las divisiones para rangos pequeños:

```c
int interpolation_sequential(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;

    while (lo <= hi && arr[lo] <= target && target <= arr[hi]) {
        /* Cambiar a secuencial para rangos pequeños */
        if (hi - lo < 16) {
            for (int i = lo; i <= hi; i++) {
                if (arr[i] == target) return i;
                if (arr[i] > target)  return -1;
            }
            return -1;
        }

        if (arr[lo] == arr[hi]) {
            return (arr[lo] == target) ? lo : -1;
        }

        long long num = (long long)(target - arr[lo]) * (hi - lo);
        int pos = lo + (int)(num / (arr[hi] - arr[lo]));

        if (arr[pos] == target) return pos;
        if (arr[pos] < target)  lo = pos + 1;
        else                    hi = pos - 1;
    }

    return -1;
}
```

Nótese el cast a `long long` en la multiplicación: `(target - arr[lo]) * (hi - lo)`
puede desbordar `int` si los valores y el rango son grandes. El
mismo tipo de bug de overflow que en búsqueda binaria.

---

## Variante: interpolación cuadrática

En lugar de interpolar linealmente (asumiendo distribución uniforme),
se puede interpolar con una función cuadrática usando tres puntos
(lo, mid, hi). Esto mejora la estimación para distribuciones no
uniformes pero conocidas, a costa de más cálculos por paso.

En la práctica, esta variante rara vez se usa porque:

1. El costo extra por paso reduce la ventaja.
2. Si la distribución es conocida, se puede transformar los datos
   para hacerla uniforme antes de buscar.

---

## Cuándo usar interpolación vs binaria

| Criterio | Interpolación | Binaria |
|----------|---------------|---------|
| Distribución | Uniforme o casi-uniforme | Cualquiera |
| Tamaño del array | Grande ($n > 10^4$) | Cualquiera |
| Tipo de datos | Numéricos (para la fórmula) | Cualquiera con orden |
| Peor caso aceptable | No (O(n) inaceptable) | Sí ($O(\log n)$ garantizado) |
| Costo por paso tolerable | Sí (divisiones) | No importa |
| Acceso al disco | No (acceso aleatorio costoso) | Sí (pocos accesos) |

### Preferir interpolación cuando

- Los datos son **numéricos** y **uniformemente distribuidos**
  (IDs secuenciales, timestamps espaciados regularmente, datos
  sintéticos).
- El array es **muy grande** ($n > 10^5$) y está en memoria.
- Se realizan **muchas búsquedas** y la reducción de $\log n$ a
  $\log \log n$ es significativa.

### Preferir binaria cuando

- Los datos tienen **distribución desconocida o sesgada**.
- Se necesita una **garantía** de peor caso.
- El array está en **disco** (cada acceso es costoso, y la binaria
  minimiza accesos mientras la interpolación puede sobreestimar).
- Los datos no son numéricos (strings, structs) — la fórmula de
  interpolación no aplica.
- El array es **pequeño** ($n < 10^4$) — el overhead de la división
  domina sobre la ganancia en número de pasos.

---

## Programa completo en C

```c
/* interp_search.c — Búsqueda por interpolación */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* --- Búsqueda por interpolación --- */
int interpolation_search(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;

    while (lo <= hi && arr[lo] <= target && target <= arr[hi]) {
        if (arr[lo] == arr[hi])
            return (arr[lo] == target) ? lo : -1;

        long long num = (long long)(target - arr[lo]) * (hi - lo);
        int pos = lo + (int)(num / (arr[hi] - arr[lo]));

        if (arr[pos] == target) return pos;
        if (arr[pos] < target)  lo = pos + 1;
        else                    hi = pos - 1;
    }

    return -1;
}

/* --- Con contador de pasos --- */
int interp_search_counted(const int *arr, int n, int target, int *steps)
{
    int lo = 0, hi = n - 1;
    *steps = 0;

    while (lo <= hi && arr[lo] <= target && target <= arr[hi]) {
        (*steps)++;

        if (arr[lo] == arr[hi])
            return (arr[lo] == target) ? lo : -1;

        long long num = (long long)(target - arr[lo]) * (hi - lo);
        int pos = lo + (int)(num / (arr[hi] - arr[lo]));

        if (arr[pos] == target) return pos;
        if (arr[pos] < target)  lo = pos + 1;
        else                    hi = pos - 1;
    }

    return -1;
}

/* --- Búsqueda binaria con contador --- */
int binary_search_counted(const int *arr, int n, int target, int *steps)
{
    int lo = 0, hi = n - 1;
    *steps = 0;

    while (lo <= hi) {
        (*steps)++;
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target)  lo = mid + 1;
        else                    hi = mid - 1;
    }

    return -1;
}

/* --- Helpers --- */
void print_arr(const char *label, const int *arr, int n)
{
    printf("%s [", label);
    for (int i = 0; i < n; i++)
        printf("%s%d", i ? ", " : "", arr[i]);
    printf("]\n");
}

int main(void)
{
    /* Demo 1: Búsqueda por interpolación básica */
    printf("=== Demo 1: Interpolación básica ===\n");
    int arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    int n = 10;
    print_arr("Array (uniforme):", arr, n);

    int targets[] = {50, 90, 10, 44, 100, 55};
    for (int i = 0; i < 6; i++) {
        int idx = interpolation_search(arr, n, targets[i]);
        printf("Buscar %3d: %s",
               targets[i], idx >= 0 ? "encontrado" : "no encontrado");
        if (idx >= 0) printf(" en posición %d", idx);
        printf("\n");
    }

    /* Demo 2: Comparar pasos con binaria */
    printf("\n=== Demo 2: Pasos interpolación vs binaria ===\n");
    int uniform[100];
    for (int i = 0; i < 100; i++) uniform[i] = i * 10;

    printf("Array: [0, 10, 20, ..., 990] (n=100, uniforme)\n\n");
    printf("%-10s  %-12s  %-12s\n", "Target", "Interp", "Binaria");
    printf("──────────  ────────────  ────────────\n");

    int test_targets[] = {0, 150, 500, 730, 990, 555};
    for (int i = 0; i < 6; i++) {
        int si, sb;
        int ri = interp_search_counted(uniform, 100, test_targets[i], &si);
        int rb = binary_search_counted(uniform, 100, test_targets[i], &sb);
        printf("%-10d  %d pasos (%s)  %d pasos (%s)\n",
               test_targets[i],
               si, ri >= 0 ? "found" : "miss ",
               sb, rb >= 0 ? "found" : "miss ");
    }

    /* Demo 3: Datos no uniformes (peor caso) */
    printf("\n=== Demo 3: Datos no uniformes (peor caso) ===\n");
    int skewed[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 1000000};
    int ns = 10;
    print_arr("Array (sesgado):", skewed, ns);

    int skew_targets[] = {5, 9, 1000000};
    for (int i = 0; i < 3; i++) {
        int si, sb;
        interp_search_counted(skewed, ns, skew_targets[i], &si);
        binary_search_counted(skewed, ns, skew_targets[i], &sb);
        printf("Buscar %7d:  interp=%d pasos,  binaria=%d pasos\n",
               skew_targets[i], si, sb);
    }

    /* Demo 4: Benchmark con datos grandes */
    printf("\n=== Demo 4: Benchmark (10^6 elementos uniformes) ===\n");
    int N = 1000000;
    int *big = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) big[i] = i * 3;  /* uniforme */

    /* Contar pasos promedio */
    long total_interp = 0, total_binary = 0;
    int queries = 10000;
    for (int q = 0; q < queries; q++) {
        int target = (q * 7919) % (N * 3);  /* targets variados */
        int si, sb;
        interp_search_counted(big, N, target, &si);
        binary_search_counted(big, N, target, &sb);
        total_interp += si;
        total_binary += sb;
    }

    printf("Pasos promedio (%d búsquedas):\n", queries);
    printf("  Interpolación: %.2f pasos\n", (double)total_interp / queries);
    printf("  Binaria:       %.2f pasos\n", (double)total_binary / queries);
    printf("  Ratio:         %.1fx menos pasos con interpolación\n",
           (double)total_binary / total_interp);

    /* Tiempo */
    volatile int dummy = 0;
    int reps = 100000;

    clock_t t0 = clock();
    for (int r = 0; r < reps; r++)
        for (int q = 0; q < 10; q++)
            dummy += interpolation_search(big, N, (q * 99991) % (N * 3));
    clock_t t1 = clock();
    double interp_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    t0 = clock();
    for (int r = 0; r < reps; r++)
        for (int q = 0; q < 10; q++)
            dummy += binary_search_counted(big, N, (q * 99991) % (N * 3), &(int){0});
    t1 = clock();
    double binary_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    printf("\nTiempo (%d×10 búsquedas):\n", reps);
    printf("  Interpolación: %.2f ms\n", interp_ms);
    printf("  Binaria:       %.2f ms\n", binary_ms);
    printf("  Speedup:       %.2fx\n", binary_ms / interp_ms);

    /* Demo 5: Datos con duplicados */
    printf("\n=== Demo 5: Duplicados ===\n");
    int dups[] = {1, 3, 3, 3, 3, 5, 5, 7, 9, 9};
    int nd = 10;
    print_arr("Array:", dups, nd);

    int dup_targets[] = {3, 5, 4, 9};
    for (int i = 0; i < 4; i++) {
        int si;
        int idx = interp_search_counted(dups, nd, dup_targets[i], &si);
        printf("Buscar %d: %s (%d pasos)",
               dup_targets[i],
               idx >= 0 ? "encontrado" : "no encontrado", si);
        if (idx >= 0) printf(", posición %d", idx);
        printf("\n");
    }

    free(big);
    (void)dummy;

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -std=c11 -Wall -Wextra -o interp_search interp_search.c
./interp_search
```

### Salida esperada

```
=== Demo 1: Interpolación básica ===
Array (uniforme): [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
Buscar  50: encontrado en posición 4
Buscar  90: encontrado en posición 8
Buscar  10: encontrado en posición 0
Buscar  44: no encontrado
Buscar 100: encontrado en posición 9
Buscar  55: no encontrado

=== Demo 2: Pasos interpolación vs binaria ===
Array: [0, 10, 20, ..., 990] (n=100, uniforme)

Target      Interp        Binaria
──────────  ────────────  ────────────
0           1 pasos (found)  7 pasos (found)
150         1 pasos (found)  5 pasos (found)
500         1 pasos (found)  2 pasos (found)
730         1 pasos (found)  5 pasos (found)
990         1 pasos (found)  7 pasos (found)
555         2 pasos (miss )  7 pasos (miss )

=== Demo 3: Datos no uniformes (peor caso) ===
Array (sesgado): [1, 2, 3, 4, 5, 6, 7, 8, 9, 1000000]
Buscar       5:  interp=5 pasos,  binaria=3 pasos
Buscar       9:  interp=9 pasos,  binaria=4 pasos
Buscar 1000000:  interp=1 pasos,  binaria=4 pasos

=== Demo 4: Benchmark (10^6 elementos uniformes) ===
Pasos promedio (10000 búsquedas):
  Interpolación: ~2.50 pasos
  Binaria:       ~19.50 pasos
  Ratio:         ~7.8x menos pasos con interpolación

Tiempo (100000×10 búsquedas):
  Interpolación: ~45.00 ms
  Binaria:       ~80.00 ms
  Speedup:       ~1.78x

=== Demo 5: Duplicados ===
Array: [1, 3, 3, 3, 3, 5, 5, 7, 9, 9]
Buscar 3: encontrado, posición 1 (1 pasos)
Buscar 5: encontrado, posición 5 (1 pasos)
Buscar 4: no encontrado (1 pasos)
Buscar 9: encontrado, posición 9 (1 pasos)
```

---

## Programa completo en Rust

```rust
// interp_search.rs — Búsqueda por interpolación
use std::time::Instant;

fn interpolation_search(arr: &[i32], target: i32) -> Option<usize> {
    if arr.is_empty() { return None; }

    let mut lo: isize = 0;
    let mut hi: isize = arr.len() as isize - 1;

    while lo <= hi
        && arr[lo as usize] <= target
        && target <= arr[hi as usize]
    {
        if arr[lo as usize] == arr[hi as usize] {
            return if arr[lo as usize] == target { Some(lo as usize) } else { None };
        }

        let num = (target as i64 - arr[lo as usize] as i64) * (hi as i64 - lo as i64);
        let den = arr[hi as usize] as i64 - arr[lo as usize] as i64;
        let pos = lo as i64 + num / den;
        let pos = pos as usize;

        if arr[pos] == target {
            return Some(pos);
        } else if arr[pos] < target {
            lo = pos as isize + 1;
        } else {
            hi = pos as isize - 1;
        }
    }

    None
}

fn interp_search_steps(arr: &[i32], target: i32) -> (Option<usize>, u32) {
    if arr.is_empty() { return (None, 0); }

    let mut lo: isize = 0;
    let mut hi: isize = arr.len() as isize - 1;
    let mut steps: u32 = 0;

    while lo <= hi
        && arr[lo as usize] <= target
        && target <= arr[hi as usize]
    {
        steps += 1;

        if arr[lo as usize] == arr[hi as usize] {
            return if arr[lo as usize] == target {
                (Some(lo as usize), steps)
            } else {
                (None, steps)
            };
        }

        let num = (target as i64 - arr[lo as usize] as i64) * (hi as i64 - lo as i64);
        let den = arr[hi as usize] as i64 - arr[lo as usize] as i64;
        let pos = (lo as i64 + num / den) as usize;

        if arr[pos] == target {
            return (Some(pos), steps);
        } else if arr[pos] < target {
            lo = pos as isize + 1;
        } else {
            hi = pos as isize - 1;
        }
    }

    (None, steps)
}

fn binary_search_steps(arr: &[i32], target: i32) -> (Option<usize>, u32) {
    let (mut lo, mut hi) = (0usize, arr.len());
    let mut steps: u32 = 0;

    while lo < hi {
        steps += 1;
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal   => return (Some(mid), steps),
            std::cmp::Ordering::Less    => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }

    (None, steps)
}

fn main() {
    // Demo 1: Básica
    println!("=== Demo 1: Interpolación básica ===");
    let arr = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
    println!("Array: {:?}", arr);

    for &t in &[50, 90, 10, 44, 100, 55] {
        match interpolation_search(&arr, t) {
            Some(i) => println!("Buscar {:>3}: encontrado en posición {}", t, i),
            None    => println!("Buscar {:>3}: no encontrado", t),
        }
    }

    // Demo 2: Comparar pasos
    println!("\n=== Demo 2: Pasos interpolación vs binaria ===");
    let uniform: Vec<i32> = (0..100).map(|i| i * 10).collect();
    println!("Array: [0, 10, 20, ..., 990] (n=100)\n");
    println!("{:<10}  {:<15}  {:<15}", "Target", "Interpolación", "Binaria");
    println!("──────────  ───────────────  ───────────────");

    for &t in &[0, 150, 500, 730, 990, 555] {
        let (ri, si) = interp_search_steps(&uniform, t);
        let (rb, sb) = binary_search_steps(&uniform, t);
        println!("{:<10}  {} pasos ({})   {} pasos ({})",
                 t,
                 si, if ri.is_some() { "found" } else { "miss " },
                 sb, if rb.is_some() { "found" } else { "miss " });
    }

    // Demo 3: Datos sesgados
    println!("\n=== Demo 3: Datos no uniformes ===");
    let skewed = [1, 2, 3, 4, 5, 6, 7, 8, 9, 1_000_000];
    println!("Array: {:?}", skewed);

    for &t in &[5, 9, 1_000_000] {
        let (_, si) = interp_search_steps(&skewed, t);
        let (_, sb) = binary_search_steps(&skewed, t);
        println!("Buscar {:>7}: interp={} pasos, binaria={} pasos", t, si, sb);
    }

    // Demo 4: Benchmark grande
    println!("\n=== Demo 4: Benchmark (10^6 elementos uniformes) ===");
    let n = 1_000_000usize;
    let big: Vec<i32> = (0..n as i32).map(|i| i * 3).collect();

    let mut total_interp: u64 = 0;
    let mut total_binary: u64 = 0;
    let queries = 10_000;

    for q in 0..queries {
        let target = ((q as i64 * 7919) % (n as i64 * 3)) as i32;
        let (_, si) = interp_search_steps(&big, target);
        let (_, sb) = binary_search_steps(&big, target);
        total_interp += si as u64;
        total_binary += sb as u64;
    }

    println!("Pasos promedio ({} búsquedas):", queries);
    println!("  Interpolación: {:.2}", total_interp as f64 / queries as f64);
    println!("  Binaria:       {:.2}", total_binary as f64 / queries as f64);
    println!("  Ratio:         {:.1}x menos pasos",
             total_binary as f64 / total_interp as f64);

    // Tiempo
    let reps = 100_000;
    let test_targets: Vec<i32> = (0..10).map(|q| ((q * 99991) % (n as i32 * 3))).collect();

    let start = Instant::now();
    let mut dummy = 0usize;
    for _ in 0..reps {
        for &t in &test_targets {
            dummy += interpolation_search(&big, t).unwrap_or(0);
        }
    }
    let interp_ms = start.elapsed().as_secs_f64() * 1000.0;

    let start = Instant::now();
    for _ in 0..reps {
        for &t in &test_targets {
            dummy += big.binary_search(&t).unwrap_or(0);
        }
    }
    let binary_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("\nTiempo ({}×10 búsquedas):", reps);
    println!("  Interpolación: {:.2} ms", interp_ms);
    println!("  Binaria:       {:.2} ms", binary_ms);
    println!("  Speedup:       {:.2}x", binary_ms / interp_ms);
    println!("(dummy={})", dummy);
}
```

### Compilar y ejecutar

```bash
rustc -O -o interp_search interp_search.rs
./interp_search
```

---

## Ejercicios

### Ejercicio 1 — Traza con datos uniformes

Traza `interpolation_search` para buscar `70` en
`[10, 20, 30, 40, 50, 60, 70, 80, 90, 100]`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
n=10, target=70
lo=0, hi=9

Paso 1:
  arr[lo]=10, arr[hi]=100
  pos = 0 + (70-10)*(9-0)/(100-10)
      = 0 + 60*9/90
      = 0 + 540/90
      = 0 + 6 = 6
  arr[6]=70 == 70 → encontrado en posición 6 ✓

Total: 1 paso
```

Para datos perfectamente uniformes, la fórmula calcula la posición
exacta en un solo paso. Esto es porque la distribución lineal
coincide exactamente con la suposición del algoritmo.

Con búsqueda binaria:

```
mid=4(50) < 70 → lo=5
mid=7(80) > 70 → hi=6
mid=5(60) < 70 → lo=6
mid=6(70) → encontrado en 4 pasos
```

Interpolación: 1 paso. Binaria: 4 pasos.

</details>

### Ejercicio 2 — Traza con datos sesgados

Traza `interpolation_search` para buscar `8` en
`[1, 2, 3, 4, 5, 6, 7, 8, 9, 1000]`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
n=10, target=8

Paso 1: lo=0, hi=9
  arr[0]=1, arr[9]=1000
  pos = 0 + (8-1)*(9-0)/(1000-1)
      = 0 + 7*9/999
      = 0 + 63/999
      = 0 + 0 = 0           (truncamiento entero)
  arr[0]=1 < 8 → lo=1

Paso 2: lo=1, hi=9
  arr[1]=2, arr[9]=1000
  pos = 1 + (8-2)*(9-1)/(1000-2)
      = 1 + 6*8/998
      = 1 + 48/998
      = 1 + 0 = 1
  arr[1]=2 < 8 → lo=2

Paso 3-7: (patrón similar, pos = lo cada vez)
  ...avanza de uno en uno...

Paso 7: lo=7, hi=9
  arr[7]=8, arr[9]=1000
  pos = 7 + (8-8)*(9-7)/(1000-8) = 7 + 0 = 7
  arr[7]=8 == 8 → encontrado ✓

Total: 7 pasos (peor que binaria con ~3 pasos)
```

El valor extremo 1000 hace que la fórmula siempre estime posiciones
muy cercanas a `lo`, avanzando paso a paso. Este es el caso $O(n)$.

</details>

### Ejercicio 3 — División por cero

¿Qué pasa si todos los elementos del array son iguales?

```c
int arr[] = {5, 5, 5, 5, 5};
interpolation_search(arr, 5, 5);
interpolation_search(arr, 5, 3);
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Buscar 5 en `[5, 5, 5, 5, 5]`**:

```
lo=0, hi=4
arr[lo]=5, arr[hi]=5
arr[lo] == arr[hi] → verificar: arr[lo]==target? 5==5? Sí → retornar 0
```

Retorna 0 (la primera posición). Sin la guarda `arr[lo] == arr[hi]`,
la fórmula calcularía `(5-5)*(4-0)/(5-5) = 0/0`, que es **división
por cero** — crash o comportamiento indefinido.

**Buscar 3 en `[5, 5, 5, 5, 5]`**:

```
lo=0, hi=4
arr[lo]=5 <= target=3? No (5 > 3)
→ condición del while falla → retornar -1
```

La guarda `arr[lo] <= target` detecta inmediatamente que 3 no puede
estar en un array donde el mínimo es 5.

La verificación `arr[lo] == arr[hi]` es **crítica** para la
corrección. Sin ella, cualquier array con duplicados en los extremos
produce división por cero.

</details>

### Ejercicio 4 — Overflow en la fórmula

¿Qué valores de `target`, `arr[lo]`, y `hi - lo` podrían causar
overflow en `(target - arr[lo]) * (hi - lo)`?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Con `int` de 32 bits, la multiplicación desborda cuando el producto
excede $2^{31} - 1 = 2{,}147{,}483{,}647$.

Ejemplo:

```
arr[lo] = 0
target  = 1,000,000,000  (10^9)
hi - lo = 1,000,000,000  (10^9 elementos)

(target - arr[lo]) * (hi - lo) = 10^9 * 10^9 = 10^18
```

$10^{18}$ excede `INT_MAX` por un factor de ~$4.6 \times 10^8$.

Solución: usar `long long` (64 bits) para la multiplicación:

```c
long long num = (long long)(target - arr[lo]) * (hi - lo);
int pos = lo + (int)(num / (arr[hi] - arr[lo]));
```

`long long` soporta hasta $\sim 9.2 \times 10^{18}$, suficiente
para arrays prácticos.

En Rust, se usa `i64`:

```rust
let num = (target as i64 - arr[lo] as i64) * (hi as i64 - lo as i64);
```

En Rust modo debug, el overflow de `i32` generaría panic — el bug
se detectaría inmediatamente.

</details>

### Ejercicio 5 — Comparación empírica

Para un array de $10^6$ enteros uniformes, ¿cuántos pasos promedio
necesita la interpolación y cuántos la binaria? Calcula
teóricamente.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Binaria**: $\lfloor \log_2(10^6) \rfloor + 1 = 20$ pasos en el
peor caso, ~19 en promedio.

**Interpolación** (datos uniformes):
$\log_2(\log_2(10^6)) = \log_2(20) \approx 4.3$ pasos.

En la práctica, para datos perfectamente uniformes (como
`arr[i] = i * k`), la interpolación encuentra la posición exacta
en **1-2 pasos** porque la estimación es perfecta.

Para datos "casi uniformes" (distribución uniforme con ruido), el
promedio sube a **2-4 pasos** porque la estimación tiene un error
pequeño que requiere 1-2 correcciones.

Verificación con el benchmark de la Demo 4:

```
Interpolación: ~2.5 pasos promedio
Binaria:       ~19.5 pasos promedio
Ratio:         ~7.8x menos pasos
```

Pero en tiempo real, la diferencia es menor (~1.5-2x) porque cada
paso de interpolación es más costoso (división entera vs shift).

</details>

### Ejercicio 6 — Interpolación + binaria híbrida

Diseña un algoritmo que use interpolación para los primeros pasos
(cuando el rango es grande) y cambie a binaria cuando el rango se
reduce a menos de 1000 elementos. ¿Por qué podría ser mejor que
ambos por separado?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int hybrid_search(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;

    while (lo <= hi && arr[lo] <= target && target <= arr[hi]) {
        int range = hi - lo;

        if (range < 1000) {
            /* Cambiar a binaria para rango pequeño */
            while (lo <= hi) {
                int mid = lo + (hi - lo) / 2;
                if (arr[mid] == target) return mid;
                if (arr[mid] < target)  lo = mid + 1;
                else                    hi = mid - 1;
            }
            return -1;
        }

        if (arr[lo] == arr[hi])
            return (arr[lo] == target) ? lo : -1;

        long long num = (long long)(target - arr[lo]) * range;
        int pos = lo + (int)(num / (arr[hi] - arr[lo]));

        if (arr[pos] == target) return pos;
        if (arr[pos] < target)  lo = pos + 1;
        else                    hi = pos - 1;
    }

    return -1;
}
```

**Por qué es mejor**:

- La interpolación reduce rápidamente un rango de $10^6$ a ~$10^3$
  en 1-2 pasos (gracias a la estimación proporcional).
- Una vez en rango < 1000, la binaria toma ~10 pasos con operaciones
  baratas (shift vs división).
- Se evita el peor caso de la interpolación: si los datos locales
  están sesgados, la binaria maneja correctamente el rango pequeño.

Costo total: ~2 pasos de interpolación + ~10 pasos de binaria = ~12
operaciones, vs ~20 pasos de binaria pura o ~2-3 pasos de
interpolación pura (pero con divisiones costosas en cada paso).

</details>

### Ejercicio 7 — Cache y acceso a memoria

La búsqueda binaria siempre accede a posiciones "predecibles"
(mitad, cuarto, octavo...), mientras que la interpolación accede a
posiciones "aleatorias". ¿Cómo afecta esto al rendimiento en
cache?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Búsqueda binaria**: los primeros accesos (mitad, cuarto) están
dispersos por todo el array, causando cache misses. Pero los últimos
accesos están en una región pequeña que cabe en L1 cache. Además,
los primeros accesos son siempre los mismos para un array dado, lo
que permite que el prefetcher y el TLB los predigan.

**Interpolación**: los accesos dependen del `target`, así que son
impredecibles para el prefetcher. Sin embargo, son **menos** accesos
en total.

Para arrays que caben en L1/L2 cache (~256KB / ~8MB), la diferencia
es mínima porque todos los accesos son rápidos (~4 ns).

Para arrays enormes que están en RAM (~100 ns por acceso):

- Binaria: ~20 accesos × ~100 ns = ~2000 ns.
- Interpolación: ~3 accesos × ~100 ns = ~300 ns.

Aquí la interpolación gana ampliamente porque **cada acceso
ahorrado equivale a ~100 ns**.

Paradoja: la interpolación es más valiosa precisamente cuando los
datos están en RAM (donde cada acceso es costoso) y menos valiosa
cuando están en cache (donde las divisiones dominan el costo).

</details>

### Ejercicio 8 — Interpolación para strings

¿Se puede aplicar búsqueda por interpolación a un array de strings
ordenados? ¿Cómo se calcularía la posición estimada?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Teóricamente sí, pero es complicado y raramente vale la pena.

Para interpolar strings, se necesita una función que convierta un
string a un valor numérico proporcional a su posición lexicográfica:

```c
/* Usar los primeros k bytes como número base-256 */
double string_to_rank(const char *s, int k)
{
    double rank = 0;
    double base = 1.0;
    for (int i = 0; i < k && s[i]; i++) {
        base /= 256.0;
        rank += (unsigned char)s[i] * base;
    }
    return rank;
}
```

Luego interpolar con estos valores numéricos:

```c
double lo_rank = string_to_rank(arr[lo], 4);
double hi_rank = string_to_rank(arr[hi], 4);
double t_rank  = string_to_rank(target, 4);

int pos = lo + (int)((t_rank - lo_rank) * (hi - lo) / (hi_rank - lo_rank));
```

**Problemas prácticos**:

1. La distribución de strings rara vez es uniforme (hay más
   palabras con 'S' que con 'X' en español).
2. El cálculo de rank tiene precisión limitada con `double` (15
   dígitos significativos ≈ 7 caracteres base-256).
3. El overhead del cálculo por paso es mayor que para enteros.
4. La ganancia ($\log \log n$ vs $\log n$) no compensa para
   vocabularios típicos ($n < 10^6$).

En la práctica, para buscar en diccionarios de strings se usan
**tries** ($O(m)$ donde $m$ es la longitud del string), no
interpolación.

</details>

### Ejercicio 9 — Cuándo la interpolación es peor que secuencial

Construye un array donde la interpolación haga **más** comparaciones
que la búsqueda secuencial optimizada (con early exit en array
ordenado).

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Esto ocurre cuando la interpolación avanza de uno en uno (caso
$O(n)$) pero cada paso tiene más overhead que la búsqueda
secuencial.

```c
/* Array sesgado: la fórmula siempre estima pos = lo */
int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10000000};
int target = 9;
```

**Interpolación**: avanza de `lo=0` a `lo=8` paso a paso (8 pasos),
cada uno con una división entera (~5-8 ns por paso).
Total: ~8 × 8 ns = ~64 ns.

**Secuencial con early exit**:

```c
for (int i = 0; i < n; i++) {
    if (arr[i] == 9) return i;   // encontrado en i=8
    if (arr[i] > 9) return -1;
}
```

9 comparaciones simples (~2 ns cada una).
Total: ~9 × 2 ns = ~18 ns.

La secuencial es ~3.5x más rápida porque sus operaciones son mucho
más baratas (comparación vs división).

Moraleja: la interpolación solo vale cuando ahorra **suficientes
pasos** para compensar el mayor costo por paso. Con datos sesgados
y $n$ pequeño, la secuencial gana.

</details>

### Ejercicio 10 — Interpolación adaptativa

Diseña una modificación que detecte cuando la estimación es
consistentemente mala (el error `|pos - posición_real|` es grande)
y cambie automáticamente a búsqueda binaria.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int adaptive_search(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;
    int bad_estimates = 0;
    const int MAX_BAD = 2;  /* tolerancia */

    while (lo <= hi && arr[lo] <= target && target <= arr[hi]) {
        int range = hi - lo + 1;
        int mid;

        if (bad_estimates >= MAX_BAD || arr[lo] == arr[hi]) {
            /* Modo binaria */
            mid = lo + (hi - lo) / 2;
        } else {
            /* Modo interpolación */
            long long num = (long long)(target - arr[lo]) * (hi - lo);
            mid = lo + (int)(num / (arr[hi] - arr[lo]));
        }

        if (arr[mid] == target) return mid;

        int new_range;
        if (arr[mid] < target) {
            lo = mid + 1;
            new_range = hi - lo + 1;
        } else {
            hi = mid - 1;
            new_range = hi - lo + 1;
        }

        /* ¿La estimación fue mala? */
        /* Si el rango se redujo menos del 50%, la binaria habría sido mejor */
        if (new_range > range / 2) {
            bad_estimates++;
        } else {
            bad_estimates = 0;  /* reset si fue buena */
        }
    }

    return -1;
}
```

**Criterio de "mala estimación"**: si después de un paso el rango
se redujo menos del 50%, la estimación no superó a la binaria
(que siempre reduce al 50%). Después de 2 estimaciones malas
consecutivas, cambiamos a binaria.

Esto da lo mejor de ambos mundos:

- Datos uniformes: la interpolación domina, `bad_estimates` se
  mantiene en 0.
- Datos sesgados: después de 2 pasos malos, cambia a binaria con
  garantía $O(\log n)$.
- Peor caso: 2 pasos extra de interpolación + $O(\log n)$ de
  binaria = $O(\log n)$.

</details>

---

## Resumen

| Aspecto | Binaria | Interpolación |
|---------|---------|---------------|
| Fórmula de posición | $\text{lo} + \frac{\text{hi}-\text{lo}}{2}$ | $\text{lo} + \frac{(\text{target}-\text{arr[lo]})(\text{hi}-\text{lo})}{\text{arr[hi]}-\text{arr[lo]}}$ |
| Mejor caso | $O(1)$ | $O(1)$ |
| Caso promedio (uniforme) | $O(\log n)$ | $O(\log \log n)$ |
| Peor caso | $O(\log n)$ | $O(n)$ |
| Costo por paso | Bajo (suma + shift) | Alto (multiplicación + división) |
| Requisito de datos | Ordenados | Ordenados + numéricos + idealmente uniformes |
| Riesgo | Ninguno (siempre correcto) | División por cero, overflow, degradación |
| Mejor uso | General, garantizado | Arrays grandes uniformes en memoria |
