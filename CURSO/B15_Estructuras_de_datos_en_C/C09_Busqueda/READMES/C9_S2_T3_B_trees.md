# B-trees

## Objetivo

Comprender el B-tree como estructura de búsqueda **optimizada para
almacenamiento secundario** (disco, SSD), donde el costo dominante
no es la comparación sino el **acceso a página**:

- Nodos con **múltiples claves** e hijos — reducen la altura del
  árbol drásticamente respecto a árboles binarios.
- Búsqueda $O(\log_t n)$ accesos a disco, donde $t$ es el **grado
  mínimo** (orden) del árbol.
- Inserción con **split** de nodos llenos.
- Eliminación con **merge** y **redistribución** entre hermanos.
- Relación con los balanceados binarios de T02 y con los B+ trees
  de T04.

Referencia: la implementación completa de BST y balanceados binarios
está en **C06/S03-S04**. Aquí el foco es el diseño multi-vía.

---

## El problema: disco vs RAM

Todos los árboles vistos hasta ahora (BST, AVL, Red-Black) son
**binarios**: cada nodo tiene a lo sumo 2 hijos. Su altura es
$O(\log_2 n)$, lo que significa ~20 comparaciones para $10^6$
elementos. En RAM, 20 accesos a nodo cuestan ~400 ns — aceptable.

Pero en **disco** cada acceso a nodo es un acceso a página:

| Medio | Latencia por acceso | 20 accesos |
|-------|-------------------:|----------:|
| RAM (L3 miss) | ~100 ns | ~2 µs |
| SSD (NVMe) | ~100 µs | ~2 ms |
| SSD (SATA) | ~200 µs | ~4 ms |
| HDD | ~10 ms | ~200 ms |

Para $10^6$ registros en HDD, un AVL necesita ~20 accesos a disco =
**200 ms por búsqueda**. Inaceptable para una base de datos que
recibe miles de consultas por segundo.

La solución: **hacer que cada nodo ocupe exactamente una página de
disco** (típicamente 4 KB o 16 KB) y almacenar **cientos de claves
por nodo**. Esto reduce la altura a 2-4 niveles, y por tanto a 2-4
accesos a disco por búsqueda.

---

## Definición formal

Un **B-tree de grado mínimo** $t$ (con $t \geq 2$) cumple:

1. **Cada nodo** almacena entre $t-1$ y $2t-1$ claves, ordenadas.
   La raíz puede tener desde 1 hasta $2t-1$ claves.
2. **Cada nodo interno** con $k$ claves tiene exactamente $k+1$
   hijos.
3. Las claves de un nodo separan los rangos de sus subárboles:
   si un nodo tiene claves $[c_1, c_2, \ldots, c_k]$, entonces el
   hijo $i$-ésimo contiene valores en el rango
   $(c_{i-1}, c_i)$ (con $c_0 = -\infty$ y $c_{k+1} = +\infty$).
4. **Todas las hojas** están al mismo nivel (el árbol es
   perfectamente balanceado en profundidad).

### Terminología

La literatura usa dos convenciones incompatibles:

| Convención | Nombre | Significado |
|------------|--------|-------------|
| Knuth | "orden $m$" | máximo de hijos = $m$, máximo claves = $m-1$ |
| CLRS | "grado mínimo $t$" | mínimo de claves = $t-1$, máximo = $2t-1$ |

La relación es $m = 2t$. Un "B-tree de orden 5" (Knuth) es un
"B-tree de grado mínimo 3" (CLRS, $t=3$): máximo 5 hijos, 2-4
claves por nodo (excepto raíz). En este documento usamos la
convención CLRS ($t$) por claridad.

### Caso especial: 2-3 tree ($t = 2$)

Con $t = 2$: cada nodo tiene 1-3 claves y 2-4 hijos. El caso más
simple ($t = 2$) es el **2-3 tree**: cada nodo interno tiene 2 o 3
hijos (1 o 2 claves). Es el B-tree más pequeño posible y sirve como
ejemplo pedagógico, aunque en la práctica $t$ es mucho mayor.

---

## Altura de un B-tree

La propiedad central: un B-tree de grado mínimo $t$ con $n$ claves
tiene altura:

$$h \leq \log_t \frac{n+1}{2}$$

**Derivación**: la raíz tiene al menos 1 clave. Los demás nodos
internos tienen al menos $t-1$ claves, con al menos $t$ hijos.
A profundidad $d$ hay al menos $2t^{d-1}$ nodos (2 en nivel 1,
$2t$ en nivel 2, $2t^2$ en nivel 3...). El número total de claves
es al menos:

$$n \geq 1 + (t-1) \sum_{d=1}^{h} 2t^{d-1} = 1 + 2(t-1) \cdot \frac{t^h - 1}{t - 1} = 2t^h - 1$$

Por tanto $t^h \leq \frac{n+1}{2}$, es decir
$h \leq \log_t \frac{n+1}{2}$.

### Comparación de alturas

Para $n = 10^6$ y distintos valores de $t$:

| Estructura | $t$ o base | Altura máxima | Accesos por búsqueda |
|------------|--------:|--------:|---------:|
| BST balanceado | 2 | ~20 | ~20 |
| 2-3 tree | 2 | ~20 | ~20 |
| B-tree $t=50$ | 50 | ~4 | ~4 |
| B-tree $t=500$ | 500 | ~3 | ~3 |
| B-tree $t=5000$ | 5000 | ~2 | ~2 |

