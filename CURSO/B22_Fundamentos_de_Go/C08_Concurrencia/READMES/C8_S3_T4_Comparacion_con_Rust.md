# Comparacion con Rust — goroutines vs tokio tasks, channels vs mpsc, mutex vs Mutex\<T\>, share-memory vs message-passing

## 1. Introduccion

Go y Rust resuelven la concurrencia desde filosofias opuestas. Go apuesta por **simplicidad en tiempo de ejecucion** — un scheduler integrado, goroutines baratas, y un garbage collector que libera al programador de pensar en ownership. Rust apuesta por **garantias en tiempo de compilacion** — el borrow checker elimina data races antes de ejecutar una sola linea, y el programador elige explicitamente su modelo de concurrencia (threads del OS, async/await con tokio, rayon para paralelismo de datos).

Ninguno es "mejor". Son trade-offs fundamentales que impactan desde la latencia de tail hasta la facilidad de onboarding de un equipo nuevo. Este topico compara ambos ecosistemas en profundidad: modelo de ejecucion, primitivas de sincronizacion, patrones de comunicacion, y garantias de seguridad.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              CONCURRENCIA: GO vs RUST                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  FILOSOFIA FUNDAMENTAL                                                     │
│  ┌──────────────────────────────┐  ┌──────────────────────────────────────┐│
│  │          GO                   │  │            RUST                      ││
│  │                               │  │                                      ││
│  │  "Don't communicate by       │  │  "Fearless concurrency"             ││
│  │   sharing memory; share      │  │                                      ││
│  │   memory by communicating"   │  │  El compilador IMPIDE data races    ││
│  │                               │  │  en tiempo de compilacion.          ││
│  │  El runtime DETECTA data     │  │  Si compila, es memory-safe.        ││
│  │  races con -race flag.       │  │                                      ││
│  │  Si no usas -race, puede     │  │  No hay "modo debug". La seguridad  ││
│  │  haber bugs silenciosos.     │  │  es el modo por defecto.            ││
│  │                               │  │                                      ││
│  │  Modelo: CSP + shared memory │  │  Modelo: Ownership + Send/Sync     ││
│  │  Runtime: scheduler M:N      │  │  Runtime: opt-in (tokio, async-std) ││
│  │  GC: si                      │  │  GC: no                             ││
│  └──────────────────────────────┘  └──────────────────────────────────────┘│
│                                                                             │
│  GARANTIAS DE SEGURIDAD                                                    │
│  ┌───────────────────────┬──────────────────────┬─────────────────────────┐│
│  │ Propiedad              │ Go                   │ Rust                    ││
│  ├───────────────────────┼──────────────────────┼─────────────────────────┤│
│  │ Data races             │ Detecta (-race)      │ PREVIENE (compilador)  ││
│  │ Memory leaks           │ GC los previene      │ Posibles (Rc cycles)   ││
│  │ Goroutine/task leaks   │ Posibles             │ Posibles               ││
│  │ Deadlocks              │ Posibles             │ Posibles               ││
│  │ Use-after-free         │ GC lo previene       │ PREVIENE (borrow check)││
│  │ Null pointer           │ Panic en nil deref   │ PREVIENE (Option<T>)   ││
│  └───────────────────────┴──────────────────────┴─────────────────────────┘│
│                                                                             │
│  DECISION RAPIDA                                                           │
│  ├─ Necesitas onboarding rapido → Go                                      │
│  ├─ Necesitas latencia predecible (no GC pauses) → Rust                   │
│  ├─ Microservicios web standard → Go (net/http es excelente)              │
│  ├─ Systems programming, embedded, WASM critico → Rust                    │
│  ├─ Equipo grande, distintos niveles → Go (curva de aprendizaje menor)    │
│  └─ Necesitas zero-cost abstractions → Rust                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Goroutines vs Tokio Tasks vs OS Threads

### 2.1 Modelo de ejecucion

**Go: scheduler integrado en el runtime (M:N)**

Go mapea N goroutines sobre M OS threads. El scheduler es parte del binario compilado — no es una libreria externa, no es configurable (mas alla de `GOMAXPROCS`), y no requiere anotacion del programador. Cada goroutine empieza con ~2KB de stack que crece dinamicamente.

```go
// Go: lanzar 100,000 goroutines es trivial
package main

import (
    "fmt"
    "sync"
)

func main() {
    var wg sync.WaitGroup
    
    for i := 0; i < 100_000; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            // Trabajo...
            _ = id * id
        }(i)
    }
    
    wg.Wait()
    fmt.Println("100,000 goroutines completadas")
}
```

**Rust: sin runtime por defecto — elige tu modelo**

Rust no tiene runtime de concurrencia integrado. Tienes tres opciones principales:

```rust
// Opcion 1: OS threads (std::thread)
// Cada thread = ~8MB de stack, scheduled por el kernel
use std::thread;

fn main() {
    let handles: Vec<_> = (0..8) // 8 threads, no 100,000
        .map(|id| {
            thread::spawn(move || {
                // Trabajo...
                id * id
            })
        })
        .collect();
    
    for handle in handles {
        let result = handle.join().unwrap();
        println!("Resultado: {}", result);
    }
}
```

```rust
// Opcion 2: tokio tasks (async runtime, M:N como Go)
// Necesita dependencia: tokio = { version = "1", features = ["full"] }
use tokio;

#[tokio::main]
async fn main() {
    let mut handles = Vec::new();
    
    for i in 0..100_000 {
        handles.push(tokio::spawn(async move {
            // Trabajo async...
            i * i
        }));
    }
    
    for handle in handles {
        let result = handle.await.unwrap();
        let _ = result;
    }
    println!("100,000 tasks completadas");
}
```

```rust
// Opcion 3: rayon (paralelismo de datos, work-stealing)
// Necesita dependencia: rayon = "1.10"
use rayon::prelude::*;

fn main() {
    let sum: i64 = (0..100_000i64)
        .into_par_iter()  // Paralelismo automatico
        .map(|i| i * i)
        .sum();
    
    println!("Suma: {}", sum);
}
```

### 2.2 Comparacion detallada

```
┌───────────────────┬────────────────────────┬────────────────────────┬──────────────────────┐
│ Propiedad          │ Go goroutine           │ Rust tokio::spawn      │ Rust std::thread     │
├───────────────────┼────────────────────────┼────────────────────────┼──────────────────────┤
│ Stack inicial      │ ~2KB (crece dynamic.)  │ Sin stack propio       │ ~8MB (configurable)  │
│ Modelo             │ M:N (scheduler Go)     │ M:N (scheduler tokio)  │ 1:1 (kernel)         │
│ Sintaxis           │ go f()                 │ tokio::spawn(async {}) │ thread::spawn(|| {}) │
│ Retorno de valor   │ No (usar channel)      │ Si (JoinHandle<T>)     │ Si (JoinHandle<T>)   │
│ Cancelacion        │ context.Context        │ CancellationToken,     │ Manual (flag atomico) │
│                    │                        │ tokio::select!, abort  │                      │
│ Panic handling     │ Mata solo la goroutine │ JoinError, catch_unwind│ thread::Result       │
│ Crear 100K         │ Trivial (~200MB)       │ Trivial (~pocas MB)    │ Imposible (~800GB)   │
│ Scheduling         │ Cooperativo (puntos    │ Cooperativo (cada      │ Preemptivo (kernel)  │
│                    │ de yield implicitos)   │ .await es yield)       │                      │
│ Preemption         │ Si (desde Go 1.14,     │ No (bloquear sin       │ Si (kernel)          │
│                    │ basado en sysmon)      │ .await bloquea thread) │                      │
│ I/O model          │ Netpoller integrado    │ epoll/kqueue/iocp      │ Blocking por defecto │
│ Color de funciones │ No (todo es sync)      │ Si (async fn vs fn)    │ No                   │
│ Overhead           │ Runtime siempre (~4MB) │ Solo si usas tokio     │ Minimo               │
└───────────────────┴────────────────────────┴────────────────────────┴──────────────────────┘
```

### 2.3 El problema del "function coloring"

Go no tiene funciones "async" y funciones "sync" — todas las funciones son iguales. El netpoller del runtime convierte I/O bloqueante en non-blocking de forma transparente:

```go
// Go: TODA funcion puede ser concurrente
func fetchData(url string) ([]byte, error) {
    resp, err := http.Get(url)  // NO bloquea el OS thread
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()
    return io.ReadAll(resp.Body) // Tampoco bloquea
}

// Puedes llamarla sincrona o concurrentemente — la funcion NO CAMBIA
result, _ := fetchData("http://example.com")        // sincrono
go func() { result, _ := fetchData("http://...") }() // concurrente
```

En Rust, las funciones async son un tipo diferente — retornan `Future`, necesitan `.await`, y no puedes mezclarlas libremente con funciones sincronas:

```rust
// Rust: async fn retorna impl Future<Output = T>
async fn fetch_data(url: &str) -> Result<String, reqwest::Error> {
    let body = reqwest::get(url)  // Retorna Future
        .await?                    // Necesitas .await para resolver
        .text()
        .await?;
    Ok(body)
}

// NO puedes llamar una async fn desde contexto sincrono sin runtime:
// let data = fetch_data("http://..."); // Esto NO ejecuta nada, solo crea un Future

// Necesitas un runtime:
#[tokio::main]
async fn main() {
    let data = fetch_data("http://example.com").await.unwrap();
}

// O block_on desde codigo sincrono:
fn sync_wrapper() -> String {
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(fetch_data("http://example.com")).unwrap()
}
```

```
FUNCTION COLORING — impacto practico:

Go:
  main() ──→ handler() ──→ fetchDB() ──→ query()
  Todas sync. Cualquiera puede tener I/O.
  Agregar concurrencia = agregar "go" delante.

Rust async:
  main() ──→ async handler() ──→ async fetchDB() ──→ async query()
    │              │                    │                  │
    │              └─ .await ───────────┘                  │
    └─ necesita runtime ───────────────────────────────────┘
  
  Si query() se vuelve async, TODO lo que la llama debe ser async.
  Esto es el "contagio" de async — un cambio en una hoja propaga
  hasta la raiz del call stack.
```

### 2.4 Scheduling: cooperativo vs preemptivo

```go
// Go: preemption desde Go 1.14
// Loops infinitos ya NO bloquean el scheduler
func cpuIntensive() {
    for {
        // Antes de Go 1.14: esto bloqueaba el P indefinidamente
        // Desde Go 1.14: el sysmon inyecta preemption via senales
        heavyComputation()
    }
}

func main() {
    go cpuIntensive()
    go cpuIntensive()
    // Ambas goroutines pueden ejecutarse en paralelo
    // sin que una monopolice el scheduler
    time.Sleep(time.Second)
}
```

