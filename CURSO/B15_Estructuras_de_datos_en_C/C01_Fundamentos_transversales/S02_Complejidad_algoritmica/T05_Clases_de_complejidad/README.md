# T05 — Clases de complejidad comunes

---

## 1. Las siete clases fundamentales

Casi todo algoritmo y operación de estructura de datos cae en una de estas
clases. Conocerlas es como conocer las notas musicales — con ellas se
construye todo lo demás.

```
Más rápido                                           Más lento
    │                                                    │
    ▼                                                    ▼
  O(1)  O(log n)  O(n)  O(n log n)  O(n²)  O(2ⁿ)  O(n!)
```

---

## 2. O(1) — Constante

### Qué significa

El tiempo no depende del tamaño de la entrada. Sea $n = 10$ o $n = 10^9$,
la operación tarda lo mismo.

### Ejemplos

```c
// Acceso por índice en array
int get(int *arr, int i) {
    return arr[i];          // un cálculo: base + i * sizeof(int)
}

// Push en stack (sin realloc)
void stack_push(Stack *s, int elem) {
    s->data[s->size++] = elem;
}

// Lookup en hash table (caso promedio)
void *hashtable_get(HashTable *ht, const char *key) {
    int idx = hash(key) % ht->capacity;
    // ... buscar en bucket (longitud ~1 con buen hash)
}
```

### En estructuras de datos

| Operación | Estructura |
|-----------|-----------|
| Acceso por índice | Array, Vec |
| Push/pop | Stack (array-based) |
| Lookup | Hash table (promedio) |
| Get min/max | Heap (solo el extremo) |
| Insert/delete head | Lista enlazada |

### Identificación en código

```c
// O(1): sin loops que dependan de n, sin recursión
int f(int n) {
    int a = n * 2;       // operación aritmética
    int b = a + 100;     // otra
    return b % 7;        // otra
}
// 3 operaciones siempre, sin importar n
```

### Cuidado: O(1) ≠ instantáneo

$O(1)$ significa constante, no rápido. Una función que hace $10^6$ operaciones
fijas es $O(1)$ — pero tarda. Lo que $O(1)$ garantiza es que **no crece** con $n$.

---

## 3. O(log n) — Logarítmico

### Qué significa

El tiempo crece proporcional al logaritmo de $n$. Cada vez que $n$ se duplica,
el trabajo aumenta en solo una unidad (una comparación, una iteración).

```
n = 1,000:        log₂(n) ≈ 10
n = 1,000,000:    log₂(n) ≈ 20
n = 1,000,000,000: log₂(n) ≈ 30

Triplicar la cantidad de dígitos solo agrega 20 operaciones.
```

### El patrón: dividir por 2

Casi todo $O(\log n)$ involucra **dividir el problema a la mitad** en cada paso:

```c
// Búsqueda binaria
int binary_search(int *arr, int n, int target) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) lo = mid + 1;
        else                   hi = mid - 1;
    }
    return -1;
}
// Cada iteración elimina la mitad del espacio de búsqueda
// n → n/2 → n/4 → ... → 1  →  log₂(n) iteraciones
```

### En estructuras de datos

| Operación | Estructura |
|-----------|-----------|
| Búsqueda binaria | Array ordenado |
| Búsqueda/inserción/eliminación | BST balanceado (AVL, RB) |
| Insert/extract | Heap binario |
| Búsqueda | B-tree (por nivel) |

### Identificación en código

```c
// Patrón 1: variable que se divide
while (x > 0) { x /= 2; }        // O(log n)

// Patrón 2: variable que se multiplica hasta n
for (int i = 1; i < n; i *= 2)    // O(log n)

// Patrón 3: recursión que divide
f(n/2)                             // T(n) = T(n/2) + O(1) → O(log n)
```

### Intuición: dígitos

$\log_2(n)$ es aproximadamente el número de bits necesarios para representar $n$.
$\log_{10}(n)$ es el número de dígitos decimales. Operar sobre los "dígitos" de $n$
(no sobre $n$ mismo) es $O(\log n)$.

---

## 4. O(n) — Lineal

### Qué significa

El tiempo crece proporcionalmente a la entrada. Duplicar $n$ duplica el tiempo.

### Ejemplos

```c
// Encontrar el máximo
int max(int *arr, int n) {
    int m = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > m) m = arr[i];
    return m;
}
// Exactamente n-1 comparaciones — siempre

// Sumar todos los elementos
long sum(int *arr, int n) {
    long total = 0;
    for (int i = 0; i < n; i++)
        total += arr[i];
    return total;
}
```

### En estructuras de datos

