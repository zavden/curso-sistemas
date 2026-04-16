# Árboles rojinegros

## Definición

Un árbol rojinegro (Red-Black Tree, RB tree) es un BST donde cada nodo
tiene un **color** (rojo o negro) y se cumplen 5 propiedades que
garantizan balance:

1. **Cada nodo es rojo o negro.**
2. **La raíz es negra.**
3. **Cada hoja (NULL) es negra.** (Los NULLs se consideran nodos
   negros — a veces llamados nodos centinela.)
4. **Si un nodo es rojo, ambos hijos son negros.** (No hay dos rojos
   consecutivos.)
5. **Para cada nodo, todos los caminos desde él hasta las hojas NULL
   contienen el mismo número de nodos negros.** (Black-height uniforme.)

La propiedad 5 es la más fuerte — garantiza que ningún camino es más
del doble de largo que otro, lo que implica $h = O(\log n)$.

---

## Ejemplo

```
            8B
          /    \
        4R      12R
       / \      / \
      2B  6B  10B  14B
     / \         \
    1R  3R       11R
```

`B` = negro, `R` = rojo.

Verificación:
1. Cada nodo es R o B → sí.
2. Raíz (8) es B → sí.
3. NULLs son B → sí (implícito).
4. No hay rojos consecutivos: los hijos de 4R son 2B y 6B → sí.
   Los hijos de 12R son 10B y 14B → sí.
5. Black-height desde la raíz:
   - 8→4→2→1→NULL: nodos negros = {8, 2} = 2.
   - 8→4→2→3→NULL: nodos negros = {8, 2} = 2.
   - 8→4→6→NULL: nodos negros = {8, 6} = 2.
   - 8→12→10→NULL: nodos negros = {8, 10} = 2.
   - 8→12→10→11→NULL: nodos negros = {8, 10} = 2.
   - 8→12→14→NULL: nodos negros = {8, 14} = 2.

Todos los caminos tienen 2 nodos negros (sin contar el NULL final).
Black-height = 2.

---

## Black-height

El **black-height** de un nodo $n$, denotado $bh(n)$, es el número de
nodos negros en cualquier camino desde $n$ (sin incluirlo) hasta una
hoja NULL.  La propiedad 5 garantiza que este número es el mismo para
todos los caminos.

El black-height de la raíz determina los límites del árbol:
- Mínimo de nodos: $2^{bh} - 1$ (todos negros, árbol completo negro).
- Máximo de nodos: $2^{2 \cdot bh} - 1$ (alternando rojo-negro).

---

## Altura garantizada

Para un RB tree con $n$ nodos internos:

$$h \leq 2 \cdot \log_2(n + 1)$$

Demostración intuitiva: por la propiedad 4 (no dos rojos seguidos), al
menos la mitad de los nodos en cualquier camino son negros.  Si el
black-height es $bh$, la altura máxima es $2 \cdot bh$.  Como el
subárbol de un nodo con black-height $bh$ tiene al menos $2^{bh} - 1$
nodos internos, $n \geq 2^{bh} - 1$, así que $bh \leq \log_2(n+1)$ y
$h \leq 2 \cdot \log_2(n+1)$.

| $n$ | Altura máxima RB | Altura máxima AVL | BST degenerado |
|-----|-----------------|-------------------|----------------|
| 7 | 5 | 3 | 6 |
| 100 | 13 | 9 | 99 |
| 1000 | 19 | 14 | 999 |
| $10^6$ | 39 | 28 | $10^6 - 1$ |

El RB tree permite alturas mayores que AVL, pero sigue siendo
$O(\log n)$.

---

## Estructura del nodo

```c
typedef enum { RED, BLACK } Color;

typedef struct RBNode {
    int data;
    Color color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;    // necesario para las correcciones
} RBNode;
```

El puntero `parent` simplifica las correcciones post-inserción (acceder
al tío y al abuelo).  Alternativa: usar recursión y corregir en el
camino de vuelta (sin `parent`), similar al AVL.

Espacio: 1 byte para color (o 1 bit robado del puntero en
implementaciones optimizadas).

---

## Inserción

La inserción en un RB tree tiene dos fases:

1. **Inserción BST normal**: insertar el nuevo nodo como hoja.
2. **Colorear rojo**: el nuevo nodo siempre se inserta como **rojo**.
3. **Corregir violaciones**: si el padre es rojo, se viola la propiedad
   4.  Corregir con recoloreo y/o rotaciones.

