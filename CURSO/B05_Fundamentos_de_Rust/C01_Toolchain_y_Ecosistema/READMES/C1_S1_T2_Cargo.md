# T02 — Cargo

## Que es Cargo

Cargo es el sistema de compilacion **y** gestor de paquetes de Rust.
Combina en una sola herramienta lo que en otros ecosistemas requiere
varias: Make (compilacion), pip/npm (dependencias), y parte de CMake
(estructura de proyecto). No es una herramienta de terceros: viene
incluida en la instalacion oficial de Rust y **todos** los proyectos
Rust la usan.

```bash
# Verificar que Cargo esta instalado:
cargo --version
# cargo 1.82.0 (8f40fc59f 2024-08-21)

# Cargo se instala junto con rustc al usar rustup:
rustup show
# Muestra el toolchain activo, que incluye cargo.
```

```bash
# Cargo crea proyectos, descarga dependencias, compila (invoca rustc),
# ejecuta, testea, genera documentacion, publica paquetes, formatea
# y analiza codigo. En la practica, casi nunca se invoca rustc
# directamente. Cargo es la interfaz principal para todo el ciclo
# de desarrollo.
```

## Crear proyectos

### cargo new — proyecto binario

`cargo new` crea un proyecto nuevo con toda la estructura necesaria:

```bash
# Crear un proyecto binario (ejecutable):
cargo new mi_proyecto
#      Created binary (application) `mi_proyecto` package

# Cargo crea esta estructura:
# mi_proyecto/
# ├── Cargo.toml       ← manifiesto del proyecto
# ├── src/
# │   └── main.rs      ← punto de entrada del binario
# └── .git/            ← repositorio git inicializado
#     └── ...
#     .gitignore        ← ignora target/
```

```rust
// src/main.rs generado:
fn main() {
    println!("Hello, world!");
}
```

```toml
# Cargo.toml generado:
[package]
name = "mi_proyecto"
version = "0.1.0"
edition = "2021"

[dependencies]
```

```bash
# El nombre del proyecto debe ser un identificador valido:
cargo new my-project        # OK — guiones se convierten a guiones bajos
cargo new 123bad            # Error — no puede empezar con numero
```

### cargo new --lib — proyecto de biblioteca

```bash
# Crear una biblioteca (library crate):
cargo new --lib mi_biblioteca
#      Created library `mi_biblioteca` package

# La diferencia: genera src/lib.rs en vez de src/main.rs
# mi_biblioteca/
# ├── Cargo.toml
# └── src/
#     └── lib.rs
```

```rust
// src/lib.rs generado:
pub fn add(left: u64, right: u64) -> u64 {
    left + right
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let result = add(2, 2);
        assert_eq!(result, 4);
    }
}
```

```bash
# Diferencia entre crate binario y crate de biblioteca:
#
# Crate binario (--bin, el default):
#   - Tiene src/main.rs con fn main()
#   - Produce un ejecutable
#   - Se puede ejecutar con cargo run
#
# Crate de biblioteca (--lib):
#   - Tiene src/lib.rs (sin fn main)
#   - Produce una biblioteca (.rlib) para que otros crates la usen
#   - NO se puede ejecutar con cargo run
#   - Se usa como dependencia en otros proyectos
#
# Un proyecto puede tener AMBOS: src/main.rs y src/lib.rs.
# En ese caso el binario puede usar la biblioteca del mismo crate.
```

### cargo init — inicializar en directorio existente

```bash
# Si ya tienes un directorio y quieres convertirlo en proyecto Cargo:
mkdir proyecto_existente
cd proyecto_existente
cargo init                   # binario
cargo init --lib             # biblioteca

# Equivale a cargo new pero dentro del directorio actual.
# No crea un subdirectorio nuevo.
# Respeta lo que ya existe (.git, .gitignore, etc.).
```

### Opciones adicionales de creacion

```bash
# No inicializar repositorio git:
cargo new mi_proyecto --vcs none

# Especificar el nombre del paquete (distinto al directorio):
cargo new my-dir --name my_package

# Especificar la edicion de Rust:
cargo new mi_proyecto --edition 2021   # default actual
cargo new mi_proyecto --edition 2024
```

## Estructura de proyecto

Un proyecto Cargo maduro tiene esta estructura de directorios.
Cargo reconoce cada directorio automaticamente por convencion:

```bash
# Estructura completa de un proyecto Cargo:
mi_proyecto/
├── Cargo.toml        # Manifiesto: metadata, dependencias, configuracion
├── Cargo.lock        # Versiones exactas de dependencias (generado automaticamente)
├── src/
│   ├── main.rs       # Punto de entrada del binario (crate binario)
│   ├── lib.rs        # Raiz de la biblioteca (crate de biblioteca)
│   └── bin/          # Binarios adicionales: cargo run --bin otro
│       └── otro.rs
├── tests/            # Tests de integracion (cada archivo es un crate separado)
├── benches/          # Benchmarks
├── examples/         # Programas de ejemplo: cargo run --example demo
├── build.rs          # Build script (se ejecuta antes de compilar)
└── target/           # Directorio de salida de compilacion (gitignored)

# No necesitas crear todo esto manualmente.
# Cargo reconoce cada directorio por convencion.
# Un proyecto simple solo tiene Cargo.toml y src/main.rs.
```

### Cargo.toml — el manifiesto

```toml
# Cargo.toml es el archivo central de configuracion del proyecto.
# Usa el formato TOML (Tom's Obvious, Minimal Language).

[package]
name = "mi_proyecto"           # Nombre del paquete
version = "0.1.0"              # Version (semver)
edition = "2021"               # Edicion de Rust (2015, 2018, 2021, 2024)

[dependencies]
serde = "1.0"                  # Descarga de crates.io
mi_lib = { path = "../mi_lib" }    # Dependencia local
otro = { git = "https://github.com/user/otro" }  # Desde git

[dev-dependencies]
tempfile = "3.0"               # Solo para tests y benchmarks
```

### Cargo.lock — versiones exactas

```bash
# Cargo.lock se genera automaticamente al compilar.
# Registra las versiones EXACTAS de todas las dependencias
# (incluyendo dependencias transitivas).

# Para proyectos binarios: hacer commit de Cargo.lock
#   → Garantiza builds reproducibles.
#   → Todos los desarrolladores usan las mismas versiones.

# Para bibliotecas: NO hacer commit de Cargo.lock
#   → Los consumidores de la biblioteca deciden las versiones.
#   → Agregar /Cargo.lock al .gitignore.

# Actualizar dependencias respetando semver:
cargo update
# Actualiza Cargo.lock a las versiones mas recientes compatibles
# con lo especificado en Cargo.toml.

# Actualizar una dependencia especifica:
cargo update serde
```

## Comandos principales

### cargo build — compilar

```bash
# Compilar en modo debug (default):
cargo build
#    Compiling mi_proyecto v0.1.0 (/home/user/mi_proyecto)
#     Finished `dev` profile [unoptimized + debuginfo] target(s) in 0.54s

# El binario se genera en target/debug/:
./target/debug/mi_proyecto
# Hello, world!

# Compilar en modo release (optimizado):
cargo build --release
#     Finished `release` profile [optimized] target(s) in 0.72s
# Binario en target/release/mi_proyecto

# La diferencia de rendimiento entre debug y release puede ser
# de 10x a 100x. SIEMPRE usar --release para medir rendimiento.
```

```bash
# Otras opciones de build:
cargo build --bin nombre    # compilar un binario especifico
cargo build --lib           # compilar solo la biblioteca
cargo build --example demo  # compilar un ejemplo
cargo build --verbose       # mostrar invocaciones a rustc
cargo build --target x86_64-unknown-linux-musl   # cross-compilation
```

```bash
# Compilacion incremental — Cargo solo recompila lo que cambio:
cargo build    # primera vez: 2.34s (compila todo)
cargo build    # sin cambios: 0.01s (nada que recompilar)
# Despues de editar un archivo:
cargo build    # parcial: 0.45s (solo recompila lo afectado)
```

### cargo run — compilar y ejecutar

```bash
# Compilar y ejecutar en un solo paso:
cargo run
#    Compiling mi_proyecto v0.1.0 (/home/user/mi_proyecto)
#     Finished `dev` profile [unoptimized + debuginfo] target(s) in 0.54s
#      Running `target/debug/mi_proyecto`
# Hello, world!

# Si ya esta compilado y no hubo cambios:
cargo run
#     Finished `dev` profile [unoptimized + debuginfo] target(s) in 0.01s
#      Running `target/debug/mi_proyecto`
# Hello, world!
```

