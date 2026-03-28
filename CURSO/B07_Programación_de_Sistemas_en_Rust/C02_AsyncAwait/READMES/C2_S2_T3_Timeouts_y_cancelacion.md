# Timeouts y Cancelación

## Índice

1. [El problema: operaciones que nunca terminan](#1-el-problema-operaciones-que-nunca-terminan)
2. [tokio::time::timeout](#2-tokiotimetimeout)
3. [tokio::time::sleep y tokio::time::interval](#3-tokiotimesleep-y-tokiotimeinterval)
4. [select! para cancelación](#4-select-para-cancelación)
5. [Cancelación por drop](#5-cancelación-por-drop)
6. [Cancellation safety](#6-cancellation-safety)
7. [Patrones de timeout avanzados](#7-patrones-de-timeout-avanzados)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema: operaciones que nunca terminan

En programación de sistemas, las operaciones de I/O pueden bloquearse indefinidamente:
un servidor remoto no responde, un cliente nunca envía datos, una resolución DNS se
estanca. Sin un mecanismo de timeout, una sola operación colgada puede bloquear recursos
del sistema indefinidamente.

```
Sin timeout:                      Con timeout:
─────────────                     ────────────
  connect() ──────────────?         connect() ──────┐
     ↑                                 ↑            │ 5s
  Espera infinita                   Err(Elapsed) ◄──┘
  Recurso perdido                   Recurso liberado
```

En código síncrono, los timeouts se implementan con `setsockopt(SO_RCVTIMEO)` o
`poll()`/`epoll()` con plazo. En async, Tokio ofrece primitivas de alto nivel que
componen naturalmente con el modelo de futures.

La cancelación en Tokio se basa en un principio simple: **dropear un `Future` lo
cancela**. No hay señales, no hay flags — el runtime simplemente deja de pollearlo y
ejecuta su destructor.

```
┌──────────────────────────────────────────────────────┐
│            Mecanismos de cancelación en Tokio         │
├──────────────────────────┬───────────────────────────┤
│  tokio::time::timeout    │  Envuelve un future con   │
│                          │  un plazo máximo          │
├──────────────────────────┼───────────────────────────┤
│  select!                 │  Cancela las ramas que    │
│                          │  no ganan la carrera      │
├──────────────────────────┼───────────────────────────┤
│  drop(future)            │  Cancelación manual       │
│                          │  inmediata                │
├──────────────────────────┼───────────────────────────┤
│  JoinHandle::abort()     │  Cancela una task         │
│                          │  spawneada                │
├──────────────────────────┼───────────────────────────┤
│  CancellationToken       │  Cancelación cooperativa  │
│                          │  multi-task               │
└──────────────────────────┴───────────────────────────┘
```

---

## 2. `tokio::time::timeout`

### 2.1 Uso básico

`timeout` envuelve cualquier `Future` con una duración máxima. Si el future se completa
antes del plazo, retorna `Ok(resultado)`. Si no, retorna `Err(Elapsed)`.

```rust
use tokio::time::{timeout, Duration};

#[tokio::main]
async fn main() {
    // Operación que tarda 200ms con timeout de 500ms → éxito
    let result = timeout(
        Duration::from_millis(500),
        simulated_request(200),
    ).await;

    match result {
        Ok(value) => println!("Success: {}", value),
        Err(_) => println!("Timed out!"),
    }

    // Operación que tarda 1s con timeout de 200ms → timeout
    let result = timeout(
        Duration::from_millis(200),
        simulated_request(1000),
    ).await;

    match result {
        Ok(value) => println!("Success: {}", value),
        Err(elapsed) => println!("Timed out after: {}", elapsed),
    }
}

async fn simulated_request(delay_ms: u64) -> String {
    tokio::time::sleep(Duration::from_millis(delay_ms)).await;
    "response data".to_string()
}
```

La firma de `timeout`:

```rust
pub async fn timeout<F: Future>(
    duration: Duration,
    future: F,
) -> Result<F::Output, Elapsed>
```

### 2.2 Qué pasa internamente

`timeout` no mata el future — lo **dropea** cuando el plazo expira. Esto ejecuta el
destructor del future, que libera sus recursos:

```
timeout(500ms, my_future)

t=0ms     t=200ms     t=400ms     t=500ms
  │         │           │           │
  ├─ poll ──┤           │           │
  │  Pending│           │           │
  │         ├─ poll ────┤           │
  │         │  Pending  │           │
  │         │           ├─ TIMEOUT ─┤
  │         │           │  drop(my_future)
  │         │           │  return Err(Elapsed)
```

### 2.3 Timeout con operaciones de I/O reales

```rust
use tokio::net::TcpStream;
use tokio::io::AsyncWriteExt;
use tokio::time::{timeout, Duration};

async fn connect_with_timeout(addr: &str) -> std::io::Result<TcpStream> {
    match timeout(Duration::from_secs(5), TcpStream::connect(addr)).await {
        Ok(Ok(stream)) => Ok(stream),        // Conectó a tiempo
        Ok(Err(e)) => Err(e),                // Error de conexión (no timeout)
        Err(_) => Err(std::io::Error::new(
            std::io::ErrorKind::TimedOut,
            "connection timed out",
        )),
    }
}

async fn send_with_timeout(
    stream: &mut TcpStream,
    data: &[u8],
    timeout_ms: u64,
) -> std::io::Result<()> {
    timeout(Duration::from_millis(timeout_ms), stream.write_all(data))
        .await
        .map_err(|_| std::io::Error::new(
            std::io::ErrorKind::TimedOut,
            "write timed out",
        ))?
}
```

Observa el doble desempaquetado: `timeout` retorna `Result<Result<T, io::Error>, Elapsed>`.
El `?` exterior maneja el timeout, el interior maneja el error de I/O.

### 2.4 `timeout_at` — Deadline absoluto

A veces necesitas un plazo absoluto en lugar de relativo. `timeout_at` recibe un `Instant`:

```rust
use tokio::time::{timeout_at, Instant, Duration};

#[tokio::main]
async fn main() {
    // "Toda esta operación debe terminar antes de las 3:00pm"
    // (en Tokio, usamos Instant relativo al arranque del runtime)
    let deadline = Instant::now() + Duration::from_secs(10);

    // Múltiples pasos comparten el mismo deadline
    let step1 = timeout_at(deadline, step_one()).await;
    // Si step1 tardó 7s, step2 solo tiene 3s restantes
    let step2 = timeout_at(deadline, step_two()).await;
    // Si step2 tardó 2s, step3 solo tiene 1s restante
    let step3 = timeout_at(deadline, step_three()).await;

    println!("step1: {:?}", step1.is_ok());
    println!("step2: {:?}", step2.is_ok());
    println!("step3: {:?}", step3.is_ok());
}

async fn step_one() -> &'static str {
    tokio::time::sleep(Duration::from_secs(2)).await;
    "one"
}

async fn step_two() -> &'static str {
    tokio::time::sleep(Duration::from_secs(3)).await;
    "two"
}

async fn step_three() -> &'static str {
    tokio::time::sleep(Duration::from_secs(6)).await; // Más que lo restante
    "three"
}
```

```
Deadline absoluto (10s total):
├────── step1 (2s) ──────┤───── step2 (3s) ─────┤── step3 ──┤
0s                       2s                      5s         10s
                                                       ↑
                                                   step3 timeout
                                                   (solo 5s restantes,
                                                    necesitaba 6s)
```

---

## 3. `tokio::time::sleep` y `tokio::time::interval`

### 3.1 `sleep` — Pausa async

`sleep` suspende la task actual por una duración. Es el equivalente async de
`std::thread::sleep`, pero **no bloquea el hilo** — el executor puede ejecutar otras
tasks mientras tanto.

```rust
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    println!("Before sleep");
    sleep(Duration::from_secs(1)).await;
    println!("After sleep"); // 1s después

    // sleep_until: esperar hasta un instante específico
    let deadline = tokio::time::Instant::now() + Duration::from_secs(2);
    tokio::time::sleep_until(deadline).await;
    println!("Deadline reached");
}
```

> **Nunca** uses `std::thread::sleep` en código async — bloquea el hilo del executor
> completo. Usa `tokio::time::sleep` siempre.

### 3.2 `interval` — Ejecución periódica

`interval` produce ticks a intervalos regulares. A diferencia de un loop con `sleep`,
`interval` compensa el drift:

```rust
use tokio::time::{interval, Duration};

#[tokio::main]
async fn main() {
    let mut ticker = interval(Duration::from_millis(100));

    for i in 0..5 {
        ticker.tick().await; // El primer tick retorna inmediatamente
        println!("Tick {} at {:?}", i, tokio::time::Instant::now());
        // Simular trabajo variable
        sleep_random().await;
    }
}

async fn sleep_random() {
    // Simular trabajo que tarda entre 10-50ms
    tokio::time::sleep(Duration::from_millis(30)).await;
}
```

Diferencia entre `interval` y `loop + sleep`:

```
interval(100ms) con trabajo de 30ms:
├── tick ── work(30ms) ──────── tick ── work(30ms) ──────── tick ──
0         30                  100     130                  200
                               ↑ compensa: espera 70ms, no 100ms

loop + sleep(100ms):
├── work(30ms) ── sleep(100ms) ── work(30ms) ── sleep(100ms) ──
0         30                 130        160                260
                               ↑ drift: cada ciclo tarda 130ms
```

### 3.3 Políticas de `MissedTickBehavior`

Cuando un tick se pierde (el trabajo tardó más que el intervalo), `interval` puede
comportarse de tres maneras:

```rust
use tokio::time::{interval, Duration, MissedTickBehavior};

#[tokio::main]
async fn main() {
    let mut ticker = interval(Duration::from_millis(100));

    // Burst: ejecuta los ticks perdidos inmediatamente (default)
    ticker.set_missed_tick_behavior(MissedTickBehavior::Burst);

    // Delay: retrasa el siguiente tick desde ahora
    // ticker.set_missed_tick_behavior(MissedTickBehavior::Delay);

    // Skip: salta ticks perdidos, alinea al siguiente
    // ticker.set_missed_tick_behavior(MissedTickBehavior::Skip);
}
```

```
Intervalo: 100ms, trabajo tardó 250ms

Burst (default):
  tick ── work(250ms) ── tick,tick,tick ── tick ──
  0                     250 (3 ticks inmediatos)  350

Delay:
  tick ── work(250ms) ── tick ── work ── tick ──
  0                     250           350

Skip:
  tick ── work(250ms) ──────── tick ── work ── tick ──
  0                           300           400
                              ↑ alinea al siguiente múltiplo
```

---

## 4. `select!` para cancelación

### 4.1 Cómo `select!` cancela

`tokio::select!` ejecuta múltiples futures concurrentemente y **cancela todos los demás**
cuando el primero completa. La cancelación ocurre por drop — las ramas que no ganaron son
dropeadas.

```rust
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    tokio::select! {
        val = fetch_from_server_a() => {
            println!("Server A responded: {}", val);
            // fetch_from_server_b() fue DROPEADO (cancelado)
        }
        val = fetch_from_server_b() => {
            println!("Server B responded: {}", val);
            // fetch_from_server_a() fue DROPEADO (cancelado)
        }
    }
}

async fn fetch_from_server_a() -> String {
    sleep(Duration::from_millis(200)).await;
    "data from A".to_string()
}

async fn fetch_from_server_b() -> String {
    sleep(Duration::from_millis(100)).await;
    "data from B".to_string()
}
```

```
select! con dos futures:

  fetch_a:  ████████████████████████████████  (200ms)
  fetch_b:  ████████████████  (100ms) ✓ GANA

  t=100ms: fetch_b completa → fetch_a es DROPEADO
           fetch_a nunca llega a 200ms
```

### 4.2 Timeout manual con `select!`

Puedes implementar timeout sin `tokio::time::timeout` usando `select!` + `sleep`:

```rust
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    tokio::select! {
        result = long_operation() => {
            println!("Completed: {}", result);
        }
        _ = sleep(Duration::from_secs(3)) => {
            println!("Operation timed out after 3s");
        }
    }
}

async fn long_operation() -> String {
    sleep(Duration::from_secs(10)).await;
    "done".to_string()
}
```

Esto es funcionalmente equivalente a `timeout(3s, long_operation())`, pero `select!`
es más flexible cuando necesitas más de dos ramas.

### 4.3 `select!` con shutdown signal

Combinar trabajo con una señal de terminación:

```rust
use tokio::sync::watch;
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    let (shutdown_tx, mut shutdown_rx) = watch::channel(false);

    let worker = tokio::spawn(async move {
        let mut count = 0u64;
        loop {
            tokio::select! {
                _ = shutdown_rx.changed() => {
                    if *shutdown_rx.borrow() {
                        println!("Shutdown after {} iterations", count);
                        return count;
                    }
                }
                _ = do_work() => {
                    count += 1;
                }
            }
        }
    });

    // Dejar correr 500ms
    sleep(Duration::from_millis(500)).await;
    shutdown_tx.send(true).unwrap();

    let total = worker.await.unwrap();
    println!("Total work done: {}", total);
}

async fn do_work() {
    sleep(Duration::from_millis(50)).await;
}
```

### 4.4 `select!` en loop con estado

Cuando `select!` corre en un loop, los futures se recrean en cada iteración. Para evitar
esto con futures que deben persistir entre iteraciones, usa `&mut`:

```rust
use tokio::sync::mpsc;
use tokio::time::{sleep, interval, Duration};

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<String>(32);
    let mut heartbeat = interval(Duration::from_secs(1));

    // Simular mensajes
    tokio::spawn(async move {
        for i in 0..5 {
            sleep(Duration::from_millis(300)).await;
            tx.send(format!("msg-{}", i)).await.unwrap();
        }
    });

    let timeout_duration = Duration::from_secs(3);
    let timeout_fut = sleep(timeout_duration);
    // Pin the sleep future so we can poll it across iterations
    tokio::pin!(timeout_fut);

    loop {
        tokio::select! {
            Some(msg) = rx.recv() => {
                println!("Received: {}", msg);
                // Reset timeout: crear nuevo sleep
                timeout_fut.as_mut().reset(
                    tokio::time::Instant::now() + timeout_duration
                );
            }
            _ = heartbeat.tick() => {
                println!("Heartbeat");
            }
            _ = &mut timeout_fut => {
                println!("No messages for 3s, exiting");
                break;
            }
        }
    }
}
```

Puntos clave:
- `tokio::pin!(timeout_fut)` pinea el future en el stack para poder reutilizarlo.
- `&mut timeout_fut` presta el future a `select!` sin consumirlo.
- `.reset()` reinicia el timer sin crear un nuevo future.

### 4.5 `select!` con `biased`

Por defecto, `select!` verifica las ramas en orden aleatorio para evitar starvation.
Con `biased`, las verifica en orden de declaración:

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<u32>(32);

    // Llenar el canal
    for i in 0..10 {
        tx.send(i).await.unwrap();
    }
    drop(tx);

    let mut high_priority = Vec::new();
    let mut low_priority = Vec::new();

    loop {
        tokio::select! {
            biased; // Verificar ramas EN ORDEN

            // Primera rama tiene prioridad
            Some(val) = rx.recv(), if val % 2 == 0 => {
                high_priority.push(val);
            }
            Some(val) = rx.recv() => {
                low_priority.push(val);
            }
            else => break,
        }
    }

    println!("High: {:?}", high_priority);
    println!("Low: {:?}", low_priority);
}
```

> **Nota**: `biased` rompe la equidad (fairness). Úsalo solo cuando realmente necesitas
> prioridad determinista.

---

## 5. Cancelación por drop

### 5.1 Principio fundamental

En Tokio, **dropear un future lo cancela**. No hay API especial de cancelación — es el
sistema de ownership de Rust aplicado a computaciones asíncronas.

```rust
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    let handle = tokio::spawn(async {
        println!("Task started");
        sleep(Duration::from_secs(60)).await;
        println!("This will never print");
    });

    // Esperar un poco
    sleep(Duration::from_millis(100)).await;

    // Cancelar la task
    handle.abort();

    // await retorna Err(JoinError) con is_cancelled() == true
    match handle.await {
        Ok(()) => println!("Completed"),
        Err(e) if e.is_cancelled() => println!("Task was cancelled"),
        Err(e) => println!("Task panicked: {}", e),
    }
}
```

### 5.2 Drop y limpieza de recursos

Cuando un future es dropeado, sus destructores corren sincrónicamente. Esto significa
que los recursos son liberados, pero el código async posterior al punto de suspensión
**nunca se ejecuta**:

```rust
use tokio::time::{sleep, Duration};

struct Resource {
    name: String,
}

impl Drop for Resource {
    fn drop(&mut self) {
        // Esto SÍ se ejecuta cuando la task es cancelada
        println!("Dropping resource: {}", self.name);
    }
}

#[tokio::main]
async fn main() {
    let handle = tokio::spawn(async {
        let _r1 = Resource { name: "db_conn".into() };
        let _r2 = Resource { name: "file_handle".into() };

        sleep(Duration::from_secs(60)).await;
        // ← Si cancelan aquí, r1 y r2 se dropean

        println!("Never reached if cancelled");
    });

    sleep(Duration::from_millis(50)).await;
    handle.abort();
    let _ = handle.await;
    // Output:
    // Dropping resource: file_handle
    // Dropping resource: db_conn
}
```

### 5.3 `JoinSet` y cancelación masiva

`JoinSet` permite cancelar múltiples tasks a la vez:

```rust
use tokio::task::JoinSet;
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    let mut set = JoinSet::new();

    for i in 0..10 {
        set.spawn(async move {
            sleep(Duration::from_secs(i)).await;
            format!("task-{}", i)
        });
    }

    // Esperar solo las primeras 3
    for _ in 0..3 {
        if let Some(result) = set.join_next().await {
            println!("Done: {:?}", result.unwrap());
        }
    }

    // Cancelar todas las restantes
    set.abort_all();
    println!("Aborted {} remaining tasks", set.len());

    // Drenar las canceladas
    while let Some(result) = set.join_next().await {
        match result {
            Ok(val) => println!("Completed: {}", val),
            Err(e) if e.is_cancelled() => {} // Esperado
            Err(e) => eprintln!("Error: {}", e),
        }
    }
}
```

### 5.4 `CancellationToken` — Cancelación cooperativa

`tokio_util::sync::CancellationToken` permite propagar cancelación a través de un árbol
de tasks:

```rust
use tokio_util::sync::CancellationToken;
use tokio::time::{sleep, Duration};

#[tokio::main]
async fn main() {
    let token = CancellationToken::new();

    // Child token: se cancela cuando el padre se cancela
    let child_token = token.child_token();

    let t1 = {
        let token = token.clone();
        tokio::spawn(async move {
            tokio::select! {
                _ = token.cancelled() => {
                    println!("Task 1 cancelled");
                }
                _ = sleep(Duration::from_secs(60)) => {
                    println!("Task 1 done");
                }
            }
        })
    };

    let t2 = tokio::spawn(async move {
        tokio::select! {
            _ = child_token.cancelled() => {
                println!("Task 2 cancelled (via child token)");
            }
            _ = sleep(Duration::from_secs(60)) => {
                println!("Task 2 done");
            }
        }
    });

    // Cancelar todo el árbol
    sleep(Duration::from_millis(100)).await;
    token.cancel();

    let _ = tokio::join!(t1, t2);
}
```

```
CancellationToken tree:
         ┌─────────┐
         │  token   │ ← cancel() aquí
         └────┬────┘
              │
    ┌─────────┴─────────┐
    │                    │
┌───┴───┐          ┌────┴────┐
│ clone │          │ child   │
│ (t1)  │          │ (t2)    │
└───────┘          └─────────┘
    ↓                   ↓
 cancelled()         cancelled()
 se activa           se activa
```

Ventajas sobre `watch(bool)`:
- `child_token()` crea jerarquías de cancelación.
- Cancelar un child no afecta al padre.
- API más ergonómica que `watch::Receiver::changed()`.

> **Nota**: `CancellationToken` está en `tokio-util`, no en `tokio` directamente.
> Añade `tokio-util` a tu `Cargo.toml`.

---

## 6. Cancellation safety

### 6.1 ¿Qué es cancellation safety?

Un future es **cancellation safe** si dropearlo en cualquier punto de `.await` no pierde
datos ni deja estado inconsistente. Esto importa especialmente con `select!`, donde las
ramas que no ganan son dropeadas.

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<u32>(32);

    tokio::spawn(async move {
        for i in 0..10 {
            tx.send(i).await.unwrap();
        }
    });

    loop {
        tokio::select! {
            Some(val) = rx.recv() => {
                // recv() es cancellation safe:
                // si esta rama no gana, no se pierde ningún mensaje
                println!("Got: {}", val);
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(1)) => {
                println!("Timeout");
                break;
            }
        }
    }
}
```

### 6.2 Operaciones cancellation safe vs unsafe

```
┌─────────────────────────────┬──────────────────────────────────┐
│  Cancellation SAFE          │  Cancellation UNSAFE             │
├─────────────────────────────┼──────────────────────────────────┤
│  mpsc::Receiver::recv()     │  AsyncReadExt::read_exact()      │
│  broadcast::Receiver::recv()│  AsyncReadExt::read_to_end()     │
│  oneshot::Receiver (await)  │  AsyncReadExt::read_to_string()  │
│  watch::Receiver::changed() │  BufReader::read_line()          │
│  TcpListener::accept()     │  tokio::io::copy()               │
│  sleep / timeout            │  Future que acumula datos        │
│  UdpSocket::recv_from()     │  internamente entre .awaits      │
└─────────────────────────────┴──────────────────────────────────┘
```

**Regla**: un future es cancellation unsafe si entre dos `.await` internos ha consumido
datos parciales que se perderían si lo dropeas.

### 6.3 Ejemplo de problema con cancellation unsafe

```rust
use tokio::io::{AsyncReadExt, AsyncBufReadExt, BufReader};
use tokio::net::TcpStream;

async fn read_message(stream: &mut BufReader<TcpStream>) -> Option<String> {
    let mut line = String::new();
    // read_line es cancellation UNSAFE:
    // Si select! lo cancela a mitad de lectura, los bytes
    // leídos parcialmente se PIERDEN
    stream.read_line(&mut line).await.ok()?;
    Some(line)
}

// MAL: usar read_line directamente en select!
async fn bad_loop(stream: &mut BufReader<TcpStream>) {
    loop {
        tokio::select! {
            result = read_message(stream) => {
                match result {
                    Some(msg) => println!("Got: {}", msg),
                    None => break,
                }
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(5)) => {
                println!("Timeout");
                // read_message fue cancelado → bytes parciales PERDIDOS
                break;
            }
        }
    }
}
```

### 6.4 Soluciones para cancellation unsafety

**Solución 1**: Mover la operación unsafe fuera de `select!` spawneando una task:

```rust
use tokio::sync::mpsc;
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::net::TcpStream;

async fn safe_reader(
    mut stream: BufReader<TcpStream>,
    tx: mpsc::Sender<String>,
) {
    let mut line = String::new();
    loop {
        line.clear();
        match stream.read_line(&mut line).await {
            Ok(0) => break,    // EOF
            Ok(_) => {
                if tx.send(line.clone()).await.is_err() {
                    break; // Receiver gone
                }
            }
            Err(_) => break,
        }
    }
}

async fn safe_loop(stream: BufReader<TcpStream>) {
    let (tx, mut rx) = mpsc::channel::<String>(32);

    // La lectura corre en su propia task — no se cancela
    tokio::spawn(safe_reader(stream, tx));

    // select! solo toca rx.recv() que ES cancellation safe
    loop {
        tokio::select! {
            Some(msg) = rx.recv() => {
                println!("Got: {}", msg);
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(5)) => {
                println!("Timeout — pero no se perdieron bytes");
                break;
            }
        }
    }
}
```

**Solución 2**: Usar `tokio::pin!` con una variable que persiste entre iteraciones:

```rust
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::net::TcpStream;

async fn reader_with_buffered_line(mut stream: BufReader<TcpStream>) {
    let mut line = String::new();
    // Prepara el future de lectura FUERA del select!
    // y lo reutiliza entre iteraciones

    loop {
        line.clear();
        let read_fut = stream.read_line(&mut line);
        tokio::pin!(read_fut);

        let timed_out = tokio::select! {
            result = &mut read_fut => {
                match result {
                    Ok(0) => return,    // EOF
                    Ok(_) => {
                        println!("Got: {}", line.trim());
                        false
                    }
                    Err(e) => {
                        eprintln!("Error: {}", e);
                        return;
                    }
                }
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(5)) => {
                true
            }
        };

        if timed_out {
            println!("Timeout waiting for data");
            return;
        }
    }
}
```

---

## 7. Patrones de timeout avanzados

### 7.1 Retry con backoff exponencial

```rust
use tokio::time::{sleep, timeout, Duration};

async fn retry_with_backoff<F, Fut, T, E>(
    mut operation: F,
    max_retries: u32,
    initial_delay: Duration,
) -> Result<T, E>
where
    F: FnMut() -> Fut,
    Fut: std::future::Future<Output = Result<T, E>>,
    E: std::fmt::Display,
{
    let mut delay = initial_delay;

    for attempt in 0..=max_retries {
        match operation().await {
            Ok(val) => return Ok(val),
            Err(e) if attempt < max_retries => {
                eprintln!(
                    "Attempt {} failed: {}. Retrying in {:?}",
                    attempt + 1, e, delay
                );
                sleep(delay).await;
                delay *= 2; // Backoff exponencial
            }
            Err(e) => return Err(e),
        }
    }
    unreachable!()
}

#[tokio::main]
async fn main() {
    let mut attempt = 0;

    let result = retry_with_backoff(
        || {
            attempt += 1;
            async move {
                if attempt < 3 {
                    Err(format!("server error on attempt {}", attempt))
                } else {
                    Ok("success!")
                }
            }
        },
        5,
        Duration::from_millis(100),
    ).await;

    println!("Final result: {:?}", result);
}
```

### 7.2 Timeout por operación con deadline global

```rust
use tokio::time::{timeout_at, Instant, Duration};
use std::io;

struct Pipeline {
    deadline: Instant,
}

impl Pipeline {
    fn new(total_timeout: Duration) -> Self {
        Self {
            deadline: Instant::now() + total_timeout,
        }
    }

    fn remaining(&self) -> Duration {
        self.deadline.saturating_duration_since(Instant::now())
    }

    async fn step<F, Fut, T>(&self, name: &str, future: F) -> io::Result<T>
    where
        F: FnOnce() -> Fut,
        Fut: std::future::Future<Output = io::Result<T>>,
    {
        println!(
            "[{}] Starting (remaining: {:?})",
            name,
            self.remaining()
        );

        timeout_at(self.deadline, future())
            .await
            .map_err(|_| io::Error::new(
                io::ErrorKind::TimedOut,
                format!("{} exceeded pipeline deadline", name),
            ))?
    }
}

#[tokio::main]
async fn main() -> io::Result<()> {
    let pipeline = Pipeline::new(Duration::from_secs(5));

    let dns = pipeline.step("DNS resolve", || async {
        tokio::time::sleep(Duration::from_millis(200)).await;
        Ok("93.184.216.34")
    }).await?;

    let conn = pipeline.step("TCP connect", || async {
        tokio::time::sleep(Duration::from_millis(300)).await;
        Ok(format!("connected to {}", dns))
    }).await?;

    let data = pipeline.step("HTTP request", || async {
        tokio::time::sleep(Duration::from_millis(500)).await;
        Ok(format!("data via {}", conn))
    }).await?;

    println!("Pipeline complete: {}", data);
    Ok(())
}
```

### 7.3 Racing con fallback

Intentar la operación rápida primero, caer al fallback si no responde a tiempo:

```rust
use tokio::time::{sleep, timeout, Duration};

async fn fast_cache_lookup(key: &str) -> Option<String> {
    sleep(Duration::from_millis(5)).await;
    if key == "popular" {
        Some("cached_value".into())
    } else {
        None
    }
}

async fn slow_database_query(key: &str) -> String {
    sleep(Duration::from_millis(200)).await;
    format!("db_value_for_{}", key)
}

async fn get_with_cache_timeout(key: &str) -> String {
    // Dar 10ms al cache; si no responde, ir directo a la DB
    match timeout(Duration::from_millis(10), fast_cache_lookup(key)).await {
        Ok(Some(cached)) => {
            println!("Cache hit");
            cached
        }
        Ok(None) => {
            println!("Cache miss, querying DB");
            slow_database_query(key).await
        }
        Err(_) => {
            println!("Cache timeout, querying DB");
            slow_database_query(key).await
        }
    }
}

#[tokio::main]
async fn main() {
    let result = get_with_cache_timeout("popular").await;
    println!("Got: {}", result); // Cache hit

    let result = get_with_cache_timeout("rare").await;
    println!("Got: {}", result); // Cache miss → DB
}
```

### 7.4 Heartbeat / keep-alive timeout

Resetear un timeout cada vez que llega actividad:

```rust
use tokio::sync::mpsc;
use tokio::time::{sleep, Duration, Instant};

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<String>(32);

    // Simular cliente que envía mensajes intermitentes
    tokio::spawn(async move {
        let delays = [100, 200, 150, 300, 2000]; // Último delay > timeout
        for (i, &delay) in delays.iter().enumerate() {
            sleep(Duration::from_millis(delay)).await;
            if tx.send(format!("msg-{}", i)).await.is_err() {
                break;
            }
        }
    });

    let idle_timeout = Duration::from_secs(1);

    loop {
        match timeout(idle_timeout, rx.recv()).await {
            Ok(Some(msg)) => {
                println!(
                    "[{:?}] Received: {}",
                    Instant::now(),
                    msg
                );
                // Timeout se resetea automáticamente en la siguiente
                // iteración porque timeout() crea un nuevo timer
            }
            Ok(None) => {
                println!("Client disconnected");
                break;
            }
            Err(_) => {
                println!("Client idle for {:?}, disconnecting", idle_timeout);
                break;
            }
        }
    }
}
```

---

## 8. Errores comunes

### Error 1: Usar `std::thread::sleep` en código async

```rust
// MAL — bloquea el hilo del executor
async fn handler() {
    std::thread::sleep(std::time::Duration::from_secs(1)); // ← BLOQUEA
    println!("done");
}

// BIEN — usa tokio::time::sleep
async fn handler_fixed() {
    tokio::time::sleep(std::time::Duration::from_secs(1)).await; // ← Suspende
    println!("done");
}
```

**Por qué importa**: `std::thread::sleep` paraliza el hilo del executor. Con el runtime
multi-thread, bloquea 1 de N workers. Con `current_thread`, bloquea **todo**. Si tienes
10 tasks y una usa `std::thread::sleep(5s)`, las otras 9 no avanzan durante 5 segundos.

### Error 2: No manejar el `Result` de `timeout`

```rust
// MAL — timeout silencioso, el programa continúa sin datos
async fn fetch_data() -> String {
    let result = tokio::time::timeout(
        std::time::Duration::from_secs(5),
        make_request(),
    ).await;

    result.unwrap_or_default() // ← Si timeout, retorna "" silenciosamente
}

// BIEN — decidir explícitamente qué hacer ante timeout
async fn fetch_data_fixed() -> Result<String, FetchError> {
    match tokio::time::timeout(
        std::time::Duration::from_secs(5),
        make_request(),
    ).await {
        Ok(Ok(data)) => Ok(data),
        Ok(Err(e)) => Err(FetchError::Network(e)),
        Err(_) => Err(FetchError::Timeout),
    }
}

#[derive(Debug)]
enum FetchError {
    Timeout,
    Network(std::io::Error),
}

async fn make_request() -> std::io::Result<String> {
    Ok("data".into())
}
```

**Por qué importa**: ignorar timeouts silenciosamente oculta problemas de latencia y puede
propagar datos vacíos o defaults incorrectos a través del sistema.

### Error 3: Cancelar una operación que no es cancellation safe en `select!`

```rust
use tokio::io::AsyncReadExt;
use tokio::net::TcpStream;

// MAL — read_exact es cancellation unsafe
async fn bad(stream: &mut TcpStream) {
    let mut buf = [0u8; 1024];
    loop {
        tokio::select! {
            result = stream.read_exact(&mut buf) => {
                // Si leyó 512 de 1024 bytes y el timeout gana,
                // esos 512 bytes se PIERDEN para siempre
                println!("Read {} bytes", result.unwrap());
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(5)) => {
                println!("Timeout");
                // Los bytes parciales están perdidos
                break;
            }
        }
    }
}

// BIEN — proteger con una task separada o usar operaciones safe
async fn good(stream: TcpStream) {
    let (tx, mut rx) = tokio::sync::mpsc::channel::<Vec<u8>>(32);

    // Reader task: nunca es cancelada
    tokio::spawn(async move {
        let mut stream = stream;
        let mut buf = [0u8; 1024];
        loop {
            match stream.read_exact(&mut buf).await {
                Ok(()) => {
                    if tx.send(buf.to_vec()).await.is_err() {
                        break;
                    }
                }
                Err(_) => break,
            }
        }
    });

    // select! solo toca rx.recv() que ES cancellation safe
    loop {
        tokio::select! {
            Some(data) = rx.recv() => {
                println!("Read {} bytes", data.len());
            }
            _ = tokio::time::sleep(std::time::Duration::from_secs(5)) => {
                println!("Timeout — sin pérdida de datos");
                break;
            }
        }
    }
}
```

### Error 4: `interval` sin `tick()` inicial

```rust
use tokio::time::{interval, Duration};

// SORPRESA — el primer tick() retorna INMEDIATAMENTE
async fn unexpected() {
    let mut ticker = interval(Duration::from_secs(60));

    // Esto retorna INMEDIATAMENTE, no después de 60s
    ticker.tick().await;
    println!("First tick is instant!");

    // ESTE espera 60s
    ticker.tick().await;
    println!("Second tick after 60s");
}

// Si quieres que el primer tick también espere:
async fn delayed_start() {
    let mut ticker = interval(Duration::from_secs(60));
    ticker.tick().await; // Consumir el tick inmediato
    // Ahora sí, cada tick espera 60s
    loop {
        ticker.tick().await;
        println!("Tick every 60s");
    }
}
```

**Por qué ocurre**: por diseño, el primer `tick()` retorna inmediatamente para que puedas
ejecutar trabajo al inicio del loop. Si no lo esperas, el comportamiento es confuso.

### Error 5: `abort()` sin esperar el `JoinHandle`

```rust
use tokio::time::{sleep, Duration};

// MAL — la task se aborta pero no esperamos confirmación
async fn fire_and_forget() {
    let handle = tokio::spawn(async {
        // Trabajo con efectos secundarios
        write_to_database().await;
    });

    handle.abort();
    // ¿Ya se abortó? ¿Escribió parcialmente a la DB?
    // No sabemos — no esperamos el handle
}

// BIEN — esperar para confirmar que se abortó
async fn abort_and_confirm() {
    let handle = tokio::spawn(async {
        write_to_database().await;
    });

    handle.abort();

    match handle.await {
        Ok(()) => println!("Completed before abort took effect"),
        Err(e) if e.is_cancelled() => println!("Successfully aborted"),
        Err(e) => println!("Task panicked: {}", e),
    }
}

async fn write_to_database() {
    sleep(Duration::from_secs(1)).await;
}
```

**Por qué importa**: `abort()` marca la task para cancelación, pero la cancelación ocurre
en el próximo `.await` dentro de la task. Si la task ya completó antes de ese punto, `await`
retorna `Ok`. Si no confirmas, no sabes si la operación terminó o fue interrumpida.

---

## 9. Cheatsheet

```text
──────────────────── timeout ─────────────────────
timeout(dur, future).await                  Ok(val) o Err(Elapsed)
timeout_at(instant, future).await           Deadline absoluto
Duration::from_secs(n)                      Duración en segundos
Duration::from_millis(n)                    Duración en milisegundos

──────────────────── sleep ───────────────────────
sleep(duration).await                       Pausa async (NO bloquea hilo)
sleep_until(instant).await                  Esperar hasta instante
Instant::now() + Duration::from_secs(n)     Calcular deadline

──────────────────── interval ────────────────────
interval(duration)                          Crear ticker
ticker.tick().await                         Esperar próximo tick
ticker.reset()                              Reiniciar desde ahora
set_missed_tick_behavior(Burst|Delay|Skip)  Política de ticks perdidos
Primer tick().await retorna inmediatamente  ← Recordar esto

──────────────────── select! ─────────────────────
tokio::select! { a = fut_a => {}, ... }     Primera rama que completa gana
biased; en select!                          Verificar en orden (no random)
_ = sleep(dur) => { /* timeout */ }         Timeout manual con select!
&mut pinned_fut                             Reusar future entre iteraciones
tokio::pin!(fut)                            Pinear future para reutilizar

──────────────────── cancelación ─────────────────
handle.abort()                              Cancelar task spawneada
handle.await → Err(JoinError)               Verificar cancelación
drop(future)                                Cancelación inmediata
JoinSet::abort_all()                        Cancelar múltiples tasks
CancellationToken::new()                    Token cooperativo (tokio-util)
token.cancel()                              Activar cancelación
token.cancelled().await                     Esperar cancelación
token.child_token()                         Token hijo (árbol)

──────────────────── cancellation safety ─────────
mpsc::recv()        ✅ safe                  No pierde mensajes
broadcast::recv()   ✅ safe                  No pierde mensajes
watch::changed()    ✅ safe                  No pierde valores
read_exact()        ❌ unsafe                Pierde bytes parciales
read_line()         ❌ unsafe                Pierde bytes parciales
io::copy()          ❌ unsafe                Pierde datos transferidos
Solución: spawar task o mpsc como intermediario
```

---

## 10. Ejercicios

### Ejercicio 1: Health checker con timeout individual

Implementa un health checker que verifica múltiples servicios concurrentemente, cada uno
con su propio timeout:

```rust
use tokio::time::{timeout, sleep, Duration};
use std::collections::HashMap;

#[derive(Debug)]
enum HealthStatus {
    Healthy,
    Unhealthy(String),
    Timeout,
}

struct ServiceCheck {
    name: String,
    timeout: Duration,
}

async fn check_database() -> Result<(), String> {
    sleep(Duration::from_millis(100)).await;
    Ok(())
}

async fn check_cache() -> Result<(), String> {
    sleep(Duration::from_millis(50)).await;
    Ok(())
}

async fn check_external_api() -> Result<(), String> {
    sleep(Duration::from_millis(2000)).await; // Simular servicio lento
    Ok(())
}

#[tokio::main]
async fn main() {
    // TODO: Definir los checks:
    //   - "database": timeout 500ms, usa check_database()
    //   - "cache": timeout 200ms, usa check_cache()
    //   - "external_api": timeout 300ms, usa check_external_api()

    // TODO: Ejecutar TODOS los checks concurrentemente (no secuencialmente)
    //       Cada uno con su propio timeout.

    // TODO: Recoger los resultados en un HashMap<String, HealthStatus>
    //       e imprimir un resumen.
    //       Expected: database=Healthy, cache=Healthy, external_api=Timeout

    // TODO: ¿Cuánto tarda el health check completo?
    //       (Debería ser ~max(timeouts), no la suma)
}
```

**Pregunta de reflexión**: ¿por qué es importante ejecutar los checks concurrentemente y no
secuencialmente? Si el check de la base de datos tarda 5 segundos en fallar, ¿deberían los
otros checks esperar a que termine antes de ejecutarse?

### Ejercicio 2: Servidor con idle timeout y shutdown

Implementa un servidor simulado que desconecta clientes inactivos y soporta shutdown
graceful:

```rust
use tokio::sync::{mpsc, watch};
use tokio::time::{sleep, timeout, Duration, Instant};

#[tokio::main]
async fn main() {
    let (shutdown_tx, shutdown_rx) = watch::channel(false);
    let (msg_tx, msg_rx) = mpsc::channel::<(u32, String)>(64);

    // TODO: Task "client simulator" que envía mensajes con delays variables:
    //   Client 0: envía cada 200ms (activo)
    //   Client 1: envía una vez y luego calla (idle)
    //   Client 2: envía cada 100ms (muy activo)
    //   Cada mensaje: (client_id, "message text")

    // TODO: Task "connection manager" que:
    //   1. Recibe mensajes del mpsc
    //   2. Trackea el último timestamp de actividad por client_id
    //   3. Cada 500ms (con interval) verifica si algún client no ha
    //      enviado nada en 1 segundo → imprime "Client X idle, disconnecting"
    //   4. Escucha shutdown_rx — cuando se activa, imprime
    //      "Shutdown: disconnecting N active clients" y termina
    //   Usa select! con 3 ramas: recv, interval.tick, shutdown

    // TODO: Después de 3 segundos, enviar shutdown signal.
    //       Esperar a que el connection manager termine.
}
```

**Pregunta de reflexión**: en el connection manager, ¿es importante usar `select!` con
`biased`? ¿Qué pasa si el shutdown signal llega mientras hay mensajes pendientes en el
channel? ¿Se procesan esos mensajes o se pierden?

### Ejercicio 3: Pipeline con deadline global

Implementa un pipeline de 4 etapas con un deadline total compartido. Si cualquier etapa
excede el tiempo restante, el pipeline completo falla:

```rust
use tokio::time::{timeout_at, sleep, Instant, Duration};
use std::io;

#[derive(Debug)]
struct PipelineResult {
    dns_ms: u128,
    connect_ms: u128,
    request_ms: u128,
    parse_ms: u128,
    total_ms: u128,
}

#[tokio::main]
async fn main() {
    // TODO: Implementar run_pipeline(total_timeout: Duration)
    //       que ejecuta 4 etapas secuenciales con deadline compartido:
    //
    //   1. DNS resolve: simular con sleep(150ms), retorna "93.184.216.34"
    //   2. TCP connect: simular con sleep(200ms), retorna connection_id
    //   3. HTTP request: simular con sleep(300ms), retorna response body
    //   4. Parse response: simular con sleep(100ms), retorna parsed data
    //
    //   Todas comparten el mismo deadline (Instant).
    //   Medir cuánto tardó cada etapa.
    //   Si alguna etapa excede el tiempo restante, retorna error
    //   indicando CUÁL etapa falló.

    // Test 1: deadline de 1s → todas completan (150+200+300+100 = 750ms)
    // TODO: Llamar a run_pipeline con 1s e imprimir el resultado

    // Test 2: deadline de 500ms → falla en la etapa 3
    //         (150+200=350ms consumidos, quedan 150ms, etapa 3 necesita 300ms)
    // TODO: Llamar a run_pipeline con 500ms e imprimir el error
}
```

**Pregunta de reflexión**: ¿cuál es la ventaja de usar `timeout_at` con un `Instant`
compartido en vez de `timeout` con duraciones independientes por etapa? ¿Qué pasa si la
etapa 1 tarda más de lo esperado pero la etapa 2 es más rápida — se "recupera" el tiempo?