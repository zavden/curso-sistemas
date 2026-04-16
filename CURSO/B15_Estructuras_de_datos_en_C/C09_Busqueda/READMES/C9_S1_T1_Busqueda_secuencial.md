# Búsqueda secuencial

## Objetivo

Dominar la búsqueda secuencial (lineal) como el algoritmo de búsqueda
más fundamental, incluyendo sus optimizaciones clásicas:

- Algoritmo básico: recorrer de inicio a fin, $O(n)$.
- **Sentinel**: eliminar la comparación de límite del loop.
- **Move-to-front**: mover el elemento encontrado al inicio.
- **Transposition**: intercambiar con el anterior.
- Análisis de cuándo la búsqueda secuencial es la mejor opción.

---

## El algoritmo básico

La búsqueda secuencial examina cada elemento del array, uno por uno,
hasta encontrar el buscado o agotar el array:

```
LINEAR_SEARCH(arr, n, target):
    para i de 0 a n-1:
        si arr[i] == target:
            retornar i
    retornar -1          // no encontrado
```

Cada iteración del loop realiza **dos comparaciones**:

1. `i < n` — ¿estamos dentro del array?
2. `arr[i] == target` — ¿es el elemento buscado?

La primera comparación es overhead puro — no contribuye a la búsqueda.
La optimización con sentinel elimina esta comparación.

### Complejidad

| Caso | Comparaciones | Cuándo |
|------|---------------|--------|
| Mejor | $1$ | El elemento está en la primera posición |
| Peor | $n$ | No está, o está en la última posición |
| Promedio (éxito) | $\frac{n+1}{2}$ | Distribución uniforme de la posición |
| Promedio (fracaso) | $n$ | Siempre recorre todo |

El caso promedio con búsqueda exitosa se calcula como:

$$\frac{1}{n} \sum_{i=1}^{n} i = \frac{1}{n} \cdot \frac{n(n+1)}{2} = \frac{n+1}{2}$$

Asumiendo que cada posición es igualmente probable.

### Implementación en C

```c
int linear_search(const int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target)
            return i;
    }
    return -1;
}
```

---

## Optimización 1: Sentinel (centinela)

La idea es colocar el valor buscado al final del array, garantizando
que siempre será encontrado. Esto elimina la comprobación de límites
del loop:

```
SENTINEL_SEARCH(arr, n, target):
    last = arr[n-1]          // guardar último elemento
    arr[n-1] = target        // colocar centinela

    i = 0
    mientras arr[i] != target:
        i++

    arr[n-1] = last          // restaurar último elemento

    si i < n-1:
        retornar i           // encontrado antes del centinela
    si last == target:
        retornar n-1         // el último elemento era el buscado
    retornar -1              // no encontrado
```

### Por qué es más rápido

El loop interno se reduce a:

```c
while (arr[i] != target)
    i++;
```

Una sola comparación por iteración en lugar de dos. Esto elimina
~50% de las instrucciones del loop. En un procesador moderno, el
loop con sentinel puede ser ~20-30% más rápido para búsquedas
fallidas en arrays grandes.

### Traza

Buscando `7` en `[3, 8, 1, 7, 5]`:

```
Estado inicial:  [3, 8, 1, 7, 5]
Guardar last=5, poner centinela: [3, 8, 1, 7, 7]

i=0: arr[0]=3 != 7 → i++
i=1: arr[1]=8 != 7 → i++
i=2: arr[2]=1 != 7 → i++
i=3: arr[3]=7 == 7 → parar

i=3 < n-1=4 → retornar 3 ✓
Restaurar: [3, 8, 1, 7, 5]
```

Buscando `9` en `[3, 8, 1, 7, 5]`:

```
Guardar last=5, poner centinela: [3, 8, 1, 7, 9]

i=0: arr[0]=3 != 9 → i++
i=1: arr[1]=8 != 9 → i++
i=2: arr[2]=1 != 9 → i++
i=3: arr[3]=7 != 9 → i++
i=4: arr[4]=9 == 9 → parar

i=4 == n-1=4, y last=5 != 9 → retornar -1 ✓
Restaurar: [3, 8, 1, 7, 5]
```

### Limitaciones del sentinel

1. **Requiere array modificable**: no funciona con `const` arrays.
2. **Necesita espacio extra o acceso al array**: si el array está
   lleno hasta su capacidad, no hay dónde poner el sentinel sin
   riesgo de buffer overflow.
3. **No es thread-safe**: modificar el array durante la búsqueda
   puede causar data races si otro thread lo lee.
4. **Mejora marginal en CPUs modernas**: los branch predictors
   modernos predicen el `i < n` con alta precisión (~99%), lo que
   reduce la ventaja del sentinel.

