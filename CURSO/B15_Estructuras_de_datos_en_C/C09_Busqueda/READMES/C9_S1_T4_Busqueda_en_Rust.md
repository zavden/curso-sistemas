# Búsqueda en Rust

## Objetivo

Dominar la API de búsqueda de la biblioteca estándar de Rust, que
ofrece un conjunto de métodos ergonómicos, type-safe y de alto
rendimiento:

- **`binary_search`**: retorna `Result<usize, usize>` — encontrado
  o punto de inserción.
- **`partition_point`**: equivalente a `lower_bound`/`upper_bound`
  de C++.
- **`contains`**: existencia simple en $O(n)$.
- **`Iterator::find`**, **`position`**, **`any`**: búsqueda
  secuencial con predicados arbitrarios.
- Comparación con `bsearch` de C.

---

## binary_search: búsqueda binaria en slices

### Firma

```rust
impl<T: Ord> [T] {
    pub fn binary_search(&self, x: &T) -> Result<usize, usize>;
}
```

Retorna un `Result`:

| Variante | Significado |
|----------|-------------|
| `Ok(index)` | Encontrado en `index` |
| `Err(index)` | No encontrado; `index` es donde se insertaría manteniendo el orden |

Este diseño es superior al `-1` de C o al `NULL` de `bsearch`:
el caso de "no encontrado" da información **útil** (el punto de
inserción), y el sistema de tipos obliga al llamador a manejar
ambos casos.

### Uso básico

```rust
let arr = [3, 7, 11, 15, 23, 31, 42];

match arr.binary_search(&23) {
    Ok(i)  => println!("Encontrado en posición {}", i),   // Ok(4)
    Err(i) => println!("No encontrado, insertar en {}", i),
}

match arr.binary_search(&20) {
    Ok(i)  => println!("Encontrado en posición {}", i),
    Err(i) => println!("No encontrado, insertar en {}", i), // Err(4)
}
```

### Duplicados

Con elementos duplicados, `binary_search` retorna **alguno** de los
índices con el valor buscado — no necesariamente el primero ni el
último:

```rust
let arr = [1, 3, 3, 3, 5, 7];
let result = arr.binary_search(&3);
// Puede ser Ok(1), Ok(2), u Ok(3) — no especificado
```

Para encontrar la primera o última ocurrencia, usar `partition_point`.

### Complejidad

$O(\log n)$ comparaciones. Internamente usa la misma lógica que la
búsqueda binaria iterativa con `lo + (hi - lo) / 2` — sin bug de
overflow porque `usize` no desborda para sumas de índices válidos.

---

## binary_search_by: comparador personalizado

```rust
impl<T> [T] {
    pub fn binary_search_by<F>(&self, f: F) -> Result<usize, usize>
    where
        F: FnMut(&T) -> Ordering;
}
```

La closure recibe un **solo** elemento (no dos) y retorna `Ordering`
respecto al valor buscado. Este diseño puede ser contraintuitivo —
la closure responde "¿este elemento es menor, igual, o mayor que lo
que busco?"

```rust
use std::cmp::Ordering;

let arr = [3, 7, 11, 15, 23, 31, 42];

// Buscar 23 con comparador explícito
let result = arr.binary_search_by(|elem| elem.cmp(&23));
// Ok(4)

// Orden invertido
let desc = [42, 31, 23, 15, 11, 7, 3];
let result = desc.binary_search_by(|elem| 23.cmp(elem));
// Ok(2) — nótese los argumentos invertidos
```

### Buscar en structs por un campo

```rust
struct Student { name: String, age: u32 }

let students = vec![
    Student { name: "Ana".into(),    age: 20 },
    Student { name: "Bob".into(),    age: 22 },
    Student { name: "Carol".into(),  age: 25 },
];

// Buscar por age (asumiendo ordenado por age)
let result = students.binary_search_by(|s| s.age.cmp(&22));
// Ok(1)
```

---

## binary_search_by_key: buscar por clave extraída

```rust
impl<T> [T] {
    pub fn binary_search_by_key<B, F>(&self, b: &B, f: F) -> Result<usize, usize>
    where
        B: Ord,
        F: FnMut(&T) -> B;
}
```

Más ergonómico que `binary_search_by` cuando se busca por un campo:

```rust
let students = vec![
    Student { name: "Ana".into(),    age: 20 },
    Student { name: "Bob".into(),    age: 22 },
    Student { name: "Carol".into(),  age: 25 },
];

// Buscar estudiante con age == 22
let result = students.binary_search_by_key(&22, |s| s.age);
// Ok(1)

// Buscar estudiante con age == 21
let result = students.binary_search_by_key(&21, |s| s.age);
// Err(1) — se insertaría entre Ana(20) y Bob(22)
```

---

## partition_point: lower_bound y upper_bound

`partition_point` es el equivalente Rust de `lower_bound` de C++.
Retorna el primer índice donde el predicado es `false`:

```rust
impl<T> [T] {
    pub fn partition_point<P>(&self, pred: P) -> usize
    where
        P: FnMut(&T) -> bool;
}
```

**Precondición**: el slice debe estar particionado respecto al
predicado — todos los `true` antes de todos los `false`. Esto se
cumple naturalmente en un slice ordenado con predicados monótonos
como `|x| x < target`.

### lower_bound

Primer índice $i$ tal que `arr[i] >= target`:

```rust
let arr = [1, 3, 3, 3, 5, 7];
let lb = arr.partition_point(|&x| x < 3);
// lb = 1 (primer 3)
```

### upper_bound

Primer índice $i$ tal que `arr[i] > target`:

```rust
let ub = arr.partition_point(|&x| x <= 3);
// ub = 4 (primer elemento > 3)
```

### equal_range

Contar ocurrencias o encontrar el rango completo:

```rust
let arr = [1, 3, 3, 3, 5, 7];
let lb = arr.partition_point(|&x| x < 3);   // 1
let ub = arr.partition_point(|&x| x <= 3);  // 4
let count = ub - lb;                          // 3

println!("3 aparece {} veces en posiciones [{}..{})", count, lb, ub);
// 3 aparece 3 veces en posiciones [1..4)
```

### Diferencia clave con binary_search

| | `binary_search` | `partition_point` |
|---|---|---|
| Retorna | `Result<usize, usize>` | `usize` |
| Con duplicados | Índice arbitrario | Primero (con `<`) o siguiente (con `<=`) |
| Predicado | Igualdad implícita | Cualquier predicado monótono |
| Caso no encontrado | `Err(inserción)` | Punto de partición |

`partition_point` es más general: puede responder preguntas como
"¿cuántos elementos son menores que X?" o "¿dónde empieza la zona
de valores >= X?" sin importar si X existe en el array.

---

## contains: existencia simple

```rust
impl<T: PartialEq> [T] {
    pub fn contains(&self, x: &T) -> bool;
}
```

Búsqueda secuencial, $O(n)$. Retorna solo `bool`:

```rust
let arr = [3, 7, 11, 15, 23];

assert!(arr.contains(&7));
assert!(!arr.contains(&10));
```

No requiere que el array esté ordenado. Es equivalente a
`arr.iter().any(|elem| elem == x)`.

### Cuándo usar contains vs binary_search

- **`contains`**: array no ordenado, o array pequeño, o una sola
  búsqueda.
- **`binary_search`**: array ordenado con múltiples búsquedas.
- **`HashSet::contains`**: muchas búsquedas, $O(1)$ amortizado.

---

## Métodos de Iterator para búsqueda

Los iteradores de Rust ofrecen búsqueda secuencial con predicados
arbitrarios — mucho más expresivo que la búsqueda por igualdad:

### find: primer elemento que satisface un predicado

```rust
fn find<P>(&mut self, predicate: P) -> Option<&T>
where P: FnMut(&&T) -> bool;
```

```rust
let nums = vec![3, 7, 11, 15, 23, 31];

// Primer par
let first_even = nums.iter().find(|&&x| x % 2 == 0);
// None (no hay pares)

// Primer mayor que 10
let first_gt10 = nums.iter().find(|&&x| x > 10);
// Some(&11)
```

`find` retorna una **referencia** al elemento. Para obtener el valor
por copia, usar `find(|&&x| ...)` con doble destructuring o
`copied().find(|&x| ...)`.

### position: índice del primer match

```rust
fn position<P>(&mut self, predicate: P) -> Option<usize>
where P: FnMut(Self::Item) -> bool;
```

```rust
let nums = [3, 7, 11, 15, 23];

let pos = nums.iter().position(|&x| x == 11);
// Some(2)

let pos = nums.iter().position(|&x| x > 20);
// Some(4) — posición de 23
```

`position` es el equivalente de la búsqueda secuencial que retorna
índice — como `linear_search` de C pero con predicados.

### rposition y rfind: buscar desde el final

```rust
let nums = [3, 7, 11, 7, 23];

let last_7 = nums.iter().rposition(|&x| x == 7);
// Some(3) — última ocurrencia

let last_gt10 = nums.iter().rfind(|&&x| x > 10);
// Some(&23) — último mayor que 10
```

Buscar desde el final es $O(n)$ pero se detiene en el primer match
desde la derecha — más eficiente que recorrer todo para encontrar
el último.

### any y all: predicados existenciales y universales

```rust
let nums = [3, 7, 11, 15, 23];

let has_even = nums.iter().any(|&x| x % 2 == 0);
// false

let all_positive = nums.iter().all(|&x| x > 0);
// true

let all_lt20 = nums.iter().all(|&x| x < 20);
// false (23 >= 20)
```

`any` es equivalente a `find().is_some()`. `all` se detiene en el
primer `false` — ambos son short-circuit.

### find_map: buscar y transformar

```rust
let strings = ["foo", "42", "bar", "7"];

let first_number: Option<i32> = strings.iter()
    .find_map(|s| s.parse::<i32>().ok());
// Some(42)
```

