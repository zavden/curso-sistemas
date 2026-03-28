# Degeneración y balanceo

## El problema

Un BST promete $O(\log n)$ para búsqueda, inserción y eliminación —
pero solo si está **balanceado**.  En el peor caso, un BST degenera a
una lista enlazada con $O(n)$ por operación.

```
Balanceado (h=2):              Degenerado (h=6):

         10                    1
        /  \                    \
       5    15                   2
      / \   / \                   \
     3   7 12  20                  3
                                    \
                                     4
                                      \
                                       5
                                        \
                                         6
                                          \
                                           7
```

Mismos 7 valores, mismos 7 nodos.  Pero la diferencia en altura (2 vs 6)
es la diferencia entre $O(\log n)$ y $O(n)$.

---

## Cuándo ocurre la degeneración

La forma del BST depende exclusivamente del **orden de inserción**.
Hay tres patrones que producen degeneración:

### 1. Inserción en orden ascendente

```
Insertar: 1, 2, 3, 4, 5

  1
   \
    2
     \
      3
       \
        4
         \
          5

Cada valor es mayor que todos los anteriores → siempre va a la derecha.
```

### 2. Inserción en orden descendente

```
Insertar: 5, 4, 3, 2, 1

          5
         /
        4
       /
      3
     /
    2
   /
  1

Cada valor es menor que todos los anteriores → siempre va a la izquierda.
```

### 3. Inserción en zigzag

```
Insertar: 1, 5, 2, 4, 3

  1
   \
    5
   /
  2
   \
    4
   /
  3

Alterna derecha-izquierda, pero sigue siendo h = n-1.
```

En los tres casos, la altura es $n - 1$ y todas las operaciones son
$O(n)$.

---

## Demostración empírica

Insertemos los valores del 1 al 15 en distintos órdenes y comparemos
alturas:

| Orden de inserción | Altura | Tipo |
|-------------------|--------|------|
| `1, 2, 3, ..., 15` | 14 | Degenerado |
| `15, 14, 13, ..., 1` | 14 | Degenerado |
| `8, 4, 12, 2, 6, 10, 14, 1, 3, 5, 7, 9, 11, 13, 15` | 3 | Perfecto |
| Aleatorio (promedio) | ~5 | Casi balanceado |

La altura mínima posible para 15 nodos es $\lfloor \log_2 15 \rfloor = 3$.
El degenerado tiene altura 14 — casi 5 veces peor.

### Programa de demostración

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

TreeNode *create_node(int value) {
    TreeNode *n = malloc(sizeof(TreeNode));
    n->data = value;
    n->left = n->right = NULL;
    return n;
}

TreeNode *bst_insert(TreeNode *root, int value) {
    if (!root) return create_node(value);
    if (value < root->data)
        root->left = bst_insert(root->left, value);
    else if (value > root->data)
        root->right = bst_insert(root->right, value);
    return root;
}

