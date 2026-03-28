# Protocolo Custom

## Índice

1. [¿Por qué necesitas un protocolo?](#1-por-qué-necesitas-un-protocolo)
2. [Framing: delimitar mensajes en TCP](#2-framing-delimitar-mensajes-en-tcp)
3. [Length-prefixed framing](#3-length-prefixed-framing)
4. [Serialización con serde](#4-serialización-con-serde)
5. [Protocolo completo: frame + serde](#5-protocolo-completo-frame--serde)
6. [tokio_util::codec](#6-tokio_utilcodec)
7. [Versionado y evolución](#7-versionado-y-evolución)
8. [Patrones avanzados](#8-patrones-avanzados)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Por qué necesitas un protocolo?

TCP es un **stream de bytes**, no un stream de mensajes. Cuando envías dos mensajes seguidos,
TCP puede entregarlos al receptor como:

```
┌─────────── Lo que envías ─────────────┐
│                                       │
│  send("hello")  →  h e l l o          │
│  send("world")  →  w o r l d          │
│                                       │
└───────────────────────────────────────┘

┌─────────── Lo que TCP puede entregar ─────────────────────┐
│                                                            │
│  Caso 1: un solo read()                                    │
│  ┌─────────────────────────┐                               │
│  │ h e l l o w o r l d     │  ← mensajes fusionados       │
│  └─────────────────────────┘                               │
│                                                            │
│  Caso 2: tres reads()                                      │
│  ┌─────────┐ ┌───┐ ┌─────────────┐                        │
│  │ h e l   │ │ l │ │ o w o r l d │  ← mensaje fragmentado │
│  └─────────┘ └───┘ └─────────────┘                        │
│                                                            │
│  Caso 3: exactamente dos reads()                           │
│  ┌───────────┐ ┌───────────┐                               │
│  │ h e l l o │ │ w o r l d │  ← perfecto (no garantizado) │
│  └───────────┘ └───────────┘                               │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

Un **protocolo** define cómo detectar dónde empieza y termina cada mensaje dentro del stream.
Sin protocolo, tu aplicación no puede distinguir un mensaje de otro.

### Las dos capas de un protocolo

```
┌──────────────────────────────────────────────┐
│          Capa de Aplicación                   │
│  ¿QUÉ significan los datos?                  │
│  Tipos de mensajes, comandos, respuestas     │
│  Serialización: JSON, binario, protobuf...   │
├──────────────────────────────────────────────┤
│          Capa de Framing                      │
│  ¿DÓNDE empieza y termina cada mensaje?      │
│  Delimitadores, longitud prefijada, etc.     │
├──────────────────────────────────────────────┤
│          TCP                                  │
│  Stream de bytes sin estructura              │
└──────────────────────────────────────────────┘
```

---

## 2. Framing: delimitar mensajes en TCP

Existen tres estrategias principales para delimitar mensajes.

### Estrategia 1: delimitador (newline, null byte)

Cada mensaje termina con un carácter especial (comúnmente `\n` o `\0`):

```
┌──── Stream TCP con delimitador \n ────────────────┐
│                                                    │
│  H E L L O \n W O R L D \n B Y E \n               │
│  ─────────    ─────────    ─────                   │
│  mensaje 1    mensaje 2    mensaje 3               │
│                                                    │
└────────────────────────────────────────────────────┘
```

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

async fn read_line_delimited(stream: TcpStream) -> tokio::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;
        if n == 0 { break; }  // EOF

        let message = line.trim_end();
        println!("Message: {:?}", message);

        writer.write_all(b"OK\n").await?;
    }

    Ok(())
}
```

**Ventajas**: simple, legible con herramientas como `nc` o `telnet`.

**Desventajas**: el delimitador no puede aparecer dentro del mensaje (o necesitas escape),
y no puedes enviar datos binarios arbitrarios.

### Estrategia 2: longitud prefijada (length-prefixed)

Cada mensaje va precedido por su longitud en bytes:

```
┌──── Stream TCP con length-prefix (4 bytes BE) ───────────────────┐
│                                                                   │
│  [00 00 00 05] H E L L O [00 00 00 05] W O R L D                 │
│   ───────────  ─────────  ───────────  ─────────                  │
│   len=5        payload    len=5        payload                    │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

```rust
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

async fn read_length_prefixed(stream: &mut TcpStream) -> tokio::io::Result<Vec<u8>> {
    // Leer 4 bytes de longitud (big-endian u32)
    let len = stream.read_u32().await? as usize;

    // Protección contra mensajes absurdamente grandes
    const MAX_MSG_SIZE: usize = 16 * 1024 * 1024;  // 16 MB
    if len > MAX_MSG_SIZE {
        return Err(tokio::io::Error::new(
            tokio::io::ErrorKind::InvalidData,
            format!("message too large: {} bytes", len),
        ));
    }

    // Leer exactamente len bytes
    let mut buf = vec![0u8; len];
    stream.read_exact(&mut buf).await?;

    Ok(buf)
}

async fn write_length_prefixed(
    stream: &mut TcpStream,
    data: &[u8],
) -> tokio::io::Result<()> {
    stream.write_u32(data.len() as u32).await?;
    stream.write_all(data).await?;
    Ok(())
}
```

**Ventajas**: soporta datos binarios, sin ambigüedad, se conoce el tamaño antes de leer.

**Desventajas**: no es legible con herramientas de texto plano, necesitas decidir el tamaño
del campo de longitud (u16, u32, u64).

### Estrategia 3: formato fijo (fixed-length)

Todos los mensajes tienen la misma longitud:

```rust
use tokio::io::AsyncReadExt;
use tokio::net::TcpStream;

const MSG_SIZE: usize = 64;

async fn read_fixed(stream: &mut TcpStream) -> tokio::io::Result<[u8; MSG_SIZE]> {
    let mut buf = [0u8; MSG_SIZE];
    stream.read_exact(&mut buf).await?;
    Ok(buf)
}
```

Raramente usado en la práctica, pero útil para protocolos binarios muy específicos.

### Comparación

| Estrategia      | Datos binarios | Legible | Tamaño conocido antes de leer | Complejidad |
|-----------------|:--------------:|:-------:|:-----------------------------:|:-----------:|
| Delimitador     | No             | Sí      | No                            | Baja        |
| Length-prefix   | Sí             | No      | Sí                            | Media       |
| Longitud fija   | Sí             | No      | Sí (fijo)                     | Baja        |

---

## 3. Length-prefixed framing

Es la estrategia más usada en protocolos reales. Vamos a construir un módulo completo.

### Formato del frame

```
┌─────── Estructura de un frame ────────────────────┐
│                                                    │
│  ┌──────────────┬────────────────────────────┐     │
│  │ Header       │ Payload                    │     │
│  │ (4 bytes)    │ (N bytes)                  │     │
│  │ N en u32 BE  │ datos del mensaje          │     │
│  └──────────────┴────────────────────────────┘     │
│                                                    │
│  Tamaño total del frame: 4 + N bytes               │
│                                                    │
│  Ejemplo: mensaje "HELLO" (5 bytes)                │
│  [00 00 00 05] [48 45 4C 4C 4F]                    │
│   header        payload                            │
│                                                    │
└────────────────────────────────────────────────────┘
```

### Implementación robusta

```rust
use tokio::io::{self, AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

/// Maximum message size: 16 MB
const MAX_FRAME_SIZE: u32 = 16 * 1024 * 1024;

/// Read a single length-prefixed frame from the stream.
/// Returns None on EOF (clean disconnect).
pub async fn read_frame<R: AsyncRead + Unpin>(
    reader: &mut R,
) -> io::Result<Option<Vec<u8>>> {
    // Read the 4-byte length header
    let len = match reader.read_u32().await {
        Ok(len) => len,
        Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => {
            // Clean EOF: the other side closed the connection
            return Ok(None);
        }
        Err(e) => return Err(e),
    };

    // Validate size
    if len > MAX_FRAME_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("frame too large: {} bytes (max {})", len, MAX_FRAME_SIZE),
        ));
    }

    // Read the payload
    let mut buf = vec![0u8; len as usize];
    reader.read_exact(&mut buf).await?;

    Ok(Some(buf))
}

/// Write a single length-prefixed frame to the stream.
pub async fn write_frame<W: AsyncWrite + Unpin>(
    writer: &mut W,
    data: &[u8],
) -> io::Result<()> {
    let len = data.len();
    if len > MAX_FRAME_SIZE as usize {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            format!("payload too large: {} bytes", len),
        ));
    }

    writer.write_u32(len as u32).await?;
    writer.write_all(data).await?;
    writer.flush().await?;

    Ok(())
}
```

### Echo server con frames

```rust
use tokio::net::TcpListener;

// Asumiendo read_frame y write_frame del bloque anterior

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Frame echo server on :8080");

    loop {
        let (stream, addr) = listener.accept().await?;

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            let (mut reader, mut writer) = stream.into_split();

            loop {
                match read_frame(&mut reader).await {
                    Ok(Some(data)) => {
                        println!("[{}] frame: {} bytes", addr, data.len());
                        if let Err(e) = write_frame(&mut writer, &data).await {
                            eprintln!("[{}] write error: {}", addr, e);
                            break;
                        }
                    }
                    Ok(None) => {
                        println!("[{}] disconnected", addr);
                        break;
                    }
                    Err(e) => {
                        eprintln!("[{}] read error: {}", addr, e);
                        break;
                    }
                }
            }
        });
    }
}
```

### Cliente que envía frames

```rust
use tokio::net::TcpStream;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut stream = TcpStream::connect("127.0.0.1:8080").await?;

    // Enviar varios mensajes
    let messages = vec!["Hello, server!", "How are you?", "Goodbye!"];

    for msg in &messages {
        write_frame(&mut stream, msg.as_bytes()).await?;
        println!("Sent: {:?}", msg);
    }

    // Leer respuestas
    let (mut reader, _) = stream.into_split();
    for _ in 0..messages.len() {
        if let Some(data) = read_frame(&mut reader).await? {
            println!("Received: {:?}", String::from_utf8_lossy(&data));
        }
    }

    Ok(())
}
```

---

## 4. Serialización con serde

`serde` es el framework de serialización estándar de Rust. Permite convertir structs a bytes
(serialización) y bytes a structs (deserialización) usando múltiples formatos.

### Setup en Cargo.toml

```toml
[dependencies]
serde = { version = "1", features = ["derive"] }
serde_json = "1"           # Formato JSON (texto, legible)
bincode = "1"              # Formato binario (compacto, rápido)
# rmp-serde = "1"          # MessagePack (binario, interoperable)
```

### Derive básico

```rust
use serde::{Serialize, Deserialize};

