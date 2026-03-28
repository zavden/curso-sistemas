# Implementación en Rust

## AVL genérico en Rust

Implementaremos un AVL completo con `T: Ord` — buscar, insertar,
eliminar, iterar — usando el patrón `Option<Box<Node<T>>>`.

### Estructura base

```rust
use std::cmp::Ordering;

type Tree<T> = Option<Box<Node<T>>>;

struct Node<T> {
    data: T,
    left: Tree<T>,
    right: Tree<T>,
    height: i32,
}

impl<T> Node<T> {
    fn new(data: T) -> Self {
        Node { data, left: None, right: None, height: 0 }
    }

    fn leaf(data: T) -> Tree<T> {
        Some(Box::new(Node::new(data)))
    }
}

fn height<T>(tree: &Tree<T>) -> i32 {
    match tree {
        None => -1,
        Some(node) => node.height,
    }
}

fn update_height<T>(node: &mut Node<T>) {
    node.height = 1 + height(&node.left).max(height(&node.right));
}

fn balance_factor<T>(node: &Node<T>) -> i32 {
    height(&node.right) - height(&node.left)
}
```

---

## Rotaciones

Las rotaciones consumen el nodo (`Box<Node<T>>`) y retornan el nuevo
nodo raíz.  `take()` es esencial para mover el ownership de los hijos:

```rust
fn rotate_right<T>(mut y: Box<Node<T>>) -> Box<Node<T>> {
    let mut x = y.left.take().expect("rotate_right: no left child");
    y.left = x.right.take();
    update_height(&mut *y);
    x.right = Some(y);
    update_height(&mut *x);
    x
}

fn rotate_left<T>(mut x: Box<Node<T>>) -> Box<Node<T>> {
    let mut y = x.right.take().expect("rotate_left: no right child");
    x.right = y.left.take();
    update_height(&mut *x);
    y.left = Some(x);
    update_height(&mut *y);
    y
}
```

`&mut *y` desreferencia el `Box` para obtener `&mut Node<T>` — necesario
para `update_height`.

---

## Rebalanceo

```rust
fn rebalance<T>(mut node: Box<Node<T>>) -> Box<Node<T>> {
    update_height(&mut *node);
    let bf = balance_factor(&node);

    if bf < -1 {
        // Exceso izquierdo
        if balance_factor(node.left.as_ref().unwrap()) > 0 {
            // Caso LR
            let left = node.left.take().unwrap();
            node.left = Some(rotate_left(left));
        }
        return rotate_right(node);
    }

    if bf > 1 {
        // Exceso derecho
        if balance_factor(node.right.as_ref().unwrap()) < 0 {
            // Caso RL
            let right = node.right.take().unwrap();
            node.right = Some(rotate_right(right));
        }
        return rotate_left(node);
    }

    node
}
```

`as_ref().unwrap()` obtiene `&Node<T>` del hijo sin moverlo — solo
necesitamos leer su balance para decidir el tipo de rotación.

---

## Inserción

```rust
fn avl_insert<T: Ord>(tree: Tree<T>, value: T) -> Tree<T> {
    let mut node = match tree {
        None => return Node::leaf(value),
        Some(node) => node,
    };

    match value.cmp(&node.data) {
        Ordering::Less    => node.left = avl_insert(node.left.take(), value),
        Ordering::Greater => node.right = avl_insert(node.right.take(), value),
        Ordering::Equal   => return Some(node),  // duplicado
    }

    Some(rebalance(node))
}
```

El patrón es: extraer el subárbol con `take()`, recurrir, reasignar el
resultado, rebalancear.  `take()` mueve el ownership al hijo para la
recursión; el resultado se reasigna al campo del padre.

---

## Eliminación

