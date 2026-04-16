# Implementación en Rust

## El problema del doble enlace en Rust

Una lista doblemente enlazada tiene la propiedad de que cada nodo es apuntado
por **dos** nodos: su anterior (`prev`) y su siguiente (`next`).  Esto viola
directamente la regla fundamental de Rust: **un valor tiene exactamente un
dueño**.

```
¿Quién es el dueño del nodo B?

  [A] ──next──→ [B] ←──prev── [C]
        ↑                ↑
      ¿dueño?          ¿dueño?
```

En C no hay problema — los punteros no implican ownership.  En Rust, hay tres
estrategias para resolverlo:

| Estrategia | Mecanismo | Safety | Overhead |
|-----------|-----------|--------|----------|
| `Rc<RefCell<Node<T>>>` | Reference counting + borrowing dinámico | Safe | Alto |
| `*mut Node<T>` | Raw pointers | Unsafe | Ninguno |
| `LinkedList<T>` | Stdlib (internamente unsafe) | Safe (API) | Ninguno |

---

## Estrategia 1: Rc<RefCell<Node<T>>>

### Fundamentos

`Rc<T>` (Reference Counted) permite múltiples dueños del mismo valor.  Cada
`Rc::clone` incrementa un contador; cuando el último `Rc` se destruye, el valor
se libera.

`RefCell<T>` permite mutabilidad interior — verificar las reglas de borrowing en
**runtime** en vez de en compilación.

Combinados: `Rc<RefCell<Node<T>>>` es un nodo que puede tener múltiples
propietarios (prev y next) y que se puede mutar a través de cualquiera de ellos.

```rust
use std::rc::Rc;
use std::cell::RefCell;

type Link<T> = Option<Rc<RefCell<DNode<T>>>>;

struct DNode<T> {
    data: T,
    prev: Link<T>,
    next: Link<T>,
}
```

### El problema de los ciclos

`Rc` no puede manejar **ciclos de referencia**.  En una lista doble, A→B y B→A
forman un ciclo: ninguno de los dos llega a conteo 0 porque el otro lo mantiene
vivo.  Resultado: **memory leak**.

```
A.next = Rc(B)     → refcount(B) = 2 (next de A + variable externa)
B.prev = Rc(A)     → refcount(A) = 2 (prev de B + variable externa)

Drop variable externa de A → refcount(A) = 1 (B.prev todavía lo tiene)
Drop variable externa de B → refcount(B) = 1 (A.next todavía lo tiene)

Ninguno llega a 0 → leak
```

### Solución: Weak para prev

`Weak<T>` es una referencia no-owning a un `Rc`.  No incrementa el conteo
fuerte — cuando todos los `Rc` se destruyen, el valor se libera aunque existan
`Weak` pendientes.

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

type StrongLink<T> = Option<Rc<RefCell<DNode<T>>>>;
type WeakLink<T> = Option<Weak<RefCell<DNode<T>>>>;

struct DNode<T> {
    data: T,
    prev: WeakLink<T>,     // Weak — no mantiene vivo al anterior
    next: StrongLink<T>,   // Rc — dueño del siguiente
}

pub struct DList<T> {
    head: StrongLink<T>,
    tail: WeakLink<T>,     // Weak — no mantiene vivo al último
    len: usize,
}
```

La cadena de ownership fluye en una sola dirección (`next`), mientras que
`prev` y `tail` son observadores que no impiden la destrucción:

```
head ──Rc──→ [A] ──Rc──→ [B] ──Rc──→ [C]
              ↑            ↑            ↑
              Weak         Weak         Weak ←── tail
              (B.prev)     (C.prev)
```

### Implementación completa con Rc<RefCell>

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;
use std::fmt;

type StrongLink<T> = Option<Rc<RefCell<DNode<T>>>>;
type WeakLink<T> = Option<Weak<RefCell<DNode<T>>>>;

struct DNode<T> {
    data: T,
    prev: WeakLink<T>,
    next: StrongLink<T>,
}

pub struct DList<T> {
    head: StrongLink<T>,
    tail: WeakLink<T>,
    len: usize,
}

impl<T> DList<T> {
    pub fn new() -> Self {
        DList { head: None, tail: None, len: 0 }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}
```

#### push_front

```rust
pub fn push_front(&mut self, value: T) {
    let new_node = Rc::new(RefCell::new(DNode {
        data: value,
        prev: None,
        next: self.head.take(),   // nuevo → viejo head
    }));

    match new_node.borrow().next.as_ref() {
        Some(old_head) => {
            // viejo head.prev = Weak al nuevo
            old_head.borrow_mut().prev = Some(Rc::downgrade(&new_node));
        }
        None => {
            // lista estaba vacía — tail = Weak al nuevo
            self.tail = Some(Rc::downgrade(&new_node));
        }
    }

    self.head = Some(new_node);
    self.len += 1;
}
```

