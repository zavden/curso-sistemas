# Inserción en BST

## Principio

Insertar en un BST es **buscar la posición** donde debería estar el
valor y, al llegar a un NULL, crear un nodo hoja ahí.  El nuevo nodo
siempre se inserta como hoja — nunca se reorganiza la estructura
existente.

```
Insertar 9 en:               Resultado:

         10                        10
        /  \                      /  \
       5    15                   5    15
      / \   / \                 / \   / \
     3   7 12  20              3   7 12  20
                                    \
                                     9
```

Camino: `10 → 5 → 7 → right(7) = NULL → crear nodo(9)`.

- 9 < 10 → izquierda.
- 9 > 5 → derecha.
- 9 > 7 → derecha → NULL → insertar aquí.

---

## Inserción recursiva

### En C

```c
TreeNode *bst_insert(TreeNode *root, int value) {
    if (!root) return create_node(value);

    if (value < root->data)
        root->left = bst_insert(root->left, value);
    else if (value > root->data)
        root->right = bst_insert(root->right, value);
    // Si value == root->data: duplicado, no insertar

    return root;
}
```

La función retorna la raíz (posiblemente nueva si el árbol estaba
vacío).  El patrón `root->left = bst_insert(root->left, value)` es
crucial: la recursión desciende hasta NULL, crea el nodo, y lo
**enlaza** al padre en el retorno.

### Uso

```c
TreeNode *root = NULL;                // árbol vacío
root = bst_insert(root, 10);          // root = nodo(10)
root = bst_insert(root, 5);           // 5 < 10 → izquierda
root = bst_insert(root, 15);          // 15 > 10 → derecha
root = bst_insert(root, 3);           // 3 < 10 < 5 → izq de 5
root = bst_insert(root, 7);           // 7 < 10 > 5 → der de 5
```

### En Rust

```rust
type Tree<T> = Option<Box<TreeNode<T>>>;

struct TreeNode<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}

fn bst_insert(tree: &mut Tree<i32>, value: i32) {
    match tree {
        None => {
            *tree = Some(Box::new(TreeNode {
                data: value,
                left: None,
                right: None,
            }));
        }
        Some(node) => {
            if value < node.data {
                bst_insert(&mut node.left, value);
            } else if value > node.data {
                bst_insert(&mut node.right, value);
            }
            // duplicado: no insertar
        }
    }
}
```

La versión Rust modifica in-place con `&mut Tree<i32>`.  No necesita
retornar nada — el `*tree = Some(...)` escribe directamente en el
puntero del padre.

---

## Traza detallada

Construir el BST de referencia insertando: `10, 5, 15, 3, 7, 12, 20`.

```
Insertar 10:
  root = NULL → crear nodo(10).

         10

Insertar 5:
  10: 5 < 10 → left.
  left = NULL → crear nodo(5).

         10
        /
       5

Insertar 15:
  10: 15 > 10 → right.
  right = NULL → crear nodo(15).

         10
        /  \
       5    15

Insertar 3:
  10: 3 < 10 → left.
  5:  3 < 5  → left.
  left = NULL → crear nodo(3).

         10
        /  \
       5    15
      /
     3

Insertar 7:
  10: 7 < 10 → left.
  5:  7 > 5  → right.
  right = NULL → crear nodo(7).

         10
        /  \
       5    15
      / \
     3   7

Insertar 12:
  10: 12 > 10 → right.
  15: 12 < 15 → left.
  left = NULL → crear nodo(12).

         10
        /  \
       5    15
      / \   /
     3   7 12

Insertar 20:
  10: 20 > 10 → right.
  15: 20 > 15 → right.
  right = NULL → crear nodo(20).

         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

7 inserciones, 7 nodos.  La forma del árbol depende del **orden de
inserción**, no de los valores.

---

## El orden de inserción determina la forma

Los mismos 7 valores producen árboles muy diferentes según el orden:

### Orden: `10, 5, 15, 3, 7, 12, 20` → balanceado

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20

Altura: 2
```

### Orden: `3, 5, 7, 10, 12, 15, 20` → degenerado

```
  3
   \
    5
     \
      7
       \
        10
         \
          12
           \
            15
             \
              20

Altura: 6
```