| Operación | Estructura |
|-----------|-----------|
| Búsqueda secuencial | Array desordenado, lista enlazada |
| Insertar/eliminar en posición | Array (shift) |
| Recorrer todos los elementos | Cualquier estructura |
| Construir | Array, lista, hash table |

### Por qué O(n) es a menudo óptimo

Para muchos problemas, $O(n)$ es la mejor complejidad posible porque
**necesitas mirar todos los elementos al menos una vez**:

```
Encontrar el mínimo: Ω(n)
  → No puedes saber que X es el mínimo sin verificar los demás
  → Necesitas n-1 comparaciones como mínimo

Verificar si un array está ordenado: Ω(n)
  → Necesitas comparar pares consecutivos: n-1 comparaciones

Sumar n números: Ω(n)
  → Cada número contribuye al resultado — hay que verlos todos
```

### Identificación en código

```c
// Un loop de 0 a n
for (int i = 0; i < n; i++) { /* O(1) por iteración */ }

// Recorrer una lista enlazada
Node *cur = head;
while (cur) { /* ... */ cur = cur->next; }

// Recursión lineal
void f(int n) { if (n > 0) f(n - 1); }   // n llamadas
```

---

## 5. O(n log n) — Linearítmico

### Qué significa

Un poco más que lineal. Para $n = 10^6$, es $\approx$20 millones de operaciones
($n \times 20$) en vez de 1 millón ($n$). En la práctica, se siente casi lineal.

### Por qué es tan importante

$\Omega(n \log n)$ **es el límite inferior teórico para ordenamiento por
comparaciones.** No se puede ordenar $n$ elementos con menos de $n \log n$
comparaciones (demostrado por el argumento del árbol de decisión).

Esto hace que $O(n \log n)$ sea la frontera entre "eficiente" e "ineficiente"
para ordenamiento.

### Ejemplos

```c
// Merge sort: dividir + merge
void merge_sort(int *arr, int n) {
    if (n <= 1) return;
    merge_sort(arr, n/2);           // T(n/2)
    merge_sort(arr + n/2, n - n/2); // T(n/2)
    merge(arr, n);                   // O(n)
}
// T(n) = 2T(n/2) + O(n) → O(n log n)
```

```
Árbol de recursión de merge sort (n = 8):

Nivel 0:  [8 elementos]           → merge O(8)
          /            \
Nivel 1:  [4]          [4]        → merge O(4) + O(4) = O(8)
         /   \        /   \
Nivel 2:  [2] [2]    [2] [2]     → O(2)×4 = O(8)
         / \ / \    / \ / \
Nivel 3: [1][1][1][1][1][1][1][1] → O(0) base

log₂(8) = 3 niveles, cada nivel hace O(n) trabajo total
→ O(n log n)
```

### En estructuras de datos

| Operación | Contexto |
|-----------|---------|
| Ordenar | Merge sort, quicksort (promedio), heapsort |
| Construir BST balanceado | Desde array desordenado |
| Ordenar + buscar | Pre-procesar para múltiples búsquedas binarias |

### Identificación en código

```c
// Patrón: loop lineal dentro de loop logarítmico
for (int i = 1; i < n; i *= 2)     // log n veces
    for (int j = 0; j < n; j++)     // n veces
        /* ... */

// Patrón: recursión divide-and-conquer con merge O(n)
// T(n) = 2T(n/2) + O(n)

// Patrón: n operaciones O(log n) cada una
for (int i = 0; i < n; i++)
    bst_insert(tree, arr[i]);   // O(log n) × n = O(n log n)
```

---

## 6. O(n²) — Cuadrático

### Qué significa

Duplicar $n$ cuadruplica el tiempo. Para $n = 10^4$, son $\approx 10^8$ operaciones
($\approx$0.1 segundos). Para $n = 10^5$, son $\approx 10^{10}$ ($\approx$minutos).

### Ejemplos

```c
// Bubble sort
void bubble_sort(int *arr, int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (arr[j] > arr[j + 1])
                swap(&arr[j], &arr[j + 1]);
}
// ~n²/2 comparaciones

// Encontrar todos los pares
void all_pairs(int *arr, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            process(arr[i], arr[j]);
}
// n(n-1)/2 pares → O(n²)
```

### En estructuras de datos

| Operación | Contexto |
|-----------|---------|
| Insertion/bubble/selection sort | $O(n^2)$ peor caso |
| Insertar n elementos en array ordenado | $n$ inserts $\times$ $O(n)$ shift cada uno |
| Búsqueda de duplicados (fuerza bruta) | Comparar todos los pares |

### Cuándo O(n²) es aceptable

