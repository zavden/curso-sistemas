# T02 — Bloques unsafe

---

## 1. Qué es un bloque unsafe

Un bloque `unsafe { ... }` le dice al compilador: "dentro de este scope,
permito operaciones que no puedes verificar estáticamente — yo garantizo
que son correctas."

```rust
let x = 42;
let p: *const i32 = &x;

// Esto no compila:
// let v = *p;   // error: dereference of raw pointer requires unsafe

// Esto sí:
unsafe {
    let v = *p;  // el programador garantiza que p es válido
}
```

### Las 5 operaciones que requieren unsafe

`unsafe` no habilita "cualquier cosa". Solo desbloquea exactamente estas
5 operaciones:

| Operación | Ejemplo |
|-----------|---------|
| Desreferenciar raw pointer | `*p` donde `p: *mut T` |
| Llamar función `unsafe fn` | `unsafe fn dangerous()` |
| Acceder/modificar `static mut` | `static mut COUNTER: i32 = 0` |
| Implementar trait `unsafe` | `unsafe impl Send for MyType {}` |
| Acceder a campos de `union` | `union U { i: i32, f: f32 }` |

Todo lo demás — aritmética, conversión de tipos, crear raw pointers — es
válido fuera de `unsafe`. La lista es exhaustiva: si no está aquí, no
necesita `unsafe`.

---

## 2. Qué garantiza el programador

`unsafe` no desactiva las reglas de Rust — **transfiere la responsabilidad**
de verificarlas del compilador al programador. Dentro de `unsafe`, el
programador debe garantizar:

### Para desreferencia de raw pointers

1. **El puntero no es null**
2. **El puntero está alineado** para el tipo `T`
3. **Apunta a memoria inicializada y válida** que contiene un `T`
4. **No viola las reglas de aliasing**: no hay `&mut T` activo al mismo tiempo
   que otra referencia al mismo dato

### Para funciones unsafe

Las precondiciones están documentadas en la firma y en los `// SAFETY:` comments.
Cada función `unsafe` define sus propias reglas.

### Ejemplo de las consecuencias

```rust
// El programador "garantiza" que p es válido.
// Si miente, el resultado es Undefined Behavior:

let p: *const i32 = std::ptr::null();
unsafe {
    println!("{}", *p);  // UB: null dereference
}

// Puede:
// - crashear con SIGSEGV
// - imprimir basura
// - parecer que funciona (peor caso: falla meses después en producción)
```

El compilador confía ciegamente en lo que hay dentro de `unsafe`. No agrega
checks de runtime. Si las garantías son falsas, el resultado es UB — con las
mismas consecuencias que en C.

---

## 3. Minimizar el scope de unsafe

### Principio fundamental

El bloque `unsafe` debe contener **solo las operaciones que realmente necesitan
unsafe** — ni una línea más. Esto tiene dos beneficios:

1. **Facilita la auditoría**: cuando algo falla, sabes exactamente dónde mirar
2. **Reduce la superficie de UB**: menos código unsafe = menos lugares donde
   puedes equivocarte

### Mal: scope demasiado amplio

```rust
fn process_node(p: *mut Node) -> i32 {
    unsafe {
        // 20 líneas de código dentro de unsafe
        let data = (*p).data;
        let result = data * 2 + 1;        // esto no necesita unsafe
        let formatted = format!("{result}"); // esto tampoco
        println!("{formatted}");            // esto tampoco
        if result > 100 {                  // esto tampoco
            panic!("too large");
        }
        result
    }
}
```

Todo el cuerpo está en `unsafe`, pero solo `(*p).data` lo requiere. Si hay un
bug en la lógica de `result`, parece un problema de seguridad cuando en realidad
es un error lógico común.

### Bien: scope mínimo

```rust
fn process_node(p: *mut Node) -> i32 {
    // Solo la desreferencia es unsafe
    let data = unsafe { (*p).data };

    // El resto es safe — el compilador protege normalmente
    let result = data * 2 + 1;
    let formatted = format!("{result}");
    println!("{formatted}");
    if result > 100 {
        panic!("too large");
    }
    result
}
```

Ahora es claro: la única operación peligrosa es leer `(*p).data`. Si `result`
tiene un bug, es un error lógico normal, no un problema de safety.

### Regla práctica

```
Pregunta: "¿esta línea compilaría fuera de unsafe?"
  Sí → sácala del bloque unsafe
  No → déjala dentro
```

---

## 4. El comentario `// SAFETY:`

### La convención

En el ecosistema Rust, todo bloque `unsafe` debe ir precedido de un comentario
`// SAFETY:` que explica **por qué** las precondiciones se cumplen:

```rust
let node = Box::into_raw(Box::new(Node { data: 42, next: ptr::null_mut() }));

// SAFETY: node fue creado con Box::into_raw justo arriba,
// por lo tanto es no-null, alineado, y apunta a un Node válido.
let data = unsafe { (*node).data };
```

### Qué debe contener

El comentario `// SAFETY:` no repite **qué** hace el código (eso se lee), sino
**por qué es correcto**:

```rust
// MAL: describe qué hace
// SAFETY: desreferenciamos el puntero
unsafe { *p }

// BIEN: explica por qué es seguro
// SAFETY: p fue devuelto por Box::into_raw y no ha sido liberado.
// Es no-null, alineado, y nadie más tiene &mut al nodo.
unsafe { *p }
```

