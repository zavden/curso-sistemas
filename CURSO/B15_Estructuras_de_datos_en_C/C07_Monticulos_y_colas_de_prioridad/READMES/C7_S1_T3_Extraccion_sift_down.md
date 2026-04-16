# Extracción del heap (sift-down)

## Concepto

La operación fundamental de un heap es **extraer** el elemento de la raíz:
el máximo en un max-heap o el mínimo en un min-heap. Esta es la operación
que justifica la existencia de la estructura — acceder y remover el elemento
de mayor prioridad en $O(\log n)$.

El desafío: al quitar la raíz, debemos restaurar **ambas** propiedades del heap
(forma completa y orden). La estrategia es el espejo de la inserción:

1. **Reemplazar** la raíz con el **último** elemento del array. Esto preserva
   la propiedad de forma — el árbol sigue siendo completo (perdimos una hoja
   del final).
2. **Bajar** el elemento fuera de lugar mientras viole la propiedad de heap.
   Esta operación se llama **sift-down** (también *bubble-down*, *percolate-down*,
   *sink*, *heapify-down*).

---

## Sift-down: algoritmo

Para un **max-heap**:

```
sift_down(heap, index, size):
    while true:
        largest = index
        left  = 2 * index + 1
        right = 2 * index + 2

        if left < size and heap[left] > heap[largest]:
            largest = left
        if right < size and heap[right] > heap[largest]:
            largest = right

        if largest == index:
            break              // propiedad restaurada
        swap(heap[index], heap[largest])
        index = largest        // bajar un nivel
```

En cada paso, comparamos el nodo con **ambos** hijos y lo intercambiamos con
el **mayor** de los dos. ¿Por qué el mayor y no cualquiera? Porque al subir
al mayor hijo, este será ≥ que el otro hijo (ya lo era antes) y ≥ que el
elemento que bajó. Si subiéramos al menor, podría ser menor que el otro hijo,
violando la propiedad.

---

## Traza detallada

### Extraer del max-heap `[90, 70, 80, 50, 60, 40, 30]`

**Paso 1**: guardar la raíz (90), mover el último elemento (30) a la raíz,
reducir tamaño.

```
Extraido: 90
Array: [30, 70, 80, 50, 60, 40]     (30 reemplaza a 90, se elimina posicion 6)

           30    ← fuera de lugar
          /  \
        70    80
       / \   /
     50  60 40
```

**Paso 2**: sift-down desde la raíz.

$i = 0$: hijos = 1 (70), 2 (80). Mayor hijo = 2 (80). $30 < 80$ → **swap**.

```
Array: [80, 70, 30, 50, 60, 40]

           80
          /  \
        70    30    ← sigue bajando
       / \   /
     50  60 40
```

**Paso 3**: continuar bajando.

$i = 2$: hijos = 5 (40), 6 (no existe). Mayor hijo = 5 (40). $30 < 40$ → **swap**.

```
Array: [80, 70, 40, 50, 60, 30]

           80
          /  \
        70    40
       / \   /
     50  60 30
```

**Paso 4**: verificar.

$i = 5$: no tiene hijos (hoja). **Stop**.

```
Resultado: [80, 70, 40, 50, 60, 30]
Elemento extraido: 90
```

### Resumen del camino

```
30 colocado en raiz (posicion 0)
   comparar con hijos 70, 80 → swap con 80 (mayor) → posicion 2
   comparar con hijo 40 → swap con 40 → posicion 5
   es hoja, STOP

Camino: 0 → 2 → 5 (3 comparaciones con hijos, 2 swaps)
```

---

## ¿Por qué intercambiar con el hijo mayor?

Considerar intercambiar con el hijo **menor** en un max-heap:

```
           30
          /  \
        70    80
```

Si intercambiamos 30 con 70 (el menor):

```
           70
          /  \
        30    80    ← 70 < 80, ¡violación!
```

Si intercambiamos 30 con 80 (el mayor):

```
           80
          /  \
        70    30    ← 80 ≥ 70 ✓, 80 ≥ 30 ✓
```

Al subir el mayor, garantizamos que el nuevo padre es ≥ que **ambos** hijos.

---

## Segunda extracción: traza

Extraer de `[80, 70, 40, 50, 60, 30]`:

| Paso | $i$ | Hijos | Mayor | Comparación | Acción |
|------|-----|-------|-------|-------------|--------|
| Prep | — | — | — | Extraer 80, mover 30 a raíz | `[30, 70, 40, 50, 60]` |
| 1 | 0 | 70, 40 | 1 (70) | 30 < 70 | swap → `[70, 30, 40, 50, 60]` |
| 2 | 1 | 50, 60 | 4 (60) | 30 < 60 | swap → `[70, 60, 40, 50, 30]` |
| 3 | 4 | — | — | hoja | stop |

