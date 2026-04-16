# C vs Rust

## El caso clásico donde C es más natural

Las listas doblemente enlazadas son el ejemplo canónico donde el modelo de
ownership de Rust genera fricción significativa.  En C, un nodo con `prev` y
`next` es una estructura con dos punteros — 3 líneas.  En Rust, representar
que un nodo es apuntado por dos vecinos simultáneamente requiere esquivar el
sistema de ownership, ya sea con `Rc<RefCell>` (overhead runtime) o con
`unsafe` (abandonar las garantías del compilador).

Este tópico no es una crítica a Rust — es un análisis honesto de los trade-offs
que cada lenguaje impone, y de por qué ambos son herramientas válidas.

---

## Comparación operación por operación

### Crear un nodo

**C:**
```c
DNode *create_dnode(int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;
    node->prev = NULL;
    node->next = NULL;
    return node;
}
```

**Rust unsafe:**
```rust
fn create_dnode<T>(value: T) -> *mut DNode<T> {
    Box::into_raw(Box::new(DNode {
        data: value,
        prev: ptr::null_mut(),
        next: ptr::null_mut(),
    }))
}
```

**Rust safe (Rc<RefCell>):**
```rust
fn create_dnode<T>(value: T) -> Rc<RefCell<DNode<T>>> {
    Rc::new(RefCell::new(DNode {
        data: value,
        prev: None,
        next: None,
    }))
}
```

| | C | Rust unsafe | Rust safe |
|-|---|------------|-----------|
| Líneas | 5 | 5 | 5 |
| Conceptos | malloc, sizeof, punteros | Box, into_raw, null_mut | Rc, RefCell, Option |
| Error posible | malloc retorna NULL | Ninguno (Box panics en OOM) | Ninguno |
| Legibilidad | Directa | Similar a C | Tres wrappers anidados |

### push_front

**C:**
```c
void dlist_push_front(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->next = list->head;
    if (list->head) list->head->prev = node;
    else list->tail = node;
    list->head = node;
    list->count++;
}
```

**Rust unsafe:**
```rust
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
```

**Rust safe (Rc<RefCell>):**
```rust
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
```

| | C | Rust unsafe | Rust safe |
|-|---|------------|-----------|
| Líneas | 6 | 10 | 14 |
| Acceso a campos | `->` directo | `unsafe { (*ptr).field }` | `.borrow()` / `.borrow_mut()` |
| Redirigir prev | `head->prev = node` | `(*self.head).prev = node` | `old_head.borrow_mut().prev = Some(Rc::downgrade(...))` |
| Probabilidad de error | Media (olvidar prev) | Media (mismos errores + UB) | Baja (compilador + panics) |

### Eliminar nodo dado su puntero

Esta operación concentra la diferencia fundamental.

**C — 6 líneas, directo:**
```c
int dlist_remove_node(DList *list, DNode *node) {
    int value = node->data;
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    else list->tail = node->prev;
    free(node);
    list->count--;
    return value;
}
```

**Rust unsafe — casi idéntico a C:**
```rust
pub unsafe fn remove_node(&mut self, node: *mut DNode<T>) -> T {
    let prev = (*node).prev;
    let next = (*node).next;
    if !prev.is_null() { (*prev).next = next; }
    else { self.head = next; }
    if !next.is_null() { (*next).prev = prev; }
    else { self.tail = prev; }
    self.len -= 1;
    Box::from_raw(node).data
}
```

**Rust safe (Rc<RefCell>) — significativamente más complejo:**
```rust
pub fn remove_node(&mut self, node: &Rc<RefCell<DNode<T>>>) -> T {
    let prev_weak = node.borrow().prev.clone();
    let next_strong = node.borrow_mut().next.take();

    match prev_weak.as_ref().and_then(|w| w.upgrade()) {
        Some(prev_node) => {
            prev_node.borrow_mut().next = next_strong.clone();
        }
        None => {
            self.head = next_strong.clone();
        }
    }

    match next_strong {
        Some(ref next_node) => {
            next_node.borrow_mut().prev = prev_weak;
        }
        None => {
            self.tail = prev_weak;
        }
    }

    self.len -= 1;
    Rc::try_unwrap(Rc::clone(node))
        .ok().expect("node still referenced")
        .into_inner().data
}
```

La versión Rc<RefCell> requiere:
- `.borrow()` y `.borrow_mut()` en cada acceso (verificación runtime).
- `.clone()` de `Weak` y `Rc` para mover references.
- `Weak::upgrade()` para convertir Weak → Rc (puede fallar).
- `Rc::try_unwrap()` para extraer el dato (puede fallar si refcount > 1).
- Manejar `Option` en cada paso.

