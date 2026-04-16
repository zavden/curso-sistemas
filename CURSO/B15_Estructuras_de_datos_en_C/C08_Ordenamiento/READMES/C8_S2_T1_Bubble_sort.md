# Bubble sort

## Objetivo

Estudiar bubble sort como el algoritmo de ordenamiento más intuitivo:

- Comparar e intercambiar elementos **adyacentes** repetidamente.
- Optimización con **bandera** de early termination.
- Análisis completo: mejor $O(n)$, promedio y peor $O(n^2)$.
- Por qué casi nunca se usa en la práctica, pese a su simplicidad.

---

## El algoritmo

Bubble sort recorre el array repetidamente. En cada pasada, compara
pares adyacentes y los intercambia si están en orden incorrecto. Los
elementos mayores "burbujean" hacia el final del array — de ahí el nombre.

### Invariante

Después de la pasada $i$ (contando desde 0), los $i + 1$ elementos
mayores están en sus posiciones finales al final del array. Es decir,
`arr[n-1-i..n-1]` está ordenado y contiene los mayores.

### Pseudocódigo

```
BUBBLE_SORT(arr, n):
    para i de 0 a n-2:
        para j de 0 a n-2-i:
            si arr[j] > arr[j+1]:
                swap(arr[j], arr[j+1])
```

El loop externo controla las pasadas. El loop interno recorre los
elementos no-fijados, comparando adyacentes. Cada pasada "fija" un
elemento más al final.

---

## Traza detallada

Array: `[5, 3, 8, 1, 4]`.

### Pasada 0 (i=0)

```
j=0: [5, 3, 8, 1, 4]  5 > 3? sí → swap → [3, 5, 8, 1, 4]
j=1: [3, 5, 8, 1, 4]  5 > 8? no
j=2: [3, 5, 8, 1, 4]  8 > 1? sí → swap → [3, 5, 1, 8, 4]
j=3: [3, 5, 1, 8, 4]  8 > 4? sí → swap → [3, 5, 1, 4, 8]
```

Después de pasada 0: `[3, 5, 1, 4, | 8]`. El 8 (máximo) está en su lugar.

### Pasada 1 (i=1)

```
j=0: [3, 5, 1, 4, | 8]  3 > 5? no
j=1: [3, 5, 1, 4, | 8]  5 > 1? sí → swap → [3, 1, 5, 4, | 8]
j=2: [3, 1, 5, 4, | 8]  5 > 4? sí → swap → [3, 1, 4, 5, | 8]
```

Después: `[3, 1, 4, | 5, 8]`.

### Pasada 2 (i=2)

```
j=0: [3, 1, 4, | 5, 8]  3 > 1? sí → swap → [1, 3, 4, | 5, 8]
j=1: [1, 3, 4, | 5, 8]  3 > 4? no
```

Después: `[1, 3, | 4, 5, 8]`.

### Pasada 3 (i=3)

```
j=0: [1, 3, | 4, 5, 8]  1 > 3? no
```

Después: `[1, | 3, 4, 5, 8]`. Terminado.

Resultado: **`[1, 3, 4, 5, 8]`**. 4 pasadas, 5 swaps, 10 comparaciones.

---

## Implementación en C

### Versión básica

```c
#include <stdio.h>

void bubble_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}
```

### Versión optimizada con bandera

Si una pasada completa no hace ningún swap, el array ya está ordenado y
podemos terminar:

```c
void bubble_sort_opt(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                swapped = 1;
            }
        }
        if (!swapped) break;  // ya ordenado
    }
}
```

Esta optimización convierte el mejor caso (array ya ordenado) de $O(n^2)$
a $O(n)$: una sola pasada sin swaps → termina.

### Versión con última posición de swap

Optimización adicional: recordar la última posición donde se hizo un swap.
Todo lo que está después de esa posición ya está ordenado:

```c
void bubble_sort_last(int arr[], int n) {
    int limit = n - 1;
    while (limit > 0) {
        int last_swap = 0;
        for (int j = 0; j < limit; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                last_swap = j;
            }
        }
        limit = last_swap;  // todo despues de last_swap ya esta ordenado
    }
}
```

Si los últimos 1000 elementos de un array de 10000 ya están ordenados,
esta versión reduce el rango del loop interno en cada pasada.

---

## Implementación en Rust

