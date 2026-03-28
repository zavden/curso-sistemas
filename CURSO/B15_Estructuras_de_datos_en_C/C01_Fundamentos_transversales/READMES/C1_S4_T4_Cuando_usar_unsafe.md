# T04 — Cuándo usar unsafe

---

## 1. La pregunta central

Rust ofrece múltiples formas de implementar estructuras de datos con punteros.
La pregunta no es "¿puedo usar unsafe?" — siempre puedes. La pregunta es
**"¿necesito unsafe?"**, y si la respuesta es sí, **"¿cuánto?"**.

```
Más safe                                              Más control
   │                                                      │
   ▼                                                      ▼
Option<Box<Node<T>>>  →  Rc<RefCell<T>>  →  *mut T / NonNull<T>

  Sin unsafe              Sin unsafe           Requiere unsafe
  Un solo owner           Múltiples owners     Ownership manual
  Sin ciclos              Sin ciclos (leak)    Permite ciclos
  Sin overhead runtime    Overhead runtime     Sin overhead runtime
```

La meta es usar la opción **más a la izquierda** que resuelva tu problema.
Solo moverte a la derecha cuando la opción safe no puede expresar la
estructura que necesitas.

---

## 2. Opción 1: Option\<Box\<Node\<T\>\>\> — completamente safe

### Cómo funciona

`Box<T>` es un puntero a heap con ownership único. `Option<Box<T>>` agrega
la posibilidad de "no hay nodo" (null). El compilador gestiona todo:

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}

pub struct Stack<T> {
    head: Option<Box<Node<T>>>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { head: None, len: 0 }
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
}

// Drop automático: Rust recorre next recursivamente y libera todo
```

### Qué puede expresar

| Estructura | Viable con Box? |
|-----------|----------------|
| Stack (lista simple) | Sí |
| Cola (lista simple con tail) | Parcial — tail no puede ser `&mut` al último nodo |
| Lista simplemente enlazada | Sí (con limitaciones para acceso al final) |
| Lista doblemente enlazada | **No** — prev crearía dos owners del mismo nodo |
| Árbol binario sin padre | Sí |
| Árbol binario con puntero a padre | **No** — ciclo padre ↔ hijo |
| Grafo | **No** — múltiples aristas al mismo nodo |

### Ventajas

- **Cero `unsafe`** — el compilador verifica todo
- **Drop automático** — sin leaks posibles
- **Sin overhead runtime** — `Box` es un puntero directo al heap
- **Optimización de nicho** — `Option<Box<T>>` = tamaño de un puntero

### Limitaciones

La raíz del problema es que `Box<T>` impone **ownership único**. Un nodo
solo puede tener un owner. Esto prohíbe:

```
Lista doblemente enlazada:
  A.next = B   (A es owner de B)
  B.prev = A   (B sería owner de A — pero A ya tiene owner: la lista)
  → Dos owners de A → imposible con Box

Árbol con padre:
  parent.left = child   (parent es owner de child)
  child.parent = parent (child sería owner de parent)
  → Ciclo de ownership → imposible con Box
```

---

## 3. Opción 2: Rc\<RefCell\<T\>\> — múltiples owners, sin unsafe

### Cómo funciona

`Rc<T>` es un puntero con conteo de referencias — múltiples `Rc` pueden
apuntar al mismo dato. `RefCell<T>` permite mutabilidad interior con
verificación en runtime:

```rust
use std::cell::RefCell;
use std::rc::Rc;

type Link<T> = Option<Rc<RefCell<Node<T>>>>;

struct Node<T> {
    data: T,
    next: Link<T>,
    prev: Link<T>,   // Weak para evitar ciclos de Rc
}
```

### Versión correcta con Weak

`Rc` crea ciclos de conteo si dos nodos se apuntan mutuamente (A → B → A):
ninguno llega a count = 0, ninguno se libera → **leak**. La solución es
usar `Weak<T>` para una de las direcciones:

```rust
use std::cell::RefCell;
use std::rc::{Rc, Weak};

type StrongLink<T> = Option<Rc<RefCell<DNode<T>>>>;
type WeakLink<T> = Option<Weak<RefCell<DNode<T>>>>;

struct DNode<T> {
    data: T,
    next: StrongLink<T>,    // owner del siguiente
    prev: WeakLink<T>,      // referencia débil al anterior (no impide drop)
}

pub struct DoublyLinked<T> {
    head: StrongLink<T>,
    tail: WeakLink<T>,
    len: usize,
}
```

### Implementación de push/pop

```rust
impl<T> DoublyLinked<T> {
    pub fn new() -> Self {
        DoublyLinked { head: None, tail: None, len: 0 }
    }

    pub fn push_back(&mut self, data: T) {
        let new_node = Rc::new(RefCell::new(DNode {
            data,
            next: None,
            prev: self.tail.clone(),   // weak ref al tail actual
        }));

        match self.tail.take() {
            None => {
                // Lista vacía
                self.head = Some(Rc::clone(&new_node));
            }
            Some(old_tail_weak) => {
                if let Some(old_tail) = old_tail_weak.upgrade() {
                    old_tail.borrow_mut().next = Some(Rc::clone(&new_node));
                }
            }
        }

        self.tail = Some(Rc::downgrade(&new_node));
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        self.head.take().map(|old_head| {
            match old_head.borrow().next.clone() {
                None => {
                    self.tail = None;
                }
                Some(new_head) => {
                    new_head.borrow_mut().prev = None;
                    self.head = Some(new_head);
                }
            }
            self.len -= 1;
            Rc::try_unwrap(old_head)
                .ok()
                .expect("node had multiple strong refs")
                .into_inner()
                .data
        })
    }
}
```

### El overhead de Rc\<RefCell\<T\>\>

Cada nodo paga costos adicionales:

```
Box<Node<T>>:
  Layout: [data | next]
  Overhead: 0 bytes extra

Rc<RefCell<Node<T>>>:
  Layout: [strong_count | weak_count | borrow_flag | data | next | prev]
  Overhead por nodo:
    strong_count: 8 bytes (usize)
    weak_count:   8 bytes (usize)
    borrow_flag:  8 bytes (isize — RefCell)
  Total extra:    24 bytes por nodo
```

Para $n = 10^6$ nodos: $24 \times 10^6 = 24$ MB de overhead solo en metadatos.

Además hay overhead de **runtime**:

```
Box:                    desreferencia directa (1 instrucción)
Rc<RefCell<T>>:         borrow_mut() verifica en runtime (branch + increment)
                        Rc::clone incrementa strong_count (atomic en algunos casos)
