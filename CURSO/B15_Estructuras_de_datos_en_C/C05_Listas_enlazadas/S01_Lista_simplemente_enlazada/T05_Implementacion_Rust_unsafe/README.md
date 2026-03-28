# Implementación Rust unsafe

## Por qué unsafe para listas

En T04 implementamos una lista safe con `Option<Box<Node<T>>>`.  Funciona, pero
tiene una limitación fundamental: **push_back es $O(n)$**.  No hay forma de
mantener un puntero al último nodo en safe Rust, porque `Box` impone un único
dueño — tener `head` como dueño de la cadena *y* `tail` apuntando al último
nodo crea dos caminos de acceso al mismo dato, lo que viola las reglas de
borrowing.

En C esto es trivial: un `Node *tail` adicional.  En Rust, la solución es usar
**raw pointers** — `*mut Node<T>` — que no participan en el sistema de
ownership.

¿Cuándo se justifica `unsafe` en una lista?

| Situación | Safe alcanza | Unsafe necesario |
|-----------|-------------|-----------------|
| Solo push_front/pop_front | Sí | No |
| push_back $O(1)$ | No (sin `Rc`) | Sí — tail pointer |
| Inserción/eliminación por cursor | Difícil | Sí — puntero al nodo |
| Rendimiento crítico (evitar overhead de `Option<Box>`) | No | Sí |
| Interfaz C (FFI) | No | Sí |

La regla general: **encapsular el `unsafe` dentro de métodos con interfaz safe**.
El usuario de la lista nunca escribe `unsafe` — solo el implementador.

---

## Raw pointers en Rust

### Tipos

Rust tiene dos tipos de raw pointers:

```rust
*const T    // lectura (análogo a const T* en C)
*mut T      // lectura y escritura (análogo a T* en C)
```

A diferencia de `&T` y `&mut T`:
- No tienen lifetime.
- No garantizan validez (pueden ser nulos o dangling).
- No participan en el borrow checker.
- **Crear** un raw pointer es safe; **desreferenciarlo** requiere `unsafe`.

### Operaciones básicas

```rust
// Crear desde referencia — safe
let x = 42;
let ptr: *const i32 = &x;
let mut y = 10;
let mptr: *mut i32 = &mut y;

// Crear nulo — safe
let null: *mut i32 = std::ptr::null_mut();

// Verificar nulidad — safe
if null.is_null() {
    println!("is null");
}

// Desreferenciar — UNSAFE
unsafe {
    println!("value: {}", *ptr);   // 42
    *mptr = 20;                     // modificar
}
```

### Box y raw pointers

La interacción entre `Box` y raw pointers es fundamental para listas:

```rust
// Box → raw pointer: consume el Box, el heap NO se libera
let boxed = Box::new(42);
let raw: *mut i32 = Box::into_raw(boxed);
// boxed ya no existe — raw es el único dueño del heap

// raw pointer → Box: reconstruye el Box para liberar
unsafe {
    let recovered = Box::from_raw(raw);
    // recovered se destruye al salir del scope → heap liberado
}
```

`Box::into_raw` transfiere ownership del heap al raw pointer.
`Box::from_raw` transfiere ownership de vuelta a un `Box`.

Si usas `into_raw` sin `from_raw`, el heap **se filtra** (memory leak).
Si usas `from_raw` dos veces con el mismo puntero, obtienes **double free**.

---

## Estructura con head y tail

```rust
use std::ptr;

pub struct List<T> {
    head: *mut Node<T>,
    tail: *mut Node<T>,
    len: usize,
}

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}
```

Comparación con la versión safe:

| Campo | Safe (T04) | Unsafe |
|-------|-----------|--------|
| head | `Option<Box<Node<T>>>` | `*mut Node<T>` |
| tail | No existe | `*mut Node<T>` |
| next | `Option<Box<Node<T>>>` | `*mut Node<T>` |
| Nodo vacío | `None` | `null_mut()` |

### Invariantes de seguridad

Todo `unsafe` debe documentar sus invariantes — las condiciones que el
programador **promete** mantener:

```rust
// Invariantes:
// 1. head es null si y solo si la lista está vacía
// 2. tail es null si y solo si la lista está vacía
// 3. Cada nodo fue creado con Box::into_raw(Box::new(...))
// 4. El next del último nodo es null
// 5. tail apunta al último nodo (next == null)
// 6. len == número real de nodos
// 7. Ningún nodo aparece más de una vez en la cadena
```

