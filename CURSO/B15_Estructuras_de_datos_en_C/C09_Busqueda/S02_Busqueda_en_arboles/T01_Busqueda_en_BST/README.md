# Búsqueda en BST

## Objetivo

Analizar el BST como **estructura de búsqueda dinámica**, enfocándose
en su rendimiento comparado con las búsquedas en secuencias (T01-T04
de S01) y en su comportamiento según la forma del árbol:

- $O(\log n)$ promedio — equivalente a búsqueda binaria en array.
- $O(n)$ peor caso — cuando el árbol degenera a una lista.
- Ventaja sobre arrays: **inserción y eliminación en $O(\log n)$**
  sin reordenar elementos.
- Referencia: la implementación completa del BST está en
  **C06/S03** (definición, inserción, búsqueda, eliminación).

---

## BST como estructura de búsqueda

### Recordatorio: la propiedad BST

Para cada nodo $N$:

- Todos los valores en el subárbol izquierdo son **menores** que $N$.
- Todos los valores en el subárbol derecho son **mayores** que $N$.

```
         20
        /  \
      10    30
     / \   / \
    5  15 25  35
```

Esta propiedad convierte cada decisión de "ir a la izquierda" o "ir
a la derecha" en el equivalente de una comparación en búsqueda
binaria. Seguir un camino de la raíz a una hoja es buscar en un
"array virtual" ordenado.

### Búsqueda: de raíz a hoja

```c
TreeNode *bst_search(TreeNode *root, int key)
{
    TreeNode *curr = root;
    while (curr) {
        if (key == curr->data) return curr;
        curr = (key < curr->data) ? curr->left : curr->right;
    }
    return NULL;
}
```

Cada iteración descarta un subárbol completo — la mitad de los nodos
restantes si el árbol está balanceado. El número de comparaciones es
exactamente la **profundidad** del nodo buscado + 1.

---

## Complejidad: depende de la forma del árbol

La complejidad de búsqueda en un BST es $O(h)$, donde $h$ es la
**altura** del árbol. La altura depende del orden de inserción:

### Caso balanceado: $h = O(\log n)$

Si los elementos se insertan en un orden que mantiene el árbol
equilibrado (o se usa un árbol autobalanceado), la altura es
$\lfloor \log_2 n \rfloor$:

```
Insertar: 20, 10, 30, 5, 15, 25, 35

         20              h = 2
        /  \             n = 7
      10    30           log₂(7) ≈ 2.8
     / \   / \
    5  15 25  35
```

Búsqueda: máximo 3 comparaciones para 7 nodos.

### Caso degenerado: $h = O(n)$

Si los elementos se insertan **ordenados**, el árbol degenera en una
lista enlazada:

```
Insertar: 5, 10, 15, 20, 25, 30, 35

    5                    h = 6
     \                   n = 7
      10                 Búsqueda = O(n)
       \
        15
         \
          20
           \
            25
             \
              30
               \
                35
```

Búsqueda de 35: 7 comparaciones — igual que búsqueda secuencial.

### Caso promedio

Si los $n$ elementos se insertan en un **orden aleatorio** (cada
permutación equiprobable), la altura esperada del BST es:

$$E[h] = 4.311 \ln n - 1.953 \ln(\ln n) + O(1) \approx 2.99 \log_2 n$$

Esto es ~3x la altura óptima $\log_2 n$. El BST aleatorio es
razonablemente eficiente, pero no óptimo.

El número promedio de comparaciones para una búsqueda exitosa en un
BST aleatorio de $n$ nodos es:

$$C_n = 2 \ln n + O(1) \approx 1.39 \log_2 n$$

Exactamente el mismo resultado que quicksort — no es coincidencia,
ya que construir un BST insertando elementos uno por uno es
equivalente a ejecutar quicksort con el primer elemento como pivote.

---

## BST vs búsqueda en array: el trade-off fundamental

### Estructura estática: array ordenado gana

Si los datos no cambian después de la construcción, un array
ordenado + búsqueda binaria es **siempre mejor** que un BST:

| Operación | Array + binaria | BST |
|-----------|----------------|-----|
| Búsqueda | $O(\log n)$ | $O(\log n)$ promedio, $O(n)$ peor |
| Espacio | $O(n)$ contiguo | $O(n)$ + punteros (2-3x más) |
| Cache | Excelente (contiguo) | Malo (nodos dispersos) |
| Construcción | $O(n \log n)$ sort | $O(n \log n)$ inserciones |

El array tiene mejor localidad de cache: la búsqueda binaria accede
a posiciones que se concentran progresivamente en una región pequeña.
El BST salta entre nodos dispersos en memoria — cada comparación es
potencialmente un cache miss.

### Estructura dinámica: BST gana

Si los datos cambian (inserciones y eliminaciones frecuentes), el
BST es significativamente mejor:

| Operación | Array ordenado | BST balanceado |
|-----------|---------------|----------------|
| Inserción | $O(n)$ (shift) | $O(\log n)$ |
| Eliminación | $O(n)$ (shift) | $O(\log n)$ |
| Búsqueda | $O(\log n)$ | $O(\log n)$ |

Insertar en un array ordenado requiere desplazar en promedio $n/2$
elementos. En un BST, insertar es seguir un camino de la raíz a una
hoja — $O(h)$ sin mover ningún otro elemento.

### Tabla de decisión

| Escenario | Mejor estructura |
|-----------|-----------------|
| Datos estáticos, muchas búsquedas | Array + búsqueda binaria |
| Datos dinámicos, inserciones frecuentes | BST (balanceado) |
| Datos dinámicos, búsquedas por rango | BST o B-tree |
| Datos dinámicos, solo búsqueda exacta | Hash table |
| Datos en disco | B-tree |
| Pocos datos ($n < 100$) | Array + secuencial |

