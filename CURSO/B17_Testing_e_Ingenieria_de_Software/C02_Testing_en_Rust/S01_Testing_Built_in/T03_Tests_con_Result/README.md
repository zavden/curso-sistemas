# T03 — Tests con `Result<(), E>`: el operador `?` en tests, errores descriptivos

> **Bloque 17 — Testing e Ingeniería de Software → C02 — Testing en Rust → S01 — Testing Built-in → T03**

---

## Índice

1. [El problema: assert! + unwrap() en tests con operaciones falibles](#1-el-problema-assert--unwrap-en-tests-con-operaciones-falibles)
2. [Tests que retornan Result<(), E>](#2-tests-que-retornan-result-e)
3. [El operador ? en tests](#3-el-operador--en-tests)
4. [Tipos de error compatibles](#4-tipos-de-error-compatibles)
5. [Box<dyn Error> como tipo de error universal](#5-boxdyn-error-como-tipo-de-error-universal)
6. [Errores descriptivos: mejorar los mensajes](#6-errores-descriptivos-mejorar-los-mensajes)
7. [Patrones con Result en tests](#7-patrones-con-result-en-tests)
8. [Verificar errores específicos con Result](#8-verificar-errores-específicos-con-result)
9. [Combinar Result con assert!](#9-combinar-result-con-assert)
10. [Result vs assert! vs should_panic: árbol de decisión](#10-result-vs-assert-vs-should_panic-árbol-de-decisión)
11. [El crate anyhow para tests](#11-el-crate-anyhow-para-tests)
12. [Comparación con C y Go](#12-comparación-con-c-y-go)
13. [Errores comunes](#13-errores-comunes)
14. [Ejemplo completo: cliente de base de datos en memoria](#14-ejemplo-completo-cliente-de-base-de-datos-en-memoria)
15. [Programa de práctica](#15-programa-de-práctica)
16. [Ejercicios](#16-ejercicios)

---

## 1. El problema: `assert!` + `unwrap()` en tests con operaciones falibles

### Código típico sin Result en tests

Cuando tu código usa `Result`, los tests frecuentemente se llenan de `.unwrap()`:

```rust
use std::fs;
use std::io;

pub fn count_lines(path: &str) -> io::Result<usize> {
    let content = fs::read_to_string(path)?;
    Ok(content.lines().count())
}

pub fn parse_csv_line(line: &str) -> Result<Vec<String>, String> {
    if line.is_empty() {
        return Err("Empty line".to_string());
    }
    Ok(line.split(',').map(|s| s.trim().to_string()).collect())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    #[test]
    fn test_count_lines() {
        // Cada operación que puede fallar necesita .unwrap()
        let dir = tempfile::tempdir().unwrap();            // unwrap #1
        let path = dir.path().join("test.txt");
        let mut file = fs::File::create(&path).unwrap();   // unwrap #2
        write!(file, "line1\nline2\nline3").unwrap();       // unwrap #3
        drop(file);

        let count = count_lines(path.to_str().unwrap()).unwrap();  // unwrap #4, #5
        assert_eq!(count, 3);
    }

    #[test]
    fn test_parse_csv() {
        let fields = parse_csv_line("a, b, c").unwrap();   // unwrap
        assert_eq!(fields, vec!["a", "b", "c"]);
    }
}
```

### Problemas con `.unwrap()` en tests

```
Problema                              Consecuencia
──────────────────────────────────────────────────────────────────────────
Ruido visual                          5 unwrap() oscurecen la lógica del test
Mensajes de error pobres              "called unwrap() on Err(...)" sin contexto
No indica qué operación falló         ¿Fue el create? ¿El write? ¿El read?
Inconsistente con código de prod      Producción usa ?, tests usan unwrap
Tentación de ignorar errores          let _ = operation() para "simplificar"
```

Mensaje de error típico con `unwrap()`:

```
thread 'tests::test_count_lines' panicked at
  'called `Result::unwrap()` on an `Err` value: Os { code: 2,
   kind: NotFound, message: "No such file or directory" }',
  src/lib.rs:15:52
```

¿Cuál de los 5 `unwrap()` falló? Hay que ir a la línea 15 para averiguarlo.

---

## 2. Tests que retornan `Result<(), E>`

### La solución: el test retorna Result

Desde Rust 1.28, las funciones de test pueden retornar `Result<(), E>` donde `E: Debug`. Si el test retorna `Ok(())`, pasa. Si retorna `Err(e)`, falla mostrando `e` con `Debug`.

```rust
#[test]
fn test_count_lines() -> Result<(), Box<dyn std::error::Error>> {
    let dir = tempfile::tempdir()?;              // ? en vez de unwrap
    let path = dir.path().join("test.txt");
    let mut file = fs::File::create(&path)?;     // ? limpio
    write!(file, "line1\nline2\nline3")?;         // ? limpio
    drop(file);

    let count = count_lines(path.to_str().unwrap())?;  // ?
    assert_eq!(count, 3);
    Ok(())                                        // Test pasa
}
```

### Comparación visual

```rust
// ── ANTES: con unwrap() ──
#[test]
fn test_before() {
    let data = read_file("input.txt").unwrap();
    let parsed = parse_data(&data).unwrap();
    let result = process(parsed).unwrap();
    let output = format_output(result).unwrap();
    assert_eq!(output, "expected");
}

// ── DESPUÉS: con Result + ? ──
#[test]
fn test_after() -> Result<(), Box<dyn std::error::Error>> {
    let data = read_file("input.txt")?;
    let parsed = parse_data(&data)?;
    let result = process(parsed)?;
    let output = format_output(result)?;
    assert_eq!(output, "expected");
    Ok(())
}
```

### Semántica

```
Retorno del test       Resultado
──────────────────────────────────
Ok(())                 PASS
Err(e)                 FAIL — muestra e con Debug
panic!                 FAIL — muestra panic message (igual que siempre)
```

El test puede fallar de dos formas: por `Err` (propagado por `?`) o por `panic!` (de un `assert!`). Ambas se reportan como FAIL.

### Mensaje de error con Result

Cuando un test falla por `Err`:

```
running 1 test
test tests::test_count_lines ... FAILED

failures:

---- tests::test_count_lines stdout ----
Error: Os { code: 2, kind: NotFound, message: "No such file or directory" }

failures:
    tests::test_count_lines

test result: FAILED. 0 passed; 1 failed
```

El error muestra la representación `Debug` del error. Es más limpio que el mensaje de `unwrap()` porque no incluye "called unwrap() on Err value" ni el stack trace de panic.

---

## 3. El operador `?` en tests

### Cómo funciona `?` en contexto de test

El operador `?` hace lo mismo que en código normal:

```rust
// Esto:
let value = operation()?;

// Es equivalente a:
let value = match operation() {
    Ok(v) => v,
    Err(e) => return Err(e.into()),  // Convierte y retorna el error
};
```

En un test que retorna `Result<(), E>`, el `?` propaga el error como valor de retorno. El test harness detecta el `Err` y reporta FAIL.

### Cadena de operaciones con ?

```rust
#[test]
fn test_pipeline() -> Result<(), Box<dyn std::error::Error>> {
    // Cada ? propaga el error si falla
    let config = Config::load("test.toml")?;          // IoError
    let db = Database::connect(&config.db_url)?;       // DbError
    let users = db.query("SELECT * FROM users")?;      // DbError
    let json = serde_json::to_string(&users)?;          // SerdeError

    assert!(json.contains("Alice"));
    Ok(())
}

// Si Config::load falla, el test se detiene ahí y muestra el IoError
// Si db.query falla, muestra el DbError
// Cada error se muestra con su tipo y contexto específico
```

### ? con Option (usando .ok_or())

`?` funciona directamente con `Result`, pero para `Option` necesitas convertir:

```rust
#[test]
fn test_with_option() -> Result<(), Box<dyn std::error::Error>> {
    let items = vec![1, 2, 3, 4, 5];

    // Option → Result con ok_or/ok_or_else
    let first = items.first().ok_or("empty list")?;
    assert_eq!(*first, 1);

    let found = items.iter().find(|&&x| x == 3).ok_or("not found")?;
    assert_eq!(*found, 3);

    Ok(())
}
```

### ? con conversión de errores

El `.into()` implícito en `?` permite convertir entre tipos de error si implementan `From`:

```rust
use std::num::ParseIntError;

#[derive(Debug)]
enum TestError {
    Io(std::io::Error),
    Parse(ParseIntError),
    Custom(String),
}

impl From<std::io::Error> for TestError {
    fn from(e: std::io::Error) -> Self { TestError::Io(e) }
}

impl From<ParseIntError> for TestError {
    fn from(e: ParseIntError) -> Self { TestError::Parse(e) }
}

#[test]
fn test_with_custom_error() -> Result<(), TestError> {
    let content = std::fs::read_to_string("numbers.txt")?;  // io::Error → TestError
    let num: i32 = content.trim().parse()?;                   // ParseIntError → TestError
    assert!(num > 0);
    Ok(())
}
```

---

## 4. Tipos de error compatibles

### Requisitos

Para que un test retorne `Result<(), E>`, `E` debe implementar `Debug`:

```rust
// FUNCIONA: String implementa Debug
#[test]
fn test_a() -> Result<(), String> {
    Ok(())
}

// FUNCIONA: io::Error implementa Debug
#[test]
fn test_b() -> Result<(), std::io::Error> {
    Ok(())
}

// FUNCIONA: Box<dyn Error> implementa Debug
#[test]
fn test_c() -> Result<(), Box<dyn std::error::Error>> {
    Ok(())
}

// FUNCIONA: tipo custom con Debug
#[derive(Debug)]
struct MyError(String);

#[test]
fn test_d() -> Result<(), MyError> {
    Ok(())
}

// NO FUNCIONA: tipo sin Debug
struct NoDebugError;

#[test]
fn test_e() -> Result<(), NoDebugError> {
    // ERROR: `NoDebugError` doesn't implement `Debug`
    Ok(())
}
```

### Tipos de error comunes en tests

```
Tipo de error                    Uso típico                     Flexibilidad
──────────────────────────────────────────────────────────────────────────────
String                           Tests simples                   Baja (solo texto)
&'static str                     Mensajes literales              Baja
std::io::Error                   Tests de I/O                    Media (solo I/O)
Box<dyn std::error::Error>       Tests que mezclan errores       Alta (acepta todo)
anyhow::Error                    Tests complejos (ver sec 11)    Muy alta
Custom enum con Debug            Tests del mismo crate           Media (tipado)
```

---

## 5. `Box<dyn Error>` como tipo de error universal

### El tipo más versátil para tests

`Box<dyn std::error::Error>` acepta **cualquier** tipo que implemente el trait `Error`. Esto incluye:

```
std::io::Error                 ✓ acepta
std::num::ParseIntError        ✓ acepta
std::num::ParseFloatError      ✓ acepta
serde_json::Error              ✓ acepta
String                         ✓ acepta (via From<String>)
&str                           ✓ acepta (via From<&str>)
Custom types que impl Error    ✓ acepta
```

### Patrón estándar

```rust
use std::error::Error;

#[test]
fn test_mixed_errors() -> Result<(), Box<dyn Error>> {
    // io::Error
    let content = std::fs::read_to_string("data.txt")?;

    // ParseIntError
    let first_line = content.lines().next().ok_or("empty file")?;
    let number: i32 = first_line.parse()?;

    // Custom string error
    if number < 0 {
        return Err("expected positive number".into());
    }

    assert!(number > 0);
    Ok(())
}
```

### El truco de `.into()`

Para convertir un `&str` o `String` a `Box<dyn Error>`, usa `.into()`:

```rust
#[test]
fn test_with_string_error() -> Result<(), Box<dyn std::error::Error>> {
    let items = vec![1, 2, 3];

    // &str → Box<dyn Error> via .into()
    let first = items.first().ok_or("list is empty")?;

    // String → Box<dyn Error> via .into()
    if *first != 1 {
        return Err(format!("expected 1, got {}", first).into());
    }

    Ok(())
}
```

### Type alias para simplicidad

```rust
#[cfg(test)]
type TestResult = Result<(), Box<dyn std::error::Error>>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_a() -> TestResult {
        // ... mucho más limpio que escribir el tipo completo
        Ok(())
    }

    #[test]
    fn test_b() -> TestResult {
        let x: i32 = "42".parse()?;
        assert_eq!(x, 42);
        Ok(())
    }
}
```

Este patrón es muy popular en la comunidad Rust. El alias `TestResult` ahorra tipear `Result<(), Box<dyn std::error::Error>>` en cada test.

---

## 6. Errores descriptivos: mejorar los mensajes

### Problema: errores sin contexto

```rust
#[test]
fn test_process() -> Result<(), Box<dyn std::error::Error>> {
    let config = load_config("test.toml")?;
    let data = fetch_data(&config)?;
    let result = process(&data)?;
    assert_eq!(result.status, "ok");
    Ok(())
}
```

Si `fetch_data` falla, verás algo como:

```
Error: Connection refused (os error 111)
```

¿Qué operación falló? ¿Con qué parámetros? No está claro.

### Solución 1: `.map_err()` para añadir contexto

```rust
#[test]
fn test_process() -> Result<(), Box<dyn std::error::Error>> {
    let config = load_config("test.toml")
        .map_err(|e| format!("Failed to load config: {e}"))?;

    let data = fetch_data(&config)
        .map_err(|e| format!("Failed to fetch data from {}: {e}", config.url))?;

    let result = process(&data)
        .map_err(|e| format!("Failed to process {} records: {e}", data.len()))?;

    assert_eq!(result.status, "ok");
    Ok(())
}
```

Ahora el error dice:

```
Error: "Failed to fetch data from https://api.example.com: Connection refused (os error 111)"
```

### Solución 2: helper que añade contexto

```rust
#[cfg(test)]
trait TestContext<T, E: std::fmt::Display> {
    fn context(self, msg: &str) -> Result<T, String>;
}

impl<T, E: std::fmt::Display> TestContext<T, E> for Result<T, E> {
    fn context(self, msg: &str) -> Result<T, String> {
        self.map_err(|e| format!("{}: {}", msg, e))
    }
}

#[test]
fn test_with_context() -> Result<(), Box<dyn std::error::Error>> {
    let content = std::fs::read_to_string("input.txt")
        .context("reading input file")?;

    let count: usize = content.trim().parse()
        .context("parsing line count")?;

    assert!(count > 0);
    Ok(())
}
```

Error resultante:

```
Error: "reading input file: No such file or directory (os error 2)"
```

### Solución 3: crear errores descriptivos con format!

```rust
#[test]
fn test_parse_all() -> Result<(), Box<dyn std::error::Error>> {
    let inputs = vec!["42", "17", "abc", "99"];

    for (i, input) in inputs.iter().enumerate() {
        let _num: i32 = input.parse()
            .map_err(|e| format!("Input #{i} ({input:?}): {e}"))?;
    }

    Ok(())
}
```

Error:

```
Error: "Input #2 (\"abc\"): invalid digit found in string"
```

El índice y el valor del input que falló están en el mensaje.

---

## 7. Patrones con `Result` en tests

### Patrón 1: setup falible

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::error::Error;

    type TestResult = Result<(), Box<dyn Error>>;

    fn setup_test_db() -> Result<Database, Box<dyn Error>> {
        let db = Database::open(":memory:")?;
        db.execute("CREATE TABLE users (id INT, name TEXT)")?;
        db.execute("INSERT INTO users VALUES (1, 'Alice')")?;
        db.execute("INSERT INTO users VALUES (2, 'Bob')")?;
        Ok(db)
    }

    #[test]
    fn test_find_user() -> TestResult {
        let db = setup_test_db()?;   // Setup falible con ?
        let user = db.find(1)?;
        assert_eq!(user.name, "Alice");
        Ok(())
    }

    #[test]
    fn test_count_users() -> TestResult {
        let db = setup_test_db()?;
        let count = db.count("users")?;
        assert_eq!(count, 2);
        Ok(())
    }
}
```

### Patrón 2: pipeline de transformaciones

```rust
#[test]
fn test_data_pipeline() -> TestResult {
    // Leer → parsear → transformar → serializar → verificar
    let raw = std::fs::read_to_string("test_data/input.csv")?;
    let records: Vec<Record> = parse_csv(&raw)?;
    let processed: Vec<Summary> = records.iter()
        .map(|r| summarize(r))
        .collect::<Result<Vec<_>, _>>()?;
    let output = serde_json::to_string_pretty(&processed)?;

    assert!(output.contains("\"total\":"));
    assert!(!processed.is_empty());
    Ok(())
}
```

### Patrón 3: test con archivos temporales

```rust
use std::io::Write;

#[test]
fn test_file_roundtrip() -> TestResult {
    // Crear archivo temporal
    let dir = tempfile::tempdir()?;
    let path = dir.path().join("data.json");

    // Escribir
    let original = Config { port: 8080, host: "localhost".into() };
    let json = serde_json::to_string(&original)?;
    std::fs::write(&path, &json)?;

    // Leer y verificar
    let content = std::fs::read_to_string(&path)?;
    let loaded: Config = serde_json::from_str(&content)?;

    assert_eq!(loaded.port, 8080);
    assert_eq!(loaded.host, "localhost");
    Ok(())
    // dir se destruye aquí (RAII) → archivos temporales eliminados
}
```

### Patrón 4: múltiples verificaciones independientes

```rust
#[test]
fn test_user_validation() -> TestResult {
    let validator = UserValidator::new();

    // Cada validación puede retornar error
    let valid_user = User::parse("Alice, alice@example.com, 30")?;
    validator.validate(&valid_user)?;

    // Para errores esperados, NO usar ? — verificar el Err
    let invalid = User::parse("Bob, not-an-email, -5")?;
    let result = validator.validate(&invalid);
    assert!(result.is_err());

    let err = result.unwrap_err();
    assert!(err.to_string().contains("invalid email"));

    Ok(())
}
```

### Patrón 5: test con conversión entre tipos de error

```rust
#[test]
fn test_mixed_operations() -> TestResult {
    // io::Error
    let bytes = std::fs::read("test_data/image.png")?;

    // Custom error
    let header = parse_png_header(&bytes)
        .map_err(|e| format!("PNG header: {e}"))?;

    // Assertion (panic, no Result)
    assert_eq!(header.width, 800);
    assert_eq!(header.height, 600);

    // &str como error
    let palette = header.palette.ok_or("no palette in test image")?;
    assert!(palette.len() > 0);

    Ok(())
}
```

---

## 8. Verificar errores específicos con `Result`

### Verificar que una operación retorna Err

Cuando quieres verificar que algo falla, **no uses `?`** — necesitas capturar el `Result` y verificar el `Err`:

```rust
#[test]
fn test_parse_invalid() -> TestResult {
    // NO usar ? aquí — queremos verificar el error
    let result = "abc".parse::<i32>();
    assert!(result.is_err());

    // Verificar el mensaje de error
    let err = result.unwrap_err();
    assert!(err.to_string().contains("invalid digit"));

    Ok(())
}
```

### Verificar el tipo exacto de error

```rust
#[derive(Debug, PartialEq)]
pub enum AppError {
    NotFound(String),
    PermissionDenied,
    InvalidInput { field: String, reason: String },
}

#[test]
fn test_not_found() -> TestResult {
    let result = find_user("nonexistent");

    assert!(matches!(
        result,
        Err(AppError::NotFound(ref name)) if name == "nonexistent"
    ));

    Ok(())
}

#[test]
fn test_invalid_input_details() -> TestResult {
    let result = create_user("", "bad@");

    match result {
        Err(AppError::InvalidInput { ref field, ref reason }) => {
            assert_eq!(field, "name");
            assert!(reason.contains("empty"));
        }
        other => {
            return Err(format!("Expected InvalidInput, got {:?}", other).into());
        }
    }

    Ok(())
}
```

### Patrón: assert_err helper

```rust
#[cfg(test)]
fn assert_err<T: std::fmt::Debug, E: std::fmt::Debug>(
    result: &Result<T, E>,
    expected_substring: &str,
) {
    match result {
        Ok(v) => panic!(
            "Expected Err containing '{}', got Ok({:?})",
            expected_substring, v
        ),
        Err(e) => {
            let msg = format!("{:?}", e);
            assert!(
                msg.contains(expected_substring),
                "Error '{:?}' does not contain '{}'",
                e, expected_substring
            );
        }
    }
}

#[test]
fn test_validation_errors() -> TestResult {
    assert_err(&parse_email(""), "empty");
    assert_err(&parse_email("no-at"), "missing @");
    assert_err(&parse_email("@no-local"), "empty local part");

    Ok(())
}
```

### Verificar que el error es de un tipo específico con downcast

```rust
#[test]
fn test_error_type() -> TestResult {
    let result: Result<(), Box<dyn std::error::Error>> = do_io_operation();

    if let Err(e) = &result {
        // Verificar que es un io::Error
        assert!(
            e.downcast_ref::<std::io::Error>().is_some(),
            "Expected io::Error, got: {:?}", e
        );

        // Verificar el tipo de io::Error
        let io_err = e.downcast_ref::<std::io::Error>().unwrap();
        assert_eq!(io_err.kind(), std::io::ErrorKind::NotFound);
    } else {
        return Err("Expected error but got Ok".into());
    }

    Ok(())
}
```

---

## 9. Combinar `Result` con `assert!`

### Result + assert! en el mismo test

En la práctica, la mayoría de tests combinan ambos estilos:

```rust
#[test]
fn test_user_creation() -> TestResult {
    // ── Parte 1: operaciones falibles con ? ──
    let db = Database::connect(":memory:")?;
    db.migrate()?;

    let user = User::new("Alice", "alice@test.com")?;
    db.insert(&user)?;

    // ── Parte 2: verificaciones con assert! ──
    let found = db.find_by_email("alice@test.com")?;
    assert_eq!(found.name, "Alice");
    assert_eq!(found.email, "alice@test.com");
    assert!(found.id > 0);
    assert!(found.created_at.is_some());

    // ── Parte 3: verificar errores ──
    let duplicate = db.insert(&user);
    assert!(duplicate.is_err(), "Duplicate insert should fail");

    Ok(())
}
```

### Cómo falla cada parte

```
Si falla el ?:
  Error: DbError("Connection failed: ...")
  → Mensaje del tipo de error, con Debug

Si falla assert_eq!:
  thread 'test_user_creation' panicked at
  'assertion `left == right` failed
    left: "Bob"
   right: "Alice"'
  → Mensaje de panic con valores left/right

Si falla assert! con mensaje:
  thread 'test_user_creation' panicked at
  'Duplicate insert should fail'
  → Mensaje de panic custom
```

### Regla práctica

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  Regla para combinar ? y assert! en el mismo test:             ║
 ║                                                                  ║
 ║  • Usa ? para SETUP (operaciones que deben funcionar para       ║
 ║    que el test tenga sentido: crear DB, abrir archivos, etc.)   ║
 ║                                                                  ║
 ║  • Usa assert! para VERIFICACIONES (lo que realmente estás      ║
 ║    testeando: valores correctos, estado esperado, etc.)         ║
 ║                                                                  ║
 ║  • Usa is_err()/is_ok() para verificar que algo DEBE fallar    ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 10. `Result` vs `assert!` vs `should_panic`: árbol de decisión

### El árbol

```
  ¿Qué estoy testeando?
  │
  ├── La función DEBE hacer panic (precondición violada)
  │   └── Usar #[should_panic(expected = "...")]
  │       Ejemplo: vec.get(out_of_bounds)
  │
  ├── La función DEBE retornar Err (operación falible)
  │   └── Usar assert!(result.is_err()) o matches!
  │       NO usar ? aquí (eso propagaría el error como fallo del test)
  │       Ejemplo: "abc".parse::<i32>() debe ser Err
  │
  ├── La función DEBE retornar Ok con un valor específico
  │   └── Usar ? + assert_eq!
  │       Ejemplo: "42".parse::<i32>()? debe ser 42
  │
  └── El test necesita setup que puede fallar
      └── Usar -> Result<(), Box<dyn Error>> con ?
          Ejemplo: crear archivo temporal, conectar DB, etc.
```

### Tabla comparativa

```
Escenario                    Herramienta           Ejemplo
──────────────────────────────────────────────────────────────────────────────
Verificar valor correcto     assert_eq!            assert_eq!(add(2,3), 5)

Verificar condición          assert!               assert!(x > 0)

Verificar que retorna Err    assert!(is_err())     assert!("abc".parse::<i32>().is_err())

Verificar tipo de Err        matches!              assert!(matches!(r, Err(E::NotFound(_))))

Verificar panic              #[should_panic]       divide(1, 0) con expected

Setup falible                ? con Result return    let db = open_db()?

Pipeline falible             ? encadenado           let x = a()?; let y = b(x)?

Cleanup automático           RAII (Drop)           TempDir, File se cierran solos
```

### Ejemplo que usa las tres técnicas

```rust
pub struct Stack<T> {
    items: Vec<T>,
    max: usize,
}

impl<T: Clone> Stack<T> {
    pub fn new(max: usize) -> Result<Self, &'static str> {
        if max == 0 { return Err("max must be positive"); }
        Ok(Stack { items: Vec::new(), max })
    }

    pub fn push(&mut self, item: T) -> Result<(), String> {
        if self.items.len() >= self.max {
            return Err(format!("stack full (max={})", self.max));
        }
        self.items.push(item);
        Ok(())
    }

    pub fn pop(&mut self) -> T {
        // panic si vacío (precondición: no llamar en stack vacío)
        self.items.pop().expect("pop on empty stack")
    }

    pub fn peek(&self) -> Option<&T> {
        self.items.last()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    type TestResult = Result<(), Box<dyn std::error::Error>>;

    // ── Result: operación que retorna Err ──
    #[test]
    fn test_new_zero_max() {
        let result = Stack::<i32>::new(0);
        assert!(result.is_err());
        assert_eq!(result.unwrap_err(), "max must be positive");
    }

    // ── Result + ?: setup falible ──
    #[test]
    fn test_push_and_pop() -> TestResult {
        let mut stack = Stack::new(5)?;  // ? para setup
        stack.push(1)?;
        stack.push(2)?;
        stack.push(3)?;

        assert_eq!(stack.pop(), 3);      // assert para verificar
        assert_eq!(stack.pop(), 2);
        Ok(())
    }

    // ── Result: verificar que push falla cuando lleno ──
    #[test]
    fn test_push_full() -> TestResult {
        let mut stack = Stack::new(2)?;
        stack.push(1)?;
        stack.push(2)?;

        let result = stack.push(3);       // NO usar ? — queremos el Err
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("stack full"));
        Ok(())
    }

    // ── should_panic: precondición violada ──
    #[test]
    #[should_panic(expected = "pop on empty stack")]
    fn test_pop_empty() {
        // No puede retornar Result porque should_panic no lo permite
        let mut stack = Stack::<i32>::new(5).unwrap();
        stack.pop();
    }

    // ── assert!: verificar Option ──
    #[test]
    fn test_peek() -> TestResult {
        let mut stack = Stack::new(5)?;
        assert!(stack.peek().is_none());

        stack.push(42)?;
        assert_eq!(stack.peek(), Some(&42));
        Ok(())
    }
}
```

---

## 11. El crate `anyhow` para tests

### ¿Qué es anyhow?

`anyhow` es un crate que proporciona un tipo de error flexible `anyhow::Error` con contexto automático. Es muy popular para tests y aplicaciones (no para bibliotecas).

### Cargo.toml

```toml
[dev-dependencies]
anyhow = "1"
```

`dev-dependencies` significa que solo se compila para tests, ejemplos y benchmarks. No afecta al binario de producción.

### Uso en tests

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use anyhow::{Context, Result};

    #[test]
    fn test_file_processing() -> Result<()> {
        let content = std::fs::read_to_string("test_data/input.txt")
            .context("Failed to read test input file")?;

        let count: usize = content.trim().parse()
            .context("Failed to parse line count")?;

        assert!(count > 0, "Expected positive count");
        Ok(())
    }
}
```

### Ventajas sobre `Box<dyn Error>`

```
Característica              Box<dyn Error>              anyhow::Error
──────────────────────────────────────────────────────────────────────────
Tipo de retorno             Result<(), Box<dyn Error>>  anyhow::Result<()>
Contexto                    .map_err(|e| format!(...))  .context("message")
Cadena de errores           Manual                      Automática
Backtrace                   No                          Sí (con RUST_BACKTRACE=1)
From implementations        Automáticas                 Automáticas
Ergonomía                   Buena                       Excelente
Dependencia                 Ninguna (std)               anyhow crate
```

### context() vs map_err()

```rust
// Sin anyhow: verboso
let file = File::open(path)
    .map_err(|e| format!("Failed to open {}: {}", path, e))?;

// Con anyhow: conciso
let file = File::open(path)
    .with_context(|| format!("Failed to open {}", path))?;

// Contexto estático (sin closure):
let file = File::open(path)
    .context("Failed to open configuration file")?;
```

### Ejemplo completo con anyhow

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use anyhow::{bail, ensure, Context, Result};

    #[test]
    fn test_config_loading() -> Result<()> {
        let config = Config::load("test.toml")
            .context("Loading test config")?;

        // ensure! es como assert! pero retorna Err en vez de panic
        ensure!(config.port > 0, "Port must be positive, got {}", config.port);
        ensure!(!config.host.is_empty(), "Host cannot be empty");

        let db = Database::connect(&config.db_url)
            .context("Connecting to test database")?;

        let count = db.count("users")
            .context("Counting users in test DB")?;

        ensure!(count >= 0, "User count cannot be negative");

        Ok(())
    }

    #[test]
    fn test_with_bail() -> Result<()> {
        let data = load_test_data()?;

        if data.is_empty() {
            bail!("Test data is empty — check test_data/ directory");
        }

        // Process...
        Ok(())
    }
}
```

### ensure!, bail! — macros de anyhow

```rust
// ensure! — como assert! pero retorna Err en vez de panic
ensure!(x > 0, "x must be positive, got {}", x);
// Equivale a:
if !(x > 0) {
    return Err(anyhow::anyhow!("x must be positive, got {}", x));
}

// bail! — retorna Err inmediatamente
bail!("something went wrong: {}", reason);
// Equivale a:
return Err(anyhow::anyhow!("something went wrong: {}", reason));

// anyhow! — crea un anyhow::Error
let err = anyhow::anyhow!("custom error with {}", data);
```

### ¿ensure! o assert! en tests?

```
Macro         Falla con        Mensaje                    Continúa?
──────────────────────────────────────────────────────────────────────
assert!       panic            "assertion failed: ..."    No (panic)
ensure!       Err(anyhow)      Custom message             No (return Err)
```

Ambos detienen el test. La diferencia:
- `assert!` produce un **panic** — el error muestra "panicked at..."
- `ensure!` produce un **Err** — el error muestra solo el mensaje, más limpio

En la práctica, `assert!` / `assert_eq!` son más comunes en tests porque la comunidad está acostumbrada y las macros son del lenguaje estándar. `ensure!` es más útil en funciones helper de tests que retornan `Result`.

---

## 12. Comparación con C y Go

### C: no tiene concepto equivalente

En C, los tests no pueden "retornar errores" porque no hay tipo `Result`. Las opciones son:

```c
// C: todo es assert o manual
void test_file(void) {
    FILE *f = fopen("test.txt", "r");
    assert(f != NULL);  // Si falla, abort() → test muere

    char buf[256];
    char *line = fgets(buf, sizeof(buf), f);
    assert(line != NULL);  // Otra vez assert

    int n = atoi(line);
    assert(n > 0);

    fclose(f);  // Cleanup manual (se pierde si assert falla antes)
}
```

Problemas:
- `assert` hace `abort()` — sin cleanup
- No hay forma de propagar errores
- No hay RAII (cleanup manual que se pierde en fallos)
- No hay información contextual en el error

### Go: t.Fatal y error returns

Go tiene un enfoque similar al de Rust para tests, pero con su propio estilo:

```go
// Go: t.Fatal como alternativa a panic
func TestFile(t *testing.T) {
    content, err := os.ReadFile("test.txt")
    if err != nil {
        t.Fatalf("Failed to read file: %v", err)  // Falla con mensaje
    }

    n, err := strconv.Atoi(strings.TrimSpace(string(content)))
    if err != nil {
        t.Fatalf("Failed to parse: %v", err)
    }

    if n <= 0 {
        t.Errorf("Expected positive, got %d", n)  // Error sin detener
    }
}
```

### Tabla comparativa

```
Aspecto                C (assert)       Go (t.Fatal)      Rust (Result + ?)
──────────────────────────────────────────────────────────────────────────────
Propagar error         No               t.Fatal/t.Error   ? operator
Tipo de error          No aplica        error interface    Box<dyn Error>
Cleanup en fallo       No (abort)       defer             RAII (Drop)
Contexto en error      assert expr      t.Fatalf(...)     .context("...")
Seguir después error   No (abort)       t.Error (sí)      No (Err retorna)
                                        t.Fatal (no)
Ergonomía              Baja             Media             Alta
Type safety            No               Parcial           Completa
```

### Lo que Rust hace mejor

```
1. ? elimina el if err != nil de Go y el assert+check de C
2. RAII garantiza cleanup incluso cuando el test falla
3. Los tipos de error son parte del sistema de tipos
4. map_err/context añade contexto sin código extra
5. El compilador verifica que manejas los errores
```

---

## 13. Errores comunes

### Error 1: usar `?` cuando quieres verificar un Err

```rust
#[test]
fn test_parse_fails() -> TestResult {
    let result = "abc".parse::<i32>()?;
    // ¡ERROR! Si el parse falla, ? PROPAGA el error
    // El test FALLA diciendo "invalid digit found"
    // Pero queríamos verificar que el error ocurre
    Ok(())
}

// CORRECTO:
#[test]
fn test_parse_fails() -> TestResult {
    let result = "abc".parse::<i32>();
    assert!(result.is_err());  // Verificar el Err sin propagarlo
    Ok(())
}
```

### Error 2: olvidar `Ok(())` al final

```rust
#[test]
fn test_something() -> TestResult {
    let x: i32 = "42".parse()?;
    assert_eq!(x, 42);
    // ERROR DE COMPILACIÓN: expected Result<(), ...>, found ()
    // Falta Ok(()) al final
}

// CORRECTO:
#[test]
fn test_something() -> TestResult {
    let x: i32 = "42".parse()?;
    assert_eq!(x, 42);
    Ok(())  // ← No olvidar
}
```

### Error 3: combinar `#[should_panic]` con Result return

```rust
#[test]
#[should_panic]
fn test_panic() -> Result<(), String> {
    // ERROR DE COMPILACIÓN:
    // `#[should_panic]` attribute is not allowed on test functions
    // that return `Result<_, _>`
    panic!("boom");
}

// Solución: elegir uno u otro
// Opción A: should_panic sin Result
#[test]
#[should_panic(expected = "boom")]
fn test_panic() {
    panic!("boom");
}

// Opción B: Result sin should_panic (usar catch_unwind)
#[test]
fn test_panic() -> Result<(), Box<dyn std::error::Error>> {
    let result = std::panic::catch_unwind(|| {
        panic!("boom");
    });
    assert!(result.is_err());
    Ok(())
}
```

### Error 4: tipo de error incompatible

```rust
#[test]
fn test_io() -> Result<(), std::io::Error> {
    let content = std::fs::read_to_string("file.txt")?;  // OK: io::Error
    let num: i32 = content.parse()?;  // ERROR: ParseIntError ≠ io::Error
    Ok(())
}

// Solución: usar Box<dyn Error> para aceptar ambos
#[test]
fn test_io() -> Result<(), Box<dyn std::error::Error>> {
    let content = std::fs::read_to_string("file.txt")?;  // io::Error → Box
    let num: i32 = content.parse()?;  // ParseIntError → Box
    Ok(())
}
```

### Error 5: assert_eq! en test con Result y olvidar que assert! hace panic

```rust
#[test]
fn test_values() -> TestResult {
    let x = compute()?;

    // assert_eq! hace PANIC si falla, no retorna Err
    // Esto está BIEN — el test falla por panic, no por Err
    // Ambas formas de fallar son detectadas por el test harness
    assert_eq!(x, 42);

    Ok(())
}
// Esto funciona correctamente. El test puede fallar de dos formas:
// 1. Err de ? → muestra el error
// 2. panic de assert_eq! → muestra left/right
// Ambas son test FAIL
```

### Error 6: Result con tipo de error que no implementa Debug

```rust
struct MyError;  // Sin #[derive(Debug)]

#[test]
fn test_x() -> Result<(), MyError> {
    // ERROR: `MyError` doesn't implement `Debug`
    Ok(())
}

// Solución: derivar Debug
#[derive(Debug)]
struct MyError;
```

---

## 14. Ejemplo completo: cliente de base de datos en memoria

### `src/lib.rs`

```rust
use std::collections::HashMap;
use std::fmt;

// ════════════════════════════════════════
// Tipos de error
// ════════════════════════════════════════

#[derive(Debug, PartialEq)]
pub enum DbError {
    TableNotFound(String),
    TableAlreadyExists(String),
    ColumnNotFound { table: String, column: String },
    DuplicateKey { table: String, key: String },
    InvalidType { expected: String, got: String },
    EmptyTableName,
    EmptyColumnName,
    NoColumns,
}

impl fmt::Display for DbError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DbError::TableNotFound(t) => write!(f, "Table '{}' not found", t),
            DbError::TableAlreadyExists(t) => write!(f, "Table '{}' already exists", t),
            DbError::ColumnNotFound { table, column } =>
                write!(f, "Column '{}' not found in table '{}'", column, table),
            DbError::DuplicateKey { table, key } =>
                write!(f, "Duplicate key '{}' in table '{}'", key, table),
            DbError::InvalidType { expected, got } =>
                write!(f, "Type mismatch: expected {}, got {}", expected, got),
            DbError::EmptyTableName => write!(f, "Table name cannot be empty"),
            DbError::EmptyColumnName => write!(f, "Column name cannot be empty"),
            DbError::NoColumns => write!(f, "Table must have at least one column"),
        }
    }
}

impl std::error::Error for DbError {}

// ════════════════════════════════════════
// Tipos de datos
// ════════════════════════════════════════

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Int(i64),
    Text(String),
    Bool(bool),
    Null,
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::Int(n) => write!(f, "{}", n),
            Value::Text(s) => write!(f, "'{}'", s),
            Value::Bool(b) => write!(f, "{}", b),
            Value::Null => write!(f, "NULL"),
        }
    }
}

pub type Row = HashMap<String, Value>;

// ════════════════════════════════════════
// Base de datos en memoria
// ════════════════════════════════════════

struct Table {
    columns: Vec<String>,
    primary_key: String,
    rows: Vec<Row>,
}

pub struct MemoryDb {
    tables: HashMap<String, Table>,
}

impl MemoryDb {
    pub fn new() -> Self {
        MemoryDb {
            tables: HashMap::new(),
        }
    }

    pub fn create_table(
        &mut self,
        name: &str,
        columns: &[&str],
        primary_key: &str,
    ) -> Result<(), DbError> {
        if name.is_empty() {
            return Err(DbError::EmptyTableName);
        }
        if columns.is_empty() {
            return Err(DbError::NoColumns);
        }
        for col in columns {
            if col.is_empty() {
                return Err(DbError::EmptyColumnName);
            }
        }
        if self.tables.contains_key(name) {
            return Err(DbError::TableAlreadyExists(name.to_string()));
        }
        if !columns.contains(&primary_key) {
            return Err(DbError::ColumnNotFound {
                table: name.to_string(),
                column: primary_key.to_string(),
            });
        }

        self.tables.insert(
            name.to_string(),
            Table {
                columns: columns.iter().map(|s| s.to_string()).collect(),
                primary_key: primary_key.to_string(),
                rows: Vec::new(),
            },
        );

        Ok(())
    }

    pub fn insert(&mut self, table_name: &str, row: Row) -> Result<(), DbError> {
        let table = self.tables.get_mut(table_name).ok_or_else(|| {
            DbError::TableNotFound(table_name.to_string())
        })?;

        // Verificar que todas las columnas existen
        for key in row.keys() {
            if !table.columns.contains(key) {
                return Err(DbError::ColumnNotFound {
                    table: table_name.to_string(),
                    column: key.clone(),
                });
            }
        }

        // Verificar clave primaria única
        if let Some(pk_value) = row.get(&table.primary_key) {
            for existing in &table.rows {
                if let Some(existing_pk) = existing.get(&table.primary_key) {
                    if existing_pk == pk_value {
                        return Err(DbError::DuplicateKey {
                            table: table_name.to_string(),
                            key: format!("{}", pk_value),
                        });
                    }
                }
            }
        }

        table.rows.push(row);
        Ok(())
    }

    pub fn select_all(&self, table_name: &str) -> Result<Vec<&Row>, DbError> {
        let table = self.tables.get(table_name).ok_or_else(|| {
            DbError::TableNotFound(table_name.to_string())
        })?;
        Ok(table.rows.iter().collect())
    }

    pub fn select_where(
        &self,
        table_name: &str,
        column: &str,
        value: &Value,
    ) -> Result<Vec<&Row>, DbError> {
        let table = self.tables.get(table_name).ok_or_else(|| {
            DbError::TableNotFound(table_name.to_string())
        })?;

        if !table.columns.contains(&column.to_string()) {
            return Err(DbError::ColumnNotFound {
                table: table_name.to_string(),
                column: column.to_string(),
            });
        }

        let results: Vec<&Row> = table
            .rows
            .iter()
            .filter(|row| row.get(column) == Some(value))
            .collect();

        Ok(results)
    }

    pub fn count(&self, table_name: &str) -> Result<usize, DbError> {
        let table = self.tables.get(table_name).ok_or_else(|| {
            DbError::TableNotFound(table_name.to_string())
        })?;
        Ok(table.rows.len())
    }

    pub fn delete_where(
        &mut self,
        table_name: &str,
        column: &str,
        value: &Value,
    ) -> Result<usize, DbError> {
        let table = self.tables.get_mut(table_name).ok_or_else(|| {
            DbError::TableNotFound(table_name.to_string())
        })?;

        if !table.columns.contains(&column.to_string()) {
            return Err(DbError::ColumnNotFound {
                table: table_name.to_string(),
                column: column.to_string(),
            });
        }

        let before = table.rows.len();
        table.rows.retain(|row| row.get(column) != Some(value));
        Ok(before - table.rows.len())
    }

    pub fn drop_table(&mut self, name: &str) -> Result<(), DbError> {
        self.tables.remove(name).ok_or_else(|| {
            DbError::TableNotFound(name.to_string())
        })?;
        Ok(())
    }
}

// ════════════════════════════════════════
// TESTS
// ════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    type TestResult = Result<(), Box<dyn std::error::Error>>;

    // ── Helpers ──

    fn make_row(pairs: &[(&str, Value)]) -> Row {
        pairs.iter().map(|(k, v)| (k.to_string(), v.clone())).collect()
    }

    fn setup_users_db() -> Result<MemoryDb, DbError> {
        let mut db = MemoryDb::new();
        db.create_table("users", &["id", "name", "email", "active"], "id")?;
        db.insert("users", make_row(&[
            ("id", Value::Int(1)),
            ("name", Value::Text("Alice".into())),
            ("email", Value::Text("alice@test.com".into())),
            ("active", Value::Bool(true)),
        ]))?;
        db.insert("users", make_row(&[
            ("id", Value::Int(2)),
            ("name", Value::Text("Bob".into())),
            ("email", Value::Text("bob@test.com".into())),
            ("active", Value::Bool(false)),
        ]))?;
        Ok(db)
    }

    // ══════════════════════════════════════
    // Tests con Result + ? para setup
    // ══════════════════════════════════════

    #[test]
    fn test_create_and_insert() -> TestResult {
        let mut db = MemoryDb::new();
        db.create_table("items", &["id", "name", "price"], "id")?;

        db.insert("items", make_row(&[
            ("id", Value::Int(1)),
            ("name", Value::Text("Widget".into())),
            ("price", Value::Int(999)),
        ]))?;

        let rows = db.select_all("items")?;
        assert_eq!(rows.len(), 1);
        assert_eq!(rows[0].get("name"), Some(&Value::Text("Widget".into())));
        Ok(())
    }

    #[test]
    fn test_select_where() -> TestResult {
        let db = setup_users_db()?;

        let active = db.select_where("users", "active", &Value::Bool(true))?;
        assert_eq!(active.len(), 1);
        assert_eq!(
            active[0].get("name"),
            Some(&Value::Text("Alice".into()))
        );

        let inactive = db.select_where("users", "active", &Value::Bool(false))?;
        assert_eq!(inactive.len(), 1);
        assert_eq!(
            inactive[0].get("name"),
            Some(&Value::Text("Bob".into()))
        );

        Ok(())
    }

    #[test]
    fn test_count() -> TestResult {
        let db = setup_users_db()?;
        assert_eq!(db.count("users")?, 2);
        Ok(())
    }

    #[test]
    fn test_delete_where() -> TestResult {
        let mut db = setup_users_db()?;

        let deleted = db.delete_where("users", "active", &Value::Bool(false))?;
        assert_eq!(deleted, 1);
        assert_eq!(db.count("users")?, 1);

        let remaining = db.select_all("users")?;
        assert_eq!(
            remaining[0].get("name"),
            Some(&Value::Text("Alice".into()))
        );

        Ok(())
    }

    #[test]
    fn test_drop_table() -> TestResult {
        let mut db = setup_users_db()?;
        db.drop_table("users")?;

        let result = db.select_all("users");
        assert!(matches!(result, Err(DbError::TableNotFound(_))));
        Ok(())
    }

    #[test]
    fn test_multiple_tables() -> TestResult {
        let mut db = MemoryDb::new();
        db.create_table("users", &["id", "name"], "id")?;
        db.create_table("posts", &["id", "title", "author_id"], "id")?;

        db.insert("users", make_row(&[
            ("id", Value::Int(1)),
            ("name", Value::Text("Alice".into())),
        ]))?;

        db.insert("posts", make_row(&[
            ("id", Value::Int(100)),
            ("title", Value::Text("Hello World".into())),
            ("author_id", Value::Int(1)),
        ]))?;

        assert_eq!(db.count("users")?, 1);
        assert_eq!(db.count("posts")?, 1);

        let posts = db.select_where("posts", "author_id", &Value::Int(1))?;
        assert_eq!(posts.len(), 1);

        Ok(())
    }

    // ══════════════════════════════════════
    // Tests de errores específicos
    // ══════════════════════════════════════

    #[test]
    fn test_table_not_found() {
        let db = MemoryDb::new();
        let result = db.select_all("nonexistent");
        assert_eq!(result, Err(DbError::TableNotFound("nonexistent".into())));
    }

    #[test]
    fn test_duplicate_table() -> TestResult {
        let mut db = MemoryDb::new();
        db.create_table("users", &["id"], "id")?;

        let result = db.create_table("users", &["id"], "id");
        assert_eq!(result, Err(DbError::TableAlreadyExists("users".into())));
        Ok(())
    }

    #[test]
    fn test_duplicate_key() -> TestResult {
        let mut db = MemoryDb::new();
        db.create_table("t", &["id", "val"], "id")?;
        db.insert("t", make_row(&[("id", Value::Int(1)), ("val", Value::Int(10))]))?;

        let result = db.insert("t", make_row(&[
            ("id", Value::Int(1)),
            ("val", Value::Int(20)),
        ]));

        assert!(matches!(
            result,
            Err(DbError::DuplicateKey { ref table, .. }) if table == "t"
        ));
        Ok(())
    }

    #[test]
    fn test_column_not_found_in_insert() -> TestResult {
        let mut db = MemoryDb::new();
        db.create_table("t", &["id", "name"], "id")?;

        let result = db.insert("t", make_row(&[
            ("id", Value::Int(1)),
            ("nonexistent", Value::Text("x".into())),
        ]));

        assert!(matches!(
            result,
            Err(DbError::ColumnNotFound { ref column, .. }) if column == "nonexistent"
        ));
        Ok(())
    }

    #[test]
    fn test_column_not_found_in_select() -> TestResult {
        let db = setup_users_db()?;
        let result = db.select_where("users", "nonexistent", &Value::Int(1));

        assert!(matches!(
            result,
            Err(DbError::ColumnNotFound { ref column, .. }) if column == "nonexistent"
        ));
        Ok(())
    }

    #[test]
    fn test_empty_table_name() {
        let mut db = MemoryDb::new();
        assert_eq!(
            db.create_table("", &["id"], "id"),
            Err(DbError::EmptyTableName)
        );
    }

    #[test]
    fn test_no_columns() {
        let mut db = MemoryDb::new();
        assert_eq!(
            db.create_table("t", &[], "id"),
            Err(DbError::NoColumns)
        );
    }

    #[test]
    fn test_empty_column_name() {
        let mut db = MemoryDb::new();
        assert_eq!(
            db.create_table("t", &["id", ""], "id"),
            Err(DbError::EmptyColumnName)
        );
    }

    #[test]
    fn test_pk_not_in_columns() {
        let mut db = MemoryDb::new();
        let result = db.create_table("t", &["id", "name"], "nonexistent");
        assert!(matches!(
            result,
            Err(DbError::ColumnNotFound { ref column, .. }) if column == "nonexistent"
        ));
    }

    #[test]
    fn test_select_where_no_match() -> TestResult {
        let db = setup_users_db()?;
        let results = db.select_where("users", "name", &Value::Text("Charlie".into()))?;
        assert!(results.is_empty());
        Ok(())
    }

    #[test]
    fn test_delete_no_match() -> TestResult {
        let mut db = setup_users_db()?;
        let deleted = db.delete_where("users", "name", &Value::Text("Nobody".into()))?;
        assert_eq!(deleted, 0);
        assert_eq!(db.count("users")?, 2);
        Ok(())
    }

    // ══════════════════════════════════════
    // Data-driven test
    // ══════════════════════════════════════

    #[test]
    fn test_value_display() {
        let cases = vec![
            (Value::Int(42), "42"),
            (Value::Text("hello".into()), "'hello'"),
            (Value::Bool(true), "true"),
            (Value::Bool(false), "false"),
            (Value::Null, "NULL"),
        ];

        for (value, expected) in &cases {
            assert_eq!(
                format!("{}", value),
                *expected,
                "Display for {:?} should be {}", value, expected
            );
        }
    }
}
```

### Ejecución

```bash
$ cargo test
running 20 tests
test tests::test_column_not_found_in_insert ... ok
test tests::test_column_not_found_in_select ... ok
test tests::test_count ... ok
test tests::test_create_and_insert ... ok
test tests::test_delete_no_match ... ok
test tests::test_delete_where ... ok
test tests::test_drop_table ... ok
test tests::test_duplicate_key ... ok
test tests::test_duplicate_table ... ok
test tests::test_empty_column_name ... ok
test tests::test_empty_table_name ... ok
test tests::test_multiple_tables ... ok
test tests::test_no_columns ... ok
test tests::test_pk_not_in_columns ... ok
test tests::test_select_where ... ok
test tests::test_select_where_no_match ... ok
test tests::test_table_not_found ... ok
test tests::test_value_display ... ok

test result: ok. 20 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

Nota: ningún test usa `unwrap()`. Los que pueden fallar en setup usan `?`. Los que verifican errores usan `assert_eq!` y `matches!` directamente sobre el `Result`.

---

## 15. Programa de práctica

### Enunciado

Implementa un parser de configuración simple con el siguiente formato:

```
# Comment
key = value
section.key = value
```

La API:

```rust
#[derive(Debug, PartialEq)]
pub enum ConfigError {
    EmptyInput,
    InvalidLine { line_number: usize, content: String },
    DuplicateKey { key: String },
    EmptyKey { line_number: usize },
    EmptyValue { line_number: usize, key: String },
}

pub struct Config { /* ... */ }

impl Config {
    /// Parsear un string de configuración
    pub fn parse(input: &str) -> Result<Self, ConfigError>;

    /// Obtener un valor por clave
    pub fn get(&self, key: &str) -> Option<&str>;

    /// Obtener con valor por defecto
    pub fn get_or(&self, key: &str, default: &str) -> String;

    /// Obtener como entero
    pub fn get_int(&self, key: &str) -> Result<i64, ConfigError>;

    /// Obtener como booleano
    pub fn get_bool(&self, key: &str) -> Result<bool, ConfigError>;

    /// Todas las claves
    pub fn keys(&self) -> Vec<&str>;

    /// Número de entradas
    pub fn len(&self) -> usize;
}
```

### Tareas

1. Implementa `Config` con todos los métodos
2. Implementa `Display` y `std::error::Error` para `ConfigError`
3. Escribe tests usando `-> TestResult` con `?` para:
   - Parseo exitoso con múltiples claves
   - `get()` de clave existente e inexistente
   - `get_int()` con valor válido e inválido
   - `get_bool()` con "true", "false", "1", "0", y valor inválido
   - Comentarios y líneas vacías se ignoran
   - Claves con secciones (`section.key`)
4. Escribe tests de errores usando `assert_eq!` y `matches!`:
   - `EmptyInput` para string vacío
   - `InvalidLine` para línea sin `=`
   - `DuplicateKey` para clave repetida
   - `EmptyKey` para `= value`
   - `EmptyValue` para `key =`
5. No uses `unwrap()` en ningún test — solo `?` y `assert`
6. Usa el type alias `TestResult`

---

## 16. Ejercicios

### Ejercicio 1: de unwrap a Result (15 min)

Refactoriza el siguiente test eliminando TODOS los `unwrap()`:

```rust
#[test]
fn test_process_file() {
    let dir = tempfile::tempdir().unwrap();
    let input = dir.path().join("input.txt");
    let output = dir.path().join("output.txt");

    std::fs::write(&input, "hello\nworld\n").unwrap();

    let content = std::fs::read_to_string(&input).unwrap();
    let upper = content.to_uppercase();
    std::fs::write(&output, &upper).unwrap();

    let result = std::fs::read_to_string(&output).unwrap();
    assert_eq!(result, "HELLO\nWORLD\n");
}
```

**Tareas**:
- a) Reescribe usando `-> Result<(), Box<dyn Error>>` con `?`
- b) Añade `.context()` o `.map_err()` a cada operación para dar contexto
- c) Provoca un fallo (path inexistente) y compara el mensaje de error con unwrap vs con context

### Ejercicio 2: verificar errores con matches! (15 min)

Dado:

```rust
#[derive(Debug, PartialEq)]
pub enum ValidationError {
    TooShort { min: usize, actual: usize },
    TooLong { max: usize, actual: usize },
    InvalidChar { ch: char, position: usize },
    Reserved(String),
}

pub fn validate_username(name: &str) -> Result<(), ValidationError>;
```

**Tareas**:
- a) Implementa `validate_username`:
  - Mínimo 3 caracteres, máximo 20
  - Solo alfanuméricos y guiones bajos
  - No puede ser "admin", "root", "system"
- b) Escribe tests que verifican cada tipo de error usando `matches!` con guards:
  ```rust
  assert!(matches!(result, Err(ValidationError::TooShort { min: 3, actual: 1 })));
  ```
