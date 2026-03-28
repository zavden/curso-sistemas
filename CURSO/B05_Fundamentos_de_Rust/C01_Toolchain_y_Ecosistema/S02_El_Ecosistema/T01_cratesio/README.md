# T01 — crates.io

## Que es crates.io

crates.io es el registro oficial de paquetes de Rust. Es el lugar
donde la comunidad publica y comparte bibliotecas (llamadas "crates"
en Rust). Cuando ejecutas `cargo build` en un proyecto que tiene
dependencias, Cargo las descarga desde crates.io.

Es analogo a otros registros de paquetes:

```
# Comparacion con otros ecosistemas:
#
# Rust     → crates.io    (cargo)
# Node.js  → npm          (npm / yarn)
# Python   → PyPI         (pip)
# Java     → Maven Central (maven / gradle)
# Ruby     → RubyGems     (gem / bundler)
#
# La diferencia principal: en Rust, Cargo y crates.io estan
# integrados de fabrica. No necesitas instalar nada adicional.
```

```
# Caracteristicas de crates.io:
#
# - Gratuito y de codigo abierto
# - Mantenido por la comunidad de Rust y el Rust Foundation
# - Inmutable: una vez publicada una version, no se puede modificar
# - No se pueden eliminar versiones (solo "yank", explicado mas adelante)
# - A marzo 2026: mas de 160,000 crates publicados
```

## Buscar crates

Antes de escribir algo desde cero, conviene buscar si ya existe
un crate que lo resuelva. Hay varias formas de buscar.

### Interfaz web: crates.io

```bash
# La interfaz web esta en:
# https://crates.io

# Funcionalidades:
# - Barra de busqueda por nombre o descripcion
# - Categorias (networking, parsing, cryptography, etc.)
# - Ordenar por: descargas totales, descargas recientes, relevancia
# - Cada crate muestra: descripcion, version actual, descargas,
#   fecha de ultima actualizacion, enlace al repositorio
```

### Busqueda desde la terminal

```bash
# cargo search busca en crates.io desde la linea de comandos:
cargo search serde
# serde = "1.0.215"    # A serialization framework for Rust
# serde_json = "1.0.133" # A JSON serialization file format
# ...

# Muestra las primeras 10 coincidencias por defecto.

# Limitar o ampliar resultados:
cargo search serde --limit 5
cargo search serde --limit 20

# Buscar por terminos mas amplios:
cargo search "http client"
cargo search async
```

### lib.rs — herramienta alternativa de descubrimiento

```
# https://lib.rs
#
# lib.rs es un indice NO oficial que presenta los crates
# con mejor curaduria:
# - Mejor interfaz para navegar categorias
# - Puntuaciones de calidad basadas en multiples factores
# - Clasificacion por popularidad y actividad
#
# lib.rs NO es un registro alternativo. Los crates se descargan
# de crates.io igualmente. lib.rs es solo una interfaz de busqueda.
```

### docs.rs — documentacion automatica

```
# https://docs.rs
#
# Cada crate publicado en crates.io se documenta automaticamente
# en docs.rs. No necesitas hacer nada — la documentacion se genera
# a partir de los comentarios /// y //! del codigo fuente.
#
# URL de documentacion de un crate:
# https://docs.rs/serde/latest
# https://docs.rs/tokio/1.41.1
#
# docs.rs compila la documentacion para multiples plataformas
# y feature flags, asi que puedes ver la API completa.
```

### Evaluar la calidad de un crate

```
# Antes de agregar una dependencia, evalua su calidad:
#
# 1. Descargas totales y recientes
#    - Muchas descargas recientes → crate activo y confiable
#    - Muchas totales pero pocas recientes → posiblemente abandonado
#
# 2. Fecha de ultima actualizacion
#    - Ultimo release hace 6+ meses sin actividad → precaucion
#    - Excepcion: crates estables y maduros no necesitan actualizaciones
#      frecuentes (ejemplo: libc se actualiza poco porque ya esta completo)
#
# 3. Documentacion
#    - Buena documentacion en docs.rs → crate bien mantenido
#    - Sin documentacion → riesgo de API confusa
#
# 4. Numero de dependientes (reverse dependencies)
#    - Cuantos otros crates dependen de este
#    - Muchos dependientes → la comunidad confia en el
#    - crates.io muestra esto en la pagina de cada crate
#
# 5. Repositorio y issues
#    - Issues abiertos respondidos → mantenedor activo
#    - PRs ignorados por meses → posible abandono
#
# 6. Licencia
#    - MIT y Apache-2.0 son las mas comunes en el ecosistema Rust
#    - Verificar que la licencia es compatible con tu proyecto
```

