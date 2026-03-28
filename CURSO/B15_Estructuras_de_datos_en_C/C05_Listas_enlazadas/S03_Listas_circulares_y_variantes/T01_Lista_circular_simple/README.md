# Lista circular simple

## Concepto

En una lista simplemente enlazada convencional, el último nodo apunta a `NULL`.
En una lista **circular**, el último nodo apunta al **primero**, formando un
anillo:

```
Lista simple:
  head ──→ [A|·]──→ [B|·]──→ [C|null]

Lista circular:
           ┌──────────────────────┐
           ↓                      │
           [A|·]──→ [B|·]──→ [C|·]┘
```

No hay `NULL` en la cadena — el recorrido puede continuar indefinidamente
pasando por los mismos nodos.

---

## Estructura

### En C

Los nodos son idénticos a una lista simple:

```c
typedef struct Node {
    int data;
    struct Node *next;
} Node;
```

La diferencia está en la **lista** — en vez de `head`, mantenemos un puntero
a `tail` (el último nodo):

```c
typedef struct CList {
    Node *tail;
    int count;
} CList;

CList clist_new(void) {
    return (CList){ .tail = NULL, .count = 0 };
}
```

¿Por qué `tail` y no `head`?

- Con `tail`, el head es `tail->next` — acceso $O(1)$ a **ambos** extremos.
- Con `head`, llegar al tail requiere recorrer toda la lista — $O(n)$.

```
tail ──→ [C|·]──→ [A|·]──→ [B|·]──→ (vuelve a C)
                    ↑
                  head = tail->next
```

### En Rust

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct CList<T> {
    tail: *mut Node<T>,
    len: usize,
}

impl<T> CList<T> {
    pub fn new() -> Self {
        CList { tail: ptr::null_mut(), len: 0 }
    }

    pub fn is_empty(&self) -> bool {
        self.tail.is_null()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}
```

---

## Recorrido con do...while

En una lista simple, recorremos con `while (cur != NULL)`.  En una lista
circular no hay `NULL` — la condición de parada es volver al nodo inicial:

```c
void clist_print(const CList *list) {
    if (!list->tail) {
        printf("[]\n");
        return;
    }

    printf("[");
    Node *cur = list->tail->next;   // head
    do {
        printf("%d", cur->data);
        cur = cur->next;
        if (cur != list->tail->next) printf(", ");
    } while (cur != list->tail->next);
    printf("]\n");
}
```

El patrón `do...while` es esencial: garantiza que ejecutamos el cuerpo al
menos una vez antes de verificar la condición.  Si usáramos `while`, la
condición `cur != head` sería falsa **inmediatamente** (empezamos en head).

```
Recorrido de [A, B, C] (tail apunta a C):