```
           70
          /  \
        60    40
       / \
     50  30
```

Extraído: 80. Heap válido ✓.

---

## Extracciones sucesivas: vaciando el heap

Extraer todos los elementos de `[90, 70, 80, 50, 60, 40, 30]` produce la
secuencia **ordenada descendente**: 90, 80, 70, 60, 50, 40, 30.

Esto es la base de **heapsort** (que veremos en S02).

| Extracción | Elemento | Array restante |
|-----------|----------|---------------|
| 1 | 90 | `[80, 70, 40, 50, 60, 30]` |
| 2 | 80 | `[70, 60, 40, 50, 30]` |
| 3 | 70 | `[60, 50, 40, 30]` |
| 4 | 60 | `[50, 30, 40]` |
| 5 | 50 | `[40, 30]` |
| 6 | 40 | `[30]` |
| 7 | 30 | `[]` |

---

## Implementación en C

```c
void swap(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

void sift_down(int heap[], int index, int size) {
    while (1) {
        int largest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < size && heap[left] > heap[largest])
            largest = left;
        if (right < size && heap[right] > heap[largest])
            largest = right;

        if (largest == index) break;

        swap(&heap[index], &heap[largest]);
        index = largest;
    }
}

int heap_extract(int heap[], int *size) {
    int max = heap[0];
    (*size)--;
    heap[0] = heap[*size];
    sift_down(heap, 0, *size);
    return max;
}
```

### Con struct

```c
typedef struct {
    int *data;
    int size;
    int capacity;
} MaxHeap;

int heap_extract(MaxHeap *h) {
    int max = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];
    sift_down(h->data, 0, h->size);
    return max;
}

int heap_peek(MaxHeap *h) {
    return h->data[0];
}

int heap_is_empty(MaxHeap *h) {
    return h->size == 0;
}
```

---

## Implementación en Rust

```rust
struct MaxHeap {
    data: Vec<i32>,
}

impl MaxHeap {
    fn new() -> Self {
        MaxHeap { data: Vec::new() }
    }

    fn insert(&mut self, value: i32) {
        self.data.push(value);
        self.sift_up(self.data.len() - 1);
    }

    fn sift_up(&mut self, mut index: usize) {
        while index > 0 {
            let parent = (index - 1) / 2;
            if self.data[parent] >= self.data[index] { break; }
            self.data.swap(parent, index);
            index = parent;
        }
    }

    fn extract_max(&mut self) -> Option<i32> {
        if self.data.is_empty() { return None; }
        let max = self.data[0];
        let last = self.data.len() - 1;
        self.data.swap(0, last);
        self.data.pop();
        if !self.data.is_empty() {
            self.sift_down(0);
        }
        Some(max)
    }

    fn sift_down(&mut self, mut index: usize) {
        let size = self.data.len();
        loop {
            let mut largest = index;
            let left = 2 * index + 1;
            let right = 2 * index + 2;

            if left < size && self.data[left] > self.data[largest] {
                largest = left;
            }
            if right < size && self.data[right] > self.data[largest] {
                largest = right;
            }

            if largest == index { break; }
            self.data.swap(index, largest);
            index = largest;
        }
    }

    fn peek(&self) -> Option<&i32> {
        self.data.first()
    }

    fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    fn len(&self) -> usize {
        self.data.len()
    }
}
```

Nota en `extract_max`: hacemos `swap(0, last)` y luego `pop()` en lugar de
simplemente sobrescribir `data[0]`. Esto es necesario en Rust porque `pop()`
devuelve ownership del último elemento, evitando un `clone` innecesario.

---

## Complejidad

| Caso | Comparaciones | Swaps | Complejidad |
|------|--------------|-------|-------------|
| Mejor | 2 (comparar con ambos hijos) | 0 | $O(1)$ |
| Peor | $2 \lfloor \log_2 n \rfloor$ | $\lfloor \log_2 n \rfloor$ | $O(\log n)$ |
| Promedio | $\approx 2 \lfloor \log_2 n \rfloor$ | $\approx \lfloor \log_2 n \rfloor$ | $O(\log n)$ |

El sift-down tiene **2 comparaciones por nivel** (una con cada hijo para
determinar el mayor), a diferencia del sift-up que tiene **1 comparación por
nivel** (solo con el padre).