### Implementación en C

```c
int sentinel_search(int *arr, int n, int target)
{
    if (n == 0) return -1;

    int last = arr[n - 1];
    arr[n - 1] = target;

    int i = 0;
    while (arr[i] != target)
        i++;

    arr[n - 1] = last;

    if (i < n - 1)
        return i;
    if (last == target)
        return n - 1;
    return -1;
}
```

---

## Optimización 2: Move-to-front

Si un elemento se busca y se encuentra, moverlo al **inicio** del
array. Esto optimiza búsquedas futuras del mismo elemento: la
próxima vez, se encontrará en $O(1)$.

```
MTF_SEARCH(arr, n, target):
    para i de 0 a n-1:
        si arr[i] == target:
            // Mover al frente
            val = arr[i]
            mover arr[0..i-1] una posición a la derecha
            arr[0] = val
            retornar 0       // nueva posición
    retornar -1
```

### Traza

Buscando `7` en `[3, 8, 1, 7, 5]`:

```
Encontrado en i=3: arr[3]=7
Mover al frente: [7, 3, 8, 1, 5]
Retornar 0

Si buscamos 7 otra vez: arr[0]=7 → encontrado en 1 comparación
```

### Análisis

Move-to-front es óptimo cuando las búsquedas siguen la **regla 80/20**
(o cualquier distribución no uniforme): unos pocos elementos se buscan
con mucha más frecuencia que el resto. Estos elementos migran al
inicio y se encuentran rápidamente.

Para una distribución de Zipf (donde el i-ésimo elemento más popular
se busca con frecuencia proporcional a $1/i$), move-to-front logra
un tiempo promedio de $O(\log n)$ — significativamente mejor que el
$O(n/2)$ de búsqueda lineal sin optimización.

Desventajas:

- **Costo del shift**: mover todos los elementos una posición es
  $O(i)$. Si se usa una lista enlazada en lugar de un array, el
  move-to-front es $O(1)$.
- **Una sola búsqueda de un elemento raro desplaza a los populares**:
  el elemento raro va al frente y empuja a los frecuentes.
- **Modifica el array**: no apto para datos inmutables.

### Implementación en C

```c
int mtf_search(int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            int val = arr[i];
            /* Shift derecha: arr[0..i-1] → arr[1..i] */
            memmove(&arr[1], &arr[0], i * sizeof(int));
            arr[0] = val;
            return 0;
        }
    }
    return -1;
}
```

---

## Optimización 3: Transposition

Similar a move-to-front, pero más conservadora: al encontrar un
elemento, intercambiarlo con su **predecesor inmediato**. Con búsquedas
repetidas, los elementos frecuentes migran gradualmente al inicio.

```
TRANSPOSE_SEARCH(arr, n, target):
    para i de 0 a n-1:
        si arr[i] == target:
            si i > 0:
                swap(arr[i], arr[i-1])
                retornar i-1     // nueva posición
            retornar 0           // ya estaba al inicio
    retornar -1
```

### Traza

Buscando `7` en `[3, 8, 1, 7, 5]`:

```
Encontrado en i=3: arr[3]=7
Swap con arr[2]=1: [3, 8, 7, 1, 5]
Retornar 2

Buscar 7 otra vez en [3, 8, 7, 1, 5]:
Encontrado en i=2: arr[2]=7
Swap con arr[1]=8: [3, 7, 8, 1, 5]
Retornar 1

Buscar 7 otra vez en [3, 7, 8, 1, 5]:
Encontrado en i=1: arr[1]=7
Swap con arr[0]=3: [7, 3, 8, 1, 5]
Retornar 0

Después de 3 búsquedas, 7 está al frente.
```

### Move-to-front vs Transposition

| Aspecto | Move-to-front | Transposition |
|---------|---------------|---------------|
| Velocidad de adaptación | Rápida (1 búsqueda) | Lenta (gradual) |
| Costo por búsqueda exitosa | $O(i)$ shift | $O(1)$ swap |
| Resistencia a anomalías | Baja (un raro desplaza) | Alta (migración suave) |
| Convergencia | Rápida al óptimo | Lenta pero más estable |
| Mejor para | Distribuciones muy sesgadas | Distribuciones que cambian |

Transposition es preferible cuando la distribución de frecuencias
**cambia con el tiempo**: los elementos se reordenan suavemente sin
que una sola búsqueda atípica destruya el orden acumulado.

### Implementación en C

```c
int transpose_search(int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            if (i > 0) {
                int tmp = arr[i];
                arr[i] = arr[i - 1];
                arr[i - 1] = tmp;
                return i - 1;
            }
            return 0;
        }
    }
    return -1;
}
```

