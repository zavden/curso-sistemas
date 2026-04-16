# BST en Rust

## Estructura base

Un BST genérico en Rust usa `Option<Box<Node<T>>>` con `T: Ord` para
garantizar que los valores son comparables:

```rust
type Tree<T> = Option<Box<Node<T>>>;

struct Node<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
}

impl<T> Node<T> {
    fn new(data: T) -> Self {
        Node { data, left: None, right: None }
    }

    fn leaf(data: T) -> Tree<T> {
        Some(Box::new(Node::new(data)))
    }
}
```

`T: Ord` proporciona `cmp()` que retorna `Ordering::{Less, Equal, Greater}`.
Esto reemplaza las comparaciones `<`, `==`, `>` con un solo `match`:

```rust
use std::cmp::Ordering;

match key.cmp(&node.data) {
    Ordering::Less    => /* ir a la izquierda */,
    Ordering::Greater => /* ir a la derecha */,
    Ordering::Equal   => /* encontrado */,
}
```

---

## Inserción

### Versión recursiva

```rust
fn bst_insert<T: Ord>(tree: &mut Tree<T>, value: T) {
    match tree {
        None => *tree = Node::leaf(value),
        Some(node) => match value.cmp(&node.data) {
            Ordering::Less    => bst_insert(&mut node.left, value),
            Ordering::Greater => bst_insert(&mut node.right, value),
            Ordering::Equal   => {}  // duplicado: no insertar
        },
    }
}
```

El `&mut Tree<T>` permite modificar in-place — cuando `tree` es `None`,
se escribe directamente en el puntero del padre.  Es el equivalente
Rust del puntero a puntero (`TreeNode **`) de C.

### Versión iterativa

```rust
fn bst_insert_iter<T: Ord>(tree: &mut Tree<T>, value: T) {
    let mut current = tree;
    loop {
        match current {
            None => {
                *current = Node::leaf(value);
                return;
            }
            Some(node) => match value.cmp(&node.data) {
                Ordering::Less    => current = &mut node.left,
                Ordering::Greater => current = &mut node.right,
                Ordering::Equal   => return,
            },
        }
    }
}
```

La referencia mutable `current` se reasigna en cada paso.  El borrow
checker verifica que no hay alias — solo un camino mutable existe en
cada momento.

---

## Búsqueda

```rust
fn bst_search<T: Ord>(tree: &Tree<T>, key: &T) -> Option<&T> {
    let mut current = tree;
    while let Some(node) = current {
        match key.cmp(&node.data) {
            Ordering::Equal   => return Some(&node.data),
            Ordering::Less    => current = &node.left,
            Ordering::Greater => current = &node.right,
        }
    }
    None
}
```

Retorna `Option<&T>` — referencia al dato encontrado.  No `Option<&Node<T>>`
porque exponer la estructura interna rompe la encapsulación.

### contains

```rust
fn bst_contains<T: Ord>(tree: &Tree<T>, key: &T) -> bool {
    bst_search(tree, key).is_some()
}
```

---

## Mínimo y máximo

```rust
fn bst_min<T>(tree: &Tree<T>) -> Option<&T> {
    let mut current = tree;
    let mut result = None;
    while let Some(node) = current {
        result = Some(&node.data);
        current = &node.left;
    }
    result
}

fn bst_max<T>(tree: &Tree<T>) -> Option<&T> {
    let mut current = tree;
    let mut result = None;
    while let Some(node) = current {
        result = Some(&node.data);
        current = &node.right;
    }
    result
}
```

---

## Eliminación

La eliminación es donde Rust se distingue más de C.  Los tres casos
requieren manipulación cuidadosa del ownership.

```rust
fn bst_delete<T: Ord>(tree: &mut Tree<T>, key: &T) {
    let node = match tree {
        None => return,
        Some(node) => node,
    };

    match key.cmp(&node.data) {
        Ordering::Less    => bst_delete(&mut node.left, key),
        Ordering::Greater => bst_delete(&mut node.right, key),
        Ordering::Equal   => {
            // Encontrado: eliminar este nodo
            match (&node.left, &node.right) {
                // Caso 1: hoja
                (None, None) => *tree = None,

                // Caso 2: solo hijo derecho
                (None, _) => *tree = tree.take().unwrap().right,

                // Caso 2: solo hijo izquierdo
                (_, None) => *tree = tree.take().unwrap().left,

                // Caso 3: dos hijos
                _ => {
                    let succ_val = take_min(&mut node.right);
                    node.data = succ_val;
                }
            }
        }
    }
}
```

