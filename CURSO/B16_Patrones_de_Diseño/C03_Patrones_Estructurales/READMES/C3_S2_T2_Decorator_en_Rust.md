# T02 — Decorator en Rust

---

## 1. De C a Rust: trait como interfaz del decorator

T01 uso una vtable (`StreamOps`) para que todos los componentes
de la cadena implementen la misma interfaz. En Rust, un trait
reemplaza la vtable:

```
  C (T01):                          Rust:
  ────────                          ─────
  StreamOps (function pointers)     trait Stream
  struct Stream { ops, ctx }        impl Stream for ConcreteType
  void *ctx                         Campos tipados del struct
  stream_write(s, data, len)        s.write(data)
  inner: Stream *                   inner: Box<dyn Stream>
```

---

## 2. Decorator basico con Box<dyn Trait>

```rust
use std::io::{self, Write};

/// Trait que define la interfaz (equivalente a StreamOps de T01)
trait Stream {
    fn write(&mut self, data: &[u8]) -> io::Result<usize>;
    fn flush(&mut self) -> io::Result<()>;
}

/// Implementacion base: escribe a un Vec (buffer en memoria)
struct MemoryStream {
    buffer: Vec<u8>,
}

impl MemoryStream {
    fn new() -> Self {
        Self { buffer: Vec::new() }
    }

    fn contents(&self) -> &[u8] {
        &self.buffer
    }
}

impl Stream for MemoryStream {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        self.buffer.extend_from_slice(data);
        Ok(data.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

/// Decorator: agrega prefijo a cada write
struct PrefixDecorator {
    inner: Box<dyn Stream>,
    prefix: Vec<u8>,
}

impl PrefixDecorator {
    fn new(inner: Box<dyn Stream>, prefix: &str) -> Self {
        Self {
            inner,
            prefix: prefix.as_bytes().to_vec(),
        }
    }
}

impl Stream for PrefixDecorator {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        self.inner.write(&self.prefix)?;
        self.inner.write(data)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

/// Decorator: convierte a uppercase
struct UppercaseDecorator {
    inner: Box<dyn Stream>,
}

impl UppercaseDecorator {
    fn new(inner: Box<dyn Stream>) -> Self {
        Self { inner }
    }
}

impl Stream for UppercaseDecorator {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        let upper: Vec<u8> = data.iter()
            .map(|b| b.to_ascii_uppercase())
            .collect();
        self.inner.write(&upper)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

fn main() {
    // Cadena: memory ← prefix ← uppercase
    let mem = Box::new(MemoryStream::new());
    let prefixed = Box::new(PrefixDecorator::new(mem, "[LOG] "));
    let mut output = UppercaseDecorator::new(prefixed);

    output.write(b"hello world").unwrap();

    // Para ver el resultado necesitamos acceso al MemoryStream
    // (ver seccion 7 para alternativas)
}
```

```
  Flujo de datos:
  ───────────────
  output.write(b"hello world")
  │
  ├─ UppercaseDecorator::write()
  │   b"hello world" → b"HELLO WORLD"
  │   self.inner.write(b"HELLO WORLD")
  │   │
  │   ├─ PrefixDecorator::write()
  │   │   self.inner.write(b"[LOG] ")
  │   │   self.inner.write(b"HELLO WORLD")
  │   │   │
  │   │   ├─ MemoryStream::write(b"[LOG] ")
  │   │   │   buffer = b"[LOG] "
  │   │   ├─ MemoryStream::write(b"HELLO WORLD")
  │   │   │   buffer = b"[LOG] HELLO WORLD"
```

---

## 3. Decorator con generics (static dispatch)

`Box<dyn Stream>` usa dynamic dispatch (vtable). Con generics,
el compilador genera codigo especializado — zero overhead:

```rust
trait Stream {
    fn write(&mut self, data: &[u8]) -> io::Result<usize>;
    fn flush(&mut self) -> io::Result<()>;
}

/// Decorator generico: el tipo del inner es conocido en compile time
struct PrefixDecorator<S: Stream> {
    inner: S,
    prefix: Vec<u8>,
}

impl<S: Stream> PrefixDecorator<S> {
    fn new(inner: S, prefix: &str) -> Self {
        Self {
            inner,
            prefix: prefix.as_bytes().to_vec(),
        }
    }
}

impl<S: Stream> Stream for PrefixDecorator<S> {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        self.inner.write(&self.prefix)?;
        self.inner.write(data)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

struct UppercaseDecorator<S: Stream> {
    inner: S,
}

impl<S: Stream> UppercaseDecorator<S> {
    fn new(inner: S) -> Self {
        Self { inner }
    }
}

impl<S: Stream> Stream for UppercaseDecorator<S> {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        let upper: Vec<u8> = data.iter()
            .map(|b| b.to_ascii_uppercase())
            .collect();
        self.inner.write(&upper)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

fn main() {
    let mem = MemoryStream::new();
    let prefixed = PrefixDecorator::new(mem, "[LOG] ");
    let mut output = UppercaseDecorator::new(prefixed);

    output.write(b"hello").unwrap();
    // Tipo de output: UppercaseDecorator<PrefixDecorator<MemoryStream>>
    // El compilador conoce la cadena completa — zero vtable
}
```

