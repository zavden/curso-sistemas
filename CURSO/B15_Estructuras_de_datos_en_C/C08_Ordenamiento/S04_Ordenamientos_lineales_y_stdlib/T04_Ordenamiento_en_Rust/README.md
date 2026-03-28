# Ordenamiento en Rust

## Objetivo

Dominar las funciones de ordenamiento de la biblioteca estándar de Rust,
que representan un salto cualitativo respecto a `qsort` de C:

- **`sort`**: estable, basado en Timsort — preserva el orden de
  elementos iguales.
- **`sort_unstable`**: basado en pdqsort — más rápido, no estable.
- **Closures como comparadores**: sin casting, sin `void *`, con
  verificación de tipos en compilación.
- **Monomorphization**: el compilador genera código especializado
  para cada tipo, permitiendo inlining total del comparador.

---

## La familia de métodos de ordenamiento

`[T]` (slices) ofrece seis métodos de ordenamiento, organizados en
dos ejes: **estable vs inestable** y **por Ord vs por clave vs por
comparador**:

| Método | Estable | Criterio | Algoritmo |
|--------|---------|----------|-----------|
| `sort()` | Sí | `Ord` del tipo | Timsort (merge sort adaptativo) |
| `sort_by(cmp)` | Sí | Closure comparador | Timsort |
| `sort_by_key(f)` | Sí | Closure que extrae clave | Timsort |
| `sort_unstable()` | No | `Ord` del tipo | pdqsort |
| `sort_unstable_by(cmp)` | No | Closure comparador | pdqsort |
| `sort_unstable_by_key(f)` | No | Closure que extrae clave | pdqsort |

La versión estable garantiza que elementos con la misma clave
mantienen su orden relativo original. La versión inestable puede
reordenarlos, pero a cambio es más rápida y no necesita memoria
auxiliar.

### Cuándo usar cada uno

- **`sort`**: cuando la estabilidad importa (ordenar una tabla ya
  ordenada por un campo secundario, preservar orden de inserción).
- **`sort_unstable`**: cuando solo importa el resultado final y se
  quiere máxima velocidad (la mayoría de los casos).
- **`sort_by`**: cuando se necesita un orden personalizado que no
  coincide con el `Ord` del tipo (descendente, parcial, etc.).
- **`sort_by_key`**: cuando el criterio es un campo o transformación
  del elemento — más ergonómico que `sort_by`.

---

## sort: Timsort estable

### Uso básico con Ord

```rust
let mut nums = vec![42, 7, -3, 99, 0, 15];
nums.sort();
// nums = [-3, 0, 7, 15, 42, 99]
```

`sort()` requiere que `T: Ord` — el tipo debe implementar orden
total. Los tipos primitivos enteros (`i32`, `u64`, etc.), `char`,
`String`, `&str`, `bool` y tuplas implementan `Ord`.

Los tipos flotantes (`f32`, `f64`) **no** implementan `Ord` porque
`NaN` viola la transitividad. `NaN != NaN` y `NaN` no es mayor ni
menor que ningún valor. Esto hace imposible definir un orden total
consistente.

```rust
let mut floats = vec![3.14, 1.41, 2.72];
// floats.sort();  // ERROR: f64 no implementa Ord
```

### Algoritmo interno: Timsort

La implementación de `sort()` en Rust usa una variante de **Timsort**,
el algoritmo diseñado por Tim Peters para Python en 2002:

1. **Detectar runs**: secuencias ya ordenadas (ascendentes o
   descendentes) en el input. Los runs descendentes se invierten.
2. **Extender runs cortos**: si un run natural es menor que un mínimo
   (típicamente 32), se extiende con insertion sort hasta alcanzar
   ese mínimo.
3. **Merge progresivo**: fusionar runs adyacentes manteniendo un
   invariante de tamaño en un stack de runs pendientes (similar a
   las reglas de merge de Fibonacci).

Propiedades:

- **Adaptativo**: $O(n)$ para datos ya ordenados o casi-ordenados.
- **Estable**: el merge preserva el orden relativo.
- **Espacio**: $O(n)$ en el peor caso para el buffer de merge.
- **Peor caso**: $O(n \log n)$ comparaciones siempre.

El Timsort de Rust difiere del de Python en detalles de implementación
(usa `unsafe` para evitar copias innecesarias durante el merge), pero
el algoritmo conceptual es el mismo.

### sort_by: comparador personalizado

```rust
let mut nums = vec![42, 7, -3, 99, 0, 15];

// Orden descendente
nums.sort_by(|a, b| b.cmp(a));
// nums = [99, 42, 15, 7, 0, -3]
```

La closure recibe dos referencias `&T` y debe retornar
`std::cmp::Ordering`, que tiene tres variantes:

```rust
enum Ordering {
    Less,    // a va antes que b
    Equal,   // equivalentes
    Greater, // a va después que b
}
```

