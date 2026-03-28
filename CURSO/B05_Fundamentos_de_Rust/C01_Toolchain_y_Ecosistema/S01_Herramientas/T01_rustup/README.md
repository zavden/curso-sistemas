# T01 — rustup

## Que es rustup

rustup es el instalador oficial y gestor de toolchains de Rust.
Funciona como un **multiplexor**: permite instalar, actualizar y
alternar entre multiples versiones del compilador Rust en la misma
maquina. Es conceptualmente similar a `nvm` (Node), `pyenv` (Python)
o `sdkman` (Java), pero con una diferencia importante: rustup es la
herramienta **oficial** mantenida por el proyecto Rust, no un proyecto
de terceros.

```bash
# rustup gestiona:
# - rustc      → el compilador de Rust
# - cargo      → el gestor de paquetes y build system
# - rust-std   → la biblioteca estandar
# - clippy     → linter oficial
# - rustfmt    → formateador oficial
# - rust-docs  → documentacion local
# - y mas componentes opcionales
```

```bash
# Ejemplo: sin rustup, para usar Rust nightly y stable
# necesitarias instalar ambos manualmente, gestionar los PATH,
# y actualizar cada uno por separado.
#
# Con rustup:
rustup default stable         # usar stable globalmente
rustup run nightly cargo build  # usar nightly para un comando puntual
rustup update                 # actualizar todos los canales instalados
```

## Instalacion

La instalacion oficial es mediante un script descargado con `curl`:

```bash
# Comando oficial de instalacion:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Desglose de flags de curl:
# --proto '=https'  → solo permitir HTTPS (seguridad)
# --tlsv1.2         → exigir TLS 1.2 minimo (seguridad)
# -s                → modo silencioso (sin barra de progreso)
# -S                → mostrar errores aunque este en modo silencioso
# -f                → fallar con exit code != 0 si el HTTP falla
# | sh              → ejecutar el script descargado en el shell
```

```bash
# El instalador pregunta por el tipo de instalacion:
#
# 1) Proceed with standard installation (default - just press enter)
# 2) Customize installation
# 3) Cancel installation
#
# La opcion 1 instala:
# - rustup (el gestor de toolchains)
# - La toolchain stable mas reciente (rustc, cargo, rust-std, etc.)
# - Configura el PATH automaticamente
```

```bash
# Que hace el instalador al PATH:
# Agrega ~/.cargo/bin al PATH en los archivos de perfil del shell.
# Los binarios se instalan en ~/.cargo/bin/:
#
# ~/.cargo/bin/rustup    → el gestor de toolchains
# ~/.cargo/bin/rustc     → proxy que invoca el rustc de la toolchain activa
# ~/.cargo/bin/cargo     → proxy que invoca el cargo de la toolchain activa
# ~/.cargo/bin/rustfmt   → proxy para rustfmt
# ~/.cargo/bin/clippy-driver → proxy para clippy
#
# Estos binarios son "proxies" — no son el compilador real.
# Cuando ejecutas rustc, el proxy consulta la toolchain activa
# y redirige al rustc real en ~/.rustup/toolchains/stable-.../bin/rustc.

# Para que el PATH se aplique en la sesion actual:
source "$HOME/.cargo/env"

# En sesiones nuevas se carga automaticamente desde:
# ~/.bashrc, ~/.zshrc o ~/.profile (segun el shell)
```

```bash
# Verificar la instalacion:
rustup --version
# rustup 1.28.1 (f9edccb0a 2025-03-05)

rustc --version
# rustc 1.85.1 (4eb161250 2025-03-15)

cargo --version
# cargo 1.85.1 (d73d2caf9 2025-01-15)

# Ver donde esta cada binario:
which rustup
# /home/user/.cargo/bin/rustup

which rustc
# /home/user/.cargo/bin/rustc
```

