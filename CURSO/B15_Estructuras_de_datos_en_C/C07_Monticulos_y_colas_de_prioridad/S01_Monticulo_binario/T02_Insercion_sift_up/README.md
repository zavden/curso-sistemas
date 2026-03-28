# Inserción en heap (sift-up)

## Concepto

Insertar un elemento en un heap binario requiere mantener **ambas** propiedades:

1. **Forma**: el árbol debe seguir siendo completo.
2. **Orden**: la propiedad de heap debe cumplirse en todos los nodos.

La estrategia es simple y elegante:

1. **Agregar** el nuevo elemento al **final** del array (primera posición libre).
   Esto preserva la propiedad de forma — el árbol sigue siendo completo.
2. **Subir** el elemento mientras viole la propiedad de heap (si es mayor que su
   padre en un max-heap, o menor en un min-heap). Esta operación se llama
   **sift-up** (también *bubble-up*, *percolate-up*, *swim*, *heapify-up*).

---

## Sift-up: algoritmo

Para un **max-heap**:

```
sift_up(heap, index):
    while index > 0:
        parent = (index - 1) / 2
        if heap[parent] >= heap[index]:
            break          // propiedad restaurada
        swap(heap[parent], heap[index])
        index = parent     // subir un nivel
```

El elemento "burbujea" hacia arriba intercambiándose con su padre hasta que
encuentra un padre mayor (o llega a la raíz).

---

## Traza detallada

Insertar **85** en el max-heap `[90, 70, 80, 50, 60, 40, 30]`:

### Paso 1: agregar al final

```
Array: [90, 70, 80, 50, 60, 40, 30, 85]
                                       ^^ nuevo (indice 7)

           90
          /  \
        70    80
       / \   / \
     50  60 40  30
    /
   85    ← agregado aqui (primera posicion libre)
```

Propiedad de forma: ✓ (sigue completo).
Propiedad de heap: ✗ (85 > 50, su padre).

### Paso 2: sift-up — comparar con padre

$i = 7$, padre $= (7-1)/2 = 3$, `heap[3] = 50`.
$85 > 50$ → **swap**.

```
Array: [90, 70, 80, 85, 60, 40, 30, 50]

           90
          /  \
        70    80
       / \   / \
     85  60 40  30
    /
   50
```

### Paso 3: seguir subiendo

$i = 3$, padre $= (3-1)/2 = 1$, `heap[1] = 70`.
$85 > 70$ → **swap**.

```
Array: [90, 85, 80, 70, 60, 40, 30, 50]

           90
          /  \
        85    80
       / \   / \
     70  60 40  30
    /
   50
```

### Paso 4: seguir subiendo

$i = 1$, padre $= (1-1)/2 = 0$, `heap[0] = 90`.
$85 < 90$ → **stop**. Propiedad restaurada.

```
Array final: [90, 85, 80, 70, 60, 40, 30, 50]
```

### Resumen del camino

```
85 insertado en posicion 7 (hijo de 50)
   swap con 50 → posicion 3
   swap con 70 → posicion 1
   stop (85 < 90)

Camino: 7 → 3 → 1 (3 comparaciones, 2 swaps)
```

---

## Traza: insertar en la raíz

Insertar **100** en `[90, 85, 80, 70, 60, 40, 30, 50]`:

| Paso | $i$ | Padre | Comparación | Acción |
|------|-----|-------|-------------|--------|
| 1 | 8 | 3 (70) | 100 > 70 | swap |
| 2 | 3 | 1 (85) | 100 > 85 | swap |
| 3 | 1 | 0 (90) | 100 > 90 | swap |
| 4 | 0 | — | Es raíz | stop |

```
Array final: [100, 90, 80, 85, 60, 40, 30, 50, 70]

          100
          /  \
        90    80
       / \   / \
     85  60 40  30
    / \
   50  70
```

El 100 subió hasta la raíz — recorrió todo el camino desde la hoja hasta
la cima. Este es el **peor caso**: $\lfloor \log_2 n \rfloor$ swaps.

---

## Traza: elemento ya en su lugar

Insertar **10** en `[90, 85, 80, 70, 60, 40, 30, 50]`:

| Paso | $i$ | Padre | Comparación | Acción |
|------|-----|-------|-------------|--------|
| 1 | 8 | 3 (70) | 10 < 70 | stop |

El 10 se queda donde fue insertado. **Mejor caso**: 1 comparación, 0 swaps.

---

## Implementación en C

```c
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void sift_up(int heap[], int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent] >= heap[index]) break;
        swap(&heap[parent], &heap[index]);
        index = parent;
    }
}

void heap_insert(int heap[], int *size, int value) {
    heap[*size] = value;
    sift_up(heap, *size);
    (*size)++;
}
```

Notas:
- `heap` es un array pre-alocado con capacidad suficiente.
- `size` es un puntero al número actual de elementos — se incrementa después
  del sift-up.
- El valor se coloca al final **antes** de sift-up, que lo mueve a su posición
  correcta.

### Con array dinámico

En la práctica, necesitamos un heap que crezca dinámicamente:

```c
typedef struct {
    int *data;
    int size;
    int capacity;
} MaxHeap;

MaxHeap *heap_create(int initial_cap) {
    MaxHeap *h = malloc(sizeof(MaxHeap));
    h->data = malloc(initial_cap * sizeof(int));
    h->size = 0;
    h->capacity = initial_cap;
    return h;
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

int heap_peek(MaxHeap *h) {
    return h->data[0];
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
            if self.data[parent] >= self.data[index] {
                break;
            }
            self.data.swap(parent, index);
            index = parent;
        }
    }

    fn peek(&self) -> Option<&i32> {
        self.data.first()
    }
}
```

`Vec::push` se encarga del crecimiento dinámico. `slice::swap` hace el
intercambio sin necesidad de variable temporal.

### Para min-heap

La única diferencia es la dirección de la comparación:

```rust
fn sift_up_min(&mut self, mut index: usize) {
    while index > 0 {
        let parent = (index - 1) / 2;
        if self.data[parent] <= self.data[index] {  // <= en vez de >=
            break;
        }
        self.data.swap(parent, index);
        index = parent;
    }
}
```

---

## Complejidad

| Caso | Comparaciones | Swaps | Complejidad |
|------|--------------|-------|-------------|
| Mejor | 1 | 0 | $O(1)$ |
| Peor | $\lfloor \log_2 n \rfloor$ | $\lfloor \log_2 n \rfloor$ | $O(\log n)$ |
| Promedio | $\approx 1.6$ | $\approx 1.6$ | $O(1)$ amortizado |

El caso promedio es sorprendente: en la **mayoría** de las inserciones, el
elemento sube muy pocos niveles. Esto es porque la mitad de los nodos son hojas,
y un elemento aleatorio tiene probabilidad $\frac{1}{2}$ de ser menor que un
nodo aleatorio de la mitad inferior.

Más precisamente: la probabilidad de subir $k$ niveles decrece geométricamente.
El valor esperado del número de swaps es:

$$E[\text{swaps}] = \sum_{k=1}^{h} k \cdot P(\text{subir } k) \approx \sum_{k=1}^{h} \frac{k}{2^k} \leq 2$$

Es decir, en promedio cada inserción hace menos de 2 swaps, independientemente
del tamaño del heap. Sin embargo, la complejidad del **peor caso** sigue siendo
$O(\log n)$, y esto es lo que se reporta formalmente.

---

## Construir un heap insertando uno por uno

Si insertamos $n$ elementos secuencialmente:

```c
for (int i = 0; i < n; i++) {
    heap_insert(heap, &size, arr[i]);
}
```

Cada inserción cuesta $O(\log n)$ en el peor caso, dando un total de
$O(n \log n)$. Este método se llama construcción **top-down** y **no** es
óptimo — en T04 veremos el método bottom-up que logra $O(n)$.

### Traza: construir heap insertando 4, 1, 7, 5, 3

