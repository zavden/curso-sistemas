# Rotaciones

## ¿Qué es una rotación?

Una rotación es una reestructuración local de un BST que cambia la
relación padre-hijo entre dos nodos sin alterar la propiedad BST.  Es
la operación fundamental que usan los árboles AVL y rojinegros para
mantener el balance.

Propiedades de una rotación:
- **Preserva el orden BST**: el recorrido inorden no cambia.
- **Es local**: solo afecta 2-3 nodos y sus enlaces.
- **Es $O(1)$**: solo reasigna punteros, no recorre el árbol.
- **Es reversible**: la rotación opuesta deshace la original.

---

## Rotación simple a la derecha (Right Rotation)

Se aplica cuando un nodo está desbalanceado hacia la **izquierda** — su
hijo izquierdo es "demasiado alto".

### Diagrama

```
Antes:                     Después:

        y                      x
       / \                    / \
      x   C      →          A   y
     / \                       / \
    A   B                     B   C
```

El nodo `x` (hijo izquierdo de `y`) **sube** para ser la nueva raíz
del subárbol.  El nodo `y` **baja** para ser hijo derecho de `x`.
El subárbol `B` (que era hijo derecho de `x`) pasa a ser hijo izquierdo
de `y`.

### ¿Por qué preserva el orden BST?

Inorden antes: `A x B y C`.
Inorden después: `A x B y C`.

Idéntico.  Los nodos cambian de posición vertical, pero el orden
horizontal (inorden) se mantiene.

### En C

```c
TreeNode *rotate_right(TreeNode *y) {
    TreeNode *x = y->left;
    TreeNode *B = x->right;

    x->right = y;
    y->left = B;

    return x;   // x es la nueva raíz del subárbol
}
```

3 reasignaciones de punteros.  $O(1)$.

### En Rust

```rust
fn rotate_right<T>(mut y: Box<Node<T>>) -> Box<Node<T>> {
    let mut x = y.left.take().expect("rotate_right requires left child");
    y.left = x.right.take();
    x.right = Some(y);
    x
}
```

`take()` extrae el nodo moviendo el ownership.  La función consume `y`
y retorna `x` como nueva raíz.

### Ejemplo concreto

```
Antes:                     Después:

        30                     20
       /                      / \
      20                    10   30
     /
    10
```

Inorden antes: `10 20 30`.  Inorden después: `10 20 30`.  Correcto.
La altura bajó de 2 a 1 — el árbol se balanceó.

### Ejemplo con subárbol B

```
Antes:                     Después:

        30                     20
       / \                    / \
      20   35                10   30
     / \                        / \
    10  25                    25   35
```

El subárbol `B = 25` era hijo derecho de 20.  Después de la rotación,
pasa a ser hijo izquierdo de 30.  Esto funciona porque
$20 < 25 < 30$ — 25 pertenece entre 20 y 30.

Inorden antes: `10 20 25 30 35`.
Inorden después: `10 20 25 30 35`.  Correcto.

---

## Rotación simple a la izquierda (Left Rotation)

Espejo de la rotación derecha.  Se aplica cuando un nodo está
desbalanceado hacia la **derecha**.

### Diagrama

```
Antes:                     Después:

    x                          y
   / \                        / \
  A   y          →           x   C
     / \                    / \
    B   C                  A   B
```

El nodo `y` (hijo derecho de `x`) sube.  `x` baja a la izquierda.
El subárbol `B` pasa de hijo izquierdo de `y` a hijo derecho de `x`.

### En C

```c
TreeNode *rotate_left(TreeNode *x) {
    TreeNode *y = x->right;
    TreeNode *B = y->left;

    y->left = x;
    x->right = B;

    return y;   // y es la nueva raíz del subárbol
}
```

### En Rust

```rust
fn rotate_left<T>(mut x: Box<Node<T>>) -> Box<Node<T>> {
    let mut y = x.right.take().expect("rotate_left requires right child");
    x.right = y.left.take();
    y.left = Some(x);
    y
}
```

### Ejemplo concreto

```
Antes:                     Después:

    10                         20
      \                       / \
       20                   10   30
        \
         30
```

Inorden: `10 20 30` → `10 20 30`.  Altura: 2 → 1.

---

## Simetría entre rotaciones

Las rotaciones izquierda y derecha son operaciones **inversas**:

```
rotate_left(rotate_right(tree)) = tree
rotate_right(rotate_left(tree)) = tree
```

Y son **espejos** estructurales:

| Rotación derecha | Rotación izquierda |
|-----------------|-------------------|
| `x = y->left` | `y = x->right` |
| `y->left = x->right` | `x->right = y->left` |
| `x->right = y` | `y->left = x` |
| `x` sube | `y` sube |
| Corrige desbalance izquierdo | Corrige desbalance derecho |

---

## Los cuatro casos de desbalance

Cuando un nodo se desbalancea, hay cuatro patrones posibles según dónde
está el exceso de altura.  Se nombran por la dirección del camino desde
el nodo desbalanceado hasta el nodo insertado:

| Caso | Patrón | Rotación necesaria |
|------|--------|-------------------|
| **LL** (Left-Left) | Exceso en izquierda-izquierda | Rotación derecha simple |
| **RR** (Right-Right) | Exceso en derecha-derecha | Rotación izquierda simple |
| **LR** (Left-Right) | Exceso en izquierda-derecha | Rotación doble izquierda-derecha |
| **RL** (Right-Left) | Exceso en derecha-izquierda | Rotación doble derecha-izquierda |

Las rotaciones simples corrigen LL y RR.  Los casos LR y RL requieren
**rotaciones dobles**.

---

## Caso LL: rotación derecha simple

El nodo desbalanceado tiene exceso de altura en su hijo izquierdo, y
ese hijo tiene exceso en su propio izquierdo.

```
Insertar 10, 20, 30... luego 5, luego 3:

        20               20                20
       / \              / \               / \
      10  30    →     10   30     →     10   30
                     /                 /
                    5                 5
                                    /
                                   3
                                   ↑ desbalance en 20 (izq-izq)

Caso LL en 20: rotate_right(20)

        10
       / \
      5   20
     /   / \
    3   ?   30
```

Espera — 20 no tenía nada entre 10 y 20 en este ejemplo.  Usemos un
ejemplo más completo:

```
Insertar: 30, 20, 40, 10, 25, 5

              30                        20
             / \       rotate_right    /  \
           20   40     ────────────→ 10    30
          / \                        /    / \
        10   25                     5   25   40
        /
       5

Nodo desbalanceado: 30 (h_izq=3, h_der=1, balance=-2)
Hijo izquierdo 20: su hijo más alto es el izquierdo (10) → caso LL.
```

Inorden antes: `5 10 20 25 30 40`.
Inorden después: `5 10 20 25 30 40`.  Correcto.

---

## Caso RR: rotación izquierda simple

Espejo del caso LL.

```
Insertar: 10, 5, 20, 30, 25, 35

        10                              20
       / \          rotate_left        /  \
      5   20        ────────────→    10    30
         / \                        / \   / \
       ?    30                     5  ?  25  35
           / \
          25  35

Pero el ? depende del ejemplo concreto. Simplifiquemos:

        10                         20
       /  \       rotate_left     / \
      5    20     ──────────→   10   30
            \                  /
             30               5
```

---

## Caso LR: rotación doble izquierda-derecha

El nodo desbalanceado tiene exceso en su hijo izquierdo, pero ese hijo
tiene el exceso en su propio **derecho**.  Una rotación simple no
funciona aquí:

```
Insertar: 30, 10, 20

    30              Rotate_right(30)?         10
   /                                            \
  10                       10                    30
   \                      / \                   /
    20                  ??   30                20

                     ¡No funciona!           ¡Sigue desbalanceado!
```

La rotación simple mueve 10 arriba, pero 20 (que está a la derecha de
10) no queda bien posicionado.

### Solución: dos rotaciones

1. **Primero**: rotación izquierda sobre el hijo izquierdo (10).
2. **Después**: rotación derecha sobre el nodo desbalanceado (30).

```
Paso 0 (inicial):        Paso 1: rotate_left(10)     Paso 2: rotate_right(30)

    30                        30                          20
   /                         /                           / \
  10                        20                         10   30
   \                       /
    20                    10
```

El resultado es un árbol balanceado con 20 como raíz.

### Ejemplo con subárboles

```
Paso 0:                   Paso 1: rotate_left(x)      Paso 2: rotate_right(z)

        z                         z                         y
       / \                       / \                       / \
      x   D                    y   D                     x     z
     / \           →          / \           →           / \   / \
    A   y                    x   C                     A   B C   D
       / \                  / \
      B   C                A   B
```