El método `.cmp()` de tipos que implementan `Ord` retorna `Ordering`.
Para invertir el orden, basta con invertir los argumentos: `b.cmp(a)`.

### sort_by con flotantes

Para ordenar `f64`, se usa `sort_by` con `partial_cmp` y un manejo
explícito de `NaN`:

```rust
let mut vals = vec![3.14, 1.41, f64::NAN, 2.72, 0.58];

// Opción 1: NaN al final (unwrap_or con Less/Greater)
vals.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Less));

// Opción 2: Usar total_cmp (desde Rust 1.62)
vals.sort_by(|a, b| a.total_cmp(b));
```

`total_cmp` define un orden total para IEEE 754: trata los bits del
float como un entero con signo, poniendo `-NaN < -∞ < ... < 0 < ... < ∞ < NaN`.
Es la opción recomendada desde Rust 1.62.

### sort_by_key: extraer clave

```rust
let mut words = vec!["banana", "fig", "apple", "cherry"];

// Ordenar por longitud
words.sort_by_key(|w| w.len());
// words = ["fig", "apple", "banana", "cherry"]
```

`sort_by_key` recibe una closure `F: FnMut(&T) -> K` donde `K: Ord`.
Es equivalente a `sort_by(|a, b| f(a).cmp(&f(b)))` pero más legible
y permite optimizaciones internas (la clave se calcula una vez por
comparación, no dos).

---

## sort_unstable: pdqsort

### Uso básico

```rust
let mut nums = vec![42, 7, -3, 99, 0, 15];
nums.sort_unstable();
// nums = [-3, 0, 7, 15, 42, 99]
```

La interfaz es idéntica a `sort`. La diferencia es interna:

### Algoritmo interno: pdqsort

**Pattern-defeating quicksort** (Orson Peters, 2021) es un híbrido
de tres algoritmos:

1. **Quicksort con block partition**: partición que evita branch
   mispredictions acumulando decisiones en bloques antes de mover
   elementos.
2. **Insertion sort para subarrays pequeños**: cuando la partición
   produce segmentos menores que ~24 elementos.
3. **Heapsort como fallback**: si la profundidad de recursión excede
   $2 \lfloor \log_2 n \rfloor$, cambia a heapsort para garantizar
   $O(n \log n)$.

Detección de patrones:

- Si una partición produce un pivote que no divide el array (todos
  los elementos son iguales al pivote), pdqsort cambia a **Dutch
  National Flag** (partición de tres vías).
- Si las particiones están consistentemente desbalanceadas (señal de
  input adversarial), **baraja** (shuffle) un subconjunto de elementos
  para romper el patrón antes de recurrir a heapsort.
- Si detecta que el input está **casi ordenado**, aprovecha runs
  existentes.

Propiedades:

- **No estable**: puede reordenar elementos iguales.
- **In-place**: $O(\log n)$ espacio (stack de recursión).
- **Peor caso**: $O(n \log n)$ garantizado (fallback a heapsort).
- **Mejor caso práctico**: más rápido que Timsort para datos
  aleatorios (~15-30% menos tiempo).

### sort_unstable_by y sort_unstable_by_key

```rust
// Descendente
nums.sort_unstable_by(|a, b| b.cmp(a));

// Por clave
students.sort_unstable_by_key(|s| s.age);
```

Misma interfaz que las versiones estables.

---

## Closures como comparadores

En C, el comparador es un puntero a función con `void *`:

```c
int cmp(const void *a, const void *b) {
    return *(int *)a - *(int *)b;  /* casting manual, posible overflow */
}
qsort(arr, n, sizeof(int), cmp);
```

En Rust, el comparador es una closure con tipos concretos:

```rust
arr.sort_by(|a, b| a.cmp(b));
```

Las diferencias son fundamentales:

| Aspecto | C (`qsort`) | Rust (`sort_by`) |
|---------|-------------|------------------|
| Tipo de los args | `const void *` (borrado) | `&T` (concreto) |
| Cast | Manual, sin verificación | No necesario |
| Overflow | Posible si se usa resta | Imposible (`.cmp()` retorna `Ordering`) |
| Inlining | Imposible (puntero a fn) | Garantizado (monomorphization) |
| Captura de estado | No (solo fn pura, o hack con globals) | Sí (closures capturan variables) |
| Error de tipo | Runtime (UB) | Compilación |

### Closures con estado capturado

Una ventaja exclusiva de las closures es que pueden **capturar
variables del entorno**:

```rust
let threshold = 50;
let mut data = vec![30, 80, 10, 60, 40, 90, 20, 70];

// Ordenar poniendo valores >= threshold primero, luego por valor
data.sort_by(|a, b| {
    let a_above = *a >= threshold;
    let b_above = *b >= threshold;

    match (a_above, b_above) {
        (true, false) => std::cmp::Ordering::Less,
        (false, true) => std::cmp::Ordering::Greater,
        _ => a.cmp(b),
    }
});
// data = [50, 60, 70, 80, 90, 10, 20, 30, 40]  (>= 50 primero)
```

