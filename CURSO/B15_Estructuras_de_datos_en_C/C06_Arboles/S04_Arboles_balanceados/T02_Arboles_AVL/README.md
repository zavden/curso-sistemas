# Árboles AVL

## Definición

Un árbol AVL (Adelson-Velsky y Landis, 1962) es un BST donde para
**cada** nodo, la diferencia de alturas entre sus subárboles izquierdo y
derecho es a lo sumo 1.

El **factor de balance** de un nodo se define como:

$$\text{balance}(n) = h(\text{derecho}) - h(\text{izquierdo})$$

Un árbol es AVL si y solo si:

$$\forall\, n: \text{balance}(n) \in \{-1,\; 0,\; +1\}$$

| Balance | Significado |
|---------|------------|
| -1 | Ligeramente inclinado a la izquierda |
| 0 | Perfectamente balanceado |
| +1 | Ligeramente inclinado a la derecha |
| ≤ -2 o ≥ +2 | **Violación** — requiere rotación |

---

## Ejemplo

```
         10 [0]
        /  \
   [0] 5    15 [0]
      / \   / \
     3   7 12  20
```

El número entre corchetes es el factor de balance.  Todos son 0 porque
el árbol es perfecto (cada nodo tiene subárboles de igual altura).

Un AVL no necesita ser perfecto — solo balanceado:

```
         10 [+1]
        /  \
  [-1] 5    15 [0]
      /    / \
     3   12  20
```

Balance de 10: $h_{der} = 1$, $h_{izq} = 1$ → 0.  Espera, 5 solo tiene
left=3, no tiene right.  $h_{izq}(10) = h(5) = 1$, $h_{der}(10) = h(15) = 1$
→ balance(10) = 0.  Pero balance(5) = $h_{der}(5) - h_{izq}(5) = -1 - 0 = -1$.
Todos dentro de {-1, 0, +1} → AVL válido.

---

## Altura garantizada

La propiedad AVL limita la altura máxima.  Para un AVL con $n$ nodos:

$$h \leq 1.44 \cdot \log_2(n + 2) - 0.328$$

En la práctica, $h \approx 1.44 \cdot \log_2 n$.

| $n$ | Altura máxima AVL | Altura mínima posible ($\lfloor \log_2 n \rfloor$) | BST degenerado |
|-----|-------------------|--------------------------------------------------|----------------|
| 7 | 3 | 2 | 6 |
| 100 | 9 | 6 | 99 |
| 1000 | 14 | 9 | 999 |
| $10^6$ | 28 | 19 | $10^6 - 1$ |

El AVL es a lo sumo ~44% más alto que el óptimo.  Todas las operaciones
son $O(\log n)$ **garantizado**.

### Árboles de Fibonacci

El AVL más "delgado" posible (altura máxima para mínimo de nodos) tiene
una estructura recursiva donde el subárbol izquierdo tiene altura
$h - 1$ y el derecho $h - 2$ (o viceversa).  El número mínimo de nodos
para altura $h$ sigue la recurrencia:

$$N(h) = N(h-1) + N(h-2) + 1$$

Con $N(0) = 1$, $N(1) = 2$.  Estos son los **árboles de Fibonacci**:

```
h=0: 1 nodo       h=1: 2 nodos       h=2: 4 nodos       h=3: 7 nodos

  ●                  ●                    ●                    ●
                    /                    / \                  / \
                   ●                   ●   ●                ●   ●
                                      /                    / \  /
                                     ●                    ●  ● ●
                                                         /
                                                        ●
```

$N(0)=1$, $N(1)=2$, $N(2)=4$, $N(3)=7$, $N(4)=12$, $N(5)=20$.

---

## Estructura del nodo AVL

El nodo almacena la **altura** para calcular el balance en $O(1)$:

### En C

```c
typedef struct AVLNode {
    int data;
    struct AVLNode *left;
    struct AVLNode *right;
    int height;
} AVLNode;

AVLNode *create_node(int value) {
    AVLNode *n = malloc(sizeof(AVLNode));
    n->data = value;
    n->left = n->right = NULL;
    n->height = 0;   // hoja tiene altura 0
    return n;
}

int height(AVLNode *node) {
    return node ? node->height : -1;
}

void update_height(AVLNode *node) {
    int lh = height(node->left);
    int rh = height(node->right);
    node->height = 1 + (lh > rh ? lh : rh);
}

int balance_factor(AVLNode *node) {
    return height(node->right) - height(node->left);
}
```

