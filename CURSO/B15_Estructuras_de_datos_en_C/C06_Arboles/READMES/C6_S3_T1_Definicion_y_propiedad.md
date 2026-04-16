# Definición y propiedad BST

## ¿Qué es un BST?

Un **árbol binario de búsqueda** (BST, Binary Search Tree) es un árbol
binario que cumple una propiedad de orden para cada nodo:

> Todos los valores en el subárbol **izquierdo** son **menores** que el
> nodo, y todos los valores en el subárbol **derecho** son **mayores**.

Formalmente, para cada nodo $N$ con dato $k$:

$$\forall\, x \in \text{subárbol\_izq}(N):\; x < k$$
$$\forall\, y \in \text{subárbol\_der}(N):\; y > k$$

Esta propiedad se cumple **recursivamente** — cada subárbol de un BST
es a su vez un BST.

---

## Ejemplo

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

Verificación nodo por nodo:

| Nodo | Subárbol izq | Subárbol der | ¿Cumple? |
|------|-------------|-------------|----------|
| 10 | {3, 5, 7} — todos < 10 | {12, 15, 20} — todos > 10 | Sí |
| 5 | {3} — 3 < 5 | {7} — 7 > 5 | Sí |
| 15 | {12} — 12 < 15 | {20} — 20 > 15 | Sí |
| 3 | {} | {} | Sí (hoja) |
| 7 | {} | {} | Sí (hoja) |
| 12 | {} | {} | Sí (hoja) |
| 20 | {} | {} | Sí (hoja) |

Este es el mismo árbol que usamos en S01 y S02 — era un BST todo el
tiempo.  Su recorrido inorden produce `3 5 7 10 12 15 20` — los valores
en orden ascendente.  Esto no era coincidencia.

---

## La propiedad fundamental: inorden = orden ascendente

**Teorema**: un árbol binario es un BST si y solo si su recorrido
inorden produce los valores en orden estrictamente creciente.

Demostración intuitiva: el inorden visita izquierda → nodo → derecha.
Si izquierda < nodo < derecha (propiedad BST), entonces el inorden
es creciente.  Y si el inorden es creciente, entonces para cada nodo
todo lo que se visitó antes (izquierda) es menor, y todo lo que se
visita después (derecha) es mayor.

```
Inorden del BST de ejemplo:  3  5  7  10  12  15  20
                              ↗  ↗  ↗   ↗   ↗   ↗
                            creciente → es BST
```

---

## Contraejemplo: NO es un BST

```
         10
        /  \
       5    15
      / \
     3   12        ← 12 > 10, pero está en el subárbol IZQUIERDO de 10
```

El nodo 12 es mayor que 5 (correcto como hijo derecho de 5), pero es
mayor que 10 (incorrecto — está en el subárbol izquierdo de 10 donde
todo debe ser < 10).

**Error común**: verificar solo padre-hijo (`5 < 12`, correcto) sin
verificar contra ancestros (`10 > 12`, incorrecto).  La propiedad BST
es sobre **todo el subárbol**, no solo el hijo inmediato.

Inorden: `3 5 12 10 15` — no es creciente (12 > 10).  Confirmado: no
es BST.

### Verificación correcta

Para verificar si un árbol es BST, cada nodo debe estar en un **rango**
definido por sus ancestros:

```
         10            rango: (-∞, +∞)
        /  \
       5    15         5 en (-∞, 10), 15 en (10, +∞)
      / \   / \
     3   7 12  20      3 en (-∞, 5), 7 en (5, 10), 12 en (10, 15), 20 en (15, +∞)
```

```c
int is_bst(TreeNode *root, int min, int max) {
    if (!root) return 1;
    if (root->data <= min || root->data >= max) return 0;

    return is_bst(root->left, min, root->data)
        && is_bst(root->right, root->data, max);
}

// Uso:
is_bst(root, INT_MIN, INT_MAX);
```

Alternativa equivalente: verificar que el inorden sea creciente (vista
en S02/T01, ejercicio 10).

---

## ¿Por qué BST? La búsqueda binaria en árboles

Un array ordenado permite búsqueda binaria en $O(\log n)$, pero
insertar o eliminar es $O(n)$ (hay que desplazar elementos).  Un BST
ofrece $O(\log n)$ para las tres operaciones — **si está balanceado**.

