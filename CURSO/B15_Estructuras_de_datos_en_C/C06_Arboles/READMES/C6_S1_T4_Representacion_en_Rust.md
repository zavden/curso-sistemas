# Representación en Rust

## Dos estilos

Rust ofrece dos formas naturales de representar un árbol binario:

1. **Struct + Option<Box>**: estilo imperativo, análogo a C con punteros.
2. **Enum recursivo**: estilo funcional, análogo a Haskell/ML.

Ambos son safe y no requieren `unsafe`.  A diferencia de las listas dobles,
los árboles binarios encajan bien en el modelo de ownership de Rust: cada nodo
tiene **un único dueño** (su padre), y los hijos son owned por su padre via
`Box`.

---

## Estilo 1: struct con Option<Box<TreeNode<T>>>

### Estructura

```rust
type Tree<T> = Option<Box<TreeNode<T>>>;

struct TreeNode<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}
```

`Tree<T>` es un alias de tipo: `None` representa el árbol vacío (análogo a
`NULL` en C), `Some(Box<TreeNode>)` representa un nodo con sus subárboles.

### Crear nodos

```rust
impl<T> TreeNode<T> {
    fn new(data: T) -> Tree<T> {
        Some(Box::new(TreeNode {
            data,
            left: None,
            right: None,
        }))
    }

    fn with_children(data: T, left: Tree<T>, right: Tree<T>) -> Tree<T> {
        Some(Box::new(TreeNode { data, left, right }))
    }
}
```

### Construir un árbol

```rust
fn build_sample() -> Tree<i32> {
    //        10
    //       /  \
    //      5    15
    //     / \     \
    //    3   7    20

    TreeNode::with_children(
        10,
        TreeNode::with_children(
            5,
            TreeNode::new(3),
            TreeNode::new(7),
        ),
        TreeNode::with_children(
            15,
            None,
            TreeNode::new(20),
        ),
    )
}
```

La construcción es bottom-up: primero se crean las hojas, luego los nodos
internos que las contienen.  Cada `Box` toma ownership del subárbol —
la cadena de ownership es `root → children → grandchildren → ...`.

### Operaciones

```rust
fn count<T>(tree: &Tree<T>) -> usize {
    match tree {
        None => 0,
        Some(node) => 1 + count(&node.left) + count(&node.right),
    }
}

fn height<T>(tree: &Tree<T>) -> i32 {
    match tree {
        None => -1,
        Some(node) => 1 + height(&node.left).max(height(&node.right)),
    }
}

fn count_leaves<T>(tree: &Tree<T>) -> usize {
    match tree {
        None => 0,
        Some(node) if node.left.is_none() && node.right.is_none() => 1,
        Some(node) => count_leaves(&node.left) + count_leaves(&node.right),
    }
}

fn sum(tree: &Tree<i32>) -> i32 {
    match tree {
        None => 0,
        Some(node) => node.data + sum(&node.left) + sum(&node.right),
    }
}
```

El patrón `match` reemplaza el `if (!root) return` de C.  Las funciones
toman `&Tree<T>` (referencia) para no consumir el árbol.

### Buscar

```rust
fn contains<T: PartialEq>(tree: &Tree<T>, value: &T) -> bool {
    match tree {
        None => false,
        Some(node) => {
            &node.data == value
            || contains(&node.left, value)
            || contains(&node.right, value)
        }
    }
}
```

Short-circuit con `||` — si lo encuentra en el subárbol izquierdo, no
recorre el derecho.

### Visualización

```rust
fn print_tree<T: std::fmt::Display>(tree: &Tree<T>, depth: usize) {
    if let Some(node) = tree {
        print_tree(&node.right, depth + 1);
        println!("{}{}", "    ".repeat(depth), node.data);
        print_tree(&node.left, depth + 1);
    }
}
```

---

## Estilo 2: enum recursivo

### Estructura

```rust
enum Tree<T> {
    Empty,
    Node {
        data: T,
        left: Box<Tree<T>>,
        right: Box<Tree<T>>,
    },
}
```

