# Servidor TCP con Tokio

## Índice

1. [De std::net a tokio::net](#1-de-stdnet-a-tokionet)
2. [tokio::net::TcpListener](#2-tokionettcplistener)
3. [tokio::net::TcpStream](#3-tokionettcpstream)
4. [Spawn por conexión](#4-spawn-por-conexión)
5. [Estado compartido async](#5-estado-compartido-async)
6. [Graceful shutdown async](#6-graceful-shutdown-async)
7. [Patrones de servidor completo](#7-patrones-de-servidor-completo)
8. [Rendimiento: threads vs async](#8-rendimiento-threads-vs-async)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. De std::net a tokio::net

En la sección anterior construimos servidores multi-cliente con `std::net` usando threads. Esa
aproximación funciona, pero tiene un límite fundamental: cada conexión consume un thread del
sistema operativo, con su stack de ~8 MB. Con async, cada conexión es una **task** que ocupa
~1-10 KB, permitiendo manejar decenas de miles de conexiones simultáneas en un solo proceso.

### Comparación arquitectónica

```
┌─────────────── Modelo con threads ───────────────┐
│                                                   │
│  SO Scheduler                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐          │
│  │ Thread 1 │ │ Thread 2 │ │ Thread N │  ...      │
│  │ 8 MB     │ │ 8 MB     │ │ 8 MB     │          │
│  │ Cliente1 │ │ Cliente2 │ │ ClienteN │          │
│  └──────────┘ └──────────┘ └──────────┘          │
│                                                   │
│  Límite práctico: ~1,000-10,000 conexiones        │
└───────────────────────────────────────────────────┘

┌─────────────── Modelo async ─────────────────────┐
│                                                   │
│  Tokio Runtime (pocos threads del SO)             │
│  ┌────────────────────────────────────────┐       │
│  │  Thread 1      Thread 2      Thread M  │       │
│  │  ┌─────┐       ┌─────┐      ┌─────┐   │       │
│  │  │task1│       │task4│      │task7│   │       │
│  │  │task2│       │task5│      │task8│   │       │
│  │  │task3│       │task6│      │ ... │   │       │
│  │  └─────┘       └─────┘      └─────┘   │       │
│  │  ~1KB c/u      ~1KB c/u    ~1KB c/u   │       │
│  └────────────────────────────────────────┘       │
│                                                   │
│  Límite práctico: ~100,000+ conexiones            │
└───────────────────────────────────────────────────┘
```

### Migración conceptual

| `std::net`                    | `tokio::net`                    |
|-------------------------------|---------------------------------|
| `TcpListener::bind(addr)`    | `TcpListener::bind(addr).await` |
| `listener.accept()`          | `listener.accept().await`       |
| `stream.read(&mut buf)`      | `stream.read(&mut buf).await`   |
| `stream.write_all(data)`     | `stream.write_all(data).await`  |
| `std::thread::spawn(move     | `tokio::spawn(async move        |
| \|\| { ... })`                | { ... })`                       |
| `std::io::BufReader`         | `tokio::io::BufReader`          |
| `std::io::copy`              | `tokio::io::copy`               |

La API es casi idéntica — la diferencia es que cada operación de I/O retorna un `Future` que
debes `.await`.

---

## 2. tokio::net::TcpListener

### Bind y accept básico

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Server listening on 127.0.0.1:8080");

    loop {
        // accept() retorna (TcpStream, SocketAddr)
        let (stream, addr) = listener.accept().await?;
        println!("New connection from: {}", addr);

        // Procesar stream...
    }
}
```

`bind()` es async porque internamente registra el socket en el reactor (epoll/kqueue/IOCP)
de Tokio. `accept()` suspende la task actual hasta que llega una conexión — **no bloquea el
thread**, permitiendo que otras tasks se ejecuten mientras tanto.

### Diferencias con std::net::TcpListener

```rust
// std::net — bloquea el thread del SO
let listener = std::net::TcpListener::bind("127.0.0.1:8080")?;
let (stream, addr) = listener.accept()?;  // Thread bloqueado aquí

// tokio::net — suspende la task, libera el thread
let listener = TcpListener::bind("127.0.0.1:8080").await?;
let (stream, addr) = listener.accept().await?;  // Task suspendida, thread libre
```

### Bind a múltiples direcciones

Tokio no soporta bind a múltiples direcciones con un solo listener. Si necesitas escuchar
en varias, crea múltiples listeners:

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener_v4 = TcpListener::bind("0.0.0.0:8080").await?;
    let listener_v6 = TcpListener::bind("[::]:8080").await?;

    loop {
        tokio::select! {
            result = listener_v4.accept() => {
                let (stream, addr) = result?;
                println!("IPv4 connection from {}", addr);
                // handle stream...
            }
            result = listener_v6.accept() => {
                let (stream, addr) = result?;
                println!("IPv6 connection from {}", addr);
                // handle stream...
            }
        }
    }
}
```

### Puerto asignado por el SO

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Puerto 0: el SO asigna uno libre
    let listener = TcpListener::bind("127.0.0.1:0").await?;
    let local_addr = listener.local_addr()?;
    println!("Listening on: {}", local_addr);  // e.g., 127.0.0.1:54321

    Ok(())
}
```

Esto es especialmente útil en tests donde múltiples servidores pueden correr en paralelo sin
conflictos de puerto.

---

## 3. tokio::net::TcpStream

### AsyncRead y AsyncWrite

`tokio::net::TcpStream` implementa `AsyncRead` y `AsyncWrite`, los equivalentes async de
`Read` y `Write` de la librería estándar. Estos traits están definidos en `tokio::io`.

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn handle_connection(mut stream: TcpStream) -> tokio::io::Result<()> {
    // Leer datos
    let mut buf = [0u8; 1024];
    let n = stream.read(&mut buf).await?;

    if n == 0 {
        // Conexión cerrada por el cliente
        return Ok(());
    }

    println!("Received: {:?}", &buf[..n]);

    // Escribir respuesta
    stream.write_all(b"ACK\n").await?;

    // Flush explícito (necesario con BufWriter)
    stream.flush().await?;

    Ok(())
}
```

### Lectura completa con read_to_end y read_to_string

```rust
use tokio::io::AsyncReadExt;
use tokio::net::TcpStream;

async fn read_all(mut stream: TcpStream) -> tokio::io::Result<String> {
    let mut contents = String::new();
    // Lee hasta EOF — cuidado con clientes que nunca cierran
    stream.read_to_string(&mut contents).await?;
    Ok(contents)
}
```

> **Advertencia**: `read_to_string` y `read_to_end` esperan hasta que el otro lado cierre la
> conexión (EOF). Si el cliente mantiene la conexión abierta, tu servidor se queda esperando
> indefinidamente. Usa timeouts o protocolos con delimitadores.

### BufReader y BufWriter async

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader, BufWriter};

async fn line_echo(stream: TcpStream) -> tokio::io::Result<()> {
    let (reader, writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut writer = BufWriter::new(writer);

    let mut line = String::new();

    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;

        if n == 0 {
            break;  // EOF
        }

        writer.write_all(line.as_bytes()).await?;
        writer.flush().await?;
    }

    Ok(())
}
```

### Split vs into_split

Tokio ofrece dos formas de dividir un `TcpStream` en mitades de lectura y escritura:

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn example(stream: TcpStream) {
    // into_split(): consume el stream, produce OwnedReadHalf + OwnedWriteHalf
    // Las mitades son 'static, pueden enviarse a distintos spawns
    let (read_half, write_half) = stream.into_split();

    // split(): toma &mut, produce ReadHalf<'_> + WriteHalf<'_>
    // Las mitades tienen lifetime ligado al stream original
    // Útil cuando no necesitas moverlas entre tasks
    // let (read_half, write_half) = stream.split();
}
```

```
┌──────── TcpStream ────────┐
│    fd del socket           │
│                            │
│  .split() ─────────────────┤──→ ReadHalf<'_>  + WriteHalf<'_>
│  (borrow, zero-cost)       │    (ligadas al lifetime del stream)
│                            │
│  .into_split() ────────────┤──→ OwnedReadHalf + OwnedWriteHalf
│  (consume, Arc interno)    │    ('static, movibles entre tasks)
└────────────────────────────┘
```

Usa `into_split()` cuando necesitas enviar cada mitad a un `tokio::spawn` diferente
(lectura y escritura concurrentes en tasks separadas). Usa `split()` cuando trabajas dentro
de una misma task y quieres evitar el overhead del `Arc`.

### tokio::io::copy — el echo más simple

```rust
use tokio::net::TcpStream;
use tokio::io;

async fn echo(mut stream: TcpStream) -> io::Result<()> {
    let (mut reader, mut writer) = stream.split();
    io::copy(&mut reader, &mut writer).await?;
    Ok(())
}
```

`io::copy` lee del reader y escribe al writer de forma async, con buffer interno.
Perfecto para proxies y echo servers.

---

## 4. Spawn por conexión

Este es el patrón fundamental de servidores async: un `tokio::spawn` por cada conexión
entrante. Cada conexión se convierte en una **task** independiente dentro del runtime.

### Patrón básico

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Echo server on 127.0.0.1:8080");

    loop {
        let (mut stream, addr) = listener.accept().await?;

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            let mut buf = [0u8; 1024];

            loop {
                let n = match stream.read(&mut buf).await {
                    Ok(0) => {
                        println!("[{}] disconnected", addr);
                        return;
                    }
                    Ok(n) => n,
                    Err(e) => {
                        eprintln!("[{}] read error: {}", addr, e);
                        return;
                    }
                };

                if let Err(e) = stream.write_all(&buf[..n]).await {
                    eprintln!("[{}] write error: {}", addr, e);
                    return;
                }
            }
        });
    }
}
```

### ¿Por qué no necesitamos thread pool explícito?

Con `std::net` necesitábamos un thread pool para limitar el número de threads. Con Tokio,
el runtime ya es un pool de threads (por defecto, tantos como CPUs) que ejecuta miles de
tasks cooperativamente:

```
┌─────────── Con threads (sección anterior) ───────────┐
│                                                       │
│  accept() ──→ ThreadPool::execute(|| handle(stream))  │
│                                                       │
│  Si el pool tiene 8 threads y llegan 100 clientes:    │
│  8 activos + 92 en cola esperando                     │
└───────────────────────────────────────────────────────┘

┌─────────── Con async (esta sección) ─────────────────┐
│                                                       │
│  accept() ──→ tokio::spawn(async { handle(stream) })  │
│                                                       │
│  Si llegan 100 clientes:                              │
│  100 tasks activas, multiplexadas en ~8 threads       │
│  Todas progresan concurrentemente                     │
└───────────────────────────────────────────────────────┘
```

Las tasks son **cooperativas**: cuando una task hace `.await` (esperando I/O), el runtime
ejecuta otra task en el mismo thread. No hay contexto switch del SO, no hay stack de 8 MB
por task.

### Manejo de errores en tasks spawneadas

`tokio::spawn` retorna un `JoinHandle<T>`. Si la task hace panic, el panic se captura y se
propaga cuando haces `.await` en el `JoinHandle`:

```rust
use tokio::net::{TcpListener, TcpStream};

async fn handle_client(stream: TcpStream) -> Result<(), Box<dyn std::error::Error + Send>> {
    // Lógica del cliente...
    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;

    loop {
        let (stream, addr) = listener.accept().await?;

        // Opción 1: fire-and-forget (ignorar resultado)
        tokio::spawn(async move {
            if let Err(e) = handle_client(stream).await {
                eprintln!("[{}] error: {}", addr, e);
            }
        });

        // Opción 2: monitorear resultado (menos común en servidores)
        // let handle = tokio::spawn(async move { handle_client(stream).await });
        // match handle.await {
        //     Ok(Ok(())) => println!("Client done"),
        //     Ok(Err(e)) => eprintln!("Client error: {}", e),
        //     Err(e) => eprintln!("Task panicked: {}", e),
        // }
    }
}
```

En servidores, generalmente se usa fire-and-forget: no queremos que el loop de accept se
detenga esperando que termine un cliente. Los errores se loggean dentro de la task.

### Requisito 'static para tasks

`tokio::spawn` requiere que el future sea `'static` — no puede tomar referencias a datos
locales del loop de accept. Todo debe moverse al closure con `move`:

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let server_name = String::from("MyServer");

    loop {
        let (stream, addr) = listener.accept().await?;

        // ERROR: server_name se movería en la primera iteración
        // tokio::spawn(async move {
        //     println!("{}: client {}", server_name, addr);
        // });

        // CORRECTO: clonar antes de mover
        let name = server_name.clone();
        tokio::spawn(async move {
            println!("{}: client {}", name, addr);
            // usar stream...
        });
    }
}
```

---

## 5. Estado compartido async

Igual que con threads, los servidores async necesitan compartir estado entre conexiones.
La diferencia es que debemos tener cuidado con los `.await` dentro de secciones protegidas
por mutex.

### Arc<Mutex<T>> con tokio::sync::Mutex

```rust
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

type SharedState = Arc<Mutex<HashMap<String, String>>>;

async fn handle_client(stream: TcpStream, state: SharedState) -> tokio::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;
        if n == 0 { break; }

        let trimmed = line.trim();

        if let Some((key, value)) = trimmed.split_once('=') {
            // SET: key=value
            let mut map = state.lock().await;
            map.insert(key.to_string(), value.to_string());
            writer.write_all(b"OK\n").await?;
        } else {
            // GET: key
            let map = state.lock().await;
            let response = match map.get(trimmed) {
                Some(v) => format!("{}\n", v),
                None => "NOT_FOUND\n".to_string(),
            };
            writer.write_all(response.as_bytes()).await?;
        }
    }

    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let state: SharedState = Arc::new(Mutex::new(HashMap::new()));

    loop {
        let (stream, addr) = listener.accept().await?;
        let state = Arc::clone(&state);

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            if let Err(e) = handle_client(stream, state).await {
                eprintln!("[{}] error: {}", addr, e);
            }
            println!("[{}] disconnected", addr);
        });
    }
}
```

### tokio::sync::Mutex vs std::sync::Mutex

Esta es una decisión importante en código async:

```
┌─────────── std::sync::Mutex ──────────────────────┐
│                                                    │
│  • Bloquea el thread del SO mientras espera        │
│  • MUY rápido si el lock dura poco                 │
│  • NO se puede mantener a través de un .await      │
│  • Ideal para: accesos rápidos sin I/O             │
│                                                    │
│  let mut map = state.lock().unwrap();              │
│  map.insert(key, value);  // rápido, sin await     │
│  // map se dropea aquí, lock liberado              │
│                                                    │
└────────────────────────────────────────────────────┘

