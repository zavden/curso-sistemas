# Nodos con doble enlace

## De simple a doble

En una lista simplemente enlazada, cada nodo tiene un único puntero `next`.
Esto impone una dirección: solo podemos avanzar.  Consecuencias prácticas:

| Operación | Simple | Problema |
|-----------|--------|----------|
| Avanzar al siguiente | $O(1)$ | Ninguno |
| Retroceder al anterior | $O(n)$ | Hay que recorrer desde `head` |
| Eliminar un nodo dado su puntero | $O(n)$ | Necesitamos el **anterior** |
| pop_back | $O(n)$ | Necesitamos el **penúltimo** |
| Recorrido inverso | $O(n)$ extra | Hay que invertir o usar recursión |

Todas estas limitaciones desaparecen si cada nodo conoce **dos** vecinos:

```
Lista simplemente enlazada:
  head ──→ [A|·]──→ [B|·]──→ [C|null]

Lista doblemente enlazada:
  head ──→ [null|A|·]⟷[·|B|·]⟷[·|C|null] ←── tail
```

---

## Estructura del nodo

### En C

```c
typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;
```

Comparación con el nodo simple:

```c
// Simple:  8 bytes (data) + 8 bytes (next) = 16 bytes
typedef struct Node {
    int data;
    struct Node *next;
};

// Doble:   8 bytes (prev) + 8 bytes (data) + 8 bytes (next) = 24 bytes
typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
};
```

En una plataforma de 64 bits con `int data` (4 bytes) y alineación a 8 bytes:

```
Nodo simple (16 bytes):
  ┌────────┬────────┐
  │  data  │  next  │
  │ 4 + 4p │   8    │
  └────────┴────────┘

Nodo doble (24 bytes):
  ┌────────┬────────┬────────┐
  │  data  │  prev  │  next  │
  │ 4 + 4p │   8    │   8    │
  └────────┴────────┴────────┘
```

El nodo doble usa 50% más memoria que el simple.  Para datos pequeños como
`int`, la *overhead* del nodo domina: 20 bytes de punteros+padding para 4
bytes de dato.

### En Rust

```rust
struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}
```

Los punteros `prev` y `next` son raw pointers (`*mut`) porque un nodo no puede
ser *owned* por dos `Box` simultáneamente (el nodo anterior y el siguiente).
En safe Rust la alternativa es `Rc<RefCell<DNode<T>>>`, pero con overhead
significativo — tema de T03.

---

## Crear un nodo

### En C

```c
#include <stdlib.h>
#include <stdio.h>

DNode *create_dnode(int value) {
    DNode *node = malloc(sizeof(DNode));
    if (!node) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
    node->data = value;
    node->prev = NULL;
    node->next = NULL;
    return node;
}
```

El nodo recién creado tiene ambos punteros en `NULL` — está *aislado*,
sin conexión a ninguna lista.

### En Rust

```rust
fn create_dnode<T>(value: T) -> *mut DNode<T> {
    Box::into_raw(Box::new(DNode {
        data: value,
        prev: std::ptr::null_mut(),
        next: std::ptr::null_mut(),
    }))
}
```

`Box::into_raw` transfiere la memoria del heap al raw pointer.  La liberación
será manual (con `Box::from_raw`), igual que `free` en C.

---

## Estructura de la lista

Una lista doblemente enlazada mantiene punteros a ambos extremos:

```c
typedef struct DList {
    DNode *head;
    DNode *tail;
    int count;
} DList;

DList dlist_new(void) {
    return (DList){ .head = NULL, .tail = NULL, .count = 0 };
}
```

```rust
pub struct DList<T> {
    head: *mut DNode<T>,
    tail: *mut DNode<T>,
    len: usize,
}

impl<T> DList<T> {
    pub fn new() -> Self {
        DList {
            head: std::ptr::null_mut(),
            tail: std::ptr::null_mut(),
            len: 0,
        }
    }
}
```

Invariantes:
1. `head == NULL` si y solo si `tail == NULL` si y solo si la lista está vacía.
2. `head->prev == NULL` (el primero no tiene anterior).
3. `tail->next == NULL` (el último no tiene siguiente).
4. Para todo nodo `n` con `n->next != NULL`: `n->next->prev == n` (consistencia bidireccional).
5. `count` == número real de nodos.

El invariante 4 es el más importante y el más fácil de romper: cada vez que
modificamos `next`, **debemos** actualizar el `prev` correspondiente (y
viceversa).

---

## Recorrido bidireccional

### Hacia adelante (forward)