Violar cualquier invariante causa comportamiento indefinido (UB).

---

## Implementación completa

### new

```rust
impl<T> List<T> {
    pub fn new() -> Self {
        List {
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
}
```

Ninguna operación `unsafe` — crear un raw pointer nulo es safe.

### push_front — $O(1)$

```rust
pub fn push_front(&mut self, value: T) {
    let new_node = Box::into_raw(Box::new(Node {
        data: value,
        next: self.head,    // el nuevo nodo apunta al viejo head
    }));

    if self.head.is_null() {
        self.tail = new_node;   // lista estaba vacía → tail = nuevo
    }
    self.head = new_node;
    self.len += 1;
}
```

No hay bloque `unsafe` porque no desreferenciamos ningún raw pointer — solo
asignamos el valor del puntero.  `Box::into_raw` es safe (no desreferencia,
solo convierte).

### push_back — $O(1)$

```rust
pub fn push_back(&mut self, value: T) {
    let new_node = Box::into_raw(Box::new(Node {
        data: value,
        next: ptr::null_mut(),
    }));

    if self.tail.is_null() {
        self.head = new_node;   // lista estaba vacía
    } else {
        unsafe {
            (*self.tail).next = new_node;   // enlazar al final
        }
    }
    self.tail = new_node;
    self.len += 1;
}
```

Aquí **sí** hay `unsafe`: `(*self.tail).next` desreferencia `self.tail`.  Esto
es seguro porque nuestro invariante 5 garantiza que `tail` apunta a un nodo
válido cuando no es nulo.

Comparación con la versión safe de T04:

```
Safe:   push_back es O(n) — recorre toda la lista con &mut
Unsafe: push_back es O(1) — desreferencia tail directamente
```

### pop_front — $O(1)$

```rust
pub fn pop_front(&mut self) -> Option<T> {
    if self.head.is_null() {
        return None;
    }

    unsafe {
        let old_head = Box::from_raw(self.head);  // reclamar ownership
        self.head = old_head.next;

        if self.head.is_null() {
            self.tail = ptr::null_mut();  // lista quedó vacía
        }
        self.len -= 1;
        Some(old_head.data)
    }
}
```

`Box::from_raw` reconstruye el `Box` desde el raw pointer.  Cuando `old_head`
sale del scope, el heap se libera automáticamente — pero solo del nodo, no de
la cadena (porque `next` es un raw pointer, no un `Box`).

### pop_back — $O(n)$

```rust
pub fn pop_back(&mut self) -> Option<T> {
    if self.head.is_null() {
        return None;
    }

    unsafe {
        if self.head == self.tail {
            // Un solo nodo
            let node = Box::from_raw(self.head);
            self.head = ptr::null_mut();
            self.tail = ptr::null_mut();
            self.len -= 1;
            return Some(node.data);
        }

        // Buscar el penúltimo nodo — O(n)
        let mut current = self.head;
        while (*current).next != self.tail {
            current = (*current).next;
        }

        let old_tail = Box::from_raw(self.tail);
        (*current).next = ptr::null_mut();
        self.tail = current;
        self.len -= 1;
        Some(old_tail.data)
    }
}
```

`pop_back` sigue siendo $O(n)$ en una lista **simplemente** enlazada — incluso
con raw pointers.  Necesitamos el penúltimo nodo para actualizar `tail`, y sin
puntero `prev` hay que recorrer.  Esto solo se resuelve con lista doblemente
enlazada (C05/S02).

### peek

```rust
pub fn peek(&self) -> Option<&T> {
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
```

`peek_back` es $O(1)$ gracias al puntero `tail` — imposible en la versión safe.

### find — $O(n)$

```rust
pub fn contains(&self, value: &T) -> bool
where T: PartialEq
{
    let mut current = self.head;
    while !current.is_null() {
        unsafe {
            if &(*current).data == value {
                return true;
            }
            current = (*current).next;
        }
    }
    false
}
```

### insert_at — $O(n)$

```rust
pub fn insert_at(&mut self, pos: usize, value: T) -> bool {
    if pos > self.len {
        return false;
    }
    if pos == 0 {
        self.push_front(value);
        return true;
    }
    if pos == self.len {
        self.push_back(value);
        return true;
    }

    unsafe {
        let mut current = self.head;
        for _ in 0..pos - 1 {
            current = (*current).next;
        }

        let new_node = Box::into_raw(Box::new(Node {
            data: value,
            next: (*current).next,
        }));
        (*current).next = new_node;
        self.len += 1;
    }
    true
}
```

