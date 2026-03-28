# Scoped Threads

## Indice
1. [El problema de los lifetimes con threads](#1-el-problema-de-los-lifetimes-con-threads)
2. [std::thread::scope (Rust 1.63+)](#2-stdthreadscope-rust-163)
3. [Como funciona internamente](#3-como-funciona-internamente)
4. [Comparacion con thread::spawn](#4-comparacion-con-threadspawn)
5. [Multiples hilos con referencias al mismo dato](#5-multiples-hilos-con-referencias-al-mismo-dato)
6. [Scoped threads con mutabilidad](#6-scoped-threads-con-mutabilidad)
7. [Patrones practicos](#7-patrones-practicos)
8. [Limitaciones de scoped threads](#8-limitaciones-de-scoped-threads)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema de los lifetimes con threads

Con `thread::spawn`, el closure debe ser `'static` — no puede
contener referencias a datos del stack del hilo padre:

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3, 4, 5];

    // Esto NO compila:
    thread::spawn(|| {
        println!("{:?}", data);  // data es una referencia al stack de main
    });
    // error[E0373]: closure may outlive the current function
}
```

El compilador tiene razon: `thread::spawn` crea un hilo que
**puede** vivir mas que la funcion que lo lanzo. Si `main`
termina antes que el hilo, `data` se destruye y el hilo
accederia a memoria invalida.

### La solucion clasica: move + clone/Arc

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3, 4, 5];

    // Opcion 1: move (transfiere ownership)
    let handle = thread::spawn(move || {
        println!("{:?}", data);
    });
    // data ya no existe en main
    handle.join().unwrap();
}
```

```rust
use std::sync::Arc;
use std::thread;

fn main() {
    let data = Arc::new(vec![1, 2, 3, 4, 5]);

    // Opcion 2: Arc (cuando necesitas datos en ambos lados)
    let data_clone = Arc::clone(&data);
    let handle = thread::spawn(move || {
        println!("{:?}", data_clone);
    });

    println!("main aun tiene data: {:?}", data);
    handle.join().unwrap();
}
```

Ambas opciones funcionan, pero tienen costes:
- `move` te quita el ownership — ya no puedes usar el dato
- `Arc` agrega conteo de referencias atomico (heap + overhead)

Muchas veces solo quieres "prestar" datos a unos hilos, esperar
a que terminen, y seguir usando los datos. Exactamente para
eso existen los **scoped threads**.

---

## 2. std::thread::scope (Rust 1.63+)

`std::thread::scope` crea un **ambito** (scope) que garantiza
que todos los hilos lanzados dentro terminan antes de que el
scope retorne. Esto permite usar referencias sin `'static`:

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3, 4, 5];
    let label = String::from("my data");

    thread::scope(|s| {
        s.spawn(|| {
            // Puede usar &data y &label directamente!
            println!("{}: {:?}", label, data);
        });

        s.spawn(|| {
            // Otro hilo tambien puede leer &data
            let sum: i32 = data.iter().sum();
            println!("sum = {}", sum);
        });
    });
    // Aqui se garantiza que ambos hilos han terminado.

    // data y label siguen siendo validos:
    println!("data still here: {:?}", data);
}
```

No hay `move`, no hay `Arc`, no hay `clone`. Los hilos
**toman prestado** `&data` y `&label`, y el compilador sabe
que es seguro porque el scope fuerza el join.

### Firma simplificada

```rust
pub fn scope<'env, F, T>(f: F) -> T
where
    F: for<'scope> FnOnce(&'scope Scope<'scope, 'env>) -> T,
```

- `'env`: el lifetime del entorno (los datos del stack)
- `'scope`: el lifetime del scope (no puede escapar del closure)
- `Scope::spawn` acepta closures con lifetime `'scope`, no `'static`

---

## 3. Como funciona internamente

```
    main()
    |
    | let data = vec![...];
    |
    +-- thread::scope(|s| { ... })
    |   |
    |   |  s.spawn(|| { ... &data ... });  --> Hilo A
    |   |  s.spawn(|| { ... &data ... });  --> Hilo B
    |   |
    |   +-- scope termina:
    |       1. Espera que Hilo A termine (join implicito)
    |       2. Espera que Hilo B termine (join implicito)
    |       3. Si algun hilo panicked, propaga el panic
    |
    | <-- Aqui data sigue vivo, todos los hilos terminaron
    |
    v
```

### Garantias del scope

1. **Join implicito**: todos los hilos se joinean al salir del
   scope, incluso si el closure de `scope` panicked.

2. **Propagacion de panics**: si un hilo scoped panicked, el
   panic se propaga despues de que todos los hilos terminen.

3. **Lifetime safety**: el compilador verifica que las
   referencias tomadas por los hilos no escapan del scope.

### Que pasa si un hilo panicked

```rust
use std::thread;

fn main() {
    let result = std::panic::catch_unwind(|| {
        thread::scope(|s| {
            s.spawn(|| {
                panic!("hilo 1 exploto");
            });

            s.spawn(|| {
                // Este hilo se completa normalmente
                println!("hilo 2 OK");
            });
        });
    });

    match result {
        Ok(_) => println!("todo bien"),
        Err(_) => println!("algun hilo panicked"),
    }
    // Output:
    // hilo 2 OK
    // algun hilo panicked
}
```

El scope espera a **todos** los hilos (incluso despues de un
panic), y luego propaga el primer panic.

---

## 4. Comparacion con thread::spawn

```
+-----------------------------+----------------------------+
| thread::spawn               | thread::scope              |
+-----------------------------+----------------------------+
| Closure: 'static + Send     | Closure: 'scope + Send     |
| Requiere move o Arc         | Puede tomar &referencias   |
| JoinHandle manual           | Join implicito al salir    |
| Hilo vive independiente     | Hilo ligado al scope       |
| Para hilos de larga vida    | Para paralelismo local     |
+-----------------------------+----------------------------+
```

### Ejemplo lado a lado

```rust
use std::sync::Arc;
use std::thread;

fn with_spawn() {
    let data = vec![1, 2, 3];

    // Necesitas Arc + clone para compartir
    let data = Arc::new(data);
    let d1 = Arc::clone(&data);
    let d2 = Arc::clone(&data);

    let h1 = thread::spawn(move || {
        println!("spawn: {:?}", d1);
    });
    let h2 = thread::spawn(move || {
        let sum: i32 = d2.iter().sum();
        println!("spawn sum: {}", sum);
    });

    h1.join().unwrap();
    h2.join().unwrap();
    // data (el Arc original) aun existe, pero el Vec esta en el heap
}

fn with_scope() {
    let data = vec![1, 2, 3];

    // Sin Arc, sin clone, sin move
    thread::scope(|s| {
        s.spawn(|| {
            println!("scope: {:?}", data);
        });
        s.spawn(|| {
            let sum: i32 = data.iter().sum();
            println!("scope sum: {}", sum);
        });
    });
    // data sigue en el stack, sin allocation extra
}
```

### Cuando usar cada uno

```
    Necesito que el hilo viva mas alla del scope actual?
    +-- Si  --> thread::spawn (con move/Arc)
    +-- No  --> thread::scope (mas simple, sin overhead)

    Necesito un thread pool / worker de larga vida?
    +-- Si  --> thread::spawn
    +-- No  --> thread::scope

    Solo quiero paralelizar un calculo y recoger resultados?
    +-- Si  --> thread::scope (ideal)
```

---

## 5. Multiples hilos con referencias al mismo dato

Scoped threads permiten que multiples hilos compartan `&T`
(referencia compartida) sin ninguna sincronizacion extra,
siempre que solo lean:

```rust
use std::thread;

fn main() {
    let matrix = vec![
        vec![1, 2, 3],
        vec![4, 5, 6],
        vec![7, 8, 9],
    ];

    let mut row_sums = vec![0; matrix.len()];

    thread::scope(|s| {
        // Cada hilo lee una fila diferente de la matriz
        // Todos comparten &matrix (lectura)
        for (i, row) in matrix.iter().enumerate() {
            s.spawn(move || {
                // move solo mueve i y row (&[i32] — una referencia)
                let sum: i32 = row.iter().sum();
                println!("row {}: sum = {}", i, sum);
                sum
            });
        }
    });

    println!("matrix still valid: {:?}", matrix);
}
```

> **Nota**: `move` aqui mueve `i` (usize, Copy) y `row`
> (que es `&Vec<i32>`, una referencia — copiar una referencia
> es barato). NO mueve la matriz ni las filas.

### Recoger resultados con JoinHandle

`s.spawn()` retorna un `ScopedJoinHandle` que puedes usar
para obtener el valor de retorno:

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3, 4, 5, 6, 7, 8];

    // Dividir en dos mitades y sumar en paralelo
    let (left, right) = data.split_at(data.len() / 2);

    let (sum_left, sum_right) = thread::scope(|s| {
        let h1 = s.spawn(|| -> i32 {
            left.iter().sum()
        });
        let h2 = s.spawn(|| -> i32 {
            right.iter().sum()
        });

        (h1.join().unwrap(), h2.join().unwrap())
    });

    println!("left: {}, right: {}, total: {}",
             sum_left, sum_right, sum_left + sum_right);
    // left: 10, right: 26, total: 36
}
```

---

## 6. Scoped threads con mutabilidad

### Escritura exclusiva: cada hilo su porcion

El borrow checker de Rust permite que **un** hilo tenga
`&mut` a un dato, o multiples hilos tengan `&`. Scoped
threads respetan esta regla. Para que multiples hilos
**escriban**, cada uno necesita su propia porcion exclusiva:

```rust
use std::thread;

fn main() {
    let mut results = vec![0u64; 4];

    thread::scope(|s| {
        // split_at_mut divide el slice en porciones exclusivas
        // Cada hilo recibe &mut sobre su porcion
        for (i, chunk) in results.chunks_mut(1).enumerate() {
            s.spawn(move || {
                // chunk es &mut [u64] — acceso exclusivo
                chunk[0] = (i as u64 + 1) * 100;
            });
        }
    });

    println!("{:?}", results);  // [100, 200, 300, 400]
}
```

```
    results: [_, _, _, _]
              |  |  |  |
    Hilo 0: &mut[_]  |  |  |   chunk[0] = 100
    Hilo 1:    &mut[_]  |  |   chunk[0] = 200
    Hilo 2:       &mut[_]  |   chunk[0] = 300
    Hilo 3:          &mut[_]   chunk[0] = 400

    Resultado: [100, 200, 300, 400]
    Sin locks, sin atomics, verificado por el borrow checker.
```

### Escritura compartida: necesitas Mutex

Si multiples hilos necesitan escribir en el **mismo** dato
(no en porciones separadas), necesitas sincronizacion:

```rust
use std::sync::Mutex;
use std::thread;

fn main() {
    let results = Mutex::new(Vec::new());

    thread::scope(|s| {
        for i in 0..4 {
            s.spawn(|| {
                // &Mutex<Vec> — referencia compartida al Mutex
                let mut vec = results.lock().unwrap();
                vec.push(i * 10);
            });
        }
    });

    let final_results = results.into_inner().unwrap();
    println!("{:?}", final_results);
    // Orden variable: [0, 30, 10, 20] (depende del scheduling)
}
```

Nota que no necesitas `Arc<Mutex<...>>` — el scoped thread
toma `&Mutex<Vec<_>>` directamente.

---

## 7. Patrones practicos

### Patron 1: paralelizar un map

```rust
use std::thread;

fn parallel_map<T, R, F>(input: &[T], f: F) -> Vec<R>
where
    T: Sync,
    R: Send,
    F: Fn(&T) -> R + Sync,
{
    let mut output = Vec::with_capacity(input.len());

    thread::scope(|s| {
        let handles: Vec<_> = input
            .iter()
            .map(|item| s.spawn(|| f(item)))
            .collect();

        for h in handles {
            output.push(h.join().unwrap());
        }
    });

    output
}

fn main() {
    let numbers = vec![1, 2, 3, 4, 5, 6, 7, 8];
    let squares = parallel_map(&numbers, |x| x * x);
    println!("{:?}", squares);  // [1, 4, 9, 16, 25, 36, 49, 64]
    // numbers sigue disponible:
    println!("original: {:?}", numbers);
}
```

### Patron 2: divide y venceras

```rust
use std::thread;

fn parallel_sum(data: &[i64]) -> i64 {
    const THRESHOLD: usize = 1000;

    if data.len() <= THRESHOLD {
        return data.iter().sum();
    }

    let mid = data.len() / 2;
    let (left, right) = data.split_at(mid);

    thread::scope(|s| {
        let left_handle = s.spawn(|| parallel_sum(left));
        let right_sum = parallel_sum(right);  // reusar este hilo
        left_handle.join().unwrap() + right_sum
    })
}

fn main() {
    let data: Vec<i64> = (1..=10_000).collect();
    let sum = parallel_sum(&data);
    println!("sum = {}", sum);  // 50005000

    // Verificar con la formula: n*(n+1)/2
    assert_eq!(sum, 10_000 * 10_001 / 2);
}
```

```
    parallel_sum([1, 2, 3, 4, 5, 6, 7, 8])
    |
    +-- scope:
        |
        +-- hilo: parallel_sum([1, 2, 3, 4])
        |         |
        |         +-- scope:
        |             +-- hilo: sum([1, 2]) = 3
        |             +-- this:  sum([3, 4]) = 7
        |             = 10
        |
        +-- this: parallel_sum([5, 6, 7, 8])
                  |
                  +-- scope:
                      +-- hilo: sum([5, 6]) = 11
                      +-- this:  sum([7, 8]) = 15
                      = 26
        |
        = 10 + 26 = 36
```

### Patron 3: procesar chunks en paralelo

```rust
use std::thread;

fn main() {
    let mut data = vec![0u32; 1_000_000];

    // Llenar en paralelo: cada hilo procesa un chunk
    let num_threads = 4;
    let chunk_size = data.len() / num_threads;

    thread::scope(|s| {
        for (chunk_idx, chunk) in data.chunks_mut(chunk_size).enumerate() {
            s.spawn(move || {
                let start = chunk_idx * chunk_size;
                for (i, elem) in chunk.iter_mut().enumerate() {
                    *elem = (start + i) as u32;
                }
            });
        }
    });

    // Verificar
    assert_eq!(data[0], 0);
    assert_eq!(data[999_999], 999_999);
    println!("First: {}, Last: {}", data[0], data[data.len() - 1]);
}
```

### Patron 4: scope que retorna un valor

```rust
use std::thread;

fn find_max(data: &[i32], num_threads: usize) -> Option<i32> {
    if data.is_empty() {
        return None;
    }

    let chunk_size = (data.len() + num_threads - 1) / num_threads;

    let max = thread::scope(|s| {
        let handles: Vec<_> = data
            .chunks(chunk_size)
            .map(|chunk| {
                s.spawn(move || {
                    chunk.iter().copied().max().unwrap()
                })
            })
            .collect();

        handles
            .into_iter()
            .map(|h| h.join().unwrap())
            .max()
            .unwrap()
    });

    Some(max)
}

fn main() {
    let data: Vec<i32> = (0..100_000).collect();
    let max = find_max(&data, 4);
    println!("max = {:?}", max);  // Some(99999)
}
```

---

## 8. Limitaciones de scoped threads

### 1. No puedes almacenar ScopedJoinHandle fuera del scope

```rust
use std::thread;

fn main() {
    let mut handle_outside = None;

    thread::scope(|s| {
        let h = s.spawn(|| 42);
        handle_outside = Some(h);  // ERROR: 'scope no vive lo suficiente
    });

    // Si esto compilara, podrias hacer join despues de que el scope
    // garantizo que el hilo ya termino — inconsistente.
}
```

### 2. Los hilos no pueden escapar del scope

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3];

    thread::scope(|s| {
        s.spawn(|| {
            // No puedes re-lanzar un hilo que viva mas que el scope
            // thread::spawn(|| { ... }) aqui crearia un hilo 'static
            // que podria referenciar datos del scope — rechazado

            println!("{:?}", data);  // OK: &data dentro del scope
        });
    });
}
```

### 3. Crear demasiados hilos

```rust
use std::thread;

fn main() {
    let data: Vec<i32> = (0..100_000).collect();

    // MAL: 100,000 hilos del OS — probablemente crash
    thread::scope(|s| {
        for item in &data {
            s.spawn(|| {
                let _ = item * 2;
            });
        }
    });
}
```

Cada `s.spawn` crea un hilo real del OS (con su propio stack,
tipicamente 2-8 MB). Para paralelismo masivo, usa **Rayon**
(siguiente topico) que gestiona un thread pool.

### 4. No reemplaza a thread::spawn para hilos de larga vida

```rust
// Servidor que acepta conexiones — necesita thread::spawn
fn server() {
    // Los hilos de conexion viven independientemente
    loop {
        let conn = accept_connection();
        std::thread::spawn(move || {
            handle_connection(conn);  // puede tardar indefinidamente
        });
    }
}

// thread::scope NO sirve aqui: el scope bloquea hasta que
// todos los hilos terminen, y queremos seguir aceptando.
```

---

## 9. Errores comunes

### Error 1: intentar mover ownership dentro de un scoped thread

```rust
use std::thread;

fn main() {
    let data = vec![1, 2, 3];

    thread::scope(|s| {
        s.spawn(move || {
            let owned = data;  // move de data al hilo
            println!("{:?}", owned);
        });

        s.spawn(|| {
            println!("{:?}", data);  // ERROR: data fue movida
        });
    });
}
```

```rust
// BIEN: no uses move, toma referencias
fn main() {
    let data = vec![1, 2, 3];

    thread::scope(|s| {
        s.spawn(|| {
            println!("{:?}", &data);  // &data
        });

        s.spawn(|| {
            println!("{:?}", &data);  // &data — multiples lectores OK
        });
    });
}
```

**Regla**: la gracia de scoped threads es evitar `move`. Si
usas `move`, pierdes la ventaja principal.

### Error 2: intentar &mut desde multiples hilos

```rust
use std::thread;

fn main() {
    let mut data = vec![0; 4];

    thread::scope(|s| {
        s.spawn(|| {
            data[0] = 1;  // &mut data
        });
        s.spawn(|| {
            data[1] = 2;  // &mut data — ERROR: dos &mut al mismo Vec
        });
    });
}
```

```rust
// BIEN: dividir en chunks exclusivos
fn main() {
    let mut data = vec![0; 4];

    thread::scope(|s| {
        for chunk in data.chunks_mut(1) {
            s.spawn(move || {
                chunk[0] = 42;  // cada hilo tiene su &mut [i32]
            });
        }
    });

    println!("{:?}", data);  // [42, 42, 42, 42]
}
```

**Por que falla**: Rust no puede verificar que `data[0]` y
`data[1]` no se solapan. `chunks_mut` crea slices garantizados
sin solapamiento, que el borrow checker acepta.

### Error 3: olvidar que scope bloquea

```rust
use std::thread;
use std::time::Duration;

fn main() {
    println!("antes del scope");

    thread::scope(|s| {
        s.spawn(|| {
            thread::sleep(Duration::from_secs(5));
            println!("hilo terminado");
        });
    });
    // scope BLOQUEA aqui 5 segundos esperando al hilo

    println!("despues del scope");  // se imprime 5s despues
}
```

Esto es el comportamiento **correcto** y deseado — es lo que
hace seguras las referencias. Pero si esperas comportamiento
asincrono, te sorprendera.

### Error 4: captura de referencia a variable del scope

```rust
use std::thread;

fn main() {
    thread::scope(|s| {
        let local = String::from("hello");

        s.spawn(|| {
            println!("{}", local);  // ERROR: local vive en el closure del scope,
                                    // no en main. El hilo podria vivir mas que local
                                    // si otro hilo paniqueara.
        });
    });
}
```

```rust
// BIEN: declarar fuera del scope
fn main() {
    let local = String::from("hello");  // vive en main

    thread::scope(|s| {
        s.spawn(|| {
            println!("{}", local);  // OK: local vive mas que el scope
        });
    });
}
```

**Regla**: los datos referenciados por hilos scoped deben vivir
**fuera** del closure de `scope`, no dentro de el.

### Error 5: crear un hilo por elemento en una coleccion grande

```rust
use std::thread;

// MAL: un hilo del OS por cada elemento
fn sum_bad(data: &[i32]) -> i32 {
    let results = std::sync::Mutex::new(0);
    thread::scope(|s| {
        for &item in data {
            s.spawn(|| {
                *results.lock().unwrap() += item;
            });
        }
    });
    results.into_inner().unwrap()
}
```

```rust
// BIEN: chunks proporcionales al numero de CPUs
fn sum_good(data: &[i32]) -> i32 {
    let num_threads = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);
    let chunk_size = (data.len() + num_threads - 1) / num_threads;

    thread::scope(|s| {
        let handles: Vec<_> = data
            .chunks(chunk_size)
            .map(|chunk| s.spawn(move || chunk.iter().sum::<i32>()))
            .collect();

        handles.into_iter().map(|h| h.join().unwrap()).sum()
    })
}
```

**Regla**: crea tantos hilos como CPUs, no como elementos.
Usa `std::thread::available_parallelism()` para obtener el
numero de CPUs logicas.

---

## 10. Cheatsheet

```
SCOPED THREADS - REFERENCIA RAPIDA
====================================

Uso basico:
  use std::thread;

  thread::scope(|s| {
      s.spawn(|| { ... });      // hilo 1
      s.spawn(|| { ... });      // hilo 2
  });
  // Aqui ambos hilos han terminado (join implicito)

Ventajas sobre thread::spawn:
  - Sin 'static: closures pueden tomar &referencias locales
  - Sin Arc: no necesitas conteo de referencias atomico
  - Sin move: los datos quedan disponibles despues del scope
  - Join implicito: no puedes olvidar hacer join

Disponible desde: Rust 1.63 (julio 2022)

Compartir datos:
  Solo lectura:   &data en multiples hilos (T: Sync)
  Escritura:      chunks_mut(n) para porciones exclusivas
  Escritura comp: &Mutex<T> (sin Arc, porque el scope garantiza lifetime)

Recoger resultados:
  let handle = s.spawn(|| value);
  let result = handle.join().unwrap();

  // O retornar del scope:
  let result = thread::scope(|s| {
      let h = s.spawn(|| compute());
      h.join().unwrap()
  });

Numero de hilos:
  let cpus = std::thread::available_parallelism()
      .map(|n| n.get())
      .unwrap_or(4);

Cuando usar scope vs spawn:
  scope:  paralelismo local, calculos, procesar datos
  spawn:  hilos de larga vida, servidores, workers

Cuando usar scope vs rayon:
  scope:  pocos hilos, control manual, sin dependencia extra
  rayon:  paralelismo masivo, iteradores, thread pool automatico

Reglas:
  - Datos referenciados deben vivir FUERA del closure de scope
  - No puedes almacenar ScopedJoinHandle fuera del scope
  - El scope BLOQUEA hasta que todos los hilos terminen
  - Un hilo por CPU, no un hilo por elemento
  - Preferir &T sobre move dentro de scoped threads
```

---

## 11. Ejercicios

### Ejercicio 1: suma paralela con scoped threads

Implementa una funcion que sume un slice de numeros en paralelo
dividiendo el trabajo entre N hilos:

```rust
use std::thread;

fn parallel_sum(data: &[i64], num_threads: usize) -> i64 {
    if data.is_empty() {
        return 0;
    }

    let chunk_size = (data.len() + num_threads - 1) / num_threads;

    thread::scope(|s| {
        // TODO:
        // 1. Dividir data en chunks de tamano chunk_size
        // 2. Lanzar un hilo por chunk que calcule la suma parcial
        // 3. Recoger los resultados con join y sumar todo
        todo!()
    })
}

fn main() {
    let data: Vec<i64> = (1..=1_000_000).collect();

    // Version secuencial para comparar
    let expected: i64 = data.iter().sum();

    // Version paralela
    let cpus = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);
    let result = parallel_sum(&data, cpus);

    assert_eq!(result, expected);
    println!("Sum: {} (using {} threads)", result, cpus);

    // Bonus: medir tiempo de ambas versiones
    // use std::time::Instant;
    // ...
}
```

Tareas:
1. Implementa `parallel_sum` usando `chunks()` y `s.spawn()`
2. Verifica que el resultado coincide con la suma secuencial
3. Mide el rendimiento con `Instant::now()` y compara secuencial
   vs paralelo para diferentes tamanos de input
4. Prueba con 1, 2, 4, 8, 16 hilos — cual da mejor resultado?

**Prediccion**: con un millon de elementos, la version paralela
sera mas rapida? A partir de que tamano de input crees que el
paralelismo empieza a ganar?

> **Pregunta de reflexion**: los hilos scoped toman `&data` (una
> referencia a un slice del stack). Con `thread::spawn`, necesitarias
> `Arc<Vec<i64>>` y clonar el Arc para cada hilo. Para esta tarea
> (sumar y descartar), la diferencia de rendimiento entre ambos
> enfoques es minima. Pero si `data` fuera de 1 GB y necesitaras
> los datos despues, por que scoped threads serian estrictamente
> superiores?

### Ejercicio 2: modificar un slice en paralelo

Crea una funcion que transforme cada elemento de un slice mutable
en paralelo, aplicando una funcion:

```rust
use std::thread;

fn parallel_map_in_place<T, F>(data: &mut [T], f: F)
where
    T: Send,
    F: Fn(&mut T) + Sync,
{
    let cpus = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);
    let chunk_size = (data.len() + cpus - 1) / cpus;

    // TODO:
    // 1. Dividir data en chunks_mut(chunk_size)
    // 2. Lanzar un hilo por chunk
    // 3. Cada hilo aplica f a cada elemento de su chunk
    thread::scope(|s| {
        todo!()
    });
}

fn main() {
    let mut numbers: Vec<i32> = (0..20).collect();
    println!("antes: {:?}", numbers);

    // Duplicar cada numero
    parallel_map_in_place(&mut numbers, |x| *x *= 2);
    println!("despues: {:?}", numbers);
    // [0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38]

    // Cuadrado de cada numero
    parallel_map_in_place(&mut numbers, |x| *x = *x * *x);
    println!("cuadrados: {:?}", numbers);

    // Verificar
    let expected: Vec<i32> = (0..20).map(|x| (x * 2) * (x * 2)).collect();
    assert_eq!(numbers, expected);
}
```

Tareas:
1. Implementa la funcion usando `chunks_mut` y scoped threads
2. Verifica que cada elemento se transforma correctamente
3. Piensa: por que `F: Fn(&mut T) + Sync` y no `F: Fn(&mut T) + Send`?
4. Que pasa si cambias `Fn` a `FnMut`? Compila? Por que?

> **Pregunta de reflexion**: `chunks_mut` divide el slice en
> porciones no solapadas, lo que permite que el borrow checker
> verifique que no hay data races. Si en lugar de `chunks_mut`
> intentaras pasar indices y acceder con `data[start..end]`,
> el compilador rechazaria el codigo. Por que? Que informacion
> tiene `chunks_mut` que los indices no proporcionan?

### Ejercicio 3: busqueda paralela con early return

Implementa una busqueda en paralelo donde los hilos pueden
"cancelarse" tempranamente cuando otro hilo encuentra el resultado:

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;

fn parallel_find<T, F>(data: &[T], predicate: F) -> Option<usize>
where
    T: Sync,
    F: Fn(&T) -> bool + Sync,
{
    let found = AtomicBool::new(false);

    let cpus = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);
    let chunk_size = (data.len() + cpus - 1) / cpus;

    thread::scope(|s| {
        let handles: Vec<_> = data
            .chunks(chunk_size)
            .enumerate()
            .map(|(chunk_idx, chunk)| {
                s.spawn(|| {
                    let base = chunk_idx * chunk_size;
                    for (i, item) in chunk.iter().enumerate() {
                        // TODO: verificar si otro hilo ya encontro resultado
                        // Si found es true, retornar None tempranamente

                        if predicate(item) {
                            // TODO: marcar found como true
                            return Some(base + i);
                        }
                    }
                    None
                })
            })
            .collect();

        // Recoger el primer Some encontrado
        handles
            .into_iter()
            .filter_map(|h| h.join().unwrap())
            .next()
    })
}

