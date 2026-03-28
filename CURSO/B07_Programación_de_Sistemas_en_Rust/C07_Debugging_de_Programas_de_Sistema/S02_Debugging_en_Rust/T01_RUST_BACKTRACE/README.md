# RUST_BACKTRACE: interpretar backtraces en Rust

## Índice

1. [¿Qué es un backtrace?](#qué-es-un-backtrace)
2. [La variable RUST_BACKTRACE](#la-variable-rust_backtrace)
3. [Niveles: 0, 1 y full](#niveles-0-1-y-full)
4. [Interpretar un backtrace](#interpretar-un-backtrace)
5. [Backtraces en diferentes contextos](#backtraces-en-diferentes-contextos)
6. [std::backtrace::Backtrace programático](#stdbacktracebacktrace-programático)
7. [Mejorar la legibilidad de backtraces](#mejorar-la-legibilidad-de-backtraces)
8. [Backtraces y bibliotecas de error](#backtraces-y-bibliotecas-de-error)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es un backtrace?

Un backtrace (stack trace) muestra la **cadena de llamadas a funciones** que llevaron al punto actual de ejecución. Cuando un programa Rust paniquea, el backtrace te dice exactamente qué función llamó a qué otra función hasta llegar al panic.

```
┌──────────────────────────────────────────────────────┐
│              Anatomía de un backtrace                │
├──────────────────────────────────────────────────────┤
│                                                      │
│  main()                    ← punto de entrada        │
│    └─ process_data()       ← tu código               │
│         └─ parse_input()   ← tu código               │
│              └─ validate() ← tu código               │
│                   └─ panic!("invalid input")         │
│                            ↑ AQUÍ ocurrió el error   │
│                                                      │
│  El backtrace se lee de ABAJO hacia ARRIBA:          │
│  - Frame 0: donde ocurrió el panic                   │
│  - Último frame: main o el runtime                   │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## La variable RUST_BACKTRACE

Por defecto, cuando un programa Rust paniquea, solo muestra el mensaje de error:

```bash
$ cargo run
thread 'main' panicked at 'index out of bounds: the len is 3 but the index is 10', src/main.rs:5:15
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
```

La variable de entorno `RUST_BACKTRACE` controla si se muestra el backtrace y cuánto detalle incluye.

### Formas de establecerla

```bash
# Solo para este comando
RUST_BACKTRACE=1 cargo run

# Para toda la sesión del shell
export RUST_BACKTRACE=1
cargo run
cargo test

# En un script
RUST_BACKTRACE=full ./target/debug/my_program

# En .bashrc/.zshrc (permanente)
echo 'export RUST_BACKTRACE=1' >> ~/.bashrc
```

---

## Niveles: 0, 1 y full

### RUST_BACKTRACE=0 (default)

Solo el mensaje de panic, sin backtrace:

```
thread 'main' panicked at 'index out of bounds: the len is 3 but the index is 10', src/main.rs:5:15
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
```

### RUST_BACKTRACE=1

Backtrace **filtrado** — omite frames internos del runtime de Rust y muestra solo los relevantes:

```
thread 'main' panicked at 'index out of bounds: the len is 3 but the index is 10', src/main.rs:5:15
stack backtrace:
   0: rust_begin_unwind
   1: core::panicking::panic_fmt
   2: core::panicking::panic_bounds_check
   3: my_project::validate
             at ./src/main.rs:5:15
   4: my_project::parse_input
             at ./src/main.rs:12:5
   5: my_project::process_data
             at ./src/main.rs:18:9
   6: my_project::main
             at ./src/main.rs:23:5
   7: core::ops::function::FnOnce::call_once
note: Some details are omitted, run with `RUST_BACKTRACE=full` for a verbose backtrace.
```

### RUST_BACKTRACE=full

Backtrace **completo** — incluye todos los frames internos del runtime, linker, y maquinaria de panics:

```
thread 'main' panicked at 'index out of bounds', src/main.rs:5:15
stack backtrace:
   0:     0x5555555d4f30 - std::backtrace_rs::backtrace::libunwind::trace
   1:     0x5555555d4f30 - std::backtrace_rs::backtrace::trace_unsynchronized
   2:     0x5555555d4f30 - std::sys_common::backtrace::_print_fmt
   3:     0x5555555d4f30 - <std::sys_common::backtrace::_print::DisplayBacktrace as core::fmt::Display>::fmt
   4:     0x5555555d8f4c - core::fmt::write
   5:     0x5555555d2e15 - std::io::Write::write_fmt
   6:     0x5555555d7423 - std::sys_common::backtrace::_print
   7:     0x5555555d7423 - std::panicking::default_hook::{{closure}}
   8:     0x5555555d7120 - std::panicking::default_hook
   9:     0x5555555d7b90 - std::panicking::rust_panic_with_hook
  10:     0x5555555d7690 - std::panicking::begin_panic_handler::{{closure}}
  11:     0x5555555d5499 - std::sys_common::backtrace::__rust_end_short_backtrace
  12:     0x5555555d7360 - rust_begin_unwind
  13:     0x5555555e1233 - core::panicking::panic_fmt
  14:     0x5555555e12c3 - core::panicking::panic_bounds_check
  15:     0x555555559234 - my_project::validate
                               at /home/user/my_project/src/main.rs:5:15
  16:     0x555555559300 - my_project::parse_input
                               at /home/user/my_project/src/main.rs:12:5
  17:     0x555555559400 - my_project::process_data
                               at /home/user/my_project/src/main.rs:18:9
  18:     0x555555559500 - my_project::main
                               at /home/user/my_project/src/main.rs:23:5
  19:     0x555555559100 - core::ops::function::FnOnce::call_once
  20:     0x555555559050 - std::sys_common::backtrace::__rust_begin_short_backtrace
  21:     0x555555558f90 - std::rt::lang_start::{{closure}}
  22:     0x5555555d0e50 - std::rt::lang_start_internal
  23:     0x555555559600 - main
  24:     0x7ffff7c29d90 - __libc_start_call_main
  25:     0x7ffff7c29e40 - __libc_start_main_impl
  26:     0x555555558e45 - _start
```

### ¿Cuándo usar cada nivel?

```
┌──────────────────┬──────────────────────────────────────┐
│ Nivel            │ Cuándo usarlo                        │
├──────────────────┼──────────────────────────────────────┤
│ 0 (default)      │ Producción, logs limpios             │
│                  │                                      │
│ 1 (recomendado)  │ Desarrollo diario, debugging         │
│                  │ Muestra TU código sin ruido           │
│                  │                                      │
│ full             │ Bugs en el runtime, problemas de      │
│                  │ linking, FFI, interacción con C       │
│                  │ Muestra TODO, incluido el runtime     │
└──────────────────┴──────────────────────────────────────┘
```

---

## Interpretar un backtrace

### Estructura de cada frame

```
   3: my_project::validators::validate_input
             at ./src/validators.rs:42:15
   │  └────────────────────────────────────┘
   │            │                    │  │
   │     función completa      archivo  │
   │  (crate::módulo::fn)            línea:columna
   │
   número de frame
```

### Leer de abajo hacia arriba

```
stack backtrace:
   0: rust_begin_unwind                    ← maquinaria de panic (ignorar)
   1: core::panicking::panic_fmt           ← maquinaria de panic (ignorar)
   2: core::panicking::panic_bounds_check  ← tipo de panic: bounds check
   3: my_project::process::validate        ← TU función que paniquea ⭐
             at ./src/process.rs:15:20
   4: my_project::process::run             ← quien llamó a validate
             at ./src/process.rs:42:9
   5: my_project::main                     ← main
             at ./src/main.rs:8:5
```

**Regla**: los frames con nombre de tu crate (`my_project::...`) son los que te importan. Los frames con `core::`, `std::`, `alloc::` son del runtime — generalmente puedes ignorarlos.

### Frames con closures y iteradores

```
   5: my_project::main::{{closure}}
             at ./src/main.rs:10:25

# {{closure}} indica un closure anónimo.
# La línea 10 es donde se define el closure.
```

```
   4: core::iter::adapters::map::map_fold::{{closure}}
   5: <core::iter::adapters::Map<I,F> as core::iter::traits::iterator::Iterator>::fold
   6: my_project::process_items
             at ./src/main.rs:15:5

# Los frames 4-5 son la maquinaria de Iterator.
# Frame 6 es donde TÚ llamaste .map().fold() o similar.
```

### Frames con genéricos

```
   3: <alloc::vec::Vec<T> as core::ops::index::Index<usize>>::index
             at /rustc/.../library/alloc/src/vec/mod.rs:2591:9

# Esto es vec[index] — el operador [] de Vec.
# T es genérico; Rust muestra la implementación.
```

### Frames sin información de línea

```
   8: my_project::hot_function
# Sin "at ./src/..." → compilado sin debug info o en release sin debug=true

# Solución: compilar con debug info
# [profile.release]
# debug = true
```

---

## Backtraces en diferentes contextos

### En tests

```bash
$ RUST_BACKTRACE=1 cargo test

running 3 tests
test test_valid ... ok
test test_edge ... ok
test test_invalid ... FAILED

failures:

---- test_invalid stdout ----
thread 'test_invalid' panicked at 'assertion failed: result.is_ok()', src/lib.rs:42:9
stack backtrace:
   0: rust_begin_unwind
   1: core::panicking::panic_fmt
   2: my_lib::tests::test_invalid
             at ./src/lib.rs:42:9
   3: my_lib::tests::test_invalid::{{closure}}
             at ./src/lib.rs:38:5
```

### En threads

```
thread 'worker-3' panicked at 'called `Option::unwrap()` on a `None` value', src/worker.rs:25:30
stack backtrace:
   0: rust_begin_unwind
   1: core::panicking::panic_fmt
   2: core::panicking::panic_nounwind_fmt
   3: core::option::unwrap_failed
   4: my_project::worker::process_task      ← la función del worker
             at ./src/worker.rs:25:30
   5: my_project::worker::run               ← el loop del worker
             at ./src/worker.rs:15:13
   6: std::thread::Builder::spawn_unchecked_::{{closure}}
```

El nombre del thread aparece en la primera línea: `thread 'worker-3'`. Útil si tienes múltiples threads.

### En async (tokio)

Los backtraces de código async son más complejos — incluyen frames del runtime:

```
thread 'tokio-runtime-worker' panicked at 'connection failed', src/client.rs:30:10
stack backtrace:
   0: rust_begin_unwind
   1: core::panicking::panic_fmt
   2: my_project::client::connect::{{closure}}    ← tu async fn
             at ./src/client.rs:30:10
   3: <core::future::from_generator::GenFuture<T> as core::future::future::Future>::poll
   4: tokio::runtime::task::core::Core<T,S>::poll
   5: tokio::runtime::task::harness::Harness<T,S>::poll
   ...muchos frames de tokio...
```

> **Tip**: para backtraces más útiles en async, considera el crate `tokio-console` o la variable `TOKIO_UNSTABLE_ENABLE_TRACING`.

### Con panic = "abort"

```toml
# Cargo.toml
[profile.dev]
panic = "abort"
```

Con `panic = "abort"`, el backtrace se muestra **antes** de abortar (si `RUST_BACKTRACE` está habilitado). No hay unwind — el proceso termina con SIGABRT después de imprimir el backtrace.

---

## std::backtrace::Backtrace programático

Desde Rust 1.65, puedes capturar backtraces programáticamente:

### Capturar un backtrace

```rust
use std::backtrace::Backtrace;

fn inner_function() {
    let bt = Backtrace::capture();
    println!("Called from:\n{}", bt);
}

fn middle_function() {
    inner_function();
}

fn main() {
    // RUST_BACKTRACE debe estar habilitado para que capture() funcione
    middle_function();
}
```

```bash
$ RUST_BACKTRACE=1 cargo run
Called from:
   0: my_project::inner_function
             at ./src/main.rs:4:14
   1: my_project::middle_function
             at ./src/main.rs:9:5
   2: my_project::main
             at ./src/main.rs:13:5
```

### Backtrace en errores custom

```rust
use std::backtrace::Backtrace;
use std::fmt;

#[derive(Debug)]
struct AppError {
    message: String,
    backtrace: Backtrace,
}

impl AppError {
    fn new(message: &str) -> Self {
        AppError {
            message: message.to_string(),
            backtrace: Backtrace::capture(),
        }
    }
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}\n\nBacktrace:\n{}", self.message, self.backtrace)
    }
}

fn validate(input: &str) -> Result<(), AppError> {
    if input.is_empty() {
        return Err(AppError::new("input cannot be empty"));
    }
    Ok(())
}

fn process(input: &str) -> Result<(), AppError> {
    validate(input)?;
    Ok(())
}

fn main() {
    if let Err(e) = process("") {
        eprintln!("Error: {}", e);
    }
}
```

```bash
$ RUST_BACKTRACE=1 cargo run
Error: input cannot be empty

Backtrace:
   0: my_project::AppError::new
             at ./src/main.rs:11:24
   1: my_project::validate
             at ./src/main.rs:23:20
   2: my_project::process
             at ./src/main.rs:29:5
   3: my_project::main
             at ./src/main.rs:33:22
```

### Backtrace::force_capture

```rust
use std::backtrace::Backtrace;

fn always_capture() {
    // capture() respeta RUST_BACKTRACE (puede no capturar si no está set)
    let bt1 = Backtrace::capture();

    // force_capture() SIEMPRE captura, independientemente de RUST_BACKTRACE
    let bt2 = Backtrace::force_capture();

    println!("bt1 status: {:?}", bt1.status()); // puede ser Disabled
    println!("bt2 status: {:?}", bt2.status()); // siempre Captured
}
```

```
┌──────────────────────────────────────────────────────┐
│  Backtrace::capture() vs force_capture()             │
├──────────────────────────────────────────────────────┤
│                                                      │
│  capture():                                          │
│  - Respeta RUST_BACKTRACE                            │
│  - Si RUST_BACKTRACE=0 o no set → Backtrace vacío   │
│  - Cero overhead si no está habilitado               │
│  - Usar en código de producción                      │
│                                                      │
│  force_capture():                                    │
│  - Ignora RUST_BACKTRACE                             │
│  - Siempre captura (tiene coste)                     │
│  - Usar en debugging o logging de errores críticos   │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Mejorar la legibilidad de backtraces

### color-backtrace (crate)

```toml
# Cargo.toml
[dependencies]
color-backtrace = "0.6"
```

```rust
fn main() {
    color_backtrace::install();

    // Ahora los backtraces se muestran con colores:
    // - Tu código en verde/brillante
    // - Runtime de Rust en gris/tenue
    // - Líneas de código fuente inline
}
```

### better-panic (crate)

```toml
[dependencies]
better-panic = "0.3"
```

```rust
fn main() {
    better_panic::install();

    // Muestra:
    // - Código fuente alrededor de la línea del panic
    // - Variables locales (si hay debug info)
    // - Colores y formato mejorado
}
```

### RUST_LIB_BACKTRACE

```bash
# Controla backtraces capturados por std::backtrace::Backtrace
# (separado de RUST_BACKTRACE que controla panics)
RUST_LIB_BACKTRACE=1 cargo run

# Puede ser diferente del nivel de panic:
RUST_BACKTRACE=0 RUST_LIB_BACKTRACE=1 cargo run
# No muestra backtrace en panic, pero Backtrace::capture() sí funciona
```

### Filtrar frames irrelevantes manualmente

```rust
use std::backtrace::Backtrace;

fn format_relevant_backtrace(bt: &Backtrace) -> String {
    let full = format!("{}", bt);

    // Filtrar frames que no son de tu crate
    full.lines()
        .filter(|line| {
            line.contains("my_project::")
            || line.starts_with("   ")  // líneas de "at ./src/..."
            || line.trim().is_empty()
        })
        .collect::<Vec<_>>()
        .join("\n")
}
```

---

## Backtraces y bibliotecas de error

### anyhow: backtraces automáticos

```toml
[dependencies]
anyhow = "1"
```

```rust
use anyhow::{Context, Result};

fn read_config(path: &str) -> Result<String> {
    std::fs::read_to_string(path)
        .with_context(|| format!("failed to read config from {}", path))
}

fn init() -> Result<()> {
    let config = read_config("/etc/myapp/config.toml")?;
    println!("config: {}", config);
    Ok(())
}

fn main() {
    if let Err(e) = init() {
        // anyhow captura backtrace automáticamente si RUST_BACKTRACE=1
        eprintln!("Error: {:?}", e);
        // Muestra: error + contexto + backtrace
    }
}
```

```bash
$ RUST_BACKTRACE=1 cargo run
Error: failed to read config from /etc/myapp/config.toml

Caused by:
    No such file or directory (os error 2)

Stack backtrace:
   0: my_project::read_config
             at ./src/main.rs:4:5
   1: my_project::init
             at ./src/main.rs:9:18
   2: my_project::main
             at ./src/main.rs:14:22
```

### eyre: personalización de reportes

```toml
[dependencies]
eyre = "0.6"
color-eyre = "0.6"
```

```rust
use color_eyre::eyre::{self, WrapErr, Result};

fn main() -> Result<()> {
    color_eyre::install()?;

    let content = std::fs::read_to_string("missing.txt")
        .wrap_err("failed to load data file")?;

    println!("{}", content);
    Ok(())
}
```

`color-eyre` produce reportes de error con colores, secciones de contexto, y backtraces con código fuente.

---

## Errores comunes

### 1. Olvidar establecer RUST_BACKTRACE

```bash
# El programa paniquea sin backtrace
$ cargo run
thread 'main' panicked at 'error', src/main.rs:5:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

# Solución: establecerlo permanentemente para desarrollo
$ echo 'export RUST_BACKTRACE=1' >> ~/.bashrc
$ source ~/.bashrc
```

### 2. Backtraces vacíos en release sin debug info

```bash
$ cargo run --release
thread 'main' panicked at 'error', src/main.rs:5:5
stack backtrace:
   0: 0x555555570f30 - <unknown>
   1: 0x555555571234 - <unknown>
   2: 0x555555559200 - <unknown>
# Sin nombres de función ni líneas

# Solución: añadir debug info a release
# Cargo.toml
# [profile.release]
# debug = true
```

### 3. Confundir el frame del panic con la causa

```
   0: rust_begin_unwind
   1: core::panicking::panic_fmt
   2: core::option::unwrap_failed           ← esto NO es tu código
   3: my_project::get_user                  ← ESTO es tu código ⭐
             at ./src/main.rs:15:30

# Frame 2 dice que fue unwrap() en un None.
# Frame 3 dice DÓNDE: get_user en la línea 15.
# La solución está en frame 3, no en frame 2.
```

### 4. No leer el mensaje de panic

```
thread 'main' panicked at 'called `Result::unwrap()` on an `Err` value:
    Os { code: 2, kind: NotFound, message: "No such file or directory" }',
    src/main.rs:8:45

# El mensaje YA te dice qué pasó: archivo no encontrado.
# El backtrace te dice DÓNDE.
# Muchas veces el mensaje basta sin backtrace.
```

### 5. Usar RUST_BACKTRACE=full cuando 1 es suficiente

```bash
# full genera MUCHO output — decenas de frames del runtime
# que raramente necesitas

# Usar 1 primero:
RUST_BACKTRACE=1 cargo run
# Si no es suficiente, entonces:
RUST_BACKTRACE=full cargo run
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║           RUST_BACKTRACE — REFERENCIA RÁPIDA             ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  VARIABLE DE ENTORNO                                      ║
║  ───────────────────                                      ║
║  RUST_BACKTRACE=0       sin backtrace (default)           ║
║  RUST_BACKTRACE=1       backtrace filtrado (recomendado)  ║
║  RUST_BACKTRACE=full    backtrace completo (verbose)      ║
║                                                           ║
║  ESTABLECER                                               ║
║  ──────────                                               ║
║  RUST_BACKTRACE=1 cargo run    para un comando            ║
║  export RUST_BACKTRACE=1       para la sesión             ║
║  ~/.bashrc                     permanente                 ║
║                                                           ║
║  INTERPRETAR                                              ║
║  ───────────                                              ║
║  Leer de abajo (main) hacia arriba (panic)                ║
║  Tu código: mi_crate::modulo::funcion                    ║
║  Runtime:   core::, std::, alloc::  (ignorar)            ║
║  Closures:  ::{{closure}}                                ║
║  Línea:     at ./src/archivo.rs:linea:columna            ║
║                                                           ║
║  PROGRAMÁTICO (std::backtrace)                            ║
║  ────────────────────────────                              ║
║  Backtrace::capture()         respeta RUST_BACKTRACE      ║
║  Backtrace::force_capture()   siempre captura             ║
║  bt.status()                  Captured | Disabled         ║
║  format!("{}", bt)            imprimir el backtrace       ║
║                                                           ║
║  CRATES ÚTILES                                            ║
║  ─────────────                                            ║
║  color-backtrace    backtraces con colores                ║
║  better-panic       código fuente inline                  ║
║  anyhow             backtrace automático en errores       ║
║  color-eyre         reportes de error completos           ║
║                                                           ║
║  DEBUG INFO EN RELEASE                                    ║
║  ────────────────────                                     ║
║  [profile.release]                                        ║
║  debug = true        ← necesario para backtraces útiles   ║
║                                                           ║
║  VARIABLES RELACIONADAS                                   ║
║  ──────────────────────                                    ║
║  RUST_BACKTRACE       panics                              ║
║  RUST_LIB_BACKTRACE   Backtrace::capture()               ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Leer y diagnosticar backtraces

Crea este programa con varios panics posibles. Para cada escenario, ejecuta con `RUST_BACKTRACE=1`, lee el backtrace, e identifica la causa raíz:

```rust
// src/main.rs
use std::collections::HashMap;

fn get_user_name(users: &HashMap<u32, String>, id: u32) -> &str {
    &users[&id] // panic si id no existe
}

fn parse_number(input: &str) -> i32 {
    input.parse().unwrap() // panic si no es número
}

fn get_element(data: &[i32], index: usize) -> i32 {
    data[index] // panic si index >= len
}

fn divide(a: i32, b: i32) -> i32 {
    a / b // panic si b == 0 (en debug)
}

fn main() {
    let mut users = HashMap::new();
    users.insert(1, "Alice".to_string());
    users.insert(2, "Bob".to_string());

    let data = vec![10, 20, 30];

    // Descomenta uno a la vez y analiza el backtrace:
    // println!("{}", get_user_name(&users, 99));
    // println!("{}", parse_number("not_a_number"));
    // println!("{}", get_element(&data, 10));
    // println!("{}", divide(10, 0));
}
```

Para cada panic:
1. Identifica el frame exacto donde ocurre en TU código.
2. Lee el mensaje de error — ¿qué te dice sin necesidad de backtrace?
3. Compara `RUST_BACKTRACE=1` con `RUST_BACKTRACE=full` — ¿cuántos frames extra añade `full`?

**Pregunta de reflexión**: ¿Cuáles de estos panics se pueden prevenir con `Option`/`Result` en lugar de `unwrap`/indexación directa? ¿Cómo reescribirías cada función para retornar `Result` en vez de paniquear?

---

### Ejercicio 2: Backtrace programático en errores

Implementa un sistema de logging de errores que capture backtraces automáticamente:

```rust
use std::backtrace::Backtrace;
use std::fmt;

#[derive(Debug)]
enum ErrorKind {
    NotFound,
    InvalidInput,
    PermissionDenied,
}

struct TracedError {
    kind: ErrorKind,
    message: String,
    backtrace: Backtrace,
}

impl TracedError {
    fn new(kind: ErrorKind, message: &str) -> Self {
        todo!()
    }

    fn not_found(message: &str) -> Self {
        todo!()
    }

    fn invalid_input(message: &str) -> Self {
        todo!()
    }
}

impl fmt::Display for TracedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        todo!()
        // Mostrar: [ErrorKind] mensaje
        //          Backtrace: ...
    }
}

impl fmt::Debug for TracedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        todo!()
        // Versión verbose con backtrace completo
    }
}
```

Usa `TracedError` en una cadena de funciones y verifica que el backtrace captura el punto exacto de creación del error.

**Pregunta de reflexión**: ¿Cuál es el coste de capturar un backtrace en cada error? ¿Deberías usar `capture()` o `force_capture()`? ¿Qué pasa en producción si `RUST_BACKTRACE=0`?

---

### Ejercicio 3: Backtraces en async y threads

Compara cómo se ven los backtraces en diferentes contextos de ejecución:

```rust
fn sync_panic() {
    panic!("sync panic");
}

fn thread_panic() {
    std::thread::Builder::new()
        .name("worker-1".to_string())
        .spawn(|| {
            panic!("thread panic");
        })
        .unwrap()
        .join()
        .unwrap(); // este unwrap propaga el panic del thread
}

async fn async_panic() {
    panic!("async panic");
}

fn nested_panic() {
    fn level_1() { level_2(); }
    fn level_2() { level_3(); }
    fn level_3() { panic!("deeply nested"); }
    level_1();
}

fn main() {
    // Ejecuta uno a la vez con RUST_BACKTRACE=1:
    // sync_panic();
    // thread_panic();
    // nested_panic();

    // Para async (necesita tokio):
    // tokio::runtime::Runtime::new().unwrap().block_on(async_panic());
}
```

Para cada caso:
1. ¿Cómo identifica el backtrace qué thread paniquó?
2. ¿Cuántos frames "de ruido" hay en cada contexto?
3. ¿El backtrace de async es más difícil de leer? ¿Por qué?

**Pregunta de reflexión**: cuando un thread paniquea, ¿qué pasa en el thread principal? ¿El backtrace del thread principal muestra información útil sobre dónde paniquó el thread hijo?