`find_map` combina `find` + `map` en un solo paso: aplica la
función, y retorna el primer `Some`. Útil para "buscar el primer
elemento que se puede convertir/parsear".

---

## Tabla completa de métodos de búsqueda

| Método | En | Complejidad | Requisito | Retorna |
|--------|-----|-------------|-----------|---------|
| `binary_search` | `[T]` | $O(\log n)$ | Ordenado, `T: Ord` | `Result<usize, usize>` |
| `binary_search_by` | `[T]` | $O(\log n)$ | Ordenado por cmp | `Result<usize, usize>` |
| `binary_search_by_key` | `[T]` | $O(\log n)$ | Ordenado por clave | `Result<usize, usize>` |
| `partition_point` | `[T]` | $O(\log n)$ | Particionado | `usize` |
| `contains` | `[T]` | $O(n)$ | Ninguno | `bool` |
| `iter().find()` | Iterator | $O(n)$ | Ninguno | `Option<&T>` |
| `iter().position()` | Iterator | $O(n)$ | Ninguno | `Option<usize>` |
| `iter().rposition()` | Iterator | $O(n)$ | Ninguno | `Option<usize>` |
| `iter().any()` | Iterator | $O(n)$ | Ninguno | `bool` |
| `iter().all()` | Iterator | $O(n)$ | Ninguno | `bool` |
| `iter().find_map()` | Iterator | $O(n)$ | Ninguno | `Option<B>` |

---

## Comparación con C

### bsearch de C vs binary_search de Rust

```c
/* C: bsearch */
int target = 23;
int *found = bsearch(&target, arr, n, sizeof(int), cmp_int);
if (found)
    printf("Encontrado en posición %td\n", found - arr);
else
    printf("No encontrado\n");
```

```rust
// Rust: binary_search
match arr.binary_search(&23) {
    Ok(i)  => println!("Encontrado en posición {}", i),
    Err(i) => println!("No encontrado, insertar en {}", i),
}
```

| Aspecto | C `bsearch` | Rust `binary_search` |
|---------|-------------|----------------------|
| Tipo retorno | `void *` (NULL si no existe) | `Result<usize, usize>` |
| Info de no-encontrado | Ninguna (solo NULL) | Punto de inserción |
| Duplicados | Cualquier ocurrencia | Cualquier ocurrencia |
| Tipo seguro | No (void * cast) | Sí (genérico `T: Ord`) |
| lower_bound | No disponible | `partition_point` |
| Comparador inlineable | No (puntero a fn) | Sí (monomorphization) |
| Búsqueda por predicado | No | `find`, `position`, etc. |

### Búsqueda secuencial: C vs Rust

En C, la búsqueda secuencial requiere un loop manual o una versión
genérica con `void *`:

```c
/* C: búsqueda secuencial manual */
int idx = -1;
for (int i = 0; i < n; i++) {
    if (arr[i] == target) { idx = i; break; }
}
```

En Rust, el mismo resultado con una línea:

```rust
let idx = arr.iter().position(|&x| x == target);
```

La versión Rust es:

1. **Más concisa**: una línea vs cuatro.
2. **Más expresiva**: el predicado puede ser cualquier condición,
   no solo igualdad.
3. **Igual de eficiente**: el compilador genera el mismo loop.
4. **Type-safe**: no hay riesgo de acceso fuera del array.

---

## Patrones comunes

### Insertar manteniendo el orden

```rust
let mut sorted = vec![10, 20, 30, 40, 50];

let val = 25;
let pos = sorted.binary_search(&val).unwrap_or_else(|i| i);
sorted.insert(pos, val);
// sorted = [10, 20, 25, 30, 40, 50]
```

`unwrap_or_else(|i| i)` convierte tanto `Ok(i)` como `Err(i)` en
`i` — funciona porque en ambos casos el índice es correcto para
inserción.

### Buscar en array de structs sin Ord

Si el struct no implementa `Ord`, usar `binary_search_by`:

```rust
#[derive(Debug)]
struct Event { time: f64, name: String }

let events = vec![
    Event { time: 1.0, name: "Start".into() },
    Event { time: 3.5, name: "Mid".into() },
    Event { time: 7.2, name: "End".into() },
];

// Buscar evento en time ~= 3.5
let result = events.binary_search_by(|e| {
    e.time.partial_cmp(&3.5).unwrap_or(std::cmp::Ordering::Equal)
});
```

### Deduplicar: sort + dedup

```rust
let mut data = vec![3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5];
data.sort();
data.dedup();
// data = [1, 2, 3, 4, 5, 6, 9]

// Ahora búsqueda binaria es eficiente
assert_eq!(data.binary_search(&5), Ok(4));
```

### Rango de valores con partition_point

Encontrar todos los valores en un rango $[\text{lo}, \text{hi}]$:

```rust
let arr = [1, 3, 5, 7, 9, 11, 13, 15];

let start = arr.partition_point(|&x| x < 5);   // 2
let end   = arr.partition_point(|&x| x <= 11);  // 6

let range = &arr[start..end];
// range = [5, 7, 9, 11]
```

Esto es equivalente a `SELECT * WHERE value BETWEEN 5 AND 11` en
SQL — dos búsquedas binarias dan el rango exacto.

### HashMap vs BTreeMap vs sorted Vec

| Estructura | Búsqueda | Inserción | Rango | Espacio |
|------------|----------|-----------|-------|---------|
| `HashMap` | $O(1)$ amortizado | $O(1)$ amortizado | No | Mayor |
| `BTreeMap` | $O(\log n)$ | $O(\log n)$ | Sí | Mayor |
| `Vec` ordenado | $O(\log n)$ | $O(n)$ (shift) | Sí | Menor |

- `Vec` ordenado es óptimo para datos estáticos (se construye una
  vez, se consulta muchas veces).
- `BTreeMap` es mejor para datos dinámicos con búsquedas por rango.
- `HashMap` es mejor cuando solo se necesita búsqueda por clave
  exacta y el rendimiento es crítico.

---

## Programa completo en Rust

```rust
// search_rust.rs — API completa de búsqueda en Rust
use std::cmp::Ordering;

#[derive(Debug)]
struct Student {
    name: String,
    age: u32,
    gpa: f64,
}

fn main() {
    // =============================================
    // Demo 1: binary_search básico
    // =============================================
    println!("=== Demo 1: binary_search ===");
    let arr = [3, 7, 11, 15, 23, 31, 42, 58, 71];
    println!("Array: {:?}\n", arr);

    for &t in &[23, 20, 3, 71, 100] {
        match arr.binary_search(&t) {
            Ok(i)  => println!("  binary_search({:>3}) = Ok({})  — encontrado", t, i),
            Err(i) => println!("  binary_search({:>3}) = Err({}) — insertar aquí", t, i),
        }
    }

    // =============================================
    // Demo 2: partition_point (lower/upper bound)
    // =============================================
    println!("\n=== Demo 2: partition_point ===");
    let dup = [1, 3, 3, 3, 5, 5, 7, 9];
    println!("Array: {:?}\n", dup);

    for &t in &[3, 5, 4, 0, 10] {
        let lb = dup.partition_point(|&x| x < t);
        let ub = dup.partition_point(|&x| x <= t);
        let count = ub - lb;

        if count > 0 {
            println!("  target={}: lb={}, ub={}, count={} → [{}, {})",
                     t, lb, ub, count, lb, ub);
        } else {
            println!("  target={}: no encontrado, insertar en {}", t, lb);
        }
    }

    // =============================================
    // Demo 3: binary_search_by con structs
    // =============================================
    println!("\n=== Demo 3: binary_search_by (structs) ===");
    let students = vec![
        Student { name: "Ana".into(),    age: 20, gpa: 3.9 },
        Student { name: "Bob".into(),    age: 22, gpa: 3.5 },
        Student { name: "Carol".into(),  age: 25, gpa: 3.7 },
        Student { name: "David".into(),  age: 28, gpa: 3.2 },
    ];

    // Buscar por age
    for &target_age in &[22, 23] {
        match students.binary_search_by(|s| s.age.cmp(&target_age)) {
            Ok(i) => println!("  age={}: encontrado → {:?}",
                              target_age, students[i].name),
            Err(i) => println!("  age={}: no encontrado, insertar en pos {}",
                               target_age, i),
        }
    }

    // =============================================
    // Demo 4: binary_search_by_key
    // =============================================
    println!("\n=== Demo 4: binary_search_by_key ===");
    match students.binary_search_by_key(&25, |s| s.age) {
        Ok(i)  => println!("  Estudiante con age=25: {}", students[i].name),
        Err(i) => println!("  No hay estudiante con age=25, pos={}", i),
    }

    // =============================================
    // Demo 5: contains
    // =============================================
    println!("\n=== Demo 5: contains ===");
    let primes = [2, 3, 5, 7, 11, 13, 17, 19, 23];
    println!("  primes.contains(&7):  {}", primes.contains(&7));
    println!("  primes.contains(&10): {}", primes.contains(&10));

    // =============================================
    // Demo 6: Iterator — find, position, any, all
    // =============================================
    println!("\n=== Demo 6: Iterator methods ===");
    let data = vec![3, 7, 11, 15, 23, 31, 42];

    // find
    let first_gt20 = data.iter().find(|&&x| x > 20);
    println!("  find(>20):      {:?}", first_gt20);

    // position
    let pos_11 = data.iter().position(|&x| x == 11);
    println!("  position(==11): {:?}", pos_11);

    // rposition
    let rpos = data.iter().rposition(|&x| x < 20);
    println!("  rposition(<20): {:?}", rpos);

    // any / all
    println!("  any(even):      {}", data.iter().any(|&x| x % 2 == 0));
    println!("  all(positive):  {}", data.iter().all(|&x| x > 0));

    // =============================================
    // Demo 7: find_map
    // =============================================
    println!("\n=== Demo 7: find_map ===");
    let tokens = ["hello", "42", "world", "7", "!"];
    let first_num: Option<i32> = tokens.iter()
        .find_map(|s| s.parse::<i32>().ok());
    println!("  Primer número en {:?}: {:?}", tokens, first_num);

    // =============================================
    // Demo 8: Insertar manteniendo orden
    // =============================================
    println!("\n=== Demo 8: Insertar manteniendo orden ===");
    let mut sorted = vec![10, 20, 30, 40, 50];
    println!("  Antes:   {:?}", sorted);

    for &val in &[25, 5, 55, 30] {
        let pos = sorted.binary_search(&val).unwrap_or_else(|i| i);
        sorted.insert(pos, val);
        println!("  +{:<4} →  {:?}", val, sorted);
    }

    // =============================================
    // Demo 9: Rango de valores con partition_point
    // =============================================
    println!("\n=== Demo 9: Rango de valores ===");
    let arr = [1, 3, 5, 7, 9, 11, 13, 15, 17, 19];
    println!("  Array: {:?}", arr);

    let (lo_val, hi_val) = (5, 13);
    let start = arr.partition_point(|&x| x < lo_val);
    let end   = arr.partition_point(|&x| x <= hi_val);
    println!("  Rango [{}, {}]: {:?} (posiciones {}..{})",
             lo_val, hi_val, &arr[start..end], start, end);

    // =============================================
    // Demo 10: Deduplicar + buscar
    // =============================================
    println!("\n=== Demo 10: sort + dedup + binary_search ===");
    let mut messy = vec![5, 3, 1, 4, 1, 5, 9, 2, 6, 5, 3];
    println!("  Original: {:?}", messy);
    messy.sort();
    messy.dedup();
    println!("  Dedup:    {:?}", messy);
    println!("  search(5): {:?}", messy.binary_search(&5));
    println!("  search(7): {:?}", messy.binary_search(&7));

    // =============================================
    // Demo 11: Comparación estabilidad vs rendimiento
    // =============================================
    println!("\n=== Demo 11: Encadenamiento de búsquedas ===");

    // Buscar el primer estudiante con GPA > 3.5
    let found = students.iter()
        .find(|s| s.gpa > 3.5);
    println!("  Primer GPA > 3.5: {:?}",
             found.map(|s| (&s.name, s.gpa)));

    // Buscar todos con GPA > 3.5
    let high_gpa: Vec<&str> = students.iter()
        .filter(|s| s.gpa > 3.5)
        .map(|s| s.name.as_str())
        .collect();
    println!("  Todos GPA > 3.5:  {:?}", high_gpa);

    // Posición del primer GPA < 3.5
    let pos = students.iter().position(|s| s.gpa < 3.5);
    println!("  Posición primer GPA < 3.5: {:?}", pos);
}
```

