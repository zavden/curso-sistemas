# T04 — thiserror y anyhow

## El problema que resuelven

En el tema anterior vimos como crear tipos de error custom
manualmente. Funciona, pero requiere mucho boilerplate:

```rust
// Para UN error enum con 3 variantes necesitas:
// 1. La definicion del enum (~15 lineas)
// 2. impl Display (~12 lineas)
// 3. impl Error con source() (~10 lineas)
// 4. impl From para cada error externo (~5 lineas cada uno)
//
// Total: ~50 lineas de codigo repetitivo por tipo de error.
// Si tienes 5 modulos, son ~250 lineas de plomeria.
```

Dos crates del ecosistema eliminan este boilerplate:

```text
thiserror  →  genera Display, Error y From con macros derive
               para LIBRERIAS que definen tipos de error propios

anyhow     →  provee un tipo de error generico con contexto
               para BINARIOS que consumen errores de otros crates
```

```text
¿Cual usar?

Escribiendo una libreria (pub crate) → thiserror
Escribiendo un binario (main.rs)     → anyhow
Escribiendo ambos                    → thiserror en lib, anyhow en main

Nota: no son excluyentes ni obligatorios. Muchos proyectos
usan solo uno, y otros no usan ninguno (errores manuales).
```

## thiserror

`thiserror` es un crate de David Tolnay (el mismo autor de `serde`)
que genera las implementaciones de `Display`, `Error` y `From`
mediante `#[derive(thiserror::Error)]`:

```toml
# Cargo.toml
[dependencies]
thiserror = "2"
```

### Ejemplo basico — comparacion con manual

```rust
// ── SIN thiserror (manual) ──

use std::fmt;
use std::io;
use std::num::ParseIntError;

#[derive(Debug)]
enum DataError {
    Io(io::Error),
    Parse(ParseIntError),
    Empty,
}

impl fmt::Display for DataError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DataError::Io(e) => write!(f, "error de lectura: {e}"),
            DataError::Parse(e) => write!(f, "numero invalido: {e}"),
            DataError::Empty => write!(f, "archivo vacio"),
        }
    }
}

impl std::error::Error for DataError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            DataError::Io(e) => Some(e),
            DataError::Parse(e) => Some(e),
            _ => None,
        }
    }
}

impl From<io::Error> for DataError {
    fn from(e: io::Error) -> Self { DataError::Io(e) }
}

impl From<ParseIntError> for DataError {
    fn from(e: ParseIntError) -> Self { DataError::Parse(e) }
}
```

```rust
// ── CON thiserror ──

use thiserror::Error;

#[derive(Debug, Error)]
enum DataError {
    #[error("error de lectura: {0}")]
    Io(#[from] std::io::Error),

    #[error("numero invalido: {0}")]
    Parse(#[from] std::num::ParseIntError),

    #[error("archivo vacio")]
    Empty,
}

// Eso es todo. thiserror genera:
// - impl Display (usando los strings de #[error(...)])
// - impl Error con source() (para variantes con #[from] o #[source])
// - impl From<io::Error> for DataError (por #[from])
// - impl From<ParseIntError> for DataError (por #[from])
```

```text
~40 lineas de boilerplate → ~10 lineas con thiserror.
El comportamiento generado es identico al codigo manual.
```

### #[error("...")] — generar Display

El atributo `#[error("...")]` define el mensaje de Display
para cada variante. Soporta interpolacion de campos:

```rust
use thiserror::Error;

#[derive(Debug, Error)]
enum AppError {
    // Campo sin nombre (tuple variant) — acceso por posicion
    #[error("archivo no encontrado: {0}")]
    NotFound(String),

    // Campos nombrados — acceso por nombre
    #[error("linea {line}: {message}")]
    ParseError {
        line: usize,
        message: String,
    },

    // Sin campos — mensaje fijo
    #[error("operacion no permitida")]
    Forbidden,

    // Acceso a metodos del campo
    #[error("I/O error (kind: {0:?})")]
    Io(std::io::Error),
    // {0:?} usa Debug en vez de Display
}
```