Delegar a `push_front`/`push_back` en los bordes mantiene los invariantes de
`head` y `tail` sin duplicar lógica.

### remove_at — $O(n)$

```rust
pub fn remove_at(&mut self, pos: usize) -> Option<T> {
    if pos >= self.len {
        return None;
    }
    if pos == 0 {
        return self.pop_front();
    }

    unsafe {
        let mut current = self.head;
        for _ in 0..pos - 1 {
            current = (*current).next;
        }

        let target = (*current).next;
        (*current).next = (*target).next;

        if (*target).next.is_null() {
            self.tail = current;   // eliminamos el último → actualizar tail
        }

        let node = Box::from_raw(target);
        self.len -= 1;
        Some(node.data)
    }
}
```

---

## Traza detallada

Secuencia: `push_back(10)`, `push_back(20)`, `push_back(30)`, `pop_front()`,
`push_front(5)`.

```
Estado inicial:
  head = null, tail = null, len = 0

push_back(10):
  new_node @ 0xA00: Node { data: 10, next: null }
  tail == null → head = 0xA00
  tail = 0xA00, len = 1

  head ──→ [10|null]
             ↑
  tail ──────┘

push_back(20):
  new_node @ 0xB00: Node { data: 20, next: null }
  tail != null → (*tail).next = 0xB00
  tail = 0xB00, len = 2

  head ──→ [10|·]──→ [20|null]
                        ↑
  tail ─────────────────┘

push_back(30):
  new_node @ 0xC00: Node { data: 30, next: null }
  (*tail).next = 0xC00
  tail = 0xC00, len = 3

  head ──→ [10|·]──→ [20|·]──→ [30|null]
                                  ↑
  tail ───────────────────────────┘

pop_front() → Some(10):
  old_head = Box::from_raw(0xA00)  ← reclamar nodo
  head = old_head.next = 0xB00
  head != null → tail no cambia
  len = 2, retorna 10
  0xA00 liberado

  head ──→ [20|·]──→ [30|null]
                        ↑
  tail ─────────────────┘

push_front(5):
  new_node @ 0xD00: Node { data: 5, next: 0xB00 }
  head != null → tail no cambia
  head = 0xD00, len = 3

  head ──→ [5|·]──→ [20|·]──→ [30|null]
                                 ↑
  tail ──────────────────────────┘
```

---

## Drop y liberación de memoria

En la versión safe, `Drop` se implementaba con `take()` para evitar recursión.
Con raw pointers, el `Drop` por defecto **no libera nada** — los raw pointers
no tienen destructor automático.  Sin `Drop` manual, toda la memoria se filtra:

```rust
impl<T> Drop for List<T> {
    fn drop(&mut self) {
        let mut current = self.head;
        while !current.is_null() {
            unsafe {
                let next = (*current).next;
                let _ = Box::from_raw(current);   // reclamar y destruir
                current = next;
            }
        }
        // head y tail quedan dangling, pero List se destruye aquí → no importa
    }
}
```

`Box::from_raw` reconstruye el `Box` y `let _` lo descarta inmediatamente,
invocando `Drop` y liberando el heap.  Es crucial leer `next` **antes** de
liberar el nodo.

---

## Iterador

```rust
pub struct Iter<'a, T> {
    current: *const Node<T>,
    _marker: std::marker::PhantomData<&'a T>,
}

impl<T> List<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        Iter {
            current: self.head,
            _marker: std::marker::PhantomData,
        }
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

`PhantomData<&'a T>` indica al compilador que el iterador **conceptualmente**
toma prestado de `List<T>` con lifetime `'a`, aunque internamente usa raw
pointers sin lifetime.  Sin `PhantomData`, el compilador no sabría que el
iterador no puede sobrevivir a la lista:

```rust
let iter;
{
    let list = List::new();
    iter = list.iter();
}   // list se destruye
// Sin PhantomData: iter compila pero apunta a memoria liberada
// Con PhantomData: error de compilación — lifetime insuficiente
```

### IterMut