### 3.1 Generics vs Box<dyn>

```
  Box<dyn Stream> (dynamic):
  ──────────────────────────
  inner: Box<dyn Stream>
  → vtable lookup en cada write()
  → composicion en RUNTIME (puedes elegir decoradores con config)
  → un solo tipo: Box<dyn Stream>

  Generics (static):
  ──────────────────
  inner: S where S: Stream
  → inline, sin vtable
  → composicion en COMPILE TIME (la cadena esta fijada)
  → tipo compuesto: UppercaseDecorator<PrefixDecorator<MemoryStream>>

  ¿Cual elegir?
  → Config determina decoradores en runtime → Box<dyn>
  → Cadena fija conocida en compile time → generics
  → Hot path, performance critico → generics
  → Muchas combinaciones, binary size importa → Box<dyn>
```

---

## 4. Decorator con std::io::Write

En vez de un trait custom, podemos decorar `std::io::Write`
directamente. Esto permite usar los decoradores con cualquier
tipo que implemente `Write` (File, TcpStream, Vec<u8>, etc.):

```rust
use std::io::{self, Write};

/// Decorator sobre cualquier Write
struct LoggingWriter<W: Write> {
    inner: W,
    label: String,
    bytes_written: usize,
    write_count: usize,
}

impl<W: Write> LoggingWriter<W> {
    fn new(inner: W, label: &str) -> Self {
        Self {
            inner,
            label: label.to_string(),
            bytes_written: 0,
            write_count: 0,
        }
    }

    fn stats(&self) -> (usize, usize) {
        (self.bytes_written, self.write_count)
    }

    /// Acceso al inner (para leer resultado si es Vec<u8>)
    fn into_inner(self) -> W {
        self.inner
    }
}

impl<W: Write> Write for LoggingWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.write_count += 1;
        let n = self.inner.write(buf)?;
        self.bytes_written += n;
        eprintln!("[{}] write #{}: {} bytes (total: {})",
                  self.label, self.write_count, n, self.bytes_written);
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        eprintln!("[{}] flush", self.label);
        self.inner.flush()
    }
}

fn main() -> io::Result<()> {
    // Decorator sobre Vec<u8>
    let buffer = Vec::new();
    let mut writer = LoggingWriter::new(buffer, "memory");

    writer.write_all(b"hello ")?;
    writer.write_all(b"world")?;
    writer.flush()?;

    let (bytes, count) = writer.stats();
    println!("Stats: {} bytes in {} writes", bytes, count);

    let result = writer.into_inner();
    println!("Content: {}", String::from_utf8_lossy(&result));

    Ok(())
}
```

### 4.1 Decorator sobre File

```rust
use std::fs::File;

fn main() -> io::Result<()> {
    let file = File::create("/tmp/decorated.log")?;
    let mut writer = LoggingWriter::new(file, "file-output");

    writeln!(writer, "First line")?;
    writeln!(writer, "Second line")?;
    writer.flush()?;

    // LoggingWriter<File> implementa Write
    // → funciona con writeln!, write_all, etc.
    Ok(())
}
```

### 4.2 Componer multiples decoradores sobre Write

```rust
struct CountingWriter<W: Write> {
    inner: W,
    count: usize,
}

impl<W: Write> CountingWriter<W> {
    fn new(inner: W) -> Self {
        Self { inner, count: 0 }
    }

    fn count(&self) -> usize { self.count }
    fn into_inner(self) -> W { self.inner }
}

impl<W: Write> Write for CountingWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.count += 1;
        self.inner.write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

fn main() -> io::Result<()> {
    let buffer = Vec::new();
    let counting = CountingWriter::new(buffer);
    let mut logging = LoggingWriter::new(counting, "output");

    logging.write_all(b"hello")?;
    logging.write_all(b"world")?;

    // Tipo: LoggingWriter<CountingWriter<Vec<u8>>>

    // Acceder a stats de cada layer:
    let (bytes, _) = logging.stats();
    println!("Logged bytes: {}", bytes);

    let counting = logging.into_inner();
    println!("Write calls to inner: {}", counting.count());

    let data = counting.into_inner();
    println!("Data: {}", String::from_utf8_lossy(&data));

    Ok(())
}
```

---

## 5. Decorator con BufWriter: el de la stdlib

`std::io::BufWriter` es un decorator en la stdlib:

```rust
use std::io::{BufWriter, Write};
use std::fs::File;

fn main() -> io::Result<()> {
    let file = File::create("/tmp/buffered.txt")?;

    // BufWriter es un decorator sobre File
    let mut writer = BufWriter::new(file);

    // Escribe al buffer interno (no al file)
    for i in 0..1000 {
        writeln!(writer, "line {}", i)?;
    }

    // flush escribe el buffer al file
    writer.flush()?;

    // into_inner() retorna el File original
    let _file = writer.into_inner()?;

    Ok(())
}
```

```
  BufWriter es exactamente el BufferedStream de T01:

  T01 C:                           Rust stdlib:
  ──────                           ────────────
  buffered_stream(inner, 4096)     BufWriter::with_capacity(4096, inner)
  buffered_write → buffer → inner  BufWriter::write → buffer → inner
  buffered_flush → flush inner     BufWriter::flush → flush inner
  buffered_close → flush + close   Drop → flush (automatico)

  Rust agrega:
  - Drop llama flush automaticamente (no puedes olvidar)
  - Generics: funciona con cualquier W: Write
  - into_inner() para recuperar el inner
```

Otros decoradores en la stdlib:
- `BufReader<R: Read>` — buffering de lectura
- `GzEncoder<W: Write>` (flate2 crate) — compresion
- `io::Cursor<T: AsRef<[u8]>>` — adapta bytes a Read/Seek

---

## 6. Decorator con ownership: into_inner()

A diferencia de C donde el decorator "posee" el inner y lo
libera en close(), en Rust puedes recuperar el inner:

```rust
struct EncryptWriter<W: Write> {
    inner: W,
    key: u8,
}

impl<W: Write> EncryptWriter<W> {
    fn new(inner: W, key: u8) -> Self {
        Self { inner, key }
    }

    /// Consume el decorator, retorna el inner
    fn into_inner(self) -> W {
        self.inner
    }

    /// Referencia al inner (sin consumir)
    fn get_ref(&self) -> &W {
        &self.inner
    }

    /// Referencia mutable al inner
    fn get_mut(&mut self) -> &mut W {
        &mut self.inner
    }
}

impl<W: Write> Write for EncryptWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let encrypted: Vec<u8> = buf.iter().map(|b| b ^ self.key).collect();
        self.inner.write(&encrypted)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}
```

```
  Patron into_inner / get_ref / get_mut:
  ───────────────────────────────────────

  into_inner(self) → W       Consume decorator, recupera inner
  get_ref(&self) → &W        Leer inner sin consumir
  get_mut(&mut self) → &mut W  Acceso mutable al inner

  BufWriter, BufReader, GzEncoder todos siguen este patron.
  Es la convencion de la stdlib para decoradores.
```

---

## 7. Comparacion con C: ownership y Drop

```
  C (T01):
  ────────
  stream_close(output)
  → close cascada: cada decorator libera su inner
  → Si olvidas close: LEAK
  → No puedes recuperar el inner despues de close

  Rust:
  ─────
  drop(output)  (o sale de scope)
  → Drop cascada: cada decorator droppea su inner
  → IMPOSIBLE olvidar — Drop es automatico
  → into_inner() permite recuperar el inner ANTES de drop

  let file = File::create("out.txt")?;
  let buf = BufWriter::new(file);
  // ... usar buf ...

  // Opcion A: dejar que Drop maneje todo (automatico)
  // buf sale de scope → flush → drop File → close fd

  // Opcion B: recuperar el file
  let file = buf.into_inner()?;  // buf se consume, file se recupera
  // Ahora file sigue abierto, sin BufWriter
```

---

## 8. Decorator con dyn para composicion runtime

Cuando la cadena de decoradores se decide en runtime (config,
flags de CLI, etc.), necesitas `Box<dyn Write>`:

```rust
use std::io::{self, Write, BufWriter};
use std::fs::File;

struct Config {
    output_path: String,
    use_buffering: bool,
    buffer_size: usize,
    use_encryption: bool,
    encrypt_key: u8,
    use_logging: bool,
}

fn build_writer(cfg: &Config) -> io::Result<Box<dyn Write>> {
    let file = File::create(&cfg.output_path)?;
    let mut writer: Box<dyn Write> = Box::new(file);

    if cfg.use_buffering {
        writer = Box::new(BufWriter::with_capacity(cfg.buffer_size, writer));
    }

    if cfg.use_encryption {
        writer = Box::new(EncryptWriter::new(writer, cfg.encrypt_key));
    }

    if cfg.use_logging {
        writer = Box::new(LoggingWriter::new(writer, "output"));
    }

    Ok(writer)
}

fn main() -> io::Result<()> {
    let cfg = Config {
        output_path: "/tmp/dynamic.dat".into(),
        use_buffering: true,
        buffer_size: 8192,
        use_encryption: true,
        encrypt_key: 0x42,
        use_logging: true,
    };

    let mut writer = build_writer(&cfg)?;
    writer.write_all(b"hello dynamic decorators")?;
    writer.flush()?;

    Ok(())
}
```