| Paso | Insertar | Array resultante | Sift-up |
|------|----------|-----------------|---------|
| 1 | 4 | `[4]` | (raíz, no sube) |
| 2 | 1 | `[4, 1]` | 1 < 4, stop |
| 3 | 7 | `[4, 1, 7]` → sift-up → `[7, 1, 4]` | 7 > 4 (padre), swap |
| 4 | 5 | `[7, 1, 4, 5]` → sift-up → `[7, 5, 4, 1]` | 5 > 1, swap; 5 < 7, stop |
| 5 | 3 | `[7, 5, 4, 1, 3]` → sift-up → `[7, 5, 4, 1, 3]` | 3 < 5, stop |

Estado final:

```
       7
      / \
     5   4
    / \
   1   3
```

Verificación: 7 ≥ 5 ✓, 7 ≥ 4 ✓, 5 ≥ 1 ✓, 5 ≥ 3 ✓. Max-heap válido ✓.

---

## Sift-up con traza impresa

Para depuración, una versión que imprime cada paso:

```c
void sift_up_verbose(int heap[], int index) {
    printf("  sift_up(%d) desde indice %d\n", heap[index], index);
    int swaps = 0;

    while (index > 0) {
        int parent = (index - 1) / 2;
        printf("    comparar %d (i=%d) con padre %d (i=%d): ",
               heap[index], index, heap[parent], parent);

        if (heap[parent] >= heap[index]) {
            printf("padre mayor, STOP\n");
            break;
        }

        printf("SWAP\n");
        swap(&heap[parent], &heap[index]);
        index = parent;
        swaps++;
    }

    if (index == 0) printf("    llego a la raiz\n");
    printf("  resultado: %d swaps\n\n", swaps);
}
```

---

## Invariante del sift-up

Antes de cada iteración del bucle, el array cumple la propiedad de heap
**excepto** posiblemente entre `heap[index]` y `heap[parent(index)]`. Es decir,
la violación está localizada en un solo par padre-hijo.

**Prueba de correctitud**:

- **Inicio**: el nuevo elemento fue colocado al final. El único par que puede
  violar la propiedad es (padre del último, último).
- **Mantenimiento**: después del swap, la violación se mueve un nivel arriba.
  Los hijos del nodo que "bajó" siguen satisfechos (el nodo que bajó era el padre
  y era ≥ que sus hijos originales; el nodo que subió es ≥ que el que bajó, así que
  también es ≥ que los hijos originales del que bajó).
- **Terminación**: el bucle termina cuando `heap[parent] >= heap[index]` (propiedad
  restaurada) o cuando `index == 0` (la raíz no tiene padre, propiedad trivial).

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

void heap_insert(MaxHeap *h, int value) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(int));
    }
    h->data[h->size] = value;
    sift_up(h->data, h->size);
    h->size++;
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

void sift_up_verbose(int heap[], int index) {
    int value = heap[index];
    printf("  sift_up(%d) desde indice %d\n", value, index);
    int swaps = 0;
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent] >= heap[index]) {
            printf("    %d < %d (padre), STOP\n", heap[index], heap[parent]);
            break;
        }
        printf("    %d > %d (padre), SWAP\n", heap[index], heap[parent]);
        swap(&heap[parent], &heap[index]);
        index = parent;
        swaps++;
    }
    if (index == 0 && swaps > 0) printf("    llego a la raiz\n");
    printf("  swaps: %d, array: ", swaps);
}