```rust
pub struct IterMut<'a, T> {
    current: *mut Node<T>,
    _marker: std::marker::PhantomData<&'a mut T>,
}

impl<T> List<T> {
    pub fn iter_mut(&mut self) -> IterMut<'_, T> {
        IterMut {
            current: self.head,
            _marker: std::marker::PhantomData,
        }
    }
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current.is_null() {
            None
        } else {
            unsafe {
                let node = &mut *self.current;
                self.current = node.next;
                Some(&mut node.data)
            }
        }
    }
}
```

La versión safe necesitaba `take()` para evitar aliasing de `&mut`.  Con raw
pointers, simplemente avanzamos el puntero — el borrow checker no interviene
dentro de `unsafe`.

---

## Unsafe correcto: la interfaz safe

El objetivo de `unsafe` en Rust no es escribir "C dentro de Rust".  Es crear
una **abstracción safe** cuya implementación interna usa operaciones que el
compilador no puede verificar:

```
┌──────────────────────────────────┐
│         API pública (safe)       │
│  push_front, pop_front, iter...  │
│  El usuario nunca escribe unsafe │
├──────────────────────────────────┤
│      Implementación (unsafe)     │
│  *mut Node<T>, Box::from_raw...  │
│  El implementador promete los    │
│  invariantes                     │
└──────────────────────────────────┘
```

```rust
// El usuario de la lista escribe:
fn main() {
    let mut list = List::new();
    list.push_back(10);
    list.push_back(20);

    for val in list.iter() {
        println!("{val}");
    }
    // Drop automático — sin leaks, sin unsafe visible
}
```

Ninguna línea del `main` contiene `unsafe`.  Toda la complejidad está
encapsulada dentro de `List`.

---

## Errores comunes con unsafe

### 1. Use-after-free

```rust
// MAL: liberar un nodo y luego acceder a su next
unsafe {
    let _ = Box::from_raw(current);
    current = (*current).next;   // UB — current ya fue liberado
}

// BIEN: leer next antes de liberar
unsafe {
    let next = (*current).next;
    let _ = Box::from_raw(current);
    current = next;
}
```

### 2. Double free

```rust
// MAL: reclamar el mismo puntero dos veces
unsafe {
    let a = Box::from_raw(ptr);
    let b = Box::from_raw(ptr);   // double free cuando a y b se destruyan
}
```

### 3. Olvidar actualizar tail

```rust
// MAL: pop_front sin actualizar tail cuando queda vacía
pub fn pop_front_broken(&mut self) -> Option<T> {
    unsafe {
        let old = Box::from_raw(self.head);
        self.head = old.next;
        // Si head es null ahora, tail sigue apuntando al nodo liberado
        // → dangling pointer en tail
        self.len -= 1;
        Some(old.data)
    }
}
```

### 4. Leak por falta de Drop

```rust
// Si no implementas Drop, al hacer:
{
    let mut list = List::new();
    list.push_back(1);
    list.push_back(2);
}   // list se destruye, pero los nodos en el heap NUNCA se liberan
```

### 5. Crear Box::from_raw con puntero inválido

```rust
// MAL: from_raw con puntero que no vino de into_raw
let x = 42;
let ptr = &x as *const i32 as *mut i32;
unsafe {
    let _ = Box::from_raw(ptr);   // UB — ptr apunta al stack, no al heap
}
```

---

## Comparación: safe vs unsafe vs C

| Aspecto | Rust safe (T04) | Rust unsafe | C |
|---------|----------------|-------------|---|
| push_front | $O(1)$ con `take()` | $O(1)$ directo | $O(1)$ directo |
| push_back | $O(n)$ recorrido | $O(1)$ con tail | $O(1)$ con tail |
| peek_back | $O(n)$ | $O(1)$ con tail | $O(1)$ con tail |
| pop_back | $O(n)$ | $O(n)$ | $O(n)$ |
| Memory leak | Imposible | Posible (olvidar `Drop`) | Posible (olvidar `free`) |
| Use-after-free | Imposible | Posible dentro de `unsafe` | Posible |
| Double free | Imposible | Posible con `from_raw` | Posible con `free` |
| Dangling pointer | Imposible | Posible (tail sin actualizar) | Posible |
| Verbosidad | Alta (`Option`, `take`, `as_ref`) | Media (raw pointers) | Baja |
| Seguridad garantizada | Por compilador | Por programador | Por programador |
| Genéricos | `<T>` nativo | `<T>` nativo | `void *` con casts |

La diferencia clave entre Rust unsafe y C: en Rust, el `unsafe` está
**delimitado** y la API pública es safe.  En C, toda la interfaz es "unsafe"
implícitamente.

