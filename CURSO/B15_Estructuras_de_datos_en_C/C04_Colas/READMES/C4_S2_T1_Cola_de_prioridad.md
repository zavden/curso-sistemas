# Cola de prioridad

## Concepto

Una **cola de prioridad** (*priority queue*) es una colección donde cada
elemento tiene un **valor de prioridad** asociado.  La extracción no sigue el
orden de llegada (FIFO) sino el orden de **prioridad**: siempre sale primero el
elemento con mayor (o menor) prioridad.

```
Cola FIFO:          sale el primero que llegó
Cola de prioridad:  sale el de mayor prioridad, sin importar cuándo llegó
```

### Analogía

Sala de emergencias: los pacientes no se atienden por orden de llegada sino por
gravedad.  Un paciente con infarto se atiende antes que uno con dolor de
cabeza, aunque haya llegado después.

### Convención min vs max

| Tipo | Extrae primero | Ejemplo |
|------|---------------|---------|
| **Min-priority queue** | El de **menor** valor de prioridad | Dijkstra, planificación de tareas por deadline |
| **Max-priority queue** | El de **mayor** valor de prioridad | Planificación por importancia |

La diferencia es solo de convención — negar las prioridades convierte una en la
otra.  En este tópico usamos **min-priority** (el menor valor = mayor urgencia),
que es la convención más común en algoritmos.

---

## Definición del TAD

### Operaciones esenciales

| Operación | Descripción | Complejidad ideal |
|-----------|-------------|-------------------|
| `insert(pq, x, pri)` | Insertar elemento con prioridad | $O(\log n)$ |
| `extract_min(pq)` | Extraer el de menor prioridad | $O(\log n)$ |
| `peek_min(pq)` | Consultar el mínimo sin extraer | $O(1)$ |
| `is_empty(pq)` | ¿Está vacía? | $O(1)$ |
| `size(pq)` | Número de elementos | $O(1)$ |

La complejidad ideal es $O(\log n)$ para insert y extract — se alcanza con un
**heap** (montículo), que veremos en C07.  En este tópico implementamos
versiones **ingenuas** con array y lista para entender el concepto y motivar
el heap.

### Diferencia clave con la cola FIFO

| Aspecto | Cola FIFO | Cola de prioridad |
|---------|----------|-------------------|
| Criterio de salida | Orden de llegada | Prioridad |
| Insert | $O(1)$ | $O(1)$ a $O(n)$ según implementación |
| Extract | $O(1)$ | $O(1)$ a $O(n)$ según implementación |
| Estructura interna | Array circular o lista | Array, lista, o heap |
| Ejemplo | Impresora | Planificador del SO |

---

## Implementaciones ingenuas

Existen dos estrategias ingenuas, ambas con un tradeoff:

| Estrategia | Insert | Extract min | Idea |
|-----------|--------|------------|------|
| **Array desordenado** | $O(1)$ — añadir al final | $O(n)$ — buscar el mínimo | Insert rápido, extract lento |
| **Array ordenado** | $O(n)$ — insertar en posición | $O(1)$ — el mínimo está al final/inicio | Insert lento, extract rápido |

Ninguna logra $O(\log n)$ en ambas.  El heap lo consigue, pero es tema de C07.

---

## Implementación 1: array desordenado en C

Los elementos se añaden al final sin importar su prioridad.  Para extraer el
mínimo, se recorre todo el array buscándolo.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

typedef struct {
    int value;
    int priority;    /* menor valor = mayor urgencia */
} PQItem;

#define PQ_CAPACITY 100

typedef struct {
    PQItem items[PQ_CAPACITY];
    int count;
} PriorityQueue;

void pq_init(PriorityQueue *pq) {
    pq->count = 0;
}

bool pq_is_empty(const PriorityQueue *pq) {
    return pq->count == 0;
}

/* Insert: O(1) — añadir al final */
bool pq_insert(PriorityQueue *pq, int value, int priority) {
    if (pq->count == PQ_CAPACITY) return false;
    pq->items[pq->count] = (PQItem){ .value = value, .priority = priority };
    pq->count++;
    return true;
}

/* Extract min: O(n) — buscar mínimo, swap con último, decrementar */
bool pq_extract_min(PriorityQueue *pq, PQItem *out) {
    if (pq->count == 0) return false;

    int min_idx = 0;
    for (int i = 1; i < pq->count; i++) {
        if (pq->items[i].priority < pq->items[min_idx].priority) {
            min_idx = i;
        }
    }

    *out = pq->items[min_idx];

    /* Reemplazar con el último elemento (O(1)) */
    pq->items[min_idx] = pq->items[pq->count - 1];
    pq->count--;
    return true;
}

