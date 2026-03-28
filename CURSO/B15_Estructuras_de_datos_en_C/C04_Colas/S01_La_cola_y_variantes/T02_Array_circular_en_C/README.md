# Array circular en C

## El problema del array lineal

En T01 vimos que una cola con array lineal tiene un defecto fundamental: al
avanzar `front` con cada `dequeue`, las posiciones liberadas al inicio del array
se desperdician:

```
Capacidad 6.  Después de 4 enqueues y 3 dequeues:

[_, _, _, D, _, _]     front=3, rear=3
                       ↑↑↑
                   espacio desperdiciado

Un enqueue más:
[_, _, _, D, E, _]     front=3, rear=4

Dos enqueues más:
[_, _, _, D, E, F]     front=3, rear=5  ← "lleno" con 3 huecos al inicio
```

El array parece lleno (`rear == capacity - 1`) aunque solo tiene 3 elementos.
El espacio al inicio es irrecuperable sin desplazar todo ($O(n)$).

---

## La solución: wrap-around con módulo

La idea del **array circular** (*circular buffer*, *ring buffer*) es tratar el
array como si el final conectara con el inicio.  Cuando un índice llega al
final del array, "da la vuelta" al principio:

```
Indices lógicos:  ... → 3 → 4 → 5 → 0 → 1 → 2 → 3 → ...

Array físico:
┌───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │
└───┴───┴───┴───┴───┴───┘
  ↑                   ↑
  └───── se conectan ─┘
```

El operador módulo (`%`) implementa el wrap-around:

```c
next_index = (current_index + 1) % capacity;
```

| `current` | `capacity` | `(current + 1) % capacity` |
|-----------|-----------|---------------------------|
| 0 | 6 | 1 |
| 4 | 6 | 5 |
| 5 | 6 | **0** ← wrap-around |

---

## Estructura de datos

```c
#define QUEUE_CAPACITY 8

typedef struct {
    int data[QUEUE_CAPACITY];
    int front;    /* índice del próximo elemento a extraer   */
    int rear;     /* índice donde se insertará el siguiente  */
    int count;    /* número de elementos actuales            */
} Queue;
```

### Estado de los índices

- `front` apunta al primer elemento (el más antiguo).
- `rear` apunta a la **siguiente posición libre** (donde irá el próximo
  enqueue).
- Ambos avanzan con wrap-around: `(index + 1) % QUEUE_CAPACITY`.

Visualización:

```
Capacidad 6, con elementos [A, B, C]:

     front              rear
       ↓                 ↓
┌───┬───┬───┬───┬───┬───┐
│   │ A │ B │ C │   │   │
└───┴───┴───┴───┴───┴───┘
  0   1   2   3   4   5

Después de dequeue() → A:

         front          rear
           ↓             ↓
┌───┬───┬───┬───┬───┬───┐
│   │   │ B │ C │   │   │
└───┴───┴───┴───┴───┴───┘

Después de enqueue(D), enqueue(E), enqueue(F), enqueue(G):

         front                  rear (wrap-around)
           ↓                     ↓
┌───┬───┬───┬───┬───┬───┐
│ G │   │ B │ C │ D │ E │  F está en [5], G en [0]
└───┴───┴───┴───┴───┴───┘

¡El rear pasó del final al inicio!
```

---

## Distinción vacía / llena

El mayor problema de diseño de un array circular es distinguir entre cola
**vacía** y cola **llena**.  En ambos casos, si solo usamos `front` y `rear`,
se cumple `front == rear`:

```
Cola vacía:               Cola llena (sin count):
front = rear = 0          front = rear = 2

┌───┬───┬───┬───┐         ┌───┬───┬───┬───┐
│   │   │   │   │         │ C │ D │ A │ B │
└───┴───┴───┴───┘         └───┴───┴───┴───┘
  ↑                             ↑
front/rear                  front/rear
```

### Tres estrategias