---

## Cuándo usar cada enfoque

### Usa la versión safe cuando

- Solo necesitas push_front/pop_front (stack o lista simple).
- La lista es corta o push_back infrecuente.
- Priorizas seguridad sobre rendimiento.
- El código no es crítico en performance.

### Usa la versión unsafe cuando

- Necesitas push_back $O(1)$ (colas, buffers).
- Implementas una estructura de datos de biblioteca.
- FFI con código C.
- Benchmarks demuestran que la versión safe es el cuello de botella.

### Usa VecDeque cuando

- No tienes un motivo específico para una lista enlazada.
- `VecDeque` cubre el 95% de los casos con mejor rendimiento (caché).
- No necesitas inserción $O(1)$ en medio con un cursor.

---

## Programa completo

```rust
use std::ptr;
use std::marker::PhantomData;

// ──────── Estructura ────────

pub struct List<T> {
    head: *mut Node<T>,
    tail: *mut Node<T>,
    len: usize,
}

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

// ──────── Operaciones básicas ────────

impl<T> List<T> {
    pub fn new() -> Self {
        List {
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
        let new_node = Box::into_raw(Box::new(Node {
            data: value,
            next: self.head,
        }));
        if self.head.is_null() {
            self.tail = new_node;
        }
        self.head = new_node;
        self.len += 1;
    }

    pub fn push_back(&mut self, value: T) {
        let new_node = Box::into_raw(Box::new(Node {
            data: value,
            next: ptr::null_mut(),
        }));
        if self.tail.is_null() {
            self.head = new_node;
        } else {
            unsafe { (*self.tail).next = new_node; }
        }
        self.tail = new_node;
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let old_head = Box::from_raw(self.head);
            self.head = old_head.next;
            if self.head.is_null() {
                self.tail = ptr::null_mut();
            }
            self.len -= 1;
            Some(old_head.data)
        }
    }

    pub fn peek(&self) -> Option<&T> {
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

// ──────── Drop ────────

impl<T> Drop for List<T> {
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

// ──────── Iterador ────────

pub struct Iter<'a, T> {
    current: *const Node<T>,
    _marker: PhantomData<&'a T>,
}

impl<T> List<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        Iter {
            current: self.head,
            _marker: PhantomData,
        }
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

// ──────── Display ────────

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

// ──────── Main ────────

fn main() {
    let mut list = List::new();

    // push_back O(1) — imposible en safe
    list.push_back(10);
    list.push_back(20);
    list.push_back(30);
    println!("After push_back: {list}");       // [10, 20, 30]

    // push_front O(1)
    list.push_front(5);
    println!("After push_front: {list}");      // [5, 10, 20, 30]

    // peek O(1) en ambos extremos
    println!("front: {:?}", list.peek());      // Some(5)
    println!("back:  {:?}", list.peek_back()); // Some(30)

    // pop_front
    println!("pop: {:?}", list.pop_front());   // Some(5)
    println!("After pop: {list}");             // [10, 20, 30]

    // len
    println!("len: {}", list.len());           // 3

    // iter + sum
    let sum: i32 = list.iter().sum();
    println!("sum: {sum}");                    // 60

    // Lista se destruye aquí — Drop libera los 3 nodos
    println!("Done — no leaks");
}
```

Salida:

```
After push_back: [10, 20, 30]
After push_front: [5, 10, 20, 30]
front: Some(5)
back:  Some(30)
pop: Some(5)
After pop: [10, 20, 30]
len: 3
sum: 60
Done — no leaks
```

---

## Ejercicios

### Ejercicio 1 — push_back vs safe

Implementa la `List<T>` unsafe con `push_back`.  Inserta 5 valores con
`push_back` y 2 con `push_front`.  Verifica con `peek` y `peek_back` que
ambos extremos son correctos.

<details>
<summary>Verificación</summary>

```rust
let mut list = List::new();
list.push_back(10);
list.push_back(20);
list.push_back(30);
list.push_back(40);
list.push_back(50);
list.push_front(5);
list.push_front(1);
// Lista: [1, 5, 10, 20, 30, 40, 50]
assert_eq!(list.peek(), Some(&1));
assert_eq!(list.peek_back(), Some(&50));
assert_eq!(list.len(), 7);
```

La diferencia clave: en safe Rust, `push_back` de 5 valores sería
$5 \times O(n) = O(n^2)$ total.  Aquí es $5 \times O(1) = O(5)$.
</details>