### Para cada una de las 5 operaciones

```rust
// 1. Desreferenciar raw pointer
// SAFETY: head fue asignado con Box::into_raw y self mantiene
// el invariante de que head es válido mientras len > 0.
let data = unsafe { (*self.head).data };

// 2. Llamar función unsafe
// SAFETY: el slice apunta a memoria válida y len no excede
// la asignación original (viene de Vec::as_mut_ptr).
let slice = unsafe { std::slice::from_raw_parts(ptr, len) };

// 3. Acceder a static mut
// SAFETY: este módulo es single-threaded; COUNTER no se accede
// desde otro thread.
unsafe { COUNTER += 1; }

// 4. Implementar trait unsafe
// SAFETY: Stack<T> no contiene Rc ni interior mutability.
// Los raw pointers se usan solo para estructura interna y
// no escapan a otros threads.
unsafe impl<T: Send> Send for Stack<T> {}

// 5. Acceder a campo de union
// SAFETY: sabemos que el último write fue al campo `i` porque
// tag == Tag::Integer (revisado en la línea anterior).
let value = unsafe { self.data.i };
```

### Clippy lo exige

El lint `clippy::undocumented_unsafe_blocks` genera un warning si un bloque
`unsafe` no tiene comentario `// SAFETY:`:

```bash
cargo clippy -- -W clippy::undocumented_unsafe_blocks
```

En proyectos serios (y en la biblioteca estándar de Rust), este lint es
obligatorio.

---

## 5. Funciones `unsafe fn`

Una función marcada como `unsafe fn` indica que **el llamador** debe cumplir
ciertas precondiciones que el compilador no puede verificar:

```rust
/// Retorna el dato del nodo apuntado por `p`.
///
/// # Safety
///
/// - `p` debe ser no-null y apuntar a un `Node` válido y alineado.
/// - El nodo no debe estar siendo mutado por otro puntero simultáneamente.
unsafe fn node_data(p: *const Node) -> i32 {
    (*p).data
}
```

### Llamar funciones unsafe

```rust
let node = Box::into_raw(Box::new(Node { data: 42, next: ptr::null_mut() }));

// SAFETY: node fue creado con Box::into_raw, es no-null y válido.
let data = unsafe { node_data(node) };
```

### Cuándo marcar una función como `unsafe fn`

```
¿La función puede causar UB si se llama con argumentos incorrectos?
  Sí → unsafe fn  (el llamador debe garantizar las precondiciones)
  No → fn normal  (la función es segura para cualquier input)
```

Ejemplo de la distinción:

```rust
// Safe: no puede causar UB sin importar qué pases
fn push(stack: &mut Stack<i32>, value: i32) {
    // ... internamente usa unsafe, pero la interfaz es safe
}

// Unsafe: si p no es válido, UB
unsafe fn read_node(p: *const Node) -> i32 {
    (*p).data
}
```

### El cuerpo de `unsafe fn`

Dentro de una `unsafe fn`, **todo el cuerpo es implícitamente unsafe** — no
necesitas bloques `unsafe` adicionales para desreferenciar punteros:

```rust
unsafe fn swap_nodes(a: *mut Node, b: *mut Node) {
    // No se necesita bloque unsafe aquí — estamos dentro de unsafe fn
    let tmp = (*a).data;
    (*a).data = (*b).data;
    (*b).data = tmp;
}
```

Sin embargo, la edición 2024 de Rust cambia esto: dentro de `unsafe fn`,
las operaciones unsafe **sí requieren** bloque `unsafe` explícito. Esto
mejora la granularidad del razonamiento de seguridad:

```rust
// Rust 2024+
unsafe fn swap_nodes(a: *mut Node, b: *mut Node) {
    // SAFETY: el llamador garantiza que a y b son válidos
    unsafe {
        let tmp = (*a).data;
        (*a).data = (*b).data;
        (*b).data = tmp;
    }
}
```

---

## 6. Unsafe no es "modo C"

### Errores conceptuales comunes

```
MITO: "unsafe desactiva el borrow checker"
REALIDAD: unsafe no cambia las reglas. Las reglas de aliasing siguen vigentes
          — el compilador simplemente no las verifica. Violarlas sigue siendo UB.

MITO: "dentro de unsafe todo vale"
REALIDAD: solo 5 operaciones se desbloquean. El type system, ownership,
          lifetimes, y el borrow checker siguen activos para todo lo demás.

MITO: "si compila con unsafe, es correcto"
REALIDAD: compilar no prueba nada sobre la corrección del código unsafe.
          Es responsabilidad del programador.
```

### Lo que sigue funcionando dentro de unsafe

```rust
unsafe {
    let x = 42;
    // let y: &str = x;  // error de tipos — sigue activo
    // let r1 = &x;
    // let r2 = &mut x;  // error de borrowing — sigue activo para refs
    // x = "hello";      // error de tipos — sigue activo
}
```

El bloque `unsafe` solo levanta las restricciones sobre las 5 operaciones
listadas. Todo lo demás sigue siendo verificado normalmente por el compilador.

### Qué SÍ desactiva unsafe respecto a raw pointers