### `take_min`: extraer el mínimo

La pieza clave del caso 3 — encontrar el mínimo del subárbol derecho,
**extraerlo** y retornar su valor:

```rust
fn take_min<T>(tree: &mut Tree<T>) -> T {
    if tree.as_ref().unwrap().left.is_none() {
        // Este nodo es el mínimo: extraerlo
        let node = tree.take().unwrap();
        *tree = node.right;   // enlazar su hijo derecho (o None)
        node.data
    } else {
        take_min(&mut tree.as_mut().unwrap().left)
    }
}
```

`take_min` desciende por la izquierda hasta encontrar el mínimo.
Lo extrae con `take()` (reemplaza `Some(...)` por `None`), enlaza su
posible hijo derecho en su lugar, y retorna el dato.

### El método `take()`

`Option::take()` es fundamental para la eliminación en Rust:

```rust
let mut x: Option<Box<Node<i32>>> = Some(Box::new(Node::new(5)));

let extracted = x.take();
// x ahora es None
// extracted es Some(Box<Node<5>>)
```

`take()` extrae el valor de un `Option` dejando `None` en su lugar.
Permite "desenlazar" un nodo sin violar las reglas de ownership — el
nodo extraído se puede manipular libremente mientras la posición original
queda vacía.

### Traza: eliminar 10 del BST de referencia

```rust
bst_delete(&mut tree, &10)
  node.data = 10, key = 10 → Equal
  left = Some, right = Some → caso 3

  take_min(&mut node.right)
    node.right = Some(15)
    15 tiene left = Some(12) → recurrir
    take_min(&mut 15.left)
      15.left = Some(12)
      12 no tiene left → es el mínimo
      node = tree.take() → extraer nodo(12), 15.left = None
      *tree = node.right → 15.left = None (12 no tenía right)
      retornar 12

  node.data = 12  (10 se reemplaza por 12)
```

Resultado: la raíz ahora contiene 12.  El nodo original 12 fue extraído
y destruido (Drop automático del `Box`).

---

## Diferencias clave C vs Rust en delete

| Aspecto | C | Rust |
|---------|---|------|
| Extraer nodo | `free(node)` manual | `take()` + Drop automático |
| Enlazar hijo | `parent->left = child` | `*tree = node.right` |
| Copiar dato del sucesor | `root->data = succ->data` | `node.data = take_min(...)` |
| Sucesor se elimina con | Recursión o búsqueda explícita | `take_min` extrae en una pasada |
| Memory leak posible | Sí (olvidar `free`) | No (Drop garantizado) |
| Use-after-free posible | Sí | No (borrow checker) |

La función `take_min` en Rust hace en una operación lo que en C requiere
dos (encontrar sucesor + eliminar sucesor).  Extraer el mínimo y
retornar su dato es más directo que copiar el dato y luego eliminar por
valor.

---

## Sucesor y predecesor

```rust
fn bst_successor<T: Ord>(tree: &Tree<T>, key: &T) -> Option<&T> {
    let mut succ: Option<&T> = None;
    let mut current = tree;

    while let Some(node) = current {
        match key.cmp(&node.data) {
            Ordering::Less => {
                succ = Some(&node.data);
                current = &node.left;
            }
            Ordering::Greater => {
                current = &node.right;
            }
            Ordering::Equal => {
                if let Some(ref right) = node.right {
                    return bst_min(&Some(right.clone()));
                    // Alternativa sin clonar:
                }
                break;
            }
        }
    }

    succ
}
```

Versión más limpia sin clonar, inlining la búsqueda del mínimo:

```rust
fn bst_successor<T: Ord>(tree: &Tree<T>, key: &T) -> Option<&T> {
    let mut succ: Option<&T> = None;
    let mut current = tree;

    while let Some(node) = current {
        match key.cmp(&node.data) {
            Ordering::Less => {
                succ = Some(&node.data);
                current = &node.left;
            }
            Ordering::Greater => current = &node.right,
            Ordering::Equal => {
                // Si tiene right, min del right
                let mut r = &node.right;
                while let Some(rn) = r {
                    succ = Some(&rn.data);
                    r = &rn.left;
                }
                break;
            }
        }
    }

    succ
}
```

---

## Recolectar inorden