```rust
// Formatos soportados en #[error("...")]:

#[derive(Debug, Error)]
enum Example {
    // {0} — Display del primer campo
    #[error("valor: {0}")]
    A(i32),

    // {name} — Display del campo "name"
    #[error("nombre: {name}")]
    B { name: String },

    // {0:?} — Debug del primer campo
    #[error("debug: {0:?}")]
    C(Vec<u8>),

    // {field:#?} — Debug pretty-print del campo
    #[error("detalle: {data:#?}")]
    D { data: Vec<String> },

    // Metodos del campo — .len(), .kind(), etc.
    #[error("buffer de {0} bytes")]
    E(Vec<u8>),
    // Nota: {0} llama Display, pero Vec<u8> no implementa Display
    // Esto NO compila. Usar {0:?} o acceder a .len()
}
```

```rust
// Acceder a metodos en el format string:
use std::io;

#[derive(Debug, Error)]
enum IoError {
    #[error("I/O ({kind}): {source}")]
    WithKind {
        kind: String,
        source: io::Error,
    },
}

// Tambien puedes usar una funcion custom para Display:
#[derive(Debug, Error)]
#[error("{format_error(self)}")]  // NO — esto no funciona directamente
enum Custom {}

// En su lugar, implementa Display manualmente si necesitas logica compleja
// y deja que thiserror solo genere Error y From.
```

### #[from] — generar From e identificar source

`#[from]` en un campo hace dos cosas:
1. Genera `impl From<TipoDelCampo> for TuError`
2. Marca ese campo como `source()` (causa del error)

```rust
use thiserror::Error;
use std::io;
use std::num::ParseIntError;

#[derive(Debug, Error)]
enum ConfigError {
    // #[from] genera:
    //   impl From<io::Error> for ConfigError
    //   source() retorna Some(&self.0)
    #[error("error de I/O")]
    Io(#[from] io::Error),

    // Otro #[from]:
    //   impl From<ParseIntError> for ConfigError
    #[error("numero invalido")]
    Parse(#[from] ParseIntError),

    // Sin #[from] — no genera From, no tiene source
    #[error("clave '{0}' no encontrada")]
    Missing(String),
}

// Ahora ? funciona automaticamente:
fn load_port(path: &str) -> Result<u16, ConfigError> {
    let content = std::fs::read_to_string(path)?;  // io::Error → ConfigError::Io
    let port = content.trim().parse::<u16>()?;      // ParseIntError → ConfigError::Parse
    Ok(port)
}
```

```text
Reglas de #[from]:
- Solo puede haber UN campo con #[from] por variante
- El campo debe ser el unico de la variante (tuple de 1 elemento)
  o el unico que no tiene valor default
- No puedes tener #[from] si la variante tiene campos extra
  (porque From no sabria como rellenarlos)
- Si necesitas campos extra, usa map_err manualmente
```

```rust
// #[from] NO funciona con campos extra:
#[derive(Debug, Error)]
enum BadExample {
    // ERROR de compilacion:
    // #[error("linea {line}: {source}")]
    // Parse {
    //     line: usize,
    //     #[from] source: ParseIntError,  // no puede generar From
    // },

    // SOLUCION: sin #[from], usar #[source] para marcar la causa
    #[error("linea {line}")]
    Parse {
        line: usize,
        #[source] source: std::num::ParseIntError,
        // #[source] marca source() pero NO genera From
    },
}

// Usar map_err para construir la variante:
fn parse_line(line_num: usize, text: &str) -> Result<i32, BadExample> {
    text.parse().map_err(|e| BadExample::Parse {
        line: line_num,
        source: e,
    })
}
```

### #[source] — marcar source() sin generar From

