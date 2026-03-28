# Cola con lista enlazada en C

## Motivación

El array circular (T02) es excelente cuando se conoce la capacidad máxima de
antemano.  Pero tiene limitaciones:

| Limitación del array circular | Solución con lista enlazada |
|-------------------------------|----------------------------|
| Capacidad fija (o redimensionamiento costoso) | Crece y decrece bajo demanda |
| Redimensionar requiere linearizar y copiar todo | Cada enqueue es una sola asignación |
| Memoria reservada aunque la cola esté casi vacía | Usa solo la memoria que necesita |

La lista enlazada ofrece **crecimiento y decrecimiento natural**: cada
`enqueue` asigna un nodo, cada `dequeue` lo libera.  No hay desperdicio de
capacidad ni necesidad de redimensionar.

El costo: cada nodo incluye un puntero `next` (8 bytes en 64 bits), y las
asignaciones con `malloc`/`free` son más lentas que indexar un array.

---

## Estructura del nodo y la cola

En una cola con lista enlazada necesitamos **dos punteros**:

- `front` → apunta al primer nodo (el más antiguo, próximo a salir).
- `rear` → apunta al último nodo (el más reciente, recién insertado).

```
 front                                rear
   ↓                                   ↓
┌───────┐    ┌───────┐    ┌───────┐    ┌───────┐
│ A │ ──┼───▶│ B │ ──┼───▶│ C │ ──┼───▶│ D │ / │
└───────┘    └───────┘    └───────┘    └───────┘

dequeue extrae de front (A)
enqueue inserta después de rear
```

### Definición en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *front;    /* NULL si vacía */
    Node *rear;     /* NULL si vacía */
    int count;
} Queue;
```

---

## Operaciones fundamentales

### Inicialización

```c
void queue_init(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}
```

### Enqueue (insertar al final)

Crear un nodo nuevo y enlazarlo después de `rear`:

```c
bool queue_enqueue(Queue *q, int value) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;       /* fallo de asignación */

    node->data = value;
    node->next = NULL;

    if (q->rear == NULL) {
        /* Cola vacía: el nuevo nodo es front Y rear */
        q->front = node;
    } else {
        /* Enlazar después del último */
        q->rear->next = node;
    }
    q->rear = node;
    q->count++;
    return true;
}
```

El caso especial es la cola vacía: tanto `front` como `rear` deben apuntar al
nuevo nodo.

```
Cola vacía:        front = NULL, rear = NULL

Después de enqueue(A):
  front              rear
    ↓                 ↓
┌───────┐
│ A │ / │             front == rear (un solo nodo)
└───────┘

Después de enqueue(B):
  front    rear
    ↓       ↓
┌───────┐  ┌───────┐
│ A │ ──┼─▶│ B │ / │
└───────┘  └───────┘
```

### Dequeue (extraer del frente)

Extraer el nodo de `front` y avanzar `front` al siguiente:

```c
bool queue_dequeue(Queue *q, int *out) {
    if (q->front == NULL) return false;    /* cola vacía */

    Node *old = q->front;
    *out = old->data;
    q->front = old->next;

    if (q->front == NULL) {
        /* Era el último nodo: rear también debe ser NULL */
        q->rear = NULL;
    }

    free(old);
    q->count--;
    return true;
}
```

El caso especial inverso: si `front` avanza a `NULL`, la cola quedó vacía y
`rear` también debe ponerse a `NULL`.  Olvidar esto deja un **dangling
pointer** en `rear` — un bug grave.

```
Antes:
  front    rear
    ↓       ↓
┌───────┐  ┌───────┐
│ A │ ──┼─▶│ B │ / │
└───────┘  └───────┘

dequeue() → A:
           front/rear
              ↓
┌───────┐  ┌───────┐
│ A │ ──┼  │ B │ / │      old = nodo A
└───────┘  └───────┘      front = B, free(A)

dequeue() → B:
  front = NULL
  rear  = NULL             old = nodo B, free(B)
                           ← cola vacía
```

### Front (observar sin extraer)

```c
bool queue_front(const Queue *q, int *out) {
    if (q->front == NULL) return false;
    *out = q->front->data;
    return true;
}
```

### Consultas

```c
bool queue_is_empty(const Queue *q) {
    return q->front == NULL;
}

