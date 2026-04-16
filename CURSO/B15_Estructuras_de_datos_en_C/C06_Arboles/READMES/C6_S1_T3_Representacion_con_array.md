# Representación con array

## La idea

En vez de nodos con punteros, almacenamos los elementos del árbol en un
**array** usando la posición como relación padre-hijo.  Para un nodo en el
índice $i$ (0-indexed):

$$\text{hijo izquierdo} = 2i + 1$$
$$\text{hijo derecho} = 2i + 2$$
$$\text{padre} = \left\lfloor \frac{i - 1}{2} \right\rfloor$$

```
Árbol:
        10
       /  \
      5    15
     / \     \
    3   7    20

Array (índice):  [0]  [1]  [2]  [3]  [4]  [5]  [6]
                  10    5   15    3    7    _   20

Índice 0 (10): left = 2(0)+1 = 1 (5),  right = 2(0)+2 = 2 (15)
Índice 1 (5):  left = 2(1)+1 = 3 (3),  right = 2(1)+2 = 4 (7)
Índice 2 (15): left = 2(2)+1 = 5 (_),  right = 2(2)+2 = 6 (20)
```

El índice 5 está vacío (15 no tiene hijo izquierdo) — se marca como vacío.

---

## Correspondencia visual

El array se lee por **niveles** (BFS order):

```
Nivel 0:  10
Nivel 1:  5, 15
Nivel 2:  3, 7, _, 20

Array:    [10, 5, 15, 3, 7, _, 20]
Índice:     0  1   2  3  4  5   6
```

Las fórmulas son aritmética pura — sin punteros, sin indirecciones, sin
allocations adicionales.

---

## Implementación en C

### Estructura

```c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define EMPTY INT_MIN    // marcador de posición vacía

typedef struct ArrayTree {
    int *data;
    int capacity;        // tamaño del array
} ArrayTree;

ArrayTree atree_new(int capacity) {
    ArrayTree tree;
    tree.capacity = capacity;
    tree.data = malloc(capacity * sizeof(int));
    for (int i = 0; i < capacity; i++) {
        tree.data[i] = EMPTY;
    }
    return tree;
}

void atree_free(ArrayTree *tree) {
    free(tree->data);
    tree->data = NULL;
    tree->capacity = 0;
}
```

### Navegación

```c
int left_child(int i)  { return 2 * i + 1; }
int right_child(int i) { return 2 * i + 2; }
int parent(int i)      { return (i - 1) / 2; }

int has_node(const ArrayTree *tree, int i) {
    return i >= 0 && i < tree->capacity && tree->data[i] != EMPTY;
}
```

### Insertar

```c
void atree_set(ArrayTree *tree, int index, int value) {
    if (index >= 0 && index < tree->capacity) {
        tree->data[index] = value;
    }
}

// Construir el árbol del ejemplo
void build_sample(ArrayTree *tree) {
    atree_set(tree, 0, 10);    // raíz
    atree_set(tree, 1, 5);     // left(0)
    atree_set(tree, 2, 15);    // right(0)
    atree_set(tree, 3, 3);     // left(1)
    atree_set(tree, 4, 7);     // right(1)
    // índice 5 vacío           // left(2) = vacío
    atree_set(tree, 6, 20);    // right(2)
}
```

### Operaciones recursivas

```c
int atree_count(const ArrayTree *tree, int i) {
    if (!has_node(tree, i)) return 0;
    return 1 + atree_count(tree, left_child(i))
             + atree_count(tree, right_child(i));
}

int atree_height(const ArrayTree *tree, int i) {
    if (!has_node(tree, i)) return -1;
    int lh = atree_height(tree, left_child(i));
    int rh = atree_height(tree, right_child(i));
    return 1 + (lh > rh ? lh : rh);
}

int atree_sum(const ArrayTree *tree, int i) {
    if (!has_node(tree, i)) return 0;
    return tree->data[i]
         + atree_sum(tree, left_child(i))
         + atree_sum(tree, right_child(i));
}

void atree_print_inorder(const ArrayTree *tree, int i) {
    if (!has_node(tree, i)) return;
    atree_print_inorder(tree, left_child(i));
    printf("%d ", tree->data[i]);
    atree_print_inorder(tree, right_child(i));
}
```

Las funciones son idénticas a las versiones con punteros — solo cambia
cómo se accede a los hijos: `root->left` se convierte en `left_child(i)`,
y `root == NULL` se convierte en `!has_node(tree, i)`.

---

## Árboles completos: el caso ideal

