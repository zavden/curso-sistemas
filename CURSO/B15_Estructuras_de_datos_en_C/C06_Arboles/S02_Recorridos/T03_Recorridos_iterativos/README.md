# Recorridos iterativos

## Motivación

Los recorridos recursivos son elegantes, pero usan la pila de llamadas
del sistema.  Si el árbol es degenerado con $n$ nodos, la profundidad de
recursión es $n$ — suficiente para provocar un stack overflow con
$n > 10^4$ en muchos sistemas (el stack típico es 1–8 MB).

La solución: reemplazar la pila implícita del sistema por una **pila
explícita** en el heap, cuyo tamaño solo está limitado por la memoria
disponible.

| Aspecto | Recursivo | Iterativo (pila explícita) |
|---------|-----------|--------------------------|
| Estructura auxiliar | Pila del sistema (automática) | Pila manual en el heap |
| Límite de profundidad | ~10⁴ (stack del SO) | ~10⁸ (memoria RAM) |
| Legibilidad | Alta | Media-baja |
| Control sobre la pila | Ninguno | Total |

Un segundo motivo: algunos lenguajes o entornos no soportan recursión
(sistemas embebidos con stack limitado, ciertos intérpretes).  Los
recorridos iterativos funcionan en cualquier contexto.

---

## Árbol de referencia

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

Resultados esperados (los mismos que en T01):

| Recorrido | Resultado |
|-----------|-----------|
| Preorden | `10 5 3 7 15 12 20` |
| Inorden | `3 5 7 10 12 15 20` |
| Postorden | `3 7 5 12 20 15 10` |

---

## Preorden iterativo

El preorden es el más sencillo de convertir a iterativo porque el nodo
se visita **al momento de encontrarlo**, antes de descender.

### Algoritmo

```
preorder_iterative(root):
    si root es NULL: retornar
    crear pila S
    push(S, root)

    mientras S no esté vacía:
        node = pop(S)
        visitar(node)
        si node->right ≠ NULL: push(S, node->right)   // derecha primero
        si node->left  ≠ NULL: push(S, node->left)     // izquierda después
```

Se apila **derecha antes que izquierda** para que izquierda se
desapile primero (la pila es LIFO).

### Traza

```
Pila: [10]

pop 10, visitar.  push right(15), push left(5).
  Pila: [15, 5]       Visitados: 10

pop 5, visitar.  push right(7), push left(3).
  Pila: [15, 7, 3]    Visitados: 10 5

pop 3, visitar.  sin hijos.
  Pila: [15, 7]       Visitados: 10 5 3

pop 7, visitar.  sin hijos.
  Pila: [15]          Visitados: 10 5 3 7

pop 15, visitar.  push right(20), push left(12).
  Pila: [20, 12]      Visitados: 10 5 3 7 15

pop 12, visitar.  sin hijos.
  Pila: [20]          Visitados: 10 5 3 7 15 12

pop 20, visitar.  sin hijos.
  Pila: []            Visitados: 10 5 3 7 15 12 20

Pila vacía → fin.
```

### En C

```c
#define STACK_CAP 128

void preorder_iter(TreeNode *root) {
    if (!root) return;

    TreeNode *stack[STACK_CAP];
    int top = -1;

    stack[++top] = root;

    while (top >= 0) {
        TreeNode *node = stack[top--];
        printf("%d ", node->data);

        if (node->right) stack[++top] = node->right;
        if (node->left)  stack[++top] = node->left;
    }
}
```

### En Rust

```rust
fn preorder_iter<T: Clone>(root: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    let Some(root_node) = root else { return result };

    let mut stack: Vec<&TreeNode<T>> = vec![root_node];

    while let Some(node) = stack.pop() {
        result.push(node.data.clone());
        if let Some(right) = &node.right { stack.push(right); }
        if let Some(left) = &node.left   { stack.push(left); }
    }

    result
}
```

`Vec` como pila: `push()` para apilar, `pop()` para desapilar.  Crece
dinámicamente — sin límite fijo.

---

## Inorden iterativo

El inorden es más complejo porque el nodo no se visita al encontrarlo —
primero hay que descender por toda la rama izquierda.

### Algoritmo

```
inorder_iterative(root):
    crear pila S
    current = root

    mientras current ≠ NULL o S no esté vacía:
        // 1. Bajar todo lo posible por la izquierda
        mientras current ≠ NULL:
            push(S, current)
            current = current->left

        // 2. Desapilar, visitar, ir a la derecha
        current = pop(S)
        visitar(current)
        current = current->right
```

