# Lista circular doble

## Concepto

Una lista circular doble combina las propiedades de la lista doblemente
enlazada (cada nodo tiene `prev` y `next`) con la circularidad (no hay
extremos — el último conecta con el primero y viceversa):

```
Lista doble lineal:
  head ──→ [null|A|·]⟷[·|B|·]⟷[·|C|null] ←── tail

Lista circular doble:
        ┌─────────────────────────────┐
        ↓                             │
        [·|A|·]⟷[·|B|·]⟷[·|C|·]──────┘
        ↑                             │
        └─────────────────────────────┘

  A.prev = C,  C.next = A
  C.prev = B,  A.next = B
```

No hay `NULL` en ningún puntero.  Cada nodo tiene exactamente un predecesor
y un sucesor, formando un anillo bidireccional.

---

## Estructura

### En C

```c
typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

typedef struct CDList {
    DNode *head;
    int count;
} CDList;
```

A diferencia de la circular simple (que usa `tail`), aquí podemos usar `head`
porque `prev` permite acceder al tail en $O(1)$: `head->prev` es el último
nodo.

```
head ──→ [·|A|·]⟷[·|B|·]⟷[·|C|·]──→ (A)
          ↑                             │
          └─────── head->prev = C ──────┘

head         = primer nodo
head->prev   = último nodo (tail)
```

No necesitamos un puntero `tail` separado — es redundante.

### En Rust

```rust
use std::ptr;

struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}

pub struct CDList<T> {
    head: *mut DNode<T>,
    len: usize,
}

impl<T> CDList<T> {
    pub fn new() -> Self {
        CDList { head: ptr::null_mut(), len: 0 }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_null()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}
```

---

## El nodo solitario

Cuando la lista tiene un solo nodo, ese nodo apunta a **sí mismo** en ambas
direcciones:

```
head ──→ [·|A|·]
          ↑   │
          └───┘   prev = next = sí mismo
```

```c
DNode *node = create_dnode(42);
node->prev = node;
node->next = node;
```

Este estado es consistente con los invariantes de la lista circular doble:
el nodo anterior al primero es el último (que es él mismo), y el siguiente
al último es el primero (que también es él mismo).

---

## Operaciones

### push_front — $O(1)$

```c
void cdlist_push_front(CDList *list, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    if (!list->head) {
        node->prev = node;
        node->next = node;
        list->head = node;
    } else {
        DNode *tail = list->head->prev;

        node->next = list->head;
        node->prev = tail;
        tail->next = node;
        list->head->prev = node;

        list->head = node;
    }
    list->count++;
}
```

El truco: `tail = head->prev` nos da el último nodo sin recorrer.  Insertamos
entre tail y head, luego movemos head al nuevo nodo.

```
Antes:    head ──→ [·|A|·]⟷[·|B|·]⟷[·|C|·]──→ (A)
          tail = head->prev = C

Insertar X al frente:
  X.next = head (A)
  X.prev = tail (C)
  C.next = X
  A.prev = X
  head = X

Después:  head ──→ [·|X|·]⟷[·|A|·]⟷[·|B|·]⟷[·|C|·]──→ (X)
```

### push_back — $O(1)$

```c
void cdlist_push_back(CDList *list, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    if (!list->head) {
        node->prev = node;
        node->next = node;
        list->head = node;
    } else {
        DNode *tail = list->head->prev;

        node->next = list->head;
        node->prev = tail;
        tail->next = node;
        list->head->prev = node;
        // NO mover head — el nuevo queda al final
    }
    list->count++;
}
```

Idéntico a `push_front` **sin** la línea `list->head = node`.  El nuevo nodo
queda entre el viejo tail y head, que es exactamente la posición del tail.

### pop_front — $O(1)$

```c
int cdlist_pop_front(CDList *list) {
    DNode *old_head = list->head;
    int value = old_head->data;

    if (old_head->next == old_head) {
        // Único nodo
        list->head = NULL;
    } else {
        DNode *tail = old_head->prev;
        DNode *new_head = old_head->next;

        tail->next = new_head;
        new_head->prev = tail;

        list->head = new_head;
    }

    free(old_head);
    list->count--;
    return value;
}
```

### pop_back — $O(1)$

```c
int cdlist_pop_back(CDList *list) {
    DNode *tail = list->head->prev;
    int value = tail->data;

    if (tail == list->head) {
        // Único nodo
        list->head = NULL;
    } else {
        DNode *new_tail = tail->prev;

        new_tail->next = list->head;
        list->head->prev = new_tail;
    }

    free(tail);
    list->count--;
    return value;
}
```

