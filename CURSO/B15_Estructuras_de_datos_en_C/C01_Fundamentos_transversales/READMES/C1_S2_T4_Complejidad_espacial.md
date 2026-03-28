# T04 — Complejidad espacial

---

## 1. ¿Qué mide la complejidad espacial?

La complejidad temporal mide cuánto **tiempo** crece con la entrada.
La complejidad espacial mide cuánta **memoria** crece con la entrada.

```
Algoritmo A: ordena un array de n enteros
  Temporal:  O(n log n)
  Espacial:  O(n)        ← necesita un array auxiliar de n elementos

Algoritmo B: ordena el mismo array
  Temporal:  O(n log n)
  Espacial:  O(1)        ← ordena in-place, sin array auxiliar
```

Ambos tardan lo mismo, pero B usa mucha menos memoria. Para $n = 10^8$
enteros (400 MB), A necesita 800 MB y B necesita 400 MB.

---

## 2. Espacio total vs espacio extra (auxiliar)

Hay dos formas de medir el espacio:

### Espacio total

Incluye la entrada misma:

```c
void sum_array(int *arr, int n) {
    int total = 0;                    // O(1) extra
    for (int i = 0; i < n; i++)
        total += arr[i];
    // Espacio total: O(n) — el array está en memoria
}
```

### Espacio extra (auxiliar)

Solo cuenta la memoria **adicional** que el algoritmo aloca, sin contar
la entrada:

```c
void sum_array(int *arr, int n) {
    int total = 0;       // 1 variable extra
    int i;               // 1 variable extra
    // Espacio extra: O(1) — solo 2 variables, independiente de n
}
```

**Convención del curso**: cuando decimos "espacio $O(f(n))$" nos referimos al
**espacio extra** (auxiliar), salvo que se diga explícitamente "espacio total".
Esta es la convención más usada en la industria.

### Por qué importa la distinción

```
Merge sort:
  Espacio total: O(n)     (array + buffer auxiliar)
  Espacio extra: O(n)     (buffer auxiliar de n elementos)

Heapsort:
  Espacio total: O(n)     (el array)
  Espacio extra: O(1)     (ordena in-place)

Ambos tienen espacio total O(n), pero el espacio EXTRA difiere.
La diferencia práctica: merge sort necesita el doble de RAM que heapsort.
```

---

## 3. Clasificación de espacio extra

### O(1) — Constante (in-place)

El algoritmo usa una cantidad fija de variables, independiente de $n$:

```c
// Invertir array in-place: O(1) espacio extra
void reverse(int *arr, int n) {
    for (int i = 0; i < n / 2; i++) {
        int tmp = arr[i];
        arr[i] = arr[n - 1 - i];
        arr[n - 1 - i] = tmp;
    }
    // Solo: i, tmp → O(1)
}

// Swap two elements: O(1)
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
    // Solo: tmp → O(1)
}

// Selection sort: O(1) espacio extra
void selection_sort(int *arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++)
            if (arr[j] < arr[min]) min = j;
        swap(&arr[i], &arr[min]);
    }
    // Solo: i, j, min → O(1)
}
```

### O(log n) — Logarítmico

Común en algoritmos recursivos donde la profundidad de recursión es $\log n$:

```c
// Quicksort (versión sin array auxiliar): O(log n) espacio en stack
void quicksort(int *arr, int lo, int hi) {
    if (lo >= hi) return;
    int pivot = partition(arr, lo, hi);
    quicksort(arr, lo, pivot - 1);
    quicksort(arr, pivot + 1, hi);
    // Cada llamada recursiva usa espacio en el call stack
    // Profundidad máxima: O(log n) si las particiones son balanceadas
    //                     O(n) en el peor caso (particiones degeneradas)
}
```

El stack de llamadas recursivas consume memoria. Cada frame tiene variables
locales (`lo`, `hi`, `pivot`, dirección de retorno). Con profundidad
$O(\log n)$, el espacio es $O(\log n)$.

### O(n) — Lineal

El algoritmo necesita una estructura auxiliar proporcional a la entrada:

```c
// Merge sort: O(n) espacio extra
void merge_sort(int *arr, int n) {
    if (n <= 1) return;
    int mid = n / 2;
    merge_sort(arr, mid);
    merge_sort(arr + mid, n - mid);

    int *tmp = malloc(n * sizeof(int));  // buffer auxiliar de n elementos
    merge(arr, mid, arr + mid, n - mid, tmp);
    memcpy(arr, tmp, n * sizeof(int));
    free(tmp);
    // Espacio extra: O(n) por el buffer tmp
}
```

```c
// Contar frecuencia de caracteres: O(1) espacio extra (256 es constante)
void count_chars(const char *s) {
    int freq[256] = {0};   // 256 es constante, no depende de strlen(s)
    for (int i = 0; s[i]; i++)
        freq[(unsigned char)s[i]]++;
    // Espacio extra: O(1) — el array freq tiene tamaño fijo 256
}

// Contar frecuencia de palabras: O(n) espacio extra
// (n palabras distintas → n entradas en la hash table)
```

### O(n²) — Cuadrático

