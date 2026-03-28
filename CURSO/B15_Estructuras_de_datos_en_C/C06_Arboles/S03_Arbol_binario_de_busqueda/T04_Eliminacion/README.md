# Eliminación en BST

## Los tres casos

Eliminar un nodo de un BST es la operación más compleja porque debe
preservar la propiedad BST.  Hay tres casos según cuántos hijos tiene
el nodo a eliminar:

| Caso | Hijos | Dificultad | Estrategia |
|------|-------|-----------|-----------|
| 1 | 0 (hoja) | Trivial | Eliminar directamente |
| 2 | 1 | Simple | Reemplazar por su único hijo |
| 3 | 2 | Complejo | Reemplazar por sucesor o predecesor inorden |

---

## Árbol de referencia

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

---

## Caso 1: nodo hoja (sin hijos)

Eliminar un nodo sin hijos — simplemente se desenlaza del padre.

### Ejemplo: eliminar 3

```
Antes:                     Después:

         10                      10
        /  \                    /  \
       5    15                 5    15
      / \   / \                 \   / \
     3   7 12  20                7 12  20
     ↑
  eliminar
```

El padre (5) cambia su puntero `left` de `&3` a `NULL`.  Se libera el
nodo 3.

```c
// En el padre: root->left = NULL; free(nodo);
```

---

## Caso 2: nodo con un hijo

El nodo se reemplaza por su único hijo.  El hijo "sube" y ocupa la
posición del nodo eliminado.

### Ejemplo: eliminar 5 (que tiene left=3, right=7)

Espera — 5 tiene dos hijos, eso es caso 3.  Usemos otro ejemplo.

Primero eliminemos 3 y 7 para crear un escenario de un solo hijo:

```
         10
        /  \
       5    15
        \   / \
         7 12  20
```

### Ejemplo: eliminar 5 (un hijo: right=7)

```
Antes:                     Después:

         10                      10
        /  \                    /  \
       5    15                 7    15
        \   / \                   / \
         7 12  20               12  20
     ↑
  eliminar
```

El padre (10) cambia su puntero `left` de `&5` a `&7`.  El nodo 7 sube
a ocupar el lugar de 5.  Se libera el nodo 5.

La propiedad BST se mantiene: 7 estaba en el subárbol izquierdo de 10,
y sigue estándolo.

```c
// En el padre: root->left = nodo->right; free(nodo);
```

---

## Caso 3: nodo con dos hijos

No se puede simplemente eliminar el nodo — ¿cuál de sus dos hijos lo
reemplaza?  Ninguno de los dos directamente, porque el reemplazante
necesita ser mayor que todo el subárbol izquierdo y menor que todo el
subárbol derecho.

La solución: reemplazar el dato del nodo con el de su **sucesor
inorden** (o predecesor inorden) y luego eliminar ese sucesor/predecesor.

### ¿Por qué funciona el sucesor inorden?

El sucesor inorden del nodo es el **mínimo de su subárbol derecho** —
el menor valor que es mayor que el nodo.

Propiedades del sucesor:
1. Es mayor que todos los valores del subárbol izquierdo (por ser mayor
   que el nodo eliminado).
2. Es menor que todos los demás valores del subárbol derecho (por ser
   el mínimo de ese subárbol).
3. El sucesor no tiene hijo izquierdo (si lo tuviera, ese hijo sería
   menor → el sucesor no sería el mínimo).

La propiedad 3 es clave: eliminar el sucesor es caso 1 (hoja) o
caso 2 (un hijo derecho).  Nunca caso 3 — no hay recursión infinita.

### Ejemplo: eliminar 10 (raíz, dos hijos)

```
         10            Sucesor de 10 = min del subárbol derecho = 12
        /  \
       5    15         Paso 1: copiar dato del sucesor → nodo
      / \   / \                10 se convierte en 12
     3   7 12  20
                       Paso 2: eliminar el sucesor original (12)
                               12 era hoja → caso 1
```

```
Paso 1: copiar dato        Paso 2: eliminar 12 original

         12                      12
        /  \                    /  \
       5    15                 5    15
      / \   / \               / \     \
     3   7 12  20            3   7    20
           ↑
        eliminar (caso 1)
```