### Orden: `20, 15, 12, 10, 7, 5, 3` → degenerado (espejo)

```
              20
             /
            15
           /
          12
         /
        10
       /
      7
     /
    5
   /
  3

Altura: 6
```

Los tres producen el mismo inorden (`3 5 7 10 12 15 20`), pero alturas
radicalmente distintas.  Insertar en orden **ascendente o descendente**
siempre produce un degenerado.

---

## Inserción iterativa

La versión iterativa busca con un puntero `parent` para saber dónde
enlazar el nuevo nodo:

### En C

```c
TreeNode *bst_insert_iter(TreeNode *root, int value) {
    TreeNode *node = create_node(value);

    if (!root) return node;

    TreeNode *current = root;
    TreeNode *parent = NULL;

    while (current) {
        parent = current;
        if (value < current->data)
            current = current->left;
        else if (value > current->data)
            current = current->right;
        else {
            // Duplicado
            free(node);
            return root;
        }
    }

    if (value < parent->data)
        parent->left = node;
    else
        parent->right = node;

    return root;
}
```

### Traza iterativa: insertar 9

```
current = 10, parent = NULL
  9 < 10 → current = left(5)

current = 5, parent = 10
  9 > 5 → current = right(7)

current = 7, parent = 5
  9 > 7 → current = right(NULL)

current = NULL, parent = 7
  9 > 7 → parent->right = nodo(9)
```

### En Rust

```rust
fn bst_insert_iter(tree: &mut Tree<i32>, value: i32) {
    let mut current = tree;

    loop {
        match current {
            None => {
                *current = Some(Box::new(TreeNode {
                    data: value,
                    left: None,
                    right: None,
                }));
                return;
            }
            Some(node) => {
                if value < node.data {
                    current = &mut node.left;
                } else if value > node.data {
                    current = &mut node.right;
                } else {
                    return;  // duplicado
                }
            }
        }
    }
}
```

La versión Rust usa `&mut Tree<i32>` que se reasigna en cada paso del
loop.  Cuando llega a `None`, escribe directamente.  No necesita
variable `parent` — el sistema de referencias de Rust maneja el enlace
automáticamente.

---

## Recursiva vs iterativa

| Aspecto | Recursiva | Iterativa |
|---------|-----------|-----------|
| Legibilidad | Alta | Media |
| Stack overflow | Posible en degenerado ($h > 10^4$) | No |
| Variable `parent` | No necesaria (el retorno enlaza) | Necesaria en C |
| Rendimiento | Overhead de llamadas | Ligeramente más rápida |
| Estilo Rust | `&mut` recursivo (natural) | `&mut` en loop (también natural) |

Para árboles que podrían degenerar, preferir la versión iterativa.
Para código claro y conciso, la recursiva es preferible.

---

## Inserción con puntero a puntero

En C, se puede evitar el caso especial de `root == NULL` usando un
puntero a puntero (`TreeNode **`):

```c
void bst_insert_pp(TreeNode **root, int value) {
    while (*root) {
        if (value < (*root)->data)
            root = &(*root)->left;
        else if (value > (*root)->data)
            root = &(*root)->right;
        else
            return;  // duplicado
    }

    *root = create_node(value);
}

// Uso:
TreeNode *tree = NULL;
bst_insert_pp(&tree, 10);   // modifica tree directamente
bst_insert_pp(&tree, 5);
```

`root` apunta al **puntero** que necesita modificarse — ya sea la
variable `tree` del caller, o el campo `left`/`right` de algún nodo.
Al llegar a `*root == NULL`, se escribe directamente.  No hay caso
especial para árbol vacío.

Este patrón es la forma más limpia en C.  Equivale a la versión Rust
con `&mut Tree<i32>`.

### Traza: insertar 9 con puntero a puntero

```
*root = 10 (root apunta a la variable tree del caller)
  9 < 10 → root = &(10->left)

*root = 5 (root apunta a 10->left)
  9 > 5 → root = &(5->right)

*root = 7 (root apunta a 5->right)
  9 > 7 → root = &(7->right)

*root = NULL (root apunta a 7->right)
  *root = create_node(9)
  → 7->right ahora apunta a nodo(9)
```

---

## Construir un BST desde un array

