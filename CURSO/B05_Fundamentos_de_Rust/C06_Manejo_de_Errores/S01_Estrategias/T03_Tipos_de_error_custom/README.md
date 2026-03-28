# T03 — Tipos de error custom

## Por que crear tipos de error propios

Los tipos de error de la stdlib (`io::Error`, `ParseIntError`, etc.)
describen problemas genericos. Cuando tu aplicacion o libreria tiene
errores especificos de su dominio, necesitas tipos propios:

```rust
// Error generico — poca informacion para el llamador:
fn connect(addr: &str) -> Result<Connection, io::Error> { /* ... */ }
// El llamador solo sabe que hubo un "error de I/O"
// ¿Fue timeout? ¿DNS? ¿Conexion rechazada? ¿Autenticacion?

// Error custom — el llamador puede distinguir y actuar:
fn connect(addr: &str) -> Result<Connection, ConnectError> { /* ... */ }
// match err {
//     ConnectError::DnsResolution(host) => retry_with_ip(...),
//     ConnectError::Timeout(duration) => increase_timeout(...),
//     ConnectError::AuthFailed => ask_credentials(),
//     ConnectError::Io(e) => handle_io(e),
// }
```

```text
Beneficios de tipos de error propios:
1. El llamador puede hacer match sobre variantes especificas
2. Cada variante lleva datos relevantes (host, linea, campo, etc.)
3. Documentan todos los modos de fallo de tu API
4. Permiten conversiones automaticas con From (para usar ?)
5. Son parte del contrato publico de tu libreria
```

## El trait std::error::Error

Cualquier tipo de error en Rust deberia implementar el trait `Error`.
Este trait requiere `Display` y `Debug`, y opcionalmente provee
`source()` para encadenar errores:

```rust
// Definicion simplificada del trait en la stdlib:
pub trait Error: Display + Debug {
    // Retorna el error subyacente que causo este error (si existe)
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        None  // default: no hay causa subyacente
    }
}

// Requisitos:
// 1. impl Display — mensaje legible para el usuario
// 2. impl Debug  — representacion para desarrolladores
// 3. impl Error  — integra con el ecosistema (Box<dyn Error>, ?, etc.)
```

```text
¿Por que importa implementar Error?

Sin Error:
- Tu tipo no funciona con Box<dyn Error>
- No puedes usarlo como source() de otro error
- No se integra con librerias como anyhow
- ? no puede convertirlo automaticamente en muchos contextos

Con Error:
- Compatible con todo el ecosistema
- Funciona con Box<dyn Error>, anyhow::Error, etc.
- Encadenamiento de errores con source()
- Se puede usar con ? en funciones que retornan Box<dyn Error>
```

## Error como struct

Un struct es apropiado cuando el error tiene un solo "tipo" de fallo
con datos asociados:

```rust
use std::fmt;

// Error simple — un solo tipo de fallo
#[derive(Debug)]
struct ValidationError {
    field: String,
    message: String,
}

// Display — mensaje para el usuario
impl fmt::Display for ValidationError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "campo '{}': {}", self.field, self.message)
    }
}

// Error — se integra con el ecosistema
impl std::error::Error for ValidationError {}
// source() usa el default (None) — no hay error subyacente

fn validate_age(age: &str) -> Result<u8, ValidationError> {
    let n: u8 = age.parse().map_err(|_| ValidationError {
        field: "age".to_string(),
        message: format!("'{age}' no es un numero valido entre 0 y 255"),
    })?;

    if n > 150 {
        return Err(ValidationError {
            field: "age".to_string(),
            message: format!("{n} no es una edad realista"),
        });
    }

    Ok(n)
}
```

```rust
// Error struct con causa subyacente (source)
#[derive(Debug)]
struct ConfigError {
    path: String,
    cause: std::io::Error,
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "error leyendo config '{}': {}", self.path, self.cause)
    }
}

impl std::error::Error for ConfigError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.cause)  // encadena el io::Error como causa
    }
}

fn load_config(path: &str) -> Result<String, ConfigError> {
    std::fs::read_to_string(path).map_err(|e| ConfigError {
        path: path.to_string(),
        cause: e,
    })
}
```

