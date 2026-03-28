# T01 — panic! vs Result

## Dos filosofias de error

Rust tiene exactamente dos mecanismos para manejar errores:

```text
Error irrecuperable → panic!  → el hilo muere (o todo el proceso)
Error recuperable   → Result  → el llamador decide que hacer
```

```rust
// panic! — "esto no deberia pasar, no puedo continuar"
fn get_config_dir() -> PathBuf {
    // Si no existe directorio home, no tiene sentido seguir
    dirs::home_dir().expect("el sistema debe tener un directorio home")
}

// Result — "esto puede fallar, el llamador decide"
fn read_config(path: &Path) -> Result<String, io::Error> {
    // El archivo puede no existir, y eso es manejable
    fs::read_to_string(path)
}
```

La regla general: si el error representa un **bug** en el programa,
`panic!`. Si el error representa una **condicion esperada** del entorno
(archivo no encontrado, red caida, input invalido), `Result`.

## Que hace panic!

`panic!` detiene la ejecucion normal del hilo actual. No es un
`return`, no es una excepcion que se pueda atrapar facilmente — es
una señal de que algo salio **fundamentalmente mal**:

```rust
fn main() {
    println!("antes del panic");
    panic!("algo terrible paso");
    println!("esto nunca se ejecuta");  // dead code
}
```

```text
antes del panic
thread 'main' panicked at 'algo terrible paso', src/main.rs:3:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
```

```rust
// panic! acepta format strings igual que println!:
let x = 42;
panic!("valor inesperado: {x}, esperaba < 10");

// El mensaje se muestra en stderr, junto con:
//   - archivo y linea donde ocurrio
//   - backtrace (si RUST_BACKTRACE=1)
```

```bash
# Para ver el backtrace completo:
RUST_BACKTRACE=1 cargo run

# Backtrace completo (incluye frames internos de la stdlib):
RUST_BACKTRACE=full cargo run
```

## Funciones y macros que hacen panic

No solo `panic!` causa un panic. Muchas operaciones comunes
tambien lo hacen cuando fallan:

```rust
// --- unwrap() y expect() ---
let val: Option<i32> = None;
val.unwrap();           // panic: "called `Option::unwrap()` on a `None` value"
val.expect("sin valor"); // panic: "sin valor"

let res: Result<i32, &str> = Err("fallo");
res.unwrap();           // panic: "called `Result::unwrap()` on an `Err` value: fallo"
res.expect("operacion"); // panic: "operacion: fallo"

// --- Acceso a indices fuera de rango ---
let v = vec![1, 2, 3];
let x = v[10];         // panic: "index out of bounds: len is 3 but the index is 10"

// --- Aritmetica en modo debug ---
let x: u8 = 255;
let y = x + 1;         // panic en debug: "attempt to add with overflow"
                        // en release: wraps a 0 (no panic)

// --- Conversion de tipos que falla ---
let big: i32 = 1_000_000;
let small: u8 = big.try_into().unwrap();  // panic: valor no cabe en u8

// --- slice::split_at con indice invalido ---
let s = "hola";
let (a, b) = s.split_at(100);  // panic: "byte index 100 is out of bounds"
```

```rust
// --- Macros de marcador ---

// todo!() — "aun no implementado, pero planeo hacerlo"
fn calculate_tax(amount: f64) -> f64 {
    todo!("implementar calculo de impuestos")
}

// unimplemented!() — "esta rama no deberia ejecutarse"
fn process(kind: &str) -> String {
    match kind {
        "json" => parse_json(),
        "xml"  => parse_xml(),
        _      => unimplemented!("formato no soportado: {kind}"),
    }
}

// unreachable!() — "el compilador no puede probar que es imposible,
//                   pero yo se que nunca se llega aqui"
fn classify(n: u32) -> &'static str {
    match n % 3 {
        0 => "divisible",
        1 => "resto 1",
        2 => "resto 2",
        _ => unreachable!("n % 3 solo puede ser 0, 1 o 2"),
    }
}
```

