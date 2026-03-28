# `std::fs` — Operaciones de Sistema de Archivos

## Índice

1. [Visión general de std::fs](#1-visión-general-de-stdfs)
2. [Leer archivos](#2-leer-archivos)
3. [Escribir archivos](#3-escribir-archivos)
4. [OpenOptions — Control fino de apertura](#4-openoptions--control-fino-de-apertura)
5. [Operaciones sobre archivos y directorios](#5-operaciones-sobre-archivos-y-directorios)
6. [Manejo de errores de I/O](#6-manejo-de-errores-de-io)
7. [Archivos temporales y atómicos](#7-archivos-temporales-y-atómicos)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Visión general de `std::fs`

El módulo `std::fs` es la interfaz de Rust para operaciones de sistema de archivos.
Todas las funciones son **síncronas y bloqueantes** — llaman directamente a syscalls
del OS (`open`, `read`, `write`, `stat`, `unlink`, etc.).

```
┌─────────────────────────────────────────────────────────┐
│                    std::fs                               │
├──────────────────┬──────────────────────────────────────┤
│  Leer            │  read, read_to_string                │
│  Escribir        │  write, File::create                 │
│  Copiar/Mover    │  copy, rename                        │
│  Eliminar        │  remove_file, remove_dir,            │
│                  │  remove_dir_all                      │
│  Directorios     │  create_dir, create_dir_all,         │
│                  │  read_dir                            │
│  Metadata        │  metadata, symlink_metadata          │
│  Links           │  hard_link, soft_link (unix)         │
│  Permisos        │  set_permissions                     │
└──────────────────┴──────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    std::fs::File                         │
├──────────────────┬──────────────────────────────────────┤
│  Abrir           │  open, create, options               │
│  Leer            │  Read trait (read, read_to_string)   │
│  Escribir        │  Write trait (write, write_all)      │
│  Posición        │  Seek trait (seek)                   │
│  Sync            │  sync_all, sync_data                 │
│  Metadata        │  metadata, set_len                   │
└──────────────────┴──────────────────────────────────────┘
```

La relación entre funciones libres y `File`:

```
fs::read_to_string("file.txt")
   ↓ equivale a ↓
File::open("file.txt")? → read_to_string(&mut contents)?

fs::write("file.txt", data)
   ↓ equivale a ↓
File::create("file.txt")? → write_all(data)?
```

Las funciones libres (`fs::read`, `fs::write`) son convenientes para operaciones simples.
`File` da control fino cuando necesitas leer/escribir incrementalmente, controlar flags
de apertura, o mantener un file descriptor abierto.

---

## 2. Leer archivos

### 2.1 `fs::read_to_string` — Leer como texto

La forma más simple. Lee todo el archivo a un `String`. Falla si el contenido no es UTF-8
válido.

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    let content = fs::read_to_string("/etc/hostname")?;
    println!("Hostname: {}", content.trim());
    Ok(())
}
```

Internamente:
1. Llama `open()` (syscall)
2. Llama `fstat()` para saber el tamaño → pre-alloca el `String`
3. Llama `read()` hasta EOF
4. Valida UTF-8
5. Cierra el file descriptor (Drop de `File`)

### 2.2 `fs::read` — Leer como bytes

Para archivos binarios o cuando no necesitas validar UTF-8:

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    let bytes: Vec<u8> = fs::read("/usr/bin/ls")?;
    println!("Size: {} bytes", bytes.len());
    println!("Magic: {:02x} {:02x} {:02x} {:02x}",
        bytes[0], bytes[1], bytes[2], bytes[3]);
    // ELF magic: 7f 45 4c 46
    Ok(())
}
```

### 2.3 Leer con `File` incrementalmente

Para archivos grandes que no quieres cargar completamente en memoria:

```rust
use std::fs::File;
use std::io::{self, Read, BufRead, BufReader};

fn main() -> io::Result<()> {
    // Leer en chunks
    let mut file = File::open("large_file.bin")?;
    let mut buffer = [0u8; 8192]; // 8 KB por lectura

    loop {
        let bytes_read = file.read(&mut buffer)?;
        if bytes_read == 0 { break; } // EOF

        process_chunk(&buffer[..bytes_read]);
    }

    Ok(())
}

fn process_chunk(data: &[u8]) {
    // Procesar cada chunk sin cargar todo en memoria
    println!("Chunk: {} bytes", data.len());
}
```

### 2.4 `BufReader` — Lectura bufferizada por líneas

`BufReader` envuelve un `Read` y añade un buffer interno (default 8 KB). Esencial para
leer línea por línea sin hacer una syscall por cada byte:

```rust
use std::fs::File;
use std::io::{self, BufRead, BufReader};

fn main() -> io::Result<()> {
    let file = File::open("/etc/passwd")?;
    let reader = BufReader::new(file);

    // lines() retorna un iterador de Result<String>
    for (i, line) in reader.lines().enumerate() {
        let line = line?;
        let user = line.split(':').next().unwrap_or("?");
        println!("User {}: {}", i, user);
    }

    Ok(())
}
```

```
Sin BufReader:              Con BufReader:
  read(1 byte) → syscall     read(8192 bytes) → syscall
  read(1 byte) → syscall     (siguiente byte del buffer)
  read(1 byte) → syscall     (siguiente byte del buffer)
  ...                         ... (8191 más sin syscall)
  1000 bytes = 1000 syscalls  1000 bytes = 1 syscall
```

### 2.5 Leer línea por línea sin allocar

`read_line` reutiliza un `String` existente en vez de allocar uno nuevo por línea:

```rust
use std::fs::File;
use std::io::{self, BufRead, BufReader};

fn count_lines(path: &str) -> io::Result<usize> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);
    let mut line = String::new();
    let mut count = 0;

    loop {
        line.clear(); // Reutilizar la misma allocación
        let bytes = reader.read_line(&mut line)?;
        if bytes == 0 { break; } // EOF
        count += 1;
    }

    Ok(count)
}

fn main() -> io::Result<()> {
    let count = count_lines("/etc/passwd")?;
    println!("Lines: {}", count);
    Ok(())
}
```

---

## 3. Escribir archivos

### 3.1 `fs::write` — Escribir de una vez

Crea el archivo (o lo trunca si existe) y escribe todo el contenido:

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    // Escribir texto
    fs::write("output.txt", "Hello, world!\n")?;

    // Escribir bytes
    fs::write("data.bin", &[0xDE, 0xAD, 0xBE, 0xEF])?;

    // Escribir datos formateados
    let report = format!(
        "Report generated at: {:?}\nItems: {}\n",
        std::time::SystemTime::now(),
        42
    );
    fs::write("report.txt", report)?;

    Ok(())
}
```

### 3.2 `File::create` y `Write` trait

Para escritura incremental o control del buffering:

```rust
use std::fs::File;
use std::io::{self, Write, BufWriter};

fn main() -> io::Result<()> {
    // File::create trunca si existe, crea si no existe
    let mut file = File::create("log.txt")?;
    writeln!(file, "Log started")?;
    writeln!(file, "Entry 1: OK")?;
    writeln!(file, "Entry 2: Error")?;
    // file se cierra al salir del scope (Drop)
    Ok(())
}
```

### 3.3 `BufWriter` — Escritura bufferizada

`BufWriter` acumula escrituras en un buffer interno y las envía al OS en bloques
grandes, reduciendo syscalls:

```rust
use std::fs::File;
use std::io::{self, Write, BufWriter};

fn write_csv(path: &str, rows: &[(String, u32)]) -> io::Result<()> {
    let file = File::create(path)?;
    let mut writer = BufWriter::new(file);

    writeln!(writer, "name,value")?;

    for (name, value) in rows {
        writeln!(writer, "{},{}", name, value)?;
        // Cada writeln! va al buffer, NO al disco
    }

    // flush() se llama automáticamente en Drop,
    // pero es buena práctica llamarlo explícitamente
    // para manejar errores de I/O
    writer.flush()?;
    Ok(())
}

fn main() -> io::Result<()> {
    let data = vec![
        ("alpha".into(), 1),
        ("beta".into(), 2),
        ("gamma".into(), 3),
    ];
    write_csv("data.csv", &data)?;
    Ok(())
}
```

```
Sin BufWriter (100 writeln!):        Con BufWriter:
  write("name,value\n") → syscall     write("name,value\n") → buffer
  write("alpha,1\n")    → syscall     write("alpha,1\n")    → buffer
  write("beta,2\n")     → syscall     write("beta,2\n")     → buffer
  ...                                  ...
  100 writeln! = 100 syscalls          flush() → 1 syscall (todo junto)
```

### 3.4 Append — Añadir sin truncar

```rust
use std::fs::OpenOptions;
use std::io::Write;

fn append_log(path: &str, message: &str) -> std::io::Result<()> {
    let mut file = OpenOptions::new()
        .append(true)  // Posición al final
        .create(true)  // Crear si no existe
        .open(path)?;

    writeln!(file, "[{}] {}", chrono_placeholder(), message)?;
    Ok(())
}

fn chrono_placeholder() -> String {
    format!("{:?}", std::time::SystemTime::now())
}

fn main() -> std::io::Result<()> {
    append_log("app.log", "Server started")?;
    append_log("app.log", "Client connected")?;
    append_log("app.log", "Request processed")?;
    Ok(())
}
```

---

## 4. `OpenOptions` — Control fino de apertura

`OpenOptions` es un builder que permite configurar exactamente cómo se abre un archivo.
Mapea directamente a los flags de `open()` en POSIX.

### 4.1 Flags disponibles

```rust
use std::fs::OpenOptions;

// Equivalencias con POSIX flags:
OpenOptions::new()
    .read(true)       // O_RDONLY
    .write(true)      // O_WRONLY (o O_RDWR con .read(true))
    .append(true)     // O_APPEND
    .truncate(true)   // O_TRUNC
    .create(true)     // O_CREAT
    .create_new(true) // O_CREAT | O_EXCL (falla si existe)
    .open("file")?;
```

### 4.2 Combinaciones comunes

```rust
use std::fs::OpenOptions;
use std::io::{Read, Write, Seek, SeekFrom};

fn main() -> std::io::Result<()> {
    // Lectura solamente (= File::open)
    let _f = OpenOptions::new()
        .read(true)
        .open("file.txt")?;

    // Escritura truncando (= File::create)
    let _f = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open("file.txt")?;

    // Append (añadir al final)
    let _f = OpenOptions::new()
        .append(true)
        .create(true)
        .open("file.txt")?;

    // Lectura + escritura (sin truncar)
    let mut f = OpenOptions::new()
        .read(true)
        .write(true)
        .open("file.txt")?;

    // Leer contenido actual
    let mut content = String::new();
    f.read_to_string(&mut content)?;
    println!("Current: {}", content);

    // Volver al inicio y sobrescribir
    f.seek(SeekFrom::Start(0))?;
    f.write_all(b"overwritten")?;

    // Crear SOLO si no existe (evitar race condition)
    let result = OpenOptions::new()
        .write(true)
        .create_new(true) // Falla si ya existe
        .open("lockfile");

    match result {
        Ok(_) => println!("Lock acquired"),
        Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
            println!("Lock held by another process");
        }
        Err(e) => return Err(e),
    }

    Ok(())
}
```

### 4.3 Permisos en creación (Unix)

```rust
use std::fs::OpenOptions;
use std::os::unix::fs::OpenOptionsExt;

fn main() -> std::io::Result<()> {
    // Crear archivo con permisos específicos (modo 0o600 = rw-------)
    let _f = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o600) // Solo owner lee/escribe
        .open("secret.key")?;

    // Script ejecutable (modo 0o755)
    let _f = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o755)
        .open("script.sh")?;

    Ok(())
}
```

### 4.4 `Seek` — Navegación dentro del archivo

```rust
use std::fs::OpenOptions;
use std::io::{Read, Write, Seek, SeekFrom};

fn main() -> std::io::Result<()> {
    // Crear archivo de ejemplo
    std::fs::write("seek_demo.bin", b"Hello, World!")?;

    let mut file = OpenOptions::new()
        .read(true)
        .write(true)
        .open("seek_demo.bin")?;

    // Leer desde posición 7
    file.seek(SeekFrom::Start(7))?;
    let mut buf = [0u8; 5];
    file.read_exact(&mut buf)?;
    println!("At offset 7: {}", std::str::from_utf8(&buf).unwrap());
    // "World"

    // Ir al final y obtener tamaño
    let size = file.seek(SeekFrom::End(0))?;
    println!("File size: {} bytes", size);

    // Retroceder 6 bytes desde la posición actual
    file.seek(SeekFrom::End(-6))?;
    file.write_all(b"Rust!!")?;

    // Verificar
    file.seek(SeekFrom::Start(0))?;
    let mut result = String::new();
    file.read_to_string(&mut result)?;
    println!("Modified: {}", result); // "Hello, Rust!!"

    std::fs::remove_file("seek_demo.bin")?;
    Ok(())
}
```

```
SeekFrom::Start(n)    → Posición absoluta desde el inicio
SeekFrom::End(n)      → Offset desde el final (negativo = retroceder)
SeekFrom::Current(n)  → Offset relativo a la posición actual

Archivo: [H][e][l][l][o][,][ ][W][o][r][l][d][!]
Offset:   0  1  2  3  4  5  6  7  8  9 10 11 12

Start(7)    → posición 7 (W)
End(0)      → posición 13 (después del último byte)
End(-6)     → posición 7 (W)
Current(3)  → avanza 3 desde donde esté
```

---

## 5. Operaciones sobre archivos y directorios

### 5.1 Copiar, mover, eliminar archivos

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    // Crear archivo de prueba
    fs::write("original.txt", "test data")?;

    // Copiar (crea nuevo archivo con el mismo contenido)
    let bytes_copied = fs::copy("original.txt", "backup.txt")?;
    println!("Copied {} bytes", bytes_copied);
    // copy() preserva permisos pero NO timestamps

    // Renombrar / mover (atómico si en el mismo filesystem)
    fs::rename("backup.txt", "moved.txt")?;
    // rename() entre filesystems diferentes FALLA
    // → necesitarías copy() + remove_file()

    // Eliminar archivo
    fs::remove_file("original.txt")?;
    fs::remove_file("moved.txt")?;

    Ok(())
}
```

> **Importante**: `fs::rename` es atómico solo dentro del mismo filesystem. Mover un
> archivo de `/home` a `/tmp` (si son filesystems diferentes) falla con `EXDEV`.

### 5.2 Crear y eliminar directorios

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    // Crear un directorio (el padre debe existir)
    fs::create_dir("new_dir")?;

    // Crear directorios recursivamente (como mkdir -p)
    fs::create_dir_all("path/to/deep/directory")?;

    // Eliminar directorio vacío
    fs::remove_dir("new_dir")?;

    // Eliminar directorio con todo su contenido (como rm -rf)
    fs::remove_dir_all("path")?;
    // CUIDADO: NO pide confirmación, es irreversible

    Ok(())
}
```

```
create_dir("a/b/c")     → Error si "a/b" no existe
create_dir_all("a/b/c") → Crea a, a/b, a/b/c según sea necesario

remove_dir("dir")       → Error si dir no está vacío
remove_dir_all("dir")   → Elimina dir y todo su contenido
```

### 5.3 `read_dir` — Listar contenido de un directorio

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    // read_dir retorna un iterador de Result<DirEntry>
    for entry in fs::read_dir("/tmp")? {
        let entry = entry?;
        let path = entry.path();
        let file_type = entry.file_type()?;

        let kind = if file_type.is_dir() {
            "DIR "
        } else if file_type.is_file() {
            "FILE"
        } else if file_type.is_symlink() {
            "LINK"
        } else {
            "????"
        };

        println!("{} {}", kind, path.display());
    }

    Ok(())
}
```

> **Nota**: `read_dir` NO es recursivo. Para recorrer directorios recursivamente,
> ver T03 (walkdir).

### 5.4 Hard links y symlinks

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    fs::write("target.txt", "original content")?;

    // Hard link: mismo inode, mismo contenido
    fs::hard_link("target.txt", "hardlink.txt")?;

    // Symlink (Unix): apunta al path, no al inode
    #[cfg(unix)]
    std::os::unix::fs::symlink("target.txt", "symlink.txt")?;

    // Leer el destino de un symlink
    let target = fs::read_link("symlink.txt")?;
    println!("Symlink points to: {}", target.display());

    // metadata() sigue symlinks, symlink_metadata() no
    let meta = fs::metadata("symlink.txt")?;        // Info del target
    let link_meta = fs::symlink_metadata("symlink.txt")?; // Info del link

    println!("metadata (follows link): is_file={}", meta.is_file());
    println!("symlink_metadata: is_symlink={}", link_meta.is_symlink());

    // Limpiar
    fs::remove_file("target.txt")?;
    fs::remove_file("hardlink.txt")?;
    fs::remove_file("symlink.txt")?;

    Ok(())
}
```

```
Hard link:                        Symlink:
  target.txt ──┐                    target.txt ← symlink.txt
  hardlink.txt─┤─► Inode 12345         │
               │   (datos reales)      Si borras target.txt,
               │                       el symlink queda roto
  Si borras target.txt,               ("dangling symlink")
  hardlink.txt sigue funcionando
