# Implementación Rust safe

## Por qué las listas son incómodas en Rust

En C, una lista enlazada es natural: punteros crudos, `malloc`, `free`, sin
restricciones de ownership.  En Rust, el modelo de ownership lineal choca con
la estructura de una lista:

1. **Un solo dueño**: cada `Box<Node>` tiene un único dueño.  Bien para la
   cadena `head → node1 → node2`, pero dificulta operaciones que necesitan
   acceso simultáneo a dos nodos (ej. insertar entre node1 y node2).

2. **No hay punteros nulos**: `Option<Box<Node>>` es seguro pero verboso.
   Cada acceso requiere `match`, `if let`, `as_ref()`, `as_mut()`.

3. **Mover vs copiar**: insertar al frente requiere *mover* todo `head` al
   campo `next` del nuevo nodo.  Conceptualmente simple, pero el compilador
   verifica que nadie más use el viejo `head`.

4. **Mutabilidad exclusiva**: no puedes tener `&mut` a un nodo y
   simultáneamente `&mut` a otro nodo de la misma cadena — aunque sean nodos
   distintos.

Nada de esto es un defecto de Rust — es el *precio* de la seguridad de
memoria.  Las listas enlazadas son el caso clásico donde ese precio es alto.

---

## Implementación con Option<Box<Node<T>>>

### Estructura

```rust
pub struct List<T> {
    head: Option<Box<Node<T>>>,
}

struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}
```

`List<T>` es un wrapper que encapsula el `head`.  El usuario nunca ve `Node`
directamente.

### new

```rust
impl<T> List<T> {
    pub fn new() -> Self {
        List { head: None }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }
}
```

### push_front — $O(1)$

```rust
pub fn push_front(&mut self, value: T) {
    let node = Box::new(Node {
        data: value,
        next: self.head.take(),   // tomar ownership del viejo head
    });
    self.head = Some(node);
}
```

`self.head.take()` es la operación clave: extrae el valor de `self.head`
(dejándolo `None`) y lo mueve al campo `next` del nuevo nodo.  Luego
`self.head` se reemplaza con el nuevo nodo.

```
Antes:  self.head = Some(Box([10] → [20] → None))

take():  self.head = None       (temporalmente)
          old_head = Some(Box([10] → [20] → None))

Nuevo nodo: Box([5] → old_head) = Box([5] → [10] → [20] → None)

self.head = Some(Box([5] → [10] → [20] → None))
```

Sin `take()`, intentar mover `self.head` directamente falla — Rust no permite
mover un campo de una referencia `&mut` sin dejar algo en su lugar.

### pop_front — $O(1)$

```rust
pub fn pop_front(&mut self) -> Option<T> {
    self.head.take().map(|old_head| {
        self.head = old_head.next;
        old_head.data
    })
}
```

`take()` extrae el head.  `map` lo procesa si es `Some`: reconecta `self.head`
al siguiente nodo y retorna el dato.

### peek — $O(1)$

```rust
pub fn peek(&self) -> Option<&T> {
    self.head.as_ref().map(|node| &node.data)
}

pub fn peek_mut(&mut self) -> Option<&mut T> {
    self.head.as_mut().map(|node| &mut node.data)
}
```

`as_ref()` convierte `&Option<Box<Node>>` en `Option<&Box<Node>>` sin mover.
`map` extrae una referencia al dato.

### push_back — $O(n)$

```rust
pub fn push_back(&mut self, value: T) {
    let new_node = Some(Box::new(Node { data: value, next: None }));

    // Buscar el último nodo
    let mut current = &mut self.head;
    while let Some(node) = current {
        current = &mut node.next;
    }
    // current ahora es &mut None (el campo next del último nodo, o head si vacía)
    *current = new_node;
}
```

El patrón `&mut self.head` + recorrido con `&mut node.next` es análogo al
`Node **` de C: `current` siempre apunta al `Option` que queremos modificar.

### len — $O(n)$

```rust
pub fn len(&self) -> usize {
    let mut count = 0;
    let mut current = &self.head;
    while let Some(node) = current {
        count += 1;
        current = &node.next;
    }
    count
}
```

### contains — $O(n)$

