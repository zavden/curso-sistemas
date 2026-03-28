# T03 — Weak\<T\>

## El problema de los ciclos

En el topico de Rc vimos que los ciclos de Rc causan memory leaks
porque el strong_count nunca llega a 0. `Weak<T>` existe para
romper estos ciclos:

```rust
use std::rc::Rc;
use std::cell::RefCell;

// Ciclo con Rc — memory leak:
struct Node {
    next: RefCell<Option<Rc<Node>>>,
}

let a = Rc::new(Node { next: RefCell::new(None) });
let b = Rc::new(Node { next: RefCell::new(None) });

// a → b → a (ciclo)
*a.next.borrow_mut() = Some(Rc::clone(&b));
*b.next.borrow_mut() = Some(Rc::clone(&a));

// Cuando a y b salen de scope:
// a: strong_count baja de 2 a 1 (b.next aun lo apunta)
// b: strong_count baja de 2 a 1 (a.next aun lo apunta)
// Ninguno llega a 0 → los datos NUNCA se liberan
```

---

## Que es Weak\<T\>

`Weak<T>` es una referencia que **no mantiene vivo** el dato.
A diferencia de Rc (strong reference), un Weak no cuenta para
decidir si el dato se destruye:

```rust
use std::rc::{Rc, Weak};

let strong = Rc::new(42);
let weak: Weak<i32> = Rc::downgrade(&strong);

println!("strong count: {}", Rc::strong_count(&strong));  // 1
println!("weak count: {}", Rc::weak_count(&strong));       // 1

// El Weak existe pero NO mantiene vivo el dato.
// Solo el strong_count decide cuando se libera.

// Acceder via Weak — retorna Option porque el dato podria ya no existir:
let val: Option<Rc<i32>> = weak.upgrade();
assert_eq!(*val.unwrap(), 42);  // OK: el dato aun vive

// Destruir el unico Rc strong:
drop(strong);  // strong_count → 0 → dato liberado

// Ahora upgrade retorna None:
let val = weak.upgrade();
assert!(val.is_none());  // el dato ya no existe
```

```text
Strong vs Weak:

  Rc (strong):
  - Incrementa strong_count
  - Mantiene vivo el dato
  - Cuando strong_count llega a 0 → dato se destruye
  - upgrade() no necesario — siempre tienes acceso

  Weak:
  - Incrementa weak_count (separado del strong_count)
  - NO mantiene vivo el dato
  - Cuando strong_count llega a 0 → dato se destruye (aunque haya Weaks)
  - upgrade() necesario — retorna Option<Rc<T>> porque el dato podria estar muerto

  El bloque de memoria del Rc se libera cuando AMBOS counters llegan a 0.
  Pero el dato (T) se destruye cuando strong_count llega a 0.
```

---

## downgrade y upgrade

```rust
use std::rc::{Rc, Weak};

// downgrade: Rc<T> → Weak<T> (crear referencia debil)
let rc = Rc::new(String::from("hello"));
let weak: Weak<String> = Rc::downgrade(&rc);

// upgrade: Weak<T> → Option<Rc<T>> (intentar obtener acceso)
if let Some(rc) = weak.upgrade() {
    println!("alive: {rc}");   // "alive: hello"
    // rc es un Rc nuevo — incremento strong_count temporalmente
}
// El Rc temporal se destruye aqui — strong_count vuelve al original

// Si el dato fue liberado:
drop(rc);
assert!(weak.upgrade().is_none());  // "el dato ya murio"
```

```rust
// upgrade incrementa strong_count temporalmente:
use std::rc::{Rc, Weak};

let rc = Rc::new(42);
let weak = Rc::downgrade(&rc);

println!("before upgrade: {}", Rc::strong_count(&rc));  // 1

{
    let upgraded = weak.upgrade().unwrap();  // strong_count = 2
    println!("during upgrade: {}", Rc::strong_count(&rc));  // 2
}  // upgraded se destruye → strong_count = 1

println!("after upgrade: {}", Rc::strong_count(&rc));  // 1
```

