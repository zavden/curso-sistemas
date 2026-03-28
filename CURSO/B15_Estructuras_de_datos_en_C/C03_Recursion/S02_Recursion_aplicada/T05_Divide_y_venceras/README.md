# Divide y vencerás

## La idea

**Divide y vencerás** (*divide and conquer*, D&C) es una estrategia algorítmica
que resuelve un problema en tres pasos:

1. **Dividir**: partir el problema en subproblemas más pequeños del mismo tipo.
2. **Conquistar**: resolver cada subproblema recursivamente (o directamente si
   es lo bastante pequeño — caso base).
3. **Combinar**: unir las soluciones de los subproblemas para formar la solución
   del problema original.

```
        Problema original
           /        \
     Subproblema   Subproblema        ← DIVIDIR
        |              |
     Solución       Solución          ← CONQUISTAR (recursión)
        \              /
      Solución combinada              ← COMBINAR
```

La clave de D&C es que **dividir y combinar son baratos**, y al reducir el
tamaño de los subproblemas, la recursión converge rápidamente al caso base.

---

## D&C vs otras estrategias recursivas

| Estrategia | Estructura | Subproblemas | Ejemplo |
|-----------|-----------|--------------|---------|
| **Recursión lineal** | Un subproblema de tamaño $n-1$ | 1 | Factorial |
| **Divide y vencerás** | Varios subproblemas de tamaño $\approx n/k$ | 2 o más | Merge sort |
| **Backtracking** | Explorar opciones, retroceder si falla | Variable | N-reinas |
| **Programación dinámica** | Subproblemas superpuestos, memorizar | Variable | Fibonacci memo |

La diferencia fundamental: en D&C los subproblemas son **independientes** y
**no se superponen** (a diferencia de Fibonacci naive, donde `fib(3)` se calcula
múltiples veces).  Cuando sí se superponen, se pasa de D&C a programación
dinámica.

---

## Pseudocódigo general

```
funcion dac(problema):
    si es_caso_base(problema):
        return resolver_directo(problema)

    (sub1, sub2, ...) = dividir(problema)

    sol1 = dac(sub1)
    sol2 = dac(sub2)
    ...

    return combinar(sol1, sol2, ...)
```

Los tres puntos de diseño:

| Decisión | Pregunta | Impacto |
|----------|----------|---------|
| **Cuántos subproblemas** | ¿Dividir en 2? ¿En 3? ¿En $k$? | Afecta la recurrencia |
| **Tamaño de cada uno** | ¿Mitades iguales? ¿Desiguales? | Afecta balance y profundidad |
| **Costo de combinar** | ¿$O(1)$? ¿$O(n)$? ¿$O(n^2)$? | Domina la complejidad total |

---

## Ejemplo introductorio: suma de un array

Antes de abordar los algoritmos de ordenamiento, veamos D&C en un problema
simple: sumar los elementos de un array.

### Enfoque

1. **Dividir**: partir el array en dos mitades.
2. **Conquistar**: sumar cada mitad recursivamente.
3. **Combinar**: sumar los dos resultados.

Caso base: array de 0 o 1 elemento.

### Implementación en C

```c
int sum_dac(const int arr[], int lo, int hi) {
    if (lo > hi) return 0;             /* array vacío */
    if (lo == hi) return arr[lo];      /* un solo elemento */

    int mid = lo + (hi - lo) / 2;     /* dividir */
    int left  = sum_dac(arr, lo, mid);        /* conquistar */
    int right = sum_dac(arr, mid + 1, hi);
    return left + right;                       /* combinar */
}
```

### Implementación en Rust

```rust
fn sum_dac(arr: &[i32]) -> i32 {
    match arr.len() {
        0 => 0,
        1 => arr[0],
        _ => {
            let mid = arr.len() / 2;
            sum_dac(&arr[..mid]) + sum_dac(&arr[mid..])   // dividir + combinar
        }
    }
}
```

### Traza para [3, 1, 4, 1, 5]