`Rc::downgrade` crea un `Weak` desde un `Rc`.  El nuevo nodo se convierte en
el `head` (owned por `self.head`), y el viejo head apunta al nuevo con `Weak`.

#### push_back

```rust
pub fn push_back(&mut self, value: T) {
    let new_node = Rc::new(RefCell::new(DNode {
        data: value,
        prev: self.tail.clone(),   // Weak al viejo tail
        next: None,
    }));

    match self.tail.take().and_then(|w| w.upgrade()) {
        Some(old_tail) => {
            old_tail.borrow_mut().next = Some(Rc::clone(&new_node));
        }
        None => {
            self.head = Some(Rc::clone(&new_node));
        }
    }

    self.tail = Some(Rc::downgrade(&new_node));
    self.len += 1;
}
```

`Weak::upgrade()` intenta obtener un `Rc` desde el `Weak`.  Retorna `None`
si el valor ya fue destruido — esto no debería pasar si nuestros invariantes
son correctos, pero `Weak` nos obliga a manejarlo.

#### pop_front

```rust
pub fn pop_front(&mut self) -> Option<T> {
    self.head.take().map(|old_head| {
        match old_head.borrow_mut().next.take() {
            Some(new_head) => {
                new_head.borrow_mut().prev = None;
                self.head = Some(new_head);
            }
            None => {
                self.tail = None;   // lista quedó vacía
            }
        }
        self.len -= 1;

        // Extraer el dato del Rc<RefCell<DNode>>
        Rc::try_unwrap(old_head)    // Result<RefCell<DNode>, Rc<...>>
            .ok()                    // Option<RefCell<DNode>>
            .expect("node still referenced")
            .into_inner()            // DNode
            .data                    // T
    })
}
```

`Rc::try_unwrap` consume el `Rc` y retorna el valor interior **solo si** el
refcount es 1 (único dueño).  Después de desconectar prev/next, el refcount
debe ser exactamente 1.

#### pop_back

```rust
pub fn pop_back(&mut self) -> Option<T> {
    self.tail.take().and_then(|weak_tail| {
        weak_tail.upgrade().map(|old_tail| {
            match old_tail.borrow().prev.as_ref().and_then(|w| w.upgrade()) {
                Some(new_tail) => {
                    new_tail.borrow_mut().next = None;
                    self.tail = Some(Rc::downgrade(&new_tail));
                }
                None => {
                    self.head = None;
                }
            }
            self.len -= 1;

            Rc::try_unwrap(old_tail)
                .ok()
                .expect("node still referenced")
                .into_inner()
                .data
        })
    })
}
```

`pop_back` es $O(1)$ — el `Weak` en `tail` da acceso directo al último nodo,
y `prev` da acceso al penúltimo.

#### peek

```rust
pub fn peek_front(&self) -> Option<std::cell::Ref<'_, T>> {
    self.head.as_ref().map(|node| {
        std::cell::Ref::map(node.borrow(), |n| &n.data)
    })
}
```

`RefCell::borrow()` retorna un `Ref<DNode<T>>`, no `&DNode<T>`.  `Ref::map`
transforma el `Ref` para que apunte solo al campo `data`.  El usuario recibe
un `Ref<T>`, no un `&T` — otra diferencia con la API de raw pointers.

#### Display

```rust
impl<T: fmt::Display> fmt::Display for DList<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[")?;
        let mut cur = self.head.clone();
        let mut first = true;
        while let Some(node) = cur {
            if !first { write!(f, ", ")?; }
            write!(f, "{}", node.borrow().data)?;
            cur = node.borrow().next.clone();
            first = false;
        }
        write!(f, "]")
    }
}
```

### Costo del Rc<RefCell>

Cada operación paga un precio por la seguridad runtime:

| Operación | Costo oculto |
|-----------|-------------|
| `Rc::new` | Alloc heap + inicializar 2 contadores (strong, weak) |
| `Rc::clone` | Incrementar strong count (atómico en `Arc`, no-atómico en `Rc`) |
| `Rc::drop` | Decrementar count, liberar si llega a 0 |
| `Rc::downgrade` | Incrementar weak count |
| `Weak::upgrade` | Verificar strong count > 0, incrementar si sí |
| `RefCell::borrow` | Verificar en runtime que no hay borrow mutable activo |
| `RefCell::borrow_mut` | Verificar que no hay ningún borrow activo |
| `Rc::try_unwrap` | Verificar refcount == 1 |

Memoria por nodo:

```
Raw pointer:   data + prev (8) + next (8) = data + 16 bytes
Rc<RefCell>:   data + prev (8+8+8) + next (8+8+8)
               + Rc overhead (strong_count + weak_count = 16)
               + RefCell overhead (borrow_flag = 8)
               ≈ data + 72 bytes
```

Para un `int` (4 bytes), el nodo con `Rc<RefCell>` usa ~76 bytes vs ~24 bytes
con raw pointers — más de 3× la memoria.