```

### Qué puede expresar

| Estructura | Viable con Rc\<RefCell\>? |
|-----------|--------------------------|
| Stack / cola / lista simple | Sí (pero overkill) |
| Lista doblemente enlazada | Sí (con Weak para prev) |
| Árbol con puntero a padre | Sí (Weak para parent) |
| Grafo general | Parcial — necesitas Weak para evitar ciclos |
| Grafo con ciclos arbitrarios | Difícil — propenso a leaks si no se rompen los ciclos |

### Ventajas

- **Sin `unsafe`** — todo verificado por el compilador + runtime
- **Múltiples owners** — resuelve el problema de doble enlace

### Desventajas

- **24 bytes extra por nodo** (mínimo)
- **Overhead runtime** en cada acceso (`borrow()`/`borrow_mut()`)
- **Panic en runtime** si se viola borrowing (doble `borrow_mut()`)
- **Ergonomía**: `Rc::clone(&node).borrow_mut().next` es verboso
- **Leaks posibles** si se crean ciclos de `Rc` sin `Weak`
- **No es `Send`/`Sync`** — no funciona en contextos multi-thread

---

## 4. Opción 3: Raw pointers — unsafe, máximo control

### Cómo funciona

`*mut T` o `NonNull<T>` — punteros sin restricciones de ownership:

```rust
use std::ptr::NonNull;

struct DNode<T> {
    data: T,
    prev: Option<NonNull<DNode<T>>>,
    next: Option<NonNull<DNode<T>>>,
}

pub struct DoublyLinked<T> {
    head: Option<NonNull<DNode<T>>>,
    tail: Option<NonNull<DNode<T>>>,
    len: usize,
}
```

### El overhead

```
NonNull<DNode<T>>:
  Layout: [data | prev | next]
  Overhead: 0 bytes extra (más allá de los punteros propios de la estructura)

Option<NonNull<T>>: tamaño = 1 puntero (optimización de nicho)
```

El nodo contiene exactamente lo que necesita — datos y punteros. Sin contadores,
sin flags de borrow, sin metadatos.

### Qué puede expresar

Todo. Raw pointers no tienen restricciones de ownership:

| Estructura | Viable? |
|-----------|---------|
| Stack / cola / lista simple | Sí |
| Lista doblemente enlazada | Sí |
| Árbol con puntero a padre | Sí |
| Grafo general | Sí |
| Grafo con ciclos | Sí |
| Arena allocator | Sí |
| Estructuras intrusivas | Sí |

### Ventajas

- **Cero overhead** de memoria y runtime
- **Puede expresar cualquier topología** de punteros
- **Rendimiento idéntico a C**
- **`Send`/`Sync`** implementable manualmente

### Desventajas

- **Requiere `unsafe`** — responsabilidad total del programador
- **Sin Drop automático** — hay que liberar manualmente o implementar `Drop`
- **Bugs difíciles**: use-after-free, double free, leaks, dangling pointers
- **Sin verificación en compile time** para la corrección de punteros

---

## 5. Árbol de decisión

```
¿Tu estructura tiene un solo owner por nodo?
│
├─ SÍ → ¿Necesitas acceso O(1) al final de una lista?
│       │
│       ├─ NO → Option<Box<Node<T>>>
│       │       ✓ Stack, lista simple, árbol binario
│       │
│       └─ SÍ → ¿Es aceptable el overhead de Rc<RefCell>?
│               │
│               ├─ SÍ → Rc<RefCell<Node<T>>> con tail
│               │
│               └─ NO → Raw pointers (unsafe)
│
└─ NO → ¿Hay múltiples owners del mismo nodo?
        │
        ├─ Solo prev/next (lista doble) o parent (árbol):
        │   │
        │   ├─ ¿Overhead de Rc/Weak aceptable?
        │   │   │
        │   │   ├─ SÍ → Rc<RefCell> + Weak
        │   │   │
        │   │   └─ NO → Raw pointers (unsafe)
        │   │
        │   └─ ¿Es multi-thread?
        │       │
        │       ├─ SÍ → Arc<Mutex> o raw pointers + unsafe Send/Sync
        │       │
        │       └─ NO → Rc<RefCell> o raw pointers
        │
        └─ Grafo general / ciclos arbitrarios:
            │
            ├─ Pocos nodos, no es hot path → Rc<RefCell> + cuidado con leaks
            │
            └─ Rendimiento crítico → Raw pointers o arena allocation
```

### Resumen compacto

| Estructura | Recomendación | Razón |
|-----------|--------------|-------|
| Stack | `Option<Box<Node>>` | Un owner, sin ciclos |
| Cola FIFO | Raw pointers | Tail necesita acceso sin ownership |
| Lista simple (solo forward) | `Option<Box<Node>>` | Un owner, sin ciclos |
| Lista doble | Raw pointers | Ciclo prev/next |
| Árbol binario (sin padre) | `Option<Box<Node>>` | Cada hijo tiene un owner |
| Árbol binario (con padre) | Raw pointers | Ciclo padre/hijo |
| BST/AVL/Red-Black | Raw pointers | Parent pointer necesario para rotaciones |
| Heap binario | `Vec<T>` | Array-based, sin punteros |
| Hash table | `Vec<Bucket>` | Array-based |
| Grafo | Raw pointers o `Vec` + índices | Topología arbitraria |

### La alternativa que no es punteros: index-based

Para grafos y estructuras con ciclos, hay una cuarta opción que evita unsafe:
usar un `Vec` como arena y referenciar nodos por índice:

```rust
struct Node<T> {
    data: T,
    next: Option<usize>,   // índice en el Vec, no puntero
    prev: Option<usize>,
}

struct IndexedList<T> {
    nodes: Vec<Node<T>>,
    head: Option<usize>,
    tail: Option<usize>,
}
```

Ventajas: sin unsafe, sin Rc, sin raw pointers.
Desventajas: borrar nodos deja "huecos" en el Vec (necesitas free list o
generational indices), y la cache locality no es mejor que con punteros si
los nodos se insertan/borran frecuentemente.

---

## 6. Benchmark de overhead

### Qué medimos

El overhead real de cada enfoque, medido con la misma operación:
push $n$ elementos y luego pop $n$ elementos.

### Código de benchmark

```rust
use std::cell::RefCell;
use std::hint::black_box;
use std::rc::Rc;
use std::ptr::NonNull;
use std::time::Instant;

// ---- Versión Box ----
mod box_stack {
    struct Node {
        data: i32,
        next: Option<Box<Node>>,
    }
    pub struct Stack { head: Option<Box<Node>> }
    impl Stack {
        pub fn new() -> Self { Stack { head: None } }
        pub fn push(&mut self, data: i32) {
            self.head = Some(Box::new(Node { data, next: self.head.take() }));
        }
        pub fn pop(&mut self) -> Option<i32> {
            let node = self.head.take()?;
            self.head = node.next;
            Some(node.data)
        }
    }
}

// ---- Versión Rc<RefCell> ----
mod rc_stack {
    use std::cell::RefCell;
    use std::rc::Rc;
    type Link = Option<Rc<RefCell<Node>>>;
    struct Node { data: i32, next: Link }
    pub struct Stack { head: Link }
    impl Stack {
        pub fn new() -> Self { Stack { head: None } }
        pub fn push(&mut self, data: i32) {
            let node = Rc::new(RefCell::new(Node {
                data,
                next: self.head.take(),
            }));
            self.head = Some(node);
        }
        pub fn pop(&mut self) -> Option<i32> {
            let head = self.head.take()?;
            let node = Rc::try_unwrap(head).ok()?.into_inner();
            self.head = node.next;
            Some(node.data)
        }
    }
}

