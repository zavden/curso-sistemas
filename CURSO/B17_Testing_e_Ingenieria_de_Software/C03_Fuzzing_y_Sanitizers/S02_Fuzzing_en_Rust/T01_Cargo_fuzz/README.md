# T01 - cargo-fuzz: instalación, cargo fuzz init, fuzz_target!, corpus, artifacts

## Índice

1. [De C a Rust: por qué fuzzing en Rust es diferente](#1-de-c-a-rust-por-qué-fuzzing-en-rust-es-diferente)
2. [cargo-fuzz: qué es y qué NO es](#2-cargo-fuzz-qué-es-y-qué-no-es)
3. [Arquitectura de cargo-fuzz](#3-arquitectura-de-cargo-fuzz)
4. [Requisitos previos](#4-requisitos-previos)
5. [Instalación](#5-instalación)
6. [Verificar la instalación](#6-verificar-la-instalación)
7. [cargo fuzz init: inicializar fuzzing en un proyecto](#7-cargo-fuzz-init-inicializar-fuzzing-en-un-proyecto)
8. [Anatomía completa de la estructura generada](#8-anatomía-completa-de-la-estructura-generada)
9. [El archivo Cargo.toml del directorio fuzz/](#9-el-archivo-cargotoml-del-directorio-fuzz)
10. [fuzz_target!: la macro fundamental](#10-fuzz_target-la-macro-fundamental)
11. [Expansión de la macro fuzz_target!](#11-expansión-de-la-macro-fuzz_target)
12. [Contrato del closure dentro de fuzz_target!](#12-contrato-del-closure-dentro-de-fuzz_target)
13. [Patrones de harness en Rust con fuzz_target!](#13-patrones-de-harness-en-rust-con-fuzz_target)
14. [cargo fuzz add: agregar más targets](#14-cargo-fuzz-add-agregar-más-targets)
15. [cargo fuzz run: ejecutar el fuzzer](#15-cargo-fuzz-run-ejecutar-el-fuzzer)
16. [Flags y opciones de cargo fuzz run](#16-flags-y-opciones-de-cargo-fuzz-run)
17. [Interpretar la salida del fuzzer](#17-interpretar-la-salida-del-fuzzer)
18. [Corpus: concepto y gestión en cargo-fuzz](#18-corpus-concepto-y-gestión-en-cargo-fuzz)
19. [Crear corpus inicial (semillas)](#19-crear-corpus-inicial-semillas)
20. [Corpus merge: optimización periódica](#20-corpus-merge-optimización-periódica)
21. [Artifacts: crashes, timeouts y OOM](#21-artifacts-crashes-timeouts-y-oom)
22. [Interpretar un crash artifact](#22-interpretar-un-crash-artifact)
23. [Minimizar artifacts](#23-minimizar-artifacts)
24. [Regression testing con artifacts](#24-regression-testing-con-artifacts)
25. [cargo fuzz coverage: medir cobertura](#25-cargo-fuzz-coverage-medir-cobertura)
26. [cargo fuzz list y cargo fuzz fmt](#26-cargo-fuzz-list-y-cargo-fuzz-fmt)
27. [Diccionarios en cargo-fuzz](#27-diccionarios-en-cargo-fuzz)
28. [Configuración avanzada en Cargo.toml](#28-configuración-avanzada-en-cargotoml)
29. [Múltiples targets: estrategia y organización](#29-múltiples-targets-estrategia-y-organización)
30. [Flujo de trabajo completo: del init al fix](#30-flujo-de-trabajo-completo-del-init-al-fix)
31. [Comparación con fuzzing en C](#31-comparación-con-fuzzing-en-c)
32. [Errores comunes](#32-errores-comunes)
33. [Programa de práctica: json_value parser](#33-programa-de-práctica-json_value-parser)
34. [Ejercicios](#34-ejercicios)

---

## 1. De C a Rust: por qué fuzzing en Rust es diferente

En los 4 tópicos anteriores (S01) aprendimos a fuzzear código C con AFL++ y libFuzzer. El fuzzing en C se centra en encontrar errores de memoria: buffer overflows, use-after-free, double-free, lecturas no inicializadas. Estos bugs son posibles porque C no tiene protecciones de memoria en tiempo de compilación.

Rust cambia fundamentalmente esta ecuación:

```
┌─────────────────────────────────────────────────────────────────┐
│                  Modelo de seguridad de Rust                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Safe Rust:                                                     │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ ✗ Buffer overflow      → bounds checking automático      │   │
│  │ ✗ Use-after-free       → ownership + borrow checker      │   │
│  │ ✗ Double-free          → Drop automático, una sola vez   │   │
│  │ ✗ Null dereference     → Option<T> en vez de NULL        │   │
│  │ ✗ Data races           → Send + Sync traits              │   │
│  │                                                          │   │
│  │ ✓ Panics (index OOB)   → el fuzzer puede encontrarlos    │   │
│  │ ✓ Lógica incorrecta    → el fuzzer puede encontrarlos    │   │
│  │ ✓ Infinite loops       → el fuzzer puede detectarlos     │   │
│  │ ✓ OOM (allocaciones)   → el fuzzer puede detectarlas     │   │
│  │ ✓ Unwrap en None/Err   → el fuzzer puede encontrarlos    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Unsafe Rust:                                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ ✓ TODOS los bugs de C son posibles en bloques unsafe     │   │
│  │ ✓ Sanitizers (ASan, MSan, TSan) son relevantes           │   │
│  │ ✓ FFI con código C hereda todos los riesgos de C         │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### ¿Entonces para qué fuzzear Rust safe?

El fuzzing en Rust safe busca tipos de bugs diferentes a los de C:

```
┌──────────────────────────────────────────────────────────────┐
│              Bugs que el fuzzer encuentra en Rust safe        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. PANICS                                                   │
│     ├── unwrap() en None o Err                               │
│     ├── index fuera de bounds (sin .get())                   │
│     ├── integer overflow en debug mode                       │
│     ├── slice::from_raw_parts con rangos inválidos           │
│     └── assert! / unreachable! alcanzados                    │
│                                                              │
│  2. ERRORES LÓGICOS                                          │
│     ├── Roundtrip failures: encode(decode(x)) ≠ x           │
│     ├── Invariantes rotos en estructuras de datos            │
│     ├── Parsing incorrecto (acepta input inválido)           │
│     └── Estado corrupto tras secuencia de operaciones        │
│                                                              │
│  3. RENDIMIENTO                                              │
│     ├── Timeouts por complejidad algorítmica                 │
│     ├── OOM por allocaciones descontroladas                  │
│     └── Recursión infinita (stack overflow)                  │
│                                                              │
│  4. UNSAFE (si existe)                                       │
│     ├── Todos los bugs de C                                  │
│     └── UB que el borrow checker no puede verificar          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

Esto significa que el fuzzing en Rust tiene un enfoque **más lógico y menos de memoria** comparado con C. En C, un crash casi siempre es un error de memoria. En Rust safe, un crash es un panic, y el fuzzer nos ayuda a encontrar los inputs que causan panics inesperados.

---

## 2. cargo-fuzz: qué es y qué NO es

### Qué es

`cargo-fuzz` es un subcomando de Cargo que proporciona una interfaz ergonómica para fuzzear proyectos Rust usando **libFuzzer** como motor de fuzzing. Es el estándar de facto para fuzzing en el ecosistema Rust.

```
┌───────────────────────────────────────────────────────────┐
│                     cargo-fuzz                             │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐  │
│  │              Interfaz de usuario                    │  │
│  │  cargo fuzz init | run | add | list | fmt | cmin    │  │
│  │  cargo fuzz tmin | coverage                         │  │
│  └─────────────┬───────────────────────────────────────┘  │
│                │                                          │
│  ┌─────────────▼───────────────────────────────────────┐  │
│  │              Motor: libFuzzer (LLVM)                 │  │
│  │  - Coverage-guided                                  │  │
│  │  - In-process (no fork)                             │  │
│  │  - Enlazado estáticamente en el binario             │  │
│  └─────────────┬───────────────────────────────────────┘  │
│                │                                          │
│  ┌─────────────▼───────────────────────────────────────┐  │
│  │              Compilación: Rust nightly               │  │
│  │  - -Cpasses=sancov-module                           │  │
│  │  - -Zsanitizer=address (opcional)                   │  │
│  │  - Instrumentación de cobertura LLVM                │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

### Qué NO es

| Lo que NO es | Explicación |
|---|---|
| Un fuzzer nuevo | No implementa ningún algoritmo de fuzzing. Usa libFuzzer internamente |
| Un sustituto de tests | Complementa tests unitarios, no los reemplaza |
| Solo para unsafe | Encuentra panics, errores lógicos y rendimiento en safe Rust |
| Una herramienta de CI completa | No gestiona reportes ni dashboards. Para CI, se integra con OSS-Fuzz |
| Compatible con stable Rust | Requiere nightly porque la instrumentación de cobertura necesita flags inestables |

### Relación con libFuzzer

En el tópico T03 de S01, aprendimos libFuzzer escribiendo `LLVMFuzzerTestOneInput` en C. `cargo-fuzz` genera exactamente esa función internamente, pero a través de la macro `fuzz_target!`. La relación es:

```
C con libFuzzer (S01/T03):
──────────────────────────
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // tu código aquí
    return 0;
}
Compilar: clang -fsanitize=fuzzer ...

Rust con cargo-fuzz (este tópico):
──────────────────────────────────
fuzz_target!(|data: &[u8]| {
    // tu código aquí
});
Compilar: cargo fuzz run target_name

  ↓ internamente genera ↓

#[no_mangle]
pub extern "C" fn rust_fuzzer_test_input(data: &[u8]) { ... }
  → que libFuzzer llama como LLVMFuzzerTestOneInput
```

---

## 3. Arquitectura de cargo-fuzz

```
                         cargo fuzz run my_target
                                │
                ┌───────────────▼──────────────────┐
                │       cargo-fuzz (subcomando)     │
                │                                   │
                │  1. Verificar nightly toolchain   │
                │  2. Configurar RUSTFLAGS           │
                │  3. Invocar cargo build            │
                └───────────────┬──────────────────┘
                                │
            ┌───────────────────▼───────────────────┐
            │            Compilación                 │
            │                                       │
            │  rustc + LLVM                          │
            │  ┌─────────────────────────────────┐  │
            │  │ Tu código (lib.rs)               │  │
            │  │  + instrumentación de cobertura  │  │
            │  │  → cada edge/branch/bloque       │  │
            │  │    registra que fue visitado      │  │
            │  └──────────────┬──────────────────┘  │
            │                 │                      │
            │  ┌──────────────▼──────────────────┐  │
            │  │ fuzz_target! (harness)           │  │
            │  │  → genera LLVMFuzzerTestOneInput │  │
            │  │  → enlaza con libFuzzer          │  │
            │  └──────────────┬──────────────────┘  │
            │                 │                      │
            │  ┌──────────────▼──────────────────┐  │
            │  │ libFuzzer (libfuzzer-sys crate)  │  │
            │  │  → main()                        │  │
            │  │  → motor de mutación             │  │
            │  │  → gestión de corpus             │  │
            │  │  → detección de crashes          │  │
            │  └─────────────────────────────────┘  │
            └───────────────────┬───────────────────┘
                                │
                    ┌───────────▼────────────┐
                    │  Binario ejecutable     │
                    │  (todo enlazado junto)  │
                    │                        │
                    │  Corre in-process:      │
                    │  1. Lee corpus          │
                    │  2. Muta inputs         │
                    │  3. Llama tu closure    │
                    │  4. Registra cobertura  │
                    │  5. Guarda lo nuevo     │
                    │  6. Detecta panics      │
                    │  7. Guarda artifacts    │
                    └────────────────────────┘
```

### Componentes clave

1. **`cargo-fuzz`** (crate): subcomando de Cargo que orquesta todo. Se instala con `cargo install`.

2. **`libfuzzer-sys`** (crate): bindings de Rust a la librería libFuzzer de LLVM. Proporciona la macro `fuzz_target!` y enlaza con el motor de fuzzing.

3. **`arbitrary`** (crate, opcional): permite derivar `Arbitrary` para tipos custom, generando inputs estructurados en vez de bytes crudos. Lo veremos en T02.

4. **Rust nightly**: necesario porque la instrumentación de cobertura usa flags del compilador que no están estabilizados: `-Cpasses=sancov-module`, `-Cllvm-args=-sanitizer-coverage-level=4`, etc.

---

## 4. Requisitos previos

### Sistema

| Requisito | Mínimo | Recomendado | Por qué |
|---|---|---|---|
| Rust nightly | Cualquier nightly reciente | Último nightly | Flags de instrumentación pueden cambiar |
| Cargo | Incluido con Rust | — | Gestor de paquetes |
| Clang/LLVM | v11+ | v15+ | Backend de compilación, necesario para libFuzzer |
| RAM | 2 GB | 8+ GB | ASan duplica/triplica uso de memoria |
| Disk | 1 GB libre | 10+ GB | Corpus crece con el tiempo |
| CPU | 1 core | 4+ cores | Para fuzzing paralelo con -jobs |

### Verificar Rust nightly

```bash
# Verificar si ya tienes nightly instalado
rustup toolchain list

# Instalar nightly si no lo tienes
rustup toolchain install nightly

# Verificar la versión
rustup run nightly rustc --version
# rustc 1.XX.0-nightly (abc1234 2026-04-01)
```

### Nota sobre nightly como default

`cargo-fuzz` automáticamente usa nightly al ejecutar `cargo fuzz run`, así que **no necesitas** hacer `rustup default nightly`. Tu toolchain default puede seguir siendo stable. Internamente, cargo-fuzz ejecuta `cargo +nightly build ...`.

Si prefieres especificar explícitamente:

```bash
# Opción 1: override por directorio (archivo rust-toolchain.toml en fuzz/)
# cargo-fuzz ya maneja esto automáticamente

# Opción 2: variable de entorno
RUSTUP_TOOLCHAIN=nightly cargo fuzz run my_target
```

---

## 5. Instalación

### Método 1: cargo install (recomendado)

```bash
cargo install cargo-fuzz
```

Esto compila e instala el binario `cargo-fuzz` en `~/.cargo/bin/`. Como `cargo-fuzz` empieza con `cargo-`, Cargo lo reconoce automáticamente como subcomando y puedes usarlo como `cargo fuzz`.

### Método 2: desde source (última versión)

```bash
# Clonar el repositorio
git clone https://github.com/rust-fuzz/cargo-fuzz.git
cd cargo-fuzz

# Instalar desde source
cargo install --path .
```

### Método 3: versión específica

```bash
# Instalar una versión específica
cargo install cargo-fuzz --version 0.12.0

# Ver versiones disponibles
cargo search cargo-fuzz
```

### Verificar la instalación

```bash
cargo fuzz --version
# cargo-fuzz 0.12.0

cargo fuzz --help
# Muestra todos los subcomandos disponibles
```

### Posibles problemas de instalación

```
Problema: "error: linker 'cc' not found"
Solución: instalar build-essential (Ubuntu) o base-devel (Fedora/Arch)
         sudo dnf groupinstall "Development Tools"

Problema: "error: failed to run custom build command for libfuzzer-sys"
Solución: instalar clang y llvm
         sudo dnf install clang llvm-devel

Problema: "error: toolchain 'nightly' is not installed"
Solución: rustup toolchain install nightly
```

---

## 6. Verificar la instalación

Vamos a crear un proyecto mínimo para verificar que todo funciona:

```bash
# Crear un proyecto temporal
cargo new --lib verify_fuzz
cd verify_fuzz

# Inicializar fuzzing
cargo fuzz init

# Ejecutar el target por defecto (5 segundos son suficientes para verificar)
cargo fuzz run fuzz_target_1 -- -max_total_time=5
```

Si ves output similar a esto, todo está funcionando:

```
   Compiling verify_fuzz v0.1.0 (/tmp/verify_fuzz)
   Compiling verify_fuzz-fuzz v0.0.0 (/tmp/verify_fuzz/fuzz)
    Finished ...
    Running `fuzz/target/x86_64-unknown-linux-gnu/release/fuzz_target_1 ...`
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 1234567890
INFO: Loaded 1 modules   (XX inline 8-bit counters): ...
INFO: -max_total_time is set to 5 (in seconds)
INFO:        0 files found in fuzz/corpus/fuzz_target_1
INFO: A]corpus was provided - starting from a single 0-byte input
INFO: #2      INITED cov: 2 ft: 2 corp: 1/1b ...
INFO: #100    NEW    cov: 3 ft: 3 corp: 2/3b ...
...
INFO: Done 12345 runs in 5 second(s)
```

Después de verificar, puedes eliminar el proyecto temporal:

```bash
cd ..
rm -rf verify_fuzz
```

---

## 7. cargo fuzz init: inicializar fuzzing en un proyecto

`cargo fuzz init` prepara un proyecto Rust existente para fuzzing. Se ejecuta **una sola vez** en la raíz del proyecto.

### Sintaxis

```bash
cargo fuzz init [OPTIONS]
```

### Opciones

| Opción | Descripción |
|---|---|
| (sin opciones) | Crea la estructura básica con un target por defecto |
| `-t <NAME>` | Nombre del target inicial (default: `fuzz_target_1`) |

### Qué hace exactamente

```bash
$ cargo fuzz init
# Crea la siguiente estructura:

fuzz/
├── Cargo.toml          ← workspace del fuzzer (separado del proyecto principal)
└── fuzz_targets/
    └── fuzz_target_1.rs  ← tu primer harness
```

### Qué modifica

`cargo fuzz init` **no modifica** tu `Cargo.toml` principal ni ningún archivo existente. Crea un directorio `fuzz/` completamente aislado que es un workspace de Cargo independiente.

```
mi_proyecto/
├── Cargo.toml           ← NO se modifica
├── src/
│   └── lib.rs           ← NO se modifica
├── tests/               ← NO se modifica
└── fuzz/                ← NUEVO: todo lo de fuzzing vive aquí
    ├── Cargo.toml       ← workspace independiente
    └── fuzz_targets/
        └── fuzz_target_1.rs
```

### Ejecutar init con nombre personalizado

```bash
# En vez de fuzz_target_1, usar un nombre descriptivo
cargo fuzz init -t parse_input

# Resultado:
fuzz/
├── Cargo.toml
└── fuzz_targets/
    └── parse_input.rs
```

### Ejemplo paso a paso con un proyecto real

```bash
# 1. Crear un proyecto de ejemplo
cargo new --lib my_parser
cd my_parser

# 2. Escribir algo de código en src/lib.rs
cat > src/lib.rs << 'EOF'
pub fn parse_csv_line(input: &str) -> Vec<String> {
    let mut fields = Vec::new();
    let mut current = String::new();
    let mut in_quotes = false;

    for ch in input.chars() {
        match ch {
            '"' => in_quotes = !in_quotes,
            ',' if !in_quotes => {
                fields.push(current.clone());
                current.clear();
            }
            '\n' if !in_quotes => break,
            _ => current.push(ch),
        }
    }

    if !current.is_empty() {
        fields.push(current);
    }

    fields
}
EOF

# 3. Inicializar fuzzing
cargo fuzz init -t fuzz_csv

# 4. Editar el harness generado
cat > fuzz/fuzz_targets/fuzz_csv.rs << 'EOF'
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_parser::parse_csv_line;

fuzz_target!(|data: &[u8]| {
    if let Ok(s) = std::str::from_utf8(data) {
        let _ = parse_csv_line(s);
    }
});
EOF

# 5. Ejecutar
cargo fuzz run fuzz_csv
```

---

## 8. Anatomía completa de la estructura generada

Después de `cargo fuzz init` y agregar corpus/artifacts, la estructura completa se ve así:

```
mi_proyecto/
├── Cargo.toml                    ← proyecto principal
├── Cargo.lock
├── src/
│   └── lib.rs                    ← código a fuzzear
├── tests/
│   └── ...                       ← tests unitarios/integración
│
└── fuzz/                         ← TODO lo de fuzzing
    ├── Cargo.toml                ← workspace del fuzzer
    ├── Cargo.lock                ← lock independiente
    │
    ├── fuzz_targets/             ← tus harnesses
    │   ├── fuzz_parse.rs         ← target 1
    │   ├── fuzz_roundtrip.rs     ← target 2
    │   └── fuzz_validate.rs      ← target 3
    │
    ├── corpus/                   ← semillas + inputs descubiertos
    │   ├── fuzz_parse/           ← un directorio por target
    │   │   ├── seed1             ← archivo binario (semilla manual)
    │   │   ├── seed2
    │   │   └── <sha1_hash>       ← inputs descubiertos por el fuzzer
    │   ├── fuzz_roundtrip/
    │   └── fuzz_validate/
    │
    ├── artifacts/                ← crashes, timeouts, OOM
    │   ├── fuzz_parse/
    │   │   ├── crash-<hash>      ← input que causa crash
    │   │   ├── timeout-<hash>    ← input que causa timeout
    │   │   └── oom-<hash>        ← input que causa OOM
    │   ├── fuzz_roundtrip/
    │   └── fuzz_validate/
    │
    └── target/                   ← binarios compilados (gitignore)
        └── x86_64-unknown-linux-gnu/
            └── release/
                ├── fuzz_parse    ← binario ejecutable del target
                ├── fuzz_roundtrip
                └── fuzz_validate
```

### Qué va en .gitignore

El archivo `fuzz/.gitignore` generado por cargo-fuzz ya excluye `target/`. Pero hay decisiones adicionales:

```
# fuzz/.gitignore (recomendado)
target/

# Decisiones del equipo:

# ¿Commitear el corpus?
# SÍ para semillas iniciales manuales
# DEPENDE para corpus descubierto (puede ser grande)

# ¿Commitear los artifacts?
# SÍ para crashes convertidos en regression tests
# NO para artifacts sin procesar (binarios grandes)
```

---

## 9. El archivo Cargo.toml del directorio fuzz/

El `fuzz/Cargo.toml` es el corazón de la configuración del fuzzer. Examinémoslo en detalle:

```toml
# fuzz/Cargo.toml (generado por cargo fuzz init)

[package]
name = "mi_proyecto-fuzz"
version = "0.0.0"
publish = false
edition = "2021"

[package.metadata]
cargo-fuzz = true          # Marca este crate como fuzzer

[dependencies]
libfuzzer-sys = "0.11"     # Bindings a libFuzzer
mi_proyecto = { path = ".." }  # Referencia al proyecto principal

# Perfiles de compilación optimizados para fuzzing

[profile.release]
opt-level = 3              # Optimización máxima para velocidad
debug = true               # Mantener debug info para stack traces legibles
debug-assertions = true    # ¡IMPORTANTE! Mantener assertions activas
overflow-checks = true     # ¡IMPORTANTE! Detectar integer overflow

# Cada target es un binario separado
[[bin]]
name = "fuzz_target_1"
path = "fuzz_targets/fuzz_target_1.rs"
doc = false
```

### Campos críticos explicados

#### `debug-assertions = true` en release

En un build normal de Rust, `cargo build --release` desactiva `debug_assert!()` y los checks de integer overflow. Pero en fuzzing **queremos** que estos checks estén activos porque son bugs reales que queremos encontrar:

```rust
// Con debug-assertions = true (fuzzing), esto PANICEA:
let x: u8 = 200;
let y: u8 = 100;
let z = x + y;  // panic: overflow en debug mode

// Con debug-assertions = false (release normal), esto WRAPPEA silenciosamente:
// z = 44 (200 + 100 = 300, 300 % 256 = 44)
```

#### `overflow-checks = true`

Similar a debug-assertions pero específico para aritmética. Asegura que `+`, `-`, `*` en tipos enteros detecten overflow:

```rust
// Con overflow-checks = true:
let n: i32 = i32::MAX;
let m = n + 1;  // panic: "attempt to add with overflow"

// Sin overflow-checks:
// m = i32::MIN (-2147483648) — wrapping silencioso
```

#### `opt-level = 3`

Máxima optimización. Contraproducente en testing normal, pero en fuzzing la velocidad de ejecución es crítica:

```
opt-level = 0: ~500 exec/sec (demasiado lento para fuzzing)
opt-level = 1: ~2000 exec/sec
opt-level = 2: ~5000 exec/sec
opt-level = 3: ~8000 exec/sec ← la velocidad importa
```

#### `debug = true` en release

Mantiene la información de debug (DWARF) incluso en release. Esto permite que los stack traces de los crashes sean legibles:

```
# Con debug = true:
thread 'main' panicked at 'index out of bounds: the len is 3 but the index is 5'
   → src/parser.rs:42:15   ← ÚTIL: sabes exactamente dónde

# Sin debug = true:
thread 'main' panicked at 'index out of bounds: the len is 3 but the index is 5'
   → 0x55a3b2c4d5e6       ← INÚTIL: solo una dirección de memoria
```

---

## 10. fuzz_target!: la macro fundamental

`fuzz_target!` es la macro que define el punto de entrada del fuzzer. Es el equivalente en Rust de `LLVMFuzzerTestOneInput` en C.

### Forma básica: bytes crudos

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    // 'data' contiene los bytes que el fuzzer genera
    // Tu código aquí: procesar data con la función que quieres fuzzear
});
```

### Forma con tipo Arbitrary

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
struct MyInput {
    name: String,
    count: u32,
    flag: bool,
}

fuzz_target!(|input: MyInput| {
    // 'input' es un MyInput generado automáticamente
    // El fuzzer convierte bytes → MyInput via Arbitrary
});
```

(El trait `Arbitrary` lo veremos en profundidad en T02.)

### Forma con &str

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &str| {
    // 'data' es un string UTF-8 válido
    // El fuzzer SOLO genera inputs que sean UTF-8 válidos
    // Inputs no-UTF-8 se descartan silenciosamente
});
```

### Atributo `#![no_main]`

```rust
#![no_main]  // ← OBLIGATORIO
```

Este atributo es necesario porque `libfuzzer-sys` proporciona su propia función `main()`. Si tu código también tuviera `main()`, habría un conflicto de símbolos. `#![no_main]` le dice a rustc que no busque una función `main()` en este crate.

Es exactamente análogo a cuando en C con libFuzzer **no** escribes `main()` porque libFuzzer proporciona la suya.

---

## 11. Expansión de la macro fuzz_target!

Para entender qué hace `fuzz_target!` internamente, veamos su expansión simplificada:

```rust
// Lo que tú escribes:
fuzz_target!(|data: &[u8]| {
    my_parser::parse(data);
});

// Lo que la macro genera (simplificado):
#[export_name = "rust_fuzzer_test_input"]
fn __fuzz_test_input(data: &[u8]) -> i32 {
    // Instala un panic handler que captura panics
    // en vez de abortar el proceso
    std::panic::catch_unwind(|| {
        my_parser::parse(data);
    })
    .err()
    .map(|err| {
        // Si hay panic, reportar como crash
        eprintln!("PANIC: {:?}", err);
    });
    0
}

// libfuzzer-sys también genera:
#[no_mangle]
pub extern "C" fn LLVMFuzzerTestOneInput(data: *const u8, size: usize) -> i32 {
    let slice = unsafe { std::slice::from_raw_parts(data, size) };
    rust_fuzzer_test_input(slice)
}
```

### Flujo de ejecución detallado

```
libFuzzer main()
     │
     ▼
LLVMFuzzerTestOneInput(data, size)     ← generada por libfuzzer-sys
     │
     ├── Convertir *const u8 → &[u8]   ← unsafe, una sola vez
     │
     ▼
rust_fuzzer_test_input(data: &[u8])    ← generada por fuzz_target!
     │
     ├── std::panic::catch_unwind(|| {
     │       TU CLOSURE(data)           ← tu código
     │   })
     │
     ├── Si Ok(()) → retornar 0 al fuzzer (todo bien, seguir mutando)
     │
     └── Si Err(panic) →
             ├── Imprimir info del panic
             ├── Guardar input como artifact (crash-<hash>)
             └── Retornar al fuzzer que pare
```

### Implicación clave: catch_unwind

`fuzz_target!` usa `catch_unwind`, lo que significa que:

1. **Panics no terminan el proceso** inmediatamente — el fuzzer los captura y registra el input causante.

2. **Tu código debe ser `UnwindSafe`** — en la práctica, esto casi siempre se cumple. La excepción es si tienes `&mut` a datos compartidos que podrían quedar en estado inconsistente después de un panic.

3. **`std::process::abort()` SÍ termina** el proceso — si tu código llama a `abort()` explícitamente o un panic con `panic = "abort"` configurado, el fuzzer no puede capturarlo como crash normal.

```rust
// PRECAUCIÓN con panic = "abort"
// NO pongas esto en fuzz/Cargo.toml:
// [profile.release]
// panic = "abort"   ← ¡NUNCA! Rompe la captura de panics
```

---

## 12. Contrato del closure dentro de fuzz_target!

El closure que escribes dentro de `fuzz_target!` debe cumplir ciertas reglas para que el fuzzing sea efectivo:

### Regla 1: no crashear por diseño

```rust
// ❌ MAL: crashea intencionalmente con inputs vacíos
fuzz_target!(|data: &[u8]| {
    let first = data[0];  // panic si data está vacío
    // ...
});

// ✓ BIEN: maneja input vacío graciosamente
fuzz_target!(|data: &[u8]| {
    if data.is_empty() {
        return;
    }
    // o mejor:
    let first = match data.first() {
        Some(&b) => b,
        None => return,
    };
    // ...
});
```

### Regla 2: ser determinista

```rust
// ❌ MAL: resultado depende del tiempo
fuzz_target!(|data: &[u8]| {
    let now = std::time::SystemTime::now();
    if now.elapsed().unwrap().as_nanos() % 2 == 0 {
        panic!("random crash");
    }
});

// ✓ BIEN: resultado depende solo del input
fuzz_target!(|data: &[u8]| {
    let result = my_lib::process(data);
    assert!(result.is_valid());
});
```

### Regla 3: no tener efectos secundarios persistentes

```rust
// ❌ MAL: escribe archivos al disco
fuzz_target!(|data: &[u8]| {
    std::fs::write("/tmp/fuzz_input.bin", data).unwrap();
    let result = my_lib::process_file("/tmp/fuzz_input.bin");
    // Lento por I/O + posible race condition
});

// ✓ BIEN: opera en memoria
fuzz_target!(|data: &[u8]| {
    let result = my_lib::process_bytes(data);
});
```

### Regla 4: ser rápido

```rust
// ❌ MAL: duerme (el fuzzer piensa que es un timeout)
fuzz_target!(|data: &[u8]| {
    std::thread::sleep(std::time::Duration::from_millis(100));
    my_lib::process(data);
});

// ❌ MAL: hace I/O de red
fuzz_target!(|data: &[u8]| {
    let resp = reqwest::blocking::get("http://example.com").unwrap();
    // ...
});

// ✓ BIEN: solo CPU + memoria
fuzz_target!(|data: &[u8]| {
    my_lib::process(data);
});
```

### Regla 5: no leak memoria indefinidamente

```rust
// ❌ MAL: acumula en un Vec global (leak creciente)
use std::sync::Mutex;
static HISTORY: Mutex<Vec<Vec<u8>>> = Mutex::new(Vec::new());

fuzz_target!(|data: &[u8]| {
    HISTORY.lock().unwrap().push(data.to_vec());
    // Después de millones de iteraciones: OOM
});

// ✓ BIEN: todo es local al closure
fuzz_target!(|data: &[u8]| {
    let local_copy = data.to_vec();
    my_lib::process(&local_copy);
    // local_copy se libera al salir del closure
});
```

### Regla 6: retornar () (no Result, no bool)

```rust
// ❌ MAL: intenta retornar Result
fuzz_target!(|data: &[u8]| -> Result<(), Error> {  // Error de compilación
    let result = my_lib::parse(data)?;
    Ok(())
});

// ✓ BIEN: manejar errores internamente
fuzz_target!(|data: &[u8]| {
    // Opción 1: ignorar errores (solo buscamos panics)
    let _ = my_lib::parse(data);

    // Opción 2: verificar propiedades del resultado
    match my_lib::parse(data) {
        Ok(parsed) => {
            // Verificar invariantes
            assert!(parsed.is_consistent());
        }
        Err(_) => {
            // Error de parsing es aceptable, no es un bug
        }
    }
});
```

### Tabla resumen del contrato

| Regla | Qué hacer | Qué NO hacer |
|---|---|---|
| Input vacío | `if data.is_empty() { return; }` | `data[0]` sin check |
| Determinismo | Solo depender de `data` | Usar tiempo, random, I/O |
| Efectos | Operar en memoria | Archivos, red, stdout |
| Velocidad | < 1 ms por ejecución | Sleep, I/O, allocaciones masivas |
| Memoria | Variables locales | Globals crecientes |
| Retorno | `()` (implícito) | `Result<>`, `bool` |
| Errores | `let _ = f(data)` o `match` | `unwrap()` en Result de parsing |

---

## 13. Patrones de harness en Rust con fuzz_target!

### Patrón 1: bytes crudos (parsers binarios)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::decode_image;

fuzz_target!(|data: &[u8]| {
    // Solo verificar que no panicea con input arbitrario
    let _ = decode_image(data);
});
```

**Cuándo usar**: parsers que aceptan `&[u8]` directamente (formatos binarios, protocolos, compresión).

### Patrón 2: string UTF-8 (parsers de texto)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::parse_json;

fuzz_target!(|data: &str| {
    // El fuzzer solo genera strings UTF-8 válidos
    let _ = parse_json(data);
});
```

**Cuándo usar**: parsers que solo aceptan texto UTF-8 válido (JSON, TOML, Markdown, SQL).

**Nota**: usar `&str` en vez de `&[u8]` reduce el espacio de búsqueda — el fuzzer nunca probará secuencias de bytes no-UTF-8. Esto es bueno si tu parser rechaza bytes no-UTF-8 de todas formas, pero malo si quieres verificar que el rechazo es correcto.

Para probar el manejo de UTF-8 inválido:

```rust
fuzz_target!(|data: &[u8]| {
    // Probar tanto UTF-8 válido como inválido
    match std::str::from_utf8(data) {
        Ok(s) => {
            let _ = parse_json(s);
        }
        Err(_) => {
            // Verificar que el parser rechaza input no-UTF-8 sin crashear
            // (si acepta &[u8])
        }
    }
});
```

### Patrón 3: roundtrip (encode/decode)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::{encode, decode};

fuzz_target!(|data: &[u8]| {
    // Encode los datos
    let encoded = encode(data);

    // Decode debe retornar el original
    match decode(&encoded) {
        Ok(decoded) => {
            assert_eq!(
                data, &decoded[..],
                "Roundtrip failed: encode(decode(data)) != data"
            );
        }
        Err(e) => {
            panic!("decode failed on valid encoded data: {}", e);
        }
    }
});
```

**Cuándo usar**: cualquier par de funciones inversas (compress/decompress, serialize/deserialize, encode/decode, parse/format).

### Patrón 4: comparación diferencial

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::my_sort;

fuzz_target!(|data: &[u8]| {
    let mut mine = data.to_vec();
    let mut reference = data.to_vec();

    my_sort(&mut mine);
    reference.sort();  // implementación de referencia de la stdlib

    assert_eq!(mine, reference, "sort mismatch");
});
```

**Cuándo usar**: cuando tienes una implementación de referencia (correcta pero lenta) y tu implementación (rápida pero posiblemente incorrecta).

### Patrón 5: stateful con operaciones

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::MyMap;

fuzz_target!(|data: &[u8]| {
    let mut map = MyMap::new();

    for chunk in data.chunks(3) {
        if chunk.len() < 2 { continue; }
        let op = chunk[0];
        let key = chunk[1];

        match op % 4 {
            0 => { map.insert(key, key.wrapping_mul(2)); }
            1 => { let _ = map.get(&key); }
            2 => { map.remove(&key); }
            3 => {
                // Invariante: después de insert(k, v), get(k) == Some(v)
                map.insert(key, 42);
                assert_eq!(map.get(&key), Some(&42));
            }
            _ => unreachable!(),
        }
    }
});
```

**Cuándo usar**: estructuras de datos con múltiples operaciones y estado mutable.

---

## 14. cargo fuzz add: agregar más targets

Un proyecto real generalmente tiene múltiples targets de fuzzing. `cargo fuzz add` crea un nuevo target:

### Sintaxis

```bash
cargo fuzz add <NAME>
```

### Qué hace

```bash
$ cargo fuzz add fuzz_roundtrip

# 1. Crea fuzz/fuzz_targets/fuzz_roundtrip.rs con el template por defecto
# 2. Agrega una sección [[bin]] en fuzz/Cargo.toml
```

### El archivo generado

```rust
// fuzz/fuzz_targets/fuzz_roundtrip.rs (generado)
#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    // fuzzed code goes here
});
```

### El Cargo.toml actualizado

```toml
# Después de cargo fuzz add fuzz_roundtrip, se agrega:
[[bin]]
name = "fuzz_roundtrip"
path = "fuzz_targets/fuzz_roundtrip.rs"
doc = false
```

### Ejemplo: agregar múltiples targets

```bash
cargo fuzz add fuzz_parse
cargo fuzz add fuzz_roundtrip
cargo fuzz add fuzz_validate
cargo fuzz add fuzz_differential

# Ver todos los targets
cargo fuzz list
# fuzz_parse
# fuzz_roundtrip
# fuzz_validate
# fuzz_differential
```

### Alternativa: crear targets manualmente

No necesitas usar `cargo fuzz add`. Puedes crear el archivo `.rs` y agregar la sección `[[bin]]` a mano. `cargo fuzz add` es solo una conveniencia.

```bash
# Manual:
cat > fuzz/fuzz_targets/fuzz_custom.rs << 'EOF'
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::custom_parse;

fuzz_target!(|data: &[u8]| {
    let _ = custom_parse(data);
});
EOF

# Agregar al Cargo.toml manualmente:
cat >> fuzz/Cargo.toml << 'EOF'

[[bin]]
name = "fuzz_custom"
path = "fuzz_targets/fuzz_custom.rs"
doc = false
EOF
```

---

## 15. cargo fuzz run: ejecutar el fuzzer

`cargo fuzz run` es el comando principal. Compila el target y ejecuta libFuzzer.

### Sintaxis básica

```bash
cargo fuzz run <TARGET> [OPTIONS] [-- <LIBFUZZER_ARGS>]
```

La parte antes de `--` son opciones de cargo-fuzz. La parte después de `--` son flags pasadas directamente a libFuzzer.

### Ejemplos de uso

```bash
# Ejecutar indefinidamente
cargo fuzz run fuzz_parse

# Ejecutar por 60 segundos
cargo fuzz run fuzz_parse -- -max_total_time=60

# Ejecutar con corpus existente
cargo fuzz run fuzz_parse fuzz/corpus/fuzz_parse

# Ejecutar con corpus + directorio de semillas
cargo fuzz run fuzz_parse fuzz/corpus/fuzz_parse seeds/

# Ejecutar con diccionario
cargo fuzz run fuzz_parse -- -dict=fuzz/dicts/json.dict

# Ejecutar con múltiples workers
cargo fuzz run fuzz_parse -- -jobs=4 -workers=4

# Ejecutar solo los inputs del corpus (regression test)
cargo fuzz run fuzz_parse -- -runs=0

# Ejecutar con ASan (ya está habilitado por defecto)
cargo fuzz run fuzz_parse --sanitizer=address

# Ejecutar sin ASan (más rápido, menos detección)
cargo fuzz run fuzz_parse --sanitizer=none
```

### El flujo interno de cargo fuzz run

```
cargo fuzz run fuzz_parse
       │
       ▼
┌──────────────────────────────────────────────────────┐
│ 1. Verificar que existe fuzz/Cargo.toml              │
│ 2. Verificar que el target existe                    │
│ 3. Configurar RUSTFLAGS:                             │
│    -Cpasses=sancov-module                            │
│    -Cllvm-args=-sanitizer-coverage-level=4           │
│    -Cllvm-args=-sanitizer-coverage-inline-8bit-...   │
│    -Cllvm-args=-sanitizer-coverage-pc-table          │
│    -Cllvm-args=-sanitizer-coverage-trace-compares    │
│ 4. Si --sanitizer=address:                           │
│    Agregar -Zsanitizer=address a RUSTFLAGS            │
│ 5. Ejecutar: cargo +nightly build --release          │
│    --manifest-path fuzz/Cargo.toml                   │
│    --bin fuzz_parse                                  │
│ 6. Crear directorio corpus si no existe:             │
│    fuzz/corpus/fuzz_parse/                           │
│ 7. Ejecutar el binario compilado:                    │
│    ./target/.../fuzz_parse fuzz/corpus/fuzz_parse    │
│    + flags de libFuzzer                              │
└──────────────────────────────────────────────────────┘
```

---

## 16. Flags y opciones de cargo fuzz run

### Opciones de cargo-fuzz (antes de --)

| Flag | Default | Descripción |
|---|---|---|
| `--sanitizer <SAN>` | `address` | Sanitizer: `address`, `memory`, `thread`, `none` |
| `--release` | activado | Compilar en modo release (ya es el default) |
| `--debug-assertions` | activado | Mantener debug assertions |
| `--dev` | desactivado | Compilar en modo dev (lento, para debugging) |
| `-j <N>` | CPUs | Threads de compilación |
| `--no-default-features` | — | Desactivar features del crate |
| `--features <F>` | — | Activar features específicas |
| `--target <TRIPLE>` | host | Target triple de compilación |

### Flags de libFuzzer (después de --)

Estos son los mismos flags que aprendimos en S01/T03 (libFuzzer en C):

| Flag | Default | Descripción |
|---|---|---|
| `-max_total_time=N` | 0 (infinito) | Duración máxima en segundos |
| `-max_len=N` | 4096 | Tamaño máximo del input en bytes |
| `-runs=N` | 0 (infinito) | Número máximo de ejecuciones. `-runs=0` es modo regression |
| `-seed=N` | 0 (aleatorio) | Semilla para reproducibilidad |
| `-dict=FILE` | — | Ruta al diccionario |
| `-jobs=N` | 1 | Número de trabajos paralelos |
| `-workers=N` | min(jobs, CPUs/2) | Workers concurrentes |
| `-timeout=N` | 1200 | Timeout por input en segundos |
| `-rss_limit_mb=N` | 2048 | Límite de memoria RSS en MB |
| `-use_value_profile=1` | 0 | Habilitar guía por comparaciones |
| `-only_ascii=1` | 0 | Solo generar inputs ASCII |
| `-artifact_prefix=DIR` | `fuzz/artifacts/TARGET/` | Dónde guardar crashes |
| `-print_final_stats=1` | 0 | Imprimir estadísticas al finalizar |
| `-fork=N` | 0 | Modo fork con N procesos (aislamiento de crashes) |
| `-merge=1` | 0 | Modo merge de corpus |
| `-minimize_crash=1` | 0 | Minimizar un crash específico |
| `-detect_leaks=1` | 1 (si ASan) | Detectar memory leaks |

### Combinaciones comunes

```bash
# Fuzzing rápido inicial: 5 minutos, sin value profile
cargo fuzz run fuzz_parse -- -max_total_time=300

# Fuzzing profundo: value profile + max_len grande
cargo fuzz run fuzz_parse -- -max_total_time=3600 \
    -use_value_profile=1 -max_len=65536

# Fuzzing de texto: solo ASCII, con diccionario
cargo fuzz run fuzz_parse -- -only_ascii=1 \
    -dict=fuzz/dicts/json.dict

# Fuzzing paralelo: 4 workers
cargo fuzz run fuzz_parse -- -jobs=4 -workers=4

# CI: timeout corto, imprimir stats
cargo fuzz run fuzz_parse -- -max_total_time=60 \
    -print_final_stats=1

# Debugging: compilar en dev, sin ASan
cargo fuzz run fuzz_parse --dev --sanitizer=none -- \
    -max_total_time=30
```

---

## 17. Interpretar la salida del fuzzer

Al ejecutar `cargo fuzz run`, verás la salida de libFuzzer. Es la misma que vimos en S01/T03 pero en el contexto de Rust:

### Salida típica

```
   Compiling my_lib v0.1.0 (/home/user/my_project)
   Compiling my_lib-fuzz v0.0.0 (/home/user/my_project/fuzz)
    Finished `release` profile [optimized + debuginfo] target(s) in 12.34s
     Running `fuzz/target/x86_64-unknown-linux-gnu/release/fuzz_parse ...`

INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 3847562910
INFO: Loaded 1 modules   (1234 inline 8-bit counters): 1234 [0x..., 0x...)
INFO: Loaded 1 PC tables (1234 PCs):    1234 [0x..., 0x...)
INFO:       42 files found in fuzz/corpus/fuzz_parse
INFO: seed corpus: files: 42 min: 1b max: 256b total: 4096b rss: 30Mb
#64     INITED cov: 145 ft: 210 corp: 42/4096b exec/s: 0 rss: 35Mb
#128    NEW    cov: 148 ft: 215 corp: 43/4200b lim: 256 exec/s: 12800 rss: 35Mb L: 12/256 MS: 4 ...
#256    NEW    cov: 153 ft: 222 corp: 44/4350b lim: 256 exec/s: 12800 rss: 36Mb L: 45/256 MS: 2 ...
#512    REDUCE cov: 153 ft: 222 corp: 44/4100b lim: 256 exec/s: 12800 rss: 36Mb L: 8/256 MS: 1 ...
#1024   pulse  cov: 153 ft: 222 corp: 44/4100b exec/s: 10240 rss: 36Mb
#2048   NEW    cov: 158 ft: 230 corp: 45/4300b lim: 512 exec/s: 10240 rss: 37Mb L: 200/512 MS: 3 ...
```

### Desglose de cada campo

```
#2048   NEW    cov: 158 ft: 230 corp: 45/4300b lim: 512 exec/s: 10240 rss: 37Mb L: 200/512 MS: 3
│       │      │        │       │             │         │          │         │          │
│       │      │        │       │             │         │          │         │          └─ Mutaciones
│       │      │        │       │             │         │          │         └─ RSS actual
│       │      │        │       │             │         │          └─ Ejecuciones/segundo
│       │      │        │       │             │         └─ Límite actual de max_len
│       │      │        │       │             └─ Tamaño total del corpus
│       │      │        │       └─ Archivos en el corpus
│       │      │        └─ Features (edges + hit counts)
│       │      └─ Cobertura (edges únicos)
│       └─ Tipo de evento
└─ Número de ejecución
```

### Tipos de evento

| Evento | Significado |
|---|---|
| `INITED` | Corpus inicial cargado. cov/ft son la cobertura base |
| `NEW` | Input nuevo que aumentó la cobertura. Se guarda en el corpus |
| `REDUCE` | Input existente reducido manteniendo la misma cobertura |
| `pulse` | Heartbeat periódico (cada 2^N ejecuciones). Muestra que sigue vivo |
| `DONE` | Fuzzing terminado (por max_total_time o max_runs) |
| `BINGO` | ¡Crash encontrado! |

### Crash (panic en Rust)

```
INFO: #8192   NEW    cov: 200 ft: 310 corp: 60/8000b ...
thread '<unnamed>' panicked at 'index out of bounds: the len is 0 but the index is 0',
    src/parser.rs:42:15
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

==12345== ERROR: libFuzzer: deadly signal
    #0 0x55a3... in __sanitizer_print_stack_trace ...
    #1 0x55a3... in fuzzer::Fuzzer::ExitCallback() ...
    ...

artifact_prefix='fuzz/artifacts/fuzz_parse/';
Test unit written to fuzz/artifacts/fuzz_parse/crash-abc123def456...

Base64: AQIDBA==
```

### Interpretación del crash

```
1. "thread panicked at 'index out of bounds'"
   → El panic message te dice QUÉ falló

2. "src/parser.rs:42:15"
   → Ubicación exacta del panic (gracias a debug = true)

3. "Test unit written to fuzz/artifacts/fuzz_parse/crash-abc123..."
   → El input que causa el crash fue guardado

4. "Base64: AQIDBA=="
   → El input codificado en base64 (útil para copy-paste)
```

### Velocidad de ejecución: qué esperar

| exec/s | Calificación | Típico en |
|---|---|---|
| < 100 | Muy lento | I/O, red, allocaciones masivas |
| 100–1000 | Lento | Parsing complejo, grandes inputs |
| 1000–5000 | Normal | Parser típico |
| 5000–20000 | Bueno | Funciones simples, inputs pequeños |
| > 20000 | Excelente | Funciones triviales |

---

## 18. Corpus: concepto y gestión en cargo-fuzz

### Repaso rápido del concepto (visto en S01/T01)

El corpus es la colección de inputs que el fuzzer usa como base para generar nuevos inputs. En coverage-guided fuzzing:

```
Corpus = semillas_manuales ∪ inputs_descubiertos_por_el_fuzzer

Algoritmo:
1. Seleccionar un input del corpus
2. Mutarlo
3. Ejecutar con el input mutado
4. Si la cobertura aumenta → agregar al corpus
5. Repetir
```

### Ubicación del corpus en cargo-fuzz

```
fuzz/corpus/<TARGET_NAME>/
```

cargo-fuzz crea este directorio automáticamente al ejecutar `cargo fuzz run`. Cada target tiene su propio directorio de corpus.

```bash
# El corpus se encuentra en:
ls fuzz/corpus/fuzz_parse/
# seed_get_request
# seed_post_request
# 5d41402abc4b2a76b9719d911017c592   ← input descubierto (nombre = hash SHA1)
# a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6
# ...
```

### Nomenclatura de archivos del corpus

```
Semillas manuales:  cualquier nombre que tú elijas
                    ej: seed_valid_json, seed_empty, seed_unicode

Inputs del fuzzer:  hash SHA1 del contenido
                    ej: 5d41402abc4b2a76b9719d911017c592
                    (el nombre es determinista: mismo contenido → mismo nombre)
```

### Persistencia del corpus entre sesiones

El corpus **persiste entre sesiones de fuzzing**. Cada vez que ejecutas `cargo fuzz run`, el fuzzer:

1. **Carga** todos los archivos del directorio de corpus
2. **Continúa** mutando desde donde quedó
3. **Agrega** nuevos inputs al mismo directorio

```bash
# Sesión 1: 5 minutos
cargo fuzz run fuzz_parse -- -max_total_time=300
# Después: 200 archivos en fuzz/corpus/fuzz_parse/

# Sesión 2: 5 minutos más (continúa desde el corpus existente)
cargo fuzz run fuzz_parse -- -max_total_time=300
# Después: 350 archivos en fuzz/corpus/fuzz_parse/

# Sesión 3: el corpus ya tiene buena cobertura → progreso más lento
cargo fuzz run fuzz_parse -- -max_total_time=300
# Después: 380 archivos
```

---

## 19. Crear corpus inicial (semillas)

Las semillas (seeds) son inputs manuales que proporcionas para dar al fuzzer un punto de partida mejor que inputs aleatorios.

### Reglas para buenas semillas

```
┌─────────────────────────────────────────────────────────────┐
│                 5 reglas para buenas semillas                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. PEQUEÑAS: < 1 KB idealmente. 100 bytes es perfecto      │
│     El fuzzer puede agrandarlas, pero no achicuarlas bien    │
│                                                             │
│  2. VÁLIDAS: que representen inputs reales que tu código     │
│     procesaría normalmente. Esto da cobertura inicial        │
│                                                             │
│  3. DIVERSAS: cubrir diferentes caminos del código            │
│     Un JSON con string, con number, con array, con null      │
│                                                             │
│  4. MÍNIMAS: no tener 20 semillas que cubren lo mismo        │
│     Mejor 5 semillas que cubren 5 caminos diferentes         │
│                                                             │
│  5. SIN DUPLICADOS: no tener semillas idénticas              │
│     El fuzzer las deduplicará, pero es desperdicio           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Crear semillas manualmente

```bash
# Crear directorio de corpus
mkdir -p fuzz/corpus/fuzz_parse

# Semillas como archivos de texto
echo -n '{"key": "value"}' > fuzz/corpus/fuzz_parse/seed_object
echo -n '[1, 2, 3]' > fuzz/corpus/fuzz_parse/seed_array
echo -n '"hello"' > fuzz/corpus/fuzz_parse/seed_string
echo -n '42' > fuzz/corpus/fuzz_parse/seed_number
echo -n 'true' > fuzz/corpus/fuzz_parse/seed_bool
echo -n 'null' > fuzz/corpus/fuzz_parse/seed_null
echo -n '' > fuzz/corpus/fuzz_parse/seed_empty

# Semillas binarias (con printf para bytes no-ASCII)
printf '\x00\x01\x02\x03' > fuzz/corpus/fuzz_parse/seed_binary
printf '\xff\xfe\xfd' > fuzz/corpus/fuzz_parse/seed_high_bytes
```

### Semillas desde tests existentes

Si ya tienes tests unitarios con inputs de ejemplo, puedes extraerlos como semillas:

```rust
// En tests/parser_tests.rs, tienes:
#[test]
fn test_parse_csv() {
    let input = "name,age,city\nAlice,30,NYC\nBob,25,LA";
    assert!(parse_csv(input).is_ok());
}

// Convertir a semilla:
// echo -n 'name,age,city\nAlice,30,NYC\nBob,25,LA' > fuzz/corpus/fuzz_csv/seed_basic
```

Script para extraer semillas de archivos de test:

```bash
# Crear semillas desde archivos en tests/fixtures/
for f in tests/fixtures/*.json; do
    cp "$f" "fuzz/corpus/fuzz_parse/seed_$(basename $f .json)"
done
```

### Usar un directorio de semillas separado

Puedes tener semillas en un directorio separado del corpus. El primer argumento posicional es el corpus principal (lectura/escritura) y los adicionales son solo lectura:

```bash
# seeds/ es solo lectura, fuzz/corpus/fuzz_parse/ es lectura/escritura
cargo fuzz run fuzz_parse fuzz/corpus/fuzz_parse seeds/

# El fuzzer:
# 1. Lee de seeds/ (pero no escribe ahí)
# 2. Lee y escribe en fuzz/corpus/fuzz_parse/
# 3. Los inputs nuevos se guardan solo en fuzz/corpus/fuzz_parse/
```

---

## 20. Corpus merge: optimización periódica

Con el tiempo, el corpus acumula inputs redundantes. `cargo fuzz cmin` (corpus minimize) reduce el corpus manteniendo la misma cobertura.

### Sintaxis

```bash
cargo fuzz cmin <TARGET>
```

### Qué hace

```
Antes del merge:
  fuzz/corpus/fuzz_parse/: 500 archivos, 1.2 MB total
  Cobertura: 300 edges

Proceso:
  1. Ejecutar cada input del corpus
  2. Registrar la cobertura de cada uno
  3. Seleccionar el subconjunto mínimo que mantiene 300 edges
  4. Eliminar los inputs redundantes

Después del merge:
  fuzz/corpus/fuzz_parse/: 120 archivos, 200 KB total
  Cobertura: 300 edges (la misma)
```

### Merge manual con libFuzzer

`cargo fuzz cmin` es un wrapper alrededor de `-merge=1` de libFuzzer. También puedes usar merge directamente:

```bash
# Merge: combinar un corpus viejo con semillas nuevas
mkdir fuzz/corpus/fuzz_parse_new
cargo fuzz run fuzz_parse fuzz/corpus/fuzz_parse_new \
    fuzz/corpus/fuzz_parse seeds/ -- -merge=1

# El resultado minimizado queda en fuzz/corpus/fuzz_parse_new/
# Si está bien, reemplazar:
rm -rf fuzz/corpus/fuzz_parse
mv fuzz/corpus/fuzz_parse_new fuzz/corpus/fuzz_parse
```

### Cuándo hacer merge

```
┌─────────────────────────────────────────────────────────────┐
│                     Cuándo hacer merge                       │
├──────────────┬──────────────────────────────────────────────┤
│ Situación    │ Acción                                       │
├──────────────┼──────────────────────────────────────────────┤
│ Corpus >     │ Merge para reducir tiempo de carga           │
│ 10000 files  │                                              │
├──────────────┼──────────────────────────────────────────────┤
│ Después de   │ Merge para combinar descubrimientos          │
│ fuzzing      │                                              │
│ paralelo     │                                              │
├──────────────┼──────────────────────────────────────────────┤
│ Antes de     │ Merge para tener un corpus limpio en git     │
│ commit a git │                                              │
├──────────────┼──────────────────────────────────────────────┤
│ Cambio de    │ Merge para eliminar inputs que ya no         │
│ código       │ aportan cobertura al nuevo código            │
│ significativo│                                              │
└──────────────┴──────────────────────────────────────────────┘
```

---

## 21. Artifacts: crashes, timeouts y OOM

### Qué son los artifacts

Los artifacts son inputs que causan comportamiento anómalo. cargo-fuzz los guarda en `fuzz/artifacts/<TARGET>/` automáticamente.

### Tipos de artifacts

```
┌───────────────────────────────────────────────────────────────────┐
│                        Tipos de artifacts                         │
├───────────────┬───────────────────────────────────────────────────┤
│ Tipo          │ Causa                                             │
├───────────────┼───────────────────────────────────────────────────┤
│ crash-<hash>  │ Panic en Rust safe:                               │
│               │   - unwrap() en None/Err                          │
│               │   - index out of bounds                           │
│               │   - integer overflow                              │
│               │   - assert! falló                                 │
│               │                                                   │
│               │ Signal en Rust unsafe:                             │
│               │   - SIGSEGV (segfault)                            │
│               │   - SIGABRT (abort)                                │
│               │   - SIGFPE (división por cero)                    │
├───────────────┼───────────────────────────────────────────────────┤
│ timeout-<hash>│ Ejecución excede el timeout (-timeout=N):         │
│               │   - Loop infinito                                 │
│               │   - Complejidad algorítmica O(n²) o peor          │
│               │   - Recursión profunda (sin stack overflow)       │
├───────────────┼───────────────────────────────────────────────────┤
│ oom-<hash>    │ Memoria excede el límite (-rss_limit_mb=N):       │
│               │   - Allocación basada en input no validado        │
│               │   - Vec::with_capacity(untrusted_size)            │
│               │   - Recursión que consume stack                   │
├───────────────┼───────────────────────────────────────────────────┤
│ leak-<hash>   │ Memory leak detectado por LSan                    │
│               │   (solo con -detect_leaks=1)                      │
│               │   Más común con FFI a código C                    │
└───────────────┴───────────────────────────────────────────────────┘
```

### Estructura de artifacts

```bash
$ ls -la fuzz/artifacts/fuzz_parse/
total 24
-rw-r--r-- 1 user user  12 Apr  5 10:30 crash-abc123def456789
-rw-r--r-- 1 user user   8 Apr  5 10:31 crash-fedcba987654321
-rw-r--r-- 1 user user 200 Apr  5 10:35 timeout-111222333444
-rw-r--r-- 1 user user  45 Apr  5 10:40 oom-aaa111bbb222

# Cada archivo contiene los bytes CRUDOS que causan el problema
# Tamaño = tamaño del input
```

### Reproducir un crash

```bash
# Método 1: cargo fuzz run con el archivo específico
cargo fuzz run fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123def456789

# Método 2: el binario directamente
./fuzz/target/x86_64-unknown-linux-gnu/release/fuzz_parse \
    fuzz/artifacts/fuzz_parse/crash-abc123def456789
```

### Ver el contenido de un artifact

```bash
# Como hexdump
xxd fuzz/artifacts/fuzz_parse/crash-abc123def456789
# 00000000: 7b22 6b65 7922 3a20 005d 7d              {"key": .]}.

# Como texto (si es legible)
cat fuzz/artifacts/fuzz_parse/crash-abc123def456789

# Como base64 (para compartir)
base64 fuzz/artifacts/fuzz_parse/crash-abc123def456789
```

---

## 22. Interpretar un crash artifact

### Crash típico en Rust safe (panic)

```
thread '<unnamed>' panicked at 'called `Option::unwrap()` on a `None` value',
    src/parser.rs:87:30
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

==54321== ERROR: libFuzzer: deadly signal
...
artifact_prefix='fuzz/artifacts/fuzz_parse/';
Test unit written to fuzz/artifacts/fuzz_parse/crash-5d41402abc4b2a76b9719d911017c592
```

**Interpretación**:

```
1. Tipo: panic (unwrap en None)
2. Ubicación: src/parser.rs:87:30
3. Causa probable: algún input hace que una operación retorne None
   donde el código asume que siempre es Some

4. Pasos:
   a. Leer src/parser.rs:87
   b. Entender qué operación retorna None
   c. Examinar el input crash para entender por qué
   d. Corregir: usar match/if-let en vez de unwrap
```

### Obtener backtrace completo

```bash
RUST_BACKTRACE=1 cargo fuzz run fuzz_parse \
    fuzz/artifacts/fuzz_parse/crash-5d41402abc4b2a76b9719d911017c592
```

Salida:

```
thread '<unnamed>' panicked at 'called `Option::unwrap()` on a `None` value',
    src/parser.rs:87:30
stack backtrace:
   0: std::panicking::begin_panic_handler
   1: core::panicking::panic_fmt
   2: core::panicking::panic
   3: core::option::Option<T>::unwrap
             at /rustc/.../library/core/src/option.rs:XXX
   4: my_parser::Parser::parse_value       ← TU CÓDIGO
             at /home/user/my_project/src/parser.rs:87:30
   5: my_parser::Parser::parse_object      ← LLAMADA POR
             at /home/user/my_project/src/parser.rs:52:18
   6: my_parser::Parser::parse             ← LLAMADA POR
             at /home/user/my_project/src/parser.rs:15:9
   7: fuzz_parse::__fuzz_test_input        ← HARNESS
             at /home/user/my_project/fuzz/fuzz_targets/fuzz_parse.rs:6:5
```

**Leer el backtrace**: de abajo hacia arriba, las líneas con tu código fuente te muestran la cadena de llamadas que llegó al panic.

### Crash en Rust unsafe (ASan)

```
==54321==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000054
READ of size 1 at 0x602000000054 thread T0
    #0 0x55a3... in my_lib::unsafe_parse::h1234567890abcdef src/unsafe_parse.rs:23:15
    #1 0x55a3... in fuzz_parse::__fuzz_test_input fuzz/fuzz_targets/fuzz_parse.rs:6:5
    ...

0x602000000054 is located 0 bytes after 4-byte region [0x602000000050,0x602000000054)
allocated by thread T0 here:
    #0 0x55a3... in __rust_alloc ...
    #1 0x55a3... in my_lib::unsafe_parse::h1234567890abcdef src/unsafe_parse.rs:20:25
```

**Interpretación**: esto es idéntico a los reportes de ASan que vimos en S01 con C. Heap-buffer-overflow significa lectura fuera de un buffer allocado en el heap. La ubicación es `src/unsafe_parse.rs:23`.

### Severidad de bugs en Rust

```
┌──────────────────────────────────────────────────────────────┐
│              Severidad de bugs en Rust por tipo               │
├──────────────────┬──────────┬────────────────────────────────┤
│ Bug              │ Contexto │ Severidad                      │
├──────────────────┼──────────┼────────────────────────────────┤
│ unwrap() panic   │ safe     │ Media: DoS en servers          │
│ index OOB panic  │ safe     │ Media: DoS en servers          │
│ overflow panic   │ safe     │ Baja-Media: depende del uso    │
│ assert! panic    │ safe     │ Media: indica invariante roto  │
│ Lógica incorrecta│ safe     │ Alta: datos corruptos, silente │
│ Timeout          │ safe     │ Media: ReDoS, algorithmic DoS  │
│ OOM              │ safe     │ Media: resource exhaustion     │
├──────────────────┼──────────┼────────────────────────────────┤
│ Buffer overflow  │ unsafe   │ Crítica: posible RCE           │
│ Use-after-free   │ unsafe   │ Crítica: posible RCE           │
│ Double free      │ unsafe   │ Alta: heap corruption          │
│ Uninitialized    │ unsafe   │ Alta: info leak                │
│ Data race        │ unsafe   │ Alta: UB                       │
└──────────────────┴──────────┴────────────────────────────────┘
```

---

## 23. Minimizar artifacts

Un crash artifact puede contener bytes innecesarios. La minimización encuentra el input más pequeño que sigue causando el mismo crash.

### cargo fuzz tmin

```bash
cargo fuzz tmin <TARGET> <ARTIFACT_PATH>
```

### Ejemplo

```bash
# El crash original tiene 200 bytes
ls -la fuzz/artifacts/fuzz_parse/crash-abc123
# -rw-r--r-- 1 user user 200 ...

# Minimizar
cargo fuzz tmin fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123

# Salida:
# INFO: Running: fuzz/artifacts/fuzz_parse/crash-abc123
# ...
# INFO: CRASH_MIN: minimizing crash input: 'fuzz/artifacts/fuzz_parse/crash-abc123' (200 bytes)
# INFO: CRASH_MIN: minimizing crash input: 'fuzz/artifacts/fuzz_parse/crash-abc123' (150 bytes)
# INFO: CRASH_MIN: minimizing crash input: 'fuzz/artifacts/fuzz_parse/crash-abc123' (80 bytes)
# INFO: CRASH_MIN: minimizing crash input: 'fuzz/artifacts/fuzz_parse/crash-abc123' (12 bytes)
# ...
# artifact_prefix='fuzz/artifacts/fuzz_parse/';
# Test unit written to fuzz/artifacts/fuzz_parse/minimized-from-abc123

# El input minimizado tiene solo 12 bytes
ls -la fuzz/artifacts/fuzz_parse/minimized-from-abc123
# -rw-r--r-- 1 user user 12 ...

# Ver el contenido minimizado
xxd fuzz/artifacts/fuzz_parse/minimized-from-abc123
# 00000000: 7b22 223a 005d 7d0a 3a22 7d0a           {"":.]}.:"}.
```

### Algoritmo de minimización

```
Input original: 200 bytes
     │
     ▼
┌────────────────────────────────────────┐
│ Fase 1: Eliminación de bloques         │
│ Probar eliminar la primera mitad       │
│ Probar eliminar la segunda mitad       │
│ Probar eliminar cuartos                │
│ ... hasta bloques de 1 byte           │
│ Mantener solo las eliminaciones que    │
│ preservan el crash                     │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ Fase 2: Simplificación de bytes        │
│ Probar reemplazar cada byte por 0x00   │
│ Probar reemplazar cada byte por ' '    │
│ Probar decrementar cada byte           │
│ Mantener solo los reemplazos que       │
│ preservan el crash                     │
└──────────────┬─────────────────────────┘
               │
               ▼
Input minimizado: 12 bytes
```

### Por qué minimizar

```
Sin minimizar:
  crash input = 200 bytes de datos semi-aleatorios
  Difícil entender QUÉ parte del input causa el crash

Con minimizar:
  crash input = 12 bytes claros: {"":.]}.:"}.
  Obvio: JSON malformado con un ']' después de una key vacía
```

---

## 24. Regression testing con artifacts

Los artifacts minimizados deben convertirse en regression tests para evitar que el bug reaparezca.

### Método 1: cargo fuzz run con -runs=0

```bash
# Ejecutar TODOS los inputs del corpus y artifacts sin mutar
# Si alguno causa crash, falla
cargo fuzz run fuzz_parse -- -runs=0
```

Esto es un **regression test instantáneo**: verifica que ningún input conocido causa crash, pero no genera inputs nuevos.

### Método 2: test unitario con el contenido del crash

```rust
// tests/regression.rs
use my_lib::parse;

#[test]
fn regression_crash_empty_key() {
    // Input minimizado del crash-abc123 (encontrado por fuzzing 2026-04-05)
    let input = b"{\"\":.]}.:\"}.\n";
    // Debe retornar Err, no paniquear
    let result = parse(input);
    assert!(result.is_err());
}

#[test]
fn regression_crash_nested_overflow() {
    // Input minimizado del crash-def456
    let input = b"[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]";
    let result = parse(input);
    // Puede ser Ok o Err, pero no debe paniquear
    let _ = result;
}
```

### Método 3: directorio de regression tests

Puedes tener un directorio con todos los crashes minimizados y ejecutarlos como regression tests:

```bash
# Estructura:
fuzz/
├── regression/
│   ├── fuzz_parse/
│   │   ├── crash_empty_key       ← crash minimizado
│   │   ├── crash_nested_overflow
│   │   └── crash_unicode_boundary
│   └── fuzz_roundtrip/
│       └── crash_encoding_mismatch

# Ejecutar como regression test:
cargo fuzz run fuzz_parse fuzz/regression/fuzz_parse -- -runs=0
```

### Flujo completo: del crash al regression test

```
1. Fuzzer encuentra crash
   fuzz/artifacts/fuzz_parse/crash-abc123 (200 bytes)
        │
        ▼
2. Minimizar
   cargo fuzz tmin fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123
   → fuzz/artifacts/fuzz_parse/minimized-from-abc123 (12 bytes)
        │
        ▼
3. Analizar
   xxd fuzz/artifacts/fuzz_parse/minimized-from-abc123
   → Entender qué parte del input causa el crash
        │
        ▼
4. Corregir el bug
   Editar src/parser.rs
   → Manejar el caso que causaba panic
        │
        ▼
5. Verificar la corrección
   cargo fuzz run fuzz_parse fuzz/artifacts/fuzz_parse/minimized-from-abc123
   → Ya no crashea
        │
        ▼
6. Crear regression test
   Copiar minimized-from-abc123 → fuzz/regression/fuzz_parse/
   Y/o crear test unitario en tests/regression.rs
        │
        ▼
7. Agregar al CI
   cargo fuzz run fuzz_parse fuzz/regression/fuzz_parse -- -runs=0
   → Ejecutar en cada PR
```

---

## 25. cargo fuzz coverage: medir cobertura

`cargo fuzz coverage` genera datos de cobertura del fuzzing para visualizar qué código fue ejercitado.

### Sintaxis

```bash
cargo fuzz coverage <TARGET> [CORPUS_DIR]
```

### Flujo completo

```bash
# 1. Generar datos de cobertura
cargo fuzz coverage fuzz_parse

# Esto:
# a. Compila el target con instrumentación de cobertura (-Cinstrument-coverage)
# b. Ejecuta todos los inputs del corpus
# c. Genera archivos .profraw en fuzz/coverage/fuzz_parse/

# 2. Ver la ubicación del profdata
# cargo fuzz coverage imprime la ruta al archivo de cobertura

# 3. Convertir a HTML con llvm-cov (como vimos en C02/S05)
# Primero encontrar el binario del target:
FUZZ_BIN=$(find fuzz/target -name fuzz_parse -type f -executable | head -1)

# Generar reporte HTML
cargo cov -- show "$FUZZ_BIN" \
    --instr-profile=fuzz/coverage/fuzz_parse/coverage.profdata \
    --format=html \
    --output-dir=fuzz/coverage/fuzz_parse/report \
    --show-line-counts-or-regions \
    --Xdemangler=rustfilt

# 4. Abrir el reporte
# firefox fuzz/coverage/fuzz_parse/report/index.html
```

### Interpretar la cobertura del fuzzer

```
src/parser.rs:
   1|    100|pub fn parse(input: &[u8]) -> Result<Value, Error> {
   2|    100|    if input.is_empty() {
   3|     15|        return Err(Error::Empty);
   4|       |    }
   5|     85|    let mut parser = Parser::new(input);
   6|     85|    parser.parse_value()
   7|    100|}
   8|       |
   9|     85|fn parse_value(&mut self) -> Result<Value, Error> {
  10|     85|    match self.peek() {
  11|     40|        b'{' => self.parse_object(),   ← cobertura alta
  12|     30|        b'[' => self.parse_array(),    ← cobertura alta
  13|     10|        b'"' => self.parse_string(),   ← cobertura media
  14|      5|        b't' | b'f' => self.parse_bool(),
  15|      0|        b'n' => self.parse_null(),     ← ¡NO CUBIERTO!
  16|      0|        _ => Err(Error::Unexpected),   ← ¡NO CUBIERTO!
  17|     85|}
```

Si hay líneas no cubiertas después de un fuzzing largo, puede indicar:
- El fuzzer necesita más tiempo
- Necesitas mejores semillas que cubran esas ramas
- Necesitas un diccionario con tokens como `null`
- El código es dead code (no alcanzable)

---

## 26. cargo fuzz list y cargo fuzz fmt

### cargo fuzz list

Lista todos los targets de fuzzing definidos en el proyecto.

```bash
$ cargo fuzz list
fuzz_parse
fuzz_roundtrip
fuzz_validate
fuzz_differential
```

Útil cuando tienes muchos targets y no recuerdas los nombres.

### cargo fuzz fmt

Formatea un artifact crash en un formato legible.

```bash
cargo fuzz fmt <TARGET> <ARTIFACT>
```

Ejemplo:

```bash
$ cargo fuzz fmt fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123

# Output:
# Compiling fuzz target...
# Running artifact through target...
#
# Output of `std::fmt::Debug`:
#
# [123, 34, 34, 58, 0, 93, 125, 46, 58, 34, 125, 46]
#
# Output of `std::fmt::Display` (if implemented):
#
# {"":.]}.:"}.
```

`cargo fuzz fmt` es especialmente útil con el trait `Arbitrary` (T02): cuando tu target acepta un struct en vez de `&[u8]`, `fmt` muestra la deserialización del struct, no los bytes crudos.

```bash
# Con Arbitrary:
$ cargo fuzz fmt fuzz_config fuzz/artifacts/fuzz_config/crash-xyz

# Output:
# MyConfig {
#     name: "hello",
#     count: 4294967295,
#     flag: true,
# }
```

---

## 27. Diccionarios en cargo-fuzz

Los diccionarios proporcionan tokens que el fuzzer inserta en los inputs para acelerar la exploración de formatos estructurados.

### Formato del diccionario

Mismo formato que en S01/T01 (libFuzzer/AFL++):

```
# json.dict — tokens para parsers JSON

# Delimitadores
"{"
"}"
"["
"]"

# Separadores
":"
","

# Valores especiales
"true"
"false"
"null"

# Strings
"\""
"\\\""
"\\n"
"\\t"
"\\u0000"

# Números
"0"
"-1"
"1e10"
"1.5"
"-0"

# Whitespace
" "
"\n"
"\r\n"
"\t"
```

### Usar un diccionario

```bash
# Pasar como flag de libFuzzer
cargo fuzz run fuzz_parse -- -dict=fuzz/dicts/json.dict
```

### Organización recomendada

```
fuzz/
├── dicts/
│   ├── json.dict
│   ├── xml.dict
│   ├── csv.dict
│   └── http.dict
├── fuzz_targets/
│   ├── fuzz_json.rs
│   └── fuzz_xml.rs
└── ...
```

### Cuándo usar diccionarios

| Situación | Diccionario | Por qué |
|---|---|---|
| Parser JSON | Sí | Sin `{`, `}`, `:` el fuzzer tarda mucho en descubrirlos |
| Parser binario (protobuf) | Probablemente no | Los bytes son arbitrarios, no hay tokens de texto |
| Parser CSV | Sí | `,`, `"`, `\n` son críticos |
| Compresión | No | El input es binario arbitrario |
| Parser TOML | Sí | `[`, `]`, `=`, `#` son tokens de estructura |
| Encoder/decoder | Depende | Si el input es texto sí, si es binario no |

---

## 28. Configuración avanzada en Cargo.toml

### Dependencias adicionales comunes

```toml
# fuzz/Cargo.toml

[dependencies]
libfuzzer-sys = "0.11"
my_project = { path = ".." }

# Para inputs estructurados (T02):
arbitrary = { version = "1", features = ["derive"] }

# Para generar datos controlados:
# (no siempre necesario si usas Arbitrary)
```

### Activar features del crate principal

```toml
# Si tu crate tiene features:
[dependencies]
my_project = { path = "..", features = ["serde", "validation"] }

# O si necesitas features específicas para testing:
my_project = { path = "..", features = ["test-utils"] }
```

### Múltiples targets con distintas dependencias

Si diferentes targets necesitan diferentes features:

```toml
# No hay forma directa de tener features diferentes por [[bin]]
# La solución es usar features del crate fuzz:

[features]
default = []
with_serde = ["my_project/serde"]
with_validation = ["my_project/validation"]
```

```bash
# Ejecutar con features específicas
cargo fuzz run fuzz_serde --features with_serde
cargo fuzz run fuzz_validate --features with_validation
```

### Perfil personalizado

```toml
# Para debugging de crashes: desactivar optimización
[profile.dev]
opt-level = 0
debug = true
debug-assertions = true
overflow-checks = true

# Para fuzzing normal: máxima velocidad con assertions
[profile.release]
opt-level = 3
debug = true
debug-assertions = true
overflow-checks = true
# NUNCA poner panic = "abort" aquí
```

---

## 29. Múltiples targets: estrategia y organización

### Cuándo crear targets separados

```
┌──────────────────────────────────────────────────────────────┐
│             Estrategia de múltiples targets                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Un target por FUNCIÓN PÚBLICA de la API:                    │
│  ┌─────────────────────────────────────────────────────┐     │
│  │ pub fn parse(input: &[u8])    → fuzz_parse          │     │
│  │ pub fn validate(data: &Data)  → fuzz_validate       │     │
│  │ pub fn encode(data: &[u8])    → fuzz_encode         │     │
│  │ pub fn decode(data: &[u8])    → fuzz_decode         │     │
│  └─────────────────────────────────────────────────────┘     │
│                                                              │
│  Un target por PROPIEDAD a verificar:                        │
│  ┌─────────────────────────────────────────────────────┐     │
│  │ No crash                      → fuzz_nocrash        │     │
│  │ Roundtrip                     → fuzz_roundtrip      │     │
│  │ Invariantes                   → fuzz_invariants     │     │
│  │ Diferencial                   → fuzz_differential   │     │
│  └─────────────────────────────────────────────────────┘     │
│                                                              │
│  Convención de nombres:                                      │
│  fuzz_<módulo>_<qué_verifica>                                │
│  Ej: fuzz_parser_roundtrip, fuzz_encoder_nocrash             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo para una librería de JSON

```
fuzz/fuzz_targets/
├── fuzz_parse.rs              ← parse(&[u8]) → Result, sin crash
├── fuzz_parse_str.rs          ← parse(&str) → Result, sin crash (solo UTF-8)
├── fuzz_roundtrip.rs          ← parse → serialize → parse → compare
├── fuzz_value_access.rs       ← parse → acceder a campos con get()
├── fuzz_pretty_print.rs       ← parse → pretty_print → parse → compare
└── fuzz_differential_serde.rs ← comparar con serde_json
```

### Script para fuzzear todos los targets

```bash
#!/bin/bash
# fuzz_all.sh — fuzzear todos los targets en secuencia

DURATION=${1:-300}  # 5 minutos por target

for target in $(cargo fuzz list); do
    echo "=== Fuzzing $target for $DURATION seconds ==="
    cargo fuzz run "$target" -- -max_total_time=$DURATION
    echo ""
done

echo "=== Results ==="
for target in $(cargo fuzz list); do
    crashes=$(ls fuzz/artifacts/$target/crash-* 2>/dev/null | wc -l)
    corpus=$(ls fuzz/corpus/$target/ 2>/dev/null | wc -l)
    echo "$target: $crashes crashes, $corpus corpus files"
done
```

---

## 30. Flujo de trabajo completo: del init al fix

```
┌──────────────────────────────────────────────────────────────────┐
│                 Flujo completo de fuzzing en Rust                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. SETUP                                                        │
│     cargo fuzz init -t fuzz_parse                                │
│     Editar fuzz/fuzz_targets/fuzz_parse.rs                       │
│     Crear semillas en fuzz/corpus/fuzz_parse/                    │
│     Crear diccionario en fuzz/dicts/                             │
│                                                                  │
│  2. FUZZ                                                         │
│     cargo fuzz run fuzz_parse -- -dict=fuzz/dicts/my.dict        │
│     Dejar correr 10+ minutos (primera sesión)                    │
│     Monitorear exec/s y cobertura                                │
│                                                                  │
│  3. TRIAGE                                                       │
│     ls fuzz/artifacts/fuzz_parse/                                │
│     Para cada crash:                                             │
│       a. Reproducir:                                             │
│          cargo fuzz run fuzz_parse artifact_path                 │
│       b. Minimizar:                                              │
│          cargo fuzz tmin fuzz_parse artifact_path                │
│       c. Analizar:                                               │
│          xxd minimized_artifact                                  │
│          RUST_BACKTRACE=1 cargo fuzz run ...                     │
│                                                                  │
│  4. FIX                                                          │
│     Corregir el bug en src/                                      │
│     Verificar: cargo fuzz run fuzz_parse minimized_artifact      │
│     → ya no crashea                                              │
│                                                                  │
│  5. REGRESSION TEST                                              │
│     cp minimized_artifact fuzz/regression/fuzz_parse/            │
│     Y/o agregar test unitario en tests/regression.rs             │
│     cargo fuzz run fuzz_parse fuzz/regression/fuzz_parse/        │
│       -- -runs=0                                                 │
│                                                                  │
│  6. OPTIMIZAR                                                    │
│     cargo fuzz cmin fuzz_parse                                   │
│     cargo fuzz coverage fuzz_parse                               │
│     Agregar semillas para código no cubierto                     │
│     Agregar tokens al diccionario                                │
│                                                                  │
│  7. CI                                                           │
│     Regression tests: cargo fuzz run ... -- -runs=0              │
│     Fuzzing breve: cargo fuzz run ... -- -max_total_time=60      │
│     OSS-Fuzz para fuzzing continuo (T04)                         │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 31. Comparación con fuzzing en C

### Tabla detallada

| Aspecto | C (libFuzzer) | C (AFL++) | Rust (cargo-fuzz) |
|---|---|---|---|
| **Función entry** | `LLVMFuzzerTestOneInput` | `main` / `__AFL_LOOP` | `fuzz_target!` macro |
| **Input type** | `uint8_t*, size_t` | archivo / stdin | `&[u8]`, `&str`, o `Arbitrary` |
| **Null terminator** | Manual (malloc + copy) | Manual | No necesario (`&[u8]` tiene longitud) |
| **Memoria** | Manual (malloc/free) | Manual | Automática (ownership) |
| **Setup** | Makefile + compilar | Makefile + compilar | `cargo fuzz init` + editar 1 archivo |
| **Compilar** | `clang -fsanitize=fuzzer` | `afl-cc` | `cargo fuzz run` (automático) |
| **Sanitizers** | Manual en CFLAGS | Manual en CFLAGS | `--sanitizer=address` (default) |
| **Corpus** | Directorio manual | Directorio manual | Automático en `fuzz/corpus/` |
| **Artifacts** | `-artifact_prefix=dir/` | `output/crashes/` | Automático en `fuzz/artifacts/` |
| **Minimizar crash** | `-minimize_crash=1` | `afl-tmin` | `cargo fuzz tmin` |
| **Minimizar corpus** | `-merge=1` | `afl-cmin` | `cargo fuzz cmin` |
| **Regression test** | `-runs=0` | Ejecutar en crashes | `cargo fuzz run -- -runs=0` |
| **Cobertura** | `llvm-cov` manual | `afl-cov` / `afl-plot` | `cargo fuzz coverage` |
| **Diccionario** | `-dict=file` | `-x file` | `-- -dict=file` |
| **Motor** | libFuzzer (LLVM) | AFL++ (fork-based) | libFuzzer (LLVM) |
| **Modo** | In-process | Fork-based (o persistent) | In-process |
| **Velocidad** | Muy alta | Alta | Alta |
| **Bugs típicos** | Memoria (OOB, UAF, UB) | Memoria (OOB, UAF, UB) | Panics, lógica, unsafe |
| **Toolchain** | Clang | afl-cc / gcc | Rust nightly |

### Diferencias filosóficas clave

```
C fuzzing:
  "¿Puedo crashear el programa?" → casi siempre SÍ
  El fuzzer encuentra bugs de MEMORIA
  Cada crash es potencialmente explotable
  Sanitizers son ESENCIALES para detectar bugs silenciosos

Rust safe fuzzing:
  "¿Puedo hacer panic al programa?" → a veces SÍ
  El fuzzer encuentra bugs LÓGICOS y panics
  Los panics son DoS (no RCE)
  Sanitizers son menos críticos (solo para unsafe/FFI)

Rust unsafe fuzzing:
  = C fuzzing
  Mismos bugs, misma criticidad
  Sanitizers son ESENCIALES
```

### Ventajas de cargo-fuzz sobre libFuzzer en C

```
1. ERGONOMÍA
   C:    Escribir Makefile, compilar a mano, gestionar directorios
   Rust: cargo fuzz init, cargo fuzz run — listo

2. MEMORIA SEGURA POR DEFECTO
   C:    Sanitizers necesarios para detectar bugs silenciosos
   Rust: El compilador ya previene la mayoría de bugs de memoria

3. TIPOS ESTRUCTURADOS
   C:    Parsear bytes a mano con FuzzedDataProvider manual
   Rust: derive(Arbitrary) → inputs tipados automáticamente

4. INTEGRACIÓN CON CARGO
   C:    Separado del sistema de build
   Rust: Parte del ecosistema Cargo

5. REPRODUCIBILIDAD
   C:    Depende de tu setup
   Rust: cargo fuzz run + Cargo.lock → reproducible
```

---

## 32. Errores comunes

### Error 1: usar stable en vez de nightly

```
$ cargo fuzz run fuzz_parse
error: the option `Z` is only accepted on the nightly compiler

Causa: tu toolchain default es stable y cargo-fuzz no puede encontrar nightly.

Solución:
  rustup toolchain install nightly
  # cargo-fuzz usa nightly automáticamente
```

### Error 2: panic = "abort" en el perfil

```toml
# ❌ En fuzz/Cargo.toml:
[profile.release]
panic = "abort"

# Resultado: los panics terminan el proceso inmediatamente.
# El fuzzer no puede capturarlos como crashes normales.
# Cada panic mata el proceso y el fuzzer debe reiniciar (lento).
```

```toml
# ✓ Corrección: NO poner panic = "abort"
[profile.release]
opt-level = 3
debug = true
debug-assertions = true
overflow-checks = true
# panic = "unwind" es el default, no necesitas ponerlo
```

### Error 3: unwrap() en el harness

```rust
// ❌ MAL: unwrap crashea en inputs inválidos (pero eso no es un bug del código)
fuzz_target!(|data: &[u8]| {
    let s = std::str::from_utf8(data).unwrap();  // panic si no es UTF-8
    let _ = my_lib::parse(s);
});

// ✓ BIEN: manejar errores de conversión
fuzz_target!(|data: &[u8]| {
    if let Ok(s) = std::str::from_utf8(data) {
        let _ = my_lib::parse(s);
    }
});

// ✓ MEJOR: usar &str directamente si solo quieres UTF-8
fuzz_target!(|data: &str| {
    let _ = my_lib::parse(data);
});
```

### Error 4: I/O en el harness

```rust
// ❌ MAL: escribir al disco es lento
fuzz_target!(|data: &[u8]| {
    std::fs::write("/tmp/input.bin", data).unwrap();
    let result = my_lib::process_file("/tmp/input.bin");
    std::fs::remove_file("/tmp/input.bin").ok();
});

// ✓ BIEN: operar en memoria
fuzz_target!(|data: &[u8]| {
    let result = my_lib::process_bytes(data);
});

// Si tu función SOLO acepta File/Path, usa Cursor:
fuzz_target!(|data: &[u8]| {
    let cursor = std::io::Cursor::new(data);
    let result = my_lib::process_reader(cursor);
});
```

### Error 5: no acotar el tamaño del input

```rust
// ❌ MAL: inputs de 1 MB causan OOM en funciones O(n²)
fuzz_target!(|data: &[u8]| {
    my_lib::expensive_sort(data);  // O(n²) con inputs grandes
});

// ✓ BIEN: limitar con -max_len
// cargo fuzz run my_target -- -max_len=1024

// ✓ TAMBIÉN BIEN: limitar en el harness
fuzz_target!(|data: &[u8]| {
    if data.len() > 1024 {
        return;
    }
    my_lib::expensive_sort(data);
});
```

### Error 6: olvidar commitear el corpus

```
Problema: fuzzeaste 8 horas, encontraste cobertura excelente,
pero no commiteaste el corpus. Próxima sesión empieza de cero.

Solución:
  # Minimizar antes de commitear (reduce tamaño)
  cargo fuzz cmin fuzz_parse

  # Commitear
  git add fuzz/corpus/fuzz_parse/
  git commit -m "Add fuzzing corpus for fuzz_parse"
```

### Error 7: no instalar las dependencias de compilación

```
$ cargo fuzz run fuzz_parse
error: linker `cc` not found

# En Fedora/RHEL:
sudo dnf groupinstall "Development Tools"
sudo dnf install clang llvm-devel

# En Ubuntu/Debian:
sudo apt install build-essential clang llvm-dev
```

---

## 33. Programa de práctica: json_value parser

### Objetivo

Crear un parser de valores JSON simple, fuzzearlo con cargo-fuzz, encontrar y corregir bugs.

### Especificación

```
json_value/
├── Cargo.toml
├── src/
│   └── lib.rs                ← parser de JSON values
├── tests/
│   └── unit_tests.rs         ← tests unitarios
└── fuzz/
    ├── Cargo.toml
    ├── fuzz_targets/
    │   ├── fuzz_parse.rs     ← harness 1: parsear, no crash
    │   ├── fuzz_roundtrip.rs ← harness 2: parse → to_string → parse → compare
    │   └── fuzz_display.rs   ← harness 3: parse → Display → verificar formato
    ├── corpus/
    │   └── fuzz_parse/       ← semillas
    └── dicts/
        └── json.dict         ← diccionario JSON
```

### src/lib.rs

```rust
use std::collections::HashMap;
use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    Str(String),
    Array(Vec<JsonValue>),
    Object(HashMap<String, JsonValue>),
}

#[derive(Debug, Clone, PartialEq)]
pub enum ParseError {
    UnexpectedEnd,
    UnexpectedChar(char),
    InvalidNumber(String),
    InvalidEscape(char),
    MaxDepthExceeded,
    TrailingContent,
}

const MAX_DEPTH: usize = 64;

pub fn parse(input: &str) -> Result<JsonValue, ParseError> {
    let mut parser = Parser::new(input);
    let value = parser.parse_value(0)?;
    parser.skip_whitespace();
    if parser.pos < parser.input.len() {
        return Err(ParseError::TrailingContent);
    }
    Ok(value)
}

struct Parser {
    input: Vec<char>,
    pos: usize,
}

impl Parser {
    fn new(input: &str) -> Self {
        Parser {
            input: input.chars().collect(),
            pos: 0,
        }
    }

    fn peek(&self) -> Option<char> {
        self.input.get(self.pos).copied()
    }

    fn advance(&mut self) -> Option<char> {
        let ch = self.input.get(self.pos).copied();
        if ch.is_some() {
            self.pos += 1;
        }
        ch
    }

    fn expect(&mut self, expected: char) -> Result<(), ParseError> {
        match self.advance() {
            Some(ch) if ch == expected => Ok(()),
            Some(ch) => Err(ParseError::UnexpectedChar(ch)),
            None => Err(ParseError::UnexpectedEnd),
        }
    }

    fn skip_whitespace(&mut self) {
        while let Some(ch) = self.peek() {
            if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' {
                self.advance();
            } else {
                break;
            }
        }
    }

    fn parse_value(&mut self, depth: usize) -> Result<JsonValue, ParseError> {
        if depth > MAX_DEPTH {
            return Err(ParseError::MaxDepthExceeded);
        }

        self.skip_whitespace();

        match self.peek() {
            None => Err(ParseError::UnexpectedEnd),
            Some('n') => self.parse_null(),
            Some('t') | Some('f') => self.parse_bool(),
            Some('"') => self.parse_string().map(JsonValue::Str),
            Some('[') => self.parse_array(depth),
            Some('{') => self.parse_object(depth),
            Some(ch) if ch == '-' || ch.is_ascii_digit() => self.parse_number(),
            Some(ch) => Err(ParseError::UnexpectedChar(ch)),
        }
    }

    fn parse_null(&mut self) -> Result<JsonValue, ParseError> {
        for expected in ['n', 'u', 'l', 'l'] {
            self.expect(expected)?;
        }
        Ok(JsonValue::Null)
    }

    fn parse_bool(&mut self) -> Result<JsonValue, ParseError> {
        match self.peek() {
            Some('t') => {
                for expected in ['t', 'r', 'u', 'e'] {
                    self.expect(expected)?;
                }
                Ok(JsonValue::Bool(true))
            }
            Some('f') => {
                for expected in ['f', 'a', 'l', 's', 'e'] {
                    self.expect(expected)?;
                }
                Ok(JsonValue::Bool(false))
            }
            Some(ch) => Err(ParseError::UnexpectedChar(ch)),
            None => Err(ParseError::UnexpectedEnd),
        }
    }

    fn parse_number(&mut self) -> Result<JsonValue, ParseError> {
        let start = self.pos;

        // Signo opcional
        if self.peek() == Some('-') {
            self.advance();
        }

        // Parte entera
        // BUG 1: No rechaza números con leading zeros (ej: "007")
        // JSON estándar solo permite "0" seguido de "." o fin
        while let Some(ch) = self.peek() {
            if ch.is_ascii_digit() {
                self.advance();
            } else {
                break;
            }
        }

        // Parte decimal
        if self.peek() == Some('.') {
            self.advance();
            // BUG 2: No verifica que haya al menos un dígito después del punto
            // "3." debería ser un error pero se acepta
            while let Some(ch) = self.peek() {
                if ch.is_ascii_digit() {
                    self.advance();
                } else {
                    break;
                }
            }
        }

        // Exponente
        if self.peek() == Some('e') || self.peek() == Some('E') {
            self.advance();
            if self.peek() == Some('+') || self.peek() == Some('-') {
                self.advance();
            }
            while let Some(ch) = self.peek() {
                if ch.is_ascii_digit() {
                    self.advance();
                } else {
                    break;
                }
            }
        }

        let number_str: String = self.input[start..self.pos].iter().collect();
        match number_str.parse::<f64>() {
            Ok(n) => Ok(JsonValue::Number(n)),
            Err(_) => Err(ParseError::InvalidNumber(number_str)),
        }
    }

    fn parse_string(&mut self) -> Result<String, ParseError> {
        self.expect('"')?;
        let mut result = String::new();

        loop {
            match self.advance() {
                None => return Err(ParseError::UnexpectedEnd),
                Some('"') => return Ok(result),
                Some('\\') => {
                    match self.advance() {
                        Some('"') => result.push('"'),
                        Some('\\') => result.push('\\'),
                        Some('/') => result.push('/'),
                        Some('n') => result.push('\n'),
                        Some('t') => result.push('\t'),
                        Some('r') => result.push('\r'),
                        // BUG 3: No implementa \uXXXX (escapes unicode)
                        // Simplemente rechaza con error
                        Some('u') => {
                            // BUG 4: consume 4 chars pero no los convierte
                            // a un codepoint unicode real
                            let mut hex = String::new();
                            for _ in 0..4 {
                                match self.advance() {
                                    Some(ch) => hex.push(ch),
                                    None => return Err(ParseError::UnexpectedEnd),
                                }
                            }
                            // Intenta parsear pero ignora errores
                            if let Ok(code) = u32::from_str_radix(&hex, 16) {
                                if let Some(ch) = char::from_u32(code) {
                                    result.push(ch);
                                }
                                // Si code no es un char válido, simplemente
                                // no agrega nada — pérdida silenciosa de datos
                            }
                        }
                        Some(ch) => return Err(ParseError::InvalidEscape(ch)),
                        None => return Err(ParseError::UnexpectedEnd),
                    }
                }
                Some(ch) => result.push(ch),
            }
        }
    }

    fn parse_array(&mut self, depth: usize) -> Result<JsonValue, ParseError> {
        self.expect('[')?;
        let mut elements = Vec::new();

        self.skip_whitespace();
        if self.peek() == Some(']') {
            self.advance();
            return Ok(JsonValue::Array(elements));
        }

        loop {
            let value = self.parse_value(depth + 1)?;
            elements.push(value);

            self.skip_whitespace();
            match self.peek() {
                Some(',') => {
                    self.advance();
                }
                Some(']') => {
                    self.advance();
                    return Ok(JsonValue::Array(elements));
                }
                Some(ch) => return Err(ParseError::UnexpectedChar(ch)),
                None => return Err(ParseError::UnexpectedEnd),
            }
        }
    }

    fn parse_object(&mut self, depth: usize) -> Result<JsonValue, ParseError> {
        self.expect('{')?;
        let mut map = HashMap::new();

        self.skip_whitespace();
        if self.peek() == Some('}') {
            self.advance();
            return Ok(JsonValue::Object(map));
        }

        loop {
            self.skip_whitespace();
            let key = self.parse_string()?;

            self.skip_whitespace();
            self.expect(':')?;

            let value = self.parse_value(depth + 1)?;
            map.insert(key, value);

            self.skip_whitespace();
            match self.peek() {
                Some(',') => {
                    self.advance();
                }
                Some('}') => {
                    self.advance();
                    return Ok(JsonValue::Object(map));
                }
                Some(ch) => return Err(ParseError::UnexpectedChar(ch)),
                None => return Err(ParseError::UnexpectedEnd),
            }
        }
    }
}

impl fmt::Display for JsonValue {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            JsonValue::Null => write!(f, "null"),
            JsonValue::Bool(b) => write!(f, "{}", b),
            JsonValue::Number(n) => {
                if n.fract() == 0.0 && n.is_finite() {
                    write!(f, "{}", *n as i64)
                } else {
                    write!(f, "{}", n)
                }
            }
            JsonValue::Str(s) => {
                write!(f, "\"")?;
                for ch in s.chars() {
                    match ch {
                        '"' => write!(f, "\\\"")?,
                        '\\' => write!(f, "\\\\")?,
                        '\n' => write!(f, "\\n")?,
                        '\r' => write!(f, "\\r")?,
                        '\t' => write!(f, "\\t")?,
                        ch if (ch as u32) < 0x20 => {
                            write!(f, "\\u{:04x}", ch as u32)?;
                        }
                        ch => write!(f, "{}", ch)?,
                    }
                }
                write!(f, "\"")
            }
            JsonValue::Array(arr) => {
                write!(f, "[")?;
                for (i, val) in arr.iter().enumerate() {
                    if i > 0 {
                        write!(f, ",")?;
                    }
                    write!(f, "{}", val)?;
                }
                write!(f, "]")
            }
            JsonValue::Object(obj) => {
                write!(f, "{{")?;
                for (i, (key, val)) in obj.iter().enumerate() {
                    if i > 0 {
                        write!(f, ",")?;
                    }
                    // Reusar la lógica de escape de strings
                    write!(f, "\"{}\":{}", key, val)?;
                    // BUG 5: no escapa las keys!
                    // Si key contiene '"' o '\', el output es JSON inválido
                    // y el roundtrip falla
                }
                write!(f, "}}")
            }
        }
    }
}
```

### Bugs intencionales resumidos

```
Bug 1 (parse_number): Acepta leading zeros ("007" → 7.0)
      JSON estándar: solo "0" o "1"-"9" seguido de dígitos

Bug 2 (parse_number): Acepta "3." sin dígitos después del punto
      JSON estándar: "3.0" es válido, "3." no

Bug 3 (parse_string): \uXXXX con codepoints inválidos pierde datos
      "\uD800" (surrogate) → char::from_u32 retorna None → nada se agrega

Bug 4 (parse_string): \uXXXX con hex inválido falla silenciosamente
      "\uZZZZ" → from_str_radix falla → nada se agrega

Bug 5 (Display): No escapa keys del Object
      Key con '"' → JSON inválido → roundtrip falla
```

### Harness 1: fuzz_parse.rs

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use json_value::parse;

fuzz_target!(|data: &str| {
    // Solo verificar que no panicea
    let _ = parse(data);
});
```

### Harness 2: fuzz_roundtrip.rs

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use json_value::{parse, JsonValue};

fuzz_target!(|data: &str| {
    // Parse
    let value = match parse(data) {
        Ok(v) => v,
        Err(_) => return,
    };

    // Serialize
    let serialized = value.to_string();

    // Re-parse
    let reparsed = match parse(&serialized) {
        Ok(v) => v,
        Err(e) => {
            panic!(
                "Roundtrip failed: serialized output is not valid JSON!\n\
                 Input:      {:?}\n\
                 Parsed:     {:?}\n\
                 Serialized: {:?}\n\
                 Re-parse error: {:?}",
                data, value, serialized, e
            );
        }
    };

    // Compare (nota: comparar f64 con == puede tener falsos positivos por NaN)
    // Para este ejercicio, comparamos directamente
    if value != reparsed {
        // Verificar si la diferencia es por float precision
        if !is_float_difference(&value, &reparsed) {
            panic!(
                "Roundtrip mismatch!\n\
                 Input:      {:?}\n\
                 First parse:  {:?}\n\
                 Serialized:   {:?}\n\
                 Second parse: {:?}",
                data, value, serialized, reparsed
            );
        }
    }
});

fn is_float_difference(a: &JsonValue, b: &JsonValue) -> bool {
    match (a, b) {
        (JsonValue::Number(x), JsonValue::Number(y)) => {
            (x - y).abs() < 1e-10 || (x.is_nan() && y.is_nan())
        }
        _ => false,
    }
}
```

### Harness 3: fuzz_display.rs

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use json_value::parse;

fuzz_target!(|data: &str| {
    // Parse
    let value = match parse(data) {
        Ok(v) => v,
        Err(_) => return,
    };

    // Display
    let displayed = value.to_string();

    // Verificar que el Display no contiene NUL bytes
    assert!(
        !displayed.contains('\0'),
        "Display output contains NUL byte: {:?}",
        displayed
    );

    // Verificar que arrays/objects están bien balanceados
    let mut bracket_depth: i32 = 0;
    let mut brace_depth: i32 = 0;
    let mut in_string = false;
    let mut prev_escape = false;

    for ch in displayed.chars() {
        if in_string {
            if ch == '\\' && !prev_escape {
                prev_escape = true;
                continue;
            }
            if ch == '"' && !prev_escape {
                in_string = false;
            }
            prev_escape = false;
            continue;
        }

        match ch {
            '"' => in_string = true,
            '[' => bracket_depth += 1,
            ']' => {
                bracket_depth -= 1;
                assert!(bracket_depth >= 0, "Unmatched ']' in Display output");
            }
            '{' => brace_depth += 1,
            '}' => {
                brace_depth -= 1;
                assert!(brace_depth >= 0, "Unmatched '}}' in Display output");
            }
            _ => {}
        }
    }

    assert_eq!(bracket_depth, 0, "Unmatched '[' in Display output");
    assert_eq!(brace_depth, 0, "Unmatched '{{' in Display output");
});
```

### Semillas

```bash
# fuzz/corpus/fuzz_parse/
echo -n 'null' > seed_null
echo -n 'true' > seed_true
echo -n 'false' > seed_false
echo -n '42' > seed_integer
echo -n '3.14' > seed_float
echo -n '-1e10' > seed_scientific
echo -n '"hello"' > seed_string
echo -n '"hello\nworld"' > seed_string_escape
echo -n '"\u0041"' > seed_unicode
echo -n '[]' > seed_empty_array
echo -n '[1,2,3]' > seed_array
echo -n '{}' > seed_empty_object
echo -n '{"key":"value"}' > seed_object
echo -n '{"a":{"b":{"c":1}}}' > seed_nested
echo -n '[1,"two",true,null,[5]]' > seed_mixed
```

### Diccionario

```
# fuzz/dicts/json.dict

# Delimitadores
"{"
"}"
"["
"]"

# Separadores
":"
","

# Keywords
"null"
"true"
"false"

# Strings
"\""
"\\\""
"\\n"
"\\t"
"\\r"
"\\\\"
"\\/"
"\\u0000"
"\\u0041"
"\\uD800"
"\\uFFFF"

# Números
"0"
"-0"
"1"
"-1"
"0.0"
"1e10"
"1E10"
"1e+10"
"1e-10"
"1.5e2"

# Edge cases
"007"
"3."
".5"

# Whitespace
" "
"\t"
"\n"
"\r\n"
```

### Tareas

1. **Crea el proyecto** `cargo new --lib json_value` e implementa `src/lib.rs` con el parser dado

2. **Inicializa fuzzing**: `cargo fuzz init -t fuzz_parse`

3. **Agrega los 3 targets**: `cargo fuzz add fuzz_roundtrip`, `cargo fuzz add fuzz_display`

4. **Crea las semillas** en `fuzz/corpus/fuzz_parse/`

5. **Crea el diccionario** en `fuzz/dicts/json.dict`

6. **Fuzzea `fuzz_parse`** por 5 minutos:
   ```bash
   cargo fuzz run fuzz_parse -- -max_total_time=300 -dict=fuzz/dicts/json.dict
   ```

7. **Fuzzea `fuzz_roundtrip`** por 10 minutos:
   ```bash
   cargo fuzz run fuzz_roundtrip -- -max_total_time=600 -dict=fuzz/dicts/json.dict
   ```
   - ¿El roundtrip encuentra el Bug 5 (keys no escapadas)?
   - ¿Encuentra los bugs de pérdida de datos con \uXXXX?

8. **Fuzzea `fuzz_display`** por 5 minutos

9. **Para cada crash encontrado**:
   - Minimiza: `cargo fuzz tmin <target> <artifact>`
   - Analiza: `xxd <minimized_artifact>`
   - Identifica qué bug corresponde (1-5)
   - Corrige el bug
   - Verifica que el crash ya no ocurre
   - Crea un test unitario de regression

10. **Mide la cobertura** final: `cargo fuzz coverage fuzz_parse`

11. **Minimiza el corpus**: `cargo fuzz cmin fuzz_parse`
    - ¿Cuántos archivos se eliminaron?

---

## 34. Ejercicios

### Ejercicio 1: primer fuzzing de un parser

**Objetivo**: Familiarizarse con el flujo completo de cargo-fuzz.

**Tareas**:

**a)** Crea una librería que parsee strings de configuración con formato `key=value`:
   ```rust
   pub fn parse_config(input: &str) -> HashMap<String, String> {
       // Formato: "key1=value1\nkey2=value2\n..."
       // Maneja: comentarios (#), whitespace, keys sin valor, keys duplicadas
   }
   ```
   Introduce un bug: si un `value` contiene `=`, el parser solo toma lo que está antes del primer `=` pero accede al índice 2 del split sin verificar.

**b)** Inicializa cargo-fuzz e implementa un harness que:
   - Llame a `parse_config` con el input del fuzzer
   - Verifique que para cada key retornada, la key no está vacía
   - Verifique que no hay keys duplicadas

**c)** Crea 5 semillas y un diccionario con tokens (`=`, `#`, `\n`, whitespace).

**d)** Fuzzea 5 minutos. ¿Encuentra el bug? ¿Cuánto tarda?

**e)** Minimiza el crash. ¿Cuál es el input minimizado?

---

### Ejercicio 2: roundtrip fuzzing

**Objetivo**: Encontrar bugs lógicos con roundtrip.

**Tareas**:

**a)** Implementa un encoder/decoder de base64 en Rust:
   ```rust
   pub fn base64_encode(input: &[u8]) -> String { ... }
   pub fn base64_decode(input: &str) -> Result<Vec<u8>, DecodeError> { ... }
   ```
   Introduce un bug sutil: el encoder no maneja correctamente padding cuando `input.len() % 3 == 1` (genera un `=` en vez de `==`).

**b)** Escribe dos harnesses:
   - `fuzz_decode`: `base64_decode(data_as_str)` no debe paniquear
   - `fuzz_roundtrip`: `base64_decode(base64_encode(data)) == data`

**c)** Fuzzea cada harness 5 minutos. ¿Cuál encuentra el bug?

**d)** ¿El harness `fuzz_decode` sería capaz de encontrar el bug de padding? ¿Por qué sí o por qué no?

---

### Ejercicio 3: fuzzing con diccionario vs sin diccionario

**Objetivo**: Medir el impacto de los diccionarios en la exploración.

**Tareas**:

**a)** Implementa un parser de expresiones matemáticas simples:
   ```rust
   pub fn eval(expr: &str) -> Result<f64, EvalError> {
       // Soporta: +, -, *, /, (), números, espacios
       // Ej: "2 + 3 * (4 - 1)" → 11.0
   }
   ```

**b)** Crea un diccionario con: `+`, `-`, `*`, `/`, `(`, `)`, `0`-`9`, `.`, ` `.

**c)** Fuzzea 5 minutos SIN diccionario. Anota: cobertura final, exec/s, corpus size.

**d)** Fuzzea 5 minutos CON diccionario (misma semilla de random para comparar):
   ```bash
   cargo fuzz run fuzz_eval -- -max_total_time=300 -seed=42
   cargo fuzz run fuzz_eval -- -max_total_time=300 -seed=42 -dict=fuzz/dicts/math.dict
   ```

**e)** Compara las métricas. ¿El diccionario mejoró la cobertura? ¿Por cuánto?

---

### Ejercicio 4: múltiples targets para una estructura de datos

**Objetivo**: Demostrar el valor de múltiples harnesses para la misma librería.

**Tareas**:

**a)** Implementa un `BoundedVec<T>` — un Vec con capacidad máxima:
   ```rust
   pub struct BoundedVec<T> {
       data: Vec<T>,
       max_capacity: usize,
   }

   impl<T> BoundedVec<T> {
       pub fn new(max_capacity: usize) -> Self { ... }
       pub fn push(&mut self, item: T) -> Result<(), FullError> { ... }
       pub fn pop(&mut self) -> Option<T> { ... }
       pub fn get(&self, index: usize) -> Option<&T> { ... }
       pub fn len(&self) -> usize { ... }
       pub fn is_empty(&self) -> bool { ... }
       pub fn is_full(&self) -> bool { ... }
       pub fn clear(&mut self) { ... }
       pub fn remove(&mut self, index: usize) -> Result<T, IndexError> { ... }
       pub fn insert(&mut self, index: usize, item: T) -> Result<(), Error> { ... }
   }
   ```
   Introduce un bug: `remove` no actualiza correctamente un campo interno de conteo cuando el índice es exactamente `len - 1`.

**b)** Escribe 3 harnesses:
   - `fuzz_ops`: secuencia de operaciones (push/pop/get/remove/insert)
   - `fuzz_invariants`: secuencia de operaciones + verificar invariantes después de cada una:
     ```rust
     assert_eq!(bv.is_empty(), bv.len() == 0);
     assert_eq!(bv.is_full(), bv.len() == bv.max_capacity);
     assert!(bv.len() <= bv.max_capacity);
     ```
   - `fuzz_comparison`: comparar con `Vec<T>` estándar (differential)

**c)** Fuzzea cada harness 5 minutos. ¿Cuál encuentra el bug y por qué?

**d)** ¿El harness `fuzz_ops` (sin invariantes) puede encontrar el bug? Explica.

---

## Navegación

- **Anterior**: [S01/T04 - Escribir harnesses](../../S01_Fuzzing_en_C/T04_Escribir_harnesses/README.md)
- **Siguiente**: [T02 - Arbitrary trait](../T02_Arbitrary_trait/README.md)

---

> **Sección 2: Fuzzing en Rust** — Tópico 1 de 4 completado
