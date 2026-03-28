# T04 — Herramientas auxiliares

## Panorama general

El toolchain de Rust incluye herramientas auxiliares que complementan
al compilador y a Cargo. Las cuatro principales son:

- **rustfmt** — formateador oficial de codigo
- **clippy** — linter oficial (analisis estatico)
- **cargo-expand** — expansion de macros
- **rust-analyzer** — servidor de lenguaje (LSP)

Estas herramientas no son opcionales en la practica profesional.
Un proyecto Rust serio usa las cuatro. rustfmt y clippy suelen
ejecutarse en CI como checks obligatorios.

```bash
# Instalacion de las principales:
rustup component add rustfmt
rustup component add clippy
rustup component add rust-analyzer
cargo install cargo-expand
```

## rustfmt — Formateador de codigo

rustfmt aplica un estilo de formato consistente al codigo Rust.
Elimina debates sobre estilo: todo el equipo usa el mismo formato,
automaticamente. El estilo por defecto sigue la guia oficial
de estilo de Rust (Rust Style Guide).

```bash
# Formatear todo el proyecto (modifica los archivos en sitio):
cargo fmt

# Formatear solo un archivo especifico:
rustfmt src/main.rs

# Verificar sin modificar (exit code 1 si hay diferencias).
# Ideal para CI — el pipeline falla si alguien no formateo:
cargo fmt -- --check

# Salida cuando hay diferencias:
# Diff in /home/user/project/src/main.rs at line 5:
#  fn main() {
# -    let x=1+2;
# +    let x = 1 + 2;
#  }
```

### Configuracion de rustfmt

rustfmt busca `rustfmt.toml` o `.rustfmt.toml` en la raiz del
proyecto. Si no encuentra ninguno, usa los valores por defecto:

```toml
# rustfmt.toml

# Ancho maximo de linea (default: 100).
max_width = 100

# Espacios por nivel de indentacion (default: 4).
tab_spaces = 4

# Edicion de Rust (default: "2015"). Debe coincidir con Cargo.toml.
edition = "2021"

# Heuristica para partir lineas. "Off" desactiva, "Max" es agresivo.
use_small_heuristics = "Default"

# Granularidad de imports: "Crate", "Module", "Item", "Preserve".
imports_granularity = "Crate"

# Ordenamiento de imports: "StdExternalCrate" agrupa std, externos, locales.
group_imports = "StdExternalCrate"
```

```rust
// Efecto de imports_granularity = "Crate":

// ANTES:
use std::collections::HashMap;
use std::collections::HashSet;
use std::io::Read;
use std::io::Write;

// DESPUES de cargo fmt:
use std::collections::{HashMap, HashSet};
use std::io::{Read, Write};
```

```rust
// Efecto de group_imports = "StdExternalCrate":

// ANTES (mezclados):
use serde::Serialize;
use std::fs;
use crate::utils::parse;
use anyhow::Result;

// DESPUES de cargo fmt:
use std::fs;                    // 1. std

use anyhow::Result;             // 2. crates externos
use serde::Serialize;

use crate::utils::parse;        // 3. crate local
```

### Saltar el formateo

```rust
// #[rustfmt::skip] — saltar el formateo de un item especifico:
#[rustfmt::skip]
fn transformation_matrix() -> [[f64; 3]; 3] {
    [
        [ 1.0,  0.0,  0.0],
        [ 0.0,  1.0,  0.0],
        [ 0.0,  0.0,  1.0],
    ]
}
// Sin #[rustfmt::skip], rustfmt reorganizaria los espacios
// de alineacion y arruinaria la presentacion visual.

// Tambien funciona en impls, modulos y otros items:
#[rustfmt::skip]
mod generated_tables;
```

### Integracion con editores

