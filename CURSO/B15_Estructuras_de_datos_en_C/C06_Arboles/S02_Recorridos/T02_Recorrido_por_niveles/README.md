# Recorrido por niveles (BFS)

## La idea

Los recorridos recursivos (preorden, inorden, postorden) siguen una
estrategia de profundidad primero (**DFS**, Depth-First Search): bajan
hasta las hojas antes de moverse horizontalmente.  El recorrido por
niveles hace lo opuesto — visita todos los nodos del nivel 0, luego
todos los del nivel 1, luego nivel 2, etc.  Esto es una búsqueda en
anchura (**BFS**, Breadth-First Search).

| Estrategia | Estructura auxiliar | Orden de visita |
|------------|-------------------|-----------------|
| **DFS** (preorden/inorden/postorden) | Pila (implícita en call stack) | Profundidad primero |
| **BFS** (por niveles) | **Cola** | Anchura primero |

La clave: **BFS usa una cola**, no una pila.  La cola garantiza que los
nodos se procesen en el orden en que fueron descubiertos — nivel por
nivel, de izquierda a derecha.

---

## Árbol de referencia

Mismo árbol que en T01:

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

Niveles:
- Nivel 0: `10`
- Nivel 1: `5, 15`
- Nivel 2: `3, 7, 12, 20`

**Resultado BFS: `10 5 15 3 7 12 20`**

---

## Algoritmo

```
bfs(root):
    si root es NULL: retornar
    crear cola Q
    encolar root en Q

    mientras Q no esté vacía:
        node = desencolar de Q
        visitar(node)
        si node->left  ≠ NULL: encolar node->left
        si node->right ≠ NULL: encolar node->right
```

Es iterativo — no usa recursión.  La cola reemplaza al call stack.

---

## Traza detallada

```
Estado inicial:
  Cola: [10]

Paso 1: desencolar 10, visitar
  encolar left(5) y right(15)
  Cola: [5, 15]
  Visitados: 10

Paso 2: desencolar 5, visitar
  encolar left(3) y right(7)
  Cola: [15, 3, 7]
  Visitados: 10 5

Paso 3: desencolar 15, visitar
  encolar left(12) y right(20)
  Cola: [3, 7, 12, 20]
  Visitados: 10 5 15

Paso 4: desencolar 3, visitar
  no tiene hijos
  Cola: [7, 12, 20]
  Visitados: 10 5 15 3

Paso 5: desencolar 7, visitar
  no tiene hijos
  Cola: [12, 20]
  Visitados: 10 5 15 3 7

Paso 6: desencolar 12, visitar
  no tiene hijos
  Cola: [20]
  Visitados: 10 5 15 3 7 12

Paso 7: desencolar 20, visitar
  no tiene hijos
  Cola: []
  Visitados: 10 5 15 3 7 12 20

Cola vacía → fin.
```

### Visualización del recorrido

```
         10 ①
        /  \
       5 ②  15 ③
      / \   / \
     3 ④ 7⑤ 12⑥ 20⑦
```

Comparar con preorden (①②③④⑤⑥⑦ baja por la izquierda) — BFS avanza
horizontalmente.

---

## ¿Por qué funciona la cola?

La cola es FIFO (First In, First Out).  Cuando desencolamos un nodo del
nivel $k$ y encolamos sus hijos (nivel $k+1$), estos hijos quedan
**después** de todos los nodos restantes del nivel $k$.  Así, todos los
nodos del nivel $k$ se procesan antes de cualquier nodo del nivel $k+1$.

La pila (DFS) haría lo contrario: los hijos recién insertados se
procesarían inmediatamente, sumergiéndose en profundidad.

---

## Implementación en C

Usamos una cola simple con array circular, como la implementada en C04:

```c
#include <stdio.h>
#include <stdlib.h>

// --- Árbol ---
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

// --- Cola de punteros a TreeNode ---
#define QUEUE_CAP 128

typedef struct {
    TreeNode *data[QUEUE_CAP];
    int front;
    int rear;
    int count;
} NodeQueue;

void queue_init(NodeQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

int queue_empty(NodeQueue *q) {
    return q->count == 0;
}

void enqueue(NodeQueue *q, TreeNode *node) {
    q->rear = (q->rear + 1) % QUEUE_CAP;
    q->data[q->rear] = node;
    q->count++;
}

TreeNode *dequeue(NodeQueue *q) {
    TreeNode *node = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_CAP;
    q->count--;
    return node;
}

// --- BFS ---
void level_order(TreeNode *root) {
    if (!root) return;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);

    while (!queue_empty(&q)) {
        TreeNode *node = dequeue(&q);
        printf("%d ", node->data);

        if (node->left)  enqueue(&q, node->left);
        if (node->right) enqueue(&q, node->right);
    }
}
```