---

## Romper ciclos con Weak

La regla es simple: en una relacion ciclica, una de las
direcciones debe ser Weak en vez de Rc:

### Arbol con referencia al padre

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

#[derive(Debug)]
struct Node {
    value: i32,
    parent: RefCell<Weak<Node>>,      // Weak hacia arriba (padre)
    children: RefCell<Vec<Rc<Node>>>,  // Rc hacia abajo (hijos)
}

impl Node {
    fn new(value: i32) -> Rc<Node> {
        Rc::new(Node {
            value,
            parent: RefCell::new(Weak::new()),  // sin padre inicialmente
            children: RefCell::new(vec![]),
        })
    }
}

fn main() {
    let root = Node::new(1);
    let child_a = Node::new(2);
    let child_b = Node::new(3);

    // Establecer relaciones padre-hijo:
    // Padre → hijo: Rc (strong, mantiene vivo al hijo)
    root.children.borrow_mut().push(Rc::clone(&child_a));
    root.children.borrow_mut().push(Rc::clone(&child_b));

    // Hijo → padre: Weak (no mantiene vivo al padre)
    *child_a.parent.borrow_mut() = Rc::downgrade(&root);
    *child_b.parent.borrow_mut() = Rc::downgrade(&root);

    // Acceder al padre desde un hijo:
    if let Some(parent) = child_a.parent.borrow().upgrade() {
        println!("child_a's parent: {}", parent.value);  // 1
    }

    // Contadores:
    println!("root strong: {}", Rc::strong_count(&root));    // 1 (variable root)
    println!("root weak: {}", Rc::weak_count(&root));        // 2 (child_a + child_b)
    println!("child_a strong: {}", Rc::strong_count(&child_a)); // 2 (variable + root.children)
}
// Cuando root se destruye:
// 1. root.strong_count → 0 → root se destruye
// 2. root.children se destruye → child_a y child_b pierden un Rc
// 3. child_a.parent y child_b.parent (Weak) → upgrade() retornaria None
// 4. SIN ciclo → todo se libera correctamente
```

```text
¿Por que el padre es Weak?

  Padre → Hijo:  Rc   (el padre "posee" a sus hijos)
  Hijo → Padre:  Weak (el hijo "conoce" a su padre, no lo posee)

  Si ambos fueran Rc:
    root → child → root → child → ...  (ciclo, memory leak)

  Con Weak en una direccion:
    root → child (Rc, mantiene vivo)
    child → root (Weak, NO mantiene vivo)

  Cuando root sale de scope, strong_count = 0, se destruye.
  Los hijos se destruyen en cascada. Sin leaks.
```

### Lista doblemente enlazada

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

#[derive(Debug)]
struct DNode {
    value: i32,
    next: RefCell<Option<Rc<DNode>>>,    // strong → siguiente
    prev: RefCell<Option<Weak<DNode>>>,   // weak → anterior
}

impl DNode {
    fn new(value: i32) -> Rc<DNode> {
        Rc::new(DNode {
            value,
            next: RefCell::new(None),
            prev: RefCell::new(None),
        })
    }
}

fn main() {
    let a = DNode::new(1);
    let b = DNode::new(2);
    let c = DNode::new(3);

    // Forward links (Rc):
    *a.next.borrow_mut() = Some(Rc::clone(&b));
    *b.next.borrow_mut() = Some(Rc::clone(&c));

    // Backward links (Weak):
    *b.prev.borrow_mut() = Some(Rc::downgrade(&a));
    *c.prev.borrow_mut() = Some(Rc::downgrade(&b));

    // Recorrer hacia adelante:
    print!("forward: ");
    let mut current = Some(Rc::clone(&a));
    while let Some(node) = current {
        print!("{} ", node.value);
        current = node.next.borrow().as_ref().map(Rc::clone);
    }
    println!();  // forward: 1 2 3

    // Recorrer hacia atras desde c:
    print!("backward: ");
    let mut current: Option<Rc<DNode>> = Some(Rc::clone(&c));
    while let Some(node) = current {
        print!("{} ", node.value);
        current = node.prev.borrow()
            .as_ref()
            .and_then(|w| w.upgrade());
    }
    println!();  // backward: 3 2 1
}
```