En C, esto requeriría una variable global o usar la extensión no
estándar `qsort_r` (que acepta un argumento extra de contexto).

### El trait Ord y PartialOrd

Rust distingue entre orden parcial y total:

```rust
trait PartialOrd: PartialEq {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering>;
}

trait Ord: Eq + PartialOrd {
    fn cmp(&self, other: &Self) -> Ordering;
}
```

- `PartialOrd`: puede retornar `None` (no comparables). `f32` y `f64`
  implementan `PartialOrd` pero no `Ord` porque `NaN.partial_cmp(x)`
  retorna `None`.
- `Ord`: siempre retorna un `Ordering` — orden total garantizado.
  `sort()` requiere `Ord` porque un orden parcial puede hacer que el
  algoritmo de sort no termine o produzca resultados inconsistentes.

### Implementar Ord para tipos propios

```rust
#[derive(Eq, PartialEq)]
struct Student {
    name: String,
    age: u32,
    gpa: f64,
}

impl PartialOrd for Student {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Student {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // Por GPA descendente, luego nombre ascendente
        other.gpa
            .total_cmp(&self.gpa)          // GPA desc (invertido)
            .then(self.name.cmp(&other.name))  // nombre asc
    }
}
```

El método `.then()` de `Ordering` encadena criterios: si el primero
es `Equal`, usa el segundo. Esto es el equivalente de la cadena de
`if` en los comparadores multi-campo de C, pero más conciso y
composicional.

### Derive para orden simple

Para structs donde el orden natural es lexicográfico por campos (en
el orden de declaración):

```rust
#[derive(PartialEq, Eq, PartialOrd, Ord)]
struct Point {
    x: i32,
    y: i32,
}
```

Con `derive`, `Point` se ordena primero por `x`, luego por `y`. Si
el orden natural no es el deseado, hay que implementar `Ord`
manualmente.

---

## Reverse y encadenamiento

### Orden inverso con Reverse

```rust
use std::cmp::Reverse;

let mut nums = vec![42, 7, -3, 99, 0];

// sort_by_key con Reverse para descendente
nums.sort_by_key(|&x| Reverse(x));
// nums = [99, 42, 7, 0, -3]
```

`Reverse` es un wrapper en la stdlib que invierte el `Ord` del tipo
envuelto. Es especialmente útil con `sort_by_key` donde no se puede
simplemente invertir argumentos:

```rust
// Ordenar strings por longitud descendente
let mut words = vec!["apple", "fig", "cherry", "banana"];
words.sort_by_key(|w| Reverse(w.len()));
// words = ["cherry", "banana", "apple", "fig"]
```

### Encadenar ordenamientos estables

Porque `sort` es estable, se pueden encadenar llamadas para lograr
ordenamiento multi-campo:

```rust
#[derive(Debug)]
struct Task { priority: u32, name: String }

let mut tasks = vec![
    Task { priority: 2, name: "Review".into() },
    Task { priority: 1, name: "Fix bug".into() },
    Task { priority: 2, name: "Docs".into() },
    Task { priority: 1, name: "Deploy".into() },
];

// Primero por nombre (será el criterio secundario)
tasks.sort_by(|a, b| a.name.cmp(&b.name));
// Luego por prioridad (será el criterio primario)
tasks.sort_by_key(|t| t.priority);
```

Después de ambos sorts, las tareas están ordenadas por prioridad y,
dentro de cada prioridad, por nombre. Esto funciona **solo** porque
`sort` es estable — el segundo sort preserva el orden del primero
para elementos con la misma prioridad.

Con `sort_unstable`, este patrón **no funciona** — el segundo sort
puede destruir el orden establecido por el primero.

---

## Métodos relacionados

### is_sorted

```rust
let a = vec![1, 2, 3, 4, 5];
assert!(a.is_sorted());

let b = vec![1, 3, 2];
assert!(!b.is_sorted());
```

Verifica en $O(n)$ si el slice está ordenado. Útil como precondición
o en tests.

### select_nth_unstable

```rust
let mut arr = vec![42, 7, -3, 99, 0, 15];
let (left, median, right) = arr.select_nth_unstable(2);
// El elemento en posición 2 es el que estaría ahí si el array estuviera ordenado
// left contiene elementos <= median, right contiene elementos >= median
// median = &mut 7  (el tercer menor)
```

Es **quickselect**: encuentra el k-ésimo menor en $O(n)$ promedio sin
ordenar todo el array. El equivalente en C sería implementar
quickselect manualmente — la stdlib de C no lo ofrece.

### sort_by con tuplas para multi-campo

Una alternativa concisa a implementar `Ord`:

```rust
students.sort_by(|a, b| {
    (Reverse(a.gpa.total_cmp(&b.gpa)), &a.name)
        .cmp(&(Reverse(b.gpa.total_cmp(&a.gpa)), &b.name))
});
```