// ---- Versión raw pointers ----
mod raw_stack {
    use std::ptr::NonNull;
    struct Node { data: i32, next: Option<NonNull<Node>> }
    pub struct Stack { head: Option<NonNull<Node>> }
    impl Stack {
        pub fn new() -> Self { Stack { head: None } }
        pub fn push(&mut self, data: i32) {
            let node = Box::new(Node { data, next: self.head });
            let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };
            self.head = Some(nn);
        }
        pub fn pop(&mut self) -> Option<i32> {
            let nn = self.head?;
            unsafe {
                let node = Box::from_raw(nn.as_ptr());
                self.head = node.next;
                Some(node.data)
            }
        }
    }
    impl Drop for Stack {
        fn drop(&mut self) { while self.pop().is_some() {} }
    }
}

// ---- Versión Vec (baseline) ----
mod vec_stack {
    pub struct Stack { data: Vec<i32> }
    impl Stack {
        pub fn new() -> Self { Stack { data: Vec::new() } }
        pub fn push(&mut self, val: i32) { self.data.push(val); }
        pub fn pop(&mut self) -> Option<i32> { self.data.pop() }
    }
}

fn bench<F: FnMut()>(name: &str, n: usize, mut f: F) {
    let start = Instant::now();
    f();
    let elapsed = start.elapsed();
    println!("{name:>15}: {elapsed:>10.2?}  ({:.0} ns/op)",
             elapsed.as_nanos() as f64 / (2 * n) as f64);
}

fn main() {
    let n = 1_000_000;

    bench("Vec", n, || {
        let mut s = vec_stack::Stack::new();
        for i in 0..n { s.push(black_box(i as i32)); }
        while s.pop().is_some() {}
    });

    bench("Box", n, || {
        let mut s = box_stack::Stack::new();
        for i in 0..n { s.push(black_box(i as i32)); }
        while s.pop().is_some() {}
    });

    bench("NonNull", n, || {
        let mut s = raw_stack::Stack::new();
        for i in 0..n { s.push(black_box(i as i32)); }
        while s.pop().is_some() {}
    });

    bench("Rc<RefCell>", n, || {
        let mut s = rc_stack::Stack::new();
        for i in 0..n { s.push(black_box(i as i32)); }
        while s.pop().is_some() {}
    });
}
```

### Resultados típicos

Los números exactos varían según hardware, pero las **proporciones** son
consistentes:

```
            Vec:     12.34 ms  (  6 ns/op)
            Box:     45.67 ms  ( 23 ns/op)
        NonNull:     45.12 ms  ( 23 ns/op)
    Rc<RefCell>:     78.90 ms  ( 39 ns/op)
```

### Análisis

| Enfoque | Relativo a Vec | Por qué |
|---------|---------------|---------|
| `Vec` | 1× (baseline) | Sin allocations individuales. Push amortizado $O(1)$. Cache-friendly. |
| `Box` / `NonNull` | ~3-4× | Una allocation por push (`Box::new`). Misma velocidad entre sí. |
| `Rc<RefCell>` | ~6-7× | Allocation + incremento de contadores + RefCell borrow check. |

### Qué nos dice el benchmark

1. **`Box` y `NonNull` tienen el mismo rendimiento**. El overhead de `Box`
   no está en el envoltorio — está en la allocation individual por nodo.
   `NonNull` no mejora esto porque también usa `Box::new` → `Box::into_raw`.

2. **`Rc<RefCell>` paga ~2× sobre Box/NonNull**. Los 24 bytes extra por
   nodo causan más cache misses, y el conteo de referencias + borrow checking
   agrega instrucciones por operación.

3. **`Vec` es el rey** para datos contiguos. Cuando la estructura se puede
   implementar sobre un array (stack, heap binario, hash table), `Vec<T>`
   es la mejor opción por cache locality.

4. **La razón para usar unsafe no es la velocidad de Box vs NonNull** — es
   la **expresividad**: poder crear topologías de punteros que Box no puede
   representar (doble enlace, ciclos, punteros al padre).

---

## 7. Cuándo Rc\<RefCell\> sí es la mejor opción

A pesar del overhead, `Rc<RefCell>` es la opción correcta en estos casos:

### Prototipado rápido

Cuando estás experimentando con una estructura y no quieres lidiar con
lifetime errors ni `unsafe`:

```rust
// "Quiero ver si mi algoritmo de balanceo funciona"
// Después optimizo con raw pointers
type Link<T> = Option<Rc<RefCell<TreeNode<T>>>>;

struct TreeNode<T> {
    data: T,
    left: Link<T>,
    right: Link<T>,
    parent: Option<Weak<RefCell<TreeNode<T>>>>,
}
```

### Estructuras compartidas en GUI/event systems

Cuando múltiples partes del sistema necesitan acceso al mismo dato y no hay
un owner claro:

```rust
// Un widget puede estar en múltiples listas (hijos del padre, lista de focus, etc.)
let widget: Rc<RefCell<Widget>> = Rc::new(RefCell::new(Widget::new()));
parent.add_child(Rc::clone(&widget));
focus_list.push(Rc::clone(&widget));
```

### Cuando $n$ es pequeño

Si la estructura tiene $n < 1000$ elementos, el overhead de `Rc<RefCell>` es
irrelevante. 24 bytes × 1000 = 24 KB — no se nota.

### Cuando la corrección es más importante que el rendimiento

En producción, un leak sutil por un `Box::from_raw` faltante puede ser peor
que pagar 2× de overhead con `Rc<RefCell>`.

---

## 8. Cuándo raw pointers son necesarios

### La biblioteca estándar los usa

Las implementaciones de la biblioteca estándar de Rust usan raw pointers
internamente:

```
std::collections::LinkedList   → *mut Node<T>
std::collections::VecDeque     → raw pointer al buffer
std::collections::BTreeMap     → raw pointers a nodos
std::vec::Vec                  → raw pointer al buffer (RawVec)
```

Todas exponen una interfaz safe. El unsafe está encapsulado.

### Regla práctica

```
Usa raw pointers cuando:
1. La estructura NECESITA aliasing mutable (doble enlace, padre, grafo)
2. El rendimiento ES crítico y mediste que Rc<RefCell> es el cuello de botella
3. Estás implementando una biblioteca que otros van a usar con interfaz safe
4. Estás haciendo FFI con C

NO uses raw pointers cuando:
1. Option<Box> resuelve el problema
2. No has medido el rendimiento (optimización prematura)
3. El código es un prototipo que va a cambiar
4. No vas a escribir tests + miri para verificar
```

---

## 9. Checklist antes de elegir unsafe

```
□ ¿Intentaste Option<Box<Node<T>>>?
  → Si funciona, úsalo. Es la opción más segura.

