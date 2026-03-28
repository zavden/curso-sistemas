# Definición de montículo binario

## Concepto

Un **montículo binario** (*binary heap*) es un árbol binario que cumple
simultáneamente dos propiedades:

1. **Propiedad de forma**: es un árbol binario **completo** — todos los niveles
   están llenos excepto posiblemente el último, que se llena de izquierda a
   derecha.
2. **Propiedad de heap**: cada nodo satisface una relación de orden con respecto
   a sus hijos.

La propiedad de heap tiene dos variantes:

- **Max-heap**: cada nodo es **mayor o igual** que sus hijos.
  $\forall$ nodo $v$: $v.\text{key} \geq v.\text{children}.\text{key}$
- **Min-heap**: cada nodo es **menor o igual** que sus hijos.
  $\forall$ nodo $v$: $v.\text{key} \leq v.\text{children}.\text{key}$

Como consecuencia, la raíz de un max-heap contiene el **máximo** del conjunto,
y la raíz de un min-heap contiene el **mínimo**. Esto es lo que hace al heap
ideal para implementar **colas de prioridad**: acceder al elemento extremo en
$O(1)$.

### Ejemplo: max-heap

```
           90
          /  \
        70    80
       / \   / \
     50  60 40  30
    / \
   10  20
```

Verificación:
- 90 ≥ 70, 90 ≥ 80 ✓
- 70 ≥ 50, 70 ≥ 60 ✓
- 80 ≥ 40, 80 ≥ 30 ✓
- 50 ≥ 10, 50 ≥ 20 ✓
- 60, 40, 30, 10, 20 son hojas ✓

### Ejemplo: min-heap

```
           5
          / \
        10   15
       / \   /
     20  25 30
```

Verificación: cada padre ≤ hijos ✓. La raíz (5) es el mínimo.

---

## Diferencias con un BST

Es fundamental no confundir un heap con un BST. Aunque ambos son árboles
binarios, sus invariantes son completamente distintas:

| Aspecto | BST | Heap |
|---------|-----|------|
| Invariante | left < node < right | parent ≥ children (max) o parent ≤ children (min) |
| Relación entre hermanos | Izquierdo < derecho (implícito) | **Ninguna** — no hay orden entre hermanos |
| Forma | Cualquiera (puede degenerar) | Siempre **completo** |
| Búsqueda de un elemento | $O(\log n)$ | $O(n)$ — no hay guía de búsqueda |
| Acceso al mínimo/máximo | $O(\log n)$ o $O(1)$ con puntero | $O(1)$ — siempre la raíz |
| Inserción | $O(\log n)$ (balanceado) | $O(\log n)$ |
| Eliminación del extremo | $O(\log n)$ | $O(\log n)$ |
| Representación típica | Punteros (nodos enlazados) | **Array** |

La diferencia clave es que en un heap **no hay relación entre hermanos**. En el
max-heap de ejemplo, el hijo izquierdo de 90 es 70 y el derecho es 80; podrían
estar intercambiados y el heap seguiría siendo válido. En un BST, el izquierdo
**debe** ser menor que el derecho.

---

## Árbol binario completo

La propiedad de forma es crucial y merece atención:

Un árbol binario **completo** tiene todos los niveles llenos excepto
posiblemente el último, que se llena **estrictamente de izquierda a derecha**.

Ejemplos válidos:

```
  1       1         1
         / \       / \
        2   3     2   3
                 / \
                4   5
```

Ejemplos **no** válidos (no son completos):

```
    1          1
   / \        / \
  2   3      2   3
   \        /     \
    4      4       5
```

El primero falla porque el nodo 2 tiene hijo derecho pero no izquierdo. El
segundo falla porque el nivel inferior no está lleno de izquierda a derecha
(hay un hueco entre 4 y 5).

### Propiedades de un árbol completo con $n$ nodos

| Propiedad | Valor |
|-----------|-------|
| Altura | $h = \lfloor \log_2 n \rfloor$ |
| Nodos internos | $\lfloor n/2 \rfloor$ |
| Hojas | $\lceil n/2 \rceil$ |
| Nodos en el nivel $k$ | $2^k$ (excepto posiblemente el último) |
| Nodos totales con altura $h$ | Entre $2^h$ y $2^{h+1} - 1$ |

La altura $\lfloor \log_2 n \rfloor$ es la más baja posible para $n$ nodos, y
es la razón por la que las operaciones del heap son $O(\log n)$: siempre suben
o bajan a lo largo de un camino de longitud $\leq h$.