```text
Resumen de que causa panic:

Operacion                    Cuando hace panic
─────────────────────────────────────────────────
panic!("msg")                Siempre
unwrap() / expect()          Si Option es None o Result es Err
vec[indice]                  Si indice >= len
slice.split_at(n)            Si n > len
x + y (enteros, debug)       Si hay overflow
todo!()                      Siempre
unimplemented!()             Siempre
unreachable!()               Siempre
assert!(cond)                Si cond es false
assert_eq!(a, b)             Si a != b
assert_ne!(a, b)             Si a == b
```

## Unwinding vs abort

Cuando ocurre un panic, Rust tiene dos estrategias para terminar:

```text
Unwinding (por defecto)
───────────────────────
1. Detiene la ejecucion normal del hilo
2. Recorre el stack frame por frame (de adentro hacia afuera)
3. En cada frame ejecuta los destructores (Drop) de las variables locales
4. Libera memoria, cierra archivos, flush de buffers
5. El hilo termina (o el proceso si es el hilo main)

Abort
─────
1. Detiene la ejecucion inmediatamente
2. El SO recupera TODA la memoria del proceso
3. No se ejecutan destructores
4. Equivalente a un kill -9 del proceso
```

```rust
// Con unwinding, los destructores SI se ejecutan:
struct Loud(i32);

impl Drop for Loud {
    fn drop(&mut self) {
        println!("destruyendo Loud({})", self.0);
    }
}

fn main() {
    let _a = Loud(1);
    let _b = Loud(2);
    let _c = Loud(3);
    panic!("boom");
    // Salida:
    //   destruyendo Loud(3)  ← ultimo creado, primero destruido
    //   destruyendo Loud(2)
    //   destruyendo Loud(1)
    //   thread 'main' panicked at 'boom', src/main.rs:13:5
}
```

```toml
# Cargo.toml — configurar la estrategia de panic

# Para modo debug (cargo build):
[profile.dev]
panic = "unwind"    # por defecto — ejecuta destructores

# Para modo release (cargo build --release):
[profile.release]
panic = "abort"     # termina inmediato — binario mas pequeño
```

```text
Comparacion:

                    Unwinding              Abort
────────────────────────────────────────────────────
Destructores        Si se ejecutan         No
Tamaño binario      Mayor (tablas de       Menor (no necesita
                    unwinding ~10-20%)     tablas de unwinding)
Velocidad           Overhead en cada       Sin overhead en
                    frame (landing pads)   ejecucion normal
Limpieza            Archivos cerrados,     El SO libera memoria,
                    buffers flushed        pero no flush/cleanup
catch_unwind        Funciona               No funciona (el
                                           proceso muere)
Uso tipico          Desarrollo, servers    Embedded, CLI tools,
                    (aislar panics)        WASM, tamaño critico
```

### Por que importa abort para el tamaño del binario

```bash
# Comparar tamaños de binario:

# Con unwind (por defecto):
cargo build --release
ls -lh target/release/myapp
# -rwxr-xr-x 1 user user 3.2M myapp

# Agregar panic = "abort" en [profile.release]:
cargo build --release
ls -lh target/release/myapp
# -rwxr-xr-x 1 user user 2.6M myapp  (~20% menor)
```

```text
El overhead de unwinding viene de:
1. Tablas de unwinding (.eh_frame section en ELF)
   — mapean cada instruccion a como restaurar el frame anterior
2. Landing pads — codigo generado para cada funcion que tiene
   variables con Drop
3. Codigo de la libreria de unwinding (libunwind o equivalente)

Con abort, nada de esto se genera.
```

### Doble panic

Si un destructor ejecuta `panic!` durante el unwinding de otro
panic, Rust aborta el proceso inmediatamente — no intenta hacer
unwind del unwind:

```rust
struct BadDrop;

impl Drop for BadDrop {
    fn drop(&mut self) {
        panic!("panic en destructor!");  // segundo panic
    }
}

fn main() {
    let _x = BadDrop;
    panic!("primer panic");
    // Resultado: abort inmediato
    // "thread panicked while panicking. aborting."
}
```

```text
Regla: NUNCA hagas panic dentro de un destructor (impl Drop).
Si necesitas manejar errores en Drop, usa:
  - eprintln! para loguear el error
  - ignorar el error silenciosamente
  - almacenar el error en algun lado para consultarlo despues
```

## Que hace Result

`Result<T, E>` es un enum con dos variantes que obliga al
llamador a manejar la posibilidad de error:

```rust
// Definicion en la stdlib (simplificada):
enum Result<T, E> {
    Ok(T),    // operacion exitosa, contiene el valor
    Err(E),   // operacion fallida, contiene el error
}
```

```rust
use std::fs;
use std::num::ParseIntError;

// Funcion que puede fallar — retorna Result
fn parse_port(s: &str) -> Result<u16, ParseIntError> {
    s.parse::<u16>()
}

fn main() {
    // El llamador DEBE manejar ambos casos
    match parse_port("8080") {
        Ok(port) => println!("puerto: {port}"),
        Err(e)   => eprintln!("error al parsear: {e}"),
    }

    // Ignorar un Result genera un warning del compilador:
    parse_port("abc");
    // warning: unused `Result` that must be used
    //   = note: this `Result` may be an `Err` variant, which should be handled
}
```

```rust
// Result obliga a decidir — no puedes "olvidarte" del error
fn read_file_size(path: &str) -> Result<u64, std::io::Error> {
    let metadata = fs::metadata(path)?;  // propaga el error con ?
    Ok(metadata.len())
}

fn main() {
    match read_file_size("/etc/hosts") {
        Ok(size) => println!("tamaño: {size} bytes"),
        Err(e)   => eprintln!("no se pudo leer: {e}"),
    }

    match read_file_size("/no/existe") {
        Ok(size) => println!("tamaño: {size} bytes"),
        Err(e)   => eprintln!("no se pudo leer: {e}"),
        // no se pudo leer: No such file or directory (os error 2)
    }
}
```

## Cuando usar panic!

`panic!` es apropiado cuando el error indica un **bug** — una
situacion que no deberia ocurrir si el codigo es correcto:

```rust
// 1. Violacion de invariantes internas
fn set_percentage(value: u8) {
    // Si el llamador pasa > 100, es un bug en SU codigo
    assert!(value <= 100, "porcentaje fuera de rango: {value}");
    // ...
}

// 2. Estado corrupto irrecuperable
fn process_buffer(buf: &[u8]) {
    // El protocolo garantiza que el header es 4 bytes
    // Si no lo es, algo salio muy mal
    assert!(buf.len() >= 4, "buffer corrupto: len = {}", buf.len());
    let header = &buf[..4];
    // ...
}

// 3. Prototipos y desarrollo rapido
fn calculate_shipping(weight: f64) -> f64 {
    todo!("implementar cuando tengamos las tarifas")
}

// 4. Ejemplos y tests
#[test]
fn test_addition() {
    assert_eq!(2 + 2, 4);  // panic si falla = test falla
}

// 5. Despues de validar con Result y llegar a un caso "imposible"
fn get_first_word(text: &str) -> &str {
    // split siempre retorna al menos un elemento
    text.split_whitespace().next()
        .expect("split_whitespace siempre tiene al menos un elemento... ¿o no?")
        // Nota: esto SI puede ser None si text es "" o solo whitespace
        // Mejor usar Result o devolver Option aqui
}
```

```text
Resumen — usa panic! cuando:

✓ El error indica un BUG (no una condicion del entorno)
✓ Estás en un prototipo/ejemplo y quieres avanzar rapido
✓ Un invariante interno se rompio y no hay forma sensata de continuar
✓ Estás en un test (assert!, assert_eq!)
✓ La documentacion del tipo/funcion GARANTIZA que el valor es valido
```

## Cuando usar Result