```bash
# Desinstalacion completa:
rustup self uninstall

# Esto elimina:
# - Todas las toolchains instaladas (~/.rustup/)
# - El propio rustup y los proxies (~/.cargo/bin/rustup, rustc, cargo, etc.)
# - Las lineas de PATH agregadas en los archivos de perfil del shell
#
# NO elimina:
# - ~/.cargo/registry/  (crates descargados — cache de dependencias)
# - Proyectos creados por el usuario
# - Binarios instalados con cargo install
#
# Para una limpieza total:
rustup self uninstall
rm -rf ~/.cargo ~/.rustup
```

## Toolchains

Una **toolchain** es un conjunto completo de herramientas para compilar
Rust: el compilador (`rustc`), la biblioteca estandar (`rust-std`),
el gestor de paquetes (`cargo`) y componentes adicionales. rustup
puede gestionar multiples toolchains simultaneamente.

### Canales de release

Rust usa tres canales de release. Los tres avanzan en paralelo:

```bash
# stable — la version de produccion.
# Se publica una nueva version cada 6 semanas.
# Toda feature que llega a stable ha pasado por beta y nightly.
# Garantiza que el codigo que compila hoy seguira compilando
# en futuras versiones stable (backward compatibility).

# beta — la proxima version stable.
# Es un "release candidate". Se usa para detectar regresiones
# antes de que lleguen a stable. Rust pide a los mantenedores
# de crates que prueben con beta para reportar bugs a tiempo.

# nightly — la version de desarrollo.
# Se publica cada noche a partir del branch master.
# Contiene features experimentales (unstable features) que
# requieren #![feature(...)] para activarse.
# Puede romperse, puede cambiar de un dia a otro.
# Es necesaria para usar ciertas herramientas (miri, algunos
# flags de rustfmt, etc.) y features en desarrollo.
```

```rust
// Ejemplo de feature que solo funciona en nightly:
#![feature(generators)]

fn main() {
    let mut gen = || {
        yield 1;
        yield 2;
        return 3;
    };
}

// Si intentas compilar esto con stable:
// error[E0554]: `#![feature]` may not be used on the stable release channel
```

### Nombres de toolchains

Cada toolchain tiene un nombre completo que incluye canal, arquitectura,
vendor y sistema operativo:

```bash
# Formato: <canal>-<arch>-<vendor>-<os>[-<abi>]

# Ejemplos:
# stable-x86_64-unknown-linux-gnu    → stable en Linux x86_64 con glibc
# nightly-x86_64-unknown-linux-gnu   → nightly en Linux x86_64
# beta-aarch64-unknown-linux-gnu     → beta en ARM64 Linux
# stable-x86_64-pc-windows-msvc      → stable en Windows con MSVC
# stable-x86_64-apple-darwin         → stable en macOS x86_64

# Tambien se pueden usar versiones especificas como canal:
# 1.77.0-x86_64-unknown-linux-gnu    → version puntual

# Listar las toolchains instaladas:
rustup toolchain list
# stable-x86_64-unknown-linux-gnu (default)
# nightly-x86_64-unknown-linux-gnu

# Con formato verbose:
rustup toolchain list -v
# Muestra la ruta completa de cada toolchain
```

```bash
# Instalar una toolchain especifica:
rustup toolchain install nightly
rustup toolchain install beta
rustup toolchain install 1.75.0    # version puntual

# Desinstalar:
rustup toolchain uninstall nightly
rustup toolchain uninstall 1.75.0
```

### Default toolchain

La toolchain por defecto es la que se usa cuando ejecutas `rustc` o
`cargo` sin especificar otra:

```bash
# Ver la toolchain por defecto actual:
rustup default
# stable-x86_64-unknown-linux-gnu (default)

# Cambiar la toolchain por defecto:
rustup default nightly
# info: using existing install for 'nightly-x86_64-unknown-linux-gnu'
# info: default toolchain set to 'nightly-x86_64-unknown-linux-gnu'

rustc --version
# rustc 1.87.0-nightly (abc1234 2025-03-17)

# Volver a stable:
rustup default stable
```

### Overrides — toolchain por directorio

Se puede fijar una toolchain para un directorio especifico
(y todos sus subdirectorios). Esto prevalece sobre la default:

```bash
# Fijar nightly para el directorio actual:
cd ~/projects/my_experimental_project
rustup override set nightly