### Observer pattern

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

trait Observer {
    fn on_event(&self, event: &str);
}

struct EventEmitter {
    observers: RefCell<Vec<Weak<dyn Observer>>>,
}

impl EventEmitter {
    fn new() -> Self {
        EventEmitter { observers: RefCell::new(vec![]) }
    }

    fn subscribe(&self, observer: &Rc<dyn Observer>) {
        // Guardar Weak — no queremos que el emitter mantenga vivos a los observers
        self.observers.borrow_mut().push(Rc::downgrade(observer));
    }

    fn emit(&self, event: &str) {
        let mut observers = self.observers.borrow_mut();

        // Limpiar observers muertos y notificar los vivos:
        observers.retain(|weak| {
            if let Some(observer) = weak.upgrade() {
                observer.on_event(event);
                true   // mantener en la lista
            } else {
                false  // observer ya no existe — eliminar
            }
        });
    }
}

struct Logger { prefix: String }

impl Observer for Logger {
    fn on_event(&self, event: &str) {
        println!("[{}] {event}", self.prefix);
    }
}

fn main() {
    let emitter = EventEmitter::new();

    let logger1: Rc<dyn Observer> = Rc::new(Logger { prefix: "L1".into() });
    let logger2: Rc<dyn Observer> = Rc::new(Logger { prefix: "L2".into() });

    emitter.subscribe(&logger1);
    emitter.subscribe(&logger2);

    emitter.emit("start");    // [L1] start, [L2] start

    drop(logger1);            // logger1 ya no existe
    emitter.emit("continue"); // [L2] continue (logger1 fue limpiado)
}
```

---

## Weak::new — referencia debil sin dato

```rust
use std::rc::Weak;

// Weak::new() crea un Weak que no apunta a nada:
let empty: Weak<i32> = Weak::new();
assert!(empty.upgrade().is_none());

// Util para inicializar campos que se llenan despues:
use std::cell::RefCell;

struct Child {
    parent: RefCell<Weak<Parent>>,
}

struct Parent {
    // ...
}

let child = Child {
    parent: RefCell::new(Weak::new()),  // sin padre aun
};
// Despues: *child.parent.borrow_mut() = Rc::downgrade(&some_parent);
```

---

## Weak con Arc (multi-thread)

`std::sync::Weak` funciona igual que `std::rc::Weak` pero para Arc:

```rust
use std::sync::{Arc, Weak};
use std::thread;

let strong = Arc::new(42);
let weak: Weak<i32> = Arc::downgrade(&strong);

let handle = {
    let weak = weak.clone();  // Weak es Clone
    thread::spawn(move || {
        // Intentar acceder al dato:
        match weak.upgrade() {
            Some(val) => println!("thread: {val}"),
            None => println!("thread: data is gone"),
        }
    })
};

handle.join().unwrap();
// Si strong aun existe → "thread: 42"
// Si strong fue dropeado → "thread: data is gone"
```

```text
Correspondencia:

  std::rc::Rc    ←→  std::rc::Weak      (single-thread)
  std::sync::Arc ←→  std::sync::Weak    (multi-thread)

  La API es identica. La implementacion interna usa
  atomicas en la version de sync.
