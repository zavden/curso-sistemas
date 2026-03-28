# Cliente TUI: lectura simultánea de stdin y socket

## Índice

1. [Arquitectura del cliente](#arquitectura-del-cliente)
2. [Conectarse al servidor](#conectarse-al-servidor)
3. [El problema: dos fuentes de input](#el-problema-dos-fuentes-de-input)
4. [tokio::select! para lectura dual](#tokioselect-para-lectura-dual)
5. [Leer stdin de forma async](#leer-stdin-de-forma-async)
6. [El cliente completo](#el-cliente-completo)
7. [Mejorar la experiencia de usuario](#mejorar-la-experiencia-de-usuario)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Arquitectura del cliente

El cliente de chat tiene un reto fundamental: debe hacer **dos cosas a la vez**:

1. Leer lo que el usuario escribe en la terminal (stdin)
2. Recibir mensajes del servidor (socket TCP)

```
┌──────────────────────────────────────────────────────────────┐
│                  Arquitectura del Cliente                     │
│                                                              │
│  ┌─────────────┐                     ┌──────────────┐       │
│  │  Terminal    │ stdin               │  Servidor    │       │
│  │  (usuario    │──────┐              │  TCP         │       │
│  │   escribe)   │      │              │              │       │
│  └──────┬───────┘      │              └──────┬───────┘       │
│         │              │                     │               │
│         │ muestra       ▼                     │ socket        │
│         │         ┌──────────────┐            │               │
│         │         │   Cliente    │            │               │
│         ◀─────────│              │◀───────────┘               │
│                   │  select! {   │                            │
│                   │    stdin     │────────────▶ Enviar al     │
│                   │    socket   │              servidor       │
│                   │  }          │                             │
│                   └──────────────┘                            │
│                                                              │
│  El usuario escribe → se envía al servidor                   │
│  El servidor envía → se muestra en terminal                  │
│  Ambos pueden ocurrir en cualquier momento                   │
└──────────────────────────────────────────────────────────────┘
```

---

## Conectarse al servidor

### Conexión básica

```rust
use tokio::net::TcpStream;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let addr = "127.0.0.1:8080";

    println!("Connecting to {}...", addr);
    let socket = TcpStream::connect(addr).await?;
    println!("Connected!");

    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);

    // Leer el mensaje de bienvenida del servidor
    let mut line = String::new();
    server_reader.read_line(&mut line).await?;
    print!("{}", line);  // "Enter your nickname: "

    // TODO: enviar nickname, luego entrar al loop principal
    Ok(())
}
```

### Argumentos de línea de comandos

```rust
use std::env;

fn parse_args() -> (String, u16) {
    let args: Vec<String> = env::args().collect();

    let host = args.get(1)
        .cloned()
        .unwrap_or_else(|| "127.0.0.1".to_string());

    let port: u16 = args.get(2)
        .and_then(|s| s.parse().ok())
        .unwrap_or(8080);

    (host, port)
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let (host, port) = parse_args();
    let addr = format!("{}:{}", host, port);

    println!("Connecting to {}...", addr);
    let socket = TcpStream::connect(&addr).await?;
    println!("Connected to {}", addr);

    run_client(socket).await
}
```

---

## El problema: dos fuentes de input

### Por qué no funciona un loop secuencial

```rust
// ❌ Secuencial: si esperamos stdin, no vemos mensajes del servidor
loop {
    // Espera a que el usuario escriba algo...
    let line = read_stdin_line().await;  // BLOQUEADO AQUÍ
    // Mientras tanto, los mensajes del servidor no se muestran
    send_to_server(&mut writer, &line).await;

    // Ahora leemos del servidor...
    let msg = read_server_line(&mut server_reader).await;  // BLOQUEADO AQUÍ
    // Mientras tanto, el usuario no puede escribir
    println!("{}", msg);
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  Problema con enfoque secuencial:                            │
│                                                              │
│  Tiempo ─────────────────────────────────────────▶          │
│                                                              │
│  esperando stdin... │ (servidor envía 3 mensajes)            │
│  ████████████████████                                        │
│  usuario escribe    │ envía                                  │
│                     │ ahora lee servidor                     │
│                     │ solo ve 1 de los 3 mensajes            │
│                     │ vuelve a esperar stdin...               │
│                                                              │
│  El usuario pierde mensajes mientras escribe                 │
└──────────────────────────────────────────────────────────────┘
```

### Solución: dos tasks o select!

Hay dos enfoques:

```
┌──────────────────────────────────────────────────────────────┐
│  Enfoque A: Dos tasks separadas                              │
│                                                              │
│  Task 1: loop { read stdin → write socket }                  │
│  Task 2: loop { read socket → print to terminal }            │
│                                                              │
│  + Simple de entender                                        │
│  - Necesita sincronización para terminar ambas               │
│                                                              │
│  Enfoque B: Un loop con select!                              │
│                                                              │
│  loop {                                                      │
│      select! {                                               │
│          line = stdin.read_line() => { send to server }      │
│          msg = socket.read_line() => { print to terminal }   │
│      }                                                       │
│  }                                                           │
│                                                              │
│  + Todo en un solo lugar                                     │
│  + Fácil manejar la terminación                              │
│  - Más complejo si hay más de 2 fuentes                      │
└──────────────────────────────────────────────────────────────┘
```

---

## tokio::select! para lectura dual

### Implementación con select!

```rust
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;

async fn run_client(socket: TcpStream) -> Result<(), Box<dyn std::error::Error>> {
    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);

    let stdin = tokio::io::stdin();
    let mut stdin_reader = BufReader::new(stdin);

    let mut server_line = String::new();
    let mut stdin_line = String::new();

    loop {
        tokio::select! {
            // Llegó un mensaje del servidor
            result = server_reader.read_line(&mut server_line) => {
                match result {
                    Ok(0) => {
                        println!("Server disconnected.");
                        break;
                    }
                    Ok(_) => {
                        print!("{}", server_line);
                        server_line.clear();
                    }
                    Err(e) => {
                        eprintln!("Error reading from server: {}", e);
                        break;
                    }
                }
            }

            // El usuario escribió algo
            result = stdin_reader.read_line(&mut stdin_line) => {
                match result {
                    Ok(0) => {
                        // EOF en stdin (Ctrl+D)
                        println!("Disconnecting...");
                        writer.write_all(b"/quit\n").await?;
                        break;
                    }
                    Ok(_) => {
                        writer.write_all(stdin_line.as_bytes()).await?;
                        stdin_line.clear();
                    }
                    Err(e) => {
                        eprintln!("Error reading stdin: {}", e);
                        break;
                    }
                }
            }
        }
    }

    Ok(())
}
```

### Entender el flujo

```
┌──────────────────────────────────────────────────────────────┐
│  select! en acción:                                          │
│                                                              │
│  Iteración 1:                                                │
│  - Ambos futures registrados para polling                    │
│  - El servidor envía "Welcome!\n"                            │
│  - server_reader.read_line completa primero                  │
│  - Se imprime "Welcome!"                                     │
│  - stdin_reader.read_line se cancela (se re-registra luego)  │
│                                                              │
│  Iteración 2:                                                │
│  - Ambos futures registrados de nuevo                        │
│  - El usuario escribe "Hello\n"                              │
│  - stdin_reader.read_line completa primero                   │
│  - Se envía "Hello\n" al servidor                            │
│  - server_reader.read_line se cancela                        │
│                                                              │
│  Iteración 3:                                                │
│  - Ambos futures registrados                                 │
│  - El servidor envía "[Bob] Hi!\n"                           │
│  - Mientras el usuario no ha escrito nada                    │
│  - server_reader completa → se imprime "[Bob] Hi!"           │
│                                                              │
│  El usuario VE los mensajes en tiempo real mientras escribe  │
└──────────────────────────────────────────────────────────────┘
```

---

## Leer stdin de forma async

### El problema de stdin en async

`tokio::io::stdin()` funciona pero tiene limitaciones: en Linux, stdin
estándar es bloqueante por defecto. Tokio lo maneja internamente usando
un thread de bloqueo (`spawn_blocking`), lo cual funciona bien para la
mayoría de los casos.

### Enfoque alternativo: spawn_blocking para stdin

```rust
use tokio::sync::mpsc;

/// Spawn a blocking thread that reads stdin and sends lines via channel
fn spawn_stdin_reader() -> mpsc::UnboundedReceiver<String> {
    let (tx, rx) = mpsc::unbounded_channel();

    // std::thread porque stdin es bloqueante
    std::thread::spawn(move || {
        let stdin = std::io::stdin();
        let mut line = String::new();

        loop {
            line.clear();
            match stdin.read_line(&mut line) {
                Ok(0) => break,  // EOF
                Ok(_) => {
                    if tx.send(line.clone()).is_err() {
                        break;  // Receiver dropped
                    }
                }
                Err(_) => break,
            }
        }
    });

    rx
}
```

Este enfoque es más explícito: un thread del OS dedicado a stdin, comunicándose
con el mundo async vía un canal.

### Cliente con spawn_blocking para stdin

```rust
async fn run_client(socket: TcpStream) -> Result<(), Box<dyn std::error::Error>> {
    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);
    let mut stdin_rx = spawn_stdin_reader();

    let mut server_line = String::new();

    loop {
        tokio::select! {
            // Mensaje del servidor
            result = server_reader.read_line(&mut server_line) => {
                match result {
                    Ok(0) => {
                        println!("Server disconnected.");
                        break;
                    }
                    Ok(_) => {
                        print!("{}", server_line);
                        server_line.clear();
                    }
                    Err(e) => {
                        eprintln!("Server error: {}", e);
                        break;
                    }
                }
            }

            // Input del usuario (vía canal desde thread bloqueante)
            Some(line) = stdin_rx.recv() => {
                // Detectar /quit localmente para salir limpiamente
                if line.trim() == "/quit" || line.trim() == "/exit" {
                    writer.write_all(line.as_bytes()).await?;
                    println!("Goodbye!");
                    break;
                }
                writer.write_all(line.as_bytes()).await?;
            }
        }
    }

    Ok(())
}
```

---

## El cliente completo

```rust
// src/main.rs (cliente)
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;
use tokio::sync::mpsc;
use std::io::BufRead;

fn spawn_stdin_reader() -> mpsc::UnboundedReceiver<String> {
    let (tx, rx) = mpsc::unbounded_channel();
    std::thread::spawn(move || {
        let stdin = std::io::stdin();
        let reader = stdin.lock();
        for line in reader.lines() {
            match line {
                Ok(line) => {
                    if tx.send(format!("{}\n", line)).is_err() {
                        break;
                    }
                }
                Err(_) => break,
            }
        }
    });
    rx
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Parse args
    let args: Vec<String> = std::env::args().collect();
    let addr = args.get(1)
        .cloned()
        .unwrap_or_else(|| "127.0.0.1:8080".to_string());

    // Connect
    eprintln!("Connecting to {}...", addr);
    let socket = TcpStream::connect(&addr).await?;
    eprintln!("Connected! Type /help for commands, /quit to exit.\n");

    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);
    let mut stdin_rx = spawn_stdin_reader();

    let mut server_line = String::new();

    loop {
        tokio::select! {
            // Server → terminal
            result = server_reader.read_line(&mut server_line) => {
                match result {
                    Ok(0) => {
                        eprintln!("\nServer closed the connection.");
                        break;
                    }
                    Ok(_) => {
                        // Imprimir el mensaje del servidor
                        print!("{}", server_line);
                        server_line.clear();
                    }
                    Err(e) => {
                        eprintln!("\nConnection error: {}", e);
                        break;
                    }
                }
            }

            // stdin → server
            line = stdin_rx.recv() => {
                match line {
                    Some(line) => {
                        let trimmed = line.trim();

                        if trimmed == "/quit" || trimmed == "/exit" {
                            // Enviar al servidor y salir
                            let _ = writer.write_all(line.as_bytes()).await;
                            break;
                        }

                        if let Err(e) = writer.write_all(line.as_bytes()).await {
                            eprintln!("\nFailed to send: {}", e);
                            break;
                        }
                    }
                    None => {
                        // stdin cerrado (Ctrl+D)
                        eprintln!("\nInput closed, disconnecting...");
                        let _ = writer.write_all(b"/quit\n").await;
                        break;
                    }
                }
            }
        }
    }

    eprintln!("Disconnected.");
    Ok(())
}
```

### Probar el cliente

```bash
# Terminal 1: servidor
cd chat-server && cargo run

# Terminal 2: cliente A
cd chat-client && cargo run -- 127.0.0.1:8080

# Terminal 3: cliente B
cd chat-client && cargo run -- 127.0.0.1:8080

# O usar nc como cliente alternativo
nc 127.0.0.1 8080
```

### Sesión de ejemplo

```
$ cargo run -- 127.0.0.1:8080
Connecting to 127.0.0.1:8080...
Connected! Type /help for commands, /quit to exit.

Enter your nickname: Alice
*** Welcome, Alice! Type /help for commands. ***
Hello everyone!
*** Bob has joined the chat ***
[Bob] Hi Alice!
/msg Bob Hey, how are you?
*** Message sent to Bob ***
[PM from Bob] I'm great, thanks!
/quit
Disconnected.
```

---

## Mejorar la experiencia de usuario

### Prompt visual

Un problema del cliente básico: cuando llega un mensaje del servidor
mientras el usuario está escribiendo, el texto se mezcla.

```
┌──────────────────────────────────────────────────────────────┐
│  Sin prompt management:                                      │
│                                                              │
│  [Bob] Hello!                                                │
│  I'm typing a mess[Carol] Hey everyone!                      │
│  age here                                                    │
│  ← El mensaje de Carol interrumpió la línea que escribíamos  │
│                                                              │
│  Esto es inherente a la terminal raw. Solucionarlo           │
│  requiere una TUI library (tema de T02).                     │
│  Por ahora, es aceptable para un chat básico.                │
└──────────────────────────────────────────────────────────────┘
```

### Usar eprintln para mensajes del sistema

Separar canales: mensajes del chat a stdout, mensajes del cliente a stderr:

```rust
// Mensajes del cliente (no del chat) van a stderr
eprintln!("Connecting to {}...", addr);
eprintln!("Connected!");
eprintln!("Disconnected.");

// Mensajes del servidor van a stdout
print!("{}", server_line);
```

Esto permite redirigir stdout a un archivo para logging:
```bash
cargo run -- 127.0.0.1:8080 > chat_log.txt
# Los mensajes del chat se guardan en chat_log.txt
# Los mensajes del cliente se ven en la terminal
```

### Timeout de conexión

```rust
use tokio::time::{timeout, Duration};

async fn connect_with_timeout(
    addr: &str,
) -> Result<TcpStream, Box<dyn std::error::Error>> {
    match timeout(Duration::from_secs(5), TcpStream::connect(addr)).await {
        Ok(Ok(socket)) => Ok(socket),
        Ok(Err(e)) => {
            eprintln!("Connection failed: {}", e);
            Err(e.into())
        }
        Err(_) => {
            eprintln!("Connection timed out after 5 seconds");
            Err("Connection timeout".into())
        }
    }
}
```

### Reconexión automática

```rust
async fn run_with_reconnect(addr: &str) -> Result<(), Box<dyn std::error::Error>> {
    let max_retries = 5;
    let mut retries = 0;

    loop {
        match connect_with_timeout(addr).await {
            Ok(socket) => {
                retries = 0;  // Reset on successful connection
                if let Err(e) = run_client(socket).await {
                    eprintln!("Session ended: {}", e);
                }
            }
            Err(_) => {
                retries += 1;
                if retries >= max_retries {
                    eprintln!("Max retries reached, giving up.");
                    return Err("Could not connect".into());
                }
            }
        }

        eprintln!("Reconnecting in 3 seconds... (attempt {}/{})", retries + 1, max_retries);
        tokio::time::sleep(Duration::from_secs(3)).await;
    }
}
```

### Señales: Ctrl+C limpio

```rust
use tokio::signal;

async fn run_client(socket: TcpStream) -> Result<(), Box<dyn std::error::Error>> {
    let (reader, mut writer) = socket.into_split();
    let mut server_reader = BufReader::new(reader);
    let mut stdin_rx = spawn_stdin_reader();
    let mut server_line = String::new();

    loop {
        tokio::select! {
            result = server_reader.read_line(&mut server_line) => {
                // ... igual que antes
            }

            line = stdin_rx.recv() => {
                // ... igual que antes
            }

            // Ctrl+C: salir limpiamente
            _ = signal::ctrl_c() => {
                eprintln!("\nCaught Ctrl+C, disconnecting...");
                let _ = writer.write_all(b"/quit\n").await;
                break;
            }
        }
    }

    Ok(())
}
```

---

## Errores comunes

### 1. Olvidar el `\n` al enviar al servidor

```rust
// ❌ El servidor usa read_line: espera \n para completar la línea
writer.write_all(b"Hello").await?;
// El servidor nunca recibe este mensaje (espera el \n)

// ✅ Incluir el newline
writer.write_all(b"Hello\n").await?;

// O formatear con format!
writer.write_all(format!("{}\n", user_input).as_bytes()).await?;
```

### 2. Usar println! en vez de print! para mensajes del servidor

```rust
// ❌ println! agrega un \n extra (el servidor ya envía \n)
println!("{}", server_line);
// Output: "[Bob] Hello!\n\n" → línea vacía de más

// ✅ print! sin newline extra (la línea del servidor ya tiene \n)
print!("{}", server_line);
```

### 3. No manejar la desconexión del servidor

```rust
// ❌ Ignorar bytes_read == 0: el cliente queda en loop vacío
loop {
    server_reader.read_line(&mut line).await?;
    print!("{}", line);
    line.clear();
    // Si el servidor cierra, read_line retorna Ok(0) infinitamente
}

// ✅ Detectar EOF y salir
let bytes = server_reader.read_line(&mut line).await?;
if bytes == 0 {
    eprintln!("Server disconnected.");
    break;
}
```

### 4. No hacer flush de stdout

```rust
// ❌ print! puede no mostrarse inmediatamente (stdout con buffer)
print!("{}", server_line);
// El mensaje puede quedar en el buffer hasta el próximo \n

// ✅ Flush explícito si es necesario
use std::io::Write;
print!("{}", server_line);
std::io::stdout().flush().unwrap();

// En la práctica, print! con \n al final hace flush automático
// en la mayoría de los terminales. Solo es problema si imprimes
// texto sin \n final (como un prompt).
```

### 5. Thread de stdin que no termina al desconectar

```rust
// ❌ El thread de stdin sigue vivo después de desconectar
// porque read_line() es bloqueante y espera input

// La solución es aceptar que el thread termina cuando:
// 1. El usuario presiona Enter (read_line retorna)
//    y tx.send() falla porque rx fue dropeado
// 2. El proceso termina (el thread muere con él)

// Para un chat simple, esto es aceptable.
// Para una TUI real, se usa crossterm o termion con
// input no-bloqueante (T02).
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            CLIENTE TUI CHEATSHEET                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CONEXIÓN                                                    │
│  TcpStream::connect(addr).await?     Conectar                │
│  socket.into_split()                 Separar read/write      │
│  BufReader::new(read_half)           Buffer de lectura        │
│  timeout(dur, connect).await         Con timeout             │
│                                                              │
│  LECTURA DUAL CON SELECT                                     │
│  tokio::select! {                                            │
│      result = server.read_line()     Mensaje del servidor    │
│        => { print!("{}", line); }                            │
│      Some(line) = stdin_rx.recv()    Input del usuario       │
│        => { writer.write_all(); }                            │
│      _ = signal::ctrl_c()           Ctrl+C                   │
│        => { send /quit; break; }                             │
│  }                                                           │
│                                                              │
│  STDIN ASYNC                                                 │
│  Opción A: tokio::io::stdin()        Tokio maneja blocking  │
│  Opción B: std::thread + mpsc        Thread dedicado         │
│                                                              │
│  SPAWN STDIN READER                                          │
│  let (tx, rx) = mpsc::unbounded_channel();                   │
│  std::thread::spawn(move || {                                │
│      for line in stdin.lock().lines() {                      │
│          tx.send(line).ok();                                 │
│      }                                                       │
│  });                                                         │
│  // rx es UnboundedReceiver<String>                          │
│                                                              │
│  SEÑALES DE TERMINACIÓN                                      │
│  bytes_read == 0          Servidor desconectó (EOF)          │
│  stdin_rx.recv() → None   stdin cerrado (Ctrl+D)             │
│  signal::ctrl_c()         Usuario presionó Ctrl+C            │
│  /quit                    Comando de salida                   │
│                                                              │
│  SALIDA LIMPIA                                               │
│  1. Enviar /quit al servidor                                 │
│  2. Break del loop de select!                                │
│  3. writer se dropea → cierra la conexión TCP                │
│  4. El thread de stdin muere con el proceso                  │
│                                                              │
│  OUTPUT                                                      │
│  print!() para mensajes del servidor (ya tienen \n)          │
│  eprintln!() para mensajes del cliente (status, errores)     │
│  stdout.flush() si no hay \n al final                        │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Cliente básico con select!

Implementa el cliente de chat mínimo:

```bash
cargo new chat-client
cd chat-client
```

**Tareas:**
1. Conéctate al servidor del proyecto anterior
2. Implementa el loop con `tokio::select!` para leer stdin y socket
3. Maneja los 3 casos de terminación:
   - Servidor cierra la conexión (bytes_read == 0)
   - Usuario escribe `/quit`
   - Usuario presiona Ctrl+D (EOF en stdin)
4. Prueba con el servidor de S01:
   - Abre dos clientes
   - Envía mensajes entre ellos
   - Desconecta uno y verifica que el otro ve "has left"
5. Prueba que `Ctrl+D` envía `/quit` al servidor antes de desconectar

**Pregunta de reflexión:** ¿Por qué usamos `socket.into_split()` en vez de
compartir `&mut TcpStream` entre lectura y escritura? ¿Qué garantiza el
borrow checker al respecto?

---

### Ejercicio 2: Timeout y reconexión

Extiende el cliente con robustez:

**Tareas:**
1. Agrega timeout de conexión (5 segundos):
   ```bash
   # Conectar a un puerto que no responde
   cargo run -- 192.0.2.1:8080
   # Debe mostrar "Connection timed out" después de 5s
   ```
2. Implementa reconexión automática (máximo 3 intentos, espera 3s entre cada uno)
3. Agrega manejo de `Ctrl+C` con `tokio::signal::ctrl_c()`
4. Si el servidor envía una línea vacía, no imprimir (filter)
5. Prueba el flujo completo:
   - Inicia el cliente sin servidor → timeout → retry
   - Inicia el servidor → el cliente se conecta
   - Mata el servidor → el cliente detecta la desconexión
   - Reinicia el servidor → el cliente se reconecta

**Pregunta de reflexión:** La reconexión automática pierde el estado de la
sesión (nickname, sala, etc.). ¿Cómo diseñarías un protocolo que soporte
"session resumption" para que el cliente pueda reconectarse y recuperar
su estado anterior?

---

### Ejercicio 3: Cliente con dos tasks

Reimplementa el cliente usando `tokio::spawn` en vez de `select!`:

```rust
// Task 1: lee del servidor y muestra en terminal
let reader_task = tokio::spawn(async move {
    loop {
        let mut line = String::new();
        match server_reader.read_line(&mut line).await {
            Ok(0) => break,
            Ok(_) => print!("{}", line),
            Err(_) => break,
        }
    }
});

// Task 2: lee de stdin y envía al servidor
let writer_task = tokio::spawn(async move {
    // ...
});

// Esperar a que cualquiera termine
tokio::select! {
    _ = reader_task => println!("Server disconnected"),
    _ = writer_task => println!("Input closed"),
}
```

**Tareas:**
1. Implementa ambas tasks
2. Cuando una task termina, la otra debe terminar también.
   Usa un `tokio::sync::Notify` o `CancellationToken` para señalizar
3. Compara con la versión `select!`:
   - ¿Cuál es más fácil de leer?
   - ¿Cuál maneja mejor la terminación?
   - ¿En cuál es más fácil agregar funcionalidad?
4. Agrega un tercer task: un "heartbeat" que envía `/ping` al servidor
   cada 30 segundos para detectar conexiones muertas

**Pregunta de reflexión:** Con el enfoque de dos tasks, ¿cómo manejas el
caso de que `reader_task` termine (servidor desconectó) pero `writer_task`
esté bloqueada esperando stdin? El thread de stdin no se puede cancelar
desde async. ¿Qué opciones tienes?