---

## Búsqueda secuencial en array ordenado

Si el array está ordenado, la búsqueda secuencial puede terminar
**antes** cuando encuentra un elemento mayor que el buscado:

```c
int sorted_linear_search(const int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) return i;
        if (arr[i] > target)  return -1;  /* early exit */
    }
    return -1;
}
```

Esto mejora el caso de búsqueda fallida de $n$ comparaciones a
$\frac{n+1}{2}$ en promedio (para distribución uniforme del target).
Sin embargo, no mejora la complejidad asintótica — sigue siendo
$O(n)$.

Si el array está ordenado, es casi siempre mejor usar búsqueda
binaria ($O(\log n)$). La búsqueda secuencial en array ordenado solo
tiene sentido cuando $n$ es muy pequeño (< ~16 elementos), donde la
simplicidad del loop compensa la menor complejidad teórica de la
binaria.

---

## Búsqueda secuencial genérica

Para buscar en arrays de cualquier tipo, el mismo patrón de `void *`
que vimos en `qsort`:

```c
void *linear_search_generic(const void *base, size_t n, size_t size,
                             const void *target,
                             int (*cmp)(const void *, const void *))
{
    const char *arr = (const char *)base;

    for (size_t i = 0; i < n; i++) {
        const void *elem = arr + i * size;
        if (cmp(elem, target) == 0)
            return (void *)elem;
    }
    return NULL;
}
```

Retorna un puntero al elemento encontrado o `NULL`. Este patrón es
idéntico al de `bsearch` de la stdlib, pero sin requerir que el
array esté ordenado.

---

## Cuándo usar búsqueda secuencial

La búsqueda secuencial es la opción correcta cuando:

1. **El array no está ordenado** y no se puede ordenar (datos
   inmutables, costo de ordenar > costo de buscar, elementos sin
   orden natural).

2. **El array es pequeño** ($n < 16$). Para arrays pequeños, la
   búsqueda secuencial es más rápida que la binaria porque no tiene
   overhead de cálculo de índices. Los procesadores modernos pueden
   cargar 16 enteros en un par de líneas de cache y compararlos
   todos en ~5 ns.

3. **Se busca una sola vez**. Si el array solo se consulta una vez,
   el costo de ordenar ($O(n \log n)$) domina sobre el ahorro de la
   búsqueda binaria ($O(\log n)$ vs $O(n)$).

4. **Los datos son una lista enlazada**. Las listas no permiten
   acceso aleatorio, así que la búsqueda binaria no es aplicable.
   La secuencial es la única opción.

5. **Se busca por un predicado, no por igualdad**. "Encontrar el
   primer elemento > 100" requiere examinar elementos hasta encontrar
   uno que satisfaga la condición — inherentemente secuencial si no
   hay estructura adicional.

### Cuándo NO usar búsqueda secuencial

- Múltiples búsquedas en el mismo array → ordenar + búsqueda binaria.
- Array grande con búsquedas frecuentes → tabla hash ($O(1)$) o
  árbol balanceado ($O(\log n)$).
- El array está ordenado → búsqueda binaria siempre.

El punto de cruce: si se realizan más de $\frac{n}{2\log_2 n}$
búsquedas, conviene ordenar primero. Para $n = 1000$, esto es
~50 búsquedas. Para $n = 10^6$, es ~25000 búsquedas.

---

## Programa completo en C

