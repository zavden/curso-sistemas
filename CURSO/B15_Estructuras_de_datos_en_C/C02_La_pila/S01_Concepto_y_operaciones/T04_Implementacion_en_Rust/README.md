# T04 — Implementación en Rust

---

## 1. Dos implementaciones, un trait

En C implementamos el stack con array y con lista enlazada. En Rust haremos
lo mismo, pero con las herramientas del lenguaje:

```
C array         →  Rust Vec<T>
C lista          →  Rust Option<Box<Node<T>>>
C void *        →  Rust generics <T>
C stack_destroy  →  Rust Drop (automático)
C bool + out     →  Rust Option<T>
```

Ambas implementaciones satisfacen el mismo trait:

```rust
pub trait Stack<T> {
    fn new() -> Self;
    fn push(&mut self, elem: T);
    fn pop(&mut self) -> Option<T>;
    fn peek(&self) -> Option<&T>;
    fn is_empty(&self) -> bool;
    fn len(&self) -> usize;
}
```

---

## 2. Implementación con Vec\<T\>

`Vec<T>` ya tiene `push` y `pop`. El stack es esencialmente un wrapper:

```rust
pub struct VecStack<T> {
    data: Vec<T>,
}

impl<T> VecStack<T> {
    pub fn new() -> Self {
        VecStack { data: Vec::new() }
    }

    pub fn push(&mut self, elem: T) {
        self.data.push(elem);
    }

    pub fn pop(&mut self) -> Option<T> {
        self.data.pop()
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.last()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }
}
```

### Por qué funciona

`Vec::push` agrega al final y `Vec::pop` remueve del final — exactamente
el comportamiento LIFO de un stack:

```
vec.push(10):   [10]
vec.push(20):   [10, 20]
vec.push(30):   [10, 20, 30]
vec.pop():      [10, 20]     → Some(30)
vec.pop():      [10]         → Some(20)
```

El final del `Vec` es el tope del stack.

### Complejidades

| Operación | Complejidad | Notas |
|-----------|------------|-------|
| `push` | $O(1)$ amortizado | $O(n)$ cuando realloc |
| `pop` | $O(1)$ | Siempre |
| `peek` | $O(1)$ | `.last()` retorna referencia al último |
| `is_empty` | $O(1)$ | Compara `len == 0` |
| `len` | $O(1)$ | Campo almacenado |

### Lo que no necesitas hacer

```rust
// No necesitas:
// - malloc/free → Vec maneja la memoria
// - verificar capacidad → Vec crece automáticamente
// - implementar Drop → Vec libera todo al salir del scope
// - verificar null → Option<T> maneja la ausencia de valor
// - memcpy para genéricos → los generics de Rust son monomorphized
```

### Con capacidad inicial

Si conoces el tamaño aproximado, puedes preasignar:

```rust
pub fn with_capacity(cap: usize) -> Self {
    VecStack { data: Vec::with_capacity(cap) }
}
```

Esto evita reallocs si el uso no excede `cap`. Equivalente a
`stack_create(capacity)` en C pero sin el límite fijo — si excedes `cap`,
el Vec crece automáticamente.

---

## 3. Implementación con lista enlazada

### El nodo

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}
```

`Option<Box<Node<T>>>` es el equivalente safe de `Node *next` en C:
- `None` = `NULL` (fin de la lista)
- `Some(Box<Node>)` = puntero válido a un nodo en el heap

### El stack

```rust
pub struct ListStack<T> {
    head: Option<Box<Node<T>>>,
    len: usize,
}

impl<T> ListStack<T> {
    pub fn new() -> Self {
        ListStack { head: None, len: 0 }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::new(Node {
            data,
            next: self.head.take(),
        });
        self.head = Some(node);
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        let node = self.head.take()?;
        self.head = node.next;
        self.len -= 1;
        Some(node.data)
    }