Con $t=500$ (un nodo de ~4 KB con claves de 4 bytes y punteros de
4 bytes), la altura es $\leq 3$ para un millón de registros. Esto
significa **3 lecturas de disco** en el peor caso — 600 µs en SATA
SSD vs 200 ms con un árbol binario.

---

## Estructura del nodo

```c
#define T 3  // grado minimo

typedef struct BTreeNode {
    int keys[2 * T - 1];     // claves ordenadas
    struct BTreeNode *children[2 * T];  // punteros a hijos
    int n;                    // numero actual de claves
    bool leaf;                // es hoja?
} BTreeNode;
```

Con $t = 3$: cada nodo almacena hasta 5 claves y 6 punteros a hijos.
En disco real, $t$ se elige para que `sizeof(BTreeNode)` sea
cercano al tamaño de página (4096 o 16384 bytes).

### Cálculo de $t$ óptimo

Si cada clave ocupa $K$ bytes, cada puntero a hijo $P$ bytes, y la
página es de $B$ bytes, queremos maximizar $t$ tal que un nodo quepa
en una página:

$$(2t - 1) \cdot K + 2t \cdot P + \text{overhead} \leq B$$

Para $K = 8$ (int64), $P = 8$ (puntero), overhead = 16 bytes,
$B = 4096$:

$$16t - 8 + 16t + 16 \leq 4096 \implies 32t \leq 4088 \implies t \leq 127$$

Con $t = 127$: máximo 253 claves y 254 hijos por nodo. Para $10^9$
registros: $h \leq \log_{127}(5 \times 10^8) \approx 4.1$, es decir
**5 accesos a disco** para mil millones de registros.

---

## Búsqueda en B-tree

La búsqueda es una generalización directa de la búsqueda en BST:
en vez de comparar con 1 clave y elegir izquierda/derecha, se
compara con hasta $2t-1$ claves y se elige entre $2t$ hijos.

### Algoritmo

```
BTREE_SEARCH(node, key):
    i = 0
    while i < node.n and key > node.keys[i]:
        i++
    if i < node.n and key == node.keys[i]:
        return (node, i)          // encontrada
    if node.leaf:
        return NULL               // no existe
    return BTREE_SEARCH(node.children[i], key)
```

### Búsqueda dentro del nodo

La búsqueda lineal dentro del nodo tiene costo $O(t)$. Para $t$
grande se puede usar **búsqueda binaria** dentro del nodo,
reduciendo a $O(\log t)$. Pero en la práctica el nodo cabe en
1-2 líneas de caché, y la búsqueda lineal con predicción de rama
es competitiva para $t < 200$.

### Traza de búsqueda

B-tree de $t = 2$ (2-3 tree) con las claves
$\{1, 3, 5, 7, 10, 12, 15, 18, 20\}$:

```
Estructura:
              [10]
            /      \
       [3, 7]     [15, 20]
      / |  \      / |   \
    [1] [5] [8] [12] [18] [22]
```

**Buscar 18**:

```
Paso 1: nodo raiz [10]
        18 > 10 → ir a hijo derecho

Paso 2: nodo [15, 20]
        18 > 15 → siguiente clave
        18 < 20 → ir a hijo medio (children[1])

Paso 3: nodo hoja [18]
        18 == 18 → ENCONTRADO en posicion 0

Resultado: encontrado en 3 accesos (= altura del arbol)
```

**Buscar 6**:

```
Paso 1: nodo raiz [10]
        6 < 10 → ir a hijo izquierdo

Paso 2: nodo [3, 7]
        6 > 3 → siguiente clave
        6 < 7 → ir a hijo medio (children[1])

Paso 3: nodo hoja [5]
        6 > 5 → i = 1, pero i >= n (n=1)
        Es hoja → NO ENCONTRADO

Resultado: no encontrado en 3 accesos
```

### Complejidad

| Operación | Accesos a disco | Comparaciones totales |
|-----------|:---------:|:----------:|
| Búsqueda | $O(\log_t n)$ | $O(t \cdot \log_t n) = O(\frac{t \cdot \log n}{\log t})$ |
| Con binaria intra-nodo | $O(\log_t n)$ | $O(\log t \cdot \log_t n) = O(\log n)$ |

El número de comparaciones totales con búsqueda binaria intra-nodo
es $O(\log n)$ — igual que un AVL. La ventaja no está en las
comparaciones sino en los **accesos a disco**: $O(\log_t n)$ en vez
de $O(\log_2 n)$.

---

## Inserción

La inserción en B-tree es más compleja que en BST porque hay que
mantener las invariantes (todas las hojas al mismo nivel, cada nodo
con $\leq 2t-1$ claves).

### Principio: split preventivo (top-down)

La estrategia de CLRS es **split on the way down**: al descender
buscando la posición de inserción, si encontramos un nodo lleno
($2t-1$ claves), lo partimos **antes** de descender a él. Esto
garantiza que cuando llegamos a la hoja, siempre hay espacio.

### Split de un nodo

Cuando un nodo tiene $2t-1$ claves (está lleno):

1. La clave **mediana** (posición $t-1$) sube al padre.
2. Las $t-1$ claves menores quedan en el nodo original.
3. Las $t-1$ claves mayores se mueven a un nuevo nodo.
4. Los hijos se reparten correspondientemente.

