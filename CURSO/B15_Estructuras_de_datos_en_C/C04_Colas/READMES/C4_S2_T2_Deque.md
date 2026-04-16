# Deque (Double-Ended Queue)

## Concepto

Un **deque** (pronunciado "deck") es una colección lineal que permite inserción
y eliminación en **ambos extremos**:

```
      push_front              push_back
          ↓                       ↓
    ┌───┬───┬───┬───┬───┐
    │ A │ B │ C │ D │ E │
    └───┴───┴───┴───┴───┘
          ↑                       ↑
      pop_front               pop_back
```

El deque generaliza tanto la pila como la cola:

| Si solo usas... | Se comporta como... |
|-----------------|---------------------|
| `push_back` + `pop_back` | **Pila** (LIFO) |
| `push_back` + `pop_front` | **Cola** (FIFO) |
| `push_front` + `pop_front` | **Pila** (LIFO, por el otro extremo) |
| `push_front` + `pop_back` | **Cola** (FIFO, invertida) |
| Las cuatro operaciones | **Deque** completo |

---

## Definición del TAD

### Operaciones

| Operación | Descripción | Complejidad esperada |
|-----------|-------------|---------------------|
| `push_front(dq, x)` | Insertar al frente | $O(1)$ |
| `push_back(dq, x)` | Insertar al final | $O(1)$ |
| `pop_front(dq)` | Extraer del frente | $O(1)$ |
| `pop_back(dq)` | Extraer del final | $O(1)$ |
| `front(dq)` | Consultar el frente | $O(1)$ |
| `back(dq)` | Consultar el final | $O(1)$ |
| `is_empty(dq)` | ¿Está vacío? | $O(1)$ |
| `size(dq)` | Número de elementos | $O(1)$ |

Todas $O(1)$ — el deque es la estructura lineal más exigente en cuanto a
operaciones eficientes en ambos extremos.

### Restricción input/output

Existen variantes restringidas:

| Variante | push_front | push_back | pop_front | pop_back |
|----------|-----------|-----------|-----------|----------|
| Deque completo | ✓ | ✓ | ✓ | ✓ |
| **Input-restricted** | ✗ | ✓ | ✓ | ✓ |
| **Output-restricted** | ✓ | ✓ | ✓ | ✗ |

Un deque input-restricted es una cola que también permite `pop_back`.  Un
output-restricted permite insertar por ambos lados pero solo extraer por el
frente.

---

## Implementación con array circular

El deque con array circular es una extensión directa de la cola circular (T02
de S01).  La diferencia: además de avanzar `rear` en `push_back`, ahora también
podemos **retroceder** `front` en `push_front`.

### Retroceder un índice circular

Para `push_front` necesitamos mover `front` **hacia atrás**:

```c
front = (front - 1 + capacity) % capacity;
```

El `+ capacity` evita índices negativos (en C, `(-1) % 8` puede dar -1, no 7).

### Implementación en C

```c
#include <stdio.h>
#include <stdbool.h>

#define DEQUE_CAPACITY 8

typedef struct {
    int data[DEQUE_CAPACITY];
    int front;     /* índice del primer elemento */
    int rear;      /* índice siguiente al último */
    int count;
} Deque;

void deque_init(Deque *dq) {
    dq->front = 0;
    dq->rear = 0;
    dq->count = 0;
}

bool deque_is_empty(const Deque *dq) { return dq->count == 0; }
bool deque_is_full(const Deque *dq)  { return dq->count == DEQUE_CAPACITY; }
int  deque_size(const Deque *dq)     { return dq->count; }

bool deque_push_back(Deque *dq, int value) {
    if (deque_is_full(dq)) return false;
    dq->data[dq->rear] = value;
    dq->rear = (dq->rear + 1) % DEQUE_CAPACITY;
    dq->count++;
    return true;
}

bool deque_push_front(Deque *dq, int value) {
    if (deque_is_full(dq)) return false;
    dq->front = (dq->front - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY;
    dq->data[dq->front] = value;
    dq->count++;
    return true;
}

bool deque_pop_front(Deque *dq, int *out) {
    if (deque_is_empty(dq)) return false;
    *out = dq->data[dq->front];
    dq->front = (dq->front + 1) % DEQUE_CAPACITY;
    dq->count--;
    return true;
}

bool deque_pop_back(Deque *dq, int *out) {
    if (deque_is_empty(dq)) return false;
    dq->rear = (dq->rear - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY;
    *out = dq->data[dq->rear];
    dq->count--;
    return true;
}

bool deque_front(const Deque *dq, int *out) {
    if (deque_is_empty(dq)) return false;
    *out = dq->data[dq->front];
    return true;
}

bool deque_back(const Deque *dq, int *out) {
    if (deque_is_empty(dq)) return false;
    *out = dq->data[(dq->rear - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY];
    return true;
}
```