---

## Representación con array implícito

La propiedad de completitud permite una representación **sin punteros**: los
nodos se almacenan en un array nivel por nivel, de izquierda a derecha.

```
           90                 Indice (base 0): 0  1  2  3  4  5  6  7  8
          /  \                Array:          [90 70 80 50 60 40 30 10 20]
        70    80
       / \   / \
     50  60 40  30
    / \
   10  20
```

### Fórmulas de navegación (base 0)

Para un nodo en el índice $i$:

| Relación | Fórmula | Ejemplo ($i = 1$, nodo 70) |
|----------|---------|--------------------------|
| Padre | $\lfloor (i - 1) / 2 \rfloor$ | $(1-1)/2 = 0$ → nodo 90 ✓ |
| Hijo izquierdo | $2i + 1$ | $2(1)+1 = 3$ → nodo 50 ✓ |
| Hijo derecho | $2i + 2$ | $2(1)+2 = 4$ → nodo 60 ✓ |
| ¿Tiene hijo izq? | $2i + 1 < n$ | $3 < 9$ → sí ✓ |
| ¿Es hoja? | $i \geq \lfloor n/2 \rfloor$ | $1 \geq 4$? → no (es interno) ✓ |

### Fórmulas (base 1)

Algunos textos usan base 1 (índice 0 desperdiciado), lo que simplifica las
fórmulas:

| Relación | Base 0 | Base 1 |
|----------|--------|--------|
| Padre | $(i-1)/2$ | $i/2$ |
| Hijo izquierdo | $2i+1$ | $2i$ |
| Hijo derecho | $2i+2$ | $2i+1$ |

Base 1 permite usar bit shifts: `parent = i >> 1`, `left = i << 1`,
`right = (i << 1) | 1`. En la práctica, la diferencia es mínima y usaremos
**base 0** por ser lo natural en C y Rust.

### Verificación con el ejemplo

```
Array: [90, 70, 80, 50, 60, 40, 30, 10, 20]
        0    1   2   3   4   5   6   7   8
```

| Índice | Valor | Padre | Hijo izq | Hijo der |
|--------|-------|-------|----------|----------|
| 0 | 90 | — (raíz) | 1 (70) | 2 (80) |
| 1 | 70 | 0 (90) | 3 (50) | 4 (60) |
| 2 | 80 | 0 (90) | 5 (40) | 6 (30) |
| 3 | 50 | 1 (70) | 7 (10) | 8 (20) |
| 4 | 60 | 1 (70) | — | — |
| 5 | 40 | 2 (80) | — | — |
| 6 | 30 | 2 (80) | — | — |
| 7 | 10 | 3 (50) | — | — |
| 8 | 20 | 3 (50) | — | — |

### ¿Por qué funciona?

La representación array funciona **solo** porque el árbol es completo. Si
hubiera huecos (nodos faltantes en medio de un nivel), los índices no
corresponderían correctamente a las relaciones padre-hijo.

Comparación con árboles no completos:

```
BST desbalanceado:        Como array (con huecos):
    10                    [10, 5, _, _, 8, _, _]
   /                       0  1  2  3  4  5  6
  5
   \                    Desperdicio: posiciones 2, 3, 5, 6 vacías
    8                   Para n nodos, peor caso: array de 2^n - 1
```

Un BST degenerado de $n$ nodos requeriría un array de $2^n - 1$ posiciones,
de las cuales solo $n$ estarían ocupadas. Para $n = 20$, eso es $\approx 10^6$
posiciones para 20 nodos. Inviable.

El heap, al ser completo, **siempre** usa exactamente $n$ posiciones
consecutivas. No hay desperdicio.

---

## Ventajas de la representación array

| Ventaja | Explicación |
|---------|-------------|
| Sin punteros | Ahorro de 2 punteros por nodo (16 bytes en 64-bit) |
| Localidad de caché | Datos contiguos en memoria → pocas cache misses |
| Navegación aritmética | Padre/hijos con multiplicación y suma, no dereferencia |
| Sin overhead de malloc | Un solo bloque de memoria para todo el heap |
| Serialización trivial | El array **es** la serialización |

La localidad de caché es especialmente importante. En un heap de $10^6$ elementos,
sift-down recorre $\approx 20$ niveles. Con array, los primeros niveles caben en
L1 cache (los 15 nodos del nivel 0-3 son 60 bytes). Con punteros, cada nivel es
una dereferencia potencialmente a una página de memoria distinta.

### Medición del ahorro de memoria