int tree_height(TreeNode *root) {
    if (!root) return -1;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

// Fisher-Yates shuffle
void shuffle(int *arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

int main(void) {
    srand(time(NULL));
    int n = 1000;
    int *arr = malloc(n * sizeof(int));

    // Sorted insertion
    TreeNode *sorted_tree = NULL;
    for (int i = 1; i <= n; i++)
        sorted_tree = bst_insert(sorted_tree, i);
    printf("Sorted (n=%d):  height = %d\n", n, tree_height(sorted_tree));
    tree_free(sorted_tree);

    // Random insertion (5 trials)
    for (int t = 0; t < 5; t++) {
        for (int i = 0; i < n; i++) arr[i] = i + 1;
        shuffle(arr, n);

        TreeNode *rand_tree = NULL;
        for (int i = 0; i < n; i++)
            rand_tree = bst_insert(rand_tree, arr[i]);
        printf("Random trial %d: height = %d\n", t + 1, tree_height(rand_tree));
        tree_free(rand_tree);
    }

    // Balanced insertion (sorted array → BST)
    for (int i = 0; i < n; i++) arr[i] = i + 1;
    // Use middle element first
    TreeNode *bal_tree = NULL;
    // Simple approach: just show what sorted_to_bst produces
    printf("Optimal:        height = %d (floor(log2(%d)))\n",
           (int)(log2(n)), n);

    free(arr);
    return 0;
}
```

Salida típica:

```
Sorted (n=1000):  height = 999
Random trial 1:   height = 21
Random trial 2:   height = 23
Random trial 3:   height = 20
Random trial 4:   height = 22
Random trial 5:   height = 21
Optimal:          height = 9 (floor(log2(1000)))
```

Altura 999 vs ~21 vs 9.  La inserción aleatoria produce alturas
razonables (~$2 \times \log_2 n$), pero la inserción ordenada es
catastrófica.

---

## Impacto en rendimiento

Para $n = 10^6$ nodos:

| Operación | BST balanceado ($h \approx 20$) | BST degenerado ($h = 10^6$) | Factor |
|-----------|-------------------------------|---------------------------|--------|
| Búsqueda | ~20 comparaciones | ~500000 promedio | 25000× |
| Inserción | ~20 comparaciones | ~$10^6$ | 50000× |
| Construir $n$ nodos | $O(n \log n) \approx 2 \times 10^7$ | $O(n^2) = 10^{12}$ | 50000× |

El BST degenerado no solo es asintóticamente peor — es **inutilizable**
para tamaños prácticos.  Construir un BST de $10^6$ elementos en
orden: $10^{12}$ operaciones ≈ horas.  En orden aleatorio: $2 \times 10^7$
operaciones ≈ milisegundos.

---

## Fuentes de datos ordenados en la práctica

La degeneración no es un caso teórico — datos (casi) ordenados son
comunes:

| Fuente | Ejemplo |
|--------|---------|
| IDs autoincrement | INSERT de registros con id = 1, 2, 3, ... |
| Timestamps | Eventos cronológicos |
| Datos ya ordenados | Lecturas desde un archivo sorted |
| Inserción post-ordenación | sort + insert en BST |
| Datos casi-ordenados | Logs con timestamps ligeramente desordenados |
| Eliminaciones sesgadas | Eliminar siempre el mínimo → sesgo a la derecha |

---

## Soluciones a la degeneración

### 1. Aleatorizar el orden de inserción

Si se tienen todos los datos de antemano, barajar antes de insertar:

```c
shuffle(arr, n);
for (int i = 0; i < n; i++)
    root = bst_insert(root, arr[i]);
```

Altura esperada: $\approx 1.39 \log_2 n$.  Simple pero requiere tener
todos los datos al inicio — no sirve para inserciones incrementales.

### 2. Construir balanceado desde array ordenado

Si los datos ya están ordenados, usar divide y vencerás:

```c
TreeNode *sorted_to_bst(int *arr, int start, int end) {
    if (start > end) return NULL;
    int mid = start + (end - start) / 2;
    TreeNode *node = create_node(arr[mid]);
    node->left = sorted_to_bst(arr, start, mid - 1);
    node->right = sorted_to_bst(arr, mid + 1, end);
    return node;
}
```

```rust
fn sorted_to_bst(arr: &[i32]) -> Tree<i32> {
    if arr.is_empty() { return None; }
    let mid = arr.len() / 2;
    Some(Box::new(TreeNode {
        data: arr[mid],
        left: sorted_to_bst(&arr[..mid]),
        right: sorted_to_bst(&arr[mid + 1..]),
    }))
}
```

Produce un árbol con altura mínima.  $O(n)$.  Pero es una construcción
puntual — inserciones posteriores pueden desbalancear.

### 3. Rebalanceo periódico

Rebalancear un BST existente:

1. Recolectar inorden → array ordenado.  $O(n)$.
2. Reconstruir con `sorted_to_bst`.  $O(n)$.
3. Liberar el árbol anterior.  $O(n)$.

Total: $O(n)$.  Se puede hacer cada $k$ operaciones.  Simple pero
costoso — todo el árbol se reconstruye.

```c
TreeNode *rebalance(TreeNode *root) {
    // Paso 1: inorden → array
    int arr[MAX_N];
    int count = 0;
    inorder_to_array(root, arr, &count);

    // Paso 2: liberar el viejo
    tree_free(root);

    // Paso 3: reconstruir balanceado
    return sorted_to_bst(arr, 0, count - 1);
}
```

### 4. Árboles auto-balanceados

La solución definitiva: mantener el balance **automáticamente** después
de cada inserción y eliminación.  Dos familias principales:

| Árbol | Invariante de balance | Operación de rebalanceo |
|-------|---------------------|----------------------|
| **AVL** | Para cada nodo: $\|h_{izq} - h_{der}\| \leq 1$ | Rotaciones (1 o 2 por operación) |
| **Rojinegro** | 5 propiedades de color | Recoloreo + rotaciones |

Ambos garantizan $h = O(\log n)$ **siempre**, independientemente del
orden de inserción.  Son el tema de S04.

---

## Rotaciones: la operación fundamental

Las rotaciones son la herramienta para rebalancear sin romper la
propiedad BST.  Hay dos rotaciones básicas:

### Rotación a la izquierda

```
Antes:                  Después:

    x                      y
     \                    / \
      y        →         x   c
     / \                  \
    b   c                  b

Condición: y es hijo derecho de x.
```

```c
TreeNode *rotate_left(TreeNode *x) {
    TreeNode *y = x->right;
    x->right = y->left;     // b pasa a ser right de x
    y->left = x;             // x pasa a ser left de y
    return y;                // y es la nueva raíz del subárbol
}
```

### Rotación a la derecha

```
Antes:                  Después:

      y                    x
     /                    / \
    x          →         a   y
   / \                      /
  a   b                    b

Condición: x es hijo izquierdo de y.
```

```c
TreeNode *rotate_right(TreeNode *y) {
    TreeNode *x = y->left;
    y->left = x->right;     // b pasa a ser left de y
    x->right = y;            // y pasa a ser right de x
    return x;                // x es la nueva raíz del subárbol
}
```

### Propiedad clave

Las rotaciones **preservan la propiedad BST**.  El inorden no cambia:

```
Antes (inorden):  ... a x b y c ...
Después (inorden): ... a x b y c ...
```

Solo cambia la estructura (quién es padre de quién), no el orden.

### Ejemplo: corregir un desbalanceo

```
Insertar 1, 2, 3 (degenerado):

  1                    2
   \     rotate_left  / \
    2    ─────────→  1   3
     \
      3

Altura: 2 → 1.  Balanceado.
```

---

## Comparación de soluciones

| Solución | Complejidad por operación | Garantía | Complejidad extra |
|----------|-------------------------|----------|------------------|
| BST simple | $O(h)$, peor $O(n)$ | Ninguna | Ninguna |
| Aleatorizar | $O(\log n)$ esperado | Probabilística | Requiere datos de antemano |
| `sorted_to_bst` | $O(n)$ única vez | Solo inicial | No mantiene tras inserciones |
| Rebalanceo periódico | $O(n)$ cada $k$ ops | Amortizada | Pausas periódicas |
| **AVL** | $O(\log n)$ siempre | Determinista | 1 int (altura) por nodo |
| **Rojinegro** | $O(\log n)$ siempre | Determinista | 1 bit (color) por nodo |

---

## Demostración en Rust

```rust
use std::cmp::Ordering;

type Tree<T> = Option<Box<Node<T>>>;

struct Node<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}

impl<T> Node<T> {
    fn leaf(data: T) -> Tree<T> {
        Some(Box::new(Node { data, left: None, right: None }))
    }
}

fn bst_insert<T: Ord>(tree: &mut Tree<T>, value: T) {
    match tree {
        None => *tree = Node::leaf(value),
        Some(node) => match value.cmp(&node.data) {
            Ordering::Less    => bst_insert(&mut node.left, value),
            Ordering::Greater => bst_insert(&mut node.right, value),
            Ordering::Equal   => {}
        },
    }
}

fn tree_height<T>(tree: &Tree<T>) -> i32 {
    match tree {
        None => -1,
        Some(node) => {
            1 + tree_height(&node.left).max(tree_height(&node.right))
        }
    }
}

fn sorted_to_bst(arr: &[i32]) -> Tree<i32> {
    if arr.is_empty() { return None; }
    let mid = arr.len() / 2;
    Some(Box::new(Node {
        data: arr[mid],
        left: sorted_to_bst(&arr[..mid]),
        right: sorted_to_bst(&arr[mid + 1..]),
    }))
}

fn inorder_vec<T: Clone>(tree: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    fn collect<T: Clone>(tree: &Tree<T>, out: &mut Vec<T>) {
        if let Some(node) = tree {
            collect(&node.left, out);
            out.push(node.data.clone());
            collect(&node.right, out);
        }
    }
    collect(tree, &mut result);
    result
}

fn rebalance(tree: &mut Tree<i32>) {
    let values = inorder_vec(tree);
    *tree = sorted_to_bst(&values);
}

fn main() {
    // Inserción ordenada → degenerado
    let mut degenerate: Tree<i32> = None;
    for i in 1..=15 {
        bst_insert(&mut degenerate, i);
    }
    println!("Sorted insert (1..15):");
    println!("  height = {}", tree_height(&degenerate));

    // Rebalancear
    rebalance(&mut degenerate);
    println!("After rebalance:");
    println!("  height = {}", tree_height(&degenerate));
    println!("  inorder = {:?}", inorder_vec(&degenerate));

    // Construir balanceado desde el inicio
    let arr: Vec<i32> = (1..=15).collect();
    let balanced = sorted_to_bst(&arr);
    println!("\nsorted_to_bst(1..15):");
    println!("  height = {}", tree_height(&balanced));

    // Comparar tamaños
    for &n in &[100, 1000, 10000] {
        let mut deg: Tree<i32> = None;
        for i in 1..=n {
            bst_insert(&mut deg, i);
        }
        let arr: Vec<i32> = (1..=n).collect();
        let bal = sorted_to_bst(&arr);
        println!("\nn={n}:  degenerate h={}, balanced h={}",
                 tree_height(&deg), tree_height(&bal));
    }
}
```

Salida:

```
Sorted insert (1..15):
  height = 14
After rebalance:
  height = 3
  inorder = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]