### Programa de prueba

```c
void deque_print(const Deque *dq) {
    printf("[");
    for (int i = 0; i < dq->count; i++) {
        if (i > 0) printf(", ");
        printf("%d", dq->data[(dq->front + i) % DEQUE_CAPACITY]);
    }
    printf("]\n");
}

int main(void) {
    Deque dq;
    deque_init(&dq);

    deque_push_back(&dq, 10);
    deque_push_back(&dq, 20);
    deque_push_front(&dq, 5);
    deque_push_front(&dq, 1);
    printf("After pushes: ");
    deque_print(&dq);                   // [1, 5, 10, 20]

    int val;
    deque_pop_front(&dq, &val);
    printf("pop_front -> %d: ", val);   // 1
    deque_print(&dq);                   // [5, 10, 20]

    deque_pop_back(&dq, &val);
    printf("pop_back -> %d: ", val);    // 20
    deque_print(&dq);                   // [5, 10]

    return 0;
}
```

---

## Traza detallada

Capacidad 6, secuencia: push_back(C), push_back(D), push_front(B),
push_front(A), pop_back, push_back(E), push_front(Z).

```
Operación        data[]               front  rear  count
────────────────────────────────────────────────────────────
init             [_, _, _, _, _, _]     0      0     0

push_back(C)     [C, _, _, _, _, _]     0      1     1
                  ↑f  ↑r

push_back(D)     [C, D, _, _, _, _]     0      2     2
                  ↑f     ↑r

push_front(B)    [C, D, _, _, _, B]     5      2     3
                                 ↑f     ↑r
                  front retrocedió: (0-1+6)%6 = 5

push_front(A)    [C, D, _, _, A, B]     4      2     4
                              ↑f        ↑r
                  front: (5-1+6)%6 = 4

pop_back → D     [C, _, _, _, A, B]     4      1     3
                              ↑f  ↑r
                  rear retrocedió: (2-1+6)%6 = 1

push_back(E)     [C, E, _, _, A, B]     4      2     4
                              ↑f     ↑r

push_front(Z)    [C, E, _, Z, A, B]     3      2     5
                           ↑f        ↑r
                  front: (4-1+6)%6 = 3

Contenido lógico (de front a rear): Z, A, B, C, E
  Verificación: pos 3=Z, 4=A, 5=B, 0=C, 1=E ✓
```

Los elementos se dispersan en el array pero el recorrido `(front + i) %
capacity` siempre los lee en orden lógico.

---

## Implementación con lista doblemente enlazada

Para pop_back en $O(1)$ con lista enlazada, necesitamos acceso al nodo anterior
al último.  Una lista **simplemente** enlazada no lo permite ($O(n)$ para
encontrar el penúltimo).  La lista **doblemente** enlazada sí:

```
         front                                    rear
           ↓                                       ↓
  NULL ← [A] ⇄ [B] ⇄ [C] ⇄ [D] → NULL
```

Cada nodo tiene punteros `prev` y `next`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

typedef struct {
    DNode *front;
    DNode *rear;
    int count;
} DequeList;

void deque_init(DequeList *dq) {
    dq->front = NULL;
    dq->rear = NULL;
    dq->count = 0;
}