fn main() {
    let data: Vec<u64> = (0..10_000_000).collect();

    // Buscar el primer multiplo de 7919 mayor que 5_000_000
    let idx = parallel_find(&data, |&x| x > 5_000_000 && x % 7919 == 0);

    match idx {
        Some(i) => println!("Found {} at index {}", data[i], i),
        None => println!("Not found"),
    }

    // Verificar con busqueda secuencial
    let expected = data.iter().position(|&x| x > 5_000_000 && x % 7919 == 0);
    println!("Sequential found at: {:?}", expected);
}
```

Tareas:
1. Completa los `TODO` usando el `AtomicBool found`
2. Elige el `Ordering` correcto para load/store de `found`
3. Ejecuta y verifica que encuentra el mismo indice que la
   version secuencial
4. Nota: los hilos no devuelven necesariamente el **primer**
   indice global. Como podrias modificar el codigo para
   garantizar el primer indice?

> **Pregunta de reflexion**: el flag `AtomicBool` para cancelacion
> temprana es un patron comun, pero tiene una limitacion: un hilo
> podria verificar `found == false`, y justo despues otro hilo lo
> cambia a `true`, causando trabajo desperdiciado. En la practica,
> esto es aceptable. Pero, seria correcto usar `Ordering::Relaxed`
> para el flag? El "retraso" en la visibilidad afecta la correccion
> del resultado, o solo el rendimiento?