```rust
use thiserror::Error;

#[derive(Debug, Error)]
enum DbError {
    // #[source] solo marca source(), NO genera From
    #[error("query fallida: {query}")]
    QueryFailed {
        query: String,
        #[source] cause: std::io::Error,
        // source() retorna Some(&self.cause)
        // pero NO hay impl From<io::Error>
    },

    // Campo llamado "source" es detectado automaticamente
    #[error("conexion perdida")]
    ConnectionLost {
        source: std::io::Error,
        // thiserror detecta "source" por nombre → lo usa como source()
        // No necesitas #[source] si el campo se llama "source"
    },
}
```

```text
Resumen de atributos en campos:

Atributo     Genera From    Marca source()    Notas
──────────────────────────────────────────────────────────
#[from]      Si             Si                Solo en variantes de 1 campo
#[source]    No             Si                Para variantes con campos extra
(nada)       No             No                Campo normal
"source"     No             Si (automatico)   Deteccion por nombre
```

### thiserror con structs

```rust
use thiserror::Error;

// Funciona igual con structs:
#[derive(Debug, Error)]
#[error("error de validacion en campo '{field}': {message}")]
struct ValidationError {
    field: String,
    message: String,
}

// Struct con source:
#[derive(Debug, Error)]
#[error("error leyendo config '{path}'")]
struct ConfigLoadError {
    path: String,
    #[source] cause: std::io::Error,
}
```

### Ejemplo completo con thiserror

```rust
use std::collections::HashMap;
use std::path::PathBuf;
use thiserror::Error;

/// Errores del modulo de configuracion.
#[derive(Debug, Error)]
pub enum ConfigError {
    #[error("no se pudo leer '{path}'")]
    ReadFile {
        path: PathBuf,
        #[source] cause: std::io::Error,
    },

    #[error("formato invalido en linea {line}")]
    InvalidFormat { line: usize },

    #[error("clave requerida '{0}' no encontrada")]
    MissingKey(String),

    #[error("valor invalido para '{key}': {reason}")]
    InvalidValue { key: String, reason: String },
}

pub type Result<T> = std::result::Result<T, ConfigError>;

pub fn load_config(path: &str) -> Result<HashMap<String, String>> {
    let path_buf = PathBuf::from(path);

    let content = std::fs::read_to_string(path)
        .map_err(|cause| ConfigError::ReadFile {
            path: path_buf,
            cause,
        })?;

    let mut map = HashMap::new();

    for (i, line) in content.lines().enumerate() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        let (key, value) = line.split_once('=')
            .ok_or(ConfigError::InvalidFormat { line: i + 1 })?;

        map.insert(key.trim().to_string(), value.trim().to_string());
    }

    Ok(map)
}

pub fn get_required(
    config: &HashMap<String, String>,
    key: &str,
) -> Result<&str> {
    config.get(key)
        .map(|v| v.as_str())
        .ok_or_else(|| ConfigError::MissingKey(key.to_string()))
}
```

## anyhow

`anyhow` es otro crate de David Tolnay que provee un tipo de
error generico `anyhow::Error` que puede contener **cualquier**
error que implemente `std::error::Error`:

```toml
# Cargo.toml
[dependencies]
anyhow = "1"
```

### Uso basico

```rust
use anyhow::Result;  // alias para Result<T, anyhow::Error>

fn load_port(path: &str) -> Result<u16> {
    let content = std::fs::read_to_string(path)?;  // io::Error → anyhow::Error
    let port = content.trim().parse::<u16>()?;      // ParseIntError → anyhow::Error
    Ok(port)
}

fn main() -> Result<()> {
    let port = load_port("config.txt")?;
    println!("puerto: {port}");
    Ok(())
}
```

```text
¿Que paso?
- No definimos ningun tipo de error propio
- No implementamos Display, Error, ni From
- ? convierte cualquier error a anyhow::Error automaticamente
- anyhow::Result<T> es un alias para Result<T, anyhow::Error>
```

### anyhow::Error vs Box<dyn Error>