O más limpio con `sort_by_key` y tuplas:

```rust
// Ordenar por edad asc, luego nombre asc
students.sort_by_key(|s| (s.age, &s.name));

// Ordenar por GPA desc, luego nombre asc (con Reverse)
students.sort_by_key(|s| (Reverse(s.gpa_as_ord()), &s.name));
```

Las tuplas implementan `Ord` lexicográficamente si todos sus
componentes implementan `Ord`, haciendo trivial el ordenamiento
multi-campo.

---

## Programa completo en Rust

```rust
// sort_demo.rs — Demostración completa de ordenamiento en Rust
use std::cmp::{Ordering, Reverse};

#[derive(Debug, Clone)]
struct Student {
    name: String,
    age: u32,
    gpa: f64,
}

fn main() {
    // Demo 1: sort básico (Ord)
    println!("=== Demo 1: sort básico ===");
    let mut nums = vec![42, 7, -3, 99, 0, 15, -8, 23];
    println!("Antes:   {:?}", nums);
    nums.sort();
    println!("Después: {:?}", nums);

    // Demo 2: sort_unstable
    println!("\n=== Demo 2: sort_unstable ===");
    let mut nums2 = vec![42, 7, -3, 99, 0, 15, -8, 23];
    println!("Antes:   {:?}", nums2);
    nums2.sort_unstable();
    println!("Después: {:?}", nums2);

    // Demo 3: sort_by descendente
    println!("\n=== Demo 3: sort_by descendente ===");
    let mut nums3 = vec![42, 7, -3, 99, 0, 15, -8, 23];
    println!("Antes:   {:?}", nums3);
    nums3.sort_by(|a, b| b.cmp(a));
    println!("Después: {:?}", nums3);

    // Demo 4: sort_by_key por longitud de string
    println!("\n=== Demo 4: sort_by_key ===");
    let mut words = vec!["mango", "apple", "cherry", "banana", "fig"];
    println!("Antes:   {:?}", words);
    words.sort_by_key(|w| w.len());
    println!("Después: {:?}", words);

    // Demo 5: Flotantes con total_cmp
    println!("\n=== Demo 5: Flotantes con total_cmp ===");
    let mut floats = vec![3.14, 1.41, f64::NAN, 2.72, 0.58, f64::NEG_INFINITY];
    println!("Antes:   {:?}", floats);
    floats.sort_by(|a, b| a.total_cmp(b));
    println!("Después: {:?}", floats);

    // Demo 6: Structs multi-campo
    println!("\n=== Demo 6: Structs (GPA desc, nombre asc) ===");
    let mut students = vec![
        Student { name: "Carlos".into(),  age: 20, gpa: 3.50 },
        Student { name: "Ana".into(),     age: 22, gpa: 3.90 },
        Student { name: "Beatriz".into(), age: 21, gpa: 3.50 },
        Student { name: "David".into(),   age: 23, gpa: 3.90 },
        Student { name: "Elena".into(),   age: 20, gpa: 3.20 },
    ];

    println!("Antes:");
    for s in &students {
        println!("  {:<10} age={} gpa={:.2}", s.name, s.age, s.gpa);
    }

    students.sort_by(|a, b| {
        b.gpa.total_cmp(&a.gpa)                  // GPA descendente
            .then_with(|| a.name.cmp(&b.name))    // nombre ascendente
    });

    println!("Después:");
    for s in &students {
        println!("  {:<10} age={} gpa={:.2}", s.name, s.age, s.gpa);
    }

    // Demo 7: Estabilidad
    println!("\n=== Demo 7: Estabilidad de sort ===");
    let mut pairs: Vec<(i32, &str)> = vec![
        (2, "B1"), (1, "A1"), (2, "B2"), (1, "A2"), (3, "C1"),
    ];
    println!("Antes:   {:?}", pairs);

    // Ordenar solo por el primer campo
    pairs.sort_by_key(|p| p.0);
    println!("Después: {:?}", pairs);
    println!("Nota: (1,A1) antes de (1,A2) y (2,B1) antes de (2,B2) — estable");

    // Demo 8: Reverse wrapper
    println!("\n=== Demo 8: Reverse ===");
    let mut nums4 = vec![42, 7, -3, 99, 0, 15];
    println!("Antes:   {:?}", nums4);
    nums4.sort_by_key(|&x| Reverse(x));
    println!("Después: {:?}", nums4);

    // Demo 9: select_nth_unstable (quickselect)
    println!("\n=== Demo 9: select_nth_unstable ===");
    let mut data = vec![42, 7, -3, 99, 0, 15, -8, 23];
    println!("Input: {:?}", data);

    let k = 3;
    let (left, median, right) = data.select_nth_unstable(k);
    println!("Mediana en posición {}: {}", k, median);
    println!("  Menores o iguales: {:?}", left);
    println!("  Mayores o iguales: {:?}", right);

    // Demo 10: Closure con estado capturado
    println!("\n=== Demo 10: Closure con captura ===");
    let priority_order = ["critical", "high", "medium", "low"];
    let mut tickets = vec!["low", "critical", "medium", "high", "low", "critical"];

    println!("Antes:   {:?}", tickets);

    tickets.sort_by_key(|ticket| {
        priority_order.iter().position(|&p| p == *ticket).unwrap_or(usize::MAX)
    });

    println!("Después: {:?}", tickets);

    // Demo 11: is_sorted
    println!("\n=== Demo 11: is_sorted ===");
    let sorted = vec![1, 2, 3, 4, 5];
    let unsorted = vec![1, 3, 2, 4];
    println!("[1,2,3,4,5] is_sorted: {}", sorted.is_sorted());
    println!("[1,3,2,4]   is_sorted: {}", unsorted.is_sorted());
}
```