```

### 5.5 `sync_all` y `sync_data` — Durabilidad

```rust
use std::fs::File;
use std::io::Write;

fn durable_write(path: &str, data: &[u8]) -> std::io::Result<()> {
    let mut file = File::create(path)?;
    file.write_all(data)?;

    // sync_data: fuerza datos al disco (fdatasync)
    // No sincroniza metadata (timestamps, tamaño)
    file.sync_data()?;

    // sync_all: fuerza datos Y metadata al disco (fsync)
    // Más lento pero garantiza que ls -l muestra el tamaño correcto
    file.sync_all()?;

    Ok(())
}
```

```
Flujo de datos sin sync:
  write() → Buffer del kernel → (eventualmente) Disco

Flujo con sync_all():
  write() → Buffer del kernel → sync_all() → Disco (garantizado)

Si el sistema crashea entre write() y el flush del kernel,
los datos se pierden. sync_all() previene esto.
```

---

## 6. Manejo de errores de I/O

### 6.1 `std::io::Error` y `ErrorKind`

Todas las operaciones de `std::fs` retornan `std::io::Result<T>`, que es un alias para
`Result<T, std::io::Error>`. El `ErrorKind` permite tomar decisiones según el tipo de error:

```rust
use std::fs;
use std::io::ErrorKind;

