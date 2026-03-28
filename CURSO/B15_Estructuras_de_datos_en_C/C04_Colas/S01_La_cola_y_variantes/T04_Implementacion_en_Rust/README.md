# Implementación de colas en Rust

## VecDeque: la cola de la biblioteca estándar

Rust incluye `std::collections::VecDeque<T>`, un **buffer circular
redimensionable** que soporta inserción y eliminación eficiente en ambos
extremos.  Es la implementación recomendada para colas en Rust.

```rust
use std::collections::VecDeque;

fn main() {
    let mut queue: VecDeque<i32> = VecDeque::new();

    // Enqueue (insertar al final)
    queue.push_back(10);
    queue.push_back(20);
    queue.push_back(30);

    // Front (observar sin extraer)
    println!("front: {:?}", queue.front());   // Some(10)

    // Dequeue (extraer del frente)
    println!("dequeue: {:?}", queue.pop_front());  // Some(10)
    println!("dequeue: {:?}", queue.pop_front());  // Some(20)

    println!("size: {}", queue.len());        // 1
    println!("empty: {}", queue.is_empty());  // false

    // Dequeue en cola vacía
    queue.pop_front();
    println!("dequeue vacía: {:?}", queue.pop_front());  // None
}
```

### Correspondencia con el TAD

| TAD Cola | `VecDeque<T>` | Retorno |
|----------|---------------|---------|
| `enqueue(x)` | `push_back(x)` | `()` |
| `dequeue()` | `pop_front()` | `Option<T>` |
| `front()` | `front()` | `Option<&T>` |
| `is_empty()` | `is_empty()` | `bool` |
| `size()` | `len()` | `usize` |
| `create()` | `VecDeque::new()` | `VecDeque<T>` |
| `destroy()` | Automático (`Drop`) | — |

`VecDeque` también soporta operaciones en el otro extremo (`push_front`,
`pop_back`, `back`) — es un **deque** completo.  Lo veremos en S02/T02.

### Internamente: buffer circular

`VecDeque` usa exactamente el mismo diseño que implementamos en C (T02): un
array en el heap con índices `head` y `len`, wrap-around con máscara de bits
(la capacidad siempre es potencia de 2).

```
VecDeque con elementos [A, B, C], capacidad 8:

       head                    head + len
         ↓                        ↓
┌───┬───┬───┬───┬───┬───┬───┬───┐
│   │   │ A │ B │ C │   │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
  0   1   2   3   4   5   6   7

Índice físico = (head + índice_lógico) & (cap - 1)
```

Cuando se llena, `VecDeque` redimensiona automáticamente (duplica la capacidad
y lineariza los datos — el mismo proceso que vimos en T02, pero implementado
correctamente por la stdlib).

### Crear con capacidad inicial

```rust
let mut queue = VecDeque::with_capacity(1000);
```

Evita redimensionamientos si conoces el tamaño aproximado.  No afecta la
corrección, solo el rendimiento.

---

## Métodos útiles de VecDeque

Más allá de las operaciones básicas de cola:

```rust
use std::collections::VecDeque;

fn main() {
    let mut q = VecDeque::from([10, 20, 30, 40, 50]);

    // Acceso por índice — O(1)
    println!("q[2] = {}", q[2]);                // 30

    // Iteración
    for val in &q {
        print!("{val} ");                        // 10 20 30 40 50
    }
    println!();

    // Contiene
    println!("contains 30: {}", q.contains(&30));  // true

    // Convertir a Vec (lineariza)
    let v: Vec<i32> = q.into_iter().collect();
    println!("{v:?}");                           // [10, 20, 30, 40, 50]
}
```

### Métodos relevantes para colas

