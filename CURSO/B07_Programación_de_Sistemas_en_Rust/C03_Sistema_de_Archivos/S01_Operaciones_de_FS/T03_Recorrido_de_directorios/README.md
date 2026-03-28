# Recorrido de Directorios

## Índice

1. [read_dir — Listar un directorio](#1-read_dir--listar-un-directorio)
2. [Recorrido recursivo manual](#2-recorrido-recursivo-manual)
3. [walkdir — Recorrido recursivo profesional](#3-walkdir--recorrido-recursivo-profesional)
4. [Filtros y predicados](#4-filtros-y-predicados)
5. [Symlinks y ciclos](#5-symlinks-y-ciclos)
6. [Rendimiento y consideraciones](#6-rendimiento-y-consideraciones)
7. [Patrones prácticos](#7-patrones-prácticos)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. `read_dir` — Listar un directorio

`std::fs::read_dir` es la primitiva base. Lista el contenido de **un solo directorio**
(no recursivo). Retorna un iterador de `Result<DirEntry>`.

### 1.1 Uso básico

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    for entry in fs::read_dir("/tmp")? {
        let entry = entry?; // Cada entry puede fallar independientemente

        println!("{}", entry.path().display());
    }
    Ok(())
}
```

### 1.2 `DirEntry` — Qué información tiene

```rust
use std::fs;

fn main() -> std::io::Result<()> {
    for entry in fs::read_dir("/etc")? {
        let entry = entry?;

        // path(): PathBuf completo (dir + nombre)
        let path = entry.path();

        // file_name(): OsString (solo el nombre, sin directorio)
        let name = entry.file_name();

        // file_type(): tipo sin seguir symlinks (no hace stat completo)
        let file_type = entry.file_type()?;

        // metadata(): stat completo (sigue symlinks)
        let meta = entry.metadata()?;

        let kind = if file_type.is_dir() {
            "DIR "
        } else if file_type.is_file() {
            "FILE"
        } else if file_type.is_symlink() {
            "LINK"
        } else {
            "OTHR"
        };

        println!("{} {:>8} {}",
            kind,
            meta.len(),
            name.to_string_lossy()
        );
    }
    Ok(())
}
```

```
DirEntry proporciona:
┌─────────────────┬──────────────────────────────────────────┐
│ entry.path()    │ PathBuf completo (/etc/hostname)         │
│ entry.file_name │ OsString solo nombre (hostname)          │
│ entry.file_type │ Dir/File/Symlink (sin stat, rápido)      │
│ entry.metadata  │ stat completo (tamaño, permisos, times)  │
└─────────────────┴──────────────────────────────────────────┘

file_type() vs metadata():
  file_type()  → usa dirent.d_type (Linux) — sin syscall extra
  metadata()   → hace stat() — syscall adicional por cada entry
  → Usa file_type() si solo necesitas saber dir/file/symlink
```

### 1.3 Orden de iteración

`read_dir` **no garantiza orden**. El orden depende del filesystem y puede variar
entre ejecuciones. Si necesitas orden, recolecta y ordena:

```rust
use std::fs;
use std::path::PathBuf;

fn sorted_dir_listing(dir: &str) -> std::io::Result<Vec<PathBuf>> {
    let mut entries: Vec<PathBuf> = fs::read_dir(dir)?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .collect();

    entries.sort();
    Ok(entries)
}

fn main() -> std::io::Result<()> {
    for path in sorted_dir_listing("/etc")? {
        println!("{}", path.display());
    }
    Ok(())
}
```

### 1.4 Contar archivos eficientemente

```rust
use std::fs;

fn count_entries(dir: &str) -> std::io::Result<(usize, usize, usize)> {
    let mut files = 0;
    let mut dirs = 0;
    let mut others = 0;

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let ft = entry.file_type()?; // Rápido: no hace stat()

        if ft.is_file() {
            files += 1;
        } else if ft.is_dir() {
            dirs += 1;
        } else {
            others += 1;
        }
    }

    Ok((files, dirs, others))
}

fn main() -> std::io::Result<()> {
    let (files, dirs, others) = count_entries("/etc")?;
    println!("Files: {}, Dirs: {}, Others: {}", files, dirs, others);
    Ok(())
}
```

---

## 2. Recorrido recursivo manual

`read_dir` solo lista un nivel. Para recorrer recursivamente, necesitas descender
a los subdirectorios manualmente.

### 2.1 Recursión con función

```rust
use std::fs;
use std::path::Path;
use std::io;

fn walk_recursive(dir: &Path) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        let ft = entry.file_type()?;

        if ft.is_dir() {
            println!("DIR:  {}", path.display());
            walk_recursive(&path)?; // Descender al subdirectorio
        } else {
            println!("FILE: {}", path.display());
        }
    }
    Ok(())
}

fn main() -> io::Result<()> {
    walk_recursive(Path::new("/tmp"))?;
    Ok(())
}
```

### 2.2 Iterativo con pila (evitar recursión profunda)

La recursión puede desbordar el stack si el directorio es muy profundo. Una pila
explícita lo evita:

```rust
use std::fs;
use std::path::PathBuf;
use std::io;

fn walk_iterative(root: &str) -> io::Result<Vec<PathBuf>> {
    let mut results = Vec::new();
    let mut stack = vec![PathBuf::from(root)];

    while let Some(dir) = stack.pop() {
        let entries = match fs::read_dir(&dir) {
            Ok(entries) => entries,
            Err(e) => {
                eprintln!("Cannot read {}: {}", dir.display(), e);
                continue; // Saltar dirs inaccesibles
            }
        };

        for entry in entries {
            let entry = match entry {
                Ok(e) => e,
                Err(e) => {
                    eprintln!("Entry error: {}", e);
                    continue;
                }
            };

            let path = entry.path();
            let ft = entry.file_type().unwrap_or_else(|_| {
                // Fallback: intentar con metadata
                fs::symlink_metadata(&path)
                    .map(|m| m.file_type())
                    .unwrap_or_else(|_| fs::metadata(".").unwrap().file_type())
            });

            if ft.is_dir() {
                stack.push(path);
            } else {
                results.push(path);
            }
        }
    }

    Ok(results)
}

fn main() -> io::Result<()> {
    let files = walk_iterative("/tmp")?;
    println!("Found {} files", files.len());
    Ok(())
}
```

```
Recursivo vs Iterativo:

Recursivo:                    Iterativo (pila):
  walk("/a")                    stack = ["/a"]
    ├── walk("/a/b")            pop "/a" → stack = ["/a/b", "/a/c"]
    │   └── walk("/a/b/d")      pop "/a/c" → stack = ["/a/b"]
    └── walk("/a/c")            pop "/a/b" → stack = ["/a/b/d"]
                                pop "/a/b/d" → stack = []

  Stack frames: O(depth)       Stack frames: O(1)
  Stack overflow si depth > ~1000  No overflow
```

### 2.3 BFS vs DFS

La pila produce DFS (depth-first). Para BFS (breadth-first), usa una cola:

```rust
use std::collections::VecDeque;
use std::fs;
use std::path::PathBuf;

fn walk_bfs(root: &str) -> std::io::Result<Vec<PathBuf>> {
    let mut results = Vec::new();
    let mut queue = VecDeque::new();
    queue.push_back(PathBuf::from(root));

    while let Some(dir) = queue.pop_front() { // front = BFS
        for entry in fs::read_dir(&dir)? {
            let entry = entry?;
            let path = entry.path();

            if entry.file_type()?.is_dir() {
                queue.push_back(path);
            } else {
                results.push(path);
            }
        }
    }

    Ok(results)
}
```

```
Árbol:          DFS order:       BFS order:
    a           a                a
   / \          b                b, c
  b   c         d                d, e, f, g
 / \  / \       e
d  e f  g       c
                f
                g

DFS: profundiza primero (útil para buscar un archivo específico)
BFS: nivel por nivel (útil para encontrar lo más cercano a la raíz)
```

---

## 3. `walkdir` — Recorrido recursivo profesional

La crate `walkdir` es la solución estándar para recorrido recursivo. Resuelve todos
los edge cases que la versión manual ignora: symlinks, ciclos, permisos, profundidad
máxima, manejo de errores.

```toml
# Cargo.toml
[dependencies]
walkdir = "2"
```

### 3.1 Uso básico

```rust
use walkdir::WalkDir;

fn main() {
    for entry in WalkDir::new("/etc") {
        match entry {
            Ok(entry) => println!("{}", entry.path().display()),
            Err(e) => eprintln!("Error: {}", e),
        }
    }
}
```

### 3.2 Configurar el recorrido

```rust
use walkdir::WalkDir;

fn main() {
    let walker = WalkDir::new("/home")
        .min_depth(1)        // Saltar la raíz (empezar en hijos)
        .max_depth(3)        // No descender más de 3 niveles
        .follow_links(false) // No seguir symlinks (default)
        .sort_by_file_name(); // Ordenar entries (requiere más memoria)

    for entry in walker {
        let entry = entry.unwrap();
        let depth = entry.depth();
        let indent = "  ".repeat(depth);
        println!("{}{}", indent, entry.file_name().to_string_lossy());
    }
}
```

### 3.3 `DirEntry` de walkdir

`walkdir::DirEntry` ofrece más información que `std::fs::DirEntry`:

```rust
use walkdir::WalkDir;

fn main() {
    for entry in WalkDir::new("/etc").max_depth(1) {
        let entry = entry.unwrap();

        // Depth: qué tan profundo estamos (0 = raíz)
        let depth = entry.depth();

        // Path completo
        let path = entry.path();

        // File type (sin stat extra en la mayoría de OSes)
        let ft = entry.file_type();

        // ¿Es un symlink que apunta a un directorio?
        let is_link_dir = entry.path_is_symlink() && ft.is_dir();

        // Metadata completa (lazy, hace stat solo si se pide)
        if let Ok(meta) = entry.metadata() {
            println!("[d={}] {:>8} {}{}",
                depth,
                meta.len(),
                path.display(),
                if is_link_dir { " -> (link)" } else { "" }
            );
        }
    }
}
```

### 3.4 `filter_map` y `filter_entry`

```rust
use walkdir::WalkDir;

fn main() {
    // filter_map: filtrar entries individuales (no afecta descenso)
    let rust_files: Vec<_> = WalkDir::new(".")
        .into_iter()
        .filter_map(|e| e.ok()) // Ignorar errores
        .filter(|e| {
            e.file_type().is_file()
                && e.path().extension().map_or(false, |ext| ext == "rs")
        })
        .collect();

    for f in &rust_files {
        println!("{}", f.path().display());
    }

    // filter_entry: filtrar ANTES de descender (afecta recorrido)
    // Si un directorio no pasa el filtro, NO se desciende en él
    println!("\n--- Sin target/ ni .git/ ---");

    let walker = WalkDir::new(".").into_iter()
        .filter_entry(|e| {
            // Retorna false → no entrar en este directorio
            let name = e.file_name().to_string_lossy();
            !name.starts_with('.') && name != "target" && name != "node_modules"
        });

    for entry in walker.filter_map(|e| e.ok()) {
        println!("{}", entry.path().display());
    }
}
```

```
filter_map vs filter_entry:

Árbol:
  project/
  ├── src/
  │   ├── main.rs
  │   └── lib.rs
  ├── target/           ← Queremos excluir
  │   └── debug/
  │       └── (miles de archivos)
  └── .git/             ← Queremos excluir
      └── objects/

filter_map(skip target):
  ✓ Descends into target/debug/ (lee miles de archivos)
  ✗ Los filtra DESPUÉS de leerlos → LENTO

filter_entry(skip target):
  ✗ No desciende en target/ ni .git/ → RÁPIDO
  Ahorra potencialmente miles de stat() calls
```

### 3.5 Ordenamiento personalizado

```rust
use walkdir::WalkDir;

fn main() {
    // Ordenar por nombre
    for entry in WalkDir::new(".").sort_by_file_name() {
        let entry = entry.unwrap();
        println!("{}", entry.path().display());
    }

    // Ordenar por tamaño (de mayor a menor)
    // sort_by permite comparación personalizada
    for entry in WalkDir::new(".")
        .sort_by(|a, b| {
            let size_a = a.metadata().map(|m| m.len()).unwrap_or(0);
            let size_b = b.metadata().map(|m| m.len()).unwrap_or(0);
            size_b.cmp(&size_a) // Descendente
        })
    {
        let entry = entry.unwrap();
        if entry.file_type().is_file() {
            let size = entry.metadata().map(|m| m.len()).unwrap_or(0);
            println!("{:>10} {}", size, entry.path().display());
        }
    }
}
```

### 3.6 Conteo y estadísticas

```rust
use walkdir::WalkDir;

fn dir_stats(root: &str) -> std::io::Result<()> {
    let mut total_files = 0u64;
    let mut total_dirs = 0u64;
    let mut total_size = 0u64;
    let mut max_depth = 0usize;
    let mut errors = 0u64;

    for entry in WalkDir::new(root) {
        match entry {
            Ok(entry) => {
                if entry.depth() > max_depth {
                    max_depth = entry.depth();
                }

                if entry.file_type().is_file() {
                    total_files += 1;
                    if let Ok(meta) = entry.metadata() {
                        total_size += meta.len();
                    }
                } else if entry.file_type().is_dir() {
                    total_dirs += 1;
                }
            }
            Err(_) => errors += 1,
        }
    }

    println!("Directory: {}", root);
    println!("  Files:     {}", total_files);
    println!("  Dirs:      {}", total_dirs);
    println!("  Size:      {} bytes ({:.2} MB)",
        total_size, total_size as f64 / 1_048_576.0);
    println!("  Max depth: {}", max_depth);
    println!("  Errors:    {}", errors);

    Ok(())
}

fn main() -> std::io::Result<()> {
    dir_stats("/etc")
}
```

---

## 4. Filtros y predicados

### 4.1 Filtrar por extensión

```rust
use walkdir::WalkDir;
use std::path::Path;

fn find_files_with_ext(root: &str, ext: &str) -> Vec<std::path::PathBuf> {
    WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| {
            e.file_type().is_file()
                && e.path().extension().map_or(false, |e| e == ext)
        })
        .map(|e| e.path().to_path_buf())
        .collect()
}

fn main() {
    let rs_files = find_files_with_ext(".", "rs");
    for f in &rs_files {
        println!("{}", f.display());
    }
    println!("Found {} .rs files", rs_files.len());
}
```

### 4.2 Filtrar por tamaño

```rust
use walkdir::WalkDir;

fn find_large_files(root: &str, min_bytes: u64) -> Vec<(std::path::PathBuf, u64)> {
    WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter_map(|e| {
            let size = e.metadata().ok()?.len();
            if size >= min_bytes {
                Some((e.path().to_path_buf(), size))
            } else {
                None
            }
        })
        .collect()
}

fn format_size(bytes: u64) -> String {
    if bytes >= 1_073_741_824 {
        format!("{:.1} GB", bytes as f64 / 1_073_741_824.0)
    } else if bytes >= 1_048_576 {
        format!("{:.1} MB", bytes as f64 / 1_048_576.0)
    } else if bytes >= 1024 {
        format!("{:.1} KB", bytes as f64 / 1024.0)
    } else {
        format!("{} B", bytes)
    }
}

fn main() {
    let large = find_large_files("/var/log", 1_048_576); // > 1MB
    let mut sorted = large;
    sorted.sort_by(|a, b| b.1.cmp(&a.1)); // Mayor primero

    for (path, size) in sorted.iter().take(20) {
        println!("{:>10} {}", format_size(*size), path.display());
    }
}
```

### 4.3 Filtrar por nombre con glob patterns

```rust
fn matches_glob(name: &str, pattern: &str) -> bool {
    // Implementación simplificada de glob matching
    // Para producción, usar la crate `glob` o `globset`
    let mut name_chars = name.chars().peekable();
    let mut pattern_chars = pattern.chars().peekable();

    while let Some(&pc) = pattern_chars.peek() {
        match pc {
            '*' => {
                pattern_chars.next();
                if pattern_chars.peek().is_none() {
                    return true; // * al final matchea todo
                }
                // Intentar matchear el resto del pattern
                while name_chars.peek().is_some() {
                    let remaining_name: String = name_chars.clone().collect();
                    let remaining_pattern: String = pattern_chars.clone().collect();
                    if matches_glob(&remaining_name, &remaining_pattern) {
                        return true;
                    }
                    name_chars.next();
                }
                return false;
            }
            '?' => {
                pattern_chars.next();
                if name_chars.next().is_none() {
                    return false;
                }
            }
            c => {
                pattern_chars.next();
                match name_chars.next() {
                    Some(nc) if nc == c => {}
                    _ => return false,
                }
            }
        }
    }

    name_chars.peek().is_none()
}

fn main() {
    // Para producción, mejor usar la crate `glob`:
    // use glob::glob;
    // for entry in glob("src/**/*.rs").unwrap() {
    //     println!("{}", entry.unwrap().display());
    // }

    assert!(matches_glob("main.rs", "*.rs"));
    assert!(matches_glob("test_utils.rs", "test_*.rs"));
    assert!(!matches_glob("main.py", "*.rs"));
    println!("Glob tests passed");
}
```

### 4.4 Filtrar por fecha de modificación

```rust
use walkdir::WalkDir;
use std::time::{SystemTime, Duration};

fn find_recently_modified(root: &str, within: Duration) -> Vec<std::path::PathBuf> {
    let cutoff = SystemTime::now() - within;

    WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter(|e| {
            e.metadata()
                .ok()
                .and_then(|m| m.modified().ok())
                .map_or(false, |modified| modified > cutoff)
        })
        .map(|e| e.path().to_path_buf())
        .collect()
}

fn main() {
    // Archivos modificados en las últimas 24 horas
    let recent = find_recently_modified("/var/log", Duration::from_secs(86400));
    println!("Recently modified files in /var/log:");
    for f in &recent {
        println!("  {}", f.display());
    }
}
```

---

## 5. Symlinks y ciclos

### 5.1 El problema de los symlinks

Los symlinks pueden crear ciclos infinitos en el recorrido:

```
/data/
├── actual/
│   ├── file.txt
│   └── link_to_data → /data/    ← CICLO INFINITO
└── other/
    └── file2.txt

Recorrido sin protección:
  /data/actual/file.txt
  /data/actual/link_to_data/actual/file.txt
  /data/actual/link_to_data/actual/link_to_data/actual/file.txt
  ... infinito
```

### 5.2 Comportamiento de `walkdir`

Por defecto, `walkdir` **no sigue symlinks** — los lista pero no desciende:

```rust
use walkdir::WalkDir;

fn main() {
    for entry in WalkDir::new("/data") {
        let entry = entry.unwrap();

        if entry.path_is_symlink() {
            println!("SYMLINK: {}", entry.path().display());
            // No desciende en el symlink
        } else {
            println!("        {}", entry.path().display());
        }
    }
}
```

### 5.3 Seguir symlinks con detección de ciclos

Si necesitas seguir symlinks, `walkdir` detecta ciclos automáticamente:

```rust
use walkdir::WalkDir;

fn main() {
    let walker = WalkDir::new("/data")
        .follow_links(true); // Seguir symlinks

    for entry in walker {
        match entry {
            Ok(entry) => {
                println!("{}", entry.path().display());
            }
            Err(e) => {
                // walkdir detecta ciclos y retorna error
                if let Some(loop_ancestor) = e.loop_ancestor() {
                    eprintln!("CYCLE detected: {} -> {}",
                        e.path().unwrap_or(std::path::Path::new("?")).display(),
                        loop_ancestor.display()
                    );
                } else {
                    eprintln!("Error: {}", e);
                }
            }
        }
    }
}
```

`walkdir` detecta ciclos comparando inodes (device + inode number). Si un directorio
ya fue visitado (mismo inode), retorna un error en vez de descender.

### 5.4 Manejo manual de symlinks con `read_dir`

Si usas `read_dir` manual, debes protegerte tú mismo:

```rust
use std::fs;
use std::path::{Path, PathBuf};
use std::collections::HashSet;
use std::io;

#[cfg(unix)]
use std::os::unix::fs::MetadataExt;

fn walk_safe(root: &Path) -> io::Result<Vec<PathBuf>> {
    let mut results = Vec::new();
    let mut visited = HashSet::new(); // Track de inodes visitados
    let mut stack = vec![root.to_path_buf()];

    while let Some(dir) = stack.pop() {
        // Obtener inode para detectar ciclos
        #[cfg(unix)]
        {
            let meta = fs::metadata(&dir)?;
            let inode_key = (meta.dev(), meta.ino());
            if !visited.insert(inode_key) {
                eprintln!("Cycle detected at: {}", dir.display());
                continue; // Ya visitado
            }
        }

        for entry in fs::read_dir(&dir)? {
            let entry = entry?;
            let path = entry.path();
            let ft = entry.file_type()?;

            if ft.is_dir() {
                stack.push(path);
            } else if ft.is_symlink() {
                // Para symlinks a directorios: resolver y verificar ciclo
                if let Ok(real) = fs::canonicalize(&path) {
                    if real.is_dir() {
                        // Podría ser un ciclo — verificar inode
                        #[cfg(unix)]
                        {
                            let meta = fs::metadata(&real)?;
                            let key = (meta.dev(), meta.ino());
                            if visited.contains(&key) {
                                eprintln!("Symlink cycle: {} -> {}",
                                    path.display(), real.display());
                                continue;
                            }
                        }
                        stack.push(path);
                    } else {
                        results.push(path);
                    }
                }
            } else {
                results.push(path);
            }
        }
    }

    Ok(results)
}
```

---

## 6. Rendimiento y consideraciones

### 6.1 Costo de cada operación

```
┌─────────────────────────┬───────────────────────────────────┐
│ Operación               │ Costo                             │
├─────────────────────────┼───────────────────────────────────┤
│ read_dir()              │ 1 syscall (opendir+readdir)        │
│ entry.file_name()       │ Gratis (del dirent)                │
│ entry.file_type()       │ Gratis en Linux (d_type en dirent) │
│                         │ 1 stat() en algunos FS             │
│ entry.metadata()        │ 1 stat() syscall                   │
│ path.canonicalize()     │ 1+ stat() + realpath               │
│ path.exists()           │ 1 stat() syscall                   │
└─────────────────────────┴───────────────────────────────────┘

Para un directorio con 10,000 archivos:
  Solo listar nombres:   ~1 syscall (readdir bulk)
  + file_type para cada:  0 extra (Linux ext4/btrfs)
  + metadata para cada:   10,000 stat() syscalls ← costoso
```

### 6.2 Optimizar recorridos grandes

```rust
use walkdir::WalkDir;

fn optimized_search(root: &str, target_ext: &str) -> Vec<std::path::PathBuf> {
    WalkDir::new(root)
        .into_iter()
        // 1. Excluir directorios que no necesitamos ANTES de descender
        .filter_entry(|e| {
            let name = e.file_name().to_string_lossy();
            // No descender en estos directorios
            !matches!(name.as_ref(), "target" | "node_modules" | ".git"
                | "vendor" | "__pycache__" | ".cache")
        })
        // 2. Ignorar errores (dirs sin permisos)
        .filter_map(|e| e.ok())
        // 3. Filtrar por tipo ANTES de comparar extensión
        //    file_type() es gratis, extension() requiere parsing
        .filter(|e| e.file_type().is_file())
        // 4. Ahora sí comparar extensión
        .filter(|e| {
            e.path().extension()
                .map_or(false, |ext| ext == target_ext)
        })
        .map(|e| e.into_path()) // into_path() evita clone
        .collect()
}
```

```
Orden de filtros importa:

Orden ineficiente:                    Orden eficiente:
  1. metadata() para cada entry        1. filter_entry (skip dirs)
  2. Comparar tamaño                   2. filter by file_type (gratis)
  3. filter_entry al final              3. filter by extension (parse)
                                        4. metadata() solo si necesario
  10K entries × stat() = lento          Reduce entries ANTES de stat()
```

### 6.3 Paralelizar el recorrido

Para directorios muy grandes, puedes paralelizar con Rayon:

```rust
use walkdir::WalkDir;
use std::path::PathBuf;

// Nota: walkdir en sí es single-threaded,
// pero puedes paralelizar el PROCESAMIENTO
fn parallel_file_search(root: &str, ext: &str) -> Vec<PathBuf> {
    // Paso 1: recolectar paths (single-threaded, I/O-bound)
    let paths: Vec<PathBuf> = WalkDir::new(root)
        .into_iter()
        .filter_entry(|e| {
            !e.file_name().to_string_lossy().starts_with('.')
        })
        .filter_map(|e| e.ok())
        .filter(|e| {
            e.file_type().is_file()
                && e.path().extension().map_or(false, |e| e == ext)
        })
        .map(|e| e.into_path())
        .collect();

    // Paso 2: procesar en paralelo (CPU-bound)
    // use rayon::prelude::*;
    // paths.par_iter()
    //     .filter(|p| file_contains_pattern(p, "TODO"))
    //     .cloned()
    //     .collect()

    paths
}
```

---

## 7. Patrones prácticos

### 7.1 `find` simplificado

```rust
use walkdir::WalkDir;
use std::path::PathBuf;

struct FindOptions {
    root: String,
    name_pattern: Option<String>,
    extension: Option<String>,
    min_size: Option<u64>,
    max_size: Option<u64>,
    file_only: bool,
    dir_only: bool,
    max_depth: Option<usize>,
}

fn find(opts: &FindOptions) -> Vec<PathBuf> {
    let mut walker = WalkDir::new(&opts.root);

    if let Some(depth) = opts.max_depth {
        walker = walker.max_depth(depth);
    }

    walker
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| {
            let ft = e.file_type();
            if opts.file_only && !ft.is_file() { return false; }
            if opts.dir_only && !ft.is_dir() { return false; }

            // Filtro por nombre
            if let Some(ref pattern) = opts.name_pattern {
                let name = e.file_name().to_string_lossy();
                if !name.contains(pattern.as_str()) { return false; }
            }

            // Filtro por extensión
            if let Some(ref ext) = opts.extension {
                if e.path().extension().map_or(true, |e| e != ext.as_str()) {
                    return false;
                }
            }

            // Filtro por tamaño (solo para archivos)
            if opts.min_size.is_some() || opts.max_size.is_some() {
                if let Ok(meta) = e.metadata() {
                    let size = meta.len();
                    if let Some(min) = opts.min_size {
                        if size < min { return false; }
                    }
                    if let Some(max) = opts.max_size {
                        if size > max { return false; }
                    }
                }
            }

            true
        })
        .map(|e| e.into_path())
        .collect()
}

fn main() {
    let results = find(&FindOptions {
        root: ".".into(),
        name_pattern: None,
        extension: Some("rs".into()),
        min_size: Some(100),
        max_size: None,
        file_only: true,
        dir_only: false,
        max_depth: Some(5),
    });

    for r in &results {
        println!("{}", r.display());
    }
    println!("Found {} files", results.len());
}
```

### 7.2 Calcular tamaño de directorio

```rust
use walkdir::WalkDir;

fn dir_size(path: &str) -> u64 {
    WalkDir::new(path)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter_map(|e| e.metadata().ok())
        .map(|m| m.len())
        .sum()
}

fn main() {
    let size = dir_size("/etc");
    println!("/etc: {:.2} MB", size as f64 / 1_048_576.0);
}
```

### 7.3 Detectar archivos duplicados (por tamaño)

```rust
use walkdir::WalkDir;
use std::collections::HashMap;
use std::path::PathBuf;

fn find_size_duplicates(root: &str) -> Vec<(u64, Vec<PathBuf>)> {
    let mut by_size: HashMap<u64, Vec<PathBuf>> = HashMap::new();

    for entry in WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
    {
        if let Ok(meta) = entry.metadata() {
            let size = meta.len();
            if size > 0 { // Ignorar archivos vacíos
                by_size.entry(size)
                    .or_default()
                    .push(entry.into_path());
            }
        }
    }

    // Solo grupos con más de un archivo del mismo tamaño
    let mut duplicates: Vec<_> = by_size
        .into_iter()
        .filter(|(_, paths)| paths.len() > 1)
        .collect();

    duplicates.sort_by(|a, b| b.0.cmp(&a.0)); // Mayor tamaño primero
    duplicates
}

fn main() {
    let dupes = find_size_duplicates("/etc");
    for (size, paths) in dupes.iter().take(10) {
        println!("Size {} bytes ({} files):", size, paths.len());
        for p in paths {
            println!("  {}", p.display());
        }
    }
}
```

### 7.4 Tree: visualización de árbol de directorios

```rust
use walkdir::WalkDir;

fn print_tree(root: &str, max_depth: usize) {
    println!("{}", root);

    let entries: Vec<_> = WalkDir::new(root)
        .min_depth(1)
        .max_depth(max_depth)
        .sort_by_file_name()
        .into_iter()
        .filter_map(|e| e.ok())
        .collect();

    let total = entries.len();

    for (i, entry) in entries.iter().enumerate() {
        let depth = entry.depth();
        let is_last_at_depth = {
            // Verificar si es el último entry a este nivel
            entries.get(i + 1)
                .map_or(true, |next| next.depth() <= depth)
        };

        // Construir el prefijo visual
        let mut prefix = String::new();
        for d in 1..depth {
            // Verificar si hay más entries en niveles anteriores
            let has_more = entries[i + 1..]
                .iter()
                .any(|e| e.depth() == d);
            if has_more {
                prefix.push_str("│   ");
            } else {
                prefix.push_str("    ");
            }
        }

        if depth > 0 {
            if is_last_at_depth {
                prefix.push_str("└── ");
            } else {
                prefix.push_str("├── ");
            }
        }

        let name = entry.file_name().to_string_lossy();
        let suffix = if entry.file_type().is_dir() { "/" } else { "" };
        println!("{}{}{}", prefix, name, suffix);
    }

    println!("\n{} entries", total);
}

fn main() {
    print_tree(".", 3);
}
```

Salida ejemplo:
```
.
├── Cargo.toml
├── src/
│   ├── lib.rs
│   └── main.rs
└── tests/
    └── integration.rs

5 entries
```

---

## 8. Errores comunes

### Error 1: No usar `filter_entry` para excluir directorios grandes

```rust
use walkdir::WalkDir;

// MAL — desciende en target/ y .git/ antes de filtrar
fn bad_search(root: &str) {
    let files: Vec<_> = WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| {
            // Esto filtra DESPUÉS de descender — ya leyó target/
            let path_str = e.path().to_string_lossy();
            !path_str.contains("target") && !path_str.contains(".git")
        })
        .collect();
}