```c
void dlist_print_forward(const DList *list) {
    printf("[");
    DNode *cur = list->head;
    while (cur) {
        printf("%d", cur->data);
        if (cur->next) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}
```

Idéntico al recorrido de lista simple: seguir `next` hasta `NULL`.

### Hacia atrás (backward)

```c
void dlist_print_backward(const DList *list) {
    printf("[");
    DNode *cur = list->tail;
    while (cur) {
        printf("%d", cur->data);
        if (cur->prev) printf(", ");
        cur = cur->prev;
    }
    printf("]\n");
}
```

Empezamos en `tail` y seguimos `prev`.  Esto es $O(n)$ — igual que forward —
pero en la dirección opuesta.

```
Lista: [10, 20, 30]

Forward:  head ──→ 10 ──→ 20 ──→ 30 ──→ NULL
Backward: NULL ←── 10 ←── 20 ←── 30 ←── tail
```

En Rust:

```rust
impl<T: std::fmt::Display> DList<T> {
    pub fn print_forward(&self) {
        print!("[");
        let mut cur = self.head;
        let mut first = true;
        while !cur.is_null() {
            unsafe {
                if !first { print!(", "); }
                print!("{}", (*cur).data);
                cur = (*cur).next;
                first = false;
            }
        }
        println!("]");
    }

    pub fn print_backward(&self) {
        print!("[");
        let mut cur = self.tail;
        let mut first = true;
        while !cur.is_null() {
            unsafe {
                if !first { print!(", "); }
                print!("{}", (*cur).data);
                cur = (*cur).prev;
                first = false;
            }
        }
        println!("]");
    }
}
```

---

## Eliminación $O(1)$ con puntero al nodo

La ventaja definitiva de la lista doblemente enlazada: si tienes un **puntero
al nodo** que quieres eliminar, la eliminación es $O(1)$.

En una lista simple, eliminar un nodo requiere encontrar el **anterior** —
$O(n)$ de recorrido.  En una lista doble, el nodo *ya conoce* su anterior:

```c
// Eliminar un nodo dado su puntero — O(1)
void dlist_remove_node(DList *list, DNode *node) {
    if (node->prev) {
        node->prev->next = node->next;    // anterior salta al siguiente
    } else {
        list->head = node->next;          // era el head
    }

    if (node->next) {
        node->next->prev = node->prev;    // siguiente retrocede al anterior
    } else {
        list->tail = node->prev;          // era el tail
    }

    list->count--;
    free(node);
}
```

Cuatro enlaces se actualizan (2 ramas × 2 punteros cada una):

```
Antes:       [A]⟷[B]⟷[C]     (eliminar B)

Paso 1:  A->next = C          (anterior salta)
Paso 2:  C->prev = A          (siguiente retrocede)

Después:     [A]⟷[C]          B liberado
```

### Casos especiales

```
Eliminar el head (no hay prev):
  Antes:   [A]⟷[B]⟷[C]     head=A
  A->prev == NULL → head = A->next = B
  B->prev = NULL
  Después: [B]⟷[C]           head=B

Eliminar el tail (no hay next):
  Antes:   [A]⟷[B]⟷[C]     tail=C
  C->next == NULL → tail = C->prev = B
  B->next = NULL
  Después: [A]⟷[B]           tail=B

Eliminar el único nodo:
  Antes:   [A]                head=A, tail=A
  A->prev == NULL → head = NULL
  A->next == NULL → tail = NULL
  Después: (vacía)            head=NULL, tail=NULL
```

En Rust:

```rust
// SAFETY: node debe ser un puntero válido a un nodo de esta lista
unsafe fn remove_node(&mut self, node: *mut DNode<T>) -> T {
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
    let boxed = Box::from_raw(node);
    boxed.data
}
```

---

## Traza detallada: construir y recorrer

Operaciones: `push_back(10)`, `push_back(20)`, `push_back(30)`.

```
push_back(10):
  Crear nodo @ 0xA00: { data:10, prev:null, next:null }
  Lista vacía → head = tail = 0xA00
  len = 1

  head ──→ [null|10|null] ←── tail

push_back(20):
  Crear nodo @ 0xB00: { data:20, prev:null, next:null }
  tail != null:
    (*tail).next = 0xB00        // A00.next = B00
    (*0xB00).prev = tail        // B00.prev = A00
  tail = 0xB00
  len = 2

  head ──→ [null|10|·]⟷[·|20|null] ←── tail
            0xA00          0xB00

  Verificar invariante 4:
    A00.next = B00, B00.prev = A00  ✓

push_back(30):
  Crear nodo @ 0xC00: { data:30, prev:null, next:null }
  tail != null:
    (*tail).next = 0xC00        // B00.next = C00
    (*0xC00).prev = tail        // C00.prev = B00
  tail = 0xC00
  len = 3

  head ──→ [null|10|·]⟷[·|20|·]⟷[·|30|null] ←── tail
            0xA00        0xB00      0xC00

  Verificar:
    A00.next=B00, B00.prev=A00  ✓
    B00.next=C00, C00.prev=B00  ✓

Recorrido forward:  10 → 20 → 30
Recorrido backward: 30 → 20 → 10
```

