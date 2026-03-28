# T03 — NonNull y Box raw

---

## 1. El problema que resuelve NonNull

En T01 vimos que los nodos de estructuras de datos usan `*mut Node<T>` para
sus enlaces. Esto funciona, pero tiene un problema: `*mut T` puede ser null,
y el compilador no sabe si lo es o no.

```rust
struct Node<T> {
    data: T,
    next: *mut Node<T>,   // ¿Es null? ¿Es válido? No se sabe
}
```

Hay dos categorías de punteros en una estructura de datos:

```
1. Punteros que PUEDEN ser null:
   - next del último nodo (fin de lista)
   - left/right de una hoja (sin hijos)
   → *mut T es correcto aquí

2. Punteros que NUNCA deberían ser null:
   - head de una lista no vacía (siempre apunta a algo)
   - el puntero que Box::into_raw retorna (siempre es heap válido)
   → *mut T funciona pero pierde información
```

`NonNull<T>` es un wrapper sobre `*mut T` que **garantiza en el tipo** que el
puntero no es null. Es la herramienta correcta para la segunda categoría.

---

## 2. NonNull\<T\> — definición y propiedades

```rust
use std::ptr::NonNull;
```

`NonNull<T>` es un puntero raw que el compilador sabe que no es null:

```rust
// Simplificación de la definición real en std:
#[repr(transparent)]
pub struct NonNull<T: ?Sized> {
    pointer: *const T,  // internamente es *const, no *mut
}
```

### Propiedades

| Propiedad | `*mut T` | `NonNull<T>` |
|-----------|----------|-------------|
| Puede ser null | Sí | No (invariante del tipo) |
| Tamaño | `size_of::<usize>()` | `size_of::<usize>()` (igual) |
| Covariant en `T` | Sí | Sí |
| `Send`/`Sync` | No | No |
| Optimización de nicho | No | Sí — `Option<NonNull<T>>` = 1 puntero |
| Requiere unsafe para crear | No | Sí (desde raw) o No (desde `&mut`) |

### La optimización de nicho

Esta es la ventaja más importante para estructuras de datos:

```rust
use std::mem::size_of;
use std::ptr::NonNull;

// *mut T: no hay forma de distinguir "null" de "no tengo puntero"
assert_eq!(size_of::<*mut i32>(), 8);           // 8 bytes
assert_eq!(size_of::<Option<*mut i32>>(), 16);  // 16 bytes (8 ptr + 8 discriminante)

// NonNull<T>: null está prohibido, así que Option usa null como None
assert_eq!(size_of::<NonNull<i32>>(), 8);           // 8 bytes
assert_eq!(size_of::<Option<NonNull<i32>>>(), 8);   // 8 bytes (null = None)
```

`Option<NonNull<Node<T>>>` ocupa exactamente lo mismo que un `*mut Node<T>`,
pero expresa la nulabilidad en el tipo:

```
*mut Node<T>                → "puede ser null, verifica manualmente"
Option<NonNull<Node<T>>>    → "Some = válido, None = no hay nodo"
```

Sin costo de memoria adicional.

---

## 3. Crear NonNull

### Desde una referencia (safe)

```rust
use std::ptr::NonNull;

let mut x = 42;

// Desde &mut T — siempre safe (las referencias nunca son null)
let nn: NonNull<i32> = NonNull::from(&mut x);

// Desde &T — también safe
let nn_const: NonNull<i32> = NonNull::from(&x);
```

### Desde un raw pointer (unsafe)

```rust
use std::ptr::NonNull;

let mut x = 42;
let raw: *mut i32 = &mut x;

// SAFETY: raw viene de &mut x, que no es null.
let nn: NonNull<i32> = unsafe { NonNull::new_unchecked(raw) };
```

`new_unchecked` es `unsafe` porque el llamador garantiza que el puntero no es
null. Si pasas null, es UB inmediato (no en la creación, sino cuando se use
asumiendo que no es null).

### Con verificación (safe, retorna Option)

```rust
use std::ptr::{self, NonNull};

let mut x = 42;
let raw: *mut i32 = &mut x;
let null: *mut i32 = ptr::null_mut();

let nn1: Option<NonNull<i32>> = NonNull::new(raw);    // Some(...)
let nn2: Option<NonNull<i32>> = NonNull::new(null);   // None

assert!(nn1.is_some());
assert!(nn2.is_none());
```

`NonNull::new()` es safe: verifica si el puntero es null y retorna
`Option<NonNull<T>>`. Es el constructor preferido cuando no estás seguro
de si el puntero puede ser null.

### Desde Box (patrón más común)

```rust
use std::ptr::NonNull;

let b = Box::new(42);
let raw: *mut i32 = Box::into_raw(b);

// SAFETY: Box::into_raw siempre retorna un puntero no-null
// (Box nunca contiene null — la asignación en heap falla con panic,
// no con null).
let nn: NonNull<i32> = unsafe { NonNull::new_unchecked(raw) };
```

Este patrón — `Box::new` → `Box::into_raw` → `NonNull::new_unchecked` — es
el más frecuente en implementaciones de estructuras de datos.

---

## 4. Usar NonNull

### Obtener el raw pointer

```rust
use std::ptr::NonNull;

let mut x = 42;
let nn = NonNull::from(&mut x);

// NonNull<T> → *mut T
let raw: *mut i32 = nn.as_ptr();

// Desreferenciar (unsafe, como cualquier raw pointer)
// SAFETY: nn fue creado desde &mut x, que es válido.
unsafe {
    *raw = 100;
}
assert_eq!(x, 100);
```