// BIEN — filter_entry evita descender
fn good_search(root: &str) {
    let files: Vec<_> = WalkDir::new(root)
        .into_iter()
        .filter_entry(|e| {
            let name = e.file_name().to_string_lossy();
            name != "target" && name != ".git" && name != "node_modules"
        })
        .filter_map(|e| e.ok())
        .collect();
}
```

**Por qué importa**: un directorio `target/` de un proyecto Rust puede contener cientos
de miles de archivos. `filter_entry` lo salta completamente sin leer su contenido.
Filtrar después con `.filter()` ya descendió y leyó todo — potencialmente segundos
de I/O desperdiciado.

### Error 2: Ignorar todos los errores con `filter_map(|e| e.ok())`

```rust
use walkdir::WalkDir;

// MAL — errores silenciosos, no sabes qué falló
fn bad(root: &str) {
    for entry in WalkDir::new(root).into_iter().filter_map(|e| e.ok()) {
        println!("{}", entry.path().display());
    }
    // ¿Saltó directorios por permisos? ¿Hubo ciclos? No lo sabes.
}

// BIEN — al menos loguear errores
fn good(root: &str) {
    for entry in WalkDir::new(root) {
        match entry {
            Ok(entry) => println!("{}", entry.path().display()),
            Err(e) => {
                if let Some(path) = e.path() {
                    eprintln!("Warning: cannot access {}: {}", path.display(), e);
                } else {
                    eprintln!("Warning: {}", e);
                }
            }
        }
    }
}
```

**Por qué importa**: `PermissionDenied` en un subdirectorio puede significar que
falta un directorio entero del resultado. Si buscas un archivo y no aparece, no sabrás
si realmente no existe o si estaba en un directorio que no pudiste leer.

### Error 3: `metadata()` en cada entry cuando solo necesitas `file_type`

```rust
use walkdir::WalkDir;