`Result` es apropiado cuando el error es una **posibilidad esperada**
que el llamador puede manejar:

```rust
use std::fs;
use std::io;
use std::net::TcpStream;

// 1. I/O — archivos, red, stdin/stdout
fn read_config(path: &str) -> Result<String, io::Error> {
    fs::read_to_string(path)
}

// 2. Parsing de datos externos
fn parse_age(input: &str) -> Result<u8, String> {
    let n: u8 = input.parse()
        .map_err(|_| format!("'{input}' no es un numero valido"))?;
    if n > 150 {
        return Err(format!("edad {n} no es realista"));
    }
    Ok(n)
}

// 3. Conexiones de red
fn connect_to_server(addr: &str) -> Result<TcpStream, io::Error> {
    TcpStream::connect(addr)
}

// 4. Busqueda que puede no encontrar
fn find_user(id: u64) -> Result<User, DatabaseError> {
    db.query("SELECT * FROM users WHERE id = ?", &[id])
}

// 5. Validacion de entrada del usuario
fn validate_email(email: &str) -> Result<(), ValidationError> {
    if !email.contains('@') {
        return Err(ValidationError::MissingAt);
    }
    if email.len() > 254 {
        return Err(ValidationError::TooLong);
    }
    Ok(())
}
```

```text
Resumen — usa Result cuando:

✓ La operacion depende del entorno externo (disco, red, usuario)
✓ El error es una posibilidad esperada y documentada
✓ El llamador puede hacer algo util con el error
✓ Estás escribiendo una libreria (las librerias casi nunca deben panic)
✓ Quieres dar al llamador la opcion de reintentar, loguear, o propagar
```

## La regla para librerias vs binarios

```text
Libreria (lib.rs, crate publicado)
──────────────────────────────────
→ Casi SIEMPRE Result
→ panic! solo en casos de bug interno (assert! en invariantes)
→ NUNCA panic! por input del usuario de la libreria
→ El usuario del crate decide como manejar el error

Binario (main.rs, aplicacion final)
────────────────────────────────────
→ Result en la logica de negocio
→ unwrap()/expect() en el punto de entrada cuando no hay
  forma razonable de recuperarse
→ panic! para invariantes internas
```

```rust
// MALO en una libreria — le quitas control al usuario:
pub fn parse_config(path: &str) -> Config {
    let content = fs::read_to_string(path)
        .expect("no se pudo leer el archivo");  // panic!
    toml::from_str(&content)
        .expect("TOML invalido");               // panic!
}

// BUENO en una libreria — el usuario decide:
pub fn parse_config(path: &str) -> Result<Config, ConfigError> {
    let content = fs::read_to_string(path)
        .map_err(ConfigError::Io)?;
    toml::from_str(&content)
        .map_err(ConfigError::Parse)
}

// En el binario, el usuario puede elegir:
fn main() {
    // Opcion 1: terminar con mensaje claro
    let config = parse_config("config.toml")
        .unwrap_or_else(|e| {
            eprintln!("Error de configuracion: {e}");
            std::process::exit(1);
        });

    // Opcion 2: usar valores por defecto
    let config = parse_config("config.toml")
        .unwrap_or_default();

    // Opcion 3: propagar al main que retorna Result
    let config = parse_config("config.toml")?;
}
```

## El espectro completo de manejo de errores

No es solo "panic o Result" — hay un espectro de severidad:

```rust
// Nivel 0: Ignorar (generalmente incorrecto)
let _ = fs::remove_file("temp.txt");  // descarta el error explicitamente

// Nivel 1: Log y continuar
if let Err(e) = fs::remove_file("temp.txt") {
    eprintln!("advertencia: no se pudo eliminar temp: {e}");
    // continua la ejecucion — no es critico
}

// Nivel 2: Retornar Result (propagar al llamador)
fn cleanup() -> Result<(), io::Error> {
    fs::remove_file("temp.txt")?;  // el llamador decide
    Ok(())
}

// Nivel 3: Terminar con mensaje (exit, no panic)
fn main() {
    if let Err(e) = run() {
        eprintln!("error fatal: {e}");
        std::process::exit(1);  // limpio, sin backtrace
    }
}

// Nivel 4: panic! (para bugs)
fn internal_function(data: &[u8]) {
    assert!(!data.is_empty(), "bug: se llamo con datos vacios");
}
```