## Usar crates

### Agregar dependencias

```bash
# Forma moderna: cargo add (disponible desde Rust 1.62)
cargo add serde
# Agrega serde a [dependencies] en Cargo.toml con la version
# mas reciente compatible.

# El resultado en Cargo.toml:
# [dependencies]
# serde = "1.0.215"
```

```bash
# Agregar con features especificas:
cargo add serde --features derive
# Resultado en Cargo.toml:
# [dependencies]
# serde = { version = "1.0.215", features = ["derive"] }

# Multiples features:
cargo add tokio --features full
cargo add tokio --features "rt,net,io-util"
```

```bash
# Agregar una version especifica:
cargo add tokio@1.0
# Resultado: tokio = "1.0"

# Version exacta:
cargo add tokio@=1.41.1
# Resultado: tokio = "=1.41.1"
```

```bash
# Agregar como dependencia de desarrollo (solo para tests):
cargo add --dev mockall

# Agregar como dependencia de build (para build.rs):
cargo add --build cc
```

```toml
# Forma manual: editar Cargo.toml directamente.
# Esto siempre funciona, incluso en versiones antiguas de Rust.

[dependencies]
serde = "1.0"
serde_json = "1.0"
tokio = { version = "1", features = ["full"] }

[dev-dependencies]
mockall = "0.13"

[build-dependencies]
cc = "1.0"
```

```bash
# Eliminar una dependencia:
cargo remove serde
# Elimina serde de Cargo.toml.
```

### Como Cargo resuelve dependencias

```
# Cuando ejecutas cargo build, Cargo:
#
# 1. Lee Cargo.toml para ver las dependencias directas
# 2. Descarga el indice de crates.io (un repositorio git con metadatos)
# 3. Resuelve el arbol completo de dependencias:
#    - Tu crate depende de A y B
#    - A depende de C v1.0
#    - B depende de C v1.2
#    - Cargo unifica: usa C v1.2 (compatible con ambos via SemVer)
# 4. Descarga los crates necesarios a ~/.cargo/registry/
# 5. Compila todo en el orden correcto
# 6. Escribe el resultado en Cargo.lock
```

```
# Cargo.lock — el archivo de versiones exactas:
#
# Cargo.lock registra las versiones EXACTAS de cada dependencia
# que se uso en la compilacion. Esto garantiza reproducibilidad.
#
# - Para binarios (aplicaciones): Cargo.lock va en el repositorio
#   (git add Cargo.lock). Asi todos compilan con las mismas versiones.
#
# - Para bibliotecas (crates que otros usan como dependencia):
#   Cargo.lock NO va en el repositorio (.gitignore).
#   Los consumidores del crate tienen su propio Cargo.lock.
```

```
# Resolucion de conflictos:
#
# Cargo puede tener DOS versiones del mismo crate en el arbol:
#
# Tu crate
# ├── serde 1.0.200 (tu dependencia directa)
# ├── crate_A
# │   └── serde 1.0.180 → Cargo unifica a 1.0.200 (compatible)
# └── crate_B
#     └── rand 0.7.3    → incompatible con rand 0.8.x
#         (si tu tambien usas rand 0.8, ambas versiones coexisten)
#
# Esto funciona porque Rust permite multiples versiones del mismo
# crate siempre que sus MAJOR versions sean diferentes.
# (0.7 y 0.8 son "major" diferentes en pre-1.0, ver seccion SemVer.)
```

### Visualizar y actualizar dependencias

