# Send y Sync

## Indice
1. [El problema que resuelven Send y Sync](#1-el-problema-que-resuelven-send-y-sync)
2. [El trait Send](#2-el-trait-send)
3. [El trait Sync](#3-el-trait-sync)
4. [Relacion entre Send y Sync](#4-relacion-entre-send-y-sync)
5. [Auto-traits: implementacion automatica](#5-auto-traits-implementacion-automatica)
6. [Tipos notables que NO son Send o Sync](#6-tipos-notables-que-no-son-send-o-sync)
7. [Por que Rc no es Send](#7-por-que-rc-no-es-send)
8. [Por que RefCell no es Sync](#8-por-que-refcell-no-es-sync)
9. [Implementar Send y Sync manualmente](#9-implementar-send-y-sync-manualmente)
10. [PhantomData y su efecto en Send/Sync](#10-phantomdata-y-su-efecto-en-sendsync)
11. [Patron: disenar tipos thread-safe](#11-patron-disenar-tipos-thread-safe)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema que resuelven Send y Sync

En C, nada impide compartir un puntero entre hilos sin
sincronizacion. El compilador no se queja; el bug aparece
en produccion como corrupcion silenciosa. Rust aborda esto
en tiempo de compilacion con dos **marker traits**:

```
                    Hilo A                          Hilo B
                 +-----------+                  +-----------+
                 |  valor x  |---move/send----->|  valor x  |
                 +-----------+                  +-----------+
                       Send: "es seguro MOVER este tipo a otro hilo"

                 +-----------+                  +-----------+
                 |  &valor x |<--- referencia --|  &valor x |
                 +-----------+  compartida      +-----------+
                       Sync: "es seguro COMPARTIR &T entre hilos"
```

Estas son las **unicas** garantias que el compilador necesita
para prevenir data races en tiempo de compilacion. No generan
codigo — son puramente verificaciones del type system.

---

## 2. El trait Send

`Send` indica que la **propiedad** (ownership) de un valor puede
transferirse de forma segura de un hilo a otro:

```rust
// Definicion en std (simplificada):
pub unsafe auto trait Send {}
```

Tres palabras clave importantes aqui:

- **`unsafe`**: implementarlo manualmente requiere `unsafe impl`,
  porque el compilador no puede verificar la seguridad — es una
  promesa del programador.
- **`auto`**: el compilador lo implementa automaticamente si todos
  los campos del tipo son `Send`.
- **`trait`**: es un trait sin metodos (marker trait).

### Que significa en la practica

```rust
use std::thread;

fn send_example() {
    let data = vec![1, 2, 3]; // Vec<i32> es Send

    // move transfiere ownership al nuevo hilo
    let handle = thread::spawn(move || {
        println!("data en otro hilo: {:?}", data);
    });

    // data ya no existe aqui — se movio
    // println!("{:?}", data); // ERROR: value used after move

    handle.join().unwrap();
}
```

La firma de `thread::spawn` **requiere** Send:

```rust
pub fn spawn<F, T>(f: F) -> JoinHandle<T>
where
    F: FnOnce() -> T + Send + 'static,
    T: Send + 'static,
{
    // ...
}
```

Tanto el closure `F` como su valor de retorno `T` deben ser `Send`.
Todo lo que el closure captura con `move` forma parte de `F`, asi
que cada valor capturado debe ser `Send`.

### Ejemplo: Send rechazado en compilacion

```rust
use std::rc::Rc;
use std::thread;

fn rc_not_send() {
    let data = Rc::new(42);

    thread::spawn(move || {
        println!("{}", data);
    });
    // ERROR de compilacion:
    // `Rc<i32>` cannot be sent between threads safely
    // the trait `Send` is not implemented for `Rc<i32>`
}
```

El compilador bloquea el codigo **antes** de que puedas crear
un data race. Esta es la diferencia fundamental con C.

---

## 3. El trait Sync

`Sync` indica que una **referencia compartida** `&T` puede
enviarse de forma segura a otro hilo:

```rust
// Definicion en std (simplificada):
pub unsafe auto trait Sync {}
```

La relacion formal es:

```
T es Sync  <=>  &T es Send
```

Es decir: un tipo `T` es `Sync` si y solo si es seguro que
multiples hilos tengan `&T` simultaneamente.

### Ejemplo: compartir una referencia

```rust
use std::sync::Arc;
use std::thread;

fn sync_example() {
    let data = Arc::new(vec![1, 2, 3]);  // Vec<i32> es Sync

    let mut handles = vec![];
    for i in 0..3 {
        let data_ref = Arc::clone(&data);
        handles.push(thread::spawn(move || {
            // Cada hilo tiene &Vec<i32> (via Deref de Arc)
            println!("hilo {}: {:?}", i, *data_ref);
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
}
```

`Arc<T>` requiere que `T: Sync + Send` para ser `Send`:

```rust
// En std:
unsafe impl<T: ?Sized + Sync + Send> Send for Arc<T> {}
unsafe impl<T: ?Sized + Sync + Send> Sync for Arc<T> {}
```

Si `T` no es `Sync`, `Arc<T>` no se puede enviar entre hilos,
porque los hilos obtendrian `&T` compartidas sin sincronizacion.

---

## 4. Relacion entre Send y Sync

```
+--------------------------------------------------+
|           Relacion formal:                        |
|                                                   |
|   T: Sync   <==>   &T: Send                      |
|                                                   |
|   "Puedo compartir &T entre hilos"                |
|   equivale a                                      |
|   "Puedo enviar &T a otro hilo"                   |
+--------------------------------------------------+
```

### Cuatro combinaciones posibles

```
                         Send
                    Si          No
              +------------+------------+
         Si   |  La mayoria | *const T   |
Sync          |  de tipos   | (raw ptr)  |
              +------------+------------+
         No   |  Cell<T>    | Rc<T>      |
              |  RefCell<T> |            |
              +------------+------------+
```

| Combinacion     | Ejemplo        | Significado                                   |
|-----------------|----------------|-----------------------------------------------|
| Send + Sync     | `i32`, `String`, `Vec<T>`, `Mutex<T>` | Se puede mover y compartir libremente |
| Send + !Sync    | `Cell<i32>`, `RefCell<T>`              | Se puede mover, pero no compartir &T  |
| !Send + Sync    | `*const T` (segun contexto)            | Raro en la practica                   |
| !Send + !Sync   | `Rc<T>`, `*mut T`                      | Solo usar en un hilo                  |

### Por que Mutex\<T\> es Sync (aunque T no lo sea)

`Mutex<T>` proporciona su propia sincronizacion — el lock
garantiza acceso exclusivo. Por eso:

```rust
// En std:
unsafe impl<T: ?Sized + Send> Sync for Mutex<T> {}
```

Nota: requiere `T: Send`, no `T: Sync`. Esto es porque el
`Mutex` convierte acceso compartido en acceso exclusivo, asi
que no necesita que `T` sea seguro para compartir — solo que
sea seguro para mover (ya que un hilo adquiere propiedad
temporal exclusiva del contenido).

```
    Sin Mutex:          Con Mutex<T>:
    &T -> &T -> &T      &Mutex<T> -> lock() -> &mut T (exclusivo)
    (necesita Sync)      (solo necesita Send)
```

---

## 5. Auto-traits: implementacion automatica

`Send` y `Sync` son **auto traits**: el compilador los implementa
automaticamente si todos los campos de un struct/enum los implementan.

```rust
struct MyData {
    name: String,       // String: Send + Sync
    values: Vec<i32>,   // Vec<i32>: Send + Sync
    count: usize,       // usize: Send + Sync
}
// MyData es automaticamente Send + Sync
```

Un solo campo que no sea Send "infecta" todo el struct:

```rust
use std::rc::Rc;

struct NotSendable {
    name: String,       // Send + Sync
    shared: Rc<i32>,    // !Send + !Sync  <-- infecta
}
// NotSendable: !Send + !Sync (por culpa de Rc)
```

### Opt-out explicito

Puedes **remover** Send o Sync con una implementacion negativa:

```rust
struct Handle {
    fd: i32,
}

// Explicitamente NO es Send (ej: file descriptor de un thread)
impl !Send for Handle {}
```

> **Nota**: las implementaciones negativas (`impl !Trait`) son
> inestables y solo disponibles en nightly. En stable, la forma
> practica de opt-out es incluir un campo `PhantomData` que no
> sea Send/Sync (ver seccion 10).

---

## 6. Tipos notables que NO son Send o Sync

### Tipos !Send

| Tipo               | Razon                                              |
|--------------------|----------------------------------------------------|
| `Rc<T>`            | Contador de referencias no atomico                 |
| `*mut T`           | Raw pointer, sin garantias                         |
| `*const T`         | Raw pointer, sin garantias                         |
| `MutexGuard<'_, T>`| Algunos OS requieren unlock en el mismo hilo       |

### Tipos !Sync

| Tipo               | Razon                                              |
|--------------------|----------------------------------------------------|
| `Rc<T>`            | Contador no atomico                                |
| `Cell<T>`          | Mutabilidad interior sin sincronizacion            |
| `RefCell<T>`       | Borrow checking en runtime sin sincronizacion      |
| `UnsafeCell<T>`    | Base de toda mutabilidad interior, sin proteccion  |
| `*mut T`           | Raw pointer, sin garantias                         |

### Tipos que SI son Send + Sync

| Tipo                | Nota                                              |
|---------------------|---------------------------------------------------|
| `i32`, `f64`, etc.  | Tipos primitivos                                  |
| `String`, `Vec<T>`  | Si T es Send/Sync                                 |
| `Arc<T>`            | Si T es Send + Sync                               |
| `Mutex<T>`          | Si T es Send (proporciona su propia Sync)         |
| `RwLock<T>`         | Si T es Send + Sync                               |
| `AtomicBool`, etc.  | Atomics son inherentemente thread-safe            |
| `mpsc::Sender<T>`   | Si T es Send                                      |

---

## 7. Por que Rc no es Send

`Rc<T>` usa un contador de referencias **no atomico** (operaciones
normales de lectura-modificacion-escritura en memoria). Veamos que
pasaria si fuera Send:

```
    Hilo A                              Hilo B
    ------                              ------
    rc.clone()                          rc.clone()
      |                                   |
      v                                   v
    lee strong_count = 1                lee strong_count = 1
    escribe strong_count = 2            escribe strong_count = 2
                                        (deberia ser 3!)

    Resultado: strong_count = 2 cuando deberia ser 3
    --> doble free --> undefined behavior
```

El problema es clasico: **read-modify-write** sin sincronizacion.

### Comparacion Rc vs Arc

```
    Rc<T>:                          Arc<T>:
    +------------------+            +------------------+
    | strong: usize    |            | strong: AtomicUsize |
    | weak:   usize    |            | weak:   AtomicUsize |
    | value:  T        |            | value:  T           |
    +------------------+            +------------------+
    Operaciones normales            fetch_add atomico
    (NO thread-safe)                (thread-safe, mas lento)
```

```rust
// Esto NO compila:
use std::rc::Rc;
use std::thread;

fn main() {
    let rc = Rc::new(42);
    let rc2 = Rc::clone(&rc);

    thread::spawn(move || {
        println!("{}", rc2);
    });
    // error[E0277]: `Rc<i32>` cannot be sent between threads safely
}
```

```rust
// Solucion: usar Arc
use std::sync::Arc;
use std::thread;

fn main() {
    let arc = Arc::new(42);
    let arc2 = Arc::clone(&arc);

    thread::spawn(move || {
        println!("{}", arc2);  // OK: Arc es Send + Sync
    });
}
```

El coste de `Arc` sobre `Rc` es una operacion atomica por
clone/drop. En codigo single-threaded, `Rc` es la eleccion
correcta — no pagues por lo que no necesitas.

---

## 8. Por que RefCell no es Sync

`RefCell<T>` implementa borrow checking en runtime: mantiene
un contador de borrows activos y paniquea si se viola la regla
de exclusividad (multiples `&mut` o `&mut` + `&`).

El problema: ese contador **no tiene sincronizacion**:

```
    Hilo A                              Hilo B
    ------                              ------
    refcell.borrow_mut()                refcell.borrow_mut()
      |                                   |
      v                                   v
    lee borrow_state = 0                lee borrow_state = 0
    (no hay borrows activos)            (no hay borrows activos)
    escribe borrow_state = -1           escribe borrow_state = -1
    (mutable borrow)                    (mutable borrow)

    Resultado: DOS borrows mutables simultaneos
    --> data race --> undefined behavior
```

`RefCell<T>` **es** Send (puedes moverlo a otro hilo), pero
**no** es Sync (no puedes tener `&RefCell<T>` en dos hilos).

### Comparacion: Cell, RefCell, Mutex

```
+-------------+------+------+----------------------------------+
| Tipo        | Send | Sync | Uso                              |
+-------------+------+------+----------------------------------+
| Cell<T>     | Si*  | No   | Interior mutability single-thread|
| RefCell<T>  | Si*  | No   | Borrow check runtime, 1 thread  |
| Mutex<T>    | Si*  | Si   | Acceso exclusivo multi-thread    |
| RwLock<T>   | Si*  | Si   | Multiples readers multi-thread   |
+-------------+------+------+----------------------------------+
  * Si T es Send
```

Regla general: `Cell`/`RefCell` para single-thread,
`Mutex`/`RwLock` para multi-thread.

---

## 9. Implementar Send y Sync manualmente

A veces necesitas implementar `Send` o `Sync` manualmente para
un tipo que contiene raw pointers u otros tipos `!Send`/`!Sync`,
cuando **tu** sabes que el uso es seguro:

```rust
struct RawBuffer {
    ptr: *mut u8,
    len: usize,
}

// SAFETY: RawBuffer owns the allocation exclusively.
// The pointer is not shared with any other RawBuffer instance.
// All access through &RawBuffer is read-only.
// All access through &mut RawBuffer is exclusive.
unsafe impl Send for RawBuffer {}

// SAFETY: &RawBuffer only permits read access to the buffer.
// No interior mutability exists through shared references.
unsafe impl Sync for RawBuffer {}
```

### Reglas para hacerlo correctamente

1. **Documenta el invariante de seguridad** (el comentario `// SAFETY:`).
   Explica por que es seguro.

2. **Verifica que el invariante se mantiene** en todo el API publico
   del tipo.

3. **Solo usa `unsafe impl`** cuando realmente lo necesitas — si
   puedes reestructurar el tipo para que el compilador derive Send/Sync
   automaticamente, eso es mejor.

### Ejemplo real: wrapper alrededor de handle de C

```rust
/// Wrapper around a C library handle that is thread-safe
/// according to the library's documentation.
struct DbHandle {
    handle: *mut ffi::db_connection,
}

// SAFETY: The C library documents that db_connection handles
// are safe to move between threads. Each handle is independent.
unsafe impl Send for DbHandle {}

// SAFETY: The C library documents that concurrent read operations
// on the same handle are safe. We only expose &self methods that
// perform reads. Write operations require &mut self.
unsafe impl Sync for DbHandle {}

impl Drop for DbHandle {
    fn drop(&mut self) {
        unsafe { ffi::db_close(self.handle); }
    }
}
```

### Cuando NO implementar Send/Sync manualmente

- El tipo tiene mutabilidad interior sin sincronizacion
- El puntero es compartido con otro propietario
- La documentacion de la libreria C dice "not thread-safe"
- No estas seguro — en caso de duda, no lo hagas

---

## 10. PhantomData y su efecto en Send/Sync

`PhantomData<T>` es un tipo de tamano cero que le dice al
compilador "actua como si tuviera un campo de tipo T" sin
realmente tenerlo. Esto afecta la derivacion automatica de
Send y Sync:

```rust
use std::marker::PhantomData;

struct MyType<T> {
    data: *mut u8,
    _marker: PhantomData<T>,  // "posee" un T conceptualmente
}
// MyType<T>: Send si T es Send
// MyType<T>: Sync si T es Sync
```

### Usos comunes

**1. Indicar ownership logico:**

```rust
use std::marker::PhantomData;

struct Slice<'a, T> {
    ptr: *const T,
    len: usize,
    _lifetime: PhantomData<&'a T>,  // ligado al lifetime 'a
}
// Sin PhantomData, el compilador no sabria que Slice
// esta ligado al lifetime 'a
```

**2. Opt-out de Send/Sync en stable Rust:**

```rust
use std::marker::PhantomData;
use std::cell::Cell;

struct NotSync {
    data: i32,
    _no_sync: PhantomData<Cell<()>>,  // Cell es !Sync
}
// NotSync: Send (Cell<()> es Send)
// NotSync: !Sync (Cell<()> es !Sync)
```

```rust
use std::marker::PhantomData;
use std::rc::Rc;

struct NotSend {
    data: i32,
    _no_send: PhantomData<Rc<()>>,  // Rc es !Send
}
// NotSend: !Send (Rc<()> es !Send)
// NotSend: !Sync (Rc<()> es !Sync)
```

### Tabla de PhantomData y sus efectos

```
+------------------------+------+------+
| PhantomData<X>         | Send | Sync |
+------------------------+------+------+
| PhantomData<T>         | si T | si T |
| PhantomData<&'a T>     | si T | si T |
| PhantomData<*const T>  | No   | No   |
| PhantomData<*mut T>    | No   | No   |
| PhantomData<Rc<T>>     | No   | No   |
| PhantomData<Cell<T>>   | si T | No   |
+------------------------+------+------+
```

---

## 11. Patron: disenar tipos thread-safe

### Paso 1: elegir la primitiva correcta

```
    +-- Solo 1 hilo? --> Cell / RefCell
    |
    +-- Multiples hilos?
         |
         +-- Solo lectura? --> Arc<T> (T: Sync)
         |
         +-- Lectura + escritura?
              |
              +-- Muchos readers, pocos writers? --> Arc<RwLock<T>>
              |
              +-- Acceso exclusivo simple? --> Arc<Mutex<T>>
              |
              +-- Solo un valor simple (bool, usize)? --> AtomicXxx
```

### Paso 2: verificar Send + Sync

```rust
// Funcion de ayuda para verificar en compilacion:
fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

struct MyService {
    data: std::sync::Arc<std::sync::Mutex<Vec<String>>>,
}

// Coloca estas llamadas en un test:
#[test]
fn service_is_thread_safe() {
    assert_send::<MyService>();
    assert_sync::<MyService>();
}
```

Si el test compila, tu tipo es thread-safe. Si no compila,
el error te dira exactamente que campo viola la restriccion.

### Paso 3: example completo

```rust
use std::sync::{Arc, Mutex};
use std::thread;

struct Counter {
    value: Arc<Mutex<u64>>,
    name: String,  // inmutable despues de creacion
}

// Counter es automaticamente Send + Sync porque:
// - Arc<Mutex<u64>>: Send + Sync (u64 es Send)
// - String: Send + Sync

impl Counter {
    fn new(name: &str) -> Self {
        Counter {
            value: Arc::new(Mutex::new(0)),
            name: name.to_string(),
        }
    }

    fn increment(&self) {
        let mut val = self.value.lock().unwrap();
        *val += 1;
    }

    fn get(&self) -> u64 {
        *self.value.lock().unwrap()
    }
}

fn main() {
    let counter = Arc::new(Counter::new("requests"));

    let mut handles = vec![];
    for _ in 0..4 {
        let c = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..1000 {
                c.increment();
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    println!("{}: {}", counter.name, counter.get());
    // Output: requests: 4000 (siempre correcto)
}
```

---

## 12. Errores comunes

### Error 1: intentar compartir Rc entre hilos

```rust
// MAL: Rc no es Send
use std::rc::Rc;
use std::thread;

fn main() {
    let data = Rc::new(vec![1, 2, 3]);
    let data2 = Rc::clone(&data);
    thread::spawn(move || {
        println!("{:?}", data2);  // ERROR en compilacion
    });
}
```

```rust
// BIEN: usar Arc
use std::sync::Arc;
use std::thread;

fn main() {
    let data = Arc::new(vec![1, 2, 3]);
    let data2 = Arc::clone(&data);
    thread::spawn(move || {
        println!("{:?}", data2);  // OK
    });
}
```

**Por que falla**: `Rc` usa contadores no atomicos. El compilador
detecta que `Rc<T>: !Send` y rechaza el `spawn`.

### Error 2: usar RefCell donde necesitas Mutex

```rust
// MAL: RefCell no es Sync, no puedes compartir &RefCell entre hilos
use std::cell::RefCell;
use std::sync::Arc;
use std::thread;

fn main() {
    let data = Arc::new(RefCell::new(0));
    let data2 = Arc::clone(&data);
    thread::spawn(move || {
        *data2.borrow_mut() += 1;  // ERROR en compilacion
    });
}
// error: `RefCell<i32>` cannot be shared between threads safely
// the trait `Sync` is not implemented for `RefCell<i32>`
```

```rust
// BIEN: usar Mutex para acceso multi-hilo
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let data = Arc::new(Mutex::new(0));
    let data2 = Arc::clone(&data);
    thread::spawn(move || {
        *data2.lock().unwrap() += 1;  // OK
    });
}
```

**Por que falla**: `Arc<T>` requiere `T: Sync` para ser `Send`.
`RefCell<i32>: !Sync`, asi que `Arc<RefCell<i32>>: !Send`.

### Error 3: unsafe impl Send/Sync sin justificacion

```rust
// MAL: implementar Send sin verificar que es seguro
struct Wrapper {
    shared_ptr: *mut Vec<String>,  // compartido con otro Wrapper!
}

unsafe impl Send for Wrapper {}  // PELIGROSO

// Ahora puedes crear data races porque dos hilos
// pueden acceder al mismo Vec sin sincronizacion
```

```rust
// BIEN: usar Arc<Mutex<T>> en lugar de raw pointer
use std::sync::{Arc, Mutex};

struct Wrapper {
    data: Arc<Mutex<Vec<String>>>,
}
// Send + Sync automaticos, sin unsafe
```

**Regla**: `unsafe impl Send/Sync` es una promesa que haces al
compilador. Si la promesa es falsa, el resultado es undefined
behavior sin que el compilador pueda detectarlo.

### Error 4: olvidar que PhantomData afecta Send/Sync

```rust
use std::marker::PhantomData;
use std::rc::Rc;

struct Config {
    value: i32,
    _marker: PhantomData<Rc<()>>,  // Oops: hace Config !Send !Sync
}

// Intentar usar en thread::spawn fallara
```

```rust
// BIEN: usar el PhantomData correcto para tu intencion
use std::marker::PhantomData;

struct Config<T> {
    value: i32,
    _marker: PhantomData<T>,  // Send/Sync depende de T
}
// Config<String>: Send + Sync  (OK)
// Config<Rc<()>>: !Send !Sync  (intencionado)
```

**Regla**: piensa en PhantomData como un campo real para efectos
de Send/Sync. Elige el tipo fantasma con cuidado.

### Error 5: confundir Send con Sync

```rust
// "Mi tipo es Send, asi que puedo compartir &T entre hilos"
// FALSO: Send permite MOVER, Sync permite COMPARTIR &T

use std::cell::Cell;

// Cell<i32> ES Send pero NO es Sync
fn example() {
    let cell = Cell::new(42);
    // Puedes mover cell a otro hilo (Send)
    // Pero NO puedes tener &cell en dos hilos (no Sync)

    std::thread::spawn(move || {
        cell.set(100);  // OK: cell fue MOVIDA (Send)
    });
}
```

```
    Send != Sync

    Send: "puedo MOVER el valor a otro hilo"
         (transferencia de ownership)

    Sync: "puedo tener &T en multiples hilos"
         (acceso compartido concurrente)
```

---

## 13. Cheatsheet

```
MARKER TRAITS - REFERENCIA RAPIDA
==================================

Definiciones:
  T: Send     El valor se puede mover a otro hilo
  T: Sync     &T se puede compartir entre hilos
  T: Sync <=> &T: Send

Auto-implementacion:
  struct Foo { a: A, b: B }
  Foo: Send si A: Send AND B: Send
  Foo: Sync si A: Sync AND B: Sync

Tipos comunes:
  Send + Sync:  i32, String, Vec<T>, Arc<T>, Mutex<T>, RwLock<T>
  Send + !Sync: Cell<T>, RefCell<T>
  !Send + !Sync: Rc<T>, *mut T, *const T, MutexGuard<T>

Cuadro de decision:
  Rc<T>     -->  Arc<T>           (counter atomico)
  Cell<T>   -->  AtomicXxx        (operaciones atomicas)
  RefCell<T> --> Mutex<T>         (lock real)
  RefCell<T> --> RwLock<T>        (si hay muchos readers)

spawn requiere:
  F: FnOnce() -> T + Send + 'static
  T: Send + 'static
  (todo lo capturado debe ser Send)

Verificar en compilacion:
  fn assert_send<T: Send>() {}
  fn assert_sync<T: Sync>() {}
  assert_send::<MyType>();
  assert_sync::<MyType>();

unsafe impl:
  unsafe impl Send for MyType {}  // promesa del programador
  unsafe impl Sync for MyType {}  // DOCUMENTA // SAFETY:

PhantomData<X> hereda Send/Sync de X:
  PhantomData<Rc<()>>    -->  !Send !Sync
  PhantomData<Cell<()>>  -->  Send  !Sync
  PhantomData<String>    -->  Send  Sync

Opt-out en stable:
  _no_sync: PhantomData<Cell<()>>   // quita Sync, mantiene Send
  _no_send: PhantomData<Rc<()>>     // quita Send y Sync
```

---

## 14. Ejercicios

### Ejercicio 1: verificar Send y Sync de tipos custom

Crea un programa que defina varios structs y use funciones de
compilacion para verificar cuales son Send y/o Sync:

```rust
use std::cell::RefCell;
use std::marker::PhantomData;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

fn assert_send<T: Send>() {}
fn assert_sync<T: Sync>() {}

struct TypeA {
    data: Vec<String>,
    count: usize,
}

struct TypeB {
    data: Vec<String>,
    cache: RefCell<Option<String>>,
}

struct TypeC {
    handle: *mut u8,
}

struct TypeD {
    data: Arc<Mutex<Vec<String>>>,
}

struct TypeE<T> {
    value: i32,
    _marker: PhantomData<T>,
}

fn main() {
    // Descomenta una por una y predice: compila o no?

    // 1. TypeA
    assert_send::<TypeA>();
    assert_sync::<TypeA>();

    // 2. TypeB
    assert_send::<TypeB>();
    // assert_sync::<TypeB>();  // Compila?

    // 3. TypeC
    // assert_send::<TypeC>();  // Compila?
    // assert_sync::<TypeC>();  // Compila?

    // 4. TypeD
    assert_send::<TypeD>();
    assert_sync::<TypeD>();

    // 5. TypeE<String>
    assert_send::<TypeE<String>>();
    assert_sync::<TypeE<String>>();

    // 6. TypeE<Rc<()>>
    // assert_send::<TypeE<Rc<()>>>();  // Compila?
    // assert_sync::<TypeE<Rc<()>>>();  // Compila?
}
```

**Prediccion antes de compilar**:
- TypeA: Send ___  Sync ___
- TypeB: Send ___  Sync ___
- TypeC: Send ___  Sync ___
- TypeD: Send ___  Sync ___
- TypeE\<String\>: Send ___  Sync ___
- TypeE\<Rc\<()\>\>: Send ___  Sync ___

Descomenta las lineas una por una y compara con tus predicciones.

> **Pregunta de reflexion**: `TypeB` contiene un `RefCell`. Es
> `Send` pero no `Sync`. Si creas un `Arc<TypeB>` e intentas
> clonarlo a otro hilo, el error de compilacion menciona `Sync`,
> no `Send`. Explica por que.

### Ejercicio 2: hacer un tipo thread-safe

Tienes un tipo que no es thread-safe. Transformalo paso a paso
para poder usarlo desde multiples hilos:

```rust
use std::cell::RefCell;
use std::rc::Rc;

// Version 1: single-thread (NO thread-safe)
struct UserCache {
    users: Rc<RefCell<Vec<String>>>,
    max_size: usize,
}

impl UserCache {
    fn new(max_size: usize) -> Self {
        UserCache {
            users: Rc::new(RefCell::new(Vec::new())),
            max_size,
        }
    }

    fn add(&self, user: String) {
        let mut users = self.users.borrow_mut();
        if users.len() < self.max_size {
            users.push(user);
        }
    }

    fn list(&self) -> Vec<String> {
        self.users.borrow().clone()
    }
}

fn main() {
    let cache = UserCache::new(100);
    cache.add("alice".to_string());
    cache.add("bob".to_string());
    println!("{:?}", cache.list());
}
```

Tareas:
1. Reemplaza `Rc` por `Arc` y `RefCell` por `Mutex`
2. Ajusta los metodos para usar `.lock()` en lugar de `.borrow()`
3. Verifica que compila con `assert_send` y `assert_sync`
4. Usa el cache desde 4 hilos que agregan usuarios en paralelo
5. Imprime el resultado final y verifica que todos los usuarios
   estan presentes

> **Pregunta de reflexion**: en la version thread-safe, `max_size`
> es un `usize` inmutable. No necesita estar dentro del `Mutex`.
> Que ventaja tiene dejarlo fuera? Que pasaria si estuviera dentro?

### Ejercicio 3: unsafe impl Send para un FFI handle

Simula un tipo que envuelve un recurso externo (como un handle
de libreria C) y necesita `unsafe impl Send`:

```rust
use std::thread;

// Simula un handle de libreria C
mod ffi {
    static mut NEXT_ID: u32 = 0;

    pub fn create_handle() -> *mut u8 {
        unsafe {
            NEXT_ID += 1;
            NEXT_ID as *mut u8
        }
    }

    pub fn use_handle(h: *mut u8) {
        println!("Using handle: {:?}", h);
    }

    pub fn destroy_handle(h: *mut u8) {
        println!("Destroyed handle: {:?}", h);
    }
}

struct SafeHandle {
    raw: *mut u8,
}

impl SafeHandle {
    fn new() -> Self {
        SafeHandle {
            raw: ffi::create_handle(),
        }
    }

    fn use_it(&self) {
        ffi::use_handle(self.raw);
    }
}

impl Drop for SafeHandle {
    fn drop(&mut self) {
        ffi::destroy_handle(self.raw);
    }
}

// TODO: agregar unsafe impl Send con comentario SAFETY
// TODO: NO implementar Sync (cada handle debe usarse desde un solo hilo)

fn main() {
    let handle = SafeHandle::new();

    // Mover el handle a otro hilo
    let t = thread::spawn(move || {
        handle.use_it();
        // handle se dropea aqui
    });

    t.join().unwrap();
}
```

Tareas:
1. Agrega `unsafe impl Send for SafeHandle {}` con un comentario
   `// SAFETY:` que explique por que es seguro
2. **No** implementes `Sync` — explica en un comentario por que no
3. Verifica que compila y ejecuta correctamente
4. Intenta agregar `unsafe impl Sync` y despues crea un escenario
   donde dos hilos llamen `use_it()` en `&SafeHandle`. Explica
   por que esto seria inseguro con un handle real de C

> **Pregunta de reflexion**: `MutexGuard` en la libreria estandar
> es `!Send` — no puedes mover el guard a otro hilo. Algunos
> sistemas operativos requieren que `pthread_mutex_unlock` se
> llame desde el mismo hilo que hizo `pthread_mutex_lock`. Como
> se relaciona esto con lo que acabas de aprender sobre `Send` y
> los invariantes que `unsafe impl` promete mantener?
