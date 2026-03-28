# T02 — Propagacion con ?

## El problema: boilerplate de match

Sin el operador `?`, propagar errores requiere un `match` explicito
en cada llamada que pueda fallar:

```rust
use std::fs;
use std::io;

// Sin ? — verbose y repetitivo
fn read_username_v1(path: &str) -> Result<String, io::Error> {
    let content = match fs::read_to_string(path) {
        Ok(c)  => c,
        Err(e) => return Err(e),  // retorno temprano con el error
    };

    let first_line = match content.lines().next() {
        Some(line) => line.to_string(),
        None       => return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "archivo vacio",
        )),
    };

    Ok(first_line)
}
```

```rust
// Con ? — la misma logica en una fraccion del codigo
fn read_username_v2(path: &str) -> Result<String, io::Error> {
    let content = fs::read_to_string(path)?;
    let first_line = content.lines().next()
        .ok_or_else(|| io::Error::new(
            io::ErrorKind::InvalidData,
            "archivo vacio",
        ))?;
    Ok(first_line)
}
```

```text
El operador ? hace exactamente esto:
1. Si el Result es Ok(v), extrae v y la ejecucion continua
2. Si el Result es Err(e), hace return Err(e.into()) inmediatamente

Es azucar sintactica para el patron match-Ok-Err-return.
```

## Mecanica exacta del operador ?

```rust
// Cuando escribes:
let value = some_function()?;

// El compilador genera algo equivalente a:
let value = match some_function() {
    Ok(v) => v,
    Err(e) => return Err(From::from(e)),
    //                    ^^^^^^^^^^^^^^
    //        Nota el .into() / From::from()
    //        Esto permite conversion automatica de tipos de error
};
```

```rust
// Ejemplo paso a paso:
use std::fs::File;
use std::io::{self, Read};

fn read_file(path: &str) -> Result<String, io::Error> {
    //  ┌── si File::open retorna Err(e), ejecuta return Err(e)
    //  │   si retorna Ok(file), asigna file a f
    //  ▼
    let mut f = File::open(path)?;

    let mut contents = String::new();
    //  ┌── si read_to_string retorna Err(e), ejecuta return Err(e)
    //  │   si retorna Ok(bytes), asigna bytes (no lo usamos)
    //  ▼
    f.read_to_string(&mut contents)?;

    Ok(contents)
}
```

```rust
// ? se puede encadenar directamente:
fn read_file_compact(path: &str) -> Result<String, io::Error> {
    let mut contents = String::new();
    File::open(path)?.read_to_string(&mut contents)?;
    //             ^                               ^
    //    primer ? — abre el archivo       segundo ? — lee el contenido
    Ok(contents)
}
```

## ? solo funciona en funciones que retornan Result (o Option)

```rust
// ESTO NO COMPILA:
fn main() {
    let content = fs::read_to_string("file.txt")?;
    //           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // error[E0277]: the `?` operator can only be used in a
    // function that returns `Result` or `Option`
    println!("{content}");
}
```

```rust
// Solucion 1: main retorna Result
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = fs::read_to_string("file.txt")?;
    println!("{content}");
    Ok(())  // necesario — main debe retornar Ok al final
}
// Si retorna Err, Rust imprime "Error: <mensaje>" y sale con codigo 1.

// Solucion 2: manejar el error en main con match
fn main() {
    match fs::read_to_string("file.txt") {
        Ok(content) => println!("{content}"),
        Err(e)      => eprintln!("error: {e}"),
    }
}

// Solucion 3: extraer la logica a una funcion que retorna Result
fn run() -> Result<(), Box<dyn std::error::Error>> {
    let content = fs::read_to_string("file.txt")?;
    println!("{content}");
    Ok(())
}

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}
```

```text
Regla: ? necesita que la funcion retorne:
- Result<T, E>  para usar ? con Result
- Option<T>     para usar ? con Option
- Cualquier tipo que implemente FromResidual (avanzado, nightly)

Si la funcion retorna otro tipo (o nada), ? no compila.
```

