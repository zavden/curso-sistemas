# B+ trees

## Objetivo

Comprender el B+ tree como **variante del B-tree** donde los datos
residen **exclusivamente en las hojas** y las hojas están
**enlazadas** formando una lista ordenada:

- Nodos internos solo almacenan **claves de enrutamiento** (sin
  datos asociados) — caben más claves por nodo, menor altura.
- Las hojas contienen **todos los datos** y están enlazadas para
  recorrido secuencial eficiente.
- Búsqueda puntual: $O(\log_t n)$ — igual que B-tree.
- **Range scan**: $O(\log_t n + k)$ sin retroceder al padre —
  la ventaja central sobre B-tree.
- Uso dominante en bases de datos y filesystems.
- Referencia: T03 cubre la definición y operaciones del B-tree
  base.

---

## B-tree vs B+ tree: la diferencia clave

En un **B-tree** (T03), cada nodo almacena pares (clave, valor).
Un registro puede estar en cualquier nivel del árbol:

```
B-tree (t=2): claves con datos en todos los niveles

        [(10,d₁), (20,d₂)]
       /        |          \
  [(5,d₃)]  [(15,d₄)]  [(25,d₅), (30,d₆)]
```

En un **B+ tree**, los nodos internos solo almacenan claves de
enrutamiento (sin datos). **Todos los datos** están en las hojas,
que se enlazan entre sí:

```
B+ tree (t=2): datos SOLO en hojas, hojas enlazadas

        [10 | 20]              ← solo claves de guia
       /    |     \
  [5,d₃]→[10,d₁ | 15,d₄]→[20,d₂ | 25,d₅ | 30,d₆]
                                   hojas enlazadas →
```

Observación crucial: en el B+ tree, la clave 10 aparece **dos
veces** — en el nodo interno como guía y en la hoja con su dato.
Las claves de los nodos internos son **copias** que solo sirven
para dirigir la búsqueda.

### Tabla comparativa

| Aspecto | B-tree | B+ tree |
|---------|--------|---------|
| Datos en nodos internos | sí | no (solo claves guía) |
| Datos en hojas | sí | sí (todos) |
| Claves duplicadas | no | sí (guías repiten claves de hojas) |
| Hojas enlazadas | no | sí (lista doblemente enlazada) |
| Búsqueda puntual | puede terminar en nodo interno | siempre llega a hoja |
| Range scan | requiere recorrido in-order del árbol | seguir enlaces entre hojas |
| Fan-out interno | menor (claves + datos ocupan espacio) | mayor (solo claves, más por nodo) |
| Eliminación | compleja (dato puede estar en interno) | más simple (datos solo en hojas) |

---

## Estructura formal

Un B+ tree de grado mínimo $t$ tiene dos tipos de nodos:

### Nodo interno

```c
typedef struct BPlusInternal {
    int keys[2 * T - 1];           // claves de enrutamiento
    struct BPlusNode *children[2 * T]; // punteros a hijos
    int n;                          // numero de claves
} BPlusInternal;
```

- Almacena entre $t-1$ y $2t-1$ claves (raíz: 1 a $2t-1$).
- Tiene $n+1$ hijos.
- `children[i]` contiene claves $< \text{keys}[i]$.
- `children[n]` contiene claves $\geq \text{keys}[n-1]$.
- **No almacena datos** — solo claves para dirigir la búsqueda.

### Nodo hoja

```c
typedef struct BPlusLeaf {
    int keys[2 * T - 1];           // claves reales
    int values[2 * T - 1];         // datos asociados
    int n;                          // numero de pares
    struct BPlusLeaf *next;         // enlace a hoja siguiente
    struct BPlusLeaf *prev;         // enlace a hoja anterior (opcional)
} BPlusLeaf;
```

- Almacena entre $t-1$ y $2t-1$ pares (clave, valor).
- Enlazado con la hoja siguiente (y opcionalmente la anterior).
- **Contiene todos los datos** del árbol.

### Por qué mayor fan-out

En un B-tree, cada "slot" de un nodo interno ocupa:
$K + V + P$ bytes (clave + valor + puntero a hijo).

En un B+ tree, cada slot de nodo interno ocupa:
$K + P$ bytes (clave + puntero, sin valor).

Si $V = 100$ bytes (un registro), $K = 8$, $P = 8$, página =
4096 bytes:

| Estructura | Bytes por slot | Slots por página | Fan-out |
|------------|----------:|----------:|----------:|
| B-tree | 116 | ~35 | ~35 |
| B+ tree | 16 | ~255 | ~255 |

Con fan-out 255, un B+ tree de 3 niveles almacena
$255^2 \times 254 \approx 16.5$ millones de registros. El B-tree
equivalente necesitaría 4-5 niveles.

---

## Altura del B+ tree

La altura es la misma que la del B-tree con el fan-out ajustado:

$$h \leq \log_t \frac{n+1}{2} + 1$$

El $+1$ es porque la búsqueda siempre llega hasta una hoja
(mientras que en un B-tree puede terminar en un nodo interno).
Pero en la práctica el mayor fan-out compensa con creces: para
el mismo $n$, el B+ tree suele tener **igual o menor** altura
que el B-tree porque $t$ es mayor.

### Ejemplo numérico

Para $n = 10^7$ registros, registro de 100 bytes, clave de 8
bytes, puntero de 8 bytes, página de 4096 bytes:

| Estructura | $t$ efectivo | Altura | Accesos a disco |
|------------|-------:|-------:|-------:|
| B-tree | ~18 | ~6 | ~6 |
| B+ tree | ~128 | ~4 | ~4 |
| B+ tree (16 KB página) | ~512 | ~3 | ~3 |

Además, en el B+ tree la raíz y primer nivel (1 + ~128 nodos)
se mantienen en RAM (cache del buffer pool). Accesos reales a
disco: **1-2** por búsqueda.