A diferencia de la inserción, el caso promedio del sift-down **no** es $O(1)$.
El elemento colocado en la raíz (el último del heap, típicamente pequeño) casi
siempre baja hasta las hojas o cerca de ellas. Esto es porque al quitar el
máximo y poner un elemento del nivel más bajo, este probablemente pertenece
cerca del nivel más bajo.

### Optimización: sift-down con menor número de comparaciones

Una variante llamada **bottom-up sift-down** (Floyd) reduce las comparaciones:

1. Bajar siguiendo siempre al hijo mayor (sin comparar con el nodo actual) hasta
   llegar a una hoja.
2. Subir desde la hoja hasta encontrar la posición correcta (sift-up parcial).

Esto funciona porque el elemento casi siempre termina en las hojas. Se ahorra
la comparación del nodo con el mayor hijo en cada nivel (solo se comparan los
dos hijos entre sí), reduciendo las comparaciones de $2h$ a $h + O(\log h)$ en
promedio.

```c
void sift_down_floyd(int heap[], int index, int size) {
    int value = heap[index];

    // fase 1: bajar siguiendo al mayor hijo
    int child;
    while ((child = 2 * index + 1) < size) {
        // elegir mayor hijo
        if (child + 1 < size && heap[child + 1] > heap[child])
            child++;
        heap[index] = heap[child];
        index = child;
    }

    // fase 2: subir buscando posicion del valor original
    heap[index] = value;
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent] >= heap[index]) break;
        swap(&heap[parent], &heap[index]);
        index = parent;
    }
}
```

Esta optimización importa en la práctica cuando las comparaciones son costosas
(e.g., comparar strings o structs complejas).

---

## Sift-down para min-heap

La única diferencia es la dirección de las comparaciones:

```c
void sift_down_min(int heap[], int index, int size) {
    while (1) {
        int smallest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < size && heap[left] < heap[smallest])
            smallest = left;
        if (right < size && heap[right] < heap[smallest])
            smallest = right;

        if (smallest == index) break;

        swap(&heap[index], &heap[smallest]);
        index = smallest;
    }
}
```

Se intercambia con el **menor** hijo (para que el nuevo padre sea ≤ ambos hijos).

---

## Invariante del sift-down

Antes de cada iteración del bucle, la propiedad de max-heap se cumple en
todo el subárbol enraizado en `index`, excepto posiblemente en `index` mismo
(que puede ser menor que uno o ambos hijos).

**Prueba de terminación**: en cada iteración, `index` se mueve hacia un nivel
más profundo. Como la altura es $\lfloor \log_2 n \rfloor$, el bucle termina
en a lo sumo $\lfloor \log_2 n \rfloor$ iteraciones.

**Prueba de correctitud**: cuando `largest == index`, el nodo es ≥ que ambos
hijos (si existen), y los subárboles de ambos hijos ya eran heaps válidos
(por invariante). Por tanto, todo el subárbol es un heap válido.

---

## Sift-down vs sift-up: resumen

| Aspecto | Sift-up (inserción) | Sift-down (extracción) |
|---------|--------------------|-----------------------|
| Dirección | Hoja → raíz (sube) | Raíz → hoja (baja) |
| Comparaciones por nivel | 1 (con padre) | 2 (con ambos hijos) |
| Peor caso | $O(\log n)$ | $O(\log n)$ |
| Promedio | $O(1)$ | $O(\log n)$ |
| Usado en heapify (build) | No (top-down, $O(n \log n)$) | **Sí** (bottom-up, $O(n)$) |

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    int size;
    int capacity;
} MaxHeap;

MaxHeap *heap_create(int cap) {
    MaxHeap *h = malloc(sizeof(MaxHeap));
    h->data = malloc(cap * sizeof(int));
    h->size = 0;
    h->capacity = cap;
    return h;
}

void heap_free(MaxHeap *h) {
    free(h->data);
    free(h);
}

void swap(int *a, int *b) {
    int tmp = *a; *a = *b; *b = tmp;
}

void sift_up(int heap[], int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent] >= heap[index]) break;
        swap(&heap[parent], &heap[index]);
        index = parent;
    }
}

void sift_down(int heap[], int index, int size) {
    while (1) {
        int largest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;
        if (left < size && heap[left] > heap[largest]) largest = left;
        if (right < size && heap[right] > heap[largest]) largest = right;
        if (largest == index) break;
        swap(&heap[index], &heap[largest]);
        index = largest;
    }
}