---

## Comparación: simple vs doble

| Aspecto | Simple | Doble |
|---------|--------|-------|
| Memoria por nodo (int, 64-bit) | 16 bytes | 24 bytes |
| push_front | $O(1)$ | $O(1)$ |
| push_back (con tail) | $O(1)$ | $O(1)$ |
| pop_front | $O(1)$ | $O(1)$ |
| pop_back | $O(n)$ | $O(1)$ |
| Eliminar nodo dado puntero | $O(n)$ | $O(1)$ |
| Recorrido inverso | $O(n)$ extra espacio | $O(n)$ sin espacio extra |
| Insertar antes de un nodo dado | $O(n)$ | $O(1)$ |
| Buscar por valor | $O(n)$ | $O(n)$ |
| Complejidad de código | Baja | Media (más punteros) |
| Errores de puntero | Pocos (solo next) | Más (prev + next + consistencia) |

La lista doble no mejora la búsqueda — sigue siendo $O(n)$ porque no hay
acceso por índice.  La mejora está en **operaciones locales**: dado un puntero
a un nodo, todo lo que involucra vecinos inmediatos es $O(1)$.

---

## Patrón del doble enlace: la regla de los 4 punteros

Toda modificación a una lista doblemente enlazada (insertar o eliminar) toca
**hasta 4 punteros**:

1. El `next` del nodo anterior (o `head` si no hay anterior).
2. El `prev` del nodo siguiente (o `tail` si no hay siguiente).
3. El `prev` del nodo insertado (apunta al anterior).
4. El `next` del nodo insertado (apunta al siguiente).

Para eliminación, los punteros 3 y 4 son los del nodo que se elimina (se
ignoran, el nodo se libera).

```
Insertar X entre A y B:

  Antes:   [A]⟷[B]

  1. A->next = X        // anterior apunta al nuevo
  2. B->prev = X        // siguiente apunta al nuevo
  3. X->prev = A        // nuevo apunta al anterior
  4. X->next = B        // nuevo apunta al siguiente

  Después: [A]⟷[X]⟷[B]
```

El **orden** de estas asignaciones importa: si sobreescribes `A->next` antes
de leer el antiguo valor (que era `B`), pierdes la referencia a `B`.  Regla
segura: **configurar el nodo nuevo primero** (pasos 3 y 4), luego reconectar
los vecinos (pasos 1 y 2).

---

## Aplicaciones de la lista doble

### Editor de texto (buffer de líneas)

Cada línea es un nodo.  El cursor apunta al nodo actual.  Moverse arriba/abajo
es `prev`/`next` — $O(1)$.  Insertar o eliminar una línea en la posición
del cursor es $O(1)$.

### LRU Cache

Least Recently Used: el elemento más recientemente accedido va al frente; el
menos reciente está al final.  Al acceder a un elemento, se **mueve** al
frente — esto requiere eliminar un nodo en medio ($O(1)$ con puntero) e
insertar al frente ($O(1)$).  Combinado con un hash map para búsqueda $O(1)$,
toda la operación es $O(1)$.

### Undo/Redo

El historial de acciones es una lista doble.  El cursor señala la acción
actual.  Undo mueve hacia atrás (`prev`); redo mueve hacia adelante (`next`).
Al realizar una nueva acción después de varios undos, se eliminan las acciones
futuras (todo lo que hay después del cursor).

### Deque (cola de doble extremo)

Una lista doblemente enlazada implementa un deque completo con todas las
operaciones en $O(1)$ — exactamente lo que vimos en C04/S02/T02 con nodos
`DNode`.

---

## Errores comunes

### 1. Olvidar actualizar prev

```c
// MAL: insertar al frente sin actualizar prev del viejo head
void push_front_broken(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->next = list->head;
    // FALTA: list->head->prev = node;
    list->head = node;
}
// El viejo head todavía tiene prev == NULL
// Recorrido backward desde tail salta directamente del viejo head a NULL
```

### 2. Inconsistencia bidireccional

```c
// MAL: actualizar next sin el prev correspondiente
node->next = new_node;
// FALTA: new_node->prev = node;

// Resultado: A→B pero B no apunta a A con prev
// Forward funciona, backward se rompe
```