Dos fases alternadas:
1. **Bajar por la izquierda**: apilar cada nodo y avanzar a `left`.
2. **Visitar y girar a la derecha**: desapilar, visitar, y moverse al
   subárbol derecho (que repite el proceso).

### Traza

```
current=10, Pila: []

--- Fase 1: bajar por la izquierda ---
  push 10.  current=5.    Pila: [10]
  push 5.   current=3.    Pila: [10, 5]
  push 3.   current=NULL. Pila: [10, 5, 3]

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 3, visitar.  current=3->right=NULL.
  Pila: [10, 5]     Visitados: 3

--- Fase 1: current=NULL, no baja ---

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 5, visitar.  current=5->right=7.
  Pila: [10]        Visitados: 3 5

--- Fase 1: bajar por la izquierda desde 7 ---
  push 7.  current=NULL.  Pila: [10, 7]

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 7, visitar.  current=7->right=NULL.
  Pila: [10]        Visitados: 3 5 7

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 10, visitar.  current=10->right=15.
  Pila: []          Visitados: 3 5 7 10

--- Fase 1: bajar por la izquierda desde 15 ---
  push 15.  current=12.  Pila: [15]
  push 12.  current=NULL. Pila: [15, 12]

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 12, visitar.  current=12->right=NULL.
  Pila: [15]        Visitados: 3 5 7 10 12

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 15, visitar.  current=15->right=20.
  Pila: []          Visitados: 3 5 7 10 12 15

--- Fase 1: bajar por la izquierda desde 20 ---
  push 20.  current=NULL.  Pila: [20]

--- Fase 2: pop, visitar, ir a la derecha ---
  pop 20, visitar.  current=20->right=NULL.
  Pila: []          Visitados: 3 5 7 10 12 15 20

current=NULL y pila vacía → fin.
```

### En C

```c
void inorder_iter(TreeNode *root) {
    TreeNode *stack[STACK_CAP];
    int top = -1;
    TreeNode *current = root;

    while (current || top >= 0) {
        while (current) {
            stack[++top] = current;
            current = current->left;
        }

        current = stack[top--];
        printf("%d ", current->data);
        current = current->right;
    }
}
```

### En Rust

```rust
fn inorder_iter<T: Clone>(root: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    let mut stack: Vec<&TreeNode<T>> = Vec::new();
    let mut current = root;

    loop {
        // Bajar por la izquierda
        while let Some(node) = current {
            stack.push(node);
            current = &node.left;
        }

        // Desapilar o terminar
        let Some(node) = stack.pop() else { break };
        result.push(node.data.clone());
        current = &node.right;
    }

    result
}
```

El `current` es `&Tree<T>` (referencia a `Option<Box<TreeNode<T>>>`).
Cuando es `None`, el while interno no entra, y se desapila.

---

## Postorden iterativo

El postorden es el más difícil de los tres porque el nodo se visita
**después** de ambos subárboles.  Al desapilar un nodo, necesitamos
saber si ya visitamos su subárbol derecho o no.

### Estrategia 1: dos pilas

El truco: el postorden es el **reverso** del preorden modificado
(nodo → derecha → izquierda).

```
Preorden normal:     nodo → left → right   → 10 5 3 7 15 12 20
Preorden invertido:  nodo → right → left   → 10 15 20 12 5 7 3
Reverso del invertido:                      → 3 7 5 12 20 15 10  ← ¡postorden!
```

Algoritmo: hacer preorden pero apilando izquierda antes que derecha
(invirtiendo el orden), y en vez de visitar, apilar en una segunda pila.
Al final, desapilar la segunda pila.

```c
void postorder_two_stacks(TreeNode *root) {
    if (!root) return;

    TreeNode *s1[STACK_CAP], *s2[STACK_CAP];
    int top1 = -1, top2 = -1;

    s1[++top1] = root;

    while (top1 >= 0) {
        TreeNode *node = s1[top1--];
        s2[++top2] = node;                          // apilar en s2

        if (node->left)  s1[++top1] = node->left;   // izquierda primero
        if (node->right) s1[++top1] = node->right;   // derecha después
    }

    while (top2 >= 0) {
        printf("%d ", s2[top2--]->data);
    }
}
```