int queue_size(const Queue *q) {
    return q->count;
}
```

### Destrucción (liberar toda la memoria)

```c
void queue_destroy(Queue *q) {
    Node *cur = q->front;
    while (cur != NULL) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}
```

Recorre la lista desde `front` hasta el final, liberando cada nodo.  Es
fundamental llamar esto antes de que la cola salga de scope — de lo contrario,
memory leak.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *front;
    Node *rear;
    int count;
} Queue;

void queue_init(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}

bool queue_enqueue(Queue *q, int value) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    node->data = value;
    node->next = NULL;

    if (q->rear == NULL) {
        q->front = node;
    } else {
        q->rear->next = node;
    }
    q->rear = node;
    q->count++;
    return true;
}

bool queue_dequeue(Queue *q, int *out) {
    if (q->front == NULL) return false;
    Node *old = q->front;
    *out = old->data;
    q->front = old->next;
    if (q->front == NULL) q->rear = NULL;
    free(old);
    q->count--;
    return true;
}

bool queue_front_peek(const Queue *q, int *out) {
    if (q->front == NULL) return false;
    *out = q->front->data;
    return true;
}

bool queue_is_empty(const Queue *q) { return q->front == NULL; }
int  queue_size(const Queue *q) { return q->count; }

void queue_destroy(Queue *q) {
    Node *cur = q->front;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}

void queue_print(const Queue *q) {
    printf("[");
    for (Node *n = q->front; n != NULL; n = n->next) {
        printf("%d", n->data);
        if (n->next) printf(", ");
    }
    printf("] (size=%d)\n", q->count);
}

int main(void) {
    Queue q;
    queue_init(&q);

    for (int i = 1; i <= 5; i++) {
        queue_enqueue(&q, i * 10);
        printf("enqueue(%d): ", i * 10);
        queue_print(&q);
    }

    int val;
    while (queue_dequeue(&q, &val)) {
        printf("dequeue() -> %d: ", val);
        queue_print(&q);
    }

    printf("dequeue on empty: %s\n",
           queue_dequeue(&q, &val) ? "ok" : "empty");

    queue_destroy(&q);
    return 0;
}
```

Salida:

```
enqueue(10): [10] (size=1)
enqueue(20): [10, 20] (size=2)
enqueue(30): [10, 20, 30] (size=3)
enqueue(40): [10, 20, 30, 40] (size=4)
enqueue(50): [10, 20, 30, 40, 50] (size=5)
dequeue() -> 10: [20, 30, 40, 50] (size=4)
dequeue() -> 20: [30, 40, 50] (size=3)
dequeue() -> 30: [40, 50] (size=2)
dequeue() -> 40: [50] (size=1)
dequeue() -> 50: [] (size=0)
dequeue on empty: empty
```

---

## Traza detallada de punteros

Secuencia: enqueue(A), enqueue(B), dequeue, enqueue(C), dequeue, dequeue.

```
Operación       front      rear       Lista                count
──────────────────────────────────────────────────────────────────
init            NULL       NULL       (vacía)              0

enqueue(A)      →[A|/]     →[A|/]    [A|/]                1
                ↑ front == rear (un nodo)

enqueue(B)      →[A|─]     →[B|/]    [A|─]→[B|/]          2
                             ↑ rear avanzó

dequeue→A       →[B|/]     →[B|/]    [B|/]                1
                free([A|─])
                front == rear (un nodo otra vez)

enqueue(C)      →[B|─]     →[C|/]    [B|─]→[C|/]          2

dequeue→B       →[C|/]     →[C|/]    [C|/]                1
                free([B|─])

dequeue→C       NULL       NULL       (vacía)              0
                free([C|/])
                front avanzó a NULL → rear = NULL
```

Cada `malloc` crea un nodo; cada `free` lo destruye.  No hay memoria
preasignada ni desperdiciada.

---

## Cola con nodo sentinela

Un **nodo sentinela** (*dummy node*) es un nodo sin datos que siempre está
presente, eliminando los casos especiales de cola vacía:

```
Cola vacía con sentinela:

  front    rear
    ↓       ↓
┌───────┐
│ * │ / │          * = sentinela (data irrelevante)
└───────┘

Cola con elementos [A, B]:

  front                  rear
    ↓                     ↓
┌───────┐  ┌───────┐  ┌───────┐
│ * │ ──┼─▶│ A │ ──┼─▶│ B │ / │
└───────┘  └───────┘  └───────┘
              ↑
    front->next = primer dato real
```