┌─────────── tokio::sync::Mutex ────────────────────┐
│                                                    │
│  • Suspende la task (no bloquea thread)            │
│  • Más lento por overhead de scheduling            │
│  • SÍ se puede mantener a través de un .await      │
│  • Ideal para: locks que cubren operaciones async  │
│                                                    │
│  let mut map = state.lock().await;                 │
│  map.insert(key, fetch_value().await);  // ← await │
│  // map se dropea aquí                             │
│                                                    │
└────────────────────────────────────────────────────┘
```

**Regla práctica**: usa `std::sync::Mutex` por defecto. Solo usa `tokio::sync::Mutex`
cuando necesites hacer `.await` mientras mantienes el lock.

```rust
use std::collections::HashMap;
use std::sync::{Arc, Mutex};  // std, no tokio
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let state = Arc::new(Mutex::new(HashMap::<String, String>::new()));

    loop {
        let (stream, _addr) = listener.accept().await?;
        let state = Arc::clone(&state);

        tokio::spawn(async move {
            // Acceso rápido, sin .await dentro del lock
            {
                let mut map = state.lock().unwrap();
                map.insert("last_connected".to_string(), "now".to_string());
            }  // lock se libera aquí, ANTES de cualquier .await

            // Ahora sí podemos hacer I/O async
            // handle_stream(stream).await;
        });
    }
}
```

### RwLock para lecturas frecuentes

```rust
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