```text
Cuando usar struct para errores:

✓ Solo hay un tipo de fallo (o muy pocos, sin variaciones)
✓ Todos los errores llevan los mismos datos
✓ Error simple con mensaje y contexto
✓ Wrapper de un solo error subyacente con contexto adicional

Ejemplos:
- ValidationError { field, message }
- ConfigError { path, cause }
- ParseError { line, column, message }
```

## Error como enum

Un enum es apropiado cuando hay **multiples tipos de fallo**
distintos, cada uno con datos diferentes:

```rust
use std::fmt;
use std::io;
use std::num::ParseIntError;

#[derive(Debug)]
enum AppError {
    // Variante que wrappea un error de I/O
    Io(io::Error),

    // Variante que wrappea un error de parsing
    Parse(ParseIntError),

    // Variante con datos custom
    InvalidConfig {
        key: String,
        reason: String,
    },

    // Variante simple sin datos
    NotFound,
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e) => write!(f, "error de I/O: {e}"),
            AppError::Parse(e) => write!(f, "error de parsing: {e}"),
            AppError::InvalidConfig { key, reason } => {
                write!(f, "config invalida — clave '{key}': {reason}")
            }
            AppError::NotFound => write!(f, "recurso no encontrado"),
        }
    }
}

impl std::error::Error for AppError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            AppError::Io(e)    => Some(e),     // io::Error es la causa
            AppError::Parse(e) => Some(e),     // ParseIntError es la causa
            _                  => None,        // sin causa subyacente
        }
    }
}
```

```text
Cuando usar enum para errores:

✓ Multiples modos de fallo (I/O, parse, validacion, etc.)
✓ El llamador necesita distinguir entre ellos (match)
✓ Cada variante tiene datos diferentes
✓ Es el patron mas comun en Rust — la mayoria de errores son enums
```

## Implementar From para conversion automatica con ?

Para que `?` convierta errores automaticamente, necesitas
implementar `From` de cada error fuente a tu error custom:

```rust
// Implementar From para cada tipo de error que quieras wrappear:

impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self {
        AppError::Io(e)
    }
}

impl From<ParseIntError> for AppError {
    fn from(e: ParseIntError) -> Self {
        AppError::Parse(e)
    }
}

// Ahora ? convierte automaticamente:
fn load_port(path: &str) -> Result<u16, AppError> {
    let content = std::fs::read_to_string(path)?;
    //           io::Error → From::from → AppError::Io
    let port = content.trim().parse::<u16>()?;
    //         ParseIntError → From::from → AppError::Parse
    Ok(port)
}
```

```rust
// Sin From, tendrias que usar map_err en cada llamada:
fn load_port_manual(path: &str) -> Result<u16, AppError> {
    let content = std::fs::read_to_string(path)
        .map_err(AppError::Io)?;
    let port = content.trim().parse::<u16>()
        .map_err(AppError::Parse)?;
    Ok(port)
}
// Ambas versiones son equivalentes — From es mas ergonomico.
```

```text
Patron:
1. Tu error enum tiene una variante que wrappea el error externo
2. Implementas From<ErrorExterno> for TuError
3. ? convierte automaticamente usando From

Nota: From debe ser una conversion sin perdida de informacion.
Si necesitas agregar contexto (path, linea, etc.), usa map_err.
```

## Receta completa: error enum profesional

Este es el patron completo que se usa en librerias Rust reales:

```rust
use std::fmt;
use std::io;
use std::num::ParseIntError;

// ── Paso 1: Definir el enum ──

/// Errores que pueden ocurrir al procesar archivos de datos.
#[derive(Debug)]
pub enum DataError {
    /// Error de lectura/escritura del sistema de archivos.
    Io(io::Error),
    /// Error al parsear un valor numerico.
    Parse {
        line: usize,
        value: String,
        source: ParseIntError,
    },
    /// Formato de archivo invalido.
    Format {
        line: usize,
        message: String,
    },
    /// Archivo vacio cuando se esperaba contenido.
    Empty,
}

// ── Paso 2: Implementar Display ──

impl fmt::Display for DataError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DataError::Io(e) => write!(f, "error de I/O: {e}"),
            DataError::Parse { line, value, .. } => {
                write!(f, "linea {line}: '{value}' no es un numero valido")
            }
            DataError::Format { line, message } => {
                write!(f, "linea {line}: {message}")
            }
            DataError::Empty => write!(f, "archivo vacio"),
        }
    }
}

// ── Paso 3: Implementar Error con source() ──

impl std::error::Error for DataError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            DataError::Io(e) => Some(e),
            DataError::Parse { source, .. } => Some(source),
            _ => None,
        }
    }
}

// ── Paso 4: Implementar From para conversiones simples ──

impl From<io::Error> for DataError {
    fn from(e: io::Error) -> Self {
        DataError::Io(e)
    }
}

// Nota: NO implementamos From<ParseIntError> porque nuestra variante
// Parse necesita datos extra (line, value) que From no puede proveer.
// Para esos casos usamos map_err.

// ── Paso 5: Usar ──

fn parse_data_file(path: &str) -> Result<Vec<i64>, DataError> {
    let content = std::fs::read_to_string(path)?;  // usa From<io::Error>

    if content.trim().is_empty() {
        return Err(DataError::Empty);
    }

    let mut values = Vec::new();

    for (i, line) in content.lines().enumerate() {
        let line = line.trim();

        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        // split por ":" — si no tiene ":", es formato invalido
        let (label, num_str) = line.split_once(':')
            .ok_or_else(|| DataError::Format {
                line: i + 1,
                message: "se esperaba formato 'etiqueta: valor'".to_string(),
            })?;

        // parsear el numero — map_err para agregar contexto
        let value: i64 = num_str.trim().parse()
            .map_err(|e| DataError::Parse {
                line: i + 1,
                value: num_str.trim().to_string(),
                source: e,
            })?;

        values.push(value);
    }

    Ok(values)
}
```

## source() y la cadena de errores

`source()` permite crear una cadena de errores — cada error
apunta a su causa raiz:

```rust
use std::error::Error;

// Imprimir toda la cadena de errores:
fn print_error_chain(err: &dyn Error) {
    eprintln!("error: {err}");
    let mut current = err.source();
    while let Some(cause) = current {
        eprintln!("  causado por: {cause}");
        current = cause.source();
    }
}

// Ejemplo de salida:
// error: linea 5: 'abc' no es un numero valido
//   causado por: invalid digit found in string
```

```rust
// Ejemplo de cadena de 3 niveles:
use std::io;

#[derive(Debug)]
struct DbError {
    query: String,
    source: io::Error,
}

impl std::fmt::Display for DbError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "error en query '{}'", self.query)
    }
}

impl std::error::Error for DbError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.source)
    }
}

#[derive(Debug)]
struct AppError {
    operation: String,
    source: DbError,
}

impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "fallo la operacion '{}'", self.operation)
    }
}

impl std::error::Error for AppError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        Some(&self.source)
    }
}

// La cadena seria:
// error: fallo la operacion 'cargar usuarios'
//   causado por: error en query 'SELECT * FROM users'
//     causado por: Connection refused (os error 111)
```

```text
Regla para source():
- Display muestra el error de ESTE nivel (sin incluir la causa)
- source() retorna la causa subyacente
- Las librerias de logging/reporte recorren la cadena completa

MAL: "error leyendo config: no such file or directory"
     (mete la causa en Display — duplica info si alguien recorre la cadena)

BIEN: Display → "error leyendo config '/etc/app.toml'"
      source() → io::Error("no such file or directory")
```

## Struct vs enum — arbol de decision

