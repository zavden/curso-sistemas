# Aplicaciones de recorridos

## Recorridos como herramientas

Los recorridos (preorden, inorden, postorden, BFS) no solo sirven para
imprimir valores.  Son la base de casi toda operación sobre árboles.
Cada recorrido tiene aplicaciones naturales según **cuándo** procesa el
nodo respecto a sus hijos:

| Recorrido | Procesa el nodo... | Aplicación natural |
|-----------|-------------------|-------------------|
| Preorden | Antes que los hijos | Copiar, serializar, imprimir estructura |
| Inorden | Entre los hijos | Obtener orden en BST, verificar BST |
| Postorden | Después de los hijos | Liberar, evaluar, calcular propiedades |
| BFS | Nivel por nivel | Serialización por niveles, ancho, profundidad mínima |

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

## Serialización (preorden)

Serializar un árbol es convertirlo en una secuencia lineal que permita
reconstruirlo.  Preorden es ideal porque procesa la raíz primero —
al reconstruir, el primer elemento define la raíz.

### El problema de la ambigüedad

Solo preorden no basta.  `10 5 3 7 15 12 20` podría corresponder a
muchos árboles distintos.  Necesitamos marcar **dónde están los NULLs**
para que la estructura sea única.

### Preorden con marcadores NULL

Usar un valor centinela (ej. `#` o `-1`) para representar un subárbol
vacío:

```
Preorden con NULLs:

  10 5 3 # # 7 # # 15 12 # # 20 # #
```

Cada nodo genera exactamente: `valor left right`.  Las hojas generan
`valor # #`.  Los NULLs generan `#`.

### En C

```c
#define MARKER -1  // centinela (asume que -1 no es un dato válido)

// Serializar a un array
void serialize(TreeNode *root, int *arr, int *idx) {
    if (!root) {
        arr[(*idx)++] = MARKER;
        return;
    }
    arr[(*idx)++] = root->data;
    serialize(root->left, arr, idx);
    serialize(root->right, arr, idx);
}
```

Para el árbol de referencia (7 nodos, 8 NULLs = 15 posiciones):

```
arr: [10, 5, 3, -1, -1, 7, -1, -1, 15, 12, -1, -1, 20, -1, -1]
idx:   0  1  2   3   4  5   6   7   8   9  10  11  12  13  14
```

Un árbol con $n$ nodos produce $n$ valores + $(n + 1)$ marcadores =
$2n + 1$ elementos totales.  Los $n + 1$ marcadores corresponden a los
$n + 1$ punteros NULL de un árbol binario (cada hoja tiene 2, cada nodo
con un hijo tiene 1).

### En Rust

```rust
fn serialize(tree: &Tree<i32>) -> Vec<Option<i32>> {
    let mut result = Vec::new();

    fn ser(tree: &Tree<i32>, out: &mut Vec<Option<i32>>) {
        match tree {
            None => out.push(None),
            Some(node) => {
                out.push(Some(node.data));
                ser(&node.left, out);
                ser(&node.right, out);
            }
        }
    }

    ser(tree, &mut result);
    result
}
```

`Option<i32>` elimina la necesidad de un centinela — `None` es el
marcador natural.

```rust
// [Some(10), Some(5), Some(3), None, None, Some(7), None, None,
//  Some(15), Some(12), None, None, Some(20), None, None]
```

### Serialización a string

Para persistir en disco o transmitir por red:

```c
void serialize_str(TreeNode *root, char *buf, int *pos) {
    if (!root) {
        *pos += sprintf(buf + *pos, "# ");
        return;
    }
    *pos += sprintf(buf + *pos, "%d ", root->data);
    serialize_str(root->left, buf, pos);
    serialize_str(root->right, buf, pos);
}

// Resultado: "10 5 3 # # 7 # # 15 12 # # 20 # #"
```

---

## Deserialización (reconstruir desde preorden)

El proceso inverso: dado el array serializado, reconstruir el árbol.

### Algoritmo

Recorrer el array secuencialmente.  Cada llamada recursiva consume un
elemento:
- Si es marcador → retornar NULL.
- Si es un valor → crear nodo, recurrir para left, recurrir para right.