### Compilar y ejecutar

```bash
rustc -o search_rust search_rust.rs
./search_rust
```

### Salida esperada

```
=== Demo 1: binary_search ===
Array: [3, 7, 11, 15, 23, 31, 42, 58, 71]

  binary_search( 23) = Ok(4)  — encontrado
  binary_search( 20) = Err(4) — insertar aquí
  binary_search(  3) = Ok(0)  — encontrado
  binary_search( 71) = Ok(8)  — encontrado
  binary_search(100) = Err(9) — insertar aquí

=== Demo 2: partition_point ===
Array: [1, 3, 3, 3, 5, 5, 7, 9]

  target=3: lb=1, ub=4, count=3 → [1, 4)
  target=5: lb=4, ub=6, count=2 → [4, 6)
  target=4: no encontrado, insertar en 4
  target=0: no encontrado, insertar en 0
  target=10: no encontrado, insertar en 8

=== Demo 3: binary_search_by (structs) ===
  age=22: encontrado → "Bob"
  age=23: no encontrado, insertar en pos 2

=== Demo 4: binary_search_by_key ===
  Estudiante con age=25: Carol

=== Demo 5: contains ===
  primes.contains(&7):  true
  primes.contains(&10): false

=== Demo 6: Iterator methods ===
  find(>20):      Some(23)
  position(==11): Some(2)
  rposition(<20): Some(3)
  any(even):      true
  all(positive):  true

=== Demo 7: find_map ===
  Primer número en ["hello", "42", "world", "7", "!"]: Some(42)

=== Demo 8: Insertar manteniendo orden ===
  Antes:   [10, 20, 30, 40, 50]
  +25   →  [10, 20, 25, 30, 40, 50]
  +5    →  [5, 10, 20, 25, 30, 40, 50]
  +55   →  [5, 10, 20, 25, 30, 40, 50, 55]
  +30   →  [5, 10, 20, 25, 30, 30, 40, 50, 55]

=== Demo 9: Rango de valores ===
  Array: [1, 3, 5, 7, 9, 11, 13, 15, 17, 19]
  Rango [5, 13]: [5, 7, 9, 11, 13] (posiciones 2..7)

=== Demo 10: sort + dedup + binary_search ===
  Original: [5, 3, 1, 4, 1, 5, 9, 2, 6, 5, 3]
  Dedup:    [1, 2, 3, 4, 5, 6, 9]
  search(5): Ok(4)
  search(7): Err(6)

=== Demo 11: Encadenamiento de búsquedas ===
  Primer GPA > 3.5: Some(("Ana", 3.9))
  Todos GPA > 3.5:  ["Ana", "Carol"]
  Posición primer GPA < 3.5: Some(3)
```