```text
¿Tu error tiene multiples variantes/modos de fallo?
├── NO (un solo tipo de fallo)
│   └── Struct
│       Ejemplo: ConfigError { path, cause }
│       - Simple, pocos campos
│       - El llamador no necesita distinguir variantes
│
└── SI (multiples modos de fallo)
    └── Enum
        Ejemplo: AppError { Io, Parse, NotFound, ... }
        - El llamador hace match para decidir como actuar
        - Cada variante puede tener datos distintos
        - Patron mas comun en Rust
```

```text
En la practica:
- Librerias → casi siempre enum (documentan todos los fallos posibles)
- Structs de error → para errores simples o wrappers con contexto
- Muchas librerias usan un enum principal + structs auxiliares:

  pub enum Error {
      Io(IoErrorDetail),        // struct con contexto
      Parse(ParseErrorDetail),  // struct con linea, columna
      Config(ConfigError),      // struct con path, clave
  }
```

## Convenciones del ecosistema

```rust
// Convencion 1: el tipo de error se llama "Error"
// y se reexporta como un type alias Result
pub mod mylib {
    // Tipo de error principal del crate
    #[derive(Debug)]
    pub enum Error {
        Io(std::io::Error),
        Parse(String),
    }

    // Alias para Result que usa nuestro Error
    pub type Result<T> = std::result::Result<T, Error>;

    // Las funciones publicas usan el alias:
    pub fn process(path: &str) -> Result<String> {
        // ...
        Ok(String::new())
    }
}

// El usuario del crate escribe:
use mylib;

fn main() -> mylib::Result<()> {
    let data = mylib::process("input.txt")?;
    Ok(())
}
```

```rust
// Convencion 2: implementar Display con mensajes cortos y claros
// (sin puntuacion final, sin mayuscula inicial)

// BIEN:
impl fmt::Display for MyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            MyError::NotFound(id) => write!(f, "user {id} not found"),
            MyError::InvalidInput(msg) => write!(f, "invalid input: {msg}"),
        }
    }
}

// MAL:
impl fmt::Display for MyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            // Mayuscula, punto final, causa duplicada
            MyError::NotFound(id) => write!(f, "Error: User {id} not found."),
            MyError::InvalidInput(msg) => write!(f, "Invalid input error: {msg}."),
        }
    }
}
```

```text
Convenciones:
1. Nombrar el tipo "Error" (o "XyzError" si hay ambiguedad)
2. Crear type Result<T> = std::result::Result<T, Error>
3. Display: minuscula, sin punto final, sin prefijo "error:"
4. source(): retorna la causa, no incluirla en Display
5. Derivar Debug (o implementarlo si quieres redactar datos)
6. Implementar From solo para conversiones directas sin perdida
7. Documentar cada variante del enum con doc comments
```

## Ejemplo integrador — libreria de CSV