Inorden en los tres: `A x B y C z D`.  Preservado.

### En C

```c
TreeNode *rotate_left_right(TreeNode *z) {
    z->left = rotate_left(z->left);
    return rotate_right(z);
}
```

Dos líneas — combina las rotaciones simples.

### En Rust

```rust
fn rotate_left_right<T>(mut z: Box<Node<T>>) -> Box<Node<T>> {
    let x = z.left.take().unwrap();
    z.left = Some(rotate_left(x));
    rotate_right(z)
}
```

---

## Caso RL: rotación doble derecha-izquierda

Espejo del caso LR.  El nodo desbalanceado tiene exceso en su hijo
derecho, que a su vez tiene el exceso en su izquierdo.

```
Paso 0:                   Paso 1: rotate_right(y)     Paso 2: rotate_left(x)

    x                         x                           z
   / \                       / \                         / \
  A   y          →          A   z          →           x     y
     / \                       / \                    / \   / \
    z   D                     B   y                  A   B C   D
   / \                           / \
  B   C                         C   D
```

### Ejemplo concreto

```
Insertar: 10, 30, 20

  10                    10                       20
    \    rotate_right     \      rotate_left     / \
     30  sobre 30→         20    sobre 10→     10   30
    /                       \
   20                        30
```

### En C

```c
TreeNode *rotate_right_left(TreeNode *x) {
    x->right = rotate_right(x->right);
    return rotate_left(x);
}
```

---

## Resumen de los cuatro casos

```
Caso LL:                 Caso RR:
  z                         x
 /                           \
x        → rotate_right(z)    y      → rotate_left(x)
 \                            /
  ...                        ...

Caso LR:                 Caso RL:
  z                         x
 /                           \
x        → rotate_left(x)     y     → rotate_right(y)
 \         then right(z)      /        then left(x)
  y                          z
```

| Caso | Detectar | Rotación | Resultado |
|------|---------|----------|-----------|
| LL | balance(z) = -2, balance(z.left) ≤ 0 | `rotate_right(z)` | z baja, x sube |
| RR | balance(z) = +2, balance(z.right) ≥ 0 | `rotate_left(z)` | z baja, y sube |
| LR | balance(z) = -2, balance(z.left) > 0 | `rotate_left(x)` + `rotate_right(z)` | y sube al medio |
| RL | balance(z) = +2, balance(z.right) < 0 | `rotate_right(y)` + `rotate_left(z)` | z sube al medio |

El **factor de balance** de un nodo es $h_{derecho} - h_{izquierdo}$.
Un valor de -2 indica exceso izquierdo, +2 indica exceso derecho.  El
signo del balance del hijo determina si es caso simple (LL/RR) o doble
(LR/RL).

---

## Visualización completa: caso LR paso a paso

Insertar 30, 10, 25, 5, 20, 15 en un BST.  Detectar desbalance y
corregir con LR.

```
Después de insertar 30, 10, 25, 5, 20:

              30     balance(30) = -2 (izq h=3, der h=0)
             /       balance(10) = +1 (izq h=1, der h=2)
           10        → caso LR
          / \
         5   25
            /
           20
```

Paso 1: `rotate_left(10)` (sobre el hijo izquierdo de 30):

```
              30
             /
           25
          / \
        10   ?
       / \
      5   20
```

Espera, 25 tiene right?  Veamos con cuidado.  Nodo 10: left=5,
right=25.  Nodo 25: left=20, right=NULL.

`rotate_left(10)`:
- y = 10->right = 25
- B = 25->left = 20
- 25->left = 10
- 10->right = 20

```
              30
             /
           25
          /
        10
       / \
      5   20
```

Paso 2: `rotate_right(30)`:
- x = 30->left = 25
- B = 25->right = NULL
- 25->right = 30
- 30->left = NULL

```
           25
          / \
        10   30
       / \
      5   20
```

Inorden: `5 10 20 25 30`.  Correcto.
Altura: 2 (de 4 a 2).

---

## Efecto de las rotaciones en la altura

| Situación | Antes | Después | Reducción |
|-----------|-------|---------|-----------|
| 3 nodos en línea (LL/RR) | 2 | 1 | -1 |
| 3 nodos en zigzag (LR/RL) | 2 | 1 | -1 |
| Subárbol con desbalance 2 | h | h-1 o h | -1 o 0 |

