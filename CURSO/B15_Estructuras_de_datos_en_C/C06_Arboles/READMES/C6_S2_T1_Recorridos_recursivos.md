# Recorridos recursivos

## Los tres recorridos

Recorrer un árbol binario significa visitar todos los nodos en un orden
determinado.  La posición en la que se **procesa** el nodo actual respecto
a sus subárboles define tres recorridos fundamentales:

| Recorrido | Orden | Mnemotécnica |
|-----------|-------|-------------|
| **Preorden** | nodo → izquierda → derecha | **N**LR (Node-Left-Right) |
| **Inorden** | izquierda → nodo → derecha | L**N**R (Left-Node-Right) |
| **Postorden** | izquierda → derecha → nodo | LR**N** (Left-Right-Node) |

"Pre", "in", "post" indican **cuándo** se visita el nodo respecto a los
subárboles: antes, en medio, o después.

---

## Árbol de referencia

Usaremos este árbol para todas las trazas:

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

7 nodos, altura 2, perfecto.

---

## Preorden (NLR)

Visitar el nodo, luego el subárbol izquierdo, luego el derecho.

### En C

```c
void preorder(TreeNode *root) {
    if (!root) return;
    printf("%d ", root->data);    // visitar
    preorder(root->left);         // izquierda
    preorder(root->right);        // derecha
}
```

### En Rust

```rust
fn preorder<T: std::fmt::Display>(tree: &Tree<T>) {
    if let Some(node) = tree {
        print!("{} ", node.data);
        preorder(&node.left);
        preorder(&node.right);
    }
}
```

### Traza detallada

```
preorder(10)
  print 10
  preorder(5)
    print 5
    preorder(3)
      print 3
      preorder(NULL) → return
      preorder(NULL) → return
    preorder(7)
      print 7
      preorder(NULL) → return
      preorder(NULL) → return
  preorder(15)
    print 15
    preorder(12)
      print 12
      preorder(NULL) → return
      preorder(NULL) → return
    preorder(20)
      print 20
      preorder(NULL) → return
      preorder(NULL) → return
```

**Resultado: `10 5 3 7 15 12 20`**

Patrón: la raíz siempre es el **primer** elemento.  Preorden "desciende"
por la izquierda lo más profundo posible, luego retrocede y toma la derecha.

### Visualización del recorrido

```
         10 ①
        /  \
       5 ②  15 ⑤
      / \   / \
     3 ③ 7④ 12⑥ 20⑦
```

---

## Inorden (LNR)

Visitar el subárbol izquierdo, luego el nodo, luego el derecho.

### En C

```c
void inorder(TreeNode *root) {
    if (!root) return;
    inorder(root->left);          // izquierda
    printf("%d ", root->data);    // visitar
    inorder(root->right);        // derecha
}
```

### En Rust

```rust
fn inorder<T: std::fmt::Display>(tree: &Tree<T>) {
    if let Some(node) = tree {
        inorder(&node.left);
        print!("{} ", node.data);
        inorder(&node.right);
    }
}
```

### Traza detallada

```
inorder(10)
  inorder(5)
    inorder(3)
      inorder(NULL) → return
      print 3
      inorder(NULL) → return
    print 5
    inorder(7)
      inorder(NULL) → return
      print 7
      inorder(NULL) → return
  print 10
  inorder(15)
    inorder(12)
      inorder(NULL) → return
      print 12
      inorder(NULL) → return
    print 15
    inorder(20)
      inorder(NULL) → return
      print 20
      inorder(NULL) → return
```

**Resultado: `3 5 7 10 12 15 20`**

Los valores salen **ordenados**.  Esto no es coincidencia — este árbol es un
BST (árbol binario de búsqueda).  El recorrido inorden de un BST siempre
produce los valores en orden ascendente.  Es la propiedad fundamental de
los BST (tema de S03).

### Visualización del recorrido