```
Antes (t=3, nodo lleno con 5 claves):

    padre: [..., A, ...]
               |
    nodo:  [10, 20, 30, 40, 50]

Split (mediana = 30 sube):

    padre: [..., A, 30, ...]
               |     |
    nodo:  [10, 20] [40, 50]
```

Si el padre también estaba lleno, ya lo habremos partido al
descender (split preventivo), así que siempre hay espacio para
recibir la mediana.

### Caso especial: split de la raíz

Cuando la raíz está llena y se parte:

1. Se crea una **nueva raíz** con 0 claves.
2. La raíz vieja se convierte en hijo de la nueva raíz.
3. Se hace split de la raíz vieja.

Este es el **único mecanismo** por el que un B-tree crece en
altura. El árbol crece **hacia arriba**, no hacia abajo.

### Traza de inserción

B-tree $t = 2$ (2-3 tree), insertando la secuencia
$\{10, 20, 30, 40, 50, 25\}$:

```
Insertar 10:
  [10]                              (raiz, 1 clave)

Insertar 20:
  [10, 20]                          (raiz, 2 claves)

Insertar 30:
  [10, 20, 30]  ← raiz llena (3 = 2t-1)
  Split raiz: mediana = 20 sube

      [20]
     /    \
  [10]   [30]                       (altura crece a 2)

Insertar 40:
      [20]
     /    \
  [10]   [30, 40]                   (espacio en hoja)

Insertar 50:
  Descender: hijo derecho [30, 40, 50] esta lleno → split
  Mediana 40 sube a raiz:

      [20, 40]
     /   |    \
  [10] [30]  [50]

Insertar 25:
  Descender: 25 > 20, 25 < 40 → hijo medio [30]
  Insertar: [25, 30]

      [20, 40]
     /   |    \
  [10] [25, 30] [50]               (estado final)
```

### Implementación del split

```c
void split_child(BTreeNode *parent, int i) {
    BTreeNode *full = parent->children[i];
    BTreeNode *new_node = create_node(full->leaf);

    new_node->n = T - 1;

    // copiar las t-1 claves mayores al nuevo nodo
    for (int j = 0; j < T - 1; j++)
        new_node->keys[j] = full->keys[j + T];

    // copiar los t hijos mayores (si no es hoja)
    if (!full->leaf) {
        for (int j = 0; j < T; j++)
            new_node->children[j] = full->children[j + T];
    }

    full->n = T - 1;

    // hacer espacio en el padre para el nuevo hijo
    for (int j = parent->n; j > i; j--)
        parent->children[j + 1] = parent->children[j];
    parent->children[i + 1] = new_node;

    // subir la mediana al padre
    for (int j = parent->n - 1; j >= i; j--)
        parent->keys[j + 1] = parent->keys[j];
    parent->keys[i] = full->keys[T - 1];

    parent->n++;
}
```

---

## Eliminación

La eliminación es la operación más compleja del B-tree. Hay que
garantizar que después de eliminar, cada nodo (excepto la raíz)
siga teniendo al menos $t-1$ claves.

### Tres casos

**Caso 1: la clave está en una hoja**

Si la hoja tiene más de $t-1$ claves, simplemente se elimina.
Si tiene exactamente $t-1$ (mínimo), hay que arreglar antes
(ver "fix underflow" abajo).

**Caso 2: la clave está en un nodo interno**

Se reemplaza con:
- El **predecesor**: la clave mayor del subárbol izquierdo
  (bajar siempre a la derecha hasta llegar a una hoja).
- O el **sucesor**: la clave menor del subárbol derecho
  (bajar siempre a la izquierda hasta llegar a una hoja).

Luego se elimina el predecesor/sucesor de su hoja (caso 1).

**Caso 3: la clave no existe**

La búsqueda llega a una hoja sin encontrarla — no hacer nada.

### Fix underflow: merge y redistribución

Cuando al descender encontramos un hijo con exactamente $t-1$
claves (mínimo), hay que asegurar que tenga al menos $t$ antes de
descender a él (para poder eliminar de él sin violar la
invariante). Dos estrategias:

**Redistribución (rotation/borrow)**: si un hermano adyacente
tiene más de $t-1$ claves, "pedimos prestada" una clave:

```
Antes (t=3, hijo tiene 2 claves = minimo):

    padre: [..., 30, ...]
               |     |
    hijo:  [10, 20]  hermano: [40, 50, 60, 70]

Redistribuir (rotar izquierda):

    padre: [..., 40, ...]
               |     |
    hijo:  [10, 20, 30]  hermano: [50, 60, 70]
```

La clave del padre baja al hijo, y la clave adyacente del hermano
sube al padre.

**Merge**: si ambos hermanos adyacentes tienen exactamente $t-1$
claves, se fusionan el hijo + la clave separadora del padre +
un hermano:

```
Antes (t=3, hijo y hermano con 2 claves = minimo):

    padre: [..., 30, ...]
               |     |
    hijo:  [10, 20]  hermano: [40, 50]

Merge:

    padre: [..., ...]          (30 baja al nodo fusionado)
               |
    fusionado: [10, 20, 30, 40, 50]
```

El nodo fusionado tiene $2(t-1) + 1 = 2t - 1$ claves (lleno pero
válido). El padre pierde una clave y un hijo.

