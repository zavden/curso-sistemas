# T04 — 'static

## Que significa 'static

`'static` es el lifetime mas largo que existe en Rust. Aparece
en dos contextos con significados distintos:

1. Como **lifetime de referencia**: `&'static T` — una referencia
   que apunta a datos que existen durante todo el programa.
2. Como **trait bound**: `T: 'static` — un tipo que no contiene
   referencias con lifetimes menores que `'static`.

Confundir estos dos usos es el error conceptual mas comun sobre
`'static` en Rust.

```rust
// Contexto 1: lifetime de referencia
let s: &'static str = "hello"; // vive en el binario

// Contexto 2: trait bound
fn takes_static<T: 'static>(val: T) { /* ... */ }
takes_static(String::from("hello")); // compila — String es owned
```

## 'static como lifetime de referencia

Una referencia `&'static T` apunta a datos que nunca se liberan.
El ejemplo mas comun son los string literals:

```rust
// Los string literals se almacenan en el segmento de datos del binario.
// No estan en el stack ni en el heap — estan en el ejecutable mismo.
let greeting: &'static str = "Hello, world!";

// El compilador infiere 'static cuando el valor es un literal:
let greeting: &str = "Hello, world!"; // tambien es &'static str
```

```rust
// Puedes retornar &'static str desde funciones sin problema.
fn status_message(code: u32) -> &'static str {
    match code {
        200 => "OK",
        404 => "Not Found",
        500 => "Internal Server Error",
        _   => "Unknown",
    }
}
// Cada rama retorna un string literal → &'static str.
// No hay problema de lifetimes porque los datos viven para siempre.
```

## Fuentes de referencias 'static

No solo los string literals producen `&'static`. Hay varias fuentes:

```rust
// Fuente 1: string literals y byte strings
let s: &'static str = "embedded in binary";
let bytes: &'static [u8] = b"raw bytes";
let header: &'static [u8; 4] = b"\x89PNG";
```

```rust
// Fuente 2: items static — viven durante toda la ejecucion
static COUNTER: u32 = 0;
static APP_NAME: &str = "my_app";

fn get_app_name() -> &'static str {
    APP_NAME
}

// static mut existe pero requiere unsafe para acceder:
static mut GLOBAL_COUNT: u32 = 0;
unsafe { GLOBAL_COUNT += 1; }
// static mut es peligroso — preferir atomics o Mutex.
```

```rust
// Fuente 3: items const — el compilador puede crear un static implicito
const MAX_RETRIES: u32 = 3;
const DEFAULT_NAME: &str = "anonymous";

fn get_default() -> &'static str {
    DEFAULT_NAME
}
```

```rust
// Fuente 4: Box::leak — se cubre en detalle mas adelante
let leaked: &'static str = Box::leak(String::from("leaked").into_boxed_str());

// Fuente 5: lazy inicializacion (OnceLock, LazyLock)
// Se cubre en la seccion de patrones comunes.
```

## 'static como trait bound

Aqui es donde la mayoria se confunde. `T: 'static` como bound
**no** significa "T es una referencia que vive para siempre".
Significa: "T no contiene referencias con lifetimes menores que
`'static`". Los tipos owned satisfacen esto trivialmente:

```rust
fn print_static<T: std::fmt::Debug + 'static>(val: T) {
    println!("{:?}", val);
}

// Todos estos compilan — son tipos owned sin referencias internas:
print_static(42_i32);
print_static(String::from("owned"));
print_static(vec![1, 2, 3]);
print_static(true);

// Esto NO compila — es una referencia con lifetime local:
let local = String::from("hello");
let r: &String = &local;
// print_static(r);
// Error: `local` does not live long enough
```

```rust
// Tabla de referencia:
//
// Tipo                          | T: 'static? | Porque
// ----------------------------- | ----------- | ------
// i32, u8, f64, bool            | Si          | Sin referencias
// String                        | Si          | Posee sus datos
// Vec<i32>, Vec<String>         | Si          | Owned recursivamente
// &'static str                  | Si          | La referencia es 'static
// &'a str (donde 'a < 'static) | No          | Referencia temporal
// Struct con campo &'a          | No          | Contiene referencia no-'static
```

## La confusion con thread::spawn

El caso mas visible de `T: 'static` como bound es `thread::spawn`:

```rust
// Firma simplificada:
// pub fn spawn<F>(f: F) -> JoinHandle<T>
// where F: FnOnce() -> T + Send + 'static,
//       T: Send + 'static,
//
// El bound 'static significa: el closure no puede capturar
// referencias locales (el thread puede vivir mas que la funcion).
```

```rust
// INCORRECTO — capturar referencia local:
fn start_thread() {
    let data = vec![1, 2, 3];
    let data_ref = &data;

    // std::thread::spawn(move || {
    //     println!("{:?}", data_ref);
    // });
    // Error: `data` does not live long enough
    // El thread podria seguir corriendo despues de que start_thread retorne.
}
```