| Método | Descripción | Complejidad |
|--------|-------------|-------------|
| `push_back(val)` | Enqueue | $O(1)$ amortizado |
| `pop_front()` | Dequeue | $O(1)$ amortizado |
| `front()` / `front_mut()` | Referencia al frente | $O(1)$ |
| `back()` / `back_mut()` | Referencia al final | $O(1)$ |
| `len()` | Tamaño | $O(1)$ |
| `is_empty()` | Vacía | $O(1)$ |
| `contains(&val)` | Búsqueda lineal | $O(n)$ |
| `iter()` | Iterador en orden | — |
| `drain(..)` | Extraer rango, liberar | $O(k)$ |
| `make_contiguous()` | Linearizar datos en memoria | $O(n)$ |
| `retain(f)` | Eliminar elementos que no cumplan `f` | $O(n)$ |

---

## Implementación manual con Vec

Para entender la mecánica, implementemos una cola con `Vec<T>` como buffer
circular, replicando en Rust lo que hicimos en C (T02):

```rust
struct CircularQueue<T> {
    data: Vec<Option<T>>,    // Option permite "vaciar" slots sin Copy
    head: usize,             // índice del frente
    count: usize,            // elementos actuales
}

impl<T> CircularQueue<T> {
    fn new(capacity: usize) -> Self {
        assert!(capacity > 0);
        let mut data = Vec::with_capacity(capacity);
        for _ in 0..capacity {
            data.push(None);
        }
        CircularQueue { data, head: 0, count: 0 }
    }

    fn capacity(&self) -> usize {
        self.data.len()
    }

    fn len(&self) -> usize {
        self.count
    }

    fn is_empty(&self) -> bool {
        self.count == 0
    }

    fn is_full(&self) -> bool {
        self.count == self.capacity()
    }

    fn enqueue(&mut self, value: T) -> Result<(), T> {
        if self.is_full() {
            return Err(value);    // devolver el valor rechazado
        }
        let rear = (self.head + self.count) % self.capacity();
        self.data[rear] = Some(value);
        self.count += 1;
        Ok(())
    }

    fn dequeue(&mut self) -> Option<T> {
        if self.is_empty() {
            return None;
        }
        let value = self.data[self.head].take();   // toma el valor, deja None
        self.head = (self.head + 1) % self.capacity();
        self.count -= 1;
        value
    }

    fn front(&self) -> Option<&T> {
        if self.is_empty() {
            return None;
        }
        self.data[self.head].as_ref()
    }
}

fn main() {
    let mut q = CircularQueue::new(4);

    q.enqueue(10).unwrap();
    q.enqueue(20).unwrap();
    q.enqueue(30).unwrap();

    println!("front: {:?}", q.front());        // Some(10)
    println!("dequeue: {:?}", q.dequeue());    // Some(10)

    q.enqueue(40).unwrap();
    q.enqueue(50).unwrap();

    // Cola llena (cap=4): 20, 30, 40, 50
    println!("enqueue full: {:?}", q.enqueue(60));  // Err(60)

    while let Some(val) = q.dequeue() {
        print!("{val} ");   // 20 30 40 50
    }
    println!();
}
```

### Decisiones de diseño en Rust

| Decisión | Motivación |
|----------|-----------|
| `Vec<Option<T>>` | `Option::take()` permite extraer el valor sin `Copy`/`Clone` |
| `Result<(), T>` para enqueue lleno | Devuelve el ownership del valor rechazado al llamador |
| `Option<T>` para dequeue | Idiomático en Rust — obliga a manejar el caso vacío |
| `Option<&T>` para front | Referencia sin transferir ownership |
| `assert!(capacity > 0)` | Capacidad 0 causaría división por cero en módulo |

### Por qué Option<T> y no un valor "basura"

En C usamos un array de `int` y los slots vacíos contenían basura — no
importaba porque nunca los leíamos.  En Rust, `T` puede no ser `Copy` (strings,
vecs, structs con recursos).  No podemos dejar "basura" ni copiar sin
permiso.  `Option<T>` resuelve esto:

- `Some(value)`: slot ocupado, contiene el valor.
- `None`: slot vacío, sin valor.
- `take()`: extrae el `Some(value)` y deja `None` — sin `Clone`, sin copia.

```rust
let mut slot: Option<String> = Some("hello".to_string());
let value = slot.take();   // value = Some("hello"), slot = None
// El String se movió, no se copió
```

---