```rust
fn bubble_sort(arr: &mut [i32]) {
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut swapped = false;
        for j in 0..n - 1 - i {
            if arr[j] > arr[j + 1] {
                arr.swap(j, j + 1);
                swapped = true;
            }
        }
        if !swapped { break; }
    }
}

fn bubble_sort_generic<T: Ord>(arr: &mut [T]) {
    let n = arr.len();
    let mut limit = n.saturating_sub(1);
    while limit > 0 {
        let mut last_swap = 0;
        for j in 0..limit {
            if arr[j] > arr[j + 1] {
                arr.swap(j, j + 1);
                last_swap = j;
            }
        }
        limit = last_swap;
    }
}
```

`saturating_sub(1)` evita underflow cuando `n == 0` (un `usize` no puede
ser negativo).

---

## Análisis de complejidad

### Comparaciones

El loop externo ejecuta hasta $n - 1$ pasadas. La pasada $i$ hace
$n - 1 - i$ comparaciones:

$$C = \sum_{i=0}^{n-2}(n-1-i) = \sum_{k=1}^{n-1}k = \frac{(n-1)n}{2} = \frac{n^2 - n}{2}$$

Con la bandera, si el array está ordenado, la primera pasada hace $n - 1$
comparaciones y termina.

| Caso | Comparaciones | Swaps |
|------|--------------|-------|
| Mejor (ordenado, con bandera) | $n - 1$ | 0 |
| Mejor (ordenado, sin bandera) | $n(n-1)/2$ | 0 |
| Peor (orden inverso) | $n(n-1)/2$ | $n(n-1)/2$ |
| Promedio | $n(n-1)/2$ | $n(n-1)/4$ |

### Swaps

Cada swap corrige exactamente una **inversión**. El número total de swaps
es igual al número de inversiones del array.

- Array ordenado: 0 inversiones → 0 swaps.
- Array inverso: $n(n-1)/2$ inversiones → $n(n-1)/2$ swaps.
- Array aleatorio: $n(n-1)/4$ inversiones en promedio → $n(n-1)/4$ swaps.

### Adaptividad

Con la bandera, bubble sort es **adaptativo**:
- $O(n)$ para datos ordenados.
- $O(n + k)$ para datos con $k$ pares adyacentes desordenados (no
  exactamente $k$ inversiones — bubble sort puede necesitar múltiples
  pasadas para resolver inversiones distantes).

Sin embargo, la adaptividad de bubble sort es peor que la de insertion
sort. Bubble sort mueve cada elemento una posición por pasada; insertion
sort mueve cada elemento directamente a su posición correcta en la
porción ya ordenada.

---

## ¿Por qué bubble sort es ineficiente?

### Comparación con insertion sort

Ambos son $O(n^2)$ y estables, pero insertion sort es consistentemente
más rápido:

| Aspecto | Bubble sort | Insertion sort |
|---------|-------------|----------------|
| Swaps por pasada | Múltiples (todos los pares desordenados) | Shifts hasta la posición correcta |
| Movimiento por operación | 1 posición | 1 posición (shift) |
| Escrituras por swap | 3 (a→tmp, b→a, tmp→b) | 1 (shift: a[j+1]=a[j]) |
| Comparaciones (casi ordenado) | Similar | Similar |
| Rendimiento práctico | Referencia | ~2× más rápido |

La diferencia clave es que cada **swap** de bubble sort escribe 3 veces
en memoria, mientras que cada **shift** de insertion sort escribe 1 vez.
Para $n(n-1)/4$ operaciones promedio, bubble sort hace $3n^2/4$ escrituras
vs $n^2/4$ de insertion sort.

### Cuándo usar bubble sort

Casi nunca. Los únicos escenarios legítimos son:

1. **Enseñanza**: es el sort más fácil de entender y explicar.
2. **Verificar si un array está ordenado**: una pasada sin swaps confirma
   orden en $O(n)$. Pero `is_sorted()` dedicado es más claro.
3. **Datos extremadamente pequeños** ($n < 10$): la diferencia con
   insertion sort es negligible.

En cualquier otro caso, insertion sort es estrictamente superior.

---

## Variantes

### Cocktail shaker sort (bubble bidireccional)

Alterna pasadas de izquierda a derecha y de derecha a izquierda. Esto
evita el problema de "la tortuga": un elemento pequeño al final del array
necesita $n$ pasadas para llegar al inicio con bubble sort normal, pero
solo 1 pasada de derecha a izquierda con cocktail sort.