```rust
fn inorder_vec<T: Clone>(tree: &Tree<T>) -> Vec<T> {
    let mut result = Vec::new();
    fn collect<T: Clone>(tree: &Tree<T>, out: &mut Vec<T>) {
        if let Some(node) = tree {
            collect(&node.left, out);
            out.push(node.data.clone());
            collect(&node.right, out);
        }
    }
    collect(tree, &mut result);
    result
}
```

---

## BST como struct con métodos

Envolver el árbol en una struct con API limpia:

```rust
use std::cmp::Ordering;

type Link<T> = Option<Box<Node<T>>>;

struct Node<T> {
    data: T,
    left: Link<T>,
    right: Link<T>,
}

pub struct BST<T: Ord> {
    root: Link<T>,
    len: usize,
}

impl<T: Ord> BST<T> {
    pub fn new() -> Self {
        BST { root: None, len: 0 }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn insert(&mut self, value: T) -> bool {
        if Self::insert_into(&mut self.root, value) {
            self.len += 1;
            true
        } else {
            false   // duplicado
        }
    }

    pub fn contains(&self, key: &T) -> bool {
        let mut current = &self.root;
        while let Some(node) = current {
            match key.cmp(&node.data) {
                Ordering::Equal   => return true,
                Ordering::Less    => current = &node.left,
                Ordering::Greater => current = &node.right,
            }
        }
        false
    }

    pub fn remove(&mut self, key: &T) -> bool {
        if Self::remove_from(&mut self.root, key) {
            self.len -= 1;
            true
        } else {
            false
        }
    }

    pub fn min(&self) -> Option<&T> {
        let mut current = &self.root;
        let mut result = None;
        while let Some(node) = current {
            result = Some(&node.data);
            current = &node.left;
        }
        result
    }

    pub fn max(&self) -> Option<&T> {
        let mut current = &self.root;
        let mut result = None;
        while let Some(node) = current {
            result = Some(&node.data);
            current = &node.right;
        }
        result
    }

    pub fn inorder(&self) -> Vec<&T> {
        let mut result = Vec::new();
        Self::collect_inorder(&self.root, &mut result);
        result
    }

    // --- Funciones internas ---

    fn insert_into(tree: &mut Link<T>, value: T) -> bool {
        match tree {
            None => {
                *tree = Some(Box::new(Node {
                    data: value,
                    left: None,
                    right: None,
                }));
                true
            }
            Some(node) => match value.cmp(&node.data) {
                Ordering::Less    => Self::insert_into(&mut node.left, value),
                Ordering::Greater => Self::insert_into(&mut node.right, value),
                Ordering::Equal   => false,
            },
        }
    }

    fn remove_from(tree: &mut Link<T>, key: &T) -> bool {
        let node = match tree {
            None => return false,
            Some(node) => node,
        };

        match key.cmp(&node.data) {
            Ordering::Less    => Self::remove_from(&mut node.left, key),
            Ordering::Greater => Self::remove_from(&mut node.right, key),
            Ordering::Equal   => {
                match (&node.left, &node.right) {
                    (None, None) => *tree = None,
                    (None, _)    => *tree = tree.take().unwrap().right,
                    (_, None)    => *tree = tree.take().unwrap().left,
                    _            => {
                        node.data = Self::take_min(&mut node.right);
                    }
                }
                true
            }
        }
    }

    fn take_min(tree: &mut Link<T>) -> T {
        if tree.as_ref().unwrap().left.is_none() {
            let node = tree.take().unwrap();
            *tree = node.right;
            node.data
        } else {
            Self::take_min(&mut tree.as_mut().unwrap().left)
        }
    }

    fn collect_inorder<'a>(tree: &'a Link<T>, out: &mut Vec<&'a T>) {
        if let Some(node) = tree {
            Self::collect_inorder(&node.left, out);
            out.push(&node.data);
            Self::collect_inorder(&node.right, out);
        }
    }
}
```

### Uso