Resultado final:

```
         12
        /  \
       5    15
      / \     \
     3   7    20
```

Inorden: `3 5 7 12 15 20` — sigue siendo creciente.  La propiedad BST
se conserva.

### Alternativa: usar el predecesor inorden

En vez del sucesor (mínimo del derecho), se puede usar el **predecesor**
(máximo del izquierdo).  Para eliminar 10:

```
Predecesor de 10 = max del subárbol izquierdo = 7

         7
        /  \
       5    15
      / \   / \
     3       12  20
         ↑
      7 original eliminado (caso 1)
```

Resultado:

```
         7
        /  \
       5    15
      /    / \
     3   12  20
```

Ambos son válidos.  La convención más común es usar el sucesor.

---

## Implementación recursiva en C

```c
TreeNode *bst_delete(TreeNode *root, int key) {
    if (!root) return NULL;

    if (key < root->data) {
        root->left = bst_delete(root->left, key);
    } else if (key > root->data) {
        root->right = bst_delete(root->right, key);
    } else {
        // Encontrado: eliminar este nodo

        // Caso 1 y 2: cero o un hijo
        if (!root->left) {
            TreeNode *right = root->right;
            free(root);
            return right;       // puede ser NULL (caso 1)
        }
        if (!root->right) {
            TreeNode *left = root->left;
            free(root);
            return left;
        }

        // Caso 3: dos hijos
        // Encontrar sucesor inorden (mínimo del subárbol derecho)
        TreeNode *succ = root->right;
        while (succ->left) succ = succ->left;

        // Copiar dato del sucesor al nodo actual
        root->data = succ->data;

        // Eliminar el sucesor original del subárbol derecho
        root->right = bst_delete(root->right, succ->data);
    }

    return root;
}
```

### Anatomía del código

El caso 1 (hoja) y caso 2 (un hijo) se unifican elegantemente:

```c
if (!root->left) {
    TreeNode *right = root->right;   // right puede ser NULL (caso 1)
    free(root);                       // o un subárbol (caso 2)
    return right;
}
```

Si el nodo no tiene hijo izquierdo, retornar su hijo derecho (sea nodo o
NULL).  Lo mismo espejado para sin hijo derecho.

Para el caso 3:
1. Encontrar sucesor (min del right).
2. Copiar su dato al nodo actual.
3. Eliminar recursivamente el sucesor del subárbol derecho.

La recursión en el paso 3 siempre termina en caso 1 o 2 — el sucesor
no tiene hijo izquierdo.

---

## Traza detallada: eliminar 5

```
BST:
         10
        /  \
       5    15
      / \   / \
     3   7 12  20

bst_delete(root=10, key=5):
  5 < 10 → root->left = bst_delete(left=5, key=5)

  bst_delete(root=5, key=5):
    5 == 5 → encontrado
    root->left = 3 (no NULL)
    root->right = 7 (no NULL)
    → Caso 3: dos hijos

    Sucesor: right=7, no tiene left → succ=7
    Copiar: root->data = 7
    Eliminar 7 del subárbol derecho:
      root->right = bst_delete(right=7, key=7)

      bst_delete(root=7, key=7):
        7 == 7 → encontrado
        root->left = NULL → Caso 1
        right = NULL
        free(7)
        return NULL

    root->right = NULL
    return nodo (ahora con data=7, left=3, right=NULL)

  root->left = nodo(7, left=3)
  return root(10)
```

Resultado:

```
         10
        /  \
       7    15
      /    / \
     3   12  20
```

Inorden: `3 7 10 12 15 20` — creciente, BST válido.

---

## Traza: eliminar la raíz (10)

```
bst_delete(root=10, key=10):
  10 == 10 → encontrado
  left=5, right=15 → Caso 3

  Sucesor: right=15, left=12 → succ=12
  Copiar: root->data = 12
  Eliminar 12:
    root->right = bst_delete(right=15, key=12)

    bst_delete(root=15, key=12):
      12 < 15 → root->left = bst_delete(left=12, key=12)

      bst_delete(root=12, key=12):
        12 == 12 → encontrado
        left = NULL → Caso 1
        right = NULL
        free(12)
        return NULL

      root->left = NULL
      return root(15, left=NULL, right=20)

  root->right = nodo(15, right=20)
  return root(ahora data=12)
```