```rust
let mut x = 42;
let p1: *mut i32 = &mut x;
let p2: *mut i32 = &mut x;  // ¡No es error! Raw pointers permiten aliasing

unsafe {
    *p1 = 100;
    *p2 = 200;  // Compila, pero ¿es correcto?
}
// El compilador no verifica si p1 y p2 crean conflicto.
// En este caso particular, ambos apuntan a x, así que es correcto
// (pero solo porque no hay &T activo simultáneamente).
```

La diferencia con `&mut`: el borrow checker **prohibiría** dos `&mut x`
simultáneos. Con `*mut`, permite el aliasing — pero la corrección sigue
siendo responsabilidad del programador.

---

## 7. Encapsulación: safe wrapper sobre unsafe interior

### El patrón correcto

La manera idiomática de usar `unsafe` en Rust es:

```
Interfaz pública:  safe  (el usuario nunca escribe unsafe)
Implementación:    unsafe (raw pointers, operaciones de bajo nivel)
Invariantes:       documentados y mantenidos por la implementación
```

### Ejemplo completo: cola circular

```rust
use std::ptr;

pub struct Queue<T> {
    head: *mut QNode<T>,
    tail: *mut QNode<T>,
    len: usize,
}

struct QNode<T> {
    data: T,
    next: *mut QNode<T>,
}

impl<T> Queue<T> {
    /// Crea una cola vacía.
    pub fn new() -> Self {
        Queue {
            head: ptr::null_mut(),
            tail: ptr::null_mut(),
            len: 0,
        }
    }

    /// Agrega un elemento al final.
    pub fn enqueue(&mut self, data: T) {
        let node = Box::into_raw(Box::new(QNode {
            data,
            next: ptr::null_mut(),
        }));

        if self.tail.is_null() {
            // Cola vacía: head y tail apuntan al nuevo nodo
            self.head = node;
        } else {
            // SAFETY: tail no es null (verificado arriba) y fue creado
            // con Box::into_raw, por lo que es válido.
            unsafe { (*self.tail).next = node; }
        }
        self.tail = node;
        self.len += 1;
    }

    /// Remueve y retorna el elemento del frente.
    pub fn dequeue(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }

        // SAFETY: head no es null (verificado arriba), fue creado con
        // Box::into_raw y no ha sido liberado.
        unsafe {
            let old_head = self.head;
            self.head = (*old_head).next;

            if self.head.is_null() {
                self.tail = ptr::null_mut(); // cola quedó vacía
            }

            let node = Box::from_raw(old_head);
            self.len -= 1;
            Some(node.data)
        }
    }

    /// Retorna una referencia al elemento del frente sin removerlo.
    pub fn front(&self) -> Option<&T> {
        if self.head.is_null() {
            None
        } else {
            // SAFETY: head no es null, fue creado con Box::into_raw,
            // y retornamos &T con lifetime ligado a &self.
            unsafe { Some(&(*self.head).data) }
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

impl<T> Drop for Queue<T> {
    fn drop(&mut self) {
        while self.dequeue().is_some() {}
    }
}
```

Observa:
- **Ningún método público requiere `unsafe`** — el usuario de `Queue<T>` no
  puede causar UB sin importar cómo llame a los métodos
- Todo bloque `unsafe` tiene su `// SAFETY:` correspondiente
- El scope de cada `unsafe` es mínimo
- `Drop` libera toda la memoria — no hay leaks
- Los invariantes (head/tail null cuando vacía, tail.next siempre null) se
  mantienen en cada método

---

## 8. `unsafe` en traits

### Implementar un trait unsafe

Algunos traits de la biblioteca estándar son `unsafe` porque el implementador
debe garantizar propiedades que el compilador no puede verificar:

```rust
// Send: "este tipo se puede enviar a otro thread de forma segura"
// Sync: "este tipo se puede compartir entre threads (&T es Send)"

pub struct Stack<T> {
    head: *mut Node<T>,
    len: usize,
}

// *mut T no es Send ni Sync por defecto.
// Pero si nuestra implementación garantiza que Stack<T> se comporta
// como si fuera un Vec<T> (ownership exclusivo, sin aliasing):

// SAFETY: Stack<T> tiene ownership exclusivo de todos los nodos.
// No hay punteros compartidos ni interior mutability.
// Es safe enviar a otro thread si T es Send.
unsafe impl<T: Send> Send for Stack<T> {}

// SAFETY: &Stack<T> solo expone &T (via peek).
// No hay mutación a través de & (no hay interior mutability).
// Es safe compartir si T es Sync.
unsafe impl<T: Sync> Sync for Stack<T> {}
```

### Definir un trait unsafe

Si creas un trait cuyas implementaciones deben cumplir invariantes no
verificables:

```rust
/// Trait para tipos que se pueden inicializar con todos los bytes a cero.
///
/// # Safety
///
/// El tipo debe ser válido cuando todos sus bytes son cero.
/// Por ejemplo, i32 sí (0 es válido), pero &T no (null no es válido).
unsafe trait ZeroInit {
    fn zero() -> Self;
}

unsafe impl ZeroInit for i32 {
    fn zero() -> Self { 0 }
}

unsafe impl ZeroInit for f64 {
    fn zero() -> Self { 0.0 }
}

// NO implementar para &T, Box<T>, bool (0 no necesariamente válido), etc.
```