### Ventaja: sin bifurcaciones en enqueue/dequeue

```c
typedef struct {
    Node *front;    /* siempre apunta al sentinela */
    Node *rear;     /* último nodo real, o sentinela si vacía */
    int count;
} QueueSentinel;

void queue_init(QueueSentinel *q) {
    Node *sentinel = malloc(sizeof(Node));
    sentinel->next = NULL;
    q->front = sentinel;
    q->rear = sentinel;
    q->count = 0;
}

bool queue_enqueue(QueueSentinel *q, int value) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    node->data = value;
    node->next = NULL;

    q->rear->next = node;     /* siempre válido — rear nunca es NULL */
    q->rear = node;
    q->count++;
    return true;
}

bool queue_dequeue(QueueSentinel *q, int *out) {
    if (q->front == q->rear) return false;   /* solo sentinela = vacía */

    Node *first = q->front->next;   /* primer dato real */
    *out = first->data;
    q->front->next = first->next;

    if (first == q->rear) {
        q->rear = q->front;   /* era el último → rear vuelve a sentinela */
    }

    free(first);
    q->count--;
    return true;
}

bool queue_is_empty(const QueueSentinel *q) {
    return q->front == q->rear;
}

void queue_destroy(QueueSentinel *q) {
    Node *cur = q->front;    /* incluye el sentinela */
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}
```

### Comparación: con y sin sentinela

| Aspecto | Sin sentinela | Con sentinela |
|---------|--------------|---------------|
| `enqueue` | `if (rear == NULL)` — caso especial | Sin bifurcación |
| `dequeue` | `if (front->next == NULL)` — actualizar rear | Todavía necesita `if (first == rear)` |
| Memoria extra | Ninguna | Un nodo permanente |
| Complejidad de código | Dos ramas en enqueue | Ligeramente más uniforme |
| `is_empty` | `front == NULL` | `front == rear` |
| `init` | Asignar dos punteros a NULL | `malloc` del sentinela |
| `destroy` | Liberar todos los nodos | Liberar todos + sentinela |

En la práctica, la versión sin sentinela es más común para colas porque los
casos especiales son simples.  El sentinela brilla más en **listas doblemente
enlazadas** (que veremos en C05), donde elimina más casos especiales.

---

## Comparación: array circular vs lista enlazada

| Criterio | Array circular | Lista enlazada |
|----------|---------------|----------------|
| `enqueue` | $O(1)$ | $O(1)$ |
| `dequeue` | $O(1)$ | $O(1)$ |
| `front` | $O(1)$ | $O(1)$ |
| Capacidad | Fija o con redimensionamiento | Ilimitada (hasta agotar memoria) |
| Memoria por elemento | Solo el dato (`sizeof(int)`) | Dato + puntero (`sizeof(int) + sizeof(Node*)`) |
| Localidad de caché | Excelente (datos contiguos) | Pobre (nodos dispersos en el heap) |
| `malloc`/`free` por operación | 0 (excepto redimensionar) | 1 por enqueue + 1 por dequeue |
| Acceso por índice | $O(1)$ con fórmula de módulo | $O(n)$ recorriendo la lista |
| Espacio desperdiciado | Slots vacíos en el array | Punteros `next` |
| Implementación | Más compleja (módulo, vacía/llena) | Más directa (punteros) |

### ¿Cuándo elegir cada una?

| Situación | Recomendación |
|-----------|---------------|
| Capacidad conocida de antemano | Array circular |
| Rendimiento crítico, muchos enqueue/dequeue | Array circular (caché) |
| Tamaño muy variable (0 a millones) | Lista enlazada |
| Sistema embebido sin `malloc` | Array circular (estático) |
| Elementos grandes (structs de 100+ bytes) | Array circular (evitar overhead del puntero, datos contiguos) |
| Elementos pequeños (int, char) | Array circular (el puntero sería mayor que el dato) |
| Necesidad de inspeccionar posiciones intermedias | Array circular ($O(1)$ acceso) |

En la mayoría de los casos prácticos, el array circular es preferible por su
mejor rendimiento de caché.  La lista enlazada es la elección cuando la
capacidad es completamente impredecible o cuando se necesita inserción/eliminación
en posiciones arbitrarias (lo que ya no es una cola pura).