## Conversion automatica con From

La parte mas poderosa de `?` es que llama a `From::from(e)` sobre
el error antes de retornarlo. Esto permite que una funcion que
retorna `Result<T, MiError>` use `?` con cualquier tipo de error
que implemente `From<OtroError> for MiError`:

```rust
use std::fs;
use std::io;
use std::num::ParseIntError;

#[derive(Debug)]
enum AppError {
    Io(io::Error),
    Parse(ParseIntError),
}

// Implementar From para cada tipo de error fuente:
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

fn read_port_from_file(path: &str) -> Result<u16, AppError> {
    let content = fs::read_to_string(path)?;
    //           ^^^^^^^^^^^^^^^^^^^^^^^^ retorna io::Error
    //           ? llama From::from(io::Error) → AppError::Io

    let port = content.trim().parse::<u16>()?;
    //         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^ retorna ParseIntError
    //         ? llama From::from(ParseIntError) → AppError::Parse

    Ok(port)
}
```

```text
Flujo de conversion con ?:

fs::read_to_string(path)    → Result<String, io::Error>
                         ?  → Err(io::Error)
                            → Err(From::from(io::Error))
                            → Err(AppError::Io(io::Error))
                            → return Err(AppError::Io(...))

content.trim().parse()      → Result<u16, ParseIntError>
                         ?  → Err(ParseIntError)
                            → Err(From::from(ParseIntError))
                            → Err(AppError::Parse(ParseIntError))
                            → return Err(AppError::Parse(...))
```

```rust
// Sin la conversion automatica, tendrias que hacer esto:
fn read_port_manual(path: &str) -> Result<u16, AppError> {
    let content = fs::read_to_string(path)
        .map_err(AppError::Io)?;            // conversion manual
    let port = content.trim().parse::<u16>()
        .map_err(AppError::Parse)?;         // conversion manual
    Ok(port)
}

// Con From implementado, ? hace la conversion por ti.
// Ambas versiones son equivalentes.
```

## El atajo Box<dyn Error>

Si no quieres definir un tipo de error propio, puedes usar
`Box<dyn std::error::Error>` como tipo de error generico.
Cualquier error que implemente el trait `Error` se convierte
automaticamente:

```rust
use std::error::Error;
use std::fs;

fn process(path: &str) -> Result<u64, Box<dyn Error>> {
    let content = fs::read_to_string(path)?;    // io::Error → Box<dyn Error>
    let number: u64 = content.trim().parse()?;  // ParseIntError → Box<dyn Error>
    Ok(number * 2)
}

fn main() -> Result<(), Box<dyn Error>> {
    let result = process("number.txt")?;
    println!("resultado: {result}");
    Ok(())
}
```

```text
Ventajas de Box<dyn Error>:
+ No necesitas definir un tipo de error propio
+ Funciona con cualquier error de la stdlib y de crates
+ Perfecto para prototipos, scripts, y main()

Desventajas:
- Pierde informacion del tipo concreto del error
- No puedes hacer match sobre el tipo de error facilmente
- Requiere allocacion en el heap (Box)
- No implementa Clone (Box<dyn Error> no es Clone)

Regla practica:
- Binarios/prototipos → Box<dyn Error> esta bien
- Librerias → define tu propio tipo de error
```

## ? con Option

El operador `?` tambien funciona con `Option`. Si el valor es
`None`, retorna `None` inmediatamente:

```rust
fn first_even(numbers: &[i32]) -> Option<i32> {
    let first = numbers.first()?;  // None si el slice esta vacio
    if first % 2 == 0 {
        Some(*first)
    } else {
        None
    }
}

fn get_middle_name(full_name: &str) -> Option<&str> {
    let mut parts = full_name.split_whitespace();
    let _first = parts.next()?;   // None si no hay nombre
    let middle = parts.next()?;   // None si no hay segundo nombre
    let _last = parts.next()?;    // None si no hay apellido
    // Solo retorna Some si hay al menos 3 partes
    Some(middle)
}
```