    pub fn peek(&self) -> Option<&T> {
        self.head.as_ref().map(|node| &node.data)
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}
```

### Drop automático

No necesitas implementar `Drop`. Cuando el `ListStack` sale del scope:

1. Rust llama `drop` en `self.head`
2. `Option<Box<Node>>` destruye el `Box`, que destruye el `Node`
3. El `Node` contiene `next: Option<Box<Node>>`, que se destruye recursivamente
4. Todos los nodos se liberan automáticamente

Sin embargo, para listas muy largas ($n > 10^5$), la destrucción recursiva
puede causar stack overflow del call stack. Una implementación iterativa de
`Drop` evita esto:

```rust
impl<T> Drop for ListStack<T> {
    fn drop(&mut self) {
        let mut current = self.head.take();
        while let Some(mut node) = current {
            current = node.next.take();
            // node se destruye aquí (sin recursión)
        }
    }
}
```

---

## 4. La clave: `take()`

El método `Option::take()` es fundamental en estas implementaciones. Extrae
el valor del `Option` y lo reemplaza con `None`:

```rust
let mut x: Option<i32> = Some(42);
let val = x.take();   // val = Some(42), x = None
```

### Por qué `take` y no asignación directa

En Rust, no puedes mover un valor fuera de una referencia `&mut` directamente
porque dejarías el campo en un estado inválido:

```rust
// Esto NO compila:
pub fn pop_bad(&mut self) -> Option<T> {
    let node = self.head;   // error: cannot move out of `self.head`
    // ...                  // self.head quedaría sin valor (inválido)
}
```

`take()` resuelve esto: extrae el valor **y deja `None` en su lugar**,
manteniendo `self.head` en un estado válido:

```rust
pub fn pop(&mut self) -> Option<T> {
    let node = self.head.take();   // self.head = None (válido)
    // ...
}
```

### take en push

```rust
pub fn push(&mut self, data: T) {
    let node = Box::new(Node {
        data,
        next: self.head.take(),   // extrae head actual, deja None
    });
    self.head = Some(node);       // pone el nuevo nodo como head
}
```

Sin `take`, no puedes mover `self.head` al campo `next` del nuevo nodo.

### Comparación con C

```c
// C: simplemente reasigna punteros
node->next = s->top;   // copia el puntero
s->top = node;          // sobreescribe (el antiguo valor de s->top
                         // sigue accesible vía node->next)
```

En C no hay problema porque los punteros se copian libremente. En Rust,
`Box<Node>` tiene **ownership único** — no se puede copiar, solo mover.
`take()` es el mecanismo de Rust para mover un valor fuera de un campo
mientras se deja el campo en un estado válido.

---

## 5. Implementar el trait

```rust
pub trait StackTrait<T> {
    fn new() -> Self;
    fn push(&mut self, elem: T);
    fn pop(&mut self) -> Option<T>;
    fn peek(&self) -> Option<&T>;
    fn is_empty(&self) -> bool;
    fn len(&self) -> usize;
}

impl<T> StackTrait<T> for VecStack<T> {
    fn new() -> Self { VecStack::new() }
    fn push(&mut self, elem: T) { self.push(elem) }
    fn pop(&mut self) -> Option<T> { self.pop() }
    fn peek(&self) -> Option<&T> { self.peek() }
    fn is_empty(&self) -> bool { self.is_empty() }
    fn len(&self) -> usize { self.len() }
}

impl<T> StackTrait<T> for ListStack<T> {
    fn new() -> Self { ListStack::new() }
    fn push(&mut self, elem: T) { self.push(elem) }
    fn pop(&mut self) -> Option<T> { self.pop() }
    fn peek(&self) -> Option<&T> { self.peek() }
    fn is_empty(&self) -> bool { self.is_empty() }
    fn len(&self) -> usize { self.len() }
}
```

### Código genérico sobre el trait

```rust
fn test_stack<S: StackTrait<i32>>() {
    let mut s = S::new();
    s.push(10);
    s.push(20);
    s.push(30);
    assert_eq!(s.peek(), Some(&30));
    assert_eq!(s.pop(), Some(30));
    assert_eq!(s.pop(), Some(20));
    assert_eq!(s.len(), 1);
    assert_eq!(s.pop(), Some(10));
    assert!(s.is_empty());
    assert_eq!(s.pop(), None);
}

fn main() {
    test_stack::<VecStack<i32>>();
    test_stack::<ListStack<i32>>();
    println!("Both implementations pass!");
}
```

Este es el equivalente en Rust de compilar `main.c` con `stack_array.c` o
`stack_list.c` en C — mismo código de test, diferentes implementaciones.

---

## 6. Comparación de ergonomía: C vs Rust

### Push

```c
// C con int
bool ok = stack_push(s, 42);
if (!ok) { /* handle overflow */ }

// C genérico
int x = 42;
bool ok = stack_push(s, &x);  // pasa puntero, memcpy interno

// Rust
s.push(42);   // crece automáticamente, no puede fallar (panic en OOM)
```

### Pop

```c
// C
int val;
if (stack_pop(s, &val)) {
    printf("%d\n", val);
} else {
    printf("empty\n");
}

// Rust
match s.pop() {
    Some(val) => println!("{val}"),
    None => println!("empty"),
}

// O más conciso:
if let Some(val) = s.pop() {
    println!("{val}");
}
```

### Peek

```c
// C
int val;
if (stack_peek(s, &val)) {
    printf("top = %d\n", val);
}

// Rust
if let Some(val) = s.peek() {
    println!("top = {val}");
}
```

### Destroy

```c
// C: obligatorio, fácil de olvidar
stack_destroy(s);
// Si olvidas → leak silencioso

// Rust: automático
// s sale del scope → Drop se llama automáticamente
// Imposible olvidar → imposible leak (con Box/Vec)
```

### Genéricos

```c
// C: void * + elem_size, sin type safety
GenericStack *si = gstack_create(10, sizeof(int));
GenericStack *sd = gstack_create(10, sizeof(double));
// Nada impide hacer push de int al stack de double → bug silencioso

// Rust: <T>, type safety en compile time
let mut si: VecStack<i32> = VecStack::new();
let mut sd: VecStack<f64> = VecStack::new();
// si.push(3.14);  // error de compilación: expected i32, found f64
```

---

## 7. Comparación de seguridad: C vs Rust

### Errores que C permite y Rust previene

| Error | C | Rust |
|-------|---|------|
| Memory leak | Posible (olvidar destroy) | Imposible con Box/Vec |
| Use-after-free | Posible (usar después de destroy) | Imposible (ownership) |
| Double free | Posible (destroy dos veces) | Imposible (move semantics) |
| Buffer overflow | Posible (push sin verificar) | Imposible (Vec crece) |
| Null dereference | Posible (pop sin verificar) | Imposible (Option force check) |
| Type confusion | Posible (void * cast incorrecto) | Imposible (generics) |
| Uninitialized read | Posible (pop escribe basura) | Imposible (Option) |

### El costo de la seguridad

La seguridad de Rust no es gratis en términos de **ergonomía del programador**:

```rust
// En C, mover un puntero es trivial:
node->next = s->top;
s->top = node;

// En Rust, mover ownership requiere take():
let old_head = self.head.take();
let node = Box::new(Node { data, next: old_head });
self.head = Some(node);
```

Pero es gratis en términos de **rendimiento runtime**: las verificaciones
de ownership son en compile time, no generan código extra.

---

## 8. Iteradores

Una ventaja significativa de Rust sobre C es la integración con iteradores:

### Iterador por referencia

```rust
pub struct Iter<'a, T> {
    current: Option<&'a Node<T>>,
}