sorted_to_bst(1..15):
  height = 3

n=100:  degenerate h=99, balanced h=6
n=1000:  degenerate h=999, balanced h=9
n=10000:  degenerate h=9999, balanced h=13
```

---

## Hacia S04: árboles auto-balanceados

El BST simple es el fundamento, pero sus limitaciones son claras.  Los
árboles auto-balanceados (S04) resuelven el problema de raíz:

```
BST simple               AVL / Rojinegro
────────────              ─────────────────
Insertar 1..7:            Insertar 1..7:

  1                              4
   \                           /   \
    2                         2     6
     \          vs           / \   / \
      3                     1   3 5   7
       \
        4                  h = 2, siempre O(log n)
         \
          5
           \
            6
             \
              7

  h = 6, operaciones O(n)
```

La clave es que las **rotaciones** (izquierda/derecha) reestructuran el
árbol después de cada inserción/eliminación sin romper la propiedad BST.
Los árboles AVL y rojinegros definen **cuándo** y **qué tipo** de
rotación aplicar.

| Concepto | BST simple | AVL | Rojinegro |
|----------|-----------|-----|-----------|
| Invariante | Propiedad BST | BST + balance ≤ 1 | BST + 5 propiedades color |
| Altura garantizada | $O(n)$ peor caso | $\leq 1.44 \log_2(n+2)$ | $\leq 2 \log_2(n+1)$ |
| Rotaciones por insert | 0 | Hasta 2 | Hasta 2 |
| Rotaciones por delete | 0 | Hasta $O(\log n)$ | Hasta 3 |
| Implementación | Simple | Moderada | Compleja |
| Uso real | Educativo | Bases de datos, mapa | Kernel Linux, Java TreeMap |

---

## Ejercicios

### Ejercicio 1 — Predecir la altura

¿Cuál es la altura del BST resultante de insertar
`5, 10, 15, 20, 25, 30, 35`?  ¿Y de `20, 10, 30, 5, 15, 25, 35`?

<details>
<summary>Predicción</summary>

`5, 10, 15, 20, 25, 30, 35` (ascendente):
```
  5
   \
    10
     \
      15
       \
        ...