```c
void cocktail_sort(int arr[], int n) {
    int start = 0, end = n - 1;
    int swapped = 1;

    while (swapped) {
        swapped = 0;

        // pasada izquierda a derecha
        for (int j = start; j < end; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j]; arr[j] = arr[j + 1]; arr[j + 1] = tmp;
                swapped = 1;
            }
        }
        end--;

        if (!swapped) break;
        swapped = 0;

        // pasada derecha a izquierda
        for (int j = end; j > start; j--) {
            if (arr[j - 1] > arr[j]) {
                int tmp = arr[j]; arr[j] = arr[j - 1]; arr[j - 1] = tmp;
                swapped = 1;
            }
        }
        start++;
    }
}
```

Cocktail sort sigue siendo $O(n^2)$ en el peor caso, pero puede hacer
menos pasadas que bubble sort para ciertos patrones.

### Comb sort

Generalización de bubble sort con **gap** decreciente (similar a cómo
Shell sort generaliza insertion sort):

```c
void comb_sort(int arr[], int n) {
    int gap = n;
    float shrink = 1.3;
    int sorted = 0;

    while (!sorted) {
        gap = (int)(gap / shrink);
        if (gap <= 1) { gap = 1; sorted = 1; }

        for (int j = 0; j + gap < n; j++) {
            if (arr[j] > arr[j + gap]) {
                int tmp = arr[j]; arr[j] = arr[j + gap]; arr[j + gap] = tmp;
                sorted = 0;
            }
        }
    }
}
```

Con gap > 1, los elementos se mueven posiciones grandes rápidamente.
El factor 1.3 fue determinado empíricamente como óptimo. Comb sort es
$O(n^2 / 2^p)$ para $p$ pasadas con gap grande, acercándose a
$O(n \log n)$ en la práctica.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { long cmp; long swp; } Stats;

void bubble_basic(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++)
        for (int j = 0; j < n-1-i; j++) {
            s->cmp++;
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
                s->swp++;
            }
        }
}

void bubble_flag(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++) {
        int sw = 0;
        for (int j = 0; j < n-1-i; j++) {
            s->cmp++;
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
                s->swp++; sw = 1;
            }
        }
        if (!sw) break;
    }
}

void bubble_last(int a[], int n, Stats *s) {
    int lim = n - 1;
    while (lim > 0) {
        int last = 0;
        for (int j = 0; j < lim; j++) {
            s->cmp++;
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
                s->swp++; last = j;
            }
        }
        lim = last;
    }
}

typedef void (*SortFn)(int[], int, Stats*);

void bench(const char *sort_name, SortFn fn,
           const char *input_name, int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    Stats s = {0, 0};

    clock_t t0 = clock();
    fn(arr, n, &s);
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-12s %-10s  %7.4fs  cmp=%-10ld swp=%-10ld\n",
           sort_name, input_name, elapsed, s.cmp, s.swp);

    // verificar
    for (int i = 1; i < n; i++) {
        if (arr[i] < arr[i-1]) { printf("  ERROR: not sorted!\n"); break; }
    }
    free(arr);
}

int main(void) {
    srand(42);
    int n = 5000;

    // generar inputs
    int *random  = malloc(n * sizeof(int));
    int *sorted  = malloc(n * sizeof(int));
    int *reverse = malloc(n * sizeof(int));
    int *nearly  = malloc(n * sizeof(int));

    for (int i = 0; i < n; i++) {
        random[i]  = rand();
        sorted[i]  = i;
        reverse[i] = n - i;
        nearly[i]  = i;
    }
    for (int i = 0; i < n / 20; i++) {
        int a = rand() % n, b = rand() % n;
        int t = nearly[a]; nearly[a] = nearly[b]; nearly[b] = t;
    }

    printf("=== Bubble sort variantes (n=%d) ===\n\n", n);

    struct { const char *name; int *data; } inputs[] = {
        {"random", random}, {"sorted", sorted},
        {"reverse", reverse}, {"nearly", nearly},
    };

    struct { const char *name; SortFn fn; } sorts[] = {
        {"basic",     bubble_basic},
        {"flag",      bubble_flag},
        {"last_swap", bubble_last},
    };

    for (int s = 0; s < 3; s++) {
        for (int i = 0; i < 4; i++) {
            bench(sorts[s].name, sorts[s].fn, inputs[i].name, inputs[i].data, n);
        }
        printf("\n");
    }

    free(random); free(sorted); free(reverse); free(nearly);
    return 0;
}
```

### Salida esperada

```
=== Bubble sort variantes (n=5000) ===

  basic        random      0.0300s  cmp=12497500   swp=6250000
  basic        sorted      0.0100s  cmp=12497500   swp=0
  basic        reverse     0.0500s  cmp=12497500   swp=12497500
  basic        nearly      0.0120s  cmp=12497500   swp=620000

  flag         random      0.0300s  cmp=12497500   swp=6250000
  flag         sorted      0.0000s  cmp=4999       swp=0
  flag         reverse     0.0500s  cmp=12497500   swp=12497500
  flag         nearly      0.0030s  cmp=3100000    swp=620000

  last_swap    random      0.0280s  cmp=12400000   swp=6250000
  last_swap    sorted      0.0000s  cmp=4999       swp=0
  last_swap    reverse     0.0500s  cmp=12497500   swp=12497500
  last_swap    nearly      0.0020s  cmp=2500000    swp=620000
