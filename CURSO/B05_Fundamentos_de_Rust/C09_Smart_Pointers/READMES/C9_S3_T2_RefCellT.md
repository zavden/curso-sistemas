# T02 — RefCell\<T\>

## De Cell a RefCell

`Cell<T>` resuelve interior mutability para tipos Copy, pero
no puede trabajar con String, Vec, HashMap u otros tipos non-Copy
porque `get()` necesita copiar el valor. `RefCell<T>` resuelve
esto prestando **referencias** al interior:

```rust
use std::cell::RefCell;

let c = RefCell::new(String::from("hello"));

// Obtener &String (referencia compartida):
let borrowed = c.borrow();
println!("{borrowed}");  // hello
drop(borrowed);

// Obtener &mut String (referencia mutable):
let mut borrowed_mut = c.borrow_mut();
borrowed_mut.push_str(" world");
drop(borrowed_mut);

println!("{}", c.borrow());  // hello world
```

```text
Cell vs RefCell:

  Cell<T>:
  - get() → T         (copia el valor, requiere Copy)
  - set(val)           (reemplaza entero)
  - Nunca presta &T ni &mut T al interior
  - Verificacion en compilacion — zero-cost

  RefCell<T>:
  - borrow() → Ref<T>       (como &T)
  - borrow_mut() → RefMut<T> (como &mut T)
  - Presta referencias al interior
  - Verificacion en RUNTIME — puede panic
```

---

## borrow y borrow_mut

`RefCell` implementa las mismas reglas de borrowing que Rust,
pero las verifica en **runtime** en lugar de compilacion:

```rust
use std::cell::RefCell;

let data = RefCell::new(vec![1, 2, 3]);

// Multiples borrows compartidos — OK (como multiples &T):
{
    let r1 = data.borrow();
    let r2 = data.borrow();
    println!("{r1:?} {r2:?}");  // [1, 2, 3] [1, 2, 3]
}  // r1 y r2 se destruyen → los borrows se liberan

// Un borrow mutable exclusivo — OK (como un &mut T):
{
    let mut w = data.borrow_mut();
    w.push(4);
    println!("{w:?}");  // [1, 2, 3, 4]
}  // w se destruye → el borrow mutable se libera
```

### Panic en runtime

```rust
use std::cell::RefCell;

let data = RefCell::new(42);

// REGLA: no puedes tener borrow() y borrow_mut() activos a la vez

// Esto compila pero PANIC en runtime:
let r = data.borrow();        // borrow compartido activo
// let w = data.borrow_mut(); // PANIC: already borrowed

// Esto tambien hace panic:
let w = data.borrow_mut();    // borrow mutable activo
// let r = data.borrow();     // PANIC: already mutably borrowed
// let w2 = data.borrow_mut(); // PANIC: already mutably borrowed
```

```text
Reglas de RefCell (verificadas en runtime):

  ✓ Multiples borrow() activos simultaneamente
  ✓ Un solo borrow_mut() sin ningun borrow() activo
  ✗ borrow() + borrow_mut() simultaneos → PANIC
  ✗ Dos borrow_mut() simultaneos → PANIC

  Son las mismas reglas que &T / &mut T del compilador,
  pero el error se detecta cuando el programa corre,
  no cuando compila.
```

---

## Ref y RefMut — los guards

`borrow()` no retorna `&T` directamente — retorna `Ref<'_, T>`,
un guard que libera el borrow cuando se destruye:

```rust
use std::cell::{RefCell, Ref, RefMut};

let c = RefCell::new(String::from("hello"));

// borrow() → Ref<String> (actua como &String)
let r: Ref<String> = c.borrow();
println!("len: {}", r.len());  // Deref: Ref<String> → &String → len()
drop(r);  // libera el borrow compartido

// borrow_mut() → RefMut<String> (actua como &mut String)
let mut w: RefMut<String> = c.borrow_mut();
w.push_str(" world");  // DerefMut: RefMut<String> → &mut String
drop(w);  // libera el borrow mutable

// Es CRUCIAL que los guards se destruyan antes de hacer otro borrow.
// Si no los dropeas, el borrow sigue activo.
```