```bash
# cargo tree muestra el arbol completo de dependencias:
cargo tree
# my_project v0.1.0
# ├── serde v1.0.215
# │   └── serde_derive v1.0.215
# │       ├── proc-macro2 v1.0.92
# │       │   └── unicode-ident v1.0.14
# │       ├── quote v1.0.37
# │       │   └── proc-macro2 v1.0.92 (*)
# │       └── syn v2.0.90
# │           ├── proc-macro2 v1.0.92 (*)
# │           ├── quote v1.0.37 (*)
# │           └── unicode-ident v1.0.14 (*)
# └── tokio v1.41.1
#     └── ...
#
# (*) indica que la dependencia ya se mostro antes (evita repeticion).
```

```bash
# Buscar por que un crate especifico esta en el arbol:
cargo tree -i syn
# syn v2.0.90
# ├── serde_derive v1.0.215
# │   └── serde v1.0.215
# │       └── my_project v0.1.0
# ...
# Esto muestra la cadena inversa: quien depende de syn.
```

```bash
# Mostrar solo las dependencias duplicadas:
cargo tree --duplicates
# Util para detectar cuando el mismo crate aparece en
# multiples versiones incompatibles.
```

```bash
# cargo update actualiza las dependencias DENTRO de los
# limites definidos en Cargo.toml:
cargo update
# Si Cargo.toml dice serde = "1.0", actualiza de 1.0.200 a 1.0.215
# pero NUNCA a 2.0.0.

# Actualizar solo un crate especifico:
cargo update serde

# Ver que se actualizaria sin aplicar cambios:
cargo update --dry-run
```

## Versionado semantico (SemVer)

El versionado semantico es un contrato entre el autor de un
crate y sus usuarios. Define que tipo de cambios se permiten
en cada componente de la version.

### MAJOR.MINOR.PATCH

```
# Formato: MAJOR.MINOR.PATCH
# Ejemplo: 1.4.2
#
# MAJOR (1.x.x → 2.0.0):
#   Cambios que ROMPEN compatibilidad con versiones anteriores.
#   Ejemplos:
#   - Eliminar una funcion publica
#   - Cambiar la firma de una funcion (parametros, tipo de retorno)
#   - Cambiar el comportamiento de forma incompatible
#   - Renombrar modulos o tipos publicos
#
# MINOR (1.4.x → 1.5.0):
#   Nuevas funcionalidades, COMPATIBLE con versiones anteriores.
#   Ejemplos:
#   - Agregar una funcion publica nueva
#   - Agregar un nuevo modulo
#   - Agregar nuevos campos opcionales a un struct (con default)
#   - Deprecar algo (pero no eliminarlo)
#
# PATCH (1.4.2 → 1.4.3):
#   Correcciones de bugs, COMPATIBLE con versiones anteriores.
#   Ejemplos:
#   - Corregir un calculo incorrecto
#   - Corregir un panic inesperado
#   - Mejorar rendimiento sin cambiar la API
#   - Corregir documentacion
```

### Versiones pre-1.0 (0.y.z)

```
# Las versiones 0.y.z tienen reglas especiales:
#
# En pre-1.0, la API se considera INESTABLE.
# El autor puede cambiar cualquier cosa en cualquier momento.
#
# Convencion en el ecosistema Rust:
# - 0.1.x → 0.2.0 se trata como cambio MAJOR (puede romper)
# - 0.1.0 → 0.1.1 se trata como cambio PATCH (compatible)
#
# Es decir, el MINOR de 0.y.z funciona como el MAJOR:
#
# Version    | Siguiente compatible | Siguiente incompatible
# 0.1.0      | 0.1.1, 0.1.2        | 0.2.0
# 0.2.3      | 0.2.4, 0.2.5        | 0.3.0
# 1.0.0      | 1.0.1, 1.1.0        | 2.0.0
#
# Esto es porque SemVer dice que 0.y.z es para "desarrollo inicial"
# donde "cualquier cosa puede cambiar en cualquier momento".
# Rust formaliza esto: ^ en 0.x respeta el segundo digito.
```

