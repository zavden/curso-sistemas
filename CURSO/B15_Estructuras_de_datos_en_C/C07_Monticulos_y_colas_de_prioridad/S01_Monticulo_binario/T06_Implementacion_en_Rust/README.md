# Implementación del heap en Rust

## Objetivo

Implementar el montículo binario en Rust desde tres ángulos:

1. **`BinaryHeap<T>` de la biblioteca estándar** — max-heap listo para usar.
2. **`Reverse<T>`** — wrapper de `std::cmp` para convertirlo en min-heap.
3. **Implementación manual** — heap genérico desde cero con traits `Ord`/`PartialOrd`.

Compararemos cada enfoque con la versión en C (T05) para resaltar cómo el
sistema de tipos de Rust elimina categorías enteras de errores que en C son
responsabilidad del programador.

---

## BinaryHeap de la biblioteca estándar

`std::collections::BinaryHeap<T>` es un max-heap incluido en la stdlib. Está
implementado internamente como un `Vec<T>` con las mismas fórmulas de
navegación padre-hijo que vimos en T01.

### Requisito: T debe implementar `Ord`

```rust
use std::collections::BinaryHeap;

fn main() {
    let mut heap = BinaryHeap::new();

    heap.push(40);
    heap.push(10);
    heap.push(70);
    heap.push(50);
    heap.push(30);

    println!("peek = {:?}", heap.peek());  // Some(70)
    println!("len  = {}", heap.len());     // 5

    while let Some(val) = heap.pop() {
        print!("{} ", val);
    }
    // 70 50 40 30 10
}
```

`BinaryHeap::push` corresponde a `heap_push` en C (sift-up interno).
`BinaryHeap::pop` corresponde a `heap_pop` (sift-down interno). Ambos son
$O(\log n)$.

### API completa

| Método | Equivalente C (T05) | Complejidad |
|--------|---------------------|-------------|
| `push(item)` | `heap_push(h, item)` | $O(\log n)$ |
| `pop()` → `Option<T>` | `heap_pop(h)` → `void *` | $O(\log n)$ |
| `peek()` → `Option<&T>` | `heap_peek(h)` → `void *` | $O(1)$ |
| `len()` | `heap_size(h)` | $O(1)$ |
| `is_empty()` | `heap_is_empty(h)` | $O(1)$ |
| `from(vec)` | `heap_build(h, items, n)` | $O(n)$ |
| `into_sorted_vec()` | (no existe) | $O(n \log n)$ |
| `drain()` | (no existe) | $O(n \log n)$ |

Diferencias clave con C:
- `pop` retorna `Option<T>`, no `void *`. No es posible desreferenciar
  `NULL` — el compilador obliga a manejar el `None`.
- `peek` retorna `Option<&T>` — una referencia inmutable, no un puntero raw.
- `from(vec)` consume el `Vec` y construye el heap en $O(n)$ (heapify
  bottom-up, igual que `heap_build` en C).
- No hay necesidad de `heap_free` — `Drop` libera automáticamente.

### Construcción desde un Vec (heapify)

```rust
use std::collections::BinaryHeap;

fn main() {
    let data = vec![4, 10, 3, 5, 1, 8, 7];
    let heap = BinaryHeap::from(data);  // O(n) heapify

    println!("peek = {:?}", heap.peek());  // Some(10)

    // extraer todo en orden descendente
    let sorted: Vec<i32> = heap.into_sorted_vec();
    println!("{:?}", sorted);  // [1, 3, 4, 5, 7, 8, 10] (ascendente!)
}
```

`into_sorted_vec` extrae todos los elementos y retorna un `Vec` en orden
**ascendente** (no descendente). Internamente hace pop repetido y luego
reversa, pero la implementación real es heapsort in-place sobre el `Vec`
subyacente.

### Iteración sin orden

```rust
use std::collections::BinaryHeap;

fn main() {
    let heap = BinaryHeap::from(vec![30, 10, 50, 20, 40]);

    // iter() NO garantiza orden de prioridad
    for val in &heap {
        print!("{} ", val);  // podria ser: 50 40 30 10 20 (orden interno)
    }
    println!();

    // para iterar en orden, usar into_sorted_vec o pop repetido
    let mut heap = BinaryHeap::from(vec![30, 10, 50, 20, 40]);
    while let Some(val) = heap.pop() {
        print!("{} ", val);  // 50 40 30 20 10 (garantizado)
    }
}
```

`iter()` recorre el array interno sin extraer — el orden es el del array
subyacente, que es un heap válido pero **no** es orden descendente. Si
necesitas orden, usa `pop` repetido o `into_sorted_vec`.

---

## Min-heap con Reverse

`BinaryHeap` es siempre un max-heap. Para obtener un min-heap, Rust ofrece
`std::cmp::Reverse`, un wrapper que invierte el orden:

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn main() {
    let mut min_heap: BinaryHeap<Reverse<i32>> = BinaryHeap::new();

    min_heap.push(Reverse(40));
    min_heap.push(Reverse(10));
    min_heap.push(Reverse(70));
    min_heap.push(Reverse(50));
    min_heap.push(Reverse(30));

    // extraer en orden ascendente
    while let Some(Reverse(val)) = min_heap.pop() {
        print!("{} ", val);
    }
    // 10 30 40 50 70
}
```

### Cómo funciona Reverse

`Reverse<T>` es un newtype que implementa `Ord` de forma invertida:

```rust
// Definicion simplificada en std::cmp
pub struct Reverse<T>(pub T);