int main(void) {
    // 1. Construccion paso a paso
    printf("=== Construccion insertando uno a uno ===\n\n");

    int values[] = {40, 10, 70, 50, 30, 85, 20, 60, 100};
    int n = sizeof(values) / sizeof(values[0]);

    MaxHeap *h = heap_create(16);

    for (int i = 0; i < n; i++) {
        printf("Insertar %d:\n", values[i]);
        h->data[h->size] = values[i];
        sift_up_verbose(h->data, h->size);
        sift_up(h->data, h->size);
        h->size++;

        // reimprimir despues del sift_up real
        print_array(h->data, h->size);
        printf("\n\n");
    }

    printf("Heap final:\n");
    print_levels(h->data, h->size);
    printf("\nis_max_heap: %s\n", is_max_heap(h->data, h->size) ? "SI" : "NO");

    // 2. Insercion en heap existente
    printf("\n=== Insertar 95 en el heap ===\n\n");
    printf("Antes:  ");
    print_array(h->data, h->size);
    printf("\n");

    heap_insert(h, 95);

    printf("Despues: ");
    print_array(h->data, h->size);
    printf("\n\nNiveles:\n");
    print_levels(h->data, h->size);

    // 3. Mejor caso: insertar valor pequeno
    printf("\n=== Insertar 1 (mejor caso) ===\n\n");
    int before = h->size;
    heap_insert(h, 1);
    printf("Array: ");
    print_array(h->data, h->size);
    printf("\n1 quedo en posicion %d (hoja), 0 swaps\n", h->size - 1);

    // 4. Estadisticas
    printf("\n=== Estadisticas ===\n\n");
    printf("Tamano final: %d\n", h->size);
    printf("Altura:       %d\n", 31 - __builtin_clz(h->size));
    printf("Maximo:       %d (siempre data[0])\n", h->data[0]);

    heap_free(h);
    return 0;
}
```

### Salida esperada

```
=== Construccion insertando uno a uno ===

Insertar 40:
  sift_up(40) desde indice 0
  swaps: 0, array: [40]

Insertar 10:
  sift_up(10) desde indice 1
    10 < 40 (padre), STOP
  swaps: 0, array: [40, 10]

Insertar 70:
  sift_up(70) desde indice 2
    70 > 40 (padre), SWAP
    llego a la raiz
  swaps: 1, array: [70, 10, 40]

Insertar 50:
  sift_up(50) desde indice 3
    50 > 10 (padre), SWAP
    50 < 70 (padre), STOP
  swaps: 1, array: [70, 50, 40, 10]

Insertar 30:
  sift_up(30) desde indice 4
    30 < 50 (padre), STOP
  swaps: 0, array: [70, 50, 40, 10, 30]

Insertar 85:
  sift_up(85) desde indice 5
    85 > 40 (padre), SWAP
    85 > 70 (padre), SWAP
    llego a la raiz
  swaps: 2, array: [85, 50, 70, 10, 30, 40]

Insertar 20:
  sift_up(20) desde indice 6
    20 < 70 (padre), STOP
  swaps: 0, array: [85, 50, 70, 10, 30, 40, 20]

Insertar 60:
  sift_up(60) desde indice 7
    60 > 10 (padre), SWAP
    60 > 50 (padre), SWAP
    60 < 85 (padre), STOP
  swaps: 2, array: [85, 60, 70, 50, 30, 40, 20, 10]

Insertar 100:
  sift_up(100) desde indice 8
    100 > 50 (padre), SWAP
    100 > 60 (padre), SWAP
    100 > 85 (padre), SWAP
    llego a la raiz
  swaps: 3, array: [100, 85, 70, 60, 30, 40, 20, 10, 50]

Heap final:
  Nivel 0: 100
  Nivel 1: 85 70
  Nivel 2: 60 30 40 20
  Nivel 3: 10 50

is_max_heap: SI

=== Insertar 95 en el heap ===

Antes:  [100, 85, 70, 60, 30, 40, 20, 10, 50]
Despues: [100, 95, 70, 60, 85, 40, 20, 10, 50, 30]

Niveles:
  Nivel 0: 100
  Nivel 1: 95 70
  Nivel 2: 60 85 40 20
  Nivel 3: 10 50 30

=== Insertar 1 (mejor caso) ===

Array: [100, 95, 70, 60, 85, 40, 20, 10, 50, 30, 1]
1 quedo en posicion 10 (hoja), 0 swaps

=== Estadisticas ===