```rust
fn take_min<T>(mut node: Box<Node<T>>) -> (T, Tree<T>) {
    match node.left.take() {
        None => {
            // Este nodo es el mínimo
            (node.data, node.right)
        }
        Some(left) => {
            let (min_val, new_left) = take_min(left);
            node.left = new_left;
            (min_val, Some(rebalance(node)))
        }
    }
}

fn avl_delete<T: Ord>(tree: Tree<T>, key: &T) -> Tree<T> {
    let mut node = match tree {
        None => return None,
        Some(node) => node,
    };

    match key.cmp(&node.data) {
        Ordering::Less => {
            node.left = avl_delete(node.left.take(), key);
            Some(rebalance(node))
        }
        Ordering::Greater => {
            node.right = avl_delete(node.right.take(), key);
            Some(rebalance(node))
        }
        Ordering::Equal => {
            // Encontrado
            match (node.left.take(), node.right.take()) {
                (None, None)    => None,
                (Some(left), None) => Some(left),
                (None, Some(right)) => Some(right),
                (Some(left), Some(right)) => {
                    // Caso 3: dos hijos
                    node.left = Some(left);
                    let (succ_val, new_right) = take_min(right);
                    node.data = succ_val;
                    node.right = new_right;
                    Some(rebalance(node))
                }
            }
        }
    }
}
```

`take_min` retorna una tupla `(T, Tree<T>)` — el valor mínimo extraído
y el subárbol restante (ya rebalanceado).  Es más idiomático en Rust que
el patrón C de "copiar dato + eliminar por valor".

### Flujo del caso 3

```
node tiene left y right.
1. Extraer ambos hijos con take().
2. Volver a colocar left.
3. Llamar take_min(right) → obtener (succ_val, new_right).
4. Reemplazar node.data con succ_val.
5. Asignar new_right como el nuevo subárbol derecho.
6. Rebalancear.
```

---

## Struct AVL con API pública

```rust
use std::cmp::Ordering;

type Link<T> = Option<Box<Node<T>>>;

struct Node<T> {
    data: T,
    left: Link<T>,
    right: Link<T>,
    height: i32,
}

pub struct AVL<T: Ord> {
    root: Link<T>,
    len: usize,
}

impl<T: Ord> AVL<T> {
    pub fn new() -> Self {
        AVL { root: None, len: 0 }
    }

    pub fn len(&self) -> usize { self.len }
    pub fn is_empty(&self) -> bool { self.len == 0 }

    pub fn insert(&mut self, value: T) -> bool {
        let (new_root, inserted) = Self::do_insert(self.root.take(), value);
        self.root = new_root;
        if inserted { self.len += 1; }
        inserted
    }

    pub fn remove(&mut self, key: &T) -> bool {
        let (new_root, removed) = Self::do_remove(self.root.take(), key);
        self.root = new_root;
        if removed { self.len -= 1; }
        removed
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

    pub fn height(&self) -> i32 {
        height(&self.root)
    }

    pub fn iter(&self) -> InorderIter<'_, T> {
        InorderIter::new(&self.root)
    }

    // --- Internals ---

    fn do_insert(tree: Link<T>, value: T) -> (Link<T>, bool) {
        let mut node = match tree {
            None => return (Node::leaf(value), true),
            Some(node) => node,
        };

        let inserted = match value.cmp(&node.data) {
            Ordering::Less => {
                let (new_left, ins) = Self::do_insert(node.left.take(), value);
                node.left = new_left;
                ins
            }
            Ordering::Greater => {
                let (new_right, ins) = Self::do_insert(node.right.take(), value);
                node.right = new_right;
                ins
            }
            Ordering::Equal => false,
        };

        (Some(rebalance(node)), inserted)
    }

    fn do_remove(tree: Link<T>, key: &T) -> (Link<T>, bool) {
        let mut node = match tree {
            None => return (None, false),
            Some(node) => node,
        };

        match key.cmp(&node.data) {
            Ordering::Less => {
                let (new_left, removed) = Self::do_remove(node.left.take(), key);
                node.left = new_left;
                (Some(rebalance(node)), removed)
            }
            Ordering::Greater => {
                let (new_right, removed) = Self::do_remove(node.right.take(), key);
                node.right = new_right;
                (Some(rebalance(node)), removed)
            }
            Ordering::Equal => {
                let result = match (node.left.take(), node.right.take()) {
                    (None, None)          => None,
                    (Some(left), None)    => Some(left),
                    (None, Some(right))   => Some(right),
                    (Some(left), Some(right)) => {
                        node.left = Some(left);
                        let (succ_val, new_right) = Self::extract_min(right);
                        node.data = succ_val;
                        node.right = new_right;
                        Some(rebalance(node))
                    }
                };
                (result, true)
            }
        }
    }

    fn extract_min(mut node: Box<Node<T>>) -> (T, Link<T>) {
        match node.left.take() {
            None => (node.data, node.right),
            Some(left) => {
                let (min_val, new_left) = Self::extract_min(left);
                node.left = new_left;
                (min_val, Some(rebalance(node)))
            }
        }
    }
}

// --- Funciones libres (height, update_height, balance_factor, rotaciones, rebalance) ---

fn height<T>(tree: &Link<T>) -> i32 {
    match tree {
        None => -1,
        Some(node) => node.height,
    }
}

fn update_height<T>(node: &mut Node<T>) {
    node.height = 1 + height(&node.left).max(height(&node.right));
}

fn balance_factor<T>(node: &Node<T>) -> i32 {
    height(&node.right) - height(&node.left)
}

fn rotate_right<T>(mut y: Box<Node<T>>) -> Box<Node<T>> {
    let mut x = y.left.take().unwrap();
    y.left = x.right.take();
    update_height(&mut *y);
    x.right = Some(y);
    update_height(&mut *x);
    x
}

fn rotate_left<T>(mut x: Box<Node<T>>) -> Box<Node<T>> {
    let mut y = x.right.take().unwrap();
    x.right = y.left.take();
    update_height(&mut *x);
    y.left = Some(x);
    update_height(&mut *y);
    y
}

fn rebalance<T>(mut node: Box<Node<T>>) -> Box<Node<T>> {
    update_height(&mut *node);
    let bf = balance_factor(&node);

    if bf < -1 {
        if balance_factor(node.left.as_ref().unwrap()) > 0 {
            let left = node.left.take().unwrap();
            node.left = Some(rotate_left(left));
        }
        return rotate_right(node);
    }

    if bf > 1 {
        if balance_factor(node.right.as_ref().unwrap()) < 0 {
            let right = node.right.take().unwrap();
            node.right = Some(rotate_right(right));
        }
        return rotate_left(node);
    }

    node
}

impl<T> Node<T> {
    fn new(data: T) -> Self {
        Node { data, left: None, right: None, height: 0 }
    }
    fn leaf(data: T) -> Link<T> {
        Some(Box::new(Node::new(data)))
    }
}
```