Para $n = 1{,}000{,}000$ enteros:

| Representación | Memoria por nodo | Total |
|---------------|-----------------|-------|
| Array de ints | 4 bytes | 4 MB |
| Nodos con punteros | 4 + 8 + 8 = 20 bytes (alineado a 24) | 24 MB |
| Factor | — | **6×** menos con array |

---

## Verificar la propiedad de heap

```c
int is_max_heap(int arr[], int n) {
    for (int i = 0; i < n / 2; i++) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < n && arr[i] < arr[left]) return 0;
        if (right < n && arr[i] < arr[right]) return 0;
    }
    return 1;
}

int is_min_heap(int arr[], int n) {
    for (int i = 0; i < n / 2; i++) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < n && arr[i] > arr[left]) return 0;
        if (right < n && arr[i] > arr[right]) return 0;
    }
    return 1;
}
```

Solo verificamos nodos internos ($i < n/2$) porque las hojas trivialmente
cumplen la propiedad (no tienen hijos).

```rust
fn is_max_heap(arr: &[i32]) -> bool {
    (0..arr.len() / 2).all(|i| {
        let left = 2 * i + 1;
        let right = 2 * i + 2;
        (left >= arr.len() || arr[i] >= arr[left]) &&
        (right >= arr.len() || arr[i] >= arr[right])
    })
}
```

---

## Visualización del heap

Para depuración, imprimir el array como árbol:

```c
void heap_print(int arr[], int n) {
    if (n == 0) { printf("(vacio)\n"); return; }

    int level = 0;
    int level_size = 1;
    int i = 0;

    while (i < n) {
        // indentacion
        int spaces = (1 << (int)(log2(n) - level + 1)) - 1;
        for (int s = 0; s < spaces; s++) printf(" ");

        // nodos del nivel
        for (int j = 0; j < level_size && i < n; j++, i++) {
            printf("%2d", arr[i]);
            for (int s = 0; s < spaces * 2 + 1; s++) printf(" ");
        }
        printf("\n");
        level++;
        level_size *= 2;
    }
}
```

Forma más simple — imprimir nivel por nivel:

```c
#include <math.h>

void heap_print_levels(int arr[], int n) {
    int i = 0;
    int level = 0;
    while (i < n) {
        int count = 1 << level;  // 2^level nodos en este nivel
        printf("Nivel %d: ", level);
        for (int j = 0; j < count && i < n; j++, i++) {
            printf("%d ", arr[i]);
        }
        printf("\n");
        level++;
    }
}
```

Para el max-heap de ejemplo:

```
Nivel 0: 90
Nivel 1: 70 80
Nivel 2: 50 60 40 30
Nivel 3: 10 20
```

---

## Min-heap vs max-heap

| Aspecto | Min-heap | Max-heap |
|---------|----------|----------|
| Raíz | Mínimo | Máximo |
| Propiedad | padre ≤ hijos | padre ≥ hijos |
| `extract` devuelve | Mínimo | Máximo |
| Uso principal | Cola de prioridad (menor = más urgente) | Heapsort, top-k mayores |
| Stdlib C++ | `std::priority_queue` con `std::greater` | `std::priority_queue` (default) |
| Stdlib Rust | `BinaryHeap` con `Reverse` | `BinaryHeap` (default) |

La implementación es idéntica excepto por la **dirección de la comparación**.
Muchas implementaciones usan un **comparador** configurable para soportar ambas
variantes con el mismo código.

---

## Relación con la cola de prioridad

En C04 implementamos colas de prioridad con listas (ordenadas o no ordenadas).
Comparación:

| Implementación | insert | extract_min | peek |
|---------------|--------|-------------|------|
| Lista no ordenada | $O(1)$ | $O(n)$ | $O(n)$ |
| Lista ordenada | $O(n)$ | $O(1)$ | $O(1)$ |
| **Heap** | $O(\log n)$ | $O(\log n)$ | $O(1)$ |

El heap ofrece el mejor compromiso: ambas operaciones en $O(\log n)$. Ninguna
implementación simple supera al heap en ambas operaciones simultáneamente.

Para $n = 100{,}000$ operaciones mixtas (insert + extract):

| Implementación | Operaciones por segundo (aprox) |
|---------------|-------------------------------|
| Lista no ordenada | $\sim 10^3$ (extract es $O(n)$) |
| Lista ordenada | $\sim 10^3$ (insert es $O(n)$) |
| **Heap** | $\sim 10^6$ (ambas $O(\log n)$) |

---

## ¿Qué no es un heap?