¿Por qué insertar rojo?  Insertar negro violaría la propiedad 5
(black-height desigual en el camino donde se insertó).  Insertar rojo
solo puede violar la propiedad 4 (rojo-rojo) — más fácil de corregir.

### Casos de corrección

Sea `N` el nodo insertado (rojo), `P` su padre, `G` el abuelo, `U` el
tío (hermano de P).

Si P es negro → no hay violación, terminamos.

Si P es rojo → violación.  Tres casos:

**Caso 1: tío U es rojo.**
Recolorear: P y U a negro, G a rojo.  Luego verificar G (puede
propagarse hacia arriba).

```
Antes:                    Después (recolorear):

      G(B)                     G(R)   ← puede violar con su padre
     / \                      / \
   P(R)  U(R)              P(B)  U(B)
   /                        /
  N(R)                     N(R)
```

**Caso 2: tío U es negro y N es hijo "interno" (zigzag).**
Rotar N sobre P para convertir en caso 3.

```
      G(B)                     G(B)
     / \         rotate       / \
   P(R)  U(B)   ────────→  N(R)  U(B)
     \                      /
      N(R)                P(R)
```

**Caso 3: tío U es negro y N es hijo "externo" (línea).**
Rotar G, recolorear P y G.

```
      G(B)                     P(B)
     / \         rotate       / \
   P(R)  U(B)   ────────→  N(R)  G(R)
   /                              \
  N(R)                            U(B)
```

### Resumen de correcciones

| Caso | Tío | Posición de N | Acción | Propagación |
|------|-----|-------------|--------|-------------|
| 1 | Rojo | Cualquiera | Recolorear P, U, G | Sí (repetir en G) |
| 2 | Negro | Interno (zigzag) | Rotar N sobre P | Convierte en caso 3 |
| 3 | Negro | Externo (línea) | Rotar P sobre G + recolorear | No (terminamos) |

Al final, la raíz se fuerza a negro (propiedad 2).

---

## Ejemplo de inserción paso a paso

Insertar `10, 20, 30, 15, 25`:

```
Insertar 10 (raíz → forzar negro):
  10B

Insertar 20 (padre=10B → no viola):
  10B
    \
     20R

Insertar 30 (padre=20R → viola):
  Padre=20R, abuelo=10B, tío=NULL(B).
  Tío negro, N=30 es externo → Caso 3.
  rotate_left(10), recolorear.

      20B
     / \
   10R  30R

Insertar 15 (padre=10R → viola):
  Padre=10R, abuelo=20B, tío=30R.
  Tío rojo → Caso 1: recolorear.

      20B            20B
     / \    →       / \
   10R  30R       10B  30B
     \              \
      15R            15R

  G=20 era raíz → forzar negro (ya es negro).  Fin.

Insertar 25 (padre=30B → no viola):
      20B
     / \
   10B  30B
     \  /
    15R 25R
```

Resultado final: RB tree válido.

---

## Eliminación (resumen)

La eliminación es significativamente más compleja que la inserción.
Hay 6 subcasos.  La idea general:

1. Eliminar como BST normal.
2. Si el nodo eliminado era negro, se viola la propiedad 5 (un camino
   tiene un negro menos).  Se introduce un "doble negro" que se corrige
   con rotaciones y recoloreo.

La eliminación puede requerir hasta 3 rotaciones (constante) para
corregir.  No se propaga como la inserción.

La complejidad de la implementación es la principal desventaja de los
RB trees.  La eliminación típicamente ocupa 50-100 líneas de código con
múltiples casos y subcasos simétricos.

---

## Uso en la práctica

Los RB trees son una de las estructuras de datos más usadas en software
de sistemas:

| Uso | Implementación |
|-----|---------------|
| `std::map`, `std::set` (C++) | RB tree |
| `TreeMap`, `TreeSet` (Java) | RB tree |
| Kernel Linux (`rbtree.h`) | RB tree intrusivo |
| Completely Fair Scheduler (Linux) | RB tree para procesos por vruntime |
| Gestión de memoria virtual (Linux) | RB tree para VMAs (áreas de memoria) |
| `epoll` (Linux) | RB tree para file descriptors |

