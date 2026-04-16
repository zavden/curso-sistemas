# Representación con punteros C

## Estructura del nodo

Un nodo de árbol binario tiene tres campos: dato, puntero al hijo izquierdo,
puntero al hijo derecho:

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;
```

Comparación con un nodo de lista:

```c
// Lista simple: 1 puntero
struct Node { int data; struct Node *next; };

// Lista doble: 2 punteros (prev, next)
struct DNode { int data; struct DNode *prev; struct DNode *next; };

// Árbol binario: 2 punteros (left, right)
struct TreeNode { int data; struct TreeNode *left; struct TreeNode *right; };
```

Un nodo de árbol binario tiene el mismo layout que un nodo de lista doble —
la diferencia es semántica: `left`/`right` representan hijos en una
jerarquía, no vecinos en una secuencia.

### sizeof

En plataforma de 64 bits con `int data`:

```
TreeNode: data(4) + padding(4) + left(8) + right(8) = 24 bytes
```

Idéntico al `DNode` de listas dobles.

---

## Crear un nodo

```c
#include <stdio.h>
#include <stdlib.h>

TreeNode *create_node(int value) {
    TreeNode *node = malloc(sizeof(TreeNode));
    if (!node) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
    node->data = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}
```

Un nodo recién creado es una **hoja**: ambos hijos son `NULL`.

---

## Árbol vacío

Un árbol vacío se representa como un puntero `NULL`:

```c
TreeNode *root = NULL;   // árbol vacío
```

Todas las funciones recursivas usan `NULL` como caso base:

```c
int height(TreeNode *root) {
    if (!root) return -1;   // árbol vacío → altura -1
    // ...
}
```

---

## Construir un árbol manualmente

No existe un "push" natural para árboles genéricos (a diferencia de BST, donde
la posición se determina por el valor).  Para construir un árbol arbitrario,
conectamos los nodos explícitamente:

```c
TreeNode *build_sample_tree(void) {
    //        10
    //       /  \
    //      5    15
    //     / \     \
    //    3   7    20

    TreeNode *root = create_node(10);
    root->left = create_node(5);
    root->right = create_node(15);
    root->left->left = create_node(3);
    root->left->right = create_node(7);
    root->right->right = create_node(20);

    return root;
}
```

Cada asignación `parent->left = child` o `parent->right = child` crea una
arista del padre al hijo.

### Traza de construcción en memoria

```
create_node(10) → 0xA00: {10, NULL, NULL}
create_node(5)  → 0xB00: {5, NULL, NULL}
create_node(15) → 0xC00: {15, NULL, NULL}
create_node(3)  → 0xD00: {3, NULL, NULL}
create_node(7)  → 0xE00: {7, NULL, NULL}
create_node(20) → 0xF00: {20, NULL, NULL}

root (0xA00): {10, left=0xB00, right=0xC00}
0xB00:        {5,  left=0xD00, right=0xE00}
0xC00:        {15, left=NULL,  right=0xF00}
0xD00:        {3,  left=NULL,  right=NULL}    ← hoja
0xE00:        {7,  left=NULL,  right=NULL}    ← hoja
0xF00:        {20, left=NULL,  right=NULL}    ← hoja
```

```
        0xA00
       /     \
    0xB00   0xC00
    /   \       \
 0xD00 0xE00  0xF00

Los nodos están dispersos en el heap — no contiguos.
```

---

## Operaciones fundamentales

### Contar nodos — $O(n)$

```c
int tree_count(TreeNode *root) {
    if (!root) return 0;
    return 1 + tree_count(root->left) + tree_count(root->right);
}
```

Visita **cada nodo exactamente una vez**.

### Altura — $O(n)$

```c
int tree_height(TreeNode *root) {
    if (!root) return -1;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}
```

Calcula la altura de cada subárbol y toma el mayor.

### Contar hojas — $O(n)$

```c
int tree_count_leaves(TreeNode *root) {
    if (!root) return 0;
    if (!root->left && !root->right) return 1;
    return tree_count_leaves(root->left) + tree_count_leaves(root->right);
}
```

### Buscar un valor — $O(n)$

En un árbol binario genérico (no BST), buscar requiere recorrer todo el
árbol:

```c
TreeNode *tree_find(TreeNode *root, int value) {
    if (!root) return NULL;
    if (root->data == value) return root;

    TreeNode *found = tree_find(root->left, value);
    if (found) return found;
    return tree_find(root->right, value);
}
```

En un BST (S03), la búsqueda es $O(\log n)$ porque podemos descartar un
subárbol en cada paso.  Aquí, sin orden, debemos explorar ambos.

### Suma de todos los valores — $O(n)$

```c
int tree_sum(TreeNode *root) {
    if (!root) return 0;
    return root->data + tree_sum(root->left) + tree_sum(root->right);
}
```

### Máximo — $O(n)$

```c
#include <limits.h>

