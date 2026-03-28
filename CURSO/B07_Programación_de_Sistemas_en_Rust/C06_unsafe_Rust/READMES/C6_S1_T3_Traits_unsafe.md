# Traits unsafe

## Índice

1. [¿Qué es un trait unsafe?](#qué-es-un-trait-unsafe)
2. [Send y Sync: los traits unsafe más importantes](#send-y-sync-los-traits-unsafe-más-importantes)
3. [Implementar Send manualmente](#implementar-send-manualmente)
4. [Implementar Sync manualmente](#implementar-sync-manualmente)
5. [Auto-traits: cómo el compilador decide](#auto-traits-cómo-el-compilador-decide)
6. [Negative impls: optar por no implementar](#negative-impls-optar-por-no-implementar)
7. [Definir tus propios traits unsafe](#definir-tus-propios-traits-unsafe)
8. [PhantomData y su efecto en auto-traits](#phantomdata-y-su-efecto-en-auto-traits)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es un trait unsafe?

Un trait marcado `unsafe trait` indica que **implementarlo** requiere garantizar invariantes que el compilador no puede verificar. El código que usa el trait **confía** en que la implementación es correcta.

```
┌──────────────────────────────────────────────────────────┐
│                Trait normal vs unsafe                     │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  trait Clone {              unsafe trait Send {          │
│      fn clone(&self) → T;       // sin métodos          │
│  }                          }                            │
│                                                          │
│  Implementar:               Implementar:                 │
│  impl Clone for Foo { ... } unsafe impl Send for Foo {} │
│  ↑ safe                     ↑ unsafe                     │
│                                                          │
│  Si clone() es incorrecto:  Si Send es incorrecto:       │
│  → bug lógico               → data race → UB            │
│                                                          │
│  El compilador puede        El compilador NO puede       │
│  verificar que retorna T    verificar thread-safety      │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

La diferencia clave:

| Aspecto | `trait` normal | `unsafe trait` |
|---------|---------------|----------------|
| Definir | `trait Foo {}` | `unsafe trait Foo {}` |
| Implementar | `impl Foo for T {}` | `unsafe impl Foo for T {}` |
| Usar | libre | libre (el `unsafe` está en `impl`, no en `use`) |
| Error de impl | Bug lógico | Undefined Behavior |

> **Punto importante**: usar un `unsafe trait` (tenerlo como bound `T: Send`) **no** requiere `unsafe`. Solo **implementarlo** lo requiere.

---

## Send y Sync: los traits unsafe más importantes

Estos dos traits son la base del sistema de concurrencia de Rust. Ambos son `unsafe trait` y ambos son **auto-traits** (el compilador los implementa automáticamente si todos los campos los implementan).

### Send

```rust
// Definido en std::marker (simplificado)
pub unsafe auto trait Send {}
```

`T: Send` significa: "es seguro **transferir** un valor de tipo `T` a otro thread".

```rust
use std::thread;

fn must_be_send<T: Send>(val: T) {
    thread::spawn(move || {
        println!("received in another thread");
        drop(val);
    });
}

fn main() {
    let s = String::from("hello");
    must_be_send(s); // String: Send ✓

    let n = 42i32;
    must_be_send(n); // i32: Send ✓

    // Rc<T> no es Send
    // let rc = std::rc::Rc::new(42);
    // must_be_send(rc); // ERROR: Rc<i32> is not Send
}
```

### Sync

```rust
// Definido en std::marker (simplificado)
pub unsafe auto trait Sync {}
```

`T: Sync` significa: "es seguro **acceder** a `&T` desde múltiples threads simultáneamente".

Equivalencia formal: `T: Sync` si y solo si `&T: Send`.

```rust
use std::sync::Arc;

fn must_be_sync<T: Sync + Send>(val: T) {
    let shared = Arc::new(val);

    let handle = {
        let shared = Arc::clone(&shared);
        std::thread::spawn(move || {
            println!("thread 1: {:?}", *shared);
        })
    };

    println!("main: {:?}", *shared);
    handle.join().unwrap();
}

fn main() {
    must_be_sync(42i32);           // i32: Sync ✓
    must_be_sync(String::from("hello")); // String: Sync ✓

    // Cell<T> no es Sync (mutación interior sin sincronización)
    // must_be_sync(std::cell::Cell::new(42)); // ERROR
}
```

### Tabla de Send/Sync para tipos comunes

```
┌────────────────────────┬──────┬──────┐
│ Tipo                   │ Send │ Sync │
├────────────────────────┼──────┼──────┤
│ i32, f64, bool, char   │  ✓   │  ✓   │
│ String, Vec<T>          │  ✓   │  ✓   │
│ Box<T>                  │  ✓*  │  ✓*  │
│ Arc<T>                  │  ✓*  │  ✓*  │
│ Mutex<T>                │  ✓*  │  ✓   │
│ RwLock<T>               │  ✓*  │  ✓   │
│ Rc<T>                   │  ✗   │  ✗   │
│ Cell<T>, RefCell<T>     │  ✓   │  ✗   │
│ *const T, *mut T        │  ✗   │  ✗   │
│ MutexGuard<T>           │  ✗   │  ✓*  │
│ &T                      │  ✓** │  ✓   │
│ &mut T                  │  ✓*  │  ✓*  │
├────────────────────────┴──────┴──────┤
│ * = si T: Send  /  ** = si T: Sync   │
└──────────────────────────────────────┘
```

---

## Implementar Send manualmente

### Caso típico: struct con raw pointer

```rust
struct RawBuffer {
    ptr: *mut u8,
    len: usize,
}

// *mut u8 no es Send → RawBuffer tampoco lo es automáticamente.
// Pero SI nosotros garantizamos que:
// 1. El buffer es propiedad exclusiva de RawBuffer (no aliased)
// 2. No hay estado compartido sin sincronización
// Entonces es safe implementar Send.

// SAFETY: RawBuffer tiene ownership exclusivo del buffer apuntado por ptr.
// No comparte el puntero con nadie. Transferir RawBuffer a otro thread
// transfiere toda la ownership, sin crear aliasing.
unsafe impl Send for RawBuffer {}
```

### Ejemplo completo: pool de conexiones

```rust
use std::ptr::NonNull;

struct Connection {
    // Simula un file descriptor o handle
    fd: i32,
}

struct ConnectionHandle {
    conn: NonNull<Connection>,
    // NonNull<T> no es Send ni Sync automáticamente
}

impl ConnectionHandle {
    fn new(fd: i32) -> Self {
        let conn = Box::new(Connection { fd });
        ConnectionHandle {
            conn: NonNull::from(Box::leak(conn)),
        }
    }

    fn fd(&self) -> i32 {
        // SAFETY: conn apunta a memoria válida (asignada en new,
        // solo liberada en drop)
        unsafe { self.conn.as_ref().fd }
    }
}

impl Drop for ConnectionHandle {
    fn drop(&mut self) {
        // SAFETY: conn fue creado con Box::leak en new,
        // por lo que reconstruir el Box es válido
        unsafe {
            drop(Box::from_raw(self.conn.as_ptr()));
        }
    }
}

// SAFETY: ConnectionHandle tiene ownership exclusivo de la Connection.
// El fd subyacente es un entero que no tiene afinidad de thread.
// Transferir el handle a otro thread es seguro.
unsafe impl Send for ConnectionHandle {}

fn main() {
    let handle = ConnectionHandle::new(42);

    let t = std::thread::spawn(move || {
        println!("fd in thread: {}", handle.fd());
        // handle se dropea aquí
    });

    t.join().unwrap();
}
```

### Cuándo NO implementar Send

```rust
// NUNCA hagas esto para tipos con afinidad de thread:

struct GpuBuffer {
    // El contexto de GPU solo es válido en el thread que lo creó
    context: *mut std::ffi::c_void,
}

// INCORRECTO: el contexto de GPU no se puede usar desde otro thread
// unsafe impl Send for GpuBuffer {}

// CORRECTO: dejarlo como !Send (el default por tener *mut)
// Esto previene enviarlo a otros threads
```

---

## Implementar Sync manualmente

### Caso: acceso inmutable thread-safe

```rust
use std::ptr::NonNull;

struct SharedConfig {
    data: NonNull<ConfigData>,
}

struct ConfigData {
    max_connections: u32,
    timeout_ms: u64,
    name: String,
}

impl SharedConfig {
    fn new(max_conn: u32, timeout: u64, name: &str) -> Self {
        let data = Box::new(ConfigData {
            max_connections: max_conn,
            timeout_ms: timeout,
            name: name.to_string(),
        });
        SharedConfig {
            data: NonNull::from(Box::leak(data)),
        }
    }

    fn max_connections(&self) -> u32 {
        // SAFETY: data apunta a memoria válida, solo lectura
        unsafe { self.data.as_ref().max_connections }
    }

    fn timeout_ms(&self) -> u64 {
        // SAFETY: data apunta a memoria válida, solo lectura
        unsafe { self.data.as_ref().timeout_ms }
    }

    fn name(&self) -> &str {
        // SAFETY: data apunta a memoria válida, solo lectura
        unsafe { &self.data.as_ref().name }
    }
}

impl Drop for SharedConfig {
    fn drop(&mut self) {
        unsafe { drop(Box::from_raw(self.data.as_ptr())); }
    }
}

// SAFETY: SharedConfig solo expone métodos de lectura (&self).
// Los datos subyacentes nunca se mutan después de la construcción.
// Múltiples threads pueden leer simultáneamente sin data races.
unsafe impl Sync for SharedConfig {}

// También Send: transferir ownership es seguro
// SAFETY: ownership exclusivo, sin estado compartido mutable
unsafe impl Send for SharedConfig {}

fn main() {
    use std::sync::Arc;

    let config = Arc::new(SharedConfig::new(100, 5000, "production"));

    let handles: Vec<_> = (0..4).map(|i| {
        let config = Arc::clone(&config);
        std::thread::spawn(move || {
            println!(
                "thread {}: max={}, timeout={}ms, name={}",
                i,
                config.max_connections(),
                config.timeout_ms(),
                config.name(),
            );
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }
}
```

### Cuándo NO implementar Sync

```rust
use std::cell::UnsafeCell;

struct BadCounter {
    // UnsafeCell permite mutación interior sin sincronización
    count: UnsafeCell<u64>,
}

impl BadCounter {
    fn new() -> Self {
        BadCounter { count: UnsafeCell::new(0) }
    }

    fn increment(&self) {
        // SAFETY: ??? — NO es safe si múltiples threads llaman esto
        unsafe {
            *self.count.get() += 1;  // data race si es Sync
        }
    }
}

// INCORRECTO: increment() tiene data race si se llama concurrentemente
// unsafe impl Sync for BadCounter {}

// CORRECTO: usar AtomicU64 en su lugar
use std::sync::atomic::{AtomicU64, Ordering};

struct GoodCounter {
    count: AtomicU64,
}

impl GoodCounter {
    fn new() -> Self {
        GoodCounter { count: AtomicU64::new(0) }
    }

    fn increment(&self) {
        self.count.fetch_add(1, Ordering::Relaxed);
    }

    fn get(&self) -> u64 {
        self.count.load(Ordering::Relaxed)
    }
}

// GoodCounter es automáticamente Send + Sync (AtomicU64 lo es)
```

---

## Auto-traits: cómo el compilador decide

`Send` y `Sync` son **auto-traits**: el compilador los implementa automáticamente según las reglas de composición.

### Reglas de derivación automática

```
┌──────────────────────────────────────────────────────┐
│          Auto-derivación de Send y Sync              │
├──────────────────────────────────────────────────────┤
│                                                      │
│  struct Foo {                                        │
│      a: A,                                           │
│      b: B,                                           │
│      c: C,                                           │
│  }                                                   │
│                                                      │
│  Foo: Send   ←→   A: Send AND B: Send AND C: Send   │
│  Foo: Sync   ←→   A: Sync AND B: Sync AND C: Sync   │
│                                                      │
│  Si CUALQUIER campo no es Send → Foo no es Send      │
│  Si CUALQUIER campo no es Sync → Foo no es Sync      │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Verificar en la práctica

```rust
// Helper para verificar en tiempo de compilación
fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

struct AllSafe {
    a: i32,
    b: String,
    c: Vec<u8>,
}

struct HasRawPtr {
    data: Vec<i32>,
    cache: *const i32,  // *const T: !Send, !Sync
}

struct HasCell {
    value: std::cell::Cell<i32>,  // Cell: Send, !Sync
}

struct HasRc {
    shared: std::rc::Rc<String>,  // Rc: !Send, !Sync
}

fn main() {
    assert_send::<AllSafe>();   // ✓
    assert_sync::<AllSafe>();   // ✓

    // assert_send::<HasRawPtr>();  // ERROR: *const i32 is not Send
    // assert_sync::<HasRawPtr>();  // ERROR: *const i32 is not Sync

    assert_send::<HasCell>();   // ✓ (Cell: Send)
    // assert_sync::<HasCell>();   // ERROR: Cell is not Sync

    // assert_send::<HasRc>();     // ERROR: Rc is not Send
    // assert_sync::<HasRc>();     // ERROR: Rc is not Sync
}
```

### Enums

La misma regla aplica a enums — todos los campos de todas las variantes deben ser Send/Sync:

```rust
enum Message {
    Text(String),        // String: Send + Sync ✓
    Data(Vec<u8>),       // Vec<u8>: Send + Sync ✓
    // Callback(*const dyn Fn()),  // si añades esto, Message pierde Send+Sync
}

fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

fn main() {
    assert_send::<Message>(); // ✓
    assert_sync::<Message>(); // ✓
}
```

---

## Negative impls: optar por no implementar

A veces quieres que un tipo **no** sea Send o Sync, incluso cuando todos sus campos lo son.

### Usando PhantomData con un tipo !Send/!Sync

```rust
use std::marker::PhantomData;

struct ThreadLocal<T> {
    value: T,
    // PhantomData<*const ()> hace que ThreadLocal sea !Send y !Sync
    // porque *const () no es ni Send ni Sync
    _not_send_sync: PhantomData<*const ()>,
}

impl<T> ThreadLocal<T> {
    fn new(value: T) -> Self {
        ThreadLocal {
            value,
            _not_send_sync: PhantomData,
        }
    }

    fn get(&self) -> &T {
        &self.value
    }
}

fn assert_send<T: Send>() {}

fn main() {
    let local = ThreadLocal::new(42);
    println!("{}", local.get()); // 42

    // assert_send::<ThreadLocal<i32>>(); // ERROR: no es Send
}
```

### Negative impls en la librería estándar (nightly)

```rust
// Así es como Rc se hace !Send y !Sync en la std (simplificado):
// impl<T> !Send for Rc<T> {}
// impl<T> !Sync for Rc<T> {}

// Negative impls no están estabilizados en stable Rust.
// Por eso usamos el truco de PhantomData<*const ()>.
```

### Optar por !Sync pero mantener Send

```rust
use std::marker::PhantomData;
use std::cell::Cell;

struct NotSync<T> {
    value: T,
    // Cell es Send pero no Sync → nuestro struct pierde Sync pero mantiene Send
    _not_sync: PhantomData<Cell<()>>,
}

fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

fn check() {
    assert_send::<NotSync<i32>>();  // ✓ (Cell es Send)
    // assert_sync::<NotSync<i32>>();  // ERROR (Cell no es Sync)
}
```

```
┌────────────────────────────────────────────────────────┐
│  PhantomData<X> hereda Send/Sync de X:                 │
│                                                        │
│  PhantomData<*const ()>  → !Send, !Sync                │
│  PhantomData<Cell<()>>   → Send, !Sync                 │
│  PhantomData<Rc<()>>     → !Send, !Sync                │
│  PhantomData<&'a T>      → Send (si T:Sync), Sync      │
└────────────────────────────────────────────────────────┘
```

---

## Definir tus propios traits unsafe

### Cuándo definir un trait como unsafe

Un trait debe ser `unsafe` cuando código que lo usa como bound puede causar UB si la implementación es incorrecta.

```rust
/// Trait para tipos que pueden ser inicializados con todos los bits en cero.
///
/// # Safety
///
/// Implementar este trait garantiza que `std::mem::zeroed::<Self>()`
/// produce un valor válido del tipo. No todos los tipos cumplen esto:
/// - `bool`: zeroed = 0x00 = false ✓
/// - `i32`:  zeroed = 0 ✓
/// - `&T`:   zeroed = null pointer ✗ (UB!)
/// - `String`: zeroed = null ptr interno ✗ (UB al dropear!)
unsafe trait ZeroValid: Sized {
    fn zeroed() -> Self {
        // SAFETY: el implementador garantiza que todos-ceros es válido
        unsafe { std::mem::zeroed() }
    }
}

// SAFETY: 0i32 es un valor válido de i32
unsafe impl ZeroValid for i32 {}

// SAFETY: 0u8 es un valor válido de u8
unsafe impl ZeroValid for u8 {}

// SAFETY: 0.0f64 es un valor válido de f64
unsafe impl ZeroValid for f64 {}

// SAFETY: false (0x00) es un valor válido de bool
unsafe impl ZeroValid for bool {}

// NO implementar para &T, String, Vec, Box, etc.

fn allocate_zeroed<T: ZeroValid>(count: usize) -> Vec<T> {
    // Safe: T: ZeroValid garantiza que zeroed() es válido
    (0..count).map(|_| T::zeroed()).collect()
}

fn main() {
    let ints: Vec<i32> = allocate_zeroed(5);
    println!("{:?}", ints); // [0, 0, 0, 0, 0]

    let bools: Vec<bool> = allocate_zeroed(3);
    println!("{:?}", bools); // [false, false, false]
}
```

### Trait unsafe con métodos que tienen precondiciones

```rust
/// Proveedor de IDs únicos. Cada llamada a `next_id()` debe retornar
/// un valor distinto, incluso bajo concurrencia.
///
/// # Safety
///
/// La implementación debe garantizar que `next_id()` NUNCA retorna
/// el mismo valor dos veces durante la vida del programa.
/// Código que depende de unicidad puede causar UB (ej: usarlo como
/// índice en memoria sin bounds check).
unsafe trait UniqueIdProvider: Send + Sync {
    fn next_id(&self) -> u64;
}

use std::sync::atomic::{AtomicU64, Ordering};

struct AtomicIdGen {
    counter: AtomicU64,
}

impl AtomicIdGen {
    fn new() -> Self {
        AtomicIdGen {
            counter: AtomicU64::new(1),
        }
    }
}

// SAFETY: fetch_add con SeqCst es atómico, garantizando unicidad.
// En la práctica, un u64 no se agotará (18 quintillones de IDs).
unsafe impl UniqueIdProvider for AtomicIdGen {
    fn next_id(&self) -> u64 {
        self.counter.fetch_add(1, Ordering::SeqCst)
    }
}

fn main() {
    let gen = AtomicIdGen::new();
    println!("id1: {}", gen.next_id()); // 1
    println!("id2: {}", gen.next_id()); // 2
    println!("id3: {}", gen.next_id()); // 3
}
```

### Trait normal vs unsafe: criterio de decisión

```
┌──────────────────────────────────────────────────────┐
│ ¿Definir mi trait como unsafe?                       │
│                                                      │
│ ¿Código que usa T: MyTrait puede causar UB           │
│  si MyTrait se implementa incorrectamente?           │
│                                                      │
│   SÍ → unsafe trait MyTrait                          │
│         Ejemplo: ZeroValid, Send, Sync,              │
│         GlobalAlloc, Allocator                       │
│                                                      │
│   NO → trait MyTrait (normal)                        │
│         Ejemplo: Clone, Debug, Iterator,             │
│         Display, From/Into                           │
│                                                      │
│ La mayoría de traits son normales. unsafe trait       │
│ es raro y casi siempre relacionado con memoria,      │
│ concurrencia o invariantes de layout.                │
└──────────────────────────────────────────────────────┘
```

---

## PhantomData y su efecto en auto-traits

`PhantomData<T>` no ocupa espacio en runtime, pero afecta al sistema de tipos incluyendo Send/Sync.

### Indicar ownership lógico

```rust
use std::marker::PhantomData;

struct Wrapper<T> {
    ptr: *mut T,
    _marker: PhantomData<T>,
    // Sin PhantomData<T>:
    //   - Wrapper no sería Send/Sync (por *mut T)
    //   - Drop checker no sabría que T se dropea
    // Con PhantomData<T>:
    //   - Send/Sync dependen de T (como si Wrapper CONTUVIERA un T)
    //   - Drop checker asume que T se dropea
}

// Si T: Send, queremos Wrapper<T>: Send
// PhantomData<T> hereda Send de T
// Pero *mut T bloquea... así que necesitamos impl manual:

// SAFETY: Wrapper tiene ownership exclusivo del T apuntado por ptr.
// Si T: Send, transferir Wrapper a otro thread es seguro.
unsafe impl<T: Send> Send for Wrapper<T> {}

// SAFETY: Wrapper solo expone acceso con &self (lectura).
// Si T: Sync, compartir &Wrapper entre threads es seguro.
unsafe impl<T: Sync> Sync for Wrapper<T> {}
```

### Variantes de PhantomData

```rust
use std::marker::PhantomData;

fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

// PhantomData<T> se comporta como si CONTUVIERA un T:
struct OwnsT<T>(PhantomData<T>);

// PhantomData<&'a T> se comporta como si tuviera un &'a T:
struct BorrowsT<'a, T>(PhantomData<&'a T>);

// PhantomData<fn() -> T>: covariante en T, siempre Send+Sync
struct ProducesT<T>(PhantomData<fn() -> T>);

// PhantomData<fn(T)>: contravariante en T, siempre Send+Sync
struct ConsumesT<T>(PhantomData<fn(T)>);

fn check() {
    // OwnsT<Rc<i32>>: !Send porque Rc: !Send
    // assert_send::<OwnsT<std::rc::Rc<i32>>>(); // ERROR

    // ProducesT: siempre Send+Sync independientemente de T
    assert_send::<ProducesT<std::rc::Rc<i32>>>(); // ✓
    assert_sync::<ProducesT<std::rc::Rc<i32>>>(); // ✓
}
```

```
┌───────────────────────────────────────────────────────────┐
│              PhantomData y variance                       │
├───────────────────────────────────────────────────────────┤
│                                                           │
│  PhantomData<T>        → covariant, Send/Sync como T     │
│  PhantomData<&'a T>    → covariant, Send si T:Sync       │
│  PhantomData<fn() → T> → covariant, siempre Send+Sync    │
│  PhantomData<fn(T)>    → contra,    siempre Send+Sync    │
│  PhantomData<*const T> → covariant, nunca Send ni Sync   │
│                                                           │
│  Usa fn() → T o fn(T) cuando NO quieres que PhantomData  │
│  afecte Send/Sync, solo la varianza.                     │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Implementar Send para un tipo con estado compartido sin sincronización

```rust
use std::cell::UnsafeCell;

struct SharedState {
    data: UnsafeCell<Vec<i32>>,
}

// INCORRECTO: UnsafeCell permite mutación sin sincronización.
// Si dos threads mutan simultáneamente → data race → UB.
// unsafe impl Send for SharedState {}
// unsafe impl Sync for SharedState {}

// CORRECTO: usar Mutex o no implementar Sync
use std::sync::Mutex;

struct SafeSharedState {
    data: Mutex<Vec<i32>>,
}
// SafeSharedState es automáticamente Send + Sync
```

### 2. Implementar Sync para un tipo con mutación interior sin atomics

```rust
struct Counter {
    count: std::cell::Cell<u64>,
}

// INCORRECTO: Cell permite mutación a través de &self,
// pero sin sincronización → data race si Sync
// unsafe impl Sync for Counter {}

// CORRECTO: dejar como !Sync (Cell es !Sync por diseño)
// o usar AtomicU64 en su lugar
```

### 3. Olvidar que unsafe impl es para siempre

```rust
struct MyType {
    safe_field: i32,
}

// SAFETY: solo contiene i32, que es Send
unsafe impl Send for MyType {}

// Meses después, alguien añade:
// struct MyType {
//     safe_field: i32,
//     handle: *mut Connection,  // ¡ahora no es safe enviar!
// }
// Pero el unsafe impl Send sigue ahí...

// SOLUCIÓN: usar auto-derivación siempre que sea posible.
// Solo añadir unsafe impl cuando el auto-trait no aplica
// y documentar QUÉ campos lo requieren.
```

### 4. Confundir Send y Sync

```rust
use std::cell::RefCell;

// RefCell<T>: Send (si T: Send), pero !Sync
// Puedes MOVER un RefCell a otro thread,
// pero no puedes compartir &RefCell entre threads.

fn this_works() {
    let cell = RefCell::new(42);
    std::thread::spawn(move || {
        // OK: movemos el RefCell, no lo compartimos
        *cell.borrow_mut() = 100;
    });
}

fn this_fails() {
    let cell = RefCell::new(42);
    let cell_ref = &cell;
    // ERROR: RefCell no es Sync, no puedes enviar &RefCell
    // std::thread::spawn(move || {
    //     println!("{}", cell_ref.borrow());
    // });
}
```

### 5. Implementar unsafe trait sin documentar SAFETY

```rust
struct Buffer {
    ptr: *mut u8,
    len: usize,
}

// INCORRECTO: sin justificación
// unsafe impl Send for Buffer {}

// CORRECTO: documentar por qué es safe
// SAFETY: Buffer tiene ownership exclusivo de la memoria apuntada por ptr.
// ptr no es compartido con ningún otro Buffer o estructura.
// Transferir el Buffer a otro thread transfiere toda la ownership.
// La memoria es válida hasta que Buffer se dropee.
unsafe impl Send for Buffer {}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║            TRAITS UNSAFE — REFERENCIA RÁPIDA             ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  DEFINICIÓN                                               ║
║  ──────────                                               ║
║  unsafe trait Foo { ... }                                 ║
║  → implementar requiere unsafe impl                      ║
║  → USAR (como bound) es safe                             ║
║                                                           ║
║  SEND                                                     ║
║  ────                                                     ║
║  T: Send  → safe mover T a otro thread                   ║
║  Auto-trait: si todos los campos: Send → T: Send          ║
║  impl manual: unsafe impl Send for T {}                   ║
║                                                           ║
║  SYNC                                                     ║
║  ────                                                     ║
║  T: Sync  → safe compartir &T entre threads              ║
║  Equivale a: &T: Send                                     ║
║  Auto-trait: si todos los campos: Sync → T: Sync          ║
║  impl manual: unsafe impl Sync for T {}                   ║
║                                                           ║
║  TIPOS COMUNES                                            ║
║  ─────────────                                            ║
║  Send+Sync: i32, String, Vec, Box, Arc, Mutex             ║
║  Send only: Cell, RefCell                                 ║
║  Neither:   Rc, *const T, *mut T                          ║
║  Sync only: MutexGuard (Send: no, Sync: si T:Sync)        ║
║                                                           ║
║  HACER UN TIPO !Send / !Sync                              ║
║  ───────────────────────────                               ║
║  PhantomData<*const ()>     → !Send, !Sync                ║
║  PhantomData<Cell<()>>      → Send,  !Sync                ║
║  PhantomData<Rc<()>>        → !Send, !Sync                ║
║                                                           ║
║  DEFINIR TU PROPIO TRAIT UNSAFE                           ║
║  ──────────────────────────────                           ║
║  ¿Impl incorrecta causa UB? → unsafe trait                ║
║  ¿Impl incorrecta es solo bug? → trait normal             ║
║                                                           ║
║  CHECKLIST ANTES DE unsafe impl Send/Sync                 ║
║  ────────────────────────────────────────                  ║
║  □ ¿Ownership exclusivo del dato?                         ║
║  □ ¿Sin mutación interior sin sincronización?             ║
║  □ ¿Sin afinidad de thread (GPU, TLS)?                    ║
║  □ ¿Documentado con // SAFETY?                           ║
║  □ ¿Qué pasa si alguien añade un campo después?          ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Wrapper Send+Sync para puntero FFI

Crea un wrapper `FfiHandle` que encapsule un puntero opaco de una "biblioteca C" simulada y que sea `Send + Sync`:

```rust
mod ffi_sim {
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::ffi::c_void;

    static NEXT_ID: AtomicU64 = AtomicU64::new(1);

    pub unsafe fn create_resource() -> *mut c_void {
        let id = NEXT_ID.fetch_add(1, Ordering::SeqCst);
        Box::into_raw(Box::new(id)) as *mut c_void
    }

    pub unsafe fn get_resource_id(handle: *mut c_void) -> u64 {
        *(handle as *const u64)
    }

    pub unsafe fn destroy_resource(handle: *mut c_void) {
        drop(Box::from_raw(handle as *mut u64));
    }
}

// Tu implementación:
struct FfiHandle { /* ... */ }

impl FfiHandle {
    fn new() -> Self { todo!() }
    fn id(&self) -> u64 { todo!() }
}

// unsafe impl Send for FfiHandle {}
// unsafe impl Sync for FfiHandle {}
// impl Drop for FfiHandle { ... }
```

Verifica que funciona con `Arc<FfiHandle>` compartido entre threads. Documenta cada `// SAFETY`.

**Pregunta de reflexión**: ¿Bajo qué condiciones sería incorrecto implementar `Sync` para `FfiHandle`? ¿Qué pasaría si `get_resource_id` de la biblioteca C real no fuera thread-safe?

---

### Ejercicio 2: Trait unsafe `ContiguousMemory`

Define un trait `unsafe trait ContiguousMemory` para tipos que se pueden representar de forma segura como un slice de bytes (como los tipos POD en C):

```rust
/// # Safety
///
/// El tipo debe:
/// 1. No contener padding con valores indeterminados
/// 2. Ser válido para cualquier patrón de bits
/// 3. No contener punteros ni referencias
/// 4. Implementar Copy
unsafe trait ContiguousMemory: Copy + Sized {
    fn as_bytes(&self) -> &[u8] { todo!() }
    fn from_bytes(bytes: &[u8]) -> Option<Self> { todo!() }
}

// Implementa para: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64
// Implementa para: [T; N] where T: ContiguousMemory (elige un N fijo)
// NO implementes para: bool, char, &T, String
```

**Pistas**:
- `as_bytes`: usa `std::slice::from_raw_parts(self as *const Self as *const u8, size_of::<Self>())`.
- `from_bytes`: verifica que `bytes.len() == size_of::<Self>()`, luego `ptr::read(bytes.as_ptr() as *const Self)`.
- Ojo con alineación en `from_bytes`: usa `ptr::read_unaligned`.

**Pregunta de reflexión**: ¿Por qué `bool` no debe implementar `ContiguousMemory`? ¿Qué valor de `bool` sería UB si lo crearas desde bytes arbitrarios?

---

### Ejercicio 3: Thread-safe event bus

Implementa un bus de eventos que use traits unsafe para garantizar que los handlers son thread-safe:

```rust
use std::sync::{Arc, Mutex};
use std::any::{Any, TypeId};
use std::collections::HashMap;

/// # Safety
///
/// El tipo de evento debe ser safe para enviar entre threads
/// y compartir referencias entre threads.
unsafe trait Event: Send + Sync + 'static {}

type Handler = Box<dyn Fn(&dyn Any) + Send + Sync>;

struct EventBus {
    handlers: Mutex<HashMap<TypeId, Vec<Handler>>>,
}

impl EventBus {
    fn new() -> Self { todo!() }

    /// Registra un handler para eventos de tipo E
    fn subscribe<E: Event>(&self, handler: impl Fn(&E) + Send + Sync + 'static) {
        todo!()
    }

    /// Emite un evento a todos los handlers registrados
    fn emit<E: Event>(&self, event: &E) {
        todo!()
    }
}
```

Crea al menos dos tipos de eventos y demuestra el bus funcionando con múltiples threads.

**Pistas**:
- `subscribe`: envuelve el handler en un closure que hace downcast: `Box::new(move |any: &dyn Any| { if let Some(e) = any.downcast_ref::<E>() { handler(e) } })`.
- `emit`: busca handlers por `TypeId::of::<E>()`, itera y llama.
- Usa `Arc<EventBus>` para compartir entre threads.

**Pregunta de reflexión**: ¿Es realmente necesario que `Event` sea un `unsafe trait`? ¿Qué pasa si lo cambias a un `trait` normal con bounds `Send + Sync + 'static`? ¿Bajo qué circunstancias la diferencia importaría?
