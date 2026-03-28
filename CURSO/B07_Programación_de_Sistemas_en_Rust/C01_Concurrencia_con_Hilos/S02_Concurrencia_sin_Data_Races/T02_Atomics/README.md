# Atomics

## Indice
1. [Que son las operaciones atomicas](#1-que-son-las-operaciones-atomicas)
2. [Tipos atomicos en std](#2-tipos-atomicos-en-std)
3. [Operaciones basicas: load, store, swap](#3-operaciones-basicas-load-store-swap)
4. [Operaciones read-modify-write](#4-operaciones-read-modify-write)
5. [Ordering: el modelo de memoria](#5-ordering-el-modelo-de-memoria)
6. [Relaxed](#6-relaxed)
7. [Acquire y Release](#7-acquire-y-release)
8. [SeqCst](#8-seqcst)
9. [Patrones practicos con atomics](#9-patrones-practicos-con-atomics)
10. [Atomics vs Mutex](#10-atomics-vs-mutex)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Que son las operaciones atomicas

En el topico anterior vimos que `Rc` no es `Send` porque su
contador usa operaciones normales de lectura-escritura. El
problema fundamental es que `read + modify + write` como tres
pasos separados no es seguro entre hilos:

```
    Operacion NO atomica (3 pasos):
    Hilo A                          Hilo B
    ------                          ------
    lee  count = 5                  lee  count = 5
    suma count = 6                  suma count = 6
    escribe count = 6               escribe count = 6
                                    (deberia ser 7!)

    Operacion ATOMICA (1 paso indivisible):
    Hilo A                          Hilo B
    ------                          ------
    fetch_add(1) = 5                (bloqueado)
                                    fetch_add(1) = 6
    Resultado: count = 7  (correcto)
```

Una **operacion atomica** es indivisible: ningun otro hilo puede
ver un estado intermedio. El hardware del CPU proporciona
instrucciones especiales para esto (como `LOCK XADD` en x86 o
`LDXR/STXR` en ARM).

### Comparacion con C

En C, usarias `_Atomic` (C11) o `__atomic_*` builtins de GCC:

```c
// C11
#include <stdatomic.h>
_Atomic int counter = 0;
atomic_fetch_add(&counter, 1);

// Equivalente en Rust:
// use std::sync::atomic::{AtomicI32, Ordering};
// let counter = AtomicI32::new(0);
// counter.fetch_add(1, Ordering::SeqCst);
```

La diferencia: en Rust, el `Ordering` es un parametro obligatorio
de cada operacion — no puedes olvidarlo.

---

## 2. Tipos atomicos en std

Todos viven en `std::sync::atomic`:

```rust
use std::sync::atomic::{
    AtomicBool,
    AtomicI8, AtomicI16, AtomicI32, AtomicI64, AtomicIsize,
    AtomicU8, AtomicU16, AtomicU32, AtomicU64, AtomicUsize,
    AtomicPtr,
    Ordering,
};
```

### Tabla de tipos

```
+---------------+-------------------+----------------------------+
| Tipo atomico  | Tipo subyacente   | Uso tipico                 |
+---------------+-------------------+----------------------------+
| AtomicBool    | bool              | Flags, senales             |
| AtomicUsize   | usize             | Contadores, indices        |
| AtomicIsize   | isize             | Contadores con signo       |
| AtomicU8..U64 | u8..u64           | Contadores de tamano fijo  |
| AtomicI8..I64 | i8..i64           | Contadores con signo fijo  |
| AtomicPtr<T>  | *mut T            | Punteros lock-free         |
+---------------+-------------------+----------------------------+
```

> **Nota**: no existe `AtomicF32` ni `AtomicF64` en std. Para
> floats atomicos, puedes usar transmute a `AtomicU32`/`AtomicU64`
> o el crate `atomic_float`.

### Propiedades importantes

- Todos son `Send + Sync` (ese es su proposito).
- Todos usan mutabilidad interior: las operaciones toman `&self`,
  no `&mut self`.
- Tienen el mismo tamano que su tipo subyacente.
- `AtomicUsize::new()` es `const` — puedes usarlo en `static`.

```rust
use std::sync::atomic::{AtomicUsize, Ordering};

// Global static — sin necesidad de lazy_static o OnceCell
static REQUEST_COUNT: AtomicUsize = AtomicUsize::new(0);

fn handle_request() {
    REQUEST_COUNT.fetch_add(1, Ordering::Relaxed);
}

fn get_count() -> usize {
    REQUEST_COUNT.load(Ordering::Relaxed)
}
```

---

## 3. Operaciones basicas: load, store, swap

### load: leer el valor actual

```rust
use std::sync::atomic::{AtomicI32, Ordering};

let counter = AtomicI32::new(42);
let value = counter.load(Ordering::SeqCst);
assert_eq!(value, 42);
```

### store: escribir un nuevo valor

```rust
use std::sync::atomic::{AtomicBool, Ordering};

let flag = AtomicBool::new(false);
flag.store(true, Ordering::SeqCst);
assert_eq!(flag.load(Ordering::SeqCst), true);
```

### swap: reemplazar y obtener el valor anterior

```rust
use std::sync::atomic::{AtomicI32, Ordering};

let counter = AtomicI32::new(10);
let old = counter.swap(20, Ordering::SeqCst);
assert_eq!(old, 10);
assert_eq!(counter.load(Ordering::SeqCst), 20);
```

### Diagrama de operaciones basicas

```
    load:          store:          swap:
    +-----+        +-----+        +-----+
    | 42  | ---->  | 42  |        | 10  | ----> old = 10
    +-----+  lee   +-----+        +-----+
                     |              |
                     v              v
                   +-----+        +-----+
                   | 99  |        | 20  |
                   +-----+        +-----+
                   escribe        reemplaza
```

---

## 4. Operaciones read-modify-write

Estas operaciones leen, modifican y escriben en un solo paso
atomico. Son la razon principal por la que existen los atomics.

### fetch_add / fetch_sub

```rust
use std::sync::atomic::{AtomicUsize, Ordering};

let counter = AtomicUsize::new(10);

let old = counter.fetch_add(5, Ordering::SeqCst);
assert_eq!(old, 10);              // retorna valor ANTERIOR
assert_eq!(counter.load(Ordering::SeqCst), 15);

let old = counter.fetch_sub(3, Ordering::SeqCst);
assert_eq!(old, 15);
assert_eq!(counter.load(Ordering::SeqCst), 12);
```

### fetch_and / fetch_or / fetch_xor

Operaciones bitwise atomicas:

```rust
use std::sync::atomic::{AtomicU8, Ordering};

let flags = AtomicU8::new(0b1100);

flags.fetch_or(0b0011, Ordering::SeqCst);   // set bits
assert_eq!(flags.load(Ordering::SeqCst), 0b1111);

flags.fetch_and(0b1010, Ordering::SeqCst);  // clear bits
assert_eq!(flags.load(Ordering::SeqCst), 0b1010);

flags.fetch_xor(0b1111, Ordering::SeqCst);  // toggle bits
assert_eq!(flags.load(Ordering::SeqCst), 0b0101);
```

### compare_exchange (CAS)

La operacion mas poderosa: **Compare-And-Swap**. Cambia el valor
solo si es igual al esperado:

```rust
use std::sync::atomic::{AtomicI32, Ordering};

let value = AtomicI32::new(5);

// Intenta cambiar 5 -> 10
match value.compare_exchange(5, 10, Ordering::SeqCst, Ordering::SeqCst) {
    Ok(old) => println!("Changed {} -> 10", old),   // old = 5
    Err(actual) => println!("Expected 5, found {}", actual),
}

// Intenta cambiar 5 -> 20 (fallara, ahora es 10)
match value.compare_exchange(5, 20, Ordering::SeqCst, Ordering::SeqCst) {
    Ok(old) => println!("Changed {} -> 20", old),
    Err(actual) => println!("Expected 5, found {}", actual), // actual = 10
}
```

```
    compare_exchange(expected, new):

    Si current == expected:         Si current != expected:
    +-----+         +-----+        +-----+         +-----+
    |  5  | ------> | 10  |        |  7  | ------> |  7  |
    +-----+  CAS    +-----+        +-----+  CAS    +-----+
    Ok(5)    exito                  Err(7)   fallo
```

### compare_exchange_weak

Igual que `compare_exchange`, pero puede fallar **espuriamente**
en algunas arquitecturas (ARM). Es mas eficiente en loops:

```rust
use std::sync::atomic::{AtomicUsize, Ordering};

let counter = AtomicUsize::new(0);

// Patron CAS loop: incrementar hasta un maximo
loop {
    let current = counter.load(Ordering::Relaxed);
    if current >= 100 {
        break;
    }
    // compare_exchange_weak puede fallar espuriamente,
    // pero el loop reintenta automaticamente
    match counter.compare_exchange_weak(
        current,
        current + 1,
        Ordering::SeqCst,
        Ordering::Relaxed,
    ) {
        Ok(_) => break,
        Err(_) => continue,  // otro hilo modifico el valor, reintentar
    }
}
```

---

## 5. Ordering: el modelo de memoria

El parametro `Ordering` controla que garantias de **visibilidad**
y **reordenamiento** ofrece la operacion. Es el concepto mas
dificil de los atomics.

### Por que existe Ordering

El hardware moderno reordena instrucciones para optimizar:

```
    Lo que escribes:             Lo que el CPU puede ejecutar:
    1. store(data, 42)           2. store(ready, true)
    2. store(ready, true)        1. store(data, 42)

    Si otro hilo lee ready == true ANTES de que data sea 42,
    obtendra basura.
```

El compilador tambien reordena instrucciones. `Ordering` le dice
tanto al compilador como al CPU que reordenamientos estan prohibidos.

### Los cuatro niveles

```
    Restrictividad creciente:

    Relaxed  ---  Solo atomicidad, sin orden
       |
    Acquire  ---  "Barrera de lectura": lo que leo aqui
       |          es visible despues de esta operacion
    Release  ---  "Barrera de escritura": todo lo que escribi
       |          antes es visible para quien haga Acquire
    SeqCst   ---  Orden total global, como si fuera secuencial
```

---

## 6. Relaxed

`Ordering::Relaxed` garantiza solo **atomicidad**: la operacion
es indivisible, pero no hay garantias de orden respecto a otras
operaciones.

### Cuando es seguro

Usa Relaxed cuando el valor atomico es **independiente** de
otros datos. El caso clasico: un contador de estadisticas.

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::thread;

static REQUESTS: AtomicU64 = AtomicU64::new(0);
static ERRORS: AtomicU64 = AtomicU64::new(0);

fn handle_request() {
    REQUESTS.fetch_add(1, Ordering::Relaxed);
    // ... procesar ...
    if false /* error condition */ {
        ERRORS.fetch_add(1, Ordering::Relaxed);
    }
}

fn print_stats() {
    // Los valores pueden estar "atrasados" respecto al
    // momento exacto, pero cada uno es correcto en si mismo
    println!(
        "requests: {}, errors: {}",
        REQUESTS.load(Ordering::Relaxed),
        ERRORS.load(Ordering::Relaxed),
    );
}
```

### Cuando NO es seguro

```rust
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

static DATA: AtomicI32 = AtomicI32::new(0);
static READY: AtomicBool = AtomicBool::new(false);

// Hilo productor
fn producer() {
    DATA.store(42, Ordering::Relaxed);
    READY.store(true, Ordering::Relaxed);  // PELIGROSO
}

// Hilo consumidor
fn consumer() {
    while !READY.load(Ordering::Relaxed) {}  // PELIGROSO
    let value = DATA.load(Ordering::Relaxed);
    // value PODRIA ser 0, no 42!
    // El CPU/compilador puede reordenar los stores en producer()
}
```

Este ejemplo necesita Acquire/Release (ver seccion siguiente).

---

## 7. Acquire y Release

`Acquire` y `Release` forman una **pareja**: un store con
`Release` en un hilo se sincroniza con un load con `Acquire`
en otro hilo. Juntos garantizan que las escrituras previas
al Release son visibles despues del Acquire.

```
    Hilo A (productor):              Hilo B (consumidor):
    -----------------------          -----------------------
    data = 42;                       while !ready.load(Acquire) {}
    other_data = "hello";            // GARANTIA: ve data = 42
    ready.store(true, Release);      //           ve other_data = "hello"
         |                                  ^
         +---- sincronizacion ------>-------+
         "todo lo de arriba es               "todo lo de abajo ve
          visible al otro lado"               lo que se escribio arriba"
```

### El ejemplo corregido

```rust
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

static DATA: AtomicI32 = AtomicI32::new(0);
static READY: AtomicBool = AtomicBool::new(false);

fn producer() {
    DATA.store(42, Ordering::Relaxed);      // escritura antes del Release
    READY.store(true, Ordering::Release);   // Release: publica todo lo anterior
}

fn consumer() {
    while !READY.load(Ordering::Acquire) {} // Acquire: ve todo lo del Release
    let value = DATA.load(Ordering::Relaxed);
    assert_eq!(value, 42);  // GARANTIZADO
}
```

### Reglas de Acquire y Release

```
+------------------+---------------------------------------------+
| Ordering         | Usado en    | Garantia                      |
+------------------+-------------+-------------------------------+
| Release          | store/swap  | Escrituras previas visibles   |
|                  |             | para quien haga Acquire       |
+------------------+-------------+-------------------------------+
| Acquire          | load        | Ve todas las escrituras       |
|                  |             | previas al Release pareado    |
+------------------+-------------+-------------------------------+
| AcqRel           | read-modify | Acquire + Release combinados  |
|                  | -write ops  | (fetch_add, compare_exchange) |
+------------------+-------------+-------------------------------+
```

### Ejemplo: spinlock simple

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::cell::UnsafeCell;

struct SpinLock<T> {
    locked: AtomicBool,
    data: UnsafeCell<T>,
}

unsafe impl<T: Send> Sync for SpinLock<T> {}
unsafe impl<T: Send> Send for SpinLock<T> {}

impl<T> SpinLock<T> {
    fn new(value: T) -> Self {
        SpinLock {
            locked: AtomicBool::new(false),
            data: UnsafeCell::new(value),
        }
    }

    fn lock(&self) -> &mut T {
        // Acquire: al adquirir el lock, vemos las escrituras
        // del hilo que lo tenia antes (su Release al unlock)
        while self.locked.compare_exchange_weak(
            false,
            true,
            Ordering::Acquire,
            Ordering::Relaxed,
        ).is_err() {
            // spin: en produccion usarias std::hint::spin_loop()
            std::hint::spin_loop();
        }
        unsafe { &mut *self.data.get() }
    }

    fn unlock(&self) {
        // Release: al soltar el lock, publicamos nuestras escrituras
        // para que el proximo Acquire las vea
        self.locked.store(false, Ordering::Release);
    }
}
```

El `Acquire` al tomar el lock ve todas las escrituras que el
hilo anterior hizo antes de su `Release` al soltar el lock.
Asi, los datos protegidos siempre estan en un estado consistente.

---

## 8. SeqCst

`Ordering::SeqCst` (Sequentially Consistent) es el mas fuerte:
todas las operaciones `SeqCst` en todos los hilos se ven en un
**unico orden total global**, como si se ejecutaran una por una.

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;

static A: AtomicBool = AtomicBool::new(false);
static B: AtomicBool = AtomicBool::new(false);

fn main() {
    let t1 = thread::spawn(|| {
        A.store(true, Ordering::SeqCst);
    });

    let t2 = thread::spawn(|| {
        B.store(true, Ordering::SeqCst);
    });

    let t3 = thread::spawn(|| {
        // Con SeqCst, si vemos B == true, entonces A TAMBIEN
        // debe ser true SI t1 completo antes que t2 en el
        // orden total global.
        while !B.load(Ordering::SeqCst) {}
        // Con Acquire/Release esto NO estaria garantizado
        // porque no hay relacion directa entre A y B.
    });

    t1.join().unwrap();
    t2.join().unwrap();
    t3.join().unwrap();
}
```

### Cuando usar SeqCst

- Cuando necesitas un orden global entre **atomics independientes**
  (no relacionados por Acquire/Release).
- Cuando no estas seguro del ordering correcto — SeqCst es siempre
  correcto (pero puede ser mas lento).

### Coste de SeqCst

```
    Velocidad relativa (x86_64):
    +----------+--------------------------------+
    | Ordering | Coste                          |
    +----------+--------------------------------+
    | Relaxed  | Casi gratis (barrera solo      |
    |          | para el compilador)            |
    +----------+--------------------------------+
    | Acquire  | Load normal (x86 lo da gratis) |
    +----------+--------------------------------+
    | Release  | Store normal (x86 lo da gratis)|
    +----------+--------------------------------+
    | SeqCst   | Instruccion MFENCE o LOCK      |
    |          | (mas costoso en x86)           |
    +----------+--------------------------------+

    En ARM/AArch64 las diferencias son mayores:
    Acquire/Release requieren instrucciones de barrera
    que Relaxed no necesita.
```

> **Regla practica**: en x86, la diferencia entre orderings es
> minima. En ARM (moviles, Apple Silicon, Raspberry Pi), Relaxed
> puede ser significativamente mas rapido. Pero la **correccion**
> siempre es mas importante que el rendimiento — empieza con
> SeqCst y optimiza solo con benchmarks.

---

## 9. Patrones practicos con atomics

### Patron 1: flag de shutdown

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

fn main() {
    let running = Arc::new(AtomicBool::new(true));

    let running_clone = Arc::clone(&running);
    let worker = thread::spawn(move || {
        let mut count = 0;
        while running_clone.load(Ordering::Relaxed) {
            // trabajo...
            count += 1;
            thread::sleep(Duration::from_millis(10));
        }
        println!("Worker did {} iterations", count);
    });

    thread::sleep(Duration::from_millis(100));
    running.store(false, Ordering::Relaxed);  // senal de parada
    worker.join().unwrap();
}
```

`Relaxed` es suficiente aqui: solo nos importa que el flag
cambie eventualmente, no el orden relativo con otros datos.

### Patron 2: contador de estadisticas

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::thread;

struct Stats {
    requests: AtomicU64,
    bytes_sent: AtomicU64,
    errors: AtomicU64,
}

impl Stats {
    fn new() -> Self {
        Stats {
            requests: AtomicU64::new(0),
            bytes_sent: AtomicU64::new(0),
            errors: AtomicU64::new(0),
        }
    }

    fn record_request(&self, bytes: u64) {
        self.requests.fetch_add(1, Ordering::Relaxed);
        self.bytes_sent.fetch_add(bytes, Ordering::Relaxed);
    }

    fn record_error(&self) {
        self.errors.fetch_add(1, Ordering::Relaxed);
    }

    fn snapshot(&self) -> (u64, u64, u64) {
        // Nota: estos tres loads no son atomicos ENTRE SI.
        // Para una snapshot consistente necesitarias un Mutex.
        (
            self.requests.load(Ordering::Relaxed),
            self.bytes_sent.load(Ordering::Relaxed),
            self.errors.load(Ordering::Relaxed),
        )
    }
}

fn main() {
    let stats = Arc::new(Stats::new());

    let mut handles = vec![];
    for _ in 0..4 {
        let s = Arc::clone(&stats);
        handles.push(thread::spawn(move || {
            for i in 0..1000 {
                s.record_request(256);
                if i % 100 == 0 {
                    s.record_error();
                }
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    let (req, bytes, err) = stats.snapshot();
    println!("requests: {}, bytes: {}, errors: {}", req, bytes, err);
    // requests: 4000, bytes: 1024000, errors: 40
}
```

### Patron 3: generador de IDs unico

```rust
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT_ID: AtomicU64 = AtomicU64::new(1);

fn generate_id() -> u64 {
    NEXT_ID.fetch_add(1, Ordering::Relaxed)
}

fn main() {
    // Cada llamada retorna un ID unico, incluso desde multiples hilos
    let id1 = generate_id();  // 1
    let id2 = generate_id();  // 2
    let id3 = generate_id();  // 3
    println!("IDs: {}, {}, {}", id1, id2, id3);
}
```

### Patron 4: one-time initialization flag

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Once;

// Opcion 1: con AtomicBool (simple, pero con limitaciones)
static INITIALIZED: AtomicBool = AtomicBool::new(false);

fn init_if_needed() {
    // compare_exchange: solo un hilo "gana" y hace la init
    if INITIALIZED.compare_exchange(
        false,
        true,
        Ordering::AcqRel,
        Ordering::Relaxed,
    ).is_ok() {
        // Solo este hilo ejecuta la inicializacion
        println!("Initializing...");
    }
}

// Opcion 2: std::sync::Once (preferida — maneja panics correctamente)
static INIT: Once = Once::new();

fn init_with_once() {
    INIT.call_once(|| {
        println!("Initializing (only once)...");
    });
}
```

---

## 10. Atomics vs Mutex

### Cuando usar cada uno

```
    Pregunta: "Que necesito proteger?"

    +-- Un solo valor numerico/bool/puntero?
    |     +-- No tiene relacion con otros datos? --> Atomic + Relaxed
    |     +-- Controla acceso a otros datos? -----> Atomic + Acquire/Release
    |                                               (o mejor usa Mutex)
    |
    +-- Multiples valores que deben cambiar juntos?
    |     +--> Mutex o RwLock
    |
    +-- Una estructura de datos compleja?
          +--> Mutex o RwLock
```

### Comparacion detallada

```
+-------------------+---------------------------+---------------------------+
| Aspecto           | Atomic                    | Mutex                     |
+-------------------+---------------------------+---------------------------+
| Tamano            | 1 valor simple            | Cualquier tipo T          |
| Overhead          | Muy bajo (instruccion HW) | Lock/unlock (syscall      |
|                   |                           | posible si hay contention)|
| Bloqueo           | No bloquea (lock-free)    | Bloquea el hilo           |
| Deadlock          | Imposible                 | Posible                   |
| Poisoning         | No aplica                 | Si (panic con lock held)  |
| Consistencia      | Solo 1 valor a la vez     | Multiples valores juntos  |
| Complejidad       | Ordering es sutil         | Sencillo de usar          |
| Starvation        | Posible (CAS loop)        | Fair con SO moderno       |
+-------------------+---------------------------+---------------------------+
```

### Ejemplo: counter con ambos

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

fn bench_atomic() -> u64 {
    let counter = Arc::new(AtomicU64::new(0));
    let mut handles = vec![];

    for _ in 0..4 {
        let c = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..1_000_000 {
                c.fetch_add(1, Ordering::Relaxed);
            }
        }));
    }

    for h in handles { h.join().unwrap(); }
    counter.load(Ordering::Relaxed)
}

fn bench_mutex() -> u64 {
    let counter = Arc::new(Mutex::new(0u64));
    let mut handles = vec![];

    for _ in 0..4 {
        let c = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..1_000_000 {
                *c.lock().unwrap() += 1;
            }
        }));
    }

    for h in handles { h.join().unwrap(); }
    *counter.lock().unwrap()
}

fn main() {
    let start = Instant::now();
    let result = bench_atomic();
    let atomic_time = start.elapsed();

    let start = Instant::now();
    let result2 = bench_mutex();
    let mutex_time = start.elapsed();

    println!("Atomic: {} in {:?}", result, atomic_time);
    println!("Mutex:  {} in {:?}", result2, mutex_time);
    // Tipicamente: Atomic es 2-10x mas rapido para esta operacion
}
```

> **Nota**: la diferencia de rendimiento depende de la contention
> (cuantos hilos compiten). Con baja contention, Mutex es casi
> tan rapido. Con alta contention en un solo valor, Atomic gana
> claramente.

---

## 11. Errores comunes

### Error 1: usar Relaxed cuando necesitas Acquire/Release

```rust
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

static DATA: AtomicI32 = AtomicI32::new(0);
static READY: AtomicBool = AtomicBool::new(false);

// MAL: Relaxed no garantiza orden entre DATA y READY
fn producer_bad() {
    DATA.store(42, Ordering::Relaxed);
    READY.store(true, Ordering::Relaxed);
    // El CPU puede reordenar: READY antes que DATA
}

fn consumer_bad() {
    while !READY.load(Ordering::Relaxed) {}
    let v = DATA.load(Ordering::Relaxed);
    // v PUEDE ser 0 en ARM!
}
```

```rust
// BIEN: Release en producer, Acquire en consumer
fn producer_good() {
    DATA.store(42, Ordering::Relaxed);
    READY.store(true, Ordering::Release);  // barrera
}

fn consumer_good() {
    while !READY.load(Ordering::Acquire) {}  // barrera
    let v = DATA.load(Ordering::Relaxed);
    assert_eq!(v, 42);  // GARANTIZADO
}
```

**Por que falla**: `Relaxed` solo garantiza que la operacion atomica
individual es indivisible. No dice nada sobre el orden en que otros
hilos ven las escrituras a **diferentes** variables.

### Error 2: asumir que varios loads atomicos son consistentes

```rust
use std::sync::atomic::{AtomicU64, Ordering};

static NUMERATOR: AtomicU64 = AtomicU64::new(0);
static DENOMINATOR: AtomicU64 = AtomicU64::new(1);

// MAL: ratio puede ser inconsistente
fn get_ratio() -> f64 {
    let n = NUMERATOR.load(Ordering::Relaxed);
    // -- otro hilo puede modificar ambos aqui --
    let d = DENOMINATOR.load(Ordering::Relaxed);
    n as f64 / d as f64
}
```

```rust
// BIEN: usar Mutex para snapshot consistente
use std::sync::Mutex;

struct Ratio {
    numerator: u64,
    denominator: u64,
}

static RATIO: Mutex<Ratio> = Mutex::new(Ratio {
    numerator: 0,
    denominator: 1,
});

fn get_ratio() -> f64 {
    let r = RATIO.lock().unwrap();
    r.numerator as f64 / r.denominator as f64
}
```

**Regla**: si dos valores deben leerse como una unidad consistente,
un solo Atomic no es suficiente — usa Mutex.

### Error 3: CAS loop sin backoff

```rust
use std::sync::atomic::{AtomicUsize, Ordering};

static COUNTER: AtomicUsize = AtomicUsize::new(0);

// MAL: spin agresivo quema CPU
fn increment_bad() {
    loop {
        let current = COUNTER.load(Ordering::Relaxed);
        if COUNTER.compare_exchange(
            current, current + 1,
            Ordering::SeqCst, Ordering::Relaxed
        ).is_ok() {
            break;
        }
        // Sin yield ni spin_loop hint — quema CPU
    }
}
```

```rust
// BIEN: fetch_add es mas simple y eficiente para incrementar
fn increment_good() {
    COUNTER.fetch_add(1, Ordering::Relaxed);
}

// Si realmente necesitas CAS loop, usa spin_loop hint:
fn increment_cas() {
    loop {
        let current = COUNTER.load(Ordering::Relaxed);
        if COUNTER.compare_exchange_weak(
            current, current + 1,
            Ordering::Relaxed, Ordering::Relaxed
        ).is_ok() {
            break;
        }
        std::hint::spin_loop();  // hint al CPU para ahorrar energia
    }
}
```

**Regla**: si existe una operacion `fetch_*` para tu caso, usala.
CAS loops son para logica condicional que `fetch_*` no puede
expresar.

### Error 4: olvidar que AtomicXxx no es Copy

```rust
use std::sync::atomic::AtomicU32;

// MAL: AtomicU32 no implementa Copy ni Clone
fn bad() {
    let a = AtomicU32::new(0);
    let b = a;  // MOVE, no copy
    // a ya no es valido
    // println!("{}", a.load(Ordering::Relaxed)); // ERROR
}
```

```rust
// BIEN: si necesitas compartir, usa Arc o referencia
use std::sync::Arc;
use std::sync::atomic::{AtomicU32, Ordering};

fn good() {
    let a = Arc::new(AtomicU32::new(0));
    let b = Arc::clone(&a);  // ambos apuntan al mismo AtomicU32
    b.store(42, Ordering::Relaxed);
    println!("{}", a.load(Ordering::Relaxed));  // 42
}
```

**Por que**: si `AtomicU32` fuera `Copy`, tendrias dos copias
independientes del valor atomico — las modificaciones a una no
afectarian a la otra, que es exactamente lo contrario de lo que
quieres.

### Error 5: usar atomics para proteger datos no atomicos

```rust
use std::sync::atomic::{AtomicBool, Ordering};

static LOCK: AtomicBool = AtomicBool::new(false);
static mut SHARED_DATA: Vec<i32> = Vec::new();  // static mut!

// MAL: atomics no protegen magicamente static mut
fn bad_lock() {
    while LOCK.compare_exchange(false, true,
        Ordering::Acquire, Ordering::Relaxed).is_err() {
        std::hint::spin_loop();
    }

    unsafe {
        SHARED_DATA.push(42);  // UB potencial: static mut es inherentemente unsafe
    }

    LOCK.store(false, Ordering::Release);
}
```

```rust
// BIEN: usar Mutex que encapsula los datos
use std::sync::Mutex;

static SHARED_DATA: Mutex<Vec<i32>> = Mutex::new(Vec::new());

fn good_lock() {
    let mut data = SHARED_DATA.lock().unwrap();
    data.push(42);  // seguro: Mutex garantiza acceso exclusivo
}
```

**Regla**: `Mutex<T>` es casi siempre mejor que "atomic flag +
`static mut`". El `Mutex` **encapsula** los datos protegidos,
haciendo imposible accederlos sin el lock.

---

## 12. Cheatsheet

```
ATOMICS - REFERENCIA RAPIDA
============================

Tipos:
  AtomicBool, AtomicUsize, AtomicIsize
  AtomicU8..U64, AtomicI8..I64, AtomicPtr<T>
  Todos en std::sync::atomic

Operaciones:
  load(Ordering)              -> T          Lee el valor
  store(val, Ordering)                      Escribe un valor
  swap(val, Ordering)         -> T          Reemplaza, retorna anterior
  fetch_add(val, Ordering)    -> T          Suma, retorna anterior
  fetch_sub(val, Ordering)    -> T          Resta, retorna anterior
  fetch_and(val, Ordering)    -> T          AND, retorna anterior
  fetch_or(val, Ordering)     -> T          OR, retorna anterior
  fetch_xor(val, Ordering)    -> T          XOR, retorna anterior
  compare_exchange(old, new,
    success_ord, fail_ord)    -> Result     CAS: cambia si == old
  compare_exchange_weak(...)  -> Result     CAS con fallo espurio (ARM)

Ordering (de menor a mayor restriccion):
  Relaxed   Solo atomicidad. Para contadores independientes.
  Acquire   Barrera de lectura. Para loads que "adquieren" acceso.
  Release   Barrera de escritura. Para stores que "publican" datos.
  AcqRel    Acquire + Release. Para read-modify-write bidireccional.
  SeqCst    Orden total global. Mas seguro, mas costoso.

Reglas:
  - Relaxed: contadores, flags simples sin relacion con otros datos
  - Acquire/Release: publicar datos (producer/consumer, locks)
  - SeqCst: cuando necesitas orden global entre atomics independientes
  - En duda: usa SeqCst y optimiza despues

Patrones:
  Flag de shutdown:     AtomicBool + Relaxed
  Contador:             fetch_add + Relaxed
  ID unico:             fetch_add + Relaxed (retorna anterior)
  Publicar datos:       store(Release) / load(Acquire)
  One-shot init:        compare_exchange + AcqRel

Atomics vs Mutex:
  1 valor simple, independiente  -->  Atomic
  Multiples valores relacionados -->  Mutex
  Estructura compleja            -->  Mutex
  Maximo rendimiento, 1 valor    -->  Atomic

Propiedades:
  - Todos son Send + Sync
  - Todos usan interior mutability (&self, no &mut self)
  - new() es const -> usable en static
  - NO son Copy (se mueven)
  - No existe AtomicF32/F64 en std
```

---

## 13. Ejercicios

### Ejercicio 1: contador atomico multi-hilo

Crea un programa con un contador atomico global que se
incrementa desde multiples hilos, y verifica el resultado:

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::thread;

static COUNTER: AtomicU64 = AtomicU64::new(0);

fn main() {
    let num_threads = 8;
    let increments_per_thread = 100_000;

    let mut handles = vec![];

    for _ in 0..num_threads {
        handles.push(thread::spawn(move || {
            for _ in 0..increments_per_thread {
                // TODO: incrementar COUNTER atomicamente
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    let final_count = COUNTER.load(Ordering::SeqCst);
    let expected = num_threads * increments_per_thread;

    println!("Final: {}, Expected: {}", final_count, expected);
    assert_eq!(final_count, expected as u64);
}
```

Tareas:
1. Completa el `TODO` con `fetch_add`
2. Elige el `Ordering` apropiado y justifica tu eleccion
3. Ejecuta y verifica que el resultado es siempre correcto
4. Cambia `fetch_add` por un CAS loop manual con
   `compare_exchange_weak` y verifica que da el mismo resultado
5. Compara el rendimiento de ambos con `std::time::Instant`

**Prediccion**: el CAS loop sera mas lento o mas rapido que
`fetch_add`? Por que?

> **Pregunta de reflexion**: usaste `Ordering::Relaxed` o
> `Ordering::SeqCst`? El resultado final es correcto con ambos.
> Cual es la diferencia practica? Hay algun escenario donde
> `Relaxed` daria un resultado incorrecto para un simple contador?

### Ejercicio 2: producer-consumer con Acquire/Release

Implementa un patron producer-consumer donde un hilo prepara
datos y senala que estan listos:

```rust
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

struct SharedData {
    ready: AtomicBool,
    // Usamos un array fijo para evitar heap allocation
    // (en produccion usarias Mutex o channel)
    values: [std::sync::atomic::AtomicI32; 4],
}

impl SharedData {
    fn new() -> Self {
        SharedData {
            ready: AtomicBool::new(false),
            values: [
                std::sync::atomic::AtomicI32::new(0),
                std::sync::atomic::AtomicI32::new(0),
                std::sync::atomic::AtomicI32::new(0),
                std::sync::atomic::AtomicI32::new(0),
            ],
        }
    }
}

fn main() {
    let data = Arc::new(SharedData::new());

    let producer_data = Arc::clone(&data);
    let producer = thread::spawn(move || {
        // Paso 1: escribir los valores
        for (i, val) in producer_data.values.iter().enumerate() {
            val.store((i as i32 + 1) * 10, Ordering::Relaxed);
        }

        // Paso 2: senalar que estan listos
        // TODO: que Ordering usar aqui?
        producer_data.ready.store(true, Ordering::___);
    });

    let consumer_data = Arc::clone(&data);
    let consumer = thread::spawn(move || {
        // Esperar a que los datos esten listos
        // TODO: que Ordering usar aqui?
        while !consumer_data.ready.load(Ordering::___) {
            std::hint::spin_loop();
        }

        // Leer los valores
        for val in &consumer_data.values {
            let v = val.load(Ordering::Relaxed);
            print!("{} ", v);
        }
        println!();
    });

    producer.join().unwrap();
    consumer.join().unwrap();
}
```

Tareas:
1. Completa los `___` con el Ordering correcto
2. Explica por que `Relaxed` seria incorrecto para `ready`
3. Explica por que los stores/loads de `values` pueden ser `Relaxed`
4. Ejecuta y verifica que siempre imprime `10 20 30 40`

> **Pregunta de reflexion**: el producer escribe los values con
> `Relaxed` y el consumer los lee con `Relaxed`. Esto es seguro
> **solo porque** hay un par `Release/Acquire` en `ready` que
> establece una relacion "happens-before". Si eliminaras la flag
> `ready` y simplemente hicieras `sleep(1)`, seria seguro?
> El sleep garantiza orden temporal, pero garantiza visibilidad
> de memoria?

### Ejercicio 3: generador de IDs thread-safe

Implementa un generador de IDs unico que nunca repite un ID,
incluso bajo contention de multiples hilos:

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::collections::HashSet;
use std::sync::{Arc, Mutex};
use std::thread;

struct IdGenerator {
    next: AtomicU64,
}

impl IdGenerator {
    fn new(start: u64) -> Self {
        IdGenerator {
            next: AtomicU64::new(start),
        }
    }

    fn next_id(&self) -> u64 {
        // TODO: retornar un ID unico usando fetch_add
        // Que valor retorna fetch_add: el anterior o el nuevo?
        todo!()
    }

    fn next_id_if_below(&self, max: u64) -> Option<u64> {
        // TODO: retornar Some(id) solo si id < max
        // Usa compare_exchange_weak en un loop
        // Retorna None si el contador ya alcanzo max
        todo!()
    }
}

fn main() {
    let gen = Arc::new(IdGenerator::new(1));
    let collected = Arc::new(Mutex::new(HashSet::new()));

    let mut handles = vec![];
    let total_ids = 10_000;
    let num_threads = 8;
    let ids_per_thread = total_ids / num_threads;

    for _ in 0..num_threads {
        let g = Arc::clone(&gen);
        let c = Arc::clone(&collected);
        handles.push(thread::spawn(move || {
            for _ in 0..ids_per_thread {
                let id = g.next_id();
                c.lock().unwrap().insert(id);
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    let ids = collected.lock().unwrap();
    println!("Generated {} unique IDs", ids.len());
    assert_eq!(ids.len(), total_ids);

    // Bonus: probar next_id_if_below
    let gen2 = IdGenerator::new(1);
    let mut count = 0;
    while gen2.next_id_if_below(100).is_some() {
        count += 1;
    }
    assert_eq!(count, 99);  // IDs 1..=99
    println!("Limited generation: {} IDs", count);
}
```

Tareas:
1. Implementa `next_id` con `fetch_add`
2. Implementa `next_id_if_below` con un CAS loop
3. Ejecuta y verifica que todos los IDs son unicos
4. Verifica que `next_id_if_below` para en el maximo correcto

> **Pregunta de reflexion**: `next_id` usa `fetch_add` que nunca
> falla — siempre retorna un ID unico. `next_id_if_below` usa
> `compare_exchange_weak` que puede fallar. Por que no se puede
> implementar `next_id_if_below` con `fetch_add`? Que pasaria
> si hicieras `fetch_add(1)` y luego comprobaras si el resultado
> es mayor que `max`?