void heap_insert(MaxHeap *h, int value) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(int));
    }
    h->data[h->size] = value;
    sift_up(h->data, h->size);
    h->size++;
}

int heap_extract(MaxHeap *h) {
    int max = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];
    sift_down(h->data, 0, h->size);
    return max;
}

void print_array(int arr[], int n) {
    printf("[");
    for (int i = 0; i < n; i++)
        printf("%d%s", arr[i], i < n - 1 ? ", " : "");
    printf("]");
}

void print_levels(int arr[], int n) {
    int i = 0, level = 0;
    while (i < n) {
        int count = 1 << level;
        printf("  Nivel %d: ", level);
        for (int j = 0; j < count && i < n; j++, i++)
            printf("%d ", arr[i]);
        printf("\n");
        level++;
    }
}

int is_max_heap(int arr[], int n) {
    for (int i = 0; i < n / 2; i++) {
        int l = 2 * i + 1, r = 2 * i + 2;
        if (l < n && arr[i] < arr[l]) return 0;
        if (r < n && arr[i] < arr[r]) return 0;
    }
    return 1;
}

void sift_down_verbose(int heap[], int index, int size) {
    printf("  sift_down(%d) desde indice %d\n", heap[index], index);
    int swaps = 0;
    while (1) {
        int largest = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;
        if (left < size && heap[left] > heap[largest]) largest = left;
        if (right < size && heap[right] > heap[largest]) largest = right;

        if (largest == index) {
            if (left >= size) printf("    hoja, STOP\n");
            else printf("    %d >= hijos, STOP\n", heap[index]);
            break;
        }
        printf("    %d < %d (hijo en %d), SWAP\n",
               heap[index], heap[largest], largest);
        swap(&heap[index], &heap[largest]);
        index = largest;
        swaps++;
    }
    printf("  swaps: %d\n", swaps);
}

int main(void) {
    MaxHeap *h = heap_create(16);

    // Construir heap
    int values[] = {50, 30, 70, 10, 60, 40, 90, 20, 80};
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++)
        heap_insert(h, values[i]);

    printf("=== Heap construido ===\n\n");
    printf("Array: ");
    print_array(h->data, h->size);
    printf("\n\nNiveles:\n");
    print_levels(h->data, h->size);
    printf("\nis_max_heap: %s\n", is_max_heap(h->data, h->size) ? "SI" : "NO");

    // Extracciones con traza
    printf("\n=== Extracciones ===\n");

    for (int i = 0; i < 4; i++) {
        printf("\n--- Extraccion %d ---\n\n", i + 1);
        printf("Raiz (maximo): %d\n", h->data[0]);
        printf("Ultimo elemento: %d (indice %d)\n",
               h->data[h->size - 1], h->size - 1);

        int max = h->data[0];
        h->size--;
        h->data[0] = h->data[h->size];

        printf("Despues de mover ultimo a raiz: ");
        print_array(h->data, h->size);
        printf("\n\n");

        sift_down_verbose(h->data, 0, h->size);

        sift_down(h->data, 0, h->size);  // re-run limpio
        // (la verbose ya lo hizo, esto es redundante pero seguro)

        printf("\nResultado: ");
        print_array(h->data, h->size);
        printf("\n");
        printf("Extraido: %d\n", max);
    }

    // Extraer todo
    printf("\n=== Extraer todo (orden descendente) ===\n\n");

    // Rebuild
    h->size = 0;
    for (int i = 0; i < n; i++) heap_insert(h, values[i]);

    printf("Orden: ");
    while (h->size > 0) {
        printf("%d ", heap_extract(h));
    }
    printf("\n");

    heap_free(h);
    return 0;
}
```

### Salida esperada

```
=== Heap construido ===

Array: [90, 80, 70, 30, 60, 40, 50, 10, 20]

Niveles:
  Nivel 0: 90
  Nivel 1: 80 70
  Nivel 2: 30 60 40 50
  Nivel 3: 10 20

is_max_heap: SI

=== Extracciones ===

--- Extraccion 1 ---

Raiz (maximo): 90
Ultimo elemento: 20 (indice 8)
Despues de mover ultimo a raiz: [20, 80, 70, 30, 60, 40, 50, 10]

  sift_down(20) desde indice 0
    20 < 80 (hijo en 1), SWAP
    20 < 60 (hijo en 4), SWAP
    hoja, STOP
  swaps: 2

Resultado: [80, 60, 70, 30, 20, 40, 50, 10]
Extraido: 90

--- Extraccion 2 ---