### Búsqueda en un BST

Para buscar el valor $k$:

1. Si el nodo actual es NULL → no encontrado.
2. Si $k$ = dato del nodo → encontrado.
3. Si $k$ < dato → buscar en el subárbol izquierdo.
4. Si $k$ > dato → buscar en el subárbol derecho.

En cada paso se descarta la mitad del árbol — es búsqueda binaria.

### Ejemplo: buscar 7

```
         10          7 < 10 → ir a la izquierda
        /
       5             7 > 5 → ir a la derecha
        \
         7           7 == 7 → encontrado
```

3 comparaciones para 7 nodos.  En un array lineal serían hasta 7.

### En C

```c
TreeNode *bst_search(TreeNode *root, int key) {
    if (!root) return NULL;
    if (key == root->data) return root;
    if (key < root->data) return bst_search(root->left, key);
    return bst_search(root->right, key);
}
```

### En Rust

```rust
fn bst_search(tree: &Tree<i32>, key: i32) -> bool {
    match tree {
        None => false,
        Some(node) => {
            if key == node.data { true }
            else if key < node.data { bst_search(&node.left, key) }
            else { bst_search(&node.right, key) }
        }
    }
}
```

---

## Complejidad: balanceado vs degenerado

La complejidad de todas las operaciones BST depende de la **altura**
del árbol:

| Forma del BST | Altura | Búsqueda | Inserción | Eliminación |
|---------------|--------|----------|-----------|-------------|
| Balanceado (perfecto) | $\lfloor \log_2 n \rfloor$ | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ |
| Promedio (aleatorio) | $\approx 1.39 \log_2 n$ | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ |
| Degenerado (lista) | $n - 1$ | $O(n)$ | $O(n)$ | $O(n)$ |

### BST balanceado vs degenerado

```
Balanceado (h=2):          Degenerado (h=6):

         10                1
        /  \                \
       5    15               2
      / \   / \               \
     3   7 12  20              3
                                \
7 nodos, h=2                    4
Buscar cualquier                 \
valor: ≤ 3 pasos                  5
                                   \
                                    6
                                     \
                                      7

                               7 nodos, h=6
                               Buscar 7: 7 pasos
```

El degenerado se produce al insertar valores ya ordenados:
`1, 2, 3, 4, 5, 6, 7`.  Cada nuevo valor es mayor que todos los
anteriores → siempre va a la derecha → se forma una lista.

---

## Análisis de la altura promedio

Si se insertan $n$ valores en orden **aleatorio** (permutación
uniforme), la altura esperada del BST es:

$$E[h] \approx 4.311 \cdot \ln n \approx 1.39 \cdot \log_2 n$$

Resultado demostrado por Reed (2003).  Esto significa que un BST con
inserciones aleatorias es "casi balanceado" en promedio.  El problema es
que en la práctica los datos no siempre llegan en orden aleatorio.

| $n$ | Altura mínima ($\lfloor \log_2 n \rfloor$) | Altura promedio ($\approx 1.39 \log_2 n$) | Altura máxima ($n-1$) |
|-----|---------------------------------------------|------------------------------------------|----------------------|
| 7 | 2 | 3–4 | 6 |
| 100 | 6 | 9 | 99 |
| 1000 | 9 | 14 | 999 |
| $10^6$ | 19 | 28 | 999999 |

---

## BST vs otras estructuras

| Operación | Array no ordenado | Array ordenado | Lista enlazada | BST balanceado |
|-----------|------------------|---------------|---------------|---------------|
| Buscar | $O(n)$ | $O(\log n)$ | $O(n)$ | $O(\log n)$ |
| Insertar | $O(1)$ amort. | $O(n)$ | $O(1)$ con puntero | $O(\log n)$ |
| Eliminar | $O(n)$ | $O(n)$ | $O(1)$ con puntero | $O(\log n)$ |
| Mínimo | $O(n)$ | $O(1)$ | $O(n)$ | $O(\log n)$ |
| Máximo | $O(n)$ | $O(1)$ | $O(n)$ | $O(\log n)$ |
| Recorrer ordenado | $O(n \log n)$ | $O(n)$ | $O(n \log n)$ | $O(n)$ inorden |