Si el padre era la raíz y queda vacío (0 claves), el nodo fusionado
se convierte en la nueva raíz. Este es el **único mecanismo** por
el que un B-tree decrece en altura.

### Traza de eliminación

Partiendo del árbol:

```
      [20, 40]
     /   |    \
  [10] [25, 30] [50]
```

**Eliminar 25** (caso 1, hoja con 2 claves > mínimo):

```
      [20, 40]
     /   |    \
  [10]  [30]  [50]
```

**Eliminar 40** (caso 2, nodo interno):

Reemplazar con sucesor (50, la menor del subárbol derecho):

```
      [20, 50]
     /   |    \
  [10]  [30]  []  ← hoja vacia! underflow
```

Fix: merge [30] + 50 + [] → pero en la práctica se hace top-down,
verificando antes de descender. Resultado:

```
      [20]
     /    \
  [10]   [30, 50]
```

---

## B-tree vs árboles binarios

| Aspecto | BST/AVL/RB | B-tree ($t$ grande) |
|---------|-----------|---------------------|
| Altura ($n = 10^6$) | ~20 | ~3-4 |
| Accesos a disco | ~20 | ~3-4 |
| Comparaciones totales | ~20 | ~$20 \times \log t$ (lineal) o ~20 (binaria) |
| Cache locality | pobre (nodos dispersos) | excelente (nodo = página) |
| Inserción | 1 split/rotación local | split puede propagar |
| Complejidad de implementación | media | alta |
| Uso principal | RAM | disco / bases de datos |

### Cuándo usar B-tree sobre binarios

- **Almacenamiento en disco**: siempre B-tree (o B+).
- **En RAM con datos grandes**: B-tree puede ganar por cache
  locality (cada nodo ocupa líneas de caché contiguas).
- **En RAM con datos pequeños**: AVL/RB suelen ser más simples
  y competitivos (la ventaja de cache del B-tree es menor si el
  dataset cabe en L2/L3).

---

## B-tree en la práctica

### Bases de datos

Casi toda base de datos relacional usa B-trees (o la variante
B+ tree del siguiente tópico) para sus índices:

| Base de datos | Estructura de índice |
|---------------|---------------------|
| PostgreSQL | B+ tree (nbtree) |
| MySQL InnoDB | B+ tree |
| SQLite | B+ tree (página 4 KB) |
| Oracle | B+ tree |
| MongoDB | B-tree |

### Sistemas de archivos

| Filesystem | Uso de B-tree |
|------------|--------------|
| NTFS (Windows) | B+ tree para MFT |
| HFS+ (macOS legacy) | B-tree para catálogo |
| APFS (macOS) | B-tree copy-on-write |
| Btrfs (Linux) | B-tree (de ahí el nombre) |
| ext4 (Linux) | HTree (variante hash + B-tree) para directorios |

### Rust: BTreeMap y BTreeSet

La biblioteca estándar de Rust usa **B-tree** (no Red-Black) para
sus colecciones ordenadas. El `BTreeMap<K, V>` es un B-tree con
$B = 6$ (grado mínimo, hasta 11 claves por nodo).

La elección se debe a:
- **Cache locality**: 11 claves contiguas en memoria vs 1 por nodo
  en RB.
- **Ownership**: un B-tree tiene menos nodos, menos allocaciones,
  y es más amigable con el modelo de ownership de Rust.
- **Sin punteros a padre**: los RB-trees necesitan puntero al
  padre para rotaciones, lo que requiere `unsafe` o `Rc<RefCell<>>`.

---

## Programa completo en C