#[derive(Debug, Serialize, Deserialize)]
struct UserInfo {
    username: String,
    age: u32,
    active: bool,
}

fn main() {
    let user = UserInfo {
        username: "alice".to_string(),
        age: 30,
        active: true,
    };

    // Serializar a JSON
    let json = serde_json::to_string(&user).unwrap();
    println!("JSON: {}", json);
    // {"username":"alice","age":30,"active":true}

    // Deserializar desde JSON
    let parsed: UserInfo = serde_json::from_str(&json).unwrap();
    println!("Parsed: {:?}", parsed);

    // Serializar a bincode (binario compacto)
    let bytes = bincode::serialize(&user).unwrap();
    println!("Bincode: {} bytes", bytes.len());  // ~20 bytes vs ~45 JSON

    // Deserializar desde bincode
    let parsed: UserInfo = bincode::deserialize(&bytes).unwrap();
    println!("Parsed: {:?}", parsed);
}
```

### Enums como mensajes de protocolo

Los enums son ideales para definir los tipos de mensajes de un protocolo:

```rust
use serde::{Serialize, Deserialize};

/// Messages from client to server
#[derive(Debug, Serialize, Deserialize)]
enum Request {
    Ping,
    Echo { message: String },
    Get { key: String },
    Set { key: String, value: String },
    Delete { key: String },
    List,
}