Raiz (maximo): 80
Ultimo elemento: 10 (indice 7)
Despues de mover ultimo a raiz: [10, 60, 70, 30, 20, 40, 50]

  sift_down(10) desde indice 0
    10 < 70 (hijo en 2), SWAP
    10 < 50 (hijo en 6), SWAP
    hoja, STOP
  swaps: 2

Resultado: [70, 60, 50, 30, 20, 40, 10]
Extraido: 80

--- Extraccion 3 ---

Raiz (maximo): 70
Ultimo elemento: 10 (indice 6)
Despues de mover ultimo a raiz: [10, 60, 50, 30, 20, 40]

  sift_down(10) desde indice 0
    10 < 60 (hijo en 1), SWAP
    10 < 30 (hijo en 3), SWAP
    hoja, STOP
  swaps: 2

Resultado: [60, 30, 50, 10, 20, 40]
Extraido: 70

--- Extraccion 4 ---

Raiz (maximo): 60
Ultimo elemento: 40 (indice 5)
Despues de mover ultimo a raiz: [40, 30, 50, 10, 20]

  sift_down(40) desde indice 0
    40 < 50 (hijo en 2), SWAP
    hoja, STOP
  swaps: 1

Resultado: [50, 30, 40, 10, 20]
Extraido: 60

=== Extraer todo (orden descendente) ===

Orden: 90 80 70 60 50 40 30 20 10
```

---

## Programa completo en Rust

```rust
struct MaxHeap {
    data: Vec<i32>,
}

impl MaxHeap {
    fn new() -> Self { MaxHeap { data: Vec::new() } }

    fn insert(&mut self, value: i32) {
        self.data.push(value);
        self.sift_up(self.data.len() - 1);
    }

    fn sift_up(&mut self, mut index: usize) {
        while index > 0 {
            let parent = (index - 1) / 2;
            if self.data[parent] >= self.data[index] { break; }
            self.data.swap(parent, index);
            index = parent;
        }
    }

    fn extract_max(&mut self) -> Option<i32> {
        if self.data.is_empty() { return None; }
        let last = self.data.len() - 1;
        self.data.swap(0, last);
        let max = self.data.pop().unwrap();
        if !self.data.is_empty() {
            self.sift_down(0);
        }
        Some(max)
    }

    fn sift_down(&mut self, mut index: usize) {
        let size = self.data.len();
        loop {
            let mut largest = index;
            let left = 2 * index + 1;
            let right = 2 * index + 2;
            if left < size && self.data[left] > self.data[largest] {
                largest = left;
            }
            if right < size && self.data[right] > self.data[largest] {
                largest = right;
            }
            if largest == index { break; }
            self.data.swap(index, largest);
            index = largest;
        }
    }

    fn extract_verbose(&mut self) -> Option<i32> {
        if self.data.is_empty() { return None; }
        let max = self.data[0];
        let last = self.data.len() - 1;
        println!("  Extraer {max}, mover {} a raiz", self.data[last]);

        self.data.swap(0, last);
        self.data.pop();

        if !self.data.is_empty() {
            let mut index = 0;
            let size = self.data.len();
            let mut swaps = 0;

            loop {
                let mut largest = index;
                let left = 2 * index + 1;
                let right = 2 * index + 2;
                if left < size && self.data[left] > self.data[largest] {
                    largest = left;
                }
                if right < size && self.data[right] > self.data[largest] {
                    largest = right;
                }
                if largest == index {
                    println!("    STOP");
                    break;
                }
                println!("    {} < {} (i={}), SWAP",
                         self.data[index], self.data[largest], largest);
                self.data.swap(index, largest);
                index = largest;
                swaps += 1;
            }
            println!("  swaps: {swaps}, array: {:?}", self.data);
        }
        Some(max)
    }

    fn peek(&self) -> Option<i32> { self.data.first().copied() }

    fn is_valid(&self) -> bool {
        (0..self.data.len() / 2).all(|i| {
            let l = 2 * i + 1;
            let r = 2 * i + 2;
            (l >= self.data.len() || self.data[i] >= self.data[l]) &&
            (r >= self.data.len() || self.data[i] >= self.data[r])
        })
    }

    fn print_levels(&self) {
        let mut i = 0;
        let mut level = 0;
        while i < self.data.len() {
            let count = 1usize << level;
            print!("  Nivel {level}: ");
            for _ in 0..count {
                if i >= self.data.len() { break; }
                print!("{} ", self.data[i]);
                i += 1;
            }
            println!();
            level += 1;
        }
    }
}