$O(1)$ gracias a `head->prev` — la ventaja combinada de circular + doble.

### remove_node — $O(1)$

```c
int cdlist_remove_node(CDList *list, DNode *node) {
    int value = node->data;

    if (node->next == node) {
        // Único nodo
        list->head = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        if (node == list->head) {
            list->head = node->next;
        }
    }

    free(node);
    list->count--;
    return value;
}
```

Sin sentinelas, solo necesitamos un caso especial: si el nodo eliminado
**es** el head, actualizar `list->head`.  Los punteros `prev/next` siempre
existen (no hay NULL), así que `node->prev->next = node->next` nunca falla.

### insert_after / insert_before — $O(1)$

```c
void cdlist_insert_after(CDList *list, DNode *ref, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    node->prev = ref;
    node->next = ref->next;
    ref->next->prev = node;
    ref->next = node;

    list->count++;
}

void cdlist_insert_before(CDList *list, DNode *ref, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    node->next = ref;
    node->prev = ref->prev;
    ref->prev->next = node;
    ref->prev = node;

    if (ref == list->head) {
        list->head = node;   // insertar antes del head = nuevo head
    }

    list->count++;
}
```

Sin `NULL` checks — `ref->prev` y `ref->next` siempre son válidos en una
lista circular doble.  Comparar con la lista doble lineal de S02/T02 donde
cada operación necesitaba verificar si `prev` o `next` eran NULL.

---

## Recorrido

### Forward (una vuelta)

```c
void cdlist_print(const CDList *list) {
    if (!list->head) {
        printf("[]\n");
        return;
    }

    printf("[");
    DNode *cur = list->head;
    do {
        printf("%d", cur->data);
        cur = cur->next;
        if (cur != list->head) printf(", ");
    } while (cur != list->head);
    printf("]\n");
}
```

### Backward (una vuelta)

```c
void cdlist_print_rev(const CDList *list) {
    if (!list->head) {
        printf("[]\n");
        return;
    }

    printf("[");
    DNode *cur = list->head->prev;   // tail
    do {
        printf("%d", cur->data);
        cur = cur->prev;
        if (cur != list->head->prev) printf(", ");
    } while (cur != list->head->prev);
    printf("]\n");
}
```

### Rotación — $O(1)$

```c
void cdlist_rotate_left(CDList *list) {
    if (list->head) {
        list->head = list->head->next;
    }
}

void cdlist_rotate_right(CDList *list) {
    if (list->head) {
        list->head = list->head->prev;
    }
}
```

Ambas rotaciones son $O(1)$.  En la circular simple, `rotate_right` era
$O(n)$ porque necesitaba el penúltimo nodo.  Aquí `head->prev` lo da
directamente.

---

## Traza detallada

Operaciones: `push_back(10)`, `push_back(20)`, `push_back(30)`,
`rotate_left()`, `pop_back()`.

```
push_back(10):
  Lista vacía → node->prev = node->next = node
  head = node

  head ──→ [·|10|·]──→ (sí mismo)

push_back(20):
  tail = head->prev = nodo(10)
  node(20).next = head (10)
  node(20).prev = tail (10)
  10.next = 20
  10.prev = 20 (head->prev)
  head no cambia

  head ──→ [·|10|·]⟷[·|20|·]──→ (10)
  head->prev = 20 (tail)

push_back(30):
  tail = head->prev = nodo(20)
  node(30).next = head (10)
  node(30).prev = tail (20)
  20.next = 30
  10.prev = 30

  head ──→ [·|10|·]⟷[·|20|·]⟷[·|30|·]──→ (10)
  head->prev = 30 (tail)

  Forward:  [10, 20, 30]
  Backward: [30, 20, 10]

rotate_left():
  head = head->next = nodo(20)

  head ──→ [·|20|·]⟷[·|30|·]⟷[·|10|·]──→ (20)

  Forward:  [20, 30, 10]

pop_back():
  tail = head->prev = nodo(10)
  new_tail = tail->prev = nodo(30)

  30.next = head (20)
  20.prev = 30

  free(nodo 10), retorna 10

  head ──→ [·|20|·]⟷[·|30|·]──→ (20)

  Forward:  [20, 30]
```

---

## Nodo sentinela circular

El patrón de sentinela se simplifica con la circularidad: un **solo** nodo
sentinela cuyo `prev` y `next` apuntan a sí mismo cuando la lista está vacía:

```c
typedef struct CDList {
    DNode sentinel;    // embebido, no puntero
    int count;
} CDList;

CDList cdlist_new_sentinel(void) {
    CDList list;
    list.sentinel.prev = &list.sentinel;
    list.sentinel.next = &list.sentinel;
    list.sentinel.data = 0;   // valor ignorado
    list.count = 0;
    return list;
}
```

```
Lista vacía:
  sentinel ──→ [·|S|·]──→ (sí mismo)
               prev=self, next=self

Lista con datos:
  sentinel ──→ [·|S|·]⟷[·|A|·]⟷[·|B|·]⟷[·|C|·]──→ (S)
```

Con un solo sentinela (en vez de dos como en S02/T02), todas las operaciones
quedan sin `if`:

```c
void cdlist_push_front_s(CDList *list, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    node->next = list->sentinel.next;
    node->prev = &list->sentinel;
    list->sentinel.next->prev = node;
    list->sentinel.next = node;
    list->count++;
}

void cdlist_push_back_s(CDList *list, int value) {
    DNode *node = malloc(sizeof(DNode));
    node->data = value;

    node->prev = list->sentinel.prev;
    node->next = &list->sentinel;
    list->sentinel.prev->next = node;
    list->sentinel.prev = node;
    list->count++;
}

int cdlist_remove_node_s(CDList *list, DNode *node) {
    int value = node->data;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node);
    list->count--;
    return value;
}
```

**Cero ramas**.  `push_front` inserta después del sentinela.  `push_back`
inserta antes del sentinela.  `remove_node` desconecta sin verificar nada —
el sentinela garantiza que `prev` y `next` siempre existen.

### Lista vacía

```c
int cdlist_is_empty_s(const CDList *list) {
    return list->sentinel.next == &list->sentinel;
}
```

### Recorrido con sentinela

```c
void cdlist_print_s(const CDList *list) {
    printf("[");
    DNode *cur = list->sentinel.next;
    while (cur != &list->sentinel) {
        printf("%d", cur->data);
        cur = cur->next;
        if (cur != &list->sentinel) printf(", ");
    }
    printf("]\n");
}
```

El recorrido usa `while` normal (no `do-while`), porque la condición de
parada es llegar al sentinela — un nodo distinguible, como un `NULL`
virtual.

---

## Patrón del kernel Linux: list_head

El kernel Linux usa una lista circular doble intrusiva en prácticamente toda
su estructura interna.  La definición es mínima:

```c
struct list_head {
    struct list_head *next, *prev;
};
```

No hay campo `data` — el `list_head` se **embebe** dentro de la estructura
que quiere ser parte de una lista:

```c
struct task_struct {
    pid_t pid;
    char comm[16];
    struct list_head tasks;     // enlace a la lista de tareas
    struct list_head children;  // enlace a la lista de hijos
    // ...
};
```

Un mismo struct puede pertenecer a **múltiples listas** simultáneamente
embebiendo varios `list_head`.

### container_of

Para obtener la estructura padre a partir del puntero a `list_head`:

```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Ejemplo: dado un list_head *pos dentro de tasks:
struct task_struct *task = container_of(pos, struct task_struct, tasks);
```

Aritmética de punteros: restar el offset del campo `tasks` dentro de
`task_struct` al puntero `pos` para obtener el inicio del struct.

### Operaciones del kernel (simplificadas)

```c
// Inicializar (sentinela — auto-referencia)
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

// Insertar (sin ifs — circular doble con sentinela)
static inline void list_add(struct list_head *new,
                            struct list_head *prev,
                            struct list_head *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

// Insertar al frente (después del sentinel head)
static inline void list_add_head(struct list_head *new,
                                 struct list_head *head) {
    list_add(new, head, head->next);
}

// Insertar al final (antes del sentinel head)
static inline void list_add_tail(struct list_head *new,
                                 struct list_head *head) {
    list_add(new, head->prev, head);
}

// Eliminar (sin ifs)
static inline void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

// Recorrer
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)
```

### Por qué este diseño

| Propiedad | Beneficio |
|-----------|-----------|
| Intrusivo | Sin allocation extra por nodo — el enlace está en el struct |
| Un struct en múltiples listas | `task_struct` está en lista de tareas, de hijos, de grupo, etc. |
| Sentinela circular | Zero branches en insert/delete |
| Solo `prev/next` | Genérico — funciona para cualquier struct via `container_of` |
| `static inline` | Sin overhead de llamada a función |

Este patrón es la razón por la que la lista circular doble es la estructura
más usada en el kernel Linux.

