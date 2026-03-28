# std::thread::spawn

## Índice
1. [Hilos en Rust vs C](#1-hilos-en-rust-vs-c)
2. [Crear un hilo con spawn](#2-crear-un-hilo-con-spawn)
3. [JoinHandle y join](#3-joinhandle-y-join)
4. [move closures](#4-move-closures)
5. [Retornar valores desde hilos](#5-retornar-valores-desde-hilos)
6. [Múltiples hilos](#6-múltiples-hilos)
7. [Builder: nombre y stack size](#7-builder-nombre-y-stack-size)
8. [Panics en hilos](#8-panics-en-hilos)
9. [Cuándo usar hilos](#9-cuándo-usar-hilos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Hilos en Rust vs C

En C (pthreads), crear un hilo es manual y propenso a errores:
no hay protección contra data races, memory leaks de join
olvidados, o acceso a datos liberados.

Rust resuelve esto con el sistema de tipos:

```
C (pthreads):                         Rust (std::thread):
  pthread_create(&tid, NULL, fn, arg)   thread::spawn(closure)
  pthread_join(tid, &result)            handle.join()

  Data races: responsabilidad tuya      Data races: error de compilación
  Join olvidado: leak/zombies           Join olvidado: thread se detach
  Dangling pointer al arg: UB           move closure: ownership transferido
```

### Garantías de Rust en compile time

```rust
// Esto NO compila:
let mut data = vec![1, 2, 3];
std::thread::spawn(|| {
    data.push(4);  // error: closure may outlive the current function
});
data.push(5);  // data se usa en dos lugares → data race
```

El compilador detecta que `data` sería accedida por dos hilos
sin sincronización. En C, este código compila y produce un data
race silencioso.

---

## 2. Crear un hilo con spawn

`std::thread::spawn` toma un closure y lo ejecuta en un hilo
nuevo:

```rust
use std::thread;
use std::time::Duration;

fn main() {
    // Crear un hilo
    thread::spawn(|| {
        for i in 1..=5 {
            println!("hilo: {i}");
            thread::sleep(Duration::from_millis(100));
        }
    });

    // El main sigue ejecutándose en paralelo
    for i in 1..=3 {
        println!("main: {i}");
        thread::sleep(Duration::from_millis(150));
    }

    println!("main terminó");
}
```

### Salida posible (entrelazada)

```
main: 1
hilo: 1
hilo: 2
main: 2
hilo: 3
main: 3
hilo: 4
main terminó
```

### ¿Qué pasó con `hilo: 5`?

Cuando `main()` termina, el proceso termina **inmediatamente**,
matando todos los hilos que aún estén corriendo. El hilo no
alcanzó a imprimir `hilo: 5`:

```
main termina
  → el proceso llama exit()
  → todos los threads mueren
  → hilo: 5 nunca se ejecutó
```

Esto es idéntico al comportamiento de pthreads en C cuando
el main thread retorna sin join.

---

## 3. JoinHandle y join

`spawn` retorna un `JoinHandle<T>` que permite esperar a que
el hilo termine:

```rust
use std::thread;

fn main() {
    let handle = thread::spawn(|| {
        for i in 1..=5 {
            println!("hilo: {i}");
        }
    });

    // Esperar a que el hilo termine
    handle.join().unwrap();

    println!("hilo terminó, ahora continúa main");
}
```

### Salida (garantizada en orden)

```
hilo: 1
hilo: 2
hilo: 3
hilo: 4
hilo: 5
hilo terminó, ahora continúa main
```

`join()` bloquea el hilo actual hasta que el hilo spawneado
termine. Retorna `Result<T, Box<dyn Any + Send>>`:

```rust
let handle = thread::spawn(|| 42);

match handle.join() {
    Ok(value)  => println!("hilo retornó: {value}"),
    Err(panic) => println!("hilo panicked: {panic:?}"),
}
```

### ¿Qué pasa si no haces join?

Si el `JoinHandle` se dropea sin join, el hilo se convierte en
**detached** — sigue corriendo pero no puedes esperar su
resultado:

```rust
fn main() {
    // JoinHandle se dropea inmediatamente (no se asigna a variable)
    thread::spawn(|| {
        // Este hilo es detached
        // Puede o no terminar antes de que main salga
        println!("¿se imprime?");
    });
    // main termina → proceso muere → hilo muere
}
```

No es un memory leak ni un error — es un detach implícito.
Pero normalmente quieres join para:
1. Asegurar que el trabajo se completó
2. Obtener el resultado
3. Detectar panics

---

## 4. move closures

Por defecto, un closure **toma prestadas** (borrow) las
variables del scope. Pero un hilo puede vivir más que el scope
que lo creó, así que el borrow checker lo rechaza:

```rust
fn main() {
    let name = String::from("Alice");

    // ERROR: closure may outlive `name`
    thread::spawn(|| {
        println!("hello {name}");
    });
    // `name` se dropea aquí... pero el hilo podría seguir usándola
}
```

### La solución: move

`move` transfiere el **ownership** de las variables capturadas
al closure. El hilo se convierte en dueño:

```rust
fn main() {
    let name = String::from("Alice");

    thread::spawn(move || {
        // name fue movida al closure — el hilo es dueño
        println!("hello {name}");
    });

    // name ya no existe aquí
    // println!("{name}");  // ERROR: value used after move
}
```

### Diagrama de ownership

```
Sin move:
  main owns name ─── borrow ──▶ closure (hilo)
       │                             │
       ▼ drop(name)                  ▼ ¿usa name?
    name liberada                 dangling reference
    → el compilador lo impide

Con move:
  main owns name ─── move ──▶ closure (hilo) owns name
       │                             │
    name ya no existe aquí      drop(name) cuando el hilo termine
    → seguro
```

### Capturar múltiples variables

`move` mueve **todas** las variables capturadas:

```rust
let name = String::from("Alice");
let age = 30;
let scores = vec![100, 95, 88];

thread::spawn(move || {
    // name, age, scores: todos movidos al hilo
    println!("{name} ({}): {:?}", age, scores);
});

// name, scores: ya no disponibles
// age: disponible (i32 es Copy, move lo copia)
```

Para tipos que implementan `Copy` (i32, f64, bool, char),
`move` crea una copia — el original sigue disponible. Para
tipos no-Copy (String, Vec, Box), el original se invalida.

### ¿Y si necesito el dato en ambos lados?

```rust
let data = vec![1, 2, 3];

// Necesito data en el hilo Y en main después
// Opción 1: clonar
let data_clone = data.clone();
thread::spawn(move || {
    println!("{data_clone:?}");
});
println!("{data:?}");  // OK: data no fue movida

// Opción 2: Arc (shared ownership) — próximo tópico
```

---

## 5. Retornar valores desde hilos

El valor que retorna el closure se obtiene con `join()`:

```rust
use std::thread;

fn main() {
    // El hilo retorna un i32
    let handle = thread::spawn(|| -> i32 {
        let mut sum = 0;
        for i in 1..=100 {
            sum += i;
        }
        sum
    });

    let result = handle.join().unwrap();
    println!("Suma 1..100 = {result}");  // 5050
}
```

### Retornar tipos complejos

```rust
let handle = thread::spawn(|| {
    let mut primes = Vec::new();
    for n in 2..1000 {
        if is_prime(n) {
            primes.push(n);
        }
    }
    primes  // retorna Vec<u32>
});

let primes: Vec<u32> = handle.join().unwrap();
println!("Found {} primes", primes.len());
```

### Retornar Result

```rust
use std::fs;
use std::io;

let handle = thread::spawn(|| -> io::Result<String> {
    let content = fs::read_to_string("/etc/hostname")?;
    Ok(content.trim().to_string())
});

match handle.join() {
    Ok(Ok(hostname)) => println!("hostname: {hostname}"),
    Ok(Err(e))       => eprintln!("I/O error: {e}"),
    Err(panic)       => eprintln!("thread panicked: {panic:?}"),
}
```

El `join()` retorna `Result<T, PanicInfo>`. Si el closure
retorna `Result<String, io::Error>`, tenemos un `Result`
anidado: `Result<Result<String, io::Error>, PanicInfo>`.

---

## 6. Múltiples hilos

Crear N hilos y esperar a todos:

```rust
use std::thread;

fn main() {
    let mut handles = Vec::new();

    for i in 0..4 {
        let handle = thread::spawn(move || {
            let result = expensive_computation(i);
            println!("Thread {i}: result = {result}");
            result
        });
        handles.push(handle);
    }

    // Esperar a todos y recolectar resultados
    let results: Vec<i32> = handles
        .into_iter()
        .map(|h| h.join().unwrap())
        .collect();

    println!("All results: {results:?}");
}

fn expensive_computation(id: usize) -> i32 {
    // Simular trabajo
    thread::sleep(std::time::Duration::from_millis(100));
    (id * id) as i32
}
```

### Patrón fork-join

```
main:     spawn(0) → spawn(1) → spawn(2) → spawn(3)
              │          │          │          │
              ▼          ▼          ▼          ▼
threads:   work(0)    work(1)    work(2)    work(3)
              │          │          │          │
              ▼          ▼          ▼          ▼
main:     join(0) → join(1) → join(2) → join(3)
              │          │          │          │
              └──────────┴──────────┴──────────┘
                              │
                    recolectar resultados
```

### ¿Por qué `move` en el loop?

```rust
for i in 0..4 {
    let handle = thread::spawn(move || {
        println!("{i}");
    });
}
```

Sin `move`, el closure captura `&i`. Pero `i` cambia en cada
iteración del loop, y los hilos pueden ejecutarse después de
que el loop termine:

```rust
// Sin move (no compila):
for i in 0..4 {
    thread::spawn(|| {
        println!("{i}");  // &i: ¿cuál i? la del scope del loop
    });
}
// Después del loop, i ya no existe → dangling reference
```

Con `move`, cada closure recibe su propia **copia** de `i`
(porque `usize` es `Copy`):

```
Iteración 0: closure captura i=0 (copia)
Iteración 1: closure captura i=1 (copia)
Iteración 2: closure captura i=2 (copia)
Iteración 3: closure captura i=3 (copia)
```

---

## 7. Builder: nombre y stack size

`thread::Builder` permite configurar el hilo antes de crearlo:

```rust
use std::thread;

fn main() {
    let builder = thread::Builder::new()
        .name("worker-1".into())
        .stack_size(4 * 1024 * 1024);  // 4 MB stack

    let handle = builder.spawn(move || {
        let name = thread::current().name()
            .unwrap_or("unnamed")
            .to_string();
        println!("Running in thread: {name}");
    }).expect("failed to spawn thread");

    handle.join().unwrap();
}
```

### Nombre del hilo

El nombre aparece en:
- `thread::current().name()` — para logging
- Debuggers (gdb, lldb) — para identificar hilos
- `/proc/<pid>/task/<tid>/comm` en Linux
- Panic messages

```
thread 'worker-1' panicked at 'index out of bounds'
         ^^^^^^^^^
         nombre del hilo
```

Sin nombre, el panic dice `thread '<unnamed>'`.

### Stack size

Por defecto, cada hilo tiene un stack de **8 MB** (configurable
con `RUST_MIN_STACK` env var). Para hilos que hacen poco trabajo,
puedes reducirlo para ahorrar memoria virtual:

```rust
// 64 KB stack — suficiente para trabajo ligero
thread::Builder::new()
    .stack_size(64 * 1024)
    .spawn(|| { /* ... */ })
    .unwrap();
```

### Builder::spawn vs thread::spawn

```rust
// thread::spawn: panics si falla (raro pero posible)
let handle = thread::spawn(|| {});

// Builder::spawn: retorna Result (puedes manejar el error)
let handle = thread::Builder::new()
    .spawn(|| {})
    .expect("failed to create thread");
```

`thread::spawn` es un atajo para `Builder::new().spawn()` con
unwrap implícito. En la práctica, la creación de hilos casi
nunca falla (solo si el OS no tiene recursos), así que
`thread::spawn` es suficiente para la mayoría de casos.

---

## 8. Panics en hilos

Si un hilo paniquea, **solo ese hilo muere**. Los demás hilos
y el main thread siguen vivos:

```rust
use std::thread;

fn main() {
    let h1 = thread::spawn(|| {
        panic!("thread 1 crashed!");
    });

    let h2 = thread::spawn(|| {
        println!("thread 2: working fine");
        42
    });

    // h1 panicked — join retorna Err
    match h1.join() {
        Ok(_)    => println!("h1 OK"),
        Err(e)   => println!("h1 panicked: {e:?}"),
    }

    // h2 completó normalmente — join retorna Ok
    match h2.join() {
        Ok(val)  => println!("h2 returned: {val}"),
        Err(e)   => println!("h2 panicked: {e:?}"),
    }
}
```

### Salida

```
thread 'unnamed' panicked at 'thread 1 crashed!'
thread 2: working fine
h1 panicked: Any { .. }
h2 returned: 42
```

### Propagación de panics

Si quieres que el panic del hilo termine el programa:

```rust
// Opción 1: unwrap (panic en main si el hilo panicked)
handle.join().unwrap();

// Opción 2: re-panic con el error original
match handle.join() {
    Ok(val) => val,
    Err(e)  => std::panic::resume_unwind(e),
}
```

### Comparación con C

```
C (pthreads):
  Segfault en un thread → todo el proceso muere (SIGSEGV)
  No hay forma de "capturar" un crash en un thread

Rust:
  Panic en un thread → solo ese thread muere
  join() retorna Err → puedes manejar el error
  (segfault sigue matando todo el proceso, pero los panics son controlados)
```

---

## 9. Cuándo usar hilos

### Sí usar hilos

```
✓ Trabajo CPU-intensivo que se puede paralelizar
    → procesar N archivos simultáneamente
    → cálculos matemáticos independientes

✓ Operaciones I/O blocking que queremos hacer en paralelo
    → leer de múltiples archivos
    → conexiones de red simultáneas (si no usas async)

✓ Tareas en background
    → logging, métricas, watchdog
```

### No usar hilos (considerar alternativas)

```
✗ Muchas conexiones concurrentes (>1000)
    → mejor async/await (Tokio) — un hilo OS por conexión no escala

✗ Paralelismo de datos simple
    → mejor rayon (par_iter) — gestiona el pool automáticamente

✗ Tareas que necesitan compartir mucho estado mutable
    → considerar canales (message passing) en vez de mutex
```

### Hilos vs async

```
Hilos (std::thread):
  1 hilo OS por tarea
  Stack completo (~8MB)
  Scheduling por el kernel
  Blocking I/O es natural
  Bueno para: CPU-bound, pocas tareas concurrentes

Async (tokio):
  N tareas en M hilos (M << N)
  Stack mínimo (~KB)
  Scheduling por el runtime
  Non-blocking I/O obligatorio
  Bueno para: I/O-bound, muchas tareas concurrentes
```

---

## 10. Errores comunes

### Error 1: olvidar move en el closure

```rust
let name = String::from("Alice");

// ERROR: closure may outlive the current function
thread::spawn(|| {
    println!("{name}");
});
```

**Solución**: usar `move`:

```rust
thread::spawn(move || {
    println!("{name}");
});
```

### Error 2: mover un valor que necesitas después

```rust
let data = vec![1, 2, 3];

thread::spawn(move || {
    println!("{data:?}");
});

// ERROR: value used after move
println!("{data:?}");
```

**Solución**: clonar antes de mover, o usar Arc (próximo tópico):

```rust
let data = vec![1, 2, 3];
let data_clone = data.clone();

thread::spawn(move || {
    println!("{data_clone:?}");
});

println!("{data:?}");  // OK: usamos el original
```

### Error 3: no hacer join (hilo muere con main)

```rust
fn main() {
    thread::spawn(move || {
        // Trabajo importante...
        save_to_database(results);
    });
    // main retorna → proceso termina → hilo muere
    // save_to_database nunca se ejecutó
}
```

**Solución**: hacer join antes de retornar:

```rust
fn main() {
    let handle = thread::spawn(move || {
        save_to_database(results);
    });
    handle.join().unwrap();  // esperar a que termine
}
```

### Error 4: ignorar el Result de join

```rust
let handle = thread::spawn(|| {
    panic!("crash!");
});

// MAL: unwrap paniquea en main si el hilo panicked
handle.join().unwrap();
// → main también paniquea → difícil de depurar
```

**Solución**: manejar el error si el panic no debe propagar:

```rust
if let Err(e) = handle.join() {
    eprintln!("Worker thread failed: {e:?}");
    // Tomar acción correctiva o log
}
```

### Error 5: asumir orden de ejecución

```rust
let h1 = thread::spawn(|| println!("primero"));
let h2 = thread::spawn(|| println!("segundo"));
h1.join().unwrap();
h2.join().unwrap();
// "segundo" puede imprimirse ANTES que "primero"
// El scheduler del OS decide el orden
```

**Solución**: si necesitas orden, usa sincronización (channels,
barriers, condvars) o ejecuta secuencialmente.

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              std::thread — Hilos en Rust                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CREAR HILO:                                                 │
│    let handle = thread::spawn(move || { ... });              │
│    handle.join().unwrap();                                    │
│                                                              │
│  MOVE CLOSURE:                                               │
│    move || { ... }    transfiere ownership al hilo           │
│    Copy types: se copian (i32, bool, f64)                    │
│    Non-Copy: se mueven (String, Vec, Box)                    │
│                                                              │
│  JOIN:                                                       │
│    handle.join()  → Result<T, Box<dyn Any + Send>>           │
│    Ok(value)      → hilo terminó normalmente                 │
│    Err(panic)     → hilo panicked                            │
│    Sin join       → hilo detached (muere con main)           │
│                                                              │
│  RETORNAR VALOR:                                             │
│    let h = thread::spawn(|| 42);                             │
│    let val = h.join().unwrap();  // val = 42                 │
│                                                              │
│  MÚLTIPLES HILOS:                                            │
│    let handles: Vec<_> = (0..N)                              │
│        .map(|i| thread::spawn(move || work(i)))              │
│        .collect();                                           │
│    let results: Vec<_> = handles                             │
│        .into_iter().map(|h| h.join().unwrap()).collect();     │
│                                                              │
│  BUILDER:                                                    │
│    thread::Builder::new()                                    │
│        .name("worker".into())                                │
│        .stack_size(4 * 1024 * 1024)                          │
│        .spawn(move || { ... })                               │
│        .expect("spawn failed");                              │
│                                                              │
│  UTILIDADES:                                                 │
│    thread::current()           hilo actual                    │
│    thread::current().name()    nombre del hilo               │
│    thread::current().id()      ThreadId único                │
│    thread::sleep(duration)     dormir                        │
│    thread::yield_now()         ceder al scheduler            │
│    thread::available_parallelism()  número de cores          │
│                                                              │
│  PANICS:                                                     │
│    Panic en hilo → solo ese hilo muere                       │
│    join() → Err(panic_info)                                  │
│    resume_unwind(e) → re-panic en el hilo actual            │
│                                                              │
│  REGLAS:                                                     │
│    Closures para spawn deben ser 'static + Send              │
│    'static: no puede tener borrows a stack de main           │
│    Send: el tipo puede cruzar boundary de threads            │
│    → usar move para transferir ownership                     │
│    → clonar si necesitas el dato en ambos lados              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: cálculo paralelo con fork-join

Calcula la suma de los cuadrados de 1..=1_000_000 distribuyendo
el trabajo en N hilos.

**Pasos**:
1. Divide el rango 1..=1_000_000 en N chunks (usa `chunks` en
   un Vec o calcula rangos manualmente)
2. Spawn un hilo por chunk con `move`
3. Cada hilo calcula la suma de cuadrados de su rango y la retorna
4. `join()` todos los hilos y suma los resultados parciales
5. Verifica contra la fórmula:
   `n(n+1)(2n+1)/6 = 333_333_833_333_500_000`

**Predicción antes de ejecutar**: si usas 4 hilos, ¿cada uno
procesa exactamente 250_000 elementos? ¿La suma final es la
misma independientemente del número de hilos? ¿Qué tipo
necesitas para la suma (i32, i64, u64)?

> **Pregunta de reflexión**: el overhead de crear 4 hilos
> (~microsegundos) vs el cálculo (milisegundos en 1M elementos)
> hace que la paralelización valga la pena. ¿A partir de qué
> tamaño de rango deja de valer la pena? ¿Qué pasaría si
> crearas 1_000_000 hilos (uno por número)?

### Ejercicio 2: procesamiento de strings en paralelo

Procesa una lista de strings en paralelo: cada hilo convierte
un grupo de strings a uppercase y retorna el resultado.

**Pasos**:
1. Crea un `Vec<String>` con 20 palabras
2. Divide el Vec en 4 chunks (usa `split_at` o `clone` ranges)
3. Cada hilo recibe ownership de su chunk (clone el sub-Vec
   antes de mover)
4. Cada hilo retorna `Vec<String>` con las palabras en uppercase
5. Concatena los resultados de todos los hilos

**Predicción antes de ejecutar**: si clonas el Vec completo para
cada hilo, ¿cada hilo tiene una copia independiente? ¿Los
resultados siempre están en el mismo orden que el input, o
dependen de qué hilo termina primero?

> **Pregunta de reflexión**: clonamos los datos para poder
> moverlos al hilo. Esto duplica la memoria. ¿Cómo evitarías
> la copia? (Pista: `Arc<[String]>` con slicing, o
> `thread::scope` que veremos en T03 de S02.) ¿Cuál es el
> trade-off entre simplicidad y eficiencia?

### Ejercicio 3: Builder con nombres y manejo de panics

Crea hilos con nombres y maneja panics correctamente.

**Pasos**:
1. Crea 5 hilos con `thread::Builder::new().name(...)`
2. El hilo 3 debe hacer `panic!("worker 3 failed")`
3. Los demás retornan su id * 10
4. Haz join de todos en orden — cuando un join retorna Err,
   imprime el nombre del hilo y el error, pero continúa con
   los demás
5. Imprime los resultados de los hilos que completaron OK

**Predicción antes de ejecutar**: ¿el panic del hilo 3 afecta
a los hilos 4 y 5? ¿El mensaje de panic incluye el nombre del
hilo? ¿Qué tipo tiene el `Err` de `join()` — puedes extraer
el string del panic?

> **Pregunta de reflexión**: `join()` retorna
> `Result<T, Box<dyn Any + Send>>`. El `Any` puede ser
> downcast a `String` o `&str` para obtener el mensaje. ¿Cómo
> harías ese downcast? ¿Qué pasa si el panic fue con un valor
> que no es string (por ejemplo, `panic!(42)`)?
