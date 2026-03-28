# T04 — OnceCell y LazyCell

## El problema: inicializacion perezosa

A veces necesitas un valor que se calcula una sola vez,
pero no en el momento de construccion sino la primera vez
que se usa. Esto se llama **lazy initialization**:

```rust
// Sin lazy init — se calcula siempre, aunque no se use:
struct App {
    config: Config,  // costoso de crear
}

// Con lazy init — se calcula solo cuando se necesita:
struct App2 {
    config: OnceCell<Config>,  // vacio hasta el primer uso
}
```

```text
Casos de uso tipicos:

  - Configuracion leida de archivo (solo si se necesita)
  - Conexion a base de datos (solo al primer query)
  - Regex compilada (costosa, solo una vez)
  - Valores globales que dependen de datos en runtime
  - Caches que se llenan bajo demanda
```

---

## OnceCell — escribir una sola vez

`OnceCell<T>` es una celda que empieza vacia y se puede
llenar **exactamente una vez**. Despues de eso es immutable:

```rust
use std::cell::OnceCell;

let cell: OnceCell<String> = OnceCell::new();

// La celda empieza vacia:
assert!(cell.get().is_none());

// Llenar por primera vez:
cell.set("hello".to_string()).unwrap();

// Ahora tiene un valor:
assert_eq!(cell.get(), Some(&"hello".to_string()));

// Intentar llenar de nuevo → error (ya esta llena):
let result = cell.set("world".to_string());
assert!(result.is_err());  // retorna Err("world") — el valor no usado

// El valor original no cambio:
assert_eq!(cell.get().unwrap(), "hello");
```

### get_or_init — el metodo mas usado

```rust
use std::cell::OnceCell;

let cell: OnceCell<String> = OnceCell::new();

// get_or_init: retorna &T si ya existe, o ejecuta la closure para inicializar:
let val = cell.get_or_init(|| {
    println!("initializing...");
    "computed value".to_string()
});
// Imprime "initializing..." y retorna &"computed value"
assert_eq!(val, "computed value");

// Segunda llamada — no ejecuta la closure:
let val = cell.get_or_init(|| {
    println!("this never runs");
    "ignored".to_string()
});
// No imprime nada — retorna &"computed value" directamente
assert_eq!(val, "computed value");
```

```rust
// Ejemplo practico — regex compilada una sola vez:
use std::cell::OnceCell;

struct Validator {
    email_pattern: OnceCell<regex::Regex>,
}

// (Ejemplo conceptual — regex es un crate externo)
// impl Validator {
//     fn validate_email(&self, input: &str) -> bool {
//         let re = self.email_pattern.get_or_init(|| {
//             regex::Regex::new(r"^[\w.]+@[\w.]+\.\w+$").unwrap()
//         });
//         re.is_match(input)
//     }
// }
```

---

## LazyCell — OnceCell con closure integrada

`LazyCell<T>` combina OnceCell con la closure de inicializacion.
No necesitas llamar `get_or_init` — simplemente usas el valor
y se inicializa automaticamente la primera vez:

```rust
use std::cell::LazyCell;

let lazy = LazyCell::new(|| {
    println!("computing...");
    42 * 2
});

// No se ha computado todavia.
println!("before access");

// Primera vez que accedes — ejecuta la closure:
println!("value: {}", *lazy);  // "computing..." luego "value: 84"

// Segunda vez — ya esta computado:
println!("value: {}", *lazy);  // "value: 84" (sin "computing...")
```

```text
OnceCell vs LazyCell:

  OnceCell<T>:
  - Separacion entre celda y logica de inicializacion
  - Inicializas con set() o get_or_init()
  - Mas flexible — puedes intentar set() con diferentes valores
  - Puedes inspeccionar si esta vacia (get() → None)

  LazyCell<T, F>:
  - La closure de inicializacion esta dentro del LazyCell
  - Se inicializa automaticamente al primer acceso via Deref
  - Mas ergonomico — simplemente usa *lazy o lazy.method()
  - Menos flexible — la closure se decide en la construccion
```