```
  Comparacion con T01 ejercicio 10 (build_stream en C):

  C:     Stream *s = file_stream_open(path);
         if (cfg.buffering) s = buffered_stream(s, size);
         if (cfg.encrypt)   s = encrypt_stream(s, key);

  Rust:  let mut w: Box<dyn Write> = Box::new(File::create(path)?);
         if cfg.buffering { w = Box::new(BufWriter::new(w)); }
         if cfg.encrypt   { w = Box::new(EncryptWriter::new(w, key)); }

  Estructura identica. La diferencia:
  - Rust: tipo verificado en cada paso
  - Rust: Drop automatico (sin stream_close manual)
  - C: sin allocation extra (el Box es overhead en Rust)
```

---

## 9. Decorator para Read

El mismo patron funciona para lectura:

```rust
use std::io::{self, Read};

struct DecompressReader<R: Read> {
    inner: R,
}

impl<R: Read> DecompressReader<R> {
    fn new(inner: R) -> Self {
        Self { inner }
    }
}

impl<R: Read> Read for DecompressReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        // Leer del inner (datos comprimidos)
        let mut compressed = vec![0u8; buf.len() * 2];
        let n = self.inner.read(&mut compressed)?;

        // "Descomprimir" (simplificado: invertir RLE)
        let mut out_pos = 0;
        let mut i = 0;
        while i + 1 < n && out_pos < buf.len() {
            let count = compressed[i] as usize;
            let byte = compressed[i + 1];
            for _ in 0..count.min(buf.len() - out_pos) {
                buf[out_pos] = byte;
                out_pos += 1;
            }
            i += 2;
        }

        Ok(out_pos)
    }
}

struct DecryptReader<R: Read> {
    inner: R,
    key: u8,
}

impl<R: Read> DecryptReader<R> {
    fn new(inner: R, key: u8) -> Self {
        Self { inner, key }
    }
}

impl<R: Read> Read for DecryptReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let n = self.inner.read(buf)?;
        for byte in &mut buf[..n] {
            *byte ^= self.key;
        }
        Ok(n)
    }
}

// Composicion:
// File → Decrypt → Decompress → caller
fn open_encrypted_compressed(path: &str, key: u8) -> impl Read {
    let file = std::fs::File::open(path).unwrap();
    let decrypted = DecryptReader::new(file, key);
    DecompressReader::new(decrypted)
}
```

---

## 10. Comparacion completa: C vs Rust

```
  Patron C (T01)                    Equivalente Rust
  ──────────────                    ────────────────
  StreamOps vtable                  trait Stream (o std::io::Write)
  struct Stream { ops, ctx }        struct Decorator<W: Write> { inner: W }
  void *ctx                         Campos tipados del struct
  inner: Stream *                   inner: Box<dyn Write> o generico W
  stream_write(s, data, len)        writer.write(data)
  stream_close(s) cascada           Drop automatico en cascada
  No puedes recuperar inner         into_inner() / get_ref() / get_mut()
  Manual free en close              Drop automatico
  Un tipo (Stream *)                Tipo compuesto visible o Box<dyn>
  Siempre dynamic dispatch          Generics (static) o dyn (dynamic)

  Lo que Rust agrega:
  ────────────────────
  • Drop automatico: imposible olvidar flush/close
  • Generics: zero-cost static dispatch
  • into_inner(): recuperar el inner sin leak
  • Tipo compuesto: compilador verifica la cadena
  • impl Write: decoradores funcionan con toda la stdlib
```

---

## 11. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| No delegar flush al inner | Datos quedan en buffers intermedios | Cada flush debe llamar `self.inner.flush()` |
| Box<dyn> cuando generics basta | Overhead de vtable innecesario | Usar generics si la cadena es fija |
| into_inner sin flush previo | Datos en buffer se pierden | BufWriter::into_inner hace flush |
| Decorator que no propaga errores | Error del inner ignorado | Usar `?` para propagar `io::Result` |
| Decorator circular (A envuelve B envuelve A) | Stack overflow o deadlock | Cadena siempre lineal, termina en base |
| Escribir datos temporales con alloc en cada write | Overhead de allocation en hot path | Reutilizar buffer interno |
| Drop que puede panic (flush en Drop) | Panic en Drop es problematico | BufWriter ignora errores en Drop, flush explicitamente antes |
| Mezclar &mut self con acceso al inner | Borrow checker rechaza | Patron reborrow o split borrows |

---

## 12. Ejercicios

### Ejercicio 1 — Decorator basico con trait custom

Implementa un trait `Logger` con `log(&mut self, level, msg)`.
Crea un `ConsoleLogger` base y un `TimestampDecorator`.

**Prediccion**: ¿el TimestampDecorator modifica el mensaje o
agrega informacion alrededor?

<details>
<summary>Respuesta</summary>

