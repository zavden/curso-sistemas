# T03 — Rc\<RefCell\<T\>\>

## Por que combinar Rc y RefCell

Cada uno resuelve un problema diferente. Juntos resuelven
**ownership compartido + mutabilidad**:

```text
Problema:                          Solucion:

Un solo dueño, immutable           T
Un solo dueño, mutable             mut T
Un solo dueño, heap                Box<T>
Multiples dueños, immutable        Rc<T>
Mutar via &self (Copy)             Cell<T>
Mutar via &self (cualquier T)      RefCell<T>
Multiples dueños + mutable         Rc<RefCell<T>>  ← este topico
```

```rust
use std::rc::Rc;
use std::cell::RefCell;

// Rc solo:     multiples dueños, pero solo lectura
// RefCell solo: mutabilidad via &self, pero un solo dueño
// Rc<RefCell>:  multiples dueños Y mutabilidad

let shared = Rc::new(RefCell::new(vec![1, 2, 3]));

let a = Rc::clone(&shared);
let b = Rc::clone(&shared);

// a y b pueden LEER:
println!("{:?}", a.borrow());   // [1, 2, 3]
println!("{:?}", b.borrow());   // [1, 2, 3]

// a y b pueden MUTAR:
a.borrow_mut().push(4);
println!("{:?}", b.borrow());   // [1, 2, 3, 4] — b ve el cambio
b.borrow_mut().push(5);
println!("{:?}", a.borrow());   // [1, 2, 3, 4, 5] — a ve el cambio
```

```text
Layout en memoria:

  let shared = Rc::new(RefCell::new(vec![1, 2, 3]));
  let a = Rc::clone(&shared);

  Stack:              Heap (bloque Rc):
  ┌──────────┐       ┌─────────────────────────────────┐
  │shared: ptr│──┐   │ strong_count: 2                  │
  └──────────┘  │   │ weak_count: 1                    │
  ┌──────────┐  ├──▶│ RefCell:                          │
  │ a: ptr   │──┘   │   borrow_state: 0                │
  └──────────┘       │   data: Vec { ptr, len, cap } ──│──▶ [1, 2, 3]
                     └─────────────────────────────────┘

  Rc maneja la propiedad compartida.
  RefCell maneja el acceso mutable.
  Vec almacena los datos reales.
```

---

## Patron basico — estado compartido mutable

```rust
use std::rc::Rc;
use std::cell::RefCell;

// Tipo alias para legibilidad (comun en proyectos Rust):
type SharedVec<T> = Rc<RefCell<Vec<T>>>;

fn new_shared_vec<T>() -> SharedVec<T> {
    Rc::new(RefCell::new(Vec::new()))
}

fn main() {
    let data: SharedVec<i32> = new_shared_vec();

    // Clonar Rc para compartir:
    let writer1 = Rc::clone(&data);
    let writer2 = Rc::clone(&data);
    let reader = Rc::clone(&data);

    // Escribir desde diferentes "dueños":
    writer1.borrow_mut().push(10);
    writer2.borrow_mut().push(20);
    writer1.borrow_mut().push(30);

    // Leer — todos ven el mismo estado:
    println!("{:?}", reader.borrow());  // [10, 20, 30]
}
```

---

## Ejemplos practicos

### Grafo mutable con nodos compartidos

```rust
use std::rc::Rc;
use std::cell::RefCell;

#[derive(Debug)]
struct Node {
    value: String,
    neighbors: RefCell<Vec<Rc<Node>>>,
}

impl Node {
    fn new(value: &str) -> Rc<Node> {
        Rc::new(Node {
            value: value.to_string(),
            neighbors: RefCell::new(vec![]),
        })
    }

    fn connect(a: &Rc<Node>, b: &Rc<Node>) {
        a.neighbors.borrow_mut().push(Rc::clone(b));
        b.neighbors.borrow_mut().push(Rc::clone(a));
    }
}

fn main() {
    let alice = Node::new("Alice");
    let bob = Node::new("Bob");
    let carol = Node::new("Carol");

    Node::connect(&alice, &bob);
    Node::connect(&bob, &carol);

    // Alice's neighbors:
    for n in alice.neighbors.borrow().iter() {
        println!("Alice → {}", n.value);
    }
    // Alice → Bob

    // Bob's neighbors:
    for n in bob.neighbors.borrow().iter() {
        println!("Bob → {}", n.value);
    }
    // Bob → Alice
    // Bob → Carol

    // NOTA: este grafo tiene ciclos (Alice ↔ Bob).
    // Para romperlos, usa Weak (ver T03_WeakT).
}
```