### Obtener referencias

```rust
let mut x = 42;
let mut nn = NonNull::from(&mut x);

// NonNull<T> → &T (unsafe)
// SAFETY: nn apunta a x, que es válido y no hay &mut activo.
let r: &i32 = unsafe { nn.as_ref() };
assert_eq!(*r, 42);

// NonNull<T> → &mut T (unsafe)
// SAFETY: nn apunta a x, que es válido. No hay otras
// referencias activas a x.
let rm: &mut i32 = unsafe { nn.as_mut() };
*rm = 100;
assert_eq!(x, 100);
```

`as_ref()` y `as_mut()` son `unsafe` por las mismas razones que desreferenciar
un raw pointer: el compilador no puede verificar validez ni aliasing.

### NonNull no permite null — eso es el punto

```rust
use std::ptr::{self, NonNull};

// Esto es un error lógico (UB potencial):
let bad = unsafe { NonNull::new_unchecked(ptr::null_mut::<i32>()) };
// bad "promete" que no es null, pero lo es.
// Cualquier código que confíe en esa promesa puede tener UB.

// Esto es lo correcto:
let checked = NonNull::new(ptr::null_mut::<i32>());
assert!(checked.is_none());  // None, no panic
```

---

## 5. Box::into_raw y Box::from_raw

### Box::into_raw — transferir al mundo raw

```rust
let b: Box<i32> = Box::new(42);

// Consume el Box, retorna *mut i32
// La memoria NO se libera — ahora es responsabilidad del programador
let raw: *mut i32 = Box::into_raw(b);

// b ya no existe — fue consumido (moved)
// println!("{b}");  // error: use of moved value
```

Después de `into_raw`:
- La memoria sigue asignada en el heap
- No hay `Box` que la libere automáticamente
- El programador **debe** eventualmente llamar `Box::from_raw` o la memoria
  se pierde (leak)

### Box::from_raw — retomar ownership

```rust
let raw: *mut i32 = Box::into_raw(Box::new(42));

// ... usar raw pointer ...

// Reconstruir el Box para liberar la memoria
// SAFETY: raw fue creado con Box::into_raw y no ha sido liberado.
// No hay otros owners del puntero.
let b: Box<i32> = unsafe { Box::from_raw(raw) };
// b se destruye al salir del scope → memoria liberada
```

### Regla de correspondencia

```
Cada Box::into_raw DEBE tener exactamente un Box::from_raw correspondiente.

0 from_raw → memory leak
1 from_raw → correcto
2 from_raw → double free (UB)
```

### El ciclo de vida completo

```rust
// 1. Crear en el heap con Box
let b = Box::new(Node { data: 42, next: ptr::null_mut() });

// 2. Transferir a raw pointer
let raw: *mut Node = Box::into_raw(b);

// 3. Usar el raw pointer (modificar, enlazar, etc.)
unsafe {
    (*raw).next = otro_nodo;
}

// 4. Cuando sea hora de liberar: retomar ownership
unsafe {
    let node = Box::from_raw(raw);
    // node.data se puede mover antes del drop si se necesita
}
// 5. Box se destruye → memoria liberada
```

---

## 6. El patrón para listas con NonNull

### Lista enlazada simple con Option\<NonNull\>

```rust
use std::ptr::{self, NonNull};

struct Node<T> {
    data: T,
    next: Option<NonNull<Node<T>>>,
}

pub struct LinkedList<T> {
    head: Option<NonNull<Node<T>>>,
    len: usize,
}
```

Compara con la versión `*mut T`:

```rust
// Con *mut T:
struct Node<T> {
    data: T,
    next: *mut Node<T>,        // null = fin de lista
}
struct LinkedList<T> {
    head: *mut Node<T>,        // null = lista vacía
    len: usize,
}

// Con Option<NonNull<T>>:
struct Node<T> {
    data: T,
    next: Option<NonNull<Node<T>>>,   // None = fin de lista
}
struct LinkedList<T> {
    head: Option<NonNull<Node<T>>>,   // None = lista vacía
    len: usize,
}
```

Ambas tienen el **mismo tamaño en memoria** gracias a la optimización de nicho,
pero `Option<NonNull>` expresa la intención de forma más clara y permite usar
pattern matching:

```rust
// Con *mut T:
if !self.head.is_null() {
    unsafe { /* ... */ }
}

// Con Option<NonNull<T>>:
if let Some(head) = self.head {
    unsafe { /* ... con head que SABEMOS que no es null */ }
}
```

### Implementación completa

```rust
use std::ptr::NonNull;

struct Node<T> {
    data: T,
    next: Option<NonNull<Node<T>>>,
}

pub struct LinkedList<T> {
    head: Option<NonNull<Node<T>>>,
    len: usize,
}

impl<T> LinkedList<T> {
    pub fn new() -> Self {
        LinkedList { head: None, len: 0 }
    }

    pub fn push_front(&mut self, data: T) {
        let node = Box::new(Node {
            data,
            next: self.head,   // el nuevo nodo apunta al head actual
        });

        // SAFETY: Box::into_raw nunca retorna null.
        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };
        self.head = Some(nn);
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        // Pattern matching: si head es None, retorna None
        let head_nn = self.head?;

        // SAFETY: head_nn es NonNull, fue creado con Box::into_raw
        // en push_front. No ha sido liberado.
        unsafe {
            let node = Box::from_raw(head_nn.as_ptr());
            self.head = node.next;
            self.len -= 1;
            Some(node.data)
        }
    }

    pub fn front(&self) -> Option<&T> {
        // SAFETY: head_nn es NonNull y válido (mismo razonamiento).
        // Retornamos &T con lifetime ligado a &self.
        self.head.map(|head_nn| unsafe { &(*head_nn.as_ptr()).data })
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }
}

impl<T> Drop for LinkedList<T> {
    fn drop(&mut self) {
        while self.pop_front().is_some() {}
    }
}
```