```rust
// Patron problematico: borrow que vive demasiado
use std::cell::RefCell;

let data = RefCell::new(vec![1, 2, 3]);

// MAL — el borrow vive hasta el final del scope:
let r = data.borrow();     // borrow activo
println!("{r:?}");
// data.borrow_mut();      // PANIC: r aun esta vivo
drop(r);                   // ahora si se puede

// BIEN — scope limitado o drop explicito:
{
    let r = data.borrow();
    println!("{r:?}");
}  // r se destruye aqui
data.borrow_mut().push(4);  // OK
```

```rust
// El borrow temporal es comun y seguro:
use std::cell::RefCell;

let data = RefCell::new(vec![1, 2, 3]);

// borrow() temporal — se destruye al final de la expresion:
println!("{:?}", data.borrow());       // OK
data.borrow_mut().push(4);             // OK — el borrow anterior ya murio
println!("len: {}", data.borrow().len()); // OK

// Pero cuidado con let — extiende el lifetime:
let len = data.borrow().len();  // OK: .len() copia, Ref se destruye
let r = data.borrow();          // el Ref vive hasta drop(r) o fin de scope
```

---

## try_borrow y try_borrow_mut

En lugar de panic, puedes usar las versiones que retornan `Result`:

```rust
use std::cell::RefCell;

let data = RefCell::new(42);

// try_borrow → Result<Ref<T>, BorrowError>
let r = data.try_borrow();
assert!(r.is_ok());

// try_borrow_mut → Result<RefMut<T>, BorrowMutError>
let w = data.try_borrow_mut();
// w falla porque r aun esta activo:
// assert!(w.is_err());
drop(r);

// Ahora si:
let w = data.try_borrow_mut();
assert!(w.is_ok());
```

```rust
// Util para evitar panics en codigo que no controlas:
use std::cell::RefCell;

fn safe_increment(data: &RefCell<i32>) {
    match data.try_borrow_mut() {
        Ok(mut val) => *val += 1,
        Err(_) => eprintln!("warning: could not acquire mutable borrow"),
    }
}
```

---

## RefCell en la practica

### Interior mutability en metodos &self

```rust
use std::cell::RefCell;

struct Logger {
    messages: RefCell<Vec<String>>,
}

impl Logger {
    fn new() -> Self {
        Logger { messages: RefCell::new(vec![]) }
    }

    fn log(&self, msg: &str) {  // &self, no &mut self
        self.messages.borrow_mut().push(msg.to_string());
    }

    fn dump(&self) {
        for msg in self.messages.borrow().iter() {
            println!("[LOG] {msg}");
        }
    }

    fn count(&self) -> usize {
        self.messages.borrow().len()
    }
}

fn main() {
    let logger = Logger::new();
    logger.log("started");
    logger.log("processing");
    logger.log("done");
    logger.dump();
    // [LOG] started
    // [LOG] processing
    // [LOG] done
    println!("total: {}", logger.count());  // 3
}
```

### Cache de resultados costosos

```rust
use std::cell::RefCell;
use std::collections::HashMap;

struct Solver {
    cache: RefCell<HashMap<u64, u64>>,
}

impl Solver {
    fn new() -> Self {
        Solver { cache: RefCell::new(HashMap::new()) }
    }

    fn fibonacci(&self, n: u64) -> u64 {
        // Verificar cache:
        if let Some(&result) = self.cache.borrow().get(&n) {
            return result;
        }

        // Calcular:
        let result = if n <= 1 { n } else {
            self.fibonacci(n - 1) + self.fibonacci(n - 2)
        };

        // Guardar en cache:
        self.cache.borrow_mut().insert(n, result);
        result
    }
}

fn main() {
    let solver = Solver::new();
    println!("fib(30) = {}", solver.fibonacci(30));  // 832040
    println!("cache size: {}", solver.cache.borrow().len());
}
```

