# Límite inferior de ordenamiento

## Objetivo

Demostrar que **ningún** algoritmo de ordenamiento basado en comparaciones
puede hacer menos de $\Omega(n \log n)$ comparaciones en el peor caso. Esta
es una cota inferior universal — no depende del algoritmo, sino del
**modelo de computación**.

La herramienta de demostración es el **árbol de decisión**: un árbol
binario donde cada nodo interno es una comparación y cada hoja es una
permutación del resultado.

---

## El modelo de comparación

Un algoritmo basado en comparaciones solo puede obtener información sobre
los datos mediante comparaciones de la forma $a_i \leq a_j$ (o $<$, $>$,
$\geq$). Esto incluye:

- Bubble sort, selection sort, insertion sort.
- Merge sort, quicksort, heapsort.
- Shell sort, Timsort, introsort.

No incluye algoritmos que explotan la estructura de los datos:
- Counting sort (usa valores como índices).
- Radix sort (examina dígitos individuales).
- Bucket sort (distribuye por rangos).

Estos últimos no están limitados por $\Omega(n \log n)$ porque no operan
en el modelo de comparación.

---

## Árbol de decisión

### Definición

Un **árbol de decisión** para ordenar $n$ elementos es un árbol binario
donde:

- Cada **nodo interno** contiene una comparación $a_i \leq a_j$.
- La rama **izquierda** corresponde a "sí" ($a_i \leq a_j$).
- La rama **derecha** corresponde a "no" ($a_i > a_j$).
- Cada **hoja** corresponde a una permutación que describe el resultado
  del ordenamiento.

El camino desde la raíz hasta una hoja representa la secuencia de
comparaciones que el algoritmo ejecuta para una entrada particular.

### Ejemplo: ordenar 3 elementos

Para $n = 3$ con elementos $a_1, a_2, a_3$:

```
                    a₁ ≤ a₂?
                   /         \
                 sí           no
              a₂ ≤ a₃?      a₁ ≤ a₃?
             /       \      /       \
           sí        no   sí        no
        [1,2,3]   a₁≤a₃? [2,1,3]  a₂≤a₃?
                  /    \          /      \
                sí     no      sí       no
             [1,3,2] [3,1,2] [2,3,1]  [3,2,1]
```

Hay $3! = 6$ hojas (una por cada permutación posible). La **altura** del
árbol es 3 — el peor caso requiere 3 comparaciones para ordenar 3
elementos.

### Propiedades

1. Cada algoritmo de ordenamiento basado en comparaciones corresponde a
   un árbol de decisión específico.
2. El **número de hojas** debe ser al menos $n!$ (cada permutación de
   entrada debe producir una hoja distinta, o el algoritmo no podría
   distinguir entre dos entradas diferentes).
3. El **peor caso** del algoritmo es la **altura** del árbol (el camino
   más largo de raíz a hoja).

---

## La demostración

### Teorema

Todo algoritmo de ordenamiento basado en comparaciones requiere
$\Omega(n \log n)$ comparaciones en el peor caso.

### Prueba

Sea $T$ el árbol de decisión de un algoritmo que ordena $n$ elementos.

**Paso 1**: $T$ debe tener al menos $n!$ hojas.

Cada una de las $n!$ permutaciones de la entrada debe corresponder a una
hoja diferente. Si dos permutaciones distintas $\pi_1$ y $\pi_2$ llegaran
a la misma hoja, el algoritmo produciría la misma salida para ambas — pero
al menos una estaría mal ordenada. Por lo tanto, el número de hojas
$L \geq n!$.

**Paso 2**: un árbol binario de altura $h$ tiene a lo sumo $2^h$ hojas.

Esto se demuestra por inducción: un árbol de altura 0 tiene 1 hoja
($2^0 = 1$). Un árbol de altura $h$ tiene como máximo $2^{h-1} + 2^{h-1} = 2^h$
hojas (los subárboles izquierdo y derecho tienen altura a lo sumo $h-1$).

**Paso 3**: combinando.