### Compilar y ejecutar

```bash
rustc -o sort_demo sort_demo.rs
./sort_demo
```

### Salida esperada

```
=== Demo 1: sort básico ===
Antes:   [42, 7, -3, 99, 0, 15, -8, 23]
Después: [-8, -3, 0, 7, 15, 23, 42, 99]

=== Demo 2: sort_unstable ===
Antes:   [42, 7, -3, 99, 0, 15, -8, 23]
Después: [-8, -3, 0, 7, 15, 23, 42, 99]

=== Demo 3: sort_by descendente ===
Antes:   [42, 7, -3, 99, 0, 15, -8, 23]
Después: [99, 42, 23, 15, 7, 0, -3, -8]

=== Demo 4: sort_by_key ===
Antes:   ["mango", "apple", "cherry", "banana", "fig"]
Después: ["fig", "mango", "apple", "banana", "cherry"]

=== Demo 5: Flotantes con total_cmp ===
Antes:   [3.14, 1.41, NaN, 2.72, 0.58, -inf]
Después: [-inf, 0.58, 1.41, 2.72, 3.14, NaN]

=== Demo 6: Structs (GPA desc, nombre asc) ===
Antes:
  Carlos     age=20 gpa=3.50
  Ana        age=22 gpa=3.90
  Beatriz    age=21 gpa=3.50
  David      age=23 gpa=3.90
  Elena      age=20 gpa=3.20
Después:
  Ana        age=22 gpa=3.90
  David      age=23 gpa=3.90
  Beatriz    age=21 gpa=3.50
  Carlos     age=20 gpa=3.50
  Elena      age=20 gpa=3.20

=== Demo 7: Estabilidad de sort ===
Antes:   [(2, "B1"), (1, "A1"), (2, "B2"), (1, "A2"), (3, "C1")]
Después: [(1, "A1"), (1, "A2"), (2, "B1"), (2, "B2"), (3, "C1")]
Nota: (1,A1) antes de (1,A2) y (2,B1) antes de (2,B2) — estable

=== Demo 8: Reverse ===
Antes:   [42, 7, -3, 99, 0, 15]
Después: [99, 42, 15, 7, 0, -3]

=== Demo 9: select_nth_unstable ===
Input: [42, 7, -3, 99, 0, 15, -8, 23]
Mediana en posición 3: 7
  Menores o iguales: [-8, -3, 0]
  Mayores o iguales: [15, 42, 99, 23]

=== Demo 10: Closure con captura ===
Antes:   ["low", "critical", "medium", "high", "low", "critical"]
Después: ["critical", "critical", "high", "medium", "low", "low"]

=== Demo 11: is_sorted ===
[1,2,3,4,5] is_sorted: true
[1,3,2,4]   is_sorted: false
```

---

## Monomorphization: por qué Rust es más rápido

Cuando se escribe `nums.sort()`, el compilador genera una versión
**completa y especializada** de Timsort para `i32`. La closure o el
`.cmp()` se inlinea directamente en el código del sort. No hay
llamada indirecta, no hay puntero a función, no hay cast.

El proceso:

```
// Código fuente (genérico)
impl<T: Ord> [T] {
    pub fn sort(&mut self) { ... }
}

// El compilador genera (especializado):
fn sort_i32(slice: &mut [i32]) {
    // Timsort con comparación directa:
    // if a < b  (una instrucción)
    // sin llamada a función, sin void *
}

fn sort_string(slice: &mut [String]) {
    // Timsort con strcmp inlineado
}
```

Esto es equivalente a escribir un sort manual para cada tipo en C,
pero sin el esfuerzo de mantener múltiples implementaciones. El costo
es mayor tamaño del binario (una copia del sort por tipo usado), pero
el rendimiento es óptimo.

En C, `qsort` tiene **una sola copia** del código que funciona para
todos los tipos, pero cada comparación pasa por una llamada indirecta.
El trade-off es: C optimiza tamaño del binario, Rust optimiza velocidad.

---

## Ejercicios

### Ejercicio 1 — sort vs sort_unstable: estabilidad