```text
Ambos son "error generico que wrappea cualquier Error".
Pero anyhow tiene ventajas:

                    Box<dyn Error>          anyhow::Error
──────────────────────────────────────────────────────────────
Tamaño              2 punteros (fat ptr)    1 puntero (thin ptr)
Send + Sync         No (a menos que lo      Si (siempre)
                    especifiques)
Backtrace           No                      Si (automatico en nightly,
                                            manual en stable)
Context/chain       Manual                  .context("msg")
Downcast            Limitado                .downcast_ref::<T>()
Clone               No                      No
```

### .context() — agregar contexto a errores

La caracteristica mas util de anyhow es `.context()`, que envuelve
un error con un mensaje descriptivo creando una cadena:

```rust
use anyhow::{Context, Result};
use std::fs;

fn load_config(path: &str) -> Result<String> {
    let content = fs::read_to_string(path)
        .context(format!("no se pudo leer el archivo de config '{path}'"))?;
    //  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //  si fs::read_to_string falla, el error sera:
    //    "no se pudo leer el archivo de config 'config.toml'"
    //    Caused by: No such file or directory (os error 2)

    Ok(content)
}

fn setup() -> Result<()> {
    let config = load_config("config.toml")
        .context("fallo la inicializacion del servidor")?;
    //  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //  cadena de 3 niveles:
    //    "fallo la inicializacion del servidor"
    //    Caused by:
    //      0: no se pudo leer el archivo de config 'config.toml'
    //      1: No such file or directory (os error 2)

    Ok(())
}
```

```rust
// .context() vs .with_context()
// context() evalua el mensaje siempre (eager)
// with_context() evalua solo si hay error (lazy)

fn process(path: &str) -> Result<Vec<u8>> {
    // Eager — el format! se ejecuta siempre, haya error o no:
    let data = fs::read(path)
        .context(format!("leyendo {path}"))?;

    // Lazy — el closure solo se ejecuta si hay error:
    let data = fs::read(path)
        .with_context(|| format!("leyendo {path}"))?;
    //  ^^^^^^^^^^^^^^ preferible si el mensaje es costoso de construir

    Ok(data)
}
```

```rust
// .context() tambien funciona con Option (convierte a Result):
use anyhow::{Context, Result};
use std::collections::HashMap;

fn get_port(config: &HashMap<String, String>) -> Result<u16> {
    let port_str = config.get("port")
        .context("clave 'port' no encontrada en config")?;
    //  Option<&String> → Result<&String, anyhow::Error>

    let port = port_str.parse::<u16>()
        .context("'port' no es un numero valido")?;

    Ok(port)
}
```

### anyhow! y bail! — crear errores ad-hoc

```rust
use anyhow::{anyhow, bail, ensure, Result};

fn validate_port(port: u16) -> Result<()> {
    // anyhow! — crea un anyhow::Error con un mensaje
    if port == 0 {
        return Err(anyhow!("puerto no puede ser 0"));
    }

    // bail! — atajo para return Err(anyhow!(...))
    if port < 1024 {
        bail!("puerto {port} es privilegiado (< 1024)");
    }

    // ensure! — atajo para if !cond { bail!(...) }
    ensure!(port < 65535, "puerto {port} fuera de rango");
    // equivalente a:
    // if !(port < 65535) {
    //     bail!("puerto {port} fuera de rango");
    // }

    Ok(())
}
```

```rust
// anyhow! soporta format strings:
let err = anyhow!("fallo al procesar archivo '{}'", path);

// bail! es el mas usado para retorno temprano:
fn process(data: &[u8]) -> Result<()> {
    if data.is_empty() {
        bail!("datos vacios");  // return Err(anyhow!("datos vacios"))
    }
    if data.len() > 1_000_000 {
        bail!("datos demasiado grandes: {} bytes", data.len());
    }
    Ok(())
}
```

### Downcast — recuperar el tipo original

Aunque `anyhow::Error` borra el tipo concreto, puedes recuperarlo
con `downcast_ref`:

```rust
use anyhow::Result;
use std::io;

fn do_something() -> Result<()> {
    std::fs::read_to_string("/no/existe")?;
    Ok(())
}

fn main() {
    if let Err(e) = do_something() {
        // Intentar recuperar el io::Error original:
        if let Some(io_err) = e.downcast_ref::<io::Error>() {
            match io_err.kind() {
                io::ErrorKind::NotFound => {
                    eprintln!("archivo no encontrado");
                }
                io::ErrorKind::PermissionDenied => {
                    eprintln!("sin permisos");
                }
                _ => eprintln!("error de I/O: {io_err}"),
            }
        } else {
            eprintln!("error: {e}");
        }
    }
}
```

```rust
// downcast_ref con errores custom (definidos con thiserror):
use thiserror::Error;

#[derive(Debug, Error)]
enum MyError {
    #[error("no encontrado: {0}")]
    NotFound(String),
    #[error("sin permisos")]
    Forbidden,
}

fn handle(err: &anyhow::Error) {
    if let Some(my_err) = err.downcast_ref::<MyError>() {
        match my_err {
            MyError::NotFound(name) => println!("buscando {name}..."),
            MyError::Forbidden => println!("pedir acceso..."),
        }
    }
}
```

```text
Nota: downcast_ref es un "escape hatch" — si te encuentras
haciendo downcast frecuentemente, probablemente necesitas
un tipo de error propio (thiserror) en vez de anyhow.
```

### Imprimir cadena de errores con anyhow

```rust
use anyhow::{Context, Result};

fn inner() -> Result<()> {
    std::fs::read_to_string("/no/existe")
        .context("leyendo archivo de datos")?;
    Ok(())
}

fn middle() -> Result<()> {
    inner().context("procesando datos")?;
    Ok(())
}

fn outer() -> Result<()> {
    middle().context("inicializando aplicacion")?;
    Ok(())
}

fn main() {
    if let Err(e) = outer() {
        // Display — solo el mensaje de nivel mas alto:
        eprintln!("error: {e}");
        // error: inicializando aplicacion

        // Alternate Display — toda la cadena:
        eprintln!("error: {e:#}");
        // error: inicializando aplicacion: procesando datos:
        //        leyendo archivo de datos: No such file or directory (os error 2)

        // Debug — cadena con formato enumerado:
        eprintln!("error: {e:?}");
        // error: inicializando aplicacion
        //
        // Caused by:
        //     0: procesando datos
        //     1: leyendo archivo de datos
        //     2: No such file or directory (os error 2)
    }
}
```

```text
Formatos de impresion de anyhow::Error:

{e}     → Display — solo el mensaje mas externo
{e:#}   → Alternate Display — toda la cadena en una linea
{e:?}   → Debug — cadena enumerada con "Caused by:"
{e:#?}  → Debug pretty — con backtrace (si esta disponible)
```

## thiserror + anyhow juntos

El patron mas comun en proyectos reales:

```text
mi_proyecto/
├── mi_libreria/          ← thiserror (tipos de error definidos)
│   ├── src/lib.rs
│   └── Cargo.toml        → thiserror = "2"
│
└── mi_binario/           ← anyhow (consume errores)
    ├── src/main.rs
    └── Cargo.toml        → anyhow = "1", mi_libreria = { path = ".." }
```

```rust
// ── mi_libreria/src/lib.rs ── (thiserror)
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DbError {
    #[error("conexion rechazada: {host}:{port}")]
    ConnectionRefused { host: String, port: u16 },

    #[error("query invalida")]
    InvalidQuery(#[from] SqlParseError),

    #[error("timeout despues de {0} segundos")]
    Timeout(u64),
}

#[derive(Debug, Error)]
#[error("error de SQL: {0}")]
pub struct SqlParseError(pub String);

pub fn connect(host: &str, port: u16) -> Result<(), DbError> {
    // simulacion
    Err(DbError::ConnectionRefused {
        host: host.to_string(),
        port,
    })
}
```