int tree_max(TreeNode *root) {
    if (!root) return INT_MIN;
    int lmax = tree_max(root->left);
    int rmax = tree_max(root->right);
    int max = root->data;
    if (lmax > max) max = lmax;
    if (rmax > max) max = rmax;
    return max;
}
```

---

## Liberar un árbol — postorden

La liberación debe seguir un orden específico: liberar los hijos **antes**
del padre (postorden).  Si liberamos el padre primero, perdemos los punteros
a los hijos:

```c
void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);    // liberar subárbol izquierdo
    tree_free(root->right);   // liberar subárbol derecho
    free(root);               // liberar el nodo actual
}
```

```
Orden de liberación para el árbol de ejemplo:
  3 → 7 → 5 → 20 → 15 → 10

Postorden: hijos primero, padre después.
```

Si liberamos en preorden (`free(root)` primero):

```c
void tree_free_broken(TreeNode *root) {
    if (!root) return;
    free(root);                        // root liberado
    tree_free_broken(root->left);      // UB: root->left es use-after-free
    tree_free_broken(root->right);     // UB: root->right es use-after-free
}
```

---

## Visualización en consola

### Formato rotado (simple)

```c
void tree_print(TreeNode *root, int depth) {
    if (!root) return;
    tree_print(root->right, depth + 1);
    for (int i = 0; i < depth; i++) printf("    ");
    printf("%d\n", root->data);
    tree_print(root->left, depth + 1);
}
```

Salida para el árbol de ejemplo:

```
        20
    15
10
        7
    5
        3
```

### Formato con ramas (más legible)

```c
void tree_print_fancy(TreeNode *root, char *prefix, int is_left) {
    if (!root) return;

    printf("%s", prefix);
    printf("%s", is_left ? "├── " : "└── ");
    printf("%d\n", root->data);

    // Construir nuevo prefijo
    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s",
             prefix, is_left ? "│   " : "    ");

    tree_print_fancy(root->left, new_prefix, 1);
    tree_print_fancy(root->right, new_prefix, 0);
}

// Uso: tree_print_fancy(root, "", 0);
```

Salida:

```
└── 10
    ├── 5
    │   ├── 3
    │   └── 7
    └── 15
        └── 20
```

---

## Puntero a puntero para modificar la raíz

Al igual que con listas (`Node **head`), modificar la raíz de un árbol
requiere un puntero a puntero:

```c
// Insertar como raíz (ejemplo didáctico)
void tree_set_root(TreeNode **root, int value) {
    TreeNode *node = create_node(value);
    node->left = *root;     // viejo árbol queda como hijo izquierdo
    *root = node;           // nuevo nodo es la raíz
}

// Uso:
TreeNode *root = NULL;
tree_set_root(&root, 10);   // root ahora apunta al nodo 10
tree_set_root(&root, 5);    // 5 es raíz, 10 es su hijo izquierdo
```

En BST (S03), la inserción recursiva usa `TreeNode **` para modificar
el puntero `left` o `right` del padre directamente.

---

## Traza de función recursiva

`tree_count` sobre el árbol:

```
    10
   /  \
  5    15
 / \
3   7
```

```
tree_count(10)
├── 1 + tree_count(5) + tree_count(15)
│        │                 │
│        ├── 1 + tree_count(3) + tree_count(7)
│        │        │                 │
│        │        ├── 1 + tree_count(NULL) + tree_count(NULL)
│        │        │        = 1 + 0 + 0 = 1
│        │        │
│        │        └── 1 + tree_count(NULL) + tree_count(NULL)
│        │                 = 1 + 0 + 0 = 1
│        │
│        └── 1 + 1 + 1 = 3
│
├── tree_count(15)
│   └── 1 + tree_count(NULL) + tree_count(NULL)
│          = 1 + 0 + 0 = 1
│
└── 1 + 3 + 1 = 5
```

5 nodos — correcto.  La recursión visita cada nodo exactamente una vez,
y cada `NULL` exactamente una vez.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

TreeNode *create_node(int value) {
    TreeNode *node = malloc(sizeof(TreeNode));
    if (!node) { fprintf(stderr, "malloc failed\n"); exit(1); }
    node->data = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

int tree_count(TreeNode *root) {
    if (!root) return 0;
    return 1 + tree_count(root->left) + tree_count(root->right);
}

int tree_height(TreeNode *root) {
    if (!root) return -1;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

int tree_count_leaves(TreeNode *root) {
    if (!root) return 0;
    if (!root->left && !root->right) return 1;
    return tree_count_leaves(root->left) + tree_count_leaves(root->right);
}

int tree_sum(TreeNode *root) {
    if (!root) return 0;
    return root->data + tree_sum(root->left) + tree_sum(root->right);
}

void tree_print(TreeNode *root, int depth) {
    if (!root) return;
    tree_print(root->right, depth + 1);
    for (int i = 0; i < depth; i++) printf("    ");
    printf("%d\n", root->data);
    tree_print(root->left, depth + 1);
}

void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    //        10
    //       /  \
    //      5    15
    //     / \     \
    //    3   7    20

    TreeNode *root = create_node(10);
    root->left = create_node(5);
    root->right = create_node(15);
    root->left->left = create_node(3);
    root->left->right = create_node(7);
    root->right->right = create_node(20);

    printf("Tree:\n");
    tree_print(root, 0);

    printf("\nNodes:  %d\n", tree_count(root));        // 6
    printf("Height: %d\n", tree_height(root));         // 2
    printf("Leaves: %d\n", tree_count_leaves(root));   // 3
    printf("Sum:    %d\n", tree_sum(root));            // 60

    tree_free(root);
    return 0;
}
```