type Config = Arc<RwLock<HashMap<String, String>>>;

async fn handle_client(stream: TcpStream, config: Config) -> tokio::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        line.clear();
        if reader.read_line(&mut line).await? == 0 { break; }

        let key = line.trim();

        // Muchas tasks pueden leer simultáneamente
        let config_read = config.read().await;
        let response = match config_read.get(key) {
            Some(v) => format!("{}={}\n", key, v),
            None => format!("{}: not configured\n", key),
        };
        drop(config_read);  // Liberar antes de I/O

        writer.write_all(response.as_bytes()).await?;
    }

    Ok(())
}
```

### Contadores atómicos

Para métricas simples, los atómicos son la opción más eficiente — no necesitan lock:

```rust
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio::net::TcpListener;

struct ServerMetrics {
    active_connections: AtomicUsize,
    total_connections: AtomicUsize,
    bytes_received: AtomicUsize,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let metrics = Arc::new(ServerMetrics {
        active_connections: AtomicUsize::new(0),
        total_connections: AtomicUsize::new(0),
        bytes_received: AtomicUsize::new(0),
    });

    // Task para reportar métricas cada 10 segundos
    let m = Arc::clone(&metrics);
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(std::time::Duration::from_secs(10));
        loop {
            interval.tick().await;
            println!(
                "active={} total={} bytes={}",
                m.active_connections.load(Ordering::Relaxed),
                m.total_connections.load(Ordering::Relaxed),
                m.bytes_received.load(Ordering::Relaxed),
            );
        }
    });

    loop {
        let (stream, addr) = listener.accept().await?;
        let metrics = Arc::clone(&metrics);

        metrics.active_connections.fetch_add(1, Ordering::Relaxed);
        metrics.total_connections.fetch_add(1, Ordering::Relaxed);

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            // handle_client(stream, &metrics).await;
            let _ = stream;

            metrics.active_connections.fetch_sub(1, Ordering::Relaxed);
            println!("[{}] disconnected", addr);
        });
    }
}
```

### DashMap para concurrencia sin locks explícitos

El crate `dashmap` ofrece un `HashMap` concurrente que evita el patrón `Arc<Mutex<HashMap>>`:

```rust
use dashmap::DashMap;
use std::sync::Arc;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