```
n ≤ 50:     Siempre aceptable. Insertion sort es más rápido que
            merge sort para arrays pequeños (mejores constantes).

n ≤ 10³:    Generalmente aceptable (~10⁶ ops, <1ms).

n ≤ 10⁴:    Límite. ~10⁸ ops, ~0.1-1 segundo.

n > 10⁴:    Generalmente inaceptable. Buscar alternativa O(n log n) o O(n).
```

### Identificación en código

```c
// Dos loops anidados sobre n
for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
        /* O(1) */

// Loop con loop dependiente (sigue siendo cuadrático)
for (int i = 0; i < n; i++)
    for (int j = 0; j < i; j++)
        /* O(1) */
// 0 + 1 + 2 + ... + (n-1) = n(n-1)/2 → O(n²)
```

---

## 7. O(2ⁿ) — Exponencial

### Qué significa

Agregar un elemento a la entrada **duplica** el tiempo. Crece tan rápido
que solo es practicable para $n$ muy pequeños.

```
n = 10:   2¹⁰ = 1,024
n = 20:   2²⁰ = 1,048,576       (~10⁶)
n = 30:   2³⁰ = 1,073,741,824   (~10⁹, ~1 segundo)
n = 40:   2⁴⁰ ≈ 10¹²            (~17 minutos)
n = 50:   2⁵⁰ ≈ 10¹⁵            (~31 años)
n = 100:  2¹⁰⁰ ≈ 10³⁰           (más átomos que en el universo)
```

### Ejemplos

```c
// Fibonacci ingenuo
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
// T(n) = T(n-1) + T(n-2) → O(φⁿ) ≈ O(1.618ⁿ) ⊂ O(2ⁿ)

// Todos los subconjuntos de un conjunto
void subsets(int *arr, int n) {
    for (int mask = 0; mask < (1 << n); mask++) {
        for (int i = 0; i < n; i++)
            if (mask & (1 << i))
                printf("%d ", arr[i]);
        printf("\n");
    }
}
// 2ⁿ subconjuntos — uno por cada combinación de incluir/excluir
```

### En estructuras de datos

$O(2^n)$ no aparece en operaciones de estructuras de datos, sino en
**algoritmos que exploran todas las combinaciones**: backtracking,
programación dinámica sin memoización, fuerza bruta sobre subconjuntos.

### Cuándo aparece

| Problema | Complejidad | n máximo viable |
|----------|------------|-----------------|
| Fibonacci ingenuo | $O(1.618^n)$ | ~45 |
| Todos los subconjuntos | $O(2^n)$ | ~25 |
| Mochila (fuerza bruta) | $O(2^n)$ | ~25 |
| SAT (fuerza bruta) | $O(2^n)$ | ~25 |
| Torres de Hanoi | $O(2^n)$ | ~25 (en tiempo de cómputo) |

### Identificación en código

```c
// Recursión que se bifurca en 2 con profundidad n
void f(int n) {
    if (n <= 0) return;
    f(n - 1);
    f(n - 1);
}
// 2ⁿ llamadas

// Iteración sobre subconjuntos
for (int mask = 0; mask < (1 << n); mask++) { ... }
// 2ⁿ iteraciones
```

---

## 8. O(n!) — Factorial

### Qué significa

El crecimiento más extremo de los comunes. Solo practicable para $n \leq$ ~12.

```
n = 5:    120
n = 10:   3,628,800       (~3.6 × 10⁶)
n = 12:   479,001,600     (~4.8 × 10⁸, ~0.5 segundos)
n = 13:   6,227,020,800   (~6 × 10⁹, ~6 segundos)
n = 15:   1,307,674,368,000 (~1.3 × 10¹², ~22 minutos)
n = 20:   2.4 × 10¹⁸       (~76 años)
```

### Ejemplos

```c
// Generar todas las permutaciones
void permute(int *arr, int n, int depth) {
    if (depth == n) {
        print_array(arr, n);
        return;
    }
    for (int i = depth; i < n; i++) {
        swap(&arr[depth], &arr[i]);
        permute(arr, n, depth + 1);
        swap(&arr[depth], &arr[i]);
    }
}
// n! permutaciones
```

### Cuándo aparece

| Problema | Complejidad | n máximo |
|----------|------------|---------|
| Todas las permutaciones | $O(n!)$ | ~12 |
| Travelling Salesman (fuerza bruta) | $O(n!)$ | ~12 |
| Determinante por expansión de cofactores | $O(n!)$ | ~12 |

### n! vs 2ⁿ

```
n = 10:  2¹⁰ = 1,024      vs   10! = 3,628,800
n = 20:  2²⁰ ≈ 10⁶        vs   20! ≈ 2.4 × 10¹⁸

n! crece MUCHO más rápido que 2ⁿ para n grande.
```