El BST balanceado es la única estructura que ofrece $O(\log n)$ para
buscar, insertar **y** eliminar simultáneamente.  Es el compromiso
ideal cuando las tres operaciones son frecuentes.

---

## Valores duplicados

La definición estricta de BST requiere valores **distintos**.  Hay
varias formas de manejar duplicados:

| Estrategia | Descripción | Uso típico |
|-----------|-------------|-----------|
| Prohibir | Rechazar inserciones duplicadas | Conjuntos (sets) |
| Contador | Cada nodo almacena la cantidad de repeticiones | Multisets, frecuencias |
| Subárbol izquierdo | Duplicados van a la izquierda ($\leq$ en vez de $<$) | Simple pero asimétrico |
| Subárbol derecho | Duplicados van a la derecha ($\geq$ en vez de $>$) | Simple pero asimétrico |

La convención más común en implementaciones de bibliotecas estándar
(como `std::set` en C++ o `BTreeSet` en Rust) es **prohibir
duplicados**.

### Con contador

```c
typedef struct BSTNode {
    int data;
    int count;                // repeticiones
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;
```

Si el valor ya existe, se incrementa `count` en vez de crear un nuevo
nodo.  Esto mantiene la estructura del árbol sin modificarla.

---

## Operaciones fundamentales del BST

| Operación | Complejidad | Recorrido asociado | Tema |
|-----------|------------|-------------------|------|
| Buscar | $O(h)$ | Descenso dirigido | T01 (aquí) |
| Insertar | $O(h)$ | Buscar posición + crear hoja | T02 |
| Mínimo/máximo | $O(h)$ | Ir al extremo izquierdo/derecho | T03 |
| Sucesor/predecesor | $O(h)$ | Subir o bajar | T03 |
| Eliminar | $O(h)$ | 3 casos | T04 |
| Verificar BST | $O(n)$ | Inorden | T01 |
| Recorrer ordenado | $O(n)$ | Inorden | T01/S02 |

Todas dependen de $h$ (la altura), no de $n$ directamente.  Por eso
mantener el árbol balanceado ($h = O(\log n)$) es crítico — tema de S04.

---

## Mínimo y máximo

El mínimo de un BST es el nodo **más a la izquierda**.  El máximo es el
nodo **más a la derecha**:

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

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
     ↑            ↑
    min          max
```

Complejidad: $O(h)$ — en el peor caso recorre toda la rama.

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
    TreeNode *n = malloc(sizeof(TreeNode));
    n->data = value;
    n->left = n->right = NULL;
    return n;
}

// --- Búsqueda ---
TreeNode *bst_search(TreeNode *root, int key) {
    if (!root) return NULL;
    if (key == root->data) return root;
    if (key < root->data) return bst_search(root->left, key);
    return bst_search(root->right, key);
}

// --- Búsqueda iterativa ---
TreeNode *bst_search_iter(TreeNode *root, int key) {
    while (root) {
        if (key == root->data) return root;
        root = (key < root->data) ? root->left : root->right;
    }
    return NULL;
}

// --- Mínimo / Máximo ---
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

// --- Verificar BST ---
int is_bst_helper(TreeNode *root, int min, int max) {
    if (!root) return 1;
    if (root->data <= min || root->data >= max) return 0;
    return is_bst_helper(root->left, min, root->data)
        && is_bst_helper(root->right, root->data, max);
}

int is_bst(TreeNode *root) {
    return is_bst_helper(root, INT_MIN, INT_MAX);
}

// --- Inorden ---
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

    printf("Inorder: ");
    inorder(root);
    printf("\n");

    printf("Is BST?  %s\n", is_bst(root) ? "yes" : "no");
    printf("Min:     %d\n", bst_min(root)->data);
    printf("Max:     %d\n", bst_max(root)->data);

    int keys[] = {7, 12, 99};
    for (int i = 0; i < 3; i++) {
        TreeNode *found = bst_search(root, keys[i]);
        printf("Search %d: %s\n", keys[i], found ? "found" : "not found");
    }

    // Romper la propiedad BST
    root->left->right->data = 11;  // 7 → 11 (ahora 11 > 10 en subárbol izq)
    printf("\nAfter breaking BST property (7 -> 11):\n");
    printf("Inorder: ");
    inorder(root);
    printf("\n");
    printf("Is BST?  %s\n", is_bst(root) ? "yes" : "no");

    tree_free(root);
    return 0;
}
```