// Cargo.toml: dashmap = "6"

type SharedMap = Arc<DashMap<String, String>>;

async fn handle_client(stream: TcpStream, map: SharedMap) -> tokio::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        line.clear();
        if reader.read_line(&mut line).await? == 0 { break; }

        let parts: Vec<&str> = line.trim().splitn(3, ' ').collect();
        match parts.as_slice() {
            ["SET", key, value] => {
                map.insert(key.to_string(), value.to_string());
                writer.write_all(b"+OK\r\n").await?;
            }
            ["GET", key] => {
                let response = match map.get(*key) {
                    Some(v) => format!("${}\r\n", v.value()),
                    None => "$nil\r\n".to_string(),
                };
                writer.write_all(response.as_bytes()).await?;
            }
            _ => {
                writer.write_all(b"-ERR unknown command\r\n").await?;
            }
        }
    }

    Ok(())
}
```

---

## 6. Graceful shutdown async

El shutdown graceful en async es más elegante que con threads porque podemos usar las
primitivas de Tokio para señalizar a todas las tasks.

### Con tokio::signal y CancellationToken

```rust
use std::sync::Arc;
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio_util::sync::CancellationToken;

// Cargo.toml:
// tokio = { version = "1", features = ["full"] }
// tokio-util = "0.7"

async fn handle_client(
    mut stream: tokio::net::TcpStream,
    token: CancellationToken,
) -> tokio::io::Result<()> {
    let mut buf = [0u8; 1024];

    loop {
        tokio::select! {
            _ = token.cancelled() => {
                stream.write_all(b"Server shutting down\n").await?;
                return Ok(());
            }
            result = stream.read(&mut buf) => {
                let n = result?;
                if n == 0 { return Ok(()); }
                stream.write_all(&buf[..n]).await?;
            }
        }
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let token = CancellationToken::new();

    println!("Server listening. Press Ctrl+C to shutdown.");

    loop {
        tokio::select! {
            _ = tokio::signal::ctrl_c() => {
                println!("\nShutdown signal received");
                token.cancel();
                // Dar tiempo a las tasks para cerrar
                tokio::time::sleep(std::time::Duration::from_secs(2)).await;
                println!("Server stopped");
                return Ok(());
            }
            result = listener.accept() => {
                let (stream, addr) = result?;
                let token = token.clone();

                tokio::spawn(async move {
                    println!("[{}] connected", addr);
                    if let Err(e) = handle_client(stream, token).await {
                        eprintln!("[{}] error: {}", addr, e);
                    }
                    println!("[{}] disconnected", addr);
                });
            }
        }
    }
}
```

### Con JoinSet para esperar a todas las tasks

`JoinSet` (de tokio) permite rastrear y esperar a un conjunto de tasks spawneadas:

```rust
use tokio::net::TcpListener;
use tokio::task::JoinSet;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio_util::sync::CancellationToken;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let token = CancellationToken::new();
    let mut tasks = JoinSet::new();

    loop {
        tokio::select! {
            _ = tokio::signal::ctrl_c() => {
                println!("\nShutting down...");
                token.cancel();

                // Esperar a que todas las tasks terminen (con timeout)
                let deadline = tokio::time::Instant::now()
                    + std::time::Duration::from_secs(5);

                while !tasks.is_empty() {
                    tokio::select! {
                        result = tasks.join_next() => {
                            match result {
                                Some(Ok(())) => {}
                                Some(Err(e)) => eprintln!("Task error: {}", e),
                                None => break,
                            }
                        }
                        _ = tokio::time::sleep_until(deadline) => {
                            println!("Timeout: aborting {} remaining tasks",
                                     tasks.len());
                            tasks.abort_all();
                            break;
                        }
                    }
                }

                println!("Server stopped cleanly");
                return Ok(());
            }
            result = listener.accept() => {
                let (mut stream, addr) = result?;
                let token = token.clone();

                tasks.spawn(async move {
                    let mut buf = [0u8; 1024];
                    loop {
                        tokio::select! {
                            _ = token.cancelled() => {
                                let _ = stream.write_all(b"BYE\n").await;
                                return;
                            }
                            result = stream.read(&mut buf) => {
                                match result {
                                    Ok(0) | Err(_) => return,
                                    Ok(n) => {
                                        let _ = stream.write_all(&buf[..n]).await;
                                    }
                                }
                            }
                        }
                    }
                });

                println!("[{}] connected ({} active)", addr, tasks.len());
            }
        }
    }
}
```

```
┌────── Ciclo de vida del shutdown ──────────────────────┐
│                                                        │
│  Ctrl+C ──→ token.cancel() ──→ tasks ven cancelled()   │
│                                                        │
│  ┌──────────────────────────────────┐                  │
│  │ Task 1: envía "BYE", cierra     │──→ join_next()    │
│  │ Task 2: envía "BYE", cierra     │──→ join_next()    │
│  │ Task 3: ya había terminado      │                   │
│  │ Task 4: no responde a tiempo    │──→ abort()        │
│  └──────────────────────────────────┘                  │
│                                                        │
│  Todas resueltas o abortadas ──→ "Server stopped"      │
└────────────────────────────────────────────────────────┘
```

### Con broadcast para shutdown

Una alternativa más simple sin `tokio-util`:

```rust
use tokio::net::TcpListener;
use tokio::sync::broadcast;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let (shutdown_tx, _) = broadcast::channel::<()>(1);

    loop {
        tokio::select! {
            _ = tokio::signal::ctrl_c() => {
                println!("\nShutting down...");
                let _ = shutdown_tx.send(());
                tokio::time::sleep(std::time::Duration::from_secs(2)).await;
                return Ok(());
            }
            result = listener.accept() => {
                let (mut stream, addr) = result?;
                let mut shutdown_rx = shutdown_tx.subscribe();

                tokio::spawn(async move {
                    let mut buf = [0u8; 1024];
                    loop {
                        tokio::select! {
                            _ = shutdown_rx.recv() => {
                                let _ = stream.write_all(b"Server closing\n").await;
                                return;
                            }
                            result = stream.read(&mut buf) => {
                                match result {
                                    Ok(0) | Err(_) => return,
                                    Ok(n) => {
                                        let _ = stream.write_all(&buf[..n]).await;
                                    }
                                }
                            }
                        }
                    }
                });

                println!("[{}] connected", addr);
            }
        }
    }
}
```

---

## 7. Patrones de servidor completo

### Echo server con todas las piezas

```rust
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::task::JoinSet;
use tokio_util::sync::CancellationToken;