```
         10 ④
        /  \
       5 ②  15 ⑥
      / \   / \
     3 ① 7③ 12⑤ 20⑦
```

---

## Postorden (LRN)

Visitar el subárbol izquierdo, luego el derecho, luego el nodo.

### En C

```c
void postorder(TreeNode *root) {
    if (!root) return;
    postorder(root->left);        // izquierda
    postorder(root->right);       // derecha
    printf("%d ", root->data);    // visitar
}
```

### En Rust

```rust
fn postorder<T: std::fmt::Display>(tree: &Tree<T>) {
    if let Some(node) = tree {
        postorder(&node.left);
        postorder(&node.right);
        print!("{} ", node.data);
    }
}
```

### Traza detallada

```
postorder(10)
  postorder(5)
    postorder(3)
      postorder(NULL) → return
      postorder(NULL) → return
      print 3
    postorder(7)
      postorder(NULL) → return
      postorder(NULL) → return
      print 7
    print 5
  postorder(15)
    postorder(12)
      postorder(NULL) → return
      postorder(NULL) → return
      print 12
    postorder(20)
      postorder(NULL) → return
      postorder(NULL) → return
      print 20
    print 15
  print 10
```

**Resultado: `3 7 5 12 20 15 10`**

Patrón: la raíz siempre es el **último** elemento.  Los hijos se procesan
antes que el padre — ideal para operaciones donde el padre depende de los
hijos (ej. liberar memoria, evaluar expresiones).

### Visualización del recorrido

```
         10 ⑦
        /  \
       5 ③  15 ⑥
      / \   / \
     3 ① 7② 12④ 20⑤
```

---

## Resumen de los tres recorridos

Para el árbol:

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

| Recorrido | Orden | Resultado | Raíz está... |
|-----------|-------|-----------|-------------|
| Preorden | NLR | `10 5 3 7 15 12 20` | Primera |
| Inorden | LNR | `3 5 7 10 12 15 20` | En medio |
| Postorden | LRN | `3 7 5 12 20 15 10` | Última |

---

## La pila de llamadas

Los tres recorridos usan la **pila de llamadas** (call stack) para "recordar"
dónde deben volver.  Cada llamada recursiva apila un frame:

```
Preorden — estado de la pila al visitar el nodo 3:

  ┌─────────────────────┐
  │ preorder(3)         │  ← actual: print 3
  ├─────────────────────┤
  │ preorder(5)         │  ya imprimió 5, falta right(7)
  ├─────────────────────┤
  │ preorder(10)        │  ya imprimió 10, falta right(15)
  ├─────────────────────┤
  │ main()              │
  └─────────────────────┘

  Profundidad de pila = profundidad del nodo + 1 = 3
```

La profundidad máxima de la pila es la **altura del árbol + 1**.  Para un
árbol balanceado de $n$ nodos: $O(\log n)$.  Para un árbol degenerado:
$O(n)$ — riesgo de stack overflow.

---

## Recorridos con acción genérica

En vez de imprimir, pasar una función que define la acción:

### En C: puntero a función

```c
typedef void (*VisitFn)(int data);

void preorder_fn(TreeNode *root, VisitFn visit) {
    if (!root) return;
    visit(root->data);
    preorder_fn(root->left, visit);
    preorder_fn(root->right, visit);
}

void inorder_fn(TreeNode *root, VisitFn visit) {
    if (!root) return;
    inorder_fn(root->left, visit);
    visit(root->data);
    inorder_fn(root->right, visit);
}

void postorder_fn(TreeNode *root, VisitFn visit) {
    if (!root) return;
    postorder_fn(root->left, visit);
    postorder_fn(root->right, visit);
    visit(root->data);
}

// Uso:
void print_val(int data) { printf("%d ", data); }
void double_print(int data) { printf("%d ", data * 2); }

preorder_fn(root, print_val);      // 10 5 3 7 15 12 20
inorder_fn(root, double_print);    // 6 10 14 20 24 30 40
```