□ ¿La limitación es ownership único?
  → ¿Puedes reestructurar para evitar múltiples owners?
  → ¿Puedes usar índices en un Vec en vez de punteros?

□ ¿Consideraste Rc<RefCell> con Weak?
  → ¿El overhead es aceptable para tu caso de uso?
  → ¿n es lo suficientemente grande para que importe?

□ Si decides usar unsafe:
  → ¿Puedes encapsular todo el unsafe en un módulo con interfaz safe?
  → ¿Vas a verificar con cargo miri test?
  → ¿Escribiste // SAFETY: en cada bloque unsafe?
  → ¿Implementaste Drop correctamente?

□ ¿Es multi-thread?
  → Rc<RefCell> no funciona → Arc<Mutex> o raw pointers + unsafe Send/Sync
```

---

## 10. Mapa completo de opciones

| Necesidad | Solución safe | Solución unsafe |
|-----------|--------------|----------------|
| Stack / LIFO | `Vec<T>` o `Option<Box<Node>>` | — |
| Cola / FIFO | `VecDeque<T>` | `*mut` head + tail |
| Lista simple (forward) | `Option<Box<Node>>` | — |
| Lista doble | `Rc<RefCell>` + `Weak` | `Option<NonNull<Node>>` |
| Árbol binario (BST simple) | `Option<Box<Node>>` | — |
| Árbol con rotaciones (AVL/RB) | `Rc<RefCell>` + `Weak` | `Option<NonNull<Node>>` |
| Heap binario | `Vec<T>` (array-based) | — |
| Hash table | `Vec<Bucket>` | — |
| Trie | `Option<Box<Node>>` con array de hijos | — |
| Grafo (adjacency list) | `Vec<Vec<usize>>` (índices) | `*mut Node` |
| Skip list | — | `*mut` (múltiples niveles de forward) |
| Arena allocator | — | Raw pointers al buffer |
| Estructura intrusiva | — | Raw pointers + offset |

Patrón: las estructuras **array-based** (heap, hash, deque) no necesitan
unsafe. Las estructuras **pointer-based con ciclos** sí.

---

## Ejercicios

### Ejercicio 1 — Elegir la representación

Para cada estructura, elige la representación más apropiada y justifica:

```
a) Stack de enteros con máximo 1000 elementos
b) Lista doblemente enlazada de strings
c) Árbol binario de búsqueda sin operación "parent"
d) Árbol AVL con rotaciones
e) Grafo dirigido de 50 nodos
f) Cola de prioridad (min-heap)
g) Buffer circular de tamaño fijo
h) Cache LRU (least recently used)
```

**Prediccion**: Cuántas de las 8 necesitan raw pointers?

<details><summary>Respuesta</summary>

| Estructura | Representación | Justificación |
|-----------|---------------|---------------|
| a) Stack ≤ 1000 | `Vec<i32>` | Array-based, sin allocation por push, $O(1)$ amortizado |
| b) Lista doble | `Option<NonNull<Node>>` (unsafe) | Doble enlace requiere aliasing mutable |
| c) BST sin padre | `Option<Box<Node<T>>>` | Ownership único: padre → hijo. Sin ciclos |
| d) Árbol AVL | `Option<NonNull<Node>>` (unsafe) | Rotaciones necesitan puntero a padre para subir |
| e) Grafo 50 nodos | `Vec<Vec<usize>>` (adjacency list) | 50 nodos es pequeño. Índices son más simples que punteros |
| f) Min-heap | `Vec<T>` | Heap binario siempre se implementa sobre array |
| g) Buffer circular | `Vec<T>` + `head`/`tail` índices | Array-based con aritmética modular |
| h) Cache LRU | `Option<NonNull<Node>>` + `HashMap` (unsafe) | LRU = hash table + lista doble para O(1) en todo |

**3 de 8** necesitan raw pointers: la lista doble (b), el AVL (d), y el
cache LRU (h). Las otras 5 se resuelven con `Vec` o `Box`.

Nota sobre (d): un AVL **sin** puntero a padre es posible con recursión
(las rotaciones se hacen durante el retorno de la recursión), y en ese caso
`Option<Box<Node>>` funciona. Pero la versión iterativa (más eficiente) necesita
puntero a padre → unsafe.

Nota sobre (h): LRU es hash map + lista doblemente enlazada. La lista doble
permite mover un nodo al frente en $O(1)$ cuando se accede. El hash map
permite encontrar el nodo en $O(1)$. Ambos necesitan apuntar al mismo nodo →
aliasing → unsafe.

</details>

---

### Ejercicio 2 — Migrar de Rc\<RefCell\> a raw pointers

Toma esta lista doblemente enlazada con `Rc<RefCell>` y reescríbela con
`Option<NonNull>`. Solo implementa `push_back` y `pop_front`:

```rust
use std::cell::RefCell;
use std::rc::{Rc, Weak};

type Strong<T> = Option<Rc<RefCell<Node<T>>>>;
type WeakRef<T> = Option<Weak<RefCell<Node<T>>>>;

struct Node<T> {
    data: T,
    next: Strong<T>,
    prev: WeakRef<T>,
}

pub struct DList<T> {
    head: Strong<T>,
    tail: WeakRef<T>,
    len: usize,
}
```

**Prediccion**: Cuántos bytes por nodo ahorra la versión raw vs la Rc\<RefCell\>?

<details><summary>Respuesta</summary>

```rust
use std::ptr::NonNull;

struct Node<T> {
    data: T,
    next: Option<NonNull<Node<T>>>,
    prev: Option<NonNull<Node<T>>>,
}

pub struct DList<T> {
    head: Option<NonNull<Node<T>>>,
    tail: Option<NonNull<Node<T>>>,
    len: usize,
}

impl<T> DList<T> {
    pub fn new() -> Self {
        DList { head: None, tail: None, len: 0 }
    }

    pub fn push_back(&mut self, data: T) {
        let node = Box::new(Node {
            data,
            next: None,
            prev: self.tail,
        });
        // SAFETY: Box::into_raw retorna no-null.
        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };

        match self.tail {
            None => self.head = Some(nn),
            Some(old_tail) => {
                // SAFETY: old_tail es válido, creado con Box::into_raw.
                unsafe { (*old_tail.as_ptr()).next = Some(nn); }
            }
        }
        self.tail = Some(nn);
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        let head_nn = self.head?;

        // SAFETY: head_nn es NonNull, creado con Box::into_raw.
        unsafe {
            let node = Box::from_raw(head_nn.as_ptr());
            self.head = node.next;

            match self.head {
                None => self.tail = None,
                Some(new_head) => {
                    // SAFETY: new_head es válido.
                    (*new_head.as_ptr()).prev = None;
                }
            }

            self.len -= 1;
            Some(node.data)
        }
    }
}

impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        while self.pop_front().is_some() {}
    }
}

