# Árboles n-arios

## Motivación

Hasta ahora hemos trabajado exclusivamente con árboles **binarios**: cada nodo tiene
a lo sumo dos hijos. Sin embargo, la realidad está llena de estructuras jerárquicas
donde un nodo puede tener un número arbitrario de hijos:

- **Sistema de archivos**: un directorio contiene 0…$n$ entradas (archivos u otros
  directorios).
- **DOM HTML**: `<div>` puede contener cualquier cantidad de elementos hijos.
- **Organigrama**: un gerente supervisa a $k$ personas.
- **Árbol sintáctico**: una llamada a función tiene $n$ argumentos.
- **Taxonomía biológica**: un género contiene múltiples especies.

Un **árbol n-ario** (o **árbol general**) es un árbol donde cada nodo puede tener
$0, 1, 2, \dots$ hijos sin límite fijo. Formalmente:

> Un árbol n-ario es un conjunto finito $T$ de nodos tal que:
> 1. Existe un nodo distinguido $r$ llamado **raíz**.
> 2. Los nodos restantes se particionan en $k \geq 0$ subconjuntos disjuntos
>    $T_1, T_2, \dots, T_k$, cada uno de los cuales es a su vez un árbol n-ario.
>    Los $T_i$ son los **subárboles** de $r$, y las raíces de $T_i$ son los
>    **hijos** de $r$.

La diferencia fundamental con un árbol binario es que no hablamos de hijo
"izquierdo" e hijo "derecho", sino de una **secuencia ordenada** de hijos
$(c_1, c_2, \dots, c_k)$ cuyo tamaño $k$ varía por nodo.

---

## El problema de la representación

En un árbol binario, cada nodo tiene exactamente dos punteros (`left`, `right`).
Esto es simple y uniforme. En un árbol n-ario, ¿cuántos punteros reservamos?

### Opción ingenua: máximo fijo

```c
#define MAX_CHILDREN 10

typedef struct NaryNode {
    int data;
    struct NaryNode *children[MAX_CHILDREN];
    int num_children;
} NaryNode;
```

Problemas:
- Si un nodo tiene 2 hijos, desperdiciamos 8 punteros ($8 \times 8 = 64$ bytes en
  64-bit).
- Si un nodo necesita 11 hijos, falla.
- Para $n$ nodos con máximo $M$ hijos, usamos $n \times M$ punteros, de los cuales
  solo $n - 1$ son no nulos (porque un árbol de $n$ nodos tiene exactamente $n-1$
  aristas). La **tasa de desperdicio** es $1 - \frac{n-1}{nM} \approx 1 - \frac{1}{M}$,
  que para $M = 10$ es del 90%.

Necesitamos representaciones más inteligentes. Las dos principales son:

1. **Array dinámico de hijos** — cada nodo tiene un arreglo que crece según necesite.
2. **Hijo-izquierdo / hermano-derecho** — transforma cualquier árbol general en un
   árbol binario.

---

## Representación 1: Array dinámico de hijos

Cada nodo almacena un arreglo (o lista) de punteros a sus hijos, cuyo tamaño se
ajusta dinámicamente.

### En C

```c
typedef struct NaryNode {
    int data;
    struct NaryNode **children;  // array dinamico de punteros
    int num_children;
    int capacity;
} NaryNode;
```

Creación y adición de hijos:

```c
NaryNode *nary_create(int data) {
    NaryNode *node = malloc(sizeof(NaryNode));
    node->data = data;
    node->children = NULL;
    node->num_children = 0;
    node->capacity = 0;
    return node;
}

void nary_add_child(NaryNode *parent, NaryNode *child) {
    if (parent->num_children == parent->capacity) {
        int new_cap = parent->capacity == 0 ? 2 : parent->capacity * 2;
        parent->children = realloc(parent->children,
                                   new_cap * sizeof(NaryNode *));
        parent->capacity = new_cap;
    }
    parent->children[parent->num_children++] = child;
}
```

El patrón es idéntico al de un **vector dinámico** (como `std::vector` o `Vec`):
capacidad inicial pequeña, duplicar al llenar. La inserción amortizada es $O(1)$.

### En Rust

```rust
struct NaryNode<T> {
    data: T,
    children: Vec<Box<NaryNode<T>>>,
}

impl<T> NaryNode<T> {
    fn new(data: T) -> Self {
        NaryNode { data, children: Vec::new() }
    }

    fn add_child(&mut self, child: NaryNode<T>) {
        self.children.push(Box::new(child));
    }
}
```

`Vec` maneja automáticamente la capacidad dinámica. Cada hijo es un `Box<NaryNode<T>>`
para tener un tamaño conocido en tiempo de compilación.

### Ventajas y desventajas

| Aspecto | Valoración |
|---------|------------|
| Acceso al hijo $i$ | $O(1)$ — acceso directo por índice |
| Agregar hijo | $O(1)$ amortizado |
| Insertar hijo en posición $i$ | $O(k)$ — desplazar elementos |
| Eliminar hijo $i$ | $O(k)$ — desplazar elementos |
| Memoria por nodo | Proporcional al número real de hijos |
| Recorrido de hijos | Excelente localidad de caché |

Esta representación es **intuitiva** y eficiente para la mayoría de aplicaciones
prácticas. Es la representación estándar en sistemas de archivos, DOM, y árboles
sintácticos.

---

## Representación 2: Hijo-izquierdo / hermano-derecho (LCRS)

También llamada **left-child right-sibling** (LCRS), **first-child next-sibling**,
o **representación de Knuth**. La idea es brillante:

> Cada nodo almacena solo **dos** punteros:
> - `first_child` → su primer hijo (el más a la izquierda).
> - `next_sibling` → su siguiente hermano (el nodo que le sigue en la lista de
>   hijos del padre).

Con solo dos punteros por nodo — exactamente como un árbol binario — podemos
representar **cualquier** árbol general.

### Transformación visual

Árbol general original:

```
         A
       / | \
      B  C  D
     /|     |
    E  F    G
```

Cada nodo tiene: `first_child` (↓) y `next_sibling` (→).

```
A
↓
B → C → D
↓       ↓
E → F   G
```

Como árbol binario (left = first_child, right = next_sibling):