```c
// btree_search.c — B-tree: insertion, search, traversal, deletion
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define T 3  // minimum degree (max 2T-1 = 5 keys per node)

// --- node structure ---

typedef struct BTreeNode {
    int keys[2 * T - 1];
    struct BTreeNode *children[2 * T];
    int n;      // current number of keys
    bool leaf;
} BTreeNode;

typedef struct {
    BTreeNode *root;
} BTree;

BTreeNode *create_node(bool leaf) {
    BTreeNode *node = calloc(1, sizeof(BTreeNode));
    node->leaf = leaf;
    return node;
}

BTree *create_btree(void) {
    BTree *tree = malloc(sizeof(BTree));
    tree->root = create_node(true);
    return tree;
}

// --- search ---

typedef struct {
    BTreeNode *node;
    int index;
} SearchResult;

SearchResult btree_search(BTreeNode *node, int key) {
    int i = 0;
    while (i < node->n && key > node->keys[i])
        i++;

    if (i < node->n && key == node->keys[i])
        return (SearchResult){node, i};

    if (node->leaf)
        return (SearchResult){NULL, -1};

    return btree_search(node->children[i], key);
}

// --- traversal (in-order) ---

void btree_traverse(BTreeNode *node) {
    for (int i = 0; i < node->n; i++) {
        if (!node->leaf)
            btree_traverse(node->children[i]);
        printf("%d ", node->keys[i]);
    }
    if (!node->leaf)
        btree_traverse(node->children[node->n]);
}

// --- split child ---

void split_child(BTreeNode *parent, int i) {
    BTreeNode *full = parent->children[i];
    BTreeNode *right = create_node(full->leaf);
    right->n = T - 1;

    for (int j = 0; j < T - 1; j++)
        right->keys[j] = full->keys[j + T];

    if (!full->leaf) {
        for (int j = 0; j < T; j++)
            right->children[j] = full->children[j + T];
    }

    full->n = T - 1;

    for (int j = parent->n; j > i; j--)
        parent->children[j + 1] = parent->children[j];
    parent->children[i + 1] = right;

    for (int j = parent->n - 1; j >= i; j--)
        parent->keys[j + 1] = parent->keys[j];
    parent->keys[i] = full->keys[T - 1];

    parent->n++;
}

// --- insert (non-full node) ---

void insert_nonfull(BTreeNode *node, int key) {
    int i = node->n - 1;

    if (node->leaf) {
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->n++;
    } else {
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        if (node->children[i]->n == 2 * T - 1) {
            split_child(node, i);
            if (key > node->keys[i])
                i++;
        }
        insert_nonfull(node->children[i], key);
    }
}

void btree_insert(BTree *tree, int key) {
    BTreeNode *root = tree->root;

    if (root->n == 2 * T - 1) {
        BTreeNode *new_root = create_node(false);
        new_root->children[0] = root;
        split_child(new_root, 0);

        int i = (key > new_root->keys[0]) ? 1 : 0;
        insert_nonfull(new_root->children[i], key);

        tree->root = new_root;
    } else {
        insert_nonfull(root, key);
    }
}

// --- deletion helpers ---

int find_key(BTreeNode *node, int key) {
    int i = 0;
    while (i < node->n && node->keys[i] < key)
        i++;
    return i;
}

int get_predecessor(BTreeNode *node) {
    while (!node->leaf)
        node = node->children[node->n];
    return node->keys[node->n - 1];
}

int get_successor(BTreeNode *node) {
    while (!node->leaf)
        node = node->children[0];
    return node->keys[0];
}

void merge(BTreeNode *node, int idx) {
    BTreeNode *left = node->children[idx];
    BTreeNode *right = node->children[idx + 1];

    // pull separator key down into left
    left->keys[T - 1] = node->keys[idx];

    // copy keys from right to left
    for (int i = 0; i < right->n; i++)
        left->keys[i + T] = right->keys[i];

    // copy children from right to left
    if (!left->leaf) {
        for (int i = 0; i <= right->n; i++)
            left->children[i + T] = right->children[i];
    }

    // shift keys and children in parent
    for (int i = idx; i < node->n - 1; i++) {
        node->keys[i] = node->keys[i + 1];
        node->children[i + 1] = node->children[i + 2];
    }

    left->n = 2 * T - 1;
    node->n--;

    free(right);
}

void borrow_from_prev(BTreeNode *node, int idx) {
    BTreeNode *child = node->children[idx];
    BTreeNode *sibling = node->children[idx - 1];

    // shift keys in child to make room
    for (int i = child->n - 1; i >= 0; i--)
        child->keys[i + 1] = child->keys[i];
    if (!child->leaf) {
        for (int i = child->n; i >= 0; i--)
            child->children[i + 1] = child->children[i];
    }

    child->keys[0] = node->keys[idx - 1];
    if (!child->leaf)
        child->children[0] = sibling->children[sibling->n];

    node->keys[idx - 1] = sibling->keys[sibling->n - 1];

    child->n++;
    sibling->n--;
}

void borrow_from_next(BTreeNode *node, int idx) {
    BTreeNode *child = node->children[idx];
    BTreeNode *sibling = node->children[idx + 1];

    child->keys[child->n] = node->keys[idx];
    if (!child->leaf)
        child->children[child->n + 1] = sibling->children[0];

    node->keys[idx] = sibling->keys[0];

    for (int i = 0; i < sibling->n - 1; i++)
        sibling->keys[i] = sibling->keys[i + 1];
    if (!sibling->leaf) {
        for (int i = 0; i < sibling->n; i++)
            sibling->children[i] = sibling->children[i + 1];
    }

    child->n++;
    sibling->n--;
}

void fill(BTreeNode *node, int idx) {
    if (idx > 0 && node->children[idx - 1]->n >= T)
        borrow_from_prev(node, idx);
    else if (idx < node->n && node->children[idx + 1]->n >= T)
        borrow_from_next(node, idx);
    else {
        if (idx < node->n)
            merge(node, idx);
        else
            merge(node, idx - 1);
    }
}

void btree_delete_from(BTreeNode *node, int key);

void btree_delete_from(BTreeNode *node, int key) {
    int idx = find_key(node, key);

    if (idx < node->n && node->keys[idx] == key) {
        if (node->leaf) {
            // case 1: key in leaf
            for (int i = idx; i < node->n - 1; i++)
                node->keys[i] = node->keys[i + 1];
            node->n--;
        } else {
            // case 2: key in internal node
            if (node->children[idx]->n >= T) {
                int pred = get_predecessor(node->children[idx]);
                node->keys[idx] = pred;
                btree_delete_from(node->children[idx], pred);
            } else if (node->children[idx + 1]->n >= T) {
                int succ = get_successor(node->children[idx + 1]);
                node->keys[idx] = succ;
                btree_delete_from(node->children[idx + 1], succ);
            } else {
                merge(node, idx);
                btree_delete_from(node->children[idx], key);
            }
        }
    } else {
        if (node->leaf) return;  // key not in tree

        bool last = (idx == node->n);
        if (node->children[idx]->n < T)
            fill(node, idx);
        if (last && idx > node->n)
            btree_delete_from(node->children[idx - 1], key);
        else
            btree_delete_from(node->children[idx], key);
    }
}

void btree_delete(BTree *tree, int key) {
    if (!tree->root) return;

    btree_delete_from(tree->root, key);

    if (tree->root->n == 0) {
        BTreeNode *old = tree->root;
        tree->root = tree->root->leaf ? create_node(true) : old->children[0];
        free(old);
    }
}

// --- print tree structure ---

void print_node(BTreeNode *node, int depth) {
    printf("%*s[", depth * 4, "");
    for (int i = 0; i < node->n; i++) {
        if (i > 0) printf(", ");
        printf("%d", node->keys[i]);
    }
    printf("]\n");

    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++)
            print_node(node->children[i], depth + 1);
    }
}

void btree_print(BTree *tree) {
    print_node(tree->root, 0);
}

// --- free ---

void free_node(BTreeNode *node) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++)
            free_node(node->children[i]);
    }
    free(node);
}

void btree_free(BTree *tree) {
    free_node(tree->root);
    free(tree);
}

// --- demo ---

int main(void) {
    // demo 1: insertion and traversal
    printf("=== Demo 1: insertion and traversal ===\n");
    BTree *tree = create_btree();
    int values[] = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 25, 35, 40, 15};
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++) {
        btree_insert(tree, values[i]);
        printf("After inserting %d:\n", values[i]);
        btree_print(tree);
        printf("\n");
    }

    printf("In-order traversal: ");
    btree_traverse(tree->root);
    printf("\n\n");

    // demo 2: search
    printf("=== Demo 2: search ===\n");
    int targets[] = {6, 15, 25, 99};
    for (int i = 0; i < 4; i++) {
        SearchResult r = btree_search(tree->root, targets[i]);
        if (r.node)
            printf("Search %d: FOUND at index %d in node [%d, ...]\n",
                   targets[i], r.index, r.node->keys[0]);
        else
            printf("Search %d: NOT FOUND\n", targets[i]);
    }
    printf("\n");

    // demo 3: deletion
    printf("=== Demo 3: deletion ===\n");
    int del[] = {6, 10, 30, 1};
    for (int i = 0; i < 4; i++) {
        printf("Delete %d:\n", del[i]);
        btree_delete(tree, del[i]);
        btree_print(tree);
        printf("Traversal: ");
        btree_traverse(tree->root);
        printf("\n\n");
    }

    // demo 4: build 2-3 tree (t=2 behavior shown with t=3 structure)
    printf("=== Demo 4: sequential insertion (worst case for BST) ===\n");
    BTree *seq = create_btree();
    for (int i = 1; i <= 20; i++)
        btree_insert(seq, i);
    printf("After inserting 1..20 sequentially:\n");
    btree_print(seq);
    printf("Traversal: ");
    btree_traverse(seq->root);
    printf("\n\n");

    // demo 5: height comparison
    printf("=== Demo 5: height for large n ===\n");
    BTree *big = create_btree();
    for (int i = 0; i < 10000; i++)
        btree_insert(big, i);

    int height = 0;
    BTreeNode *cur = big->root;
    while (!cur->leaf) {
        height++;
        cur = cur->children[0];
    }
    printf("B-tree (t=%d) with 10000 keys: height = %d\n", T, height);
    printf("BST balanced would have height ~%.0f\n", 14.0);  // log2(10000) ~ 13.3
    printf("Reduction factor: %.1fx fewer levels\n\n",
           14.0 / (height > 0 ? height : 1));

    // demo 6: search count
    printf("=== Demo 6: comparisons per search ===\n");
    int comparisons = 0;
    BTreeNode *node = big->root;
    int target = 7777;
    while (node) {
        int i = 0;
        while (i < node->n && target > node->keys[i]) {
            comparisons++;
            i++;
        }
        comparisons++;  // final comparison (== or >)
        if (i < node->n && target == node->keys[i]) {
            printf("Found %d after %d comparisons\n", target, comparisons);
            break;
        }
        if (node->leaf) {
            printf("Not found after %d comparisons\n", comparisons);
            break;
        }
        node = node->children[i];
    }

    btree_free(tree);
    btree_free(seq);
    btree_free(big);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -o btree_search btree_search.c
./btree_search
```