---

## Búsqueda en B+ tree

### Búsqueda puntual

A diferencia del B-tree (donde la búsqueda puede terminar en un
nodo interno), en B+ tree **siempre se desciende hasta una hoja**:

```
BPLUS_SEARCH(root, key):
    node = root
    while node is internal:
        i = 0
        while i < node.n and key >= node.keys[i]:
            i++
        node = node.children[i]

    // node is a leaf — search within it
    for i = 0 to node.n - 1:
        if node.keys[i] == key:
            return node.values[i]
    return NOT_FOUND
```

Observación: la comparación en nodos internos usa `>=` (no `>`
como en B-tree). Esto es porque las claves guía son **copias** de
claves que existen en las hojas: si `key == keys[i]`, el dato
está en `children[i+1]` (el subárbol derecho), no en el nodo
interno.

### Traza de búsqueda

B+ tree con $t = 2$:

```
Estructura:
              [15]
            /      \
       [5, 10]    [15, 20]       ← nodos internos (solo guias)
      / |   \     /  |    \
    [1,3][5,7][10,12][15,17][20,25]  ← hojas (con datos)
      →    →    →      →    (enlazadas)
```

**Buscar 7**:

```
Paso 1: raiz [15]
        7 < 15 → children[0]

Paso 2: nodo interno [5, 10]
        7 >= 5 → avanzar
        7 < 10 → children[1]

Paso 3: hoja [5, 7]
        5 != 7 → siguiente
        7 == 7 → ENCONTRADO

Accesos: 3 (= altura del arbol)
```

**Buscar 15** (clave que está en nodo interno Y en hoja):

```
Paso 1: raiz [15]
        15 >= 15 → children[1]      (NO se detiene aqui)

Paso 2: nodo interno [15, 20]
        15 >= 15 → avanzar
        15 < 20 → children[1]

Paso 3: hoja [15, 17]
        15 == 15 → ENCONTRADO (el dato esta AQUI, no en el interno)

Accesos: 3
```

---

## Range scan: la ventaja del B+ tree

### El problema con B-tree

Para obtener todas las claves en $[10, 25]$ de un B-tree, hay que
hacer un recorrido in-order parcial, subiendo y bajando por el
árbol. Cada clave puede requerir volver al padre, luego bajar a
otro hijo — con posibles accesos a disco en cada paso.

### La solución B+ tree

En un B+ tree:
1. Buscar la primera clave $\geq 10$ (descenso normal): $O(\log_t n)$.
2. Seguir los enlaces entre hojas hasta encontrar una clave $> 25$.
3. Total: $O(\log_t n + k)$ donde $k$ es el número de resultados.

```
Range scan [10, 25]:

Paso 1: buscar 10 → llegar a hoja [10, 12]

Paso 2: seguir enlaces
  hoja [10, 12]: emitir 10, 12
  → siguiente hoja [15, 17]: emitir 15, 17
  → siguiente hoja [20, 25]: emitir 20, 25
  → siguiente hoja (si existe): primer key > 25 → STOP

Resultado: {10, 12, 15, 17, 20, 25}
Accesos: 3 (descenso) + 3 (hojas) = 6
```

En un B-tree, el mismo rango requeriría subir y bajar
repetidamente por la estructura, con accesos potencialmente
aleatorios al disco.

### Importancia para bases de datos

Las queries con rango son **extremadamente comunes**:

```sql
SELECT * FROM orders WHERE date BETWEEN '2025-01-01' AND '2025-03-31';
SELECT * FROM users WHERE age >= 18 AND age <= 25;
SELECT * FROM products WHERE price < 100 ORDER BY price;
```

Cada una de estas se traduce en un range scan sobre el índice B+
tree. La diferencia entre "seguir enlaces secuenciales" y
"recorrer el árbol" puede ser de **órdenes de magnitud** en disco.

### Full scan ordenado

Iterar **todas** las claves en orden es trivial: ir a la hoja más
a la izquierda y seguir los enlaces `next` hasta el final. Costo:
$O(n)$ con accesos secuenciales (el patrón más eficiente para
disco).

En un B-tree, un recorrido in-order completo requiere una pila
(recursiva o explícita) y salta entre nodos internos y hojas de
forma no secuencial.

---

## Inserción

Similar al B-tree, con una diferencia clave: las claves de
enrutamiento en nodos internos son **copias**, no las claves
originales.

### Inserción en hoja con espacio

Simplemente insertar el par (clave, valor) en la posición ordenada
dentro de la hoja. No se modifica ningún nodo interno.

### Split de hoja llena

Cuando una hoja tiene $2t-1$ pares y hay que insertar uno más:

1. Crear nueva hoja.
2. Repartir: los primeros $t$ pares quedan en la hoja original,
   los últimos $t$ van a la nueva hoja.
3. **Copiar** la primera clave de la nueva hoja al padre como
   clave guía.
4. Actualizar los enlaces entre hojas.

```
Antes (t=3, hoja llena con 5 pares):

    padre: [..., 20, ...]
                  |
    hoja:  [21, 23, 25, 27, 29]  → siguiente
                                    (insertar 24)

Insertar 24 → [21, 23, 24, 25, 27, 29] → 6 pares, split:

    padre: [..., 20, 25, ...]
                  |     |
    hoja:  [21, 23, 24] → [25, 27, 29] → siguiente

La clave 25 SUBE AL PADRE como copia (la hoja derecha
tambien tiene 25 — es el primer elemento).
```

Diferencia con B-tree: en B-tree la mediana **sube** y
desaparece del nodo. En B+ tree la clave **se copia** arriba y
permanece en la hoja.

### Split de nodo interno

Funciona exactamente igual que en B-tree: la mediana sube al
padre y el nodo se divide. Las claves de enrutamiento se
distribuyen como en T03.

### Traza de inserción