Tamano final: 11
Altura:       3
Maximo:       100 (siempre data[0])
```

---

## Programa completo en Rust

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

    fn insert_verbose(&mut self, value: i32) {
        self.data.push(value);
        let mut index = self.data.len() - 1;
        println!("  sift_up({value}) desde indice {index}");
        let mut swaps = 0;

        while index > 0 {
            let parent = (index - 1) / 2;
            if self.data[parent] >= self.data[index] {
                println!("    {} < {} (padre), STOP",
                         self.data[index], self.data[parent]);
                break;
            }
            println!("    {} > {} (padre), SWAP",
                     self.data[index], self.data[parent]);
            self.data.swap(parent, index);
            index = parent;
            swaps += 1;
        }
        if index == 0 && swaps > 0 { println!("    llego a la raiz"); }
        println!("  swaps: {swaps}, array: {:?}\n", self.data);
    }

    fn peek(&self) -> Option<i32> {
        self.data.first().copied()
    }

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
    let values = [40, 10, 70, 50, 30, 85, 20, 60, 100];

    println!("=== Construccion paso a paso ===\n");
    for &v in &values {
        print!("Insertar {v}:\n");
        heap.insert_verbose(v);
    }

    println!("Heap final:");
    heap.print_levels();
    println!("\nis_valid: {}", heap.is_valid());
    println!("peek: {:?}", heap.peek());
    println!("size: {}", heap.data.len());

    // Min-heap
    println!("\n=== Min-heap ===\n");
    let mut min_data: Vec<i32> = Vec::new();
    for &v in &[40, 10, 70, 50, 30] {
        min_data.push(v);
        let mut idx = min_data.len() - 1;
        while idx > 0 {
            let p = (idx - 1) / 2;
            if min_data[p] <= min_data[idx] { break; }
            min_data.swap(p, idx);
            idx = p;
        }
        println!("Insertar {v}: {:?}", min_data);
    }
}
```

### Salida

```
=== Construccion paso a paso ===

Insertar 40:
  sift_up(40) desde indice 0
  swaps: 0, array: [40]

Insertar 10:
  sift_up(10) desde indice 1
    10 < 40 (padre), STOP
  swaps: 0, array: [40, 10]

Insertar 70:
  sift_up(70) desde indice 2
    70 > 40 (padre), SWAP
    llego a la raiz
  swaps: 1, array: [70, 10, 40]

Insertar 50:
  sift_up(50) desde indice 3
    50 > 10 (padre), SWAP
    50 < 70 (padre), STOP
  swaps: 1, array: [70, 50, 40, 10]

Insertar 30:
  sift_up(30) desde indice 4
    30 < 50 (padre), STOP
  swaps: 0, array: [70, 50, 40, 10, 30]

Insertar 85:
  sift_up(85) desde indice 5
    85 > 40 (padre), SWAP
    85 > 70 (padre), SWAP
    llego a la raiz
  swaps: 2, array: [85, 50, 70, 10, 30, 40]

Insertar 20:
  sift_up(20) desde indice 6
    20 < 70 (padre), STOP
  swaps: 0, array: [85, 50, 70, 10, 30, 40, 20]

Insertar 60:
  sift_up(60) desde indice 7
    60 > 10 (padre), SWAP
    60 > 50 (padre), SWAP
    60 < 85 (padre), STOP
  swaps: 2, array: [85, 60, 70, 50, 30, 40, 20, 10]

Insertar 100:
  sift_up(100) desde indice 8
    100 > 50 (padre), SWAP
    100 > 60 (padre), SWAP
    100 > 85 (padre), SWAP
    llego a la raiz
  swaps: 3, array: [100, 85, 70, 60, 30, 40, 20, 10, 50]

Heap final:
  Nivel 0: 100
  Nivel 1: 85 70
  Nivel 2: 60 30 40 20
  Nivel 3: 10 50

is_valid: true
peek: Some(100)
size: 9

=== Min-heap ===

Insertar 40: [40]
Insertar 10: [10, 40]
Insertar 70: [10, 40, 70]
Insertar 50: [10, 40, 70, 50]
Insertar 30: [10, 30, 70, 50, 40]
```

---

## Ejercicios

### Ejercicio 1 — Trazar inserción

Partiendo del max-heap vacío, inserta los valores 20, 15, 30, 10, 25, 5, 35
uno por uno. Muestra el array después de cada inserción.

<details>
<summary>Verificar</summary>