fn safe_read(path: &str) -> String {
    match fs::read_to_string(path) {
        Ok(content) => content,
        Err(e) => match e.kind() {
            ErrorKind::NotFound => {
                eprintln!("File not found: {}", path);
                String::new()
            }
            ErrorKind::PermissionDenied => {
                eprintln!("Permission denied: {}", path);
                String::new()
            }
            _ => {
                eprintln!("Unexpected error reading {}: {}", path, e);
                String::new()
            }
        }
    }
}
```

### 6.2 ErrorKind más comunes en FS

```
┌──────────────────────┬──────────────────────────────────────┐
│ ErrorKind            │ Cuándo ocurre                        │
├──────────────────────┼──────────────────────────────────────┤
│ NotFound             │ Archivo/directorio no existe         │
│ PermissionDenied     │ Sin permisos (rwx)                   │
│ AlreadyExists        │ create_new y ya existe               │
│ IsADirectory         │ Intentar leer directorio como file   │
│ NotADirectory        │ Path incluye un archivo como dir     │
│ DirectoryNotEmpty    │ remove_dir con contenido             │
│ InvalidInput         │ Argumento inválido                   │
│ StorageFull          │ Disco lleno (ENOSPC)                 │
│ ReadOnlyFilesystem   │ FS montado read-only                 │
│ FileTooLarge         │ Excede límite de tamaño              │
│ CrossesDevices       │ rename entre filesystems (EXDEV)     │
└──────────────────────┴──────────────────────────────────────┘
```

### 6.3 Patrón "crear si no existe"

```rust
use std::fs;
use std::io::ErrorKind;