struct Server {
    active: AtomicUsize,
    total: AtomicUsize,
    max_connections: usize,
}

impl Server {
    fn new(max_connections: usize) -> Self {
        Self {
            active: AtomicUsize::new(0),
            total: AtomicUsize::new(0),
            max_connections,
        }
    }

    fn try_acquire(&self) -> bool {
        let current = self.active.load(Ordering::Relaxed);
        if current >= self.max_connections {
            return false;
        }
        self.active.fetch_add(1, Ordering::Relaxed);
        self.total.fetch_add(1, Ordering::Relaxed);
        true
    }

    fn release(&self) {
        self.active.fetch_sub(1, Ordering::Relaxed);
    }
}

async fn handle_client(
    stream: TcpStream,
    server: Arc<Server>,
    token: CancellationToken,
) -> tokio::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    // Timeout para inactividad
    let idle_timeout = std::time::Duration::from_secs(60);

    loop {
        line.clear();

        tokio::select! {
            _ = token.cancelled() => {
                writer.write_all(b"Server shutting down.\n").await?;
                break;
            }
            result = tokio::time::timeout(idle_timeout, reader.read_line(&mut line)) => {
                match result {
                    Err(_) => {
                        writer.write_all(b"Idle timeout, disconnecting.\n").await?;
                        break;
                    }
                    Ok(Ok(0)) => break,  // EOF
                    Ok(Ok(_)) => {
                        let trimmed = line.trim();
                        if trimmed.eq_ignore_ascii_case("quit") {
                            writer.write_all(b"Goodbye!\n").await?;
                            break;
                        }
                        if trimmed.eq_ignore_ascii_case("stats") {
                            let msg = format!(
                                "Active: {} | Total: {}\n",
                                server.active.load(Ordering::Relaxed),
                                server.total.load(Ordering::Relaxed),
                            );
                            writer.write_all(msg.as_bytes()).await?;
                        } else {
                            writer.write_all(line.as_bytes()).await?;
                        }
                    }
                    Ok(Err(e)) => return Err(e),
                }
            }
        }
    }

    server.release();
    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let server = Arc::new(Server::new(100));
    let token = CancellationToken::new();
    let mut tasks = JoinSet::new();

    println!("Server ready on 127.0.0.1:8080 (max 100 clients)");

    loop {
        tokio::select! {
            _ = tokio::signal::ctrl_c() => {
                println!("\nGraceful shutdown initiated...");
                token.cancel();

                let deadline = tokio::time::Instant::now()
                    + std::time::Duration::from_secs(5);

                while !tasks.is_empty() {
                    tokio::select! {
                        _ = tasks.join_next() => {}
                        _ = tokio::time::sleep_until(deadline) => {
                            tasks.abort_all();
                            break;
                        }
                    }
                }

                println!(
                    "Server stopped. Total connections served: {}",
                    server.total.load(Ordering::Relaxed)
                );
                return Ok(());
            }
            result = listener.accept() => {
                let (stream, addr) = result?;

                if !server.try_acquire() {
                    eprintln!("[{}] rejected: max connections reached", addr);
                    // stream se dropea aquí, cerrando la conexión
                    continue;
                }

                let server = Arc::clone(&server);
                let token = token.clone();

                tasks.spawn(async move {
                    println!("[{}] connected", addr);
                    if let Err(e) = handle_client(stream, server, token).await {
                        eprintln!("[{}] error: {}", addr, e);
                    }
                    println!("[{}] disconnected", addr);
                });
            }
        }
    }
}
```

### Chat server con broadcast

```rust
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::broadcast;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

static NEXT_ID: AtomicUsize = AtomicUsize::new(1);