---

## Implementación en Rust

```rust
use std::ptr;
use std::marker::PhantomData;

struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}

pub struct CDList<T> {
    head: *mut DNode<T>,
    len: usize,
}

impl<T> CDList<T> {
    pub fn new() -> Self {
        CDList { head: ptr::null_mut(), len: 0 }
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
            next: ptr::null_mut(),
        }));

        if self.head.is_null() {
            unsafe {
                (*node).prev = node;
                (*node).next = node;
            }
            self.head = node;
        } else {
            unsafe {
                let tail = (*self.head).prev;
                (*node).next = self.head;
                (*node).prev = tail;
                (*tail).next = node;
                (*self.head).prev = node;
            }
            self.head = node;
        }
        self.len += 1;
    }

    pub fn push_back(&mut self, value: T) {
        let node = Box::into_raw(Box::new(DNode {
            data: value,
            prev: ptr::null_mut(),
            next: ptr::null_mut(),
        }));

        if self.head.is_null() {
            unsafe {
                (*node).prev = node;
                (*node).next = node;
            }
            self.head = node;
        } else {
            unsafe {
                let tail = (*self.head).prev;
                (*node).next = self.head;
                (*node).prev = tail;
                (*tail).next = node;
                (*self.head).prev = node;
            }
        }
        self.len += 1;
    }

    pub fn pop_front(&mut self) -> Option<T> {
        if self.head.is_null() { return None; }
        unsafe {
            let old = self.head;
            if (*old).next == old {
                self.head = ptr::null_mut();
            } else {
                let tail = (*old).prev;
                let new_head = (*old).next;
                (*tail).next = new_head;
                (*new_head).prev = tail;
                self.head = new_head;
            }
            self.len -= 1;
            Some(Box::from_raw(old).data)
        }
    }

    pub fn pop_back(&mut self) -> Option<T> {
        if self.head.is_null() { return None; }
        unsafe {
            let tail = (*self.head).prev;
            if tail == self.head {
                self.head = ptr::null_mut();
            } else {
                let new_tail = (*tail).prev;
                (*new_tail).next = self.head;
                (*self.head).prev = new_tail;
            }
            self.len -= 1;
            Some(Box::from_raw(tail).data)
        }
    }

    pub fn rotate_left(&mut self) {
        if !self.head.is_null() {
            unsafe { self.head = (*self.head).next; }
        }
    }

    pub fn rotate_right(&mut self) {
        if !self.head.is_null() {
            unsafe { self.head = (*self.head).prev; }
        }
    }
}

impl<T> Drop for CDList<T> {
    fn drop(&mut self) {
        if self.head.is_null() { return; }
        unsafe {
            (*(*self.head).prev).next = ptr::null_mut();  // romper ciclo
            let mut cur = self.head;
            while !cur.is_null() {
                let next = (*cur).next;
                let _ = Box::from_raw(cur);
                cur = next;
            }
        }
    }
}

// Iterador (una vuelta)
pub struct Iter<'a, T> {
    current: *const DNode<T>,
    head: *const DNode<T>,
    started: bool,
    _marker: PhantomData<&'a T>,
}

impl<T> CDList<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        Iter {
            current: self.head,
            head: self.head,
            started: false,
            _marker: PhantomData,
        }
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current.is_null() { return None; }
        if self.started && self.current == self.head { return None; }
        self.started = true;
        unsafe {
            let node = &*self.current;
            self.current = node.next;
            Some(&node.data)
        }
    }
}
```

---

## Comparación: circular simple vs circular doble

| Aspecto | Circular simple | Circular doble |
|---------|----------------|----------------|
| Punteros por nodo | 1 (next) | 2 (prev + next) |
| Memoria por nodo (int, 64-bit) | 16 bytes | 24 bytes |
| push_front | $O(1)$ | $O(1)$ |
| push_back | $O(1)$ | $O(1)$ |
| pop_front | $O(1)$ | $O(1)$ |
| pop_back | $O(n)$ | $O(1)$ |
| rotate_left | $O(1)$ | $O(1)$ |
| rotate_right | $O(n)$ | $O(1)$ |
| remove_node(ptr) | $O(n)$ (buscar prev) | $O(1)$ |
| Recorrido backward | $O(n)$ extra o no posible | $O(n)$ natural |
| Sentinela necesario | No (pero ayuda) | 1 nodo basta |
| Complejidad de código | Media | Media-Alta |
| Uso en kernel Linux | Raro | `list_head` — ubicuo |

---