```c
TreeNode *deserialize(int *arr, int *idx) {
    if (arr[*idx] == MARKER) {
        (*idx)++;
        return NULL;
    }

    TreeNode *node = create_node(arr[(*idx)++]);
    node->left = deserialize(arr, idx);
    node->right = deserialize(arr, idx);
    return node;
}

// Uso:
int idx = 0;
TreeNode *copy = deserialize(arr, &idx);
```

### Traza

```
arr: [10, 5, 3, -1, -1, 7, -1, -1, 15, 12, -1, -1, 20, -1, -1]

deserialize(idx=0): arr[0]=10, crear nodo(10)
  left = deserialize(idx=1): arr[1]=5, crear nodo(5)
    left = deserialize(idx=2): arr[2]=3, crear nodo(3)
      left = deserialize(idx=3): arr[3]=-1 → NULL
      right = deserialize(idx=4): arr[4]=-1 → NULL
      retornar nodo(3)
    right = deserialize(idx=5): arr[5]=7, crear nodo(7)
      left = deserialize(idx=6): arr[6]=-1 → NULL
      right = deserialize(idx=7): arr[7]=-1 → NULL
      retornar nodo(7)
    retornar nodo(5)
  right = deserialize(idx=8): arr[8]=15, crear nodo(15)
    left = deserialize(idx=9): arr[9]=12, crear nodo(12)
      left = deserialize(idx=10): arr[10]=-1 → NULL
      right = deserialize(idx=11): arr[11]=-1 → NULL
      retornar nodo(12)
    right = deserialize(idx=12): arr[12]=20, crear nodo(20)
      left = deserialize(idx=13): arr[13]=-1 → NULL
      right = deserialize(idx=14): arr[14]=-1 → NULL
      retornar nodo(20)
    retornar nodo(15)
  retornar nodo(10)
```

El árbol se reconstruye exactamente como el original.  El `idx`
compartido (pasado por puntero) garantiza que cada elemento se consume
una sola vez.

### En Rust

```rust
fn deserialize(data: &[Option<i32>], idx: &mut usize) -> Tree<i32> {
    if *idx >= data.len() {
        return None;
    }

    match data[*idx] {
        None => {
            *idx += 1;
            None
        }
        Some(val) => {
            *idx += 1;
            let left = deserialize(data, idx);
            let right = deserialize(data, idx);
            Some(Box::new(TreeNode {
                data: val,
                left,
                right,
            }))
        }
    }
}
```

### Invariante

`serialize` seguido de `deserialize` produce un árbol idéntico al
original.  `deserialize` seguido de `serialize` produce el mismo array.
Son funciones inversas:

$$\text{deserialize}(\text{serialize}(T)) = T$$

---

## Serialización por niveles (BFS)

Alternativa a preorden — serializar en orden BFS.  Es el formato que
usa LeetCode para representar árboles:

```
BFS con NULLs: [10, 5, 15, 3, 7, 12, 20]

Para un árbol incompleto:
       10
      /  \
     5    15
    /       \
   3        20

BFS: [10, 5, 15, 3, null, null, 20]
```

### Serializar BFS en C

```c
void serialize_bfs(TreeNode *root, int *arr, int *size) {
    if (!root) { *size = 0; return; }

    // Cola de punteros (NULL permitido)
    TreeNode *queue[256];
    int front = 0, rear = 0;
    queue[rear++] = root;
    *size = 0;

    while (front < rear) {
        TreeNode *node = queue[front++];

        if (node) {
            arr[(*size)++] = node->data;
            queue[rear++] = node->left;   // puede ser NULL
            queue[rear++] = node->right;
        } else {
            arr[(*size)++] = MARKER;
        }
    }

    // Eliminar marcadores finales (trailing NULLs)
    while (*size > 0 && arr[*size - 1] == MARKER) {
        (*size)--;
    }
}
```

### Deserializar BFS

```c
TreeNode *deserialize_bfs(int *arr, int size) {
    if (size == 0 || arr[0] == MARKER) return NULL;

    TreeNode *root = create_node(arr[0]);
    TreeNode *queue[256];
    int front = 0, rear = 0;
    queue[rear++] = root;
    int i = 1;

    while (front < rear && i < size) {
        TreeNode *parent = queue[front++];

        // Hijo izquierdo
        if (i < size && arr[i] != MARKER) {
            parent->left = create_node(arr[i]);
            queue[rear++] = parent->left;
        }
        i++;

        // Hijo derecho
        if (i < size && arr[i] != MARKER) {
            parent->right = create_node(arr[i]);
            queue[rear++] = parent->right;
        }
        i++;
    }

    return root;
}
```

