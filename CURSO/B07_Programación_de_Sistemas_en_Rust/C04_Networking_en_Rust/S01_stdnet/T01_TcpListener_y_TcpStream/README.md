# `TcpListener` y `TcpStream`

## Índice

1. [TCP en Rust: std::net](#1-tcp-en-rust-stdnet)
2. [TcpListener — Escuchar conexiones](#2-tcplistener--escuchar-conexiones)
3. [TcpStream — Leer y escribir](#3-tcpstream--leer-y-escribir)
4. [Servidor echo completo](#4-servidor-echo-completo)
5. [Cliente TCP](#5-cliente-tcp)
6. [Opciones de socket](#6-opciones-de-socket)
7. [Protocolo basado en líneas](#7-protocolo-basado-en-líneas)
8. [Shutdown y cierre de conexiones](#8-shutdown-y-cierre-de-conexiones)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. TCP en Rust: `std::net`

`std::net` proporciona TCP y UDP síncronos (bloqueantes). Cada operación de lectura o
escritura bloquea el hilo hasta que se completa o falla. Es la base sobre la que se
construyen abstracciones async como `tokio::net`.

```
┌─────────────────────────────────────────────────────────┐
│                    std::net (TCP)                        │
├──────────────────┬──────────────────────────────────────┤
│  TcpListener     │  bind, accept, incoming              │
│  TcpStream       │  connect, read, write, shutdown      │
│  SocketAddr      │  IP + puerto                         │
│  ToSocketAddrs   │  Trait para resolución DNS            │
└──────────────────┴──────────────────────────────────────┘
```

Flujo de una conexión TCP:

```
Servidor                              Cliente
────────                              ───────
TcpListener::bind("0.0.0.0:8080")
     │
     ▼
  listener.accept()  ◄─────────────  TcpStream::connect("server:8080")
     │                                    │
     ▼                                    ▼
  TcpStream  ◄═══════════════════════► TcpStream
     │          conexión establecida      │
     │                                    │
  stream.read()  ◄────── datos ──────  stream.write()
  stream.write() ────── datos ──────►  stream.read()
     │                                    │
  drop(stream)   ═══════ FIN ══════►  EOF (read = 0)
```

Mapeo a syscalls del OS:

```
Rust                        Syscall Linux
──────────                  ────────────────
TcpListener::bind(addr)    socket() + bind() + listen()
listener.accept()          accept()
TcpStream::connect(addr)   socket() + connect()
stream.read(&mut buf)      recv() / read()
stream.write(data)         send() / write()
stream.shutdown(how)       shutdown()
drop(stream)               close()
```

---

## 2. `TcpListener` — Escuchar conexiones

### 2.1 Bind y accept

```rust
use std::net::TcpListener;

fn main() -> std::io::Result<()> {
    // Bind: reservar un puerto para escuchar
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    println!("Listening on {}", listener.local_addr()?);

    // accept(): bloquea hasta que un cliente se conecte
    // Retorna (TcpStream, SocketAddr del cliente)
    let (stream, addr) = listener.accept()?;
    println!("Client connected from {}", addr);

    // stream es un TcpStream listo para leer/escribir
    drop(stream);
    Ok(())
}
```

### 2.2 Direcciones de bind

```rust
use std::net::TcpListener;

fn main() -> std::io::Result<()> {
    // Solo localhost (no accesible desde otras máquinas)
    let _local = TcpListener::bind("127.0.0.1:8080")?;

    // Todas las interfaces (accesible desde la red)
    let _all = TcpListener::bind("0.0.0.0:8080")?;

    // IPv6 localhost
    let _v6 = TcpListener::bind("[::1]:8080")?;

    // Todas las interfaces IPv6 (incluye IPv4 en dual-stack)
    let _v6_all = TcpListener::bind("[::]:8080")?;

    // Puerto 0: el OS asigna un puerto disponible
    let auto = TcpListener::bind("127.0.0.1:0")?;
    println!("Auto-assigned: {}", auto.local_addr()?);
    // Ej: 127.0.0.1:43567

    Ok(())
}
```

### 2.3 `incoming()` — Iterador de conexiones

```rust
use std::net::TcpListener;
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    println!("Listening on :8080");

    // incoming() retorna un iterador infinito de Result<TcpStream>
    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                println!("Client: {}", stream.peer_addr()?);

                // Leer y responder (un cliente a la vez)
                let mut buf = [0u8; 1024];
                let n = stream.read(&mut buf)?;
                stream.write_all(&buf[..n])?;
            }
            Err(e) => {
                eprintln!("Accept error: {}", e);
            }
        }
    }

    Ok(())
}
```

> **Limitación**: con `incoming()` se procesa un cliente a la vez. Mientras se atiende
> a un cliente, los demás esperan en la cola del listener (backlog). Ver T03 para
> servidor multi-cliente.

### 2.4 Non-blocking accept

```rust
use std::net::TcpListener;

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;

    // Modo non-blocking: accept() retorna inmediatamente
    listener.set_nonblocking(true)?;

    loop {
        match listener.accept() {
            Ok((stream, addr)) => {
                println!("Got connection from {}", addr);
                // Manejar stream...
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                // No hay conexiones pendientes — hacer otra cosa
                std::thread::sleep(std::time::Duration::from_millis(100));
            }
            Err(e) => {
                eprintln!("Accept error: {}", e);
            }
        }
    }
}
```

---

## 3. `TcpStream` — Leer y escribir

### 3.1 Read y Write traits

`TcpStream` implementa `Read` y `Write` de `std::io`:

```rust
use std::net::TcpStream;
use std::io::{Read, Write};

fn handle_connection(mut stream: TcpStream) -> std::io::Result<()> {
    // read: lee lo que esté disponible (puede ser parcial)
    let mut buf = [0u8; 4096];
    let n = stream.read(&mut buf)?;
    println!("Received {} bytes: {:?}", n, &buf[..n]);

    // write: puede escribir parcialmente
    let written = stream.write(b"Hello, client!")?;
    println!("Wrote {} bytes", written);

    // write_all: garantiza escribir todo (o error)
    stream.write_all(b"Complete message\n")?;

    // flush: asegurar que los datos se envíen
    stream.flush()?;

    Ok(())
}
```

### 3.2 `read` retorna datos parciales

Un aspecto crucial de TCP: `read()` puede retornar **menos** datos de los que esperabas.
TCP es un protocolo de flujo (stream), no de mensajes.

```
Envío:                          Recepción:
write_all("Hello, World!")      read() puede retornar:
                                  "Hello, Wor"   (parcial)
                                  luego: "ld!"   (el resto)

                                O todo junto:
                                  "Hello, World!"

                                O byte por byte:
                                  "H" "e" "l" "l" "o" ...
```

```rust
use std::io::Read;
use std::net::TcpStream;

fn read_exact_n(stream: &mut TcpStream, n: usize) -> std::io::Result<Vec<u8>> {
    let mut buf = vec![0u8; n];
    stream.read_exact(&mut buf)?; // Lee EXACTAMENTE n bytes o error
    Ok(buf)
}

// read_exact internamente hace un loop:
// while bytes_read < n {
//     let chunk = stream.read(&mut buf[bytes_read..])?;
//     if chunk == 0 { return Err(UnexpectedEof); }
//     bytes_read += chunk;
// }
```

### 3.3 Detectar cierre de conexión

```rust
use std::io::Read;
use std::net::TcpStream;

fn read_until_close(mut stream: TcpStream) -> std::io::Result<Vec<u8>> {
    let mut data = Vec::new();
    let mut buf = [0u8; 4096];

    loop {
        match stream.read(&mut buf) {
            Ok(0) => {
                // read() retorna 0 bytes = el peer cerró la conexión (EOF)
                println!("Connection closed by peer");
                break;
            }
            Ok(n) => {
                data.extend_from_slice(&buf[..n]);
                println!("Read {} bytes (total: {})", n, data.len());
            }
            Err(e) => {
                eprintln!("Read error: {}", e);
                return Err(e);
            }
        }
    }

    Ok(data)
}
```

### 3.4 Timeouts de lectura y escritura

```rust
use std::net::TcpStream;
use std::io::Read;
use std::time::Duration;

fn read_with_timeout(mut stream: TcpStream) -> std::io::Result<()> {
    // Establecer timeout de lectura
    stream.set_read_timeout(Some(Duration::from_secs(5)))?;

    let mut buf = [0u8; 1024];
    match stream.read(&mut buf) {
        Ok(n) => println!("Read {} bytes", n),
        Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock
                    || e.kind() == std::io::ErrorKind::TimedOut => {
            println!("Read timed out after 5 seconds");
        }
        Err(e) => return Err(e),
    }

    // Establecer timeout de escritura
    stream.set_write_timeout(Some(Duration::from_secs(5)))?;

    // Quitar timeout (blocking indefinido)
    stream.set_read_timeout(None)?;

    Ok(())
}
```

### 3.5 `try_clone` — Compartir el socket

`try_clone()` crea otro handle al mismo socket. Útil para leer en un thread y
escribir en otro:

```rust
use std::net::TcpStream;
use std::io::{Read, Write, BufRead, BufReader};
use std::thread;

fn bidirectional(stream: TcpStream) -> std::io::Result<()> {
    let mut writer = stream.try_clone()?;

    // Thread de lectura
    let reader_handle = thread::spawn(move || {
        let reader = BufReader::new(stream);
        for line in reader.lines() {
            match line {
                Ok(line) => println!("Received: {}", line),
                Err(_) => break,
            }
        }
    });

    // Thread de escritura (en el main thread)
    for i in 0..5 {
        writeln!(writer, "Message {}", i)?;
        thread::sleep(std::time::Duration::from_millis(500));
    }

    // Cerrar escritura → el reader recibe EOF
    writer.shutdown(std::net::Shutdown::Write)?;
    reader_handle.join().unwrap();

    Ok(())
}
```

```
try_clone():

  stream ──────┐
               ├──► mismo file descriptor del OS
  clone  ──────┘

  Thread A:  stream.read()   (bloqueante, independiente)
  Thread B:  clone.write()   (bloqueante, independiente)

  Cerrar un clon NO cierra el otro.
  El socket se cierra cuando AMBOS se dropean.
```

---

## 4. Servidor echo completo

Un servidor que devuelve todo lo que el cliente envía:

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};

fn handle_client(mut stream: TcpStream) {
    let addr = stream.peer_addr().unwrap();
    println!("[{}] Connected", addr);

    let mut buf = [0u8; 4096];

    loop {
        match stream.read(&mut buf) {
            Ok(0) => {
                println!("[{}] Disconnected", addr);
                return;
            }
            Ok(n) => {
                println!("[{}] {} bytes", addr, n);
                if stream.write_all(&buf[..n]).is_err() {
                    println!("[{}] Write failed", addr);
                    return;
                }
            }
            Err(e) => {
                println!("[{}] Read error: {}", addr, e);
                return;
            }
        }
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:7878")?;
    println!("Echo server on :7878");

    // Servidor single-threaded: un cliente a la vez
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => handle_client(stream),
            Err(e) => eprintln!("Accept error: {}", e),
        }
    }

    Ok(())
}
```

Para probar:
```bash
# Terminal 1: servidor
cargo run

# Terminal 2: cliente con netcat
echo "Hello" | nc localhost 7878

# O cliente interactivo
nc localhost 7878
# Escribir texto → el servidor lo devuelve
```

---

## 5. Cliente TCP

### 5.1 Cliente simple

```rust
use std::net::TcpStream;
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    // Conectar al servidor
    let mut stream = TcpStream::connect("127.0.0.1:7878")?;
    println!("Connected to {}", stream.peer_addr()?);

    // Enviar datos
    stream.write_all(b"Hello, server!\n")?;

    // Leer respuesta
    let mut buf = [0u8; 1024];
    let n = stream.read(&mut buf)?;
    println!("Response: {}", String::from_utf8_lossy(&buf[..n]));

    Ok(())
    // stream se cierra aquí (Drop)
}
```

### 5.2 Cliente con timeout de conexión

```rust
use std::net::{TcpStream, SocketAddr};
use std::time::Duration;

fn connect_with_timeout(addr: &str, timeout: Duration) -> std::io::Result<TcpStream> {
    let socket_addr: SocketAddr = addr.parse().map_err(|e| {
        std::io::Error::new(std::io::ErrorKind::InvalidInput, e)
    })?;

    TcpStream::connect_timeout(&socket_addr, timeout)
}

fn main() {
    match connect_with_timeout("93.184.216.34:80", Duration::from_secs(5)) {
        Ok(stream) => println!("Connected to {}", stream.peer_addr().unwrap()),
        Err(e) => println!("Connection failed: {}", e),
    }
}
```

> **Nota**: `connect_timeout` requiere `SocketAddr` (IP numérica), no acepta hostnames.
> Para timeout con resolución DNS, necesitas resolver primero con `ToSocketAddrs`.

### 5.3 Cliente HTTP minimalista

```rust
use std::net::TcpStream;
use std::io::{Read, Write};

fn http_get(host: &str, path: &str) -> std::io::Result<String> {
    let addr = format!("{}:80", host);
    let mut stream = TcpStream::connect(&addr)?;

    // Enviar request HTTP/1.1
    let request = format!(
        "GET {} HTTP/1.1\r\n\
         Host: {}\r\n\
         Connection: close\r\n\
         \r\n",
        path, host
    );
    stream.write_all(request.as_bytes())?;

    // Leer toda la respuesta
    let mut response = String::new();
    stream.read_to_string(&mut response)?;

    Ok(response)
}

fn main() -> std::io::Result<()> {
    let response = http_get("example.com", "/")?;

    // Separar headers del body
    if let Some(pos) = response.find("\r\n\r\n") {
        let headers = &response[..pos];
        let body = &response[pos + 4..];
        println!("=== Headers ===\n{}\n", headers);
        println!("=== Body ({} bytes) ===\n{}",
            body.len(), &body[..body.len().min(200)]);
    }

    Ok(())
}
```

### 5.4 `ToSocketAddrs` — Resolución DNS

```rust
use std::net::{TcpStream, ToSocketAddrs};

fn main() -> std::io::Result<()> {
    // TcpStream::connect acepta cualquier cosa que implemente ToSocketAddrs
    // Esto incluye resolución DNS automática

    // String "host:port" → resolución DNS
    let _s = TcpStream::connect("example.com:80")?;

    // &str directo
    let _s = TcpStream::connect("127.0.0.1:8080")?;

    // SocketAddr (sin resolución)
    let addr: std::net::SocketAddr = "127.0.0.1:8080".parse().unwrap();
    let _s = TcpStream::connect(addr)?;

    // Tuple (IpAddr, port)
    let _s = TcpStream::connect(("127.0.0.1", 8080))?;

    // Resolver manualmente (puede retornar múltiples IPs)
    let addrs: Vec<_> = "example.com:80".to_socket_addrs()?.collect();
    println!("Resolved addresses:");
    for addr in &addrs {
        println!("  {}", addr);
    }

    Ok(())
}
```

---

## 6. Opciones de socket

### 6.1 Opciones disponibles

```rust
use std::net::TcpStream;
use std::time::Duration;

fn configure_stream(stream: &TcpStream) -> std::io::Result<()> {
    // TCP_NODELAY: deshabilitar el algoritmo de Nagle
    // Enviar paquetes inmediatamente sin esperar a acumular datos
    stream.set_nodelay(true)?;
    println!("nodelay: {}", stream.nodelay()?);

    // SO_RCVTIMEO / SO_SNDTIMEO: timeouts de lectura/escritura
    stream.set_read_timeout(Some(Duration::from_secs(30)))?;
    stream.set_write_timeout(Some(Duration::from_secs(10)))?;

    // TTL: Time To Live de los paquetes IP
    stream.set_ttl(64)?;
    println!("ttl: {}", stream.ttl()?);

    // Non-blocking mode
    stream.set_nonblocking(false)?;

    // Obtener direcciones
    println!("local:  {}", stream.local_addr()?);
    println!("peer:   {}", stream.peer_addr()?);

    Ok(())
}
```

### 6.2 `TCP_NODELAY` — Latencia vs throughput

```
Sin NODELAY (Nagle habilitado, default):
  write("H") → buffer
  write("e") → buffer
  write("l") → buffer
  ...
  (200ms después o buffer lleno) → enviar "Hello" en 1 paquete
  Mejor throughput, peor latencia

Con NODELAY (Nagle deshabilitado):
  write("H") → enviar inmediatamente
  write("e") → enviar inmediatamente
  ...
  Cada write genera un paquete TCP separado
  Peor throughput, mejor latencia
```

Cuándo usar `NODELAY`:
- **Activar** para protocolos interactivos (chat, gaming, remote terminal)
- **Dejar desactivado** para transferencias de datos grandes (archivos, backup)

### 6.3 Opciones del listener

```rust
use std::net::TcpListener;

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:0")?;

    // TTL para conexiones aceptadas
    listener.set_ttl(64)?;

    // Non-blocking: accept() retorna WouldBlock si no hay conexiones
    listener.set_nonblocking(true)?;

    // Dirección asignada
    println!("Listening on {}", listener.local_addr()?);

    Ok(())
}
```

### 6.4 `SO_REUSEADDR` — Reusar puerto

Por defecto, después de cerrar un servidor, el puerto queda en estado `TIME_WAIT`
durante ~60 segundos. Para poder reiniciar inmediatamente, necesitas `SO_REUSEADDR`.
Rust lo habilita automáticamente en `TcpListener::bind()`.

```rust
// Rust habilita SO_REUSEADDR automáticamente en bind()
// No necesitas hacerlo manualmente

// Si necesitas control manual (por ejemplo, SO_REUSEPORT en Linux),
// usa socket2 crate:
//
// use socket2::{Socket, Domain, Type, Protocol};
//
// let socket = Socket::new(Domain::IPV4, Type::STREAM, Some(Protocol::TCP))?;
// socket.set_reuse_address(true)?;
// socket.set_reuse_port(true)?;  // Linux: permite múltiples listeners
// socket.bind(&addr.into())?;
// socket.listen(128)?;
// let listener: TcpListener = socket.into();
```

---

## 7. Protocolo basado en líneas

Muchos protocolos de texto usan líneas (terminadas en `\n` o `\r\n`) como
delimitador de mensajes. `BufReader` facilita esto:

### 7.1 Servidor de comandos

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Write};

fn handle_client(stream: TcpStream) -> std::io::Result<()> {
    let addr = stream.peer_addr()?;
    let mut writer = stream.try_clone()?;
    let reader = BufReader::new(stream);

    writeln!(writer, "Welcome! Commands: PING, TIME, QUIT")?;

    for line in reader.lines() {
        let line = line?;
        let command = line.trim().to_uppercase();

        match command.as_str() {
            "PING" => {
                writeln!(writer, "PONG")?;
            }
            "TIME" => {
                let now = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_secs();
                writeln!(writer, "TIME {}", now)?;
            }
            "QUIT" => {
                writeln!(writer, "BYE")?;
                println!("[{}] Client quit", addr);
                return Ok(());
            }
            _ => {
                writeln!(writer, "ERROR Unknown command: {}", line.trim())?;
            }
        }
        writer.flush()?;
    }

    println!("[{}] Connection closed", addr);
    Ok(())
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:9000")?;
    println!("Command server on :9000");

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                println!("[{}] Connected", stream.peer_addr()?);
                if let Err(e) = handle_client(stream) {
                    eprintln!("Handler error: {}", e);
                }
            }
            Err(e) => eprintln!("Accept error: {}", e),
        }
    }
    Ok(())
}
```

### 7.2 Protocolo con headers tipo HTTP

```rust
use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::collections::HashMap;

fn read_request(stream: &TcpStream) -> std::io::Result<(String, HashMap<String, String>, Vec<u8>)> {
    let mut reader = BufReader::new(stream);

    // Línea 1: request line (ej: "GET /path")
    let mut request_line = String::new();
    reader.read_line(&mut request_line)?;
    let request_line = request_line.trim().to_string();

    // Headers: "Key: Value" hasta línea vacía
    let mut headers = HashMap::new();
    loop {
        let mut line = String::new();
        reader.read_line(&mut line)?;
        let line = line.trim().to_string();

        if line.is_empty() {
            break; // Fin de headers
        }

        if let Some((key, value)) = line.split_once(':') {
            headers.insert(
                key.trim().to_lowercase(),
                value.trim().to_string(),
            );
        }
    }

    // Body: leer Content-Length bytes si existe
    let body = if let Some(len_str) = headers.get("content-length") {
        let len: usize = len_str.parse().unwrap_or(0);
        let mut body = vec![0u8; len];
        std::io::Read::read_exact(&mut reader, &mut body)?;
        body
    } else {
        Vec::new()
    };

    Ok((request_line, headers, body))
}
```

### 7.3 Framing: length-prefixed messages

Para datos binarios, el patrón más robusto es prefixar cada mensaje con su longitud:

```rust
use std::io::{Read, Write};
use std::net::TcpStream;

fn send_message(stream: &mut TcpStream, data: &[u8]) -> std::io::Result<()> {
    // Enviar longitud como 4 bytes big-endian
    let len = data.len() as u32;
    stream.write_all(&len.to_be_bytes())?;
    // Enviar datos
    stream.write_all(data)?;
    stream.flush()?;
    Ok(())
}

fn recv_message(stream: &mut TcpStream) -> std::io::Result<Vec<u8>> {
    // Leer 4 bytes de longitud
    let mut len_buf = [0u8; 4];
    stream.read_exact(&mut len_buf)?;
    let len = u32::from_be_bytes(len_buf) as usize;

    // Protección contra mensajes demasiado grandes
    const MAX_MSG_SIZE: usize = 16 * 1024 * 1024; // 16 MB
    if len > MAX_MSG_SIZE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            format!("message too large: {} bytes", len),
        ));
    }

    // Leer exactamente len bytes
    let mut data = vec![0u8; len];
    stream.read_exact(&mut data)?;
    Ok(data)
}

fn main() -> std::io::Result<()> {
    // Ejemplo de uso
    let mut stream = TcpStream::connect("127.0.0.1:8080")?;

    send_message(&mut stream, b"Hello, server!")?;
    let response = recv_message(&mut stream)?;
    println!("Response: {:?}", String::from_utf8_lossy(&response));

    Ok(())
}
```

```
Wire format:
┌──────────┬────────────────────────────┐
│ 4 bytes  │  N bytes                   │
│ length N │  message data              │
│ (BE u32) │                            │
└──────────┴────────────────────────────┘

Ejemplo: "Hello" (5 bytes)
  [0x00, 0x00, 0x00, 0x05] [0x48, 0x65, 0x6C, 0x6C, 0x6F]
   ← longitud: 5 →          ←        "Hello"        →
```

---

## 8. Shutdown y cierre de conexiones

### 8.1 `shutdown` — Cerrar un lado de la conexión

```rust
use std::net::{TcpStream, Shutdown};
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    let mut stream = TcpStream::connect("127.0.0.1:8080")?;

    // Enviar datos
    stream.write_all(b"request data")?;

    // Cerrar el LADO DE ESCRITURA
    // El peer recibirá EOF (read retorna 0)
    // pero nosotros podemos seguir leyendo la respuesta
    stream.shutdown(Shutdown::Write)?;

    // Leer respuesta (el peer sabe que no enviaremos más)
    let mut response = String::new();
    stream.read_to_string(&mut response)?;
    println!("Response: {}", response);

    Ok(())
}
```

```
Opciones de Shutdown:

Shutdown::Read   → No leer más (el peer puede seguir enviando)
Shutdown::Write  → No escribir más (el peer recibe EOF)
Shutdown::Both   → Cerrar ambos lados

Uso típico:
  1. Cliente envía request completo
  2. Cliente hace shutdown(Write) → "ya no envío más"
  3. Servidor recibe EOF → sabe que el request terminó
  4. Servidor envía response y cierra
  5. Cliente lee response hasta EOF
```

### 8.2 Diferencia entre `shutdown` y `drop`

```rust
use std::net::{TcpStream, Shutdown};
use std::io::Write;

fn graceful_close(stream: &mut TcpStream) -> std::io::Result<()> {
    // shutdown(Write): envía FIN al peer, el peer puede seguir enviando
    stream.shutdown(Shutdown::Write)?;

    // Leer hasta que el peer también cierre
    let mut buf = [0u8; 1024];
    loop {
        match std::io::Read::read(stream, &mut buf) {
            Ok(0) => break,     // Peer cerró
            Ok(_) => continue,  // Datos restantes del peer
            Err(_) => break,
        }
    }

    Ok(())
    // drop(stream) envía RST si hay datos no leídos
}
```

```
Graceful close:
  shutdown(Write) → FIN → peer recibe EOF
                        ← peer envía FIN → recibimos EOF
  drop(stream)    → close() limpio

Abrupt close (solo drop):
  drop(stream) → close()
  Si hay datos en el buffer de recepción no leídos → RST
  El peer recibe "Connection reset by peer"
```

---

## 9. Errores comunes

### Error 1: No manejar `read()` retornando menos bytes de los esperados

```rust
use std::io::Read;
use std::net::TcpStream;

// MAL — asumir que read() llena todo el buffer
fn bad_read(stream: &mut TcpStream) -> std::io::Result<()> {
    let mut buf = [0u8; 1024];
    stream.read(&mut buf)?;
    // Procesar buf COMPLETO — pero quizá solo se leyeron 10 bytes
    let msg = String::from_utf8_lossy(&buf); // Incluye 1014 bytes de basura
    println!("{}", msg);
    Ok(())
}

// BIEN — usar el número de bytes retornado
fn good_read(stream: &mut TcpStream) -> std::io::Result<()> {
    let mut buf = [0u8; 1024];
    let n = stream.read(&mut buf)?;
    if n == 0 {
        println!("Connection closed");
        return Ok(());
    }
    // Solo procesar los bytes que realmente se leyeron
    let msg = String::from_utf8_lossy(&buf[..n]);
    println!("{}", msg);
    Ok(())
}
```

**Por qué ocurre**: TCP es un stream de bytes, no de mensajes. `read()` retorna lo
que el kernel tiene disponible, que puede ser 1 byte o el buffer completo. Siempre
usar `&buf[..n]` con el valor retornado.

### Error 2: No considerar que `write()` puede ser parcial

```rust
use std::io::Write;
use std::net::TcpStream;

// MAL — write() puede no escribir todo
fn bad_write(stream: &mut TcpStream, data: &[u8]) -> std::io::Result<()> {
    stream.write(data)?; // Puede escribir solo parte de data
    Ok(()) // El resto se pierde silenciosamente
}

// BIEN — usar write_all()
fn good_write(stream: &mut TcpStream, data: &[u8]) -> std::io::Result<()> {
    stream.write_all(data)?; // Garantiza escribir todo o error
    Ok(())
}
```

### Error 3: Olvidar que el servidor single-threaded bloquea

```rust
use std::net::TcpListener;
use std::io::{Read, Write};

// MAL — un cliente lento bloquea a todos los demás
fn bad_server() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080")?;

    for stream in listener.incoming() {
        let mut stream = stream?;
        // Si este cliente envía datos lentamente (1 byte/segundo),
        // NINGÚN otro cliente puede conectarse hasta que termine
        let mut buf = Vec::new();
        stream.read_to_end(&mut buf)?;
        stream.write_all(&buf)?;
    }
    Ok(())
}

// BIEN — un thread por cliente (ver T03 para soluciones mejores)
fn good_server() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080")?;

    for stream in listener.incoming() {
        let stream = stream?;
        std::thread::spawn(move || {
            let mut stream = stream;
            let mut buf = [0u8; 4096];
            loop {
                match stream.read(&mut buf) {
                    Ok(0) => return,
                    Ok(n) => { let _ = stream.write_all(&buf[..n]); }
                    Err(_) => return,
                }
            }
        });
    }
    Ok(())
}
```

### Error 4: No limitar el tamaño de mensajes recibidos

```rust
use std::io::Read;
use std::net::TcpStream;

// MAL — un cliente malicioso puede agotar la memoria
fn bad_read_all(stream: &mut TcpStream) -> std::io::Result<Vec<u8>> {
    let mut data = Vec::new();
    stream.read_to_end(&mut data)?; // Sin límite: puede ser 100 GB
    Ok(data)
}

// BIEN — limitar el tamaño
fn safe_read(stream: &mut TcpStream, max_size: usize) -> std::io::Result<Vec<u8>> {
    let mut data = Vec::new();
    let mut buf = [0u8; 4096];
    let mut total = 0;

    loop {
        let n = stream.read(&mut buf)?;
        if n == 0 { break; }

        total += n;
        if total > max_size {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                format!("message exceeds {} byte limit", max_size),
            ));
        }
        data.extend_from_slice(&buf[..n]);
    }

    Ok(data)
}
```

### Error 5: Usar `connect` sin timeout

```rust
use std::net::{TcpStream, SocketAddr};
use std::time::Duration;

// MAL — connect puede bloquear ~2 minutos en Linux (TCP retransmissions)
fn bad_connect() -> std::io::Result<TcpStream> {
    TcpStream::connect("10.0.0.1:8080") // Timeout del kernel: ~127 segundos
}

// BIEN — timeout explícito
fn good_connect() -> std::io::Result<TcpStream> {
    let addr: SocketAddr = "10.0.0.1:8080".parse().unwrap();
    TcpStream::connect_timeout(&addr, Duration::from_secs(5))
}
```

**Por qué importa**: cuando un host no responde (firewall dropeando paquetes), el
kernel de Linux reintenta SYN hasta 6 veces con backoff exponencial, tardando ~127
segundos en total antes de retornar error. `connect_timeout` acorta esta espera.

---

## 10. Cheatsheet

```text
──────────────────── TcpListener ─────────────────────
TcpListener::bind(addr)?            Bind a addr (habilita SO_REUSEADDR)
listener.accept()?                  Bloquea → (TcpStream, SocketAddr)
listener.incoming()                 Iterador de Result<TcpStream>
listener.local_addr()?              SocketAddr asignado
listener.set_nonblocking(bool)?     Non-blocking accept
listener.set_ttl(n)?                TTL de paquetes
"127.0.0.1:0"                       Puerto auto-asignado por el OS

──────────────────── TcpStream ───────────────────────
TcpStream::connect(addr)?           Conectar (bloquea, timeout ~127s)
TcpStream::connect_timeout(&sa, d)? Conectar con timeout explícito
stream.read(&mut buf)?              Leer (puede ser parcial, 0 = EOF)
stream.read_exact(&mut buf)?        Leer exactamente buf.len() bytes
stream.read_to_end(&mut vec)?       Leer hasta EOF
stream.write(data)?                 Escribir (puede ser parcial)
stream.write_all(data)?             Escribir todo (o error)
stream.flush()?                     Forzar envío de datos bufferizados
stream.shutdown(how)?               Cerrar Read/Write/Both
stream.try_clone()?                 Otro handle al mismo socket
stream.peer_addr()?                 Dirección del peer
stream.local_addr()?                Dirección local

──────────────────── Opciones ────────────────────────
stream.set_nodelay(true)?           TCP_NODELAY (Nagle off)
stream.set_read_timeout(Some(d))?   Timeout de lectura
stream.set_write_timeout(Some(d))?  Timeout de escritura
stream.set_nonblocking(bool)?       Modo non-blocking
stream.set_ttl(n)?                  IP TTL

──────────────────── Direcciones ─────────────────────
"127.0.0.1:8080"                    Solo localhost IPv4
"0.0.0.0:8080"                      Todas las interfaces IPv4
"[::1]:8080"                        Solo localhost IPv6
"[::]:8080"                         Todas las interfaces (dual-stack)
"example.com:80"                    DNS resolution (ToSocketAddrs)
addr.to_socket_addrs()?             Resolver manualmente

──────────────────── Framing ─────────────────────────
BufReader + lines()                 Protocolo basado en líneas
len.to_be_bytes() + data            Length-prefixed (binario)
read_exact para longitud fija       Leer N bytes exactos
\r\n\r\n como separador             Estilo HTTP headers/body

──────────────────── Shutdown ────────────────────────
Shutdown::Write                     "No envío más" → peer ve EOF
Shutdown::Read                      "No leo más"
Shutdown::Both                      Cerrar todo
drop(stream)                        close() — puede enviar RST
```

---

## 11. Ejercicios

### Ejercicio 1: Key-Value server

Implementa un servidor TCP que almacene y recupere pares clave-valor con un
protocolo de texto:

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Write};
use std::collections::HashMap;

fn handle_client(stream: TcpStream, store: &mut HashMap<String, String>) -> std::io::Result<()> {
    // TODO: Implementar protocolo:
    //   GET key       → responde "VALUE <value>\n" o "NOT_FOUND\n"
    //   SET key value → almacena y responde "OK\n"
    //   DEL key       → elimina y responde "DELETED\n" o "NOT_FOUND\n"
    //   LIST          → responde con todas las claves, una por línea,
    //                   terminando con "END\n"
    //   QUIT          → responde "BYE\n" y cierra
    //   (desconocido) → responde "ERROR Unknown command\n"
    //
    // Usar BufReader para leer líneas.
    // Cada comando es una línea terminada en \n.

    todo!()
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:6379")?;
    println!("KV server on :6379");

    let mut store = HashMap::new();

    // Nota: este servidor es single-threaded, un cliente a la vez.
    // El store no necesita Arc/Mutex porque nunca hay acceso concurrente.
    for stream in listener.incoming() {
        let stream = stream?;
        println!("Client: {}", stream.peer_addr()?);
        if let Err(e) = handle_client(stream, &mut store) {
            eprintln!("Error: {}", e);
        }
    }
    Ok(())
}

// Probar con: nc localhost 6379
// SET name Rust
// GET name
// LIST
// QUIT
```

**Pregunta de reflexión**: este servidor es single-threaded — solo atiende un cliente a
la vez. ¿Qué pasa si un cliente se conecta pero nunca envía datos? ¿Cómo afecta eso a
los demás clientes? ¿Qué mecanismo del tópico anterior (timeouts) podrías usar para
mitigar este problema?

### Ejercicio 2: File transfer (sender + receiver)

Implementa un par de programas para transferir archivos por TCP usando length-prefixed
framing:

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};
use std::fs;

// Protocolo:
// 1. Sender envía: [4 bytes: filename length][filename bytes]
// 2. Sender envía: [8 bytes: file size][file data bytes]
// 3. Receiver lee filename, luego file data, y escribe al disco

fn send_file(addr: &str, file_path: &str) -> std::io::Result<()> {
    // TODO:
    // 1. Conectar al receiver
    // 2. Obtener el nombre del archivo (solo file_name, no path completo)
    // 3. Enviar longitud del nombre + nombre
    // 4. Leer el archivo y obtener su tamaño
    // 5. Enviar tamaño (8 bytes, u64 big-endian) + contenido
    // 6. Imprimir "Sent <filename> (<size> bytes)"

    todo!()
}

fn receive_files(addr: &str, output_dir: &str) -> std::io::Result<()> {
    // TODO:
    // 1. Bind y escuchar
    // 2. Para cada conexión:
    //    a. Leer longitud del nombre + nombre
    //    b. Leer tamaño del archivo + datos
    //    c. Escribir a output_dir/filename
    //    d. Imprimir "Received <filename> (<size> bytes)"
    // 3. Validar: rechazar nombres con "/" o ".." (path traversal)
    // 4. Limitar tamaño máximo a 100 MB

    todo!()
}

fn main() -> std::io::Result<()> {
    let args: Vec<String> = std::env::args().collect();

    match args.get(1).map(|s| s.as_str()) {
        Some("send") => {
            let addr = args.get(2).map(|s| s.as_str()).unwrap_or("127.0.0.1:9000");
            let file = args.get(3).expect("Usage: send <addr> <file>");
            send_file(addr, file)?;
        }
        Some("recv") => {
            let addr = args.get(2).map(|s| s.as_str()).unwrap_or("127.0.0.1:9000");
            let dir = args.get(3).map(|s| s.as_str()).unwrap_or("./received");
            fs::create_dir_all(dir)?;
            receive_files(addr, dir)?;
        }
        _ => {
            println!("Usage:");
            println!("  recv [addr] [output_dir]");
            println!("  send <addr> <file>");
        }
    }
    Ok(())
}
```

**Pregunta de reflexión**: ¿por qué es importante validar el nombre del archivo en el
receptor? Si un atacante envía `"../../../etc/cron.d/malicious"` como nombre, ¿qué
podría pasar sin validación? ¿Cómo se relaciona esto con path traversal (T02_stdpath)?

### Ejercicio 3: Port scanner

Implementa un scanner de puertos TCP que verifique qué puertos están abiertos en
un host:

```rust
use std::net::{TcpStream, SocketAddr};
use std::time::Duration;

fn scan_port(addr: &str, port: u16, timeout: Duration) -> bool {
    // TODO: Intentar connect_timeout al addr:port
    // Retorna true si la conexión tuvo éxito (puerto abierto)
    // Retorna false si falló (puerto cerrado/filtrado/timeout)

    todo!()
}

fn scan_range(addr: &str, start: u16, end: u16, timeout: Duration) -> Vec<u16> {
    // TODO: Escanear todos los puertos en el rango [start, end]
    // Retornar lista de puertos abiertos
    // Imprimir progreso cada 100 puertos
    //
    // BONUS: paralelizar con threads (usar un thread pool de ~50 threads)
    // para que el scan no tarde minutos

    todo!()
}

fn main() {
    let addr = std::env::args().nth(1).unwrap_or_else(|| "127.0.0.1".into());
    let start: u16 = std::env::args().nth(2)
        .and_then(|s| s.parse().ok()).unwrap_or(1);
    let end: u16 = std::env::args().nth(3)
        .and_then(|s| s.parse().ok()).unwrap_or(1024);

    println!("Scanning {}:{}-{}", addr, start, end);
    let open = scan_range(&addr, start, end, Duration::from_millis(500));

    println!("\nOpen ports on {}:", addr);
    for port in &open {
        println!("  {} open", port);
    }
    println!("Scan complete: {} open ports", open.len());
}
```

**Pregunta de reflexión**: un scan secuencial de 65535 puertos con timeout de 500ms
tardaría ~9 horas. ¿Cuántos threads necesitarías para completar el scan en menos de
1 minuto? ¿Hay un límite práctico de threads concurrentes? (Hint: file descriptors,
memoria, y la perspectiva del host escaneado que ve miles de SYN simultáneos).