# Ahora, dentro de ese directorio:
rustc --version
# rustc 1.87.0-nightly (...)

# Fuera de ese directorio:
cd ~
rustc --version
# rustc 1.85.1 (...)   ← usa la default (stable)

# Ver overrides configurados:
rustup override list
# /home/user/projects/my_experimental_project    nightly-x86_64-unknown-linux-gnu

# Eliminar el override del directorio actual:
rustup override unset

# Eliminar un override de un directorio especifico:
rustup override unset --path ~/projects/my_experimental_project
```

```bash
# Orden de precedencia de toolchain (de mayor a menor):
#
# 1. Variable de entorno RUSTUP_TOOLCHAIN
#    RUSTUP_TOOLCHAIN=nightly cargo build
#
# 2. Shorthand con + en cargo/rustc
#    cargo +nightly build
#
# 3. Archivo rust-toolchain.toml en el directorio del proyecto
#    (o ancestros — busca hacia arriba)
#
# 4. Override por directorio (rustup override set)
#
# 5. Toolchain default (rustup default)
```

### Ejecutar con una toolchain especifica

Para usar una toolchain puntual sin cambiar la default ni configurar
un override:

```bash
# Sintaxis con rustup run:
rustup run nightly cargo build
rustup run nightly rustc --version
rustup run 1.75.0 cargo test

# Sintaxis abreviada con + (solo para cargo y rustc):
cargo +nightly build
cargo +beta test
cargo +1.75.0 check
rustc +nightly --version

# Ambas formas son equivalentes.
# La sintaxis + es mas comoda para uso diario.
```

```bash
# Caso de uso tipico: probar compatibilidad con versiones anteriores.
cargo +1.70.0 check
cargo +1.75.0 check
cargo +stable check
cargo +nightly check
# Si los cuatro pasan, el crate es compatible con un rango amplio.
```

## Components

Los **components** son las piezas individuales de una toolchain.
No toda toolchain incluye todos los componentes por defecto.
Algunos son opcionales y se instalan bajo demanda.

```bash
# Componentes principales (incluidos por defecto en stable):
# rustc        → el compilador
# cargo        → gestor de paquetes y build system
# rust-std     → biblioteca estandar (incluye core, alloc, std)
# rust-docs    → documentacion local navegable

# Componentes opcionales (se instalan con rustup component add):
# rustfmt      → formateador de codigo (cargo fmt)
# clippy       → linter con cientos de reglas (cargo clippy)
# rust-src     → codigo fuente de la biblioteca estandar
#                (necesario para rust-analyzer y para cross-compilar a
#                 targets sin std precompilada)
# rust-analyzer → servidor LSP para IDEs (autocompletado, etc.)
# miri         → interprete para detectar undefined behavior
#                (solo disponible en nightly)
# llvm-tools   → herramientas LLVM (llvm-profdata, llvm-cov, etc.)
```

```bash
# Listar componentes disponibles para la toolchain activa:
rustup component list
# cargo-x86_64-unknown-linux-gnu (installed)
# clippy-x86_64-unknown-linux-gnu (installed)
# rust-analyzer-x86_64-unknown-linux-gnu
# rust-docs-x86_64-unknown-linux-gnu (installed)
# rust-src
# rust-std-x86_64-unknown-linux-gnu (installed)
# rustc-x86_64-unknown-linux-gnu (installed)
# rustfmt-x86_64-unknown-linux-gnu (installed)
# ...

# Los marcados con (installed) ya estan instalados.
```

```bash
# Instalar componentes:
rustup component add rustfmt
rustup component add clippy
rustup component add rust-src rust-analyzer

# Instalar multiples de una vez:
rustup component add rustfmt clippy rust-analyzer

# Instalar un componente para una toolchain especifica:
rustup component add miri --toolchain nightly

# Desinstalar:
rustup component remove rust-docs
```

```bash
# Verificar que un componente esta disponible y funciona:
rustfmt --version
# rustfmt 1.7.1-stable (4eb16125 2025-03-15)