fn main() {
    let mut list = DList::new();
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);

    while let Some(val) = list.pop_front() {
        println!("{val}");
    }
}
```

Ahorro por nodo (asumiendo `T = i32`, arquitectura 64-bit):

```
Rc<RefCell<Node<i32>>>:
  strong_count:  8 bytes
  weak_count:    8 bytes
  borrow_flag:   8 bytes (RefCell)
  data (i32):    4 bytes
  next (Rc):     8 bytes
  prev (Weak):   8 bytes
  padding:       4 bytes
  Total:        ~48 bytes por nodo

Option<NonNull<Node<i32>>>:
  data (i32):    4 bytes
  next (Option<NonNull>): 8 bytes
  prev (Option<NonNull>): 8 bytes
  padding:       4 bytes
  Total:        ~24 bytes por nodo
```

Ahorro: **~24 bytes por nodo** (50% menos memoria).
Para $10^6$ nodos: $24 \times 10^6 = 24$ MB de ahorro.

</details>

---

### Ejercicio 3 — Implementar con índices (sin unsafe)

Implementa una lista doblemente enlazada usando índices en un `Vec` en vez
de punteros. Sin `unsafe`:

```rust
struct Node<T> {
    data: T,
    prev: Option<usize>,
    next: Option<usize>,
}

pub struct IndexedDList<T> {
    nodes: Vec<Option<Node<T>>>,   // Some = ocupado, None = libre
    head: Option<usize>,
    tail: Option<usize>,
    free: Vec<usize>,              // índices libres para reusar
    len: usize,
}
```

Implementa `new`, `push_back`, `pop_front`, y `print_forward`.

**Prediccion**: Esta versión tiene las mismas operaciones $O(1)$ que la versión
con punteros?

<details><summary>Respuesta</summary>

```rust
struct Node<T> {
    data: T,
    prev: Option<usize>,
    next: Option<usize>,
}

pub struct IndexedDList<T> {
    nodes: Vec<Option<Node<T>>>,
    head: Option<usize>,
    tail: Option<usize>,
    free: Vec<usize>,
    len: usize,
}

impl<T: std::fmt::Display> IndexedDList<T> {
    pub fn new() -> Self {
        IndexedDList {
            nodes: Vec::new(),
            head: None,
            tail: None,
            free: Vec::new(),
            len: 0,
        }
    }

    fn alloc_slot(&mut self, node: Node<T>) -> usize {
        if let Some(idx) = self.free.pop() {
            self.nodes[idx] = Some(node);
            idx
        } else {
            let idx = self.nodes.len();
            self.nodes.push(Some(node));
            idx
        }
    }

    pub fn push_back(&mut self, data: T) {
        let idx = self.alloc_slot(Node {
            data,
            prev: self.tail,
            next: None,
        });

        if let Some(tail_idx) = self.tail {
            if let Some(ref mut tail_node) = self.nodes[tail_idx] {
                tail_node.next = Some(idx);
            }
        } else {
            self.head = Some(idx);
        }

        self.tail = Some(idx);
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        let head_idx = self.head?;
        let node = self.nodes[head_idx].take()?;

        self.head = node.next;
        if let Some(new_head) = self.head {
            if let Some(ref mut new_head_node) = self.nodes[new_head] {
                new_head_node.prev = None;
            }
        } else {
            self.tail = None;
        }

        self.free.push(head_idx);
        self.len -= 1;
        Some(node.data)
    }

    pub fn print_forward(&self) {
        let mut current = self.head;
        while let Some(idx) = current {
            if let Some(ref node) = self.nodes[idx] {
                print!("{} ", node.data);
                current = node.next;
            } else {
                break;  // slot vacío — bug
            }
        }
        println!();
    }
}

fn main() {
    let mut list = IndexedDList::new();
    list.push_back(10);
    list.push_back(20);
    list.push_back(30);
    list.print_forward();           // 10 20 30

    list.pop_front();
    list.print_forward();           // 20 30

    list.push_back(40);
    list.print_forward();           // 20 30 40
    // 40 reutiliza el slot de 10 (free list)
}
```

Sí, las operaciones son $O(1)$ — `push_back` y `pop_front` hacen un número
constante de accesos a `Vec` por índice (que es $O(1)$).

Tradeoffs vs punteros:

| Aspecto | Índices en Vec | Raw pointers |
|---------|---------------|-------------|
| Unsafe | No | Sí |
| $O(1)$ push/pop | Sí | Sí |
| Memoria | `Vec<Option<Node>>` tiene overhead por `Option` + slots vacíos | Solo nodos activos |
| Borrar nodo del medio | $O(1)$ (marcar slot como None) | $O(1)$ (relink + free) |
| Cache locality | Buena (nodos contiguos en Vec) | Variable (cada `Box::new` puede ir a cualquier dirección) |
| Complejidad del código | Media (free list management) | Media (unsafe management) |

La versión con índices es una alternativa legítima para evitar unsafe,
especialmente cuando la cache locality importa.

</details>

---

### Ejercicio 4 — Medir overhead real

Ejecuta el benchmark de la sección 6 en tu máquina. Usa $n = 10^6$.
Responde:

1. Cuál es la diferencia entre Box y NonNull? (deberían ser similares)
2. Cuánto más lento es Rc\<RefCell\> que Box?
3. Cuánto más rápido es Vec que todas las versiones con nodos?

```rust
// Copia el código de la sección 6 y ejecútalo con:
// cargo run --release
```

**Prediccion**: Antes de ejecutar, estima: Rc\<RefCell\> será 2×, 5×, o 10×
más lento que Box?

<details><summary>Respuesta</summary>

Compila y ejecuta con optimizaciones:

```bash
cargo run --release
```

Resultados de referencia (los tuyos variarán pero las proporciones se mantienen):

```
            Vec:     12.34 ms  (  6 ns/op)
            Box:     45.67 ms  ( 23 ns/op)
        NonNull:     45.12 ms  ( 23 ns/op)
    Rc<RefCell>:     78.90 ms  ( 39 ns/op)
```

Respuestas esperadas:

1. **Box vs NonNull**: diferencia < 5%. Ambos hacen lo mismo — `Box` internamente
   es un raw pointer. La abstracción de `Box` tiene costo cero (zero-cost
   abstraction).

2. **Rc\<RefCell\> vs Box**: ~1.5-2× más lento. El overhead viene de:
   - Incrementar/decrementar `strong_count` en cada clone/drop
   - `borrow_mut()` verifica y modifica el borrow flag
   - Nodos más grandes (24 bytes extra) → más cache misses

3. **Vec vs nodos**: ~3-4× más rápido. Vec no hace allocation individual por
   push (solo reallocs amortizados), y los datos están contiguos en memoria
   → excelente cache locality.

La lección: para un **stack**, usa `Vec<T>`. Para estructuras que **necesitan**
nodos enlazados (lista doble, árboles con padre), elige entre `Rc<RefCell>` y
raw pointers según el tradeoff safety/rendimiento.

</details>

---

### Ejercicio 5 — Cola FIFO: Box vs raw

Intenta implementar una cola FIFO (enqueue al final, dequeue del frente) usando
`Option<Box<Node<T>>>`. Encuentra la limitación:

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}

pub struct Queue<T> {
    head: Option<Box<Node<T>>>,
    // tail: ???   ← ¿qué tipo usas?
}
```