  head = tail->next = A
  cur = A  →  print A  →  cur = B
  cur = B  →  print B  →  cur = C
  cur = C  →  print C  →  cur = A  →  cur == head → STOP
```

### En Rust

```rust
impl<T: std::fmt::Display> CList<T> {
    pub fn print(&self) {
        if self.tail.is_null() {
            println!("[]");
            return;
        }

        unsafe {
            let head = (*self.tail).next;
            let mut cur = head;
            print!("[");
            loop {
                print!("{}", (*cur).data);
                cur = (*cur).next;
                if cur == head { break; }
                print!(", ");
            }
            println!("]");
        }
    }
}
```

Rust no tiene `do...while`, así que usamos `loop` con `break` al final del
cuerpo — semánticamente equivalente.

---

## Operaciones

### push_front — $O(1)$

Insertar al frente = insertar **después de tail** (porque `tail->next` es el
head):

```c
void clist_push_front(CList *list, int value) {
    Node *node = malloc(sizeof(Node));
    node->data = value;

    if (!list->tail) {
        node->next = node;         // apunta a sí mismo
        list->tail = node;
    } else {
        node->next = list->tail->next;   // nuevo → viejo head
        list->tail->next = node;         // tail → nuevo
    }
    list->count++;
}
```

```
Antes:   tail ──→ [C|·]──→ [A|·]──→ [B|·]──→ (C)

push_front(X):
  node->next = tail->next (A)     // X apunta al viejo head
  tail->next = node (X)           // tail apunta al nuevo

Después: tail ──→ [C|·]──→ [X|·]──→ [A|·]──→ [B|·]──→ (C)
                             ↑
                           nuevo head
```

### push_back — $O(1)$

Insertar al final = insertar después de tail **y actualizar tail**:

```c
void clist_push_back(CList *list, int value) {
    clist_push_front(list, value);   // insertar como head
    list->tail = list->tail->next;   // el nuevo nodo es ahora el tail
}
```

Truco elegante: `push_front` inserta después de tail (como nuevo head).
Si luego avanzamos `tail` al nodo recién insertado, ese nodo pasa de ser
el head a ser el tail:

```
push_front(X):    tail ──→ [C|·]──→ [X|·]──→ [A|·]──→ [B|·]──→ (C)
                                      ↑ head

tail = tail->next: tail ──→ [X|·]──→ [A|·]──→ [B|·]──→ [C|·]──→ (X)
                                      ↑ head
```

El head vuelve a ser A, y X queda como tail.

### pop_front — $O(1)$

```c
int clist_pop_front(CList *list) {
    Node *head = list->tail->next;
    int value = head->data;

    if (head == list->tail) {
        // Único nodo — lista queda vacía
        list->tail = NULL;
    } else {
        list->tail->next = head->next;   // tail → nuevo head
    }

    free(head);
    list->count--;
    return value;
}
```

```
Antes:   tail ──→ [C|·]──→ [A|·]──→ [B|·]──→ (C)
                             ↑ head (a eliminar)

tail->next = head->next:
         tail ──→ [C|·]──→ [B|·]──→ (C)
                             ↑ nuevo head

free(A), count--
```

### pop_back — $O(n)$

```c
int clist_pop_back(CList *list) {
    int value = list->tail->data;

    if (list->tail->next == list->tail) {
        // Único nodo
        free(list->tail);
        list->tail = NULL;
    } else {
        // Buscar penúltimo — O(n)
        Node *cur = list->tail->next;
        while (cur->next != list->tail) {
            cur = cur->next;
        }
        cur->next = list->tail->next;   // penúltimo → head (cierra el ciclo)
        free(list->tail);
        list->tail = cur;
    }

    list->count--;
    return value;
}
```

$O(n)$ — misma limitación que la lista simple: sin `prev`, encontrar el
penúltimo requiere recorrer.

### Buscar — $O(n)$

```c
Node *clist_find(const CList *list, int value) {
    if (!list->tail) return NULL;

    Node *cur = list->tail->next;
    do {
        if (cur->data == value) return cur;
        cur = cur->next;
    } while (cur != list->tail->next);

    return NULL;
}
```

### Destruir — $O(n)$

```c
void clist_destroy(CList *list) {
    if (!list->tail) return;

    Node *cur = list->tail->next;   // head
    list->tail->next = NULL;        // romper el ciclo
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    list->tail = NULL;
    list->count = 0;
}
```

El truco: **romper el ciclo** (`tail->next = NULL`) y luego recorrer como una
lista simple.  Sin este paso, el `while (cur)` nunca terminaría.

---

## Traza detallada

Operaciones: `push_back(10)`, `push_back(20)`, `push_back(30)`, `pop_front()`,
`push_front(5)`.

```
push_back(10):
  Lista vacía → node->next = node (apunta a sí mismo)
  tail = node

  tail ──→ [10|·]──→ (sí mismo)
  head = tail->next = 10

push_back(20):
  push_front(20): node(20)->next = tail->next (10)
                  tail->next = node(20)
    tail ──→ [10|·]──→ [20|·]──→ (10)
  tail = tail->next (20):
    tail ──→ [20|·]──→ [10|·]──→ (20)
  head = tail->next = 10

  Circular: 10 → 20 → 10 → 20 → ...

push_back(30):
  push_front(30): node(30)->next = tail->next (10)
                  tail->next = node(30)
    tail ──→ [20|·]──→ [30|·]──→ [10|·]──→ (20)
  tail = tail->next (30):
    tail ──→ [30|·]──→ [10|·]──→ [20|·]──→ (30)
  head = tail->next = 10