impl<T> ListStack<T> {
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

`as_deref()` convierte `Option<Box<Node<T>>>` → `Option<&Node<T>>`,
desreferenciando el `Box` dentro del `Option`.

### Uso

```rust
let mut s = ListStack::new();
s.push(10);
s.push(20);
s.push(30);

// Iterar sin consumir el stack (top a bottom)
for val in s.iter() {
    print!("{val} ");
}
// 30 20 10

// El stack sigue intacto
assert_eq!(s.len(), 3);
```

### Iterador que consume (IntoIterator)

```rust
impl<T> IntoIterator for ListStack<T> {
    type Item = T;
    type IntoIter = IntoIter<T>;

    fn into_iter(mut self) -> IntoIter<T> {
        IntoIter { stack: self }
    }
}

pub struct IntoIter<T> {
    stack: ListStack<T>,
}

impl<T> Iterator for IntoIter<T> {
    type Item = T;

    fn next(&mut self) -> Option<T> {
        self.stack.pop()
    }
}
```

```rust
let mut s = ListStack::new();
s.push(10);
s.push(20);
s.push(30);

// Consume el stack
let collected: Vec<i32> = s.into_iter().collect();
assert_eq!(collected, vec![30, 20, 10]);
// s ya no existe (fue consumido)
```

### Comparación con C

En C, iterar un stack requiere romper la abstracción (acceder a los campos
internos) o implementar una función de recorrido ad-hoc. En Rust, los
iteradores son un patrón estándar que se integra con `for`, `collect`,
`map`, `filter`, etc.

---

## 9. Display y Debug

### Implementar Display

```rust
use std::fmt;

impl<T: fmt::Display> fmt::Display for ListStack<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[")?;
        let mut current = self.head.as_deref();
        let mut first = true;
        while let Some(node) = current {
            if !first { write!(f, ", ")?; }
            write!(f, "{}", node.data)?;
            first = false;
            current = node.next.as_deref();
        }
        write!(f, "]")
    }
}

impl<T: fmt::Display> fmt::Display for VecStack<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[")?;
        for (i, item) in self.data.iter().enumerate() {
            if i > 0 { write!(f, ", ")?; }
            write!(f, "{item}")?;
        }
        write!(f, "]")
    }
}
```

```rust
let mut s = ListStack::new();
s.push(10);
s.push(20);
s.push(30);
println!("{s}");   // [30, 20, 10]
```

Nota: `ListStack` imprime de top a bottom (orden de recorrido natural de la
lista). `VecStack` imprime de bottom a top (orden del array). Para consistencia,
se puede invertir el de `VecStack` usando `.iter().rev()`.

### Implementar Debug

```rust
#[derive(Debug)]
struct Node<T: fmt::Debug> {
    data: T,
    next: Option<Box<Node<T>>>,
}

// O manualmente para VecStack:
impl<T: fmt::Debug> fmt::Debug for VecStack<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("VecStack")
            .field("data", &self.data)
            .field("len", &self.data.len())
            .finish()
    }
}
```

---

## 10. Resumen comparativo completo

| Aspecto | C array | C lista | Rust Vec | Rust lista |
|---------|---------|---------|----------|-----------|
| Push | $O(1)$ o realloc | $O(1)$ + malloc | $O(1)$ amortizado | $O(1)$ + Box::new |
| Pop | $O(1)$ | $O(1)$ + free | $O(1)$ | $O(1)$ |
| Memoria/elem | `sizeof(T)` | `sizeof(T)` + ptr | `sizeof(T)` | `sizeof(T)` + ptr + Box header |
| Cache | Excelente | Mala | Excelente | Mala |
| Genéricos | `void *` (unsafe) | `void *` (unsafe) | `<T>` (safe) | `<T>` (safe) |
| Memory safety | Manual | Manual | Automática | Automática |
| Destroy | Manual | Manual (recorrer) | Automático | Automático |
| Iteradores | Ad-hoc | Ad-hoc | `Iterator` trait | `Iterator` trait |
| Null check | Manual | Manual | `Option` | `Option` |
| Lines of code | ~60 | ~70 | ~25 | ~45 |

### Recomendación

```
Para producción:     VecStack<T>  (o directamente Vec<T>)
Para aprendizaje:    ListStack<T> (para entender ownership y Box)
Para rendimiento:    VecStack<T>  (cache locality)
Para O(1) estricto:  ListStack<T> (sin reallocs)
```

---

## Ejercicios

### Ejercicio 1 — VecStack completo

Implementa `VecStack<T>` con todos los métodos. Prueba con `i32`, `String`,
y un struct personalizado:

```rust
struct Point { x: f64, y: f64 }

fn main() {
    let mut si = VecStack::<i32>::new();
    let mut ss = VecStack::<String>::new();
    let mut sp = VecStack::<Point>::new();

    // Push, peek, pop para cada uno
    // Verificar que funciona con los tres tipos
}
```

**Prediccion**: Necesitas agregar algún trait bound a `T` para que funcione
con los tres tipos?

<details><summary>Respuesta</summary>

```rust
pub struct VecStack<T> {
    data: Vec<T>,
}

impl<T> VecStack<T> {
    pub fn new() -> Self { VecStack { data: Vec::new() } }
    pub fn push(&mut self, elem: T) { self.data.push(elem); }
    pub fn pop(&mut self) -> Option<T> { self.data.pop() }
    pub fn peek(&self) -> Option<&T> { self.data.last() }
    pub fn is_empty(&self) -> bool { self.data.is_empty() }
    pub fn len(&self) -> usize { self.data.len() }
}

#[derive(Debug)]
struct Point { x: f64, y: f64 }

fn main() {
    // i32
    let mut si = VecStack::<i32>::new();
    si.push(1);
    si.push(2);
    assert_eq!(si.pop(), Some(2));
    assert_eq!(si.peek(), Some(&1));

    // String
    let mut ss = VecStack::<String>::new();
    ss.push(String::from("hello"));
    ss.push(String::from("world"));
    assert_eq!(ss.pop(), Some(String::from("world")));

    // Point
    let mut sp = VecStack::<Point>::new();
    sp.push(Point { x: 1.0, y: 2.0 });
    sp.push(Point { x: 3.0, y: 4.0 });
    let top = sp.peek().unwrap();
    assert_eq!(top.x, 3.0);
    assert_eq!(sp.len(), 2);

    println!("All types work!");
}
```

**No** necesitas agregar trait bounds a `T`. `VecStack<T>` funciona con
cualquier tipo sin restricciones porque:
- `push` solo mueve `T` al Vec (no necesita Clone, Copy, ni Display)
- `pop` mueve `T` fuera del Vec
- `peek` retorna `&T` (referencia, no copia)

Solo si agregas funcionalidad que requiera traits específicos (como
`Display` para imprimir, o `PartialEq` para comparar) necesitarías bounds.
La estructura base es completamente genérica.

</details>

---

### Ejercicio 2 — ListStack completo

Implementa `ListStack<T>` con `push`, `pop`, `peek`, `is_empty`, `len`, y
`Drop` iterativo. Prueba con el mismo test del ejercicio 1:

```rust
fn main() {
    let mut s = ListStack::new();
    for i in 0..10 {
        s.push(i);
    }
    assert_eq!(s.len(), 10);
    assert_eq!(s.peek(), Some(&9));

    while let Some(val) = s.pop() {
        print!("{val} ");
    }
    println!();
    // 9 8 7 6 5 4 3 2 1 0
}
```

**Prediccion**: Si no implementas `Drop`, la destrucción automática recurre.
Para cuántos elementos es eso un problema?

<details><summary>Respuesta</summary>

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}