---

## Ejercicios

### Ejercicio 1 — Result<usize, usize>

¿Qué retorna `binary_search` en cada caso?

```rust
let arr = [10, 20, 30, 40, 50];
arr.binary_search(&30);  // a
arr.binary_search(&25);  // b
arr.binary_search(&5);   // c
arr.binary_search(&60);  // d
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
a) Ok(2)   — 30 está en posición 2
b) Err(2)  — 25 se insertaría en posición 2 (entre 20 y 30)
c) Err(0)  — 5 se insertaría al inicio (antes de 10)
d) Err(5)  — 60 se insertaría al final (después de 50)
```

El `Err(i)` siempre indica un punto de inserción válido:
`arr[..i]` son todos menores, `arr[i..]` son todos mayores o iguales.
Para `Err(5)` con un array de 5 elementos, la posición 5 es
"después del final" — igualmente válida para `vec.insert(5, val)`.

</details>

### Ejercicio 2 — partition_point vs binary_search con duplicados

¿Cómo encontrar la **primera** y **última** posición de un valor
en un slice con duplicados usando `partition_point`?

```rust
let arr = [1, 2, 2, 2, 3, 4, 4, 5];
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
let target = 2;

// Primera posición (lower_bound)
let first = arr.partition_point(|&x| x < target);
// first = 1

// Última posición (upper_bound - 1)
let after = arr.partition_point(|&x| x <= target);
// after = 4
let last = after - 1;
// last = 3

// Verificar que existe
if first < arr.len() && arr[first] == target {
    println!("Primera en {}, última en {}, count={}",
             first, last, after - first);
}
// "Primera en 1, última en 3, count=3"

// Para target = 4:
let first_4 = arr.partition_point(|&x| x < 4);    // 4  (espera: 5)
// Corrección: arr = [1,2,2,2,3,4,4,5]
// Posiciones:         0,1,2,3,4,5,6,7
// first_4 = 5, after_4 = 7, last_4 = 6, count = 2
```

La clave: `|x| x < target` da la primera ocurrencia,
`|x| x <= target` da la posición después de la última. La resta
da el conteo.

</details>

### Ejercicio 3 — Insertar sin duplicados

Escribe una función que inserte un valor en un `Vec` ordenado solo
si no existe. Retorna `true` si se insertó.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
fn insert_unique(vec: &mut Vec<i32>, val: i32) -> bool {
    match vec.binary_search(&val) {
        Ok(_)  => false,          // ya existe
        Err(i) => {
            vec.insert(i, val);   // insertar en punto correcto
            true
        }
    }
}
```

Uso:

```rust
let mut v = vec![10, 20, 30];
assert!(insert_unique(&mut v, 25));    // true, v = [10, 20, 25, 30]
assert!(!insert_unique(&mut v, 25));   // false, v sin cambios
assert!(insert_unique(&mut v, 5));     // true, v = [5, 10, 20, 25, 30]
```

El pattern `match binary_search` con `Ok`/`Err` es exactamente
para este caso: `Ok` significa "ya existe", `Err` significa "no
existe, pero aquí iría". Es más limpio que el equivalente en C
(llamar `bsearch`, verificar NULL, calcular posición, `memmove`).

</details>

### Ejercicio 4 — find vs position vs binary_search

Para cada escenario, ¿qué método es más apropiado?

1. Array ordenado, buscar si existe el valor 42.
2. Array desordenado de strings, buscar la primera que empiece con "Z".
3. Array ordenado, necesito el índice del valor 42.
4. Iterator de archivos, buscar el primero > 1MB.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

1. **`binary_search(&42).is_ok()`** — $O(\log n)$ porque está
   ordenado. `contains` funcionaría pero sería $O(n)$.

2. **`iter().find(|s| s.starts_with("Z"))`** — búsqueda secuencial
   con predicado. No se puede usar `binary_search` porque el
   predicado no coincide con el criterio de ordenación.

3. **`binary_search(&42)`** — retorna `Ok(i)` con el índice
   directamente. `position` funcionaría pero sería $O(n)$.

4. **`iter().find(|f| f.size() > 1_000_000)`** — búsqueda
   secuencial. Los archivos no están ordenados por tamaño, y es
   un iterador (no un slice), así que `binary_search` no aplica.

Regla general: si el dato está **ordenado por el criterio de
búsqueda**, usar `binary_search`. Si no, usar `find`/`position`.

</details>

### Ejercicio 5 — partition_point para búsqueda en dominio continuo

Usa `partition_point` para encontrar el entero más grande cuyo
cuadrado es $\leq n$, es decir, $\lfloor\sqrt{n}\rfloor$:

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
fn isqrt(n: u64) -> u64 {
    // Crear un rango [0..=n] como slice virtual
    // partition_point necesita un slice, así que usamos un rango
    let candidates: Vec<u64> = (0..=n.min(100_000)).collect();
    candidates.partition_point(|&x| x * x <= n) as u64 - 1
}
```