### Trait con metodos &self que mutan estado

```rust
use std::cell::RefCell;

trait Validator {
    fn validate(&self, input: &str) -> bool;
    fn error_count(&self) -> usize;
}

struct StrictValidator {
    errors: RefCell<Vec<String>>,
}

impl StrictValidator {
    fn new() -> Self {
        StrictValidator { errors: RefCell::new(vec![]) }
    }
}

impl Validator for StrictValidator {
    fn validate(&self, input: &str) -> bool {
        // El trait define &self, no podemos cambiar a &mut self.
        // RefCell permite mutar errors a traves de &self:
        if input.is_empty() {
            self.errors.borrow_mut().push("empty input".into());
            false
        } else if input.len() < 3 {
            self.errors.borrow_mut().push(format!("'{input}' too short"));
            false
        } else {
            true
        }
    }

    fn error_count(&self) -> usize {
        self.errors.borrow().len()
    }
}

fn main() {
    let v = StrictValidator::new();
    v.validate("ok");   // false — too short
    v.validate("");     // false — empty
    v.validate("hello"); // true
    println!("errors: {}", v.error_count());  // 2
}
```

---

## Ref::map — transformar el borrow

```rust
use std::cell::{RefCell, Ref};

struct User {
    name: String,
    email: String,
}

let user = RefCell::new(User {
    name: "Alice".into(),
    email: "alice@example.com".into(),
});

// Quieres retornar una referencia a un campo,
// pero borrow() retorna Ref<User>, no &String.

// Ref::map transforma Ref<User> → Ref<String>:
let name: Ref<String> = Ref::map(user.borrow(), |u| &u.name);
println!("name: {name}");
drop(name);

// RefMut::map tambien existe:
use std::cell::RefMut;
let mut email: RefMut<String> = RefMut::map(user.borrow_mut(), |u| &mut u.email);
email.push_str(".org");
drop(email);

println!("email: {}", user.borrow().email);  // alice@example.com.org
```

---

## RefCell NO es Sync

Al igual que Cell, RefCell es solo para single-thread:

```rust
use std::cell::RefCell;

let data = RefCell::new(42);

// std::thread::spawn(|| {
//     *data.borrow_mut() += 1;
// });
// error: `RefCell<i32>` cannot be shared between threads safely

// Para multi-thread:
// RefCell → Mutex  (bloquea hasta obtener acceso)
// RefCell → RwLock (multiples lectores O un escritor)
```

```text
Correspondencia single-thread → multi-thread:

  Cell<T>       →  Atomic*     (Copy types, sin lock)
  RefCell<T>    →  Mutex<T>    (cualquier T, lock exclusivo)
  RefCell<T>    →  RwLock<T>   (cualquier T, lectores/escritor)

  RefCell panic en conflicto → Mutex bloquea hasta obtener acceso
  Esa es la diferencia fundamental:
    RefCell:  "ya prestado" → PANIC (bug en tu codigo)
    Mutex:    "ya locked"   → ESPERA (concurrencia normal)
```

---

## Costo de RefCell

A diferencia de Cell (zero-cost), RefCell tiene un overhead
minimo — mantiene un contador de borrows activos:

```rust
use std::cell::RefCell;
use std::mem::size_of;

// RefCell agrega un campo para el estado de borrow:
assert_eq!(size_of::<i32>(), 4);
assert_eq!(size_of::<RefCell<i32>>(), 8);  // i32 + borrow counter

// El overhead es:
// - 1 isize extra para el estado de borrow
// - Una verificacion (branch) en cada borrow/borrow_mut
// - Insignificante en la practica
```