## Ejercicios

### Ejercicio 1 — Construir y verificar circularidad

Crea una lista circular doble `[10, 20, 30]` con `push_back`.  Verifica
manualmente que `head->prev` es el nodo 30, que `30->next` es el nodo 10,
y que el recorrido forward y backward producen los resultados correctos.

<details>
<summary>Verificación</summary>

```
head ──→ [·|10|·]⟷[·|20|·]⟷[·|30|·]──→ (10)

head->prev == nodo(30)       ✓  (tail)
nodo(30)->next == nodo(10)   ✓  (cierra el ciclo forward)
nodo(10)->prev == nodo(30)   ✓  (cierra el ciclo backward)

Forward:  10 → 20 → 30 → (10) stop
Backward: 30 → 20 → 10 → (30) stop
```
</details>

### Ejercicio 2 — Rotación bidireccional

Sobre la lista `[A, B, C, D, E]`, aplica `rotate_left` dos veces y luego
`rotate_right` una vez.  ¿Cuál es el contenido?

<details>
<summary>Traza</summary>

```
Inicio:         [A, B, C, D, E]   head=A
rotate_left:    [B, C, D, E, A]   head=B
rotate_left:    [C, D, E, A, B]   head=C
rotate_right:   [B, C, D, E, A]   head=B
```

`rotate_right` deshace un `rotate_left` — son inversas.  Ambas son $O(1)$.
</details>

### Ejercicio 3 — Sentinela único

Implementa la versión con un solo nodo sentinela embebido en el struct.
Verifica que `push_front`, `push_back`, y `remove_node` no tienen ningún
`if` (excepto `malloc` check).

<details>
<summary>Verificación</summary>

```c
// push_front — 0 ifs:
node->next = list->sentinel.next;
node->prev = &list->sentinel;
list->sentinel.next->prev = node;
list->sentinel.next = node;

// push_back — 0 ifs:
node->prev = list->sentinel.prev;
node->next = &list->sentinel;
list->sentinel.prev->next = node;
list->sentinel.prev = node;

// remove_node — 0 ifs:
node->prev->next = node->next;
node->next->prev = node->prev;
free(node);
```

El sentinela circular elimina toda ramificación porque `prev` y `next`
siempre apuntan a un nodo válido (el sentinela cuando la lista está vacía).
</details>

### Ejercicio 4 — pop_back $O(1)$

Implementa `pop_back` para la lista circular doble.  Compara con `pop_back`
de la circular simple (T01).  ¿Por qué la doble no necesita recorrer?

<details>
<summary>Respuesta</summary>

```c
// Circular doble — O(1):
DNode *tail = list->head->prev;   // acceso directo al último
// desconectar tail, actualizar head->prev

// Circular simple — O(n):
Node *cur = list->tail->next;     // head
while (cur->next != list->tail)   // buscar penúltimo
    cur = cur->next;
```

La circular simple no tiene `prev` — para encontrar el penúltimo hay que
recorrer toda la lista.  La circular doble tiene `tail->prev` = penúltimo
directamente.
</details>

### Ejercicio 5 — Josephus con circular doble

Modifica el problema de Josephus (T02) para usar una lista circular doble.
Implementa la variante donde el sentido de conteo **alterna**: ronda 1
cuenta hacia adelante, ronda 2 hacia atrás, ronda 3 hacia adelante, etc.

<details>
<summary>Idea</summary>

```c
int direction = 1;   // 1 = forward, -1 = backward

while (cur->next != cur) {
    for (int i = 1; i < k; i++) {
        if (direction == 1) cur = cur->next;
        else cur = cur->prev;
    }
    // eliminar cur
    direction *= -1;   // alternar
}
```

Esta variante es imposible con la lista circular simple — no hay `prev`
para contar hacia atrás.
</details>

### Ejercicio 6 — list_head simplificado

Implementa una versión simplificada del `list_head` del kernel: define
el struct, `list_add_head`, `list_add_tail`, `list_del`, y
`list_for_each`.  Usa una lista de structs `Student { id, name, link }`.

<details>
<summary>Esqueleto</summary>

```c
struct list_head {
    struct list_head *prev, *next;
};

struct Student {
    int id;
    char name[32];
    struct list_head link;
};

// Inicializar cabeza (sentinela)
struct list_head students = { &students, &students };

// Añadir estudiante
struct Student *s = malloc(sizeof(*s));
s->id = 1;
strcpy(s->name, "Alice");
list_add_tail(&s->link, &students);

// Recorrer
struct list_head *pos;
list_for_each(pos, &students) {
    struct Student *st = container_of(pos, struct Student, link);
    printf("%d: %s\n", st->id, st->name);
}
```
</details>