```rust
// Rust tokio: cooperativo SIN preemption
// Un loop CPU-bound sin .await BLOQUEA el thread del runtime
async fn cpu_intensive_BAD() {
    loop {
        heavy_computation(); // Nunca hace yield!
        // Este task monopoliza su thread del runtime.
        // Otros tasks en ese thread se mueren de hambre.
    }
}

// Solucion 1: yield explicitamente
async fn cpu_intensive_yield() {
    loop {
        heavy_computation();
        tokio::task::yield_now().await; // Cede el control
    }
}

// Solucion 2: mover a un thread dedicado
async fn cpu_intensive_proper() {
    tokio::task::spawn_blocking(|| {
        loop {
            heavy_computation();
        }
    }).await.unwrap();
}
```

### 2.5 Panic y recuperacion

```go
// Go: un panic en una goroutine mata ESA goroutine
// Si no se recupera, mata el proceso entero
func riskyWork() {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("recovered: %v", r)
        }
    }()
    panic("algo salio mal")
}

func main() {
    go riskyWork() // El panic se recupera dentro de la goroutine
    // PERO: no puedes recover() un panic de OTRA goroutine
    // Si la goroutine no tiene recover, el programa entero muere
    time.Sleep(time.Second)
}
```

```rust
// Rust: panics son capturados por JoinHandle
use std::thread;

fn main() {
    let handle = thread::spawn(|| {
        panic!("algo salio mal");
    });
    
    // El panic NO mata el proceso — se captura en join()
    match handle.join() {
        Ok(val) => println!("Exito: {:?}", val),
        Err(panic_info) => println!("Thread hizo panic: {:?}", panic_info),
    }
    
    println!("Main sigue ejecutando"); // Esto SI se ejecuta
}

// tokio: igual con JoinError
#[tokio::main]
async fn main() {
    let handle = tokio::spawn(async {
        panic!("boom");
    });
    
    match handle.await {
        Ok(val) => println!("OK: {:?}", val),
        Err(e) if e.is_panic() => println!("Task panicked"),
        Err(e) if e.is_cancelled() => println!("Task cancelled"),
        Err(e) => println!("Other error: {}", e),
    }
}
```

---

## 3. Channels: Go channels vs Rust mpsc/crossbeam/tokio

### 3.1 Creacion y uso basico

**Go: channels como primitiva del lenguaje**

```go
// Go: channels son parte del lenguaje
// No necesitan import (excepto para sync, context, etc.)

// Unbuffered (sincrono — send bloquea hasta que alguien recibe)
ch := make(chan int)

// Buffered (asincrono hasta llenar el buffer)
ch := make(chan int, 100)

// Send
ch <- 42

// Receive
val := <-ch

// Receive con ok (detectar cierre)
val, ok := <-ch

// Close
close(ch)

// Range (iterar hasta cierre)
for val := range ch {
    fmt.Println(val)
}
```

**Rust: multiples opciones, cada una con trade-offs**

```rust
// Rust std::sync::mpsc — Multi-Producer, Single-Consumer
// Parte de la biblioteca estandar, NO necesita crate externo
use std::sync::mpsc;

fn main() {
    // Unbuffered NO existe en mpsc — siempre tiene buffer "infinito"
    // (bounded NO esta en std, necesitas crossbeam o tokio)
    let (tx, rx) = mpsc::channel(); // "infinito" buffer (async send)
    
    // Bounded (con limite, send bloquea si lleno)
    let (tx, rx) = mpsc::sync_channel(100); // Buffer de 100
    
    // mpsc::sync_channel(0) = rendez-vous (como Go unbuffered)
    let (tx, rx) = mpsc::sync_channel(0);
    
    // Send
    tx.send(42).unwrap(); // Result — falla si el receiver murio
    
    // Receive
    let val = rx.recv().unwrap(); // Result — falla si todos los senders murieron
    
    // Receive con timeout
    let val = rx.recv_timeout(Duration::from_secs(1));
    
    // Try receive (no bloqueante)
    match rx.try_recv() {
        Ok(val) => println!("got {}", val),
        Err(mpsc::TryRecvError::Empty) => println!("nothing yet"),
        Err(mpsc::TryRecvError::Disconnected) => println!("channel closed"),
    }
    
    // Iterar (como range en Go)
    for val in rx {
        println!("{}", val);
    }
    // Drop del sender cierra implicitamente (no hay close() explicito)
}
```

```rust
// crossbeam-channel — la alternativa mas popular
// Necesita: crossbeam-channel = "0.5"
use crossbeam_channel::{bounded, unbounded, select};

fn main() {
    let (tx, rx) = bounded(100);    // Buffered
    let (tx, rx) = bounded(0);      // Rendez-vous (como Go unbuffered)
    let (tx, rx) = unbounded();     // Sin limite
    
    // crossbeam tiene select! (como Go select)
    // std::sync::mpsc NO tiene select
}
```

```rust
// tokio::sync::mpsc — para codigo async
// Viene con tokio = { features = ["sync"] }
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel(100); // Siempre bounded en tokio
    
    // Tambien hay tokio::sync::broadcast (multi-consumer)
    // Y tokio::sync::watch (single-value, multi-consumer)
    // Y tokio::sync::oneshot (un solo valor, una vez)
    
    tokio::spawn(async move {
        tx.send(42).await.unwrap();
    });
    
    while let Some(val) = rx.recv().await {
        println!("{}", val);
    }
}
```

### 3.2 Comparacion detallada de channels

```
┌────────────────────┬──────────────────┬───────────────────┬──────────────────┬──────────────────┐
│ Propiedad           │ Go chan T         │ Rust std::mpsc    │ crossbeam        │ tokio::mpsc      │
├────────────────────┼──────────────────┼───────────────────┼──────────────────┼──────────────────┤
│ Bounded             │ make(chan T, N)   │ sync_channel(N)   │ bounded(N)       │ channel(N)       │
│ Unbounded           │ No (siempre N)   │ channel()         │ unbounded()      │ unbounded_ch()   │
│ Rendez-vous         │ make(chan T)      │ sync_channel(0)   │ bounded(0)       │ No               │
│ Multi-producer      │ Si               │ Si (clone tx)     │ Si               │ Si               │
│ Multi-consumer      │ Si               │ NO (single-cons)  │ Si               │ NO               │
│ Select              │ select {}        │ NO                │ select! {}       │ tokio::select!   │
│ Direction types     │ chan<-T, <-chan T │ Sender/Receiver   │ Sender/Receiver  │ Sender/Receiver  │
│ Cerrar              │ close(ch)        │ Drop(sender)      │ Drop(sender)     │ Drop(sender)     │
│ Detec. cierre recv  │ val, ok := <-ch  │ Err(RecvError)    │ Err(RecvError)   │ None de recv()   │
│ Detec. cierre send  │ Panic!           │ Err(SendError)    │ Err(SendError)   │ Err(SendError)   │
│ Iterate             │ for range ch     │ for val in rx     │ for val in rx    │ while let Some() │
│ Nil channel         │ Bloquea siempre  │ N/A (no existe)   │ N/A              │ N/A              │
│ Tipo primitiva      │ Lenguaje         │ Std library       │ Crate externo    │ Crate externo    │
│ Send bloqueante     │ ch <- val        │ tx.send(val)      │ tx.send(val)     │ tx.send(val).aw  │
│ Send no-bloqueante  │ select + default │ N/A               │ tx.try_send(val) │ tx.try_send(val) │
│ Recv con timeout    │ select + time    │ recv_timeout()    │ recv_timeout()   │ tokio::time::to  │
└────────────────────┴──────────────────┴───────────────────┴──────────────────┴──────────────────┘
```

### 3.3 Send en channel cerrado: panic vs Result

Una de las diferencias mas criticas: en Go, enviar a un channel cerrado es un **panic irrecuperable** (a menos que uses `recover`). En Rust, es un `Result::Err` que puedes manejar normalmente.

```go
// Go: PANIC si envias a channel cerrado
func main() {
    ch := make(chan int)
    close(ch)
    
    // Esto causa panic: "send on closed channel"
    // No hay forma de verificar si esta cerrado antes de enviar
    ch <- 42 // PANIC
}

// Patron defensivo en Go: usar select con done channel
func safeSend(ch chan<- int, done <-chan struct{}, val int) bool {
    select {
    case ch <- val:
        return true
    case <-done:
        return false
    }
}
```

```rust
// Rust: Result::Err si envias a channel "cerrado" (receiver dropped)
use std::sync::mpsc;

fn main() {
    let (tx, rx) = mpsc::channel();
    drop(rx); // "Cerrar" el canal del lado receptor
    
    // Esto NO hace panic — retorna Err
    match tx.send(42) {
        Ok(()) => println!("enviado"),
        Err(mpsc::SendError(val)) => {
            println!("channel cerrado, valor {} no entregado", val);
            // Puedes recuperar el valor que no se envio!
        }
    }
}
```

### 3.4 Multi-consumer: Go lo tiene, Rust std no

```go
// Go: multiples goroutines pueden recibir del mismo channel
// El runtime garantiza que cada valor lo recibe exactamente UNA goroutine
func main() {
    jobs := make(chan int, 100)
    
    // 5 workers, todos recibiendo del mismo channel
    var wg sync.WaitGroup
    for w := 0; w < 5; w++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for job := range jobs { // Cada job va a exactamente 1 worker
                fmt.Printf("worker %d procesando job %d\n", id, job)
            }
        }(w)
    }
    
    for j := 0; j < 50; j++ {
        jobs <- j
    }
    close(jobs)
    wg.Wait()
}
```

```rust
// Rust std::mpsc: el Receiver NO implementa Clone
// No puedes compartir un receiver entre threads sin wrapping

// Opcion 1: Arc<Mutex<Receiver>> (funciona pero feo)
use std::sync::{mpsc, Arc, Mutex};
use std::thread;

fn main() {
    let (tx, rx) = mpsc::channel();
    let rx = Arc::new(Mutex::new(rx));
    
    let mut handles = Vec::new();
    for id in 0..5 {
        let rx = Arc::clone(&rx);
        handles.push(thread::spawn(move || {
            loop {
                let msg = rx.lock().unwrap().recv();
                match msg {
                    Ok(job) => println!("worker {} procesando job {}", id, job),
                    Err(_) => break, // Channel cerrado
                }
            }
        }));
    }
    
    for j in 0..50 {
        tx.send(j).unwrap();
    }
    drop(tx);
    for h in handles { h.join().unwrap(); }
}

// Opcion 2: crossbeam (el Receiver SI es Clone)
use crossbeam_channel::bounded;

fn main() {
    let (tx, rx) = bounded(100);
    
    let mut handles = Vec::new();
    for id in 0..5 {
        let rx = rx.clone(); // Crossbeam permite clonar el receiver!
        handles.push(std::thread::spawn(move || {
            for job in rx { // Limpio, como Go
                println!("worker {} procesando job {}", id, job);
            }
        }));
    }
    
    for j in 0..50 {
        tx.send(j).unwrap();
    }
    drop(tx);
    for h in handles { h.join().unwrap(); }
}
```

### 3.5 Select: multiplexacion de channels

