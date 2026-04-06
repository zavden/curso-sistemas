# T04 - cargo test Flags y Opciones

## Índice

1. [El problema: ejecutar los tests correctos en el momento correcto](#1-el-problema-ejecutar-los-tests-correctos-en-el-momento-correcto)
2. [Anatomía del comando cargo test](#2-anatomía-del-comando-cargo-test)
3. [El separador -- : flags de Cargo vs flags del test harness](#3-el-separador-----flags-de-cargo-vs-flags-del-test-harness)
4. [Selección por tipo de test: --lib, --test, --doc, --bins](#4-selección-por-tipo-de-test---lib---test---doc---bins)
5. [Filtrado por nombre de test](#5-filtrado-por-nombre-de-test)
6. [--exact: coincidencia exacta](#6---exact-coincidencia-exacta)
7. [#[ignore] y --ignored / --include-ignored](#7-ignore-y---ignored----include-ignored)
8. [-- --nocapture: ver stdout/stderr](#8------nocapture-ver-stdoutstderr)
9. [--release: tests en modo release](#9---release-tests-en-modo-release)
10. [Paralelismo: --test-threads](#10-paralelismo---test-threads)
11. [--no-fail-fast: ejecutar todo aunque falle](#11---no-fail-fast-ejecutar-todo-aunque-falle)
12. [Flags de compilación: --features, --no-default-features](#12-flags-de-compilación---features---no-default-features)
13. [--workspace y -p: tests en workspaces](#13---workspace-y--p-tests-en-workspaces)
14. [--no-run: compilar sin ejecutar](#14---no-run-compilar-sin-ejecutar)
15. [--list: listar tests sin ejecutar](#15---list-listar-tests-sin-ejecutar)
16. [-- --format: formato del output](#16------format-formato-del-output)
17. [-- --show-output: mostrar output de tests exitosos](#17------show-output-mostrar-output-de-tests-exitosos)
18. [-- -Z unstable-options: opciones experimentales](#18----z-unstable-options-opciones-experimentales)
19. [Variables de entorno relevantes](#19-variables-de-entorno-relevantes)
20. [Workflows prácticos para desarrollo](#20-workflows-prácticos-para-desarrollo)
21. [Comparación con C y Go](#21-comparación-con-c-y-go)
22. [Referencia rápida completa](#22-referencia-rápida-completa)
23. [Errores comunes](#23-errores-comunes)
24. [Programa de práctica](#24-programa-de-práctica)
25. [Ejercicios](#25-ejercicios)

---

## 1. El problema: ejecutar los tests correctos en el momento correcto

Un proyecto Rust típico puede tener cientos de tests distribuidos en tres zonas diferentes. Ejecutar **todos** cada vez es lento e innecesario:

```
Proyecto mediano — distribución de tests:

  src/                        tests/                  doc comments
  ├── lib.rs (8 tests)        ├── api.rs (12 tests)   ├── lib.rs (5 doc tests)
  ├── parser.rs (15 tests)    ├── auth.rs (8 tests)   ├── parser.rs (3 doc tests)
  ├── validator.rs (10 tests) ├── flow.rs (6 tests)   └── config.rs (2 doc tests)
  ├── engine.rs (12 tests)    └── perf.rs (4 tests)
  └── config.rs (5 tests)

  Total: 50 unit + 30 integration + 10 doc = 90 tests
  Tiempo: ~3s unit + ~8s integration + ~5s doc = ~16s

  Mientras escribo código en parser.rs, ¿necesito los 90?
  No. Solo los 15 de parser + quizás los 12 de api.
  Eso toma ~1s en lugar de ~16s.
```

`cargo test` sin argumentos ejecuta todo. Con flags, puedes ser quirúrgico:

```
Escenarios de desarrollo y sus comandos:

  "Estoy editando parser.rs"
  → cargo test --lib parser             Solo unit tests del parser

  "Quiero verificar que la API no se rompió"
  → cargo test --test api               Solo integration tests de API

  "Necesito ver el println! de un test"
  → cargo test test_name -- --nocapture  Con output visible

  "El CI debe ejecutar todo"
  → cargo test                           Todo (unit + integration + doc)

  "Quiero los tests lentos también"
  → cargo test -- --include-ignored      Todo incluyendo #[ignore]

  "Solo quiero compilar, no ejecutar"
  → cargo test --no-run                  Verifica compilación
```

---

## 2. Anatomía del comando cargo test

```
cargo test [OPTIONS] [TESTNAME] [-- TEST_OPTIONS]

  ├──────────┘ ├─────────────┘  ├──────────────┘
  │            │                │
  │            │                └── Flags para el test harness
  │            │                    (el binario que ejecuta los tests)
  │            │
  │            └── Filtro de nombre (substring match)
  │
  └── Flags para Cargo
      (qué compilar, cómo compilar)
```

### Los tres niveles de control

```
Nivel 1: Cargo decide QUÉ compilar
  cargo test --lib              → Solo compila/ejecuta unit tests
  cargo test --test api         → Solo compila/ejecuta tests/api.rs
  cargo test --doc              → Solo compila/ejecuta doc tests
  cargo test --release          → Compila con optimizaciones
  cargo test --features "x"     → Activa feature flags

Nivel 2: TESTNAME filtra por nombre
  cargo test parser             → Tests cuyo nombre contiene "parser"
  cargo test test_add           → Tests cuyo nombre contiene "test_add"

Nivel 3: TEST_OPTIONS controlan la ejecución
  cargo test -- --nocapture     → Muestra stdout/stderr
  cargo test -- --test-threads=1 → Ejecución secuencial
  cargo test -- --ignored       → Solo tests marcados #[ignore]
```

### Parsing del comando

```
Ejemplo completo:

  cargo test --lib --release parser -- --nocapture --test-threads=1

  ├────────────────────────────────┘   ├─────────────────────────────┘
  │                                    │
  │  Flags de Cargo:                   │  Flags del test harness:
  │    --lib     → Solo unit tests     │    --nocapture     → Ver output
  │    --release → Con optimizaciones  │    --test-threads=1 → Secuencial
  │    parser    → Filtro de nombre    │
  │                                    │
  └── TODO antes del --                └── TODO después del --
```

---

## 3. El separador -- : flags de Cargo vs flags del test harness

El `--` es crucial y es la fuente principal de confusión. Separa dos mundos:

```
                    ┌── El separador
                    │
  cargo test --lib  --  --nocapture
  ├───────────────┘     ├──────────┘
  │                     │
  │ ANTES del --        │ DESPUÉS del --
  │ = Flags de CARGO    │ = Flags del BINARIO de test
  │                     │
  │ Controlan:          │ Controlan:
  │  - Qué compilar     │  - Cómo ejecutar
  │  - Qué tipo de test │  - Qué mostrar
  │  - Modo de build    │  - Paralelismo
  │  - Features         │  - Filtrado fino
  └─────────────────────┴──────────────────────
```

### Error típico: poner flags del lado equivocado

```bash
# ✗ INCORRECTO: --nocapture es para el test harness, no para Cargo
$ cargo test --nocapture
# error: unexpected argument '--nocapture' found

# ✓ CORRECTO: --nocapture después del --
$ cargo test -- --nocapture

# ✗ INCORRECTO: --lib es para Cargo, no para el test harness
$ cargo test -- --lib
# error: unexpected argument '--lib'

# ✓ CORRECTO: --lib antes del --
$ cargo test --lib

# ✗ INCORRECTO: mezcla de lados
$ cargo test --nocapture --lib
# error: unexpected argument '--nocapture'

# ✓ CORRECTO: cada flag en su lado
$ cargo test --lib -- --nocapture
```

### Tabla de qué va dónde

| Flag | Lado | Ejemplo |
|------|------|---------|
| `--lib` | Cargo (antes de `--`) | `cargo test --lib` |
| `--test NAME` | Cargo | `cargo test --test api` |
| `--doc` | Cargo | `cargo test --doc` |
| `--release` | Cargo | `cargo test --release` |
| `--features` | Cargo | `cargo test --features "x"` |
| `--no-run` | Cargo | `cargo test --no-run` |
| `--workspace` | Cargo | `cargo test --workspace` |
| `-p NAME` | Cargo | `cargo test -p my_crate` |
| `TESTNAME` | Cargo (filtro) | `cargo test parser` |
| `--nocapture` | Harness (después de `--`) | `cargo test -- --nocapture` |
| `--test-threads` | Harness | `cargo test -- --test-threads=1` |
| `--ignored` | Harness | `cargo test -- --ignored` |
| `--include-ignored` | Harness | `cargo test -- --include-ignored` |
| `--exact` | Harness | `cargo test test_add -- --exact` |
| `--show-output` | Harness | `cargo test -- --show-output` |
| `--format` | Harness | `cargo test -- --format terse` |
| `--list` | Harness | `cargo test -- --list` |

---

## 4. Selección por tipo de test: --lib, --test, --doc, --bins

### --lib: solo unit tests

```bash
# Ejecuta solo los tests en src/ (los que están en mod tests {})
$ cargo test --lib
```

```
$ cargo test --lib

     Running unittests src/lib.rs (target/debug/deps/mi_crate-abc123)

running 50 tests
test parser::tests::test_tokenize ... ok
test parser::tests::test_parse ... ok
test validator::tests::test_range ... ok
...
test result: ok. 50 passed; 0 failed

# NO aparecen integration tests ni doc tests
```

Esto compila `src/` con `cfg(test)` activo y ejecuta todos los `#[test]` en `src/`.

### --test NAME: un archivo de integration test específico

```bash
# Ejecuta solo tests/api.rs
$ cargo test --test api

# Ejecuta solo tests/auth.rs
$ cargo test --test auth

# Ejecuta múltiples archivos específicos
$ cargo test --test api --test auth
```

```
$ cargo test --test api

     Running tests/api.rs (target/debug/deps/api-def456)

running 12 tests
test test_create_resource ... ok
test test_get_resource ... ok
...
test result: ok. 12 passed; 0 failed

# SOLO tests/api.rs — ni unit tests, ni otros integration, ni doc tests
```

### --test '*': todos los integration tests

```bash
# Ejecuta TODOS los archivos en tests/ (pero no unit tests ni doc tests)
$ cargo test --test '*'
```

### --doc: solo doc tests

```bash
$ cargo test --doc
```

```
$ cargo test --doc

   Doc-tests mi_crate

running 10 tests
test src/lib.rs - add (line 5) ... ok
test src/parser.rs - parser::parse (line 12) ... ok
...
test result: ok. 10 passed; 0 failed

# SOLO doc tests — ni unit tests ni integration tests
```

### --bins: tests en binarios

```bash
# Ejecuta tests en archivos binarios (src/main.rs, src/bin/*.rs)
$ cargo test --bins

# Tests de un binario específico
$ cargo test --bin my_tool
```

### --benches: benchmarks

```bash
# Ejecuta benchmarks (requiere nightly o criterion)
$ cargo test --benches
```

### Combinaciones

```bash
# Unit tests + doc tests (sin integration)
$ cargo test --lib --doc

# Solo integration y doc tests (sin unit)
$ cargo test --test '*' --doc

# Un integration test específico + unit tests
$ cargo test --lib --test api
```

### Diagrama de qué incluye cada flag

```
cargo test (sin flags) ejecuta TODO:

  ┌─────────────────────────────────────────┐
  │  ┌──────────┐  ┌──────────┐  ┌───────┐ │
  │  │ --lib    │  │ --test * │  │ --doc │ │
  │  │ Unit     │  │ Integr.  │  │ Doc   │ │
  │  │ tests    │  │ tests    │  │ tests │ │
  │  │ (src/)   │  │ (tests/) │  │ (///) │ │
  │  └──────────┘  └──────────┘  └───────┘ │
  └─────────────────────────────────────────┘

  cargo test --lib:
  ┌──────────────────┐
  │  ┌──────────┐    │
  │  │ Unit     │    │
  │  │ tests    │    │
  │  │ (src/)   │    │
  │  └──────────┘    │
  └──────────────────┘

  cargo test --test api:
  ┌──────────────────┐
  │  ┌──────────┐    │
  │  │ tests/   │    │
  │  │ api.rs   │    │
  │  └──────────┘    │
  └──────────────────┘

  cargo test --doc:
  ┌──────────────────┐
  │  ┌───────┐       │
  │  │ Doc   │       │
  │  │ tests │       │
  │  │ (///) │       │
  │  └───────┘       │
  └──────────────────┘
```

---

## 5. Filtrado por nombre de test

El argumento `TESTNAME` después de `cargo test` filtra tests cuyo nombre **contiene** el substring dado:

### Filtrado básico (substring match)

```bash
# Tests cuyo nombre contiene "parser"
$ cargo test parser

# Esto coincide con:
#   parser::tests::test_tokenize          ← contiene "parser"
#   parser::tests::test_parse_expression  ← contiene "parser"
#   tests/parser_integration.rs → *       ← el ARCHIVO contiene "parser"
#   test_parser_handles_utf8              ← contiene "parser"
```

```
$ cargo test parser

     Running unittests src/lib.rs (target/debug/deps/mi_crate-abc123)

running 15 tests
test parser::tests::test_tokenize ... ok
test parser::tests::test_parse_number ... ok
test parser::tests::test_parse_expression ... ok
...
test result: ok. 15 passed; 0 failed; 35 filtered out
                                       ^^^^^^^^^^^^^^^^^^
                                       35 tests no coincidieron

     Running tests/parser_tests.rs (target/debug/deps/parser_tests-def456)

running 5 tests
test test_parse_simple ... ok
...
test result: ok. 5 passed; 0 failed; 0 filtered out
```

### Filtrado con módulo

```bash
# Solo tests dentro del módulo parser::tests
$ cargo test parser::tests

# Solo test_tokenize dentro de parser::tests
$ cargo test parser::tests::test_tokenize

# Tests que contengan "validate" en cualquier módulo
$ cargo test validate
```

### Filtrado con --test combinado

```bash
# Tests en tests/api.rs cuyo nombre contiene "create"
$ cargo test --test api create

# Tests en tests/auth.rs cuyo nombre contiene "login"
$ cargo test --test auth login

# Unit tests cuyo nombre contiene "parse"
$ cargo test --lib parse
```

### El filtro es substring, no prefix ni regex

```bash
$ cargo test add
# Coincide con:
#   test_add              ← "add" está en el nombre
#   test_add_two_numbers  ← "add" está en el nombre
#   test_address_parsing  ← "add" está en "address" (¡cuidado!)
#   test_padding          ← "add" está en "padding" (¡cuidado!)

# Para evitar falsos positivos, usa un nombre más específico:
$ cargo test test_add_
# O usa --exact para match exacto
```

### Filtrado de doc tests

```bash
# Doc tests del item "parse"
$ cargo test --doc parse

# Doc tests en un archivo específico
$ cargo test --doc "src/parser.rs"

# Doc test en una línea específica
$ cargo test --doc "line 15"
```

---

## 6. --exact: coincidencia exacta

Por defecto el filtro es substring. Con `--exact`, el nombre debe coincidir **exactamente**:

```bash
# Substring: coincide con test_add, test_add_two, test_adding, etc.
$ cargo test test_add

# Exacto: SOLO coincide con "test_add"
$ cargo test test_add -- --exact
```

### Cuándo es útil --exact

```bash
# Tienes estos tests:
#   test_add
#   test_add_two_numbers
#   test_add_negative
#   test_address_parsing

# Sin --exact (ejecuta los 4):
$ cargo test test_add
# running 4 tests

# Con --exact (ejecuta solo 1):
$ cargo test test_add -- --exact
# running 1 test

# Nombre completo con módulo:
$ cargo test parser::tests::test_tokenize -- --exact
# running 1 test
```

### --exact con path completo del test

```bash
# Los unit tests tienen path completo: modulo::tests::nombre
$ cargo test parser::tests::test_tokenize -- --exact

# Los integration tests no tienen mod tests wrapper:
$ cargo test --test api test_create_resource -- --exact

# Los doc tests tienen formato especial:
$ cargo test --doc "src/lib.rs - add (line 5)" -- --exact
```

---

## 7. #[ignore] y --ignored / --include-ignored

### Marcando tests como ignorados

```rust
#[test]
#[ignore]  // Este test no se ejecuta por defecto
fn test_slow_operation() {
    // Test que toma 30 segundos...
    std::thread::sleep(std::time::Duration::from_secs(30));
    assert!(true);
}

#[test]
#[ignore = "requires database server"]  // Con razón documentada
fn test_database_connection() {
    let db = Database::connect("postgres://localhost/test").unwrap();
    assert!(db.is_connected());
}

#[test]
#[ignore = "flaky - investigating #1234"]  // Test intermitente
fn test_race_condition() {
    // ...
}
```

### Comportamiento por defecto

```
$ cargo test

running 10 tests
test test_fast_a ... ok
test test_fast_b ... ok
test test_fast_c ... ok
test test_slow_operation ... ignored       ← No se ejecuta
test test_database_connection ... ignored  ← No se ejecuta
test test_race_condition ... ignored       ← No se ejecuta
...
test result: ok. 7 passed; 0 failed; 3 ignored
                                    ^^^^^^^^^
                                    Contados pero no ejecutados
```

### --ignored: ejecutar SOLO los ignorados

```bash
$ cargo test -- --ignored
```

```
$ cargo test -- --ignored

running 3 tests
test test_slow_operation ... ok
test test_database_connection ... ok
test test_race_condition ... FAILED
test result: FAILED. 2 passed; 1 failed; 0 ignored
```

Solo ejecuta los tests marcados con `#[ignore]`. Los tests normales se omiten.

### --include-ignored: ejecutar TODO

```bash
$ cargo test -- --include-ignored
```

```
$ cargo test -- --include-ignored

running 10 tests
test test_fast_a ... ok
test test_fast_b ... ok
test test_fast_c ... ok
test test_slow_operation ... ok
test test_database_connection ... ok
test test_race_condition ... FAILED
...
test result: FAILED. 9 passed; 1 failed; 0 ignored
```

Ejecuta **todos** los tests: normales + ignorados.

### Combinación con filtrado

```bash
# Solo los tests ignorados del módulo parser
$ cargo test parser -- --ignored

# Solo los tests ignorados en tests/perf.rs
$ cargo test --test perf -- --ignored

# Todo en un workspace incluyendo ignorados
$ cargo test --workspace -- --include-ignored
```

### Patrón: categorizar tests por velocidad

```rust
// Tests rápidos — siempre se ejecutan
#[test]
fn test_parse_simple() {
    assert_eq!(parse("42"), Ok(42));
}

// Tests medios — se ignoran en desarrollo, se ejecutan en CI
#[test]
#[ignore = "slow: processes 100K records"]
fn test_large_dataset() {
    let data = generate_data(100_000);
    let result = process_batch(&data);
    assert_eq!(result.len(), 100_000);
}

// Tests que necesitan infraestructura — solo en CI con servicios
#[test]
#[ignore = "requires: postgres"]
fn test_real_database() {
    let db = Database::connect_real().unwrap();
    // ...
}
```

```bash
# Desarrollo local: rápido
$ cargo test                         # Solo tests rápidos

# CI básico: incluyendo lentos
$ cargo test -- --include-ignored    # Todo

# CI con servicios: incluyendo infra
$ cargo test -- --include-ignored    # Todo (DB disponible en CI)
```

---

## 8. -- --nocapture: ver stdout/stderr

Por defecto, `cargo test` **captura** el stdout/stderr de los tests. Solo lo muestra si el test falla:

### Comportamiento por defecto (captura)

```rust
#[test]
fn test_with_println() {
    println!("DEBUG: starting test");
    let result = process("input");
    println!("DEBUG: result = {:?}", result);
    assert_eq!(result, "expected");
}
```

```
$ cargo test test_with_println

running 1 test
test test_with_println ... ok       ← Sin output de println!

test result: ok. 1 passed; 0 failed
```

Si el test **falla**, Cargo muestra el output capturado:

```
$ cargo test test_with_println

running 1 test
test test_with_println ... FAILED

failures:

---- test_with_println stdout ----    ← Output capturado se muestra
DEBUG: starting test
DEBUG: result = "wrong"

failures:
    test_with_println

test result: FAILED. 0 passed; 1 failed
```

### --nocapture: siempre mostrar output

```bash
$ cargo test -- --nocapture
```

```
$ cargo test test_with_println -- --nocapture

running 1 test
DEBUG: starting test                 ← Se ve inmediatamente
DEBUG: result = "expected"           ← Se ve inmediatamente
test test_with_println ... ok

test result: ok. 1 passed; 0 failed
```

### --show-output: mostrar output al final (solo tests exitosos)

```bash
$ cargo test -- --show-output
```

```
$ cargo test -- --show-output

running 3 tests
test test_a ... ok
test test_b ... ok
test test_c ... ok

successes:

---- test_a stdout ----
Debug info from test_a

---- test_b stdout ----
Debug info from test_b

successes:
    test_a
    test_b

test result: ok. 3 passed; 0 failed
```

A diferencia de `--nocapture`, `--show-output` recopila el output y lo muestra **después** de la ejecución, agrupado por test. Es más legible cuando hay muchos tests con output.

### --nocapture vs --show-output

```
--nocapture:
  + Output en TIEMPO REAL (útil para debugging lento)
  - Output mezclado de tests paralelos (ilegible)
  - El output se intercala con "test ... ok" mensajes

--show-output:
  + Output agrupado por test (legible)
  + Solo muestra output de tests exitosos
  - No es en tiempo real (espera a que termine)
  - No muestra output de tests fallidos (esos siempre se muestran)

Para debugging:
  cargo test nombre_test -- --nocapture   (1 test, tiempo real)

Para inspección general:
  cargo test -- --show-output             (muchos tests, agrupado)
```

### Captura de stderr

`--nocapture` también desactiva la captura de stderr:

```rust
#[test]
fn test_with_stderr() {
    eprintln!("WARNING: this is stderr");
    println!("INFO: this is stdout");
    assert!(true);
}
```

```bash
$ cargo test test_with_stderr -- --nocapture
# WARNING: this is stderr     ← stderr visible
# INFO: this is stdout        ← stdout visible
# test test_with_stderr ... ok
```

---

## 9. --release: tests en modo release

Por defecto, `cargo test` compila en modo debug (sin optimizaciones, con debug info). Con `--release`, compila con optimizaciones:

```bash
# Debug (default): compilación rápida, ejecución lenta
$ cargo test

# Release: compilación lenta, ejecución rápida
$ cargo test --release
```

### Diferencias entre debug y release

```
cargo test (debug):
  ├── Optimizaciones: ninguna (opt-level = 0)
  ├── Debug info: sí (debug = 2)
  ├── overflow checks: sí (activados por defecto)
  ├── debug_assertions: activadas
  ├── Compilación: rápida (~5s)
  ├── Ejecución: lenta
  └── Directorio: target/debug/deps/

cargo test --release:
  ├── Optimizaciones: máximas (opt-level = 3)
  ├── Debug info: no (debug = 0)
  ├── overflow checks: no (desactivados por defecto)
  ├── debug_assertions: desactivadas
  ├── Compilación: lenta (~30s)
  ├── Ejecución: rápida
  └── Directorio: target/release/deps/
```

### Cuándo usar --release

```bash
# 1. Tests de rendimiento (timing)
$ cargo test --release -- --ignored perf

# 2. Verificar que optimizaciones no rompen nada
$ cargo test --release

# 3. Tests que son demasiado lentos en debug
$ cargo test --release -- --include-ignored slow
```

### Cuidado: overflow checks desactivados en release

```rust
#[test]
fn test_overflow() {
    let x: u8 = 255;
    let y = x + 1;  // ¿Qué pasa?
    // En debug: PANIC (overflow check)
    // En release: silenciosamente wraps a 0
    println!("y = {}", y);
}
```

```bash
$ cargo test test_overflow
# thread 'test_overflow' panicked at 'attempt to add with overflow'
# → Test FALLA (en debug, overflow = panic)

$ cargo test --release test_overflow
# y = 0
# → Test PASA (en release, overflow = wrap)
# ¡El comportamiento es DIFERENTE!
```

### debug_assertions en release

```rust
#[test]
fn test_debug_assertions() {
    // debug_assert! solo se compila en debug
    debug_assert!(false, "This only fires in debug");
    assert!(true, "This always fires");
}
```

```bash
$ cargo test test_debug_assertions
# FALLA (debug_assert! se activa en debug)

$ cargo test --release test_debug_assertions
# PASA (debug_assert! se ignora en release)
```

### Perfiles custom para tests

```toml
# Cargo.toml

# Perfil para tests que necesitan algo de optimización
# pero sin modo release completo
[profile.test]
opt-level = 1       # Alguna optimización (default es 0)

# Perfil para benchmarks
[profile.bench]
opt-level = 3
debug = false
```

---

## 10. Paralelismo: --test-threads

Por defecto, `cargo test` ejecuta tests **en paralelo** usando tantos threads como cores disponibles. Cada test es independiente y puede ejecutarse en cualquier orden:

### Ejecución secuencial

```bash
# Ejecutar un test a la vez (secuencial)
$ cargo test -- --test-threads=1
```

### Especificar número de threads

```bash
# 4 threads
$ cargo test -- --test-threads=4

# 1 thread (secuencial)
$ cargo test -- --test-threads=1

# Usar todos los cores (default)
$ cargo test
# Equivalente a: cargo test -- --test-threads=$(nproc)
```

### Cuándo necesitas --test-threads=1

```rust
// Tests que comparten un recurso global:

static FILE: &str = "/tmp/shared_test_file.txt";

#[test]
fn test_write_file() {
    std::fs::write(FILE, "data from write test").unwrap();
    let content = std::fs::read_to_string(FILE).unwrap();
    assert_eq!(content, "data from write test");
}

#[test]
fn test_read_file() {
    std::fs::write(FILE, "data from read test").unwrap();
    let content = std::fs::read_to_string(FILE).unwrap();
    assert_eq!(content, "data from read test");
}

// En paralelo: PUEDE FALLAR (race condition)
//   Thread 1: write("data from write")
//   Thread 2: write("data from read")   ← Sobrescribe
//   Thread 1: read → "data from read"   ← ¡Sorpresa!

// Con --test-threads=1: SIEMPRE PASA (secuencial)
```

### Mejor solución: evitar la necesidad de secuencial

```rust
// En vez de compartir /tmp/shared_file.txt,
// cada test usa su propio archivo:

#[test]
fn test_write_file() {
    let path = format!("/tmp/test_write_{}.txt", std::process::id());
    std::fs::write(&path, "data").unwrap();
    let content = std::fs::read_to_string(&path).unwrap();
    assert_eq!(content, "data");
    std::fs::remove_file(&path).unwrap();
}

#[test]
fn test_read_file() {
    let path = format!("/tmp/test_read_{}.txt", std::process::id());
    std::fs::write(&path, "data").unwrap();
    let content = std::fs::read_to_string(&path).unwrap();
    assert_eq!(content, "data");
    std::fs::remove_file(&path).unwrap();
}

// Ahora funciona en paralelo sin problemas
```

### Variable de entorno alternativa

```bash
# También se puede controlar con variable de entorno
$ RUST_TEST_THREADS=1 cargo test
```

### Paralelismo entre archivos vs entre tests

```
cargo test ejecuta paralelismo en DOS niveles:

  Nivel 1: ENTRE binarios de test (secuencial por defecto)
  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
  │  Unit tests    │ →→→ │  tests/api.rs  │ →→→ │  tests/auth.rs │
  │  (1 binario)   │     │  (1 binario)   │     │  (1 binario)   │
  └───────────────┘     └───────────────┘     └───────────────┘
       Secuencial entre binarios (uno a la vez)

  Nivel 2: DENTRO de cada binario (paralelo por defecto)
  ┌───────────────────────────────────┐
  │  Binario: unit tests              │
  │                                   │
  │  Thread 1: test_parse_a           │
  │  Thread 2: test_parse_b  ← En    │
  │  Thread 3: test_validate ← paralelo │
  │  Thread 4: test_engine            │
  └───────────────────────────────────┘

  --test-threads=1 afecta el nivel 2 (tests dentro de cada binario)
```

---

## 11. --no-fail-fast: ejecutar todo aunque falle

### Comportamiento por defecto: fail-fast

```bash
$ cargo test

     Running unittests src/lib.rs

running 50 tests
test test_a ... ok
test test_b ... FAILED          ← Primer fallo
# ← Cargo DETIENE la ejecución de este binario y pasa al siguiente

     Running tests/api.rs
# ← Pero SÍ ejecuta los otros binarios
```

**Nota**: fail-fast detiene los tests **dentro de un binario** tras el primer fallo, pero Cargo sigue ejecutando los demás binarios.

### --no-fail-fast: ejecutar todo

```bash
$ cargo test --no-fail-fast
```

```
$ cargo test --no-fail-fast

     Running unittests src/lib.rs

running 50 tests
test test_a ... ok
test test_b ... FAILED
test test_c ... ok              ← Sigue ejecutando aunque test_b falló
test test_d ... FAILED
test test_e ... ok
...
test result: FAILED. 45 passed; 5 failed
                                ^^^^^^^^^
                                TODOS los fallos reportados

     Running tests/api.rs
...
```

### Cuándo usar --no-fail-fast

```
Desarrollo local:
  → Sin flag (fail-fast) — arregla un fallo a la vez
  → Más rápido, enfocado

CI/Reporte completo:
  → Con --no-fail-fast — muestra TODOS los problemas
  → Permite arreglar múltiples fallos en un solo ciclo
```

---

## 12. Flags de compilación: --features, --no-default-features

### --features: activar feature flags

```toml
# Cargo.toml
[features]
default = ["json"]
json = ["serde_json"]
xml = ["quick-xml"]
yaml = ["serde_yaml"]
all-formats = ["json", "xml", "yaml"]
```

```rust
// src/lib.rs
pub fn export_data(data: &Data, format: &str) -> Result<String, Error> {
    match format {
        #[cfg(feature = "json")]
        "json" => Ok(serde_json::to_string(data)?),

        #[cfg(feature = "xml")]
        "xml" => Ok(quick_xml::se::to_string(data)?),

        #[cfg(feature = "yaml")]
        "yaml" => Ok(serde_yaml::to_string(data)?),

        _ => Err(Error::UnsupportedFormat(format.to_string())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(feature = "json")]
    fn test_export_json() {
        let data = Data::sample();
        let result = export_data(&data, "json").unwrap();
        assert!(result.contains("{"));
    }

    #[test]
    #[cfg(feature = "xml")]
    fn test_export_xml() {
        let data = Data::sample();
        let result = export_data(&data, "xml").unwrap();
        assert!(result.contains("<"));
    }
}
```

```bash
# Solo tests de la feature json (default)
$ cargo test

# Tests con feature xml activada
$ cargo test --features xml

# Tests con todas las features
$ cargo test --features all-formats
# O equivalente:
$ cargo test --all-features

# Tests sin ninguna feature (ni las default)
$ cargo test --no-default-features

# Tests sin defaults, pero con xml
$ cargo test --no-default-features --features xml
```

### Combinación de --features con selección de tests

```bash
# Solo unit tests con feature yaml
$ cargo test --lib --features yaml

# Solo integration tests con todas las features
$ cargo test --test '*' --all-features

# Doc tests con feature json
$ cargo test --doc --features json
```

---

## 13. --workspace y -p: tests en workspaces

### --workspace: tests de todo el workspace

```bash
# Ejecuta tests de TODAS las crates en el workspace
$ cargo test --workspace
```

```
$ cargo test --workspace

     Running unittests (target/debug/deps/core-abc123)
running 20 tests ... ok

     Running unittests (target/debug/deps/api-def456)
running 15 tests ... ok

     Running unittests (target/debug/deps/auth-ghi789)
running 10 tests ... ok

     Running tests/integration.rs (target/debug/deps/integration-jkl012)
running 8 tests ... ok

   Doc-tests core
running 5 tests ... ok

   Doc-tests api
running 3 tests ... ok
```

### -p: tests de una crate específica

```bash
# Solo tests de la crate "core"
$ cargo test -p core

# Solo tests de la crate "api"
$ cargo test -p api

# Múltiples crates
$ cargo test -p core -p api
```

### Combinaciones en workspace

```bash
# Unit tests de "core" + integration tests de "api"
$ cargo test -p core --lib
$ cargo test -p api --test '*'

# Todos los doc tests del workspace
$ cargo test --workspace --doc

# Tests de "core" con feature especial
$ cargo test -p core --features "experimental"

# Todo el workspace excepto una crate
$ cargo test --workspace --exclude legacy_crate
```

---

## 14. --no-run: compilar sin ejecutar

```bash
$ cargo test --no-run
```

Compila todos los binarios de test pero **no los ejecuta**. Útil para:

```bash
# 1. Verificar que los tests compilan sin ejecutarlos
$ cargo test --no-run

# 2. CI: separar compilación de ejecución
$ cargo test --no-run          # Paso 1: compilar
$ cargo test --no-build        # Paso 2: ejecutar (no recompila)
  # Nota: --no-build no existe, pero el caché hace que no recompile

# 3. Pre-compilar antes de un debugging session
$ cargo test --no-run
$ cargo test test_specific_case  # Ejecuta rápido (ya compilado)
```

### Output de --no-run

```
$ cargo test --no-run

   Compiling mi_crate v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
  Executable unittests src/lib.rs (target/debug/deps/mi_crate-abc123)
  Executable tests/api.rs (target/debug/deps/api-def456)
  Executable tests/auth.rs (target/debug/deps/auth-ghi789)
```

Muestra las rutas de los binarios generados. Puedes ejecutar un binario de test directamente:

```bash
# Ejecutar el binario de unit tests directamente
$ ./target/debug/deps/mi_crate-abc123

# Ejecutar con filtro
$ ./target/debug/deps/mi_crate-abc123 parser

# Ejecutar con flags del harness
$ ./target/debug/deps/mi_crate-abc123 --nocapture --test-threads=1
```

---

## 15. --list: listar tests sin ejecutar

```bash
$ cargo test -- --list
```

Lista todos los tests sin ejecutarlos. Útil para inspeccionar qué tests existen:

```
$ cargo test -- --list

     Running unittests src/lib.rs (target/debug/deps/mi_crate-abc123)

parser::tests::test_tokenize: test
parser::tests::test_parse_number: test
parser::tests::test_parse_expression: test
validator::tests::test_range: test
validator::tests::test_email: test
engine::tests::test_process: test

     Running tests/api.rs (target/debug/deps/api-def456)

test_create_resource: test
test_get_resource: test
test_list_resources: test
test_update_resource: test
test_delete_resource: test

   Doc-tests mi_crate

src/lib.rs - add (line 5): test
src/parser.rs - parser::parse (line 12): test
```

### Combinación con filtros

```bash
# Listar solo tests que contienen "parse"
$ cargo test parser -- --list

# Listar solo unit tests
$ cargo test --lib -- --list

# Listar solo tests ignorados
$ cargo test -- --list --ignored

# Contar tests
$ cargo test -- --list 2>/dev/null | grep -c ": test"
```

### Formato machine-readable

```bash
# Formato terse (solo nombres)
$ cargo test -- --list --format terse

parser::tests::test_tokenize
parser::tests::test_parse_number
parser::tests::test_parse_expression
...
```

---

## 16. -- --format: formato del output

### Formato default (pretty)

```
$ cargo test

running 5 tests
test test_a ... ok
test test_b ... ok
test test_c ... FAILED
test test_d ... ok
test test_e ... ignored

failures:

---- test_c stdout ----
assertion `left == right` failed
  left: 3
  right: 4

failures:
    test_c

test result: FAILED. 3 passed; 1 failed; 1 ignored
```

### Formato terse

```bash
$ cargo test -- --format terse
```

```
running 5 tests
.F.i.
failures:

---- test_c stdout ----
assertion `left == right` failed

failures:
    test_c

test result: FAILED. 3 passed; 1 failed; 1 ignored
```

Cada test es un solo carácter:
- `.` = passed
- `F` = failed
- `i` = ignored

Más compacto para muchos tests.

### Formato JSON (nightly)

```bash
$ cargo test -- -Z unstable-options --format json
```

```json
{ "type": "suite", "event": "started", "test_count": 5 }
{ "type": "test", "event": "ok", "name": "test_a" }
{ "type": "test", "event": "ok", "name": "test_b" }
{ "type": "test", "event": "failed", "name": "test_c", "stdout": "..." }
{ "type": "test", "event": "ok", "name": "test_d" }
{ "type": "test", "event": "ignored", "name": "test_e" }
{ "type": "suite", "event": "failed", "passed": 3, "failed": 1, "ignored": 1 }
```

Útil para integración con herramientas de CI que parsean JSON.

---

## 17. -- --show-output: mostrar output de tests exitosos

Ya vimos esto en la sección 8, pero aquí lo detallamos:

```rust
#[test]
fn test_with_logging() {
    println!("Step 1: Initializing");
    let data = setup();
    println!("Step 2: Processing {} items", data.len());
    let result = process(&data);
    println!("Step 3: Result = {:?}", result);
    assert!(result.is_ok());
}
```

### Comportamiento con diferentes flags

```
cargo test:
  test test_with_logging ... ok
  (sin output de println!)

cargo test -- --nocapture:
  Step 1: Initializing
  Step 2: Processing 10 items
  Step 3: Result = Ok(...)
  test test_with_logging ... ok
  (output mezclado con estado del test)

cargo test -- --show-output:
  test test_with_logging ... ok

  successes:
  ---- test_with_logging stdout ----
  Step 1: Initializing
  Step 2: Processing 10 items
  Step 3: Result = Ok(...)
  (output agrupado al final)
```

---

## 18. -- -Z unstable-options: opciones experimentales

Algunas opciones del test harness requieren el flag `-Z unstable-options`. Algunas están disponibles en stable, otras requieren nightly:

### --ensure-time (nightly)

```bash
$ cargo +nightly test -- -Z unstable-options --ensure-time
```

Muestra el tiempo de ejecución de cada test y falla si alguno es demasiado lento:

```
running 5 tests
test test_fast ... ok <0.001s>
test test_medium ... ok <0.5s>
test test_slow ... ok <2.1s>     ← Warning: tardó más de 1s
```

### --report-time (nightly)

```bash
$ cargo +nightly test -- -Z unstable-options --report-time
```

```
running 5 tests
test test_a ... ok <0.001s>
test test_b ... ok <0.023s>
test test_c ... ok <1.234s>
test test_d ... ok <0.002s>
```

### --shuffle (estable desde Rust 1.70)

```bash
$ cargo test -- --shuffle
```

Ejecuta los tests en orden aleatorio. Útil para detectar dependencias entre tests:

```
$ cargo test -- --shuffle

running 5 tests (shuffled)
test test_d ... ok
test test_a ... ok
test test_e ... ok
test test_b ... ok
test test_c ... ok
```

### --shuffle-seed

```bash
$ cargo test -- --shuffle --shuffle-seed 42
```

Misma aleatorización reproducible. Si un test falla con shuffle, puedes reproducir el mismo orden.

---

## 19. Variables de entorno relevantes

### Variables que afectan a cargo test

| Variable | Efecto | Ejemplo |
|----------|--------|---------|
| `RUST_TEST_THREADS` | Número de threads para ejecución | `RUST_TEST_THREADS=1 cargo test` |
| `RUST_TEST_NOCAPTURE` | Equivale a `--nocapture` | `RUST_TEST_NOCAPTURE=1 cargo test` |
| `RUST_BACKTRACE` | Muestra backtrace en panics | `RUST_BACKTRACE=1 cargo test` |
| `RUST_BACKTRACE=full` | Backtrace completo | `RUST_BACKTRACE=full cargo test` |
| `CARGO_TARGET_DIR` | Directorio de build | `CARGO_TARGET_DIR=/tmp/build cargo test` |
| `RUSTFLAGS` | Flags para rustc | `RUSTFLAGS="-D warnings" cargo test` |

### Variables disponibles DENTRO de los tests

```rust
#[test]
fn test_env_vars() {
    // Disponibles en tests:
    let manifest_dir = env!("CARGO_MANIFEST_DIR");
    // → "/home/user/mi_proyecto" (compilado en el binario)

    let pkg_name = env!("CARGO_PKG_NAME");
    // → "mi_proyecto"

    let pkg_version = env!("CARGO_PKG_VERSION");
    // → "0.1.0"

    // Variables de entorno en runtime:
    if let Ok(val) = std::env::var("MY_TEST_CONFIG") {
        println!("Config: {}", val);
    }
}
```

### CARGO_MANIFEST_DIR: acceso a archivos del proyecto

```rust
#[test]
fn test_load_fixture() {
    let fixture_path = format!(
        "{}/tests/fixtures/sample.json",
        env!("CARGO_MANIFEST_DIR")
    );
    let content = std::fs::read_to_string(&fixture_path).unwrap();
    assert!(content.contains("name"));
}
```

### RUST_BACKTRACE: debugging panics

```
$ cargo test test_panic

test test_panic ... FAILED
thread 'test_panic' panicked at 'assertion failed'
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

$ RUST_BACKTRACE=1 cargo test test_panic

test test_panic ... FAILED
thread 'test_panic' panicked at 'assertion failed', src/parser.rs:42:5
stack backtrace:
   0: std::panicking::begin_panic
   1: mi_crate::parser::parse
             at ./src/parser.rs:42
   2: mi_crate::parser::tests::test_panic
             at ./src/parser.rs:85
   ...
```

### Pasar variables de entorno a tests

```bash
# Variable para un solo comando
$ DATABASE_URL="memory://" cargo test

# Exportar para la sesión
$ export TEST_MODE=1
$ cargo test

# Con env en el mismo comando
$ env MY_VAR=value cargo test
```

---

## 20. Workflows prácticos para desarrollo

### Workflow 1: Desarrollo en un módulo

```bash
# Mientras editas src/parser.rs:

# 1. Ejecutar solo tests del módulo (rápido, loop de desarrollo)
$ cargo test --lib parser

# 2. Cuando el módulo está listo, verificar integration tests
$ cargo test --test parser_tests

# 3. Antes de commit, verificar todo
$ cargo test
```

### Workflow 2: Debugging de un test

```bash
# 1. Encontrar el test exacto
$ cargo test -- --list | grep "problematic"

# 2. Ejecutar solo ese test con output visible
$ cargo test problematic_test -- --nocapture --exact

# 3. Si necesitas backtrace
$ RUST_BACKTRACE=1 cargo test problematic_test -- --nocapture --exact

# 4. Si sospechas race condition, ejecutar secuencialmente
$ cargo test problematic_test -- --nocapture --test-threads=1
```

### Workflow 3: CI pipeline

```bash
# Paso 1: Verificar que compila (rápido feedback)
$ cargo test --no-run

# Paso 2: Tests rápidos
$ cargo test

# Paso 3: Tests lentos
$ cargo test -- --ignored

# Paso 4: Tests con todas las features
$ cargo test --all-features

# Paso 5: Release mode (verifica optimizaciones)
$ cargo test --release
```

### Workflow 4: Test-Driven Development (TDD)

```bash
# Loop TDD:

# 1. Escribir test que falla
$ cargo test --lib test_new_feature
# → FAILED (función no implementada)

# 2. Implementar mínimamente
$ cargo test --lib test_new_feature
# → PASSED

# 3. Refactorizar
$ cargo test --lib        # Verificar que nada se rompe
$ cargo test --test '*'   # Verificar integration tests
```

### Workflow 5: Identificar tests lentos

```bash
# 1. Listar todos los tests
$ cargo test -- --list | wc -l

# 2. Ejecutar con timing (nightly)
$ cargo +nightly test -- -Z unstable-options --report-time

# 3. Ejecutar tests en orden aleatorio para encontrar dependencias
$ cargo test -- --shuffle

# 4. Ejecutar secuencialmente para encontrar race conditions
$ cargo test -- --test-threads=1
```

### Workflow 6: Pre-push verification

```bash
# Script de verificación completa antes de push:

# Formateo
$ cargo fmt --check

# Linting
$ cargo clippy -- -D warnings

# Tests unitarios
$ cargo test --lib

# Tests de integración
$ cargo test --test '*'

# Doc tests
$ cargo test --doc

# Release mode
$ cargo test --release

# Tests ignorados (opcionales)
$ cargo test -- --ignored
```

---

## 21. Comparación con C y Go

### C: make test (manual)

```makefile
# Makefile — configuración manual

CC = gcc
CFLAGS = -Wall -Wextra
TEST_FLAGS = -DTEST_MODE

# Compilar y ejecutar todos los tests
test: test_parser test_validator test_engine
	./test_parser
	./test_validator
	./test_engine

# Compilar un test individual
test_parser: tests/test_parser.c src/parser.c
	$(CC) $(CFLAGS) $(TEST_FLAGS) -o $@ $^ -lunity

# Solo un test específico (no hay soporte nativo)
# Necesitarías parametrizar Unity o CTest

# Con CMake/CTest se puede:
# ctest -R "parser"           ← Filtrar por nombre
# ctest -j4                   ← Paralelo (4 jobs)
# ctest -V                    ← Verbose output
# ctest --timeout 10          ← Timeout por test
```

### Go: go test (similar a cargo test)

```bash
# Equivalencias Go → Rust:

# Todos los tests
go test ./...                    # cargo test
cargo test

# Solo un paquete
go test ./pkg/parser             # cargo test -p parser
cargo test -p parser

# Filtrar por nombre (regex en Go, substring en Rust)
go test -run "TestParse"         # cargo test parse
cargo test parse

# Match exacto
go test -run "^TestParseSimple$" # cargo test test_parse_simple -- --exact
cargo test test_parse_simple -- --exact

# Verbose
go test -v ./...                 # cargo test -- --nocapture
cargo test -- --nocapture

# Sin cache
go test -count=1 ./...           # No equivalente directo
                                 # (cargo test no cachea ejecución)

# Solo compilar
go test -run ^$ ./...            # cargo test --no-run
cargo test --no-run

# Tests cortos (skip largos)
go test -short ./...             # cargo test (sin --include-ignored)
cargo test                       # (tests #[ignore] se omiten por default)

# Timeout
go test -timeout 30s             # No equivalente directo
                                 # (se puede con timeout del shell)

# Secuencial
go test -parallel 1              # cargo test -- --test-threads=1
cargo test -- --test-threads=1

# Coverage
go test -cover ./...             # cargo llvm-cov (herramienta externa)

# Race detector
go test -race ./...              # No equivalente
                                 # (Rust previene races en compilación)

# Listar tests
go test -list ".*" ./...         # cargo test -- --list
cargo test -- --list

# Benchmarks
go test -bench "." ./...         # cargo bench (o criterion)
cargo bench

# Shuffle
go test -shuffle=on ./...        # cargo test -- --shuffle
cargo test -- --shuffle
```

### Tabla comparativa

| Aspecto | C (Make/CTest) | Go (go test) | Rust (cargo test) |
|---------|----------------|--------------|-------------------|
| **Ejecutar todo** | `make test` | `go test ./...` | `cargo test` |
| **Filtrar por nombre** | `ctest -R name` | `go test -run "Regex"` | `cargo test substring` |
| **Match exacto** | No nativo | `go test -run "^X$"` | `cargo test X -- --exact` |
| **Solo compilar** | `make test_bin` | `go test -run ^$` | `cargo test --no-run` |
| **Output visible** | Siempre visible | `go test -v` | `cargo test -- --nocapture` |
| **Secuencial** | `ctest -j1` | `go test -parallel 1` | `cargo test -- --test-threads=1` |
| **Skip lentos** | No estándar | `testing.Short()` | `#[ignore]` / `--ignored` |
| **Shuffle** | No | `go test -shuffle=on` | `cargo test -- --shuffle` |
| **Release mode** | `-O2` en Makefile | No (siempre build) | `cargo test --release` |
| **Features** | `#ifdef` en Makefile | Build tags | `--features` / `--all-features` |
| **Workspace** | No | `./...` recursivo | `--workspace` |
| **Listar tests** | `ctest -N` | `go test -list "."` | `cargo test -- --list` |
| **Separador de flags** | N/A | No necesario | `--` (Cargo vs harness) |

---

## 22. Referencia rápida completa

### Flags de Cargo (antes del --)

| Flag | Efecto |
|------|--------|
| `--lib` | Solo unit tests de src/lib.rs |
| `--bins` | Solo tests de binarios |
| `--bin NAME` | Tests del binario NAME |
| `--test NAME` | Solo tests/NAME.rs |
| `--test '*'` | Todos los integration tests |
| `--doc` | Solo doc tests |
| `--benches` | Solo benchmarks |
| `--all-targets` | Todo: lib + bins + tests + benches + examples |
| `--release` | Compilar con optimizaciones |
| `--features "F"` | Activar feature F |
| `--all-features` | Activar todas las features |
| `--no-default-features` | Desactivar features default |
| `--workspace` | Todos los miembros del workspace |
| `-p NAME` | Solo la crate NAME |
| `--exclude NAME` | Excluir crate del workspace |
| `--no-run` | Compilar sin ejecutar |
| `--no-fail-fast` | No parar en primer fallo |
| `--jobs N` / `-j N` | Compilaciones en paralelo (compilación, no ejecución) |
| `--target TRIPLE` | Cross-compilation target |
| `--message-format FMT` | json, short, human |
| `TESTNAME` | Filtro substring para nombres de test |

### Flags del test harness (después del --)

| Flag | Efecto |
|------|--------|
| `--nocapture` | No capturar stdout/stderr |
| `--show-output` | Mostrar output de tests exitosos al final |
| `--test-threads=N` | N threads para ejecución (1 = secuencial) |
| `--ignored` | Solo tests con `#[ignore]` |
| `--include-ignored` | Incluir tests con `#[ignore]` |
| `--exact` | Match exacto de nombre (no substring) |
| `--list` | Listar tests sin ejecutar |
| `--format FMT` | pretty (default), terse, json |
| `--shuffle` | Orden aleatorio |
| `--shuffle-seed N` | Seed para shuffle reproducible |
| `-Z unstable-options` | Habilitar opciones experimentales |
| `--report-time` | Mostrar tiempo por test (nightly) |
| `--ensure-time` | Fallar si un test es muy lento (nightly) |
| `--skip FILTER` | Excluir tests que contienen FILTER |
| `--color auto\|always\|never` | Control de color |
| `-q` / `--quiet` | Output mínimo |

### Combinaciones más usadas

```bash
# Desarrollo diario
cargo test --lib                           # Unit tests rápidos
cargo test --lib parser                    # Solo un módulo
cargo test --test api create               # Integration test filtrado

# Debugging
cargo test test_name -- --nocapture        # Ver output
cargo test test_name -- --exact --nocapture # Test exacto con output
RUST_BACKTRACE=1 cargo test test_name      # Con backtrace

# CI
cargo test                                 # Todo estándar
cargo test --no-fail-fast                  # Ver todos los fallos
cargo test -- --include-ignored            # Incluir lentos
cargo test --release                       # Verificar en release
cargo test --all-features                  # Todas las features
cargo test --workspace                     # Todo el workspace

# Investigación
cargo test -- --list                       # Listar tests
cargo test -- --list | wc -l               # Contar tests
cargo test -- --shuffle                    # Detectar dependencias
cargo test -- --test-threads=1             # Detectar race conditions
cargo test --no-run                        # Solo verificar compilación
```

---

## 23. Errores comunes

### Error 1: Flags del lado equivocado del --

```bash
# ✗ ERROR: --nocapture es del harness, no de Cargo
$ cargo test --nocapture
# error: unexpected argument '--nocapture'

# ✓ CORRECTO
$ cargo test -- --nocapture

# ✗ ERROR: --lib es de Cargo, no del harness
$ cargo test -- --lib
# error: unexpected argument '--lib'

# ✓ CORRECTO
$ cargo test --lib
```

### Error 2: Confundir el filtro con --test

```bash
# ✗ Estos NO son lo mismo:

$ cargo test auth
# Ejecuta TODOS los tests (unit + integration + doc) cuyo nombre contiene "auth"
# Esto incluye tests de TODOS los binarios

$ cargo test --test auth
# Ejecuta solo el archivo tests/auth.rs
# Todos los tests dentro de ese archivo, sin importar nombre
```

```
cargo test auth:                      cargo test --test auth:

  src/lib.rs:                           tests/auth.rs:
    auth::tests::test_login ✓            test_login ✓
    auth::tests::test_logout ✓           test_logout ✓
  tests/auth.rs:                         test_create_user ✓
    test_login ✓                         test_permission ✓
    test_logout ✓
    test_create_user ✓
    test_permission ✓
  tests/api.rs:
    test_auth_header ✓                 (solo tests/auth.rs,
    test_auth_middleware ✓              todos los tests del archivo)
  (todo lo que contenga "auth")
```

### Error 3: --ignored sin el --

```bash
# ✗ ERROR: "ignored" se interpreta como filtro de nombre
$ cargo test --ignored
# Busca tests cuyo nombre contenga "ignored"... no encuentra ninguno

# ✓ CORRECTO
$ cargo test -- --ignored
```

### Error 4: Esperar que --test-threads acelere compilación

```bash
# ✗ CONFUSIÓN: --test-threads afecta EJECUCIÓN, no compilación
$ cargo test -- --test-threads=8
# La compilación sigue siendo igual de lenta

# ✓ Para acelerar compilación, usa -j:
$ cargo test -j 8
# -j es un flag de CARGO (antes del --)
```

### Error 5: No encontrar por qué un test no aparece

```bash
$ cargo test my_test
# running 0 tests ... ← No encuentra nada

# Diagnóstico:
# 1. ¿El test existe?
$ cargo test -- --list | grep my_test

# 2. ¿Está marcado como #[ignore]?
$ cargo test -- --list --ignored | grep my_test

# 3. ¿Está detrás de un #[cfg(feature)]?
$ cargo test --all-features my_test

# 4. ¿Está en otro crate del workspace?
$ cargo test --workspace my_test

# 5. ¿El nombre es correcto?
$ cargo test -- --list | grep -i test  # Buscar case-insensitive
```

### Error 6: Asumir orden de ejecución

```bash
# Los tests NO se ejecutan en el orden del código fuente.
# Se ejecutan en paralelo y en orden no determinístico.

# Si un test depende de otro:
#   test_a: crea un archivo
#   test_b: lee el archivo creado por test_a
# → test_b puede ejecutarse ANTES que test_a

# Solución: hacer cada test independiente
# O usar --test-threads=1 como workaround temporal
```

### Error 7: --release cambiando resultados

```bash
$ cargo test            # Pasa
$ cargo test --release  # Falla

# Posibles causas:
# 1. Integer overflow (debug paniquea, release wraps)
# 2. debug_assert! (solo en debug)
# 3. Timing-dependent code (release es más rápido)
# 4. Undefined behavior expuesto por optimizaciones
```

### Error 8: Olvidar --no-fail-fast en CI

```bash
# ✗ CI con fail-fast: solo reporta el primer fallo
$ cargo test
# → Ve 1 fallo, el desarrollador lo arregla, push, espera CI...
# → Ahora ve otro fallo. Otro ciclo.

# ✓ CI con todos los fallos: reporta todo
$ cargo test --no-fail-fast
# → Ve 5 fallos, arregla todos, push una vez.
```

---

## 24. Programa de práctica

Crea un proyecto `cargo_test_lab` con la siguiente estructura y practica todos los flags documentados en este tópico.

### Estructura del proyecto

```
cargo_test_lab/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── fast.rs       // Funciones rápidas
│   └── slow.rs       // Funciones lentas (simuladas)
└── tests/
    ├── common/
    │   └── mod.rs
    ├── fast_tests.rs
    ├── slow_tests.rs
    └── edge_cases.rs
```

### Requisitos del código

```rust
// src/lib.rs
pub mod fast;
pub mod slow;

// src/fast.rs — Al menos 3 funciones pub con unit tests (5+ tests)

pub fn add(a: i32, b: i32) -> i32 { todo!() }
pub fn reverse(s: &str) -> String { todo!() }
pub fn is_palindrome(s: &str) -> bool { todo!() }

// src/slow.rs — Al menos 2 funciones pub
pub fn fibonacci(n: u64) -> u64 { todo!() }
pub fn process_large_data(size: usize) -> Vec<i32> { todo!() }
```

### Requisitos de los tests

1. **Unit tests en `src/fast.rs`** (dentro de `mod tests`):
   - Al menos 5 tests normales
   - Al menos 1 test con `println!` para probar `--nocapture`
   - Al menos 1 test con `#[ignore = "slow computation"]`

2. **Unit tests en `src/slow.rs`** (dentro de `mod tests`):
   - Al menos 2 tests normales
   - Al menos 2 tests con `#[ignore = "requires time"]`

3. **Integration tests en `tests/fast_tests.rs`**:
   - Al menos 4 tests que usen solo API pública
   - Usar `mod common;` con al menos 1 helper

4. **Integration tests en `tests/slow_tests.rs`**:
   - Al menos 2 tests con `#[ignore]`
   - Al menos 1 test normal

5. **Integration tests en `tests/edge_cases.rs`**:
   - Al menos 3 tests de edge cases
   - Al menos 1 test con `#[should_panic]`

### Ejercicios de ejecución con flags

Una vez implementado el proyecto, ejecuta cada uno de estos comandos y **anota el output** (cuántos tests se ejecutaron, cuántos se ignoraron, cuántos se filtraron):

```bash
# A. Ejecutar todo
$ cargo test

# B. Solo unit tests
$ cargo test --lib

# C. Solo unit tests del módulo fast
$ cargo test --lib fast

# D. Solo tests/fast_tests.rs
$ cargo test --test fast_tests

# E. Solo doc tests
$ cargo test --doc

# F. Tests con output visible
$ cargo test -- --nocapture

# G. Solo tests ignorados
$ cargo test -- --ignored

# H. Todo incluyendo ignorados
$ cargo test -- --include-ignored

# I. Ejecución secuencial
$ cargo test -- --test-threads=1

# J. Listar todos los tests
$ cargo test -- --list

# K. Tests en modo release
$ cargo test --release

# L. Solo compilar
$ cargo test --no-run

# M. Filtrar por nombre "palindrome"
$ cargo test palindrome

# N. Match exacto
$ cargo test fast::tests::test_add -- --exact

# O. Formato terse
$ cargo test -- --format terse

# P. Shuffle
$ cargo test -- --shuffle

# Q. No parar en primer fallo
$ cargo test --no-fail-fast

# R. Con backtrace
$ RUST_BACKTRACE=1 cargo test

# S. Secuencial con output
$ cargo test -- --test-threads=1 --nocapture

# T. Listar solo los ignorados
$ cargo test -- --list --ignored
```

Crea un archivo `RESULTADOS.md` con la salida resumida de cada comando (cuántos tests corrieron, cuántos pasaron, cuántos ignorados, cuántos filtrados).

---

## 25. Ejercicios

### Ejercicio 1: Construir el comando correcto

Para cada escenario, escribe el comando `cargo test` exacto:

**a)** Ejecutar solo los unit tests del módulo `validator` sin capturar output.

**b)** Ejecutar todos los integration tests cuyo nombre contiene "auth", en un solo thread.

**c)** Listar todos los tests marcados con `#[ignore]` en todo el workspace.

**d)** Ejecutar solo el test llamado exactamente `tests::test_parse_simple` en modo release.

**e)** Compilar todos los tests sin ejecutarlos, con la feature "experimental" activada.

**f)** Ejecutar todos los tests del archivo `tests/api.rs` que contengan "create", mostrando output y en modo secuencial.

**g)** Ejecutar todo el workspace sin parar en primer fallo e incluyendo tests ignorados.

**h)** Ejecutar solo doc tests con backtrace activado.

---

### Ejercicio 2: Diagnóstico de comandos

Para cada comando, explica exactamente qué hará:

```bash
# a)
$ cargo test --lib -- --test-threads=1 --ignored parser

# b)
$ cargo test --test integration auth -- --exact --nocapture

# c)
$ cargo test --workspace --release -- --include-ignored --format terse

# d)
$ cargo test --no-run --all-features -p core

# e)
$ RUST_BACKTRACE=full cargo test --no-fail-fast -- --nocapture --test-threads=1
```

---

### Ejercicio 3: Diseño de CI pipeline

Diseña un pipeline de CI con 5 pasos usando `cargo test`. Cada paso debe:
- Tener un nombre descriptivo
- Usar el comando exacto
- Explicar qué verifica
- Indicar si debe bloquear el merge o solo informar

```
Paso 1: ??? — cargo test ???
  Verifica: ???
  Bloquea merge: sí/no

Paso 2: ...
```

---

### Ejercicio 4: Análisis de output

Dado el siguiente output de `cargo test`, responde las preguntas:

```
$ cargo test --no-fail-fast

   Compiling mi_proyecto v0.1.0
    Finished test [unoptimized + debuginfo] target(s)

     Running unittests src/lib.rs (target/debug/deps/mi_proyecto-a1b2c3)

running 25 tests
test engine::tests::test_process ... ok
test engine::tests::test_parallel ... FAILED
test parser::tests::test_tokenize ... ok
test parser::tests::test_parse ... ok
test parser::tests::test_complex ... ok
test validator::tests::test_range ... ok
test validator::tests::test_email ... FAILED
test validator::tests::test_phone ... ok
test config::tests::test_load ... ok
test config::tests::test_save ... ok
test config::tests::test_merge ... ok
test utils::tests::test_helper_a ... ok
test utils::tests::test_helper_b ... ok
test utils::tests::test_slow ... ignored
test utils::tests::test_flaky ... ignored
...
test result: FAILED. 20 passed; 2 failed; 3 ignored; 0 measured; 0 filtered out

     Running tests/api.rs (target/debug/deps/api-d4e5f6)

running 8 tests
test test_create ... ok
test test_read ... ok
test test_update ... ok
test test_delete ... ok
test test_list ... ok
test test_search ... ok
test test_bulk_create ... ok
test test_permissions ... FAILED
test result: FAILED. 7 passed; 1 failed; 0 ignored; 0 filtered out

     Running tests/auth.rs (target/debug/deps/auth-g7h8i9)

running 5 tests
test test_login ... ok
test test_logout ... ok
test test_refresh_token ... ok
test test_expire_token ... ok
test test_invalid_token ... ok
test result: ok. 5 passed; 0 failed; 0 ignored; 0 filtered out

   Doc-tests mi_proyecto

running 4 tests
test src/lib.rs - Config (line 12) ... ok
test src/parser.rs - parser::parse (line 5) ... FAILED
test src/lib.rs - (line 3) ... ok
test src/engine.rs - engine::process (line 8) ... ok
test result: FAILED. 3 passed; 1 failed; 0 ignored; 0 filtered out
```

**a)** ¿Cuántos tests se ejecutaron en total? ¿Cuántos pasaron, fallaron e ignoraron?

**b)** ¿Por qué se ejecutaron TODOS los tests aunque hubo fallos? ¿Qué flag lo causó?

**c)** Si solo quisieras re-ejecutar los tests fallidos, ¿qué comandos usarías? (Da los comandos exactos para cada fallo)

**d)** ¿Qué comando usarías para ejecutar solo los 3 tests ignorados?

**e)** ¿Cuántos binarios de test se generaron? Nómbralos.

**f)** Si el pipeline de CI usara `cargo test` (sin `--no-fail-fast`), ¿cuántos fallos se habrían reportado en la primera ejecución?

---

## Navegación

- **Anterior**: [T03 - Test helpers compartidos](../T03_Test_helpers_compartidos/README.md)
- **Siguiente**: [S03/T01 - Concepto de Property-Based Testing](../../S03_Property_Based_Testing/T01_Concepto/README.md)

---

> **Fin de la Sección 2: Integration Tests y Doc Tests**
>
> Esta sección cubrió el testing más allá del módulo:
> - T01: Integration tests — directorio tests/, crate separado, API pública, helpers compartidos
> - T02: Doc tests — ` ``` ` en doc comments, compilación por rustdoc, no_run, ignore, compile_fail
> - T03: Test helpers compartidos — common/mod.rs, fixtures, builders, RAII, custom assertions, fakes
> - T04: cargo test flags — --lib, --test, --doc, --release, --nocapture, filtrado, --ignored, --shuffle
>
> Con las Secciones 1 y 2 completas, tienes dominio del sistema de testing integrado de Rust.
> La Sección 3 introduce **Property-Based Testing** — generación automática de inputs y verificación de invariantes.