---

## Ergonomía del recorrido

**C:**
```c
DNode *cur = list->head;
while (cur) {
    printf("%d ", cur->data);
    cur = cur->next;
}
```

**Rust unsafe:**
```rust
let mut cur = self.head;
while !cur.is_null() {
    unsafe {
        print!("{} ", (*cur).data);
        cur = (*cur).next;
    }
}
```

**Rust safe:**
```rust
let mut cur = self.head.clone();
while let Some(node) = cur {
    print!("{} ", node.borrow().data);
    cur = node.borrow().next.clone();
}
```

En C, el patrón `while (cur)` con `cur = cur->next` es tan natural que se
aprende en la primera semana.  En Rust safe, cada paso requiere `.borrow()`
(verificación runtime), `.clone()` (incrementar refcount), y manejar `Option`.

---

## El puntero `->` vs la cadena de métodos

El operador `->` de C desreferencia un puntero y accede a un campo en una
sola expresión.  En Rust, la cadena equivalente crece según la estrategia:

```
C:           node->next->prev->data
Rust unsafe: (*(*(*node).next).prev).data
Rust safe:   node.borrow().next.as_ref().unwrap().borrow().prev.as_ref()
                 .and_then(|w| w.upgrade()).unwrap().borrow().data
```

La versión safe acumula 7 llamadas a métodos para lo que C hace con 3
operadores `->`.

---

## Dónde C gana

### 1. Simplicidad conceptual

Un puntero en C es una dirección de memoria.  No hay ownership, no hay
borrowing, no hay lifetimes.  `node->next = other` simplemente escribe
una dirección en un campo.

En Rust, cada asignación de enlace implica una pregunta: ¿quién es el dueño?
¿Es `Rc` o `Weak`?  ¿Necesito `.borrow_mut()`?  ¿Cómo extraigo el dato
del `Rc<RefCell<...>>`?

### 2. Rendimiento predecible

```c
node->prev->next = node->next;   // 2 cargas de memoria, 1 escritura
```

No hay contadores atómicos, no hay verificaciones de borrow, no hay
indirecciones extra.  El compilador de C genera exactamente las instrucciones
de memoria que el programador espera.

### 3. Mutabilidad libre

En C, cualquier puntero válido se puede leer o escribir en cualquier momento.
Para listas dobles, esto es una ventaja: podemos modificar `prev` de un nodo
mientras leemos `next` de otro, sin restricciones.

### 4. Patrón del kernel Linux

La estructura `list_head` del kernel Linux es una lista doble intrusiva usada
en miles de lugares.  Su diseño es posible gracias a la libertad de punteros
de C:

```c
struct list_head {
    struct list_head *next, *prev;
};

// Se embebe dentro de cualquier struct:
struct task_struct {
    // ... campos ...
    struct list_head tasks;
    // ... más campos ...
};
```

El macro `container_of` recupera la struct padre a partir del puntero a
`list_head` — aritmética de punteros pura, imposible en safe Rust.

---

## Dónde Rust gana

### 1. Seguridad de memoria garantizada (safe Rust)

C no tiene protección contra:

```c
// Use-after-free
DNode *node = list->head;
dlist_remove_node(list, node);
printf("%d\n", node->data);       // UB — node ya fue liberado

// Double free
free(node);
free(node);                        // UB

// Dangling pointer en tail
dlist_pop_front(list);
// Si olvidamos actualizar tail cuando la lista queda vacía:
printf("%d\n", list->tail->data);  // UB — tail apunta a memoria liberada

// Buffer overread desde nodo corrupto
node->next = (DNode *)0xDEADBEEF;
DNode *bad = node->next;
printf("%d\n", bad->data);         // UB — dirección inválida
```

En Rust safe (`Rc<RefCell>`), todos estos errores son imposibles.  En Rust
unsafe, son posibles pero están delimitados por bloques `unsafe` — el
programador sabe exactamente dónde buscar bugs.

### 2. Genéricos nativos

```c
// C: genéricos con void* — sin type safety
typedef struct DNode {
    void *data;
    struct DNode *prev, *next;
} DNode;

// Cada uso requiere cast:
int *val = (int *)node->data;
```

```rust
// Rust: genéricos con <T> — type safety completa
struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}

// Sin casts, el compilador verifica tipos
let val: &i32 = &node.data;
```

