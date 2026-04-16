# Selection sort

## Objetivo

Estudiar selection sort como el algoritmo de ordenamiento con el **mínimo
número de swaps**:

- Encontrar el mínimo del subarray no ordenado e intercambiarlo con la
  primera posición.
- Siempre $O(n^2)$ comparaciones, pero solo $O(n)$ swaps.
- No es estable, no es adaptativo.
- Útil cuando los swaps son costosos (structs grandes).

---

## El algoritmo

Selection sort divide el array en dos zonas:

- **Zona ordenada** (izquierda): contiene los $i$ menores en posición final.
- **Zona no ordenada** (derecha): el resto.

En cada iteración, busca el **mínimo** de la zona no ordenada y lo
intercambia con el primer elemento de esa zona.

### Invariante

Después de la iteración $i$, los $i$ elementos más pequeños están en
`arr[0..i-1]` en su posición correcta y definitiva.

### Pseudocódigo

```
SELECTION_SORT(arr, n):
    para i de 0 a n-2:
        min_idx = i
        para j de i+1 a n-1:
            si arr[j] < arr[min_idx]:
                min_idx = j
        swap(arr[i], arr[min_idx])
```

---

## Traza detallada

Array: `[29, 10, 14, 37, 13]`.

### Iteración 0 (i=0)

Buscar mínimo en `[29, 10, 14, 37, 13]` (todo el array):

```
j=1: 10 < 29? sí → min_idx=1
j=2: 14 < 10? no
j=3: 37 < 10? no
j=4: 13 < 10? no
```

Mínimo: 10 en idx 1. Swap arr[0] ↔ arr[1]:

```
[10, 29, 14, 37, 13]
 ↑ fijado
```

### Iteración 1 (i=1)

Buscar mínimo en `[29, 14, 37, 13]`:

```
j=2: 14 < 29? sí → min_idx=2
j=3: 37 < 14? no
j=4: 13 < 14? sí → min_idx=4
```

Mínimo: 13 en idx 4. Swap arr[1] ↔ arr[4]:

```
[10, 13, 14, 37, 29]
 ↑   ↑ fijados
```

### Iteración 2 (i=2)

Buscar mínimo en `[14, 37, 29]`:

```
j=3: 37 < 14? no
j=4: 29 < 14? no
```

Mínimo: 14 en idx 2 (ya en su lugar). Swap arr[2] ↔ arr[2] (no-op):

```
[10, 13, 14, 37, 29]
 ↑   ↑   ↑ fijados
```

### Iteración 3 (i=3)

Buscar mínimo en `[37, 29]`:

```
j=4: 29 < 37? sí → min_idx=4
```

Mínimo: 29 en idx 4. Swap arr[3] ↔ arr[4]:

```
[10, 13, 14, 29, 37]
 ↑   ↑   ↑   ↑   ↑ todo fijado
```

Resultado: **`[10, 13, 14, 29, 37]`**. 4 iteraciones, 3 swaps (1 fue
no-op), 10 comparaciones.

---

## Implementación en C

### Versión básica

```c
#include <stdio.h>

void selection_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[min_idx])
                min_idx = j;
        }
        if (min_idx != i) {
            int tmp = arr[i];
            arr[i] = arr[min_idx];
            arr[min_idx] = tmp;
        }
    }
}
```

El `if (min_idx != i)` evita swaps innecesarios cuando el mínimo ya
está en su posición. Esto no cambia la complejidad pero reduce
escrituras.

### Versión instrumentada

```c
typedef struct { long cmp; long swp; } Stats;

void selection_sort_stats(int arr[], int n, Stats *s) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            s->cmp++;
            if (arr[j] < arr[min_idx])
                min_idx = j;
        }
        if (min_idx != i) {
            int tmp = arr[i];
            arr[i] = arr[min_idx];
            arr[min_idx] = tmp;
            s->swp++;
        }
    }
}
```

### Versión genérica

```c
void selection_sort_generic(void *base, int n, size_t size,
                            int (*cmp)(const void *, const void *)) {
    char *arr = (char *)base;
    char *tmp = malloc(size);

    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (cmp(arr + j * size, arr + min_idx * size) < 0)
                min_idx = j;
        }
        if (min_idx != i) {
            memcpy(tmp, arr + i * size, size);
            memcpy(arr + i * size, arr + min_idx * size, size);
            memcpy(arr + min_idx * size, tmp, size);
        }
    }

    free(tmp);
}
```

