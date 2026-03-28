# Búsqueda en árboles balanceados

## Objetivo

Analizar AVL y Red-Black trees como **estructuras de búsqueda con
garantía de peor caso**, enfocándose en su rendimiento de búsqueda
comparado con BST no balanceado y con arrays:

- $O(\log n)$ **garantizado** — eliminan la degeneración $O(n)$ del
  BST.
- **AVL**: más bajo ($\leq 1.44 \log_2 n$), búsquedas más rápidas.
- **Red-Black**: más alto ($\leq 2 \log_2 n$), inserciones/
  eliminaciones más baratas.
- Cuándo usar cada uno según el patrón de operaciones.
- Referencia: la implementación completa está en **C06/S04**
  (rotaciones, AVL, Red-Black, comparación).

---

## Por qué balancear: el costo de la degeneración

En T01 vimos que un BST no balanceado con $10^4$ elementos puede
tener altura 9999 (insertando en orden). La diferencia práctica:

| Métrica ($n = 10^6$) | BST degenerado | BST aleatorio | Balanceado |
|-------------------|-----------:|----------:|----------:|
| Altura | 999,999 | ~40 | ~20 |
| Comparaciones promedio | 500,000 | ~20 | ~20 |
| Peor búsqueda | 1,000,000 | ~40 | ~20 |
| Tiempo por búsqueda | ~500 µs | ~5 µs | ~3 µs |

Los árboles balanceados **garantizan** la columna derecha
independientemente del orden de inserción. No hay input adversarial
que pueda degradar el rendimiento.

---

## AVL: balance estricto

### Invariante

Para cada nodo, la diferencia de alturas entre los subárboles
izquierdo y derecho es $\leq 1$:

$$|\text{height}(\text{left}) - \text{height}(\text{right})| \leq 1$$

### Altura máxima

Un AVL de $n$ nodos tiene altura $\leq 1.44 \log_2(n + 2) - 0.328$.
Esto se deriva de los **árboles de Fibonacci**: los AVL más altos
posibles para un número dado de nodos.

Un árbol de Fibonacci $F_h$ de altura $h$ tiene $F_{h+2} - 1$ nodos
(donde $F_k$ es el $k$-ésimo número de Fibonacci):

```
F₀ (h=0):    F₁ (h=1):      F₂ (h=2):         F₃ (h=3):
   1             2              4                   7
                / \            / \                 / \
               1   3          2   6               4   11
                             / \  |              / \  / \
                            1  3  5             2  6 9  12
                                               /\ |  |
                                              1 3 5  10
```

El AVL con el menor número de nodos para una altura dada es el
árbol de Fibonacci. Cualquier AVL con más nodos para esa altura
será "más lleno" y tendrá la misma o menor altura.

### Comparaciones de búsqueda

En el peor caso, una búsqueda en un AVL de $n$ nodos hace:

$$C_{\text{AVL}} \leq 1.44 \log_2(n + 2) \approx 1.44 \log_2 n$$

En promedio (para un AVL construido con inserciones aleatorias):

$$C_{\text{AVL}} \approx \log_2 n + 0.25$$

Casi idéntico a un BST perfectamente balanceado ($\log_2 n$). El
overhead del invariante AVL es solo ~0.25 comparaciones extra en
promedio.

---

## Red-Black: balance relajado

### Invariante (5 propiedades)

1. Cada nodo es rojo o negro.
2. La raíz es negra.
3. Las hojas (NIL) son negras.
4. Un nodo rojo no tiene hijos rojos.
5. Todo camino raíz-hoja tiene el mismo número de nodos negros
   (**black-height**).

### Altura máxima

Un Red-Black tree de $n$ nodos tiene altura $\leq 2 \log_2(n + 1)$.

La demostración: el black-height es $\leq \log_2(n + 1)$ (propiedad 5
implica que cada nivel negro al menos duplica los nodos). La altura
total es $\leq 2 \times \text{black-height}$ (propiedad 4 impide dos
rojos consecutivos, así que a lo sumo la mitad de los nodos en un
camino son rojos).

### Comparaciones de búsqueda

Peor caso:

$$C_{\text{RB}} \leq 2 \log_2(n + 1)$$

Promedio:

$$C_{\text{RB}} \approx \log_2 n + 0.5$$

El RB tree es ~0.25 comparaciones peor que AVL en promedio, y hasta
~40% peor en el peor caso.

---

## AVL vs Red-Black: perspectiva de búsqueda

### Comparación directa

| $n$ | AVL peor caso | RB peor caso | Diferencia |
|-----|--------------|-------------|------------|
| $10^3$ | 15 | 20 | +5 (33%) |
| $10^6$ | 29 | 40 | +11 (38%) |
| $10^9$ | 44 | 60 | +16 (36%) |

Para $n = 10^6$, la diferencia es ~11 comparaciones en el peor caso.
A ~5 ns por comparación (con cache miss), esto es ~55 ns — relevante
en aplicaciones de latencia crítica pero negligible en la mayoría de
los casos.

### Cuándo AVL gana

- **Workloads de lectura intensiva** (>90% búsquedas): el árbol más
  bajo del AVL compensa el mayor costo de rebalanceo en las pocas
  escrituras.
- **Bases de datos en memoria** donde la latencia de búsqueda es
  crítica.