### 3. Drop automático (con raw pointers)

Incluso la versión unsafe de Rust puede implementar `Drop` para liberación
automática:

```rust
impl<T> Drop for DList<T> {
    fn drop(&mut self) {
        while self.pop_front().is_some() {}
    }
}
```

En C, olvidar `dlist_destroy` filtra toda la lista.  No hay destructor
automático.

### 4. Iteradores integrados

```rust
// Rust: integración directa con el ecosistema
let sum: i32 = list.iter().sum();
let evens: Vec<&i32> = list.iter().filter(|&&x| x % 2 == 0).collect();
let reversed: Vec<&i32> = list.iter().rev().collect();  // con DoubleEndedIterator
```

En C, cada operación de este tipo requiere un loop manual o una función
con callback.

### 5. Concurrencia segura

Con `Arc<Mutex<DNode<T>>>` (versión thread-safe de `Rc<RefCell>`), una lista
puede compartirse entre hilos con garantías del compilador.  En C, los
data races son responsabilidad del programador.

---

## Tabla comparativa completa

| Aspecto | C | Rust safe (Rc<RefCell>) | Rust unsafe (*mut) |
|---------|---|------------------------|-------------------|
| **Ergonomía** | | | |
| Acceso a campo | `node->data` | `node.borrow().data` | `(*node).data` |
| Asignar enlace | `a->next = b` | `a.borrow_mut().next = Some(Rc::clone(&b))` | `(*a).next = b` |
| Recorrido | `while (cur)` | `while let Some(node) = cur` | `while !cur.is_null()` |
| Verbosidad | Baja | Alta | Media |
| **Seguridad** | | | |
| Use-after-free | Posible | Imposible | Posible (en unsafe) |
| Double free | Posible | Imposible | Posible |
| Null dereference | Posible (crash) | Imposible | Posible |
| Memory leak | Posible | Posible (ciclos Rc) | Posible (olvidar Drop) |
| Data race | Posible | Imposible (Rc no es Send) | Posible |
| **Rendimiento** | | | |
| Overhead por acceso | 0 | borrow check runtime | 0 |
| Overhead por enlace | 0 | refcount increment | 0 |
| Memoria por nodo (int) | 24 bytes | ~80 bytes | 24 bytes |
| Localidad de caché | Mala (heap disperso) | Peor (más indirección) | Mala (heap disperso) |
| **Diseño** | | | |
| Genéricos | `void *` (no safe) | `<T>` nativo | `<T>` nativo |
| Destructor automático | No | Sí (Rc → Drop) | Manual (impl Drop) |
| Iteradores | Manual / callbacks | Trait Iterator | Trait Iterator |
| Thread safety | Manual | Compilador (Rc ≠ Send) | Manual (impl Send/Sync) |
| **Desarrollo** | | | |
| Curva de aprendizaje | Baja (punteros) | Alta (Rc, Weak, RefCell) | Media (unsafe rules) |
| Debugging | Valgrind, ASAN | Panic con mensaje | Miri, ASAN |
| Tiempo de compilación | Rápido | Lento (generics, traits) | Lento |

---

## Métricas cuantitativas

### Líneas de código por operación

| Operación | C | Rust unsafe | Rust Rc<RefCell> |
|-----------|---|-------------|-----------------|
| push_front | 6 | 10 | 14 |
| push_back | 6 | 10 | 14 |
| pop_front | 7 | 10 | 12 |
| pop_back | 7 | 10 | 14 |
| remove_node | 7 | 9 | 18 |
| reverse | 9 | 9 | ~20 |
| **Total** | **~42** | **~58** | **~92** |

La versión Rust safe requiere ~2.2× más líneas que C para la misma
funcionalidad.  La versión unsafe es ~1.4× más que C.

### Conceptos necesarios para implementar

**C (7 conceptos):**
```
malloc, free, sizeof, punteros, ->, NULL, struct
```

**Rust unsafe (10 conceptos):**
```
Box, into_raw, from_raw, null_mut, is_null, unsafe, PhantomData,
ptr module, struct, impl
```

**Rust safe (15+ conceptos):**
```
Rc, Weak, RefCell, borrow, borrow_mut, clone, downgrade, upgrade,
try_unwrap, into_inner, Option, take, as_ref, match, struct, impl
```

---

## Cuándo elegir cada uno

### Elige C cuando

- Trabajas en sistemas embebidos, kernels o drivers.
- Necesitas control total sobre el layout de memoria.
- Interoperabilidad con código C existente.
- El equipo tiene experiencia con gestión manual de memoria.
- El rendimiento es absolutamente crítico y cada byte cuenta.

