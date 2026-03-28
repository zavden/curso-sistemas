# Workspaces: múltiples crates en un solo proyecto

## Índice
1. [Qué es un workspace](#1-qué-es-un-workspace)
2. [Crear un workspace](#2-crear-un-workspace)
3. [Cargo.toml raíz](#3-cargotoml-raíz)
4. [members y exclude](#4-members-y-exclude)
5. [Dependencias entre crates del workspace](#5-dependencias-entre-crates-del-workspace)
6. [Dependencias compartidas](#6-dependencias-compartidas)
7. [workspace.dependencies (herencia)](#7-workspacedependencies-herencia)
8. [Compilación y comandos Cargo](#8-compilación-y-comandos-cargo)
9. [El Cargo.lock compartido](#9-el-cargolock-compartido)
10. [Estructura de un workspace real](#10-estructura-de-un-workspace-real)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué es un workspace

Un **workspace** es un conjunto de crates (paquetes) que se gestionan juntos. Comparten un único `Cargo.lock`, un directorio `target/` común, y pueden depender unos de otros fácilmente:

```text
mi_proyecto/
├── Cargo.toml          ← workspace root (define los miembros)
├── Cargo.lock          ← compartido por todos los miembros
├── target/             ← compilación compartida
├── crate_a/
│   ├── Cargo.toml      ← miembro del workspace
│   └── src/
│       └── lib.rs
├── crate_b/
│   ├── Cargo.toml      ← miembro del workspace
│   └── src/
│       └── main.rs
└── crate_c/
    ├── Cargo.toml      ← miembro del workspace
    └── src/
        └── lib.rs
```

### ¿Por qué usar un workspace?

| Sin workspace | Con workspace |
|---------------|---------------|
| Cada crate tiene su `Cargo.lock` | Un solo `Cargo.lock` — versiones coherentes |
| Cada crate compila en su `target/` | Un solo `target/` — sin recompilar duplicados |
| Dependencias entre crates vía git/crates.io | Dependencias locales con `path = "..."` |
| `cargo test` corre tests de un solo crate | `cargo test` corre tests de todos los miembros |
| Versiones de dependencias pueden diverger | Misma versión de serde, tokio, etc. en todos |

### Cuándo crear un workspace

- Tienes una **library + CLI** (el CLI usa la library).
- Tienes una **aplicación + componentes separados** (server, client, shared types).
- Tienes **múltiples binarios** que comparten código.
- Tu proyecto crece y quieres separar responsabilidades sin repositorios separados.

---

## 2. Crear un workspace

### Desde cero

```bash
# Crear directorio del workspace
mkdir mi_workspace && cd mi_workspace

# Crear el Cargo.toml raíz
cat > Cargo.toml << 'EOF'
[workspace]
members = [
    "core",
    "cli",
    "server",
]
resolver = "2"
EOF

# Crear los crates miembros
cargo init core --lib
cargo init cli
cargo init server
```

Resultado:

```text
mi_workspace/
├── Cargo.toml          ← [workspace]
├── core/
│   ├── Cargo.toml      ← [package] name = "core"
│   └── src/lib.rs
├── cli/
│   ├── Cargo.toml      ← [package] name = "cli"
│   └── src/main.rs
└── server/
    ├── Cargo.toml      ← [package] name = "server"
    └── src/main.rs
```

### Desde un proyecto existente

Si ya tienes un crate y quieres convertirlo en workspace:

```bash
# Estructura actual:
# mi_app/
# ├── Cargo.toml      ← [package] existente
# └── src/main.rs

# 1. Mover el crate a un subdirectorio
mkdir app
mv src app/
mv Cargo.toml app/

# 2. Crear el Cargo.toml raíz del workspace
cat > Cargo.toml << 'EOF'
[workspace]
members = ["app"]
resolver = "2"
EOF

# 3. Crear nuevos crates según necesites
cargo init shared --lib
# Añadir "shared" a members
```

---

## 3. Cargo.toml raíz

El `Cargo.toml` raíz de un workspace contiene la sección `[workspace]` y **no** contiene `[package]` (a menos que sea un workspace virtual con crate raíz):

### Workspace virtual (sin crate raíz) — recomendado

```toml
# Cargo.toml (raíz)
[workspace]
members = [
    "core",
    "cli",
    "server",
]
resolver = "2"
```

No hay `[package]` — el directorio raíz solo es el workspace, no un crate. Esta es la forma más clara y recomendada.

### Workspace con crate raíz

También es posible que el propio crate raíz sea un miembro:

```toml
# Cargo.toml (raíz)
[package]
name = "mi_app"
version = "0.1.0"
edition = "2021"

[workspace]
members = [
    "core",
    "utils",
]

[dependencies]
core = { path = "core" }
utils = { path = "utils" }
```

Aquí el directorio raíz es tanto el workspace como un crate (`mi_app`). Esto es menos común y puede generar confusión — se prefiere el workspace virtual.

### resolver = "2"

```toml
[workspace]
resolver = "2"  # obligatorio para edición 2021+
```

El resolver "2" maneja correctamente features condicionales por plataforma y por perfil (dev vs release). Si usas edición 2021 o superior en cualquier miembro, Cargo exige `resolver = "2"`.

---

## 4. members y exclude

### members: declarar miembros

`members` lista los crates que forman parte del workspace. Acepta paths y globs:

```toml
[workspace]
members = [
    "core",
    "cli",
    "server",
    "plugins/*",       # todos los subdirectorios de plugins/
    "crates/*",        # patrón común: crates/foo, crates/bar, etc.
]
```

Con globs:

```text
mi_workspace/
├── Cargo.toml
├── core/
├── cli/
├── crates/
│   ├── auth/          ← incluido por "crates/*"
│   ├── db/            ← incluido por "crates/*"
│   └── utils/         ← incluido por "crates/*"
└── plugins/
    ├── json/          ← incluido por "plugins/*"
    └── csv/           ← incluido por "plugins/*"
```

### exclude: excluir directorios

`exclude` evita que ciertos paths sean considerados miembros, incluso si coinciden con un glob:

```toml
[workspace]
members = ["crates/*"]
exclude = [
    "crates/experimental",   # no incluir este
    "crates/deprecated",     # ni este
]
```

### default-members

`default-members` define qué crates afectan los comandos de Cargo cuando se ejecutan desde la raíz sin `-p`:

```toml
[workspace]
members = ["core", "cli", "server", "benchmarks"]
default-members = ["core", "cli", "server"]
# cargo build desde la raíz NO compila benchmarks
# Para compilar benchmarks: cargo build -p benchmarks
```

---

## 5. Dependencias entre crates del workspace

Los miembros del workspace se referencian entre sí con `path`:

```toml
# cli/Cargo.toml
[package]
name = "cli"
version = "0.1.0"
edition = "2021"

[dependencies]
core = { path = "../core" }  # dependencia local
```

```rust
// cli/src/main.rs
use core::process;  // usa la library "core"

fn main() {
    process::run();
}
```

### Múltiples dependencias internas

```toml
# server/Cargo.toml
[package]
name = "server"
version = "0.1.0"
edition = "2021"

[dependencies]
core = { path = "../core" }
utils = { path = "../utils" }
```

### Nombre del crate vs nombre del directorio

El nombre del crate es el `name` en su `Cargo.toml`, no el nombre del directorio:

```toml
# crates/my-core/Cargo.toml
[package]
name = "my_core"  # ← este es el nombre para use/import
```

```toml
# cli/Cargo.toml
[dependencies]
my_core = { path = "../crates/my-core" }  # usa el name, no el directorio
```

```rust
// cli/src/main.rs
use my_core::something;  // el nombre del crate en código
```

### Dependencias cíclicas

Cargo **no permite** dependencias cíclicas, ni siquiera dentro de un workspace:

```text
A depende de B, B depende de A → ERROR
```

Si necesitas tipos compartidos entre A y B, extráelos a un crate C del que ambos dependan:

```text
ANTES (ciclo):             DESPUÉS (sin ciclo):
A ←→ B                    A → C ← B
```

---

## 6. Dependencias compartidas

### El problema sin workspace.dependencies

Sin centralizar versiones, cada crate especifica la suya:

```toml
# core/Cargo.toml
[dependencies]
serde = { version = "1.0.193", features = ["derive"] }
tokio = { version = "1.35", features = ["full"] }

# cli/Cargo.toml
[dependencies]
serde = { version = "1.0.190", features = ["derive"] }  # ¡versión distinta!
tokio = { version = "1.35", features = ["rt"] }

# server/Cargo.toml
[dependencies]
serde = { version = "1.0.193", features = ["derive"] }
tokio = { version = "1.35", features = ["full"] }
```

Problemas:
- Versiones inconsistentes entre miembros.
- Actualizar requiere cambiar N archivos.
- Features duplicadas o inconsistentes.

---

## 7. workspace.dependencies (herencia)

Desde Cargo 1.64, puedes centralizar versiones en el `Cargo.toml` raíz:

### Declarar en el workspace raíz

```toml
# Cargo.toml (raíz)
[workspace]
members = ["core", "cli", "server"]
resolver = "2"

[workspace.dependencies]
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
tokio = { version = "1.35", features = ["full"] }
anyhow = "1.0"
clap = { version = "4.4", features = ["derive"] }
tracing = "0.1"

# Crates internos del workspace también se declaran aquí:
core = { path = "core" }
```

### Usar en los miembros con workspace = true

```toml
# core/Cargo.toml
[package]
name = "core"
version = "0.1.0"
edition = "2021"

[dependencies]
serde.workspace = true           # hereda version y features del workspace
serde_json.workspace = true
tokio.workspace = true
anyhow.workspace = true
```

```toml
# cli/Cargo.toml
[package]
name = "cli"
version = "0.1.0"
edition = "2021"

[dependencies]
core.workspace = true            # crate interno
clap.workspace = true
anyhow.workspace = true
serde.workspace = true
tracing.workspace = true
```

```toml
# server/Cargo.toml
[package]
name = "server"
version = "0.1.0"
edition = "2021"

[dependencies]
core.workspace = true
tokio.workspace = true
serde.workspace = true
serde_json.workspace = true
tracing.workspace = true
```

### Sobrescribir features en un miembro

Un miembro puede añadir features adicionales a las del workspace:

```toml
# workspace root
[workspace.dependencies]
tokio = { version = "1.35" }  # sin features por defecto

# server/Cargo.toml — necesita más features
[dependencies]
tokio = { workspace = true, features = ["full"] }

# cli/Cargo.toml — necesita menos features
[dependencies]
tokio = { workspace = true, features = ["rt", "macros"] }
```

Las features declaradas en el miembro se **suman** a las del workspace (no las reemplazan).

### También para dev-dependencies y build-dependencies

```toml
# Cargo.toml raíz
[workspace.dependencies]
criterion = { version = "0.5", features = ["html_reports"] }
tempfile = "3.8"

# core/Cargo.toml
[dev-dependencies]
criterion.workspace = true
tempfile.workspace = true
```

---

## 8. Compilación y comandos Cargo

### Compilar todo el workspace

```bash
# Desde la raíz del workspace:
cargo build           # compila todos los miembros (o default-members)
cargo test            # ejecuta tests de todos los miembros
cargo check           # verifica todos los miembros
cargo clippy          # lint de todos los miembros
```

### Compilar un miembro específico

```bash
cargo build -p core          # solo el crate "core"
cargo test -p cli            # tests solo de "cli"
cargo run -p server          # ejecutar el binario "server"
cargo check -p core -p cli   # verificar dos crates específicos
```

### Ejecutar desde un subdirectorio

```bash
cd cli/
cargo run          # ejecuta el crate del directorio actual
cargo test         # tests del crate actual
# Cargo detecta que está dentro de un workspace
# y usa el Cargo.lock y target/ de la raíz
```

### Compilar en release

```bash
cargo build --release    # todos los miembros en release
cargo build --release -p server  # solo server en release
```

### Tests con filtros

```bash
cargo test                          # todos los tests de todos los crates
cargo test -p core                  # tests solo de core
cargo test -p core -- test_parse    # tests de core que contengan "test_parse"
cargo test --workspace              # explícitamente todos los miembros
```

### Documentación

```bash
cargo doc                    # genera docs para todos los miembros
cargo doc -p core --open     # docs de core, abrir en navegador
```

---

## 9. El Cargo.lock compartido

Todos los miembros del workspace comparten un **único `Cargo.lock`** en la raíz. Esto garantiza:

1. **Reproducibilidad**: todos los miembros usan exactamente las mismas versiones de dependencias.
2. **Coherencia**: si `core` y `server` dependen de `serde 1.0`, ambos usan la misma versión exacta.
3. **Eficiencia**: las dependencias compartidas se compilan una sola vez.

### El directorio target/ compartido

```text
mi_workspace/
├── target/                  ← compilación de TODOS los miembros
│   ├── debug/
│   │   ├── cli              ← binario del crate cli
│   │   ├── server           ← binario del crate server
│   │   ├── libcore.rlib     ← library del crate core
│   │   └── deps/            ← dependencias compiladas (compartidas)
│   └── release/
└── ...
```

**Ventaja**: si `core` y `server` dependen de `serde`, serde se compila **una vez** y se reutiliza. Sin workspace, cada crate compilaría serde independientemente.

---

## 10. Estructura de un workspace real

### Proyecto web típico

```text
my_web_app/
├── Cargo.toml                    ← workspace root
├── Cargo.lock
├── crates/
│   ├── api/                      ← servidor HTTP
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── main.rs
│   │       ├── routes/
│   │       └── middleware/
│   ├── core/                     ← lógica de negocio
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── models/
│   │       └── services/
│   ├── db/                       ← acceso a base de datos
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── migrations/
│   │       └── queries/
│   ├── cli/                      ← herramienta de admin
│   │   ├── Cargo.toml
│   │   └── src/main.rs
│   └── shared/                   ← tipos compartidos
│       ├── Cargo.toml
│       └── src/
│           ├── lib.rs
│           ├── types.rs
│           └── errors.rs
└── README.md
```

```toml
# Cargo.toml (raíz)
[workspace]
members = ["crates/*"]
resolver = "2"

[workspace.dependencies]
# Dependencias externas
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
tokio = { version = "1.35", features = ["full"] }
sqlx = { version = "0.7", features = ["runtime-tokio", "postgres"] }
axum = "0.7"
clap = { version = "4.4", features = ["derive"] }
anyhow = "1.0"
tracing = "0.1"
tracing-subscriber = "0.3"

# Crates internos
shared = { path = "crates/shared" }
core = { path = "crates/core" }
db = { path = "crates/db" }
```

```toml
# crates/api/Cargo.toml
[package]
name = "api"
version = "0.1.0"
edition = "2021"

[dependencies]
shared.workspace = true
core.workspace = true
db.workspace = true
axum.workspace = true
tokio.workspace = true
serde.workspace = true
serde_json.workspace = true
tracing.workspace = true
```

### Grafo de dependencias

```text
         ┌──────────┐
         │  shared   │ ← tipos, errores (sin dependencias internas)
         └──────────┘
           ↑      ↑
    ┌──────┘      └──────┐
    │                     │
┌───┴──┐            ┌────┴─┐
│  db  │            │ core │
└──────┘            └──────┘
    ↑                  ↑
    │    ┌─────────────┤
    │    │             │
┌───┴────┴──┐     ┌───┴──┐
│    api    │     │  cli  │
└───────────┘     └──────┘
```

Regla: las dependencias fluyen **hacia abajo** (api/cli → core/db → shared). Nunca hacia arriba ni en ciclo.

---

## 11. Errores comunes

### Error 1: Olvidar añadir el miembro a members

```bash
cargo init crates/new_crate --lib
# Crear el crate no lo añade al workspace automáticamente
```

```
error: current package believes it's in a workspace when it's not
```

**Solución**: añadir a `members` en el Cargo.toml raíz:

```toml
[workspace]
members = [
    "crates/*",  # si usas glob, los nuevos se incluyen automáticamente
]
```

### Error 2: Cargo.toml raíz con [package] y sin [workspace]

```toml
# Cargo.toml raíz
[package]
name = "mi_app"
version = "0.1.0"

# Olvidó la sección [workspace]
# Los subdirectorios no son parte de ningún workspace
```

Si tienes crates en subdirectorios, necesitas `[workspace]` con `members`.

### Error 3: Versiones inconsistentes sin workspace.dependencies

```toml
# core/Cargo.toml
[dependencies]
serde = "1.0.193"

# cli/Cargo.toml
[dependencies]
serde = "1.0.150"    # versión distinta → Cargo puede compilar dos versiones
```

Aunque Cargo.lock unifica cuando es posible, especificar versiones distintas puede causar que se compilen múltiples versiones de la misma dependencia.

**Solución**: usar `[workspace.dependencies]` para centralizar.

### Error 4: Dependencia cíclica entre miembros

```toml
# core/Cargo.toml
[dependencies]
utils = { path = "../utils" }

# utils/Cargo.toml
[dependencies]
core = { path = "../core" }   # ¡ciclo!
```

```
error: cyclic package dependency
```

**Solución**: extraer los tipos compartidos a un tercer crate:

```text
core → shared ← utils
```

### Error 5: Confundir workspace = true con definir la versión

```toml
# Cargo.toml raíz
[workspace.dependencies]
serde = "1.0"

# miembro/Cargo.toml
[dependencies]
serde = { workspace = true, version = "1.0" }  # ERROR: no puedes poner version con workspace
```

```
error: `workspace` cannot be combined with `version`
```

Con `workspace = true` no puedes especificar `version`, `registry`, ni `path` (ya están definidos en la raíz). Solo puedes añadir `features` y `optional`.

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                      WORKSPACES CHEATSHEET                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA MÍNIMA                                                 ║
║  proyecto/                                                         ║
║  ├── Cargo.toml        [workspace] members = ["crate_a","crate_b"] ║
║  ├── crate_a/Cargo.toml   [package] + [dependencies]               ║
║  └── crate_b/Cargo.toml   [package] + [dependencies]               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CARGO.TOML RAÍZ                                                   ║
║  [workspace]                                                       ║
║  members = ["crate_a", "crate_b", "crates/*"]                      ║
║  exclude = ["crates/experimental"]                                 ║
║  default-members = ["crate_a"]                                     ║
║  resolver = "2"                 Obligatorio para edición 2021+     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DEPENDENCIAS INTERNAS                                             ║
║  # En miembro/Cargo.toml:                                          ║
║  [dependencies]                                                    ║
║  otro_crate = { path = "../otro_crate" }                           ║
║  # o con workspace.dependencies:                                   ║
║  otro_crate.workspace = true                                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  WORKSPACE.DEPENDENCIES (centralizar versiones)                    ║
║  # En Cargo.toml raíz:                                             ║
║  [workspace.dependencies]                                          ║
║  serde = { version = "1.0", features = ["derive"] }                ║
║  mi_lib = { path = "crates/mi_lib" }                               ║
║                                                                    ║
║  # En miembro/Cargo.toml:                                          ║
║  [dependencies]                                                    ║
║  serde.workspace = true                                            ║
║  serde = { workspace = true, features = ["extra"] }  # + features  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMANDOS CARGO                                                    ║
║  cargo build                  Todo el workspace                    ║
║  cargo build -p crate_a      Solo crate_a                          ║
║  cargo test --workspace       Tests de todos los miembros          ║
║  cargo test -p crate_a       Tests solo de crate_a                 ║
║  cargo run -p mi_bin          Ejecutar un binario específico       ║
║  cargo clippy --workspace    Lint de todo                           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  COMPARTIDOS                                                       ║
║  Cargo.lock   → versiones exactas unificadas                       ║
║  target/      → compilación reutilizada entre miembros             ║
║                                                                    ║
║  REGLAS                                                            ║
║  ✗ No hay dependencias cíclicas                                    ║
║  ✗ workspace = true no se combina con version                      ║
║  ✓ Un crate debe estar en members para ser miembro                 ║
║  ✓ Globs en members para escalar fácilmente                        ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Crear un workspace desde cero

Crea un workspace llamado `task_manager` con tres crates:

```text
task_manager/
├── Cargo.toml               ← workspace virtual
├── crates/
│   ├── core/                ← library: Task, Project, TaskStatus
│   │   ├── Cargo.toml
│   │   └── src/lib.rs
│   ├── storage/             ← library: guardar/cargar tasks (usa serde_json)
│   │   ├── Cargo.toml
│   │   └── src/lib.rs
│   └── cli/                 ← binary: interfaz de línea de comandos
│       ├── Cargo.toml
│       └── src/main.rs
```

**Requisitos:**
1. El Cargo.toml raíz debe usar `[workspace.dependencies]` para `serde`, `serde_json`, y los crates internos.
2. `core` no depende de nada externo (solo tipos y lógica).
3. `storage` depende de `core`, `serde`, y `serde_json`.
4. `cli` depende de `core` y `storage`.
5. Todos usan `edition = "2021"`.

**Escribe los 4 archivos `Cargo.toml` y un `lib.rs`/`main.rs` mínimo que compile.**

**Preguntas:**
1. ¿Por qué `core` no necesita `serde` si sus tipos se serializan en `storage`?
2. ¿Qué pasa si `core` necesita `#[derive(Serialize)]`? ¿Cómo lo manejas sin que `core` dependa de serde?

### Ejercicio 2: Migrar a workspace.dependencies

Convierte estos Cargo.toml independientes a usar `workspace.dependencies`. Identifica las inconsistencias de versión y resuélvelas:

```toml
# crates/api/Cargo.toml
[dependencies]
tokio = { version = "1.34", features = ["full"] }
serde = { version = "1.0.190", features = ["derive"] }
serde_json = "1.0.107"
tracing = "0.1.40"

# crates/worker/Cargo.toml
[dependencies]
tokio = { version = "1.35", features = ["rt", "macros"] }
serde = { version = "1.0.193", features = ["derive"] }
serde_json = "1.0.108"
tracing = "0.1.37"

# crates/shared/Cargo.toml
[dependencies]
serde = { version = "1.0.193", features = ["derive"] }
```

**Tareas:**
1. Escribe la sección `[workspace.dependencies]` unificada (usa la versión más reciente).
2. Reescribe cada `Cargo.toml` de miembro usando `workspace = true`.
3. ¿Cómo manejas que `api` necesita `tokio` con `features = ["full"]` pero `worker` solo necesita `["rt", "macros"]`?

### Ejercicio 3: Diseñar el grafo de dependencias

Tienes un sistema de e-commerce con estos componentes. Diseña el workspace, decide los crates, y dibuja el grafo de dependencias (quién depende de quién):

**Componentes:**
- Tipos compartidos: `Product`, `Order`, `User`, `Money`
- Lógica de negocio: pricing, inventory, order processing
- Base de datos: queries, migrations, connection pool
- API REST: routes, middleware, authentication
- Worker de background: procesar pagos, enviar emails
- CLI de administración: seed data, run migrations

**Preguntas:**
1. ¿Cuántos crates crearías? ¿Cómo los nombrarías?
2. ¿Qué crate no depende de ningún otro crate interno?
3. ¿Qué crates tienen más dependencias internas?
4. ¿Hay riesgo de dependencia cíclica en tu diseño? ¿Dónde?
5. ¿Qué dependencias externas centralizarías en `workspace.dependencies`?

---

> **Nota**: los workspaces son la forma estándar de organizar proyectos Rust no triviales. Prácticamente todo proyecto del ecosistema (tokio, serde, bevy, rustc mismo) usa workspaces. Dominar su configuración te permite escalar proyectos manteniendo compilaciones rápidas y dependencias coherentes.