---

## 9. `static mut` y por qué evitarlo

### El problema

```rust
static mut COUNTER: i32 = 0;

fn increment() {
    // SAFETY: ???
    unsafe {
        COUNTER += 1;
    }
}
```

`static mut` requiere `unsafe` porque **cualquier thread puede acceder al
mismo tiempo**, causando data races. En la práctica, es casi imposible
escribir un `// SAFETY:` correcto para `static mut` en código multi-threaded.

### La alternativa: atomics

```rust
use std::sync::atomic::{AtomicI32, Ordering};

static COUNTER: AtomicI32 = AtomicI32::new(0);

fn increment() {
    COUNTER.fetch_add(1, Ordering::SeqCst);  // safe, sin unsafe
}
```

### Otra alternativa: `Mutex` u `OnceLock`

```rust
use std::sync::Mutex;

static DATA: Mutex<Vec<String>> = Mutex::new(Vec::new());

fn add_entry(s: String) {
    DATA.lock().unwrap().push(s);  // safe
}
```

En el contexto de estructuras de datos, `static mut` aparece muy rara vez.
Se menciona aquí por completitud, pero la recomendación es: **no uses
`static mut`**. Siempre hay una alternativa safe.

---

## 10. Checklist de auditoría para bloques unsafe

Cuando escribas o revises código `unsafe`, verifica cada punto:

```
□ ¿El bloque unsafe es lo más pequeño posible?
  → Solo contiene las operaciones que realmente necesitan unsafe

□ ¿Hay un comentario // SAFETY: que explica POR QUÉ es correcto?
  → No qué hace, sino por qué las precondiciones se cumplen

□ ¿Los raw pointers desreferenciados son válidos?
  → No null, alineados, apuntan a T inicializado, no liberados

□ ¿Se respetan las reglas de aliasing?
  → No hay &mut T activo al mismo tiempo que otra referencia

□ ¿Los bounds de aritmética de punteros son correctos?
  → No se excede la asignación de memoria (incluyendo one-past-end)

□ ¿La memoria se libera exactamente una vez?
  → Todo Box::into_raw tiene su Box::from_raw correspondiente
  → Drop está implementado correctamente

□ ¿La interfaz pública es safe?
  → El usuario no puede causar UB sin escribir unsafe propio

□ ¿Se verificó con Miri?
  → cargo miri test pasa sin errores
```

---

## Ejercicios

### Ejercicio 1 — Scope mínimo

Refactoriza este código para que el bloque `unsafe` sea lo más pequeño posible:

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

fn sum_list(head: *const Node) -> i32 {
    unsafe {
        let mut total = 0;
        let mut current = head;
        while !current.is_null() {
            let data = (*current).data;
            total += data;
            if data > 100 {
                println!("WARNING: large value {data}");
            }
            current = (*current).next;
        }
        total
    }
}
```

**Prediccion**: Cuántas líneas realmente necesitan estar dentro de `unsafe`?

<details><summary>Respuesta</summary>

```rust
fn sum_list(head: *const Node) -> i32 {
    let mut total = 0;
    let mut current = head;
    while !current.is_null() {
        // SAFETY: current no es null (verificado por el while),
        // y todos los nodos fueron creados con Box::into_raw.
        let data = unsafe { (*current).data };
        total += data;
        if data > 100 {
            println!("WARNING: large value {data}");
        }
        // SAFETY: misma justificación — current es válido.
        current = unsafe { (*current).next };
    }
    total
}
```

Solo **2 líneas** necesitan `unsafe`: las dos desreferencias de `(*current)`.
Todo lo demás — la aritmética, el `println!`, el `if`, la comparación con
`is_null()` — es código safe normal.

Beneficio: si hay un bug en la lógica de `total += data`, sabes que no es un
problema de safety. Si hay un crash, sabes que es en una de las 2 líneas
marcadas.

</details>

---

### Ejercicio 2 — Escribir comentarios SAFETY

Agrega comentarios `// SAFETY:` correctos a cada bloque `unsafe`:

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct Stack<T> {
    head: *mut Node<T>,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { head: ptr::null_mut() }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let old = self.head;
            self.head = (*old).next;
            let node = Box::from_raw(old);
            Some(node.data)
        }
    }

    pub fn peek(&self) -> Option<&T> {
        if self.head.is_null() {
            None
        } else {
            unsafe { Some(&(*self.head).data) }
        }
    }
}
```

**Prediccion**: El `push` no tiene `unsafe`. Necesita un `// SAFETY:` aun así?

<details><summary>Respuesta</summary>

```rust
impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { head: ptr::null_mut() }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
        // No tiene unsafe → no necesita // SAFETY:
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        // SAFETY: head no es null (verificado arriba). Fue creado con
        // Box::into_raw en push() y no ha sido liberado. Nadie más tiene
        // &mut al nodo porque Stack tiene ownership exclusivo.
        // Box::from_raw retoma ownership para liberar la memoria del nodo.
        unsafe {
            let old = self.head;
            self.head = (*old).next;
            let node = Box::from_raw(old);
            Some(node.data)
        }
    }

    pub fn peek(&self) -> Option<&T> {
        if self.head.is_null() {
            None
        } else {
            // SAFETY: head no es null (verificado arriba). Fue asignado
            // en push() con Box::into_raw. Retornamos &T con lifetime
            // ligado a &self, lo que impide que el nodo sea liberado
            // mientras la referencia exista.
            unsafe { Some(&(*self.head).data) }
        }
    }
}
```