```rust
fn main() {
    let mut bst = BST::new();

    for v in [10, 5, 15, 3, 7, 12, 20] {
        bst.insert(v);
    }

    println!("len:      {}", bst.len());           // 7
    println!("inorder:  {:?}", bst.inorder());     // [3, 5, 7, 10, 12, 15, 20]
    println!("min:      {:?}", bst.min());         // Some(3)
    println!("max:      {:?}", bst.max());         // Some(20)
    println!("has 7:    {}", bst.contains(&7));    // true
    println!("has 9:    {}", bst.contains(&9));    // false

    bst.remove(&10);
    println!("\nAfter removing 10:");
    println!("inorder:  {:?}", bst.inorder());     // [3, 5, 7, 12, 15, 20]
    println!("len:      {}", bst.len());           // 6

    bst.insert(10);
    println!("\nAfter re-inserting 10:");
    println!("inorder:  {:?}", bst.inorder());     // [3, 5, 7, 10, 12, 15, 20]

    // Duplicado
    let inserted = bst.insert(10);
    println!("\nInsert 10 again: {}", inserted);   // false
    println!("len:      {}", bst.len());           // 7
}
```

Salida:

```
len:      7
inorder:  [3, 5, 7, 10, 12, 15, 20]
min:      Some(3)
max:      Some(20)
has 7:    true
has 9:    false

After removing 10:
inorder:  [3, 5, 7, 12, 15, 20]
len:      6

After re-inserting 10:
inorder:  [3, 5, 7, 10, 12, 15, 20]

Insert 10 again: false
len:      7
```

---

## Iterator inorden

Implementar `IntoIterator` para recorrer el BST en orden:

```rust
pub struct InorderIter<'a, T> {
    stack: Vec<&'a Node<T>>,
}

impl<'a, T> InorderIter<'a, T> {
    fn new(root: &'a Link<T>) -> Self {
        let mut iter = InorderIter { stack: Vec::new() };
        iter.push_left(root);
        iter
    }

    fn push_left(&mut self, mut tree: &'a Link<T>) {
        while let Some(node) = tree {
            self.stack.push(node);
            tree = &node.left;
        }
    }
}

impl<'a, T> Iterator for InorderIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        let node = self.stack.pop()?;
        self.push_left(&node.right);
        Some(&node.data)
    }
}

impl<T: Ord> BST<T> {
    pub fn iter(&self) -> InorderIter<'_, T> {
        InorderIter::new(&self.root)
    }
}
```

Es el inorden iterativo de T03 (S02), encapsulado como `Iterator`.
Cada llamada a `next()` produce el siguiente valor en orden.

```rust
for val in bst.iter() {
    print!("{val} ");
}
// 3 5 7 10 12 15 20
```

Lazy: no recorre todo el árbol de antemano.  Útil para `take()`,
`skip()`, `find()` — se detiene temprano.

```rust
let first_3: Vec<_> = bst.iter().take(3).collect();
// [3, 5, 7]

let has_12 = bst.iter().any(|&v| v == 12);
// true (se detiene al encontrar 12, no recorre todo)
```

---

## BST genérico: cualquier tipo Ord

```rust
let mut names = BST::new();
names.insert("Carlos");
names.insert("Ana");
names.insert("Elena");
names.insert("Diana");
names.insert("Bruno");

println!("{:?}", names.inorder());
// ["Ana", "Bruno", "Carlos", "Diana", "Elena"]

println!("min: {:?}", names.min());   // Some("Ana")
println!("max: {:?}", names.max());   // Some("Elena")
```

Funciona con cualquier tipo que implemente `Ord`: `i32`, `String`,
`&str`, `char`, tipos propios con `#[derive(Ord, PartialOrd, Eq, PartialEq)]`.

---

## Comparación: BST propio vs BTreeSet

La biblioteca estándar ofrece `BTreeSet<T>` y `BTreeMap<K, V>` — no son
BSTs sino **B-trees**, pero ofrecen la misma interfaz conceptual:

```rust
use std::collections::BTreeSet;

let mut set = BTreeSet::new();
for v in [10, 5, 15, 3, 7, 12, 20] {
    set.insert(v);
}

println!("{:?}", set);                          // {3, 5, 7, 10, 12, 15, 20}
println!("contains 7: {}", set.contains(&7));  // true
set.remove(&10);
println!("min: {:?}", set.iter().next());      // Some(3)
println!("range 5..15: {:?}", set.range(5..15).collect::<Vec<_>>());
// [5, 7, 12]
```

| Aspecto | BST propio | `BTreeSet<T>` |
|---------|-----------|---------------|
| Complejidad búsqueda | $O(h)$, peor caso $O(n)$ | $O(\log n)$ siempre |
| Balanceo | Ninguno | Automático (B-tree) |
| Cache locality | Mala (cada nodo en heap separado) | Buena (nodos con muchas claves) |
| Iteración ordenada | Inorden $O(n)$ | Inorden $O(n)$ |
| `range()` | Implementar manualmente | Incluido |
| Uso recomendado | **Aprendizaje** | **Producción** |