```rust
// LazyCell en un struct:
use std::cell::LazyCell;

struct Database {
    connection: LazyCell<String>,
}

impl Database {
    fn new(url: &str) -> Self {
        let url = url.to_string();
        Database {
            connection: LazyCell::new(move || {
                println!("connecting to {url}...");
                format!("Connection({})", url)
            }),
        }
    }

    fn query(&self, sql: &str) -> String {
        // La primera llamada conecta, las siguientes reusan:
        format!("{}: {sql}", *self.connection)
    }
}

fn main() {
    let db = Database::new("postgres://localhost/mydb");
    println!("db created, not connected yet");

    println!("{}", db.query("SELECT 1"));
    // "connecting to postgres://localhost/mydb..."
    // "Connection(postgres://localhost/mydb): SELECT 1"

    println!("{}", db.query("SELECT 2"));
    // "Connection(postgres://localhost/mydb): SELECT 2"  (sin "connecting...")
}
```

---

## OnceLock — OnceCell para multi-thread

`std::cell::OnceCell` no es `Sync` — no puede compartirse entre
threads. Para multi-thread existe `std::sync::OnceLock`:

```rust
use std::sync::OnceLock;

// OnceLock es la version thread-safe de OnceCell:
static CONFIG: OnceLock<String> = OnceLock::new();

fn get_config() -> &'static str {
    CONFIG.get_or_init(|| {
        println!("loading config...");
        "production".to_string()
    })
}

fn main() {
    // Multiples threads pueden llamar get_config():
    // Solo uno ejecutara la closure, los demas esperan y obtienen el resultado.
    println!("{}", get_config());  // "loading config..." + "production"
    println!("{}", get_config());  // "production" (sin "loading...")
}
```

```rust
// OnceLock para globales que necesitan datos de runtime:
use std::sync::OnceLock;

static DB_URL: OnceLock<String> = OnceLock::new();

fn init_db(url: &str) {
    DB_URL.set(url.to_string())
        .expect("DB_URL already initialized");
}

fn get_db_url() -> &'static str {
    DB_URL.get().expect("DB_URL not initialized")
}

fn main() {
    // Inicializar una vez al arrancar:
    init_db("postgres://localhost/mydb");

    // Usar desde cualquier parte del programa:
    println!("url: {}", get_db_url());

    // Intentar reinicializar → panic:
    // init_db("other_url");  // panic: DB_URL already initialized
}
```

### OnceLock con threads

```rust
use std::sync::OnceLock;
use std::thread;

static COUNTER: OnceLock<u64> = OnceLock::new();

fn main() {
    let mut handles = vec![];

    for i in 0..5 {
        handles.push(thread::spawn(move || {
            // Solo un thread ejecutara la closure.
            // Los demas obtienen el mismo resultado.
            let val = COUNTER.get_or_init(|| {
                println!("thread {i} initializing");
                42
            });
            println!("thread {i}: {val}");
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
    // Solo una linea "initializing", todas dicen 42.
}
```

---

## LazyLock — LazyCell para multi-thread

`std::sync::LazyLock` es la version thread-safe de `LazyCell`:

```rust
use std::sync::LazyLock;

// Constante global que se computa la primera vez que se accede:
static GREETING: LazyLock<String> = LazyLock::new(|| {
    println!("computing greeting...");
    format!("Hello from thread {:?}", std::thread::current().id())
});

fn main() {
    println!("before access");
    println!("{}", *GREETING);  // computa aqui
    println!("{}", *GREETING);  // reutiliza
}
```

```rust
// LazyLock para patrones que antes requerian lazy_static! o once_cell:
use std::sync::LazyLock;
use std::collections::HashMap;

static EXTENSIONS: LazyLock<HashMap<&str, &str>> = LazyLock::new(|| {
    let mut m = HashMap::new();
    m.insert("rs", "Rust");
    m.insert("py", "Python");
    m.insert("js", "JavaScript");
    m.insert("c", "C");
    m
});

fn language_for(ext: &str) -> Option<&&str> {
    EXTENSIONS.get(ext)
}

fn main() {
    println!("{:?}", language_for("rs"));   // Some("Rust")
    println!("{:?}", language_for("py"));   // Some("Python")
    println!("{:?}", language_for("txt"));  // None
}
```