fn ensure_dir_exists(path: &str) -> std::io::Result<()> {
    match fs::create_dir(path) {
        Ok(()) => Ok(()),
        Err(e) if e.kind() == ErrorKind::AlreadyExists => Ok(()),
        Err(e) => Err(e),
    }
    // Equivalente más simple:
    // fs::create_dir_all(path)
    // create_dir_all no falla si ya existe
}

fn read_or_create_default(path: &str, default: &str) -> std::io::Result<String> {
    match fs::read_to_string(path) {
        Ok(content) => Ok(content),
        Err(e) if e.kind() == ErrorKind::NotFound => {
            fs::write(path, default)?;
            Ok(default.to_string())
        }
        Err(e) => Err(e),
    }
}
```

---

## 7. Archivos temporales y atómicos

### 7.1 Escritura atómica (write-rename pattern)

Escribir directamente a un archivo es peligroso: si el programa crashea a mitad de
escritura, el archivo queda corrupto. El patrón write-rename garantiza atomicidad:

```rust
use std::fs::{self, File};
use std::io::Write;
use std::path::Path;

fn atomic_write(path: &str, data: &[u8]) -> std::io::Result<()> {
    let path = Path::new(path);
    let temp_path = path.with_extension("tmp");

    // 1. Escribir a archivo temporal
    {
        let mut file = File::create(&temp_path)?;
        file.write_all(data)?;
        file.sync_all()?; // Asegurar datos en disco
    }

    // 2. Renombrar atómicamente (reemplaza el original)
    fs::rename(&temp_path, path)?;

    // 3. Sync del directorio padre (asegurar que el rename persista)
    #[cfg(unix)]
    {
        let parent = path.parent().unwrap_or(Path::new("."));
        let dir = File::open(parent)?;
        dir.sync_all()?;
    }

    Ok(())
}