```bash
# Pasar argumentos al programa (despues de --):
cargo run -- arg1 arg2 arg3
#      Running `target/debug/mi_proyecto arg1 arg2 arg3`

# Ejemplo con un programa que lee argumentos:
cargo run -- --verbose -n 5 input.txt
# Todo despues de -- se pasa al programa, no a Cargo.
# Sin --, Cargo intentaria interpretar los argumentos.
```

```bash
# Ejecutar en modo release:
cargo run --release
# Compila con optimizaciones y luego ejecuta.

# Ejecutar un binario especifico (si hay varios):
cargo run --bin otro_binario

# Ejecutar un ejemplo:
cargo run --example demo

# cargo run solo funciona con crates binarios (que tienen main.rs).
# Si intentas cargo run en una biblioteca:
# error: a bin target must be available for `cargo run`
```

### cargo check — verificar sin compilar

```bash
# Verificar que el codigo compila sin generar el binario:
cargo check
#     Checking mi_proyecto v0.1.0 (/home/user/mi_proyecto)
#     Finished `dev` profile [unoptimized + debuginfo] target(s) in 0.32s

# cargo check es MUCHO mas rapido que cargo build porque:
# - Ejecuta el analisis completo de tipos y borrowing
# - Detecta todos los errores de compilacion
# - Pero NO genera codigo maquina (la parte mas lenta)
#
# En proyectos grandes:
# cargo check  → 2-5 segundos
# cargo build  → 30-120 segundos
#
# Es el comando ideal para el ciclo de desarrollo:
# editar → cargo check → corregir errores → repetir
```

```bash
# cargo check detecta los mismos errores que cargo build:
# sintaxis, tipos, borrowing/lifetime, imports, warnings.
# Lo unico que NO detecta: errores de enlazado (linker errors),
# que son raros en Rust puro.
```

### cargo test — ejecutar tests

```bash
# Ejecutar todos los tests:
cargo test
#    Compiling mi_proyecto v0.1.0 (/home/user/mi_proyecto)
#     Finished `test` profile [unoptimized + debuginfo] target(s) in 0.82s
#      Running unittests src/main.rs (target/debug/deps/mi_proyecto-abc123)
#
# running 2 tests
# test tests::test_add ... ok
# test tests::test_multiply ... ok
#
# test result: ok. 2 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

```rust
// Ejemplo de tests unitarios en src/lib.rs o src/main.rs:
fn add(a: i32, b: i32) -> i32 {
    a + b
}

#[cfg(test)]          // Este modulo solo se compila para tests
mod tests {
    use super::*;     // Importa las funciones del modulo padre

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    fn test_add_negative() {
        assert_eq!(add(-1, 1), 0);
    }

    #[test]
    #[should_panic]
    fn test_that_panics() {
        panic!("Este test espera un panic");
    }
}
```

```bash
# Cargo ejecuta tres tipos de tests:
# 1. Tests unitarios — funciones #[test] dentro de src/
# 2. Tests de integracion — archivos en tests/
# 3. Doc tests — ejemplos en la documentacion (///)

# Filtrar tests por nombre:
cargo test test_add         # solo tests cuyo nombre contiene "test_add"

# Opciones utiles (despues de --):
cargo test -- --nocapture       # mostrar println! de los tests
cargo test -- --test-threads=1  # ejecutar en un solo hilo
cargo test -- --list            # listar tests sin ejecutarlos
cargo test -- --ignored         # ejecutar solo tests marcados #[ignore]

# Ejecutar solo un tipo de tests:
cargo test --lib                # solo unitarios
cargo test --doc                # solo doc tests
cargo test --test nombre        # un test de integracion especifico
```

### cargo doc — generar documentacion

```bash
# Generar documentacion HTML del proyecto y sus dependencias:
cargo doc
#  Documenting mi_proyecto v0.1.0 (/home/user/mi_proyecto)
#     Finished `dev` profile [unoptimized + debuginfo] target(s) in 1.23s
#   Generated /home/user/mi_proyecto/target/doc/mi_proyecto/index.html