A diferencia del estilo struct donde `None` es el árbol vacío, aquí
`Tree::Empty` es una variante explícita del enum.  El `Box` es necesario
porque el enum es recursivo — sin `Box`, el tamaño sería infinito.

### Comparación de representación en memoria

```
Estilo struct:
  Tree<T> = Option<Box<TreeNode<T>>>
  None → 0 bytes (null pointer optimization)
  Some → 8 bytes (puntero al heap)

  TreeNode en heap: data + left (8 bytes) + right (8 bytes)

Estilo enum:
  Tree<T> = Empty | Node { data, left: Box<Tree>, right: Box<Tree> }
  Empty → discriminante (8 bytes, alineación)
  Node → discriminante + data + left (8 bytes) + right (8 bytes)

  Box<Tree> → puntero al heap → Tree en heap (puede ser Empty o Node)
```

El estilo struct es más eficiente: `Option<Box<T>>` tiene **null pointer
optimization** (NPO) — `None` se representa como un puntero nulo, sin
discriminante extra.  El enum siempre necesita el discriminante.

Además, en el estilo enum, un `Empty` dentro de un `Box<Tree>` sigue
ocupando espacio en el heap (el `Box` alloca aunque solo contenga un
discriminante).  En el estilo struct, `None` no alloca nada.

### Crear nodos

```rust
use Tree::*;

impl<T> Tree<T> {
    fn leaf(data: T) -> Self {
        Node {
            data,
            left: Box::new(Empty),
            right: Box::new(Empty),
        }
    }

    fn node(data: T, left: Tree<T>, right: Tree<T>) -> Self {
        Node {
            data,
            left: Box::new(left),
            right: Box::new(right),
        }
    }
}
```

### Construir

```rust
fn build_sample() -> Tree<i32> {
    Tree::node(
        10,
        Tree::node(
            5,
            Tree::leaf(3),
            Tree::leaf(7),
        ),
        Tree::node(
            15,
            Empty,
            Tree::leaf(20),
        ),
    )
}
```

### Operaciones

```rust
impl<T> Tree<T> {
    fn count(&self) -> usize {
        match self {
            Empty => 0,
            Node { left, right, .. } => 1 + left.count() + right.count(),
        }
    }

    fn height(&self) -> i32 {
        match self {
            Empty => -1,
            Node { left, right, .. } => 1 + left.height().max(right.height()),
        }
    }

    fn is_empty(&self) -> bool {
        matches!(self, Empty)
    }

    fn is_leaf(&self) -> bool {
        matches!(self, Node { left, right, .. }
                 if left.is_empty() && right.is_empty())
    }

    fn count_leaves(&self) -> usize {
        match self {
            Empty => 0,
            node if node.is_leaf() => 1,
            Node { left, right, .. } => left.count_leaves() + right.count_leaves(),
        }
    }
}
```

El estilo enum permite `impl` directamente sobre `Tree<T>` — las funciones
son métodos.  En el estilo struct, las funciones toman `&Tree<T>` (que es
`&Option<Box<TreeNode<T>>>`) y no pueden ser métodos de `Tree<T>` (es un
alias).

### Funcionales: map, fold

El estilo enum se presta a operaciones funcionales:

```rust
impl<T: Clone> Tree<T> {
    fn map<U, F: Fn(&T) -> U>(&self, f: &F) -> Tree<U> {
        match self {
            Empty => Empty,
            Node { data, left, right } => Tree::node(
                f(data),
                left.map(f),
                right.map(f),
            ),
        }
    }
}

impl Tree<i32> {
    fn sum(&self) -> i32 {
        match self {
            Empty => 0,
            Node { data, left, right } => data + left.sum() + right.sum(),
        }
    }

    fn fold<U, F: Fn(U, &i32, U) -> U>(&self, init: U, f: &F) -> U
    where U: Clone
    {
        match self {
            Empty => init.clone(),
            Node { data, left, right } => {
                let l = left.fold(init.clone(), f);
                let r = right.fold(init, f);
                f(l, data, r)
            }
        }
    }
}
```

