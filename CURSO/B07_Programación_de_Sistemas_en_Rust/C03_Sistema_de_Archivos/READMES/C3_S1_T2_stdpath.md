# `std::path` — Path y PathBuf

## Índice

1. [Path vs PathBuf: la dualidad str/String](#1-path-vs-pathbuf-la-dualidad-strstring)
2. [Crear y construir paths](#2-crear-y-construir-paths)
3. [Descomponer un path](#3-descomponer-un-path)
4. [Consultas sobre paths](#4-consultas-sobre-paths)
5. [Canonicalización y resolución](#5-canonicalización-y-resolución)
6. [Diferencias entre plataformas](#6-diferencias-entre-plataformas)
7. [Paths y Strings: conversiones](#7-paths-y-strings-conversiones)
8. [Patrones idiomáticos](#8-patrones-idiomáticos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. `Path` vs `PathBuf`: la dualidad `str`/`String`

Rust tiene dos tipos para representar paths del sistema de archivos. Su relación es
análoga a `str`/`String` y `[T]`/`Vec<T>`:

```
┌──────────────────────────────────────────────────────────────┐
│  Path                          PathBuf                       │
├──────────────────────────────────────────────────────────────┤
│  Referencia prestada (&Path)   Dueño del dato (owned)        │
│  No se puede modificar         Se puede modificar (push, pop)│
│  Tamaño desconocido (DST)      Tamaño conocido (en heap)     │
│  Análogo a &str                Análogo a String              │
│  Análogo a &[T]                Análogo a Vec<T>              │
└──────────────────────────────────────────────────────────────┘

Analogía completa:

  Texto:     &str        ↔  String
  Bytes:     &[u8]       ↔  Vec<u8>
  Paths:     &Path       ↔  PathBuf
  OS Strings: &OsStr     ↔  OsString
```

```rust
use std::path::{Path, PathBuf};

fn main() {
    // Path: referencia, no puede ser modificada
    let p: &Path = Path::new("/home/user/file.txt");
    println!("{}", p.display());

    // PathBuf: owned, puede ser modificada
    let mut pb: PathBuf = PathBuf::from("/home/user");
    pb.push("documents");
    pb.push("file.txt");
    println!("{}", pb.display());

    // PathBuf → &Path (gratis, como String → &str)
    let p_ref: &Path = pb.as_path();
    let p_ref2: &Path = &pb; // Deref coercion

    // &Path → PathBuf (clonar, alloca)
    let owned: PathBuf = p.to_path_buf();
    let owned2: PathBuf = p.to_owned();
}
```

### 1.1 ¿Por qué no son `String`/`&str`?

Los paths del OS **no son necesariamente UTF-8 válido**:

```
Linux:   Paths son secuencias de bytes (cualquier byte excepto 0x00 y /)
         Un archivo puede llamarse b"\xff\xfe" — no es UTF-8 válido

Windows: Paths son secuencias de UTF-16 code units
         Puede contener surrogates desemparejados — no es UTF-8

macOS:   Paths son UTF-8, pero con normalización NFC del filesystem
```

`Path` y `PathBuf` usan internamente `OsStr`/`OsString`, que puede representar estos
strings no-UTF-8 sin pérdida. Usar `String` para paths perdería datos en estos casos
edge.

```rust
use std::path::Path;
use std::ffi::OsStr;

fn main() {
    let p = Path::new("/home/user/file.txt");

    // Path internamente es un OsStr
    let os_str: &OsStr = p.as_os_str();

    // Conversión a &str: puede fallar si no es UTF-8
    match p.to_str() {
        Some(s) => println!("UTF-8 path: {}", s),
        None => println!("Path contains non-UTF-8 bytes"),
    }

    // display() siempre funciona (reemplaza bytes inválidos con �)
    println!("Display: {}", p.display());
}
```

---

## 2. Crear y construir paths

### 2.1 `Path::new` — Desde string literal o `&str`

```rust
use std::path::Path;

fn main() {
    // Desde &str
    let p1 = Path::new("/etc/passwd");

    // Desde String (referencia)
    let s = String::from("/var/log/syslog");
    let p2 = Path::new(&s);

    // Desde OsStr
    let os = std::ffi::OsStr::new("/tmp");
    let p3 = Path::new(os);

    println!("{} {} {}", p1.display(), p2.display(), p3.display());
}
```

### 2.2 `PathBuf::from` y `PathBuf::new`

```rust
use std::path::PathBuf;

fn main() {
    // Desde string
    let p1 = PathBuf::from("/home/user");

    // Vacío (para construir incrementalmente)
    let mut p2 = PathBuf::new();
    p2.push("/home");
    p2.push("user");
    p2.push("documents");
    // Resultado: "/home/user/documents"

    println!("{}", p2.display());
}
```

### 2.3 `push` y `join` — Construir paths

`push` modifica el PathBuf in-place. `join` retorna un nuevo PathBuf:

```rust
use std::path::{Path, PathBuf};

fn main() {
    // join: crea un nuevo PathBuf (no modifica el original)
    let base = Path::new("/home/user");
    let full = base.join("documents").join("report.pdf");
    println!("{}", full.display());
    // "/home/user/documents/report.pdf"

    // push: modifica in-place
    let mut path = PathBuf::from("/home/user");
    path.push("documents");
    path.push("report.pdf");
    println!("{}", path.display());
    // "/home/user/documents/report.pdf"

    // CUIDADO: push con path absoluto REEMPLAZA todo
    let mut path = PathBuf::from("/home/user");
    path.push("/etc/passwd");
    println!("{}", path.display());
    // "/etc/passwd" — NO "/home/user/etc/passwd"
}
```

```
join("relative"):     base + separator + relative
  /home/user + documents → /home/user/documents

join("/absolute"):    REEMPLAZA todo el path
  /home/user + /etc     → /etc

join(""):             No cambia nada
  /home/user + ""       → /home/user
```

### 2.4 `pop` — Quitar el último componente

```rust
use std::path::PathBuf;

fn main() {
    let mut path = PathBuf::from("/home/user/documents/file.txt");

    path.pop(); // Quita "file.txt"
    println!("{}", path.display()); // "/home/user/documents"

    path.pop(); // Quita "documents"
    println!("{}", path.display()); // "/home/user"

    path.pop(); // Quita "user"
    println!("{}", path.display()); // "/home"

    path.pop(); // Quita "home"
    println!("{}", path.display()); // "/"

    let popped = path.pop(); // Ya no hay más
    println!("Could pop: {}", popped); // false
    println!("{}", path.display()); // "/"
}
```

### 2.5 `set_file_name` y `set_extension`

```rust
use std::path::PathBuf;

fn main() {
    let mut path = PathBuf::from("/home/user/report.txt");

    // Cambiar nombre de archivo
    path.set_file_name("summary.txt");
    println!("{}", path.display()); // "/home/user/summary.txt"

    // Cambiar extensión
    path.set_extension("md");
    println!("{}", path.display()); // "/home/user/summary.md"

    // Quitar extensión
    path.set_extension("");
    println!("{}", path.display()); // "/home/user/summary"

    // Añadir extensión a archivo sin extensión
    path.set_extension("json");
    println!("{}", path.display()); // "/home/user/summary.json"
}
```

---

## 3. Descomponer un path

### 3.1 Componentes principales

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/home/user/documents/report.final.txt");

    // Directorio padre
    println!("parent:     {:?}", path.parent());
    // Some("/home/user/documents")

    // Nombre del archivo (último componente)
    println!("file_name:  {:?}", path.file_name());
    // Some("report.final.txt")

    // Stem (nombre sin la última extensión)
    println!("file_stem:  {:?}", path.file_stem());
    // Some("report.final")

    // Extensión (solo la última)
    println!("extension:  {:?}", path.extension());
    // Some("txt")

    // Prefijo (Windows: "C:", Unix: siempre None)
    println!("prefix:     {:?}", path.components().next());
    // RootDir (/)
}
```

```
Path: /home/user/documents/report.final.txt
      ├────────────────────┤├──────────────┤
      parent()              file_name()
                            ├────────┤├───┤
                            file_stem() extension()
```

### 3.2 `components()` — Iterador de componentes

```rust
use std::path::{Path, Component};

fn main() {
    let path = Path::new("/home/user/../user/./documents/file.txt");

    for component in path.components() {
        match component {
            Component::RootDir => println!("Root: /"),
            Component::Normal(name) => {
                println!("Normal: {:?}", name);
            }
            Component::ParentDir => println!("ParentDir: .."),
            Component::CurDir => println!("CurDir: ."),
            Component::Prefix(prefix) => {
                println!("Prefix: {:?}", prefix);
            }
        }
    }
}
```

Salida:
```
Root: /
Normal: "home"
Normal: "user"
ParentDir: ..
Normal: "user"
CurDir: .
Normal: "documents"
Normal: "file.txt"
```

### 3.3 `ancestors()` — Todos los padres

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/home/user/documents/file.txt");

    for ancestor in path.ancestors() {
        println!("{}", ancestor.display());
    }
}
```

Salida:
```
/home/user/documents/file.txt
/home/user/documents
/home/user
/home
/
```

### 3.4 `strip_prefix` — Quitar prefijo

```rust
use std::path::Path;

fn main() {
    let full = Path::new("/home/user/documents/report.txt");
    let base = Path::new("/home/user");

    match full.strip_prefix(base) {
        Ok(relative) => println!("Relative: {}", relative.display()),
        Err(_) => println!("Not a prefix"),
    }
    // "documents/report.txt"

    // Falla si no es un prefijo
    let other = Path::new("/var/log");
    assert!(full.strip_prefix(other).is_err());
}
```

---

## 4. Consultas sobre paths

### 4.1 Consultas que NO tocan el filesystem

Estas operaciones solo analizan el **string** del path, sin verificar si existe:

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/home/user/file.txt");

    // ¿Es absoluto? (empieza con / en Unix, C:\ en Windows)
    println!("is_absolute: {}", path.is_absolute());  // true

    // ¿Es relativo?
    println!("is_relative: {}", path.is_relative());  // false

    let rel = Path::new("src/main.rs");
    println!("is_absolute: {}", rel.is_absolute());   // false
    println!("is_relative: {}", rel.is_relative());   // true

    // ¿Empieza con cierto prefijo?
    println!("starts_with /home: {}", path.starts_with("/home")); // true
    println!("starts_with /var: {}", path.starts_with("/var"));   // false

    // ¿Termina con cierto sufijo?
    println!("ends_with file.txt: {}", path.ends_with("file.txt")); // true

    // ¿Tiene extensión?
    println!("has extension: {}", path.extension().is_some()); // true
}
```

### 4.2 Consultas que SÍ tocan el filesystem

Estas hacen syscalls reales (`stat`, `lstat`, etc.):

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/etc/passwd");

    // ¿Existe? (sigue symlinks)
    println!("exists: {}", path.exists());        // true

    // ¿Es un archivo regular?
    println!("is_file: {}", path.is_file());      // true

    // ¿Es un directorio?
    println!("is_dir: {}", path.is_dir());        // false

    // ¿Es un symlink? (NO sigue el symlink)
    println!("is_symlink: {}", path.is_symlink()); // false

    // Metadata completa
    if let Ok(meta) = path.metadata() {
        println!("size: {} bytes", meta.len());
        println!("readonly: {}", meta.permissions().readonly());
    }
}
```

> **Nota**: `exists()`, `is_file()`, `is_dir()` siguen symlinks. Si el symlink apunta a
> un archivo inexistente (dangling), `exists()` retorna `false`. Usa `symlink_metadata()`
> si necesitas verificar el link en sí.

### 4.3 `try_exists` — Distinguir "no existe" de "error de acceso"

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/root/secret");

    // exists() retorna false tanto si no existe como si no tienes permiso
    println!("exists: {}", path.exists()); // false (quizá por PermissionDenied)

    // try_exists() distingue los dos casos
    match path.try_exists() {
        Ok(true) => println!("Exists"),
        Ok(false) => println!("Does not exist"),
        Err(e) => println!("Cannot determine: {}", e),
        // Puede ser PermissionDenied
    }
}
```

---

## 5. Canonicalización y resolución

### 5.1 `canonicalize` — Path absoluto, resuelto

`canonicalize` resuelve `.`, `..`, y symlinks, retornando un path absoluto real.
El archivo o directorio **debe existir** (hace syscalls).

```rust
use std::path::Path;

fn main() -> std::io::Result<()> {
    // Resolver un path relativo
    let relative = Path::new("src/../Cargo.toml");
    let canonical = relative.canonicalize()?;
    println!("Canonical: {}", canonical.display());
    // Ej: "/home/user/project/Cargo.toml"

    // Resolver symlinks
    let link = Path::new("/usr/bin/python3");
    match link.canonicalize() {
        Ok(real) => println!("Real path: {}", real.display()),
        // Ej: "/usr/bin/python3.11"
        Err(e) => println!("Error: {}", e),
    }

    Ok(())
}
```

### 5.2 Normalización sin tocar el filesystem

`canonicalize` requiere que el path exista. Para normalizar sin filesystem,
puedes usar `components()`:

```rust
use std::path::{Path, PathBuf, Component};

fn normalize(path: &Path) -> PathBuf {
    let mut result = PathBuf::new();

    for component in path.components() {
        match component {
            Component::CurDir => {} // Ignorar "."
            Component::ParentDir => {
                result.pop(); // Subir un nivel
            }
            other => result.push(other),
        }
    }

    result
}

fn main() {
    let messy = Path::new("/home/user/../user/./documents/../file.txt");
    let clean = normalize(messy);
    println!("{}", clean.display());
    // "/home/user/file.txt"

    // Esto funciona sin que el path exista
    let imaginary = Path::new("/a/b/c/../../d/./e");
    println!("{}", normalize(imaginary).display());
    // "/a/d/e"
}
```

### 5.3 Hacer un path absoluto sin resolver symlinks

```rust
use std::path::{Path, PathBuf};

fn make_absolute(path: &Path) -> std::io::Result<PathBuf> {
    if path.is_absolute() {
        Ok(path.to_path_buf())
    } else {
        let cwd = std::env::current_dir()?;
        Ok(cwd.join(path))
    }
}

fn main() -> std::io::Result<()> {
    let rel = Path::new("src/main.rs");
    let abs = make_absolute(rel)?;
    println!("{}", abs.display());
    // "/home/user/project/src/main.rs"
    // (sin resolver .. ni symlinks)
    Ok(())
}
```

> **Nota**: desde Rust 1.79, `std::path::absolute()` está disponible y hace exactamente
> esto sin necesidad de función helper.

---

## 6. Diferencias entre plataformas

### 6.1 Separadores

```
┌──────────┬───────────────┬───────────────────────────────┐
│ OS       │ Separador     │ Ejemplo                       │
├──────────┼───────────────┼───────────────────────────────┤
│ Linux    │ /             │ /home/user/file.txt           │
│ macOS    │ /             │ /Users/user/file.txt          │
│ Windows  │ \ (y / a      │ C:\Users\user\file.txt        │
│          │ veces acepta) │ \\?\C:\Users\user\file.txt    │
└──────────┴───────────────┴───────────────────────────────┘
```

Rust abstrae esto: `Path::new("a").join("b")` usa el separador correcto del OS:

```rust
use std::path::{Path, MAIN_SEPARATOR};

fn main() {
    println!("Separator: '{}'", MAIN_SEPARATOR);
    // Linux/macOS: '/'
    // Windows: '\'

    // join() usa el separador correcto automáticamente
    let path = Path::new("home").join("user").join("file.txt");
    println!("{}", path.display());
    // Linux: "home/user/file.txt"
    // Windows: "home\user\file.txt"
}
```

### 6.2 Raíz del filesystem

```rust
use std::path::{Path, Component};

fn main() {
    // Unix: la raíz es /
    let unix_path = Path::new("/home/user");
    for c in unix_path.components() {
        println!("{:?}", c);
    }
    // RootDir
    // Normal("home")
    // Normal("user")

    // Windows (si compilas en Windows):
    // let win_path = Path::new(r"C:\Users\user");
    // Prefix(PrefixComponent { raw: "C:", parsed: Disk('C') })
    // RootDir
    // Normal("Users")
    // Normal("user")
}
```

### 6.3 Case sensitivity

```
┌──────────┬────────────────────────────────────────────┐
│ OS       │ Case sensitivity                           │
├──────────┼────────────────────────────────────────────┤
│ Linux    │ Case sensitive: "File.txt" ≠ "file.txt"   │
│ macOS    │ Case insensitive (default): "File.txt"     │
│          │ = "file.txt" (pero preserva case)          │
│ Windows  │ Case insensitive: "File.txt" = "FILE.TXT" │
└──────────┴────────────────────────────────────────────┘
```

`Path` **no** normaliza case — las comparaciones son byte-a-byte:

```rust
use std::path::Path;

fn main() {
    let p1 = Path::new("File.txt");
    let p2 = Path::new("file.txt");

    // Path::eq compara bytes, no filesystem case
    println!("Equal: {}", p1 == p2); // false en TODOS los OS

    // Para comparar según el OS, necesitarías canonicalize()
    // o una comparación case-insensitive manual
}
```

### 6.4 Longitud máxima de paths

```
┌──────────┬────────────────────────────────────────────┐
│ OS       │ Límite                                     │
├──────────┼────────────────────────────────────────────┤
│ Linux    │ PATH_MAX = 4096 bytes (path completo)      │
│          │ NAME_MAX = 255 bytes (componente individual)│
│ macOS    │ PATH_MAX = 1024 bytes                      │
│ Windows  │ MAX_PATH = 260 chars (legacy)              │
│          │ \\?\ prefix: ~32,767 chars (extended)      │
└──────────┴────────────────────────────────────────────┘
```

### 6.5 Caracteres especiales

```rust
use std::path::Path;

fn main() {
    // Linux: casi todo es válido excepto NUL y /
    let unusual = Path::new("file with spaces.txt");
    let unicode = Path::new("日本語ファイル.txt");
    let dots = Path::new("..hidden");

    println!("{}", unusual.display());
    println!("{}", unicode.display());
    println!("{}", dots.display());

    // Estos son paths válidos en Linux:
    // "file\twith\ttabs" — tabs en el nombre
    // "file\nwith\nnewlines" — newlines en el nombre
    // Evitarlos, pero Path los maneja correctamente
}
```

---

## 7. Paths y Strings: conversiones

### 7.1 Mapa de conversiones

```
                 Path/PathBuf
                      │
          ┌───────────┼───────────┐
          ↓           ↓           ↓
      to_str()    display()    as_os_str()
     Option<&str> Display fmt  &OsStr
          │                       │
          ↓                       ↓
   Some("valid")           OsStr (siempre OK)
   None (non-UTF8)
```

```rust
use std::path::{Path, PathBuf};

fn main() {
    let path = Path::new("/home/user/file.txt");

    // Path → &str (puede fallar)
    if let Some(s) = path.to_str() {
        println!("As str: {}", s);
    }

    // Path → String (puede fallar)
    if let Some(s) = path.to_str() {
        let string: String = s.to_string();
        println!("As String: {}", string);
    }

    // Path → display (siempre funciona, para impresión)
    println!("Display: {}", path.display());

    // &str → Path
    let p: &Path = Path::new("/tmp/file.txt");

    // String → PathBuf
    let s = String::from("/tmp/file.txt");
    let pb: PathBuf = PathBuf::from(s);
    let pb2: PathBuf = "/tmp/file.txt".into();

    // PathBuf → String (puede fallar)
    match pb.into_os_string().into_string() {
        Ok(s) => println!("String: {}", s),
        Err(os_string) => println!("Not UTF-8: {:?}", os_string),
    }
}
```

### 7.2 `to_string_lossy` — Conversión con reemplazo

```rust
use std::path::Path;

fn main() {
    let path = Path::new("/home/user/file.txt");

    // to_string_lossy(): siempre retorna algo
    // Si es UTF-8 válido, retorna Cow::Borrowed(&str)
    // Si no, retorna Cow::Owned(String) con bytes inválidos → '�'
    let s: std::borrow::Cow<str> = path.to_string_lossy();
    println!("{}", s); // "/home/user/file.txt"

    // Útil para logs y mensajes de error donde no puedes fallar
    log_path(path);
}

fn log_path(path: &Path) {
    // to_string_lossy es la forma más robusta de imprimir un path
    eprintln!("Processing: {}", path.to_string_lossy());
}
```

### 7.3 Recibir paths en funciones: `AsRef<Path>`

Para que una función acepte `&str`, `String`, `Path`, `PathBuf`, y más:

```rust
use std::path::Path;
use std::fs;

// Esta función acepta cualquier cosa convertible a &Path
fn file_size(path: impl AsRef<Path>) -> std::io::Result<u64> {
    let meta = fs::metadata(path.as_ref())?;
    Ok(meta.len())
}

fn main() -> std::io::Result<()> {
    // Todas estas llamadas compilan:
    let size1 = file_size("/etc/passwd")?;         // &str
    let size2 = file_size(String::from("/etc/hosts"))?; // String
    let size3 = file_size(Path::new("/etc/hostname"))?; // &Path
    let size4 = file_size(std::path::PathBuf::from("/etc/fstab"))?; // PathBuf

    println!("{} {} {} {}", size1, size2, size3, size4);
    Ok(())
}
```

```
Tipos que implementan AsRef<Path>:

  &str       ──┐
  String     ──┤
  &Path      ──┼──► AsRef<Path> → &Path
  PathBuf    ──┤
  &OsStr     ──┤
  OsString   ──┘
```

---

## 8. Patrones idiomáticos

### 8.1 Construir paths desde variables de entorno

```rust
use std::path::PathBuf;
use std::env;

fn config_path() -> PathBuf {
    // $XDG_CONFIG_HOME o fallback a ~/.config
    match env::var("XDG_CONFIG_HOME") {
        Ok(dir) => PathBuf::from(dir).join("myapp"),
        Err(_) => {
            let home = env::var("HOME").expect("HOME not set");
            PathBuf::from(home).join(".config").join("myapp")
        }
    }
}

fn data_dir() -> PathBuf {
    match env::var("XDG_DATA_HOME") {
        Ok(dir) => PathBuf::from(dir).join("myapp"),
        Err(_) => {
            let home = env::var("HOME").expect("HOME not set");
            PathBuf::from(home).join(".local").join("share").join("myapp")
        }
    }
}

fn main() {
    println!("Config: {}", config_path().display());
    println!("Data: {}", data_dir().display());
}
```

### 8.2 Cambiar extensión para archivos de salida

```rust
use std::path::{Path, PathBuf};

fn output_path(input: &Path, new_ext: &str) -> PathBuf {
    let mut output = input.to_path_buf();
    output.set_extension(new_ext);
    output
}

fn add_suffix(path: &Path, suffix: &str) -> PathBuf {
    let stem = path.file_stem().unwrap_or_default();
    let ext = path.extension();

    let mut new_name = stem.to_os_string();
    new_name.push(suffix);
    if let Some(ext) = ext {
        new_name.push(".");
        new_name.push(ext);
    }

    path.with_file_name(new_name)
}

fn main() {
    let input = Path::new("/data/image.png");

    let jpg = output_path(input, "jpg");
    println!("{}", jpg.display()); // "/data/image.jpg"

    let backup = add_suffix(input, "_backup");
    println!("{}", backup.display()); // "/data/image_backup.png"
}
```

### 8.3 Path relativo entre dos paths

```rust
use std::path::{Path, PathBuf, Component};

/// Calcula el path relativo de `path` respecto a `base`
fn relative_path(path: &Path, base: &Path) -> PathBuf {
    let mut path_components = path.components().peekable();
    let mut base_components = base.components().peekable();

    // Avanzar mientras los componentes sean iguales
    while let (Some(a), Some(b)) = (path_components.peek(), base_components.peek()) {
        if a != b { break; }
        path_components.next();
        base_components.next();
    }

    // Por cada componente restante en base, subir (..)
    let mut result = PathBuf::new();
    for _ in base_components {
        result.push("..");
    }

    // Añadir los componentes restantes de path
    for component in path_components {
        result.push(component);
    }

    result
}

fn main() {
    let file = Path::new("/home/user/project/src/main.rs");
    let base = Path::new("/home/user/project");

    let rel = relative_path(file, base);
    println!("{}", rel.display()); // "src/main.rs"

    let file2 = Path::new("/home/user/other/file.txt");
    let rel2 = relative_path(file2, base);
    println!("{}", rel2.display()); // "../../other/file.txt"
}
```

### 8.4 Validar y sanitizar paths de input del usuario

```rust
use std::path::{Path, PathBuf, Component};

fn sanitize_path(base: &Path, user_input: &str) -> Option<PathBuf> {
    let requested = Path::new(user_input);

    // Rechazar paths absolutos
    if requested.is_absolute() {
        return None;
    }

    // Construir el path completo
    let full = base.join(requested);

    // Verificar que no escapa del directorio base
    // Normalizar para resolver .. y .
    let mut normalized = PathBuf::new();
    for component in full.components() {
        match component {
            Component::ParentDir => {
                normalized.pop();
            }
            Component::CurDir => {}
            other => normalized.push(other),
        }
    }

    // Verificar que sigue dentro de base
    if normalized.starts_with(base) {
        Some(normalized)
    } else {
        None // Path traversal attempt
    }
}

fn main() {
    let base = Path::new("/var/www/files");

    // Válido
    assert!(sanitize_path(base, "docs/readme.txt").is_some());

    // Path traversal — rechazado
    assert!(sanitize_path(base, "../../etc/passwd").is_none());

    // Absoluto — rechazado
    assert!(sanitize_path(base, "/etc/passwd").is_none());
}
```

---

## 9. Errores comunes

### Error 1: Usar `format!` para construir paths

```rust
use std::path::PathBuf;

// MAL — hard-codea el separador como /
fn bad_build(dir: &str, file: &str) -> String {
    format!("{}/{}", dir, file)
    // En Windows esto produce "C:\Users/file" — separador mixto
}

// BIEN — usar join
fn good_build(dir: &str, file: &str) -> PathBuf {
    PathBuf::from(dir).join(file)
    // join usa el separador correcto del OS
}

fn main() {
    let path = good_build("/home/user", "file.txt");
    println!("{}", path.display());
}
```

**Por qué importa**: `format!` hard-codea `/` como separador. En Windows, el separador
nativo es `\`. `join()` usa `MAIN_SEPARATOR` automáticamente. Además, `format!` retorna
`String`, no `PathBuf`, perdiendo la semántica de path.

### Error 2: Comparar paths sin normalizar

```rust
use std::path::Path;

// MAL — dos paths al mismo archivo parecen diferentes
fn bad_compare() {
    let p1 = Path::new("/home/user/./docs/../docs/file.txt");
    let p2 = Path::new("/home/user/docs/file.txt");

    // false! Los strings son diferentes aunque apuntan al mismo archivo
    println!("Equal: {}", p1 == p2);
}

// BIEN — normalizar antes de comparar
fn good_compare() -> std::io::Result<()> {
    let p1 = Path::new("/home/user/./docs/../docs/file.txt");
    let p2 = Path::new("/home/user/docs/file.txt");

    // canonicalize resuelve todo
    let c1 = p1.canonicalize()?;
    let c2 = p2.canonicalize()?;
    println!("Equal: {}", c1 == c2); // true (si existen)

    Ok(())
}
```

**Por qué ocurre**: `Path` compara byte-a-byte el string del path, no el inode del
filesystem. `./` y `../dir/` hacen que dos paths al mismo archivo se vean diferentes.

### Error 3: `push` con path absoluto reemplaza todo

```rust
use std::path::PathBuf;

// MAL — push con "/" al inicio reemplaza el path
fn bad_push() {
    let mut path = PathBuf::from("/home/user");

    // Esto REEMPLAZA todo en vez de añadir
    path.push("/etc/passwd");
    println!("{}", path.display()); // "/etc/passwd" — NO "/home/user/etc/passwd"
}

// BIEN — quitar el / inicial o usar join con path relativo
fn good_push() {
    let mut path = PathBuf::from("/home/user");
    path.push("etc/passwd"); // Sin / al inicio
    println!("{}", path.display()); // "/home/user/etc/passwd"
}

// BIEN — o verificar antes de push
fn safe_push(base: &mut PathBuf, component: &str) {
    let p = std::path::Path::new(component);
    if p.is_absolute() {
        // Quitar la raíz para hacer relativo
        if let Ok(stripped) = p.strip_prefix("/") {
            base.push(stripped);
        }
    } else {
        base.push(component);
    }
}
```

### Error 4: Asumir que `to_str()` siempre funciona

```rust
use std::path::Path;

// MAL — unwrap en to_str() puede panic
fn bad(path: &Path) -> &str {
    path.to_str().unwrap() // Panic si hay bytes non-UTF-8
}

// BIEN — manejar el caso non-UTF-8
fn good(path: &Path) -> String {
    // Opción 1: lossy (reemplaza bytes inválidos con '�')
    path.to_string_lossy().into_owned()
}

fn good_ref(path: &Path) -> std::borrow::Cow<str> {
    // Opción 2: Cow para evitar allocar si ya es UTF-8
    path.to_string_lossy()
}

fn good_option(path: &Path) -> Option<&str> {
    // Opción 3: propagar la opcionalidad
    path.to_str()
}
```

**Por qué importa**: en Linux, un nombre de archivo puede contener cualquier byte
excepto `0x00` y `/`. Un directorio llamado `\xff` es válido pero no es UTF-8.
`to_str()` retorna `None` para estos paths.

### Error 5: Usar `exists()` para decidir si crear un archivo

```rust
use std::path::Path;
use std::fs;

// MAL — TOCTOU race condition
fn bad_create(path: &str) {
    if !Path::new(path).exists() {
        // Otro proceso puede crear el archivo AQUÍ
        fs::write(path, "data").unwrap();
        // Puede sobrescribir el archivo del otro proceso
    }
}

// BIEN — usar create_new (atómico)
fn good_create(path: &str) -> std::io::Result<()> {
    use std::fs::OpenOptions;
    use std::io::Write;

    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(path)?;
    file.write_all(b"data")?;
    Ok(())
}
```

**Por qué ocurre**: entre `exists()` y `write()` hay una ventana donde otro proceso
puede actuar. Esto es un bug de TOCTOU (Time of Check to Time of Use), ya cubierto
en T01, pero reforzado aquí porque `Path::exists()` tienta a usarlo para guardia.

---

## 10. Cheatsheet

```text
──────────────────── Crear ───────────────────────────
Path::new("/home/user")             &Path desde &str
PathBuf::from("/home/user")         PathBuf owned
PathBuf::new()                      PathBuf vacío

──────────────────── Construir ───────────────────────
path.join("subdir")                 Nuevo PathBuf (no modifica)
pathbuf.push("subdir")              Modifica in-place
pathbuf.pop()                       Quitar último componente
pathbuf.set_file_name("new.txt")    Cambiar nombre de archivo
pathbuf.set_extension("md")         Cambiar extensión
path.with_file_name("new.txt")      Nuevo Path con otro nombre
path.with_extension("md")           Nuevo Path con otra extensión

──────────────────── Descomponer ─────────────────────
path.parent()                       Option<&Path> (directorio padre)
path.file_name()                    Option<&OsStr> (nombre completo)
path.file_stem()                    Option<&OsStr> (nombre sin ext)
path.extension()                    Option<&OsStr> (extensión)
path.components()                   Iterador de Component
path.ancestors()                    Iterador de &Path (padres)
path.strip_prefix(base)             Result<&Path> (path relativo)

──────────────────── Consultas (sin filesystem) ──────
path.is_absolute()                  Empieza con /  (o C:\ en Win)
path.is_relative()                  No es absoluto
path.starts_with("/home")           Prefijo de componentes
path.ends_with("file.txt")          Sufijo de componentes
path.has_root()                     Tiene componente raíz
MAIN_SEPARATOR                      char: '/' o '\\'

──────────────────── Consultas (con filesystem) ──────
path.exists()                       ¿Existe? (sigue symlinks)
path.try_exists()                   Result<bool> (distingue errores)
path.is_file()                      ¿Es archivo regular?
path.is_dir()                       ¿Es directorio?
path.is_symlink()                   ¿Es symlink?
path.metadata()                     Result<Metadata>
path.symlink_metadata()             Metadata sin seguir symlinks
path.canonicalize()                 Path absoluto real (resuelve todo)
path.read_dir()                     Iterador de DirEntry

──────────────────── Conversiones ────────────────────
path.to_str()                       Option<&str> (puede fallar)
path.to_string_lossy()              Cow<str> (siempre funciona, '�')
path.display()                      impl Display (para println!)
path.as_os_str()                    &OsStr
pathbuf.as_path()                   &Path (Deref)
pathbuf.into_os_string()            OsString (consume)
Path::new(s)                        &str/&OsStr → &Path
PathBuf::from(s)                    String/OsString → PathBuf

──────────────────── En funciones ────────────────────
fn f(p: impl AsRef<Path>)           Acepta &str/String/Path/PathBuf
fn f(p: &Path)                      Solo &Path (más restrictivo)
fn f() -> PathBuf                   Retornar paths owned
```

---

## 11. Ejercicios

### Ejercicio 1: Normalizador de paths

Implementa una función que normalice paths sin tocar el filesystem:

```rust
use std::path::{Path, PathBuf, Component};

fn normalize(path: &Path) -> PathBuf {
    // TODO: Resolver . y .. sin acceder al filesystem
    // Ejemplos:
    //   "/a/b/../c/./d"    → "/a/c/d"
    //   "a/b/../../c"      → "c"
    //   "../a/b/../c"      → "../a/c"
    //   "/a/b/c/../../.."  → "/"
    //   "."                → ""
    //   ".."               → ".."
    //
    // Reglas:
    // - CurDir (.) se ignora siempre
    // - ParentDir (..) hace pop() si hay componentes normales
    // - Si no hay componentes que popear (estamos en la raíz o
    //   ya hay .. acumulados), mantener el ..
    // - Preservar si es absoluto o relativo

    todo!()
}

fn main() {
    let tests = [
        ("/a/b/../c/./d", "/a/c/d"),
        ("a/b/../../c", "c"),
        ("../a/b/../c", "../a/c"),
        ("/a/b/c/../../..", "/"),
        (".", ""),
        ("..", ".."),
        ("a/../../b", "../b"),
    ];

    for (input, expected) in &tests {
        let result = normalize(Path::new(input));
        let expected = PathBuf::from(expected);
        println!("{:<25} → {:<15} (expected: {})",
            input, result.display(), expected.display());
        assert_eq!(result, expected, "Failed for input: {}", input);
    }
    println!("All tests passed!");
}
```

**Pregunta de reflexión**: ¿por qué `canonicalize()` requiere que el path exista pero
la normalización "lógica" no? ¿En qué caso la normalización lógica daría un resultado
diferente al de `canonicalize()`? (Hint: piensa en symlinks que apuntan a directorios
en otro nivel).

### Ejercicio 2: Buscador de archivos por extensión

Implementa una función que busque archivos por extensión en un directorio (no recursivo):

```rust
use std::path::{Path, PathBuf};
use std::fs;

fn find_by_extension(dir: &Path, ext: &str) -> std::io::Result<Vec<PathBuf>> {
    // TODO:
    // 1. Listar el directorio con read_dir
    // 2. Filtrar solo archivos regulares (is_file())
    // 3. Filtrar por extensión (path.extension() == Some(ext))
    // 4. Retornar los paths ordenados alfabéticamente
    // 5. Manejar errores de entries individuales sin abortar

    todo!()
}

fn main() -> std::io::Result<()> {
    // Crear archivos de prueba
    let test_dir = Path::new("/tmp/path_test");
    fs::create_dir_all(test_dir)?;

    for name in &["a.txt", "b.txt", "c.rs", "d.rs", "e.md", "readme"] {
        fs::write(test_dir.join(name), "test")?;
    }
    fs::create_dir(test_dir.join("subdir.txt"))?; // Dir con extensión .txt

    let txt_files = find_by_extension(test_dir, "txt")?;
    println!("TXT files: {:?}", txt_files);
    assert_eq!(txt_files.len(), 2); // a.txt, b.txt (no subdir.txt)

    let rs_files = find_by_extension(test_dir, "rs")?;
    println!("RS files: {:?}", rs_files);
    assert_eq!(rs_files.len(), 2); // c.rs, d.rs

    let no_ext = find_by_extension(test_dir, "")?;
    println!("No extension: {:?}", no_ext);

    // Limpiar
    fs::remove_dir_all(test_dir)?;
    Ok(())
}
```

**Pregunta de reflexión**: `path.extension()` retorna `Option<&OsStr>`, no `Option<&str>`.
¿Por qué? ¿En qué situación una extensión de archivo podría no ser UTF-8 válido en Linux?
¿Cómo afecta esto a la comparación `extension == Some("txt")`?

### Ejercicio 3: Generador de paths únicos

Implementa una función que genere un nombre de archivo que no exista, añadiendo un
sufijo numérico si es necesario:

```rust
use std::path::{Path, PathBuf};
use std::fs;

fn unique_path(base: &Path) -> PathBuf {
    // TODO:
    // Si base no existe → retornar base tal cual
    // Si base existe → probar base_1.ext, base_2.ext, etc.
    //
    // Ejemplo: si "report.txt" existe:
    //   Probar "report_1.txt" → existe
    //   Probar "report_2.txt" → no existe → retornar
    //
    // Si el archivo no tiene extensión:
    //   "readme" → "readme_1" → "readme_2" → ...
    //
    // Límite de 10000 intentos para evitar loop infinito

    todo!()
}

fn main() -> std::io::Result<()> {
    let test_dir = Path::new("/tmp/unique_test");
    fs::create_dir_all(test_dir)?;

    // Primer archivo: report.txt (no existe, lo crea directo)
    let path = unique_path(&test_dir.join("report.txt"));
    println!("1st: {}", path.display()); // report.txt
    fs::write(&path, "v1")?;

    // Segundo: report.txt existe → report_1.txt
    let path = unique_path(&test_dir.join("report.txt"));
    println!("2nd: {}", path.display()); // report_1.txt
    fs::write(&path, "v2")?;

    // Tercero: ambos existen → report_2.txt
    let path = unique_path(&test_dir.join("report.txt"));
    println!("3rd: {}", path.display()); // report_2.txt

    // Sin extensión
    fs::write(test_dir.join("readme"), "data")?;
    let path = unique_path(&test_dir.join("readme"));
    println!("No ext: {}", path.display()); // readme_1

    // Limpiar
    fs::remove_dir_all(test_dir)?;
    Ok(())
}
```

**Pregunta de reflexión**: esta implementación tiene un TOCTOU bug — entre verificar que
`report_2.txt` no existe y crearlo, otro proceso podría crearlo. ¿Cómo usarías
`OpenOptions::create_new(true)` para hacer la operación atómica? ¿Cambia la firma de la
función?