```rust
// ── mi_binario/src/main.rs ── (anyhow)
use anyhow::{Context, Result};
use mi_libreria::{self, DbError};

fn setup_database() -> Result<()> {
    mi_libreria::connect("localhost", 5432)
        .context("conectando a la base de datos")?;
    //  DbError → anyhow::Error con contexto adicional
    Ok(())
}

fn main() -> Result<()> {
    setup_database()
        .context("inicializacion fallida")?;
    // Cadena:
    //   inicializacion fallida
    //   Caused by:
    //     0: conectando a la base de datos
    //     1: conexion rechazada: localhost:5432

    println!("todo listo");
    Ok(())
}
```

```text
¿Por que esta separacion?

Libreria con thiserror:
- Los usuarios pueden hacer match sobre DbError::Timeout vs ConnectionRefused
- El contrato de error es parte de la API publica
- Tipos concretos, documentados, estables

Binario con anyhow:
- No necesita definir tipos de error propios
- Solo agrega contexto y reporta al usuario
- Rapido de escribir, facil de mantener
```

## Comparacion completa

```text
                    Manual           thiserror          anyhow
──────────────────────────────────────────────────────────────────
Boilerplate         Alto             Bajo               Minimo
Tipo concreto       Si               Si                 No (borrado)
Match por variante  Si               Si                 Solo con downcast
source()            Manual           Automatico         Automatico
From                Manual           #[from]            Automatico (todo)
Contexto            map_err          map_err            .context()
Uso tipico          Aprender/simple  Librerias          Binarios
Dependencias        Ninguna          thiserror (proc)   anyhow
```

```text
Arbol de decision:

¿Estoy escribiendo una libreria publica?
├── SI → thiserror
│   - Los usuarios necesitan match sobre variantes
│   - El tipo de error es parte de tu API
│
└── NO → ¿Necesito que el llamador distinga tipos de error?
         ├── SI → thiserror (o manual)
         │
         └── NO → anyhow
              - Solo necesitas reportar errores
              - .context() es suficiente
              - Prototipos, CLIs, scripts

¿Y si no quiero dependencias extra?
→ Errores manuales (tema T03)
→ O Box<dyn Error> como alternativa a anyhow sin dependencias
```

## Alternativas en el ecosistema

```text
Crate         Proposito                      Notas
──────────────────────────────────────────────────────────────
thiserror     Derive para error types         El estandar de facto
anyhow        Error generico + contexto       El estandar de facto
eyre          Fork de anyhow con hooks        Reportes custom (color-eyre)
color-eyre    eyre + reportes coloridos       Popular en CLIs
miette        Errores con spans/diagnosticos  Para compiladores/linters
snafu         Alternativa a thiserror         Mas verbose, mas control
displaydoc    Derive Display desde doc        Alternativa minima
```

```rust
// color-eyre — ejemplo de reporte bonito en terminal:
// use color_eyre::eyre::Result;
//
// fn main() -> Result<()> {
//     color_eyre::install()?;
//     // Los errores ahora se imprimen con colores y formato
//     // incluyendo backtrace y sugerencias
// }
//
// Salida:
//   Error:
//     × no se pudo leer el archivo de config
//     ├── Caused by:
//     │   No such file or directory (os error 2)
//     ╰── at src/main.rs:15:10
```

## Errores comunes

```rust
// ERROR 1: usar anyhow en una libreria publica
// Cargo.toml de un crate publicado en crates.io:
// [dependencies]
// anyhow = "1"
//
// pub fn parse(input: &str) -> anyhow::Result<Data> { ... }
//
// Problema: los usuarios no pueden hacer match sobre los errores.
// anyhow::Error borra el tipo — solo pueden imprimir o hacer downcast.
//
// Solucion: usar thiserror para librerias publicas.

// ERROR 2: #[from] en variante con campos extra
#[derive(Debug, thiserror::Error)]
enum Bad {
    // NO COMPILA:
    // #[error("parse en linea {line}")]
    // Parse {
    //     line: usize,
    //     #[from] source: std::num::ParseIntError,
    // },
}
// Solucion: usar #[source] (sin From) y construir manualmente con map_err

// ERROR 3: olvidar importar el trait Context
fn example() -> anyhow::Result<()> {
    std::fs::read_to_string("x")
        .context("msg")?;
    //  ^^^^^^^ method not found
    // Falta: use anyhow::Context;
    Ok(())
}

// ERROR 4: confundir thiserror::Error con std::error::Error
// use thiserror::Error;  ← macro derive
// use std::error::Error; ← trait
// Si importas ambos, hay conflicto.
// Solucion: usar path completo o rename
// use thiserror::Error as ThisError;

// ERROR 5: retornar anyhow::Error desde un trait que espera Box<dyn Error>
// anyhow::Error implementa Into<Box<dyn Error + Send + Sync>>
// pero no Into<Box<dyn Error>> (sin Send + Sync)
// Si necesitas esa conversion, usa .into() explicitamente.
```