```rust
pub fn contains(&self, value: &T) -> bool
where T: PartialEq
{
    let mut current = &self.head;
    while let Some(node) = current {
        if &node.data == value { return true; }
        current = &node.next;
    }
    false
}
```

---

## Iteradores

### Iterador por referencia

```rust
pub struct Iter<'a, T> {
    current: Option<&'a Node<T>>,
}

impl<T> List<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        Iter {
            current: self.head.as_deref(),
        }
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        self.current.map(|node| {
            self.current = node.next.as_deref();
            &node.data
        })
    }
}
```

`as_deref()` convierte `Option<&Box<Node<T>>>` en `Option<&Node<T>>` —
desreferencia a través del `Box`.

### Iterador por referencia mutable

```rust
pub struct IterMut<'a, T> {
    current: Option<&'a mut Node<T>>,
}

impl<T> List<T> {
    pub fn iter_mut(&mut self) -> IterMut<'_, T> {
        IterMut {
            current: self.head.as_deref_mut(),
        }
    }
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
        self.current.take().map(|node| {
            self.current = node.next.as_deref_mut();
            &mut node.data
        })
    }
}
```

`take()` en el iterador mutable es necesario para evitar aliasing de `&mut` —
al tomar `self.current`, liberamos la referencia mutable al nodo actual antes
de crear una al siguiente.

### Iterador consumidor (into_iter)

```rust
impl<T> IntoIterator for List<T> {
    type Item = T;
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        IntoIter { list: self }
    }
}

pub struct IntoIter<T> {
    list: List<T>,
}

impl<T> Iterator for IntoIter<T> {
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        self.list.pop_front()
    }
}
```

Este iterador **consume** la lista — cada `next()` llama a `pop_front()`,
destruyendo el nodo.

```rust
let list = List::from_iter([10, 20, 30]);

for val in list {             // into_iter — consume la lista
    println!("{val}");
}
// list ya no existe aquí
```

---

## Drop personalizado

El `Drop` por defecto de Rust destruye recursivamente: `head` destruye su
`Box`, que destruye su `Node`, que destruye su `next` (otro `Box`), etc.  Para
listas largas, esto desborda la pila:

```rust
impl<T> Drop for List<T> {
    fn drop(&mut self) {
        let mut current = self.head.take();
        while let Some(mut node) = current {
            current = node.next.take();
            // node se destruye aquí, pero su next ya es None → sin recursión
        }
    }
}
```

`take()` reemplaza el campo con `None` antes de que `Drop` lo destruya
recursivamente.  Profundidad de pila: $O(1)$.

---

## Estilo funcional: enum List

Una representación alternativa, inspirada en Haskell/ML:

```rust
enum FList<T> {
    Nil,
    Cons(T, Box<FList<T>>),
}
```

`Cons` viene del Lisp (*construct*): un valor y el resto de la lista.

```rust
use FList::{Nil, Cons};

fn main() {
    // Lista [1, 2, 3]
    let list = Cons(1, Box::new(
                   Cons(2, Box::new(
                       Cons(3, Box::new(Nil))))));

    // Recorrer
    fn print_list(list: &FList<i32>) {
        match list {
            Nil => println!(),
            Cons(val, rest) => {
                print!("{val} ");
                print_list(rest);
            }
        }
    }
    print_list(&list);   // 1 2 3
}
```

### Operaciones funcionales

```rust
impl<T> FList<T> {
    fn new() -> Self {
        Nil
    }

    // Prepend — O(1), retorna nueva lista (inmutable)
    fn prepend(self, value: T) -> Self {
        Cons(value, Box::new(self))
    }

    fn head(&self) -> Option<&T> {
        match self {
            Nil => None,
            Cons(val, _) => Some(val),
        }
    }

    fn tail(&self) -> Option<&FList<T>> {
        match self {
            Nil => None,
            Cons(_, rest) => Some(rest),
        }
    }

    fn is_empty(&self) -> bool {
        matches!(self, Nil)
    }
}
```

### Funcionales: map, filter, fold

