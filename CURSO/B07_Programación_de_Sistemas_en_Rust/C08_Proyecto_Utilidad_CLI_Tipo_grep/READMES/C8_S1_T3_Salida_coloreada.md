# Salida coloreada: colored crate y detección de TTY

## Índice

1. [Colores en la terminal](#colores-en-la-terminal)
2. [Códigos de escape ANSI](#códigos-de-escape-ansi)
3. [El crate colored](#el-crate-colored)
4. [Detección de TTY](#detección-de-tty)
5. [Colorear la salida de minigrep](#colorear-la-salida-de-minigrep)
6. [Formateo completo de resultados](#formateo-completo-de-resultados)
7. [Otras técnicas de formato](#otras-técnicas-de-formato)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Colores en la terminal

Las terminales modernas soportan colores y estilos de texto mediante
**secuencias de escape ANSI**. Estas son secuencias especiales de bytes
que la terminal interpreta como instrucciones de formato en vez de texto.

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Por qué colores en una herramienta CLI?                    │
│                                                              │
│  Sin colores:                                                │
│  src/main.rs:15:    let result = parse_record(data);         │
│  src/main.rs:42:    let result = parse_header(buf);          │
│  → Difícil distinguir archivo, número, y match               │
│                                                              │
│  Con colores (como grep --color):                            │
│  src/main.rs:15:    let result = parse_record(data);         │
│  ^^^^^^^^^^^^ ^^                 ^^^^^^^^^^^^                │
│  magenta      green              red+bold                    │
│  (filename)   (line num)         (match)                     │
│  → Información instantáneamente distinguible                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Códigos de escape ANSI

Antes de usar crates, es importante entender qué pasa debajo.

### Estructura de una secuencia ANSI

```
ESC [ <código> m
 │  │    │     │
 │  │    │     └─ Terminador
 │  │    └─ Código de color/estilo
 │  └─ Inicio de secuencia CSI
 └─ Carácter de escape (0x1B, \x1b, \033)
```

### Códigos comunes

```
┌──────────────────────────────────────────────────────────────┐
│  Código  │ Efecto                                            │
├──────────┼───────────────────────────────────────────────────┤
│  0       │ Reset (volver a normal)                           │
│  1       │ Bold / Bright                                     │
│  2       │ Dim                                               │
│  4       │ Underline                                         │
│  7       │ Reverse (intercambiar fg/bg)                      │
│                                                              │
│  Foreground (texto)        Background (fondo)                │
│  30  Negro                 40  Negro                         │
│  31  Rojo                  41  Rojo                          │
│  32  Verde                 42  Verde                         │
│  33  Amarillo              43  Amarillo                      │
│  34  Azul                  44  Azul                          │
│  35  Magenta               45  Magenta                       │
│  36  Cyan                  46  Cyan                          │
│  37  Blanco                47  Blanco                        │
│                                                              │
│  Bright variants: 90-97 (fg), 100-107 (bg)                  │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo manual en Rust

```rust
fn main() {
    // Rojo bold
    print!("\x1b[1;31mError:\x1b[0m something went wrong\n");
    //      ^^^^^^^^^^       ^^^^^^^^
    //      bold+red          reset

    // Verde
    print!("\x1b[32mSuccess\x1b[0m\n");

    // Múltiples estilos
    print!("\x1b[1;4;35mBold underline magenta\x1b[0m\n");
    //           │ │ │
    //           │ │ └─ magenta
    //           │ └─ underline
    //           └─ bold
}
```

Escribir secuencias ANSI manualmente es tedioso y propenso a errores.
Por eso usamos crates.

---

## El crate colored

`colored` es el crate más simple y popular para colores en Rust. Extiende
`&str` y `String` con métodos de color.

### Instalación

```toml
# Cargo.toml
[dependencies]
colored = "2"
```

### Uso básico

```rust
use colored::*;

fn main() {
    // Colores de texto
    println!("{}", "Red text".red());
    println!("{}", "Green text".green());
    println!("{}", "Blue text".blue());
    println!("{}", "Yellow text".yellow());
    println!("{}", "Magenta text".magenta());
    println!("{}", "Cyan text".cyan());

    // Estilos
    println!("{}", "Bold text".bold());
    println!("{}", "Dimmed text".dimmed());
    println!("{}", "Underline text".underline());
    println!("{}", "Italic text".italic());

    // Combinar color + estilo
    println!("{}", "Bold red".red().bold());
    println!("{}", "Bold green underline".green().bold().underline());

    // Color de fondo
    println!("{}", "Red on white".red().on_white());
    println!("{}", "White on blue".white().on_blue());

    // Bright variants
    println!("{}", "Bright red".bright_red());
    println!("{}", "Bright green".bright_green());
}
```

### Colores como los de grep

grep real usa estos colores por defecto:

```rust
use colored::*;

fn grep_style_output() {
    let filename = "src/main.rs";
    let line_number = 42;
    let line = "    let result = parse_record(data);";
    let match_text = "parse_record";

    // Estilo grep: filename:linenum:line con match resaltado
    print!("{}:", filename.magenta());
    print!("{}:", line_number.to_string().green());

    // Resaltar el match dentro de la línea
    let before = &line[..16];  // "    let result = "
    let matched = match_text;
    let after = &line[28..];   // "(data);"

    print!("{}", before);
    print!("{}", matched.red().bold());
    println!("{}", after);

    // Output: src/main.rs:42:    let result = parse_record(data);
    // Con colores: magenta:green: normal red+bold normal
}
```

### Control global de colores

```rust
use colored::control;

fn main() {
    // Desactivar colores globalmente
    control::set_override(false);
    println!("{}", "This will NOT be colored".red());

    // Reactivar
    control::set_override(true);
    println!("{}", "This WILL be colored".red());

    // Desactivar por variable de entorno
    // Si NO_COLOR está definida, colored se desactiva automáticamente
    // (estándar https://no-color.org/)
}
```

---

## Detección de TTY

Un problema fundamental: ¿cuándo mostrar colores?

```bash
# Caso 1: output a terminal → colores SÍ
minigrep "pattern" file.txt

# Caso 2: output a pipe → colores NO
minigrep "pattern" file.txt | less

# Caso 3: output a archivo → colores NO
minigrep "pattern" file.txt > results.txt
```

Si envías secuencias ANSI a un pipe o archivo, el receptor ve basura:
`^[[1;31mError^[[0m` en vez de `Error`.

### ¿Qué es un TTY?

```
┌──────────────────────────────────────────────────────────────┐
│  TTY (TeleTYpewriter) = terminal interactiva                 │
│                                                              │
│  stdout es un TTY cuando:                                    │
│  ┌──────────┐    ┌──────────┐                               │
│  │ programa │───▶│ terminal │  → El usuario ve la salida    │
│  └──────────┘    └──────────┘    directamente               │
│                                                              │
│  stdout NO es un TTY cuando:                                 │
│  ┌──────────┐    ┌──────────┐                               │
│  │ programa │───▶│   pipe   │───▶ otro programa / archivo   │
│  └──────────┘    └──────────┘                               │
│                                                              │
│  La syscall isatty(fd) determina esto                        │
└──────────────────────────────────────────────────────────────┘
```

### Detectar TTY en Rust

```rust
use std::io::IsTerminal;

fn main() {
    if std::io::stdout().is_terminal() {
        println!("stdout is a TTY — colors OK");
    } else {
        println!("stdout is a pipe/file — no colors");
    }

    // stderr puede tener estado diferente
    if std::io::stderr().is_terminal() {
        eprintln!("stderr is a TTY");
    }
}
```

`IsTerminal` está en la stdlib desde Rust 1.70. Para versiones anteriores,
existe el crate `is-terminal` (o `atty`, ahora deprecated).

### Implementar la lógica --color

```rust
use std::io::IsTerminal;

#[derive(Debug, Clone)]
pub enum ColorWhen {
    Auto,
    Always,
    Never,
}

/// Determine if we should use colors based on --color flag and TTY
pub fn should_colorize(color_when: &ColorWhen) -> bool {
    match color_when {
        ColorWhen::Always => true,
        ColorWhen::Never => false,
        ColorWhen::Auto => std::io::stdout().is_terminal(),
    }
}

/// Apply the color decision globally
pub fn configure_colors(color_when: &ColorWhen) {
    let use_colors = should_colorize(color_when);
    colored::control::set_override(use_colors);
    // Después de esto, .red(), .bold(), etc. son no-ops si use_colors=false
}
```

Llamar a `configure_colors` al inicio de `main`, antes de imprimir nada:

```rust
fn main() {
    let cli = Cli::parse();
    configure_colors(&cli.color);  // ← Configurar antes de cualquier output
    // ... resto del programa
}
```

### Respetar NO_COLOR

El estándar `NO_COLOR` (https://no-color.org/) dice que si la variable de
entorno `NO_COLOR` está definida (con cualquier valor), los programas deben
desactivar colores.

`colored` respeta `NO_COLOR` automáticamente. Pero si quieres ser explícito:

```rust
pub fn configure_colors(color_when: &ColorWhen) {
    let use_colors = match color_when {
        ColorWhen::Always => true,
        ColorWhen::Never => false,
        ColorWhen::Auto => {
            std::io::stdout().is_terminal()
                && std::env::var_os("NO_COLOR").is_none()
        }
    };
    colored::control::set_override(use_colors);
}
```

---

## Colorear la salida de minigrep

### Resaltar el match dentro de la línea

La parte más interesante: dividir la línea en segmentos y colorear solo
la parte que coincide con el patrón.

```rust
use colored::*;
use regex::Regex;

/// Colorize all matches within a line
pub fn colorize_matches(line: &str, regex: &Regex) -> String {
    let mut result = String::new();
    let mut last_end = 0;

    for m in regex.find_iter(line) {
        // Text before the match (normal)
        result.push_str(&line[last_end..m.start()]);
        // The match itself (colored)
        result.push_str(&line[m.start()..m.end()].red().bold().to_string());
        last_end = m.end();
    }

    // Text after the last match
    result.push_str(&line[last_end..]);
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_colorize_single_match() {
        // Disable colors for testing
        colored::control::set_override(false);

        let re = Regex::new("hello").unwrap();
        let result = colorize_matches("say hello world", &re);
        // Without colors, the output is the same as the input
        assert_eq!(result, "say hello world");
    }

    #[test]
    fn test_colorize_multiple_matches() {
        colored::control::set_override(false);

        let re = Regex::new("ab").unwrap();
        let result = colorize_matches("ab cd ab ef ab", &re);
        assert_eq!(result, "ab cd ab ef ab");
    }

    #[test]
    fn test_colorize_no_match() {
        colored::control::set_override(false);

        let re = Regex::new("xyz").unwrap();
        let result = colorize_matches("hello world", &re);
        assert_eq!(result, "hello world");
    }
}
```

### Módulo output completo

```rust
// src/output.rs

use colored::*;
use regex::Regex;
use std::io::{self, BufWriter, Write};

use crate::cli::Cli;
use crate::search::{MatchResult, SearchResult, ContextMatch};

pub struct Printer {
    show_filename: bool,
    show_line_numbers: bool,
    colorize: bool,
    regex: Option<Regex>,
}

impl Printer {
    pub fn new(cli: &Cli, regex: Option<Regex>) -> Self {
        Printer {
            show_filename: cli.show_filename(),
            show_line_numbers: cli.line_number,
            colorize: should_colorize(&cli.color),
            regex,
        }
    }

    /// Print a single match line
    pub fn print_match(
        &self,
        writer: &mut impl Write,
        source: &str,
        m: &MatchResult,
    ) -> io::Result<()> {
        // Filename prefix
        if self.show_filename {
            if self.colorize {
                write!(writer, "{}:", source.magenta())?;
            } else {
                write!(writer, "{}:", source)?;
            }
        }

        // Line number
        if self.show_line_numbers {
            if self.colorize {
                write!(writer, "{}:", m.line_number.to_string().green())?;
            } else {
                write!(writer, "{}:", m.line_number)?;
            }
        }

        // Line content with highlighted match
        if self.colorize {
            if let Some(ref re) = self.regex {
                writeln!(writer, "{}", colorize_matches(&m.line, re))?;
            } else {
                writeln!(writer, "{}", m.line)?;
            }
        } else {
            writeln!(writer, "{}", m.line)?;
        }

        Ok(())
    }

    /// Print a context line (not a match)
    pub fn print_context_line(
        &self,
        writer: &mut impl Write,
        source: &str,
        line_number: usize,
        line: &str,
    ) -> io::Result<()> {
        if self.show_filename {
            if self.colorize {
                write!(writer, "{}-", source.magenta())?;
            } else {
                write!(writer, "{}-", source)?;
            }
        }

        if self.show_line_numbers {
            if self.colorize {
                write!(writer, "{}-", line_number.to_string().green())?;
            } else {
                write!(writer, "{}-", line_number)?;
            }
        }

        writeln!(writer, "{}", line)
    }

    /// Print the group separator between context blocks
    pub fn print_separator(&self, writer: &mut impl Write) -> io::Result<()> {
        if self.colorize {
            writeln!(writer, "{}", "--".cyan())
        } else {
            writeln!(writer, "--")
        }
    }

    /// Print count mode (-c)
    pub fn print_count(
        &self,
        writer: &mut impl Write,
        source: &str,
        count: usize,
    ) -> io::Result<()> {
        if self.show_filename {
            if self.colorize {
                writeln!(writer, "{}:{}", source.magenta(), count)
            } else {
                writeln!(writer, "{}:{}", source, count)
            }
        } else {
            writeln!(writer, "{}", count)
        }
    }

    /// Print files-with-matches mode (-l)
    pub fn print_filename(
        &self,
        writer: &mut impl Write,
        source: &str,
    ) -> io::Result<()> {
        if self.colorize {
            writeln!(writer, "{}", source.magenta())
        } else {
            writeln!(writer, "{}", source)
        }
    }
}

/// Print all results for a single source
pub fn print_results(
    printer: &Printer,
    writer: &mut impl Write,
    result: &SearchResult,
    cli: &Cli,
) -> io::Result<()> {
    if cli.count {
        printer.print_count(writer, &result.source, result.matches.len())?;
        return Ok(());
    }

    if cli.files_with_matches {
        if !result.matches.is_empty() {
            printer.print_filename(writer, &result.source)?;
        }
        return Ok(());
    }

    for m in &result.matches {
        printer.print_match(writer, &result.source, m)?;
    }

    Ok(())
}
```

---

## Formateo completo de resultados

### Escribir a stdout con BufWriter

Para rendimiento, envolver stdout en un `BufWriter`:

```rust
use std::io::{self, BufWriter, Write};

fn main() {
    let cli = Cli::parse();
    configure_colors(&cli.color);

    let stdout = io::stdout();
    let mut writer = BufWriter::new(stdout.lock());

    // Todas las operaciones de escritura usan writer
    if let Err(e) = run(&cli, &mut writer) {
        // Flush antes de escribir a stderr
        let _ = writer.flush();
        if !cli.silent {
            eprintln!("minigrep: {}", e);
        }
        std::process::exit(2);
    }

    // Flush final
    let _ = writer.flush();
}
```

### La función run integrada

```rust
pub fn run(cli: &Cli, writer: &mut impl Write) -> Result<(), Box<dyn std::error::Error>> {
    let matcher = Matcher::new(
        &cli.pattern,
        cli.ignore_case,
        cli.fixed_strings,
        cli.word_regexp,
        cli.invert_match,
    )?;

    // Build regex for colorization (separate from matcher to avoid borrow issues)
    let color_regex = if should_colorize(&cli.color) && !cli.invert_match {
        Some(matcher.regex_clone())
    } else {
        None
    };

    let printer = Printer::new(cli, color_regex);
    let mut found_any = false;

    if cli.paths.is_empty() {
        let result = search_stdin(&matcher, cli.max_count)?;
        found_any = !result.matches.is_empty();
        print_results(&printer, writer, &result, cli)?;
    } else {
        for path in &cli.paths {
            let multi = search_path(
                &matcher, path, cli.recursive,
                &cli.include, &cli.exclude,
                cli.max_count, cli.silent,
            );

            for result in &multi.results {
                if !result.matches.is_empty() {
                    found_any = true;
                }
                print_results(&printer, writer, &result, cli)?;
            }
        }
    }

    if !found_any {
        std::process::exit(1);
    }

    Ok(())
}
```

### Ejemplo de output completo

```
$ minigrep -n --color=always "fn \w+" src/main.rs src/lib.rs

src/main.rs:1:fn main() {
src/main.rs:15:    fn helper(x: i32) -> i32 {
--
src/lib.rs:3:pub fn parse(input: &str) -> Result<Data> {
src/lib.rs:42:fn validate(data: &Data) -> bool {
```

Donde:
- `src/main.rs` está en magenta
- `1`, `15`, `3`, `42` están en verde
- `fn main`, `fn helper`, `fn parse`, `fn validate` están en rojo bold
- `--` es el separador de contexto en cyan

---

## Otras técnicas de formato

### Alternativa: el crate termcolor

`termcolor` es una alternativa a `colored` que soporta Windows nativo:

```toml
[dependencies]
termcolor = "1"
```

```rust
use termcolor::{Color, ColorChoice, ColorSpec, StandardStream, WriteColor};
use std::io::Write;

fn main() {
    let mut stdout = StandardStream::stdout(ColorChoice::Auto);
    // ColorChoice::Auto detecta TTY automáticamente

    // Escribir en rojo
    stdout.set_color(
        ColorSpec::new().set_fg(Some(Color::Red)).set_bold(true)
    ).unwrap();
    write!(&mut stdout, "Error").unwrap();

    // Reset
    stdout.reset().unwrap();
    writeln!(&mut stdout, ": something happened").unwrap();
}
```

### Alternativa: el crate owo-colors

Más ligero que `colored`, con API similar:

```toml
[dependencies]
owo-colors = "4"
```

```rust
use owo_colors::OwoColorize;

fn main() {
    println!("{}", "Red text".red());
    println!("{}", "Bold blue".blue().bold());
}
```

### Comparación de crates de color

```
┌───────────────┬──────────┬──────────┬──────────┬────────────┐
│ Crate         │ API      │ Windows  │ Tamaño   │ TTY auto   │
├───────────────┼──────────┼──────────┼──────────┼────────────┤
│ colored       │ Métodos  │ Parcial  │ Mediano  │ NO_COLOR   │
│               │ en &str  │ (ANSI)   │          │ soportado  │
├───────────────┼──────────┼──────────┼──────────┼────────────┤
│ termcolor     │ Write    │ Nativo   │ Pequeño  │ Sí         │
│               │ trait    │ (WinAPI) │          │ (ColorChoice)│
├───────────────┼──────────┼──────────┼──────────┼────────────┤
│ owo-colors    │ Métodos  │ ANSI     │ Mínimo   │ Manual     │
│               │ en T     │          │          │            │
├───────────────┼──────────┼──────────┼──────────┼────────────┤
│ ansi_term     │ Objetos  │ No       │ Pequeño  │ Manual     │
│ (deprecated)  │ Style    │          │          │            │
└───────────────┴──────────┴──────────┴──────────┴────────────┘

Recomendación:
- CLI Linux simple → colored (más ergonómico)
- Multiplataforma → termcolor (mejor Windows)
- Mínimo overhead → owo-colors
```

### Barras de progreso con indicatif

Para operaciones largas (búsqueda recursiva en miles de archivos):

```toml
[dependencies]
indicatif = "0.17"
```

```rust
use indicatif::{ProgressBar, ProgressStyle};

fn search_with_progress(files: &[PathBuf], matcher: &Matcher) {
    let pb = ProgressBar::new(files.len() as u64);
    pb.set_style(
        ProgressStyle::default_bar()
            .template("{spinner} [{bar:40}] {pos}/{len} files ({eta})")
            .unwrap()
    );

    for file in files {
        pb.set_message(file.display().to_string());
        search_file(matcher, file);
        pb.inc(1);
    }

    pb.finish_with_message("Done");
}
```

---

## Errores comunes

### 1. Enviar colores a un pipe

```bash
# ❌ Colores en un pipe: el receptor ve secuencias ANSI basura
minigrep "pattern" file.txt | head -5
# ^[[35msrc/main.rs^[[0m:^[[32m1^[[0m:fn main() {

# ✅ Detectar TTY y desactivar colores automáticamente
# (con --color=auto, que debe ser el default)
```

```rust
// ❌ Siempre colorear
println!("{}", "text".red());

// ✅ Configurar al inicio según --color y TTY
configure_colors(&cli.color);
// Ahora .red() es no-op cuando colores están desactivados
```

### 2. Olvidar resetear colores al final

```rust
// ❌ Si el programa crashea entre set_color y reset,
// la terminal queda con colores residuales
print!("\x1b[31m");  // Rojo
panic!("oops");       // La terminal queda en rojo

// ✅ colored maneja esto automáticamente con Display
// cada .red() incluye el reset al final de la cadena
println!("{}", "safe".red());
// Equivale a: \x1b[31msafe\x1b[0m (el reset está incluido)
```

### 3. Colorear toda la línea en vez de solo el match

```rust
// ❌ Toda la línea en rojo: difícil leer
println!("{}", line.red());

// ✅ Solo el match en rojo, el resto normal
println!("{}", colorize_matches(&line, &regex));
```

### 4. No testear con colores desactivados

```rust
// ❌ Tests que dependen de secuencias ANSI específicas
#[test]
fn test_output() {
    let output = format!("{}", "hello".red());
    assert_eq!(output, "\x1b[31mhello\x1b[0m");
    // Frágil: depende de la implementación interna de colored
}

// ✅ Desactivar colores en tests y verificar contenido
#[test]
fn test_output() {
    colored::control::set_override(false);
    let output = format!("{}", "hello".red());
    assert_eq!(output, "hello");
    // Robusto: verifica el contenido, no el formato
}

// ✅ O testear la lógica separada del formateo
#[test]
fn test_match_positions() {
    let re = Regex::new("hello").unwrap();
    let m = re.find("say hello world").unwrap();
    assert_eq!(m.start(), 4);
    assert_eq!(m.end(), 9);
}
```

### 5. No respetar la convención NO_COLOR

```rust
// ❌ Ignorar NO_COLOR
// Si el usuario tiene NO_COLOR=1, tu programa sigue mostrando colores

// ✅ colored respeta NO_COLOR automáticamente
// Pero si usas override manual, verifica:
pub fn configure_colors(color_when: &ColorWhen) {
    let use_colors = match color_when {
        ColorWhen::Always => true,  // --color=always ignora NO_COLOR
        ColorWhen::Never => false,
        ColorWhen::Auto => {
            std::io::stdout().is_terminal()
                && std::env::var_os("NO_COLOR").is_none()
        }
    };
    colored::control::set_override(use_colors);
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            SALIDA COLOREADA CHEATSHEET                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  COLORED CRATE                                               │
│  colored = "2"                       Cargo.toml              │
│  use colored::*;                     Import                  │
│                                                              │
│  COLORES                                                     │
│  "text".red()                        Rojo                    │
│  "text".green()                      Verde                   │
│  "text".blue()                       Azul                    │
│  "text".yellow()                     Amarillo                │
│  "text".magenta()                    Magenta                 │
│  "text".cyan()                       Cyan                    │
│  "text".bright_red()                 Rojo brillante          │
│  "text".on_white()                   Fondo blanco            │
│                                                              │
│  ESTILOS                                                     │
│  "text".bold()                       Negrita                 │
│  "text".dimmed()                     Atenuado                │
│  "text".underline()                  Subrayado               │
│  "text".italic()                     Cursiva                 │
│  "text".red().bold()                 Combinar                │
│                                                              │
│  CONTROL                                                     │
│  colored::control::set_override(b)   Forzar on/off           │
│  NO_COLOR env var                    Desactiva colores       │
│                                                              │
│  DETECCIÓN DE TTY                                            │
│  use std::io::IsTerminal;            Stdlib (Rust ≥ 1.70)   │
│  stdout().is_terminal()              ¿Es una terminal?      │
│                                                              │
│  COLORES DE GREP                                             │
│  Filename  → magenta                                         │
│  Line num  → green                                           │
│  Match     → red + bold                                      │
│  Separator → cyan                                            │
│  Context   → separador "-" en vez de ":"                     │
│                                                              │
│  PATRÓN DE USO                                               │
│  1. Parsear --color (auto/always/never)                      │
│  2. configure_colors() al inicio de main                     │
│  3. Usar .red(), .bold() etc. normalmente                    │
│  4. colored se desactiva solo cuando override = false        │
│                                                              │
│  ANSI MANUAL (referencia)                                    │
│  \x1b[0m     Reset                                          │
│  \x1b[1m     Bold                                            │
│  \x1b[31m    Red fg                                          │
│  \x1b[32m    Green fg                                        │
│  \x1b[35m    Magenta fg                                      │
│  \x1b[1;31m  Bold + Red                                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Resaltar matches en una línea

Implementa la función `colorize_matches`:

```rust
use regex::Regex;
use colored::*;

fn colorize_matches(line: &str, regex: &Regex) -> String {
    todo!()
}
```

**Tareas:**
1. Implementa la función que recorre todos los matches de la regex en la línea
   y colorea cada uno en rojo bold, dejando el resto normal
2. Escribe tests para:
   - Una línea con un solo match
   - Una línea con múltiples matches
   - Una línea sin matches (debe retornar la línea sin cambios)
   - Un match al inicio de la línea
   - Un match al final de la línea
   - Match de longitud cero (regex `^` o `\b`) — ¿qué pasa?
3. Desactiva colores con `colored::control::set_override(false)` en tests
   y verifica que el contenido de texto es correcto

**Pregunta de reflexión:** ¿Qué ocurre si la regex tiene matches solapados
(por ejemplo, regex `a+` con el texto `"aaa"`)? ¿`find_iter` produce matches
solapados? ¿Y `captures_iter`?

---

### Ejercicio 2: Printer con detección de TTY

Implementa el módulo `output.rs` completo:

**Tareas:**
1. Crea la struct `Printer` con la configuración de display
2. Implementa `print_match` que formatea:
   - `filename:linenum:line` si `show_filename` y `show_line_numbers`
   - `filename:line` si solo `show_filename`
   - `linenum:line` si solo `show_line_numbers`
   - `line` si ninguno
3. Implementa `print_context_line` con `-` como separador (vs `:` para matches)
4. Prueba la detección de TTY:
   ```bash
   # Debería tener colores
   cargo run -- -n "fn" src/main.rs

   # No debería tener colores
   cargo run -- -n "fn" src/main.rs | cat

   # Forzar colores
   cargo run -- -n --color=always "fn" src/main.rs | cat
   ```
5. Verifica que `NO_COLOR=1 cargo run -- "fn" src/main.rs` desactiva colores

**Pregunta de reflexión:** ¿Por qué los errores (`eprintln!`) típicamente
NO se colorean incluso cuando stdout tiene colores? ¿Qué pasaría si
stderr va a un archivo de log pero stdout va a la terminal?

---

### Ejercicio 3: Output formateado completo

Integra todo el proyecto minigrep hasta este punto:

```bash
# Crear el proyecto si no existe
cargo new minigrep
cd minigrep
# Agregar dependencias: clap, regex, colored, walkdir
```

**Tareas:**
1. Integra los módulos: `cli.rs` (T01), `search.rs` (T02), `output.rs` (T03)
2. Implementa `main.rs` que conecta todo con `BufWriter`
3. Ejecuta y compara con grep real:
   ```bash
   # Tu minigrep
   cargo run -- -rn --color=always --include "*.rs" "fn \w+" src/

   # grep real
   grep -rn --color=always --include="*.rs" "fn \w+" src/
   ```
4. Verifica estos escenarios:
   - Búsqueda simple: `minigrep "pattern" file.txt`
   - Múltiples archivos: `minigrep "pattern" file1.txt file2.txt`
   - Stdin: `echo "hello world" | minigrep "hello"`
   - Recursivo: `minigrep -r "TODO" src/`
   - Exit code 0 (match) vs 1 (no match): `minigrep "xyz" file.txt; echo $?`
5. ¿La salida es visualmente comparable a grep? ¿Qué falta?

**Pregunta de reflexión:** grep colorea el separador `:` entre filename y
line number de forma diferente al `:` entre line number y contenido.
¿Vale la pena ese nivel de detalle? ¿Cuándo la fidelidad visual importa
y cuándo es over-engineering?