```
sum([3, 1, 4, 1, 5])
├── sum([3, 1])
│   ├── sum([3])  → 3
│   └── sum([1])  → 1
│   → 3 + 1 = 4
└── sum([4, 1, 5])
    ├── sum([4])  → 4
    └── sum([1, 5])
        ├── sum([1])  → 1
        └── sum([5])  → 5
        → 1 + 5 = 6
    → 4 + 6 = 10
→ 4 + 10 = 14
```

### Análisis

- **Dividir**: $O(1)$ — solo calcular el punto medio.
- **Combinar**: $O(1)$ — una suma.
- **Subproblemas**: 2 de tamaño $n/2$.
- **Recurrencia**: $T(n) = 2T(n/2) + O(1)$.
- **Solución**: $O(n)$ — lo mismo que un bucle simple.

Para la suma, D&C no ofrece ventaja sobre el bucle lineal.  Pero el mismo
patrón con una operación de combinar más sofisticada produce algoritmos
extraordinarios, como merge sort.

---

## Merge sort (preview)

Merge sort es el ejemplo definitivo de D&C.  Se profundizará en C08
(Algoritmos de ordenamiento), pero el algoritmo es inseparable de D&C, así que
lo presentamos aquí en su forma completa.

### Algoritmo

1. **Dividir**: partir el array en dos mitades (punto medio).
2. **Conquistar**: ordenar cada mitad recursivamente.
3. **Combinar**: **mezclar** (*merge*) las dos mitades ordenadas en un array
   ordenado.

La operación de mezcla es la clave: toma dos secuencias ya ordenadas y las
combina en una sola secuencia ordenada, recorriendo ambas con un puntero y
eligiendo siempre el menor elemento.

### Implementación en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void merge(int arr[], int lo, int mid, int hi) {
    int n = hi - lo + 1;
    int *tmp = malloc(n * sizeof(int));
    if (!tmp) { perror("malloc"); exit(1); }

    int i = lo;           /* puntero mitad izquierda */
    int j = mid + 1;      /* puntero mitad derecha   */
    int k = 0;            /* puntero en tmp           */

    while (i <= mid && j <= hi) {
        if (arr[i] <= arr[j]) {       /* <= para estabilidad */
            tmp[k++] = arr[i++];
        } else {
            tmp[k++] = arr[j++];
        }
    }
    while (i <= mid) tmp[k++] = arr[i++];   /* residuo izquierdo */
    while (j <= hi)  tmp[k++] = arr[j++];   /* residuo derecho   */

    memcpy(&arr[lo], tmp, n * sizeof(int));
    free(tmp);
}

void merge_sort(int arr[], int lo, int hi) {
    if (lo >= hi) return;              /* caso base: 0 o 1 elemento */

    int mid = lo + (hi - lo) / 2;     /* dividir */
    merge_sort(arr, lo, mid);          /* conquistar mitad izquierda */
    merge_sort(arr, mid + 1, hi);      /* conquistar mitad derecha   */
    merge(arr, lo, mid, hi);           /* combinar */
}

int main(void) {
    int arr[] = {38, 27, 43, 3, 9, 82, 10};
    int n = sizeof(arr) / sizeof(arr[0]);

    merge_sort(arr, 0, n - 1);

    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    return 0;
}
```

### Implementación en Rust

```rust
fn merge_sort(arr: &mut [i32]) {
    let len = arr.len();
    if len <= 1 { return; }

    let mid = len / 2;

    // Conquistar: ordenar cada mitad
    merge_sort(&mut arr[..mid]);
    merge_sort(&mut arr[mid..]);

    // Combinar: mezclar las dos mitades ordenadas
    let left = arr[..mid].to_vec();
    let right = arr[mid..].to_vec();

    let (mut i, mut j, mut k) = (0, 0, 0);
    while i < left.len() && j < right.len() {
        if left[i] <= right[j] {
            arr[k] = left[i];
            i += 1;
        } else {
            arr[k] = right[j];
            j += 1;
        }
        k += 1;
    }
    while i < left.len() { arr[k] = left[i]; i += 1; k += 1; }
    while j < right.len() { arr[k] = right[j]; j += 1; k += 1; }
}

