# Channels: mpsc

## Índice
1. [Message passing vs shared state](#1-message-passing-vs-shared-state)
2. [mpsc::channel (asíncrono)](#2-mpscchannel-asíncrono)
3. [Enviar datos con move](#3-enviar-datos-con-move)
4. [Múltiples productores](#4-múltiples-productores)
5. [Recibir en loop](#5-recibir-en-loop)
6. [mpsc::sync_channel (síncrono)](#6-mpscsync_channel-síncrono)
7. [Patrones con channels](#7-patrones-con-channels)
8. [Channels vs Mutex](#8-channels-vs-mutex)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Message passing vs shared state

Hay dos formas fundamentales de comunicar hilos:

```
Shared state (Arc<Mutex<T>>):
  Hilos acceden al MISMO dato, protegido con locks
  ├── Más familiar (similar a variables globales)
  ├── Propenso a deadlocks
  └── Difícil de razonar con muchos locks

Message passing (channels):
  Hilos ENVÍAN datos de uno a otro, sin compartir
  ├── Sin locks, sin deadlocks (por acceso compartido)
  ├── Ownership se transfiere con el mensaje
  └── Más fácil de razonar (flujo de datos lineal)
```

La filosofía de Go lo resume: *"Don't communicate by sharing
memory; share memory by communicating."* Rust soporta ambos
estilos, pero los channels son idiomáticos para muchos patrones.

```
Shared state:                    Message passing:

  ┌──────────┐                     Hilo A ──msg──▶ Hilo B
  │ Mutex<T> │
  ├──────────┤                     Datos fluyen en una dirección
  │  datos   │◀── Hilo A           Sin estado compartido
  │          │◀── Hilo B           Ownership transferido con el mensaje
  └──────────┘
```

---

## 2. mpsc::channel (asíncrono)

`mpsc` = **M**ultiple **P**roducer, **S**ingle **C**onsumer.
Múltiples hilos pueden enviar, pero solo un hilo recibe:

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    // Crear un channel
    let (tx, rx) = mpsc::channel();
    //  ^^  ^^
    //  │    └── receiver (receptor)
    //  └─── transmitter (emisor)

    // Hilo productor: envía un valor
    thread::spawn(move || {
        tx.send("hello from thread").unwrap();
    });

    // Main thread: recibe el valor
    let msg = rx.recv().unwrap();
    println!("Received: {msg}");
}
```

### ¿Qué son tx y rx?

```
tx: Sender<T>     → puede enviar valores de tipo T
rx: Receiver<T>   → puede recibir valores de tipo T

tx.send(value)    → envía value al channel
rx.recv()         → bloquea hasta recibir un valor
```

### Channel asíncrono (unbounded)

`mpsc::channel()` crea un channel sin límite de capacidad.
`send()` nunca bloquea — los mensajes se acumulan en una cola
interna:

```
tx.send(1) → cola: [1]
tx.send(2) → cola: [1, 2]
tx.send(3) → cola: [1, 2, 3]
                     ▲
rx.recv()  → 1      │ FIFO (primero en entrar, primero en salir)
rx.recv()  → 2
rx.recv()  → 3
```

---

## 3. Enviar datos con move

Cuando envías un valor por el channel, el **ownership se
transfiere** al receptor. El emisor ya no puede usarlo:

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    let (tx, rx) = mpsc::channel();

    thread::spawn(move || {
        let msg = String::from("hello");
        tx.send(msg).unwrap();

        // ERROR: value used after move
        // println!("{msg}");
        // msg fue movido al channel → el receptor es dueño
    });

    let received = rx.recv().unwrap();
    println!("Got: {received}");
    // received es el dueño del String ahora
}
```

### Diagrama de ownership

```
Antes de send:
  Hilo A owns "hello"          Channel: vacío

tx.send(msg):
  Hilo A: msg movido ──────▶  Channel: ["hello"]
  msg ya no existe en A

rx.recv():
  Channel: vacío ────────────▶ Hilo B owns "hello"
```

Esto es fundamentalmente diferente de shared state: no hay
posibilidad de data race porque el dato tiene **un solo dueño**
en todo momento.

### Enviar múltiples valores

```rust
let (tx, rx) = mpsc::channel();

thread::spawn(move || {
    let messages = vec!["hello", "from", "the", "thread"];
    for msg in messages {
        tx.send(msg).unwrap();
        thread::sleep(Duration::from_millis(100));
    }
    // tx se dropea al salir → channel se cierra
});

// Recibir todos los mensajes
for msg in rx {
    println!("Got: {msg}");
}
// El for termina cuando tx se dropea (channel cerrado)
```

---

## 4. Múltiples productores

`Sender<T>` implementa `Clone`. Cada clone puede enviar al
mismo channel:

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    let (tx, rx) = mpsc::channel();
    let mut handles = Vec::new();

    for i in 0..4 {
        let tx = tx.clone();  // clonar el sender para cada hilo
        handles.push(thread::spawn(move || {
            for j in 0..3 {
                tx.send(format!("Worker {i}: message {j}")).unwrap();
            }
            // tx (clone) se dropea al salir del hilo
        }));
    }

    // Dropear el tx original — solo los clones quedan
    drop(tx);

    // Recibir todos los mensajes
    for msg in rx {
        println!("{msg}");
    }
    // El for termina cuando TODOS los tx (clones incluidos) se dropean

    for h in handles {
        h.join().unwrap();
    }
}
```

### ¿Por qué drop(tx)?

El channel se cierra cuando **todos** los `Sender` se dropean.
Si el main thread conserva el `tx` original, el `for msg in rx`
nunca termina:

```
Sin drop(tx):
  main tiene: tx (original)
  hilos tienen: tx (clones)
  Hilos terminan → sus tx se dropean
  Pero main sigue teniendo tx → channel abierto
  → rx.recv() espera indefinidamente → HANG

Con drop(tx):
  main: drop(tx) → ya no tiene sender
  Hilos terminan → sus tx se dropean
  Ningún sender vivo → channel se cierra
  → for msg in rx termina ✓
```

### Diagrama: multiple producers, single consumer

```
          tx.clone()  tx.clone()  tx.clone()
              │           │           │
              ▼           ▼           ▼
          ┌───────┐   ┌───────┐   ┌───────┐
          │Hilo 0 │   │Hilo 1 │   │Hilo 2 │   Productores
          │ send()│   │ send()│   │ send()│
          └───┬───┘   └───┬───┘   └───┬───┘
              │           │           │
              ▼           ▼           ▼
         ┌────────────────────────────────┐
         │        Channel (cola FIFO)     │
         │  [msg0, msg1, msg2, msg0, ...] │
         └──────────────┬─────────────────┘
                        │
                        ▼
                   ┌─────────┐
                   │  Main   │   Consumidor (único)
                   │  recv() │
                   └─────────┘
```

---

## 5. Recibir en loop

### Tres formas de recibir

```rust
let (tx, rx) = mpsc::channel();

// 1. recv() — bloquea hasta recibir
let msg = rx.recv().unwrap();
// Retorna Err si todos los tx se dropearon (channel cerrado)

// 2. try_recv() — no bloquea
match rx.try_recv() {
    Ok(msg)                        => println!("{msg}"),
    Err(mpsc::TryRecvError::Empty) => println!("no messages yet"),
    Err(mpsc::TryRecvError::Disconnected) => println!("channel closed"),
}

// 3. for loop — itera hasta que el channel se cierre
for msg in rx {
    println!("{msg}");
}
// Equivalente a: while let Ok(msg) = rx.recv() { ... }
```

### recv con timeout

```rust
use std::time::Duration;

match rx.recv_timeout(Duration::from_secs(5)) {
    Ok(msg) => println!("{msg}"),
    Err(mpsc::RecvTimeoutError::Timeout) => println!("timeout!"),
    Err(mpsc::RecvTimeoutError::Disconnected) => println!("closed"),
}
```

### Tabla de métodos del receptor

```
Método              Bloquea    Retorna
────────────────────────────────────────────────────
recv()              Sí         Result<T, RecvError>
try_recv()          No         Result<T, TryRecvError>
recv_timeout(dur)   Hasta dur  Result<T, RecvTimeoutError>
iter()              Sí (cada)  Iterator<Item = T>
for msg in rx       Sí (cada)  T (hasta disconnect)
```

---

## 6. mpsc::sync_channel (síncrono)

`sync_channel(bound)` crea un channel con capacidad limitada.
`send()` **bloquea** si el buffer está lleno:

```rust
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

fn main() {
    // Canal con capacidad para 2 mensajes
    let (tx, rx) = mpsc::sync_channel(2);

    thread::spawn(move || {
        tx.send(1).unwrap();  // OK: buffer tiene espacio
        println!("sent 1");
        tx.send(2).unwrap();  // OK: buffer tiene espacio
        println!("sent 2");
        tx.send(3).unwrap();  // BLOQUEA: buffer lleno (2/2)
        println!("sent 3");   // Solo se imprime cuando rx consume algo
    });

    thread::sleep(Duration::from_secs(1));

    println!("receiving...");
    println!("got: {}", rx.recv().unwrap());  // desbloquea send(3)
    println!("got: {}", rx.recv().unwrap());
    println!("got: {}", rx.recv().unwrap());
}
```

### Salida

```
sent 1
sent 2
(pausa de 1 segundo)
receiving...
got: 1
sent 3        ← desbloqueado cuando rx consumió el primer mensaje
got: 2
got: 3
```

### sync_channel(0): rendezvous

Con capacidad 0, el sender se bloquea **hasta que el receiver
llame recv()**. Sender y receiver se sincronizan en cada mensaje:

```rust
let (tx, rx) = mpsc::sync_channel(0);

thread::spawn(move || {
    tx.send("ready").unwrap();
    // Bloqueado hasta que rx.recv() se llame
    // → sincronización punto a punto
});

let msg = rx.recv().unwrap();
// Ambos hilos llegan a este punto simultáneamente
```

### channel vs sync_channel

```
mpsc::channel() — unbounded (asíncrono):
  send() nunca bloquea
  Cola crece sin límite
  Riesgo: memoria infinita si el productor es más rápido
  Bueno para: pipelines donde el consumidor es rápido

mpsc::sync_channel(N) — bounded (síncrono):
  send() bloquea si la cola tiene N mensajes
  Backpressure natural
  El productor se adapta a la velocidad del consumidor
  Bueno para: control de recursos, prevenir OOM

mpsc::sync_channel(0) — rendezvous:
  send() bloquea hasta que recv() se llame
  Sincronización estricta
  Bueno para: handshake entre hilos
```

---

## 7. Patrones con channels

### Pipeline (productor → procesador → consumidor)

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    // Stage 1 → Stage 2 → Stage 3
    let (tx1, rx1) = mpsc::channel();
    let (tx2, rx2) = mpsc::channel();

    // Stage 1: generar datos
    thread::spawn(move || {
        for i in 1..=10 {
            tx1.send(i).unwrap();
        }
    });

    // Stage 2: procesar (filtrar pares, doblar)
    thread::spawn(move || {
        for val in rx1 {
            if val % 2 == 0 {
                tx2.send(val * 2).unwrap();
            }
        }
    });

    // Stage 3: consumir
    for result in rx2 {
        println!("Result: {result}");
    }
    // Output: 4, 8, 12, 16, 20
}
```

```
┌─────────┐     ┌─────────────┐     ┌──────────┐
│ Generar  │────▶│  Filtrar +  │────▶│ Imprimir │
│ 1..=10   │ tx1 │  Doblar     │ tx2 │          │
└─────────┘     └─────────────┘     └──────────┘
```

### Fan-out / Fan-in (distribuir trabajo)

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    let (result_tx, result_rx) = mpsc::channel();
    let mut handles = Vec::new();

    let tasks = vec!["file1.txt", "file2.txt", "file3.txt", "file4.txt"];

    // Fan-out: distribuir tareas a workers
    for task in tasks {
        let result_tx = result_tx.clone();
        handles.push(thread::spawn(move || {
            let result = process_file(task);
            result_tx.send((task, result)).unwrap();
        }));
    }

    drop(result_tx);  // dropear el original

    // Fan-in: recolectar resultados
    for (task, result) in result_rx {
        println!("{task}: {result}");
    }

    for h in handles {
        h.join().unwrap();
    }
}

fn process_file(name: &str) -> usize {
    thread::sleep(std::time::Duration::from_millis(100));
    name.len()  // simular procesamiento
}
```

```
            ┌──────────┐
            │ Worker 0 │──┐
┌───────┐   ├──────────┤  │   ┌────────────┐
│ Tasks │──▶│ Worker 1 │──┼──▶│ Resultados │
│       │   ├──────────┤  │   │  (result_rx)│
└───────┘   │ Worker 2 │──┤   └────────────┘
            ├──────────┤  │
            │ Worker 3 │──┘
            └──────────┘
         Fan-out         Fan-in
```

### Request-response (bidireccional)

Para comunicación bidireccional, usa dos channels:

```rust
use std::sync::mpsc;
use std::thread;

// Request: qué queremos
// Response: el resultado
struct Request {
    input: String,
    response_tx: mpsc::Sender<String>,
}

fn main() {
    let (request_tx, request_rx) = mpsc::channel::<Request>();

    // Worker thread: procesa requests
    thread::spawn(move || {
        for req in request_rx {
            let result = req.input.to_uppercase();
            req.response_tx.send(result).unwrap();
        }
    });

    // Enviar request y esperar response
    let (resp_tx, resp_rx) = mpsc::channel();
    request_tx.send(Request {
        input: "hello".to_string(),
        response_tx: resp_tx,
    }).unwrap();

    let response = resp_rx.recv().unwrap();
    println!("Response: {response}");  // "HELLO"
}
```

### Shutdown con channel

Un channel se cierra al dropear todos los senders. El receiver
ve `Err(RecvError)` o el for loop termina. Esto funciona como
señal de shutdown:

```rust
let (tx, rx) = mpsc::channel();

let worker = thread::spawn(move || {
    for msg in rx {
        process(msg);
    }
    // for termina cuando tx se dropea
    println!("Worker shutting down");
});

// Enviar trabajo
tx.send("task 1").unwrap();
tx.send("task 2").unwrap();

// Shutdown: dropear tx → channel se cierra → worker termina
drop(tx);
worker.join().unwrap();
```

---

## 8. Channels vs Mutex

### Cuándo usar channels

```
✓ Flujo de datos en una dirección (pipeline)
✓ Distribuir trabajo a workers (fan-out)
✓ Recolectar resultados (fan-in)
✓ Comunicación entre componentes desacoplados
✓ Cuando el ownership del dato se transfiere naturalmente
```

### Cuándo usar Mutex

```
✓ Estado compartido que todos leen/escriben (config, cache)
✓ Contador simple (o mejor: AtomicUsize)
✓ Colección que todos modifican (HashMap de sesiones)
✓ Cuando clonar datos para cada mensaje sería costoso
```

### Comparación

```
                     Channel              Arc<Mutex<T>>
                     ───────              ─────────────
Modelo mental        correo postal        pizarra compartida
Ownership            transferido          compartido
Dirección            unidireccional       cualquiera
Datos                se mueven            se copian/referencian
Deadlocks            no (por acceso)      posibles
Backpressure         sync_channel(N)      N/A
Overhead             alloc por mensaje    lock/unlock por acceso
Escalabilidad        excelente            contención con muchos hilos
```

---

## 9. Errores comunes

### Error 1: no dropear el tx original con múltiples productores

```rust
let (tx, rx) = mpsc::channel();

for i in 0..4 {
    let tx = tx.clone();
    thread::spawn(move || { tx.send(i).unwrap(); });
}

// tx original sigue vivo → for nunca termina
for msg in rx {
    println!("{msg}");
}
// HANG: el for espera más mensajes porque tx no fue dropeado
```

**Solución**: dropear tx antes del for loop:

```rust
drop(tx);
for msg in rx {
    println!("{msg}");
}
```

### Error 2: enviar a un channel cerrado

```rust
let (tx, rx) = mpsc::channel();
drop(rx);  // receptor dropeado

tx.send(42).unwrap();
// PANIC: "sending on a closed channel"
// send retorna Err(SendError) si no hay receptor
```

**Solución**: manejar el error de send:

```rust
if tx.send(42).is_err() {
    println!("receiver disconnected, stopping");
    break;
}
```

### Error 3: usar channel para estado compartido

```rust
// MAL: enviar toda la config por channel cada vez que cambia
// Si 10 workers necesitan la config actual, necesitas 10 channels
// o clonar la config 10 veces → ineficiente

// BIEN: usar Arc<RwLock<Config>> para estado compartido
```

Channels son para **transferir** datos, no para **compartir**
estado. Si múltiples hilos necesitan leer el mismo dato
repetidamente, Arc + RwLock/Mutex es más apropiado.

### Error 4: recv bloqueante sin condición de salida

```rust
// MAL: loop infinito si el sender nunca se dropea
loop {
    let msg = rx.recv().unwrap();  // bloquea para siempre si no hay mensajes
    process(msg);
}
```

**Solución**: usar `for msg in rx` (termina al cerrar channel)
o `recv_timeout`:

```rust
// Opción 1: for (termina cuando tx se dropea)
for msg in rx {
    process(msg);
}

// Opción 2: timeout
loop {
    match rx.recv_timeout(Duration::from_secs(5)) {
        Ok(msg) => process(msg),
        Err(RecvTimeoutError::Timeout) => {
            if should_stop() { break; }
        }
        Err(RecvTimeoutError::Disconnected) => break,
    }
}
```

### Error 5: asumir orden entre múltiples productores

```rust
let (tx, rx) = mpsc::channel();

// Hilo A envía: 1, 2, 3
// Hilo B envía: 4, 5, 6
// El receptor puede ver: 1, 4, 2, 5, 3, 6  (entrelazado)
// o: 4, 5, 1, 6, 2, 3  (otro orden)
```

Dentro de **un** productor, el orden FIFO está garantizado.
Entre productores diferentes, el orden depende del scheduling.

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│               Channels (mpsc)                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CREAR:                                                      │
│    let (tx, rx) = mpsc::channel();        unbounded          │
│    let (tx, rx) = mpsc::sync_channel(N);  bounded (cap N)    │
│    let (tx, rx) = mpsc::sync_channel(0);  rendezvous         │
│                                                              │
│  ENVIAR (Sender<T>):                                         │
│    tx.send(value)     → Result (Err si rx dropeado)          │
│    tx.clone()         → nuevo Sender al mismo channel        │
│    drop(tx)           → cierra channel si es el último       │
│                                                              │
│  RECIBIR (Receiver<T>):                                      │
│    rx.recv()          → bloquea hasta mensaje o disconnect   │
│    rx.try_recv()      → no bloquea, retorna Err si vacío    │
│    rx.recv_timeout(d) → bloquea hasta d, retorna Err        │
│    for msg in rx      → itera hasta disconnect              │
│                                                              │
│  MPSC:                                                       │
│    Multiple Producer: tx.clone() para cada hilo              │
│    Single Consumer:  solo UN rx (no clonable)                │
│                                                              │
│  OWNERSHIP:                                                  │
│    send(value) → value se mueve al channel                   │
│    recv() → value se mueve al receptor                       │
│    Nunca hay dos dueños → sin data races                     │
│                                                              │
│  CIERRE:                                                     │
│    Todos los tx dropeados → channel cerrado                  │
│    rx dropeado → send retorna Err                            │
│    for loop termina automáticamente al cierre                │
│                                                              │
│  PATRONES:                                                   │
│    Pipeline:        tx1→rx1 → tx2→rx2 → consumidor          │
│    Fan-out/in:      tasks → N workers → result channel       │
│    Request-response: channel + oneshot response channel      │
│    Shutdown:         drop(tx) → worker sale del for loop     │
│                                                              │
│  ELEGIR:                                                     │
│    channel()          cuando el consumidor es rápido          │
│    sync_channel(N)    cuando quieres backpressure            │
│    sync_channel(0)    cuando quieres sincronización          │
│                                                              │
│  CHANNEL vs MUTEX:                                           │
│    Channel → transferir datos, pipeline, desacoplamiento     │
│    Mutex   → compartir estado, lectura frecuente, cache      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: pipeline de tres etapas

Implementa un pipeline de procesamiento de texto con tres hilos.

**Pasos**:
1. Crea dos channels: `(tx1, rx1)` y `(tx2, rx2)`
2. Hilo 1 (generador): envía las strings
   `["hello world", "rust is great", "channels work"]` por tx1
3. Hilo 2 (procesador): recibe de rx1, convierte a uppercase,
   envía por tx2
4. Main (consumidor): recibe de rx2 e imprime cada resultado
5. Verifica que el pipeline termina limpiamente cuando el
   generador se detiene

**Predicción antes de ejecutar**: ¿el pipeline termina
automáticamente cuando el hilo 1 sale y tx1 se dropea? ¿Qué
pasa en el hilo 2 cuando rx1 se desconecta — su `for val in rx1`
termina? ¿Y eso causa que tx2 se dropee?

> **Pregunta de reflexión**: este pipeline procesa un mensaje
> a la vez. ¿Cuántos mensajes están "en vuelo" como máximo?
> (Pista: con channel unbounded, ¿cuántos puede acumular tx1
> antes de que rx1 los consuma?) ¿Cómo cambiarías el pipeline
> para que como máximo haya 2 mensajes en cada buffer?

### Ejercicio 2: fan-out con N workers

Implementa un pool de workers que procesan tareas enviadas
por un channel.

**Pasos**:
1. Crea un channel `(task_tx, task_rx)` y envuelve rx en
   `Arc<Mutex<Receiver<T>>>` (Receiver no es Clone)
2. Spawn 4 workers que comparten el rx: cada uno hace
   `loop { let task = rx.lock().unwrap().recv() }`
3. El main envía 20 tareas (números) por tx
4. Cada worker calcula el factorial del número y lo imprime
   con su thread id
5. Drop tx y join todos los workers

**Predicción antes de ejecutar**: ¿cada worker procesa
exactamente 5 tareas? ¿O la distribución es desigual? ¿Qué
pasa si un worker tarda más que otro — los demás siguen
tomando tareas? ¿Por qué necesitamos `Arc<Mutex<Receiver>>`?

> **Pregunta de reflexión**: envolver Receiver en Mutex
> funciona, pero cada recv() adquiere el lock. Para un pool
> de workers, ¿sería mejor usar un crate como `crossbeam`
> que ofrece channels multi-consumer (mpmc)? ¿Cuál es la
> ventaja de `crossbeam::channel` sobre `std::sync::mpsc` para
> este patrón? (Pista: Receiver de crossbeam sí es Clone.)

### Ejercicio 3: sync_channel con backpressure

Implementa un productor rápido y un consumidor lento con
control de backpressure.

**Pasos**:
1. Crea `sync_channel(3)` (capacidad 3)
2. Hilo productor: envía 10 mensajes lo más rápido posible,
   imprime cuándo envía cada uno (con timestamp o id)
3. Hilo consumidor: recibe cada mensaje, espera 200ms antes
   del siguiente (simular procesamiento lento)
4. Observa cómo el productor se bloquea después de llenar
   el buffer
5. Repite con `sync_channel(0)` y observa la diferencia

**Predicción antes de ejecutar**: con capacidad 3, ¿cuántos
mensajes envía el productor antes de bloquearse? ¿El productor
envía el 4to mensaje inmediatamente cuando el consumidor lee
el 1ro? ¿Con capacidad 0, el productor y consumidor se
sincronizan en cada mensaje?

> **Pregunta de reflexión**: sync_channel implementa
> backpressure: el productor se ralentiza automáticamente
> al ritmo del consumidor. ¿Qué pasa con channel() unbounded
> si el productor es 100x más rápido? ¿Cuánta memoria
> consumiría? ¿Cuándo preferirías que el productor se bloquee
> (sync_channel) vs que acumule en memoria (channel)?
