# Channels Async: `tokio::sync`

## Índice

1. [Por qué channels async separados](#1-por-qué-channels-async-separados)
2. [mpsc — Multiple Producer Single Consumer](#2-mpsc--multiple-producer-single-consumer)
3. [oneshot — Una sola respuesta](#3-oneshot--una-sola-respuesta)
4. [broadcast — Múltiples consumidores](#4-broadcast--múltiples-consumidores)
5. [watch — Valor observable](#5-watch--valor-observable)
6. [Comparación de los cuatro channels](#6-comparación-de-los-cuatro-channels)
7. [Patrones de composición](#7-patrones-de-composición)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Por qué channels async separados

En `std::sync::mpsc` tenemos channels síncronos: `send()` bloquea el hilo si el buffer está
lleno y `recv()` bloquea mientras no haya datos. Eso funciona con threads del OS, pero dentro
de un runtime async bloquear un hilo es **fatal** — paraliza el executor completo (o al menos
la task que lo ocupa).

`tokio::sync` ofrece cuatro tipos de channel, cada uno optimizado para un patrón de
comunicación diferente:

```
┌─────────────────────────────────────────────────────────┐
│              Channels en tokio::sync                    │
├──────────────┬──────────────────────────────────────────┤
│  mpsc        │  N productores → 1 consumidor            │
│  oneshot     │  1 productor  → 1 consumidor (un valor)  │
│  broadcast   │  1 productor  → N consumidores           │
│  watch       │  1 productor  → N observadores (último)  │
└──────────────┴──────────────────────────────────────────┘
```

Diferencias clave con `std::sync::mpsc`:

| Aspecto               | `std::sync::mpsc`        | `tokio::sync::mpsc`       |
|------------------------|--------------------------|---------------------------|
| `send()`               | Bloquea hilo             | `.await` suspende task    |
| `recv()`               | Bloquea hilo             | `.await` suspende task    |
| Bounded                | `sync_channel(n)`        | `mpsc::channel(n)`        |
| Unbounded              | `mpsc::channel()`        | `mpsc::unbounded_channel` |
| Tipo `Sender`          | `Clone` (multi-producer) | `Clone` (multi-producer)  |
| Cancelación            | No integrada             | Drop cierra el channel    |
| Compatibilidad runtime | Solo threads             | Tokio tasks               |

> **Regla de oro**: dentro de código `async`, usa **siempre** los channels de `tokio::sync`.
> Usar `std::sync::mpsc` dentro de una task async bloquea el hilo del executor.

---

## 2. `mpsc` — Multiple Producer Single Consumer

### 2.1 Channel acotado (bounded)

El channel más usado. Tiene un buffer de capacidad fija: si el buffer está lleno, `send().await`
suspende la task productora hasta que haya espacio.

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    // Buffer de 32 mensajes
    let (tx, mut rx) = mpsc::channel::<String>(32);

    // Productor
    tokio::spawn(async move {
        for i in 0..5 {
            tx.send(format!("msg-{}", i)).await.unwrap();
            println!("Sent msg-{}", i);
        }
        // tx se dropea aquí → cierra el channel
    });

    // Consumidor
    while let Some(msg) = rx.recv().await {
        println!("Received: {}", msg);
    }
    // recv() retorna None cuando todos los tx han sido dropeados
    println!("Channel closed");
}
```

Diagrama del flujo con back-pressure:

```
Productor                Buffer (cap=3)           Consumidor
─────────               ──────────────           ──────────
send("a") ───────────►  [ a |   |   ]
send("b") ───────────►  [ a | b |   ]
send("c") ───────────►  [ a | b | c ]  ◄──────── recv() → "a"
send("d") ───────────►  [ d | b | c ]  ◄──────── recv() → "b"
                         [ d |   | c ]  ◄──────── recv() → "c"
                         [ d |   |   ]  ◄──────── recv() → "d"
                         [   |   |   ]  ◄──────── recv() → None
```

**Back-pressure**: cuando el buffer está lleno, el productor se suspende automáticamente.
Esto protege contra productores más rápidos que consumidores — el sistema se autoregula sin
consumir memoria infinita.

### 2.2 Channel no acotado (unbounded)

Sin límite de buffer. `send()` nunca bloquea (es síncrono, no async). Útil cuando necesitas
enviar desde contexto no-async o cuando garantizas que el consumidor es suficientemente rápido.

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::unbounded_channel::<u32>();

    // send() es síncrono — NO retorna un future
    tx.send(1).unwrap();
    tx.send(2).unwrap();
    tx.send(3).unwrap();

    drop(tx); // Cerrar el channel

    while let Some(val) = rx.recv().await {
        println!("Got: {}", val);
    }
}
```

> **Peligro**: un unbounded channel puede consumir memoria sin límite si el productor
> es más rápido que el consumidor. Prefiere bounded a menos que tengas una razón específica.

### 2.3 Múltiples productores

`Sender` implementa `Clone` — cada clon es un productor independiente:

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<String>(32);

    for id in 0..3 {
        let tx = tx.clone();
        tokio::spawn(async move {
            for i in 0..3 {
                let msg = format!("worker-{}: item-{}", id, i);
                tx.send(msg).await.unwrap();
            }
            // Este clon de tx se dropea aquí
        });
    }

    // IMPORTANTE: dropear el tx original
    // Si no lo hacemos, el channel nunca se cierra
    drop(tx);

    while let Some(msg) = rx.recv().await {
        println!("{}", msg);
    }
    println!("All workers done");
}
```

```
              ┌──────────┐
  tx.clone()──│ Worker 0 │──┐
              └──────────┘  │
              ┌──────────┐  │    ┌────────┐    ┌──────────┐
  tx.clone()──│ Worker 1 │──┼───►│ Buffer │───►│ Receiver │
              └──────────┘  │    └────────┘    └──────────┘
              ┌──────────┐  │
  tx.clone()──│ Worker 2 │──┘
              └──────────┘

  drop(tx) ← El original DEBE dropearse
```

### 2.4 `try_send` y `try_recv`

Variantes no bloqueantes que retornan inmediatamente:

```rust
use tokio::sync::mpsc;
use tokio::sync::mpsc::error::TrySendError;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<i32>(2);

    tx.send(1).await.unwrap();
    tx.send(2).await.unwrap();

    // Buffer lleno — try_send falla inmediatamente
    match tx.try_send(3) {
        Ok(()) => println!("Sent"),
        Err(TrySendError::Full(val)) => {
            println!("Buffer full, couldn't send {}", val);
        }
        Err(TrySendError::Closed(val)) => {
            println!("Channel closed, couldn't send {}", val);
        }
    }

    // try_recv: no bloquea
    match rx.try_recv() {
        Ok(val) => println!("Got {}", val),
        Err(mpsc::error::TryRecvError::Empty) => println!("Nothing yet"),
        Err(mpsc::error::TryRecvError::Disconnected) => println!("Closed"),
    }
}
```

### 2.5 `recv_many` — Recibir en lote

Disponible desde Tokio 1.33. Recibe múltiples mensajes en un solo `Vec`, reduciendo el overhead
de despertar la task repetidamente:

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<u32>(100);

    tokio::spawn(async move {
        for i in 0..50 {
            tx.send(i).await.unwrap();
        }
    });

    let mut buffer = Vec::with_capacity(16);

    // Recibe hasta 16 mensajes a la vez
    // Bloquea solo si el channel está vacío
    while rx.recv_many(&mut buffer, 16).await > 0 {
        println!("Batch of {}: {:?}", buffer.len(), &buffer);
        buffer.clear();
    }
}
```

---

## 3. `oneshot` — Una sola respuesta

Un channel para enviar exactamente **un** valor. Se consume al usarse. Es el patrón
request-response del mundo async.

### 3.1 Uso básico

```rust
use tokio::sync::oneshot;

#[tokio::main]
async fn main() {
    let (tx, rx) = oneshot::channel::<String>();

    tokio::spawn(async move {
        // Simular trabajo
        tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        tx.send("resultado computado".to_string()).unwrap();
        // tx se consume — no puede enviar más
    });

    // rx.await retorna Result<T, RecvError>
    match rx.await {
        Ok(val) => println!("Got: {}", val),
        Err(_) => println!("Sender dropped without sending"),
    }
}
```

Observa que `rx` no tiene `.recv()` — directamente haces `.await` sobre el `Receiver`.
Y `tx.send()` no es async — consume `self` (no es `&self`), así que solo puede llamarse
una vez.

### 3.2 Patrón request-response

El uso más poderoso de `oneshot`: enviar una petición por `mpsc` junto con un canal de
respuesta:

```rust
use tokio::sync::{mpsc, oneshot};

// El mensaje incluye un canal para la respuesta
struct Request {
    query: String,
    respond_to: oneshot::Sender<String>,
}

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<Request>(32);

    // Worker que procesa requests
    tokio::spawn(async move {
        while let Some(req) = rx.recv().await {
            let answer = format!("Result for '{}'", req.query);
            // Responder por el oneshot
            let _ = req.respond_to.send(answer);
        }
    });

    // Cliente envía request y espera respuesta
    let (resp_tx, resp_rx) = oneshot::channel();
    tx.send(Request {
        query: "search term".to_string(),
        respond_to: resp_tx,
    }).await.unwrap();

    let response = resp_rx.await.unwrap();
    println!("Response: {}", response);
}
```

Diagrama del patrón:

```
Cliente                                   Worker
───────                                   ──────
  │                                         │
  │  Request { query, respond_to: tx }      │
  ├────────────── mpsc ───────────────────► │
  │                                         │ procesa query
  │         "resultado"                     │
  │ ◄──────── oneshot ─────────────────────┤
  │                                         │
```

### 3.3 Cancelación con `oneshot`

`oneshot::Sender` tiene un método `is_closed()` que permite detectar si el receptor ya fue
dropeado (la task que esperaba la respuesta fue cancelada):

```rust
use tokio::sync::oneshot;

#[tokio::main]
async fn main() {
    let (tx, rx) = oneshot::channel::<u32>();

    tokio::spawn(async move {
        // Trabajo costoso que queremos cancelar si nadie espera
        for i in 0..100 {
            if tx.is_closed() {
                println!("Receiver gone, aborting at step {}", i);
                return;
            }
            // Simular paso de trabajo
            tokio::time::sleep(std::time::Duration::from_millis(10)).await;
        }
        let _ = tx.send(42);
    });

    // Esperamos solo 50ms y luego cancelamos
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    drop(rx); // Cancelar la espera
    println!("Cancelled the request");

    tokio::time::sleep(std::time::Duration::from_millis(200)).await;
}
```

También puedes usar `closed()` que retorna un `Future` que se completa cuando el receptor
se dropea:

```rust
use tokio::sync::oneshot;

#[tokio::main]
async fn main() {
    let (tx, rx) = oneshot::channel::<String>();

    tokio::spawn(async move {
        tokio::select! {
            _ = tx.closed() => {
                println!("Receiver dropped, cancelling work");
            }
            result = expensive_computation() => {
                let _ = tx.send(result);
            }
        }
    });

    // Simular timeout
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    drop(rx);
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;
}

async fn expensive_computation() -> String {
    tokio::time::sleep(std::time::Duration::from_secs(5)).await;
    "done".to_string()
}
```

---

## 4. `broadcast` — Múltiples consumidores

Cada mensaje enviado es recibido por **todos** los receptores activos. A diferencia de `mpsc`
donde un mensaje va a un solo consumidor, `broadcast` replica el mensaje.

### 4.1 Uso básico

```rust
use tokio::sync::broadcast;

#[tokio::main]
async fn main() {
    // Capacidad del buffer de replay
    let (tx, _rx) = broadcast::channel::<String>(16);

    let mut rx1 = tx.subscribe();
    let mut rx2 = tx.subscribe();

    tx.send("hello".to_string()).unwrap();
    tx.send("world".to_string()).unwrap();

    // Ambos receptores reciben los mismos mensajes
    println!("rx1: {}", rx1.recv().await.unwrap()); // "hello"
    println!("rx2: {}", rx2.recv().await.unwrap()); // "hello"
    println!("rx1: {}", rx1.recv().await.unwrap()); // "world"
    println!("rx2: {}", rx2.recv().await.unwrap()); // "world"
}
```

```
                         ┌──── rx1: recibe "A", "B", "C"
                         │
  tx ──── "A","B","C" ───┤
                         │
                         └──── rx2: recibe "A", "B", "C"
```

### 4.2 Suscripción tardía y `Lagged`

Los nuevos suscriptores solo reciben mensajes **posteriores** a su suscripción. Y si un
receptor es demasiado lento, pierde mensajes del buffer circular:

```rust
use tokio::sync::broadcast;
use tokio::sync::broadcast::error::RecvError;

#[tokio::main]
async fn main() {
    let (tx, _) = broadcast::channel::<u32>(4); // Buffer de solo 4

    let mut rx = tx.subscribe();

    // Enviar 6 mensajes — los primeros 2 serán expulsados del buffer
    for i in 1..=6 {
        tx.send(i).unwrap();
    }

    // El receptor perdió mensajes
    loop {
        match rx.recv().await {
            Ok(val) => println!("Got: {}", val),
            Err(RecvError::Lagged(n)) => {
                println!("Missed {} messages!", n);
                // Continuar recibiendo desde donde está ahora
            }
            Err(RecvError::Closed) => {
                println!("Channel closed");
                break;
            }
        }
    }
}
```

Salida:
```
Missed 2 messages!
Got: 3
Got: 4
Got: 5
Got: 6
```

### 4.3 Patrón de eventos (event bus)

`broadcast` es ideal para publicar eventos que múltiples subsistemas necesitan observar:

```rust
use tokio::sync::broadcast;

#[derive(Clone, Debug)]
enum Event {
    UserConnected(String),
    UserDisconnected(String),
    MessageSent { from: String, text: String },
}

#[tokio::main]
async fn main() {
    let (tx, _) = broadcast::channel::<Event>(64);

    // Logger: observa todos los eventos
    let mut rx_logger = tx.subscribe();
    tokio::spawn(async move {
        while let Ok(event) = rx_logger.recv().await {
            println!("[LOG] {:?}", event);
        }
    });

    // Stats: solo cuenta conexiones
    let mut rx_stats = tx.subscribe();
    tokio::spawn(async move {
        let mut connections = 0i32;
        while let Ok(event) = rx_stats.recv().await {
            match event {
                Event::UserConnected(_) => connections += 1,
                Event::UserDisconnected(_) => connections -= 1,
                _ => {}
            }
            println!("[STATS] Active connections: {}", connections);
        }
    });

    // Publicar eventos
    tx.send(Event::UserConnected("alice".into())).unwrap();
    tx.send(Event::MessageSent {
        from: "alice".into(),
        text: "hi!".into(),
    }).unwrap();
    tx.send(Event::UserConnected("bob".into())).unwrap();
    tx.send(Event::UserDisconnected("alice".into())).unwrap();

    drop(tx);
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;
}
```

> **Requisito**: `T` debe implementar `Clone` porque cada receptor obtiene su propia copia.

### 4.4 `send()` retorna el número de receptores

```rust
use tokio::sync::broadcast;

let (tx, _) = broadcast::channel::<&str>(16);

let _rx1 = tx.subscribe();
let _rx2 = tx.subscribe();

// send() retorna cuántos receptores recibieron el mensaje
let num_receivers = tx.send("hello").unwrap();
println!("Delivered to {} receivers", num_receivers); // 2
```

Si `send()` retorna `Err`, significa que no hay receptores activos.

---

## 5. `watch` — Valor observable

Mantiene **un solo valor** que puede ser observado por múltiples receptores. Los receptores
siempre ven el **último** valor — no hay cola de mensajes. Si el productor actualiza el
valor 10 veces antes de que un receptor lea, el receptor solo ve el último.

### 5.1 Uso básico

```rust
use tokio::sync::watch;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = watch::channel("initial");

    tokio::spawn(async move {
        tokio::time::sleep(std::time::Duration::from_millis(50)).await;
        tx.send("updated").unwrap();

        tokio::time::sleep(std::time::Duration::from_millis(50)).await;
        tx.send("final").unwrap();

        // tx se dropea → notifica a los receptores
    });

    // Leer el valor actual (no async)
    println!("Current: {}", *rx.borrow());

    // Esperar a que cambie
    while rx.changed().await.is_ok() {
        println!("Changed to: {}", *rx.borrow());
    }
    println!("Sender dropped");
}
```

Diferencia fundamental con `broadcast`:

```
broadcast (cola):           watch (último valor):
  msg1 → msg2 → msg3         valor = msg3
  ↑                           ↑
  Receptor lee todos          Receptor ve solo el último
```

### 5.2 `borrow()` y `borrow_and_update()`

- `borrow()` retorna una referencia `Ref<T>` al valor actual. No marca el valor como "visto".
- `borrow_and_update()` retorna la referencia Y marca como visto, para que `changed()` solo
   despierte con valores **posteriores**.

```rust
use tokio::sync::watch;

#[tokio::main]
async fn main() {
    let (tx, mut rx) = watch::channel(0u32);

    tx.send(1).unwrap();

    // borrow() — lee pero NO marca como visto
    println!("borrow: {}", *rx.borrow()); // 1

    // changed() retornará Ok inmediatamente porque
    // no hemos marcado el valor como visto
    rx.changed().await.unwrap();
    println!("Still notified about: {}", *rx.borrow()); // 1

    // borrow_and_update() — lee Y marca como visto
    println!("borrow_and_update: {}", *rx.borrow_and_update()); // 1

    // Ahora changed() esperará a un NUEVO valor
    tx.send(2).unwrap();
    rx.changed().await.unwrap();
    println!("New value: {}", *rx.borrow_and_update()); // 2
}
```

### 5.3 `send_modify` y `send_if_modified`

Para modificar el valor in-place sin clonarlo:

```rust
use tokio::sync::watch;

#[derive(Debug, Clone)]
struct AppState {
    connections: u32,
    requests: u64,
}

#[tokio::main]
async fn main() {
    let (tx, mut rx) = watch::channel(AppState {
        connections: 0,
        requests: 0,
    });

    // Modificar in-place (sin clonar y reemplazar)
    tx.send_modify(|state| {
        state.connections += 1;
    });

    // Solo notificar si realmente cambió
    tx.send_if_modified(|state| {
        if state.connections > 0 {
            state.requests += 1;
            true  // Sí cambió → notificar
        } else {
            false // No cambió → no notificar
        }
    });

    rx.changed().await.unwrap();
    println!("{:?}", *rx.borrow_and_update());
}
```

### 5.4 Patrones típicos de `watch`

**Shutdown signal** — el uso más común:

```rust
use tokio::sync::watch;

#[tokio::main]
async fn main() {
    let (shutdown_tx, shutdown_rx) = watch::channel(false);

    // Múltiples workers reciben la señal
    for id in 0..3 {
        let mut rx = shutdown_rx.clone();
        tokio::spawn(async move {
            loop {
                tokio::select! {
                    _ = rx.changed() => {
                        if *rx.borrow_and_update() {
                            println!("Worker {} shutting down", id);
                            return;
                        }
                    }
                    _ = do_work(id) => {}
                }
            }
        });
    }

    // Dejar que trabajen un poco
    tokio::time::sleep(std::time::Duration::from_millis(200)).await;

    // Señal de shutdown a todos
    shutdown_tx.send(true).unwrap();
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;
}

async fn do_work(id: u32) {
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    println!("Worker {} did work", id);
}
```

**Configuration reload** — actualizar configuración sin reiniciar:

```rust
use tokio::sync::watch;

#[derive(Clone, Debug)]
struct Config {
    max_connections: usize,
    timeout_ms: u64,
    debug: bool,
}

async fn server_loop(mut config_rx: watch::Receiver<Config>) {
    let mut current = config_rx.borrow().clone();

    loop {
        tokio::select! {
            _ = config_rx.changed() => {
                current = config_rx.borrow_and_update().clone();
                println!("Config reloaded: {:?}", current);
            }
            _ = handle_connections(&current) => {}
        }
    }
}

async fn handle_connections(config: &Config) {
    tokio::time::sleep(std::time::Duration::from_millis(config.timeout_ms)).await;
}
```

---

## 6. Comparación de los cuatro channels

```
┌───────────┬─────────┬─────────┬──────────────┬───────────────────┐
│ Channel   │ Senders │ Recvrs  │ Mensajes     │ Caso de uso       │
├───────────┼─────────┼─────────┼──────────────┼───────────────────┤
│ mpsc      │  N      │  1      │ Cola (FIFO)  │ Worker pool,      │
│           │         │         │              │ pipeline, fan-in  │
├───────────┼─────────┼─────────┼──────────────┼───────────────────┤
│ oneshot   │  1      │  1      │ Exacto 1     │ Request-response, │
│           │         │         │              │ resultado único   │
├───────────┼─────────┼─────────┼──────────────┼───────────────────┤
│ broadcast │  1*     │  N      │ Cola (cada   │ Event bus,        │
│           │         │         │ uno recibe   │ pub-sub           │
│           │         │         │ todos)       │                   │
├───────────┼─────────┼─────────┼──────────────┼───────────────────┤
│ watch     │  1      │  N      │ Solo último  │ Config, shutdown, │
│           │         │         │ valor        │ estado compartido │
└───────────┴─────────┴─────────┴──────────────┴───────────────────┘

*broadcast: tx no es Clone, pero subscribe() crea nuevos rx
```

Diagrama de decisión:

```
¿Cuántos consumidores necesitas?
│
├── Uno solo
│   ├── ¿Cuántos mensajes?
│   │   ├── Muchos     → mpsc
│   │   └── Exacto 1  → oneshot
│   │
│   └── ¿Necesitas back-pressure?
│       ├── Sí  → mpsc::channel(cap)     (bounded)
│       └── No  → mpsc::unbounded_channel
│
└── Múltiples
    ├── ¿Todos los mensajes importan?
    │   ├── Sí  → broadcast
    │   └── No, solo el último → watch
    │
    └── ¿T es Clone?
        ├── Sí  → broadcast o watch
        └── No  → mpsc + Arc<T>  (workaround)
```

---

## 7. Patrones de composición

### 7.1 Fan-out con `mpsc` + `oneshot`

Distribuir trabajo a un pool de workers y recolectar resultados:

```rust
use tokio::sync::{mpsc, oneshot};

struct Job {
    data: u64,
    result_tx: oneshot::Sender<u64>,
}

#[tokio::main]
async fn main() {
    let (job_tx, job_rx) = mpsc::channel::<Job>(32);
    let job_rx = std::sync::Arc::new(tokio::sync::Mutex::new(job_rx));

    // Pool de 4 workers que compiten por jobs
    for id in 0..4 {
        let rx = job_rx.clone();
        tokio::spawn(async move {
            loop {
                let job = {
                    let mut rx = rx.lock().await;
                    rx.recv().await
                };
                match job {
                    Some(job) => {
                        let result = job.data * job.data; // Trabajo
                        println!("Worker {} processed {}", id, job.data);
                        let _ = job.result_tx.send(result);
                    }
                    None => break, // Channel cerrado
                }
            }
        });
    }

    // Enviar 10 jobs y recoger resultados
    let mut result_rxs = Vec::new();
    for i in 0..10 {
        let (result_tx, result_rx) = oneshot::channel();
        job_tx.send(Job { data: i, result_tx }).await.unwrap();
        result_rxs.push(result_rx);
    }
    drop(job_tx); // Cerrar el channel de jobs

    // Recoger todos los resultados
    for (i, rx) in result_rxs.into_iter().enumerate() {
        let result = rx.await.unwrap();
        println!("Job {} result: {}", i, result);
    }
}
```

### 7.2 Pipeline con múltiples `mpsc`

Procesar datos en etapas, cada una conectada por un channel:

```rust
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    let (tx1, mut rx1) = mpsc::channel::<String>(32);
    let (tx2, mut rx2) = mpsc::channel::<String>(32);
    let (tx3, mut rx3) = mpsc::channel::<String>(32);

    // Stage 1: leer datos
    tokio::spawn(async move {
        for line in ["hello world", "foo bar", "rust async"] {
            tx1.send(line.to_string()).await.unwrap();
        }
    });

    // Stage 2: convertir a mayúsculas
    tokio::spawn(async move {
        while let Some(s) = rx1.recv().await {
            tx2.send(s.to_uppercase()).await.unwrap();
        }
    });

    // Stage 3: añadir prefijo
    tokio::spawn(async move {
        while let Some(s) = rx2.recv().await {
            tx3.send(format!("[PROCESSED] {}", s)).await.unwrap();
        }
    });

    // Consumir resultado final
    while let Some(result) = rx3.recv().await {
        println!("{}", result);
    }
}
```

```
Stage 1          Stage 2          Stage 3          Output
────────         ────────         ────────         ──────
 read ──mpsc──► uppercase ──mpsc──► prefix ──mpsc──► print

"hello world" → "HELLO WORLD" → "[PROCESSED] HELLO WORLD"
```

### 7.3 `broadcast` + `mpsc` (pub-sub con respuesta)

Publicar un evento y recolectar respuestas de los suscriptores:

```rust
use tokio::sync::{broadcast, mpsc};

#[derive(Clone, Debug)]
struct HealthCheck;

#[derive(Debug)]
struct HealthReport {
    service: String,
    healthy: bool,
}

#[tokio::main]
async fn main() {
    let (check_tx, _) = broadcast::channel::<HealthCheck>(8);
    let (report_tx, mut report_rx) = mpsc::channel::<HealthReport>(32);

    // Servicio "database"
    let mut rx = check_tx.subscribe();
    let rtx = report_tx.clone();
    tokio::spawn(async move {
        while rx.recv().await.is_ok() {
            rtx.send(HealthReport {
                service: "database".into(),
                healthy: true,
            }).await.unwrap();
        }
    });

    // Servicio "cache"
    let mut rx = check_tx.subscribe();
    let rtx = report_tx.clone();
    tokio::spawn(async move {
        while rx.recv().await.is_ok() {
            rtx.send(HealthReport {
                service: "cache".into(),
                healthy: true,
            }).await.unwrap();
        }
    });

    drop(report_tx); // Dropear el original

    // Ejecutar health check
    check_tx.send(HealthCheck).unwrap();
    drop(check_tx);

    // Recoger reportes
    while let Some(report) = report_rx.recv().await {
        println!("{:?}", report);
    }
}
```

### 7.4 `watch` + `select!` para graceful shutdown

Patrón completo con múltiples subsistemas:

```rust
use tokio::sync::{mpsc, watch};

#[tokio::main]
async fn main() {
    let (shutdown_tx, shutdown_rx) = watch::channel(false);
    let (log_tx, mut log_rx) = mpsc::channel::<String>(64);

    // HTTP server task
    let mut sd = shutdown_rx.clone();
    let log = log_tx.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                _ = sd.changed() => {
                    if *sd.borrow() { break; }
                }
                _ = accept_http_request() => {
                    let _ = log.send("Handled HTTP request".into()).await;
                }
            }
        }
        let _ = log.send("HTTP server stopped".into()).await;
    });

    // Background worker task
    let mut sd = shutdown_rx.clone();
    let log = log_tx.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                _ = sd.changed() => {
                    if *sd.borrow() { break; }
                }
                _ = background_job() => {
                    let _ = log.send("Background job done".into()).await;
                }
            }
        }
        let _ = log.send("Background worker stopped".into()).await;
    });

    // Logger (drains until all senders dropped)
    drop(log_tx);
    let log_handle = tokio::spawn(async move {
        while let Some(msg) = log_rx.recv().await {
            println!("[LOG] {}", msg);
        }
    });

    // Dejar que el sistema corra
    tokio::time::sleep(std::time::Duration::from_millis(300)).await;

    // Shutdown graceful
    println!("Initiating shutdown...");
    shutdown_tx.send(true).unwrap();
    drop(shutdown_tx);

    log_handle.await.unwrap();
    println!("Clean shutdown complete");
}

async fn accept_http_request() {
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;
}

async fn background_job() {
    tokio::time::sleep(std::time::Duration::from_millis(150)).await;
}
```

---

## 8. Errores comunes

### Error 1: Olvidar dropear el `Sender` original con `mpsc`

```rust
// MAL — el programa nunca termina
let (tx, mut rx) = mpsc::channel::<i32>(32);

let tx2 = tx.clone();
tokio::spawn(async move {
    tx2.send(1).await.unwrap();
});

// tx original sigue vivo → rx.recv() nunca retorna None
while let Some(v) = rx.recv().await { // ← bloquea para siempre
    println!("{}", v);
}

// BIEN — dropear el tx original
let (tx, mut rx) = mpsc::channel::<i32>(32);

let tx2 = tx.clone();
tokio::spawn(async move {
    tx2.send(1).await.unwrap();
});

drop(tx); // ← Esto permite que rx.recv() retorne None

while let Some(v) = rx.recv().await {
    println!("{}", v);
}
```

**Por qué ocurre**: el channel se cierra cuando **todos** los `Sender` se dropean. Si clonaste
el sender pero no dropeaste el original, queda un sender vivo indefinidamente.

### Error 2: Usar `std::sync::mpsc` en código async

```rust
// MAL — bloquea el hilo del executor
use std::sync::mpsc;

async fn bad_example() {
    let (tx, rx) = mpsc::channel();
    tokio::spawn(async move {
        tx.send(42).unwrap();
    });
    let val = rx.recv().unwrap(); // ← BLOQUEA EL HILO
}

// BIEN — usar tokio::sync::mpsc
use tokio::sync::mpsc;

async fn good_example() {
    let (tx, mut rx) = mpsc::channel(32);
    tokio::spawn(async move {
        tx.send(42).await.unwrap();
    });
    let val = rx.recv().await; // ← Suspende la task, no el hilo
}
```

**Regla**: dentro de `async`, siempre `tokio::sync`. El único caso válido para `std::sync`
es cuando necesitas enviar desde un thread síncrono (y en ese caso usa `unbounded_channel`
ya que su `send()` no es async).

### Error 3: Ignorar `RecvError::Lagged` en `broadcast`

```rust
// MAL — panic inesperado al hacer unwrap
let (tx, _) = broadcast::channel::<u32>(4);
let mut rx = tx.subscribe();

for i in 0..10 {
    tx.send(i).unwrap();
}

let val = rx.recv().await.unwrap(); // ← PANIC: Lagged(6)

// BIEN — manejar el lag
loop {
    match rx.recv().await {
        Ok(val) => println!("Got: {}", val),
        Err(broadcast::error::RecvError::Lagged(n)) => {
            eprintln!("Warning: skipped {} messages", n);
            // Continuar — el receptor se reposiciona automáticamente
        }
        Err(broadcast::error::RecvError::Closed) => break,
    }
}
```

**Por qué ocurre**: `broadcast` tiene buffer circular. Si el receptor es más lento que el
productor, los mensajes viejos se sobrescriben.

### Error 4: Mantener `borrow()` de `watch` a través de `.await`

```rust
// MAL — deadlock potencial
let (tx, rx) = watch::channel(0);

// borrow() retorna un Ref que mantiene un read lock
let val = rx.borrow();
// Si alguien hace tx.send() mientras este Ref existe,
// send() bloquea esperando que el lock se libere
some_async_fn().await; // ← Ref vive a través de .await
drop(val);

// BIEN — clonar el valor o dropear el Ref antes de .await
let val = rx.borrow().clone(); // Clonar y soltar el Ref inmediatamente
some_async_fn().await; // Seguro: no hay lock retenido
println!("{}", val);
```

**Por qué ocurre**: `watch::Ref` internamente mantiene un `RwLock` read guard. Si la task
se suspende con el guard activo, el `Sender` no puede escribir. En el peor caso, el executor
con un solo hilo deadlockea.

### Error 5: Buffer de `broadcast` demasiado pequeño

```rust
// MAL — receptores pierden mensajes constantemente
let (tx, _) = broadcast::channel::<Event>(2); // ← Buffer muy pequeño
// Con N suscriptores lentos, casi todos verán Lagged

// BIEN — dimensionar según la tasa de producción y velocidad del receptor más lento
// Regla práctica: capacidad >= tasa_por_segundo * latencia_máxima_receptor
let (tx, _) = broadcast::channel::<Event>(256);
```

**Por qué ocurre**: a diferencia de `mpsc` donde el productor se suspende, en `broadcast`
el productor **nunca espera**. Los mensajes viejos simplemente se descartan del buffer.
Un buffer pequeño garantiza pérdidas si cualquier receptor es momentáneamente lento.

---

## 9. Cheatsheet

```text
──────────────────────── mpsc ────────────────────────
mpsc::channel::<T>(cap)            Canal acotado (bounded)
mpsc::unbounded_channel::<T>()     Canal sin límite
tx.send(val).await?                Enviar (suspende si lleno)
tx.try_send(val)?                  Enviar sin bloquear
rx.recv().await                    Recibir (None = cerrado)
rx.try_recv()?                     Recibir sin bloquear
rx.recv_many(&mut buf, max).await  Recibir en lote
tx.clone()                         Clonar sender (multi-producer)
drop(tx)                           Cerrar lado escritura

──────────────────────── oneshot ─────────────────────
oneshot::channel::<T>()            Crear canal de un solo uso
tx.send(val)                       Enviar (consume tx, NO async)
rx.await                           Recibir (.await directo)
tx.is_closed()                     ¿Receptor dropeado?
tx.closed().await                  Esperar a que receptor se dropee

──────────────────────── broadcast ───────────────────
broadcast::channel::<T>(cap)       Crear canal broadcast
tx.send(val)?                      Enviar a todos (NO async)
tx.subscribe()                     Crear nuevo receptor
rx.recv().await                    Recibir (maneja Lagged)
rx.try_recv()?                     Recibir sin bloquear
T: Clone                           REQUISITO para el tipo

──────────────────────── watch ───────────────────────
watch::channel(init)               Crear con valor inicial
tx.send(val)?                      Actualizar valor (NO async)
tx.send_modify(|v| ...)            Modificar in-place
tx.send_if_modified(|v| bool)      Modificar solo si cambió
rx.borrow()                        Leer valor (retiene lock)
rx.borrow_and_update()             Leer y marcar como visto
rx.changed().await                 Esperar cambio
rx.clone()                         Clonar receptor

──────────────────────── Decidir cuál usar ───────────
N → 1, muchos mensajes             mpsc
1 → 1, un solo valor               oneshot
1 → N, todos los mensajes          broadcast (T: Clone)
1 → N, solo último valor           watch
Desde código síncrono              unbounded_channel o watch
Request-response                   mpsc + oneshot
Graceful shutdown                  watch(bool) o watch(())
Event bus                          broadcast
Config reload                      watch
```

---

## 10. Ejercicios

### Ejercicio 1: Logger centralizado con `mpsc`

Implementa un sistema donde múltiples tasks envían mensajes de log a un logger centralizado:

```rust
use tokio::sync::mpsc;
use tokio::time::{sleep, Duration};

#[derive(Debug)]
enum LogLevel {
    Info,
    Warn,
    Error,
}

struct LogEntry {
    level: LogLevel,
    source: String,
    message: String,
}

#[tokio::main]
async fn main() {
    let (tx, mut rx) = mpsc::channel::<LogEntry>(64);

    // TODO: Crear 3 tasks "servicios" que envíen logs periódicamente:
    //   - "http-server": envía Info cada 50ms
    //   - "database": envía Warn cada 80ms
    //   - "auth": envía Error cada 120ms
    // Cada task envía 5 logs y termina.
    // No olvides dropear el tx original.

    // TODO: Task logger que recibe todos los mensajes
    // y los imprime con formato: [LEVEL] (source) message
    // Debe terminar cuando el channel se cierre.

    // TODO: Esperar a que el logger termine.
}
```

**Pregunta de reflexión**: si cambias la capacidad del channel de 64 a 1, ¿qué efecto tiene
en el comportamiento del programa? ¿Algún servicio se bloqueará? ¿Se perderán mensajes?

### Ejercicio 2: Cache con invalidación usando `watch`

Implementa un cache que recarga sus datos cuando recibe una señal de invalidación:

```rust
use tokio::sync::watch;
use std::collections::HashMap;

type CacheData = HashMap<String, String>;

async fn load_from_database() -> CacheData {
    // Simular carga de BD
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    let mut data = HashMap::new();
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis();
    data.insert("key1".into(), format!("value1@{}", timestamp));
    data.insert("key2".into(), format!("value2@{}", timestamp));
    data
}

#[tokio::main]
async fn main() {
    // TODO: Crear un watch::channel<CacheData> con datos iniciales
    //       (carga load_from_database().await como valor inicial)

    // TODO: Task "invalidator" que envía nuevos datos
    //       cada 200ms llamando a load_from_database()
    //       Usa send_modify o send para actualizar.
    //       Después de 5 recargas, termina.

    // TODO: 3 tasks "readers" que observan cambios con changed().await
    //       y al recibir cambio, imprimen cuántos entries tiene el cache.
    //       Deben terminar cuando el sender se dropee.
    //       CUIDADO: no mantener borrow() a través de .await

    // TODO: Esperar a que todo termine.
}
```

**Pregunta de reflexión**: ¿qué pasa si un reader es muy lento y el invalidator actualiza
tres veces antes de que el reader vuelva a llamar `changed()`? ¿Se pierden datos? Compara
este comportamiento con lo que pasaría usando `broadcast` en vez de `watch`.

### Ejercicio 3: Request-response con timeout

Implementa un sistema actor-style donde un "servicio" procesa requests con timeout:

```rust
use tokio::sync::{mpsc, oneshot};
use tokio::time::{timeout, Duration};

enum Command {
    Get {
        key: String,
        respond_to: oneshot::Sender<Option<String>>,
    },
    Set {
        key: String,
        value: String,
        respond_to: oneshot::Sender<bool>,
    },
}

#[tokio::main]
async fn main() {
    // TODO: Crear un mpsc channel para enviar Commands al servicio

    // TODO: Task "key-value service" que mantiene un HashMap<String, String>
    //       y procesa Commands en un loop:
    //       - Get: responde con el valor (o None)
    //       - Set: inserta y responde true
    //       Si el oneshot respond_to ya está cerrado, simplemente ignorar.

    // TODO: Función helper async que envía un Get y espera la respuesta
    //       con un timeout de 100ms. Si el timeout expira, retorna
    //       Err("timeout"). Firma sugerida:
    //   async fn get(tx: &mpsc::Sender<Command>, key: &str)
    //       -> Result<Option<String>, &'static str>

    // TODO: Función helper async para Set con timeout de 100ms.

    // TODO: Usar las funciones helper para:
    //   1. Set("name", "Rust")
    //   2. Set("version", "1.75")
    //   3. Get("name") → imprimir resultado
    //   4. Get("missing") → imprimir resultado
    //   5. Cerrar el channel y hacer un Get — debe fallar

    // TODO: ¿Qué error recibes al hacer Get después de cerrar el channel?
}
```

**Pregunta de reflexión**: en el servicio, ¿por qué es importante verificar si el `oneshot`
`respond_to` ya está cerrado antes de enviar la respuesta? ¿Qué pasa si el cliente que hizo
el request fue cancelado (su task dropeada) mientras el servicio procesaba el command?