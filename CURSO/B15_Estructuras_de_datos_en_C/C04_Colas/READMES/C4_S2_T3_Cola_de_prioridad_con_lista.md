# Cola de prioridad con lista enlazada

## Motivación

En T01 implementamos la cola de prioridad con arrays — desordenado (insert
$O(1)$, extract $O(n)$) y ordenado (insert $O(n)$, extract $O(1)$).  Ambos
tienen el mismo tradeoff: una operación es rápida y la otra lenta.

La lista enlazada ordenada ofrece el mismo tradeoff que el array ordenado
(insert $O(n)$, extract $O(1)$), pero con una ventaja: la **inserción no
requiere desplazar** elementos.  En un array, insertar en la posición correcta
desplaza todos los posteriores una posición.  En una lista, basta con
redirigir punteros.

| Implementación | Insert | Extract min | Insert: operación costosa |
|---------------|--------|-------------|--------------------------|
| Array desordenado | $O(1)$ | $O(n)$ buscar | — |
| Array ordenado | $O(n)$ desplazar | $O(1)$ | Mover datos en memoria |
| **Lista ordenada** | $O(n)$ recorrer | $O(1)$ | Solo recorrer (sin mover datos) |
| Heap (C07) | $O(\log n)$ | $O(\log n)$ | — |

La constante oculta del insert en lista es menor que la del array ordenado,
especialmente para elementos grandes (structs de muchos bytes), porque no se
copian datos — solo se redirigen 1-2 punteros.

---

## Diseño

Mantenemos la lista **ordenada por prioridad** en todo momento (menor prioridad
al frente).  Las operaciones:

- **Insert**: recorrer la lista hasta encontrar la posición donde la prioridad
  del nuevo nodo es menor o igual que la del siguiente.  Insertar ahí.
- **Extract min**: el mínimo siempre está al frente — simplemente extraer el
  primer nodo.
- **Peek min**: retornar el dato del primer nodo.

```
Insertar (D, prioridad 3) en lista ordenada:

front → [(A,1)] → [(B,2)] → [(C,5)] → NULL

Recorrer: (A,1) → pri 1 < 3, seguir
          (B,2) → pri 2 < 3, seguir
          (C,5) → pri 5 ≥ 3, insertar aquí

front → [(A,1)] → [(B,2)] → [(D,3)] → [(C,5)] → NULL
```

---

## Implementación en C

### Estructuras

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct PQNode {
    int value;
    int priority;           /* menor valor = mayor urgencia */
    struct PQNode *next;
} PQNode;

typedef struct {
    PQNode *front;          /* siempre el de menor prioridad */
    int count;
} PriorityQueueList;
```

### Inicialización y consultas

```c
void pql_init(PriorityQueueList *pq) {
    pq->front = NULL;
    pq->count = 0;
}

bool pql_is_empty(const PriorityQueueList *pq) {
    return pq->front == NULL;
}

int pql_size(const PriorityQueueList *pq) {
    return pq->count;
}
```

### Insert: $O(n)$ — inserción ordenada

```c
bool pql_insert(PriorityQueueList *pq, int value, int priority) {
    PQNode *node = malloc(sizeof(PQNode));
    if (!node) return false;
    node->value = value;
    node->priority = priority;
    node->next = NULL;

    /* Caso 1: lista vacía o nuevo nodo va al frente */
    if (pq->front == NULL || priority < pq->front->priority) {
        node->next = pq->front;
        pq->front = node;
        pq->count++;
        return true;
    }

    /* Caso 2: buscar posición — insertar después de prev */
    PQNode *prev = pq->front;
    while (prev->next != NULL && prev->next->priority <= priority) {
        prev = prev->next;
    }
    node->next = prev->next;
    prev->next = node;
    pq->count++;
    return true;
}
```

### ¿Por qué `<=` en la comparación?

```c
while (prev->next != NULL && prev->next->priority <= priority)
```

Usar `<=` (avanzar mientras la prioridad del siguiente es menor **o igual**)
hace que los elementos con la misma prioridad se inserten **al final** de su
grupo — esto preserva el orden FIFO entre elementos de igual prioridad
(**estabilidad**).

Con `<` en su lugar, los nuevos elementos se insertarían **al inicio** de su
grupo (LIFO entre iguales).

```
Lista: [(A,2)] → [(B,2)] → [(C,5)]