impl<T: Ord> Ord for Reverse<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        other.0.cmp(&self.0)  // invertido: other vs self
    }
}
```

Esto es el equivalente exacto de invertir el comparador en C: en lugar de
`return a - b` para max-heap, usamos `return b - a` para min-heap. Pero en
Rust, el compilador garantiza que `Reverse` es consistente — en C, un
comparador con signo incorrecto es un error silencioso.

### Comparación con C

| Aspecto | C (T05) | Rust |
|---------|---------|------|
| Max-heap | `cmp_int_max`: `return a - b` | `BinaryHeap<i32>` |
| Min-heap | `cmp_int_min`: `return b - a` | `BinaryHeap<Reverse<i32>>` |
| Cambiar semántica | Cambiar el comparador (riesgo de error) | Cambiar el tipo (verificado en compilación) |
| Error de signo | Compila, falla en tiempo de ejecución | No aplica — `Reverse` es correcto por construcción |

### Min-heap con tipos complejos

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn main() {
    let words = vec!["mango", "apple", "cherry", "banana", "date"];
    let mut heap: BinaryHeap<Reverse<&str>> = BinaryHeap::new();

    for w in &words {
        heap.push(Reverse(*w));
    }

    println!("Orden alfabetico:");
    while let Some(Reverse(w)) = heap.pop() {
        println!("  {}", w);
    }
    // apple, banana, cherry, date, mango
}
```

---

## Orden personalizado con structs

En C, cualquier struct puede entrar al heap vía `void *` con un comparador
apropiado. En Rust, el struct debe implementar `Ord` (y por tanto `PartialOrd`,
`Eq`, `PartialEq`).

### Ejemplo: cola de prioridad de tareas

```rust
use std::collections::BinaryHeap;
use std::cmp::Ordering;

#[derive(Debug, Eq, PartialEq)]
struct Task {
    name: String,
    priority: u32,
    arrival: u32,
}

// max-heap por prioridad, desempate por llegada (menor arrival = primero)
impl Ord for Task {
    fn cmp(&self, other: &Self) -> Ordering {
        self.priority.cmp(&other.priority)
            .then_with(|| other.arrival.cmp(&self.arrival))
    }
}

impl PartialOrd for Task {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

fn main() {
    let mut pq = BinaryHeap::new();

    pq.push(Task { name: "backup".into(),  priority: 2, arrival: 0 });
    pq.push(Task { name: "deploy".into(),  priority: 5, arrival: 1 });
    pq.push(Task { name: "logs".into(),    priority: 1, arrival: 2 });
    pq.push(Task { name: "migrate".into(), priority: 5, arrival: 3 });
    pq.push(Task { name: "alert".into(),   priority: 9, arrival: 4 });
    pq.push(Task { name: "cleanup".into(), priority: 3, arrival: 5 });

    println!("Ejecutar por prioridad:");
    while let Some(task) = pq.pop() {
        println!("  {:10} (prioridad {}, llegada {})",
                 task.name, task.priority, task.arrival);
    }
}
```

Salida:

```
Ejecutar por prioridad:
  alert      (prioridad 9, llegada 4)
  deploy     (prioridad 5, llegada 1)
  migrate    (prioridad 5, llegada 3)
  cleanup    (prioridad 3, llegada 5)
  backup     (prioridad 2, llegada 0)
  logs       (prioridad 1, llegada 2)
```

Misma salida que la versión C con `cmp_task`. La diferencia: en Rust el
orden es parte del **tipo**, no de un puntero a función que puede cambiar
en runtime.

### Atajo: derive con tuplas

Para evitar implementar `Ord` manualmente, se puede usar el orden léxico
de tuplas:

```rust
use std::collections::BinaryHeap;

fn main() {
    // BinaryHeap de (prioridad, -arrival, nombre)
    // Tuplas comparan lexicograficamente: primero por prioridad, luego por
    // el negativo de arrival (menor arrival = mayor -(-arrival) = primero)
    let mut pq: BinaryHeap<(i32, i32, String)> = BinaryHeap::new();

    pq.push((2, 0, "backup".into()));
    pq.push((5, -1, "deploy".into()));
    pq.push((1, -2, "logs".into()));
    pq.push((5, -3, "migrate".into()));
    pq.push((9, -4, "alert".into()));
    pq.push((3, -5, "cleanup".into()));

    while let Some((pri, neg_arr, name)) = pq.pop() {
        println!("{:10} prioridad={}, llegada={}", name, pri, -neg_arr);
    }
}
```

El truco de negar `arrival` funciona porque las tuplas comparan campo por
campo en orden, y queremos que menor `arrival` tenga mayor prioridad. Este
patrón es común en competencias de programación.

---

## Implementación manual

Ahora construimos un heap genérico desde cero. Esto equivale a la
implementación C de T05, pero con las garantías de Rust.

### Estructura

```rust
pub struct Heap<T: Ord> {
    data: Vec<T>,
    is_max: bool,  // true = max-heap, false = min-heap
}
```

En C usamos un `void **` con un comparador externo. En Rust, `Vec<T>`
almacena los valores directamente (como `IntHeap` de T05), y el bound
`T: Ord` garantiza en compilación que los elementos son comparables.

La flag `is_max` reemplaza al comparador: determina si usamos
`Greater` o `Less` como condición de prioridad.

### Funciones auxiliares

```rust
impl<T: Ord> Heap<T> {
    fn parent(i: usize) -> usize {
        (i - 1) / 2
    }

    fn left(i: usize) -> usize {
        2 * i + 1
    }

    fn right(i: usize) -> usize {
        2 * i + 2
    }

    // retorna true si a tiene mayor prioridad que b
    fn has_priority(&self, a: &T, b: &T) -> bool {
        if self.is_max {
            a > b
        } else {
            a < b
        }
    }
}
```

Las fórmulas de navegación son idénticas a C. La diferencia es que
`has_priority` encapsula la lógica del comparador en un solo lugar,
eliminando el riesgo de invertir el signo accidentalmente.

### Sift-up y sift-down

```rust
impl<T: Ord> Heap<T> {
    fn sift_up(&mut self, mut index: usize) {
        while index > 0 {
            let p = Self::parent(index);
            if !self.has_priority(&self.data[index], &self.data[p]) {
                break;
            }
            self.data.swap(index, p);
            index = p;
        }
    }

    fn sift_down(&mut self, mut index: usize) {
        let n = self.data.len();
        loop {
            let mut target = index;
            let l = Self::left(index);
            let r = Self::right(index);

            if l < n && self.has_priority(&self.data[l], &self.data[target]) {
                target = l;
            }
            if r < n && self.has_priority(&self.data[r], &self.data[target]) {
                target = r;
            }

            if target == index {
                break;
            }
            self.data.swap(index, target);
            index = target;
        }
    }
}
```