```c
/* linear_search.c — Variantes de búsqueda secuencial */
#include <stdio.h>
#include <string.h>
#include <time.h>

/* --- Búsqueda básica --- */
int linear_search(const int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target)
            return i;
    }
    return -1;
}

/* --- Sentinel --- */
int sentinel_search(int *arr, int n, int target)
{
    if (n == 0) return -1;

    int last = arr[n - 1];
    arr[n - 1] = target;

    int i = 0;
    while (arr[i] != target)
        i++;

    arr[n - 1] = last;

    if (i < n - 1) return i;
    if (last == target) return n - 1;
    return -1;
}

/* --- Move-to-front --- */
int mtf_search(int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            int val = arr[i];
            memmove(&arr[1], &arr[0], i * sizeof(int));
            arr[0] = val;
            return 0;
        }
    }
    return -1;
}

/* --- Transposition --- */
int transpose_search(int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            if (i > 0) {
                int tmp = arr[i];
                arr[i] = arr[i - 1];
                arr[i - 1] = tmp;
                return i - 1;
            }
            return 0;
        }
    }
    return -1;
}

/* --- Helpers --- */
void print_arr(const char *label, const int *arr, int n)
{
    printf("%-25s [", label);
    for (int i = 0; i < n; i++)
        printf("%s%d", i ? ", " : "", arr[i]);
    printf("]\n");
}

int main(void)
{
    /* Demo 1: Búsqueda básica */
    printf("=== Demo 1: Búsqueda básica ===\n");
    int arr1[] = {3, 8, 1, 7, 5, 9, 2, 4, 6};
    int n = 9;

    print_arr("Array:", arr1, n);
    int idx = linear_search(arr1, n, 7);
    printf("Buscar 7: índice %d\n", idx);
    idx = linear_search(arr1, n, 10);
    printf("Buscar 10: índice %d\n", idx);

    /* Demo 2: Sentinel */
    printf("\n=== Demo 2: Sentinel ===\n");
    int arr2[] = {3, 8, 1, 7, 5, 9, 2, 4, 6};

    print_arr("Array:", arr2, n);
    idx = sentinel_search(arr2, n, 7);
    printf("Buscar 7: índice %d\n", idx);
    idx = sentinel_search(arr2, n, 10);
    printf("Buscar 10: índice %d\n", idx);
    print_arr("Array después:", arr2, n);

    /* Demo 3: Move-to-front */
    printf("\n=== Demo 3: Move-to-front ===\n");
    int arr3[] = {3, 8, 1, 7, 5};
    int n3 = 5;

    print_arr("Inicial:", arr3, n3);

    idx = mtf_search(arr3, n3, 7);
    printf("Buscar 7 → posición %d\n", idx);
    print_arr("Después:", arr3, n3);

    idx = mtf_search(arr3, n3, 5);
    printf("Buscar 5 → posición %d\n", idx);
    print_arr("Después:", arr3, n3);

    idx = mtf_search(arr3, n3, 7);
    printf("Buscar 7 de nuevo → posición %d\n", idx);
    print_arr("Después:", arr3, n3);

    /* Demo 4: Transposition */
    printf("\n=== Demo 4: Transposition ===\n");
    int arr4[] = {3, 8, 1, 7, 5};
    int n4 = 5;

    print_arr("Inicial:", arr4, n4);

    for (int round = 0; round < 4; round++) {
        idx = transpose_search(arr4, n4, 7);
        printf("Buscar 7 (ronda %d) → posición %d  ", round + 1, idx);
        print_arr("", arr4, n4);
    }

    /* Demo 5: Benchmark sentinel vs básica */
    printf("\n=== Demo 5: Benchmark (10^6 elementos, buscar no existente) ===\n");
    #define BN 1000000
    int *big = (int *)malloc(BN * sizeof(int));
    for (int i = 0; i < BN; i++) big[i] = i * 2;  /* pares: 0,2,4,...*/

    int target = -1;  /* no existe */
    clock_t t0, t1;

    t0 = clock();
    for (int r = 0; r < 20; r++)
        linear_search(big, BN, target);
    t1 = clock();
    double basic_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    t0 = clock();
    for (int r = 0; r < 20; r++)
        sentinel_search(big, BN, target);
    t1 = clock();
    double sentinel_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    printf("Básica:   %7.2f ms (20 búsquedas)\n", basic_ms);
    printf("Sentinel: %7.2f ms (20 búsquedas)\n", sentinel_ms);
    printf("Speedup:  %.2fx\n", basic_ms / sentinel_ms);

    free(big);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -std=c11 -Wall -Wextra -o linear_search linear_search.c
./linear_search
```

### Salida esperada

```
=== Demo 1: Búsqueda básica ===
Array:                    [3, 8, 1, 7, 5, 9, 2, 4, 6]
Buscar 7: índice 3
Buscar 10: índice -1

=== Demo 2: Sentinel ===
Array:                    [3, 8, 1, 7, 5, 9, 2, 4, 6]
Buscar 7: índice 3
Buscar 10: índice -1
Array después:            [3, 8, 1, 7, 5, 9, 2, 4, 6]

=== Demo 3: Move-to-front ===
Inicial:                  [3, 8, 1, 7, 5]
Buscar 7 → posición 0
Después:                  [7, 3, 8, 1, 5]
Buscar 5 → posición 0
Después:                  [5, 7, 3, 8, 1]
Buscar 7 de nuevo → posición 0
Después:                  [7, 5, 3, 8, 1]

=== Demo 4: Transposition ===
Inicial:                  [3, 8, 1, 7, 5]
Buscar 7 (ronda 1) → posición 2   [3, 8, 7, 1, 5]
Buscar 7 (ronda 2) → posición 1   [3, 7, 8, 1, 5]
Buscar 7 (ronda 3) → posición 0   [7, 3, 8, 1, 5]
Buscar 7 (ronda 4) → posición 0   [7, 3, 8, 1, 5]

=== Demo 5: Benchmark (10^6 elementos, buscar no existente) ===
Básica:     ~25.00 ms (20 búsquedas)
Sentinel:   ~20.00 ms (20 búsquedas)
Speedup:    ~1.25x
```