Simple pero usa $O(n)$ espacio extra (la segunda pila almacena todos los
nodos).

### Estrategia 2: una pila con nodo previo

Más eficiente en espacio — solo $O(h)$:

```c
void postorder_iter(TreeNode *root) {
    if (!root) return;

    TreeNode *stack[STACK_CAP];
    int top = -1;
    TreeNode *current = root;
    TreeNode *last_visited = NULL;

    while (current || top >= 0) {
        // Bajar por la izquierda
        while (current) {
            stack[++top] = current;
            current = current->left;
        }

        TreeNode *peek = stack[top];

        // ¿Tiene subárbol derecho sin visitar?
        if (peek->right && peek->right != last_visited) {
            current = peek->right;
        } else {
            printf("%d ", peek->data);
            last_visited = stack[top--];
        }
    }
}
```

La clave: `last_visited` rastrea el último nodo visitado.  Si el tope
de la pila tiene un hijo derecho que **no** es `last_visited`, debemos
descender a la derecha.  Si no tiene hijo derecho, o ya lo visitamos,
podemos visitar el nodo actual.

### Traza del postorden con una pila

```
current=10, last=NULL, Pila: []

Bajar: push 10, 5, 3.  current=NULL.
  Pila: [10, 5, 3]

peek=3.  right=NULL → visitar 3.  last=3.
  Pila: [10, 5]     Visitados: 3

peek=5.  right=7, 7≠last(3) → current=7.
  Pila: [10, 5]

Bajar: push 7.  current=NULL.
  Pila: [10, 5, 7]

peek=7.  right=NULL → visitar 7.  last=7.
  Pila: [10, 5]     Visitados: 3 7

peek=5.  right=7, 7==last(7) → visitar 5.  last=5.
  Pila: [10]        Visitados: 3 7 5

peek=10.  right=15, 15≠last(5) → current=15.
  Pila: [10]

Bajar: push 15, 12.  current=NULL.
  Pila: [10, 15, 12]

peek=12.  right=NULL → visitar 12.  last=12.
  Pila: [10, 15]    Visitados: 3 7 5 12

peek=15.  right=20, 20≠last(12) → current=20.
  Pila: [10, 15]

Bajar: push 20.  current=NULL.
  Pila: [10, 15, 20]

peek=20.  right=NULL → visitar 20.  last=20.
  Pila: [10, 15]    Visitados: 3 7 5 12 20

peek=15.  right=20, 20==last(20) → visitar 15.  last=15.
  Pila: [10]        Visitados: 3 7 5 12 20 15

peek=10.  right=15, 15==last(15) → visitar 10.  last=10.
  Pila: []          Visitados: 3 7 5 12 20 15 10

Pila vacía → fin.
```

### En Rust

```rust
fn postorder_iter<T: Clone>(root: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    let Some(root_node) = root else { return result };

    let mut stack: Vec<&TreeNode<T>> = Vec::new();
    let mut current: Option<&TreeNode<T>> = Some(root_node);
    let mut last_visited: Option<*const TreeNode<T>> = None;

    loop {
        while let Some(node) = current {
            stack.push(node);
            current = node.left.as_deref();
        }

        let Some(&peek) = stack.last() else { break };

        match &peek.right {
            Some(right) if last_visited != Some(right.as_ref() as *const _) => {
                current = Some(right);
            }
            _ => {
                result.push(peek.data.clone());
                last_visited = Some(stack.pop().unwrap() as *const _);
            }
        }
    }

    result
}
```

Se usa `*const TreeNode<T>` para comparar identidad de puntero (no
igualdad de valor).  Esto evita requerir `PartialEq` en `T`.

---

## Morris traversal (inorden sin pila)

El Morris traversal logra inorden en $O(n)$ tiempo y $O(1)$ espacio
**sin pila ni recursión**.  El truco: usar los punteros `right` de las
hojas (que son NULL) como hilos temporales para "volver" al ancestro.

### Idea

Para cada nodo, su **predecesor inorden** es el nodo más a la derecha
de su subárbol izquierdo.  El predecesor normalmente tiene `right = NULL`.
Morris lo modifica temporalmente para apuntar de vuelta al nodo actual,
creando un hilo de retorno.

```
Antes:                    Con hilo:
      10                       10
     /  \                     /  \
    5    15                  5    15
   / \                     / \
  3   7                   3   7
                               \
                                → 10  (hilo temporal)
```