La cola almacena **punteros** a nodos, no valores.  El tamaño máximo de
la cola en cualquier momento es el ancho máximo del árbol.  Para un
árbol perfecto de altura $h$, el nivel más ancho tiene $2^h$ nodos.

### Cola dinámica

Si no queremos el límite fijo de `QUEUE_CAP`, podemos usar una cola
enlazada:

```c
typedef struct QueueNode {
    TreeNode *tree_node;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front;
    QueueNode *rear;
} DynQueue;

void dyn_enqueue(DynQueue *q, TreeNode *node) {
    QueueNode *qn = malloc(sizeof(QueueNode));
    qn->tree_node = node;
    qn->next = NULL;
    if (!q->rear) {
        q->front = q->rear = qn;
    } else {
        q->rear->next = qn;
        q->rear = qn;
    }
}

TreeNode *dyn_dequeue(DynQueue *q) {
    QueueNode *qn = q->front;
    TreeNode *node = qn->tree_node;
    q->front = qn->next;
    if (!q->front) q->rear = NULL;
    free(qn);
    return node;
}
```

---

## Implementación en Rust

`VecDeque<T>` de la biblioteca estándar es la cola ideal: `push_back`
para encolar, `pop_front` para desencolar, ambos amortizados $O(1)$:

```rust
use std::collections::VecDeque;

type Tree<T> = Option<Box<TreeNode<T>>>;

struct TreeNode<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}

fn level_order<T: std::fmt::Display>(root: &Tree<T>) {
    let Some(root_node) = root else { return };

    let mut queue: VecDeque<&TreeNode<T>> = VecDeque::new();
    queue.push_back(root_node);

    while let Some(node) = queue.pop_front() {
        print!("{} ", node.data);

        if let Some(left) = &node.left {
            queue.push_back(left);
        }
        if let Some(right) = &node.right {
            queue.push_back(right);
        }
    }
}
```

La cola almacena `&TreeNode<T>` — referencias inmutables.  No hay
transferencia de propiedad, solo préstamo durante el recorrido.

### Recolectar en Vec

```rust
fn level_order_vec<T: Clone>(root: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    let Some(root_node) = root else { return result };

    let mut queue = VecDeque::new();
    queue.push_back(root_node.as_ref());

    while let Some(node) = queue.pop_front() {
        result.push(node.data.clone());
        if let Some(left) = &node.left {
            queue.push_back(left);
        }
        if let Some(right) = &node.right {
            queue.push_back(right);
        }
    }

    result
}
```

---

## BFS con separación de niveles

El BFS básico produce una secuencia plana: `10 5 15 3 7 12 20`.  A veces
necesitamos saber dónde termina un nivel y comienza el siguiente.  Hay
dos técnicas.

### Técnica 1: contar nodos del nivel actual

```c
void level_order_by_level(TreeNode *root) {
    if (!root) return;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);

    int level = 0;
    while (!q.count == 0) {
        int level_size = q.count;  // nodos en este nivel
        printf("Level %d: ", level);

        for (int i = 0; i < level_size; i++) {
            TreeNode *node = dequeue(&q);
            printf("%d ", node->data);
            if (node->left)  enqueue(&q, node->left);
            if (node->right) enqueue(&q, node->right);
        }
        printf("\n");
        level++;
    }
}
```

```
Level 0: 10
Level 1: 5 15
Level 2: 3 7 12 20
```

El truco: al inicio de cada iteración del `while`, todos los nodos en la
cola pertenecen al mismo nivel.  Guardamos `level_size` y procesamos
exactamente esa cantidad antes de avanzar al siguiente nivel.

### En Rust

```rust
fn levels<T: Clone>(root: &Tree<T>) -> Vec<Vec<T>> {
    let mut result: Vec<Vec<T>> = Vec::new();
    let Some(root_node) = root else { return result };

    let mut queue = VecDeque::new();
    queue.push_back(root_node.as_ref());

    while !queue.is_empty() {
        let level_size = queue.len();
        let mut level = Vec::with_capacity(level_size);

        for _ in 0..level_size {
            let node = queue.pop_front().unwrap();
            level.push(node.data.clone());
            if let Some(left) = &node.left {
                queue.push_back(left);
            }
            if let Some(right) = &node.right {
                queue.push_back(right);
            }
        }

        result.push(level);
    }

    result
}
```