Resultado:

```
         12
        /  \
       5    15
      / \     \
     3   7    20
```

---

## Implementación iterativa en C

```c
TreeNode *bst_delete_iter(TreeNode *root, int key) {
    TreeNode *parent = NULL;
    TreeNode *current = root;

    // Buscar el nodo
    while (current && current->data != key) {
        parent = current;
        current = (key < current->data) ? current->left : current->right;
    }

    if (!current) return root;  // no encontrado

    // Caso 3: dos hijos → reducir a caso 1 o 2
    if (current->left && current->right) {
        // Encontrar sucesor y su padre
        TreeNode *succ_parent = current;
        TreeNode *succ = current->right;
        while (succ->left) {
            succ_parent = succ;
            succ = succ->left;
        }

        // Copiar dato del sucesor
        current->data = succ->data;

        // Ahora eliminar el sucesor (caso 1 o 2)
        current = succ;
        parent = succ_parent;
    }

    // Caso 1 o 2: el nodo a eliminar tiene 0 o 1 hijo
    TreeNode *child = current->left ? current->left : current->right;

    if (!parent) {
        // Eliminar la raíz
        free(current);
        return child;
    }

    if (current == parent->left)
        parent->left = child;
    else
        parent->right = child;

    free(current);
    return root;
}
```

La versión iterativa es más larga pero no consume stack.  La idea es la
misma: caso 3 se reduce a caso 1/2 copiando el dato del sucesor.

---

## Comparación: sucesor vs predecesor

| Aspecto | Sucesor (min del right) | Predecesor (max del left) |
|---------|----------------------|------------------------|
| Ubicación | Subárbol derecho | Subárbol izquierdo |
| Garantía | No tiene left | No tiene right |
| Caso al eliminarlo | 1 (hoja) o 2 (solo right) | 1 (hoja) o 2 (solo left) |
| Resultado | Válido | Válido |

Ambos preservan la propiedad BST.  Elegir uno u otro puede afectar el
balance del árbol resultante.  Alternar entre ambos (a veces sucesor,
a veces predecesor) puede mantener mejor balance, pero en la práctica
se usa consistentemente uno.

---

## Caso especial: eliminar la raíz

La raíz no tiene padre.  El patrón `root->left = bst_delete(...)` no
aplica si el nodo a eliminar es la raíz misma.  La versión recursiva
maneja esto naturalmente — retorna el nuevo root.  En la versión
iterativa, hay que verificar `if (!parent)`.

```c
// Recursiva: el caller actualiza root
root = bst_delete(root, 10);

// Iterativa: el caso parent==NULL se maneja explícitamente
if (!parent) {
    free(current);
    return child;
}
```

---

## Complejidad

| Operación dentro de delete | Costo |
|---------------------------|-------|
| Buscar el nodo | $O(h)$ |
| Encontrar sucesor (caso 3) | $O(h)$ |
| Eliminar sucesor (caso 1/2) | $O(1)$ en la recursiva, $O(h)$ en la iterativa |
| **Total** | $O(h)$ |

| Forma del BST | Complejidad |
|---------------|------------|
| Balanceado | $O(\log n)$ |
| Degenerado | $O(n)$ |

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

TreeNode *bst_delete(TreeNode *root, int key) {
    if (!root) return NULL;

    if (key < root->data) {
        root->left = bst_delete(root->left, key);
    } else if (key > root->data) {
        root->right = bst_delete(root->right, key);
    } else {
        // Caso 1 y 2
        if (!root->left) {
            TreeNode *right = root->right;
            free(root);
            return right;
        }
        if (!root->right) {
            TreeNode *left = root->left;
            free(root);
            return left;
        }

        // Caso 3: sucesor inorden
        TreeNode *succ = root->right;
        while (succ->left) succ = succ->left;
        root->data = succ->data;
        root->right = bst_delete(root->right, succ->data);
    }

    return root;
}

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

int tree_count(TreeNode *root) {
    if (!root) return 0;
    return 1 + tree_count(root->left) + tree_count(root->right);
}