/* Peek min: O(n) — misma búsqueda sin extraer */
bool pq_peek_min(const PriorityQueue *pq, PQItem *out) {
    if (pq->count == 0) return false;

    int min_idx = 0;
    for (int i = 1; i < pq->count; i++) {
        if (pq->items[i].priority < pq->items[min_idx].priority) {
            min_idx = i;
        }
    }
    *out = pq->items[min_idx];
    return true;
}
```

### Programa de prueba

```c
int main(void) {
    PriorityQueue pq;
    pq_init(&pq);

    /* Pacientes: (id, gravedad) — menor = más urgente */
    pq_insert(&pq, 101, 5);    /* dolor de cabeza */
    pq_insert(&pq, 102, 1);    /* infarto */
    pq_insert(&pq, 103, 3);    /* fractura */
    pq_insert(&pq, 104, 2);    /* hemorragia */
    pq_insert(&pq, 105, 4);    /* esguince */

    printf("Orden de atención:\n");
    PQItem item;
    while (pq_extract_min(&pq, &item)) {
        printf("  Paciente %d (prioridad %d)\n", item.value, item.priority);
    }
    return 0;
}
```

Salida:

```
Orden de atención:
  Paciente 102 (prioridad 1)    ← infarto primero
  Paciente 104 (prioridad 2)    ← hemorragia
  Paciente 103 (prioridad 3)    ← fractura
  Paciente 105 (prioridad 4)    ← esguince
  Paciente 101 (prioridad 5)    ← dolor de cabeza último
```

### Traza del extract con swap

```
Estado inicial:  [(101,5), (102,1), (103,3), (104,2), (105,4)]
                                 ↑ min_idx=1

extract_min:  out = (102,1)
              swap con último:  [(101,5), (105,4), (103,3), (104,2)]
              count: 5 → 4

Siguiente extract:  buscar en [(101,5), (105,4), (103,3), (104,2)]
                                                           ↑ min_idx=3
              out = (104,2)
              swap con último:  [(101,5), (105,4), (103,3)]
              count: 4 → 3
```

El truco del swap con el último evita desplazar elementos — el extract es
$O(n)$ por la búsqueda, pero la eliminación propiamente dicha es $O(1)$.

---

## Implementación 2: array ordenado en C

Los elementos se mantienen ordenados por prioridad.  Insert busca la posición
correcta y desplaza.  Extract simplemente toma el primero (o último).

Usamos orden descendente: el mínimo al final (para que extract sea `count--`
sin desplazar):

```c
typedef struct {
    PQItem items[PQ_CAPACITY];
    int count;
} PriorityQueueSorted;

void pqs_init(PriorityQueueSorted *pq) {
    pq->count = 0;
}

/* Insert: O(n) — inserción ordenada (mayor primero, menor al final) */
bool pqs_insert(PriorityQueueSorted *pq, int value, int priority) {
    if (pq->count == PQ_CAPACITY) return false;

    /* Encontrar posición: buscar desde el final hacia atrás */
    int i = pq->count - 1;
    while (i >= 0 && pq->items[i].priority < priority) {
        pq->items[i + 1] = pq->items[i];   /* desplazar */
        i--;
    }
    pq->items[i + 1] = (PQItem){ .value = value, .priority = priority };
    pq->count++;
    return true;
}

/* Extract min: O(1) — el mínimo está al final */
bool pqs_extract_min(PriorityQueueSorted *pq, PQItem *out) {
    if (pq->count == 0) return false;
    pq->count--;
    *out = pq->items[pq->count];
    return true;
}

/* Peek min: O(1) */
bool pqs_peek_min(const PriorityQueueSorted *pq, PQItem *out) {
    if (pq->count == 0) return false;
    *out = pq->items[pq->count - 1];
    return true;
}
```

### Traza de inserción ordenada

Insertar (104, 2) en array con [(101, 5), (103, 3)]:

```
Array actual (mayor prioridad al inicio, menor al final):
  [(101,5), (103,3)]

Insertar (104,2):  prioridad 2
  i=1: items[1].priority=3 > 2 → no desplazar
  i=1: insertar en posición 2

Array final:  [(101,5), (103,3), (104,2)]
                                    ↑ menor prioridad al final
                                      extract toma este