```
# Por que tantos crates estan en 0.x:
#
# Muchos crates populares permanecen en 0.x durante anios:
# - rand estuvo en 0.x durante mucho tiempo
# - actix-web fue 0.x en sus primeras versiones
#
# Razones:
# 1. Compromiso con 1.0: una vez en 1.0, romper la API requiere 2.0
# 2. Flexibilidad: en 0.x puedes iterar rapidamente
# 3. Cultura: la comunidad Rust valora la estabilidad post-1.0
#
# Regla general:
# - 0.x es perfectamente usable en produccion
# - Pero espera cambios de API entre versiones minor
# - Lee el CHANGELOG antes de actualizar
```

### Requisitos de version en Cargo.toml

```toml
[dependencies]

# ^ (caret) — es el DEFECTO cuando escribes solo "1.2.3"
# Permite actualizaciones que NO cambien el digito mas significativo
# distinto de cero.
serde = "1.2.3"      # equivale a ^1.2.3 → >=1.2.3, <2.0.0
serde = "0.2.3"      # equivale a ^0.2.3 → >=0.2.3, <0.3.0
serde = "0.0.3"      # equivale a ^0.0.3 → >=0.0.3, <0.0.4

# ^ protege contra cambios incompatibles:
# - En 1.x.x: el MAJOR es significativo → no sube de 1 a 2
# - En 0.x.x: el MINOR es significativo → no sube de 0.2 a 0.3
# - En 0.0.x: el PATCH es significativo → no sube de 0.0.3 a 0.0.4
```

```toml
[dependencies]

# ~ (tilde) — permite solo cambios PATCH
tokio = "~1.2.3"     # >=1.2.3, <1.3.0
tokio = "~1.2"       # >=1.2.0, <1.3.0
tokio = "~1"         # >=1.0.0, <2.0.0

# La diferencia con ^:
# ^1.2.3 permite 1.3.0, 1.4.0, etc. (hasta <2.0.0)
# ~1.2.3 solo permite 1.2.4, 1.2.5, etc. (hasta <1.3.0)
# ~ es mas conservador.
```

```toml
[dependencies]

# = (exacta) — solo permite ESA version exacta
uuid = "=1.11.0"     # solo 1.11.0, nada mas

# Usar con precaucion: impide que Cargo unifique versiones.
# Si tu crate pide =1.11.0 y otro pide =1.11.1, es un conflicto
# irresoluble.
```

```toml
[dependencies]

# Rangos:
rand = ">=1.0.0, <1.5.0"

# Wildcard:
regex = "1.*"         # >=1.0.0, <2.0.0 (similar a ^1)
regex = "1.2.*"       # >=1.2.0, <1.3.0 (similar a ~1.2)
```

```
# Resumen de operadores de version:
#
# Requisito      | Ejemplo   | Rango permitido
# "1.2.3" (^)    | ^1.2.3    | >=1.2.3, <2.0.0
# "0.2.3" (^)    | ^0.2.3    | >=0.2.3, <0.3.0
# "0.0.3" (^)    | ^0.0.3    | >=0.0.3, <0.0.4
# ~1.2.3         | ~1.2.3    | >=1.2.3, <1.3.0
# =1.2.3         | =1.2.3    | solo 1.2.3
# >=1.0, <2.0    | rango     | >=1.0.0, <2.0.0
# 1.*            | wildcard  | >=1.0.0, <2.0.0
```

### Violaciones de SemVer en la practica

```
# En el mundo real, los autores a veces cometen errores:
#
# 1. Romper la API en un PATCH:
#    - El crate foo 1.2.3 cambia la firma de una funcion publica
#    - Tu codigo que dependia de foo ^1.2.0 deja de compilar
#    - Solucion inmediata: fijar la version en Cargo.toml
#      foo = "=1.2.2"  (la ultima version que funcionaba)
#    - Solucion correcta: reportar un issue al autor del crate
#
# 2. Agregar un breaking change en MINOR:
#    - foo 1.3.0 elimina una funcion que existia en 1.2.x
#    - Solucion: foo = "~1.2" (restringir a patch updates)
#
# 3. Que hacer si descubres una violacion de SemVer:
#    a. Fijar la version que funciona en Cargo.toml
#    b. Reportar un issue en el repositorio del crate
#    c. El autor puede "yank" la version problematica
#    d. Considerar buscar un crate alternativo si es recurrente
```