```rust
impl<T: Clone> FList<T> {
    fn map<U, F: Fn(&T) -> U>(&self, f: F) -> FList<U> {
        match self {
            Nil => Nil,
            Cons(val, rest) => Cons(f(val), Box::new(rest.map(f))),
        }
    }

    fn filter<F: Fn(&T) -> bool>(&self, pred: F) -> FList<T> {
        match self {
            Nil => Nil,
            Cons(val, rest) => {
                if pred(val) {
                    Cons(val.clone(), Box::new(rest.filter(pred)))
                } else {
                    rest.filter(pred)
                }
            }
        }
    }

    fn fold<U, F: Fn(U, &T) -> U>(&self, init: U, f: F) -> U {
        match self {
            Nil => init,
            Cons(val, rest) => rest.fold(f(init, val), f),
        }
    }

    fn length(&self) -> usize {
        self.fold(0, |acc, _| acc + 1)
    }
}
```

### Struct vs enum: comparación

| Aspecto | `List<T>` (struct + Node) | `FList<T>` (enum) |
|---------|--------------------------|-------------------|
| Estilo | Imperativo (mutación) | Funcional (inmutable) |
| push_front | `&mut self` — modifica in-place | Retorna nueva lista, consume la vieja |
| pop_front | `&mut self` → `Option<T>` | Destructuring con `match` |
| Compartir sublistas | Imposible (ownership único) | Imposible con `Box` (un solo dueño) |
| Legibilidad | Más código, más explícito | Más conciso, más recursivo |
| Rendimiento | Similar | Similar (pero recursión puede ser más lenta) |
| Uso idiomático en Rust | Más común en producción | Más común en aprendizaje/funcional |

Ninguna de las dos representaciones permite compartir sublistas (structural
sharing) con `Box`, porque `Box` implica ownership único.  Para eso se necesita
`Rc<...>` (reference counting), que no cubrimos en este tópico.

---

## Métodos adicionales

### reverse — $O(n)$

```rust
impl<T> List<T> {
    pub fn reverse(&mut self) {
        let mut prev = None;
        let mut current = self.head.take();
        while let Some(mut node) = current {
            let next = node.next.take();
            node.next = prev;
            prev = Some(node);
            current = next;
        }
        self.head = prev;
    }
}
```

Mismo algoritmo de 3 punteros que en C (T02), pero con `take()` para mover
ownership en cada paso.

### from_iter: construir desde iterador

```rust
impl<T> FromIterator<T> for List<T> {
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        let mut list = List::new();
        let items: Vec<T> = iter.into_iter().collect();
        // Insertar en reversa para mantener el orden
        for item in items.into_iter().rev() {
            list.push_front(item);
        }
        list
    }
}

// Uso:
let list: List<i32> = [10, 20, 30].into_iter().collect();
```

### Display

```rust
impl<T: std::fmt::Display> std::fmt::Display for List<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[")?;
        let mut first = true;
        for val in self.iter() {
            if !first { write!(f, ", ")?; }
            write!(f, "{val}")?;
            first = false;
        }
        write!(f, "]")
    }
}

// println!("{list}");  → [10, 20, 30]
```

---

## Programa completo

```rust
fn main() {
    let mut list = List::new();
    assert!(list.is_empty());

    // push_front
    list.push_front(30);
    list.push_front(20);
    list.push_front(10);
    println!("After push_front: {list}");     // [10, 20, 30]

    // push_back
    list.push_back(40);
    list.push_back(50);
    println!("After push_back: {list}");      // [10, 20, 30, 40, 50]

    // peek
    println!("peek: {:?}", list.peek());      // Some(10)

    // pop_front
    println!("pop: {:?}", list.pop_front());  // Some(10)
    println!("After pop: {list}");            // [20, 30, 40, 50]

    // len, contains
    println!("len: {}", list.len());          // 4
    println!("has 30: {}", list.contains(&30)); // true

    // iter + sum
    let sum: i32 = list.iter().sum();
    println!("sum: {sum}");                   // 140

    // iter_mut: duplicar cada valor
    for val in list.iter_mut() {
        *val *= 2;
    }
    println!("After double: {list}");         // [40, 60, 80, 100]

    // reverse
    list.reverse();
    println!("Reversed: {list}");             // [100, 80, 60, 40]

    // into_iter (consume)
    let collected: Vec<i32> = list.into_iter().collect();
    println!("Collected: {collected:?}");     // [100, 80, 60, 40]
    // list ya no existe
}
```

---

## Comparación con C