```rust
// Uso de fold para sumar
let tree = build_sample();
let sum = tree.fold(0, &|l, val, r| l + val + r);   // 60

// Uso de fold para contar
let count = tree.fold(0, &|l, _, r| l + 1 + r);     // 6

// Uso de fold para altura
let height = tree.fold(-1_i32, &|l, _, r| 1 + l.max(r));  // 2
```

`fold` generaliza toda operación recursiva sobre el árbol: el caso base
(`init`) y la combinación de los resultados de los subárboles con el dato
del nodo.

---

## Comparación de estilos

| Aspecto | Struct + Option<Box> | Enum recursivo |
|---------|---------------------|---------------|
| Árbol vacío | `None` | `Tree::Empty` |
| Nodo | `Some(Box::new(TreeNode { ... }))` | `Tree::Node { ... }` |
| Null pointer optimization | Sí — `None` = 0 bytes | No — discriminante siempre |
| Allocation para vacío | Ninguna | `Box::new(Empty)` alloca en heap |
| Métodos | Funciones libres con `&Tree<T>` | `impl Tree<T>` — métodos |
| Estilo | Imperativo (similar a C) | Funcional (similar a Haskell) |
| Pattern matching | `match tree { None => ..., Some(n) => ... }` | `match self { Empty => ..., Node { ... } => ... }` |
| Uso idiomático | Más común en producción | Más común en aprendizaje |
| Eficiencia | Mejor (NPO, sin alloc para vacío) | Peor (discriminante, alloc para Empty) |
| Ergonomía para BST | Buena (take/replace para mutación) | Buena (consume y reconstruye) |

### Recomendación

- **Para producción / BST mutables**: estilo struct (`Option<Box<TreeNode<T>>>`).
  Más eficiente, patrón `take()` para mutación, idéntico al patrón de listas.
- **Para aprendizaje / algoritmos funcionales**: estilo enum.  Más expresivo,
  `match` más claro, `map`/`fold` naturales.

---

## Drop automático

A diferencia de las listas enlazadas, el `Drop` por defecto de Rust funciona
bien para árboles **razonablemente balanceados**:

```rust
// Drop por defecto: recursivo
// root se destruye → destruye left (Box) → destruye su left → ...
// Profundidad de pila = altura del árbol
```

Para un árbol balanceado de $10^6$ nodos, la altura es ~20 — la pila aguanta
sin problema.

Para un árbol **degenerado** (altura $= n - 1$), el Drop recursivo puede
desbordar la pila.  Solución: Drop iterativo (igual que en listas):

```rust
impl<T> Drop for TreeNode<T> {
    fn drop(&mut self) {
        // Iterar por la rama más larga para evitar recursión profunda
        let mut stack = Vec::new();
        if let Some(left) = self.left.take() {
            stack.push(left);
        }
        if let Some(right) = self.right.take() {
            stack.push(right);
        }
        while let Some(mut node) = stack.pop() {
            if let Some(left) = node.left.take() {
                stack.push(left);
            }
            if let Some(right) = node.right.take() {
                stack.push(right);
            }
            // node se destruye aquí, pero left y right ya son None
        }
    }
}
```

`take()` extrae los hijos antes de que el Drop recursivo los procese.
La pila explícita (`Vec`) vive en el heap — sin límite de profundidad.

En la práctica, solo es necesario si el árbol puede ser degenerado con
miles de nodos.  Para árboles balanceados (BST, AVL, heaps), el Drop
por defecto es suficiente.

---

## Comparación con C