### Ejercicio 2 — Traza de pop_front con tail

Traza manualmente `pop_front()` sobre la lista `[10, 20]` (2 nodos).
Muestra `head`, `tail`, y `len` después de cada pop.  ¿Qué pasa con `tail`
cuando la lista queda vacía?

<details>
<summary>Traza</summary>

```
Estado: head=0xA00, tail=0xB00, len=2
  0xA00: [10|0xB00]   0xB00: [20|null]

pop_front() → Some(10):
  Box::from_raw(0xA00) → reclamar [10]
  head = 0xB00 (old_head.next)
  head != null → tail no cambia
  0xA00 liberado, len=1
  head=0xB00, tail=0xB00

pop_front() → Some(20):
  Box::from_raw(0xB00) → reclamar [20]
  head = null (old_head.next)
  head == null → tail = null    ← CRÍTICO
  0xB00 liberado, len=0
  head=null, tail=null
```

Si no actualizamos `tail` a null cuando la lista queda vacía, `tail`
apuntaría a `0xB00` — memoria ya liberada (dangling pointer).
</details>

### Ejercicio 3 — Drop manual

Comenta el `impl Drop` y crea una lista con 3 nodos.  Ejecuta con Miri
(`cargo +nightly miri run`) para detectar el memory leak.  Luego restaura
el `Drop` y verifica que Miri pasa limpio.

<details>
<summary>Resultado</summary>

Sin `Drop`, Miri reporta:

```
error: memory leaked: alloc1234, alloc1235, alloc1236
```

Tres allocations (los 3 nodos) nunca liberadas.  Con `Drop` restaurado,
Miri no reporta errores.

Miri es la herramienta oficial de Rust para detectar UB y leaks en código
unsafe.  Comandos:
```bash
rustup component add miri --toolchain nightly
cargo +nightly miri run
```
</details>

### Ejercicio 4 — pop_back

Implementa `pop_back()` para la lista unsafe.  Recuerda: en lista simplemente
enlazada, necesitas encontrar el penúltimo nodo ($O(n)$).  Prueba con listas
de 1, 2 y 3 nodos.

<details>
<summary>Caso difícil</summary>

El caso más delicado es la lista de 1 nodo: `head == tail`, así que no hay
penúltimo.  Hay que tratar este caso por separado:

```rust
if self.head == self.tail {
    let node = Box::from_raw(self.head);
    self.head = ptr::null_mut();
    self.tail = ptr::null_mut();
    self.len -= 1;
    return Some(node.data);
}
```

Para 2+ nodos, recorrer hasta que `(*current).next == self.tail`.
</details>

### Ejercicio 5 — Box::into_raw y Box::from_raw

Explica qué ocurre en cada paso:

```rust
let b = Box::new(42);           // paso 1
let raw = Box::into_raw(b);     // paso 2
// b ya no existe aquí
unsafe {
    *raw = 100;                  // paso 3
    let b2 = Box::from_raw(raw); // paso 4
    println!("{}", b2);          // paso 5
}                                // paso 6
```

<details>
<summary>Análisis</summary>

1. `Box::new(42)`: alloca 4 bytes en el heap, escribe 42, `b` es dueño.
2. `Box::into_raw(b)`: consume `b`, retorna `*mut i32` al heap. El heap
   **no se libera** — ownership pasa al raw pointer.
3. `*raw = 100`: escribe 100 en el heap (desreferenciar raw pointer, unsafe).
4. `Box::from_raw(raw)`: reconstruye un `Box<i32>` desde el raw pointer.
   Ahora `b2` es dueño del heap.
5. `println!`: imprime 100.
6. Fin del scope: `b2` se destruye → `Drop` libera el heap.

Si omites el paso 4, el heap con el 100 se filtra permanentemente.
</details>

### Ejercicio 6 — PhantomData

Elimina el campo `_marker: PhantomData<&'a T>` del iterador.  ¿Compila?
¿Qué problema introduce?  Escribe un ejemplo donde se manifieste el bug.

<details>
<summary>Respuesta</summary>

Sin `PhantomData`, el iterador no tiene relación de lifetime con la lista.
Esto compila:

```rust
let iter;
{
    let mut list = List::new();
    list.push_back(42);
    iter = list.iter();
}   // list se destruye, nodos liberados
for val in iter {
    println!("{val}");   // use-after-free — UB silencioso
}
```