async fn handle_client(
    stream: TcpStream,
    tx: broadcast::Sender<(usize, String)>,
) -> tokio::io::Result<()> {
    let client_id = NEXT_ID.fetch_add(1, Ordering::Relaxed);
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut rx = tx.subscribe();

    // Anunciar entrada
    let _ = tx.send((client_id, format!("User {} joined\n", client_id)));

    let mut line = String::new();

    loop {
        tokio::select! {
            // Recibir mensajes de otros clientes
            result = rx.recv() => {
                match result {
                    Ok((sender_id, msg)) => {
                        // No enviar nuestros propios mensajes de vuelta
                        if sender_id != client_id {
                            if writer.write_all(msg.as_bytes()).await.is_err() {
                                break;
                            }
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        let msg = format!("*** Missed {} messages ***\n", n);
                        let _ = writer.write_all(msg.as_bytes()).await;
                    }
                    Err(broadcast::error::RecvError::Closed) => break,
                }
            }
            // Leer input del cliente
            result = reader.read_line(&mut line) => {
                match result {
                    Ok(0) => break,  // EOF
                    Ok(_) => {
                        let msg = format!("[User {}] {}", client_id, &line);
                        let _ = tx.send((client_id, msg));
                        line.clear();
                    }
                    Err(_) => break,
                }
            }
        }
    }

    let _ = tx.send((client_id, format!("User {} left\n", client_id)));
    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let (tx, _) = broadcast::channel::<(usize, String)>(100);

    println!("Chat server on 127.0.0.1:8080");

    loop {
        let (stream, addr) = listener.accept().await?;
        let tx = tx.clone();

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            if let Err(e) = handle_client(stream, tx).await {
                eprintln!("[{}] error: {}", addr, e);
            }
        });
    }
}
```

Este patrón combina `broadcast` para mensajes entre clientes con `select!` para leer
y escribir concurrentemente en cada conexión.

### Proxy TCP simple

```rust
use tokio::net::{TcpListener, TcpStream};
use tokio::io;

async fn proxy(mut client: TcpStream, upstream_addr: &str) -> io::Result<()> {
    let mut upstream = TcpStream::connect(upstream_addr).await?;

    let (mut client_read, mut client_write) = client.split();
    let (mut upstream_read, mut upstream_write) = upstream.split();

    // Copiar en ambas direcciones simultáneamente
    let client_to_upstream = io::copy(&mut client_read, &mut upstream_write);
    let upstream_to_client = io::copy(&mut upstream_read, &mut client_write);

    // Esperar a que alguna dirección termine
    tokio::select! {
        result = client_to_upstream => {
            result?;
        }
        result = upstream_to_client => {
            result?;
        }
    }

    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:9090").await?;
    let upstream = "127.0.0.1:8080";

    println!("Proxy :9090 -> {}", upstream);

    loop {
        let (stream, addr) = listener.accept().await?;

        tokio::spawn(async move {
            println!("[{}] proxying", addr);
            if let Err(e) = proxy(stream, "127.0.0.1:8080").await {
                eprintln!("[{}] proxy error: {}", addr, e);
            }
        });
    }
}
```

---

## 8. Rendimiento: threads vs async

### Benchmark conceptual

```
┌────── Escenario: 10,000 conexiones idle ──────────┐
│                                                    │
│  Con threads:                                      │
│  • Memoria: 10,000 × 8 MB = 80 GB (imposible)     │
│  • Con stack reducido (64 KB): 640 MB              │
│  • Context switches: ~10,000/seg                   │
│                                                    │
│  Con async (Tokio):                                │
│  • Memoria: 10,000 × ~1 KB = ~10 MB               │
│  • Context switches: 0 (cooperativo)               │
│  • Threads del SO: 8 (= número de CPUs)            │
│                                                    │
└────────────────────────────────────────────────────┘
```

### ¿Cuándo no usar async?

Async brilla con I/O-bound, pero no siempre es la mejor opción:

```
┌────── Decisión: threads vs async ─────────────────────────┐
│                                                            │
│  ¿Pocas conexiones (<100) con CPU intensivo?               │
│  └─→ Threads: más simple, sin overhead de runtime          │
│                                                            │
│  ¿Muchas conexiones (>1000) mayormente idle?               │
│  └─→ Async: menor memoria, mejor escalabilidad            │
│                                                            │
│  ¿Operaciones que bloquean (disk I/O sync, cómputo)?       │
│  └─→ spawn_blocking() dentro de async                      │
│                                                            │
│  ¿Latencia ultra-baja (microsegundos)?                     │
│  └─→ Threads dedicados + busy-polling                      │
│                                                            │
│  ¿Prototipo rápido sin dependencias externas?              │
│  └─→ Threads: solo std, sin tokio                          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### spawn_blocking para operaciones CPU-bound

Si dentro de tu handler async necesitas hacer algo que bloquea (cómputo pesado, I/O
síncrono de archivos), usa `spawn_blocking` para no bloquear el runtime:

```rust
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn handle_client(mut stream: TcpStream) -> tokio::io::Result<()> {
    let mut data = vec![0u8; 4096];
    let n = stream.read(&mut data).await?;
    data.truncate(n);

    // Operación CPU-intensiva: ejecutar en thread de bloqueo
    let result = tokio::task::spawn_blocking(move || {
        // Simulación de cómputo pesado
        let hash = data.iter().fold(0u64, |acc, &b| {
            acc.wrapping_mul(31).wrapping_add(b as u64)
        });
        format!("Hash: {}\n", hash)
    })
    .await
    .expect("blocking task panicked");

    stream.write_all(result.as_bytes()).await?;
    Ok(())
}
```

### Configuración del runtime

```rust
// Runtime por defecto: multi-thread, CPUs automáticos
#[tokio::main]
async fn main() { }

// Equivale a:
fn main() {
    tokio::runtime::Runtime::new()
        .unwrap()
        .block_on(async { });
}

// Configuración personalizada
fn main() {
    let rt = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(4)           // 4 threads del SO
        .max_blocking_threads(128)   // Para spawn_blocking
        .enable_all()                // Timer + I/O
        .thread_name("my-server")
        .build()
        .unwrap();

    rt.block_on(async {
        // Tu servidor aquí
    });
}

// Single-thread (para tests o servidores simples)
#[tokio::main(flavor = "current_thread")]
async fn main() { }
```