---

## Programa completo en Rust

```rust
// btree_search.rs — BTreeMap operations for search
use std::collections::BTreeMap;

fn main() {
    // demo 1: basic insertion and search
    println!("=== Demo 1: insertion and search ===");
    let mut map = BTreeMap::new();
    let values = [10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 25, 35, 40, 15];

    for &v in &values {
        map.insert(v, v * 10);  // key -> value
    }

    println!("Map: {:?}", map);
    println!("get(12): {:?}", map.get(&12));
    println!("get(99): {:?}", map.get(&99));
    println!("contains_key(7): {}", map.contains_key(&7));
    println!();

    // demo 2: range queries
    println!("=== Demo 2: range queries ===");
    print!("range(10..=25): ");
    for (&k, &v) in map.range(10..=25) {
        print!("({}, {}) ", k, v);
    }
    println!("\n");

    print!("range(..15): ");
    for (&k, _) in map.range(..15) {
        print!("{} ", k);
    }
    println!("\n");

    // demo 3: floor and ceiling (no direct method, use range)
    println!("=== Demo 3: floor and ceiling ===");
    let target = 8;

    // floor: largest key <= target
    let floor = map.range(..=target).next_back();
    println!("floor({}) = {:?}", target, floor.map(|(&k, _)| k));

    // ceiling: smallest key >= target
    let ceiling = map.range(target..).next();
    println!("ceiling({}) = {:?}", target, ceiling.map(|(&k, _)| k));
    println!();

    // demo 4: ordered iteration
    println!("=== Demo 4: ordered iteration ===");
    print!("Ascending:  ");
    for (&k, _) in &map {
        print!("{} ", k);
    }
    println!();

    print!("Descending: ");
    for (&k, _) in map.iter().rev() {
        print!("{} ", k);
    }
    println!("\n");

    // demo 5: min, max, successor, predecessor
    println!("=== Demo 5: min/max/successor/predecessor ===");
    println!("min: {:?}", map.iter().next().map(|(&k, _)| k));
    println!("max: {:?}", map.iter().next_back().map(|(&k, _)| k));

    // successor of 12: smallest key > 12
    let succ = map.range((std::ops::Bound::Excluded(12), std::ops::Bound::Unbounded)).next();
    println!("successor(12): {:?}", succ.map(|(&k, _)| k));

    // predecessor of 12: largest key < 12
    let pred = map.range(..12).next_back();
    println!("predecessor(12): {:?}", pred.map(|(&k, _)| k));
    println!();

    // demo 6: deletion
    println!("=== Demo 6: deletion ===");
    let removed = map.remove(&10);
    println!("remove(10): {:?}", removed);
    println!("remove(99): {:?}", map.remove(&99));
    println!("Map after deletions: {:?}", map);
    println!();

    // demo 7: sequential insertion (B-tree stays balanced)
    println!("=== Demo 7: sequential insertion ===");
    let mut seq_map = BTreeMap::new();
    for i in 1..=10000 {
        seq_map.insert(i, i);
    }
    println!("Inserted 1..=10000 sequentially");
    println!("len = {}", seq_map.len());
    println!("min = {:?}", seq_map.keys().next());
    println!("max = {:?}", seq_map.keys().next_back());

    // B-tree guarantees O(log n) regardless of insertion order
    // Unlike BST which degenerates to O(n)
    println!("Search 7777: {:?}", seq_map.get(&7777));
    println!();

    // demo 8: rank and select via iterators
    println!("=== Demo 8: rank and select ===");
    let small: BTreeMap<i32, &str> = [
        (10, "a"), (20, "b"), (30, "c"), (40, "d"), (50, "e"),
    ].into_iter().collect();

    // rank of key 30: how many keys < 30?
    let rank = small.range(..30).count();
    println!("rank(30) = {} (0-indexed)", rank);

    // select k-th element (0-indexed)
    let k = 2;
    let kth = small.iter().nth(k);
    println!("select({}) = {:?}", k, kth.map(|(&key, _)| key));
    println!();

    // demo 9: BTreeSet for membership queries
    println!("=== Demo 9: BTreeSet ===");
    use std::collections::BTreeSet;

    let set: BTreeSet<i32> = values.iter().copied().collect();
    println!("set: {:?}", set);
    println!("contains(12): {}", set.contains(&12));
    println!("contains(99): {}", set.contains(&99));

    // set operations
    let other: BTreeSet<i32> = [5, 10, 50, 60].into_iter().collect();
    let inter: Vec<_> = set.intersection(&other).collect();
    let diff: Vec<_> = set.difference(&other).collect();
    println!("intersection with {{5,10,50,60}}: {:?}", inter);
    println!("difference with {{5,10,50,60}}: {:?}", diff);
    println!();

    // demo 10: entry API
    println!("=== Demo 10: entry API ===");
    let mut counts: BTreeMap<&str, i32> = BTreeMap::new();
    let words = ["hello", "world", "hello", "rust", "hello", "world"];

    for word in &words {
        *counts.entry(word).or_insert(0) += 1;
    }
    println!("Word counts: {:?}", counts);
}
```