/// Messages from server to client
#[derive(Debug, Serialize, Deserialize)]
enum Response {
    Pong,
    Echo { message: String },
    Value(Option<String>),
    Ok,
    Error { message: String },
    Keys(Vec<String>),
}
```

Serde serializa enums de forma que incluye el nombre de la variante, lo que actúa como
discriminador del tipo de mensaje:

```rust
fn main() {
    let req = Request::Set {
        key: "name".to_string(),
        value: "Alice".to_string(),
    };

    // JSON (legible para debugging)
    let json = serde_json::to_string(&req).unwrap();
    println!("{}", json);
    // {"Set":{"key":"name","value":"Alice"}}

    // Bincode (eficiente para producción)
    let bytes = bincode::serialize(&req).unwrap();
    println!("{} bytes", bytes.len());  // ~21 bytes
}
```

### JSON vs Bincode vs MessagePack

```
┌──────────────────────────────────────────────────────────────┐
│                Comparación de formatos                        │
├──────────┬───────────┬───────────┬───────────────────────────┤
│          │   JSON    │  Bincode  │  MessagePack              │
├──────────┼───────────┼───────────┼───────────────────────────┤
│ Tipo     │ Texto     │ Binario   │ Binario                   │
│ Legible  │ Sí        │ No        │ No                        │
│ Tamaño   │ Grande    │ Compacto  │ Compacto                  │
│ Velocidad│ Moderada  │ Muy rápida│ Rápida                    │
│ Schema   │ Flexible  │ Rust-only │ Interoperable             │
│ Interop  │ Universal │ Rust↔Rust │ Muchos lenguajes          │
│ Errores  │ Claros    │ Crípticos │ Claros                    │
│ Uso      │ APIs, log │ IPC Rust  │ Cross-language, storage   │
└──────────┴───────────┴───────────┴───────────────────────────┘
```

**Regla práctica**: usa JSON para prototipos y debugging, bincode para comunicación
Rust↔Rust en producción, MessagePack cuando necesitas interoperabilidad con otros lenguajes.

---

## 5. Protocolo completo: frame + serde

Aquí combinamos length-prefixed framing con serialización serde para crear un protocolo
completo y robusto.

### Arquitectura del protocolo

```
┌─── Enviar mensaje ────────────────────────────────────────┐
│                                                            │
│  Request::Set { key, value }                               │
│       │                                                    │
│       ▼                                                    │
│  bincode::serialize(&request)  →  [bytes del mensaje]      │
│       │                                                    │
│       ▼                                                    │
│  write_frame(stream, &bytes)                               │
│       │                                                    │
│       ▼                                                    │
│  [00 00 00 15] [serialized bytes...]   →  TCP stream       │
│   length        payload                                    │
│                                                            │
└────────────────────────────────────────────────────────────┘

┌─── Recibir mensaje ───────────────────────────────────────┐
│                                                            │
│  TCP stream  →  [00 00 00 15] [serialized bytes...]        │
│       │                                                    │
│       ▼                                                    │
│  read_frame(stream)  →  Some([bytes del mensaje])          │
│       │                                                    │
│       ▼                                                    │
│  bincode::deserialize(&bytes)  →  Request::Set { key, v }  │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Módulo de protocolo

```rust
// protocol.rs
use serde::{Serialize, Deserialize, de::DeserializeOwned};
use tokio::io::{self, AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

const MAX_FRAME_SIZE: u32 = 16 * 1024 * 1024;

/// Messages from client to server.
#[derive(Debug, Serialize, Deserialize)]
pub enum Request {
    Ping,
    Get { key: String },
    Set { key: String, value: String },
    Delete { key: String },
    ListKeys,
}

/// Messages from server to client.
#[derive(Debug, Serialize, Deserialize)]
pub enum Response {
    Pong,
    Value(Option<String>),
    Ok,
    Error(String),
    Keys(Vec<String>),
}

/// Send a serializable message as a length-prefixed frame.
pub async fn send_message<W, M>(writer: &mut W, msg: &M) -> io::Result<()>
where
    W: AsyncWrite + Unpin,
    M: Serialize,
{
    let bytes = bincode::serialize(msg).map_err(|e| {
        io::Error::new(io::ErrorKind::InvalidData, e)
    })?;

    if bytes.len() > MAX_FRAME_SIZE as usize {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "message too large",
        ));
    }

    writer.write_u32(bytes.len() as u32).await?;
    writer.write_all(&bytes).await?;
    writer.flush().await?;

    Ok(())
}

/// Receive a length-prefixed frame and deserialize it.
/// Returns None on clean EOF.
pub async fn recv_message<R, M>(reader: &mut R) -> io::Result<Option<M>>
where
    R: AsyncRead + Unpin,
    M: DeserializeOwned,
{
    let len = match reader.read_u32().await {
        Ok(len) => len,
        Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => {
            return Ok(None);
        }
        Err(e) => return Err(e),
    };

    if len > MAX_FRAME_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("frame too large: {} bytes", len),
        ));
    }

    let mut buf = vec![0u8; len as usize];
    reader.read_exact(&mut buf).await?;

    let msg = bincode::deserialize(&buf).map_err(|e| {
        io::Error::new(io::ErrorKind::InvalidData, e)
    })?;

    Ok(Some(msg))
}
```

### Servidor usando el protocolo

```rust
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use tokio::net::TcpListener;

// Usar Request, Response, send_message, recv_message del módulo protocol

type Store = Arc<Mutex<HashMap<String, String>>>;

async fn handle_client(
    stream: tokio::net::TcpStream,
    store: Store,
) -> tokio::io::Result<()> {
    let (mut reader, mut writer) = stream.into_split();

    loop {
        let request: Option<Request> = recv_message(&mut reader).await?;

        let request = match request {
            Some(r) => r,
            None => break,  // Client disconnected
        };

        let response = process_request(&request, &store);
        send_message(&mut writer, &response).await?;
    }

    Ok(())
}

fn process_request(request: &Request, store: &Store) -> Response {
    match request {
        Request::Ping => Response::Pong,

        Request::Get { key } => {
            let map = store.lock().unwrap();
            Response::Value(map.get(key).cloned())
        }

        Request::Set { key, value } => {
            let mut map = store.lock().unwrap();
            map.insert(key.clone(), value.clone());
            Response::Ok
        }

        Request::Delete { key } => {
            let mut map = store.lock().unwrap();
            if map.remove(key).is_some() {
                Response::Ok
            } else {
                Response::Error(format!("key not found: {}", key))
            }
        }

        Request::ListKeys => {
            let map = store.lock().unwrap();
            let keys: Vec<String> = map.keys().cloned().collect();
            Response::Keys(keys)
        }
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    let store: Store = Arc::new(Mutex::new(HashMap::new()));

    println!("KV server on :8080");

    loop {
        let (stream, addr) = listener.accept().await?;
        let store = Arc::clone(&store);

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            if let Err(e) = handle_client(stream, store).await {
                eprintln!("[{}] error: {}", addr, e);
            }
            println!("[{}] disconnected", addr);
        });
    }
}
```