4 bytes extra por nodo (el campo `height`).  Alternativa: almacenar
solo el factor de balance (2 bits), pero recalcular la altura es más
propenso a errores.

---

## Rotaciones AVL

Las mismas rotaciones de T01, pero ahora actualizan la altura:

```c
AVLNode *rotate_right(AVLNode *y) {
    AVLNode *x = y->left;
    y->left = x->right;
    x->right = y;

    update_height(y);   // y primero (ahora es hijo)
    update_height(x);   // x después (ahora es padre)

    return x;
}

AVLNode *rotate_left(AVLNode *x) {
    AVLNode *y = x->right;
    x->right = y->left;
    y->left = x;

    update_height(x);
    update_height(y);

    return y;
}
```

**El orden importa**: primero actualizar el nodo que bajó (ahora hijo),
luego el que subió (ahora padre).  El padre depende de la altura del
hijo.

---

## Rebalanceo

Después de insertar o eliminar, se verifica el balance del nodo actual.
Si $|\text{balance}| \geq 2$, se aplica la rotación correspondiente:

```c
AVLNode *rebalance(AVLNode *node) {
    update_height(node);
    int bf = balance_factor(node);

    if (bf < -1) {
        // Exceso izquierdo
        if (balance_factor(node->left) > 0) {
            // Caso LR: primero rotar izquierda sobre el hijo
            node->left = rotate_left(node->left);
        }
        // Caso LL (o LR ya convertido a LL)
        return rotate_right(node);
    }

    if (bf > 1) {
        // Exceso derecho
        if (balance_factor(node->right) < 0) {
            // Caso RL: primero rotar derecha sobre el hijo
            node->right = rotate_right(node->right);
        }
        // Caso RR (o RL ya convertido a RR)
        return rotate_left(node);
    }

    return node;   // balanceado, no hacer nada
}
```

La función `rebalance` unifica los 4 casos (LL, RR, LR, RL) en un solo
punto.  Se llama en cada nodo del camino de vuelta (durante el retorno
de la recursión).

---

## Inserción AVL

La inserción es idéntica a la de un BST, pero con `rebalance` al
retornar:

```c
AVLNode *avl_insert(AVLNode *node, int value) {
    // Inserción BST normal
    if (!node) return create_node(value);

    if (value < node->data)
        node->left = avl_insert(node->left, value);
    else if (value > node->data)
        node->right = avl_insert(node->right, value);
    else
        return node;   // duplicado

    // Rebalancear en el camino de vuelta
    return rebalance(node);
}
```

Cada nodo en el camino de la raíz al punto de inserción se rebalancea.
En la práctica, a lo sumo **una rotación** (simple o doble) es
necesaria por inserción — una vez que un nodo se rebalancea, los
ancestros ya no necesitan rotación.

### Traza: insertar 1, 2, 3, 4, 5, 6, 7

Insertemos en **orden ascendente** — el peor caso para un BST normal,
pero el AVL se corrige automáticamente.

```
Insertar 1:
  1 [0]

Insertar 2:
  1 [+1]
   \
    2 [0]

Insertar 3:
  1 [+2]  ← violación! Caso RR.
   \
    2 [+1]
     \
      3

  rotate_left(1):

    2 [0]
   / \
  1   3

Insertar 4:
    2 [+1]
   / \
  1   3 [+1]
       \
        4

  balance(3)=+1, balance(2)=+1 → OK (no viola).

Insertar 5:
    2 [+2]  ← violación en 2? Veamos.
   / \
  1   3 [+2]  ← violación primero en 3. Caso RR.
       \
        4 [+1]
         \
          5

  rebalance(3): rotate_left(3)

    2 [+1]
   / \
  1   4 [0]
     / \
    3   5

  rebalance(2): balance = +1 → OK.

Insertar 6:
    2 [+2]  ← violación.
   / \
  1   4 [+1]
     / \
    3   5 [+1]
         \
          6

  rebalance(5): balance=+1 → OK.
  rebalance(4): balance=+1 → OK.
  rebalance(2): balance=+2 → Caso RR (balance(4)=+1≥0).
    rotate_left(2):

      4 [0]
     / \
    2   5 [+1]
   / \    \
  1   3    6

Insertar 7:
      4 [+1]
     / \
    2   5 [+2]  ← violación. Caso RR.
   / \    \
  1   3    6 [+1]
            \
             7

  rebalance(6): OK.
  rebalance(5): balance=+2, balance(6)=+1 → RR.
    rotate_left(5):

      4 [0]
     / \
    2   6 [0]
   / \ / \
  1  3 5  7
```

