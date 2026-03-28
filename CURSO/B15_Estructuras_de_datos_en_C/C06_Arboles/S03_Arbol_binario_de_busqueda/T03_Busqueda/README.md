# Búsqueda en BST

## Búsqueda básica

La propiedad BST permite descartar la mitad del árbol en cada paso —
es búsqueda binaria sobre una estructura enlazada:

```
Buscar 12 en:

         10          12 > 10 → derecha
        /  \
       5    15       12 < 15 → izquierda
      / \   / \
     3   7 12  20    12 == 12 → encontrado
```

3 comparaciones para 7 nodos.  En un árbol general (no BST) serían
hasta 7.

---

## Búsqueda recursiva

### En C

```c
TreeNode *bst_search(TreeNode *root, int key) {
    if (!root) return NULL;              // no encontrado
    if (key == root->data) return root;  // encontrado
    if (key < root->data)
        return bst_search(root->left, key);
    return bst_search(root->right, key);
}
```

Retorna un puntero al nodo encontrado, o `NULL` si no existe.  La
recursión es de cola en ambas ramas (tail recursion) — el compilador
puede optimizarla a un loop.

### En Rust

```rust
fn bst_search(tree: &Tree<i32>, key: i32) -> Option<&TreeNode<i32>> {
    match tree {
        None => None,
        Some(node) => {
            if key == node.data { Some(node) }
            else if key < node.data { bst_search(&node.left, key) }
            else { bst_search(&node.right, key) }
        }
    }
}
```

Retorna `Option<&TreeNode<i32>>` — referencia al nodo o `None`.

---

## Búsqueda iterativa

Preferida en producción por no consumir stack:

### En C

```c
TreeNode *bst_search_iter(TreeNode *root, int key) {
    while (root) {
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}
```

Tres líneas en el loop.  Sin overhead de llamadas recursivas.

### En Rust

```rust
fn bst_search_iter(tree: &Tree<i32>, key: i32) -> Option<&TreeNode<i32>> {
    let mut current = tree;
    while let Some(node) = current {
        if key == node.data { return Some(node); }
        current = if key < node.data { &node.left } else { &node.right };
    }
    None
}
```

---

## Mínimo y máximo

El mínimo es el nodo **más a la izquierda**.  El máximo es el nodo **más
a la derecha**.

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
     ↑            ↑
    min          max
```

### En C

```c
TreeNode *bst_min(TreeNode *root) {
    if (!root) return NULL;
    while (root->left) root = root->left;
    return root;
}

TreeNode *bst_max(TreeNode *root) {
    if (!root) return NULL;
    while (root->right) root = root->right;
    return root;
}
```

### Versión recursiva

```c
TreeNode *bst_min_rec(TreeNode *root) {
    if (!root) return NULL;
    if (!root->left) return root;
    return bst_min_rec(root->left);
}
```

### En Rust

```rust
fn bst_min(tree: &Tree<i32>) -> Option<&TreeNode<i32>> {
    let mut current = tree;
    let mut result = None;
    while let Some(node) = current {
        result = Some(node.as_ref());
        current = &node.left;
    }
    result
}
```

Complejidad: $O(h)$ — recorre una rama completa.

---

## Sucesor inorden

El **sucesor inorden** de un nodo $N$ es el nodo con el valor
inmediatamente mayor que $N$ en el recorrido inorden.

```
Inorden: 3  5  7  10  12  15  20
                ↑       ↑
              nodo   sucesor de 10 = 12
```

### Dos casos

**Caso 1**: $N$ tiene subárbol derecho.  El sucesor es el **mínimo** del
subárbol derecho.

```
         10              sucesor(10):
        /  \               10 tiene right(15)
       5    15             → min del subárbol {15, 12, 20}
      / \   / \            → 12
     3   7 12  20
```

**Caso 2**: $N$ no tiene subárbol derecho.  El sucesor es el **ancestro
más cercano** cuyo subárbol izquierdo contiene a $N$ — es decir, el
primer ancestro al que llegamos subiendo por la izquierda.

```
         10              sucesor(7):
        /  \               7 no tiene right
       5    15             → subir: 7 es right de 5 → seguir
      / \   / \            → subir: 5 es left de 10 → 10 es el sucesor
     3   7 12  20