### 3. No manejar los extremos

```c
// MAL: eliminar nodo sin verificar si es head o tail
void remove_broken(DList *list, DNode *node) {
    node->prev->next = node->next;   // crash si node == head (prev es NULL)
    node->next->prev = node->prev;   // crash si node == tail (next es NULL)
    free(node);
}
```

### 4. Doble enlace parcial al insertar

```c
// MAL: solo 2 de los 4 punteros
void insert_after_broken(DNode *a, DNode *x) {
    x->next = a->next;
    a->next = x;
    // FALTA: x->prev = a;
    // FALTA: x->next->prev = x;   (si x->next != NULL)
}
```

### 5. Liberar sin desconectar

```c
// MAL: free sin redirigir los vecinos
void remove_leak(DList *list, DNode *node) {
    free(node);   // prev->next y next->prev ahora son dangling
    list->count--;
}
```

---

## Ejercicios

### Ejercicio 1 — Sizeof y memoria

Calcula el `sizeof` de `DNode` en una plataforma de 64 bits con `int data`.
¿Cuánta memoria total consume una lista doble de 1000 enteros (nodos + struct
DList)?  Compara con una lista simple de 1000 enteros.

<details>
<summary>Cálculo</summary>

```
DNode: int (4) + padding (4) + prev (8) + next (8) = 24 bytes
Node:  int (4) + padding (4) + next (8) = 16 bytes

Lista doble: 1000 × 24 + sizeof(DList) = 24000 + 24 = 24024 bytes
Lista simple: 1000 × 16 + sizeof(List) = 16000 + 16 = 16016 bytes
Dato puro: 1000 × 4 = 4000 bytes

Overhead doble:  24024 / 4000 = 6.0×
Overhead simple: 16016 / 4000 = 4.0×
```

Para datos pequeños como `int`, la lista doble usa 6 veces más memoria que
un array.  Para datos grandes (ej. struct de 200 bytes), el overhead relativo
baja drásticamente.
</details>

### Ejercicio 2 — Construir y recorrer

Crea manualmente una lista doble de 3 nodos `[A, B, C]` en C, configurando
todos los punteros.  Recorre forward (imprime `A B C`) y backward (imprime
`C B A`).

<details>
<summary>Código</summary>

```c
DNode *a = create_dnode('A');
DNode *b = create_dnode('B');
DNode *c = create_dnode('C');

a->next = b;  b->prev = a;
b->next = c;  c->prev = b;

DList list = { .head = a, .tail = c, .count = 3 };

// Forward: cur = head, seguir next
// Backward: cur = tail, seguir prev
```

Verificar invariante 4: `a->next->prev == a` ✓, `b->next->prev == b` ✓.
</details>

### Ejercicio 3 — Los 4 punteros

Dada la lista `[10, 30]`, inserta el nodo con valor 20 entre ellos.
Escribe las 4 asignaciones de punteros en orden seguro y dibuja el estado
antes/después.

<details>
<summary>Orden seguro</summary>

```
Antes:  [null|10|·]⟷[·|30|null]
         A              B
Nuevo:  X = {data:20, prev:?, next:?}

Paso 1: X->prev = A       // configurar nuevo primero
Paso 2: X->next = B
Paso 3: A->next = X       // reconectar vecinos
Paso 4: B->prev = X

Después: [null|10|·]⟷[·|20|·]⟷[·|30|null]
```

Si haces paso 3 antes de paso 2, pierdes la referencia a B (porque
`A->next` era la única referencia a B que tenías en las variables).
</details>

### Ejercicio 4 — Eliminar nodo dado puntero

Implementa `dlist_remove_node` en C.  Prueba con: eliminar del medio,
eliminar el head, eliminar el tail, eliminar el único nodo.

<details>
<summary>Caso más delicado</summary>

El único nodo: `prev == NULL` y `next == NULL`.  El código debe actualizar
tanto `head` como `tail` a NULL:

```c
// node->prev == NULL → list->head = node->next (= NULL)
// node->next == NULL → list->tail = node->prev (= NULL)
```

Funciona correctamente con la implementación de 4 ramas sin caso especial.
</details>

### Ejercicio 5 — Invariante bidireccional

Escribe una función `dlist_verify` que recorra la lista y verifique el
invariante 4: para cada nodo `n`, si `n->next != NULL` entonces
`n->next->prev == n`.  Retorna 1 si la lista es consistente, 0 si no.

<details>
<summary>Implementación</summary>