void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    TreeNode *root = NULL;
    int vals[] = {10, 5, 15, 3, 7, 12, 20};
    for (int i = 0; i < 7; i++)
        root = bst_insert(root, vals[i]);

    printf("Initial:   ");
    inorder(root);
    printf("  (n=%d, h=%d)\n", tree_count(root), tree_height(root));

    // Caso 1: eliminar hoja
    printf("\nDelete 3 (leaf):\n");
    root = bst_delete(root, 3);
    printf("  Inorder: ");
    inorder(root);
    printf("  (n=%d, h=%d)\n", tree_count(root), tree_height(root));

    // Caso 2: eliminar nodo con un hijo
    printf("\nDelete 5 (one child: right=7):\n");
    root = bst_delete(root, 5);
    printf("  Inorder: ");
    inorder(root);
    printf("  (n=%d, h=%d)\n", tree_count(root), tree_height(root));

    // Caso 3: eliminar nodo con dos hijos
    printf("\nDelete 15 (two children: left=12, right=20):\n");
    root = bst_delete(root, 15);
    printf("  Inorder: ");
    inorder(root);
    printf("  (n=%d, h=%d)\n", tree_count(root), tree_height(root));

    // Eliminar raíz
    printf("\nDelete 10 (root):\n");
    root = bst_delete(root, 10);
    printf("  Inorder: ");
    inorder(root);
    printf("  (n=%d, h=%d)\n", tree_count(root), tree_height(root));

    // Eliminar valor inexistente
    printf("\nDelete 99 (not found):\n");
    root = bst_delete(root, 99);
    printf("  Inorder: ");
    inorder(root);
    printf("  (unchanged)\n");

    tree_free(root);
    return 0;
}
```

Salida:

```
Initial:   3 5 7 10 12 15 20  (n=7, h=2)

Delete 3 (leaf):
  Inorder: 5 7 10 12 15 20  (n=6, h=2)

Delete 5 (one child: right=7):
  Inorder: 7 10 12 15 20  (n=5, h=2)

Delete 15 (two children: left=12, right=20):
  Inorder: 7 10 12 20  (n=4, h=2)

Delete 10 (root):
  Inorder: 7 12 20  (n=3, h=1)

Delete 99 (not found):
  Inorder: 7 12 20  (unchanged)
```

---

## Visualización paso a paso

```
Inicial:               Eliminar 3:          Eliminar 5:
         10                  10                  10
        /  \                /  \                /  \
       5    15             5    15             7    15
      / \   / \             \   / \               / \
     3   7 12  20            7 12  20           12  20

Eliminar 15:           Eliminar 10:
(succ=20)              (succ=12)
         10                  12
        /  \                /  \
       7    20             7    20
           /
          12
```

---

## Ejercicios

### Ejercicio 1 — Identificar el caso

Para cada eliminación, identifica el caso (1, 2 o 3):

```
         20
        /  \
       10   30
      / \     \
     5   15    35
        /
       12
```

a) Eliminar 5.  b) Eliminar 30.  c) Eliminar 10.  d) Eliminar 20.

<details>
<summary>Predicción</summary>

a) **Caso 1** (hoja): 5 no tiene hijos.
b) **Caso 2** (un hijo): 30 tiene solo right=35.
c) **Caso 3** (dos hijos): 10 tiene left=5 y right=15.
d) **Caso 3** (dos hijos): 20 tiene left=10 y right=30.
</details>

### Ejercicio 2 — Eliminar con sucesor: traza

En el BST del ejercicio 1, elimina 10 (caso 3).  Muestra el sucesor,
el árbol intermedio y el resultado final.

<details>
<summary>Traza</summary>

Sucesor de 10 = min del subárbol derecho {15, 12} = **12**.

Paso 1: copiar dato 12 al nodo que tenía 10.

```
         20
        /  \
       12   30
      / \     \
     5   15    35
        /
       12  ← este hay que eliminarlo
```

Paso 2: eliminar 12 original del subárbol derecho.  12 no tiene left →
caso 1 (hoja).

```
         20
        /  \
       12   30
      / \     \
     5   15    35