| Estrategia | Mecanismo | Capacidad efectiva | Complejidad |
|-----------|-----------|-------------------|-------------|
| **Contador** (`count`) | Mantener un entero con el número de elementos | `CAPACITY` completa | $O(1)$ todas las operaciones |
| **Slot vacío** | Siempre dejar una posición sin usar; llena cuando `(rear+1) % cap == front` | `CAPACITY - 1` | $O(1)$, sin variable extra |
| **Flag booleano** | Un `bool full` que se actualiza en enqueue/dequeue | `CAPACITY` completa | $O(1)$, un byte extra |

La estrategia con **contador** es la más clara y la que usaremos.  La de
**slot vacío** es común en implementaciones de bajo nivel (kernels, drivers)
porque evita la variable extra.

---

## Implementación con contador

```c
#include <stdio.h>
#include <stdbool.h>

#define QUEUE_CAPACITY 8

typedef struct {
    int data[QUEUE_CAPACITY];
    int front;
    int rear;
    int count;
} Queue;

void queue_init(Queue *q) {
    q->front = 0;
    q->rear = 0;
    q->count = 0;
}

bool queue_is_empty(const Queue *q) {
    return q->count == 0;
}

bool queue_is_full(const Queue *q) {
    return q->count == QUEUE_CAPACITY;
}

int queue_size(const Queue *q) {
    return q->count;
}

bool queue_enqueue(Queue *q, int value) {
    if (queue_is_full(q)) return false;

    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->count++;
    return true;
}

bool queue_dequeue(Queue *q, int *out) {
    if (queue_is_empty(q)) return false;

    *out = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->count--;
    return true;
}

bool queue_front(const Queue *q, int *out) {
    if (queue_is_empty(q)) return false;

    *out = q->data[q->front];
    return true;
}
```

### Programa de prueba

```c
int main(void) {
    Queue q;
    queue_init(&q);

    /* Encolar 1..5 */
    for (int i = 1; i <= 5; i++) {
        queue_enqueue(&q, i * 10);
        printf("enqueue(%d)  size=%d\n", i * 10, queue_size(&q));
    }

    /* Desencolar 3 */
    int val;
    for (int i = 0; i < 3; i++) {
        queue_dequeue(&q, &val);
        printf("dequeue() -> %d  size=%d\n", val, queue_size(&q));
    }

    /* Encolar 5 más (provocar wrap-around) */
    for (int i = 6; i <= 10; i++) {
        bool ok = queue_enqueue(&q, i * 10);
        printf("enqueue(%d) -> %s  size=%d\n",
               i * 10, ok ? "ok" : "FULL", queue_size(&q));
    }

    /* Vaciar */
    while (queue_dequeue(&q, &val)) {
        printf("dequeue() -> %d\n", val);
    }

    return 0;
}
```

Salida esperada:

```
enqueue(10)  size=1
enqueue(20)  size=2
enqueue(30)  size=3
enqueue(40)  size=4
enqueue(50)  size=5
dequeue() -> 10  size=4
dequeue() -> 20  size=3
dequeue() -> 30  size=2
enqueue(60) -> ok  size=3
enqueue(70) -> ok  size=4
enqueue(80) -> ok  size=5
enqueue(90) -> ok  size=6
enqueue(100) -> ok  size=7
dequeue() -> 40
dequeue() -> 50
dequeue() -> 60
dequeue() -> 70
dequeue() -> 80
dequeue() -> 90
dequeue() -> 100
```

---

## Traza detallada del wrap-around

Capacidad 4, secuencia: enqueue(A), enqueue(B), enqueue(C), dequeue, dequeue,
enqueue(D), enqueue(E), enqueue(F).