---

## Errores comunes

### 1. Dangling pointer en rear al vaciar

```c
/* MAL */
bool queue_dequeue(Queue *q, int *out) {
    if (q->front == NULL) return false;
    Node *old = q->front;
    *out = old->data;
    q->front = old->next;
    free(old);
    q->count--;
    return true;
    /* Si front se volvió NULL, rear sigue apuntando al nodo liberado */
}
```

Corrección: después de `q->front = old->next`, verificar `if (q->front == NULL)
q->rear = NULL;`.

### 2. Memory leak al no destruir la cola

```c
void some_function(void) {
    Queue q;
    queue_init(&q);
    queue_enqueue(&q, 42);
    /* return sin queue_destroy → los nodos quedan en el heap */
}
```

En Rust, `Drop` se invoca automáticamente.  En C es responsabilidad del
programador.

### 3. Usar el nodo después de free

```c
/* MAL */
Node *old = q->front;
free(old);
q->front = old->next;    /* UB: old ya fue liberado */
```

Corrección: guardar `old->next` **antes** de `free`:

```c
Node *old = q->front;
q->front = old->next;    /* primero avanzar */
free(old);               /* luego liberar */
```

### 4. Olvidar verificar malloc

```c
/* MAL */
Node *node = malloc(sizeof(Node));
node->data = value;    /* si malloc retornó NULL → crash */
```

Siempre verificar: `if (!node) return false;`.

### 5. Sizeof incorrecto

```c
/* MAL */
Node *node = malloc(sizeof(Node*));    /* solo 8 bytes (un puntero) */
                                       /* Node necesita 16+ bytes  */

/* BIEN */
Node *node = malloc(sizeof(Node));     /* tamaño completo del nodo */

/* MEJOR — inmune a cambios de tipo */
Node *node = malloc(sizeof(*node));
```

---

## Memoria: visualización del heap

Cada `enqueue` pide un bloque del heap; cada `dequeue` lo devuelve.  Los nodos
no están necesariamente contiguos:

```
Heap después de enqueue(A), enqueue(B), enqueue(C):

Dirección    Contenido
─────────────────────────
0x1000       [data=A, next=0x1040]    ← front
0x1020       (otro dato del programa)
0x1040       [data=B, next=0x1080]
0x1060       (fragmento libre)
0x1080       [data=C, next=NULL]      ← rear

Después de dequeue() → A:

0x1000       (libre — devuelto a free)
0x1040       [data=B, next=0x1080]    ← front
0x1080       [data=C, next=NULL]      ← rear
```

La dispersión en el heap causa **cache misses**: acceder a `front->next`
requiere saltar a una dirección potencialmente lejana.  El array circular, con
datos contiguos, tiene accesos secuenciales que son mucho más cache-friendly.

Para $N$ nodos en un sistema de 64 bits:

| Estructura | Bytes por elemento | Total para $N = 10^6$ |
|-----------|-------------------|----------------------|
| Array de `int` | 4 | 4 MB |
| Lista de `int` | 4 (data) + 8 (next) + 8 (malloc overhead) ≈ 24 | 24 MB |

La lista usa ~6× más memoria para datos de tipo `int`.

---

## Cola genérica con lista enlazada

Para almacenar cualquier tipo, usamos `void*`:

```c
typedef struct GNode {
    void *data;           /* puntero al dato (copiado con memcpy) */
    struct GNode *next;
} GNode;

typedef struct {
    GNode *front;
    GNode *rear;
    int count;
    size_t elem_size;
} GenericQueue;

GenericQueue *gqueue_create(size_t elem_size) {
    GenericQueue *q = malloc(sizeof(GenericQueue));
    if (!q) return NULL;
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
    q->elem_size = elem_size;
    return q;
}

bool gqueue_enqueue(GenericQueue *q, const void *elem) {
    GNode *node = malloc(sizeof(GNode));
    if (!node) return false;

    node->data = malloc(q->elem_size);
    if (!node->data) { free(node); return false; }
    memcpy(node->data, elem, q->elem_size);
    node->next = NULL;

    if (q->rear == NULL) {
        q->front = node;
    } else {
        q->rear->next = node;
    }
    q->rear = node;
    q->count++;
    return true;
}

bool gqueue_dequeue(GenericQueue *q, void *out) {
    if (q->front == NULL) return false;

    GNode *old = q->front;
    memcpy(out, old->data, q->elem_size);
    q->front = old->next;
    if (q->front == NULL) q->rear = NULL;

    free(old->data);
    free(old);
    q->count--;
    return true;
}

void gqueue_destroy(GenericQueue *q) {
    GNode *cur = q->front;
    while (cur) {
        GNode *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    free(q);
}
```