```bash
# VS Code — en settings.json:
# {
#     "editor.formatOnSave": true,
#     "[rust]": {
#         "editor.defaultFormatter": "rust-lang.rust-analyzer"
#     }
# }

# Neovim — en init.lua:
# vim.api.nvim_create_autocmd("BufWritePre", {
#     pattern = "*.rs",
#     callback = function() vim.lsp.buf.format() end,
# })

# Resultado: cada vez que guardas un .rs, se formatea automaticamente.
```

## clippy — Linter oficial

clippy analiza el codigo y detecta patrones problematicos: errores
comunes, codigo no idiomatico, problemas de rendimiento y practicas
sospechosas. Tiene mas de 700 lints organizados en categorias.

```bash
# Ejecutar clippy en el proyecto:
cargo clippy

# Incluir tests, benchmarks, examples y todas las features:
cargo clippy --all-targets --all-features

# En CI — convertir warnings en errores:
cargo clippy --all-targets --all-features -- -D warnings
```

### Categorias de lints

```bash
# clippy::correctness — errores casi seguros. Deny por defecto.
# clippy::suspicious  — codigo probablemente erroneo. Warn por defecto.
# clippy::style       — no sigue convenciones idiomaticas. Warn por defecto.
# clippy::complexity  — codigo que se puede simplificar. Warn por defecto.
# clippy::perf        — problemas de rendimiento. Warn por defecto.
# clippy::pedantic    — reglas estrictas y opinadas. NO activado por defecto.
# clippy::nursery     — lints experimentales. NO activado por defecto.
```

```bash
# Activar categorias adicionales con -W (warn):
cargo clippy -- -W clippy::pedantic

# Combinar: activar pedantic pero silenciar lints ruidosos:
cargo clippy -- -W clippy::pedantic -A clippy::must_use_candidate

# Convertir una categoria en error con -D (deny):
cargo clippy -- -D clippy::perf
```

### Controlar lints en el codigo

```rust
// Suprimir un lint en un item:
#[allow(clippy::needless_return)]
fn compute(x: i32) -> i32 {
    return x * 2;
}

// Convertir un lint en error:
#[deny(clippy::unwrap_used)]
fn safe_parse(s: &str) -> Option<i32> {
    s.parse().ok()
    // .unwrap() aqui seria error de compilacion.
}

// Uso comun: activar pedantic a nivel de crate y suprimir
// los lints demasiado ruidosos:
#![warn(clippy::pedantic)]
#![allow(clippy::must_use_candidate)]
#![allow(clippy::missing_errors_doc)]
```

### Configuracion de clippy

```toml
# clippy.toml — configuracion del linter a nivel de proyecto

too-many-lines-threshold = 100
too-many-arguments-threshold = 7
cognitive-complexity-threshold = 25

# Prohibir tipos o metodos especificos:
# disallowed-methods = ["std::option::Option::unwrap"]
# disallowed-types = ["std::collections::HashMap"]
```

### Auto-correccion

```bash
# clippy puede corregir muchos lints automaticamente:
cargo clippy --fix
cargo clippy --fix --allow-dirty --allow-staged

# Revisar siempre los cambios despues:
git diff
```

### Lints utiles frecuentes

```rust
// needless_return — return explicito innecesario:
// MAL:  fn double(x: i32) -> i32 { return x * 2; }
// BIEN: fn double(x: i32) -> i32 { x * 2 }

// redundant_clone — clonar cuando el valor ya no se usa:
// MAL:  let copy = data.clone();  // data no se usa despues
// BIEN: let copy = data;

// manual_map — match reemplazable por .map():
// MAL:
fn maybe_double(x: Option<i32>) -> Option<i32> {
    match x {
        Some(v) => Some(v * 2),
        None => None,
    }
}
// BIEN:
fn maybe_double(x: Option<i32>) -> Option<i32> {
    x.map(|v| v * 2)
}

// len_zero — .len() == 0 en vez de .is_empty():
// MAL:  if v.len() == 0 { ... }
// BIEN: if v.is_empty() { ... }

// map_unwrap_or — .map().unwrap_or() reemplazable por .map_or():
// MAL:  opt.map(|x| x + 1).unwrap_or(0)
// BIEN: opt.map_or(0, |x| x + 1)
```