| Aspecto | C | Rust safe |
|---------|---|-----------|
| Crear nodo | `malloc` + inicializar + verificar NULL | `Box::new(Node { ... })` |
| Modificar head | `Node **head` | `&mut self` + `take()` |
| Liberar | `free` manual, recorrer toda la lista | Automático (`Drop`) |
| NULL check | `if (ptr == NULL)` — fácil de olvidar | `Option` — compilador obliga |
| Recorrido | `while (cur != NULL) cur = cur->next` | `while let Some(node) = current` |
| Insertar en medio | Redirigir 2 punteros | `take()` + reasignar |
| Memory leak | Posible si se olvida `free` | Imposible en safe Rust |
| Use-after-free | Posible | Imposible en safe Rust |
| Verbosidad | Baja (punteros directos) | Alta (`Option`, `Box`, `take`, `as_ref`) |
| Ergonomía para listas | Natural | Incómoda |

La conclusión pragmática: en Rust, si necesitas una lista enlazada, usa
`VecDeque` o `Vec` (que cubren el 95% de los casos).  La lista enlazada manual
se justifica para aprender ownership, o cuando se necesita inserción $O(1)$ en
medio con un cursor.

---

## Ejercicios

### Ejercicio 1 — Lista básica

Implementa `List<T>` con `new`, `push_front`, `pop_front`, `peek`, `is_empty`,
`len`.  Prueba con `i32` y con `String`.

<details>
<summary>Prueba con String</summary>

```rust
let mut list = List::new();
list.push_front("gamma".to_string());
list.push_front("beta".to_string());
list.push_front("alpha".to_string());
assert_eq!(list.pop_front(), Some("alpha".to_string()));
```

`String` no es `Copy` — `pop_front` **mueve** el String fuera del nodo.
</details>

### Ejercicio 2 — take() paso a paso

Traza manualmente qué ocurre con `self.head` durante `push_front(5)` cuando
la lista contiene `[10, 20]`.  Muestra el estado de `self.head` antes del
`take()`, después del `take()`, y al final.

<details>
<summary>Traza</summary>

```
Antes:     self.head = Some(Box([10] → [20] → None))

take():    self.head = None
           old = Some(Box([10] → [20] → None))

new_node:  Box([5] → old) = Box([5] → [10] → [20] → None)

Final:     self.head = Some(Box([5] → [10] → [20] → None))
```

`take()` es necesario porque Rust no permite mover `self.head` (un campo de
`&mut self`) sin dejar algo en su lugar.  `take()` deja `None`.
</details>

### Ejercicio 3 — push_back sin recorrido

`push_back` es $O(n)$ porque recorre hasta el final.  Añade un campo `tail:
*mut Node<T>` (raw pointer) a `List<T>` para hacer `push_back` en $O(1)$.
¿Es esto safe Rust?

<details>
<summary>Respuesta</summary>

No — `*mut Node<T>` requiere `unsafe` para desreferenciar.  Es exactamente
la misma situación que la cola con lista enlazada (C04/S01/T04).  El raw
pointer `tail` es necesario porque `Box` no permite tener dos dueños del
mismo nodo.  Alternativa safe: recorrer con `&mut` cada vez ($O(n)$), o usar
`VecDeque`.
</details>

### Ejercicio 4 — Estilo funcional

Implementa `FList<T>` con `prepend`, `head`, `tail`, `map`, `filter`, `fold`.
Usa la lista `[1, 2, 3, 4, 5]` para:
1. Filtrar los impares.
2. Mapear multiplicando por 10.
3. Sumar con fold.

<details>
<summary>Resultado</summary>

Impares: `[1, 3, 5]`.  Multiplicar por 10: `[10, 30, 50]`.  Suma: 90.

```rust
let list = Nil.prepend(5).prepend(4).prepend(3).prepend(2).prepend(1);
let odds = list.filter(|x| x % 2 != 0);
let scaled = odds.map(|x| x * 10);
let sum = scaled.fold(0, |acc, x| acc + x);
```
</details>

### Ejercicio 5 — Los tres iteradores

Implementa `iter()`, `iter_mut()` e `into_iter()` para `List<i32>`.  Demuestra
la diferencia:
1. `iter()`: recorre sin consumir.
2. `iter_mut()`: modifica cada elemento.
3. `into_iter()`: consume la lista.

<details>
<summary>Ejemplo</summary>