- **Sistemas con cache limitada**: menos niveles = menos cache misses.

### Cuándo Red-Black gana

- **Workloads mixtos** (muchas inserciones/eliminaciones): el RB
  necesita $\leq 2$ rotaciones por inserción y $\leq 3$ por
  eliminación. El AVL puede necesitar $O(\log n)$ rotaciones por
  eliminación (en la práctica, 1-2 en promedio).
- **Implementaciones de stdlib**: la mayoría de las implementaciones
  estándar usan RB trees porque el peor caso de inserción/eliminación
  es más predecible.
- **Sistemas real-time**: el límite constante de rotaciones del RB
  da un WCET (Worst-Case Execution Time) más ajustado.

### En la práctica

| Implementación | Estructura |
|----------------|------------|
| Linux kernel (`rbtree.h`) | Red-Black |
| Java `TreeMap` | Red-Black |
| C++ `std::map` / `std::set` | Red-Black (mayoría de implementaciones) |
| Rust `BTreeMap` | B-tree (no binario) |
| PostgreSQL indices | B+ tree |
| Bases de datos en memoria | A menudo AVL |

La tendencia moderna: **B-trees** (tema de T03-T04) están
reemplazando a ambos para búsqueda porque son más cache-friendly.
Los árboles binarios balanceados persisten donde se necesita
rotación eficiente o complejidad de implementación mínima.

---

## Búsqueda: el código es idéntico al BST

La operación de búsqueda en un AVL o RB tree es **exactamente la
misma** que en un BST no balanceado:

```c
/* Búsqueda — idéntica para BST, AVL, y RB */
Node *search(Node *root, int key)
{
    while (root) {
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}
```

El balanceo no afecta la lógica de búsqueda. Solo afecta la
**estructura** del árbol (y por tanto la altura), garantizando que
el camino recorrido sea corto.

La diferencia está en la inserción y eliminación, que deben
mantener el invariante de balanceo. Estas operaciones están
implementadas en C06/S04.

---

## Operaciones extendidas en árboles balanceados

Todas las operaciones de T01 (min, max, floor, ceiling, rank,
select, rango) funcionan en árboles balanceados con la misma lógica
y **garantía** $O(\log n)$:

| Operación | BST no balanceado | AVL/RB |
|-----------|------------------|--------|
| Búsqueda | $O(n)$ peor | $O(\log n)$ garantizado |
| Min/Max | $O(n)$ peor | $O(\log n)$ garantizado |
| Floor/Ceiling | $O(n)$ peor | $O(\log n)$ garantizado |
| Rank/Select | $O(n)$ peor | $O(\log n)$ garantizado |
| Rango $[lo, hi]$ | $O(n + k)$ peor | $O(\log n + k)$ garantizado |
| Inserción | $O(n)$ peor | $O(\log n)$ garantizado |
| Eliminación | $O(n)$ peor | $O(\log n)$ garantizado |

El balanceo transforma todas las operaciones de "caso promedio
$O(\log n)$" a "siempre $O(\log n)$".

---

## Comparación empírica: BST vs AVL vs RB vs array

### Patrón de acceso: solo búsquedas

Para $n = 10^5$ elementos, $10^5$ búsquedas aleatorias:

```
Estructura            Comparaciones/búsqueda    Tiempo/búsqueda
────────────────────  ────────────────────────  ───────────────
Array + binaria       ~17 (siempre)             ~120 ns
AVL                   ~17 (garantizado)         ~350 ns
Red-Black             ~18 (garantizado)         ~380 ns
BST aleatorio         ~20 (promedio)            ~500 ns
BST degenerado        ~50,000 (peor)            ~50 µs
```

El array gana en tiempo absoluto por cache locality, pero no
soporta inserciones eficientes. AVL y RB son comparables entre sí,
significativamente mejores que BST no balanceado.

### Patrón de acceso: 50% búsquedas, 50% inserciones

```
Estructura            Tiempo total (10⁵ ops)
────────────────────  ────────────────────────
Array + binaria       ~800 ms  (inserciones O(n) dominan)
AVL                   ~45 ms
Red-Black             ~40 ms
BST aleatorio         ~50 ms  (puede degradar)
BST degenerado        ~2500 ms
HashMap               ~15 ms  (pero sin orden)
```

Con workloads mixtos, los árboles balanceados son la mejor opción
para datos ordenados con operaciones dinámicas.

---

## Árboles balanceados en la stdlib

### C: no hay árbol balanceado estándar

La stdlib de C no incluye ninguna estructura de árbol balanceado.
Las opciones son:

1. **Implementación propia** (C06/S04).
2. **`tsearch`/`tfind`** de POSIX (`<search.h>`) — BST sin
   garantía de balanceo en el estándar. glibc lo implementa como
   RB tree, pero no es portable.
3. **Bibliotecas externas** (GLib `GTree`, etc.).

```c
#include <search.h>

/* POSIX tsearch — interfaz poco ergonómica */
void *root = NULL;
int val = 42;
int cmp(const void *a, const void *b) {
    return *(int *)a - *(int *)b;  /* mismo patrón que qsort */
}
tsearch(&val, &root, cmp);        /* insertar */
void *found = tfind(&val, &root, cmp);  /* buscar */
```