## cargo-expand — Expansion de macros

cargo-expand muestra el codigo generado por macros despues de la
expansion. Esencial para depurar derive macros, macros declarativas
(`macro_rules!`) y macros procedurales.

```bash
# Requiere el compilador nightly:
rustup toolchain install nightly

# Expandir todo el crate:
cargo +nightly expand

# Expandir solo la libreria o un modulo:
cargo +nightly expand --lib
cargo +nightly expand module_name
cargo +nightly expand module_name::submodule
```

### Ejemplo: expandir derive(Debug)

```rust
// Codigo original:
#[derive(Debug)]
struct Point {
    x: f64,
    y: f64,
}

fn main() {
    let p = Point { x: 1.0, y: 2.0 };
    println!("{:?}", p);
}
```

```bash
# cargo +nightly expand muestra (simplificado):
#
# impl ::core::fmt::Debug for Point {
#     fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
#         ::core::fmt::Formatter::debug_struct(f, "Point")
#             .field("x", &self.x)
#             .field("y", &self.y)
#             .finish()
#     }
# }
#
# println!("{:?}", p) se expande a ::std::io::_print(...)
# con Arguments::new_v1. Todas las rutas son absolutas
# (::core::fmt::Debug) para evitar conflictos con nombres locales.
```

### Ejemplo: expandir vec![]

```bash
# vec![1, 2, 3, 4, 5] se expande a:
#
# <[_]>::into_vec(
#     #[rustc_box]
#     ::alloc::boxed::Box::new([1, 2, 3, 4, 5])
# )
#
# Crea un array en el heap (Box::new) y lo convierte a Vec
# via into_vec. Asigna toda la memoria de una vez, sin push()
# repetidos — por eso vec![] es eficiente.
```

```bash
# Casos de uso de cargo-expand:
# 1. Depurar derive macros de crates externos (serde, etc.)
# 2. Entender macros del lenguaje (println!, vec!, todo!)
# 3. Verificar que una macro procedural genera el codigo esperado
# 4. Diagnosticar conflictos entre derives
```

## rust-analyzer — Servidor de lenguaje

rust-analyzer es el servidor LSP oficial de Rust. Reemplazo a RLS
(deprecado). Proporciona analisis de codigo en tiempo real al editor.

```bash
# Instalacion:
rustup component add rust-analyzer
rust-analyzer --version

# rust-analyzer se ejecuta como proceso en segundo plano.
# El editor lo inicia automaticamente al abrir un proyecto Rust.
```

### Funcionalidades principales

```
Funcionalidad         Descripcion
--------------------  --------------------------------------------------
Code completion       Autocompletado con tipos, metodos, imports.
Go to definition      Ir a la definicion de cualquier simbolo.
Find references       Todos los usos de un simbolo en el proyecto.
Inline type hints     Tipos inferidos como anotaciones fantasma.
Rename symbol         Renombrar en todo el proyecto de forma segura.
Code actions          Refactoring: extraer funcion, inline, auto-import.
Diagnostics           Errores y warnings en tiempo real sin compilar.
Hover information     Documentacion y firma al pasar el cursor.
```

### Integracion con editores

```bash
# VS Code:
# Instalar extension "rust-analyzer" (rust-lang.rust-analyzer).
# Desinstalar "Rust" (rls) si existe — entran en conflicto.

# Neovim (nvim-lspconfig):
# require('lspconfig').rust_analyzer.setup {
#     settings = {
#         ['rust-analyzer'] = {
#             check = { command = "clippy" },
#             cargo = { allFeatures = true },
#         },
#     },
# }

# Emacs (eglot, built-in desde Emacs 29):
# (add-hook 'rust-mode-hook 'eglot-ensure)

# Helix: soporte integrado, detecta rust-analyzer automaticamente.
```

