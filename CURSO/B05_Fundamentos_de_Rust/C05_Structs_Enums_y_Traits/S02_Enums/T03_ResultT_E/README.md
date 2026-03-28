# T03 — Result\<T, E\>

## Que es Result

Result es el enum estandar de Rust para representar operaciones que
pueden fallar. A diferencia de Option (que modela ausencia de valor),
Result modela **exito o error con informacion sobre la causa**.
Esta en el prelude, no necesita `use`.

```rust
// Definicion en la libreria estandar:
enum Result<T, E> {
    Ok(T),   // operacion exitosa, contiene el valor de tipo T
    Err(E),  // operacion fallida, contiene el error de tipo E
}

// Dos parametros de tipo:
//   T — tipo del valor en caso de exito
//   E — tipo del error en caso de fallo
//
// Comparacion con Option:
//   Option<T>    → Some(T) | None        (hay valor o no hay)
//   Result<T, E> → Ok(T)  | Err(E)      (exito con valor o error con causa)
//
// Result, Ok y Err estan en el prelude — se usan directamente.
```

## Crear Results

```rust
// Construccion manual:
let success: Result<i32, String> = Ok(42);
let failure: Result<i32, String> = Err(String::from("division by zero"));
let not_found: Result<i32, &str> = Err("file not found");
```

```rust
// En la practica, Result casi siempre viene de funciones que pueden fallar:
use std::fs::File;

fn main() {
    let file = File::open("config.txt");          // Result<File, io::Error>
    let number: Result<i32, _> = "42".parse();    // Result<i32, ParseIntError>
    let bad: Result<i32, _> = "abc".parse();      // Err(ParseIntError)
    let pi = "3.14".parse::<f64>();               // turbofish para especificar tipo
}
```

```rust
// Funciones propias que retornan Result:
fn divide(a: f64, b: f64) -> Result<f64, String> {
    if b == 0.0 {
        Err(String::from("cannot divide by zero"))
    } else {
        Ok(a / b)
    }
}
```

## Pattern matching con Result

```rust
use std::fs::File;
use std::io::ErrorKind;

fn main() {
    let result = File::open("data.txt");

    match result {
        Ok(file) => println!("Abierto: {:?}", file),
        Err(ref error) if error.kind() == ErrorKind::NotFound => {
            println!("Archivo no existe, creando...");
            match File::create("data.txt") {
                Ok(fc) => println!("Creado: {:?}", fc),
                Err(e) => println!("Error al crear: {}", e),
            }
        }
        Err(error) => println!("Otro error: {}", error),
    }
}
```

```rust
// if let y let else con Result:
fn main() {
    // if let — solo manejar el caso exitoso
    if let Ok(n) = "42".parse::<i32>() {
        println!("Parsed: {}", n);
    }

    // let else — manejar el error y salir temprano
    let Ok(n) = "42".parse::<i32>() else {
        println!("Failed to parse");
        return;
    };
    println!("Parsed: {}", n);
}
```

## La familia unwrap

Igual que Option, Result tiene metodos para extraer el valor
contenido, con diferentes estrategias ante errores:

```rust
fn main() {
    let ok: Result<i32, &str> = Ok(42);
    let err: Result<i32, &str> = Err("something went wrong");

    // unwrap() — extrae el valor o panic con el mensaje del error
    let val = ok.unwrap();       // 42
    // let val = err.unwrap();   // panic!

    // expect() — como unwrap pero con mensaje personalizado
    let val = ok.expect("failed to get value");  // 42

    // unwrap_or() — valor por defecto si es Err
    let val = err.unwrap_or(0);                  // 0

    // unwrap_or_else() — closure que recibe el error
    let val = err.unwrap_or_else(|e| {
        println!("Error: {}", e);
        -1
    });

    // unwrap_or_default() — usa Default::default() del tipo T
    let val: i32 = err.unwrap_or_default();      // 0
}
```

```rust
// Cuando usar cada uno:
//
// unwrap()            → prototipos, tests, error logicamente imposible
// expect("msg")       → documentar por que no deberia fallar
// unwrap_or(val)      → valor por defecto razonable
// unwrap_or_else(f)   → fallback con logica o costoso de calcular
// unwrap_or_default() → Default del tipo es el fallback adecuado
//
// En produccion, evitar unwrap/expect salvo que el error sea
// realmente imposible. Preferir ? o match.
```

## El operador ?

El operador `?` es la forma idiomatica de propagar errores en Rust.
Si el Result es Ok, extrae el valor. Si es Err, retorna el error
inmediatamente desde la funcion actual:

```rust
use std::fs::File;
use std::io::{self, Read};

// Sin ?, cada operacion necesita un match explicito:
//   let file = match File::open(path) {
//       Ok(f) => f,
//       Err(e) => return Err(e),
//   };

// Con ? — limpio y lineal:
fn read_file(path: &str) -> Result<String, io::Error> {
    let mut file = File::open(path)?;
    let mut contents = String::new();
    file.read_to_string(&mut contents)?;
    Ok(contents)
}

// ? equivale al match de arriba:
// 1. Si el Result es Ok(val) → extrae val y continua
// 2. Si el Result es Err(e)  → ejecuta return Err(e.into())
//
// El .into() es clave — permite conversion automatica de errores.
```

```rust
// ? solo funciona en funciones que retornan Result (o Option):

// fn main() {
//     let file = File::open("foo.txt")?;  // ERROR: main retorna ()
// }

// Solucion: hacer que la funcion retorne Result.
// (Ver seccion "main() retornando Result" mas adelante.)

// ? tambien funciona con Option, pero no se pueden mezclar:
fn find_first(numbers: &[i32]) -> Option<i32> {
    let first = numbers.first()?;  // retorna None si slice vacio
    Some(*first)
}

// Para convertir entre ambos:
//   result.ok()         → Result → Option (descarta error)
//   option.ok_or(err)   → Option → Result (proporciona error)
```

## From para conversion de errores

Cuando `?` encuentra un error de tipo diferente al que la funcion
retorna, llama `.into()` que busca una implementacion de `From`.
Esto permite combinar errores de distintas librerias:

```rust
use std::fmt;
use std::io;
use std::num::ParseIntError;

// Tipo de error propio:
#[derive(Debug)]
enum AppError {
    Io(io::Error),
    Parse(ParseIntError),
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e) => write!(f, "IO error: {}", e),
            AppError::Parse(e) => write!(f, "Parse error: {}", e),
        }
    }
}

// Implementar From para cada tipo de error que queremos convertir:
impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self { AppError::Io(e) }
}

impl From<ParseIntError> for AppError {
    fn from(e: ParseIntError) -> Self { AppError::Parse(e) }
}
```

```rust
// Ahora ? convierte automaticamente:
use std::fs;

fn read_number_from_file(path: &str) -> Result<i32, AppError> {
    let contents = fs::read_to_string(path)?;  // io::Error → AppError via From
    let number = contents.trim().parse()?;      // ParseIntError → AppError via From
    Ok(number)
}

// Mecanismo paso a paso con el primer ?:
// 1. fs::read_to_string retorna Result<String, io::Error>
// 2. Si es Err(io_error), ? ejecuta: return Err(io_error.into())
// 3. .into() busca impl From<io::Error> for AppError
// 4. Convierte a AppError::Io(io_error) y retorna
```

```rust
// Atajo: Box<dyn Error> acepta cualquier error sin implementar From.
// Util para prototipos y scripts, no recomendado para librerias:

use std::error::Error;
use std::fs;

fn read_number(path: &str) -> Result<i32, Box<dyn Error>> {
    let contents = fs::read_to_string(path)?;  // io::Error → Box<dyn Error>
    let number = contents.trim().parse()?;      // ParseIntError → Box<dyn Error>
    Ok(number)
}
// Funciona porque la libreria estandar implementa:
//   impl<E: Error> From<E> for Box<dyn Error>
```

## Transformaciones

Result tiene combinadores para transformar valores y errores
sin necesidad de match explicito:

```rust
fn main() {
    // map() — transforma el valor Ok, deja Err intacto
    let result: Result<i32, &str> = Ok(5);
    let doubled = result.map(|x| x * 2);       // Ok(10)
    let err: Result<i32, &str> = Err("fail");
    let doubled = err.map(|x| x * 2);          // Err("fail")

    // map_err() — transforma el error, deja Ok intacto
    let mapped = err.map_err(|e| format!("Error: {}", e));
    // Err("Error: fail")

    // and_then() — encadena operaciones que retornan Result
    let chained = result.and_then(|x| {
        if x > 0 { Ok(x * 2) } else { Err("must be positive") }
    });
    // Ok(10)
    // Diferencia con map:
    //   map:      closure retorna U       → Ok(U)
    //   and_then: closure retorna Result  → Result (no se anida)

    // or() / or_else() — alternativa si es Err
    let val = err.or(Ok(99));                   // Ok(99)
    let val = err.or_else(|_| Ok(0));           // Ok(0)
}
```