---

## Programa completo en Rust

```rust
// linear_search.rs — Variantes de búsqueda secuencial
use std::time::Instant;

fn linear_search(arr: &[i32], target: i32) -> Option<usize> {
    for (i, &val) in arr.iter().enumerate() {
        if val == target {
            return Some(i);
        }
    }
    None
}

fn sentinel_search(arr: &mut [i32], target: i32) -> Option<usize> {
    let n = arr.len();
    if n == 0 { return None; }

    let last = arr[n - 1];
    arr[n - 1] = target;

    let mut i = 0;
    while arr[i] != target {
        i += 1;
    }

    arr[n - 1] = last;

    if i < n - 1 {
        Some(i)
    } else if last == target {
        Some(n - 1)
    } else {
        None
    }
}

fn mtf_search(arr: &mut [i32], target: i32) -> Option<usize> {
    for i in 0..arr.len() {
        if arr[i] == target {
            let val = arr[i];
            arr.copy_within(0..i, 1);
            arr[0] = val;
            return Some(0);
        }
    }
    None
}

fn transpose_search(arr: &mut [i32], target: i32) -> Option<usize> {
    for i in 0..arr.len() {
        if arr[i] == target {
            if i > 0 {
                arr.swap(i, i - 1);
                return Some(i - 1);
            }
            return Some(0);
        }
    }
    None
}

fn print_arr(label: &str, arr: &[i32]) {
    print!("{:<25} [", label);
    for (i, v) in arr.iter().enumerate() {
        if i > 0 { print!(", "); }
        print!("{}", v);
    }
    println!("]");
}

fn main() {
    // Demo 1: Búsqueda básica
    println!("=== Demo 1: Búsqueda básica ===");
    let arr = [3, 8, 1, 7, 5, 9, 2, 4, 6];
    print_arr("Array:", &arr);
    println!("Buscar 7: {:?}", linear_search(&arr, 7));
    println!("Buscar 10: {:?}", linear_search(&arr, 10));

    // Demo 2: Move-to-front
    println!("\n=== Demo 2: Move-to-front ===");
    let mut arr2 = [3, 8, 1, 7, 5];
    print_arr("Inicial:", &arr2);

    let idx = mtf_search(&mut arr2, 7);
    println!("Buscar 7 → {:?}", idx);
    print_arr("Después:", &arr2);

    let idx = mtf_search(&mut arr2, 5);
    println!("Buscar 5 → {:?}", idx);
    print_arr("Después:", &arr2);

    let idx = mtf_search(&mut arr2, 7);
    println!("Buscar 7 de nuevo → {:?}", idx);
    print_arr("Después:", &arr2);

    // Demo 3: Transposition
    println!("\n=== Demo 3: Transposition ===");
    let mut arr3 = [3, 8, 1, 7, 5];
    print_arr("Inicial:", &arr3);

    for round in 1..=4 {
        let idx = transpose_search(&mut arr3, 7);
        print!("Buscar 7 (ronda {}) → {:?}  ", round, idx);
        print_arr("", &arr3);
    }

    // Demo 4: Idiomatic Rust — Iterator methods
    println!("\n=== Demo 4: Búsqueda idiomática en Rust ===");
    let data = vec![3, 8, 1, 7, 5, 9, 2, 4, 6];
    println!("Array: {:?}", data);

    // contains
    println!("contains(7): {}", data.contains(&7));
    println!("contains(10): {}", data.contains(&10));

    // iter().position()
    println!("position(7): {:?}", data.iter().position(|&x| x == 7));
    println!("position(10): {:?}", data.iter().position(|&x| x == 10));

    // iter().find() — retorna referencia al elemento
    println!("find(>5): {:?}", data.iter().find(|&&x| x > 5));

    // iter().find() con predicado complejo
    let first_even_gt4 = data.iter().find(|&&x| x > 4 && x % 2 == 0);
    println!("first even > 4: {:?}", first_even_gt4);

    // Demo 5: Benchmark
    println!("\n=== Demo 5: Benchmark (10^6 elementos) ===");
    let n = 1_000_000;
    let big: Vec<i32> = (0..n).map(|i| i * 2).collect();
    let mut big_mut = big.clone();
    let target = -1;
    let runs = 20;

    let start = Instant::now();
    for _ in 0..runs {
        let _ = linear_search(&big, target);
    }
    let basic_ms = start.elapsed().as_secs_f64() * 1000.0;

    let start = Instant::now();
    for _ in 0..runs {
        let _ = sentinel_search(&mut big_mut, target);
    }
    let sentinel_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("Básica:   {:>7.2} ms ({} búsquedas)", basic_ms, runs);
    println!("Sentinel: {:>7.2} ms ({} búsquedas)", sentinel_ms, runs);
    println!("Speedup:  {:.2}x", basic_ms / sentinel_ms);
}
```

