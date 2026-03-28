# Async vs Threads: Cuándo Usar Cada Uno

## Índice

1. [Dos modelos de concurrencia](#1-dos-modelos-de-concurrencia)
2. [Cómo funciona cada modelo internamente](#2-cómo-funciona-cada-modelo-internamente)
3. [Costos: memoria, CPU, scheduling](#3-costos-memoria-cpu-scheduling)
4. [I/O-bound vs CPU-bound](#4-io-bound-vs-cpu-bound)
5. [Escalabilidad: 10 vs 10.000 vs 1.000.000](#5-escalabilidad-10-vs-10000-vs-1000000)
6. [Complejidad del código](#6-complejidad-del-código)
7. [Interoperabilidad: mezclar async y threads](#7-interoperabilidad-mezclar-async-y-threads)
8. [Guía de decisión](#8-guía-de-decisión)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Dos modelos de concurrencia

Rust ofrece dos modelos de concurrencia completos, cada uno con sus fortalezas:

```
┌─────────────────────────────────────────────────────────────────┐
│                   Threads del OS                                │
├─────────────────────────────────────────────────────────────────┤
│  • Unidad: std::thread::JoinHandle                              │
│  • Scheduler: kernel del OS                                     │
│  • Modelo: preemptivo (el kernel interrumpe)                    │
│  • Stack: ~8 MB por thread (configurable)                       │
│  • Creación: ~10-50 μs                                          │
│  • Bloqueo: cada thread puede bloquear independientemente       │
│  • Ideal para: CPU-bound, pocas conexiones, código síncrono     │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                   Async Tasks                                   │
├─────────────────────────────────────────────────────────────────┤
│  • Unidad: Future / tokio::task::JoinHandle                     │
│  • Scheduler: runtime userspace (Tokio)                         │
│  • Modelo: cooperativo (la task cede en .await)                  │
│  • Stack: ~pocos KB por task (proporcional a variables)          │
│  • Creación: ~0.1-1 μs                                          │
│  • Bloqueo: PROHIBIDO (paraliza el executor)                    │
│  • Ideal para: I/O-bound, miles de conexiones, networking       │
└─────────────────────────────────────────────────────────────────┘
```

Ambos modelos son válidos — la elección depende de la **naturaleza de la carga de trabajo**,
no de una superioridad inherente. Muchos programas reales usan los dos a la vez.

---

## 2. Cómo funciona cada modelo internamente

### 2.1 Threads del OS

Cada thread tiene su propio stack asignado por el kernel. El scheduler del OS decide cuándo
interrumpir un thread y ejecutar otro (preemptive scheduling):

```
Kernel Scheduler (preemptivo):

CPU Core 0              CPU Core 1
──────────              ──────────
Thread A ████████        Thread C ████████
         ↓ preempt               ↓ preempt
Thread B ████████        Thread D ████████
         ↓ preempt               ↓ preempt
Thread A ████████        Thread C ████████

• El kernel decide cuándo cambiar (timer interrupt, ~1-10ms)
• Cada thread tiene stack independiente (~8MB)
• Context switch: guardar/restaurar registros + TLB flush
• No necesita cooperación del código del thread
```

```rust
use std::thread;
use std::sync::{Arc, Mutex};

fn main() {
    let counter = Arc::new(Mutex::new(0));
    let mut handles = Vec::new();

    for _ in 0..4 {
        let counter = counter.clone();
        // Cada spawn crea un thread del OS con su propio stack
        handles.push(thread::spawn(move || {
            for _ in 0..1000 {
                let mut num = counter.lock().unwrap();
                *num += 1;
                // El kernel puede interrumpir AQUÍ, entre cualquier instrucción
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
    println!("Counter: {}", *counter.lock().unwrap());
}
```

### 2.2 Async Tasks

Las tasks comparten un pool pequeño de threads del OS. El runtime (Tokio) multiplexa miles
de tasks sobre esos pocos threads. Las tasks ceden control voluntariamente en cada `.await`:

```
Tokio Runtime (cooperativo):

Thread Pool: 4 threads (= cores)

Thread 0                Thread 1
──────────              ──────────
Task A ██ .await         Task E ██ .await
Task B ██ .await         Task F ██████ .await
Task C ████ .await       Task E ██ .await
Task A ██ .await         Task G ██ .await
Task D ██ done           Task F ██ done

• Las tasks ceden en .await (cooperativo)
• Una task que no cede bloquea su thread ("starvation")
• Cada task ocupa solo lo que necesitan sus variables
• No hay context switch del kernel — es cambio de puntero
• Miles de tasks en pocos threads
```

```rust
use tokio::time::{sleep, Duration};

#[tokio::main] // Crea 1 thread por core
async fn main() {
    let mut handles = Vec::new();

    // 10_000 tasks en ~4 threads
    for i in 0..10_000 {
        handles.push(tokio::spawn(async move {
            // .await → cede control al runtime
            sleep(Duration::from_millis(100)).await;
            // El runtime reanuda esta task cuando el timer expira
            i * 2
        }));
    }

    for h in handles {
        let _ = h.await;
    }
}
```

### 2.3 Comparación visual del modelo de ejecución

```
4 conexiones de red, cada una: 2ms CPU + 100ms espera I/O + 2ms CPU

═══════════ Threads (1 thread por conexión) ═══════════

Thread 0: ██░░░░░░░░░░░░░░░░░░░░██
Thread 1:   ██░░░░░░░░░░░░░░░░░░░░██
Thread 2:     ██░░░░░░░░░░░░░░░░░░░░██
Thread 3:       ██░░░░░░░░░░░░░░░░░░░░██

█ = CPU    ░ = Esperando I/O (thread bloqueado, desperdiciado)
Total threads: 4
Total tiempo: ~104ms por conexión


═══════════ Async (1 thread, 4 tasks) ═══════════

Thread 0: ██  ██  ██  ██░░░░░░░░██  ██  ██  ██

Detalle por task:
Task 0: ██─────wait─────██
Task 1:   ██───wait─────██
Task 2:     ██──wait────██
Task 3:       ██─wait───██

█ = CPU    ─ = Suspendida (.await, no consume thread)
Total threads: 1
Total tiempo: ~108ms (las 4 completas)

→ Async usó 1 thread para hacer el trabajo de 4
```

---

## 3. Costos: memoria, CPU, scheduling

### 3.1 Memoria por unidad de concurrencia

```
┌──────────────────┬──────────────────────────────────────────┐
│                  │  Memoria                                 │
├──────────────────┼──────────────────────────────────────────┤
│  Thread del OS   │  Stack: 8 MB default (Linux)             │
│                  │  + estructuras kernel: ~10 KB             │
│                  │  + TLS: variable                         │
│                  │  ≈ 8 MB por thread                       │
├──────────────────┼──────────────────────────────────────────┤
│  Async Task      │  Future state machine: proporcional a    │
│                  │  variables vivas en .await points         │
│                  │  Típico: 256 bytes - 10 KB               │
│                  │  ≈ 1-10 KB por task                      │
└──────────────────┴──────────────────────────────────────────┘
```

Implicación práctica:

```
10.000 conexiones simultáneas:

Threads:  10.000 × 8 MB = 80 GB   ← imposible en la mayoría de máquinas
Async:    10.000 × 4 KB = 40 MB   ← sin problema

1.000.000 conexiones:

Threads:  imposible (800 TB)
Async:    1.000.000 × 4 KB = 4 GB ← factible en un servidor moderno
```

### 3.2 Costo de creación

```rust
use std::time::Instant;

fn bench_thread_creation() {
    let start = Instant::now();
    let mut handles = Vec::new();

    for _ in 0..10_000 {
        handles.push(std::thread::spawn(|| {
            // Mínimo trabajo
            42
        }));
    }
    for h in handles {
        h.join().unwrap();
    }

    println!("10K threads: {:?}", start.elapsed());
    // Típico: 200-500ms
}

async fn bench_task_creation() {
    let start = Instant::now();
    let mut handles = Vec::new();

    for _ in 0..10_000 {
        handles.push(tokio::spawn(async {
            42
        }));
    }
    for h in handles {
        h.await.unwrap();
    }

    println!("10K tasks: {:?}", start.elapsed());
    // Típico: 2-10ms
}
```

```
Costo de creación (aproximado):

Thread del OS:   10-50 μs  (syscall clone + stack allocation)
Async Task:      0.1-1 μs  (heap allocation + enqueue)

Factor: 50-500x más rápido crear una task async
```

### 3.3 Costo de context switch

```
Context switch del OS (thread → thread):
  • Guardar registros CPU
  • Flush TLB (Translation Lookaside Buffer)
  • Restaurar registros del otro thread
  • Cache miss penalty
  ≈ 1-10 μs

Context switch de task (task → task):
  • Retornar Poll::Pending
  • Executor selecciona siguiente task
  • Llamar poll() de la siguiente
  ≈ 10-100 ns

Factor: 10-100x más rápido cambiar tasks que threads
```

### 3.4 Tabla resumen de costos

```
┌─────────────────────┬───────────────┬───────────────┬──────────┐
│ Métrica             │ Thread OS     │ Async Task    │ Factor   │
├─────────────────────┼───────────────┼───────────────┼──────────┤
│ Memoria             │ ~8 MB         │ ~1-10 KB      │ 800-8000x│
│ Creación            │ 10-50 μs      │ 0.1-1 μs      │ 50-500x  │
│ Context switch      │ 1-10 μs       │ 10-100 ns     │ 10-100x  │
│ Escalabilidad       │ ~1.000-10.000 │ ~100K-1M+     │ 100x+    │
│ Scheduling          │ Preemptivo    │ Cooperativo   │ —        │
│ Puede bloquear      │ Sí            │ NO            │ —        │
│ CPU-bound           │ Natural       │ Necesita      │ —        │
│                     │               │ spawn_blocking│          │
└─────────────────────┴───────────────┴───────────────┴──────────┘
```

---

## 4. I/O-bound vs CPU-bound

### 4.1 I/O-bound: async gana

Un programa es I/O-bound cuando pasa la mayor parte del tiempo **esperando**: red,
disco, bases de datos, APIs externas. El CPU está ocioso mientras espera.

```
I/O-bound típico (servidor web):

Recibir request ─── 0.1ms CPU
Consultar BD    ─── 0.1ms CPU + 50ms espera
Formatear resp  ─── 0.1ms CPU
Enviar response ─── 0.1ms CPU

Total: 0.4ms CPU de 50.4ms → 99.2% esperando I/O
```

Para I/O-bound, async es claramente superior porque las tasks suspendidas en `.await` no
consumen un thread:

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;

    loop {
        let (mut socket, _) = listener.accept().await?;

        // Una task por conexión — con 10K conexiones,
        // solo ~4 threads hacen todo el trabajo
        tokio::spawn(async move {
            let mut buf = [0u8; 1024];
            loop {
                let n = match socket.read(&mut buf).await {
                    Ok(0) => return,
                    Ok(n) => n,
                    Err(_) => return,
                };
                // .await cede control mientras se envía
                if socket.write_all(&buf[..n]).await.is_err() {
                    return;
                }
            }
        });
    }
}
```

### 4.2 CPU-bound: threads ganan

Un programa es CPU-bound cuando el CPU está activo constantemente: cálculos matemáticos,
compresión, criptografía, parsing pesado. No hay esperas de I/O.

```
CPU-bound típico (procesamiento de imagen):

Leer píxeles    ─── CPU activo
Aplicar filtro  ─── CPU activo
Resize          ─── CPU activo
Comprimir       ─── CPU activo

Total: 100% CPU, 0% espera
```

Para CPU-bound, threads son mejores porque aprovechan todos los cores sin overhead de
scheduling cooperativo:

```rust
use std::thread;

fn main() {
    let data: Vec<u64> = (0..10_000_000).collect();
    let num_cores = thread::available_parallelism().unwrap().get();
    let chunk_size = data.len() / num_cores;

    let handles: Vec<_> = data
        .chunks(chunk_size)
        .map(|chunk| {
            let chunk = chunk.to_vec();
            thread::spawn(move || {
                // Trabajo CPU-intensivo en cada core
                chunk.iter().map(|&x| expensive_computation(x)).sum::<u64>()
            })
        })
        .collect();

    let total: u64 = handles.into_iter()
        .map(|h| h.join().unwrap())
        .sum();

    println!("Total: {}", total);
}

fn expensive_computation(x: u64) -> u64 {
    // Simular trabajo CPU-heavy
    (0..100).fold(x, |acc, i| acc.wrapping_mul(i + 1).wrapping_add(7))
}
```

### 4.3 ¿Por qué async es malo para CPU-bound?

Las tasks async ceden control en `.await`. Si una task nunca hace `.await` (porque está
calculando), monopoliza su thread del executor:

```rust
// MAL: tarea CPU-bound en async
#[tokio::main]
async fn main() {
    // Esta task nunca cede → bloquea 1 thread del executor
    tokio::spawn(async {
        let mut sum = 0u64;
        for i in 0..1_000_000_000 {
            sum = sum.wrapping_add(i); // No hay .await → no cede
        }
        sum
    });

    // Esta task puede NO ejecutarse durante segundos
    // porque la anterior monopoliza el thread
    tokio::spawn(async {
        println!("I might be starved!");
    });
}
```

```
Efecto de una task CPU-bound en el executor:

Thread 0: [CPU-bound task ██████████████████████████████████████]
Thread 1: [Task A ██ Task B ██ Task C ██ Task D ██ ...]
Thread 2: [Task E ██ Task F ██ ...]
Thread 3: [Task G ██ Task H ██ ...]

→ Thread 0 está "secuestrado" por la task CPU-bound
→ Las tasks que el runtime quiere asignar a Thread 0 esperan
→ Latencia impredecible para todas las tasks
```

### 4.4 La solución: `spawn_blocking`

Cuando necesitas CPU-bound dentro de un programa async, usa `spawn_blocking`:

```rust
use tokio::task;

#[tokio::main]
async fn main() {
    // Mover trabajo CPU-heavy a un thread dedicado
    let result = task::spawn_blocking(|| {
        // Esto corre en un thread separado, NO en el executor
        let mut sum = 0u64;
        for i in 0..1_000_000_000 {
            sum = sum.wrapping_add(i);
        }
        sum
    }).await.unwrap();

    println!("Result: {}", result);

    // El executor siguió libre para otras tasks
    // mientras spawn_blocking trabajaba
}
```

```
Con spawn_blocking:

Executor threads (async):     Blocking threads (sync):
Thread 0: [Task A ██ B ██]    Thread B0: [CPU-bound ████████████]
Thread 1: [Task C ██ D ██]    Thread B1: [disponible]
Thread 2: [Task E ██ F ██]
Thread 3: [Task G ██ H ██]

→ El executor permanece libre para I/O
→ El trabajo CPU-bound tiene su propio thread
```

---

## 5. Escalabilidad: 10 vs 10.000 vs 1.000.000

### 5.1 Pocos (< 100): ambos funcionan

Con pocas tareas concurrentes, la diferencia es insignificante. Threads son más simples
de escribir:

```rust
// Para 10 conexiones, threads es perfectamente viable
use std::net::TcpListener;
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;

    for stream in listener.incoming() {
        let mut stream = stream?;
        std::thread::spawn(move || {
            let mut buf = [0u8; 1024];
            loop {
                let n = stream.read(&mut buf).unwrap_or(0);
                if n == 0 { return; }
                stream.write_all(&buf[..n]).unwrap();
            }
        });
    }
    Ok(())
}
// Simple, claro, sin runtime. Para < 100 conexiones, perfecto.
```

### 5.2 Miles (1K-10K): async empieza a ganar

Con miles de conexiones concurrentes, el costo de memoria de threads se vuelve
significativo:

```
1.000 conexiones:
  Threads: 1.000 × 8 MB = 8 GB   ← funciona pero es caro
  Async:   1.000 × 4 KB = 4 MB   ← trivial

10.000 conexiones:
  Threads: 10.000 × 8 MB = 80 GB  ← necesitas un servidor enorme
  Async:   10.000 × 4 KB = 40 MB  ← sin esfuerzo
```

Además, el scheduler del kernel degrada con miles de threads (O(n) o O(log n) para
seleccionar el siguiente thread), mientras que Tokio usa colas lock-free optimizadas.

### 5.3 Muchos (100K+): async es la única opción

```
100.000 conexiones simultáneas (C100K problem):
  Threads: 100.000 × 8 MB = 800 GB  ← imposible
  Async:   100.000 × 4 KB = 400 MB  ← un servidor modesto

1.000.000 conexiones (C1M):
  Threads: imposible
  Async:   1M × 4 KB = 4 GB         ← factible con tuning del OS
```

Ejemplo realista: un proxy reverso o un chat server que mantiene conexiones WebSocket
abiertas de cientos de miles de usuarios simultáneos.

```rust
use tokio::net::TcpListener;
use std::sync::atomic::{AtomicU64, Ordering};

static ACTIVE_CONNECTIONS: AtomicU64 = AtomicU64::new(0);

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080").await?;

    loop {
        let (socket, _) = listener.accept().await?;
        let count = ACTIVE_CONNECTIONS.fetch_add(1, Ordering::Relaxed) + 1;

        if count % 10_000 == 0 {
            println!("Active connections: {}", count);
        }

        tokio::spawn(async move {
            handle_connection(socket).await;
            ACTIVE_CONNECTIONS.fetch_sub(1, Ordering::Relaxed);
        });
    }
}

async fn handle_connection(_socket: tokio::net::TcpStream) {
    // Mantener conexión viva (WebSocket, long-polling, etc.)
    tokio::time::sleep(std::time::Duration::from_secs(300)).await;
}
```

### 5.4 Resumen de escalabilidad

```
Conexiones  │  Threads          │  Async           │  Recomendación
────────────┼───────────────────┼──────────────────┼─────────────────
< 100       │  ✅ Simple        │  ✅ Funciona     │  Threads (simple)
100-1K      │  ✅ Funciona      │  ✅ Mejor mem    │  Depende del caso
1K-10K      │  ⚠️ Costoso       │  ✅ Ideal        │  Async
10K-100K    │  ❌ Impracticable │  ✅ Sin problema  │  Async
100K+       │  ❌ Imposible     │  ✅ Factible     │  Async obligatorio
```

---

## 6. Complejidad del código

### 6.1 Threads: código más simple

El código con threads se lee de arriba a abajo. Las operaciones bloquean naturalmente,
los errores son locales, y el modelo mental es directo:

```rust
use std::net::TcpStream;
use std::io::{BufRead, BufReader, Write};

fn handle_client(stream: TcpStream) -> std::io::Result<()> {
    let mut reader = BufReader::new(stream.try_clone()?);
    let mut writer = stream;
    let mut line = String::new();

    loop {
        line.clear();
        let n = reader.read_line(&mut line)?;  // Bloquea — simple
        if n == 0 { return Ok(()); }

        let response = process(&line);          // Síncrono — simple
        writer.write_all(response.as_bytes())?; // Bloquea — simple
    }
}

fn process(input: &str) -> String {
    format!("Echo: {}", input)
}
```

### 6.2 Async: más complejo pero más poderoso

El código async introduce conceptos adicionales: `Pin`, `Send + 'static`, cancellation
safety, runtimes, y la dificultad de que ciertos patrones no funcionan igual:

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

async fn handle_client(stream: TcpStream) -> std::io::Result<()> {
    let (reader, mut writer) = stream.into_split(); // Necesario para async
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?; // .await obligatorio
        if n == 0 { return Ok(()); }

        let response = process(&line);
        writer.write_all(response.as_bytes()).await?;
    }
}

fn process(input: &str) -> String {
    format!("Echo: {}", input)
}
```

### 6.3 Complejidades específicas de async

**Restricción `Send + 'static` en `spawn`**:

```rust
use std::rc::Rc;

#[tokio::main]
async fn main() {
    let data = Rc::new(42); // Rc no es Send

    // ERROR: Rc no es Send, pero spawn requiere Send
    // tokio::spawn(async move {
    //     println!("{}", data);
    // });

    // Solución: usar Arc en vez de Rc
    let data = std::sync::Arc::new(42);
    tokio::spawn(async move {
        println!("{}", data); // Arc es Send ✅
    });
}
```

**Variables no-Send a través de `.await`**:

```rust
use std::cell::RefCell;

async fn example() {
    let cell = RefCell::new(42);

    // Si cell está viva a través de .await, el Future no es Send
    // y no puede usarse con tokio::spawn
    {
        let _borrow = cell.borrow();
        // _borrow se dropea aquí, antes de .await → OK
    }

    tokio::time::sleep(std::time::Duration::from_millis(1)).await;
    // cell se usa después de .await, pero no hay borrow activo → OK
    println!("{}", cell.borrow());
}
```

**Async en traits (limitaciones históricas)**:

```rust
// Desde Rust 1.75: async fn en traits
trait Service {
    async fn handle(&self, request: String) -> String;
}

// Pero spawn requiere Send, y el future retornado por
// trait async fn no es automáticamente Send.
// Para eso necesitas trait bounds adicionales o async-trait crate.
```

### 6.4 Tabla de complejidad

```
┌─────────────────────────┬───────────────┬──────────────────────┐
│ Aspecto                 │ Threads       │ Async                │
├─────────────────────────┼───────────────┼──────────────────────┤
│ Modelo mental           │ Secuencial    │ State machine        │
│ Lectura del código      │ Top-to-bottom │ .await interrumpe    │
│ Compartir datos         │ Arc + Mutex   │ Arc + Mutex (+ Send) │
│ Errores del compilador  │ Normales      │ A veces crípticos    │
│ Debugging               │ Directo       │ Stacktraces difusos  │
│ Composición             │ join/channel  │ join!/select!/spawn  │
│ Cancellation            │ No nativa     │ Drop = cancel        │
│ Lifetime constraints    │ move o 'static│ Send + 'static       │
│ Curva de aprendizaje    │ Baja          │ Media-Alta           │
│ Dependencia de runtime  │ Ninguna (std) │ Tokio/async-std      │
└─────────────────────────┴───────────────┴──────────────────────┘
```

---

## 7. Interoperabilidad: mezclar async y threads

En la práctica, la mayoría de programas reales usan ambos. Tokio facilita la transición
entre los dos mundos.

### 7.1 Código síncrono bloqueante dentro de async: `spawn_blocking`

```rust
use tokio::task;

#[tokio::main]
async fn main() {
    // Leer archivo grande (operación bloqueante del OS)
    let content = task::spawn_blocking(|| {
        // Esto corre en un thread del pool de blocking
        std::fs::read_to_string("/etc/passwd").unwrap()
    }).await.unwrap();

    println!("File has {} bytes", content.len());

    // Cálculo CPU-heavy
    let result = task::spawn_blocking(|| {
        (0..10_000_000u64).map(|x| x * x).sum::<u64>()
    }).await.unwrap();

    println!("Sum of squares: {}", result);
}
```

### 7.2 Código async dentro de threads síncronos: `Runtime::block_on`

```rust
use tokio::runtime::Runtime;

fn synchronous_function() {
    // Crear un runtime ad-hoc para ejecutar código async
    let rt = Runtime::new().unwrap();

    let result = rt.block_on(async {
        // Código async dentro de un contexto síncrono
        tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        reqwest_like_operation().await
    });

    println!("Got: {}", result);
}

async fn reqwest_like_operation() -> String {
    "http response".to_string()
}

fn main() {
    // Thread síncrono que necesita llamar código async
    std::thread::spawn(|| {
        synchronous_function();
    }).join().unwrap();
}
```

### 7.3 Handle al runtime desde threads

```rust
use tokio::runtime::Handle;

#[tokio::main]
async fn main() {
    let handle = Handle::current();

    // Spawn un thread del OS que interactúa con el runtime
    let thread_handle = std::thread::spawn(move || {
        // block_on ejecuta un future desde un thread síncrono
        handle.block_on(async {
            tokio::time::sleep(std::time::Duration::from_millis(100)).await;
            println!("Async from OS thread!");
        });
    });

    thread_handle.join().unwrap();
}
```

### 7.4 Patrón completo: servidor async + workers CPU-bound

```rust
use tokio::sync::mpsc;
use tokio::task;

#[derive(Debug)]
struct ImageJob {
    id: u32,
    data: Vec<u8>,
}

#[derive(Debug)]
struct ProcessedImage {
    id: u32,
    result: Vec<u8>,
}

fn cpu_heavy_processing(data: &[u8]) -> Vec<u8> {
    // Simular compresión/resize de imagen
    std::thread::sleep(std::time::Duration::from_millis(50));
    data.iter().map(|&b| b.wrapping_mul(2)).collect()
}

#[tokio::main]
async fn main() {
    let (job_tx, mut job_rx) = mpsc::channel::<ImageJob>(32);
    let (result_tx, mut result_rx) = mpsc::channel::<ProcessedImage>(32);

    // Pool de workers CPU-bound
    for _ in 0..4 {
        let result_tx = result_tx.clone();
        let job_rx_shared = std::sync::Arc::new(
            tokio::sync::Mutex::new(None::<mpsc::Receiver<ImageJob>>)
        );

        // Simplificación: un worker por tarea
        let rtx = result_tx.clone();
        // En producción usarías un patrón más robusto
        tokio::spawn(async move {
            // Procesar en blocking thread
            let _ = rtx;
        });
    }
    drop(result_tx);

    // Simular: enviar jobs desde el lado async
    for id in 0..10 {
        let job = ImageJob {
            id,
            data: vec![1, 2, 3, 4, 5],
        };

        // Procesar en thread bloqueante y enviar resultado por channel
        let rtx = result_tx.clone();
        let data = job.data.clone();
        let job_id = job.id;

        task::spawn_blocking(move || {
            let result = cpu_heavy_processing(&data);
            // send() es síncrono — no necesitamos .await
            // Pero estamos en un blocking thread, así que usamos
            // el runtime handle o un unbounded channel
            let _ = rtx;
            ProcessedImage { id: job_id, result }
        });
    }

    // Versión simplificada y correcta:
    let results: Vec<ProcessedImage> = {
        let mut futs = Vec::new();
        for id in 0..10 {
            let data = vec![1, 2, 3, 4, 5];
            futs.push(task::spawn_blocking(move || {
                let result = cpu_heavy_processing(&data);
                ProcessedImage { id, result }
            }));
        }
        let mut results = Vec::new();
        for f in futs {
            results.push(f.await.unwrap());
        }
        results
    };

    for r in &results {
        println!("Image {} processed: {} bytes", r.id, r.result.len());
    }
}
```

---

## 8. Guía de decisión

### 8.1 Diagrama de decisión

```
¿Tu carga principal es...?
│
├── I/O-bound (red, disco, BD, APIs)
│   │
│   ├── ¿Cuántas conexiones concurrentes?
│   │   │
│   │   ├── < 100         → Threads (más simple)
│   │   │                   o Async (si ya usas Tokio)
│   │   │
│   │   ├── 100 - 10K     → Async recomendado
│   │   │
│   │   └── 10K+          → Async obligatorio
│   │
│   └── ¿Necesitas cancelación/timeout granular?
│       ├── Sí  → Async (select!, timeout nativo)
│       └── No  → Threads puede ser suficiente
│
├── CPU-bound (cálculo, compresión, parsing)
│   │
│   ├── ¿Solo CPU, sin I/O?
│   │   └── Threads (o Rayon para data parallelism)
│   │
│   └── ¿CPU-bound DENTRO de un programa async?
│       └── spawn_blocking o Rayon
│
└── Mixto (I/O + CPU)
    └── Async para I/O + spawn_blocking para CPU
```

### 8.2 Decisión por tipo de aplicación

```
┌───────────────────────────┬──────────────┬──────────────────────┐
│ Aplicación                │ Modelo       │ Por qué              │
├───────────────────────────┼──────────────┼──────────────────────┤
│ Web server / API          │ Async        │ Miles de conexiones  │
│ Proxy reverso             │ Async        │ I/O puro, alta conc. │
│ Chat / WebSocket server   │ Async        │ Conexiones long-lived│
│ CLI tool                  │ Threads      │ Pocas tasks, simple  │
│ Image processing          │ Threads/Rayon│ CPU-bound puro       │
│ Game engine               │ Threads      │ Latencia predecible  │
│ Database engine           │ Threads      │ Control fino, I/O    │
│                           │              │ directo (io_uring)   │
│ Microservicio gRPC        │ Async (tonic)│ I/O, muchos clientes │
│ Crawler / scraper         │ Async        │ Miles de HTTP conc.  │
│ Scientific computing      │ Threads/Rayon│ CPU-bound puro       │
│ File processor (batch)    │ Threads      │ I/O secuencial       │
│ Log aggregator            │ Async        │ Muchas fuentes conc. │
│ Embedded / no_std         │ Threads (bare)│ Sin allocator/runtime│
└───────────────────────────┴──────────────┴──────────────────────┘
```

### 8.3 Preguntas clave para decidir

1. **¿Tu programa pasa más tiempo esperando o calculando?**
   - Esperando → Async
   - Calculando → Threads

2. **¿Cuántas unidades concurrentes necesitas?**
   - < 100 → Cualquiera (threads es más simple)
   - \> 1000 → Async

3. **¿Necesitas cancelación granular (timeouts, select)?**
   - Sí → Async tiene cancelación nativa
   - No → Threads es más simple

4. **¿Tu equipo ya conoce async Rust?**
   - No → Threads tiene curva de aprendizaje menor
   - Sí → Async si el caso lo justifica

5. **¿Dependes de librerías que son async o sync?**
   - Si tu BD driver es async (sqlx) → todo el stack será async
   - Si tu librería es sync → threads o `spawn_blocking`

6. **¿Necesitas latencia predecible (real-time)?**
   - Sí → Threads (el scheduler cooperativo de async puede causar jitter)
   - No → Cualquiera

### 8.4 Anti-patrones

```
❌ Usar async para todo
   → Overhead innecesario si tienes 5 threads haciendo cálculos

❌ Usar threads para 10K+ conexiones
   → Agotarás la memoria y el scheduler del kernel degradará

❌ CPU-bound en tasks async sin spawn_blocking
   → Starves el executor, degrada latencia de todas las tasks

❌ block_on dentro de un contexto async
   → Panic o deadlock (el thread ya está ejecutando el runtime)

❌ Reescribir código sync que funciona a async "por modernidad"
   → Si funciona y escala suficiente, no lo toques

❌ Elegir basándose solo en rendimiento micro
   → La complejidad del código y la mantenibilidad importan más
     que 10μs de diferencia por operación
```

---

## 9. Errores comunes

### Error 1: Usar `block_on` dentro de un contexto async

```rust
use tokio::runtime::Runtime;

// MAL — panic: "Cannot start a runtime from within a runtime"
#[tokio::main]
async fn main() {
    // Ya estamos dentro del runtime de Tokio
    let rt = Runtime::new().unwrap();
    rt.block_on(async {
        println!("This panics!");
    });
}

// BIEN — simplemente hacer .await
#[tokio::main]
async fn main() {
    some_async_function().await;
}

// O si necesitas llamar async desde sync FUERA del runtime:
fn main() {
    let rt = Runtime::new().unwrap();
    rt.block_on(async {
        some_async_function().await; // OK: no hay runtime anidado
    });
}

async fn some_async_function() {
    println!("Works!");
}
```

**Por qué ocurre**: `block_on` toma control del thread actual para ejecutar el runtime.
Si ya estás dentro de un runtime, intentar crear otro en el mismo thread causa un conflicto.

### Error 2: Elegir async solo por la cantidad de `.await`

```rust
// MAL — programa CPU-bound disfrazado de async
// Que haya .await no significa que sea I/O-bound
#[tokio::main]
async fn main() {
    let data = load_data().await; // 1 lectura de disco

    // El 99% del tiempo es CPU puro — async no ayuda aquí
    let result = data.iter()
        .map(|x| expensive_math(*x))
        .collect::<Vec<_>>();

    save_result(&result).await; // 1 escritura de disco
}

// BIEN — async para I/O, threads/rayon para CPU
#[tokio::main]
async fn main() {
    let data = load_data().await;

    // Mover CPU-bound a un blocking thread
    let result = tokio::task::spawn_blocking(move || {
        data.iter()
            .map(|x| expensive_math(*x))
            .collect::<Vec<_>>()
    }).await.unwrap();

    save_result(&result).await;
}

async fn load_data() -> Vec<u64> { vec![1, 2, 3] }
async fn save_result(_data: &[u64]) {}
fn expensive_math(x: u64) -> u64 { x * x }
```

**Por qué importa**: tener dos `.await` en el programa no lo convierte en I/O-bound.
La decisión threads vs async debe basarse en **dónde pasa el tiempo** el programa,
no en cuántos `.await` tiene.

### Error 3: Crear un thread por conexión a escala

```rust
use std::net::TcpListener;

// MAL — con 10K clientes, crea 10K threads = 80GB de stacks
fn main() {
    let listener = TcpListener::bind("0.0.0.0:8080").unwrap();

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        std::thread::spawn(move || {
            handle_client(stream); // Si el cliente está idle 5 min,
                                    // el thread duerme 5 min
        });
    }
}

// BIEN — thread pool limitado para moderar
fn main_pool() {
    let listener = TcpListener::bind("0.0.0.0:8080").unwrap();
    let pool = threadpool::ThreadPool::new(128); // Máximo 128 threads

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        pool.execute(move || {
            handle_client(stream);
        });
    }
    // Pero: si los 128 threads están bloqueados en I/O,
    // nuevas conexiones esperan. Async resuelve esto mejor.
}

fn handle_client(_stream: std::net::TcpStream) {
    std::thread::sleep(std::time::Duration::from_secs(300)); // Simular idle
}
```

**Por qué ocurre**: el patrón "un thread por conexión" escala linealmente en memoria.
Funciona para servicios internos con pocos clientes, pero no para servicios públicos.

### Error 4: Ignorar que `spawn_blocking` tiene un pool limitado

```rust
// MAL — saturar el pool de blocking threads
#[tokio::main]
async fn main() {
    let mut handles = Vec::new();

    // El pool de blocking tiene por defecto 512 threads
    // Si todos están ocupados, las nuevas llamadas ESPERAN
    for i in 0..1000 {
        handles.push(tokio::task::spawn_blocking(move || {
            // Operación que tarda 10 segundos
            std::thread::sleep(std::time::Duration::from_secs(10));
            i
        }));
    }

    // Las últimas 488 tareas esperan a que las primeras 512 terminen
    for h in handles {
        let _ = h.await;
    }
}

// BIEN — limitar la concurrencia con un semáforo
use tokio::sync::Semaphore;
use std::sync::Arc;

#[tokio::main]
async fn main() {
    let semaphore = Arc::new(Semaphore::new(32)); // Max 32 concurrentes
    let mut handles = Vec::new();

    for i in 0..1000 {
        let permit = semaphore.clone().acquire_owned().await.unwrap();
        handles.push(tokio::task::spawn_blocking(move || {
            let _permit = permit; // Se libera al dropear
            std::thread::sleep(std::time::Duration::from_secs(1));
            i
        }));
    }

    for h in handles {
        let _ = h.await;
    }
}
```

### Error 5: Mezclar `std::sync::Mutex` en async sin cuidado

```rust
use std::sync::Mutex;

// PELIGROSO — si el lock se mantiene a través de .await
#[tokio::main]
async fn main() {
    let data = std::sync::Arc::new(Mutex::new(Vec::new()));

    let d = data.clone();
    tokio::spawn(async move {
        let mut guard = d.lock().unwrap();
        // Si hacemos .await con el guard retenido,
        // bloqueamos otras tasks que quieren el lock
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
        // ← Otras tasks esperan este lock durante 1 segundo
        guard.push(42);
    });
}

// BIEN — Opción 1: mantener el lock el mínimo tiempo
#[tokio::main]
async fn main() {
    let data = std::sync::Arc::new(Mutex::new(Vec::new()));

    let d = data.clone();
    tokio::spawn(async move {
        // Trabajo async ANTES del lock
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;

        // Lock solo para la operación rápida
        {
            let mut guard = d.lock().unwrap();
            guard.push(42);
        } // guard se dropea inmediatamente
    });
}

// BIEN — Opción 2: usar tokio::sync::Mutex si necesitas .await con lock
use tokio::sync::Mutex as AsyncMutex;

#[tokio::main]
async fn main_v2() {
    let data = std::sync::Arc::new(AsyncMutex::new(Vec::new()));

    let d = data.clone();
    tokio::spawn(async move {
        let mut guard = d.lock().await; // Lock async
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
        guard.push(42); // OK: AsyncMutex permite .await con guard
    });
}
```

**Regla**: usa `std::sync::Mutex` si el lock se mantiene brevemente sin `.await` dentro.
Usa `tokio::sync::Mutex` si necesitas `.await` mientras mantienes el lock. `std::sync::Mutex`
es más rápido para operaciones cortas.

---

## 10. Cheatsheet

```text
──────────────── Cuándo usar Threads ─────────────────
CPU-bound puro (cálculos, compresión)    → std::thread o Rayon
Pocas tareas concurrentes (< 100)        → std::thread (más simple)
Código que bloquea (FFI, DNS legacy)     → std::thread
Latencia predecible necesaria            → std::thread
Sin dependencia de runtime               → std::thread (solo std)

──────────────── Cuándo usar Async ───────────────────
I/O-bound (red, disco, BD)              → Tokio async
Miles de conexiones concurrentes         → Tokio async
Cancelación/timeout granular             → select!, timeout
Web servers, proxies, chat               → Tokio async
El ecosistema ya es async (sqlx, tonic)  → Tokio async

──────────────── Mezclar ambos ───────────────────────
CPU-bound en programa async              → spawn_blocking
Sync bloqueante en programa async        → spawn_blocking
Async desde contexto sync                → Runtime::block_on
Handle al runtime desde thread           → Handle::current()

──────────────── Costos comparativos ─────────────────
                  Thread OS     Async Task
Memoria           ~8 MB         ~1-10 KB
Creación          10-50 μs      0.1-1 μs
Context switch    1-10 μs       10-100 ns
Escala máxima     ~10K          ~1M+

──────────────── Decisión rápida ─────────────────────
¿Espera > calcula?           → Async
¿Calcula > espera?           → Threads
¿> 1000 concurrentes?        → Async
¿< 100 concurrentes?         → Threads (o lo que ya usas)
¿Necesitas timeout/select?   → Async
¿Librería core es async?     → Async para todo
¿Librería core es sync?      → Threads o spawn_blocking

──────────────── Anti-patrones ───────────────────────
std::thread::sleep en async        → tokio::time::sleep
block_on dentro de async           → .await directo
Thread por conexión a escala       → Async o thread pool
CPU-bound sin spawn_blocking       → Starva el executor
Mutex lock a través de .await      → tokio::sync::Mutex
Async "porque sí"                  → Solo si lo necesitas
```

---

## 11. Ejercicios

### Ejercicio 1: Benchmark threads vs async

Implementa la **misma tarea** con threads y con async, y mide la diferencia. La tarea:
crear N unidades concurrentes, cada una espera 100ms (simulando I/O) y retorna un valor.

```rust
use std::time::Instant;

fn bench_threads(n: usize) -> (Vec<usize>, std::time::Duration) {
    let start = Instant::now();

    // TODO: Crear n threads, cada uno:
    //   1. std::thread::sleep(100ms)
    //   2. Retorna su índice
    // Recolectar todos los resultados en un Vec
    // Retornar (resultados, duración)

    todo!()
}

async fn bench_async(n: usize) -> (Vec<usize>, std::time::Duration) {
    let start = Instant::now();

    // TODO: Crear n tasks con tokio::spawn, cada una:
    //   1. tokio::time::sleep(100ms).await
    //   2. Retorna su índice
    // Recolectar todos los resultados en un Vec
    // Retornar (resultados, duración)

    todo!()
}

#[tokio::main]
async fn main() {
    // TODO: Ejecutar ambos benchmarks para n = 10, 100, 1000, 10000
    // Imprimir una tabla comparativa:
    //   N     | Threads (ms)  | Async (ms)  | Factor
    //   10    | ???           | ???         | ???
    //   100   | ???           | ???         | ???
    //   1000  | ???           | ???         | ???
    //   10000 | ???           | ???         | ???
    //
    // NOTA: bench_threads es sync, así que usa spawn_blocking
    // para llamarlo desde el contexto async.
}
```

**Pregunta de reflexión**: ¿a partir de qué valor de N la diferencia se vuelve
dramática? ¿Por qué el tiempo de threads crece con N pero el de async se mantiene
cercano a 100ms independientemente de N?

### Ejercicio 2: Servidor mixto (async I/O + CPU-bound)

Implementa un servidor que recibe números por TCP, calcula si son primos (CPU-bound),
y envía la respuesta:

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

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

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:9999").await?;
    println!("Listening on :9999");

    loop {
        let (socket, addr) = listener.accept().await?;
        println!("Client connected: {}", addr);

        tokio::spawn(async move {
            // TODO: Leer líneas del socket
            // Cada línea contiene un número (u64)
            // Verificar si es primo usando spawn_blocking
            // (porque is_prime con números grandes es CPU-heavy)
            // Responder "N is prime\n" o "N is not prime\n"
            // Manejar errores de parsing con un mensaje al cliente
        });
    }
}

// Para probar: echo "7919" | nc localhost 9999
// O enviar múltiples: printf "2\n17\n100\n7919\n" | nc localhost 9999
```

**Pregunta de reflexión**: si `is_prime` tarda microsegundos para números pequeños,
¿vale la pena usar `spawn_blocking`? ¿Cuál sería un umbral razonable para decidir
"este número es lo suficientemente grande para moverlo a un blocking thread"? Implementa
esa heurística.

### Ejercicio 3: Comparar modelos de cancelación

Implementa la misma lógica de "carrera entre dos fuentes de datos con timeout"
usando tres enfoques: threads puros, async puro, y mixto:

```rust
use std::time::{Duration, Instant};

// Simular dos fuentes de datos con latencia variable
fn source_a_sync() -> String {
    std::thread::sleep(Duration::from_millis(150));
    "data from A".into()
}

fn source_b_sync() -> String {
    std::thread::sleep(Duration::from_millis(300));
    "data from B".into()
}

async fn source_a_async() -> String {
    tokio::time::sleep(Duration::from_millis(150)).await;
    "data from A".into()
}

async fn source_b_async() -> String {
    tokio::time::sleep(Duration::from_millis(300)).await;
    "data from B".into()
}

// TODO: Implementar race_threads()
// Lanzar dos threads, usar un channel para recibir el primer resultado,
// timeout de 200ms. La fuente B no debería completar (tarda 300ms).
// ¿Cómo cancelas el thread que pierde? (Spoiler: no puedes fácilmente)
fn race_threads() -> Result<String, &'static str> {
    todo!()
}

// TODO: Implementar race_async()
// Usar select! para correr ambas fuentes async con timeout de 200ms.
// La fuente que pierde es dropeada automáticamente.
async fn race_async() -> Result<String, &'static str> {
    todo!()
}

#[tokio::main]
async fn main() {
    // TODO: Ejecutar ambas versiones, imprimir:
    //   - Qué fuente ganó
    //   - Cuánto tardó
    //   - Si la otra fuente fue cancelada efectivamente

    // TODO: Discutir: en race_threads, el thread perdedor
    // sigue corriendo hasta que termina. ¿Cuántos recursos
    // desperdicia? ¿Cómo lo compara con la versión async?
}
```

**Pregunta de reflexión**: una diferencia fundamental entre threads y async es la
cancelación. En async, `select!` dropea las ramas que pierden (cancelación inmediata y
limpia). Con threads, ¿cómo "cancelarías" el thread perdedor? Considera tres opciones:
un `AtomicBool` de verificación periódica, `pthread_cancel` (peligroso), o simplemente
dejarlo terminar. ¿Cuáles son los tradeoffs de cada enfoque?