`tsearch` usa el mismo patrón de `void *` + comparador que `qsort`.
Las mismas limitaciones aplican: sin inlining, sin type safety.

### Rust: BTreeMap y BTreeSet

Rust ofrece `BTreeMap<K, V>` y `BTreeSet<T>` en la stdlib — basados
en **B-tree**, no en árboles binarios:

```rust
use std::collections::BTreeMap;

let mut map = BTreeMap::new();
map.insert(20, "veinte");
map.insert(10, "diez");
map.insert(30, "treinta");

// Búsqueda O(log n)
if let Some(val) = map.get(&20) {
    println!("Encontrado: {}", val);
}

// Rango
for (k, v) in map.range(10..=25) {
    println!("{}: {}", k, v);
}

// Floor/ceiling equivalentes
let ceiling = map.range(15..).next();       // primer ≥ 15
let floor = map.range(..=15).next_back();   // último ≤ 15
```

`BTreeMap` es mejor que un AVL/RB binario para búsqueda porque el
B-tree agrupa múltiples claves por nodo, reduciendo cache misses.
Veremos B-trees en detalle en T03.

---

## Programa completo en C

```c
/* balanced_search.c — Comparación BST vs AVL para búsqueda */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* --- Nodo AVL --- */
typedef struct AVLNode {
    int data;
    int height;
    struct AVLNode *left, *right;
} AVLNode;

static int avl_height(AVLNode *n) { return n ? n->height : -1; }

static void avl_update(AVLNode *n)
{
    int lh = avl_height(n->left);
    int rh = avl_height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

static int avl_balance(AVLNode *n)
{
    return n ? avl_height(n->left) - avl_height(n->right) : 0;
}

static AVLNode *avl_new(int data)
{
    AVLNode *n = malloc(sizeof(AVLNode));
    n->data = data;
    n->height = 0;
    n->left = n->right = NULL;
    return n;
}

static AVLNode *rotate_right(AVLNode *y)
{
    AVLNode *x = y->left;
    y->left = x->right;
    x->right = y;
    avl_update(y);
    avl_update(x);
    return x;
}

static AVLNode *rotate_left(AVLNode *x)
{
    AVLNode *y = x->right;
    x->right = y->left;
    y->left = x;
    avl_update(x);
    avl_update(y);
    return y;
}

AVLNode *avl_insert(AVLNode *root, int data)
{
    if (!root) return avl_new(data);

    if (data < root->data)
        root->left = avl_insert(root->left, data);
    else if (data > root->data)
        root->right = avl_insert(root->right, data);
    else
        return root;

    avl_update(root);
    int bal = avl_balance(root);

    if (bal > 1 && data < root->left->data)
        return rotate_right(root);
    if (bal < -1 && data > root->right->data)
        return rotate_left(root);
    if (bal > 1 && data > root->left->data) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (bal < -1 && data < root->right->data) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }

    return root;
}

/* --- BST simple (sin balanceo) --- */
typedef struct BSTNode {
    int data;
    struct BSTNode *left, *right;
} BSTNode;

BSTNode *bst_new(int data)
{
    BSTNode *n = malloc(sizeof(BSTNode));
    n->data = data;
    n->left = n->right = NULL;
    return n;
}

BSTNode *bst_insert(BSTNode *root, int data)
{
    if (!root) return bst_new(data);
    if (data < root->data) root->left = bst_insert(root->left, data);
    else if (data > root->data) root->right = bst_insert(root->right, data);
    return root;
}

/* --- Búsqueda genérica (misma lógica, distinto tipo) --- */
int avl_search_counted(AVLNode *root, int key)
{
    int count = 0;
    while (root) {
        count++;
        if (key == root->data) return count;
        root = (key < root->data) ? root->left : root->right;
    }
    return count;
}

int bst_search_counted(BSTNode *root, int key)
{
    int count = 0;
    while (root) {
        count++;
        if (key == root->data) return count;
        root = (key < root->data) ? root->left : root->right;
    }
    return count;
}

int bst_height(BSTNode *root)
{
    if (!root) return -1;
    int lh = bst_height(root->left);
    int rh = bst_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

/* --- Limpieza --- */
void avl_free(AVLNode *r) { if (r) { avl_free(r->left); avl_free(r->right); free(r); } }
void bst_free(BSTNode *r) { if (r) { bst_free(r->left); bst_free(r->right); free(r); } }

/* --- LCG + shuffle --- */
static unsigned int lcg = 42;
static int lcg_rand(void) { lcg = lcg * 1103515245 + 12345; return (int)((lcg >> 1) & 0x7FFFFFFF); }
void shuffle(int *a, int n) {
    for (int i = n-1; i > 0; i--) { int j = lcg_rand() % (i+1); int t = a[i]; a[i] = a[j]; a[j] = t; }
}

int main(void)
{
    /* Demo 1: Inserción ordenada — BST degenera, AVL no */
    printf("=== Demo 1: Inserción ordenada ===\n");
    int N = 10000;
    AVLNode *avl = NULL;
    BSTNode *bst = NULL;

    for (int i = 0; i < N; i++) {
        avl = avl_insert(avl, i);
        bst = bst_insert(bst, i);
    }

    printf("  n = %d\n", N);
    printf("  BST height: %d  (óptimo: %d)\n", bst_height(bst), (int)(log2(N)));
    printf("  AVL height: %d  (máx teórico: %d)\n",
           avl_height(avl), (int)(1.44 * log2(N + 2)));

    /* Demo 2: Comparar comparaciones */
    printf("\n=== Demo 2: Comparaciones por búsqueda ===\n");
    long total_bst = 0, total_avl = 0;
    int queries = 10000;
    for (int q = 0; q < queries; q++) {
        int target = lcg_rand() % N;
        total_bst += bst_search_counted(bst, target);
        total_avl += avl_search_counted(avl, target);
    }

    printf("  %d búsquedas aleatorias:\n", queries);
    printf("  BST degenerado: %.1f comparaciones/búsqueda\n",
           (double)total_bst / queries);
    printf("  AVL:            %.1f comparaciones/búsqueda\n",
           (double)total_avl / queries);
    printf("  Ratio BST/AVL:  %.0fx\n",
           (double)total_bst / total_avl);

    avl_free(avl);
    bst_free(bst);

    /* Demo 3: Inserción aleatoria — BST razonable, AVL mejor */
    printf("\n=== Demo 3: Inserción aleatoria ===\n");
    int *data = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) data[i] = i;
    shuffle(data, N);

    AVLNode *avl2 = NULL;
    BSTNode *bst2 = NULL;
    for (int i = 0; i < N; i++) {
        avl2 = avl_insert(avl2, data[i]);
        bst2 = bst_insert(bst2, data[i]);
    }

    printf("  BST aleatorio height: %d\n", bst_height(bst2));
    printf("  AVL height:           %d\n", avl_height(avl2));

    total_bst = 0; total_avl = 0;
    for (int q = 0; q < queries; q++) {
        int target = lcg_rand() % N;
        total_bst += bst_search_counted(bst2, target);
        total_avl += avl_search_counted(avl2, target);
    }

    printf("  BST aleatorio: %.1f comparaciones/búsqueda\n",
           (double)total_bst / queries);
    printf("  AVL:           %.1f comparaciones/búsqueda\n",
           (double)total_avl / queries);

    /* Demo 4: Benchmark de tiempo */
    printf("\n=== Demo 4: Benchmark de tiempo ===\n");

    /* Array + binary search */
    int *sorted = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) sorted[i] = i;

    volatile int dummy = 0;
    int reps = 100000;

    clock_t t0 = clock();
    for (int r = 0; r < reps; r++) {
        int target = lcg_rand() % N;
        int lo = 0, hi = N - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (sorted[mid] == target) { dummy = mid; break; }
            if (sorted[mid] < target) lo = mid + 1;
            else hi = mid - 1;
        }
    }
    clock_t t1 = clock();
    double arr_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    t0 = clock();
    for (int r = 0; r < reps; r++) {
        int target = lcg_rand() % N;
        AVLNode *curr = avl2;
        while (curr) {
            if (target == curr->data) { dummy = curr->data; break; }
            curr = (target < curr->data) ? curr->left : curr->right;
        }
    }
    t1 = clock();
    double avl_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    t0 = clock();
    for (int r = 0; r < reps; r++) {
        int target = lcg_rand() % N;
        BSTNode *curr = bst2;
        while (curr) {
            if (target == curr->data) { dummy = curr->data; break; }
            curr = (target < curr->data) ? curr->left : curr->right;
        }
    }
    t1 = clock();
    double bst_ms = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000;

    printf("  %d búsquedas (n=%d):\n", reps, N);
    printf("  Array + binaria: %7.2f ms  (%.0f ns/búsqueda)\n",
           arr_ms, arr_ms / reps * 1e6);
    printf("  AVL:             %7.2f ms  (%.0f ns/búsqueda)\n",
           avl_ms, avl_ms / reps * 1e6);
    printf("  BST aleatorio:   %7.2f ms  (%.0f ns/búsqueda)\n",
           bst_ms, bst_ms / reps * 1e6);

    avl_free(avl2);
    bst_free(bst2);
    free(data);
    free(sorted);
    (void)dummy;

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -std=c11 -Wall -Wextra -o balanced_search balanced_search.c -lm
./balanced_search
```