```
Operación       data[]          front  rear  count  Nota
─────────────────────────────────────────────────────────────
init            [_, _, _, _]     0      0     0
enqueue(A)      [A, _, _, _]     0      1     1
enqueue(B)      [A, B, _, _]     0      2     2
enqueue(C)      [A, B, C, _]     0      3     3
dequeue→A       [_, B, C, _]     1      3     2
dequeue→B       [_, _, C, _]     2      3     1
enqueue(D)      [_, _, C, D]     2      0     2     rear: 3→0 (wrap)
enqueue(E)      [E, _, C, D]     2      1     3     rear en posición 0→1
enqueue(F)      [E, F, C, D]     2      2     4     ← LLENA (count==4)

Estado final:
         front          rear
           ↓             ↓
┌───┬───┬───┬───┐
│ E │ F │ C │ D │
└───┴───┴───┴───┘
  0   1   2   3

Orden FIFO: C, D, E, F (empezando desde front=2, con wrap-around)
```

La belleza del módulo: los datos no se mueven nunca.  Solo `front` y `rear`
avanzan en círculo.

---

## Implementación con slot vacío

La alternativa sin `count`: reservar siempre una posición vacía.  La cola está
llena cuando `(rear + 1) % cap == front`:

```c
#define QUEUE_CAPACITY 8   /* capacidad efectiva: 7 */

typedef struct {
    int data[QUEUE_CAPACITY];
    int front;
    int rear;
} QueueSlot;

void queue_init(QueueSlot *q) {
    q->front = 0;
    q->rear = 0;
}

bool queue_is_empty(const QueueSlot *q) {
    return q->front == q->rear;
}

bool queue_is_full(const QueueSlot *q) {
    return (q->rear + 1) % QUEUE_CAPACITY == q->front;
}

int queue_size(const QueueSlot *q) {
    return (q->rear - q->front + QUEUE_CAPACITY) % QUEUE_CAPACITY;
}

bool queue_enqueue(QueueSlot *q, int value) {
    if (queue_is_full(q)) return false;
    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    return true;
}

bool queue_dequeue(QueueSlot *q, int *out) {
    if (queue_is_empty(q)) return false;
    *out = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    return true;
}
```

### Fórmula de size sin contador

```c
int queue_size(const QueueSlot *q) {
    return (q->rear - q->front + QUEUE_CAPACITY) % QUEUE_CAPACITY;
}
```

¿Por qué sumar `QUEUE_CAPACITY`?  Porque `rear - front` puede ser negativo
cuando `rear` ha dado la vuelta y está "detrás" de `front`:

| `front` | `rear` | `rear - front` | `+ cap` | `% cap` | Resultado |
|---------|--------|----------------|---------|---------|-----------|
| 2 | 5 | 3 | 11 | 3 | 3 ✓ |
| 5 | 2 | -3 | 5 | 5 | 5 ✓ (wrap-around) |
| 0 | 0 | 0 | 8 | 0 | 0 ✓ (vacía) |

### Comparación de estrategias

| Aspecto | Con `count` | Con slot vacío |
|---------|------------|----------------|
| Variables | `front`, `rear`, `count` | `front`, `rear` |
| Capacidad real | `CAPACITY` | `CAPACITY - 1` |
| `is_empty` | `count == 0` | `front == rear` |
| `is_full` | `count == CAPACITY` | `(rear+1) % cap == front` |
| `size` | `count` | `(rear - front + cap) % cap` |
| Claridad | Más legible | Más sutil |
| Uso típico | Aplicaciones generales | Kernels, buffers de hardware |

---

## Módulo con potencias de 2

Cuando la capacidad es una potencia de 2 ($2^k$), el módulo se puede reemplazar
por un AND bit a bit, que es más rápido:

```c
/* Si CAPACITY = 8 (2³): */
next = (index + 1) & (CAPACITY - 1);    /* equivale a % CAPACITY */

/* CAPACITY - 1 = 7 = 0b0111 */
/* & 0b0111 conserva solo los 3 bits bajos → wrap automático */
```

| `index` | `index + 1` | Binario | `& 0b0111` | Resultado |
|---------|-------------|---------|-----------|-----------|
| 5 | 6 | `0110` | `0110` | 6 |
| 6 | 7 | `0111` | `0111` | 7 |
| 7 | 8 | `1000` | `0000` | **0** ← wrap |