Insertar (D,2) con <=:
  avanzar por A(2≤2), avanzar por B(2≤2), parar en C(5>2)
  Resultado: [(A,2)] → [(B,2)] → [(D,2)] → [(C,5)]   ← FIFO ✓

Insertar (D,2) con <:
  avanzar por A(2<2? no), insertar después de nada especial...
  Depende de la lógica — generalmente D iría antes de A o entre A y B
```

### Extract min: $O(1)$

```c
bool pql_extract_min(PriorityQueueList *pq, int *out_value, int *out_priority) {
    if (pq->front == NULL) return false;

    PQNode *old = pq->front;
    *out_value = old->value;
    *out_priority = old->priority;

    pq->front = old->next;
    free(old);
    pq->count--;
    return true;
}
```

### Peek min: $O(1)$

```c
bool pql_peek_min(const PriorityQueueList *pq,
                  int *out_value, int *out_priority) {
    if (pq->front == NULL) return false;
    *out_value = pq->front->value;
    *out_priority = pq->front->priority;
    return true;
}
```

### Destrucción

```c
void pql_destroy(PriorityQueueList *pq) {
    PQNode *cur = pq->front;
    while (cur) {
        PQNode *next = cur->next;
        free(cur);
        cur = next;
    }
    pq->front = NULL;
    pq->count = 0;
}
```

### Print (debug)

```c
void pql_print(const PriorityQueueList *pq) {
    printf("[");
    for (PQNode *n = pq->front; n != NULL; n = n->next) {
        printf("(%d,pri=%d)", n->value, n->priority);
        if (n->next) printf(" → ");
    }
    printf("]\n");
}
```

---

## Programa completo

```c
int main(void) {
    PriorityQueueList pq;
    pql_init(&pq);

    printf("Insertando pacientes:\n");
    pql_insert(&pq, 101, 5);   printf("  +101(pri=5): "); pql_print(&pq);
    pql_insert(&pq, 102, 1);   printf("  +102(pri=1): "); pql_print(&pq);
    pql_insert(&pq, 103, 3);   printf("  +103(pri=3): "); pql_print(&pq);
    pql_insert(&pq, 104, 1);   printf("  +104(pri=1): "); pql_print(&pq);
    pql_insert(&pq, 105, 4);   printf("  +105(pri=4): "); pql_print(&pq);
    pql_insert(&pq, 106, 2);   printf("  +106(pri=2): "); pql_print(&pq);

    printf("\nAtendiendo:\n");
    int val, pri;
    while (pql_extract_min(&pq, &val, &pri)) {
        printf("  Paciente %d (prioridad %d)\n", val, pri);
    }

    pql_destroy(&pq);
    return 0;
}
```

Salida:

```
Insertando pacientes:
  +101(pri=5): [(101,pri=5)]
  +102(pri=1): [(102,pri=1) → (101,pri=5)]
  +103(pri=3): [(102,pri=1) → (103,pri=3) → (101,pri=5)]
  +104(pri=1): [(102,pri=1) → (104,pri=1) → (103,pri=3) → (101,pri=5)]
  +105(pri=4): [(102,pri=1) → (104,pri=1) → (103,pri=3) → (105,pri=4) → (101,pri=5)]
  +106(pri=2): [(102,pri=1) → (104,pri=1) → (106,pri=2) → (103,pri=3) → (105,pri=4) → (101,pri=5)]

Atendiendo:
  Paciente 102 (prioridad 1)    ← primero en llegar con pri=1
  Paciente 104 (prioridad 1)    ← segundo con pri=1 (FIFO)
  Paciente 106 (prioridad 2)
  Paciente 103 (prioridad 3)
  Paciente 105 (prioridad 4)
  Paciente 101 (prioridad 5)
```

Notar que 102 y 104 tienen la misma prioridad — salen en orden FIFO gracias
al `<=` en la inserción.

---

## Traza detallada de inserción

Insertar (106, prioridad 2) en la lista:

```
front → [(102,1)] → [(104,1)] → [(103,3)] → [(105,4)] → [(101,5)] → NULL

Nuevo nodo: (106, pri=2)

¿Caso 1?  pri=2 < front->pri=1?  No (2 ≥ 1).  → Caso 2.

Recorrer con prev:
  prev = (102,1):  prev->next=(104,1), pri 1 ≤ 2 → avanzar
  prev = (104,1):  prev->next=(103,3), pri 3 ≤ 2? No (3 > 2) → PARAR