### Compilar y ejecutar

```bash
rustc -O -o linear_search linear_search.rs
./linear_search
```

---

## Ejercicios

### Ejercicio 1 — Traza de búsqueda básica

Traza la ejecución de `linear_search` para:

```c
int arr[] = {5, 2, 8, 1, 9, 3};
linear_search(arr, 6, 9);
```

¿Cuántas comparaciones se realizan?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
i=0: arr[0]=5 == 9? No
i=1: arr[1]=2 == 9? No
i=2: arr[2]=8 == 9? No
i=3: arr[3]=1 == 9? No
i=4: arr[4]=9 == 9? Sí → retornar 4
```

5 comparaciones de igualdad (más 5 comparaciones de `i < n`).

Si contamos solo comparaciones de igualdad: 5.
Si contamos todas las comparaciones del loop: 10.

El sentinel eliminaría las 5 comparaciones de `i < n`.

</details>

### Ejercicio 2 — Sentinel: caso borde

¿Qué pasa si el elemento buscado está en la **última posición**
del array? Traza `sentinel_search` para:

```c
int arr[] = {3, 8, 1, 7, 5};
sentinel_search(arr, 5, 5);
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
n=5, target=5
last = arr[4] = 5
arr[4] = 5 (centinela = 5, coincide con last)

Array temporal: [3, 8, 1, 7, 5]  (sin cambio visible)

i=0: arr[0]=3 != 5 → i++
i=1: arr[1]=8 != 5 → i++
i=2: arr[2]=1 != 5 → i++
i=3: arr[3]=7 != 5 → i++
i=4: arr[4]=5 == 5 → parar

Restaurar: arr[4] = last = 5 (sin cambio)

i=4, n-1=4 → i == n-1, NO es < n-1
Verificar: last == target? 5 == 5? Sí → retornar n-1 = 4 ✓
```

El caso funciona correctamente gracias a la verificación explícita
`if (last == target)` después de restaurar. Sin esta verificación,
el sentinel no podría distinguir entre "encontrado en la última
posición" y "encontrado solo el centinela".

</details>

### Ejercicio 3 — Move-to-front: distribución Zipf

Si buscamos los elementos `{A, B, C, D}` con frecuencias
respectivas `{50%, 25%, 15%, 10%}`, ¿cuál es el costo promedio de
búsqueda con y sin move-to-front?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Sin optimización** (orden fijo `[A, B, C, D]`):

Costo promedio = $\sum p_i \cdot \text{posición}_i$

Si el orden coincide con la frecuencia:
$= 0.50 \times 1 + 0.25 \times 2 + 0.15 \times 3 + 0.10 \times 4$
$= 0.50 + 0.50 + 0.45 + 0.40 = 1.85$ comparaciones promedio.

Si el orden es el **peor** (inverso):
$= 0.50 \times 4 + 0.25 \times 3 + 0.15 \times 2 + 0.10 \times 1$
$= 2.00 + 0.75 + 0.30 + 0.10 = 3.15$ comparaciones promedio.

**Con move-to-front** (después de convergencia):

El array tiende a organizarse con los más frecuentes al inicio.
En equilibrio, el costo promedio se aproxima al óptimo:
~1.85 comparaciones — independientemente del orden inicial.

Move-to-front converge al orden óptimo sin necesidad de conocer
las frecuencias de antemano. Es una forma de **aprendizaje online**.

</details>

### Ejercicio 4 — Transposition: convergencia

Partiendo de `[D, C, B, A]` y buscando repetidamente `A`, ¿cuántas
búsquedas necesita transposition para que `A` llegue al frente?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
Búsqueda 1: Buscar A en [D, C, B, A]
  Encontrado en i=3, swap con i=2 → [D, C, A, B]

Búsqueda 2: Buscar A en [D, C, A, B]
  Encontrado en i=2, swap con i=1 → [D, A, C, B]

Búsqueda 3: Buscar A en [D, A, C, B]
  Encontrado en i=1, swap con i=0 → [A, D, C, B]

Búsqueda 4: Buscar A en [A, D, C, B]
  Encontrado en i=0, ya al frente → [A, D, C, B]
```