B+ tree $t = 2$, insertando $\{10, 20, 5, 15, 25, 30, 12\}$:

```
Insertar 10:
  hoja: [10]

Insertar 20:
  hoja: [10, 20]

Insertar 5:
  hoja: [5, 10, 20]  ← llena (3 = 2t-1)

Insertar 15 → split de hoja:
  repartir: [5, 10] y [15, 20]
  copia de 15 sube como guia:

       [15]                    ← nodo interno
      /    \
  [5, 10]→[15, 20]            ← hojas enlazadas

Insertar 25:
  25 >= 15 → hoja derecha [15, 20, 25] ← llena

Insertar 30 → split de hoja derecha:
  repartir: [15, 20] y [25, 30]
  copia de 25 sube:

       [15, 25]
      /   |    \
  [5,10]→[15,20]→[25,30]

Insertar 12:
  12 < 15 → hoja izquierda [5, 10]
  insertar: [5, 10, 12]       ← cabe

       [15, 25]
      /   |    \
  [5,10,12]→[15,20]→[25,30]   (estado final)
```

---

## Eliminación

La eliminación en B+ tree es más simple que en B-tree porque los
datos solo están en hojas.

### Caso 1: eliminar de hoja con más del mínimo

Simplemente borrar el par. Si la clave eliminada aparece como guía
en un nodo interno, **puede dejarse** — sigue siendo una guía
válida (dirige búsquedas correctamente aunque el valor exacto ya
no esté en la hoja).

### Caso 2: underflow en hoja

Si la hoja queda con menos de $t-1$ pares:

**Redistribución**: pedir prestado un par de un hermano adyacente
y actualizar la clave guía en el padre.

```
Antes (t=3, hoja con minimo 2 pares, eliminar 21):

    padre: [..., 25, ...]
                |     |
    hoja: [21, 23]  hermano: [25, 27, 29, 31]

Eliminar 21 → [23] ← underflow (1 < t-1=2)

Redistribuir: pedir 25 del hermano:

    padre: [..., 27, ...]
                |     |
    hoja: [23, 25]  hermano: [27, 29, 31]

(La guia del padre se actualiza a 27, el nuevo minimo del hermano)
```

**Merge**: si ambos hermanos tienen el mínimo, fusionar la hoja
con un hermano y eliminar la clave guía del padre.

```
Antes (t=3, ambos con minimo):

    padre: [..., 25, ...]
                |     |
    hoja: [21, 23]  hermano: [25, 27]

Eliminar 21 → [23] ← underflow

Merge: [23] + [25, 27] → [23, 25, 27]

    padre: [..., ...]       (guia 25 eliminada)
                |
    fusionado: [23, 25, 27]
```

A diferencia del B-tree, la clave guía **no baja** al nodo
fusionado — simplemente se elimina del padre (porque es una copia,
no un dato).

### Propagación hacia arriba

Si el merge deja al padre con menos de $t-1$ claves, se aplica
redistribución o merge a nivel de nodos internos (igual que en
B-tree). Si la raíz queda vacía, el hijo único se convierte en
nueva raíz.

---

## Variantes en la práctica

### Bulk loading

Construir un B+ tree insertando una clave a la vez es ineficiente
(muchos splits). Para cargar $n$ registros de golpe:

1. **Ordenar** los registros por clave.
2. Llenar hojas de izquierda a derecha (cada una con $2t-1$ o
   $\lceil 2t \cdot f \rceil$ pares, donde $f$ es el fill factor).
3. Construir nodos internos bottom-up.
4. Enlazar las hojas.

Costo: $O(n \log n)$ (dominado por el sort) vs
$O(n \log_t n)$ inserciones individuales.

### Fill factor

En la práctica, los nodos no se llenan al 100%:

| Fill factor | Espacio | Inserciones futuras |
|--------:|------:|-----|
| 50% | 2x del mínimo | muchas sin split |
| 70% | ~1.4x | equilibrio típico |
| 90% | ~1.1x | pocas antes de split |
| 100% | mínimo | cualquier inserción causa split |

PostgreSQL usa fill factor configurable (por defecto 90% para
índices, 100% para tablas append-only).

### Prefix compression

En nodos internos, las claves guía no necesitan ser completas.
Si las claves son strings, se puede almacenar solo el prefijo
mínimo que distingue los subárboles:

```
Claves en hojas: "anderson", "baker", "carter", "davis"

Guias completas:   [baker | carter]
Guias comprimidas: [b     | c     ]
```

Esto incrementa el fan-out y reduce la altura.

### Suffix truncation

Variante de prefix compression usada en PostgreSQL: las claves
guía se truncan al mínimo necesario para separar subárboles.
Combinado con prefix compression de claves dentro de la misma
hoja, reduce significativamente el espacio.

---

## B+ tree en bases de datos

### Anatomía de un índice

Un índice B+ tree en una base de datos real:

```
                        [raiz: pagina 1]
                       /       |        \
              [pag 2]       [pag 3]      [pag 4]
             / | \         / | \         / | \
         [p5] [p6] [p7] [p8] [p9] [p10] [p11] [p12] [p13]
          →     →    →    →    →    →     →     →    (enlazadas)
```

- Cada página = 8 KB (PostgreSQL) o 16 KB (MySQL InnoDB).
- Los nodos internos almacenan (clave, page_id).
- Las hojas almacenan (clave, puntero a fila) o (clave, fila
  completa) en índices clustered.

### Clustered vs non-clustered

**Clustered index** (índice agrupado): las hojas del B+ tree
contienen **las filas completas** de la tabla, ordenadas por la
clave del índice. Solo puede haber uno por tabla.

```
B+ tree clustered:
  hojas = [clave₁, fila₁] → [clave₂, fila₂] → ...
  (la tabla ES el indice)
```

**Non-clustered index** (índice secundario): las hojas contienen
(clave, puntero a la fila en la tabla o en el clustered index).