## Implementación manual con lista enlazada

La lista enlazada en Rust es más compleja que en C por las reglas de ownership.
Usamos `Box<Node<T>>` para los nodos en el heap:

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}

struct LinkedQueue<T> {
    head: Option<Box<Node<T>>>,
    tail: *mut Node<T>,        // raw pointer al último nodo
    count: usize,
}

impl<T> LinkedQueue<T> {
    fn new() -> Self {
        LinkedQueue {
            head: None,
            tail: std::ptr::null_mut(),
            count: 0,
        }
    }

    fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    fn len(&self) -> usize {
        self.count
    }

    fn enqueue(&mut self, value: T) {
        let mut new_node = Box::new(Node { data: value, next: None });
        let raw: *mut Node<T> = &mut *new_node;

        if self.tail.is_null() {
            self.head = Some(new_node);
        } else {
            unsafe { (*self.tail).next = Some(new_node); }
        }
        self.tail = raw;
        self.count += 1;
    }

    fn dequeue(&mut self) -> Option<T> {
        self.head.take().map(|old_head| {
            self.head = old_head.next;
            if self.head.is_none() {
                self.tail = std::ptr::null_mut();
            }
            self.count -= 1;
            old_head.data
        })
    }

    fn front(&self) -> Option<&T> {
        self.head.as_ref().map(|node| &node.data)
    }
}

impl<T> Drop for LinkedQueue<T> {
    fn drop(&mut self) {
        while self.dequeue().is_some() {}
    }
}

fn main() {
    let mut q = LinkedQueue::new();

    q.enqueue("alpha".to_string());
    q.enqueue("beta".to_string());
    q.enqueue("gamma".to_string());

    println!("front: {:?}", q.front());         // Some("alpha")
    println!("dequeue: {:?}", q.dequeue());     // Some("alpha")
    println!("dequeue: {:?}", q.dequeue());     // Some("beta")
    println!("len: {}", q.len());               // 1
}   // Drop llama dequeue repetidamente → libera "gamma"
```

### El problema del puntero tail

En C, mantener un puntero `rear` al último nodo es trivial.  En Rust, es el
mayor desafío:

- `head` posee la cadena de nodos (`Box` = ownership).
- `tail` necesita apuntar al último nodo, pero ese nodo ya es propiedad de
  algún `Box` en la cadena.
- Rust no permite dos `&mut` al mismo dato.

**Soluciones posibles:**

| Enfoque | Mecanismo | Complejidad | Seguridad |
|---------|-----------|-------------|-----------|
| Raw pointer (`*mut`) | Puntero sin borrow checker | Baja | `unsafe` — el programador garantiza validez |
| `Rc<RefCell<Node>>` | Reference counting + mutabilidad interior | Alta | Safe, pero overhead en runtime |
| Sin `tail` | Recorrer hasta el final en cada enqueue | Baja | Safe, pero enqueue es $O(n)$ |
| Usar `VecDeque` | Delegar a la stdlib | Ninguna | Safe y eficiente |

La implementación con raw pointer es la más fiel al diseño de C y la más
eficiente, pero requiere `unsafe`.  Analicemos por qué es correcta:

### Seguridad del raw pointer tail

El `unsafe` en nuestra implementación es seguro porque mantenemos estos
**invariantes**:

1. `tail` siempre apunta al último nodo de la cadena, o es `null` si la cola
   está vacía.
2. El nodo al que apunta `tail` siempre existe (es propiedad de algún `Box` en
   la cadena desde `head`).
3. Solo accedemos a `tail` para modificar `next` del último nodo — nunca
   leemos ni movemos el nodo a través de `tail`.
4. Cuando se elimina el último nodo (dequeue que deja la cola vacía), `tail`
   se pone a `null` inmediatamente.

Si alguno de estos invariantes se rompiera, tendríamos un dangling pointer —
el mismo riesgo que en C, pero localizado en un bloque `unsafe` explícito.

---

## Cola sin unsafe: solo con head

Si queremos evitar `unsafe` completamente, podemos prescindir de `tail` y
recorrer la lista para encontrar el final:

```rust
struct SafeQueue<T> {
    head: Option<Box<Node<T>>>,
    count: usize,
}