| Aspecto | C | Rust (struct) | Rust (enum) |
|---------|---|--------------|-------------|
| Crear nodo | `malloc` + init | `Box::new(TreeNode { ... })` | `Tree::Node { ... }` |
| Árbol vacío | `NULL` | `None` | `Empty` |
| Acceso a hijo | `root->left` | `node.left` (dentro de match) | `left` (destructuring) |
| Liberar | `free` recursivo (postorden) | Automático (Drop) | Automático (Drop) |
| NULL check | `if (!root)` — fácil de olvidar | `match` — compilador obliga | `match` — compilador obliga |
| Genéricos | `void *` | `<T>` | `<T>` |
| Ownership | Manual (quién hace free?) | Claro (Box = único dueño) | Claro (Box = único dueño) |
| Árboles binarios | Natural | Natural | Natural |

Los árboles son la estructura donde Rust es casi tan ergonómico como C:
la jerarquía padre→hijos se mapea directamente a la cadena de ownership
`Box`.  No hay el problema de múltiples dueños que complica las listas
dobles.

---

## Programa completo

```rust
// ── Estilo struct ──

type STree<T> = Option<Box<SNode<T>>>;

struct SNode<T> {
    data: T,
    left: STree<T>,
    right: STree<T>,
}

impl<T> SNode<T> {
    fn new(data: T) -> STree<T> {
        Some(Box::new(SNode { data, left: None, right: None }))
    }

    fn with(data: T, left: STree<T>, right: STree<T>) -> STree<T> {
        Some(Box::new(SNode { data, left, right }))
    }
}

fn s_count<T>(tree: &STree<T>) -> usize {
    match tree {
        None => 0,
        Some(n) => 1 + s_count(&n.left) + s_count(&n.right),
    }
}

fn s_height<T>(tree: &STree<T>) -> i32 {
    match tree {
        None => -1,
        Some(n) => 1 + s_height(&n.left).max(s_height(&n.right)),
    }
}

fn s_print<T: std::fmt::Display>(tree: &STree<T>, depth: usize) {
    if let Some(n) = tree {
        s_print(&n.right, depth + 1);
        println!("{}{}", "    ".repeat(depth), n.data);
        s_print(&n.left, depth + 1);
    }
}

// ── Estilo enum ──

enum ETree<T> {
    Empty,
    Node { data: T, left: Box<ETree<T>>, right: Box<ETree<T>> },
}

use ETree::*;

impl<T> ETree<T> {
    fn leaf(data: T) -> Self {
        Node { data, left: Box::new(Empty), right: Box::new(Empty) }
    }

    fn node(data: T, left: ETree<T>, right: ETree<T>) -> Self {
        Node { data, left: Box::new(left), right: Box::new(right) }
    }

    fn count(&self) -> usize {
        match self {
            Empty => 0,
            Node { left, right, .. } => 1 + left.count() + right.count(),
        }
    }

    fn height(&self) -> i32 {
        match self {
            Empty => -1,
            Node { left, right, .. } => 1 + left.height().max(right.height()),
        }
    }
}

impl<T: std::fmt::Display> ETree<T> {
    fn print(&self, depth: usize) {
        if let Node { data, left, right } = self {
            right.print(depth + 1);
            println!("{}{}", "    ".repeat(depth), data);
            left.print(depth + 1);
        }
    }
}

// ── Main ──

fn main() {
    //        10
    //       /  \
    //      5    15
    //     / \     \
    //    3   7    20

    // Estilo struct
    println!("=== Struct style ===");
    let st = SNode::with(10,
        SNode::with(5, SNode::new(3), SNode::new(7)),
        SNode::with(15, None, SNode::new(20)),
    );
    s_print(&st, 0);
    println!("Nodes:  {}", s_count(&st));
    println!("Height: {}", s_height(&st));

    // Estilo enum
    println!("\n=== Enum style ===");
    let et = ETree::node(10,
        ETree::node(5, ETree::leaf(3), ETree::leaf(7)),
        ETree::node(15, Empty, ETree::leaf(20)),
    );
    et.print(0);
    println!("Nodes:  {}", et.count());
    println!("Height: {}", et.height());
}
```

Salida (idéntica para ambos):