---

## 9. Tabla resumen con ejemplos concretos

| Clase | Nombre | Ejemplo clásico | $n=10^6$ ops | Viable hasta |
|-------|--------|-----------------|-----------|-------------|
| $O(1)$ | Constante | Hash lookup | 1 | Cualquier $n$ |
| $O(\log n)$ | Logarítmico | Búsqueda binaria | 20 | Cualquier $n$ |
| $O(n)$ | Lineal | Búsqueda secuencial | $10^6$ | $\approx 10^8$ |
| $O(n \log n)$ | Linearítmico | Merge sort | $2 \times 10^7$ | $\approx 10^6 - 10^7$ |
| $O(n^2)$ | Cuadrático | Insertion sort | $10^{12}$ | $\approx 10^4$ |
| $O(2^n)$ | Exponencial | Subconjuntos | $10^{301029}$ | ~25 |
| $O(n!)$ | Factorial | Permutaciones | $\approx \infty$ | ~12 |

### Gráfica mental de crecimiento

```
Operaciones
    │
10¹²│                                               n!
    │                                          ╱
    │                                    2ⁿ ╱
    │                                  ╱  ╱
    │                               ╱  ╱
10⁶ │                        n²  ╱  ╱
    │                      ╱   ╱  ╱
    │                   ╱    ╱  ╱
    │             n·logn   ╱  ╱
    │            ╱   ╱   ╱  ╱
10³ │         n ╱  ╱   ╱  ╱
    │       ╱ ╱  ╱   ╱
    │     ╱╱  ╱    ╱
    │  logn ╱    ╱
 1  │ 1───╱───╱──────────────────────────── n
    └─────┬────┬─────┬──────┬──────┬────▶
         10   10²   10³    10⁴   10⁵
```

### Regla del factor al duplicar n

| Clase | $T(2n)/T(n)$ | Interpretación |
|-------|-----------|----------------|
| $O(1)$ | 1 | No cambia |
| $O(\log n)$ | $\approx 1$ | Casi no cambia (+1 operación) |
| $O(n)$ | 2 | Se duplica |
| $O(n \log n)$ | ~2.x | Se duplica y un poco más |
| $O(n^2)$ | 4 | Se cuadruplica |
| $O(n^3)$ | 8 | Se octuplica |
| $O(2^n)$ | $2^n$ | Se eleva al cuadrado |
| $O(n!)$ | inmenso | Impracticable |

---

## 10. Casos intermedios y menos comunes

Además de las siete fundamentales, existen clases intermedias:

| Clase | Dónde aparece |
|-------|-------------|
| $O(\sqrt{n})$ | Algoritmos de búsqueda en bloques (jump search), factorización trial division |
| $O(n\sqrt{n})$ | Algunos algoritmos de geometría computacional |
| $O(n^2 \cdot \log n)$ | Algunos algoritmos de grafos con optimizaciones parciales |
| $O(n \cdot k)$ | Algoritmos parametrizados ($k$ = dimensión, profundidad, etc.) |
| $O(V + E)$ | Recorridos de grafos (BFS, DFS) — lineal en el tamaño del grafo |
| $O(V^2)$ vs $O((V+E) \cdot \log V)$ | Dijkstra con matriz vs con heap |

### O(V + E) para grafos

Los algoritmos de grafos suelen expresarse en función de vértices ($V$) y
aristas ($E$) en vez de un solo $n$:

```
BFS/DFS: O(V + E)
  → Visita cada vértice una vez: O(V)
  → Examina cada arista una vez: O(E)
  → Total: O(V + E)

Para grafos dispersos (E ≈ V):     O(V)
Para grafos densos (E ≈ V²):       O(V²)
```

---

## Ejercicios

### Ejercicio 1 — Clasificar algoritmos

Clasifica cada algoritmo en su clase de complejidad temporal:

```
a) Acceder a arr[42]
b) Buscar en array ordenado de 10⁶ elementos
c) Ordenar 10⁶ elementos con merge sort
d) Buscar duplicados con doble loop
e) Generar todas las permutaciones de "ABCDE"
f) Buscar en hash table
g) Fibonacci recursivo sin memo para n=30
h) Recorrer lista enlazada de n nodos
```

**Predicción**: Clasifica los 8 antes de ver la respuesta.

<details><summary>Respuesta</summary>