Versión más eficiente sin crear un vector:

```rust
fn isqrt(n: u64) -> u64 {
    let mut lo: u64 = 0;
    let mut hi: u64 = n.min(3_000_000_000); // sqrt(u64::MAX) < 2^32

    while lo < hi {
        let mid = lo + (hi - lo + 1) / 2;  // +1 para redondear arriba
        if mid <= n / mid {  // evita overflow de mid*mid
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    lo
}

assert_eq!(isqrt(0), 0);
assert_eq!(isqrt(1), 1);
assert_eq!(isqrt(8), 2);
assert_eq!(isqrt(9), 3);
assert_eq!(isqrt(99), 9);
assert_eq!(isqrt(100), 10);
```

Nótese `mid <= n / mid` en lugar de `mid * mid <= n` para evitar
overflow. Este es el mismo patrón que la búsqueda binaria en dominio
continuo, pero con enteros.

</details>

### Ejercicio 6 — Buscar en Vec de structs sin Ord

Dado un vector de `Event` no ordenado, encuentra el evento con
el `time` más cercano a un valor dado:

```rust
struct Event { time: f64, name: String }
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Sin ordenar** — búsqueda secuencial:

```rust
fn nearest_event(events: &[Event], target_time: f64) -> Option<&Event> {
    events.iter()
        .min_by(|a, b| {
            let da = (a.time - target_time).abs();
            let db = (b.time - target_time).abs();
            da.partial_cmp(&db).unwrap_or(std::cmp::Ordering::Equal)
        })
}
```

**Si está ordenado por time** — búsqueda binaria + comparar vecinos:

```rust
fn nearest_event_sorted(events: &[Event], target: f64) -> Option<&Event> {
    if events.is_empty() { return None; }

    let pos = events.partition_point(|e| e.time < target);

    match pos {
        0 => Some(&events[0]),
        n if n == events.len() => Some(&events[n - 1]),
        i => {
            let before = (events[i - 1].time - target).abs();
            let after  = (events[i].time - target).abs();
            if before <= after { Some(&events[i - 1]) }
            else               { Some(&events[i]) }
        }
    }
}
```

La versión ordenada es $O(\log n)$ vs $O(n)$ de la desordenada.
`partition_point` da el primer evento con `time >= target`, y luego
comparamos con el anterior para encontrar el más cercano.

</details>

### Ejercicio 7 — Encadenar filter + binary_search

¿Se puede hacer `binary_search` en el resultado de `filter`? ¿Por
qué sí o por qué no?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**No directamente.** `filter` retorna un `Iterator`, no un slice.
`binary_search` es un método de `[T]` (slices), no de iteradores.

```rust
let data = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Esto NO compila:
// data.iter().filter(|&&x| x % 2 == 0).binary_search(&6);

// Opciones:

// 1. Collect a Vec, luego binary_search
let evens: Vec<i32> = data.iter()
    .filter(|&&x| x % 2 == 0)
    .copied()
    .collect();
evens.binary_search(&6);  // Ok(2) — [2,4,6,8,10]

// 2. Usar find en el iterador (O(n) pero sin alloc)
let found = data.iter()
    .filter(|&&x| x % 2 == 0)
    .find(|&&x| x == 6);
// Some(&6)
```

La razón: `binary_search` necesita **acceso aleatorio** (ir al
elemento $i$ en $O(1)$), que los iteradores no proveen. Un iterador
filtrado no sabe cuántos elementos tiene ni puede saltar posiciones.

Si el filtrado se hace frecuentemente, es mejor materializar el
resultado en un `Vec` ordenado y reutilizarlo.

</details>

### Ejercicio 8 — any vs contains

¿Cuál es la diferencia entre `arr.contains(&x)` e
`arr.iter().any(|&e| e == x)`? ¿Cuándo preferir uno u otro?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Funcionalmente son idénticos** para búsqueda por igualdad. De
hecho, la implementación de `contains` en la stdlib es:

```rust
pub fn contains(&self, x: &T) -> bool
where T: PartialEq
{
    self.iter().any(|e| e == x)
}
```

**Cuándo preferir cada uno**:

- **`contains(&x)`**: cuando buscas por igualdad exacta. Más
  legible, intención más clara.
- **`any(|e| ...)`**: cuando el predicado no es igualdad simple.
  Por ejemplo: `any(|s| s.starts_with("Z"))`, `any(|&x| x > 100)`.

```rust
let words = ["apple", "banana", "cherry"];

