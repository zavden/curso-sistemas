# T01 — Cell\<T\>

## El problema: mutar a traves de &T

Las reglas de borrowing de Rust dicen: puedes tener multiples `&T`
O un solo `&mut T`, nunca ambos. Pero hay situaciones donde necesitas
mutar un campo mientras tienes una referencia compartida:

```rust
struct Counter {
    value: i32,
    access_count: i32,  // queremos incrementar esto en cada lectura
}

impl Counter {
    fn get(&self) -> i32 {  // &self — referencia compartida
        // self.access_count += 1;  // error: cannot assign to `self.access_count`
        //                          // which is behind a `&` reference
        self.value
    }
}

// El problema: get() recibe &self (inmutable),
// pero queremos mutar access_count.
// No podemos cambiar &self a &mut self porque eso impediria
// tener multiples lectores simultaneos.
```

```text
Interior mutability resuelve esto:

  "Mutar el contenido de un valor a traves de una referencia compartida (&T)"

  En vez de: &mut self para mutar un campo
  Usamos:    &self + Cell/RefCell para mutar campos especificos

  Rust verifica las reglas en COMPILACION normalmente.
  Con interior mutability, la verificacion se mueve a RUNTIME (RefCell)
  o se restringe a operaciones seguras (Cell).
```

---

## Que es Cell\<T\>

`Cell<T>` permite leer y escribir el valor interno a traves de
`&Cell<T>` (referencia compartida). No necesitas `&mut Cell<T>`:

```rust
use std::cell::Cell;

let c = Cell::new(5);  // Cell<i32>

// Leer:
let val = c.get();     // 5 — copia el valor (requiere T: Copy)

// Escribir (sin &mut!):
c.set(10);             // reemplaza el valor interno
let val = c.get();     // 10

// Notar: c NO es mut. Cell permite mutar a traves de &Cell.
```

```rust
// Resolviendo el problema del Counter:
use std::cell::Cell;

struct Counter {
    value: i32,
    access_count: Cell<i32>,  // envuelto en Cell
}

impl Counter {
    fn new(value: i32) -> Self {
        Counter { value, access_count: Cell::new(0) }
    }

    fn get(&self) -> i32 {  // &self — no &mut self
        self.access_count.set(self.access_count.get() + 1);  // mutar via Cell
        self.value
    }

    fn accesses(&self) -> i32 {
        self.access_count.get()
    }
}

fn main() {
    let counter = Counter::new(42);
    println!("{}", counter.get());  // 42
    println!("{}", counter.get());  // 42
    println!("accessed {} times", counter.accesses());  // 2
}
```

---

## Cell requiere T: Copy

`Cell::get()` **copia** el valor fuera de la celda. No retorna
una referencia — retorna T directamente. Esto requiere que T
implemente Copy:

```rust
use std::cell::Cell;

// OK — i32 es Copy:
let c = Cell::new(42);
let val: i32 = c.get();  // copia 42 fuera de la celda

// OK — bool es Copy:
let b = Cell::new(true);
let val: bool = b.get();

// OK — f64 es Copy:
let f = Cell::new(3.14);
let val: f64 = f.get();

// ERROR — String NO es Copy:
// let s = Cell::new(String::from("hello"));
// let val = s.get();
// error: the trait `Copy` is not implemented for `String`
// Para String y otros non-Copy, usa RefCell<T> (siguiente topico).
```

```text
¿Por que Cell solo funciona con Copy?

  Cell::get() no puede retornar &T porque:
  - Mientras tienes &T apuntando al interior de la celda,
    alguien podria llamar cell.set(nuevo_valor)
  - El viejo valor se destruiria → &T apuntaria a memoria invalida
  - Esto seria use-after-free → undefined behavior

  Solucion de Cell: copiar el valor fuera.
  - get() copia T y retorna la copia
  - La celda conserva su propia copia
  - No hay referencia al interior → no hay peligro

  Para non-Copy, necesitas RefCell que SI presta referencias
  pero verifica en runtime que no haya conflictos.
```

---

## API de Cell