```
Altura = **6** (degenerado a la derecha).

`20, 10, 30, 5, 15, 25, 35` (BFS order):
```
         20
        /  \
      10    30
      / \   / \
     5  15 25  35
```
Altura = **2** (perfecto).

Mismo inorden `[5, 10, 15, 20, 25, 30, 35]`, alturas 6 vs 2.
</details>

### Ejercicio 2 — Peor caso para n nodos

Para $n$ nodos, ¿cuántos órdenes de inserción producen un BST
degenerado (altura $n - 1$)?

<details>
<summary>Respuesta</summary>

Un BST de altura $n - 1$ es una cadena — cada nodo tiene exactamente un
hijo.  En cada paso de la inserción, el nuevo valor debe ser o el
mayor o el menor de todos los restantes (para ir siempre a un extremo).

Hay exactamente $2^{n-1}$ órdenes que producen un degenerado:
- El primer valor puede ser cualquiera de los $n$ (pero fija la raíz).
- Error: en realidad, para una cadena pura, el primer valor debe ser un
  extremo (el mínimo o el máximo).

Corrección: los órdenes que producen un degenerado son exactamente
$2^{n-1}$.  En cada paso (excepto el primero), el siguiente valor
puede ser el mínimo o máximo de los valores restantes — 2 opciones
por paso, $n - 1$ pasos → $2^{n-1}$.

De las $n!$ permutaciones totales, solo $2^{n-1}$ producen el peor caso.
Para $n = 10$: $2^9 = 512$ de $10! = 3628800$ → ~0.014%.  Es raro
aleatoriamente, pero común con datos reales ordenados.
</details>

### Ejercicio 3 — sorted_to_bst

Construye manualmente el BST que produce `sorted_to_bst` para el array
`[1, 2, 3, 4, 5, 6, 7]`.

<details>
<summary>Resultado</summary>

```
mid = 3 → raíz = 4
  left: [1, 2, 3], mid = 1 → raíz = 2
    left: [1], mid = 0 → hoja 1
    right: [3], mid = 0 → hoja 3
  right: [5, 6, 7], mid = 1 → raíz = 6
    left: [5], mid = 0 → hoja 5
    right: [7], mid = 0 → hoja 7