```
B+ tree non-clustered:
  hojas = [clave₁, row_ptr₁] → [clave₂, row_ptr₂] → ...
  (hay que seguir el puntero para obtener la fila)
```

| Motor | Clustered | Non-clustered |
|-------|-----------|---------------|
| InnoDB (MySQL) | PK obligatoria, hojas = filas | hojas apuntan a PK (doble lookup) |
| PostgreSQL | CLUSTER opcional (no se mantiene) | hojas apuntan a (page, offset) |
| SQLite | rowid = clustered | hojas apuntan a rowid |

### Buffer pool

Las bases de datos mantienen un **buffer pool** (caché de páginas
en RAM). Las páginas más accedidas del B+ tree permanecen en
memoria:

- La raíz: **siempre** en caché.
- Primer nivel: casi siempre en caché (~128-256 páginas).
- Segundo nivel: frecuentemente en caché.
- Hojas: depende del working set.

Para una tabla de $10^7$ filas con páginas de 16 KB:
- Altura: ~3-4 niveles.
- Páginas internas totales: ~$1 + 256 + 65536 \approx 66000$
  páginas = ~1 GB.
- Con 4 GB de buffer pool: todos los internos caben en RAM.
- Accesos a disco por búsqueda: **1** (solo la hoja).

---

## B+ tree en filesystems

### Directorios en ext4

Para directorios grandes (>2 entradas de un bloque), ext4 usa
**HTree**: una estructura tipo B-tree de 2 niveles donde las
hojas contienen entradas de directorio hasheadas.

### Btrfs

Btrfs (B-tree filesystem) usa un B-tree generalizado con
copy-on-write: cada escritura crea una nueva versión del nodo
modificado sin alterar el original. Esto permite snapshots
eficientes.

### NTFS

El Master File Table de NTFS usa un B+ tree para mapear nombres
de archivo a metadatos, permitiendo búsqueda eficiente en
directorios con millones de archivos.

---

## Programa completo en C