Un BST simple sin balanceo **no se usa en producción** — puede
degenerar a $O(n)$.  `BTreeSet`/`BTreeMap` o árboles AVL/rojinegros
garantizan $O(\log n)$.  El BST simple es fundamental como concepto
base.

---

## Ejercicios

### Ejercicio 1 — Construir con la struct BST

Usa la struct `BST<T>` para insertar `[20, 10, 30, 5, 15, 25, 35]`.
¿Cuál es el inorden?  ¿Cuál es el min y max?

<details>
<summary>Predicción</summary>

```rust
let mut bst = BST::new();
for v in [20, 10, 30, 5, 15, 25, 35] {
    bst.insert(v);
}
```

Inorden: `[5, 10, 15, 20, 25, 30, 35]` — los valores ordenados.

Min: `Some(5)`.  Max: `Some(35)`.

El árbol:
```
         20
        /  \
      10    30
      / \   / \
     5  15 25  35
```
</details>

### Ejercicio 2 — take_min: traza

Traza `take_min` sobre el subárbol derecho cuando se elimina 10:

```
Subárbol derecho de 10:
    15
   / \
  12  20
```

<details>
<summary>Traza</summary>

```
take_min(tree = &mut Some(15))
  15 tiene left → recurrir
  take_min(tree = &mut 15.left = &mut Some(12))
    12 no tiene left → es el mínimo
    node = tree.take() → extraer nodo(12)
    *tree = node.right → 15.left = None (12 no tenía right)
    retornar 12
```

Después: el subárbol es `15 → right(20)`, sin hijo izquierdo.
El valor 12 se usa para reemplazar el 10 del nodo eliminado.
</details>

### Ejercicio 3 — Eliminar secuencia

Usando la struct `BST`, ejecuta:

```rust
let mut bst = BST::new();
for v in [10, 5, 15, 3, 7, 12, 20] { bst.insert(v); }

bst.remove(&3);    // caso 1
bst.remove(&15);   // caso 3
bst.remove(&10);   // caso 3 (raíz)
```

¿Cuál es el inorden y len después de cada remove?

<details>
<summary>Predicción</summary>

Después de `remove(&3)`:
- Inorden: `[5, 7, 10, 12, 15, 20]`, len=6.

Después de `remove(&15)` (succ=20):
- Inorden: `[5, 7, 10, 12, 20]`, len=5.

Después de `remove(&10)` (succ=12):
- Inorden: `[5, 7, 12, 20]`, len=4.
</details>

### Ejercicio 4 — Iterator con take

Usando el iterator, obtén los 3 valores más pequeños del BST de
referencia.

<details>
<summary>Código</summary>

```rust
let smallest_3: Vec<&i32> = bst.iter().take(3).collect();
assert_eq!(smallest_3, vec![&3, &5, &7]);
```

El iterator produce valores en orden ascendente (inorden).  `take(3)`
se detiene después de 3 elementos sin recorrer el resto del árbol.

Para los 3 más grandes: no hay `DoubleEndedIterator` implementado
aquí, pero se podría implementar con un stack que baje por la derecha.
Alternativa: `bst.inorder().into_iter().rev().take(3)`.
</details>

### Ejercicio 5 — BST de strings

Crea un BST con los nombres `["Elena", "Ana", "Carlos", "Diana", "Bruno"]`.
¿Cuál es el inorden?

<details>
<summary>Predicción</summary>

```rust
let mut bst = BST::new();
for name in ["Elena", "Ana", "Carlos", "Diana", "Bruno"] {
    bst.insert(name);
}
```

Los strings se comparan lexicográficamente.  Inorden:
`["Ana", "Bruno", "Carlos", "Diana", "Elena"]`.

Árbol (según orden de inserción):
```
       "Elena"
       /
    "Ana"
       \
     "Carlos"
      /     \
   "Bruno" "Diana"
```
</details>

### Ejercicio 6 — Implementar height

Añade un método `height(&self) -> i32` a la struct BST.

<details>
<summary>Implementación</summary>