### Salida esperada

```
=== Demo 1: Inserción ordenada ===
  n = 10000
  BST height: 9999  (óptimo: 13)
  AVL height: 13  (máx teórico: 19)

=== Demo 2: Comparaciones por búsqueda ===
  10000 búsquedas aleatorias:
  BST degenerado: ~5000.0 comparaciones/búsqueda
  AVL:            ~12.5 comparaciones/búsqueda
  Ratio BST/AVL:  ~400x

=== Demo 3: Inserción aleatoria ===
  BST aleatorio height: ~27
  AVL height:           ~14
  BST aleatorio: ~16.0 comparaciones/búsqueda
  AVL:           ~12.5 comparaciones/búsqueda

=== Demo 4: Benchmark de tiempo ===
  100000 búsquedas (n=10000):
  Array + binaria:  ~5.00 ms  (~50 ns/búsqueda)
  AVL:              ~15.00 ms  (~150 ns/búsqueda)
  BST aleatorio:    ~20.00 ms  (~200 ns/búsqueda)
```

---

## Programa completo en Rust

```rust
// balanced_search.rs — Comparación de estructuras de búsqueda
use std::collections::{BTreeMap, HashMap};
use std::time::Instant;

// BST simple para comparación
type Tree = Option<Box<BSTNode>>;

struct BSTNode {
    data: i32,
    left: Tree,
    right: Tree,
}

fn bst_insert(tree: &mut Tree, data: i32) {
    match tree {
        None => *tree = Some(Box::new(BSTNode { data, left: None, right: None })),
        Some(node) => {
            if data < node.data { bst_insert(&mut node.left, data); }
            else if data > node.data { bst_insert(&mut node.right, data); }
        }
    }
}

fn bst_search(tree: &Tree, key: i32) -> bool {
    let mut curr = tree;
    while let Some(node) = curr {
        if key == node.data { return true; }
        curr = if key < node.data { &node.left } else { &node.right };
    }
    false
}

fn bst_height(tree: &Tree) -> i32 {
    match tree {
        None => -1,
        Some(n) => 1 + bst_height(&n.left).max(bst_height(&n.right)),
    }
}

fn main() {
    let n = 10_000i32;

    // LCG para reproducibilidad
    let mut lcg: u32 = 42;
    let mut rand = || -> i32 {
        lcg = lcg.wrapping_mul(1103515245).wrapping_add(12345);
        ((lcg >> 1) & 0x7FFFFFFF) as i32
    };

    // Shuffle
    let mut data: Vec<i32> = (0..n).collect();
    for i in (1..data.len()).rev() {
        let j = rand() as usize % (i + 1);
        data.swap(i, j);
    }

    // ============================================
    // Demo 1: Construir estructuras
    // ============================================
    println!("=== Demo 1: Altura BST vs BTreeMap ===");

    // BST aleatorio
    let mut bst: Tree = None;
    for &v in &data { bst_insert(&mut bst, v); }
    println!("  BST aleatorio height: {}", bst_height(&bst));

    // BST degenerado
    let mut bst_degen: Tree = None;
    for i in 0..n { bst_insert(&mut bst_degen, i); }
    println!("  BST degenerado height: {}", bst_height(&bst_degen));

    // BTreeMap (balanceado automáticamente)
    let mut btree = BTreeMap::new();
    for &v in &data { btree.insert(v, ()); }
    println!("  BTreeMap: balanceado automáticamente (B-tree)");

    // ============================================
    // Demo 2: Benchmark de búsqueda
    // ============================================
    println!("\n=== Demo 2: Benchmark de búsqueda ===");

    // Array ordenado
    let sorted: Vec<i32> = (0..n).collect();
    let reps = 100_000usize;

    // Generar queries
    let queries: Vec<i32> = (0..reps).map(|_| rand() % n).collect();

    // Array + binary_search
    let start = Instant::now();
    let mut dummy = 0usize;
    for &q in &queries {
        dummy += sorted.binary_search(&q).unwrap_or(0);
    }
    let arr_ns = start.elapsed().as_nanos() as f64 / reps as f64;

    // BTreeMap
    let start = Instant::now();
    for &q in &queries {
        if btree.contains_key(&q) { dummy += 1; }
    }
    let btree_ns = start.elapsed().as_nanos() as f64 / reps as f64;

    // HashMap
    let hmap: HashMap<i32, ()> = (0..n).map(|i| (i, ())).collect();
    let start = Instant::now();
    for &q in &queries {
        if hmap.contains_key(&q) { dummy += 1; }
    }
    let hash_ns = start.elapsed().as_nanos() as f64 / reps as f64;

    // BST manual
    let start = Instant::now();
    for &q in &queries {
        if bst_search(&bst, q) { dummy += 1; }
    }
    let bst_ns = start.elapsed().as_nanos() as f64 / reps as f64;

    println!("  {} búsquedas (n={}):", reps, n);
    println!("  Array + binary_search: {:>7.0} ns/búsqueda", arr_ns);
    println!("  BTreeMap:              {:>7.0} ns/búsqueda", btree_ns);
    println!("  HashMap:               {:>7.0} ns/búsqueda", hash_ns);
    println!("  BST manual:            {:>7.0} ns/búsqueda", bst_ns);
    println!("  (dummy={})", dummy);

    // ============================================
    // Demo 3: BTreeMap — operaciones extendidas
    // ============================================
    println!("\n=== Demo 3: BTreeMap operaciones extendidas ===");

    let mut map = BTreeMap::new();
    for &v in &[20, 10, 30, 5, 15, 25, 35] {
        map.insert(v, format!("val_{}", v));
    }

    // Floor (último ≤ key)
    let floor_17 = map.range(..=17).next_back();
    println!("  floor(17): {:?}", floor_17);

    // Ceiling (primero ≥ key)
    let ceiling_17 = map.range(17..).next();
    println!("  ceiling(17): {:?}", ceiling_17);

    // Rango
    print!("  range [12, 28]:");
    for (k, _) in map.range(12..=28) {
        print!(" {}", k);
    }
    println!();

    // Min / Max
    println!("  min: {:?}", map.iter().next());
    println!("  max: {:?}", map.iter().next_back());

    // ============================================
    // Demo 4: BTreeSet para búsqueda pura
    // ============================================
    println!("\n=== Demo 4: BTreeSet ===");
    use std::collections::BTreeSet;

    let set: BTreeSet<i32> = (0..n).collect();

    println!("  contains(500): {}", set.contains(&500));
    println!("  contains({}): {}", n, set.contains(&n));

    // Rango
    let count = set.range(4000..=4100).count();
    println!("  count in [4000, 4100]: {}", count);

    // k-ésimo menor (O(k) — no O(log n))
    let kth = set.iter().nth(42);
    println!("  43rd smallest: {:?}", kth);
}
```