### Observer pattern — listeners mutables

```rust
use std::rc::Rc;
use std::cell::RefCell;

type Callback = Box<dyn Fn(i32)>;

struct Button {
    on_click: RefCell<Vec<Callback>>,
}

impl Button {
    fn new() -> Rc<Button> {
        Rc::new(Button {
            on_click: RefCell::new(vec![]),
        })
    }

    fn add_listener(&self, f: impl Fn(i32) + 'static) {
        self.on_click.borrow_mut().push(Box::new(f));
    }

    fn click(&self, value: i32) {
        for callback in self.on_click.borrow().iter() {
            callback(value);
        }
    }
}

fn main() {
    let counter = Rc::new(RefCell::new(0));

    let btn = Button::new();

    // Listener 1: imprime
    btn.add_listener(|v| println!("clicked: {v}"));

    // Listener 2: acumula en un contador compartido
    let counter_clone = Rc::clone(&counter);
    btn.add_listener(move |v| {
        *counter_clone.borrow_mut() += v;
    });

    btn.click(5);
    btn.click(3);

    println!("total: {}", counter.borrow());  // 8
}
```

### Estado compartido entre componentes

```rust
use std::rc::Rc;
use std::cell::RefCell;

#[derive(Debug)]
struct AppState {
    user: Option<String>,
    theme: String,
    notifications: Vec<String>,
}

impl AppState {
    fn new() -> Rc<RefCell<AppState>> {
        Rc::new(RefCell::new(AppState {
            user: None,
            theme: "light".into(),
            notifications: vec![],
        }))
    }
}

struct Header {
    state: Rc<RefCell<AppState>>,
}

impl Header {
    fn render(&self) {
        let state = self.state.borrow();
        let user = state.user.as_deref().unwrap_or("Guest");
        println!("=== Header: {user} | Theme: {} ===", state.theme);
    }
}

struct Sidebar {
    state: Rc<RefCell<AppState>>,
}

impl Sidebar {
    fn render(&self) {
        let state = self.state.borrow();
        println!("--- Notifications ({}) ---", state.notifications.len());
        for n in &state.notifications {
            println!("  - {n}");
        }
    }

    fn add_notification(&self, msg: &str) {
        self.state.borrow_mut().notifications.push(msg.into());
    }
}

fn main() {
    let state = AppState::new();

    let header = Header { state: Rc::clone(&state) };
    let sidebar = Sidebar { state: Rc::clone(&state) };

    // Login:
    state.borrow_mut().user = Some("Alice".into());
    sidebar.add_notification("Welcome, Alice!");
    sidebar.add_notification("You have 3 new messages");

    header.render();
    // === Header: Alice | Theme: light ===
    sidebar.render();
    // --- Notifications (2) ---
    //   - Welcome, Alice!
    //   - You have 3 new messages

    // Cambiar tema:
    state.borrow_mut().theme = "dark".into();
    header.render();
    // === Header: Alice | Theme: dark ===
}
```

---

## Orden de las capas: Rc por fuera, RefCell por dentro