Cada swap hace 3 `memcpy` de `size` bytes. Si `size` es grande (e.g., un
struct de 1 KB), cada swap cuesta ~3 KB de copia. Selection sort minimiza
estas copias costosas.

---

## Implementación en Rust

```rust
fn selection_sort<T: Ord>(arr: &mut [T]) {
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut min_idx = i;
        for j in (i + 1)..n {
            if arr[j] < arr[min_idx] {
                min_idx = j;
            }
        }
        if min_idx != i {
            arr.swap(i, min_idx);
        }
    }
}

fn main() {
    let mut arr = [29, 10, 14, 37, 13];
    selection_sort(&mut arr);
    println!("{:?}", arr);  // [10, 13, 14, 29, 37]

    let mut words = ["cherry", "apple", "banana", "date"];
    selection_sort(&mut words);
    println!("{:?}", words);  // ["apple", "banana", "cherry", "date"]
}
```

---

## Análisis de complejidad

### Comparaciones

El número de comparaciones es **siempre** el mismo, sin importar el input:

$$C = \sum_{i=0}^{n-2}(n-1-i) = \frac{n(n-1)}{2}$$

No hay mejor caso ni peor caso para comparaciones — selection sort no es
adaptativo. Siempre examina cada elemento de la zona no ordenada.

### Swaps

Cada iteración hace **a lo sumo** 1 swap. Hay $n - 1$ iteraciones:

$$S_{\text{máx}} = n - 1$$

En el mejor caso (array ya ordenado), $\text{min\_idx} = i$ en cada
iteración → 0 swaps.

| Caso | Comparaciones | Swaps |
|------|--------------|-------|
| Mejor (ordenado) | $n(n-1)/2$ | 0 |
| Peor (inverso) | $n(n-1)/2$ | $n - 1$ |
| Promedio | $n(n-1)/2$ | $\sim n - n/e \approx 0.63n$ |

El promedio de swaps se calcula así: en la iteración $i$, el mínimo ya
está en posición $i$ con probabilidad $1/(n-i)$. El número esperado de
swaps es:

$$E[S] = \sum_{i=0}^{n-2}\left(1 - \frac{1}{n-i}\right) = (n-1) - \sum_{k=2}^{n}\frac{1}{k} \approx n - 1 - \ln n$$

### Comparación directa con bubble sort e insertion sort

Para $n = 10000$ aleatorio:

| Métrica | Bubble sort | Insertion sort | Selection sort |
|---------|-------------|----------------|----------------|
| Comparaciones | 50M | 25M | 50M |
| Swaps | 25M | 25M (shifts) | ~6300 |
| Escrituras totales | 75M | 25M | ~19K |
| Adaptativo | Sí (con bandera) | Sí | No |
| Estable | Sí | Sí | No |

Selection sort tiene órdenes de magnitud menos escrituras. Si cada
escritura cuesta mucho (e.g., memoria Flash con número limitado de
escrituras, o structs grandes), selection sort gana.

---

## ¿Por qué selection sort no es estable?

El swap de larga distancia puede saltar sobre elementos iguales:

```
Array: [(5,'A'), (3,'B'), (5,'C'), (3,'D')]
        idx 0     idx 1    idx 2    idx 3
```

**Iteración 0**: mínimo = (3,'B') en idx 1. Swap arr[0] ↔ arr[1]:

```
[(3,'B'), (5,'A'), (5,'C'), (3,'D')]
```

**Iteración 1**: mínimo = (3,'D') en idx 3. Swap arr[1] ↔ arr[3]:

```
[(3,'B'), (3,'D'), (5,'C'), (5,'A')]
```

**Iteración 2**: mínimo = (5,'C') en idx 2. Ya en su lugar.

Resultado: `[(3,'B'), (3,'D'), (5,'C'), (5,'A')]`.

Los dos 5s están invertidos: originalmente A(idx 0) estaba antes de C(idx 2),
pero en la salida C aparece antes de A. El swap en la iteración 1 saltó
sobre (5,'C') al mover (3,'D'), empujando (5,'A') lejos de su posición
relativa original.

### Hacer selection sort estable

Se puede, pero requiere reemplazar el swap por un **shift** (inserción):
en lugar de intercambiar `arr[i]` con `arr[min_idx]`, desplazar todos los
elementos entre $i$ y $\text{min\_idx}$ una posición a la derecha y colocar
el mínimo en $i$. Esto es $O(n)$ por iteración → $O(n^2)$ total, y
convierte selection sort en algo muy similar a insertion sort, perdiendo su
ventaja de $O(n)$ swaps.