Dado este array de tuplas:

```rust
let mut data = vec![(2, "B"), (1, "A"), (2, "C"), (1, "D")];
```

Ordena por el primer campo usando `sort_by_key` y luego repite con
`sort_unstable_by_key`. ¿El resultado es siempre el mismo?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
// Versión estable
data.sort_by_key(|p| p.0);
// Resultado garantizado: [(1,"A"), (1,"D"), (2,"B"), (2,"C")]
// A antes de D y B antes de C — preserva orden original

// Versión inestable
data.sort_unstable_by_key(|p| p.0);
// Resultado posible: [(1,"D"), (1,"A"), (2,"C"), (2,"B")]
// El orden dentro de cada grupo NO está garantizado
```

Con `sort_by_key` (estable), los elementos con la misma clave
mantienen su orden relativo original: `(1,"A")` aparecía antes que
`(1,"D")` en el input, así que aparece antes en el output.

Con `sort_unstable_by_key`, dentro de cada grupo el orden es
arbitrario. El resultado puede coincidir con el estable o no —
depende de la implementación y del input.

</details>

### Ejercicio 2 — Ordenar f64 sin NaN

¿Cómo ordenar un `Vec<f64>` que sabes que **no contiene NaN**?
Muestra dos formas: con `sort_by` y con `sort_by_key`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let mut vals = vec![3.14, 1.41, 2.72, 0.58];

// Forma 1: sort_by con total_cmp
vals.sort_by(|a, b| a.total_cmp(b));

// Forma 2: sort_by con unwrap (seguro porque no hay NaN)
vals.sort_by(|a, b| a.partial_cmp(b).unwrap());

// Forma 3: Convertir a OrderedFloat o wrapper con Ord
// (requiere crate externo)
```

`total_cmp` es la opción más robusta — funciona correctamente
incluso si un `NaN` se cuela inesperadamente (lo coloca al final).

`partial_cmp().unwrap()` paniquea si encuentra un `NaN`, lo que
puede ser deseable como validación: si no debería haber `NaN`, es
mejor fallar ruidosamente que producir un resultado silenciosamente
incorrecto (como hace C con un comparador que retorna 0 para `NaN`).

Resultado en ambos casos: `[0.58, 1.41, 2.72, 3.14]`

</details>

### Ejercicio 3 — Comparador multi-campo con .then()

Ordena este array de structs por `department` ascendente y, dentro
de cada departamento, por `salary` descendente:

```rust
struct Employee { name: String, department: String, salary: u32 }
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
employees.sort_by(|a, b| {
    a.department.cmp(&b.department)
        .then(b.salary.cmp(&a.salary))  // salary desc: b antes que a
});
```

Ejemplo con datos:

```rust
let mut employees = vec![
    Employee { name: "Ana".into(),    department: "Eng".into(),   salary: 90000 },
    Employee { name: "Bob".into(),    department: "Eng".into(),   salary: 85000 },
    Employee { name: "Carol".into(),  department: "Sales".into(), salary: 95000 },
    Employee { name: "Dan".into(),    department: "Eng".into(),   salary: 95000 },
    Employee { name: "Eve".into(),    department: "Sales".into(), salary: 80000 },
];
```

Resultado:

```
Eng    Dan    95000
Eng    Ana    90000
Eng    Bob    85000
Sales  Carol  95000
Sales  Eve    80000
```

`.then()` retorna el segundo `Ordering` solo si el primero es `Equal`.
Para invertir un campo específico (salary descendente), se invierten
los argumentos solo en ese `.cmp()`.

</details>

### Ejercicio 4 — Reverse con sort_by_key

Ordena un vector de strings primero por longitud descendente y, ante
empate en longitud, alfabéticamente ascendente.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let mut words = vec!["fig", "apple", "cherry", "banana", "kiwi", "date"];

words.sort_by(|a, b| {
    b.len().cmp(&a.len())          // longitud desc
        .then(a.cmp(b))            // alfabético asc
});
```

Resultado:

```
["banana", "cherry", "apple", "date", "kiwi", "fig"]
```

- Longitud 6: "banana", "cherry" (b < c alfabéticamente)
- Longitud 5: "apple"
- Longitud 4: "date", "kiwi" (d < k)
- Longitud 3: "fig"

Con `sort_by_key` y tuplas:

```rust
words.sort_by_key(|w| (Reverse(w.len()), *w));
```

Esto usa el `Ord` lexicográfico de tuplas: primero compara
`Reverse(len)` (longitud descendente), luego `w` (string ascendente).

</details>

### Ejercicio 5 — select_nth_unstable para mediana

Usa `select_nth_unstable` para encontrar la mediana de un vector de
enteros sin ordenar todo el array. ¿Cuál es la complejidad?

```rust
let mut data = vec![42, 7, -3, 99, 0, 15, -8, 23, 50];
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let n = data.len();
let mid = n / 2;