```rust
// Ejemplo practico — pipeline de transformaciones:
fn parse_and_validate(input: &str) -> Result<u16, String> {
    input
        .trim()
        .parse::<u16>()
        .map_err(|e| format!("parse error: {}", e))
        .and_then(|port| {
            if port >= 1024 {
                Ok(port)
            } else {
                Err(String::from("port must be >= 1024"))
            }
        })
}

fn main() {
    println!("{:?}", parse_and_validate("8080"));  // Ok(8080)
    println!("{:?}", parse_and_validate("80"));     // Err("port must be >= 1024")
    println!("{:?}", parse_and_validate("abc"));    // Err("parse error: ...")
}
```

## Result a Option y viceversa

```rust
fn main() {
    let ok: Result<i32, &str> = Ok(42);
    let err: Result<i32, &str> = Err("fail");

    // Result → Option
    let opt: Option<i32> = ok.ok();     // Some(42) — descarta error
    let opt: Option<i32> = err.ok();    // None
    let opt: Option<&str> = ok.err();   // None
    let opt: Option<&str> = err.err();  // Some("fail") — descarta valor

    // Option → Result
    let some: Option<i32> = Some(42);
    let none: Option<i32> = None;
    let r: Result<i32, &str> = some.ok_or("missing");        // Ok(42)
    let r: Result<i32, &str> = none.ok_or("missing");        // Err("missing")
    let r = none.ok_or_else(|| format!("not found"));        // lazy
}
```

```rust
// Patron comun: convertir Option a Result para usar ?
use std::collections::HashMap;

fn get_required(
    config: &HashMap<String, String>,
    key: &str,
) -> Result<String, String> {
    config
        .get(key)
        .cloned()
        .ok_or_else(|| format!("missing key: {}", key))
}

fn load_config(config: &HashMap<String, String>) -> Result<(String, u16), String> {
    let host = get_required(config, "host")?;
    let port: u16 = get_required(config, "port")?
        .parse()
        .map_err(|e| format!("invalid port: {}", e))?;
    Ok((host, port))
}
```

## Recolectar Results

Un iterador de Results se puede recolectar en un solo Result
que contiene un Vec de todos los valores exitosos, o el primer
error encontrado:

```rust
fn main() {
    // Todos validos → Result<Vec<T>, E> con collect()
    let strings = vec!["1", "2", "3", "4"];
    let numbers: Result<Vec<i32>, _> = strings
        .iter()
        .map(|s| s.parse::<i32>())
        .collect();
    println!("{:?}", numbers);  // Ok([1, 2, 3, 4])

    // Con error → se detiene en el primero
    let strings = vec!["1", "abc", "3", "def"];
    let numbers: Result<Vec<i32>, _> = strings
        .iter()
        .map(|s| s.parse::<i32>())
        .collect();
    println!("{:?}", numbers);  // Err(ParseIntError)
    // "3" y "def" ni se procesan (iteradores son lazy).
}
```

```rust
fn main() {
    let strings = vec!["1", "abc", "3", "def"];

    // Solo los exitos con filter_map:
    let numbers: Vec<i32> = strings
        .iter()
        .filter_map(|s| s.parse::<i32>().ok())
        .collect();
    println!("{:?}", numbers);  // [1, 3]

    // Separar exitos y errores con partition:
    let (oks, errs): (Vec<_>, Vec<_>) = strings
        .iter()
        .map(|s| s.parse::<i32>())
        .partition(Result::is_ok);
    let oks: Vec<i32> = oks.into_iter().map(Result::unwrap).collect();
    let errs: Vec<_> = errs.into_iter().map(Result::unwrap_err).collect();
    println!("Ok: {:?}, Err: {:?}", oks, errs);
    // Ok: [1, 3], Err: [invalid digit...]
}
```

## main() retornando Result

Por defecto, `main()` retorna `()`. Pero se puede hacer que
retorne `Result`, lo que permite usar `?` directamente en main:

```rust
use std::error::Error;
use std::fs;

fn main() -> Result<(), Box<dyn Error>> {
    let contents = fs::read_to_string("config.txt")?;
    let port: u16 = contents.trim().parse()?;
    println!("Port: {}", port);
    Ok(())
}

// Si main retorna Ok(()) → exit code 0.
// Si main retorna Err(e) → Rust imprime el error a stderr, exit code 1:
//   $ ./program
//   Error: No such file or directory (os error 2)
```

```rust
// Para control sobre el codigo de salida, patron main + run:
use std::process::ExitCode;

fn main() -> ExitCode {
    if let Err(e) = run() {
        eprintln!("Error: {}", e);
        return ExitCode::from(2);  // codigo personalizado
    }
    ExitCode::SUCCESS
}

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let contents = std::fs::read_to_string("data.txt")?;
    println!("{}", contents);
    Ok(())
}
// main() delega a run() y decide como manejar el error.
```