---

## Iterator inorden

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
```

Lazy iteration — cada `next()` produce un valor sin recorrer todo el
árbol.  La pila almacena el camino actual, simulando el inorden
iterativo de S02/T03.

---

## Uso completo

```rust
fn main() {
    let mut avl = AVL::new();

    // Insertar en orden ascendente (peor caso BST)
    for i in 1..=15 {
        avl.insert(i);
    }

    println!("n={}, height={}", avl.len(), avl.height());
    println!("min={:?}, max={:?}", avl.min(), avl.max());
    print!("inorder: ");
    for val in avl.iter() {
        print!("{val} ");
    }
    println!();

    // Eliminar
    avl.remove(&8);
    avl.remove(&4);
    avl.remove(&12);
    println!("\nAfter removing 8, 4, 12:");
    println!("n={}, height={}", avl.len(), avl.height());
    print!("inorder: ");
    for val in avl.iter() {
        print!("{val} ");
    }
    println!();

    // Búsqueda
    println!("\ncontains(7)={}, contains(8)={}", avl.contains(&7), avl.contains(&8));

    // Strings
    let mut names = AVL::new();
    for name in ["Elena", "Ana", "Carlos", "Diana", "Bruno"] {
        names.insert(name);
    }
    print!("\nnames: ");
    for name in names.iter() {
        print!("{name} ");
    }
    println!();
}
```

Salida:

```
n=15, height=3
min=Some(1), max=Some(15)
inorder: 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15

After removing 8, 4, 12:
n=12, height=3
inorder: 1 2 3 5 6 7 9 10 11 13 14 15

contains(7)=true, contains(8)=false

names: Ana Bruno Carlos Diana Elena
```

---

## BTreeMap/BTreeSet: la alternativa práctica

En código Rust de producción, se usa `BTreeMap<K, V>` o `BTreeSet<T>`
en vez de implementar un AVL o RB propio:

```rust
use std::collections::BTreeMap;
use std::collections::BTreeSet;