Compilar y ejecutar:

```bash
rustc btree_search.rs && ./btree_search
```

---

## Errores frecuentes

1. **Off-by-one en split**: la mediana es `keys[T-1]`, no `keys[T]`.
   Copiar las claves mayores desde índice $T$, no $T-1$.
2. **Olvidar repartir los hijos** durante el split: las claves se
   copian correctamente pero los punteros a hijos del nodo original
   no se transfieren al nuevo nodo.
3. **No manejar el caso raíz** en inserción: si la raíz está llena,
   se crea una nueva raíz antes del split. Sin esto el árbol
   corrompe su estructura.
4. **Confundir orden y grado mínimo**: un "B-tree de orden 5"
   (Knuth) tiene máximo 4 claves, mientras que un "B-tree de grado
   mínimo 5" (CLRS) tiene máximo 9 claves. Verificar qué
   convención usa cada referencia.
5. **Eliminación sin fix previo**: al descender para eliminar, no
   verificar que el hijo tenga al menos $t$ claves antes de
   descender. Esto viola la invariante y puede dejar nodos vacíos.

---

## Ejercicios

Todos los ejercicios se resuelven en C salvo que se indique lo
contrario.

### Ejercicio 1 — Traza de inserción

Dibuja el B-tree de $t = 2$ (2-3 tree) paso a paso al insertar la
secuencia $\{5, 15, 25, 35, 45, 55, 20, 10\}$. Indica en qué
inserciones ocurre un split y cuándo crece la altura.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos splits ocurren en total? ¿Cuál es la altura final?
¿En qué inserción crece la altura por segunda vez?