Insertar (106,2) entre (104,1) y (103,3):
  node->next = prev->next = (103,3)
  prev->next = node

front → [(102,1)] → [(104,1)] → [(106,2)] → [(103,3)] → [(105,4)] → [(101,5)]
                                    ↑ nuevo
```

Operación: recorrió 2 nodos.  En el peor caso (prioridad mayor que todos),
recorrería $n$ nodos.

---

## Implementación en Rust

```rust
struct PQNode<T> {
    value: T,
    priority: i32,
}

struct PriorityQueueList<T> {
    items: Vec<PQNode<T>>,    // mantenido ordenado por priority
}

impl<T> PriorityQueueList<T> {
    fn new() -> Self {
        PriorityQueueList { items: Vec::new() }
    }

    fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    fn len(&self) -> usize {
        self.items.len()
    }

    // Insert: O(n) — buscar posición + insertar
    fn insert(&mut self, value: T, priority: i32) {
        let pos = self.items.iter()
            .position(|item| item.priority > priority)
            .unwrap_or(self.items.len());
        self.items.insert(pos, PQNode { value, priority });
    }

    // Extract min: O(1) — el mínimo está en [0]
    fn extract_min(&mut self) -> Option<T> {
        if self.items.is_empty() {
            None
        } else {
            Some(self.items.remove(0).value)
        }
    }

    // Peek min: O(1)
    fn peek_min(&self) -> Option<&T> {
        self.items.first().map(|item| &item.value)
    }
}

fn main() {
    let mut pq = PriorityQueueList::new();

    pq.insert("headache", 5);
    pq.insert("heart_attack", 1);
    pq.insert("fracture", 3);
    pq.insert("hemorrhage", 1);
    pq.insert("sprain", 4);
    pq.insert("burn", 2);

    while let Some(patient) = pq.extract_min() {
        println!("Attending: {patient}");
    }
}
```

### Nota sobre Vec::insert y Vec::remove

`Vec::insert(pos, val)` desplaza los elementos desde `pos` hacia la derecha —
$O(n)$ en el peor caso.  `Vec::remove(0)` desplaza todos hacia la izquierda —
también $O(n)$.

Esto significa que nuestra versión Rust con `Vec` tiene un matiz: aunque la
lógica es "lista ordenada", el desplazamiento físico es el de un **array
ordenado**.  Para tener el comportamiento real de una lista enlazada (sin
desplazar datos), necesitaríamos usar nodos con `Box`, como en T03 de S01.

La implementación con `Vec` es más simple y, para tamaños pequeños, más rápida
(por localidad de caché), pero no refleja la ventaja real de la lista enlazada
para elementos grandes.

### Versión con lista enlazada real en Rust

Para fidelidad con el diseño de lista enlazada:

```rust
struct Node<T> {
    value: T,
    priority: i32,
    next: Option<Box<Node<T>>>,
}

struct PQLinked<T> {
    head: Option<Box<Node<T>>>,
    count: usize,
}

impl<T> PQLinked<T> {
    fn new() -> Self {
        PQLinked { head: None, count: 0 }
    }

    fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    fn insert(&mut self, value: T, priority: i32) {
        let new_node = Node { value, priority, next: None };

        // Caso 1: insertar al frente
        if self.head.is_none() ||
           priority < self.head.as_ref().unwrap().priority {
            let mut boxed = Box::new(new_node);
            boxed.next = self.head.take();
            self.head = Some(boxed);
            self.count += 1;
            return;
        }

        // Caso 2: buscar posición
        let mut current = self.head.as_mut().unwrap();
        while current.next.is_some() &&
              current.next.as_ref().unwrap().priority <= priority {
            current = current.next.as_mut().unwrap();
        }
        let mut boxed = Box::new(new_node);
        boxed.next = current.next.take();
        current.next = Some(boxed);
        self.count += 1;
    }

    fn extract_min(&mut self) -> Option<T> {
        self.head.take().map(|old_head| {
            self.head = old_head.next;
            self.count -= 1;
            old_head.value
        })
    }

    fn peek_min(&self) -> Option<&T> {
        self.head.as_ref().map(|node| &node.value)
    }
}