  Contenido en orden: [10, 20, 30]

pop_front() → 10:
  head = tail->next = nodo(10)
  tail->next = head->next = nodo(20)
  free(nodo 10), count=2

  tail ──→ [30|·]──→ [20|·]──→ (30)
  head = tail->next = 20
  Contenido: [20, 30]

push_front(5):
  node(5)->next = tail->next (20)
  tail->next = node(5)
  count=3

  tail ──→ [30|·]──→ [5|·]──→ [20|·]──→ (30)
  head = tail->next = 5
  Contenido: [5, 20, 30]
```

---

## Ventajas de la lista circular

### Rotación $O(1)$

Rotar la lista (el primero pasa al final, o viceversa) es simplemente mover
`tail`:

```c
// Rotar izquierda: head pasa a ser tail
void clist_rotate_left(CList *list) {
    if (list->tail) {
        list->tail = list->tail->next;
    }
}

// Rotar derecha: tail pasa a ser head (necesita penúltimo — O(n))
```

```
Antes:     tail ──→ [C|·]──→ [A|·]──→ [B|·]──→ (C)
           Contenido: [A, B, C]

rotate_left (tail = tail->next):
           tail ──→ [A|·]──→ [B|·]──→ [C|·]──→ (A)
           Contenido: [B, C, A]
```

Una sola asignación.  En un array o lista simple, rotar requiere mover
elementos — $O(n)$.

### Round-robin scheduling

El algoritmo de scheduling más simple: dar a cada proceso un quantum de tiempo
y pasar al siguiente.  Recorrido infinito natural:

```c
void round_robin(CList *processes, int rounds) {
    if (!processes->tail) return;

    Node *cur = processes->tail->next;
    for (int i = 0; i < rounds * processes->count; i++) {
        printf("Executing process %d\n", cur->data);
        cur = cur->next;
    }
}
```

No hay verificación de `NULL`, no hay reset al principio — el recorrido
circular maneja todo.

### Buffer circular (streaming)

Procesar un flujo de datos donde los datos antiguos se sobreescriben:

```c
void clist_overwrite(CList *list, int value) {
    // Avanzar tail y sobreescribir head
    list->tail = list->tail->next;
    list->tail->data = value;
}
```

El buffer tiene tamaño fijo — sin allocations después de la inicialización.

---

## Implementación en Rust

```rust
use std::ptr;
use std::marker::PhantomData;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct CList<T> {
    tail: *mut Node<T>,
    len: usize,
}

impl<T> CList<T> {
    pub fn new() -> Self {
        CList { tail: ptr::null_mut(), len: 0 }
    }