```
=== Struct style ===
        20
    15
10
        7
    5
        3
Nodes:  6
Height: 2

=== Enum style ===
        20
    15
10
        7
    5
        3
Nodes:  6
Height: 2
```

---

## Ejercicios

### Ejercicio 1 — Construir con struct

Construye el siguiente árbol usando el estilo struct (`Option<Box<TreeNode>>`):

```
        1
       / \
      2   3
         / \
        4   5
```

<details>
<summary>Código</summary>

```rust
let tree = SNode::with(1,
    SNode::new(2),
    SNode::with(3, SNode::new(4), SNode::new(5)),
);
```

`SNode::new(2)` crea una hoja (left=None, right=None).
`SNode::with` crea un nodo con hijos explícitos.
</details>

### Ejercicio 2 — Construir con enum

Construye el mismo árbol usando el estilo enum.  ¿Cuántos `Box::new(Empty)`
se crean?

<details>
<summary>Respuesta</summary>

```rust
let tree = ETree::node(1,
    ETree::leaf(2),
    ETree::node(3, ETree::leaf(4), ETree::leaf(5)),
);
```

Cada `leaf` crea 2 `Box::new(Empty)`.  Hay 3 hojas (2, 4, 5) → 6
`Box::new(Empty)`.  Cada `node` con `Empty` directo suma 0 extra aquí.
Total: **6 allocations para vacíos** que el estilo struct no necesita.
</details>

### Ejercicio 3 — contains genérico

Implementa `contains` para el estilo struct.  Busca 7 y 99 en el árbol de
ejemplo.

<details>
<summary>Implementación</summary>

```rust
fn s_contains<T: PartialEq>(tree: &STree<T>, value: &T) -> bool {
    match tree {
        None => false,
        Some(n) => {
            &n.data == value
            || s_contains(&n.left, value)
            || s_contains(&n.right, value)
        }
    }
}

assert!(s_contains(&st, &7));
assert!(!s_contains(&st, &99));
```
</details>

### Ejercicio 4 — mirror

Implementa `mirror` para ambos estilos: intercambiar left y right de cada
nodo recursivamente.  ¿Cuál estilo es más ergonómico para esta operación?

<details>
<summary>Ambas versiones</summary>

Struct (mutación in-place):
```rust
fn s_mirror<T>(tree: &mut STree<T>) {
    if let Some(node) = tree {
        std::mem::swap(&mut node.left, &mut node.right);
        s_mirror(&mut node.left);
        s_mirror(&mut node.right);
    }
}
```

Enum (crear nuevo árbol):
```rust
impl<T: Clone> ETree<T> {
    fn mirror(&self) -> ETree<T> {
        match self {
            Empty => Empty,
            Node { data, left, right } => {
                ETree::node(data.clone(), right.mirror(), left.mirror())
            }
        }
    }
}
```

El estilo struct muta in-place con `swap` — más eficiente.
El estilo enum crea un nuevo árbol — más funcional, pero clona datos.
</details>

### Ejercicio 5 — map con enum

Implementa `map` para el estilo enum que transforme cada dato.  Dado el
árbol `[10, 5, 15, 3, 7, 20]`, crea un nuevo árbol con cada valor
multiplicado por 2.

<details>
<summary>Resultado</summary>

```rust
impl<T> ETree<T> {
    fn map<U, F: Fn(&T) -> U>(&self, f: &F) -> ETree<U> {
        match self {
            Empty => Empty,
            Node { data, left, right } => {
                ETree::node(f(data), left.map(f), right.map(f))
            }
        }
    }
}

let doubled = et.map(&|x| x * 2);
// [20, 10, 30, 6, 14, 40]
```
</details>

### Ejercicio 6 — fold generalizado

Usa `fold` del estilo enum para implementar: sum, count, y height.
Verifica que los 3 dan el mismo resultado que las funciones dedicadas.

<details>
<summary>Los tres folds</summary>