pub struct ListStack<T> {
    head: Option<Box<Node<T>>>,
    len: usize,
}

impl<T> ListStack<T> {
    pub fn new() -> Self {
        ListStack { head: None, len: 0 }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::new(Node {
            data,
            next: self.head.take(),
        });
        self.head = Some(node);
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        let node = self.head.take()?;
        self.head = node.next;
        self.len -= 1;
        Some(node.data)
    }

    pub fn peek(&self) -> Option<&T> {
        self.head.as_ref().map(|node| &node.data)
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}

impl<T> Drop for ListStack<T> {
    fn drop(&mut self) {
        let mut current = self.head.take();
        while let Some(mut node) = current {
            current = node.next.take();
        }
    }
}

fn main() {
    let mut s = ListStack::new();
    for i in 0..10 {
        s.push(i);
    }
    assert_eq!(s.len(), 10);
    assert_eq!(s.peek(), Some(&9));

    while let Some(val) = s.pop() {
        print!("{val} ");
    }
    println!();

    // Test Drop con lista grande
    let mut big = ListStack::new();
    for i in 0..1_000_000 {
        big.push(i);
    }
    // big sale del scope → Drop iterativo maneja 10⁶ nodos sin problema
    println!("All tests passed!");
}
```

Sin `Drop` personalizado, la destrucción recursiva causa stack overflow
alrededor de $10^4 - 10^5$ elementos (depende del tamaño del stack del
programa, típicamente 8 MB, y del tamaño de cada frame recursivo).

Con el `Drop` iterativo, no hay límite práctico — $10^6$, $10^7$ elementos
funcionan sin problema porque el loop usa $O(1)$ espacio extra en el call
stack.

</details>

---

### Ejercicio 3 — take() paso a paso

Traza manualmente la ejecución de `push(10)`, `push(20)`, `pop()` mostrando
el estado de `self.head` después de cada `take()`:

```
Inicio:  head = None

push(10):
  1. self.head.take() = ???, self.head = ???
  2. Box::new(Node { data: 10, next: ??? })
  3. self.head = ???
```

**Prediccion**: Después de `push(10).take()`, head es `None` momentáneamente?

<details><summary>Respuesta</summary>

```
═══ Inicio ═══
  head = None

═══ push(10) ═══
  1. old_head = self.head.take()
     → old_head = None
     → self.head = None              ← sí, momentáneamente None