Se necesitan **3 búsquedas** para llevar `A` de la posición 3 a la
posición 0. En general, un elemento en posición $k$ necesita
exactamente $k$ búsquedas exitosas para llegar al frente con
transposition.

Con move-to-front, **1 sola búsqueda** bastaría para cualquier
posición. Este es el trade-off: transposition es más lento en
converger pero más estable frente a anomalías.

</details>

### Ejercicio 5 — Sentinel en array de un solo elemento

¿Qué retorna `sentinel_search` para un array de tamaño 1?

```c
int arr[] = {42};
sentinel_search(arr, 1, 42);  // buscar 42
sentinel_search(arr, 1, 99);  // buscar 99
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Buscar 42 en `[42]`**:

```
n=1, target=42
last = arr[0] = 42
arr[0] = 42 (centinela = target = last)

i=0: arr[0]=42 == 42 → parar

Restaurar: arr[0] = 42

i=0, n-1=0 → i == n-1, NO < n-1
last == target? 42 == 42? Sí → retornar 0 ✓
```

**Buscar 99 en `[42]`**:

```
n=1, target=99
last = arr[0] = 42
arr[0] = 99 (centinela)

i=0: arr[0]=99 == 99 → parar

Restaurar: arr[0] = 42

i=0, n-1=0 → i == n-1, NO < n-1
last == target? 42 == 99? No → retornar -1 ✓
```

El caso de un solo elemento funciona correctamente, pero la
eficiencia del sentinel no tiene sentido aquí — el loop siempre
ejecuta exactamente 0 o 1 iteraciones.

</details>

### Ejercicio 6 — Genérica con void *

Escribe una función genérica `find_max` que encuentre el máximo en
un array de cualquier tipo usando búsqueda secuencial y un
comparador al estilo de `qsort`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
void *find_max(const void *base, size_t n, size_t size,
               int (*cmp)(const void *, const void *))
{
    if (n == 0) return NULL;

    const char *arr = (const char *)base;
    const char *max = arr;  /* primer elemento como candidato */

    for (size_t i = 1; i < n; i++) {
        const char *elem = arr + i * size;
        if (cmp(elem, max) > 0)
            max = elem;
    }

    return (void *)max;
}
```

Uso:

```c
int nums[] = {3, 8, 1, 7, 5};
int *max = find_max(nums, 5, sizeof(int), cmp_int);
printf("Max: %d\n", *max);  /* 8 */

double vals[] = {3.14, 1.41, 2.72};
double *dmax = find_max(vals, 3, sizeof(double), cmp_double);
printf("Max: %.2f\n", *dmax);  /* 3.14 */
```

Siempre $n - 1$ comparaciones. No hay forma de hacerlo en menos
sin información adicional sobre la estructura de los datos.

</details>

### Ejercicio 7 — Comparación: dos comparaciones vs una

¿Es más rápido hacer `if (arr[i] == target)` dentro del loop, o
usar `if (arr[i] >= target)` y luego verificar igualdad fuera, para
un array **ordenado**?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Para un array **ordenado**, se puede usar una sola comparación:

```c
int sorted_search_one_cmp(const int *arr, int n, int target)
{
    int i = 0;
    while (i < n && arr[i] < target)
        i++;

    if (i < n && arr[i] == target)
        return i;
    return -1;
}
```

El loop solo tiene `arr[i] < target` (una comparación de datos).
La verificación de igualdad se hace **una sola vez** al salir.

Comparación de operaciones totales para búsqueda exitosa en
posición $k$:

| Versión | Comparaciones de datos | Comparaciones de límite |
|---------|----------------------|------------------------|
| Dos comparaciones | $k + 1$ (igualdad) | $k + 1$ (`i < n`) |
| Una comparación | $k$ (`<`) + 1 (`==`) | $k + 1$ (`i < n`) |

La versión de una comparación ahorra $k - 1$ comparaciones de
igualdad, reemplazándolas por comparaciones `<` (mismo costo).
La ganancia es mínima.

Combinando con sentinel en array ordenado:

```c
int fast_sorted_search(int *arr, int n, int target)
{
    if (n == 0) return -1;
    int last = arr[n - 1];
    arr[n - 1] = target;

    int i = 0;
    while (arr[i] < target) i++;

    arr[n - 1] = last;
    if (arr[i] == target && i < n) return i;
    if (i == n - 1 && last == target) return n - 1;
    return -1;
}
```

Esto elimina ambas comparaciones redundantes: `i < n` y la igualdad
dentro del loop.

</details>

### Ejercicio 8 — Move-to-front en lista enlazada

¿Por qué move-to-front es más eficiente en una lista enlazada que
en un array? Implementa move-to-front para una lista enlazada simple.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