```text
Estado interno de RefCell:

  borrow_state: isize

  0        → no hay borrows activos
  1, 2, 3  → N borrows compartidos activos
  -1       → un borrow mutable activo

  borrow():     verifica state >= 0, incrementa
  borrow_mut(): verifica state == 0, pone -1
  drop(Ref):    decrementa
  drop(RefMut): pone 0
```

---

## Errores comunes

```rust
// ERROR 1: borrow y borrow_mut simultaneos → panic
use std::cell::RefCell;

let data = RefCell::new(vec![1, 2, 3]);

let r = data.borrow();       // borrow compartido
// data.borrow_mut().push(4); // PANIC: already borrowed

// Solucion: asegurar que r ya no exista:
drop(r);
data.borrow_mut().push(4);   // OK
```

```rust
// ERROR 2: borrow dentro de un loop que tambien muta
use std::cell::RefCell;

let items = RefCell::new(vec![1, 2, 3, 4, 5]);

// MAL — borrow compartido vive durante todo el for:
// for item in items.borrow().iter() {  // borrow activo durante el loop
//     if *item > 3 {
//         items.borrow_mut().push(item * 10); // PANIC
//     }
// }

// BIEN — clonar primero para liberar el borrow:
let snapshot: Vec<i32> = items.borrow().clone();
for item in &snapshot {
    if *item > 3 {
        items.borrow_mut().push(item * 10);
    }
}
assert_eq!(*items.borrow(), vec![1, 2, 3, 4, 5, 40, 50]);
```

```rust
// ERROR 3: retornar Ref de un metodo sin cuidado
use std::cell::{RefCell, Ref};

struct Container {
    data: RefCell<Vec<i32>>,
}

impl Container {
    // OK — retornar Ref:
    fn data(&self) -> Ref<Vec<i32>> {
        self.data.borrow()
    }

    // PROBLEMATICO — el caller debe saber que tiene un borrow activo:
    // Si llama data() y luego intenta borrow_mut → panic
}

let c = Container { data: RefCell::new(vec![1, 2, 3]) };
let d = c.data();  // borrow activo
// c.data.borrow_mut().push(4);  // PANIC — d aun vive
drop(d);
c.data.borrow_mut().push(4);  // OK
```

```rust
// ERROR 4: RefCell<T> dentro de &T sin necesidad
struct Config {
    name: RefCell<String>,  // ¿necesitas mutar name via &Config?
}

// Si siempre tienes &mut Config disponible, RefCell no aporta nada.
// Solo usa RefCell cuando &self es la unica opcion
// (ej: trait define &self, Rc<Config>, multiples referencias).

// Sin RefCell (preferible si puedes):
struct Config2 {
    name: String,
}
```

```rust
// ERROR 5: RefCell con recursion que mantiene el borrow
use std::cell::RefCell;

struct Tree {
    children: RefCell<Vec<Tree>>,
}

impl Tree {
    fn depth(&self) -> usize {
        let children = self.children.borrow();
        // OK: cada nivel crea su propio borrow
        // No hay conflicto porque son RefCells DIFERENTES
        1 + children.iter().map(|c| c.depth()).max().unwrap_or(0)
    }

    fn add_child_recursive(&self) {
        let children = self.children.borrow();
        if children.len() < 3 {
            drop(children);  // IMPORTANTE: liberar ANTES de borrow_mut
            self.children.borrow_mut().push(Tree {
                children: RefCell::new(vec![]),
            });
        }
    }
}
```

---

## Cheatsheet