fn main() -> std::io::Result<()> {
    atomic_write("config.json", b"{\"version\": 2}")?;
    println!("Config updated atomically");
    Ok(())
}
```

```
Sin write-rename:                   Con write-rename:
  open("config.json", TRUNC)         open("config.json.tmp")
  write(datos_parciales)             write(datos_completos)
  ← CRASH AQUÍ                      sync_all()
  config.json = corrupto              rename("config.json.tmp",
                                             "config.json")
                                      ← CRASH AQUÍ
                                      config.json = versión vieja
                                                    (intacta)
```

### 7.2 Lock file

Usar `create_new` para crear un lock file sin race conditions:

```rust
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::Path;

struct FileLock {
    path: std::path::PathBuf,
}

impl FileLock {
    fn acquire(path: &str) -> std::io::Result<Self> {
        let mut file = OpenOptions::new()
            .write(true)
            .create_new(true) // Atómico: falla si existe
            .open(path)?;

        // Escribir PID para debugging
        writeln!(file, "{}", std::process::id())?;

        Ok(FileLock {
            path: path.into(),
        })
    }
}

impl Drop for FileLock {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.path);
    }
}

fn main() {
    match FileLock::acquire("/tmp/myapp.lock") {
        Ok(_lock) => {
            println!("Lock acquired, doing work...");
            std::thread::sleep(std::time::Duration::from_secs(2));
            // _lock se dropea aquí → elimina el lockfile
        }
        Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
            eprintln!("Another instance is running");
            std::process::exit(1);
        }
        Err(e) => {
            eprintln!("Lock error: {}", e);
            std::process::exit(1);
        }
    }
}
```

### 7.3 Archivos temporales con `tempfile` crate

Para archivos temporales seguros, la crate `tempfile` es la opción idiomática:

```rust
use std::io::Write;

// Con la crate tempfile:
// tempfile::NamedTempFile — archivo temporal con nombre (para pasar a procesos)
// tempfile::tempfile()    — archivo temporal anónimo (se elimina al cerrar)

