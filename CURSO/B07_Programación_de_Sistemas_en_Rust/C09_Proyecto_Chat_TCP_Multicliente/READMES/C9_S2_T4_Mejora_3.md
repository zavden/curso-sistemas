# Mejora 3: persistir historial al disco

## Índice

1. [Por qué persistir el historial](#por-qué-persistir-el-historial)
2. [Estrategia de append-only](#estrategia-de-append-only)
3. [Escribir el log de chat](#escribir-el-log-de-chat)
4. [Formato del archivo de log](#formato-del-archivo-de-log)
5. [Rotación de archivos](#rotación-de-archivos)
6. [Cargar historial al iniciar](#cargar-historial-al-iniciar)
7. [Persistencia en el servidor](#persistencia-en-el-servidor)
8. [Integración con el cliente TUI](#integración-con-el-cliente-tui)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Por qué persistir el historial

El historial en memoria de T02 se pierde al cerrar el cliente. Y en el
servidor, un usuario que se conecta tarde no ve los mensajes anteriores.
Persistir el historial al disco resuelve ambos problemas:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin persistencia (estado actual):                           │
│                                                              │
│  Alice se conecta a las 10:00                               │
│  Bob envía "Hola" a las 10:05                               │
│  Alice cierra el cliente a las 10:10                         │
│  Alice se reconecta a las 10:15                              │
│  → Alice NO ve el "Hola" de Bob                             │
│  → Todo el historial desapareció                            │
│                                                              │
│  Con persistencia:                                           │
│                                                              │
│  Lado cliente:                                               │
│    → chat_log.txt guarda todo lo recibido                   │
│    → Al reiniciar, puede leer el archivo                     │
│                                                              │
│  Lado servidor:                                              │
│    → server_log.txt guarda todos los mensajes               │
│    → Al reconectarse, el servidor envía las últimas N líneas│
│    → "Replay" del historial reciente                        │
└──────────────────────────────────────────────────────────────┘
```

---

## Estrategia de append-only

### Por qué append y no rewrite

Para un log de chat, append-only es la estrategia natural:

```
┌──────────────────────────────────────────────────────────────┐
│  Rewrite (leer todo → modificar → escribir todo):            │
│  - Lento: O(n) por cada mensaje nuevo                       │
│  - Riesgoso: si el proceso muere durante la escritura,      │
│    el archivo completo puede corromperse                     │
│  - Innecesario: nunca modificamos mensajes anteriores       │
│                                                              │
│  Append-only (agregar al final):                             │
│  - Rápido: O(1) por mensaje                                 │
│  - Seguro: si el proceso muere, se pierde como máximo       │
│    el último mensaje, no todo el archivo                     │
│  - Natural: un chat es una secuencia cronológica             │
│  - Atomicidad: una línea append en Linux es atómico          │
│    para líneas ≤ PIPE_BUF (4096 bytes)                      │
└──────────────────────────────────────────────────────────────┘
```

### OpenOptions para append

```rust
use std::fs::OpenOptions;
use std::io::Write;

fn open_log_file(path: &str) -> std::io::Result<std::fs::File> {
    OpenOptions::new()
        .create(true)    // Crear si no existe
        .append(true)    // Escribir al final (no sobrescribir)
        .open(path)
}

// Uso básico
let mut log = open_log_file("chat.log")?;
writeln!(log, "[2026-03-20 10:05:32] [Alice] Hello!")?;
```

### append(true) vs write(true) + seek

```rust
// ❌ write(true) sin seek: sobrescribe desde el inicio
let mut file = OpenOptions::new()
    .create(true)
    .write(true)
    .open("chat.log")?;
writeln!(file, "New message")?;
// Sobrescribe los primeros bytes del archivo

// ❌ write(true) + seek al final: race condition si dos procesos escriben
let mut file = OpenOptions::new()
    .create(true)
    .write(true)
    .open("chat.log")?;
file.seek(std::io::SeekFrom::End(0))?;
// Otro proceso puede escribir aquí entre seek y write
writeln!(file, "New message")?;

// ✅ append(true): atómico a nivel de kernel
let mut file = OpenOptions::new()
    .create(true)
    .append(true)
    .open("chat.log")?;
writeln!(file, "New message")?;
// El kernel garantiza que cada write va al final
```

---

## Escribir el log de chat

### ChatLogger síncrono

```rust
use std::fs::{File, OpenOptions};
use std::io::{self, BufWriter, Write};
use std::path::Path;

pub struct ChatLogger {
    writer: BufWriter<File>,
}

impl ChatLogger {
    pub fn new(path: impl AsRef<Path>) -> io::Result<Self> {
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(path)?;

        Ok(Self {
            writer: BufWriter::new(file),
        })
    }

    pub fn log(&mut self, message: &str) -> io::Result<()> {
        let timestamp = Self::timestamp();
        writeln!(self.writer, "[{}] {}", timestamp, message)?;
        self.writer.flush()?;
        Ok(())
    }

    fn timestamp() -> String {
        // Sin dependencia externa: usar SystemTime
        use std::time::SystemTime;
        let now = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_default();
        let secs = now.as_secs();

        // Formato simple: epoch seconds
        // Para formato legible, ver la sección de chrono más abajo
        format!("{}", secs)
    }
}
```

### Timestamps legibles con chrono

Para timestamps en formato humano, el crate `chrono` es el estándar:

```toml
[dependencies]
chrono = "0.4"
```

```rust
use chrono::Local;

fn timestamp() -> String {
    Local::now().format("%Y-%m-%d %H:%M:%S").to_string()
}

// Resultado: "2026-03-20 10:05:32"
```

### Timestamps sin dependencia externa

Si preferimos no agregar chrono, podemos calcular la fecha manualmente a
partir del epoch, pero es tedioso. Una alternativa práctica: delegar al OS:

```rust
fn timestamp_from_os() -> String {
    // Usa el comando date del sistema (solo Unix)
    // Aceptable para un log, no para código de producción
    std::process::Command::new("date")
        .arg("+%Y-%m-%d %H:%M:%S")
        .output()
        .ok()
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "???".to_string())
}
```

Para nuestro proyecto de chat, usamos el epoch en segundos por simplicidad.
Si el usuario quiere formato legible, agrega chrono.

### BufWriter y flush

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Por qué BufWriter + flush explícito?                       │
│                                                              │
│  Sin BufWriter:                                              │
│    writeln!(file, msg)  → syscall write() cada vez          │
│    100 mensajes/segundo → 100 syscalls/segundo              │
│                                                              │
│  Con BufWriter (8KB buffer por defecto):                     │
│    writeln!(writer, msg) → copia al buffer en userspace     │
│    El buffer se escribe cuando:                              │
│    - Se llena (8KB)                                         │
│    - Se llama .flush()                                      │
│    - El BufWriter se dropea                                  │
│                                                              │
│  Para un chat, hacemos flush después de cada mensaje:        │
│    - Queremos que el log esté actualizado                   │
│    - Si el proceso muere, no queremos perder mensajes       │
│    - El "buffer" de BufWriter sigue siendo útil porque      │
│      agrupa el writeln! (que puede ser dos writes internos) │
│      en un solo syscall                                      │
│                                                              │
│  Para un servidor de alto rendimiento:                       │
│    - Flush cada N mensajes o cada N segundos                │
│    - Aceptar perder los últimos N mensajes en un crash      │
└──────────────────────────────────────────────────────────────┘
```

---

## Formato del archivo de log

### Formato simple: texto plano

```
[1710936332] [Alice] Hello everyone!
[1710936345] *** Bob has joined the chat ***
[1710936350] [Bob] Hi Alice!
[1710936360] [#rust][Alice] Anyone doing async?
[1710936400] *** Alice has left the chat ***
```

Ventajas: legible, `grep`-able, compatible con herramientas Unix.

### Formato estructurado: JSON lines

Cada línea es un objeto JSON independiente (JSONL / NDJSON):

```json
{"ts":1710936332,"type":"msg","room":"#lobby","sender":"Alice","content":"Hello everyone!"}
{"ts":1710936345,"type":"join","nick":"Bob"}
{"ts":1710936350,"type":"msg","room":"#lobby","sender":"Bob","content":"Hi Alice!"}
{"ts":1710936360,"type":"msg","room":"#rust","sender":"Alice","content":"Anyone doing async?"}
{"ts":1710936400,"type":"leave","nick":"Alice"}
```

```rust
use serde::Serialize;

#[derive(Serialize)]
struct LogEntry {
    ts: u64,
    #[serde(rename = "type")]
    kind: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    room: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sender: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    nick: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    content: Option<String>,
}
```

```toml
[dependencies]
serde = { version = "1", features = ["derive"] }
serde_json = "1"
```

### Comparación de formatos

```
┌──────────────────────────────────────────────────────────────┐
│  Texto plano:                                                │
│  + Legible con cat, less, tail -f                           │
│  + Grep directo: grep "\[Alice\]" chat.log                  │
│  + Sin dependencias extra                                    │
│  - Difícil de parsear programáticamente                     │
│  - Ambigüedades si el mensaje contiene [ o ]                │
│                                                              │
│  JSONL:                                                      │
│  + Parseo exacto con serde_json                              │
│  + Campos tipados, sin ambigüedad                           │
│  + Fácil de procesar con jq: cat log | jq 'select(.room)'  │
│  - Menos legible a simple vista                              │
│  - Requiere serde + serde_json                               │
│                                                              │
│  Recomendación para este proyecto:                           │
│  - Cliente: texto plano (el usuario lo lee directamente)    │
│  - Servidor: JSONL (para procesamiento posterior)            │
└──────────────────────────────────────────────────────────────┘
```

Para el resto de este tema usaremos texto plano en el cliente y mostraremos
JSONL como alternativa en el servidor.

---

## Rotación de archivos

Si el chat corre durante días, el archivo de log crece indefinidamente. La
rotación crea archivos nuevos periódicamente:

### Rotación por tamaño

```rust
use std::fs;
use std::path::{Path, PathBuf};

pub struct RotatingLogger {
    base_path: PathBuf,
    writer: BufWriter<File>,
    current_size: u64,
    max_size: u64,       // Bytes antes de rotar
    max_files: usize,    // Cuántos archivos históricos mantener
}

impl RotatingLogger {
    pub fn new(
        base_path: impl Into<PathBuf>,
        max_size: u64,
        max_files: usize,
    ) -> io::Result<Self> {
        let base_path = base_path.into();
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&base_path)?;
        let current_size = file.metadata()?.len();

        Ok(Self {
            writer: BufWriter::new(file),
            base_path,
            current_size,
            max_size,
            max_files,
        })
    }

    pub fn log(&mut self, message: &str) -> io::Result<()> {
        let line = format!("[{}] {}\n", epoch_secs(), message);
        let line_bytes = line.len() as u64;

        // ¿Necesitamos rotar?
        if self.current_size + line_bytes > self.max_size {
            self.rotate()?;
        }

        self.writer.write_all(line.as_bytes())?;
        self.writer.flush()?;
        self.current_size += line_bytes;

        Ok(())
    }

    fn rotate(&mut self) -> io::Result<()> {
        // Flush y cerrar el archivo actual
        self.writer.flush()?;

        // Rotar archivos: chat.log.3 → chat.log.4, .2 → .3, etc.
        for i in (1..self.max_files).rev() {
            let from = rotated_path(&self.base_path, i);
            let to = rotated_path(&self.base_path, i + 1);
            if from.exists() {
                fs::rename(&from, &to)?;
            }
        }

        // chat.log → chat.log.1
        let first_rotated = rotated_path(&self.base_path, 1);
        fs::rename(&self.base_path, &first_rotated)?;

        // Eliminar el más viejo si excede max_files
        let oldest = rotated_path(&self.base_path, self.max_files);
        if oldest.exists() {
            fs::remove_file(&oldest)?;
        }

        // Crear nuevo archivo
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&self.base_path)?;
        self.writer = BufWriter::new(file);
        self.current_size = 0;

        Ok(())
    }
}

fn rotated_path(base: &Path, n: usize) -> PathBuf {
    let name = format!(
        "{}.{}",
        base.file_name().unwrap().to_string_lossy(),
        n
    );
    base.with_file_name(name)
}

fn epoch_secs() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}
```

### Rotación por fecha

```rust
use std::path::PathBuf;

pub struct DailyLogger {
    base_dir: PathBuf,
    prefix: String,
    writer: Option<BufWriter<File>>,
    current_date: String,
}

impl DailyLogger {
    pub fn new(base_dir: impl Into<PathBuf>, prefix: &str) -> io::Result<Self> {
        let base_dir = base_dir.into();
        fs::create_dir_all(&base_dir)?;

        Ok(Self {
            base_dir,
            prefix: prefix.to_string(),
            writer: None,
            current_date: String::new(),
        })
    }

    pub fn log(&mut self, message: &str) -> io::Result<()> {
        let today = today_string();

        // ¿Cambió el día? Abrir nuevo archivo
        if today != self.current_date {
            self.open_new_file(&today)?;
        }

        if let Some(ref mut writer) = self.writer {
            writeln!(writer, "[{}] {}", epoch_secs(), message)?;
            writer.flush()?;
        }

        Ok(())
    }

    fn open_new_file(&mut self, date: &str) -> io::Result<()> {
        // Flush el archivo anterior
        if let Some(ref mut w) = self.writer {
            w.flush()?;
        }

        let filename = format!("{}_{}.log", self.prefix, date);
        let path = self.base_dir.join(&filename);

        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path)?;

        self.writer = Some(BufWriter::new(file));
        self.current_date = date.to_string();

        Ok(())
    }
}

fn today_string() -> String {
    // Días desde epoch (simple, sin chrono)
    let secs = epoch_secs();
    let days = secs / 86400;
    // Fórmula simplificada — para producción, usar chrono
    format!("day-{}", days)

    // Con chrono:
    // chrono::Local::now().format("%Y-%m-%d").to_string()
}
```

### Resultado en disco

```
logs/
├── chat_2026-03-18.log
├── chat_2026-03-19.log
└── chat_2026-03-20.log     ← archivo actual

# O con rotación por tamaño:
chat.log       ← actual (escribiendo aquí)
chat.log.1     ← anterior
chat.log.2     ← más antiguo
chat.log.3     ← el más viejo (se eliminará cuando se cree .4)
```

---

## Cargar historial al iniciar

### Leer las últimas N líneas

Al iniciar el cliente, cargar las últimas N líneas del log para mostrar
contexto previo:

```rust
use std::io::{BufRead, BufReader};

/// Read the last N lines from a file efficiently
fn tail_lines(path: &str, n: usize) -> io::Result<Vec<String>> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);

    // Leer todas las líneas y quedarnos con las últimas N
    // Para archivos pequeños/medianos, esto es suficiente
    let lines: Vec<String> = reader.lines()
        .filter_map(|l| l.ok())
        .collect();

    let start = lines.len().saturating_sub(n);
    Ok(lines[start..].to_vec())
}
```

### Tail eficiente para archivos grandes

Para archivos de muchos MB, leer todo para quedarse con las últimas N líneas
es ineficiente. Leer desde el final es mejor:

```rust
use std::io::{Read, Seek, SeekFrom};

/// Read the last N lines by seeking from the end
fn tail_lines_efficient(path: &str, n: usize) -> io::Result<Vec<String>> {
    let mut file = File::open(path)?;
    let file_size = file.metadata()?.len();

    if file_size == 0 {
        return Ok(vec![]);
    }

    // Empezar leyendo los últimos 8KB (suficiente para ~100 líneas típicas)
    let mut chunk_size = 8192u64.min(file_size);
    let mut result = Vec::new();

    loop {
        let offset = file_size.saturating_sub(chunk_size);
        file.seek(SeekFrom::Start(offset))?;

        let mut buf = String::new();
        file.read_to_string(&mut buf)?;

        // Contar líneas
        result = buf.lines().map(|l| l.to_string()).collect::<Vec<_>>();

        if result.len() > n || offset == 0 {
            // Tenemos suficientes líneas o leímos todo el archivo
            break;
        }

        // Duplicar el chunk y reintentar
        chunk_size = (chunk_size * 2).min(file_size);
    }

    // Retornar solo las últimas N
    let start = result.len().saturating_sub(n);
    Ok(result[start..].to_vec())
}
```

### Integración con MessageHistory

```rust
fn load_history_from_disk(
    log_path: &str,
    history: &mut MessageHistory,
    max_lines: usize,
) {
    match tail_lines(log_path, max_lines) {
        Ok(lines) => {
            for line in lines {
                // Remover el timestamp del log para mostrar solo el mensaje
                let message = strip_log_timestamp(&line);
                history.push(message);
            }
        }
        Err(e) => {
            history.push(format!("*** Could not load history: {} ***", e));
        }
    }
}

/// Strip the "[timestamp] " prefix from a log line
fn strip_log_timestamp(line: &str) -> String {
    // Formato: "[1710936332] [Alice] Hello!"
    // Queremos: "[Alice] Hello!"
    if let Some(rest) = line.strip_prefix('[') {
        if let Some(pos) = rest.find("] ") {
            return rest[pos + 2..].to_string();
        }
    }
    line.to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_strip_timestamp() {
        assert_eq!(
            strip_log_timestamp("[1710936332] [Alice] Hello!"),
            "[Alice] Hello!"
        );
        assert_eq!(
            strip_log_timestamp("[1710936345] *** Bob has joined ***"),
            "*** Bob has joined ***"
        );
        // Sin timestamp, devuelve tal cual
        assert_eq!(
            strip_log_timestamp("No timestamp here"),
            "No timestamp here"
        );
    }
}
```

---

## Persistencia en el servidor

### Log del servidor

El servidor puede mantener su propio log para:
1. **Auditoría**: quién dijo qué y cuándo
2. **Replay**: enviar historial reciente a clientes que se reconectan
3. **Debug**: investigar problemas de producción

```rust
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct ServerLogger {
    inner: Arc<Mutex<ChatLogger>>,
}

impl ServerLogger {
    pub fn new(path: &str) -> io::Result<Self> {
        let logger = ChatLogger::new(path)?;
        Ok(Self {
            inner: Arc::new(Mutex::new(logger)),
        })
    }

    pub async fn log(&self, message: &str) {
        if let Ok(mut logger) = self.inner.lock().await {
            if let Err(e) = logger.log(message) {
                eprintln!("Failed to write log: {}", e);
            }
        }
    }

    pub fn clone_handle(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}
```

### Log por sala

Para el servidor con salas (T03), un archivo por sala:

```rust
use std::collections::HashMap;
use std::path::PathBuf;

pub struct RoomLogger {
    log_dir: PathBuf,
    writers: HashMap<String, BufWriter<File>>,
}

impl RoomLogger {
    pub fn new(log_dir: impl Into<PathBuf>) -> io::Result<Self> {
        let log_dir = log_dir.into();
        fs::create_dir_all(&log_dir)?;
        Ok(Self {
            log_dir,
            writers: HashMap::new(),
        })
    }

    pub fn log_to_room(&mut self, room: &str, message: &str) -> io::Result<()> {
        let writer = self.get_or_create_writer(room)?;
        writeln!(writer, "[{}] {}", epoch_secs(), message)?;
        writer.flush()
    }

    fn get_or_create_writer(&mut self, room: &str) -> io::Result<&mut BufWriter<File>> {
        if !self.writers.contains_key(room) {
            // Sanitizar el nombre de sala para usarlo como nombre de archivo
            let filename = room_to_filename(room);
            let path = self.log_dir.join(filename);

            let file = OpenOptions::new()
                .create(true)
                .append(true)
                .open(&path)?;
            self.writers.insert(room.to_string(), BufWriter::new(file));
        }

        Ok(self.writers.get_mut(room).unwrap())
    }
}

/// Convert room name to safe filename
/// "#rust-lang" → "rust-lang.log"
fn room_to_filename(room: &str) -> String {
    let name = room.trim_start_matches('#');
    format!("{}.log", name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_room_to_filename() {
        assert_eq!(room_to_filename("#general"), "general.log");
        assert_eq!(room_to_filename("#rust-lang"), "rust-lang.log");
        assert_eq!(room_to_filename("#off_topic"), "off_topic.log");
    }
}
```

### Replay de historial al reconectarse

Cuando un usuario se une a una sala, enviar las últimas N líneas del log:

```rust
impl ServerState {
    pub async fn replay_room_history(
        &self,
        addr: &SocketAddr,
        room: &str,
        room_logger: &RoomLogger,
        max_lines: usize,
    ) {
        let client = match self.clients.get(addr) {
            Some(c) => c,
            None => return,
        };

        let filename = room_to_filename(room);
        let path = room_logger.log_dir.join(&filename);
        let path_str = path.to_string_lossy();

        match tail_lines(&path_str, max_lines) {
            Ok(lines) if !lines.is_empty() => {
                let header = format!(
                    "*** Last {} messages in {} ***",
                    lines.len(), room
                );
                let _ = client.direct_tx.send(format!("{}\n", header));

                for line in &lines {
                    let message = strip_log_timestamp(line);
                    let _ = client.direct_tx.send(format!("{}\n", message));
                }

                let footer = "*** End of history ***".to_string();
                let _ = client.direct_tx.send(format!("{}\n", footer));
            }
            _ => {
                // Sin historial o error leyendo: no enviar nada
            }
        }
    }
}
```

### Sesión de ejemplo con replay

```
$ cargo run -- 127.0.0.1:8080
Connected!
/join #rust
*** Joined #rust (2 members) — Topic: Rust programming ***
*** Last 5 messages in #rust ***
[Alice] Anyone tried tokio 1.36?
[Bob] Yes, the new select! syntax is nicer
[Alice] Cool, I'll upgrade then
*** Carol has joined #rust ***
[Carol] Hello!
*** End of history ***
>
```

---

## Integración con el cliente TUI

### Logger en el cliente

El cliente de T02 puede guardar cada mensaje recibido al disco:

```rust
struct ChatClient {
    history: MessageHistory,
    logger: Option<ChatLogger>,
    // ... otros campos
}

impl ChatClient {
    fn new(log_path: Option<&str>) -> Self {
        let logger = log_path.and_then(|path| {
            ChatLogger::new(path).ok()
        });

        let mut history = MessageHistory::new(10_000);

        // Cargar historial previo si existe
        if let Some(path) = log_path {
            load_history_from_disk(path, &mut history, 100);
        }

        Self {
            history,
            logger,
        }
    }

    fn receive_message(&mut self, message: String) {
        // Guardar en disco
        if let Some(ref mut logger) = self.logger {
            let _ = logger.log(&message);
        }

        // Agregar al historial en memoria
        self.history.push(message);
    }
}
```

### Ruta del archivo de log

```rust
fn default_log_path(room: &str) -> PathBuf {
    // ~/.local/share/chat-client/logs/
    let base = dirs::data_local_dir()
        .unwrap_or_else(|| PathBuf::from("."));
    let log_dir = base.join("chat-client").join("logs");
    fs::create_dir_all(&log_dir).ok();
    log_dir.join(format!("{}.log", room.trim_start_matches('#')))
}

// Sin el crate dirs, usar $HOME directamente:
fn default_log_path_simple(room: &str) -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
    let log_dir = PathBuf::from(home)
        .join(".chat-client")
        .join("logs");
    fs::create_dir_all(&log_dir).ok();
    log_dir.join(format!("{}.log", room.trim_start_matches('#')))
}
```

### Argumento de CLI para el log

```rust
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();

    let addr = args.get(1).cloned()
        .unwrap_or_else(|| "127.0.0.1:8080".into());

    let log_path = args.get(2).cloned()
        .unwrap_or_else(|| {
            let home = env::var("HOME").unwrap_or_else(|_| ".".into());
            format!("{}/.chat-client/chat.log", home)
        });

    // Asegurar que el directorio existe
    if let Some(parent) = Path::new(&log_path).parent() {
        fs::create_dir_all(parent).ok();
    }

    // ...
}
```

### Integración en el select! loop

```rust
loop {
    tokio::select! {
        // Mensaje del servidor
        result = server_reader.read_line(&mut server_line) => {
            match result {
                Ok(0) => {
                    client.receive_message(
                        "*** Server closed the connection ***".into()
                    );
                    render_full(&mut out, client.active_history(), &input)?;
                    break;
                }
                Ok(_) => {
                    let msg = server_line.trim_end().to_string();
                    client.receive_message(msg);
                    server_line.clear();
                    render_full(&mut out, client.active_history(), &input)?;
                }
                Err(e) => {
                    client.receive_message(
                        format!("*** Connection error: {} ***", e)
                    );
                    render_full(&mut out, client.active_history(), &input)?;
                    break;
                }
            }
        }

        // Evento de teclado
        Some(evt) = input_rx.recv() => {
            // ... procesar igual que en T02 ...
        }
    }
}
```

### Exportar historial

Bonus: dado que stdout está ocupado por la TUI, el log en disco funciona
como exportación automática. Pero si el usuario quiere exportar explícitamente:

```rust
// Comando /save para exportar el historial actual a un archivo
Command::Save(path) => {
    let history = client.active_history();
    match export_history(history, &path) {
        Ok(count) => {
            client.receive_message(
                format!("*** Saved {} messages to {} ***", count, path)
            );
        }
        Err(e) => {
            client.receive_message(
                format!("*** Failed to save: {} ***", e)
            );
        }
    }
}

fn export_history(history: &MessageHistory, path: &str) -> io::Result<usize> {
    let mut file = File::create(path)?;
    let messages = history.all_messages();
    for msg in messages {
        writeln!(file, "{}", msg)?;
    }
    Ok(messages.len())
}
```

---

## Errores comunes

### 1. No crear el directorio padre del log

```rust
// ❌ Falla si ~/.chat-client/ no existe
let file = OpenOptions::new()
    .create(true)
    .append(true)
    .open("~/.chat-client/logs/chat.log")?;
// Error: No such file or directory
// (create(true) crea el ARCHIVO, no los directorios padre)

// Además, ~ no se expande en Rust — es una convención del shell

// ✅ Usar $HOME y crear directorios
let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
let log_dir = format!("{home}/.chat-client/logs");
std::fs::create_dir_all(&log_dir)?;  // Crea toda la jerarquía
let file = OpenOptions::new()
    .create(true)
    .append(true)
    .open(format!("{log_dir}/chat.log"))?;
```

### 2. No hacer flush y perder mensajes

```rust
// ❌ Sin flush: los últimos mensajes pueden perderse en un crash
let mut writer = BufWriter::new(file);
writeln!(writer, "Important message")?;
// Si el proceso muere aquí, "Important message" está en el
// buffer de BufWriter, no en el disco

// ✅ Flush después de cada mensaje importante
writeln!(writer, "Important message")?;
writer.flush()?;
// Ahora está en disco (o al menos en el buffer del kernel)

// Para garantía total: fsync (más lento)
writer.flush()?;
writer.get_ref().sync_all()?; // fsync: fuerza al disco físico
```

### 3. Archivo de log creciendo sin límite

```rust
// ❌ Append sin rotación: después de semanas, el archivo tiene 500MB
let mut logger = ChatLogger::new("chat.log")?;
// Un mes después: 500MB de log

// ✅ Usar rotación por tamaño o por fecha
let mut logger = RotatingLogger::new(
    "chat.log",
    10 * 1024 * 1024,  // 10 MB max por archivo
    5,                  // Mantener 5 archivos históricos
)?;
// Total máximo: 50 MB en disco
```

### 4. Race condition al escribir desde múltiples tasks

```rust
// ❌ Múltiples tasks escribiendo al mismo BufWriter sin sync
let logger = ChatLogger::new("server.log")?;
// Task A: logger.log("msg A")
// Task B: logger.log("msg B")
// Los writes pueden intercalarse: "msg msg AB\n\n"

// ✅ Proteger con Mutex
let logger = Arc::new(Mutex::new(ChatLogger::new("server.log")?));

// En cada task:
let mut guard = logger.lock().await;
guard.log("msg A")?;
// El lock garantiza que una sola task escribe a la vez
```

### 5. Cargar historial con formato incorrecto

```rust
// ❌ Asumir que todas las líneas del log tienen el formato esperado
fn load_line(line: &str) -> String {
    let bracket_end = line.find("] ").unwrap(); // PANIC si no hay "]"
    line[bracket_end + 2..].to_string()
}

// ✅ Manejar formatos inesperados
fn strip_log_timestamp(line: &str) -> String {
    if let Some(rest) = line.strip_prefix('[') {
        if let Some(pos) = rest.find("] ") {
            return rest[pos + 2..].to_string();
        }
    }
    // Si no matchea el formato, devolver la línea tal cual
    line.to_string()
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│        PERSISTIR HISTORIAL AL DISCO CHEATSHEET               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  OPEN FILE PARA APPEND                                       │
│  OpenOptions::new()                                          │
│    .create(true)       Crear si no existe                   │
│    .append(true)       Escribir al final                    │
│    .open(path)?        Abrir/crear                           │
│                                                              │
│  BUFWRITER                                                   │
│  BufWriter::new(file)  Buffer en userspace (8KB)            │
│  writer.flush()        Enviar buffer al kernel               │
│  file.sync_all()       fsync: forzar al disco físico        │
│                                                              │
│  CHATLOGGER                                                  │
│  ChatLogger::new(path) Abrir archivo en modo append         │
│  logger.log(msg)       Escribir [timestamp] msg + flush     │
│                                                              │
│  FORMATO DEL LOG                                             │
│  Texto:  [1710936332] [Alice] Hello!                        │
│  JSONL:  {"ts":..., "type":"msg", "sender":"Alice", ...}    │
│                                                              │
│  ROTACIÓN POR TAMAÑO                                        │
│  chat.log   ← actual                                        │
│  chat.log.1 ← anterior                                      │
│  chat.log.N ← más antiguo (se elimina al superar max)       │
│  Trigger: current_size + line_bytes > max_size               │
│                                                              │
│  ROTACIÓN POR FECHA                                          │
│  chat_2026-03-20.log ← un archivo por día                   │
│  Trigger: today_string() != current_date                     │
│                                                              │
│  LEER ÚLTIMAS N LÍNEAS                                       │
│  tail_lines(path, n)           Leer todo, tomar últimas n   │
│  tail_lines_efficient(path, n) Seek desde el final          │
│                                                              │
│  REPLAY (SERVIDOR)                                           │
│  1. Cliente hace /join #room                                │
│  2. Servidor lee tail_lines(room.log, 20)                   │
│  3. Envía por direct_tx: "*** Last N messages ***"          │
│  4. Envía cada línea del historial                          │
│  5. Envía "*** End of history ***"                          │
│                                                              │
│  INTEGRACIÓN CLIENTE                                         │
│  receive_message(msg):                                       │
│    logger.log(&msg)      → disco                            │
│    history.push(msg)     → memoria                          │
│  Al iniciar:                                                 │
│    load_history_from_disk(path, &mut history, 100)          │
│                                                              │
│  RUTAS RECOMENDADAS                                          │
│  Cliente: ~/.chat-client/logs/room.log                      │
│  Servidor: ./logs/room.log                                  │
│  Siempre: create_dir_all() antes de open()                  │
│                                                              │
│  SEGURIDAD                                                   │
│  - flush() después de cada mensaje                          │
│  - Arc<Mutex<Logger>> para acceso multi-task                │
│  - Rotación para limitar uso de disco                       │
│  - Manejar errores de I/O sin crashear                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Logger básico del cliente

Implementa la persistencia de historial en el cliente de T02:

**Tareas:**
1. Implementa `ChatLogger` con `OpenOptions::new().create(true).append(true)`
2. Cada mensaje recibido del servidor se escribe al log con timestamp
3. Usa `BufWriter` con flush después de cada línea
4. Al iniciar el cliente, lee las últimas 50 líneas del log y cárgalas
   en `MessageHistory`:
   ```bash
   # Primer inicio: historial vacío
   cargo run -- 127.0.0.1:8080

   # Chatear un rato, cerrar con /quit

   # Segundo inicio: los últimos 50 mensajes aparecen
   cargo run -- 127.0.0.1:8080
   # "*** Loaded 23 messages from history ***"
   # [Alice] Hello!
   # [Bob] Hi!
   # ... (mensajes de la sesión anterior)
   ```
5. Verifica que el log se puede leer con herramientas Unix:
   ```bash
   cat ~/.chat-client/chat.log
   grep "Alice" ~/.chat-client/chat.log
   tail -f ~/.chat-client/chat.log  # En otra terminal, ver en tiempo real
   wc -l ~/.chat-client/chat.log    # Contar mensajes
   ```

**Pregunta de reflexión:** ¿Por qué hacemos `flush()` después de cada mensaje
en vez de dejar que `BufWriter` acumule y escriba cuando el buffer se llene?
¿En qué escenario sería aceptable no hacer flush por cada mensaje?

---

### Ejercicio 2: Rotación y limpieza

Agrega rotación de archivos de log:

**Tareas:**
1. Implementa `RotatingLogger` con rotación por tamaño (1 MB, máximo 3 archivos)
2. Escribe un test que genera suficientes líneas para provocar una rotación:
   ```rust
   #[test]
   fn test_rotation() {
       let dir = tempfile::tempdir().unwrap();
       let path = dir.path().join("test.log");
       let mut logger = RotatingLogger::new(&path, 1024, 3).unwrap();

       // Escribir suficiente para provocar rotación
       for i in 0..100 {
           logger.log(&format!("Message number {}", i)).unwrap();
       }

       // Verificar que existen los archivos rotados
       assert!(path.exists());
       assert!(dir.path().join("test.log.1").exists());
   }
   ```
3. Verifica que el archivo más viejo se elimina al superar `max_files`
4. Alternativa: implementa `DailyLogger` que crea un archivo por día
5. Agrega un argumento CLI `--max-log-size` para configurar el tamaño máximo

**Pregunta de reflexión:** La rotación por tamaño puede cortar una sesión
de chat a mitad de una conversación. ¿Sería mejor rotar por sesión (nuevo
archivo cada vez que el cliente se conecta)? ¿Cuáles son las ventajas y
desventajas de cada estrategia?

---

### Ejercicio 3: Replay en el servidor

Implementa replay de historial en el servidor para clientes que se reconectan:

**Tareas:**
1. Agrega `RoomLogger` al servidor: un archivo por sala en `./logs/`
2. Cada mensaje que pasa por `send_to_room` se escribe al log de la sala
3. Cuando un usuario hace `/join #room`, enviar las últimas 20 líneas
   del log de la sala antes de los mensajes en vivo:
   ```
   /join #rust
   *** Joined #rust (2 members) ***
   *** Last 5 messages in #rust ***
   [Alice] Has anyone tried the new borrow checker?
   [Bob] Yes, much better error messages
   [Alice] Nice, upgrading now
   *** Carol has joined #rust ***
   [Carol] Hello!
   *** End of history ***
   >
   ```
4. Protege el `RoomLogger` con `Arc<Mutex<>>` para acceso desde múltiples tasks
5. Implementa `tail_lines_efficient` que lee desde el final del archivo
   para archivos grandes
6. Prueba que el replay funciona:
   - Inicia servidor y 2 clientes, envía mensajes
   - Desconecta un cliente
   - Reconecta → los mensajes anteriores aparecen como replay

**Pregunta de reflexión:** El replay envía mensajes por `direct_tx`, que
el cliente muestra igual que mensajes nuevos. ¿Cómo distinguiría el cliente
entre historial replay y mensajes en vivo? ¿Debería marcarlos visualmente
de forma diferente (ej: color gris para historial)?
