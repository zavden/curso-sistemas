# Cuándo usar cada smart pointer: árbol de decisión práctico

## Índice
1. [Inventario de smart pointers](#1-inventario-de-smart-pointers)
2. [Árbol de decisión principal](#2-árbol-de-decisión-principal)
3. [Ownership único: Box, Vec, String](#3-ownership-único-box-vec-string)
4. [Ownership compartido: Rc vs Arc](#4-ownership-compartido-rc-vs-arc)
5. [Mutabilidad interior: Cell vs RefCell vs Mutex](#5-mutabilidad-interior-cell-vs-refcell-vs-mutex)
6. [Combinaciones frecuentes](#6-combinaciones-frecuentes)
7. [Cow: evitar clones innecesarios](#7-cow-evitar-clones-innecesarios)
8. [Pin: inmovilizar en memoria](#8-pin-inmovilizar-en-memoria)
9. [Weak: romper ciclos](#9-weak-romper-ciclos)
10. [Anti-patrones](#10-anti-patrones)
11. [Tabla comparativa completa](#11-tabla-comparativa-completa)
12. [Patrones por dominio](#12-patrones-por-dominio)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. Inventario de smart pointers

Rust ofrece múltiples smart pointers, cada uno con un propósito preciso. La clave es entender **qué problema resuelve cada uno** y elegir el más simple que satisfaga tus necesidades:

| Pointer | Ownership | Mutabilidad | Thread-safe | Coste |
|---------|-----------|-------------|-------------|-------|
| `Box<T>` | Único | Normal (`&mut`) | Sí (si `T: Send`) | 1 alloc, 0 overhead |
| `Rc<T>` | Compartido | Solo lectura | No | 1 alloc + refcount |
| `Arc<T>` | Compartido | Solo lectura | Sí | 1 alloc + atomic refcount |
| `Cell<T>` | No es pointer | Mutable vía `&self` | No | 0 (zero-cost) |
| `RefCell<T>` | No es pointer | Mutable vía `&self` | No | 1 isize + runtime check |
| `Mutex<T>` | No es pointer | Mutable vía `&self` | Sí | OS mutex |
| `RwLock<T>` | No es pointer | Mutable vía `&self` | Sí | OS rwlock |
| `Cow<'a, B>` | Prestado u owned | Clone-on-write | Depende de B | 0 si no clona |
| `Weak<T>` | No-owning | Solo lectura (si upgrade) | Depende (Rc/Arc) | 0 extra |
| `Pin<P>` | Igual que P | Restringida si `!Unpin` | Igual que P | 0 (wrapper) |

### La regla de oro

> **Usa la herramienta más simple que resuelva tu problema.**
> `&T` > `Box<T>` > `Rc<T>` > `Arc<T>` > `Arc<Mutex<T>>`

Cada paso hacia la derecha añade complejidad. Solo avanza cuando el anterior es insuficiente.

---

## 2. Árbol de decisión principal

```text
¿Necesitas ownership del dato?
├── NO → usa &T o &mut T (referencias)
│        ¿El lifetime es difícil de expresar?
│        ├── NO → quédate con referencias
│        └── SÍ → considera Box<T>, Rc<T> o clonar
│
└── SÍ → ¿Cuántos owners necesitas?
         │
         ├── UNO SOLO
         │   ├── ¿Cabe en el stack? → valor directo (T)
         │   ├── ¿Tipo de tamaño dinámico (dyn Trait, [T])? → Box<T>
         │   ├── ¿Tipo recursivo? → Box<T>
         │   └── ¿Dato muy grande? → Box<T>
         │
         └── MÚLTIPLES OWNERS
             ├── ¿Un solo hilo?
             │   ├── SÍ → Rc<T>
             │   │        ¿Necesitas mutar?
             │   │        ├── NO → Rc<T> directamente
             │   │        └── SÍ → Rc<RefCell<T>> o Rc<Cell<T>>
             │   │
             │   └── NO (multi-hilo)
             │       └── Arc<T>
             │            ¿Necesitas mutar?
             │            ├── NO → Arc<T> directamente
             │            └── SÍ → Arc<Mutex<T>> o Arc<RwLock<T>>
             │
             ¿Hay ciclos posibles?
             └── SÍ → Weak<T> para romperlos
```

---

## 3. Ownership único: Box, Vec, String

### Cuándo Box\<T\>

`Box<T>` tiene exactamente tres casos de uso legítimos:

```rust
// 1. Tipos de tamaño dinámico (trait objects)
let handler: Box<dyn Fn(i32) -> i32> = Box::new(|x| x * 2);
let error: Box<dyn std::error::Error> = Box::new(io_err);

// 2. Tipos recursivos (tamaño infinito sin indirección)
enum List<T> {
    Cons(T, Box<List<T>>),
    Nil,
}

// 3. Datos grandes que no quieres en el stack
let big: Box<[u8; 1_000_000]> = Box::new([0u8; 1_000_000]);
```

### Cuándo NO usar Box

```rust
// INNECESARIO: Box<String>, Box<Vec<T>>
// String y Vec ya están en el heap internamente
let bad: Box<String> = Box::new(String::from("hello"));  // doble indirección
let good: String = String::from("hello");                 // correcto

// INNECESARIO: Box solo para pasar ownership
fn process(data: Box<Vec<i32>>) { ... }  // malo
fn process(data: Vec<i32>) { ... }        // bueno — ownership se transfiere igual

// INNECESARIO: Box para devolver un valor
fn make() -> Box<i32> { Box::new(42) }  // malo (excepto si es dyn Trait)
fn make() -> i32 { 42 }                  // bueno
```

### Vec y String como smart pointers

`Vec<T>` y `String` son técnicamente smart pointers al heap con metadata adicional (length, capacity). Se comportan como `Box<[T]>` y `Box<str>` con capacidad de crecer:

```rust
// Vec<T> implementa Deref<Target = [T]>
let v = vec![1, 2, 3];
let slice: &[i32] = &v;  // deref coercion

// String implementa Deref<Target = str>
let s = String::from("hello");
let st: &str = &s;  // deref coercion

// Prefiere &[T] y &str en parámetros de funciones
fn process_items(items: &[i32]) { ... }    // acepta Vec, array, slice
fn process_text(text: &str) { ... }        // acepta String, &str, Cow<str>
```

---

## 4. Ownership compartido: Rc vs Arc

### Diagrama de decisión

```text
¿Necesitas compartir ownership?
│
├── ¿Single-thread? → Rc<T>
│   Coste: ~2 words overhead + refcount (no-atómico)
│   Rc::clone es O(1) — solo incrementa contador
│
└── ¿Multi-thread? → Arc<T>
    Coste: ~2 words overhead + refcount (atómico)
    Arc::clone es O(1) — con barrera atómica (5-20x más lento que Rc::clone)
```

### Señales de que necesitas Rc/Arc

```rust
// 1. Múltiples estructuras apuntan al mismo dato
struct Graph {
    nodes: Vec<Rc<Node>>,  // nodos compartidos entre edges
}

// 2. Caché compartido
struct App {
    config: Arc<Config>,  // leído desde múltiples hilos
}

// 3. El dato debe sobrevivir a múltiples scopes sin saber cuál termina primero
let shared = Rc::new(ExpensiveData::load());
let handle_a = Rc::clone(&shared);
let handle_b = Rc::clone(&shared);
// shared, handle_a, handle_b pueden droppearse en cualquier orden
```

### Cuándo NO usar Rc/Arc

```rust
// No necesitas Rc si un owner es claramente "el principal"
struct Parent {
    child: Child,  // ownership directo, sin Rc
}

// No necesitas Arc si pasas datos por canal
tx.send(data).unwrap();  // ownership se transfiere, no se comparte

// No uses Arc "por si acaso" — usa Rc single-thread si no cruzas hilos
// Rc → Arc es un cambio de una línea si después necesitas multi-thread
```

---

## 5. Mutabilidad interior: Cell vs RefCell vs Mutex

### Diagrama de decisión

```text
¿Necesitas mutar a través de &self?
│
├── ¿Single-thread?
│   │
│   ├── ¿T: Copy y operaciones simples (get/set)?
│   │   └── Cell<T>     (zero-cost, sin runtime check)
│   │
│   ├── ¿Necesitas &T o &mut T al contenido?
│   │   └── RefCell<T>  (runtime borrow check, puede panic)
│   │
│   └── ¿Inicialización lazy una sola vez?
│       └── OnceCell<T> / LazyCell<T>
│
└── ¿Multi-thread?
    │
    ├── ¿Lecturas frecuentes, escrituras raras?
    │   └── RwLock<T>  (múltiples lectores O un escritor)
    │
    ├── ¿Acceso exclusivo suficiente?
    │   └── Mutex<T>   (un acceso a la vez)
    │
    ├── ¿Operaciones atómicas simples (contadores, flags)?
    │   └── AtomicU32, AtomicBool, etc.
    │
    └── ¿Inicialización lazy una sola vez?
        └── OnceLock<T> / LazyLock<T>
```

### Comparación directa

```rust
use std::cell::{Cell, RefCell};
use std::sync::{Mutex, RwLock};

struct SingleThread {
    // Contador simple → Cell (Copy, get/set)
    count: Cell<u32>,

    // Caché de string → RefCell (necesitamos &str para leer)
    cache: RefCell<Option<String>>,

    // Config lazy → OnceCell
    config: OnceCell<Config>,
}

struct MultiThread {
    // Contador → Atomic (más ligero que Mutex para primitivos)
    count: AtomicU32,

    // Caché compartido → RwLock (muchas lecturas)
    cache: RwLock<Option<String>>,

    // Estado exclusivo → Mutex
    state: Mutex<State>,

    // Config lazy → OnceLock
    config: OnceLock<Config>,
}
```

---

## 6. Combinaciones frecuentes

### Rc\<RefCell\<T\>\> — shared mutable state (single-thread)

```rust
use std::cell::RefCell;
use std::rc::Rc;

// Múltiples owners que necesitan mutar
let shared = Rc::new(RefCell::new(vec![1, 2, 3]));

let a = Rc::clone(&shared);
let b = Rc::clone(&shared);

a.borrow_mut().push(4);
b.borrow_mut().push(5);
// shared = [1, 2, 3, 4, 5]
```

**Cuándo**: grafos mutables, observer pattern, estado de aplicación single-thread.

### Arc\<Mutex\<T\>\> — shared mutable state (multi-thread)

```rust
use std::sync::{Arc, Mutex};

let shared = Arc::new(Mutex::new(HashMap::new()));

let map = Arc::clone(&shared);
std::thread::spawn(move || {
    map.lock().unwrap().insert("key", "value");
});
```

**Cuándo**: caché compartido, contadores globales, estado de servidor.

### Arc\<RwLock\<T\>\> — lectura frecuente, escritura rara

```rust
use std::sync::{Arc, RwLock};

let config = Arc::new(RwLock::new(load_config()));

// Múltiples lectores simultáneos
let r = config.read().unwrap();

// Escritor exclusivo (bloquea lectores)
let mut w = config.write().unwrap();
*w = reload_config();
```

**Cuándo**: configuración hot-reload, feature flags, routing tables.

### Box\<dyn Trait\> — polimorfismo con ownership

```rust
trait Shape {
    fn area(&self) -> f64;
}

let shapes: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle { radius: 5.0 }),
    Box::new(Rectangle { w: 3.0, h: 4.0 }),
];

for shape in &shapes {
    println!("{}", shape.area());
}
```

**Cuándo**: colecciones heterogéneas, plugins, strategy pattern.

### Rc\<dyn Trait\> — polimorfismo compartido

```rust
trait Handler {
    fn handle(&self, event: &Event);
}

let handler: Rc<dyn Handler> = Rc::new(LogHandler);
let a = Rc::clone(&handler);
let b = Rc::clone(&handler);
// a y b comparten el mismo handler sin clonar datos
```

**Cuándo**: event dispatchers, middleware chains, UI widgets compartidos.

---

## 7. Cow: evitar clones innecesarios

```text
¿Recibes datos que a veces necesitas modificar y a veces no?
│
├── Siempre los modificas → toma ownership (T, String, Vec<T>)
├── Nunca los modificas → toma prestado (&T, &str, &[T])
└── A veces → Cow<'a, B>
```

```rust
use std::borrow::Cow;

// Patrón clásico: normalización que a veces es no-op
fn normalize(input: &str) -> Cow<'_, str> {
    if input.contains('\t') {
        Cow::Owned(input.replace('\t', "    "))
    } else {
        Cow::Borrowed(input)  // sin allocación
    }
}

// En APIs: acepta tanto &str como String
fn log(msg: Cow<'_, str>) {
    println!("[LOG] {}", msg);
}
```

**Cuándo**: parsers, normalización de texto, funciones que *posiblemente* transforman datos, APIs que reciben tanto borrowed como owned.

---

## 8. Pin: inmovilizar en memoria

```text
¿Necesitas Pin?
│
├── ¿Usas async/await normalmente? → NO (el runtime lo maneja)
├── ¿Implementas Future/Stream manualmente? → SÍ
├── ¿Tienes structs auto-referenciales? → SÍ
└── ¿Almacenas futures en structs? → SÍ, probablemente
```

```rust
use std::pin::Pin;
use std::future::Future;

// Almacenar futures heterogéneos — necesitas Pin<Box<dyn Future>>
let futures: Vec<Pin<Box<dyn Future<Output = i32>>>> = vec![
    Box::pin(async { 1 }),
    Box::pin(async { compute().await }),
];

// Implementar Future — el método poll recibe Pin<&mut Self>
impl Future for MyFuture {
    type Output = i32;
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<i32> {
        // ...
    }
}
```

**Cuándo**: implementaciones manuales de `Future`/`Stream`, wrappers de futures, tipos con auto-referencias.

---

## 9. Weak: romper ciclos

```text
¿Tienes relaciones circulares con Rc/Arc?
│
├── Parent → Child: Rc/Arc (ownership)
└── Child → Parent: Weak (no ownership, no ciclo)
```

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

struct Node {
    children: RefCell<Vec<Rc<Node>>>,     // owner de hijos
    parent: RefCell<Option<Weak<Node>>>,  // referencia débil al padre
}
```

**Cuándo**: árboles con referencia al padre, doubly-linked structures, observer pattern (observadores registrados como `Weak`), cachés con entradas que pueden invalidarse.

---

## 10. Anti-patrones

### Anti-patrón 1: Box innecesario

```rust
// MAL: doble indirección, sin beneficio
fn process(data: Box<Vec<i32>>) { ... }

// BIEN: Vec ya gestiona el heap
fn process(data: Vec<i32>) { ... }
// O si solo necesitas leer:
fn process(data: &[i32]) { ... }
```

### Anti-patrón 2: Arc cuando Rc basta

```rust
// MAL: overhead atómico sin necesidad multi-hilo
let data = Arc::new(vec![1, 2, 3]);
// Todo en un solo hilo...

// BIEN: sin coste atómico
let data = Rc::new(vec![1, 2, 3]);
```

### Anti-patrón 3: Rc\<RefCell\<T\>\> para todo

```rust
// MAL: usar Rc<RefCell<T>> cuando ownership directo funciona
struct App {
    config: Rc<RefCell<Config>>,  // ¿quién más lo necesita?
}

// BIEN: si solo App lo usa, ownership directo
struct App {
    config: Config,
}
```

### Anti-patrón 4: Mutex cuando no hay contención

```rust
// MAL: Mutex para dato que solo un hilo accede
let data = Arc::new(Mutex::new(vec![1, 2, 3]));
// Solo un hilo lo usa...

// BIEN: sin lock si no hay contención
// Pasa ownership al hilo directamente
std::thread::spawn(move || {
    let mut data = vec![1, 2, 3];
    data.push(4);
});
```

### Anti-patrón 5: RefCell para lógica que debería ser &mut

```rust
// MAL: usar RefCell para evitar reestructurar el código
struct Processor {
    cache: RefCell<HashMap<String, String>>,
}

impl Processor {
    fn process(&self, key: &str) -> String {
        // RefCell porque process toma &self...
        self.cache.borrow_mut().entry(key.to_string())
            .or_insert_with(|| expensive_compute(key)).clone()
    }
}

// BIEN: ¿realmente necesitas &self? Si eres el único owner, usa &mut self
impl Processor {
    fn process(&mut self, key: &str) -> String {
        self.cache.entry(key.to_string())
            .or_insert_with(|| expensive_compute(key)).clone()
    }
}
```

`RefCell` es para cuando **no puedes** usar `&mut self` (trait impone `&self`, dato compartido con `Rc`). Si puedes reestructurar para usar `&mut self`, hazlo — es más eficiente y detecta errores en compilación.

### Anti-patrón 6: Clonar en lugar de compartir (y viceversa)

```rust
// Clonar datos grandes repetidamente → considerar Rc/Arc
let big_data = load_huge_dataset();
let a = big_data.clone();  // copia costosa
let b = big_data.clone();  // otra copia costosa

// Mejor:
let big_data = Rc::new(load_huge_dataset());
let a = Rc::clone(&big_data);  // O(1), sin copia
let b = Rc::clone(&big_data);  // O(1)

// PERO: para datos pequeños, clonar es más simple que Rc
let name = "Alice".to_string();
let a = name.clone();  // barato, simple
// No uses Rc<String> para strings cortos — el overhead no vale
```

---

## 11. Tabla comparativa completa

```text
╔═══════════════════╦═════════╦══════════╦════════╦═══════════════════════════╗
║ Tipo              ║ Thread  ║ Mutación ║ Coste  ║ Caso de uso típico        ║
╠═══════════════════╬═════════╬══════════╬════════╬═══════════════════════════╣
║ &T / &mut T       ║ Sí*     ║ Normal   ║ 0      ║ Default, siempre primero  ║
║ Box<T>            ║ Sí*     ║ Normal   ║ 1 alloc║ Trait objects, recursión  ║
║ Rc<T>             ║ No      ║ No       ║ Refcnt ║ Shared ownership (1 hilo) ║
║ Arc<T>            ║ Sí      ║ No       ║ Atomic ║ Shared ownership (N hilos)║
║ Cell<T>           ║ No      ║ &self    ║ 0      ║ Flags, counters (Copy)    ║
║ RefCell<T>        ║ No      ║ &self    ║ Bajo   ║ Mutabilidad con &self     ║
║ Mutex<T>          ║ Sí      ║ &self    ║ OS     ║ Mut exclusiva multi-hilo  ║
║ RwLock<T>         ║ Sí      ║ &self    ║ OS     ║ Muchos readers, pocos wr  ║
║ Cow<'a, B>        ║ Sí*     ║ CoW      ║ 0/alloc║ Evitar clones innecesarios║
║ Weak<T>           ║ Depende ║ No       ║ 0      ║ Romper ciclos Rc/Arc      ║
║ Pin<P>            ║ Depende ║ Depende  ║ 0      ║ Futures, auto-referencias ║
║ OnceCell<T>       ║ No      ║ Init 1x  ║ 0      ║ Lazy init single-thread   ║
║ OnceLock<T>       ║ Sí      ║ Init 1x  ║ Bajo   ║ Lazy init multi-thread    ║
║ Atomic*           ║ Sí      ║ &self    ║ 0      ║ Counters/flags multi-hilo ║
╚═══════════════════╩═════════╩══════════╩════════╩═══════════════════════════╝
  * Sí si T: Send/Sync según corresponda
```

---

## 12. Patrones por dominio

### CLI tools

```rust
struct App {
    config: Config,               // ownership directo (un solo owner)
    cache: HashMap<String, Data>, // ownership directo con &mut self
}
// Raramente necesitas Rc/Arc en un CLI single-thread
```

### Web servers (multi-thread)

```rust
struct AppState {
    db_pool: Pool,                          // el pool maneja concurrencia internamente
    config: Arc<Config>,                    // compartido read-only entre handlers
    rate_limiter: Arc<Mutex<RateLimiter>>,  // estado mutable compartido
    cache: Arc<RwLock<HashMap<K, V>>>,      // lecturas frecuentes
}
```

### Game engines / ECS

```rust
// Generalmente evitan smart pointers — usan arena allocation
struct World {
    entities: Vec<Entity>,           // datos contiguos
    components: Vec<Option<Comp>>,   // indexados por entity ID
    // Los "punteros" son índices (usize), no Rc/Box
}
```

### Parsers / compiladores

```rust
// AST con Box para recursión:
enum Expr {
    Literal(i64),
    BinOp {
        op: Op,
        left: Box<Expr>,
        right: Box<Expr>,
    },
}

// Interning de strings con Rc:
type Symbol = Rc<str>;

// O arena allocation para performance:
struct Arena {
    storage: Vec<String>,
}
```

### GUI / event-driven

```rust
// Callbacks con Rc<RefCell<T>>:
let state = Rc::new(RefCell::new(AppState::default()));

let state_clone = Rc::clone(&state);
button.on_click(move || {
    state_clone.borrow_mut().counter += 1;
});
```

---

## 13. Errores comunes

### Error 1: Elegir por familiaridad, no por necesidad

```rust
// Viene de Java/C# → todo es "referencia compartida"
let data = Rc::new(RefCell::new(MyStruct::new()));

// En Rust, la mayoría de datos tienen un solo owner:
let data = MyStruct::new();  // empezar aquí
// Solo agregar Rc/RefCell cuando el compilador te lo pida
```

**Regla**: empieza con ownership directo y `&`/`&mut`. Solo añade smart pointers cuando el compilador rechace tu código o cuando genuinamente necesites las capacidades extra.

### Error 2: Confundir dónde va RefCell en Rc\<RefCell\<T\>\>

```rust
// MAL: RefCell por fuera, Rc por dentro
let bad = RefCell::new(Rc::new(data));
// Esto permite mutar qué Rc contiene el RefCell,
// pero no los datos internos del Rc

// BIEN: Rc por fuera, RefCell por dentro
let good = Rc::new(RefCell::new(data));
// Esto permite compartir (Rc) un dato mutable (RefCell)
```

Regla mnemónica: **"Rc fuera, RefCell dentro"** — primero compartes, luego mutas.

### Error 3: Usar Arc\<Mutex\<T\>\> para datos read-only

```rust
// MAL: Mutex para datos que nunca se mutan
let config = Arc::new(Mutex::new(load_config()));
// Cada lectura requiere lock, sin necesidad

// BIEN: Arc solo, los datos son inmutables
let config = Arc::new(load_config());
// Lectura sin lock, múltiples hilos leen simultáneamente
```

### Error 4: No considerar alternativas a smart pointers

```rust
// A veces la solución no es un smart pointer, sino reestructurar:

// Problema: "necesito que A y B compartan datos"
// Solución con smart pointer:
let shared = Rc::new(data);
let a = A { data: Rc::clone(&shared) };
let b = B { data: Rc::clone(&shared) };

// Solución sin smart pointer — A y B reciben referencias:
let data = Data::new();
let a = A { data: &data };
let b = B { data: &data };
process(&a, &b);
// Si los lifetimes son manejables, esto es más simple y eficiente

// Otra alternativa: clonar si el dato es barato
let a = A { data: data.clone() };
let b = B { data: data };
```

### Error 5: Olvidar que Rc/Arc no dan mutabilidad

```rust
let shared = Rc::new(String::from("hello"));

// ERROR: Rc solo da &T, no &mut T
// shared.push_str(" world");  // no compila
// Rc::get_mut(&mut shared)    // retorna None si strong_count > 1

// Necesitas interior mutability para mutar contenido compartido:
let shared = Rc::new(RefCell::new(String::from("hello")));
shared.borrow_mut().push_str(" world");
```

---

## 14. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║              SMART POINTER DECISION CHEATSHEET                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EMPIEZA AQUÍ                                                      ║
║  T (valor directo) → &T/&mut T → solo si no basta → smart pointer ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  UN OWNER                                                          ║
║  ┌─────────────────────────────────────────────────────────────┐    ║
║  │ Trait object / recursivo / grande → Box<T>                  │    ║
║  │ Secuencia dinámica              → Vec<T>                    │    ║
║  │ Texto dinámico                  → String                    │    ║
║  │ Todo lo demás                   → T directo                 │    ║
║  └─────────────────────────────────────────────────────────────┘    ║
║                                                                    ║
║  MÚLTIPLES OWNERS                                                  ║
║  ┌─────────────────────────────────────────────────────────────┐    ║
║  │ 1 hilo, read-only  → Rc<T>                                 │    ║
║  │ 1 hilo, mutable    → Rc<RefCell<T>>                         │    ║
║  │ N hilos, read-only  → Arc<T>                                │    ║
║  │ N hilos, mutable    → Arc<Mutex<T>> o Arc<RwLock<T>>        │    ║
║  │ Ciclos posibles     → agregar Weak<T>                       │    ║
║  └─────────────────────────────────────────────────────────────┘    ║
║                                                                    ║
║  INTERIOR MUTABILITY (mutar vía &self)                             ║
║  ┌─────────────────────────────────────────────────────────────┐    ║
║  │ T: Copy, simple    → Cell<T>                                │    ║
║  │ Necesitas &T/&mut T → RefCell<T>                            │    ║
║  │ Multi-thread       → Mutex<T> / RwLock<T> / Atomic*         │    ║
║  │ Init una vez       → OnceCell / OnceLock / Lazy*            │    ║
║  └─────────────────────────────────────────────────────────────┘    ║
║                                                                    ║
║  CASOS ESPECIALES                                                  ║
║  ┌─────────────────────────────────────────────────────────────┐    ║
║  │ Evitar clone condicional → Cow<'a, B>                       │    ║
║  │ Async / Future manual    → Pin<Box<T>> o pin!               │    ║
║  │ Referencia no-owning     → Weak<T>                          │    ║
║  └─────────────────────────────────────────────────────────────┘    ║
║                                                                    ║
║  COMBINACIONES VÁLIDAS                                             ║
║  Rc<RefCell<T>>    ≈   Arc<Mutex<T>>     (single ≈ multi)          ║
║  Rc<Cell<T>>       ≈   Arc<AtomicU32>    (Copy types)              ║
║  Rc<Vec<RefCell>>      granularidad por elemento                   ║
║  Box<dyn Trait>        polimorfismo con ownership                  ║
║                                                                    ║
║  COMBINACIONES SOSPECHOSAS                                         ║
║  Box<String>           doble indirección innecesaria               ║
║  Box<Vec<T>>           doble indirección innecesaria               ║
║  Rc<T> multi-thread    no compila (no es Send)                     ║
║  RefCell multi-thread  no compila (no es Sync)                     ║
║  Arc sin contención    ¿necesitas Arc realmente?                   ║
║  Rc<RefCell> simple    ¿puedes usar &mut self?                     ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 15. Ejercicios

### Ejercicio 1: Elegir el smart pointer correcto

Para cada escenario, indica qué smart pointer(s) usarías y por qué. Después verifica implementando un esqueleto que compile:

```rust
// Escenario A: Parser que construye un AST con nodos de distintos tipos
//              (Literal, BinOp, UnaryOp, FnCall)
// ¿Qué usas para los hijos de cada nodo?

// Escenario B: Servidor web donde cada request handler necesita
//              acceso a un pool de conexiones DB (solo lectura)
//              y a un rate limiter (escritura)
// ¿Qué usas para compartir cada uno?

// Escenario C: Función que recibe un path como &str y necesita
//              normalizarlo (reemplazar \ por / en Windows).
//              En Linux el path ya es correcto, no hay que clonar.
// ¿Qué tipo de retorno usas?

// Escenario D: Struct con un campo `visited: ???` que cuenta
//              cuántas veces se llama a un método &self.
// ¿Qué tipo usas para visited?

// Escenario E: Árbol DOM donde cada nodo tiene hijos y una
//              referencia a su padre. El padre puede morir antes
//              que el hijo si se reestructura el árbol.
// ¿Qué usas para hijos? ¿Y para padre?
```

### Ejercicio 2: Refactorizar anti-patrones

El siguiente código funciona pero usa smart pointers innecesarios. Refactoriza eliminando los que sobran y simplificando, sin cambiar el comportamiento observable:

```rust
use std::cell::RefCell;
use std::rc::Rc;

struct Config {
    name: Box<String>,
    values: Box<Vec<i32>>,
}

struct Processor {
    config: Rc<RefCell<Config>>,
    buffer: Rc<RefCell<Vec<String>>>,
}

impl Processor {
    fn new(name: &str, values: Vec<i32>) -> Self {
        Processor {
            config: Rc::new(RefCell::new(Config {
                name: Box::new(name.to_string()),
                values: Box::new(values),
            })),
            buffer: Rc::new(RefCell::new(Vec::new())),
        }
    }

    fn process(&self) {
        let config = self.config.borrow();
        for v in config.values.iter() {
            let msg = format!("{}: {}", config.name, v);
            self.buffer.borrow_mut().push(msg);
        }
    }

    fn results(&self) -> Vec<String> {
        self.buffer.borrow().clone()
    }
}

fn main() {
    let p = Processor::new("test", vec![1, 2, 3]);
    p.process();
    for r in p.results() {
        println!("{}", r);
    }
}
```

**Pistas:**
1. ¿`Config` se comparte con alguien más?
2. ¿`buffer` se comparte con alguien más?
3. ¿`Box<String>` y `Box<Vec<i32>>` aportan algo?
4. ¿Podrías cambiar `&self` por `&mut self`?

### Ejercicio 3: Diseño desde cero

Diseña los tipos (structs y sus campos con los smart pointers apropiados) para un sistema de notificaciones con estos requisitos:

1. Un `NotificationCenter` mantiene una lista de suscriptores.
2. Cada `Subscriber` tiene un nombre y un callback (`Fn(&str)`).
3. Múltiples productores pueden enviar notificaciones al centro.
4. Los suscriptores pueden desregistrarse en cualquier momento (incluso mientras se procesan notificaciones).
5. Todo es single-thread.

```rust
// Define los tipos aquí:
// struct NotificationCenter { ... }
// struct Subscriber { ... }
// type SubscriberHandle = ...;

// Implementa:
// NotificationCenter::new()
// NotificationCenter::subscribe(name, callback) -> SubscriberHandle
// NotificationCenter::unsubscribe(handle)
// NotificationCenter::notify(message: &str)

// Pistas:
// - ¿Quién es owner de los subscribers?
// - ¿Cómo manejas que un subscriber se desregistre?
// - ¿Qué pasa si notify() está iterando y alguien llama unsubscribe()?
// - ¿Necesitas Weak para algo?
```

**Preguntas de diseño:**
1. ¿Por qué `Rc<RefCell<Vec<...>>>` podría causar panic en `notify` si un callback llama a `unsubscribe`?
2. ¿Cómo evitarías ese panic?
3. ¿Cambiaría tu diseño si el sistema fuera multi-thread?

---

> **Consejo final**: la elección del smart pointer correcto rara vez es una decisión que debas tomar *a priori*. Empieza con ownership directo y referencias. Cuando el compilador te diga que necesitas algo más (o cuando genuinamente identifiques el patrón), sube un nivel de complejidad. Rust te guía hacia la solución correcta — escucha al compilador.