Retorna `Vec<Vec<T>>`: cada vector interno es un nivel.

```rust
let lvls = levels(&tree);
// [[10], [5, 15], [3, 7, 12, 20]]
```

### Técnica 2: centinela NULL

Otra forma: insertar un `NULL` después de cada nivel como marcador:

```c
void level_order_sentinel(TreeNode *root) {
    if (!root) return;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);
    enqueue(&q, NULL);   // centinela de fin de nivel

    int level = 0;
    printf("Level %d: ", level);

    while (!queue_empty(&q)) {
        TreeNode *node = dequeue(&q);

        if (node == NULL) {
            printf("\n");
            if (queue_empty(&q)) break;  // era el último nivel
            level++;
            printf("Level %d: ", level);
            enqueue(&q, NULL);  // centinela para el siguiente nivel
        } else {
            printf("%d ", node->data);
            if (node->left)  enqueue(&q, node->left);
            if (node->right) enqueue(&q, node->right);
        }
    }
    printf("\n");
}
```

La técnica del contador es más limpia y no requiere almacenar NULLs en
la cola.  Se recomienda la técnica 1.

---

## Traza del BFS por niveles

```
Estado inicial:
  Cola: [10]
  level_size = 1

--- Nivel 0 ---
  Desencolar 10, visitar.  Encolar 5, 15.
  Cola: [5, 15]
  Impreso: "Level 0: 10"

--- Nivel 1 ---
  level_size = 2
  Desencolar 5, visitar.  Encolar 3, 7.
  Desencolar 15, visitar.  Encolar 12, 20.
  Cola: [3, 7, 12, 20]
  Impreso: "Level 1: 5 15"

--- Nivel 2 ---
  level_size = 4
  Desencolar 3, visitar.   Sin hijos.
  Desencolar 7, visitar.   Sin hijos.
  Desencolar 12, visitar.  Sin hijos.
  Desencolar 20, visitar.  Sin hijos.
  Cola: []
  Impreso: "Level 2: 3 7 12 20"

Cola vacía → fin.
```

---

## Complejidad

| Aspecto | Valor |
|---------|-------|
| Tiempo | $O(n)$ — cada nodo se encola y desencola exactamente una vez |
| Espacio (cola) | $O(w)$ donde $w$ es el ancho máximo del árbol |
| Ancho máximo (perfecto) | $2^h = \lceil n/2 \rceil$ (último nivel) |
| Ancho máximo (degenerado) | 1 (cada nivel tiene un solo nodo) |

Para un árbol perfecto con $n = 2^{h+1} - 1$ nodos, la cola necesita
almacenar hasta $2^h \approx n/2$ punteros simultáneamente.  Para un
árbol degenerado, la cola nunca tiene más de 1 elemento.

Comparado con DFS: la pila de llamadas de DFS usa $O(h)$ espacio (la
altura).  BFS usa $O(w)$ (el ancho).  Para árboles balanceados,
$h = O(\log n)$ pero $w = O(n)$, así que **DFS es más eficiente en
espacio** para árboles anchos.  Para árboles degenerados, $h = O(n)$
pero $w = O(1)$, así que **BFS es más eficiente**.

| Forma del árbol | DFS (pila) | BFS (cola) | Gana |
|----------------|-----------|-----------|------|
| Perfecto, $n$ nodos | $O(\log n)$ | $O(n/2)$ | DFS |
| Completo, balanceado | $O(\log n)$ | $O(n/2)$ | DFS |
| Degenerado | $O(n)$ | $O(1)$ | BFS |

---

## BFS vs DFS: cuándo usar cada uno

| Criterio | BFS | DFS |
|----------|-----|-----|
| Encontrar el nodo más cercano a la raíz | Ideal — lo encuentra primero | Puede recorrer todo el subárbol antes |
| Camino más corto (en aristas) | Garantizado | No garantizado |
| Imprimir por niveles | Directo | Requiere profundidad como parámetro |
| Liberar memoria | Funciona pero requiere cola | Postorden recursivo (natural) |
| Serialización nivel a nivel | Directo (es el formato de array) | Indirecto |
| Espacio para árboles balanceados | $O(n)$ | $O(\log n)$ |
| Implementación | Iterativo con cola | Recursivo (simple) o iterativo con pila |

---

## Conexión con la representación de array

En T03 de S01 vimos que un árbol completo se almacena en un array
siguiendo el orden BFS.  Esto no es coincidencia:

```
Array:  [10, 5, 15, 3, 7, 12, 20]
Índice:   0  1   2  3  4   5   6

BFS:     10, 5, 15, 3, 7, 12, 20
```

Recorrer el array de izquierda a derecha **es** un recorrido BFS.  Las
fórmulas `left = 2i + 1`, `right = 2i + 2` codifican exactamente las
relaciones padre-hijo que BFS descubre dinámicamente.

---

## Variaciones útiles

### BFS inverso (bottom-up)

Recorrer los niveles de abajo hacia arriba:

```rust
fn level_order_bottom_up<T: Clone>(root: &Tree<T>) -> Vec<Vec<T>> {
    let mut result = levels(root);  // usa levels() de arriba
    result.reverse();
    result
}
// [[3, 7, 12, 20], [5, 15], [10]]
```

### Recorrido en zigzag

Nivel 0 izquierda→derecha, nivel 1 derecha→izquierda, alternando:

```rust
fn zigzag<T: Clone>(root: &Tree<T>) -> Vec<Vec<T>> {
    let mut result = levels(root);
    for (i, level) in result.iter_mut().enumerate() {
        if i % 2 == 1 {
            level.reverse();
        }
    }
    result
}
// [[10], [15, 5], [3, 7, 12, 20]]
```

### Vista derecha del árbol

El último nodo de cada nivel — lo que se ve mirando el árbol desde la
derecha:

```rust
fn right_view<T: Clone>(root: &Tree<T>) -> Vec<T> {
    levels(root)
        .into_iter()
        .filter_map(|level| level.into_iter().last())
        .collect()
}
// [10, 15, 20]
```

### Ancho máximo

```rust
fn max_width<T>(root: &Tree<T>) -> usize {
    levels(root)
        .iter()
        .map(|level| level.len())
        .max()
        .unwrap_or(0)
}
// 4 (nivel 2 tiene 4 nodos)
```

---

## Programa completo en C

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

// --- Cola circular ---
#define QUEUE_CAP 128

typedef struct {
    TreeNode *data[QUEUE_CAP];
    int front;
    int rear;
    int count;
} NodeQueue;

void queue_init(NodeQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

int queue_empty(NodeQueue *q) { return q->count == 0; }

void enqueue(NodeQueue *q, TreeNode *node) {
    q->rear = (q->rear + 1) % QUEUE_CAP;
    q->data[q->rear] = node;
    q->count++;
}

TreeNode *dequeue(NodeQueue *q) {
    TreeNode *node = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_CAP;
    q->count--;
    return node;
}

// --- BFS plano ---
void level_order(TreeNode *root) {
    if (!root) return;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);

    while (!queue_empty(&q)) {
        TreeNode *node = dequeue(&q);
        printf("%d ", node->data);
        if (node->left)  enqueue(&q, node->left);
        if (node->right) enqueue(&q, node->right);
    }
}

// --- BFS por niveles ---
void level_order_by_level(TreeNode *root) {
    if (!root) return;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);

    int level = 0;
    while (!queue_empty(&q)) {
        int level_size = q.count;
        printf("Level %d:", level);

        for (int i = 0; i < level_size; i++) {
            TreeNode *node = dequeue(&q);
            printf(" %d", node->data);
            if (node->left)  enqueue(&q, node->left);
            if (node->right) enqueue(&q, node->right);
        }
        printf("\n");
        level++;
    }
}

// --- Altura con BFS ---
int bfs_height(TreeNode *root) {
    if (!root) return -1;

    NodeQueue q;
    queue_init(&q);
    enqueue(&q, root);

    int height = -1;
    while (!queue_empty(&q)) {
        int level_size = q.count;
        height++;
        for (int i = 0; i < level_size; i++) {
            TreeNode *node = dequeue(&q);
            if (node->left)  enqueue(&q, node->left);
            if (node->right) enqueue(&q, node->right);
        }
    }
    return height;
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

    printf("BFS:  ");
    level_order(root);
    printf("\n\n");

    level_order_by_level(root);

    printf("\nHeight (BFS): %d\n", bfs_height(root));

    tree_free(root);
    return 0;
}
```

Salida:

```
BFS:  10 5 15 3 7 12 20

Level 0: 10
Level 1: 5 15
Level 2: 3 7 12 20