// MAL — stat() innecesario para cada entry
fn bad_count(root: &str) -> usize {
    WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.metadata().unwrap().is_file()) // stat() por entry
        .count()
}

// BIEN — file_type() no requiere stat en la mayoría de filesystems Linux
fn good_count(root: &str) -> usize {
    WalkDir::new(root)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file()) // Gratis (d_type)
        .count()
}
```

**Por qué importa**: en un directorio con 100,000 archivos, la versión mala hace
100,000 llamadas `stat()` innecesarias. `file_type()` en Linux ext4/btrfs/xfs lee
el tipo directamente del `dirent` sin syscall adicional.

### Error 4: Recorrido recursivo manual sin límite de profundidad

```rust
use std::fs;
use std::path::Path;

// MAL — puede causar stack overflow o loop infinito
fn bad_walk(dir: &Path) -> std::io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if entry.file_type()?.is_dir() {
            bad_walk(&entry.path())?; // Sin límite de profundidad
            // Symlink a directorio padre → recursión infinita
            // Directorio con 10,000 niveles → stack overflow
        }
    }
    Ok(())
}

// BIEN — limitar profundidad y detectar symlinks
fn good_walk(dir: &Path, max_depth: usize, current: usize) -> std::io::Result<()> {
    if current >= max_depth { return Ok(()); }

    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let ft = entry.file_type()?;

        if ft.is_dir() && !ft.is_symlink() { // No seguir symlinks
            good_walk(&entry.path(), max_depth, current + 1)?;
        }
    }
    Ok(())
}
```

### Error 5: Asumir que `read_dir` retorna entries en orden

```rust
use std::fs;