### Preorden vs BFS para serializar

| Aspecto | Preorden con NULLs | BFS con NULLs |
|---------|-------------------|---------------|
| Tamaño del output | $2n + 1$ siempre | Variable (trailing NULLs eliminados) |
| Reconstrucción | Recursiva (natural) | Iterativa con cola |
| Legibilidad | Difícil de visualizar | Fácil (corresponde a niveles) |
| Formato estándar | Compiladores, serializadores | LeetCode, visualizadores |

---

## Copia de un árbol (preorden)

Copiar un árbol es crear un nuevo árbol con la misma estructura y
valores, pero con nodos independientes en memoria.

### En C

```c
TreeNode *tree_copy(TreeNode *root) {
    if (!root) return NULL;

    TreeNode *copy = create_node(root->data);       // preorden: nodo primero
    copy->left = tree_copy(root->left);
    copy->right = tree_copy(root->right);
    return copy;
}
```

Es preorden: crear el nodo, luego recursivamente copiar izquierda y
derecha.  El resultado es una **copia profunda** — modificar la copia
no afecta al original.

### En Rust

```rust
fn tree_copy(tree: &Tree<i32>) -> Tree<i32> {
    match tree {
        None => None,
        Some(node) => Some(Box::new(TreeNode {
            data: node.data,
            left: tree_copy(&node.left),
            right: tree_copy(&node.right),
        })),
    }
}
```

En Rust idiomático, si `T: Clone`, se puede implementar `Clone` para
`TreeNode`:

```rust
impl<T: Clone> Clone for TreeNode<T> {
    fn clone(&self) -> Self {
        TreeNode {
            data: self.data.clone(),
            left: self.left.clone(),    // Option<Box<T>> implementa Clone si T: Clone
            right: self.right.clone(),
        }
    }
}

// Uso:
let copy = tree.clone();   // copia profunda automática
```

`Box<T>` implementa `Clone` si `T: Clone`, y `Option<T>` implementa
`Clone` si `T: Clone`.  La recursión es implícita en las llamadas a
`clone()` de cada subárbol.

### Verificar la independencia

```c
TreeNode *original = /* construir árbol */;
TreeNode *copy = tree_copy(original);

// Modificar la copia no afecta al original
copy->data = 999;
printf("Original root: %d\n", original->data);   // 10
printf("Copy root: %d\n", copy->data);            // 999

// Cada uno debe liberarse por separado
tree_free(original);
tree_free(copy);
```

---

## Igualdad de árboles (cualquier recorrido)

Dos árboles son iguales si tienen la misma estructura y los mismos
valores en cada posición.

### En C

```c
int tree_equal(TreeNode *a, TreeNode *b) {
    if (!a && !b) return 1;             // ambos vacíos
    if (!a || !b) return 0;             // uno vacío, otro no
    return a->data == b->data
        && tree_equal(a->left, b->left)
        && tree_equal(a->right, b->right);
}
```

Short-circuit: si las raíces difieren, no recorre los subárboles.

### En Rust

```rust
fn tree_equal(a: &Tree<i32>, b: &Tree<i32>) -> bool {
    match (a, b) {
        (None, None) => true,
        (Some(na), Some(nb)) => {
            na.data == nb.data
                && tree_equal(&na.left, &nb.left)
                && tree_equal(&na.right, &nb.right)
        }
        _ => false,   // uno None, otro Some
    }
}
```

Con `PartialEq` derivado:

```rust
#[derive(PartialEq)]
struct TreeNode<T: PartialEq> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}

// El derive genera automáticamente la comparación recursiva
let equal = tree_a == tree_b;
```

### Complejidad

Mejor caso: $O(1)$ (raíces distintas).  Peor caso: $O(n)$ (árboles
idénticos — debe comparar todo).  Caso promedio: depende de cuándo
encuentra la primera diferencia.

---

## Espejo (mirror) de un árbol (postorden)