```rust
let sum = tree.fold(0, &|l, val, r| l + val + r);         // 60
let count = tree.fold(0usize, &|l, _, r| l + 1 + r);      // 6
let height = tree.fold(-1i32, &|l, _, r| 1 + l.max(r));   // 2
```

`fold` captura el patrón recursivo universal: qué hacer con el caso vacío
(`init`) y cómo combinar el resultado de izquierda, el dato, y el resultado
de derecha.
</details>

### Ejercicio 7 — NPO: verificar tamaño

Verifica con `std::mem::size_of` los tamaños de:
1. `Option<Box<TreeNode<i32>>>` (estilo struct).
2. `ETree<i32>` (estilo enum).
3. `Box<ETree<i32>>`.

<details>
<summary>Resultados típicos (64-bit)</summary>

```rust
use std::mem::size_of;

// 1. Option<Box<TreeNode<i32>>>
size_of::<Option<Box<SNode<i32>>>>()   // 8 bytes (NPO: None = null pointer)

// 2. ETree<i32>
size_of::<ETree<i32>>()                // 24 bytes (discriminant + data + 2 Box)

// 3. Box<ETree<i32>>
size_of::<Box<ETree<i32>>>()           // 8 bytes (puntero al heap)
```

El estilo struct con NPO usa 8 bytes para el "puntero al nodo" (o 0 si
vacío).  El enum sin Box usa 24 bytes inline.  Dentro de un `Box<ETree>`,
ambos usan 8 bytes (puntero), pero el enum alloca incluso para `Empty`.
</details>

### Ejercicio 8 — Display

Implementa `Display` para el estilo enum que imprima el árbol como
expresión con paréntesis: `(left data right)` o `data` para hojas.

<details>
<summary>Implementación</summary>

```rust
impl<T: std::fmt::Display> std::fmt::Display for ETree<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Empty => Ok(()),
            Node { data, left, right } if left.is_empty() && right.is_empty() => {
                write!(f, "{data}")
            }
            Node { data, left, right } => {
                write!(f, "({left} {data} {right})")
            }
        }
    }
}

// Árbol [10, 5, 15, 3, 7, _, 20]:
// ((3 5 7) 10 ( 15 20))
```
</details>

### Ejercicio 9 — Comparar árboles

Implementa `PartialEq` para el estilo struct.  Dos árboles son iguales si
tienen la misma estructura y los mismos datos.

<details>
<summary>Implementación</summary>

```rust
fn s_equal<T: PartialEq>(a: &STree<T>, b: &STree<T>) -> bool {
    match (a, b) {
        (None, None) => true,
        (Some(na), Some(nb)) => {
            na.data == nb.data
            && s_equal(&na.left, &nb.left)
            && s_equal(&na.right, &nb.right)
        }
        _ => false,
    }
}
```

Tres líneas de lógica — idéntico al C de T02 pero con pattern matching
que elimina la posibilidad de null dereference.
</details>

### Ejercicio 10 — Elegir estilo

Para cada caso, elige struct o enum y justifica:
1. BST con inserciones y eliminaciones mutables.
2. Árbol de expresión evaluado recursivamente.
3. Estructura de datos en una biblioteca pública.
4. Ejercicio para aprender pattern matching.

<details>
<summary>Respuestas</summary>

1. **BST mutable → struct**.  `take()` y `replace` para mover ownership
   durante inserción/eliminación.  Más eficiente (NPO, sin alloc para
   vacíos).

2. **Árbol de expresión → enum**.  Las variantes del enum representan
   naturalmente los tipos de nodo (`Num(i32)`, `Add(Box, Box)`, etc.).
   Evaluación recursiva con `match` es elegante.

3. **Biblioteca pública → struct**.  Más eficiente, patrón estándar en el
   ecosistema Rust, compatible con `Option` idiomático.

4. **Aprendizaje de pattern matching → enum**.  `match` con destructuring
   de `Node { data, left, right }` enseña pattern matching avanzado.
   `map`/`fold` enseñan pensamiento funcional.
</details>