### En Rust: closures

```rust
fn preorder_with<T, F: FnMut(&T)>(tree: &Tree<T>, f: &mut F) {
    if let Some(node) = tree {
        f(&node.data);
        preorder_with(&node.left, f);
        preorder_with(&node.right, f);
    }
}

fn inorder_with<T, F: FnMut(&T)>(tree: &Tree<T>, f: &mut F) {
    if let Some(node) = tree {
        inorder_with(&node.left, f);
        f(&node.data);
        inorder_with(&node.right, f);
    }
}

fn postorder_with<T, F: FnMut(&T)>(tree: &Tree<T>, f: &mut F) {
    if let Some(node) = tree {
        postorder_with(&node.left, f);
        postorder_with(&node.right, f);
        f(&node.data);
    }
}
```

```rust
// Recolectar en un Vec
let mut result = Vec::new();
inorder_with(&tree, &mut |val| result.push(*val));
// result: [3, 5, 7, 10, 12, 15, 20]
```

La closure captura `result` por referencia mutable — puede acumular
estado durante el recorrido.

---

## Recolectar en un Vec

Patrón útil: recorrer y recolectar los valores en un vector:

```rust
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
```

```c
// En C: pasar array y puntero a índice
void inorder_array(TreeNode *root, int *arr, int *idx) {
    if (!root) return;
    inorder_array(root->left, arr, idx);
    arr[(*idx)++] = root->data;
    inorder_array(root->right, arr, idx);
}

// Uso:
int arr[100];
int idx = 0;
inorder_array(root, arr, &idx);
// arr[0..idx] contiene el recorrido inorden
```

---

## Recorridos con contexto: acumular resultado

### Suma con preorden

```c
int preorder_sum(TreeNode *root) {
    if (!root) return 0;
    return root->data + preorder_sum(root->left) + preorder_sum(root->right);
}
```

El orden (pre/in/post) no afecta el resultado de la suma — la suma es
conmutativa y asociativa.  Solo importa que cada nodo se visite una vez.

### Máximo con cualquier recorrido

```c
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

### Comprobar si un valor existe

```c
int tree_contains(TreeNode *root, int value) {
    if (!root) return 0;
    if (root->data == value) return 1;
    return tree_contains(root->left, value)
        || tree_contains(root->right, value);
}
```

Short-circuit: si lo encuentra en la izquierda, no recorre la derecha.

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

void preorder(TreeNode *root) {
    if (!root) return;
    printf("%d ", root->data);
    preorder(root->left);
    preorder(root->right);
}

void inorder(TreeNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

void postorder(TreeNode *root) {
    if (!root) return;
    postorder(root->left);
    postorder(root->right);
    printf("%d ", root->data);
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
    //     / \   / \
    //    3   7 12  20

    TreeNode *root = create_node(10);
    root->left = create_node(5);
    root->right = create_node(15);
    root->left->left = create_node(3);
    root->left->right = create_node(7);
    root->right->left = create_node(12);
    root->right->right = create_node(20);

    printf("Preorder:  ");
    preorder(root);
    printf("\n");

    printf("Inorder:   ");
    inorder(root);
    printf("\n");

    printf("Postorder: ");
    postorder(root);
    printf("\n");

    tree_free(root);
    return 0;
}
```

Salida:

```
Preorder:  10 5 3 7 15 12 20
Inorder:   3 5 7 10 12 15 20
Postorder: 3 7 5 12 20 15 10
```

---

## Cuándo usar cada recorrido

| Recorrido | Caso de uso | Por qué |
|-----------|-----------|---------|
| **Preorden** | Copiar/serializar un árbol | La raíz se procesa primero — reconstrucción natural |
| **Preorden** | Imprimir estructura jerárquica | Padre antes que hijos |
| **Inorden** | Obtener valores ordenados de un BST | Propiedad BST: left < node < right |
| **Inorden** | Verificar si un árbol es BST | Los valores deben salir en orden creciente |
| **Postorden** | Liberar memoria (`tree_free`) | Hijos se liberan antes que el padre |
| **Postorden** | Evaluar expresiones | Operandos antes que operador |
| **Postorden** | Calcular propiedades dependientes (altura, tamaño) | El resultado del padre depende de los hijos |