fn main() {
    let mut arr = [38, 27, 43, 3, 9, 82, 10];
    merge_sort(&mut arr);
    println!("{:?}", arr);
}
```

### Traza para [38, 27, 43, 3]

```
merge_sort([38, 27, 43, 3])
├── merge_sort([38, 27])
│   ├── merge_sort([38])  → [38]
│   └── merge_sort([27])  → [27]
│   └── merge([38], [27]) → [27, 38]
└── merge_sort([43, 3])
    ├── merge_sort([43])  → [43]
    └── merge_sort([3])   → [3]
    └── merge([43], [3])  → [3, 43]
└── merge([27, 38], [3, 43]) → [3, 27, 38, 43]
```

La operación `merge` en detalle para el paso final:

```
Izquierda: [27, 38]     Derecha: [3, 43]     Resultado: []
           ^i                    ^j

Paso 1: 27 vs 3 → 3 menor                    [3]
                                  ^j                ← j avanza

Paso 2: 27 vs 43 → 27 menor                  [3, 27]
             ^i                                     ← i avanza

Paso 3: 38 vs 43 → 38 menor                  [3, 27, 38]
                ^i                                  ← i se agota

Paso 4: residuo derecho                      [3, 27, 38, 43]
```

### Análisis de merge sort

**Recurrencia**:

$$T(n) = 2T(n/2) + O(n)$$

- 2 subproblemas de tamaño $n/2$.
- `merge` recorre todos los elementos: $O(n)$.

**Desarrollo** (árbol de recursión):

```
Nivel 0:  1 problema  de tamaño n     → trabajo: n
Nivel 1:  2 problemas de tamaño n/2   → trabajo: 2 × n/2 = n
Nivel 2:  4 problemas de tamaño n/4   → trabajo: 4 × n/4 = n
  ...
Nivel k:  2ᵏ problemas de tamaño n/2ᵏ → trabajo: n
```

Hay $\log_2 n$ niveles (cada nivel divide por 2).  Cada nivel hace $O(n)$
trabajo total.  Por lo tanto:

$$T(n) = O(n \log n)$$

### Propiedades de merge sort

| Propiedad | Valor |
|-----------|-------|
| Tiempo peor caso | $O(n \log n)$ — **garantizado** |
| Tiempo mejor caso | $O(n \log n)$ |
| Espacio adicional | $O(n)$ — para el array temporal en `merge` |
| Estable | Sí (con `<=` en la comparación) |
| In-place | No (requiere memoria auxiliar) |
| Profundidad de recursión | $O(\log n)$ — muy seguro |

La principal desventaja es el espacio $O(n)$ adicional.  Esto motiva quicksort,
que ordena in-place.

---

## Quicksort (preview)

Quicksort es el otro gran algoritmo de D&C para ordenamiento.  Invierte la
estrategia de merge sort:

| Aspecto | Merge sort | Quicksort |
|---------|-----------|-----------|
| Trabajo principal | En **combinar** (merge) | En **dividir** (partición) |
| Dividir | Trivial (punto medio) | Requiere partición alrededor de pivote |
| Combinar | $O(n)$ merge | Trivial (los subarrays ya están en su lugar) |
| Estabilidad | Estable | No estable (versión estándar) |
| Espacio | $O(n)$ | $O(\log n)$ — in-place |

### Algoritmo

1. **Dividir**: elegir un **pivote** y reorganizar el array de modo que:
   - Todos los elementos $\leq$ pivote quedan a la izquierda.
   - Todos los elementos $>$ pivote quedan a la derecha.
   - El pivote queda en su posición final.
2. **Conquistar**: ordenar recursivamente la parte izquierda y la derecha.
3. **Combinar**: nada — el array ya está ordenado in-place.

### Partición (esquema Lomuto)

El esquema Lomuto usa el último elemento como pivote y recorre el array con
dos índices: `i` (frontera de los elementos menores) y `j` (exploración):

```c
int partition(int arr[], int lo, int hi) {
    int pivot = arr[hi];
    int i = lo;   /* frontera: todo arr[lo..i-1] <= pivot */

    for (int j = lo; j < hi; j++) {
        if (arr[j] <= pivot) {
            /* swap arr[i] y arr[j] */
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++;
        }
    }
    /* colocar pivote en su posición final */
    int tmp = arr[i];
    arr[i] = arr[hi];
    arr[hi] = tmp;
    return i;   /* índice del pivote */
}
```

### Implementación en C

```c
void quicksort(int arr[], int lo, int hi) {
    if (lo >= hi) return;

    int pivot_idx = partition(arr, lo, hi);
    quicksort(arr, lo, pivot_idx - 1);    /* izquierda del pivote */
    quicksort(arr, pivot_idx + 1, hi);    /* derecha del pivote   */
}
```

### Implementación en Rust

```rust
fn partition(arr: &mut [i32]) -> usize {
    let hi = arr.len() - 1;
    let pivot = arr[hi];
    let mut i = 0;

    for j in 0..hi {
        if arr[j] <= pivot {
            arr.swap(i, j);
            i += 1;
        }
    }
    arr.swap(i, hi);
    i
}