```rust
impl<T: Ord> BST<T> {
    pub fn height(&self) -> i32 {
        Self::node_height(&self.root)
    }

    fn node_height(tree: &Link<T>) -> i32 {
        match tree {
            None => -1,
            Some(node) => {
                let lh = Self::node_height(&node.left);
                let rh = Self::node_height(&node.right);
                1 + lh.max(rh)
            }
        }
    }
}
```

Para el BST de referencia (7 nodos, perfecto): `height() = 2`.
Para un degenerado de 7 nodos: `height() = 6`.
</details>

### Ejercicio 7 — insert retorna bool

El método `insert` retorna `true` si se insertó, `false` si era
duplicado.  ¿Por qué es útil?

<details>
<summary>Explicación</summary>

Permite al caller saber si el BST se modificó:

```rust
if bst.insert(42) {
    println!("42 added (new)");
} else {
    println!("42 already exists");
}
```

Esto replica la API de `BTreeSet::insert()` y `HashSet::insert()` de
la biblioteca estándar.  Es útil para:
- Contar valores únicos.
- Evitar procesamiento redundante.
- Mantener un contador `len` preciso (solo incrementar si se insertó).

Sin el retorno, habría que llamar `contains` antes de `insert` — dos
recorridos en vez de uno.
</details>

### Ejercicio 8 — Comparar con BTreeSet

Realiza las mismas operaciones con `BST<i32>` propio y
`BTreeSet<i32>`.  ¿En qué difiere la API?

<details>
<summary>Comparación</summary>

```rust
// BST propio                          // BTreeSet
let mut bst = BST::new();             let mut set = BTreeSet::new();
bst.insert(10);                       set.insert(10);
bst.contains(&10);                    set.contains(&10);
bst.remove(&10);                      set.remove(&10);
bst.min();                            set.iter().next();
bst.max();                            set.iter().next_back();
bst.inorder();                        set.iter().collect::<Vec<_>>();
bst.len();                            set.len();
```

La API es casi idéntica.  `BTreeSet` ofrece adicionalmente:
- `range(lo..hi)` — iteración sobre un rango.
- `difference()`, `intersection()`, `union()` — operaciones de
  conjuntos.
- `DoubleEndedIterator` — iterar desde el final.
- Garantía de $O(\log n)$ siempre (no puede degenerar).
</details>

### Ejercicio 9 — Implementar floor

Añade `floor(&self, key: &T) -> Option<&T>` a la struct BST.

<details>
<summary>Implementación</summary>

```rust
impl<T: Ord> BST<T> {
    pub fn floor(&self, key: &T) -> Option<&T> {
        let mut result: Option<&T> = None;
        let mut current = &self.root;

        while let Some(node) = current {
            match key.cmp(&node.data) {
                Ordering::Equal => return Some(&node.data),
                Ordering::Greater => {
                    result = Some(&node.data);
                    current = &node.right;
                }
                Ordering::Less => current = &node.left,
            }
        }

        result
    }
}

// bst.floor(&9)  → Some(&7)
// bst.floor(&10) → Some(&10)
// bst.floor(&1)  → None
```
</details>

### Ejercicio 10 — Drop y árboles profundos

La implementación por defecto de `Drop` para `Option<Box<Node<T>>>` es
recursiva.  ¿Para qué tamaño de BST degenerado podría causar stack
overflow?  ¿Cómo se soluciona?

<details>
<summary>Análisis</summary>

Cada frame de `Drop` consume ~64-128 bytes de stack.  Con un stack
típico de 8 MB: $8 \times 10^6 / 100 \approx 80000$ niveles de
recursión.  Un BST degenerado con $n > 80000$ nodos puede desbordar
el stack al hacer `drop`.

Solución: implementar `Drop` iterativo para `BST`:

```rust
impl<T: Ord> Drop for BST<T> {
    fn drop(&mut self) {
        let mut stack = Vec::new();
        if let Some(root) = self.root.take() {
            stack.push(root);
        }
        while let Some(mut node) = stack.pop() {
            if let Some(left) = node.left.take() {
                stack.push(left);
            }
            if let Some(right) = node.right.take() {
                stack.push(right);
            }
            // node se libera aquí (Drop de Box<Node>)
        }
    }
}
```

`take()` extrae los hijos antes de que el nodo se destruya.  Los hijos
se apilan para destruirse después.  Usa stack en el heap (ilimitado)
en vez del call stack del sistema.

Para BSTs balanceados esto no es un problema (profundidad máxima
$\approx 20$ para un millón de nodos).  Solo importa para degenerados.
</details>