---

## Operaciones de búsqueda extendidas

El BST soporta operaciones de búsqueda que un array ordenado no
puede hacer eficientemente sin modificaciones:

### Mínimo y máximo: $O(h)$

```c
TreeNode *bst_min(TreeNode *root)
{
    if (!root) return NULL;
    while (root->left) root = root->left;
    return root;
}

TreeNode *bst_max(TreeNode *root)
{
    if (!root) return NULL;
    while (root->right) root = root->right;
    return root;
}
```

En un array ordenado, min es `arr[0]` y max es `arr[n-1]` — $O(1)$.
El BST requiere recorrer una rama completa. Pero el BST permite
**actualizar** min/max después de inserciones/eliminaciones sin
reordenar.

### Sucesor y predecesor: $O(h)$

El **sucesor** de un nodo es el siguiente valor mayor en el recorrido
inorden. Tiene dos casos:

1. Si el nodo tiene hijo derecho: el sucesor es el **mínimo** del
   subárbol derecho.
2. Si no tiene hijo derecho: el sucesor es el **ancestro más cercano**
   para el cual el nodo está en el subárbol izquierdo.

```c
/* Versión con puntero al padre */
TreeNode *bst_successor(TreeNode *node)
{
    if (node->right)
        return bst_min(node->right);

    TreeNode *parent = node->parent;
    while (parent && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}
```

Esto permite recorrer el BST **en orden** visitando cada nodo una
vez, empezando desde el mínimo y llamando repetidamente a
`bst_successor` — un iterador inorden sin recursión ni stack
explícito.

### Floor y ceiling

- **Floor(x)**: el mayor valor en el BST que es $\leq x$.
- **Ceiling(x)**: el menor valor en el BST que es $\geq x$.

```c
TreeNode *bst_floor(TreeNode *root, int key)
{
    TreeNode *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key < root->data) {
            root = root->left;
        } else {
            result = root;       /* candidato: root->data < key */
            root = root->right;
        }
    }
    return result;
}

TreeNode *bst_ceiling(TreeNode *root, int key)
{
    TreeNode *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key > root->data) {
            root = root->right;
        } else {
            result = root;       /* candidato: root->data > key */
            root = root->left;
        }
    }
    return result;
}
```

`floor` y `ceiling` son los equivalentes BST de `lower_bound` y
`upper_bound` en arrays — pero funcionan sobre la estructura de
árbol sin necesidad de que los datos estén contiguos en memoria.

### Rank y select: $O(h)$ con tamaño de subárbol

Si cada nodo almacena el tamaño de su subárbol (order-statistic
tree), se pueden responder queries adicionales:

```c
typedef struct Node {
    int data;
    int size;           /* número de nodos en este subárbol */
    struct Node *left, *right;
} Node;
```

- **Rank(x)**: ¿cuántos elementos son menores que $x$? → $O(h)$.
- **Select(k)**: ¿cuál es el $k$-ésimo menor elemento? → $O(h)$.

```c
/* Rank: contar elementos < key */
int bst_rank(Node *root, int key)
{
    if (!root) return 0;

    if (key < root->data)
        return bst_rank(root->left, key);
    if (key > root->data)
        return 1 + node_size(root->left) + bst_rank(root->right, key);
    /* key == root->data */
    return node_size(root->left);
}

/* Select: encontrar el k-ésimo menor (0-indexed) */
Node *bst_select(Node *root, int k)
{
    if (!root) return NULL;

    int left_size = node_size(root->left);
    if (k < left_size)
        return bst_select(root->left, k);
    if (k > left_size)
        return bst_select(root->right, k - left_size - 1);
    return root;
}

static int node_size(Node *n) { return n ? n->size : 0; }
```

Estas operaciones no tienen equivalente eficiente en una hash table
(que requiere $O(n)$). En un array ordenado, `rank` es `lower_bound`
y `select` es acceso por índice — ambos $O(\log n)$ y $O(1)$
respectivamente, pero el array no soporta inserción/eliminación
eficiente.

---

## Búsqueda por rango

Una ventaja exclusiva de los BSTs sobre las hash tables: encontrar
todos los elementos en un rango $[\text{lo}, \text{hi}]$:

```c
void bst_range(TreeNode *root, int lo, int hi)
{
    if (!root) return;

    if (root->data > lo)
        bst_range(root->left, lo, hi);

    if (root->data >= lo && root->data <= hi)
        printf("%d ", root->data);

    if (root->data < hi)
        bst_range(root->right, lo, hi);
}
```

Complejidad: $O(h + k)$ donde $k$ es el número de elementos en el
rango. Solo visita nodos que están en el rango o en el camino hacia
ellos — no recorre todo el árbol.

En un array ordenado, esto es `lower_bound(lo)` + recorrido lineal
hasta `upper_bound(hi)` — $O(\log n + k)$, similar. Pero el BST
permite insertar/eliminar sin reordenar.

---

## Análisis de cache: por qué los BSTs son lentos en práctica

A pesar de tener la misma complejidad teórica $O(\log n)$ que la
búsqueda binaria en array, los BSTs son significativamente más
lentos en la práctica por el comportamiento de cache:

### Búsqueda binaria en array

```
Array: [3, 5, 7, 10, 12, 15, 20, 25, 30, 35]

Accesos: arr[4], arr[1], arr[2]  (cache line de 64 bytes = 16 ints)
→ Primer acceso: cache miss (carga toda la línea)
→ Accesos siguientes: probablemente cache hits
```