// MAL — asume que el output está ordenado
fn bad_latest_log(dir: &str) -> std::io::Result<String> {
    let entries: Vec<_> = fs::read_dir(dir)?
        .filter_map(|e| e.ok())
        .collect();

    // El "último" entry NO es el más reciente — el orden es arbitrario
    let last = entries.last().unwrap();
    Ok(last.file_name().to_string_lossy().into())
}

// BIEN — ordenar explícitamente por criterio
fn good_latest_log(dir: &str) -> std::io::Result<Option<String>> {
    let mut entries: Vec<_> = fs::read_dir(dir)?
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().map_or(false, |ft| ft.is_file()))
        .filter_map(|e| {
            let modified = e.metadata().ok()?.modified().ok()?;
            Some((e.file_name(), modified))
        })
        .collect();

    entries.sort_by(|a, b| b.1.cmp(&a.1)); // Más reciente primero

    Ok(entries.first().map(|(name, _)| name.to_string_lossy().into()))
}
```

---

## 9. Cheatsheet

```text
──────────────────── std::fs::read_dir ───────────────
fs::read_dir(path)?                Iterador de Result<DirEntry>
entry.path()                       PathBuf completo
entry.file_name()                  OsString (solo nombre)
entry.file_type()?                 FileType (gratis en Linux)
entry.metadata()?                  Metadata (stat syscall)
Orden: NO garantizado              Ordenar manualmente si necesario
Profundidad: solo 1 nivel          Recursión manual o walkdir