---

## 9. Errores comunes

### Error 1: usar std::net dentro de Tokio

```rust
// MAL: bloquea el thread del runtime
use std::net::TcpListener;

#[tokio::main]
async fn main() {
    let listener = std::net::TcpListener::bind("127.0.0.1:8080").unwrap();
    for stream in listener.incoming() {
        // Esto bloquea un worker thread de Tokio
        // Las demás tasks se paralizan
    }
}

// BIEN: usar tokio::net
use tokio::net::TcpListener;

#[tokio::main]
async fn main() {
    let listener = TcpListener::bind("127.0.0.1:8080").await.unwrap();
    loop {
        let (stream, _) = listener.accept().await.unwrap();
        tokio::spawn(async move {
            // handle async...
        });
    }
}
```

**Por qué falla**: `std::net` bloquea el thread del SO. Tokio tiene pocos threads (uno por
CPU), así que bloquear uno impacta a todas las tasks asignadas a ese thread.

### Error 2: mantener un MutexGuard a través de .await

```rust
use std::sync::{Arc, Mutex};
use std::collections::HashMap;

type Shared = Arc<Mutex<HashMap<String, String>>>;

// MAL: MutexGuard vive a través del .await
async fn bad_handler(shared: Shared, key: String) {
    let mut map = shared.lock().unwrap();
    // Este .await hace que el MutexGuard cruce un punto de suspensión
    // Si la task se suspende aquí, el lock queda tomado
    let value = fetch_from_db(&key).await;  // ← PELIGRO
    map.insert(key, value);
}

// BIEN: liberar el lock antes del .await
async fn good_handler(shared: Shared, key: String) {
    let value = fetch_from_db(&key).await;  // Primero I/O async
    let mut map = shared.lock().unwrap();    // Luego lock breve
    map.insert(key, value);                  // Lock se libera al salir del scope
}

async fn fetch_from_db(key: &str) -> String {
    tokio::time::sleep(std::time::Duration::from_millis(10)).await;
    format!("value_for_{}", key)
}
```

**Por qué falla**: `std::sync::Mutex` no es async-aware. Si la task se suspende con el lock
tomado, el thread queda disponible para otras tasks que intentarán el mismo lock → deadlock.

### Error 3: olvidar que tokio::spawn requiere Send + 'static

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() {
    let listener = TcpListener::bind("127.0.0.1:8080").await.unwrap();
    let config = String::from("my config");

    loop {
        let (stream, _) = listener.accept().await.unwrap();

        // ERROR de compilación: config no es 'static (se mueve en el primer iter)
        // tokio::spawn(async move {
        //     println!("{}", config);
        // });

        // BIEN: clonar para cada task
        let config = config.clone();
        tokio::spawn(async move {
            println!("{}", config);
        });
    }
}
```

**Por qué falla**: cada iteración del loop intenta mover `config` al closure. Después de la
primera, `config` ya no existe. Clona antes de mover.

### Error 4: no manejar errores de accept

```rust
// MAL: un error en accept mata todo el servidor
#[tokio::main]
async fn main() {
    let listener = tokio::net::TcpListener::bind("127.0.0.1:8080")
        .await.unwrap();

    loop {
        let (stream, addr) = listener.accept().await.unwrap();  // ← panic si falla
        // ...
    }
}

// BIEN: manejar errores transitorios
#[tokio::main]
async fn main() {
    let listener = tokio::net::TcpListener::bind("127.0.0.1:8080")
        .await.unwrap();

    loop {
        match listener.accept().await {
            Ok((stream, addr)) => {
                tokio::spawn(async move {
                    // handle...
                });
            }
            Err(e) => {
                // Errores como EMFILE (too many open files) son recuperables
                eprintln!("Accept error: {}. Retrying...", e);
                tokio::time::sleep(std::time::Duration::from_millis(100)).await;
            }
        }
    }
}
```

**Por qué falla**: en producción, `accept()` puede fallar por límite de file descriptors,
interrupciones del SO, o condiciones transitorias. El servidor debe seguir corriendo.

### Error 5: hacer I/O síncrono pesado en una task async

```rust
use tokio::net::TcpStream;
use tokio::io::AsyncWriteExt;

// MAL: std::fs::read bloquea el thread del runtime
async fn serve_file(mut stream: TcpStream, path: &str) -> tokio::io::Result<()> {
    let contents = std::fs::read(path)?;  // ← Bloquea el worker thread
    stream.write_all(&contents).await?;
    Ok(())
}

// BIEN: usar tokio::fs o spawn_blocking
async fn serve_file_async(mut stream: TcpStream, path: &str) -> tokio::io::Result<()> {
    // Opción 1: tokio::fs (delega a spawn_blocking internamente)
    let contents = tokio::fs::read(path).await?;
    stream.write_all(&contents).await?;
    Ok(())
}