```rust
// ? con Option es especialmente util para navegar estructuras anidadas:
struct Company {
    ceo: Option<Person>,
}

struct Person {
    address: Option<Address>,
}

struct Address {
    city: Option<String>,
}

fn get_ceo_city(company: &Company) -> Option<&str> {
    let ceo = company.ceo.as_ref()?;
    let address = ceo.address.as_ref()?;
    let city = address.city.as_ref()?;
    Some(city.as_str())
}

// Equivalente sin ?:
fn get_ceo_city_verbose(company: &Company) -> Option<&str> {
    match &company.ceo {
        Some(ceo) => match &ceo.address {
            Some(address) => match &address.city {
                Some(city) => Some(city.as_str()),
                None => None,
            },
            None => None,
        },
        None => None,
    }
}
```

## No se puede mezclar ? de Result y Option

```rust
// ESTO NO COMPILA — mezcla Result y Option:
fn mixed(path: &str) -> Result<char, io::Error> {
    let content = fs::read_to_string(path)?;   // Result — OK
    let first = content.chars().next()?;        // Option — ERROR
    //                                 ^
    // error: the `?` operator can only be used on `Result`s
    // in a function that returns `Result`
    Ok(first)
}
```

```rust
// Solucion 1: convertir Option a Result con ok_or / ok_or_else
fn mixed_fixed(path: &str) -> Result<char, io::Error> {
    let content = fs::read_to_string(path)?;
    let first = content.chars().next()
        .ok_or_else(|| io::Error::new(
            io::ErrorKind::InvalidData,
            "archivo vacio"
        ))?;  // Ahora es Result → ? funciona
    Ok(first)
}

// Solucion 2: si la funcion retorna Option, convertir Result a Option con .ok()
fn first_char_of_file(path: &str) -> Option<char> {
    let content = fs::read_to_string(path).ok()?;  // Result → Option
    content.chars().next()  // ya es Option
}
```

```text
Regla: dentro de una funcion, todos los ? deben ser del mismo "sabor":
- Si la funcion retorna Result → ? solo con Result
- Si la funcion retorna Option → ? solo con Option

Conversiones:
  Option → Result:  .ok_or(err)  o  .ok_or_else(|| err)
  Result → Option:  .ok()  (descarta el error)
                    .err() (descarta el valor)
```

## Retorno temprano — ? es un return implicito

Es importante entender que `?` causa un `return`. Esto tiene
implicaciones para el control de flujo:

```rust
use std::fs;
use std::io;

fn process_files(paths: &[&str]) -> Result<Vec<String>, io::Error> {
    let mut results = Vec::new();

    for path in paths {
        // Si CUALQUIER archivo falla, toda la funcion retorna Err
        let content = fs::read_to_string(path)?;
        results.push(content);
    }

    Ok(results)
    // Si hay 100 archivos y el 50° falla, los 49 anteriores se
    // descartan y la funcion retorna el error del 50°.
}
```

```rust
// Si quieres continuar aunque algunos fallen, NO uses ?:
fn process_files_resilient(paths: &[&str]) -> Vec<Result<String, io::Error>> {
    paths.iter()
        .map(|path| fs::read_to_string(path))
        .collect()
    // Retorna un Vec con Ok y Err mezclados
}

// O separar exitos de fallos:
fn process_files_partitioned(paths: &[&str]) -> (Vec<String>, Vec<io::Error>) {
    let mut successes = Vec::new();
    let mut failures = Vec::new();

    for path in paths {
        match fs::read_to_string(path) {
            Ok(content) => successes.push(content),
            Err(e)      => failures.push(e),
        }
    }

    (successes, failures)
}
```

## ? en closures

`?` en una closure retorna de **la closure**, no de la funcion
que la contiene:

```rust
fn process(items: &[&str]) -> Result<Vec<i32>, std::num::ParseIntError> {
    // ? dentro del map retorna Err DE LA CLOSURE
    // collect() recolecta Result<Vec<i32>, ParseIntError>
    let numbers: Result<Vec<i32>, _> = items.iter()
        .map(|s| s.parse::<i32>())  // cada parse retorna Result
        .collect();                 // collect sabe juntar Results

    // Ahora ? retorna de process si numbers es Err
    let numbers = numbers?;
    Ok(numbers)
}

// Forma compacta:
fn process_compact(items: &[&str]) -> Result<Vec<i32>, std::num::ParseIntError> {
    items.iter()
        .map(|s| s.parse::<i32>())
        .collect()  // infiere Result<Vec<i32>, ParseIntError>
}
```

```rust
// Cuidado: ? dentro de una closure no propaga al scope exterior
fn example() -> Result<(), Box<dyn std::error::Error>> {
    let paths = vec!["a.txt", "b.txt"];

    // ESTO NO COMPILA si la closure no retorna Result:
    // paths.iter().for_each(|p| {
    //     let content = fs::read_to_string(p)?;  // error
    //     println!("{content}");
    // });

    // Solucion: usar for en vez de for_each
    for p in &paths {
        let content = fs::read_to_string(p)?;  // ? retorna de example()
        println!("{content}");
    }

    Ok(())
}
```

## Encadenar ? con metodos

```rust
use std::fs;
use std::io::{self, BufRead, BufReader};

// ? funciona bien encadenado porque extrae el valor Ok:
fn count_lines(path: &str) -> Result<usize, io::Error> {
    let file = fs::File::open(path)?;
    let reader = BufReader::new(file);
    let count = reader.lines().count();
    //                 ^^^^^^^^
    // lines() retorna un iterador de Result<String, io::Error>
    // count() cuenta todos los Items (incluyendo los Err)
    Ok(count)
}

// Si quieres que lines() falle en el primer error:
fn count_lines_strict(path: &str) -> Result<usize, io::Error> {
    let file = fs::File::open(path)?;
    let reader = BufReader::new(file);
    let lines: Result<Vec<String>, io::Error> = reader.lines().collect();
    let lines = lines?;
    Ok(lines.len())
}

// Forma idiomatica — collect en Result:
fn count_lines_idiomatic(path: &str) -> Result<usize, io::Error> {
    let count = fs::File::open(path)
        .map(BufReader::new)?
        .lines()
        .collect::<Result<Vec<_>, _>>()?
        .len();
    Ok(count)
}
```

## Agregar contexto con map_err

A veces el error original no tiene suficiente informacion. Puedes
usar `map_err` **antes** de `?` para añadir contexto:

```rust
use std::fs;
use std::io;

fn load_config(path: &str) -> Result<Config, String> {
    let content = fs::read_to_string(path)
        .map_err(|e| format!("no se pudo leer '{path}': {e}"))?;
    //  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //  transforma io::Error en String con contexto adicional

    let config: Config = toml::from_str(&content)
        .map_err(|e| format!("TOML invalido en '{path}': {e}"))?;

    Ok(config)
}
```

```rust
// map_err es especialmente util para distinguir errores del mismo tipo:
fn setup() -> Result<(), io::Error> {
    // Sin contexto, si falla, no sabes CUAL archivo fue:
    let config = fs::read_to_string("/etc/app/config.toml")?;
    let template = fs::read_to_string("/etc/app/template.html")?;

    // Con contexto:
    let config = fs::read_to_string("/etc/app/config.toml")
        .map_err(|e| io::Error::new(e.kind(), format!("config: {e}")))?;
    let template = fs::read_to_string("/etc/app/template.html")
        .map_err(|e| io::Error::new(e.kind(), format!("template: {e}")))?;

    Ok(())
}
```