──────────────────── walkdir crate ───────────────────
WalkDir::new(path)                 Crear walker
  .min_depth(n)                    Saltar primeros n niveles
  .max_depth(n)                    No descender más de n niveles
  .follow_links(true)              Seguir symlinks (detecta ciclos)
  .sort_by_file_name()             Ordenar por nombre
  .sort_by(|a, b| cmp)            Ordenar personalizado
  .contents_first(true)            Archivos antes que dirs
  .into_iter()                     Iterador (necesario para filter_*)

──────────────────── Filtrado ────────────────────────
.filter_entry(|e| bool)            Filtrar ANTES de descender
                                   (excluir dirs completos)
.filter_map(|e| e.ok())            Ignorar errores
.filter(|e| predicate)             Filtrar entries individuales

──────────────────── walkdir::DirEntry ───────────────
entry.path()                       &Path
entry.into_path()                  PathBuf (consume entry)
entry.file_name()                  &OsStr
entry.file_type()                  FileType
entry.metadata()                   Result<Metadata>
entry.depth()                      usize (0 = raíz)
entry.path_is_symlink()            bool

──────────────────── Recorrido manual ────────────────
Recursivo                          fn walk(dir) { walk(subdir) }
Iterativo DFS                      stack.push/pop (Vec)
Iterativo BFS                      queue.push_back/pop_front (VecDeque)
Detección de ciclos                HashSet<(dev, ino)>