## Ejemplo completo

```rust
use std::error::Error;
use std::fmt;
use std::fs;
use std::num::ParseIntError;

#[derive(Debug)]
enum ConfigError {
    Io(std::io::Error),
    Parse(ParseIntError),
    Missing(String),
    Invalid(String),
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConfigError::Io(e) => write!(f, "io error: {}", e),
            ConfigError::Parse(e) => write!(f, "parse error: {}", e),
            ConfigError::Missing(key) => write!(f, "missing key: {}", key),
            ConfigError::Invalid(msg) => write!(f, "invalid: {}", msg),
        }
    }
}

impl Error for ConfigError {}

impl From<std::io::Error> for ConfigError {
    fn from(e: std::io::Error) -> Self { ConfigError::Io(e) }
}

impl From<ParseIntError> for ConfigError {
    fn from(e: ParseIntError) -> Self { ConfigError::Parse(e) }
}

fn read_config(path: &str) -> Result<(String, u16), ConfigError> {
    let content = fs::read_to_string(path)?;        // io::Error → ConfigError
    let mut host = None;
    let mut port = None;

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        let (k, v) = line.split_once('=')
            .ok_or_else(|| ConfigError::Invalid(format!("no '=' in: {}", line)))?;
        match k.trim() {
            "host" => host = Some(v.trim().to_string()),
            "port" => port = Some(v.trim().parse()?),   // ParseIntError → ConfigError
            _ => {}
        }
    }

    let host = host.ok_or_else(|| ConfigError::Missing("host".into()))?;
    let port = port.ok_or_else(|| ConfigError::Missing("port".into()))?;
    Ok((host, port))
}

fn main() -> Result<(), Box<dyn Error>> {
    let (host, port) = read_config("server.conf")?;
    println!("Server: {}:{}", host, port);
    Ok(())
}

// Flujo: main → read_config. Cada ? convierte errores via From.
// Si algo falla, sube hasta main donde Box<dyn Error> lo captura.
```

---

## Ejercicios

### Ejercicio 1 — Funciones con Result

```rust
// Implementar:
//
// 1. safe_divide(a: f64, b: f64) -> Result<f64, String>
//    Retorna Err si b es 0.0, Ok(a/b) en caso contrario.
//
// 2. parse_pair(input: &str) -> Result<(i32, i32), String>
//    Recibe "x,y" (ej: "10,20"). Retorna Err si no contiene ','
//    o si alguno no es entero valido. Usar split_once y parse.
//
// 3. main que llame ambas con inputs validos e invalidos,
//    usando match para imprimir resultados.
//
// Verificar:
//   safe_divide(10.0, 3.0)  → Ok(3.333...)
//   safe_divide(10.0, 0.0)  → Err("cannot divide by zero")
//   parse_pair("10,20")     → Ok((10, 20))
//   parse_pair("10")        → Err (sin coma)
//   parse_pair("a,b")       → Err (parse falla)
```

### Ejercicio 2 — Propagacion con ?

```rust
// Crear un programa que:
// 1. Lea "numbers.txt" (un numero por linea)
// 2. Parse cada linea a i64
// 3. Retorne la suma de todos los numeros
//
// Definir enum NumberError con variantes:
//   Io(std::io::Error)
//   Parse { line: usize, source: std::num::ParseIntError }
//
// Implementar From<io::Error> para NumberError.
// Para parse, usar map_err para agregar el numero de linea.
//
// sum_file(path: &str) -> Result<i64, NumberError> debe
// usar ? en todas las operaciones que pueden fallar.
//
// Verificar con archivo valido y uno con linea invalida.
```

### Ejercicio 3 — Collect y transformaciones

```rust
// Dado: let inputs = vec!["42", "bad", "7", "", "13", "nope"];
//
// Implementar:
//
// 1. parse_all_or_fail(inputs: &[&str]) -> Result<Vec<i32>, String>
//    Usar .collect() → falla en primer error.
//
// 2. parse_only_valid(inputs: &[&str]) -> Vec<i32>
//    Usar .filter_map() con .ok() → solo valores validos.
//
// 3. parse_partitioned(inputs: &[&str]) -> (Vec<i32>, Vec<String>)
//    Usar .partition() → separar exitos de errores.
//
// Verificar:
//   parse_all_or_fail → Err (falla en "bad")
//   parse_only_valid  → [42, 7, 13]
//   parse_partitioned → ([42, 7, 13], ["bad", "", "nope"])
```