```text
Cuando usar map_err:
- El tipo de error destino no tiene From implementado
- Quieres añadir contexto (que archivo, que operacion, que input)
- El error original es demasiado generico (ej: io::Error sin path)

Alternativa en el ecosistema:
- anyhow::Context → .context("mensaje")?  (tema T04)
```

## Ejemplo integrador

```rust
use std::collections::HashMap;
use std::fs;
use std::io;
use std::path::Path;

#[derive(Debug)]
enum DbError {
    Io(io::Error),
    InvalidFormat { line: usize, content: String },
    DuplicateKey(String),
}

impl std::fmt::Display for DbError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DbError::Io(e) => write!(f, "I/O: {e}"),
            DbError::InvalidFormat { line, content } => {
                write!(f, "formato invalido en linea {line}: '{content}'")
            }
            DbError::DuplicateKey(k) => write!(f, "clave duplicada: '{k}'"),
        }
    }
}

impl From<io::Error> for DbError {
    fn from(e: io::Error) -> Self {
        DbError::Io(e)
    }
}

/// Carga un archivo de texto con formato "clave=valor" en un HashMap.
/// Demuestra multiples usos de ? con conversion automatica.
fn load_db(path: &Path) -> Result<HashMap<String, String>, DbError> {
    // ? convierte io::Error → DbError::Io via From
    let content = fs::read_to_string(path)?;

    let mut db = HashMap::new();

    for (i, line) in content.lines().enumerate() {
        let line = line.trim();

        // Saltar vacias y comentarios
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        // ? con ok_or_else — convierte Option → Result
        let (key, value) = line.split_once('=')
            .ok_or_else(|| DbError::InvalidFormat {
                line: i + 1,
                content: line.to_string(),
            })?;

        let key = key.trim().to_string();
        let value = value.trim().to_string();

        // Verificar duplicados
        if db.contains_key(&key) {
            return Err(DbError::DuplicateKey(key));
        }

        db.insert(key, value);
    }

    Ok(db)
}

/// Busca un valor y lo parsea como u16.
fn get_port(db: &HashMap<String, String>) -> Result<u16, DbError> {
    let port_str = db.get("port")
        .ok_or_else(|| DbError::InvalidFormat {
            line: 0,
            content: "clave 'port' no encontrada".to_string(),
        })?;

    port_str.parse::<u16>()
        .map_err(|e| DbError::InvalidFormat {
            line: 0,
            content: format!("'port' no es un numero: {e}"),
        })
    // No necesita ? al final — retorna directamente el Result
}

fn run() -> Result<(), DbError> {
    let db = load_db(Path::new("server.conf"))?;
    let port = get_port(&db)?;
    println!("servidor listo en puerto {port}");
    Ok(())
}

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}
```

```text
Resumen de tecnicas usadas en el ejemplo:

1. fs::read_to_string(path)?     → io::Error convertido a DbError via From
2. .split_once('=').ok_or(..)?   → Option convertida a Result, luego ?
3. port_str.parse().map_err(..)  → Result con tipo de error transformado
4. run() usa ? para propagar DbError directamente
5. main() captura el error final y hace exit con mensaje
```

## Errores comunes