### Cliente usando el protocolo

```rust
use tokio::net::TcpStream;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let stream = TcpStream::connect("127.0.0.1:8080").await?;
    let (mut reader, mut writer) = stream.into_split();

    // Ping
    send_message(&mut writer, &Request::Ping).await?;
    let resp: Response = recv_message(&mut reader).await?.unwrap();
    println!("Ping -> {:?}", resp);  // Pong

    // Set
    send_message(&mut writer, &Request::Set {
        key: "name".to_string(),
        value: "Alice".to_string(),
    }).await?;
    let resp: Response = recv_message(&mut reader).await?.unwrap();
    println!("Set -> {:?}", resp);  // Ok

    // Get
    send_message(&mut writer, &Request::Get {
        key: "name".to_string(),
    }).await?;
    let resp: Response = recv_message(&mut reader).await?.unwrap();
    println!("Get -> {:?}", resp);  // Value(Some("Alice"))

    // List
    send_message(&mut writer, &Request::ListKeys).await?;
    let resp: Response = recv_message(&mut reader).await?.unwrap();
    println!("List -> {:?}", resp);  // Keys(["name"])

    Ok(())
}
```

---

## 6. tokio_util::codec

`tokio_util` proporciona una abstracción de alto nivel para framing: `Encoder` + `Decoder` +
`Framed`. En lugar de manejar reads parciales manualmente, defines cómo codificar/decodificar
y `Framed` se encarga del buffering.

### Cargo.toml

```toml
[dependencies]
tokio = { version = "1", features = ["full"] }
tokio-util = { version = "0.7", features = ["codec"] }
bytes = "1"
serde = { version = "1", features = ["derive"] }
bincode = "1"
```

### Implementar Encoder y Decoder

```rust
use bytes::{Buf, BufMut, BytesMut};
use serde::{Serialize, de::DeserializeOwned};
use tokio_util::codec::{Decoder, Encoder};
use std::marker::PhantomData;

const MAX_FRAME_SIZE: usize = 16 * 1024 * 1024;

/// A codec that frames messages using 4-byte length prefix
/// and serializes/deserializes with bincode.
pub struct LengthCodec<Enc, Dec> {
    _enc: PhantomData<Enc>,
    _dec: PhantomData<Dec>,
}

impl<Enc, Dec> LengthCodec<Enc, Dec> {
    pub fn new() -> Self {
        Self {
            _enc: PhantomData,
            _dec: PhantomData,
        }
    }
}

impl<Enc, Dec: DeserializeOwned> Decoder for LengthCodec<Enc, Dec> {
    type Item = Dec;
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {
        // Need at least 4 bytes for the length header
        if src.len() < 4 {
            return Ok(None);  // Not enough data yet
        }

        // Peek at the length without consuming
        let len = u32::from_be_bytes([src[0], src[1], src[2], src[3]]) as usize;

        if len > MAX_FRAME_SIZE {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                format!("frame too large: {}", len),
            ));
        }

        // Check if we have the full frame
        if src.len() < 4 + len {
            // Reserve space for the rest
            src.reserve(4 + len - src.len());
            return Ok(None);  // Not enough data yet
        }

        // Consume the length header
        src.advance(4);

        // Consume the payload
        let payload = src.split_to(len);

        // Deserialize
        let msg = bincode::deserialize(&payload).map_err(|e| {
            std::io::Error::new(std::io::ErrorKind::InvalidData, e)
        })?;

        Ok(Some(msg))
    }
}

impl<Enc: Serialize, Dec> Encoder<Enc> for LengthCodec<Enc, Dec> {
    type Error = std::io::Error;

    fn encode(&mut self, item: Enc, dst: &mut BytesMut) -> Result<(), Self::Error> {
        let bytes = bincode::serialize(&item).map_err(|e| {
            std::io::Error::new(std::io::ErrorKind::InvalidData, e)
        })?;

        if bytes.len() > MAX_FRAME_SIZE {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                "message too large",
            ));
        }

        // Write length header + payload
        dst.reserve(4 + bytes.len());
        dst.put_u32(bytes.len() as u32);
        dst.put_slice(&bytes);

        Ok(())
    }
}
```

### Usar Framed con el codec

```rust
use tokio::net::{TcpListener, TcpStream};
use tokio_util::codec::Framed;
use futures::{SinkExt, StreamExt};  // Para .send() y .next()
use serde::{Serialize, Deserialize};

// Cargo.toml: futures = "0.3"

#[derive(Debug, Serialize, Deserialize)]
enum Request {
    Ping,
    Echo(String),
}

#[derive(Debug, Serialize, Deserialize)]
enum Response {
    Pong,
    Echo(String),
}

async fn handle_client(stream: TcpStream) -> std::io::Result<()> {
    // Framed envuelve el stream con nuestro codec
    let mut framed = Framed::new(
        stream,
        LengthCodec::<Response, Request>::new(),
    );

    // StreamExt::next() lee el siguiente frame decodificado
    while let Some(result) = framed.next().await {
        let request = result?;

        let response = match request {
            Request::Ping => Response::Pong,
            Request::Echo(msg) => Response::Echo(msg),
        };

        // SinkExt::send() codifica y envía el frame
        framed.send(response).await?;
    }

    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;

    loop {
        let (stream, addr) = listener.accept().await?;
        tokio::spawn(async move {
            println!("[{}] connected", addr);
            if let Err(e) = handle_client(stream).await {
                eprintln!("[{}] error: {}", addr, e);
            }
        });
    }
}
```

### Codecs built-in de tokio_util

`tokio_util::codec` incluye codecs listos para usar:

```rust
use tokio_util::codec::{LinesCodec, BytesCodec, LengthDelimitedCodec, Framed};
use tokio::net::TcpStream;
use futures::{SinkExt, StreamExt};

async fn with_lines(stream: TcpStream) -> std::io::Result<()> {
    // LinesCodec: delimitador \n, decodifica a String
    let mut framed = Framed::new(stream, LinesCodec::new());

    while let Some(result) = framed.next().await {
        let line = result?;
        println!("Line: {}", line);
        framed.send(format!("echo: {}", line)).await?;
    }

    Ok(())
}

async fn with_length_delimited(stream: TcpStream) -> std::io::Result<()> {
    // LengthDelimitedCodec: length-prefix configurable
    let codec = LengthDelimitedCodec::builder()
        .length_field_length(4)          // 4 bytes para la longitud
        .max_frame_length(16 * 1024 * 1024)  // 16 MB máximo
        .new_codec();

    let mut framed = Framed::new(stream, codec);

    // Decodifica a BytesMut (bytes crudos, sin serde)
    while let Some(result) = framed.next().await {
        let bytes = result?;
        println!("Frame: {} bytes", bytes.len());

        // Para usar con serde, deserializar manualmente:
        // let msg: Request = bincode::deserialize(&bytes).unwrap();
    }

    Ok(())
}
```

```
┌──── Comparación: manual vs codec ─────────────────────────────────┐
│                                                                    │
│  Manual (read_frame/write_frame):                                  │
│  + Simple, sin dependencias extra                                  │
│  + Fácil de entender                                               │
│  - Buffering manual                                                │
│  - Más código repetitivo                                           │
│                                                                    │
│  Codec (Framed + Encoder/Decoder):                                 │
│  + Abstracción limpia (Stream + Sink)                              │
│  + Buffering automático (BytesMut)                                 │
│  + Composable con otros adaptadores de futures                     │
│  - Más boilerplate inicial                                         │
│  - Dependencia de tokio-util, bytes, futures                       │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## 7. Versionado y evolución

Los protocolos evolucionan. Necesitas que versiones nuevas del servidor acepten clientes
antiguos (y viceversa).

### Versionado en el handshake

```rust
use serde::{Serialize, Deserialize};

const PROTOCOL_MAGIC: u32 = 0x4D595356;  // "MYSV" en ASCII
const PROTOCOL_VERSION: u32 = 2;

#[derive(Debug, Serialize, Deserialize)]
struct Handshake {
    magic: u32,
    version: u32,
    client_name: String,
}

#[derive(Debug, Serialize, Deserialize)]
struct HandshakeResponse {
    accepted: bool,
    server_version: u32,
    message: String,
}
```

```rust
use tokio::net::TcpStream;

// Usar send_message/recv_message del módulo protocol

async fn client_handshake(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
    writer: &mut (impl tokio::io::AsyncWrite + Unpin),
) -> std::io::Result<()> {
    let handshake = Handshake {
        magic: PROTOCOL_MAGIC,
        version: PROTOCOL_VERSION,
        client_name: "my-client".to_string(),
    };

    send_message(writer, &handshake).await?;

    let response: HandshakeResponse = recv_message(reader).await?
        .ok_or_else(|| std::io::Error::new(
            std::io::ErrorKind::ConnectionAborted,
            "server closed during handshake",
        ))?;

    if !response.accepted {
        return Err(std::io::Error::new(
            std::io::ErrorKind::ConnectionRefused,
            response.message,
        ));
    }

    println!("Connected to server v{}", response.server_version);
    Ok(())
}

async fn server_handshake(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
    writer: &mut (impl tokio::io::AsyncWrite + Unpin),
) -> std::io::Result<u32> {
    let handshake: Handshake = recv_message(reader).await?
        .ok_or_else(|| std::io::Error::new(
            std::io::ErrorKind::ConnectionAborted,
            "client closed during handshake",
        ))?;

    // Verify magic number
    if handshake.magic != PROTOCOL_MAGIC {
        let resp = HandshakeResponse {
            accepted: false,
            server_version: PROTOCOL_VERSION,
            message: "invalid protocol".to_string(),
        };
        send_message(writer, &resp).await?;
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "invalid magic number",
        ));
    }

    // Version compatibility (accept v1 and v2)
    if handshake.version < 1 || handshake.version > PROTOCOL_VERSION {
        let resp = HandshakeResponse {
            accepted: false,
            server_version: PROTOCOL_VERSION,
            message: format!(
                "unsupported version {} (supported: 1-{})",
                handshake.version, PROTOCOL_VERSION
            ),
        };
        send_message(writer, &resp).await?;
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "version mismatch",
        ));
    }

    let resp = HandshakeResponse {
        accepted: true,
        server_version: PROTOCOL_VERSION,
        message: format!("welcome, {}", handshake.client_name),
    };
    send_message(writer, &resp).await?;

    Ok(handshake.version)
}
```

### Evolución compatible con serde

Serde permite evolucionar structs sin romper compatibilidad:

```rust
use serde::{Serialize, Deserialize};

// Versión 1 del protocolo
#[derive(Debug, Serialize, Deserialize)]
enum RequestV1 {
    Ping,
    Get { key: String },
    Set { key: String, value: String },
}

// Versión 2: nuevos comandos, campos opcionales
#[derive(Debug, Serialize, Deserialize)]
enum Request {
    Ping,
    Get { key: String },
    Set {
        key: String,
        value: String,
        #[serde(default)]  // Si no está presente, usa Default (0)
        ttl_secs: u64,
    },
    Delete { key: String },        // Nuevo en v2
    Subscribe { pattern: String }, // Nuevo en v2
}
```

> **Cuidado con bincode**: bincode no usa nombres de campo, serializa por posición. Agregar
> un campo al medio de un struct rompe la compatibilidad. JSON y MessagePack son más
> tolerantes porque usan nombres. Si usas bincode, agrega campos nuevos siempre al final y
> usa `#[serde(default)]`.

### Manejo de mensajes desconocidos

```rust
use serde::{Serialize, Deserialize};

#[derive(Debug, Serialize, Deserialize)]
#[serde(tag = "type")]
enum Message {
    Ping,
    Pong,
    Data { payload: Vec<u8> },

    // Catch-all para mensajes de versiones futuras
    #[serde(other)]
    Unknown,
}

fn handle_message(msg: Message) {
    match msg {
        Message::Ping => println!("Got ping"),
        Message::Pong => println!("Got pong"),
        Message::Data { payload } => println!("Got {} bytes", payload.len()),
        Message::Unknown => println!("Unknown message, ignoring"),
    }
}
```

> **Nota**: `#[serde(other)]` solo funciona con enums con representación externa (`#[serde(tag)]`)
> y variantes sin datos. Con bincode, un variante desconocida causará un error de
> deserialización directamente.