### Ventajas sobre `*mut T` puro

| Aspecto | `*mut T` | `Option<NonNull<T>>` |
|---------|----------|---------------------|
| Nulabilidad | Implícita (check manual) | Explícita (`None`/`Some`) |
| Pattern matching | No | Sí (`if let Some(nn)`) |
| Operador `?` | No | Sí (`self.head?`) |
| Misma memoria | Sí | Sí |
| Documentación en tipos | "Puede ser null" implícito | "Puede no existir" explícito |

---

## 7. Lista doblemente enlazada con NonNull

Las listas doblemente enlazadas son el caso más representativo para raw pointers
porque `prev` y `next` crean ciclos que las referencias safe no pueden expresar:

```rust
use std::ptr::NonNull;

struct DNode<T> {
    data: T,
    prev: Option<NonNull<DNode<T>>>,
    next: Option<NonNull<DNode<T>>>,
}

pub struct DoublyLinkedList<T> {
    head: Option<NonNull<DNode<T>>>,
    tail: Option<NonNull<DNode<T>>>,
    len: usize,
}

impl<T> DoublyLinkedList<T> {
    pub fn new() -> Self {
        DoublyLinkedList {
            head: None,
            tail: None,
            len: 0,
        }
    }

    pub fn push_back(&mut self, data: T) {
        let node = Box::new(DNode {
            data,
            prev: self.tail,    // nuevo nodo apunta al tail actual
            next: None,
        });

        // SAFETY: Box::into_raw retorna no-null.
        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };

        match self.tail {
            None => {
                // Lista vacía: head y tail son el nuevo nodo
                self.head = Some(nn);
            }
            Some(old_tail) => {
                // SAFETY: old_tail es válido (fue creado con Box::into_raw,
                // no ha sido liberado).
                unsafe { (*old_tail.as_ptr()).next = Some(nn); }
            }
        }

        self.tail = Some(nn);
        self.len += 1;
    }

    pub fn push_front(&mut self, data: T) {
        let node = Box::new(DNode {
            data,
            prev: None,
            next: self.head,    // nuevo nodo apunta al head actual
        });

        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };

        match self.head {
            None => {
                self.tail = Some(nn);
            }
            Some(old_head) => {
                // SAFETY: old_head es válido.
                unsafe { (*old_head.as_ptr()).prev = Some(nn); }
            }
        }

        self.head = Some(nn);
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        let head_nn = self.head?;

        // SAFETY: head_nn es NonNull, creado con Box::into_raw.
        unsafe {
            let node = Box::from_raw(head_nn.as_ptr());
            self.head = node.next;

            match self.head {
                None => self.tail = None,     // lista quedó vacía
                Some(new_head) => {
                    // SAFETY: new_head es válido.
                    (*new_head.as_ptr()).prev = None;
                }
            }

            self.len -= 1;
            Some(node.data)
        }
    }

    pub fn pop_back(&mut self) -> Option<T> {
        let tail_nn = self.tail?;

        // SAFETY: tail_nn es NonNull, creado con Box::into_raw.
        unsafe {
            let node = Box::from_raw(tail_nn.as_ptr());
            self.tail = node.prev;

            match self.tail {
                None => self.head = None,
                Some(new_tail) => {
                    // SAFETY: new_tail es válido.
                    (*new_tail.as_ptr()).next = None;
                }
            }

            self.len -= 1;
            Some(node.data)
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

impl<T> Drop for DoublyLinkedList<T> {
    fn drop(&mut self) {
        while self.pop_front().is_some() {}
    }
}
```

Observa cómo `Option<NonNull<DNode<T>>>` para `prev` y `next` es natural:
- `prev` del primer nodo: `None`
- `next` del último nodo: `None`
- Todos los demás: `Some(nn)` donde `nn` apunta a un nodo válido

---

## 8. Box::leak — la alternativa para datos permanentes

`Box::leak` es una variante de `Box::into_raw` que retorna `&'static mut T`
en vez de `*mut T`:

```rust
let b = Box::new(42);
let r: &'static mut i32 = Box::leak(b);
*r = 100;
println!("{r}");  // 100
// La memoria NUNCA se libera (leak intencional)
```

### Cuándo usar Box::leak vs Box::into_raw

```
Box::into_raw:
  → Quieres un *mut T para gestionar manualmente
  → Lo vas a liberar después con Box::from_raw
  → Uso: nodos de estructuras de datos

Box::leak:
  → Quieres una referencia &'static mut T que viva para siempre
  → NO piensas liberar la memoria
  → Uso: datos globales que se crean una vez y nunca se liberan
```

### Ejemplo: configuración global

```rust
struct Config {
    db_url: String,
    port: u16,
}

fn init_config() -> &'static Config {
    let config = Box::new(Config {
        db_url: String::from("postgres://localhost/mydb"),
        port: 5432,
    });
    Box::leak(config)
}

fn main() {
    let config: &'static Config = init_config();
    // config vive para siempre — no necesita cleanup
    println!("port: {}", config.port);
}
```

En el contexto de estructuras de datos, `Box::leak` es poco frecuente.
Se usa `Box::into_raw` / `Box::from_raw` porque los nodos **sí** se liberan
cuando se remueven de la estructura.

---