```rust
// CORRECTO — mover el valor owned al closure:
fn start_thread() {
    let data = vec![1, 2, 3];
    std::thread::spawn(move || {
        println!("{:?}", data); // data se MUEVE al closure
    });
    // Vec<i32> satisface 'static — no hay dangling reference.
}
```

```rust
// CORRECTO — clonar o usar Arc para datos compartidos:
use std::sync::Arc;

fn start_threads() {
    let data = Arc::new(vec![1, 2, 3]);
    for i in 0..3 {
        let data_clone = Arc::clone(&data);
        std::thread::spawn(move || {
            println!("Thread {}: {:?}", i, data_clone);
        });
    }
    // Arc<Vec<i32>> satisface 'static (tipo owned, sin referencias).
}
```

## Referencias 'static en la practica

```rust
// &'static str vs String:
//
// &'static str: referencia a datos que viven para siempre
//   - Tipicamente string literals o datos leakeados
//   - Costo cero (apunta al binario)
//
// String: posee sus datos en el heap
//   - Se libera cuando sale de scope (Drop)
//   - Satisface T: 'static (bound), pero NO es &'static str

let literal: &'static str = "zero cost";
let owned: String = String::from("heap allocated");
let borrowed: &str = &owned; // lifetime atado a `owned`, NO 'static
```

```rust
// Patron comun: metodos que retornan &'static str mapeando valores:

#[derive(Debug)]
enum LogLevel { Error, Warn, Info, Debug }

impl LogLevel {
    fn as_str(&self) -> &'static str {
        match self {
            LogLevel::Error => "ERROR",
            LogLevel::Warn  => "WARN",
            LogLevel::Info  => "INFO",
            LogLevel::Debug => "DEBUG",
        }
    }
}
// Retorna &'static str, no &'self str — los literals no dependen de self.
```

## Box::leak y 'static intencional

`Box::leak` consume un `Box<T>` y retorna `&'static mut T`.
Los datos nunca se liberan — se "filtran" intencionalmente:

```rust
let boxed = Box::new(42);
let leaked: &'static i32 = Box::leak(boxed);
println!("{}", leaked); // 42
// leaked apunta a memoria del heap que nunca se va a liberar.
```

```rust
// Caso de uso principal: configuracion cargada una vez al inicio.
struct Config {
    db_url: String,
    port: u16,
    workers: usize,
}

fn load_config() -> &'static Config {
    let config = Config {
        db_url: String::from("postgres://localhost/mydb"),
        port: 8080,
        workers: 4,
    };
    Box::leak(Box::new(config))
}
// Cualquier parte del programa puede tener &'static Config.
// No necesita Arc, Mutex, ni lifetime parameters.
```

```rust
// Box::leak con String → &'static str:
fn make_static_str(s: String) -> &'static str {
    Box::leak(s.into_boxed_str())
}
```

```rust
// Cuidado: no abusar de Box::leak.
// BIEN: leak una vez al inicio del programa (configuracion, logger).
// MAL:  leak en un loop o en cada request — consume memoria sin limite.
```

## 'static en tipos de error

```rust
// Box<dyn Error> es en realidad Box<dyn Error + 'static>.
// El bound 'static esta implicito — el error no puede contener
// referencias temporales.

use std::error::Error;

fn do_something() -> Result<(), Box<dyn Error>> {
    Ok(())
}
```

```rust
// La mayoria de tipos de error son owned → satisfacen 'static:
#[derive(Debug)]
struct MyError {
    message: String, // owned
    code: u32,       // owned
}

impl std::fmt::Display for MyError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Error {}: {}", self.code, self.message)
    }
}
impl std::error::Error for MyError {}
// MyError satisface 'static → se puede usar como Box<dyn Error>.
```

```rust
// Error con datos prestados — NO satisface 'static:
#[derive(Debug)]
struct BorrowedError<'a> {
    context: &'a str, // referencia no-'static
}
// BorrowedError<'a> NO se puede convertir en Box<dyn Error>.
// Solucion: usar String en vez de &str en tipos de error.

// anyhow::Error requiere Error + Send + Sync + 'static.
// Los errores de la libreria estandar (io::Error, ParseIntError)
// todos satisfacen 'static porque son owned.
```

## 'static vs 'a — cuando usar cada uno

```rust
// 'a: la funcion opera sobre datos prestados y retorna algo
// que depende de esos datos.
fn first_word(s: &str) -> &str {
    s.split_whitespace().next().unwrap_or("")
}

// 'static: los datos son literales o globales.
fn error_message(code: u32) -> &'static str {
    match code {
        1 => "file not found",
        2 => "permission denied",
        _ => "unknown error",
    }
}
```

```rust
// NO convertir 'a a 'static a menos que sea realmente necesario.
// La unica forma segura es copiar los datos:
fn to_static(s: &str) -> &'static str {
    Box::leak(s.to_string().into_boxed_str())
    // Copia al heap y leakea. Solo si realmente necesitas 'static.
}
```