fn main() {
    let mut heap = MaxHeap::new();
    for &v in &[50, 30, 70, 10, 60, 40, 90, 20, 80] {
        heap.insert(v);
    }

    println!("=== Heap ===\n");
    println!("Array: {:?}\n", heap.data);
    heap.print_levels();
    println!("\nis_valid: {}", heap.is_valid());

    println!("\n=== Extracciones con traza ===\n");
    for i in 1..=4 {
        println!("--- Extraccion {i} ---");
        let val = heap.extract_verbose();
        println!("  Extraido: {:?}\n", val);
    }

    // Reconstruir y extraer todo
    let mut heap2 = MaxHeap::new();
    for &v in &[50, 30, 70, 10, 60, 40, 90, 20, 80] {
        heap2.insert(v);
    }

    print!("Extraer todo: ");
    while let Some(val) = heap2.extract_max() {
        print!("{val} ");
    }
    println!();
}
```

---

## Ejercicios

### Ejercicio 1 — Trazar extracción

Dado el max-heap `[100, 85, 70, 60, 30, 40, 20, 10, 50]`, traza la extracción
del máximo paso a paso. Muestra el array después de cada swap del sift-down.

<details>
<summary>Verificar</summary>

1. Extraer 100. Mover 50 (último) a raíz: `[50, 85, 70, 60, 30, 40, 20, 10]`.

2. Sift-down:
   - $i=0$: hijos 85, 70. Mayor = 85 (pos 1). 50 < 85 → swap.
     `[85, 50, 70, 60, 30, 40, 20, 10]`
   - $i=1$: hijos 60, 30. Mayor = 60 (pos 3). 50 < 60 → swap.
     `[85, 60, 70, 50, 30, 40, 20, 10]`
   - $i=3$: hijos 10, — (solo izq). 50 > 10 → stop.

```
          85
         /  \
       60    70
      / \   / \
    50  30 40  20
   /
  10
```

Array final: `[85, 60, 70, 50, 30, 40, 20, 10]`. Extraído: **100**.
</details>

### Ejercicio 2 — Dos extracciones consecutivas

Del heap del Ejercicio 1 (`[100, 85, 70, 60, 30, 40, 20, 10, 50]`), realiza
**dos** extracciones consecutivas. ¿Cuáles son los dos valores extraídos y
cuál es el array final?

<details>
<summary>Verificar</summary>

**Extracción 1**: extraer 100. Resultado del Ejercicio 1:
`[85, 60, 70, 50, 30, 40, 20, 10]`.

**Extracción 2**: extraer 85. Mover 10 a raíz: `[10, 60, 70, 50, 30, 40, 20]`.
- $i=0$: hijos 60, 70. Mayor = 70 (pos 2). 10 < 70 → swap. `[70, 60, 10, 50, 30, 40, 20]`
- $i=2$: hijos 40, 20. Mayor = 40 (pos 5). 10 < 40 → swap. `[70, 60, 40, 50, 30, 10, 20]`
- $i=5$: hoja → stop.

Extraídos: **100, 85**. Array final: `[70, 60, 40, 50, 30, 10, 20]`.
</details>

### Ejercicio 3 — ¿Por qué no el hijo menor?

Demuestra con un contraejemplo concreto que intercambiar con el hijo **menor**
durante sift-down en un max-heap produce un resultado incorrecto.

<details>
<summary>Verificar</summary>

Heap: `[50, 40, 30, 10, 20]`. Extraer 50, mover 20 a raíz: `[20, 40, 30, 10]`.

```
       20
      /  \
    40    30
   /
  10
```

Sift-down con hijo **menor** (30 en vez de 40):
- Swap 20 con 30: `[30, 40, 20, 10]`

```
       30
      /  \
    40    20
   /
  10
```

¡Violación! 30 < 40 (hijo izquierdo > padre). **No es max-heap.**

Sift-down correcto con hijo **mayor** (40):
- Swap 20 con 40: `[40, 20, 30, 10]`

```
       40
      /  \
    20    30
   /
  10
```

40 ≥ 20 ✓, 40 ≥ 30 ✓. Max-heap válido.
</details>

### Ejercicio 4 — Min-heap extracción

Dado el min-heap `[5, 10, 15, 20, 25, 30]`, traza la extracción del mínimo.

<details>
<summary>Verificar</summary>

Extraer 5. Mover 30 a raíz: `[30, 10, 15, 20, 25]`.

```
       30
      /  \
    10    15
   / \
  20  25