## Publicar crates

### Requisitos previos

```bash
# 1. Crear una cuenta en crates.io
#    Ve a https://crates.io y haz login con tu cuenta de GitHub.
#    No hay otro metodo de registro — se requiere GitHub.

# 2. Obtener un token API
#    En crates.io → Account Settings → API Tokens → New Token
#    El token se muestra UNA sola vez. Guardalo de forma segura.

# 3. Iniciar sesion con Cargo
cargo login <tu-token-api>
# El token se guarda en ~/.cargo/credentials.toml
# Este archivo NO debe ir en control de versiones.
```

```bash
# Verificar que estas autenticado:
cat ~/.cargo/credentials.toml
# [registry]
# token = "cio..."
#
# Nunca compartas este archivo ni lo subas a git.
# Agregalo a .gitignore si es necesario.
```

### Campos requeridos en Cargo.toml

```toml
# Cargo.toml MINIMO para publicar:

[package]
name = "my-crate-name"     # unico en todo crates.io
version = "0.1.0"          # SemVer
edition = "2021"           # edicion de Rust
description = "A short description of what this crate does"  # obligatorio
license = "MIT"            # obligatorio (identificador SPDX)

# Si no tienes description o license, cargo publish falla:
# error: 3 mandatory fields are missing: description, license
```

```toml
# Campos recomendados (no obligatorios pero importantes):

[package]
name = "my-crate-name"
version = "0.1.0"
edition = "2021"
description = "A short description of what this crate does"
license = "MIT OR Apache-2.0"     # licencia dual (comun en Rust)
# license-file = "LICENSE"        # alternativa si no es una licencia SPDX
authors = ["Your Name <you@example.com>"]
repository = "https://github.com/user/my-crate"
homepage = "https://my-crate.example.com"
documentation = "https://docs.rs/my-crate"
readme = "README.md"
keywords = ["keyword1", "keyword2"]      # maximo 5
categories = ["development-tools"]       # de la lista de crates.io
rust-version = "1.75"                    # MSRV (Minimum Supported Rust Version)
```

```
# Licencias SPDX comunes en el ecosistema Rust:
#
# "MIT"                  — permisiva, la mas popular
# "Apache-2.0"           — permisiva, con proteccion de patentes
# "MIT OR Apache-2.0"    — licencia dual (la convencion de Rust)
# "BSD-2-Clause"         — permisiva
# "GPL-3.0-only"         — copyleft fuerte
# "LGPL-3.0-only"        — copyleft debil
# "MPL-2.0"              — copyleft a nivel de archivo
#
# La comunidad Rust prefiere "MIT OR Apache-2.0" porque:
# - MIT es simple y permisiva
# - Apache-2.0 agrega proteccion de patentes
# - El dual licensing da flexibilidad a los usuarios
```

### Preparar y verificar antes de publicar

```bash
# cargo package empaqueta tu crate como un archivo .crate
# y verifica que todo este correcto:
cargo package
# Packaging my-crate v0.1.0
# Verifying my-crate v0.1.0
# Compiling my-crate v0.1.0
# Finished dev profile [unoptimized + debuginfo]
# Packaged 5 files, 3.2 KiB

# El archivo .crate se crea en target/package/
ls target/package/
# my-crate-0.1.0.crate

# cargo package tambien compila el crate desde el archivo empaquetado
# para verificar que funciona correctamente.
```

```bash
# Listar exactamente que archivos se incluiran en el paquete:
cargo package --list
# Cargo.toml
# Cargo.toml.orig
# src/lib.rs
# README.md
# LICENSE

# Revisar esta lista es importante. Verifica que:
# - No se incluyan archivos sensibles (tokens, claves, .env)
# - Si se incluyan los archivos necesarios (README, LICENSE)
```