---

## 8. Patrones avanzados

### Request-response con ID de correlación

Para permitir múltiples requests en vuelo simultáneamente (pipelining):

```rust
use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use tokio::sync::oneshot;
use std::sync::{Arc, Mutex};

#[derive(Debug, Serialize, Deserialize)]
struct Envelope<T> {
    id: u64,
    payload: T,
}

#[derive(Debug, Serialize, Deserialize)]
enum Request {
    Ping,
    Get { key: String },
}

#[derive(Debug, Serialize, Deserialize)]
enum Response {
    Pong,
    Value(Option<String>),
    Error(String),
}

type PendingRequests = Arc<Mutex<HashMap<u64, oneshot::Sender<Response>>>>;

/// Client side: send request and wait for response
async fn send_request(
    writer: &mut (impl tokio::io::AsyncWrite + Unpin),
    pending: &PendingRequests,
    next_id: &mut u64,
    request: Request,
) -> std::io::Result<Response> {
    let id = *next_id;
    *next_id += 1;

    let (tx, rx) = oneshot::channel();

    {
        let mut map = pending.lock().unwrap();
        map.insert(id, tx);
    }

    let envelope = Envelope { id, payload: request };
    send_message(writer, &envelope).await?;

    rx.await.map_err(|_| {
        std::io::Error::new(std::io::ErrorKind::ConnectionAborted, "channel closed")
    })
}

/// Client side: task that reads responses and routes them
async fn response_router(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
    pending: PendingRequests,
) -> std::io::Result<()> {
    loop {
        let envelope: Option<Envelope<Response>> = recv_message(reader).await?;

        match envelope {
            Some(env) => {
                let mut map = pending.lock().unwrap();
                if let Some(tx) = map.remove(&env.id) {
                    let _ = tx.send(env.payload);
                }
            }
            None => break,  // Server disconnected
        }
    }

    Ok(())
}
```

```
┌─── Request-Response con pipelining ───────────────────────┐
│                                                            │
│  Cliente                            Servidor               │
│  ───────                            ────────               │
│  {id:1, Get("a")}  ──────────────→                         │
│  {id:2, Get("b")}  ──────────────→  (procesando id:1)      │
│  {id:3, Ping}      ──────────────→  (procesando id:2)      │
│                                                            │
│                     ←──────────────  {id:2, Value("B")}    │
│                     ←──────────────  {id:1, Value("A")}    │
│                     ←──────────────  {id:3, Pong}          │
│                                                            │
│  Las respuestas pueden llegar en cualquier orden.           │
│  El id de correlación permite emparejar cada una.           │
└────────────────────────────────────────────────────────────┘
```

### Heartbeat / keep-alive

```rust
use serde::{Serialize, Deserialize};
use std::time::Duration;

#[derive(Debug, Serialize, Deserialize)]
enum Message {
    Heartbeat,
    Data(Vec<u8>),
}

async fn client_with_heartbeat(
    stream: tokio::net::TcpStream,
) -> std::io::Result<()> {
    let (mut reader, mut writer) = stream.into_split();
    let heartbeat_interval = Duration::from_secs(15);
    let read_timeout = Duration::from_secs(45);  // 3x heartbeat

    // Task de heartbeat: envía periódicamente
    let writer_handle = tokio::spawn(async move {
        let mut interval = tokio::time::interval(heartbeat_interval);
        loop {
            interval.tick().await;
            if send_message(&mut writer, &Message::Heartbeat).await.is_err() {
                break;
            }
        }
    });

    // Lectura con timeout
    loop {
        match tokio::time::timeout(read_timeout, recv_message(&mut reader)).await {
            Ok(Ok(Some(Message::Heartbeat))) => {
                // Heartbeat recibido, conexión viva
            }
            Ok(Ok(Some(Message::Data(data)))) => {
                println!("Received {} bytes", data.len());
            }
            Ok(Ok(None)) => {
                println!("Server disconnected");
                break;
            }
            Ok(Err(e)) => {
                eprintln!("Read error: {}", e);
                break;
            }
            Err(_) => {
                eprintln!("No heartbeat received in {:?}, disconnecting", read_timeout);
                break;
            }
        }
    }

    writer_handle.abort();
    Ok(())
}
```

### Streaming de datos grandes

Para enviar archivos u otros datos grandes, divide en chunks en vez de un solo frame
gigante:

```rust
use serde::{Serialize, Deserialize};

#[derive(Debug, Serialize, Deserialize)]
enum DataMessage {
    /// Start of a data stream: name and total size
    StreamStart { name: String, total_size: u64 },
    /// A chunk of data (index for ordering)
    Chunk { index: u32, data: Vec<u8> },
    /// End of data stream
    StreamEnd { checksum: u64 },
}

async fn send_file(
    writer: &mut (impl tokio::io::AsyncWrite + Unpin),
    name: &str,
    data: &[u8],
) -> std::io::Result<()> {
    const CHUNK_SIZE: usize = 64 * 1024;  // 64 KB chunks

    // Announce the transfer
    send_message(writer, &DataMessage::StreamStart {
        name: name.to_string(),
        total_size: data.len() as u64,
    }).await?;

    // Send chunks
    let mut checksum: u64 = 0;
    for (i, chunk) in data.chunks(CHUNK_SIZE).enumerate() {
        checksum = checksum.wrapping_add(
            chunk.iter().fold(0u64, |acc, &b| acc.wrapping_add(b as u64))
        );

        send_message(writer, &DataMessage::Chunk {
            index: i as u32,
            data: chunk.to_vec(),
        }).await?;
    }

    // Finish
    send_message(writer, &DataMessage::StreamEnd { checksum }).await?;

    Ok(())
}
```

---

## 9. Errores comunes

### Error 1: no validar el tamaño del frame

```rust
// MAL: un cliente malicioso envía longitud 0xFFFFFFFF
async fn read_frame_naive(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
) -> tokio::io::Result<Vec<u8>> {
    let len = reader.read_u32().await? as usize;
    let mut buf = vec![0u8; len];  // ← Intenta asignar 4 GB
    reader.read_exact(&mut buf).await?;
    Ok(buf)
}

// BIEN: validar antes de asignar memoria
async fn read_frame_safe(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
) -> tokio::io::Result<Vec<u8>> {
    let len = reader.read_u32().await? as usize;

    const MAX_SIZE: usize = 16 * 1024 * 1024;
    if len > MAX_SIZE {
        return Err(tokio::io::Error::new(
            tokio::io::ErrorKind::InvalidData,
            format!("frame too large: {} bytes", len),
        ));
    }

    let mut buf = vec![0u8; len];
    reader.read_exact(&mut buf).await?;
    Ok(buf)
}
```