$$n! \leq L \leq 2^h$$

$$h \geq \log_2(n!)$$

**Paso 4**: acotar $\log_2(n!)$ con la **aproximación de Stirling**.

$$n! \approx \sqrt{2\pi n} \left(\frac{n}{e}\right)^n$$

$$\log_2(n!) = \log_2\left(\sqrt{2\pi n}\right) + n \log_2\left(\frac{n}{e}\right)$$

$$= \frac{1}{2}\log_2(2\pi n) + n\log_2 n - n\log_2 e$$

$$= n\log_2 n - n\log_2 e + O(\log n)$$

$$= n\log_2 n - 1.443n + O(\log n)$$

$$= \Theta(n \log n)$$

**Conclusión**: $h \geq \log_2(n!) = \Omega(n \log n)$. El peor caso de
cualquier algoritmo basado en comparaciones es al menos $\Omega(n \log n)$.

### Cota exacta sin Stirling

También podemos acotar $\log_2(n!)$ directamente:

$$n! = 1 \cdot 2 \cdot 3 \cdots n \geq \left(\frac{n}{2}\right)^{n/2}$$

porque la mitad de los factores son $\geq n/2$. Entonces:

$$\log_2(n!) \geq \frac{n}{2} \log_2\left(\frac{n}{2}\right) = \frac{n}{2}(\log_2 n - 1) = \Omega(n \log n)$$

Esta cota es más débil (factor $1/2$) pero no requiere Stirling.

---

## Valores exactos

La siguiente tabla muestra el número mínimo de comparaciones necesarias
para ordenar $n$ elementos, comparado con $\lceil \log_2(n!) \rceil$:

| $n$ | $n!$ | $\lceil \log_2(n!) \rceil$ | Mínimo real | Algoritmo óptimo |
|-----|------|---------------------------|-------------|-----------------|
| 2 | 2 | 1 | 1 | 1 comparación |
| 3 | 6 | 3 | 3 | Árbol del ejemplo |
| 4 | 24 | 5 | 5 | Merge-insertion sort |
| 5 | 120 | 7 | 7 | Ford-Johnson |
| 6 | 720 | 10 | 10 | Ford-Johnson |
| 10 | 3628800 | 22 | 22 | — |
| 20 | ~$2.4 \times 10^{18}$ | 62 | — | Desconocido |

Para $n$ pequeño, el mínimo real coincide con $\lceil \log_2(n!) \rceil$.
Para $n$ grande, se sabe que es $\Theta(n \log n)$ pero el valor exacto
es un problema abierto.

### Comparación con algoritmos reales

| Algoritmo | Comparaciones (peor caso) | vs $n \log_2 n$ |
|-----------|--------------------------|-----------------|
| Cota inferior | $\lceil \log_2(n!) \rceil \approx n \log_2 n - 1.44n$ | Referencia |
| Merge sort | $n \lceil \log_2 n \rceil \approx n \log_2 n$ | ~1.0× |
| Heapsort | $\leq 2n \log_2 n$ | ~2.0× |
| Quicksort (promedio) | $\approx 1.39 n \log_2 n$ | ~1.39× |
| Insertion sort (peor) | $n(n-1)/2$ | $\Theta(n / \log n)$× |

Merge sort es el más cercano al óptimo teórico en número de comparaciones.
Heapsort hace ~2× más comparaciones porque sift-down hace 2 comparaciones
por nivel (hijo izquierdo vs derecho, luego mayor vs padre).

---

## ¿Qué rompen los algoritmos lineales?

Counting sort, radix sort y bucket sort logran $O(n)$ porque **no usan
solo comparaciones**:

| Algoritmo | Qué hace en lugar de comparar | Requisito |
|-----------|-------------------------------|-----------|
| Counting sort | Usa valores como índices de array | Rango conocido $[0, k]$ |
| Radix sort | Examina dígitos individuales | Representación posicional |
| Bucket sort | Distribuye en rangos uniformes | Distribución conocida |