Nota que aquí cada nodo tiene **dos asignaciones** (`malloc` para el nodo y
`malloc` para el dato).  Alternativa: asignar un solo bloque con `malloc(sizeof(GNode) + elem_size)` y usar alineación flexible (*flexible array
member*):

```c
typedef struct GNode {
    struct GNode *next;
    char data[];       /* flexible array member — C99 */
} GNode;

/* Asignación: */
GNode *node = malloc(sizeof(GNode) + q->elem_size);
memcpy(node->data, elem, q->elem_size);
```

Esto reduce las asignaciones a una por nodo y mejora la localidad de caché.

---

## Ejercicios

### Ejercicio 1 — Traza de punteros

Traza la cola con lista enlazada para la secuencia:

```
enqueue(5), enqueue(10), enqueue(15), dequeue, enqueue(20),
dequeue, dequeue, dequeue
```

Muestra `front`, `rear`, la lista completa y `count` después de cada operación.
Indica cuándo `rear` se pone a `NULL`.

<details>
<summary>Predice cuándo rear se vuelve NULL</summary>

`rear` se pone a `NULL` en el último dequeue (el cuarto), cuando se extrae 20
y `front` avanza a `NULL`.  Los dequeues retornan: 5, 10, 15, 20 (FIFO).
Nodos creados: 4.  Nodos liberados: 4.  No hay memory leak.
</details>

### Ejercicio 2 — Detectar el bug del dangling pointer

El siguiente código tiene un bug.  Encuéntralo y corrígelo:

```c
bool queue_dequeue(Queue *q, int *out) {
    if (q->front == NULL) return false;
    Node *old = q->front;
    free(old);
    *out = old->data;
    q->front = old->next;
    q->count--;
    return true;
}
```

<details>
<summary>Problema y solución</summary>

Se accede a `old->data` y `old->next` **después** de `free(old)` — uso después
de liberación (use-after-free, UB).  Corrección: guardar los datos antes de
liberar:

```c
*out = old->data;
q->front = old->next;
if (q->front == NULL) q->rear = NULL;   /* también faltaba esto */
free(old);
```
</details>

### Ejercicio 3 — Cola con sentinela

Implementa la cola completa con nodo sentinela (init, enqueue, dequeue, front,
is_empty, size, destroy).  Verifica que produce los mismos resultados que la
versión sin sentinela para una secuencia de 1000 operaciones.

<details>
<summary>Pista para la verificación</summary>

Ejecuta la misma secuencia aleatoria en ambas colas (con y sin sentinela).
Después de cada operación, verifica que `size` y `front` coinciden.  Si
`dequeue` tiene éxito en una, debe tener éxito en la otra con el mismo valor.
</details>

### Ejercicio 4 — Memory leak detector

Añade un contador global que se incrementa en cada `malloc` de nodo y se
decrementa en cada `free`.  Al final del programa, verifica que el contador
sea 0 (sin leaks).  Introduce deliberadamente un leak (olvidar `queue_destroy`)
y verifica que el detector lo reporta.

<details>
<summary>Estructura del detector</summary>

```c
static int alloc_count = 0;

Node *node_alloc(int value) {
    Node *n = malloc(sizeof(Node));
    if (n) { n->data = value; n->next = NULL; alloc_count++; }
    return n;
}

void node_free(Node *n) {
    free(n);
    alloc_count--;
}

/* Al final: */
printf("Leak check: %d nodes still allocated\n", alloc_count);
```
</details>

### Ejercicio 5 — Concatenar dos colas

Escribe una función que concatene la cola `b` al final de la cola `a` en
$O(1)$, dejando `b` vacía.  Esto es trivial con listas enlazadas (enlazar
`a->rear->next` a `b->front`) pero imposible en $O(1)$ con array circular.

<details>
<summary>Implementación</summary>