```text
¿Por que Rc<RefCell<T>> y no RefCell<Rc<T>>?

  Rc<RefCell<T>>:
  - Rc maneja cuantos dueños hay (clone/drop)
  - RefCell maneja el acceso al dato (borrow/borrow_mut)
  - clone() clona el Rc (barato) → ambos apuntan al MISMO RefCell
  - Todos los clones ven los mismos datos

  RefCell<Rc<T>>:
  - RefCell envuelve un Rc
  - borrow_mut() te da &mut Rc<T>
  - Podrias reemplazar el Rc entero (apuntar a OTRO dato)
  - Pero no podrias mutar el dato al que apunta el Rc
  - Semantica confusa — rara vez lo que quieres

  Regla: Rc va FUERA, RefCell va DENTRO.
         Rc<RefCell<T>> — siempre en este orden.
```

```rust
// CORRECTO: Rc<RefCell<T>>
use std::rc::Rc;
use std::cell::RefCell;

let shared = Rc::new(RefCell::new(vec![1, 2, 3]));
let clone = Rc::clone(&shared);

clone.borrow_mut().push(4);
// shared y clone ven el mismo Vec modificado:
assert_eq!(*shared.borrow(), vec![1, 2, 3, 4]);

// RARO: RefCell<Rc<T>>
let data = Rc::new(vec![1, 2, 3]);
let cell = RefCell::new(Rc::clone(&data));

// Puedes reemplazar el Rc entero:
*cell.borrow_mut() = Rc::new(vec![99, 100]);
// Ahora cell apunta a un Vec DIFERENTE
// data original no cambio: assert_eq!(*data, vec![1, 2, 3]);
```

---

## RefCell dentro de structs en Rc

El patron mas comun es poner RefCell solo en los campos que
necesitan mutabilidad, no envolver todo el struct:

```rust
use std::rc::Rc;
use std::cell::RefCell;

// MAL — todo el struct en RefCell (granularidad gruesa):
struct UserV1 {
    name: String,
    score: i32,
}
type SharedUserV1 = Rc<RefCell<UserV1>>;

// Para leer name necesitas borrow(), que bloquea score tambien.
// Para mutar score necesitas borrow_mut(), que bloquea name tambien.

// BIEN — RefCell solo donde se necesita (granularidad fina):
struct UserV2 {
    name: String,             // immutable despues de creacion
    score: RefCell<i32>,      // mutable via &self
    history: RefCell<Vec<String>>,  // mutable via &self
}
type SharedUserV2 = Rc<UserV2>;

// Puedes leer name y mutar score simultaneamente:
fn update_user(user: &UserV2) {
    println!("updating {}", user.name);         // no borrow necesario
    *user.score.borrow_mut() += 10;              // borrow_mut solo en score
    user.history.borrow_mut().push("scored".into()); // independiente de score
}
```

```text
Granularidad de RefCell:

  Rc<RefCell<Struct>>:
  - Todo o nada — un borrow_mut bloquea todo
  - Simple pero inflexible
  - Util si siempre accedes a todo junto

  Rc<Struct con RefCell en campos>:
  - Campos mutables independientes
  - Menos riesgo de panic (borrows no se estorban)
  - Mas verbose pero mas seguro
  - Preferible en la mayoria de casos
```

---

## Equivalencia multi-thread

```text
Single-thread          →  Multi-thread

Rc<RefCell<T>>         →  Arc<Mutex<T>>
Rc<RefCell<T>>         →  Arc<RwLock<T>>
Rc<Cell<T>>            →  Arc<AtomicXxx>

Diferencias:
  RefCell panic si se viola  →  Mutex bloquea hasta obtener acceso
  RefCell es zero-lock       →  Mutex usa OS primitives
  Rc::clone es non-atomic   →  Arc::clone es atomic
```

```rust
// Convertir de single-thread a multi-thread:

// Single-thread:
use std::rc::Rc;
use std::cell::RefCell;

let data = Rc::new(RefCell::new(vec![1, 2, 3]));
let clone = Rc::clone(&data);
clone.borrow_mut().push(4);

// Multi-thread (misma logica):
use std::sync::{Arc, Mutex};

let data = Arc::new(Mutex::new(vec![1, 2, 3]));
let clone = Arc::clone(&data);
clone.lock().unwrap().push(4);

// El patron es el mismo — cambiar Rc→Arc y RefCell→Mutex.
// borrow_mut() → lock().unwrap()
// borrow() → lock().unwrap() (Mutex no distingue lectura/escritura)
```