```text
                    Ignorar → Log → Result → exit → panic!
                    ←─────── recuperable ──────────→ irrecuperable
                    ←── menos informacion ────────→ mas informacion
```

## catch_unwind — atrapar panics

En algunos casos necesitas atrapar un panic sin que mate al hilo.
`std::panic::catch_unwind` permite esto:

```rust
use std::panic;

fn main() {
    // catch_unwind envuelve el panic y retorna un Result
    let result = panic::catch_unwind(|| {
        println!("antes del panic");
        panic!("oh no!");
    });

    // El panic fue atrapado — el programa continua
    match result {
        Ok(val) => println!("exito: {val:?}"),
        Err(_)  => println!("se atrapo un panic"),
    }

    println!("el programa continua normalmente");
}
```

```text
Salida:
antes del panic
thread 'main' panicked at 'oh no!', src/main.rs:6:9
se atrapo un panic
el programa continua normalmente
```

```rust
// Caso de uso real: servidor web que no debe morir por un request
fn handle_request(req: Request) -> Response {
    let result = panic::catch_unwind(panic::AssertUnwindSafe(|| {
        process_request(req)  // si esto hace panic...
    }));

    match result {
        Ok(response) => response,
        Err(_) => {
            eprintln!("panic al procesar request — retornando 500");
            Response::internal_server_error()
        }
    }
}
```

```text
Advertencias sobre catch_unwind:

1. Solo funciona con panic = "unwind" (no con "abort")
2. NO es un mecanismo de manejo de errores — es un safety net
3. El estado puede quedar inconsistente despues de un panic atrapado
4. AssertUnwindSafe es necesario para closures que capturan &mut
5. Usalo en boundaries de aislamiento (threads, FFI, plugins),
   NO como try/catch general
```

```rust
// catch_unwind NO funciona con abort:
// Si en Cargo.toml tienes panic = "abort", catch_unwind
// simplemente nunca atrapa nada — el proceso muere.

// catch_unwind NO reemplaza a Result:
// MALO — usar panic como control de flujo:
fn divide(a: i32, b: i32) -> i32 {
    panic::catch_unwind(|| a / b).unwrap_or(0)  // NO hagas esto
}

// BUENO — usar Result:
fn divide(a: i32, b: i32) -> Result<i32, &'static str> {
    if b == 0 {
        Err("division por cero")
    } else {
        Ok(a / b)
    }
}
```

## process::exit vs panic!

```rust
use std::process;

fn main() {
    // process::exit — terminacion limpia con codigo de salida
    // NO ejecuta destructores del stack actual
    // SI flush stdout (en la mayoria de implementaciones)
    if let Err(e) = run() {
        eprintln!("error: {e}");
        process::exit(1);  // codigo de salida 1 = error
    }
    // Codigo de salida 0 = exito (implicito si main termina normal)
}
```

```text
Comparacion:

                   panic!            process::exit(n)
──────────────────────────────────────────────────────
Destructores       Si (unwinding)    No
Codigo salida      101 (por defecto) Lo que pases como n
Backtrace          Si                No
Mensaje            En stderr         Tu decides
Uso                Bugs internos     Errores de usuario,
                                     terminacion controlada
```

```rust
// Patron idomiatico en Rust para binarios:
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let config = load_config()?;
    let result = run(config)?;
    println!("{result}");
    Ok(())
}
// Si retorna Err, Rust imprime el error y sale con codigo 1.
// Si retorna Ok, sale con codigo 0.
// No necesitas process::exit en la mayoria de casos.
```

## Arbol de decision