```c
int dlist_verify(const DList *list) {
    DNode *cur = list->head;
    if (cur && cur->prev != NULL) return 0;  // head.prev debe ser NULL

    while (cur) {
        if (cur->next) {
            if (cur->next->prev != cur) return 0;  // invariante 4
        }
        cur = cur->next;
    }

    // Verificar tail
    if (list->tail && list->tail->next != NULL) return 0;
    return 1;
}
```

Ejecutar esta función después de cada operación durante el desarrollo es
una técnica efectiva para detectar errores de enlace.
</details>

### Ejercicio 6 — pop_back $O(1)$

Implementa `pop_back` para la lista doble en C.  Compara con el `pop_back`
de la lista simple: ¿por qué aquí es $O(1)$ y allá era $O(n)$?

<details>
<summary>Respuesta</summary>

```c
int dlist_pop_back(DList *list) {
    DNode *old_tail = list->tail;
    int value = old_tail->data;

    list->tail = old_tail->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;  // era el único nodo
    }

    free(old_tail);
    list->count--;
    return value;
}
```

Es $O(1)$ porque `tail->prev` nos da el penúltimo directamente.  En la lista
simple, no existe `prev` — hay que recorrer desde `head` para encontrar
el penúltimo ($O(n)$).
</details>

### Ejercicio 7 — Recorrido con Rust unsafe

Implementa `DList<T>` en Rust con `print_forward` y `print_backward` usando
raw pointers.  Construye la lista `[10, 20, 30]` con `push_back` y muestra
ambos recorridos.

<details>
<summary>Salida esperada</summary>

```
Forward:  [10, 20, 30]
Backward: [30, 20, 10]
```

El recorrido backward en Rust usa el mismo patrón:
```rust
let mut cur = self.tail;
while !cur.is_null() {
    unsafe {
        // usar (*cur).data
        cur = (*cur).prev;
    }
}
```
</details>

### Ejercicio 8 — Insertar antes de un nodo

Implementa `insert_before(list, node, value)` que inserta un nuevo nodo con
`value` justo antes de `node`.  ¿Cuál es la complejidad?  ¿Sería posible
en una lista simple?

<details>
<summary>Análisis</summary>

$O(1)$ — tenemos `node->prev` directamente.

```c
void insert_before(DList *list, DNode *node, int value) {
    DNode *new_node = create_dnode(value);
    new_node->next = node;
    new_node->prev = node->prev;

    if (node->prev) {
        node->prev->next = new_node;
    } else {
        list->head = new_node;   // insertar antes del head
    }
    node->prev = new_node;
    list->count++;
}
```

En lista simple, necesitaríamos recorrer desde `head` para encontrar el
nodo anterior a `node` — $O(n)$.  Es precisamente esta operación la que
justifica el puntero `prev`.
</details>

### Ejercicio 9 — LRU Cache conceptual

Un LRU Cache con capacidad 3 recibe las claves `A, B, C, A, D`.  Dibuja
el estado de la lista doble después de cada acceso (el más reciente va al
frente).  ¿Qué clave se elimina cuando llega D?

<details>
<summary>Traza</summary>

```
Acceso A: [A]                        (insertar)
Acceso B: [B, A]                     (insertar al frente)
Acceso C: [C, B, A]                  (insertar al frente — lleno)
Acceso A: [A, C, B]                  (A ya existe: mover al frente)
Acceso D: [D, A, C]                  (lleno → eliminar tail (B), insertar D)
```

B se elimina porque es el tail (least recently used).  Mover A al frente
es $O(1)$: eliminar del medio ($O(1)$ con puntero) + insertar al frente
($O(1)$).
</details>

### Ejercicio 10 — Simple vs doble: cuándo elegir

Para cada escenario, indica si usarías lista simple o doble y por qué:
1. Stack (solo push/pop al frente).
2. Cola FIFO (enqueue al final, dequeue al frente).
3. Playlist de música (avanzar y retroceder canciones).
4. Buffer de undo/redo en un editor.

<details>
<summary>Respuestas</summary>

1. **Stack → simple**.  Solo push_front/pop_front — ambos $O(1)$ en lista
   simple.  El puntero `prev` extra no aporta nada.

2. **Cola FIFO → simple con tail**.  Enqueue (push_back) y dequeue (pop_front)
   son $O(1)$ con tail pointer.  No necesitamos `prev`.

3. **Playlist → doble**.  El usuario puede ir a la canción anterior (`prev`) o
   siguiente (`next`).  Lista simple requeriría $O(n)$ para retroceder.

4. **Undo/redo → doble**.  Undo es `prev` (retroceder), redo es `next`
   (avanzar).  Al hacer una nueva acción, se eliminan las futuras (truncar
   desde el cursor hacia adelante).
</details>