fn main() {
    // BTreeSet: conjunto ordenado
    let mut set = BTreeSet::new();
    for i in [10, 5, 15, 3, 7, 12, 20] {
        set.insert(i);
    }

    println!("set: {:?}", set);
    println!("contains 7: {}", set.contains(&7));
    println!("min: {:?}", set.iter().next());
    println!("max: {:?}", set.iter().next_back());

    // Range queries
    let range: Vec<_> = set.range(5..15).collect();
    println!("range [5,15): {:?}", range);

    // BTreeMap: diccionario ordenado
    let mut scores: BTreeMap<&str, i32> = BTreeMap::new();
    scores.insert("Alice", 95);
    scores.insert("Bob", 87);
    scores.insert("Carol", 92);
    scores.insert("David", 78);

    println!("\nscores: {:?}", scores);

    // Iterar en orden de clave
    for (name, score) in &scores {
        println!("  {name}: {score}");
    }

    // Range
    let b_names: Vec<_> = scores.range("B".."D").collect();
    println!("names B..D: {:?}", b_names);

    // Entry API
    scores.entry("Eve").or_insert(0);
    *scores.entry("Alice").or_insert(0) += 5;
    println!("Alice: {}", scores["Alice"]);   // 100
}
```

Salida:

```
set: {3, 5, 7, 10, 12, 15, 20}
contains 7: true
min: Some(3)
max: Some(20)
range [5,15): [5, 7, 10, 12]

scores: {"Alice": 95, "Bob": 87, "Carol": 92, "David": 78}
  Alice: 95
  Bob: 87
  Carol: 92
  David: 78
names B..D: [("Bob", 87), ("Carol", 92)]
Alice: 100
```

### API destacada de BTreeMap/BTreeSet

| Método | Descripción | Complejidad |
|--------|-------------|------------|
| `insert(value)` | Insertar, retorna si es nuevo | $O(\log n)$ |
| `remove(key)` | Eliminar | $O(\log n)$ |
| `contains(key)` | Buscar | $O(\log n)$ |
| `get(key)` | Obtener referencia al valor | $O(\log n)$ |
| `range(lo..hi)` | Iterador sobre un rango | $O(\log n + k)$ |
| `iter()` | Iterador inorden | $O(n)$ total |
| `len()` | Número de elementos | $O(1)$ |
| `entry(key)` | API de inserción condicional | $O(\log n)$ |
| `split_off(key)` | Partir en dos árboles | $O(\log n)$ |
| `first_key_value()` | Mínimo | $O(\log n)$ |
| `last_key_value()` | Máximo | $O(\log n)$ |

---

## AVL propio vs BTreeMap

| Criterio | AVL propio | BTreeMap |
|----------|-----------|----------|
| Garantía $O(\log n)$ | Sí | Sí |
| Rendimiento real | Bueno | Mejor (cache locality) |
| `range()` | Implementar manualmente | Incluido |
| `entry()` API | Implementar manualmente | Incluido |
| `DoubleEndedIterator` | Implementar manualmente | Incluido |
| Código | ~150 líneas | 0 líneas (stdlib) |
| Aprendizaje | Excelente | Ninguno (caja negra) |
| Personalización | Total | Ninguna |

**Recomendación**: implementar AVL para aprender, usar `BTreeMap`/
`BTreeSet` en producción.

---

## Cuándo implementar propio

| Situación | ¿Propio? |
|-----------|---------|
| Aprender árboles balanceados | Sí — es el objetivo |
| Necesitas estadísticas de orden (k-ésimo elemento) | Sí — BTreeMap no lo soporta |
| Necesitas split/join eficiente | Depende — BTreeMap tiene `split_off` |
| Necesitas nodo augmented (con datos extra) | Sí — BTreeMap no lo permite |
| Código de producción general | No — usa BTreeMap |
| Entrevista técnica | Depende — muestra que entiendes la estructura |

---

## Drop seguro para árboles profundos

El Drop por defecto de `Option<Box<Node<T>>>` es recursivo — puede
desbordar el stack con árboles degenerados.  Para un AVL esto es poco
probable ($h \leq 1.44 \log_2 n$, así que $n = 10^6$ → $h \approx 29$),
pero un Drop iterativo es más robusto:

```rust
impl<T: Ord> Drop for AVL<T> {
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
            // node se destruye aquí
        }
    }
}
```

Para un AVL esto es una precaución.  Para un BST sin balance, es
necesario.

---

## Ejercicios

### Ejercicio 1 — Construir y consultar

Crea un `AVL<i32>` con los valores `[50, 25, 75, 10, 30, 60, 80]`.
¿Cuál es la altura?  ¿Cuántas rotaciones ocurren?

<details>
<summary>Predicción</summary>

El orden de inserción produce un árbol perfecto sin rotaciones:

```
         50
        /  \
      25    75
      / \   / \
    10  30 60  80
