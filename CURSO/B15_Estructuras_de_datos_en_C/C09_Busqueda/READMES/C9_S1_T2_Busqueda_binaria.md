# Búsqueda binaria

## Objetivo

Dominar la búsqueda binaria, el algoritmo fundamental para buscar en
secuencias ordenadas:

- Complejidad $O(\log n)$ — cada paso descarta la mitad del espacio.
- Versiones **iterativa** y **recursiva**.
- **`lower_bound`** y **`upper_bound`**: encontrar la primera/última
  posición de un valor, o el punto de inserción.
- El **bug clásico** de overflow en `(lo + hi) / 2`.
- `bsearch` de la stdlib de C.

---

## El algoritmo

### Precondición: array ordenado

La búsqueda binaria **requiere** que el array esté ordenado. Si no
lo está, el resultado es indefinido — puede retornar un falso
negativo (no encontrar un elemento que sí existe) o un falso
positivo (retornar un índice cuyo elemento no coincide con el
buscado).

### Idea

Comparar el elemento buscado con el elemento del medio:

1. Si coincide, encontrado.
2. Si el buscado es menor, buscar en la mitad izquierda.
3. Si el buscado es mayor, buscar en la mitad derecha.

Cada paso reduce el espacio de búsqueda a la mitad.

```
BINARY_SEARCH(arr, n, target):
    lo = 0
    hi = n - 1
    mientras lo <= hi:
        mid = lo + (hi - lo) / 2
        si arr[mid] == target:
            retornar mid
        si arr[mid] < target:
            lo = mid + 1
        sino:
            hi = mid - 1
    retornar -1    // no encontrado
```

### Traza

Buscando `23` en `[3, 7, 11, 15, 23, 31, 42, 58, 71]` ($n = 9$):

```
Paso 1: lo=0, hi=8, mid=4
        arr[4]=23 == 23 → encontrado, retornar 4

Total: 1 comparación
```

Buscando `31` en el mismo array:

```
Paso 1: lo=0, hi=8, mid=4
        arr[4]=23 < 31 → lo=5

Paso 2: lo=5, hi=8, mid=6
        arr[6]=42 > 31 → hi=5

Paso 3: lo=5, hi=5, mid=5
        arr[5]=31 == 31 → encontrado, retornar 5

Total: 3 comparaciones
```

Buscando `20` (no existe):

```
Paso 1: lo=0, hi=8, mid=4
        arr[4]=23 > 20 → hi=3

Paso 2: lo=0, hi=3, mid=1
        arr[1]=7 < 20 → lo=2

Paso 3: lo=2, hi=3, mid=2
        arr[2]=11 < 20 → lo=3

Paso 4: lo=3, hi=3, mid=3
        arr[3]=15 < 20 → lo=4

Paso 5: lo=4, hi=3 → lo > hi → retornar -1

Total: 4 comparaciones
```

### Complejidad

| Caso | Comparaciones |
|------|---------------|
| Mejor | $1$ (el elemento está justo en el medio) |
| Peor | $\lfloor \log_2 n \rfloor + 1$ |
| Promedio | $\lfloor \log_2 n \rfloor$ |

Para $n = 10^6$: máximo $\lfloor \log_2(10^6) \rfloor + 1 = 20$
comparaciones. Comparado con las 500,000 comparaciones promedio de
la búsqueda secuencial, es una diferencia de 25,000x.

La razón es que cada comparación da **1 bit** de información sobre
la posición del elemento, y necesitamos $\log_2 n$ bits para
identificar una posición entre $n$ posibles.

---

## El bug clásico: overflow en el cálculo del punto medio

### El bug

La forma natural de calcular el punto medio es:

```c
int mid = (lo + hi) / 2;
```

Este código tiene un bug que tardó **20 años** en ser descubierto en
la implementación de Java (reportado en 2006 por Joshua Bloch). El
problema: si `lo + hi` excede `INT_MAX`, la suma desborda y `mid`
se convierte en un valor negativo.

Para `int` de 32 bits, esto ocurre cuando `lo + hi > 2,147,483,647`.
Con un array de $10^9$ elementos (posible en sistemas con >4 GB de
RAM), `lo` y `hi` pueden ambos ser del orden de $10^9$, y su suma
excede $2^{31} - 1$.

### La corrección

```c
int mid = lo + (hi - lo) / 2;
```

`hi - lo` nunca desborda porque `hi >= lo` (invariante del loop).
Dividir por 2 y sumar a `lo` mantiene todo dentro del rango.

Alternativa con operaciones de bits:

```c
int mid = (lo + hi) >> 1;           /* NO corrige el overflow */
int mid = ((unsigned)lo + hi) >> 1; /* correcto con unsigned */
int mid = lo + ((hi - lo) >> 1);    /* correcto */
```

El shift `>> 1` no corrige el overflow porque la suma ya desbordó.
Convertir a `unsigned` antes de sumar sí funciona porque el overflow
de unsigned está definido (wrap around), pero es menos legible.

### Impacto histórico

Este bug existió durante décadas en:

- **Java**: `java.util.Arrays.binarySearch` (corregido en JDK 6).
- **Python**: `bisect` module (corregido).
- **Muchos libros de texto**: incluyendo ediciones tempranas de CLRS
  y Sedgewick.