impl<T> Drop for PQLinked<T> {
    fn drop(&mut self) {
        while self.extract_min().is_some() {}
    }
}
```

Esta versión no desplaza datos — solo redirige punteros `Box`.

---

## Análisis de complejidad

### Insert

| Caso | Nodos recorridos | Situación |
|------|-----------------|-----------|
| Mejor | 0 | Prioridad menor que el frente (insertar al inicio) |
| Peor | $n$ | Prioridad mayor que todos (insertar al final) |
| Promedio | $n/2$ | Prioridad uniformemente distribuida |

Complejidad: $O(n)$ en todos los casos (el promedio sigue siendo lineal).

### Extract min

Siempre $O(1)$ — el mínimo está en `front`.  Sin búsqueda, sin desplazamiento.

### Comparación del costo real de insert $O(n)$

Aunque tanto el array ordenado como la lista ordenada son $O(n)$ para insert,
la operación es cualitativamente diferente:

| Array ordenado | Lista ordenada |
|---------------|----------------|
| Buscar posición: $O(\log n)$ (búsqueda binaria posible) | Buscar posición: $O(n)$ (recorrido secuencial) |
| Desplazar elementos: $O(n)$ movimientos de datos | Redirigir punteros: $O(1)$ |
| Total: $O(n)$ dominado por desplazamiento | Total: $O(n)$ dominado por búsqueda |

Para **elementos grandes** (structs de 100+ bytes), la lista es más eficiente
porque no mueve datos — solo cambia 2 punteros.  Para **elementos pequeños**
(int, char), el array es más eficiente por localidad de caché, y el
desplazamiento es un `memcpy` rápido.

---

## Insertar al frente: el caso especial

El caso de insertar al frente merece atención porque es el que difiere de un
simple recorrido:

```c
/* Caso 1: lista vacía o prioridad menor que el frente */
if (pq->front == NULL || priority < pq->front->priority) {
    node->next = pq->front;
    pq->front = node;
}
```

Si olvidamos este caso, el primer nodo nunca se "adelanta" al frente actual,
y los elementos con prioridad mínima se insertan mal.

Alternativa elegante: usar un **nodo sentinela** con prioridad $-\infty$
(o `INT_MIN`) que siempre está al frente.  Así nunca se da el caso 1:

```c
void pql_init_sentinel(PriorityQueueList *pq) {
    PQNode *sentinel = malloc(sizeof(PQNode));
    sentinel->priority = INT_MIN;
    sentinel->next = NULL;
    pq->front = sentinel;
    pq->count = 0;
}

bool pql_insert(PriorityQueueList *pq, int value, int priority) {
    PQNode *node = malloc(sizeof(PQNode));
    if (!node) return false;
    node->value = value;
    node->priority = priority;

    /* No hay caso especial — el sentinela siempre tiene menor prioridad */
    PQNode *prev = pq->front;
    while (prev->next != NULL && prev->next->priority <= priority) {
        prev = prev->next;
    }
    node->next = prev->next;
    prev->next = node;
    pq->count++;
    return true;
}

bool pql_extract_min(PriorityQueueList *pq, int *out_value, int *out_priority) {
    if (pq->front->next == NULL) return false;   /* solo sentinela = vacía */

    PQNode *first = pq->front->next;   /* primer nodo real */
    *out_value = first->value;
    *out_priority = first->priority;
    pq->front->next = first->next;
    free(first);
    pq->count--;
    return true;
}
```

El sentinela elimina la bifurcación en insert, a costa de un nodo extra
permanente y la necesidad de recordar que `front->next` (no `front`) es el
primer dato real.

---

## Comparación de las tres implementaciones ingenuas

| Criterio | Array desordenado | Array ordenado | Lista ordenada |
|----------|------------------|----------------|----------------|
| Insert | $O(1)$ | $O(n)$ desplazar | $O(n)$ recorrer |
| Extract min | $O(n)$ buscar | $O(1)$ | $O(1)$ |
| Peek min | $O(n)$ | $O(1)$ | $O(1)$ |
| Memoria | Contigua | Contigua | Dispersa + punteros |
| Caché | Buena | Buena | Pobre |
| Elementos grandes | Mover datos en extract | Mover datos en insert | Solo mover punteros |
| Capacidad | Fija (o realloc) | Fija (o realloc) | Ilimitada |
| Estabilidad (FIFO entre iguales) | Depende del orden de búsqueda | Sí, con inserción estable | Sí, con `<=` |
| Mejor escenario | Muchos inserts, pocos extracts | Muchos peeks/extracts | Elementos grandes, capacidad impredecible |

### Recomendación

Para producción, usar un **heap** ($O(\log n)$ ambas operaciones).  Las
implementaciones ingenuas son aceptables cuando:

- $n$ es pequeño (< 50).
- El patrón de uso favorece claramente una operación (batch de inserts seguido
  de batch de extracts, o viceversa).
- Se está aprendiendo — las implementaciones ingenuas son más fáciles de
  entender y depurar.

---

## Max-priority con lista

Para extraer el **máximo** primero, simplemente invertir el orden de la lista
(mayor al frente):

```c
/* En insert, cambiar la comparación: */
if (pq->front == NULL || priority > pq->front->priority) {
    /* Insertar al frente */
}