```
        A
       /
      B
     / \
    E   C
     \   \
      F   D
         /
        G
```

### Algoritmo de transformación

Para convertir un árbol n-ario a LCRS:

1. El `left` del nodo binario apunta al **primer hijo** del nodo original.
2. El `right` del nodo binario apunta al **siguiente hermano** del nodo original.
3. La raíz nunca tiene hermano derecho (es el único en su nivel).

Para la inversa (LCRS a n-ario):

1. Los hijos de un nodo son: su hijo izquierdo, y todos los nodos alcanzables
   siguiendo los punteros derechos desde ese hijo izquierdo.

### En C

```c
typedef struct LCRSNode {
    int data;
    struct LCRSNode *first_child;
    struct LCRSNode *next_sibling;
} LCRSNode;

LCRSNode *lcrs_create(int data) {
    LCRSNode *node = malloc(sizeof(LCRSNode));
    node->data = data;
    node->first_child = NULL;
    node->next_sibling = NULL;
    return node;
}

void lcrs_add_child(LCRSNode *parent, LCRSNode *child) {
    if (parent->first_child == NULL) {
        parent->first_child = child;
    } else {
        // recorrer hasta el ultimo hermano
        LCRSNode *sibling = parent->first_child;
        while (sibling->next_sibling != NULL) {
            sibling = sibling->next_sibling;
        }
        sibling->next_sibling = child;
    }
}
```

Agregar un hijo requiere recorrer la lista de hermanos: $O(k)$ donde $k$ es el
número actual de hijos. Si se necesita $O(1)$, se puede mantener un puntero al
último hermano, o insertar siempre al inicio:

```c
void lcrs_add_child_front(LCRSNode *parent, LCRSNode *child) {
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}
```

### En Rust

```rust
type Link<T> = Option<Box<LCRSNode<T>>>;

struct LCRSNode<T> {
    data: T,
    first_child: Link<T>,
    next_sibling: Link<T>,
}

impl<T> LCRSNode<T> {
    fn new(data: T) -> Self {
        LCRSNode {
            data,
            first_child: None,
            next_sibling: None,
        }
    }

    fn add_child_front(&mut self, data: T) {
        let mut child = Box::new(LCRSNode::new(data));
        child.next_sibling = self.first_child.take();
        self.first_child = Some(child);
    }
}
```

### Ventajas y desventajas

| Aspecto | Valoración |
|---------|------------|
| Memoria por nodo | Fija: 2 punteros (como árbol binario) |
| Acceso al hijo $i$ | $O(i)$ — recorrer lista de hermanos |
| Agregar hijo al inicio | $O(1)$ |
| Agregar hijo al final | $O(k)$ (o $O(1)$ con puntero extra) |
| Reusar algoritmos de árboles binarios | Sí, directamente |
| Intuitividad | Menor — la estructura no refleja la jerarquía original |

---

## Comparación de las dos representaciones

| Criterio | Array de hijos | LCRS |
|----------|---------------|------|
| Punteros por nodo | Variable (vector dinámico) | Exactamente 2 |
| Memoria total ($n$ nodos) | $n \times$ (struct + vec overhead) | $n \times$ (struct + 2 ptrs) |
| Acceso al hijo $i$ | $O(1)$ | $O(i)$ |
| Número de hijos | $O(1)$ — campo almacenado | $O(k)$ — recorrer lista |
| Agregar hijo | $O(1)$ amortizado | $O(1)$ al inicio, $O(k)$ al final |
| Eliminar hijo $i$ | $O(k)$ — desplazar | $O(i)$ — recorrer + reenlazar |
| Recorridos | `for` sobre array | Seguir `first_child` + `next_sibling` |
| Conversión a binario | Requiere transformación | Ya **es** un árbol binario |
| Uso típico | DOM, filesystems, UI | Algoritmos teóricos, compiladores |

**Regla práctica**: si necesitas acceso rápido al hijo $i$ o conocer el número de
hijos, usa array de hijos. Si la memoria es crítica o quieres reutilizar algoritmos
de árboles binarios, usa LCRS.

---

## Recorridos en árboles n-arios

Los recorridos de árboles binarios se generalizan a n-arios, pero con una diferencia
importante: **el recorrido inorden no está bien definido** para árboles n-arios,
porque no existe un punto natural "entre" los hijos cuando hay más de dos.

### Preorden (DFS)

Visitar el nodo, luego recursivamente cada hijo de izquierda a derecha.

```
         A
       / | \
      B  C  D
     /|     |
    E  F    G
```

Preorden: **A, B, E, F, C, D, G**

```c
// Array de hijos
void nary_preorder(NaryNode *root) {
    if (root == NULL) return;
    printf("%d ", root->data);
    for (int i = 0; i < root->num_children; i++) {
        nary_preorder(root->children[i]);
    }
}

// LCRS
void lcrs_preorder(LCRSNode *root) {
    if (root == NULL) return;
    printf("%d ", root->data);
    lcrs_preorder(root->first_child);
    lcrs_preorder(root->next_sibling);
}
```

Observación crucial: el preorden del LCRS con la función de arriba produce
exactamente el mismo resultado que el preorden del árbol general original.
Esto es porque `first_child` visita al primer hijo (y recursivamente sus
descendientes), y `next_sibling` visita a los hermanos restantes.

Sin embargo, hay una sutileza: la llamada `lcrs_preorder(root->next_sibling)`
**no** debe ejecutarse para la raíz del árbol completo (que no tiene hermanos).
En la práctica esto funciona porque `root->next_sibling` es `NULL` para la raíz.

### Postorden (DFS)

Visitar recursivamente cada hijo, luego el nodo.

Postorden del ejemplo: **E, F, B, C, G, D, A**

```c
// Array de hijos
void nary_postorder(NaryNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < root->num_children; i++) {
        nary_postorder(root->children[i]);
    }
    printf("%d ", root->data);
}

// LCRS — cuidado: no es identico al postorden binario
void lcrs_postorder(LCRSNode *root) {
    if (root == NULL) return;
    lcrs_postorder(root->first_child);   // todos los hijos primero
    printf("%d ", root->data);            // visitar nodo
    lcrs_postorder(root->next_sibling);   // luego los hermanos
}
```