Salida:

```
Inorder: 3 5 7 10 12 15 20
Is BST?  yes
Min:     3
Max:     20
Search 7: found
Search 12: found
Search 99: not found

After breaking BST property (7 -> 11):
Inorder: 3 5 11 10 12 15 20
Is BST?  no
```

---

## Ejercicios

### Ejercicio 1 — ¿Es BST?

¿Este árbol es un BST?

```
       8
      / \
     3   10
    / \    \
   1   6    14
      / \   /
     4   7 13
```

<details>
<summary>Predicción</summary>

Sí, es un BST.

Verificación por rangos:
- 8: rango (-∞, +∞) → cumple.
- 3: rango (-∞, 8) → 3 < 8 → cumple.
- 10: rango (8, +∞) → 10 > 8 → cumple.
- 1: rango (-∞, 3) → 1 < 3 → cumple.
- 6: rango (3, 8) → 3 < 6 < 8 → cumple.
- 14: rango (10, +∞) → 14 > 10 → cumple.
- 4: rango (3, 6) → 3 < 4 < 6 → cumple.
- 7: rango (6, 8) → 6 < 7 < 8 → cumple.
- 13: rango (10, 14) → 10 < 13 < 14 → cumple.

Inorden: `1 3 4 6 7 8 10 13 14` — creciente.
</details>

### Ejercicio 2 — Encontrar la violación

¿Este árbol es un BST?

```
       5
      / \
     2   8
    / \
   1   4
      /
     3
      \
       6
```

<details>
<summary>Predicción</summary>

No es BST.

El nodo 6 está en el subárbol izquierdo de 5, pero 6 > 5.  Rango de 6:
debe estar en (3, 4), pero 6 > 4.

La verificación padre-hijo local pasaría: 6 > 3 (correcto como hijo
derecho de 3).  Pero la verificación global falla: 6 está en el
subárbol izquierdo de 5.

Inorden: `1 2 3 6 4 5 8` — no creciente (6 > 4).
</details>

### Ejercicio 3 — Búsqueda: camino recorrido

En el BST del ejercicio 1, ¿qué nodos se visitan al buscar 7?  ¿Y al
buscar 5?

<details>
<summary>Predicción</summary>

Buscar 7: `8 → 3 → 6 → 7` (encontrado, 4 comparaciones).

- 7 < 8 → izquierda.
- 7 > 3 → derecha.
- 7 > 6 → derecha.
- 7 == 7 → encontrado.

Buscar 5: `8 → 3 → 6 → 4` → right de 4 es NULL → **no encontrado** (4
comparaciones).

- 5 < 8 → izquierda.
- 5 > 3 → derecha.
- 5 < 6 → izquierda.
- 5 > 4 → derecha → NULL.
</details>

### Ejercicio 4 — Mínimo y máximo

En el BST del ejercicio 1, ¿cuántos pasos (nodos visitados) se
necesitan para encontrar el mínimo?  ¿Y el máximo?

<details>
<summary>Predicción</summary>

Mínimo: `8 → 3 → 1` → **3 pasos**.  1 no tiene hijo izquierdo.

Máximo: `8 → 10 → 14` → **3 pasos**.  14 no tiene hijo derecho.

En ambos casos, los pasos = profundidad del nodo extremo + 1.
</details>

### Ejercicio 5 — BST con los mismos valores, diferente estructura

Los valores `{1, 3, 5, 7, 10, 12, 15, 20}` pueden formar muchos BSTs
distintos.  Dibuja dos BSTs con estos valores que tengan alturas
diferentes.

<details>
<summary>Ejemplos</summary>

Balanceado (h = 3):
```
            10
          /    \
        5       15
       / \     /  \
      3   7   12   20
     /
    1
```

Degenerado (h = 7):
```
  1
   \
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
```

Mismo inorden (`1 3 5 7 10 12 15 20`), pero alturas 3 vs 7.  La
estructura depende del **orden de inserción**, no de los valores en sí.
</details>

### Ejercicio 6 — Orden de inserción

