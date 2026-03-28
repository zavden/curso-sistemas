# Servidor con Tokio: TcpListener, spawn y broadcast

## Índice

1. [Arquitectura de un chat TCP](#arquitectura-de-un-chat-tcp)
2. [Setup del proyecto](#setup-del-proyecto)
3. [TcpListener async con Tokio](#tcplistener-async-con-tokio)
4. [Spawn por conexión](#spawn-por-conexión)
5. [Leer y escribir con AsyncRead/AsyncWrite](#leer-y-escribir-con-asyncreadasyncwrite)
6. [Broadcast de mensajes](#broadcast-de-mensajes)
7. [El servidor completo](#el-servidor-completo)
8. [Testing del servidor](#testing-del-servidor)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Arquitectura de un chat TCP

Un chat TCP multi-cliente sigue el modelo **servidor centralizado**: todos los
clientes se conectan al servidor, y el servidor retransmite los mensajes.

```
┌──────────────────────────────────────────────────────────────┐
│                  Arquitectura del Chat                        │
│                                                              │
│  Cliente A ──────┐                                           │
│                  │     ┌──────────────────┐                  │
│  Cliente B ──────┼────▶│    Servidor      │                  │
│                  │     │                  │                  │
│  Cliente C ──────┘     │  - Acepta        │                  │
│                        │    conexiones    │                  │
│       ▲                │  - Recibe msg    │                  │
│       │                │    de cualquier  │                  │
│       │                │    cliente       │                  │
│       └────────────────│  - Broadcast a   │                  │
│      Recibe mensajes   │    todos los     │                  │
│      de TODOS los      │    demás         │                  │
│      otros clientes    └──────────────────┘                  │
│                                                              │
│  Flujo:                                                      │
│  1. Cliente A envía: "Hola"                                  │
│  2. Servidor recibe de A                                     │
│  3. Servidor reenvía "A: Hola" a B y C                       │
│  4. B y C ven el mensaje en su terminal                      │
└──────────────────────────────────────────────────────────────┘
```

### ¿Por qué Tokio?

Con threads estándar, cada conexión requiere un thread del OS. Con 1000
clientes necesitas 1000 threads, cada uno consumiendo ~8MB de stack.

Con Tokio (async), cada conexión es una **task** liviana (~pocos KB). Puedes
manejar miles de conexiones con un puñado de threads del OS.

```
┌──────────────────────────────────────────────────────────────┐
│  Threads (std::thread)           Async tasks (Tokio)         │
│                                                              │
│  1 thread = 1 conexión           1 task = 1 conexión         │
│  ~8MB stack por thread           ~pocos KB por task           │
│  1000 conexiones = 8GB RAM       1000 conexiones = ~10MB     │
│  Context switch costoso          Cooperativo (más eficiente) │
│  Paralelismo real (multi-core)   Concurrencia + paralelismo  │
│                                                              │
│  Para un chat: async es claramente mejor                     │
│  (mucho I/O wait, poco cómputo por conexión)                │
└──────────────────────────────────────────────────────────────┘
```

---

## Setup del proyecto

### Estructura

```
chat-server/
├── Cargo.toml
├── src/
│   ├── main.rs           ← Punto de entrada, acepta conexiones
│   ├── server.rs         ← Lógica del servidor
│   ├── client_handler.rs ← Manejo de cada cliente (T02)
│   └── state.rs          ← Estado compartido (T03)
└── tests/
    └── integration.rs
```

### Dependencias

```toml
# Cargo.toml
[package]
name = "chat-server"
version = "0.1.0"
edition = "2021"

[dependencies]
tokio = { version = "1", features = ["full"] }
```

La feature `"full"` incluye: runtime multi-thread, I/O async, timers,
sync primitives, macros (`#[tokio::main]`, `tokio::select!`).

---

## TcpListener async con Tokio

### El loop de aceptación

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Bind: reservar el puerto
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Chat server listening on 127.0.0.1:8080");

    // Loop infinito: aceptar conexiones
    loop {
        // .await suspende hasta que llegue una nueva conexión
        let (socket, addr) = listener.accept().await?;
        println!("New connection from {}", addr);

        // Manejar la conexión (por ahora, inline)
        // En la siguiente sección: spawn
        handle_connection(socket).await;
    }
}

async fn handle_connection(socket: tokio::net::TcpStream) {
    println!("Handling connection...");
    // TODO
}
```

### Probar con netcat

```bash
# Terminal 1: iniciar el servidor
cargo run

# Terminal 2: conectar con netcat
nc 127.0.0.1 8080

# Terminal 3: otra conexión
nc 127.0.0.1 8080
```

### Problema: un cliente bloquea al servidor

En el código anterior, `handle_connection(socket).await` **bloquea el loop**.
Mientras atendemos al primer cliente, no podemos aceptar nuevos.

```
┌──────────────────────────────────────────────────────────────┐
│  Problema: await secuencial                                  │
│                                                              │
│  Tiempo ───────────────────────────────────────────▶         │
│                                                              │
│  accept() → handle(client_1) ──────────────────┐            │
│                                                 │            │
│  client_2 conecta ──── esperando ────── espera  │            │
│                                                 │            │
│  client_3 conecta ──── esperando ────── espera  │            │
│                                                 ▼            │
│                                          client_1 termina    │
│                                          AHORA acepta client_2│
│                                                              │
│  Solución: tokio::spawn para cada conexión                   │
└──────────────────────────────────────────────────────────────┘
```

---

## Spawn por conexión

`tokio::spawn` lanza una task async que corre independientemente. Cada
conexión obtiene su propia task.

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Chat server listening on 127.0.0.1:8080");

    loop {
        let (socket, addr) = listener.accept().await?;
        println!("New connection from {}", addr);

        // Spawn: la task corre en background
        tokio::spawn(async move {
            if let Err(e) = handle_connection(socket).await {
                eprintln!("Error handling {}: {}", addr, e);
            }
        });

        // El loop inmediatamente vuelve a accept()
        // No esperamos a que handle_connection termine
    }
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  Con tokio::spawn:                                           │
│                                                              │
│  Tiempo ───────────────────────────────────────────▶         │
│                                                              │
│  accept() → spawn(handle(client_1)) ──────────────▶         │
│     │                                                        │
│     └─ accept() → spawn(handle(client_2)) ────────▶         │
│           │                                                  │
│           └─ accept() → spawn(handle(client_3)) ──▶         │
│                 │                                            │
│                 └─ accept() → esperando...                   │
│                                                              │
│  Todas las conexiones se manejan simultáneamente             │
└──────────────────────────────────────────────────────────────┘
```

### Requisitos de `tokio::spawn`

El future pasado a `tokio::spawn` debe ser:
- `Send` — puede moverse entre threads del runtime
- `'static` — no puede tener referencias a datos del stack del caller

```rust
// ❌ No compila: socket no es 'static si se toma por referencia
tokio::spawn(async {
    handle_connection(&socket).await;  // &socket no es 'static
});

// ✅ Mover ownership al future
tokio::spawn(async move {
    handle_connection(socket).await;   // socket movido, es 'static
});
```

---

## Leer y escribir con AsyncRead/AsyncWrite

### Leer líneas de un socket

Tokio provee `BufReader` async y el trait `AsyncBufReadExt` con `.lines()`:

```rust
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;

async fn handle_connection(socket: TcpStream) -> Result<(), Box<dyn std::error::Error>> {
    // Split: separar lectura y escritura
    let (reader, mut writer) = socket.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    // Enviar bienvenida
    writer.write_all(b"Welcome to the chat!\n").await?;

    // Leer líneas del cliente
    loop {
        line.clear();
        let bytes_read = reader.read_line(&mut line).await?;

        if bytes_read == 0 {
            // EOF: el cliente se desconectó
            println!("Client disconnected");
            break;
        }

        let trimmed = line.trim();
        println!("Received: {}", trimmed);

        // Echo: enviar de vuelta al mismo cliente
        writer.write_all(format!("Echo: {}\n", trimmed).as_bytes()).await?;
    }

    Ok(())
}
```

### Split: lectura y escritura simultáneas

`into_split()` divide un `TcpStream` en una mitad de lectura (`OwnedReadHalf`)
y una de escritura (`OwnedWriteHalf`). Esto permite leer y escribir
concurrentemente en la misma conexión.

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Por qué split?                                             │
│                                                              │
│  Sin split:                                                  │
│  Necesitas &mut TcpStream para leer Y para escribir          │
│  → Solo puedes hacer uno a la vez                            │
│                                                              │
│  Con split:                                                  │
│  ┌────────────────┐         ┌─────────────────┐             │
│  │ OwnedReadHalf  │         │ OwnedWriteHalf  │             │
│  │ (solo lectura)  │         │ (solo escritura) │             │
│  └────────────────┘         └─────────────────┘             │
│  Cada mitad se puede mover a una task diferente              │
│  → Leer y escribir simultáneamente                           │
└──────────────────────────────────────────────────────────────┘
```

### Framing: delimitar mensajes

TCP es un stream de bytes sin noción de "mensaje". Necesitamos un **protocolo
de framing** para saber dónde termina un mensaje y empieza otro.

La opción más simple: cada mensaje es una línea (terminada en `\n`).

```
┌──────────────────────────────────────────────────────────────┐
│  TCP stream raw:                                             │
│  "Hello\nHow are you\nFine thanks\n"                         │
│  → ¿Es un mensaje o tres? Con framing por líneas: son tres   │
│                                                              │
│  Problema potencial:                                         │
│  TCP puede entregar "Hel" y "lo\nHow" en dos read() calls   │
│  → read_line() maneja esto automáticamente: espera el \n     │
└──────────────────────────────────────────────────────────────┘
```

---

## Broadcast de mensajes

El corazón del chat: cuando un cliente envía un mensaje, el servidor lo
reenvía a **todos** los demás clientes.

### Tokio broadcast channel

```rust
use tokio::sync::broadcast;

// Crear el canal: capacidad para 100 mensajes en buffer
let (tx, _rx) = broadcast::channel::<String>(100);

// Cada cliente obtiene:
// - Un clone del tx (para enviar mensajes al canal)
// - Un rx propio (para recibir mensajes del canal)
```

```
┌──────────────────────────────────────────────────────────────┐
│  broadcast::channel                                          │
│                                                              │
│  tx.send("Hello")                                            │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────────────┐                                       │
│  │  Canal broadcast  │                                       │
│  └──┬──────┬────────┘                                       │
│     │      │      │                                          │
│     ▼      ▼      ▼                                          │
│   rx_A   rx_B   rx_C                                         │
│     │      │      │                                          │
│     ▼      ▼      ▼                                          │
│  Client  Client  Client                                      │
│    A       B       C                                         │
│                                                              │
│  Todos los rx reciben una copia del mensaje                  │
│  Si un rx es lento y se llena el buffer → RecvError::Lagged  │
└──────────────────────────────────────────────────────────────┘
```

### Server con broadcast

```rust
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::broadcast;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Chat server listening on 127.0.0.1:8080");

    // Canal de broadcast compartido por todos los clientes
    let (tx, _rx) = broadcast::channel::<String>(100);

    loop {
        let (socket, addr) = listener.accept().await?;
        println!("{} connected", addr);

        // Cada cliente obtiene su propio tx (clone) y rx (subscribe)
        let tx = tx.clone();
        let rx = tx.subscribe();

        tokio::spawn(async move {
            if let Err(e) = handle_client(socket, addr, tx, rx).await {
                eprintln!("{} error: {}", addr, e);
            }
            println!("{} disconnected", addr);
        });
    }
}

async fn handle_client(
    socket: TcpStream,
    addr: std::net::SocketAddr,
    tx: broadcast::Sender<String>,
    mut rx: broadcast::Receiver<String>,
) -> Result<(), Box<dyn std::error::Error>> {
    let (reader, mut writer) = socket.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    writer.write_all(b"Welcome! Type messages and press Enter.\n").await?;

    loop {
        // tokio::select! espera AMBOS futures simultáneamente:
        // 1. Leer una línea del cliente
        // 2. Recibir un mensaje del broadcast channel
        tokio::select! {
            // El cliente envió algo
            bytes_read = reader.read_line(&mut line) => {
                let bytes_read = bytes_read?;
                if bytes_read == 0 {
                    break;  // Cliente desconectado
                }

                let msg = format!("{}: {}", addr, line.trim());
                println!("{}", msg);

                // Broadcast a todos los demás
                let _ = tx.send(msg);

                line.clear();
            }

            // Llegó un mensaje de otro cliente
            result = rx.recv() => {
                match result {
                    Ok(msg) => {
                        // No reenviar al mismo cliente que lo envió
                        if !msg.starts_with(&addr.to_string()) {
                            writer.write_all(format!("{}\n", msg).as_bytes()).await?;
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        writer.write_all(
                            format!("[Lost {} messages due to lag]\n", n).as_bytes()
                        ).await?;
                    }
                    Err(broadcast::error::RecvError::Closed) => {
                        break;  // Canal cerrado, servidor terminando
                    }
                }
            }
        }
    }

    Ok(())
}
```

### Entender `tokio::select!`

```
┌──────────────────────────────────────────────────────────────┐
│  tokio::select! — espera múltiples futures                   │
│                                                              │
│  select! {                                                   │
│      result = future_1 => { /* future_1 completó primero */ }│
│      result = future_2 => { /* future_2 completó primero */ }│
│  }                                                           │
│                                                              │
│  Semántica:                                                  │
│  - Registra ambos futures para polling                       │
│  - El primero que completa "gana"                            │
│  - El otro se cancela (dropped)                              │
│  - En el loop, se re-registran en la siguiente iteración     │
│                                                              │
│  Para el chat:                                               │
│  - Si el cliente envía un mensaje → branch de read_line      │
│  - Si llega un broadcast → branch de rx.recv                 │
│  - Ambos pueden ocurrir en cualquier orden                   │
│  - select! los maneja sin blocking                           │
└──────────────────────────────────────────────────────────────┘
```

---

## El servidor completo

### Versión funcional mínima

```rust
// src/main.rs
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::broadcast;
use std::net::SocketAddr;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

#[tokio::main]
async fn main() -> Result<()> {
    let addr = "127.0.0.1:8080";
    let listener = TcpListener::bind(addr).await?;
    println!("Chat server running on {}", addr);

    let (tx, _rx) = broadcast::channel::<(SocketAddr, String)>(256);

    loop {
        let (socket, addr) = listener.accept().await?;
        let tx = tx.clone();
        let rx = tx.subscribe();

        tokio::spawn(async move {
            if let Err(e) = handle_client(socket, addr, tx, rx).await {
                eprintln!("[{}] error: {}", addr, e);
            }
        });
    }
}

async fn handle_client(
    socket: TcpStream,
    addr: SocketAddr,
    tx: broadcast::Sender<(SocketAddr, String)>,
    mut rx: broadcast::Receiver<(SocketAddr, String)>,
) -> Result<()> {
    let (reader, mut writer) = socket.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    // Announce connection
    let _ = tx.send((addr, format!("*** {} has joined ***", addr)));
    writer.write_all(b"Welcome to the chat! Type and press Enter.\n").await?;

    loop {
        tokio::select! {
            // Read from client
            result = reader.read_line(&mut line) => {
                match result {
                    Ok(0) => break,  // Disconnected
                    Ok(_) => {
                        let msg = line.trim().to_string();
                        if !msg.is_empty() {
                            let _ = tx.send((addr, msg));
                        }
                        line.clear();
                    }
                    Err(e) => {
                        eprintln!("[{}] read error: {}", addr, e);
                        break;
                    }
                }
            }

            // Receive broadcast from other clients
            result = rx.recv() => {
                match result {
                    Ok((sender_addr, msg)) => {
                        if sender_addr != addr {
                            let formatted = if msg.starts_with("***") {
                                format!("{}\n", msg)
                            } else {
                                format!("{}: {}\n", sender_addr, msg)
                            };
                            if writer.write_all(formatted.as_bytes()).await.is_err() {
                                break;  // Write failed, client gone
                            }
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        let msg = format!("[Missed {} messages]\n", n);
                        let _ = writer.write_all(msg.as_bytes()).await;
                    }
                    Err(broadcast::error::RecvError::Closed) => break,
                }
            }
        }
    }

    // Announce disconnection
    let _ = tx.send((addr, format!("*** {} has left ***", addr)));
    Ok(())
}
```

### Probar el servidor

```bash
# Terminal 1: servidor
cargo run

# Terminal 2: cliente A
nc 127.0.0.1 8080
# Escribe: Hello from A

# Terminal 3: cliente B
nc 127.0.0.1 8080
# Recibe: 127.0.0.1:54321: Hello from A
# Escribe: Hello from B

# Terminal 2 (cliente A):
# Recibe: 127.0.0.1:54322: Hello from B
```

### Diagrama de flujo completo

```
┌──────────────────────────────────────────────────────────────┐
│  Flujo de un mensaje                                         │
│                                                              │
│  Client A                Server                Client B      │
│  ─────────               ──────                ─────────     │
│  │                       │                     │             │
│  │ "Hello\n"             │                     │             │
│  │──────────────────────▶│                     │             │
│  │         TCP write     │ read_line()         │             │
│  │                       │ returns "Hello"     │             │
│  │                       │                     │             │
│  │                       │ tx.send(            │             │
│  │                       │   (addr_a, "Hello"))│             │
│  │                       │         │           │             │
│  │                       │         ▼           │             │
│  │                       │    broadcast chan   │             │
│  │                       │    ┌─────────┐      │             │
│  │                       │    │ rx_a    │←skip │             │
│  │                       │    │ rx_b ───┼──────┼──▶          │
│  │                       │    └─────────┘      │ "addr_a:   │
│  │                       │                     │  Hello\n"   │
│  │                       │                     │ write_all() │
└──────────────────────────────────────────────────────────────┘
```

---

## Testing del servidor

### Test de conexión básica

```rust
#[cfg(test)]
mod tests {
    use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
    use tokio::net::TcpStream;
    use tokio::time::{sleep, Duration};

    async fn start_test_server() -> u16 {
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();

        let (tx, _rx) = tokio::sync::broadcast::channel::<(std::net::SocketAddr, String)>(256);

        tokio::spawn(async move {
            loop {
                let (socket, addr) = listener.accept().await.unwrap();
                let tx = tx.clone();
                let rx = tx.subscribe();
                tokio::spawn(async move {
                    let _ = super::handle_client(socket, addr, tx, rx).await;
                });
            }
        });

        port
    }

    #[tokio::test]
    async fn test_connect_and_receive_welcome() {
        let port = start_test_server().await;
        sleep(Duration::from_millis(50)).await;

        let stream = TcpStream::connect(format!("127.0.0.1:{}", port))
            .await.unwrap();
        let mut reader = BufReader::new(stream);
        let mut line = String::new();

        reader.read_line(&mut line).await.unwrap();
        assert!(line.contains("Welcome"));
    }

    #[tokio::test]
    async fn test_two_clients_communicate() {
        let port = start_test_server().await;
        sleep(Duration::from_millis(50)).await;

        // Client A
        let stream_a = TcpStream::connect(format!("127.0.0.1:{}", port))
            .await.unwrap();
        let (reader_a, mut writer_a) = stream_a.into_split();
        let mut reader_a = BufReader::new(reader_a);
        let mut buf_a = String::new();
        reader_a.read_line(&mut buf_a).await.unwrap();  // Welcome
        buf_a.clear();
        reader_a.read_line(&mut buf_a).await.unwrap();  // Join announcement
        buf_a.clear();

        // Client B
        let stream_b = TcpStream::connect(format!("127.0.0.1:{}", port))
            .await.unwrap();
        let (reader_b, mut writer_b) = stream_b.into_split();
        let mut reader_b = BufReader::new(reader_b);
        let mut buf_b = String::new();
        reader_b.read_line(&mut buf_b).await.unwrap();  // Welcome
        buf_b.clear();

        // Wait for join announcements
        sleep(Duration::from_millis(50)).await;

        // A sends a message
        writer_a.write_all(b"Hello from A\n").await.unwrap();
        sleep(Duration::from_millis(50)).await;

        // B should receive it
        buf_b.clear();
        reader_b.read_line(&mut buf_b).await.unwrap();
        assert!(buf_b.contains("Hello from A"), "B got: {}", buf_b);
    }
}
```

### Bind al puerto 0

```rust
// Puerto 0: el OS asigna un puerto libre automáticamente
let listener = TcpListener::bind("127.0.0.1:0").await?;
let port = listener.local_addr()?.port();
// Ideal para tests: sin conflictos de puerto
```

---

## Errores comunes

### 1. No usar `tokio::spawn` para cada conexión

```rust
// ❌ Secuencial: un cliente bloquea a todos
loop {
    let (socket, addr) = listener.accept().await?;
    handle_client(socket).await;  // Bloquea el loop
}

// ✅ Spawn: todas las conexiones corren en paralelo
loop {
    let (socket, addr) = listener.accept().await?;
    tokio::spawn(async move {
        handle_client(socket).await;
    });
}
```

### 2. No manejar la desconexión del cliente

```rust
// ❌ Ignorar bytes_read == 0 causa loop infinito
loop {
    let bytes = reader.read_line(&mut line).await?;
    // Si bytes == 0, el cliente se desconectó
    // Pero el loop sigue ejecutándose con líneas vacías
    process_line(&line);
    line.clear();
}

// ✅ Detectar EOF y salir del loop
loop {
    line.clear();
    let bytes = reader.read_line(&mut line).await?;
    if bytes == 0 {
        break;  // Cliente cerró la conexión
    }
    process_line(line.trim());
}
```

### 3. Broadcast al remitente (eco no deseado)

```rust
// ❌ El remitente recibe su propio mensaje
let msg = format!("{}: {}", addr, line);
tx.send(msg.clone());
// Todos los rx lo reciben, incluyendo el del remitente

// ✅ Incluir el SocketAddr y filtrar
tx.send((addr, line));
// En el receptor:
if sender_addr != my_addr {
    writer.write_all(...).await?;
}
```

### 4. Ignorar RecvError::Lagged

```rust
// ❌ Si el receptor es lento, se pierden mensajes silenciosamente
match rx.recv().await {
    Ok(msg) => { /* procesar */ }
    Err(_) => break,  // Tanto Lagged como Closed causan break
}

// ✅ Manejar Lagged informando al usuario
match rx.recv().await {
    Ok(msg) => { /* procesar */ }
    Err(broadcast::error::RecvError::Lagged(n)) => {
        // El receptor perdió n mensajes porque es lento
        notify_user("Lost {} messages", n);
        // Continuar, no salir
    }
    Err(broadcast::error::RecvError::Closed) => break,
}
```

### 5. No hacer `async move` en el spawn

```rust
// ❌ No compila: socket se toma prestado, no movido
let tx = tx.clone();
tokio::spawn(async {
    handle_client(socket, tx).await;
    // Error: socket y tx no son 'static
});

// ✅ async move: transfiere ownership
let tx = tx.clone();
let rx = tx.subscribe();
tokio::spawn(async move {
    handle_client(socket, tx, rx).await;
});
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          SERVIDOR TOKIO CHEATSHEET                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SETUP                                                       │
│  tokio = { version="1", features=["full"] }                  │
│  #[tokio::main]                      Runtime async           │
│  async fn main() -> Result<()>       Entry point             │
│                                                              │
│  TCP LISTENER                                                │
│  TcpListener::bind("addr:port").await?   Crear listener      │
│  listener.accept().await?                Aceptar conexión     │
│  TcpListener::bind("127.0.0.1:0")       Puerto aleatorio     │
│                                                              │
│  SPAWN                                                       │
│  tokio::spawn(async move { ... })    Task independiente      │
│  Requiere: Send + 'static                                    │
│  async move { ... }                  Mover variables         │
│                                                              │
│  I/O ASYNC                                                   │
│  socket.into_split()                 → (ReadHalf, WriteHalf) │
│  BufReader::new(read_half)           Lectura buffered        │
│  reader.read_line(&mut buf).await?   Leer una línea          │
│  writer.write_all(bytes).await?      Escribir bytes          │
│  bytes_read == 0 → EOF              Cliente desconectado     │
│                                                              │
│  BROADCAST CHANNEL                                           │
│  broadcast::channel::<T>(capacity)   Crear canal             │
│  tx.clone()                          Clonar sender           │
│  tx.subscribe()                      Nuevo receiver          │
│  tx.send(msg)                        Enviar a todos          │
│  rx.recv().await                     Recibir mensaje         │
│  RecvError::Lagged(n)                Perdió n mensajes       │
│  RecvError::Closed                   Canal cerrado           │
│                                                              │
│  SELECT                                                      │
│  tokio::select! {                                            │
│      result = future_a => { ... }    Primer future           │
│      result = future_b => { ... }    Segundo future          │
│  }                                   El primero que completa │
│                                                              │
│  TESTING                                                     │
│  #[tokio::test]                      Test async              │
│  bind("127.0.0.1:0")                Puerto libre             │
│  listener.local_addr()?.port()       Obtener puerto          │
│  sleep(Duration::from_millis(N))     Esperar estabilización │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Echo server async

Antes de construir el chat completo, implementa un echo server:

```rust
// El servidor devuelve cada línea que recibe, en mayúsculas
// "hello\n" → "HELLO\n"
```

**Tareas:**
1. Crea el proyecto con `cargo new echo-server`
2. Implementa un `TcpListener` que acepta conexiones
3. Usa `tokio::spawn` para manejar cada conexión
4. Lee líneas con `BufReader` + `read_line`
5. Convierte a mayúsculas y envía de vuelta con `write_all`
6. Maneja la desconexión (bytes_read == 0)
7. Prueba con `nc 127.0.0.1 8080`

**Pregunta de reflexión:** Si un cliente envía datos pero nunca un `\n`,
`read_line` nunca retorna. ¿Cómo protegerías el servidor contra un cliente
malicioso que envía datos infinitos sin newline? (Hint: `tokio::time::timeout`)

---

### Ejercicio 2: Chat con broadcast

Extiende el echo server a un chat multi-cliente:

**Tareas:**
1. Agrega un `broadcast::channel` compartido
2. Cuando un cliente envía un mensaje, haz `tx.send()` al canal
3. Cada cliente tiene un `rx` que recibe mensajes de los demás
4. Usa `tokio::select!` para manejar lectura del cliente y recepción del broadcast
5. Filtra para que un cliente no reciba su propio mensaje
6. Agrega mensajes de "join" y "leave" cuando un cliente se conecta/desconecta
7. Prueba con 3 terminales usando `nc`

**Pregunta de reflexión:** El `broadcast::channel` tiene un buffer de capacidad
fija (e.g., 256). Si un cliente es muy lento leyendo, su `rx` se llena y
recibe `RecvError::Lagged`. ¿Qué alternativas de diseño existen para manejar
clientes lentos? (drop messages, buffer per-client, disconnect slow clients)

---

### Ejercicio 3: Servidor robusto con graceful shutdown

**Tareas:**
1. Agrega manejo de `Ctrl+C` con `tokio::signal::ctrl_c()`:
   ```rust
   tokio::select! {
       _ = accept_loop(&listener, tx) => {}
       _ = tokio::signal::ctrl_c() => {
           println!("Shutting down...");
       }
   }
   ```
2. Cuando el servidor recibe `Ctrl+C`:
   - Deja de aceptar nuevas conexiones
   - Envía un mensaje "Server shutting down" a todos los clientes
   - Espera a que las tasks existentes terminen (con timeout)
3. Agrega un timeout de lectura: si un cliente no envía nada en 5 minutos,
   desconectarlo
4. Agrega un límite de longitud de línea (e.g., 1024 bytes) para proteger
   contra clientes maliciosos
5. Prueba el shutdown: conecta 2 clientes, presiona Ctrl+C, verifica que
   ambos reciben el mensaje de despedida

**Pregunta de reflexión:** ¿Cuál es la diferencia entre `tokio::spawn` que
retorna un `JoinHandle` (que puedes `.await`) y simplemente "fire and forget"
(ignorar el JoinHandle)? ¿Cuándo importa hacer join de las tasks?