## 9. Resumen de patrones de conversión

### El flujo completo para nodos

```
Crear:    Box::new(Node{...})  →  Box::into_raw  →  NonNull::new_unchecked
                                                          ↓
Usar:                               NonNull.as_ptr()  →  (*ptr).field
                                                          ↓
Liberar:                            Box::from_raw(nn.as_ptr())  →  drop
```

### Tabla de conversiones

| De | A | Cómo | Safe? |
|----|---|------|-------|
| `Box<T>` | `*mut T` | `Box::into_raw(b)` | Safe (pero debes liberar) |
| `*mut T` | `Box<T>` | `Box::from_raw(p)` | Unsafe |
| `Box<T>` | `NonNull<T>` | `NonNull::new_unchecked(Box::into_raw(b))` | Unsafe (`new_unchecked`) |
| `NonNull<T>` | `*mut T` | `nn.as_ptr()` | Safe |
| `NonNull<T>` | `&T` | `nn.as_ref()` | Unsafe |
| `NonNull<T>` | `&mut T` | `nn.as_mut()` | Unsafe |
| `NonNull<T>` | `Box<T>` | `Box::from_raw(nn.as_ptr())` | Unsafe |
| `*mut T` | `NonNull<T>` | `NonNull::new(p)` → `Option` | Safe |
| `*mut T` | `NonNull<T>` | `NonNull::new_unchecked(p)` | Unsafe |
| `&mut T` | `NonNull<T>` | `NonNull::from(r)` | Safe |
| `&T` | `NonNull<T>` | `NonNull::from(r)` | Safe |

### Cuándo usar cada tipo

```
¿El puntero puede ser null legítimamente?
  ├─ Sí  → Option<NonNull<T>>  (nulabilidad explícita, optimización de nicho)
  └─ No  → NonNull<T>          (garantía de no-null en el tipo)

¿Necesitas un raw pointer temporal para una operación?
  → .as_ptr() para obtener *mut T

¿Estás haciendo FFI con C?
  → *mut T / *const T  (lo que C espera)

¿Es un campo de nodo que enlaza a otro nodo?
  → Option<NonNull<Node<T>>>  (patrón idiomático)
```

---

## Ejercicios

### Ejercicio 1 — Tamaño de Option\<NonNull\>

Verifica experimentalmente la optimización de nicho. Imprime el tamaño de cada
tipo:

```rust
use std::mem::size_of;
use std::ptr::NonNull;

fn main() {
    // Imprime el tamaño de:
    // a) *mut i32
    // b) Option<*mut i32>
    // c) NonNull<i32>
    // d) Option<NonNull<i32>>
    // e) Option<Option<NonNull<i32>>>
}
```

**Prediccion**: (d) es menor que (b)? Y (e)?

<details><summary>Respuesta</summary>

```rust
use std::mem::size_of;
use std::ptr::NonNull;

fn main() {
    println!("*mut i32:                    {}", size_of::<*mut i32>());
    println!("Option<*mut i32>:            {}", size_of::<Option<*mut i32>>());
    println!("NonNull<i32>:                {}", size_of::<NonNull<i32>>());
    println!("Option<NonNull<i32>>:        {}", size_of::<Option<NonNull<i32>>>());
    println!("Option<Option<NonNull<i32>>>:{}", size_of::<Option<Option<NonNull<i32>>>>());
}
```

Salida (en arquitectura de 64 bits):

```
*mut i32:                    8
Option<*mut i32>:            16
NonNull<i32>:                8
Option<NonNull<i32>>:        8
Option<Option<NonNull<i32>>>:16
```

- (d) = 8 < (b) = 16 — **sí, la mitad**. `Option<NonNull>` usa null como
  discriminante, sin costo adicional.
- (e) = 16 — el doble `Option` necesita distinguir `None`, `Some(None)`, y
  `Some(Some(nn))`. Null cubre un caso, pero necesita un byte extra para
  el segundo nivel. En la práctica, el compilador redondea a 16.

Para nodos de lista, la diferencia entre (b) y (d) ahorra 8 bytes por nodo.
En una lista de $10^6$ nodos con `prev` y `next`, eso son $10^6 \times 2 \times 8 = 16$ MB
de diferencia.

</details>

---

### Ejercicio 2 — Crear NonNull de todas las formas

Crea un `NonNull<i32>` usando cada uno de estos métodos:
1. `NonNull::from(&mut x)`
2. `NonNull::new(raw)` — maneja el `Option`
3. `NonNull::new_unchecked(raw)`
4. Desde `Box::into_raw`

Para cada uno, imprime la dirección y el valor apuntado.

```rust
use std::ptr::NonNull;

fn main() {
    // Tu código aquí
}
```

**Prediccion**: `NonNull::new(ptr::null_mut())` retorna `Some` o `None`?

<details><summary>Respuesta</summary>