```c
// Matriz de adyacencia para grafo: O(V²) espacio
int **adj = malloc(V * sizeof(int *));
for (int i = 0; i < V; i++)
    adj[i] = calloc(V, sizeof(int));
// Espacio: V × V = O(V²)
```

---

## 4. Espacio del stack de llamadas (recursión)

Las llamadas recursivas no son "gratis" en espacio — cada llamada agrega
un frame al call stack:

```
factorial(5):

  Stack:
  ┌──────────────┐
  │ factorial(1) │  ← tope (frame 5)
  ├──────────────┤
  │ factorial(2) │  (frame 4)
  ├──────────────┤
  │ factorial(3) │  (frame 3)
  ├──────────────┤
  │ factorial(4) │  (frame 2)
  ├──────────────┤
  │ factorial(5) │  (frame 1)
  └──────────────┘

  5 frames → O(n) espacio en stack
```

```c
// factorial: O(n) espacio (n frames en el stack)
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);   // n llamadas anidadas
}

// fibonacci ingenuo: O(n) espacio (profundidad máxima n)
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
    // Aunque hay 2ⁿ llamadas en total (tiempo O(2ⁿ)),
    // el espacio es O(n) porque solo n frames están activos a la vez
    // (la rama izquierda se completa antes de empezar la derecha)
}
```

### Fibonacci: tiempo O(2ⁿ) pero espacio O(n)

```
fib(5):
                    fib(5)
                   /      \
              fib(4)      fib(3)
             /     \      /    \
         fib(3)  fib(2) fib(2) fib(1)
        /    \
    fib(2) fib(1)

Tiempo: ~2⁵ = 32 nodos (llamadas)
Espacio máximo: 5 frames en stack (la rama más profunda)

En cualquier instante, solo UNA rama está activa.
fib(3) izquierdo se resuelve completamente (y libera sus frames)
antes de que fib(2) derecho comience.
```

### Tail recursion: O(1) espacio (con optimización)

```c
// Factorial con tail recursion (el compilador puede optimizar a loop)
int factorial_tail(int n, int acc) {
    if (n <= 1) return acc;
    return factorial_tail(n - 1, n * acc);  // la llamada recursiva es lo último
}
// Con -O2, GCC convierte esto en un loop → O(1) espacio
// Sin optimización, sigue siendo O(n) espacio
```

En Rust, el compilador no garantiza tail call optimization. Para $O(1)$
espacio, se usa un loop explícito:

```rust
fn factorial(n: u64) -> u64 {
    let mut acc = 1;
    for i in 2..=n {
        acc *= i;
    }
    acc
}
```

---

## 5. Tradeoffs tiempo-espacio

Frecuentemente, usar más memoria permite menos tiempo (y viceversa):

### Ejemplo 1: Lookup table vs cálculo

```c
// Opción A: calcular sin(x) cada vez — O(1) espacio, O(k) tiempo
double my_sin(double x) {
    // Aproximación con serie de Taylor: k términos
    double result = 0;
    for (int i = 0; i < k; i++) { /* ... */ }
    return result;
}

// Opción B: tabla pre-calculada — O(n) espacio, O(1) tiempo
double sin_table[3600];  // cada 0.1 grados
void init_table(void) {
    for (int i = 0; i < 3600; i++)
        sin_table[i] = sin(i * 0.1 * M_PI / 180.0);
}
double fast_sin(double degrees) {
    int idx = (int)(degrees * 10) % 3600;
    return sin_table[idx];
}
```

### Ejemplo 2: Memoización

```c
// Fibonacci sin memo: O(1) espacio, O(2ⁿ) tiempo
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

// Fibonacci con memo: O(n) espacio, O(n) tiempo
int memo[1000] = {0};
int fib_memo(int n) {
    if (n <= 1) return n;
    if (memo[n] != 0) return memo[n];
    memo[n] = fib_memo(n - 1) + fib_memo(n - 2);
    return memo[n];
}
```

```
              Sin memo              Con memo
Tiempo:       O(2ⁿ)                O(n)
Espacio:      O(n) stack           O(n) tabla + O(n) stack
n = 40:       ~10⁹ llamadas       40 cálculos
n = 50:       ~10¹⁵ llamadas      50 cálculos
```

### Ejemplo 3: Duplicados en array

```c
// Opción A: fuerza bruta — O(1) espacio, O(n²) tiempo
bool has_duplicate_brute(int *arr, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (arr[i] == arr[j]) return true;
    return false;
}

// Opción B: ordenar + scan — O(1) espacio extra*, O(n log n) tiempo
bool has_duplicate_sort(int *arr, int n) {
    qsort(arr, n, sizeof(int), cmp_int);  // modifica el array
    for (int i = 0; i < n - 1; i++)
        if (arr[i] == arr[i + 1]) return true;
    return false;
}
// * heapsort es in-place; qsort de glibc puede usar O(log n) extra

// Opción C: hash set — O(n) espacio, O(n) tiempo
bool has_duplicate_hash(int *arr, int n) {
    Set *seen = set_create(cmp_int, NULL);
    for (int i = 0; i < n; i++) {
        if (set_contains(seen, &arr[i])) {
            set_destroy(seen);
            return true;
        }
        set_insert(seen, &arr[i]);
    }
    set_destroy(seen);
    return false;
}
```