En LCRS, el postorden n-ario corresponde al **inorden** del árbol binario
equivalente. Esto es un resultado interesante de la transformación.

### Recorrido por niveles (BFS)

Funciona igual que en árboles binarios, pero al encolar se encolan **todos** los
hijos en lugar de solo dos.

```c
// Array de hijos — BFS
void nary_bfs(NaryNode *root) {
    if (root == NULL) return;

    NaryNode *queue[1000];
    int front = 0, rear = 0;
    queue[rear++] = root;

    while (front < rear) {
        NaryNode *node = queue[front++];
        printf("%d ", node->data);

        for (int i = 0; i < node->num_children; i++) {
            queue[rear++] = node->children[i];
        }
    }
}
```

BFS del ejemplo: **A, B, C, D, E, F, G**

Para LCRS, el BFS requiere más cuidado. Al desencolar un nodo, se debe encolar
su `first_child`, y luego seguir la cadena de `next_sibling` de ese hijo para
encolar a todos los hermanos:

```c
// LCRS — BFS
void lcrs_bfs(LCRSNode *root) {
    if (root == NULL) return;

    LCRSNode *queue[1000];
    int front = 0, rear = 0;
    queue[rear++] = root;

    while (front < rear) {
        LCRSNode *node = queue[front++];
        printf("%d ", node->data);

        // encolar todos los hijos
        LCRSNode *child = node->first_child;
        while (child != NULL) {
            queue[rear++] = child;
            child = child->next_sibling;
        }
    }
}
```

### Recorridos en Rust

```rust
impl<T: std::fmt::Display> NaryNode<T> {
    fn preorder(&self) {
        print!("{} ", self.data);
        for child in &self.children {
            child.preorder();
        }
    }

    fn postorder(&self) {
        for child in &self.children {
            child.postorder();
        }
        print!("{} ", self.data);
    }

    fn bfs(&self) {
        use std::collections::VecDeque;
        let mut queue = VecDeque::new();
        queue.push_back(self);

        while let Some(node) = queue.pop_front() {
            print!("{} ", node.data);
            for child in &node.children {
                queue.push_back(child);
            }
        }
    }
}
```

### Collecting en Vec

En la práctica, es más útil recolectar los valores que imprimirlos:

```rust
impl<T: Clone> NaryNode<T> {
    fn preorder_vec(&self) -> Vec<T> {
        let mut result = vec![self.data.clone()];
        for child in &self.children {
            result.extend(child.preorder_vec());
        }
        result
    }

    fn postorder_vec(&self) -> Vec<T> {
        let mut result = Vec::new();
        for child in &self.children {
            result.extend(child.postorder_vec());
        }
        result.push(self.data.clone());
        result
    }
}
```

---

## Operaciones comunes

### Altura

La altura de un árbol n-ario es la longitud del camino más largo desde la raíz
hasta una hoja.

```c
// Array de hijos
int nary_height(NaryNode *root) {
    if (root == NULL) return -1;
    int max_h = -1;
    for (int i = 0; i < root->num_children; i++) {
        int h = nary_height(root->children[i]);
        if (h > max_h) max_h = h;
    }
    return max_h + 1;
}

// LCRS
int lcrs_height(LCRSNode *root) {
    if (root == NULL) return -1;
    int child_h = lcrs_height(root->first_child);
    int sibling_h = lcrs_height(root->next_sibling);
    // height = 1 + max height among children
    // but sibling is at same level, not one deeper
    return (child_h + 1 > sibling_h) ? child_h + 1 : sibling_h;
}
```

La versión LCRS es más sutil: la altura a través de `first_child` suma 1 (bajamos
un nivel), pero a través de `next_sibling` **no** suma 1 (los hermanos están en el
mismo nivel que el nodo actual).

```rust
impl<T> NaryNode<T> {
    fn height(&self) -> i32 {
        self.children.iter()
            .map(|c| c.height())
            .max()
            .map_or(0, |h| h + 1)
    }
}
```

### Contar nodos

```c
int nary_size(NaryNode *root) {
    if (root == NULL) return 0;
    int count = 1;
    for (int i = 0; i < root->num_children; i++) {
        count += nary_size(root->children[i]);
    }
    return count;
}

int lcrs_size(LCRSNode *root) {
    if (root == NULL) return 0;
    return 1 + lcrs_size(root->first_child) + lcrs_size(root->next_sibling);
}
```

### Contar hojas

Una hoja en un árbol n-ario es un nodo sin hijos.

```c
int nary_count_leaves(NaryNode *root) {
    if (root == NULL) return 0;
    if (root->num_children == 0) return 1;
    int count = 0;
    for (int i = 0; i < root->num_children; i++) {
        count += nary_count_leaves(root->children[i]);
    }
    return count;
}

int lcrs_count_leaves(LCRSNode *root) {
    if (root == NULL) return 0;
    if (root->first_child == NULL) {
        // es hoja — contarla + contar hermanos
        return 1 + lcrs_count_leaves(root->next_sibling);
    }
    return lcrs_count_leaves(root->first_child)
         + lcrs_count_leaves(root->next_sibling);
}
```

### Buscar un valor

```c
NaryNode *nary_search(NaryNode *root, int target) {
    if (root == NULL) return NULL;
    if (root->data == target) return root;
    for (int i = 0; i < root->num_children; i++) {
        NaryNode *found = nary_search(root->children[i], target);
        if (found != NULL) return found;
    }
    return NULL;
}
```

---

## Liberar memoria

En C, liberar un árbol n-ario requiere liberar recursivamente todos los hijos
antes de liberar el nodo (postorden).

```c
// Array de hijos
void nary_free(NaryNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < root->num_children; i++) {
        nary_free(root->children[i]);
    }
    free(root->children);  // liberar el array de punteros
    free(root);
}

// LCRS
void lcrs_free(LCRSNode *root) {
    if (root == NULL) return;
    lcrs_free(root->first_child);
    lcrs_free(root->next_sibling);
    free(root);
}
```

En Rust, `Drop` se ejecuta automáticamente. Para árboles muy anchos (un nodo con
miles de hijos en la representación Vec), la recursión de `Drop` baja un nivel por
hijo, así que la profundidad de la pila de destrucción es la **altura** del árbol,
no su tamaño. Normalmente seguro.

---