```rust
use std::fmt;
use std::io;
use std::num::{ParseFloatError, ParseIntError};

// ── Tipo de error del crate ──

/// Errores que pueden ocurrir al leer archivos CSV.
#[derive(Debug)]
pub enum CsvError {
    /// Error de lectura del archivo.
    Io(io::Error),

    /// Numero incorrecto de columnas en una fila.
    ColumnCount {
        line: usize,
        expected: usize,
        found: usize,
    },

    /// Valor numerico invalido.
    InvalidNumber {
        line: usize,
        column: usize,
        value: String,
        source: NumberError,
    },

    /// Header ausente o malformado.
    BadHeader(String),
}

/// Error auxiliar para agrupar errores de parsing numerico.
#[derive(Debug)]
pub enum NumberError {
    Int(ParseIntError),
    Float(ParseFloatError),
}

impl fmt::Display for NumberError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NumberError::Int(e) => write!(f, "{e}"),
            NumberError::Float(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for NumberError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            NumberError::Int(e) => Some(e),
            NumberError::Float(e) => Some(e),
        }
    }
}

impl fmt::Display for CsvError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CsvError::Io(e) => write!(f, "error de lectura: {e}"),
            CsvError::ColumnCount { line, expected, found } => {
                write!(f, "linea {line}: se esperaban {expected} columnas, hay {found}")
            }
            CsvError::InvalidNumber { line, column, value, .. } => {
                write!(f, "linea {line}, columna {column}: '{value}' no es un numero")
            }
            CsvError::BadHeader(msg) => write!(f, "header invalido: {msg}"),
        }
    }
}

impl std::error::Error for CsvError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            CsvError::Io(e) => Some(e),
            CsvError::InvalidNumber { source, .. } => Some(source),
            _ => None,
        }
    }
}

impl From<io::Error> for CsvError {
    fn from(e: io::Error) -> Self {
        CsvError::Io(e)
    }
}

// ── Type alias ──

pub type Result<T> = std::result::Result<T, CsvError>;

// ── Funcion que usa el error ──

/// Lee un CSV con formato: nombre,edad,salario
pub fn parse_csv(path: &str) -> Result<Vec<(String, u32, f64)>> {
    let content = std::fs::read_to_string(path)?;  // From<io::Error>

    let mut lines = content.lines();

    // Validar header
    let header = lines.next()
        .ok_or_else(|| CsvError::BadHeader("archivo vacio".to_string()))?;

    if header.trim() != "nombre,edad,salario" {
        return Err(CsvError::BadHeader(
            format!("se esperaba 'nombre,edad,salario', encontrado '{header}'"),
        ));
    }

    let mut records = Vec::new();

    for (i, line) in lines.enumerate() {
        let line_num = i + 2;  // +2 por header y 0-indexing
        let fields: Vec<&str> = line.split(',').collect();

        // Validar numero de columnas
        if fields.len() != 3 {
            return Err(CsvError::ColumnCount {
                line: line_num,
                expected: 3,
                found: fields.len(),
            });
        }

        let name = fields[0].trim().to_string();

        let age: u32 = fields[1].trim().parse()
            .map_err(|e| CsvError::InvalidNumber {
                line: line_num,
                column: 2,
                value: fields[1].trim().to_string(),
                source: NumberError::Int(e),
            })?;

        let salary: f64 = fields[2].trim().parse()
            .map_err(|e| CsvError::InvalidNumber {
                line: line_num,
                column: 3,
                value: fields[2].trim().to_string(),
                source: NumberError::Float(e),
            })?;

        records.push((name, age, salary));
    }

    Ok(records)
}

// ── Uso ──

fn main() {
    match parse_csv("empleados.csv") {
        Ok(records) => {
            for (name, age, salary) in &records {
                println!("{name} ({age}): ${salary:.2}");
            }
        }
        Err(e) => {
            eprintln!("error: {e}");
            // Imprimir cadena de causas
            let mut source = std::error::Error::source(&e);
            while let Some(cause) = source {
                eprintln!("  causado por: {cause}");
                source = cause.source();
            }
            std::process::exit(1);
        }
    }
}
```

## Errores comunes

```rust
// ERROR 1: no implementar Error, solo Display
#[derive(Debug)]
struct MyError(String);

impl fmt::Display for MyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}
// Falta: impl std::error::Error for MyError {}
// Sin esto, MyError no funciona con Box<dyn Error> ni con source()

// ERROR 2: incluir la causa en Display (duplica al recorrer la cadena)
impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // MAL — la causa se imprimira dos veces si alguien recorre source()
        write!(f, "config error en '{}': {}", self.path, self.cause)
    }
}
// BIEN:
impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "config error en '{}'", self.path)
        // source() retorna self.cause por separado
    }
}

// ERROR 3: From que pierde informacion
impl From<io::Error> for AppError {
    fn from(_e: io::Error) -> Self {
        AppError::Generic("error de I/O".to_string())
        // Perdiste el io::Error original — imposible inspeccionar
    }
}
// BIEN:
impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self {
        AppError::Io(e)  // preserva el error original
    }
}

// ERROR 4: From cuando necesitas contexto adicional
impl From<ParseIntError> for DataError {
    fn from(e: ParseIntError) -> Self {
        DataError::Parse {
            line: 0,      // ¡no tenemos la linea aqui!
            value: "???".to_string(),  // ¡no tenemos el valor!
            source: e,
        }
    }
}
// SOLUCION: no implementar From, usar map_err con contexto
let val: i64 = num_str.parse()
    .map_err(|e| DataError::Parse {
        line: current_line,
        value: num_str.to_string(),
        source: e,
    })?;

// ERROR 5: demasiadas variantes en un solo enum
#[derive(Debug)]
enum Error {
    Io(io::Error),
    Parse(ParseIntError),
    Validation(String),
    Network(String),
    Database(String),
    Auth(String),
    Timeout(String),
    RateLimit(String),
    NotFound(String),
    Permission(String),
    Serialization(String),
    // ... 20 variantes mas
}
// Mejor: dividir en sub-enums por modulo
enum Error {
    Io(io::Error),
    Network(NetworkError),    // sub-enum con sus variantes
    Database(DatabaseError),  // sub-enum con sus variantes
    Auth(AuthError),          // sub-enum con sus variantes
}
```