// Sin crate externa, enfoque manual:
fn manual_temp_file() -> std::io::Result<std::path::PathBuf> {
    let temp_dir = std::env::temp_dir();
    let temp_path = temp_dir.join(format!("myapp-{}.tmp", std::process::id()));

    let mut file = std::fs::File::create(&temp_path)?;
    file.write_all(b"temporary data")?;

    Ok(temp_path)
    // NOTA: el llamador debe limpiar el archivo
}
```

---

## 8. Errores comunes

### Error 1: No manejar `TOCTOU` (Time of Check to Time of Use)

```rust
use std::fs;
use std::path::Path;

// MAL — race condition TOCTOU
fn bad_write(path: &str, data: &str) -> std::io::Result<()> {
    if Path::new(path).exists() {
        // Otro proceso puede crear/eliminar el archivo AQUÍ
        // entre el check y la operación
        eprintln!("File already exists!");
        return Ok(());
    }
    fs::write(path, data)?; // ← Puede sobrescribir si otro proceso creó el archivo
    Ok(())
}

// BIEN — usar create_new que es atómico
fn good_write(path: &str, data: &str) -> std::io::Result<()> {
    use std::fs::OpenOptions;
    use std::io::Write;

    match OpenOptions::new()
        .write(true)
        .create_new(true) // Atómico: check + create en una sola syscall
        .open(path)
    {
        Ok(mut file) => {
            file.write_all(data.as_bytes())?;
            Ok(())
        }
        Err(e) if e.kind() == std::io::ErrorKind::AlreadyExists => {
            eprintln!("File already exists!");
            Ok(())
        }
        Err(e) => Err(e),
    }
}
```

**Por qué ocurre**: entre `exists()` y `write()`, otro proceso puede actuar. `create_new`
combina la verificación y la creación en una sola operación atómica del kernel (O_EXCL).

### Error 2: Ignorar errores de `flush` / `Drop` de `BufWriter`

```rust
use std::fs::File;
use std::io::{BufWriter, Write};

// MAL — error silencioso en flush
fn bad() {
    let file = File::create("data.txt").unwrap();
    let mut writer = BufWriter::new(file);
    writeln!(writer, "important data").unwrap();
    // Drop de BufWriter llama flush(), pero si flush() falla,
    // el error se IGNORA silenciosamente
}

// BIEN — flush explícito con manejo de error
fn good() -> std::io::Result<()> {
    let file = File::create("data.txt")?;
    let mut writer = BufWriter::new(file);
    writeln!(writer, "important data")?;
    writer.flush()?; // Manejar error explícitamente
    Ok(())
}
```

**Por qué importa**: si el disco se llena o hay un error de I/O durante `flush()`, los datos
no se escriben. El `Drop` de `BufWriter` intenta hacer flush pero no puede retornar un
error. El programa cree que escribió exitosamente, pero el archivo está incompleto.

### Error 3: `read_to_string` con archivos binarios

```rust
use std::fs;

// MAL — falla con archivos binarios
fn bad() {
    let content = fs::read_to_string("/usr/bin/ls");
    // Error: "stream did not contain valid UTF-8"
    // read_to_string requiere que el archivo sea UTF-8 válido
}

// BIEN — usar read() para archivos binarios
fn good() -> std::io::Result<()> {
    let bytes = fs::read("/usr/bin/ls")?; // Vec<u8>
    println!("Size: {} bytes", bytes.len());

    // Si QUIERES intentar interpretar como texto:
    match String::from_utf8(bytes) {
        Ok(text) => println!("Text: {}", text),
        Err(e) => println!("Not valid UTF-8: {}", e),
    }
    Ok(())
}
```

### Error 4: `remove_dir_all` sin precaución

```rust
use std::fs;
use std::path::Path;

// MAL — destrucción accidental
fn clean_build() -> std::io::Result<()> {
    let path = std::env::var("BUILD_DIR").unwrap_or_default();
    fs::remove_dir_all(&path)?; // Si path = "" o "/", desastre
    Ok(())
}

// BIEN — validar el path antes de borrar
fn safe_clean_build() -> std::io::Result<()> {
    let path = std::env::var("BUILD_DIR")
        .map_err(|_| std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "BUILD_DIR not set",
        ))?;

    let path = Path::new(&path);

    // Validaciones de seguridad
    if path.as_os_str().is_empty() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "BUILD_DIR is empty",
        ));
    }
    if path == Path::new("/") || path == Path::new("/home") {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "Refusing to delete system directory",
        ));
    }
    if !path.starts_with(std::env::current_dir()?) {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "BUILD_DIR outside project",
        ));
    }

    fs::remove_dir_all(path)?;
    Ok(())
}
```

**Por qué importa**: `remove_dir_all` es el equivalente de `rm -rf`. No pide confirmación.
Con un path vacío, malformado, o proveniente de input externo, puede destruir datos
irrecuperables.

### Error 5: Asumir que `rename` funciona entre filesystems

```rust
use std::fs;

// MAL — falla si /tmp y . están en diferentes filesystems
fn bad_move() -> std::io::Result<()> {
    fs::rename("/tmp/download.zip", "./download.zip")?;
    // Error: "Invalid cross-device link" (EXDEV)
    Ok(())
}