¿Por qué no AVL?  Los RB trees ofrecen mejor rendimiento en
**inserciones y eliminaciones** porque necesitan menos rotaciones.  En
escenarios con muchas modificaciones y pocas búsquedas (típico en
kernels), la ventaja es significativa.

### Kernel Linux: rbtree.h

El kernel usa una implementación intrusiva (el nodo RB se embebe en la
estructura del usuario):

```c
// Kernel Linux (simplificado)
struct rb_node {
    unsigned long rb_parent_color;  // parent pointer + color en 1 bit
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};

struct rb_root {
    struct rb_node *rb_node;
};

// Uso para el scheduler:
struct sched_entity {
    struct rb_node run_node;   // nodo RB embebido
    u64 vruntime;              // dato del scheduler
    // ...
};
```

El color se almacena en el **bit menos significativo** del puntero
`parent` (los punteros siempre están alineados a 4/8 bytes, así que los
2-3 bits bajos siempre son 0 — se puede reutilizar uno).  Cero bytes
extra por nodo.

---

## Implementación simplificada: Left-Leaning Red-Black (LLRB)

Robert Sedgewick propuso una variante simplificada donde los nodos rojos
solo pueden ser hijos **izquierdos**.  Esto reduce los casos a la mitad:

```c
typedef struct LLRBNode {
    int data;
    int is_red;         // 1 = rojo, 0 = negro
    struct LLRBNode *left;
    struct LLRBNode *right;
} LLRBNode;

int is_red(LLRBNode *n) {
    return n && n->is_red;
}

LLRBNode *rotate_left(LLRBNode *h) {
    LLRBNode *x = h->right;
    h->right = x->left;
    x->left = h;
    x->is_red = h->is_red;
    h->is_red = 1;
    return x;
}

LLRBNode *rotate_right(LLRBNode *h) {
    LLRBNode *x = h->left;
    h->left = x->right;
    x->right = h;
    x->is_red = h->is_red;
    h->is_red = 1;
    return x;
}

void flip_colors(LLRBNode *h) {
    h->is_red = !h->is_red;
    h->left->is_red = !h->left->is_red;
    h->right->is_red = !h->right->is_red;
}

LLRBNode *llrb_insert(LLRBNode *h, int value) {
    if (!h) {
        LLRBNode *n = malloc(sizeof(LLRBNode));
        n->data = value;
        n->is_red = 1;   // nuevo nodo siempre rojo
        n->left = n->right = NULL;
        return n;
    }

    if (value < h->data)
        h->left = llrb_insert(h->left, value);
    else if (value > h->data)
        h->right = llrb_insert(h->right, value);

    // Correcciones (3 líneas)
    if (is_red(h->right) && !is_red(h->left))
        h = rotate_left(h);
    if (is_red(h->left) && is_red(h->left->left))
        h = rotate_right(h);
    if (is_red(h->left) && is_red(h->right))
        flip_colors(h);

    return h;
}
```

La inserción LLRB tiene la misma estructura que la inserción AVL:
inserción BST + correcciones en el camino de vuelta.  Las 3 líneas de
corrección cubren todos los casos:

1. **Rojo a la derecha**: rotar izquierda (forzar left-leaning).
2. **Dos rojos consecutivos a la izquierda**: rotar derecha.
3. **Ambos hijos rojos**: flip colors (equivalente al caso 1 del RB
   clásico — tío rojo).

---

## Comparación: 5 propiedades visualmente

```
Propiedad 1: cada nodo es R o B
  ✓ Trivial por definición de la estructura.

Propiedad 2: raíz es B
  ✓ Se fuerza al final de cada inserción.

Propiedad 3: NULLs son B
  ✓ Convención — los NULLs no existen físicamente.

Propiedad 4: rojo → hijos negros
      B                B
     / \      OK      / \      VIOLACIÓN
    R   B            R   B
   / \              /
  B   B            R   ← rojo-rojo

Propiedad 5: black-height uniforme
    B(bh=2)              B(bh=?)
   / \                  / \
  B   B     OK        B   R     ¿VIOLACIÓN?
 /   / \             /     \
R   R   R           R       B
                    No necesariamente — depende de los NULLs.
```

---

## Programa de demostración (LLRB)

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct LLRBNode {
    int data;
    int is_red;
    struct LLRBNode *left;
    struct LLRBNode *right;
} LLRBNode;

int is_red(LLRBNode *n) { return n && n->is_red; }