Con `PhantomData<&'a T>`, el compilador rechaza este código:

```
error[E0597]: `list` does not live long enough
```

`PhantomData` es el mecanismo para que raw pointers participen en el
sistema de lifetimes sin ocupar memoria (es zero-sized type).
</details>

### Ejercicio 7 — insert_at

Implementa `insert_at(pos, value)` para la lista unsafe.  Maneja los casos:
`pos == 0` (push_front), `pos == len` (push_back), y posición en medio.

<details>
<summary>Pista: caso medio</summary>

Para insertar en posición `pos`, recorre `pos - 1` nodos para llegar al nodo
anterior.  Luego:

```rust
let new_node = Box::into_raw(Box::new(Node {
    data: value,
    next: (*prev).next,     // nuevo apunta al que estaba en pos
}));
(*prev).next = new_node;    // prev ahora apunta al nuevo
```

No necesitas actualizar `tail` porque no estás insertando al final (ese caso
ya lo delega `push_back`).
</details>

### Ejercicio 8 — remove_at con actualización de tail

Implementa `remove_at(pos)`.  Particular atención: si eliminas el último
nodo (el que `tail` señala), debes actualizar `tail` al penúltimo.

<details>
<summary>Verificación</summary>

```rust
let mut list = List::new();
list.push_back(10);
list.push_back(20);
list.push_back(30);

list.remove_at(2);   // eliminar el 30 (último)
assert_eq!(list.peek_back(), Some(&20));  // tail actualizado

list.remove_at(0);   // eliminar el 10 (primero)
assert_eq!(list.peek(), Some(&20));       // head actualizado

list.remove_at(0);   // eliminar el 20 (único)
assert!(list.is_empty());
assert_eq!(list.peek_back(), None);       // tail es null
```
</details>

### Ejercicio 9 — Safe wrapper con Send y Sync

`*mut T` no implementa `Send` ni `Sync`.  Esto significa que `List<T>` tampoco
los implementa por defecto, impidiendo su uso entre hilos.  Implementa
`Send` y `Sync` manualmente y explica por qué es seguro hacerlo.

<details>
<summary>Implementación</summary>

```rust
unsafe impl<T: Send> Send for List<T> {}
unsafe impl<T: Sync> Sync for List<T> {}
```

Es seguro porque:
- `List<T>` tiene ownership exclusivo de todos los nodos.
- Los raw pointers internos no se comparten con nadie fuera de `List`.
- Si `T` es `Send`, mover la lista a otro hilo es seguro (los nodos van con
  ella).
- Si `T` es `Sync`, compartir `&List<T>` entre hilos es seguro (solo lectura,
  y nuestro `iter()` solo lee).

Sin estos `impl`, el compilador rechaza `send_list_to_thread(list)` incluso
cuando es perfectamente seguro.
</details>

### Ejercicio 10 — Benchmark: safe vs unsafe vs VecDeque

Mide el tiempo de $10^6$ operaciones `push_back` para:
1. `List<i32>` safe de T04 (push_back $O(n)$ cada una).
2. `List<i32>` unsafe con tail (push_back $O(1)$ cada una).
3. `VecDeque<i32>` (push_back $O(1)$ amortizado).

<details>
<summary>Predicción</summary>

- Safe `push_back`: $O(n)$ por operación × $10^6$ = $O(n^2)$ total.
  Con $n = 10^6$, cada push_back recorre hasta $10^6$ nodos.
  Estimado: **minutos** (posiblemente varios).

- Unsafe `push_back`: $O(1)$ por operación × $10^6$ = $O(n)$ total.
  Cada push es una asignación de puntero + alloc.
  Estimado: **~50-100 ms** (dominado por `malloc` y cache misses).

- `VecDeque::push_back`: $O(1)$ amortizado × $10^6$ = $O(n)$ total.
  Datos contiguos, pocas reallocaciones.
  Estimado: **~5-15 ms** (excelente localidad de caché).

`VecDeque` gana al unsafe por localidad de caché, pero el unsafe gana
masivamente al safe.  Para `push_front`, la lista unsafe y VecDeque empatan
asintóticamente, pero VecDeque sigue ganando por caché en la mayoría de
arquitecturas.

Moraleja: el raw pointer resuelve la complejidad asintótica ($O(n) → O(1)$),
pero las constantes de hardware (caché) siguen favoreciendo a las
estructuras contiguas.
</details>