```rust
use std::ptr::{self, NonNull};

fn main() {
    // 1. Desde &mut T
    let mut x = 10;
    let nn1 = NonNull::from(&mut x);
    // SAFETY: nn1 viene de una referencia válida.
    unsafe { println!("1. addr={:p} val={}", nn1.as_ptr(), *nn1.as_ptr()); }

    // 2. NonNull::new con Option
    let mut y = 20;
    let raw: *mut i32 = &mut y;
    let nn2: NonNull<i32> = NonNull::new(raw).expect("was null");
    unsafe { println!("2. addr={:p} val={}", nn2.as_ptr(), *nn2.as_ptr()); }

    // NonNull::new con null → None
    let nn_null = NonNull::<i32>::new(ptr::null_mut());
    println!("   null → {:?}", nn_null);  // None

    // 3. new_unchecked
    let mut z = 30;
    let raw_z: *mut i32 = &mut z;
    // SAFETY: raw_z viene de &mut z, no es null.
    let nn3 = unsafe { NonNull::new_unchecked(raw_z) };
    unsafe { println!("3. addr={:p} val={}", nn3.as_ptr(), *nn3.as_ptr()); }

    // 4. Desde Box::into_raw
    let b = Box::new(40);
    let raw_b: *mut i32 = Box::into_raw(b);
    // SAFETY: Box::into_raw nunca retorna null.
    let nn4 = unsafe { NonNull::new_unchecked(raw_b) };
    unsafe { println!("4. addr={:p} val={}", nn4.as_ptr(), *nn4.as_ptr()); }

    // Liberar la memoria del Box
    // SAFETY: raw_b fue creado con Box::into_raw y no ha sido liberado.
    unsafe { drop(Box::from_raw(nn4.as_ptr())); }
}
```

`NonNull::new(ptr::null_mut())` retorna **`None`** — ese es el propósito
del check. Si quieres forzar un null dentro de `NonNull`, necesitarías
`new_unchecked`, lo cual sería un error lógico (violación del invariante).

</details>

---

### Ejercicio 3 — Migrar de \*mut T a Option\<NonNull\>

Refactoriza este stack de `*mut Node<T>` a `Option<NonNull<Node<T>>>`:

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct Stack<T> {
    head: *mut Node<T>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { head: ptr::null_mut(), len: 0 }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::into_raw(Box::new(Node { data, next: self.head }));
        self.head = node;
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.head.is_null() { return None; }
        unsafe {
            let old = self.head;
            self.head = (*old).next;
            let node = Box::from_raw(old);
            self.len -= 1;
            Some(node.data)
        }
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) { while self.pop().is_some() {} }
}
```

**Prediccion**: El `pop` con `Option<NonNull>` puede usar el operador `?`?

<details><summary>Respuesta</summary>

```rust
use std::ptr::NonNull;

struct Node<T> {
    data: T,
    next: Option<NonNull<Node<T>>>,
}

pub struct Stack<T> {
    head: Option<NonNull<Node<T>>>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { head: None, len: 0 }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::new(Node {
            data,
            next: self.head,  // el nuevo nodo enlaza al head actual
        });
        // SAFETY: Box::into_raw nunca retorna null.
        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };
        self.head = Some(nn);
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        let head_nn = self.head?;  // retorna None si la lista está vacía

        // SAFETY: head_nn es NonNull, creado con Box::into_raw en push().
        unsafe {
            let node = Box::from_raw(head_nn.as_ptr());
            self.head = node.next;
            self.len -= 1;
            Some(node.data)
        }
    }

    pub fn peek(&self) -> Option<&T> {
        // SAFETY: head_nn es válido.
        self.head.map(|nn| unsafe { &(*nn.as_ptr()).data })
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}

fn main() {
    let mut s = Stack::new();
    s.push(1);
    s.push(2);
    s.push(3);
    assert_eq!(s.peek(), Some(&3));
    assert_eq!(s.pop(), Some(3));
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert_eq!(s.pop(), None);
    println!("All tests passed!");
}
```

Sí, `self.head?` funciona porque `head` es `Option<NonNull<...>>` — si es
`None`, retorna `None` de `pop()` inmediatamente. Esto elimina el check
manual `if self.head.is_null()`.

Cambios respecto a la versión `*mut T`:
- `ptr::null_mut()` → `None`
- `if self.head.is_null()` → `self.head?`
- `self.head = node` → `self.head = Some(nn)`
- `(*old).next` → `node.next` (tras `Box::from_raw`)

La estructura del código es la misma; solo cambia la expresión de nulabilidad.

</details>

---

### Ejercicio 4 — Box::into_raw / Box::from_raw matching

Cada uno de estos fragmentos tiene un error de correspondencia entre
`into_raw` y `from_raw`. Identifícalo:

```rust
// Fragmento A
fn a() {
    let raw = Box::into_raw(Box::new(42));
    // ... se usa raw ...
    // fin de función — sin from_raw
}

// Fragmento B
fn b() {
    let raw = Box::into_raw(Box::new(42));
    unsafe {
        drop(Box::from_raw(raw));
        println!("{}", *raw);
    }
}

// Fragmento C
fn c() {
    let raw = Box::into_raw(Box::new(42));
    unsafe {
        drop(Box::from_raw(raw));
        drop(Box::from_raw(raw));
    }
}

// Fragmento D
fn d() {
    let mut x = 42;   // stack
    let raw: *mut i32 = &mut x;
    unsafe {
        drop(Box::from_raw(raw));
    }
}
```

**Prediccion**: Clasifica cada uno como leak, use-after-free, double free,
o invalid free.

<details><summary>Respuesta</summary>

| Fragmento | Error | Tipo |
|-----------|-------|------|
| A | Sin `from_raw` → la memoria nunca se libera | **Memory leak** |
| B | `from_raw` libera, luego `*raw` lee memoria liberada | **Use-after-free** |
| C | `from_raw` se llama dos veces sobre el mismo puntero | **Double free** |
| D | `raw` apunta al stack, no al heap. `Box::from_raw` intenta liberar memoria que no fue asignada con `Box::new` | **Invalid free** |

Correcciones:

```rust
// A: agregar from_raw
fn a() {
    let raw = Box::into_raw(Box::new(42));
    // ... se usa raw ...
    unsafe { drop(Box::from_raw(raw)); }
}