## Cheatsheet

```text
Patron basico para error enum:

1. #[derive(Debug)] pub enum Error { ... }
2. impl Display for Error { match cada variante }
3. impl std::error::Error for Error { source() encadena }
4. impl From<Ext> for Error { wrappea errores externos }
5. pub type Result<T> = std::result::Result<T, Error>;

Checklist:
  [x] Debug    — para {:?} y dbg!
  [x] Display  — mensaje sin causa, minuscula, sin punto
  [x] Error    — source() retorna la causa si existe
  [x] From     — solo para conversiones directas sin perdida
  [x] type alias Result<T>

Struct vs Enum:
  struct → un solo modo de fallo, o wrapper con contexto
  enum   → multiples modos de fallo (lo mas comun)

Contexto con map_err vs From:
  From    → conversion automatica, sin datos extra
  map_err → conversion manual, puedes añadir linea, path, etc.
```

---

## Ejercicios

### Ejercicio 1 — Error struct basico

```rust
// Crea un struct ParseConfigError con:
//   - field: String (que campo fallo)
//   - message: String (que salio mal)
//
// Implementa Debug (derive), Display, y Error.
//
// Escribe una funcion parse_key_value(&str) -> Result<(String, String), ParseConfigError>
// que parsee "clave=valor" y retorne error si no tiene '='.
//
// Verifica:
//   parse_key_value("host=localhost") → Ok(("host", "localhost"))
//   parse_key_value("invalido")       → Err con mensaje descriptivo
//   println!("{}", err)               → debe imprimir algo legible
```

### Ejercicio 2 — Error enum con From

```rust
// Crea un enum FileDbError con variantes:
//   Io(std::io::Error)
//   Parse { line: usize, source: ParseIntError }
//   DuplicateKey(String)
//
// Implementa Display, Error (con source), y From<io::Error>.
// NO implementes From<ParseIntError> — necesitas el numero de linea.
//
// Escribe load_db(path) -> Result<HashMap<String, i64>, FileDbError>
// que lea un archivo con formato "clave:valor" por linea.
//
// Usa ? para io::Error (con From) y map_err para ParseIntError.
//
// Predice: ¿que imprime print_error_chain para cada tipo de error?
```

### Ejercicio 3 — Cadena de errores

```rust
// Construye una cadena de 3 niveles de error:
//
// 1. IoDetail — wrappea io::Error, agrega un path
// 2. DbError — wrappea IoDetail, agrega nombre de tabla
// 3. AppError — wrappea DbError, agrega nombre de operacion
//
// Cada nivel implementa Display (sin incluir la causa)
// y source() (retorna el nivel inferior).
//
// Escribe print_chain(err: &dyn Error) que recorra e imprima
// toda la cadena con indentacion.
//
// Simula un error y verifica que la cadena se imprime completa:
//   error: fallo la operacion 'cargar_usuarios'
//     causado por: error en tabla 'users'
//       causado por: no se pudo leer '/var/db/users.dat'
//         causado por: No such file or directory (os error 2)
```