## Equivalencia formal: n-ario ↔ binario

La transformación LCRS establece una **biyección** entre:

- Bosques ordenados (colecciones de árboles n-arios ordenados)
- Árboles binarios

Esta correspondencia, formalizada por Knuth, tiene consecuencias importantes:

| Concepto en árbol n-ario | Equivalente en árbol binario LCRS |
|--------------------------|-----------------------------------|
| Primer hijo | Hijo izquierdo |
| Siguiente hermano | Hijo derecho |
| Preorden n-ario | Preorden binario |
| Postorden n-ario | Inorden binario |
| Hoja (sin hijos) | Nodo sin hijo izquierdo |
| Número de hijos | Longitud de la cadena derecha desde el hijo izquierdo |

La correspondencia **postorden n-ario = inorden binario** es particularmente
elegante. Si tienes un algoritmo de inorden para árboles binarios, puedes
aplicarlo directamente al LCRS para obtener el postorden del árbol general.

### Conversión programática

```c
// Convertir array-de-hijos a LCRS
LCRSNode *nary_to_lcrs(NaryNode *root) {
    if (root == NULL) return NULL;

    LCRSNode *node = lcrs_create(root->data);

    // convertir primer hijo
    if (root->num_children > 0) {
        node->first_child = nary_to_lcrs(root->children[0]);

        // enlazar hermanos
        LCRSNode *prev = node->first_child;
        for (int i = 1; i < root->num_children; i++) {
            prev->next_sibling = nary_to_lcrs(root->children[i]);
            prev = prev->next_sibling;
        }
    }

    return node;
}

// Convertir LCRS a array-de-hijos
NaryNode *lcrs_to_nary(LCRSNode *root) {
    if (root == NULL) return NULL;

    NaryNode *node = nary_create(root->data);

    // recorrer cadena de hijos
    LCRSNode *child = root->first_child;
    while (child != NULL) {
        nary_add_child(node, lcrs_to_nary(child));
        child = child->next_sibling;
    }

    return node;
}
```

---

## Impresión jerárquica

Para depuración, es útil imprimir el árbol con indentación que refleje la
estructura:

```c
void nary_print(NaryNode *root, int depth) {
    if (root == NULL) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%d\n", root->data);
    for (int i = 0; i < root->num_children; i++) {
        nary_print(root->children[i], depth + 1);
    }
}
```

Salida para el árbol de ejemplo:

```
A
  B
    E
    F
  C
  D
    G
```

Con prefijos estilo `tree` de Unix:

```c
void nary_print_tree(NaryNode *root, char *prefix, int is_last) {
    if (root == NULL) return;

    printf("%s", prefix);
    printf("%s", is_last ? "└── " : "├── ");
    printf("%d\n", root->data);

    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s",
             prefix, is_last ? "    " : "│   ");

    for (int i = 0; i < root->num_children; i++) {
        nary_print_tree(root->children[i], new_prefix,
                        i == root->num_children - 1);
    }
}
```

Salida:

```
A
├── B
│   ├── E
│   └── F
├── C
└── D
    └── G
```

---

## Caso práctico: sistema de archivos simplificado

Un sistema de archivos es un árbol n-ario donde cada nodo es un directorio o
archivo. Los directorios tienen hijos; los archivos son hojas.

```c
typedef struct FSNode {
    char name[64];
    int is_dir;
    int size;                    // solo para archivos
    struct FSNode **children;
    int num_children;
    int capacity;
} FSNode;

// calcular tamano total recursivamente
int fs_total_size(FSNode *node) {
    if (!node->is_dir) return node->size;
    int total = 0;
    for (int i = 0; i < node->num_children; i++) {
        total += fs_total_size(node->children[i]);
    }
    return total;
}

// buscar archivo por nombre (DFS)
FSNode *fs_find(FSNode *root, const char *name) {
    if (root == NULL) return NULL;
    if (strcmp(root->name, name) == 0) return root;
    for (int i = 0; i < root->num_children; i++) {
        FSNode *found = fs_find(root->children[i], name);
        if (found != NULL) return found;
    }
    return NULL;
}

// listar contenido con profundidad (como find)
void fs_list(FSNode *root, int depth) {
    if (root == NULL) return;
    for (int i = 0; i < depth; i++) printf("  ");
    if (root->is_dir) {
        printf("[%s]\n", root->name);
    } else {
        printf("%s (%d bytes)\n", root->name, root->size);
    }
    for (int i = 0; i < root->num_children; i++) {
        fs_list(root->children[i], depth + 1);
    }
}
```

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ======== Representacion: array de hijos ========

typedef struct NaryNode {
    char data;
    struct NaryNode **children;
    int num_children;
    int capacity;
} NaryNode;

NaryNode *nary_create(char data) {
    NaryNode *node = malloc(sizeof(NaryNode));
    node->data = data;
    node->children = NULL;
    node->num_children = 0;
    node->capacity = 0;
    return node;
}

void nary_add_child(NaryNode *parent, NaryNode *child) {
    if (parent->num_children == parent->capacity) {
        int new_cap = parent->capacity == 0 ? 2 : parent->capacity * 2;
        parent->children = realloc(parent->children,
                                   new_cap * sizeof(NaryNode *));
        parent->capacity = new_cap;
    }
    parent->children[parent->num_children++] = child;
}

void nary_preorder(NaryNode *root) {
    if (root == NULL) return;
    printf("%c ", root->data);
    for (int i = 0; i < root->num_children; i++) {
        nary_preorder(root->children[i]);
    }
}

void nary_postorder(NaryNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < root->num_children; i++) {
        nary_postorder(root->children[i]);
    }
    printf("%c ", root->data);
}

void nary_bfs(NaryNode *root) {
    if (root == NULL) return;
    NaryNode *queue[100];
    int front = 0, rear = 0;
    queue[rear++] = root;
    while (front < rear) {
        NaryNode *node = queue[front++];
        printf("%c ", node->data);
        for (int i = 0; i < node->num_children; i++) {
            queue[rear++] = node->children[i];
        }
    }
}

int nary_height(NaryNode *root) {
    if (root == NULL) return -1;
    int max_h = -1;
    for (int i = 0; i < root->num_children; i++) {
        int h = nary_height(root->children[i]);
        if (h > max_h) max_h = h;
    }
    return max_h + 1;
}