No, `push` no necesita `// SAFETY:` porque no contiene bloques `unsafe`.
El comentario `// SAFETY:` se asocia exclusivamente a bloques `unsafe`, no a
funciones que internamente son safe pero contribuyen a invariantes unsafe.

Sin embargo, una práctica avanzada es documentar los **invariantes del tipo**
en un comentario al inicio de la implementación:

```rust
// Invariants:
// - head is either null (empty) or points to a valid, Box-allocated Node
// - each node.next is either null or points to a valid, Box-allocated Node
// - no two Stacks share nodes
impl<T> Stack<T> { ... }
```

</details>

---

### Ejercicio 3 — Identificar operaciones unsafe

Para cada línea marcada, indica si requiere `unsafe` o no:

```rust
use std::ptr;

fn exercise() {
    let mut x: i32 = 42;
    let p: *mut i32 = &mut x;           // (a)
    let q: *const i32 = p as *const _;  // (b)
    let is_null = p.is_null();           // (c)
    let addr = p as usize;              // (d)
    let val = unsafe { *p };            // (e)
    unsafe { *p = 100; }               // (f)
    let r: *const i32 = ptr::null();    // (g)
    let eq = ptr::eq(p, q);            // (h)
}
```

**Prediccion**: Clasifica las 8 líneas como safe o unsafe antes de ver la
respuesta.

<details><summary>Respuesta</summary>

| Línea | Operación | Safe/Unsafe | Razón |
|-------|-----------|-------------|-------|
| (a) | Crear `*mut` desde `&mut` | **Safe** | Solo calcula una dirección |
| (b) | Cast `*mut` → `*const` | **Safe** | Conversión entre tipos de puntero |
| (c) | `.is_null()` | **Safe** | Compara dirección con cero, no accede memoria |
| (d) | Cast a `usize` | **Safe** | Convierte dirección a número, no accede memoria |
| (e) | `*p` (leer) | **Unsafe** | Desreferencia de raw pointer |
| (f) | `*p = 100` (escribir) | **Unsafe** | Desreferencia de raw pointer |
| (g) | `ptr::null()` | **Safe** | Crea un puntero, no desreferencia |
| (h) | `ptr::eq()` | **Safe** | Compara direcciones, no accede memoria |

Patrón: **crear, convertir y comparar** raw pointers es siempre safe.
Solo **desreferenciar** (leer o escribir a través del puntero) es unsafe.

</details>

---

### Ejercicio 4 — Unsafe fn vs safe fn con unsafe interior

Decide si cada función debería ser `unsafe fn` o `fn` con unsafe interno.
Justifica:

```rust
// Función A: retorna el dato del nodo apuntado por p
fn a(p: *const Node) -> i32 { ... }

// Función B: stack.push() — agrega elemento al stack
fn b(stack: &mut Stack<i32>, value: i32) { ... }

// Función C: lee n bytes desde un puntero arbitrario
fn c(src: *const u8, n: usize) -> Vec<u8> { ... }

// Función D: swap de datos entre dos nodos
fn d(a: *mut Node, b: *mut Node) { ... }
```

**Prediccion**: Cuáles deberían ser `unsafe fn`?

<details><summary>Respuesta</summary>

| Función | Firma | Razón |
|---------|-------|-------|
| A | `unsafe fn` | Recibe raw pointer que podría ser inválido — el llamador debe garantizar validez |
| B | `fn` (safe) | Recibe `&mut Stack` — una referencia safe. El unsafe está encapsulado dentro |
| C | `unsafe fn` | Recibe raw pointer + longitud arbitraria — el llamador debe garantizar que `src` apunta a `n` bytes válidos |
| D | `unsafe fn` | Recibe dos raw pointers — el llamador debe garantizar que ambos son válidos y no se solapan |

La regla: si la función recibe raw pointers como parámetros y los desreferencia,
generalmente debería ser `unsafe fn`. Si la función maneja los raw pointers
internamente (creándolos ella misma o recibiéndolos envueltos en un tipo safe
como `Stack`), debería ser `fn` con unsafe interno.

```rust
// A: el llamador asume la responsabilidad
unsafe fn node_data(p: *const Node) -> i32 {
    (*p).data
}

// B: la implementación asume la responsabilidad
fn push(stack: &mut Stack<i32>, value: i32) {
    let node = Box::into_raw(Box::new(Node { data: value, next: stack.head }));
    stack.head = node;
}
```

Excepción: si puedes verificar la validez del puntero (por ejemplo, con
`is_null()`), puedes hacer la función safe y manejar el error:

```rust
fn safe_node_data(p: *const Node) -> Option<i32> {
    if p.is_null() {
        None
    } else {
        // SAFETY: p no es null (verificado arriba).
        // El llamador pasó un puntero que asumimos válido si no es null.
        unsafe { Some((*p).data) }
    }
}
```

Pero esto solo cubre el caso null — no protege contra punteros no-null
pero inválidos (dangling, misaligned). Para eso, `unsafe fn` es más honesto.