---

## Variante: double selection sort

En cada pasada, encontrar tanto el mínimo como el máximo y colocarlos en
sus posiciones:

```c
void double_selection_sort(int arr[], int n) {
    for (int left = 0, right = n - 1; left < right; left++, right--) {
        int min_idx = left, max_idx = left;

        for (int j = left; j <= right; j++) {
            if (arr[j] < arr[min_idx]) min_idx = j;
            if (arr[j] > arr[max_idx]) max_idx = j;
        }

        // colocar minimo
        if (min_idx != left) {
            int tmp = arr[left]; arr[left] = arr[min_idx]; arr[min_idx] = tmp;
            // si el maximo estaba en left, ahora esta en min_idx
            if (max_idx == left) max_idx = min_idx;
        }

        // colocar maximo
        if (max_idx != right) {
            int tmp = arr[right]; arr[right] = arr[max_idx]; arr[max_idx] = tmp;
        }
    }
}
```

Esto reduce el número de pasadas a la mitad ($n/2$ en lugar de $n$), pero
el número total de comparaciones sigue siendo $\sim n^2/2$. La mejora real
es mínima.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { long cmp; long swp; } Stats;

// ======== Selection sort variantes ========

void sel_basic(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++) {
        int m = i;
        for (int j = i+1; j < n; j++) {
            s->cmp++;
            if (a[j] < a[m]) m = j;
        }
        if (m != i) {
            int t = a[i]; a[i] = a[m]; a[m] = t;
            s->swp++;
        }
    }
}

void sel_double(int a[], int n, Stats *s) {
    for (int l = 0, r = n-1; l < r; l++, r--) {
        int mi = l, mx = l;
        for (int j = l; j <= r; j++) {
            s->cmp++;
            if (a[j] < a[mi]) mi = j;
            s->cmp++;
            if (a[j] > a[mx]) mx = j;
        }
        if (mi != l) {
            int t = a[l]; a[l] = a[mi]; a[mi] = t;
            s->swp++;
            if (mx == l) mx = mi;
        }
        if (mx != r) {
            int t = a[r]; a[r] = a[mx]; a[mx] = t;
            s->swp++;
        }
    }
}

// ======== Insertion y bubble para comparacion ========

void ins_sort(int a[], int n, Stats *s) {
    for (int i = 1; i < n; i++) {
        int k = a[i]; int j = i - 1;
        while (j >= 0) { s->cmp++; if (a[j] > k) { a[j+1] = a[j]; s->swp++; j--; } else break; }
        a[j+1] = k;
    }
}

void bub_sort(int a[], int n, Stats *s) {
    for (int i = 0; i < n-1; i++) {
        int sw = 0;
        for (int j = 0; j < n-1-i; j++) {
            s->cmp++;
            if (a[j] > a[j+1]) { int t = a[j]; a[j] = a[j+1]; a[j+1] = t; s->swp++; sw = 1; }
        }
        if (!sw) break;
    }
}

// ======== Benchmark ========

typedef void (*SortFn)(int[], int, Stats*);

void bench(const char *name, SortFn fn, int orig[], int n) {
    int *arr = malloc(n * sizeof(int));
    memcpy(arr, orig, n * sizeof(int));
    Stats s = {0, 0};

    clock_t t0 = clock();
    fn(arr, n, &s);
    double t = (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("  %-18s %7.4fs  cmp=%-10ld swp=%-8ld\n", name, t, s.cmp, s.swp);
    free(arr);
}

int main(void) {
    srand(42);
    int n = 10000;
    int *arr = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) arr[i] = rand();

    printf("=== Comparacion cuadraticos (n=%d, random) ===\n\n", n);
    bench("selection",      sel_basic,  arr, n);
    bench("selection-double",sel_double, arr, n);
    bench("insertion",      ins_sort,   arr, n);
    bench("bubble",         bub_sort,   arr, n);

    printf("\n=== Selection sort por tipo de input (n=%d) ===\n\n", n);

    // sorted
    int *sorted = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) sorted[i] = i;
    bench("sel-sorted",     sel_basic, sorted, n);

    // reverse
    int *rev = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) rev[i] = n - i;
    bench("sel-reverse",    sel_basic, rev, n);

    // nearly sorted
    int *near = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) near[i] = i;
    for (int i = 0; i < n/20; i++) { int a = rand()%n, b = rand()%n; int t=near[a]; near[a]=near[b]; near[b]=t; }
    bench("sel-nearly",     sel_basic, near, n);

    // comparar swaps con structs grandes
    printf("\n=== Ventaja de pocos swaps ===\n\n");
    printf("  Para n=%d random:\n", n);
    printf("  Selection: %d swaps max (cheap for big structs)\n", n - 1);
    printf("  Insertion: ~%d shifts (each moves data)\n", n * n / 4);
    printf("  Bubble:    ~%d swaps (3 writes each)\n", n * n / 4);

    free(arr); free(sorted); free(rev); free(near);
    return 0;
}
```

### Salida esperada

```
=== Comparacion cuadraticos (n=10000, random) ===

  selection          0.0150s  cmp=49995000   swp=9990
  selection-double   0.0160s  cmp=99990000   swp=9985
  insertion          0.0200s  cmp=25000000   swp=24990000
  bubble             0.0500s  cmp=49950000   swp=24990000