Salida:

```
Tree:
        20
    15
10
        7
    5
        3

Nodes:  6
Height: 2
Leaves: 3
Sum:    60
```

---

## Errores comunes

### 1. Liberar en el orden incorrecto

```c
// MAL: preorden — use-after-free
void tree_free_broken(TreeNode *root) {
    if (!root) return;
    free(root);
    tree_free_broken(root->left);   // UB
    tree_free_broken(root->right);  // UB
}
```

### 2. No verificar NULL

```c
// MAL: acceder sin verificar
int height_broken(TreeNode *root) {
    return 1 + height_broken(root->left);   // crash si root es NULL
}
```

### 3. Olvidar inicializar hijos a NULL

```c
// MAL: hijos sin inicializar
TreeNode *node = malloc(sizeof(TreeNode));
node->data = 42;
// node->left y node->right contienen basura
// tree_count(node) recurrirá en punteros basura → UB
```

### 4. Memory leak al reasignar

```c
// MAL: perder referencia al subárbol
root->left = create_node(99);   // el viejo left y todos sus descendientes
                                 // se filtraron — no hay forma de liberarlos
```

### 5. Confundir profundidad con altura

```c
// La profundidad crece hacia abajo desde la raíz (0, 1, 2...)
// La altura crece hacia arriba desde las hojas (0, 1, 2...)
// Para la raíz: profundidad = 0, altura = altura del árbol
// Para una hoja: profundidad = variable, altura = 0
```

---

## Ejercicios

### Ejercicio 1 — Construir y visualizar

Construye manualmente en C el siguiente árbol y visualízalo con `tree_print`:

```
        1
       / \
      2   3
     /   / \
    4   5   6
       /
      7
```

<details>
<summary>Código</summary>

```c
TreeNode *root = create_node(1);
root->left = create_node(2);
root->right = create_node(3);
root->left->left = create_node(4);
root->right->left = create_node(5);
root->right->right = create_node(6);
root->right->left->left = create_node(7);
```

`tree_print` salida:
```
        6
    3
            7
        5
1
    2
        4
```
</details>

### Ejercicio 2 — Traza de height

Traza `tree_height` sobre:

```
    A
   /
  B
 / \
C   D
```

Muestra cada llamada recursiva y su retorno.

<details>
<summary>Traza</summary>

```
height(A)
  height(B)
    height(C)
      height(NULL) = -1
      height(NULL) = -1
      return 1 + max(-1,-1) = 0
    height(D)
      height(NULL) = -1
      height(NULL) = -1
      return 1 + max(-1,-1) = 0
    return 1 + max(0, 0) = 1
  height(NULL) = -1
  return 1 + max(1, -1) = 2
```

Altura = 2 (camino A→B→C o A→B→D).
</details>

### Ejercicio 3 — Postorden para free

Traza el orden de liberación con `tree_free` sobre:

```
    10
   /  \
  5    15
 / \
3   7
```

¿Por qué preorden causaría UB?

<details>
<summary>Orden</summary>

Postorden: 3, 7, 5, 15, 10.

Preorden causaría: `free(10)` primero, luego `root->left` accede a memoria
liberada.  El puntero `left` ya no es válido después del `free` — es
use-after-free (UB).
</details>

### Ejercicio 4 — tree_find genérico

Implementa `tree_find` que busque un valor en un árbol binario genérico
(no BST).  Busca 7 y 99 en el árbol de ejemplo.