### Que se publica y que no

```
# Por defecto, cargo publish incluye:
#
# INCLUIDO:
# - src/           — todo el codigo fuente
# - Cargo.toml     — manifiesto del proyecto
# - Cargo.toml.orig — copia original (Cargo modifica algunos campos)
# - README.md      — si existe
# - LICENSE / LICENSE-MIT / LICENSE-APACHE — si existen
# - build.rs       — si existe
# - benches/       — benchmarks
# - examples/      — ejemplos
# - tests/         — tests de integracion
#
# EXCLUIDO automaticamente:
# - target/        — artefactos de compilacion
# - .git/          — historial de git
# - .gitignore     — archivo de git
# - Cargo.lock     — para bibliotecas (si es binario, se incluye)
# - Archivos ocultos (empiezan con .)
```

```toml
# Controlar que se incluye con include/exclude en Cargo.toml:

[package]
# ...

# OPCION 1: exclude — excluir archivos especificos
# (todo lo demas se incluye)
exclude = [
    "tests/fixtures/large_file.bin",
    "docs/internal/",
    "*.bak",
]

# OPCION 2: include — incluir SOLO estos archivos
# (todo lo demas se excluye)
include = [
    "src/**/*",
    "Cargo.toml",
    "README.md",
    "LICENSE",
]

# No se pueden usar ambos a la vez.
# include es mas seguro: garantiza que solo se publique
# lo que explicitamente quieres.
```

### Publicar

```bash
# Dry run — probar sin publicar:
cargo publish --dry-run
# Hace todo el proceso (empaquetar, verificar, compilar)
# pero NO sube nada a crates.io.
# Util para detectar problemas antes de publicar.

# Publicar:
cargo publish
# Uploading my-crate v0.1.0
# ...
# Publicado! El crate ya esta disponible en crates.io.
```

```
# Lo que sucede despues de publicar:
#
# 1. El crate aparece en crates.io en segundos
# 2. docs.rs genera la documentacion automaticamente (minutos)
# 3. El indice de crates.io se actualiza
# 4. Otros usuarios pueden hacer: cargo add my-crate
#
# IMPORTANTE: una vez publicada una version, NO se puede:
# - Modificar el contenido
# - Eliminar la version
# - Republicar la misma version con cambios
#
# Si cometiste un error, tu unica opcion es publicar una nueva
# version (0.1.1) o hacer yank de la version problematica.
```

```bash
# Publicar una nueva version:
# 1. Actualiza la version en Cargo.toml:
#    version = "0.1.1"
# 2. Publica:
cargo publish

# No puedes publicar la misma version dos veces:
# error: crate version `0.1.0` is already uploaded
```

### Registros alternativos

```toml
# Para empresas que necesitan crates privados:
# Configurar en .cargo/config.toml

[registries]
my-company = { index = "https://registry.company.com/git/index" }

# Publicar a un registro alternativo:
# cargo publish --registry my-company

# Depender de un crate en un registro alternativo:
# [dependencies]
# internal-lib = { version = "1.0", registry = "my-company" }
```

```
# Opciones de registros privados:
#
# - Cloudsmith: registro gestionado en la nube
# - Shipyard: registro privado como servicio
# - Kellnr: registro privado auto-hospedado (open source)
# - Artifactory: soporta registros de Cargo
# - Git index: puedes montar tu propio indice git
#
# Para proyectos personales o pequenos equipos, los path
# dependencies o git dependencies suelen ser suficientes:
#
# [dependencies]
# my-lib = { path = "../my-lib" }
# my-lib = { git = "https://github.com/user/my-lib" }
```

## Yanking

Yanking es el mecanismo de crates.io para marcar una version
como "no recomendada para nuevos proyectos". No es una
eliminacion — es una advertencia.

### Que significa yank

```
# "Yank" marca una version como no deseable.
#
# Efecto:
# - Proyectos NUEVOS no pueden depender de esa version
#   (cargo build no la seleccionara como dependencia nueva)
# - Proyectos EXISTENTES que ya tienen esa version en su
#   Cargo.lock siguen funcionando sin cambios
#
# Esto es critico:
# - No rompe builds existentes
# - Previene que nuevos proyectos usen una version problematica
# - Es reversible (se puede "un-yank")
```