Ejemplos de arrays que **no** son max-heaps:

```
[90, 80, 70, 95, 60]     ← 95 > 80 (hijo > padre), viola propiedad
      90
     /  \
   80    70
  / \
 95  60              95 > 80 ✗
```

```
[90, 80, _, 50, 60]      ← hueco en posición 2, no es completo
```

```
[90, 80, 70, 60]          ← ¿es max-heap?
      90
     /  \
   80    70
  /
 60
Sí: 90≥80, 90≥70, 80≥60 ✓ y es completo ✓
```

---

## Programa completo

```c
#include <stdio.h>

// ======== Navegacion ========

int parent(int i) { return (i - 1) / 2; }
int left_child(int i) { return 2 * i + 1; }
int right_child(int i) { return 2 * i + 2; }

// ======== Verificacion ========

int is_max_heap(int arr[], int n) {
    for (int i = 0; i < n / 2; i++) {
        int l = left_child(i);
        int r = right_child(i);
        if (l < n && arr[i] < arr[l]) return 0;
        if (r < n && arr[i] < arr[r]) return 0;
    }
    return 1;
}

int is_min_heap(int arr[], int n) {
    for (int i = 0; i < n / 2; i++) {
        int l = left_child(i);
        int r = right_child(i);
        if (l < n && arr[i] > arr[l]) return 0;
        if (r < n && arr[i] > arr[r]) return 0;
    }
    return 1;
}

// ======== Impresion ========

void print_array(const char *label, int arr[], int n) {
    printf("%s [", label);
    for (int i = 0; i < n; i++) {
        printf("%d%s", arr[i], i < n - 1 ? ", " : "");
    }
    printf("]\n");
}

void print_levels(int arr[], int n) {
    int i = 0, level = 0;
    while (i < n) {
        int count = 1 << level;
        printf("  Nivel %d: ", level);
        for (int j = 0; j < count && i < n; j++, i++) {
            printf("%d ", arr[i]);
        }
        printf("\n");
        level++;
    }
}

void print_relations(int arr[], int n) {
    printf("  Indice | Valor | Padre     | Hijo izq  | Hijo der\n");
    printf("  -------|-------|-----------|-----------|----------\n");
    for (int i = 0; i < n; i++) {
        printf("    %d    |  %3d  |", i, arr[i]);
        if (i == 0) printf("   (raiz)  |");
        else printf("  %d (%3d)  |", parent(i), arr[parent(i)]);

        int l = left_child(i);
        if (l < n) printf("  %d (%3d)  |", l, arr[l]);
        else printf("     -     |");

        int r = right_child(i);
        if (r < n) printf("  %d (%3d)", r, arr[r]);
        else printf("     -    ");
        printf("\n");
    }
}

int main(void) {
    // Max-heap
    int max_heap[] = {90, 70, 80, 50, 60, 40, 30, 10, 20};
    int n1 = sizeof(max_heap) / sizeof(max_heap[0]);

    printf("=== Max-heap ===\n\n");
    print_array("Array:", max_heap, n1);
    printf("\nNiveles:\n");
    print_levels(max_heap, n1);
    printf("\nRelaciones:\n");
    print_relations(max_heap, n1);
    printf("\nis_max_heap: %s\n", is_max_heap(max_heap, n1) ? "SI" : "NO");
    printf("is_min_heap: %s\n", is_min_heap(max_heap, n1) ? "SI" : "NO");

    // Min-heap
    int min_heap[] = {5, 10, 15, 20, 25, 30};
    int n2 = sizeof(min_heap) / sizeof(min_heap[0]);

    printf("\n=== Min-heap ===\n\n");
    print_array("Array:", min_heap, n2);
    printf("\nNiveles:\n");
    print_levels(min_heap, n2);
    printf("\nis_max_heap: %s\n", is_max_heap(min_heap, n2) ? "SI" : "NO");
    printf("is_min_heap: %s\n", is_min_heap(min_heap, n2) ? "SI" : "NO");

    // No es heap
    int not_heap[] = {90, 80, 70, 95, 60};
    int n3 = sizeof(not_heap) / sizeof(not_heap[0]);

    printf("\n=== No es heap ===\n\n");
    print_array("Array:", not_heap, n3);
    printf("\nNiveles:\n");
    print_levels(not_heap, n3);
    printf("\nis_max_heap: %s\n", is_max_heap(not_heap, n3) ? "SI" : "NO");
    printf("Razon: arr[3]=%d > arr[1]=%d (hijo > padre)\n",
           not_heap[3], not_heap[1]);

    // Propiedades
    printf("\n=== Propiedades del max-heap (n=%d) ===\n\n", n1);
    printf("Altura:          %d (floor(log2(%d)))\n", 3, n1);
    printf("Nodos internos:  %d (floor(%d/2))\n", n1 / 2, n1);
    printf("Hojas:           %d (ceil(%d/2))\n", (n1 + 1) / 2, n1);
    printf("Maximo:          %d (siempre arr[0])\n", max_heap[0]);
    printf("Ultimo nodo:     %d (arr[%d])\n", max_heap[n1-1], n1-1);
    printf("Ultimo interno:  %d (arr[%d])\n", max_heap[n1/2 - 1], n1/2 - 1);

    return 0;
}
```