Para arrays de hasta ~16 elementos, toda la búsqueda ocurre en una
sola línea de cache. Para arrays más grandes, los primeros accesos
son cache misses, pero los últimos se concentran en una región
pequeña.

### BST

```
Nodos: cada nodo es un struct separado en el heap
       posiblemente dispersos en memoria

Acceso a nodo 1: cache miss (cargar 64 bytes, usar 16-24)
Acceso a nodo 2: cache miss (otro lugar del heap)
Acceso a nodo 3: cache miss (otro lugar)
...
```

Cada nivel del BST es potencialmente un cache miss (~100 ns en RAM).
Para $h = 20$ niveles, esto son ~2 µs de latencia pura — vs ~200 ns
para la búsqueda binaria en array (que tiene ~3-5 cache misses para
$n = 10^6$).

### Medición empírica

Para $n = 10^6$ búsquedas aleatorias en $10^6$ elementos:

| Estructura | Tiempo por búsqueda | Cache misses por búsqueda |
|------------|--------------------|-----------------------|
| Array + binaria | ~150 ns | ~3-5 |
| BST balanceado | ~400-800 ns | ~15-20 |
| Hash table | ~50-100 ns | ~1-2 |

El BST es 3-5x más lento que la búsqueda binaria en array
principalmente por cache misses. Las técnicas para mitigar esto
(B-trees, cache-oblivious layouts) se ven en T03 y T04.

---

## Programa completo en C

```c
/* bst_search.c — BST como estructura de búsqueda */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct Node {
    int data;
    int size;  /* tamaño del subárbol */
    struct Node *left, *right;
} Node;

/* --- Creación e inserción --- */
Node *new_node(int data)
{
    Node *n = malloc(sizeof(Node));
    n->data = data;
    n->size = 1;
    n->left = n->right = NULL;
    return n;
}

static int node_size(Node *n) { return n ? n->size : 0; }

static void update_size(Node *n)
{
    if (n) n->size = 1 + node_size(n->left) + node_size(n->right);
}

Node *bst_insert(Node *root, int data)
{
    if (!root) return new_node(data);
    if (data < root->data)
        root->left = bst_insert(root->left, data);
    else if (data > root->data)
        root->right = bst_insert(root->right, data);
    update_size(root);
    return root;
}

/* --- Búsqueda --- */
Node *bst_search(Node *root, int key)
{
    while (root) {
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}

/* Con contador de comparaciones */
Node *bst_search_counted(Node *root, int key, int *comparisons)
{
    *comparisons = 0;
    while (root) {
        (*comparisons)++;
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}

/* --- Min, Max, Floor, Ceiling --- */
Node *bst_min(Node *root)
{
    if (!root) return NULL;
    while (root->left) root = root->left;
    return root;
}

Node *bst_max(Node *root)
{
    if (!root) return NULL;
    while (root->right) root = root->right;
    return root;
}

Node *bst_floor(Node *root, int key)
{
    Node *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key < root->data) {
            root = root->left;
        } else {
            result = root;
            root = root->right;
        }
    }
    return result;
}

Node *bst_ceiling(Node *root, int key)
{
    Node *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key > root->data) {
            root = root->right;
        } else {
            result = root;
            root = root->left;
        }
    }
    return result;
}

/* --- Rank y Select --- */
int bst_rank(Node *root, int key)
{
    if (!root) return 0;
    if (key < root->data)
        return bst_rank(root->left, key);
    if (key > root->data)
        return 1 + node_size(root->left) + bst_rank(root->right, key);
    return node_size(root->left);
}

Node *bst_select(Node *root, int k)
{
    if (!root) return NULL;
    int ls = node_size(root->left);
    if (k < ls) return bst_select(root->left, k);
    if (k > ls) return bst_select(root->right, k - ls - 1);
    return root;
}

/* --- Rango --- */
void bst_range_helper(Node *root, int lo, int hi, int *count)
{
    if (!root) return;
    if (root->data > lo) bst_range_helper(root->left, lo, hi, count);
    if (root->data >= lo && root->data <= hi) (*count)++;
    if (root->data < hi) bst_range_helper(root->right, lo, hi, count);
}

/* --- Altura --- */
int bst_height(Node *root)
{
    if (!root) return -1;
    int lh = bst_height(root->left);
    int rh = bst_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

/* --- Limpieza --- */
void bst_free(Node *root)
{
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

/* --- LCG determinista --- */
static unsigned int lcg_state = 42;
static int lcg_rand(void)
{
    lcg_state = lcg_state * 1103515245 + 12345;
    return (int)((lcg_state >> 1) & 0x7FFFFFFF);
}

/* --- Shuffle (Fisher-Yates) --- */
void shuffle(int *arr, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = lcg_rand() % (i + 1);
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

int main(void)
{
    /* Demo 1: Construcción y búsqueda */
    printf("=== Demo 1: Búsqueda en BST ===\n");
    int vals[] = {20, 10, 30, 5, 15, 25, 35};
    int n = 7;
    Node *root = NULL;
    for (int i = 0; i < n; i++)
        root = bst_insert(root, vals[i]);

    printf("Árbol (inorden): ");
    /* impresión simplificada via búsqueda del rango completo */
    Node *mn = bst_min(root), *mx = bst_max(root);
    printf("min=%d, max=%d, height=%d, size=%d\n",
           mn->data, mx->data, bst_height(root), root->size);

    int targets[] = {15, 25, 12, 35, 1};
    for (int i = 0; i < 5; i++) {
        int cmp;
        Node *found = bst_search_counted(root, targets[i], &cmp);
        printf("  Buscar %2d: %s (%d comparaciones)\n",
               targets[i], found ? "encontrado" : "no encontrado", cmp);
    }

    /* Demo 2: Floor y Ceiling */
    printf("\n=== Demo 2: Floor y Ceiling ===\n");
    int queries[] = {12, 20, 1, 40, 22};
    for (int i = 0; i < 5; i++) {
        Node *fl = bst_floor(root, queries[i]);
        Node *cl = bst_ceiling(root, queries[i]);
        printf("  key=%2d: floor=%s, ceiling=%s\n",
               queries[i],
               fl ? (char[12]){0} : "NULL",
               cl ? (char[12]){0} : "NULL");
        /* Imprimir valores reales */
        printf("  key=%2d: floor=", queries[i]);
        if (fl) printf("%d", fl->data); else printf("NULL");
        printf(", ceiling=");
        if (cl) printf("%d", cl->data); else printf("NULL");
        printf("\n");
    }

    /* Demo 3: Rank y Select */
    printf("\n=== Demo 3: Rank y Select ===\n");
    printf("  Rank(15) = %d  (elementos < 15)\n", bst_rank(root, 15));
    printf("  Rank(20) = %d  (elementos < 20)\n", bst_rank(root, 20));
    printf("  Rank(12) = %d  (elementos < 12)\n", bst_rank(root, 12));

    for (int k = 0; k < n; k++) {
        Node *s = bst_select(root, k);
        printf("  Select(%d) = %d\n", k, s->data);
    }

    /* Demo 4: BST aleatorio vs degenerado */
    printf("\n=== Demo 4: Aleatorio vs degenerado ===\n");
    int N = 10000;
    int *data = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) data[i] = i;

    /* Inserción ordenada → degenerado */
    Node *degen = NULL;
    for (int i = 0; i < N; i++) degen = bst_insert(degen, i);
    printf("  Degenerado (n=%d): height=%d\n", N, bst_height(degen));

    /* Inserción aleatoria → balanceado */
    shuffle(data, N);
    Node *balanced = NULL;
    for (int i = 0; i < N; i++) balanced = bst_insert(balanced, data[i]);
    printf("  Aleatorio  (n=%d): height=%d (óptimo=%d)\n",
           N, bst_height(balanced), (int)(log2(N)));

    /* Demo 5: Comparar búsquedas promedio */
    printf("\n=== Demo 5: Comparaciones promedio ===\n");
    long total_degen = 0, total_balanced = 0;
    int queries_n = 1000;
    for (int q = 0; q < queries_n; q++) {
        int target = lcg_rand() % N;
        int c1, c2;
        bst_search_counted(degen, target, &c1);
        bst_search_counted(balanced, target, &c2);
        total_degen += c1;
        total_balanced += c2;
    }
    printf("  Degenerado: %.1f comparaciones promedio\n",
           (double)total_degen / queries_n);
    printf("  Aleatorio:  %.1f comparaciones promedio\n",
           (double)total_balanced / queries_n);
    printf("  Ratio:      %.1fx\n",
           (double)total_degen / total_balanced);

    /* Demo 6: Búsqueda por rango */
    printf("\n=== Demo 6: Búsqueda por rango ===\n");
    int range_count = 0;
    bst_range_helper(balanced, 4000, 4100, &range_count);
    printf("  Elementos en [4000, 4100]: %d\n", range_count);

    bst_free(root);
    bst_free(degen);
    bst_free(balanced);
    free(data);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -std=c11 -Wall -Wextra -o bst_search bst_search.c -lm
./bst_search
```