---

## Estrategia 2: raw pointers unsafe

Exactamente lo que haríamos en C, pero dentro de bloques `unsafe`:

```rust
use std::ptr;
use std::marker::PhantomData;

struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}

pub struct DList<T> {
    head: *mut DNode<T>,
    tail: *mut DNode<T>,
    len: usize,
}
```

### Implementación

```rust
impl<T> DList<T> {
    pub fn new() -> Self {
        DList {
            head: ptr::null_mut(),
            tail: ptr::null_mut(),
            len: 0,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_null()
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn push_front(&mut self, value: T) {
        let node = Box::into_raw(Box::new(DNode {
            data: value,
            prev: ptr::null_mut(),
            next: self.head,
        }));

        if !self.head.is_null() {
            unsafe { (*self.head).prev = node; }
        } else {
            self.tail = node;
        }

        self.head = node;
        self.len += 1;
    }

    pub fn push_back(&mut self, value: T) {
        let node = Box::into_raw(Box::new(DNode {
            data: value,
            prev: self.tail,
            next: ptr::null_mut(),
        }));

        if !self.tail.is_null() {
            unsafe { (*self.tail).next = node; }
        } else {
            self.head = node;
        }

        self.tail = node;
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let old = Box::from_raw(self.head);
            self.head = old.next;
            if !self.head.is_null() {
                (*self.head).prev = ptr::null_mut();
            } else {
                self.tail = ptr::null_mut();
            }
            self.len -= 1;
            Some(old.data)
        }
    }

    pub fn pop_back(&mut self) -> Option<T> {
        if self.tail.is_null() {
            return None;
        }
        unsafe {
            let old = Box::from_raw(self.tail);
            self.tail = old.prev;
            if !self.tail.is_null() {
                (*self.tail).next = ptr::null_mut();
            } else {
                self.head = ptr::null_mut();
            }
            self.len -= 1;
            Some(old.data)
        }
    }

    pub fn peek_front(&self) -> Option<&T> {
        if self.head.is_null() {
            None
        } else {
            unsafe { Some(&(*self.head).data) }
        }
    }

    pub fn peek_back(&self) -> Option<&T> {
        if self.tail.is_null() {
            None
        } else {
            unsafe { Some(&(*self.tail).data) }
        }
    }
}
```

### Drop

```rust
impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        let mut current = self.head;
        while !current.is_null() {
            unsafe {
                let next = (*current).next;
                let _ = Box::from_raw(current);
                current = next;
            }
        }
    }
}
```

### remove_node — $O(1)$

```rust
impl<T> DList<T> {
    /// SAFETY: node debe ser un puntero válido a un nodo de esta lista
    pub unsafe fn remove_node(&mut self, node: *mut DNode<T>) -> T {
        let prev = (*node).prev;
        let next = (*node).next;

        if !prev.is_null() {
            (*prev).next = next;
        } else {
            self.head = next;
        }

        if !next.is_null() {
            (*next).prev = prev;
        } else {
            self.tail = prev;
        }

        self.len -= 1;
        Box::from_raw(node).data
    }
}
```

Idéntico al C de T02, pero con `Box::from_raw` en vez de `free`.

### Iteradores

```rust
pub struct Iter<'a, T> {
    current: *const DNode<T>,
    _marker: PhantomData<&'a T>,
}

impl<T> DList<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        Iter { current: self.head, _marker: PhantomData }
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current.is_null() {
            None
        } else {
            unsafe {
                let node = &*self.current;
                self.current = node.next;
                Some(&node.data)
            }
        }
    }
}
```

#### DoubleEndedIterator

Con una lista doble, podemos implementar iteración desde ambos extremos:

```rust
pub struct IterBoth<'a, T> {
    front: *const DNode<T>,
    back: *const DNode<T>,
    remaining: usize,
    _marker: PhantomData<&'a T>,
}

impl<T> DList<T> {
    pub fn iter_both(&self) -> IterBoth<'_, T> {
        IterBoth {
            front: self.head,
            back: self.tail,
            remaining: self.len,
            _marker: PhantomData,
        }
    }
}

impl<'a, T> Iterator for IterBoth<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.remaining == 0 {
            return None;
        }
        unsafe {
            let node = &*self.front;
            self.front = node.next;
            self.remaining -= 1;
            Some(&node.data)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.remaining, Some(self.remaining))
    }
}

impl<'a, T> DoubleEndedIterator for IterBoth<'a, T> {
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.remaining == 0 {
            return None;
        }
        unsafe {
            let node = &*self.back;
            self.back = node.prev;
            self.remaining -= 1;
            Some(&node.data)
        }
    }
}
```

`DoubleEndedIterator` permite `.rev()`:

```rust
let list = ...; // [10, 20, 30]

// Forward
for val in list.iter_both() { print!("{val} "); }      // 10 20 30

// Backward
for val in list.iter_both().rev() { print!("{val} "); } // 30 20 10
```