### Elige Rust unsafe cuando

- Necesitas rendimiento equivalente a C con genéricos type-safe.
- Implementas una biblioteca de estructuras de datos.
- FFI bidireccional con C.
- Quieres encapsular el `unsafe` dentro de una API safe.
- Tienes acceso a Miri para verificar corrección.

### Elige Rust safe (Rc<RefCell>) cuando

- Estás aprendiendo y quieres entender ownership en profundidad.
- Prototipando — la corrección importa más que el rendimiento.
- La lista es pequeña y el overhead no importa.
- No quieres ningún `unsafe` en tu código.

### Elige ni C ni Rust (usa VecDeque) cuando

- No necesitas específicamente una lista enlazada.
- Solo necesitas push/pop en ambos extremos.
- El rendimiento por localidad de caché importa más que la complejidad
  asintótica de operaciones raras.
- No necesitas `append` en $O(1)$ ni inserción en medio por cursor.

---

## La lección

La lista doblemente enlazada expone un trade-off fundamental:

```
C:    Máxima flexibilidad con punteros → responsabilidad total del programador
Rust: Máxima seguridad de memoria → contorsiones cuando la estructura no es un árbol
```

Las estructuras tipo **árbol** (un padre, múltiples hijos) se mapean
naturalmente al ownership de Rust: cada nodo tiene un único dueño (su padre).
Las estructuras tipo **grafo** (múltiples referencias a un nodo) chocan con
el modelo — y la lista doblemente enlazada es el grafo más simple posible.

Esto no significa que Rust sea malo para grafos.  Significa que grafos y
listas dobles son los casos donde el programador debe tomar una decisión
explícita sobre el trade-off: ¿safety runtime (Rc<RefCell>), unsafe
encapsulado, o una estructura alternativa (VecDeque)?

La capacidad de C para manipular punteros sin restricciones es simultáneamente
su mayor fortaleza y su mayor debilidad.  Rust obliga a elegir — y ese acto
de elegir es lo que previene los bugs de memoria.

---

## Ejercicios

### Ejercicio 1 — Traducir de C a Rust unsafe

Traduce esta función C a Rust unsafe:

```c
void dlist_insert_after(DList *list, DNode *ref, int value) {
    DNode *node = create_dnode(value);
    node->prev = ref;
    node->next = ref->next;
    if (ref->next) ref->next->prev = node;
    else list->tail = node;
    ref->next = node;
    list->count++;
}
```

<details>
<summary>Traducción</summary>

```rust
pub unsafe fn insert_after(&mut self, refer: *mut DNode<T>, value: T) {
    let node = Box::into_raw(Box::new(DNode {
        data: value,
        prev: refer,
        next: (*refer).next,
    }));
    if !(*refer).next.is_null() {
        (*(*refer).next).prev = node;
    } else {
        self.tail = node;
    }
    (*refer).next = node;
    self.len += 1;
}
```

La traducción es casi mecánica: `->` se convierte en `(*ptr).field`,
`NULL` en `null_mut()` / `.is_null()`, `free` en `Box::from_raw`.
</details>

### Ejercicio 2 — Traducir de C a Rust safe

Traduce la misma `dlist_insert_after` a Rust con `Rc<RefCell>`.  Compara
la cantidad de líneas y conceptos adicionales.

<details>
<summary>Traducción</summary>

```rust
pub fn insert_after(&mut self, refer: &Rc<RefCell<DNode<T>>>, value: T) {
    let new_node = Rc::new(RefCell::new(DNode {
        data: value,
        prev: Some(Rc::downgrade(refer)),
        next: refer.borrow_mut().next.take(),
    }));

    match new_node.borrow().next.as_ref() {
        Some(next_node) => {
            next_node.borrow_mut().prev = Some(Rc::downgrade(&new_node));
        }
        None => {
            self.tail = Some(Rc::downgrade(&new_node));
        }
    }

    refer.borrow_mut().next = Some(new_node);
    self.len += 1;
}
```

C: 7 líneas, 0 llamadas a métodos de tipo.
Rust safe: 14 líneas, 6 llamadas (`borrow`, `borrow_mut`, `downgrade`,
`take`, `as_ref`, `Rc::new`).
</details>

### Ejercicio 3 — Contar conceptos

Para la operación `pop_back`, lista todos los conceptos del lenguaje
necesarios en C y en Rust safe.  ¿Cuál requiere más conocimiento previo?