**Prediccion**: Puedes tener un `tail` que apunte al último nodo con `Box`?
Si no, por qué?

<details><summary>Respuesta</summary>

No puedes tener `tail: &mut Node<T>` porque el lifetime de tail dependería
del contenido de `head`, creando un self-referential struct. Tampoco puedes
tener `tail: Box<Node<T>>` porque eso significaría dos owners del mismo nodo.

Las opciones son:

**Opción A: sin tail, O(n) enqueue** — funcional pero lento:

```rust
pub struct Queue<T> {
    head: Option<Box<Node<T>>>,
}

impl<T> Queue<T> {
    pub fn enqueue(&mut self, data: T) {
        let new_node = Box::new(Node { data, next: None });

        if self.head.is_none() {
            self.head = Some(new_node);
            return;
        }

        // Recorrer hasta el final: O(n)
        let mut current = &mut self.head;
        while let Some(ref mut node) = current {
            if node.next.is_none() {
                node.next = Some(new_node);
                return;
            }
            current = &mut node.next;
        }
    }
}
```

Esto funciona pero `enqueue` es $O(n)$ porque necesita recorrer toda la lista.

**Opción B: raw pointer para tail, O(1) enqueue**:

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct Queue<T> {
    head: *mut Node<T>,
    tail: *mut Node<T>,
    len: usize,
}

impl<T> Queue<T> {
    pub fn new() -> Self {
        Queue { head: ptr::null_mut(), tail: ptr::null_mut(), len: 0 }
    }

    pub fn enqueue(&mut self, data: T) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: ptr::null_mut(),
        }));

        if self.tail.is_null() {
            self.head = node;
        } else {
            // SAFETY: tail es válido (no null, creado con Box::into_raw).
            unsafe { (*self.tail).next = node; }
        }
        self.tail = node;
        self.len += 1;
    }

    pub fn dequeue(&mut self) -> Option<T> {
        if self.head.is_null() { return None; }
        // SAFETY: head es válido.
        unsafe {
            let old_head = self.head;
            self.head = (*old_head).next;
            if self.head.is_null() {
                self.tail = ptr::null_mut();
            }
            let node = Box::from_raw(old_head);
            self.len -= 1;
            Some(node.data)
        }
    }
}

impl<T> Drop for Queue<T> {
    fn drop(&mut self) {
        while self.dequeue().is_some() {}
    }
}
```

Este es un caso donde `Box` solo **no puede** dar $O(1)$ en ambos extremos —
necesitas un puntero auxiliar al tail, que es inherentemente unsafe porque
es una segunda referencia al interior de la estructura de nodos.

**Opción C: `VecDeque<T>`** — la solución pragmática:

```rust
use std::collections::VecDeque;

let mut q = VecDeque::new();
q.push_back(1);     // O(1) amortizado
q.pop_front();      // O(1)
```

Si no necesitas implementar la cola tú mismo, `VecDeque` es la respuesta
correcta — $O(1)$ amortizado en ambos extremos, sin unsafe, excelente
cache locality.

</details>

---

### Ejercicio 6 — Árbol de decisión aplicado

Para cada escenario, sigue el árbol de decisión de la sección 5 y llega a
una conclusión:

```
Escenario A:
  Necesitas un conjunto ordenado de strings que soporte inserción,
  búsqueda, y recorrido in-order. No necesitas puntero al padre.
  n ≈ 10,000.

Escenario B:
  Implementas un editor de texto. Necesitas una lista doblemente
  enlazada de líneas donde puedas insertar/borrar en O(1) en
  cualquier posición (dado un cursor).

Escenario C:
  Necesitas un sistema de caché donde las entradas más
  recientemente accedidas se muevan al frente. Acceso por
  clave en O(1). n ≈ 500.

Escenario D:
  Representar un árbol de dependencias de paquetes. Los paquetes
  pueden depender de múltiples otros paquetes (grafo DAG).
  n ≈ 200 paquetes.
```

**Prediccion**: Cuántos escenarios requieren unsafe?

<details><summary>Respuesta</summary>

**Escenario A — BST sin padre, $n = 10{,}000$**

```
¿Un solo owner por nodo? → SÍ (padre → hijo)
¿Necesita acceso O(1) al final? → NO
→ Option<Box<Node<T>>>
```

Sin padre y sin ciclos, `Box` funciona perfecto. Para $n = 10{,}000$, la
profundidad del árbol balanceado es $\log_2(10{,}000) \approx 14$ — las
operaciones son rápidas.

Alternativa pragmática: `BTreeMap<String, ()>` o `BTreeSet<String>` de la
biblioteca estándar.

**Escenario B — Editor de texto, lista doble con cursor**

```
¿Un solo owner por nodo? → NO (prev + next = doble enlace)
¿Múltiples owners? → SÍ (nodo apuntado por prev del siguiente Y next del anterior)
→ ¿Overhead de Rc aceptable?
  → Para un editor, el rendimiento importa (el usuario nota lag)
  → Raw pointers
```

Un editor de texto con operaciones frecuentes de inserción/borrado necesita
$O(1)$ real (no amortizado). `Rc<RefCell>` agrega overhead innecesario.
Raw pointers dan rendimiento idéntico a C.

**Escenario C — Cache LRU, $n = 500$**

```
¿Múltiples owners? → SÍ (HashMap apunta al nodo + lista doble)
→ ¿Overhead de Rc aceptable?
  → n = 500, overhead total: 500 × 24 = 12 KB → irrelevante
  → Rc<RefCell> + Weak
```

Para 500 entradas, el overhead de `Rc<RefCell>` es despreciable. La corrección
y facilidad de debugging justifican evitar unsafe.

Sin embargo, si este cache está en un hot path (miles de lookups por segundo),
medirlo antes de decidir.

**Escenario D — Grafo DAG de dependencias, $n = 200$**

```
¿Ciclos? → No (DAG = directed acyclic graph)
¿Múltiples padres por nodo? → SÍ (un paquete puede ser dependencia de varios)
→ n = 200, overhead irrelevante
→ Vec<Vec<usize>> (adjacency list con índices)
```

Para 200 nodos, la opción más simple es un `Vec` de nodos con listas de
adyacencia por índice. Sin punteros, sin Rc, sin unsafe:

```rust
struct Package {
    name: String,
    dependencies: Vec<usize>,  // índices en el Vec principal
}
let packages: Vec<Package> = Vec::new();
```

**Resumen: 1 de 4 requiere unsafe** (el editor de texto). Los otros 3 se
resuelven con abstracciones safe.

</details>

---

### Ejercicio 7 — Implementar Drop correcto

Esta lista doble tiene un bug en su `Drop`. Encuéntralo y corrígelo:

```rust
use std::ptr::NonNull;