```rust
use std::cell::Cell;

let c = Cell::new(42);

// get() → T (copia el valor, requiere T: Copy)
let val = c.get();  // 42

// set(val) — reemplaza el valor (el viejo se dropea)
c.set(100);

// replace(val) → T — reemplaza y retorna el viejo
let old = c.replace(200);
assert_eq!(old, 100);
assert_eq!(c.get(), 200);

// into_inner() — consume la Cell y retorna el valor
let val = c.into_inner();
assert_eq!(val, 200);
// c ya no existe

// take() → T — reemplaza con Default y retorna el valor
// (requiere T: Default)
let c = Cell::new(42);
let val = c.take();  // toma el 42, deja 0 (Default de i32)
assert_eq!(val, 42);
assert_eq!(c.get(), 0);
```

```rust
// update() — aplica una funcion al valor (requiere T: Copy)
use std::cell::Cell;

let c = Cell::new(10);
c.update(|val| val * 2);  // 10 * 2 = 20
assert_eq!(c.get(), 20);

// Equivalente a:
// c.set(c.get() * 2);
// Pero update es atomico respecto a la celda (no hay momento donde
// el valor "viejo" y "nuevo" coexistan fuera de la celda).
```

---

## Cell es zero-cost

Cell no tiene overhead en runtime — es del mismo tamano que T
y no tiene contadores ni locks:

```rust
use std::cell::Cell;
use std::mem::size_of;

assert_eq!(size_of::<i32>(), 4);
assert_eq!(size_of::<Cell<i32>>(), 4);  // mismo tamano

assert_eq!(size_of::<bool>(), 1);
assert_eq!(size_of::<Cell<bool>>(), 1);

// Cell es un wrapper #[repr(transparent)] sobre T.
// En el binario compilado, Cell<i32> y i32 son identicos.
```

```text
¿Por que Cell es seguro sin overhead?

  Cell tiene una restriccion clave: NO presta referencias al interior.

  - get() retorna una COPIA (no &T)
  - set() reemplaza el valor (no da &mut T)
  - No hay forma de obtener &T apuntando dentro de la Cell

  Sin referencias al interior, no puede haber aliasing.
  Sin aliasing, set() es seguro — nadie mas esta leyendo el viejo valor.

  Esto es verificable en compilacion → zero overhead en runtime.
```

---

## Cell NO es Sync

`Cell<T>` no puede compartirse entre threads. Si dos threads
pudieran llamar `set()` simultaneamente, habria una data race:

```rust
use std::cell::Cell;

let c = Cell::new(42);

// std::thread::spawn(|| {
//     c.set(100);
// });
// error: `Cell<i32>` cannot be shared between threads safely
// Cell no implementa Sync

// Para multi-thread, usa atomics (AtomicI32, etc.) o Mutex.
```

```text
Sync y Cell:

  Cell<T>: !Sync — no puede ser &Cell<T> en multiples threads
  Cell<T>: Send si T: Send — puede MOVERSE a otro thread

  ¿Por que no Sync?
  Cell::set() no usa lock ni operacion atomica.
  Si dos threads llaman set() a la vez → data race → UB.
  Rust previene esto estaticamente al no implementar Sync.
```

---

## Patrones comunes con Cell

### Contadores y flags

```rust
use std::cell::Cell;

struct Logger {
    enabled: Cell<bool>,
    msg_count: Cell<u32>,
}

impl Logger {
    fn new() -> Self {
        Logger {
            enabled: Cell::new(true),
            msg_count: Cell::new(0),
        }
    }

    fn log(&self, msg: &str) {
        if self.enabled.get() {
            self.msg_count.set(self.msg_count.get() + 1);
            println!("[{}] {msg}", self.msg_count.get());
        }
    }

    fn disable(&self) {
        self.enabled.set(false);
    }

    fn count(&self) -> u32 {
        self.msg_count.get()
    }
}

fn main() {
    let logger = Logger::new();
    logger.log("hello");       // [1] hello
    logger.log("world");       // [2] world
    logger.disable();           // desactivar via &self
    logger.log("ignored");     // no imprime
    println!("total: {}", logger.count());  // 2
}
```