`Vec::swap` es equivalente a la función `swap` de C, pero es un método
seguro sin punteros raw. El borrow checker garantiza que no accedemos a
índices inválidos en compilación (excepto out-of-bounds, que se detecta en
runtime con panic).

### API pública

```rust
impl<T: Ord> Heap<T> {
    pub fn new_max() -> Self {
        Heap { data: Vec::new(), is_max: true }
    }

    pub fn new_min() -> Self {
        Heap { data: Vec::new(), is_max: false }
    }

    pub fn push(&mut self, item: T) {
        self.data.push(item);
        let last = self.data.len() - 1;
        self.sift_up(last);
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.data.is_empty() {
            return None;
        }
        let last = self.data.len() - 1;
        self.data.swap(0, last);
        let top = self.data.pop();  // remove last (was root)
        if !self.data.is_empty() {
            self.sift_down(0);
        }
        top
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.first()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn from_vec(mut data: Vec<T>, is_max: bool) -> Self {
        let mut heap = Heap { data, is_max };
        let n = heap.data.len();
        // heapify bottom-up O(n)
        for i in (0..n / 2).rev() {
            heap.sift_down(i);
        }
        heap
    }
}
```

Notar la elegancia de `pop`:
1. Swap raíz con último.
2. `Vec::pop()` elimina el último (que era la raíz) y lo retorna como
   `Option<T>`.
3. Sift-down la nueva raíz.

En C necesitamos guardar `data[0]` antes del swap, luego decrementar `size`,
y luego retornar el puntero. En Rust, `Vec::pop` hace la eliminación y
retorno atómicamente.

### Comparación directa C vs Rust

| Aspecto | C (T05) | Rust (manual) |
|---------|---------|---------------|
| Almacenamiento | `void **data` (punteros) | `Vec<T>` (valores) |
| Genérico vía | Comparador `CmpFunc` | Trait bound `T: Ord` |
| Verificación de tipo | Ninguna (cast `void *`) | Compilación |
| Crecimiento | `realloc` manual | `Vec` automático |
| Liberación | `heap_free` manual | `Drop` automático |
| Overflow en comparación | Posible con `a - b` | `Ord::cmp` retorna `Ordering`, no `int` |
| NULL check | Responsabilidad del usuario | `Option<T>` obligatorio |
| Dangling pointer | Posible (§ errores T05) | Imposible (borrow checker) |

---

## Programa completo