### Salida esperada

```
=== Max-heap ===

Array: [90, 70, 80, 50, 60, 40, 30, 10, 20]

Niveles:
  Nivel 0: 90
  Nivel 1: 70 80
  Nivel 2: 50 60 40 30
  Nivel 3: 10 20

Relaciones:
  Indice | Valor | Padre     | Hijo izq  | Hijo der
  -------|-------|-----------|-----------|----------
    0    |   90  |   (raiz)  |  1 ( 70)  |  2 ( 80)
    1    |   70  |  0 ( 90)  |  3 ( 50)  |  4 ( 60)
    2    |   80  |  0 ( 90)  |  5 ( 40)  |  6 ( 30)
    3    |   50  |  1 ( 70)  |  7 ( 10)  |  8 ( 20)
    4    |   60  |  1 ( 70)  |     -     |     -
    5    |   40  |  2 ( 80)  |     -     |     -
    6    |   30  |  2 ( 80)  |     -     |     -
    7    |   10  |  3 ( 50)  |     -     |     -
    8    |   20  |  3 ( 50)  |     -     |     -

is_max_heap: SI
is_min_heap: NO

=== Min-heap ===

Array: [5, 10, 15, 20, 25, 30]

Niveles:
  Nivel 0: 5
  Nivel 1: 10 15
  Nivel 2: 20 25 30

is_max_heap: NO
is_min_heap: SI

=== No es heap ===

Array: [90, 80, 70, 95, 60]

Niveles:
  Nivel 0: 90
  Nivel 1: 80 70
  Nivel 2: 95 60

is_max_heap: NO
Razon: arr[3]=95 > arr[1]=80 (hijo > padre)

=== Propiedades del max-heap (n=9) ===

Altura:          3 (floor(log2(9)))
Nodos internos:  4 (floor(9/2))
Hojas:           5 (ceil(9/2))
Maximo:          90 (siempre arr[0])
Ultimo nodo:     20 (arr[8])
Ultimo interno:  50 (arr[3])
```

---

## En Rust

```rust
fn parent(i: usize) -> usize { (i - 1) / 2 }
fn left_child(i: usize) -> usize { 2 * i + 1 }
fn right_child(i: usize) -> usize { 2 * i + 2 }

fn is_max_heap(arr: &[i32]) -> bool {
    (0..arr.len() / 2).all(|i| {
        let l = left_child(i);
        let r = right_child(i);
        (l >= arr.len() || arr[i] >= arr[l]) &&
        (r >= arr.len() || arr[i] >= arr[r])
    })
}

fn is_min_heap(arr: &[i32]) -> bool {
    (0..arr.len() / 2).all(|i| {
        let l = left_child(i);
        let r = right_child(i);
        (l >= arr.len() || arr[i] <= arr[l]) &&
        (r >= arr.len() || arr[i] <= arr[r])
    })
}

fn print_levels(arr: &[i32]) {
    let mut i = 0;
    let mut level = 0;
    while i < arr.len() {
        let count = 1usize << level;
        print!("  Nivel {level}: ");
        for _ in 0..count {
            if i >= arr.len() { break; }
            print!("{} ", arr[i]);
            i += 1;
        }
        println!();
        level += 1;
    }
}

fn main() {
    let max_heap = [90, 70, 80, 50, 60, 40, 30, 10, 20];
    let min_heap = [5, 10, 15, 20, 25, 30];
    let not_heap = [90, 80, 70, 95, 60];

    println!("=== Max-heap ===\n");
    println!("Array: {:?}\n", max_heap);
    print_levels(&max_heap);
    println!("\nis_max_heap: {}", is_max_heap(&max_heap));
    println!("is_min_heap: {}", is_min_heap(&max_heap));

    println!("\n=== Min-heap ===\n");
    println!("Array: {:?}\n", min_heap);
    print_levels(&min_heap);
    println!("\nis_max_heap: {}", is_max_heap(&min_heap));
    println!("is_min_heap: {}", is_min_heap(&min_heap));

    println!("\n=== No es heap ===\n");
    println!("Array: {:?}\n", not_heap);
    print_levels(&not_heap);
    println!("\nis_max_heap: {}", is_max_heap(&not_heap));

    // Navegacion
    println!("\n=== Navegacion (max-heap) ===\n");
    for i in 0..max_heap.len() {
        let p = if i == 0 { "raiz".to_string() }
                else { format!("{} ({})", parent(i), max_heap[parent(i)]) };
        let l = left_child(i);
        let r = right_child(i);
        let ls = if l < max_heap.len() { format!("{} ({})", l, max_heap[l]) }
                 else { "-".to_string() };
        let rs = if r < max_heap.len() { format!("{} ({})", r, max_heap[r]) }
                 else { "-".to_string() };
        println!("  i={i} val={}: padre={p}, izq={ls}, der={rs}", max_heap[i]);
    }
}
```