7 es el predecesor inorden de 10 (el más a la derecha en el subárbol
izquierdo de 10).  Su `right` se modifica para apuntar a 10.  Cuando el
recorrido llega a 7 y sigue `right`, vuelve a 10 — sin pila.

### Algoritmo

```
morris_inorder(root):
    current = root

    mientras current ≠ NULL:
        si current->left == NULL:
            visitar(current)
            current = current->right        // avanzar (o seguir hilo)
        sino:
            // Encontrar predecesor inorden
            pred = current->left
            mientras pred->right ≠ NULL y pred->right ≠ current:
                pred = pred->right

            si pred->right == NULL:
                // Crear hilo
                pred->right = current
                current = current->left     // descender
            sino:
                // Hilo ya existe → ya volvimos, restaurar
                pred->right = NULL
                visitar(current)
                current = current->right    // avanzar
```

### Traza

```
current = 10

10 tiene left(5).  Predecesor: 5→right=7→right=NULL → pred=7.
  pred->right == NULL → crear hilo: 7->right = 10.
  current = 5.

5 tiene left(3).  Predecesor: 3→right=NULL → pred=3.
  pred->right == NULL → crear hilo: 3->right = 5.
  current = 3.

3 no tiene left.
  Visitar 3.  current = 3->right = 5 (hilo).
  Visitados: 3

5 tiene left(3).  Predecesor: 3→right=5==current → hilo existe.
  Restaurar: 3->right = NULL.
  Visitar 5.  current = 5->right = 7.
  Visitados: 3 5

7 no tiene left.
  Visitar 7.  current = 7->right = 10 (hilo).
  Visitados: 3 5 7

10 tiene left(5).  Predecesor: 5→right=7→right=10==current → hilo existe.
  Restaurar: 7->right = NULL.
  Visitar 10.  current = 10->right = 15.
  Visitados: 3 5 7 10

15 tiene left(12).  Predecesor: 12→right=NULL → pred=12.
  pred->right == NULL → crear hilo: 12->right = 15.
  current = 12.

12 no tiene left.
  Visitar 12.  current = 12->right = 15 (hilo).
  Visitados: 3 5 7 10 12

15 tiene left(12).  Predecesor: 12→right=15==current → hilo existe.
  Restaurar: 12->right = NULL.
  Visitar 15.  current = 15->right = 20.
  Visitados: 3 5 7 10 12 15

20 no tiene left.
  Visitar 20.  current = 20->right = NULL.
  Visitados: 3 5 7 10 12 15 20

current = NULL → fin.
```

El árbol queda intacto al final — todos los hilos se restauran.

### En C

```c
void morris_inorder(TreeNode *root) {
    TreeNode *current = root;

    while (current) {
        if (!current->left) {
            printf("%d ", current->data);
            current = current->right;
        } else {
            // Encontrar predecesor inorden
            TreeNode *pred = current->left;
            while (pred->right && pred->right != current) {
                pred = pred->right;
            }

            if (!pred->right) {
                // Crear hilo
                pred->right = current;
                current = current->left;
            } else {
                // Restaurar y visitar
                pred->right = NULL;
                printf("%d ", current->data);
                current = current->right;
            }
        }
    }
}
```

### En Rust (unsafe necesario)

Morris modifica punteros temporalmente — requiere mutabilidad compartida.
En Rust seguro esto viola las reglas de borrowing.  Se necesita `unsafe`:

```rust
fn morris_inorder(root: &Tree<i32>) -> Vec<i32> {
    let mut result = Vec::new();
    let Some(root_node) = root else { return result };
    let mut current = root_node.as_ref() as *const TreeNode<i32> as *mut TreeNode<i32>;

    unsafe {
        while !current.is_null() {
            if (*current).left.is_none() {
                result.push((*current).data);
                current = match &(*current).right {
                    Some(r) => r.as_ref() as *const _ as *mut _,
                    None => std::ptr::null_mut(),
                };
            } else {
                // Encontrar predecesor
                let left = (*current).left.as_ref().unwrap().as_ref()
                    as *const TreeNode<i32> as *mut TreeNode<i32>;
                let mut pred = left;
                while (*pred).right.is_some() {
                    let r = (*pred).right.as_ref().unwrap().as_ref()
                        as *const _ as *mut TreeNode<i32>;
                    if r == current { break; }
                    pred = r;
                }

                if (*pred).right.is_none() {
                    // Crear hilo (temporalmente violar la estructura de Box)
                    // NOTA: esto es conceptual — en la práctica se usaría
                    // raw pointers desde el inicio, no Box
                    result.push(0); // placeholder
                    current = left;
                } else {
                    result.push((*current).data);
                    current = match &(*current).right {
                        Some(r) => r.as_ref() as *const _ as *mut _,
                        None => std::ptr::null_mut(),
                    };
                }
            }
        }
    }

    result
}
```