```text
¿El error indica un BUG en el codigo?
├── SI → panic! / assert! / unreachable!
│       Ejemplos:
│       - indice fuera de rango por logica incorrecta
│       - invariante rota (ej: vector que deberia tener elementos)
│       - estado "imposible" alcanzado
│
└── NO → ¿Puede el llamador hacer algo util con el error?
         ├── SI → Result<T, E>
         │       Ejemplos:
         │       - archivo no encontrado → pedir otro path
         │       - conexion rechazada → reintentar
         │       - input invalido → pedir de nuevo
         │
         └── NO → ¿Es una libreria o un binario?
                  ├── Libreria → Result de todas formas
                  │   (deja la decision al usuario)
                  │
                  └── Binario → process::exit con mensaje
                      o main() -> Result
```

## Ejemplo integrador

```rust
use std::collections::HashMap;
use std::fs;
use std::io;
use std::num::ParseIntError;

/// Error custom para nuestro parser de configuracion.
#[derive(Debug)]
enum ConfigError {
    Io(io::Error),
    Parse { line: usize, msg: String },
    Missing(String),
}

impl std::fmt::Display for ConfigError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigError::Io(e) => write!(f, "error de I/O: {e}"),
            ConfigError::Parse { line, msg } => {
                write!(f, "error en linea {line}: {msg}")
            }
            ConfigError::Missing(key) => {
                write!(f, "clave requerida '{key}' no encontrada")
            }
        }
    }
}

impl From<io::Error> for ConfigError {
    fn from(e: io::Error) -> Self {
        ConfigError::Io(e)
    }
}

/// Lee un archivo de configuracion con formato "clave=valor".
/// Retorna Result — el archivo puede no existir o tener formato malo.
fn load_config(path: &str) -> Result<HashMap<String, String>, ConfigError> {
    let content = fs::read_to_string(path)?;  // io::Error → ConfigError::Io

    let mut map = HashMap::new();
    for (i, line) in content.lines().enumerate() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;  // lineas vacias y comentarios
        }
        let (key, value) = line.split_once('=').ok_or_else(|| {
            ConfigError::Parse {
                line: i + 1,
                msg: format!("falta '=' en: {line}"),
            }
        })?;
        map.insert(key.trim().to_string(), value.trim().to_string());
    }
    Ok(map)
}

/// Extrae un valor numerico del config.
/// Retorna Result — la clave puede faltar o no ser un numero.
fn get_port(config: &HashMap<String, String>) -> Result<u16, ConfigError> {
    let port_str = config.get("port").ok_or_else(|| {
        ConfigError::Missing("port".to_string())
    })?;
    port_str.parse::<u16>().map_err(|e| ConfigError::Parse {
        line: 0,
        msg: format!("'port' no es un numero valido: {e}"),
    })
}

/// Funcion interna — si se llama con un map vacio es un BUG.
fn validate_config(config: &HashMap<String, String>) {
    // assert! para invariantes internas — esto es un panic
    assert!(
        !config.is_empty(),
        "BUG: validate_config llamado con config vacio"
    );

    // Verificar que "host" existe — esto SI es un error recuperable
    // pero aqui elegimos panic porque es un requisito interno
    // que ya debio validarse antes de llamar a esta funcion
}

fn run() -> Result<(), ConfigError> {
    let config = load_config("server.conf")?;

    // validate_config usa panic — si config esta vacio, es un bug
    // porque load_config nunca retorna Ok con map vacio si el
    // archivo tiene contenido (y si esta vacio, no tiene port)
    validate_config(&config);

    let port = get_port(&config)?;
    println!("servidor en puerto {port}");
    Ok(())
}

fn main() {
    // El binario decide como manejar los errores
    match run() {
        Ok(()) => {}
        Err(e) => {
            eprintln!("error: {e}");
            std::process::exit(1);
        }
    }
}
```

```text
En este ejemplo:
- load_config → Result (I/O puede fallar)
- get_port → Result (clave puede faltar, valor puede no ser numero)
- validate_config → panic! (invariante interna)
- main → captura Result y hace exit con mensaje
```