```go
// Go: select es una keyword del lenguaje
func main() {
    ch1 := make(chan string)
    ch2 := make(chan string)
    done := make(chan struct{})
    
    go func() { ch1 <- "uno" }()
    go func() { ch2 <- "dos" }()
    go func() { time.Sleep(2 * time.Second); close(done) }()
    
    for {
        select {
        case msg := <-ch1:
            fmt.Println("ch1:", msg)
        case msg := <-ch2:
            fmt.Println("ch2:", msg)
        case <-done:
            fmt.Println("terminando")
            return
        }
    }
}
```

```rust
// Rust std::mpsc: NO TIENE select
// Necesitas crossbeam o tokio

// crossbeam: select! macro
use crossbeam_channel::{bounded, select};
use std::thread;
use std::time::Duration;

fn main() {
    let (tx1, rx1) = bounded(1);
    let (tx2, rx2) = bounded(1);
    let (done_tx, done_rx) = bounded(0);
    
    thread::spawn(move || { tx1.send("uno").unwrap(); });
    thread::spawn(move || { tx2.send("dos").unwrap(); });
    thread::spawn(move || {
        thread::sleep(Duration::from_secs(2));
        drop(done_tx);
    });
    
    loop {
        select! {
            recv(rx1) -> msg => println!("rx1: {:?}", msg),
            recv(rx2) -> msg => println!("rx2: {:?}", msg),
            recv(done_rx) -> _ => {
                println!("terminando");
                return;
            }
        }
    }
}

// tokio: tokio::select! macro (para codigo async)
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx1, mut rx1) = mpsc::channel(1);
    let (tx2, mut rx2) = mpsc::channel(1);
    
    tokio::spawn(async move { tx1.send("uno").await.unwrap(); });
    tokio::spawn(async move { tx2.send("dos").await.unwrap(); });
    
    tokio::select! {
        msg = rx1.recv() => println!("rx1: {:?}", msg),
        msg = rx2.recv() => println!("rx2: {:?}", msg),
    }
}
```

### 3.6 Tipos de channels en Rust que Go no tiene

```rust
// tokio::sync::oneshot — un solo valor, una sola vez
// Equivalente en Go: make(chan T, 1) y disciplina de "enviar una vez"
use tokio::sync::oneshot;

#[tokio::main]
async fn main() {
    let (tx, rx) = oneshot::channel();
    
    tokio::spawn(async move {
        let result = expensive_computation().await;
        tx.send(result).unwrap(); // Solo puedes enviar UNA vez (tx se consume)
    });
    
    let result = rx.await.unwrap();
    println!("Resultado: {}", result);
}

// tokio::sync::broadcast — multi-producer, multi-consumer
// En Go necesitarias un patron manual o multiples channels
use tokio::sync::broadcast;

#[tokio::main]
async fn main() {
    let (tx, _) = broadcast::channel(100);
    
    let mut rx1 = tx.subscribe();
    let mut rx2 = tx.subscribe();
    
    tx.send("evento").unwrap();
    
    // AMBOS receivers obtienen el mismo mensaje
    println!("rx1: {}", rx1.recv().await.unwrap());
    println!("rx2: {}", rx2.recv().await.unwrap());
}

// tokio::sync::watch — single-producer, multi-consumer, solo ultimo valor
// Ideal para configuracion, estado que cambia
use tokio::sync::watch;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = watch::channel("config_v1");
    
    let mut rx2 = rx.clone();
    
    tx.send("config_v2").unwrap(); // Sobreescribe el valor anterior
    
    // changed() espera hasta que haya un nuevo valor
    rx.changed().await.unwrap();
    println!("rx: {}", *rx.borrow());
    
    rx2.changed().await.unwrap();
    println!("rx2: {}", *rx2.borrow());
}
```

```
EQUIVALENCIAS DE CHANNEL TYPES

Go                                      Rust
─────────────────────────────────────────────────────────────────
chan T (unbuffered)                      mpsc::sync_channel(0)
                                        crossbeam::bounded(0)

chan T (buffered N)                      mpsc::sync_channel(N)
                                        crossbeam::bounded(N)
                                        tokio::mpsc::channel(N)

chan T (multi-consumer)                  crossbeam::bounded(N) + clone Rx
                                        (std::mpsc NO puede)

No existe                               tokio::oneshot (single-value)
                                        tokio::broadcast (multi-consumer copy)
                                        tokio::watch (latest-value)

close(ch)                               drop(tx) (implicito)

ch <- val en cerrado → PANIC            tx.send(val) → Err(SendError)

nil channel (bloquea siempre)           No existe en Rust
```

---

## 4. Mutex: sync.Mutex vs std::sync::Mutex\<T\>

### 4.1 Diferencia fundamental: que protege un mutex?

Esta es probablemente la diferencia mas importante entre Go y Rust en concurrencia:

- **Go**: un mutex protege una **seccion de codigo**. El programador debe recordar que datos estan protegidos — el compilador no lo verifica.
- **Rust**: un mutex protege un **valor**. Los datos estan _dentro_ del mutex. No puedes acceder a ellos sin tomar el lock — el compilador lo impone.

```go
// Go: el mutex y los datos estan SEPARADOS
type Counter struct {
    mu    sync.Mutex
    count int  // El programador SABE que mu protege count
                // Pero el compilador no lo sabe
}

func (c *Counter) Increment() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.count++ // Correcto: bajo lock
}

func (c *Counter) GetUnsafe() int {
    // BUG: olvidamos tomar el lock!
    // El compilador NO avisa. Solo -race flag lo detecta en runtime.
    return c.count
}

func (c *Counter) Get() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.count // Correcto
}
```

```rust
// Rust: los datos estan DENTRO del mutex
use std::sync::Mutex;

struct Counter {
    count: Mutex<i32>, // El i32 esta ADENTRO del Mutex
}

impl Counter {
    fn new() -> Self {
        Counter { count: Mutex::new(0) }
    }
    
    fn increment(&self) {
        let mut guard = self.count.lock().unwrap();
        *guard += 1;
        // guard se dropea al final del scope → unlock automatico
    }
    
    fn get(&self) -> i32 {
        *self.count.lock().unwrap()
    }
    
    // Esto NO COMPILA — no hay forma de acceder al i32 sin lock:
    // fn get_unsafe(&self) -> i32 {
    //     self.count.???  // No hay metodo para acceder sin lock
    // }
}
```

```
MUTEX: GO vs RUST — modelo mental

Go:
  ┌──────────────────────────┐
  │     struct               │
  │  ┌───────────┐           │
  │  │  mu Mutex │ ← Lock() │    El programador SABE que mu
  │  └───────────┘           │    protege count, pero nada
  │  ┌───────────┐           │    impide acceder a count
  │  │  count int│ ← ???     │    sin tomar mu primero.
  │  └───────────┘           │
  └──────────────────────────┘

Rust:
  ┌──────────────────────────────────┐
  │     struct                       │
  │  ┌──────────────────────────┐    │
  │  │  count: Mutex<i32>       │    │   El i32 esta DENTRO del
  │  │  ┌────────────────────┐  │    │   Mutex. No hay forma de
  │  │  │  data: i32         │  │    │   llegar al i32 sin pasar
  │  │  │  (solo via .lock())│  │    │   por .lock() primero.
  │  │  └────────────────────┘  │    │
  │  └──────────────────────────┘    │
  └──────────────────────────────────┘
```

### 4.2 MutexGuard: RAII vs defer

```go
// Go: defer Unlock() es un PATRON — no es obligatorio
func (c *Counter) Increment() {
    c.mu.Lock()
    defer c.mu.Unlock() // Buena practica, pero puedes olvidarlo
    c.count++
}

// Esto compila y es un BUG:
func (c *Counter) IncrementBuggy() {
    c.mu.Lock()
    c.count++
    // Olvidamos Unlock()! → Deadlock eventual
    // El compilador no avisa.
}
```

```rust
// Rust: MutexGuard implementa Drop → unlock automatico SIEMPRE
use std::sync::Mutex;

fn main() {
    let data = Mutex::new(42);
    
    {
        let mut guard = data.lock().unwrap();
        *guard += 1;
    } // guard se dropea aqui → unlock automatico
    // No hay forma de "olvidar" hacer unlock
    
    // Para unlock explicito antes del final del scope:
    {
        let mut guard = data.lock().unwrap();
        *guard += 1;
        drop(guard); // Unlock explicito
        
        // Aqui el lock ya no se tiene
        // Intentar usar guard despues: error de compilacion (moved value)
    }
}
```

### 4.3 Poisoning: que pasa cuando un thread con lock hace panic?

Go y Rust manejan esto de forma completamente diferente:

```go
// Go: si una goroutine hace panic mientras tiene un lock...
// El Unlock en defer funciona, pero el estado puede ser inconsistente

type SafeMap struct {
    mu   sync.Mutex
    data map[string]int
}

func (m *SafeMap) Update(key string, val int) {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    // Si esto hace panic (ej: nil map), defer Unlock() SI se ejecuta.
    // El lock se libera. Pero el estado del map puede ser inconsistente.
    // La siguiente goroutine que tome el lock vera datos corruptos.
    // Go NO tiene mecanismo para detectar esto.
    m.data[key] = val // Si data es nil → panic pero unlock ocurre
}
```

```rust
// Rust: Mutex POISONING — el mutex sabe que hubo un panic
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let data = Arc::new(Mutex::new(vec![1, 2, 3]));
    
    let data_clone = Arc::clone(&data);
    let handle = thread::spawn(move || {
        let mut guard = data_clone.lock().unwrap();
        guard.push(4);
        panic!("oops"); // El MutexGuard se dropea durante el panic
        // El mutex queda POISONED
    });
    
    let _ = handle.join(); // El panic se captura
    
    // Intentar tomar el lock: Err(PoisonError)
    match data.lock() {
        Ok(guard) => println!("datos: {:?}", *guard),
        Err(poisoned) => {
            // Puedes decidir: abortar o intentar recuperar
            println!("mutex envenenado!");
            
            // Recuperar los datos de todas formas:
            let guard = poisoned.into_inner();
            println!("datos (posiblemente inconsistentes): {:?}", *guard);
        }
    }
}
```

```
MUTEX POISONING: por que importa

Go:
  Thread 1: Lock() → modifica parcialmente → panic → defer Unlock()
  Thread 2: Lock() → ve datos a medias → comportamiento indefinido
  
  No hay advertencia. El programador debe disenar para que
  las operaciones bajo lock sean atomicas "a mano".

Rust:
  Thread 1: lock() → modifica parcialmente → panic → MutexGuard::drop()
  Thread 2: lock() → Err(PoisonError) → puede elegir:
    ├─ Abortar: "los datos pueden estar corruptos"
    └─ Recuperar: into_inner() para acceder de todas formas
  
  El mutex RECUERDA que hubo un problema.
```

### 4.4 RWMutex / RwLock