### Comandos de yank

```bash
# Marcar una version como yanked:
cargo yank --vers 1.0.1
# Yanking my-crate@1.0.1
#
# --vers es la version exacta. No acepta rangos.

# Si el crate no es tuyo o no estas autenticado:
# error: failed to yank: must be logged in
```

```bash
# Deshacer un yank (un-yank):
cargo yank --vers 1.0.1 --undo
# Unyank my-crate@1.0.1

# Esto restaura la version a su estado normal.
# Util si hiciste yank por error.
```

```bash
# Yank de un crate que no es el del directorio actual:
cargo yank --vers 1.0.1 my-other-crate
```

### Cuando hacer yank

```
# Situaciones donde yank es apropiado:
#
# 1. Vulnerabilidad de seguridad
#    - Descubres que la version 1.2.0 tiene un bug de seguridad
#    - Publicas 1.2.1 con la correccion
#    - Haces yank de 1.2.0
#    - Los proyectos existentes siguen compilando con 1.2.0
#      pero cargo update los movera a 1.2.1
#
# 2. Bug grave
#    - La version 0.5.0 tiene un bug que corrompe datos
#    - Publicas 0.5.1 con la correccion
#    - Haces yank de 0.5.0
#
# 3. Publicacion accidental
#    - Publicaste una version con credenciales en el codigo
#    - IMPORTANTE: yank NO elimina el contenido de crates.io
#    - Si publicaste secretos, rotalos inmediatamente
#    - Haz yank para evitar nuevos usos, pero asume que
#      el secreto ya fue expuesto
#
# 4. Version rota
#    - La version no compila en ciertas plataformas
#    - Dependencia critica fue removida
```

### Lo que yank NO hace

```
# Yank NO es una eliminacion:
#
# - El codigo sigue disponible en crates.io
# - Cualquiera puede descargar la version yanked manualmente
# - Proyectos con esa version en Cargo.lock siguen funcionando
# - No se puede eliminar una version de crates.io (por diseno)
#
# Por que no se permite eliminar:
#
# 1. Reproducibilidad: si alguien tiene Cargo.lock con tu version,
#    eliminarla romperia su build
# 2. Seguridad: evita ataques de "squatting" donde alguien elimina
#    un crate popular y sube uno malicioso con el mismo nombre
# 3. Confianza: el ecosistema depende de que las dependencias
#    siempre esten disponibles
#
# Si necesitas eliminar contenido sensible:
# - Contacta al equipo de crates.io: help@crates.io
# - Proporcionan asistencia en casos excepcionales
# - Pero asume que el contenido ya fue visto/descargado
```

### Yank y Cargo.lock

```
# Como interactua yank con Cargo.lock:
#
# Escenario:
# - Tu Cargo.lock tiene foo = 1.2.0
# - El autor de foo hace yank de 1.2.0
#
# Caso 1: cargo build (sin cambiar nada)
# → Funciona perfectamente. Cargo usa lo que hay en Cargo.lock.
#   La version yanked se usa sin problemas.
#
# Caso 2: cargo update
# → Cargo actualizara foo a la siguiente version compatible
#   que NO este yanked (ej: 1.2.1).
#   Si no hay ninguna version compatible no-yanked, falla.
#
# Caso 3: Proyecto nuevo, cargo build por primera vez
# → Cargo NO seleccionara 1.2.0 porque esta yanked.
#   Seleccionara la version mas reciente compatible no-yanked.
#
# Caso 4: Borras Cargo.lock y ejecutas cargo build
# → Mismo que caso 3: no seleccionara versiones yanked.
```

---

## Ejercicios

### Ejercicio 1 — Buscar y evaluar crates