LLRBNode *create_node(int value) {
    LLRBNode *n = malloc(sizeof(LLRBNode));
    n->data = value;
    n->is_red = 1;
    n->left = n->right = NULL;
    return n;
}

LLRBNode *rotate_left(LLRBNode *h) {
    LLRBNode *x = h->right;
    h->right = x->left;
    x->left = h;
    x->is_red = h->is_red;
    h->is_red = 1;
    return x;
}

LLRBNode *rotate_right(LLRBNode *h) {
    LLRBNode *x = h->left;
    h->left = x->right;
    x->right = h;
    x->is_red = h->is_red;
    h->is_red = 1;
    return x;
}

void flip_colors(LLRBNode *h) {
    h->is_red = !h->is_red;
    h->left->is_red = !h->left->is_red;
    h->right->is_red = !h->right->is_red;
}

LLRBNode *llrb_insert(LLRBNode *h, int value) {
    if (!h) return create_node(value);

    if (value < h->data)
        h->left = llrb_insert(h->left, value);
    else if (value > h->data)
        h->right = llrb_insert(h->right, value);

    if (is_red(h->right) && !is_red(h->left))    h = rotate_left(h);
    if (is_red(h->left) && is_red(h->left->left)) h = rotate_right(h);
    if (is_red(h->left) && is_red(h->right))      flip_colors(h);

    return h;
}

int tree_height(LLRBNode *n) {
    if (!n) return -1;
    int lh = tree_height(n->left);
    int rh = tree_height(n->right);
    return 1 + (lh > rh ? lh : rh);
}

int black_height(LLRBNode *n) {
    if (!n) return 0;
    int lbh = black_height(n->left);
    return lbh + (n->is_red ? 0 : 1);
}

void inorder(LLRBNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d%c ", root->data, root->is_red ? 'R' : 'B');
    inorder(root->right);
}

void print_tree(LLRBNode *root, int level) {
    if (!root) return;
    print_tree(root->right, level + 1);
    for (int i = 0; i < level; i++) printf("    ");
    printf("%d%c\n", root->data, root->is_red ? 'R' : 'B');
    print_tree(root->left, level + 1);
}

void tree_free(LLRBNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    LLRBNode *root = NULL;

    printf("=== Insert 1..10 ===\n");
    for (int i = 1; i <= 10; i++) {
        root = llrb_insert(root, i);
        root->is_red = 0;   // forzar raíz negra
    }

    printf("Tree:\n");
    print_tree(root, 0);
    printf("\nInorder: ");
    inorder(root);
    printf("\nHeight: %d, Black-height: %d\n",
           tree_height(root), black_height(root));

    // Comparar con inserción mayor
    tree_free(root);
    root = NULL;

    for (int i = 1; i <= 1000; i++) {
        root = llrb_insert(root, i);
        root->is_red = 0;
    }
    printf("\nn=1000: height=%d, black-height=%d\n",
           tree_height(root), black_height(root));

    tree_free(root);
    return 0;
}
```

Salida típica:

```
=== Insert 1..10 ===
Tree:
        10R
    9B
8B
        7R
    6B
            5R
        4B
            3R
    2B
        1B

Inorder: 1B 2B 3R 4B 5R 6B 7R 8B 9B 10R
Height: 4, Black-height: 3

n=1000: height=16, black-height=9
```

Para 1000 inserciones en orden: altura 16 (vs 999 en BST, vs 14 en AVL,
vs 9 óptimo).

---

## Ejercicios

### Ejercicio 1 — Verificar las 5 propiedades

¿Este árbol cumple las 5 propiedades RB?

```
        10B
       / \
      5R  15R
     / \
    3B  7B
```

<details>
<summary>Verificación</summary>

1. Cada nodo es R o B → sí.
2. Raíz (10) es B → sí.
3. NULLs son B → sí.
4. Rojo → hijos negros: 5R tiene hijos 3B y 7B → sí.
   15R tiene hijos NULL(B) y NULL(B) → sí.
5. Black-height uniforme desde la raíz:
   - 10→5→3→NULL: negros = {10, 3} = 2.
   - 10→5→7→NULL: negros = {10, 7} = 2.
   - 10→15→NULL: negros = {10, 15}... espera, 15 es rojo.
     negros en camino = {10} = 1.

**Violación de propiedad 5.** Los caminos por la izquierda tienen
black-height 2, pero el camino 10→15→NULL tiene black-height 1.

No es un RB tree válido.
</details>

### Ejercicio 2 — Black-height

Calcula el black-height de cada nodo:

```
            8B
          /    \
        4R      12B
       / \      / \
      2B  6B  10R  14B
                \
                11B