La representación con array es **perfecta** para árboles completos (todos
los niveles llenos excepto posiblemente el último, que llena de izquierda a
derecha):

```
Árbol completo:
        1
       / \
      2   3
     / \ /
    4  5 6

Array: [1, 2, 3, 4, 5, 6]

Sin huecos — cada posición del array está ocupada.
```

Los **heaps** (montículos, tema de C07) son árboles completos y siempre
se implementan con array.

### Por qué funciona

En un árbol completo de $n$ nodos:
- Los nodos ocupan las posiciones $0, 1, 2, \ldots, n-1$ sin huecos.
- No se desperdicia memoria.
- La localidad de caché es excelente (datos contiguos).

---

## Árboles desbalanceados: el problema

Para un árbol desbalanceado, el array tiene **huecos** que desperdician
memoria:

```
Árbol degenerado:
  1
   \
    2
     \
      3
       \
        4

Array: [1, _, 2, _, _, _, 3, _, _, _, _, _, _, _, 4]
Índice: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14

Nodo 1: índice 0
Nodo 2: right(0) = 2
Nodo 3: right(2) = 6
Nodo 4: right(6) = 14
```

4 nodos requieren un array de 15 posiciones — 11 vacías (73% desperdicio).

En general, un árbol degenerado de $n$ nodos requiere un array de tamaño
$2^n - 1$:

| Nodos | Array (degenerado) | Array (completo) | Punteros |
|:-----:|:------------------:|:----------------:|:--------:|
| 4 | 15 | 4 | 4 nodos × 24 bytes |
| 10 | 1,023 | 10 | 10 nodos × 24 bytes |
| 20 | 1,048,575 | 20 | 20 nodos × 24 bytes |
| 30 | ~$10^9$ | 30 | 30 nodos × 24 bytes |

Para 30 nodos degenerados, el array necesitaría **mil millones** de posiciones.
La representación con punteros usa solo 30 × 24 = 720 bytes.

---

## Implementación en Rust

```rust
const EMPTY: i32 = i32::MIN;

struct ArrayTree {
    data: Vec<i32>,
}

impl ArrayTree {
    fn new(capacity: usize) -> Self {
        ArrayTree {
            data: vec![EMPTY; capacity],
        }
    }

    fn left(i: usize) -> usize { 2 * i + 1 }
    fn right(i: usize) -> usize { 2 * i + 2 }
    fn parent(i: usize) -> usize { (i.wrapping_sub(1)) / 2 }

    fn has_node(&self, i: usize) -> bool {
        i < self.data.len() && self.data[i] != EMPTY
    }

    fn set(&mut self, i: usize, value: i32) {
        if i < self.data.len() {
            self.data[i] = value;
        }
    }

    fn count(&self, i: usize) -> usize {
        if !self.has_node(i) { return 0; }
        1 + self.count(Self::left(i)) + self.count(Self::right(i))
    }

    fn height(&self, i: usize) -> i32 {
        if !self.has_node(i) { return -1; }
        1 + self.height(Self::left(i)).max(self.height(Self::right(i)))
    }

    fn print_inorder(&self, i: usize) {
        if !self.has_node(i) { return; }
        self.print_inorder(Self::left(i));
        print!("{} ", self.data[i]);
        self.print_inorder(Self::right(i));
    }
}
```

### Versión genérica con Option

Para tipos que no tienen un valor "vacío" natural:

```rust
struct ArrayTree<T> {
    data: Vec<Option<T>>,
}

impl<T> ArrayTree<T> {
    fn new(capacity: usize) -> Self {
        let mut data = Vec::with_capacity(capacity);
        for _ in 0..capacity {
            data.push(None);
        }
        ArrayTree { data }
    }

    fn has_node(&self, i: usize) -> bool {
        i < self.data.len() && self.data[i].is_some()
    }

    fn set(&mut self, i: usize, value: T) {
        if i < self.data.len() {
            self.data[i] = Some(value);
        }
    }

    fn get(&self, i: usize) -> Option<&T> {
        self.data.get(i).and_then(|opt| opt.as_ref())
    }
}
```

`Option<T>` es más limpio que usar un valor centinela (`INT_MIN`) porque
funciona para cualquier tipo y no hay riesgo de confundir un dato real con
el marcador de vacío.

---

## Recorrido por niveles (BFS)

El array ya almacena los nodos en order BFS — recorrer el array de izquierda
a derecha es un recorrido por niveles:

```c
void atree_print_bfs(const ArrayTree *tree) {
    for (int i = 0; i < tree->capacity; i++) {
        if (tree->data[i] != EMPTY) {
            printf("%d ", tree->data[i]);
        }
    }
    printf("\n");
}
```