```rust
use std::time::SystemTime;

trait Logger {
    fn log(&mut self, level: &str, msg: &str);
}

struct ConsoleLogger;

impl Logger for ConsoleLogger {
    fn log(&mut self, level: &str, msg: &str) {
        eprintln!("[{}] {}", level, msg);
    }
}

struct TimestampDecorator<L: Logger> {
    inner: L,
}

impl<L: Logger> TimestampDecorator<L> {
    fn new(inner: L) -> Self {
        Self { inner }
    }
}

impl<L: Logger> Logger for TimestampDecorator<L> {
    fn log(&mut self, level: &str, msg: &str) {
        let now = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap()
            .as_secs();
        // Modifica el mensaje: agrega timestamp al inicio
        let timestamped = format!("[t={}] {}", now, msg);
        self.inner.log(level, &timestamped);
    }
}

fn main() {
    let console = ConsoleLogger;
    let mut logger = TimestampDecorator::new(console);

    logger.log("INFO", "server started");
    logger.log("ERROR", "connection failed");
    // Output: [INFO] [t=1711612800] server started
    //         [ERROR] [t=1711612800] connection failed
}
```

Modifica el mensaje: antepone el timestamp al msg y pasa el
mensaje modificado al inner. El decorator transforma los datos
antes de delegar, no agrega output separado.

</details>

---

### Ejercicio 2 — Cadena de decoradores con generics

Agrega un `FilterDecorator` que solo pasa mensajes de nivel
ERROR o superior. Encadena: Console ← Timestamp ← Filter.

**Prediccion**: ¿que tipo tiene la variable `logger` final?

<details>
<summary>Respuesta</summary>

```rust
struct FilterDecorator<L: Logger> {
    inner: L,
    min_level: String,
}

impl<L: Logger> FilterDecorator<L> {
    fn new(inner: L, min_level: &str) -> Self {
        Self { inner, min_level: min_level.to_string() }
    }

    fn level_value(level: &str) -> u8 {
        match level {
            "DEBUG" => 0,
            "INFO"  => 1,
            "WARN"  => 2,
            "ERROR" => 3,
            _       => 0,
        }
    }
}

impl<L: Logger> Logger for FilterDecorator<L> {
    fn log(&mut self, level: &str, msg: &str) {
        if Self::level_value(level) >= Self::level_value(&self.min_level) {
            self.inner.log(level, msg);
        }
        // Si no pasa el filtro: se descarta silenciosamente
    }
}

fn main() {
    let console = ConsoleLogger;
    let timestamped = TimestampDecorator::new(console);
    let mut logger = FilterDecorator::new(timestamped, "WARN");

    logger.log("DEBUG", "ignored");       // Descartado
    logger.log("INFO", "also ignored");   // Descartado
    logger.log("WARN", "attention");      // Pasa → timestamp → console
    logger.log("ERROR", "critical!");     // Pasa → timestamp → console
}
```

Tipo de `logger`:
```
FilterDecorator<TimestampDecorator<ConsoleLogger>>
```

El compilador conoce la cadena completa. Cada llamada a `log()`
se inlinea — sin vtable, sin indirecion. El tipo es largo pero
el codigo generado es optimo.

</details>

---

### Ejercicio 3 — Decorator sobre std::io::Write

Implementa un `CountingWriter<W: Write>` que cuenta bytes
escritos. Usalo con `File` y con `Vec<u8>`.

**Prediccion**: ¿necesitas implementar `flush()`?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write};

struct CountingWriter<W: Write> {
    inner: W,
    bytes: usize,
}

impl<W: Write> CountingWriter<W> {
    fn new(inner: W) -> Self {
        Self { inner, bytes: 0 }
    }

    fn bytes_written(&self) -> usize { self.bytes }
    fn into_inner(self) -> W { self.inner }
}

impl<W: Write> Write for CountingWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let n = self.inner.write(buf)?;
        self.bytes += n;
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

fn main() -> io::Result<()> {
    // Con Vec<u8>
    let mut counter = CountingWriter::new(Vec::new());
    counter.write_all(b"hello ")?;
    counter.write_all(b"world")?;
    println!("Bytes: {}", counter.bytes_written());  // 11
    let data = counter.into_inner();
    println!("Data: {}", String::from_utf8_lossy(&data));

    // Con File
    let file = std::fs::File::create("/tmp/counted.txt")?;
    let mut counter = CountingWriter::new(file);
    writeln!(counter, "line 1")?;
    writeln!(counter, "line 2")?;
    println!("File bytes: {}", counter.bytes_written());

    Ok(())
}
```

Si, `flush()` es **obligatorio** porque es parte del trait
`Write`. Aunque el counting decorator no tiene buffer propio
que flushear, debe delegar al inner porque el inner podria ser
un `BufWriter` que si tiene buffer. No delegar flush = datos
perdidos en buffers intermedios.

</details>

---

### Ejercicio 4 — Decorator con Box<dyn> runtime

Escribe una funcion que construye una cadena de `Write` decorators
basada en una config, retornando `Box<dyn Write>`.

**Prediccion**: ¿por que no puedes retornar `impl Write`?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write, BufWriter};

struct Config {
    use_counting: bool,
    use_buffering: bool,
    buffer_size: usize,
}

fn build_writer(inner: impl Write + 'static, cfg: &Config) -> Box<dyn Write> {
    let mut writer: Box<dyn Write> = Box::new(inner);

    if cfg.use_buffering {
        writer = Box::new(BufWriter::with_capacity(cfg.buffer_size, writer));
    }

    if cfg.use_counting {
        writer = Box::new(CountingWriter::new(writer));
    }

    writer
}

fn main() -> io::Result<()> {
    let cfg = Config {
        use_counting: true,
        use_buffering: true,
        buffer_size: 4096,
    };

    let mut writer = build_writer(Vec::<u8>::new(), &cfg);
    writer.write_all(b"hello")?;
    writer.flush()?;

    Ok(())
}
```