| Insertar | Sift-up | Array resultante |
|----------|---------|-----------------|
| 20 | raíz | `[20]` |
| 15 | 15 < 20, stop | `[20, 15]` |
| 30 | 30 > 20, swap | `[30, 15, 20]` |
| 10 | 10 < 15, stop | `[30, 15, 20, 10]` |
| 25 | 25 > 15, swap; 25 < 30, stop | `[30, 25, 20, 10, 15]` |
| 5 | 5 < 20, stop | `[30, 25, 20, 10, 15, 5]` |
| 35 | 35 > 20, swap; 35 > 30, swap | `[35, 25, 30, 10, 15, 5, 20]` |

Heap final:

```
         35
        /  \
      25    30
     / \   / \
   10  15 5  20
```
</details>

### Ejercicio 2 — Min-heap

Partiendo del min-heap vacío, inserta 50, 30, 40, 10, 20, 35. Muestra el
array después de cada inserción.

<details>
<summary>Verificar</summary>

| Insertar | Sift-up (min) | Array resultante |
|----------|--------------|-----------------|
| 50 | raíz | `[50]` |
| 30 | 30 < 50, swap | `[30, 50]` |
| 40 | 40 > 30, stop | `[30, 50, 40]` |
| 10 | 10 < 50, swap; 10 < 30, swap | `[10, 30, 40, 50]` |
| 20 | 20 < 30, swap; 20 > 10, stop | `[10, 20, 40, 50, 30]` |
| 35 | 35 < 40, swap; 35 > 10, stop | `[10, 20, 35, 50, 30, 40]` |

```
         10
        /  \
      20    35
     / \   /
   50  30 40
```
</details>

### Ejercicio 3 — Contar swaps

Para la secuencia 1, 2, 3, 4, 5, 6, 7 insertada en un max-heap vacío,
¿cuántos swaps totales se realizan?

<details>
<summary>Verificar</summary>

| Insertar | Swaps | Detalle |
|----------|-------|---------|
| 1 | 0 | raíz |
| 2 | 1 | 2 > 1 |
| 3 | 1 | 3 > 2 |
| 4 | 2 | 4 > 1, 4 > 3 → no, veamos: `[3,1,2]` → insert 4: pos 3, padre=1(val 1), 4>1 swap → `[3,4,2,1]`, padre=0(val 3), 4>3 swap → `[4,3,2,1]`. 2 swaps. |
| 5 | 1 | pos 4, padre=1(3), 5>3 swap → `[4,5,2,1,3]`, padre=0(4), 5>4 swap. 2 swaps. |
| 6 | 2 | pos 5, padre=2(2), 6>2 swap, padre=0(5), 6>5 swap. 2 swaps. |
| 7 | 2 | pos 6, padre=2(val depende), sube hasta raíz. |

Traza exacta:

```
[1] → 0 swaps
[2, 1] → 1 swap (2>1)
[3, 1, 2] → 1 swap (3>2→raíz... wait: insert 3 at pos 2, parent=0(val 2), 3>2 swap) → [3, 1, 2]
[3, 1, 2, 4] → 4>1 swap [3,4,2,1], 4>3 swap [4,3,2,1] → 2 swaps
[4, 3, 2, 1, 5] → 5>3 swap [4,5,2,1,3], 5>4 swap [5,4,2,1,3] → 2 swaps
[5, 4, 2, 1, 3, 6] → 6>2 swap [5,4,6,1,3,2], 6>5 swap [6,4,5,1,3,2] → 2 swaps
[6, 4, 5, 1, 3, 2, 7] → 7>5 swap [6,4,7,1,3,2,5], 7>6 swap [7,4,6,1,3,2,5] → 2 swaps
```

Total: $0 + 1 + 1 + 2 + 2 + 2 + 2 = $ **10 swaps**.

Patrón: insertar una secuencia ascendente es el **peor caso** — cada elemento
sube hasta la raíz.
</details>

### Ejercicio 4 — Secuencia descendente

¿Cuántos swaps se realizan al insertar 7, 6, 5, 4, 3, 2, 1 en un max-heap
vacío? ¿Por qué?

<details>
<summary>Verificar</summary>

| Insertar | Swaps | Razón |
|----------|-------|-------|
| 7 | 0 | raíz |
| 6 | 0 | 6 < 7 |
| 5 | 0 | 5 < 7 |
| 4 | 0 | 4 < 6 |
| 3 | 0 | 3 < 6 |
| 2 | 0 | 2 < 5 |
| 1 | 0 | 1 < 5 |