| Opción | Tiempo | Espacio extra | ¿Modifica input? |
|--------|--------|--------------|-------------------|
| Fuerza bruta | $O(n^2)$ | $O(1)$ | No |
| Ordenar + scan | $O(n \log n)$ | $O(1)$* | Sí |
| Hash set | $O(n)$ | $O(n)$ | No |

No hay opción "perfecta" — cada una sacrifica algo.

### Tabla general de tradeoffs

| Más espacio → | Tiempo sin vs con |
|--------------|-------------------|
| Lookup tables | $O(k)$ cálculo → $O(1)$ acceso |
| Caches/memo | $O(2^n)$ recalcular → $O(n)$ con tabla $O(n)$ |
| Hash tables | $O(n)$ búsqueda lineal → $O(1)$ con tabla $O(n)$ |
| Índices (DB) | $O(n)$ full scan → $O(\log n)$ con índice $O(n)$ |
| Grafos: matriz adj | $O(\text{grado})$ verificar arista → $O(1)$ con matriz $O(V^2)$ |

| Menos espacio → | Más tiempo |
|----------------|------------|
| In-place sort | $O(1)$ extra pero más swaps |
| Streaming | $O(1)$ espacio pero una pasada |
| Compresión | Menos espacio pero CPU para des/comprimir |

---

## 6. Complejidad espacial de estructuras de datos

| Estructura | Espacio | Overhead por elemento | Notas |
|-----------|---------|----------------------|-------|
| Array | $O(n)$ | 0 (contiguo) | Puede tener overhead por capacidad no usada |
| Vec/vector dinámico | $O(n)$ | ~0-50% (capacidad extra) | Peor caso: 50% desperdiciado justo después de realloc |
| Lista enlazada simple | $O(n)$ | 1 puntero (8 bytes en 64-bit) | Cada nodo: dato + next |
| Lista doblemente enlazada | $O(n)$ | 2 punteros (16 bytes) | Cada nodo: dato + next + prev |
| BST | $O(n)$ | 2 punteros (16 bytes) | Cada nodo: dato + left + right |
| AVL | $O(n)$ | 2 punteros + 1 int (20 bytes) | + factor de balance |
| Red-Black tree | $O(n)$ | 2 punteros + 1 int (20 bytes) | + color (en la práctica `int` o `enum`, no 1 bit) |
| Hash table (chaining) | $O(n + m)$ | 1 puntero por nodo + array de m buckets | m = número de buckets |
| Hash table (open addr) | $O(m)$ | ~0 extra pero tabla más grande (m > n) | Load factor < 1 |
| Grafo (matriz adj) | $O(V^2)$ | — | Independiente del número de aristas |
| Grafo (lista adj) | $O(V + E)$ | 1 puntero por arista | Eficiente para grafos dispersos |
| Heap binario (array) | $O(n)$ | 0 (array implícito) | Sin punteros — posiciones calculadas |
| Trie | $O(\Sigma \cdot m)$ | $\Sigma$ punteros por nodo | $\Sigma$ = tamaño alfabeto, m = nodos |

### Array vs lista enlazada: el costo oculto

```
Para n = 1000 enteros (int32):

Array:     1000 × 4 bytes = 4,000 bytes = 3.9 KB
           + sin overhead por elemento
           + contiguo en memoria (cache-friendly)

Lista:     1000 × (4 + 8) bytes = 12,000 bytes = 11.7 KB
           + 8 bytes de overhead por nodo (puntero next en 64-bit)
           + nodos dispersos en el heap (cache-unfriendly)
           + 1000 llamadas a malloc (metadata del allocator: ~16-32 bytes cada una)

Espacio real de la lista: ~28-44 KB (vs 3.9 KB del array)
Factor: 7-11x más memoria para los mismos 1000 enteros
```

Este overhead es la razón por la que `Vec<T>` es casi siempre preferible
a una lista enlazada para datos pequeños.

---

## 7. In-place: ¿realmente O(1)?

Un algoritmo "in-place" usa $O(1)$ espacio extra. Pero hay matices:

### Heapsort: genuinamente in-place

```c
// Heapsort: O(1) espacio extra, O(n log n) tiempo
void heapsort(int *arr, int n) {
    // Build heap in-place (reusa el array)
    for (int i = n / 2 - 1; i >= 0; i--)
        heapify_down(arr, n, i);

    // Extract elements
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        heapify_down(arr, i, 0);
    }
    // Solo variables locales: i, tmp en swap → O(1)
}
```

### Quicksort: "in-place" pero O(log n) en stack

```c
// Quicksort: O(1) espacio auxiliar explícito,
// pero O(log n) en el call stack (O(n) worst case)
void quicksort(int *arr, int lo, int hi) {
    if (lo >= hi) return;
    int p = partition(arr, lo, hi);
    quicksort(arr, lo, p - 1);     // ← frame en el stack
    quicksort(arr, p + 1, hi);     // ← frame en el stack
}
```

