# Búsqueda en archivos: regex crate y matching multi-archivo

## Índice

1. [El motor de búsqueda de minigrep](#el-motor-de-búsqueda-de-minigrep)
2. [Búsqueda literal con la stdlib](#búsqueda-literal-con-la-stdlib)
3. [El crate regex](#el-crate-regex)
4. [Búsqueda línea por línea](#búsqueda-línea-por-línea)
5. [Búsqueda multi-archivo](#búsqueda-multi-archivo)
6. [Búsqueda recursiva en directorios](#búsqueda-recursiva-en-directorios)
7. [Contexto: líneas antes y después](#contexto-líneas-antes-y-después)
8. [Integración con la CLI](#integración-con-la-cli)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El motor de búsqueda de minigrep

En T01 definimos la interfaz de usuario (CLI). Ahora construimos el motor
de búsqueda: la parte que abre archivos, lee líneas y encuentra matches.

```
┌──────────────────────────────────────────────────────────────┐
│           Arquitectura del motor de búsqueda                 │
│                                                              │
│  CLI (T01)         Search (T02)          Output (T03)        │
│  ┌──────────┐      ┌──────────────┐     ┌──────────────┐   │
│  │ Cli::parse│─────▶│ search_file  │────▶│ print_match  │   │
│  │ pattern   │      │ search_stdin │     │ colorize     │   │
│  │ paths     │      │ search_dir   │     │ line numbers │   │
│  │ flags     │      │              │     │              │   │
│  └──────────┘      └──────┬───────┘     └──────────────┘   │
│                           │                                  │
│                    ┌──────┴───────┐                          │
│                    │   Matcher    │                          │
│                    │ regex / fixed│                          │
│                    │ case / word  │                          │
│                    └──────────────┘                          │
└──────────────────────────────────────────────────────────────┘
```

---

## Búsqueda literal con la stdlib

Antes de usar regex, veamos la búsqueda más simple: texto literal.

```rust
/// Search for a literal pattern in text, line by line
fn search_literal<'a>(pattern: &str, contents: &'a str) -> Vec<&'a str> {
    contents
        .lines()
        .filter(|line| line.contains(pattern))
        .collect()
}

/// Case insensitive version
fn search_case_insensitive<'a>(pattern: &str, contents: &'a str) -> Vec<&'a str> {
    let pattern_lower = pattern.to_lowercase();
    contents
        .lines()
        .filter(|line| line.to_lowercase().contains(&pattern_lower))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_literal_search() {
        let contents = "hello world\nfoo bar\nhello rust";
        let results = search_literal("hello", contents);
        assert_eq!(results, vec!["hello world", "hello rust"]);
    }

    #[test]
    fn test_case_insensitive() {
        let contents = "Hello World\nfoo bar\nHELLO RUST";
        let results = search_case_insensitive("hello", contents);
        assert_eq!(results, vec!["Hello World", "HELLO RUST"]);
    }

    #[test]
    fn test_no_matches() {
        let contents = "foo\nbar\nbaz";
        let results = search_literal("xyz", contents);
        assert!(results.is_empty());
    }
}
```

Esto funciona para minigrep básico, pero el grep real usa **expresiones
regulares**. Ahí entra el crate `regex`.

---

## El crate regex

`regex` es el crate estándar de expresiones regulares en Rust. Está diseñado
para ser **rápido y seguro**: garantiza tiempo de ejecución lineal (no es
vulnerable a ReDoS — regex denial of service).

### Instalación

```toml
# Cargo.toml
[dependencies]
regex = "1"
```

### Uso básico

```rust
use regex::Regex;

fn main() {
    // Compilar el patrón (puede fallar si la regex es inválida)
    let re = Regex::new(r"fn\s+(\w+)").unwrap();

    let text = "fn main() {\n    fn helper() {\n    }\n}";

    // is_match: ¿hay algún match?
    assert!(re.is_match(text));

    // find: primer match (posición)
    if let Some(m) = re.find(text) {
        println!("Match: '{}' at {}..{}", m.as_str(), m.start(), m.end());
        // Match: 'fn main' at 0..7
    }

    // captures: grupos de captura
    for caps in re.captures_iter(text) {
        println!("Function: {}", &caps[1]);
        // Function: main
        // Function: helper
    }
}
```

### Sintaxis de regex más usada

```
┌──────────────────────────────────────────────────────────────┐
│  Sintaxis               │ Significado                        │
├─────────────────────────┼────────────────────────────────────┤
│  .                      │ Cualquier carácter (excepto \n)    │
│  \d                     │ Dígito [0-9]                       │
│  \w                     │ Alfanumérico [a-zA-Z0-9_]         │
│  \s                     │ Espacio en blanco                  │
│  \b                     │ Límite de palabra (word boundary)  │
│  [abc]                  │ Uno de a, b, c                     │
│  [^abc]                 │ Cualquiera excepto a, b, c         │
│  [a-z]                  │ Rango                              │
│  ^                      │ Inicio de línea                    │
│  $                      │ Fin de línea                       │
│  *                      │ Cero o más                         │
│  +                      │ Uno o más                          │
│  ?                      │ Cero o uno                         │
│  {n,m}                  │ Entre n y m repeticiones           │
│  (grupo)                │ Grupo de captura                   │
│  (?:grupo)              │ Grupo sin captura                  │
│  (?i)                   │ Case insensitive (inline flag)     │
│  a|b                    │ Alternativa                        │
└─────────────────────────┴────────────────────────────────────┘
```

### RegexBuilder: configuración avanzada

```rust
use regex::RegexBuilder;

// Construir regex con opciones
let re = RegexBuilder::new(r"fn\s+\w+")
    .case_insensitive(true)   // (?i) equivalente
    .multi_line(true)         // ^ y $ son inicio/fin de LÍNEA
    .dot_matches_new_line(false)
    .build()
    .expect("Invalid regex");
```

### Rendimiento: compilar una vez, usar muchas veces

```rust
use regex::Regex;

// ❌ Compilar regex en cada iteración (lento)
fn search_bad(pattern: &str, lines: &[String]) -> Vec<String> {
    lines.iter()
        .filter(|line| {
            let re = Regex::new(pattern).unwrap();  // Recompila cada vez
            re.is_match(line)
        })
        .cloned()
        .collect()
}

// ✅ Compilar una vez, reusar
fn search_good(pattern: &str, lines: &[String]) -> Vec<String> {
    let re = Regex::new(pattern).unwrap();  // Una sola compilación
    lines.iter()
        .filter(|line| re.is_match(line))
        .cloned()
        .collect()
}
```

---

## Búsqueda línea por línea

El patrón fundamental de grep: leer un archivo línea por línea y reportar
las que hacen match.

### Estructura de un resultado de búsqueda

```rust
/// A single match result
#[derive(Debug, Clone)]
pub struct MatchResult {
    /// Line number (1-based)
    pub line_number: usize,
    /// The full line content
    pub line: String,
    /// Byte offset of the match within the line (start, end)
    pub match_range: Option<(usize, usize)>,
}

/// Results from searching a single source
#[derive(Debug)]
pub struct SearchResult {
    /// Source identifier (filename or "<stdin>")
    pub source: String,
    /// All matching lines
    pub matches: Vec<MatchResult>,
    /// Total lines examined
    pub lines_searched: usize,
}
```

### El matcher: abstracción sobre regex y literal

```rust
use regex::{Regex, RegexBuilder};

/// Encapsulates the matching logic
pub struct Matcher {
    regex: Regex,
    invert: bool,
}

impl Matcher {
    pub fn new(
        pattern: &str,
        case_insensitive: bool,
        fixed_string: bool,
        word_regexp: bool,
        invert: bool,
    ) -> Result<Self, regex::Error> {
        // Escapar el patrón si es fixed string (literal, no regex)
        let pattern = if fixed_string {
            regex::escape(pattern)
        } else {
            pattern.to_string()
        };

        // Envolver en \b para word matching
        let pattern = if word_regexp {
            format!(r"\b(?:{})\b", pattern)
        } else {
            pattern
        };

        let regex = RegexBuilder::new(&pattern)
            .case_insensitive(case_insensitive)
            .multi_line(true)
            .build()?;

        Ok(Matcher { regex, invert })
    }

    /// Does this line match?
    pub fn is_match(&self, line: &str) -> bool {
        let matches = self.regex.is_match(line);
        if self.invert { !matches } else { matches }
    }

    /// Find the position of the match within the line
    pub fn find_match(&self, line: &str) -> Option<(usize, usize)> {
        if self.invert {
            return None;  // Inverted matches don't highlight
        }
        self.regex.find(line).map(|m| (m.start(), m.end()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_literal_match() {
        let m = Matcher::new("hello", false, true, false, false).unwrap();
        assert!(m.is_match("say hello world"));
        assert!(!m.is_match("say goodbye"));
    }

    #[test]
    fn test_regex_match() {
        let m = Matcher::new(r"fn\s+\w+", false, false, false, false).unwrap();
        assert!(m.is_match("fn main() {"));
        assert!(!m.is_match("let x = 5;"));
    }

    #[test]
    fn test_case_insensitive() {
        let m = Matcher::new("hello", true, true, false, false).unwrap();
        assert!(m.is_match("HELLO WORLD"));
        assert!(m.is_match("Hello World"));
    }

    #[test]
    fn test_word_boundary() {
        let m = Matcher::new("error", false, true, true, false).unwrap();
        assert!(m.is_match("an error occurred"));
        assert!(!m.is_match("no errors here"));  // "errors" ≠ "error"
    }

    #[test]
    fn test_invert_match() {
        let m = Matcher::new("skip", false, true, false, true).unwrap();
        assert!(!m.is_match("skip this line"));
        assert!(m.is_match("keep this line"));
    }

    #[test]
    fn test_invalid_regex() {
        let result = Matcher::new("[invalid", false, false, false, false);
        assert!(result.is_err());
    }

    #[test]
    fn test_fixed_string_escapes_regex() {
        // "." in fixed mode matches literal dot, not any character
        let m = Matcher::new("file.txt", false, true, false, false).unwrap();
        assert!(m.is_match("open file.txt"));
        assert!(!m.is_match("open fileTtxt"));  // '.' no es wildcard
    }
}
```

### Buscar en un lector (genérico sobre Read)

```rust
use std::io::{self, BufRead, BufReader, Read};

/// Search a readable source line by line
pub fn search_reader<R: Read>(
    matcher: &Matcher,
    reader: R,
    source_name: &str,
    max_count: Option<usize>,
) -> io::Result<SearchResult> {
    let reader = BufReader::new(reader);
    let mut matches = Vec::new();
    let mut lines_searched = 0;

    for (idx, line_result) in reader.lines().enumerate() {
        let line = line_result?;
        lines_searched += 1;
        let line_number = idx + 1;  // 1-based

        if matcher.is_match(&line) {
            let match_range = matcher.find_match(&line);
            matches.push(MatchResult {
                line_number,
                line,
                match_range,
            });

            // Stop early if max_count reached
            if let Some(max) = max_count {
                if matches.len() >= max {
                    break;
                }
            }
        }
    }

    Ok(SearchResult {
        source: source_name.to_string(),
        matches,
        lines_searched,
    })
}
```

Esta función es genérica sobre `Read`: funciona con archivos, stdin, buffers
de test, o cualquier fuente de datos.

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn test_search_reader() {
        let content = "hello world\nfoo bar\nhello rust\nbaz";
        let reader = Cursor::new(content);
        let matcher = Matcher::new("hello", false, true, false, false).unwrap();

        let result = search_reader(&matcher, reader, "test", None).unwrap();
        assert_eq!(result.matches.len(), 2);
        assert_eq!(result.matches[0].line_number, 1);
        assert_eq!(result.matches[0].line, "hello world");
        assert_eq!(result.matches[1].line_number, 3);
        assert_eq!(result.matches[1].line, "hello rust");
        assert_eq!(result.lines_searched, 4);
    }

    #[test]
    fn test_search_reader_max_count() {
        let content = "a\na\na\na\na";
        let reader = Cursor::new(content);
        let matcher = Matcher::new("a", false, true, false, false).unwrap();

        let result = search_reader(&matcher, reader, "test", Some(2)).unwrap();
        assert_eq!(result.matches.len(), 2);
    }

    #[test]
    fn test_search_reader_no_matches() {
        let content = "foo\nbar\nbaz";
        let reader = Cursor::new(content);
        let matcher = Matcher::new("xyz", false, true, false, false).unwrap();

        let result = search_reader(&matcher, reader, "test", None).unwrap();
        assert!(result.matches.is_empty());
        assert_eq!(result.lines_searched, 3);
    }
}
```

---

## Búsqueda multi-archivo

### Buscar en un archivo

```rust
use std::fs::File;
use std::path::Path;

/// Search a single file
pub fn search_file(
    matcher: &Matcher,
    path: &Path,
    max_count: Option<usize>,
) -> io::Result<SearchResult> {
    let file = File::open(path)?;
    let source_name = path.display().to_string();
    search_reader(matcher, file, &source_name, max_count)
}
```

### Buscar en stdin

```rust
/// Search stdin
pub fn search_stdin(
    matcher: &Matcher,
    max_count: Option<usize>,
) -> io::Result<SearchResult> {
    let stdin = io::stdin();
    let reader = stdin.lock();
    search_reader(matcher, reader, "<stdin>", max_count)
}
```

### Buscar en múltiples archivos

```rust
use std::path::PathBuf;

/// Results from searching multiple files
pub struct MultiSearchResult {
    pub results: Vec<SearchResult>,
    pub errors: Vec<(PathBuf, io::Error)>,
}

/// Search multiple files, collecting results and errors
pub fn search_files(
    matcher: &Matcher,
    paths: &[PathBuf],
    max_count: Option<usize>,
    silent: bool,
) -> MultiSearchResult {
    let mut results = Vec::new();
    let mut errors = Vec::new();

    for path in paths {
        match search_file(matcher, path, max_count) {
            Ok(result) => {
                results.push(result);
            }
            Err(e) => {
                if !silent {
                    eprintln!("minigrep: {}: {}", path.display(), e);
                }
                errors.push((path.clone(), e));
            }
        }
    }

    MultiSearchResult { results, errors }
}
```

### Detectar archivos binarios

grep real detecta archivos binarios y los omite o muestra un aviso:

```rust
use std::io::Read;

/// Check if a file appears to be binary (contains null bytes)
fn is_binary(path: &Path) -> io::Result<bool> {
    let mut file = File::open(path)?;
    let mut buffer = [0u8; 8192];
    let n = file.read(&mut buffer)?;
    Ok(buffer[..n].contains(&0))
}

/// Search a file, skipping binary files
pub fn search_file_smart(
    matcher: &Matcher,
    path: &Path,
    max_count: Option<usize>,
) -> io::Result<SearchResult> {
    if is_binary(path)? {
        // Return empty result for binary files
        return Ok(SearchResult {
            source: path.display().to_string(),
            matches: Vec::new(),
            lines_searched: 0,
        });
    }
    search_file(matcher, path, max_count)
}
```

---

## Búsqueda recursiva en directorios

### Recorrido manual con std::fs

```rust
use std::fs;
use std::path::Path;

/// Collect all files in a directory recursively
fn collect_files(dir: &Path) -> io::Result<Vec<PathBuf>> {
    let mut files = Vec::new();
    collect_files_recursive(dir, &mut files)?;
    Ok(files)
}

fn collect_files_recursive(dir: &Path, files: &mut Vec<PathBuf>) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();

        if path.is_dir() {
            // Skip hidden directories and common non-source dirs
            let name = path.file_name()
                .and_then(|n| n.to_str())
                .unwrap_or("");

            if !name.starts_with('.') && name != "target" && name != "node_modules" {
                collect_files_recursive(&path, files)?;
            }
        } else if path.is_file() {
            files.push(path);
        }
    }
    Ok(())
}
```

### El crate walkdir: recorrido robusto

Para recorrido recursivo de producción, el crate `walkdir` es más robusto:

```toml
# Cargo.toml
[dependencies]
walkdir = "2"
```

```rust
use walkdir::WalkDir;
use std::path::{Path, PathBuf};

/// Collect files recursively with filtering
pub fn collect_files_filtered(
    dir: &Path,
    include_globs: &[String],
    exclude_globs: &[String],
) -> Vec<PathBuf> {
    WalkDir::new(dir)
        .follow_links(false)    // No seguir symlinks (evitar loops)
        .into_iter()
        .filter_entry(|entry| {
            // Filtrar directorios ocultos y comunes
            let name = entry.file_name().to_str().unwrap_or("");
            if entry.file_type().is_dir() {
                return !name.starts_with('.')
                    && name != "target"
                    && name != "node_modules"
                    && !exclude_globs.iter().any(|g| matches_glob(name, g));
            }
            true
        })
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.file_type().is_file())
        .filter(|entry| {
            let name = entry.file_name().to_str().unwrap_or("");

            // Si hay include globs, el archivo debe coincidir con alguno
            let included = include_globs.is_empty()
                || include_globs.iter().any(|g| matches_glob(name, g));

            // El archivo no debe coincidir con ningún exclude glob
            let excluded = exclude_globs.iter().any(|g| matches_glob(name, g));

            included && !excluded
        })
        .map(|entry| entry.into_path())
        .collect()
}

/// Simple glob matching (supports * and ?)
fn matches_glob(name: &str, glob: &str) -> bool {
    // For production, use the `glob` or `globset` crate
    // This is a simplified version for common cases
    if let Some(suffix) = glob.strip_prefix("*.") {
        // "*.rs" → matches files ending in ".rs"
        name.ends_with(&format!(".{}", suffix))
    } else if let Some(prefix) = glob.strip_suffix("*") {
        name.starts_with(prefix)
    } else {
        name == glob
    }
}
```

### Integrar recorrido recursivo con búsqueda

```rust
/// Search a path: file or directory
pub fn search_path(
    matcher: &Matcher,
    path: &Path,
    recursive: bool,
    include_globs: &[String],
    exclude_globs: &[String],
    max_count: Option<usize>,
    silent: bool,
) -> MultiSearchResult {
    if path.is_file() {
        // Single file
        let mut results = Vec::new();
        let mut errors = Vec::new();

        match search_file(matcher, path, max_count) {
            Ok(result) => results.push(result),
            Err(e) => {
                if !silent {
                    eprintln!("minigrep: {}: {}", path.display(), e);
                }
                errors.push((path.to_path_buf(), e));
            }
        }

        MultiSearchResult { results, errors }
    } else if path.is_dir() && recursive {
        // Recursive directory search
        let files = collect_files_filtered(path, include_globs, exclude_globs);
        search_files(matcher, &files, max_count, silent)
    } else if path.is_dir() {
        // Directory without -r flag
        if !silent {
            eprintln!("minigrep: {}: Is a directory", path.display());
        }
        MultiSearchResult {
            results: Vec::new(),
            errors: vec![(
                path.to_path_buf(),
                io::Error::new(io::ErrorKind::Other, "Is a directory"),
            )],
        }
    } else {
        // Path doesn't exist
        if !silent {
            eprintln!("minigrep: {}: No such file or directory", path.display());
        }
        MultiSearchResult {
            results: Vec::new(),
            errors: vec![(
                path.to_path_buf(),
                io::Error::new(io::ErrorKind::NotFound, "No such file or directory"),
            )],
        }
    }
}
```

---

## Contexto: líneas antes y después

grep con `-A` (after), `-B` (before), y `-C` (context) muestra líneas
alrededor de cada match. Esto requiere un enfoque diferente: no podemos
procesar líneas de forma aislada.

### Implementación con buffer circular

```rust
use std::collections::VecDeque;
use std::io::{BufRead, BufReader, Read};

/// A match with its surrounding context lines
#[derive(Debug)]
pub struct ContextMatch {
    /// Lines before the match
    pub before: Vec<(usize, String)>,  // (line_number, content)
    /// The matching line itself
    pub line_number: usize,
    pub line: String,
    pub match_range: Option<(usize, usize)>,
    /// Lines after the match
    pub after: Vec<(usize, String)>,
}

/// Search with context lines
pub fn search_with_context<R: Read>(
    matcher: &Matcher,
    reader: R,
    source_name: &str,
    before_count: usize,
    after_count: usize,
    max_count: Option<usize>,
) -> io::Result<Vec<ContextMatch>> {
    let reader = BufReader::new(reader);
    let lines: Vec<String> = reader.lines().collect::<io::Result<_>>()?;
    let mut results = Vec::new();

    for (idx, line) in lines.iter().enumerate() {
        if !matcher.is_match(line) {
            continue;
        }

        let line_number = idx + 1;

        // Collect before-context lines
        let before_start = idx.saturating_sub(before_count);
        let before: Vec<(usize, String)> = (before_start..idx)
            .map(|i| (i + 1, lines[i].clone()))
            .collect();

        // Collect after-context lines
        let after_end = (idx + 1 + after_count).min(lines.len());
        let after: Vec<(usize, String)> = (idx + 1..after_end)
            .map(|i| (i + 1, lines[i].clone()))
            .collect();

        let match_range = matcher.find_match(line);

        results.push(ContextMatch {
            before,
            line_number,
            line: line.clone(),
            match_range,
            after,
        });

        if let Some(max) = max_count {
            if results.len() >= max {
                break;
            }
        }
    }

    Ok(results)
}
```

### Evitar contexto duplicado entre matches cercanos

Cuando dos matches están cerca, sus contextos se solapan. grep real los
fusiona con un separador `--`:

```rust
/// Check if two context matches overlap
fn contexts_overlap(prev: &ContextMatch, curr: &ContextMatch) -> bool {
    if prev.after.is_empty() && curr.before.is_empty() {
        return prev.line_number + 1 >= curr.line_number;
    }

    let prev_end = prev.after.last()
        .map(|(n, _)| *n)
        .unwrap_or(prev.line_number);

    let curr_start = curr.before.first()
        .map(|(n, _)| *n)
        .unwrap_or(curr.line_number);

    prev_end >= curr_start - 1
}

/// Merge overlapping context groups for display
pub fn group_context_matches(matches: &[ContextMatch]) -> Vec<Vec<&ContextMatch>> {
    let mut groups: Vec<Vec<&ContextMatch>> = Vec::new();

    for m in matches {
        let should_merge = groups.last()
            .and_then(|group| group.last())
            .map(|prev| contexts_overlap(prev, m))
            .unwrap_or(false);

        if should_merge {
            groups.last_mut().unwrap().push(m);
        } else {
            groups.push(vec![m]);
        }
    }

    groups
}
```

Cada grupo se imprime junto, y entre grupos se imprime `--` como separador
(igual que grep).

---

## Integración con la CLI

### La función run: conectar todo

```rust
use std::io;
use std::process;

/// Main entry point that connects CLI to search engine
pub fn run(cli: &Cli) -> Result<(), Box<dyn std::error::Error>> {
    // Build the matcher from CLI flags
    let matcher = Matcher::new(
        &cli.pattern,
        cli.ignore_case,
        cli.fixed_strings,
        cli.word_regexp,
        cli.invert_match,
    ).map_err(|e| format!("Invalid pattern: {}", e))?;

    let max_count = cli.max_count;
    let has_context = cli.before_context_lines() > 0
        || cli.after_context_lines() > 0;

    // Determine exit code: 0 if any match found, 1 if none
    let mut found_any = false;

    if cli.paths.is_empty() {
        // Read from stdin
        if has_context {
            let stdin = io::stdin();
            let matches = search_with_context(
                &matcher,
                stdin.lock(),
                "<stdin>",
                cli.before_context_lines(),
                cli.after_context_lines(),
                max_count,
            )?;
            found_any = !matches.is_empty();
            print_context_results("<stdin>", &matches, cli);
        } else {
            let result = search_stdin(&matcher, max_count)?;
            found_any = !result.matches.is_empty();
            print_results(&result, cli);
        }
    } else {
        // Search files/directories
        for path in &cli.paths {
            let multi = search_path(
                &matcher,
                path,
                cli.recursive,
                &cli.include,
                &cli.exclude,
                max_count,
                cli.silent,
            );

            for result in &multi.results {
                if !result.matches.is_empty() {
                    found_any = true;
                }
                print_results(result, cli);
            }
        }
    }

    if !found_any {
        process::exit(1);  // grep convention: exit 1 if no matches
    }

    Ok(())
}

/// Print results for a single source (simplified — T03 adds colors)
fn print_results(result: &SearchResult, cli: &Cli) {
    let show_filename = cli.show_filename();

    if cli.count {
        // -c: only print count
        if show_filename {
            println!("{}:{}", result.source, result.matches.len());
        } else {
            println!("{}", result.matches.len());
        }
        return;
    }

    if cli.files_with_matches {
        // -l: only print filename if matches exist
        if !result.matches.is_empty() {
            println!("{}", result.source);
        }
        return;
    }

    // Normal output
    for m in &result.matches {
        let mut prefix = String::new();

        if show_filename {
            prefix.push_str(&result.source);
            prefix.push(':');
        }

        if cli.line_number {
            prefix.push_str(&m.line_number.to_string());
            prefix.push(':');
        }

        println!("{}{}", prefix, m.line);
    }
}

fn print_context_results(source: &str, matches: &[ContextMatch], cli: &Cli) {
    let groups = group_context_matches(matches);

    for (group_idx, group) in groups.iter().enumerate() {
        if group_idx > 0 {
            println!("--");  // Group separator
        }

        // Track which lines we've printed to avoid duplicates
        let mut printed_lines = std::collections::HashSet::new();

        for m in group {
            // Print before-context
            for (num, line) in &m.before {
                if printed_lines.insert(*num) {
                    if cli.line_number {
                        println!("{}-{}", num, line);  // '-' for context lines
                    } else {
                        println!("{}", line);
                    }
                }
            }

            // Print matching line
            if printed_lines.insert(m.line_number) {
                if cli.line_number {
                    println!("{}:{}", m.line_number, m.line);  // ':' for match
                } else {
                    println!("{}", m.line);
                }
            }

            // Print after-context
            for (num, line) in &m.after {
                if printed_lines.insert(*num) {
                    if cli.line_number {
                        println!("{}-{}", num, line);
                    } else {
                        println!("{}", line);
                    }
                }
            }
        }
    }
}
```

### Exit codes (convención grep)

```
┌──────────────────────────────────────────────────────────────┐
│  Exit code  │ Significado                                    │
├─────────────┼────────────────────────────────────────────────┤
│      0      │ Al menos un match encontrado                   │
│      1      │ Ningún match encontrado                        │
│      2      │ Error (archivo no encontrado, regex inválida)  │
└─────────────┴────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. No manejar errores de I/O por archivo

```rust
// ❌ Un archivo inaccesible mata toda la búsqueda
for path in &cli.paths {
    let content = fs::read_to_string(path)?;  // Error → se detiene todo
    search(&content);
}

// ✅ Reportar error y continuar con el siguiente archivo
for path in &cli.paths {
    match fs::read_to_string(path) {
        Ok(content) => search(&content),
        Err(e) => eprintln!("minigrep: {}: {}", path.display(), e),
    }
}
```

### 2. Cargar todo el archivo en memoria para búsqueda simple

```rust
// ❌ Lee todo el archivo antes de buscar (problema con archivos de GB)
let content = fs::read_to_string(path)?;
for line in content.lines() {
    if matcher.is_match(line) { /* ... */ }
}

// ✅ Leer línea por línea con BufReader (memoria constante)
let file = File::open(path)?;
let reader = BufReader::new(file);
for line in reader.lines() {
    let line = line?;
    if matcher.is_match(&line) { /* ... */ }
}

// ⚠️ Excepción: la búsqueda con contexto NECESITA acceso a líneas
// anteriores. Usa un buffer circular (VecDeque) de tamaño fijo,
// o carga el archivo entero si necesitas contexto.
```

### 3. No escapar el patrón en modo fixed-string

```rust
// ❌ El usuario escribe "file.txt" esperando búsqueda literal
// pero regex interpreta "." como "cualquier carácter"
let re = Regex::new("file.txt").unwrap();
assert!(re.is_match("fileTtxt"));  // ¡Match falso!

// ✅ Usar regex::escape para modo fixed-string (-F)
let escaped = regex::escape("file.txt");  // "file\\.txt"
let re = Regex::new(&escaped).unwrap();
assert!(!re.is_match("fileTtxt"));  // Correcto: no match
assert!(re.is_match("file.txt"));   // Correcto: match
```

### 4. Seguir symlinks circulares en búsqueda recursiva

```bash
# Crear un symlink circular
ln -s /home/user/project /home/user/project/self_link
# Recursión infinita si sigues symlinks

# walkdir con follow_links(false) evita este problema
# std::fs::read_dir requiere manejo manual
```

### 5. No usar BufReader al abrir archivos

```rust
// ❌ File sin buffering = una syscall read() por cada byte/línea
let file = File::open(path)?;
// io::BufRead methods call read() directamente en file

// ✅ BufReader agrega un buffer de 8KB
let file = File::open(path)?;
let reader = BufReader::new(file);
// Ahora read() se llama cada 8KB, no cada línea
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          BÚSQUEDA EN ARCHIVOS CHEATSHEET                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  REGEX CRATE                                                 │
│  regex = "1"                         Cargo.toml              │
│  let re = Regex::new(r"pattern")?;   Compilar                │
│  re.is_match(text)                   ¿Hay match?             │
│  re.find(text)                       Primer match            │
│  re.captures(text)                   Grupos de captura       │
│  re.captures_iter(text)              Todos los captures      │
│  regex::escape("literal")           Escapar metacaracteres  │
│                                                              │
│  REGEX BUILDER                                               │
│  RegexBuilder::new(pattern)                                  │
│    .case_insensitive(true)           (?i)                    │
│    .multi_line(true)                 ^ $ por línea           │
│    .build()?                                                 │
│                                                              │
│  LEER ARCHIVOS                                               │
│  BufReader::new(file)                Lectura buffered        │
│  reader.lines()                      Iterador de líneas      │
│  fs::read_to_string(path)            Todo a memoria          │
│                                                              │
│  RECORRIDO RECURSIVO                                         │
│  walkdir = "2"                       Crate                   │
│  WalkDir::new(dir)                   Crear walker            │
│    .follow_links(false)              No seguir symlinks      │
│    .into_iter()                      Iterar entradas         │
│    .filter_entry(|e| ...)            Filtrar directorios     │
│                                                              │
│  ARQUITECTURA                                                │
│  Matcher     Abstracción regex/literal, case, word, invert   │
│  MatchResult Línea + número + posición del match             │
│  search_reader(matcher, R, name)     Genérico sobre Read     │
│  search_file(matcher, path)          Archivo específico      │
│  search_stdin(matcher)               Entrada estándar        │
│  search_files(matcher, paths)        Múltiples archivos      │
│  search_path(matcher, path, recur)   Auto-detecta file/dir  │
│                                                              │
│  CONTEXTO (-A, -B, -C)                                       │
│  Cargar todas las líneas en Vec                              │
│  Acceder a lines[idx-B..idx] y lines[idx+1..idx+1+A]        │
│  Agrupar matches cercanos para evitar duplicados             │
│  Separador "--" entre grupos                                 │
│                                                              │
│  EXIT CODES                                                  │
│  0 = match found   1 = no match   2 = error                 │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Motor de búsqueda básico

Implementa el motor de búsqueda para minigrep:

```
cargo new minigrep
cd minigrep
```

**Tareas:**
1. Implementa `Matcher::new` y `Matcher::is_match` con soporte para:
   - Regex (por defecto)
   - Fixed strings (`-F`)
   - Case insensitive (`-i`)
2. Implementa `search_reader` genérico sobre `Read`
3. Escribe al menos 5 tests con `Cursor::new` para:
   - Match literal simple
   - Regex con metacaracteres (`\d+`, `^pattern`, `end$`)
   - Case insensitive
   - Sin matches
   - Fixed string con caracteres regex (`file.txt`, `[test]`)
4. Integra con la CLI de T01: `cargo run -- "fn \w+" src/main.rs` debe funcionar

**Pregunta de reflexión:** ¿Por qué `search_reader` es genérico sobre `Read`
en vez de tomar directamente un `&Path`? ¿Qué beneficios tiene para testing
y para soportar stdin?

---

### Ejercicio 2: Búsqueda recursiva con filtrado

Extiende minigrep para buscar en directorios:

**Tareas:**
1. Agrega el crate `walkdir` a tu proyecto
2. Implementa `collect_files_filtered` que:
   - Recorra directorios recursivamente
   - Omita directorios ocultos (`.git`, `.cache`, etc.)
   - Omita `target/` y `node_modules/`
   - Filtre por `--include` y `--exclude` globs
3. Prueba con tu propio proyecto:
   ```bash
   cargo run -- -r --include "*.rs" "fn main" .
   ```
4. Maneja el caso de directorio sin `-r` (error, como grep real)
5. Compara la salida con `grep -rn --include="*.rs" "fn main" .`

**Pregunta de reflexión:** ¿Cuál es la diferencia entre filtrar directorios
en `filter_entry` (antes de descender) vs filtrar archivos después de
recopilarlos? ¿Qué impacto tiene en rendimiento con un directorio como
`node_modules` con miles de archivos?

---

### Ejercicio 3: Contexto de líneas

Implementa las opciones `-A`, `-B`, y `-C`:

**Tareas:**
1. Implementa `search_with_context` que capture líneas antes y después
2. Implementa la lógica de agrupación para evitar contexto duplicado
3. Prueba con un archivo que tenga matches cercanos:
   ```
   line 1
   line 2: match
   line 3
   line 4: match
   line 5
   ```
   Con `-C 1`, las líneas 1-5 deben imprimirse como un solo bloque
   (sin separador `--`), no como dos bloques separados
4. Prueba con matches lejanos (deben separarse con `--`)
5. Verifica que `-n` muestra `:` para matches y `-` para contexto:
   ```
   1-line 1
   2:line 2: match
   3-line 3
   4:line 4: match
   5-line 5
   ```

**Pregunta de reflexión:** La implementación actual carga todas las líneas en
un `Vec` para soportar contexto. ¿Cómo implementarías contexto de forma
streaming (sin cargar todo el archivo)? Pista: `VecDeque` como buffer circular
de tamaño `before_count`, y un contador de "líneas pendientes después del
último match".