int nary_size(NaryNode *root) {
    if (root == NULL) return 0;
    int count = 1;
    for (int i = 0; i < root->num_children; i++) {
        count += nary_size(root->children[i]);
    }
    return count;
}

int nary_count_leaves(NaryNode *root) {
    if (root == NULL) return 0;
    if (root->num_children == 0) return 1;
    int count = 0;
    for (int i = 0; i < root->num_children; i++) {
        count += nary_count_leaves(root->children[i]);
    }
    return count;
}

void nary_print_tree(NaryNode *root, const char *prefix, int is_last) {
    if (root == NULL) return;
    printf("%s%s%c\n", prefix, is_last ? "└── " : "├── ", root->data);
    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s",
             prefix, is_last ? "    " : "│   ");
    for (int i = 0; i < root->num_children; i++) {
        nary_print_tree(root->children[i], new_prefix,
                        i == root->num_children - 1);
    }
}

void nary_free(NaryNode *root) {
    if (root == NULL) return;
    for (int i = 0; i < root->num_children; i++) {
        nary_free(root->children[i]);
    }
    free(root->children);
    free(root);
}

// ======== Representacion: LCRS ========

typedef struct LCRSNode {
    char data;
    struct LCRSNode *first_child;
    struct LCRSNode *next_sibling;
} LCRSNode;

LCRSNode *lcrs_create(char data) {
    LCRSNode *node = malloc(sizeof(LCRSNode));
    node->data = data;
    node->first_child = NULL;
    node->next_sibling = NULL;
    return node;
}

void lcrs_add_child(LCRSNode *parent, LCRSNode *child) {
    if (parent->first_child == NULL) {
        parent->first_child = child;
    } else {
        LCRSNode *s = parent->first_child;
        while (s->next_sibling != NULL) s = s->next_sibling;
        s->next_sibling = child;
    }
}

void lcrs_preorder(LCRSNode *root) {
    if (root == NULL) return;
    printf("%c ", root->data);
    lcrs_preorder(root->first_child);
    lcrs_preorder(root->next_sibling);
}

void lcrs_postorder(LCRSNode *root) {
    if (root == NULL) return;
    lcrs_postorder(root->first_child);
    printf("%c ", root->data);
    lcrs_postorder(root->next_sibling);
}

void lcrs_bfs(LCRSNode *root) {
    if (root == NULL) return;
    LCRSNode *queue[100];
    int front = 0, rear = 0;
    queue[rear++] = root;
    while (front < rear) {
        LCRSNode *node = queue[front++];
        printf("%c ", node->data);
        LCRSNode *child = node->first_child;
        while (child != NULL) {
            queue[rear++] = child;
            child = child->next_sibling;
        }
    }
}

int lcrs_height(LCRSNode *root) {
    if (root == NULL) return -1;
    int child_h = lcrs_height(root->first_child);
    int sibling_h = lcrs_height(root->next_sibling);
    int via_child = child_h + 1;
    return via_child > sibling_h ? via_child : sibling_h;
}

int lcrs_size(LCRSNode *root) {
    if (root == NULL) return 0;
    return 1 + lcrs_size(root->first_child) + lcrs_size(root->next_sibling);
}

void lcrs_free(LCRSNode *root) {
    if (root == NULL) return;
    lcrs_free(root->first_child);
    lcrs_free(root->next_sibling);
    free(root);
}

// ======== Conversion ========

LCRSNode *nary_to_lcrs(NaryNode *root) {
    if (root == NULL) return NULL;
    LCRSNode *node = lcrs_create(root->data);
    if (root->num_children > 0) {
        node->first_child = nary_to_lcrs(root->children[0]);
        LCRSNode *prev = node->first_child;
        for (int i = 1; i < root->num_children; i++) {
            prev->next_sibling = nary_to_lcrs(root->children[i]);
            prev = prev->next_sibling;
        }
    }
    return node;
}

// ======== main ========

int main(void) {
    // Construir arbol:
    //        A
    //      / | \
    //     B  C  D
    //    /|     |
    //   E  F    G

    NaryNode *a = nary_create('A');
    NaryNode *b = nary_create('B');
    NaryNode *c = nary_create('C');
    NaryNode *d = nary_create('D');
    NaryNode *e = nary_create('E');
    NaryNode *f = nary_create('F');
    NaryNode *g = nary_create('G');

    nary_add_child(a, b);
    nary_add_child(a, c);
    nary_add_child(a, d);
    nary_add_child(b, e);
    nary_add_child(b, f);
    nary_add_child(d, g);

    printf("=== Array de hijos ===\n\n");

    printf("Estructura:\n");
    nary_print_tree(a, "", 1);

    printf("\nPreorden:  ");
    nary_preorder(a);

    printf("\nPostorden: ");
    nary_postorder(a);

    printf("\nBFS:       ");
    nary_bfs(a);

    printf("\n\nAltura: %d\n", nary_height(a));
    printf("Nodos:  %d\n", nary_size(a));
    printf("Hojas:  %d\n", nary_count_leaves(a));

    // Convertir a LCRS
    LCRSNode *lcrs_root = nary_to_lcrs(a);

    printf("\n=== LCRS (mismo arbol) ===\n\n");

    printf("Preorden:  ");
    lcrs_preorder(lcrs_root);

    printf("\nPostorden: ");
    lcrs_postorder(lcrs_root);

    printf("\nBFS:       ");
    lcrs_bfs(lcrs_root);

    printf("\n\nAltura: %d\n", lcrs_height(lcrs_root));
    printf("Nodos:  %d\n", lcrs_size(lcrs_root));

    // Verificar equivalencia
    printf("\n=== Verificacion ===\n");
    printf("Preorden coincide:  ambas dan A B E F C D G\n");
    printf("Postorden n-ario = inorden LCRS: E F B C G D A\n");
    printf("BFS coincide:       ambas dan A B C D E F G\n");

    nary_free(a);
    lcrs_free(lcrs_root);

    return 0;
}
```

### Salida esperada

```
=== Array de hijos ===

Estructura:
└── A
    ├── B
    │   ├── E
    │   └── F
    ├── C
    └── D
        └── G