<details>
<summary>Resultado</summary>

```c
TreeNode *found7 = tree_find(root, 7);    // retorna puntero al nodo 7
TreeNode *found99 = tree_find(root, 99);  // retorna NULL

// La búsqueda recorre: 10 → 5 → 3 → (NULL, NULL) → 7 → encontrado
// Para 99: recorre los 6 nodos completos → NULL
```

$O(n)$ en el peor caso — sin propiedad de orden, no hay atajo.
</details>

### Ejercicio 5 — Espejo (mirror)

Implementa `tree_mirror` que intercambie left y right de cada nodo
recursivamente.  Dado el árbol de ejemplo, ¿cuál es el resultado?

<details>
<summary>Implementación</summary>

```c
void tree_mirror(TreeNode *root) {
    if (!root) return;
    TreeNode *temp = root->left;
    root->left = root->right;
    root->right = temp;
    tree_mirror(root->left);
    tree_mirror(root->right);
}
```

```
Original:           Espejo:
    10                10
   /  \              /  \
  5    15           15    5
 / \     \         /     / \
3   7    20       20    7   3
```
</details>

### Ejercicio 6 — Es hoja

Implementa `is_leaf(TreeNode *node)` que retorne 1 si el nodo es hoja
(ambos hijos NULL) y 0 en caso contrario.  Usa esta función para
reimplementar `tree_count_leaves`.

<details>
<summary>Implementación</summary>

```c
int is_leaf(TreeNode *node) {
    return node && !node->left && !node->right;
}

int tree_count_leaves(TreeNode *root) {
    if (!root) return 0;
    if (is_leaf(root)) return 1;
    return tree_count_leaves(root->left) + tree_count_leaves(root->right);
}
```
</details>

### Ejercicio 7 — Profundidad de un nodo

Implementa `tree_depth(root, value)` que retorne la profundidad del nodo
con el valor dado, o -1 si no existe.

<details>
<summary>Implementación</summary>

```c
int tree_depth(TreeNode *root, int value) {
    if (!root) return -1;
    if (root->data == value) return 0;

    int left = tree_depth(root->left, value);
    if (left >= 0) return left + 1;

    int right = tree_depth(root->right, value);
    if (right >= 0) return right + 1;

    return -1;
}
```

Para el árbol de ejemplo: `tree_depth(root, 7)` retorna 2 (10→5→7).
</details>

### Ejercicio 8 — Comparar dos árboles

Implementa `tree_equal(a, b)` que retorne 1 si dos árboles tienen la misma
estructura y los mismos datos.

<details>
<summary>Implementación</summary>

```c
int tree_equal(TreeNode *a, TreeNode *b) {
    if (!a && !b) return 1;           // ambos vacíos
    if (!a || !b) return 0;           // uno vacío, otro no
    return a->data == b->data
        && tree_equal(a->left, b->left)
        && tree_equal(a->right, b->right);
}
```

Tres líneas de lógica.  La recursión es elegante: dos árboles son iguales
si sus raíces tienen el mismo dato y sus subárboles izquierdo y derecho
son iguales (recursivamente).
</details>

### Ejercicio 9 — Nodos en un nivel

Implementa `tree_count_at_level(root, level)` que cuente los nodos en el
nivel dado.

<details>
<summary>Implementación</summary>

```c
int tree_count_at_level(TreeNode *root, int level) {
    if (!root) return 0;
    if (level == 0) return 1;
    return tree_count_at_level(root->left, level - 1)
         + tree_count_at_level(root->right, level - 1);
}
```

Para el árbol de ejemplo:
- Nivel 0: 1 (solo 10)
- Nivel 1: 2 (5, 15)
- Nivel 2: 3 (3, 7, 20)
</details>

### Ejercicio 10 — Memory leak detector

Implementa un contador global que se incrementa en `create_node` y se
decrementa en `tree_free` (dentro del `free`).  Después de `tree_free(root)`,
verifica que el contador es 0.

<details>
<summary>Implementación</summary>

```c
static int alloc_count = 0;

TreeNode *create_node_tracked(int value) {
    TreeNode *node = create_node(value);
    alloc_count++;
    return node;
}

void tree_free_tracked(TreeNode *root) {
    if (!root) return;
    tree_free_tracked(root->left);
    tree_free_tracked(root->right);
    free(root);
    alloc_count--;
}

// Después de tree_free_tracked(root):
// assert(alloc_count == 0);   // si falla, hay leak
```

Este patrón es un mini Valgrind manual.  Para un árbol de 6 nodos,
`alloc_count` debe subir a 6 durante la construcción y bajar a 0 después
del free.
</details>
