# T03 — Constantes

## Que son las constantes en Rust

Las constantes son valores que se fijan en tiempo de compilacion y nunca
cambian. Rust ofrece dos mecanismos: `const` y `static`. Ambos requieren
anotacion de tipo y un valor evaluable en compilacion, pero difieren en
como el compilador los maneja en memoria.

```rust
// const — el valor se incrusta directamente donde se usa (inlining):
const MAX_CONNECTIONS: u32 = 100;

// static — el valor vive en una direccion de memoria fija:
static APP_NAME: &str = "MyApp";

// Ambos:
//   - Requieren anotacion de tipo explicita
//   - Requieren un valor evaluable en tiempo de compilacion
//   - No pueden usar let
//   - Convencion de nombres: SCREAMING_SNAKE_CASE
```

## const

La palabra clave `const` define un valor que el compilador incrusta
(inline) en cada lugar donde se usa. No ocupa una posicion fija en
memoria:

```rust
// Sintaxis: const NOMBRE: Tipo = valor;
const MAX_SIZE: u32 = 100;
const PI: f64 = 3.14159265358979;
const APP_VERSION: &str = "1.0.0";

// La anotacion de tipo es OBLIGATORIA:
// const MAX_SIZE = 100;  // error: missing type for `const` item

// No puede usar mut ni let:
// const mut Y: u32 = 5;  // error

// El valor DEBE ser evaluable en compilacion:
// const NOW: std::time::Instant = std::time::Instant::now();
//   error: calls in constants are limited to constant functions
```

```rust
// Convencion SCREAMING_SNAKE_CASE — el compilador advierte si no se sigue:
const max_retries: u32 = 3;
// warning: constant `max_retries` should have an upper case name
const MAX_RETRIES: u32 = 3;  // correcto
```

### Alcance y uso de const

Las constantes se pueden declarar en cualquier alcance: global,
dentro de funciones, bloques, impl o traits:

```rust
const GLOBAL_LIMIT: u32 = 1000;

fn main() {
    const LOCAL_LIMIT: u32 = 50;
    println!("Global: {}, Local: {}", GLOBAL_LIMIT, LOCAL_LIMIT);
}

struct Circle { radius: f64 }

impl Circle {
    const DEFAULT_RADIUS: f64 = 1.0;

    fn new() -> Circle {
        Circle { radius: Self::DEFAULT_RADIUS }
    }
}

trait Shape {
    const DIMENSIONS: u32;  // constante asociada al trait
}

impl Shape for Circle {
    const DIMENSIONS: u32 = 2;
}
```

```rust
// Inlining: el compilador sustituye el valor en cada punto de uso.
const FACTOR: u32 = 10;

fn main() {
    let a = FACTOR * 2;   // el compilador ve: let a = 10 * 2;
    let b = FACTOR + 5;   // el compilador ve: let b = 10 + 5;
}

// Dos &FACTOR pueden apuntar a direcciones distintas —
// el compilador crea un temporal anonimo cada vez.
// Si necesitas una direccion fija, usa static.

// Comparacion con C:
//   const en Rust se parece a #define o enum { FACTOR = 10 }
//   NO se parece a const int FACTOR = 10 (en C eso tiene direccion)
```

### Expresiones permitidas en const

```rust
const BASE: u32 = 10;
const SQUARED: u32 = BASE * BASE;               // aritmetica
const IS_LARGE: bool = BASE > 5;                 // comparacion
const MASK: u32 = 0xFF & 0x0F;                   // bitwise
const SIZE: usize = std::mem::size_of::<u64>();   // const fn de stdlib

const PAIR: (u32, u32) = (BASE, SQUARED);         // tuplas
const TABLE: [u32; 3] = [BASE, SQUARED, 1000];    // arrays
const GREETING: &str = "hello";                    // strings
const FIRST: u32 = TABLE[0];                       // indexacion
```

## const fn

Las funciones `const fn` pueden evaluarse en tiempo de compilacion.
Esto permite usarlas para inicializar constantes y statics:

```rust
const fn square(x: u32) -> u32 {
    x * x
}

const fn factorial(n: u32) -> u32 {
    match n {
        0 | 1 => 1,
        _ => n * factorial(n - 1),
    }
}

// Uso en constantes (evaluado en compilacion):
const NINE: u32 = square(3);
const FACT_SIX: u32 = factorial(6);  // 720

fn main() {
    // Tambien se puede llamar en tiempo de ejecucion:
    let runtime_value = square(5);
    println!("{}", runtime_value);  // 25
}
```

```rust
// const fn con loops (disponible desde Rust 1.46):
const fn sum_to(n: u32) -> u32 {
    let mut total = 0;
    let mut i = 1;
    while i <= n {
        total += i;
        i += 1;
    }
    total
}

const SUM_100: u32 = sum_to(100); // 5050
```

### Limitaciones de const fn

```rust
// NO permitido en const fn:
//   1. Allocacion en heap (Box, Vec, String)
//   2. I/O (println!, leer archivos, red)
//   3. Llamar a funciones que NO son const fn
//   4. for loops (requiere Iterator, que no es const — usar while)
//   5. Traits dinamicos (dyn Trait)
//
// SI permitido:
//   1. Aritmetica, comparaciones, operaciones bitwise
//   2. if, else, match
//   3. while, loop
//   4. Llamadas a otras const fn
//   5. Creacion de structs y enums
//   6. Referencias (&, &mut)
//   7. Indexacion de slices y arrays

const fn max(a: u32, b: u32) -> u32 {
    if a > b { a } else { b }
}

const MAX_VAL: u32 = max(42, 17);  // 42

// Las capacidades de const fn crecen con cada version de Rust:
// 1.31: aritmetica basica | 1.46: if, match, loop | 1.79: &mut
```

## static

La palabra clave `static` define un valor con una **direccion de
memoria fija** que existe durante toda la ejecucion del programa
(lifetime `'static`):

```rust
static GREETING: &str = "Hello, world!";
static MAX_THREADS: u32 = 8;

fn main() {
    // static tiene una direccion fija — dos referencias apuntan
    // siempre al mismo lugar:
    let r1 = &MAX_THREADS;
    let r2 = &MAX_THREADS;
    println!("{:p} == {:p}", r1, r2);
    // Siempre la misma direccion (con const NO esta garantizado)
}

// Equivalente en C:
//   static const int MAX_THREADS = 8;
//   // vive en .rodata, tiene direccion fija
```

### Cuando usar static en vez de const

```rust
// 1. Direccion de memoria fija (FFI / codigo C):
static BUFFER_SIZE: usize = 4096;

// 2. Datos grandes que no quieres copiar en cada punto de uso:
static LOOKUP_TABLE: [u32; 256] = {
    let mut table = [0u32; 256];
    let mut i = 0;
    while i < 256 {
        table[i] = (i as u32).wrapping_mul(2654435761);
        i += 1;
    }
    table
};
// Con const, esta tabla de 1 KB se copiaria en cada uso.
// Con static, existe una sola copia.

// 3. Interior mutability (Mutex, Atomic — ver mas adelante).
```

## static mut

Se puede declarar un `static` mutable con `static mut`. Todo
acceso (lectura y escritura) requiere un bloque `unsafe`:

```rust
static mut COUNTER: u32 = 0;

fn increment() {
    unsafe { COUNTER += 1; }
}

fn get_count() -> u32 {
    unsafe { COUNTER }
}

fn main() {
    increment();
    increment();
    println!("Count: {}", get_count()); // Count: 2
}
```

```rust
// POR QUE es unsafe:
// static mut es estado global mutable. Si dos hilos acceden
// simultaneamente, hay una data race (comportamiento indefinido).
// Rust NO proporciona sincronizacion automatica.
//
// Ejemplo de data race (NO hacer esto):
// static mut COUNTER: u32 = 0;
// thread::spawn(|| { unsafe { COUNTER += 1; } }); // data race!
```

### Alternativas seguras a static mut

En la practica, `static mut` casi nunca es la solucion correcta:

```rust
// 1. Atomic types — contadores y flags simples:
use std::sync::atomic::{AtomicU32, Ordering};

static COUNTER: AtomicU32 = AtomicU32::new(0);

fn increment() {
    COUNTER.fetch_add(1, Ordering::SeqCst);  // sin unsafe
}

// 2. Mutex — datos complejos con sincronizacion:
use std::sync::Mutex;

static CONFIG: Mutex<Vec<String>> = Mutex::new(Vec::new());

fn add_config(entry: String) {
    CONFIG.lock().unwrap().push(entry);  // sin unsafe
}

// 3. OnceLock — inicializacion unica (antes OnceCell):
use std::sync::OnceLock;

static DB_URL: OnceLock<String> = OnceLock::new();

fn init_db(url: &str) {
    DB_URL.set(url.to_string()).expect("already set");
}

// Resumen:
//   Contador atomico            → AtomicU32, AtomicBool
//   Datos complejos compartidos → Mutex<T> o RwLock<T>
//   Inicializacion perezosa     → OnceLock<T>
//   static mut                  → Evitar (solo FFI como ultimo recurso)
```

## Comparacion const vs static

```rust
// Caracteristica        | const              | static
// ----------------------|--------------------|---------------------
// Memoria               | Inlined (sin dir.) | Direccion fija
// Copias                | Una por uso         | Una sola instancia
// &referencia           | Temporal anonimo   | Siempre misma dir.
// Lifetime              | No aplica          | 'static
// Mutabilidad           | Nunca              | static mut (unsafe)
// Interior mutability   | No                 | Si (Mutex, Atomic)

// Usar const para:
//   - Valores simples (numeros, &str, bools)
//   - Valores pequenos que no importa copiar
const MAX_RETRIES: u32 = 3;
const VERSION: &str = "2.1.0";

// Usar static para:
//   - Datos grandes (tablas, buffers)
//   - Cuando necesitas direccion de memoria estable
//   - Para FFI o interior mutability
static LOOKUP: [u8; 256] = [0; 256];
```

```rust
// Ejemplo que muestra la diferencia:
const CONST_VAL: u32 = 42;
static STATIC_VAL: u32 = 42;

fn main() {
    let a = &CONST_VAL;
    let b = &CONST_VAL;
    println!("const:  {:p}, {:p}", a, b); // pueden ser DIFERENTES

    let c = &STATIC_VAL;
    let d = &STATIC_VAL;
    println!("static: {:p}, {:p}", c, d); // siempre IGUALES
}
```

## Const generics

Los const generics permiten parametrizar tipos y funciones con
valores constantes. Esencial para arreglos de tamano fijo:

```rust
// Sintaxis: <const N: Tipo>
fn print_array<const N: usize>(arr: [i32; N]) {
    for item in arr {
        print!("{} ", item);
    }
    println!();
}

fn main() {
    print_array([1, 2, 3]);         // N = 3
    print_array([10, 20, 30, 40]);  // N = 4
}
```

```rust
// Const generics en structs:
struct Matrix<const ROWS: usize, const COLS: usize> {
    data: [[f64; COLS]; ROWS],
}

impl<const ROWS: usize, const COLS: usize> Matrix<ROWS, COLS> {
    fn new() -> Self {
        Matrix { data: [[0.0; COLS]; ROWS] }
    }
}

fn main() {
    let m: Matrix<3, 4> = Matrix::new(); // 3 filas, 4 columnas
}
```

## Evaluacion en tiempo de compilacion

### Bloques const y aserciones

```rust
// Bloques const { ... } — evaluar expresiones en contexto no-const:
fn main() {
    let value = const { 2_u32.pow(10) }; // 1024, en compilacion
    println!("{}", value);
}

// Aserciones en compilacion — si fallan, el programa NO compila:
const _: () = assert!(std::mem::size_of::<u32>() == 4);
const _: () = assert!(std::mem::size_of::<usize>() >= 4);
// Patron: const _: () = assert!(...);
// _ descarta el nombre, () es el resultado de assert! exitoso.
```

```rust
// Validar invariantes de una estructura en compilacion:
struct Config {
    buffer_size: usize,
    max_connections: u32,
}

impl Config {
    const fn new(buffer_size: usize, max_connections: u32) -> Self {
        assert!(buffer_size > 0, "buffer_size must be positive");
        assert!(buffer_size <= 1024 * 1024, "buffer_size too large");
        Config { buffer_size, max_connections }
    }
}

const DEFAULT_CONFIG: Config = Config::new(4096, 100); // OK
// const BAD: Config = Config::new(0, 100);
//   error: evaluation of constant panicked: buffer_size must be positive
```