// BIEN — copy + remove como fallback
fn safe_move(from: &str, to: &str) -> std::io::Result<()> {
    match fs::rename(from, to) {
        Ok(()) => Ok(()),
        Err(e) if e.raw_os_error() == Some(libc::EXDEV) => {
            // Cross-device: copiar y luego eliminar
            fs::copy(from, to)?;
            fs::remove_file(from)?;
            Ok(())
        }
        Err(e) => Err(e),
    }
}

// Versión portable sin libc:
fn portable_move(from: &str, to: &str) -> std::io::Result<()> {
    if fs::rename(from, to).is_err() {
        fs::copy(from, to)?;
        fs::remove_file(from)?;
    }
    Ok(())
}
```

---

## 9. Cheatsheet

```text
──────────────────── Leer ────────────────────────────
fs::read_to_string(path)?          Todo como String (UTF-8)
fs::read(path)?                    Todo como Vec<u8> (binario)
File::open(path)?                  Abrir para lectura
BufReader::new(file)               Buffer de lectura (8KB)
reader.lines()                     Iterador de líneas
reader.read_line(&mut s)?         Leer una línea (reutiliza String)
file.read(&mut buf)?               Leer chunk en buffer
file.read_exact(&mut buf)?        Leer exactamente buf.len() bytes
file.read_to_end(&mut vec)?       Leer todo hasta EOF

──────────────────── Escribir ────────────────────────
fs::write(path, data)?             Crear/truncar y escribir todo
File::create(path)?                Crear/truncar para escritura
BufWriter::new(file)               Buffer de escritura
writer.write_all(data)?            Escribir todo
writeln!(writer, "{}", val)?       Escribir línea formateada
writer.flush()?                    Forzar escritura del buffer
file.sync_all()?                   Forzar datos+metadata a disco
file.sync_data()?                  Forzar solo datos a disco

──────────────────── OpenOptions ─────────────────────
OpenOptions::new()
    .read(true)                    Lectura
    .write(true)                   Escritura
    .append(true)                  Añadir al final
    .truncate(true)                Vaciar al abrir
    .create(true)                  Crear si no existe
    .create_new(true)              Crear, falla si existe (atómico)
    .mode(0o644)                   Permisos Unix (con use ...Ext)
    .open(path)?                   Abrir

──────────────────── Seek ────────────────────────────
file.seek(SeekFrom::Start(n))?     Desde inicio
file.seek(SeekFrom::End(n))?       Desde final
file.seek(SeekFrom::Current(n))?   Desde posición actual
file.stream_position()?            Posición actual (= seek Current(0))

──────────────────── Archivos/Dirs ───────────────────
fs::copy(src, dst)?                Copiar archivo (ret bytes)
fs::rename(old, new)?              Mover/renombrar (mismo FS)
fs::remove_file(path)?             Eliminar archivo
fs::create_dir(path)?              Crear directorio
fs::create_dir_all(path)?          Crear con padres (mkdir -p)
fs::remove_dir(path)?              Eliminar dir vacío
fs::remove_dir_all(path)?          Eliminar dir recursivo (rm -rf)
fs::read_dir(path)?                Listar contenido (iterador)
fs::hard_link(src, dst)?           Crear hard link
unix::fs::symlink(src, dst)?       Crear symlink (Unix)
fs::read_link(path)?               Leer destino de symlink

──────────────────── Metadata ────────────────────────
fs::metadata(path)?                Info del archivo (sigue symlinks)
fs::symlink_metadata(path)?        Info sin seguir symlinks
meta.len()                         Tamaño en bytes
meta.is_file() / is_dir()          Tipo
meta.is_symlink()                  Es symlink
meta.modified()? / accessed()?     Timestamps
meta.permissions()                 Permisos
file.set_len(n)?                   Truncar/extender a n bytes

──────────────────── ErrorKind ───────────────────────
NotFound                           No existe
PermissionDenied                   Sin permisos
AlreadyExists                      create_new pero ya existe
DirectoryNotEmpty                  remove_dir no vacío
```

---

## 10. Ejercicios

### Ejercicio 1: File copy con progreso

Implementa una función que copie un archivo mostrando progreso porcentual, usando
lectura en chunks:

```rust
use std::fs::{self, File};
use std::io::{self, Read, Write, BufWriter};
use std::path::Path;

fn copy_with_progress(src: &str, dst: &str) -> io::Result<u64> {
    // TODO:
    // 1. Abrir archivo fuente con File::open
    // 2. Obtener el tamaño total con metadata()
    // 3. Crear archivo destino con File::create envuelto en BufWriter
    // 4. Leer en chunks de 64 KB
    // 5. Por cada chunk leído, imprimir progreso:
    //    "Copied 1024000 / 5242880 bytes (19%)"
    //    Imprimir cada 10% para no saturar la terminal
    // 6. Flush del BufWriter al final
    // 7. Retornar total de bytes copiados

    todo!()
}