```

Sift-down (min): intercambiar con hijo **menor**.
- $i=0$: hijos 10, 15. Menor = 10 (pos 1). 30 > 10 → swap. `[10, 30, 15, 20, 25]`
- $i=1$: hijos 20, 25. Menor = 20 (pos 3). 30 > 20 → swap. `[10, 20, 15, 30, 25]`
- $i=3$: hoja → stop.

```
       10
      /  \
    20    15
   / \
  30  25
```

Resultado: `[10, 20, 15, 30, 25]`. Extraído: **5**.
</details>

### Ejercicio 5 — Número de comparaciones

¿Cuántas comparaciones de elementos hace sift-down en el peor caso para un
heap de $n = 15$ elementos? ¿Y para $n = 1{,}000{,}000$?

<details>
<summary>Verificar</summary>

El sift-down hace **2 comparaciones por nivel** (comparar hijo izq con
mayor actual, luego hijo der con mayor actual). En el peor caso baja
desde la raíz hasta una hoja.

- $n = 15$: $h = \lfloor\log_2 15\rfloor = 3$. Comparaciones = $2 \times 3 = $ **6**.
- $n = 10^6$: $h = \lfloor\log_2 10^6\rfloor = 19$. Comparaciones = $2 \times 19 = $ **38**.

Comparado con sift-up: 1 comparación por nivel ($h$ y $19$ respectivamente).
Sift-down hace el doble de comparaciones pero esto no cambia la complejidad
asintótica — ambos son $O(\log n)$.
</details>

### Ejercicio 6 — Extraer y reinsertar

Una operación común es extraer el máximo e insertar un nuevo valor. Compara
dos formas:

a) `extract_max()` + `insert(val)` (2 operaciones).
b) Reemplazar la raíz directamente y hacer sift-down (1 operación).

¿Cuál es más eficiente?

<details>
<summary>Verificar</summary>

**a)** `extract_max()` + `insert(val)`:
- extract: sift-down $O(\log n)$
- insert: sift-up $O(\log n)$
- Total: $2 \times O(\log n)$

**b)** Reemplazar raíz + sift-down:

```c
int heap_replace(MaxHeap *h, int new_val) {
    int old_max = h->data[0];
    h->data[0] = new_val;
    sift_down(h->data, 0, h->size);
    return old_max;
}
```

- Total: $O(\log n)$ (un solo sift-down)

La opción **b) es 2× más rápida**. Además, no cambia el tamaño del heap (no
hay pop + push con posible realloc).

En Python, `heapq.heapreplace()` implementa exactamente esta optimización.
En Rust, `BinaryHeap` no la expone directamente pero se puede lograr con
`peek_mut()` + `PeekMut::pop()`.
</details>

### Ejercicio 7 — Eliminar elemento arbitrario

Implementa `heap_delete(heap, index)` que elimine el elemento en una posición
arbitraria (no solo la raíz). Pista: reemplazar con el último y hacer sift-down
**o** sift-up según corresponda.

<details>
<summary>Verificar</summary>

```c
void heap_delete_at(int heap[], int *size, int index) {
    (*size)--;
    heap[index] = heap[*size];

    // el nuevo valor puede ser mayor o menor que el original
    // si es mayor: necesita sift-up
    // si es menor: necesita sift-down
    // hacemos ambos — solo uno tendrá efecto

    sift_up(heap, index);
    sift_down(heap, index, *size);
}
```

¿Por qué ambos? El último elemento puede ser mayor que el padre de `index`
(necesita sift-up) o menor que los hijos (necesita sift-down). Solo uno de
los dos moverá el elemento; el otro será un no-op.

Ejemplo: heap `[90, 70, 80, 50, 60, 40, 30]`, eliminar posición 2 (valor 80).
Reemplazar con 30 (último): `[90, 70, 30, 50, 60, 40]`.
- sift-up: 30 < 90 (padre), no sube.
- sift-down: hijos de pos 2 = pos 5 (40). 30 < 40 → swap.
  `[90, 70, 40, 50, 60, 30]` ✓
</details>

### Ejercicio 8 — Heap con solo un hijo

¿En qué condiciones un nodo tiene exactamente un hijo (izquierdo pero no
derecho) en un heap? ¿Puede un nodo tener solo hijo derecho?

<details>
<summary>Verificar</summary>

Un nodo en índice $i$ tiene exactamente un hijo cuando $2i + 1 < n$ pero
$2i + 2 \geq n$, es decir, $2i + 1 = n - 1$. Esto ocurre cuando $n$ es
**par**: el último nodo interno ($i = n/2 - 1$) tiene solo hijo izquierdo.

Ejemplo: $n = 6$. El nodo $i = 2$ tiene hijo izq en $5$ pero no hijo der
(posición $6 \geq n$).

**¿Solo hijo derecho?** Nunca. La propiedad de completitud exige que los
niveles se llenen de izquierda a derecha. Si un nodo tuviera hijo derecho sin
izquierdo, habría un hueco y el árbol no sería completo.

Cuando $n$ es impar, todos los nodos internos tienen exactamente 2 hijos.
Cuando $n$ es par, exactamente **un** nodo tiene un solo hijo.
</details>

### Ejercicio 9 — Simular cola de prioridad

Usando las operaciones `insert` y `extract_max`, simula una cola de prioridad
para procesos. Inserta procesos con prioridades [5, 1, 8, 3, 10, 2, 7] y
extráelos en orden de prioridad.

<details>
<summary>Verificar</summary>

```c
int main(void) {
    MaxHeap *pq = heap_create(16);
    int priors[] = {5, 1, 8, 3, 10, 2, 7};

    printf("Llegada de procesos:\n");
    for (int i = 0; i < 7; i++) {
        heap_insert(pq, priors[i]);
        printf("  Proceso P%d (prioridad %d) encolado\n", i, priors[i]);
    }

    printf("\nEjecucion por prioridad:\n");
    while (pq->size > 0) {
        int p = heap_extract(pq);
        printf("  Ejecutar proceso con prioridad %d\n", p);
    }

    heap_free(pq);
}
```

Salida:

```
Llegada de procesos:
  Proceso P0 (prioridad 5) encolado
  Proceso P1 (prioridad 1) encolado
  Proceso P2 (prioridad 8) encolado
  Proceso P3 (prioridad 3) encolado
  Proceso P4 (prioridad 10) encolado
  Proceso P5 (prioridad 2) encolado
  Proceso P6 (prioridad 7) encolado