Jon Bentley, en "Programming Pearls" (1986), presentó la versión con
`(lo + hi) / 2` como correcta. En 2006, Bloch estimó que el 90% de
las implementaciones de búsqueda binaria en producción tenían este
bug.

---

## Versión iterativa

```c
int binary_search(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;

        if (arr[mid] == target)
            return mid;
        if (arr[mid] < target)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    return -1;
}
```

### Invariante del loop

Al inicio de cada iteración:

- Si `target` está en el array, está en `arr[lo..hi]` (inclusive).
- `lo <= hi` (el rango no está vacío).

Al terminar (`lo > hi`): el rango está vacío, el elemento no existe.

---

## Versión recursiva

```c
int binary_search_r(const int *arr, int lo, int hi, int target)
{
    if (lo > hi)
        return -1;

    int mid = lo + (hi - lo) / 2;

    if (arr[mid] == target)
        return mid;
    if (arr[mid] < target)
        return binary_search_r(arr, mid + 1, hi, target);
    return binary_search_r(arr, lo, mid - 1, target);
}
```

La versión recursiva es conceptualmente más clara pero tiene
overhead de $O(\log n)$ frames en el stack. El compilador
generalmente la convierte en iterativa con tail-call optimization
(`-O2`), pero no está garantizado.

En la práctica, la versión iterativa es preferible en C porque:

1. No depende de optimización del compilador.
2. Es igual de legible para este caso simple.
3. No puede causar stack overflow para arrays extremadamente grandes.

---

## lower_bound y upper_bound

La búsqueda binaria básica encuentra **alguna** ocurrencia del
valor. Pero frecuentemente necesitamos:

- **lower_bound**: la primera posición donde se puede insertar
  `target` sin romper el orden (primera ocurrencia, o punto de
  inserción si no existe).
- **upper_bound**: la primera posición **después** de todas las
  ocurrencias de `target`.

Estas dos funciones definen el **rango** `[lower_bound, upper_bound)`
de todas las ocurrencias de `target`.

### lower_bound

Retorna el menor índice $i$ tal que `arr[i] >= target`:

```c
int lower_bound(const int *arr, int n, int target)
{
    int lo = 0, hi = n;

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] < target)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}
```

Diferencias clave con la búsqueda binaria estándar:

1. `hi` empieza en `n` (no `n-1`), porque el resultado puede ser `n`
   (todos los elementos son menores que `target`).
2. La condición es `lo < hi` (no `lo <= hi`).
3. Cuando `arr[mid] >= target`, hacemos `hi = mid` (no `mid - 1`),
   porque `mid` podría ser la respuesta.
4. No hay retorno temprano — siempre buscamos la **primera**
   ocurrencia.

### Traza de lower_bound

`lower_bound` en `[1, 3, 3, 3, 5, 7]` buscando `3`:

```
Paso 1: lo=0, hi=6, mid=3
        arr[3]=3 >= 3 → hi=3

Paso 2: lo=0, hi=3, mid=1
        arr[1]=3 >= 3 → hi=1

Paso 3: lo=0, hi=1, mid=0
        arr[0]=1 < 3 → lo=1

lo=1, hi=1 → parar, retornar 1

arr[1]=3 es la primera ocurrencia ✓
```

`lower_bound` en el mismo array buscando `4` (no existe):

```
Paso 1: lo=0, hi=6, mid=3
        arr[3]=3 < 4 → lo=4

Paso 2: lo=4, hi=6, mid=5
        arr[5]=7 >= 4 → hi=5

Paso 3: lo=4, hi=5, mid=4
        arr[4]=5 >= 4 → hi=4

lo=4, hi=4 → parar, retornar 4

arr[4]=5 es el primer elemento >= 4 (punto de inserción) ✓
```

### upper_bound

Retorna el menor índice $i$ tal que `arr[i] > target`:

```c
int upper_bound(const int *arr, int n, int target)
{
    int lo = 0, hi = n;

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] <= target)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}
```

La única diferencia con `lower_bound` es `<=` en lugar de `<` en la
comparación. Esto hace que `upper_bound` salte **sobre** las
ocurrencias iguales a `target`.

### Traza de upper_bound

`upper_bound` en `[1, 3, 3, 3, 5, 7]` buscando `3`:

```
Paso 1: lo=0, hi=6, mid=3
        arr[3]=3 <= 3 → lo=4

Paso 2: lo=4, hi=6, mid=5
        arr[5]=7 > 3 → hi=5

Paso 3: lo=4, hi=5, mid=4
        arr[4]=5 > 3 → hi=4

lo=4, hi=4 → parar, retornar 4

arr[4]=5 es el primer elemento > 3 ✓
```

### Usar lower_bound y upper_bound juntos

```c
int lb = lower_bound(arr, n, target);  /* primera ocurrencia */
int ub = upper_bound(arr, n, target);  /* después de la última */

int count = ub - lb;  /* número de ocurrencias */

if (lb < n && arr[lb] == target)
    printf("Encontrado %d veces, posiciones [%d, %d)\n",
           count, lb, ub);
else
    printf("No encontrado, se insertaría en posición %d\n", lb);
```

Para `[1, 3, 3, 3, 5, 7]` buscando `3`:
- `lower_bound = 1` (primera ocurrencia)
- `upper_bound = 4` (después de la última)
- `count = 4 - 1 = 3` ocurrencias