```rust
use std::fmt;

// ======== Implementacion manual ========

pub struct Heap<T: Ord> {
    data: Vec<T>,
    is_max: bool,
}

impl<T: Ord> Heap<T> {
    pub fn new_max() -> Self {
        Heap { data: Vec::new(), is_max: true }
    }

    pub fn new_min() -> Self {
        Heap { data: Vec::new(), is_max: false }
    }

    pub fn push(&mut self, item: T) {
        self.data.push(item);
        let last = self.data.len() - 1;
        self.sift_up(last);
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.data.is_empty() {
            return None;
        }
        let last = self.data.len() - 1;
        self.data.swap(0, last);
        let top = self.data.pop();
        if !self.data.is_empty() {
            self.sift_down(0);
        }
        top
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.first()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn from_vec(data: Vec<T>, is_max: bool) -> Self {
        let mut heap = Heap { data, is_max };
        let n = heap.data.len();
        for i in (0..n / 2).rev() {
            heap.sift_down(i);
        }
        heap
    }

    fn parent(i: usize) -> usize {
        (i - 1) / 2
    }

    fn left(i: usize) -> usize {
        2 * i + 1
    }

    fn right(i: usize) -> usize {
        2 * i + 2
    }

    fn has_priority(&self, a: &T, b: &T) -> bool {
        if self.is_max { a > b } else { a < b }
    }

    fn sift_up(&mut self, mut index: usize) {
        while index > 0 {
            let p = Self::parent(index);
            if !self.has_priority(&self.data[index], &self.data[p]) {
                break;
            }
            self.data.swap(index, p);
            index = p;
        }
    }

    fn sift_down(&mut self, mut index: usize) {
        let n = self.data.len();
        loop {
            let mut target = index;
            let l = Self::left(index);
            let r = Self::right(index);

            if l < n && self.has_priority(&self.data[l], &self.data[target]) {
                target = l;
            }
            if r < n && self.has_priority(&self.data[r], &self.data[target]) {
                target = r;
            }

            if target == index {
                break;
            }
            self.data.swap(index, target);
            index = target;
        }
    }
}

impl<T: Ord + fmt::Debug> fmt::Debug for Heap<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Heap({:?})", self.data)
    }
}

// ======== Demos con stdlib ========

fn demo_stdlib_max_heap() {
    use std::collections::BinaryHeap;

    println!("=== BinaryHeap max-heap (stdlib) ===\n");

    let mut heap = BinaryHeap::new();
    let values = [40, 10, 70, 50, 30, 85, 20, 60];

    for &v in &values {
        heap.push(v);
        println!("push({}) -> peek = {:?}, len = {}",
                 v, heap.peek(), heap.len());
    }

    println!("\nExtraer en orden:");
    while let Some(val) = heap.pop() {
        println!("  pop() = {}", val);
    }
}

fn demo_stdlib_min_heap() {
    use std::collections::BinaryHeap;
    use std::cmp::Reverse;

    println!("\n=== BinaryHeap min-heap con Reverse ===\n");

    let mut heap = BinaryHeap::new();
    for &v in &[40, 10, 70, 50, 30] {
        heap.push(Reverse(v));
    }

    println!("Extraer en orden ascendente:");
    while let Some(Reverse(val)) = heap.pop() {
        println!("  pop() = {}", val);
    }
}

fn demo_stdlib_strings() {
    use std::collections::BinaryHeap;
    use std::cmp::Reverse;

    println!("\n=== Min-heap de strings (stdlib) ===\n");

    let words = ["mango", "apple", "cherry", "banana", "date"];
    let mut heap: BinaryHeap<Reverse<&str>> = BinaryHeap::new();

    for w in &words {
        heap.push(Reverse(*w));
        println!("push(\"{}\")", w);
    }

    println!("\nExtraer en orden alfabetico:");
    while let Some(Reverse(w)) = heap.pop() {
        println!("  pop() = \"{}\"", w);
    }
}

fn demo_stdlib_build() {
    use std::collections::BinaryHeap;

    println!("\n=== Build con BinaryHeap::from (stdlib) ===\n");

    let data = vec![4, 10, 3, 5, 1, 8, 7];
    println!("Vec original: {:?}", data);

    let heap = BinaryHeap::from(data);
    println!("peek = {:?}", heap.peek());

    let sorted = heap.into_sorted_vec();
    println!("into_sorted_vec = {:?}", sorted);
}

// ======== Demos con implementacion manual ========

fn demo_manual_max_heap() {
    println!("\n=== Max-heap manual ===\n");

    let mut heap = Heap::new_max();
    for &v in &[40, 10, 70, 50, 30, 85, 20, 60] {
        heap.push(v);
        println!("push({}) -> peek = {:?}, len = {}",
                 v, heap.peek(), heap.len());
    }

    println!("\nExtraer:");
    while let Some(val) = heap.pop() {
        print!("{} ", val);
    }
    println!();
}

fn demo_manual_min_heap() {
    println!("\n=== Min-heap manual ===\n");

    let mut heap = Heap::new_min();
    for &v in &[40, 10, 70, 50, 30] {
        heap.push(v);
    }

    print!("Extraer: ");
    while let Some(val) = heap.pop() {
        print!("{} ", val);
    }
    println!();
}

fn demo_manual_build() {
    println!("\n=== Build manual (heapify) ===\n");

    let data = vec![4, 10, 3, 5, 1, 8, 7];
    println!("Vec original: {:?}", data);

    let mut heap = Heap::from_vec(data, true);  // max-heap
    println!("peek = {:?}", heap.peek());

    print!("Extraer: ");
    while let Some(val) = heap.pop() {
        print!("{} ", val);
    }
    println!();
}

fn demo_top_k() {
    use std::collections::BinaryHeap;
    use std::cmp::Reverse;

    println!("\n=== Top-3 de 10 elementos ===\n");

    let values = [42, 17, 93, 8, 55, 71, 29, 64, 36, 81];
    let k = 3;

    println!("Array: {:?}", values);

    // min-heap de tamano k
    let mut heap: BinaryHeap<Reverse<i32>> = BinaryHeap::new();

    for &v in &values {
        if heap.len() < k {
            heap.push(Reverse(v));
        } else if let Some(&Reverse(min)) = heap.peek() {
            if v > min {
                heap.pop();
                heap.push(Reverse(v));
            }
        }
    }

    let mut result: Vec<i32> = heap.into_iter().map(|Reverse(v)| v).collect();
    result.sort_unstable_by(|a, b| b.cmp(a));
    println!("Top {}: {:?}", k, result);
}

fn demo_task_queue() {
    println!("\n=== Cola de prioridad de tareas (manual) ===\n");

    #[derive(Debug, Eq, PartialEq)]
    struct Task {
        name: String,
        priority: u32,
        arrival: u32,
    }

    impl Ord for Task {
        fn cmp(&self, other: &Self) -> std::cmp::Ordering {
            self.priority.cmp(&other.priority)
                .then_with(|| other.arrival.cmp(&self.arrival))
        }
    }

    impl PartialOrd for Task {
        fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
            Some(self.cmp(other))
        }
    }

    let mut pq = Heap::new_max();
    let tasks = [
        ("backup",  2, 0), ("deploy",  5, 1), ("logs",    1, 2),
        ("migrate", 5, 3), ("alert",   9, 4), ("cleanup", 3, 5),
    ];

    for &(name, pri, arr) in &tasks {
        println!("enqueue: {:10} (prioridad {})", name, pri);
        pq.push(Task { name: name.into(), priority: pri, arrival: arr });
    }

    println!("\nEjecutar por prioridad:");
    while let Some(task) = pq.pop() {
        println!("  {:10} (prioridad {}, llegada {})",
                 task.name, task.priority, task.arrival);
    }
}

fn main() {
    demo_stdlib_max_heap();
    demo_stdlib_min_heap();
    demo_stdlib_strings();
    demo_stdlib_build();
    demo_manual_max_heap();
    demo_manual_min_heap();
    demo_manual_build();
    demo_top_k();
    demo_task_queue();
}
```

### Salida esperada