### Configuracion

```toml
# rust-analyzer.toml — en la raiz del proyecto (junto a Cargo.toml).
# Funciona con cualquier editor.

[check]
command = "clippy"     # usar clippy en vez de cargo check

[cargo]
allFeatures = true     # activar todas las features
```

```json5
// .vscode/settings.json — configuracion especifica de VS Code:
{
    "rust-analyzer.check.command": "clippy",
    "rust-analyzer.cargo.allFeatures": true,
    "rust-analyzer.inlayHints.typeHints.enable": true,
    "rust-analyzer.inlayHints.parameterHints.enable": true
}
```

### Problemas comunes

```bash
# 1. Lentitud al abrir un proyecto por primera vez.
#    rust-analyzer compila el proyecto para indexarlo. Normal.
#    Las aperturas posteriores son rapidas (cache persistente).

# 2. Conflicto con IntelliJ Rust.
#    IntelliJ tiene su propio plugin que NO usa rust-analyzer.
#    No usar ambos simultaneamente. Elegir uno.

# 3. "proc-macro not expanded" en derive macros.
#    rust-analyzer necesita compilar proc-macros. Si falla,
#    ejecutar cargo build para verificar que el proyecto compila.

# 4. rust-analyzer no encuentra el proyecto.
#    Necesita Cargo.toml en el directorio raiz abierto en el editor.

# 5. Alto consumo de memoria en proyectos grandes.
#    Puede consumir varios GB. Desactivar inlay hints si es problema.
```

## Otras herramientas utiles

Cualquier binario llamado `cargo-<nombre>` se puede invocar
como `cargo <nombre>`. Estas son las herramientas mas relevantes:

```bash
# cargo-watch — rebuild automatico al guardar archivos:
cargo install cargo-watch
cargo watch -x check                    # check en cada cambio
cargo watch -x check -x test -x run    # encadenar: check, test, run
cargo watch -c -x check                # limpiar pantalla entre ejecuciones
```

```bash
# cargo add — gestionar dependencias (integrado desde Rust 1.62):
cargo add serde --features derive      # agregar dependencia
cargo add --dev pretty_assertions      # dependencia de desarrollo
cargo remove serde                     # eliminar dependencia
```

```bash
# cargo-audit — auditoria de seguridad de dependencias:
cargo install cargo-audit
cargo audit                            # busca vulnerabilidades conocidas
```

```bash
# cargo-deny — lint de dependencias (licencias, duplicados, etc.):
cargo install cargo-deny
cargo deny init                        # crear deny.toml
cargo deny check                       # ejecutar todos los checks
cargo deny check licenses             # solo verificar licencias
```

```bash
# cargo-bloat — analisis de tamano del binario:
cargo install cargo-bloat
cargo bloat --release                  # funciones mas pesadas
cargo bloat --release --crates         # crates mas pesados
```

```bash
# cargo tree — arbol de dependencias (integrado en Cargo):
cargo tree                             # arbol completo
cargo tree -i rand                     # por que esta rand incluido
cargo tree --duplicates                # dependencias con multiples versiones
```

```bash
# cargo-outdated — dependencias desactualizadas:
cargo install cargo-outdated
cargo outdated                         # ver todas las desactualizadas
cargo outdated --root-deps-only        # solo dependencias directas
```

---

## Ejercicios

### Ejercicio 1 — rustfmt basico