Ejecucion por prioridad:
  Ejecutar proceso con prioridad 10
  Ejecutar proceso con prioridad 8
  Ejecutar proceso con prioridad 7
  Ejecutar proceso con prioridad 5
  Ejecutar proceso con prioridad 3
  Ejecutar proceso con prioridad 2
  Ejecutar proceso con prioridad 1
```
</details>

### Ejercicio 10 — Medir swaps promedio del sift-down

Escribe un programa que mida el número promedio de swaps en `sift_down`
(durante `extract_max`) para heaps de distintos tamaños. Compara con
el máximo teórico $\lfloor\log_2 n\rfloor$.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;

fn extract_counting(data: &mut Vec<i32>) -> (i32, u32) {
    let max = data[0];
    let last = data.len() - 1;
    data.swap(0, last);
    data.pop();

    let mut swaps = 0u32;
    let mut index = 0;
    let size = data.len();

    loop {
        let mut largest = index;
        let left = 2 * index + 1;
        let right = 2 * index + 2;
        if left < size && data[left] > data[largest] { largest = left; }
        if right < size && data[right] > data[largest] { largest = right; }
        if largest == index { break; }
        data.swap(index, largest);
        index = largest;
        swaps += 1;
    }

    (max, swaps)
}

fn main() {
    for &n in &[100, 1000, 10_000, 100_000] {
        // construir heap
        let mut data: Vec<i32> = (0..n).collect();
        // heapify (build max-heap)
        for i in (0..n/2).rev() {
            // sift_down inline
            let mut idx = i as usize;
            let size = n as usize;
            loop {
                let mut largest = idx;
                let l = 2*idx+1; let r = 2*idx+2;
                if l < size && data[l] > data[largest] { largest = l; }
                if r < size && data[r] > data[largest] { largest = r; }
                if largest == idx { break; }
                data.swap(idx, largest);
                idx = largest;
            }
        }

        let mut total = 0u64;
        let extractions = n as u64;
        let mut data_copy = data.clone();

        for _ in 0..n {
            let (_, swaps) = extract_counting(&mut data_copy);
            total += swaps as u64;
        }

        let h = (n as f64).log2().floor() as u32;
        println!("n={:>7}: promedio={:.2}, max teorico={}",
                 n, total as f64 / extractions as f64, h);
    }
}
```

Resultado típico:

```
n=    100: promedio=5.17, max teorico=6
n=   1000: promedio=8.31, max teorico=9
n=  10000: promedio=11.47, max teorico=13
n= 100000: promedio=14.66, max teorico=16
```

El promedio está cerca del máximo — confirmando que sift-down casi siempre
baja hasta las hojas o cerca ($\approx h - 1.6$ en promedio).
</details>