En la práctica, Morris en Rust no es idiomático.  Preferir la versión
iterativa con pila (`Vec`), que es segura y casi igual de eficiente.
Morris es más relevante en C donde la manipulación de punteros es
natural.

---

## Comparación de técnicas

| Aspecto | Recursivo | Iterativo (pila) | Morris |
|---------|-----------|-----------------|--------|
| Tiempo | $O(n)$ | $O(n)$ | $O(n)$ |
| Espacio | $O(h)$ pila del sistema | $O(h)$ pila en heap | $O(1)$ |
| Modifica el árbol | No | No | Sí (temporalmente) |
| Legibilidad | Alta | Media | Baja |
| Stack overflow posible | Sí ($h > 10^4$) | No | No |
| Funciona con `const` | Sí | Sí | No (modifica punteros) |
| Preorden | Trivial | Fácil | Posible (variante) |
| Inorden | Trivial | Medio | Natural |
| Postorden | Trivial | Difícil | Complejo |

### Cuándo usar cada uno

| Situación | Recomendación |
|-----------|--------------|
| Árboles balanceados, $n < 10^5$ | Recursivo (simple, claro) |
| Árboles potencialmente degenerados | Iterativo (sin riesgo de stack overflow) |
| Memoria extremadamente limitada | Morris (solo para inorden) |
| Código de producción en Rust | Iterativo con `Vec` como pila |
| Entrevistas / competiciones | Recursivo primero, iterativo si se pide |

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

#define STACK_CAP 128

// --- Preorden iterativo ---
void preorder_iter(TreeNode *root) {
    if (!root) return;
    TreeNode *stack[STACK_CAP];
    int top = -1;
    stack[++top] = root;

    while (top >= 0) {
        TreeNode *node = stack[top--];
        printf("%d ", node->data);
        if (node->right) stack[++top] = node->right;
        if (node->left)  stack[++top] = node->left;
    }
}

// --- Inorden iterativo ---
void inorder_iter(TreeNode *root) {
    TreeNode *stack[STACK_CAP];
    int top = -1;
    TreeNode *current = root;

    while (current || top >= 0) {
        while (current) {
            stack[++top] = current;
            current = current->left;
        }
        current = stack[top--];
        printf("%d ", current->data);
        current = current->right;
    }
}

// --- Postorden iterativo (una pila) ---
void postorder_iter(TreeNode *root) {
    if (!root) return;
    TreeNode *stack[STACK_CAP];
    int top = -1;
    TreeNode *current = root;
    TreeNode *last_visited = NULL;

    while (current || top >= 0) {
        while (current) {
            stack[++top] = current;
            current = current->left;
        }

        TreeNode *peek = stack[top];
        if (peek->right && peek->right != last_visited) {
            current = peek->right;
        } else {
            printf("%d ", peek->data);
            last_visited = stack[top--];
        }
    }
}

// --- Morris inorden ---
void morris_inorder(TreeNode *root) {
    TreeNode *current = root;

    while (current) {
        if (!current->left) {
            printf("%d ", current->data);
            current = current->right;
        } else {
            TreeNode *pred = current->left;
            while (pred->right && pred->right != current)
                pred = pred->right;

            if (!pred->right) {
                pred->right = current;
                current = current->left;
            } else {
                pred->right = NULL;
                printf("%d ", current->data);
                current = current->right;
            }
        }
    }
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

    printf("Preorder (iter):  ");
    preorder_iter(root);
    printf("\n");

    printf("Inorder (iter):   ");
    inorder_iter(root);
    printf("\n");

    printf("Postorder (iter): ");
    postorder_iter(root);
    printf("\n");

    printf("Morris inorder:   ");
    morris_inorder(root);
    printf("\n");

    tree_free(root);
    return 0;
}
```

Salida:

```
Preorder (iter):  10 5 3 7 15 12 20
Inorder (iter):   3 5 7 10 12 15 20
Postorder (iter): 3 7 5 12 20 15 10
Morris inorder:   3 5 7 10 12 15 20
```

---

## Ejercicios

### Ejercicio 1 — Traza del preorden iterativo

Traza el preorden iterativo para este árbol, mostrando la pila en cada
paso:

```
      A
     / \
    B   C
       /
      D