cargo clippy --version
# clippy 0.1.85 (4eb16125 2025-03-15)

cargo fmt -- --check     # verificar formato sin modificar archivos
cargo clippy             # ejecutar linter
```

```bash
# Problema comun con nightly: componentes no disponibles.
#
# No todos los componentes se compilan exitosamente cada noche.
# Si un componente fallo en la compilacion nocturna, no esta
# disponible para esa fecha de nightly.

rustup component add miri --toolchain nightly
# error: component 'miri' for target 'x86_64-unknown-linux-gnu'
#        is unavailable for download for channel 'nightly'

# Solucion 1: usar una fecha de nightly anterior
rustup toolchain install nightly-2025-03-15
rustup component add miri --toolchain nightly-2025-03-15

# Solucion 2: esperar a la siguiente nightly y reintentar
rustup update nightly
rustup component add miri --toolchain nightly

# Solucion 3: usar rustup-components-history para ver
# que fechas tienen el componente disponible.
# https://rust-lang.github.io/rustup-components-history/
```

## Targets (compilacion cruzada)

Un **target** define la plataforma para la que se genera el codigo
compilado. Por defecto, rustup instala el target de la maquina
host (la maquina donde estas compilando). Para compilar programas
que se ejecuten en **otra** plataforma, necesitas agregar targets
adicionales.

```bash
# Ver el target del host:
rustc --print target-list | head -5
# (imprime la lista completa de targets soportados)

# Ver el target por defecto de tu sistema:
rustup show
# Default host: x86_64-unknown-linux-gnu
# ...

# Formato de un target: <arch>-<vendor>-<os>-<abi>
# x86_64  -unknown-linux-gnu    → Linux 64-bit con glibc
# x86_64  -unknown-linux-musl   → Linux 64-bit con musl (estatico)
# aarch64 -unknown-linux-gnu    → ARM64 Linux
# wasm32  -unknown-unknown      → WebAssembly (sin OS)
# x86_64  -pc-windows-gnu       → Windows 64-bit con MinGW
# x86_64  -apple-darwin         → macOS x86_64
# aarch64 -apple-darwin         → macOS ARM (Apple Silicon)
# thumbv7em-none-eabihf         → ARM Cortex-M (embedded, sin OS)
```

```bash
# Listar targets instalados:
rustup target list --installed
# x86_64-unknown-linux-gnu

# Listar TODOS los targets disponibles:
rustup target list
# (lista larga — Rust soporta docenas de targets)

# Instalar un target:
rustup target add x86_64-unknown-linux-musl
rustup target add wasm32-unknown-unknown
rustup target add aarch64-unknown-linux-gnu

# Instalar para una toolchain especifica:
rustup target add wasm32-unknown-unknown --toolchain nightly

# Desinstalar:
rustup target remove x86_64-unknown-linux-musl
```

```bash
# Compilar para otro target:
cargo build --target x86_64-unknown-linux-musl

# El binario resultante esta en:
# target/x86_64-unknown-linux-musl/debug/mi_programa

# Para release:
cargo build --release --target x86_64-unknown-linux-musl
# target/x86_64-unknown-linux-musl/release/mi_programa
```

### Targets comunes y sus usos

```bash
# 1. musl — binarios estaticos en Linux
#    glibc (gnu) enlaza dinamicamente la libc.
#    musl produce binarios completamente estaticos: no dependen
#    de la version de glibc del sistema destino.
rustup target add x86_64-unknown-linux-musl
cargo build --release --target x86_64-unknown-linux-musl

# Verificar que es estatico:
file target/x86_64-unknown-linux-musl/release/mi_programa
# ELF 64-bit LSB executable, x86-64, statically linked, ...

ldd target/x86_64-unknown-linux-musl/release/mi_programa
# not a dynamic executable

# Util para: distribuir binarios que funcionen en cualquier Linux,
# contenedores Docker FROM scratch (sin libc), etc.
```

```bash
# 2. wasm32 — WebAssembly
#    Compila Rust a WebAssembly para ejecucion en el navegador
#    o en runtimes como wasmtime, wasmer, etc.
rustup target add wasm32-unknown-unknown
cargo build --target wasm32-unknown-unknown