Las rotaciones siempre reducen la altura del subárbol en al menos 0 y
a lo sumo 1.  Esto es suficiente para los árboles AVL: mantener
$|balance| \leq 1$ en cada nodo garantiza $h = O(\log n)$.

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

// --- Rotaciones ---
TreeNode *rotate_right(TreeNode *y) {
    TreeNode *x = y->left;
    y->left = x->right;
    x->right = y;
    return x;
}

TreeNode *rotate_left(TreeNode *x) {
    TreeNode *y = x->right;
    x->right = y->left;
    y->left = x;
    return y;
}

TreeNode *rotate_left_right(TreeNode *z) {
    z->left = rotate_left(z->left);
    return rotate_right(z);
}

TreeNode *rotate_right_left(TreeNode *x) {
    x->right = rotate_right(x->right);
    return rotate_left(x);
}

// --- Auxiliares ---
int tree_height(TreeNode *root) {
    if (!root) return -1;
    int lh = tree_height(root->left);
    int rh = tree_height(root->right);
    return 1 + (lh > rh ? lh : rh);
}

void inorder(TreeNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

void print_tree(TreeNode *root, int level) {
    if (!root) return;
    print_tree(root->right, level + 1);
    for (int i = 0; i < level; i++) printf("    ");
    printf("%d\n", root->data);
    print_tree(root->left, level + 1);
}

void tree_free(TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    // Caso LL: 30, 20, 10
    printf("=== Case LL ===\n");
    TreeNode *ll = create_node(30);
    ll->left = create_node(20);
    ll->left->left = create_node(10);
    printf("Before (h=%d):\n", tree_height(ll));
    print_tree(ll, 0);
    printf("Inorder: "); inorder(ll); printf("\n");

    ll = rotate_right(ll);
    printf("After rotate_right (h=%d):\n", tree_height(ll));
    print_tree(ll, 0);
    printf("Inorder: "); inorder(ll); printf("\n\n");

    // Caso RR: 10, 20, 30
    printf("=== Case RR ===\n");
    TreeNode *rr = create_node(10);
    rr->right = create_node(20);
    rr->right->right = create_node(30);
    printf("Before (h=%d):\n", tree_height(rr));
    print_tree(rr, 0);

    rr = rotate_left(rr);
    printf("After rotate_left (h=%d):\n", tree_height(rr));
    print_tree(rr, 0);
    printf("Inorder: "); inorder(rr); printf("\n\n");

    // Caso LR: 30, 10, 20
    printf("=== Case LR ===\n");
    TreeNode *lr = create_node(30);
    lr->left = create_node(10);
    lr->left->right = create_node(20);
    printf("Before (h=%d):\n", tree_height(lr));
    print_tree(lr, 0);

    lr = rotate_left_right(lr);
    printf("After rotate_left_right (h=%d):\n", tree_height(lr));
    print_tree(lr, 0);
    printf("Inorder: "); inorder(lr); printf("\n\n");

    // Caso RL: 10, 30, 20
    printf("=== Case RL ===\n");
    TreeNode *rl = create_node(10);
    rl->right = create_node(30);
    rl->right->left = create_node(20);
    printf("Before (h=%d):\n", tree_height(rl));
    print_tree(rl, 0);

    rl = rotate_right_left(rl);
    printf("After rotate_right_left (h=%d):\n", tree_height(rl));
    print_tree(rl, 0);
    printf("Inorder: "); inorder(rl); printf("\n");

    tree_free(ll);
    tree_free(rr);
    tree_free(lr);
    tree_free(rl);
    return 0;
}
```

Salida:

```
=== Case LL ===
Before (h=2):
    30
        20
            10
Inorder: 10 20 30
After rotate_right (h=1):
    30
20
    10
Inorder: 10 20 30

=== Case RR ===
Before (h=2):
            30
        20
    10
After rotate_left (h=1):
    30
20
    10
Inorder: 10 20 30

=== Case LR ===
Before (h=2):
    30
        20
    10
After rotate_left_right (h=1):
    30
20
    10
Inorder: 10 20 30

=== Case RL ===
Before (h=2):
        30
            20
    10
After rotate_right_left (h=1):
    30
20
    10
Inorder: 10 20 30
```

Los cuatro casos producen el mismo resultado balanceado `10-20-30` con
el mismo inorden.  La diferencia es solo qué caso de desbalance corrigen.

---

## Ejercicios

### Ejercicio 1 — Rotación derecha a mano

Aplica `rotate_right` sobre el nodo 50:

```
        50
       / \
      30   60
     / \
    20  40
```

Dibuja el resultado y verifica el inorden.

<details>
<summary>Resultado</summary>

```
      30
     / \
    20   50
        / \
       40  60
```

x = 30, y = 50, B = 40.
- 30->right = 50
- 50->left = 40

Inorden antes: `20 30 40 50 60`.
Inorden después: `20 30 40 50 60`.  Correcto.

El subárbol 40 (que era right de 30) pasó a ser left de 50.  Funciona
porque $30 < 40 < 50$.
</details>

### Ejercicio 2 — Rotación izquierda a mano

Aplica `rotate_left` sobre el nodo 10:

```
    10
   / \
  5   30
     / \
    20  40
```

<details>
<summary>Resultado</summary>

```
      30
     / \
    10  40
   / \
  5   20
```

x = 10, y = 30, B = 20.
- 30->left = 10
- 10->right = 20

Inorden: `5 10 20 30 40` → `5 10 20 30 40`.  Correcto.
</details>

### Ejercicio 3 — Identificar el caso

¿Qué caso de desbalance (LL, RR, LR, RL) tiene cada uno?

a)
```
      50
     /
    30
   /
  10
```

b)
```
  10
    \
     30
    /
   20
```

c)
```
  10
    \
     20
      \
       30
```

d)
```
      50
     /
    20
      \
       30
```

<details>
<summary>Respuestas</summary>

a) **LL** — izquierda-izquierda.  Solución: `rotate_right(50)`.

b) **RL** — derecha-izquierda.  Solución: `rotate_right(30)` + `rotate_left(10)`.

c) **RR** — derecha-derecha.  Solución: `rotate_left(10)`.

d) **LR** — izquierda-derecha.  Solución: `rotate_left(20)` + `rotate_right(50)`.
</details>

### Ejercicio 4 — Doble rotación paso a paso

Resuelve el caso LR del ejercicio 3d mostrando el árbol después de
cada rotación.

<details>
<summary>Pasos</summary>

Inicial (caso LR en 50):
```
      50
     /
    20
      \
       30
```

Paso 1: `rotate_left(20)`:
```
      50
     /
    30
   /
  20
```

Paso 2: `rotate_right(50)`:
```
    30
   / \
  20  50
```

Inorden: `20 30 50` → `20 30 50`.  Altura: 2 → 1.
</details>

### Ejercicio 5 — Rotación no cambia inorden

Demuestra que `rotate_left` preserva el inorden usando los subárboles
genéricos A, B, C.

<details>
<summary>Demostración</summary>

Antes:
```
    x
   / \
  A   y
     / \
    B   C
```

Inorden: recorrer A, visitar x, recorrer B, visitar y, recorrer C.
Resultado: `[A] x [B] y [C]`.

Después de `rotate_left(x)`:
```
      y
     / \
    x   C
   / \
  A   B
```

Inorden: recorrer A, visitar x, recorrer B, visitar y, recorrer C.
Resultado: `[A] x [B] y [C]`.

Los subárboles A, B, C no se modifican internamente — solo cambia dónde
están colgados.  El orden relativo `A < x < B < y < C` se preserva
porque B (que era entre x e y) sigue estando entre x e y.
</details>

### Ejercicio 6 — Rotación inversa

Demuestra que `rotate_right(rotate_left(tree))` produce el árbol
original.

<details>
<summary>Demostración</summary>

```
Original:        rotate_left:       rotate_right:
    x                 y                  x
   / \               / \                / \
  A   y      →      x   C      →      A   y
     / \            / \                   / \
    B   C          A   B                 B   C
```

`rotate_left` mueve y arriba y x abajo.  `rotate_right` mueve x arriba
y y abajo — deshace exactamente lo anterior.  Los subárboles A, B, C
vuelven a sus posiciones originales.

Formalmente: sean $L$ y $R$ las funciones de rotación izquierda y
derecha.  $R(L(T)) = T$ y $L(R(T)) = T$ para cualquier árbol $T$ donde
la rotación es válida (tiene el hijo necesario).
</details>

### Ejercicio 7 — Implementar en Rust

Implementa las 4 rotaciones en Rust usando `Box<Node<T>>`.

<details>
<summary>Implementación</summary>

```rust
fn rotate_right<T>(mut y: Box<Node<T>>) -> Box<Node<T>> {
    let mut x = y.left.take().unwrap();
    y.left = x.right.take();
    x.right = Some(y);
    x
}

fn rotate_left<T>(mut x: Box<Node<T>>) -> Box<Node<T>> {
    let mut y = x.right.take().unwrap();
    x.right = y.left.take();
    y.left = Some(x);
    y
}

fn rotate_left_right<T>(mut z: Box<Node<T>>) -> Box<Node<T>> {
    let x = z.left.take().unwrap();
    z.left = Some(rotate_left(x));
    rotate_right(z)
}

fn rotate_right_left<T>(mut z: Box<Node<T>>) -> Box<Node<T>> {
    let y = z.right.take().unwrap();
    z.right = Some(rotate_right(y));
    rotate_left(z)
}
```

`take()` es esencial — extrae el ownership del nodo hijo para pasarlo a
la función de rotación.  Sin `take()`, el borrow checker rechazaría
mover un campo de una referencia mutable.
</details>

### Ejercicio 8 — Cuántas rotaciones para balancear

¿Cuántas rotaciones (simples o dobles) se necesitan para balancear
este degenerado de 7 nodos?

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
           \
            6
             \
              7
```

<details>
<summary>Análisis</summary>

Aplicando rotaciones izquierdas sucesivas:

Rotación 1: `rotate_left(1)` → 2 sube.
Rotación 2: `rotate_left(2)` sobre la nueva raíz → etc.

Pero esto solo "gira" la cadena, no la balancea óptimamente.

Para un balanceo óptimo (algoritmo DSW simplificado):
1. Aplanar a vine (cadena derecha): ya está hecho (0 rotaciones).
2. Comprimir: $\lfloor \log_2 7 \rfloor = 2$ fases de rotaciones
   izquierdas, cada fase con ~n/2 rotaciones.

Total aproximado: **~6 rotaciones** para llegar a:
```
         4
        / \
       2    6
      / \  / \
     1  3 5   7
```

En la práctica, los árboles AVL nunca llegan a este estado — rebalancean
tras cada inserción, así que a lo sumo hacen 1-2 rotaciones por
operación.
</details>

### Ejercicio 9 — Rotación con más contexto

En este árbol, hay un desbalance en el nodo 50.  Identifica el caso y
aplica la rotación correcta:

```
            50
           / \
          25  60
         / \
        15  35
       /
      10
```

<details>
<summary>Solución</summary>

Balance de 50: $h_{izq} = 3$, $h_{der} = 1$ → balance = -2.
Balance de 25 (hijo izquierdo): $h_{izq} = 2$, $h_{der} = 1$ → balance = -1.

Balance del hijo izquierdo ≤ 0 → **caso LL**.

`rotate_right(50)`:

```
        25
       / \
      15   50
     /    / \
    10   35  60
```

Inorden antes: `10 15 25 35 50 60`.
Inorden después: `10 15 25 35 50 60`.  Correcto.

Nueva altura: 2 (de 3).  Balance de 25: 0.  Balanceado.
</details>

### Ejercicio 10 — ¿Por qué no una sola rotación para LR?

¿Qué pasa si intentas resolver un caso LR con una sola rotación
derecha?

```
    30
   /
  10
    \
     20
```

Aplica `rotate_right(30)` y muestra por qué no funciona.

<details>
<summary>Resultado</summary>

`rotate_right(30)`:
- x = 30->left = 10
- B = 10->right = 20
- 10->right = 30
- 30->left = 20

```
  10
    \
     30
    /
   20
```

¡Sigue desbalanceado!  La altura sigue siendo 2.  Solo se "volteó" el
zigzag de izquierda-derecha a derecha-izquierda.

El problema: la rotación simple mueve 10 arriba, pero 20 (que es el
nodo del medio en valor) queda atrapado abajo.  La rotación doble
primero alinea los nodos (`rotate_left(10)` convierte LR en LL) y
luego aplica la rotación simple que sí funciona.

Moraleja: los casos en zigzag (LR, RL) siempre requieren rotación
doble.  La rotación simple solo funciona para casos en línea (LL, RR).
</details>