## Cheatsheet

```text
──── thiserror ────

#[derive(Debug, thiserror::Error)]
enum MyError {
    #[error("msg con {0}")]           // Display del campo 0
    Variant(#[from] OtherError),      // From + source

    #[error("msg con {field}")]       // Display de campo nombrado
    Variant2 {
        field: String,
        #[source] cause: io::Error,   // source sin From
    },

    #[error("msg fijo")]
    Simple,                           // sin source, sin From
}

──── anyhow ────

use anyhow::{anyhow, bail, ensure, Context, Result};

fn f() -> Result<T> {                 // Result<T, anyhow::Error>
    op().context("que paso")?;        // agrega contexto
    op().with_context(|| msg)?;       // contexto lazy
    bail!("fallo: {x}");             // return Err(anyhow!(...))
    ensure!(cond, "msg");            // if !cond { bail!(...) }
    Err(anyhow!("error ad-hoc"))     // error sin tipo
}

// Imprimir:
eprintln!("{e}");                     // solo mensaje externo
eprintln!("{e:#}");                   // cadena en una linea
eprintln!("{e:?}");                   // cadena con "Caused by:"
```

---

## Ejercicios

### Ejercicio 1 — Migrar a thiserror

```rust
// Toma el tipo DataError del tema T03 (el ejemplo de CSV) y
// reescribelo usando thiserror.
//
// Compara ambas versiones:
// - ¿Cuantas lineas ahorraste?
// - ¿El comportamiento es identico?
// - ¿Que ganas y que pierdes?
//
// Pista: variantes con campos extra necesitan #[source], no #[from].
```

### Ejercicio 2 — Binario con anyhow

```rust
// Escribe un programa CLI que:
// 1. Lea un path de argv (primer argumento)
// 2. Lea el archivo
// 3. Cuente las lineas no vacias
// 4. Imprima el resultado
//
// Usa anyhow::Result para main.
// Agrega .context() en cada operacion que pueda fallar.
//
// Prueba con:
//   cargo run -- archivo_real.txt      → debe funcionar
//   cargo run                          → debe dar error con contexto
//   cargo run -- /no/existe            → debe dar error con contexto
//
// Observa la diferencia entre {e}, {e:#} y {e:?} en la salida.
```

### Ejercicio 3 — thiserror + anyhow juntos

```rust
// Crea un mini-proyecto con dos crates en un workspace:
//
// my_parser/   (libreria con thiserror)
//   - Define ParseError con variantes:
//     - Io (con #[from] io::Error)
//     - Syntax { line: usize, message: String }
//     - UnexpectedEof
//   - Funcion pub parse_file(path) -> Result<Vec<String>, ParseError>
//     que lea un archivo y retorne las lineas no vacias
//     (error Syntax si una linea empieza con '!')
//
// my_app/      (binario con anyhow)
//   - Usa my_parser::parse_file
//   - Agrega .context() para el contexto de la aplicacion
//   - Imprime resultado o error con cadena completa
//
// Verifica que:
//   - Archivo inexistente → cadena: contexto app > contexto parser > io error
//   - Archivo con '!' → cadena: contexto app > Syntax error
//   - Archivo valido → imprime las lineas
```