### Compilar y ejecutar

```bash
rustc -O -o balanced_search balanced_search.rs
./balanced_search
```

### Salida esperada

```
=== Demo 1: Altura BST vs BTreeMap ===
  BST aleatorio height: ~27
  BST degenerado height: 9999
  BTreeMap: balanceado automáticamente (B-tree)

=== Demo 2: Benchmark de búsqueda ===
  100000 búsquedas (n=10000):
  Array + binary_search:      50 ns/búsqueda
  BTreeMap:                  100 ns/búsqueda
  HashMap:                    30 ns/búsqueda
  BST manual:                200 ns/búsqueda

=== Demo 3: BTreeMap operaciones extendidas ===
  floor(17): Some((15, "val_15"))
  ceiling(17): Some((&20, &"val_20"))
  range [12, 28]: 15 20 25
  min: Some((5, "val_5"))
  max: Some((35, "val_35"))

=== Demo 4: BTreeSet ===
  contains(500): true
  contains(10000): false
  count in [4000, 4100]: 101
  43rd smallest: Some(42)
```

---

## Ejercicios

### Ejercicio 1 — Altura AVL vs RB

Para $n = 10^6$, calcula la altura máxima teórica de un AVL y un
RB tree. ¿Cuántas comparaciones extra hace el RB en el peor caso?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**AVL**: $h \leq 1.44 \log_2(10^6 + 2) \approx 1.44 \times 20 = 28.8 \approx 29$