// B: leer antes de liberar
fn b() {
    let raw = Box::into_raw(Box::new(42));
    unsafe {
        println!("{}", *raw);     // leer primero
        drop(Box::from_raw(raw)); // liberar después
    }
}

// C: solo un from_raw
fn c() {
    let raw = Box::into_raw(Box::new(42));
    unsafe {
        drop(Box::from_raw(raw)); // una vez, no dos
    }
}

// D: no usar from_raw con punteros al stack
fn d() {
    let mut x = 42;
    let raw: *mut i32 = &mut x;
    unsafe {
        println!("{}", *raw);  // leer es OK
    }
    // x se libera automáticamente al salir del scope
}
```

La regla: `Box::from_raw` solo se llama con punteros que fueron creados por
`Box::into_raw` (o `Box::leak`, si quieres recuperar la memoria).

</details>

---

### Ejercicio 5 — Iterar con NonNull

Implementa un iterador para la `LinkedList<T>` de la sección 6. El iterador
debe retornar `&T` (referencias inmutables):

```rust
pub struct Iter<'a, T> {
    current: Option<NonNull<Node<T>>>,
    _marker: std::marker::PhantomData<&'a T>,
}

impl<T> LinkedList<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        todo!()
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        todo!()
    }
}
```

**Prediccion**: Por qué necesitamos `PhantomData<&'a T>`?

<details><summary>Respuesta</summary>

```rust
use std::marker::PhantomData;
use std::ptr::NonNull;

pub struct Iter<'a, T> {
    current: Option<NonNull<Node<T>>>,
    _marker: PhantomData<&'a T>,
}

impl<T> LinkedList<T> {
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
        let nn = self.current?;

        // SAFETY: nn es NonNull, fue creado con Box::into_raw en push_front.
        // Retornamos &'a T donde 'a está ligado al lifetime de &LinkedList,
        // lo que impide que la lista se modifique mientras el iterador existe.
        unsafe {
            let node = nn.as_ptr();
            self.current = (*node).next;
            Some(&(*node).data)
        }
    }
}

// Uso:
fn main() {
    let mut list = LinkedList::new();
    list.push_front(3);
    list.push_front(2);
    list.push_front(1);

    for val in list.iter() {
        print!("{val} ");
    }
    println!();
    // Salida: 1 2 3
}
```

`PhantomData<&'a T>` es necesario porque:

1. `NonNull<Node<T>>` no tiene lifetime — es un raw pointer sin seguimiento
   del compilador
2. Sin `PhantomData`, el struct `Iter` no tendría relación con el lifetime
   `'a`, y el compilador no podría verificar que la lista no se modifica
   mientras el iterador existe
3. `PhantomData<&'a T>` "pretende" que `Iter` contiene un `&'a T`, lo que
   liga el lifetime del iterador al de la lista

Sin `PhantomData`, este código peligroso compilaría:

```rust
let mut list = LinkedList::new();
list.push_front(1);
let iter = list.iter();
list.pop_front();     // ← modifica la lista mientras iter existe
iter.next();          // ← use-after-free: iter apunta al nodo liberado
```

Con `PhantomData`, el borrow checker detecta que `iter` tiene un borrow
implícito de `list`, y prohíbe `pop_front` mientras `iter` está vivo.

</details>

---

### Ejercicio 6 — Pop de lista doble por ambos extremos

Usando la `DoublyLinkedList<T>` de la sección 7, escribe un programa que:
1. Inserte los valores 1, 2, 3, 4, 5 con `push_back`
2. Haga `pop_front` y `pop_back` alternadamente hasta vaciar la lista
3. Imprima cada valor removido y de qué extremo salió

```rust
fn main() {
    let mut list = DoublyLinkedList::new();
    // Tu código aquí
}
```

**Prediccion**: En qué orden salen los valores?

<details><summary>Respuesta</summary>

```rust
fn main() {
    let mut list = DoublyLinkedList::new();

    for i in 1..=5 {
        list.push_back(i);
    }
    // Lista: 1 <-> 2 <-> 3 <-> 4 <-> 5

    let mut from_front = true;
    while !list.is_empty() {
        if from_front {
            if let Some(val) = list.pop_front() {
                println!("front: {val}");
            }
        } else {
            if let Some(val) = list.pop_back() {
                println!("back:  {val}");
            }
        }
        from_front = !from_front;
    }
}
```

Salida:

```
front: 1
back:  5
front: 2
back:  4
front: 3
```

Orden: 1 (front), 5 (back), 2 (front), 4 (back), 3 (front).

La lista "se encoge" desde ambos extremos:

```
[1, 2, 3, 4, 5]    → pop_front: 1
   [2, 3, 4, 5]    → pop_back:  5
   [2, 3, 4]       → pop_front: 2
      [3, 4]       → pop_back:  4
      [3]          → pop_front: 3
      []           → vacía
```

Este patrón verifica que los punteros `prev` y `next` se actualizan
correctamente en ambas direcciones — si algún enlace está roto, el recorrido
falla.

</details>

---

### Ejercicio 7 — Leak detector manual

Escribe una función que verifique si una `LinkedList<T>` tiene leaks.
La función recorre la lista contando nodos y compara con `len`:

```rust
impl<T> LinkedList<T> {
    /// Verifica que el número de nodos accesibles desde head == self.len.
    /// Retorna Ok(()) o Err con el conteo real vs esperado.
    pub fn check_integrity(&self) -> Result<(), (usize, usize)> {
        todo!()
    }
}
```