```

Inorden: `5 12 15 20 30 35` — creciente, BST válido.
</details>

### Ejercicio 3 — Eliminar con predecesor

Repite el ejercicio 2 pero usando el **predecesor** en vez del sucesor.
¿El resultado es diferente?

<details>
<summary>Resultado</summary>

Predecesor de 10 = max del subárbol izquierdo {5} = **5**.

Paso 1: copiar 5 al nodo.

```
         20
        /  \
       5    30
      / \     \
     5   15    35
     ↑  /
  eliminar
       12
```

Paso 2: eliminar 5 original.  5 es hoja → caso 1.

```
         20
        /  \
       5    30
        \     \
        15    35
        /
       12
```

Inorden: `5 12 15 20 30 35` — el mismo inorden, pero la **estructura**
es diferente.  La altura del subárbol izquierdo cambió.
</details>

### Ejercicio 4 — Eliminar todos los nodos

Elimina todos los nodos del árbol de referencia en este orden:
`10, 5, 15, 3, 7, 12, 20`.  Dibuja el árbol después de cada paso.

<details>
<summary>Secuencia</summary>

```
Eliminar 10 (caso 3, succ=12):     Eliminar 5 (caso 3, succ=7):
         12                              12
        /  \                            /  \
       5    15                         7    15
      / \     \                              \
     3   7    20                             20

Eliminar 15 (caso 2, hijo=20):     Eliminar 3: error — 3 ya no existe.
         12                         (fue eliminado implícitamente? No,
        /  \                         3 sigue en el árbol anterior)
       7    20

Retrocedamos. Después de eliminar 5 con succ=7:
         12
        /  \
       7    15
      /       \
     3        20

Eliminar 15 (caso 2, hijo=20):
         12
        /  \
       7    20
      /
     3

Eliminar 3 (caso 1, hoja):
         12
        /  \
       7    20

Eliminar 7 (caso 1, hoja):
         12
           \
           20

Eliminar 12 (caso 2, hijo=20):
    20

Eliminar 20 (caso 1, hoja):
    NULL (árbol vacío)
```
</details>

### Ejercicio 5 — Implementar delete en Rust

Implementa `bst_delete` para `Tree<i32>`.

<details>
<summary>Implementación</summary>

```rust
fn bst_delete(tree: &mut Tree<i32>, key: i32) {
    let node = match tree {
        None => return,
        Some(node) => node,
    };

    if key < node.data {
        bst_delete(&mut node.left, key);
    } else if key > node.data {
        bst_delete(&mut node.right, key);
    } else {
        // Encontrado
        match (&node.left, &node.right) {
            (None, _) => *tree = tree.take().unwrap().right,
            (_, None) => *tree = tree.take().unwrap().left,
            _ => {
                // Caso 3: encontrar sucesor
                let succ_val = {
                    let mut curr = node.right.as_ref().unwrap().as_ref();
                    while let Some(ref left) = curr.left {
                        curr = left;
                    }
                    curr.data
                };
                node.data = succ_val;
                bst_delete(&mut node.right, succ_val);
            }
        }
    }
}
```

`tree.take()` extrae el `Some(Box<...>)` dejando `None`, luego se
reasigna `*tree` con el hijo correspondiente.  Para el caso 3, se
copia el valor del sucesor y se elimina recursivamente.
</details>

### Ejercicio 6 — ¿Por qué el sucesor no tiene hijo izquierdo?

Demuestra que el sucesor inorden de un nodo con dos hijos nunca tiene
hijo izquierdo.

<details>
<summary>Demostración</summary>

El sucesor inorden de un nodo $N$ (caso dos hijos) es el **mínimo**
del subárbol derecho de $N$.

Supongamos por contradicción que este mínimo $M$ tiene un hijo
izquierdo $L$.  Entonces $L < M$ (propiedad BST).  Pero $L$ está en
el subárbol derecho de $N$ (es descendiente de $M$, que está en el
subárbol derecho).  Por tanto, $L$ debería ser el mínimo del subárbol
derecho, no $M$ — contradicción.

Conclusión: el mínimo de cualquier subárbol nunca tiene hijo izquierdo.
Por eso eliminarlo siempre es caso 1 (hoja) o caso 2 (solo hijo
derecho).
</details>

### Ejercicio 7 — Eliminar degenerado

Elimina los nodos de este BST degenerado en el orden `3, 2, 4, 1, 5`:

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

<details>
<summary>Secuencia</summary>

```
Eliminar 3 (caso 2, hijo=4):    Eliminar 2 (caso 2, hijo=4):
  1                                1
   \                                \
    2                                4
     \                                \
      4                                5
       \
        5