# wasm32-unknown-unknown → sin OS, sin libc
# wasm32-wasi            → con interfaz WASI (acceso a archivos, etc.)
rustup target add wasm32-wasip1
```

```bash
# 3. ARM — cross-compilacion a ARM (Raspberry Pi, servidores ARM, etc.)
rustup target add aarch64-unknown-linux-gnu
cargo build --target aarch64-unknown-linux-gnu

# NOTA IMPORTANTE: agregar el target solo instala la rust-std
# precompilada para esa plataforma. Para compilar programas
# que usen libc, tambien necesitas el LINKER y las bibliotecas
# C de la plataforma destino.

# En Debian/Ubuntu, instalar el toolchain de cross-compilacion:
sudo apt install gcc-aarch64-linux-gnu

# Y configurar cargo para usar ese linker (en .cargo/config.toml):
# [target.aarch64-unknown-linux-gnu]
# linker = "aarch64-linux-gnu-gcc"
```

```bash
# 4. Embedded (sin OS)
#    Para microcontroladores (no_std — sin biblioteca estandar).
rustup target add thumbv7em-none-eabihf

# En este caso el programa no puede usar std (no hay OS).
# Solo puede usar core y alloc.
# El programa se marca con #![no_std] y #![no_main].
```

## Actualizacion

```bash
# Actualizar TODAS las toolchains instaladas y rustup mismo:
rustup update
# info: syncing channel updates for 'stable-x86_64-unknown-linux-gnu'
# info: syncing channel updates for 'nightly-x86_64-unknown-linux-gnu'
# info: checking for self-update
#
#   stable-x86_64-unknown-linux-gnu updated - rustc 1.85.1
#   nightly-x86_64-unknown-linux-gnu updated - rustc 1.87.0-nightly
#

# Actualizar solo un canal:
rustup update stable
rustup update nightly

# Actualizar solo rustup (sin actualizar toolchains):
rustup self update

# Nota: rustup update normalmente incluye self update tambien.
# Pero si solo quieres actualizar la herramienta rustup sin
# tocar las toolchains, usas self update.
```

```bash
# Ver que se actualizaria sin instalar nada:
rustup check
# stable-x86_64-unknown-linux-gnu - Up to date : 1.85.1
# nightly-x86_64-unknown-linux-gnu - Update available : 1.87.0-nightly
```

## Comandos utiles

```bash
# Resumen completo del estado actual de rustup:
rustup show
# Default host: x86_64-unknown-linux-gnu
# rustup home:  /home/user/.rustup
#
# installed toolchains
# --------------------
# stable-x86_64-unknown-linux-gnu (default)
# nightly-x86_64-unknown-linux-gnu
#
# installed targets for active toolchain
# --------------------------------------
# x86_64-unknown-linux-gnu
# x86_64-unknown-linux-musl
# wasm32-unknown-unknown
#
# active toolchain
# ----------------
# stable-x86_64-unknown-linux-gnu (default)
# rustc 1.85.1 (4eb161250 2025-03-15)
```

```bash
# Averiguar la ruta real del binario que se usa:
rustup which rustc
# /home/user/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/rustc

rustup which cargo
# /home/user/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/cargo

# Util para diagnosticar problemas: si rustc no es el esperado,
# which muestra exactamente cual se esta usando.
```

```bash
# Abrir la documentacion local en el navegador:
rustup doc

# Abrir directamente la documentacion de la biblioteca estandar:
rustup doc --std

# Abrir la documentacion de un modulo o tipo especifico:
rustup doc --std std::collections::HashMap

# Abrir The Rust Programming Language (el libro oficial):
rustup doc --book

# Abrir Rust by Example:
rustup doc --rust-by-example

# Abrir la referencia del lenguaje:
rustup doc --reference

# Toda esta documentacion esta instalada localmente.
# No necesitas conexion a internet para consultarla.
```

```bash
# Listar toolchains instaladas:
rustup toolchain list
# stable-x86_64-unknown-linux-gnu (default)
# nightly-x86_64-unknown-linux-gnu