async fn serve_file_blocking(
    mut stream: TcpStream,
    path: String,
) -> tokio::io::Result<()> {
    // Opción 2: spawn_blocking explícito
    let contents = tokio::task::spawn_blocking(move || {
        std::fs::read(&path)
    }).await.unwrap()?;
    stream.write_all(&contents).await?;
    Ok(())
}
```

**Por qué falla**: cada thread del runtime de Tokio ejecuta muchas tasks. Si bloqueas un
thread con I/O síncrono, todas las tasks asignadas a ese thread se detienen. Con pocas
conexiones no lo notarás, pero bajo carga el servidor se congela.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                     SERVIDOR TCP CON TOKIO                             ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  SETUP                                                                 ║
║  ─────                                                                 ║
║  let listener = TcpListener::bind("addr").await?;                      ║
║  let (stream, addr) = listener.accept().await?;                        ║
║  let local = listener.local_addr()?;        // puerto asignado         ║
║                                                                        ║
║  STREAM I/O                                                            ║
║  ─────────                                                             ║
║  let n = stream.read(&mut buf).await?;      // parcial, 0 = EOF       ║
║  stream.write_all(data).await?;             // escribe todo            ║
║  let (r, w) = stream.split();               // borrow, misma task     ║
║  let (r, w) = stream.into_split();          // owned, entre tasks     ║
║  io::copy(&mut r, &mut w).await?;           // proxy/echo             ║
║                                                                        ║
║  BUFFERED I/O                                                          ║
║  ───────────                                                           ║
║  let mut r = BufReader::new(reader);                                   ║
║  r.read_line(&mut line).await?;             // lee hasta \n            ║
║  let mut w = BufWriter::new(writer);                                   ║
║  w.write_all(data).await?; w.flush().await?;                           ║
║                                                                        ║
║  SPAWN POR CONEXIÓN                                                    ║
║  ──────────────────                                                    ║
║  tokio::spawn(async move { handle(stream).await; });                   ║
║  // Requiere: future es Send + 'static                                 ║
║  // Clonar datos compartidos ANTES del move                            ║
║                                                                        ║
║  ESTADO COMPARTIDO                                                     ║
║  ─────────────────                                                     ║
║  Arc<std::sync::Mutex<T>>      // lock rápido sin .await dentro        ║
║  Arc<tokio::sync::Mutex<T>>    // lock con .await dentro               ║
║  Arc<RwLock<T>>                // muchas lecturas, pocas escrituras     ║
║  Arc<AtomicUsize>              // contadores                           ║
║  Arc<DashMap<K,V>>             // HashMap concurrente (crate)          ║
║                                                                        ║
║  GRACEFUL SHUTDOWN                                                     ║
║  ─────────────────                                                     ║
║  CancellationToken::new()      // tokio-util                           ║
║  token.cancel() / token.cancelled().await                              ║
║  broadcast::channel()          // alternativa sin dep extra            ║
║  JoinSet::new()                // rastrear tasks activas               ║
║  tasks.abort_all()             // forzar cierre tras timeout           ║
║                                                                        ║
║  RENDIMIENTO                                                           ║
║  ────────────                                                          ║
║  spawn_blocking(|| { ... })    // CPU-bound o I/O sync                 ║
║  tokio::fs::read(path).await   // file I/O async                       ║
║  #[tokio::main(flavor = "current_thread")]  // single-thread           ║
║  Builder::new_multi_thread().worker_threads(N)  // config              ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Echo server async con estadísticas

Construye un servidor echo TCP usando `tokio::net` que:
1. Acepte múltiples conexiones concurrentes con `tokio::spawn`.
2. Mantenga contadores atómicos de: conexiones activas, total de conexiones, y total de
   bytes transmitidos.
3. Si el cliente envía la línea `STATS\n`, responda con las estadísticas actuales en lugar
   de hacer echo.
4. Si el cliente envía `QUIT\n`, cierre la conexión con un mensaje de despedida.

Prueba con múltiples `nc 127.0.0.1 8080` en terminales diferentes.

**Pista**: usa `Arc` con `AtomicUsize` para los contadores. No necesitas `Mutex` para ellos.

> **Pregunta de reflexión**: ¿Qué ventaja tiene usar `AtomicUsize` en lugar de
> `Arc<Mutex<usize>>` para un simple contador? ¿En qué situaciones sí necesitarías el
> `Mutex`?

---

### Ejercicio 2: Servidor key-value con timeout de inactividad

Implementa un servidor TCP async que funcione como un almacén key-value simple:
1. Protocolo basado en líneas: `SET key value\n` → `+OK\r\n`, `GET key\n` →
   `$value\r\n` o `$nil\r\n`.
2. Estado compartido usando `Arc<std::sync::Mutex<HashMap<String, String>>>`.
3. Si un cliente no envía nada durante 30 segundos, desconectarlo con un mensaje de timeout.
4. Limitar a máximo 50 conexiones simultáneas (rechazar las que excedan).

**Pista**: usa `tokio::time::timeout` alrededor de `read_line` para la inactividad. Para el
límite de conexiones, un `AtomicUsize` con check antes del `spawn`.

> **Pregunta de reflexión**: ¿Por qué en este ejercicio es seguro usar `std::sync::Mutex`
> en lugar de `tokio::sync::Mutex`? ¿Qué cambio en la lógica te obligaría a usar la versión
> de tokio?

---

### Ejercicio 3: Proxy TCP con logging

Crea un proxy TCP async que reenvíe tráfico entre un cliente y un servidor upstream:
1. Escucha en `127.0.0.1:9090` y reenvía a `127.0.0.1:8080`.
2. Usa `stream.into_split()` y `tokio::io::copy` para copiar datos en ambas direcciones.
3. Registra (con `println!`): dirección del cliente, bytes enviados en cada dirección, y
   duración de la conexión.
4. Implementa graceful shutdown con `Ctrl+C` usando `tokio::signal::ctrl_c()`.

Para probar: ejecuta el echo server del Ejercicio 1 en puerto 8080, luego conecta clientes
a 9090 con `nc`.

**Pista**: `io::copy` retorna el número de bytes copiados. Usa `select!` para copiar en
ambas direcciones y registra el ganador (quién cerró primero).

> **Pregunta de reflexión**: en el proxy, ¿por qué usamos `into_split()` en lugar de
> `split()`? ¿Qué error de compilación obtendrías si intentaras usar `split()` y pasar las
> mitades a `tokio::spawn`?