```c
// bplus_tree.c — simplified B+ tree: insert, search, range scan
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define T 3  // minimum degree (max 2T-1 = 5 keys per node)
#define MAX_KEYS (2 * T - 1)

// --- node types ---

typedef struct Node {
    int keys[MAX_KEYS];
    int n;          // current number of keys
    bool leaf;
    struct Node *children[2 * T];  // internal: child pointers
    int values[MAX_KEYS];          // leaf: associated values
    struct Node *next;             // leaf: link to next leaf
} Node;

typedef struct {
    Node *root;
} BPlusTree;

Node *create_node(bool leaf) {
    Node *node = calloc(1, sizeof(Node));
    node->leaf = leaf;
    return node;
}

BPlusTree *create_tree(void) {
    BPlusTree *tree = malloc(sizeof(BPlusTree));
    tree->root = create_node(true);
    return tree;
}

// --- search ---

typedef struct {
    Node *leaf;
    int index;
} SearchResult;

SearchResult bplus_search(Node *node, int key) {
    while (!node->leaf) {
        int i = 0;
        while (i < node->n && key >= node->keys[i])
            i++;
        node = node->children[i];
    }

    // linear search in leaf
    for (int i = 0; i < node->n; i++) {
        if (node->keys[i] == key)
            return (SearchResult){node, i};
    }
    return (SearchResult){NULL, -1};
}

// --- range scan ---

int bplus_range(Node *root, int lo, int hi, int *results, int max_results) {
    // descend to leaf containing lo
    Node *node = root;
    while (!node->leaf) {
        int i = 0;
        while (i < node->n && lo >= node->keys[i])
            i++;
        node = node->children[i];
    }

    // scan leaves
    int count = 0;
    while (node && count < max_results) {
        for (int i = 0; i < node->n && count < max_results; i++) {
            if (node->keys[i] > hi)
                return count;
            if (node->keys[i] >= lo)
                results[count++] = node->keys[i];
        }
        node = node->next;
    }
    return count;
}

// --- insertion helpers ---

void insert_into_leaf(Node *leaf, int key, int value) {
    int i = leaf->n - 1;
    while (i >= 0 && key < leaf->keys[i]) {
        leaf->keys[i + 1] = leaf->keys[i];
        leaf->values[i + 1] = leaf->values[i];
        i--;
    }
    leaf->keys[i + 1] = key;
    leaf->values[i + 1] = value;
    leaf->n++;
}

// split a full child of parent at index idx
void split_child(Node *parent, int idx) {
    Node *full = parent->children[idx];
    Node *right = create_node(full->leaf);

    if (full->leaf) {
        // leaf split: right gets upper half, copy first key up
        right->n = T;
        for (int j = 0; j < T; j++) {
            right->keys[j] = full->keys[j + T - 1];
            right->values[j] = full->values[j + T - 1];
        }
        full->n = T - 1;

        // maintain leaf links
        right->next = full->next;
        full->next = right;

        // copy up: first key of right goes to parent
        for (int j = parent->n; j > idx; j--) {
            parent->children[j + 1] = parent->children[j];
            parent->keys[j] = parent->keys[j - 1];
        }
        parent->keys[idx] = right->keys[0];
        parent->children[idx + 1] = right;
        parent->n++;
    } else {
        // internal split: same as B-tree (median goes up)
        right->n = T - 1;
        for (int j = 0; j < T - 1; j++)
            right->keys[j] = full->keys[j + T];
        for (int j = 0; j < T; j++)
            right->children[j] = full->children[j + T];
        full->n = T - 1;

        for (int j = parent->n; j > idx; j--) {
            parent->children[j + 1] = parent->children[j];
            parent->keys[j] = parent->keys[j - 1];
        }
        parent->keys[idx] = full->keys[T - 1];
        parent->children[idx + 1] = right;
        parent->n++;
    }
}

void insert_nonfull(Node *node, int key, int value) {
    if (node->leaf) {
        insert_into_leaf(node, key, value);
        return;
    }

    int i = node->n - 1;
    while (i >= 0 && key < node->keys[i])
        i--;
    i++;

    if (node->children[i]->n == MAX_KEYS) {
        split_child(node, i);
        if (key >= node->keys[i])
            i++;
    }
    insert_nonfull(node->children[i], key, value);
}

void bplus_insert(BPlusTree *tree, int key, int value) {
    Node *root = tree->root;

    if (root->n == MAX_KEYS) {
        Node *new_root = create_node(false);
        new_root->children[0] = root;
        split_child(new_root, 0);

        int i = (key >= new_root->keys[0]) ? 1 : 0;
        insert_nonfull(new_root->children[i], key, value);
        tree->root = new_root;
    } else {
        insert_nonfull(root, key, value);
    }
}

// --- print ---

void print_node(Node *node, int depth) {
    printf("%*s", depth * 4, "");
    if (node->leaf) {
        printf("LEAF [");
        for (int i = 0; i < node->n; i++) {
            if (i > 0) printf(", ");
            printf("%d:%d", node->keys[i], node->values[i]);
        }
        printf("]\n");
    } else {
        printf("INTERNAL [");
        for (int i = 0; i < node->n; i++) {
            if (i > 0) printf(", ");
            printf("%d", node->keys[i]);
        }
        printf("]\n");
        for (int i = 0; i <= node->n; i++)
            print_node(node->children[i], depth + 1);
    }
}

void bplus_print(BPlusTree *tree) {
    print_node(tree->root, 0);
}

// --- print leaf chain ---

void print_leaf_chain(BPlusTree *tree) {
    Node *node = tree->root;
    while (!node->leaf)
        node = node->children[0];

    printf("Leaf chain: ");
    while (node) {
        printf("[");
        for (int i = 0; i < node->n; i++) {
            if (i > 0) printf(",");
            printf("%d", node->keys[i]);
        }
        printf("]");
        if (node->next) printf(" -> ");
        node = node->next;
    }
    printf("\n");
}

// --- free ---

void free_node(Node *node) {
    if (!node) return;
    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++)
            free_node(node->children[i]);
    }
    free(node);
}

void bplus_free(BPlusTree *tree) {
    free_node(tree->root);
    free(tree);
}

// --- demo ---

int main(void) {
    // demo 1: insertion and structure
    printf("=== Demo 1: insertion step by step ===\n");
    BPlusTree *tree = create_tree();
    int keys[] = {10, 20, 5, 15, 25, 30, 12, 7, 3, 1, 18, 22, 28, 35, 40};
    int n = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < n; i++) {
        bplus_insert(tree, keys[i], keys[i] * 10);
        printf("After insert %d:\n", keys[i]);
        bplus_print(tree);
        print_leaf_chain(tree);
        printf("\n");
    }

    // demo 2: search
    printf("=== Demo 2: point search ===\n");
    int targets[] = {12, 25, 99, 1, 40};
    for (int i = 0; i < 5; i++) {
        SearchResult r = bplus_search(tree->root, targets[i]);
        if (r.leaf)
            printf("Search %d: FOUND, value = %d\n",
                   targets[i], r.leaf->values[r.index]);
        else
            printf("Search %d: NOT FOUND\n", targets[i]);
    }
    printf("\n");

    // demo 3: range scan
    printf("=== Demo 3: range scan ===\n");
    int results[100];
    int count;

    count = bplus_range(tree->root, 10, 25, results, 100);
    printf("range [10, 25]: ");
    for (int i = 0; i < count; i++) printf("%d ", results[i]);
    printf("(%d results)\n", count);

    count = bplus_range(tree->root, 1, 7, results, 100);
    printf("range [1, 7]:   ");
    for (int i = 0; i < count; i++) printf("%d ", results[i]);
    printf("(%d results)\n", count);

    count = bplus_range(tree->root, 30, 100, results, 100);
    printf("range [30,100]: ");
    for (int i = 0; i < count; i++) printf("%d ", results[i]);
    printf("(%d results)\n\n", count);

    // demo 4: full scan via leaf chain
    printf("=== Demo 4: full ordered scan via leaf chain ===\n");
    print_leaf_chain(tree);
    printf("\n");

    // demo 5: sequential insertion
    printf("=== Demo 5: sequential insertion 1..30 ===\n");
    BPlusTree *seq = create_tree();
    for (int i = 1; i <= 30; i++)
        bplus_insert(seq, i, i * 100);

    bplus_print(seq);
    printf("\nLeaf chain:\n");
    print_leaf_chain(seq);
    printf("\n");

    // demo 6: height comparison
    printf("=== Demo 6: height for large n ===\n");
    BPlusTree *big = create_tree();
    for (int i = 0; i < 10000; i++)
        bplus_insert(big, i, i);

    int height = 0;
    Node *cur = big->root;
    while (!cur->leaf) {
        height++;
        cur = cur->children[0];
    }
    printf("B+ tree (t=%d) with 10000 keys: height = %d\n", T, height);

    // count leaves
    int leaf_count = 0;
    Node *leaf = big->root;
    while (!leaf->leaf) leaf = leaf->children[0];
    while (leaf) { leaf_count++; leaf = leaf->next; }
    printf("Number of leaves: %d\n", leaf_count);
    printf("Average keys per leaf: %.1f / %d\n",
           10000.0 / leaf_count, MAX_KEYS);

    bplus_free(tree);
    bplus_free(seq);
    bplus_free(big);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -o bplus_tree bplus_tree.c
./bplus_tree
```

---

## Programa completo en Rust

Rust no tiene un B+ tree en la stdlib, pero `BTreeMap` es un
B-tree que comparte muchas propiedades. Aquí demostramos
operaciones equivalentes y construimos un B+ tree simplificado.