```text
Crear:
  RefCell::new(val)          envolver cualquier valor

Borrow compartido (&T):
  cell.borrow()              → Ref<T>   (panic si borrow_mut activo)
  cell.try_borrow()          → Result<Ref<T>, BorrowError>

Borrow mutable (&mut T):
  cell.borrow_mut()          → RefMut<T>  (panic si cualquier borrow activo)
  cell.try_borrow_mut()      → Result<RefMut<T>, BorrowMutError>

Transformar:
  Ref::map(ref, |v| &v.field)      → Ref<Field>
  RefMut::map(refmut, |v| &mut ..) → RefMut<Field>

Consumir:
  cell.into_inner() → T      consume la RefCell
  cell.replace(val) → T      reemplaza y retorna el viejo
  cell.take() → T             reemplaza con Default

Reglas (verificadas en runtime):
  ✓ Multiples borrow() simultaneos
  ✓ Un solo borrow_mut() sin otros borrows
  ✗ borrow() + borrow_mut() → PANIC
  ✗ Dos borrow_mut() → PANIC

Propiedades:
  - Funciona con cualquier T (no requiere Copy)
  - Overhead: 1 isize + 1 branch por borrow
  - !Sync (single-thread)
  - Send si T: Send

Equivalencias multi-thread:
  RefCell<T>  →  Mutex<T>   (lock exclusivo)
  RefCell<T>  →  RwLock<T>  (lectores/escritor)

Cuando usar:
  - Traits con &self que necesitan mutar estado
  - Dentro de Rc<T> para mutabilidad compartida
  - Caches y memoizacion con &self
  - Cuando Cell no alcanza (T non-Copy o necesitas &T)
```

---

## Ejercicios

### Ejercicio 1 — Logger con RefCell

```rust
use std::cell::RefCell;

// Implementa un MemoizedFn que:
//
// struct MemoizedFn<F, T> {
//     func: F,
//     cache: RefCell<Option<T>>,
// }
//
// a) new(func) crea un MemoizedFn con cache vacia.
//
// b) call(&self) → &T — ejecuta func la primera vez
//    y guarda el resultado en cache. Las siguientes veces
//    retorna el valor cacheado.
//    Pista: usa Ref::map para retornar Ref<T>.
//
// c) reset(&self) — limpia el cache para forzar recalculo.
//
// d) Verifica:
//    let expensive = MemoizedFn::new(|| {
//        println!("computing...");
//        42
//    });
//    println!("{}", expensive.call());  // "computing..." + 42
//    println!("{}", expensive.call());  // 42 (sin "computing...")
```

### Ejercicio 2 — borrow panic

```rust
use std::cell::RefCell;

// Predice si cada bloque hace PANIC o ejecuta OK.
// Luego verifica ejecutando:

let data = RefCell::new(vec![1, 2, 3]);

// a)
{
    let r = data.borrow();
    let r2 = data.borrow();
    println!("{r:?} {r2:?}");
}

// b)
{
    let r = data.borrow();
    drop(r);
    let w = data.borrow_mut();
    drop(w);
}

// c)
{
    let r = data.borrow();
    let w = data.borrow_mut();
    println!("{r:?}");
}

// d)
{
    println!("{}", data.borrow().len());
    data.borrow_mut().push(4);
    println!("{}", data.borrow().len());
}

// Para cada caso: ¿compila? ¿panic? ¿por que?
```

### Ejercicio 3 — Observer con RefCell

```rust
use std::cell::RefCell;

// Implementa un EventBus simple:
//
// struct EventBus {
//     listeners: RefCell<Vec<Box<dyn Fn(&str)>>>,
// }
//
// a) fn subscribe(&self, listener: impl Fn(&str) + 'static)
//    Agrega un listener al bus.
//
// b) fn emit(&self, event: &str)
//    Llama a todos los listeners con el evento.
//
// c) Verifica:
//    let bus = EventBus::new();
//    bus.subscribe(|e| println!("L1: {e}"));
//    bus.subscribe(|e| println!("L2: {e}"));
//    bus.emit("click");
//    // L1: click
//    // L2: click
//
// d) ¿Que pasa si un listener intenta llamar bus.subscribe()
//    dentro de su callback? ¿Panic? ¿Por que?
//    Pista: emit() tiene un borrow activo.
```