```

Insertar (102, 1):

```
Array:  [(101,5), (103,3), (104,2)]

Insertar (102,1):  prioridad 1
  i=2: items[2].priority=2 > 1 → no desplazar
  i=2: insertar en posición 3

Array:  [(101,5), (103,3), (104,2), (102,1)]
                                       ↑ extract toma este primero
```

---

## Implementación en Rust: array desordenado

```rust
#[derive(Debug, Clone)]
struct PQItem<T> {
    value: T,
    priority: i32,
}

struct PriorityQueue<T> {
    items: Vec<PQItem<T>>,
}

impl<T> PriorityQueue<T> {
    fn new() -> Self {
        PriorityQueue { items: Vec::new() }
    }

    fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    fn len(&self) -> usize {
        self.items.len()
    }

    // Insert: O(1)
    fn insert(&mut self, value: T, priority: i32) {
        self.items.push(PQItem { value, priority });
    }

    // Extract min: O(n)
    fn extract_min(&mut self) -> Option<T> {
        if self.items.is_empty() {
            return None;
        }

        let min_idx = self.items.iter()
            .enumerate()
            .min_by_key(|(_, item)| item.priority)
            .map(|(idx, _)| idx)
            .unwrap();

        // swap_remove: O(1) — intercambia con último y elimina
        Some(self.items.swap_remove(min_idx).value)
    }

    // Peek min: O(n)
    fn peek_min(&self) -> Option<&T> {
        self.items.iter()
            .min_by_key(|item| item.priority)
            .map(|item| &item.value)
    }
}

fn main() {
    let mut pq = PriorityQueue::new();

    pq.insert("headache", 5);
    pq.insert("heart_attack", 1);
    pq.insert("fracture", 3);
    pq.insert("hemorrhage", 2);

    while let Some(patient) = pq.extract_min() {
        println!("Attending: {patient}");
    }
}
```

Nota: `Vec::swap_remove(idx)` es el equivalente exacto del truco de C (swap
con último + decrementar count).  Rust lo tiene como método de `Vec`.

---

## Implementación en Rust: con Ord

Para tipos que implementan `Ord`, podemos prescindir del campo `priority`
separado y usar el orden natural del tipo:

```rust
struct MinQueue<T: Ord> {
    items: Vec<T>,
}

impl<T: Ord> MinQueue<T> {
    fn new() -> Self {
        MinQueue { items: Vec::new() }
    }

    fn insert(&mut self, value: T) {
        self.items.push(value);
    }

    fn extract_min(&mut self) -> Option<T> {
        if self.items.is_empty() {
            return None;
        }
        let min_idx = self.items.iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| a.cmp(b))
            .map(|(idx, _)| idx)
            .unwrap();
        Some(self.items.swap_remove(min_idx))
    }

    fn peek_min(&self) -> Option<&T> {
        self.items.iter().min()
    }
}
```

Para tipos personalizados, implementar `Ord` define el orden:

```rust
#[derive(Debug, Eq, PartialEq)]
struct Patient {
    name: String,
    severity: u32,    // menor = más urgente
}

impl Ord for Patient {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.severity.cmp(&other.severity)   // ordenar por severidad
    }
}