// Igualdad → contains (más claro)
words.contains(&"banana");

// Predicado → any (necesario)
words.iter().any(|w| w.len() > 5);
```

En rendimiento son iguales — ambos son $O(n)$ búsqueda secuencial
con short-circuit.

</details>

### Ejercicio 9 — find_map para parseo robusto

Dado un vector de strings que representan configuraciones
`"key=value"`, escribe una función que busque una clave específica
y retorne su valor parseado como `i32`:

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
fn get_config_int(configs: &[&str], key: &str) -> Option<i32> {
    configs.iter()
        .find_map(|line| {
            let (k, v) = line.split_once('=')?;
            if k.trim() == key {
                v.trim().parse::<i32>().ok()
            } else {
                None
            }
        })
}

let configs = [
    "port = 8080",
    "timeout = 30",
    "name = server1",
    "workers = 4",
];

assert_eq!(get_config_int(&configs, "port"), Some(8080));
assert_eq!(get_config_int(&configs, "timeout"), Some(30));
assert_eq!(get_config_int(&configs, "name"), None);    // no es i32
assert_eq!(get_config_int(&configs, "missing"), None);  // no existe
```

`find_map` es perfecto aquí: combina buscar la clave correcta **y**
parsear el valor en un solo paso. Si la clave no existe o el valor
no parsea como `i32`, retorna `None`. Sin `find_map`, necesitaríamos
`find` + `map` + `and_then` — menos legible.

</details>

### Ejercicio 10 — Benchmark: binary_search vs contains vs HashMap

Para $n = 10^5$ elementos y $10^5$ búsquedas, compara el tiempo de
`binary_search`, `contains`, y `HashSet::contains`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```rust
use std::collections::HashSet;
use std::time::Instant;

fn main() {
    let n = 100_000;
    let sorted: Vec<i32> = (0..n).collect();
    let set: HashSet<i32> = (0..n).collect();

    // Generar queries (50% existentes, 50% no)
    let queries: Vec<i32> = (0..n).map(|i| i * 2 - n / 2).collect();

    // binary_search
    let start = Instant::now();
    let mut found_bs = 0;
    for &q in &queries {
        if sorted.binary_search(&q).is_ok() { found_bs += 1; }
    }
    let bs_ms = start.elapsed().as_secs_f64() * 1000.0;

    // contains (lineal)
    let start = Instant::now();
    let mut found_c = 0;
    for &q in &queries {
        if sorted.contains(&q) { found_c += 1; }
    }
    let c_ms = start.elapsed().as_secs_f64() * 1000.0;

    // HashSet
    let start = Instant::now();
    let mut found_h = 0;
    for &q in &queries {
        if set.contains(&q) { found_h += 1; }
    }
    let h_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("n = {}, {} búsquedas", n, n);
    println!("  binary_search:   {:>8.2} ms  ({} found)", bs_ms, found_bs);
    println!("  Vec::contains:   {:>8.2} ms  ({} found)", c_ms, found_c);
    println!("  HashSet::contains:{:>7.2} ms  ({} found)", h_ms, found_h);
}
```

Resultados típicos:

```
n = 100000, 100000 búsquedas
  binary_search:       5.00 ms  (50000 found)
  Vec::contains:    2500.00 ms  (50000 found)
  HashSet::contains:   2.50 ms  (50000 found)
```

- **`HashSet`**: ~2.5 ms — $O(1)$ por búsqueda, el más rápido.
- **`binary_search`**: ~5 ms — $O(\log n)$ por búsqueda, 2x más
  lento que hash pero sin overhead de la estructura hash.
- **`contains` (lineal)**: ~2500 ms — $O(n)$ por búsqueda, ~500x
  más lento que binary search.

Para $10^5$ búsquedas en $10^5$ elementos: `contains` es
$O(n^2) = 10^{10}$ operaciones vs `binary_search`
$O(n \log n) \approx 1.7 \times 10^6$ vs `HashSet`
$O(n) = 10^5$.

</details>

---

## Resumen

| Método | Complejidad | Retorna | Requisito | Uso principal |
|--------|-------------|---------|-----------|---------------|
| `binary_search` | $O(\log n)$ | `Result<usize, usize>` | Ordenado, `T: Ord` | Búsqueda exacta + punto de inserción |
| `partition_point` | $O(\log n)$ | `usize` | Particionado | lower/upper bound, rangos |
| `contains` | $O(n)$ | `bool` | Ninguno | Existencia simple |
| `find` | $O(n)$ | `Option<&T>` | Ninguno | Primer match por predicado |
| `position` | $O(n)$ | `Option<usize>` | Ninguno | Índice del primer match |
| `any` / `all` | $O(n)$ | `bool` | Ninguno | Predicado existencial/universal |
| `find_map` | $O(n)$ | `Option<B>` | Ninguno | Buscar + transformar |