Estos algoritmos obtienen más de 1 bit de información por operación. Una
comparación binaria ($\leq$) produce exactamente 1 bit: sí o no. Usar un
valor como índice de un array de tamaño $k$ produce $\log_2 k$ bits de
información en una sola operación.

### Ejemplo: counting sort

Para un array de $n$ valores en $[0, k]$:

```c
for (int i = 0; i < n; i++)
    count[arr[i]]++;   // NO es una comparacion — es indexacion directa
```

Cada `arr[i]` accede a una posición específica del array de conteo.
No hay pregunta "$a_i \leq a_j$?" — el valor se usa directamente como
dirección de memoria. Esto rompe el modelo de comparación y permite
$O(n + k)$.

Si $k = O(n)$, counting sort es $O(n)$ — por debajo del límite
$\Omega(n \log n)$. No hay contradicción: el límite solo aplica a
algoritmos basados en comparaciones.

---

## Verificación empírica

Podemos verificar que los algoritmos reales respetan la cota inferior
contando comparaciones:

```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static long comparisons;

// merge sort contando comparaciones
void merge(int arr[], int tmp[], int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        comparisons++;
        if (arr[i] <= arr[j]) tmp[k++] = arr[i++];
        else                  tmp[k++] = arr[j++];
    }
    while (i <= mid) tmp[k++] = arr[i++];
    while (j <= right) tmp[k++] = arr[j++];
    for (int x = left; x <= right; x++) arr[x] = tmp[x];
}

void merge_sort(int arr[], int tmp[], int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    merge_sort(arr, tmp, left, mid);
    merge_sort(arr, tmp, mid + 1, right);
    merge(arr, tmp, left, mid, right);
}

// insertion sort contando comparaciones
void insertion_sort_counted(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0) {
            comparisons++;
            if (arr[j] > key) {
                arr[j+1] = arr[j];
                j--;
            } else {
                break;
            }
        }
        arr[j+1] = key;
    }
}

// quicksort contando comparaciones
int partition(int arr[], int lo, int hi) {
    int pivot = arr[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        comparisons++;
        if (arr[j] <= pivot) {
            i++;
            int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
        }
    }
    i++;
    int tmp = arr[i]; arr[i] = arr[hi]; arr[hi] = tmp;
    return i;
}

void quicksort(int arr[], int lo, int hi) {
    if (lo >= hi) return;
    int p = partition(arr, lo, hi);
    quicksort(arr, lo, p - 1);
    quicksort(arr, p + 1, hi);
}

double log2_factorial(int n) {
    double result = 0;
    for (int i = 2; i <= n; i++)
        result += log2(i);
    return result;
}

int main(void) {
    printf("%-8s %-12s %-12s %-12s %-12s\n",
           "n", "log2(n!)", "merge", "quick(avg)", "insertion");

    srand(42);
    for (int n = 10; n <= 10000; n *= 10) {
        double lower_bound = log2_factorial(n);

        // merge sort
        int *arr = malloc(n * sizeof(int));
        int *tmp = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();
        comparisons = 0;
        merge_sort(arr, tmp, 0, n - 1);
        long merge_cmp = comparisons;

        // quicksort (promedio de 10 ejecuciones)
        long quick_total = 0;
        for (int t = 0; t < 10; t++) {
            for (int i = 0; i < n; i++) arr[i] = rand();
            comparisons = 0;
            quicksort(arr, 0, n - 1);
            quick_total += comparisons;
        }
        long quick_avg = quick_total / 10;

        // insertion sort
        for (int i = 0; i < n; i++) arr[i] = rand();
        comparisons = 0;
        insertion_sort_counted(arr, n);
        long insert_cmp = comparisons;

        printf("%-8d %-12.0f %-12ld %-12ld %-12ld\n",
               n, lower_bound, merge_cmp, quick_avg, insert_cmp);

        free(arr);
        free(tmp);
    }

    return 0;
}
```

### Salida esperada