struct Node<T> {
    data: T,
    prev: Option<NonNull<Node<T>>>,
    next: Option<NonNull<Node<T>>>,
}

pub struct DList<T> {
    head: Option<NonNull<Node<T>>>,
    tail: Option<NonNull<Node<T>>>,
}

impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        let mut current = self.head;
        while let Some(nn) = current {
            unsafe {
                current = (*nn.as_ptr()).next;
                drop(Box::from_raw(nn.as_ptr()));
            }
        }
    }
}
```

**Prediccion**: Si un nodo tiene `prev` apuntando a un nodo ya liberado,
eso causa UB al hacer drop?

<details><summary>Respuesta</summary>

El código presentado en realidad **funciona correctamente** para la liberación
de memoria — recorre forward y libera cada nodo. El `prev` del nodo apunta a
un nodo ya liberado, pero como **no se accede** a `prev` durante el drop
(solo se lee `next`), no hay UB.

Sin embargo, hay un **problema sutil**: si `T` tiene un `Drop` personalizado
que accede a `prev` o `next` (poco probable pero posible si `Node` expone
estos campos), habría UB.

El problema más real con este código es que **no limpia `head`/`tail`**. Si
algún código (por ejemplo, un destructor de otro campo del mismo struct) accede
a `self.head` después del loop, encontraría un dangling pointer.

Versión más robusta:

```rust
impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        let mut current = self.head.take();  // take limpia head
        while let Some(nn) = current {
            unsafe {
                let mut node = Box::from_raw(nn.as_ptr());
                current = node.next.take();  // take limpia next del nodo
                // node.prev se queda como está — no se accede
            }
            // node se destruye aquí: libera T y la memoria del nodo
        }
        self.tail = None;  // limpiar tail
    }
}
```

Mejoras:
1. `self.head.take()` — deja `head` como `None` inmediatamente
2. `node.next.take()` — limpia el enlace antes de liberar (evita que el
   destructor de `T` pueda seguir el puntero)
3. `self.tail = None` — consistencia

Sobre la predicción: no, `prev` apuntando a un nodo liberado **no causa UB
durante el drop** porque el drop solo recorre `next`. Los `prev` colgantes
(dangling) son inofensivos siempre que nadie los desreferencie. Serían un
problema solo si el destructor de `T` o un código entre el `Box::from_raw`
y el final del scope accediera a `prev`.

</details>

---

### Ejercicio 8 — Rc\<RefCell\> leak demo

Demuestra cómo `Rc<RefCell>` puede causar un leak de memoria creando un
ciclo. Luego corrígelo con `Weak`:

```rust
use std::cell::RefCell;
use std::rc::Rc;

struct Node {
    name: String,
    next: Option<Rc<RefCell<Node>>>,
}

fn create_cycle() {
    let a = Rc::new(RefCell::new(Node {
        name: String::from("A"),
        next: None,
    }));
    let b = Rc::new(RefCell::new(Node {
        name: String::from("B"),
        next: Some(Rc::clone(&a)),   // B → A
    }));

    // Crear ciclo: A → B
    a.borrow_mut().next = Some(Rc::clone(&b));

    println!("a strong count: {}", Rc::strong_count(&a));
    println!("b strong count: {}", Rc::strong_count(&b));
}

fn main() {
    create_cycle();
    println!("After create_cycle returns...");
    // ¿Se liberaron A y B?
}
```

**Prediccion**: Al salir de `create_cycle`, cuánto es el strong_count de A y B?
Se liberan?

<details><summary>Respuesta</summary>

```
a strong count: 2   (la variable a + b.next)
b strong count: 2   (la variable b + a.next)
```

Al salir de `create_cycle`:
- Se destruye la variable `b` → strong_count de B baja a 1 (queda `a.next`)
- Se destruye la variable `a` → strong_count de A baja a 1 (queda `b.next`)
- Ninguno llega a 0 → **ninguno se libera** → **memory leak**

```
Después de drop de variables locales:
  A (count=1) → B (count=1) → A (count=1) → ...
  Ciclo infinito de referencias. Nadie llega a count=0.
```

Versión corregida con `Weak`:

```rust
use std::cell::RefCell;
use std::rc::{Rc, Weak};

struct Node {
    name: String,
    next: Option<Rc<RefCell<Node>>>,
    back: Option<Weak<RefCell<Node>>>,  // Weak para romper el ciclo
}

fn no_cycle() {
    let a = Rc::new(RefCell::new(Node {
        name: String::from("A"),
        next: None,
        back: None,
    }));
    let b = Rc::new(RefCell::new(Node {
        name: String::from("B"),
        next: None,
        back: Some(Rc::downgrade(&a)),   // B -weak-> A
    }));

    // A tiene strong ref a B
    a.borrow_mut().next = Some(Rc::clone(&b));

    println!("a strong: {}, weak: {}",
             Rc::strong_count(&a), Rc::weak_count(&a));
    println!("b strong: {}, weak: {}",
             Rc::strong_count(&b), Rc::weak_count(&b));
}