fn quicksort(arr: &mut [i32]) {
    if arr.len() <= 1 { return; }

    let pivot_idx = partition(arr);

    let (left, right) = arr.split_at_mut(pivot_idx);
    quicksort(left);
    quicksort(&right[1..]);   // saltar el pivote
}

fn main() {
    let mut arr = [38, 27, 43, 3, 9, 82, 10];
    quicksort(&mut arr);
    println!("{:?}", arr);
}
```

Nota el uso de `split_at_mut`: Rust no permite tener dos `&mut` al mismo
slice, pero `split_at_mut` produce dos slices mutables **disjuntos**, lo cual
es seguro.

### Traza para [10, 3, 7, 5]

```
quicksort([10, 3, 7, 5])   pivote = 5
  partición:
    j=0: 10 > 5 → no swap              [10, 3, 7, 5]  i=0
    j=1: 3 ≤ 5 → swap(0,1)             [3, 10, 7, 5]  i=1
    j=2: 7 > 5 → no swap               [3, 10, 7, 5]  i=1
    colocar pivote: swap(1,3)           [3, 5, 7, 10]  pivote en idx=1

  quicksort([3])        → [3]             (caso base)
  quicksort([7, 10])    pivote = 10
    partición:
      j=0: 7 ≤ 10 → swap(0,0)          [7, 10]  i=1
      colocar pivote: swap(1,1)         [7, 10]  pivote en idx=1
    quicksort([7])      → [7]             (caso base)
    quicksort([])       → []              (caso base)