**RB**: $h \leq 2 \log_2(10^6 + 1) \approx 2 \times 20 = 40$

Diferencia: $40 - 29 = 11$ comparaciones extra en el peor caso.

A ~5 ns por comparación: ~55 ns de diferencia. Para una aplicación
que hace $10^9$ búsquedas/segundo, esto sería ~55 segundos de
diferencia total — significativo.

En promedio, la diferencia es mucho menor (~0.25 comparaciones,
~1.25 ns). La mayoría de aplicaciones no notan la diferencia.

</details>

### Ejercicio 2 — Inserción ordenada: comparar alturas

Si se insertan los valores 1, 2, 3, ..., 15 en un AVL, ¿cuál es
la altura resultante? ¿Y en un BST sin balanceo?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**BST sin balanceo**: inserción ordenada produce una cadena:

```
1→2→3→...→15    height = 14
```

**AVL**: las rotaciones mantienen el balanceo. Después de insertar
1-15 en orden:

```
            8
          /   \
        4      12
       / \    /  \
      2   6  10   14
     /\ /\ /\   / \
    1 3 5 7 9 11 13 15

height = 3
```

El AVL produce un árbol perfectamente balanceado de altura 3 para
15 nodos ($\lfloor \log_2 15 \rfloor = 3$). Esto es porque las
rotaciones redistribuyen los nodos durante la inserción.

La diferencia: 14 comparaciones (BST) vs 4 comparaciones (AVL)
para el peor caso de búsqueda. Factor ~3.5x.

</details>

### Ejercicio 3 — POSIX tsearch

¿Es seguro depender de que `tsearch` use un RB tree? ¿Qué dice
el estándar?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**No es seguro.** POSIX especifica la **interfaz** de `tsearch`
pero no el algoritmo interno:

> "The tsearch() function shall build and access a binary search
> tree."

Solo dice "binary search tree" — no especifica balanceo. Las
implementaciones:

| Implementación | Algoritmo |
|----------------|-----------|
| glibc (Linux) | Red-Black tree |
| musl (Alpine) | AVL tree |
| macOS | Red-Black tree |
| FreeBSD | Red-Black tree |

Todas las implementaciones prácticas usan árboles balanceados
(sería negligente no hacerlo), pero el estándar no lo garantiza.
Un sistema embebido podría implementar `tsearch` como BST simple.

Código portable no debe asumir $O(\log n)$ garantizado de `tsearch`.
Si se necesita la garantía, implementar el árbol propio o usar una
biblioteca como GLib.

En Rust, `BTreeMap` documenta explícitamente $O(\log n)$ como parte
de su contrato — no es un detalle de implementación.

</details>

### Ejercicio 4 — Read-heavy vs write-heavy

Un sistema tiene dos modos: modo consulta (99% búsquedas, 1%
inserciones) y modo carga (10% búsquedas, 90% inserciones). ¿Qué
estructura recomendarías para cada modo?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Modo consulta** (99% búsquedas):

Mejor opción: **Array ordenado + búsqueda binaria**.

- Las búsquedas son ~3x más rápidas que en AVL por cache locality.
- El 1% de inserciones es costoso ($O(n)$ shift) pero infrecuente.
- Si $n = 10^5$ y 1% de $10^6$ ops = $10^4$ inserciones: costo
  total de inserciones = $10^4 \times 5 \times 10^4 / 2 = 2.5 \times 10^8$ ops.
  Costo de búsquedas = $9.9 \times 10^5 \times 17 = 1.7 \times 10^7$ ops.
  Las inserciones dominan, así que si la tasa de inserción sube, cambiar a AVL.