    pub fn is_empty(&self) -> bool {
        self.tail.is_null()
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn push_front(&mut self, value: T) {
        let node = Box::into_raw(Box::new(Node {
            data: value,
            next: ptr::null_mut(),
        }));

        if self.tail.is_null() {
            unsafe { (*node).next = node; }
            self.tail = node;
        } else {
            unsafe {
                (*node).next = (*self.tail).next;
                (*self.tail).next = node;
            }
        }
        self.len += 1;
    }

    pub fn push_back(&mut self, value: T) {
        self.push_front(value);
        unsafe {
            self.tail = (*self.tail).next;
        }
    }

    pub fn pop_front(&mut self) -> Option<T> {
        if self.tail.is_null() {
            return None;
        }
        unsafe {
            let head = (*self.tail).next;
            if head == self.tail {
                self.tail = ptr::null_mut();
            } else {
                (*self.tail).next = (*head).next;
            }
            self.len -= 1;
            Some(Box::from_raw(head).data)
        }
    }

    pub fn peek_front(&self) -> Option<&T> {
        if self.tail.is_null() {
            None
        } else {
            unsafe { Some(&(*(*self.tail).next).data) }
        }
    }

    pub fn peek_back(&self) -> Option<&T> {
        if self.tail.is_null() {
            None
        } else {
            unsafe { Some(&(*self.tail).data) }
        }
    }

    pub fn rotate_left(&mut self) {
        if !self.tail.is_null() {
            unsafe { self.tail = (*self.tail).next; }
        }
    }
}

impl<T> Drop for CList<T> {
    fn drop(&mut self) {
        if self.tail.is_null() { return; }
        unsafe {
            let head = (*self.tail).next;
            (*self.tail).next = ptr::null_mut();   // romper ciclo
            let mut cur = head;
            while !cur.is_null() {
                let next = (*cur).next;
                let _ = Box::from_raw(cur);
                cur = next;
            }
        }
    }
}

// Iterador (finito — recorre una sola vuelta)
pub struct Iter<'a, T> {
    current: *const Node<T>,
    head: *const Node<T>,
    started: bool,
    _marker: PhantomData<&'a T>,
}

impl<T> CList<T> {
    pub fn iter(&self) -> Iter<'_, T> {
        let head = if self.tail.is_null() {
            ptr::null()
        } else {
            unsafe { (*self.tail).next as *const _ }
        };
        Iter {
            current: head,
            head,
            started: false,
            _marker: PhantomData,
        }
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current.is_null() {
            return None;
        }
        if self.started && self.current == self.head {
            return None;   // completamos una vuelta
        }
        self.started = true;
        unsafe {
            let node = &*self.current;
            self.current = node.next;
            Some(&node.data)
        }
    }
}
```

El iterador usa un flag `started` para distinguir el estado inicial (aún no
hemos visitado ningún nodo) del estado final (volvimos al head después de una
vuelta completa).

---

## Comparación: lineal vs circular

| Aspecto | Lista simple (lineal) | Lista circular |
|---------|----------------------|----------------|
| Último nodo apunta a | `NULL` | Head (primer nodo) |
| Condición de fin | `cur == NULL` | `cur == head` (do-while) |
| Acceso al tail (con tail ptr) | $O(1)$ | $O(1)$ |
| Acceso al head (con tail ptr) | $O(n)$ sin head ptr | $O(1)$ — `tail->next` |
| Rotación | $O(n)$ | $O(1)$ |
| Recorrido cíclico | Requiere reset manual | Natural |
| Destruir | `while (cur)` directo | Romper ciclo primero |
| Insertar al frente | Modificar head | Modificar `tail->next` |
| Bug típico | Olvidar NULL check | Loop infinito |
| Complejidad de código | Baja | Media (do-while, caso 1 nodo) |

---

## Errores comunes

### 1. Loop infinito por olvidar la condición de parada

```c
// MAL: while sin parada
Node *cur = list->tail->next;
while (cur) {                       // NUNCA es NULL en circular
    printf("%d ", cur->data);
    cur = cur->next;                // loop infinito
}

