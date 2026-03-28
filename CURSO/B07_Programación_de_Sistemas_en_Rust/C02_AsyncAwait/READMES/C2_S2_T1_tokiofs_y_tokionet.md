# tokio::fs y tokio::net

## Indice
1. [I/O asincrono: por que no usar std](#1-io-asincrono-por-que-no-usar-std)
2. [tokio::net::TcpListener](#2-tokionettcplistener)
3. [tokio::net::TcpStream](#3-tokionettcpstream)
4. [AsyncRead y AsyncWrite](#4-asyncread-y-asyncwrite)
5. [Servidor TCP: accept loop con spawn](#5-servidor-tcp-accept-loop-con-spawn)
6. [tokio::net::UdpSocket](#6-tokionetudpsocket)
7. [tokio::fs: operaciones de archivo](#7-tokiofs-operaciones-de-archivo)
8. [BufReader y BufWriter async](#8-bufreader-y-bufwriter-async)
9. [Leer lineas de un TcpStream](#9-leer-lineas-de-un-tcpstream)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. I/O asincrono: por que no usar std

Las operaciones de `std::net` y `std::fs` son **bloqueantes**:
cuando llamas `read()`, el hilo se detiene hasta que haya datos.
En un runtime async, esto bloquea el worker thread e impide que
otras tareas se ejecuten:

```
    std::net (bloqueante):
    Worker 0: [accept] ---- bloqueado esperando conexion ----
    Worker 1: [read]   ---- bloqueado esperando datos -------
    Worker 2: [read]   ---- bloqueado esperando datos -------
    Worker 3: (libre)
    Resultado: 3 de 4 workers inutilizados

    tokio::net (asincrono):
    Worker 0: [accept][tarea X][tarea Y][accept][tarea Z]...
    Worker 1: [read A][read B][write C][read A][write B]...
    Resultado: 2 workers manejan miles de conexiones
```

### Equivalencias std -> tokio

```
+---------------------------+----------------------------------+
| std (bloqueante)          | tokio (async)                    |
+---------------------------+----------------------------------+
| std::net::TcpListener     | tokio::net::TcpListener          |
| std::net::TcpStream       | tokio::net::TcpStream            |
| std::net::UdpSocket       | tokio::net::UdpSocket            |
| std::fs::read_to_string   | tokio::fs::read_to_string        |
| std::fs::write            | tokio::fs::write                 |
| std::fs::File             | tokio::fs::File                  |
| std::io::BufReader        | tokio::io::BufReader             |
| std::io::BufWriter        | tokio::io::BufWriter             |
| std::io::Read             | tokio::io::AsyncRead             |
| std::io::Write            | tokio::io::AsyncWrite            |
| std::io::BufRead          | tokio::io::AsyncBufRead          |
+---------------------------+----------------------------------+
```

---

## 2. tokio::net::TcpListener

`TcpListener` escucha conexiones TCP entrantes. La API es
casi identica a `std::net::TcpListener` pero con `.await`:

```rust
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Listening on 127.0.0.1:8080");

    loop {
        // accept() retorna (TcpStream, SocketAddr)
        let (stream, addr) = listener.accept().await?;
        println!("New connection from: {}", addr);

        // Manejar la conexion (aqui solo la dropeamos)
        drop(stream);
    }
}
```

### Comparacion con std

```rust
// std (bloqueante):
let listener = std::net::TcpListener::bind("127.0.0.1:8080")?;
let (stream, addr) = listener.accept()?;  // bloquea el hilo

// tokio (async):
let listener = TcpListener::bind("127.0.0.1:8080").await?;
let (stream, addr) = listener.accept().await?;  // suspende la tarea
```

### Comparacion con C (B06/C08)

```c
// C (lo que hicimos en el servidor HTTP):
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
bind(server_fd, ...);
listen(server_fd, BACKLOG);
int client_fd = accept(server_fd, ...);  // bloquea
```

En C usamos `fork()` o un thread pool para manejar multiples
conexiones. En Tokio, `spawn` una tarea async por conexion
sin crear hilos del OS.

---

## 3. tokio::net::TcpStream

`TcpStream` representa una conexion TCP establecida. Soporta
lectura y escritura asincrona:

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn handle_connection(mut stream: TcpStream) -> std::io::Result<()> {
    // Leer datos
    let mut buf = vec![0u8; 1024];
    let n = stream.read(&mut buf).await?;
    println!("Received: {}", String::from_utf8_lossy(&buf[..n]));

    // Escribir respuesta
    stream.write_all(b"HTTP/1.1 200 OK\r\n\r\nHello!\n").await?;

    // Shutdown (equivale al shutdown(fd, SHUT_WR) de C)
    stream.shutdown().await?;

    Ok(())
}
```

### Conectarse como cliente

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn client() -> std::io::Result<()> {
    let mut stream = TcpStream::connect("127.0.0.1:8080").await?;

    // Enviar request
    stream.write_all(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n").await?;

    // Leer respuesta
    let mut response = String::new();
    stream.read_to_string(&mut response).await?;
    println!("Response: {}", response);

    Ok(())
}
```

### split: lectura y escritura concurrente

`TcpStream` no permite `read` y `write` simultaneos desde
la misma referencia. `split()` lo divide en dos mitades
independientes:

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

async fn echo(stream: TcpStream) -> std::io::Result<()> {
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);

    let mut line = String::new();
    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;
        if n == 0 { break; }  // conexion cerrada

        writer.write_all(line.as_bytes()).await?;
    }

    Ok(())
}
```

```
    TcpStream
    +--------------------+
    |  read + write      |
    +--------------------+
           |
      into_split()
           |
    +----------+  +----------+
    | OwnedRead|  |OwnedWrite|
    | Half     |  | Half     |
    +----------+  +----------+
    Puede estar     Puede estar
    en una tarea    en otra tarea
```

> **Nota**: `split()` (sin `into_`) retorna mitades con
> lifetime del stream (no owned). `into_split()` consume el
> stream y retorna mitades owned que puedes mover a tareas
> separadas.

---

## 4. AsyncRead y AsyncWrite

Los traits `AsyncRead` y `AsyncWrite` son los equivalentes
async de `std::io::Read` y `std::io::Write`. No los implementas
manualmente casi nunca — usas los **extension traits**:

### AsyncReadExt (metodos de conveniencia)

```rust
use tokio::io::AsyncReadExt;

async fn read_examples(stream: &mut TcpStream) -> std::io::Result<()> {
    // Leer hasta N bytes
    let mut buf = [0u8; 1024];
    let n = stream.read(&mut buf).await?;

    // Leer exactamente N bytes (error si hay menos)
    let mut exact = [0u8; 4];
    stream.read_exact(&mut exact).await?;

    // Leer todo hasta EOF
    let mut all = Vec::new();
    stream.read_to_end(&mut all).await?;

    // Leer todo como String
    let mut text = String::new();
    stream.read_to_string(&mut text).await?;

    Ok(())
}
```

### AsyncWriteExt (metodos de conveniencia)

```rust
use tokio::io::AsyncWriteExt;

async fn write_examples(stream: &mut TcpStream) -> std::io::Result<()> {
    // Escribir bytes
    stream.write(b"hello").await?;

    // Escribir TODOS los bytes (reintenta si es short write)
    stream.write_all(b"complete data").await?;

    // Flush el buffer interno
    stream.flush().await?;

    // Shutdown la escritura
    stream.shutdown().await?;

    Ok(())
}
```

### AsyncBufReadExt (lectura con buffer)

```rust
use tokio::io::{AsyncBufReadExt, BufReader};

async fn bufread_examples(stream: TcpStream) -> std::io::Result<()> {
    let reader = BufReader::new(stream);
    let mut lines = reader.lines();

    // Leer linea por linea
    while let Some(line) = lines.next_line().await? {
        println!("line: {}", line);
    }

    Ok(())
}
```

---

## 5. Servidor TCP: accept loop con spawn

El patron fundamental de un servidor async en Tokio:

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Server listening on 127.0.0.1:8080");

    loop {
        let (stream, addr) = listener.accept().await?;
        println!("[{}] connected", addr);

        // Spawn una tarea por conexion
        tokio::spawn(async move {
            if let Err(e) = handle_client(stream).await {
                eprintln!("[{}] error: {}", addr, e);
            }
            println!("[{}] disconnected", addr);
        });
    }
}

async fn handle_client(mut stream: tokio::net::TcpStream) -> std::io::Result<()> {
    let mut buf = vec![0u8; 1024];

    loop {
        let n = stream.read(&mut buf).await?;
        if n == 0 { return Ok(()); }  // conexion cerrada

        // Echo: devolver lo que recibimos
        stream.write_all(&buf[..n]).await?;
    }
}
```

### Comparacion con el servidor C de B06/C08

```
    C (fork por conexion):          Rust/Tokio (spawn por conexion):
    +------------------------+      +------------------------+
    | accept()               |      | accept().await         |
    | fork()                 |      | tokio::spawn(...)      |
    |   hijo: handle_client  |      |   tarea: handle_client |
    |   padre: continue      |      |   continue             |
    +------------------------+      +------------------------+
    Coste: ~1 proceso/conexion     Coste: ~KB/conexion
    Limite: ~miles de procesos      Limite: ~millones de tareas
```

### Servidor echo completo con graceful shutdown

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::signal;

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Echo server on 127.0.0.1:8080");

    loop {
        tokio::select! {
            result = listener.accept() => {
                let (stream, addr) = result?;
                tokio::spawn(async move {
                    if let Err(e) = echo(stream).await {
                        eprintln!("[{}] {}", addr, e);
                    }
                });
            }
            _ = signal::ctrl_c() => {
                println!("\nShutting down");
                break;
            }
        }
    }

    Ok(())
}

async fn echo(mut stream: tokio::net::TcpStream) -> std::io::Result<()> {
    let mut buf = vec![0u8; 4096];
    loop {
        let n = stream.read(&mut buf).await?;
        if n == 0 { return Ok(()); }
        stream.write_all(&buf[..n]).await?;
    }
}
```

---

## 6. tokio::net::UdpSocket

UDP es connectionless — no hay accept/connect. Envias y
recibes datagramas directamente:

```rust
use tokio::net::UdpSocket;

async fn udp_server() -> std::io::Result<()> {
    let socket = UdpSocket::bind("127.0.0.1:9000").await?;
    println!("UDP listening on 127.0.0.1:9000");

    let mut buf = [0u8; 1024];
    loop {
        let (len, addr) = socket.recv_from(&mut buf).await?;
        let msg = String::from_utf8_lossy(&buf[..len]);
        println!("[{}] {}", addr, msg);

        // Echo back
        socket.send_to(&buf[..len], addr).await?;
    }
}

async fn udp_client() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0").await?;

    socket.send_to(b"hello", "127.0.0.1:9000").await?;

    let mut buf = [0u8; 1024];
    let (len, _addr) = socket.recv_from(&mut buf).await?;
    println!("Response: {}", String::from_utf8_lossy(&buf[..len]));

    Ok(())
}
```

### connect para UDP "conectado"

```rust
async fn connected_udp() -> std::io::Result<()> {
    let socket = UdpSocket::bind("0.0.0.0:0").await?;
    socket.connect("127.0.0.1:9000").await?;

    // Despues de connect, usa send/recv en vez de send_to/recv_from
    socket.send(b"hello").await?;

    let mut buf = [0u8; 1024];
    let len = socket.recv(&mut buf).await?;
    println!("{}", String::from_utf8_lossy(&buf[..len]));

    Ok(())
}
```

---

## 7. tokio::fs: operaciones de archivo

`tokio::fs` proporciona versiones async de las operaciones de
`std::fs`. Internamente usa `spawn_blocking` — las operaciones
de archivo del OS son bloqueantes por naturaleza:

```rust
use tokio::fs;

async fn file_operations() -> std::io::Result<()> {
    // Leer archivo completo
    let content = fs::read_to_string("config.toml").await?;

    // Leer como bytes
    let bytes = fs::read("image.png").await?;

    // Escribir archivo
    fs::write("output.txt", "hello world").await?;

    // Crear directorio
    fs::create_dir_all("path/to/dir").await?;

    // Renombrar
    fs::rename("old.txt", "new.txt").await?;

    // Eliminar
    fs::remove_file("temp.txt").await?;
    fs::remove_dir_all("temp_dir").await?;

    // Metadata
    let meta = fs::metadata("file.txt").await?;
    println!("size: {} bytes", meta.len());

    Ok(())
}
```

### tokio::fs::File para lectura/escritura granular

```rust
use tokio::fs::File;
use tokio::io::{AsyncReadExt, AsyncWriteExt};

async fn file_rw() -> std::io::Result<()> {
    // Escribir
    let mut file = File::create("data.txt").await?;
    file.write_all(b"line 1\nline 2\nline 3\n").await?;
    file.flush().await?;

    // Leer
    let mut file = File::open("data.txt").await?;
    let mut contents = String::new();
    file.read_to_string(&mut contents).await?;
    println!("{}", contents);

    Ok(())
}
```

### Cuando usar tokio::fs vs spawn_blocking + std::fs

```
+----------------------------+-----------------------------------+
| tokio::fs                  | spawn_blocking + std::fs          |
+----------------------------+-----------------------------------+
| API async natural          | Mas control                       |
| Conveniente para ops       | Util si necesitas seek, lock,    |
| simples (read, write)      | o APIs no disponibles en tokio   |
| Internamente usa           | Puedes agrupar multiples ops     |
| spawn_blocking por op      | en un solo spawn_blocking        |
+----------------------------+-----------------------------------+
```

```rust
// tokio::fs: un spawn_blocking por operacion
let a = tokio::fs::read_to_string("a.txt").await?;
let b = tokio::fs::read_to_string("b.txt").await?;
// 2 spawn_blocking internos

// spawn_blocking agrupado: un hilo para ambas
let (a, b) = tokio::task::spawn_blocking(|| {
    let a = std::fs::read_to_string("a.txt")?;
    let b = std::fs::read_to_string("b.txt")?;
    Ok::<_, std::io::Error>((a, b))
}).await??;
// 1 spawn_blocking para ambos
```

---

## 8. BufReader y BufWriter async

Igual que en std, envolver un stream en `BufReader`/`BufWriter`
reduce el numero de syscalls agrupando lecturas/escrituras:

### BufReader

```rust
use tokio::io::{BufReader, AsyncBufReadExt};
use tokio::net::TcpStream;

async fn read_lines(stream: TcpStream) -> std::io::Result<()> {
    let reader = BufReader::new(stream);
    let mut lines = reader.lines();

    while let Some(line) = lines.next_line().await? {
        println!("> {}", line);
    }
    // lines retorna None cuando la conexion se cierra (EOF)

    Ok(())
}
```

### BufWriter

```rust
use tokio::io::{BufWriter, AsyncWriteExt};
use tokio::net::TcpStream;

async fn write_buffered(stream: TcpStream) -> std::io::Result<()> {
    let mut writer = BufWriter::new(stream);

    // Estas escrituras se acumulan en el buffer
    writer.write_all(b"HTTP/1.1 200 OK\r\n").await?;
    writer.write_all(b"Content-Type: text/plain\r\n").await?;
    writer.write_all(b"\r\n").await?;
    writer.write_all(b"Hello, World!\n").await?;

    // flush envia todo de una vez (una syscall en vez de cuatro)
    writer.flush().await?;

    Ok(())
}
```

### BufReader + BufWriter con split

```rust
use tokio::io::{BufReader, BufWriter, AsyncBufReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

async fn process(stream: TcpStream) -> std::io::Result<()> {
    let (read_half, write_half) = stream.into_split();
    let mut reader = BufReader::new(read_half);
    let mut writer = BufWriter::new(write_half);

    let mut line = String::new();
    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;
        if n == 0 { break; }

        let response = format!("echo: {}", line);
        writer.write_all(response.as_bytes()).await?;
        writer.flush().await?;
    }

    Ok(())
}
```

---

## 9. Leer lineas de un TcpStream

Patron muy comun para protocolos basados en texto (HTTP,
SMTP, IRC, protocolos custom):

### Con lines()

```rust
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::net::TcpStream;

async fn read_request(stream: &mut TcpStream) -> std::io::Result<Vec<String>> {
    let mut reader = BufReader::new(stream);
    let mut headers = Vec::new();

    let mut line = String::new();
    loop {
        line.clear();
        let n = reader.read_line(&mut line).await?;
        if n == 0 { break; }       // conexion cerrada
        if line == "\r\n" { break; } // fin de headers HTTP
        headers.push(line.trim_end().to_string());
    }

    Ok(headers)
}
```

### Con read_until (delimitador custom)

```rust
use tokio::io::{AsyncBufReadExt, BufReader};

async fn read_until_null(stream: tokio::net::TcpStream) -> std::io::Result<Vec<u8>> {
    let mut reader = BufReader::new(stream);
    let mut data = Vec::new();

    // Leer hasta encontrar byte nulo (0x00)
    reader.read_until(0x00, &mut data).await?;

    Ok(data)
}
```

### tokio::io::copy: copiar entre streams

```rust
use tokio::io;
use tokio::net::TcpStream;

async fn proxy(mut inbound: TcpStream, mut outbound: TcpStream) -> io::Result<()> {
    let (mut ri, mut wi) = inbound.split();
    let (mut ro, mut wo) = outbound.split();

    // Copiar en ambas direcciones concurrentemente
    tokio::select! {
        result = io::copy(&mut ri, &mut wo) => result?,
        result = io::copy(&mut ro, &mut wi) => result?,
    };

    Ok(())
}
```

`io::copy` es como `sendfile()` en C pero async — copia datos
entre un AsyncRead y un AsyncWrite eficientemente.

---

## 10. Errores comunes

### Error 1: usar std::net dentro de async

```rust
// MAL: bloquea el worker thread
async fn bad_server() {
    let listener = std::net::TcpListener::bind("0.0.0.0:8080").unwrap();
    let (stream, _) = listener.accept().unwrap();  // BLOQUEA
}
```

```rust
// BIEN: usar tokio::net
async fn good_server() {
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await.unwrap();
    let (stream, _) = listener.accept().await.unwrap();  // suspende
}
```

### Error 2: olvidar flush en BufWriter

```rust
// MAL: datos se quedan en el buffer y nunca se envian
async fn bad_write(stream: TcpStream) {
    let mut writer = BufWriter::new(stream);
    writer.write_all(b"hello").await.unwrap();
    // El buffer no se flushea antes de que writer se droppee
    // Los datos se PIERDEN si el drop no flushea
}
```

```rust
// BIEN: flush explicitamente
async fn good_write(stream: TcpStream) {
    let mut writer = BufWriter::new(stream);
    writer.write_all(b"hello").await.unwrap();
    writer.flush().await.unwrap();  // enviar ahora
}
```

> **Nota**: `BufWriter` de tokio **no** flushea en `Drop`
> (porque Drop es sincrono y no puede hacer `.await`). Siempre
> llama `.flush().await` o `.shutdown().await`.

### Error 3: no manejar read() retornando 0

```rust
// MAL: loop infinito cuando la conexion se cierra
async fn bad_read(mut stream: TcpStream) {
    let mut buf = [0u8; 1024];
    loop {
        let n = stream.read(&mut buf).await.unwrap();
        // Si n == 0, la conexion se cerro pero el loop sigue!
        process(&buf[..n]);
    }
}
```

```rust
// BIEN: verificar n == 0 (EOF)
async fn good_read(mut stream: TcpStream) {
    let mut buf = [0u8; 1024];
    loop {
        let n = stream.read(&mut buf).await.unwrap();
        if n == 0 { break; }  // conexion cerrada
        process(&buf[..n]);
    }
}
```

### Error 4: asumir que read() llena el buffer

```rust
// MAL: asumir que read llena todo el buffer
async fn bad_exact(mut stream: TcpStream) {
    let mut buf = [0u8; 100];
    stream.read(&mut buf).await.unwrap();
    // read puede retornar 1 byte aunque pediste 100!
    let msg = String::from_utf8_lossy(&buf);  // basura despues de n bytes
}
```

```rust
// BIEN: usar read_exact o verificar n
async fn good_exact(mut stream: TcpStream) {
    // Opcion A: read_exact (garantiza N bytes o error)
    let mut buf = [0u8; 100];
    stream.read_exact(&mut buf).await.unwrap();

    // Opcion B: usar el n retornado
    let mut buf = [0u8; 1024];
    let n = stream.read(&mut buf).await.unwrap();
    let msg = String::from_utf8_lossy(&buf[..n]);  // solo n bytes validos
}
```

### Error 5: spawn sin move con datos del stream

```rust
// MAL: stream movido a spawn sin move
async fn bad_spawn(listener: TcpListener) {
    let (stream, addr) = listener.accept().await.unwrap();
    tokio::spawn(async {
        handle(stream).await;  // ERROR: stream no es 'static sin move
    });
}
```

```rust
// BIEN: async move
async fn good_spawn(listener: TcpListener) {
    let (stream, addr) = listener.accept().await.unwrap();
    tokio::spawn(async move {
        handle(stream).await;  // OK: stream movido al Future
    });
}
```

---

## 11. Cheatsheet

```
TOKIO I/O - REFERENCIA RAPIDA
================================

Setup:
  tokio = { version = "1", features = ["full"] }
  use tokio::net::{TcpListener, TcpStream, UdpSocket};
  use tokio::io::{AsyncReadExt, AsyncWriteExt, AsyncBufReadExt};
  use tokio::io::{BufReader, BufWriter};

TCP Server:
  let listener = TcpListener::bind("addr:port").await?;
  loop {
      let (stream, addr) = listener.accept().await?;
      tokio::spawn(async move { handle(stream).await });
  }

TCP Client:
  let mut stream = TcpStream::connect("addr:port").await?;
  stream.write_all(data).await?;
  let n = stream.read(&mut buf).await?;

Leer:
  stream.read(&mut buf).await?          -> usize (puede ser parcial)
  stream.read_exact(&mut buf).await?    -> () (exacto o error)
  stream.read_to_end(&mut vec).await?   -> usize (hasta EOF)
  stream.read_to_string(&mut s).await?  -> usize (hasta EOF, UTF-8)

Escribir:
  stream.write(data).await?             -> usize (puede ser parcial)
  stream.write_all(data).await?         -> () (todo o error)
  stream.flush().await?                 -> ()
  stream.shutdown().await?              -> ()

Lectura bufferizada:
  let reader = BufReader::new(stream);
  reader.read_line(&mut line).await?    -> usize
  reader.lines().next_line().await?     -> Option<String>
  reader.read_until(delim, &mut v).await?

Split:
  let (rd, wr) = stream.split();         lifetime del stream
  let (rd, wr) = stream.into_split();    owned (para spawn)

Copy:
  tokio::io::copy(&mut reader, &mut writer).await?

UDP:
  let socket = UdpSocket::bind("addr:port").await?;
  socket.send_to(data, addr).await?
  let (n, addr) = socket.recv_from(&mut buf).await?

File:
  tokio::fs::read_to_string(path).await?
  tokio::fs::write(path, data).await?
  tokio::fs::File::create(path).await?
  tokio::fs::File::open(path).await?

Reglas:
  - n == 0 en read = EOF (conexion cerrada)
  - Siempre flush() BufWriter antes de drop
  - Usar async move en spawn con datos de conexion
  - std::net/fs bloquean → usar tokio::net/fs
  - read() puede retornar menos bytes que el buffer
```

---

## 12. Ejercicios

### Ejercicio 1: servidor echo con conteo

Implementa un servidor echo que cuente conexiones y bytes
transferidos:

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

struct Stats {
    connections: AtomicU64,
    bytes_echoed: AtomicU64,
}

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let stats = Arc::new(Stats {
        connections: AtomicU64::new(0),
        bytes_echoed: AtomicU64::new(0),
    });

    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("Echo server on :8080");

    // TODO: spawn una tarea que imprima stats cada 5 segundos

    loop {
        let (stream, addr) = listener.accept().await?;
        let stats = Arc::clone(&stats);

        tokio::spawn(async move {
            stats.connections.fetch_add(1, Ordering::Relaxed);
            // TODO: implementar echo loop
            // Contar bytes leidos y actualizar stats.bytes_echoed
            // Manejar errores con if let Err(e) = ...
        });
    }
}
```

Tareas:
1. Implementa el echo loop dentro del spawn
2. Implementa la tarea de stats que imprime cada 5 segundos
3. Prueba con `nc localhost 8080` o `telnet localhost 8080`
4. Abre multiples conexiones y verifica que stats es correcto

**Prediccion**: si abres 10 conexiones y envias 100 bytes cada
una, que mostrara stats? connections: 10, bytes: 1000?

> **Pregunta de reflexion**: usamos `AtomicU64` en vez de
> `Mutex<u64>` para las stats. En T02 de C01/S02 aprendimos
> que atomics son mas eficientes para contadores simples. Pero
> si quisieras una snapshot consistente (connections y bytes
> juntos), necesitarias Mutex. Por que?

### Ejercicio 2: servidor de archivos HTTP minimo

Construye un servidor HTTP que sirva archivos de un directorio,
combinando `tokio::net` y `tokio::fs`:

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader, BufWriter};
use tokio::fs;
use std::path::PathBuf;

const DOC_ROOT: &str = "./www";

async fn handle_request(stream: tokio::net::TcpStream) -> std::io::Result<()> {
    let (reader, writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut writer = BufWriter::new(writer);

    // Leer la request line
    let mut request_line = String::new();
    reader.read_line(&mut request_line).await?;

    // TODO: parsear metodo y path de la request line
    // "GET /index.html HTTP/1.1\r\n" -> path = "/index.html"

    // TODO: leer y descartar el resto de headers (hasta \r\n vacio)

    // TODO: construir la ruta al archivo: DOC_ROOT + path
    // Cuidado con path traversal (../)!

    // TODO: si el archivo existe, responder 200 con el contenido
    // Si no existe, responder 404

    // TODO: flush el writer

    Ok(())
}

#[tokio::main]
async fn main() -> std::io::Result<()> {
    // Crear directorio www con un archivo de prueba
    fs::create_dir_all(DOC_ROOT).await?;
    fs::write(format!("{}/index.html", DOC_ROOT), "<h1>Hello!</h1>").await?;

    let listener = TcpListener::bind("127.0.0.1:8080").await?;
    println!("HTTP server on http://127.0.0.1:8080");

    loop {
        let (stream, _) = listener.accept().await?;
        tokio::spawn(async move {
            if let Err(e) = handle_request(stream).await {
                eprintln!("Error: {}", e);
            }
        });
    }
}
```

Tareas:
1. Implementa el parser de la request line
2. Implementa la proteccion contra path traversal
   (verifica que la ruta resuelta esta dentro de DOC_ROOT)
3. Responde con Content-Type basico (text/html para .html,
   text/plain para el resto)
4. Prueba con `curl http://localhost:8080/index.html`

> **Pregunta de reflexion**: este servidor usa `tokio::fs::read`
> para leer archivos completos en memoria. En C08 de B06
> usamos `sendfile()` para zero-copy. Tokio no tiene un
> `sendfile` async directo. Como implementarias zero-copy en
> un servidor async Rust? (Pista: investiga `tokio::io::copy`
> con un `tokio::fs::File`.)

### Ejercicio 3: proxy TCP

Implementa un proxy TCP que reenvie trafico entre un cliente
y un servidor remoto:

```rust
use tokio::net::{TcpListener, TcpStream};
use tokio::io;

async fn proxy(local_addr: &str, remote_addr: &str) -> std::io::Result<()> {
    let listener = TcpListener::bind(local_addr).await?;
    println!("Proxy: {} -> {}", local_addr, remote_addr);

    loop {
        let (inbound, addr) = listener.accept().await?;
        let remote = remote_addr.to_string();

        tokio::spawn(async move {
            println!("[{}] connected", addr);
            match handle_proxy(inbound, &remote).await {
                Ok((up, down)) => {
                    println!("[{}] done: {} bytes up, {} bytes down", addr, up, down);
                }
                Err(e) => eprintln!("[{}] error: {}", addr, e),
            }
        });
    }
}

async fn handle_proxy(inbound: TcpStream, remote: &str) -> io::Result<(u64, u64)> {
    // TODO:
    // 1. Conectar al servidor remoto
    // 2. Split ambos streams
    // 3. Copiar datos en ambas direcciones con io::copy
    // 4. Usar tokio::try_join! o select! para ejecutar ambas copias
    // 5. Retornar bytes transferidos en cada direccion
    todo!()
}

#[tokio::main]
async fn main() -> std::io::Result<()> {
    // Proxy local:9090 -> httpbin.org:80
    proxy("127.0.0.1:9090", "httpbin.org:80").await
}
```

Tareas:
1. Implementa `handle_proxy` con `io::copy` bidireccional
2. Prueba: `curl -H "Host: httpbin.org" http://localhost:9090/get`
3. Observa los bytes transferidos en ambas direcciones
4. Que pasa si el servidor remoto cierra la conexion? Como lo
   manejas?

> **Pregunta de reflexion**: `io::copy` mueve datos de un
> `AsyncRead` a un `AsyncWrite` en chunks. No necesita cargar
> todo en memoria. Para un proxy, el throughput depende del
> tamano del buffer interno de `io::copy`. Tokio usa 8 KB por
> defecto. Como afectaria cambiar este tamano al rendimiento
> en conexiones de alta velocidad?