¿Qué orden de inserción produce el BST balanceado de referencia?

```
         10
        /  \
       5    15
      / \   / \
     3   7 12  20
```

<details>
<summary>Respuesta</summary>

La raíz debe insertarse **primero**: `10`.  Luego los nodos del
siguiente nivel: `5, 15` (en cualquier orden).  Luego los del último
nivel: `3, 7, 12, 20` (en cualquier orden).

Una secuencia válida: `10, 5, 15, 3, 7, 12, 20`.

También funciona: `10, 15, 5, 7, 3, 20, 12` (cualquier permutación
donde 10 va primero, y cada nodo se inserta después de su padre).

En general: un orden BFS del árbol resultante produce ese árbol.
</details>

### Ejercicio 7 — Implementar is_bst en Rust

Implementa `is_bst` usando el método de rangos.  Usa `Option<i32>` para
representar $-\infty$ y $+\infty$.

<details>
<summary>Implementación</summary>

```rust
fn is_bst(tree: &Tree<i32>) -> bool {
    fn check(tree: &Tree<i32>, min: Option<i32>, max: Option<i32>) -> bool {
        match tree {
            None => true,
            Some(node) => {
                if let Some(lo) = min {
                    if node.data <= lo { return false; }
                }
                if let Some(hi) = max {
                    if node.data >= hi { return false; }
                }
                check(&node.left, min, Some(node.data))
                    && check(&node.right, Some(node.data), max)
            }
        }
    }
    check(tree, None, None)
}
```

`None` representa sin límite.  `Some(v)` representa el límite concreto.
Esto evita el problema de usar `INT_MIN`/`INT_MAX` como centinelas (que
fallaría si el árbol contiene esos valores).
</details>

### Ejercicio 8 — Complejidad de la verificación

`is_bst` recorre todos los nodos.  ¿Es posible verificar si un árbol
es BST en menos de $O(n)$?

<details>
<summary>Respuesta</summary>

No.  En el peor caso, la violación puede estar en cualquier hoja.
Considerar un BST perfecto de $n$ nodos donde la única violación es
que una hoja tiene un valor incorrecto.  Para encontrarla, hay que
visitar todas las hojas — que son $\lceil n/2 \rceil$ → $\Omega(n)$.

Verificar BST es $\Theta(n)$ en el peor caso.  Solo se puede terminar
antes si se encuentra una violación temprano (best case $O(1)$).
</details>

### Ejercicio 9 — BST como diccionario

Un BST puede funcionar como diccionario (mapa clave-valor).  Modifica
la estructura `TreeNode` para almacenar pares clave-valor.

<details>
<summary>Estructura</summary>

```c
typedef struct BSTNode {
    int key;
    char *value;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;
```

La propiedad BST se aplica solo a `key`.  La búsqueda compara claves
y retorna el valor asociado.  Es el principio detrás de `TreeMap` en
Java y `BTreeMap` en Rust.

```rust
struct BSTNode<K: Ord, V> {
    key: K,
    value: V,
    left: Option<Box<BSTNode<K, V>>>,
    right: Option<Box<BSTNode<K, V>>>,
}
```

`K: Ord` garantiza que las claves son comparables.  `V` puede ser
cualquier tipo.
</details>

### Ejercicio 10 — Cuántos BSTs distintos

¿Cuántos BSTs estructuralmente distintos existen con los valores
`{1, 2, 3}`?  Dibuja todos.

<details>
<summary>Todos los BSTs</summary>

Hay 5 BSTs distintos (número de Catalan $C_3 = 5$):

```
1.  1           2.  1         3.   2        4.  3       5.    3
     \               \           / \           /           /
      2               3         1   3        2            1
       \             /                      /              \
        3           2                      1                2
```

Verificar: el inorden de los 5 es siempre `1 2 3`.  La estructura
cambia según cuál valor es raíz.

Para $n$ valores, el número de BSTs distintos es el $n$-ésimo número
de Catalan:

$$C_n = \frac{1}{n+1}\binom{2n}{n} = \frac{(2n)!}{(n+1)!\,n!}$$

$C_1=1$, $C_2=2$, $C_3=5$, $C_4=14$, $C_5=42$, $C_6=132$.
</details>