<details>
<summary>Análisis</summary>

**C** — `pop_back`:
1. Punteros (`*`)
2. Dereference (`->`)
3. NULL check
4. `free`
5. Asignación de punteros

**Rust safe** — `pop_back`:
1. `Option` y pattern matching (`take`, `and_then`, `map`)
2. `Weak::upgrade` (puede fallar)
3. `Rc::downgrade`
4. `RefCell::borrow` / `borrow_mut`
5. `Rc::try_unwrap` (consume Rc si refcount == 1)
6. `.ok().expect()` (manejar Result)
7. `.into_inner()` (extraer de RefCell)
8. Closures (para `map` / `and_then`)

C: 5 conceptos básicos.  Rust safe: 8 conceptos, varios de nivel intermedio.
</details>

### Ejercicio 4 — Bug hunting en C

Este código tiene 2 bugs.  Encuéntralos y explica qué UB causan:

```c
void dlist_remove_value(DList *list, int target) {
    DNode *cur = list->head;
    while (cur) {
        if (cur->data == target) {
            cur->prev->next = cur->next;
            cur->next->prev = cur->prev;
            free(cur);
            list->count--;
        }
        cur = cur->next;
    }
}
```

<details>
<summary>Bugs</summary>

**Bug 1**: `cur->prev->next` cuando `cur` es el head — `prev` es NULL.
Null dereference → crash o UB.

**Bug 2**: `cur = cur->next` después de `free(cur)` — use-after-free.
`cur` ya fue liberado, acceder a `cur->next` es UB.

Versión corregida:
```c
while (cur) {
    DNode *next = cur->next;     // guardar ANTES del free
    if (cur->data == target) {
        if (cur->prev) cur->prev->next = cur->next;
        else list->head = cur->next;
        if (cur->next) cur->next->prev = cur->prev;
        else list->tail = cur->prev;
        free(cur);
        list->count--;
    }
    cur = next;
}
```

En Rust safe, ambos bugs son imposibles: `borrow()` impide use-after-free
y `Option` impide null dereference.
</details>

### Ejercicio 5 — Bug hunting en Rust safe

¿Este código compila?  Si sí, ¿qué pasa en runtime?

```rust
let node = Rc::new(RefCell::new(DNode {
    data: 42, prev: None, next: None,
}));

let borrow1 = node.borrow();
let borrow2 = node.borrow_mut();   // ???
```

<details>
<summary>Respuesta</summary>

**Compila** — RefCell verifica en runtime, no en compilación.

**Panic en runtime**: `already borrowed: BorrowMutError`.  `borrow()` tomó
un shared borrow; `borrow_mut()` intenta un exclusive borrow mientras el
shared sigue activo.

Este es el trade-off de RefCell: los errores de borrowing se detectan, pero
en runtime (panic) en vez de en compilación (error de compilador).  Con raw
pointers, este mismo patrón sería UB silencioso.
</details>

### Ejercicio 6 — Rendimiento: insertar y eliminar en medio

Para una lista de $n = 10^5$ nodos, compara insertar y eliminar en la
posición del medio para:
1. C con puntero al nodo (previamente obtenido).
2. C buscando por posición.
3. `Vec::insert(n/2, val)` en Rust.

<details>
<summary>Análisis</summary>

1. **C con puntero al nodo**: $O(1)$ — redirigir 4 punteros.
   Tiempo: nanosegundos.

2. **C buscando por posición**: $O(n/2)$ recorrido + $O(1)$ inserción.
   Tiempo: ~50,000 pasos de puntero (~microsegundos).

3. **Vec::insert(n/2)**: $O(n/2)$ para desplazar la mitad del array.
   Tiempo: ~50,000 movimientos de memoria.

Misma complejidad asintótica para 2 y 3, pero Vec es más rápido en la
práctica por localidad de caché (mover memoria contigua vs saltar entre
nodos dispersos).

La ventaja real de la lista doble es el caso 1: cuando *ya tienes* el
puntero al nodo (ej. LRU cache con hash map → puntero).
</details>

### Ejercicio 7 — Container_of en Rust

En el kernel Linux, `container_of(ptr, type, member)` calcula la dirección
de la struct padre a partir de un puntero a un campo.  ¿Se puede hacer
en Rust?

<details>
<summary>Respuesta</summary>

Sí, con unsafe y aritmética de punteros:

```rust
macro_rules! container_of {
    ($ptr:expr, $type:ty, $field:ident) => {
        unsafe {
            let field_offset = std::mem::offset_of!($type, $field);
            ($ptr as *const u8).sub(field_offset) as *const $type
        }
    }
}
```

`offset_of!` (estabilizado en Rust 1.77) retorna el offset en bytes de un
campo dentro de un struct.  Restando ese offset al puntero del campo,
obtenemos el puntero al struct padre.

Esto es `unsafe` puro — aritmética de punteros sin verificación.  Demuestra
que Rust *puede* hacer lo mismo que C, pero el programador debe optar
explícitamente por abandonar las garantías.
</details>

### Ejercicio 8 — Miri: detectar UB

Escribe una versión unsafe de `pop_front` con un bug intencional
(use-after-free o doble free).  Ejecuta con Miri y observa el error
reportado.  ¿Podría Valgrind detectar lo mismo en C?

<details>
<summary>Ejemplo y comparación</summary>

```rust
pub fn pop_front_buggy(&mut self) -> Option<T> {
    if self.head.is_null() { return None; }
    unsafe {
        let old = Box::from_raw(self.head);
        let _ = Box::from_raw(self.head);   // DOUBLE FREE
        self.head = old.next;
        self.len -= 1;
        Some(old.data)
    }
}
```

Miri reporta:
```
error: Undefined Behavior: memory freed twice
```

Valgrind en C reportaría algo similar:
```
Invalid free() / delete / delete[] / realloc()
```

Ambas herramientas detectan el error, pero Miri corre sin compilar a código
nativo (interpreta MIR) y detecta UB que Valgrind podría no ver (ej. UB que
no causa crash pero viola las reglas de Rust).
</details>

### Ejercicio 9 — Implementar en ambos lenguajes

Implementa una función `dlist_swap_nodes` que intercambie las **posiciones**
de dos nodos (no solo sus datos) en la lista.  Implementa en C y en Rust
unsafe.  ¿Cuántos punteros necesitas actualizar?

<details>
<summary>Análisis</summary>

Caso general (nodos no adyacentes): **hasta 10 punteros** a actualizar:
- `a->prev->next`, `a->next->prev`, `b->prev->next`, `b->next->prev`
  (4 vecinos redirigidos)
- `a->prev`, `a->next`, `b->prev`, `b->next` (los propios nodos)
- Posiblemente `list->head` y/o `list->tail`

Caso adyacente (a→b o b→a): menos punteros pero lógica más delicada porque
los nodos son vecinos directos.

Swap de datos (`a->data ↔ b->data`) es $O(1)$ y trivial, pero mueve el dato
en vez del nodo — con tipos grandes o no-movibles, intercambiar nodos es
preferible.

Esta operación tiene la misma complejidad en C y Rust unsafe, pero la cantidad
de punteros a manejar manualmente demuestra por qué las listas dobles son
propensas a errores en ambos lenguajes.
</details>

### Ejercicio 10 — Decisión de diseño

Estás construyendo un LRU cache con capacidad 1000.  Las operaciones son:
- `get(key)`: buscar por clave, mover al frente si existe.
- `put(key, value)`: insertar/actualizar, evictar el LRU si lleno.

¿Qué combinación de lenguaje + estructura elegirías y por qué?

<details>
<summary>Análisis</summary>

**Estructura ideal**: HashMap + lista doblemente enlazada.

- `get`: HashMap busca en $O(1)$, el valor almacenado incluye un puntero al
  nodo en la lista.  Mover al frente: `remove_node` $O(1)$ + `push_front`
  $O(1)$.
- `put`: HashMap inserta en $O(1)$, `push_front` $O(1)$.  Si lleno,
  `pop_back` $O(1)$ + HashMap remove $O(1)$.

**En C**: natural.  HashMap con `DNode *` como valor.  Todos los punteros
son directos.  Riesgo: bugs de memoria en producción.

**En Rust unsafe**: equivalente a C pero con Drop automático y API safe
sobre el HashMap.  El `unsafe` queda encapsulado dentro de la lista.

**En Rust safe (Rc<RefCell>)**: funciona pero el HashMap almacenaría
`Rc<RefCell<DNode>>` — overhead significativo de memoria y runtime para
1000 entries.

**En Rust con stdlib**: `LinkedList<T>` no sirve porque no expone punteros
a nodos (sin CursorMut estable).  La opción realista es un crate como `lru`
que usa unsafe internamente.

**Recomendación**: Rust unsafe con API safe para producción.  C para
sistemas embebidos.  Rust safe o crate `lru` para prototipo rápido.
</details>