```

---

## Cuando usar Weak vs alternativas

```text
¿Necesitas romper un ciclo?

  1. Weak<T>: la solucion directa para ciclos en Rc/Arc.
     Pro: flexible, funciona con cualquier estructura.
     Con: overhead de upgrade() (retorna Option), mas complejo.

  2. Indices (usize) en un Vec:
     En vez de Rc<Node>, guarda los nodos en un Vec<Node>
     y referencia por indice. Sin reference counting.
     Pro: simple, sin overhead de Rc/Weak.
     Con: el Vec no puede shrink, indices invalidados al eliminar.

  3. Arena (typed_arena, bumpalo):
     Aloca todos los nodos en una arena. Referencias directas
     con lifetime de la arena. Sin conteo.
     Pro: rapido, sin overhead.
     Con: todo se libera junto (no individual).

  4. Rediseñar sin ciclos:
     A veces el ciclo indica un diseño mejorable.
     ¿Puede el hijo no conocer al padre?
     ¿Puede pasar el padre como argumento en vez de guardarlo?

Regla general:
  - Grafos pequenos/medianos → Rc + Weak
  - Grafos grandes/performance-critical → indices en Vec o arena
  - Si puedes evitar el ciclo → evitalo
```

---

## Errores comunes

```rust
// ERROR 1: usar Weak y olvidar que upgrade puede fallar
use std::rc::{Rc, Weak};

let weak: Weak<i32>;
{
    let rc = Rc::new(42);
    weak = Rc::downgrade(&rc);
}  // rc se destruye aqui

// MAL — panic si el dato ya no existe:
// let val = weak.upgrade().unwrap();

// BIEN — manejar el None:
match weak.upgrade() {
    Some(val) => println!("alive: {val}"),
    None => println!("data no longer exists"),
}
```

```rust
// ERROR 2: usar Weak donde Rc es correcto
use std::rc::{Rc, Weak};

// Si el dato SIEMPRE debe estar vivo cuando lo usas,
// no uses Weak — usa Rc.

// MAL: Weak forzado donde no hay ciclo:
struct Config {
    db_url: String,
}

struct Service {
    config: Weak<Config>,  // ¿por que Weak? no hay ciclo
}
// Ahora cada acceso a config requiere upgrade() + unwrap()

// BIEN: si no hay ciclo, usa Rc directamente:
struct Service2 {
    config: Rc<Config>,  // simple, siempre accesible
}
```

```rust
// ERROR 3: Weak que sobrevive a todos los Rc → siempre None
use std::rc::{Rc, Weak};

fn create_weak() -> Weak<String> {
    let rc = Rc::new(String::from("temporary"));
    Rc::downgrade(&rc)
    // rc se destruye al retornar → strong_count = 0
}

let weak = create_weak();
assert!(weak.upgrade().is_none());
// El Weak es inutil — el dato murio inmediatamente.
// Asegurate de que al menos un Rc viva mientras el Weak se use.
```

```rust
// ERROR 4: acumular Weaks muertos sin limpiar
use std::rc::{Rc, Weak};

struct Emitter {
    listeners: Vec<Weak<String>>,  // podrian acumularse muertos
}

impl Emitter {
    fn notify(&mut self) {
        // MAL: iterar sin limpiar — Weaks muertos se acumulan:
        // for w in &self.listeners { ... }

        // BIEN: limpiar muertos al recorrer:
        self.listeners.retain(|w| w.upgrade().is_some());
        for w in &self.listeners {
            if let Some(s) = w.upgrade() {
                println!("{s}");
            }
        }
    }
}
```

```rust
// ERROR 5: confundir strong_count y weak_count
use std::rc::{Rc, Weak};

let rc = Rc::new(42);
let w1 = Rc::downgrade(&rc);
let w2 = Rc::downgrade(&rc);

// Los Weaks NO cuentan para strong_count:
assert_eq!(Rc::strong_count(&rc), 1);  // solo el Rc original
assert_eq!(Rc::weak_count(&rc), 2);    // w1 y w2

// El dato se destruye cuando strong_count = 0,
// independientemente de cuantos Weaks existan.
drop(rc);  // strong_count → 0 → dato destruido
assert!(w1.upgrade().is_none());
assert!(w2.upgrade().is_none());
// w1 y w2 aun existen como objetos pero apuntan a nada.
```

---

## Cheatsheet

```text
Crear:
  Rc::downgrade(&rc)        Rc<T> → Weak<T>
  Arc::downgrade(&arc)      Arc<T> → Weak<T> (multi-thread)
  Weak::new()               Weak sin dato (upgrade siempre None)

