# Servidor Multi-cliente

## Índice

1. [El problema del servidor single-threaded](#1-el-problema-del-servidor-single-threaded)
2. [Un thread por conexión](#2-un-thread-por-conexión)
3. [Thread pool](#3-thread-pool)
4. [Estado compartido entre clientes](#4-estado-compartido-entre-clientes)
5. [Graceful shutdown](#5-graceful-shutdown)
6. [Patrones de servidor completo](#6-patrones-de-servidor-completo)
7. [Límites y back-pressure](#7-límites-y-back-pressure)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema del servidor single-threaded

Un servidor que procesa un cliente a la vez bloquea a todos los demás mientras atiende
al actual:

```rust
use std::net::TcpListener;
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;

    for stream in listener.incoming() {
        let mut stream = stream?;
        // PROBLEMA: mientras este cliente está conectado,
        // NINGÚN otro cliente puede ser atendido
        let mut buf = [0u8; 1024];
        loop {
            let n = stream.read(&mut buf)?;
            if n == 0 { break; }
            stream.write_all(&buf[..n])?;
        }
    }
    Ok(())
}
```

```
Single-threaded:

  Client A ═══════════════════════════╗
                                      ║ A es atendido
  Client B ──── espera ─── espera ────╬────────════════════╗
                                      ║                    ║ B espera
  Client C ──── espera ─── espera ────╬── espera ── espera ─╬──────
                                      ║                    ║
  Tiempo ──────────────────────────────►

Multi-threaded:

  Client A ═══════════════════════════
  Client B ═══════════════════════════
  Client C ═══════════════════════════
  Todos atendidos simultáneamente
```

---

## 2. Un thread por conexión

### 2.1 Modelo básico

La solución más simple: crear un thread del OS por cada conexión entrante.

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write, BufRead, BufReader};
use std::thread;

fn handle_client(stream: TcpStream) {
    let addr = stream.peer_addr().unwrap();
    println!("[{}] Connected", addr);

    let mut writer = stream.try_clone().unwrap();
    let reader = BufReader::new(stream);

    for line in reader.lines() {
        match line {
            Ok(line) => {
                println!("[{}] {}", addr, line);
                if writeln!(writer, "echo: {}", line).is_err() {
                    break;
                }
            }
            Err(_) => break,
        }
    }

    println!("[{}] Disconnected", addr);
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    println!("Server on :8080");

    for stream in listener.incoming() {
        let stream = stream?;
        // Un thread nuevo por cada conexión
        thread::spawn(|| handle_client(stream));
    }

    Ok(())
}
```

### 2.2 Diagrama del modelo

```
                    ┌─────────────┐
  Client A ════════►│   Thread A  │ handle_client()
                    └─────────────┘
                    ┌─────────────┐
  Client B ════════►│   Thread B  │ handle_client()
  ┌──────────┐      └─────────────┘
  │ Listener │      ┌─────────────┐
  │ (accept) │═════►│   Thread C  │ handle_client()
  └──────────┘      └─────────────┘
  Main thread       ┌─────────────┐
  (loop)     ═════► │   Thread D  │ handle_client()
                    └─────────────┘

  Cada accept() crea un thread nuevo
  El main thread vuelve inmediatamente a accept()
```

### 2.3 Ventajas y desventajas

```
✅ Ventajas:
  - Muy simple de implementar
  - Cada cliente tiene aislamiento total
  - Código síncrono natural (sin async)
  - Blocking I/O funciona sin problemas

❌ Desventajas:
  - Cada thread consume ~8 MB de stack
  - 1000 clientes = 8 GB de memoria solo en stacks
  - Creación de threads tiene overhead (~50μs por thread)
  - El scheduler del kernel degrada con miles de threads
  - Sin límite: un atacante puede agotar recursos (fork bomb)
```

### 2.4 Con join para esperar threads

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};
use std::thread::{self, JoinHandle};

fn handle_client(mut stream: TcpStream) {
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) => return,
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
            Err(_) => return,
        }
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let mut handles: Vec<JoinHandle<()>> = Vec::new();

    // Aceptar 10 clientes y luego esperar a que todos terminen
    for stream in listener.incoming().take(10) {
        let stream = stream?;
        handles.push(thread::spawn(|| handle_client(stream)));
    }

    println!("Waiting for {} clients to finish...", handles.len());
    for handle in handles {
        handle.join().unwrap();
    }
    println!("All done");

    Ok(())
}
```

---

## 3. Thread pool

### 3.1 ¿Por qué un pool?

Un thread pool reutiliza un conjunto fijo de threads en vez de crear uno nuevo por
conexión. Resuelve los problemas de escalabilidad del modelo anterior.

```
Thread por conexión:          Thread pool:
  Conexión → Thread nuevo       Conexión → Cola → Worker existente
  100 conn = 100 threads        100 conn = N workers (N fijo)
  Sin límite de threads         Máximo N conexiones simultáneas
  ~50μs por creación           0μs (reutilización)

Pool con 4 workers:
  ┌────────────────────┐
  │    Job Queue       │
  │  [C5][C6][C7]...   │ ← conexiones esperando
  └────┬───┬───┬───┬───┘
       │   │   │   │
  ┌────▼┐┌─▼──┐┌─▼─┐┌─▼──┐
  │ W-0 ││ W-1││W-2 ││ W-3│ ← workers (threads fijos)
  │ C1  ││ C2 ││ C3 ││ C4 │ ← conexiones siendo atendidas
  └─────┘└────┘└────┘└────┘
```

### 3.2 Thread pool manual con `mpsc`

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write, BufRead, BufReader};
use std::sync::mpsc;
use std::thread;

struct ThreadPool {
    workers: Vec<thread::JoinHandle<()>>,
    sender: Option<mpsc::Sender<TcpStream>>,
}

impl ThreadPool {
    fn new(size: usize) -> Self {
        let (sender, receiver) = mpsc::channel::<TcpStream>();
        let receiver = std::sync::Arc::new(std::sync::Mutex::new(receiver));

        let mut workers = Vec::with_capacity(size);

        for id in 0..size {
            let rx = receiver.clone();
            workers.push(thread::spawn(move || {
                loop {
                    // Tomar un job de la cola (bloquea si está vacía)
                    let stream = {
                        let lock = rx.lock().unwrap();
                        lock.recv()
                    };

                    match stream {
                        Ok(stream) => {
                            println!("[Worker {}] Handling client", id);
                            handle_client(stream);
                        }
                        Err(_) => {
                            // Channel cerrado → shutdown
                            println!("[Worker {}] Shutting down", id);
                            return;
                        }
                    }
                }
            }));
        }

        ThreadPool {
            workers,
            sender: Some(sender),
        }
    }

    fn execute(&self, stream: TcpStream) {
        if let Some(ref sender) = self.sender {
            sender.send(stream).unwrap();
        }
    }
}

impl Drop for ThreadPool {
    fn drop(&mut self) {
        // Cerrar el channel → workers terminan
        drop(self.sender.take());

        for worker in self.workers.drain(..) {
            worker.join().unwrap();
        }
    }
}

fn handle_client(stream: TcpStream) {
    let addr = stream.peer_addr().unwrap();
    let mut writer = stream.try_clone().unwrap();
    let reader = BufReader::new(stream);

    for line in reader.lines() {
        match line {
            Ok(line) => {
                if writeln!(writer, "echo: {}", line).is_err() {
                    break;
                }
            }
            Err(_) => break,
        }
    }
    println!("[{}] Done", addr);
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let pool = ThreadPool::new(4);

    println!("Server on :8080 (4 workers)");

    for stream in listener.incoming() {
        let stream = stream?;
        pool.execute(stream);
    }

    Ok(())
}
```

### 3.3 Con la crate `threadpool`

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Write};
use threadpool::ThreadPool;

fn handle_client(stream: TcpStream) {
    let mut writer = stream.try_clone().unwrap();
    let reader = BufReader::new(stream);

    for line in reader.lines().flatten() {
        if writeln!(writer, "echo: {}", line).is_err() {
            break;
        }
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let pool = ThreadPool::new(8);

    println!("Server on :8080 ({} workers)", pool.max_count());

    for stream in listener.incoming() {
        let stream = stream?;
        pool.execute(move || handle_client(stream));
    }

    Ok(())
}
```

### 3.4 Dimensionar el pool

```
¿Cuántos workers?

  I/O-bound (la mayoría de servidores):
    workers = num_cores × 2 a × 4
    Cada worker pasa la mayoría del tiempo esperando I/O
    Más workers aprovechan el tiempo de espera

  CPU-bound (cálculo pesado):
    workers = num_cores
    Más workers que cores causa context switching innecesario

  Mixto:
    workers = num_cores × 2 (ajustar según benchmarks)

Obtener número de cores:
  std::thread::available_parallelism().unwrap().get()
```

---

## 4. Estado compartido entre clientes

### 4.1 `Arc<Mutex<T>>` — El patrón fundamental

Cuando los clientes necesitan acceder a un estado compartido (contadores, lista de
usuarios, etc.), se usa `Arc<Mutex<T>>`:

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Write};
use std::sync::{Arc, Mutex};
use std::thread;
use std::collections::HashMap;

type SharedStore = Arc<Mutex<HashMap<String, String>>>;

fn handle_client(stream: TcpStream, store: SharedStore) {
    let addr = stream.peer_addr().unwrap();
    let mut writer = stream.try_clone().unwrap();
    let reader = BufReader::new(stream);

    for line in reader.lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => break,
        };

        let parts: Vec<&str> = line.trim().splitn(3, ' ').collect();

        let response = match parts.as_slice() {
            ["GET", key] => {
                let store = store.lock().unwrap();
                match store.get(*key) {
                    Some(val) => format!("VALUE {}\n", val),
                    None => "NOT_FOUND\n".to_string(),
                }
            }
            ["SET", key, value] => {
                let mut store = store.lock().unwrap();
                store.insert(key.to_string(), value.to_string());
                "OK\n".to_string()
            }
            ["COUNT"] => {
                let store = store.lock().unwrap();
                format!("COUNT {}\n", store.len())
            }
            _ => "ERROR Unknown command\n".to_string(),
        };

        if writer.write_all(response.as_bytes()).is_err() {
            break;
        }
    }
    println!("[{}] Disconnected", addr);
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let store: SharedStore = Arc::new(Mutex::new(HashMap::new()));

    println!("KV server on :8080");

    for stream in listener.incoming() {
        let stream = stream?;
        let store = store.clone();
        thread::spawn(move || handle_client(stream, store));
    }

    Ok(())
}
```

```
                    ┌─────────────┐
  Client A ════════►│  Thread A   │
                    │  store.lock │──┐
                    └─────────────┘  │
                    ┌─────────────┐  │   ┌─────────────────┐
  Client B ════════►│  Thread B   │  ├──►│ Arc<Mutex<Map>> │
                    │  store.lock │──┤   │ Shared state    │
                    └─────────────┘  │   └─────────────────┘
                    ┌─────────────┐  │
  Client C ════════►│  Thread C   │  │
                    │  store.lock │──┘
                    └─────────────┘
```

### 4.2 `RwLock` para reads frecuentes

Si la mayoría de operaciones son lecturas, `RwLock` permite múltiples lectores
concurrentes:

```rust
use std::sync::{Arc, RwLock};
use std::collections::HashMap;

type SharedConfig = Arc<RwLock<HashMap<String, String>>>;

fn handle_read(config: &SharedConfig, key: &str) -> Option<String> {
    let guard = config.read().unwrap(); // Múltiples lectores simultáneos
    guard.get(key).cloned()
}

fn handle_write(config: &SharedConfig, key: String, value: String) {
    let mut guard = config.write().unwrap(); // Escritor exclusivo
    guard.insert(key, value);
}
```

```
Mutex:                          RwLock:
  Thread A: lock → READ           Thread A: read() → READ    ✓
  Thread B: espera                 Thread B: read() → READ    ✓ (concurrente)
  Thread C: espera                 Thread C: write() → espera ✗
                                   (espera a que A y B terminen)

  Un solo acceso a la vez           Múltiples lectores O un escritor
```

### 4.3 Contador de conexiones activas

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::thread;

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let active = Arc::new(AtomicUsize::new(0));
    let total = Arc::new(AtomicUsize::new(0));

    println!("Server on :8080");

    for stream in listener.incoming() {
        let stream = stream?;
        let active = active.clone();
        let total = total.clone();

        let count = active.fetch_add(1, Ordering::Relaxed) + 1;
        let total_count = total.fetch_add(1, Ordering::Relaxed) + 1;
        println!("Connection #{} (active: {})", total_count, count);

        thread::spawn(move || {
            handle_echo(stream);
            let remaining = active.fetch_sub(1, Ordering::Relaxed) - 1;
            println!("Disconnected (active: {})", remaining);
        });
    }

    Ok(())
}

fn handle_echo(mut stream: TcpStream) {
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) | Err(_) => return,
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
        }
    }
}
```

### 4.4 Lista de clientes conectados

```rust
use std::net::{TcpListener, TcpStream, SocketAddr};
use std::io::{BufRead, BufReader, Write};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::thread;

type ClientList = Arc<Mutex<HashMap<SocketAddr, String>>>;

fn handle_client(stream: TcpStream, clients: ClientList) {
    let addr = stream.peer_addr().unwrap();
    let mut writer = stream.try_clone().unwrap();
    let reader = BufReader::new(stream);

    // Registrar cliente
    {
        let mut list = clients.lock().unwrap();
        list.insert(addr, "anonymous".to_string());
    }

    writeln!(writer, "Welcome! Commands: NAME <name>, WHO, QUIT").unwrap();

    for line in reader.lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => break,
        };

        let parts: Vec<&str> = line.trim().splitn(2, ' ').collect();
        match parts.as_slice() {
            ["NAME", name] => {
                let mut list = clients.lock().unwrap();
                list.insert(addr, name.to_string());
                writeln!(writer, "OK, you are now '{}'", name).unwrap();
            }
            ["WHO"] => {
                let list = clients.lock().unwrap();
                writeln!(writer, "Connected users:").unwrap();
                for (a, name) in list.iter() {
                    let marker = if *a == addr { " (you)" } else { "" };
                    writeln!(writer, "  {} — {}{}", a, name, marker).unwrap();
                }
            }
            ["QUIT"] => {
                writeln!(writer, "BYE").unwrap();
                break;
            }
            _ => {
                writeln!(writer, "Unknown command").unwrap();
            }
        }
    }

    // Desregistrar cliente
    {
        let mut list = clients.lock().unwrap();
        list.remove(&addr);
    }
    println!("[{}] Left", addr);
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let clients: ClientList = Arc::new(Mutex::new(HashMap::new()));

    println!("Server on :8080");

    for stream in listener.incoming() {
        let stream = stream?;
        let clients = clients.clone();
        thread::spawn(move || handle_client(stream, clients));
    }
    Ok(())
}
```

---

## 5. Graceful shutdown

### 5.1 El problema

`listener.incoming()` bloquea indefinidamente. ¿Cómo parar el servidor limpiamente?

### 5.2 Con `AtomicBool` + non-blocking accept

```rust
use std::net::TcpListener;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;
use std::io::{Read, Write};

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    listener.set_nonblocking(true)?;

    let running = Arc::new(AtomicBool::new(true));

    // Ctrl+C handler
    let r = running.clone();
    ctrlc_handler(r);

    println!("Server on :8080 (Ctrl+C to stop)");

    let mut handles = Vec::new();

    while running.load(Ordering::Relaxed) {
        match listener.accept() {
            Ok((stream, addr)) => {
                println!("Client: {}", addr);
                handles.push(thread::spawn(move || {
                    handle_echo(stream);
                }));
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                // Sin conexiones pendientes — esperar un poco
                thread::sleep(Duration::from_millis(100));
            }
            Err(e) => {
                eprintln!("Accept error: {}", e);
            }
        }
    }

    println!("\nShutting down...");
    println!("Waiting for {} clients to finish", handles.len());

    for handle in handles {
        handle.join().unwrap();
    }

    println!("Server stopped cleanly");
    Ok(())
}

fn handle_echo(mut stream: std::net::TcpStream) {
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) | Err(_) => return,
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
        }
    }
}

fn ctrlc_handler(running: Arc<AtomicBool>) {
    // Implementación simplificada — en producción usar la crate ctrlc
    // ctrlc::set_handler(move || running.store(false, Ordering::Relaxed));

    // Alternativa manual con signal:
    thread::spawn(move || {
        // Esperar a una señal de parada (simplificación)
        thread::sleep(Duration::from_secs(30)); // Simular Ctrl+C después de 30s
        running.store(false, Ordering::Relaxed);
    });
}
```

### 5.3 Con `ctrlc` crate (producción)

```rust
use std::net::TcpListener;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    listener.set_nonblocking(true)?;

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    // Con la crate ctrlc:
    // ctrlc::set_handler(move || {
    //     println!("\nReceived Ctrl+C, shutting down...");
    //     r.store(false, Ordering::SeqCst);
    // }).expect("Error setting Ctrl-C handler");

    while running.load(Ordering::SeqCst) {
        match listener.accept() {
            Ok((stream, addr)) => {
                let running = running.clone();
                std::thread::spawn(move || {
                    // Pasar running al handler para que pueda verificar
                    handle_client_with_shutdown(stream, running);
                });
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                std::thread::sleep(Duration::from_millis(50));
            }
            Err(e) => eprintln!("Accept error: {}", e),
        }
    }

    Ok(())
}

fn handle_client_with_shutdown(
    mut stream: std::net::TcpStream,
    running: Arc<AtomicBool>,
) {
    use std::io::{Read, Write};

    stream.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    let mut buf = [0u8; 4096];

    while running.load(Ordering::Relaxed) {
        match stream.read(&mut buf) {
            Ok(0) => return,
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => continue,
            Err(_) => return,
        }
    }
    // Shutdown limpio: notificar al cliente
    let _ = stream.write_all(b"Server shutting down\n");
}
```

---

## 6. Patrones de servidor completo

### 6.1 Servidor con thread pool + estado + shutdown

```rust
use std::net::{TcpListener, TcpStream, Shutdown};
use std::io::{BufRead, BufReader, Write};
use std::sync::{Arc, Mutex, atomic::{AtomicBool, AtomicUsize, Ordering}};
use std::collections::HashMap;
use std::thread;

struct Server {
    listener: TcpListener,
    store: Arc<Mutex<HashMap<String, String>>>,
    active_connections: Arc<AtomicUsize>,
    total_connections: Arc<AtomicUsize>,
    running: Arc<AtomicBool>,
    max_connections: usize,
}

impl Server {
    fn new(addr: &str, max_connections: usize) -> std::io::Result<Self> {
        let listener = TcpListener::bind(addr)?;
        listener.set_nonblocking(true)?;

        Ok(Server {
            listener,
            store: Arc::new(Mutex::new(HashMap::new())),
            active_connections: Arc::new(AtomicUsize::new(0)),
            total_connections: Arc::new(AtomicUsize::new(0)),
            running: Arc::new(AtomicBool::new(true)),
            max_connections,
        })
    }

    fn run(&self) {
        println!("Server running (max {} connections)", self.max_connections);

        while self.running.load(Ordering::Relaxed) {
            match self.listener.accept() {
                Ok((stream, addr)) => {
                    let active = self.active_connections.load(Ordering::Relaxed);
                    if active >= self.max_connections {
                        eprintln!("Max connections reached, rejecting {}", addr);
                        let mut s = stream;
                        let _ = s.write_all(b"Server full, try later\n");
                        let _ = s.shutdown(Shutdown::Both);
                        continue;
                    }

                    let id = self.total_connections.fetch_add(1, Ordering::Relaxed);
                    self.active_connections.fetch_add(1, Ordering::Relaxed);

                    let store = self.store.clone();
                    let active = self.active_connections.clone();
                    let running = self.running.clone();

                    thread::spawn(move || {
                        println!("[#{}] {} connected", id, addr);
                        Self::handle(stream, store, &running);
                        let remaining = active.fetch_sub(1, Ordering::Relaxed) - 1;
                        println!("[#{}] {} disconnected (active: {})", id, addr, remaining);
                    });
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    thread::sleep(std::time::Duration::from_millis(50));
                }
                Err(e) => eprintln!("Accept error: {}", e),
            }
        }

        println!("Server stopped");
    }

    fn handle(
        stream: TcpStream,
        store: Arc<Mutex<HashMap<String, String>>>,
        running: &AtomicBool,
    ) {
        stream.set_read_timeout(
            Some(std::time::Duration::from_secs(1))
        ).unwrap();

        let mut writer = stream.try_clone().unwrap();
        let reader = BufReader::new(stream);
        let mut lines = reader.lines();

        while running.load(Ordering::Relaxed) {
            let line = match lines.next() {
                Some(Ok(line)) => line,
                Some(Err(ref e))
                    if e.kind() == std::io::ErrorKind::WouldBlock => continue,
                _ => break,
            };

            let parts: Vec<&str> = line.trim().splitn(3, ' ').collect();
            let response = match parts.as_slice() {
                ["GET", key] => {
                    let s = store.lock().unwrap();
                    match s.get(*key) {
                        Some(v) => format!("{}\n", v),
                        None => "NOT_FOUND\n".into(),
                    }
                }
                ["SET", key, val] => {
                    store.lock().unwrap()
                        .insert(key.to_string(), val.to_string());
                    "OK\n".into()
                }
                ["QUIT"] => {
                    let _ = writer.write_all(b"BYE\n");
                    return;
                }
                _ => "ERROR\n".into(),
            };

            if writer.write_all(response.as_bytes()).is_err() {
                return;
            }
        }
    }

    fn stop(&self) {
        self.running.store(false, Ordering::Relaxed);
    }
}

fn main() -> std::io::Result<()> {
    let server = Server::new("127.0.0.1:8080", 100)?;
    println!("KV Server on :8080");
    server.run();
    Ok(())
}
```

### 6.2 Comparación de modelos

```
┌────────────────────────┬────────┬───────────┬────────────┬──────────┐
│ Modelo                 │ Complejidad │ Escala │ Latencia  │ Memoria  │
├────────────────────────┼────────┼───────────┼────────────┼──────────┤
│ Single-threaded        │ Baja   │ 1 cliente │ Bloqueado  │ Mínima   │
│ Thread por conexión    │ Baja   │ ~1-10K    │ Buena      │ ~8MB/conn│
│ Thread pool            │ Media  │ ~1-10K    │ Buena*     │ Fija     │
│ Async (Tokio)          │ Alta   │ ~100K-1M  │ Buena      │ ~KB/conn │
└────────────────────────┴────────┴───────────┴────────────┴──────────┘

* Thread pool: la latencia depende de que haya workers disponibles.
  Si todos están ocupados, las conexiones nuevas esperan en la cola.
```

---

## 7. Límites y back-pressure

### 7.1 Limitar conexiones concurrentes

```rust
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Semaphore};
use std::thread;

// Nota: std::sync::Semaphore no existe en std.
// Implementación manual o usar parking_lot/tokio.
// Aquí usamos AtomicUsize como semáforo simplificado:

use std::sync::atomic::{AtomicUsize, Ordering};

struct ConnectionLimiter {
    current: AtomicUsize,
    max: usize,
}

impl ConnectionLimiter {
    fn new(max: usize) -> Self {
        Self {
            current: AtomicUsize::new(0),
            max,
        }
    }

    fn try_acquire(&self) -> bool {
        let mut current = self.current.load(Ordering::Relaxed);
        loop {
            if current >= self.max {
                return false;
            }
            match self.current.compare_exchange_weak(
                current,
                current + 1,
                Ordering::Relaxed,
                Ordering::Relaxed,
            ) {
                Ok(_) => return true,
                Err(c) => current = c,
            }
        }
    }

    fn release(&self) {
        self.current.fetch_sub(1, Ordering::Relaxed);
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let limiter = Arc::new(ConnectionLimiter::new(50));

    for stream in listener.incoming() {
        let stream = stream?;
        let limiter = limiter.clone();

        if !limiter.try_acquire() {
            // Rechazar conexión limpiamente
            let mut s = stream;
            let _ = std::io::Write::write_all(
                &mut s, b"Server busy, try later\n"
            );
            continue;
        }

        thread::spawn(move || {
            handle_echo(stream);
            limiter.release();
        });
    }
    Ok(())
}

fn handle_echo(mut stream: TcpStream) {
    use std::io::{Read, Write};
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) | Err(_) => return,
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
        }
    }
}
```

### 7.2 Timeout de inactividad

```rust
use std::net::TcpStream;
use std::io::{Read, Write};
use std::time::Duration;

fn handle_with_idle_timeout(mut stream: TcpStream, idle_timeout: Duration) {
    stream.set_read_timeout(Some(idle_timeout)).unwrap();

    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) => return, // Client disconnected
            Ok(n) => {
                if stream.write_all(&buf[..n]).is_err() {
                    return;
                }
                // Timeout se resetea automáticamente en la próxima read
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock
                        || e.kind() == std::io::ErrorKind::TimedOut => {
                let _ = stream.write_all(b"Idle timeout, disconnecting\n");
                return;
            }
            Err(_) => return,
        }
    }
}
```

### 7.3 Limitar tamaño de requests

```rust
use std::io::{BufRead, BufReader};
use std::net::TcpStream;

const MAX_LINE_LENGTH: usize = 4096;
const MAX_LINES_PER_REQUEST: usize = 100;

fn read_limited_request(stream: &TcpStream) -> std::io::Result<Vec<String>> {
    let reader = BufReader::new(stream);
    let mut lines = Vec::new();

    for line in reader.lines() {
        let line = line?;

        if line.len() > MAX_LINE_LENGTH {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Line too long",
            ));
        }

        if line.is_empty() {
            break; // Fin del request
        }

        lines.push(line);

        if lines.len() > MAX_LINES_PER_REQUEST {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Too many lines",
            ));
        }
    }

    Ok(lines)
}
```

---

## 8. Errores comunes

### Error 1: No limitar el número de threads

```rust
use std::net::TcpListener;
use std::thread;

// MAL — un atacante puede crear miles de conexiones
fn bad_server() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080")?;

    for stream in listener.incoming() {
        let stream = stream?;
        thread::spawn(move || {
            handle(stream); // Sin límite de threads
        });
    }
    Ok(())
}

// BIEN — limitar con un pool o un contador
fn good_server() -> std::io::Result<()> {
    let listener = TcpListener::bind("0.0.0.0:8080")?;
    let pool = threadpool::ThreadPool::new(64); // Máximo 64 threads

    for stream in listener.incoming() {
        let stream = stream?;
        pool.execute(move || handle(stream));
    }
    Ok(())
}

fn handle(_stream: std::net::TcpStream) {}
```

**Por qué importa**: cada thread consume ~8 MB de stack. Un atacante que abre 10,000
conexiones (trivial con un script) consume 80 GB y crashea el servidor.

### Error 2: Mutex poisoning no manejado

```rust
use std::sync::{Arc, Mutex};
use std::collections::HashMap;

// MAL — panic propagado por lock().unwrap()
fn bad_access(store: &Arc<Mutex<HashMap<String, String>>>) {
    let mut guard = store.lock().unwrap(); // PANIC si otro thread paniceó con el lock
    guard.insert("key".into(), "value".into());
}

// BIEN — manejar el poisoning
fn good_access(store: &Arc<Mutex<HashMap<String, String>>>) -> Option<()> {
    let mut guard = match store.lock() {
        Ok(g) => g,
        Err(poisoned) => {
            // El Mutex fue envenenado — otro thread paniceó
            // Podemos recuperar el guard si queremos:
            eprintln!("Mutex poisoned, recovering");
            poisoned.into_inner()
        }
    };
    guard.insert("key".into(), "value".into());
    Some(())
}
```

**Por qué ocurre**: si un thread paniquea mientras tiene el `MutexGuard`, el `Mutex`
se marca como "poisoned". Futuros `lock()` retornan `Err(PoisonError)`. `unwrap()` en
ese `Err` causa un panic en cascada.

### Error 3: Mantener el lock del Mutex mientras haces I/O

```rust
use std::sync::{Arc, Mutex};
use std::net::TcpStream;
use std::io::Write;

// MAL — lock retenido durante write (I/O lento)
fn bad_broadcast(
    clients: &Arc<Mutex<Vec<TcpStream>>>,
    msg: &[u8],
) {
    let mut clients = clients.lock().unwrap();
    for client in clients.iter_mut() {
        client.write_all(msg).unwrap(); // I/O con lock retenido
        // Si un cliente es lento, TODOS los demás threads esperan el lock
    }
}

// BIEN — copiar lo necesario y soltar el lock antes de I/O
fn good_broadcast(
    clients: &Arc<Mutex<Vec<TcpStream>>>,
    msg: &[u8],
) {
    // Clonar los streams bajo el lock (rápido)
    let streams: Vec<TcpStream> = {
        let clients = clients.lock().unwrap();
        clients.iter()
            .filter_map(|c| c.try_clone().ok())
            .collect()
    }; // Lock liberado aquí

    // I/O sin lock
    for mut stream in streams {
        let _ = stream.write_all(msg);
    }
}
```

### Error 4: No cerrar streams de clientes desconectados

```rust
use std::sync::{Arc, Mutex};
use std::net::TcpStream;
use std::io::Write;

// MAL — acumular streams muertos en la lista
fn bad_send(clients: &Arc<Mutex<Vec<TcpStream>>>, msg: &[u8]) {
    let clients = clients.lock().unwrap();
    for mut client in clients.iter() {
        let _ = client.write_all(msg); // Error ignorado
        // El stream muerto sigue en la lista → memoria leak
    }
}

// BIEN — limpiar streams que fallaron
fn good_send(clients: &Arc<Mutex<Vec<TcpStream>>>, msg: &[u8]) {
    let mut clients = clients.lock().unwrap();
    clients.retain(|client| {
        // try_clone para no consumir el stream
        match client.try_clone() {
            Ok(mut clone) => clone.write_all(msg).is_ok(),
            Err(_) => false, // Stream ya cerrado
        }
    });
}
```

### Error 5: Thread pool con cola ilimitada

```rust
// MAL — si los workers están saturados, la cola crece sin límite
fn bad_pool() {
    let pool = threadpool::ThreadPool::new(4);

    // Si llegan 10,000 conexiones por segundo y cada una tarda 1 segundo,
    // la cola acumula ~9,996 jobs pendientes, consumiendo memoria
    for stream in listener.incoming() {
        pool.execute(move || handle(stream.unwrap()));
        // La cola puede crecer a millones de jobs
    }
}

// BIEN — rechazar cuando la cola está llena
use std::sync::mpsc;

fn good_pool() {
    let (tx, rx) = mpsc::sync_channel::<std::net::TcpStream>(100); // Cola de 100
    // Los workers consumen del channel...

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        match tx.try_send(stream) {
            Ok(()) => {} // Aceptado
            Err(mpsc::TrySendError::Full(stream)) => {
                // Cola llena: rechazar
                let mut s = stream;
                let _ = std::io::Write::write_all(&mut s, b"Server busy\n");
            }
            Err(_) => break, // Pool cerrado
        }
    }
}

fn handle(_s: std::net::TcpStream) {}
static listener: std::sync::LazyLock<std::net::TcpListener> =
    std::sync::LazyLock::new(|| std::net::TcpListener::bind("0.0.0.0:0").unwrap());
```

---

## 9. Cheatsheet

```text
──────────────────── Modelos ─────────────────────────
Single-threaded              1 cliente a la vez
Thread por conexión          thread::spawn por accept()
Thread pool (manual)         mpsc channel + N workers
Thread pool (crate)          threadpool::ThreadPool::new(N)
Async (Tokio)                tokio::spawn por accept()

──────────────────── Estado compartido ───────────────
Arc<Mutex<T>>                Acceso exclusivo (reads y writes)
Arc<RwLock<T>>               Múltiples readers O un writer
Arc<AtomicUsize>             Contadores sin lock
Arc::clone(&data)            Clonar el Arc para cada thread

──────────────────── Thread pool manual ──────────────
mpsc::channel()              Cola ilimitada (cuidado)
mpsc::sync_channel(N)        Cola limitada (back-pressure)
rx.recv()                    Worker espera por job
drop(sender)                 Cerrar channel → workers terminan

──────────────────── Límites ─────────────────────────
Max conexiones               AtomicUsize o Semaphore
Idle timeout                 set_read_timeout(Some(dur))
Max request size             Contar bytes, rechazar si > N
Max line length              Verificar len() en cada línea
Queue back-pressure          sync_channel(N) o try_send

──────────────────── Graceful shutdown ───────────────
AtomicBool (running)         Flag compartido
set_nonblocking(true)        Accept no bloqueante
ctrlc crate                  Handler de Ctrl+C
Drenar workers               join() de todos los handles
Notificar clientes           write "server shutting down"

──────────────────── Dimensionamiento ────────────────
I/O-bound                   workers = cores × 2-4
CPU-bound                   workers = cores
available_parallelism()      Detectar número de cores
Thread por conexión          Hasta ~1-10K conexiones
Más de 10K                   Usar async (Tokio)
```

---

## 10. Ejercicios

### Ejercicio 1: Echo server con thread pool manual

Implementa un servidor echo con un thread pool construido desde cero:

```rust
use std::net::{TcpListener, TcpStream};
use std::sync::{mpsc, Arc, Mutex};
use std::thread;
use std::io::{Read, Write};

struct ThreadPool {
    workers: Vec<thread::JoinHandle<()>>,
    sender: Option<mpsc::Sender<Job>>,
}

type Job = Box<dyn FnOnce() + Send + 'static>;

impl ThreadPool {
    fn new(size: usize) -> Self {
        // TODO:
        // 1. Crear un mpsc::channel para distribuir jobs
        // 2. Envolver el receiver en Arc<Mutex<>> para compartir
        // 3. Crear `size` threads, cada uno en loop:
        //    - lock() el receiver
        //    - recv() un job
        //    - Ejecutar el job
        //    - Si recv retorna Err, terminar (channel cerrado)
        // 4. Guardar los JoinHandles

        todo!()
    }

    fn execute<F: FnOnce() + Send + 'static>(&self, job: F) {
        // TODO: Enviar el job por el channel

        todo!()
    }
}

impl Drop for ThreadPool {
    fn drop(&mut self) {
        // TODO: Cerrar el channel y join todos los workers

        todo!()
    }
}

fn handle_client(mut stream: TcpStream) {
    let addr = stream.peer_addr().unwrap();
    let mut buf = [0u8; 4096];
    loop {
        match stream.read(&mut buf) {
            Ok(0) => { println!("[{}] EOF", addr); return; }
            Ok(n) => { let _ = stream.write_all(&buf[..n]); }
            Err(e) => { println!("[{}] Error: {}", addr, e); return; }
        }
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let pool = ThreadPool::new(4);
    println!("Echo server on :8080 (4 workers)");

    for stream in listener.incoming().take(20) {
        let stream = stream?;
        pool.execute(move || handle_client(stream));
    }

    println!("Accepted 20 connections, shutting down pool...");
    // pool se dropea aquí → graceful shutdown
    Ok(())
}
```

**Pregunta de reflexión**: en esta implementación, cada worker hace `lock()` + `recv()`.
¿Qué pasa si un worker paniquea dentro del job? ¿El lock se envenena? ¿Los demás
workers pueden seguir trabajando? ¿Cómo mitigarías esto?

### Ejercicio 2: Chat room multi-cliente

Implementa un chat donde los mensajes de un cliente se reenvían a todos los demás:

```rust
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Write};
use std::sync::{Arc, Mutex};
use std::thread;
use std::collections::HashMap;

type ClientMap = Arc<Mutex<HashMap<u32, TcpStream>>>;

// TODO:
// 1. Aceptar conexiones y asignar un ID numérico a cada cliente
// 2. Al conectarse, enviar "Welcome! You are user #N"
// 3. Cada mensaje recibido se reenvía a TODOS los demás clientes
//    con formato "[#N] mensaje"
// 4. Al desconectarse, notificar a los demás: "[#N] left the chat"
// 5. Comando especial "/who" lista los usuarios conectados
// 6. Comando "/quit" desconecta limpiamente
//
// Hints:
//   - Almacenar clientes en HashMap<u32, TcpStream>
//   - Para broadcast: clonar streams, soltar el lock, escribir
//   - Limpiar streams muertos del HashMap
//   - Un thread por cliente para lectura

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:4000")?;
    let clients: ClientMap = Arc::new(Mutex::new(HashMap::new()));
    let mut next_id = 0u32;

    println!("Chat server on :4000");

    for stream in listener.incoming() {
        let stream = stream?;
        let id = next_id;
        next_id += 1;

        let clients = clients.clone();

        // TODO: Registrar cliente y spawar thread de manejo

        todo!()
    }
    Ok(())
}
```

**Pregunta de reflexión**: si un cliente es lento para leer (buffer TCP lleno),
`write_all` a su stream se bloqueará. Mientras tanto, estás en el loop de broadcast
con el `Mutex` del `HashMap` retenido. ¿Cómo impacta esto a los demás clientes?
¿Cómo lo resolverías? (Hint: clonar streams y soltar el lock antes de escribir,
o usar channels intermediarios por cliente).

### Ejercicio 3: Rate-limited server

Implementa un servidor que limite las peticiones por cliente por segundo:

```rust
use std::net::{TcpListener, TcpStream, SocketAddr};
use std::io::{BufRead, BufReader, Write};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::time::{Instant, Duration};

struct RateLimiter {
    limits: Mutex<HashMap<SocketAddr, Vec<Instant>>>,
    max_requests: usize,
    window: Duration,
}

impl RateLimiter {
    fn new(max_requests: usize, window: Duration) -> Self {
        // TODO: Crear rate limiter

        todo!()
    }

    fn check(&self, addr: &SocketAddr) -> bool {
        // TODO:
        // 1. Obtener o crear la lista de timestamps para addr
        // 2. Limpiar timestamps fuera de la ventana
        // 3. Si la cantidad de timestamps < max_requests → permitir
        //    y añadir Instant::now()
        // 4. Si >= max_requests → denegar

        todo!()
    }
}

fn main() -> std::io::Result<()> {
    let listener = TcpListener::bind("127.0.0.1:8080")?;
    let limiter = Arc::new(RateLimiter::new(10, Duration::from_secs(1)));
    // Max 10 requests por segundo por IP

    println!("Rate-limited server on :8080 (10 req/s per IP)");

    for stream in listener.incoming() {
        let stream = stream?;
        let limiter = limiter.clone();

        std::thread::spawn(move || {
            // TODO: Implementar el handler que:
            //   - Lee líneas del cliente
            //   - Antes de procesar cada línea, verifica rate limit
            //   - Si el rate limit se excede, responde "RATE_LIMITED\n"
            //     y NO procesa la petición
            //   - Si no, procesa normalmente (echo)
        });
    }
    Ok(())
}
```

**Pregunta de reflexión**: ¿cuánta memoria consume este rate limiter si hay 100,000
IPs diferentes? Cada `Vec<Instant>` puede tener hasta `max_requests` entries.
¿Cómo limpiarías entradas viejas del `HashMap` para IPs que dejaron de enviar
requests hace minutos? (Hint: thread de limpieza periódica o verificación lazy
al acceder).