## Errores comunes

```rust
// ERROR 1: usar unwrap en codigo de produccion
fn read_data() -> Vec<u8> {
    fs::read("data.bin").unwrap()  // panic si el archivo no existe
}
// Solucion: retornar Result
fn read_data() -> Result<Vec<u8>, io::Error> {
    fs::read("data.bin")
}

// ERROR 2: panic en una libreria por input del usuario
pub fn parse(input: &str) -> Config {
    if input.is_empty() {
        panic!("input vacio");  // el usuario del crate no puede atrapar esto
    }
    // ...
}
// Solucion: retornar Result
pub fn parse(input: &str) -> Result<Config, ParseError> {
    if input.is_empty() {
        return Err(ParseError::EmptyInput);
    }
    // ...
}

// ERROR 3: confundir expect() con manejo de errores
let port: u16 = env::var("PORT")
    .expect("PORT no definido")  // esto sigue siendo un panic
    .parse()
    .expect("PORT no es un numero");  // esto tambien
// expect() NO maneja el error — solo mejora el mensaje del panic.

// ERROR 4: catch_unwind como try/catch
fn safe_divide(a: i32, b: i32) -> Option<i32> {
    panic::catch_unwind(|| a / b).ok()  // anti-patron
}
// Solucion: validar antes
fn safe_divide(a: i32, b: i32) -> Option<i32> {
    if b == 0 { None } else { Some(a / b) }
}

// ERROR 5: silenciar errores con let _ =
let _ = important_operation();  // ¿fallo? nunca lo sabremos
// Solo usa let _ = cuando REALMENTE no te importa el resultado
// y has pensado en las consecuencias.
```

---

## Ejercicios

### Ejercicio 1 — Clasificar errores

```rust
// Para cada situacion, decide: ¿panic! o Result?
// Justifica tu respuesta.

// a) Una funcion que divide dos numeros recibidos del usuario
// b) Un assert que verifica que un vector interno no esta vacio
// c) Una funcion que conecta a una base de datos
// d) Acceder al primer elemento de un Vec que "siempre tiene al menos uno"
// e) Parsear un archivo JSON proporcionado por el usuario
// f) Un match que cubre todos los valores posibles de un enum propio

// Predice para cada uno y luego verifica con las reglas de este tema.
```

### Ejercicio 2 — Refactorizar unwrap

```rust
// Este codigo usa unwrap en todas partes.
// Refactorizalo para usar Result con el operador ?.

use std::fs;
use std::collections::HashMap;

fn load_scores() -> HashMap<String, u32> {
    let content = fs::read_to_string("scores.txt").unwrap();
    let mut scores = HashMap::new();
    for line in content.lines() {
        let parts: Vec<&str> = line.split(':').collect();
        let name = parts[0].trim().to_string();
        let score: u32 = parts[1].trim().parse().unwrap();
        scores.insert(name, score);
    }
    scores
}

fn main() {
    let scores = load_scores();
    for (name, score) in &scores {
        println!("{name}: {score}");
    }
}

// Pistas:
// - Define un tipo de error (o usa Box<dyn std::error::Error>)
// - load_scores debe retornar Result
// - Maneja el caso de que parts no tenga 2 elementos
// - main puede retornar Result o usar match/if let
```

### Ejercicio 3 — Unwinding en accion

```rust
// Crea un programa que demuestre el orden de destruccion con panic.
//
// 1. Define un struct Tracker(String) que implemente Drop,
//    imprimiendo "drop: {nombre}" en el destructor.
// 2. En main, crea tres Trackers: "primero", "segundo", "tercero".
// 3. Haz panic! despues de crearlos.
// 4. Ejecuta y observa el orden de destruccion.
// 5. Cambia panic = "abort" en Cargo.toml y ejecuta de nuevo.
//    ¿Que cambia?
//
// Predice el orden de salida ANTES de ejecutar.
```