Alternativa: **AVL** — si las inserciones no se pueden tolerar como
$O(n)$, el AVL da las búsquedas más rápidas entre los árboles
balanceados.

**Modo carga** (90% inserciones):

Mejor opción: **Red-Black tree** o **HashMap** (si no se necesita
orden).

- RB: $\leq 2$ rotaciones por inserción (constante), búsquedas
  $O(\log n)$ cuando se necesitan.
- HashMap: $O(1)$ inserción y búsqueda, pero sin orden.
- Evitar array: $O(n)$ por inserción sería catastrófico.

Si después de la carga se pasa a modo consulta, se podría volcarse
el RB/HashMap a un array ordenado para búsquedas más rápidas.

</details>

### Ejercicio 5 — Cache miss analysis

Si un nodo AVL ocupa 32 bytes y una línea de cache es 64 bytes,
¿cuántos nodos caben por línea? ¿Cómo se compara con un array de
`int` (4 bytes)?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**AVL**: 64 / 32 = **2 nodos** por línea de cache. Pero los nodos
no están contiguos en memoria (allocados con `malloc`), así que en
la práctica cada acceso a un nodo es un cache miss independiente.

Estructura de un nodo AVL:

```
struct AVLNode {
    int data;          // 4 bytes
    int height;        // 4 bytes
    AVLNode *left;     // 8 bytes
    AVLNode *right;    // 8 bytes
};
// Total: 24 bytes + padding = 32 bytes (con alineamiento)
```

**Array de `int`**: 64 / 4 = **16 enteros** por línea de cache.
Los enteros están contiguos, así que cargar una línea trae 16
valores útiles.

Para búsqueda binaria en un array de $n = 10^6$:

- Primeros accesos: dispersos (cache misses), pero cada miss carga
  16 enteros cercanos.
- Últimos ~4 niveles: todo en 1-2 líneas de cache (cache hits).
- Total: ~5-7 cache misses para 20 comparaciones.

Para búsqueda en AVL de $n = 10^6$:

- Cada nivel es un cache miss (nodos dispersos).
- ~29 niveles = ~29 cache misses.
- Total: ~29 cache misses para 29 comparaciones.

Ratio: 29/6 ≈ **5x más cache misses** en el AVL. A ~100 ns por
miss, la diferencia es ~2.3 µs vs ~0.6 µs — el AVL es ~4x más
lento solo por cache, a pesar de hacer un número similar de
comparaciones.

</details>

### Ejercicio 6 — BTreeMap::range como lower/upper bound

Implementa `lower_bound` y `upper_bound` usando `BTreeMap::range`
en Rust:

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
use std::collections::BTreeMap;
use std::ops::Bound;

fn lower_bound(map: &BTreeMap<i32, ()>, target: i32) -> Option<i32> {
    // Primer elemento >= target
    map.range(target..).next().map(|(&k, _)| k)
}

fn upper_bound(map: &BTreeMap<i32, ()>, target: i32) -> Option<i32> {
    // Primer elemento > target
    map.range((Bound::Excluded(target), Bound::Unbounded))
       .next()
       .map(|(&k, _)| k)
}

fn floor(map: &BTreeMap<i32, ()>, target: i32) -> Option<i32> {
    // Último elemento <= target
    map.range(..=target).next_back().map(|(&k, _)| k)
}

fn ceiling(map: &BTreeMap<i32, ()>, target: i32) -> Option<i32> {
    // Primer elemento >= target (igual que lower_bound)
    lower_bound(map, target)
}
```

Uso:

```rust
let map: BTreeMap<i32, ()> = [5, 10, 15, 20, 25].iter()
    .map(|&k| (k, ())).collect();