// BIEN: do-while con comparación al head
Node *head = list->tail->next;
Node *cur = head;
do {
    printf("%d ", cur->data);
    cur = cur->next;
} while (cur != head);
```

### 2. No manejar el nodo único

```c
// MAL: pop_front sin caso especial de nodo único
int clist_pop_front_broken(CList *list) {
    Node *head = list->tail->next;
    list->tail->next = head->next;   // tail->next apunta a head->next
    free(head);
    // Si head == tail, tail ahora es dangling y tail->next apunta a head (liberado)
}
```

Cuando la lista tiene un solo nodo, `head == tail`.  Después del free,
`tail` apunta a memoria liberada.  Siempre verificar `head == tail` primero.

### 3. Destruir sin romper el ciclo

```c
// MAL: destruir como lista simple
void clist_destroy_broken(CList *list) {
    Node *cur = list->tail->next;
    while (cur) {                    // nunca NULL → loop infinito
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
}
```

### 4. Comparar con NULL en vez de con head

```c
// MAL: buscar con NULL como parada
Node *clist_find_broken(CList *list, int value) {
    Node *cur = list->tail->next;
    while (cur != NULL) {            // nunca NULL → loop infinito
        if (cur->data == value) return cur;
        cur = cur->next;
    }
    return NULL;
}
```

### 5. push en lista vacía sin auto-referencia

```c
// MAL: nodo no apunta a sí mismo
void clist_push_broken(CList *list, int value) {
    Node *node = malloc(sizeof(Node));
    node->data = value;
    node->next = list->tail;   // debería ser node, no tail (que es NULL)
    list->tail = node;
}
// node->next es NULL → no es circular
```

---

## Ejercicios

### Ejercicio 1 — Construir y recorrer

Crea una lista circular con `push_back` de los valores `[10, 20, 30, 40, 50]`.
Imprímela.  Luego haz `rotate_left` dos veces e imprime de nuevo.

<details>
<summary>Salida esperada</summary>

```
[10, 20, 30, 40, 50]

Después de 2 rotaciones:
[30, 40, 50, 10, 20]
```

Cada `rotate_left` mueve el head al final: `[10,20,30,40,50]` → `[20,30,40,50,10]` → `[30,40,50,10,20]`.
</details>

### Ejercicio 2 — Recorrido con do-while vs while

Escribe dos funciones `print_dowhile` y `print_while_broken` para la lista
circular.  Demuestra que la versión con `while` entra en loop infinito
(puedes añadir un contador de seguridad para detenerla).

<details>
<summary>Demostración</summary>

```c
void print_while_broken(const CList *list, int max_iters) {
    Node *cur = list->tail->next;
    int i = 0;
    while (cur != NULL && i < max_iters) {   // cur nunca es NULL
        printf("%d ", cur->data);
        cur = cur->next;
        i++;
    }
    printf("\n[stopped after %d iterations]\n", i);
}
```

Para una lista de 3 nodos con `max_iters=10`: imprime los 3 valores
repetidos hasta 10.  Sin el contador, sería infinito.
</details>

### Ejercicio 3 — Nodo único

Traza manualmente las operaciones sobre una lista circular:
`push_front(42)` → `pop_front()`.  Muestra el estado de `tail`, `node->next`,
y la condición `head == tail` en cada paso.

<details>
<summary>Traza</summary>

```
push_front(42):
  node @ 0xA00, data=42
  lista vacía → node->next = node (0xA00 → 0xA00)
  tail = 0xA00
  head = tail->next = 0xA00

  tail ──→ [42|·]──→ (sí mismo)
  head == tail ✓ (un solo nodo)

pop_front():
  head = tail->next = 0xA00
  head == tail → lista quedará vacía
  tail = NULL
  free(0xA00)
  retorna 42

  tail = NULL, lista vacía
```

El caso de un solo nodo es el que más requiere atención: `head == tail` y
`node->next == node`.
</details>

### Ejercicio 4 — push_back con el truco de push_front

Explica paso a paso por qué `push_back` puede implementarse como
`push_front` seguido de `tail = tail->next`.  Dibuja el estado después de
cada instrucción para la lista `[A, B]` al hacer `push_back(C)`.

<details>
<summary>Traza</summary>

```
Estado inicial:
  tail ──→ [B|·]──→ [A|·]──→ (B)
  head = tail->next = A
  Contenido: [A, B]

push_front(C):
  node(C)->next = tail->next = A
  tail->next = node(C)
  tail ──→ [B|·]──→ [C|·]──→ [A|·]──→ (B)
  head = tail->next = C
  Contenido: [C, A, B]    ← C al frente, no es lo que queremos

tail = tail->next:
  tail ──→ [C|·]──→ [A|·]──→ [B|·]──→ (C)
  head = tail->next = A
  Contenido: [A, B, C]    ← C al final ✓
```

Al avanzar `tail`, C pasa de ser el head (posición 0) al tail (última
posición), y el head vuelve a ser A.
</details>

### Ejercicio 5 — Recorrido cíclico con n vueltas

Escribe una función que recorra la lista circular exactamente `k` vueltas,
imprimiendo cada elemento cada vez que lo visita.

<details>
<summary>Implementación</summary>

```c
void clist_print_k_rounds(const CList *list, int k) {
    if (!list->tail || k <= 0) return;

    Node *cur = list->tail->next;
    int total = k * list->count;
    for (int i = 0; i < total; i++) {
        printf("%d ", cur->data);
        cur = cur->next;
    }
    printf("\n");
}
```

Para `[1, 2, 3]` con `k=2`: imprime `1 2 3 1 2 3`.

Alternativa sin usar `count`:
```c
int round = 0;
Node *head = list->tail->next;
Node *cur = head;
do {
    printf("%d ", cur->data);
    cur = cur->next;
    if (cur == head) round++;
} while (round < k);
```
</details>

### Ejercicio 6 — Destruir: romper el ciclo

Implementa `clist_destroy` de dos formas:
1. Rompiendo el ciclo (`tail->next = NULL`) y luego recorriendo.
2. Sin romper el ciclo, usando un contador.

<details>
<summary>Versión con contador</summary>

```c
void clist_destroy_v2(CList *list) {
    if (!list->tail) return;

    Node *cur = list->tail->next;
    for (int i = 0; i < list->count; i++) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    list->tail = NULL;
    list->count = 0;
}
```

La versión con romper ciclo es más robusta (funciona incluso si `count` es
incorrecto).  La versión con contador asume que `count` es preciso.
</details>

### Ejercicio 7 — Round-robin scheduling

Simula un scheduler round-robin con 4 procesos (PIDs: 1, 2, 3, 4) y 3 rondas.
Cada ronda da a cada proceso un turno.  Usa la lista circular para el
recorrido natural.

<details>
<summary>Salida esperada</summary>

```
Round 1: P1 P2 P3 P4
Round 2: P1 P2 P3 P4
Round 3: P1 P2 P3 P4
```

```c
void round_robin(CList *procs, int rounds) {
    if (!procs->tail) return;
    Node *cur = procs->tail->next;
    for (int r = 1; r <= rounds; r++) {
        printf("Round %d: ", r);
        for (int i = 0; i < procs->count; i++) {
            printf("P%d ", cur->data);
            cur = cur->next;
        }
        printf("\n");
    }
}
```

La lista circular no necesita "resetear" el puntero entre rondas —
`cur` naturalmente continúa donde la ronda anterior terminó (que es el
head, porque recorrimos exactamente `count` nodos).
</details>

### Ejercicio 8 — Implementar en Rust

Implementa `CList<T>` en Rust con raw pointers: `push_front`, `push_back`,
`pop_front`, `rotate_left`, `iter` (una vuelta), y `Drop`.  Prueba con
`i32` y con `String`.

<details>
<summary>Prueba con String</summary>

```rust
let mut list = CList::new();
list.push_back("alpha".to_string());
list.push_back("beta".to_string());
list.push_back("gamma".to_string());

for s in list.iter() {
    print!("{s} ");
}
// alpha beta gamma

list.rotate_left();
for s in list.iter() {
    print!("{s} ");
}
// beta gamma alpha
```

`String` no es `Copy` — `pop_front` **mueve** el String fuera del nodo, y
`Drop` destruye los Strings restantes cuando la lista se destruye.
</details>

### Ejercicio 9 — Circular vs lineal: insertar al final

Compara `push_back` para:
1. Lista simple **sin** tail pointer (recorrer $O(n)$).
2. Lista simple **con** tail pointer ($O(1)$, pero tail es extra).
3. Lista circular con tail pointer ($O(1)$, head = `tail->next` gratis).

¿Cuál usa menos memoria en la estructura de la lista?

<details>
<summary>Comparación</summary>

```
1. Lista simple sin tail:  head (8 bytes)           push_back O(n)
2. Lista simple con tail:  head (8) + tail (8)      push_back O(1)
3. Lista circular con tail: tail (8)                 push_back O(1)
```

La circular usa menos memoria en la estructura (un solo puntero vs dos)
y ofrece $O(1)$ en ambos extremos.  El costo es la complejidad adicional
del `do-while` y manejar el nodo auto-referente.
</details>

### Ejercicio 10 — Detectar circularidad

Dada una lista que podría ser lineal (termina en NULL) o circular, escribe
una función que determine cuál es.  Usa el algoritmo de Floyd (tortuga y
liebre).

<details>
<summary>Implementación</summary>

```c
int is_circular(Node *head) {
    if (!head) return 0;

    Node *slow = head;          // avanza 1 paso
    Node *fast = head->next;    // avanza 2 pasos

    while (fast && fast->next) {
        if (slow == fast) return 1;   // se encontraron → ciclo
        slow = slow->next;
        fast = fast->next->next;
    }
    return 0;   // fast llegó a NULL → lineal
}
```

Si la lista es circular, la liebre (2 pasos) alcanza a la tortuga (1 paso)
en a lo sumo $n$ iteraciones.  Si es lineal, `fast` llega a NULL.

Complejidad: $O(n)$ tiempo, $O(1)$ espacio.
</details>