```rust
let mut list = List::from_iter([1, 2, 3]);

// iter — borrow inmutable
let sum: i32 = list.iter().sum();

// iter_mut — borrow mutable
for val in list.iter_mut() { *val += 10; }
// list ahora es [11, 12, 13]

// into_iter — consume
for val in list { println!("{val}"); }
// list ya no existe
```
</details>

### Ejercicio 6 — Drop iterativo

Crea una lista de 200,000 nodos.  Verifica que el `Drop` por defecto (sin
implementación manual) causa stack overflow.  Luego implementa el `Drop`
iterativo y verifica que funciona.

<details>
<summary>Cómo verificar</summary>

```rust
fn test_large_drop() {
    let mut list = List::new();
    for i in 0..200_000 {
        list.push_front(i);
    }
    // Al salir del scope, Drop se ejecuta
}
```

Sin Drop manual: `thread 'main' has overflowed its stack`.
Con Drop iterativo: completa sin error.
</details>

### Ejercicio 7 — insert_at y remove_at

Implementa:
```rust
fn insert_at(&mut self, pos: usize, value: T) -> bool;
fn remove_at(&mut self, pos: usize) -> Option<T>;
```

Usa el patrón de recorrido con `&mut Option<Box<Node>>`.

<details>
<summary>Pista para insert_at</summary>

```rust
pub fn insert_at(&mut self, pos: usize, value: T) -> bool {
    let mut current = &mut self.head;
    for _ in 0..pos {
        match current {
            Some(node) => current = &mut node.next,
            None => return false,
        }
    }
    let new_node = Box::new(Node {
        data: value,
        next: current.take(),
    });
    *current = Some(new_node);
    true
}
```
</details>

### Ejercicio 8 — PartialEq para List

Implementa `PartialEq` para `List<T>` donde `T: PartialEq`.  Dos listas son
iguales si tienen los mismos datos en el mismo orden.

<details>
<summary>Implementación</summary>

```rust
impl<T: PartialEq> PartialEq for List<T> {
    fn eq(&self, other: &Self) -> bool {
        let mut a = self.iter();
        let mut b = other.iter();
        loop {
            match (a.next(), b.next()) {
                (Some(x), Some(y)) => if x != y { return false; },
                (None, None) => return true,
                _ => return false,
            }
        }
    }
}
```
</details>

### Ejercicio 9 — Collect y FromIterator

Implementa `FromIterator<T>` para `List<T>`.  Luego demuestra:

```rust
let list: List<i32> = (1..=5).collect();
let doubled: List<i32> = list.iter().map(|x| x * 2).collect();
```

<details>
<summary>Reto: mantener el orden</summary>

`push_front` invierte el orden.  Para mantener el orden original del iterador,
hay que recolectar primero y luego insertar en reversa, o usar `push_back`
(que es $O(n)$ por inserción pero correcto).  El enfoque con reversa al final
es $O(n)$ total:

```rust
impl<T> FromIterator<T> for List<T> {
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        let mut list = List::new();
        for item in iter { list.push_front(item); }
        list.reverse();
        list
    }
}
```
</details>

### Ejercicio 10 — Benchmark: List vs Vec vs VecDeque

Para $n = 10^6$ operaciones de `push_front`:
1. `List<i32>` (push_front — $O(1)$ cada una).
2. `Vec<i32>` (insert(0, val) — $O(n)$ cada una).
3. `VecDeque<i32>` (push_front — $O(1)$ amortizado).

Mide tiempos.  ¿Gana la lista?

<details>
<summary>Predicción</summary>

- `Vec::insert(0)`: $O(n)$ por operación × $10^6$ = $O(n^2)$.  Minutos.
- `List::push_front`: $O(1)$ × $10^6$ = $O(n)$.  Decenas de milisegundos.
- `VecDeque::push_front`: $O(1)$ amortizado × $10^6$ = $O(n)$.  Pocos ms.

`VecDeque` probablemente gana a `List` pese a tener la misma complejidad
asintótica, gracias a la localidad de caché (datos contiguos vs nodos
dispersos).  `List` gana cómodamente a `Vec` para push_front.

Este benchmark muestra que la complejidad asintótica no es todo — las
constantes de caché importan enormemente en hardware real.
</details>