```go
// Go: sync.RWMutex
type Cache struct {
    mu   sync.RWMutex
    data map[string]string
}

func (c *Cache) Get(key string) (string, bool) {
    c.mu.RLock()         // Multiples lectores simultaneos
    defer c.mu.RUnlock()
    val, ok := c.data[key]
    return val, ok
}

func (c *Cache) Set(key, val string) {
    c.mu.Lock()          // Exclusivo para escritura
    defer c.mu.Unlock()
    c.data[key] = val
}
```

```rust
// Rust: std::sync::RwLock<T>
use std::sync::RwLock;

struct Cache {
    data: RwLock<std::collections::HashMap<String, String>>,
}

impl Cache {
    fn new() -> Self {
        Cache { data: RwLock::new(std::collections::HashMap::new()) }
    }
    
    fn get(&self, key: &str) -> Option<String> {
        let guard = self.data.read().unwrap(); // RwLockReadGuard
        guard.get(key).cloned()
    }
    
    fn set(&self, key: String, val: String) {
        let mut guard = self.data.write().unwrap(); // RwLockWriteGuard
        guard.insert(key, val);
    }
}
```

```
┌─────────────────────┬──────────────────────┬──────────────────────┐
│ Propiedad            │ Go sync.RWMutex      │ Rust RwLock<T>       │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Protege              │ Seccion de codigo    │ Valor T              │
│ Read guard           │ Manual (RLock/RUnlo) │ RwLockReadGuard<T>   │
│ Write guard          │ Manual (Lock/Unlock) │ RwLockWriteGuard<T>  │
│ Writer priority      │ Si                   │ Depende del OS       │
│ Poisoning            │ No                   │ Si                   │
│ TryLock              │ TryLock/TryRLock     │ try_read/try_write   │
│ Downgrade W→R        │ No                   │ No (tokio si)        │
│ Upgrade R→W          │ No (deadlock!)       │ No                   │
└─────────────────────┴──────────────────────┴──────────────────────┘
```

### 4.5 Compartir mutex entre threads/goroutines

```go
// Go: compartir un mutex es trivial — punteros + GC
type Service struct {
    mu    sync.Mutex
    state string
}

func main() {
    svc := &Service{state: "init"}
    
    var wg sync.WaitGroup
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            svc.mu.Lock()         // svc es un puntero — Go se encarga
            svc.state = "updated"
            svc.mu.Unlock()
        }()
    }
    wg.Wait()
}
```

```rust
// Rust: necesitas Arc (Atomic Reference Counting) para compartir
// entre threads. El compilador OBLIGA a esto.
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let state = Arc::new(Mutex::new(String::from("init")));
    
    let mut handles = Vec::new();
    for _ in 0..10 {
        let state = Arc::clone(&state); // Incrementa ref count
        handles.push(thread::spawn(move || {
            let mut guard = state.lock().unwrap();
            *guard = String::from("updated");
        }));
    }
    
    for h in handles { h.join().unwrap(); }
    println!("Final: {}", state.lock().unwrap());
}

// Por que Arc y no Rc?
// Rc: Reference Counting, NOT thread-safe (no implementa Send)
// Arc: Atomic Reference Counting, thread-safe
// El compilador no te deja usar Rc entre threads:
// use std::rc::Rc;
// let state = Rc::new(Mutex::new(...));
// thread::spawn(move || state.lock()...) // ERROR: Rc<...> cannot be sent between threads
```

---

## 5. Send y Sync: el sistema de tipos como guardián de concurrencia

Rust tiene dos traits clave que Go no tiene equivalente:

- **`Send`**: un tipo `T: Send` puede ser **transferido** a otro thread.
- **`Sync`**: un tipo `T: Sync` puede ser **referenciado** desde multiples threads simultaneamente.

Estos traits son **auto-implementados** por el compilador. No necesitas hacer nada — el compilador analiza la composicion de tu tipo y decide si es Send/Sync.

```rust
// Estos tipos son Send + Sync:
// i32, String, Vec<T> (si T: Send), Arc<T> (si T: Send + Sync)

// Estos NO son Send:
// Rc<T>: el reference count no es atomico → no seguro entre threads
// *mut T: raw pointers no son Send

// Estos NO son Sync:
// Cell<T>, RefCell<T>: interior mutability sin atomics → no seguro compartir
// Rc<T>: idem

use std::sync::{Arc, Mutex};
use std::thread;
use std::cell::RefCell;

fn main() {
    // Esto COMPILA — Arc<Mutex<i32>> es Send + Sync
    let data = Arc::new(Mutex::new(42));
    let data2 = Arc::clone(&data);
    thread::spawn(move || {
        *data2.lock().unwrap() += 1;
    }).join().unwrap();
    
    // Esto NO COMPILA — RefCell<i32> no es Sync
    // let data = Arc::new(RefCell::new(42));
    // let data2 = Arc::clone(&data);
    // thread::spawn(move || {
    //     *data2.borrow_mut() += 1; // ERROR: RefCell<i32> cannot be shared between threads
    // });
}
```

```
SEND + SYNC: por que Go no necesita esto

Go tiene garbage collector. Cualquier goroutine puede tener un puntero a cualquier
dato y el GC se asegura de que la memoria no se libere prematuramente. No hay 
use-after-free, no hay double-free.

PERO: no hay proteccion contra data races a nivel de tipos. Necesitas:
  - sync.Mutex/RWMutex (convencion del programador)
  - Race detector (-race flag, solo en testing)
  - Code review

Rust NO tiene GC. El borrow checker + Send/Sync + ownership garantizan:
  - Si compila, no hay data races
  - Si compila, no hay use-after-free
  - Si compila, no hay double-free

Trade-off:
  Go: rapido de escribir, posibles bugs sutiles en produccion
  Rust: mas lento de escribir, bugs eliminados en compilacion
```

```
TABLA: Send/Sync de tipos comunes

Tipo                    Send?    Sync?    Notas
─────────────────────────────────────────────────────────────────
i32, f64, bool          Si       Si       Tipos primitivos
String, Vec<T>          Si*      Si*      *Si T es Send/Sync
Arc<T>                  Si*      Si*      *Si T es Send+Sync
Mutex<T>                Si*      Si*      *Si T es Send
RwLock<T>               Si*      Si*      *Si T es Send+Sync
Rc<T>                   NO       NO       Ref count no atomico
Cell<T>                 Si       NO       Interior mutability sin lock
RefCell<T>              Si       NO       Interior mutability sin lock
*mut T                  NO       NO       Raw pointer
mpsc::Sender<T>         Si*      NO*      *Si T es Send
mpsc::Receiver<T>       Si*      NO       *Si T es Send
```

---

## 6. Share-Memory vs Message-Passing: filosofias comparadas

### 6.1 Go: "share memory by communicating"

Go favorece channels (message-passing) pero ofrece ambos modelos. La decision es del programador:

```go
// Modelo 1: Message-passing (preferido por la filosofia de Go)
func sumWithChannels(nums []int) int {
    ch := make(chan int)
    
    mid := len(nums) / 2
    go func() { ch <- sum(nums[:mid]) }()
    go func() { ch <- sum(nums[mid:]) }()
    
    return <-ch + <-ch
}

// Modelo 2: Shared memory (cuando es mas simple)
func sumWithMutex(nums []int) int {
    var mu sync.Mutex
    total := 0
    var wg sync.WaitGroup
    
    mid := len(nums) / 2
    for _, chunk := range [][]int{nums[:mid], nums[mid:]} {
        wg.Add(1)
        go func(c []int) {
            defer wg.Done()
            s := sum(c)
            mu.Lock()
            total += s
            mu.Unlock()
        }(chunk)
    }
    wg.Wait()
    return total
}
```

### 6.2 Rust: ownership como modelo de concurrencia

Rust va mas alla de ambos modelos — el **sistema de ownership** es el fundamento. Tanto channels como mutexes son implementaciones que se construyen sobre ownership + Send/Sync:

```rust
// Rust ownership: mover datos a otro thread = transferir propiedad
use std::thread;

fn main() {
    let data = vec![1, 2, 3, 4, 5];
    
    // `move` transfiere ownership de `data` al nuevo thread
    let handle = thread::spawn(move || {
        let sum: i32 = data.iter().sum();
        sum
        // `data` se dropea cuando el thread termina
    });
    
    // data ya no es accesible aqui — el compilador lo impide:
    // println!("{:?}", data); // ERROR: borrow of moved value
    
    let result = handle.join().unwrap();
    println!("Sum: {}", result);
}

// Esto es fundamentalmente diferente a Go:
// En Go, pasar datos a una goroutine es pasar un puntero.
// Ambas goroutines pueden acceder a los datos simultaneamente.
// En Rust, pasar datos a un thread MUEVE los datos — solo el
// nuevo thread puede acceder a ellos.
```

### 6.3 Patron de transferencia de ownership

```rust
// Rust: pipeline con transferencia de ownership
// Cada stage POSEE los datos que procesa — no hay data races posibles
use std::thread;
use std::sync::mpsc;

fn main() {
    let (tx1, rx1) = mpsc::channel();
    let (tx2, rx2) = mpsc::channel();
    let (tx3, rx3) = mpsc::channel();
    
    // Stage 1: generar datos
    thread::spawn(move || {
        for i in 0..10 {
            tx1.send(vec![i; 100]).unwrap(); // Vec se MUEVE al channel
            // No podemos usar el Vec aqui — ya no nos pertenece
        }
    });
    
    // Stage 2: transformar (ownership del Vec pasa de rx1 a tx2)
    thread::spawn(move || {
        for mut data in rx1 { // data: Vec<i32>, ownership transferida
            data.iter_mut().for_each(|x| *x *= 2);
            tx2.send(data).unwrap(); // Ownership transferida otra vez
        }
    });
    
    // Stage 3: agregar
    thread::spawn(move || {
        for data in rx2 {
            let sum: i32 = data.iter().sum();
            tx3.send(sum).unwrap();
        }
    });
    
    // Consumir resultados
    for result in rx3 {
        println!("Resultado: {}", result);
    }
}
```

```go
// Go equivalente: los datos se COMPARTEN via punteros internos
// No hay garantia de que alguien no los modifique despues de enviar
func pipeline() {
    ch1 := make(chan []int)
    ch2 := make(chan []int)
    ch3 := make(chan int)
    
    // Stage 1
    go func() {
        for i := 0; i < 10; i++ {
            data := make([]int, 100)
            for j := range data { data[j] = i }
            ch1 <- data
            // CUIDADO: data es un slice (header con puntero al array)
            // Si modificamos data[0] aqui, el receptor VE el cambio
            // Go NO transfiere ownership — ambos pueden acceder
        }
        close(ch1)
    }()
    
    // Stage 2
    go func() {
        for data := range ch1 {
            for i := range data { data[i] *= 2 }
            ch2 <- data
        }
        close(ch2)
    }()
    
    // Stage 3
    go func() {
        for data := range ch2 {
            sum := 0
            for _, v := range data { sum += v }
            ch3 <- sum
        }
        close(ch3)
    }()
    
    for result := range ch3 {
        fmt.Println("Resultado:", result)
    }
}
```