```

Altura: **2**.  Rotaciones: **0**.

Cada inserción mantiene balance en {-1, 0, +1} naturalmente porque
el orden (raíz → nivel 1 → nivel 2) llena el árbol nivel por nivel.
</details>

### Ejercicio 2 — Forzar rotaciones

Inserta `[1, 2, 3, 4, 5]` en un `AVL<i32>`.  ¿Cuántas rotaciones
ocurren y de qué tipo?

<details>
<summary>Traza</summary>

```
1: [1]
2: [1, 2] — OK
3: [1, 2, 3] — balance(1)=+2, RR → rotate_left(1)
   Resultado: [2; 1, 3]  — 1 rotación

4: [2; 1, 3→4] — balance(3)=+1, balance(2)=+1 → OK

5: [2; 1, 3→4→5] — balance(3)=+2, RR → rotate_left(3)
   Resultado: [2; 1, 4; 3, 5] — 1 rotación
   Pero balance(2)=+1 → OK
```

Total: **2 rotaciones**, ambas `rotate_left` (caso RR).

Resultado: `[2; 1, 4; 3, 5]` con altura 2.
</details>

### Ejercicio 3 — BTreeSet range

Usando `BTreeSet`, encuentra todos los valores entre 10 y 20
(inclusive) en el conjunto `{3, 7, 10, 12, 15, 18, 20, 25, 30}`.

<details>
<summary>Código</summary>

```rust
use std::collections::BTreeSet;

let set: BTreeSet<i32> = [3, 7, 10, 12, 15, 18, 20, 25, 30]
    .into_iter().collect();

let range: Vec<_> = set.range(10..=20).collect();
assert_eq!(range, vec![&10, &12, &15, &18, &20]);
```

`range(10..=20)` usa `RangeInclusive`.  La búsqueda del inicio es
$O(\log n)$, luego la iteración es $O(k)$ donde $k = 5$.
</details>

### Ejercicio 4 — BTreeMap entry API

Cuenta la frecuencia de cada carácter en `"abracadabra"` usando
`BTreeMap` y la entry API.

<details>
<summary>Código</summary>

```rust
use std::collections::BTreeMap;

let mut freq = BTreeMap::new();
for ch in "abracadabra".chars() {
    *freq.entry(ch).or_insert(0) += 1;
}

for (ch, count) in &freq {
    println!("{ch}: {count}");
}
// a: 5
// b: 2
// c: 1
// d: 1
// r: 2
```

La salida está ordenada por clave (carácter) porque es un BTreeMap.
Con HashMap la salida estaría en orden arbitrario.
</details>

### Ejercicio 5 — Iterator lazy

Usando el `InorderIter` del AVL, obtén el tercer elemento más pequeño
sin recorrer todo el árbol.

<details>
<summary>Código</summary>

```rust
let mut avl = AVL::new();
for i in [10, 5, 15, 3, 7, 12, 20] {
    avl.insert(i);
}