  2. node = Box::new(Node { data: 10, next: None })
     → [Node(10, None)] en el heap

  3. self.head = Some(node)
     → head = Some(Box → Node(10, None))

═══ push(20) ═══
  1. old_head = self.head.take()
     → old_head = Some(Box → Node(10, None))
     → self.head = None              ← momentáneamente None otra vez

  2. node = Box::new(Node { data: 20, next: old_head })
     → [Node(20, Some(Box → Node(10, None)))] en el heap

  3. self.head = Some(node)
     → head = Some(Box → Node(20, → Node(10, None)))

═══ pop() ═══
  1. node = self.head.take()?
     → node = Box → Node(20, Some(Box → Node(10, None)))
     → self.head = None              ← momentáneamente None

  2. self.head = node.next
     → self.head = Some(Box → Node(10, None))

  3. return Some(node.data)
     → return Some(20)
     → node (el Box del nodo 20) se destruye, liberando la memoria

═══ Estado final ═══
  head = Some(Box → Node(10, None))
  len = 1
```

Sí, `head` es `None` momentáneamente durante cada `take()`. Esto es seguro
porque nadie más puede acceder al stack durante la ejecución de `push`/`pop`
(la `&mut self` garantiza acceso exclusivo). Si el código panics entre el
`take()` y la reasignación, el destructor verá `head = None` y liberará
correctamente (una lista vacía).

</details>

---

### Ejercicio 4 — Iterador para VecStack

Implementa un iterador por referencia para `VecStack<T>` que itere de top
a bottom:

```rust
impl<T> VecStack<T> {
    pub fn iter(&self) -> impl Iterator<Item = &T> {
        // Tu código aquí
    }
}
```

Pista: `Vec` ya tiene `.iter()` que va de inicio a fin. Necesitas invertir.

**Prediccion**: Puedes usar `.iter().rev()` directamente?

<details><summary>Respuesta</summary>

```rust
impl<T> VecStack<T> {
    pub fn iter(&self) -> impl Iterator<Item = &T> {
        self.data.iter().rev()
    }
}

fn main() {
    let mut s = VecStack::new();
    s.push(10);
    s.push(20);
    s.push(30);

    // Top a bottom: 30, 20, 10
    for val in s.iter() {
        print!("{val} ");
    }
    println!();

    // También funciona con collect, map, filter, etc.
    let sum: i32 = s.iter().sum();
    assert_eq!(sum, 60);

    let doubled: Vec<i32> = s.iter().map(|x| x * 2).collect();
    assert_eq!(doubled, vec![60, 40, 20]);
}
```

Sí, `.iter().rev()` funciona directamente porque `Vec::iter()` implementa
`DoubleEndedIterator`, lo que habilita `.rev()`.

El resultado: el iterador del `VecStack` recorre de top (último elemento del
Vec) a bottom (primer elemento), que es el orden natural para un stack.

Comparación con `ListStack`: la lista solo puede recorrerse en una dirección
(top → bottom), así que su iterador no puede hacer `.rev()` sin construir
una estructura auxiliar. El `VecStack` es bidireccional gratuitamente.

</details>

---

### Ejercicio 5 — Probar con tipos no-Copy

Prueba que `ListStack` funciona con `String` (un tipo que no implementa
`Copy`). Verifica que el ownership se transfiere correctamente:

```rust
fn main() {
    let mut s = ListStack::new();
    let greeting = String::from("hello");
    s.push(greeting);
    // println!("{greeting}");  // ← ¿compila?

    let popped = s.pop().unwrap();
    println!("{popped}");       // ← ¿funciona?
}
```

**Prediccion**: La línea comentada compila? Por qué o por qué no?

<details><summary>Respuesta</summary>

```rust
fn main() {
    let mut s = ListStack::new();
    let greeting = String::from("hello");
    s.push(greeting);
    // println!("{greeting}");  // ERROR: value used after move

    let popped = s.pop().unwrap();
    println!("{popped}");       // OK: "hello"

    // Verificar que pop realmente mueve el String
    let mut s2 = ListStack::new();
    s2.push(String::from("world"));
    s2.push(String::from("foo"));

    let a = s2.pop().unwrap();   // a = "foo"
    let b = s2.pop().unwrap();   // b = "world"
    println!("{a}, {b}");        // "foo, world"

    // a y b son owners independientes de sus Strings
    // Cuando salen del scope, se liberan automáticamente
}
```

No, la línea comentada **no compila**. `s.push(greeting)` mueve `greeting`
al stack — `greeting` ya no es válido después del push. El compilador lo
detecta:

```
error[E0382]: borrow of moved value: `greeting`
  --> src/main.rs:5:16
   |
3  |     let greeting = String::from("hello");
   |         -------- move occurs because `greeting` has type `String`
4  |     s.push(greeting);
   |            -------- value moved here
5  |     println!("{greeting}");
   |                ^^^^^^^^ value borrowed here after move
```

Esto es ownership en acción:
1. `push(greeting)` transfiere el ownership del `String` al stack
2. Dentro del stack, el `String` vive en un `Node` en el heap
3. `pop()` mueve el `String` fuera del `Node` y lo retorna al llamador
4. El `Node` se destruye (sin intentar liberar el `String`, porque fue movido)

Si `String` fuera `Copy` (no lo es), `push` haría una copia y `greeting`
seguiría siendo válido. Pero `String` tiene un buffer en el heap que no
se puede copiar implícitamente.

</details>

---

### Ejercicio 6 — Test genérico con trait

Escribe una función de test que sea genérica sobre el trait `StackTrait`.
Debe verificar los 7 axiomas del TAD (de T01, sección 3):

```rust
fn verify_axioms<S: StackTrait<i32>>() {
    // Axioma 1: pop(push(s, x)) = (s, x)
    // ...
    // Axioma 7: size(pop(s)) = size(s) - 1
}

fn main() {
    verify_axioms::<VecStack<i32>>();
    verify_axioms::<ListStack<i32>>();
    println!("All axioms verified for both implementations!");
}
```

**Prediccion**: Si una implementación tiene un bug en `len()`, cuántos axiomas
fallan?

<details><summary>Respuesta</summary>

```rust
fn verify_axioms<S: StackTrait<i32>>() {
    // Axioma 1: pop(push(s, x)) = (s, x)
    let mut s = S::new();
    s.push(42);
    assert_eq!(s.pop(), Some(42));
    assert!(s.is_empty());

    // Axioma 1 — con stack no vacío
    let mut s = S::new();
    s.push(1);
    s.push(2);
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert!(s.is_empty());

    // Axioma 2: peek(push(s, x)) = x
    let mut s = S::new();
    s.push(99);
    assert_eq!(s.peek(), Some(&99));
    s.push(100);
    assert_eq!(s.peek(), Some(&100));

    // Axioma 3: is_empty(new()) = true
    let s = S::new();
    assert!(s.is_empty());

    // Axioma 4: is_empty(push(s, x)) = false
    let mut s = S::new();
    s.push(1);
    assert!(!s.is_empty());

    // Axioma 5: size(new()) = 0
    let s = S::new();
    assert_eq!(s.len(), 0);

    // Axioma 6: size(push(s, x)) = size(s) + 1
    let mut s = S::new();
    for i in 0..10 {
        let old_len = s.len();
        s.push(i);
        assert_eq!(s.len(), old_len + 1);
    }

    // Axioma 7: size(pop(s)) = size(s) - 1
    let mut s = S::new();
    for i in 0..5 { s.push(i); }
    for _ in 0..5 {
        let old_len = s.len();
        s.pop();
        assert_eq!(s.len(), old_len - 1);
    }
}

fn main() {
    verify_axioms::<VecStack<i32>>();
    verify_axioms::<ListStack<i32>>();
    println!("All axioms verified for both implementations!");
}
```

Si `len()` tiene un bug, fallan los axiomas **5, 6, y 7** directamente
(todos dependen de `len`). El axioma **3** también podría fallar si
`is_empty` se implementa como `self.len() == 0` en vez de revisar `head`
directamente.

Axiomas 1, 2, y 4 no dependen de `len`, por lo que no se verían afectados
(asumiendo que `is_empty` no usa `len`).

</details>

---

### Ejercicio 7 — Benchmark Vec vs Lista

Mide el tiempo de push $n$ + pop $n$ para ambas implementaciones con
$n = 10^6$:

```rust
use std::time::Instant;

fn bench_vec(n: usize) -> std::time::Duration {
    let start = Instant::now();
    let mut s = VecStack::new();
    for i in 0..n { s.push(i); }
    while s.pop().is_some() {}
    start.elapsed()
}

fn bench_list(n: usize) -> std::time::Duration {
    // similar
}
```

Compila con `cargo run --release`.

**Prediccion**: El ratio será similar al que vimos en C (~5-10×)?

<details><summary>Respuesta</summary>

```rust
use std::hint::black_box;
use std::time::Instant;

fn bench_vec(n: usize) -> std::time::Duration {
    let start = Instant::now();
    let mut s = VecStack::new();
    for i in 0..n { s.push(black_box(i)); }
    while s.pop().is_some() {}
    start.elapsed()
}

fn bench_list(n: usize) -> std::time::Duration {
    let start = Instant::now();
    let mut s = ListStack::new();
    for i in 0..n { s.push(black_box(i)); }
    while s.pop().is_some() {}
    start.elapsed()
}

fn main() {
    let n = 1_000_000;
    let tv = bench_vec(n);
    let tl = bench_list(n);
    println!("Vec:  {tv:?}");
    println!("List: {tl:?}");
    println!("Ratio: {:.1}x", tl.as_nanos() as f64 / tv.as_nanos() as f64);
}
```

```bash
cargo run --release
```

Resultados típicos:

```
Vec:  5.2ms
List: 42.1ms
Ratio: 8.1x
```

Sí, el ratio es **similar al de C** (~5-10×). Las razones son las mismas:
- `Vec` no hace allocation individual por push
- `Vec` tiene mejor cache locality
- `Box::new` / drop en cada push/pop de la lista tiene overhead del allocator

La conclusión se mantiene: para un stack en producción, usa `Vec<T>`.

</details>

---

### Ejercicio 8 — Invertir con stack

Implementa una función genérica que invierta un `Vec<T>` usando un stack:

```rust
fn reverse_with_stack<T>(input: Vec<T>) -> Vec<T> {
    // Tu código aquí
}

fn main() {
    assert_eq!(reverse_with_stack(vec![1, 2, 3]), vec![3, 2, 1]);
    assert_eq!(reverse_with_stack(vec!["a", "b", "c"]),
               vec!["c", "b", "a"]);
    assert_eq!(reverse_with_stack::<i32>(vec![]), vec![]);
}
```

**Prediccion**: Funciona con `T` que no es `Clone`? Por ejemplo, `Vec<String>`?

<details><summary>Respuesta</summary>

```rust
fn reverse_with_stack<T>(input: Vec<T>) -> Vec<T> {
    let mut stack = VecStack::new();
    for item in input {
        stack.push(item);      // mueve cada elemento al stack
    }
    let mut result = Vec::new();
    while let Some(item) = stack.pop() {
        result.push(item);     // mueve cada elemento al resultado
    }
    result
}

fn main() {
    assert_eq!(reverse_with_stack(vec![1, 2, 3]), vec![3, 2, 1]);
    assert_eq!(reverse_with_stack(vec!["a", "b", "c"]),
               vec!["c", "b", "a"]);
    assert_eq!(reverse_with_stack::<i32>(vec![]), vec![]);

    // Funciona con String (no es Clone-dependiente)
    let strings = vec![
        String::from("hello"),
        String::from("world"),
    ];
    let reversed = reverse_with_stack(strings);
    assert_eq!(reversed, vec!["world", "hello"]);

    println!("All reverse tests passed!");
}
```

Sí, funciona con `T` que **no es `Clone`**. La función usa **move semantics**:
`for item in input` consume el Vec de entrada y mueve cada elemento al stack.
`stack.pop()` mueve cada elemento fuera del stack al resultado.

Nunca se copia ni se clona nada — los valores se mueven de `input` → `stack`
→ `result`. Por eso no necesita `T: Clone`.

En la práctica, `input.into_iter().rev().collect()` hace lo mismo sin stack
explícito. Pero el ejercicio demuestra cómo el stack invierte naturalmente
el orden por su propiedad LIFO.

</details>

---

### Ejercicio 9 — Stack con peek_mut

Agrega un método `peek_mut` que retorne `Option<&mut T>` (referencia mutable
al tope). Esto permite modificar el tope sin pop + push:

```rust
impl<T> ListStack<T> {
    pub fn peek_mut(&mut self) -> Option<&mut T> {
        // Tu código aquí
    }
}
```

Pruébalo:

```rust
let mut s = ListStack::new();
s.push(10);
if let Some(top) = s.peek_mut() {
    *top += 5;
}
assert_eq!(s.peek(), Some(&15));
```

**Prediccion**: Puedes llamar `peek_mut` y `peek` al mismo tiempo?

<details><summary>Respuesta</summary>

```rust
impl<T> ListStack<T> {
    pub fn peek_mut(&mut self) -> Option<&mut T> {
        self.head.as_mut().map(|node| &mut node.data)
    }
}

impl<T> VecStack<T> {
    pub fn peek_mut(&mut self) -> Option<&mut T> {
        self.data.last_mut()
    }
}

fn main() {
    let mut s = ListStack::new();
    s.push(10);
    s.push(20);

    // Modificar el tope
    if let Some(top) = s.peek_mut() {
        *top += 5;
    }
    assert_eq!(s.peek(), Some(&25));   // 20 + 5

    // El segundo elemento no cambió
    s.pop();
    assert_eq!(s.peek(), Some(&10));

    println!("peek_mut works!");
}
```

No, **no puedes** llamar `peek_mut` y `peek` simultáneamente:

```rust
let r = s.peek_mut();   // &mut borrow de s
let p = s.peek();       // & borrow de s — ERROR: ya hay &mut
```

```
error[E0502]: cannot borrow `s` as immutable because it is also
              borrowed as mutable
```

Esto es el borrow checker protegiendo contra aliasing mutable: si tienes
una referencia mutable al tope, nadie más puede leer el stack al mismo
tiempo (podrían ver un estado inconsistente).

La solución: usar el resultado de `peek_mut` antes de llamar a `peek`:

```rust
// Correcto: usa peek_mut, luego peek
if let Some(top) = s.peek_mut() {
    *top += 5;
}
// peek_mut ya no está activo aquí
let val = s.peek();   // OK
```

</details>

---

### Ejercicio 10 — Port del test de C

Toma el test exhaustivo del ejercicio 10 de T02 (C array) y portéalo a Rust.
Verifica ambas implementaciones:

```rust
fn exhaustive_test<S: StackTrait<i32>>() {
    let mut s = S::new();

    // Push 1000 elementos (0..999)
    // Size = 1000
    // Peek = 999
    // Pop 1000 en orden inverso
    // Vacío
    // Pop en vacío = None
}
```

Verifica con `cargo miri test`.

**Prediccion**: El test en Rust es más corto que en C? Por cuántas líneas
aproximadamente?

<details><summary>Respuesta</summary>

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn exhaustive_test<S: StackTrait<i32>>() {
        let mut s = S::new();

        // 1. Push 1000 elementos
        for i in 0..1000 {
            s.push(i);
        }

        // 2. Size = 1000
        assert_eq!(s.len(), 1000);

        // 3. Peek = 999
        assert_eq!(s.peek(), Some(&999));

        // 4. Pop en orden inverso
        for i in (0..1000).rev() {
            assert_eq!(s.pop(), Some(i));
        }

        // 5. Vacío
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);

        // 6. Pop en vacío = None
        assert_eq!(s.pop(), None);
        assert_eq!(s.peek(), None);
    }

    #[test]
    fn test_vec_stack() {
        exhaustive_test::<VecStack<i32>>();
    }

    #[test]
    fn test_list_stack() {
        exhaustive_test::<ListStack<i32>>();
    }
}
```

```bash
cargo test
cargo miri test
```

El test en Rust es **~15 líneas** vs **~30 líneas** en C. La diferencia
viene de:

| Aspecto | C | Rust |
|---------|---|------|
| Variable out para pop | `int val; stack_pop(s, &val);` | `s.pop()` directo |
| Verificación de error | `assert(stack_pop(s, &val)); assert(val == i);` | `assert_eq!(s.pop(), Some(i));` |
| Verificación de vacío | `assert(!stack_pop(s, &val));` | `assert_eq!(s.pop(), None);` |
| Genérico sobre impl | No posible directamente | `fn exhaustive_test<S: StackTrait>` |
| Cleanup | `stack_destroy(s);` | Automático |

`Option<T>` elimina la necesidad de variables intermedias y checks separados.
Los genéricos permiten reusar el mismo test para ambas implementaciones. Y
`Drop` elimina la necesidad de cleanup.

`cargo miri test` verifica que no hay UB, leaks, ni violaciones de aliasing
en la versión con lista (que usa `Box` internamente). Para la versión Vec,
Miri verifica que Vec internamente no tiene UB.

</details>