──────────────────── Rendimiento ─────────────────────
file_type()                        Gratis (d_type en Linux)
metadata()                         1 stat() syscall por entry
filter_entry > filter              Evita descender en dirs excluidos
Excluir: target, .git, node_modules  Ahorra miles de stats
into_path() > path().to_path_buf()  Evita clone innecesario
```

---

## 10. Ejercicios

### Ejercicio 1: `du` simplificado

Implementa un comando que calcule el uso de disco por subdirectorio, similar a
`du -sh --max-depth=1`:

```rust
use walkdir::WalkDir;
use std::collections::HashMap;
use std::path::{Path, PathBuf};

fn disk_usage(root: &str) -> std::io::Result<Vec<(PathBuf, u64)>> {
    // TODO:
    // 1. Recorrer el directorio recursivamente
    // 2. Para cada archivo, sumar su tamaño al directorio padre
    //    inmediato (hijo directo de root)
    // 3. Archivos directamente en root se suman a root
    // 4. Retornar una lista de (directorio, tamaño_total)
    //    ordenada de mayor a menor
    //
    // Ejemplo para root = "/home/user/project":
    //   src/        15.2 MB
    //   target/    450.0 MB
    //   tests/       2.1 MB
    //   (root)       0.5 MB  (archivos en la raíz: Cargo.toml, etc.)

    todo!()
}