| Algoritmo | Clase |
|-----------|-------|
| a) Acceso por índice | $O(1)$ |
| b) Búsqueda binaria | $O(\log n)$ |
| c) Merge sort | $O(n \log n)$ |
| d) Duplicados doble loop | $O(n^2)$ |
| e) Permutaciones de 5 elementos | $O(n!)$ = $O(5!)$ = 120 |
| f) Hash lookup | $O(1)$ promedio |
| g) Fibonacci recursivo | $O(2^n)$ $\approx$ $O(1.618^{30})$ $\approx$ $10^6$ |
| h) Recorrer lista | $O(n)$ |

Nota sobre g): técnicamente $O(\varphi^n)$ donde $\varphi \approx 1.618$ (ratio áureo), pero se
clasifica como exponencial. Para $n=30$: $\approx 10^6$ llamadas, que es manejable.
Para $n=50$: $\approx 10^{10}$, que ya no lo es.

</details>

---

### Ejercicio 2 — Tiempo estimado

Para cada clase, calcula cuánto tardaría procesar $n = 10^6$ elementos
asumiendo $10^9$ operaciones por segundo:

```
a) O(1)
b) O(log n)
c) O(n)
d) O(n log n)
e) O(n²)
f) O(2ⁿ)
```

**Predicción**: ¿Cuáles terminan en menos de 1 segundo? ¿Cuáles son
impracticables?

<details><summary>Respuesta</summary>

| Clase | Operaciones para $n=10^6$ | Tiempo ($10^9$ ops/s) |
|-------|----------------------|---------------------|
| $O(1)$ | 1 | **1 ns** |
| $O(\log n)$ | 20 | **20 ns** |
| $O(n)$ | $10^6$ | **1 ms** |
| $O(n \log n)$ | $2 \times 10^7$ | **20 ms** |
| $O(n^2)$ | $10^{12}$ | **17 minutos** |
| $O(2^n)$ | $2^{10^6}$ | **imposible** (más que la edad del universo) |

Terminan en < 1 segundo: $O(1)$, $O(\log n)$, $O(n)$, $O(n \log n)$.
Lento pero factible: $O(n^2)$ (17 minutos).
Impracticable: $O(2^n)$, $O(n!)$.

$O(n^2)$ es la frontera: para $n = 10^6$ ya es lento. Para $n = 10^4$ es
aceptable (~0.1 ms). La regla práctica: si $n > 10^4$, necesitas mejor
que $O(n^2)$.

</details>

---

### Ejercicio 3 — Identificar por el patrón de código

¿Cuál es la complejidad de cada fragmento? Identifica el patrón.

```c
// A
for (int i = n; i > 0; i /= 3)
    sum++;

// B
for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
        for (int k = 0; k < n; k++)
            sum++;

// C
for (int i = 0; i < n; i++)
    sum++;
for (int j = 0; j < n; j++)
    sum++;

// D
for (int i = 1; i <= n; i *= 2)
    for (int j = 0; j < n; j++)
        sum++;

// E
for (int i = 0; i < n; i++)
    for (int j = 1; j < n; j *= 2)
        sum++;
```

**Predicción**: ¿D y E dan la misma complejidad aunque los loops estén
invertidos?

<details><summary>Respuesta</summary>

| Fragmento | Patrón | Complejidad |
|-----------|--------|------------|
| A | Dividir por 3 | $O(\log_3 n) = O(\log n)$ |
| B | Tres loops anidados | $O(n^3)$ |
| C | Dos loops secuenciales | $O(n) + O(n) = O(n)$ |
| D | Log externo $\times$ lineal interno | $O(n \log n)$ |
| E | Lineal externo $\times$ log interno | $O(n \log n)$ |

Sí, D y E dan **la misma complejidad** $O(n \log n)$, aunque los loops están
invertidos.

D: $\log_2(n)$ iteraciones del externo $\times$ $n$ del interno = $n \log n$.
E: $n$ iteraciones del externo $\times$ $\log_2(n)$ del interno = $n \log n$.

La multiplicación es conmutativa: $(\log n) \times n = n \times (\log n)$.

Lo que importa para la complejidad no es **cuál loop es el externo**,
sino **cuántas iteraciones totales** se ejecutan.

</details>

---

### Ejercicio 4 — n máximo viable

Para cada complejidad, calcula el $n$ máximo que se puede procesar en
1 segundo ($10^9$ operaciones):

```
a) O(n)
b) O(n log n)
c) O(n²)
d) O(n³)
e) O(2ⁿ)
f) O(n!)
```

**Predicción**: ¿Para $O(n!)$ se puede llegar a $n = 20$?

<details><summary>Respuesta</summary>

Para encontrar $n$ máximo, resolvemos $f(n) \leq 10^9$:

| Clase | Ecuación | $n$ máximo |
|-------|---------|---------|
| $O(n)$ | $n \leq 10^9$ | $\approx 10^9$ |
| $O(n \log n)$ | $n \cdot \log_2(n) \leq 10^9$ | $\approx 3.6 \times 10^7$ |
| $O(n^2)$ | $n^2 \leq 10^9$ | $\approx 31{,}623$ ($\sqrt{10^9}$) |
| $O(n^3)$ | $n^3 \leq 10^9$ | $\approx 1{,}000$ ($\sqrt[3]{10^9}$) |
| $O(2^n)$ | $2^n \leq 10^9$ | $\approx 30$ ($\log_2(10^9)$) |
| $O(n!)$ | $n! \leq 10^9$ | $\approx 12$ ($12! \approx 4.8 \times 10^8$) |

No, para $O(n!)$ no se puede llegar a $n = 20$ en 1 segundo:
$20! = 2.4 \times 10^{18}$ operaciones → ~76 años.

Tabla útil para entrevistas y competencias de programación:

```
Si n ≤ 12:        O(n!) es viable
Si n ≤ 25:        O(2ⁿ) es viable
Si n ≤ 500:       O(n³) es viable
Si n ≤ 30,000:    O(n²) es viable
Si n ≤ 10⁶-10⁷:  O(n log n) es viable
Si n ≤ 10⁸-10⁹:  O(n) es viable
```

</details>

---

### Ejercicio 5 — Verificación experimental de la clase

Escribe un programa que ejecute una operación $O(n^2)$ y mida $T(n)$ para
$n = 1000, 2000, 4000, 8000$. Calcula $T(2n)/T(n)$ para confirmar que
la razón es $\approx$4.

```c
volatile int sink;
void quadratic(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            sink = i + j;
}
```

Repite para una operación $O(n \log n)$:

```c
void nlogn(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 1; j < n; j *= 2)
            sink = i + j;
}
```

**Predicción**: ¿La razón $T(2n)/T(n)$ de nlogn será exactamente 2 o un
poco más?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <time.h>

volatile int sink;

void quadratic(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            sink = i + j;
}

void nlogn(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 1; j < n; j *= 2)
            sink = i + j;
}

double measure(void (*fn)(int), int n) {
    clock_t start = clock();
    fn(n);
    return 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
}

int main(void) {
    int sizes[] = {1000, 2000, 4000, 8000};
    printf("%-8s  %-12s ratio  %-12s ratio\n", "n", "O(n²) ms", "O(nlogn) ms");
    double prev_q = 0, prev_nl = 0;
    for (int s = 0; s < 4; s++) {
        double tq = measure(quadratic, sizes[s]);
        double tnl = measure(nlogn, sizes[s]);
        printf("%-8d  %-12.2f %-5.1f  %-12.2f %.1f\n",
               sizes[s], tq, prev_q > 0 ? tq/prev_q : 0,
               tnl, prev_nl > 0 ? tnl/prev_nl : 0);
        prev_q = tq;
        prev_nl = tnl;
    }
    return 0;
}
```

Resultados esperados:

```
n         O(n²) ms     ratio  O(nlogn) ms  ratio
1000      2.00         0.0    0.10         0.0
2000      8.00         4.0    0.23         2.3
4000      32.00        4.0    0.50         2.2
8000      128.00       4.0    1.10         2.2
```

- $O(n^2)$: ratio $\approx$ **4.0** (se cuadruplica)
- $O(n \log n)$: ratio $\approx$ **2.0-2.3** (un poco más que duplicar)

El ratio de $O(n \log n)$ no es exactamente 2 porque:
```
T(2n)/T(n) = (2n · log(2n)) / (n · log(n))
           = 2 · (log(n) + 1) / log(n)
           = 2 · (1 + 1/log(n))
           ≈ 2.1 para n = 1000-8000
