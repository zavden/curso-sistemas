# Union access y static mut

## Índice

1. [Unions en Rust](#unions-en-rust)
2. [Acceder a campos de union (unsafe)](#acceder-a-campos-de-union-unsafe)
3. [Unions con repr(C)](#unions-con-reprc)
4. [Unions y pattern matching](#unions-y-pattern-matching)
5. [ManuallyDrop en unions](#manuallydrop-en-unions)
6. [MaybeUninit: la union más útil](#maybeuninit-la-union-más-útil)
7. [static mut: variables estáticas mutables](#static-mut-variables-estáticas-mutables)
8. [Alternativas a static mut](#alternativas-a-static-mut)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Unions en Rust

Una `union` es similar a un `struct`, pero todos los campos **comparten la misma memoria**. Solo un campo es válido en cada momento — pero Rust no sabe cuál.

```rust
union IntOrFloat {
    i: i32,
    f: f32,
}

fn main() {
    let mut u = IntOrFloat { i: 42 };

    // Escribir un campo es safe
    u.i = 100;

    // Leer un campo es UNSAFE: Rust no sabe cuál es el campo activo
    let val = unsafe { u.i };
    println!("i = {}", val); // i = 100
}
```

```
┌──────────────────────────────────────────────────┐
│              struct vs union                     │
├──────────────────────────────────────────────────┤
│                                                  │
│  struct Foo { a: i32, b: f32 }                   │
│  ┌──────────┬──────────┐                         │
│  │  a: i32  │  b: f32  │  8 bytes total          │
│  └──────────┴──────────┘                         │
│                                                  │
│  union Bar { a: i32, b: f32 }                    │
│  ┌──────────────────────┐                        │
│  │  a: i32  ╱  b: f32   │  4 bytes total         │
│  └──────────────────────┘  (comparten memoria)   │
│                                                  │
└──────────────────────────────────────────────────┘
```

### Restricciones de unions

```rust
// Los campos de union deben implementar Copy...
union Valid {
    a: i32,
    b: f64,
    c: u8,
}

// ...O estar envueltos en ManuallyDrop si no son Copy
use std::mem::ManuallyDrop;

union WithString {
    i: i32,
    s: ManuallyDrop<String>,  // String no es Copy
}
```

---

## Acceder a campos de union (unsafe)

Leer un campo de union es uno de los 5 superpoderes de `unsafe`. Escribir un campo es safe (en Rust, no en C).

### Lectura: siempre unsafe

```rust
union Number {
    integer: i64,
    floating: f64,
    bytes: [u8; 8],
}

fn main() {
    let n = Number { integer: 42 };

    // Leer requiere unsafe — tú garantizas que el campo es el correcto
    unsafe {
        println!("integer: {}", n.integer);   // 42
        // Leer 'floating' aquí sería reinterpretar los bits — válido
        // pero el resultado no tiene sentido como float útil
        println!("floating: {}", n.floating); // algún f64
        println!("bytes: {:?}", n.bytes);     // [42, 0, 0, 0, 0, 0, 0, 0]
    }
}
```

### Escritura: safe (pero ojo)

```rust
union Data {
    a: u32,
    b: f32,
}

fn main() {
    let mut d = Data { a: 0 };

    // Escribir es safe — simplemente sobrescribe los bytes
    d.a = 42;
    d.b = 3.14; // ahora el campo activo es 'b'

    // Pero leer 'a' después de escribir 'b' sería reinterpretar bits
    unsafe {
        println!("b = {}", d.b);  // 3.14
        // d.a sería la representación de 3.14 como u32
        println!("a = {:#010X}", d.a); // 0x4048F5C3
    }
}
```

> **Predicción**: si escribes `d.b = 1.0f32` y luego lees `d.a`, ¿qué valor obtienes?
> `0x3F800000` — la representación IEEE 754 de `1.0f32`.

### Cuándo es válido leer un campo "incorrecto"

En Rust (a diferencia de C), leer el campo "equivocado" de una union está definido si:

1. **Ambos tipos tienen la misma representación válida para esos bits**. Ejemplo: `i32` ↔ `u32` (transmute entre enteros del mismo tamaño).
2. **Usas tipos que aceptan cualquier patrón de bits**: `u8`, `[u8; N]`, `MaybeUninit<T>`.

```rust
// Caso válido: type punning entre enteros del mismo tamaño
union IntReinterpret {
    signed: i32,
    unsigned: u32,
}

fn main() {
    let u = IntReinterpret { signed: -1 };
    // SAFETY: i32 y u32 tienen el mismo tamaño y ambos
    // aceptan cualquier patrón de bits de 32 bits
    let as_unsigned = unsafe { u.unsigned };
    println!("{}", as_unsigned); // 4294967295 (0xFFFFFFFF)
}
```

---

## Unions con repr(C)

Para FFI, las unions deben usar `#[repr(C)]` para garantizar layout compatible con C.

### Equivalencia con union de C

```c
// En C:
typedef union {
    int32_t i;
    float f;
    uint8_t bytes[4];
} CValue;
```

```rust
// En Rust (equivalente):
#[repr(C)]
union CValue {
    i: i32,
    f: f32,
    bytes: [u8; 4],
}
```

### Tagged union (discriminated union)

El patrón más común es combinar una union con un enum que indica el campo activo:

```rust
#[repr(C)]
#[derive(Clone, Copy)]
enum ValueType {
    Integer = 0,
    Float = 1,
    Boolean = 2,
}

#[repr(C)]
#[derive(Clone, Copy)]
union ValueData {
    integer: i64,
    float: f64,
    boolean: u8,  // 0 = false, 1 = true
}

#[repr(C)]
struct TaggedValue {
    tag: ValueType,
    data: ValueData,
}

impl TaggedValue {
    fn new_int(v: i64) -> Self {
        TaggedValue {
            tag: ValueType::Integer,
            data: ValueData { integer: v },
        }
    }

    fn new_float(v: f64) -> Self {
        TaggedValue {
            tag: ValueType::Float,
            data: ValueData { float: v },
        }
    }

    fn new_bool(v: bool) -> Self {
        TaggedValue {
            tag: ValueType::Boolean,
            data: ValueData { boolean: v as u8 },
        }
    }

    fn display(&self) {
        match self.tag {
            ValueType::Integer => {
                // SAFETY: tag es Integer, así que data.integer es el campo activo
                let v = unsafe { self.data.integer };
                println!("int: {}", v);
            }
            ValueType::Float => {
                // SAFETY: tag es Float, así que data.float es el campo activo
                let v = unsafe { self.data.float };
                println!("float: {}", v);
            }
            ValueType::Boolean => {
                // SAFETY: tag es Boolean, data.boolean es 0 o 1
                let v = unsafe { self.data.boolean != 0 };
                println!("bool: {}", v);
            }
        }
    }
}

fn main() {
    let values = [
        TaggedValue::new_int(42),
        TaggedValue::new_float(3.14),
        TaggedValue::new_bool(true),
    ];

    for v in &values {
        v.display();
    }
    // int: 42
    // float: 3.14
    // bool: true
}
```

```
┌─────────────────────────────────────────────────────┐
│  Tagged union en memoria (repr(C))                  │
│                                                     │
│  TaggedValue:                                       │
│  ┌──────────┬──────────────────────┐                │
│  │ tag: u32 │ data (8 bytes)       │                │
│  │ (enum)   │ integer/float/boolean │                │
│  └──────────┴──────────────────────┘                │
│  │← 4 bytes→│←    8 bytes         →│                │
│                                                     │
│  Equivale al patrón de enum de Rust:                │
│  enum Value {                                       │
│      Integer(i64),                                  │
│      Float(f64),                                    │
│      Boolean(bool),                                 │
│  }                                                  │
│  → Rust maneja el tag automáticamente               │
│  → Usar enum siempre que no necesites FFI           │
└─────────────────────────────────────────────────────┘
```

> **Nota**: los `enum` de Rust ya son tagged unions internamente. Solo necesitas `union` explícita para FFI con C o cuando controlas el layout manualmente.

---

## Unions y pattern matching

Puedes hacer pattern matching sobre campos de union, pero dentro de `unsafe`:

```rust
union Packet {
    raw: [u8; 4],
    value: u32,
}

fn main() {
    let p = Packet { value: 0x01020304 };

    unsafe {
        // Pattern matching destructurando
        match p {
            Packet { raw: [a, b, c, d] } => {
                // Little-endian: byte 0 = parte baja
                println!("bytes: {:#04X} {:#04X} {:#04X} {:#04X}", a, b, c, d);
                // bytes: 0x04 0x03 0x02 0x01
            }
        }

        // Binding con ref
        let Packet { value: ref v } = p;
        println!("value: {:#010X}", v); // value: 0x01020304
    }
}
```

### if let con unions

```rust
union SmallOrBig {
    small: u8,
    big: u64,
}

fn main() {
    let s = SmallOrBig { small: 42 };

    unsafe {
        if let SmallOrBig { small: v @ 0..=127 } = s {
            println!("small ASCII-range value: {}", v);
        }
    }
}
```

---

## ManuallyDrop en unions

Los campos de union que no son `Copy` deben envolverse en `ManuallyDrop<T>` porque Rust no puede saber qué campo dropear.

```rust
use std::mem::ManuallyDrop;

union StringOrVec {
    text: ManuallyDrop<String>,
    data: ManuallyDrop<Vec<u8>>,
}

impl StringOrVec {
    fn from_string(s: String) -> Self {
        StringOrVec {
            text: ManuallyDrop::new(s),
        }
    }

    fn from_vec(v: Vec<u8>) -> Self {
        StringOrVec {
            data: ManuallyDrop::new(v),
        }
    }
}

fn main() {
    let mut u = StringOrVec::from_string(String::from("hello"));

    unsafe {
        println!("text: {}", *u.text); // text: hello

        // Para dropear, debes hacerlo manualmente sobre el campo CORRECTO
        ManuallyDrop::drop(&mut u.text);
        // Después de drop, no uses u.text de nuevo
    }
}
```

### Patrón: tagged union con Drop

```rust
use std::mem::ManuallyDrop;

enum Tag { Text, Data }

struct SmartUnion {
    tag: Tag,
    value: StringOrVec,
}

union StringOrVec2 {
    text: ManuallyDrop<String>,
    data: ManuallyDrop<Vec<u8>>,
}

impl SmartUnion {
    fn new_text(s: String) -> Self {
        SmartUnion {
            tag: Tag::Text,
            value: StringOrVec2 {
                text: ManuallyDrop::new(s),
            },
        }
    }

    fn new_data(v: Vec<u8>) -> Self {
        SmartUnion {
            tag: Tag::Data,
            value: StringOrVec2 {
                data: ManuallyDrop::new(v),
            },
        }
    }
}

impl Drop for SmartUnion {
    fn drop(&mut self) {
        match self.tag {
            Tag::Text => {
                // SAFETY: tag indica que el campo activo es text
                unsafe { ManuallyDrop::drop(&mut self.value.text); }
            }
            Tag::Data => {
                // SAFETY: tag indica que el campo activo es data
                unsafe { ManuallyDrop::drop(&mut self.value.data); }
            }
        }
    }
}

fn main() {
    let a = SmartUnion::new_text(String::from("hello"));
    let b = SmartUnion::new_data(vec![1, 2, 3]);
    // a y b se dropean correctamente al salir de scope
}
```

---

## MaybeUninit: la union más útil

`MaybeUninit<T>` es una union definida en la librería estándar que representa un valor que puede o no estar inicializado:

```rust
// Definición simplificada en std:
// union MaybeUninit<T> {
//     uninit: (),
//     value: ManuallyDrop<T>,
// }
```

### Inicialización diferida

```rust
use std::mem::MaybeUninit;

fn main() {
    // Crear valor no inicializado — safe
    let mut x: MaybeUninit<i32> = MaybeUninit::uninit();

    // Escribir valor — safe
    x.write(42);

    // Leer valor — unsafe (tú garantizas que fue inicializado)
    let value = unsafe { x.assume_init() };
    println!("{}", value); // 42
}
```

### Inicializar un array elemento por elemento

```rust
use std::mem::MaybeUninit;

fn create_squares(n: usize) -> Vec<i32> {
    let mut buf: Vec<MaybeUninit<i32>> = Vec::with_capacity(n);

    for i in 0..n {
        buf.push(MaybeUninit::new((i * i) as i32));
    }

    // SAFETY: todos los elementos fueron inicializados en el loop
    // MaybeUninit<i32> y i32 tienen el mismo layout (garantizado)
    unsafe {
        let ptr = buf.as_mut_ptr() as *mut i32;
        let len = buf.len();
        let cap = buf.capacity();
        std::mem::forget(buf); // evitar double-free
        Vec::from_raw_parts(ptr, len, cap)
    }
}

fn main() {
    let squares = create_squares(5);
    println!("{:?}", squares); // [0, 1, 4, 9, 16]
}
```

### MaybeUninit con arrays (API estable desde 1.72+)

```rust
use std::mem::MaybeUninit;

fn main() {
    // Crear array no inicializado
    let mut arr: [MaybeUninit<String>; 3] = [
        MaybeUninit::uninit(),
        MaybeUninit::uninit(),
        MaybeUninit::uninit(),
    ];

    // O más corto con unsafe:
    // let mut arr: [MaybeUninit<String>; 3] = unsafe {
    //     MaybeUninit::uninit().assume_init()
    // };

    // Inicializar cada elemento
    arr[0].write(String::from("hello"));
    arr[1].write(String::from("world"));
    arr[2].write(String::from("!"));

    // Convertir a array inicializado
    // SAFETY: los 3 elementos fueron escritos arriba
    let arr: [String; 3] = unsafe {
        // transmute no funciona aquí por tamaño genérico,
        // usamos ptr::read
        let ptr = arr.as_ptr() as *const [String; 3];
        std::ptr::read(ptr)
    };

    println!("{:?}", arr); // ["hello", "world", "!"]
}
```

### ¿Por qué MaybeUninit y no mem::uninitialized?

```rust
// mem::uninitialized() está DEPRECATED — nunca usar
// Crea un valor "inicializado" con basura, causando UB inmediato para
// tipos como bool, &T, enums, etc.

// MaybeUninit es la alternativa correcta:
// - Es una union, así que tener "basura" es válido (campo uninit)
// - No se puede leer accidentalmente (necesitas assume_init)
// - El compilador sabe que puede no estar inicializado
```

---

## static mut: variables estáticas mutables

`static mut` declara una variable global mutable. Acceder a ella (leer o escribir) es `unsafe` porque no hay protección contra data races.

### Sintaxis básica

```rust
static mut COUNTER: u32 = 0;

fn increment() {
    // UNSAFE: acceder a static mut
    unsafe {
        COUNTER += 1;
    }
}

fn get_count() -> u32 {
    unsafe { COUNTER }
}

fn main() {
    increment();
    increment();
    increment();
    println!("count: {}", get_count()); // count: 3
}
```

### El problema fundamental

```rust
static mut BUFFER: Vec<i32> = Vec::new();

fn push_value(v: i32) {
    unsafe {
        BUFFER.push(v);
    }
}

// Si dos threads llaman push_value simultáneamente:
// Thread A: BUFFER.push(1)  ← puede reubicar el buffer interno
// Thread B: BUFFER.push(2)  ← usa el puntero antiguo → UB
//
// Incluso en single-thread, el compilador puede reordenar accesos
// porque no hay barrera de sincronización.
```

```
┌──────────────────────────────────────────────────────┐
│  static mut: por qué es problemático                 │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Thread A           Thread B                         │
│  ────────           ────────                         │
│  read COUNTER (=5)                                   │
│                     read COUNTER (=5)                │
│  COUNTER = 5 + 1                                     │
│                     COUNTER = 5 + 1                  │
│  write COUNTER (=6)                                  │
│                     write COUNTER (=6)  ← ¡perdido!  │
│                                                      │
│  Resultado: 6 (debería ser 7) → data race           │
│                                                      │
│  Con atomics: no hay este problema                   │
│  Con Mutex: no hay este problema                     │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### static mut se está deprecando

A partir de Rust 2024 edition, `static mut` genera warnings y se espera que sea removido eventualmente. Las alternativas modernas son preferibles en todos los casos.

```rust
// Rust 2024: warning[E0796]: creating a mutable reference to a mutable static
// is deprecated and will be hard-error in a future edition

static mut OLD_WAY: i32 = 0;

fn main() {
    // Rust 2024 emite warning aquí:
    unsafe {
        OLD_WAY += 1; // warning: creating a mutable reference to a mutable static
    }

    // Forma aceptada (sin referencia mutable):
    unsafe {
        let ptr = std::ptr::addr_of_mut!(OLD_WAY);
        *ptr += 1;  // sin warning, pero sigue siendo unsafe
    }
}
```

---

## Alternativas a static mut

### 1. AtomicT para tipos simples (preferida)

```rust
use std::sync::atomic::{AtomicU32, AtomicBool, Ordering};

static COUNTER: AtomicU32 = AtomicU32::new(0);
static INITIALIZED: AtomicBool = AtomicBool::new(false);

fn increment() {
    COUNTER.fetch_add(1, Ordering::Relaxed);
}

fn get_count() -> u32 {
    COUNTER.load(Ordering::Relaxed)
}

fn main() {
    increment();
    increment();
    increment();
    println!("count: {}", get_count()); // count: 3
    // Sin unsafe, sin data races
}
```

### 2. OnceLock para inicialización única (Rust 1.70+)

```rust
use std::sync::OnceLock;

static CONFIG: OnceLock<String> = OnceLock::new();

fn get_config() -> &'static str {
    CONFIG.get_or_init(|| {
        // Inicialización costosa, solo ocurre una vez
        println!("initializing config...");
        String::from("production")
    })
}

fn main() {
    println!("{}", get_config()); // initializing config... production
    println!("{}", get_config()); // production (sin reinicializar)
}
```

### 3. Mutex/RwLock para estado mutable complejo

```rust
use std::sync::Mutex;

static REGISTRY: Mutex<Vec<String>> = Mutex::new(Vec::new());

fn register(name: &str) {
    REGISTRY.lock().unwrap().push(name.to_string());
}

fn list_registered() -> Vec<String> {
    REGISTRY.lock().unwrap().clone()
}

fn main() {
    register("alice");
    register("bob");
    register("charlie");

    println!("{:?}", list_registered());
    // ["alice", "bob", "charlie"]
}
```

### 4. LazyLock para inicialización lazy con estado (Rust 1.80+)

```rust
use std::sync::LazyLock;
use std::collections::HashMap;

static DEFAULTS: LazyLock<HashMap<&str, i32>> = LazyLock::new(|| {
    let mut m = HashMap::new();
    m.insert("timeout", 30);
    m.insert("retries", 3);
    m.insert("port", 8080);
    m
});

fn main() {
    println!("timeout: {}", DEFAULTS["timeout"]);   // 30
    println!("port: {}", DEFAULTS["port"]);          // 8080
}
```

### Comparación de alternativas

```
┌────────────────────┬───────────┬──────────┬──────────┬────────────┐
│ Solución           │ Mutable   │ Thread-  │ unsafe   │ Caso de uso│
│                    │           │ safe     │          │            │
├────────────────────┼───────────┼──────────┼──────────┼────────────┤
│ static mut         │ ✓         │ ✗        │ ✓        │ EVITAR     │
│ AtomicT            │ ✓         │ ✓        │ ✗        │ contadores │
│ OnceLock           │ init once │ ✓        │ ✗        │ config     │
│ LazyLock           │ init once │ ✓        │ ✗        │ maps, etc  │
│ Mutex<T>           │ ✓         │ ✓        │ ✗        │ estado     │
│ RwLock<T>          │ ✓         │ ✓        │ ✗        │ read-heavy │
└────────────────────┴───────────┴──────────┴──────────┴────────────┘
```

---

## Errores comunes

### 1. Leer el campo incorrecto de una union

```rust
union Data {
    text_ptr: *const u8,
    number: u64,
}

fn main() {
    let d = Data { number: 42 };

    unsafe {
        // INCORRECTO: interpretar 42 como un puntero y dereferenciar
        // let s = std::ffi::CStr::from_ptr(d.text_ptr as *const i8);
        // → segfault o UB

        // CORRECTO: solo leer el campo que fue escrito
        println!("{}", d.number); // 42
    }
}
```

### 2. Dropear el campo equivocado de una union con ManuallyDrop

```rust
use std::mem::ManuallyDrop;

union Resource {
    name: ManuallyDrop<String>,
    id: u64,
}

fn main() {
    let mut r = Resource { id: 42 };

    unsafe {
        // INCORRECTO: dropear name cuando el campo activo es id
        // ManuallyDrop::drop(&mut r.name);
        // → intenta liberar memoria en la dirección 42 → crash

        // CORRECTO: solo dropear si sabes que name es el campo activo
        let mut r2 = Resource {
            name: ManuallyDrop::new(String::from("hello")),
        };
        ManuallyDrop::drop(&mut r2.name); // ✓
    }
}
```

### 3. Usar static mut en código multi-thread

```rust
static mut SHARED: Vec<i32> = Vec::new();

fn bad_multithreaded() {
    let handles: Vec<_> = (0..4).map(|i| {
        std::thread::spawn(move || {
            unsafe {
                // DATA RACE: múltiples threads mutan sin sincronización
                SHARED.push(i);
            }
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }
}

// CORRECTO: usar Mutex
use std::sync::Mutex;

static SHARED_SAFE: Mutex<Vec<i32>> = Mutex::new(Vec::new());

fn good_multithreaded() {
    let handles: Vec<_> = (0..4).map(|i| {
        std::thread::spawn(move || {
            SHARED_SAFE.lock().unwrap().push(i);
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }
}
```

### 4. Olvidar que MaybeUninit no dropea su contenido

```rust
use std::mem::MaybeUninit;

fn leaks_memory() {
    let mut m: MaybeUninit<String> = MaybeUninit::uninit();
    m.write(String::from("this will leak"));

    // m sale de scope pero MaybeUninit NO llama drop en su contenido
    // → memory leak

    // CORRECTO: llamar assume_init() para tomar ownership
    // let s = unsafe { m.assume_init() };
    // ahora s se dropea normalmente

    // O dropear manualmente:
    // unsafe { m.assume_init_drop(); }
}
```

### 5. Crear referencia mutable a static mut (deprecado en 2024)

```rust
static mut DATA: [i32; 4] = [0; 4];

fn main() {
    unsafe {
        // DEPRECADO en Rust 2024: crear &mut a static mut
        // let slice = &mut DATA[..];

        // CORRECTO en Rust 2024: usar addr_of_mut! + raw pointers
        let ptr = std::ptr::addr_of_mut!(DATA);
        let slice = std::slice::from_raw_parts_mut(
            (*ptr).as_mut_ptr(),
            (*ptr).len(),
        );
        slice[0] = 42;
        println!("{:?}", slice); // [42, 0, 0, 0]
    }
}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║       UNION ACCESS Y STATIC MUT — REF. RÁPIDA           ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  UNIONS                                                   ║
║  ──────                                                   ║
║  union Foo { a: T1, b: T2 }                              ║
║  Campos comparten memoria (tamaño = max de campos)       ║
║  Campos deben ser Copy o ManuallyDrop<T>                  ║
║  Escribir campo: safe                                     ║
║  Leer campo: unsafe (tú sabes cuál es el activo)         ║
║                                                           ║
║  #[repr(C)] union  → layout compatible con C             ║
║                                                           ║
║  TAGGED UNION (patrón)                                    ║
║  ─────────────────────                                    ║
║  struct Tagged { tag: enum, data: union }                ║
║  match tag → leer el campo correcto                      ║
║  Drop → dropear según tag                                ║
║  → Preferir enum de Rust salvo para FFI                  ║
║                                                           ║
║  MANUALLYDROP                                             ║
║  ────────────                                             ║
║  ManuallyDrop::new(val)       envolver                   ║
║  ManuallyDrop::drop(&mut md)  dropear (unsafe)           ║
║  *md                          acceder al valor            ║
║                                                           ║
║  MAYBEUNINIT                                              ║
║  ───────────                                              ║
║  MaybeUninit::uninit()    crear sin inicializar           ║
║  MaybeUninit::new(val)    crear inicializado              ║
║  mu.write(val)            escribir (safe)                 ║
║  mu.assume_init()         leer (unsafe, consume)          ║
║  mu.assume_init_ref()     &T (unsafe)                     ║
║  mu.assume_init_drop()    dropear (unsafe)                ║
║                                                           ║
║  STATIC MUT (⚠ EVITAR)                                   ║
║  ──────────────────────                                   ║
║  static mut X: T = val;                                   ║
║  unsafe { X += 1; }      leer/escribir requiere unsafe   ║
║  Sin protección contra data races                        ║
║  Deprecándose en Rust 2024                               ║
║                                                           ║
║  ALTERNATIVAS A STATIC MUT (PREFERIDAS)                  ║
║  ──────────────────────────────────────                   ║
║  AtomicT      → contadores, flags                        ║
║  OnceLock<T>  → inicializar una vez                      ║
║  LazyLock<T>  → inicialización lazy                      ║
║  Mutex<T>     → estado mutable complejo                  ║
║  RwLock<T>    → muchas lecturas, pocas escrituras        ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Valor polimórfico con tagged union

Implementa un tipo `Value` que pueda contener enteros, floats, booleanos y strings, usando una union `#[repr(C)]` con tag explícito:

```rust
use std::mem::ManuallyDrop;

#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
enum ValueTag {
    Null,
    Int,
    Float,
    Bool,
    Str,
}

#[repr(C)]
union ValueData {
    int_val: i64,
    float_val: f64,
    bool_val: bool,
    str_val: ManuallyDrop<String>,
}

struct Value {
    tag: ValueTag,
    data: ValueData,
}

impl Value {
    fn null() -> Self { todo!() }
    fn from_int(v: i64) -> Self { todo!() }
    fn from_float(v: f64) -> Self { todo!() }
    fn from_bool(v: bool) -> Self { todo!() }
    fn from_str(v: &str) -> Self { todo!() }

    fn tag(&self) -> ValueTag { todo!() }
    fn as_int(&self) -> Option<i64> { todo!() }
    fn as_float(&self) -> Option<f64> { todo!() }
    fn as_bool(&self) -> Option<bool> { todo!() }
    fn as_str(&self) -> Option<&str> { todo!() }
}

impl std::fmt::Display for Value { /* ... */ }
impl Drop for Value { /* solo dropear str_val si tag == Str */ }
impl Clone for Value { /* clonar según tag */ }
```

Prueba con:
```rust
let values = vec![
    Value::null(),
    Value::from_int(42),
    Value::from_float(3.14),
    Value::from_bool(true),
    Value::from_str("hello"),
];
for v in &values {
    println!("{}", v);
}
```

**Pregunta de reflexión**: ¿Cuántos bytes ocupa `Value`? Compáralo con `enum Value { Null, Int(i64), Float(f64), Bool(bool), Str(String) }`. ¿Cuál es más eficiente en espacio? ¿Cuál es más seguro?

---

### Ejercicio 2: Migrar static mut a alternativas modernas

Dado este código que usa `static mut` en todas partes, reescríbelo sin ningún `static mut` ni `unsafe`:

```rust
static mut NEXT_ID: u64 = 1;
static mut USERS: Vec<(u64, String)> = Vec::new();
static mut APP_NAME: Option<String> = None;
static mut REQUEST_COUNT: u64 = 0;

unsafe fn init(name: &str) {
    APP_NAME = Some(name.to_string());
    NEXT_ID = 1;
    USERS.clear();
    REQUEST_COUNT = 0;
}

unsafe fn add_user(name: &str) -> u64 {
    REQUEST_COUNT += 1;
    let id = NEXT_ID;
    NEXT_ID += 1;
    USERS.push((id, name.to_string()));
    id
}

unsafe fn get_user(id: u64) -> Option<String> {
    REQUEST_COUNT += 1;
    USERS.iter()
        .find(|(uid, _)| *uid == id)
        .map(|(_, name)| name.clone())
}

unsafe fn stats() -> (u64, usize) {
    (REQUEST_COUNT, USERS.len())
}
```

**Pistas**:
- `NEXT_ID` → `AtomicU64`
- `REQUEST_COUNT` → `AtomicU64`
- `USERS` → `Mutex<Vec<(u64, String)>>`
- `APP_NAME` → `OnceLock<String>`
- Todas las funciones pasan a ser safe

**Pregunta de reflexión**: ¿Hay algún escenario donde `static mut` sea realmente la única opción? (Pista: piensa en handlers de señales POSIX, interrupt handlers en embedded, o código que se ejecuta antes de que el runtime de threads esté disponible.)

---

### Ejercicio 3: Buffer con MaybeUninit

Implementa un `FixedVec<T, const N: usize>` — un vector de tamaño fijo que vive en el stack, usando `MaybeUninit`:

```rust
use std::mem::MaybeUninit;

struct FixedVec<T, const N: usize> {
    data: [MaybeUninit<T>; N],
    len: usize,
}

impl<T, const N: usize> FixedVec<T, N> {
    fn new() -> Self { todo!() }
    fn push(&mut self, value: T) -> Result<(), T> { todo!() }  // Err si lleno
    fn pop(&mut self) -> Option<T> { todo!() }
    fn get(&self, index: usize) -> Option<&T> { todo!() }
    fn len(&self) -> usize { todo!() }
    fn is_full(&self) -> bool { todo!() }
    fn as_slice(&self) -> &[T] { todo!() }
}

impl<T, const N: usize> Drop for FixedVec<T, N> {
    fn drop(&mut self) {
        // Dropear solo los elementos inicializados (0..self.len)
        todo!()
    }
}
```

Prueba con:
```rust
let mut v: FixedVec<String, 4> = FixedVec::new();
v.push(String::from("a")).unwrap();
v.push(String::from("b")).unwrap();
v.push(String::from("c")).unwrap();
assert_eq!(v.len(), 3);
assert_eq!(v.get(1), Some(&String::from("b")));
assert_eq!(v.as_slice(), &["a", "b", "c"]);
let popped = v.pop();
assert_eq!(popped, Some(String::from("c")));
// Al dropear, "a" y "b" se liberan correctamente
```

**Pistas**:
- `new`: usa `[const { MaybeUninit::uninit() }; N]` (Rust 1.79+) o un loop con `MaybeUninit::uninit()`.
- `push`: `self.data[self.len].write(value); self.len += 1;`
- `pop`: `self.len -= 1; unsafe { self.data[self.len].assume_init_read() }`
- `as_slice`: `unsafe { std::slice::from_raw_parts(self.data.as_ptr() as *const T, self.len) }`
- `drop`: iterar `0..self.len` y llamar `assume_init_drop()` en cada uno.

**Pregunta de reflexión**: ¿Qué pasaría si `push` paniquease entre `write` y `len += 1`? ¿Se perdería el elemento? ¿Cómo podrías hacerlo panic-safe?