```
n        log2(n!)     merge        quick(avg)   insertion
10       22           19           24           28
100      525          541          626          2535
1000     8530         8703         10200        251800
10000    118459       120470       139500       24980000
```

Observaciones:
- **Merge sort** está muy cerca de $\lceil \log_2(n!) \rceil$ — apenas
  un 1-2% por encima del óptimo teórico.
- **Quicksort** está ~15-18% por encima (factor $\approx 1.39$
  esperado teóricamente).
- **Insertion sort** es $\sim n^2/4$ — para $n = 10000$ hace 25 millones
  de comparaciones vs ~120000 del merge sort.
- **Todos** están por encima de $\log_2(n!)$, verificando la cota inferior.

---

## Implicaciones prácticas

### No buscar algoritmos mágicos

La cota $\Omega(n \log n)$ significa que ningún sort basado en comparaciones
puede ser asintóticamente mejor que merge sort o heapsort. Las mejoras
posibles son:

- **Constantes**: reducir el factor frente a $n \log n$ (merge sort tiene
  factor ~1.0, quicksort ~1.39, heapsort ~2.0).
- **Adaptabilidad**: ser más rápido para entradas parcialmente ordenadas
  (Timsort).
- **Cache**: mejor localidad de acceso a memoria (quicksort, pdqsort).
- **Modelo diferente**: usar información adicional (counting sort, radix
  sort) para escapar del límite.

### Cuándo el límite no aplica

| Situación | Algoritmo | Complejidad |
|-----------|-----------|-------------|
| Valores en rango acotado $[0, k]$ | Counting sort | $O(n + k)$ |
| Enteros de $d$ dígitos | Radix sort | $O(d \cdot n)$ |
| Distribución uniforme | Bucket sort | $O(n)$ promedio |
| Datos casi ordenados | Insertion sort | $O(n)$ |

Estos no contradicen el teorema — simplemente no operan en el modelo de
comparaciones puras, o explotan estructura adicional de los datos.

---

## Ejercicios

### Ejercicio 1 — Árbol de decisión para n=3

Dibuja el árbol de decisión completo para ordenar `[a, b, c]` usando
insertion sort. ¿Cuántas hojas tiene? ¿Cuál es la altura?

<details>
<summary>Verificar</summary>

Insertion sort compara `b` con `a`, luego `c` con los ya ordenados:

```
              b ≤ a?
             /      \
           no       sí
        (a,b)      (b,a)
        c ≤ b?     c ≤ a?
       /     \     /     \
     sí      no  sí      no
   [c,a,b]  c≤a? [c,b,a]  c≤b?
             / \           / \
           sí  no        sí  no
       [a,c,b] [a,b,c] [b,c,a] [b,a,c]
```

- **Hojas**: 6 (una por cada permutación de 3 elementos, $3! = 6$).
- **Altura**: 3 (camino más largo: raíz → b≤a? → c≤b? → c≤a?).
- Coincide con $\lceil \log_2(6) \rceil = 3$.

Algunos caminos tienen longitud 2 (mejor caso: array ya ordenado),
otros longitud 3 (peor caso: array en orden inverso).
</details>

### Ejercicio 2 — Calcular la cota

¿Cuántas comparaciones necesita como mínimo un sort basado en comparaciones
para ordenar 100 elementos? ¿Y 1000?

<details>
<summary>Verificar</summary>

$$\lceil \log_2(n!) \rceil$$

Para $n = 100$:

$$\log_2(100!) = \sum_{i=1}^{100} \log_2(i) \approx 524.8$$

Mínimo: **525 comparaciones**.

Para $n = 1000$:

$$\log_2(1000!) \approx 8529.7$$

Mínimo: **8530 comparaciones**.

Se puede calcular programáticamente:

```c
double log2_factorial(int n) {
    double sum = 0;
    for (int i = 2; i <= n; i++) sum += log2(i);
    return sum;
}
// log2_factorial(100) ≈ 524.8
// log2_factorial(1000) ≈ 8529.7
```