</details>

---

### Ejercicio 5 — Encontrar el bug de safety

Este código compila y parece funcionar, pero tiene un bug de safety. Encuéntralo:

```rust
use std::ptr;

struct Node {
    data: String,
    next: *mut Node,
}

fn remove_head(head: &mut *mut Node) -> Option<String> {
    if head.is_null() {
        return None;
    }

    unsafe {
        let old_head = *head;
        *head = (*old_head).next;
        let data = (*old_head).data.clone();
        Some(data)
    }
}
```

**Prediccion**: El nodo removido se libera? Qué consecuencia tiene?

<details><summary>Respuesta</summary>

**Bug: memory leak**. El nodo removido (`old_head`) nunca se libera. Se lee
su dato con `.clone()`, se actualiza `head`, pero la memoria del nodo queda
en el heap sin nadie que la libere.

Versión corregida:

```rust
fn remove_head(head: &mut *mut Node) -> Option<String> {
    if head.is_null() {
        return None;
    }

    // SAFETY: head no es null (verificado arriba), fue creado con
    // Box::into_raw. Box::from_raw retoma ownership para liberar.
    unsafe {
        let old_head = *head;
        *head = (*old_head).next;
        let node = Box::from_raw(old_head);  // retoma ownership
        Some(node.data)                       // mueve data fuera del Box
    }
    // Box se destruye aquí → libera la memoria del nodo
}
```

Diferencia clave:
- Original: `(*old_head).data.clone()` — clona el dato pero el nodo queda
- Corregido: `Box::from_raw(old_head)` → `node.data` — mueve el dato y
  libera el nodo

Además, la versión original con `.clone()` requiere `T: Clone`, mientras
que la corregida funciona con cualquier `T` porque mueve en vez de clonar.

</details>

---

### Ejercicio 6 — Auditoría completa

Audita esta implementación usando el checklist de la sección 10. Lista todos
los problemas que encuentres:

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

static mut LIST_HEAD: *mut Node = ptr::null_mut();

fn push(data: i32) {
    unsafe {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: LIST_HEAD,
        }));
        LIST_HEAD = node;
    }
}

fn sum() -> i32 {
    unsafe {
        let mut total = 0;
        let mut cur = LIST_HEAD;
        while !cur.is_null() {
            total += (*cur).data;
            cur = (*cur).next;
        }
        total
    }
}
```

**Prediccion**: Cuántos problemas hay? Es solo un problema de estilo o hay
riesgo real de UB?

<details><summary>Respuesta</summary>

Problemas encontrados:

**1. `static mut` sin protección multi-thread** (riesgo de UB)

`LIST_HEAD` es mutable global. Si dos threads llaman `push()` simultáneamente,
hay data race → UB. No hay `// SAFETY:` que justifique por qué es correcto.

**2. Scope de unsafe demasiado amplio**

En `push()`, todo está dentro de `unsafe`, pero la creación del `Node` y
el `Box::new` no necesitan unsafe. Solo el acceso a `LIST_HEAD` lo requiere.

**3. Sin comentarios `// SAFETY:`**

Ninguno de los dos bloques `unsafe` tiene justificación.

**4. No hay `Drop` ni forma de liberar la memoria**

No existe función `clear()` o similar. Los nodos creados con `Box::into_raw`
nunca se liberan → memory leak permanente.

**5. Interfaz no encapsulada**

Las funciones trabajan directamente con `static mut` en vez de encapsular
el estado en un struct con métodos safe.

Versión corregida:

```rust
use std::ptr;

pub struct List {
    head: *mut Node,
}

struct Node {
    data: i32,
    next: *mut Node,
}

impl List {
    pub fn new() -> Self {
        List { head: ptr::null_mut() }
    }

    pub fn push(&mut self, data: i32) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    pub fn sum(&self) -> i32 {
        let mut total = 0;
        let mut cur = self.head;
        while !cur.is_null() {
            // SAFETY: cur no es null (verificado por while),
            // fue creado con Box::into_raw en push().
            total += unsafe { (*cur).data };
            cur = unsafe { (*cur).next };
        }
        total
    }
}

impl Drop for List {
    fn drop(&mut self) {
        let mut cur = self.head;
        while !cur.is_null() {
            // SAFETY: cur no es null, fue creado con Box::into_raw.
            unsafe {
                let next = (*cur).next;
                drop(Box::from_raw(cur));
                cur = next;
            }
        }
    }
}
```

Hay **riesgo real de UB** (problema 1: data race), no es solo estilo.

</details>

---

### Ejercicio 7 — `unsafe impl Send`

Esta estructura usa raw pointers internamente:

```rust
use std::ptr;

pub struct UniquePtr<T> {
    ptr: *mut T,
}

impl<T> UniquePtr<T> {
    pub fn new(val: T) -> Self {
        UniquePtr {
            ptr: Box::into_raw(Box::new(val)),
        }
    }

    pub fn get(&self) -> &T {
        // SAFETY: ptr fue creado con Box::into_raw en new(),
        // nunca se libera hasta Drop, y no hay aliasing mutable.
        unsafe { &*self.ptr }
    }

    pub fn get_mut(&mut self) -> &mut T {
        // SAFETY: &mut self garantiza acceso exclusivo.
        // ptr es válido (mismo razonamiento que get).
        unsafe { &mut *self.ptr }
    }
}

impl<T> Drop for UniquePtr<T> {
    fn drop(&mut self) {
        // SAFETY: ptr fue creado con Box::into_raw y no ha sido liberado.
        unsafe { drop(Box::from_raw(self.ptr)); }
    }
}
```

