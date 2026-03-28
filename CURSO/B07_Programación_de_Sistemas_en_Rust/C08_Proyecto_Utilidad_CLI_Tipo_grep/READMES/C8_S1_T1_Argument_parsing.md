# Argument parsing: clap crate para CLIs en Rust

## Índice

1. [Parsing de argumentos en CLIs](#parsing-de-argumentos-en-clis)
2. [std::env::args: el enfoque básico](#stdenvargs-el-enfoque-básico)
3. [clap: el estándar en Rust](#clap-el-estándar-en-rust)
4. [Derive API de clap](#derive-api-de-clap)
5. [Flags, opciones y argumentos posicionales](#flags-opciones-y-argumentos-posicionales)
6. [Validación y valores por defecto](#validación-y-valores-por-defecto)
7. [Subcomandos](#subcomandos)
8. [Diseño de la CLI de minigrep](#diseño-de-la-cli-de-minigrep)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Parsing de argumentos en CLIs

Toda herramienta de línea de comandos necesita parsear sus argumentos. Veamos
cómo funciona en Linux y cómo Rust lo maneja.

```
┌──────────────────────────────────────────────────────────────┐
│  Anatomía de un comando                                      │
│                                                              │
│  $ grep -i -n --color=auto "pattern" file1.txt file2.txt     │
│    ┬─── ┬─ ┬─ ┬──────────── ┬──────── ┬─────── ┬────────   │
│    │    │  │  │              │         │        │            │
│    │    │  │  │              │         └────────┴─ args      │
│    │    │  │  │              │           posicionales        │
│    │    │  │  │              │                               │
│    │    │  │  │              └─ argumento posicional          │
│    │    │  │  │                 (el patrón)                   │
│    │    │  │  │                                               │
│    │    │  │  └─ opción larga con valor (--key=value)         │
│    │    │  │                                                  │
│    │    │  └─ flag corta (-n = show line numbers)             │
│    │    │                                                     │
│    │    └─ flag corta (-i = case insensitive)                 │
│    │                                                          │
│    └─ nombre del programa                                     │
│                                                              │
│  Convenciones POSIX/GNU:                                     │
│  -x          Flag corta (un carácter)                        │
│  -xyz        Múltiples flags cortas combinadas (-x -y -z)    │
│  --long      Flag larga                                      │
│  --key=val   Opción larga con valor                          │
│  --key val   Opción larga con valor (espacio)                │
│  -o val      Opción corta con valor                          │
│  --          Fin de opciones (todo después son posicionales)  │
└──────────────────────────────────────────────────────────────┘
```

---

## std::env::args: el enfoque básico

Rust provee acceso directo a los argumentos de la línea de comandos:

```rust
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();

    // args[0] = nombre del programa
    // args[1..] = argumentos del usuario
    println!("Programa: {}", args[0]);
    println!("Argumentos: {:?}", &args[1..]);
}
```

```bash
$ cargo run -- hello world
Programa: target/debug/my_program
Argumentos: ["hello", "world"]
```

### Parsing manual simple

```rust
use std::env;
use std::process;

struct Config {
    pattern: String,
    filename: String,
    case_insensitive: bool,
    line_numbers: bool,
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();

    let mut pattern = None;
    let mut filename = None;
    let mut case_insensitive = false;
    let mut line_numbers = false;

    let mut i = 1;  // Saltar args[0] (nombre del programa)
    while i < args.len() {
        match args[i].as_str() {
            "-i" | "--ignore-case" => case_insensitive = true,
            "-n" | "--line-number" => line_numbers = true,
            "-h" | "--help" => {
                eprintln!("Usage: minigrep [OPTIONS] PATTERN FILE");
                eprintln!("  -i, --ignore-case  Case insensitive");
                eprintln!("  -n, --line-number  Show line numbers");
                process::exit(0);
            }
            arg if arg.starts_with('-') => {
                eprintln!("Unknown option: {}", arg);
                process::exit(1);
            }
            arg => {
                if pattern.is_none() {
                    pattern = Some(arg.to_string());
                } else if filename.is_none() {
                    filename = Some(arg.to_string());
                } else {
                    eprintln!("Too many arguments");
                    process::exit(1);
                }
            }
        }
        i += 1;
    }

    Config {
        pattern: pattern.unwrap_or_else(|| {
            eprintln!("Missing pattern");
            process::exit(1);
        }),
        filename: filename.unwrap_or_else(|| {
            eprintln!("Missing filename");
            process::exit(1);
        }),
        case_insensitive,
        line_numbers,
    }
}
```

Esto funciona, pero es tedioso y propenso a errores. Para cualquier CLI real,
usa **clap**.

---

## clap: el estándar en Rust

`clap` es el crate más usado para argument parsing en Rust. Maneja
automáticamente:

- Parsing de flags cortas y largas
- Mensajes de ayuda (`--help`)
- Información de versión (`--version`)
- Validación de tipos y valores
- Autocompletado de shell
- Mensajes de error descriptivos

### Instalación

```toml
# Cargo.toml
[dependencies]
clap = { version = "4", features = ["derive"] }
```

La feature `derive` habilita la API declarativa con macros derive, que es
la forma más ergonómica de usar clap.

### Primer ejemplo

```rust
use clap::Parser;

/// A simple grep-like tool
#[derive(Parser)]
#[command(version, about)]
struct Cli {
    /// The pattern to search for
    pattern: String,

    /// The file to search in
    file: String,
}

fn main() {
    let cli = Cli::parse();
    println!("Searching for '{}' in '{}'", cli.pattern, cli.file);
}
```

```bash
$ cargo run -- "hello" test.txt
Searching for 'hello' in 'test.txt'

$ cargo run -- --help
A simple grep-like tool

Usage: minigrep <PATTERN> <FILE>

Arguments:
  <PATTERN>  The pattern to search for
  <FILE>     The file to search in

Options:
  -h, --help     Print help
  -V, --version  Print version

$ cargo run --
error: the following required arguments were not provided:
  <PATTERN>
  <FILE>

Usage: minigrep <PATTERN> <FILE>
```

clap genera automáticamente `--help`, `--version`, y mensajes de error
informativos.

---

## Derive API de clap

La derive API convierte una struct en un parser de argumentos. Cada campo
se convierte en un argumento, y los atributos controlan el comportamiento.

### Estructura general

```rust
use clap::Parser;

/// Documentación que aparece en --help                    ← doc comment = about
#[derive(Parser)]
#[command(name = "minigrep")]                               ← nombre del programa
#[command(version = "1.0")]                                 ← --version
#[command(about = "Search for patterns in files")]          ← --help header
#[command(long_about = "Detailed description...")]          ← --help (largo)
struct Cli {
    /// Pattern doc comment                                 ← help del argumento
    #[arg(...)]                                             ← configuración
    field_name: Type,                                       ← tipo determina parsing
}
```

### Tipos y su efecto en el parsing

```rust
use clap::Parser;

#[derive(Parser)]
struct Cli {
    // String → argumento posicional obligatorio
    pattern: String,

    // Option<String> → argumento posicional opcional
    file: Option<String>,

    // Vec<String> → múltiples valores posicionales
    files: Vec<String>,

    // bool con flag → flag (presente/ausente)
    #[arg(short, long)]
    verbose: bool,

    // Option<T> con flag → opción con valor opcional
    #[arg(short, long)]
    count: Option<usize>,

    // T con default_value → opción con valor por defecto
    #[arg(short, long, default_value = "10")]
    max_results: usize,
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  Tipo Rust          │ Comportamiento en clap                 │
├─────────────────────┼────────────────────────────────────────┤
│ String              │ Obligatorio, toma un valor             │
│ Option<String>      │ Opcional, toma un valor si presente    │
│ Vec<String>         │ Cero o más valores                     │
│ bool (con #[arg])   │ Flag: true si presente, false si no    │
│ usize, i32, etc.    │ Parsea automáticamente como número     │
│ PathBuf             │ Ruta de archivo (validación del OS)     │
│ Option<PathBuf>     │ Ruta opcional                          │
└─────────────────────┴────────────────────────────────────────┘
```

---

## Flags, opciones y argumentos posicionales

### Flags (booleanos)

```rust
use clap::Parser;

#[derive(Parser)]
struct Cli {
    /// Case insensitive search
    #[arg(short, long)]
    ignore_case: bool,
    // Genera: -i, --ignore-case

    /// Show line numbers
    #[arg(short = 'n', long = "line-number")]
    line_numbers: bool,
    // Genera: -n, --line-number (nombres personalizados)

    /// Increase verbosity (-v, -vv, -vvv)
    #[arg(short, long, action = clap::ArgAction::Count)]
    verbose: u8,
    // -v = 1, -vv = 2, -vvv = 3
}
```

### Opciones (con valor)

```rust
#[derive(Parser)]
struct Cli {
    /// Number of context lines
    #[arg(short = 'C', long, default_value = "0")]
    context: usize,
    // --context 3 o --context=3 o -C 3

    /// Output format
    #[arg(short, long, default_value = "text")]
    format: String,
    // --format json o -f json

    /// Maximum number of matches
    #[arg(short, long)]
    max_count: Option<usize>,
    // Opcional: --max-count 10 o ausente (None)

    /// Directories to exclude
    #[arg(long)]
    exclude: Vec<String>,
    // Repetible: --exclude target --exclude .git
}
```

### Argumentos posicionales

```rust
#[derive(Parser)]
struct Cli {
    /// The pattern to search for
    pattern: String,  // Primer posicional (obligatorio)

    /// Files to search in
    files: Vec<PathBuf>,  // Resto de posicionales (cero o más)
}
```

```bash
$ minigrep "pattern" file1.txt file2.txt file3.txt
# pattern = "pattern"
# files = [file1.txt, file2.txt, file3.txt]
```

### Leer de stdin si no hay archivos

```rust
use clap::Parser;
use std::path::PathBuf;

#[derive(Parser)]
struct Cli {
    /// The pattern to search for
    pattern: String,

    /// Files to search (reads stdin if none given)
    files: Vec<PathBuf>,
}

fn main() {
    let cli = Cli::parse();

    if cli.files.is_empty() {
        // Leer de stdin
        search_stdin(&cli.pattern);
    } else {
        // Buscar en archivos
        for file in &cli.files {
            search_file(&cli.pattern, file);
        }
    }
}
```

---

## Validación y valores por defecto

### Validar valores con value_parser

```rust
use clap::Parser;
use std::path::PathBuf;

#[derive(Parser)]
struct Cli {
    /// Number of context lines (0-100)
    #[arg(short = 'C', long, default_value = "0",
          value_parser = clap::value_parser!(u32).range(0..=100))]
    context: u32,

    /// Output file (must be writable path)
    #[arg(short, long)]
    output: Option<PathBuf>,
}
```

### Enums como valores

```rust
use clap::{Parser, ValueEnum};

#[derive(Debug, Clone, ValueEnum)]
enum ColorMode {
    Auto,
    Always,
    Never,
}

#[derive(Parser)]
struct Cli {
    /// When to use colors
    #[arg(long, default_value = "auto", value_enum)]
    color: ColorMode,
}
```

```bash
$ minigrep --color always "pattern" file.txt
$ minigrep --color=never "pattern" file.txt

$ minigrep --color invalid "pattern" file.txt
error: invalid value 'invalid' for '--color <COLOR>'
  [possible values: auto, always, never]
```

### Grupos de argumentos y conflictos

```rust
use clap::Parser;

#[derive(Parser)]
struct Cli {
    /// Count matching lines only
    #[arg(short, long)]
    count: bool,

    /// List files with matches only
    #[arg(short, long, conflicts_with = "count")]
    list_files: bool,
    // --count y --list-files no pueden usarse juntos

    /// Show N lines before match
    #[arg(short = 'B', long)]
    before_context: Option<usize>,

    /// Show N lines after match
    #[arg(short = 'A', long)]
    after_context: Option<usize>,

    /// Show N lines before and after match
    #[arg(short = 'C', long,
          conflicts_with_all = ["before_context", "after_context"])]
    context: Option<usize>,
    // -C no se puede combinar con -A o -B
}
```

### Requerir al menos un argumento de un grupo

```rust
use clap::{Parser, ArgGroup};

#[derive(Parser)]
#[command(group(
    ArgGroup::new("input")
        .required(true)
        .args(["pattern", "pattern_file"]),
))]
struct Cli {
    /// Pattern to search for
    #[arg(short = 'e', long)]
    pattern: Option<String>,

    /// File containing patterns (one per line)
    #[arg(short = 'f', long)]
    pattern_file: Option<PathBuf>,
    // Debe proporcionar -e PATTERN o -f FILE (pero no ambos)
}
```

---

## Subcomandos

Para CLIs más complejas con múltiples modos de operación:

```rust
use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Parser)]
#[command(version, about = "A file search utility")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Search for a pattern in files
    Search {
        /// The pattern to search for
        pattern: String,

        /// Files to search
        files: Vec<PathBuf>,

        /// Case insensitive
        #[arg(short, long)]
        ignore_case: bool,
    },

    /// Count occurrences of a pattern
    Count {
        /// The pattern to count
        pattern: String,

        /// File to search in
        file: PathBuf,
    },

    /// List supported file types
    Types,
}

fn main() {
    let cli = Cli::parse();

    match cli.command {
        Commands::Search { pattern, files, ignore_case } => {
            println!("Searching '{}' in {:?} (case_insensitive: {})",
                     pattern, files, ignore_case);
        }
        Commands::Count { pattern, file } => {
            println!("Counting '{}' in {:?}", pattern, file);
        }
        Commands::Types => {
            println!("Supported: .rs, .txt, .md, .c, .h");
        }
    }
}
```

```bash
$ mytool search -i "pattern" src/main.rs
$ mytool count "TODO" src/lib.rs
$ mytool types
$ mytool --help
$ mytool search --help
```

---

## Diseño de la CLI de minigrep

Ahora diseñamos la CLI completa del proyecto minigrep, que construiremos
en los siguientes temas.

### Definición completa

```rust
use clap::{Parser, ValueEnum};
use std::path::PathBuf;

#[derive(Debug, Clone, ValueEnum)]
enum ColorWhen {
    /// Colorize output automatically (when stdout is a TTY)
    Auto,
    /// Always colorize output
    Always,
    /// Never colorize output
    Never,
}

/// minigrep — search for patterns in files
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
pub struct Cli {
    /// The pattern (regex) to search for
    pub pattern: String,

    /// Files or directories to search
    /// (reads stdin if none given)
    pub paths: Vec<PathBuf>,

    // ── Matching options ──

    /// Case insensitive search
    #[arg(short, long)]
    pub ignore_case: bool,

    /// Match whole words only
    #[arg(short, long)]
    pub word_regexp: bool,

    /// Invert match (show non-matching lines)
    #[arg(short = 'v', long)]
    pub invert_match: bool,

    /// Use fixed strings, not regex
    #[arg(short = 'F', long)]
    pub fixed_strings: bool,

    // ── Output options ──

    /// Show line numbers
    #[arg(short = 'n', long)]
    pub line_number: bool,

    /// Only print count of matching lines
    #[arg(short, long)]
    pub count: bool,

    /// Only print filenames containing matches
    #[arg(short = 'l', long)]
    pub files_with_matches: bool,

    /// Show N lines before each match
    #[arg(short = 'B', long, value_name = "NUM")]
    pub before_context: Option<usize>,

    /// Show N lines after each match
    #[arg(short = 'A', long, value_name = "NUM")]
    pub after_context: Option<usize>,

    /// Show N lines before and after each match
    #[arg(short = 'C', long, value_name = "NUM")]
    pub context: Option<usize>,

    /// Stop after NUM matches
    #[arg(short, long, value_name = "NUM")]
    pub max_count: Option<usize>,

    // ── File selection ──

    /// Search directories recursively
    #[arg(short, long)]
    pub recursive: bool,

    /// Glob pattern for files to include
    #[arg(long, value_name = "GLOB")]
    pub include: Vec<String>,

    /// Glob pattern for files to exclude
    #[arg(long, value_name = "GLOB")]
    pub exclude: Vec<String>,

    // ── Display ──

    /// When to colorize output
    #[arg(long, default_value = "auto", value_enum)]
    pub color: ColorWhen,

    /// Suppress error messages about nonexistent files
    #[arg(short, long)]
    pub silent: bool,
}

impl Cli {
    /// Resolve context: -C overrides -A and -B
    pub fn before_context_lines(&self) -> usize {
        self.context.or(self.before_context).unwrap_or(0)
    }

    pub fn after_context_lines(&self) -> usize {
        self.context.or(self.after_context).unwrap_or(0)
    }

    /// Should we print filenames?
    /// Yes if multiple files, no if single file or stdin
    pub fn show_filename(&self) -> bool {
        self.paths.len() > 1 || self.recursive
    }
}
```

### Ejemplo de uso

```bash
# Búsqueda simple
minigrep "fn main" src/main.rs

# Case insensitive con números de línea
minigrep -in "error" log.txt

# Búsqueda recursiva con filtro de archivos
minigrep -r --include "*.rs" "TODO" src/

# Contexto alrededor de matches
minigrep -C 3 "panic" src/lib.rs

# Contar matches por archivo
minigrep -rc "unsafe" src/

# Leer de stdin
cat file.txt | minigrep "pattern"

# Invertir match (líneas que NO contienen el patrón)
minigrep -v "^#" config.txt
```

### Estructura del proyecto

```
minigrep/
├── Cargo.toml
├── src/
│   ├── main.rs          ← Punto de entrada, parseo de args
│   ├── cli.rs           ← Definición de Cli (struct + impl)
│   ├── search.rs        ← Lógica de búsqueda (T02)
│   ├── output.rs        ← Formateo y colores (T03)
│   └── lib.rs           ← Re-exportaciones para testing
└── tests/
    └── integration.rs   ← Tests de integración
```

```rust
// src/main.rs
mod cli;
mod search;
mod output;

use clap::Parser;
use cli::Cli;

fn main() {
    let cli = Cli::parse();

    if let Err(e) = run(&cli) {
        if !cli.silent {
            eprintln!("minigrep: {}", e);
        }
        std::process::exit(1);
    }
}

fn run(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    // Implementación en temas siguientes
    todo!()
}
```

### Testing de la CLI

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    // clap permite parsear desde un iterator en vez de env::args
    #[test]
    fn test_basic_args() {
        let cli = Cli::parse_from(["minigrep", "pattern", "file.txt"]);
        assert_eq!(cli.pattern, "pattern");
        assert_eq!(cli.paths, vec![PathBuf::from("file.txt")]);
        assert!(!cli.ignore_case);
    }

    #[test]
    fn test_flags() {
        let cli = Cli::parse_from([
            "minigrep", "-i", "-n", "--color", "never",
            "pattern", "file.txt",
        ]);
        assert!(cli.ignore_case);
        assert!(cli.line_number);
        assert!(matches!(cli.color, ColorWhen::Never));
    }

    #[test]
    fn test_multiple_files() {
        let cli = Cli::parse_from([
            "minigrep", "pattern", "a.txt", "b.txt", "c.txt",
        ]);
        assert_eq!(cli.paths.len(), 3);
        assert!(cli.show_filename());
    }

    #[test]
    fn test_context_resolution() {
        // -C overrides -A and -B
        let cli = Cli::parse_from([
            "minigrep", "-C", "5", "pattern", "file.txt",
        ]);
        assert_eq!(cli.before_context_lines(), 5);
        assert_eq!(cli.after_context_lines(), 5);
    }

    #[test]
    fn test_stdin_mode() {
        let cli = Cli::parse_from(["minigrep", "pattern"]);
        assert!(cli.paths.is_empty());
        assert!(!cli.show_filename());
    }

    #[test]
    fn test_missing_pattern_fails() {
        let result = Cli::try_parse_from(["minigrep"]);
        assert!(result.is_err());
    }
}
```

`Cli::parse_from` es clave para testing: permite pasar argumentos como un
slice en vez de leer de `env::args()`.

---

## Errores comunes

### 1. Usar `env::args` manual para CLIs complejas

```rust
// ❌ Parsing manual: tedioso, propenso a bugs, sin --help
let args: Vec<String> = env::args().collect();
let pattern = &args[1];  // panic si no hay argumento
let file = &args[2];     // panic si no hay segundo argumento

// ✅ clap: robusto, --help gratis, validación automática
#[derive(Parser)]
struct Cli {
    pattern: String,
    file: PathBuf,
}
let cli = Cli::parse();
// Si falta un argumento, clap muestra un error descriptivo y sale
```

### 2. Confundir short y long en nombres con guiones

```rust
// ❌ El campo ignore_case genera --ignore-case (con guión)
// pero el short es -i (primera letra del nombre del campo)
#[arg(short, long)]
ignore_case: bool,

// Cuidado: si tienes otro campo que empieza con 'i',
// habrá conflicto de shorts
#[arg(short, long)]
invert_match: bool,
// Error: Short option names must be unique

// ✅ Especificar shorts explícitamente cuando hay conflicto
#[arg(short = 'i', long)]
ignore_case: bool,

#[arg(short = 'v', long)]
invert_match: bool,
```

### 3. No usar try_parse_from en tests

```rust
// ❌ parse_from en tests: si falla, hace process::exit
// y el test termina sin un error claro
#[test]
fn test_bad_args() {
    let cli = Cli::parse_from(["prog"]);  // Mata el proceso
}

// ✅ try_parse_from retorna Result
#[test]
fn test_bad_args() {
    let result = Cli::try_parse_from(["prog"]);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert!(err.to_string().contains("PATTERN"));
}
```

### 4. No separar la definición de la CLI del main

```rust
// ❌ Todo en main.rs: difícil de testear
fn main() {
    #[derive(Parser)]
    struct Cli { /* ... */ }
    let cli = Cli::parse();
    // ...lógica de negocio mezclada...
}

// ✅ CLI en su propio módulo, lógica separada
// src/cli.rs — definición pura, testeable
// src/search.rs — lógica de búsqueda
// src/main.rs — solo conecta ambos
```

### 5. Olvidar el separador `--` con cargo run

```bash
# ❌ Los argumentos van a cargo, no a tu programa
cargo run -i "pattern" file.txt
# cargo interpreta -i como su propio flag

# ✅ Usar -- para separar argumentos de cargo de los del programa
cargo run -- -i "pattern" file.txt
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              CLAP ARGUMENT PARSING CHEATSHEET                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SETUP                                                       │
│  clap = { version = "4", features = ["derive"] }             │
│  use clap::Parser;                                           │
│  #[derive(Parser)] struct Cli { ... }                        │
│  let cli = Cli::parse();                                     │
│                                                              │
│  TIPOS DE ARGUMENTO                                          │
│  pattern: String              Posicional obligatorio         │
│  file: Option<String>         Posicional opcional            │
│  files: Vec<PathBuf>          Múltiples posicionales         │
│  #[arg(short, long)]                                         │
│    verbose: bool              Flag (-v, --verbose)           │
│  #[arg(short, long)]                                         │
│    count: Option<usize>       Opción con valor              │
│  #[arg(long, default_value = "10")]                          │
│    max: usize                 Con valor por defecto          │
│                                                              │
│  ATRIBUTOS DE #[arg()]                                       │
│  short                        -x (primera letra)             │
│  short = 'n'                  -n (letra específica)          │
│  long                         --field-name (del nombre)      │
│  long = "custom-name"         --custom-name                  │
│  default_value = "val"        Valor si no se provee          │
│  value_name = "NUM"           Nombre en --help               │
│  value_enum                   Enum como valor                │
│  conflicts_with = "other"     Mutuamente excluyente          │
│  requires = "other"           Requiere otro arg              │
│  action = Count               -vvv → 3                      │
│  value_parser = range(0..=N)  Validar rango                 │
│                                                              │
│  ATRIBUTOS DE #[command()]                                   │
│  version                      --version automático           │
│  about                        Descripción en --help          │
│  name = "prog"                Nombre del programa            │
│  #[command(subcommand)]       Subcomandos                    │
│                                                              │
│  ENUMS COMO VALORES                                          │
│  #[derive(ValueEnum)]                                        │
│  enum Color { Auto, Always, Never }                          │
│  #[arg(value_enum)] color: Color                             │
│                                                              │
│  SUBCOMANDOS                                                 │
│  #[derive(Subcommand)]                                       │
│  enum Commands { Search { ... }, Count { ... } }             │
│  #[command(subcommand)] command: Commands                    │
│                                                              │
│  TESTING                                                     │
│  Cli::parse_from(["prog", "arg1"])     Parse desde slice     │
│  Cli::try_parse_from(["prog"])         Retorna Result        │
│                                                              │
│  EJECUCIÓN                                                   │
│  cargo run -- -i "pattern" file.txt    -- separa args        │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: CLI básica con clap

Crea un proyecto Rust con una CLI que simule el comando `wc` (word count):

```bash
cargo new miniwc
cd miniwc
```

La CLI debe soportar:

```
$ miniwc [OPTIONS] [FILES...]

Options:
  -l, --lines    Count lines
  -w, --words    Count words
  -c, --bytes    Count bytes
  -h, --help     Print help
  -V, --version  Print version
```

**Tareas:**
1. Define la struct `Cli` con los flags `-l`, `-w`, `-c` y un `Vec<PathBuf>` para files
2. Si ningún flag está activo, mostrar las tres métricas (como `wc` real)
3. Si no se dan archivos, leer de stdin
4. Implementa la lógica de conteo (solo lectura + conteo, sin la parte de buscar)
5. Escribe tests con `Cli::try_parse_from` para verificar el parsing

**Pregunta de reflexión:** El `wc` real tiene una bandera `-m` (count characters)
que difiere de `-c` (count bytes) con texto UTF-8. ¿Cómo modelarías esto con
clap si `-m` y `-c` son mutuamente excluyentes?

---

### Ejercicio 2: Validación personalizada

Extiende la CLI del Ejercicio 1 con validación:

```rust
/// Maximum line width to consider
#[arg(long, value_parser = validate_width)]
max_width: Option<usize>,
```

**Tareas:**
1. Escribe una función `validate_width` que acepte valores entre 1 y 10,000
2. Agrega un flag `--format` con un enum `Format { Table, Json, Plain }`
3. Agrega un flag `--separator` que solo acepte un único carácter
   (hint: usar `value_parser` con una función custom)
4. Escribe tests que verifiquen que la validación rechaza valores incorrectos
   con mensajes descriptivos

**Pregunta de reflexión:** ¿Es mejor validar en el parser (con `value_parser`)
o después del parsing (en la lógica de la aplicación)? ¿Cuándo usarías cada
enfoque?

---

### Ejercicio 3: Subcomandos para una herramienta multi-modo

Diseña una CLI con subcomandos para una herramienta de análisis de texto:

```
$ textool count "pattern" file.txt      # Contar ocurrencias
$ textool replace "old" "new" file.txt  # Reemplazar texto
$ textool stats file.txt                # Estadísticas del archivo
```

**Tareas:**
1. Define un enum `Commands` con tres variantes, cada una con sus argumentos
2. `count` necesita: pattern (posicional), files (Vec), -i (ignore case)
3. `replace` necesita: old (posicional), new (posicional), file (posicional),
   --dry-run (flag), --backup (flag)
4. `stats` necesita: file (posicional), --format (enum: text/json)
5. Implementa el `match` en main y al menos la lógica de `stats` (contar
   líneas, palabras, bytes, línea más larga)
6. Escribe tests para cada subcomando con `try_parse_from`

**Pregunta de reflexión:** ¿Cuándo es mejor usar subcomandos vs flags para
distinguir modos de operación? Por ejemplo, `git commit` vs `git status` son
subcomandos, pero `grep -c` (count mode) es un flag. ¿Cuál es el criterio
de diseño?