```
=== BinaryHeap max-heap (stdlib) ===

push(40) -> peek = Some(40), len = 1
push(10) -> peek = Some(40), len = 2
push(70) -> peek = Some(70), len = 3
push(50) -> peek = Some(70), len = 4
push(30) -> peek = Some(70), len = 5
push(85) -> peek = Some(85), len = 6
push(20) -> peek = Some(85), len = 7
push(60) -> peek = Some(85), len = 8

Extraer en orden:
  pop() = 85
  pop() = 70
  pop() = 60
  pop() = 50
  pop() = 40
  pop() = 30
  pop() = 20
  pop() = 10

=== BinaryHeap min-heap con Reverse ===

Extraer en orden ascendente:
  pop() = 10
  pop() = 30
  pop() = 40
  pop() = 50
  pop() = 70

=== Min-heap de strings (stdlib) ===

push("mango")
push("apple")
push("cherry")
push("banana")
push("date")

Extraer en orden alfabetico:
  pop() = "apple"
  pop() = "banana"
  pop() = "cherry"
  pop() = "date"
  pop() = "mango"

=== Build con BinaryHeap::from (stdlib) ===

Vec original: [4, 10, 3, 5, 1, 8, 7]
peek = Some(10)
into_sorted_vec = [1, 3, 4, 5, 7, 8, 10]

=== Max-heap manual ===

push(40) -> peek = Some(40), len = 1
push(10) -> peek = Some(40), len = 2
push(70) -> peek = Some(70), len = 3
push(50) -> peek = Some(70), len = 4
push(30) -> peek = Some(70), len = 5
push(85) -> peek = Some(85), len = 6
push(20) -> peek = Some(85), len = 7
push(60) -> peek = Some(85), len = 8

Extraer:
85 70 60 50 40 30 20 10

=== Min-heap manual ===

Extraer: 10 30 40 50 70

=== Build manual (heapify) ===

Vec original: [4, 10, 3, 5, 1, 8, 7]
peek = Some(10)
Extraer: 10 8 7 5 4 3 1

=== Top-3 de 10 elementos ===

Array: [42, 17, 93, 8, 55, 71, 29, 64, 36, 81]
Top 3: [93, 81, 71]

=== Cola de prioridad de tareas (manual) ===

enqueue: backup     (prioridad 2)
enqueue: deploy     (prioridad 5)
enqueue: logs       (prioridad 1)
enqueue: migrate    (prioridad 5)
enqueue: alert      (prioridad 9)
enqueue: cleanup    (prioridad 3)

Ejecutar por prioridad:
  alert      (prioridad 9, llegada 4)
  deploy     (prioridad 5, llegada 1)
  migrate    (prioridad 5, llegada 3)
  cleanup    (prioridad 3, llegada 5)
  backup     (prioridad 2, llegada 0)
  logs       (prioridad 1, llegada 2)
```

---

## Errores que Rust previene

Cada uno de estos errores ocurrió en la sección de errores comunes de T05 (C).
En Rust, ninguno compila:

### 1. Dangling pointer a variable local

```rust
// En C: heap_push(h, &x) donde x es variable de loop — todos apuntan al mismo sitio
// En Rust: imposible. push(item) toma ownership de item (T, no &T).
// El valor se MUEVE al heap — no hay puntero que pueda quedar colgando.

let mut heap = Heap::new_max();
for v in [40, 10, 70] {
    heap.push(v);  // v se copia (i32 es Copy), no hay referencia
}
```

### 2. Comparador con signo incorrecto

```rust
// En C: cmp_wrong retorna b-a cuando queria a-b → min en lugar de max
// En Rust: no existe comparador runtime. El orden esta definido por Ord
// del tipo. Para invertir, se usa Reverse — que es correcto por construccion.
```

### 3. Pop/peek sin verificar vacío

```rust
// En C: *(int *)heap_pop(h) → dereferencia NULL si heap vacio
// En Rust: heap.pop() retorna Option<T>

// esto NO compila:
// let val: i32 = heap.pop();  // expected i32, found Option<i32>

// obligatorio manejar el None:
if let Some(val) = heap.pop() {
    println!("{}", val);
}
```

### 4. Tipo incompatible con comparación

```rust
// En C: puedo hacer heap_push(h, &my_struct) con un comparador para int → UB
// En Rust: Heap<T: Ord> solo acepta T que implemente Ord

struct Point { x: f64, y: f64 }  // no implementa Ord

let mut heap = Heap::new_max();
// heap.push(Point { x: 1.0, y: 2.0 });  // ERROR: Point no implementa Ord
```

`f64` tampoco implementa `Ord` (por `NaN`), solo `PartialOrd`. Si intentas
hacer `BinaryHeap<f64>`, no compila. Para floats, se necesita un wrapper
como `OrderedFloat` de la crate `ordered-float`, o implementar `Ord`
manualmente garantizando que no haya `NaN`.

---

## Cuándo usar cada enfoque

| Situación | Enfoque |
|-----------|---------|
| Max-heap simple | `BinaryHeap<T>` |
| Min-heap simple | `BinaryHeap<Reverse<T>>` |
| Orden personalizado | Implementar `Ord` en el struct |
| Necesitas `delete` o `decrease_key` | Implementación manual (o crate `priority-queue`) |
| Rendimiento crítico | `BinaryHeap` stdlib (optimizada, usa unsafe interno) |
| Aprendizaje | Implementación manual |

La stdlib cubre el 90% de los casos. La implementación manual es necesaria
cuando necesitas operaciones que `BinaryHeap` no expone — como borrado por
índice o decrease-key, que son esenciales para algoritmos de grafos
(Dijkstra, Prim).

---

## Ejercicios

### Ejercicio 1 — BinaryHeap básico

Crea un `BinaryHeap` max-heap con los valores `[20, 5, 15, 10, 25, 30]`.
¿Cuál es `peek()` después de construirlo? Extrae todos los elementos.
¿En qué orden salen?

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;

fn main() {
    let heap = BinaryHeap::from(vec![20, 5, 15, 10, 25, 30]);
    println!("peek = {:?}", heap.peek());  // Some(30)

    let mut heap = heap;
    while let Some(v) = heap.pop() {
        print!("{} ", v);
    }
    // 30 25 20 15 10 5
}
```

`peek()` retorna `Some(30)` — el máximo. Extraen en orden descendente:
**30, 25, 20, 15, 10, 5**.
</details>

### Ejercicio 2 — Min-heap con Reverse

Construye un min-heap con `[3.14, 1.41, 2.72, 0.58, 1.73]` usando
`Reverse`. Nota: `f64` no implementa `Ord`, así que necesitas un wrapper.
Usa `OrderedFloat` conceptualmente, o bien convierte a enteros (multiplica
por 100 y usa `i32`).

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn main() {
    // Opcion 1: multiplicar por 100 para evitar f64
    let values = [314, 141, 272, 58, 173];  // centesimas
    let mut heap = BinaryHeap::new();

    for &v in &values {
        heap.push(Reverse(v));
    }

    println!("Extraer en orden ascendente:");
    while let Some(Reverse(v)) = heap.pop() {
        println!("  {:.2}", v as f64 / 100.0);
    }
    // 0.58, 1.41, 1.73, 2.72, 3.14
}
```