Esto es lo que hacen las implementaciones de alto rendimiento (`VecDeque` de
Rust, `ArrayDeque` de Java).  La restricción de potencia de 2 vale la pena por
la ganancia de velocidad.

---

## Cola genérica con void*

Para hacer la cola reutilizable con cualquier tipo, podemos usar `void*` y
`memcpy`:

```c
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char *data;           /* buffer de bytes crudos */
    size_t elem_size;     /* tamaño de cada elemento */
    size_t capacity;
    size_t front;
    size_t rear;
    size_t count;
} GenericQueue;

GenericQueue *gqueue_create(size_t capacity, size_t elem_size) {
    GenericQueue *q = malloc(sizeof(GenericQueue));
    if (!q) return NULL;
    q->data = malloc(capacity * elem_size);
    if (!q->data) { free(q); return NULL; }
    q->elem_size = elem_size;
    q->capacity = capacity;
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    return q;
}

void gqueue_destroy(GenericQueue *q) {
    free(q->data);
    free(q);
}

bool gqueue_enqueue(GenericQueue *q, const void *elem) {
    if (q->count == q->capacity) return false;
    memcpy(q->data + q->rear * q->elem_size, elem, q->elem_size);
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;
    return true;
}

bool gqueue_dequeue(GenericQueue *q, void *out) {
    if (q->count == 0) return false;
    memcpy(out, q->data + q->front * q->elem_size, q->elem_size);
    q->front = (q->front + 1) % q->capacity;
    q->count--;
    return true;
}
```

### Uso

```c
int main(void) {
    /* Cola de doubles */
    GenericQueue *q = gqueue_create(10, sizeof(double));

    double val = 3.14;
    gqueue_enqueue(q, &val);
    val = 2.71;
    gqueue_enqueue(q, &val);

    double out;
    gqueue_dequeue(q, &out);
    printf("%.2f\n", out);   /* 3.14 */

    gqueue_destroy(q);
    return 0;
}
```

### Limitaciones de void*

| Limitación | Problema |
|-----------|---------|
| Sin type safety | Se puede encolar un `int` y desencolar un `double` — UB |
| `elem_size` en runtime | No hay verificación en compilación |
| No funciona con tipos de tamaño variable | Strings requieren almacenar punteros, no los datos |
| Sin constructores/destructores | El dato se copia bit a bit — no hay `free` de recursos internos |

Rust resuelve todo esto con genéricos (`Queue<T>`) — sin costo en runtime y con
verificación completa en compilación.  Lo veremos en T04.

---

## Redimensionamiento dinámico

Para una cola que crezca según la demanda, necesitamos redimensionar el array
cuando esté lleno.  El procedimiento es más complejo que para una pila porque
los elementos pueden estar "partidos" por el wrap-around:

```
Antes (capacidad 4, llena):
     front      rear
       ↓         ↓
┌───┬───┬───┬───┐
│ C │ D │ A │ B │       Orden lógico: A, B, C, D
└───┴───┴───┴───┘
  0   1   2   3         front=2, rear=2

Después de redimensionar a 8:
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │   │   │   │   │    front=0, rear=4
└───┴───┴───┴───┴───┴───┴───┴───┘
```

No basta con `realloc` — hay que **linearizar** los elementos:

```c
bool queue_grow(Queue *q) {
    size_t new_cap = q->capacity * 2;
    int *new_data = malloc(new_cap * sizeof(int));
    if (!new_data) return false;

    /* Copiar elementos en orden lógico (from front to rear) */
    for (size_t i = 0; i < q->count; i++) {
        new_data[i] = q->data[(q->front + i) % q->capacity];
    }

    free(q->data);
    q->data = new_data;
    q->capacity = new_cap;
    q->front = 0;
    q->rear = q->count;
    return true;
}
```

### ¿Por qué no funciona realloc?

`realloc` solo extiende el bloque al final.  Si los datos están partidos por
el wrap-around, la extensión no los reagrupa:

```
Antes (cap=4): [C, D | A, B]   front=2
realloc a 8:   [C, D | A, B, _, _, _, _]
                                ↑ rear se pone en 4
                                pero front sigue en 2

Dequeue retorna: A, B, C, D, ¿basura en 4-5?
```

Los datos en posiciones 0-1 quedarían "detrás" de rear, creando un hueco
ilegítimo.  La linearización explícita es obligatoria.

### Análisis amortizado del redimensionamiento

Duplicar la capacidad cada vez que se llena: costo amortizado $O(1)$ por
enqueue (mismo análisis que `Vec::push` en Rust o el vector dinámico en C).
Cada elemento se copia como máximo $O(\log n)$ veces en total a lo largo de
todas las redimensiones.

---

## Complejidad de todas las operaciones

| Operación | Tiempo | Notas |
|-----------|--------|-------|
| `enqueue` | $O(1)$ | $O(1)$ amortizado con redimensionamiento |
| `dequeue` | $O(1)$ | |
| `front` | $O(1)$ | |
| `is_empty` | $O(1)$ | |
| `is_full` | $O(1)$ | Solo para capacidad fija |
| `size` | $O(1)$ | Con `count`; $O(1)$ con fórmula de módulo |
| Espacio | $O(n)$ | Exactamente `capacity` slots (posible desperdicio si $n \ll$ capacity) |

---

## Errores comunes

### 1. Olvidar el módulo

```c
/* MAL */
q->rear = q->rear + 1;          /* desborda el array */

/* BIEN */
q->rear = (q->rear + 1) % QUEUE_CAPACITY;
```

### 2. Confundir vacía con llena (sin count ni slot vacío)

```c
/* MAL — sin mecanismo de distinción */
bool is_empty(Queue *q) { return q->front == q->rear; }
bool is_full(Queue *q)  { return q->front == q->rear; }   /* ¡mismo test! */
```

### 3. Usar realloc para redimensionar

```c
/* MAL — no lineariza los datos */
q->data = realloc(q->data, new_cap * sizeof(int));
q->capacity = new_cap;
/* Los elementos partidos por wrap-around quedan desordenados */
```

### 4. Off-by-one en la fórmula de size

```c
/* MAL */
int size = (q->rear - q->front) % QUEUE_CAPACITY;
/* Si rear < front, el resultado es negativo (en C, % con negativos
   tiene signo definido por implementación hasta C99) */

/* BIEN */
int size = (q->rear - q->front + QUEUE_CAPACITY) % QUEUE_CAPACITY;
```

### 5. No inicializar front y rear

```c
/* MAL — front y rear tienen basura */
Queue q;
queue_enqueue(&q, 42);   /* escribe en posición basura */

/* BIEN */
Queue q;
queue_init(&q);
```

---

## Visualización del ciclo de vida

```
Estado 1 — Vacía (cap=6):
┌───┬───┬───┬───┬───┬───┐
│   │   │   │   │   │   │   front=0, rear=0, count=0
└───┴───┴───┴───┴───┴───┘
  ↑
 f/r

Estado 2 — Tras enqueue(A,B,C,D):
┌───┬───┬───┬───┬───┬───┐
│ A │ B │ C │ D │   │   │   front=0, rear=4, count=4
└───┴───┴───┴───┴───┴───┘
  ↑               ↑
  f               r

Estado 3 — Tras dequeue()×2  → A, B:
┌───┬───┬───┬───┬───┬───┐
│   │   │ C │ D │   │   │   front=2, rear=4, count=2
└───┴───┴───┴───┴───┴───┘
          ↑       ↑
          f       r

Estado 4 — Tras enqueue(E,F,G,H):
┌───┬───┬───┬───┬───┬───┐
│ G │ H │ C │ D │ E │ F │   front=2, rear=2, count=6  ← LLENA
└───┴───┴───┴───┴───┴───┘
          ↑
        f/r

Estado 5 — Tras dequeue()×3  → C, D, E:
┌───┬───┬───┬───┬───┬───┐
│ G │ H │   │   │   │ F │   front=5, rear=2, count=3
└───┴───┴───┴───┴───┴───┘
          ↑           ↑
          r           f

Orden FIFO de salida: F, G, H  (front avanza 5→0→1)
```