Preorden:  A B E F C D G
Postorden: E F B C G D A
BFS:       A B C D E F G

Altura: 2
Nodos:  7
Hojas:  4

=== LCRS (mismo arbol) ===

Preorden:  A B E F C D G
Postorden: E F B C G D A
BFS:       A B C D E F G

Altura: 2
Nodos:  7

=== Verificacion ===
Preorden coincide:  ambas dan A B E F C D G
Postorden n-ario = inorden LCRS: E F B C G D A
BFS coincide:       ambas dan A B C D E F G
```

---

## Programa completo en Rust

```rust
use std::collections::VecDeque;

// ======== Representacion: Vec de hijos ========

struct NaryNode<T> {
    data: T,
    children: Vec<Box<NaryNode<T>>>,
}

impl<T> NaryNode<T> {
    fn new(data: T) -> Self {
        NaryNode { data, children: Vec::new() }
    }

    fn add_child(&mut self, child: NaryNode<T>) {
        self.children.push(Box::new(child));
    }
}

impl<T: std::fmt::Display> NaryNode<T> {
    fn preorder_vec(&self) -> Vec<String> {
        let mut result = vec![self.data.to_string()];
        for child in &self.children {
            result.extend(child.preorder_vec());
        }
        result
    }

    fn postorder_vec(&self) -> Vec<String> {
        let mut result = Vec::new();
        for child in &self.children {
            result.extend(child.postorder_vec());
        }
        result.push(self.data.to_string());
        result
    }

    fn bfs_vec(&self) -> Vec<String> {
        let mut result = Vec::new();
        let mut queue = VecDeque::new();
        queue.push_back(self);
        while let Some(node) = queue.pop_front() {
            result.push(node.data.to_string());
            for child in &node.children {
                queue.push_back(child);
            }
        }
        result
    }

    fn height(&self) -> i32 {
        self.children.iter()
            .map(|c| c.height())
            .max()
            .map_or(0, |h| h + 1)
    }

    fn size(&self) -> usize {
        1 + self.children.iter().map(|c| c.size()).sum::<usize>()
    }

    fn count_leaves(&self) -> usize {
        if self.children.is_empty() {
            1
        } else {
            self.children.iter().map(|c| c.count_leaves()).sum()
        }
    }

    fn print_tree(&self, prefix: &str, is_last: bool) {
        println!("{}{}{}", prefix, if is_last { "└── " } else { "├── " },
                 self.data);
        let new_prefix = format!("{}{}", prefix,
                                 if is_last { "    " } else { "│   " });
        for (i, child) in self.children.iter().enumerate() {
            child.print_tree(&new_prefix, i == self.children.len() - 1);
        }
    }
}

// ======== Representacion: LCRS ========

type Link<T> = Option<Box<LCRSNode<T>>>;

struct LCRSNode<T> {
    data: T,
    first_child: Link<T>,
    next_sibling: Link<T>,
}

impl<T> LCRSNode<T> {
    fn new(data: T) -> Self {
        LCRSNode { data, first_child: None, next_sibling: None }
    }
}

impl<T: std::fmt::Display> LCRSNode<T> {
    fn preorder_vec(&self) -> Vec<String> {
        let mut result = vec![self.data.to_string()];
        if let Some(ref child) = self.first_child {
            result.extend(child.preorder_vec());
        }
        if let Some(ref sibling) = self.next_sibling {
            result.extend(sibling.preorder_vec());
        }
        result
    }

    fn postorder_vec(&self) -> Vec<String> {
        let mut result = Vec::new();
        if let Some(ref child) = self.first_child {
            result.extend(child.postorder_vec());
        }
        result.push(self.data.to_string());
        if let Some(ref sibling) = self.next_sibling {
            result.extend(sibling.postorder_vec());
        }
        result
    }

    fn bfs_vec(&self) -> Vec<String> {
        let mut result = Vec::new();
        let mut queue: VecDeque<&LCRSNode<T>> = VecDeque::new();
        queue.push_back(self);
        while let Some(node) = queue.pop_front() {
            result.push(node.data.to_string());
            let mut child = &node.first_child;
            while let Some(ref c) = child {
                queue.push_back(c);
                child = &c.next_sibling;
            }
        }
        result
    }

    fn height(&self) -> i32 {
        let child_h = self.first_child.as_ref()
            .map_or(-1, |c| c.height());
        let sibling_h = self.next_sibling.as_ref()
            .map_or(-1, |s| s.height());
        let via_child = child_h + 1;
        via_child.max(sibling_h)
    }

    fn size(&self) -> usize {
        1 + self.first_child.as_ref().map_or(0, |c| c.size())
          + self.next_sibling.as_ref().map_or(0, |s| s.size())
    }
}

// ======== Conversion nary -> LCRS ========

fn nary_to_lcrs<T: Clone>(node: &NaryNode<T>) -> Box<LCRSNode<T>> {
    let mut lcrs = Box::new(LCRSNode::new(node.data.clone()));

    if !node.children.is_empty() {
        lcrs.first_child = Some(nary_to_lcrs(&node.children[0]));
        let mut prev = lcrs.first_child.as_mut().unwrap();
        for child in &node.children[1..] {
            prev.next_sibling = Some(nary_to_lcrs(child));
            prev = prev.next_sibling.as_mut().unwrap();
        }
    }

    lcrs
}

fn main() {
    //        A
    //      / | \
    //     B  C  D
    //    /|     |
    //   E  F    G

    let mut a = NaryNode::new('A');
    let mut b = NaryNode::new('B');
    let d_child = NaryNode::new('G');
    let mut d = NaryNode::new('D');
    d.add_child(d_child);

    b.add_child(NaryNode::new('E'));
    b.add_child(NaryNode::new('F'));

    a.add_child(b);
    a.add_child(NaryNode::new('C'));
    a.add_child(d);

    println!("=== Vec de hijos ===\n");
    a.print_tree("", true);

    println!("\nPreorden:  {}", a.preorder_vec().join(" "));
    println!("Postorden: {}", a.postorder_vec().join(" "));
    println!("BFS:       {}", a.bfs_vec().join(" "));

    println!("\nAltura: {}", a.height());
    println!("Nodos:  {}", a.size());
    println!("Hojas:  {}", a.count_leaves());

    // Convertir a LCRS
    let lcrs = nary_to_lcrs(&a);

    println!("\n=== LCRS (mismo arbol) ===\n");
    println!("Preorden:  {}", lcrs.preorder_vec().join(" "));
    println!("Postorden: {}", lcrs.postorder_vec().join(" "));
    println!("BFS:       {}", lcrs.bfs_vec().join(" "));

    println!("\nAltura: {}", lcrs.height());
    println!("Nodos:  {}", lcrs.size());
}
```

### Salida

```
=== Vec de hijos ===