fn format_size(bytes: u64) -> String {
    if bytes >= 1_073_741_824 {
        format!("{:.1} GB", bytes as f64 / 1_073_741_824.0)
    } else if bytes >= 1_048_576 {
        format!("{:.1} MB", bytes as f64 / 1_048_576.0)
    } else if bytes >= 1024 {
        format!("{:.1} KB", bytes as f64 / 1024.0)
    } else {
        format!("{} B", bytes)
    }
}

fn main() -> std::io::Result<()> {
    let root = std::env::args().nth(1).unwrap_or_else(|| ".".into());
    let usage = disk_usage(&root)?;

    let total: u64 = usage.iter().map(|(_, s)| s).sum();

    for (dir, size) in &usage {
        let pct = (*size as f64 / total as f64) * 100.0;
        println!("{:>10} {:5.1}%  {}",
            format_size(*size), pct, dir.display());
    }
    println!("{:>10}        TOTAL", format_size(total));

    Ok(())
}
```

**Pregunta de reflexión**: `metadata().len()` retorna el tamaño del archivo, no el
espacio en disco (que se redondea a bloques de 4KB). ¿Cómo obtendrías el tamaño
real en disco? (Hint: en Unix, `MetadataExt::blocks() * 512`). ¿En qué casos
`len()` y el tamaño en disco difieren significativamente? (sparse files, small files).

### Ejercicio 2: Buscador grep-like recursivo

Implementa un buscador que encuentre líneas que contengan un patrón en archivos
de texto dentro de un directorio:

```rust
use walkdir::WalkDir;
use std::fs::File;
use std::io::{self, BufRead, BufReader};
use std::path::Path;