</details>

### Ejercicio 2 — Traza de eliminación

Partiendo del B-tree de $t = 2$ con claves
$\{1, 3, 5, 7, 10, 12, 15, 18, 20\}$ (usa la estructura que
resulte de insertarlas en ese orden), elimina en secuencia:
$12, 10, 5, 3$. Dibuja el árbol después de cada eliminación e
indica si ocurre merge o redistribución.

<details><summary>Predice antes de ejecutar</summary>

¿En cuál eliminación ocurre el primer merge? ¿La altura del
árbol decrece en algún momento?

</details>

### Ejercicio 3 — Cálculo de $t$ óptimo

Dado un sistema con:
- Página de disco: 8192 bytes
- Clave: `int64_t` (8 bytes)
- Valor asociado: 64 bytes
- Puntero a hijo (offset en archivo): 8 bytes
- Overhead por nodo: 24 bytes (número de claves, flag hoja, padding)

Calcula el $t$ máximo que cabe en una página. ¿Cuál es la altura
máxima para $10^9$ registros con ese $t$?

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas claves caben por nodo? ¿La altura será 2, 3, 4 o 5?

</details>

### Ejercicio 4 — Búsqueda con contador

Modifica la función `btree_search` para que cuente el número de
comparaciones realizadas (tanto inter-nodo como intra-nodo).
Inserta $10^4$ claves aleatorias y reporta el promedio y máximo de
comparaciones por búsqueda.

<details><summary>Predice antes de ejecutar</summary>

Con $t = 3$ y $n = 10^4$, ¿cuántas comparaciones esperarías en
promedio? Pista: la altura es ~$\log_3(5000) \approx 7.7$, y
en cada nodo se comparan ~$t$ claves.

</details>

### Ejercicio 5 — Búsqueda binaria intra-nodo

Implementa una versión de `btree_search` que use búsqueda binaria
dentro de cada nodo en vez de lineal. Compara el número de
comparaciones con la versión lineal para $t = 50$ y $n = 10^5$.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas comparaciones ahorra la búsqueda binaria por nivel?
Para $t = 50$: lineal ~50 comparaciones/nodo, binaria ~6.
¿Es significativa la diferencia en tiempo real?

</details>

### Ejercicio 6 — Inserción secuencial vs aleatoria

Inserta las claves $1, 2, 3, \ldots, 1000$ (secuencial) y luego
1000 claves aleatorias en dos B-trees de $t = 3$. Compara:
- Altura final de cada árbol.
- Ocupación promedio de los nodos (claves/nodo vs máximo).

<details><summary>Predice antes de ejecutar</summary>

A diferencia de BST, ¿la inserción secuencial produce peor
estructura en un B-tree? ¿Cuál tendrá mayor ocupación?

</details>

### Ejercicio 7 — Rango en B-tree

Implementa `btree_range(BTreeNode *root, int lo, int hi)` que
imprima todas las claves $k$ tales que $lo \leq k \leq hi$.
La complejidad debe ser $O(\log_t n + k)$ donde $k$ es el número
de claves en el rango.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos nodos se visitan para un rango que contiene $k$ claves?
¿Es proporcional a $k$ o a algo más?

</details>

### Ejercicio 8 — B-tree en disco (simulado)

Simula el almacenamiento en disco: cada nodo se serializa a un
`char[4096]` (una "página"). Implementa:
- `write_node(FILE *f, int page_id, BTreeNode *node)` — escribe
  el nodo en la posición `page_id * 4096` del archivo.
- `read_node(FILE *f, int page_id)` — lee un nodo del archivo.
- `disk_search(FILE *f, int root_page, int key)` — busca
  contando el número de lecturas de página.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas lecturas de página necesitará para buscar en un árbol
de $10^4$ claves con $t$ calculado para 4096 bytes?

</details>

### Ejercicio 9 — BTreeMap vs HashMap en Rust

Compara `BTreeMap` y `HashMap` para $10^6$ inserciones y búsquedas
de enteros aleatorios. Mide el tiempo de:
- Inserción de todas las claves.
- Búsqueda de 1000 claves aleatorias.
- Iteración ordenada de todas las claves.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál será más rápido en inserción? ¿Y en búsqueda individual?
¿Y en iteración ordenada? ¿Por qué?

</details>

### Ejercicio 10 — Verificador de invariantes

Implementa `btree_verify(BTree *tree)` que verifique todas las
invariantes del B-tree:
1. Cada nodo (excepto raíz) tiene entre $t-1$ y $2t-1$ claves.
2. Las claves están ordenadas dentro de cada nodo.
3. Cada nodo interno con $k$ claves tiene $k+1$ hijos no nulos.
4. Los rangos de los subárboles son consistentes con las claves.
5. Todas las hojas están al mismo nivel.

Pruébalo insertando y eliminando claves aleatoriamente.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál invariante es más probable que se viole si hay un bug en
`split_child`? ¿Y si hay un bug en `merge`?

</details>