### Comparación con la versión Rc<RefCell>

| Aspecto | `Rc<RefCell>` | `*mut` unsafe |
|---------|-------------|---------------|
| Líneas de push_front | ~15 | ~10 |
| Líneas de pop_front | ~15 + `try_unwrap` | ~10 |
| peek retorna | `Option<Ref<T>>` | `Option<&T>` |
| Errores en runtime | `borrow_mut` panic si doble borrow | UB silencioso |
| Memoria por nodo | ~76 bytes (int) | ~24 bytes (int) |
| `Drop` | Automático (Rc) | Manual requerido |
| Ciclos | Leak si se olvida Weak | No aplica (no hay ownership) |
| Verificación | Runtime (RefCell) | Programador |

---

## Estrategia 3: LinkedList<T> de stdlib

Rust incluye `std::collections::LinkedList<T>` — una lista doblemente enlazada
genérica, implementada internamente con unsafe raw pointers:

```rust
use std::collections::LinkedList;

fn main() {
    let mut list = LinkedList::new();

    // push/pop en ambos extremos — O(1)
    list.push_back(10);
    list.push_back(20);
    list.push_back(30);
    list.push_front(5);

    println!("{:?}", list);                    // [5, 10, 20, 30]

    println!("front: {:?}", list.front());     // Some(5)
    println!("back:  {:?}", list.back());      // Some(30)

    println!("pop_front: {:?}", list.pop_front());  // Some(5)
    println!("pop_back:  {:?}", list.pop_back());   // Some(30)

    println!("len: {}", list.len());           // 2
    println!("{:?}", list);                    // [10, 20]
}
```

### API principal

| Método | Descripción | Complejidad |
|--------|-------------|-------------|
| `push_front(val)` | Insertar al frente | $O(1)$ |
| `push_back(val)` | Insertar al final | $O(1)$ |
| `pop_front()` | Extraer del frente | $O(1)$ |
| `pop_back()` | Extraer del final | $O(1)$ |
| `front()` / `back()` | Peek extremos | $O(1)$ |
| `len()` | Longitud | $O(1)$ |
| `is_empty()` | Verificar vacío | $O(1)$ |
| `contains(&val)` | Buscar valor | $O(n)$ |
| `iter()` / `iter_mut()` | Iteradores | — |
| `clear()` | Vaciar | $O(n)$ |
| `append(&mut other)` | Concatenar | $O(1)$ |
| `split_off(at)` | Dividir en posición | $O(\min(at, n-at))$ |

### Iteración bidireccional

`LinkedList` implementa `DoubleEndedIterator`:

```rust
let list: LinkedList<i32> = [10, 20, 30, 40].into_iter().collect();

// Forward
for val in &list {
    print!("{val} ");           // 10 20 30 40
}

// Backward
for val in list.iter().rev() {
    print!("{val} ");           // 40 30 20 10
}
```

### CursorMut (nightly)

La limitación de `LinkedList` es que no expone punteros a nodos para
inserción/eliminación $O(1)$ en medio.  La API `CursorMut` (inestable,
requiere `#![feature(linked_list_cursors)]`) resuelve esto:

```rust
#![feature(linked_list_cursors)]

let mut list: LinkedList<i32> = [10, 20, 30].into_iter().collect();
let mut cursor = list.cursor_front_mut();

cursor.move_next();                // avanzar al 20
cursor.insert_before(15);         // insertar 15 antes del 20
cursor.insert_after(25);          // insertar 25 después del 20

// list: [10, 15, 20, 25, 30]
```

Sin `CursorMut`, insertar en medio requiere `split_off` + `push` + `append` —
$O(n)$ por el split.

### Por qué LinkedList es poco usada

La documentación de Rust dice explícitamente:

> *It is almost always better to use Vec or VecDeque instead of LinkedList.*

Razones:
1. `Vec` y `VecDeque` tienen localidad de caché (datos contiguos).
2. La mayoría de operaciones "en medio" son infrecuentes en la práctica.
3. `LinkedList` sin cursores no ofrece ventaja sobre `VecDeque` para la mayoría
   de patrones de uso.
4. El overhead de memoria por nodo (2 punteros + alloc por nodo) es
   significativo.

Casos donde `LinkedList` **sí** se justifica:
- `append` (concatenar) en $O(1)$ — `Vec` requiere $O(n)$.
- `split_off` eficiente.
- Cuando se usan cursores para inserción/eliminación masiva en medio.
- Garantía de que las direcciones de los elementos no cambian (no hay
  reallocación).

---

## Comparación de las tres estrategias