└── A
    ├── B
    │   ├── E
    │   └── F
    ├── C
    └── D
        └── G

Preorden:  A B E F C D G
Postorden: E F B C G D A
BFS:       A B C D E F G

Altura: 2
Nodos:  7
Hojas:  4

=== LCRS (mismo arbol) ===

Preorden:  A B E F C D G
Postorden: E F B C G D A
BFS:       A B C D E F G

Altura: 2
Nodos:  7
```

---

## Ejercicios

### Ejercicio 1 — Trazar preorden y postorden

Dado el árbol:

```
        1
      / | \
     2  3  4
    /|  |
   5 6  7
       /|\
      8 9 10
```

Escribe el recorrido preorden y postorden antes de ejecutar.

<details>
<summary>Verificar</summary>

**Preorden** (visitar, luego hijos izq→der):
1, 2, 5, 6, 3, 7, 8, 9, 10, 4

**Postorden** (hijos izq→der, luego visitar):
5, 6, 2, 8, 9, 10, 7, 3, 4, 1

**BFS** (por niveles):
1, 2, 3, 4, 5, 6, 7, 8, 9, 10
</details>

### Ejercicio 2 — Dibujar LCRS

Convierte el árbol del Ejercicio 1 a representación LCRS (hijo-izquierdo /
hermano-derecho). Dibuja el árbol binario resultante.

<details>
<summary>Verificar</summary>

Paso 1: cada nodo apunta a su primer hijo (↓) y siguiente hermano (→):

```
1
↓
2 → 3 → 4
↓   ↓
5→6 7
    ↓
    8→9→10
```

Paso 2: como árbol binario (left = first_child, right = next_sibling):

```
         1
        /
       2
      / \
     5   3
      \  / \
       6 7  4
        /
       8
        \
         9
          \
          10
```

Verificación: preorden del binario = 1,2,5,6,3,7,8,9,10,4 ✓ (coincide con preorden n-ario).
</details>

### Ejercicio 3 — Grado máximo

Implementa `nary_max_degree(NaryNode *root)` que retorne el **grado** máximo
(número máximo de hijos que tiene cualquier nodo) del árbol. ¿Cuál es el grado
del árbol del Ejercicio 1?

<details>
<summary>Verificar</summary>

```c
int nary_max_degree(NaryNode *root) {
    if (root == NULL) return 0;
    int max_deg = root->num_children;
    for (int i = 0; i < root->num_children; i++) {
        int d = nary_max_degree(root->children[i]);
        if (d > max_deg) max_deg = d;
    }
    return max_deg;
}
```

Para el árbol del Ejercicio 1:
- Nodo 1: 3 hijos (2, 3, 4)
- Nodo 7: 3 hijos (8, 9, 10)
- Nodo 2: 2 hijos (5, 6)
- Resto: 0 o 1 hijo

Grado máximo = **3**.
</details>

### Ejercicio 4 — Profundidad de un nodo

Implementa `nary_depth(NaryNode *root, int target)` que retorne la profundidad
(distancia desde la raíz) del nodo con valor `target`, o $-1$ si no existe.

<details>
<summary>Verificar</summary>

```c
int nary_depth(NaryNode *root, int target) {
    if (root == NULL) return -1;
    if (root->data == target) return 0;
    for (int i = 0; i < root->num_children; i++) {
        int d = nary_depth(root->children[i], target);
        if (d != -1) return d + 1;
    }
    return -1;
}
```

En el árbol del Ejercicio 1: `nary_depth(root, 9)` → raíz(1) → 3 → 7 → 9,
profundidad = **3**.
</details>

### Ejercicio 5 — Camino raíz a nodo

Implementa una función que encuentre y devuelva el camino desde la raíz hasta
un nodo dado. Usa un array auxiliar y backtracking.

<details>
<summary>Verificar</summary>

```c
int nary_path(NaryNode *root, int target, int path[], int depth) {
    if (root == NULL) return 0;
    path[depth] = root->data;
    if (root->data == target) {
        for (int i = 0; i <= depth; i++) {
            printf("%d%s", path[i], i < depth ? " -> " : "\n");
        }
        return 1;
    }
    for (int i = 0; i < root->num_children; i++) {
        if (nary_path(root->children[i], target, path, depth + 1))
            return 1;
    }
    return 0;  // backtrack
}
```

En Rust:

```rust
fn path_to(&self, target: &T) -> Option<Vec<&T>>
where T: PartialEq
{
    if self.data == *target {
        return Some(vec![&self.data]);
    }
    for child in &self.children {
        if let Some(mut p) = child.path_to(target) {
            p.insert(0, &self.data);
            return Some(p);
        }
    }
    None
}
```

`path_to(9)` → `Some([1, 3, 7, 9])`.
</details>

### Ejercicio 6 — LCRS: contar hijos

En la representación LCRS, escribe `lcrs_num_children(LCRSNode *node)` que
cuente cuántos hijos tiene un nodo dado. Compara la complejidad con la versión
array.

<details>
<summary>Verificar</summary>

```c
int lcrs_num_children(LCRSNode *node) {
    if (node == NULL || node->first_child == NULL) return 0;
    int count = 1;
    LCRSNode *sibling = node->first_child;
    while (sibling->next_sibling != NULL) {
        count++;
        sibling = sibling->next_sibling;
    }
    return count;
}
```

Complejidad: $O(k)$ donde $k$ es el número de hijos — hay que recorrer toda la
cadena de hermanos. En la versión array, es $O(1)$ porque `num_children` está
almacenado.

Este es el costo principal de LCRS: el acceso a la "anchura" del nodo es lineal.
</details>

### Ejercicio 7 — Nodos en un nivel dado

Implementa `nary_count_at_level(NaryNode *root, int level)` que cuente cuántos
nodos hay en el nivel especificado (raíz = nivel 0). Resuélvelo recursivamente
y con BFS. ¿Cuántos nodos hay en el nivel 2 del Ejercicio 1?

<details>
<summary>Verificar</summary>

Recursivo:

```c
int nary_count_at_level(NaryNode *root, int level) {
    if (root == NULL) return 0;
    if (level == 0) return 1;
    int count = 0;
    for (int i = 0; i < root->num_children; i++) {
        count += nary_count_at_level(root->children[i], level - 1);
    }
    return count;
}
```

Con BFS: usar la técnica de separación por niveles (contador por nivel).

Nivel 0: {1} → 1 nodo
Nivel 1: {2, 3, 4} → 3 nodos
Nivel 2: {5, 6, 7} → **3 nodos**
Nivel 3: {8, 9, 10} → 3 nodos
</details>

### Ejercicio 8 — Espejo n-ario

Implementa `nary_mirror(NaryNode *root)` que invierta el orden de los hijos de
cada nodo (recursivamente). Escribe el preorden del árbol espejado del Ejercicio 1.

<details>
<summary>Verificar</summary>

```c
void nary_mirror(NaryNode *root) {
    if (root == NULL) return;
    // invertir array de hijos
    for (int i = 0; i < root->num_children / 2; i++) {
        int j = root->num_children - 1 - i;
        NaryNode *tmp = root->children[i];
        root->children[i] = root->children[j];
        root->children[j] = tmp;
    }
    // recurrir en cada hijo
    for (int i = 0; i < root->num_children; i++) {
        nary_mirror(root->children[i]);
    }
}
```

Árbol espejado:

```
        1
      / | \
     4  3  2
        |  |\
        7  6 5
      / | \
    10  9  8