```

Si subimos hasta la raíz sin encontrar un ancestro por la izquierda,
no hay sucesor (el nodo es el máximo).

### En C (con puntero a padre)

Si cada nodo tiene un puntero `parent`:

```c
typedef struct BSTNode {
    int data;
    struct BSTNode *left;
    struct BSTNode *right;
    struct BSTNode *parent;
} BSTNode;

BSTNode *successor(BSTNode *node) {
    if (!node) return NULL;

    // Caso 1: tiene subárbol derecho
    if (node->right) {
        BSTNode *curr = node->right;
        while (curr->left) curr = curr->left;
        return curr;
    }

    // Caso 2: subir hasta encontrar un ancestro por la izquierda
    BSTNode *parent = node->parent;
    while (parent && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;  // NULL si no hay sucesor
}
```

### En C (sin puntero a padre)

Si no hay puntero `parent`, se busca desde la raíz:

```c
TreeNode *successor_from_root(TreeNode *root, int key) {
    TreeNode *succ = NULL;
    TreeNode *current = root;

    while (current) {
        if (key < current->data) {
            succ = current;         // candidato a sucesor
            current = current->left;
        } else if (key > current->data) {
            current = current->right;
        } else {
            // Encontrado: si tiene right, min del right
            if (current->right) {
                TreeNode *r = current->right;
                while (r->left) r = r->left;
                return r;
            }
            break;
        }
    }

    return succ;
}
```

La clave: cada vez que giramos a la **izquierda**, el nodo actual es un
candidato a sucesor (es mayor que `key`).  El último candidato antes de
encontrar `key` es el sucesor.

### Traza: sucesor de 7

```
Buscar 7 desde la raíz:

current=10, key=7.  7 < 10 → succ=10, current=left(5).
current=5,  key=7.  7 > 5  →           current=right(7).
current=7,  key=7.  Encontrado.  right=NULL → break.

Retornar succ = 10.
```

### Traza: sucesor de 12

```
current=10, key=12.  12 > 10 →           current=right(15).
current=15, key=12.  12 < 15 → succ=15, current=left(12).
current=12, key=12.  Encontrado.  right=NULL → break.

Retornar succ = 15.
```

---

## Predecesor inorden

El **predecesor inorden** de $N$ es el nodo con el valor inmediatamente
menor.  Es el espejo del sucesor:

**Caso 1**: $N$ tiene subárbol izquierdo → el predecesor es el
**máximo** del subárbol izquierdo.

**Caso 2**: $N$ no tiene subárbol izquierdo → subir hasta el primer
ancestro cuyo subárbol derecho contiene a $N$.

```
Inorden: 3  5  7  10  12  15  20
                    ↑   ↑
           predecesor   nodo
           de 12 = 10
```

### En C (desde la raíz)

```c
TreeNode *predecessor_from_root(TreeNode *root, int key) {
    TreeNode *pred = NULL;
    TreeNode *current = root;

    while (current) {
        if (key > current->data) {
            pred = current;          // candidato a predecesor
            current = current->right;
        } else if (key < current->data) {
            current = current->left;
        } else {
            // Encontrado: si tiene left, max del left
            if (current->left) {
                TreeNode *l = current->left;
                while (l->right) l = l->right;
                return l;
            }
            break;
        }
    }

    return pred;
}
```

Espejo exacto del sucesor: girar a la **derecha** actualiza el
candidato.

### En Rust

```rust
fn successor(tree: &Tree<i32>, key: i32) -> Option<i32> {
    let mut succ: Option<i32> = None;
    let mut current = tree;

    while let Some(node) = current {
        if key < node.data {
            succ = Some(node.data);
            current = &node.left;
        } else if key > node.data {
            current = &node.right;
        } else {
            // Encontrado
            if let Some(ref right) = node.right {
                return Some(bst_min_val(right));
            }
            break;
        }
    }

    succ
}

fn predecessor(tree: &Tree<i32>, key: i32) -> Option<i32> {
    let mut pred: Option<i32> = None;
    let mut current = tree;

    while let Some(node) = current {
        if key > node.data {
            pred = Some(node.data);
            current = &node.right;
        } else if key < node.data {
            current = &node.left;
        } else {
            if let Some(ref left) = node.left {
                return Some(bst_max_val(left));
            }
            break;
        }
    }

    pred
}

fn bst_min_val(tree: &TreeNode<i32>) -> i32 {
    let mut current = tree;
    while let Some(ref left) = current.left {
        current = left;
    }
    current.data
}

fn bst_max_val(tree: &TreeNode<i32>) -> i32 {
    let mut current = tree;
    while let Some(ref right) = current.right {
        current = right;
    }
    current.data
}
```

---

## Tabla de sucesor/predecesor

Para el árbol de referencia:

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

| Nodo | Predecesor | Sucesor | Caso predecesor | Caso sucesor |
|------|-----------|---------|----------------|-------------|
| 3 | — (es el mínimo) | 5 | sin left, sin ancestro der | sin right, ancestro izq = 5 |
| 5 | 3 | 7 | max de left = 3 | min de right = 7 |
| 7 | 5 | 10 | sin left, ancestro der = 5 | sin right, ancestro izq = 10 |
| 10 | 7 | 12 | max de left = 7 | min de right = 12 |
| 12 | 10 | 15 | sin left, ancestro der = 10 | sin right, ancestro izq = 15 |
| 15 | 12 | 20 | min de left = 12 | max de right = 20 |
| 20 | 15 | — (es el máximo) | sin left, ancestro der = 15 | sin right, sin ancestro izq |

---

## Floor y ceiling

Operaciones más generales que buscan valores que **pueden no estar**
en el árbol:

| Operación | Definición | Ejemplo (key=9) |
|-----------|-----------|-----------------|
| **Floor** | Mayor valor ≤ key | 7 |
| **Ceiling** | Menor valor ≥ key | 10 |

Floor y ceiling generalizan predecesor y sucesor para claves que no
necesariamente existen en el árbol.

### Floor en C

```c
TreeNode *bst_floor(TreeNode *root, int key) {
    TreeNode *result = NULL;

    while (root) {
        if (key == root->data) return root;
        if (key > root->data) {
            result = root;             // candidato (root->data ≤ key)
            root = root->right;        // buscar algo más cercano
        } else {
            root = root->left;
        }
    }

    return result;
}
```

### Ceiling en C

```c
TreeNode *bst_ceiling(TreeNode *root, int key) {
    TreeNode *result = NULL;

    while (root) {
        if (key == root->data) return root;
        if (key < root->data) {
            result = root;             // candidato (root->data ≥ key)
            root = root->left;         // buscar algo más cercano
        } else {
            root = root->right;
        }
    }

    return result;
}
```

### Ejemplo: floor y ceiling de 9

```
current=10, key=9.  9 < 10 → result=10 (ceiling candidato), current=left(5).
current=5,  key=9.  9 > 5  → result=5  (floor candidato),   current=right(7).
current=7,  key=9.  9 > 7  → result=7  (floor candidato),   current=right(NULL).

Floor(9)   = 7   (mayor valor ≤ 9)
Ceiling(9) = 10  (menor valor ≥ 9)
```

---

## Resumen de operaciones de búsqueda

| Operación | Descripción | Complejidad |
|-----------|-------------|------------|
| `search(key)` | Encontrar nodo con valor `key` | $O(h)$ |
| `min()` | Nodo más a la izquierda | $O(h)$ |
| `max()` | Nodo más a la derecha | $O(h)$ |
| `successor(key)` | Siguiente valor mayor en inorden | $O(h)$ |
| `predecessor(key)` | Siguiente valor menor en inorden | $O(h)$ |
| `floor(key)` | Mayor valor $\leq$ key | $O(h)$ |
| `ceiling(key)` | Menor valor $\geq$ key | $O(h)$ |

Todas son $O(h)$ — la altura domina.  Con árbol balanceado: $O(\log n)$.
Con degenerado: $O(n)$.

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

TreeNode *bst_insert(TreeNode *root, int value) {
    if (!root) return create_node(value);
    if (value < root->data)
        root->left = bst_insert(root->left, value);
    else if (value > root->data)
        root->right = bst_insert(root->right, value);
    return root;
}

// --- Búsqueda ---
TreeNode *bst_search(TreeNode *root, int key) {
    while (root) {
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}

// --- Min / Max ---
TreeNode *bst_min(TreeNode *root) {
    if (!root) return NULL;
    while (root->left) root = root->left;
    return root;
}

TreeNode *bst_max(TreeNode *root) {
    if (!root) return NULL;
    while (root->right) root = root->right;
    return root;
}

// --- Sucesor ---
TreeNode *bst_successor(TreeNode *root, int key) {
    TreeNode *succ = NULL;
    while (root) {
        if (key < root->data) {
            succ = root;
            root = root->left;
        } else if (key > root->data) {
            root = root->right;
        } else {
            if (root->right) {
                TreeNode *r = root->right;
                while (r->left) r = r->left;
                return r;
            }
            break;
        }
    }
    return succ;
}

// --- Predecesor ---
TreeNode *bst_predecessor(TreeNode *root, int key) {
    TreeNode *pred = NULL;
    while (root) {
        if (key > root->data) {
            pred = root;
            root = root->right;
        } else if (key < root->data) {
            root = root->left;
        } else {
            if (root->left) {
                TreeNode *l = root->left;
                while (l->right) l = l->right;
                return l;
            }
            break;
        }
    }
    return pred;
}

// --- Floor / Ceiling ---
TreeNode *bst_floor(TreeNode *root, int key) {
    TreeNode *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key > root->data) {
            result = root;
            root = root->right;
        } else {
            root = root->left;
        }
    }
    return result;
}

TreeNode *bst_ceiling(TreeNode *root, int key) {
    TreeNode *result = NULL;
    while (root) {
        if (key == root->data) return root;
        if (key < root->data) {
            result = root;
            root = root->left;
        } else {
            root = root->right;
        }
    }
    return result;
}

void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

void print_result(const char *label, TreeNode *node) {
    if (node)
        printf("%-20s %d\n", label, node->data);
    else
        printf("%-20s (none)\n", label);
}

int main(void) {
    TreeNode *root = NULL;
    int vals[] = {10, 5, 15, 3, 7, 12, 20};
    for (int i = 0; i < 7; i++)
        root = bst_insert(root, vals[i]);

    //        10
    //       /  \
    //      5    15
    //     / \   / \
    //    3   7 12  20

    printf("=== Search ===\n");
    print_result("search(7):",  bst_search(root, 7));
    print_result("search(9):",  bst_search(root, 9));
    print_result("search(20):", bst_search(root, 20));

    printf("\n=== Min / Max ===\n");
    print_result("min:", bst_min(root));
    print_result("max:", bst_max(root));

    printf("\n=== Successor ===\n");
    int succ_tests[] = {3, 5, 7, 10, 12, 15, 20};
    for (int i = 0; i < 7; i++) {
        char label[32];
        sprintf(label, "successor(%d):", succ_tests[i]);
        print_result(label, bst_successor(root, succ_tests[i]));
    }

    printf("\n=== Predecessor ===\n");
    for (int i = 0; i < 7; i++) {
        char label[32];
        sprintf(label, "predecessor(%d):", succ_tests[i]);
        print_result(label, bst_predecessor(root, succ_tests[i]));
    }

    printf("\n=== Floor / Ceiling (key=9) ===\n");
    print_result("floor(9):",   bst_floor(root, 9));
    print_result("ceiling(9):", bst_ceiling(root, 9));

    printf("\n=== Floor / Ceiling (key=15) ===\n");
    print_result("floor(15):",   bst_floor(root, 15));
    print_result("ceiling(15):", bst_ceiling(root, 15));

    tree_free(root);
    return 0;
}
```

Salida:

```
=== Search ===
search(7):           7
search(9):           (none)
search(20):          20

=== Min / Max ===
min:                 3
max:                 20

=== Successor ===
successor(3):        5
successor(5):        7
successor(7):        10
successor(10):       12
successor(12):       15
successor(15):       20
successor(20):       (none)

=== Predecessor ===
predecessor(3):      (none)
predecessor(5):      3
predecessor(7):      5
predecessor(10):     7
predecessor(12):     10
predecessor(15):     12
predecessor(20):     15

=== Floor / Ceiling (key=9) ===
floor(9):            7
ceiling(9):          10

=== Floor / Ceiling (key=15) ===
floor(15):           15
ceiling(15):         15
```

---

## Ejercicios

### Ejercicio 1 — Camino de búsqueda

En este BST, ¿qué nodos se visitan al buscar 13?  ¿Y al buscar 9?

```
         8
        / \
       4    12
      / \   / \
     2   6 10  14
              /
             13
```

<details>
<summary>Predicción</summary>

Buscar 13: `8 → 12 → 14 → 13`.  4 comparaciones, encontrado.

- 13 > 8 → derecha.
- 13 > 12 → derecha.
- 13 < 14 → izquierda.
- 13 == 13 → encontrado.

Buscar 9: `8 → 12 → 10 → left(NULL)`.  3 comparaciones + 1 NULL, no
encontrado.

- 9 > 8 → derecha.
- 9 < 12 → izquierda.
- 9 < 10 → izquierda → NULL → no encontrado.
</details>

### Ejercicio 2 — Sucesor sin subárbol derecho

En el BST del ejercicio 1, ¿cuál es el sucesor de 6?  Traza el
algoritmo desde la raíz.

<details>
<summary>Predicción</summary>

Sucesor de 6:

```
current=8,  6 < 8 → succ=8,  current=left(4).
current=4,  6 > 4 →          current=right(6).
current=6,  found. right=NULL → break.

Retornar succ = 8.
```

Sucesor de 6 = **8**.  El 6 no tiene subárbol derecho, así que el
sucesor es el ancestro más cercano al que se llega por la izquierda.

Verificar con inorden: `2 4 6 8 10 12 13 14`.  Después de 6 viene 8.
</details>

### Ejercicio 3 — Predecesor con subárbol izquierdo

¿Cuál es el predecesor de 12 en el BST del ejercicio 1?

<details>
<summary>Predicción</summary>

12 tiene subárbol izquierdo (nodo 10).  El predecesor es el **máximo**
del subárbol izquierdo.

Subárbol izquierdo de 12: solo el nodo 10 (sin hijos derechos).
Máximo = 10.

Predecesor de 12 = **10**.

Inorden: `2 4 6 8 10 12 13 14`.  Antes de 12 viene 10.
</details>

### Ejercicio 4 — Floor y ceiling

En el BST de referencia `{3, 5, 7, 10, 12, 15, 20}`, calcula floor y
ceiling para los valores 1, 6, 10 y 25.

<details>
<summary>Predicción</summary>

| Key | Floor (mayor ≤ key) | Ceiling (menor ≥ key) |
|-----|-------|---------|
| 1 | — (no hay valor ≤ 1) | 3 |
| 6 | 5 | 7 |
| 10 | 10 (existe en el árbol) | 10 |
| 25 | 20 | — (no hay valor ≥ 25) |

Cuando el key existe en el árbol, floor y ceiling son el mismo nodo.
Cuando no existe, floor < key < ceiling (si ambos existen).
</details>

### Ejercicio 5 — Implementar successor en Rust

Implementa `successor` que retorne `Option<i32>`.

<details>
<summary>Implementación</summary>

```rust
fn successor(tree: &Tree<i32>, key: i32) -> Option<i32> {
    let mut succ = None;
    let mut current = tree;

    while let Some(node) = current {
        if key < node.data {
            succ = Some(node.data);
            current = &node.left;
        } else if key > node.data {
            current = &node.right;
        } else {
            if let Some(ref right) = node.right {
                let mut r = right.as_ref();
                while let Some(ref left) = r.left {
                    r = left;
                }
                return Some(r.data);
            }
            break;
        }
    }

    succ
}
```

Patrón: al girar a la izquierda (key < node), el nodo actual es
candidato a sucesor.  Se actualiza `succ` con el valor más reciente
(más cercano a key).
</details>

### Ejercicio 6 — Recorrer en orden con sucesor

Es posible recorrer un BST en orden usando solo `min` y `successor`
repetidamente, sin recursión ni pila.  Implementa este recorrido.

<details>
<summary>Implementación</summary>

```c
void inorder_with_successor(TreeNode *root) {
    TreeNode *current = bst_min(root);
    while (current) {
        printf("%d ", current->data);
        current = bst_successor(root, current->data);
    }
}
```

Funciona, pero **ineficiente**: cada `bst_successor` es $O(h)$, y hay
$n$ llamadas → total $O(nh)$.  Para un árbol balanceado: $O(n \log n)$.
Para un degenerado: $O(n^2)$.  El inorden recursivo es $O(n)$ siempre.

Es útil conceptualmente: demuestra que el BST define un orden total
navegable paso a paso.
</details>

### Ejercicio 7 — Rango de valores

Implementa `bst_range(root, lo, hi)` que imprima todos los valores en
el rango $[\text{lo}, \text{hi}]$.

<details>
<summary>Implementación</summary>

```c
void bst_range(TreeNode *root, int lo, int hi) {
    if (!root) return;

    if (root->data > lo)
        bst_range(root->left, lo, hi);

    if (root->data >= lo && root->data <= hi)
        printf("%d ", root->data);

    if (root->data < hi)
        bst_range(root->right, lo, hi);
}

// bst_range(root, 5, 15) → 5 7 10 12 15
```

Es un inorden podado: no recorre subárboles que no pueden contener
valores en el rango.  Complejidad: $O(h + k)$ donde $k$ es el número
de valores en el rango.
</details>

### Ejercicio 8 — Elemento más cercano

Implementa `bst_closest(root, key)` que retorne el valor del árbol más
cercano a `key` (que puede no estar en el árbol).

<details>
<summary>Implementación</summary>

```c
int bst_closest(TreeNode *root, int key) {
    int closest = root->data;

    while (root) {
        if (abs(root->data - key) < abs(closest - key))
            closest = root->data;

        if (key < root->data)
            root = root->left;
        else if (key > root->data)
            root = root->right;
        else
            return key;  // exacto
    }

    return closest;
}

// bst_closest(root, 9) → 10 (|10-9|=1 < |7-9|=2)
// bst_closest(root, 6) → 5 o 7 (|5-6|=|7-6|=1)
```

El recorrido de búsqueda visita los nodos más relevantes.  El más
cercano siempre está en el camino de búsqueda — no es necesario
explorar subárboles que se alejan de `key`.  Complejidad: $O(h)$.
</details>

### Ejercicio 9 — kth smallest

Implementa `kth_smallest(root, k)` que retorne el $k$-ésimo valor más
pequeño del BST (1-indexed).

<details>
<summary>Implementación</summary>

```c
int kth_smallest(TreeNode *root, int k, int *count) {
    if (!root) return -1;

    int left = kth_smallest(root->left, k, count);
    if (*count == k) return left;

    (*count)++;
    if (*count == k) return root->data;

    return kth_smallest(root->right, k, count);
}

// Uso:
int count = 0;
int result = kth_smallest(root, 3, &count);  // 3er menor = 7
```

Es un inorden que cuenta.  Al llegar al $k$-ésimo nodo, retorna sin
recorrer el resto.  Complejidad: $O(h + k)$.

Verificar: inorden = `3 5 7 10 12 15 20`.  k=1→3, k=2→5, k=3→7,
k=4→10, etc.
</details>

### Ejercicio 10 — Sucesor y predecesor de todo el árbol

Para el BST del ejercicio 1, escribe predecesor y sucesor de cada nodo.
Verifica que coincidan con el inorden.

<details>
<summary>Tabla completa</summary>

```
         8
        / \
       4    12
      / \   / \
     2   6 10  14
              /
             13
```

Inorden: `2 4 6 8 10 12 13 14`

| Nodo | Predecesor | Sucesor | Caso pred | Caso succ |
|------|-----------|---------|-----------|-----------|
| 2 | — | 4 | mínimo del árbol | sin right, ancestro izq |
| 4 | 2 | 6 | max de left(2) | min de right(6) |
| 6 | 4 | 8 | sin left, ancestro der(4) | sin right, ancestro izq(8) |
| 8 | 6 | 10 | max de left(→4→6) | min de right(→12→10) |
| 10 | 8 | 12 | sin left, ancestro der(8) | sin right, ancestro izq(12) |
| 12 | 10 | 13 | min de left(10) | min de right(→14→13) |
| 13 | 12 | 14 | sin left, ancestro der(12) | sin right, ancestro izq(14) |
| 14 | 13 | — | max de left(13) | máximo del árbol |

Predecesor de cada nodo = elemento anterior en inorden.
Sucesor de cada nodo = elemento siguiente en inorden.
</details>