**Prediccion**: Si hay un nodo huérfano (apuntado por nadie pero no liberado),
esta función lo detecta?

<details><summary>Respuesta</summary>

```rust
impl<T> LinkedList<T> {
    pub fn check_integrity(&self) -> Result<(), (usize, usize)> {
        let mut count = 0;
        let mut current = self.head;
        while let Some(nn) = current {
            count += 1;
            // SAFETY: nn es NonNull, fue creado con Box::into_raw.
            current = unsafe { (*nn.as_ptr()).next };

            // Protección contra loops infinitos (lista circular por bug)
            if count > self.len + 1 {
                return Err((count, self.len));
            }
        }

        if count == self.len {
            Ok(())
        } else {
            Err((count, self.len))
        }
    }
}

fn main() {
    let mut list = LinkedList::new();
    list.push_front(1);
    list.push_front(2);
    list.push_front(3);

    match list.check_integrity() {
        Ok(()) => println!("Integrity OK: {} nodes", list.len()),
        Err((actual, expected)) => {
            println!("ERROR: found {actual} nodes, expected {expected}");
        }
    }
}
```

No, esta función **no detecta nodos huérfanos**. Un nodo huérfano (memoria
en el heap que nadie apunta) no es accesible desde `head`, así que el
recorrido no lo encuentra. La función solo verifica que la cadena
head → next → next → ... tenga la longitud correcta.

Para detectar nodos huérfanos necesitas herramientas externas:
- `cargo miri test` (detecta leaks al final del programa)
- Valgrind (si compilas via FFI)
- Un allocator personalizado que cuente allocations vs frees

Lo que sí detecta esta función:
- `len` desincronizado con la cantidad real de nodos
- Lista circular por un bug en los enlaces (gracias al guard `count > len + 1`)

</details>

---

### Ejercicio 8 — Box::from_raw con datos no-Copy

Implementa un `pop_front` para una lista de `String`. El punto clave: el
`String` debe **moverse** fuera del nodo, no clonarse.

```rust
use std::ptr::NonNull;

struct Node {
    data: String,
    next: Option<NonNull<Node>>,
}

struct StringList {
    head: Option<NonNull<Node>>,
}

impl StringList {
    fn new() -> Self { StringList { head: None } }

    fn push_front(&mut self, data: String) {
        let node = Box::new(Node { data, next: self.head });
        let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };
        self.head = Some(nn);
    }

    fn pop_front(&mut self) -> Option<String> {
        todo!()
    }
}

impl Drop for StringList {
    fn drop(&mut self) { while self.pop_front().is_some() {} }
}
```

**Prediccion**: Si usaras `(*nn.as_ptr()).data.clone()` en vez de mover,
qué pasaría con la memoria del nodo?

<details><summary>Respuesta</summary>

```rust
impl StringList {
    fn pop_front(&mut self) -> Option<String> {
        let nn = self.head?;

        // SAFETY: nn es NonNull, creado con Box::into_raw en push_front.
        // Box::from_raw retoma ownership. node.data mueve el String
        // fuera del nodo. El nodo se libera al salir del scope.
        unsafe {
            let node = Box::from_raw(nn.as_ptr());
            self.head = node.next;
            Some(node.data)  // mueve data fuera del Box
        }
    }
}

fn main() {
    let mut list = StringList::new();
    list.push_front(String::from("hello"));
    list.push_front(String::from("world"));

    assert_eq!(list.pop_front(), Some(String::from("world")));
    assert_eq!(list.pop_front(), Some(String::from("hello")));
    assert_eq!(list.pop_front(), None);
    println!("All tests passed!");
}
```

Si usaras `.clone()` en vez de mover:

```rust
// MAL:
let data = unsafe { (*nn.as_ptr()).data.clone() };
self.head = unsafe { (*nn.as_ptr()).next };
Some(data)
// Problema: el nodo nunca se libera → memory leak
// Además, requiere T: Clone innecesariamente
```

Con `.clone()`, obtienes una copia del `String` pero el nodo original
(incluyendo su propio `String`) sigue en el heap sin nadie que lo libere.

Con `Box::from_raw` + mover `node.data`:
1. `Box::from_raw` retoma ownership del nodo
2. `node.data` mueve el `String` fuera del nodo (sin clonar)
3. El `Box` se destruye al salir del scope, liberando la memoria del nodo
4. El `String` movido sobrevive como valor de retorno

No hay leak, no hay clone, funciona con cualquier `T` (no solo `T: Clone`).

</details>

---

### Ejercicio 9 — Dangling NonNull

Predice qué pasa en cada caso:

```rust
use std::ptr::NonNull;

// Caso A: NonNull a variable local
fn case_a() -> NonNull<i32> {
    let mut x = 42;
    NonNull::from(&mut x)
}

// Caso B: NonNull a Box leaked
fn case_b() -> NonNull<i32> {
    let b = Box::new(42);
    // SAFETY: Box::into_raw retorna no-null.
    unsafe { NonNull::new_unchecked(Box::into_raw(b)) }
}

fn main() {
    let a = case_a();
    let b = case_b();

    unsafe {
        println!("A: {}", *a.as_ptr());
        println!("B: {}", *b.as_ptr());
        drop(Box::from_raw(b.as_ptr()));
    }
}
```

**Prediccion**: NonNull previene dangling pointers?

<details><summary>Respuesta</summary>

- **Caso A**: `NonNull` apunta a `x` que vive en el stack de `case_a()`.
  Cuando la función retorna, `x` se destruye → `a` es **dangling**.
  Desreferenciar `*a.as_ptr()` es **UB**.