impl<T> SafeQueue<T> {
    fn new() -> Self {
        SafeQueue { head: None, count: 0 }
    }

    fn enqueue(&mut self, value: T) {
        let new_node = Box::new(Node { data: value, next: None });

        if self.head.is_none() {
            self.head = Some(new_node);
        } else {
            // Recorrer hasta el final — O(n)
            let mut current = self.head.as_mut().unwrap();
            while current.next.is_some() {
                current = current.next.as_mut().unwrap();
            }
            current.next = Some(new_node);
        }
        self.count += 1;
    }

    fn dequeue(&mut self) -> Option<T> {
        self.head.take().map(|old_head| {
            self.head = old_head.next;
            self.count -= 1;
            old_head.data
        })
    }

    fn front(&self) -> Option<&T> {
        self.head.as_ref().map(|node| &node.data)
    }
}
```

`enqueue` es $O(n)$ — inaceptable para una cola real, pero demuestra que
la limitación no es del algoritmo sino del modelo de ownership de Rust.  En la
práctica, usa `VecDeque`.

---

## Comparación de las tres implementaciones

| Aspecto | `VecDeque<T>` | `CircularQueue<T>` manual | `LinkedQueue<T>` |
|---------|---------------|---------------------------|-------------------|
| Enqueue | $O(1)$ amort. | $O(1)$ | $O(1)$ con unsafe |
| Dequeue | $O(1)$ amort. | $O(1)$ | $O(1)$ |
| Capacidad | Dinámica | Fija | Dinámica |
| Memoria extra | Buffer + metadata | `Vec<Option<T>>` | `Box` + puntero por nodo |
| `unsafe` | No (encapsulado en stdlib) | No | Sí (raw pointer tail) |
| Caché | Excelente | Excelente | Pobre |
| Genérico | Sí | Sí | Sí |
| `Drop` automático | Sí | Sí (Vec + Option) | Manual (`impl Drop`) |
| Recomendado para | Producción | Aprendizaje | Aprendizaje |

### Recomendación práctica

```
¿Necesitas una cola en Rust?
    │
    ├── Sí, en producción → VecDeque<T>
    │
    └── Sí, para aprender
        ├── Ownership y genéricos → CircularQueue<T> manual
        └── Unsafe y raw pointers → LinkedQueue<T>
```

---

## Comparación C vs Rust

### Manejo de cola vacía

```c
/* C: el llamador puede ignorar el retorno */
int val;
queue_dequeue(&q, &val);    /* ¿chequeó el bool? */
printf("%d\n", val);        /* si era vacía, val es basura */
```

```rust
// Rust: el compilador obliga a manejar None
match queue.pop_front() {
    Some(val) => println!("{val}"),
    None      => println!("cola vacía"),
}

// O con if let:
if let Some(val) = queue.pop_front() {
    println!("{val}");
}
```

### Gestión de memoria

```c
/* C: manual — olvidar free es un leak, doble free es UB */
Queue q;
queue_init(&q);
queue_enqueue(&q, 42);
/* ... */
queue_destroy(&q);    /* obligatorio — fácil de olvidar */
```

```rust
// Rust: automático via Drop
{
    let mut q = VecDeque::new();
    q.push_back(42);
}   // Drop se ejecuta aquí — sin leak posible
```

### Genéricos

```c
/* C: void* — sin type safety */
GenericQueue *q = gqueue_create(sizeof(int));
int x = 42;
gqueue_enqueue(q, &x);
double y;
gqueue_dequeue(q, &y);    /* compila, pero UB (tipo incorrecto) */
```

```rust
// Rust: genéricos con verificación en compilación
let mut q: VecDeque<i32> = VecDeque::new();
q.push_back(42);
// q.push_back("hello");  // error de compilación
let val: i32 = q.pop_front().unwrap();
```

### Tabla resumen

| Aspecto | C | Rust |
|---------|---|------|
| Cola vacía | Bool ignorable / valor basura | `Option<T>` obligatorio |
| Memoria | Manual (`malloc`/`free`) | Automática (`Drop`) |
| Genéricos | `void*` (inseguro) | `<T>` (verificado en compilación) |
| Dangling pointers | Posibles | Imposibles en safe Rust |
| Buffer overflow | Posible (sin bounds check) | Panic en debug, bounds check |
| Rendimiento | Máximo (sin overhead) | Equivalente (bounds check eliminable con `-O`) |
| Concurrencia | Nada impide data races | `Send`/`Sync` verifican seguridad |

---

## Iteración sobre VecDeque

`VecDeque` implementa los traits de iteración estándar:

```rust
use std::collections::VecDeque;