`f64` no implementa `Ord` porque `NaN != NaN` viola la reflexividad que
`Eq` requiere. Opciones reales:
- Crate `ordered-float`: `BinaryHeap<Reverse<OrderedFloat<f64>>>`.
- Newtype manual que implemente `Ord` asumiendo que no habrá `NaN`.
- Escalar a enteros (como aquí) cuando la precisión lo permite.
</details>

### Ejercicio 3 — Struct con Ord personalizado

Define un struct `Student` con campos `name: String`, `gpa: u32` (GPA × 100
para evitar floats) y `year: u32`. Implementa `Ord` para que un `BinaryHeap`
extraiga primero al de mayor GPA, desempatando por año ascendente (más
antiguo primero). Prueba con 3 estudiantes.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Ordering;

#[derive(Debug, Eq, PartialEq)]
struct Student {
    name: String,
    gpa: u32,    // gpa * 100 (e.g., 390 = 3.90)
    year: u32,
}

impl Ord for Student {
    fn cmp(&self, other: &Self) -> Ordering {
        // mayor GPA primero
        self.gpa.cmp(&other.gpa)
            // mismo GPA: menor year primero (mas antiguo = mayor prioridad)
            .then_with(|| other.year.cmp(&self.year))
    }
}

impl PartialOrd for Student {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

fn main() {
    let mut heap = BinaryHeap::new();
    heap.push(Student { name: "Ana".into(), gpa: 390, year: 2022 });
    heap.push(Student { name: "Bob".into(), gpa: 390, year: 2021 });
    heap.push(Student { name: "Cat".into(), gpa: 400, year: 2023 });

    while let Some(s) = heap.pop() {
        println!("{}: GPA {:.2}, year {}",
                 s.name, s.gpa as f64 / 100.0, s.year);
    }
    // Cat: GPA 4.00, year 2023
    // Bob: GPA 3.90, year 2021
    // Ana: GPA 3.90, year 2022
}
```

`then_with` encadena criterios: solo evalúa el segundo si el primero es
`Equal`. El orden invertido de `year` (`other.year.cmp(&self.year)`) da
prioridad al más antiguo.
</details>

### Ejercicio 4 — Top-k con min-heap

Implementa `top_k(arr: &[i32], k: usize) -> Vec<i32>` que retorne los $k$
mayores elementos en orden descendente, usando un `BinaryHeap<Reverse<i32>>`
de tamaño $k$.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn top_k(arr: &[i32], k: usize) -> Vec<i32> {
    let mut heap: BinaryHeap<Reverse<i32>> = BinaryHeap::new();

    for &v in arr {
        if heap.len() < k {
            heap.push(Reverse(v));
        } else if let Some(&Reverse(min)) = heap.peek() {
            if v > min {
                heap.pop();
                heap.push(Reverse(v));
            }
        }
    }

    let mut result: Vec<i32> = heap.into_iter().map(|Reverse(v)| v).collect();
    result.sort_unstable_by(|a, b| b.cmp(a));
    result
}

fn main() {
    let arr = [42, 17, 93, 8, 55, 71, 29, 64, 36, 81];
    println!("Top 3: {:?}", top_k(&arr, 3));
    // Top 3: [93, 81, 71]
}
```

Complejidad: $O(n \log k)$. El min-heap de tamaño $k$ mantiene en la raíz
el menor de los $k$ mayores vistos hasta ahora. Cada nuevo elemento que
supere esa raíz la reemplaza. `into_iter` + sort al final es $O(k \log k)$,
despreciable.
</details>

### Ejercicio 5 — Merge de k iteradores ordenados

Dados $k$ `Vec<i32>` ordenados, combínalos en un solo `Vec<i32>` ordenado
usando un min-heap de tamaño $k$.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Ordering;

#[derive(Eq, PartialEq)]
struct Entry {
    value: i32,
    array_idx: usize,
    elem_idx: usize,
}

impl Ord for Entry {
    fn cmp(&self, other: &Self) -> Ordering {
        // invertido para min-heap
        other.value.cmp(&self.value)
    }
}

impl PartialOrd for Entry {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

fn merge_k_sorted(arrays: &[Vec<i32>]) -> Vec<i32> {
    let mut heap = BinaryHeap::new();
    let mut result = Vec::new();

    // insertar primer elemento de cada array
    for (i, arr) in arrays.iter().enumerate() {
        if !arr.is_empty() {
            heap.push(Entry { value: arr[0], array_idx: i, elem_idx: 0 });
        }
    }

    while let Some(entry) = heap.pop() {
        result.push(entry.value);

        let next = entry.elem_idx + 1;
        if next < arrays[entry.array_idx].len() {
            heap.push(Entry {
                value: arrays[entry.array_idx][next],
                array_idx: entry.array_idx,
                elem_idx: next,
            });
        }
    }

    result
}

fn main() {
    let arrays = vec![
        vec![1, 4, 7],
        vec![2, 5, 8],
        vec![3, 6, 9],
    ];
    println!("{:?}", merge_k_sorted(&arrays));
    // [1, 2, 3, 4, 5, 6, 7, 8, 9]
}
```

Complejidad: $O(N \log k)$ donde $N$ es el total de elementos. Cada
operación de heap es $O(\log k)$, y cada elemento entra y sale exactamente
una vez.

En C (T05) necesitamos un pool de `HeapEntry` alocados manualmente. En Rust,
`BinaryHeap` posee los `Entry` directamente — sin punteros, sin pool.
</details>

### Ejercicio 6 — Mediana en stream

Implementa un `MedianFinder` que use dos `BinaryHeap`:
- `lower`: max-heap con la mitad inferior.
- `upper`: min-heap (`Reverse`) con la mitad superior.

Métodos: `add(val: i32)` y `median() -> f64`.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

struct MedianFinder {
    lower: BinaryHeap<i32>,           // max-heap
    upper: BinaryHeap<Reverse<i32>>,  // min-heap
}

impl MedianFinder {
    fn new() -> Self {
        MedianFinder {
            lower: BinaryHeap::new(),
            upper: BinaryHeap::new(),
        }
    }

    fn add(&mut self, val: i32) {
        if self.lower.is_empty() || val <= *self.lower.peek().unwrap() {
            self.lower.push(val);
        } else {
            self.upper.push(Reverse(val));
        }

        // balancear: lower puede tener a lo sumo 1 mas que upper
        if self.lower.len() > self.upper.len() + 1 {
            let top = self.lower.pop().unwrap();
            self.upper.push(Reverse(top));
        } else if self.upper.len() > self.lower.len() {
            let Reverse(top) = self.upper.pop().unwrap();
            self.lower.push(top);
        }
    }

    fn median(&self) -> f64 {
        if self.lower.len() > self.upper.len() {
            *self.lower.peek().unwrap() as f64
        } else {
            let a = *self.lower.peek().unwrap() as f64;
            let Reverse(b) = *self.upper.peek().unwrap();
            (a + b as f64) / 2.0
        }
    }
}

fn main() {
    let mut mf = MedianFinder::new();

    for &v in &[5, 2, 8, 1, 9] {
        mf.add(v);
        println!("add({}) -> mediana = {:.1}", v, mf.median());
    }
    // add(5) -> mediana = 5.0
    // add(2) -> mediana = 3.5
    // add(8) -> mediana = 5.0
    // add(1) -> mediana = 3.5
    // add(9) -> mediana = 5.0
}
```

En C necesitamos dos `Heap *` con comparadores distintos y castear `void *`
en cada acceso. En Rust, los dos heaps tienen tipos distintos (`BinaryHeap<i32>`
vs `BinaryHeap<Reverse<i32>>`), lo que hace imposible confundir cuál es cuál.

Complejidad: $O(\log n)$ por inserción, $O(1)$ para consultar la mediana.
</details>

### Ejercicio 7 — Delete en heap manual

Añade un método `delete(&mut self, index: usize) -> Option<T>` a la
implementación manual de `Heap<T>`. Debe poder subir o bajar el reemplazo
según sea necesario.

<details>
<summary>Verificar</summary>

```rust
impl<T: Ord> Heap<T> {
    pub fn delete(&mut self, index: usize) -> Option<T> {
        if index >= self.data.len() {
            return None;
        }

        let last = self.data.len() - 1;

        if index == last {
            return self.data.pop();
        }

        self.data.swap(index, last);
        let removed = self.data.pop();

        // el nuevo elemento en index puede necesitar subir o bajar
        if index < self.data.len() {
            self.sift_up(index);
            self.sift_down(index);
        }

        removed
    }
}
```

Igual que en C (T05 ejercicio 7): swap con el último, pop, luego intentar
sift-up y sift-down. Solo uno de los dos hará algo (o ninguno si ya está
en su lugar). Complejidad: $O(\log n)$.

La ventaja sobre C: `Option<T>` comunica que el delete puede fallar (índice
inválido), mientras que en C retornamos `NULL` y el usuario puede olvidar
verificarlo.
</details>

### Ejercicio 8 — Comparar BinaryHeap vs Vec ordenado

Escribe un programa que mida el tiempo de $n$ operaciones `push` + `pop`
alternadas, comparando `BinaryHeap` vs `Vec` con inserción ordenada
(`binary_search` + `insert`), para $n = 10^4$ y $n = 10^5$.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::time::Instant;

fn bench_heap(n: usize) -> f64 {
    let start = Instant::now();
    let mut heap = BinaryHeap::new();

    for i in 0..n {
        heap.push(i as i32 * 7 % 1000);
        if i % 2 == 1 { heap.pop(); }
    }
    while heap.pop().is_some() {}
    start.elapsed().as_secs_f64()
}

fn bench_sorted_vec(n: usize) -> f64 {
    let start = Instant::now();
    let mut vec: Vec<i32> = Vec::new();

    for i in 0..n {
        let val = i as i32 * 7 % 1000;
        let pos = vec.binary_search(&val).unwrap_or_else(|p| p);
        vec.insert(pos, val);
        if i % 2 == 1 { vec.pop(); }  // max esta al final (ascendente)
    }
    while vec.pop().is_some() {}
    start.elapsed().as_secs_f64()
}

fn main() {
    for &n in &[10_000, 100_000] {
        let heap_t = bench_heap(n);
        let vec_t = bench_sorted_vec(n);
        println!("n={:>7}: heap={:.4}s, vec={:.4}s, ratio={:.1}x",
                 n, heap_t, vec_t, vec_t / heap_t);
    }
}
```

Resultado típico:

```
n=  10000: heap=0.0003s, vec=0.0050s, ratio=16.7x
n= 100000: heap=0.0040s, vec=0.4500s, ratio=112.5x
```

`Vec::insert` es $O(n)$ por el shift de elementos, mientras que `push`/`pop`
del heap son $O(\log n)$. Para $n = 10^5$ la diferencia ya es de dos órdenes
de magnitud.
</details>

### Ejercicio 9 — Heap genérico con closure

Modifica la implementación manual para que en lugar de `is_max: bool`, acepte
un closure `F: Fn(&T, &T) -> bool` que determine si el primer argumento tiene
mayor prioridad que el segundo. Esto es análogo al `CmpFunc` de C.

<details>
<summary>Verificar</summary>

```rust
pub struct HeapFn<T, F: Fn(&T, &T) -> bool> {
    data: Vec<T>,
    has_priority: F,
}

impl<T, F: Fn(&T, &T) -> bool> HeapFn<T, F> {
    pub fn new(has_priority: F) -> Self {
        HeapFn { data: Vec::new(), has_priority }
    }