### Salida esperada

```
=== Demo 1: Búsqueda en BST ===
Árbol (inorden): min=5, max=35, height=2, size=7
  Buscar 15: encontrado (2 comparaciones)
  Buscar 25: encontrado (2 comparaciones)
  Buscar 12: no encontrado (3 comparaciones)
  Buscar 35: encontrado (2 comparaciones)
  Buscar  1: no encontrado (3 comparaciones)

=== Demo 2: Floor y Ceiling ===
  key=12: floor=10, ceiling=15
  key=20: floor=20, ceiling=20
  key= 1: floor=NULL, ceiling=5
  key=40: floor=35, ceiling=NULL
  key=22: floor=20, ceiling=25

=== Demo 3: Rank y Select ===
  Rank(15) = 2  (elementos < 15)
  Rank(20) = 3  (elementos < 20)
  Rank(12) = 2  (elementos < 12)
  Select(0) = 5
  Select(1) = 10
  Select(2) = 15
  Select(3) = 20
  Select(4) = 25
  Select(5) = 30
  Select(6) = 35

=== Demo 4: Aleatorio vs degenerado ===
  Degenerado (n=10000): height=9999
  Aleatorio  (n=10000): height=~27 (óptimo=13)

=== Demo 5: Comparaciones promedio ===
  Degenerado: ~5000.0 comparaciones promedio
  Aleatorio:  ~16.5 comparaciones promedio
  Ratio:      ~303.0x

=== Demo 6: Búsqueda por rango ===
  Elementos en [4000, 4100]: 101
```

---

## Programa completo en Rust