Invertir un árbol: intercambiar los subárboles izquierdo y derecho de
cada nodo.

```
Original:              Espejo:
       10                   10
      /  \                 /  \
     5    15             15    5
    / \   / \           / \   / \
   3   7 12  20       20  12 7   3
```

### En C (in-place)

```c
void tree_mirror(TreeNode *root) {
    if (!root) return;

    tree_mirror(root->left);
    tree_mirror(root->right);

    // Intercambiar hijos (postorden: después de recurrir)
    TreeNode *temp = root->left;
    root->left = root->right;
    root->right = temp;
}
```

Es postorden: primero se reflejan los subárboles, luego se
intercambian.  Funciona también con preorden (intercambiar primero,
luego recurrir en los ya intercambiados).  **No funciona con inorden**
— intercambiar entre las dos recursiones provoca que el subárbol
izquierdo (ya reflejado) se mueva a la derecha y se refleje otra vez.

### En Rust (nueva copia)

```rust
fn tree_mirror(tree: &Tree<i32>) -> Tree<i32> {
    match tree {
        None => None,
        Some(node) => Some(Box::new(TreeNode {
            data: node.data,
            left: tree_mirror(&node.right),     // derecha → izquierda
            right: tree_mirror(&node.left),     // izquierda → derecha
        })),
    }
}
```

### Propiedad

El espejo del espejo es el árbol original:
$\text{mirror}(\text{mirror}(T)) = T$.

El inorden del espejo es el **reverso** del inorden del original:
- Original: `3 5 7 10 12 15 20`
- Espejo: `20 15 12 10 7 5 3`

---

## Liberación de memoria (postorden)

Ya cubierto en T01, pero formalizado aquí como aplicación.  Liberar un
árbol **debe** ser postorden — el nodo se libera después de sus hijos:

```c
void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);      // liberar subárbol izquierdo
    tree_free(root->right);     // liberar subárbol derecho
    free(root);                 // liberar el nodo actual
}
```

### ¿Por qué solo postorden?

| Orden | ¿Funciona? | Motivo |
|-------|-----------|--------|
| Preorden | No | `free(root)` invalida `root->left` y `root->right` → use-after-free |
| Inorden | No | Después de `free(root)`, `root->right` es inválido |
| Postorden | Sí | `root->left` y `root->right` se leen antes de `free(root)` |
| BFS | Sí (con cola) | Cada nodo se desencola, sus hijos se encolan, luego se libera |

### Liberación con BFS

Alternativa válida — los hijos se encolan antes de liberar el padre:

```c
void tree_free_bfs(TreeNode *root) {
    if (!root) return;

    TreeNode *queue[256];
    int front = 0, rear = 0;
    queue[rear++] = root;

    while (front < rear) {
        TreeNode *node = queue[front++];
        if (node->left)  queue[rear++] = node->left;
        if (node->right) queue[rear++] = node->right;
        free(node);
    }
}
```

Funciona porque `node->left` y `node->right` se leen (encolados) antes
del `free(node)`.  Usa más memoria que postorden ($O(w)$ vs $O(h)$).

### Verificación con Valgrind

```bash
$ gcc -g tree.c -o tree
$ valgrind --leak-check=full ./tree
==12345== All heap blocks were freed -- no leaks are possible
```

Si `tree_free` es correcto, Valgrind reporta cero fugas.  Si se usa
preorden para liberar, Valgrind detecta lecturas inválidas
(use-after-free).

---

## Transformación (map)

Aplicar una función a cada dato del árbol, creando un nuevo árbol con
la misma estructura:

### En C

```c
typedef int (*MapFn)(int);

TreeNode *tree_map(TreeNode *root, MapFn f) {
    if (!root) return NULL;

    TreeNode *node = create_node(f(root->data));
    node->left = tree_map(root->left, f);
    node->right = tree_map(root->right, f);
    return node;
}

int double_val(int x) { return x * 2; }
int negate(int x)     { return -x; }

TreeNode *doubled = tree_map(root, double_val);
// doubled:  20 10 6 14 30 24 40
TreeNode *negated = tree_map(root, negate);
// negated: -10 -5 -3 -7 -15 -12 -20
```

### En Rust