```rust
// Tablas generadas en compilacion — costo cero en ejecucion:
const fn generate_squares() -> [u32; 16] {
    let mut table = [0u32; 16];
    let mut i = 0;
    while i < 16 {
        table[i] = (i as u32) * (i as u32);
        i += 1;
    }
    table
}

const SQUARES: [u32; 16] = generate_squares();
// SQUARES = [0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, ...]
```

## Ejemplo completo

```rust
use std::sync::atomic::{AtomicU64, Ordering};

const MAX_RETRIES: u32 = 3;
const TIMEOUT_MS: u64 = 5000;
const VERSION: &str = "1.0.0";

const fn build_powers_of_two() -> [u64; 16] {
    let mut table = [0u64; 16];
    let mut i = 0;
    while i < 16 {
        table[i] = 1 << i;
        i += 1;
    }
    table
}

const POWERS_OF_TWO: [u64; 16] = build_powers_of_two();
const _: () = assert!(MAX_RETRIES > 0, "MAX_RETRIES must be positive");

static APP_NAME: &str = "ConstDemo";
static REQUEST_COUNT: AtomicU64 = AtomicU64::new(0);

fn handle_request() {
    REQUEST_COUNT.fetch_add(1, Ordering::Relaxed);
}

fn main() {
    println!("{} v{}", APP_NAME, VERSION);
    println!("2^10 = {}", POWERS_OF_TWO[10]); // 1024

    for _ in 0..5 {
        handle_request();
    }
    println!("Requests: {}", REQUEST_COUNT.load(Ordering::Relaxed)); // 5
}
```

```bash
cargo run
# ConstDemo v1.0.0
# 2^10 = 1024
# Requests: 5
```

---

## Ejercicios

### Ejercicio 1 — const y const fn

```rust
// 1. Declarar constantes:
//    SECONDS_PER_MINUTE: u32, MINUTES_PER_HOUR: u32, HOURS_PER_DAY: u32
//
// 2. Crear const fn seconds_per_day() -> u32
//    que calcule los segundos en un dia usando las constantes.
//
// 3. Declarar SECONDS_PER_DAY usando seconds_per_day().
//
// 4. Crear const fn days_to_seconds(days: u32) -> u32.
//
// 5. Declarar SECONDS_PER_WEEK usando days_to_seconds(7).
//
// 6. Verificar: SECONDS_PER_DAY == 86400, SECONDS_PER_WEEK == 604800.
```

### Ejercicio 2 — static vs const

```rust
// 1. Declarar const CONST_ID: u32 = 100 y static STATIC_ID: u32 = 100.
//
// 2. Tomar &CONST_ID dos veces, imprimir direcciones con {:p}.
//    Observar si son iguales o distintas.
//
// 3. Hacer lo mismo con &STATIC_ID. Observar que siempre son iguales.
//
// 4. Crear static LARGE_TABLE: [u32; 1000] inicializada con const fn
//    que llena cada posicion con su indice. Verificar LARGE_TABLE[500] == 500.
//
// 5. Reflexionar: si LARGE_TABLE fuera const, que pasaria con la memoria
//    si se referencia en 10 lugares distintos del codigo?
```

### Ejercicio 3 — Alternativas seguras a static mut

```rust
// 1. Crear un contador de eventos usando AtomicU32 (sin static mut).
//
// 2. Implementar:
//    - log_event() — incrementa el contador
//    - event_count() -> u32 — retorna el valor actual
//    - reset_events() — pone el contador en 0
//
// 3. En main, llamar log_event() 10 veces, imprimir event_count(),
//    llamar reset_events(), imprimir de nuevo.
//
// 4. Verificar que compila SIN ningun bloque unsafe.
//
// 5. Bonus: agregar OnceLock<String> para nombre del logger.
//    Inicializarlo con init_logger("main"). Intentar inicializarlo
//    dos veces y manejar el error.
```