Height (BFS): 2
```

---

## Comparación: los 4 recorridos

| Recorrido | Tipo | Estructura auxiliar | Resultado |
|-----------|------|-------------------|-----------|
| Preorden (NLR) | DFS | Pila (call stack) | `10 5 3 7 15 12 20` |
| Inorden (LNR) | DFS | Pila (call stack) | `3 5 7 10 12 15 20` |
| Postorden (LRN) | DFS | Pila (call stack) | `3 7 5 12 20 15 10` |
| **Por niveles** | **BFS** | **Cola** | `10 5 15 3 7 12 20` |

Notar: BFS y preorden empiezan igual (`10`) porque ambos visitan la
raíz primero.  Pero divergen inmediatamente: preorden baja a la
izquierda (`5`), BFS avanza al hermano (`15` en el paso 3 vs paso 5 de
preorden).

---

## Errores comunes

| Error | Consecuencia |
|-------|-------------|
| Usar pila en vez de cola | Produce recorrido DFS, no BFS |
| Encolar `node` en vez de `node->left`/`node->right` | Encola el mismo nodo, bucle infinito |
| No verificar `NULL` antes de encolar | Encola NULL, crash al acceder a `node->data` |
| `level_size` dentro del for en vez de antes | El tamaño cambia durante el for, niveles mezclados |
| Olvidar el caso `root == NULL` | Encolar NULL, crash inmediato |

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

Escribe el resultado del recorrido BFS.

<details>
<summary>Predicción</summary>

```
BFS: A B C D E F
```

Nivel 0: A.  Nivel 1: B, C (hijos de A).  Nivel 2: D (hijo de B), E, F
(hijos de C).

Notar que D aparece antes que E y F porque B se desencoló antes que C,
y D (hijo de B) se encoló primero.
</details>

### Ejercicio 2 — Árbol degenerado

¿Cuál es el resultado BFS para este árbol?

```
  1
   \
    2
   /
  3
   \
    4
```

¿Cuántos nodos hay en la cola como máximo en cualquier momento?

<details>
<summary>Predicción</summary>

```
BFS: 1 2 3 4
```

Cada nivel tiene exactamente 1 nodo, así que la cola nunca tiene más de
**1 elemento**.  El BFS coincide con preorden aquí porque cada nodo
tiene a lo sumo un hijo.

Ancho máximo = 1.  Esto es lo opuesto al árbol perfecto, donde el
ancho máximo es $\lceil n/2 \rceil$.
</details>

### Ejercicio 3 — BFS por niveles en Rust

Implementa `levels()` que retorne `Vec<Vec<i32>>` para el árbol de
referencia.  Usa `VecDeque`.

<details>
<summary>Resultado esperado</summary>

```rust
let lvls = levels(&tree);
assert_eq!(lvls, vec![
    vec![10],
    vec![5, 15],
    vec![3, 7, 12, 20],
]);
```

La clave es guardar `queue.len()` al inicio de cada iteración del while
y procesar exactamente esa cantidad de nodos antes de pasar al
siguiente nivel.
</details>

### Ejercicio 4 — Tamaño máximo de la cola

Para un árbol **perfecto** de altura $h$:
1. ¿Cuántos nodos tiene el último nivel?
2. ¿Cuál es el tamaño máximo de la cola durante BFS?
3. Para $h = 19$ (aprox. $10^6$ nodos), ¿cuánta memoria consume la cola
   si almacena punteros de 8 bytes?

<details>
<summary>Cálculos</summary>

1. El último nivel tiene $2^h$ nodos.
2. El máximo se alcanza justo después de procesar todos los nodos del
   penúltimo nivel: la cola contiene todos los $2^h$ nodos del último
   nivel.  Tamaño máximo = $2^h$.
3. Para $h = 19$: $2^{19} = 524288$ punteros × 8 bytes = **4 MB**.

Comparar con DFS, que solo usa $h + 1 = 20$ frames de pila.
</details>

### Ejercicio 5 — Implementar con cola dinámica en C

Reimplementa `level_order` usando una cola enlazada en vez de un array
circular.  Libera cada `QueueNode` después de desencolar.

<details>
<summary>Esqueleto</summary>

```c
typedef struct QueueNode {
    TreeNode *tree_node;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front;
    QueueNode *rear;
} DynQueue;

