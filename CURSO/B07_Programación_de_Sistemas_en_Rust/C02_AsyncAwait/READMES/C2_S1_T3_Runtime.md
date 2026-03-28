# Runtime (Tokio)

## Indice
1. [Que es un async runtime](#1-que-es-un-async-runtime)
2. [Por que Rust no incluye un runtime](#2-por-que-rust-no-incluye-un-runtime)
3. [Tokio: el runtime estandar de facto](#3-tokio-el-runtime-estandar-de-facto)
4. [Inicializar el runtime](#4-inicializar-el-runtime)
5. [tokio::spawn: crear tareas](#5-tokiospawn-crear-tareas)
6. [JoinHandle y resultados de tareas](#6-joinhandle-y-resultados-de-tareas)
7. [join! y try_join!](#7-join-y-try_join)
8. [select!: race de Futures](#8-select-race-de-futures)
9. [Multi-thread vs current-thread runtime](#9-multi-thread-vs-current-thread-runtime)
10. [spawn_blocking: ejecutar codigo sync](#10-spawn_blocking-ejecutar-codigo-sync)
11. [yield_now y cooperative scheduling](#11-yield_now-y-cooperative-scheduling)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Que es un async runtime

En T01 y T02 vimos que un Future es una maquina de estados
inerte — no hace nada por si solo. Alguien tiene que llamar
`poll()` repetidamente, gestionar los Wakers, y decidir que
tarea ejecutar a continuacion. Ese "alguien" es el **runtime**:

```
    +-----------------------------------------------------+
    |                   Async Runtime                      |
    |                                                      |
    |  +----------+  +----------+  +----------+           |
    |  | Tarea A  |  | Tarea B  |  | Tarea C  |  ...      |
    |  | (Future) |  | (Future) |  | (Future) |           |
    |  +----------+  +----------+  +----------+           |
    |       |              |              |                |
    |       v              v              v                |
    |  +-------------------------------------------------+ |
    |  |              Executor (polea Futures)            | |
    |  +-------------------------------------------------+ |
    |       |                                              |
    |       v                                              |
    |  +-------------------------------------------------+ |
    |  |              Reactor (I/O events, timers)        | |
    |  |              epoll / kqueue / io_uring           | |
    |  +-------------------------------------------------+ |
    +-----------------------------------------------------+
```

### Componentes del runtime

```
+-------------+---------------------------------------------------+
| Componente  | Responsabilidad                                   |
+-------------+---------------------------------------------------+
| Executor    | Polea Futures, decide cual ejecutar                |
| Reactor     | Escucha eventos del OS (epoll), despierta Futures  |
| Timer       | Gestiona timeouts y sleeps                        |
| I/O driver  | Operaciones async de red y disco                  |
| Task queue  | Cola de Futures listos para ejecutar               |
+-------------+---------------------------------------------------+
```

---

## 2. Por que Rust no incluye un runtime

En JavaScript, el runtime (V8/Node event loop) esta integrado
en el lenguaje. En Rust, no — y es una decision deliberada:

- **Embedded**: un microcontrolador no puede pagar un runtime
  completo. Runtimes como `embassy` son minimalistas.
- **Servidores**: necesitan un runtime multi-hilo con I/O
  optimizado (Tokio).
- **WASM**: el browser ya tiene su propio event loop.
- **Aplicaciones de escritorio**: `async-std` o `smol` son
  mas ligeros.

Rust provee el trait `Future` y la sintaxis `async/await` en
la libreria estandar. El runtime es un crate que **tu eliges**.

### Runtimes disponibles

```
+-------------+-----------------------------------------------+
| Runtime     | Uso                                           |
+-------------+-----------------------------------------------+
| Tokio       | Servidores, networking, el mas usado           |
| async-std   | API similar a std, mas simple                  |
| smol        | Minimalista, pocas lineas de codigo             |
| embassy     | Embedded (no_std)                               |
| glommio     | Thread-per-core, io_uring                       |
+-------------+-----------------------------------------------+
```

Usamos **Tokio** porque es el estandar de la industria y el
que encontraras en la mayoria de proyectos Rust.

---

## 3. Tokio: el runtime estandar de facto

### Instalacion

```toml
# Cargo.toml
[dependencies]
tokio = { version = "1", features = ["full"] }
```

`features = ["full"]` habilita todo: macros, I/O, timers,
sync, fs, net, process, signal. En produccion puedes ser
selectivo:

```toml
# Solo lo que necesitas:
tokio = { version = "1", features = ["rt-multi-thread", "macros", "net", "time"] }
```

### Features de Tokio

```
+-------------------+----------------------------------------------+
| Feature           | Que habilita                                 |
+-------------------+----------------------------------------------+
| rt                | Runtime single-thread                        |
| rt-multi-thread   | Runtime multi-thread (work-stealing)         |
| macros            | #[tokio::main], #[tokio::test]               |
| io-util           | AsyncReadExt, AsyncWriteExt                  |
| net               | TcpListener, TcpStream, UdpSocket            |
| time              | sleep, timeout, interval                     |
| fs                | Operaciones de archivo async                 |
| sync              | Mutex, RwLock, mpsc, broadcast, oneshot       |
| process           | Command async                                |
| signal            | Manejo de senales async                      |
| full              | Todas las anteriores                         |
+-------------------+----------------------------------------------+
```

---

## 4. Inicializar el runtime

### Opcion 1: #[tokio::main] (la mas comun)

```rust
#[tokio::main]
async fn main() {
    println!("running in Tokio runtime");
    do_async_work().await;
}
```

Esta macro transforma tu `async fn main()` en:

```rust
fn main() {
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap()
        .block_on(async {
            println!("running in Tokio runtime");
            do_async_work().await;
        })
}
```

### Opcion 2: construir manualmente

```rust
fn main() {
    let rt = tokio::runtime::Runtime::new().unwrap();

    rt.block_on(async {
        println!("inside the runtime");
        do_async_work().await;
    });
}
```

`block_on` bloquea el hilo actual hasta que el Future
se complete. Es el puente entre el mundo sincrono y el async.

### Opcion 3: Builder para configuracion

```rust
fn main() {
    let rt = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(4)          // 4 hilos worker
        .thread_name("my-worker")   // nombre de los hilos
        .enable_all()               // habilitar I/O y timers
        .build()
        .unwrap();

    rt.block_on(async {
        // ...
    });
}
```

### Para tests: #[tokio::test]

```rust
#[tokio::test]
async fn test_async_operation() {
    let result = async_function().await;
    assert_eq!(result, 42);
}
```

Crea un runtime dedicado para cada test.

---

## 5. tokio::spawn: crear tareas

`tokio::spawn` lanza un Future como una **tarea independiente**
en el runtime. Es el equivalente async de `thread::spawn`:

```rust
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    // Lanzar tarea en background
    let handle = tokio::spawn(async {
        sleep(Duration::from_secs(1)).await;
        println!("background task done");
        42
    });

    println!("main continues immediately");
    sleep(Duration::from_millis(500)).await;
    println!("main still running");

    // Esperar resultado
    let result = handle.await.unwrap();
    println!("task returned: {}", result);
}
```

```
    Timeline:
    main:       [spawn]--[print]--[sleep 500ms]--[print]--[await handle]--[print]
    background:          [sleep 1s                       ]--[print "done"]
```

### spawn vs .await directo

```rust
// .await: secuencial, bloquea esta tarea
async fn sequential() {
    let a = task_a().await;  // espera a que termine
    let b = task_b().await;  // luego espera a que termine
}

// spawn: concurrente, tareas independientes
async fn concurrent() {
    let ha = tokio::spawn(task_a());
    let hb = tokio::spawn(task_b());
    let a = ha.await.unwrap();
    let b = hb.await.unwrap();
}
```

### Requisitos de spawn

```rust
pub fn spawn<F>(future: F) -> JoinHandle<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static,
```

- **`Send`**: el Future puede moverse entre hilos del runtime
- **`'static`**: el Future no contiene referencias a datos
  que se puedan destruir (usa ownership, no borrowing)

```rust
// MAL: referencia a variable local
async fn bad() {
    let data = vec![1, 2, 3];
    tokio::spawn(async {
        println!("{:?}", data);  // ERROR: data no es 'static
    });
}

// BIEN: move ownership
async fn good() {
    let data = vec![1, 2, 3];
    tokio::spawn(async move {
        println!("{:?}", data);  // OK: data movida al Future
    });
}
```

---

## 6. JoinHandle y resultados de tareas

`tokio::spawn` retorna un `JoinHandle<T>` que es un Future
que se resuelve al resultado de la tarea:

```rust
use tokio::task::JoinHandle;

#[tokio::main]
async fn main() {
    let handle: JoinHandle<i32> = tokio::spawn(async {
        expensive_computation().await
    });

    // El handle es un Future — puedes hacer .await
    match handle.await {
        Ok(result) => println!("task returned: {}", result),
        Err(e) => println!("task panicked: {}", e),
    }
}
```

### JoinHandle.await retorna Result

```rust
// JoinHandle::await retorna Result<T, JoinError>
// Ok(T): la tarea termino normalmente
// Err(JoinError): la tarea panicked o fue cancelada

let handle = tokio::spawn(async {
    panic!("oops");
});

match handle.await {
    Ok(v) => println!("value: {}", v),
    Err(e) if e.is_panic() => println!("task panicked!"),
    Err(e) if e.is_cancelled() => println!("task cancelled!"),
    Err(e) => println!("other error: {}", e),
}
```

### abort: cancelar una tarea

```rust
let handle = tokio::spawn(async {
    tokio::time::sleep(Duration::from_secs(60)).await;
    println!("this won't print");
});

// Cancelar la tarea
handle.abort();

// El await retorna JoinError con is_cancelled() == true
match handle.await {
    Err(e) if e.is_cancelled() => println!("cancelled"),
    _ => {}
}
```

### Lanzar muchas tareas y recoger resultados

```rust
use tokio::task::JoinSet;

#[tokio::main]
async fn main() {
    let mut set = JoinSet::new();

    for i in 0..10 {
        set.spawn(async move {
            tokio::time::sleep(Duration::from_millis(100 * i)).await;
            i * 10
        });
    }

    // Recoger resultados conforme terminan (no necesariamente en orden)
    while let Some(result) = set.join_next().await {
        match result {
            Ok(value) => println!("got: {}", value),
            Err(e) => println!("error: {}", e),
        }
    }
}
```

`JoinSet` es el equivalente de recoger multiples `JoinHandle`s
pero mas ergonomico. Los resultados llegan en orden de
completado, no de creacion.

---

## 7. join! y try_join!

### join!: ejecutar Futures concurrentemente

```rust
use tokio::join;
use tokio::time::{sleep, Duration};

async fn fetch_user() -> String {
    sleep(Duration::from_secs(1)).await;
    "Alice".to_string()
}

async fn fetch_orders() -> Vec<u32> {
    sleep(Duration::from_secs(2)).await;
    vec![1, 2, 3]
}

#[tokio::main]
async fn main() {
    // Ambos se ejecutan concurrentemente
    let (user, orders) = join!(fetch_user(), fetch_orders());

    println!("user: {}, orders: {:?}", user, orders);
    // Tarda ~2 segundos (el maximo), no 3 (la suma)
}
```

### join! vs spawn

```
    join!:                           spawn:
    - Misma tarea, mismo hilo        - Tareas independientes
    - Sin overhead de scheduling     - Pueden estar en diferentes hilos
    - Todos deben completar          - Puedes ignorar resultados
    - Futures no necesitan Send      - Futures deben ser Send + 'static
    - Cancelacion: si uno panics,    - Cancelacion: independiente
      los otros se cancelan
```

**Regla**: usa `join!` cuando los Futures son parte de la misma
operacion logica. Usa `spawn` cuando son tareas independientes.

### try_join!: con Result

```rust
use tokio::try_join;

async fn fetch_user() -> Result<String, String> {
    Ok("Alice".to_string())
}

async fn fetch_orders() -> Result<Vec<u32>, String> {
    Err("database error".to_string())
}

#[tokio::main]
async fn main() {
    match try_join!(fetch_user(), fetch_orders()) {
        Ok((user, orders)) => println!("{}: {:?}", user, orders),
        Err(e) => println!("error: {}", e),
        // Si CUALQUIERA falla, los demas se cancelan y se retorna el error
    }
}
```

### join! con mas de dos Futures

```rust
let (a, b, c, d) = join!(
    fetch_a(),
    fetch_b(),
    fetch_c(),
    fetch_d(),
);
// Los cuatro se ejecutan concurrentemente
```

---

## 8. select!: race de Futures

`select!` ejecuta multiples Futures y retorna cuando el
**primero** se complete. Los demas se cancelan:

```rust
use tokio::select;
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    select! {
        _ = sleep(Duration::from_secs(1)) => {
            println!("1 second passed");
        }
        _ = sleep(Duration::from_secs(5)) => {
            println!("5 seconds passed");
        }
    }
    // Siempre imprime "1 second passed"
    // El sleep de 5s se cancela
}
```

### Patron: timeout

```rust
use tokio::select;
use tokio::time::{sleep, Duration};

async fn do_work() -> String {
    sleep(Duration::from_secs(10)).await;
    "done".to_string()
}

#[tokio::main]
async fn main() {
    select! {
        result = do_work() => {
            println!("work completed: {}", result);
        }
        _ = sleep(Duration::from_secs(3)) => {
            println!("timeout after 3 seconds");
        }
    }
}
```

> **Nota**: Tokio tambien tiene `tokio::time::timeout` que es
> mas ergonomico para este patron:
> ```rust
> match timeout(Duration::from_secs(3), do_work()).await {
>     Ok(result) => println!("completed: {}", result),
>     Err(_) => println!("timeout"),
> }
> ```

### select! en un loop

```rust
use tokio::sync::mpsc;
use tokio::time::{sleep, Duration, interval};

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<String>(32);

    // Productor en background
    tokio::spawn(async move {
        for i in 0..5 {
            sleep(Duration::from_millis(800)).await;
            tx.send(format!("message {}", i)).await.unwrap();
        }
    });

    let mut tick = interval(Duration::from_secs(1));

    loop {
        select! {
            Some(msg) = rx.recv() => {
                println!("received: {}", msg);
            }
            _ = tick.tick() => {
                println!("tick");
            }
            // select! en loop: usar biased si necesitas prioridad
        }
    }
}
```

### select! con condiciones

```rust
let mut count = 0;

loop {
    select! {
        msg = rx.recv() => {
            count += 1;
            println!("message #{}: {:?}", count, msg);
        }
        _ = sleep(Duration::from_secs(5)), if count < 10 => {
            // Esta rama solo se evalua si count < 10
            println!("waiting for more messages...");
        }
        else => {
            println!("all branches disabled, exiting");
            break;
        }
    }
}
```

---

## 9. Multi-thread vs current-thread runtime

Tokio ofrece dos modos de ejecucion:

### Multi-thread (default)

```rust
#[tokio::main]  // usa multi-thread por defecto
async fn main() { ... }

// Equivalente a:
#[tokio::main(flavor = "multi_thread", worker_threads = N)]
// N = numero de CPUs por defecto
```

```
    Multi-thread runtime:
    +--------------------------------------------------+
    | Thread pool (N worker threads)                    |
    |                                                   |
    | Worker 0: [task A] [task D] [task A] ...          |
    | Worker 1: [task B] [task C] [task E] ...          |
    | Worker 2: [task F] [task B] ...                   |
    | Worker 3: [task C] [task G] ...                   |
    |                                                   |
    | Work-stealing: si un worker esta ocioso,          |
    | roba tareas de otro worker (como Rayon)            |
    +--------------------------------------------------+
```

Las tareas pueden migrar entre hilos. Por eso `spawn` requiere
`Send`.

### Current-thread (single-thread)

```rust
#[tokio::main(flavor = "current_thread")]
async fn main() { ... }
```

```
    Current-thread runtime:
    +--------------------------------------------------+
    | Un solo hilo                                      |
    |                                                   |
    | [task A] [task B] [task A] [task C] [task B] ...  |
    |                                                   |
    | Todo se ejecuta en el hilo que llamo block_on()   |
    +--------------------------------------------------+
```

No necesita `Send` para tareas locales (`tokio::task::spawn_local`).

### Cuando usar cada uno

```
+--------------------+-------------------------------------------+
| Multi-thread       | Current-thread                            |
+--------------------+-------------------------------------------+
| Servidores de alta | Clientes, scripts, herramientas CLI       |
| concurrencia       |                                           |
| Aprovecha N CPUs   | Un solo CPU                               |
| Tareas deben ser   | Tareas pueden ser !Send                   |
| Send               | (spawn_local)                             |
| Default (recomend.)| Menor overhead, mas simple                |
+--------------------+-------------------------------------------+
```

### spawn_local: tareas !Send

En el runtime current-thread, puedes usar `spawn_local` para
tareas que no son `Send`:

```rust
use std::rc::Rc;  // Rc no es Send

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let local = tokio::task::LocalSet::new();

    local.run_until(async {
        tokio::task::spawn_local(async {
            let data = Rc::new(42);  // OK: Rc en tarea local
            println!("{}", data);
        }).await.unwrap();
    }).await;
}
```

---

## 10. spawn_blocking: ejecutar codigo sync

Las funciones sincronas que bloquean (I/O de archivo, calculo
pesado, llamadas FFI) no deben ejecutarse directamente en el
runtime async. `spawn_blocking` las ejecuta en un **thread
pool dedicado**:

```rust
#[tokio::main]
async fn main() {
    // MAL: bloquea un worker thread del runtime
    // let content = std::fs::read_to_string("big_file.txt").unwrap();

    // BIEN: ejecutar en el thread pool de blocking
    let content = tokio::task::spawn_blocking(|| {
        std::fs::read_to_string("big_file.txt").unwrap()
    }).await.unwrap();

    println!("file has {} bytes", content.len());
}
```

```
    Runtime threads:                Blocking threads:
    +---------------------------+   +---------------------------+
    | Worker 0: async tasks     |   | Blocking 0: fs::read     |
    | Worker 1: async tasks     |   | Blocking 1: (idle)       |
    | Worker 2: async tasks     |   | Blocking 2: FFI call     |
    | Worker 3: async tasks     |   | ...                      |
    +---------------------------+   +---------------------------+
    Nunca bloquean                  Pueden bloquear libremente
    (max N, fijo)                   (crece bajo demanda, max 512)
```

### Cuando usar spawn_blocking

```
    Operacion          | Usar
    -------------------|------------------------------
    std::fs::*         | spawn_blocking O tokio::fs::*
    Calculo pesado CPU | spawn_blocking
    FFI (libreria C)   | spawn_blocking
    DNS lookup sync    | spawn_blocking
    Compression (zlib) | spawn_blocking
    Sleep de std       | tokio::time::sleep (no spawn_blocking)
    Networking sync    | tokio::net (no spawn_blocking)
```

### block_in_place: alternativa para multi-thread

```rust
// En el runtime multi-thread, block_in_place convierte
// el worker actual en un blocking thread temporalmente:
#[tokio::main]
async fn main() {
    let result = tokio::task::block_in_place(|| {
        // Codigo sync pesado aqui
        heavy_computation()
    });
}
```

La diferencia: `spawn_blocking` crea una nueva tarea en otro
hilo, `block_in_place` bloquea el hilo actual (mas eficiente
si ya estas en ese hilo, pero solo disponible en multi-thread).

---

## 11. yield_now y cooperative scheduling

Async Rust usa **cooperative scheduling**: las tareas deben
ceder control voluntariamente en cada `.await`. Si una tarea
no tiene puntos de await, bloquea todo:

```rust
// MAL: este loop bloquea el runtime
async fn cpu_hog() {
    let mut sum = 0u64;
    for i in 0..10_000_000_000 {
        sum += i;
    }
    // Sin .await — ninguna otra tarea puede ejecutarse
}
```

### yield_now: ceder control explicitamente

```rust
async fn cooperative_cpu() {
    let mut sum = 0u64;
    for i in 0..10_000_000_000u64 {
        sum += i;
        if i % 1_000_000 == 0 {
            tokio::task::yield_now().await;
            // Cede el hilo para que otras tareas puedan ejecutarse
        }
    }
    println!("sum: {}", sum);
}
```

### Budget de Tokio: proteccion automatica

Tokio tiene un sistema de **coop budget** que fuerza una
cesion automatica despues de cierto numero de operaciones.
Esto protege contra tareas que no ceden, pero no es perfecto:

```rust
// Tokio detecta que un poll ha durado demasiado y fuerza Pending.
// Esto funciona para operaciones de Tokio (recv, read, etc.)
// pero NO para loops puros de CPU sin operaciones de Tokio.
```

### Regla general

```
    Tu codigo async tiene .await frecuentes? -> OK
    Hay loops largos sin .await? -> Agregar yield_now().await
    Hay computo CPU pesado? -> spawn_blocking
```

---

## 12. Errores comunes

### Error 1: llamar block_on dentro de un runtime

```rust
#[tokio::main]
async fn main() {
    // MAL: block_on dentro de un runtime = panic
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(async { 42 });
    // panic: cannot start a runtime from within a runtime
}
```

```rust
// BIEN: usa .await directamente
#[tokio::main]
async fn main() {
    let result = async { 42 }.await;
}
```

**Regla**: `block_on` es el puente de sync a async. Dentro
de un runtime ya estas en async — usa `.await`.

### Error 2: no awaitar un JoinHandle

```rust
#[tokio::main]
async fn main() {
    tokio::spawn(async {
        important_work().await;
    });
    // main termina inmediatamente!
    // La tarea spawned puede no completarse
}
```

```rust
// BIEN: awaitar el handle
#[tokio::main]
async fn main() {
    let handle = tokio::spawn(async {
        important_work().await;
    });
    handle.await.unwrap();  // esperar a que termine
}
```

Si main termina, el runtime se destruye y todas las tareas
pendientes se cancelan.

### Error 3: usar std::sync::Mutex cruzando .await

```rust
use std::sync::Mutex;

// MAL: MutexGuard de std cruza un .await
async fn bad(data: &Mutex<Vec<i32>>) {
    let mut guard = data.lock().unwrap();
    do_async_work().await;  // SUSPENSION con el lock held!
    guard.push(42);
    // Si otro hilo intenta el lock durante el await, deadlock
}
```

```rust
// BIEN opcion 1: soltar el lock antes del await
async fn good1(data: &Mutex<Vec<i32>>) {
    {
        let mut guard = data.lock().unwrap();
        guard.push(42);
    }  // lock soltado aqui
    do_async_work().await;  // OK: no hay lock held
}

// BIEN opcion 2: usar tokio::sync::Mutex
use tokio::sync::Mutex;

async fn good2(data: &Mutex<Vec<i32>>) {
    let mut guard = data.lock().await;  // lock async
    do_async_work().await;  // OK con tokio::sync::Mutex
    guard.push(42);
}
```

**Regla**: `std::sync::Mutex` esta bien si el lock NO cruza un
`.await`. Si cruza un `.await`, usa `tokio::sync::Mutex`.

### Error 4: spawned task panic no detectado

```rust
#[tokio::main]
async fn main() {
    tokio::spawn(async {
        panic!("oops");
    });

    tokio::time::sleep(Duration::from_secs(1)).await;
    println!("main is fine");
    // El panic de la tarea se "trago" — nadie hizo .await en el handle
}
```

```rust
// BIEN: capturar el panic via JoinHandle
#[tokio::main]
async fn main() {
    let handle = tokio::spawn(async {
        panic!("oops");
    });

    match handle.await {
        Ok(_) => println!("task ok"),
        Err(e) => println!("task failed: {}", e),
    }
}
```

### Error 5: confundir concurrencia con paralelismo

```rust
// join! es CONCURRENTE pero NO necesariamente PARALELO
let (a, b) = join!(task_a(), task_b());
// En current_thread: alternancia en un solo hilo
// En multi_thread: pueden ejecutarse en hilos diferentes

// spawn es CONCURRENTE y POTENCIALMENTE PARALELO
let ha = tokio::spawn(task_a());
let hb = tokio::spawn(task_b());
// En multi_thread: pueden ejecutarse en hilos diferentes simultaneamente
```

```
    Concurrente (join!):           Paralelo (spawn + multi-thread):
    Hilo 1: [A][B][A][B]          Hilo 1: [A][A][A]
                                   Hilo 2: [B][B][B]
    Intercalados en 1 hilo         Simultaneos en 2 hilos
```

---

## 13. Cheatsheet

```
TOKIO RUNTIME - REFERENCIA RAPIDA
===================================

Setup:
  [dependencies]
  tokio = { version = "1", features = ["full"] }

Inicializar:
  #[tokio::main]                        macro (comun)
  #[tokio::main(flavor = "current_thread")]  single-thread
  #[tokio::test]                        para tests

  Runtime::new().unwrap().block_on(f)   manual

Crear tareas:
  tokio::spawn(future)                  tarea independiente (Send + 'static)
  tokio::task::spawn_local(future)      tarea local (!Send, current_thread)
  tokio::task::spawn_blocking(closure)  para codigo sync/CPU pesado

JoinHandle:
  handle.await           -> Result<T, JoinError>
  handle.abort()         cancelar tarea
  handle.is_finished()   verificar sin bloquear

JoinSet:
  let mut set = JoinSet::new();
  set.spawn(future);
  set.join_next().await  -> Option<Result<T, JoinError>>

Composicion:
  join!(a, b, c)        ejecutar concurrentemente, esperar TODOS
  try_join!(a, b, c)    como join! pero con Result, falla rapido
  select! {             ejecutar concurrentemente, esperar PRIMERO
      val = a => { .. }
      val = b => { .. }
  }

Runtime flavors:
  multi_thread:    N worker threads, work-stealing, tareas Send
  current_thread:  1 hilo, menor overhead, permite !Send con spawn_local

spawn_blocking:
  Para: std::fs, CPU pesado, FFI, DNS sync
  No para: networking (usar tokio::net), sleep (usar tokio::time)

Cooperative scheduling:
  .await cede control automaticamente
  yield_now().await para loops largos sin I/O
  spawn_blocking para CPU pesado

Reglas de Mutex:
  std::sync::Mutex: OK si lock NO cruza .await
  tokio::sync::Mutex: necesario si lock CRUZA .await
  Preferir: soltar lock antes de .await cuando sea posible

No hacer:
  block_on() dentro de un runtime (panic)
  std::thread::sleep en async (bloquea worker)
  Ignorar JoinHandle de tareas importantes
  Loops CPU sin .await (bloquea runtime)
```

---

## 14. Ejercicios

### Ejercicio 1: spawn y JoinSet

Lanza multiples tareas concurrentes y recoge sus resultados
conforme terminan:

```rust
use tokio::task::JoinSet;
use tokio::time::{sleep, Duration, Instant};

async fn process_item(id: u32) -> (u32, u64) {
    // Simular trabajo con duracion variable
    let work_ms = (id * 137 % 500) + 100;  // 100-600ms pseudo-random
    sleep(Duration::from_millis(work_ms as u64)).await;
    let result = id as u64 * id as u64;
    (id, result)
}

#[tokio::main]
async fn main() {
    let start = Instant::now();
    let mut set = JoinSet::new();

    // TODO:
    // 1. Lanzar 20 tareas con process_item(0..20)
    // 2. Recoger resultados con join_next() en un loop
    // 3. Imprimir cada resultado conforme llega
    // 4. Imprimir el tiempo total

    // Preguntas:
    // - Los resultados llegan en orden de ID o de completado?
    // - Cuanto tarda en total? Es la suma o el maximo de los tiempos?
}
```

Tareas:
1. Implementa con `JoinSet`
2. Compara el tiempo total con ejecutar secuencialmente (un
   `.await` tras otro en un loop)
3. Prueba con 100 tareas — todas se ejecutan concurrentemente?

**Prediccion**: 20 tareas de 100-600ms cada una. Secuencial
tardaria ~7 segundos (promedio 350ms * 20). Concurrente
tardaria ~600ms (el maximo). Acertaste?

> **Pregunta de reflexion**: `JoinSet` retorna resultados en
> orden de completado (el mas rapido primero). `join!` espera
> a todos y retorna en el orden en que los declaraste. Para un
> servidor que procesa requests, cual patron es mas apropiado?
> Por que?

### Ejercicio 2: select! con shutdown graceful

Implementa un worker que procesa mensajes de un channel pero
se detiene gracefully cuando recibe una senal de shutdown:

```rust
use tokio::sync::mpsc;
use tokio::time::{sleep, Duration};

async fn worker(mut rx: mpsc::Receiver<String>, mut shutdown: tokio::sync::oneshot::Receiver<()>) {
    let mut processed = 0u32;

    loop {
        tokio::select! {
            // TODO: branch para recibir mensajes de rx
            // Imprimir cada mensaje y contar processed
            // Si rx se cierra (None), salir del loop

            // TODO: branch para la senal de shutdown
            // Imprimir mensaje de shutdown y salir del loop
        }
    }

    println!("Worker shutting down. Processed {} messages.", processed);
}

#[tokio::main]
async fn main() {
    let (tx, rx) = mpsc::channel::<String>(32);
    let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel::<()>();

    // Lanzar worker
    let worker_handle = tokio::spawn(worker(rx, shutdown_rx));

    // Enviar mensajes
    for i in 0..10 {
        tx.send(format!("message {}", i)).await.unwrap();
        sleep(Duration::from_millis(100)).await;
    }

    // Enviar shutdown
    println!("Sending shutdown signal...");
    shutdown_tx.send(()).unwrap();

    // Esperar a que el worker termine
    worker_handle.await.unwrap();
    println!("Worker terminated cleanly.");
}
```

Tareas:
1. Completa las ramas del `select!`
2. Ejecuta y verifica que el worker procesa mensajes y luego
   se detiene
3. Modifica para que el shutdown se envie despues de 500ms
   (no despues de todos los mensajes) — el worker deberia
   procesar ~5 mensajes y luego parar
4. Agrega un timeout: si no hay mensajes en 2 segundos, el
   worker se auto-detiene

> **Pregunta de reflexion**: cuando `select!` elige la rama de
> shutdown, puede haber mensajes pendientes en `rx` que no se
> procesaron. En un sistema real (por ejemplo, un pipeline de
> datos), perder mensajes es inaceptable. Como modificarias el
> worker para que, al recibir shutdown, deje de aceptar mensajes
> nuevos pero procese todos los pendientes antes de salir?
> (Pista: este patron se llama "drain".)

### Ejercicio 3: spawn_blocking vs async

Compara el rendimiento de operaciones CPU-bound en async vs
spawn_blocking:

```rust
use std::time::Instant;
use tokio::task;

fn fibonacci(n: u64) -> u64 {
    if n <= 1 { return n; }
    let mut a = 0u64;
    let mut b = 1u64;
    for _ in 2..=n {
        let tmp = a + b;
        a = b;
        b = tmp;
    }
    b
}

#[tokio::main]
async fn main() {
    let n = 1_000_000u64;

    // Version 1: directamente en async (BLOQUEA el runtime)
    let start = Instant::now();
    let result = fibonacci(n);
    let time1 = start.elapsed();
    println!("Direct: fib({}) last digits = ...{} in {:?}",
             n, result % 1000, time1);

    // Version 2: con spawn_blocking
    let start = Instant::now();
    let result = task::spawn_blocking(move || fibonacci(n))
        .await
        .unwrap();
    let time2 = start.elapsed();
    println!("spawn_blocking: fib({}) last digits = ...{} in {:?}",
             n, result % 1000, time2);

    // Version 3: multiples fibonacci concurrentes
    // TODO: lanzar 8 fibonacci(n) concurrentes con spawn_blocking
    // TODO: lanzar 8 fibonacci(n) concurrentes con spawn (async puro)
    // Comparar tiempos — cual es mas rapido? Por que?

    // Pista: spawn de computo CPU puro bloquea workers del runtime
    // spawn_blocking los ejecuta en hilos dedicados
}
```

Tareas:
1. Implementa la version 3 con ambos enfoques
2. Mide y compara tiempos
3. Mientras corren las 8 fibonacci en `spawn` (sin blocking),
   intenta que otra tarea async haga algo simple (como imprimir
   un mensaje cada 100ms) — se bloquea?

> **Pregunta de reflexion**: `spawn_blocking` tiene un thread
> pool que crece hasta 512 hilos por defecto. Si lanzas 1000
> tareas `spawn_blocking` que cada una duerme 10 segundos,
> cuantas se ejecutan en paralelo? Que pasa con las demas?
> Esto es un riesgo real — como lo mitigarias?