```

Observaciones:

- **basic + sorted**: hace 12.5M comparaciones innecesariamente.
- **flag + sorted**: solo 4999 comparaciones — la bandera detecta el
  array ordenado en la primera pasada.
- **last_swap + nearly**: reduce comparaciones a 2.5M (vs 3.1M del flag)
  porque acota el rango del loop interno.
- **reverse**: las tres variantes hacen lo mismo — no hay optimización
  posible para el peor caso.

---

## Ejercicios

### Ejercicio 1 — Traza manual

Aplica bubble sort (con bandera) a `[4, 2, 7, 1, 3]`. Muestra el estado
del array después de cada pasada. ¿Cuántas pasadas se necesitan?

<details>
<summary>Verificar</summary>

**Pasada 0**:
- j=0: 4>2 → swap → `[2, 4, 7, 1, 3]`
- j=1: 4>7 → no
- j=2: 7>1 → swap → `[2, 4, 1, 7, 3]`
- j=3: 7>3 → swap → `[2, 4, 1, 3, | 7]`
- swapped=true.

**Pasada 1**:
- j=0: 2>4 → no
- j=1: 4>1 → swap → `[2, 1, 4, 3, | 7]`
- j=2: 4>3 → swap → `[2, 1, 3, | 4, 7]`
- swapped=true.

**Pasada 2**:
- j=0: 2>1 → swap → `[1, 2, 3, | 4, 7]`
- j=1: 2>3 → no
- swapped=true.

**Pasada 3**:
- j=0: 1>2 → no
- swapped=false → **break**.

Total: **4 pasadas**, 5 swaps, 9 comparaciones.
</details>

### Ejercicio 2 — Contar comparaciones

¿Cuántas comparaciones hace bubble sort (sin bandera) para $n = 100$?
¿Y con bandera para un array ya ordenado?

<details>
<summary>Verificar</summary>

**Sin bandera**: $\frac{n(n-1)}{2} = \frac{100 \times 99}{2} = 4950$ comparaciones. Siempre, independientemente del input.

**Con bandera + array ordenado**: $n - 1 = 99$ comparaciones. Una sola
pasada sin swaps → break.

Ratio: $4950 / 99 = 50$×. La bandera da una mejora de 50× en el mejor caso.
</details>

### Ejercicio 3 — Bubble sort descendente

Modifica bubble sort para ordenar en orden **descendente**. ¿Qué cambio
se necesita?

<details>
<summary>Verificar</summary>

Cambiar `>` por `<` en la comparación:

```c
void bubble_sort_desc(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] < arr[j + 1]) {  // < en lugar de >
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}
```

Ahora los elementos **menores** burbujean al final y los mayores se
quedan al inicio.
</details>

### Ejercicio 4 — La tortuga

Explica por qué el elemento `1` en `[2, 3, 4, 5, 1]` necesita 4 pasadas
de bubble sort para llegar a su posición. ¿Cómo lo resuelve cocktail sort?

<details>
<summary>Verificar</summary>

El 1 es una "tortuga" — un elemento pequeño al final del array. En cada
pasada (izquierda a derecha), el 1 solo se mueve **una posición** hacia
la izquierda:

```
Pasada 0: [2, 3, 4, 1, 5]  (1 se mueve de idx 4 a idx 3)
Pasada 1: [2, 3, 1, 4, 5]  (1 se mueve de idx 3 a idx 2)
Pasada 2: [2, 1, 3, 4, 5]  (1 se mueve de idx 2 a idx 1)
Pasada 3: [1, 2, 3, 4, 5]  (1 se mueve de idx 1 a idx 0)
```

4 pasadas para mover un solo elemento.

**Cocktail sort**: en la primera pasada derecha-a-izquierda, el 1 se
mueve directamente de idx 4 a idx 0 en una sola pasada. Cocktail sort
resuelve este caso en 2 pasadas (una ida, una vuelta) en lugar de 4.

Las "tortugas" son la debilidad principal de bubble sort — un solo
elemento fuera de lugar puede forzar $O(n)$ pasadas.
</details>

### Ejercicio 5 — Estabilidad

Demuestra que bubble sort es estable. ¿Qué propiedad del swap garantiza
la estabilidad?

<details>
<summary>Verificar</summary>

Bubble sort compara `arr[j] > arr[j+1]` con `>` estricto. Cuando dos
elementos son iguales ($a = b$), la condición `a > b` es **falsa** → no
se hace swap → mantienen su orden relativo.

Si cambiáramos la condición a `>=`, bubble sort se volvería **no estable**:

```
[A(5), B(5)] con >=:
j=0: A(5) >= B(5)? sí → swap → [B(5), A(5)]  ← orden invertido
```

La clave es que el swap solo ocurre para `>` estricto, nunca para `=`.
Esto garantiza que elementos iguales nunca se intercambian → el orden
relativo original se preserva.

Esta propiedad se mantiene en todas las variantes (flag, last_swap,
cocktail) siempre que la comparación use `>` estricto.
</details>

### Ejercicio 6 — Swaps = inversiones

Demuestra que el número de swaps de bubble sort es exactamente igual al
número de inversiones del array. Pista: cada swap elimina exactamente una
inversión.

<details>
<summary>Verificar</summary>

**Demostración**:

Un swap de `arr[j]` y `arr[j+1]` ocurre solo cuando `arr[j] > arr[j+1]`,
es decir, el par $(j, j+1)$ es una inversión.

**Cada swap elimina exactamente una inversión**:
- El par $(j, j+1)$ pasa de inversión a no-inversión.
- Para cualquier otro par $(j, k)$ con $k > j+1$: si $arr[j] > arr[k]$
  antes del swap, entonces $arr[j+1] > arr[k]$ sigue siendo inversión
  (porque $arr[j+1]$ era $\leq arr[j]$). El swap no afecta otros pares.

Más formalmente: un swap de adyacentes afecta solo la inversión entre
esos dos elementos. No crea nuevas inversiones ni elimina otras.

**Ningún swap crea nuevas inversiones**: antes del swap, $arr[j] > arr[j+1]$.
Después, $arr[j] < arr[j+1]$ (eran adyacentes, ahora están en orden).
Los pares con otros elementos no cambian.

Como cada swap elimina exactamente 1 inversión y no crea ninguna:

$$\text{swaps} = \text{inversiones iniciales}$$

Para un array en orden inverso: $n(n-1)/2$ inversiones → $n(n-1)/2$ swaps.
Para un array aleatorio: $\sim n(n-1)/4$ inversiones → $\sim n(n-1)/4$ swaps.
</details>

### Ejercicio 7 — Cocktail sort vs bubble sort

Implementa cocktail sort y compara con bubble sort para `[5, 1, 4, 2, 8, 0, 3]`.
¿Cuántas pasadas hace cada uno?

<details>
<summary>Verificar</summary>

**Bubble sort (flag)**:

```
Pasada 0: [1, 4, 2, 5, 0, 3, | 8]  → 4 swaps
Pasada 1: [1, 2, 4, 0, 3, | 5, 8]  → 2 swaps
Pasada 2: [1, 2, 0, 3, | 4, 5, 8]  → 1 swap
Pasada 3: [1, 0, 2, | 3, 4, 5, 8]  → 1 swap
Pasada 4: [0, 1, | 2, 3, 4, 5, 8]  → 1 swap
Pasada 5: [0, | 1, 2, 3, 4, 5, 8]  → 0 swaps → break
```

6 pasadas (5 con swaps + 1 de verificación).

**Cocktail sort**:

```
→ Pasada 0: [1, 4, 2, 5, 0, 3, | 8]  → 4 swaps
← Pasada 0: [0, | 1, 4, 2, 5, 3, | 8]  → 3 swaps (el 0 llega a idx 0)
→ Pasada 1: [0, | 1, 2, 4, 3, | 5, 8]  → 1 swap
← Pasada 1: [0, 1, | 2, 3, 4, | 5, 8]  → 1 swap
→ Pasada 2: no swaps → break
```

5 pasadas (2 completas ida+vuelta + 1 final). El cocktail sort maneja
la "tortuga" (el 0) en una sola pasada de vuelta vs 4 pasadas extra
del bubble sort.
</details>

### Ejercicio 8 — Bubble sort en Rust genérico

Implementa bubble sort genérico en Rust para `&mut [T]` donde `T: Ord`.
Prueba con `Vec<String>`.

<details>
<summary>Verificar</summary>

```rust
fn bubble_sort<T: Ord>(arr: &mut [T]) {
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut swapped = false;
        for j in 0..n - 1 - i {
            if arr[j] > arr[j + 1] {
                arr.swap(j, j + 1);
                swapped = true;
            }
        }
        if !swapped { break; }
    }
}