static DNode *create_node(int value) {
    DNode *node = malloc(sizeof(DNode));
    if (!node) return NULL;
    node->data = value;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

bool deque_push_front(DequeList *dq, int value) {
    DNode *node = create_node(value);
    if (!node) return false;

    if (dq->front == NULL) {
        dq->front = node;
        dq->rear = node;
    } else {
        node->next = dq->front;
        dq->front->prev = node;
        dq->front = node;
    }
    dq->count++;
    return true;
}

bool deque_push_back(DequeList *dq, int value) {
    DNode *node = create_node(value);
    if (!node) return false;

    if (dq->rear == NULL) {
        dq->front = node;
        dq->rear = node;
    } else {
        node->prev = dq->rear;
        dq->rear->next = node;
        dq->rear = node;
    }
    dq->count++;
    return true;
}

bool deque_pop_front(DequeList *dq, int *out) {
    if (dq->front == NULL) return false;

    DNode *old = dq->front;
    *out = old->data;
    dq->front = old->next;

    if (dq->front == NULL) {
        dq->rear = NULL;
    } else {
        dq->front->prev = NULL;
    }

    free(old);
    dq->count--;
    return true;
}

bool deque_pop_back(DequeList *dq, int *out) {
    if (dq->rear == NULL) return false;

    DNode *old = dq->rear;
    *out = old->data;
    dq->rear = old->prev;

    if (dq->rear == NULL) {
        dq->front = NULL;
    } else {
        dq->rear->next = NULL;
    }

    free(old);
    dq->count--;
    return true;
}

void deque_destroy(DequeList *dq) {
    DNode *cur = dq->front;
    while (cur) {
        DNode *next = cur->next;
        free(cur);
        cur = next;
    }
    dq->front = NULL;
    dq->rear = NULL;
    dq->count = 0;
}
```

### Traza de punteros

push_front(A), push_back(B), push_front(Z), pop_back:

```
push_front(A):
  front → [A] ← rear
          prev=NULL, next=NULL

push_back(B):
  front → [A] ⇄ [B] ← rear
          A.next=B, B.prev=A

push_front(Z):
  front → [Z] ⇄ [A] ⇄ [B] ← rear
          Z.next=A, A.prev=Z

pop_back → B:
  front → [Z] ⇄ [A] ← rear
          A.next=NULL, free(B)
```

Cada operación modifica como máximo 3 punteros — $O(1)$.

---

## Comparación: array circular vs lista doblemente enlazada

| Criterio | Array circular | Lista doblemente enlazada |
|----------|---------------|---------------------------|
| push_front | $O(1)$ | $O(1)$ |
| push_back | $O(1)$ | $O(1)$ |
| pop_front | $O(1)$ | $O(1)$ |
| pop_back | $O(1)$ | $O(1)$ |
| Acceso por índice | $O(1)$ | $O(n)$ |
| Memoria por elemento | Solo dato | Dato + 2 punteros (16 bytes en 64-bit) |
| Localidad de caché | Excelente | Pobre |
| Capacidad | Fija / redimensionable | Ilimitada |
| malloc/free por op | 0 | 1 por push + 1 por pop |

El array circular es casi siempre preferible para un deque.  La lista
doblemente enlazada tiene sentido cuando se necesita inserción/eliminación en
posiciones **intermedias** (más allá del deque puro) — tema de C05.

---

## VecDeque en Rust

`VecDeque<T>` ya es un deque completo.  En T04 de S01 lo usamos como cola;
aquí mostramos su interfaz completa de deque:

```rust
use std::collections::VecDeque;

fn main() {
    let mut dq = VecDeque::new();

    // Insertar en ambos extremos
    dq.push_back(10);
    dq.push_back(20);
    dq.push_front(5);
    dq.push_front(1);
    println!("{dq:?}");          // [1, 5, 10, 20]

    // Extraer de ambos extremos
    println!("{:?}", dq.pop_front());   // Some(1)
    println!("{:?}", dq.pop_back());    // Some(20)
    println!("{dq:?}");                 // [5, 10]

    // Consultar extremos
    println!("front: {:?}", dq.front());  // Some(5)
    println!("back: {:?}", dq.back());    // Some(10)

    // Acceso por índice — O(1)
    println!("dq[0]: {}", dq[0]);         // 5
    println!("dq[1]: {}", dq[1]);         // 10

    // Rotaciones
    dq.push_back(15);
    dq.push_back(20);     // [5, 10, 15, 20]
    dq.rotate_left(1);    // [10, 15, 20, 5]
    println!("{dq:?}");
}
```

### Métodos exclusivos de deque

| Método | Descripción | Complejidad |
|--------|-------------|-------------|
| `push_front(val)` | Insertar al frente | $O(1)$ amort. |
| `push_back(val)` | Insertar al final | $O(1)$ amort. |
| `pop_front()` | Extraer del frente | $O(1)$ amort. |
| `pop_back()` | Extraer del final | $O(1)$ amort. |
| `front()` / `back()` | Consultar extremos | $O(1)$ |
| `rotate_left(n)` | Rotar $n$ posiciones a la izquierda | $O(\min(n, len-n))$ |
| `rotate_right(n)` | Rotar $n$ posiciones a la derecha | $O(\min(n, len-n))$ |
| `make_contiguous()` | Linearizar en memoria | $O(n)$ |
| `as_slices()` | Dos slices (puede estar partido) | $O(1)$ |

### as_slices: el buffer partido

Dado que `VecDeque` es un buffer circular, los datos pueden estar "partidos"
en dos segmentos del array:

```rust
let mut dq = VecDeque::with_capacity(4);
dq.push_back(1);
dq.push_back(2);
dq.pop_front();        // avanzar head
dq.push_back(3);
dq.push_back(4);       // wrap-around

// Internamente: [3, 4, _, 2]   head=3
//               ↑ parte 2      ↑ parte 1

let (a, b) = dq.as_slices();
println!("{a:?}");     // [2]
println!("{b:?}");     // [3, 4]

// make_contiguous reorganiza: [2, 3, 4, _]
dq.make_contiguous();
let (a, b) = dq.as_slices();
println!("{a:?}");     // [2, 3, 4]
println!("{b:?}");     // []
```

`as_slices` es útil cuando necesitas pasar los datos a una función que espera
un slice contiguo (ej. una API de red o archivo).

---

## Implementación manual de deque en Rust

Un deque con array circular, análogo a la versión C:

```rust
struct Deque<T> {
    data: Vec<Option<T>>,
    front: usize,
    count: usize,
}

impl<T> Deque<T> {
    fn new(capacity: usize) -> Self {
        assert!(capacity > 0);
        let mut data = Vec::with_capacity(capacity);
        for _ in 0..capacity {
            data.push(None);
        }
        Deque { data, front: 0, count: 0 }
    }

    fn capacity(&self) -> usize { self.data.len() }
    fn len(&self) -> usize { self.count }
    fn is_empty(&self) -> bool { self.count == 0 }
    fn is_full(&self) -> bool { self.count == self.capacity() }

    fn push_back(&mut self, value: T) -> Result<(), T> {
        if self.is_full() { return Err(value); }
        let rear = (self.front + self.count) % self.capacity();
        self.data[rear] = Some(value);
        self.count += 1;
        Ok(())
    }

    fn push_front(&mut self, value: T) -> Result<(), T> {
        if self.is_full() { return Err(value); }
        self.front = (self.front + self.capacity() - 1) % self.capacity();
        self.data[self.front] = Some(value);
        self.count += 1;
        Ok(())
    }

    fn pop_front(&mut self) -> Option<T> {
        if self.is_empty() { return None; }
        let value = self.data[self.front].take();
        self.front = (self.front + 1) % self.capacity();
        self.count -= 1;
        value
    }

    fn pop_back(&mut self) -> Option<T> {
        if self.is_empty() { return None; }
        let rear = (self.front + self.count - 1) % self.capacity();
        let value = self.data[rear].take();
        self.count -= 1;
        value
    }

    fn front(&self) -> Option<&T> {
        if self.is_empty() { return None; }
        self.data[self.front].as_ref()
    }

    fn back(&self) -> Option<&T> {
        if self.is_empty() { return None; }
        let rear = (self.front + self.count - 1) % self.capacity();
        self.data[rear].as_ref()
    }
}

fn main() {
    let mut dq = Deque::new(6);
    dq.push_back(10).unwrap();
    dq.push_back(20).unwrap();
    dq.push_front(5).unwrap();
    dq.push_front(1).unwrap();

    // Contenido lógico: 1, 5, 10, 20
    println!("front: {:?}", dq.front());       // Some(1)
    println!("back: {:?}", dq.back());         // Some(20)
    println!("pop_front: {:?}", dq.pop_front()); // Some(1)
    println!("pop_back: {:?}", dq.pop_back());   // Some(20)
    // Queda: 5, 10
}
```

### Nota sobre retroceder el índice

En la versión C usamos `(front - 1 + CAPACITY) % CAPACITY`.  En Rust, `usize`
no puede ser negativo, así que la fórmula equivalente es:

```rust
self.front = (self.front + self.capacity() - 1) % self.capacity();
```

Sumar `capacity - 1` es lo mismo que restar 1 con wrap-around, sin riesgo de
underflow de `usize`.

---

## Aplicaciones del deque

### 1. Sliding window maximum/minimum

Dado un array y una ventana de tamaño $k$, encontrar el máximo (o mínimo) de
cada posición de la ventana al deslizarla por el array.  Se resuelve en $O(n)$
con un deque que mantiene índices en orden decreciente de valor:

```
Array: [1, 3, -1, -3, 5, 3, 6, 7]    k=3

Ventana [1,3,-1]: max=3
Ventana [3,-1,-3]: max=3
Ventana [-1,-3,5]: max=5
...
```

Sin deque: $O(nk)$.  Con deque: $O(n)$ — cada elemento entra y sale del deque
exactamente una vez.

```rust
use std::collections::VecDeque;

fn sliding_max(arr: &[i32], k: usize) -> Vec<i32> {
    let mut result = Vec::new();
    let mut dq: VecDeque<usize> = VecDeque::new();  // índices

    for i in 0..arr.len() {
        // Eliminar índices fuera de la ventana
        while let Some(&front) = dq.front() {
            if front + k <= i { dq.pop_front(); }
            else { break; }
        }

        // Eliminar por atrás los menores que arr[i]
        while let Some(&back) = dq.back() {
            if arr[back] <= arr[i] { dq.pop_back(); }
            else { break; }
        }

        dq.push_back(i);

        if i >= k - 1 {
            result.push(arr[*dq.front().unwrap()]);
        }
    }
    result
}

fn main() {
    let arr = [1, 3, -1, -3, 5, 3, 6, 7];
    let maxes = sliding_max(&arr, 3);
    println!("{maxes:?}");   // [3, 3, 5, 5, 6, 7]
}
```

### 2. Palíndromo checker

Comparar caracteres desde ambos extremos simultáneamente:

```rust
fn is_palindrome(s: &str) -> bool {
    let mut dq: VecDeque<char> = s.chars()
        .filter(|c| c.is_alphanumeric())
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();

    while dq.len() > 1 {
        if dq.pop_front() != dq.pop_back() {
            return false;
        }
    }
    true
}
```

### 3. Work stealing (planificación de tareas)

En sistemas multi-hilo, cada worker tiene un deque de tareas.  El worker toma
tareas de su propio extremo (`pop_back`), pero cuando se queda sin trabajo,
**roba** del extremo opuesto (`pop_front`) del deque de otro worker.  Esto
minimiza la contención porque cada thread opera en un extremo diferente.

```
Worker 1:  [tarea_A, tarea_B, tarea_C]  ← pop_back (propias)
Worker 2:  [tarea_D]                    ← se quedó sin trabajo
Worker 2 roba de Worker 1:              → pop_front(Worker1) = tarea_A
```

### 4. Buffer de historial con límite

Un deque con capacidad máxima: al añadir un elemento cuando está lleno, se
descarta el más antiguo (del frente):

```rust
struct BoundedHistory<T> {
    inner: VecDeque<T>,
    max: usize,
}

impl<T> BoundedHistory<T> {
    fn new(max: usize) -> Self {
        BoundedHistory { inner: VecDeque::with_capacity(max), max }
    }

    fn add(&mut self, item: T) {
        if self.inner.len() == self.max {
            self.inner.pop_front();   // descartar el más antiguo
        }
        self.inner.push_back(item);
    }

    fn recent(&self) -> impl Iterator<Item = &T> {
        self.inner.iter().rev()   // del más reciente al más antiguo
    }
}
```

---

## Deque vs Vec vs VecDeque: cuándo usar cada uno

| Operación | `Vec<T>` | `VecDeque<T>` |
|-----------|----------|---------------|
| push_back | $O(1)$ amort. | $O(1)$ amort. |
| pop_back | $O(1)$ | $O(1)$ |
| push_front | $O(n)$ ✗ | $O(1)$ amort. ✓ |
| pop_front | $O(n)$ ✗ | $O(1)$ ✓ |
| Acceso `[i]` | $O(1)$ | $O(1)$ |
| Slicing `&[..]` | Directo (`&vec[..]`) | `make_contiguous()` necesario |
| Caché | Siempre contiguo | Puede estar partido |

**Regla**: si solo necesitas push/pop en un extremo, usa `Vec`.  Si necesitas
ambos extremos, usa `VecDeque`.  El costo de `VecDeque` es que los datos no
siempre están contiguos en memoria (lo cual importa para interop con APIs que
esperan slices).

---

## Ejercicios

### Ejercicio 1 — Traza completa

Traza un deque con array circular de capacidad 5 para:

```
push_back(A), push_back(B), push_front(Z), pop_back,
push_front(Y), push_back(C), pop_front, pop_front, pop_back
```

Muestra `front`, `rear` (calculado), `count` y el array después de cada
operación.

<details>
<summary>Predice el estado final</summary>

Después de todas las operaciones, quedan solo los elementos que no fueron
extraídos.  Secuencia de push: Z, A, B (pop B), Y, C.  Secuencia de pop:
B(back), Y(front), A(front), C(back).  Queda: Z.  El front apuntará a la
posición de Z en el array.
</details>

### Ejercicio 2 — Deque como pila y cola

Usando solo un `VecDeque<i32>` en Rust, demuestra que puede funcionar como
pila y como cola:
- Usa como pila: push_back + pop_back para los datos [1, 2, 3].
- Usa como cola: push_back + pop_front para los datos [4, 5, 6].
Imprime el orden de salida en cada caso.

<details>
<summary>Resultado esperado</summary>

Pila (LIFO): 3, 2, 1.  Cola (FIFO): 4, 5, 6.  Mismo deque, diferentes
combinaciones de operaciones.
</details>

### Ejercicio 3 — Lista doblemente enlazada completa

Completa la implementación de `DequeList` con las funciones `front`, `back`,
`is_empty`, `size`, `destroy` y `print`.  Verifica con la misma secuencia del
ejercicio 1.

<details>
<summary>Pista para print</summary>

```c
void deque_print(const DequeList *dq) {
    printf("[");
    for (DNode *n = dq->front; n != NULL; n = n->next) {
        printf("%d", n->data);
        if (n->next) printf(", ");
    }
    printf("]\n");
}
```

Para imprimir en reversa (del rear al front): recorrer con `prev`.
</details>

### Ejercicio 4 — Sliding window minimum

Implementa `sliding_min` análogo al `sliding_max` del tópico.  Solo cambia la
condición del pop_back.  Prueba con `[5, 3, 4, 2, 8, 1, 7]` y $k = 3$.

<details>
<summary>Resultado esperado</summary>

Ventanas y mínimos: [5,3,4]→3, [3,4,2]→2, [4,2,8]→2, [2,8,1]→1, [8,1,7]→1.
Resultado: `[3, 2, 2, 1, 1]`.

Cambio vs max: `arr[back] <= arr[i]` se convierte en `arr[back] >= arr[i]`.
El deque mantiene índices en orden **creciente** de valor.
</details>

### Ejercicio 5 — Palíndromo con deque en C

Implementa el verificador de palíndromos usando el deque de array circular en
C.  Inserta cada carácter del string, luego compara pop_front vs pop_back
hasta que quede 0 o 1 elemento.

<details>
<summary>Estructura</summary>

```c
bool is_palindrome(const char *s) {
    Deque dq;
    deque_init(&dq);
    for (int i = 0; s[i]; i++) {
        if (isalpha(s[i])) {
            deque_push_back(&dq, tolower(s[i]));
        }
    }
    while (deque_size(&dq) > 1) {
        int front, back;
        deque_pop_front(&dq, &front);
        deque_pop_back(&dq, &back);
        if (front != back) return false;
    }
    return true;
}
```
</details>

### Ejercicio 6 — Bounded history

Implementa `BoundedHistory` en C con un deque de capacidad fija.  Cuando el
historial está lleno, `add` descarta el elemento más antiguo (pop_front) antes
de insertar.  Prueba con capacidad 5 y 10 inserciones.

<details>
<summary>Predicción</summary>

Después de insertar 1..10 con capacidad 5, el historial contiene solo [6, 7,
8, 9, 10] — los 5 más recientes.  Los elementos 1-5 fueron descartados uno por
uno a medida que se insertaban los nuevos.
</details>

### Ejercicio 7 — Redimensionamiento del deque

Extiende el `Deque<T>` manual en Rust para que se redimensione automáticamente
(duplicando capacidad) cuando esté lleno.  Debe linearizar los datos al
redimensionar, igual que en la cola circular (T02).

<details>
<summary>Pista</summary>

```rust
fn grow(&mut self) {
    let new_cap = self.capacity() * 2;
    let mut new_data = Vec::with_capacity(new_cap);
    for i in 0..self.count {
        let phys = (self.front + i) % self.capacity();
        new_data.push(self.data[phys].take());
    }
    for _ in self.count..new_cap {
        new_data.push(None);
    }
    self.data = new_data;
    self.front = 0;
}
```

Llamar `grow` al inicio de `push_front` y `push_back` si `is_full()`.
</details>

### Ejercicio 8 — Deque genérico en C

Implementa un deque genérico usando `void*` y `memcpy` con array circular.
Prueba con `int`, `double` y una struct `Point { int x, y; }`.

<details>
<summary>Firma sugerida</summary>

```c
typedef struct {
    char *data;
    size_t elem_size;
    size_t capacity;
    size_t front;
    size_t count;
} GenericDeque;

GenericDeque *gdeque_create(size_t capacity, size_t elem_size);
bool gdeque_push_front(GenericDeque *dq, const void *elem);
bool gdeque_push_back(GenericDeque *dq, const void *elem);
bool gdeque_pop_front(GenericDeque *dq, void *out);
bool gdeque_pop_back(GenericDeque *dq, void *out);
```

Acceso al slot $i$: `dq->data + ((dq->front + i) % dq->capacity) * dq->elem_size`.
</details>

### Ejercicio 9 — Simular cola con dos pilas vs VecDeque

Compara rendimiento de 3 implementaciones de cola para $10^5$ operaciones
intercaladas:
- `VecDeque` (push_back + pop_front)
- Dos `Vec` (inbox/outbox, como en T01)
- `Vec` directo (push + remove(0))

<details>
<summary>Predicción de ranking</summary>

1. `VecDeque`: más rápido — $O(1)$ por operación, excelente caché.
2. Dos `Vec`: segundo — $O(1)$ amortizado, pero el volcado periódico genera
   spikes.
3. `Vec` con `remove(0)`: mucho más lento — $O(n)$ por dequeue, total
   $O(n^2)$.

Para $10^5$ operaciones: VecDeque ≈ 0.1ms, dos Vec ≈ 0.5ms, Vec directo ≈
500ms.
</details>

### Ejercicio 10 — Work stealing simplificado

Simula 3 workers, cada uno con un `VecDeque<u32>` de tareas.  Worker 0 empieza
con 30 tareas (1..30).  Los otros empiezan vacíos.  En cada paso:
- Cada worker procesa una tarea de su cola (pop_back — su propia tarea).
- Si un worker no tiene tareas, roba del frente (pop_front) del worker con
  más tareas.

Ejecuta hasta que todas las tareas estén procesadas.  Imprime qué tareas
procesó cada worker.

<details>
<summary>Observación</summary>

El work stealing es un patrón real usado en Tokio (Rust async runtime),
Rayon (parallel iterators), y el fork-join framework de Java.  El deque es la
estructura ideal porque el dueño y el ladrón operan en extremos opuestos,
minimizando contención.  En producción se usa un deque lock-free — aquí basta
con `VecDeque` secuencial.
</details>