let (_, median, _) = data.select_nth_unstable(mid);
println!("Mediana: {}", median);
```

El array tiene 9 elementos, así que `mid = 4`. Después de
`select_nth_unstable(4)`, el elemento en posición 4 es el quinto
menor, que es la mediana.

Valores ordenados: `[-8, -3, 0, 7, 15, 23, 42, 50, 99]`
Mediana (posición 4): `15`

Complejidad: $O(n)$ en promedio — es quickselect, que solo particiona
recursivamente el lado que contiene la posición buscada. Comparado
con ordenar ($O(n \log n)$) y luego tomar el elemento central, es
significativamente más rápido para arrays grandes.

Para mediana de array de tamaño par (dos valores centrales), se
necesitan dos llamadas o usar el hecho de que después de
`select_nth_unstable(mid)`, los elementos en `left` son todos
menores, y el máximo de `left` es el otro valor central.

</details>

### Ejercicio 6 — Trait Ord manual

Implementa `Ord` para este struct, ordenando por `score` descendente
y, ante empate, por `name` ascendente:

```rust
struct Player { name: String, score: u32 }
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
impl PartialEq for Player {
    fn eq(&self, other: &Self) -> bool {
        self.score == other.score && self.name == other.name
    }
}

impl Eq for Player {}

impl PartialOrd for Player {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Player {
    fn cmp(&self, other: &Self) -> Ordering {
        other.score.cmp(&self.score)         // score descendente
            .then(self.name.cmp(&other.name)) // name ascendente
    }
}
```

Ahora se puede usar `sort()` directamente:

```rust
let mut players = vec![
    Player { name: "Alice".into(), score: 100 },
    Player { name: "Bob".into(),   score: 150 },
    Player { name: "Carol".into(), score: 100 },
];

players.sort();
// [Bob(150), Alice(100), Carol(100)]
```

Nota: `PartialOrd` delega a `Ord` con `Some(self.cmp(other))` — es
el patrón estándar cuando se implementa `Ord`. Nunca retorna `None`
porque el orden es total.

Se necesitan los cuatro traits: `PartialEq`, `Eq`, `PartialOrd`, `Ord`.
`Eq` y `PartialEq` se pueden derivar con `#[derive]` si todos los
campos los implementan.

</details>

### Ejercicio 7 — Encadenamiento de sorts estables

Dado un array de tareas con prioridad y categoría, logra el
ordenamiento "por categoría asc, dentro de categoría por prioridad
asc" usando **dos llamadas** a `sort_by_key` (sin comparador
multi-campo).

```rust
let mut tasks = vec![
    ("low",  "backend"),
    ("high", "frontend"),
    ("low",  "frontend"),
    ("high", "backend"),
];
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
// Paso 1: ordenar por criterio SECUNDARIO (prioridad)
tasks.sort_by_key(|t| match t.0 {
    "high" => 0,
    "medium" => 1,
    "low" => 2,
    _ => 3,
});

// Paso 2: ordenar por criterio PRIMARIO (categoría)
tasks.sort_by_key(|t| t.1);
```

Resultado:

```
[("high", "backend"), ("low", "backend"), ("high", "frontend"), ("low", "frontend")]
```

El truco es ordenar en **orden inverso de importancia**: primero el
criterio secundario, luego el primario. El sort estable preserva el
orden del criterio secundario dentro de cada grupo del primario.

Si usáramos `sort_unstable_by_key` en el paso 2, el orden de
prioridad dentro de cada categoría podría perderse — la estabilidad
es esencial para esta técnica.

</details>

### Ejercicio 8 — Closure con captura mutable

¿Es posible contar cuántas comparaciones realiza `sort_by`? Escribe
una closure que cuente las llamadas.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let mut nums = vec![42, 7, -3, 99, 0, 15, -8, 23];
let mut count = 0u64;

nums.sort_by(|a, b| {
    count += 1;
    a.cmp(b)
});

println!("Comparaciones: {}", count);
println!("Resultado: {:?}", nums);
```

Salida típica:

```
Comparaciones: ~17-20  (para 8 elementos con Timsort)
Resultado: [-8, -3, 0, 7, 15, 23, 42, 99]
```

Esto funciona porque `sort_by` acepta `FnMut`, no solo `Fn`. La
closure puede mutar su entorno capturado (el contador). En C esto es
imposible con `qsort` porque el comparador es un puntero a función
pura sin estado — se necesitaría una variable global, que no es
thread-safe.

El número exacto de comparaciones depende del algoritmo (Timsort) y
del input. Para $n$ elementos aleatorios, Timsort hace
aproximadamente $n \log_2 n$ comparaciones.

</details>

### Ejercicio 9 — Ordenar por campo calculado eficientemente

Ordena un vector de strings por la cantidad de vocales. ¿Qué
diferencia hay entre `sort_by_key` y `sort_by` en eficiencia para
esta operación?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
fn count_vowels(s: &str) -> usize {
    s.chars().filter(|c| "aeiouAEIOU".contains(*c)).count()
}

let mut words = vec!["rhythm", "aeiou", "hello", "sky", "queue"];

// Opción 1: sort_by_key — recalcula la clave en cada comparación
words.sort_by_key(|w| count_vowels(w));

// Opción 2: sort_by — también recalcula
words.sort_by(|a, b| count_vowels(a).cmp(&count_vowels(b)));
```