fn main() {
    let q = VecDeque::from([10, 20, 30, 40]);

    // Iteración por referencia (no consume la cola)
    for val in &q {
        print!("{val} ");   // 10 20 30 40
    }
    println!();

    // Iteración con índice
    for (i, val) in q.iter().enumerate() {
        println!("  q[{i}] = {val}");
    }

    // Iteración consumidora (mueve la cola)
    let sum: i32 = q.into_iter().sum();
    println!("sum = {sum}");   // 100
    // q ya no existe aquí
}
```

### Implementar Iterator para CircularQueue

Para que nuestra cola manual sea iterable:

```rust
struct CircularQueueIter<'a, T> {
    queue: &'a CircularQueue<T>,
    index: usize,
}

impl<'a, T> Iterator for CircularQueueIter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index >= self.queue.count {
            return None;
        }
        let phys = (self.queue.head + self.index) % self.queue.capacity();
        self.index += 1;
        self.queue.data[phys].as_ref()
    }
}

impl<T> CircularQueue<T> {
    fn iter(&self) -> CircularQueueIter<'_, T> {
        CircularQueueIter { queue: self, index: 0 }
    }
}
```

Ahora funciona `for val in q.iter()` y todos los adaptadores de iterador
(`map`, `filter`, `sum`, etc.).

---

## VecDeque como cola doble, pila, y cola

`VecDeque` es versátil: según qué métodos uses, se comporta como diferentes
TADs:

| TAD | Inserción | Extracción |
|-----|-----------|------------|
| **Cola** (FIFO) | `push_back` | `pop_front` |
| **Pila** (LIFO) | `push_back` | `pop_back` |
| **Deque** | `push_back` / `push_front` | `pop_front` / `pop_back` |

Esto hace que `VecDeque` sea la estructura más flexible de la stdlib para
colecciones lineales.  Un `Vec<T>` solo es eficiente como pila (`push`/`pop`
al final); para cola necesita `remove(0)` que es $O(n)$.

```rust
// Vec como cola: INEFICIENTE
let mut bad_queue = vec![1, 2, 3];
bad_queue.push(4);         // enqueue: O(1) — ok
bad_queue.remove(0);       // dequeue: O(n) — desplaza todo

// VecDeque como cola: EFICIENTE
let mut good_queue = VecDeque::from([1, 2, 3]);
good_queue.push_back(4);   // enqueue: O(1)
good_queue.pop_front();    // dequeue: O(1)
```

---

## Ejercicios

### Ejercicio 1 — VecDeque básico

Escribe un programa que use `VecDeque` para simular una cola de impresión.
Encola 5 documentos (strings), luego imprímelos en orden FIFO.  Muestra el
tamaño de la cola después de cada operación.

<details>
<summary>Código base</summary>

```rust
use std::collections::VecDeque;