Acceder:
  weak.upgrade()             → Option<Rc<T>>  (None si dato murio)

Consultar:
  Rc::strong_count(&rc)      cuantos Rc apuntan al dato
  Rc::weak_count(&rc)        cuantos Weak apuntan al dato
  weak.strong_count()        strong_count del dato (0 si murio)
  weak.weak_count()          weak_count del dato

Regla del ciclo:
  Padre → Hijo:  Rc/Arc   (el padre posee al hijo)
  Hijo → Padre:  Weak     (el hijo conoce al padre, no lo posee)
  Forward:       Rc/Arc   (direccion principal)
  Backward:      Weak     (direccion secundaria)

Cuando se destruye el dato:
  strong_count → 0  → Drop::drop() se ejecuta, dato destruido
  weak_count → 0    → bloque de memoria liberado (metadata)
  Weaks existentes → upgrade() retorna None

Patron observer:
  Emitter guarda Vec<Weak<dyn Observer>>
  Observer tiene Rc (strong) ownership externo
  emit() hace upgrade + retain para limpiar muertos

Alternativas a Weak:
  Indices en Vec (sin Rc)
  Arena allocator (lifetime unico)
  Rediseñar sin ciclos
```

---

## Ejercicios

### Ejercicio 1 — Arbol con padre

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

// a) Define un struct TreeNode con:
//    - value: String
//    - parent: RefCell<Weak<TreeNode>>
//    - children: RefCell<Vec<Rc<TreeNode>>>
//
// b) Implementa:
//    - TreeNode::new(value) → Rc<TreeNode>
//    - TreeNode::add_child(parent: &Rc<TreeNode>, child: &Rc<TreeNode>)
//      que agregue el child y setee su parent
//
// c) Construye:
//        root
//       /    \
//     child1  child2
//               |
//             grandchild
//
// d) Desde grandchild, navega hacia arriba imprimiendo
//    cada ancestor. ¿Que imprime?
//
// e) Dropea root. ¿Se liberan todos los nodos?
//    Verifica con un impl Drop que imprima el value.
```

### Ejercicio 2 — Cache con expiracion

```rust
use std::rc::{Rc, Weak};
use std::collections::HashMap;

// Implementa un cache donde los datos se eliminan cuando
// ningun consumidor los referencia:
//
// struct Cache<T> {
//     entries: HashMap<String, Weak<T>>,
// }
//
// a) fn get(&self, key: &str) → Option<Rc<T>>
//    Intenta upgrade del Weak. Si el dato murio, elimina la entrada.
//
// b) fn insert(&mut self, key: String, value: &Rc<T>)
//    Guarda un Weak del valor.
//
// c) fn cleanup(&mut self)
//    Elimina todas las entradas muertas.
//
// d) Prueba:
//    let data = Rc::new("hello".to_string());
//    cache.insert("key1", &data);
//    assert!(cache.get("key1").is_some());
//    drop(data);
//    assert!(cache.get("key1").is_none());  // dato expirado
```

### Ejercicio 3 — Predecir strong/weak counts

```rust
use std::rc::{Rc, Weak};

// Predice los valores de strong_count y weak_count
// en cada punto marcado. Luego verifica ejecutando.

let a = Rc::new(100);
// Punto 1: strong=? weak=?

let b = Rc::clone(&a);
// Punto 2: strong=? weak=?

let w1 = Rc::downgrade(&a);
// Punto 3: strong=? weak=?

let w2 = Rc::downgrade(&b);
// Punto 4: strong=? weak=?

let c = w1.upgrade();
// Punto 5: strong=? weak=?

drop(b);
// Punto 6: strong=? weak=?

drop(c);
// Punto 7: strong=? weak=?

drop(a);
// Punto 8: strong=? weak=?
// ¿Que retorna w1.upgrade()? ¿Y w2.upgrade()?
```