| Aspecto | `Rc<RefCell>` | `*mut` unsafe | `LinkedList<T>` |
|---------|-------------|---------------|-----------------|
| Safety | Safe (runtime checks) | Unsafe | Safe (API) |
| Memoria por nodo (int) | ~76 bytes | ~24 bytes | ~24 bytes |
| push_front/back | $O(1)$ + overhead Rc | $O(1)$ | $O(1)$ |
| pop_front/back | $O(1)$ + try_unwrap | $O(1)$ | $O(1)$ |
| Eliminar nodo por puntero | Complejo (Weak) | $O(1)$ directo | Solo con CursorMut (nightly) |
| `Drop` | Automático | Manual obligatorio | Automático |
| Debug | Panic en doble borrow | UB silencioso | Sin problemas |
| Ergonomía | Baja (verbose) | Media | Alta |
| Producción | Raro | Bibliotecas de bajo nivel | Raro (VecDeque preferido) |
| Aprendizaje | Excelente para entender Rc/RefCell | Excelente para entender unsafe | Poco que aprender |

### Recomendación práctica

```
¿Necesitas una lista doblemente enlazada?
│
├─ ¿Estás aprendiendo Rust?
│   ├─ Ownership y RefCell → Rc<RefCell>
│   └─ Unsafe y raw pointers → *mut
│
├─ ¿Es código de producción?
│   ├─ ¿Necesitas append O(1)? → LinkedList<T>
│   ├─ ¿Necesitas cursores? → LinkedList<T> (nightly) o crate externo
│   └─ ¿Otro caso? → VecDeque<T> (probablemente no necesitas lista)
│
└─ ¿Es una biblioteca de bajo nivel / FFI?
    └─ *mut raw pointers
```

---

## Programa completo: las tres implementaciones