=== Selection sort por tipo de input (n=10000) ===

  sel-sorted         0.0150s  cmp=49995000   swp=0
  sel-reverse        0.0150s  cmp=49995000   swp=5000
  sel-nearly         0.0150s  cmp=49995000   swp=490

=== Ventaja de pocos swaps ===

  Para n=10000 random:
  Selection: 9999 swaps max (cheap for big structs)
  Insertion: ~25000000 shifts (each moves data)
  Bubble:    ~25000000 swaps (3 writes each)
```

Observaciones:

- **Selection siempre 50M comparaciones** — independiente del input.
  Es el algoritmo más predecible.
- **Selection 10K swaps vs insertion/bubble 25M** — 2500× menos.
- **double selection** duplica las comparaciones (busca min Y max en cada
  scan) pero no mejora significativamente.
- **sel-sorted 0 swaps**: cada mínimo ya está en su lugar.
- **sel-reverse 5000 swaps**: exactamente $n/2$ swaps (cada par se
  intercambia).

---

## Ejercicios

### Ejercicio 1 — Traza manual

Aplica selection sort a `[64, 25, 12, 22, 11]`. Muestra el array después
de cada swap y el mínimo seleccionado.

<details>
<summary>Verificar</summary>

**i=0**: buscar mín en [64,25,12,22,11]. Mín=11 (idx 4). Swap arr[0]↔arr[4].

```
[11, 25, 12, 22, 64]
 ↑
```

**i=1**: buscar mín en [25,12,22,64]. Mín=12 (idx 2). Swap arr[1]↔arr[2].

```
[11, 12, 25, 22, 64]
 ↑   ↑
```

**i=2**: buscar mín en [25,22,64]. Mín=22 (idx 3). Swap arr[2]↔arr[3].

```
[11, 12, 22, 25, 64]
 ↑   ↑   ↑
```

**i=3**: buscar mín en [25,64]. Mín=25 (idx 3). Ya en su lugar, no swap.

```
[11, 12, 22, 25, 64]
 ↑   ↑   ↑   ↑   ↑
```

4 iteraciones, 3 swaps, 10 comparaciones.
</details>

### Ejercicio 2 — Comparaciones constantes

¿Por qué selection sort siempre hace exactamente $n(n-1)/2$ comparaciones,
incluso si el array ya está ordenado?

<details>
<summary>Verificar</summary>

Porque el loop interno **siempre** recorre toda la zona no ordenada para
encontrar el mínimo. A diferencia de insertion sort (que puede terminar
el loop interno temprano con `break`) o bubble sort (que puede terminar
con la bandera), selection sort no tiene condición de salida temprana en
el loop interno.

Encontrar el mínimo de $k$ elementos requiere comparar con todos ellos —
no hay forma de saber que un elemento es el mínimo sin haberlo comparado
con todos los demás. Por eso el loop interno es irreducible.

Esto hace que selection sort sea **completamente no adaptativo**: no
importa si el array está ordenado, inverso, o aleatorio — el número de
comparaciones es siempre $n(n-1)/2$.
</details>

### Ejercicio 3 — Swaps vs tamaño del struct

Dado un array de 1000 structs de 1 KB cada uno, ¿cuántos bytes copia en
total selection sort vs insertion sort (caso aleatorio)?

<details>
<summary>Verificar</summary>

**Selection sort**:
- Máximo $n - 1 = 999$ swaps.
- Cada swap: 3 `memcpy` de 1 KB = 3 KB.
- Total: $999 \times 3 = 2997$ KB $\approx$ **3 MB**.

**Insertion sort**:
- Promedio $n^2/4 = 250000$ shifts.
- Cada shift: 1 `memcpy` de 1 KB = 1 KB (más el save/restore del key).
- Total: $\sim 250000$ KB $\approx$ **250 MB**.

Selection sort mueve **83× menos datos**. Para structs grandes, esta
diferencia domina completamente el rendimiento — selection sort puede ser
10-50× más rápido que insertion sort pese a tener las mismas comparaciones.

Solución alternativa: ordenar un array de **punteros** a los structs en
lugar de los structs mismos. Los swaps mueven punteros de 8 bytes en lugar
de structs de 1 KB. Esto elimina la ventaja de selection sort.
</details>

### Ejercicio 4 — Demostrar no-estabilidad

Muestra un ejemplo concreto donde selection sort no es estable con el
array `[(2,'A'), (2,'B'), (1,'C')]`.

<details>
<summary>Verificar</summary>

```
Array: [(2,'A'), (2,'B'), (1,'C')]
        idx 0     idx 1    idx 2