Eliminar 4 (caso 2, hijo=5):    Eliminar 1 (caso 2, hijo=5):
  1                                5
   \
    5

Eliminar 5 (caso 1, hoja):
  NULL
```

En un degenerado, la mayoría de eliminaciones son caso 2 (un hijo).
Cada eliminación reduce la altura en 1 si se elimina un nodo intermedio.
</details>

### Ejercicio 8 — delete preserva BST

Después de eliminar un nodo, ¿cómo puedes verificar rápidamente que el
árbol sigue siendo un BST válido?

<details>
<summary>Métodos</summary>

1. **Inorden**: recorrer en inorden y verificar que la secuencia es
   estrictamente creciente.  $O(n)$.

2. **is_bst con rangos**: ejecutar la verificación con min/max.  $O(n)$.

3. **Verificación local** (insuficiente): comprobar solo los vecinos
   del nodo modificado NO es suficiente — la propiedad BST es global.

En las pruebas, el método más robusto es comparar el inorden antes y
después:

```c
// Antes: inorder = [3, 5, 7, 10, 12, 15, 20]
// Eliminar 10
// Después: inorder = [3, 5, 7, 12, 15, 20]
// Verificar: es creciente Y contiene los valores correctos
```
</details>

### Ejercicio 9 — delete iterativo con puntero a puntero

Reimplementa `bst_delete` en C usando puntero a puntero (`TreeNode **`)
para evitar manejar el caso especial de la raíz.

<details>
<summary>Implementación</summary>

```c
void bst_delete_pp(TreeNode **root, int key) {
    // Buscar el nodo
    while (*root && (*root)->data != key) {
        if (key < (*root)->data)
            root = &(*root)->left;
        else
            root = &(*root)->right;
    }

    if (!*root) return;  // no encontrado

    TreeNode *node = *root;

    if (!node->left) {
        *root = node->right;
        free(node);
    } else if (!node->right) {
        *root = node->left;
        free(node);
    } else {
        // Caso 3: encontrar sucesor con puntero a puntero
        TreeNode **succ = &node->right;
        while ((*succ)->left)
            succ = &(*succ)->left;

        node->data = (*succ)->data;

        TreeNode *to_free = *succ;
        *succ = (*succ)->right;
        free(to_free);
    }
}

// Uso:
TreeNode *tree = /* ... */;
bst_delete_pp(&tree, 10);  // modifica tree directamente
```

El puntero a puntero `root` siempre apunta al lugar donde está el
puntero al nodo — ya sea la variable del caller o el campo `left`/`right`
de un nodo padre.  Escribir `*root = X` modifica directamente el enlace
correcto.  No hay caso especial para la raíz.
</details>

### Ejercicio 10 — Secuencia de operaciones

Partiendo de un BST vacío, ejecuta estas operaciones y dibuja el
árbol después de cada una:

```
insert 8, insert 4, insert 12, insert 2, insert 6,
insert 10, insert 14, delete 4, delete 12, delete 8
```

<details>
<summary>Resultado paso a paso</summary>

Después de las 7 inserciones:

```
         8
        / \
       4    12
      / \   / \
     2   6 10  14
```

delete 4 (caso 3, succ=6):

```
         8
        / \
       6    12
      /    / \
     2   10  14
```

delete 12 (caso 3, succ=14):

```
         8
        / \
       6    14
      /    /
     2   10
```

delete 8 (caso 3, succ=10):

```
         10
        /  \
       6    14
      /
     2
```

Inorden: `2 6 10 14` — creciente, BST válido.

Notar que 3 eliminaciones caso 3 consecutivas pueden desbalancear el
árbol (altura 3 para 4 nodos, cuando el mínimo sería 2).
</details>