### equal_range

Combinar `lower_bound` y `upper_bound` en una sola función:

```c
void equal_range(const int *arr, int n, int target, int *lb, int *ub)
{
    *lb = lower_bound(arr, n, target);
    *ub = upper_bound(arr, n, target);
}
```

En C++ esto es `std::equal_range`. En Rust, `.binary_search()` con
`partition_point` logra lo mismo (veremos en T04).

---

## bsearch de la stdlib

La stdlib de C incluye `bsearch` con el mismo patrón que `qsort`:

```c
#include <stdlib.h>

void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
```

`bsearch` retorna un puntero al elemento encontrado, o `NULL`.

### Limitaciones de bsearch

1. **No garantiza cuál ocurrencia retorna** si hay duplicados. Puede
   retornar cualquiera de las posiciones con valor igual a `key`.

2. **No ofrece lower_bound/upper_bound**. Si necesitas la primera o
   última ocurrencia, debes implementar tu propia búsqueda.

3. **Misma indirección que `qsort`**: el comparador es un puntero a
   función con `void *`. No es posible el inlining.

4. **El primer parámetro es `key`, no un elemento del array**. Si el
   tipo del array es `struct`, `key` debe apuntar a un valor con el
   layout correcto.

```c
int target = 23;
int arr[] = {3, 7, 11, 15, 23, 31, 42};

int cmp(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

int *found = bsearch(&target, arr, 7, sizeof(int), cmp);
if (found)
    printf("Encontrado en posición %td\n", found - arr);
```

---

## Errores comunes en búsqueda binaria

Knuth observó que aunque la búsqueda binaria fue publicada por
primera vez en 1946, la primera implementación correcta no apareció
hasta 1962 — 16 años de bugs. Los errores más frecuentes:

### 1. Off-by-one en los límites

```c
/* BUG: hi debería ser n-1 o las condiciones ajustadas */
hi = n;
while (lo < hi) {  /* si usas hi=n, necesitas < en vez de <= */
    ...
    hi = mid;      /* y mid en vez de mid-1 */
}
```

La búsqueda binaria tiene **dos variantes válidas** con condiciones
diferentes:

| Variante | `hi` inicial | Condición | Ajuste derecho |
|----------|-------------|-----------|----------------|
| Cerrado `[lo, hi]` | `n - 1` | `lo <= hi` | `hi = mid - 1` |
| Semiabierto `[lo, hi)` | `n` | `lo < hi` | `hi = mid` |

Mezclar elementos de ambas variantes produce bugs sutiles. La
variante semiabierta es preferible porque es más simple de razonar
y coincide con la convención de rangos de C/Rust.

### 2. Loop infinito

```c
/* BUG: si arr[mid] == target pero no es la versión con retorno temprano */
if (arr[mid] < target)
    lo = mid;      /* debería ser mid + 1 */
else
    hi = mid;
```

Si `lo = mid` y `mid = lo` (cuando `hi = lo + 1`), el loop nunca
termina. La solución: `lo = mid + 1` garantiza que `lo` siempre
avanza.

### 3. Rango vacío no detectado

```c
/* BUG: no verifica si lo > hi antes de acceder a arr[mid] */
int mid = lo + (hi - lo) / 2;
if (arr[mid] == target) ...  /* posible acceso fuera del array */
```

La condición `lo <= hi` (o `lo < hi`) debe verificarse **antes** de
calcular `mid`.

---

## Búsqueda binaria para punto de inserción

Una aplicación frecuente es encontrar dónde insertar un elemento
para mantener el array ordenado. Esto es exactamente lo que
`lower_bound` retorna:

```c
int insertion_point(const int *arr, int n, int target)
{
    return lower_bound(arr, n, target);
}
```

Si `target` ya existe, `lower_bound` retorna la posición de la
primera ocurrencia — insertar ahí mantiene la estabilidad (los
elementos existentes se desplazan a la derecha).

Si `target` no existe, retorna la posición donde debería estar.

---

## Búsqueda binaria en dominios continuos

La búsqueda binaria no se limita a arrays — funciona en cualquier
dominio monótono. Un uso clásico es encontrar raíces de funciones
monótonas:

```c
/* Encontrar sqrt(x) con precisión eps usando búsqueda binaria */
double bsearch_sqrt(double x, double eps)
{
    double lo = 0, hi = (x < 1) ? 1 : x;

    while (hi - lo > eps) {
        double mid = lo + (hi - lo) / 2;
        if (mid * mid < x)
            lo = mid;
        else
            hi = mid;
    }

    return lo + (hi - lo) / 2;
}
```