# Ver la home de rustup (donde se guardan las toolchains):
rustup show home
# /home/user/.rustup
```

## rust-toolchain.toml

El archivo `rust-toolchain.toml` se coloca en la raiz de un
proyecto para **fijar la toolchain y sus componentes**. Cuando
alguien clona el repositorio y ejecuta `cargo build`, rustup
detecta el archivo, instala automaticamente la toolchain
especificada (si no la tiene) e instala los componentes y targets
declarados.

```toml
# rust-toolchain.toml (en la raiz del proyecto)

[toolchain]
channel = "1.77.0"
components = ["rustfmt", "clippy"]
targets = ["wasm32-unknown-unknown"]
```

```bash
# Con ese archivo, al hacer cargo build en el proyecto:
# 1. rustup detecta rust-toolchain.toml
# 2. Si la toolchain 1.77.0 no esta instalada, la descarga
# 3. Instala los componentes rustfmt y clippy si faltan
# 4. Instala el target wasm32-unknown-unknown si falta
# 5. Usa esa toolchain para la compilacion
#
# Todo esto es automatico y transparente.
```

```toml
# Variantes comunes de rust-toolchain.toml:

# --- Proyecto que necesita nightly por features inestables ---
[toolchain]
channel = "nightly-2025-03-15"
components = ["rustfmt", "clippy", "miri", "rust-src"]

# Fijar una fecha de nightly garantiza reproducibilidad.
# Todos los desarrolladores usan exactamente la misma nightly.
```

```toml
# --- Proyecto de produccion que fija version estable exacta ---
[toolchain]
channel = "1.85.1"
components = ["rustfmt", "clippy", "rust-analyzer"]
```

```toml
# --- Proyecto que compila para multiples targets ---
[toolchain]
channel = "stable"
components = ["rustfmt", "clippy"]
targets = ["x86_64-unknown-linux-musl", "wasm32-unknown-unknown", "aarch64-unknown-linux-gnu"]
```

```bash
# Tambien existe el formato legacy rust-toolchain (sin .toml).
# Es un archivo de texto plano con solo el nombre del canal:

# Contenido de rust-toolchain (sin extension):
# nightly

# Este formato es mas limitado: no permite especificar
# componentes ni targets. Se recomienda rust-toolchain.toml.
#
# Si ambos archivos existen, rust-toolchain.toml tiene prioridad.
```

```bash
# Diferencias entre override y rust-toolchain.toml:
#
# rustup override set:
# - Se guarda en la configuracion LOCAL de rustup (~/.rustup/settings.toml)
# - No se comparte con el equipo (no esta en el repositorio)
# - Se asocia al directorio, no al proyecto
#
# rust-toolchain.toml:
# - Se guarda EN EL REPOSITORIO (se comparte con todo el equipo)
# - Cualquiera que clone el proyecto obtiene la misma toolchain
# - Especifica componentes y targets ademas del canal
# - Es la forma recomendada para proyectos colaborativos
```

## Estructura de directorios de rustup

```bash
# Donde vive todo:
#
# ~/.cargo/
# ├── bin/           → proxies (rustc, cargo, rustup, etc.)
# ├── registry/      → cache de crates descargados
# ├── git/           → crates descargados desde git
# └── env            → script que agrega bin/ al PATH
#
# ~/.rustup/
# ├── toolchains/    → las toolchains reales
# │   ├── stable-x86_64-unknown-linux-gnu/
# │   │   ├── bin/   → rustc, cargo, etc. (los binarios reales)
# │   │   ├── lib/   → bibliotecas
# │   │   └── ...
# │   └── nightly-x86_64-unknown-linux-gnu/
# │       └── ...
# ├── update-hashes/ → hashes para detectar actualizaciones
# └── settings.toml  → configuracion de rustup (default, overrides)
```

```bash
# Variables de entorno relevantes:

# RUSTUP_HOME — cambia la ubicacion de ~/.rustup
# Por defecto: ~/.rustup
export RUSTUP_HOME=/opt/rustup