Quicksort se llama "in-place" porque no aloca arrays auxiliares, pero
la recursión usa $O(\log n)$ espacio en el stack. Estrictamente, no es $O(1)$.

Para hacerlo genuinamente $O(\log n)$ espacio en el peor caso: recursar
primero en la partición más pequeña y convertir la más grande en tail call
(o loop):

```c
void quicksort_tail(int *arr, int lo, int hi) {
    while (lo < hi) {
        int p = partition(arr, lo, hi);
        if (p - lo < hi - p) {
            quicksort_tail(arr, lo, p - 1);   // recursión en la más pequeña
            lo = p + 1;                         // loop para la más grande
        } else {
            quicksort_tail(arr, p + 1, hi);
            hi = p - 1;
        }
    }
    // Profundidad máxima: O(log n) garantizado
}
```

### Resumen de "in-place" en sorting

| Algoritmo | Espacio extra real | Se considera in-place |
|-----------|-------------------|----------------------|
| Selection sort | $O(1)$ | Sí |
| Insertion sort | $O(1)$ | Sí |
| Heapsort | $O(1)$ | Sí |
| Quicksort | $O(\log n)$ stack | Sí (por convención) |
| Merge sort | $O(n)$ buffer | No |
| Timsort | $O(n)$ buffer | No |
| Counting sort | $O(k)$ | No (k = rango) |

---

## 8. Espacio en Rust vs C

### Stack vs Heap

```rust
// Rust: stack allocation — O(1) espacio heap, datos en stack
fn stack_example() {
    let x = 42;           // stack: 4 bytes
    let arr = [0i32; 100]; // stack: 400 bytes
    let tup = (1, 2.0, 'a'); // stack
}
// Todo se libera automáticamente al salir del scope

// Rust: heap allocation — O(n) espacio heap
fn heap_example() {
    let v = vec![0; 1000]; // heap: 4000 bytes + 24 bytes metadata (ptr, len, cap)
    let s = String::from("hello"); // heap: 5 bytes + 24 bytes metadata
    let b = Box::new(42);  // heap: 4 bytes + 8 bytes puntero
}
// Drop libera automáticamente al salir del scope
```

### Box, Rc, Arc: overhead de indirección

```rust
// Almacenar un i32:
let x: i32 = 42;              // 4 bytes, stack
let b: Box<i32> = Box::new(42); // 4 + 8 = 12 bytes, heap + puntero
let r: Rc<i32> = Rc::new(42);   // 4 + 8 + 8 = 20 bytes, heap + ptr + refcount
let a: Arc<i32> = Arc::new(42); // 4 + 8 + 8 + 8 = 28 bytes, heap + ptr + 2 refcounts

// Para un i32, Box usa 3x la memoria
// Para un struct grande de 1000 bytes, el overhead es ~1%
```

### Comparación de overhead por nodo en una lista enlazada

```
C (lista simple):
  struct Node { int data; struct Node *next; };
  Tamaño: 4 + 8 = 12 bytes (+ padding = 16 bytes)
  + malloc metadata: ~16-32 bytes
  Total real: ~32-48 bytes por nodo

Rust (Option<Box<Node>>):
  struct Node { data: i32, next: Option<Box<Node>> }
  Tamaño: 4 + 8 = 12 bytes (Option<Box<T>> se optimiza a un puntero nullable)
  + allocator metadata: ~16 bytes
  Total real: ~28 bytes por nodo

Rust (con arena/pool allocator):
  Tamaño: 12 bytes por nodo (sin overhead de malloc individual)
```

---

## 9. Medir el espacio en la práctica

### En C: Valgrind massif

```bash
valgrind --tool=massif ./program
ms_print massif.out.12345
```

Massif muestra un gráfico del uso de heap a lo largo del tiempo:

```
    KB
19.71^                                                       #
     |                                                       #
     |                                                    @@@#
     |                                               @@@@@@@#
     |                                          @@@@@@@@@@@@#
     |                                     @@@@@@@@@@@@@@@@@#
     |                                @@@@@@@@@@@@@@@@@@@@@@#
     |                           @@@@@@@@@@@@@@@@@@@@@@@@@@@#
     |                      @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
     |                 @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
     |            @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
     |       @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
     |  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
     | @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
   0 +------------------------------------------------------->
```

### En Rust: std::mem::size_of

```rust
use std::mem::size_of;

println!("i32:           {} bytes", size_of::<i32>());           // 4
println!("Box<i32>:      {} bytes", size_of::<Box<i32>>());      // 8 (puntero)
println!("Vec<i32>:      {} bytes", size_of::<Vec<i32>>());      // 24 (ptr+len+cap)
println!("Option<i32>:   {} bytes", size_of::<Option<i32>>());   // 8
println!("Option<Box<i32>>: {} bytes", size_of::<Option<Box<i32>>>()); // 8 (niche opt)
```

---

## Ejercicios

### Ejercicio 1 — Clasificar espacio extra

¿Cuál es el espacio extra de cada fragmento?