```

**i=0**: mínimo = (1,'C') en idx 2. Swap arr[0] ↔ arr[2]:

```
[(1,'C'), (2,'B'), (2,'A')]
```

**i=1**: mínimo = (2,'B') en idx 1. Ya en su lugar.

Resultado: `[(1,'C'), (2,'B'), (2,'A')]`.

Estable sería: `[(1,'C'), (2,'A'), (2,'B')]` — A antes de B como en la
entrada.

El swap en i=0 movió (2,'A') de idx 0 a idx 2, saltando sobre (2,'B').
Esto invirtió el orden relativo de A y B.
</details>

### Ejercicio 5 — Selection sort con máximo

Implementa selection sort que busca el **máximo** en cada iteración y lo
coloca al final. ¿El resultado es distinto?

<details>
<summary>Verificar</summary>

```c
void selection_sort_max(int arr[], int n) {
    for (int i = n - 1; i > 0; i--) {
        int max_idx = 0;
        for (int j = 1; j <= i; j++) {
            if (arr[j] > arr[max_idx])
                max_idx = j;
        }
        if (max_idx != i) {
            int tmp = arr[i]; arr[i] = arr[max_idx]; arr[max_idx] = tmp;
        }
    }
}
```

El resultado (array ordenado) es el mismo. La diferencia es la dirección:
- Version original: fija los menores de izquierda a derecha.
- Version max: fija los mayores de derecha a izquierda.

Las mismas comparaciones, los mismos swaps (en esencia), solo en orden
inverso. La complejidad es idéntica.
</details>

### Ejercicio 6 — Selection sort en Rust con structs

Ordena un `Vec<Player>` por score usando selection sort manual. Verifica
que funciona con structs que implementan `Ord`.

<details>
<summary>Verificar</summary>

```rust
#[derive(Debug)]
struct Player {
    name: String,
    score: u32,
}

fn selection_sort_by<T, F>(arr: &mut [T], less: F)
where F: Fn(&T, &T) -> bool
{
    let n = arr.len();
    for i in 0..n.saturating_sub(1) {
        let mut min_idx = i;
        for j in (i + 1)..n {
            if less(&arr[j], &arr[min_idx]) {
                min_idx = j;
            }
        }
        if min_idx != i {
            arr.swap(i, min_idx);
        }
    }
}