Resultado: [3, 5, 7, 10]
```

### Análisis de quicksort

**Mejor caso y caso promedio**: el pivote divide el array en mitades
aproximadamente iguales.

$$T(n) = 2T(n/2) + O(n) = O(n \log n)$$

**Peor caso**: el pivote es siempre el menor o mayor elemento (array ya
ordenado con pivote Lomuto).

$$T(n) = T(n-1) + O(n) = O(n^2)$$

El peor caso se mitiga con **pivote aleatorio** o **mediana de tres**.

| Propiedad | Valor |
|-----------|-------|
| Tiempo promedio | $O(n \log n)$ |
| Tiempo peor caso | $O(n^2)$ — pero raro con buen pivote |
| Espacio | $O(\log n)$ de pila (in-place) |
| Estable | No (la partición intercambia elementos no adyacentes) |
| In-place | Sí |

---

## El teorema maestro

El **teorema maestro** (*Master Theorem*) proporciona la solución de
recurrencias de la forma:

$$T(n) = aT(n/b) + O(n^c)$$

donde:
- $a$ = número de subproblemas.
- $b$ = factor de reducción.
- $n^c$ = costo de dividir + combinar.

### Los tres casos

Sea $x = \log_b a$ (el exponente crítico):

| Caso | Condición | Resultado | Intuición |
|------|-----------|-----------|-----------|
| 1 | $c < x$ | $O(n^x)$ | El trabajo de los subproblemas domina |
| 2 | $c = x$ | $O(n^c \log n)$ | Equilibrio: cada nivel aporta lo mismo |
| 3 | $c > x$ | $O(n^c)$ | El trabajo de dividir/combinar domina |

### Aplicaciones

| Algoritmo | $a$ | $b$ | $c$ | $x = \log_b a$ | Caso | Resultado |
|-----------|-----|-----|-----|-----------------|------|-----------|
| Búsqueda binaria | 1 | 2 | 0 | 0 | 2 | $O(\log n)$ |
| Merge sort | 2 | 2 | 1 | 1 | 2 | $O(n \log n)$ |
| Suma D&C | 2 | 2 | 0 | 1 | 1 | $O(n)$ |
| Karatsuba | 3 | 2 | 1 | $\approx 1.58$ | 1 | $O(n^{1.58})$ |
| Strassen | 7 | 2 | 2 | $\approx 2.81$ | 1 | $O(n^{2.81})$ |

El teorema maestro se aplica cuando los subproblemas son de tamaño **igual**.
Para particiones desiguales (como quicksort en el peor caso) se requieren otros
métodos de análisis.

---

## Merge sort vs quicksort: resumen comparativo

| Criterio | Merge sort | Quicksort |
|----------|-----------|-----------|
| Peor caso | $O(n \log n)$ ✓ | $O(n^2)$ ✗ |
| Caso promedio | $O(n \log n)$ | $O(n \log n)$ |
| Constante oculta | Mayor (copia) | Menor (in-place, cache-friendly) |
| Espacio | $O(n)$ | $O(\log n)$ |
| Estable | Sí | No |
| En la práctica | Preferido para listas enlazadas | Preferido para arrays |
| En la biblioteca estándar | Rust `sort_stable()`, Python `sorted()` | C `qsort()`, variantes híbridas |

En la práctica, las implementaciones modernas usan **algoritmos híbridos**:
introsort (quicksort + heapsort como fallback), timsort (merge sort + insertion
sort para runs naturales).  Los veremos en C08.

---

## Otros algoritmos D&C clásicos (menciones)

Estos algoritmos se estudian en profundidad en otros bloques del curso.  Los
mencionamos aquí para mostrar la universalidad de D&C:

| Algoritmo | Problema | División | Combinación | Complejidad |
|-----------|----------|----------|-------------|-------------|
| **Búsqueda binaria** | Buscar en array ordenado | Descartar mitad | Ninguna | $O(\log n)$ |
| **Karatsuba** | Multiplicar enteros grandes | 3 multiplicaciones de mitad de dígitos | Sumas y shifts | $O(n^{1.58})$ |
| **Strassen** | Multiplicar matrices | 7 multiplicaciones de submatrices | Sumas de matrices | $O(n^{2.81})$ |
| **Closest pair** | Par de puntos más cercano | Dividir por coordenada $x$ | Verificar franja central | $O(n \log n)$ |
| **FFT** | Transformada de Fourier | Coeficientes pares/impares | Butterfly operation | $O(n \log n)$ |

---

## El árbol de recursión como herramienta de análisis

El árbol de recursión visualiza el trabajo en cada nivel y es la herramienta
fundamental para analizar algoritmos D&C:

### Merge sort ($T(n) = 2T(n/2) + n$)

```
Nivel 0:    [────────── n ──────────]              trabajo: n
             /                    \
Nivel 1:  [──── n/2 ────]  [──── n/2 ────]         trabajo: n
           /        \       /        \
Nivel 2: [n/4]    [n/4]  [n/4]    [n/4]            trabajo: n
           ...        ...    ...       ...
Nivel k:  [1][1][1] ... [1][1][1]                  trabajo: n

Total de niveles: log₂(n)
Trabajo por nivel: n
Total: n × log₂(n)
```

### Suma D&C ($T(n) = 2T(n/2) + 1$)

```
Nivel 0:    [──── n ────]                   trabajo: 1
             /          \