**Por qué falla**: sin validación, un atacante puede causar OOM (out of memory) enviando
un header con longitud gigante. Siempre define y valida un tamaño máximo.

### Error 2: confundir EOF parcial con EOF limpio

```rust
use tokio::io::AsyncReadExt;

// MAL: no distingue desconexión limpia de corrupta
async fn read_frame_bad(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
) -> tokio::io::Result<Option<Vec<u8>>> {
    let mut len_buf = [0u8; 4];
    // Si read() retorna 0, asumimos EOF limpio
    // Pero ¿y si leyó 2 de 4 bytes del header?
    let n = reader.read(&mut len_buf).await?;
    if n == 0 { return Ok(None); }
    // Si n == 2, len_buf[2..4] son ceros → longitud incorrecta
    todo!()
}

// BIEN: usar read_exact para detectar EOF vs truncamiento
async fn read_frame_good(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
) -> tokio::io::Result<Option<Vec<u8>>> {
    let len = match reader.read_u32().await {
        Ok(len) => len as usize,
        Err(ref e) if e.kind() == tokio::io::ErrorKind::UnexpectedEof => {
            return Ok(None);  // EOF antes de empezar un frame: limpio
        }
        Err(e) => return Err(e),
    };

    // Si EOF ocurre DURANTE la lectura del payload,
    // read_exact retorna UnexpectedEof → es un error legítimo
    let mut buf = vec![0u8; len];
    reader.read_exact(&mut buf).await?;  // Error si no llegan todos los bytes

    Ok(Some(buf))
}
```

**Por qué falla**: `read()` puede retornar lecturas parciales. Si el header de longitud
se lee parcialmente, interpretas basura como la longitud. `read_u32` y `read_exact` manejan
esto correctamente internamente.

### Error 3: serializar con formato incompatible entre versiones

```rust
use serde::{Serialize, Deserialize};

// Versión 1
#[derive(Serialize, Deserialize)]
struct ConfigV1 {
    host: String,
    port: u16,
}

// Versión 2: campo nuevo en el medio (con bincode)
#[derive(Serialize, Deserialize)]
struct ConfigV2 {
    host: String,
    timeout: u32,  // ← Nuevo campo en el MEDIO
    port: u16,
}

// MAL: un cliente v1 envía ConfigV1, el servidor v2 deserializa como ConfigV2
// bincode serializa por posición: los bytes de port (u16) se interpretan
// como parte de timeout (u32) → datos corruptos sin error

// BIEN: agregar campos nuevos AL FINAL con default
#[derive(Serialize, Deserialize)]
struct ConfigV2Fixed {
    host: String,
    port: u16,
    #[serde(default)]
    timeout: u32,  // Al final, con default
}
```

**Por qué falla**: bincode serializa por posición, no por nombre. Insertar un campo cambia
la posición de todos los siguientes. JSON y MessagePack no tienen este problema.

### Error 4: olvidar flush después de write

```rust
use tokio::io::{AsyncWriteExt, BufWriter};

// MAL: datos quedan en el buffer, el cliente no los recibe
async fn send_response_bad(
    writer: &mut BufWriter<tokio::net::tcp::OwnedWriteHalf>,
    data: &[u8],
) -> tokio::io::Result<()> {
    writer.write_u32(data.len() as u32).await?;
    writer.write_all(data).await?;
    // Sin flush → datos en el buffer del BufWriter
    Ok(())
}

// BIEN: flush para asegurar que los datos se envían
async fn send_response_good(
    writer: &mut BufWriter<tokio::net::tcp::OwnedWriteHalf>,
    data: &[u8],
) -> tokio::io::Result<()> {
    writer.write_u32(data.len() as u32).await?;
    writer.write_all(data).await?;
    writer.flush().await?;  // Envía los bytes pendientes
    Ok(())
}
```

**Por qué falla**: `BufWriter` acumula writes pequeños para eficiencia. Sin `flush()`, el
receptor puede quedarse esperando datos que están en el buffer del emisor. Sin `BufWriter`
(escribiendo directo al `TcpStream`) no necesitas flush explícito.

### Error 5: mezclar protocolos de texto y binario

```rust
// MAL: servidor espera frames binarios, cliente envía texto
// El servidor interpreta "HELLO\n" como un header de longitud:
// bytes [48 45 4C 4C] = 1212501068 → intenta leer 1.2 GB

// BIEN: validar la primera lectura para detectar protocolo incorrecto
async fn validate_first_frame(
    reader: &mut (impl tokio::io::AsyncRead + Unpin),
) -> tokio::io::Result<Vec<u8>> {
    let len = reader.read_u32().await? as usize;

    // Sanity check: si la "longitud" parece texto ASCII,
    // probablemente alguien conectó con nc/telnet
    let len_bytes = (len as u32).to_be_bytes();
    if len_bytes.iter().all(|&b| b.is_ascii_graphic() || b.is_ascii_whitespace()) {
        return Err(tokio::io::Error::new(
            tokio::io::ErrorKind::InvalidData,
            "received text data — expected binary protocol. \
             Use the matching client, not telnet/nc.",
        ));
    }

    const MAX_SIZE: usize = 16 * 1024 * 1024;
    if len > MAX_SIZE {
        return Err(tokio::io::Error::new(
            tokio::io::ErrorKind::InvalidData,
            format!("frame too large: {} bytes", len),
        ));
    }

    let mut buf = vec![0u8; len];
    reader.read_exact(&mut buf).await?;
    Ok(buf)
}
```