```

<details>
<summary>Predicción</summary>

```
Pila: [A]

pop A, visitar.  push C, push B.
  Pila: [C, B]       Visitados: A

pop B, visitar.  sin hijos.
  Pila: [C]          Visitados: A B

pop C, visitar.  push left(D).  (no right)
  Pila: [D]          Visitados: A B C

pop D, visitar.  sin hijos.
  Pila: []           Visitados: A B C D
```

Resultado: `A B C D`.  Mismo que preorden recursivo.
</details>

### Ejercicio 2 — Inorden iterativo: estado de la pila al visitar 10

En el inorden iterativo del árbol de referencia, ¿cuál es el estado de
la pila justo **antes** de visitar el nodo 10?

<details>
<summary>Predicción</summary>

```
Justo antes de visitar 10:

  Pila: []  (vacía)
```

Cuando se visita 10, se acaba de desapilar de la pila.  Todo el
subárbol izquierdo {3, 5, 7} ya fue procesado y desapilado.  La pila
está vacía porque 10 es la raíz — no tiene ancestros esperando.

Después de visitar 10, `current` se mueve a `10->right = 15`, y el
proceso continúa con el subárbol derecho.
</details>

### Ejercicio 3 — Preorden iterativo en Rust

Implementa `preorder_iter` en Rust que retorne `Vec<T>`.  Usa `Vec`
como pila.

<details>
<summary>Esqueleto</summary>

```rust
fn preorder_iter<T: Clone>(root: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    let Some(root_node) = root else { return result };

    let mut stack = vec![root_node.as_ref()];

    while let Some(node) = stack.pop() {
        result.push(node.data.clone());
        // right primero (para que left salga primero)
        if let Some(right) = &node.right { stack.push(right); }
        if let Some(left) = &node.left { stack.push(left); }
    }

    result
}
```

Verificar: `preorder_iter(&tree)` debe dar `[10, 5, 3, 7, 15, 12, 20]`.
</details>

### Ejercicio 4 — ¿Por qué derecha antes que izquierda?

En el preorden iterativo, se apila el hijo **derecho antes** que el
izquierdo.  ¿Qué pasa si se invierte el orden (izquierda primero)?

<details>
<summary>Respuesta</summary>

Si apilas izquierda primero y derecha después, el hijo derecho queda en
el tope y se desapila primero.  El resultado sería preorden **espejo**:
nodo → derecha → izquierda.

```
Con orden invertido: 10 15 20 12 5 7 3
```

Es un recorrido NRL en vez de NLR.  La pila invierte el orden de
inserción, así que se debe insertar en orden inverso al deseado.
</details>

### Ejercicio 5 — Postorden con dos pilas: traza

Traza `postorder_two_stacks` para este árbol:

```
    1
   / \
  2   3
 /
4
```

Muestra s1 y s2 en cada paso.

<details>
<summary>Traza</summary>

```
s1: [1]  s2: []

pop 1 de s1, push a s2.  push left(2) y right(3) a s1.
  s1: [2, 3]  s2: [1]

pop 3 de s1, push a s2.  sin hijos.
  s1: [2]     s2: [1, 3]

pop 2 de s1, push a s2.  push left(4) a s1.
  s1: [4]     s2: [1, 3, 2]

pop 4 de s1, push a s2.  sin hijos.
  s1: []      s2: [1, 3, 2, 4]

s1 vacía.  Desapilar s2: 4 2 3 1
```

Resultado: `4 2 3 1`.  Correcto — es postorden (LRN).
</details>

### Ejercicio 6 — Pila máxima del inorden iterativo

Para un árbol degenerado de $n$ nodos (todos a la derecha), ¿cuál es
el tamaño máximo de la pila durante el inorden iterativo?

<details>
<summary>Análisis</summary>

```
  1
   \
    2
     \
      3
       \
        ...