### Lazy initialization de valores Copy

```rust
use std::cell::Cell;

struct Config {
    cached_hash: Cell<Option<u64>>,
    data: String,
}

impl Config {
    fn new(data: &str) -> Self {
        Config {
            cached_hash: Cell::new(None),
            data: data.to_string(),
        }
    }

    fn hash(&self) -> u64 {
        if let Some(h) = self.cached_hash.get() {
            return h;
        }
        // Calcular hash (costoso, solo una vez):
        let h = self.data.len() as u64 * 31;  // hash simplificado
        self.cached_hash.set(Some(h));
        h
    }
}

fn main() {
    let config = Config::new("production");
    println!("{}", config.hash());  // calcula
    println!("{}", config.hash());  // usa cache
}
```

### Cell en campos de structs con Rc

```rust
use std::rc::Rc;
use std::cell::Cell;

// Rc<T> solo da &T. Cell permite mutar campos Copy dentro de &T:
struct SharedState {
    counter: Cell<i32>,
    flag: Cell<bool>,
}

fn main() {
    let state = Rc::new(SharedState {
        counter: Cell::new(0),
        flag: Cell::new(false),
    });

    let s1 = Rc::clone(&state);
    let s2 = Rc::clone(&state);

    // Ambos Rc pueden mutar los Cell:
    s1.counter.set(s1.counter.get() + 1);
    s2.counter.set(s2.counter.get() + 1);
    s1.flag.set(true);

    println!("counter: {}", state.counter.get());  // 2
    println!("flag: {}", state.flag.get());          // true
}
```

---

## Cell vs otras opciones

```text
¿Cuando usar Cell<T>?

  ✓ T es Copy (i32, f64, bool, Option<i32>, etc.)
  ✓ Necesitas mutar a traves de &self
  ✓ Single-thread
  ✓ No necesitas referencia al interior (&T al contenido)
  ✓ Zero overhead

¿Cuando NO usar Cell<T>?

  ✗ T no es Copy (String, Vec, HashMap) → usa RefCell<T>
  ✗ Necesitas &T al contenido → usa RefCell<T>
  ✗ Multi-thread → usa Atomic* o Mutex<T>
  ✗ El campo ya es &mut self accesible → no necesitas Cell

Comparacion:

  Cell<T>:      get/set, T: Copy,  zero-cost,  compile-time safe
  RefCell<T>:   borrow/borrow_mut, cualquier T, runtime check, panic si viola
  Mutex<T>:     lock/unlock, cualquier T, thread-safe, puede bloquear
  Atomic*:      load/store, solo primitivos, thread-safe, zero-lock
```

---

## Errores comunes

```rust
// ERROR 1: usar Cell con tipos non-Copy
use std::cell::Cell;

// let c = Cell::new(String::from("hello"));
// let s = c.get();
// error: the trait `Copy` is not implemented for `String`

// Solucion: RefCell para non-Copy:
use std::cell::RefCell;
let c = RefCell::new(String::from("hello"));
let s = c.borrow().clone();  // borrow + clone
```

```rust
// ERROR 2: asumir que Cell es thread-safe
use std::cell::Cell;

let c = Cell::new(0);

// Compartir entre threads:
// let r = &c;
// std::thread::spawn(move || { c.set(1); });
// error: Cell<i32> cannot be shared between threads safely

// Solucion para multi-thread:
use std::sync::atomic::{AtomicI32, Ordering};
let a = AtomicI32::new(0);
a.store(1, Ordering::SeqCst);
```

```rust
// ERROR 3: intentar obtener &T del interior de Cell
use std::cell::Cell;

let c = Cell::new(42);

// No hay metodo para obtener &i32 apuntando dentro de la Cell:
// let r: &i32 = c.get_ref();  // no existe
// let r: &i32 = &c.get();     // esto es &temp, no apunta al interior

// get() retorna una copia, no una referencia.
// Si necesitas &T al contenido, usa RefCell.
```