fn main() -> io::Result<()> {
    // Crear archivo de prueba de 5 MB
    let test_data = vec![0xABu8; 5 * 1024 * 1024];
    fs::write("test_large.bin", &test_data)?;

    let copied = copy_with_progress("test_large.bin", "test_copy.bin")?;
    println!("Total: {} bytes", copied);

    // Verificar que la copia es idéntica
    let original = fs::read("test_large.bin")?;
    let copy = fs::read("test_copy.bin")?;
    assert_eq!(original, copy, "Files differ!");
    println!("Verification passed!");

    // Limpiar
    fs::remove_file("test_large.bin")?;
    fs::remove_file("test_copy.bin")?;
    Ok(())
}
```

**Pregunta de reflexión**: ¿qué tamaño de chunk produce el mejor rendimiento? Si el chunk
es muy pequeño (1 byte) hay demasiadas syscalls. Si es demasiado grande (1 GB) usas
demasiada RAM. ¿Cómo encontrarías el óptimo? ¿Por qué 64 KB suele ser un buen valor
(relación con el page size del kernel)?

### Ejercicio 2: Escritura atómica de configuración

Implementa un sistema de configuración que lee/escribe archivos JSON de forma atómica:

```rust
use std::fs::{self, File};
use std::io::{self, Write};
use std::collections::HashMap;
use std::path::Path;

type Config = HashMap<String, String>;

fn load_config(path: &str) -> io::Result<Config> {
    // TODO:
    // 1. Si el archivo no existe, retornar HashMap vacío
    // 2. Leer el contenido
    // 3. Parsear como pares "key=value" (uno por línea)
    //    Ignorar líneas vacías y líneas que empiezan con #
    // 4. Retornar el HashMap

    todo!()
}

fn save_config(path: &str, config: &Config) -> io::Result<()> {
    // TODO: Implementar escritura ATÓMICA:
    // 1. Escribir a path.tmp
    // 2. Formato: "key=value\n" por cada entrada, ordenado por key
    // 3. sync_all() el archivo temporal
    // 4. rename(path.tmp, path)
    // 5. Si rename falla, eliminar el temporal y propagar el error

    todo!()
}

fn main() -> io::Result<()> {
    let path = "app_config.txt";

    // Cargar (o crear si no existe)
    let mut config = load_config(path)?;
    println!("Loaded {} entries", config.len());

    // Modificar
    config.insert("name".into(), "my_app".into());
    config.insert("version".into(), "1.0".into());
    config.insert("debug".into(), "false".into());

    // Guardar atómicamente
    save_config(path, &config)?;
    println!("Saved");

    // Verificar que se puede releer
    let reloaded = load_config(path)?;
    assert_eq!(config, reloaded);
    println!("Verification passed!");

    fs::remove_file(path)?;
    Ok(())
}
```

**Pregunta de reflexión**: ¿qué pasa si el programa crashea entre `rename()` y `sync_all()`
del directorio padre? ¿Los datos están a salvo? ¿Cuál es la diferencia entre "los datos
están en disco" y "el directorio apunta al nuevo archivo"? Investiga el concepto de
"rename barrier" en filesystems ext4 y XFS.

### Ejercicio 3: Tail -f (seguir un archivo)

Implementa una versión simplificada de `tail -f` que muestra las últimas N líneas de un
archivo y luego sigue mostrando nuevas líneas a medida que se añaden:

```rust
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom, BufRead, BufReader};
use std::time::Duration;
use std::thread;

fn tail_follow(path: &str, initial_lines: usize) -> io::Result<()> {
    // TODO:
    // 1. Abrir el archivo
    // 2. Mostrar las últimas `initial_lines` líneas
    //    (hint: leer desde el final, buscando '\n')
    // 3. Recordar la posición actual (después de las últimas líneas)
    // 4. En un loop infinito:
    //    a. Seek a la posición guardada
    //    b. Intentar leer nuevos datos
    //    c. Si hay datos nuevos, imprimir las líneas y actualizar posición
    //    d. Si no hay datos, sleep 100ms y reintentar
    //    e. Verificar si el archivo fue truncado (posición > tamaño)
    //       Si fue truncado, volver al inicio

    todo!()
}

fn main() -> io::Result<()> {
    let path = std::env::args().nth(1)
        .unwrap_or_else(|| "test.log".to_string());

    println!("Following {}...", path);
    tail_follow(&path, 10)
}

// Para probar en otra terminal:
// while true; do echo "$(date): log entry" >> test.log; sleep 1; done
```

**Pregunta de reflexión**: ¿qué pasa si el archivo es eliminado y recreado (log rotation)?
Tu implementación con `File` abierto seguirá leyendo del inode original (el archivo
eliminado), no del nuevo archivo. ¿Cómo detectarías esta situación? (Hint: comparar
inodes con `metadata().ino()` en Unix).