Total: **0 swaps**.

Razón: una secuencia descendente ya satisface la propiedad de max-heap al
insertarla secuencialmente — cada nuevo elemento es menor que todos los
anteriores, así que nunca necesita subir. Este es el **mejor caso**.
</details>

### Ejercicio 5 — Demostrar que sift-up es correcto

Demuestra que después de `sift_up`, el array completo satisface la propiedad
de max-heap. Usa el invariante del bucle.

<details>
<summary>Verificar</summary>

**Invariante**: al inicio de cada iteración, la propiedad de max-heap se cumple
en todo el array excepto posiblemente entre `heap[index]` y `heap[parent(index)]`.

**Inicialización**: antes del bucle, el array era un heap válido de tamaño $n-1$.
El nuevo elemento en posición $n-1$ puede violar la propiedad solo con su padre.
Invariante cumplido.

**Mantenimiento**: si `heap[index] > heap[parent]`, hacemos swap. Después del swap:
- Los antiguos hijos de `parent` (que ahora están bajo `index`, porque el valor de
  `parent` bajó a `index`) siguen satisfechos: el valor que bajó era `parent` original,
  que era ≥ que ellos; y el nuevo `index` (ex-parent) también era ≥ que ellos.
- La nueva posible violación es entre el nuevo `heap[parent]` (que es el elemento
  insertado) y `heap[parent(parent)]`. Invariante mantenido, un nivel arriba.

**Terminación**: el bucle termina cuando `heap[parent] >= heap[index]` (no hay
violación) o `index == 0` (la raíz no tiene padre). En ambos casos, la única
posible violación se ha resuelto.

$\therefore$ Al terminar, todo el array satisface la propiedad de max-heap. $\square$
</details>

### Ejercicio 6 — Sift-up para min-heap

Implementa `sift_up` y `heap_insert` para un **min-heap** en C. Inserta
50, 30, 40, 10, 20 y verifica.

<details>
<summary>Verificar</summary>

```c
void sift_up_min(int heap[], int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent] <= heap[index]) break;
        swap(&heap[parent], &heap[index]);
        index = parent;
    }
}

void min_heap_insert(int heap[], int *size, int value) {
    heap[*size] = value;
    sift_up_min(heap, *size);
    (*size)++;
}
```

Traza:
- 50 → `[50]`
- 30 → 30 < 50, swap → `[30, 50]`
- 40 → 40 > 30, stop → `[30, 50, 40]`
- 10 → 10 < 50, swap; 10 < 30, swap → `[10, 30, 40, 50]`
- 20 → 20 < 30, swap; 20 > 10, stop → `[10, 20, 40, 50, 30]`

```
       10
      /  \
    20    40
   / \
  50  30
```

Min-heap válido ✓.
</details>

### Ejercicio 7 — Inserción con comparador genérico

Implementa en Rust un heap genérico que acepte un comparador para funcionar
como max-heap o min-heap:

```rust
struct Heap<T, F: Fn(&T, &T) -> bool> {
    data: Vec<T>,
    should_swap: F,  // true si parent y child deben swapearse
}
```

<details>
<summary>Verificar</summary>

```rust
struct Heap<T, F: Fn(&T, &T) -> bool> {
    data: Vec<T>,
    should_swap: F,
}

impl<T, F: Fn(&T, &T) -> bool> Heap<T, F> {
    fn new(should_swap: F) -> Self {
        Heap { data: Vec::new(), should_swap }
    }

    fn insert(&mut self, value: T) {
        self.data.push(value);
        self.sift_up(self.data.len() - 1);
    }

    fn sift_up(&mut self, mut index: usize) {
        while index > 0 {
            let parent = (index - 1) / 2;
            if !(self.should_swap)(&self.data[parent], &self.data[index]) {
                break;
            }
            self.data.swap(parent, index);
            index = parent;
        }
    }

    fn peek(&self) -> Option<&T> {
        self.data.first()
    }
}

// Uso:
let mut max_heap = Heap::new(|p: &i32, c: &i32| p < c);  // swap if parent < child
max_heap.insert(30);
max_heap.insert(50);
max_heap.insert(10);
assert_eq!(max_heap.peek(), Some(&50));

let mut min_heap = Heap::new(|p: &i32, c: &i32| p > c);  // swap if parent > child
min_heap.insert(30);
min_heap.insert(50);
min_heap.insert(10);
assert_eq!(min_heap.peek(), Some(&10));
```