```rust
// bst_search.rs — BST como estructura de búsqueda
use std::cmp::Ordering;

type Tree = Option<Box<Node>>;

struct Node {
    data: i32,
    size: usize,
    left: Tree,
    right: Tree,
}

impl Node {
    fn new(data: i32) -> Self {
        Node { data, size: 1, left: None, right: None }
    }
}

fn node_size(tree: &Tree) -> usize {
    tree.as_ref().map_or(0, |n| n.size)
}

fn update_size(node: &mut Node) {
    node.size = 1 + node_size(&node.left) + node_size(&node.right);
}

fn insert(tree: &mut Tree, data: i32) {
    match tree {
        None => *tree = Some(Box::new(Node::new(data))),
        Some(node) => {
            match data.cmp(&node.data) {
                Ordering::Less    => insert(&mut node.left, data),
                Ordering::Greater => insert(&mut node.right, data),
                Ordering::Equal   => return,
            }
            update_size(node);
        }
    }
}

fn search(tree: &Tree, key: i32) -> bool {
    let mut curr = tree;
    while let Some(node) = curr {
        match key.cmp(&node.data) {
            Ordering::Equal   => return true,
            Ordering::Less    => curr = &node.left,
            Ordering::Greater => curr = &node.right,
        }
    }
    false
}

fn search_counted(tree: &Tree, key: i32) -> (bool, u32) {
    let mut curr = tree;
    let mut cmp_count = 0;
    while let Some(node) = curr {
        cmp_count += 1;
        match key.cmp(&node.data) {
            Ordering::Equal   => return (true, cmp_count),
            Ordering::Less    => curr = &node.left,
            Ordering::Greater => curr = &node.right,
        }
    }
    (false, cmp_count)
}

fn bst_min(tree: &Tree) -> Option<i32> {
    let mut curr = tree;
    let mut result = None;
    while let Some(node) = curr {
        result = Some(node.data);
        curr = &node.left;
    }
    result
}

fn bst_max(tree: &Tree) -> Option<i32> {
    let mut curr = tree;
    let mut result = None;
    while let Some(node) = curr {
        result = Some(node.data);
        curr = &node.right;
    }
    result
}

fn floor(tree: &Tree, key: i32) -> Option<i32> {
    let mut curr = tree;
    let mut result = None;
    while let Some(node) = curr {
        match key.cmp(&node.data) {
            Ordering::Equal   => return Some(node.data),
            Ordering::Less    => curr = &node.left,
            Ordering::Greater => { result = Some(node.data); curr = &node.right; }
        }
    }
    result
}

fn ceiling(tree: &Tree, key: i32) -> Option<i32> {
    let mut curr = tree;
    let mut result = None;
    while let Some(node) = curr {
        match key.cmp(&node.data) {
            Ordering::Equal   => return Some(node.data),
            Ordering::Greater => curr = &node.right,
            Ordering::Less    => { result = Some(node.data); curr = &node.left; }
        }
    }
    result
}

fn rank(tree: &Tree, key: i32) -> usize {
    match tree {
        None => 0,
        Some(node) => match key.cmp(&node.data) {
            Ordering::Less    => rank(&node.left, key),
            Ordering::Greater => 1 + node_size(&node.left) + rank(&node.right, key),
            Ordering::Equal   => node_size(&node.left),
        }
    }
}

fn select(tree: &Tree, k: usize) -> Option<i32> {
    let node = tree.as_ref()?;
    let ls = node_size(&node.left);
    match k.cmp(&ls) {
        Ordering::Less    => select(&node.left, k),
        Ordering::Greater => select(&node.right, k - ls - 1),
        Ordering::Equal   => Some(node.data),
    }
}

fn height(tree: &Tree) -> i32 {
    match tree {
        None => -1,
        Some(node) => 1 + height(&node.left).max(height(&node.right)),
    }
}

fn range_count(tree: &Tree, lo: i32, hi: i32) -> usize {
    match tree {
        None => 0,
        Some(node) => {
            let mut count = 0;
            if node.data > lo { count += range_count(&node.left, lo, hi); }
            if node.data >= lo && node.data <= hi { count += 1; }
            if node.data < hi { count += range_count(&node.right, lo, hi); }
            count
        }
    }
}

fn main() {
    // Demo 1: Búsqueda básica
    println!("=== Demo 1: Búsqueda en BST ===");
    let mut tree: Tree = None;
    for &v in &[20, 10, 30, 5, 15, 25, 35] {
        insert(&mut tree, v);
    }
    println!("min={}, max={}, height={}, size={}",
             bst_min(&tree).unwrap(), bst_max(&tree).unwrap(),
             height(&tree), node_size(&tree));

    for &t in &[15, 25, 12, 35, 1] {
        let (found, cmps) = search_counted(&tree, t);
        println!("  Buscar {:>2}: {} ({} comparaciones)",
                 t, if found { "encontrado" } else { "no encontrado" }, cmps);
    }

    // Demo 2: Floor y Ceiling
    println!("\n=== Demo 2: Floor y Ceiling ===");
    for &q in &[12, 20, 1, 40, 22] {
        println!("  key={:>2}: floor={:?}, ceiling={:?}",
                 q, floor(&tree, q), ceiling(&tree, q));
    }

    // Demo 3: Rank y Select
    println!("\n=== Demo 3: Rank y Select ===");
    for &k in &[15, 20, 12] {
        println!("  Rank({}) = {}", k, rank(&tree, k));
    }
    for k in 0..7 {
        println!("  Select({}) = {:?}", k, select(&tree, k));
    }

    // Demo 4: Aleatorio vs degenerado
    println!("\n=== Demo 4: Aleatorio vs degenerado ===");
    let n = 10_000;

    let mut degen: Tree = None;
    for i in 0..n { insert(&mut degen, i); }
    println!("  Degenerado (n={}): height={}", n, height(&degen));

    // Inserción aleatoria (LCG shuffle)
    let mut data: Vec<i32> = (0..n).collect();
    let mut lcg: u32 = 42;
    for i in (1..data.len()).rev() {
        lcg = lcg.wrapping_mul(1103515245).wrapping_add(12345);
        let j = (lcg as usize >> 1) % (i + 1);
        data.swap(i, j);
    }

    let mut balanced: Tree = None;
    for &v in &data { insert(&mut balanced, v); }
    println!("  Aleatorio  (n={}): height={} (óptimo={})",
             n, height(&balanced), (n as f64).log2() as i32);

    // Demo 5: Comparaciones promedio
    println!("\n=== Demo 5: Comparaciones promedio ===");
    let queries = 1000;
    let mut total_d: u64 = 0;
    let mut total_b: u64 = 0;
    let mut lcg2: u32 = 7;
    for _ in 0..queries {
        lcg2 = lcg2.wrapping_mul(1103515245).wrapping_add(12345);
        let target = (lcg2 >> 1) as i32 % n;
        let (_, c1) = search_counted(&degen, target);
        let (_, c2) = search_counted(&balanced, target);
        total_d += c1 as u64;
        total_b += c2 as u64;
    }
    println!("  Degenerado: {:.1} comparaciones promedio",
             total_d as f64 / queries as f64);
    println!("  Aleatorio:  {:.1} comparaciones promedio",
             total_b as f64 / queries as f64);

    // Demo 6: Rango
    println!("\n=== Demo 6: Búsqueda por rango ===");
    let rc = range_count(&balanced, 4000, 4100);
    println!("  Elementos en [4000, 4100]: {}", rc);
}
```