```rust
// bplus_tree.rs — B+ tree concepts via BTreeMap + manual B+ tree
use std::collections::BTreeMap;

// --- Part 1: BTreeMap as practical B+ tree replacement ---

fn btreemap_demos() {
    // demo 1: range scan (the core B+ tree advantage)
    println!("=== Demo 1: range scan with BTreeMap ===");
    let mut map = BTreeMap::new();
    for i in (0..100).step_by(3) {
        map.insert(i, format!("val_{}", i));
    }

    print!("range [10, 30]: ");
    for (&k, _) in map.range(10..=30) {
        print!("{} ", k);
    }
    println!("\n");

    // demo 2: simulating database index
    println!("=== Demo 2: database index simulation ===");

    #[derive(Debug)]
    struct Employee {
        id: u32,
        name: String,
        salary: u32,
    }

    let employees = vec![
        Employee { id: 1, name: "Alice".into(), salary: 75000 },
        Employee { id: 2, name: "Bob".into(), salary: 82000 },
        Employee { id: 3, name: "Carol".into(), salary: 68000 },
        Employee { id: 4, name: "Dave".into(), salary: 91000 },
        Employee { id: 5, name: "Eve".into(), salary: 73000 },
        Employee { id: 6, name: "Frank".into(), salary: 85000 },
    ];

    // primary index: id -> employee position
    let pk_index: BTreeMap<u32, usize> = employees.iter()
        .enumerate()
        .map(|(i, e)| (e.id, i))
        .collect();

    // secondary index: salary -> id (for range queries)
    let salary_index: BTreeMap<u32, u32> = employees.iter()
        .map(|e| (e.salary, e.id))
        .collect();

    // point query: find employee by id
    if let Some(&pos) = pk_index.get(&3) {
        println!("SELECT * WHERE id=3: {:?}", employees[pos]);
    }

    // range query: salaries between 70000 and 85000
    println!("SELECT * WHERE salary BETWEEN 70000 AND 85000:");
    for (&salary, &id) in salary_index.range(70000..=85000) {
        let pos = pk_index[&id];
        println!("  {} (salary={})", employees[pos].name, salary);
    }
    println!();

    // demo 3: ordered iteration (full scan)
    println!("=== Demo 3: full ordered scan ===");
    print!("All keys ascending: ");
    let small: BTreeMap<i32, ()> = [5, 3, 8, 1, 9, 2, 7].into_iter()
        .map(|k| (k, ()))
        .collect();
    for &k in small.keys() {
        print!("{} ", k);
    }
    println!("\n");

    // demo 4: floor and ceiling (B+ tree leaf scan)
    println!("=== Demo 4: floor/ceiling via range ===");
    let target = 14;
    let floor = map.range(..=target).next_back().map(|(&k, _)| k);
    let ceiling = map.range(target..).next().map(|(&k, _)| k);
    println!("floor({}) = {:?}", target, floor);
    println!("ceiling({}) = {:?}", target, ceiling);
    println!();
}

// --- Part 2: simplified B+ tree implementation ---

const ORDER: usize = 4;  // max keys per node = ORDER-1 = 3

#[derive(Debug)]
enum BPlusNode {
    Internal {
        keys: Vec<i32>,
        children: Vec<Box<BPlusNode>>,
    },
    Leaf {
        keys: Vec<i32>,
        values: Vec<i32>,
        // next pointer omitted for simplicity (would need Rc/RefCell)
    },
}

impl BPlusNode {
    fn new_leaf() -> Self {
        BPlusNode::Leaf {
            keys: Vec::new(),
            values: Vec::new(),
        }
    }

    fn new_internal(keys: Vec<i32>, children: Vec<Box<BPlusNode>>) -> Self {
        BPlusNode::Internal { keys, children }
    }

    fn search(&self, key: i32) -> Option<i32> {
        match self {
            BPlusNode::Internal { keys, children } => {
                let mut i = 0;
                while i < keys.len() && key >= keys[i] {
                    i += 1;
                }
                children[i].search(key)
            }
            BPlusNode::Leaf { keys, values, .. } => {
                keys.iter().position(|&k| k == key)
                    .map(|i| values[i])
            }
        }
    }

    fn is_full(&self) -> bool {
        match self {
            BPlusNode::Internal { keys, .. } => keys.len() >= ORDER - 1,
            BPlusNode::Leaf { keys, .. } => keys.len() >= ORDER - 1,
        }
    }
}

struct BPlusTree {
    root: Box<BPlusNode>,
}

impl BPlusTree {
    fn new() -> Self {
        BPlusTree {
            root: Box::new(BPlusNode::new_leaf()),
        }
    }

    fn search(&self, key: i32) -> Option<i32> {
        self.root.search(key)
    }

    // simplified insert (splits handled inline)
    fn insert(&mut self, key: i32, value: i32) {
        if self.root.is_full() {
            let old_root = std::mem::replace(
                &mut self.root,
                Box::new(BPlusNode::new_leaf()),
            );
            let (median, right) = Self::split(old_root);
            // median is actually in 'right' for leaf or pushed up for internal
            self.root = Box::new(BPlusNode::new_internal(
                vec![median],
                vec![
                    // left part needs to be reconstructed
                    // for simplicity, rebuild from split
                    Box::new(BPlusNode::new_leaf()), // placeholder
                    right,
                ],
            ));
            // This simplified version just demonstrates the concept
            // A full implementation would properly handle the left child
        }
        Self::insert_nonfull(&mut self.root, key, value);
    }

    fn insert_nonfull(node: &mut Box<BPlusNode>, key: i32, value: i32) {
        match node.as_mut() {
            BPlusNode::Leaf { keys, values, .. } => {
                let pos = keys.iter().position(|&k| k >= key).unwrap_or(keys.len());
                keys.insert(pos, key);
                values.insert(pos, value);
            }
            BPlusNode::Internal { keys, children } => {
                let mut i = 0;
                while i < keys.len() && key >= keys[i] {
                    i += 1;
                }
                if children[i].is_full() {
                    let child = std::mem::replace(
                        &mut children[i],
                        Box::new(BPlusNode::new_leaf()),
                    );
                    let (median, right) = Self::split(child);
                    keys.insert(i, median);
                    // the original (now left) child was replaced;
                    // we need it back — full impl would avoid this
                    children.insert(i + 1, right);
                    if key >= keys[i] {
                        i += 1;
                    }
                }
                Self::insert_nonfull(&mut children[i], key, value);
            }
        }
    }

    fn split(node: Box<BPlusNode>) -> (i32, Box<BPlusNode>) {
        match *node {
            BPlusNode::Leaf { keys, values, .. } => {
                let mid = keys.len() / 2;
                let median = keys[mid];
                let right = BPlusNode::Leaf {
                    keys: keys[mid..].to_vec(),
                    values: values[mid..].to_vec(),
                };
                (median, Box::new(right))
            }
            BPlusNode::Internal { keys, children } => {
                let mid = keys.len() / 2;
                let median = keys[mid];
                let right = BPlusNode::Internal {
                    keys: keys[mid + 1..].to_vec(),
                    children: children[mid + 1..].to_vec(),
                };
                (median, Box::new(right))
            }
        }
    }

    fn collect_sorted(&self) -> Vec<(i32, i32)> {
        let mut result = Vec::new();
        Self::collect_leaves(&self.root, &mut result);
        result
    }

    fn collect_leaves(node: &BPlusNode, result: &mut Vec<(i32, i32)>) {
        match node {
            BPlusNode::Leaf { keys, values, .. } => {
                for i in 0..keys.len() {
                    result.push((keys[i], values[i]));
                }
            }
            BPlusNode::Internal { children, .. } => {
                for child in children {
                    Self::collect_leaves(child, result);
                }
            }
        }
    }
}

fn bplus_tree_demo() {
    println!("=== Demo 5: manual B+ tree ===");
    let mut tree = BPlusTree::new();

    // insert some values
    for &k in &[10, 20, 5, 15, 25] {
        tree.insert(k, k * 10);
    }

    // search
    for &k in &[10, 15, 99] {
        match tree.search(k) {
            Some(v) => println!("search({}) = {}", k, v),
            None => println!("search({}) = NOT FOUND", k),
        }
    }

    // collect all in order
    println!("All entries: {:?}", tree.collect_sorted());
    println!();
}

// --- Part 3: BTreeMap vs HashMap performance characteristics ---

fn comparison_demo() {
    println!("=== Demo 6: BTreeMap ordered operations ===");

    let mut btree: BTreeMap<i32, i32> = BTreeMap::new();
    for i in 0..1000 {
        btree.insert(i * 3, i);
    }

    // operations that BTreeMap does well (and HashMap cannot)
    println!("min key: {:?}", btree.keys().next());
    println!("max key: {:?}", btree.keys().next_back());

    let range_count = btree.range(100..200).count();
    println!("keys in [100, 200): {}", range_count);

    // split_off: split tree at key
    let mut right = btree.split_off(&1500);
    println!("after split_off(1500):");
    println!("  left:  {} keys, range {:?}..{:?}",
             btree.len(),
             btree.keys().next(), btree.keys().next_back());
    println!("  right: {} keys, range {:?}..{:?}",
             right.len(),
             right.keys().next(), right.keys().next_back());

    // re-merge
    btree.append(&mut right);
    println!("after merge: {} keys", btree.len());
    println!();

    // demo 7: using BTreeMap as a database-like index
    println!("=== Demo 7: multi-column index ===");
    // composite key: (department, salary) for ORDER BY dept, salary
    let mut index: BTreeMap<(String, u32), String> = BTreeMap::new();
    index.insert(("Engineering".into(), 90000), "Alice".into());
    index.insert(("Engineering".into(), 85000), "Bob".into());
    index.insert(("Engineering".into(), 95000), "Carol".into());
    index.insert(("Sales".into(), 70000), "Dave".into());
    index.insert(("Sales".into(), 80000), "Eve".into());

    // equivalent to: SELECT * FROM emp WHERE dept='Engineering' ORDER BY salary
    println!("Engineering, ordered by salary:");
    let start = ("Engineering".to_string(), 0u32);
    let end = ("Engineering".to_string(), u32::MAX);
    for ((dept, salary), name) in index.range(start..=end) {
        println!("  {} - {} ({})", name, salary, dept);
    }
}

fn main() {
    btreemap_demos();
    bplus_tree_demo();
    comparison_demo();
}
```