```rust
fn tree_map<T, U, F: Fn(&T) -> U>(tree: &Tree<T>, f: &F) -> Tree<U> {
    match tree {
        None => None,
        Some(node) => Some(Box::new(TreeNode {
            data: f(&node.data),
            left: tree_map(&node.left, f),
            right: tree_map(&node.right, f),
        })),
    }
}

let doubled = tree_map(&tree, &|x| x * 2);
let as_str = tree_map(&tree, &|x: &i32| x.to_string());
// as_str es Tree<String>: "10" "5" "3" ...
```

Puede cambiar el tipo: `Tree<i32>` → `Tree<String>`.

---

## Fold (reducir)

Combinar todos los valores del árbol en un solo resultado:

```rust
fn tree_fold<T, R, F: Fn(R, &T, R) -> R>(
    tree: &Tree<T>,
    init: R,
    f: &F,
) -> R
where
    R: Clone,
{
    match tree {
        None => init,
        Some(node) => {
            let left_result = tree_fold(&node.left, init.clone(), f);
            let right_result = tree_fold(&node.right, init.clone(), f);
            f(left_result, &node.data, right_result)
        }
    }
}

// Suma:
let sum = tree_fold(&tree, 0, &|l, val, r| l + val + r);
// 72

// Contar nodos:
let count = tree_fold(&tree, 0, &|l, _, r| l + 1 + r);
// 7

// Altura:
let height = tree_fold(&tree, -1i32, &|l, _, r| 1 + l.max(r));
// 2
```

`fold` generaliza muchas operaciones: la función `f` recibe el resultado
del subárbol izquierdo, el dato del nodo, y el resultado del subárbol
derecho.

---

## Reconstrucción desde dos recorridos

En T01 (ejercicio 9) vimos que **preorden + inorden** reconstruyen un
árbol.  Aquí formalizamos el algoritmo:

### Preorden + inorden → árbol

**Requisito**: todos los valores deben ser distintos.

```
Preorden: [10, 5, 3, 7, 15, 12, 20]
Inorden:  [3, 5, 7, 10, 12, 15, 20]
```

Algoritmo:
1. El primer elemento de preorden es la raíz: `10`.
2. Buscar `10` en inorden → posición 3.
3. Todo a la izquierda de la posición 3 en inorden es el subárbol
   izquierdo: `[3, 5, 7]` (3 elementos).
4. Los siguientes 3 elementos en preorden son el preorden del subárbol
   izquierdo: `[5, 3, 7]`.
5. El resto de preorden es el preorden del subárbol derecho:
   `[15, 12, 20]`.
6. Repetir recursivamente para cada subárbol.

### En C

```c
TreeNode *build_tree(int *preorder, int pre_start, int pre_end,
                     int *inorder,  int in_start,  int in_end) {
    if (pre_start > pre_end) return NULL;

    int root_val = preorder[pre_start];
    TreeNode *root = create_node(root_val);

    // Buscar root_val en inorden
    int in_root = in_start;
    while (inorder[in_root] != root_val) in_root++;

    int left_size = in_root - in_start;

    root->left = build_tree(preorder, pre_start + 1, pre_start + left_size,
                            inorder,  in_start,      in_root - 1);

    root->right = build_tree(preorder, pre_start + left_size + 1, pre_end,
                             inorder,  in_root + 1,               in_end);

    return root;
}

// Uso:
int pre[] = {10, 5, 3, 7, 15, 12, 20};
int ino[] = {3, 5, 7, 10, 12, 15, 20};
TreeNode *root = build_tree(pre, 0, 6, ino, 0, 6);
```

### ¿Qué pares de recorridos funcionan?

| Par | ¿Reconstruye? | Motivo |
|-----|-------------|--------|
| Preorden + inorden | Sí | Preorden da la raíz, inorden divide izq/der |
| Postorden + inorden | Sí | Postorden da la raíz (último), inorden divide |
| Preorden + postorden | No (en general) | Sin inorden no se puede dividir izq/der |
| BFS + inorden | Sí | BFS da la raíz (primero), inorden divide |

Preorden + postorden funciona solo si el árbol es **completo** (cada
nodo tiene 0 o 2 hijos).

---

## Resumen de aplicaciones por recorrido