```

A medida que $n$ crece, el ratio tiende a 2 (el +1 se vuelve despreciable).

</details>

Limpieza:

```bash
rm -f class_bench
```

---

### Ejercicio 6 — De O(n²) a O(n log n)

El problema: dado un array de n enteros, encontrar el par con la
menor diferencia absoluta.

Versión $O(n^2)$:

```c
int min_diff_brute(int *arr, int n) {
    int min = INT_MAX;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            int d = abs(arr[i] - arr[j]);
            if (d < min) min = d;
        }
    return min;
}
```

Escribe una versión $O(n \log n)$. Pista: los elementos con menor diferencia
están adyacentes en el array ordenado.

**Predicción**: ¿Cuál es el paso clave que permite bajar de $O(n^2)$ a $O(n \log n)$?

<details><summary>Respuesta</summary>

```c
int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int min_diff_sort(int *arr, int n) {
    qsort(arr, n, sizeof(int), cmp_int);   // O(n log n)

    int min = INT_MAX;
    for (int i = 0; i < n - 1; i++) {       // O(n)
        int d = arr[i + 1] - arr[i];        // ya están ordenados → no abs
        if (d < min) min = d;
    }
    return min;                              // Total: O(n log n) + O(n) = O(n log n)
}
```

El paso clave: **ordenar**. Una vez ordenado, los candidatos a menor
diferencia son pares adyacentes (la diferencia entre arr[i] y arr[i+1]
siempre es $\leq$ la diferencia entre arr[i] y arr[i+k] para $k > 1$).

Esto reduce la búsqueda de $n(n-1)/2$ pares a $n-1$ pares.

```
Fuerza bruta: comparar todos los C(n,2) pares → O(n²)
Con sort:     sort O(n log n) + scan O(n)     → O(n log n)

Para n = 10⁶:
  Brute: ~5 × 10¹¹ comparaciones → ~8 minutos
  Sort:  ~2 × 10⁷ comparaciones → ~20 ms
  Speedup: ~25,000x
```

</details>

---

### Ejercicio 7 — Reconocer la clase por el crecimiento

Te dan estos tiempos de ejecución medidos. ¿A qué clase pertenece
cada algoritmo?

```
Algoritmo X:     n=1000 → 5ms,   n=2000 → 10ms,   n=4000 → 20ms
Algoritmo Y:     n=1000 → 3ms,   n=2000 → 12ms,   n=4000 → 48ms
Algoritmo Z:     n=1000 → 1ms,   n=2000 → 1ms,    n=4000 → 1ms
Algoritmo W:     n=1000 → 2ms,   n=2000 → 4.4ms,  n=4000 → 9.6ms
```

**Predicción**: Calcula $T(2n)/T(n)$ para cada uno y clasifica.

<details><summary>Respuesta</summary>

| Algoritmo | $T(2n)/T(n)$ | $T(4n)/T(n)$ | Clase |
|-----------|-----------|-----------|-------|
| X | 10/5 = **2** | 20/5 = 4 | $O(n)$ — se duplica |
| Y | 12/3 = **4** | 48/3 = 16 | $O(n^2)$ — se cuadruplica |
| Z | 1/1 = **1** | 1/1 = 1 | $O(1)$ o $O(\log n)$ — no cambia |
| W | 4.4/2 = **2.2** | 9.6/2 = 4.8 | $O(n \log n)$ — un poco más que duplicar |

Verificación de W:
```
Si T(n) = c · n · log₂(n):
  T(2000)/T(1000) = (2000 · log 2000) / (1000 · log 1000)
                   = 2 · 11/10 = 2.2  ✓

  T(4000)/T(1000) = (4000 · log 4000) / (1000 · log 1000)
                   = 4 · 12/10 = 4.8  ✓
```

Z podría ser $O(1)$ (constante) o $O(\log n)$ (el cambio de log es tan pequeño
que no se nota en estos rangos). Para distinguirlos: medir $n = 10^6$ vs $n = 10^9$.
Si sigue igual → $O(1)$. Si crece de 1ms a 1.5ms → $O(\log n)$.

</details>

---

### Ejercicio 8 — Mapa de complejidades de la biblioteca estándar

Completa la complejidad de estas operaciones de Rust:

```rust
// Vec<T>
v.push(x)           // a)
v.pop()              // b)
v.insert(0, x)       // c)
v.contains(&x)       // d)
v.sort()             // e)
v.binary_search(&x)  // f)

// HashMap<K, V>
m.insert(k, v)       // g)
m.get(&k)            // h)
m.contains_key(&k)   // i)
m.remove(&k)         // j)