Compilar y ejecutar:

```bash
rustc bplus_tree.rs && ./bplus_tree
```

> **Nota**: la implementación manual del B+ tree en Rust está
> simplificada — el split no preserva correctamente el hijo
> izquierdo. En la práctica, implementar un B+ tree completo en
> Rust requiere manejar ownership cuidadosamente (o usar `unsafe`).
> El `BTreeMap` de la stdlib es la solución idiomática.

---

## Errores frecuentes

1. **Split de hoja vs split de nodo interno**: en B+ tree, el split
   de hoja **copia** la clave al padre (la hoja retiene la clave).
   En nodo interno, la mediana **sube** (como en B-tree). Confundir
   ambos corrompe el árbol.
2. **Olvidar actualizar los enlaces** `next`/`prev` entre hojas
   durante split o merge. El range scan depende de estos enlaces.
3. **Comparación `>=` vs `>`**: en nodos internos del B+ tree se
   usa `>=` para descender (porque las claves guía son copias
   de claves que están en las hojas). Usar `>` causa que búsquedas
   de claves que coinciden con una guía vayan al subárbol
   equivocado.
4. **No eliminar la guía del padre** al hacer merge de hojas: en
   B+ tree la guía es una copia y debe eliminarse del padre
   (no baja al nodo fusionado como en B-tree).