```c
TreeNode *build_bst(int *arr, int n) {
    TreeNode *root = NULL;
    for (int i = 0; i < n; i++) {
        bst_insert_pp(&root, arr[i]);
    }
    return root;
}

int values[] = {10, 5, 15, 3, 7, 12, 20};
TreeNode *root = build_bst(values, 7);
```

```rust
fn build_bst(values: &[i32]) -> Tree<i32> {
    let mut tree: Tree<i32> = None;
    for &v in values {
        bst_insert(&mut tree, v);
    }
    tree
}
```

Complejidad de construir: cada inserción es $O(h)$, y $h$ va creciendo.
- Orden aleatorio: $\sum_{i=1}^{n} O(\log i) = O(n \log n)$.
- Orden sorted: $\sum_{i=1}^{n} O(i) = O(n^2)$.

---

## Complejidad de la inserción

| Caso | Altura | Complejidad |
|------|--------|------------|
| Mejor (hoja cercana a la raíz) | 1 | $O(1)$ |
| Balanceado | $\lfloor \log_2 n \rfloor$ | $O(\log n)$ |
| Promedio (aleatorio) | $\approx 1.39 \log_2 n$ | $O(\log n)$ |
| Degenerado | $n - 1$ | $O(n)$ |

La inserción no rebalancea — si el árbol ya está degenerado, la
inserción es $O(n)$.  Los árboles AVL y rojinegros (S04) solucionan
esto con rotaciones automáticas después de cada inserción.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>

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

// --- Inserción recursiva ---
TreeNode *bst_insert(TreeNode *root, int value) {
    if (!root) return create_node(value);
    if (value < root->data)
        root->left = bst_insert(root->left, value);
    else if (value > root->data)
        root->right = bst_insert(root->right, value);
    return root;
}

// --- Inserción con puntero a puntero ---
void bst_insert_pp(TreeNode **root, int value) {
    while (*root) {
        if (value < (*root)->data)
            root = &(*root)->left;
        else if (value > (*root)->data)
            root = &(*root)->right;
        else
            return;
    }
    *root = create_node(value);
}