- c) Escribe tests del happy path usando `-> TestResult` con `?`
- d) ¿Cuál es la ventaja de `matches!` sobre `assert_eq!` cuando el error tiene campos que no te importan?

### Ejercicio 3: anyhow en tests (20 min)

**Tareas**:
- a) Añade `anyhow = "1"` a `[dev-dependencies]`
- b) Reescribe 5 tests de los ejercicios anteriores usando `anyhow::Result<()>` en vez de `Box<dyn Error>`
- c) Usa `.context()` de anyhow en al menos 3 operaciones falibles
- d) Usa `ensure!` en vez de `assert!` en al menos 2 verificaciones
- e) Usa `bail!` en al menos 1 caso
- f) Compara la legibilidad y los mensajes de error entre `Box<dyn Error>` y `anyhow`

### Ejercicio 4: diseñar API Result-first (25 min)

Diseña e implementa una calculadora de expresiones RPN (Reverse Polish Notation) que use **exclusivamente** `Result` para errores (ningún panic):

```rust
pub enum RpnError {
    EmptyExpression,
    StackUnderflow { operation: String },
    DivisionByZero,
    InvalidToken(String),
    TooManyValues { remaining: usize },
}

pub fn eval_rpn(expr: &str) -> Result<f64, RpnError>;
// Ejemplo: "3 4 + 2 *" → Ok(14.0)
```

**Tareas**:
- a) Implementa `eval_rpn` y `Display` para `RpnError`
- b) Escribe 5+ tests del happy path con `-> TestResult` usando `?`
- c) Escribe 5+ tests de error verificando cada variante de `RpnError`
- d) Escribe un data-driven test con al menos 10 expresiones y sus resultados esperados
- e) Ningún test debe usar `unwrap()` ni `#[should_panic]`

---

**Navegación**:
- ← Anterior: [T02 — Tests que deben fallar](../T02_Tests_que_deben_fallar/README.md)
- → Siguiente: [T04 — Organización](../T04_Organizacion/README.md)
- ↑ Sección: [S01 — Testing Built-in](../README.md)