// BTreeMap<K, V>
bt.insert(k, v)      // k)
bt.get(&k)           // l)
bt.range(a..b)       // m)
```

**Predicción**: ¿Cuáles son $O(1)$? ¿Cuáles $O(\log n)$? ¿Hay alguna $O(n)$?

<details><summary>Respuesta</summary>

| Operación | Complejidad | Notas |
|-----------|------------|-------|
| a) `push` | $O(1)$ amortizado | $O(n)$ peor caso (realloc) |
| b) `pop` | $O(1)$ | Siempre |
| c) `insert(0, x)` | $O(n)$ | Shift de todos los elementos |
| d) `contains` | $O(n)$ | Búsqueda lineal |
| e) `sort` | $O(n \log n)$ | Timsort (estable) |
| f) `binary_search` | $O(\log n)$ | Requiere vec ordenado |
| g) `HashMap::insert` | $O(1)$ amortizado | $O(n)$ peor caso (rehash) |
| h) `HashMap::get` | $O(1)$ promedio | $O(n)$ peor caso (colisiones) |
| i) `contains_key` | $O(1)$ promedio | Igual que get |
| j) `HashMap::remove` | $O(1)$ promedio | Igual que get |
| k) `BTreeMap::insert` | $O(\log n)$ | B-tree balanceado |
| l) `BTreeMap::get` | $O(\log n)$ | B-tree balanceado |
| m) `BTreeMap::range` | $O(\log n + k)$ | $\log n$ para encontrar inicio, $k$ para iterar resultados |

Patrón: `Vec` es $O(1)$ por los extremos, $O(n)$ por el medio o búsqueda.
`HashMap` es $O(1)$ para todo (amortizado/promedio). `BTreeMap` es $O(\log n)$
para todo pero soporta orden y rangos.

</details>

---

### Ejercicio 9 — Reducir complejidad con la estructura correcta

Para cada problema, indica qué estructura de datos cambiaría la
complejidad del enfoque ingenuo:

```
a) Buscar un elemento → de O(n) a O(1)
b) Encontrar el mínimo → de O(n) a O(1)
c) Verificar si un elemento existe → de O(n) a O(1)
d) Insertar manteniendo orden → de O(n) a O(log n)
e) Buscar strings por prefijo → de O(n·m) a O(m)
```

**Predicción**: ¿Qué estructura resuelve cada caso?

<details><summary>Respuesta</summary>

| Problema | Ingenuo | Estructura | Resultado |
|----------|---------|-----------|-----------|
| a) Buscar | $O(n)$ en array | **Hash table** | $O(1)$ promedio |
| b) Encontrar mínimo | $O(n)$ scan | **Min-heap** | $O(1)$ para ver min ($O(\log n)$ extract) |
| c) Verificar existencia | $O(n)$ en array | **HashSet** | $O(1)$ promedio |
| d) Insertar con orden | $O(n)$ shift en array | **BST balanceado (AVL/RB)** | $O(\log n)$ |
| e) Buscar por prefijo | $O(n \cdot m)$ comparar todas las strings | **Trie** | $O(m)$ donde $m$ = longitud del prefijo |

Cada estructura sacrifica algo para ganar en la operación crítica:
- Hash table: pierde orden, gana búsqueda $O(1)$
- Heap: pierde búsqueda general, gana acceso al extremo $O(1)$
- BST: pierde acceso por índice $O(1)$, gana inserción ordenada $O(\log n)$
- Trie: usa más memoria (punteros por carácter), gana búsqueda por prefijo

Elegir la estructura correcta puede cambiar un programa de inutilizable
a instantáneo.

</details>

---

### Ejercicio 10 — Clasificar un algoritmo desconocido

Un colega te muestra un algoritmo que no conoces. Mides estos tiempos:

```
n = 100:    0.003 ms
n = 200:    0.004 ms
n = 400:    0.005 ms
n = 800:    0.006 ms
n = 1600:   0.007 ms
n = 3200:   0.008 ms
```

¿A qué clase pertenece? Justifica con los ratios.

**Predicción**: ¿Es lineal? ¿Logarítmico? ¿Constante?

<details><summary>Respuesta</summary>

Ratios $T(2n)/T(n)$:

```
T(200)/T(100)   = 0.004/0.003 = 1.33
T(400)/T(200)   = 0.005/0.004 = 1.25
T(800)/T(400)   = 0.006/0.005 = 1.20
T(1600)/T(800)  = 0.007/0.006 = 1.17
T(3200)/T(1600) = 0.008/0.007 = 1.14
```

El ratio es ~1.1-1.3, no 1 (no es constante), no 2 (no es lineal).
El ratio **disminuye** a medida que $n$ crece y tiende a 1.

Esto es $O(\log n)$.

Verificación: si $T(n) = c \cdot \log_2(n)$:
```
T(200)/T(100) = log₂(200)/log₂(100) = 7.64/6.64 = 1.15
T(3200)/T(1600) = log₂(3200)/log₂(1600) = 11.64/10.64 = 1.09
```

Los valores medidos (1.33, 1.14) son ligeramente mayores que los teóricos
(1.15, 1.09) probablemente por overhead constante, pero el patrón
"ratio que decrece hacia 1" es la firma de $O(\log n)$.

Comparación de firmas:
```
O(1):       ratio = 1 siempre
O(log n):   ratio ~1.1-1.3, decrece hacia 1
O(n):       ratio = 2 siempre
O(n log n): ratio ~2.1-2.3, decrece hacia 2
O(n²):      ratio = 4 siempre
```

</details>