---

## Ejercicios

### Ejercicio 1 — Traza completa

Traza una cola circular de capacidad 5 (con `count`) para la secuencia:

```
enqueue(10), enqueue(20), enqueue(30), dequeue, dequeue,
enqueue(40), enqueue(50), enqueue(60), enqueue(70), dequeue,
dequeue, dequeue
```

Muestra `front`, `rear`, `count` y el contenido del array después de cada
operación.

<details>
<summary>Predice cuándo ocurre el wrap-around</summary>

El wrap-around de `rear` ocurre cuando llega a posición 5 (→ 0).  Con los 3
enqueue iniciales: rear=3.  Tras 2 dequeues: front=2.  Tras 4 enqueues más:
rear pasa por 3, 4, 0, 1.  El wrap ocurre en el tercer enqueue de la segunda
tanda (rear: 4→0).  Los dequeues finales retornan 30, 40, 50 (front: 2→3→4).
</details>

### Ejercicio 2 — Slot vacío vs contador

Implementa la cola circular con la estrategia de slot vacío (sin `count`).
Para capacidad 5, ¿cuántos elementos puede almacenar realmente?  Verifica con
un programa que intente encolar 5 elementos.

<details>
<summary>Respuesta</summary>

Con slot vacío, capacidad efectiva = `CAPACITY - 1` = 4.  El quinto enqueue
retorna `false` (llena) porque `(rear + 1) % 5 == front`.  Necesitas declarar
un array de tamaño 6 para almacenar 5 elementos.
</details>

### Ejercicio 3 — Operación % con potencia de 2

Reimplementa la cola circular usando `& (CAPACITY - 1)` en lugar de
`% CAPACITY`, con `CAPACITY = 16`.  Escribe un programa que verifique que
ambas versiones producen resultados idénticos para 10000 operaciones aleatorias
de enqueue/dequeue.

<details>
<summary>Pista de verificación</summary>

```c
#include <assert.h>
/* Después de cada operación, verificar: */
assert(queue_size(&q_mod) == queue_size(&q_and));
/* Y que dequeue retorna el mismo valor en ambas */
```

Para generar operaciones aleatorias: `rand() % 2` decide entre enqueue (con
valor `rand()`) y dequeue.  Procesar la misma secuencia en ambas colas.
</details>

### Ejercicio 4 — Función queue_peek_at

Implementa una función que permita acceder al elemento en la posición $i$
(0 = frente, `count-1` = final) sin extraerlo:

```c
bool queue_peek_at(const Queue *q, int index, int *out);
```

<details>
<summary>Fórmula clave</summary>

```c
bool queue_peek_at(const Queue *q, int index, int *out) {
    if (index < 0 || index >= q->count) return false;
    *out = q->data[(q->front + index) % QUEUE_CAPACITY];
    return true;
}
```

El módulo convierte el índice lógico (relativo a `front`) en índice físico (con
wrap-around).  Complejidad: $O(1)$.
</details>

### Ejercicio 5 — Redimensionamiento

Implementa una cola circular con capacidad dinámica que se duplique cuando esté
llena.  Comienza con capacidad 4 y encola 20 elementos.  Imprime un mensaje
cada vez que se redimensione, indicando la nueva capacidad.

<details>
<summary>Predicción de redimensionamientos</summary>

Capacidad inicial: 4.  Se redimensiona al encolar el 5to → cap 8.  Al 9no →
cap 16.  Al 17vo → cap 32.  Total: 3 redimensionamientos para 20 elementos.
Patrón: se redimensiona en los enqueues 5, 9, 17 (potencias de 2 + 1).
</details>