**Por qué falla**: si alguien conecta con `nc` o `telnet` a un servidor de protocolo
binario, los caracteres ASCII se interpretan como headers de longitud, produciendo valores
enormes. Un magic number o un sanity check en la primera lectura ayuda a detectar esto.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                       PROTOCOLO CUSTOM                                 ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  FRAMING                                                               ║
║  ───────                                                               ║
║  Delimitador:    read_line() → String          simple, solo texto      ║
║  Length-prefix:  read_u32() + read_exact()      binario, robusto       ║
║  Fijo:           read_exact(&mut [u8; N])       raro, protocolos HW    ║
║                                                                        ║
║  LENGTH-PREFIX MANUAL                                                  ║
║  ────────────────────                                                  ║
║  write: write_u32(len) + write_all(data) + flush()                     ║
║  read:  read_u32() → validate → read_exact(buf)                       ║
║  EOF:   read_u32() → UnexpectedEof → Ok(None)                         ║
║                                                                        ║
║  SERDE                                                                 ║
║  ─────                                                                 ║
║  #[derive(Serialize, Deserialize)]                                     ║
║  serde_json::to_string / from_str         texto, legible, universal    ║
║  bincode::serialize / deserialize         binario, rápido, Rust-only   ║
║  rmp_serde::to_vec / from_slice           binario, interoperable       ║
║  Enums como mensajes: variantes = tipos de mensaje                     ║
║  #[serde(default)] para campos nuevos opcionales                       ║
║                                                                        ║
║  PROTOCOLO COMPLETO                                                    ║
║  ──────────────────                                                    ║
║  send: serialize(msg) → write_u32(len) + write_all(bytes)              ║
║  recv: read_u32() → read_exact(buf) → deserialize(buf)                 ║
║  Funciones genéricas: send_message<W,M> / recv_message<R,M>            ║
║                                                                        ║
║  TOKIO_UTIL::CODEC                                                     ║
║  ─────────────────                                                     ║
║  Decoder::decode(&mut BytesMut) → Option<Item>                         ║
║  Encoder::encode(Item, &mut BytesMut)                                  ║
║  Framed::new(stream, codec)  → Stream + Sink                           ║
║  Built-in: LinesCodec, LengthDelimitedCodec, BytesCodec               ║
║  StreamExt::next() para leer, SinkExt::send() para escribir            ║
║                                                                        ║
║  VERSIONADO                                                            ║
║  ──────────                                                            ║
║  Magic number en handshake para identificar protocolo                  ║
║  Version field para negociar features                                  ║
║  Campos nuevos al FINAL con #[serde(default)]                          ║
║  #[serde(other)] para variantes desconocidas (solo JSON)               ║
║                                                                        ║
║  PATRONES                                                              ║
║  ────────                                                              ║
║  Request-response con ID correlación: Envelope<T> { id, payload }      ║
║  Heartbeat: timer + timeout en lectura                                 ║
║  Streaming: StreamStart → Chunk[] → StreamEnd                          ║
║                                                                        ║
║  VALIDACIÓN (SIEMPRE)                                                  ║
║  ────────────────────                                                  ║
║  MAX_FRAME_SIZE antes de asignar memoria                               ║
║  Magic number para detectar protocolo incorrecto                       ║
║  flush() después de cada mensaje con BufWriter                         ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: protocolo de calculadora

Implementa un servidor TCP con protocolo binario (length-prefixed + bincode) para una
calculadora remota:

1. Define `Request` con variantes: `Add(f64, f64)`, `Sub(f64, f64)`, `Mul(f64, f64)`,
   `Div(f64, f64)`.
2. Define `Response` con variantes: `Result(f64)`, `Error(String)` (para división por cero).
3. Implementa `send_message` y `recv_message` genéricos.
4. El servidor procesa operaciones y envía respuestas.
5. El cliente envía 5 operaciones, recibe los resultados y los muestra.

Prueba enviando `Div(10.0, 0.0)` para verificar que el error se maneja correctamente.

**Pista**: `send_message` y `recv_message` deben ser genéricos sobre `Serialize` y
`DeserializeOwned` respectivamente.

> **Pregunta de reflexión**: ¿Qué pasaría si usaras JSON en vez de bincode? ¿Cambiaría
> algo en las funciones `send_message`/`recv_message`, o solo en la implementación interna?

---

### Ejercicio 2: chat con protocolo tipado

Construye un chat TCP usando `tokio_util::codec::Framed` con un codec custom:

1. Define mensajes: `Join { username }`, `Message { text }`, `Leave`, `UserList`,
   y respuestas: `Welcome { users: Vec<String> }`, `Broadcast { from, text }`,
   `UserJoined { username }`, `UserLeft { username }`.
2. Implementa `Encoder` y `Decoder` con length-prefix + bincode.
3. Al conectarse, el cliente envía `Join`. El servidor responde `Welcome` con la lista de
   usuarios actuales.
4. Los mensajes se difunden a todos los clientes conectados excepto al emisor.
5. Al desconectarse, el servidor envía `UserLeft` a los demás.

**Pista**: usa `Framed` para obtener un `Stream + Sink` y `broadcast::channel` de tokio para
difundir mensajes entre tasks.

> **Pregunta de reflexión**: ¿Qué ventaja tiene definir `Join`, `Message` y `Leave` como
> variantes de un enum en lugar de usar strings con un prefijo de comando (como `"/join alice"`)?
> Piensa en validación, errores de tipeo y evolución del protocolo.

---

### Ejercicio 3: protocolo con handshake y heartbeat

Crea un protocolo cliente-servidor que incluya:

1. **Handshake**: el cliente envía `{ magic: 0xCAFE, version: 1, name: "..." }`.
   El servidor verifica el magic number y la versión, y responde con aceptación o rechazo.
2. **Heartbeat**: después del handshake, ambos lados envían `Heartbeat` cada 10 segundos.
   Si no se recibe heartbeat en 35 segundos, desconectar.
3. **Datos**: entre heartbeats, el cliente puede enviar `Echo { data }` y el servidor
   responde con el mismo dato.

Usa `tokio::select!` para manejar simultáneamente: heartbeat timer, timeout de lectura,
y procesamiento de mensajes.

**Pista**: el patrón `select!` con tres ramas (interval.tick, timeout de read, señal de
shutdown) es la estructura central. Define un solo enum `Message` con variantes `Handshake`,
`HandshakeReply`, `Heartbeat`, `Echo`, `EchoReply`.

> **Pregunta de reflexión**: ¿Por qué el timeout de heartbeat (35s) es mayor que el intervalo
> (10s)? ¿Qué problema tendrías si fueran iguales? ¿Y si el timeout fuera menor?