struct Match {
    path: std::path::PathBuf,
    line_number: usize,
    line: String,
}

fn search_files(root: &str, pattern: &str, extensions: &[&str]) -> Vec<Match> {
    // TODO:
    // 1. Recorrer recursivamente con walkdir
    // 2. Excluir .git, target, node_modules con filter_entry
    // 3. Solo procesar archivos con las extensiones dadas
    //    (si extensions está vacío, procesar todos los archivos)
    // 4. Leer cada archivo línea por línea con BufReader
    // 5. Si la línea contiene `pattern` (case-insensitive),
    //    añadir un Match con path, número de línea y contenido
    // 6. Ignorar archivos que no se pueden leer (binarios, permisos)
    //    Hint: si read_line falla o produce bytes non-UTF-8, skip
    // 7. Limitar a máximo 1000 matches para no saturar memoria

    todo!()
}

fn main() {
    let root = std::env::args().nth(1).unwrap_or_else(|| ".".into());
    let pattern = std::env::args().nth(2).unwrap_or_else(|| "TODO".into());

    let matches = search_files(&root, &pattern, &["rs", "toml", "md"]);

    for m in &matches {
        println!("{}:{}:{}", m.path.display(), m.line_number, m.line.trim());
    }
    println!("\n{} matches found", matches.len());
}
```

**Pregunta de reflexión**: ¿cómo distingues un archivo de texto de un archivo binario
antes de intentar leerlo? (Hint: leer los primeros bytes y verificar si contienen
bytes NUL `0x00`). ¿Es confiable verificar solo por extensión?

### Ejercicio 3: Detector de directorios vacíos

Implementa una herramienta que encuentre directorios vacíos (o que solo contienen
otros directorios vacíos recursivamente):

```rust
use std::path::{Path, PathBuf};

fn find_empty_dirs(root: &str) -> Vec<PathBuf> {
    // TODO:
    // Un directorio está "vacío" si:
    //   a) No tiene ningún entry, O
    //   b) Solo contiene subdirectorios que están todos "vacíos"
    //
    // Ejemplo:
    //   project/
    //   ├── src/          ← NO vacío (tiene main.rs)
    //   │   └── main.rs
    //   ├── tests/        ← Vacío (solo contiene empty_dir/)
    //   │   └── empty_dir/ ← Vacío (sin entries)
    //   └── build/        ← Vacío (sin entries)
    //
    // Resultado: [tests/empty_dir/, tests/, build/]
    // (tests/ es vacío porque su único hijo empty_dir/ es vacío)
    //
    // Hint: procesar bottom-up (contents_first) o post-order
    // para que los hijos se evalúen antes que los padres

    todo!()
}

fn main() {
    let root = std::env::args().nth(1).unwrap_or_else(|| ".".into());
    let empty = find_empty_dirs(&root);

    if empty.is_empty() {
        println!("No empty directories found");
    } else {
        println!("Empty directories:");
        for dir in &empty {
            println!("  {}", dir.display());
        }
        println!("\nFound {} empty directories", empty.len());
        println!("To remove: rm -rf <dir>  (or implement --delete flag)");
    }
}
```

**Pregunta de reflexión**: si implementas un flag `--delete` para eliminar los directorios
vacíos, ¿en qué orden debes eliminarlos? ¿Qué pasa si eliminas `tests/` antes de
`tests/empty_dir/`? ¿Y si otro proceso crea un archivo en `tests/` entre tu verificación
y la eliminación?