let third = avl.iter().nth(2);  // 0-indexed: 3rd element
assert_eq!(third, Some(&7));
```

`nth(2)` llama `next()` 3 veces y se detiene.  Solo se visitan los
nodos 3, 5 y 7 — no se recorre el resto del árbol.

Complejidad: $O(h + k)$ donde $k = 3$.  Para un AVL con $10^6$ nodos:
~23 operaciones en vez de $10^6$.
</details>

### Ejercicio 6 — Implementar floor en AVL

Añade `floor(&self, key: &T) -> Option<&T>` a la struct AVL.

<details>
<summary>Implementación</summary>

```rust
impl<T: Ord> AVL<T> {
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
```

Idéntico al BST — la búsqueda no depende del mecanismo de balance.
La diferencia es que en el AVL, siempre tarda $O(\log n)$; en un BST
podría tardar $O(n)$.
</details>

### Ejercicio 7 — AVL de structs

Crea un AVL de `Student` ordenado por nota, donde:

```rust
#[derive(Eq, PartialEq)]
struct Student { name: String, grade: u32 }
```

<details>
<summary>Implementación</summary>

```rust
#[derive(Eq, PartialEq)]
struct Student {
    name: String,
    grade: u32,
}

impl Ord for Student {
    fn cmp(&self, other: &Self) -> Ordering {
        self.grade.cmp(&other.grade)
            .then(self.name.cmp(&other.name))  // desempate por nombre
    }
}

impl PartialOrd for Student {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

let mut students = AVL::new();
students.insert(Student { name: "Ana".into(), grade: 95 });
students.insert(Student { name: "Bob".into(), grade: 87 });
students.insert(Student { name: "Carol".into(), grade: 92 });

// Iteración en orden de nota
for s in students.iter() {
    println!("{}: {}", s.name, s.grade);
}
// Bob: 87
// Carol: 92
// Ana: 95
```

`Ord` se implementa manualmente para ordenar por `grade` primero.
El `then` desempata por nombre cuando las notas son iguales.
</details>

### Ejercicio 8 — Comparar rendimiento

Inserta $10^5$ valores en orden ascendente en un `AVL<i32>` propio y en
un `BTreeSet<i32>`.  ¿Cuál es más rápido?

<details>
<summary>Análisis</summary>

```rust
use std::time::Instant;

let n = 100_000;

let t1 = Instant::now();
let mut avl = AVL::new();
for i in 0..n { avl.insert(i); }
println!("AVL: {:?}", t1.elapsed());

let t2 = Instant::now();
let mut btree = BTreeSet::new();
for i in 0..n { btree.insert(i); }
println!("BTreeSet: {:?}", t2.elapsed());
```

Resultado típico: `BTreeSet` es 2-5× más rápido.

Razones:
1. Cache locality: BTreeSet almacena múltiples claves por nodo.
2. Menos asignaciones: ~$n/B$ nodos vs $n$ nodos.
3. Código optimizado: la stdlib está altamente optimizada.

El AVL propio es un ejercicio de aprendizaje excelente, pero no compite
con la stdlib en rendimiento.
</details>

### Ejercicio 9 — is_avl para la struct

Implementa `is_avl(&self) -> bool` que verifique el invariante AVL
(útil para pruebas).

<details>
<summary>Implementación</summary>

```rust
impl<T: Ord> AVL<T> {
    pub fn is_avl(&self) -> bool {
        Self::check_avl(&self.root).is_some()
    }

    // Retorna Some(height) si es AVL válido, None si no
    fn check_avl(tree: &Link<T>) -> Option<i32> {
        match tree {
            None => Some(-1),
            Some(node) => {
                let lh = Self::check_avl(&node.left)?;
                let rh = Self::check_avl(&node.right)?;

                // Verificar balance
                if (rh - lh).abs() > 1 { return None; }

                // Verificar altura almacenada
                let expected = 1 + lh.max(rh);
                if node.height != expected { return None; }

                Some(expected)
            }
        }
    }
}
```

Retorna `None` en cuanto encuentra una violación (early return con `?`).
Verifica tanto el balance como que las alturas almacenadas sean
correctas.
</details>

### Ejercicio 10 — ¿Cuándo falla el Drop recursivo?

Para un AVL con $n = 10^6$ nodos, ¿el Drop recursivo por defecto
desborda el stack?  Calcula la profundidad máxima de recursión.

<details>
<summary>Cálculo</summary>

Altura máxima AVL para $n = 10^6$:

$$h \leq 1.44 \log_2(10^6 + 2) \approx 1.44 \times 20 = 28.8 \approx 29$$

El Drop recursivo tiene profundidad = altura = ~29 frames.  Cada frame
~100 bytes → ~2.9 KB de stack.  El stack típico es 8 MB.

**No desborda.**  El Drop recursivo es seguro para AVL porque la altura
está garantizada en $O(\log n)$.

Comparar con BST sin balance: $n = 10^6$ → $h = 999999$ → ~100 MB de
stack → **desborda**.

El Drop iterativo es necesario para BSTs sin balance, pero es solo una
precaución para AVL.  Implementarlo no hace daño y cubre el caso
teórico.
</details>