# Generar y abrir en el navegador:
cargo doc --open
# Abre la documentacion en el navegador por defecto.
```

```rust
// La documentacion se escribe con comentarios ///:
/// Suma dos numeros enteros.
///
/// # Arguments
///
/// * `a` - Primer sumando
/// * `b` - Segundo sumando
///
/// # Examples
///
/// ```
/// let result = mi_proyecto::add(2, 3);
/// assert_eq!(result, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

// Los ejemplos en /// se ejecutan como tests con cargo test.
// Esto garantiza que la documentacion nunca quede desactualizada.
```

```bash
# Opciones adicionales:
cargo doc --no-deps                 # solo tu proyecto, sin dependencias
cargo doc --document-private-items  # incluir items privados
# La documentacion se genera en target/doc/mi_proyecto/index.html
```

### cargo clean — limpiar artefactos

```bash
# Eliminar el directorio target/:
cargo clean
#      Removed 42 files, 156.3MiB total

# Limpiar solo un perfil:
cargo clean --release     # solo target/release/
cargo clean --profile dev # solo target/debug/
```

### cargo fmt — formatear codigo

```bash
# Formatear todo el codigo del proyecto segun el estilo oficial de Rust:
cargo fmt
# Modifica los archivos in-place.

# Verificar sin modificar (util en CI):
cargo fmt --check
# Retorna exit code 1 si hay archivos que necesitan formateo.

# Si cargo fmt no esta disponible:
rustup component add rustfmt

# La configuracion se puede personalizar con rustfmt.toml en la raiz
# del proyecto, pero la mayoria de proyectos usan el estilo default.
```

### cargo clippy — linter

```bash
# Analizar el codigo con el linter oficial de Rust:
cargo clippy
# warning: redundant clone
#   --> src/main.rs:5:20
#    |
#  5 |     let s = name.clone();
#    |                  ^^^^^^^^ help: remove this

# Clippy detecta codigo no idiomatico, posibles bugs,
# patrones ineficientes y simplificaciones posibles.

# Tratar warnings como errores (util en CI):
cargo clippy -- -D warnings

# Aplicar correcciones automaticamente:
cargo clippy --fix

# Si cargo clippy no esta disponible:
rustup component add clippy
```

## Perfiles de compilacion

Cargo tiene perfiles predefinidos que controlan como se compila
el codigo. Los dos principales son `dev` y `release`:

```bash
# Perfil dev (cargo build, cargo run, cargo test):
# - opt-level = 0         → sin optimizacion
# - debug = true          → incluye informacion de depuracion
# - overflow-checks = true → detecta overflow de enteros
# - Compilacion rapida, ejecucion lenta.

# Perfil release (cargo build --release, cargo run --release):
# - opt-level = 3         → optimizacion maxima
# - debug = false         → sin informacion de depuracion
# - overflow-checks = false → no detecta overflow
# - Compilacion lenta, ejecucion rapida.
```

```bash
# La diferencia de rendimiento es dramatica:

# Ejemplo con un programa que calcula fibonacci(42):
cargo run
# Resultado en 4.2 segundos (debug)

cargo run --release
# Resultado en 0.04 segundos (release)
# → 100x mas rapido

# NUNCA medir rendimiento con el perfil debug.
# Los resultados no son representativos.
```

```toml
# Los perfiles se pueden personalizar en Cargo.toml:
[profile.dev]
opt-level = 1        # Optimizacion ligera en debug (mas rapido que 0, compila casi igual)

[profile.release]
opt-level = 3        # Optimizacion maxima (default)
lto = true           # Link-Time Optimization (binario mas pequeno/rapido, compilacion mas lenta)
strip = true         # Eliminar simbolos del binario (reduce tamano)
panic = "abort"      # Abortar en vez de unwind en panic (binario mas pequeno)
```

## El directorio target/

El directorio `target/` contiene todos los artefactos de compilacion.
Esta en el `.gitignore` generado por Cargo y **nunca** se debe incluir
en control de versiones:

```bash
# Estructura principal de target/:
target/
├── debug/               # Builds del perfil dev
│   ├── mi_proyecto      # El binario compilado
│   ├── deps/            # Dependencias compiladas
│   ├── incremental/     # Cache de compilacion incremental
│   └── .fingerprint/    # Hashes para detectar cambios
├── release/             # Builds del perfil release (misma estructura)
│   └── mi_proyecto      # El binario optimizado
└── doc/                 # Documentacion generada
```

```bash
# El directorio target/ crece rapidamente:
du -sh target/
# 500M	target/         ← un proyecto mediano
# 2.0G	target/         ← un proyecto con muchas dependencias