Nivel 1:  [n/2]       [n/2]                trabajo: 2
           / \         / \
Nivel 2: [n/4][n/4] [n/4][n/4]            trabajo: 4
           ...                              ...
Nivel k: [1][1]...[1]                      trabajo: n

Total: 1 + 2 + 4 + ... + n = 2n - 1 = O(n)
```

El trabajo se concentra en las **hojas** (caso 1 del teorema maestro).

### Quicksort peor caso ($T(n) = T(n-1) + n$)

```
Nivel 0:  [──── n ────]                    trabajo: n
          /
Nivel 1: [── n-1 ──]                      trabajo: n-1
         /
Nivel 2: [n-2]                            trabajo: n-2
         ...
Nivel n-1: [1]                            trabajo: 1

Total: n + (n-1) + ... + 1 = n(n+1)/2 = O(n²)
```

Aquí no hay división real — cada nivel elimina solo 1 elemento.  El árbol
degenera en una lista (como recursión lineal).

---

## D&C y profundidad de recursión

Una ventaja fundamental de D&C con división balanceada: la profundidad de
recursión es $O(\log n)$, extraordinariamente pequeña:

| $n$ | $\log_2 n$ |
|-----|-----------|
| 1,000 | 10 |
| 1,000,000 | 20 |
| 1,000,000,000 | 30 |
| $2^{64}$ | 64 |

Con marcos de ~64 bytes, ordenar mil millones de elementos con merge sort
consume apenas $30 \times 64 = 1920$ bytes de pila.  El stack overflow no es
una preocupación práctica en D&C bien implementado.

Excepción: quicksort con pivote malo puede tener profundidad $O(n)$ — una de
las razones por las que las implementaciones reales usan *tail call* en la
partición más grande (recursión solo en la más pequeña), limitando la
profundidad a $O(\log n)$ incluso en el peor caso.

---

## D&C en Rust: slices y ownership

Los slices de Rust (`&[T]` y `&mut [T]`) son ideales para D&C:

### División sin copia

```rust
let (left, right) = arr.split_at(mid);       // dos &[T] inmutables
let (left, right) = arr.split_at_mut(mid);   // dos &mut [T] disjuntos
```

No se copia memoria — cada slice es un puntero + longitud al segmento
original.  Esto es $O(1)$.

### El patrón D&C con slices

```rust
fn dac(arr: &[i32]) -> Resultado {
    if arr.len() <= 1 { return caso_base(arr); }
    let mid = arr.len() / 2;
    let sol_izq = dac(&arr[..mid]);
    let sol_der = dac(&arr[mid..]);
    combinar(sol_izq, sol_der)
}
```

Este patrón es más limpio que la versión C con índices `lo, hi`:
- No hay riesgo de confundir índices.
- El `len()` de cada slice refleja directamente el tamaño del subproblema.
- El borrow checker garantiza que no se modifica el array original si es `&[T]`.

### Mutación con split_at_mut

Para quicksort (in-place):

```rust
let (left, right) = arr.split_at_mut(pivot_idx);
quicksort(left);
quicksort(&mut right[1..]);  // saltar el pivote
```

El borrow checker verifica en compilación que los dos rangos son disjuntos —
imposible tener aliasing de punteros mutables.  En C, esto es responsabilidad
del programador (y fuente de bugs sutiles).

---

## D&C en C: índices vs punteros

En C hay dos estilos equivalentes:

### Estilo índices (lo, hi)

```c
void algo(int arr[], int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    algo(arr, lo, mid);
    algo(arr, mid + 1, hi);
}
```

Ventaja: claro, familiar.  Desventaja: arrastrar `arr`, `lo`, `hi` por todas
las funciones.

### Estilo puntero + tamaño

```c
void algo(int *arr, int n) {
    int mid = n / 2;
    algo(arr, mid);
    algo(arr + mid, n - mid);
}
```

Ventaja: firma más simple, similar a slices de Rust.  Desventaja: la
aritmética de punteros puede confundir.

Ambos son correctos.  El estilo índices es más seguro para principiantes.

---

## Cuándo usar D&C

### Usar cuando

1. El problema se puede dividir en subproblemas **independientes** del mismo
   tipo.
2. La división y la combinación son eficientes ($O(n)$ o menos).
3. Se busca complejidad $O(n \log n)$ o mejor.
4. La profundidad $O(\log n)$ es aceptable para la pila.

### No usar cuando

1. Los subproblemas se **superponen** — usar programación dinámica (memoización).
2. La combinación es tan cara como resolver el problema directamente.
3. Un bucle simple logra la misma complejidad (como en la suma de un array).
4. El problema no se descompone naturalmente en partes similares.

---

## Ejercicios

### Ejercicio 1 — Traza de merge sort

Traza completa de `merge_sort([5, 2, 8, 1, 9, 3])`.  Muestra el árbol de
recursión, cada operación `merge`, y el estado del array después de cada merge.

<details>
<summary>Predice cuántas operaciones merge se ejecutan</summary>

El array tiene 6 elementos.  Divisiones: [5,2,8] y [1,9,3].  Luego [5] y [2,8],
[1] y [9,3], etc.  Total de merges: 5 (una por cada nodo interno del árbol).
Resultado final: [1, 2, 3, 5, 8, 9].
</details>

### Ejercicio 2 — Traza de quicksort

Traza `quicksort([7, 2, 1, 6, 8, 5, 3, 4])` con pivote Lomuto (último
elemento).  Muestra cada partición paso a paso, el índice del pivote, y los
subarrays resultantes.

<details>
<summary>Predice la primera partición</summary>

Pivote = 4.  Después de la partición: [2, 1, 3, 4, 8, 5, 7, 6] (o similar —
el orden de los elementos menores que el pivote puede variar, pero el 4 queda
en la posición correcta, índice 3).
</details>

### Ejercicio 3 — Máximo con D&C

Implementa una función que encuentre el máximo de un array usando D&C en C y
Rust.  Compara la complejidad con el enfoque lineal.

<details>
<summary>Predice la recurrencia</summary>

$T(n) = 2T(n/2) + O(1)$ — misma que la suma D&C.  Resultado: $O(n)$, igual
que el enfoque lineal.  D&C no aporta ventaja aquí, pero el ejercicio practica
el patrón.  Comparaciones totales: $n - 1$ (igual que iterativo).
</details>

### Ejercicio 4 — Aplicar el teorema maestro

Resuelve las siguientes recurrencias usando el teorema maestro:

1. $T(n) = 4T(n/2) + n$
2. $T(n) = 2T(n/4) + \sqrt{n}$
3. $T(n) = 3T(n/3) + n$
4. $T(n) = T(n/2) + n^2$

<details>
<summary>Soluciones</summary>

1. $a=4, b=2, c=1, x=\log_2 4 = 2$.  Caso 1 ($c < x$): $O(n^2)$.
2. $a=2, b=4, c=0.5, x=\log_4 2 = 0.5$.  Caso 2 ($c = x$): $O(\sqrt{n}\log n)$.
3. $a=3, b=3, c=1, x=\log_3 3 = 1$.  Caso 2 ($c = x$): $O(n \log n)$.
4. $a=1, b=2, c=2, x=\log_2 1 = 0$.  Caso 3 ($c > x$): $O(n^2)$.
</details>

### Ejercicio 5 — Merge sort estable vs inestable

Modifica el merge sort en C cambiando `<=` por `<` en la comparación del merge.
Crea un array de structs `{int value; int original_index;}` y demuestra que:
- Con `<=`: los elementos iguales mantienen su orden relativo (estable).
- Con `<`: los elementos iguales pueden cambiar de orden (inestable).

<details>
<summary>Ejemplo de entrada</summary>

```c
typedef struct { int value; int index; } Item;
Item arr[] = {{3,'a'}, {1,'b'}, {3,'c'}, {1,'d'}, {2,'e'}};
```

Estable: los dos 3s aparecen como (3,'a'), (3,'c'); los dos 1s como (1,'b'),
(1,'d').  Inestable: podrían intercambiarse.
</details>

### Ejercicio 6 — Quicksort: peor caso

Demuestra el peor caso de quicksort (Lomuto) alimentándolo con un array ya
ordenado.  Mide el número de comparaciones y verifica que es $\approx n^2/2$.
Luego implementa pivote aleatorio y muestra que el peor caso desaparece.

<details>
<summary>Pista para pivote aleatorio</summary>

Antes de la partición, intercambiar `arr[hi]` con `arr[rand() % (hi-lo+1) + lo]`.
En Rust: `arr.swap(rng.gen_range(0..arr.len()), arr.len() - 1)` usando la
crate `rand`, o para un ejercicio sin dependencias: un LCG simple o
`std::collections::hash_map::RandomState` como fuente de entropía.
</details>

### Ejercicio 7 — Merge sort con conteo de inversiones

Una **inversión** es un par $(i, j)$ con $i < j$ pero $arr[i] > arr[j]$.
Modifica merge sort para contar las inversiones mientras ordena.  Pista: en el
merge, cada vez que eliges un elemento de la mitad derecha, cuenta cuántos
elementos quedan en la mitad izquierda (todos ellos forman inversiones con ese
elemento).

<details>
<summary>Ejemplo</summary>

Array [2, 4, 1, 3, 5]: inversiones = (2,1), (4,1), (4,3) = 3.
El merge sort modificado retorna tanto el array ordenado como el conteo.
Complejidad: $O(n \log n)$, mucho mejor que contar inversiones por fuerza
bruta $O(n^2)$.
</details>

### Ejercicio 8 — Profundidad de recursión

Instrumenta merge sort y quicksort (Lomuto, sin pivote aleatorio) para medir la
profundidad máxima de recursión.  Prueba con arrays de tamaño 1000:
- Array aleatorio.
- Array ya ordenado.
- Array ordenado en reversa.

<details>
<summary>Predicciones</summary>

Merge sort: siempre $\lceil\log_2 1000\rceil = 10$ niveles, independiente del
input.  Quicksort (Lomuto): ~10 niveles para aleatorio, ~1000 para ya ordenado
(peor caso).  Esto demuestra por qué quicksort necesita pivote aleatorio o
introsort como fallback.
</details>

### Ejercicio 9 — Potencia rápida con D&C

Implementa la exponenciación rápida (*fast exponentiation*): $x^n$ en
$O(\log n)$ multiplicaciones usando D&C.

$$
x^n =
\begin{cases}
1 & n = 0 \\
(x^{n/2})^2 & n \text{ par} \\
x \cdot (x^{(n-1)/2})^2 & n \text{ impar}
\end{cases}
$$

Implementa en C y Rust.  Compara el número de multiplicaciones contra el
enfoque iterativo simple ($n - 1$ multiplicaciones).

<details>
<summary>Traza para x=2, n=10</summary>

$2^{10} = (2^5)^2$.  $2^5 = 2 \cdot (2^2)^2$.  $2^2 = (2^1)^2$.  $2^1 = 2
\cdot (2^0)^2 = 2 \cdot 1 = 2$.  Total: 4 multiplicaciones (vs 9 en
iterativo).  $\lfloor\log_2 10\rfloor + \text{popcount}(10) - 1 = 3 + 2 - 1 = 4$.
</details>

### Ejercicio 10 — D&C en Rust con genéricos

Implementa una función genérica `merge_sort<T: Ord + Clone>(arr: &mut [T])`
que ordene cualquier tipo que implemente `Ord` y `Clone`.  Prueba con `i32`,
`String`, y un struct propio que derive `Ord`.

<details>
<summary>Pista para el struct</summary>

```rust
#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd)]
struct Student {
    grade: u32,    // campo principal de ordenación
    name: String,  // desempate
}
```

`#[derive(Ord)]` genera orden lexicográfico por campos en orden de declaración.
Si quieres ordenar solo por `grade`, implementa `Ord` manualmente.
</details>