```rust
use std::collections::LinkedList;

fn main() {
    // ── Estrategia 1: Rc<RefCell> ──
    println!("=== Rc<RefCell> ===");
    let mut rc_list = rc_refcell::DList::new();
    rc_list.push_back(10);
    rc_list.push_back(20);
    rc_list.push_back(30);
    rc_list.push_front(5);
    println!("{rc_list}");                             // [5, 10, 20, 30]
    println!("pop_front: {:?}", rc_list.pop_front());  // Some(5)
    println!("pop_back:  {:?}", rc_list.pop_back());   // Some(30)
    println!("{rc_list}");                             // [10, 20]

    // ── Estrategia 2: raw pointers ──
    println!("\n=== Raw pointers ===");
    let mut raw_list = raw_ptr::DList::new();
    raw_list.push_back(10);
    raw_list.push_back(20);
    raw_list.push_back(30);
    raw_list.push_front(5);
    println!("{raw_list}");                              // [5, 10, 20, 30]
    println!("pop_front: {:?}", raw_list.pop_front());   // Some(5)
    println!("pop_back:  {:?}", raw_list.pop_back());    // Some(30)
    println!("{raw_list}");                              // [10, 20]

    // ── Estrategia 3: stdlib LinkedList ──
    println!("\n=== LinkedList<T> ===");
    let mut std_list = LinkedList::new();
    std_list.push_back(10);
    std_list.push_back(20);
    std_list.push_back(30);
    std_list.push_front(5);
    println!("{std_list:?}");                              // [5, 10, 20, 30]
    println!("pop_front: {:?}", std_list.pop_front());     // Some(5)
    println!("pop_back:  {:?}", std_list.pop_back());      // Some(30)
    println!("{std_list:?}");                              // [10, 20]

    // ── LinkedList: append O(1) ──
    let mut a: LinkedList<i32> = [1, 2, 3].into_iter().collect();
    let mut b: LinkedList<i32> = [4, 5, 6].into_iter().collect();
    a.append(&mut b);
    println!("\nappend: {a:?}");    // [1, 2, 3, 4, 5, 6]
    println!("b empty: {}", b.is_empty());  // true

    // ── LinkedList: DoubleEndedIterator ──
    let list: LinkedList<i32> = [10, 20, 30].into_iter().collect();
    let rev: Vec<&i32> = list.iter().rev().collect();
    println!("reversed: {rev:?}");  // [30, 20, 10]
}

mod rc_refcell {
    use std::rc::{Rc, Weak};
    use std::cell::RefCell;
    use std::fmt;

    type StrongLink<T> = Option<Rc<RefCell<DNode<T>>>>;
    type WeakLink<T> = Option<Weak<RefCell<DNode<T>>>>;

    struct DNode<T> {
        data: T,
        prev: WeakLink<T>,
        next: StrongLink<T>,
    }

    pub struct DList<T> {
        head: StrongLink<T>,
        tail: WeakLink<T>,
        len: usize,
    }

    impl<T> DList<T> {
        pub fn new() -> Self {
            DList { head: None, tail: None, len: 0 }
        }

        pub fn push_front(&mut self, value: T) {
            let new_node = Rc::new(RefCell::new(DNode {
                data: value,
                prev: None,
                next: self.head.take(),
            }));
            match new_node.borrow().next.as_ref() {
                Some(old_head) => {
                    old_head.borrow_mut().prev = Some(Rc::downgrade(&new_node));
                }
                None => {
                    self.tail = Some(Rc::downgrade(&new_node));
                }
            }
            self.head = Some(new_node);
            self.len += 1;
        }

        pub fn push_back(&mut self, value: T) {
            let new_node = Rc::new(RefCell::new(DNode {
                data: value,
                prev: self.tail.clone(),
                next: None,
            }));
            match self.tail.take().and_then(|w| w.upgrade()) {
                Some(old_tail) => {
                    old_tail.borrow_mut().next = Some(Rc::clone(&new_node));
                }
                None => {
                    self.head = Some(Rc::clone(&new_node));
                }
            }
            self.tail = Some(Rc::downgrade(&new_node));
            self.len += 1;
        }

        pub fn pop_front(&mut self) -> Option<T> {
            self.head.take().map(|old_head| {
                match old_head.borrow_mut().next.take() {
                    Some(new_head) => {
                        new_head.borrow_mut().prev = None;
                        self.head = Some(new_head);
                    }
                    None => { self.tail = None; }
                }
                self.len -= 1;
                Rc::try_unwrap(old_head).ok().expect("refs").into_inner().data
            })
        }

        pub fn pop_back(&mut self) -> Option<T> {
            self.tail.take().and_then(|weak| {
                weak.upgrade().map(|old_tail| {
                    match old_tail.borrow().prev.as_ref().and_then(|w| w.upgrade()) {
                        Some(new_tail) => {
                            new_tail.borrow_mut().next = None;
                            self.tail = Some(Rc::downgrade(&new_tail));
                        }
                        None => { self.head = None; }
                    }
                    self.len -= 1;
                    Rc::try_unwrap(old_tail).ok().expect("refs").into_inner().data
                })
            })
        }
    }

    impl<T: fmt::Display> fmt::Display for DList<T> {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            write!(f, "[")?;
            let mut cur = self.head.clone();
            let mut first = true;
            while let Some(node) = cur {
                if !first { write!(f, ", ")?; }
                write!(f, "{}", node.borrow().data)?;
                cur = node.borrow().next.clone();
                first = false;
            }
            write!(f, "]")
        }
    }
}

mod raw_ptr {
    use std::ptr;
    use std::fmt;
    use std::marker::PhantomData;

    struct DNode<T> {
        data: T,
        prev: *mut DNode<T>,
        next: *mut DNode<T>,
    }

    pub struct DList<T> {
        head: *mut DNode<T>,
        tail: *mut DNode<T>,
        len: usize,
    }

    impl<T> DList<T> {
        pub fn new() -> Self {
            DList { head: ptr::null_mut(), tail: ptr::null_mut(), len: 0 }
        }

        pub fn push_front(&mut self, value: T) {
            let node = Box::into_raw(Box::new(DNode {
                data: value, prev: ptr::null_mut(), next: self.head,
            }));
            if !self.head.is_null() {
                unsafe { (*self.head).prev = node; }
            } else {
                self.tail = node;
            }
            self.head = node;
            self.len += 1;
        }

        pub fn push_back(&mut self, value: T) {
            let node = Box::into_raw(Box::new(DNode {
                data: value, prev: self.tail, next: ptr::null_mut(),
            }));
            if !self.tail.is_null() {
                unsafe { (*self.tail).next = node; }
            } else {
                self.head = node;
            }
            self.tail = node;
            self.len += 1;
        }

        pub fn pop_front(&mut self) -> Option<T> {
            if self.head.is_null() { return None; }
            unsafe {
                let old = Box::from_raw(self.head);
                self.head = old.next;
                if !self.head.is_null() { (*self.head).prev = ptr::null_mut(); }
                else { self.tail = ptr::null_mut(); }
                self.len -= 1;
                Some(old.data)
            }
        }

        pub fn pop_back(&mut self) -> Option<T> {
            if self.tail.is_null() { return None; }
            unsafe {
                let old = Box::from_raw(self.tail);
                self.tail = old.prev;
                if !self.tail.is_null() { (*self.tail).next = ptr::null_mut(); }
                else { self.head = ptr::null_mut(); }
                self.len -= 1;
                Some(old.data)
            }
        }

        pub fn iter(&self) -> Iter<'_, T> {
            Iter { current: self.head, _marker: PhantomData }
        }
    }

    impl<T> Drop for DList<T> {
        fn drop(&mut self) {
            let mut cur = self.head;
            while !cur.is_null() {
                unsafe {
                    let next = (*cur).next;
                    let _ = Box::from_raw(cur);
                    cur = next;
                }
            }
        }
    }

    pub struct Iter<'a, T> {
        current: *const DNode<T>,
        _marker: PhantomData<&'a T>,
    }

    impl<'a, T> Iterator for Iter<'a, T> {
        type Item = &'a T;
        fn next(&mut self) -> Option<Self::Item> {
            if self.current.is_null() { None }
            else { unsafe {
                let node = &*self.current;
                self.current = node.next;
                Some(&node.data)
            }}
        }
    }

    impl<T: fmt::Display> fmt::Display for DList<T> {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
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
}
```