while (prev->next != NULL && prev->next->priority >= priority) {
    prev = prev->next;
}
```

O, más simple: negar las prioridades al insertar (`pql_insert(pq, value,
-priority)`) y usar la misma implementación min.

---

## Ejercicios

### Ejercicio 1 — Traza de inserción

Partiendo de una lista vacía, inserta los siguientes elementos en orden y
muestra el estado de la lista después de cada inserción:

```
(E, 4), (A, 2), (C, 5), (B, 1), (D, 2), (F, 3)
```

<details>
<summary>Predice el estado final y el orden FIFO de prioridad 2</summary>

Estado final: `(B,1) → (A,2) → (D,2) → (F,3) → (E,4) → (C,5)`.
A y D tienen la misma prioridad — A fue insertada primero, así que con `<=`
está antes.  Extract los retorna: B, A, D, F, E, C.
</details>

### Ejercicio 2 — Insert al frente

Modifica la inserción para que NO use el caso especial del frente (solo caso 2
con recorrido).  ¿Qué bug aparece?  Demuéstralo con la secuencia: insert(X, 5),
insert(Y, 1).

<details>
<summary>El bug</summary>

Sin el caso 1, cuando Y(pri=1) debe ir al frente, el código intenta buscar un
`prev` — pero `prev` empieza en `front` que es X(pri=5).  La condición
`prev->next->priority <= 1` falla inmediatamente (5 > 1).  Y se inserta
**después** de X: `X(5) → Y(1)`.  Extract retorna X primero — orden incorrecto.

Con el caso 1: se detecta `1 < 5`, Y va al frente: `Y(1) → X(5)` ✓.
</details>

### Ejercicio 3 — Versión con sentinela

Implementa la cola de prioridad con nodo sentinela (INT_MIN) completa en C.
Verifica que produce los mismos resultados que la versión sin sentinela para
la secuencia del ejercicio 1.

<details>
<summary>Ventaja a verificar</summary>

La función `insert` no tiene `if` para el caso del frente — solo un bucle.
Esto la hace ligeramente más rápida (una rama menos) y más resistente a bugs.
El costo es 1 malloc extra permanente y la condición `front->next == NULL` en
lugar de `front == NULL` para verificar vacío.
</details>

### Ejercicio 4 — Estabilidad: <= vs <

Implementa dos versiones de insert — una con `<=` y otra con `<` en la
comparación del recorrido.  Inserta 5 elementos con prioridad idéntica (todas
prioridad 3) y compara el orden de extracción.

<details>
<summary>Resultado esperado</summary>

Con `<=`: los elementos salen en orden FIFO (el primero insertado sale primero).
Con `<`: los elementos salen en orden LIFO (el último insertado sale primero)
o en un orden inconsistente, dependiendo de la implementación exacta.
La estabilidad FIFO suele ser deseable.
</details>

### Ejercicio 5 — Cola de prioridad con lista en Rust (Box)

Implementa la versión con `Box<Node<T>>` (lista enlazada real, no Vec).
Prueba con un tipo que no es `Copy` (ej. `String`).  Verifica que los
elementos se mueven correctamente sin clonar.

<details>
<summary>Pista para String</summary>

```rust
let mut pq = PQLinked::new();
pq.insert("backup".to_string(), 5);
pq.insert("deploy".to_string(), 1);
pq.insert("test".to_string(), 3);