No puedes retornar `impl Write` porque:
```rust
fn build_writer(...) -> impl Write {
    if cfg.use_buffering {
        return BufWriter::new(inner);  // Tipo A
    }
    inner  // Tipo B — DIFERENTE de Tipo A
}
// ERROR: `if` and `else` have incompatible types
// impl Trait requiere UN solo tipo concreto
```

`impl Write` en return position significa "un tipo concreto
que el compilador infiere". Con `if/else`, hay multiples tipos
posibles → el compilador no puede elegir uno. `Box<dyn Write>`
resuelve esto: todos los tipos se borran detras del trait object.

</details>

---

### Ejercicio 5 — Decorator XOR encrypt

Implementa `XorWriter<W: Write>` que XOR cada byte con una key.
Verifica que encrypt(encrypt(data)) == data.

**Prediccion**: ¿XOR es simetrico? ¿Puedes usar el mismo
decorator para encrypt y decrypt?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write, Read, Cursor};

struct XorWriter<W: Write> {
    inner: W,
    key: u8,
}

impl<W: Write> XorWriter<W> {
    fn new(inner: W, key: u8) -> Self {
        Self { inner, key }
    }

    fn into_inner(self) -> W { self.inner }
}

impl<W: Write> Write for XorWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let encrypted: Vec<u8> = buf.iter().map(|b| b ^ self.key).collect();
        self.inner.write(&encrypted)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

// XorReader para decrypt
struct XorReader<R: Read> {
    inner: R,
    key: u8,
}

impl<R: Read> XorReader<R> {
    fn new(inner: R, key: u8) -> Self { Self { inner, key } }
}

impl<R: Read> Read for XorReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let n = self.inner.read(buf)?;
        for byte in &mut buf[..n] {
            *byte ^= self.key;
        }
        Ok(n)
    }
}

fn main() -> io::Result<()> {
    let key = 0x42;
    let original = b"hello world";

    // Encrypt
    let mut enc = XorWriter::new(Vec::new(), key);
    enc.write_all(original)?;
    let encrypted = enc.into_inner();

    assert_ne!(&encrypted, original);
    println!("Encrypted: {:?}", encrypted);

    // Decrypt: mismo XOR
    let mut dec = XorReader::new(Cursor::new(&encrypted), key);
    let mut decrypted = Vec::new();
    dec.read_to_end(&mut decrypted)?;

    assert_eq!(&decrypted, original);
    println!("Decrypted: {}", String::from_utf8_lossy(&decrypted));

    // Probar simetria: XOR dos veces = original
    let mut double = XorWriter::new(Vec::new(), key);
    double.write_all(&encrypted)?;
    let result = double.into_inner();
    assert_eq!(&result, original);
    println!("Double XOR = original: true");

    Ok(())
}
```

Si, XOR es simetrico: `(x ^ key) ^ key == x`. El mismo
decorator (con la misma key) funciona para encrypt y decrypt.
`XorWriter` para escribir datos cifrados, `XorReader` para
leer datos cifrados. O usar `XorWriter` dos veces (encrypt
→ decrypt).

</details>

---

### Ejercicio 6 — Comparar con T01: Tee decorator

Implementa un TeeWriter que escribe a dos Writers simultaneamente
(como el ejercicio 9 de T01).

**Prediccion**: ¿como manejas errores si uno de los writers falla?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write};

struct TeeWriter<W1: Write, W2: Write> {
    primary: W1,
    secondary: W2,
}

impl<W1: Write, W2: Write> TeeWriter<W1, W2> {
    fn new(primary: W1, secondary: W2) -> Self {
        Self { primary, secondary }
    }

    fn into_parts(self) -> (W1, W2) {
        (self.primary, self.secondary)
    }
}

impl<W1: Write, W2: Write> Write for TeeWriter<W1, W2> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let n = self.primary.write(buf)?;
        // Secundario: best-effort (ignorar errores)
        let _ = self.secondary.write(&buf[..n]);
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.primary.flush()?;
        let _ = self.secondary.flush();
        Ok(())
    }
}

fn main() -> io::Result<()> {
    let file = std::fs::File::create("/tmp/tee_output.txt")?;
    let mut tee = TeeWriter::new(file, io::stdout());

    writeln!(tee, "This goes to file AND stdout")?;
    writeln!(tee, "So does this")?;
    tee.flush()?;

    Ok(())
}
```