# Esto es normal. Cada dependencia se compila y se cachea.
# Limpiar cuando necesites espacio: cargo clean

# Para compartir target/ entre proyectos y ahorrar espacio:
export CARGO_TARGET_DIR=$HOME/.cargo-target
# Todos los proyectos compilan en ~/.cargo-target/.
# Alternativa: configurar en .cargo/config.toml:
# [build]
# target-dir = "/home/user/.cargo-target"
```

## Variables de entorno de Cargo

Cargo usa y define varias variables de entorno:

```bash
# CARGO_HOME — directorio raiz de Cargo (default: ~/.cargo)
ls ~/.cargo/
# bin/          ← binarios instalados (cargo, rustc, herramientas de cargo install)
# registry/     ← cache de crates descargados de crates.io
# git/          ← cache de dependencias descargadas de repositorios git
# config.toml   ← configuracion global de Cargo (opcional)

# CARGO_TARGET_DIR — directorio de salida de compilacion
# Default: ./target en la raiz del proyecto
export CARGO_TARGET_DIR=/tmp/cargo-build
cargo build    # artefactos van a /tmp/cargo-build/ en vez de ./target/

# RUSTFLAGS — flags adicionales para rustc
RUSTFLAGS="-C target-cpu=native" cargo build --release
RUSTFLAGS="-C link-arg=-s" cargo build       # strip symbols
```

```bash
# Otras variables utiles:
#
# CARGO_INCREMENTAL=0      → deshabilitar compilacion incremental (util en CI)
# CARGO_BUILD_JOBS=4       → limitar compilaciones paralelas (default: num CPUs)
# RUST_BACKTRACE=1         → mostrar backtrace en panics
# RUST_BACKTRACE=full      → backtrace completo
```

```rust
// Variables que Cargo define para tu codigo (accesibles con env!):
fn main() {
    let name = env!("CARGO_PKG_NAME");       // de Cargo.toml
    let version = env!("CARGO_PKG_VERSION"); // de Cargo.toml
    println!("{} v{}", name, version);
    // mi_proyecto v0.1.0

    // Otras: CARGO_PKG_AUTHORS, CARGO_MANIFEST_DIR, PROFILE
}
```

## cargo install — instalar binarios

`cargo install` descarga, compila e instala crates binarios
en `~/.cargo/bin/`, haciendolos disponibles como comandos del sistema:

```bash
# Instalar una herramienta desde crates.io:
cargo install ripgrep
#  Installing ripgrep v14.1.0
#   Compiling ...
#    Finished
#   Installed to /home/user/.cargo/bin/rg

# Ahora rg esta disponible globalmente:
rg "pattern" .

# Herramientas populares instalables con cargo install:
cargo install fd-find        # fd — alternativa a find
cargo install bat            # bat — alternativa a cat con syntax highlighting
cargo install tokei          # tokei — contar lineas de codigo
cargo install cargo-edit     # agrega cargo add, cargo rm
cargo install cargo-watch    # ejecutar comandos al detectar cambios
```

```bash
# Instalar una version especifica:
cargo install ripgrep --version 14.0.0

# Instalar forzando recompilacion (util para actualizar):
cargo install ripgrep --force

# Ver que binarios estan instalados:
cargo install --list

# Desinstalar:
cargo uninstall ripgrep

# Asegurarse de que ~/.cargo/bin esta en el PATH:
# Si no esta, agregar al perfil del shell (~/.bashrc o ~/.zshrc):
# export PATH="$HOME/.cargo/bin:$PATH"
```

## Configuracion global de Cargo

```bash
# Cargo busca configuracion en estos archivos (en orden de prioridad):
# 1. .cargo/config.toml   ← en el directorio del proyecto (o padres)
# 2. $CARGO_HOME/config.toml  ← configuracion global (~/.cargo/config.toml)
```

```toml
# Ejemplo de ~/.cargo/config.toml:
[build]
jobs = 4                        # Numero de compilaciones paralelas

[net]
retry = 3                       # Reintentos para descargas