```c
// A
int max = arr[0];
for (int i = 1; i < n; i++)
    if (arr[i] > max) max = arr[i];

// B
int *copy = malloc(n * sizeof(int));
memcpy(copy, arr, n * sizeof(int));
qsort(copy, n, sizeof(int), cmp);
free(copy);

// C
void foo(int n) {
    if (n <= 0) return;
    foo(n - 1);
}

// D
int matrix[100][100];
for (int i = 0; i < 100; i++)
    for (int j = 0; j < 100; j++)
        matrix[i][j] = i * j;

// E
int **matrix = malloc(n * sizeof(int *));
for (int i = 0; i < n; i++)
    matrix[i] = malloc(n * sizeof(int));
```

**Predicción**: Clasifica cada uno en $O(1)$, $O(\log n)$, $O(n)$ o $O(n^2)$.

<details><summary>Respuesta</summary>

| Fragmento | Espacio extra | Razón |
|-----------|-------------|-------|
| A | **$O(1)$** | Solo `max` e `i` — constante |
| B | **$O(n)$** | `copy` es un array de $n$ elementos |
| C | **$O(n)$** | $n$ frames en el call stack (recursión de profundidad $n$) |
| D | **$O(1)$** | $100 \times 100$ es constante (no depende de $n$) |
| E | **$O(n^2)$** | $n$ punteros + $n \times n$ enteros |

Matiz de D: `int matrix[100][100]` aloca 40,000 bytes en el stack, que
es bastante. Pero como no depende de ninguna variable $n$, es $O(1)$ en
términos de complejidad. En la práctica, 40 KB en el stack podría ser
un problema (stack overflow con recursión profunda).

Matiz de C: aunque `foo` no hace "nada útil", cada llamada recursiva
ocupa espacio en el stack. La complejidad espacial incluye el stack.

</details>

---

### Ejercicio 2 — Espacio de algoritmos de sorting

Completa la tabla de espacio extra para cada algoritmo:

```
Algoritmo          Espacio extra     ¿In-place?
Bubble sort           ?                ?
Insertion sort        ?                ?
Selection sort        ?                ?
Merge sort            ?                ?
Quicksort             ?                ?
Heapsort              ?                ?
Counting sort         ?                ?
Radix sort            ?                ?
```

**Predicción**: ¿Cuáles son genuinamente $O(1)$? ¿Cuáles se consideran
in-place por convención aunque no lo sean estrictamente?

<details><summary>Respuesta</summary>

| Algoritmo | Espacio extra | ¿In-place? |
|-----------|-------------|-----------|
| Bubble sort | **$O(1)$** | Sí |
| Insertion sort | **$O(1)$** | Sí |
| Selection sort | **$O(1)$** | Sí |
| Merge sort | **$O(n)$** | No |
| Quicksort | **$O(\log n)$** stack | Sí (convención) |
| Heapsort | **$O(1)$** | Sí |
| Counting sort | **$O(k)$** donde k = rango | No |
| Radix sort | **$O(n + k)$** | No |

Genuinamente $O(1)$: bubble, insertion, selection, heapsort.

In-place por convención: quicksort — no aloca arrays auxiliares pero
usa $O(\log n)$ en el stack de recursión.

Merge sort puede hacerse in-place (merge sort in-place existe) pero es
significativamente más complejo y lento en la práctica. La versión estándar
usa $O(n)$ extra.

</details>

---

### Ejercicio 3 — Tradeoff tiempo-espacio: duplicados

Implementa "¿hay duplicados en un array?" de tres formas en C. Mide
tiempo y espacio para $n = 10^4$:

1. Fuerza bruta $O(n^2)$ tiempo, $O(1)$ espacio
2. Ordenar primero $O(n \log n)$ tiempo, $O(1)$ espacio extra (modifica input)
3. Hash set $O(n)$ tiempo, $O(n)$ espacio

**Predicción**: ¿Cuál será más rápida para $n = 10^4$? ¿Y para $n = 10^2$?

<details><summary>Respuesta</summary>

```c
// 1. Fuerza bruta: O(n²) tiempo, O(1) espacio
bool has_dup_brute(int *arr, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (arr[i] == arr[j]) return true;
    return false;
}

// 2. Sort + scan: O(n log n) tiempo, O(1) espacio extra
bool has_dup_sort(int *arr, int n) {
    qsort(arr, n, sizeof(int), cmp_int);
    for (int i = 0; i < n - 1; i++)
        if (arr[i] == arr[i + 1]) return true;
    return false;
}

// 3. Hash set: O(n) tiempo promedio, O(n) espacio
bool has_dup_hash(int *arr, int n) {
    Set *seen = set_create(cmp_int, NULL);
    for (int i = 0; i < n; i++) {
        if (set_contains(seen, &arr[i])) {
            set_destroy(seen);
            return true;
        }
        set_insert(seen, &arr[i]);
    }
    set_destroy(seen);
    return false;
}
```

Para $n = 10^4$:
- Brute: ~$10^8$/2 comparaciones → lento (~50-100 ms)
- Sort: ~$10^4 \times 13$ comparaciones → rápido (~1 ms)
- Hash: ~$10^4$ lookups $O(1)$ → rápido (~1-2 ms)