### Ejercicio 6 — Imprimir la cola en orden

Escribe una función `queue_print` que imprima todos los elementos de la cola
en orden FIFO (del frente al final) sin modificar la cola.  Debe funcionar
correctamente cuando los datos están "partidos" por el wrap-around.

<details>
<summary>Implementación</summary>

```c
void queue_print(const Queue *q) {
    printf("[");
    for (int i = 0; i < q->count; i++) {
        if (i > 0) printf(", ");
        printf("%d", q->data[(q->front + i) % QUEUE_CAPACITY]);
    }
    printf("]\n");
}
```

El truco es iterar con índice lógico `i` de 0 a `count-1` y convertir a
índice físico con `(front + i) % capacity`.
</details>

### Ejercicio 7 — Cola circular overwrite

Implementa una variante donde `enqueue` en cola llena **sobreescribe** el
elemento más antiguo (avanzando `front`) en lugar de rechazar.  Esto es útil
para buffers de log donde solo importan los últimos $N$ eventos.

<details>
<summary>Modificación necesaria</summary>

```c
void queue_enqueue_overwrite(Queue *q, int value) {
    if (queue_is_full(q)) {
        q->front = (q->front + 1) % QUEUE_CAPACITY;  /* descartar más antiguo */
        q->count--;
    }
    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->count++;
}
```

Siempre tiene éxito (no retorna `bool`).  El elemento descartado se pierde.
</details>

### Ejercicio 8 — Cola genérica con structs

Usa la cola genérica con `void*` para encolar structs `Task`:

```c
typedef struct {
    int id;
    char name[32];
    int priority;
} Task;
```

Encola 5 tareas y desencólalas en orden FIFO.  Imprime cada tarea al
desencolarla.

<details>
<summary>Pista de uso</summary>

```c
GenericQueue *q = gqueue_create(10, sizeof(Task));
Task t = {1, "compile", 3};
gqueue_enqueue(q, &t);
/* ... */
Task out;
gqueue_dequeue(q, &out);
printf("Task %d: %s (pri=%d)\n", out.id, out.name, out.priority);
```

Funciona porque `sizeof(Task)` incluye el padding — `memcpy` copia la struct
completa bit a bit.
</details>

### Ejercicio 9 — Verificación de invariantes

Escribe una función `queue_check_invariants` que verifique las invariantes
internas de la cola:

1. `0 <= front < CAPACITY`
2. `0 <= rear < CAPACITY`
3. `0 <= count <= CAPACITY`
4. `(front + count) % CAPACITY == rear`

Llámala después de cada operación en un programa de prueba.

<details>
<summary>Sobre el invariante 4</summary>

`(front + count) % CAPACITY == rear` expresa que `rear` está exactamente
`count` posiciones adelante de `front` (con wrap-around).  Si esta invariante
se rompe, hay un bug en enqueue o dequeue.  Es una excelente herramienta de
debugging — úsala con `assert`.
</details>

### Ejercicio 10 — Benchmark: array circular vs dos pilas

Implementa ambas versiones de cola (array circular y dos pilas) y compara el
rendimiento para $10^6$ operaciones alternadas (enqueue, dequeue, enqueue,
dequeue, ...) y para $10^6$ enqueues seguidos de $10^6$ dequeues.

<details>
<summary>Predice cuál patrón favorece a cada implementación</summary>

**Operaciones alternadas**: el array circular es consistente ($O(1)$ cada una).
Las dos pilas también son $O(1)$ cada una (siempre hay un elemento en outbox).
Rendimiento similar.

**Enqueues seguidos + dequeues seguidos**: array circular es $O(1)$ cada uno.
Dos pilas: los enqueues son $O(1)$, pero el primer dequeue vuelca $10^6$
elementos de inbox a outbox — un spike de $O(n)$.  El costo amortizado sigue
siendo $O(1)$, pero la latencia del primer dequeue es alta.  En sistemas de
tiempo real, este spike es inaceptable.
</details>