El comparador `should_swap(parent, child)` retorna `true` cuando la propiedad
de heap está violada y se necesita swap. Para max-heap: violar = padre < hijo.
Para min-heap: violar = padre > hijo.
</details>

### Ejercicio 8 — Costo amortizado

Si insertamos $n$ elementos aleatorios en un max-heap, ¿cuántos swaps totales
esperamos? Escribe un programa que lo mida empíricamente para $n = 10^5$.

<details>
<summary>Verificar</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand(time(NULL));
    int n = 100000;
    int *heap = malloc(n * sizeof(int));
    long total_swaps = 0;

    for (int s = 0; s < n; s++) {
        heap[s] = rand();
        int index = s;
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (heap[parent] >= heap[index]) break;
            int tmp = heap[parent];
            heap[parent] = heap[index];
            heap[index] = tmp;
            index = parent;
            total_swaps++;
        }
    }

    printf("n = %d\n", n);
    printf("Total swaps: %ld\n", total_swaps);
    printf("Promedio swaps/insercion: %.2f\n", (double)total_swaps / n);

    free(heap);
    return 0;
}
```

Resultado típico: promedio **≈ 1.6 swaps/inserción** para datos aleatorios,
confirmando que el caso promedio es $O(1)$, muy lejos del peor caso
$O(\log n) \approx 17$ para $n = 10^5$.
</details>

### Ejercicio 9 — Insertar duplicados

¿Qué sucede al insertar valores duplicados en un max-heap? Inserta 5, 5, 5
en un heap vacío. ¿Es válido? ¿Dónde terminan?

<details>
<summary>Verificar</summary>

- Insertar 5: `[5]` (raíz).
- Insertar 5: pos 1, padre=0(5). $5 \geq 5$, stop. `[5, 5]`.
- Insertar 5: pos 2, padre=0(5). $5 \geq 5$, stop. `[5, 5, 5]`.

```
     5
    / \
   5   5
```

**Sí**, es un max-heap válido. La propiedad es $\geq$ (no estrictamente $>$),
así que los duplicados se permiten. El sift-up se detiene en `>=`, por lo que
los duplicados no suben más allá de un padre con el mismo valor.

Los heaps manejan duplicados naturalmente, a diferencia de los BST donde
hay que decidir una estrategia.
</details>

### Ejercicio 10 — Visualizar el camino de sift-up

Implementa una función que, dado un heap y un índice, imprima el camino
completo desde ese índice hasta la raíz (todos los ancestros). Úsala para
mostrar el camino recorrido durante una inserción.

<details>
<summary>Verificar</summary>

```c
void print_path_to_root(int heap[], int index) {
    printf("Camino desde indice %d hasta raiz: ", index);
    while (index >= 0) {
        printf("%d", heap[index]);
        if (index == 0) break;
        printf(" -> ");
        index = (index - 1) / 2;
    }
    printf("\n");
}
```

En Rust:

```rust
fn path_to_root(data: &[i32], mut index: usize) -> Vec<i32> {
    let mut path = Vec::new();
    loop {
        path.push(data[index]);
        if index == 0 { break; }
        index = (index - 1) / 2;
    }
    path
}
```

Para el heap `[100, 85, 70, 60, 30, 40, 20, 10, 50]`:

`print_path_to_root(heap, 8)`:
- Índice 8 (50) → padre 3 (60) → padre 1 (85) → padre 0 (100)
- Salida: `50 -> 60 -> 85 -> 100`

El sift-up recorre exactamente este camino, comparando en cada paso.
Longitud del camino = $\lfloor \log_2(8) \rfloor + 1 = 4$ nodos.
</details>