# CARGO_HOME — cambia la ubicacion de ~/.cargo
# Por defecto: ~/.cargo
export CARGO_HOME=/opt/cargo

# RUSTUP_TOOLCHAIN — fuerza una toolchain (prioridad maxima)
RUSTUP_TOOLCHAIN=nightly cargo build

# RUSTUP_DIST_SERVER — mirror alternativo para descargas
# Util si el servidor oficial es lento en tu region.
```

---

## Ejercicios

### Ejercicio 1 — Instalacion y exploracion

```bash
# 1. Verificar que rustup, rustc y cargo estan instalados.
#    Obtener la version de cada uno.
#
# 2. Ejecutar rustup show y analizar la salida:
#    - Cual es el host?
#    - Que toolchains estan instaladas?
#    - Que targets hay para la toolchain activa?
#
# 3. Ejecutar rustup which rustc y comparar con which rustc.
#    Explicar por que which rustc muestra ~/.cargo/bin/rustc
#    (el proxy) mientras que rustup which rustc muestra el
#    binario real dentro de ~/.rustup/toolchains/.
#
# 4. Abrir la documentacion local con rustup doc --book.
#    Verificar que funciona sin conexion a internet.
```

### Ejercicio 2 — Toolchains y overrides

```bash
# 1. Instalar la toolchain nightly:
#    rustup toolchain install nightly
#
# 2. Verificar con rustup toolchain list que aparece.
#
# 3. Comparar versiones:
#    rustc +stable --version
#    rustc +nightly --version
#
# 4. Crear un directorio temporal y configurar un override:
#    mkdir /tmp/rust-nightly-test && cd /tmp/rust-nightly-test
#    rustup override set nightly
#    rustc --version    # debe mostrar nightly
#    cd ~
#    rustc --version    # debe mostrar stable
#
# 5. Verificar con rustup override list.
#    Limpiar con rustup override unset --path /tmp/rust-nightly-test
#
# 6. Crear un archivo rust-toolchain.toml en /tmp/rust-nightly-test:
#    [toolchain]
#    channel = "nightly"
#    components = ["rustfmt", "clippy"]
#
#    Entrar al directorio y verificar que rustc --version muestra nightly.
```

### Ejercicio 3 — Componentes y targets

```bash
# 1. Listar los componentes instalados:
#    rustup component list | grep installed
#
# 2. Si no tienes clippy y rustfmt, instalarlos:
#    rustup component add clippy rustfmt
#
# 3. Crear un proyecto minimo y verificar que funcionan:
#    cargo new /tmp/component-test && cd /tmp/component-test
#    cargo fmt -- --check    # verificar formato
#    cargo clippy             # ejecutar linter
#
# 4. Instalar el target musl y compilar el proyecto como binario estatico:
#    rustup target add x86_64-unknown-linux-musl
#    cargo build --target x86_64-unknown-linux-musl
#    file target/x86_64-unknown-linux-musl/debug/component-test
#    # Verificar que dice "statically linked"
#
# 5. Listar los targets instalados y verificar que aparece musl:
#    rustup target list --installed
```

### Ejercicio 4 — rust-toolchain.toml en un proyecto

```bash
# 1. Crear un proyecto nuevo:
#    cargo new /tmp/pinned-project && cd /tmp/pinned-project
#
# 2. Crear un archivo rust-toolchain.toml que:
#    - Fije el canal a una version estable especifica (ej. "1.80.0")
#    - Incluya los componentes rustfmt y clippy
#
# 3. Ejecutar cargo build. Observar que rustup descarga la
#    toolchain 1.80.0 automaticamente si no la tenia.
#
# 4. Verificar que rustc --version (dentro del directorio)
#    muestra la version fijada, no la default.
#
# 5. Fuera del directorio, verificar que rustc --version
#    muestra la toolchain default.
#
# 6. Reflexionar: por que es buena practica incluir
#    rust-toolchain.toml en el repositorio git de un proyecto
#    colaborativo? Que problemas previene?
```