// extract_min mueve el String fuera del nodo — sin Clone
while let Some(task) = pq.extract_min() {
    println!("{task}");   // deploy, test, backup
}
```

`Box<Node>` se destruye automáticamente al hacer `extract_min` — el `Drop`
de `Box` libera el nodo.
</details>

### Ejercicio 6 — Benchmark: lista vs array

Compara el rendimiento de insert para $n = 10000$ elementos con prioridades
aleatorias, usando:
1. Array desordenado (insert $O(1)$).
2. Array ordenado (insert $O(n)$ con desplazamiento).
3. Lista ordenada (insert $O(n)$ con recorrido).

Luego extrae todos.

<details>
<summary>Predicción</summary>

Insert total — desordenado: $O(n)$ = instantáneo.  Ordenado y lista: ambos
$O(n^2)$, pero la lista debería ser más rápida que el array para inserts porque
no desplaza datos.  La diferencia se amplifica con elementos grandes.

Extract total — desordenado: $O(n^2)$ (cada extract busca).  Ordenado y lista:
$O(n)$ (cada extract es $O(1)$).

Total ambos: $O(n^2)$ para las tres.  La ventaja del heap ($O(n \log n)$) se
nota a partir de $n \approx 1000$.
</details>

### Ejercicio 7 — Delete y change_priority

Añade dos operaciones a la cola de prioridad con lista:
- `delete(pq, value)` — eliminar el elemento con ese valor.
- `change_priority(pq, value, new_priority)` — cambiar la prioridad de un
  elemento existente.

<details>
<summary>Complejidad</summary>

Ambas requieren recorrer la lista para encontrar el nodo: $O(n)$.  `delete`
además redirige punteros.  `change_priority` puede implementarse como
delete + insert, o encontrar el nodo, sacarlo, y reinsertarlo con la nueva
prioridad.

En un heap, estas operaciones también son $O(n)$ para la búsqueda (a menos
que se use un hash map auxiliar para localizar posiciones — técnica usada en
Dijkstra con decrease-key).
</details>

### Ejercicio 8 — Merge de dos colas de prioridad

Escribe una función que mezcle dos colas de prioridad ordenadas en una sola,
también ordenada.  Esto es análogo al merge de merge sort.

<details>
<summary>Complejidad</summary>

Con listas enlazadas: $O(n + m)$ — recorrer ambas listas simultáneamente
eligiendo el menor, exactamente como el merge de merge sort.  No se necesita
espacio extra (solo redirigir punteros de los nodos existentes).

Con arrays: también $O(n + m)$, pero se necesita un array auxiliar.

Es una ventaja clara de la lista sobre el array para esta operación.
</details>

### Ejercicio 9 — Planificador de procesos

Simula un planificador de SO con cola de prioridad.  Cada proceso tiene un
nombre, prioridad (1-10, menor = más importante) y tiempo de CPU requerido.
El planificador extrae el proceso de mayor prioridad, lo ejecuta 1 unidad de
tiempo, y si le queda tiempo, lo reinserta.

<details>
<summary>Estructura</summary>

```c
typedef struct {
    char name[16];
    int priority;
    int remaining_time;
} Process;
```

Simular hasta que todos los procesos terminen.  Este es un planificador con
**preemption por prioridad** — un proceso de mayor prioridad que llega
interrumpe al actual.  Notar que reinsertar cuesta $O(n)$ cada vez — con un
heap sería $O(\log n)$.
</details>

### Ejercicio 10 — Comparar con BinaryHeap

Implementa la misma cola de prioridad con la lista ordenada en Rust y con
`BinaryHeap<Reverse<(i32, T)>>`.  Para $n = 50000$:
1. Mide el tiempo de $n$ inserts + $n$ extracts con cada una.
2. Calcula el speedup del heap vs la lista.

<details>
<summary>Predicción</summary>

Lista: insert $O(n)$ × $n$ + extract $O(1)$ × $n$ = $O(n^2) \approx 2.5
\times 10^9$.  Para $n = 50000$: probablemente varios segundos.

Heap: insert $O(\log n)$ × $n$ + extract $O(\log n)$ × $n$ = $O(n \log n)
\approx 50000 \times 16 \times 2 \approx 1.6 \times 10^6$.

Speedup teórico: $\approx 1500\times$.  Esto demuestra convincentemente por
qué el heap es la implementación correcta para colas de prioridad de tamaño
no trivial.  La implementación con lista es útil pedagógicamente, pero no en
producción para $n$ grande.
</details>