---

## Ejercicios

### Ejercicio 1 — Identificar heaps

¿Cuáles de los siguientes arrays son max-heaps válidos?

- a) `[100, 50, 80, 30, 40, 70, 60]`
- b) `[100, 80, 50, 60, 70, 30, 40]`
- c) `[50, 40, 30, 20, 10]`
- d) `[10, 20, 30, 40, 50]`

<details>
<summary>Verificar</summary>

**a)** `[100, 50, 80, 30, 40, 70, 60]`

```
       100
      /   \
    50     80
   / \    / \
  30  40 70  60
```

- 100 ≥ 50 ✓, 100 ≥ 80 ✓
- 50 ≥ 30 ✓, 50 ≥ 40 ✓
- 80 ≥ 70 ✓, 80 ≥ 60 ✓

**Sí**, es max-heap.

**b)** `[100, 80, 50, 60, 70, 30, 40]`

```
       100
      /   \
    80     50
   / \    / \
  60  70 30  40
```

- 100 ≥ 80 ✓, 100 ≥ 50 ✓
- 80 ≥ 60 ✓, 80 ≥ 70 ✓
- 50 ≥ 30 ✓, 50 ≥ 40 ✓

**Sí**, es max-heap.

**c)** `[50, 40, 30, 20, 10]` — descendente → **sí**, max-heap.

**d)** `[10, 20, 30, 40, 50]` — ascendente → 10 < 20, **no** es max-heap.
Es un **min-heap** válido.
</details>

### Ejercicio 2 — Calcular índices

Para el array `[90, 70, 80, 50, 60, 40, 30, 10, 20]` (base 0):

1. ¿Cuál es el padre del nodo en índice 7?
2. ¿Cuáles son los hijos del nodo en índice 2?
3. ¿Cuál es el último nodo interno?
4. ¿Cuántas hojas hay?

<details>
<summary>Verificar</summary>

1. Padre de $i=7$: $\lfloor(7-1)/2\rfloor = 3$ → valor 50 ✓
2. Hijos de $i=2$: izq $= 2(2)+1 = 5$ (valor 40), der $= 2(2)+2 = 6$ (valor 30)
3. Último interno: $\lfloor n/2 \rfloor - 1 = \lfloor 9/2 \rfloor - 1 = 3$ → valor 50
4. Hojas: $\lceil 9/2 \rceil = 5$ → nodos en índices 4, 5, 6, 7, 8 (valores 60, 40, 30, 10, 20)
</details>

### Ejercicio 3 — Max-heap con mínimo de nodos

¿Cuál es el max-heap válido más pequeño (menos nodos) que contiene los valores
{1, 2, 3, 4, 5}? ¿Cuántas formas válidas hay de organizar estos 5 valores
como max-heap?

<details>
<summary>Verificar</summary>

La raíz **debe** ser 5 (es el máximo). Después hay opciones:

```
     5            5            5           5
    / \          / \          / \         / \
   4   3        4   2        3   4       2   4
  / \          / \          / \         / \
 2   1        3   1        2   1       3   1
```

Y más: 5 siempre arriba, luego 4 puede estar en posición 1 o 2, etc.

Conteo: la raíz es fija (5). Para posiciones 1 y 2, necesitamos que sean ≥ sus
hijos. Posición 1 tiene hijos en 3, 4. Posición 2 no tiene hijos (5 nodos →
último padre es posición 1).