```bash
# Crear un proyecto:
#   cargo new fmt_practice && cd fmt_practice
#
# En src/main.rs, escribir codigo mal formateado intencionalmente:
#   fn main(){let x=1+2;let y=x*3;println!("{}",y);}
#   fn add(a:i32,b:i32)->i32{a+b}
#   Imports desordenados:
#     use std::io::Write;
#     use std::collections::HashMap;
#     use std::fs;
#
# 1. Ejecutar cargo fmt -- --check. Debe reportar diferencias
#    y salir con exit code 1 (verificar con echo $?).
#
# 2. Ejecutar cargo fmt. Abrir src/main.rs y comparar.
#
# 3. Crear rustfmt.toml con:
#      edition = "2021"
#      imports_granularity = "Crate"
#      group_imports = "StdExternalCrate"
#    Agregar use std::io::Read junto al de Write.
#    Ejecutar cargo fmt y observar el agrupamiento.
#
# 4. Agregar una funcion con alineacion manual decorada con
#    #[rustfmt::skip]. Verificar que cargo fmt no la toca.
```

### Ejercicio 2 — clippy en profundidad

```bash
# En src/main.rs, escribir codigo con lints conocidos:
#
#   fn double(x: i32) -> i32 { return x * 2; }
#
#   fn maybe_name(opt: Option<String>) -> String {
#       match opt {
#           Some(s) => s,
#           None => String::from("anonymous"),
#       }
#   }
#
#   fn main() {
#       let v: Vec<i32> = Vec::new();
#       if v.len() == 0 { println!("empty"); }
#       println!("{}", double(5));
#       println!("{}", maybe_name(Some(String::from("Rust"))));
#   }
#
# 1. Ejecutar cargo clippy. Anotar cada warning e identificar
#    su categoria (style, complexity, etc.).
#
# 2. Corregir cada warning manualmente. Ejecutar cargo clippy
#    de nuevo: debe terminar sin warnings.
#
# 3. Ejecutar cargo clippy -- -W clippy::pedantic. Suprimir
#    los lints excesivos con #![allow(clippy::...)] al inicio.
#
# 4. Probar cargo clippy --fix y verificar con git diff.
```

### Ejercicio 3 — cargo-expand

```bash
# Crear un proyecto nuevo. Instalar nightly y cargo-expand.
#
# 1. Definir un struct con #[derive(Debug, Clone, PartialEq)]:
#      struct Color { r: u8, g: u8, b: u8 }
#    Ejecutar cargo +nightly expand.
#    Identificar las implementaciones generadas para Debug,
#    Clone y PartialEq.
#
# 2. Crear un vec![1, 2, 3] en main y expandir.
#    Observar como se convierte en Box::new + into_vec.
#
# 3. Crear un macro_rules! propio:
#      macro_rules! make_pair {
#          ($a:expr, $b:expr) => { ($a, $b) };
#      }
#    Usarlo en main y expandir para ver la resolucion.
```

### Ejercicio 4 — Flujo de trabajo integrado

```bash
# Combinar todas las herramientas en un flujo real:
#
# 1. cargo new workflow_demo && cd workflow_demo
# 2. cargo add serde --features derive
# 3. Escribir codigo funcional pero desordenado:
#      use serde::Serialize;
#      use std::collections::HashMap;
#      #[derive(Serialize, Debug)]
#      struct Config {
#          name: String,
#          values: HashMap<String, i32>,
#      }
#      impl Config {
#          fn new(name: String) -> Config {
#              return Config{name: name, values: HashMap::new()};
#          }
#          fn get(&self, key: &str) -> Option<i32> {
#              match self.values.get(key) {
#                  Some(v) => Some(*v),
#                  None => None,
#              }
#          }
#      }
#      fn main() {
#          let cfg = Config::new(String::from("demo"));
#          if cfg.values.len() == 0 { println!("empty"); }
#          println!("{:?}", cfg);
#      }
#
# 4. Ejecutar en orden:
#      a. cargo fmt -- --check    (ver que esta mal formateado)
#      b. cargo fmt               (corregir formato)
#      c. cargo clippy            (ver warnings: needless_return,
#                                  manual_map, len_zero, etc.)
#      d. Corregir cada warning
#      e. cargo clippy            (verificar cero warnings)
#      f. cargo +nightly expand   (ver expansiones de Serialize y Debug)
```