Este patrón se usa en programación competitiva para resolver
ecuaciones, optimización, y problemas de decisión binaria ("¿es
posible con presupuesto X?").

---

## Programa completo en C

```c
/* binary_search.c — Búsqueda binaria: variantes y aplicaciones */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* --- Búsqueda binaria iterativa --- */
int binary_search(const int *arr, int n, int target)
{
    int lo = 0, hi = n - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) lo = mid + 1;
        else                   hi = mid - 1;
    }

    return -1;
}

/* --- Búsqueda binaria recursiva --- */
int binary_search_r(const int *arr, int lo, int hi, int target)
{
    if (lo > hi) return -1;

    int mid = lo + (hi - lo) / 2;
    if (arr[mid] == target) return mid;
    if (arr[mid] < target)
        return binary_search_r(arr, mid + 1, hi, target);
    return binary_search_r(arr, lo, mid - 1, target);
}

/* --- lower_bound --- */
int lower_bound(const int *arr, int n, int target)
{
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] < target) lo = mid + 1;
        else                   hi = mid;
    }
    return lo;
}

/* --- upper_bound --- */
int upper_bound(const int *arr, int n, int target)
{
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] <= target) lo = mid + 1;
        else                    hi = mid;
    }
    return lo;
}

/* --- sqrt por búsqueda binaria --- */
double bsearch_sqrt(double x, double eps)
{
    double lo = 0, hi = (x < 1) ? 1 : x;
    while (hi - lo > eps) {
        double mid = lo + (hi - lo) / 2;
        if (mid * mid < x) lo = mid;
        else               hi = mid;
    }
    return lo + (hi - lo) / 2;
}

/* --- Helper --- */
void print_arr(const char *label, const int *arr, int n)
{
    printf("%s [", label);
    for (int i = 0; i < n; i++)
        printf("%s%d", i ? ", " : "", arr[i]);
    printf("]\n");
}

int main(void)
{
    /* Demo 1: Búsqueda binaria iterativa */
    printf("=== Demo 1: Búsqueda binaria iterativa ===\n");
    int arr[] = {3, 7, 11, 15, 23, 31, 42, 58, 71};
    int n = 9;
    print_arr("Array:", arr, n);

    int targets[] = {23, 31, 20, 3, 71, 100};
    for (int i = 0; i < 6; i++) {
        int idx = binary_search(arr, n, targets[i]);
        printf("Buscar %3d: %s",
               targets[i], idx >= 0 ? "encontrado" : "no encontrado");
        if (idx >= 0) printf(" en posición %d", idx);
        printf("\n");
    }

    /* Demo 2: Recursiva */
    printf("\n=== Demo 2: Búsqueda binaria recursiva ===\n");
    int idx = binary_search_r(arr, 0, n - 1, 42);
    printf("Buscar 42 (recursiva): posición %d\n", idx);

    /* Demo 3: lower_bound y upper_bound */
    printf("\n=== Demo 3: lower_bound y upper_bound ===\n");
    int dup[] = {1, 3, 3, 3, 5, 5, 7, 9};
    int nd = 8;
    print_arr("Array:", dup, nd);

    int search_vals[] = {3, 5, 4, 0, 10};
    for (int i = 0; i < 5; i++) {
        int lb = lower_bound(dup, nd, search_vals[i]);
        int ub = upper_bound(dup, nd, search_vals[i]);
        printf("target=%d: lower_bound=%d, upper_bound=%d, count=%d",
               search_vals[i], lb, ub, ub - lb);
        if (lb < nd && dup[lb] == search_vals[i])
            printf("  → encontrado en [%d, %d)", lb, ub);
        else
            printf("  → no encontrado, insertar en %d", lb);
        printf("\n");
    }

    /* Demo 4: Punto de inserción */
    printf("\n=== Demo 4: Inserción manteniendo orden ===\n");
    int sorted[] = {10, 20, 30, 40, 50};
    int ns = 5;
    print_arr("Array:", sorted, ns);

    int inserts[] = {25, 5, 55, 30};
    for (int i = 0; i < 4; i++) {
        int pos = lower_bound(sorted, ns, inserts[i]);
        printf("Insertar %d → posición %d\n", inserts[i], pos);
    }

    /* Demo 5: sqrt por búsqueda binaria */
    printf("\n=== Demo 5: sqrt por búsqueda binaria ===\n");
    double test_vals[] = {2.0, 9.0, 0.25, 100.0};
    for (int i = 0; i < 4; i++) {
        double result = bsearch_sqrt(test_vals[i], 1e-10);
        printf("sqrt(%.2f) = %.10f  (real: %.10f, error: %.2e)\n",
               test_vals[i], result, sqrt(test_vals[i]),
               fabs(result - sqrt(test_vals[i])));
    }

    /* Demo 6: Benchmark secuencial vs binaria */
    printf("\n=== Demo 6: Benchmark (10^6 elementos) ===\n");
    int N = 1000000;
    int *big = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) big[i] = i * 2;

    int queries[] = {0, 500000, 999998, 1999998, 1};
    int nq = 5;

    int reps = 1000000;
    volatile int dummy;

    clock_t t0 = clock();
    for (int r = 0; r < reps; r++)
        for (int q = 0; q < nq; q++)
            dummy = binary_search(big, N, queries[q]);
    clock_t t1 = clock();
    double bs_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    /* Búsqueda secuencial para comparar (menos repeticiones) */
    int seq_reps = 100;
    t0 = clock();
    for (int r = 0; r < seq_reps; r++) {
        for (int q = 0; q < nq; q++) {
            for (int i = 0; i < N; i++) {
                if (big[i] == queries[q]) { dummy = i; break; }
                if (big[i] > queries[q])  { dummy = -1; break; }
            }
        }
    }
    t1 = clock();
    double seq_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    printf("Binaria:    %7.2f ms (%d×%d búsquedas)\n",
           bs_ms, reps, nq);
    printf("Secuencial: %7.2f ms (%d×%d búsquedas)\n",
           seq_ms, seq_reps, nq);
    printf("Binaria por búsqueda:    %.4f µs\n",
           bs_ms / (reps * nq) * 1000);
    printf("Secuencial por búsqueda: %.1f µs\n",
           seq_ms / (seq_reps * nq) * 1000);

    free(big);
    (void)dummy;

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -std=c11 -Wall -Wextra -o binary_search binary_search.c -lm
./binary_search
```

### Salida esperada

```
=== Demo 1: Búsqueda binaria iterativa ===
Array: [3, 7, 11, 15, 23, 31, 42, 58, 71]
Buscar  23: encontrado en posición 4
Buscar  31: encontrado en posición 5
Buscar  20: no encontrado
Buscar   3: encontrado en posición 0
Buscar  71: encontrado en posición 8
Buscar 100: no encontrado

=== Demo 2: Búsqueda binaria recursiva ===
Buscar 42 (recursiva): posición 6

=== Demo 3: lower_bound y upper_bound ===
Array: [1, 3, 3, 3, 5, 5, 7, 9]
target=3: lower_bound=1, upper_bound=4, count=3  → encontrado en [1, 4)
target=5: lower_bound=4, upper_bound=6, count=2  → encontrado en [4, 6)
target=4: lower_bound=4, upper_bound=4, count=0  → no encontrado, insertar en 4
target=0: lower_bound=0, upper_bound=0, count=0  → no encontrado, insertar en 0
target=10: lower_bound=8, upper_bound=8, count=0  → no encontrado, insertar en 8

=== Demo 4: Inserción manteniendo orden ===
Array: [10, 20, 30, 40, 50]
Insertar 25 → posición 2
Insertar 5 → posición 0
Insertar 55 → posición 5
Insertar 30 → posición 2

=== Demo 5: sqrt por búsqueda binaria ===
sqrt(2.00) = 1.4142135624  (real: 1.4142135624, error: 7.85e-13)
sqrt(9.00) = 9.0000000000  (real: 3.0000000000, error: ...)
sqrt(0.25) = 0.5000000000  (real: 0.5000000000, error: ...)
sqrt(100.00) = 10.0000000000 (real: 10.0000000000, error: ...)

=== Demo 6: Benchmark (10^6 elementos) ===
Binaria:    ~120.00 ms (1000000×5 búsquedas)
Secuencial: ~450.00 ms (100×5 búsquedas)
Binaria por búsqueda:    ~0.024 µs
Secuencial por búsqueda: ~900.0 µs
```

---

## Programa completo en Rust

```rust
// binary_search.rs — Búsqueda binaria: variantes y aplicaciones
use std::time::Instant;

fn binary_search(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo: usize = 0;
    let mut hi: usize = arr.len();

    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal   => return Some(mid),
            std::cmp::Ordering::Less    => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }

    None
}

fn binary_search_recursive(arr: &[i32], target: i32) -> Option<usize> {
    fn helper(arr: &[i32], lo: usize, hi: usize, target: i32) -> Option<usize> {
        if lo >= hi { return None; }
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal   => Some(mid),
            std::cmp::Ordering::Less    => helper(arr, mid + 1, hi, target),
            std::cmp::Ordering::Greater => helper(arr, lo, mid, target),
        }
    }
    helper(arr, 0, arr.len(), target)
}

fn lower_bound(arr: &[i32], target: i32) -> usize {
    let (mut lo, mut hi) = (0, arr.len());
    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        if arr[mid] < target { lo = mid + 1; }
        else                 { hi = mid; }
    }
    lo
}

fn upper_bound(arr: &[i32], target: i32) -> usize {
    let (mut lo, mut hi) = (0, arr.len());
    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        if arr[mid] <= target { lo = mid + 1; }
        else                  { hi = mid; }
    }
    lo
}

fn bsearch_sqrt(x: f64, eps: f64) -> f64 {
    let (mut lo, mut hi) = (0.0, if x < 1.0 { 1.0 } else { x });
    while hi - lo > eps {
        let mid = lo + (hi - lo) / 2.0;
        if mid * mid < x { lo = mid; }
        else             { hi = mid; }
    }
    lo + (hi - lo) / 2.0
}

fn main() {
    // Demo 1: Búsqueda binaria iterativa
    println!("=== Demo 1: Búsqueda binaria iterativa ===");
    let arr = [3, 7, 11, 15, 23, 31, 42, 58, 71];
    println!("Array: {:?}", arr);

    for &t in &[23, 31, 20, 3, 71, 100] {
        match binary_search(&arr, t) {
            Some(i) => println!("Buscar {:>3}: encontrado en posición {}", t, i),
            None    => println!("Buscar {:>3}: no encontrado", t),
        }
    }

    // Demo 2: Recursiva
    println!("\n=== Demo 2: Búsqueda recursiva ===");
    println!("Buscar 42: {:?}", binary_search_recursive(&arr, 42));

    // Demo 3: lower_bound y upper_bound
    println!("\n=== Demo 3: lower_bound y upper_bound ===");
    let dup = [1, 3, 3, 3, 5, 5, 7, 9];
    println!("Array: {:?}", dup);

    for &t in &[3, 5, 4, 0, 10] {
        let lb = lower_bound(&dup, t);
        let ub = upper_bound(&dup, t);
        print!("target={}: lb={}, ub={}, count={}",
               t, lb, ub, ub - lb);
        if lb < dup.len() && dup[lb] == t {
            println!("  → encontrado en [{}, {})", lb, ub);
        } else {
            println!("  → no encontrado, insertar en {}", lb);
        }
    }

    // Demo 4: stdlib binary_search (retorna Result<usize, usize>)
    println!("\n=== Demo 4: stdlib binary_search ===");
    let sorted = [10, 20, 30, 40, 50];
    println!("Array: {:?}", sorted);

    for &t in &[30, 25, 50, 5] {
        match sorted.binary_search(&t) {
            Ok(i)  => println!("Buscar {}: Ok({}) — encontrado", t, i),
            Err(i) => println!("Buscar {}: Err({}) — insertar en posición {}", t, i, i),
        }
    }

    // Demo 5: partition_point (equivalente a lower_bound)
    println!("\n=== Demo 5: partition_point ===");
    let data = [1, 3, 3, 3, 5, 5, 7, 9];
    println!("Array: {:?}", data);

    let lb = data.partition_point(|&x| x < 3);
    let ub = data.partition_point(|&x| x <= 3);
    println!("lower_bound(3) = {} (partition_point |x| x < 3)", lb);
    println!("upper_bound(3) = {} (partition_point |x| x <= 3)", ub);
    println!("Rango de 3s: [{}..{}), count={}", lb, ub, ub - lb);

    // Demo 6: sqrt por búsqueda binaria
    println!("\n=== Demo 6: sqrt por búsqueda binaria ===");
    for &x in &[2.0, 9.0, 0.25, 100.0] {
        let result = bsearch_sqrt(x, 1e-10);
        let real = x.sqrt();
        println!("sqrt({:.2}) = {:.10}  (real: {:.10}, error: {:.2e})",
                 x, result, real, (result - real).abs());
    }

    // Demo 7: Benchmark
    println!("\n=== Demo 7: Benchmark (10^6 elementos) ===");
    let n = 1_000_000usize;
    let big: Vec<i32> = (0..n as i32).map(|i| i * 2).collect();
    let queries = [0, 500_000, 999_998, 1_999_998, 1];
    let reps = 1_000_000;

    let start = Instant::now();
    let mut dummy = 0usize;
    for _ in 0..reps {
        for &q in &queries {
            dummy += binary_search(&big, q).unwrap_or(0);
        }
    }
    let bs_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("Binaria: {:.2} ms ({}×{} búsquedas)", bs_ms, reps, queries.len());
    println!("Por búsqueda: {:.4} µs", bs_ms / (reps * queries.len()) as f64 * 1000.0);
    println!("(dummy={})", dummy);  // evitar optimización
}
```

### Compilar y ejecutar

```bash
rustc -O -o binary_search binary_search.rs
./binary_search
```

---

## Ejercicios

### Ejercicio 1 — Traza completa

Traza `binary_search` paso a paso para buscar `15` en:

```c
int arr[] = {2, 5, 8, 12, 15, 18, 23, 31};
```

¿Cuántas comparaciones se realizan?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
n=8, target=15

Paso 1: lo=0, hi=7, mid=0+(7-0)/2=3
        arr[3]=12
        12 == 15? No
        12 < 15? Sí → lo=4

Paso 2: lo=4, hi=7, mid=4+(7-4)/2=5
        arr[5]=18
        18 == 15? No
        18 < 15? No → hi=4

Paso 3: lo=4, hi=4, mid=4+(4-4)/2=4
        arr[4]=15
        15 == 15? Sí → retornar 4
```

3 comparaciones de igualdad + 2 comparaciones de `<` = 5
comparaciones totales (o 3 pasos si contamos cada paso como una
unidad).

La profundidad máxima para $n = 8$ es
$\lfloor \log_2 8 \rfloor + 1 = 4$, así que 3 pasos es un caso
promedio.

</details>

### Ejercicio 2 — El bug de overflow

¿Para qué valor de `n` el bug `(lo + hi) / 2` se manifiesta con
`int` de 32 bits? Calcula el tamaño mínimo del array.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

El overflow ocurre cuando `lo + hi > INT_MAX = 2,147,483,647`.

En el peor caso, `lo` y `hi` están ambos cerca de `n - 1`. El
máximo de `lo + hi` es `(n-2) + (n-1) = 2n - 3`.

Para que desborde: $2n - 3 > 2{,}147{,}483{,}647$

$$n > \frac{2{,}147{,}483{,}650}{2} = 1{,}073{,}741{,}825$$

Esto es ~$10^9$ elementos, o ~4 GB para un array de `int` (4 bytes
cada uno). Es un tamaño alcanzable en servidores modernos con
64+ GB de RAM.

Con `size_t` (unsigned 64-bit), el problema desaparece para arrays
prácticos:

```c
size_t mid = lo + (hi - lo) / 2;  /* seguro hasta 2^64 */
```

En Rust, `usize` es 64 bits en plataformas de 64 bits, y el overflow
de enteros genera panic en debug — el bug se detectaría inmediatamente.

</details>

### Ejercicio 3 — lower_bound para elemento no existente

¿Qué retorna `lower_bound` para `target = 6` en
`[1, 3, 5, 7, 9]`? Traza paso a paso.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
n=5, target=6

Paso 1: lo=0, hi=5, mid=2
        arr[2]=5 < 6 → lo=3

Paso 2: lo=3, hi=5, mid=4
        arr[4]=9 >= 6 → hi=4

Paso 3: lo=3, hi=4, mid=3
        arr[3]=7 >= 6 → hi=3

lo=3, hi=3 → parar, retornar 3
```

`lower_bound` retorna **3**. `arr[3] = 7` es el primer elemento
$\geq 6$. Si quisiéramos insertar 6 manteniendo el orden, lo
pondríamos en posición 3:

```
[1, 3, 5, 6, 7, 9]
```

Nótese que el valor 6 no existe en el array, pero `lower_bound`
retorna una posición válida que indica **dónde debería estar**.
Verificar: `arr[3] != 6`, así que sabemos que no existe.

</details>

### Ejercicio 4 — equal_range

Implementa `equal_range` y úsala para contar ocurrencias de cada
valor en `[1, 2, 2, 3, 3, 3, 4, 4, 4, 4]`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
void equal_range(const int *arr, int n, int target, int *lb, int *ub)
{
    *lb = lower_bound(arr, n, target);
    *ub = upper_bound(arr, n, target);
}
```

Aplicación:

```c
int arr[] = {1, 2, 2, 3, 3, 3, 4, 4, 4, 4};
int n = 10;

for (int v = 1; v <= 4; v++) {
    int lb, ub;
    equal_range(arr, n, v, &lb, &ub);
    printf("%d aparece %d veces en [%d, %d)\n", v, ub - lb, lb, ub);
}
```

Resultado:

```
1 aparece 1 veces en [0, 1)
2 aparece 2 veces en [1, 3)
3 aparece 3 veces en [3, 6)
4 aparece 4 veces en [6, 10)
```

Cada llamada a `equal_range` hace $2 \times O(\log n)$ comparaciones.
Para contar ocurrencias de $k$ valores distintos, el costo total es
$O(k \log n)$.

</details>

### Ejercicio 5 — Diferencia entre `<` y `<=` en lower/upper_bound

¿Qué pasa si cambiamos `<` por `<=` en `lower_bound`? ¿Se convierte
en `upper_bound`? Verifica con un ejemplo.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Sí**, exactamente. La única diferencia entre `lower_bound` y
`upper_bound` es:

```c
// lower_bound: primer i tal que arr[i] >= target
if (arr[mid] < target)   lo = mid + 1;  // < : salta estrictamente menores
else                     hi = mid;

// upper_bound: primer i tal que arr[i] > target
if (arr[mid] <= target)  lo = mid + 1;  // <= : salta menores E iguales
else                     hi = mid;
```

Con `<`: elementos iguales a `target` van al lado derecho
(`hi = mid`) → se busca el **primero** igual.

Con `<=`: elementos iguales a `target` van al lado izquierdo
(`lo = mid + 1`) → se salta **sobre** todos los iguales → se
encuentra el primero **mayor**.

Para `[1, 3, 3, 3, 5]` buscando `3`:

- `lower_bound` (con `<`): retorna 1 (primer 3).
- `upper_bound` (con `<=`): retorna 4 (primer elemento > 3, que
  es arr[4]=5).

</details>

### Ejercicio 6 — Búsqueda binaria genérica en C

Escribe una versión genérica de `lower_bound` usando `void *`,
compatible con `qsort`:

```c
void *generic_lower_bound(const void *base, size_t n, size_t size,
                           const void *target,
                           int (*cmp)(const void *, const void *));
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
void *generic_lower_bound(const void *base, size_t n, size_t size,
                           const void *target,
                           int (*cmp)(const void *, const void *))
{
    const char *arr = (const char *)base;
    size_t lo = 0, hi = n;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cmp(arr + mid * size, target) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    return (void *)(arr + lo * size);
}
```

Uso:

```c
int arr[] = {1, 3, 3, 3, 5, 7};
int target = 3;
int *lb = generic_lower_bound(arr, 6, sizeof(int), &target, cmp_int);

printf("lower_bound(3) → posición %td, valor %d\n",
       lb - arr, *lb);
// lower_bound(3) → posición 1, valor 3
```

Cuando `lo == n` (todos los elementos son menores que `target`),
retorna un puntero al "one past end" — análogo a `arr + n`. El
llamador debe verificar `lb < arr + n` antes de desreferenciar.

</details>

### Ejercicio 7 — Primer elemento mayor que X

Usando búsqueda binaria, encuentra el primer elemento **estrictamente
mayor** que `target` en un array ordenado. ¿Es `lower_bound` o
`upper_bound`?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Es **`upper_bound`**. Por definición:

- `lower_bound(target)`: primer $i$ tal que `arr[i] >= target`.
- `upper_bound(target)`: primer $i$ tal que `arr[i] > target`.

"Primer elemento estrictamente mayor que `target`" es exactamente la
definición de `upper_bound`.

```c
int arr[] = {1, 3, 3, 5, 5, 7};
int ub = upper_bound(arr, 6, 3);
// ub = 3, arr[3] = 5 → primer elemento > 3 ✓

int ub2 = upper_bound(arr, 6, 4);
// ub2 = 3, arr[3] = 5 → primer elemento > 4 ✓
// (para 4, lower_bound también retornaría 3)
```

Relación útil:

- "Primer elemento `>= x`" → `lower_bound(x)`
- "Primer elemento `> x`" → `upper_bound(x)`
- "Último elemento `<= x`" → `upper_bound(x) - 1`
- "Último elemento `< x`" → `lower_bound(x) - 1`

</details>

### Ejercicio 8 — Búsqueda binaria en dominio continuo

Usa búsqueda binaria para encontrar la raíz de
$f(x) = x^3 - 2x - 5$ en el intervalo $[2, 3]$.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
double f(double x) { return x*x*x - 2*x - 5; }

double find_root(double lo, double hi, double eps)
{
    while (hi - lo > eps) {
        double mid = lo + (hi - lo) / 2;
        if (f(mid) < 0) lo = mid;
        else            hi = mid;
    }
    return lo + (hi - lo) / 2;
}

double root = find_root(2.0, 3.0, 1e-10);
// root ≈ 2.0945514815
```

Verificación: $f(2) = 8 - 4 - 5 = -1 < 0$ y
$f(3) = 27 - 6 - 5 = 16 > 0$, así que hay una raíz en $[2, 3]$
por el teorema del valor intermedio.

La búsqueda binaria converge en $\lceil \log_2((3-2)/10^{-10}) \rceil = 34$
iteraciones — mucho más rápido que métodos iterativos que necesitan
derivadas (Newton-Raphson).

Requisitos para que funcione:

1. $f$ debe ser **continua** en $[lo, hi]$.
2. $f(lo)$ y $f(hi)$ deben tener **signos opuestos** (o se conoce la
   monotonía).

</details>

### Ejercicio 9 — Resultado de binary_search en Rust

¿Qué retorna `[1, 3, 3, 5].binary_search(&3)` en Rust? ¿Siempre
retorna la misma posición?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let arr = [1, 3, 3, 5];
let result = arr.binary_search(&3);
```

Retorna `Ok(i)` donde `i` es **algún** índice con valor 3. Puede
ser `Ok(1)` o `Ok(2)` — la documentación dice explícitamente:

> "If there are multiple matches, then any one of the matches could
> be returned."

Para encontrar la **primera** ocurrencia, usar `partition_point`:

```rust
let first = arr.partition_point(|&x| x < 3);
// first = 1 (siempre la primera ocurrencia)

let last_exclusive = arr.partition_point(|&x| x <= 3);
// last_exclusive = 3 (después de la última ocurrencia)
```

`partition_point` es el equivalente Rust de `lower_bound`:
retorna el primer índice donde el predicado es `false`. Con
`|x| x < target` retorna `lower_bound`, con `|x| x <= target`
retorna `upper_bound`.

Si el valor no existe, `binary_search` retorna `Err(i)` donde `i`
es el punto de inserción — exactamente `lower_bound`.

</details>

### Ejercicio 10 — Off-by-one

Esta implementación tiene un bug. Encuéntralo y explica para qué
input falla:

```c
int buggy_bsearch(const int *arr, int n, int target)
{
    int lo = 0, hi = n;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target)  lo = mid + 1;
        else                    hi = mid - 1;
    }
    return -1;
}
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