Para el árbol de ejemplo: `10 5 15 3 7 20` — nivel por nivel.

Con punteros, BFS requiere una cola auxiliar (tema de S02/T02).  Con array,
es un loop trivial.

---

## Comparación: punteros vs array

| Aspecto | Punteros | Array |
|---------|---------|-------|
| Memoria (completo, n nodos) | $n \times 24$ bytes | $n \times 4$ bytes (int) |
| Memoria (degenerado, n nodos) | $n \times 24$ bytes | $(2^n - 1) \times 4$ bytes |
| Acceso a hijo | Desreferenciar puntero | Aritmética: $2i+1$, $2i+2$ |
| Acceso a padre | No disponible (sin parent ptr) | Aritmética: $(i-1)/2$ |
| Localidad de caché | Mala (nodos dispersos) | Excelente (array contiguo) |
| Inserción/eliminación | Redirigir punteros | Marcar posición (+ posible resize) |
| Árboles desbalanceados | Eficiente | Desperdicio exponencial |
| Implementación | Más compleja (malloc/free) | Más simple (aritmética) |
| Uso principal | BST, AVL, árboles generales | Heaps, árboles completos |

### Regla práctica

- **Árbol completo** (heap) → array.
- **Árbol potencialmente desbalanceado** (BST, AVL) → punteros.

---

## Acceso al padre

Una ventaja exclusiva del array: acceder al padre es trivial.  Con punteros,
necesitaríamos un campo `parent` adicional (tercer puntero) o pasar el padre
como argumento en la recursión.

```c
// Con array: padre inmediato
int get_parent_value(const ArrayTree *tree, int i) {
    if (i == 0) return EMPTY;   // raíz no tiene padre
    int p = parent(i);
    return tree->data[p];
}

// Con punteros: no disponible sin campo parent
// Alternativa: buscar el padre recorriendo desde la raíz — O(n)
```

Esto es especialmente útil en heaps, donde el "swim up" (subir el nodo
hacia la raíz) requiere acceder al padre repetidamente.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define EMPTY INT_MIN

typedef struct ArrayTree {
    int *data;
    int capacity;
} ArrayTree;

ArrayTree atree_new(int capacity) {
    ArrayTree t;
    t.capacity = capacity;
    t.data = malloc(capacity * sizeof(int));
    for (int i = 0; i < capacity; i++) t.data[i] = EMPTY;
    return t;
}

void atree_free(ArrayTree *t) { free(t->data); }

int left_child(int i)  { return 2 * i + 1; }
int right_child(int i) { return 2 * i + 2; }
int parent_idx(int i)  { return (i - 1) / 2; }

int has_node(const ArrayTree *t, int i) {
    return i >= 0 && i < t->capacity && t->data[i] != EMPTY;
}

int atree_count(const ArrayTree *t, int i) {
    if (!has_node(t, i)) return 0;
    return 1 + atree_count(t, left_child(i)) + atree_count(t, right_child(i));
}

int atree_height(const ArrayTree *t, int i) {
    if (!has_node(t, i)) return -1;
    int lh = atree_height(t, left_child(i));
    int rh = atree_height(t, right_child(i));
    return 1 + (lh > rh ? lh : rh);
}

void atree_print_tree(const ArrayTree *t, int i, int depth) {
    if (!has_node(t, i)) return;
    atree_print_tree(t, right_child(i), depth + 1);
    for (int d = 0; d < depth; d++) printf("    ");
    printf("%d\n", t->data[i]);
    atree_print_tree(t, left_child(i), depth + 1);
}

void atree_print_bfs(const ArrayTree *t) {
    printf("BFS: ");
    for (int i = 0; i < t->capacity; i++) {
        if (t->data[i] != EMPTY) printf("%d ", t->data[i]);
    }
    printf("\n");
}