### Ejercicio 7 — Implementación Rust completa

Implementa `CDList<T>` en Rust con: `push_front`, `push_back`, `pop_front`,
`pop_back`, `rotate_left`, `rotate_right`, `iter`, `iter_rev` (backward),
y `Drop`.  Prueba con strings.

<details>
<summary>Prueba</summary>

```rust
let mut list = CDList::new();
list.push_back("one".to_string());
list.push_back("two".to_string());
list.push_back("three".to_string());

for s in list.iter() { print!("{s} "); }       // one two three
// para iter_rev necesitas un iterador que siga prev
for s in list.iter_rev() { print!("{s} "); }   // three two one

list.rotate_right();
for s in list.iter() { print!("{s} "); }       // three one two
```
</details>

### Ejercicio 8 — Diferencia con doble lineal

¿Cuántos `if` hay en `remove_node` para cada variante?
1. Doble lineal sin sentinela (S02/T02).
2. Doble lineal con 2 sentinelas (S02/T02).
3. Circular doble sin sentinela.
4. Circular doble con 1 sentinela.

<details>
<summary>Conteo</summary>

```
1. Doble lineal sin sentinela:     2 ifs (prev==NULL?, next==NULL?)
                                   + 1 if implícito (head o tail?)
2. Doble lineal con 2 sentinelas:  0 ifs
3. Circular doble sin sentinela:   1 if (¿es el head? + ¿es el único?)
4. Circular doble con 1 sentinela: 0 ifs
```

Las versiones con sentinela eliminan todas las ramas.  La circular con
1 sentinela es la más elegante: un solo nodo extra, cero ifs.
</details>

### Ejercicio 9 — Buffer circular con overwrite

Implementa un buffer circular de tamaño fijo $n$ donde `write(value)`
sobreescribe el dato más antiguo cuando está lleno.  Usa la lista circular
doble con $n$ nodos pre-allocados (sin malloc/free después de la
inicialización).

<details>
<summary>Diseño</summary>

```c
CDList buffer;
// Inicializar con n nodos (datos = 0)
for (int i = 0; i < n; i++) cdlist_push_back(&buffer, 0);

// write: sobreescribir head y rotar
void buffer_write(CDList *buf, int value) {
    buf->head->data = value;
    buf->head = buf->head->next;   // rotate_left
}

// read: leer todos los datos en orden (del más antiguo al más reciente)
// El head actual apunta al dato más antiguo
```

Sin allocations en estado estable — ideal para sistemas embebidos o logging
de alta frecuencia.
</details>

### Ejercicio 10 — Complejidad comparada

Completa la tabla de complejidades para las 4 variantes de lista con
puntero(s) al head (y tail si aplica).  Asume que `remove_node` recibe
un puntero al nodo.

| Operación | Simple lineal | Simple circular | Doble lineal | Doble circular |
|-----------|:---:|:---:|:---:|:---:|
| push_front | ? | ? | ? | ? |
| push_back | ? | ? | ? | ? |
| pop_front | ? | ? | ? | ? |
| pop_back | ? | ? | ? | ? |
| rotate_left | ? | ? | ? | ? |
| rotate_right | ? | ? | ? | ? |
| remove_node(ptr) | ? | ? | ? | ? |

<details>
<summary>Tabla completa</summary>

| Operación | Simple lineal | Simple circular | Doble lineal | Doble circular |
|-----------|:---:|:---:|:---:|:---:|
| push_front | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ |
| push_back | $O(1)$* | $O(1)$ | $O(1)$ | $O(1)$ |
| pop_front | $O(1)$ | $O(1)$ | $O(1)$ | $O(1)$ |
| pop_back | $O(n)$ | $O(n)$ | $O(1)$ | $O(1)$ |
| rotate_left | $O(n)$** | $O(1)$ | $O(n)$** | $O(1)$ |
| rotate_right | $O(n)$ | $O(n)$ | $O(n)$** | $O(1)$ |
| remove_node(ptr) | $O(n)$ | $O(n)$ | $O(1)$ | $O(1)$ |

*Con tail pointer.  **Sin tail pointer es $O(n)$; con tail pointer
la rotación izquierda lineal es ajustar head/tail pero no es la
operación natural.

La circular doble tiene $O(1)$ en **todas** las operaciones — es la
variante más versátil a costa de 2 punteros por nodo.
</details>