[registries.crates-io]
protocol = "sparse"             # Protocolo rapido para el indice de crates.io

# Alias de comandos:
[alias]
b = "build"
r = "run"
t = "test"
c = "check"
br = "build --release"
rr = "run --release"
# Uso: cargo b, cargo r, cargo rr, etc.
```

---

## Ejercicios

### Ejercicio 1 — Primer proyecto con Cargo

```bash
# Crear un proyecto binario llamado "calculadora".
# Modificar src/main.rs para que:
#   - Defina una funcion add(a: i32, b: i32) -> i32
#   - Defina una funcion multiply(a: i32, b: i32) -> i32
#   - En main(), imprima los resultados de add(3, 4) y multiply(3, 4)
#
# Verificar:
#   1. cargo check   → compila sin errores
#   2. cargo run     → imprime los resultados correctos
#   3. cargo build --release → genera binario optimizado
#   4. Comparar los tamanos de target/debug/calculadora y target/release/calculadora
#      con ls -lh
#   5. cargo clean   → elimina target/
#   6. ls target/    → confirma que target/ ya no existe
```

### Ejercicio 2 — Biblioteca con tests

```bash
# Crear un proyecto de biblioteca: cargo new --lib mathlib
# En src/lib.rs:
#   - Implementar pub fn factorial(n: u64) -> u64
#   - Implementar pub fn is_prime(n: u64) -> bool
#   - Agregar tests unitarios para ambas funciones:
#       factorial(0) == 1
#       factorial(1) == 1
#       factorial(5) == 120
#       is_prime(2) == true
#       is_prime(4) == false
#       is_prime(17) == true
#
# Verificar:
#   1. cargo test    → todos los tests pasan
#   2. cargo test factorial  → solo ejecuta tests con "factorial" en el nombre
#   3. cargo test -- --list  → lista todos los tests sin ejecutarlos
#   4. Agregar un test que falle intencionalmente, observar la salida
#   5. Corregir el test, verificar que todo pasa
```

### Ejercicio 3 — Explorando la estructura

```bash
# Crear un proyecto: cargo new explorador
# Ejecutar cargo build y luego explorar target/:
#   1. Encontrar el binario compilado y ejecutarlo directamente (sin cargo run)
#   2. Medir el tamano de target/ con du -sh target/
#   3. Ejecutar cargo build --release
#   4. Medir el tamano de target/ otra vez (ahora tiene debug y release)
#   5. Comparar los binarios:
#      file target/debug/explorador
#      file target/release/explorador
#      ls -lh target/debug/explorador target/release/explorador
#   6. Ejecutar cargo clean y verificar que target/ desaparecio
#
# Observar:
#   - El binario debug tiene simbolos de depuracion (tamano mayor)
#   - El binario release esta stripeado y optimizado
#   - target/ puede ocupar cientos de MB incluso para un proyecto pequeno
```

### Ejercicio 4 — Documentacion y cargo doc

```bash
# En el proyecto mathlib del ejercicio 2:
#   1. Agregar documentacion con /// a las funciones factorial e is_prime
#      - Incluir descripcion, parametros, valor de retorno
#      - Incluir un bloque de ejemplo con ```
#   2. Ejecutar cargo doc --open
#   3. Navegar la documentacion generada en el navegador
#   4. Ejecutar cargo test → verificar que los ejemplos de la documentacion
#      se ejecutan como doc tests
#   5. Introducir un error en un ejemplo de la documentacion
#      (por ejemplo, assert_eq!(factorial(5), 999))
#   6. Ejecutar cargo test → observar que el doc test falla
#   7. Corregir el ejemplo
```

### Ejercicio 5 — Multiples binarios

```bash
# Crear un proyecto: cargo new multi_bin
# Agregar la carpeta src/bin/ con dos archivos:
#   src/bin/greet.rs   → imprime "Hello from greet!"
#   src/bin/count.rs   → imprime numeros del 1 al 10
#
# Verificar:
#   1. cargo run --bin multi_bin  → ejecuta src/main.rs
#   2. cargo run --bin greet      → ejecuta src/bin/greet.rs
#   3. cargo run --bin count      → ejecuta src/bin/count.rs
#   4. cargo build → genera tres binarios en target/debug/
#   5. Listar los binarios: ls target/debug/{multi_bin,greet,count}
```