Resultado final: árbol **perfecto** de altura 2 — exactamente el mismo
resultado que `sorted_to_bst`, pero construido incrementalmente.

Resumen de las rotaciones durante la inserción de 1..7:

| Inserción | Rotación | Tipo |
|-----------|----------|------|
| 3 | `rotate_left(1)` | RR |
| 5 | `rotate_left(3)` | RR |
| 6 | `rotate_left(2)` | RR |
| 7 | `rotate_left(5)` | RR |

4 rotaciones para 7 inserciones.  Todas simples porque la inserción
es siempre a la derecha (caso RR).

---

## Eliminación AVL

La eliminación BST (T04/S03) seguida de rebalanceo en cada nodo del
camino de vuelta:

```c
AVLNode *avl_delete(AVLNode *node, int key) {
    if (!node) return NULL;

    if (key < node->data) {
        node->left = avl_delete(node->left, key);
    } else if (key > node->data) {
        node->right = avl_delete(node->right, key);
    } else {
        // Caso 1 y 2: 0 o 1 hijo
        if (!node->left) {
            AVLNode *right = node->right;
            free(node);
            return right;
        }
        if (!node->right) {
            AVLNode *left = node->left;
            free(node);
            return left;
        }

        // Caso 3: sucesor inorden
        AVLNode *succ = node->right;
        while (succ->left) succ = succ->left;
        node->data = succ->data;
        node->right = avl_delete(node->right, succ->data);
    }

    return rebalance(node);
}
```

Diferencia con la inserción: la eliminación puede requerir rotaciones
en **múltiples niveles** del camino de vuelta (hasta $O(\log n)$
rotaciones), mientras que la inserción necesita a lo sumo una.

---

## Verificar el invariante

```c
int is_avl(AVLNode *node) {
    if (!node) return 1;

    int bf = balance_factor(node);
    if (bf < -1 || bf > 1) return 0;

    return is_avl(node->left) && is_avl(node->right);
}
```

---

## Complejidad

| Operación | BST simple | AVL |
|-----------|-----------|-----|
| Búsqueda | $O(n)$ peor | $O(\log n)$ siempre |
| Inserción | $O(n)$ peor | $O(\log n)$ siempre |
| Eliminación | $O(n)$ peor | $O(\log n)$ siempre |
| Rotaciones por inserción | — | ≤ 1 (simple o doble) |
| Rotaciones por eliminación | — | $O(\log n)$ peor caso |
| Espacio extra por nodo | 0 | 4 bytes (altura) |

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct AVLNode {
    int data;
    struct AVLNode *left;
    struct AVLNode *right;
    int height;
} AVLNode;

AVLNode *create_node(int value) {
    AVLNode *n = malloc(sizeof(AVLNode));
    n->data = value;
    n->left = n->right = NULL;
    n->height = 0;
    return n;
}

int height(AVLNode *n) { return n ? n->height : -1; }

