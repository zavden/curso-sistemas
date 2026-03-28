# Bloque 7: Programación de Sistemas en Rust

**Objetivo**: Aplicar las abstracciones de Rust a la programación de sistemas, incluyendo
concurrencia segura, async y FFI.

## Capítulo 1: Concurrencia con Hilos [A]

### Sección 1: Threads
- **T01 - std::thread::spawn**: closures, JoinHandle, move semántica en closures
- **T02 - Datos compartidos**: Arc<T>, Mutex<T>, RwLock<T>, por qué Arc + Mutex es idiomático
- **T03 - Channels**: mpsc::channel, mpsc::sync_channel, comunicación por mensajes
- **T04 - Poisoning**: qué pasa cuando un thread paniquea con un Mutex locked

### Sección 2: Concurrencia sin Data Races
- **T01 - Send y Sync**: qué significan, qué tipos los implementan, por qué Rc no es Send
- **T02 - Atomics**: AtomicBool, AtomicUsize, Ordering (Relaxed, Acquire, Release, SeqCst)
- **T03 - Scoped threads**: std::thread::scope (Rust 1.63+), por qué resuelven lifetimes
- **T04 - Rayon**: paralelismo de datos, par_iter, join — introducción práctica

## Capítulo 2: Async/Await [A]

### Sección 1: Fundamentos Async
- **T01 - Futures y el trait Future**: Poll, Pending, Ready, lazy evaluation
- **T02 - async fn y .await**: transformación del compilador, máquina de estados
- **T03 - Runtime (Tokio)**: tokio::main, spawn, select!, JoinHandle
- **T04 - Pin y Unpin**: por qué existen, self-referential structs, cuándo preocuparse

### Sección 2: I/O Asíncrono
- **T01 - tokio::fs y tokio::net**: TcpListener, TcpStream, operaciones de archivo
- **T02 - Channels async**: tokio::sync::mpsc, broadcast, oneshot, watch
- **T03 - Timeouts y cancelación**: tokio::time::timeout, select!, drop para cancelar
- **T04 - Async vs threads**: cuándo usar cada uno, análisis de tradeoffs

## Capítulo 3: Sistema de Archivos [A]

### Sección 1: Operaciones de FS
- **T01 - std::fs**: read, write, create_dir_all, remove_file, rename, metadata
- **T02 - std::path**: Path, PathBuf, join, extension, canonicalize, diferencias entre OS
- **T03 - Recorrido de directorios**: read_dir, walkdir crate, filtros, symlinks
- **T04 - Permisos y metadata**: permissions, set_permissions, modificación de timestamps

## Capítulo 4: Networking en Rust [A]

### Sección 1: std::net
- **T01 - TcpListener y TcpStream**: bind, accept, connect, read/write
- **T02 - UdpSocket**: send_to, recv_from, broadcast
- **T03 - Servidor multi-cliente**: un thread por conexión, thread pool

### Sección 2: Networking Async
- **T01 - Servidor TCP con Tokio**: tokio::net::TcpListener, spawn por conexión
- **T02 - Protocolo custom**: framing, serialización con serde, length-prefixed messages
- **T03 - HTTP client**: reqwest crate, GET/POST, JSON, manejo de errores de red

## Capítulo 5: FFI (Foreign Function Interface) [M]

### Sección 1: Llamar C desde Rust
- **T01 - extern "C" y #[link]**: declarar funciones C, linking
- **T02 - Tipos FFI**: std::ffi (CStr, CString, c_int, c_char...), conversiones seguras
- **T03 - bindgen**: generar bindings automáticamente desde headers C
- **T04 - Wrappers seguros**: envolver APIs C inseguras en interfaces Rust idiomáticas

### Sección 2: Llamar Rust desde C
- **T01 - #[no_mangle] y extern "C"**: exportar funciones Rust como ABI C
- **T02 - cbindgen**: generar headers .h desde código Rust
- **T03 - Tipos opacos**: Box<T> como puntero opaco, free functions, ownership a través del boundary

## Capítulo 6: unsafe Rust [A]

### Sección 1: Los 5 Superpoderes
- **T01 - Raw pointers**: *const T, *mut T, creación segura pero dereference unsafe
- **T02 - unsafe fn y bloques unsafe**: qué promete el programador, safety invariants
- **T03 - Traits unsafe**: implementar Send/Sync manualmente, cuándo es necesario
- **T04 - Union access y static mut**: acceso a campos de union, variables estáticas mutables
- **T05 - extern functions**: llamar código externo siempre es unsafe

### Sección 2: Buen Uso de unsafe
- **T01 - Minimizar scope**: bloques unsafe lo más pequeños posible
- **T02 - Documentar invariantes**: safety comments, por qué es correcto
- **T03 - Miri**: herramienta de detección de UB en Rust, cargo miri test

## Capítulo 7: Debugging de Programas de Sistema [M]

### Sección 1: Debugging en C (Avanzado)
- **T01 - GDB avanzado**: conditional breakpoints, watchpoints, remote debugging, core dumps
- **T02 - strace avanzado**: filtrar por syscall, timing (-T), conteo (-c), follow forks (-f)
- **T03 - Core dumps**: ulimit -c, dónde se guardan, analizar con GDB, coredumpctl (systemd)
- **T04 - ltrace**: trazar llamadas a bibliotecas dinámicas, comparación con strace

### Sección 2: Debugging en Rust
- **T01 - RUST_BACKTRACE**: niveles (0, 1, full), interpretar backtraces
- **T02 - cargo-expand**: ver código generado por macros, entender derive
- **T03 - Miri**: detección de UB, cargo miri test, limitaciones
- **T04 - Debugging concurrente**: ThreadSanitizer (-Zsanitizer=thread), detectar data races

### Sección 3: Metodología
- **T01 - Investigar un crash**: flujo sistemático (reproducir → aislar → diagnosticar → corregir)
- **T02 - Bisección**: git bisect para localizar regresiones, automatización con scripts
- **T03 - Profiling básico**: perf stat, perf record, flame graphs (inferno crate), bottlenecks de CPU vs I/O

## Capítulo 8: Proyecto — Utilidad CLI Tipo grep [P]

### Sección 1: Implementación
- **T01 - Argument parsing**: clap crate, subcomandos, flags
- **T02 - Búsqueda en archivos**: regex crate, matching multi-archivo, recursivo
- **T03 - Salida coloreada**: colored crate, detección de TTY
- **T04 - Rendimiento**: buffered I/O, memmap para archivos grandes, paralelismo con rayon

## Capítulo 9: Proyecto — Chat TCP Multi-cliente [P]

### Sección 1: Servidor
- **T01 - Servidor con Tokio**: TcpListener, spawn por conexión, broadcast de mensajes
- **T02 - Protocolo de chat**: framing de mensajes, nicknames, comandos (/nick, /quit, /list)
- **T03 - Estado compartido**: Arc<Mutex<HashMap>> de clientes conectados, broadcast channel

### Sección 2: Cliente
- **T01 - Cliente TUI**: lectura de stdin + socket simultánea, tokio::select!
- **T02 - Mejora 1**: historial de mensajes con scroll
- **T03 - Mejora 2**: salas/canales (/join #sala, /msg usuario)
- **T04 - Mejora 3**: persistir historial al disco (append to file)