```

```
         4
        / \
       2    6
      / \  / \
     1  3 5   7
```

Árbol perfecto, altura 2 — mínima posible para 7 nodos.
</details>

### Ejercicio 4 — Rotación a mano

Aplica una rotación a la izquierda sobre el nodo 1:

```
  1
   \
    2
   / \
  b   3
```

(donde `b` es un subárbol posiblemente vacío)

<details>
<summary>Resultado</summary>

```
    2
   / \
  1   3
   \
    b
```

El nodo 2 sube a ser raíz.  El subárbol `b` (que era `left` de 2) pasa
a ser `right` de 1.  Inorden antes: `1 b 2 3`.  Inorden después:
`1 b 2 3`.  No cambió — la rotación preserva el orden BST.
</details>

### Ejercicio 5 — Rebalancear degenerado

Un BST degenerado contiene `[1, 2, 3, 4, 5]`.  Aplica el algoritmo de
rebalanceo (inorden → sorted_to_bst).  ¿Cuál es la altura resultante?

<details>
<summary>Resultado</summary>

Inorden: `[1, 2, 3, 4, 5]`.

sorted_to_bst: mid=2 → raíz=3.

```
       3
      / \
     2    5
    /    /
   1    4
```

Altura: **2**.  Antes era 4 (degenerado).

Notar que para 5 nodos no se puede lograr un árbol perfecto (necesita
$2^{h+1} - 1$ nodos = 7).  Altura 2 es la mínima posible:
$\lfloor \log_2 5 \rfloor = 2$.
</details>

### Ejercicio 6 — Cuántas rotaciones

Para convertir este degenerado en balanceado, ¿cuántas rotaciones
izquierdas se necesitan?

```
  1
   \
    2
     \
      3
```

<details>
<summary>Análisis</summary>

Una rotación izquierda sobre 1:

```
    2
   / \
  1   3
```

**1 rotación** basta para balancear 3 nodos.

Para $n$ nodos en degenerado, se necesitan $O(n)$ rotaciones para
balancear completamente (el algoritmo DSW — Day-Stout-Warren — usa
exactamente $n - 1$ rotaciones derechas para aplanar + $\lfloor \log_2 n
\rfloor$ fases de rotaciones izquierdas).

Las rotaciones son $O(1)$ cada una, así que el rebalanceo total es
$O(n)$ — igual que el método inorden + sorted_to_bst, pero sin
necesitar memoria extra para el array.
</details>

### Ejercicio 7 — Comparar tiempos de construcción

Calcula el número total de comparaciones para construir un BST de
$n = 1023$ ($= 2^{10} - 1$) nodos con inserción ordenada vs
inserción BFS-order (que produce un árbol perfecto).

<details>
<summary>Cálculos</summary>

**Ordenada**: insertar el $i$-ésimo elemento requiere $i - 1$
comparaciones (recorrer toda la cadena).

$$\sum_{i=1}^{1023} (i-1) = \frac{1022 \times 1023}{2} = 522753$$

**BFS-order** (perfecto): insertar el $i$-ésimo elemento requiere
$\lfloor \log_2 i \rfloor$ comparaciones (profundidad en un árbol
balanceado).

$$\sum_{i=1}^{1023} \lfloor \log_2 i \rfloor \approx 8175$$

Ratio: $522753 / 8175 \approx 64\times$ más comparaciones para la
inserción ordenada.  Para $n = 10^6$, el ratio sería $\approx 25000\times$.
</details>

### Ejercicio 8 — Detectar degeneración

Implementa una función que determine si un BST es "peligrosamente
degenerado" — si su altura es mayor que $3 \times \lfloor \log_2 n
\rfloor$.

<details>
<summary>Implementación</summary>

```rust
fn is_degenerate<T>(tree: &Tree<T>) -> bool {
    let h = tree_height(tree);
    let n = tree_count(tree);
    if n <= 1 { return false; }

    let threshold = 3 * ((n as f64).log2() as i32);
    h > threshold
}