---

## Ejercicios

### Ejercicio 1 — Rc y Weak: conteos

Traza los valores de `strong_count` y `weak_count` para cada nodo durante
la construcción de la lista `[A, B]` con `Rc<RefCell>`.

<details>
<summary>Traza</summary>

```
push_back(A):
  Rc(A) creado → strong=1, weak=0
  self.head = Rc::clone → strong=2, weak=0
  self.tail = Rc::downgrade → strong=2, weak=1
  Pero self.head.take() mueve, no clona → strong=1, weak=1

push_back(B):
  Rc(B) creado → strong_B=1, weak_B=0
  B.prev = self.tail.clone() → weak copia del Weak(A) → weak_A=2
  tail.upgrade() → strong_A temporalmente +1
  A.next = Rc::clone(B) → strong_B=2
  upgrade drop → strong_A vuelve a 1
  self.tail = downgrade(B) → weak_B=1

Estado final:
  A: strong=1 (en head), weak=1 (en B.prev)
  B: strong=1 (en A.next), weak=1 (en self.tail)
```

Cada nodo tiene exactamente 1 strong (de su `next` anterior o de `head`) y
1 weak (de `prev` o `tail`).  Sin ciclos de strong → Drop funciona.
</details>

### Ejercicio 2 — Ciclo con Rc (sin Weak)

Implementa una versión incorrecta donde `prev` sea `Rc` (no `Weak`).
Crea una lista de 2 nodos y muestra que `Rc::strong_count` nunca llega a 0
cuando la lista se destruye.

<details>
<summary>Por qué hay leak</summary>

```rust
// CON Rc en prev (incorrecto):
// A.next = Rc(B)   → strong_B = 2
// B.prev = Rc(A)   → strong_A = 2

// Drop list → head deja de apuntar a A
// strong_A = 1 (B.prev todavía lo tiene)
// strong_B = 1 (A.next todavía lo tiene)
// Ninguno llega a 0 → LEAK

// CON Weak en prev (correcto):
// A.next = Rc(B)   → strong_B = 1
// B.prev = Weak(A) → strong_A no cambia = 1

// Drop list → head deja de apuntar a A
// strong_A = 0 → A se destruye → A.next se destruye
// strong_B = 0 → B se destruye
// Sin leak
```
</details>

### Ejercicio 3 — LinkedList: append y split

Crea dos `LinkedList`: `[1, 2, 3]` y `[4, 5, 6]`.  Haz `append` para obtener
`[1..6]`.  Luego haz `split_off(3)` para separar de nuevo.  ¿Cuáles son las
complejidades?

<details>
<summary>Resultado</summary>

```rust
let mut a: LinkedList<i32> = (1..=3).collect();
let mut b: LinkedList<i32> = (4..=6).collect();

a.append(&mut b);           // a=[1,2,3,4,5,6], b=[]  — O(1)

let c = a.split_off(3);     // a=[1,2,3], c=[4,5,6]   — O(min(3, 3)) = O(3)
```

`append` es $O(1)$: solo reconecta tail de `a` con head de `b`.
`split_off(at)` es $O(\min(at, n-at))$: recorre desde el extremo más cercano.
</details>

### Ejercicio 4 — DoubleEndedIterator

Implementa `DoubleEndedIterator` para la versión raw pointer.  Verifica
con `.rev()` y con iteración simultánea desde ambos extremos (toma el
primero y el último alternadamente).

<details>
<summary>Uso de next + next_back</summary>

```rust
let list = ...; // [10, 20, 30, 40]
let mut it = list.iter_both();

it.next();       // Some(&10) — desde el frente
it.next_back();  // Some(&40) — desde atrás
it.next();       // Some(&20) — sigue desde el frente
it.next_back();  // Some(&30) — sigue desde atrás
it.next();       // None — remaining == 0
```

El campo `remaining` evita que front y back se crucen y lean el mismo nodo
dos veces.
</details>

### Ejercicio 5 — Raw pointer: remove_node

Implementa `remove_node` para la versión unsafe.  Prueba eliminando:
el head, el tail, un nodo en medio, y el único nodo.  Verifica con
recorrido forward y backward.

<details>
<summary>Verificación</summary>

Los 4 casos deben dejar la lista consistente:
```
Eliminar head de [10,20,30]  → [20,30], forward y backward ok
Eliminar tail de [10,20,30]  → [10,20], forward y backward ok
Eliminar medio de [10,20,30] → [10,30], forward y backward ok
Eliminar único de [10]       → [], is_empty() == true
```

Si algún recorrido falla (ej. backward no muestra todos), hay un `prev` o
`next` inconsistente.
</details>

### Ejercicio 6 — Memoria: Rc<RefCell> vs raw