---

## Ejercicios

### Ejercicio 1 — Traza manual

Dado el árbol:

```
        A
       / \
      B   C
     /   / \
    D   E   F
```

Escribe el resultado de preorden, inorden y postorden sin ejecutar código.

<details>
<summary>Resultados</summary>

```
Preorden:  A B D C E F
Inorden:   D B A E C F
Postorden: D B E F C A
```

Verificar: en preorden, A (raíz) es primero.  En postorden, A es último.
En inorden, A está entre el subárbol izquierdo {D, B} y el derecho {E, C, F}.
</details>

### Ejercicio 2 — Recorrido de un solo nodo

¿Cuál es el resultado de los tres recorridos para un árbol de un solo
nodo con valor 42?

<details>
<summary>Respuesta</summary>

Los tres dan: `42`.

Un solo nodo no tiene subárboles — la posición del "visitar" respecto a
los subárboles no importa.  Los tres recorridos coinciden.
</details>

### Ejercicio 3 — Árbol degenerado

Escribe los tres recorridos para:

```
  1
   \
    2
     \
      3
       \
        4
```

<details>
<summary>Resultados</summary>

```
Preorden:  1 2 3 4
Inorden:   1 2 3 4
Postorden: 4 3 2 1
```

Para un árbol degenerado hacia la derecha, preorden e inorden coinciden
(no hay subárbol izquierdo que procesar antes del nodo).  Postorden
produce el orden inverso.
</details>

### Ejercicio 4 — Implementar en Rust

Implementa los tres recorridos en Rust usando el estilo struct
(`Option<Box<TreeNode<T>>>`).  Recolecta los resultados en `Vec<T>`.

<details>
<summary>Uso</summary>

```rust
let tree = build_sample();   // [10, 5, 15, 3, 7, 12, 20]

let pre = preorder_vec(&tree);    // [10, 5, 3, 7, 15, 12, 20]
let ino = inorder_vec(&tree);     // [3, 5, 7, 10, 12, 15, 20]
let post = postorder_vec(&tree);  // [3, 7, 5, 12, 20, 15, 10]
```
</details>

### Ejercicio 5 — Pila de llamadas

Para el recorrido inorden del árbol de referencia, dibuja el estado de la
pila de llamadas en el momento en que se imprime el valor 10 (la raíz).

<details>
<summary>Estado de la pila</summary>

```
Al imprimir 10:

  ┌─────────────┐
  │ inorder(10) │  ← acaba de volver de left(5), va a print 10
  ├─────────────┤
  │ main()      │
  └─────────────┘

  Profundidad: 2 (solo main + inorder(10))
```

En inorden, la raíz se procesa después de todo el subárbol izquierdo.
Cuando se imprime 10, todas las llamadas recursivas del subárbol izquierdo
ya han terminado y desapilado.  Solo queda `inorder(10)` esperando para
procesar su derecha.
</details>

### Ejercicio 6 — Profundidad máxima de pila

¿Cuál es la profundidad máxima de la pila de llamadas para cada recorrido
sobre el árbol de referencia?  ¿Es diferente entre preorden, inorden y
postorden?

<details>
<summary>Respuesta</summary>

La profundidad máxima es la misma para los tres: **altura + 1 = 3**.

Los tres recorridos recorren las mismas ramas — la diferencia es solo
cuándo se ejecuta el `printf` dentro de cada frame, no cuántos frames
hay.  El nodo más profundo (ej. 3, profundidad 2) requiere 3 frames
activos: `main → recorrido(10) → recorrido(5) → recorrido(3)`.
</details>