| Aplicación | Recorrido | Por qué |
|-----------|-----------|---------|
| Serializar | Preorden | La raíz primero facilita la reconstrucción |
| Deserializar | Preorden | Consumir en el mismo orden que se serializó |
| Copiar | Preorden | Crear nodo antes de recurrir en hijos |
| Igualdad | Cualquiera | Comparar nodo + recurrir; preorden permite short-circuit temprano |
| Espejo | Pre o postorden | Intercambiar hijos; **no** inorden |
| Liberar | Postorden | Leer punteros antes de `free` |
| Evaluar expresión | Postorden | Operandos antes que operador |
| Calcular altura/tamaño | Postorden | Resultado del padre depende de los hijos |
| Map (transformar) | Preorden | Crear nuevo nodo, recurrir |
| Fold (reducir) | Postorden | Combinar resultados de los hijos con el nodo |
| Obtener orden BST | Inorden | Propiedad BST: izq < nodo < der |
| Imprimir niveles | BFS | Natural por definición |

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

// --- Serialización (preorden con marcadores) ---
#define MARKER -1

void serialize(TreeNode *root, int *arr, int *idx) {
    if (!root) {
        arr[(*idx)++] = MARKER;
        return;
    }
    arr[(*idx)++] = root->data;
    serialize(root->left, arr, idx);
    serialize(root->right, arr, idx);
}

TreeNode *deserialize(int *arr, int *idx) {
    if (arr[*idx] == MARKER) {
        (*idx)++;
        return NULL;
    }
    TreeNode *node = create_node(arr[(*idx)++]);
    node->left = deserialize(arr, idx);
    node->right = deserialize(arr, idx);
    return node;
}

// --- Copia ---
TreeNode *tree_copy(TreeNode *root) {
    if (!root) return NULL;
    TreeNode *copy = create_node(root->data);
    copy->left = tree_copy(root->left);
    copy->right = tree_copy(root->right);
    return copy;
}

// --- Igualdad ---
int tree_equal(TreeNode *a, TreeNode *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return a->data == b->data
        && tree_equal(a->left, b->left)
        && tree_equal(a->right, b->right);
}

// --- Espejo (in-place) ---
void tree_mirror(TreeNode *root) {
    if (!root) return;
    tree_mirror(root->left);
    tree_mirror(root->right);
    TreeNode *temp = root->left;
    root->left = root->right;
    root->right = temp;
}

// --- Auxiliares ---
void inorder(TreeNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
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

    // Serializar
    int arr[32];
    int idx = 0;
    serialize(root, arr, &idx);
    printf("Serialized (%d elements): ", idx);
    for (int i = 0; i < idx; i++) {
        if (arr[i] == MARKER) printf("# ");
        else printf("%d ", arr[i]);
    }
    printf("\n");

    // Deserializar
    int di = 0;
    TreeNode *rebuilt = deserialize(arr, &di);
    printf("Rebuilt inorder:    ");
    inorder(rebuilt);
    printf("\n");

    // Copiar y verificar igualdad
    TreeNode *copy = tree_copy(root);
    printf("Copy inorder:       ");
    inorder(copy);
    printf("\n");
    printf("Equal to original?  %s\n", tree_equal(root, copy) ? "yes" : "no");

    // Modificar copia, verificar independencia
    copy->data = 999;
    printf("After modifying copy:\n");
    printf("  Original root:    %d\n", root->data);
    printf("  Copy root:        %d\n", copy->data);
    printf("  Still equal?      %s\n", tree_equal(root, copy) ? "yes" : "no");

    // Espejo
    tree_mirror(root);
    printf("Mirror inorder:     ");
    inorder(root);
    printf("\n");

    tree_free(root);
    tree_free(copy);
    tree_free(rebuilt);
    return 0;
}
```

Salida:

```
Serialized (15 elements): 10 5 3 # # 7 # # 15 12 # # 20 # #
Rebuilt inorder:    3 5 7 10 12 15 20
Copy inorder:       3 5 7 10 12 15 20
Equal to original?  yes
After modifying copy:
  Original root:    10
  Copy root:        999
  Still equal?      no
Mirror inorder:     20 15 12 10 7 5 3
```

---

## Ejercicios

### Ejercicio 1 — Serializar a mano

Escribe la serialización preorden con marcadores (`#`) para este árbol:

```
      A
     / \
    B   C
   /
  D
```

¿Cuántos elementos tiene la secuencia?

<details>
<summary>Predicción</summary>

```
A B D # # # C # #
```

4 nodos + 5 marcadores = **9 elementos**.

Traza: A → B → D → # (left de D) → # (right de D) → # (right de B) →
C → # (left de C) → # (right de C).

Fórmula: $2n + 1 = 2(4) + 1 = 9$.
</details>

### Ejercicio 2 — Deserializar

Dado `[5, 3, 1, #, #, 4, #, #, 8, #, 9, #, #]`, reconstruye el árbol.

<details>
<summary>Resultado</summary>

```
      5
     / \
    3   8
   / \    \
  1   4    9
```

Traza: 5=raíz.  Left: 3=raíz izq.  Left de 3: 1=hoja (#, #).  Right
de 3: 4=hoja (#, #).  Right de 5: 8.  Left de 8: # (NULL).  Right de
8: 9=hoja (#, #).
</details>

### Ejercicio 3 — Serializar + deserializar en Rust

Implementa `serialize` y `deserialize` en Rust usando
`Vec<Option<i32>>`.  Verifica que el árbol reconstruido es igual al
original.

<details>
<summary>Verificación</summary>

```rust
let tree = build_sample();   // [10, 5, 15, 3, 7, 12, 20]
let data = serialize(&tree);
let mut idx = 0;
let rebuilt = deserialize(&data, &mut idx);
assert_eq!(tree, rebuilt);   // requiere PartialEq derivado
```

La clave del deserialize: pasar `&mut usize` como índice para consumir
elementos secuencialmente.  Cada llamada recursiva avanza el índice.
</details>

### Ejercicio 4 — Copia modificada

Implementa `tree_copy_if` que copie solo los nodos cuyo dato cumple un
predicado.  Si un nodo no cumple, el subárbol completo se descarta.

<details>
<summary>Implementación</summary>

```c
TreeNode *tree_copy_if(TreeNode *root, int (*pred)(int)) {
    if (!root) return NULL;
    if (!pred(root->data)) return NULL;  // descartar subárbol

    TreeNode *copy = create_node(root->data);
    copy->left = tree_copy_if(root->left, pred);
    copy->right = tree_copy_if(root->right, pred);
    return copy;
}

int is_odd(int x) { return x % 2 != 0; }
TreeNode *odds = tree_copy_if(root, is_odd);
```

Nota: esto no preserva la estructura BST si se descartan nodos internos.
Es útil como filtro para propósitos específicos, no como poda de BST.
</details>

### Ejercicio 5 — Espejo con inorden

Explica por qué `tree_mirror` **no funciona** si se usa inorden
(intercambiar entre las dos recursiones).

<details>
<summary>Explicación</summary>

```c
// INCORRECTO:
void mirror_inorder(TreeNode *root) {
    if (!root) return;
    mirror_inorder(root->left);     // reflejar subárbol izquierdo

    TreeNode *temp = root->left;    // intercambiar
    root->left = root->right;
    root->right = temp;

    mirror_inorder(root->right);    // ¡root->right ahora es el ANTIGUO left!
}
```

Después del intercambio, `root->right` apunta al subárbol que **ya fue
reflejado** (el antiguo izquierdo).  Se refleja otra vez, deshaciendo
el trabajo.  El antiguo subárbol derecho (ahora en `root->left`) nunca
se refleja.

Resultado: solo se refleja el subárbol izquierdo, luego se deshace.
El árbol queda sin cambios (o parcialmente corrompido según la
estructura).
</details>

### Ejercicio 6 — Reconstruir desde postorden + inorden

Dados postorden `[3, 7, 5, 12, 20, 15, 10]` e inorden
`[3, 5, 7, 10, 12, 15, 20]`, reconstruye el árbol.

<details>
<summary>Método</summary>

1. En postorden, el **último** elemento es la raíz: `10`.
2. En inorden, `10` divide: izquierdo = `[3, 5, 7]`, derecho =
   `[12, 15, 20]`.
3. En postorden, los primeros 3 son izquierdo: `[3, 7, 5]`, los
   siguientes 3 son derecho: `[12, 20, 15]`.
4. Recursión izquierda: postorden `[3, 7, 5]`, inorden `[3, 5, 7]`.
   Raíz = 5.  Izq = [3], Der = [7].
5. Recursión derecha: postorden `[12, 20, 15]`, inorden `[12, 15, 20]`.
   Raíz = 15.  Izq = [12], Der = [20].

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

Es el mismo árbol original.  Postorden + inorden reconstruye
correctamente.
</details>

### Ejercicio 7 — tree_map genérico en C

Implementa `tree_map` en C que use un puntero a función `int (*f)(int)`
y retorne un nuevo árbol.

<details>
<summary>Uso</summary>

```c
int square(int x) { return x * x; }
TreeNode *squared = tree_map(root, square);
// Inorden de squared: 9 25 49 100 144 225 400
```

Estructura idéntica al original, pero cada dato es el cuadrado del
original.  Es un nuevo árbol — el original no se modifica.
</details>

### Ejercicio 8 — BFS para liberar

Implementa `tree_free_bfs` y explica por qué funciona — los punteros
`left` y `right` se leen antes de liberar.

<details>
<summary>Implementación</summary>

```c
void tree_free_bfs(TreeNode *root) {
    if (!root) return;

    TreeNode *queue[256];
    int front = 0, rear = 0;
    queue[rear++] = root;

    while (front < rear) {
        TreeNode *node = queue[front++];
        if (node->left)  queue[rear++] = node->left;
        if (node->right) queue[rear++] = node->right;
        free(node);   // punteros ya fueron leídos arriba
    }
}
```

Funciona porque: 1) `node->left` y `node->right` se leen y encolan
**antes** de `free(node)`.  2) Los nodos encolados siguen vivos hasta
que se desencolan y se liberan en su turno.

Desventaja vs postorden: la cola puede necesitar $O(w)$ espacio
(hasta $n/2$ para un árbol perfecto), mientras postorden solo usa
$O(h)$.
</details>

### Ejercicio 9 — fold en Rust

Usando `tree_fold`, implementa una función que determine si un árbol
es simétrico (espejo de sí mismo) sin comparar directamente.

<details>
<summary>Idea</summary>

`tree_fold` no es la herramienta ideal para esto — la simetría
requiere comparar subárboles opuestos, no reducir valores.  La función
correcta es:

```rust
fn is_symmetric(tree: &Tree<i32>) -> bool {
    fn mirror_eq(a: &Tree<i32>, b: &Tree<i32>) -> bool {
        match (a, b) {
            (None, None) => true,
            (Some(na), Some(nb)) => {
                na.data == nb.data
                    && mirror_eq(&na.left, &nb.right)    // cruzado
                    && mirror_eq(&na.right, &nb.left)
            }
            _ => false,
        }
    }

    match tree {
        None => true,
        Some(node) => mirror_eq(&node.left, &node.right),
    }
}
```

Esto es un recorrido simultáneo de dos subárboles, comparando
posiciones espejo (izquierda con derecha y viceversa).

Moraleja: no toda operación sobre árboles se reduce a un solo
recorrido estándar.  A veces se necesitan recorridos ad-hoc.
</details>

### Ejercicio 10 — Roundtrip con Valgrind

Escribe un programa en C que:
1. Construya el árbol de referencia.
2. Lo serialice.
3. Lo deserialice en un nuevo árbol.
4. Verifique igualdad.
5. Libere **ambos** árboles.
6. Compílalo y ejecútalo con Valgrind para verificar cero fugas.

<details>
<summary>Verificación</summary>

```bash
$ gcc -g roundtrip.c -o roundtrip
$ valgrind --leak-check=full ./roundtrip
```

Debe reportar:
```
==PID== HEAP SUMMARY:
==PID==   in use at exit: 0 bytes in 0 blocks
==PID== All heap blocks were freed -- no leaks are possible
```

Puntos clave:
- El original tiene 7 nodos → 7 mallocs.
- El deserializado tiene 7 nodos → 7 mallocs.
- Total: 14 mallocs, 14 frees.
- Si olvidas `tree_free(rebuilt)`, Valgrind reporta 7 bloques perdidos
  (168 bytes en sistema de 64 bits: 7 × 24).
</details>