El bug: **`hi` empieza en `n` pero se usa con `lo <= hi` y
`arr[mid]`**.

Cuando `lo = hi = n`, se calcula `mid = n` y se accede a `arr[n]`
— un **acceso fuera del array** (buffer over-read). El array tiene
índices válidos `0` a `n-1`.

Ejemplo: buscar `99` en `[1, 2, 3]` ($n = 3$):

```
Paso 1: lo=0, hi=3, mid=1, arr[1]=2 < 99 → lo=2
Paso 2: lo=2, hi=3, mid=2, arr[2]=3 < 99 → lo=3
Paso 3: lo=3, hi=3, mid=3, arr[3]=??? ← fuera del array!
```

Hay dos correcciones posibles:

1. Usar `hi = n - 1` (variante cerrada `[lo, hi]`):
   ```c
   int hi = n - 1;  // cambiado
   while (lo <= hi) { ... hi = mid - 1; }
   ```

2. Usar `lo < hi` (variante semiabierta `[lo, hi)`):
   ```c
   int hi = n;
   while (lo < hi) { ... hi = mid; }  // cambiados ambos
   ```

El código mezcla `hi = n` (semiabierto) con `lo <= hi` y
`hi = mid - 1` (cerrado) — una combinación inválida.

</details>

---

## Resumen

| Aspecto | Detalle |
|---------|---------|
| Complejidad | $O(\log n)$ — máximo $\lfloor \log_2 n \rfloor + 1$ comparaciones |
| Requisito | Array **ordenado** |
| Bug clásico | `(lo + hi) / 2` desborda — usar `lo + (hi - lo) / 2` |
| Variantes | Cerrada `[lo, hi]` con `<=` y `mid ± 1`, semiabierta `[lo, hi)` con `<` y `hi = mid` |
| `lower_bound` | Primer $i$ con `arr[i] >= target` — punto de inserción estable |
| `upper_bound` | Primer $i$ con `arr[i] > target` — después de la última ocurrencia |
| `bsearch` (C) | Genérica con `void *`, no garantiza cuál duplicado, no ofrece bounds |
| Dominio continuo | Funciona para raíces de funciones monótonas con precisión arbitraria |