```

<details>
<summary>Cálculos</summary>

Desde las hojas hacia arriba:
- 2B: bh = 1 (sin contar el nodo, camino al NULL = 0 negros... La
  convención puede variar).

Usando la convención "bh = número de nodos negros desde el nodo
(sin incluirlo) hasta NULL":
- Hojas (2, 6, 11, 14): bh = 0 (camino a NULL sin nodos negros intermedios).
- 10R: left=NULL(bh=0), right=11B→NULL: bh = 1. ¿Uniforme? left: 0 negros.
  right: 1 negro (11B). **No uniforme** → violación.

Espera, revisemos. 10R: left = NULL (bh=0), right = 11B (un negro antes
de NULL, bh=1). Propiedad 5 requiere que ambos caminos tengan el mismo
número de negros. Izquierda: 0. Derecha: 1. **Violación.**

Para corregir: 10 debería ser negro, o 11 debería ser rojo, o agregar
un nodo negro a la izquierda de 10.
</details>

### Ejercicio 3 — Inserción LLRB: traza

Traza la inserción de 5, 3, 7 en un LLRB vacío.

<details>
<summary>Traza</summary>

```
Insertar 5:
  5R → raíz, forzar negro → 5B

Insertar 3:
  3 < 5 → left.
  5B
 /
3R

  Correcciones en 5:
  - is_red(right)? No.
  - is_red(left) && is_red(left->left)? left=3R, left->left=NULL → No.
  - is_red(left) && is_red(right)? left=3R, right=NULL → No.
  Sin cambios.  Forzar raíz negra → ya es negro.

Insertar 7:
  7 > 5 → right.
  5B
 / \
3R  7R

  Correcciones en 5:
  - is_red(right) && !is_red(left)? right=7R, left=3R → No (left es rojo).
  - is_red(left) && is_red(left->left)? left->left=NULL → No.
  - is_red(left) && is_red(right)? 3R y 7R → Sí → flip_colors.

  5R           → forzar raíz negra → 5B
 / \                                 / \
3B  7B                              3B  7B
```

Resultado: árbol perfecto negro.
</details>

### Ejercicio 4 — ¿Por qué insertar rojo?

Explica por qué un nodo nuevo se inserta siempre como rojo, no negro.

<details>
<summary>Explicación</summary>

Insertar un nodo **negro** violaría la propiedad 5 (black-height
uniforme) en todos los casos: el camino que pasa por el nuevo nodo
tendría un negro más que los demás caminos.  Corregir esto requeriría
propagar cambios por todo el árbol.

Insertar **rojo** solo puede violar la propiedad 4 (no rojo-rojo) si
el padre es rojo.  Si el padre es negro, no hay violación alguna.  La
propiedad 5 no se afecta porque los nodos rojos no cuentan en el
black-height.

Es más fácil corregir una violación de propiedad 4 (local, se arregla
con 1-2 rotaciones) que de propiedad 5 (global, afecta todos los
caminos).
</details>

### Ejercicio 5 — Caso 1: tío rojo

En esta situación, aplica la corrección del caso 1:

```
        G(B)
       / \
      P(R) U(R)
     /
    N(R)
```

<details>
<summary>Resultado</summary>

Caso 1: recolorear P, U → negro; G → rojo.

```
        G(R)
       / \
      P(B) U(B)
     /
    N(R)