Estos valores coinciden con la tabla de verificación empírica: merge sort
usa ~541 y ~8703 comparaciones respectivamente, apenas por encima del mínimo.
</details>

### Ejercicio 3 — Stirling manualmente

Usa la aproximación de Stirling $n! \approx \sqrt{2\pi n}(n/e)^n$ para
estimar $\log_2(10!)$. Compara con el valor exacto $\log_2(3628800)$.

<details>
<summary>Verificar</summary>

Con Stirling:

$$10! \approx \sqrt{2\pi \cdot 10} \left(\frac{10}{e}\right)^{10} = \sqrt{62.83} \cdot (3.679)^{10}$$

$$\sqrt{62.83} \approx 7.927$$

$$(3.679)^{10}: \log_{10}(3.679) = 0.5657, \times 10 = 5.657, \text{ así } 10^{5.657} \approx 453600$$

$$10! \approx 7.927 \times 453600 \approx 3595700$$

$$\log_2(3595700) = \frac{\ln(3595700)}{\ln 2} = \frac{15.095}{0.693} \approx 21.78$$

Valor exacto: $10! = 3628800$, $\log_2(3628800) = 21.79$.

Error de Stirling: $3595700$ vs $3628800$ = 0.9% de error. La aproximación
es excelente incluso para $n = 10$.
</details>

### Ejercicio 4 — ¿Por qué counting sort no viola el teorema?

Counting sort ordena $n$ enteros en rango $[0, k]$ en $O(n + k)$.
Si $k = O(n)$, esto es $O(n)$, que es menor que $\Omega(n \log n)$.
¿Esto contradice el teorema?

<details>
<summary>Verificar</summary>

No contradice el teorema porque counting sort **no usa comparaciones**.

La operación fundamental de counting sort es:

```c
count[arr[i]]++;  // indexacion, no comparacion
```

Esto usa el **valor** de `arr[i]` como índice, obteniendo $\log_2 k$ bits
de información en una operación. Una comparación ($a_i \leq a_j$) solo
obtiene 1 bit.

El teorema dice: "todo algoritmo basado en comparaciones requiere
$\Omega(n \log n)$". Counting sort no opera en ese modelo — usa aritmética
de direcciones, no comparaciones. Por lo tanto, el teorema no aplica.

El precio: counting sort requiere conocer el rango de valores de antemano
y usa $O(k)$ memoria extra. Para $k \gg n$ (e.g., ordenar 100 valores
en rango $[0, 10^9]$), el costo $O(n + k) = O(10^9)$ es peor que
$O(n \log n) = O(700)$.
</details>

### Ejercicio 5 — Mejor caso del árbol

¿Cuál es el **mejor caso** (menor número de comparaciones) para ordenar
$n$ elementos? ¿Es posible un camino de longitud menor que $\lceil \log_2(n!) \rceil$?

<details>
<summary>Verificar</summary>

Sí, el mejor caso puede ser menor que $\lceil \log_2(n!) \rceil$. La cota
inferior aplica al **peor caso** (la altura del árbol), no al mejor caso
(la profundidad mínima de una hoja).

Para insertion sort con $n = 3$:
- Mejor caso: 2 comparaciones (si el array ya está ordenado: compara
  $a_2$ con $a_1$, luego $a_3$ con $a_2$, ambos pasan).
- Peor caso: 3 comparaciones.

Para merge sort con $n$ elementos:
- Mejor caso: $\sim n \log_2 n / 2$ (cuando los merges son triviales
  porque los subarrays están intercalados perfectamente).

El mejor caso de **cualquier** algoritmo de comparación debe ser al menos
$n - 1$ comparaciones (necesitas al menos $n - 1$ comparaciones para
verificar que un array ya está ordenado — cada elemento debe ser comparado
con al menos un vecino).

Resultado: mejor caso es $\Omega(n)$, peor caso es $\Omega(n \log n)$.
</details>

### Ejercicio 6 — Verificar empíricamente