En un **array**, mover al frente requiere desplazar todos los
elementos anteriores una posición a la derecha: $O(i)$ operaciones
con `memmove`.

En una **lista enlazada**, mover al frente es $O(1)$ después de
encontrar el nodo:

1. Desenlazar el nodo de su posición actual (cambiar 1 puntero).
2. Insertarlo al inicio (cambiar 2 punteros).

```c
typedef struct Node {
    int data;
    struct Node *next;
} Node;

Node *mtf_list_search(Node **head, int target)
{
    Node *prev = NULL;
    Node *curr = *head;

    while (curr != NULL) {
        if (curr->data == target) {
            if (prev != NULL) {
                /* Desenlazar */
                prev->next = curr->next;
                /* Mover al frente */
                curr->next = *head;
                *head = curr;
            }
            return *head;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}
```

La búsqueda sigue siendo $O(n)$ en el peor caso, pero el
move-to-front es $O(1)$ en lugar de $O(n)$. Para datos con
localidad temporal (accesos repetidos al mismo elemento), esta
combinación es muy eficiente.

</details>

### Ejercicio 9 — Búsqueda con predicado en Rust

Escribe una función que encuentre el **último** elemento que
satisface un predicado, usando búsqueda secuencial. Compara con
los métodos de iterador de Rust.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
// Versión manual
fn find_last<T, F: Fn(&T) -> bool>(arr: &[T], pred: F) -> Option<usize> {
    let mut last = None;
    for (i, elem) in arr.iter().enumerate() {
        if pred(elem) {
            last = Some(i);
        }
    }
    last
}

// Uso
let data = vec![3, 8, 1, 7, 5, 9, 2, 4, 6];
let idx = find_last(&data, |&x| x > 5);
// idx = Some(8) (data[8] = 6... no, 6 > 5 sí)
// Revisando: 8>5(i=1), 7>5(i=3), 9>5(i=5), 6>5(i=8) → Some(8)
```

Versiones idiomáticas de Rust:

```rust
// Con rposition (busca desde el final)
let idx = data.iter().rposition(|&x| x > 5);
// Equivalente pero más eficiente: recorre desde el final y para
// en el primer match → O(n-k) en lugar de O(n)

// Con rfind (referencia al elemento, desde el final)
let val = data.iter().rfind(|&&x| x > 5);
```

`rposition` es más eficiente que la versión manual porque recorre
desde el final — para la primera coincidencia, que es la última
en el orden original. La versión manual siempre recorre todo el
array.

</details>

### Ejercicio 10 — Punto de cruce: secuencial vs binaria

Si la búsqueda secuencial tarda 5 ns por elemento y la binaria
tarda 15 ns por paso (más overhead de cálculo del punto medio),
¿para qué tamaño de array la binaria empieza a ser más rápida?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Costo promedio de búsqueda exitosa:

- **Secuencial**: $\frac{n}{2} \times 5 \text{ ns} = 2.5n$ ns
- **Binaria**: $\log_2(n) \times 15$ ns

Igualar: $2.5n = 15 \log_2(n)$

| $n$ | Secuencial ($2.5n$ ns) | Binaria ($15 \log_2 n$ ns) |
|-----|----------------------|--------------------------|
| 4 | 10 | 30 |
| 8 | 20 | 45 |
| 16 | 40 | 60 |
| 32 | 80 | 75 |
| 64 | 160 | 90 |

El cruce está alrededor de $n \approx 30$. Para $n < 30$, la
búsqueda secuencial es más rápida. Para $n > 30$, la binaria domina.

En la práctica, el cruce varía entre 16 y 64 dependiendo de la
CPU y la localidad de cache. Por esto, implementaciones como
`std::lower_bound` en C++ cambian a búsqueda secuencial para los
últimos ~16 elementos de la búsqueda binaria.

Este es el mismo patrón que vimos en sorting: los algoritmos simples
$O(n)$/$O(n^2)$ ganan para $n$ pequeño por menor overhead constante.

</details>

---

## Resumen

| Variante | Complejidad | Modifica array | Mejor para |
|----------|-------------|----------------|------------|
| Básica | $O(n)$ | No | Caso general, datos inmutables |
| Sentinel | $O(n)$, ~20-30% menos tiempo | Sí (temporal) | Búsquedas fallidas frecuentes |
| Move-to-front | $O(n)$ amortizado, tiende a $O(\log n)$ con Zipf | Sí | Distribución muy sesgada |
| Transposition | $O(n)$ amortizado | Sí | Distribución que cambia |
| En array ordenado | $O(n)$ pero con early exit | No | Cuando no se justifica binaria |