fn main() {
    let mut words = vec![
        "banana".to_string(), "apple".to_string(),
        "cherry".to_string(), "date".to_string(),
    ];
    bubble_sort(&mut words);
    println!("{:?}", words);
    // ["apple", "banana", "cherry", "date"]
}
```

`T: Ord` permite comparar con `>`. `arr.swap(j, j+1)` hace el intercambio
sin necesidad de una variable temporal ni de que `T` implemente `Copy`.
</details>

### Ejercicio 9 — Comb sort benchmark

Implementa comb sort y compara con bubble sort para $n = 10000$ aleatorio.
¿Cuántas veces más rápido es comb sort?

<details>
<summary>Verificar</summary>

```c
void comb_sort(int arr[], int n, Stats *s) {
    int gap = n;
    int sorted = 0;

    while (!sorted) {
        gap = (int)(gap / 1.3);
        if (gap < 1) gap = 1;
        if (gap == 1) sorted = 1;

        for (int j = 0; j + gap < n; j++) {
            s->cmp++;
            if (arr[j] > arr[j + gap]) {
                int t = arr[j]; arr[j] = arr[j + gap]; arr[j + gap] = t;
                s->swp++;
                sorted = 0;
            }
        }
    }
}
```

Para $n = 10000$ aleatorio:

| Algoritmo | Comparaciones | Swaps | Tiempo |
|-----------|--------------|-------|--------|
| Bubble sort | 50M | 25M | ~0.050s |
| Comb sort | ~180K | ~90K | ~0.001s |

Comb sort es ~50× más rápido que bubble sort. Los gaps grandes eliminan
inversiones a larga distancia rápidamente, reduciendo las comparaciones
de $O(n^2)$ a $\sim O(n \log n)$ en la práctica (aunque el peor caso
teórico sigue siendo $O(n^2)$).
</details>

### Ejercicio 10 — Bubble sort nunca en producción

Explica por qué insertion sort es siempre preferible a bubble sort.
Enumera al menos 3 razones concretas.

<details>
<summary>Verificar</summary>

1. **Menos escrituras en memoria**: insertion sort hace 1 escritura por
   movimiento (shift: `a[j+1] = a[j]`), bubble sort hace 3 escrituras por
   swap (`tmp = a; a = b; b = tmp`). Para el mismo número de movimientos,
   bubble sort escribe 3× más — impacto en caché y rendimiento.

2. **Mejor adaptividad**: para datos casi ordenados con $k$ inversiones,
   insertion sort hace exactamente $n + k$ comparaciones. Bubble sort
   puede necesitar múltiples pasadas completas para resolver inversiones
   distantes — la "tortuga" fuerza $O(n)$ pasadas extra por cada elemento
   pequeño al final.

3. **Uso en la práctica**: insertion sort es usado como subrutina en
   algoritmos reales (Timsort, introsort, pdqsort) para subarrays
   pequeños ($n < 16$-$64$). Bubble sort no se usa en ningún algoritmo
   de producción.

4. **Rendimiento empírico**: para $n = 1000$ aleatorio, insertion sort
   es ~2× más rápido que bubble sort. La diferencia crece con $n$.

5. **Patrón de acceso**: insertion sort trabaja sobre una porción
   "ordenada" que crece, manteniendo localidad. Bubble sort recorre todo
   el array no-fijado en cada pasada.

La única "ventaja" de bubble sort es que es marginalmente más fácil de
entender y explicar. Pero como herramienta práctica, insertion sort lo
supera en **todo**.
</details>