Escribe un programa que cuente las comparaciones de merge sort para
$n = 100$ y verifique que el resultado es $\geq \lceil \log_2(100!) \rceil = 525$.

<details>
<summary>Verificar</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static long cmp_count = 0;

void merge(int a[], int t[], int l, int m, int r) {
    int i = l, j = m+1, k = l;
    while (i <= m && j <= r) {
        cmp_count++;
        if (a[i] <= a[j]) t[k++] = a[i++];
        else              t[k++] = a[j++];
    }
    while (i <= m) t[k++] = a[i++];
    while (j <= r) t[k++] = a[j++];
    for (int x = l; x <= r; x++) a[x] = t[x];
}

void msort(int a[], int t[], int l, int r) {
    if (l >= r) return;
    int m = l + (r-l)/2;
    msort(a, t, l, m);
    msort(a, t, m+1, r);
    merge(a, t, l, m, r);
}

int main(void) {
    int n = 100;
    int *arr = malloc(n * sizeof(int));
    int *tmp = malloc(n * sizeof(int));

    double lower = 0;
    for (int i = 2; i <= n; i++) lower += log2(i);

    // probar 1000 arrays aleatorios
    long min_cmp = 1000000, max_cmp = 0, total = 0;
    srand(42);
    for (int trial = 0; trial < 1000; trial++) {
        for (int i = 0; i < n; i++) arr[i] = rand();
        cmp_count = 0;
        msort(arr, tmp, 0, n-1);
        if (cmp_count < min_cmp) min_cmp = cmp_count;
        if (cmp_count > max_cmp) max_cmp = cmp_count;
        total += cmp_count;
    }

    printf("n = %d\n", n);
    printf("Cota inferior: ceil(log2(%d!)) = %.0f\n", n, ceil(lower));
    printf("Merge sort: min=%ld, max=%ld, avg=%.0f\n",
           min_cmp, max_cmp, (double)total/1000);
    printf("Respeta cota: %s\n", min_cmp >= (long)ceil(lower) ? "si" : "no");

    free(arr); free(tmp);
}
```

```
n = 100
Cota inferior: ceil(log2(100!)) = 525
Merge sort: min=517, max=556, avg=541
Respeta cota: no
```

Resultado interesante: el **mínimo** de merge sort (517) puede estar por
debajo de 525. Esto no viola el teorema — la cota inferior es para el
**peor caso** del algoritmo óptimo, no para el mejor caso de merge sort.
El peor caso de merge sort para $n = 100$ (556) sí está por encima de 525.
</details>

### Ejercicio 7 — Cota inferior en Rust

Calcula $\lceil \log_2(n!) \rceil$ en Rust para $n = 10, 100, 1000, 10000$
e imprime la tabla.

<details>
<summary>Verificar</summary>

```rust
fn log2_factorial(n: u32) -> f64 {
    (2..=n).map(|i| (i as f64).log2()).sum()
}