fn main() {
    let mut printer: VecDeque<String> = VecDeque::new();
    let docs = ["report.pdf", "photo.jpg", "slides.pptx",
                "readme.md", "data.csv"];

    for doc in &docs {
        printer.push_back(doc.to_string());
        println!("Queued: {doc}  (pending: {})", printer.len());
    }

    while let Some(doc) = printer.pop_front() {
        println!("Printing: {doc}  (remaining: {})", printer.len());
    }
}
```
</details>

### Ejercicio 2 — CircularQueue con redimensionamiento

Extiende la `CircularQueue<T>` manual para que se redimensione automáticamente
(duplique capacidad) cuando esté llena.  El `enqueue` nunca debe fallar (excepto
por falta de memoria).  Verifica con 100 enqueues empezando con capacidad 4.

<details>
<summary>Pista para redimensionar</summary>

```rust
fn grow(&mut self) {
    let new_cap = self.capacity() * 2;
    let mut new_data = Vec::with_capacity(new_cap);
    // Copiar en orden lógico (from head, count elements)
    for i in 0..self.count {
        let phys = (self.head + i) % self.capacity();
        new_data.push(self.data[phys].take());
    }
    // Rellenar el resto con None
    for _ in self.count..new_cap {
        new_data.push(None);
    }
    self.data = new_data;
    self.head = 0;
}
```

Misma lógica que la linearización en C (T02) — mover elementos en orden lógico
al nuevo buffer.
</details>

### Ejercicio 3 — Cola de tipos mixtos con enum

Usa un enum para crear una cola que pueda contener diferentes tipos de datos:

```rust
enum Task {
    Print(String),
    Calculate(f64, f64),
    Notify(String, u32),
}
```

Encola varias tareas y procésalas con `match` al desencolar.

<details>
<summary>Patrón de procesamiento</summary>

```rust
while let Some(task) = queue.pop_front() {
    match task {
        Task::Print(doc) => println!("Printing: {doc}"),
        Task::Calculate(a, b) => println!("Result: {}", a + b),
        Task::Notify(msg, id) => println!("User {id}: {msg}"),
    }
}
```

Este es el patrón de **message queue** — cada variante del enum es un tipo de
mensaje, y el consumidor despacha según el tipo.
</details>

### Ejercicio 4 — BFS con VecDeque

Implementa BFS para encontrar el camino más corto en el laberinto de T01,
usando `VecDeque` como cola.  Compara la claridad del código con la versión C.

<details>
<summary>Estructura sugerida</summary>

```rust
use std::collections::VecDeque;

let mut queue = VecDeque::new();
queue.push_back((start_r, start_c));

while let Some((r, c)) = queue.pop_front() {
    // explorar vecinos
    for (dr, dc) in &[(-1i32,0),(1,0),(0,-1),(0,1)] {
        // ...
        queue.push_back((nr, nc));
    }
}
```

`while let Some(...)` reemplaza el patrón `while (!empty) { dequeue }` de C.
</details>

### Ejercicio 5 — Comparar Vec vs VecDeque como cola

Mide el tiempo de $10^5$ operaciones `push_back` + `remove(0)` con `Vec<i32>`
vs $10^5$ operaciones `push_back` + `pop_front` con `VecDeque<i32>`.

<details>
<summary>Predicción</summary>

`Vec::remove(0)` es $O(n)$ porque desplaza todos los elementos.  Para $10^5$
operaciones: $\sim 10^{10}$ movimientos en total ($O(n^2)$ total).  Debería
tardar segundos.

`VecDeque::pop_front` es $O(1)$.  Total: $10^5$ operaciones — microsegundos.

Diferencia esperada: 10,000× o más.  Esta es la razón de existir de `VecDeque`.
</details>

### Ejercicio 6 — Drop personalizado

Implementa `LinkedQueue<T>` (la versión con raw pointer) y verifica que `Drop`
libera toda la memoria.  Usa un tipo que imprima un mensaje al destruirse:

```rust
struct Loud(i32);
impl Drop for Loud {
    fn drop(&mut self) { println!("Dropping {}", self.0); }
}
```

<details>
<summary>Qué observar</summary>

Al salir del scope de la cola, `Drop` de `LinkedQueue` llama `dequeue`
repetidamente.  Cada `dequeue` destruye el `Box<Node<Loud>>`, que destruye
el `Loud`, que imprime "Dropping N".  Deberías ver un mensaje por cada
elemento encolado, en orden FIFO.
</details>

### Ejercicio 7 — Iterator para LinkedQueue

Implementa `Iterator` para `LinkedQueue<T>` (iteración por referencia, sin
consumir la cola).  Luego usa `.iter().sum()` para sumar los elementos.

<details>
<summary>Pista</summary>

```rust
struct LinkedQueueIter<'a, T> {
    current: Option<&'a Node<T>>,
}