Calcula la memoria total para una lista de 1000 `i32` con cada estrategia.
Incluye todos los overheads (Rc counters, RefCell flag, punteros, padding).

<details>
<summary>Cálculo</summary>

```
Raw pointer:
  DNode: data(4) + padding(4) + prev(8) + next(8) = 24 bytes
  DList: head(8) + tail(8) + len(8) = 24 bytes
  Total: 1000 × 24 + 24 = 24,024 bytes

Rc<RefCell>:
  DNode: data(4) + padding(4) + prev(~24*) + next(~24*) = ~56 bytes
  Rc overhead: strong_count(8) + weak_count(8) = 16 bytes
  RefCell overhead: borrow_flag(8) = 8 bytes
  Total por nodo: ~80 bytes
  Total: 1000 × 80 + 24 ≈ 80,024 bytes

*Weak/Option<Rc> son ~24 bytes (8 ptr + 8 discriminant/padding)
```

La versión `Rc<RefCell>` usa ~3.3× más memoria que raw pointers.
</details>

### Ejercicio 7 — RefCell panic

Escribe código que cause un panic de `RefCell` por doble borrow mutable.
¿En qué situación práctica con listas podría ocurrir esto?

<details>
<summary>Ejemplo</summary>

```rust
let node = Rc::new(RefCell::new(DNode { data: 42, prev: None, next: None }));

let borrow1 = node.borrow_mut();   // OK — primer borrow mutable
let borrow2 = node.borrow_mut();   // PANIC: already mutably borrowed
```

En listas, esto puede pasar al recorrer: si durante el recorrido intentas
modificar un nodo que ya está borrowed por el iterador.  Con raw pointers
esto compila pero puede causar UB; con RefCell, al menos obtienes un panic
con mensaje claro.
</details>

### Ejercicio 8 — LinkedList vs VecDeque

Para cada operación, indica cuál es más eficiente y por qué:
1. 10⁶ `push_back` seguidos.
2. `append` de dos colecciones de 10⁵ elementos.
3. Iterar y sumar todos los elementos.
4. Acceder al elemento en posición 500,000.

<details>
<summary>Análisis</summary>

1. **push_back ×10⁶**: `VecDeque` gana — misma complejidad amortizada $O(1)$
   pero localidad de caché (contiguous memory vs scattered nodes).

2. **append**: `LinkedList` gana — $O(1)$ reconectar punteros vs $O(n)$
   copiar/mover elementos de VecDeque.

3. **Iterar**: `VecDeque` gana — cache-friendly sequential access.  La lista
   enlazada causa cache misses en cada `next` (nodos dispersos en el heap).

4. **Acceso posicional**: `VecDeque` gana — $O(1)$ con índice vs $O(n)$
   recorrido en lista.  VecDeque soporta `deque[500_000]`.
</details>

### Ejercicio 9 — Drop de Rc<RefCell>

¿Necesita la versión `Rc<RefCell>` un `Drop` manual como la versión raw
pointer?  ¿Qué pasa si `prev` fuera `Rc` en vez de `Weak`?

<details>
<summary>Respuesta</summary>

Con `Weak` en `prev`: **no necesita `Drop` manual**.  La cadena de ownership
es lineal (head → A → B → C vía `next`).  Cuando `DList` se destruye, `head`
pierde su `Rc`.  Si es el único strong ref a A, A se destruye, lo que destruye
su `next` (Rc a B), etc.  Cascada automática.

Con `Rc` en `prev`: **necesitaría Drop manual** o habría leak.  Los ciclos
(A.next→B, B.prev→A) mantienen strong counts > 0 indefinidamente.  Habría
que romper los ciclos manualmente en `Drop` (ej. recorrer y poner todos los
`prev` en `None`).
</details>

### Ejercicio 10 — Elegir estrategia

Para cada escenario, elige la estrategia más apropiada y justifica:
1. Biblioteca de estructuras de datos para un crate público.
2. Ejercicio de curso para entender ownership.
3. Prototipo rápido que necesita lista doble.
4. Interop con código C que pasa `*Node` entre funciones.

<details>
<summary>Respuestas</summary>

1. **Crate público → raw pointers unsafe con API safe**.  Máximo rendimiento,
   mínimo overhead, la API pública no expone unsafe.  Es lo que hace la
   stdlib.

2. **Curso de ownership → Rc<RefCell>**.  Enseña Rc, Weak, RefCell, lifetime
   dinámico, y por qué las listas dobles son difíciles en Rust.  Los errores
   son panics (claros), no UB.

3. **Prototipo rápido → `LinkedList<T>` de stdlib**.  Cero código de
   infraestructura, API conocida, funciona.  O mejor: `VecDeque<T>` si no
   necesitas append $O(1)$.

4. **FFI con C → raw pointers**.  Los raw pointers de Rust son compatibles con
   punteros C en layout.  `Rc<RefCell>` no tiene equivalente en C.
</details>