```rust
// Resumen:
//
// Situacion                              | Usar
// -------------------------------------- | --------
// Funcion que retorna substring           | 'a
// Funcion que retorna string literal      | 'static
// Struct que almacena referencia temporal | 'a
// Struct que solo tiene campos owned      | (satisface 'static implicitamente)
// Bound para thread::spawn / async task  | T: 'static
// Tipo de error                           | 'static (owned)
```

## Patrones comunes con 'static

### const y static

```rust
// const: valor conocido en compilacion, inlineado en cada uso.
const MAX_CONNECTIONS: usize = 100;
const API_VERSION: &str = "v2";
const SUPPORTED_FORMATS: &[&str] = &["json", "xml", "csv"];

fn get_version() -> &'static str { API_VERSION }

// static: valor conocido en compilacion, direccion fija, unica copia.
static INSTANCE_ID: u64 = 42;

fn get_id_ref() -> &'static u64 { &INSTANCE_ID }
```

### OnceLock y LazyLock para inicializacion lazy

```rust
// OnceLock (estable desde Rust 1.70) — inicializa una sola vez, thread-safe.
use std::sync::OnceLock;

static CONFIG: OnceLock<String> = OnceLock::new();

fn get_config() -> &'static String {
    CONFIG.get_or_init(|| String::from("loaded from file"))
}
```

```rust
// LazyLock (estable desde Rust 1.80) — similar pero con Deref implicito.
use std::sync::LazyLock;
use std::collections::HashMap;

static MIME_TYPES: LazyLock<HashMap<&'static str, &'static str>> = LazyLock::new(|| {
    let mut m = HashMap::new();
    m.insert("html", "text/html");
    m.insert("css",  "text/css");
    m.insert("js",   "application/javascript");
    m.insert("json", "application/json");
    m
});

fn get_mime(ext: &str) -> &'static str {
    MIME_TYPES.get(ext).copied().unwrap_or("application/octet-stream")
}
```

### Comparacion

```rust
//                  | Tiempo de init | Thread-safe | Direccion fija
// const            | Compilacion    | N/A         | No (inlined)
// static           | Compilacion    | N/A         | Si
// OnceLock         | Runtime (lazy) | Si          | Si
// LazyLock         | Runtime (lazy) | Si          | Si
//
// Todos producen datos con lifetime 'static.
```

## Resumen de reglas

```rust
// 1. &'static T — los datos viven durante toda la ejecucion.
//    Fuentes: string literals, static items, Box::leak.

// 2. T: 'static — T no contiene referencias no-'static.
//    Todos los tipos owned (String, Vec, i32) lo satisfacen.

// 3. T: 'static NO significa "T vive para siempre".
//    Un String satisface 'static pero se libera al salir de scope.

// 4. thread::spawn requiere F: 'static — mover datos owned al closure.

// 5. Box<dyn Error> implica 'static — los errores deben ser owned.

// 6. const/static para datos en compilacion; OnceLock/LazyLock para lazy.
```

---

## Ejercicios

### Ejercicio 1 — Identificar lifetime 'static

```rust
// Para cada expresion, determinar si el tipo resultante tiene
// lifetime 'static. Justificar cada respuesta.
//
// a) let a = "hello";
// b) let s = String::from("hello"); let b = s.as_str();
// c) static X: i32 = 10; let c = &X;
// d) let v = vec![1, 2, 3]; let d = &v[..];
// e) const Y: &str = "world"; let e = Y;
//
// Verificar compilando cada caso con:
// fn needs_static_ref(s: &'static str) {}
// fn needs_static_i32(n: &'static i32) {}
```

### Ejercicio 2 — T: 'static con tipos owned

```rust
// Implementar una funcion store_value que:
//   - Acepte un valor generico T: Debug + 'static
//   - Lo imprima con {:?}
//   - Lo almacene en un Box y retorne Box<dyn Debug>
//
// Llamarla con i32, String, Vec<f64>, (u32, bool).
// Luego intentar llamarla con &local_string donde local_string
// es un String local. Observar y explicar el error.
//
// Firma:
// fn store_value<T: std::fmt::Debug + 'static>(val: T) -> Box<dyn std::fmt::Debug>
```

### Ejercicio 3 — thread::spawn y 'static

```rust
// Escribir un programa que:
//   1. Cree un Vec<String> con 5 elementos.
//   2. Spawne 5 threads, cada uno recibe UNO de los strings (no referencia).
//   3. Cada thread imprime su string.
//   4. El main espera a todos los threads (join).
//
// Primero intentar pasar &strings[i] al thread y observar el error.
// Luego corregir usando into_iter() para mover cada String.
// Variante: usar Arc<Vec<String>> para compartir en solo lectura.
```

### Ejercicio 4 — OnceLock y datos globales

```rust
// Crear un modulo de configuracion con:
//   - struct Config { host: String, port: u16, debug: bool }
//   - static CONFIG: OnceLock<Config>
//   - fn init_config(host: &str, port: u16, debug: bool)
//   - fn get_config() -> &'static Config
//
// En main:
//   1. Llamar init_config("localhost", 8080, true)
//   2. Desde dos threads, llamar get_config() e imprimir los valores.
//   3. Verificar que ambos threads ven la misma configuracion.
```