### Ejercicio 7 — tree_free es postorden

Explica por qué `tree_free` **debe** ser postorden.  ¿Qué pasa si usas
preorden o inorden para liberar?

<details>
<summary>Explicación</summary>

Postorden garantiza: los hijos se liberan **antes** que el padre.

Preorden: `free(root)` primero → `root->left` y `root->right` son
use-after-free.  Los punteros del nodo ya no son válidos.

Inorden: `inorder(left)` libera todo el subárbol izquierdo → `free(root)` →
`inorder(right)` es use-after-free (root ya está liberado, `root->right`
es inválido).

Solo postorden es seguro porque el nodo se libera después de que ya no se
necesitan sus punteros `left` y `right`.
</details>

### Ejercicio 8 — Callback con contexto

Implementa en C un recorrido inorden que acumule la suma de todos los
valores usando un puntero a contexto (`void *ctx`):

```c
typedef void (*VisitCtxFn)(int data, void *ctx);
void inorder_ctx(TreeNode *root, VisitCtxFn visit, void *ctx);
```

<details>
<summary>Implementación</summary>

```c
void inorder_ctx(TreeNode *root, VisitCtxFn visit, void *ctx) {
    if (!root) return;
    inorder_ctx(root->left, visit, ctx);
    visit(root->data, ctx);
    inorder_ctx(root->right, visit, ctx);
}

void accumulate(int data, void *ctx) {
    *(int *)ctx += data;
}

// Uso:
int total = 0;
inorder_ctx(root, accumulate, &total);
printf("Sum: %d\n", total);   // 72
```

En Rust, la closure captura el contexto directamente — no necesita `void *`.
</details>

### Ejercicio 9 — Reconstruir recorridos

Dados preorden `[A, B, D, E, C, F]` e inorden `[D, B, E, A, F, C]`,
reconstruye el árbol.

<details>
<summary>Método</summary>

1. En preorden, el primer elemento es la raíz: **A**.
2. En inorden, A divide: izquierdo = `[D, B, E]`, derecho = `[F, C]`.
3. En preorden, los siguientes 3 elementos son izquierdo: `[B, D, E]`,
   y los 2 restantes son derecho: `[C, F]`.
4. Repetir recursivamente:
   - Izq: raíz=B, inorden izq=[D], inorden der=[E]
   - Der: raíz=C, inorden izq=[F], inorden der=[]

```
        A
       / \
      B   C
     / \ /
    D  E F
```

La combinación de preorden + inorden reconstruye el árbol de forma única
(asumiendo valores distintos).  Tema de T04.
</details>

### Ejercicio 10 — Verificar BST con inorden

Un BST es válido si y solo si su recorrido inorden produce valores en orden
estrictamente creciente.  Implementa `is_bst(root)` usando esta propiedad.

<details>
<summary>Implementación</summary>

```c
int is_bst_helper(TreeNode *root, int *prev) {
    if (!root) return 1;

    if (!is_bst_helper(root->left, prev)) return 0;

    if (*prev != INT_MIN && root->data <= *prev) return 0;
    *prev = root->data;

    return is_bst_helper(root->right, prev);
}

int is_bst(TreeNode *root) {
    int prev = INT_MIN;
    return is_bst_helper(root, &prev);
}
```

```rust
fn is_bst(tree: &Tree<i32>) -> bool {
    fn check(tree: &Tree<i32>, prev: &mut Option<i32>) -> bool {
        match tree {
            None => true,
            Some(node) => {
                if !check(&node.left, prev) { return false; }
                if let Some(p) = prev {
                    if node.data <= *p { return false; }
                }
                *prev = Some(node.data);
                check(&node.right, prev)
            }
        }
    }
    check(tree, &mut None)
}
```

Recorrido inorden que mantiene el valor previo.  Si en algún punto el
valor actual no es mayor que el previo, no es un BST.
</details>