Decide: debería implementar `Send`? `Sync`? Escribe las implementaciones
con sus `// SAFETY:`.

**Prediccion**: `*mut T` es `Send` por defecto? Qué pasa si `T` no es `Send`?

<details><summary>Respuesta</summary>

`*mut T` **no es** `Send` ni `Sync` por defecto. Esto hace que `UniquePtr<T>`
tampoco lo sea, aunque conceptualmente se comporta como un `Box<T>` (ownership
exclusivo).

```rust
// SAFETY: UniquePtr<T> tiene ownership exclusivo del valor en el heap.
// No hay punteros compartidos ni interior mutability.
// get() retorna &T (ligado a &self) y get_mut() retorna &mut T (ligado
// a &mut self), por lo que las garantías de Rust sobre aliasing se
// mantienen a nivel de la interfaz pública.
// Es safe enviar a otro thread si T es Send.
unsafe impl<T: Send> Send for UniquePtr<T> {}

// SAFETY: &UniquePtr<T> solo expone &T (via get()).
// No hay forma de mutar a través de una referencia compartida.
// Es safe compartir si T es Sync.
unsafe impl<T: Sync> Sync for UniquePtr<T> {}
```

Notas importantes:

- Si `T` no es `Send`, `UniquePtr<T>` **no debe ser `Send`**. Por ejemplo,
  `Rc<i32>` no es `Send` — si lo envuelves en `UniquePtr<Rc<i32>>`, enviarlo
  a otro thread podría corromper el conteo de referencias.

- El bound `T: Send` (no `T: Send + Sync`) es correcto para `Send` porque
  `UniquePtr` tiene ownership exclusivo — no comparte `T` con nadie.

- El bound `T: Sync` es correcto para `Sync` porque `&UniquePtr<T>` solo
  permite `&T`, y `T: Sync` significa que `&T` es safe entre threads.

Esto es exactamente lo que `Box<T>` hace internamente.

</details>

---

### Ejercicio 8 — El error del `unsafe` innecesario

Identifica qué está mal en este código. No tiene bugs de memoria, pero
tiene un problema de diseño:

```rust
fn find_max(arr: &[i32]) -> Option<i32> {
    if arr.is_empty() {
        return None;
    }

    unsafe {
        let mut max = arr[0];
        for i in 1..arr.len() {
            if arr[i] > max {
                max = arr[i];
            }
        }
        Some(max)
    }
}
```

**Prediccion**: El `unsafe` causa UB? Si no, por qué es un problema?

<details><summary>Respuesta</summary>

El `unsafe` **no causa UB** — todo el código dentro del bloque es perfectamente
safe. Acceso a slices con `arr[i]`, comparaciones, asignaciones — nada de esto
requiere `unsafe`.

El problema: **`unsafe` innecesario**. Esto es peor que inofensivo porque:

1. **Señal falsa**: quien audita el código busca bloques `unsafe` para encontrar
   posibles fuentes de UB. Este bloque es una falsa alarma que desperdicia
   tiempo de revisión.

2. **Erosión de confianza**: si hay `unsafe` donde no hace falta, el revisor
   cuestiona si el autor entiende qué es unsafe, lo que reduce la confianza
   en los bloques `unsafe` que sí son necesarios.

3. **Riesgo futuro**: si alguien agrega código dentro del bloque `unsafe`
   después, podría no darse cuenta de que está en un contexto unsafe y
   agregar operaciones peligrosas sin pensarlo.

Versión correcta:

```rust
fn find_max(arr: &[i32]) -> Option<i32> {
    if arr.is_empty() {
        return None;
    }
    let mut max = arr[0];
    for i in 1..arr.len() {
        if arr[i] > max {
            max = arr[i];
        }
    }
    Some(max)
}

// O más idiomático:
fn find_max_idiomatic(arr: &[i32]) -> Option<i32> {
    arr.iter().copied().max()
}
```

Regla: **nunca uses `unsafe` si no necesitas una de las 5 operaciones**.
El bloque `unsafe` no es un "permiso general" — es una declaración de que
estás haciendo algo que el compilador no puede verificar.

</details>

---

### Ejercicio 9 — Union y unsafe

Implementa un `IntOrFloat` usando `union` que almacene un `i32` o un `f32`
en la misma memoria. Incluye un tag para saber qué tipo contiene:

```rust
union IntOrFloatData {
    i: i32,
    f: f32,
}

enum Tag {
    Int,
    Float,
}

struct IntOrFloat {
    tag: Tag,
    data: IntOrFloatData,
}

impl IntOrFloat {
    fn new_int(val: i32) -> Self { todo!() }
    fn new_float(val: f32) -> Self { todo!() }
    fn get_int(&self) -> Option<i32> { todo!() }
    fn get_float(&self) -> Option<f32> { todo!() }
}
```

**Prediccion**: Acceder a `self.data.i` cuando el tag es `Float` compila?
Es UB?