### 6.4 Tabla de decision: cuando usar que modelo

```
┌───────────────────────────────┬─────────────────────────────┬─────────────────────────────┐
│ Situacion                      │ Go                          │ Rust                        │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Transferir datos entre tasks  │ Channel (el dato se copia   │ Channel (el dato se MUEVE   │
│                                │ o comparte via puntero)     │ — ownership transferido)    │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Multiples lectores, un writer │ RWMutex                     │ RwLock<T> o Arc<RwLock<T>>  │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Contador atomico              │ sync/atomic (AddInt64, etc) │ std::sync::atomic::AtomicI64│
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Inicializar una vez           │ sync.Once                   │ std::sync::OnceLock         │
│                                │                             │ (antes: once_cell crate)    │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Map concurrente               │ sync.Map                    │ dashmap crate               │
│                                │                             │ (std no tiene equivalente)  │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Pool de objetos               │ sync.Pool                   │ No hay equivalente directo  │
│                                │                             │ (object-pool crate)         │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Cancelacion                   │ context.Context             │ CancellationToken (tokio_   │
│                                │                             │ util) o channel manual      │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Paralelismo de datos          │ Manual (goroutines + split) │ rayon::par_iter()           │
├───────────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Error group                   │ golang.org/x/sync/errgroup  │ tokio::JoinSet,             │
│                                │                             │ futures::join_all           │
└───────────────────────────────┴─────────────────────────────┴─────────────────────────────┘
```

---

## 7. Atomics: sync/atomic vs std::sync::atomic

Ambos lenguajes ofrecen operaciones atomicas de bajo nivel, pero con diferencias en ergonomia y seguridad:

```go
// Go: sync/atomic opera sobre *int64, *uint64, *int32, etc.
import "sync/atomic"

var counter int64

func increment() {
    atomic.AddInt64(&counter, 1)
}

func get() int64 {
    return atomic.LoadInt64(&counter)
}

// Go 1.19+: atomic.Int64 (wrapper con metodos)
var counter2 atomic.Int64

func increment2() {
    counter2.Add(1)
}

func get2() int64 {
    return counter2.Load()
}

// atomic.Pointer[T] (Go 1.19+)
var config atomic.Pointer[Config]

func updateConfig(c *Config) {
    config.Store(c)
}

func getConfig() *Config {
    return config.Load()
}
```

```rust
// Rust: std::sync::atomic opera sobre AtomicI64, AtomicBool, etc.
use std::sync::atomic::{AtomicI64, Ordering};

static COUNTER: AtomicI64 = AtomicI64::new(0);

fn increment() {
    COUNTER.fetch_add(1, Ordering::SeqCst);
    //                    ^^^^^^^^^^^^^^^^
    // Rust EXIGE que especifiques el memory ordering.
    // Go usa SeqCst por defecto (lo mas conservador).
}

fn get() i64 {
    COUNTER.load(Ordering::SeqCst)
}
```

### 7.1 Memory Ordering: la diferencia critica

```
MEMORY ORDERING

Go:
  - Todas las operaciones atomicas usan sequential consistency (SeqCst)
  - No puedes elegir un ordering mas relajado
  - Mas simple pero potencialmente mas lento en arquitecturas con
    modelos de memoria debiles (ARM, etc.)

Rust:
  - DEBES especificar el ordering en cada operacion:
    ├─ Ordering::Relaxed    → solo atomicidad, sin ordering
    ├─ Ordering::Acquire    → lectura con acquire semantics
    ├─ Ordering::Release    → escritura con release semantics
    ├─ Ordering::AcqRel     → ambos (para read-modify-write)
    └─ Ordering::SeqCst     → sequential consistency (como Go)
  
  - Mas control → mas rendimiento potencial
  - Pero tambien mas facil de equivocarse
  - Recomendacion: usar SeqCst a menos que profiling muestre
    que atomics son un cuello de botella
```

```rust
// Ejemplo: patron release-acquire en Rust
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};
use std::thread;

static DATA: AtomicI32 = AtomicI32::new(0);
static READY: AtomicBool = AtomicBool::new(false);

fn main() {
    // Producer
    let producer = thread::spawn(|| {
        DATA.store(42, Ordering::Relaxed);        // Escribir dato
        READY.store(true, Ordering::Release);      // "Publicar" que esta listo
        //                      ^^^^^^^
        // Release: todo lo que escribi ANTES de este store
        // sera visible para quien lea este store con Acquire
    });
    
    // Consumer
    let consumer = thread::spawn(|| {
        while !READY.load(Ordering::Acquire) {    // Esperar publicacion
            //                     ^^^^^^^
            // Acquire: todo lo que el producer escribio antes de su
            // Release store sera visible DESPUES de este load
            std::hint::spin_loop();
        }
        assert_eq!(DATA.load(Ordering::Relaxed), 42); // Garantizado!
    });
    
    producer.join().unwrap();
    consumer.join().unwrap();
}
```

```
┌─────────────────────┬──────────────────────────┬─────────────────────────────┐
│ Operacion            │ Go sync/atomic            │ Rust std::sync::atomic      │
├─────────────────────┼──────────────────────────┼─────────────────────────────┤
│ Load                 │ atomic.LoadInt64(&v)     │ v.load(Ordering)            │
│ Store                │ atomic.StoreInt64(&v, n) │ v.store(n, Ordering)        │
│ Add                  │ atomic.AddInt64(&v, n)   │ v.fetch_add(n, Ordering)    │
│ CAS                  │ atomic.CompareAndSwapI64 │ v.compare_exchange(old, new,│
│                      │  (&v, old, new)          │   success_ord, fail_ord)    │
│ Swap                 │ atomic.SwapInt64(&v, n)  │ v.swap(n, Ordering)         │
│ Pointer              │ atomic.Pointer[T]        │ AtomicPtr<T>                │
│ Memory ordering      │ SeqCst (fijo)            │ Configurable por operacion  │
│ Bool                 │ atomic.Bool (Go 1.19+)   │ AtomicBool                  │
│ Weak CAS             │ No                       │ compare_exchange_weak       │
└─────────────────────┴──────────────────────────┴─────────────────────────────┘
```

---

## 8. Cancelacion: context.Context vs CancellationToken/tokio::select!

### 8.1 Propagacion de cancelacion

```go
// Go: context.Context se pasa como primer parametro
func processRequest(ctx context.Context, req Request) error {
    // Crear sub-contexto con timeout
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()
    
    // Pasar a sub-operaciones
    data, err := fetchFromDB(ctx, req.ID)
    if err != nil {
        return err
    }
    
    return sendResponse(ctx, data)
}

func fetchFromDB(ctx context.Context, id string) (Data, error) {
    // Verificar cancelacion antes de operacion costosa
    select {
    case <-ctx.Done():
        return Data{}, ctx.Err()
    default:
    }
    
    // Usar contexto en operaciones de base de datos
    row := db.QueryRowContext(ctx, "SELECT * FROM items WHERE id = $1", id)
    // QueryRowContext respeta el deadline del contexto
    // ...
    return Data{}, nil
}
```

```rust
// Rust tokio: varias estrategias

// Estrategia 1: CancellationToken (tokio_util)
use tokio_util::sync::CancellationToken;
use tokio::time::{timeout, Duration};

async fn process_request(token: CancellationToken, req: Request) -> Result<(), Error> {
    // Sub-token (como sub-context en Go)
    let child_token = token.child_token();
    
    // Timeout + cancelacion
    let result = tokio::select! {
        result = timeout(Duration::from_secs(5), fetch_from_db(&child_token, &req.id)) => {
            result.map_err(|_| Error::Timeout)?
        }
        _ = token.cancelled() => {
            return Err(Error::Cancelled);
        }
    };
    
    result
}

async fn fetch_from_db(token: &CancellationToken, id: &str) -> Result<Data, Error> {
    // Verificar cancelacion
    if token.is_cancelled() {
        return Err(Error::Cancelled);
    }
    
    // tokio::select! para cancelacion cooperativa
    tokio::select! {
        result = sqlx::query("SELECT * FROM items WHERE id = $1")
            .bind(id)
            .fetch_one(&pool) => {
            Ok(result?)
        }
        _ = token.cancelled() => {
            Err(Error::Cancelled)
        }
    }
}

// Estrategia 2: tokio::select! con channel (mas manual, sin dependencia extra)
async fn process_with_channel(
    mut shutdown: tokio::sync::broadcast::Receiver<()>,
    req: Request,
) -> Result<(), Error> {
    tokio::select! {
        result = do_work(&req) => result,
        _ = shutdown.recv() => Err(Error::Shutdown),
    }
}
```

### 8.2 Comparacion de modelos de cancelacion

```
┌────────────────────────┬──────────────────────────┬─────────────────────────────┐
│ Aspecto                 │ Go context.Context       │ Rust tokio                  │
├────────────────────────┼──────────────────────────┼─────────────────────────────┤
│ Primitiva               │ context.Context (interf.)│ CancellationToken o select! │
│ Propagacion             │ Parametro (primer arg)   │ Parametro o clone           │
│ Jerarquia               │ Arbol (WithCancel, etc.) │ parent/child tokens         │
│ Timeout                 │ WithTimeout/WithDeadline │ tokio::time::timeout()      │
│ Valores de request      │ WithValue (key-value)    │ No equivalente directo      │
│                          │                          │ (usar structs explicitamente)│
│ Cancelacion de I/O      │ Integrado (net, http, db)│ tokio::select! con I/O      │
│ Forzar abort             │ No (cooperativo)         │ JoinHandle::abort()         │
│ Multiples senales        │ Un Done() channel        │ select! con N branches      │
│ Leak protection          │ defer cancel()           │ Drop de CancellationToken   │
│ Estandar en el ecosist.  │ Si (desde Go 1.7)        │ De facto (tokio dominante)  │
│ Costo en firmas          │ ctx como primer param    │ Token como param o implict. │
└────────────────────────┴──────────────────────────┴─────────────────────────────┘
```

### 8.3 Context values vs typed state

Una diferencia filosofica importante: Go permite pasar valores arbitrarios a traves del context (request ID, auth tokens, etc.). Rust prefiere tipos explicitos:

```go
// Go: context values — flexible pero no type-safe
type ctxKey string

const requestIDKey ctxKey = "request_id"

func middleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        ctx := context.WithValue(r.Context(), requestIDKey, uuid.New().String())
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

func handler(w http.ResponseWriter, r *http.Request) {
    // Necesitas type assertion — no es type-safe en compile time
    reqID := r.Context().Value(requestIDKey).(string)
    log.Printf("[%s] handling request", reqID)
}
```