```

En la primera iteración del while externo: `current = 1`, que no tiene
left, así que el while interno no apila nada.  Se visita 1.
`current = 1->right = 2`.  Lo mismo para 2, 3, etc.

Tamaño máximo de la pila: **1**.

Cada nodo se apila, se desapila inmediatamente (no tiene left), se
visita, y se avanza a la derecha.  La pila nunca acumula más de 1
elemento.

Para un degenerado a la izquierda, sería $O(n)$ — se apilan todos los
nodos antes de empezar a visitar.
</details>

### Ejercicio 7 — Morris: contar modificaciones

En el Morris traversal del árbol de referencia (7 nodos), ¿cuántas veces
se modifica un puntero `right`?  Cuenta tanto la creación como la
restauración de cada hilo.

<details>
<summary>Conteo</summary>

Hilos creados (punteros modificados):
1. `3->right = 5` (crear)
2. `7->right = 10` (crear)
3. `12->right = 15` (crear)

Hilos restaurados:
4. `3->right = NULL` (restaurar)
5. `7->right = NULL` (restaurar)
6. `12->right = NULL` (restaurar)

Total: **6 modificaciones** (3 creaciones + 3 restauraciones).

En general, el número de hilos es igual al número de nodos que tienen
subárbol izquierdo.  Cada hilo se crea y se destruye exactamente una
vez.  Modificaciones totales = $2 \times$ (nodos con hijo izquierdo).
</details>

### Ejercicio 8 — Equivalencia recursivo-iterativo

Demuestra informalmente que `inorder_iter` produce el mismo resultado
que `inorder` recursivo.

<details>
<summary>Argumento</summary>

El inorden recursivo hace:
1. Recorrer todo el subárbol izquierdo (recursión en left).
2. Visitar el nodo.
3. Recorrer todo el subárbol derecho (recursión en right).

El iterativo reproduce esto exactamente:
1. El while interno `while (current)` baja por la izquierda, apilando
   cada nodo — simula las llamadas recursivas `inorder(left)`.
2. Al desapilar, el nodo se visita — equivale al `print` entre las dos
   llamadas recursivas.
3. `current = current->right` inicia el recorrido del subárbol derecho
   — equivale a `inorder(right)`.

La pila explícita guarda los mismos nodos que estarían en el call stack
de la versión recursiva.  El while externo (`current || top >= 0`)
continúa mientras haya trabajo pendiente — ya sea nodos por descender
(`current`) o nodos por visitar (`top >= 0`).
</details>

### Ejercicio 9 — tree_free iterativo

Implementa `tree_free_iter(root)` que libere todos los nodos usando
postorden iterativo con una pila.  ¿Por qué no puedes usar preorden
iterativo para liberar?

<details>
<summary>Implementación</summary>

```c
void tree_free_iter(TreeNode *root) {
    if (!root) return;
    TreeNode *stack[STACK_CAP];
    int top = -1;
    TreeNode *current = root;
    TreeNode *last_visited = NULL;

    while (current || top >= 0) {
        while (current) {
            stack[++top] = current;
            current = current->left;
        }

        TreeNode *peek = stack[top];
        if (peek->right && peek->right != last_visited) {
            current = peek->right;
        } else {
            last_visited = stack[top--];
            free(last_visited);
        }
    }
}
```

No se puede usar preorden porque al hacer `free(node)`, sus punteros
`left` y `right` ya no son válidos.  Pero en preorden iterativo, el
nodo se apila como referencia para después apilar sus hijos.  Si ya
fue liberado, acceder a `node->right` y `node->left` es
use-after-free.

En postorden, el nodo se libera después de que ambos subárboles ya
fueron procesados — los punteros se leyeron antes del `free`.
</details>

### Ejercicio 10 — Cuándo Morris no funciona

Morris modifica temporalmente los punteros `right` del árbol.  Lista
tres situaciones donde esto es problemático.

<details>
<summary>Situaciones</summary>

1. **Concurrencia**: si otro hilo lee el árbol mientras Morris lo está
   recorriendo, verá un árbol con ciclos.  Puede entrar en bucle
   infinito o producir resultados incorrectos.

2. **Árboles `const`**: si el árbol es de solo lectura (ej.
   `const TreeNode *root` en C, `&Tree<T>` en Rust), Morris no puede
   modificar los punteros.  El compilador rechaza el código o se
   requiere `unsafe` para castear.

3. **Interrupciones o excepciones**: si el recorrido se interrumpe
   antes de restaurar todos los hilos (ej. un `return` prematuro, una
   excepción, o un `break`), el árbol queda con ciclos — corrupto
   permanentemente.  Las funciones recursivas e iterativas con pila
   no tienen este problema.
</details>