assert_eq!(lower_bound(&map, 12), Some(15));
assert_eq!(upper_bound(&map, 15), Some(20));
assert_eq!(floor(&map, 17), Some(15));
assert_eq!(ceiling(&map, 17), Some(20));
```

`BTreeMap::range` es $O(\log n)$ para posicionar el iterador +
$O(1)$ para cada elemento iterado. Así que `lower_bound` y
`upper_bound` son $O(\log n)$.

</details>

### Ejercicio 7 — Peor caso de AVL vs mejor caso de BST

¿Es posible que un BST no balanceado sea más rápido que un AVL
para una secuencia específica de búsquedas?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Sí.** Si las búsquedas tienen **localidad temporal** (los mismos
pocos elementos se buscan repetidamente), un BST con move-to-root
(splay tree) puede ser más rápido:

- Splay tree: amortizado $O(\log n)$, pero elementos frecuentes
  migran a la raíz → $O(1)$ para hot keys.
- AVL: siempre $O(\log n)$, incluso para hot keys.

Ejemplo: si el 90% de las búsquedas son para el mismo elemento,
un splay tree lo pone en la raíz tras la primera búsqueda:
$0.9 \times 1 + 0.1 \times O(\log n) \approx 1$ comparación
promedio.

El AVL haría $O(\log n)$ para **todas** las búsquedas porque no
modifica la estructura durante la búsqueda.

Sin embargo, los splay trees modifican el árbol en cada búsqueda
(rotaciones), lo que:

1. Impide búsquedas concurrentes sin locking.
2. Invalida cache más frecuentemente.
3. Tiene peor caso $O(n)$ por operación (aunque amortizado
   $O(\log n)$).

Para la mayoría de aplicaciones, un AVL/RB + cache separada es
preferible a un splay tree.

</details>

### Ejercicio 8 — Eliminar y buscar

Si se eliminan el 50% de los nodos de un AVL de $n = 10^6$, ¿cuál
es la nueva altura máxima? ¿Las búsquedas son más rápidas?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Después de eliminar 50%, quedan $n/2 = 500{,}000$ nodos.

Nueva altura máxima AVL:
$h \leq 1.44 \log_2(500{,}001) \approx 1.44 \times 19 = 27.4 \approx 28$

Altura original:
$h \leq 1.44 \log_2(10^6 + 2) \approx 29$

Diferencia: solo ~1 nivel menos. Esto es porque $\log$ crece
muy lentamente — reducir $n$ a la mitad solo resta ~1 al logaritmo.

Las búsquedas son apenas más rápidas (~1 comparación menos en peor
caso). Pero el beneficio real de eliminar nodos es **menos nodos
en memoria** → mejor probabilidad de cache hits si el working set
cabe en L2/L3.

Moraleja: eliminar nodos de un árbol balanceado tiene poco impacto
en la altura pero puede mejorar la localidad de cache.

</details>

### Ejercicio 9 — Verificar balanceo durante búsqueda

Escribe una búsqueda en AVL que al mismo tiempo verifique que el
invariante de balanceo ($|h_l - h_r| \leq 1$) se cumple en cada
nodo visitado.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
AVLNode *avl_search_validate(AVLNode *root, int key, int *balanced)
{
    *balanced = 1;

    while (root) {
        /* Verificar invariante AVL */
        int bal = avl_height(root->left) - avl_height(root->right);
        if (bal < -1 || bal > 1) {
            *balanced = 0;
            printf("VIOLACIÓN: nodo %d tiene balance %d\n",
                   root->data, bal);
        }

        /* Verificar que height almacenado es correcto */
        int expected = 1 + (avl_height(root->left) > avl_height(root->right)
                           ? avl_height(root->left) : avl_height(root->right));
        if (root->height != expected) {
            *balanced = 0;
            printf("VIOLACIÓN: nodo %d tiene height=%d, esperado=%d\n",
                   root->data, root->height, expected);
        }

        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }

    return NULL;
}
```

Esta función es útil para debugging: detecta corrupciones del
invariante AVL durante operaciones normales de búsqueda, sin el
costo de validar todo el árbol ($O(n)$).

Solo verifica los nodos en el camino de búsqueda ($O(\log n)$
nodos), no todo el árbol. Una violación en un subárbol no visitado
no se detectará.

</details>

### Ejercicio 10 — Diseño: ¿por qué Rust usa B-tree, no RB?

¿Por qué `BTreeMap` de Rust usa un B-tree en lugar de un Red-Black
tree como Java y C++?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Razones de rendimiento (cache)**:

1. Un nodo de B-tree contiene **múltiples claves** contiguas en
   memoria (típicamente 5-11 en la implementación de Rust). Buscar
   dentro de un nodo es búsqueda secuencial en datos contiguos —
   excelente para cache.

2. Un nodo RB contiene **una sola clave** y dos punteros a nodos
   posiblemente dispersos en memoria. Cada nivel es un cache miss.

3. Para $n = 10^6$: un B-tree de orden ~11 tiene altura ~6 (vs ~40
   del RB). Son ~6 cache misses vs ~40.

**Razones de diseño de Rust**:

4. **Ownership**: un B-tree almacena claves directamente en arrays
   dentro de los nodos, lo que se alinea bien con el modelo de
   ownership de Rust (sin punteros crudos entre nodos hoja).

5. **Menos allocaciones**: un B-tree con $n$ claves tiene $O(n/B)$
   nodos (donde $B$ es el branching factor). Un RB tiene $n$ nodos.
   Menos allocaciones = menos presión sobre el allocator.

6. **Cache-oblivious**: el B-tree se beneficia de la jerarquía de
   cache sin necesidad de tuning manual del branching factor.

**La implementación de Rust** usa nodos con capacidad para ~11
elementos (dependiendo de `size_of::<K>() + size_of::<V>()`). Cada
nodo interno tiene entre 6 y 11 claves.

El trade-off: el B-tree tiene inserciones y eliminaciones más
complejas (splits y merges de nodos) que las rotaciones del RB. Pero
el compilador de Rust inline y optimiza estas operaciones
agresivamente.

</details>

---

## Resumen

| Aspecto | BST | AVL | Red-Black | B-tree |
|---------|-----|-----|-----------|--------|
| Búsqueda peor caso | $O(n)$ | $O(1.44 \log n)$ | $O(2 \log n)$ | $O(\log_B n)$ |
| Inserción peor caso | $O(n)$ | $O(\log n)$ | $O(\log n)$ | $O(\log_B n)$ |
| Rotaciones por inserción | 0 | $O(\log n)$ | $\leq 2$ | 0 (splits) |
| Cache locality | Mala | Mala | Mala | Buena |
| Uso en stdlib | — | Bases de datos | Java/C++ maps | Rust `BTreeMap` |
| Mejor para | Educación | Lecturas intensivas | Escrituras frecuentes | General, disco |