### Compilar y ejecutar

```bash
rustc -O -o bst_search bst_search.rs
./bst_search
```

---

## Ejercicios

### Ejercicio 1 — Comparaciones en BST balanceado

Dado este BST, ¿cuántas comparaciones se necesitan para buscar
cada valor?

```
         20
        /  \
      10    30
     / \   / \
    5  15 25  35
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

| Valor | Camino | Comparaciones |
|-------|--------|---------------|
| 20 | raíz | 1 |
| 10 | 20→10 | 2 |
| 30 | 20→30 | 2 |
| 5 | 20→10→5 | 3 |
| 15 | 20→10→15 | 3 |
| 25 | 20→30→25 | 3 |
| 35 | 20→30→35 | 3 |

Promedio: $(1 + 2 + 2 + 3 + 3 + 3 + 3) / 7 = 17/7 \approx 2.43$

Este es un BST perfecto de altura 2. La búsqueda binaria en un
array de 7 elementos haría en promedio
$\lfloor \log_2 7 \rfloor \approx 2.8$ comparaciones — muy similar.

Para un valor inexistente (ej. 12): 20→10→15→NULL = 3
comparaciones + 1 verificación de NULL.

</details>

### Ejercicio 2 — BST degenerado vs búsqueda secuencial

Si se insertan $n$ elementos en orden creciente en un BST, ¿cuántas
comparaciones promedio se necesitan para una búsqueda exitosa?
Compara con búsqueda secuencial.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

El BST degenerado es una lista enlazada:

```
1 → 2 → 3 → ... → n
```

Para buscar el elemento en posición $i$ (1-indexed), se necesitan
$i$ comparaciones (recorrer desde la raíz).

Promedio: $\frac{1}{n}\sum_{i=1}^{n} i = \frac{n+1}{2}$

Esto es **idéntico** al promedio de búsqueda secuencial. De hecho,
buscar en un BST degenerado **es** búsqueda secuencial — con el
overhead adicional de seguir punteros (`node->right`) en lugar de
incrementar un índice (`arr[i+1]`).

El BST degenerado es **peor** que la búsqueda secuencial en array
porque:

1. Cada nodo es un malloc separado → cache misses.
2. Seguir punteros es más lento que incrementar un índice.
3. Usa ~3x más memoria (data + left + right por nodo).

Moraleja: un BST sin balanceo puede ser peor que un array en todos
los aspectos. Los árboles autobalanceados (AVL, Red-Black)
garantizan $O(\log n)$ — tema de T02.

</details>

### Ejercicio 3 — Floor y ceiling

En el BST `{5, 10, 15, 20, 25, 30, 35}`, ¿qué retornan `floor` y
`ceiling` para `key = 17`? Traza la ejecución.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Asumiendo BST balanceado:

```
         20
        /  \
      10    30
     / \   / \
    5  15 25  35
```

**floor(17)** — mayor valor ≤ 17:

```
root=20: 17 < 20 → ir a izquierda
root=10: 17 > 10 → result=10, ir a derecha
root=15: 17 > 15 → result=15, ir a derecha
root=NULL → retornar result=15 ✓
```

**ceiling(17)** — menor valor ≥ 17:

```
root=20: 17 < 20 → result=20, ir a izquierda
root=10: 17 > 10 → ir a derecha
root=15: 17 > 15 → ir a derecha
root=NULL → retornar result=20 ✓
```

`floor(17) = 15`, `ceiling(17) = 20`.

En un array ordenado, esto equivale a:
- `lower_bound(17) = 3` (posición de 20) → ceiling = arr[3] = 20
- `upper_bound(17) - 1 = 2` → floor = arr[2] = 15

</details>

### Ejercicio 4 — Rank y select

En el BST con tamaños de subárbol, calcula `rank(22)` y
`select(4)`:

```
         20(7)
        /    \
     10(3)   30(3)
     / \     / \
   5(1) 15(1) 25(1) 35(1)
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**rank(22)** — elementos menores que 22:

```
root=20(7): 22 > 20 → 1 + size(left) + rank(right, 22)
            = 1 + 3 + rank(30-subtree, 22)
root=30(3): 22 < 30 → rank(left, 22)
root=25(1): 22 < 25 → rank(left, 22)
root=NULL → 0

Total: 1 + 3 + 0 = 4
```

Verificación: valores menores que 22 son {5, 10, 15, 20} = 4 ✓

**select(4)** — el 5° menor (0-indexed):

```
root=20(7): ls = size(left) = 3
            k=4 > ls=3 → select(right, 4-3-1) = select(right, 0)
root=30(3): ls = size(left) = 1
            k=0 < ls=1 → select(left, 0)
root=25(1): ls = size(left) = 0
            k=0 == ls=0 → retornar 25
```

select(4) = 25 ✓ (el 5° menor de {5,10,15,20,25,30,35})

</details>

### Ejercicio 5 — Búsqueda por rango

¿Cuántos nodos visita `bst_range(root, 12, 28)` en el BST del
ejercicio anterior? Lista los nodos visitados.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
         20
        /  \
      10    30
     / \   / \
    5  15 25  35
```

Traza de `bst_range(root, 12, 28)`:

```
root=20: 20 > 12 → visitar izquierda
  root=10: 10 < 28 → visitar derecha (no izquierda: 10 < 12 pero condición es >)
    Corrección: 10 > 12? No → no visitar izquierda
    10 en [12,28]? No (10 < 12)
    10 < 28? Sí → visitar derecha
      root=15: 15 > 12? Sí → visitar izquierda (NULL)
               15 en [12,28]? Sí → contar ✓
               15 < 28? Sí → visitar derecha (NULL)
  20 en [12,28]? Sí → contar ✓
  20 < 28? Sí → visitar derecha
    root=30: 30 > 12? Sí → visitar izquierda
      root=25: 25 > 12? Sí → visitar izquierda (NULL)
               25 en [12,28]? Sí → contar ✓
               25 < 28? Sí → visitar derecha (NULL)
    30 en [12,28]? No (30 > 28)
    30 < 28? No → no visitar derecha
```

Nodos visitados: 20, 10, 15, 30, 25 = **5 nodos**.
Resultados en rango: 15, 20, 25 = **3 elementos**.

No visitados: 5 (podado porque 10 < 12 → no ir a izquierda de 10)
y 35 (podado porque 30 > 28 → no ir a derecha de 30).

Complejidad: $O(h + k) = O(2 + 3) = 5$ — correcto.

</details>

### Ejercicio 6 — BST vs array: punto de cruce

Si cada búsqueda en BST cuesta 400 ns (cache misses) y en array
150 ns, pero insertar en array cuesta $n/2 \times 4$ ns (shift) y
en BST $\log_2(n) \times 400$ ns, ¿para qué ratio de
búsquedas/inserciones el BST es mejor?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Sea $q$ el número de búsquedas y $m$ el número de inserciones,
con $n = 10^5$ elementos.

**Costo total array**:
$q \times 150 + m \times (n/2 \times 4) = 150q + 200{,}000m$ ns

**Costo total BST**:
$q \times 400 + m \times (\log_2 n \times 400) = 400q + 6{,}640m$ ns

El BST es mejor cuando:
$400q + 6{,}640m < 150q + 200{,}000m$
$250q < 193{,}360m$
$q/m < 773$

Si hay menos de ~773 búsquedas por cada inserción, el BST gana.

Dicho de otra manera:

- **> 99.87% búsquedas**: array gana (las pocas inserciones no
  compensan el overhead de cache del BST).
- **< 99.87% búsquedas** (> 0.13% inserciones): BST gana.

En la mayoría de aplicaciones dinámicas (bases de datos, índices,
caches), el ratio búsquedas/inserciones está entre 10:1 y 100:1
— claramente en el rango donde el BST (balanceado) es mejor.

</details>

### Ejercicio 7 — Verificar propiedad BST durante búsqueda

Escribe una función que busque un valor y al mismo tiempo verifique
que la propiedad BST se cumple en el camino recorrido.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
Node *bst_search_validate(Node *root, int key, int min, int max,
                           int *valid)
{
    *valid = 1;

    while (root) {
        /* Verificar propiedad BST */
        if (root->data <= min || root->data >= max) {
            *valid = 0;
            return NULL;
        }

        if (key == root->data) return root;

        if (key < root->data) {
            max = root->data;      /* actualizar cota superior */
            root = root->left;
        } else {
            min = root->data;      /* actualizar cota inferior */
            root = root->right;
        }
    }

    return NULL;
}

/* Uso: */
int valid;
Node *found = bst_search_validate(root, 15, INT_MIN, INT_MAX, &valid);
if (!valid) printf("¡Propiedad BST violada!\n");
```

Cada nodo visitado debe estar dentro de las cotas
$(\text{min}, \text{max})$ que se ajustan al descender. Si se va
a la izquierda, el nodo actual se convierte en la cota superior.
Si se va a la derecha, se convierte en la cota inferior.

Esto detecta corrupciones locales del BST a costo cero (las
verificaciones se hacen durante la búsqueda normal, sin recorrido
extra).

</details>

### Ejercicio 8 — Iterador inorden con floor/ceiling