---

## Tabla completa de tipos

```text
                    Single-thread         Multi-thread
                    ─────────────         ────────────
Escribir una vez:   OnceCell<T>           OnceLock<T>
Lazy con closure:   LazyCell<T>           LazyLock<T>
Mutar Copy:         Cell<T>               Atomic*
Mutar cualquier T:  RefCell<T>            Mutex<T> / RwLock<T>

Todas son interior mutability — permiten mutar a traves de &self.

OnceCell/OnceLock: vacia → llena (una vez) → immutable para siempre
LazyCell/LazyLock: se inicializa automaticamente al primer acceso
Cell/Atomic: get/set, sin referencias al interior
RefCell/Mutex: borrow/lock, presta referencias al interior
```

---

## Antes de la stdlib: once_cell y lazy_static

Antes de Rust 1.70+ (cuando OnceCell/OnceLock se estabilizaron),
el ecosistema usaba crates externos. Los encontraras en codigo existente:

```rust
// ANTES — crate once_cell (el predecesor de la stdlib):
// use once_cell::sync::Lazy;
// static CONFIG: Lazy<String> = Lazy::new(|| "value".to_string());

// AHORA — stdlib:
use std::sync::LazyLock;
static CONFIG: LazyLock<String> = LazyLock::new(|| "value".to_string());

// ANTES — macro lazy_static!:
// lazy_static::lazy_static! {
//     static ref REGEX: Regex = Regex::new(r"\d+").unwrap();
// }

// AHORA — stdlib:
// static REGEX: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"\d+").unwrap());
```

```text
Migracion:

  once_cell::unsync::OnceCell  →  std::cell::OnceCell
  once_cell::unsync::Lazy      →  std::cell::LazyCell
  once_cell::sync::OnceCell    →  std::sync::OnceLock
  once_cell::sync::Lazy        →  std::sync::LazyLock
  lazy_static!                 →  std::sync::LazyLock

  En proyectos nuevos, usa la stdlib. No necesitas crates externos.
```

---

## Errores comunes

```rust
// ERROR 1: usar OnceCell donde const basta
use std::cell::OnceCell;

// MAL — el valor es conocido en compilacion:
let cell = OnceCell::new();
cell.set(42).unwrap();

// BIEN — const o let directo:
const VALUE: i32 = 42;
let value = 42;

// OnceCell es para valores que se calculan en RUNTIME.
```

```rust
// ERROR 2: OnceCell (single-thread) en contexto static
use std::cell::OnceCell;

// NO compila — static requiere Sync:
// static GLOBAL: OnceCell<String> = OnceCell::new();
// error: OnceCell<String> cannot be shared between threads safely

// Solucion: usar OnceLock:
use std::sync::OnceLock;
static GLOBAL: OnceLock<String> = OnceLock::new();
```

```rust
// ERROR 3: set() sin manejar el error
use std::cell::OnceCell;

let cell = OnceCell::new();
cell.set("first".to_string()).unwrap();
// cell.set("second".to_string()).unwrap();  // PANIC: already full

// Solucion: manejar el Err o usar get_or_init:
let val = cell.get_or_init(|| "default".to_string());
// Si ya tiene valor, retorna ese. Si no, ejecuta la closure.
```

```rust
// ERROR 4: esperar que LazyCell se pueda reiniciar
use std::cell::LazyCell;

let lazy = LazyCell::new(|| 42);
println!("{}", *lazy);  // 42

// No puedes "resetear" LazyCell — es write-once.
// Si necesitas un valor que cambie, usa Cell o RefCell.
```

```rust
// ERROR 5: closure costosa que podria fallar
use std::sync::OnceLock;

static CONFIG: OnceLock<String> = OnceLock::new();

// MAL — si la closure hace panic, el OnceLock queda vacio
// y el proximo get_or_init ejecuta otra closure:
// let val = CONFIG.get_or_init(|| {
//     std::fs::read_to_string("config.txt").unwrap()  // panic si no existe
// });

// BIEN — manejar el error dentro de la closure:
fn get_config() -> &'static str {
    CONFIG.get_or_init(|| {
        std::fs::read_to_string("config.txt")
            .unwrap_or_else(|_| "default_config".to_string())
    })
}
```