---

## Errores comunes

```rust
// ERROR 1: panic por borrow cruzado entre Rc clones
use std::rc::Rc;
use std::cell::RefCell;

let shared = Rc::new(RefCell::new(vec![1, 2, 3]));
let a = Rc::clone(&shared);
let b = Rc::clone(&shared);

let r = a.borrow();         // borrow via a
// b.borrow_mut().push(4);  // PANIC — mismo RefCell, via b
// Todos los Rc apuntan al MISMO RefCell.
// Un borrow via cualquier Rc afecta a todos.

drop(r);
b.borrow_mut().push(4);    // OK ahora
```

```rust
// ERROR 2: borrow_mut dentro de un iterador sobre borrow
use std::rc::Rc;
use std::cell::RefCell;

let items = Rc::new(RefCell::new(vec![1, 2, 3]));

// MAL:
// for item in items.borrow().iter() {
//     items.borrow_mut().push(item * 2);  // PANIC: ya hay borrow activo
// }

// BIEN: separar lectura y escritura
let to_add: Vec<i32> = items.borrow().iter().map(|x| x * 2).collect();
items.borrow_mut().extend(to_add);
```

```rust
// ERROR 3: RefCell<Rc<T>> en vez de Rc<RefCell<T>>
use std::rc::Rc;
use std::cell::RefCell;

// MAL — semantica confusa:
let data = RefCell::new(Rc::new(vec![1, 2, 3]));
// borrow_mut te da &mut Rc — puedes reemplazar el Rc
// pero NO puedes mutar el Vec

// BIEN:
let data = Rc::new(RefCell::new(vec![1, 2, 3]));
// borrow_mut te da &mut Vec — puedes mutar el contenido
data.borrow_mut().push(4);
```

```rust
// ERROR 4: olvidar que Rc<RefCell<T>> no es thread-safe
use std::rc::Rc;
use std::cell::RefCell;

let data = Rc::new(RefCell::new(42));

// std::thread::spawn(move || {
//     *data.borrow_mut() += 1;
// });
// error: Rc<RefCell<i32>> cannot be sent between threads safely

// Solucion: Arc<Mutex<T>>
use std::sync::{Arc, Mutex};
let data = Arc::new(Mutex::new(42));
std::thread::spawn(move || {
    *data.lock().unwrap() += 1;
});
```

```rust
// ERROR 5: Rc<RefCell<T>> para todo en vez de analizar si es necesario

// ¿Necesitas Rc? ¿O basta con ownership normal + &T?
// ¿Necesitas RefCell? ¿O puedes usar &mut self?
//
// Rc<RefCell<T>> es la opcion mas flexible pero tambien la mas costosa
// en complejidad. Antes de usarlo, preguntate:
//
// 1. ¿Hay realmente multiples dueños? Si no → Box<T> o T
// 2. ¿Necesitas mutar via &self? Si no → Rc<T>
// 3. ¿El campo mutable es Copy? Si si → Rc<T> con Cell en el campo
// 4. Solo si necesitas ownership compartido + mutabilidad non-Copy → Rc<RefCell<T>>
```

---

## Cheatsheet