    pub fn push(&mut self, item: T) {
        self.data.push(item);
        let last = self.data.len() - 1;
        self.sift_up(last);
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.data.is_empty() { return None; }
        let last = self.data.len() - 1;
        self.data.swap(0, last);
        let top = self.data.pop();
        if !self.data.is_empty() { self.sift_down(0); }
        top
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.first()
    }

    fn sift_up(&mut self, mut i: usize) {
        while i > 0 {
            let p = (i - 1) / 2;
            if !(self.has_priority)(&self.data[i], &self.data[p]) { break; }
            self.data.swap(i, p);
            i = p;
        }
    }

    fn sift_down(&mut self, mut i: usize) {
        let n = self.data.len();
        loop {
            let mut t = i;
            let l = 2 * i + 1;
            let r = 2 * i + 2;
            if l < n && (self.has_priority)(&self.data[l], &self.data[t]) { t = l; }
            if r < n && (self.has_priority)(&self.data[r], &self.data[t]) { t = r; }
            if t == i { break; }
            self.data.swap(i, t);
            i = t;
        }
    }
}

fn main() {
    // max-heap
    let mut max_h = HeapFn::new(|a: &i32, b: &i32| a > b);
    for v in [40, 10, 70, 50] { max_h.push(v); }
    while let Some(v) = max_h.pop() { print!("{} ", v); }
    println!();  // 70 50 40 10

    // min-heap con el mismo tipo, diferente closure
    let mut min_h = HeapFn::new(|a: &i32, b: &i32| a < b);
    for v in [40, 10, 70, 50] { min_h.push(v); }
    while let Some(v) = min_h.pop() { print!("{} ", v); }
    println!();  // 10 40 50 70
}
```

Este diseño es el equivalente Rust del patrón `CmpFunc` de C. La diferencia:
el closure se verifica en compilación (no es un `void *` que puede ser
cualquier cosa). Además, `T` no necesita implementar `Ord` — el closure
define el orden, lo cual es útil para tipos que no tienen un orden natural
(como `f64`).

```rust
// funciona con f64 (que no implementa Ord)
let mut float_heap = HeapFn::new(|a: &f64, b: &f64| a < b);
float_heap.push(3.14);
float_heap.push(1.41);
float_heap.push(2.72);
while let Some(v) = float_heap.pop() { print!("{} ", v); }
// 1.41 2.72 3.14
```
</details>

### Ejercicio 10 — Decrease-key con HashMap

Implementa un `IndexedHeap<K, V>` donde `K: Hash + Eq + Clone` es la clave
(e.g., ID de nodo en un grafo) y `V: Ord` es la prioridad. Soporta
`decrease_key(key, new_value)` en $O(\log n)$ usando un `HashMap<K, usize>`
que mapea clave a índice en el array.

<details>
<summary>Verificar</summary>

```rust
use std::collections::HashMap;
use std::hash::Hash;

pub struct IndexedHeap<K: Hash + Eq + Clone, V: Ord> {
    data: Vec<(K, V)>,
    index: HashMap<K, usize>,  // key -> posicion en data
}

impl<K: Hash + Eq + Clone, V: Ord> IndexedHeap<K, V> {
    pub fn new() -> Self {
        IndexedHeap { data: Vec::new(), index: HashMap::new() }
    }