fn tree_count<T>(tree: &Tree<T>) -> usize {
    match tree {
        None => 0,
        Some(node) => 1 + tree_count(&node.left) + tree_count(&node.right),
    }
}
```

Para $n = 1000$: threshold = $3 \times 9 = 27$.  Altura aleatoria
~21 → no degenerado.  Altura sorted = 999 → degenerado.

Esto podría disparar un rebalanceo automático.
</details>

### Ejercicio 9 — Inserción aleatoria en Rust

Usa el crate `rand` (o un generador simple) para insertar 1000 valores
en orden aleatorio.  Compara la altura con la inserción ordenada y con
`sorted_to_bst`.

<details>
<summary>Código conceptual</summary>

```rust
// Sin crate rand, usar un LCG simple:
fn lcg_shuffle(arr: &mut [i32]) {
    let mut seed: u64 = 12345;
    for i in (1..arr.len()).rev() {
        seed = seed.wrapping_mul(6364136223846793005)
                   .wrapping_add(1442695040888963407);
        let j = (seed >> 33) as usize % (i + 1);
        arr.swap(i, j);
    }
}

fn main() {
    let n = 1000;
    let mut arr: Vec<i32> = (1..=n).collect();

    // Ordenado
    let mut t1: Tree<i32> = None;
    for i in 1..=n { bst_insert(&mut t1, i); }
    println!("sorted:   h={}", tree_height(&t1));

    // Aleatorio
    lcg_shuffle(&mut arr);
    let mut t2: Tree<i32> = None;
    for &v in &arr { bst_insert(&mut t2, v); }
    println!("random:   h={}", tree_height(&t2));

    // Balanceado
    let sorted: Vec<i32> = (1..=n).collect();
    let t3 = sorted_to_bst(&sorted);
    println!("balanced: h={}", tree_height(&t3));
}
```

Salida típica:
```
sorted:   h=999
random:   h=22
balanced: h=9
```
</details>

### Ejercicio 10 — Motivar AVL

Después de construir un BST balanceado con `sorted_to_bst([1..15])`,
inserta los valores 16, 17, 18, 19, 20.  ¿Cuál es la nueva altura?
¿El árbol sigue balanceado?

<details>
<summary>Análisis</summary>

El BST balanceado de `[1..15]`:
```
              8
           /     \
         4        12
        / \       / \
       2   6    10   14
      /\ / \   /\   / \
     1 3 5 7  9 11 13 15
```
Altura = 3 (perfecto).

Insertar 16: va a `15 → right`.  Insertar 17: va a `16 → right`.
Etc.

```
              8
           /     \
         4        12
        / \       / \
       2   6    10   14
      /\ / \   /\   / \
     1 3 5 7  9 11 13 15
                         \
                          16
                           \
                            17
                             \
                              18
                               \
                                19
                                 \
                                  20
```

Altura: **8** (de 3 a 8 con solo 5 inserciones).  El subárbol
derecho de 15 es una cadena de 5 nodos.

Un AVL habría rebalanceado después de cada inserción, manteniendo
la altura en $\lfloor \log_2 20 \rfloor = 4$.

Este ejemplo demuestra que `sorted_to_bst` solo resuelve el balance
inicial — **inserciones posteriores pueden desbalancear**.  Solo los
árboles auto-balanceados mantienen $O(\log n)$ de forma permanente.
</details>