Formas válidas: **8**.

Razonamiento: raíz=5 fijo. Pos 1 puede ser {4, 3, 2} (debe ser ≥ sus hijos en
pos 3 y 4). Pos 2 es hoja, solo debe ser ≤ 5.
- Pos 1 = 4: pos 2 ∈ {1,2,3}, pos 3,4 del resto. Si pos2=3: pos3,4 = {1,2} (2 formas). Si pos2=2: pos3,4 ∈ {1,3} → 3 debe ir a válido, 2 formas. Si pos2=1: pos3,4 = {2,3}, 2 formas. Total con pos1=4: 6.
- Pos 1 = 3: hijos deben ser ≤ 3 → pos3,4 ∈ {1,2}. Pos2 = 4. 2 formas.
- Pos 1 = 2: hijos ≤ 2 → pos3,4 = {1, x≤2}. Pos2=4, pos 3,4={1,3}... pero 3>2. No válido si pos4=3. Solo pos1=2 funciona si hijos son {1,?≤2} y solo queda 1 y algo... Hay restricciones.

Total exacto: **8** max-heaps válidos.
</details>

### Ejercicio 4 — Min-heap desde max-heap

Dado el max-heap `[90, 70, 80, 50, 60, 40, 30]`, ¿se puede convertir en
min-heap simplemente invirtiendo el array? Verifica.

<details>
<summary>Verificar</summary>

Invertir: `[30, 40, 50, 60, 70, 80, 90]`.

No necesariamente es un min-heap válido. Verificar:

```
       30
      /  \
    40    50
   / \   / \
  60  70 80  90
```

- 30 ≤ 40 ✓, 30 ≤ 50 ✓
- 40 ≤ 60 ✓, 40 ≤ 70 ✓
- 50 ≤ 80 ✓, 50 ≤ 90 ✓

En este caso particular, **sí** es min-heap. Pero esto es coincidencia —
el array original estaba ordenado de forma descendente por niveles.

Contraejemplo: max-heap `[90, 80, 50, 60, 70, 30, 40]`.
Invertir: `[40, 30, 70, 60, 80, 50, 90]`.

```
       40
      /  \
    30    70
   / \   / \
  60  80 50  90
```

30 ≤ 60 ✓, 30 ≤ 80 ✓, pero 70 ≤ 50? **NO**. No es min-heap.

**Conclusión**: invertir un max-heap **no** produce un min-heap en general.
</details>

### Ejercicio 5 — Altura y nodos

Completa la tabla:

| $n$ (nodos) | Altura $h$ | Nodos internos | Hojas |
|-------------|-----------|----------------|-------|
| 1 | ? | ? | ? |
| 7 | ? | ? | ? |
| 10 | ? | ? | ? |
| 15 | ? | ? | ? |
| 1000 | ? | ? | ? |

<details>
<summary>Verificar</summary>

| $n$ | $h = \lfloor\log_2 n\rfloor$ | Internos $= \lfloor n/2\rfloor$ | Hojas $= \lceil n/2\rceil$ |
|-----|------|----------|-------|
| 1 | 0 | 0 | 1 |
| 7 | 2 | 3 | 4 |
| 10 | 3 | 5 | 5 |
| 15 | 3 | 7 | 8 |
| 1000 | 9 | 500 | 500 |

Notar: para $n = 15$ (árbol perfectamente lleno), $h = 3$ y hay $2^3 = 8$
hojas. Para $n = 1000$, $h = 9$ y exactamente la mitad son hojas (porque
$n$ es par).
</details>

### Ejercicio 6 — Base 1 vs base 0

Reescribe las funciones `parent`, `left_child`, `right_child` para base 1
(índice 0 no se usa). Verifica con el array `[_, 90, 70, 80, 50, 60]`
(el `_` es la posición 0 desperdiciada).

<details>
<summary>Verificar</summary>

```c
int parent_b1(int i) { return i / 2; }
int left_b1(int i) { return 2 * i; }
int right_b1(int i) { return 2 * i + 1; }
```

Verificación con `[_, 90, 70, 80, 50, 60]`:

| Índice | Valor | Padre | Hijo izq | Hijo der |
|--------|-------|-------|----------|----------|
| 1 | 90 | 0 (raíz) | 2 (70) | 3 (80) |
| 2 | 70 | 1 (90) | 4 (50) | 5 (60) |
| 3 | 80 | 1 (90) | 6 (—) | 7 (—) |
| 4 | 50 | 2 (70) | 8 (—) | 9 (—) |
| 5 | 60 | 2 (70) | 10 (—) | 11 (—) |