```

Preorden espejado: **1, 4, 3, 7, 10, 9, 8, 2, 6, 5**

En Rust: `self.children.reverse()` + recurrir.
</details>

### Ejercicio 9 — Ancestro común más bajo (LCA)

Implementa LCA para árboles n-arios: dado un árbol y dos valores $a$, $b$,
encuentra el ancestro común más profundo. Usa el enfoque de encontrar los
caminos y comparar.

<details>
<summary>Verificar</summary>

Estrategia: encontrar el camino a cada nodo, luego comparar los caminos y
devolver el último valor común.

```c
// reutilizar nary_find_path que llena un array
int find_path(NaryNode *root, int target, int path[], int *len) {
    if (root == NULL) return 0;
    path[(*len)++] = root->data;
    if (root->data == target) return 1;
    for (int i = 0; i < root->num_children; i++) {
        if (find_path(root->children[i], target, path, len))
            return 1;
    }
    (*len)--;  // backtrack
    return 0;
}

int nary_lca(NaryNode *root, int a, int b) {
    int path_a[100], path_b[100];
    int len_a = 0, len_b = 0;

    find_path(root, a, path_a, &len_a);
    find_path(root, b, path_b, &len_b);

    int lca = -1;
    for (int i = 0; i < len_a && i < len_b; i++) {
        if (path_a[i] == path_b[i]) lca = path_a[i];
        else break;
    }
    return lca;
}
```

Ejemplo: `lca(5, 9)` en el Ejercicio 1:
- Camino a 5: [1, 2, 5]
- Camino a 9: [1, 3, 7, 9]
- Comparar: 1 == 1 ✓, 2 ≠ 3 → LCA = **1**

`lca(8, 10)`:
- Camino a 8: [1, 3, 7, 8]
- Camino a 10: [1, 3, 7, 10]
- Comparar: 1==1, 3==3, 7==7, 8≠10 → LCA = **7**
</details>

### Ejercicio 10 — Serialización y deserialización

Diseña un formato de serialización para árboles n-arios. Una opción: preorden
con marcador de "subir nivel". Implementa `serialize` y `deserialize`.

<details>
<summary>Verificar</summary>

Formato: cada nodo se serializa como su valor seguido de sus hijos, terminando
con un marcador especial `)` que indica "fin de hijos de este nodo".

Ejemplo: `1(2(5()6())3(7(8()9()10()))4())` → simplificado como secuencia:
`[1, 2, 5, ), 6, ), ), 3, 7, 8, ), 9, ), 10, ), ), ), 4, ), )]`

Implementación con pila:

```c
#define END_MARKER -1

void nary_serialize(NaryNode *root, int out[], int *pos) {
    if (root == NULL) return;
    out[(*pos)++] = root->data;
    for (int i = 0; i < root->num_children; i++) {
        nary_serialize(root->children[i], out, pos);
    }
    out[(*pos)++] = END_MARKER;  // fin de hijos
}

NaryNode *nary_deserialize(int data[], int *pos) {
    if (data[*pos] == END_MARKER) {
        (*pos)++;  // consumir marcador
        return NULL;
    }
    NaryNode *node = nary_create(data[(*pos)++]);
    while (data[*pos] != END_MARKER) {
        nary_add_child(node, nary_deserialize(data, pos));
    }
    (*pos)++;  // consumir END_MARKER de este nodo
    return node;
}
```

En Rust:

```rust
fn serialize(node: &NaryNode<i32>) -> Vec<Option<i32>> {
    let mut result = vec![Some(node.data)];
    for child in &node.children {
        result.extend(serialize(child));
    }
    result.push(None);  // end marker
    result
}

fn deserialize(data: &[Option<i32>], pos: &mut usize) -> Option<NaryNode<i32>> {
    match data.get(*pos)? {
        None => { *pos += 1; None }
        Some(val) => {
            *pos += 1;
            let mut node = NaryNode::new(*val);
            while data.get(*pos) != Some(&None) {
                if let Some(child) = deserialize(data, pos) {
                    node.add_child(child);
                }
            }
            *pos += 1;  // consume None marker
            Some(node)
        }
    }
}
```

La serialización del Ejercicio 1 produce:
`[1, 2, 5, ∅, 6, ∅, ∅, 3, 7, 8, ∅, 9, ∅, 10, ∅, ∅, ∅, 4, ∅, ∅]`
(20 elementos para 10 nodos: cada nodo produce su valor + un marcador de cierre).
</details>