Manejo de errores: el primario es autoritativo — si falla,
retornamos error. El secundario es best-effort: usamos `let _`
para ignorar errores. Esto es la misma semantica que T01
ejercicio 9 (y el comando `tee` de Unix).

Alternativa estricta: fallar si cualquiera falla:
```rust
fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
    let n1 = self.primary.write(buf)?;
    let n2 = self.secondary.write(&buf[..n1])?;
    Ok(n1.min(n2))
}
```

</details>

---

### Ejercicio 7 — into_inner en cadena

Tienes `LoggingWriter<CountingWriter<Vec<u8>>>`. Extrae el
Vec<u8> final.

**Prediccion**: ¿cuantas llamadas a into_inner necesitas?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write};

fn main() -> io::Result<()> {
    let buf = Vec::new();
    let counting = CountingWriter::new(buf);
    let mut logging = LoggingWriter::new(counting, "test");

    logging.write_all(b"hello")?;
    logging.write_all(b"world")?;

    // Desempaquetar la cadena
    let counting = logging.into_inner();    // 1. LoggingWriter → CountingWriter
    println!("Writes: {}", counting.bytes_written());

    let data = counting.into_inner();       // 2. CountingWriter → Vec<u8>
    println!("Data: {}", String::from_utf8_lossy(&data));

    Ok(())
}
```

Dos llamadas: una por cada layer de decorator. Con N decoradores,
necesitas N llamadas a into_inner. Cada una consume el decorator
externo y retorna el siguiente.

Esto es analogo a T01 donde close() cascadea, pero en Rust
puedes "pelar" la cadena layer por layer en vez de destruirla
toda de una vez.

</details>

---

### Ejercicio 8 — Decorator para Read con transformacion

Implementa un `Base64Reader<R: Read>` que lee datos raw y los
retorna codificados en base64 (simplificado).

**Prediccion**: ¿el decorator lee del inner y transforma, o
transforma y escribe al inner?

<details>
<summary>Respuesta</summary>

Lee del inner y transforma antes de entregar al caller:

```rust
use std::io::{self, Read};

const B64_CHARS: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct Base64Reader<R: Read> {
    inner: R,
    leftover: Vec<u8>,  // Bytes codificados pendientes de entregar
}

impl<R: Read> Base64Reader<R> {
    fn new(inner: R) -> Self {
        Self { inner, leftover: Vec::new() }
    }

    fn encode_chunk(input: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        for chunk in input.chunks(3) {
            let b0 = chunk[0] as u32;
            let b1 = if chunk.len() > 1 { chunk[1] as u32 } else { 0 };
            let b2 = if chunk.len() > 2 { chunk[2] as u32 } else { 0 };
            let combined = (b0 << 16) | (b1 << 8) | b2;

            out.push(B64_CHARS[((combined >> 18) & 0x3F) as usize]);
            out.push(B64_CHARS[((combined >> 12) & 0x3F) as usize]);
            if chunk.len() > 1 {
                out.push(B64_CHARS[((combined >> 6) & 0x3F) as usize]);
            } else {
                out.push(b'=');
            }
            if chunk.len() > 2 {
                out.push(B64_CHARS[(combined & 0x3F) as usize]);
            } else {
                out.push(b'=');
            }
        }
        out
    }
}

impl<R: Read> Read for Base64Reader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        // Si hay leftover, entregar primero
        if !self.leftover.is_empty() {
            let n = buf.len().min(self.leftover.len());
            buf[..n].copy_from_slice(&self.leftover[..n]);
            self.leftover.drain(..n);
            return Ok(n);
        }

        // Leer del inner
        let mut raw = vec![0u8; buf.len()];
        let n = self.inner.read(&mut raw)?;
        if n == 0 { return Ok(0); }

        // Codificar
        let encoded = Self::encode_chunk(&raw[..n]);
        let out_n = buf.len().min(encoded.len());
        buf[..out_n].copy_from_slice(&encoded[..out_n]);

        if encoded.len() > out_n {
            self.leftover = encoded[out_n..].to_vec();
        }

        Ok(out_n)
    }
}

fn main() -> io::Result<()> {
    let data = io::Cursor::new(b"Hello, World!");
    let mut reader = Base64Reader::new(data);

    let mut output = String::new();
    reader.read_to_string(&mut output)?;
    println!("Base64: {}", output);  // SGVsbG8sIFdvcmxkIQ==

    Ok(())
}
```

El decorator **lee** del inner (datos raw), **transforma** (base64
encode), y **entrega** al caller (datos codificados). Flujo:
inner → read raw → encode → buf del caller. Es un decorator
de lectura, no de escritura.

</details>

---

### Ejercicio 9 — Decorator con estado compartido

Dos decoradores necesitan compartir un contador. ¿Como lo haces
sin variable global?

**Prediccion**: ¿Arc<AtomicUsize> o Rc<Cell<usize>>?

<details>
<summary>Respuesta</summary>

Para single-thread: `Rc<Cell<usize>>`. Para multi-thread:
`Arc<AtomicUsize>`.

```rust
use std::io::{self, Write};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