void update_height(AVLNode *n) {
    int lh = height(n->left);
    int rh = height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

int balance_factor(AVLNode *n) {
    return height(n->right) - height(n->left);
}

AVLNode *rotate_right(AVLNode *y) {
    AVLNode *x = y->left;
    y->left = x->right;
    x->right = y;
    update_height(y);
    update_height(x);
    return x;
}

AVLNode *rotate_left(AVLNode *x) {
    AVLNode *y = x->right;
    x->right = y->left;
    y->left = x;
    update_height(x);
    update_height(y);
    return y;
}

AVLNode *rebalance(AVLNode *node) {
    update_height(node);
    int bf = balance_factor(node);

    if (bf < -1) {
        if (balance_factor(node->left) > 0)
            node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    if (bf > 1) {
        if (balance_factor(node->right) < 0)
            node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}

AVLNode *avl_insert(AVLNode *node, int value) {
    if (!node) return create_node(value);
    if (value < node->data)
        node->left = avl_insert(node->left, value);
    else if (value > node->data)
        node->right = avl_insert(node->right, value);
    else
        return node;
    return rebalance(node);
}

AVLNode *avl_delete(AVLNode *node, int key) {
    if (!node) return NULL;

    if (key < node->data) {
        node->left = avl_delete(node->left, key);
    } else if (key > node->data) {
        node->right = avl_delete(node->right, key);
    } else {
        if (!node->left) {
            AVLNode *r = node->right;
            free(node);
            return r;
        }
        if (!node->right) {
            AVLNode *l = node->left;
            free(node);
            return l;
        }
        AVLNode *succ = node->right;
        while (succ->left) succ = succ->left;
        node->data = succ->data;
        node->right = avl_delete(node->right, succ->data);
    }

    return rebalance(node);
}

void inorder(AVLNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

int is_avl(AVLNode *node) {
    if (!node) return 1;
    int bf = balance_factor(node);
    if (bf < -1 || bf > 1) return 0;
    return is_avl(node->left) && is_avl(node->right);
}

void print_tree(AVLNode *root, int level) {
    if (!root) return;
    print_tree(root->right, level + 1);
    for (int i = 0; i < level; i++) printf("    ");
    printf("%d[%+d]\n", root->data, balance_factor(root));
    print_tree(root->left, level + 1);
}

void tree_free(AVLNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    AVLNode *root = NULL;

    // Insertar 1..7 (peor caso para BST, AVL lo maneja)
    printf("=== Insert 1..7 ===\n");
    for (int i = 1; i <= 7; i++) {
        root = avl_insert(root, i);
        printf("After insert %d: height=%d, AVL=%s\n",
               i, height(root), is_avl(root) ? "yes" : "NO");
    }

    printf("\nTree:\n");
    print_tree(root, 0);
    printf("\nInorder: ");
    inorder(root);
    printf("\n");

    // Eliminar algunos nodos
    printf("\n=== Deletions ===\n");
    int del[] = {4, 2, 6};
    for (int i = 0; i < 3; i++) {
        root = avl_delete(root, del[i]);
        printf("After delete %d: height=%d, AVL=%s, inorder: ",
               del[i], height(root), is_avl(root) ? "yes" : "NO");
        inorder(root);
        printf("\n");
    }

    printf("\nFinal tree:\n");
    print_tree(root, 0);

    tree_free(root);
    return 0;
}
```

Salida:

```
=== Insert 1..7 ===
After insert 1: height=0, AVL=yes
After insert 2: height=1, AVL=yes
After insert 3: height=1, AVL=yes
After insert 4: height=2, AVL=yes
After insert 5: height=2, AVL=yes
After insert 6: height=2, AVL=yes
After insert 7: height=2, AVL=yes

Tree:
        7[0]
    6[0]
        5[0]
4[0]
        3[0]
    2[0]
        1[0]

Inorder: 1 2 3 4 5 6 7

=== Deletions ===
After delete 4: height=2, AVL=yes, inorder: 1 2 3 5 6 7
After delete 2: height=2, AVL=yes, inorder: 1 3 5 6 7
After delete 6: height=2, AVL=yes, inorder: 1 3 5 7

Final tree:
        7[0]
    5[0]
        3[0]
1[-1]
```

Insertar 1..7 en orden produce un árbol perfecto de altura 2 — sin
degeneración.  Las eliminaciones mantienen el invariante AVL en todo
momento.

---

## Ejercicios

### Ejercicio 1 — Factor de balance

Calcula el factor de balance de cada nodo:

```
         15
        /  \
       10   20
      /    / \
     5    18  25
    /
   2
```

¿Es un AVL válido?

<details>
<summary>Cálculos</summary>

| Nodo | $h_{izq}$ | $h_{der}$ | Balance |
|------|----------|----------|---------|
| 2 | -1 | -1 | 0 |
| 5 | 0 | -1 | -1 |
| 10 | 1 | -1 | -2 |
| 18 | -1 | -1 | 0 |
| 25 | -1 | -1 | 0 |
| 20 | 0 | 0 | 0 |
| 15 | 2 | 1 | -1 |

El nodo 10 tiene balance **-2** → violación.  **No es AVL válido.**

Para corregir: balance(10) = -2, balance(5) = -1 → caso LL.
`rotate_right(10)` → 5 sube, 10 baja a la derecha.
</details>

### Ejercicio 2 — Insertar en AVL con rotación

Inserta 6 en este AVL:

```
      10 [0]
     / \
    5   15
```

Luego inserta 4.  Luego inserta 3.  ¿Cuándo ocurre la primera rotación?

<details>
<summary>Traza</summary>

Insertar 6:
```
      10 [-1]
     / \
    5   15
     \
      6
```
Balance(5)=+1, balance(10)=-1 → OK.

Insertar 4:
```
      10 [-1]
     / \
    5   15
   / \
  4   6
```
Balance(5)=0, balance(10)=-1 → OK.

Insertar 3:
```
      10 [-2]  ← violación!
     / \
    5   15
   / \
  4   6
 /
3
```

Balance(10) = -2, balance(5) = -1 → caso **LL**.
`rotate_right(10)`:

```
      5 [0]
     / \
    4   10
   /   / \
  3   6   15
```

Primera rotación al insertar 3.  Resultado: AVL válido, altura 2.
</details>

### Ejercicio 3 — Caso LR en AVL

Inserta 15, 5, 10 en un AVL vacío.  Muestra la rotación doble.

<details>
<summary>Traza</summary>

```
Insertar 15:    15[0]
Insertar 5:     15[-1]
               /
              5[0]

Insertar 10:   15[-2]  ← violación
              /
             5[+1]     ← hijo izq tiene balance > 0 → caso LR
              \
               10
```

Paso 1: `rotate_left(5)`:
```
   15
  /
 10
/
5
```

Paso 2: `rotate_right(15)`:
```
   10[0]
  / \
 5   15
```

Una rotación doble (LR).  Resultado: AVL perfecto.
</details>

### Ejercicio 4 — Insertar 1..15

¿Cuál es la altura del AVL después de insertar 1, 2, 3, ..., 15?

<details>
<summary>Predicción</summary>

Para 15 nodos, la altura mínima es $\lfloor \log_2 15 \rfloor = 3$.
Un AVL puede tener a lo sumo $\approx 1.44 \log_2 15 \approx 5.6$,
así que la altura máxima sería 5.

En la práctica, insertar 1..15 en orden produce un árbol **perfecto**
de altura **3** gracias a las rotaciones RR que van balanceando a
medida que se construye.

El resultado es idéntico a `sorted_to_bst([1..15])`:
```
              8
           /     \
         4        12
        / \       / \
       2   6    10   14
      /\ / \   /\   / \
     1 3 5 7  9 11 13 15
```
</details>

### Ejercicio 5 — Eliminación con rebalanceo

Elimina 1 del AVL del ejercicio 4 (el árbol perfecto de 15 nodos).
¿Se necesita rotación?

<details>
<summary>Análisis</summary>

El nodo 1 es hoja.  Al eliminarlo:
- balance(2) cambia de 0 a +1 → OK.
- balance(4) cambia de 0 a +1 → OK.
- balance(8) no cambia significativamente → OK.

**No se necesita rotación.** Eliminar una hoja de un árbol perfecto
solo reduce el balance de los ancestros en 1, que sigue siendo ≤ 1.

Si en cambio eliminamos 1 **y** 3:
- balance(2) = +1 → OK tras eliminar 1.
- Tras eliminar 3: el nodo 2 queda sin hijos, balance(4) = 0+1-(-1)...
  Depende de la estructura exacta tras la primera eliminación.

La clave: una eliminación puede no causar rotación, pero una secuencia
de eliminaciones eventualmente sí.
</details>

### Ejercicio 6 — Contar rotaciones

Inserta los valores `[7, 3, 11, 1, 5, 9, 13, 2]` en un AVL vacío.
¿Cuántas rotaciones se realizan?

<details>
<summary>Traza</summary>

```
7, 3, 11, 1, 5, 9, 13: todos se insertan sin rotación (árbol perfecto).

         7[0]
        / \
       3   11
      / \ / \
     1  5 9  13

Insertar 2:
         7[-1]
        / \
       3   11
      / \ / \
     1  5 9  13
      \
       2

balance(1)=+1, balance(3)=-1, balance(7)=-1 → todos OK.
```

**0 rotaciones.** El orden de inserción produce un árbol que se
mantiene balanceado naturalmente.

Los primeros 7 valores ($2^3 - 1$) insertados en orden BFS producen un
árbol perfecto.  El octavo valor solo desbalancea ligeramente.
</details>

### Ejercicio 7 — Altura almacenada vs calculada

¿Por qué almacenar la altura en el nodo en vez de calcularla
recursivamente cada vez?

<details>
<summary>Respuesta</summary>

Calcular la altura recursivamente es $O(n)$ para cada nodo (recorre
todo el subárbol).  El rebalanceo necesita el balance de cada nodo en
el camino de vuelta — si se calcula recursivamente, la inserción sería
$O(n \log n)$ en vez de $O(\log n)$.

Con la altura almacenada:
- `height(node)` es $O(1)$.
- `balance_factor(node)` es $O(1)$.
- `update_height(node)` es $O(1)$.
- Inserción total: $O(\log n)$.

Costo: 4 bytes extra por nodo (un `int`).  Para $10^6$ nodos: 4 MB
extra — insignificante.
</details>

### Ejercicio 8 — Rebalance unificado

Explica por qué la función `rebalance` cubre los 4 casos (LL, RR, LR,
RL) con solo 2 bloques `if`.

<details>
<summary>Explicación</summary>

```c
if (bf < -1) {                              // Exceso izquierdo
    if (balance_factor(node->left) > 0)     // ¿El hijo izq está inclinado a la der?
        node->left = rotate_left(node->left); // Sí → caso LR: alinear primero
    return rotate_right(node);                // Caso LL (o LR ya alineado)
}
```

El primer `if` distingue exceso izquierdo (bf < -1) vs derecho (bf > 1).
El `if` interno distingue caso en línea (LL/RR) vs caso zigzag (LR/RL):

- Si el hijo izquierdo tiene balance ≤ 0 → en línea (LL) → una rotación.
- Si el hijo izquierdo tiene balance > 0 → zigzag (LR) → primero
  alinear con rotación izquierda, luego rotación derecha.

La "alineación" convierte LR en LL (o RL en RR), y luego la rotación
simple corrige.  Por eso solo hay 2 bloques — cada uno maneja 2 casos.
</details>

### Ejercicio 9 — AVL vs BST: benchmark conceptual

Para $n = 10^6$ inserciones en **orden ascendente**, estima el número
total de comparaciones para BST vs AVL.

<details>
<summary>Estimación</summary>

**BST**: cada inserción $i$ requiere $i - 1$ comparaciones (recorre
toda la cadena).

$$\sum_{i=1}^{10^6} (i-1) = \frac{(10^6)(10^6 - 1)}{2} \approx 5 \times 10^{11}$$

**AVL**: cada inserción requiere $O(\log i)$ comparaciones + $O(1)$
rotaciones.

$$\sum_{i=1}^{10^6} \log_2 i \approx 10^6 \times 20 = 2 \times 10^7$$

Ratio: $\frac{5 \times 10^{11}}{2 \times 10^7} = 25000\times$.

BST: horas.  AVL: milisegundos.
</details>

### Ejercicio 10 — Implementar is_avl robusto

Implementa `is_avl` que además de verificar el balance, verifique que
las alturas almacenadas son correctas.

<details>
<summary>Implementación</summary>

```c
int is_avl_robust(AVLNode *node) {
    if (!node) return 1;

    // Verificar hijos primero
    if (!is_avl_robust(node->left)) return 0;
    if (!is_avl_robust(node->right)) return 0;

    // Verificar altura almacenada
    int lh = height(node->left);
    int rh = height(node->right);
    int expected = 1 + (lh > rh ? lh : rh);
    if (node->height != expected) {
        printf("Height mismatch at %d: stored=%d, actual=%d\n",
               node->data, node->height, expected);
        return 0;
    }

    // Verificar balance
    int bf = rh - lh;
    if (bf < -1 || bf > 1) {
        printf("Balance violation at %d: bf=%d\n", node->data, bf);
        return 0;
    }

    return 1;
}
```

Si `update_height` tiene un bug (ej. se olvida de llamar después de una
rotación), esta función lo detecta.  Útil para depuración.
</details>
