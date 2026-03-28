# Permisos y Metadata

## Índice

1. [std::fs::Metadata — Información del archivo](#1-stdfsmetadata--información-del-archivo)
2. [Permisos en Unix](#2-permisos-en-unix)
3. [Leer permisos en Rust](#3-leer-permisos-en-rust)
4. [Modificar permisos](#4-modificar-permisos)
5. [Timestamps: acceso, modificación, creación](#5-timestamps-acceso-modificación-creación)
6. [MetadataExt — Información Unix extendida](#6-metadataext--información-unix-extendida)
7. [Ownership: UID y GID](#7-ownership-uid-y-gid)
8. [Patrones prácticos](#8-patrones-prácticos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. `std::fs::Metadata` — Información del archivo

`Metadata` es el resultado de la syscall `stat()`. Contiene toda la información que el
filesystem almacena sobre un archivo: tipo, tamaño, permisos, timestamps, y en Unix,
inode, device, links, etc.

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    // Dos formas de obtener metadata:
    // 1. fs::metadata() — sigue symlinks (usa stat())
    let meta = fs::metadata("/etc/passwd")?;

    // 2. fs::symlink_metadata() — NO sigue symlinks (usa lstat())
    let link_meta = fs::symlink_metadata("/etc/passwd")?;

    // 3. Desde un File abierto (usa fstat())
    let file = fs::File::open("/etc/passwd")?;
    let file_meta = file.metadata()?;

    println!("Size:     {} bytes", meta.len());
    println!("Is file:  {}", meta.is_file());
    println!("Is dir:   {}", meta.is_dir());
    println!("Is link:  {}", meta.is_symlink());
    println!("Readonly: {}", meta.permissions().readonly());

    Ok(())
}
```

```
Relación entre funciones y syscalls:

  fs::metadata(path)          → stat(path)     Sigue symlinks
  fs::symlink_metadata(path)  → lstat(path)    No sigue symlinks
  file.metadata()             → fstat(fd)      Desde file descriptor

  stat()  en symlink → info del TARGET
  lstat() en symlink → info del LINK mismo
  fstat() en fd      → info del archivo abierto (siempre el target)
```

### 1.1 Qué contiene `Metadata` (portable)

```
┌─────────────────────────────────────────────────────────────┐
│  Metadata (cross-platform)                                   │
├──────────────────┬──────────────────────────────────────────┤
│  len()           │  Tamaño en bytes (u64)                   │
│  is_file()       │  ¿Es archivo regular?                    │
│  is_dir()        │  ¿Es directorio?                         │
│  is_symlink()    │  ¿Es symlink? (solo con symlink_metadata)│
│  file_type()     │  FileType (file/dir/symlink)             │
│  permissions()   │  Permissions                              │
│  modified()      │  SystemTime de última modificación        │
│  accessed()      │  SystemTime de último acceso              │
│  created()       │  SystemTime de creación (si disponible)   │
└──────────────────┴──────────────────────────────────────────┘
```

### 1.2 `FileType` — Tipo de archivo

```rust
use std::fs;

fn describe_type(path: &str) -> std::io::Result<&'static str> {
    let ft = fs::symlink_metadata(path)?.file_type();

    Ok(if ft.is_file() {
        "regular file"
    } else if ft.is_dir() {
        "directory"
    } else if ft.is_symlink() {
        "symbolic link"
    } else {
        // En Unix, otros tipos: socket, fifo, block/char device
        "other (socket, fifo, device...)"
    })
}

fn main() -> std::io::Result<()> {
    for path in &["/etc/passwd", "/etc", "/dev/null", "/dev/stdin"] {
        match describe_type(path) {
            Ok(desc) => println!("{}: {}", path, desc),
            Err(e) => println!("{}: error: {}", path, e),
        }
    }
    Ok(())
}
```

---

## 2. Permisos en Unix

Recordatorio del sistema de permisos Unix (cubierto en detalle en bloques de Linux):

```
Bits de permisos (modo octal):

  ┌─ Tipo ─┐┌── Owner ──┐┌── Group ──┐┌── Others ─┐
  │  d/-/l ││  r  w  x  ││  r  w  x  ││  r  w  x  │
  └────────┘└───────────┘└───────────┘└───────────┘
               4  2  1      4  2  1      4  2  1

  0o755  =  rwxr-xr-x   (típico ejecutable/directorio)
  0o644  =  rw-r--r--   (típico archivo regular)
  0o600  =  rw-------   (archivo privado)
  0o700  =  rwx------   (directorio privado)
  0o777  =  rwxrwxrwx   (todo para todos)
  0o400  =  r--------   (solo lectura para owner)

Bits especiales:
  0o4000 = setuid (s en owner x)
  0o2000 = setgid (s en group x)
  0o1000 = sticky bit (t en others x)
```

```
Permisos comunes por tipo de archivo:

  Archivo de texto     0o644  rw-r--r--
  Script ejecutable    0o755  rwxr-xr-x
  Clave privada        0o600  rw-------
  Directorio público   0o755  rwxr-xr-x
  Directorio privado   0o700  rwx------
  Archivo de log       0o640  rw-r-----
  Socket               0o660  rw-rw----
```

---

## 3. Leer permisos en Rust

### 3.1 API portable: `Permissions::readonly()`

La API portable de `std::fs::Permissions` solo expone un método: `readonly()`.
Esto es porque Windows no tiene un modelo de permisos octal como Unix.

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    let meta = fs::metadata("/etc/passwd")?;
    let perms = meta.permissions();

    println!("Readonly: {}", perms.readonly());
    // true si NADIE puede escribir (todos los write bits = 0)

    Ok(())
}
```

### 3.2 API Unix: `PermissionsExt::mode()`

Para acceder al modo octal completo, necesitas el trait `PermissionsExt`:

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

fn main() -> std::io::Result<()> {
    let meta = fs::metadata("/etc/passwd")?;
    let perms = meta.permissions();

    // mode() retorna u32 con los bits de permisos
    let mode = perms.mode();

    println!("Mode (octal): {:o}", mode);   // 644
    println!("Mode (bits):  {:012b}", mode); // 110100100

    // Extraer permisos individuales
    let owner_read    = mode & 0o400 != 0;
    let owner_write   = mode & 0o200 != 0;
    let owner_execute = mode & 0o100 != 0;
    let group_read    = mode & 0o040 != 0;
    let group_write   = mode & 0o020 != 0;
    let group_execute = mode & 0o010 != 0;
    let other_read    = mode & 0o004 != 0;
    let other_write   = mode & 0o002 != 0;
    let other_execute = mode & 0o001 != 0;

    println!("Owner: {}{}{}",
        if owner_read { 'r' } else { '-' },
        if owner_write { 'w' } else { '-' },
        if owner_execute { 'x' } else { '-' },
    );
    println!("Group: {}{}{}",
        if group_read { 'r' } else { '-' },
        if group_write { 'w' } else { '-' },
        if group_execute { 'x' } else { '-' },
    );
    println!("Other: {}{}{}",
        if other_read { 'r' } else { '-' },
        if other_write { 'w' } else { '-' },
        if other_execute { 'x' } else { '-' },
    );

    Ok(())
}
```

### 3.3 Formatear permisos como `ls -l`

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

fn format_permissions(mode: u32) -> String {
    let mut s = String::with_capacity(10);

    // Tipo de archivo (bits 12-15)
    let file_type = mode & 0o170000;
    s.push(match file_type {
        0o140000 => 's', // socket
        0o120000 => 'l', // symlink
        0o100000 => '-', // regular file
        0o060000 => 'b', // block device
        0o040000 => 'd', // directory
        0o020000 => 'c', // char device
        0o010000 => 'p', // FIFO/pipe
        _ => '?',
    });

    // Owner
    s.push(if mode & 0o400 != 0 { 'r' } else { '-' });
    s.push(if mode & 0o200 != 0 { 'w' } else { '-' });
    s.push(if mode & 0o4000 != 0 {
        if mode & 0o100 != 0 { 's' } else { 'S' }
    } else {
        if mode & 0o100 != 0 { 'x' } else { '-' }
    });

    // Group
    s.push(if mode & 0o040 != 0 { 'r' } else { '-' });
    s.push(if mode & 0o020 != 0 { 'w' } else { '-' });
    s.push(if mode & 0o2000 != 0 {
        if mode & 0o010 != 0 { 's' } else { 'S' }
    } else {
        if mode & 0o010 != 0 { 'x' } else { '-' }
    });

    // Others
    s.push(if mode & 0o004 != 0 { 'r' } else { '-' });
    s.push(if mode & 0o002 != 0 { 'w' } else { '-' });
    s.push(if mode & 0o1000 != 0 {
        if mode & 0o001 != 0 { 't' } else { 'T' }
    } else {
        if mode & 0o001 != 0 { 'x' } else { '-' }
    });

    s
}

fn main() -> std::io::Result<()> {
    for path in &["/etc/passwd", "/etc", "/dev/null", "/tmp"] {
        let meta = fs::symlink_metadata(path)?;
        let mode = meta.permissions().mode();
        println!("{} {}", format_permissions(mode), path);
    }
    Ok(())
}
```

Salida:
```
-rw-r--r-- /etc/passwd
drwxr-xr-x /etc
crw-rw-rw- /dev/null
drwxrwxrwt /tmp
```

### 3.4 Verificar permisos específicos

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

fn is_executable(path: &str) -> std::io::Result<bool> {
    let mode = fs::metadata(path)?.permissions().mode();
    // Verificar si ALGUIEN puede ejecutar (owner, group, o others)
    Ok(mode & 0o111 != 0)
}

fn is_world_writable(path: &str) -> std::io::Result<bool> {
    let mode = fs::metadata(path)?.permissions().mode();
    Ok(mode & 0o002 != 0) // Others write bit
}

fn is_setuid(path: &str) -> std::io::Result<bool> {
    let mode = fs::metadata(path)?.permissions().mode();
    Ok(mode & 0o4000 != 0)
}

fn main() -> std::io::Result<()> {
    println!("/usr/bin/passwd executable: {}", is_executable("/usr/bin/passwd")?);
    println!("/tmp world-writable: {}", is_world_writable("/tmp")?);
    println!("/usr/bin/passwd setuid: {}", is_setuid("/usr/bin/passwd")?);
    Ok(())
}
```

---

## 4. Modificar permisos

### 4.1 `fs::set_permissions` — API portable

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    fs::write("test_perms.txt", "data")?;

    // Hacer read-only (portable)
    let mut perms = fs::metadata("test_perms.txt")?.permissions();
    perms.set_readonly(true);
    fs::set_permissions("test_perms.txt", perms)?;

    // Verificar
    let meta = fs::metadata("test_perms.txt")?;
    println!("Readonly: {}", meta.permissions().readonly()); // true

    // Intentar escribir falla
    match fs::write("test_perms.txt", "new data") {
        Ok(()) => println!("Write succeeded (unexpected)"),
        Err(e) => println!("Write failed: {} (expected)", e),
    }

    // Quitar read-only para poder limpiar
    let mut perms = fs::metadata("test_perms.txt")?.permissions();
    perms.set_readonly(false);
    fs::set_permissions("test_perms.txt", perms)?;

    fs::remove_file("test_perms.txt")?;
    Ok(())
}
```

### 4.2 `PermissionsExt::set_mode` — Modo octal Unix

```rust
use std::fs::{self, Permissions};
use std::os::unix::fs::PermissionsExt;

fn main() -> std::io::Result<()> {
    fs::write("script.sh", "#!/bin/bash\necho hello")?;

    // Hacer ejecutable (0o755 = rwxr-xr-x)
    fs::set_permissions("script.sh", Permissions::from_mode(0o755))?;

    // Verificar
    let mode = fs::metadata("script.sh")?.permissions().mode();
    println!("Mode: {:o}", mode & 0o7777); // 755

    // Archivo privado (0o600 = rw-------)
    fs::write("secret.key", "private key data")?;
    fs::set_permissions("secret.key", Permissions::from_mode(0o600))?;

    let mode = fs::metadata("secret.key")?.permissions().mode();
    println!("Mode: {:o}", mode & 0o7777); // 600

    // Limpiar
    fs::remove_file("script.sh")?;
    fs::remove_file("secret.key")?;
    Ok(())
}
```

### 4.3 Modificar bits específicos sin reemplazar todo

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

fn add_permission(path: &str, bits: u32) -> std::io::Result<()> {
    let meta = fs::metadata(path)?;
    let current = meta.permissions().mode();
    let new_mode = current | bits; // OR para añadir bits
    fs::set_permissions(path, fs::Permissions::from_mode(new_mode))?;
    Ok(())
}

fn remove_permission(path: &str, bits: u32) -> std::io::Result<()> {
    let meta = fs::metadata(path)?;
    let current = meta.permissions().mode();
    let new_mode = current & !bits; // AND NOT para quitar bits
    fs::set_permissions(path, fs::Permissions::from_mode(new_mode))?;
    Ok(())
}

fn main() -> std::io::Result<()> {
    fs::write("demo.txt", "test")?;

    // Equivalente a chmod +x demo.txt
    add_permission("demo.txt", 0o111)?;

    let mode = fs::metadata("demo.txt")?.permissions().mode();
    println!("After +x: {:o}", mode & 0o7777); // 755 (si era 644 → 755)

    // Equivalente a chmod -w demo.txt (quitar write para todos)
    remove_permission("demo.txt", 0o222)?;

    let mode = fs::metadata("demo.txt")?.permissions().mode();
    println!("After -w: {:o}", mode & 0o7777); // 555

    fs::set_permissions("demo.txt", fs::Permissions::from_mode(0o644))?;
    fs::remove_file("demo.txt")?;
    Ok(())
}
```

```
Operaciones con bits:

Añadir execute:    current | 0o111     (chmod +x)
Quitar write:      current & !0o222    (chmod -w)
Solo owner rwx:    current & 0o700     (quitar group y others)
Añadir group read: current | 0o040    (chmod g+r)

Ejemplo:
  current = 0o644  (rw-r--r--)
  | 0o111          (añadir x para todos)
  = 0o755          (rwxr-xr-x)

  current = 0o755  (rwxr-xr-x)
  & !0o022         (quitar write de group y others)
  = 0o755 & 0o7755 = 0o755  (ya no tenía write en g/o)
```

### 4.4 Permisos al crear archivos

```rust
use std::fs::OpenOptions;
use std::os::unix::fs::OpenOptionsExt;
use std::io::Write;

fn main() -> std::io::Result<()> {
    // Crear archivo con permisos específicos desde el inicio
    // Esto es más seguro que crear y luego chmod (evita ventana TOCTOU)
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600) // rw------- desde la creación
        .open("created_private.txt")?;

    file.write_all(b"secret data")?;

    // Crear directorio con permisos específicos
    // std::fs::create_dir NO acepta modo directamente
    // Usar DirBuilder en su lugar:
    std::fs::DirBuilder::new()
        .mode(0o700) // rwx------
        .create("private_dir")?;

    // Verificar
    use std::os::unix::fs::PermissionsExt;
    let mode = std::fs::metadata("private_dir")?.permissions().mode();
    println!("Dir mode: {:o}", mode & 0o7777);

    // Limpiar
    std::fs::remove_file("created_private.txt")?;
    std::fs::remove_dir("private_dir")?;
    Ok(())
}
```

> **Nota**: `umask` del proceso afecta los permisos finales. Si `umask = 0o022` y
> creas con `mode(0o777)`, el resultado real es `0o755` (777 & ~022). Para permisos
> exactos, necesitas cambiar el umask temporalmente o usar `fchmod` después de crear.

---

## 5. Timestamps: acceso, modificación, creación

### 5.1 Los tres timestamps Unix

```
┌────────────┬────────────────────────────────────────────┐
│ Timestamp  │ Significado                                │
├────────────┼────────────────────────────────────────────┤
│ mtime      │ Última MODIFICACIÓN del contenido          │
│ (modified) │ Cambia al escribir datos                   │
│            │ ls -l muestra este por defecto             │
├────────────┼────────────────────────────────────────────┤
│ atime      │ Último ACCESO (lectura)                    │
│ (accessed) │ Cambia al leer el archivo                  │
│            │ Muchos FS lo desactivan (noatime, relatime)│
├────────────┼────────────────────────────────────────────┤
│ ctime      │ Último cambio de METADATA (inode)          │
│ (created*) │ Cambia al chmod, chown, rename, write      │
│            │ NO es "creation time" en Unix clásico       │
├────────────┼────────────────────────────────────────────┤
│ btime      │ BIRTH time (creación real)                 │
│            │ Solo en algunos FS (ext4, btrfs, APFS)     │
│            │ No todos los kernels lo exponen             │
└────────────┴────────────────────────────────────────────┘

*Nota: Rust llama created() al birth time, NO al ctime Unix.
 En sistemas sin birth time, created() retorna Err.
```

### 5.2 Leer timestamps

```rust
use std::fs;
use std::time::{SystemTime, UNIX_EPOCH};

fn format_time(time: SystemTime) -> String {
    match time.duration_since(UNIX_EPOCH) {
        Ok(duration) => {
            let secs = duration.as_secs();
            // Formato simplificado (para formato real, usar chrono crate)
            let hours = (secs % 86400) / 3600;
            let mins = (secs % 3600) / 60;
            let days_since_epoch = secs / 86400;
            format!("epoch+{}d {:02}:{:02}", days_since_epoch, hours, mins)
        }
        Err(_) => "before epoch".into(),
    }
}

fn main() -> std::io::Result<()> {
    let meta = fs::metadata("/etc/passwd")?;

    // modified() — siempre disponible en Unix
    if let Ok(mtime) = meta.modified() {
        println!("Modified: {}", format_time(mtime));
    }

    // accessed() — puede no actualizarse (noatime mount)
    if let Ok(atime) = meta.accessed() {
        println!("Accessed: {}", format_time(atime));
    }

    // created() — puede no estar disponible
    match meta.created() {
        Ok(ctime) => println!("Created:  {}", format_time(ctime)),
        Err(e) => println!("Created:  not available ({})", e),
    }

    Ok(())
}
```

### 5.3 Timestamps con `MetadataExt` (precisión Unix)

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn main() -> std::io::Result<()> {
    let meta = fs::metadata("/etc/passwd")?;

    // Segundos desde epoch (i64)
    println!("mtime: {} seconds since epoch", meta.mtime());
    println!("atime: {} seconds since epoch", meta.atime());
    println!("ctime: {} seconds since epoch", meta.ctime());

    // Nanosegundos (parte fraccionaria)
    println!("mtime_nsec: {} ns", meta.mtime_nsec());

    // Timestamp completo con precisión nanosegundo:
    let mtime_ns = meta.mtime() as u128 * 1_000_000_000
                   + meta.mtime_nsec() as u128;
    println!("mtime total: {} ns since epoch", mtime_ns);

    Ok(())
}
```

### 5.4 Comparar timestamps

```rust
use std::fs;
use std::path::Path;

fn is_newer_than(file_a: &str, file_b: &str) -> std::io::Result<bool> {
    let mtime_a = fs::metadata(file_a)?.modified()?;
    let mtime_b = fs::metadata(file_b)?.modified()?;
    Ok(mtime_a > mtime_b)
}

fn needs_rebuild(source: &str, output: &str) -> std::io::Result<bool> {
    // Si el output no existe, necesita rebuild
    let output_meta = match fs::metadata(output) {
        Ok(m) => m,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(true),
        Err(e) => return Err(e),
    };

    let source_mtime = fs::metadata(source)?.modified()?;
    let output_mtime = output_meta.modified()?;

    // Rebuild si el source es más nuevo que el output
    Ok(source_mtime > output_mtime)
}

fn main() -> std::io::Result<()> {
    if needs_rebuild("src/main.rs", "target/debug/myapp")? {
        println!("Source changed, rebuild needed");
    } else {
        println!("Output is up to date");
    }
    Ok(())
}
```

### 5.5 Modificar timestamps con `filetime` crate

La biblioteca estándar no ofrece API para **modificar** timestamps. La crate `filetime`
llena ese vacío:

```rust
// Cargo.toml: filetime = "0.2"

// use filetime::{FileTime, set_file_mtime, set_file_atime, set_file_times};
//
// // Establecer mtime a "ahora"
// let now = FileTime::now();
// set_file_mtime("file.txt", now)?;
//
// // Establecer ambos timestamps
// set_file_times("file.txt", atime, mtime)?;
//
// // Copiar timestamps de un archivo a otro
// let meta = fs::metadata("source.txt")?;
// let atime = FileTime::from_last_access_time(&meta);
// let mtime = FileTime::from_last_modification_time(&meta);
// set_file_times("dest.txt", atime, mtime)?;
//
// // Timestamp específico
// let specific = FileTime::from_unix_time(1700000000, 0);
// set_file_mtime("file.txt", specific)?;
```

Sin crate externa, puedes usar la syscall `utimensat` directamente via `libc`:

```rust
use std::ffi::CString;

fn touch(path: &str) -> std::io::Result<()> {
    let c_path = CString::new(path).map_err(|_| {
        std::io::Error::new(std::io::ErrorKind::InvalidInput, "nul in path")
    })?;

    // utimensat con UTIME_NOW actualiza ambos timestamps a "ahora"
    let times = [
        libc::timespec { tv_sec: 0, tv_nsec: libc::UTIME_NOW },
        libc::timespec { tv_sec: 0, tv_nsec: libc::UTIME_NOW },
    ];

    let ret = unsafe {
        libc::utimensat(
            libc::AT_FDCWD,
            c_path.as_ptr(),
            times.as_ptr(),
            0,
        )
    };

    if ret == 0 {
        Ok(())
    } else {
        Err(std::io::Error::last_os_error())
    }
}
```

---

## 6. `MetadataExt` — Información Unix extendida

El trait `std::os::unix::fs::MetadataExt` expone todos los campos de `struct stat`:

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn full_stat(path: &str) -> std::io::Result<()> {
    let meta = fs::metadata(path)?;

    println!("=== stat({}) ===", path);

    // Device y inode
    println!("Device:     {} (major={}, minor={})",
        meta.dev(),
        // major/minor desde dev_t
        (meta.dev() >> 8) & 0xff,
        meta.dev() & 0xff,
    );
    println!("Inode:      {}", meta.ino());

    // Tipo y permisos (mode incluye ambos)
    println!("Mode:       {:o} (octal)", meta.mode());

    // Links
    println!("Hard links: {}", meta.nlink());

    // Ownership
    println!("UID:        {}", meta.uid());
    println!("GID:        {}", meta.gid());

    // Tamaño
    println!("Size:       {} bytes", meta.size());

    // Bloques en disco (unidades de 512 bytes)
    println!("Blocks:     {} (= {} bytes on disk)",
        meta.blocks(),
        meta.blocks() * 512,
    );
    println!("Block size: {} (preferred I/O)", meta.blksize());

    // Device especial (solo para block/char devices)
    println!("rdev:       {}", meta.rdev());

    // Timestamps
    println!("atime:      {}.{:09}", meta.atime(), meta.atime_nsec());
    println!("mtime:      {}.{:09}", meta.mtime(), meta.mtime_nsec());
    println!("ctime:      {}.{:09}", meta.ctime(), meta.ctime_nsec());

    Ok(())
}

fn main() -> std::io::Result<()> {
    full_stat("/etc/passwd")?;
    println!();
    full_stat("/dev/null")?;
    Ok(())
}
```

### 6.1 Detectar el tipo real de archivo con `mode()`

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn file_type_from_mode(mode: u32) -> &'static str {
    match mode & 0o170000 {
        0o140000 => "socket",
        0o120000 => "symbolic link",
        0o100000 => "regular file",
        0o060000 => "block device",
        0o040000 => "directory",
        0o020000 => "character device",
        0o010000 => "FIFO (named pipe)",
        _ => "unknown",
    }
}

fn main() -> std::io::Result<()> {
    for path in &["/etc/passwd", "/dev/null", "/dev/sda", "/tmp", "/run/systemd/notify"] {
        match fs::symlink_metadata(path) {
            Ok(meta) => {
                let ft = file_type_from_mode(meta.mode());
                println!("{}: {}", path, ft);
            }
            Err(e) => println!("{}: {}", path, e),
        }
    }
    Ok(())
}
```

### 6.2 Detectar sparse files

Un sparse file tiene bloques "vacíos" que no ocupan espacio en disco. Su tamaño
reportado (`len()`) es mayor que su uso real (`blocks() * 512`):

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn is_sparse(path: &str) -> std::io::Result<bool> {
    let meta = fs::metadata(path)?;
    let apparent_size = meta.size();
    let disk_size = meta.blocks() * 512;
    Ok(disk_size < apparent_size)
}

fn show_sizes(path: &str) -> std::io::Result<()> {
    let meta = fs::metadata(path)?;
    let apparent = meta.size();
    let on_disk = meta.blocks() * 512;

    println!("{}", path);
    println!("  Apparent size: {} bytes", apparent);
    println!("  On disk:       {} bytes", on_disk);
    if on_disk < apparent {
        let saved = apparent - on_disk;
        println!("  Sparse: YES (saved {} bytes, {:.1}%)",
            saved, (saved as f64 / apparent as f64) * 100.0);
    }

    Ok(())
}
```

### 6.3 Comparar archivos por inode

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn same_file(path_a: &str, path_b: &str) -> std::io::Result<bool> {
    let meta_a = fs::metadata(path_a)?;
    let meta_b = fs::metadata(path_b)?;

    // Mismo archivo si mismo device Y mismo inode
    Ok(meta_a.dev() == meta_b.dev() && meta_a.ino() == meta_b.ino())
}

fn main() -> std::io::Result<()> {
    // Crear hard link para probar
    fs::write("original.txt", "data")?;
    fs::hard_link("original.txt", "hardlink.txt")?;

    println!("Same file: {}", same_file("original.txt", "hardlink.txt")?);
    // true — mismo inode

    fs::write("copy.txt", "data")?;
    println!("Same file: {}", same_file("original.txt", "copy.txt")?);
    // false — diferente inode (aunque mismo contenido)

    // Limpiar
    fs::remove_file("original.txt")?;
    fs::remove_file("hardlink.txt")?;
    fs::remove_file("copy.txt")?;
    Ok(())
}
```

---

## 7. Ownership: UID y GID

### 7.1 Leer owner y group

```rust
use std::fs;
use std::os::unix::fs::MetadataExt;

fn main() -> std::io::Result<()> {
    let meta = fs::metadata("/etc/passwd")?;

    let uid = meta.uid();
    let gid = meta.gid();

    println!("UID: {} GID: {}", uid, gid);
    // Típico: UID=0 GID=0 (root:root)

    Ok(())
}
```

### 7.2 Resolver UID/GID a nombres

La biblioteca estándar no incluye resolución de nombres. Puedes usar la crate `nix`
o leer `/etc/passwd` y `/etc/group` directamente:

```rust
use std::fs;
use std::io::{BufRead, BufReader};

fn uid_to_name(uid: u32) -> Option<String> {
    let file = fs::File::open("/etc/passwd").ok()?;
    let reader = BufReader::new(file);

    for line in reader.lines() {
        let line = line.ok()?;
        let fields: Vec<&str> = line.split(':').collect();
        if fields.len() >= 3 {
            if let Ok(file_uid) = fields[2].parse::<u32>() {
                if file_uid == uid {
                    return Some(fields[0].to_string());
                }
            }
        }
    }
    None
}

fn gid_to_name(gid: u32) -> Option<String> {
    let file = fs::File::open("/etc/group").ok()?;
    let reader = BufReader::new(file);

    for line in reader.lines() {
        let line = line.ok()?;
        let fields: Vec<&str> = line.split(':').collect();
        if fields.len() >= 3 {
            if let Ok(file_gid) = fields[2].parse::<u32>() {
                if file_gid == gid {
                    return Some(fields[0].to_string());
                }
            }
        }
    }
    None
}

fn main() -> std::io::Result<()> {
    use std::os::unix::fs::MetadataExt;

    let meta = fs::metadata("/etc/passwd")?;
    let uid = meta.uid();
    let gid = meta.gid();

    let user = uid_to_name(uid).unwrap_or_else(|| uid.to_string());
    let group = gid_to_name(gid).unwrap_or_else(|| gid.to_string());

    println!("/etc/passwd owned by {}:{} ({}:{})", user, group, uid, gid);
    Ok(())
}
```

### 7.3 Cambiar ownership con `chown` (requiere root)

```rust
use std::ffi::CString;

fn chown(path: &str, uid: u32, gid: u32) -> std::io::Result<()> {
    let c_path = CString::new(path).map_err(|_| {
        std::io::Error::new(std::io::ErrorKind::InvalidInput, "nul in path")
    })?;

    let ret = unsafe {
        libc::chown(c_path.as_ptr(), uid, gid)
    };

    if ret == 0 {
        Ok(())
    } else {
        Err(std::io::Error::last_os_error())
    }
}

// Uso: chown("file.txt", 1000, 1000)?;
// Requiere ser root o ser el owner actual del archivo
```

> **Nota**: la crate `nix` ofrece wrappers seguros para `chown`, `fchown`, etc.
> En producción, prefiere `nix::unistd::chown` sobre el FFI directo.

---

## 8. Patrones prácticos

### 8.1 `ls -la` simplificado

```rust
use std::fs;
use std::os::unix::fs::{MetadataExt, PermissionsExt};

fn format_permissions(mode: u32) -> String {
    let mut s = String::with_capacity(10);
    let ft = mode & 0o170000;
    s.push(match ft {
        0o040000 => 'd',
        0o120000 => 'l',
        0o100000 => '-',
        0o020000 => 'c',
        0o060000 => 'b',
        0o010000 => 'p',
        0o140000 => 's',
        _ => '?',
    });
    for &(bit, ch) in &[
        (0o400, 'r'), (0o200, 'w'), (0o100, 'x'),
        (0o040, 'r'), (0o020, 'w'), (0o010, 'x'),
        (0o004, 'r'), (0o002, 'w'), (0o001, 'x'),
    ] {
        s.push(if mode & bit != 0 { ch } else { '-' });
    }
    s
}

fn list_directory(path: &str) -> std::io::Result<()> {
    let mut entries: Vec<_> = fs::read_dir(path)?
        .filter_map(|e| e.ok())
        .collect();

    entries.sort_by_key(|e| e.file_name());

    for entry in &entries {
        let meta = match entry.metadata() {
            Ok(m) => m,
            Err(_) => continue,
        };

        let mode = meta.permissions().mode();
        let nlink = meta.nlink();
        let uid = meta.uid();
        let gid = meta.gid();
        let size = meta.len();
        let name = entry.file_name();

        println!("{} {:>3} {:>5} {:>5} {:>8} {}",
            format_permissions(mode),
            nlink,
            uid,
            gid,
            size,
            name.to_string_lossy(),
        );
    }
    Ok(())
}

fn main() -> std::io::Result<()> {
    let dir = std::env::args().nth(1).unwrap_or_else(|| ".".into());
    list_directory(&dir)
}
```

### 8.2 Auditoría de seguridad: encontrar archivos inseguros

```rust
use walkdir::WalkDir;
use std::os::unix::fs::{MetadataExt, PermissionsExt};

fn security_audit(root: &str) {
    println!("=== Security Audit: {} ===\n", root);

    for entry in WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
    {
        let meta = match entry.metadata() {
            Ok(m) => m,
            Err(_) => continue,
        };

        let mode = meta.permissions().mode();
        let path = entry.path();

        // World-writable files (excluir /tmp y symlinks)
        if meta.is_file() && mode & 0o002 != 0 {
            println!("[WARN] World-writable file: {}", path.display());
        }

        // World-writable directories sin sticky bit
        if meta.is_dir() && mode & 0o002 != 0 && mode & 0o1000 == 0 {
            println!("[CRIT] World-writable dir without sticky: {}",
                path.display());
        }

        // Setuid/setgid binaries
        if meta.is_file() && mode & 0o6000 != 0 {
            let suid = if mode & 0o4000 != 0 { "setuid" } else { "" };
            let sgid = if mode & 0o2000 != 0 { "setgid" } else { "" };
            println!("[INFO] {} {} binary: {}", suid, sgid, path.display());
        }

        // Files owned by root but group/world writable
        if meta.uid() == 0 && mode & 0o022 != 0 {
            println!("[WARN] Root-owned but writable by others: {}",
                path.display());
        }
    }
}

fn main() {
    let root = std::env::args().nth(1).unwrap_or_else(|| "/usr/bin".into());
    security_audit(&root);
}
```

### 8.3 Preservar metadata al copiar

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

fn copy_with_metadata(src: &str, dst: &str) -> std::io::Result<()> {
    // Paso 1: copiar contenido
    fs::copy(src, dst)?;
    // fs::copy ya preserva permisos, pero NO timestamps

    // Paso 2: copiar timestamps (con filetime crate o manualmente)
    // Sin crate externa, al menos copiar permisos explícitamente:
    let src_meta = fs::metadata(src)?;
    let src_perms = src_meta.permissions();
    fs::set_permissions(dst, src_perms)?;

    // Con filetime crate:
    // let atime = FileTime::from_last_access_time(&src_meta);
    // let mtime = FileTime::from_last_modification_time(&src_meta);
    // filetime::set_file_times(dst, atime, mtime)?;

    Ok(())
}
```

---

## 9. Errores comunes

### Error 1: Mask del `mode()` — incluye bits de tipo de archivo

```rust
use std::fs;
use std::os::unix::fs::PermissionsExt;

// MAL — comparar mode() directamente incluye bits de tipo
fn bad_check(path: &str) -> std::io::Result<bool> {
    let mode = fs::metadata(path)?.permissions().mode();
    Ok(mode == 0o755)
    // false para archivos! mode() retorna 0o100755 (regular file + 755)
}

// BIEN — enmascarar para obtener solo los bits de permisos
fn good_check(path: &str) -> std::io::Result<bool> {
    let mode = fs::metadata(path)?.permissions().mode();
    Ok(mode & 0o7777 == 0o755)
    // 0o7777 extrae solo permisos (incluyendo setuid/setgid/sticky)
    // 0o777 extrae solo rwx (sin bits especiales)
}
```

**Por qué ocurre**: `mode()` retorna el campo `st_mode` completo de `stat()`, que
incluye el tipo de archivo en los bits superiores. Un archivo regular con permisos
`755` tiene `mode() = 0o100755`. Un directorio con `755` tiene `mode() = 0o040755`.
Siempre enmascarar con `& 0o7777` o `& 0o777`.

### Error 2: Confundir `ctime` con "creation time"

```rust
use std::os::unix::fs::MetadataExt;

// MAL — ctime NO es creation time
fn bad_creation_time(path: &str) -> std::io::Result<i64> {
    let meta = std::fs::metadata(path)?;
    Ok(meta.ctime()) // Esto es "change time" (último cambio de metadata)
                      // chmod, chown, rename cambian ctime
}

// BIEN — usar created() para birth time (si disponible)
fn good_creation_time(path: &str) -> std::io::Result<std::time::SystemTime> {
    let meta = std::fs::metadata(path)?;
    meta.created() // Birth time — puede retornar Err si no disponible
}
```

```
Timeline de un archivo:

  Creación  → btime (birth time, si el FS lo soporta)
  Lectura   → atime (access time, si no hay noatime)
  Escritura → mtime (modification time)
  chmod/chown → ctime (change time, NO creation!)

  Confusión:
    Windows:  "created" = birth time ✅
    macOS:    "created" = birth time ✅
    Linux:    "ctime"   = change time ❌ (NO creation)
              "btime"   = birth time ✅ (ext4/btrfs, si kernel lo expone)
```

### Error 3: `set_readonly(true)` no protege contra root

```rust
use std::fs;

// MAL — asumir que readonly protege el archivo
fn bad_protect(path: &str) -> std::io::Result<()> {
    let mut perms = fs::metadata(path)?.permissions();
    perms.set_readonly(true);
    fs::set_permissions(path, perms)?;

    // Root puede escribir de todos modos
    // Y cualquiera puede cambiar los permisos de vuelta
    // si es el owner
    Ok(())
}

// BIEN — entender las limitaciones
// set_readonly quita TODOS los write bits (equivale a chmod a-w)
// Protege contra escritura accidental, NO contra ataques
// Para protección real: chattr +i (immutable flag, requiere root)
```

### Error 4: Ignorar `umask` al crear archivos

```rust
use std::fs::{self, OpenOptions, Permissions};
use std::os::unix::fs::{OpenOptionsExt, PermissionsExt};

// MAL — esperar permisos exactos sin considerar umask
fn bad_create() -> std::io::Result<()> {
    OpenOptions::new()
        .write(true)
        .create(true)
        .mode(0o666) // Esperas rw-rw-rw-
        .open("public.txt")?;

    let mode = fs::metadata("public.txt")?.permissions().mode() & 0o7777;
    println!("Expected 666, got {:o}", mode);
    // Con umask=022, obtienes 644, no 666
    Ok(())
}

// BIEN — saber que umask se aplica automáticamente
// El modo final es: mode & ~umask
// mode=0o666, umask=0o022 → 0o644
// Si necesitas permisos EXACTOS, establécelos DESPUÉS de crear:
fn exact_perms(path: &str, mode: u32) -> std::io::Result<()> {
    fs::write(path, "")?;
    fs::set_permissions(path, Permissions::from_mode(mode))?;
    // set_permissions NO está afectado por umask
    Ok(())
}
```

### Error 5: Leer metadata de symlinks sin darse cuenta

```rust
use std::fs;

// MAL — metadata() sigue symlinks silenciosamente
fn bad_check_symlink(path: &str) -> std::io::Result<bool> {
    let meta = fs::metadata(path)?;
    Ok(meta.is_symlink()) // SIEMPRE false con metadata()
    // metadata() usa stat() que sigue symlinks
}

// BIEN — usar symlink_metadata()
fn good_check_symlink(path: &str) -> std::io::Result<bool> {
    let meta = fs::symlink_metadata(path)?;
    Ok(meta.is_symlink()) // Correcto: lstat() ve el symlink
}

// O verificar qué tipo realmente es:
fn file_info(path: &str) -> std::io::Result<()> {
    let link_meta = fs::symlink_metadata(path)?;

    if link_meta.is_symlink() {
        let target = fs::read_link(path)?;
        let target_meta = fs::metadata(path)?; // Sigue el symlink
        println!("{} -> {} (target is_file: {})",
            path, target.display(), target_meta.is_file());
    } else {
        println!("{}: is_file={} is_dir={}",
            path, link_meta.is_file(), link_meta.is_dir());
    }

    Ok(())
}
```

---

## 10. Cheatsheet

```text
──────────────────── Obtener Metadata ────────────────
fs::metadata(path)?                stat()  — sigue symlinks
fs::symlink_metadata(path)?        lstat() — no sigue symlinks
file.metadata()?                   fstat() — desde file descriptor

──────────────────── Metadata (portable) ─────────────
meta.len()                         Tamaño en bytes (u64)
meta.is_file()                     ¿Archivo regular?
meta.is_dir()                      ¿Directorio?
meta.is_symlink()                  ¿Symlink? (solo con lstat)
meta.file_type()                   FileType
meta.permissions()                 Permissions
meta.modified()?                   SystemTime (mtime)
meta.accessed()?                   SystemTime (atime)
meta.created()?                    SystemTime (btime, puede fallar)

──────────────────── Permisos (portable) ─────────────
perms.readonly()                   ¿Nadie puede escribir?
perms.set_readonly(true/false)     Establecer read-only
fs::set_permissions(path, perms)?  Aplicar permisos

──────────────────── Permisos (Unix) ─────────────────
use std::os::unix::fs::PermissionsExt;
perms.mode()                       u32 con st_mode completo
Permissions::from_mode(0o755)      Crear desde octal
mode & 0o7777                      Extraer solo permisos
mode & 0o777                       Solo rwx (sin setuid/sticky)
mode & 0o100 != 0                  ¿Owner execute?
mode | 0o111                       Añadir execute (chmod +x)
mode & !0o222                      Quitar write (chmod -w)

──────────────────── MetadataExt (Unix) ──────────────
use std::os::unix::fs::MetadataExt;
meta.dev()                         Device number
meta.ino()                         Inode number
meta.mode()                        st_mode (tipo + permisos)
meta.nlink()                       Hard link count
meta.uid()                         Owner user ID
meta.gid()                         Owner group ID
meta.size()                        Tamaño aparente
meta.blocks()                      Bloques de 512 bytes en disco
meta.blksize()                     Tamaño de bloque preferido I/O
meta.rdev()                        Device type (block/char devices)
meta.atime() / atime_nsec()        Access time (epoch + ns)
meta.mtime() / mtime_nsec()        Modification time
meta.ctime() / ctime_nsec()        Change time (NO creation!)

──────────────────── Crear con permisos ──────────────
OpenOptions::new().mode(0o600)     Archivo con modo (afectado por umask)
DirBuilder::new().mode(0o700)      Directorio con modo
fs::set_permissions(p, perms)?     Establecer modo exacto (sin umask)

──────────────────── Timestamps ──────────────────────
meta.modified()?                   mtime (SystemTime)
meta.accessed()?                   atime (SystemTime)
meta.created()?                    btime (puede fallar en Linux)
filetime::set_file_mtime()         Modificar mtime (crate)
filetime::set_file_times()         Modificar atime + mtime

──────────────────── Ownership ───────────────────────
meta.uid() / meta.gid()            Leer owner (MetadataExt)
libc::chown() / nix::unistd::chown Cambiar owner (requiere root)

──────────────────── Comparaciones ───────────────────
(dev, ino) iguales                 Mismo archivo (hard links)
mtime_a > mtime_b                 a más reciente que b
blocks * 512 < size                Sparse file
mode & 0o4000 != 0                 Setuid
mode & 0o002 != 0                  World-writable
```

---

## 11. Ejercicios

### Ejercicio 1: `stat` command en Rust

Implementa un comando que muestre la información detallada de un archivo, similar
a `stat`:

```rust
use std::fs;
use std::os::unix::fs::{MetadataExt, PermissionsExt};
use std::time::{UNIX_EPOCH, Duration};

fn stat_file(path: &str) -> std::io::Result<()> {
    // TODO: Usar symlink_metadata para obtener info del path
    // TODO: Imprimir toda la información con este formato:
    //
    //   File: /etc/passwd
    //   Size: 2847       Blocks: 8          IO Block: 4096   regular file
    //   Device: 803h/2051d  Inode: 1234567   Links: 1
    //   Access: (0644/-rw-r--r--)  Uid: (0/root)  Gid: (0/root)
    //   Access: 2024-01-15 10:30:45.123456789
    //   Modify: 2024-01-10 08:15:22.987654321
    //   Change: 2024-01-10 08:15:22.987654321
    //    Birth: -
    //
    // Hints:
    //   - format_permissions ya implementada arriba
    //   - uid_to_name/gid_to_name para resolver nombres
    //   - Para formatear timestamps, convertir epoch secs a fecha
    //     (puedes usar formato simplificado sin crate chrono)
    //   - Para birth time, usar meta.created() y manejar Err

    todo!()
}

fn main() -> std::io::Result<()> {
    let path = std::env::args().nth(1)
        .unwrap_or_else(|| "/etc/passwd".into());
    stat_file(&path)
}
```

**Pregunta de reflexión**: ¿por qué `stat` muestra tanto el valor numérico como el
string de permisos (`0644/-rw-r--r--`)? ¿En qué situaciones el formato octal es más
útil que el string, y viceversa? ¿Por qué `stat` muestra "Birth: -" en muchos sistemas
Linux aunque el filesystem (ext4) sí almacena el birth time?

### Ejercicio 2: `chmod` recursivo

Implementa una herramienta que cambie permisos recursivamente, con opciones para
archivos y directorios por separado:

```rust
use walkdir::WalkDir;
use std::fs;
use std::os::unix::fs::PermissionsExt;

struct ChmodOptions {
    path: String,
    file_mode: Option<u32>,   // Modo para archivos (ej: 0o644)
    dir_mode: Option<u32>,    // Modo para directorios (ej: 0o755)
    recursive: bool,
    dry_run: bool,            // Solo mostrar, no cambiar
}

fn chmod_recursive(opts: &ChmodOptions) -> std::io::Result<(usize, usize)> {
    // TODO:
    // 1. Recorrer con walkdir (o read_dir si !recursive)
    // 2. Para cada entry:
    //    - Si es directorio y dir_mode está definido → aplicar dir_mode
    //    - Si es archivo y file_mode está definido → aplicar file_mode
    //    - Si dry_run, solo imprimir qué haría
    // 3. Contar archivos y directorios modificados
    // 4. Manejar errores por entry (no abortar todo por un permiso denegado)
    //
    // Ejemplo de uso:
    //   chmod_recursive(&ChmodOptions {
    //       path: "project/".into(),
    //       file_mode: Some(0o644),
    //       dir_mode: Some(0o755),
    //       recursive: true,
    //       dry_run: false,
    //   })?;

    todo!()
}

fn main() -> std::io::Result<()> {
    // TODO: Crear estructura de prueba:
    //   test_chmod/
    //   ├── file1.txt (0o777 → debería quedar 0o644)
    //   ├── subdir/   (0o777 → debería quedar 0o755)
    //   │   └── file2.txt
    //   └── script.sh (0o777 → debería quedar 0o644)

    // TODO: Ejecutar chmod_recursive con file_mode=0o644, dir_mode=0o755
    // TODO: Verificar que los permisos son correctos
    // TODO: Limpiar

    Ok(())
}
```

**Pregunta de reflexión**: ¿por qué es común usar un modo diferente para directorios
(0o755) y archivos (0o644)? ¿Qué pasa si un directorio no tiene el bit `x` (execute)?
¿Puedes listar su contenido? ¿Puedes acceder a archivos dentro de él?

### Ejercicio 3: Detector de archivos con permisos inseguros

Implementa un scanner que encuentre archivos con permisos potencialmente peligrosos:

```rust
use walkdir::WalkDir;
use std::os::unix::fs::{MetadataExt, PermissionsExt};
use std::path::PathBuf;

#[derive(Debug)]
enum SecurityIssue {
    WorldWritableFile(PathBuf),
    WorldWritableDir(PathBuf),        // Sin sticky bit
    SetuidBinary(PathBuf, u32, u32),  // path, uid, mode
    SetgidBinary(PathBuf, u32, u32),
    PrivateKeyTooOpen(PathBuf, u32),  // .pem/.key con mode > 600
    DotFileWorldReadable(PathBuf),    // .env, .secrets, etc.
}

fn scan_security(root: &str) -> Vec<SecurityIssue> {
    // TODO:
    // 1. Recorrer recursivamente (excluir .git, node_modules)
    // 2. Para cada entry, verificar:
    //    a) World-writable file (mode & 0o002)
    //    b) World-writable directory sin sticky (mode & 0o002, !(mode & 0o1000))
    //    c) Setuid binary (mode & 0o4000)
    //    d) Setgid binary (mode & 0o2000)
    //    e) Archivos .pem/.key/.p12 con permisos > 0o600
    //    f) Archivos .env/.secrets/.password world-readable (mode & 0o004)
    // 3. Retornar lista de issues encontrados

    todo!()
}

fn main() {
    let root = std::env::args().nth(1).unwrap_or_else(|| ".".into());
    let issues = scan_security(&root);

    if issues.is_empty() {
        println!("No security issues found in {}", root);
    } else {
        println!("Found {} security issues:\n", issues.len());
        for issue in &issues {
            match issue {
                SecurityIssue::WorldWritableFile(p) =>
                    println!("[HIGH] World-writable file: {}", p.display()),
                SecurityIssue::WorldWritableDir(p) =>
                    println!("[CRIT] World-writable dir (no sticky): {}", p.display()),
                SecurityIssue::SetuidBinary(p, uid, mode) =>
                    println!("[INFO] Setuid (uid={}, mode={:o}): {}",
                        uid, mode & 0o7777, p.display()),
                SecurityIssue::SetgidBinary(p, gid, mode) =>
                    println!("[INFO] Setgid (gid={}, mode={:o}): {}",
                        gid, mode & 0o7777, p.display()),
                SecurityIssue::PrivateKeyTooOpen(p, mode) =>
                    println!("[HIGH] Private key too open ({:o}): {}",
                        mode & 0o7777, p.display()),
                SecurityIssue::DotFileWorldReadable(p) =>
                    println!("[WARN] Sensitive dotfile world-readable: {}",
                        p.display()),
            }
        }
    }
}
```

**Pregunta de reflexión**: si ejecutas este scanner en `/usr/bin`, encontrarás varios
binarios setuid legítimos (como `passwd`, `sudo`, `ping`). ¿Cómo distinguirías un
setuid legítimo de uno sospechoso? ¿Es suficiente verificar que el owner sea root?
¿Qué otras señales indicarían un setuid malicioso (hint: fecha de modificación,
ubicación fuera de paths estándar)?