<details><summary>Respuesta</summary>

```rust
union IntOrFloatData {
    i: i32,
    f: f32,
}

#[derive(PartialEq)]
enum Tag {
    Int,
    Float,
}

struct IntOrFloat {
    tag: Tag,
    data: IntOrFloatData,
}

impl IntOrFloat {
    fn new_int(val: i32) -> Self {
        IntOrFloat {
            tag: Tag::Int,
            data: IntOrFloatData { i: val },
        }
    }

    fn new_float(val: f32) -> Self {
        IntOrFloat {
            tag: Tag::Float,
            data: IntOrFloatData { f: val },
        }
    }

    fn get_int(&self) -> Option<i32> {
        if self.tag == Tag::Int {
            // SAFETY: tag es Int, por lo que el último write fue al campo i.
            // Leer el campo correcto es válido.
            unsafe { Some(self.data.i) }
        } else {
            None
        }
    }

    fn get_float(&self) -> Option<f32> {
        if self.tag == Tag::Float {
            // SAFETY: tag es Float, por lo que el último write fue al campo f.
            unsafe { Some(self.data.f) }
        } else {
            None
        }
    }
}

fn main() {
    let a = IntOrFloat::new_int(42);
    let b = IntOrFloat::new_float(3.14);

    assert_eq!(a.get_int(), Some(42));
    assert_eq!(a.get_float(), None);
    assert_eq!(b.get_float(), Some(3.14));
    assert_eq!(b.get_int(), None);
    println!("All tests passed!");
}
```

Acceder a `self.data.i` cuando el tag es `Float` **sí compila** (el compilador
no verifica correspondencia entre tag y campo), pero es **UB** en el sentido
general: interpretas los bits de un `f32` como `i32`. En Rust, para tipos
primitivos como `i32`/`f32` que tienen el mismo tamaño y todos los bit-patterns
son válidos, no es UB estricto — es una "transmute" de bits. Pero para tipos
con invariantes (como `bool`, donde solo 0 y 1 son válidos), leer el campo
equivocado sí sería UB.

El patrón tag + union es exactamente cómo funcionan los tagged unions en C.
En Rust idiomático, usarías un `enum`:

```rust
enum IntOrFloat {
    Int(i32),
    Float(f32),
}
```

El `enum` de Rust hace lo mismo (tag + datos) pero de forma safe. Las unions
se usan solo para FFI con C o para optimizaciones muy específicas.

</details>

---

### Ejercicio 10 — Refactorizar a safe wrapper

Toma este código que usa `unsafe` directamente en `main` y refactorízalo en
un struct con interfaz completamente safe:

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

fn main() {
    let mut head: *mut Node = ptr::null_mut();

    // Push 3 elementos
    for val in [10, 20, 30] {
        let node = Box::into_raw(Box::new(Node { data: val, next: head }));
        head = node;
    }

    // Imprimir
    let mut cur = head;
    while !cur.is_null() {
        unsafe {
            print!("{} ", (*cur).data);
            cur = (*cur).next;
        }
    }
    println!();

    // Liberar
    while !head.is_null() {
        unsafe {
            let next = (*head).next;
            drop(Box::from_raw(head));
            head = next;
        }
    }
}
```

**Prediccion**: En tu versión refactorizada, cuántos bloques `unsafe` tendrá
`main`?

<details><summary>Respuesta</summary>

```rust
use std::ptr;
use std::fmt;

struct Node {
    data: i32,
    next: *mut Node,
}

pub struct IntList {
    head: *mut Node,
}

impl IntList {
    pub fn new() -> Self {
        IntList { head: ptr::null_mut() }
    }

    pub fn push(&mut self, data: i32) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    pub fn iter(&self) -> ListIter {
        ListIter { current: self.head }
    }
}

impl Drop for IntList {
    fn drop(&mut self) {
        let mut cur = self.head;
        while !cur.is_null() {
            // SAFETY: cur no es null (verificado por while),
            // fue creado con Box::into_raw en push().
            unsafe {
                let next = (*cur).next;
                drop(Box::from_raw(cur));
                cur = next;
            }
        }
    }
}

impl fmt::Display for IntList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut first = true;
        for val in self.iter() {
            if !first { write!(f, " ")?; }
            write!(f, "{val}")?;
            first = false;
        }
        Ok(())
    }
}

pub struct ListIter {
    current: *mut Node,
}

impl Iterator for ListIter {
    type Item = i32;

    fn next(&mut self) -> Option<i32> {
        if self.current.is_null() {
            None
        } else {
            // SAFETY: current no es null (verificado arriba),
            // fue creado con Box::into_raw.
            unsafe {
                let data = (*self.current).data;
                self.current = (*self.current).next;
                Some(data)
            }
        }
    }
}

fn main() {
    let mut list = IntList::new();

    for val in [10, 20, 30] {
        list.push(val);
    }

    println!("{list}");
    // Salida: 30 20 10

    // Drop automático al salir de scope — sin unsafe en main
}
```

`main` tiene **cero** bloques `unsafe`. Todo el unsafe está encapsulado dentro
de `IntList`, `Drop`, y `ListIter`. El usuario de la API no puede causar UB.

Este es el patrón fundamental: el `unsafe` es un **detalle de implementación**,
no parte de la interfaz.

</details>