/// Estado compartido entre decoradores
struct SharedStats {
    total_bytes: AtomicUsize,
    total_writes: AtomicUsize,
}

impl SharedStats {
    fn new() -> Arc<Self> {
        Arc::new(Self {
            total_bytes: AtomicUsize::new(0),
            total_writes: AtomicUsize::new(0),
        })
    }

    fn record(&self, bytes: usize) {
        self.total_bytes.fetch_add(bytes, Ordering::Relaxed);
        self.total_writes.fetch_add(1, Ordering::Relaxed);
    }

    fn bytes(&self) -> usize {
        self.total_bytes.load(Ordering::Relaxed)
    }

    fn writes(&self) -> usize {
        self.total_writes.load(Ordering::Relaxed)
    }
}

struct TrackedWriter<W: Write> {
    inner: W,
    stats: Arc<SharedStats>,
}

impl<W: Write> TrackedWriter<W> {
    fn new(inner: W, stats: Arc<SharedStats>) -> Self {
        Self { inner, stats }
    }
}

impl<W: Write> Write for TrackedWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let n = self.inner.write(buf)?;
        self.stats.record(n);
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

fn main() -> io::Result<()> {
    let stats = SharedStats::new();

    // Dos writers comparten las mismas stats
    let mut w1 = TrackedWriter::new(Vec::new(), Arc::clone(&stats));
    let mut w2 = TrackedWriter::new(Vec::new(), Arc::clone(&stats));

    w1.write_all(b"hello")?;
    w2.write_all(b"world!")?;
    w1.write_all(b"more")?;

    println!("Total bytes: {}", stats.bytes());   // 15
    println!("Total writes: {}", stats.writes());  // 3

    Ok(())
}
```

`Arc<AtomicUsize>` porque:
- `Arc` permite multiples owners (los dos decoradores)
- `AtomicUsize` permite mutacion sin Mutex (solo contadores)
- Thread-safe (si los writers fueran en threads diferentes)

Para single-thread, `Rc<Cell<usize>>` seria mas ligero pero
no thread-safe.

</details>

---

### Ejercicio 10 — BufWriter como decorator

Demuestra que `BufWriter` de la stdlib es un decorator:
- Usa BufWriter sobre File
- Compara el numero de syscalls con y sin BufWriter
- Usa into_inner() para recuperar el File

**Prediccion**: ¿cuantos syscalls hace escribir 1000 lineas
sin buffer vs con BufWriter de 8KB?

<details>
<summary>Respuesta</summary>

```rust
use std::io::{self, Write, BufWriter};
use std::fs::File;

fn write_without_buffer(path: &str) -> io::Result<()> {
    let mut file = File::create(path)?;
    for i in 0..1000 {
        // Cada writeln! = un syscall write()
        writeln!(file, "line {}: some data here", i)?;
    }
    Ok(())
    // ~1000 syscalls (uno por linea)
}

fn write_with_buffer(path: &str) -> io::Result<()> {
    let file = File::create(path)?;
    let mut writer = BufWriter::with_capacity(8192, file);
    for i in 0..1000 {
        // Escribe al buffer de 8KB, no al file
        writeln!(writer, "line {}: some data here", i)?;
    }
    writer.flush()?;

    // Recuperar el File
    let file = writer.into_inner()?;
    println!("File recovered: {:?}", file);

    Ok(())
    // ~3-4 syscalls (8KB buffer, ~25 bytes/line, ~320 lines/buffer)
}

fn main() -> io::Result<()> {
    write_without_buffer("/tmp/no_buf.txt")?;
    write_with_buffer("/tmp/with_buf.txt")?;

    // Verificar con strace:
    // strace -e write ./program 2>&1 | grep -c 'write('
    // Sin buffer: ~1000 write syscalls
    // Con buffer: ~4 write syscalls

    Ok(())
}
```

Sin buffer: ~1000 syscalls (uno por `writeln!`). Cada linea
es ~25 bytes, y cada `writeln!` llama `write(2)` directamente.

Con BufWriter de 8KB: ~3-4 syscalls. El buffer acumula hasta
8192 bytes (~320 lineas), luego hace un solo `write(2)` de 8KB.
1000 lineas / 320 por buffer ≈ 3.1 writes + 1 flush final ≈ 4.

BufWriter es el decorator canonico de la stdlib: misma interfaz
(`Write`), agrega buffering, transparente para el caller, y
`into_inner()` para recuperar el inner.

</details>