int main(void) {
    //        10
    //       /  \
    //      5    15
    //     / \     \
    //    3   7    20

    ArrayTree tree = atree_new(16);
    tree.data[0] = 10;
    tree.data[1] = 5;
    tree.data[2] = 15;
    tree.data[3] = 3;
    tree.data[4] = 7;
    // tree.data[5] = EMPTY   (15 no tiene left)
    tree.data[6] = 20;

    printf("Tree:\n");
    atree_print_tree(&tree, 0, 0);

    printf("\nNodes:  %d\n", atree_count(&tree, 0));     // 6
    printf("Height: %d\n", atree_height(&tree, 0));      // 2

    atree_print_bfs(&tree);   // BFS: 10 5 15 3 7 20

    // Acceso al padre
    printf("\nParent of 7 (index 4): %d\n",
           tree.data[parent_idx(4)]);    // 5
    printf("Parent of 15 (index 2): %d\n",
           tree.data[parent_idx(2)]);    // 10

    // Array interno
    printf("\nArray: [");
    for (int i = 0; i < 7; i++) {
        if (tree.data[i] == EMPTY) printf("_");
        else printf("%d", tree.data[i]);
        if (i < 6) printf(", ");
    }
    printf("]\n");   // [10, 5, 15, 3, 7, _, 20]

    atree_free(&tree);
    return 0;
}
```

Salida:

```
Tree:
        20
    15
10
        7
    5
        3

Nodes:  6
Height: 2
BFS: 10 5 15 3 7 20

Parent of 7 (index 4): 5
Parent of 15 (index 2): 10

Array: [10, 5, 15, 3, 7, _, 20]
```

---

## Ejercicios

### Ejercicio 1 — Fórmulas de navegación

Dado el array `[A, B, C, D, E, F, G]` (árbol perfecto de 7 nodos), calcula
manualmente:
- Hijos de B (índice 1).
- Hijos de C (índice 2).
- Padre de F (índice 5).
- Padre de G (índice 6).

<details>
<summary>Cálculos</summary>

```
B (i=1): left = 2(1)+1 = 3 (D),  right = 2(1)+2 = 4 (E)
C (i=2): left = 2(2)+1 = 5 (F),  right = 2(2)+2 = 6 (G)
F (i=5): parent = (5-1)/2 = 2 (C)
G (i=6): parent = (6-1)/2 = 2 (C)
```

Las fórmulas dan la navegación completa sin punteros.
</details>

### Ejercicio 2 — Array para árbol degenerado

Dibuja el array necesario para el árbol degenerado `1 → 2 → 3 → 4 → 5`
(cada nodo solo tiene hijo derecho).  ¿Cuántas posiciones se desperdician?

<details>
<summary>Array</summary>

```
Índices necesarios:
  1 en [0]
  2 en right(0) = [2]
  3 en right(2) = [6]
  4 en right(6) = [14]
  5 en right(14) = [30]

Array: necesita 31 posiciones (índices 0..30)
       5 ocupadas, 26 vacías → 84% desperdicio

[1, _, 2, _, _, _, 3, _, _, _, _, _, _, _, 4,
 _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, 5]
```

Crece exponencialmente: cada nivel derecho duplica el índice + 2.
</details>

### Ejercicio 3 — Nivel de un nodo por índice

Dado un índice $i$ en el array, ¿en qué nivel está el nodo?  Encuentra una
fórmula sin recursión.

<details>
<summary>Fórmula</summary>

$$\text{level}(i) = \lfloor \log_2(i + 1) \rfloor$$

```
i=0: log2(1) = 0   → nivel 0 (raíz)
i=1: log2(2) = 1   → nivel 1
i=2: log2(3) = 1   → nivel 1
i=3: log2(4) = 2   → nivel 2
i=6: log2(7) = 2   → nivel 2
i=7: log2(8) = 3   → nivel 3
```

En C: `(int)(log2(i + 1))` o con bits: `31 - __builtin_clz(i + 1)` (GCC).
En Rust: `(i + 1).ilog2()`.
</details>

### Ejercicio 4 — Verificar si es hoja

Implementa `is_leaf(tree, i)` para la representación con array.  Un nodo
es hoja si existe pero ambos hijos no existen.

<details>
<summary>Implementación</summary>

```c
int is_leaf(const ArrayTree *t, int i) {
    return has_node(t, i)
        && !has_node(t, left_child(i))
        && !has_node(t, right_child(i));
}
```

Idéntica a la versión con punteros, pero con `has_node` en vez de
`!= NULL`.
</details>

### Ejercicio 5 — BFS trivial

Muestra que recorrer el array de izquierda a derecha (saltando vacíos)
produce exactamente el orden BFS.  ¿Por qué esto funciona?

<details>
<summary>Explicación</summary>

El array almacena los nodos en order de niveles por construcción:
- Índice 0: nivel 0 (raíz).
- Índices 1-2: nivel 1.
- Índices 3-6: nivel 2.
- Índices 7-14: nivel 3.

El rango del nivel $k$ es $[2^k - 1, 2^{k+1} - 2]$.  Estos rangos son
contiguos y crecientes → recorrer el array secuencialmente visita todos
los nodos del nivel 0, luego todos los del nivel 1, etc. — exactamente
BFS.

Con punteros, BFS necesita una cola para manejar el orden de visita.
Con array, el orden ya está en la disposición de los datos.
</details>

### Ejercicio 6 — Implementar en Rust con Option

Implementa `ArrayTree<T>` con `Vec<Option<T>>`.  Incluye `set`, `get`,
`count`, `height`.  Prueba con `String`.

<details>
<summary>Prueba</summary>

```rust
let mut tree: ArrayTree<String> = ArrayTree::new(7);
tree.set(0, "root".to_string());
tree.set(1, "left".to_string());
tree.set(2, "right".to_string());