```rust
// ERROR 1: usar ? en una funcion que retorna ()
fn do_stuff() {
    let x = "42".parse::<i32>()?;  // error: ? needs Result/Option return
}
// Solucion: cambiar la firma a retornar Result
fn do_stuff() -> Result<(), Box<dyn std::error::Error>> {
    let x = "42".parse::<i32>()?;
    Ok(())
}

// ERROR 2: ? con tipos de error incompatibles sin From
fn mixed_errors() -> Result<(), io::Error> {
    let n: i32 = "abc".parse()?;
    //                        ^
    // error: `?` couldn't convert ParseIntError to io::Error
    // porque no existe impl From<ParseIntError> for io::Error
    Ok(())
}
// Solucion: usar map_err, un error propio con From, o Box<dyn Error>
fn mixed_errors() -> Result<(), Box<dyn std::error::Error>> {
    let n: i32 = "abc".parse()?;  // ahora ambos errores se convierten
    Ok(())
}

// ERROR 3: olvidar Ok(()) al final de una funcion que retorna Result
fn process() -> Result<(), io::Error> {
    fs::write("out.txt", "hola")?;
    // error: expected `Result<(), Error>`, found `()`
}
// Solucion: agregar Ok(()) al final
fn process() -> Result<(), io::Error> {
    fs::write("out.txt", "hola")?;
    Ok(())
}

// ERROR 4: ? en closure dentro de for_each
fn read_all(paths: &[&str]) -> Result<(), io::Error> {
    paths.iter().for_each(|p| {
        fs::read_to_string(p)?;  // error: closure returns (), not Result
    });
    Ok(())
}
// Solucion: usar try_for_each o un for loop
fn read_all(paths: &[&str]) -> Result<(), io::Error> {
    for p in paths {
        fs::read_to_string(p)?;  // ? retorna de read_all
    }
    Ok(())
}

// ERROR 5: mezclar ? de Result y Option en la misma funcion
fn find_value(data: &HashMap<String, String>) -> Result<i32, String> {
    let val = data.get("key")?;  // ? sobre Option, pero funcion retorna Result
    Ok(val.parse().unwrap())
}
// Solucion: convertir Option a Result con ok_or
fn find_value(data: &HashMap<String, String>) -> Result<i32, String> {
    let val = data.get("key")
        .ok_or_else(|| "clave no encontrada".to_string())?;
    val.parse::<i32>()
        .map_err(|e| format!("parse error: {e}"))
}
```

## Cheatsheet

```text
Sintaxis                         Efecto
──────────────────────────────────────────────────────────────
expr?                            Si Err(e): return Err(e.into())
                                 Si Ok(v):  extrae v

expr.map_err(|e| ...)?          Transforma el error antes de ?

opt.ok_or(err)?                 Option → Result, luego ?
opt.ok_or_else(|| err)?         Option → Result (lazy), luego ?

res.ok()?                       Result → Option (descarta Err), luego ?

.collect::<Result<Vec<_>, _>>() Recolecta iterador de Results

main() -> Result<(), E>         Permite ? en main

try_for_each(|x| { ... })      for_each que soporta ?
```

---

## Ejercicios

### Ejercicio 1 — Refactorizar con ?

```rust
// Reescribe esta funcion usando ? en vez de match.
// El comportamiento debe ser identico.

use std::fs;
use std::io;

fn read_first_line(path: &str) -> Result<String, io::Error> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(e) => return Err(e),
    };
    match content.lines().next() {
        Some(line) => Ok(line.to_string()),
        None => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "archivo vacio",
        )),
    }
}

// Predice: ¿cuantas lineas tendra la version con ??
// Verifica tu prediccion.
```

### Ejercicio 2 — Multiples tipos de error

```rust
// Escribe una funcion sum_from_file que:
//   1. Lea un archivo (puede dar io::Error)
//   2. Parsee cada linea como i64 (puede dar ParseIntError)
//   3. Retorne la suma
//
// Define un enum SumError con variantes Io y Parse.
// Implementa From<io::Error> y From<ParseIntError> para SumError.
// Usa ? para propagar ambos tipos de error.
//
// Bonus: ¿que pasa si una linea esta vacia? Decide como manejarlo.
```

### Ejercicio 3 — ? con Option

```rust
// Dada esta estructura:
struct Config {
    database: Option<DbConfig>,
}

struct DbConfig {
    connection: Option<ConnConfig>,
}

struct ConnConfig {
    host: Option<String>,
}

// Escribe una funcion get_db_host(&Config) -> Option<&str>
// que navegue la estructura usando ? con Option.
//
// Luego escribe otra version get_db_host_or_err(&Config) -> Result<&str, &'static str>
// que retorne un mensaje de error descriptivo indicando QUE campo falta.
//
// Predice: ¿cual version es mas corta? ¿cual es mas informativa?
```