// --- Auxiliares ---
void inorder(TreeNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
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

int main(void) {
    // Construir BST balanceado
    printf("=== Balanced insertion ===\n");
    int balanced[] = {10, 5, 15, 3, 7, 12, 20};
    TreeNode *root1 = NULL;
    for (int i = 0; i < 7; i++)
        root1 = bst_insert(root1, balanced[i]);

    printf("Inorder: ");
    inorder(root1);
    printf("\nHeight:  %d\n", tree_height(root1));

    // Construir BST degenerado
    printf("\n=== Sorted insertion (degenerate) ===\n");
    int sorted[] = {3, 5, 7, 10, 12, 15, 20};
    TreeNode *root2 = NULL;
    for (int i = 0; i < 7; i++)
        bst_insert_pp(&root2, sorted[i]);

    printf("Inorder: ");
    inorder(root2);
    printf("\nHeight:  %d\n", tree_height(root2));

    // Insertar valor adicional
    printf("\n=== Insert 9 into balanced BST ===\n");
    root1 = bst_insert(root1, 9);
    printf("Inorder: ");
    inorder(root1);
    printf("\nHeight:  %d\n", tree_height(root1));

    // Duplicado
    printf("\n=== Insert duplicate 10 ===\n");
    root1 = bst_insert(root1, 10);
    printf("Inorder: ");
    inorder(root1);
    printf("\n(unchanged)\n");

    tree_free(root1);
    tree_free(root2);
    return 0;
}
```

Salida:

```
=== Balanced insertion ===
Inorder: 3 5 7 10 12 15 20
Height:  2

=== Sorted insertion (degenerate) ===
Inorder: 3 5 7 10 12 15 20
Height:  6

=== Insert 9 into balanced BST ===
Inorder: 3 5 7 9 10 12 15 20
Height:  3

=== Insert duplicate 10 ===
Inorder: 3 5 7 9 10 12 15 20
(unchanged)
```

Mismo inorden, alturas 2 vs 6.  La inserción de 9 aumenta la altura
de 2 a 3.

---

## Ejercicios

### Ejercicio 1 — Construir paso a paso

Inserta los valores `8, 4, 12, 2, 6, 10, 14` en un BST vacío (en ese
orden).  Dibuja el árbol después de cada inserción.

<details>
<summary>Resultado final</summary>

```
         8
        / \
       4    12
      / \   / \
     2   6 10  14
```

Es un árbol perfecto de altura 2.  El orden de inserción (raíz primero,
luego nivel 1, luego nivel 2) produce la forma balanceada.

Cada inserción: 8=raíz.  4<8→izq.  12>8→der.  2<8<4→izq de 4.
6>4<8→der de 4.  10>8<12→izq de 12.  14>12→der de 12.
</details>

### Ejercicio 2 — Inserción degenerada

Inserta `1, 2, 3, 4, 5` en un BST vacío.  ¿Cuál es la altura?  ¿Cuántas
comparaciones se hacen en total?

<details>
<summary>Predicción</summary>

```
  1
   \
    2
     \
      3
       \
        4
         \
          5
```

Altura: **4**.

Comparaciones: insertar 1=0, insertar 2=1 (con 1), insertar 3=2
(con 1, 2), insertar 4=3 (con 1, 2, 3), insertar 5=4 (con 1, 2, 3, 4).
Total: $0 + 1 + 2 + 3 + 4 = 10 = \frac{n(n-1)}{2}$ → $O(n^2)$.
</details>

### Ejercicio 3 — Puntero a puntero

Traza `bst_insert_pp` para insertar 6 en este BST:

```
       8
      / \
     4    12
    /
   2
```

Muestra a qué apunta `root` en cada paso.

<details>
<summary>Traza</summary>

```
root = &tree         *root = 8
  6 < 8 → root = &(8->left)

root = &(8->left)    *root = 4
  6 > 4 → root = &(4->right)

root = &(4->right)   *root = NULL
  *root = create_node(6)
  → 4->right = nodo(6)
```

Resultado:
```
       8
      / \
     4    12
    / \
   2   6
```

`root` nunca fue `NULL` — siempre apuntó a un puntero válido.  Al final,
escribir en `*root` modifica directamente `4->right`.
</details>

### Ejercicio 4 — Implementar build_bst en Rust

Implementa una función que construya un BST a partir de un slice
`&[i32]`.

<details>
<summary>Implementación</summary>

```rust
fn build_bst(values: &[i32]) -> Tree<i32> {
    let mut tree: Tree<i32> = None;
    for &v in values {
        bst_insert(&mut tree, v);
    }
    tree
}

let tree = build_bst(&[10, 5, 15, 3, 7, 12, 20]);
// Inorden: [3, 5, 7, 10, 12, 15, 20]
```

Cada llamada a `bst_insert` desciende por el árbol existente y coloca
el nuevo valor como hoja.
</details>

### Ejercicio 5 — Cuántas formas

¿Cuántos órdenes de inserción distintos producen exactamente este BST?

```
    5
   / \
  3   7
```

<details>
<summary>Conteo</summary>

El 5 **debe** insertarse primero (es la raíz).  Luego 3 y 7 pueden
insertarse en cualquier orden — cada uno va a un subárbol diferente.

Órdenes válidos:
1. `5, 3, 7`
2. `5, 7, 3`

**2 órdenes**.

En general, si la raíz tiene subárboles de tamaños $L$ y $R$, las
inserciones de los subárboles pueden **intercalarse** de cualquier
forma que preserve el orden relativo dentro de cada subárbol.  El
número de intercalaciones es $\binom{L+R}{L}$.  Aquí: $\binom{1+1}{1} = 2$.
</details>

### Ejercicio 6 — Insertar duplicado con contador

Modifica la inserción para que, en vez de ignorar duplicados, incremente
un contador:

```c
typedef struct BSTNode {
    int data;
    int count;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;
```

<details>
<summary>Implementación</summary>

```c
BSTNode *create_bst_node(int value) {
    BSTNode *n = malloc(sizeof(BSTNode));
    n->data = value;
    n->count = 1;
    n->left = n->right = NULL;
    return n;
}

BSTNode *bst_insert_count(BSTNode *root, int value) {
    if (!root) return create_bst_node(value);

    if (value < root->data)
        root->left = bst_insert_count(root->left, value);
    else if (value > root->data)
        root->right = bst_insert_count(root->right, value);
    else
        root->count++;   // duplicado: incrementar

    return root;
}
```

Insertar `5, 3, 5, 7, 5` produce: nodo 5 con count=3, nodo 3 con
count=1, nodo 7 con count=1.  La estructura del árbol no cambia por
los duplicados.
</details>

### Ejercicio 7 — Altura después de n inserciones aleatorias

Si insertas los valores del 1 al 1000 en **orden aleatorio**, ¿cuál
es la altura esperada del BST?  ¿Y si los insertas en orden?

<details>
<summary>Estimación</summary>

Orden aleatorio: $E[h] \approx 1.39 \cdot \log_2(1000) \approx 1.39 \times 10 \approx 14$.

Orden sorted: $h = n - 1 = 999$.

La diferencia es enorme: 14 vs 999.  La búsqueda en el caso aleatorio
necesita ~14 comparaciones, en el degenerado ~500 en promedio.  Esto
motiva los árboles auto-balanceados (S04).
</details>

### Ejercicio 8 — Inserción iterativa en Rust

Implementa `bst_insert_iter` en Rust.  Compara con la versión recursiva.

<details>
<summary>Implementación</summary>

```rust
fn bst_insert_iter(tree: &mut Tree<i32>, value: i32) {
    let mut current = tree;
    loop {
        match current {
            None => {
                *current = Some(Box::new(TreeNode {
                    data: value,
                    left: None,
                    right: None,
                }));
                return;
            }
            Some(node) => {
                if value < node.data {
                    current = &mut node.left;
                } else if value > node.data {
                    current = &mut node.right;
                } else {
                    return;
                }
            }
        }
    }
}
```

En Rust, la versión iterativa es tan limpia como la recursiva gracias
a `&mut`.  La referencia mutable `current` se reasigna en cada paso,
descendiendo por el árbol hasta encontrar `None`.
</details>

### Ejercicio 9 — Construir balanceado desde array ordenado

Dado un array **ya ordenado** `[1, 2, 3, 4, 5, 6, 7]`, ¿cómo
construirías un BST balanceado sin que degenere?

<details>
<summary>Algoritmo</summary>

Inserción directa del array ordenado produce degenerado.  En cambio,
usar **divide y vencerás**: el elemento del medio es la raíz, la mitad
izquierda forma el subárbol izquierdo, la mitad derecha el derecho.

```c
TreeNode *sorted_to_bst(int *arr, int start, int end) {
    if (start > end) return NULL;

    int mid = start + (end - start) / 2;
    TreeNode *node = create_node(arr[mid]);
    node->left = sorted_to_bst(arr, start, mid - 1);
    node->right = sorted_to_bst(arr, mid + 1, end);
    return node;
}

int arr[] = {1, 2, 3, 4, 5, 6, 7};
TreeNode *root = sorted_to_bst(arr, 0, 6);
```

```
         4
        / \
       2    6
      / \  / \
     1  3 5   7
```

Altura: 2 (mínima posible para 7 nodos).  Complejidad: $O(n)$.
</details>

### Ejercicio 10 — Propiedad BST después de inserción

Demuestra que si un árbol cumple la propiedad BST antes de la inserción,
la sigue cumpliendo después.

<details>
<summary>Argumento</summary>

La inserción coloca el nuevo valor $v$ como hoja en una posición
determinada por las comparaciones durante el descenso.

1. $v$ se inserta como hijo izquierdo de un nodo $p$ solo si $v < p$.
   Además, durante el descenso, $v$ fue mayor que todos los ancestros
   que enviaron al recorrido a la derecha, y menor que todos los que lo
   enviaron a la izquierda.  Esto garantiza que $v$ está en el rango
   correcto para su posición.

2. Los nodos existentes no se modifican — la inserción solo asigna un
   puntero NULL a un nuevo nodo.  Las relaciones entre nodos existentes
   no cambian.

3. El nuevo nodo es una hoja sin hijos → sus subárboles (vacíos)
   trivialmente satisfacen la propiedad BST.

Por lo tanto, la propiedad BST se preserva.  El inorden del nuevo
árbol es el inorden anterior con $v$ insertado en su posición ordenada.
</details>