Con bit shifts: `parent = i >> 1`, `left = i << 1`, `right = (i << 1) | 1`.
Más eficiente que base 0, pero desperdicia una posición.
</details>

### Ejercicio 7 — ¿Dónde puede estar el mínimo en un max-heap?

En un max-heap de $n$ elementos, ¿en qué posiciones puede estar el elemento
**mínimo**? ¿Cuántas posiciones posibles hay?

<details>
<summary>Verificar</summary>

El mínimo **siempre** está en una **hoja**. Razonamiento: si un nodo no es hoja,
tiene al menos un hijo, y ese hijo es ≤ que él. Por lo tanto, cualquier nodo
interno es mayor que al menos uno de sus descendientes, y no puede ser el mínimo.

Número de hojas: $\lceil n/2 \rceil$.

Para $n = 9$: hojas en posiciones 4, 5, 6, 7, 8 → **5 posiciones posibles**.

El mínimo puede estar en **cualquiera** de las hojas. No hay forma de saber cuál
sin examinarlas todas. Por eso buscar un elemento arbitrario en un heap es $O(n)$:
no hay orden entre hermanos ni entre subárboles distintos.
</details>

### Ejercicio 8 — Dibujar heap desde array

Dibuja el árbol correspondiente al array `[1, 3, 2, 7, 5, 4, 6, 9, 8]`.
¿Es min-heap? ¿Es max-heap?

<details>
<summary>Verificar</summary>

```
           1
          / \
        3     2
       / \   / \
      7   5 4   6
     / \
    9   8
```

Verificar min-heap (padre ≤ hijos):
- 1 ≤ 3 ✓, 1 ≤ 2 ✓
- 3 ≤ 7 ✓, 3 ≤ 5 ✓
- 2 ≤ 4 ✓, 2 ≤ 6 ✓
- 7 ≤ 9 ✓, 7 ≤ 8 ✓

**Sí**, es min-heap. **No** es max-heap (1 < 3).
</details>

### Ejercicio 9 — Implementar is_heap genérico

Escribe una función `is_heap` en Rust que acepte un comparador y funcione tanto
para min-heap como max-heap.

<details>
<summary>Verificar</summary>

```rust
fn is_heap<T, F>(arr: &[T], cmp: F) -> bool
where
    F: Fn(&T, &T) -> bool,  // retorna true si parent-child es valido
{
    (0..arr.len() / 2).all(|i| {
        let l = 2 * i + 1;
        let r = 2 * i + 2;
        (l >= arr.len() || cmp(&arr[i], &arr[l])) &&
        (r >= arr.len() || cmp(&arr[i], &arr[r]))
    })
}

// Uso:
let max_h = [90, 70, 80, 50, 60];
let min_h = [5, 10, 15, 20, 25];

assert!(is_heap(&max_h, |p, c| p >= c));   // max-heap
assert!(is_heap(&min_h, |p, c| p <= c));   // min-heap
assert!(!is_heap(&max_h, |p, c| p <= c));  // no es min-heap
```

Este patrón con comparador es la base para implementar un heap genérico que
sirva como max-heap o min-heap sin duplicar código.
</details>

### Ejercicio 10 — Rango de posiciones del segundo mayor

En un max-heap de $n \geq 2$ elementos, ¿en qué posiciones puede estar el
**segundo mayor** elemento? ¿Y el tercero?

<details>
<summary>Verificar</summary>

**Segundo mayor**: debe ser hijo directo de la raíz (el máximo). En un heap
con $n \geq 2$, la raíz tiene 1 o 2 hijos. El segundo mayor debe ser uno de
ellos, porque cualquier otro nodo es ≤ que su padre, que a su vez es ≤ que la
raíz.

Posiciones posibles: **1 o 2** (los hijos de la raíz). Solo 2 candidatos.

**Tercer mayor**: puede ser:
- El otro hijo de la raíz (si no es el segundo).
- Un hijo del segundo mayor (posiciones 3 o 4 si el segundo está en pos 1,
  o posiciones 5 o 6 si está en pos 2).

Posiciones posibles: **1, 2, 3, 4, 5, o 6** — los hijos de la raíz y los
nietos de la raíz. Máximo 5 candidatos (uno es el segundo, quedan hasta 5).

En general, el $k$-ésimo mayor está en los primeros $2^k - 1$ nodos. Encontrar
el $k$-ésimo mayor en un heap es $O(k \log k)$, no $O(1)$.
</details>