fn main() {
    no_cycle();
    println!("After no_cycle: memory freed correctly");
}
```

```
a strong: 1, weak: 1    (variable a + weak desde b.back)
b strong: 2, weak: 0    (variable b + a.next)
```

Al salir:
- Drop `b` → B strong_count: 2 → 1 (queda `a.next`)
- Drop `a` → A strong_count: 1 → 0 → **A se libera**
  - Al liberar A, `a.next` (que es `Rc<B>`) se destruye → B count: 1 → 0
  - **B se libera**
  - `b.back` (Weak) no impide la liberación de A

Sin leak. La regla: en un par de referencias mutuas, una debe ser `Weak`.

</details>

---

### Ejercicio 9 — Benchmark: índices vs punteros

Implementa la misma operación (push $n$ + pop $n$) usando la lista por
índices del ejercicio 3 y la lista con raw pointers. Mide cuál es más
rápida para $n = 10^6$:

```rust
fn main() {
    let n = 1_000_000;

    // Medir IndexedDList
    let start = std::time::Instant::now();
    let mut indexed = IndexedDList::new();
    for i in 0..n { indexed.push_back(i); }
    while indexed.pop_front().is_some() {}
    println!("Indexed: {:?}", start.elapsed());

    // Medir DList (raw pointers)
    let start = std::time::Instant::now();
    let mut raw = DList::new();
    for i in 0..n { raw.push_back(i); }
    while raw.pop_front().is_some() {}
    println!("Raw:     {:?}", start.elapsed());
}
```

Compila con `cargo run --release`.

**Prediccion**: Cuál es más rápida? La versión indexed tiene mejor cache
locality pero overhead de Vec management.

<details><summary>Respuesta</summary>

Resultados típicos:

```
Indexed: ~45-55 ms
Raw:     ~50-60 ms
```

Los resultados suelen ser **similares**, con la versión indexed a veces
ligeramente más rápida.

Por qué son similares:
- **Indexed**: los nodos están contiguos en el Vec → excelente cache locality.
  Pero `Vec::push` hace bounds check y ocasionalmente realloc.
- **Raw**: cada `Box::new` pide memoria al allocator individualmente →
  nodos dispersos en memoria. Pero no tiene overhead de Vec management.

El tradeoff:
- Para acceso **secuencial** (push/pop en orden): indexed es ligeramente mejor
  por cache locality
- Para acceso **aleatorio** (acceder nodo 500, luego nodo 3, luego nodo 999):
  ambos son similares si los nodos indexed no están compactos (slots vacíos)
- Para **borrar del medio** frecuentemente: indexed deja huecos,
  raw pointers no desperdician espacio

En la práctica, la diferencia es menor al 20% para esta operación. La
decisión entre índices y raw pointers se basa más en **ergonomía** (índices
= safe, punteros = unsafe) que en rendimiento.

Nota: compila siempre con `--release`. Sin optimizaciones, los bounds checks
de Vec penalizan mucho a la versión indexed.

</details>

---

### Ejercicio 10 — Diseñar la API de un BST

Diseña (sin implementar completamente) la API de un BST (árbol binario de
búsqueda) que use `Option<Box<Node<T>>>` para un BST sin puntero a padre.
Define los tipos y las firmas de los métodos:

```rust
// Define Node<T> y BST<T>
// Métodos: new, insert, contains, min, max, in_order
// T: Ord para comparación
```

Luego responde: si quisieras agregar una operación `delete` que necesite
acceder al padre, qué cambiarías?

**Prediccion**: `delete` sin puntero a padre es posible con recursión?

<details><summary>Respuesta</summary>

```rust
struct Node<T> {
    data: T,
    left: Option<Box<Node<T>>>,
    right: Option<Box<Node<T>>>,
}

pub struct BST<T: Ord> {
    root: Option<Box<Node<T>>>,
    len: usize,
}

impl<T: Ord> BST<T> {
    /// Crea un BST vacío.
    pub fn new() -> Self {
        BST { root: None, len: 0 }
    }

    /// Inserta un valor. Retorna true si se insertó (no existía).
    pub fn insert(&mut self, data: T) -> bool {
        Self::insert_rec(&mut self.root, data, &mut self.len)
    }

    fn insert_rec(node: &mut Option<Box<Node<T>>>, data: T, len: &mut usize) -> bool {
        match node {
            None => {
                *node = Some(Box::new(Node { data, left: None, right: None }));
                *len += 1;
                true
            }
            Some(n) => {
                use std::cmp::Ordering;
                match data.cmp(&n.data) {
                    Ordering::Less => Self::insert_rec(&mut n.left, data, len),
                    Ordering::Greater => Self::insert_rec(&mut n.right, data, len),
                    Ordering::Equal => false,  // ya existe
                }
            }
        }
    }

    /// Retorna true si el valor existe en el árbol.
    pub fn contains(&self, data: &T) -> bool {
        Self::contains_rec(&self.root, data)
    }

    fn contains_rec(node: &Option<Box<Node<T>>>, data: &T) -> bool {
        match node {
            None => false,
            Some(n) => match data.cmp(&n.data) {
                std::cmp::Ordering::Less => Self::contains_rec(&n.left, data),
                std::cmp::Ordering::Greater => Self::contains_rec(&n.right, data),
                std::cmp::Ordering::Equal => true,
            }
        }
    }

    /// Retorna referencia al mínimo.
    pub fn min(&self) -> Option<&T> {
        let mut current = &self.root;
        let mut result = None;
        while let Some(ref node) = current {
            result = Some(&node.data);
            current = &node.left;
        }
        result
    }

    /// Retorna referencia al máximo.
    pub fn max(&self) -> Option<&T> {
        let mut current = &self.root;
        let mut result = None;
        while let Some(ref node) = current {
            result = Some(&node.data);
            current = &node.right;
        }
        result
    }

    /// Recorrido in-order (izquierda, nodo, derecha).
    pub fn in_order(&self) -> Vec<&T> {
        let mut result = Vec::new();
        Self::in_order_rec(&self.root, &mut result);
        result
    }

    fn in_order_rec<'a>(node: &'a Option<Box<Node<T>>>, result: &mut Vec<&'a T>) {
        if let Some(ref n) = node {
            Self::in_order_rec(&n.left, result);
            result.push(&n.data);
            Self::in_order_rec(&n.right, result);
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

fn main() {
    let mut bst = BST::new();
    bst.insert(5);
    bst.insert(3);
    bst.insert(7);
    bst.insert(1);
    bst.insert(4);

    println!("contains 3: {}", bst.contains(&3));  // true
    println!("contains 6: {}", bst.contains(&6));  // false
    println!("min: {:?}", bst.min());               // Some(1)
    println!("max: {:?}", bst.max());               // Some(7)
    println!("in_order: {:?}", bst.in_order());     // [1, 3, 4, 5, 7]
}
```

Sí, `delete` **sin puntero a padre es posible con recursión**:

```rust
impl<T: Ord> BST<T> {
    pub fn delete(&mut self, data: &T) -> bool {
        Self::delete_rec(&mut self.root, data, &mut self.len)
    }

    fn delete_rec(node: &mut Option<Box<Node<T>>>, data: &T, len: &mut usize) -> bool {
        let n = match node {
            None => return false,
            Some(n) => n,
        };

        match data.cmp(&n.data) {
            std::cmp::Ordering::Less => Self::delete_rec(&mut n.left, data, len),
            std::cmp::Ordering::Greater => Self::delete_rec(&mut n.right, data, len),
            std::cmp::Ordering::Equal => {
                // Caso 1: sin hijos o un hijo
                if n.left.is_none() {
                    *node = n.right.take();
                } else if n.right.is_none() {
                    *node = n.left.take();
                } else {
                    // Caso 2: dos hijos — reemplazar con sucesor in-order
                    let successor_data = Self::extract_min(&mut n.right);
                    n.data = successor_data;
                }
                *len -= 1;
                true
            }
        }
    }

    fn extract_min(node: &mut Option<Box<Node<T>>>) -> T {
        if node.as_ref().unwrap().left.is_none() {
            let n = node.take().unwrap();
            *node = n.right;
            n.data
        } else {
            Self::extract_min(&mut node.as_mut().unwrap().left)
        }
    }
}
```

La recursión baja por el árbol con `&mut` al enlace del padre, lo que
permite modificar `*node` para "desconectar" el nodo encontrado. No se
necesita puntero a padre porque **la recursión mantiene la referencia al
padre implícitamente en el call stack**.

Si quisieras una versión **iterativa** de delete (o rotaciones de AVL/RB),
necesitarías puntero a padre para subir → raw pointers.

</details>