```bash
# Objetivo: practicar la busqueda y evaluacion de crates.
#
# 1. Usa cargo search para buscar crates de serializacion JSON.
#    Identifica cual es el mas descargado.
#
# 2. Visita crates.io y busca "clap".
#    Responde:
#    a. Cuantas descargas totales tiene?
#    b. Cual es la version actual?
#    c. Cuantos dependientes (reverse dependencies) tiene?
#    d. Tiene documentacion en docs.rs?
#
# 3. Busca en lib.rs un crate para generar numeros aleatorios.
#    Compara lo que encuentras con buscar "random" en crates.io.
#    Cual interfaz te resulta mas util para descubrir crates?
```

### Ejercicio 2 — Gestionar dependencias

```bash
# Objetivo: practicar cargo add, cargo tree y cargo update.
#
# 1. Crea un proyecto nuevo:
#    cargo new semver_practice && cd semver_practice
#
# 2. Agrega estas dependencias:
#    cargo add serde --features derive
#    cargo add serde_json
#    cargo add rand
#
# 3. Ejecuta cargo tree y responde:
#    a. Cuantas dependencias transitivas tiene tu proyecto en total?
#    b. Que crate aparece como dependencia compartida?
#
# 4. Ejecuta cargo update --dry-run y observa si hay
#    actualizaciones disponibles.
#
# 5. Elimina rand con cargo remove rand.
#    Ejecuta cargo tree de nuevo y verifica que desaparecio.
```

### Ejercicio 3 — Entender SemVer

```toml
# Objetivo: predecir el comportamiento de los requisitos de version.
#
# Para cada requisito, indica el rango de versiones permitidas
# y si la version indicada entre parentesis seria aceptada:
#
# 1. serde = "1.0.200"
#    Rango: ???
#    Acepta 1.1.0?  → ???
#    Acepta 2.0.0?  → ???
#
# 2. tokio = "~1.2.3"
#    Rango: ???
#    Acepta 1.2.5?  → ???
#    Acepta 1.3.0?  → ???
#
# 3. rand = "0.8.5"
#    Rango: ???
#    Acepta 0.8.6?  → ???
#    Acepta 0.9.0?  → ???
#
# 4. uuid = "=1.11.0"
#    Rango: ???
#    Acepta 1.11.1? → ???
#
# 5. my-lib = "0.0.3"
#    Rango: ???
#    Acepta 0.0.4?  → ???
#    Acepta 0.0.3?  → ???
```

### Ejercicio 4 — Simular publicacion

```bash
# Objetivo: practicar el flujo de publicacion sin publicar realmente.
#
# 1. Crea un proyecto de biblioteca:
#    cargo new --lib my_practice_crate
#    cd my_practice_crate
#
# 2. Agrega al Cargo.toml los campos obligatorios para publicar:
#    description, license.
#    Agrega tambien: repository, keywords, categories.
#
# 3. Escribe una funcion publica simple en src/lib.rs:
#    pub fn add(a: i32, b: i32) -> i32 { a + b }
#    Agrega documentacion con ///.
#
# 4. Ejecuta cargo package --list y verifica que archivos
#    se incluirian.
#
# 5. Ejecuta cargo publish --dry-run y observa la salida.
#    Si hay errores, corrigelos hasta que el dry-run pase.
#
# 6. NO ejecutes cargo publish (no queremos publicar basura
#    en crates.io). El dry-run es suficiente para practicar.
```

### Ejercicio 5 — Cargo.lock y reproducibilidad

```bash
# Objetivo: entender el rol de Cargo.lock en la reproducibilidad.
#
# 1. Crea un proyecto con una dependencia:
#    cargo new lock_test && cd lock_test
#    cargo add serde
#
# 2. Ejecuta cargo build. Observa que se crea Cargo.lock.
#
# 3. Abre Cargo.lock y busca la version exacta de serde.
#    Anota el numero de version.
#
# 4. Elimina Cargo.lock:
#    rm Cargo.lock
#
# 5. Ejecuta cargo build de nuevo.
#    Abre el nuevo Cargo.lock y compara la version de serde.
#    Es la misma? Podria ser diferente?
#
# 6. Reflexiona: por que los proyectos binarios deben incluir
#    Cargo.lock en git, pero las bibliotecas no?
```