// enqueue: crear QueueNode, enlazar al final
// dequeue: extraer del frente, liberar QueueNode, retornar TreeNode*
// level_order: mismo algoritmo, diferente cola
```

La ventaja es que no hay límite fijo.  La desventaja es una asignación
de memoria (malloc/free) por cada enqueue/dequeue.
</details>

### Ejercicio 6 — Altura sin recursión

Implementa `bfs_height(root)` que calcule la altura del árbol usando
BFS, sin recursión.

<details>
<summary>Idea</summary>

Usa BFS por niveles.  Inicializa `height = -1`.  Por cada nivel
completo procesado, incrementa `height`.  Al final, `height` es la
altura del árbol.

```c
int bfs_height(TreeNode *root) {
    if (!root) return -1;
    // ... BFS por niveles ...
    // height++ por cada iteración del while
    return height;
}
```

Para el árbol de referencia: 3 iteraciones (niveles 0, 1, 2), height
empieza en -1, termina en 2.  Coincide con la definición recursiva.
</details>

### Ejercicio 7 — Vista izquierda

Implementa `left_view(root)` que retorne el primer nodo de cada nivel
— lo que se ve mirando el árbol desde la izquierda.

<details>
<summary>Resultado esperado</summary>

Para el árbol de referencia:

```
Vista izquierda: [10, 5, 3]
```

Nivel 0: primer nodo = 10.  Nivel 1: primer nodo = 5.  Nivel 2: primer
nodo = 3.

Implementación: BFS por niveles, tomar solo `i == 0` de cada nivel.

```rust
fn left_view<T: Clone>(root: &Tree<T>) -> Vec<T> {
    levels(root)
        .into_iter()
        .filter_map(|level| level.into_iter().next())
        .collect()
}
```
</details>

### Ejercicio 8 — BFS en zigzag

Para el árbol de referencia, escribe el resultado del recorrido en
zigzag: nivel 0 izquierda→derecha, nivel 1 derecha→izquierda,
nivel 2 izquierda→derecha.

<details>
<summary>Predicción</summary>

```
Nivel 0 (→): 10
Nivel 1 (←): 15 5
Nivel 2 (→): 3 7 12 20

Resultado: [10, 15, 5, 3, 7, 12, 20]
```

El nivel 1 se invierte: en vez de `5 15`, es `15 5`.  Los niveles pares
van de izquierda a derecha, los impares de derecha a izquierda.
</details>

### Ejercicio 9 — BFS vs DFS para buscar

Tienes un árbol donde sabes que el nodo buscado está a profundidad 1
(es hijo directo de la raíz).  ¿Qué recorrido lo encuentra más rápido:
BFS o DFS preorden?  ¿Y si está en la última hoja (profundidad máxima)?

<details>
<summary>Análisis</summary>

**Profundidad 1 (cerca de la raíz):**
- BFS: lo encuentra en el paso 2 o 3 (visita raíz, luego nivel 1).
- DFS preorden: podría bajar por toda la rama izquierda antes de llegar
  al hijo derecho.  Peor caso: recorre todo el subárbol izquierdo.

BFS gana cuando el objetivo está cerca de la raíz.

**Última hoja (profundidad máxima):**
- Ambos visitan todos los nodos antes de encontrarlo.  $O(n)$ en ambos
  casos.
- Pero BFS necesita $O(w)$ espacio (ancho), DFS solo $O(h)$ (altura).
  Para un árbol balanceado, DFS gana en espacio.

En general: BFS es mejor para buscar "cerca de la raíz", DFS para
buscar "profundo" (especialmente si hay información para elegir rama).
</details>

### Ejercicio 10 — Recorrido BFS produce el array

Demuestra que el recorrido BFS de un árbol completo produce los
elementos en el mismo orden que la representación de array (S01/T03).
¿Por qué las fórmulas `left = 2i + 1`, `right = 2i + 2` son
equivalentes a "encolar hijos"?

<details>
<summary>Razonamiento</summary>

En la representación de array, la posición de cada nodo es su índice en
BFS.  La raíz está en la posición 0 (primer nodo visitado por BFS).
El primer hijo visitado tras la raíz es su hijo izquierdo (posición 1),
luego su hijo derecho (posición 2).  Luego el hijo izquierdo del nodo 1
(posición 3), etc.

Las fórmulas `left = 2i + 1`, `right = 2i + 2` codifican exactamente
el orden de encolamiento:

```
Desencolar índice i → encolar left (índice 2i+1), right (índice 2i+2)

i=0: encolar 1, 2       Cola: [1, 2]
i=1: encolar 3, 4       Cola: [2, 3, 4]
i=2: encolar 5, 6       Cola: [3, 4, 5, 6]
```

Los índices se asignan en el orden exacto de desencolamiento, que es el
orden BFS.  La representación de array es BFS congelado: la posición
de cada elemento es su número de visita en BFS.
</details>