Sin puntero al padre ni stack, usa `ceiling` repetidamente para
implementar un iterador inorden:

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
void inorder_via_ceiling(Node *root)
{
    /* Empezar con el mínimo */
    Node *curr = bst_min(root);
    if (!curr) return;

    printf("%d", curr->data);

    while (1) {
        /* Buscar el menor valor estrictamente mayor que curr->data */
        Node *next = NULL;
        Node *r = root;
        while (r) {
            if (r->data > curr->data) {
                next = r;
                r = r->left;
            } else {
                r = r->right;
            }
        }

        if (!next) break;
        printf(" %d", next->data);
        curr = next;
    }
    printf("\n");
}
```

Complejidad: $O(n \cdot h)$ — cada llamada a ceiling es $O(h)$,
y hay $n$ llamadas. Para un BST balanceado es $O(n \log n)$, peor
que el $O(n)$ del recorrido con stack o recursión.

Esta técnica es útil solo cuando no se puede mantener estado
(stack/puntero al padre) entre llamadas — por ejemplo, en una API
donde el usuario pide "el siguiente elemento" en momentos
arbitrarios.

</details>

### Ejercicio 9 — BST en Rust: BTreeMap

¿Cuál es la diferencia entre implementar un BST manualmente en Rust
vs usar `BTreeMap`?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
use std::collections::BTreeMap;

let mut map = BTreeMap::new();
map.insert(20, "veinte");
map.insert(10, "diez");
map.insert(30, "treinta");

// Búsqueda: O(log n)
assert_eq!(map.get(&20), Some(&"veinte"));
assert_eq!(map.get(&15), None);

// Rango: iterador sobre [lo, hi]
for (k, v) in map.range(10..=25) {
    println!("{}: {}", k, v);
}
// 10: diez
// 20: veinte
```

| Aspecto | BST manual | `BTreeMap` |
|---------|-----------|------------|
| Estructura | Nodos binarios en heap | B-tree (nodos con múltiples claves) |
| Cache | Malo (nodos dispersos) | Bueno (claves contiguas en nodo) |
| Balanceo | Manual o ninguno | Automático |
| API | Funciones propias | `.get`, `.insert`, `.range`, etc. |
| Key-value | Solo key (sin adaptar) | Key-value nativo |
| Seguridad | `unsafe` probable | Safe |
| Rendimiento | Peor (cache misses) | Mejor (cache-friendly) |

`BTreeMap` es **siempre preferible** en Rust a un BST manual:

1. Está implementado como B-tree (no BST) → mejor cache.
2. Está balanceado automáticamente → $O(\log n)$ garantizado.
3. API completa con rangos, iteradores, entry API.
4. Probado exhaustivamente, sin bugs de memoria.

Un BST manual en Rust solo tiene sentido como ejercicio educativo
(como en C06/S03) o si se necesita una variante específica (splay
tree, treap) no disponible en la stdlib.

</details>

### Ejercicio 10 — Comparación empírica de 4 estructuras

Para $10^4$ inserciones seguidas de $10^4$ búsquedas, predice el
orden de velocidad entre: array ordenado + binaria, BST no
balanceado (inserción aleatoria), `BTreeMap` de Rust, y `HashMap`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Fase de inserción** ($10^4$ inserciones):

| Estructura | Costo por inserción | Total |
|------------|--------------------:|------:|
| Array ordenado | $O(n)$ shift | $O(n^2)$ ≈ $5 \times 10^7$ ops |
| BST aleatorio | $O(\log n)$ | $O(n \log n)$ ≈ $1.4 \times 10^5$ ops |
| `BTreeMap` | $O(\log n)$ | $O(n \log n)$ ≈ $1.4 \times 10^5$ ops |
| `HashMap` | $O(1)$ amortizado | $O(n)$ ≈ $10^4$ ops |

**Fase de búsqueda** ($10^4$ búsquedas):

| Estructura | Costo por búsqueda | Total |
|------------|-------------------:|------:|
| Array + binaria | $O(\log n)$, excelente cache | ~150 ns × $10^4$ |
| BST | $O(\log n)$, mal cache | ~400 ns × $10^4$ |
| `BTreeMap` | $O(\log n)$, buen cache | ~200 ns × $10^4$ |
| `HashMap` | $O(1)$, buen cache | ~50 ns × $10^4$ |

**Orden total** (inserción + búsqueda):

1. **`HashMap`**: más rápido — $O(1)$ en ambas fases.
2. **`BTreeMap`**: segundo — $O(\log n)$ con buen cache.
3. **BST aleatorio**: tercero — $O(\log n)$ pero mal cache.
4. **Array ordenado**: último — la fase de inserción $O(n^2)$ domina.

Si las inserciones se hacen todas antes de las búsquedas (bulk
insert), el array puede usar sort una vez ($O(n \log n)$) en lugar
de $n$ inserciones ordenadas ($O(n^2)$), lo que lo pone en segundo
lugar.

</details>

---

## Resumen

| Aspecto | BST | Array + binaria | Hash table |
|---------|-----|----------------|------------|
| Búsqueda | $O(h)$ — $O(\log n)$ a $O(n)$ | $O(\log n)$ siempre | $O(1)$ amortizado |
| Inserción | $O(h)$ | $O(n)$ (shift) | $O(1)$ amortizado |
| Eliminación | $O(h)$ | $O(n)$ (shift) | $O(1)$ amortizado |
| Rango | $O(h + k)$ | $O(\log n + k)$ | No soportado |
| Floor/Ceiling | $O(h)$ | $O(\log n)$ | No soportado |
| Rank/Select | $O(h)$ con size | $O(\log n)$ / $O(1)$ | No soportado |
| Cache | Malo | Excelente | Bueno |
| Peor caso | $O(n)$ sin balanceo | $O(\log n)$ | $O(n)$ |
| Mejor para | Datos dinámicos con rangos | Datos estáticos | Búsqueda exacta |