Ambas opciones recalculan `count_vowels` en cada comparación. Para
$n$ elementos con $O(n \log n)$ comparaciones, la función se llama
$O(n \log n)$ veces.

Para evitar el recálculo, se usa el **patrón Schwartzian transform**
(decorate-sort-undecorate):

```rust
let mut decorated: Vec<(usize, &str)> = words.iter()
    .map(|w| (count_vowels(w), *w))
    .collect();

decorated.sort_by_key(|&(vowels, _)| vowels);

let result: Vec<&str> = decorated.into_iter()
    .map(|(_, w)| w)
    .collect();
// result = ["rhythm", "sky", "hello", "queue", "aeiou"]
```

Ahora `count_vowels` se llama solo $n$ veces (en el `map`), no
$O(n \log n)$. Para claves costosas de calcular, esta optimización
es significativa.

Resultado (por vocales): `rhythm`(0), `sky`(0), `hello`(2),
`queue`(3), `aeiou`(5).

</details>

### Ejercicio 10 — sort vs sort_unstable: benchmark

Escribe un programa que compare el rendimiento de `sort` vs
`sort_unstable` para $10^6$ enteros aleatorios, ordenados y
casi-ordenados.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
use std::time::Instant;

fn benchmark<F: FnMut()>(label: &str, mut f: F) {
    let start = Instant::now();
    f();
    let elapsed = start.elapsed();
    println!("  {:<25} {:>8.2} ms", label, elapsed.as_secs_f64() * 1000.0);
}

fn main() {
    let n: usize = 1_000_000;

    // Generar datos
    let random: Vec<i32> = (0..n as i32).map(|i| i.wrapping_mul(2654435761u32 as i32)).collect();
    let sorted: Vec<i32> = (0..n as i32).collect();
    let mut almost: Vec<i32> = (0..n as i32).collect();
    // Perturbar 1% de las posiciones
    for i in (0..n).step_by(100) {
        if i + 1 < n { almost.swap(i, i + 1); }
    }

    for (label, data) in [("Aleatorio", &random), ("Ordenado", &sorted), ("Casi-ordenado", &almost)] {
        println!("\n--- {} ({} elementos) ---", label, n);

        let mut copy1 = data.clone();
        benchmark("sort (Timsort)", || copy1.sort());

        let mut copy2 = data.clone();
        benchmark("sort_unstable (pdqsort)", || copy2.sort_unstable());
    }
}
```

Resultados típicos (x86-64, release):

```
--- Aleatorio (1000000 elementos) ---
  sort (Timsort)              ~85 ms
  sort_unstable (pdqsort)     ~55 ms

--- Ordenado (1000000 elementos) ---
  sort (Timsort)               ~3 ms
  sort_unstable (pdqsort)      ~3 ms

--- Casi-ordenado (1000000 elementos) ---
  sort (Timsort)               ~8 ms
  sort_unstable (pdqsort)     ~10 ms
```

Observaciones:

- **Aleatorio**: `sort_unstable` es ~30-40% más rápido (in-place,
  sin allocación de merge buffer).
- **Ordenado**: ambos son $O(n)$ — Timsort detecta el run completo,
  pdqsort detecta particiones triviales.
- **Casi-ordenado**: Timsort puede ser más rápido porque detecta runs
  naturales y los fusiona eficientemente. pdqsort no aprovecha runs
  tan bien.

La regla general: usar `sort_unstable` para datos aleatorios donde la
estabilidad no importa, `sort` cuando se necesita estabilidad o el
input tiene estructura (casi-ordenado, runs parciales).

</details>

---

## Resumen

| Aspecto | `sort` | `sort_unstable` |
|---------|--------|-----------------|
| Algoritmo | Timsort (merge sort adaptativo) | pdqsort (quicksort híbrido) |
| Estable | Sí | No |
| Espacio | $O(n)$ | $O(\log n)$ |
| Peor caso | $O(n \log n)$ | $O(n \log n)$ |
| Datos aleatorios | Más lento (~30-40%) | Más rápido |
| Datos ordenados | $O(n)$ — adaptativo | $O(n)$ — detecta patrón |
| Variantes | `sort_by`, `sort_by_key` | `sort_unstable_by`, `sort_unstable_by_key` |
| Comparador | Closure `FnMut(&T, &T) -> Ordering` | Igual |
| Inlining | Sí (monomorphization) | Sí |
| Equivalente C | — | — |
| Ventaja sobre C | Tipo seguro, sin cast, sin overhead de llamada indirecta | Igual |