```rust
// Rust: tipos explicitos — verbose pero type-safe
// En frameworks web como axum:
use axum::{extract::Extension, middleware, response::IntoResponse};

#[derive(Clone)]
struct RequestId(String);

async fn add_request_id(
    mut req: axum::http::Request<axum::body::Body>,
    next: middleware::Next,
) -> impl IntoResponse {
    req.extensions_mut().insert(RequestId(uuid::Uuid::new_v4().to_string()));
    next.run(req).await
}

async fn handler(Extension(req_id): Extension<RequestId>) -> String {
    // Type-safe en compile time — si RequestId no esta en extensions,
    // axum retorna 500, no un panic en runtime
    format!("[{}] handling request", req_id.0)
}

// Alternativa sin framework: pasar como parametro explicito
async fn process(req_id: &str, data: &Data) -> Result<(), Error> {
    // No hay "contexto magico" — todo es explicito en la firma
    Ok(())
}
```

---

## 9. Error Handling en concurrencia

### 9.1 errgroup vs JoinSet

```go
// Go: errgroup (golang.org/x/sync/errgroup)
import "golang.org/x/sync/errgroup"

func fetchAll(ctx context.Context, urls []string) ([]string, error) {
    g, ctx := errgroup.WithContext(ctx)
    results := make([]string, len(urls))
    
    for i, url := range urls {
        i, url := i, url // Capture para Go < 1.22
        g.Go(func() error {
            body, err := fetch(ctx, url)
            if err != nil {
                return err // Cancela el contexto del grupo
            }
            results[i] = body
            return nil
        })
    }
    
    if err := g.Wait(); err != nil {
        return nil, err // Retorna el PRIMER error
    }
    return results, nil
}
```

```rust
// Rust: tokio::task::JoinSet
use tokio::task::JoinSet;

async fn fetch_all(urls: Vec<String>) -> Result<Vec<String>, Error> {
    let mut set = JoinSet::new();
    
    for url in urls {
        set.spawn(async move {
            fetch(&url).await
        });
    }
    
    let mut results = Vec::new();
    while let Some(result) = set.join_next().await {
        match result {
            Ok(Ok(body)) => results.push(body),
            Ok(Err(e)) => return Err(e),      // Error de la tarea
            Err(e) => return Err(e.into()),    // JoinError (panic, cancel)
        }
    }
    
    Ok(results)
}

// Alternativa: futures::try_join_all (falla rapido, cancela los demas)
use futures::future::try_join_all;

async fn fetch_all_v2(urls: Vec<String>) -> Result<Vec<String>, Error> {
    let futures: Vec<_> = urls.iter()
        .map(|url| fetch(url))
        .collect();
    
    try_join_all(futures).await
}
```

### 9.2 Panic recovery

```
PANIC EN CONCURRENCIA

Go:
  - Un panic NO recuperado en una goroutine mata el PROCESO entero
  - recover() solo funciona dentro de la goroutine que hizo panic
  - No puedes "capturar" el panic de otra goroutine
  - Patron: defer func() { if r := recover(); r != nil { ... } }()

Rust threads:
  - Un panic en un thread NO mata el proceso (a menos que sea el main)
  - join() retorna Result<T, Box<dyn Any>> — puedes inspeccionar el panic
  - std::panic::catch_unwind() captura panics como Result

Rust tokio:
  - Un panic en un task NO mata el runtime
  - JoinHandle.await retorna Result<T, JoinError>
  - JoinError distingue: is_panic() vs is_cancelled()

Preferencia en ambos lenguajes:
  Usar Result/error en vez de panic para errores esperados.
  Panic = bug irrecuperable, no flujo de control.
```

---

## 10. Patrones completos comparados

### 10.1 Worker Pool

```go
// Go: worker pool clasico
func workerPool(ctx context.Context, jobs []Job, workers int) []Result {
    jobsCh := make(chan Job)
    resultsCh := make(chan Result)
    
    // Lanzar workers
    var wg sync.WaitGroup
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for job := range jobsCh {
                select {
                case <-ctx.Done():
                    return
                default:
                    resultsCh <- process(job)
                }
            }
        }(i)
    }
    
    // Cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(resultsCh)
    }()
    
    // Enviar jobs
    go func() {
        for _, job := range jobs {
            select {
            case jobsCh <- job:
            case <-ctx.Done():
                break
            }
        }
        close(jobsCh)
    }()
    
    // Recolectar
    var results []Result
    for r := range resultsCh {
        results = append(results, r)
    }
    return results
}
```

```rust
// Rust tokio: worker pool con JoinSet y semaphore
use tokio::sync::Semaphore;
use std::sync::Arc;

async fn worker_pool(jobs: Vec<Job>, workers: usize) -> Vec<Result<Output, Error>> {
    let semaphore = Arc::new(Semaphore::new(workers));
    let mut set = tokio::task::JoinSet::new();
    
    for job in jobs {
        let permit = semaphore.clone().acquire_owned().await.unwrap();
        set.spawn(async move {
            let result = process(job).await;
            drop(permit); // Liberar slot
            result
        });
    }
    
    let mut results = Vec::new();
    while let Some(result) = set.join_next().await {
        results.push(result.unwrap());
    }
    results
}

// Alternativa con rayon (para trabajo CPU-bound sincrono):
use rayon::prelude::*;

fn worker_pool_sync(jobs: Vec<Job>) -> Vec<Result<Output, Error>> {
    jobs.into_par_iter() // Automaticamente usa N threads (N = num CPUs)
        .map(|job| process_sync(job))
        .collect()
}
```

### 10.2 Pipeline con cancelacion

```go
// Go: pipeline de 3 etapas con cancelacion via context
func pipeline(ctx context.Context, input <-chan int) <-chan string {
    doubled := stage(ctx, input, func(n int) int { return n * 2 })
    filtered := filter(ctx, doubled, func(n int) bool { return n > 10 })
    return format(ctx, filtered)
}

func stage[T, U any](ctx context.Context, in <-chan T, fn func(T) U) <-chan U {
    out := make(chan U)
    go func() {
        defer close(out)
        for v := range in {
            select {
            case out <- fn(v):
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

func filter[T any](ctx context.Context, in <-chan T, pred func(T) bool) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        for v := range in {
            if pred(v) {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    return out
}

func format(ctx context.Context, in <-chan int) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for v := range in {
            select {
            case out <- fmt.Sprintf("valor: %d", v):
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}
```

```rust
// Rust: pipeline con streams (tokio-stream o futures::Stream)
use tokio_stream::{self as stream, StreamExt};
use tokio_util::sync::CancellationToken;

async fn pipeline(
    token: CancellationToken,
    input: impl stream::Stream<Item = i32> + Send + 'static,
) -> Vec<String> {
    let token_clone = token.clone();
    
    let doubled = input.map(|n| n * 2);
    let filtered = doubled.filter(|n| *n > 10);
    let formatted = filtered.map(|n| format!("valor: {}", n));
    
    // Recolectar con cancelacion
    let mut results = Vec::new();
    tokio::pin!(formatted);
    
    loop {
        tokio::select! {
            item = formatted.next() => {
                match item {
                    Some(s) => results.push(s),
                    None => break,
                }
            }
            _ = token_clone.cancelled() => break,
        }
    }
    
    results
}

// Alternativa con channels (mas similar a Go):
use tokio::sync::mpsc;

async fn pipeline_channels(
    token: CancellationToken,
    mut input: mpsc::Receiver<i32>,
) -> mpsc::Receiver<String> {
    let (tx1, mut rx1) = mpsc::channel(32);
    let (tx2, mut rx2) = mpsc::channel(32);
    let (tx_out, rx_out) = mpsc::channel(32);
    
    let t1 = token.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                Some(n) = input.recv() => { let _ = tx1.send(n * 2).await; }
                _ = t1.cancelled() => break,
                else => break,
            }
        }
    });
    
    let t2 = token.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                Some(n) = rx1.recv() => {
                    if n > 10 { let _ = tx2.send(n).await; }
                }
                _ = t2.cancelled() => break,
                else => break,
            }
        }
    });
    
    let t3 = token.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                Some(n) = rx2.recv() => {
                    let _ = tx_out.send(format!("valor: {}", n)).await;
                }
                _ = t3.cancelled() => break,
                else => break,
            }
        }
    });
    
    rx_out
}
```

---

## 11. Resumen de trade-offs

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                 GO vs RUST: RESUMEN DE CONCURRENCIA                                │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  SEGURIDAD                                                                          │
│  ├─ Go: data races posibles, detectados con -race flag (runtime)                   │
│  └─ Rust: data races IMPOSIBLES (compilador los previene via Send/Sync/ownership) │
│                                                                                     │
│  FACILIDAD DE USO                                                                   │
│  ├─ Go: go f(), channels, select — todo integrado, curva de aprendizaje baja      │
│  └─ Rust: Arc, Mutex<T>, async/await, lifetimes — curva de aprendizaje alta       │
│                                                                                     │
│  RENDIMIENTO                                                                        │
│  ├─ Go: muy bueno para I/O bound, GC pauses ocasionales (~0.5ms Go 1.22)          │
│  └─ Rust: excelente en todo, zero-cost abstractions, sin GC                        │
│                                                                                     │
│  MODELO DE EJECUCION                                                                │
│  ├─ Go: runtime integrado M:N, no-choice (simplifica)                              │
│  └─ Rust: elige tu modelo (threads, tokio, rayon, async-std, smol...)              │
│                                                                                     │
│  CHANNELS                                                                           │
│  ├─ Go: primitiva del lenguaje, multi-producer Y multi-consumer                    │
│  └─ Rust: libreria (mpsc, crossbeam, tokio), MPSC por defecto                      │
│                                                                                     │
│  MUTEX                                                                              │
│  ├─ Go: protege CODIGO (convencion del programador)                                │
│  └─ Rust: protege DATOS (compilador lo impone, Mutex<T>)                            │
│                                                                                     │
│  CANCELACION                                                                        │
│  ├─ Go: context.Context (estandar, integrado en stdlib, valores de request)        │
│  └─ Rust: CancellationToken/select! (no estandar unico, mas flexible)             │
│                                                                                     │
│  PANIC                                                                              │
│  ├─ Go: panic en goroutine mata el proceso (sin recover)                           │
│  └─ Rust: panic en thread/task se captura en join()                                │
│                                                                                     │
│  FUNCTION COLORING                                                                  │
│  ├─ Go: NO (todas las funciones son iguales)                                       │
│  └─ Rust: SI (async fn vs fn — async contagia hacia arriba)                        │
│                                                                                     │
│  ECOSISTEMA                                                                         │
│  ├─ Go: stdlib cubre 90% de necesidades (net, http, context, sync)                │
│  └─ Rust: necesitas crates (tokio, rayon, crossbeam, dashmap, etc.)               │
│                                                                                     │
│  IDEAL PARA                                                                         │
│  ├─ Go: microservicios, APIs, CLI tools, DevOps, equipos grandes                  │
│  └─ Rust: sistemas, embedded, game engines, infra critica, WASM                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. Programa completo: Infrastructure Monitor — Go vs Rust

Este programa implementa un sistema de monitoreo de infraestructura con las mismas funcionalidades en ambos lenguajes, para comparar directamente como se expresan los mismos conceptos:

**Funcionalidades**:
- 3 colectores concurrentes de metricas (CPU, memory, disk)
- Worker pool para procesar metricas
- Agregador que combina resultados
- Cancelacion con timeout global
- Graceful shutdown con signal handling

### 12.1 Version Go

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "os"
    "os/signal"
    "sync"
    "sync/atomic"
    "time"
)

// --- Tipos ---

type MetricType int

const (
    CPU MetricType = iota
    Memory
    Disk
)

func (m MetricType) String() string {
    return [...]string{"CPU", "Memory", "Disk"}[m]
}

type Metric struct {
    Type      MetricType
    Host      string
    Value     float64
    Timestamp time.Time
}

type AggregatedResult struct {
    Type    MetricType
    Avg     float64
    Max     float64
    Min     float64
    Count   int
    Hosts   []string
}

// --- Colector de metricas ---

func collectMetrics(ctx context.Context, metricType MetricType, hosts []string, interval time.Duration) <-chan Metric {
    out := make(chan Metric)
    go func() {
        defer close(out)
        ticker := time.NewTicker(interval)
        defer ticker.Stop()

        for {
            select {
            case <-ctx.Done():
                fmt.Printf("[Collector %s] shutting down: %v\n", metricType, ctx.Err())
                return
            case <-ticker.C:
                for _, host := range hosts {
                    metric := Metric{
                        Type:      metricType,
                        Host:      host,
                        Value:     generateMetricValue(metricType),
                        Timestamp: time.Now(),
                    }
                    select {
                    case out <- metric:
                    case <-ctx.Done():
                        return
                    }
                }
            }
        }
    }()
    return out
}

func generateMetricValue(mt MetricType) float64 {
    switch mt {
    case CPU:
        return rand.Float64() * 100 // 0-100%
    case Memory:
        return 40 + rand.Float64()*50 // 40-90%
    case Disk:
        return 20 + rand.Float64()*70 // 20-90%
    default:
        return 0
    }
}

// --- Fan-in: merge multiple metric streams ---

func fanIn(ctx context.Context, channels ...<-chan Metric) <-chan Metric {
    merged := make(chan Metric)
    var wg sync.WaitGroup

    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan Metric) {
            defer wg.Done()
            for {
                select {
                case metric, ok := <-c:
                    if !ok {
                        return
                    }
                    select {
                    case merged <- metric:
                    case <-ctx.Done():
                        return
                    }
                case <-ctx.Done():
                    return
                }
            }
        }(ch)
    }

    go func() {
        wg.Wait()
        close(merged)
    }()

    return merged
}

// --- Worker pool para procesar metricas ---

type ProcessedMetric struct {
    Metric   Metric
    Alert    bool
    AlertMsg string
}

func processWorkerPool(ctx context.Context, input <-chan Metric, workers int) <-chan ProcessedMetric {
    output := make(chan ProcessedMetric)
    var wg sync.WaitGroup

    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            for {
                select {
                case metric, ok := <-input:
                    if !ok {
                        return
                    }
                    processed := processMetric(workerID, metric)
                    select {
                    case output <- processed:
                    case <-ctx.Done():
                        return
                    }
                case <-ctx.Done():
                    return
                }
            }
        }(i)
    }

    go func() {
        wg.Wait()
        close(output)
    }()

    return output
}

func processMetric(workerID int, m Metric) ProcessedMetric {
    // Simulamos procesamiento
    time.Sleep(time.Duration(rand.Intn(50)) * time.Millisecond)

    pm := ProcessedMetric{Metric: m}

    // Generar alertas por umbrales
    switch m.Type {
    case CPU:
        if m.Value > 90 {
            pm.Alert = true
            pm.AlertMsg = fmt.Sprintf("HIGH CPU on %s: %.1f%%", m.Host, m.Value)
        }
    case Memory:
        if m.Value > 85 {
            pm.Alert = true
            pm.AlertMsg = fmt.Sprintf("HIGH MEMORY on %s: %.1f%%", m.Host, m.Value)
        }
    case Disk:
        if m.Value > 80 {
            pm.Alert = true
            pm.AlertMsg = fmt.Sprintf("HIGH DISK on %s: %.1f%%", m.Host, m.Value)
        }
    }

    return pm
}

// --- Agregador ---

func aggregate(ctx context.Context, input <-chan ProcessedMetric) (map[MetricType]*AggregatedResult, []string) {
    results := map[MetricType]*AggregatedResult{
        CPU:    {Type: CPU, Min: 100},
        Memory: {Type: Memory, Min: 100},
        Disk:   {Type: Disk, Min: 100},
    }
    var alerts []string
    var mu sync.Mutex
    var metricsProcessed atomic.Int64

    for pm := range input {
        mu.Lock()
        r := results[pm.Metric.Type]
        r.Count++
        r.Avg = (r.Avg*float64(r.Count-1) + pm.Metric.Value) / float64(r.Count)
        if pm.Metric.Value > r.Max {
            r.Max = pm.Metric.Value
        }
        if pm.Metric.Value < r.Min {
            r.Min = pm.Metric.Value
        }
        // Track unique hosts
        found := false
        for _, h := range r.Hosts {
            if h == pm.Metric.Host {
                found = true
                break
            }
        }
        if !found {
            r.Hosts = append(r.Hosts, pm.Metric.Host)
        }

        if pm.Alert {
            alerts = append(alerts, pm.AlertMsg)
        }
        mu.Unlock()

        metricsProcessed.Add(1)

        if metricsProcessed.Load()%10 == 0 {
            fmt.Printf("[Monitor] Processed %d metrics so far...\n", metricsProcessed.Load())
        }
    }

    fmt.Printf("[Monitor] Total metrics processed: %d\n", metricsProcessed.Load())
    return results, alerts
}

// --- Main ---

func main() {
    // Contexto con signal handling y timeout global
    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
    defer stop()

    ctx, cancel := context.WithTimeout(ctx, 10*time.Second)
    defer cancel()

    fmt.Println("=== Infrastructure Monitor (Go) ===")
    fmt.Println("Monitoring for 10 seconds (Ctrl+C to stop early)...")
    fmt.Println()

    hosts := []string{"web-01", "web-02", "api-01", "db-01"}

    // Colectores concurrentes (fan-out)
    cpuStream := collectMetrics(ctx, CPU, hosts, 500*time.Millisecond)
    memStream := collectMetrics(ctx, Memory, hosts, 700*time.Millisecond)
    diskStream := collectMetrics(ctx, Disk, hosts, 1*time.Second)

    // Fan-in: merge all streams
    merged := fanIn(ctx, cpuStream, memStream, diskStream)

    // Worker pool: 4 workers procesan metricas
    processed := processWorkerPool(ctx, merged, 4)

    // Agregar resultados
    results, alerts := aggregate(ctx, processed)

    // Reporte final
    fmt.Println()
    fmt.Println("=== MONITORING REPORT ===")
    fmt.Println()

    for _, mt := range []MetricType{CPU, Memory, Disk} {
        r := results[mt]
        if r.Count > 0 {
            fmt.Printf("%-8s | Avg: %5.1f%% | Max: %5.1f%% | Min: %5.1f%% | Samples: %d | Hosts: %v\n",
                mt, r.Avg, r.Max, r.Min, r.Count, r.Hosts)
        }
    }

    fmt.Println()
    if len(alerts) > 0 {
        fmt.Printf("ALERTS (%d):\n", len(alerts))
        for _, a := range alerts {
            fmt.Printf("  ⚠ %s\n", a)
        }
    } else {
        fmt.Println("No alerts generated.")
    }
}
```

### 12.2 Version Rust

```rust
// Cargo.toml:
// [dependencies]
// tokio = { version = "1", features = ["full"] }
// tokio-util = "0.7"
// rand = "0.8"

use std::collections::HashMap;
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::mpsc;
use tokio::signal;
use tokio_util::sync::CancellationToken;
use rand::Rng;

// --- Tipos ---

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
enum MetricType {
    Cpu,
    Memory,
    Disk,
}

impl std::fmt::Display for MetricType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MetricType::Cpu => write!(f, "CPU"),
            MetricType::Memory => write!(f, "Memory"),
            MetricType::Disk => write!(f, "Disk"),
        }
    }
}

#[derive(Debug, Clone)]
struct Metric {
    metric_type: MetricType,
    host: String,
    value: f64,
}

#[derive(Debug)]
struct ProcessedMetric {
    metric: Metric,
    alert: Option<String>,
}

#[derive(Debug)]
struct AggregatedResult {
    metric_type: MetricType,
    avg: f64,
    max: f64,
    min: f64,
    count: usize,
    hosts: Vec<String>,
}

impl AggregatedResult {
    fn new(metric_type: MetricType) -> Self {
        Self {
            metric_type,
            avg: 0.0,
            max: f64::MIN,
            min: f64::MAX,
            count: 0,
            hosts: Vec::new(),
        }
    }

    fn update(&mut self, value: f64, host: &str) {
        self.count += 1;
        self.avg = (self.avg * (self.count - 1) as f64 + value) / self.count as f64;
        if value > self.max { self.max = value; }
        if value < self.min { self.min = value; }
        if !self.hosts.contains(&host.to_string()) {
            self.hosts.push(host.to_string());
        }
    }
}

// --- Colector de metricas ---

fn generate_metric_value(metric_type: MetricType) -> f64 {
    let mut rng = rand::thread_rng();
    match metric_type {
        MetricType::Cpu => rng.gen::<f64>() * 100.0,
        MetricType::Memory => 40.0 + rng.gen::<f64>() * 50.0,
        MetricType::Disk => 20.0 + rng.gen::<f64>() * 70.0,
    }
}

fn collect_metrics(
    token: CancellationToken,
    metric_type: MetricType,
    hosts: Vec<String>,
    interval: Duration,
) -> mpsc::Receiver<Metric> {
    let (tx, rx) = mpsc::channel(64);

    tokio::spawn(async move {
        let mut ticker = tokio::time::interval(interval);
        ticker.tick().await; // Primer tick inmediato

        loop {
            tokio::select! {
                _ = token.cancelled() => {
                    println!("[Collector {}] shutting down", metric_type);
                    break;
                }
                _ = ticker.tick() => {
                    for host in &hosts {
                        let metric = Metric {
                            metric_type,
                            host: host.clone(),
                            value: generate_metric_value(metric_type),
                        };
                        if tx.send(metric).await.is_err() {
                            return; // Receiver dropped
                        }
                    }
                }
            }
        }
    });

    rx
}

// --- Fan-in ---

fn fan_in(
    token: CancellationToken,
    receivers: Vec<mpsc::Receiver<Metric>>,
) -> mpsc::Receiver<Metric> {
    let (tx, rx) = mpsc::channel(64);

    for mut input in receivers {
        let tx = tx.clone();
        let token = token.clone();
        tokio::spawn(async move {
            loop {
                tokio::select! {
                    _ = token.cancelled() => break,
                    msg = input.recv() => {
                        match msg {
                            Some(metric) => {
                                if tx.send(metric).await.is_err() { break; }
                            }
                            None => break,
                        }
                    }
                }
            }
        });
    }

    // drop(tx) implicito al salir del scope de la funcion
    // — los clones dentro de los spawns mantienen el channel abierto
    rx
}