```text
El patron:
  Rc<RefCell<T>>  — ownership compartido + mutabilidad (single-thread)
  Arc<Mutex<T>>   — lo mismo pero multi-thread

Orden:
  Rc por FUERA, RefCell por DENTRO — siempre
  Rc<RefCell<T>>  ✓
  RefCell<Rc<T>>  ✗ (rara vez lo que quieres)

Crear:
  let shared = Rc::new(RefCell::new(value));
  let clone = Rc::clone(&shared);

Leer:
  shared.borrow()          → Ref<T>
  clone.borrow()           → Ref<T> (mismo dato)

Mutar:
  shared.borrow_mut()      → RefMut<T>
  clone.borrow_mut()       → RefMut<T> (mismo dato)

Granularidad:
  Rc<RefCell<Struct>>               todo el struct en RefCell (grueso)
  Rc<Struct { field: RefCell<T> }>  campos individuales (fino, preferible)

Equivalencias:
  Rc<RefCell<T>>   →  Arc<Mutex<T>>     (multi-thread)
  Rc<Cell<T>>      →  Arc<AtomicXxx>    (multi-thread, Copy)
  borrow()         →  lock().unwrap()   (Mutex)
  borrow_mut()     →  lock().unwrap()   (Mutex, mismo metodo)
  borrow()         →  read().unwrap()   (RwLock, lectura)
  borrow_mut()     →  write().unwrap()  (RwLock, escritura)

Arbol de decision:
  ¿Un solo dueño?         → T o Box<T>
  ¿Multiples, read-only?  → Rc<T>
  ¿Multiples, mutar Copy? → Rc<T> con Cell en campos
  ¿Multiples, mutar any?  → Rc<RefCell<T>> o Rc<T> con RefCell en campos
  ¿Multi-thread?          → Arc<Mutex<T>> o Arc<RwLock<T>>
```

---

## Ejercicios

### Ejercicio 1 — Estado compartido

```rust
use std::rc::Rc;
use std::cell::RefCell;

// Implementa un sistema de inventario simple:
//
// struct Inventory {
//     items: Vec<(String, u32)>,  // (nombre, cantidad)
// }
//
// a) Crea un Rc<RefCell<Inventory>> compartido entre
//    un Shop y un Warehouse:
//
//    struct Shop { inventory: Rc<RefCell<Inventory>> }
//    struct Warehouse { inventory: Rc<RefCell<Inventory>> }
//
// b) Shop::sell(&self, item: &str, qty: u32)
//    Decrementa la cantidad. Si no hay suficiente, no vende.
//
// c) Warehouse::restock(&self, item: &str, qty: u32)
//    Incrementa la cantidad.
//
// d) Verifica:
//    - Warehouse restockea "widget" x 100
//    - Shop vende "widget" x 30
//    - Ambos ven el inventario con 70 widgets
```

### Ejercicio 2 — Granularidad fina

```rust
use std::rc::Rc;
use std::cell::RefCell;

// Refactoriza este codigo para usar RefCell solo donde se necesita:
//
// struct Player {
//     name: String,       // nunca cambia
//     hp: i32,            // cambia frecuentemente
//     inventory: Vec<String>,  // cambia a veces
//     id: u64,            // nunca cambia
// }
// type SharedPlayer = Rc<RefCell<Player>>;
//
// a) Rediseña Player para que solo los campos mutables
//    esten en RefCell (o Cell si es Copy).
//
// b) Implementa:
//    - take_damage(&self, amount: i32)
//    - add_item(&self, item: &str)
//    - info(&self) → String  (sin borrow_mut)
//
// c) ¿Puedes leer name mientras mutas hp? ¿Y con la
//    version original Rc<RefCell<Player>>?
```

### Ejercicio 3 — Convertir a multi-thread

```rust
// Dado este codigo single-thread:

use std::rc::Rc;
use std::cell::RefCell;

struct Counter {
    value: Rc<RefCell<i32>>,
}

impl Counter {
    fn new() -> Self {
        Counter { value: Rc::new(RefCell::new(0)) }
    }

    fn increment(&self) {
        *self.value.borrow_mut() += 1;
    }

    fn get(&self) -> i32 {
        *self.value.borrow()
    }

    fn clone_counter(&self) -> Self {
        Counter { value: Rc::clone(&self.value) }
    }
}

// a) Convierte este codigo para que funcione en multi-thread.
//    Cambia los tipos necesarios.
//
// b) Crea 4 threads, cada uno incrementa 1000 veces.
//    El resultado final debe ser 4000.
//
// c) ¿Que metodo reemplaza a borrow_mut() en la version
//    multi-thread? ¿Que diferencia hay en comportamiento?
//
// d) ¿Necesitas cambiar la logica ademas de los tipos?
```