- **Caso B**: `NonNull` apunta a memoria en el heap creada por `Box::new`.
  `Box::into_raw` previene la liberación automática. `b` es **válido**
  después del retorno.

**No**, `NonNull` **no previene dangling pointers**. `NonNull` solo garantiza
una cosa: que el puntero no es null. No dice nada sobre:
- Si la memoria sigue siendo válida
- Si el puntero está alineado
- Si apunta a un valor inicializado

```
NonNull garantiza:    ptr ≠ null
NonNull NO garantiza: ptr → memoria válida
```

Es la misma situación que `*mut T`, con la única diferencia de que sabes que
no es null. La validez de la memoria sigue siendo responsabilidad del
programador.

Por eso `as_ref()` y `as_mut()` siguen siendo `unsafe` en `NonNull`.

</details>

---

### Ejercicio 10 — Comparar las tres APIs

Implementa la misma operación — insertar un nodo al frente — usando tres
estilos distintos. Compara la ergonomía:

```rust
// Estilo A: *mut T puro
fn push_raw(head: &mut *mut RawNode, data: i32) { todo!() }

// Estilo B: Option<NonNull<T>>
fn push_nn(head: &mut Option<NonNull<NNNode>>, data: i32) { todo!() }

// Estilo C: Option<Box<T>> (safe)
fn push_safe(head: &mut Option<Box<SafeNode>>, data: i32) { todo!() }
```

Define los tres tipos de nodo y las tres funciones.

**Prediccion**: Cuál de las tres no necesita `unsafe` en absoluto?

<details><summary>Respuesta</summary>

```rust
use std::ptr::{self, NonNull};

// --- Estilo A: *mut T ---
struct RawNode {
    data: i32,
    next: *mut RawNode,
}

fn push_raw(head: &mut *mut RawNode, data: i32) {
    let node = Box::into_raw(Box::new(RawNode {
        data,
        next: *head,
    }));
    *head = node;
}

fn pop_raw(head: &mut *mut RawNode) -> Option<i32> {
    if head.is_null() { return None; }
    unsafe {
        let old = *head;
        *head = (*old).next;
        let node = Box::from_raw(old);
        Some(node.data)
    }
}

// --- Estilo B: Option<NonNull<T>> ---
struct NNNode {
    data: i32,
    next: Option<NonNull<NNNode>>,
}

fn push_nn(head: &mut Option<NonNull<NNNode>>, data: i32) {
    let node = Box::new(NNNode { data, next: *head });
    // SAFETY: Box::into_raw retorna no-null.
    let nn = unsafe { NonNull::new_unchecked(Box::into_raw(node)) };
    *head = Some(nn);
}

fn pop_nn(head: &mut Option<NonNull<NNNode>>) -> Option<i32> {
    let nn = (*head)?;
    // SAFETY: nn es NonNull, creado con Box::into_raw.
    unsafe {
        let node = Box::from_raw(nn.as_ptr());
        *head = node.next;
        Some(node.data)
    }
}

// --- Estilo C: Option<Box<T>> (safe) ---
struct SafeNode {
    data: i32,
    next: Option<Box<SafeNode>>,
}

fn push_safe(head: &mut Option<Box<SafeNode>>, data: i32) {
    let node = Box::new(SafeNode {
        data,
        next: head.take(),   // mueve el head actual al nuevo nodo
    });
    *head = Some(node);
}

fn pop_safe(head: &mut Option<Box<SafeNode>>) -> Option<i32> {
    let node = head.take()?;  // extrae el Box, head queda None
    *head = node.next;        // restaura head con el siguiente
    Some(node.data)
}

fn main() {
    // A: *mut T
    let mut head_a: *mut RawNode = ptr::null_mut();
    push_raw(&mut head_a, 1);
    push_raw(&mut head_a, 2);
    println!("A: {:?} {:?}", pop_raw(&mut head_a), pop_raw(&mut head_a));

    // B: Option<NonNull<T>>
    let mut head_b: Option<NonNull<NNNode>> = None;
    push_nn(&mut head_b, 1);
    push_nn(&mut head_b, 2);
    println!("B: {:?} {:?}", pop_nn(&mut head_b), pop_nn(&mut head_b));

    // C: Option<Box<T>> (safe)
    let mut head_c: Option<Box<SafeNode>> = None;
    push_safe(&mut head_c, 1);
    push_safe(&mut head_c, 2);
    println!("C: {:?} {:?}", pop_safe(&mut head_c), pop_safe(&mut head_c));
}
```

Salida (las tres idénticas):

```
A: Some(2) Some(1)
B: Some(2) Some(1)
C: Some(2) Some(1)
```

**Estilo C** no necesita `unsafe` en absoluto. `Option<Box<Node>>` es
completamente safe — el compilador gestiona ownership y liberación.

Comparación:

| Aspecto | `*mut T` | `Option<NonNull>` | `Option<Box>` |
|---------|----------|-------------------|---------------|
| Unsafe necesario | Sí | Sí | **No** |
| Null check | Manual (`is_null()`) | Pattern match / `?` | Pattern match / `?` |
| Liberación | Manual (`Box::from_raw`) | Manual (`Box::from_raw`) | **Automática** |
| Doble enlace (prev+next) | Sí | Sí | **No** (un Box = un owner) |
| Overhead runtime | Ninguno | Ninguno | Ninguno |

`Option<Box<Node>>` es la opción correcta para listas simples. Se necesitan
raw pointers (estilo A o B) solo cuando la estructura requiere aliasing mutable
(listas doblemente enlazadas, grafos, árboles con puntero al padre).

</details>