// --- Worker pool ---

fn process_metric(worker_id: usize, m: &Metric) -> ProcessedMetric {
    let alert = match m.metric_type {
        MetricType::Cpu if m.value > 90.0 => {
            Some(format!("HIGH CPU on {}: {:.1}%", m.host, m.value))
        }
        MetricType::Memory if m.value > 85.0 => {
            Some(format!("HIGH MEMORY on {}: {:.1}%", m.host, m.value))
        }
        MetricType::Disk if m.value > 80.0 => {
            Some(format!("HIGH DISK on {}: {:.1}%", m.host, m.value))
        }
        _ => None,
    };

    ProcessedMetric {
        metric: m.clone(),
        alert,
    }
}

fn process_worker_pool(
    token: CancellationToken,
    mut input: mpsc::Receiver<Metric>,
    workers: usize,
) -> mpsc::Receiver<ProcessedMetric> {
    let (tx, rx) = mpsc::channel(64);
    // Distribuir trabajo con un channel compartido
    let (job_tx, _) = tokio::sync::broadcast::channel::<()>(1);

    // Crear un channel interno para distribuir jobs
    let (dist_tx, _) = mpsc::channel::<Metric>(64);
    
    // Enfoque mas idiomatico: usar Arc<Mutex<Receiver>>
    // o multiples channels. Aqui usamos un channel intermedio.
    let input_shared = Arc::new(tokio::sync::Mutex::new(input));
    
    for worker_id in 0..workers {
        let tx = tx.clone();
        let token = token.clone();
        let input = Arc::clone(&input_shared);
        
        tokio::spawn(async move {
            loop {
                let metric = {
                    let mut guard = input.lock().await;
                    tokio::select! {
                        _ = token.cancelled() => break,
                        msg = guard.recv() => msg,
                    }
                };
                
                match metric {
                    Some(m) => {
                        // Simular procesamiento
                        let delay = rand::thread_rng().gen_range(0..50);
                        tokio::time::sleep(Duration::from_millis(delay)).await;
                        
                        let processed = process_metric(worker_id, &m);
                        if tx.send(processed).await.is_err() { break; }
                    }
                    None => break,
                }
            }
        });
    }

    rx
}

// --- Agregador ---

async fn aggregate(
    mut input: mpsc::Receiver<ProcessedMetric>,
) -> (HashMap<MetricType, AggregatedResult>, Vec<String>) {
    let mut results = HashMap::new();
    results.insert(MetricType::Cpu, AggregatedResult::new(MetricType::Cpu));
    results.insert(MetricType::Memory, AggregatedResult::new(MetricType::Memory));
    results.insert(MetricType::Disk, AggregatedResult::new(MetricType::Disk));
    let mut alerts = Vec::new();
    let mut total = 0usize;

    while let Some(pm) = input.recv().await {
        if let Some(r) = results.get_mut(&pm.metric.metric_type) {
            r.update(pm.metric.value, &pm.metric.host);
        }
        if let Some(alert_msg) = pm.alert {
            alerts.push(alert_msg);
        }
        total += 1;
        if total % 10 == 0 {
            println!("[Monitor] Processed {} metrics so far...", total);
        }
    }

    println!("[Monitor] Total metrics processed: {}", total);
    (results, alerts)
}

// --- Main ---

#[tokio::main]
async fn main() {
    let token = CancellationToken::new();

    // Signal handling
    let shutdown_token = token.clone();
    tokio::spawn(async move {
        signal::ctrl_c().await.expect("failed to listen for ctrl+c");
        println!("\nReceived Ctrl+C, shutting down...");
        shutdown_token.cancel();
    });

    // Timeout global de 10 segundos
    let timeout_token = token.clone();
    tokio::spawn(async move {
        tokio::time::sleep(Duration::from_secs(10)).await;
        timeout_token.cancel();
    });

    println!("=== Infrastructure Monitor (Rust) ===");
    println!("Monitoring for 10 seconds (Ctrl+C to stop early)...\n");

    let hosts: Vec<String> = vec!["web-01", "web-02", "api-01", "db-01"]
        .into_iter().map(String::from).collect();

    // Colectores concurrentes
    let cpu_rx = collect_metrics(
        token.clone(), MetricType::Cpu, hosts.clone(), Duration::from_millis(500),
    );
    let mem_rx = collect_metrics(
        token.clone(), MetricType::Memory, hosts.clone(), Duration::from_millis(700),
    );
    let disk_rx = collect_metrics(
        token.clone(), MetricType::Disk, hosts.clone(), Duration::from_secs(1),
    );

    // Fan-in
    let merged = fan_in(token.clone(), vec![cpu_rx, mem_rx, disk_rx]);

    // Worker pool
    let processed = process_worker_pool(token.clone(), merged, 4);

    // Agregar
    let (results, alerts) = aggregate(processed).await;

    // Reporte
    println!("\n=== MONITORING REPORT ===\n");

    for mt in [MetricType::Cpu, MetricType::Memory, MetricType::Disk] {
        if let Some(r) = results.get(&mt) {
            if r.count > 0 {
                println!(
                    "{:<8} | Avg: {:5.1}% | Max: {:5.1}% | Min: {:5.1}% | Samples: {} | Hosts: {:?}",
                    r.metric_type, r.avg, r.max, r.min, r.count, r.hosts
                );
            }
        }
    }

    println!();
    if !alerts.is_empty() {
        println!("ALERTS ({}):", alerts.len());
        for a in &alerts {
            println!("  ⚠ {}", a);
        }
    } else {
        println!("No alerts generated.");
    }
}
```

### 12.3 Analisis comparativo del programa

```
┌────────────────────────────┬──────────────────────────┬─────────────────────────────┐
│ Aspecto                     │ Version Go               │ Version Rust                │
├────────────────────────────┼──────────────────────────┼─────────────────────────────┤
│ Lineas de codigo           │ ~195                     │ ~260                        │
│ Dependencias externas       │ 0 (todo stdlib)          │ 3 (tokio, tokio-util, rand) │
│ Concurrency primitives     │ goroutine, chan, select,  │ tokio::spawn, mpsc,         │
│                             │ WaitGroup, Mutex, atomic │ select!, CancellationToken, │
│                             │                          │ Arc, Mutex                  │
│ Signal handling             │ signal.NotifyContext     │ tokio::signal::ctrl_c       │
│ Timeout                    │ context.WithTimeout      │ Manual (tokio::time::sleep)  │
│ Channel close              │ close(ch) explicito      │ Drop implicito               │
│ Multi-consumer channel     │ Nativo (range ch)        │ Arc<Mutex<Receiver>> wrapper │
│ Type safety en alertas     │ switch + string           │ match + Option<String>       │
│ Error handling             │ Ignorado (prototipo)     │ Ignorado (prototipo)         │
│ Data race protection       │ Solo con -race flag      │ Compilador (Send/Sync)       │
│ Memory management          │ GC                       │ Ownership + Arc              │
│ Boilerplate de concurrencia│ Bajo                     │ Medio-alto                   │
└────────────────────────────┴──────────────────────────┴─────────────────────────────┘
```

```
OBSERVACIONES CLAVE del programa comparativo:

1. CHANNEL CLOSE
   Go: close(ch) es explicito y crucial — range termina al cerrar
   Rust: drop(tx) es implicito — el channel se cierra cuando el ultimo
         Sender se dropea. Menos ceremonioso pero puede sorprender.

2. MULTI-CONSUMER
   Go: 5 goroutines pueden hacer "for m := range ch" del mismo channel
   Rust: necesitas Arc<Mutex<Receiver>> para compartir un receiver,
         porque mpsc::Receiver no implementa Clone.
         crossbeam::Receiver SI lo hace — mas ergonomico.

3. SIGNAL HANDLING
   Go: signal.NotifyContext integra signals con context → una linea
   Rust: necesitas un task separado con tokio::signal::ctrl_c
         y un CancellationToken manual → mas codigo

4. TIMEOUT COMPUESTO
   Go: context.WithTimeout(ctx, 10s) hereda la cancelacion del padre
       Y agrega timeout. Una sola primitiva para ambos.
   Rust: necesitas combinar CancellationToken con tokio::time::sleep
         en un task separado. Mas explicito pero mas verbose.

5. BOILERPLATE
   Go gana en conciseness: go func() {} vs tokio::spawn(async move {})
   Pero Rust gana en safety: el compilador verifica todo el ownership.
```

---

## 13. Ejercicios

### Ejercicio 1: Implementa ambas versiones (Go y Rust)
Escribe un programa que:
- Lea lineas de stdin concurrentemente
- Cuente las palabras de cada linea en un worker pool de 3 workers
- Imprima los resultados ordenados por numero de palabras
- Se cancele con Ctrl+C o despues de 30 segundos

Implementalo primero en Go y luego en Rust. Compara las lineas de codigo, la cantidad de imports, y los errores que el compilador de Rust te obligo a corregir vs los bugs que podrias tener en Go sin `-race`.

### Ejercicio 2: Benchmark de channels
Crea un benchmark que mida el throughput (mensajes/segundo) de:
- Go: `chan int` unbuffered, buffered(100), buffered(10000)
- Rust: `mpsc::sync_channel(0)`, `mpsc::sync_channel(100)`, `crossbeam::bounded(100)`

Usa 1 producer y 1 consumer. Envia 1,000,000 de mensajes. Reporta el tiempo y throughput de cada configuracion.

### Ejercicio 3: Race condition deliberada
Escribe un programa Go con un data race deliberado (multiples goroutines incrementando un contador sin mutex). Verifica que `go run -race .` lo detecta. Luego escribe el equivalente en Rust y observa que el compilador **no te deja compilarlo** sin `Arc<Mutex<>>` o atomics.

### Ejercicio 4: Pipeline bidireccional
Implementa un sistema request-response donde:
- Un "client" envia requests por un channel
- Un "server" procesa las requests y envia responses por otro channel
- Cada request tiene un ID que se matchea con su response
- Timeout de 2 segundos por request

Implementalo en Go (con channels + context) y en Rust (con tokio::oneshot para cada response). Compara la ergonomia de cada enfoque.

---

> **Fin de C08 — Concurrencia**. Este capitulo cubrio el ecosistema completo de concurrencia en Go: goroutines, channels (basicos, con direccion, select, patrones), sincronizacion (mutex, once/map/pool, context), y esta comparacion profunda con Rust. El siguiente capitulo aborda el sistema de paquetes e I/O.
>
> **Siguiente**: C09/S01 - Sistema de Paquetes, T01 - Paquetes — convenciones de nombrado, visibilidad (mayuscula/minuscula), internal packages