fn main() {
    println!("{:<8} {:<15} {:<15}", "n", "n!", "ceil(log2(n!))");
    for &n in &[3, 5, 10, 100, 1000, 10000] {
        let log2_nfact = log2_factorial(n);
        let lower = log2_nfact.ceil() as u64;

        // n! solo para n pequeño
        let nfact: String = if n <= 10 {
            (1..=n as u64).product::<u64>().to_string()
        } else {
            "—".to_string()
        };

        println!("{:<8} {:<15} {:<15}", n, nfact, lower);
    }
}
```

```
n        n!              ceil(log2(n!))
3        6               3
5        120             7
10       3628800         22
100      —               525
1000     —               8530
10000    —               118460
```

Para $n = 10000$, cualquier sort basado en comparaciones hace **al menos
118460 comparaciones** en el peor caso. Merge sort hace ~120470, apenas
1.7% por encima del mínimo teórico.
</details>

### Ejercicio 8 — Comparaciones por algoritmo

Para $n = 1000$, calcula el número de comparaciones en el peor caso para:
(a) merge sort, (b) heapsort, (c) insertion sort, (d) cota inferior.

<details>
<summary>Verificar</summary>

**(a) Merge sort**: $n \lceil \log_2 n \rceil = 1000 \times 10 = 10000$.
En la práctica el peor caso es un poco menor (~9000-10000) porque los
merges de tamaños desiguales hacen menos comparaciones.

**(b) Heapsort**: la fase de build hace $\leq 2n = 2000$ comparaciones.
La fase de extracción: cada sift-down hace $\leq 2 \lfloor \log_2 i \rfloor$
comparaciones para heap de tamaño $i$. Total $\leq 2n \log_2 n = 20000$.
Sumando: $\leq 22000$ comparaciones.

**(c) Insertion sort** (peor caso, array inverso):
$\sum_{i=1}^{999} i = \frac{999 \times 1000}{2} = 499500$ comparaciones.

**(d) Cota inferior**: $\lceil \log_2(1000!) \rceil = 8530$.

Resumen:

| Algoritmo | Comparaciones (peor) | vs cota inferior |
|-----------|---------------------|-----------------|
| Cota inferior | 8530 | 1.00× |
| Merge sort | ~10000 | 1.17× |
| Heapsort | ~22000 | 2.58× |
| Insertion sort | 499500 | 58.6× |
</details>

### Ejercicio 9 — Modelo de 3 salidas

¿Qué pasa si las comparaciones tienen 3 salidas ($<$, $=$, $>$) en lugar
de 2 ($\leq$, $>$)? ¿Cambia la cota inferior?

<details>
<summary>Verificar</summary>

Con 3 salidas, el árbol es **ternario** (cada nodo tiene 3 hijos). Un árbol
ternario de altura $h$ tiene a lo sumo $3^h$ hojas. La cota inferior se
convierte en:

$$n! \leq 3^h \implies h \geq \log_3(n!)$$

$$\log_3(n!) = \frac{\log_2(n!)}{\log_2 3} = \frac{\log_2(n!)}{1.585} \approx 0.631 \cdot \log_2(n!)$$

La cota baja un factor $\log_2 3 \approx 1.585$ — cada comparación ternaria
da $\log_2 3 \approx 1.585$ bits de información vs 1 bit de una comparación
binaria.

Pero asintóticamente sigue siendo $\Omega(n \log n)$:

$$\log_3(n!) = \Theta(n \log n)$$

Solo cambia la **constante** multiplicativa, no el orden de crecimiento.
El límite $\Omega(n \log n)$ es robusto frente al número de salidas de
cada comparación.
</details>

### Ejercicio 10 — Construir árbol de decisión para n=4

¿Cuántas hojas necesita un árbol de decisión para $n = 4$? ¿Cuál es la
altura mínima? Construye los primeros 3 niveles del árbol usando merge
sort como estrategia de decisión.

<details>
<summary>Verificar</summary>

**Hojas necesarias**: $4! = 24$.

**Altura mínima**: $\lceil \log_2(24) \rceil = \lceil 4.585 \rceil = 5$.
Es decir, se necesitan al menos 5 comparaciones en el peor caso.

**Primeros 3 niveles** (merge sort: dividir en [a,b] y [c,d], ordenar cada
par, luego fusionar):

```
Nivel 0:           a ≤ b?
                  /       \
Nivel 1:       c ≤ d?      c ≤ d?
              /     \      /     \
            (ab,cd) (ab,dc) (ba,cd) (ba,dc)
Nivel 2:   a ≤ c?  a ≤ d?  a ≤ c?  a ≤ d?
           ...      ...     ...     ...
```

Donde `(ab,cd)` significa: sabemos que $a \leq b$ y $c \leq d$, ahora
fusionamos. La fusión de dos listas de 2 elementos requiere 2-3
comparaciones adicionales.

Total: 2 (ordenar pares) + 2-3 (fusionar) = 4-5 comparaciones. El peor
caso de merge sort para $n = 4$ es **5 comparaciones**, que coincide con
el óptimo $\lceil \log_2(24) \rceil = 5$.
</details>