fn main() {
    let mut players = vec![
        Player { name: "Alice".into(), score: 85 },
        Player { name: "Bob".into(),   score: 72 },
        Player { name: "Cat".into(),   score: 93 },
        Player { name: "Dan".into(),   score: 68 },
    ];

    selection_sort_by(&mut players, |a, b| a.score < b.score);

    for p in &players {
        println!("{}: {}", p.name, p.score);
    }
    // Dan: 68, Bob: 72, Alice: 85, Cat: 93
}
```

El closure `less` permite ordenar por cualquier criterio sin que `Player`
implemente `Ord`.
</details>

### Ejercicio 7 — Double selection: ¿vale la pena?

Compara selection sort simple vs double selection sort para $n = 10000$.
¿La versión doble es más rápida?

<details>
<summary>Verificar</summary>

**Comparaciones**:
- Simple: $n(n-1)/2 = 49\,995\,000$.
- Double: $\sim n^2/2 = 50\,000\,000$ (2 comparaciones por elemento en el
  scan, la mitad de pasadas).

**Swaps**:
- Simple: $\leq n-1 = 9999$.
- Double: $\leq 2 \times n/2 = n = 10000$ (2 swaps por pasada, n/2 pasadas).

**Tiempo**: la versión doble hace el mismo número de comparaciones pero
con un loop interno más complejo (buscar min y max simultáneamente, manejar
el caso `max_idx == left`). En la práctica es **ligeramente más lenta**
o igual.

No vale la pena. La versión doble agrega complejidad de código sin mejorar
el rendimiento. El cuello de botella son las $O(n^2)$ comparaciones, no los
$O(n)$ swaps.
</details>

### Ejercicio 8 — Verificar O(n) swaps empíricamente

Ejecuta selection sort para $n = 1000, 5000, 10000$ y verifica que el
número de swaps crece linealmente (no cuadráticamente).

<details>
<summary>Verificar</summary>

```c
int main(void) {
    srand(42);
    for (int n = 1000; n <= 10000; n += (n < 5000 ? 4000 : 5000)) {
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();
        Stats s = {0, 0};
        sel_basic(arr, n, &s);
        printf("n=%5d: swaps=%ld, swaps/n=%.3f, cmp=%ld, cmp/n^2=%.3f\n",
               n, s.swp, (double)s.swp/n,
               s.cmp, (double)s.cmp/n/n);
        free(arr);
    }
}
```

```
n= 1000: swaps=997,  swaps/n=0.997, cmp=499500,  cmp/n^2=0.500
n= 5000: swaps=4995, swaps/n=0.999, cmp=12497500, cmp/n^2=0.500
n=10000: swaps=9993, swaps/n=0.999, cmp=49995000, cmp/n^2=0.500
```

- `swaps/n` $\approx 1.0$ para todos los $n$ → $O(n)$ confirmado.
- `cmp/n²` $= 0.5$ exacto → $(n^2-n)/2 \approx n^2/2$ confirmado.

Los swaps crecen linealmente mientras las comparaciones crecen
cuadráticamente.
</details>

### Ejercicio 9 — Cuándo selection sort gana

Describe 3 escenarios concretos donde selection sort es preferible a
insertion sort y merge sort.

<details>
<summary>Verificar</summary>

1. **Structs grandes sin punteros**: si los elementos son structs de
   varios KB que no se pueden acceder por puntero (e.g., escritos en un
   buffer mapeado a memoria o Flash), selection sort minimiza copias:
   $O(n)$ copias de $k$ bytes vs $O(n^2)$ de insertion sort o $O(n)$
   copias + $O(n)$ espacio extra de merge sort.

2. **Memoria de escritura limitada**: en memoria Flash (EEPROM),
   cada celda soporta un número limitado de escrituras (~10K-100K).
   Selection sort con $\leq 3(n-1)$ escrituras vs insertion sort con
   $\sim n^2/4$ escrituras puede significar la diferencia entre desgastar
   la memoria o no.

3. **Requisito de previsibilidad**: en sistemas de tiempo real donde se
   necesita un tiempo de ejecución **predecible** (no solo bounded),
   selection sort siempre hace exactamente $n(n-1)/2$ comparaciones y
   a lo sumo $n-1$ swaps. Insertion sort y bubble sort varían según el
   input, lo que dificulta el análisis de worst-case execution time (WCET).
</details>

### Ejercicio 10 — Selection sort estable

Implementa una versión estable de selection sort usando shifts en lugar
de swaps. Compara el número de movimientos con insertion sort.

<details>
<summary>Verificar</summary>

```c
void selection_sort_stable(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[min_idx])
                min_idx = j;
        }
        if (min_idx != i) {
            // shift en lugar de swap: preserva orden relativo
            int min_val = arr[min_idx];
            for (int j = min_idx; j > i; j--) {
                arr[j] = arr[j - 1];  // shift derecha
            }
            arr[i] = min_val;
        }
    }
}
```

Número de movimientos para $n = 1000$ aleatorio:
- Selection sort estable: $\sim n^2/2$ shifts (el shift promedio recorre
  $n/4$ posiciones).
- Insertion sort: $\sim n^2/4$ shifts.

La versión estable pierde la ventaja de $O(n)$ swaps — ahora hace
$O(n^2)$ shifts, igual que insertion sort pero con más comparaciones
(siempre $n(n-1)/2$ vs $\sim n(n-1)/4$ de insertion sort).

Conclusión: si necesitas estabilidad, usa insertion sort. Selection sort
estable es estrictamente peor que insertion sort en todo aspecto.
</details>