impl PartialOrd for Patient {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
```

---

## BinaryHeap de la stdlib

Rust incluye `std::collections::BinaryHeap<T>`, que es una cola de prioridad
eficiente implementada con un heap binario (tema de C07).  Es **max-priority**
por defecto:

```rust
use std::collections::BinaryHeap;

fn main() {
    let mut heap = BinaryHeap::new();
    heap.push(3);
    heap.push(1);
    heap.push(4);
    heap.push(1);
    heap.push(5);

    // Extrae el MAYOR primero (max-heap)
    while let Some(val) = heap.pop() {
        print!("{val} ");   // 5 4 3 1 1
    }
    println!();
}
```

### Convertir a min-priority

Dos técnicas:

**1. Negar las prioridades (solo para números):**

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

let mut min_heap = BinaryHeap::new();
min_heap.push(Reverse(3));
min_heap.push(Reverse(1));
min_heap.push(Reverse(5));

// Extrae el menor primero
while let Some(Reverse(val)) = min_heap.pop() {
    print!("{val} ");   // 1 3 5
}
```

`std::cmp::Reverse` es un wrapper que invierte el orden de `Ord`.

**2. Implementar Ord invertido para tipos personalizados:**

```rust
use std::collections::BinaryHeap;

#[derive(Debug, Eq, PartialEq)]
struct Task {
    name: String,
    priority: u32,   // menor = más urgente
}

impl Ord for Task {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        other.priority.cmp(&self.priority)   // invertido: menor primero
    }
}

impl PartialOrd for Task {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

fn main() {
    let mut pq = BinaryHeap::new();
    pq.push(Task { name: "backup".into(), priority: 5 });
    pq.push(Task { name: "deploy".into(), priority: 1 });
    pq.push(Task { name: "test".into(), priority: 3 });

    while let Some(task) = pq.pop() {
        println!("{}: priority {}", task.name, task.priority);
    }
    // deploy (1), test (3), backup (5)
}
```

### Complejidad de BinaryHeap

| Operación | Complejidad |
|-----------|-------------|
| `push` | $O(\log n)$ |
| `pop` | $O(\log n)$ |
| `peek` | $O(1)$ |
| `from(vec)` | $O(n)$ — heapify |

Veremos la implementación interna en C07.  Por ahora, basta saber que el heap
logra lo que nuestras implementaciones ingenuas no pueden: $O(\log n)$ para
ambas operaciones.

---

## Comparación de implementaciones

| Implementación | Insert | Extract min | Peek min | Espacio |
|---------------|--------|-------------|----------|---------|
| Array desordenado | $O(1)$ | $O(n)$ | $O(n)$ | $O(n)$ |
| Array ordenado | $O(n)$ | $O(1)$ | $O(1)$ | $O(n)$ |
| Lista ordenada (T03) | $O(n)$ | $O(1)$ | $O(1)$ | $O(n)$ |
| **Heap binario** (C07) | $O(\log n)$ | $O(\log n)$ | $O(1)$ | $O(n)$ |

### Cuándo es aceptable cada una

| Implementación | Aceptable cuando |
|---------------|-----------------|
| Array desordenado | Muchos inserts, pocos extracts (batch: insertar todo, luego extraer todo) |
| Array ordenado | Pocos inserts, muchos peeks al mínimo |
| Lista ordenada | Pocos inserts, muchos extracts |
| Heap | Caso general — siempre preferible para $n$ grande |

Para $n$ pequeño (< 20-30 elementos), la diferencia entre $O(n)$ y $O(\log n)$
es insignificante.  Para $n$ grande, el heap es obligatorio.

---

## Estabilidad: prioridades iguales

¿Qué ocurre cuando dos elementos tienen la misma prioridad?  Depende de la
implementación:

| Implementación | Comportamiento con prioridades iguales |
|---------------|----------------------------------------|
| Array desordenado | El primero encontrado por la búsqueda (no determinista si se usa swap_remove) |
| Array ordenado | FIFO si se usa inserción estable |
| Heap | No FIFO — el heap no preserva orden de inserción |

Para garantizar FIFO entre iguales, se añade un **timestamp** (número de
secuencia) como desempate:

```c
typedef struct {
    int value;
    int priority;
    int sequence;    /* incrementado en cada insert */
} PQItem;

/* Comparar: primero por priority, luego por sequence */
bool has_higher_priority(PQItem a, PQItem b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    return a.sequence < b.sequence;   /* FIFO como desempate */
}
```

```rust
use std::cmp::Reverse;
use std::collections::BinaryHeap;

#[derive(Eq, PartialEq)]
struct StableItem<T: Eq> {
    priority: i32,
    sequence: u64,     // desempate FIFO
    value: T,
}

impl<T: Eq> Ord for StableItem<T> {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        other.priority.cmp(&self.priority)          // min-priority
            .then(other.sequence.cmp(&self.sequence)) // FIFO
    }
}

impl<T: Eq> PartialOrd for StableItem<T> {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
```

---

## Aplicaciones de colas de prioridad

| Aplicación | Prioridad | Tipo |
|-----------|-----------|------|
| **Dijkstra** (camino más corto) | Distancia acumulada | Min |
| **Planificador del SO** | Prioridad del proceso | Depende del algoritmo |
| **Huffman coding** | Frecuencia del símbolo | Min |
| **A\*** (pathfinding) | $f(n) = g(n) + h(n)$ | Min |
| **Merge de k listas ordenadas** | Valor del elemento | Min |
| **Simulación de eventos** | Tiempo del evento | Min |
| **Mediana en stream** | Valor del elemento | Min + Max (dos heaps) |

Muchos de estos algoritmos se estudian en bloques posteriores.  La cola de
prioridad es una estructura **ubicua** en ciencias de la computación — tan
importante como la pila y la cola FIFO.

---

## Preview: el heap binario

En C07 implementaremos el heap binario, que es simplemente un **array** con una
interpretación de árbol binario:

```
Array:  [1, 3, 5, 7, 4, 8, 6]

Árbol:
         1
       /   \
      3     5
     / \   / \
    7   4 8   6

Padre de i:      (i - 1) / 2
Hijo izq de i:   2*i + 1
Hijo der de i:   2*i + 2
```

La propiedad del min-heap: cada padre es $\leq$ que sus hijos.  Insert y extract
mantienen esta propiedad con operaciones **sift-up** y **sift-down** que
recorren como máximo $O(\log n)$ niveles del árbol.

No necesita punteros, no necesita malloc por operación, tiene excelente
localidad de caché.  Es la implementación definitiva de la cola de prioridad.

---

## Ejercicios

### Ejercicio 1 — Traza de extract en array desordenado

Dado el array `[(A,3), (B,1), (C,4), (D,1), (E,5), (F,2)]`, traza 3
operaciones `extract_min`.  Muestra el estado del array después de cada
extracción (con el swap_remove).

<details>
<summary>Predice el orden de extracción</summary>

Primer extract: mínimo es (B,1) en idx=1.  Swap con último (F,2): array
`[(A,3), (F,2), (C,4), (D,1), (E,5)]`.  Segundo: mínimo es (D,1) en idx=3.
Swap con último (E,5): `[(A,3), (F,2), (C,4), (E,5)]`.  Tercero: mínimo es
(F,2) en idx=1.  Swap con último (E,5): `[(A,3), (E,5), (C,4)]`.
Orden: B, D, F (prioridades 1, 1, 2).  Notar que B y D tienen la misma
prioridad — el orden entre ellos depende de quién está primero en el array.
</details>

### Ejercicio 2 — Traza de insert en array ordenado

Partiendo de un array ordenado vacío, inserta los elementos `(A,3), (B,1),
(C,4), (D,1), (E,5), (F,2)` uno por uno.  Muestra el estado del array después
de cada inserción y cuántos desplazamientos requirió cada una.

<details>
<summary>Predicción</summary>

Insert (A,3): `[(A,3)]` — 0 desplazamientos.
Insert (B,1): `[(A,3), (B,1)]` — 0 (B va al final, prioridad menor).
Insert (C,4): `[(C,4), (A,3), (B,1)]` — 2 desplazamientos (C tiene mayor
prioridad numérica, va al inicio con nuestro orden descendente).
Etc.  Recordar: el array está en orden descendente (mayor prioridad numérica
al inicio, menor al final para que extract sea $O(1)$).
</details>

### Ejercicio 3 — Max-priority queue

Modifica la implementación de array desordenado en C para que sea
**max-priority** (extraer el de mayor prioridad primero).  ¿Cuántas líneas
cambian?

<details>
<summary>Respuesta</summary>

Solo cambia una línea: en `extract_min`, el `<` se convierte en `>`:

```c
if (pq->items[i].priority > pq->items[max_idx].priority)
```

Y se renombran las funciones (`extract_max`, `peek_max`).  La estructura y el
algoritmo son idénticos — solo cambia la dirección de la comparación.
</details>

### Ejercicio 4 — Benchmark insert-heavy vs extract-heavy

Crea un benchmark con $n = 10000$:
- **Escenario A**: 10000 inserts + 10000 extracts.
- **Escenario B**: 10000 inserts intercalados con extracts (insert, insert,
  extract, insert, insert, extract, ...).

Mide tiempos para ambas implementaciones (desordenado y ordenado).

<details>
<summary>Predicción</summary>

**Escenario A**: desordenado = $10000 \times O(1) + 10000 \times O(n) \approx
O(n^2)$.  Ordenado = $10000 \times O(n) + 10000 \times O(1) \approx O(n^2)$.
Similar.

**Escenario B**: desordenado es mejor — la cola nunca crece mucho (a lo sumo
~6666), así que los extracts buscan en un array más pequeño.  Ordenado paga
$O(n)$ en cada insert.

Para ambos: $O(n^2)$ con $n = 10000$ toma milisegundos — es rápido.  Para $n =
10^6$ la diferencia vs heap ($O(n \log n)$) sería brutal.
</details>

### Ejercicio 5 — Cola de prioridad estable

Implementa una cola de prioridad estable (FIFO entre iguales) en C, añadiendo
un campo `sequence` al struct.  Verifica que para prioridades iguales, los
elementos salen en orden de inserción.

<details>
<summary>Pista</summary>

```c
static int next_sequence = 0;

bool pq_insert(PriorityQueue *pq, int value, int priority) {
    /* ... */
    pq->items[pq->count].sequence = next_sequence++;
    /* ... */
}

/* En la búsqueda del mínimo: */
if (pq->items[i].priority < min_pri ||
    (pq->items[i].priority == min_pri &&
     pq->items[i].sequence < min_seq)) {
    min_idx = i;
}
```
</details>

### Ejercicio 6 — BinaryHeap con Reverse

Usa `BinaryHeap<Reverse<i32>>` para implementar una min-priority queue en
Rust.  Inserta los números [5, 2, 8, 1, 9, 3] y extráelos todos en orden
ascendente.

<details>
<summary>Código</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn main() {
    let mut min_heap = BinaryHeap::new();
    for &x in &[5, 2, 8, 1, 9, 3] {
        min_heap.push(Reverse(x));
    }
    while let Some(Reverse(val)) = min_heap.pop() {
        print!("{val} ");   // 1 2 3 5 8 9
    }
}
```
</details>

### Ejercicio 7 — Planificador de tareas

Implementa un planificador simple: un array de tareas con nombre y deadline
(día del año).  Usa una cola de prioridad (min, por deadline) para procesarlas
en orden de urgencia.  Implementa en C o Rust.

<details>
<summary>Ejemplo</summary>

```
Tareas:
  "deploy"    deadline=5
  "backup"    deadline=2
  "test"      deadline=7
  "review"    deadline=3

Procesamiento:
  1. backup  (deadline 2)
  2. review  (deadline 3)
  3. deploy  (deadline 5)
  4. test    (deadline 7)
```
</details>

### Ejercicio 8 — Merge de k arrays ordenados

Dados $k$ arrays ya ordenados, produce un único array ordenado.  Usa una cola
de prioridad que almacene `(valor, array_idx, pos_en_array)`.  Implementa en
Rust con `BinaryHeap<Reverse<...>>`.

<details>
<summary>Algoritmo</summary>

1. Insertar el primer elemento de cada array en la PQ.
2. Extraer el mínimo → es el siguiente del resultado.
3. Si el array del que vino tiene más elementos, insertar el siguiente.
4. Repetir hasta vaciar la PQ.

Complejidad: $O(N \log k)$ donde $N$ es el total de elementos.  Con $k$
arrays de tamaño $m$: $O(km \log k)$ — mucho mejor que concatenar y
reordenar ($O(km \log km)$).
</details>

### Ejercicio 9 — Comparar O(n) vs O(log n)

Implementa la cola de prioridad con array desordenado y con `BinaryHeap`.
Mide el tiempo de $10^5$ inserts seguidos de $10^5$ extracts para ambas.
Calcula el speedup.

<details>
<summary>Predicción</summary>

Array desordenado: inserts $O(1) \times 10^5$ + extracts $O(n) \times 10^5$ =
$\approx 10^{10}/2$ comparaciones.

BinaryHeap: inserts $O(\log n) \times 10^5$ + extracts $O(\log n) \times 10^5$
= $\approx 10^5 \times 17 \times 2 = 3.4 \times 10^6$ operaciones.

Speedup teórico: $\approx 1500\times$.  En la práctica será menor por
constantes, pero el heap debería ser 100-1000× más rápido.
</details>

### Ejercicio 10 — Top-K elementos

Dado un stream de $n$ números, mantener los $k$ menores vistos hasta ahora.
Usa una **max-priority queue** de tamaño $k$: si el nuevo número es menor que
el máximo de la PQ, reemplazar el máximo.

<details>
<summary>Análisis</summary>

```rust
use std::collections::BinaryHeap;

fn top_k_smallest(stream: &[i32], k: usize) -> Vec<i32> {
    let mut max_heap = BinaryHeap::new();
    for &val in stream {
        if max_heap.len() < k {
            max_heap.push(val);
        } else if val < *max_heap.peek().unwrap() {
            max_heap.pop();
            max_heap.push(val);
        }
    }
    max_heap.into_sorted_vec()
}
```

Complejidad: $O(n \log k)$ — mucho mejor que ordenar todo ($O(n \log n)$)
cuando $k \ll n$.  Espacio: $O(k)$.  Es un max-heap porque necesitamos
comparar cada nuevo elemento con el **mayor** de los $k$ menores.
</details>