```c
void queue_concat(Queue *a, Queue *b) {
    if (b->front == NULL) return;          /* b vacía */
    if (a->rear == NULL) {
        a->front = b->front;              /* a vacía: adoptar b */
    } else {
        a->rear->next = b->front;         /* enlazar */
    }
    a->rear = b->rear;
    a->count += b->count;
    b->front = NULL;                      /* vaciar b */
    b->rear = NULL;
    b->count = 0;
}
```

$O(1)$: solo se modifican 3-4 punteros.  Con array circular necesitaríamos
copiar todos los elementos de b → $O(n)$.
</details>

### Ejercicio 6 — Invertir la cola in-place

Invierte el orden de los elementos de la cola usando solo la lista enlazada
(sin array auxiliar ni pila).  Pista: invertir los punteros `next` de todos los
nodos, luego intercambiar `front` y `rear`.

<details>
<summary>Algoritmo</summary>

```c
void queue_reverse(Queue *q) {
    Node *prev = NULL;
    Node *cur = q->front;
    q->rear = q->front;     /* el antiguo front será el nuevo rear */
    while (cur != NULL) {
        Node *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    q->front = prev;        /* el antiguo último nodo es el nuevo front */
}
```

Es el mismo algoritmo de inversión de lista enlazada — $O(n)$ tiempo, $O(1)$
espacio.
</details>

### Ejercicio 7 — Queue con flexible array member

Reimplementa la cola genérica usando un solo `malloc` por nodo con flexible
array member:

```c
typedef struct GNode {
    struct GNode *next;
    char data[];
} GNode;
```

Verifica que funciona con `int`, `double` y una struct de 3 campos.

<details>
<summary>Pista de asignación</summary>

```c
GNode *node = malloc(sizeof(GNode) + q->elem_size);
memcpy(node->data, elem, q->elem_size);
```

Para dequeue: `memcpy(out, old->data, q->elem_size)`.  Un solo `free(old)`
libera todo.  Esto reduce el overhead de `malloc` a la mitad (1 llamada vs 2).
</details>

### Ejercicio 8 — Benchmark: array circular vs lista

Mide el tiempo de $10^6$ operaciones enqueue seguidas de $10^6$ dequeues para
ambas implementaciones.  Luego mide $10^6$ operaciones intercaladas
(enqueue-dequeue-enqueue-dequeue...).  Reporta los tiempos.

<details>
<summary>Predicción</summary>

El array circular será más rápido en ambos escenarios, típicamente 2-5× para
operaciones secuenciales.  La diferencia se debe a:
1. Sin `malloc`/`free` por operación (el costo dominante de la lista).
2. Mejor localidad de caché (datos contiguos vs dispersos).

Para las intercaladas, la diferencia puede ser menor porque la lista reutiliza
la misma dirección de memoria (un `malloc` tras un `free` suele retornar la
dirección recién liberada).
</details>

### Ejercicio 9 — Cola multipista

Implementa un sistema con $K$ colas (una por cada "cajero").  Los clientes
(enteros 1..N) se asignan al cajero con la cola más corta.  Muestra qué
clientes atiende cada cajero.  Usa la cola con lista enlazada.

<details>
<summary>Estructura sugerida</summary>

```c
#define NUM_CASHIERS 3
Queue cashiers[NUM_CASHIERS];

/* Encontrar cajero con cola más corta */
int shortest_queue(void) {
    int min_idx = 0;
    for (int i = 1; i < NUM_CASHIERS; i++) {
        if (queue_size(&cashiers[i]) < queue_size(&cashiers[min_idx])) {
            min_idx = i;
        }
    }
    return min_idx;
}
```

Este es un preview del problema de simulación de T04/S02.
</details>

### Ejercicio 10 — Valgrind y AddressSanitizer

Compila el programa completo de la cola con `-fsanitize=address` (GCC/Clang) o
ejecútalo con Valgrind.  Verifica que reporta 0 errores.  Luego introduce
deliberadamente:
(a) un use-after-free, (b) un memory leak, (c) un doble free.
Observa cómo cada herramienta reporta el error.

<details>
<summary>Comandos</summary>

```bash
# AddressSanitizer
gcc -g -fsanitize=address queue.c -o queue
./queue

# Valgrind
gcc -g queue.c -o queue
valgrind --leak-check=full ./queue
```

ASan detecta use-after-free y doble free inmediatamente (abort).  Valgrind
detecta leaks al final del programa.  Ambas herramientas son indispensables
para programación con memoria manual en C.
</details>