impl<'a, T> Iterator for LinkedQueueIter<'a, T> {
    type Item = &'a T;
    fn next(&mut self) -> Option<Self::Item> {
        self.current.map(|node| {
            self.current = node.next.as_deref();
            &node.data
        })
    }
}

impl<T> LinkedQueue<T> {
    fn iter(&self) -> LinkedQueueIter<'_, T> {
        LinkedQueueIter {
            current: self.head.as_deref(),
        }
    }
}
```

`as_deref()` convierte `Option<Box<Node<T>>>` en `Option<&Node<T>>`.
</details>

### Ejercicio 8 — Cola con capacidad máxima (wrapper)

Crea un wrapper sobre `VecDeque<T>` que rechace enqueue cuando se alcance una
capacidad máxima:

```rust
struct BoundedQueue<T> {
    inner: VecDeque<T>,
    max_capacity: usize,
}
```

Implementa `enqueue` que retorne `Result<(), T>` y `dequeue` que retorne
`Option<T>`.

<details>
<summary>Implementación</summary>

```rust
impl<T> BoundedQueue<T> {
    fn new(max_capacity: usize) -> Self {
        BoundedQueue {
            inner: VecDeque::with_capacity(max_capacity),
            max_capacity,
        }
    }

    fn enqueue(&mut self, value: T) -> Result<(), T> {
        if self.inner.len() >= self.max_capacity {
            Err(value)
        } else {
            self.inner.push_back(value);
            Ok(())
        }
    }

    fn dequeue(&mut self) -> Option<T> {
        self.inner.pop_front()
    }
}
```
</details>

### Ejercicio 9 — Hot potato con VecDeque

Reimplementa el problema de Josephus (ejercicio 10 de T01) usando `VecDeque`.
La rotación es `pop_front` + `push_back`.

<details>
<summary>Código</summary>

```rust
use std::collections::VecDeque;

fn josephus(n: usize, k: usize) -> usize {
    let mut circle: VecDeque<usize> = (1..=n).collect();
    while circle.len() > 1 {
        for _ in 0..k - 1 {
            let person = circle.pop_front().unwrap();
            circle.push_back(person);   // rotar
        }
        let eliminated = circle.pop_front().unwrap();
        println!("Eliminated: {eliminated}");
    }
    circle[0]
}
```

`VecDeque` hace la rotación naturalmente con `pop_front`/`push_back` en $O(1)$
cada una.  `VecDeque` también tiene `rotate_left(k)` que hace lo mismo en
$O(\min(k, n-k))$.
</details>

### Ejercicio 10 — Cola thread-safe

Envuelve una `VecDeque<T>` en `Mutex<VecDeque<T>>` y lanza 4 hilos productores
y 2 hilos consumidores.  Cada productor encola 100 números; cada consumidor
desencola hasta que la cola esté vacía y todos los productores hayan terminado.

<details>
<summary>Estructura</summary>

```rust
use std::sync::{Arc, Mutex};
use std::thread;

let queue = Arc::new(Mutex::new(VecDeque::new()));

// Productor
let q = Arc::clone(&queue);
thread::spawn(move || {
    for i in 0..100 {
        q.lock().unwrap().push_back(i);
    }
});

// Consumidor
let q = Arc::clone(&queue);
thread::spawn(move || {
    loop {
        let val = q.lock().unwrap().pop_front();
        match val {
            Some(v) => println!("consumed: {v}"),
            None => { /* esperar o terminar */ }
        }
    }
});
```

Este es un patrón productor-consumidor simplificado.  En producción se usaría
`std::sync::mpsc` (channel) o `crossbeam::channel`, que son colas thread-safe
optimizadas.  Pero el ejercicio muestra cómo `Mutex` hace safe un `VecDeque`
compartido.
</details>