```rust
// ERROR 4: race condition logica (no UB, pero bug)
use std::cell::Cell;

let balance = Cell::new(100);

// Dos "operaciones" que leen, calculan, y escriben:
let withdraw = balance.get() - 30;   // lee 100, calcula 70
let deposit = balance.get() + 50;    // lee 100, calcula 150

balance.set(withdraw);  // balance = 70
balance.set(deposit);   // balance = 150 (perdimos el withdraw!)

// En single-thread esto es un bug logico, no UB.
// Solucion: usar update() o hacer las operaciones atomicamente:
let balance = Cell::new(100);
balance.update(|b| b - 30);   // 100 → 70
balance.update(|b| b + 50);   // 70 → 120 (correcto)
```

```rust
// ERROR 5: Cell<T> donde let mut basta
struct Point {
    x: Cell<f64>,  // ¿por que Cell si no hay &self compartido?
    y: Cell<f64>,
}

// Si siempre tienes &mut Point, Cell no aporta nada:
struct Point2 {
    x: f64,
    y: f64,
}

// Cell es para cuando NO puedes tener &mut self.
// Si puedes, usa mutabilidad normal.
```

---

## Cheatsheet

```text
Crear:
  Cell::new(val)            envolver un valor Copy

Leer:
  cell.get()                copia el valor fuera (T: Copy)

Escribir:
  cell.set(val)             reemplaza el valor
  cell.replace(val) → T     reemplaza y retorna el viejo
  cell.update(|v| ...)      aplica funcion al valor
  cell.take() → T           reemplaza con Default, retorna viejo

Consumir:
  cell.into_inner() → T     consume la Cell, retorna el valor

Propiedades:
  - Zero-cost (mismo tamano que T)
  - Solo para T: Copy
  - No presta referencias al interior
  - !Sync (single-thread solamente)
  - Send si T: Send

Patron tipico:
  struct MyStruct {
      normal_field: String,         // mutado con &mut self
      tracked_field: Cell<i32>,     // mutado con &self
  }

Cuando usar:
  - Contadores, flags, caches de valores Copy
  - Campos que mutan en metodos &self
  - Dentro de Rc<T> para mutar campos Copy
```

---

## Ejercicios

### Ejercicio 1 — Contador de accesos

```rust
use std::cell::Cell;

// Implementa un struct CachedValue<T: Copy> que:
//
// a) Almacena un valor T y un Cell<u32> para contar accesos.
//
// b) Tiene un metodo get(&self) → T que retorna el valor
//    e incrementa el contador.
//
// c) Tiene un metodo access_count(&self) → u32.
//
// d) Verifica:
//    let cv = CachedValue::new(42);
//    assert_eq!(cv.get(), 42);
//    assert_eq!(cv.get(), 42);
//    assert_eq!(cv.get(), 42);
//    assert_eq!(cv.access_count(), 3);
//
// e) ¿Por que no podrias hacer esto sin Cell?
```

### Ejercicio 2 — Toggle flag

```rust
use std::cell::Cell;

// a) Crea un struct Feature con:
//    - name: String
//    - enabled: Cell<bool>
//
// b) Implementa toggle(&self) que invierte el estado.
//    Implementa is_enabled(&self) → bool.
//
// c) Crea un Vec<Feature> con 3 features.
//    Itera con .iter() (que da &Feature) y llama toggle()
//    en cada una. ¿Funciona? ¿Por que?
//
// d) ¿Funcionaria si enabled fuera bool sin Cell?
//    ¿Que error daria?
```

### Ejercicio 3 — Cell vs RefCell

```rust
// Para cada caso, decide si Cell o RefCell es apropiado
// y explica por que:
//
// a) Un campo `hits: ???<u64>` que cuenta paginas visitadas
//    en un web server single-thread.
//
// b) Un campo `cache: ???<HashMap<String, String>>` que
//    cachea resultados de queries.
//
// c) Un campo `last_error: ???<Option<i32>>` que guarda
//    el ultimo codigo de error.
//
// d) Un campo `buffer: ???<Vec<u8>>` que acumula bytes
//    recibidos de una conexion.
//
// Regla: Cell si T: Copy y no necesitas &T al contenido.
//        RefCell si T no es Copy o necesitas borrow().
```