```

El black-height no cambió: antes, los caminos pasaban por un negro
(G).  Después, pasan por un negro (P o U) — el mismo conteo.

Pero G ahora es rojo — si el padre de G es rojo, se viola propiedad 4
de nuevo.  La corrección se **propaga** hacia arriba.  En el peor caso,
se propaga hasta la raíz y se fuerza la raíz a negro.
</details>

### Ejercicio 6 — LLRB: insertar 1..7

Inserta 1, 2, 3, 4, 5, 6, 7 en un LLRB.  ¿Cuál es la altura final?

<details>
<summary>Resultado</summary>

El LLRB rebalancea automáticamente.  Para 7 inserciones en orden
ascendente, el resultado es un árbol balanceado.

Altura final: **3-4** (dependiendo de la distribución exacta de rojos
y negros).  Black-height: 2-3.

Comparar: BST daría altura 6.  AVL daría altura 2 (perfecto).  El RB
tree permite un poco más de altura que AVL, pero con menos rotaciones
durante la construcción.
</details>

### Ejercicio 7 — Color del bit en el puntero

El kernel Linux almacena el color en el bit menos significativo del
puntero `parent`.  ¿Por qué esto funciona?

<details>
<summary>Explicación</summary>

En arquitecturas de 64 bits, los punteros a structs están alineados a
al menos 4 bytes (generalmente 8).  Esto significa que los 2-3 bits
menos significativos de cualquier puntero válido siempre son 0.

```
Puntero alineado a 8: 0x7fff_dead_beef_0
                                       ↑
                                  bit 0 siempre 0

Con color:             0x7fff_dead_beef_1
                                       ↑
                                  1 = RED
```

Para obtener el puntero real: `ptr & ~1` (limpiar bit 0).
Para obtener el color: `ptr & 1`.

Ventaja: 0 bytes extra por nodo.  El color es "gratis".
Desventaja: cada acceso al parent requiere enmascarar el bit.
</details>

### Ejercicio 8 — Máximo de rotaciones

¿Cuántas rotaciones puede requerir una inserción en un RB tree?  ¿Y una
eliminación?

<details>
<summary>Respuesta</summary>

**Inserción**: a lo sumo **2 rotaciones** (una doble, que son 2 simples).
El caso 1 (recoloreo) puede propagarse $O(\log n)$ veces, pero no
implica rotaciones.  Cuando se llega al caso 2 o 3, se hacen 1-2
rotaciones y se termina.

**Eliminación**: a lo sumo **3 rotaciones**.  Después de las rotaciones,
el árbol está corregido.  No hay propagación de rotaciones.

Comparar con AVL:
- AVL inserción: a lo sumo 2 rotaciones.
- AVL eliminación: hasta $O(\log n)$ rotaciones.

El RB tree tiene mejor peor caso para eliminación en términos de
rotaciones.
</details>

### Ejercicio 9 — RB tree mínimo

¿Cuál es el menor RB tree válido con black-height 2?

<details>
<summary>Resultado</summary>

Black-height 2 significa que todo camino raíz→NULL tiene 2 nodos negros
(sin contar NULL).

Mínimo: todos negros, sin rojos.

```
      B
     / \
    B   B
```

3 nodos, todos negros, bh=2.  Cada camino tiene 2 negros.

Con nodos adicionales (rojos):

```
        B
       / \
      R   B
     / \
    B   B
```

5 nodos, bh=2 (caminos: B→R→B→NULL = 2 negros, B→B→NULL = 2 negros).

El mínimo para bh=2 es **3 nodos** (todos negros).
El máximo para bh=2 es **$2^{2 \cdot 2} - 1 = 15$ nodos** (alternando
rojo-negro en un árbol perfecto de altura 3).
</details>

### Ejercicio 10 — ¿Por qué RB en el kernel?

El kernel Linux usa RB trees en vez de AVL trees.  ¿Por qué?

<details>
<summary>Razones</summary>

1. **Menos rotaciones en eliminación**: RB elimina con ≤ 3 rotaciones
   vs $O(\log n)$ en AVL.  En el kernel, las eliminaciones son
   frecuentes (procesos terminan, VMAs se desasignan).

2. **Menor overhead por nodo**: el color se almacena en 1 bit (robado
   del puntero parent) vs 4 bytes de altura en AVL.  En el kernel,
   donde hay millones de nodos (VMAs, file descriptors), esto importa.

3. **Implementación intrusiva**: el nodo RB (`struct rb_node`) se
   embebe directamente en la estructura del usuario.  Esto evita
   asignaciones de memoria extra y mejora la localidad de caché.

4. **Rendimiento predecible**: las rotaciones constantes (≤ 3) hacen
   el peor caso más predecible, importante para un scheduler de tiempo
   real.

5. **Historia**: la implementación RB del kernel fue escrita por Andrea
   Arcangeli en 1999 y ha sido extensivamente probada y optimizada.
   Cambiar a AVL requeriría reescribir y re-testear código crítico sin
   beneficio claro.
</details>