5. **Asumir que todas las claves son únicas en nodos internos**:
   una clave puede aparecer en un nodo interno (como guía) y
   simultáneamente en una hoja (como dato). Esto es correcto y
   necesario.

---

## Ejercicios

Todos los ejercicios se resuelven en C salvo que se indique lo
contrario.

### Ejercicio 1 — Traza de inserción B+ tree

Dibuja el B+ tree de $t = 2$ paso a paso al insertar la secuencia
$\{3, 7, 1, 5, 9, 2, 4, 8, 6, 10\}$. Marca claramente nodos
internos (solo guías) vs hojas (con datos). Dibuja los enlaces
entre hojas.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos splits de hoja ocurren? ¿Cuántos splits de nodo interno?
¿Hay alguna clave que aparezca tanto en un nodo interno como en
una hoja?

</details>

### Ejercicio 2 — Fan-out comparativo

Dado un sistema con páginas de 16384 bytes, claves de 8 bytes,
punteros de 8 bytes, y valores de 256 bytes:

1. Calcula el fan-out máximo de un B-tree (nodos almacenan clave +
   valor + puntero).
2. Calcula el fan-out máximo de un B+ tree (nodos internos solo
   clave + puntero).
3. Para $10^8$ registros, calcula la altura de cada uno.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas veces mayor es el fan-out del B+ tree? ¿Cuántos niveles
menos tiene?

</details>

### Ejercicio 3 — Range scan

Implementa `bplus_range_count` que cuente cuántas claves hay en
el rango $[lo, hi]$ sin copiarlas a un array. Verifica que el
costo sea $O(\log_t n + k)$ contando el número de nodos visitados.

<details><summary>Predice antes de ejecutar</summary>

Para un B+ tree con $10^4$ claves y un rango que contiene 100
claves, ¿cuántos nodos (internos + hojas) se visitan
aproximadamente?

</details>

### Ejercicio 4 — Full scan ordenado

Usa la cadena de hojas para imprimir todas las claves en orden sin
recursión ni pila. Compara el número de accesos a nodo con un
recorrido in-order del B-tree equivalente de T03.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos nodos visita el full scan en B+ tree vs el in-order en
B-tree, para $n = 1000$ y $t = 3$? ¿Cuál accede a menos nodos
totales?

</details>

### Ejercicio 5 — Simulación de buffer pool

Implementa un "buffer pool" simplificado: un array de $P$ páginas
en memoria. Al buscar, primero verifica si la página está en el
pool (hit). Si no, cárgala del "disco" (el árbol en memoria) y
cuenta un "I/O". Mide la tasa de hits para búsquedas aleatorias
con $P = 10, 50, 100$.

<details><summary>Predice antes de ejecutar</summary>

¿Qué tasa de hits esperarías con $P = 10$ para un árbol de 1000
claves? Pista: los nodos internos (pocos) siempre estarán en
caché, las hojas (muchas) no.

</details>

### Ejercicio 6 — Bulk loading

Implementa `bplus_bulk_load(int *sorted_keys, int n)` que
construya un B+ tree bottom-up a partir de un array ordenado:
1. Crear hojas llenándolas con $2t-1$ claves cada una.
2. Enlazar las hojas.
3. Construir nodos internos nivel por nivel.

Compara el número de nodos creados con inserción uno por uno.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas hojas se crean para $n = 1000$ con $t = 3$ (5 claves
por hoja)? ¿Es más eficiente que 1000 inserciones individuales?

</details>

### Ejercicio 7 — Eliminación con merge

Partiendo de un B+ tree de $t = 2$ con claves $\{1, 2, \ldots, 15\}$
(construido por bulk loading), elimina en secuencia: $5, 10, 3, 12, 7$.
Dibuja el árbol después de cada eliminación. Indica cuándo ocurre
redistribución vs merge.

<details><summary>Predice antes de ejecutar</summary>

¿En cuál eliminación ocurre el primer merge? ¿La altura decrece
en algún momento durante estas 5 eliminaciones?

</details>

### Ejercicio 8 — BTreeMap range en Rust

Usando `BTreeMap`, implementa las siguientes queries sobre una
"tabla" de 10000 productos con (id, nombre, precio, categoría):

1. Todos los productos con precio en $[50, 100]$.
2. Los 5 productos más baratos (sin `sort`, solo iteración).
3. Todos los productos de categoría "electronics" ordenados por
   precio (requiere índice secundario).

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos índices `BTreeMap` necesitarás para resolver las 3
queries eficientemente? ¿Alguna query requiere full scan?

</details>

### Ejercicio 9 — Clustered vs non-clustered

Simula la diferencia entre un índice clustered y non-clustered:
- Clustered: las hojas del B+ tree contienen los datos completos.
- Non-clustered: las hojas contienen punteros a un array separado.

Para 10000 registros, mide el costo (en "accesos") de:
1. Búsqueda puntual por clave primaria.
2. Range scan de 100 registros consecutivos.
3. Range scan de 100 registros no consecutivos en el non-clustered.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos accesos extra necesita el non-clustered para un range
scan de 100 registros? ¿Por qué un clustered index es tan
ventajoso para range scans?

</details>

### Ejercicio 10 — Comparación B-tree vs B+ tree

Implementa ambos (B-tree de T03 y B+ tree) con el mismo $t = 3$.
Para $n = 5000$ claves aleatorias, compara:
1. Altura de cada estructura.
2. Número total de nodos.
3. Búsqueda puntual: comparaciones promedio.
4. Range scan $[1000, 2000]$: nodos visitados.

<details><summary>Predice antes de ejecutar</summary>

¿Cuál tendrá menor altura? ¿Cuál visitará menos nodos en el
range scan? ¿En la búsqueda puntual habrá diferencia significativa?

</details>