**Sort y hash ganan**, hash un poco más rápido en teoría pero sort
es competitivo por cache locality.

Para $n = 10^2$:
- Brute: ~5000 comparaciones → rápido (<0.01 ms)
- Sort: ~700 comparaciones → rápido (<0.01 ms)
- Hash: ~100 lookups pero overhead de crear/destruir set

**Para $n$ pequeño, brute puede ganar** porque no tiene overhead de
setup (no hay malloc, no hay función hash).

</details>

---

### Ejercicio 4 — Espacio del call stack

¿Cuál es la profundidad máxima del call stack (y por lo tanto el espacio
extra) de cada función?

```c
// A
int sum(int *arr, int n) {
    if (n == 0) return 0;
    return arr[n-1] + sum(arr, n-1);
}

// B
int binary_search(int *arr, int lo, int hi, int target) {
    if (lo > hi) return -1;
    int mid = lo + (hi - lo) / 2;
    if (arr[mid] == target) return mid;
    if (target < arr[mid])
        return binary_search(arr, lo, mid - 1, target);
    return binary_search(arr, mid + 1, hi, target);
}

// C
void hanoi(int n, char from, char to, char aux) {
    if (n == 0) return;
    hanoi(n-1, from, aux, to);
    printf("Move %d: %c -> %c\n", n, from, to);
    hanoi(n-1, aux, to, from);
}
```

**Predicción**: ¿Cuál tiene mayor profundidad para $n = 20$?

<details><summary>Respuesta</summary>

| Función | Profundidad | Espacio stack |
|---------|-----------|--------------|
| A (sum) | $n = 20$ | **$O(n)$** — reducción lineal |
| B (binary_search) | $\log_2(20) \approx 4\text{-}5$ | **$O(\log n)$** — divide por 2 |
| C (hanoi) | $n = 20$ | **$O(n)$** — reduce en 1, pero dos llamadas |

Para $n = 20$:
- A: 20 frames
- B: ~5 frames
- C: 20 frames (la profundidad es $n$, no $2^n$)

**B tiene la menor profundidad** porque divide el problema por 2.

Nota sobre C: Torres de Hanoi hace $2^n$ llamadas **en total** ($O(2^n)$ tiempo)
pero la profundidad es solo $n$. En cualquier momento, hay como máximo $n$
frames activos. El segundo `hanoi(n-1, ...)` no comienza hasta que el
primero termina completamente.

Esto es análogo a fibonacci: $O(2^n)$ tiempo pero $O(n)$ espacio.

</details>

---

### Ejercicio 5 — Overhead real de una lista enlazada

Calcula la memoria real (incluyendo overhead de malloc) para almacenar
1000 enteros (int32) en:

1. Un array (`int arr[1000]`)
2. Un `Vec<i32>` de Rust (capacidad 1024 tras reallocs)
3. Una lista enlazada simple en C con malloc por nodo

Asume sistema 64-bit, malloc agrega 16 bytes de metadata por alocación.

**Predicción**: ¿Cuántas veces más memoria usa la lista comparada con
el array?

<details><summary>Respuesta</summary>

**1. Array `int arr[1000]`**:
```
1000 × 4 bytes = 4,000 bytes = 3.9 KB
Una sola alocación (stack o malloc).
Si malloc: +16 bytes overhead = 4,016 bytes.
```

**2. Vec\<i32\> con capacidad 1024**:
```
Datos: 1024 × 4 = 4,096 bytes (capacidad, no size)
Metadata: 24 bytes (ptr + len + cap en struct Vec)
Allocator overhead: ~16 bytes
Total: ~4,136 bytes = 4.0 KB
Desperdicio: 24 slots × 4 bytes = 96 bytes (2.3%)
```

**3. Lista enlazada simple en C**:
```
struct Node { int data; struct Node *next; };

sizeof(Node) = 4 + 8 = 12 bytes
Padding a 16 bytes (alineación de 8 bytes para el puntero)
+ 16 bytes malloc metadata por nodo

Por nodo: 16 + 16 = 32 bytes
1000 nodos: 32,000 bytes = 31.25 KB
+ struct para la cabeza de la lista: ~8-16 bytes
```

| Estructura | Memoria total | Factor vs array |
|-----------|-------------|----------------|
| Array | 4,000 bytes | **1x** |
| Vec | 4,136 bytes | 1.03x |
| Lista enlazada | 32,000 bytes | **8x** |

La lista enlazada usa **8 veces más memoria** que el array para los
mismos 1000 enteros. Esto es por:
- 8 bytes de puntero `next` por nodo
- 4 bytes de padding por alineación
- 16 bytes de malloc metadata por nodo (¡el mayor overhead!)

Para datos grandes (ej: struct de 1000 bytes por nodo), el overhead
relativo se reduce: $(1000+8+16)/1000 \approx 2.4\%$ overhead, vs $(4+8+16)/4 = 700\%$.

</details>

---

### Ejercicio 6 — Memoización: medir el tradeoff

Implementa fibonacci con y sin memoización. Mide tiempo y memoria para
$n = 40$:

```c
// Sin memo
long long fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}

// Con memo
long long memo[100] = {0};
int memo_set[100] = {0};
long long fib_memo(int n) {
    if (n <= 1) return n;
    if (memo_set[n]) return memo[n];
    memo_set[n] = 1;
    memo[n] = fib_memo(n-1) + fib_memo(n-2);
    return memo[n];
}
```

**Predicción**: ¿Cuántas veces más rápido es fib_memo(40)? ¿Cuánta
memoria extra usa?

<details><summary>Respuesta</summary>

```
Sin memo (n=40):
  Llamadas: ~2⁴⁰ ≈ 10¹² — impracticable
  Tiempo: ~minutos a horas
  Espacio: O(40) stack = ~40 frames × ~32 bytes = ~1.3 KB

Con memo (n=40):
  Llamadas: ~80 (cada valor se calcula una vez)
  Tiempo: ~microsegundos
  Espacio: O(40) tabla = 40 × 16 bytes = 640 bytes + stack
```

Speedup: ~$10^{10}$ veces (diez mil millones). No es un typo.

Memoria extra: ~1.3 KB (dos arrays de 100 elementos) — trivial.

Este es el caso más dramático de tradeoff tiempo-espacio: 1.3 KB de
memoria extra convierten un problema imposible en uno instantáneo.

La razón: sin memo, `fib(38)` se calcula ~$10^8$ veces. Con memo, se
calcula **una vez** y se reutiliza. El espacio extra "recuerda" resultados
previos.

Alternativa aún mejor — iterativa $O(1)$ espacio:

```c
long long fib_iter(int n) {
    if (n <= 1) return n;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        long long tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}
// O(n) tiempo, O(1) espacio — lo mejor de ambos mundos
```

</details>

---

### Ejercicio 7 — Grafo: matriz vs lista de adyacencia

Para un grafo con $V = 1000$ vértices y $E$ aristas, calcula el espacio de:
1. Matriz de adyacencia (`int adj[V][V]`)
2. Lista de adyacencia (`struct Node *adj[V]`)

Para tres valores de E: 1000 (disperso), 100,000 (moderado), 499,500 (completo).

**Predicción**: ¿Para qué valor de $E$ la lista deja de ser ventajosa?

<details><summary>Respuesta</summary>

```
V = 1000

Matriz de adyacencia:
  Siempre: V × V × 4 bytes = 1000 × 1000 × 4 = 4,000,000 bytes = 3.8 MB
  Independiente de E.

Lista de adyacencia:
  V punteros: 1000 × 8 = 8,000 bytes
  E nodos (cada uno: int destino + puntero next = 12+4 padding = 16 bytes
           + 16 bytes malloc = 32 bytes)
  Total ≈ 8,000 + 32 × E bytes
```

| E | Matriz | Lista adj | ¿Cuál gana? |
|---|--------|-----------|------------|
| 1,000 (disperso) | 3.8 MB | 40 KB | **Lista** (95x menos) |
| 100,000 (moderado) | 3.8 MB | 3.2 MB | **Lista** (ligeramente) |
| 499,500 (completo) | 3.8 MB | 15.6 MB | **Matriz** (4x menos) |

El punto de cruce es aproximadamente $E \approx V^2/8 \approx 125{,}000$.

Para grafos dispersos ($E \ll V^2$), la lista ahorra memoria enormemente.
Para grafos densos ($E \approx V^2/2$), la matriz es mejor en espacio y también
en tiempo (verificar arista es $O(1)$ vs $O(\text{grado})$ en la lista).

</details>

---

### Ejercicio 8 — size_of en Rust

Ejecuta este programa y explica cada resultado:

```rust
use std::mem::size_of;

fn main() {
    println!("i32:               {}", size_of::<i32>());
    println!("i64:               {}", size_of::<i64>());
    println!("Box<i32>:          {}", size_of::<Box<i32>>());
    println!("Option<i32>:       {}", size_of::<Option<i32>>());
    println!("Option<Box<i32>>:  {}", size_of::<Option<Box<i32>>>());
    println!("Vec<i32>:          {}", size_of::<Vec<i32>>());
    println!("String:            {}", size_of::<String>());
    println!("&str:              {}", size_of::<&str>());
    println!("&[i32]:            {}", size_of::<&[i32]>());
}
```

**Predicción**: ¿Cuántos bytes ocupa cada uno? ¿Por qué `Option<Box<i32>>`
ocupa lo mismo que `Box<i32>`?

<details><summary>Respuesta</summary>

```
i32:               4      ← 4 bytes, directo
i64:               8      ← 8 bytes, directo
Box<i32>:          8      ← puntero al heap (64-bit)
Option<i32>:       8      ← 4 bytes dato + 4 bytes discriminante
Option<Box<i32>>:  8      ← MISMO tamaño que Box<i32> (niche optimization)
Vec<i32>:          24     ← ptr(8) + len(8) + cap(8)
String:            24     ← igual que Vec<u8> (es un Vec<u8> internamente)
&str:              16     ← ptr(8) + len(8) (fat pointer / slice)
&[i32]:            16     ← ptr(8) + len(8) (fat pointer / slice)
```