---

## Cheatsheet

```text
Single-thread:
  OnceCell::new()               crear vacia
  cell.set(val)                 llenar (una vez) → Result
  cell.get()                    → Option<&T>
  cell.get_or_init(|| ...)      → &T (inicializa si vacia)

  LazyCell::new(|| ...)         crear con closure
  *lazy / lazy.method()         accede y auto-inicializa via Deref

Multi-thread:
  OnceLock::new()               crear vacia (usable en static)
  lock.set(val)                 llenar (una vez, thread-safe)
  lock.get()                    → Option<&T>
  lock.get_or_init(|| ...)      → &T (solo un thread ejecuta la closure)

  LazyLock::new(|| ...)         crear con closure (usable en static)
  *lazy / lazy.method()         accede y auto-inicializa via Deref

Para globales (static):
  static X: OnceLock<T>  = OnceLock::new();
  static X: LazyLock<T>  = LazyLock::new(|| ...);

Migracion:
  lazy_static! / once_cell  →  std::sync::LazyLock / OnceLock

Arbol de decision:
  ¿Valor conocido en compilacion?         → const
  ¿Inicializar una vez, single-thread?    → OnceCell / LazyCell
  ¿Inicializar una vez, multi-thread?     → OnceLock / LazyLock
  ¿Closure conocida al construir?         → LazyCell / LazyLock
  ¿Inicializacion flexible (set)?         → OnceCell / OnceLock
  ¿Necesitas mutar despues?               → NO usar Once/Lazy → Cell / RefCell
```

---

## Ejercicios

### Ejercicio 1 — OnceCell basico

```rust
use std::cell::OnceCell;

// Implementa un struct ExpensiveConfig que:
//
// a) Tiene un campo settings: OnceCell<HashMap<String, String>>
//
// b) fn load(&self) → &HashMap<String, String>
//    Usa get_or_init para "cargar" la configuracion
//    (simula con un HashMap hardcoded).
//    Imprime "loading config..." solo la primera vez.
//
// c) Verifica:
//    let cfg = ExpensiveConfig::new();
//    println!("{:?}", cfg.load());  // "loading config..." + HashMap
//    println!("{:?}", cfg.load());  // solo HashMap (sin "loading...")
//
// d) ¿Que pasa si intentas hacer cfg.settings.set(...) despues
//    de que load() ya inicializo la celda?
```

### Ejercicio 2 — LazyLock global

```rust
use std::sync::LazyLock;
use std::collections::HashMap;

// a) Crea un static MORSE_CODE: LazyLock<HashMap<char, &str>>
//    que mapee letras a codigo Morse (al menos A-F).
//
// b) Implementa fn to_morse(text: &str) → String
//    que convierta un string a Morse separado por espacios.
//    Caracteres no encontrados se representan como "?".
//
// c) Verifica:
//    assert_eq!(to_morse("ABC"), ".- -... -.-.");
//
// d) ¿En que momento se inicializa MORSE_CODE?
//    ¿Que pasa si to_morse nunca se llama?
```

### Ejercicio 3 — Elegir el tipo correcto

```rust
// Para cada escenario, decide que tipo usar y explica por que:
//
// a) Un campo `id` que se genera una vez al primer acceso
//    y luego es immutable. Single-thread.
//
// b) Un static global con la version del programa,
//    calculada en runtime desde env!("CARGO_PKG_VERSION").
//
// c) Un campo `cache: ???` que almacena resultados de queries
//    y crece con el tiempo. Accesible via &self.
//
// d) Un campo `initialized: ???<bool>` que registra si
//    el struct ya fue inicializado. Accesible via &self.
//
// e) Un static global con un Vec<String> construido al
//    inicio del programa, accesible desde multiples threads.
//
// Opciones: const, Cell, RefCell, OnceCell, LazyCell,
//           OnceLock, LazyLock, Mutex
```