assert_eq!(tree.get(0), Some(&"root".to_string()));
assert_eq!(tree.get(5), None);
assert_eq!(tree.count(0), 3);
```

`Option<T>` no necesita un valor centinela — `None` indica vacío para
cualquier tipo.
</details>

### Ejercicio 7 — Heap preview

Un **min-heap** cumple: `array[parent(i)] <= array[i]` para todo nodo.
Dado el array `[2, 5, 3, 8, 7, 6, 4]`, verifica si es un min-heap.
Dibuja el árbol correspondiente.

<details>
<summary>Verificación</summary>

```
Árbol:
        2
       / \
      5   3
     / \ / \
    8  7 6  4

Verificar propiedad:
  parent(1)=0: array[0]=2 <= array[1]=5 ✓
  parent(2)=0: array[0]=2 <= array[2]=3 ✓
  parent(3)=1: array[1]=5 <= array[3]=8 ✓
  parent(4)=1: array[1]=5 <= array[4]=7 ✓
  parent(5)=2: array[2]=3 <= array[5]=6 ✓
  parent(6)=2: array[2]=3 <= array[6]=4 ✓
```

Sí es un min-heap — la raíz (2) es el mínimo.  La representación con
array es ideal para heaps porque el árbol siempre es completo.
</details>

### Ejercicio 8 — Comparar memoria

Para $n = 1000$ nodos de tipo `int`, calcula la memoria total de:
1. Array (árbol completo — sin huecos).
2. Punteros (nodos de 24 bytes).

<details>
<summary>Cálculo</summary>

```
Array:   1000 × 4 bytes = 4,000 bytes + overhead de struct ≈ 4,024 bytes
Punteros: 1000 × 24 bytes = 24,000 bytes

Ratio: punteros usan 6× más memoria que array.
```

Para un árbol completo (como un heap), el array es dramáticamente más
eficiente.  Sin punteros (16 bytes ahorrados por nodo), más localidad de
caché (datos contiguos).
</details>

### Ejercicio 9 — Convertir punteros a array

Escribe `tree_to_array(root, array, i)` que convierta un árbol con punteros
a la representación con array.

<details>
<summary>Implementación</summary>

```c
void tree_to_array(TreeNode *root, ArrayTree *tree, int i) {
    if (!root) return;
    if (i >= tree->capacity) return;   // array demasiado pequeño

    tree->data[i] = root->data;
    tree_to_array(root->left, tree, left_child(i));
    tree_to_array(root->right, tree, right_child(i));
}

// Uso:
ArrayTree arr = atree_new(64);
tree_to_array(root, &arr, 0);
```

Recorrido preorden — cada nodo se almacena en su posición calculada.
Para árboles desbalanceados, `capacity` debe ser lo suficientemente grande.
</details>

### Ejercicio 10 — Cuándo usar cada representación

Para cada escenario, elige array o punteros y justifica:
1. Min-heap para cola de prioridad.
2. BST con inserciones aleatorias.
3. Árbol de expresión aritmética.
4. Heap binario que crece dinámicamente.

<details>
<summary>Respuestas</summary>

1. **Min-heap → array**.  El heap siempre es completo (sin huecos).  Array
   da localidad de caché + acceso al padre $O(1)$ para swim-up.

2. **BST → punteros**.  Inserciones aleatorias pueden desbalancear el árbol.
   Con array, un BST degenerado desperdiciaría memoria exponencialmente.

3. **Árbol de expresión → punteros**.  La estructura es arbitraria (depende
   de la expresión).  Nodos con operadores tienen 2 hijos, nodos con
   números son hojas.  No necesariamente completo.

4. **Heap dinámico → array (Vec)**.  Usar `Vec` con `push`/`pop` para
   crecer.  El heap se mantiene completo, así que el array nunca tiene
   huecos.  `Vec` da amortized $O(1)$ para resize.
</details>