    pub fn push(&mut self, key: K, value: V) {
        let pos = self.data.len();
        self.index.insert(key.clone(), pos);
        self.data.push((key, value));
        self.sift_up(pos);
    }

    pub fn pop(&mut self) -> Option<(K, V)> {
        if self.data.is_empty() { return None; }
        let last = self.data.len() - 1;
        self.swap_positions(0, last);
        let (key, val) = self.data.pop().unwrap();
        self.index.remove(&key);
        if !self.data.is_empty() { self.sift_down(0); }
        Some((key, val))
    }

    pub fn decrease_key(&mut self, key: &K, new_value: V) {
        if let Some(&pos) = self.index.get(key) {
            // solo actualizar si el nuevo valor es menor (min-heap)
            if new_value < self.data[pos].1 {
                self.data[pos].1 = new_value;
                self.sift_up(pos);
            }
        }
    }

    pub fn contains(&self, key: &K) -> bool {
        self.index.contains_key(key)
    }

    fn swap_positions(&mut self, i: usize, j: usize) {
        self.index.insert(self.data[i].0.clone(), j);
        self.index.insert(self.data[j].0.clone(), i);
        self.data.swap(i, j);
    }

    fn sift_up(&mut self, mut i: usize) {
        while i > 0 {
            let p = (i - 1) / 2;
            // min-heap: menor valor = mayor prioridad
            if self.data[i].1 >= self.data[p].1 { break; }
            self.swap_positions(i, p);
            i = p;
        }
    }

    fn sift_down(&mut self, mut i: usize) {
        let n = self.data.len();
        loop {
            let mut t = i;
            let l = 2 * i + 1;
            let r = 2 * i + 2;
            if l < n && self.data[l].1 < self.data[t].1 { t = l; }
            if r < n && self.data[r].1 < self.data[t].1 { t = r; }
            if t == i { break; }
            self.swap_positions(i, t);
            i = t;
        }
    }
}

fn main() {
    // Simular Dijkstra: nodos A-E con distancias
    let mut heap = IndexedHeap::new();
    heap.push("A", u32::MAX);
    heap.push("B", u32::MAX);
    heap.push("C", u32::MAX);
    heap.push("D", u32::MAX);

    // "relajar" aristas
    heap.decrease_key(&"A", 0);    // fuente
    heap.decrease_key(&"B", 10);
    heap.decrease_key(&"C", 5);

    println!("Extraer nodos por distancia:");
    while let Some((node, dist)) = heap.pop() {
        if dist == u32::MAX { continue; }
        println!("  {} -> distancia {}", node, dist);
    }
    // C -> distancia 5
    // B -> distancia 10 (o actualizado si hay decrease posterior)
}
```

Este es el **indexed priority queue** mencionado en T05 ejercicio 10.
En C, el mapa `position[node_id]` es un array simple (solo funciona si los
nodos son enteros 0..N). En Rust, `HashMap<K, usize>` funciona con cualquier
tipo de clave hashable.

La clave está en `swap_positions`: cada vez que dos elementos intercambian
posiciones en el array, actualizamos el `HashMap` para que siempre refleje
la posición correcta. Esto permite que `decrease_key` localice el nodo en
$O(1)$ y luego haga sift-up en $O(\log n)$.
</details>