**Niche optimization**: `Option<Box<i32>>` ocupa 8 bytes (no 16) porque
`Box<T>` nunca es null (0x0 no es un valor válido para `Box`). Rust usa el
valor 0x0 para representar `None`, eliminando la necesidad de un byte extra
de discriminante. Esta optimización aplica a `Box`, `&T`, `&mut T`,
`NonNull<T>`, y otros tipos que no pueden ser cero.

`Vec<i32>` y `String` son 24 bytes en el stack pero los datos reales
están en el heap. `size_of` solo mide la parte en stack.

`&str` y `&[i32]` son "fat pointers" — contienen puntero + longitud.
Un `&i32` regular es solo 8 bytes (thin pointer).

</details>

---

### Ejercicio 9 — Espacio de una tabla hash

Una tabla hash con chaining (listas enlazadas por bucket) tiene:
- $m = 1024$ buckets
- $n = 750$ elementos (load factor $\alpha = 0.73$)
- Cada elemento: struct con 100 bytes de datos

Calcula la memoria total usada (incluyendo overhead de malloc, punteros
de lista, y el array de buckets).

**Predicción**: ¿Qué porcentaje de la memoria es "overhead" (no datos útiles)?

<details><summary>Respuesta</summary>

```
Array de buckets:
  1024 × 8 bytes (punteros) = 8,192 bytes

Nodos (cada uno con dato + puntero next):
  struct Node { char data[100]; struct Node *next; };
  sizeof: 100 + 8 = 108 bytes → padded a 112 bytes
  + malloc metadata: 16 bytes
  → 128 bytes por nodo
  750 nodos × 128 = 96,000 bytes

Total: 8,192 + 96,000 = 104,192 bytes ≈ 101.75 KB
```

```
Datos útiles: 750 × 100 = 75,000 bytes = 73.2 KB
Overhead:     104,192 - 75,000 = 29,192 bytes = 28.5 KB

Porcentaje overhead: 29,192 / 104,192 = 28%
```

Desglose del overhead:
- Array de buckets: 8,192 bytes (8%)
- Punteros next: 750 × 8 = 6,000 bytes (6%)
- Padding: 750 × 4 = 3,000 bytes (3%)
- Malloc metadata: 750 × 16 = 12,000 bytes (12%)

El mayor overhead es el **malloc metadata** — tener 750 alocaciones
separadas es costoso. Una alternativa es un pool allocator (pre-alocar
un bloque grande y subdividirlo → sin metadata por nodo).

Con open addressing (sin listas), el overhead sería solo el array:
$1024 \times 112$ bytes = 114,688 bytes, con 274 slots vacíos $\times$ 112 = 30,688
bytes desperdiciados. Similar overhead total pero sin malloc per-node.

</details>

---

### Ejercicio 10 — Elegir por restricción de memoria

Un sistema embebido tiene solo **64 KB de RAM** disponible para datos.
Necesitas almacenar hasta 5000 registros de 8 bytes cada uno y soportar
búsqueda por clave.

Opciones:
- A) Hash table con chaining
- B) Array ordenado + búsqueda binaria
- C) BST balanceado (AVL)

Calcula la memoria necesaria de cada opción y determina cuál cabe en 64 KB.

**Predicción**: ¿Cuál de las tres cabe? ¿Cuál es la más eficiente en
memoria?

<details><summary>Respuesta</summary>

$5000 \times 8$ bytes = 40,000 bytes de datos útiles = 39 KB.

**A) Hash table con chaining**:
```
Array de buckets (~7000 para α ≈ 0.7): 7000 × 8 = 56,000 bytes
Nodos: 5000 × (8 datos + 8 next + 16 malloc) = 160,000 bytes
Total: ~216,000 bytes = 211 KB → NO CABE
```

**B) Array ordenado**:
```
Array: 5000 × 8 = 40,000 bytes
Variables auxiliares: ~20 bytes
Total: ~40,020 bytes = 39.1 KB → CABE ✓
```

**C) AVL tree**:
```
Nodos: 5000 × (8 datos + 8 left + 8 right + 4 height + 16 malloc)
     = 5000 × 44 = 220,000 bytes = 215 KB → NO CABE
```

**Solo el array ordenado cabe** en 64 KB.

| Opción | Memoria | ¿Cabe en 64 KB? | Búsqueda |
|--------|---------|-----------------|----------|
| Hash table | ~211 KB | No | $O(1)$ promedio |
| Array ordenado | ~39 KB | **Sí** | $O(\log n) = 13$ |
| AVL tree | ~215 KB | No | $O(\log n) = 13$ |

El array ordenado es 5x más eficiente en memoria que las alternativas.
El costo: inserción $O(n)$ en vez de $O(1)$/$O(\log n)$. Para un sistema
embebido con pocas inserciones (configuración), esto es aceptable.

Si se necesita inserción eficiente en 64 KB: hash table con open
addressing (sin punteros, sin malloc por nodo):
```
Array: 7000 × 8 = 56,000 bytes = 54.7 KB → CABE (ajustado)
```

</details>
