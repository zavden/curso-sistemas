# Rendimiento: buffered I/O, memmap y paralelismo con rayon

## Índice

1. [Rendimiento en herramientas CLI](#rendimiento-en-herramientas-cli)
2. [Buffered I/O: reducir syscalls](#buffered-io-reducir-syscalls)
3. [Memory-mapped files (mmap)](#memory-mapped-files-mmap)
4. [Paralelismo con rayon](#paralelismo-con-rayon)
5. [Optimización del matching](#optimización-del-matching)
6. [Medir y comparar: benchmarks](#medir-y-comparar-benchmarks)
7. [Perfil de minigrep: de lento a rápido](#perfil-de-minigrep-de-lento-a-rápido)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Rendimiento en herramientas CLI

Una herramienta grep que tarda 5 segundos en buscar en un proyecto de 10,000
archivos no es útil. `ripgrep` busca en el kernel de Linux (70,000+ archivos)
en menos de un segundo. ¿Cómo?

```
┌──────────────────────────────────────────────────────────────┐
│  Capas de optimización de una herramienta de búsqueda        │
│                                                              │
│  ┌──────────────────────────────────────────┐               │
│  │  4. Paralelismo (rayon)                  │  Múltiples    │
│  │     Buscar en varios archivos a la vez   │  cores        │
│  ├──────────────────────────────────────────┤               │
│  │  3. I/O eficiente (mmap, buffers)        │  Menos        │
│  │     Reducir copias y syscalls            │  syscalls     │
│  ├──────────────────────────────────────────┤               │
│  │  2. Motor de regex optimizado            │  Menos        │
│  │     regex crate con Aho-Corasick, DFA    │  CPU          │
│  ├──────────────────────────────────────────┤               │
│  │  1. Evitar trabajo innecesario           │  Menos        │
│  │     Filtrar archivos, skip binarios      │  archivos     │
│  └──────────────────────────────────────────┘               │
│                                                              │
│  Impacto típico:                                             │
│  Capa 1: 2-10x (no buscar donde no hay que buscar)           │
│  Capa 2: 2-5x (el crate regex ya es excelente)              │
│  Capa 3: 2-3x (mmap vs read, BufWriter)                     │
│  Capa 4: 2-8x (según número de cores)                       │
│  Combinado: 10-100x respecto a la versión naive              │
└──────────────────────────────────────────────────────────────┘
```

---

## Buffered I/O: reducir syscalls

Cada llamada a `read()` o `write()` es una **syscall**: el programa pasa del
modo usuario al modo kernel y vuelve. Esto cuesta ~1-10 microsegundos por
llamada. Con millones de líneas, el overhead es significativo.

### El problema: I/O sin buffer

```rust
use std::io::{self, Read, Write};
use std::fs::File;

// ❌ Sin buffer: una syscall write() por cada println!
fn search_unbuffered(path: &str, pattern: &str) -> io::Result<()> {
    let content = std::fs::read_to_string(path)?;  // Una read() grande (OK)
    for line in content.lines() {
        if line.contains(pattern) {
            println!("{}", line);  // write() syscall POR CADA LÍNEA
        }
    }
    Ok(())
}
```

```bash
# Verificar con strace
strace -c ./target/release/minigrep "fn" src/main.rs 2>&1 | grep write
# Sin BufWriter: miles de write() calls
# Con BufWriter: decenas de write() calls
```

### Solución: BufWriter para la salida

```rust
use std::io::{self, BufWriter, Write};

fn search_buffered(path: &str, pattern: &str) -> io::Result<()> {
    let content = std::fs::read_to_string(path)?;
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    for line in content.lines() {
        if line.contains(pattern) {
            writeln!(writer, "{}", line)?;
            // Escribe al buffer interno (8KB por defecto)
            // Solo hace write() syscall cuando el buffer está lleno
        }
    }
    // Flush automático al hacer drop del BufWriter
    Ok(())
}
```

### BufReader para la entrada

```rust
use std::io::{BufRead, BufReader};
use std::fs::File;

// ✅ BufReader: lee 8KB a la vez del disco, luego sirve líneas del buffer
fn search_file_buffered(path: &str, pattern: &str) -> io::Result<Vec<String>> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);  // Buffer de 8KB
    let mut matches = Vec::new();

    for line in reader.lines() {
        let line = line?;
        if line.contains(pattern) {
            matches.push(line);
        }
    }
    Ok(matches)
}

// Con buffer personalizado (para archivos muy grandes)
fn search_big_file(path: &str, pattern: &str) -> io::Result<Vec<String>> {
    let file = File::open(path)?;
    let reader = BufReader::with_capacity(64 * 1024, file);  // 64KB buffer
    // ...
    todo!()
}
```

### Comparación de rendimiento

```
┌──────────────────────────────────────────────────────────────┐
│  Benchmark: buscar en archivo de 1M líneas                   │
│                                                              │
│  Método                    │ Tiempo   │ write() syscalls     │
│  ──────────────────────────┼──────────┼─────────────────     │
│  println! (sin buffer)     │ 850ms    │ ~50,000              │
│  BufWriter (8KB default)   │ 120ms    │ ~200                 │
│  BufWriter (64KB)          │ 110ms    │ ~25                  │
│  write_all en un String    │ 95ms     │ 1                    │
│  ──────────────────────────┼──────────┼─────────────────     │
│  Mejora: ~7x solo con BufWriter                              │
└──────────────────────────────────────────────────────────────┘
```

### Patrón completo: stdout con lock + BufWriter

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let cli = Cli::parse();

    // Lock stdout UNA vez, envolver en BufWriter
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    // Toda la escritura pasa por writer
    run(&cli, &mut writer)?;

    // Flush explícito antes de que se destruya writer
    writer.flush()?;
    Ok(())
}
```

Por qué `stdout.lock()`:
- `println!` hace lock/unlock en CADA llamada
- `stdout.lock()` toma el lock una vez; las escrituras posteriores no tienen
  overhead de locking

---

## Memory-mapped files (mmap)

Memory mapping permite al programa acceder al contenido de un archivo como
si fuera un slice de bytes en memoria, sin copiarlo explícitamente.

### ¿Cómo funciona mmap?

```
┌──────────────────────────────────────────────────────────────┐
│  read() convencional:                                        │
│  ┌────────┐  read()  ┌────────┐  copy  ┌────────────┐      │
│  │ Disco  │ ───────▶ │ Kernel │ ─────▶ │ User buffer│      │
│  └────────┘  syscall └────────┘        └────────────┘      │
│  Datos se copian: disco → kernel buffer → user buffer        │
│                                                              │
│  mmap:                                                       │
│  ┌────────┐  mmap()  ┌─────────────────────────────┐       │
│  │ Disco  │ ───────▶ │ Memoria virtual del proceso │       │
│  └────────┘          │ (page cache del kernel)      │       │
│                      └─────────────────────────────┘       │
│  El kernel mapea las páginas del archivo directamente       │
│  en el espacio de direcciones del proceso.                   │
│  No hay copia extra. El acceso genera page faults que       │
│  cargan datos del disco bajo demanda.                       │
└──────────────────────────────────────────────────────────────┘
```

### El crate memmap2

```toml
# Cargo.toml
[dependencies]
memmap2 = "0.9"
```

```rust
use memmap2::Mmap;
use std::fs::File;

fn search_mmap(path: &str, pattern: &str) -> io::Result<Vec<String>> {
    let file = File::open(path)?;
    let mmap = unsafe { Mmap::map(&file)? };
    // mmap ahora es un &[u8] que apunta al contenido del archivo

    let content = std::str::from_utf8(&mmap)
        .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;

    let matches: Vec<String> = content
        .lines()
        .filter(|line| line.contains(pattern))
        .map(|s| s.to_string())
        .collect();

    Ok(matches)
}
```

### ¿Por qué `unsafe`?

```rust
// Mmap::map es unsafe porque:
// 1. Si otro proceso modifica el archivo mientras lo lees → datos inconsistentes
// 2. Si el archivo se trunca → SIGBUS (acceso a memoria no mapeada)
// 3. No hay protección contra estas condiciones a nivel de tipo

// Para minigrep esto es aceptable:
// - Solo leemos archivos, no los modificamos
// - Si un archivo cambia durante la búsqueda, el peor caso es
//   un resultado inconsistente (no UB de Rust)
// - SIGBUS es manejable pero raro en la práctica
```

### Cuándo usar mmap vs read

```
┌──────────────────────────────────────────────────────────────┐
│  Usar mmap cuando:                                           │
│  ✅ Archivos grandes (> 1MB)                                 │
│  ✅ Solo lees el archivo (no modificas)                      │
│  ✅ No necesitas todo el contenido (acceso parcial/random)   │
│  ✅ Múltiples pases sobre el mismo archivo                   │
│                                                              │
│  Usar read / BufReader cuando:                               │
│  ✅ Archivos pequeños (< 1MB)                                │
│  ✅ Lectura secuencial una sola vez                          │
│  ✅ Necesitas portabilidad máxima                            │
│  ✅ Stdin (no se puede mapear un pipe)                       │
│  ✅ El archivo puede cambiar durante la lectura              │
│                                                              │
│  Rendimiento típico:                                         │
│  ┌──────────┬──────────┬──────────┬────────────┐            │
│  │ Método   │ 1KB      │ 10MB     │ 1GB        │            │
│  ├──────────┼──────────┼──────────┼────────────┤            │
│  │ read_to  │ 0.01ms   │ 25ms     │ 2500ms     │            │
│  │ _string  │          │          │            │            │
│  ├──────────┼──────────┼──────────┼────────────┤            │
│  │ BufReader│ 0.01ms   │ 20ms     │ 2000ms     │            │
│  ├──────────┼──────────┼──────────┼────────────┤            │
│  │ mmap     │ 0.02ms   │ 15ms     │ 800ms      │            │
│  │          │ (overhead│ (ventaja │ (gran      │            │
│  │          │ de setup)│ moderada)│ ventaja)   │            │
│  └──────────┴──────────┴──────────┴────────────┘            │
│  mmap brilla con archivos grandes: evita la copia al        │
│  user space y deja que el kernel gestione la paginación      │
└──────────────────────────────────────────────────────────────┘
```

### Estrategia adaptativa

```rust
use memmap2::Mmap;
use std::fs::{self, File};
use std::io::{self, BufRead, BufReader};
use std::path::Path;

const MMAP_THRESHOLD: u64 = 1024 * 1024;  // 1MB

/// Choose the best strategy based on file size
pub fn read_file_contents(path: &Path) -> io::Result<FileContents> {
    let metadata = fs::metadata(path)?;
    let file_size = metadata.len();

    if file_size >= MMAP_THRESHOLD {
        // Large file: use mmap
        let file = File::open(path)?;
        let mmap = unsafe { Mmap::map(&file)? };
        Ok(FileContents::Mapped(mmap))
    } else {
        // Small file: read into String
        let content = fs::read_to_string(path)?;
        Ok(FileContents::Owned(content))
    }
}

pub enum FileContents {
    Owned(String),
    Mapped(Mmap),
}

impl FileContents {
    pub fn as_str(&self) -> Result<&str, std::str::Utf8Error> {
        match self {
            FileContents::Owned(s) => Ok(s),
            FileContents::Mapped(m) => std::str::from_utf8(m),
        }
    }
}
```

---

## Paralelismo con rayon

Cuando buscas en miles de archivos, cada archivo es independiente. Puedes
buscar en varios archivos simultáneamente usando todos los cores del CPU.

### El crate rayon

```toml
# Cargo.toml
[dependencies]
rayon = "1"
```

rayon convierte iteradores secuenciales en paralelos con un cambio mínimo:

```rust
use rayon::prelude::*;

// Secuencial
let results: Vec<_> = files.iter()
    .map(|f| search_file(&matcher, f))
    .collect();

// Paralelo (cambiar .iter() por .par_iter())
let results: Vec<_> = files.par_iter()
    .map(|f| search_file(&matcher, f))
    .collect();
```

### Búsqueda paralela en minigrep

```rust
use rayon::prelude::*;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};

/// Search multiple files in parallel
pub fn search_files_parallel(
    matcher: &Matcher,
    files: &[PathBuf],
    max_count: Option<usize>,
) -> Vec<Result<SearchResult, (PathBuf, io::Error)>> {
    files.par_iter()
        .map(|path| {
            match search_file(matcher, path, max_count) {
                Ok(result) => Ok(result),
                Err(e) => Err((path.clone(), e)),
            }
        })
        .collect()
}
```

### El problema: orden de salida

Con paralelismo, los resultados llegan en orden arbitrario. grep siempre
imprime en el orden en que se especificaron los archivos.

```rust
/// Search in parallel but maintain output order
pub fn search_files_ordered(
    matcher: &Matcher,
    files: &[PathBuf],
    max_count: Option<usize>,
    silent: bool,
) -> MultiSearchResult {
    // Buscar en paralelo: cada resultado tiene su índice
    let mut indexed_results: Vec<(usize, Result<SearchResult, (PathBuf, io::Error)>)> =
        files.par_iter()
            .enumerate()
            .map(|(idx, path)| {
                let result = search_file(matcher, path, max_count)
                    .map_err(|e| (path.clone(), e));
                (idx, result)
            })
            .collect();

    // Ordenar por índice original
    indexed_results.sort_by_key(|(idx, _)| *idx);

    // Separar resultados exitosos de errores
    let mut results = Vec::new();
    let mut errors = Vec::new();

    for (_, result) in indexed_results {
        match result {
            Ok(r) => results.push(r),
            Err((path, e)) => {
                if !silent {
                    eprintln!("minigrep: {}: {}", path.display(), e);
                }
                errors.push((path, e));
            }
        }
    }

    MultiSearchResult { results, errors }
}
```

### Controlar el número de threads

```rust
use rayon::ThreadPoolBuilder;

fn main() {
    // Usar la mitad de los cores disponibles
    let num_threads = num_cpus::get() / 2;

    ThreadPoolBuilder::new()
        .num_threads(num_threads)
        .build_global()
        .unwrap();

    // Ahora par_iter() usa solo num_threads threads
}
```

### Cuándo NO paralelizar

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Vale la pena paralelizar?                                  │
│                                                              │
│  ✅ Sí: muchos archivos, CPU-bound (regex compleja)          │
│  ✅ Sí: archivos en SSD (I/O paralelo es eficiente)          │
│  ✅ Sí: búsqueda recursiva en proyectos grandes              │
│                                                              │
│  ❌ No: un solo archivo (nada que paralelizar)               │
│  ❌ No: stdin (fuente secuencial)                            │
│  ❌ No: disco HDD lento (I/O es el cuello, no CPU)           │
│  ❌ No: pocos archivos (overhead de threads > beneficio)     │
│  ❌ No: pattern muy simple (I/O bound, no CPU bound)         │
└──────────────────────────────────────────────────────────────┘
```

### Decisión adaptativa

```rust
const PARALLEL_THRESHOLD: usize = 16;  // Mínimo de archivos para paralelizar

pub fn search_files_smart(
    matcher: &Matcher,
    files: &[PathBuf],
    max_count: Option<usize>,
    silent: bool,
) -> MultiSearchResult {
    if files.len() >= PARALLEL_THRESHOLD {
        search_files_ordered(matcher, files, max_count, silent)
    } else {
        search_files_sequential(matcher, files, max_count, silent)
    }
}
```

---

## Optimización del matching

### Evitar alocaciones innecesarias

```rust
// ❌ Cada línea se convierte en String (alocación en heap)
for line_result in reader.lines() {
    let line = line_result?;  // String alocada
    if matcher.is_match(&line) {
        results.push(line);   // Otra alocación al guardar
    }
}

// ✅ Leer en un buffer reutilizable
let mut buf = String::new();
loop {
    buf.clear();  // Reutilizar la misma String
    let bytes_read = reader.read_line(&mut buf)?;
    if bytes_read == 0 {
        break;  // EOF
    }
    let line = buf.trim_end_matches('\n');
    if matcher.is_match(line) {
        results.push(MatchResult {
            line: line.to_string(),  // Solo alocar si es match
            // ...
        });
    }
}
```

### Búsqueda por bytes en vez de por líneas

Para archivos grandes, buscar byte por byte puede ser más rápido que
`lines()` porque evitas la conversión a `&str` de líneas que no necesitas:

```rust
use regex::bytes::Regex as BytesRegex;

/// Search using byte-level regex (no UTF-8 validation per line)
pub fn search_bytes(pattern: &str, data: &[u8]) -> Vec<(usize, Vec<u8>)> {
    let re = BytesRegex::new(pattern).unwrap();
    let mut results = Vec::new();
    let mut line_number = 0;

    for line in data.split(|&b| b == b'\n') {
        line_number += 1;
        if re.is_match(line) {
            results.push((line_number, line.to_vec()));
        }
    }

    results
}
```

`regex::bytes::Regex` trabaja directamente con `&[u8]` sin requerir UTF-8
válido, lo cual es importante para archivos que pueden contener bytes
inválidos.

### Pre-filtro con memchr

El crate `memchr` busca bytes individuales usando instrucciones SIMD,
mucho más rápido que iterar byte a byte:

```toml
[dependencies]
memchr = "2"
```

```rust
use memchr::memmem;

/// Fast literal pre-filter: skip lines that can't possibly match
pub fn search_with_prefilter(
    literal_prefix: &str,
    regex: &Regex,
    content: &str,
) -> Vec<(usize, String)> {
    let finder = memmem::Finder::new(literal_prefix.as_bytes());
    let mut results = Vec::new();

    for (idx, line) in content.lines().enumerate() {
        // Pre-filter: if the literal isn't present, skip the regex
        if finder.find(line.as_bytes()).is_none() {
            continue;
        }
        // Full regex check only if pre-filter passes
        if regex.is_match(line) {
            results.push((idx + 1, line.to_string()));
        }
    }

    results
}
```

El crate `regex` internamente ya usa `memchr` para literales extraídos del
patrón. Pero si conoces un literal que siempre está presente en el match,
puedes hacer el pre-filtro explícitamente.

### Aho-Corasick para múltiples patrones

Si necesitas buscar múltiples patrones a la vez (como `grep -e pat1 -e pat2`):

```toml
[dependencies]
aho-corasick = "1"
```

```rust
use aho_corasick::AhoCorasick;

fn search_multiple_patterns(patterns: &[&str], content: &str) -> Vec<(usize, String)> {
    let ac = AhoCorasick::new(patterns).unwrap();
    let mut results = Vec::new();

    for (idx, line) in content.lines().enumerate() {
        if ac.is_match(line) {
            results.push((idx + 1, line.to_string()));
        }
    }

    results
}
```

Aho-Corasick busca todos los patrones simultáneamente en un solo pase,
con complejidad O(n + m) donde n es el texto y m es la longitud total de
los patrones.

---

## Medir y comparar: benchmarks

### hyperfine: benchmarks de CLIs

```bash
# Instalar
cargo install hyperfine
# o
sudo dnf install hyperfine  # Fedora

# Comparar minigrep con grep
hyperfine \
    'grep -rn "fn main" src/' \
    './target/release/minigrep -rn "fn main" src/'

# Con warmup (para que el file cache esté caliente)
hyperfine --warmup 3 \
    'grep -rn "fn main" src/' \
    './target/release/minigrep -rn "fn main" src/'

# Comparar versiones de minigrep
hyperfine --warmup 3 \
    './minigrep_v1 -rn "pattern" big_project/' \
    './minigrep_v2 -rn "pattern" big_project/' \
    --export-markdown benchmark.md
```

Output típico:

```
Benchmark 1: grep -rn "fn main" src/
  Time (mean ± σ):      23.4 ms ±   1.2 ms    [User: 18.1 ms, System: 5.1 ms]
  Range (min … max):    21.8 ms …  27.3 ms    50 runs

Benchmark 2: ./target/release/minigrep -rn "fn main" src/
  Time (mean ± σ):      31.2 ms ±   2.1 ms    [User: 25.3 ms, System: 5.7 ms]
  Range (min … max):    28.1 ms …  36.5 ms    50 runs

Summary
  grep -rn "fn main" src/ ran
    1.33 ± 0.11 times faster than ./target/release/minigrep -rn "fn main" src/
```

### criterion: benchmarks dentro de Rust

```rust
// benches/search_bench.rs
use criterion::{criterion_group, criterion_main, Criterion, BenchmarkId};
use std::io::Cursor;

fn bench_search_methods(c: &mut Criterion) {
    // Generate test data
    let content: String = (0..10_000)
        .map(|i| {
            if i % 100 == 0 {
                format!("fn function_{}() {{}}\n", i)
            } else {
                format!("let x_{} = {};\n", i, i)
            }
        })
        .collect();

    let mut group = c.benchmark_group("search");

    group.bench_function("literal_contains", |b| {
        b.iter(|| {
            content.lines()
                .filter(|line| line.contains("fn function"))
                .count()
        })
    });

    group.bench_function("regex", |b| {
        let re = regex::Regex::new(r"fn function_\d+").unwrap();
        b.iter(|| {
            content.lines()
                .filter(|line| re.is_match(line))
                .count()
        })
    });

    group.bench_function("regex_bytes", |b| {
        let re = regex::bytes::Regex::new(r"fn function_\d+").unwrap();
        let bytes = content.as_bytes();
        b.iter(|| {
            bytes.split(|&b| b == b'\n')
                .filter(|line| re.is_match(line))
                .count()
        })
    });

    group.finish();
}

criterion_group!(benches, bench_search_methods);
criterion_main!(benches);
```

---

## Perfil de minigrep: de lento a rápido

### Versión 1: naive (baseline)

```rust
// La versión más simple posible
fn search_v1(pattern: &str, paths: &[PathBuf]) -> io::Result<()> {
    let re = Regex::new(pattern)?;

    for path in paths {
        let content = fs::read_to_string(path)?;   // Todo a memoria
        for (idx, line) in content.lines().enumerate() {
            if re.is_match(line) {
                println!("{}:{}:{}", path.display(), idx + 1, line);
                // println! = lock + write + unlock cada vez
            }
        }
    }
    Ok(())
}
```

### Versión 2: BufWriter (+3-7x en output pesado)

```rust
fn search_v2(pattern: &str, paths: &[PathBuf]) -> io::Result<()> {
    let re = Regex::new(pattern)?;
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());  // ← cambio

    for path in paths {
        let content = fs::read_to_string(path)?;
        for (idx, line) in content.lines().enumerate() {
            if re.is_match(line) {
                writeln!(writer, "{}:{}:{}", path.display(), idx + 1, line)?;
            }
        }
    }
    Ok(())
}
```

### Versión 3: mmap para archivos grandes (+1.5-2x en archivos >1MB)

```rust
fn search_v3(pattern: &str, paths: &[PathBuf]) -> io::Result<()> {
    let re = Regex::new(pattern)?;
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    for path in paths {
        let contents = read_file_contents(path)?;  // ← mmap si > 1MB
        let text = contents.as_str()
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e))?;

        for (idx, line) in text.lines().enumerate() {
            if re.is_match(line) {
                writeln!(writer, "{}:{}:{}", path.display(), idx + 1, line)?;
            }
        }
    }
    Ok(())
}
```

### Versión 4: paralelismo con rayon (+2-8x según cores)

```rust
fn search_v4(pattern: &str, paths: &[PathBuf]) -> io::Result<()> {
    let re = Regex::new(pattern)?;

    // Buscar en paralelo, recoger resultados
    let all_results: Vec<(PathBuf, Vec<(usize, String)>)> = paths
        .par_iter()
        .filter_map(|path| {
            let contents = read_file_contents(path).ok()?;
            let text = contents.as_str().ok()?;

            let matches: Vec<(usize, String)> = text.lines()
                .enumerate()
                .filter(|(_, line)| re.is_match(line))
                .map(|(idx, line)| (idx + 1, line.to_string()))
                .collect();

            if matches.is_empty() {
                None
            } else {
                Some((path.clone(), matches))
            }
        })
        .collect();

    // Imprimir secuencialmente (mantener orden)
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    for (path, matches) in &all_results {
        for (line_num, line) in matches {
            writeln!(writer, "{}:{}:{}", path.display(), line_num, line)?;
        }
    }

    Ok(())
}
```

### Versión 5: buffer reutilizable (+10-20% en muchos archivos pequeños)

```rust
fn search_v5(pattern: &str, paths: &[PathBuf]) -> io::Result<()> {
    let re = Regex::new(pattern)?;
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    for path in paths {
        let file = File::open(path)?;
        let reader = BufReader::new(file);
        let mut line_buf = String::new();
        let mut line_number = 0usize;

        let path_display = path.display().to_string();

        loop {
            line_buf.clear();  // Reutilizar la misma alocación
            let bytes = reader.read_line(&mut line_buf)?;
            if bytes == 0 { break; }
            line_number += 1;

            let line = line_buf.trim_end();
            if re.is_match(line) {
                writeln!(writer, "{}:{}:{}", path_display, line_number, line)?;
            }
        }
    }
    Ok(())
}
```

### Resumen de optimizaciones

```
┌──────────────────────────────────────────────────────────────┐
│  Versión  │ Optimización              │ Speedup acumulado   │
├───────────┼───────────────────────────┼─────────────────────┤
│  v1       │ Baseline (println!)       │ 1x                  │
│  v2       │ + BufWriter               │ ~5x                 │
│  v3       │ + mmap para grandes       │ ~7x                 │
│  v4       │ + rayon (4 cores)         │ ~20x                │
│  v5       │ + buffer reutilizable     │ ~22x                │
│           │                           │                     │
│  ripgrep  │ + SIMD, ignore, DFA,     │ ~50-100x vs v1      │
│           │   parallel directory walk│                     │
└───────────┴───────────────────────────┴─────────────────────┘
```

---

## Errores comunes

### 1. Paralelizar la salida en vez de la búsqueda

```rust
// ❌ Output entrelazado: las líneas de diferentes archivos se mezclan
files.par_iter().for_each(|path| {
    let content = fs::read_to_string(path).unwrap();
    for line in content.lines() {
        if re.is_match(line) {
            println!("{}", line);  // Cada thread escribe a stdout
            // → las líneas se entrelazan caóticamente
        }
    }
});

// ✅ Buscar en paralelo, imprimir secuencialmente
let results: Vec<_> = files.par_iter()
    .map(|path| search_file(&matcher, path))
    .collect();

// Imprimir en orden
for result in results {
    print_results(&result);
}
```

### 2. Usar mmap para stdin

```rust
// ❌ No puedes hacer mmap de stdin (es un pipe, no un archivo)
let mmap = unsafe { Mmap::map(&io::stdin()) };  // Error

// ✅ BufReader para stdin, mmap solo para archivos
if paths.is_empty() {
    let reader = BufReader::new(io::stdin().lock());
    search_reader(&matcher, reader);
} else {
    for path in paths {
        let mmap = unsafe { Mmap::map(&File::open(path)?)? };
        search_bytes(&matcher, &mmap);
    }
}
```

### 3. Optimizar antes de medir

```rust
// ❌ "Voy a usar mmap y SIMD y un pool de threads custom"
// Sin saber dónde está el cuello de botella real

// ✅ Proceso correcto:
// 1. Implementar la versión simple
// 2. Medir con hyperfine/perf stat
// 3. Identificar el cuello de botella (CPU? I/O? syscalls?)
// 4. Aplicar la optimización correcta
// 5. Medir de nuevo
```

### 4. No compilar en release para benchmarks

```bash
# ❌ Benchmark en debug: resultados no representativos
cargo run -- -rn "pattern" big_project/
# Debug: 5 segundos (bounds checks, sin inlining, sin SIMD)

# ✅ Siempre release para benchmarks
cargo run --release -- -rn "pattern" big_project/
# Release: 0.2 segundos
```

### 5. Olvidar flush en BufWriter

```rust
// ❌ Si el programa hace exit() o panic, BufWriter no flushea
let mut writer = BufWriter::new(stdout.lock());
writeln!(writer, "last line")?;
std::process::exit(0);  // ← BufWriter no hace flush → "last line" se pierde

// ✅ Flush explícito antes de exit
writer.flush()?;
std::process::exit(0);

// ✅ O dejar que el drop haga el flush (no usar process::exit)
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│             RENDIMIENTO CLI CHEATSHEET                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  BUFFERED I/O                                                │
│  BufWriter::new(stdout.lock())       Buffer de escritura     │
│  BufReader::new(file)                Buffer de lectura       │
│  BufReader::with_capacity(64*1024,f) Buffer custom           │
│  writer.flush()?                     Flush explícito         │
│  stdout.lock()                       Lock una sola vez       │
│                                                              │
│  MMAP                                                        │
│  memmap2 = "0.9"                     Cargo.toml              │
│  let mmap = unsafe { Mmap::map(&f)?};                        │
│  &mmap[..] → &[u8]                  Acceso como slice       │
│  Usar para archivos > 1MB                                    │
│  No usar para stdin (pipes)                                  │
│                                                              │
│  RAYON                                                       │
│  rayon = "1"                         Cargo.toml              │
│  use rayon::prelude::*;                                      │
│  .iter() → .par_iter()              Paralelizar             │
│  Buscar en paralelo, imprimir en orden                      │
│  ThreadPoolBuilder::new()                                    │
│    .num_threads(N).build_global()    Limitar threads         │
│                                                              │
│  MATCHING RÁPIDO                                             │
│  regex::bytes::Regex                 Sin validar UTF-8       │
│  memchr = "2"                        Búsqueda SIMD          │
│  aho_corasick = "1"                  Múltiples patrones     │
│  read_line(&mut buf) + buf.clear()   Reutilizar buffer      │
│                                                              │
│  BENCHMARKS                                                  │
│  cargo install hyperfine             CLI benchmark tool      │
│  hyperfine --warmup 3 'cmd1' 'cmd2'  Comparar               │
│  criterion (crate)                   Micro-benchmarks Rust   │
│  perf stat -r 5 ./program            Contadores HW          │
│                                                              │
│  ORDEN DE OPTIMIZACIÓN                                       │
│  1. Compilar en --release                                    │
│  2. BufWriter para stdout                                    │
│  3. Evitar alocaciones en el loop interno                    │
│  4. mmap para archivos grandes                               │
│  5. Paralelismo con rayon                                    │
│  6. Pre-filtro con memchr (si aplica)                        │
│                                                              │
│  REGLAS DE ORO                                               │
│  • Medir antes de optimizar                                  │
│  • El cuello de botella más grande primero                   │
│  • Siempre benchmarks en release                             │
│  • Buscar en paralelo, imprimir secuencialmente              │
│  • No usar mmap para pipes ni archivos pequeños              │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Medir el impacto de BufWriter

**Tareas:**
1. Crea un programa que imprima 100,000 líneas con `println!`
2. Crea una versión con `BufWriter`
3. Mide ambas con `hyperfine` (o `/usr/bin/time -v`)
4. Ejecuta ambas con `strace -c` y compara el número de syscalls `write`
5. Prueba con diferentes tamaños de buffer: `BufWriter::with_capacity(SIZE, ...)`
   con SIZE = 1KB, 8KB, 64KB, 256KB. ¿Dónde dejan de importar los rendimientos?

```rust
// Versión sin buffer
fn no_buffer() {
    for i in 0..100_000 {
        println!("Line number {} with some padding text to make it realistic", i);
    }
}

// Versión con buffer
fn with_buffer() {
    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());
    for i in 0..100_000 {
        writeln!(writer, "Line number {} with some padding text to make it realistic", i).unwrap();
    }
}
```

**Pregunta de reflexión:** `println!` hace `stdout().lock()` internamente en cada
llamada. ¿Cuánto del overhead es el locking vs las syscalls extra? ¿Cómo
diseñarías un experimento para separar estos dos factores?

---

### Ejercicio 2: Comparar read vs mmap

**Tareas:**
1. Genera un archivo de prueba de 100MB:
   ```bash
   python3 -c "
   import random, string
   for i in range(2_000_000):
       line = ''.join(random.choices(string.ascii_lowercase + ' ', k=50))
       if i % 1000 == 0:
           line = 'fn match_target_' + str(i) + '() {}'
       print(line)
   " > /tmp/big_file.txt
   ```
2. Implementa tres versiones de búsqueda:
   - `search_read_to_string`: `fs::read_to_string` + `.lines().filter()`
   - `search_bufreader`: `BufReader` + `.lines()`
   - `search_mmap`: `Mmap::map` + split por `\n`
3. Mide las tres con `hyperfine --warmup 3`
4. Ejecuta `perf stat` en cada una. Compara page-faults y cache-misses
5. ¿Cuál gana? ¿Cambia el resultado si el archivo está en el page cache
   (segunda ejecución) vs cold (primera ejecución después de `sync; echo 3 > /proc/sys/vm/drop_caches`)?

**Pregunta de reflexión:** mmap no copia datos del kernel al user space, pero sí
genera page faults que el kernel debe resolver. ¿En qué escenario los page faults
de mmap son más costosos que las copias de `read()`?

---

### Ejercicio 3: Búsqueda paralela con rayon

**Tareas:**
1. Genera un directorio con 1,000 archivos:
   ```bash
   mkdir -p /tmp/many_files
   for i in $(seq 1 1000); do
       echo "fn function_$i() { println!(\"hello\"); }" > /tmp/many_files/file_$i.rs
       for j in $(seq 1 100); do
           echo "let x_$j = $j;" >> /tmp/many_files/file_$i.rs
       done
   done
   ```
2. Implementa `search_sequential` (un archivo a la vez) y `search_parallel` (rayon)
3. Mide con `hyperfine` con diferentes números de threads:
   ```bash
   RAYON_NUM_THREADS=1 hyperfine './minigrep -r "fn function" /tmp/many_files/'
   RAYON_NUM_THREADS=2 hyperfine './minigrep -r "fn function" /tmp/many_files/'
   RAYON_NUM_THREADS=4 hyperfine './minigrep -r "fn function" /tmp/many_files/'
   RAYON_NUM_THREADS=8 hyperfine './minigrep -r "fn function" /tmp/many_files/'
   ```
4. ¿A partir de cuántos threads los rendimientos dejan de mejorar?
5. Verifica que el output es **idéntico** (mismo orden) entre la versión secuencial
   y la paralela

**Pregunta de reflexión:** ripgrep paraleliza no solo la búsqueda en archivos sino
también el recorrido del directorio (directory walking). ¿Cuándo sería beneficioso
paralelizar el walking y cuándo sería overhead innecesario?