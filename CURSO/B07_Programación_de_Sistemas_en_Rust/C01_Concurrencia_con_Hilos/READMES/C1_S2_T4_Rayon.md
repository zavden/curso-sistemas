# Rayon

## Indice
1. [Que es Rayon](#1-que-es-rayon)
2. [Instalacion y primer uso](#2-instalacion-y-primer-uso)
3. [par_iter: iteradores paralelos](#3-par_iter-iteradores-paralelos)
4. [par_iter_mut y into_par_iter](#4-par_iter_mut-y-into_par_iter)
5. [Operaciones sobre iteradores paralelos](#5-operaciones-sobre-iteradores-paralelos)
6. [join: fork-join explicito](#6-join-fork-join-explicito)
7. [El thread pool de Rayon](#7-el-thread-pool-de-rayon)
8. [Work stealing: como funciona](#8-work-stealing-como-funciona)
9. [Cuando usar y cuando NO usar Rayon](#9-cuando-usar-y-cuando-no-usar-rayon)
10. [Patrones practicos](#10-patrones-practicos)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Que es Rayon

Rayon es una libreria de **paralelismo de datos** para Rust.
Su premisa es simple: si tienes un iterador, puedes hacerlo
paralelo cambiando `.iter()` por `.par_iter()`:

```rust
// Secuencial:
let sum: i64 = data.iter().map(|x| x * x).sum();

// Paralelo (con Rayon):
let sum: i64 = data.par_iter().map(|x| x * x).sum();
```

Una sola linea de cambio, y Rayon distribuye el trabajo entre
todos los CPUs disponibles usando un **thread pool** con
**work stealing**.

### Comparacion con lo que ya conoces

```
+---------------------------+-------------------------------------+
| Mecanismo                 | Uso                                 |
+---------------------------+-------------------------------------+
| thread::spawn             | Hilo de larga vida, independiente   |
| thread::scope             | Paralelismo local con referencias   |
| Rayon (par_iter, join)    | Paralelismo de datos, sin boilerplate|
+---------------------------+-------------------------------------+
```

Con `thread::scope` hicimos paralelismo manual: dividir en
chunks, crear N hilos, recoger resultados. Rayon automatiza
todo eso — tu describes **que** paralelizar, Rayon decide
**como** dividir el trabajo.

---

## 2. Instalacion y primer uso

Agrega Rayon a tu proyecto:

```bash
cargo add rayon
```

Esto agrega en `Cargo.toml`:

```toml
[dependencies]
rayon = "1"
```

### Primer ejemplo

```rust
use rayon::prelude::*;

fn main() {
    let numbers: Vec<i64> = (1..=10_000_000).collect();

    // Suma secuencial
    let seq_sum: i64 = numbers.iter().sum();

    // Suma paralela — solo cambia iter() por par_iter()
    let par_sum: i64 = numbers.par_iter().sum();

    assert_eq!(seq_sum, par_sum);
    println!("Sum: {}", par_sum);
}
```

### Que importa `rayon::prelude::*`

```rust
// Trae a scope los traits necesarios:
use rayon::prelude::*;

// Equivale a:
use rayon::iter::IntoParallelRefIterator;     // .par_iter()
use rayon::iter::IntoParallelRefMutIterator;  // .par_iter_mut()
use rayon::iter::IntoParallelIterator;        // .into_par_iter()
use rayon::iter::ParallelIterator;            // map, filter, sum, etc.
use rayon::iter::IndexedParallelIterator;     // zip, enumerate, chunks
```

Con `use rayon::prelude::*` tienes todo lo necesario.

---

## 3. par_iter: iteradores paralelos

`par_iter()` crea un iterador paralelo sobre **referencias**
(`&T`), igual que `.iter()` crea un iterador secuencial sobre
`&T`:

```rust
use rayon::prelude::*;

fn main() {
    let words = vec!["hello", "world", "foo", "bar", "baz"];

    // Secuencial: iter() -> &str
    let lengths: Vec<usize> = words.iter().map(|w| w.len()).collect();

    // Paralelo: par_iter() -> &str
    let par_lengths: Vec<usize> = words.par_iter().map(|w| w.len()).collect();

    assert_eq!(lengths, par_lengths);
}
```

### Tipos que soportan par_iter

```
+---------------------+---------------------------+
| Tipo                | Metodo paralelo           |
+---------------------+---------------------------+
| &Vec<T>             | .par_iter()    -> &T      |
| &[T]                | .par_iter()    -> &T      |
| &mut Vec<T>         | .par_iter_mut()-> &mut T  |
| &mut [T]            | .par_iter_mut()-> &mut T  |
| Vec<T>              | .into_par_iter() -> T     |
| Range<i64>          | .into_par_iter() -> i64   |
| &HashMap<K,V>       | .par_iter()    -> (&K,&V) |
| &BTreeMap<K,V>      | .par_iter()    -> (&K,&V) |
| &HashSet<T>         | .par_iter()    -> &T      |
| &String (chars)     | .par_chars()              |
+---------------------+---------------------------+
```

### Ranges paralelos

```rust
use rayon::prelude::*;

fn main() {
    // Rangos se pueden paralelizar directamente
    let sum: i64 = (1..=1_000_000i64).into_par_iter().sum();
    println!("Sum: {}", sum);  // 500000500000

    // Equivale a:
    // (1..=1_000_000).collect::<Vec<_>>().par_iter().sum()
    // pero sin allocar el Vec
}
```

---

## 4. par_iter_mut y into_par_iter

### par_iter_mut: modificar elementos en paralelo

```rust
use rayon::prelude::*;

fn main() {
    let mut data = vec![1, 2, 3, 4, 5, 6, 7, 8];

    // Duplicar cada elemento en paralelo
    data.par_iter_mut().for_each(|x| *x *= 2);

    println!("{:?}", data);  // [2, 4, 6, 8, 10, 12, 14, 16]
}
```

Esto es equivalente a lo que hicimos con `thread::scope` +
`chunks_mut`, pero en una linea.

### into_par_iter: consumir la coleccion

```rust
use rayon::prelude::*;

fn main() {
    let strings = vec![
        String::from("hello"),
        String::from("world"),
        String::from("foo"),
    ];

    // into_par_iter consume el Vec, cada hilo recibe ownership de un String
    let upper: Vec<String> = strings
        .into_par_iter()
        .map(|s| s.to_uppercase())
        .collect();

    // strings ya no existe (fue consumido)
    println!("{:?}", upper);  // ["HELLO", "WORLD", "FOO"]
}
```

### Resumen de los tres metodos

```
    par_iter()      -->  &T       (lectura, coleccion prestada)
    par_iter_mut()  -->  &mut T   (modificacion in-place)
    into_par_iter() -->  T        (consume la coleccion)

    Exactamente paralelo a:
    iter()          -->  &T
    iter_mut()      -->  &mut T
    into_iter()     -->  T
```

---

## 5. Operaciones sobre iteradores paralelos

Los iteradores paralelos soportan la mayoria de operaciones
que conoces de los iteradores secuenciales:

### map, filter, filter_map

```rust
use rayon::prelude::*;

fn main() {
    let data: Vec<i32> = (0..100).collect();

    // map + filter + collect — todo en paralelo
    let result: Vec<i32> = data
        .par_iter()
        .filter(|&&x| x % 3 == 0)
        .map(|&x| x * x)
        .collect();

    println!("Squares of multiples of 3: {:?}", &result[..5]);
    // [0, 9, 36, 81, 144]
}
```

### sum, product, min, max

```rust
use rayon::prelude::*;

fn main() {
    let data: Vec<f64> = (1..=1000).map(|x| x as f64).collect();

    let sum: f64 = data.par_iter().sum();
    let min: f64 = *data.par_iter().min_by(|a, b| a.partial_cmp(b).unwrap()).unwrap();
    let max: f64 = *data.par_iter().max_by(|a, b| a.partial_cmp(b).unwrap()).unwrap();

    println!("sum={}, min={}, max={}", sum, min, max);
}
```

### reduce

`reduce` combina elementos en paralelo. Necesita una operacion
**asociativa** (el orden de agrupacion no importa):

```rust
use rayon::prelude::*;

fn main() {
    let data = vec![1, 2, 3, 4, 5, 6, 7, 8];

    // reduce necesita: identity element + operacion asociativa
    let sum = data.par_iter().copied().reduce(|| 0, |a, b| a + b);

    println!("sum = {}", sum);  // 36
}
```

```
    reduce en paralelo:

    [1, 2, 3, 4, 5, 6, 7, 8]
     |     |     |     |
     v     v     v     v
    1+2   3+4   5+6   7+8     (4 hilos, cada uno reduce su chunk)
     3     7    11    15
      \   /      \   /
       10          26          (se combinan los resultados parciales)
        \         /
           36
```

### for_each: efecto lateral

```rust
use rayon::prelude::*;
use std::sync::atomic::{AtomicUsize, Ordering};

fn main() {
    let data: Vec<i32> = (0..1000).collect();
    let even_count = AtomicUsize::new(0);

    data.par_iter().for_each(|&x| {
        if x % 2 == 0 {
            even_count.fetch_add(1, Ordering::Relaxed);
        }
    });

    println!("even numbers: {}", even_count.load(Ordering::Relaxed));
    // 500
}
```

### flat_map

```rust
use rayon::prelude::*;

fn main() {
    let sentences = vec![
        "hello world",
        "foo bar baz",
        "rust is great",
    ];

    let word_count: usize = sentences
        .par_iter()
        .flat_map(|s| s.split_whitespace())
        .count();

    println!("words: {}", word_count);  // 9
}
```

### any, all, find_any, position_any

```rust
use rayon::prelude::*;

fn main() {
    let data: Vec<i32> = (0..1_000_000).collect();

    let has_negative = data.par_iter().any(|&x| x < 0);
    let all_positive = data.par_iter().all(|&x| x >= 0);

    // find_any: retorna ALGUN elemento que cumpla (no necesariamente el primero)
    let found = data.par_iter().find_any(|&&x| x == 999_999);

    // position_any: retorna ALGUNA posicion (no necesariamente la primera)
    let pos = data.par_iter().position_any(|&x| x == 500_000);

    println!("has_negative: {}", has_negative);    // false
    println!("all_positive: {}", all_positive);    // true
    println!("found: {:?}", found);                // Some(999999)
    println!("position: {:?}", pos);               // Some(500000)
}
```

> **Nota**: `find_any` y `position_any` no garantizan retornar
> el **primer** match. Si necesitas el primero, usa `find_first`
> o `position_first` (IndexedParallelIterator).

---

## 6. join: fork-join explicito

`rayon::join` ejecuta dos closures en paralelo y espera a que
ambos terminen. Es el bloque basico de Rayon:

```rust
use rayon;

fn main() {
    let (a, b) = rayon::join(
        || expensive_computation_1(),
        || expensive_computation_2(),
    );
    // Ambas terminaron, a y b son los resultados
    println!("a={}, b={}", a, b);
}

fn expensive_computation_1() -> i64 {
    (1..=1_000_000i64).sum()
}

fn expensive_computation_2() -> i64 {
    (1..=1_000_000i64).map(|x| x * x % 1000).sum()
}
```

### join vs thread::scope

```rust
use rayon;
use std::thread;

// Con thread::scope — crea hilos del OS
fn with_scope(data: &[i64]) -> (i64, i64) {
    let (left, right) = data.split_at(data.len() / 2);
    thread::scope(|s| {
        let h = s.spawn(|| left.iter().sum::<i64>());
        let r: i64 = right.iter().sum();
        (h.join().unwrap(), r)
    })
}

// Con rayon::join — reutiliza el thread pool
fn with_rayon(data: &[i64]) -> (i64, i64) {
    let (left, right) = data.split_at(data.len() / 2);
    rayon::join(
        || left.iter().sum::<i64>(),
        || right.iter().sum::<i64>(),
    )
}
```

La diferencia clave: `rayon::join` **no crea hilos nuevos** —
usa hilos existentes del thread pool. Crear un hilo del OS
cuesta ~microsegundos; programar una tarea en el pool de Rayon
cuesta ~nanosegundos.

### Divide y venceras recursivo

```rust
fn parallel_merge_sort(data: &mut [i32]) {
    if data.len() <= 32 {
        data.sort();  // caso base: sort secuencial
        return;
    }

    let mid = data.len() / 2;
    let (left, right) = data.split_at_mut(mid);

    rayon::join(
        || parallel_merge_sort(left),
        || parallel_merge_sort(right),
    );

    // Merge (simplificado — en produccion usarias un buffer auxiliar)
    let merged: Vec<i32> = {
        let mut result = Vec::with_capacity(data.len());
        let (mut i, mut j) = (0, 0);
        while i < left.len() && j < right.len() {
            if left[i] <= right[j] {
                result.push(left[i]);
                i += 1;
            } else {
                result.push(right[j]);
                j += 1;
            }
        }
        result.extend_from_slice(&left[i..]);
        result.extend_from_slice(&right[j..]);
        result
    };
    data.copy_from_slice(&merged);
}

fn main() {
    let mut data: Vec<i32> = (0..10_000).rev().collect();
    parallel_merge_sort(&mut data);
    assert!(data.windows(2).all(|w| w[0] <= w[1]));
    println!("sorted: [{}, {}, ..., {}]", data[0], data[1], data[data.len()-1]);
}
```

Con `thread::scope` esto crearia miles de hilos del OS en la
recursion. Con `rayon::join`, todas las tareas se distribuyen
entre los mismos N hilos del pool.

---

## 7. El thread pool de Rayon

Rayon crea automaticamente un **thread pool global** la primera
vez que lo usas. Por defecto tiene tantos hilos como CPUs logicas.

```
    Thread Pool de Rayon (4 CPUs):

    +--------+  +--------+  +--------+  +--------+
    | Hilo 0 |  | Hilo 1 |  | Hilo 2 |  | Hilo 3 |
    |        |  |        |  |        |  |        |
    | [cola] |  | [cola] |  | [cola] |  | [cola] |
    +--------+  +--------+  +--------+  +--------+
         ^           ^           ^           ^
         |           |           |           |
    par_iter divide el trabajo y lo envia a las colas
```

### Configurar el pool global

```rust
use rayon::ThreadPoolBuilder;

fn main() {
    // Debe llamarse ANTES de usar cualquier operacion de Rayon
    ThreadPoolBuilder::new()
        .num_threads(8)
        .build_global()
        .unwrap();

    // Ahora par_iter y join usan 8 hilos
}
```

### Crear un pool local

```rust
use rayon::ThreadPoolBuilder;

fn main() {
    let pool = ThreadPoolBuilder::new()
        .num_threads(2)
        .build()
        .unwrap();

    let data: Vec<i64> = (1..=1_000_000).collect();

    // Ejecutar en el pool local
    let sum = pool.install(|| {
        use rayon::prelude::*;
        data.par_iter().sum::<i64>()
    });

    println!("sum = {}", sum);
}
```

### Cuando crear un pool local

- **Testing**: controlar el numero de hilos para reproducibilidad.
- **Aislamiento**: evitar que operaciones lentas bloqueen el pool global.
- **Librerias**: no contaminar el pool global del usuario.

---

## 8. Work stealing: como funciona

Rayon usa **work stealing** para balancear carga automaticamente:

```
    Paso 1: Rayon divide el trabajo recursivamente

    [1, 2, 3, 4, 5, 6, 7, 8]
           /          \
    [1, 2, 3, 4]   [5, 6, 7, 8]
       /    \          /    \
    [1,2]  [3,4]    [5,6]  [7,8]

    Paso 2: Cada mitad se asigna a la cola del hilo que la creo

    Hilo 0: [1,2] [3,4]    (tiene 2 tareas)
    Hilo 1: [5,6] [7,8]    (tiene 2 tareas)
    Hilo 2: (vacio)
    Hilo 3: (vacio)

    Paso 3: Work stealing — hilos ociosos ROBAN del fondo
            de la cola de hilos ocupados

    Hilo 0: [1,2]           (procesa [1,2])
    Hilo 1: [5,6]           (procesa [5,6])
    Hilo 2: [3,4]  <--roba  (robo de Hilo 0)
    Hilo 3: [7,8]  <--roba  (robo de Hilo 1)
```

### Por que funciona bien

- **Balanceo automatico**: si una tarea tarda mas, otros hilos
  roban las tareas pendientes.
- **Localidad**: un hilo procesa sus propias tareas primero
  (push/pop del mismo lado de la deque), lo que aprovecha cache.
- **Sin contention central**: no hay una cola central compartida,
  el robo solo ocurre cuando un hilo esta ocioso.
- **Adaptativo**: Rayon decide en runtime si paralelizar o no
  basandose en el tamano de la tarea.

---

## 9. Cuando usar y cuando NO usar Rayon

### Usar Rayon cuando

- Tienes una coleccion grande y la operacion por elemento es
  significativa (CPU-bound)
- Quieres paralelizar sin escribir boilerplate de threads
- La operacion es un map/filter/reduce sobre datos
- Necesitas divide-y-venceras recursivo (rayon::join)

### NO usar Rayon cuando

**1. El trabajo es demasiado pequeno:**

```rust
use rayon::prelude::*;

// MAL: overhead de Rayon > trabajo util
let sum: i32 = vec![1, 2, 3, 4, 5].par_iter().sum();

// BIEN: usa iterador secuencial
let sum: i32 = vec![1, 2, 3, 4, 5].iter().sum();
```

**2. El trabajo es I/O-bound (no CPU-bound):**

```rust
use rayon::prelude::*;

// MAL: los hilos del pool se bloquean esperando I/O
let contents: Vec<String> = filenames
    .par_iter()
    .map(|f| std::fs::read_to_string(f).unwrap())
    .collect();

// BIEN: usar async/await con Tokio para I/O concurrente
// o thread::scope con hilos dedicados a I/O
```

Rayon tiene un numero fijo de hilos. Si todos estan bloqueados
en I/O, no hay hilos para hacer trabajo util.

**3. Necesitas efectos laterales ordenados:**

```rust
use rayon::prelude::*;

// MAL: el orden de println es impredecible
(0..10).into_par_iter().for_each(|i| {
    println!("{}", i);  // puede salir 3, 0, 7, 1, ...
});
```

**4. Necesitas hilos de larga vida o un servidor:**

```rust
// MAL: Rayon es para tareas cortas de datos
// No lo uses como servidor de conexiones
```

### Regla general

```
    Rayon es ideal cuando:
    - Tienes N elementos
    - Cada uno requiere O(k) de CPU
    - N * k es grande (> millisegundos de trabajo total)
    - No hay I/O ni efectos laterales ordenados
```

---

## 10. Patrones practicos

### Patron 1: procesamiento de imagenes (simulado)

```rust
use rayon::prelude::*;

#[derive(Clone)]
struct Pixel {
    r: u8,
    g: u8,
    b: u8,
}

impl Pixel {
    fn grayscale(&self) -> Pixel {
        let gray = ((self.r as u16 * 299
                    + self.g as u16 * 587
                    + self.b as u16 * 114) / 1000) as u8;
        Pixel { r: gray, g: gray, b: gray }
    }
}

fn main() {
    // Simular una imagen de 1920x1080
    let width = 1920;
    let height = 1080;
    let image: Vec<Pixel> = (0..width * height)
        .map(|i| Pixel {
            r: (i % 256) as u8,
            g: (i / 256 % 256) as u8,
            b: (i / 65536 % 256) as u8,
        })
        .collect();

    // Convertir a escala de grises en paralelo
    let gray: Vec<Pixel> = image
        .par_iter()
        .map(|p| p.grayscale())
        .collect();

    println!("Processed {} pixels", gray.len());
}
```

### Patron 2: busqueda en multiples archivos

```rust
use rayon::prelude::*;
use std::path::PathBuf;

fn search_files(paths: &[PathBuf], pattern: &str) -> Vec<(PathBuf, usize, String)> {
    paths
        .par_iter()
        .flat_map(|path| {
            let content = match std::fs::read_to_string(path) {
                Ok(c) => c,
                Err(_) => return vec![],
            };

            content
                .lines()
                .enumerate()
                .filter(|(_, line)| line.contains(pattern))
                .map(|(num, line)| (path.clone(), num + 1, line.to_string()))
                .collect::<Vec<_>>()
        })
        .collect()
}
```

> **Nota**: este caso mezcla I/O con CPU. Funciona aceptablemente
> si los archivos estan en cache del OS, pero para I/O puro (red,
> disco lento), async es mejor.

### Patron 3: par_sort (sort paralelo)

Rayon proporciona `par_sort` y `par_sort_unstable` directamente:

```rust
use rayon::prelude::*;

fn main() {
    let mut data: Vec<i32> = (0..10_000_000).rev().collect();

    // Sort paralelo — usa merge sort paralelo internamente
    data.par_sort_unstable();

    assert!(data.windows(2).all(|w| w[0] <= w[1]));
    println!("sorted {} elements", data.len());
}
```

```
    Comparacion de rendimiento (10M elementos, 8 CPUs):
    +-------------------------+--------------------+
    | Metodo                  | Tiempo aproximado  |
    +-------------------------+--------------------+
    | Vec::sort_unstable()    | ~500ms             |
    | par_sort_unstable()     | ~100ms             |
    +-------------------------+--------------------+
    Speedup tipico: 3-6x (no 8x por overhead de merge)
```

### Patron 4: par_chunks para control de granularidad

```rust
use rayon::prelude::*;

fn main() {
    let data: Vec<i32> = (0..1_000_000).collect();

    // Procesar en chunks de 10,000 — cada chunk es una tarea
    let results: Vec<i64> = data
        .par_chunks(10_000)
        .map(|chunk| {
            // Cada chunk se procesa secuencialmente DENTRO del hilo
            chunk.iter().map(|&x| x as i64 * x as i64).sum::<i64>()
        })
        .collect();

    let total: i64 = results.iter().sum();
    println!("sum of squares: {}", total);
}
```

`par_chunks` es util cuando el trabajo por elemento es muy
pequeno pero el trabajo por chunk es significativo.

---

## 11. Errores comunes

### Error 1: paralelizar trabajo trivial

```rust
use rayon::prelude::*;

// MAL: el overhead de Rayon es mayor que sumar 10 numeros
fn sum_small(data: &[i32]) -> i32 {
    data.par_iter().sum()
}
```

```rust
// BIEN: usar par_iter solo con datos suficientes
fn sum_smart(data: &[i32]) -> i32 {
    if data.len() < 10_000 {
        data.iter().sum()
    } else {
        data.par_iter().sum()
    }
}
```

**Regla**: Rayon tiene overhead de ~microsegundos por operacion
paralela. Si tu trabajo total es menor que eso, la version
secuencial es mas rapida.

### Error 2: bloquear hilos del pool con I/O

```rust
use rayon::prelude::*;
use std::time::Duration;

// MAL: todos los hilos del pool bloqueados en sleep/IO
fn bad_io() {
    let urls: Vec<String> = (0..100).map(|i| format!("url_{}", i)).collect();

    urls.par_iter().for_each(|url| {
        std::thread::sleep(Duration::from_secs(1));  // simula I/O lento
        println!("fetched {}", url);
    });
    // Con 4 CPUs: solo 4 requests en paralelo, 25 segundos total
    // Con async: 100 requests en paralelo, ~1 segundo total
}
```

```rust
// BIEN: usar thread::scope o async para I/O
fn good_io() {
    let urls: Vec<String> = (0..100).map(|i| format!("url_{}", i)).collect();

    std::thread::scope(|s| {
        for url in &urls {
            s.spawn(|| {
                std::thread::sleep(Duration::from_millis(100));
                println!("fetched {}", url);
            });
        }
    });
}
```

**Regla**: Rayon es para CPU-bound. I/O-bound necesita async
o threads dedicados.

### Error 3: asumir orden en iteradores paralelos

```rust
use rayon::prelude::*;

// MAL: asumir que for_each procesa en orden
fn bad_order() {
    let data = vec![1, 2, 3, 4, 5];
    let mut results = Vec::new();

    data.par_iter().for_each(|&x| {
        // Orden impredecible + data race en results!
        // results.push(x * 2);  // NO compila sin Mutex
    });
}
```

```rust
// BIEN: usar map + collect (preserva orden)
fn good_order() {
    let data = vec![1, 2, 3, 4, 5];
    let results: Vec<i32> = data
        .par_iter()
        .map(|&x| x * 2)
        .collect();
    // results: [2, 4, 6, 8, 10] — ORDEN PRESERVADO
    println!("{:?}", results);
}
```

**Regla**: `map + collect` preserva el orden. `for_each` no
garantiza orden. `find_any` retorna cualquier match, no el
primero.

### Error 4: paralelismo anidado excesivo

```rust
use rayon::prelude::*;

// MAL: par_iter dentro de par_iter genera explosion de tareas
fn bad_nested(matrix: &Vec<Vec<i32>>) -> Vec<i32> {
    matrix
        .par_iter()
        .map(|row| {
            row.par_iter().sum::<i32>()  // par_iter anidado
        })
        .collect()
}
```

```rust
// BIEN: paralelizar solo el nivel externo
fn good_nested(matrix: &Vec<Vec<i32>>) -> Vec<i32> {
    matrix
        .par_iter()
        .map(|row| {
            row.iter().sum::<i32>()  // iter secuencial en el interior
        })
        .collect()
}
```

**Regla**: paraleliza el nivel que tiene mas trabajo. Rayon
maneja bien el anidamiento gracias a work stealing, pero es
innecesario si el nivel externo ya tiene suficiente paralelismo.

### Error 5: confundir find_any con find (primer elemento)

```rust
use rayon::prelude::*;

fn main() {
    let data: Vec<i32> = (0..1_000_000).collect();

    // find_any: retorna ALGUNO que cumple — no necesariamente el primero
    let any = data.par_iter().find_any(|&&x| x > 500_000);
    println!("find_any: {:?}", any);  // podria ser 500001, 750000, etc.

    // find_first: retorna el PRIMERO (mas lento — debe coordinar entre hilos)
    let first = data.par_iter().find_first(|&&x| x > 500_000);
    println!("find_first: {:?}", first);  // siempre 500001
}
```

**Regla**: `find_any` es mas rapido pero no determinista. Usa
`find_first` solo cuando necesites especificamente el primer
elemento.

---

## 12. Cheatsheet

```
RAYON - REFERENCIA RAPIDA
==========================

Setup:
  cargo add rayon
  use rayon::prelude::*;

Iteradores paralelos:
  .par_iter()       &T       (lectura)
  .par_iter_mut()   &mut T   (modificacion)
  .into_par_iter()  T        (consume)

Operaciones:
  .map(|x| ...)              transforma (orden preservado en collect)
  .filter(|x| ...)           filtra
  .flat_map(|x| ...)         map + flatten
  .for_each(|x| ...)         efecto lateral (sin orden)
  .sum()                     suma
  .product()                 producto
  .min() / .max()            minimo / maximo
  .reduce(|| init, |a,b| ..) reduccion paralela
  .collect()                 recoger resultados (preserva orden)
  .count()                   contar elementos
  .any() / .all()            predicados
  .find_any(|x| ...)         algun match (rapido, no determinista)
  .find_first(|x| ...)       primer match (determinista, mas lento)
  .position_any(|x| ...)     alguna posicion
  .par_chunks(n)             agrupar en chunks
  .enumerate()               con indice

Sort paralelo:
  data.par_sort()            sort estable paralelo
  data.par_sort_unstable()   sort inestable paralelo (mas rapido)
  data.par_sort_by(|a,b| ..) sort con comparador

Fork-join:
  rayon::join(|| a, || b)    ejecuta a y b en paralelo

Thread pool:
  ThreadPoolBuilder::new()
      .num_threads(N)
      .build_global()        pool global
      .build()               pool local

  pool.install(|| { ... })   ejecutar en pool local

Cuando usar:
  CPU-bound + datos grandes     --> SI
  I/O-bound                     --> NO (usar async)
  Pocos elementos (< 10,000)    --> probablemente NO
  Necesitas orden estricto      --> map+collect SI, for_each NO

Equivalencias con std:
  .iter()          ->  .par_iter()
  .iter_mut()      ->  .par_iter_mut()
  .into_iter()     ->  .into_par_iter()
  .sort()          ->  .par_sort()
  thread::scope    ->  rayon::join (reutiliza pool)
```

---

## 13. Ejercicios

### Ejercicio 1: de secuencial a paralelo

Toma este programa secuencial y conviertelo a paralelo con
Rayon. Mide la diferencia de rendimiento:

```rust
// Cargo.toml: rayon = "1"
use std::time::Instant;

fn is_prime(n: u64) -> bool {
    if n < 2 { return false; }
    if n < 4 { return true; }
    if n % 2 == 0 || n % 3 == 0 { return false; }
    let mut i = 5;
    while i * i <= n {
        if n % i == 0 || n % (i + 2) == 0 { return false; }
        i += 6;
    }
    true
}

fn main() {
    let limit = 1_000_000u64;

    // Version secuencial
    let start = Instant::now();
    let seq_count = (2..limit).filter(|&n| is_prime(n)).count();
    let seq_time = start.elapsed();
    println!("Sequential: {} primes in {:?}", seq_count, seq_time);

    // TODO: version paralela con Rayon
    // 1. Convierte el rango a into_par_iter()
    // 2. Usa filter y count
    let start = Instant::now();
    // let par_count = ...
    let par_time = start.elapsed();

    // println!("Parallel:   {} primes in {:?}", par_count, par_time);
    // assert_eq!(seq_count, par_count);
    // println!("Speedup: {:.1}x", seq_time.as_secs_f64() / par_time.as_secs_f64());
}
```

Tareas:
1. Implementa la version paralela (una linea)
2. Compara tiempos — cual es el speedup?
3. Prueba con `limit = 100` — el paralelismo sigue ayudando?
4. Prueba con `limit = 10_000_000` — como escala?

**Prediccion**: con 4 CPUs y `limit = 1_000_000`, esperas un
speedup de 4x? Sera mas o menos? Por que?

> **Pregunta de reflexion**: `is_prime` tarda mas para numeros
> grandes (el loop itera hasta sqrt(n)). Esto significa que los
> ultimos elementos del rango son mas costosos que los primeros.
> Como afecta esto al balanceo de carga? Rayon maneja bien esta
> situacion? Por que?

### Ejercicio 2: par_iter_mut para transformar datos

Implementa un "efecto sepia" sobre un vector de pixeles usando
`par_iter_mut`:

```rust
// Cargo.toml: rayon = "1"
use rayon::prelude::*;

#[derive(Debug, Clone)]
struct Pixel {
    r: u8,
    g: u8,
    b: u8,
}

impl Pixel {
    fn apply_sepia(&mut self) {
        let r = self.r as f64;
        let g = self.g as f64;
        let b = self.b as f64;

        self.r = ((r * 0.393 + g * 0.769 + b * 0.189).min(255.0)) as u8;
        self.g = ((r * 0.349 + g * 0.686 + b * 0.168).min(255.0)) as u8;
        self.b = ((r * 0.272 + g * 0.534 + b * 0.131).min(255.0)) as u8;
    }
}

fn main() {
    let width = 3840;  // 4K
    let height = 2160;
    let mut image: Vec<Pixel> = (0..width * height)
        .map(|i| Pixel {
            r: (i % 256) as u8,
            g: ((i * 7) % 256) as u8,
            b: ((i * 13) % 256) as u8,
        })
        .collect();

    println!("Image size: {} pixels", image.len());
    println!("Before: {:?}", &image[0]);

    // TODO: aplicar sepia en paralelo usando par_iter_mut + for_each

    println!("After:  {:?}", &image[0]);

    // Bonus: comparar tiempo con version secuencial
    // image.iter_mut().for_each(|p| p.apply_sepia());
}
```

Tareas:
1. Aplica `apply_sepia()` en paralelo con `par_iter_mut()`
2. Mide el tiempo y compara con la version secuencial
3. Prueba con `par_chunks_mut(1000)` y aplica sepia dentro de
   cada chunk secuencialmente. Es mas rapido o mas lento que
   `par_iter_mut`?

> **Pregunta de reflexion**: `par_iter_mut` da a cada hilo un
> `&mut Pixel` individual. `par_chunks_mut(1000)` da a cada
> hilo un `&mut [Pixel]` de 1000 elementos. En teoria, chunks
> mas grandes mejoran la localidad de cache (los pixeles estan
> contiguos en memoria). En la practica, la diferencia suele
> ser pequena para operaciones simples. Para que tipo de
> operaciones la granularidad del chunk seria mas importante?

### Ejercicio 3: reduce paralelo para encontrar estadisticas

Implementa un calculo de estadisticas (min, max, sum, count)
en paralelo usando `reduce`:

```rust
// Cargo.toml: rayon = "1"
use rayon::prelude::*;

#[derive(Debug, Clone)]
struct Stats {
    min: f64,
    max: f64,
    sum: f64,
    count: usize,
}

impl Stats {
    fn new(value: f64) -> Self {
        Stats {
            min: value,
            max: value,
            sum: value,
            count: 1,
        }
    }

    fn identity() -> Self {
        Stats {
            min: f64::INFINITY,
            max: f64::NEG_INFINITY,
            sum: 0.0,
            count: 0,
        }
    }

    fn merge(self, other: Stats) -> Stats {
        // TODO: combinar dos Stats en uno
        // min = menor de ambos
        // max = mayor de ambos
        // sum = suma de ambos
        // count = suma de ambos
        todo!()
    }

    fn mean(&self) -> f64 {
        if self.count == 0 { 0.0 } else { self.sum / self.count as f64 }
    }
}

fn main() {
    let data: Vec<f64> = (0..10_000_000)
        .map(|i| (i as f64 * 0.001).sin())
        .collect();

    // TODO: calcular Stats en paralelo usando par_iter + map + reduce
    // 1. map cada &f64 a Stats::new(*x)
    // 2. reduce con Stats::identity() y Stats::merge

    // let stats = data.par_iter()...

    // println!("count: {}", stats.count);
    // println!("min:   {:.6}", stats.min);
    // println!("max:   {:.6}", stats.max);
    // println!("mean:  {:.6}", stats.mean());

    // Verificar contra version secuencial
    let seq_min = data.iter().copied().fold(f64::INFINITY, f64::min);
    let seq_max = data.iter().copied().fold(f64::NEG_INFINITY, f64::max);
    let seq_sum: f64 = data.iter().sum();

    println!("Verification - min: {:.6}, max: {:.6}, sum: {:.2}",
             seq_min, seq_max, seq_sum);
}
```

Tareas:
1. Implementa `Stats::merge`
2. Usa `par_iter().map().reduce()` para calcular estadisticas
3. Verifica que los resultados coinciden con la version secuencial
4. Mide el speedup

> **Pregunta de reflexion**: `reduce` requiere que la operacion
> sea **asociativa** — es decir, `(a merge b) merge c == a merge
> (b merge c)`. La suma de floats NO es estrictamente asociativa
> (errores de redondeo cambian con el agrupamiento). Esto significa
> que la suma paralela puede dar un resultado ligeramente diferente
> al secuencial. En que escenarios esta diferencia importaria?
> Como afecta esto a `Stats::merge`?
