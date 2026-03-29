# T04 — Object Pool en Rust

---

## 1. De C a Rust: el problema del puntero prestado

T03 mostro el pool en C: un array contiguo + free stack, donde
`acquire` retorna un puntero al objeto dentro del pool:

```c
/* C (T03): acquire retorna puntero directo */
Buffer *buf = pool_acquire(pool);
buf->data[0] = 'A';
pool_release(pool, buf);
buf->data[0] = 'B';  /* USE AFTER FREE — pero compila */
```

En Rust, intentar esto choca con el borrow checker:

```rust
// INTENTO: pool que retorna &mut T
impl<T> Pool<T> {
    fn acquire(&mut self) -> &mut T {
        // Retorna referencia a un slot interno
        &mut self.slots[idx]
    }

    fn release(&mut self, obj: &mut T) {
        // PROBLEMA: necesito &mut self
        // pero ya hay un &mut T vivo que apunta dentro de self
        // → ERROR: cannot borrow `self` as mutable more than once
    }
}
```

```
  El problema fundamental:
  ─────────────────────────

  pool.acquire() → &mut T (referencia dentro del pool)
  │
  │  Mientras &mut T existe, el borrow checker
  │  impide CUALQUIER otra operacion en pool:
  │  - No puedes llamar pool.release()
  │  - No puedes llamar pool.acquire() de nuevo
  │  - No puedes llamar pool.available()
  │
  └─ La referencia "congela" el pool entero
```

---

## 2. Solucion: handles (indices)

En vez de retornar una referencia, retornar un **handle**
(indice opaco) que el caller usa para acceder al objeto:

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Handle(usize);

pub struct Pool<T> {
    slots: Vec<Option<T>>,
    free_stack: Vec<usize>,
}

impl<T> Pool<T> {
    pub fn new(capacity: usize) -> Self {
        let slots = (0..capacity).map(|_| None).collect();
        let free_stack = (0..capacity).collect();
        Self { slots, free_stack }
    }

    pub fn acquire(&mut self, value: T) -> Option<Handle> {
        let idx = self.free_stack.pop()?;
        self.slots[idx] = Some(value);
        Some(Handle(idx))
    }

    pub fn release(&mut self, handle: Handle) -> Option<T> {
        let value = self.slots[handle.0].take()?;
        self.free_stack.push(handle.0);
        Some(value)
    }

    pub fn get(&self, handle: Handle) -> Option<&T> {
        self.slots[handle.0].as_ref()
    }

    pub fn get_mut(&mut self, handle: Handle) -> Option<&mut T> {
        self.slots[handle.0].as_mut()
    }

    pub fn available(&self) -> usize {
        self.free_stack.len()
    }
}
```

```rust
fn main() {
    let mut pool: Pool<String> = Pool::new(4);

    let h1 = pool.acquire("hello".to_string()).unwrap();
    let h2 = pool.acquire("world".to_string()).unwrap();

    println!("{}", pool.get(h1).unwrap());  // "hello"
    println!("Available: {}", pool.available());  // 2

    pool.release(h1);
    println!("Available: {}", pool.available());  // 3

    // h1 ya no es valido, pero el compilador no lo sabe:
    println!("{:?}", pool.get(h1));  // None (slot fue liberado)
}
```

### 2.1 Comparacion con C

```
  C (T03):                          Rust:
  ──────                            ─────
  void *pool_acquire(Pool *p)       fn acquire(&mut self, val: T) -> Handle
  → Retorna puntero directo         → Retorna indice opaco

  buf->data[0] = 'A'               pool.get_mut(h).unwrap().field = value
  → Acceso directo                  → Acceso indirecto via handle

  pool_release(pool, buf)           pool.release(handle)
  → Caller puede seguir usando     → get(handle) retorna None
    buf → use-after-free

  El handle no es un puntero: no puedes desreferenciarlo.
  Solo el pool sabe convertir handle → &T.
```

### 2.2 Por que el pool recibe el valor en acquire

En C, `acquire` retorna memoria sin inicializar y el caller la llena.
En Rust, el caller pasa el valor y el pool lo almacena:

```rust
// C-style: pool retorna slot vacio, caller llena
let h = pool.acquire_empty()?;    // ¿Que tipo tiene el slot vacio?
pool.get_mut(h).unwrap().name = "Alice";  // ← parcialmente inicializado

// Rust-style: caller crea valor completo, pool lo almacena
let h = pool.acquire(User { name: "Alice".into(), age: 30 })?;
// El valor siempre esta completamente inicializado
```

Rust no permite valores parcialmente inicializados (sin unsafe).
Pasar el valor completo en `acquire` es mas natural.

---

## 3. Handle con generacion: detectar use-after-free

El pool basico tiene un problema: un handle viejo puede coincidir
con un slot reutilizado:

```rust
let h1 = pool.acquire("first".into()).unwrap();   // Handle(0)
pool.release(h1);
let h2 = pool.acquire("second".into()).unwrap();   // Handle(0) de nuevo!

// h1 y h2 son el mismo indice
// pool.get(h1) retorna "second" — INCORRECTO!
// h1 deberia ser invalido despues de release
```

Solucion: agregar un **numero de generacion** al handle:

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Handle {
    index: usize,
    generation: u64,
}

struct Slot<T> {
    value: Option<T>,
    generation: u64,
}

pub struct Pool<T> {
    slots: Vec<Slot<T>>,
    free_stack: Vec<usize>,
}

impl<T> Pool<T> {
    pub fn new(capacity: usize) -> Self {
        let slots = (0..capacity)
            .map(|_| Slot { value: None, generation: 0 })
            .collect();
        let free_stack = (0..capacity).collect();
        Self { slots, free_stack }
    }

    pub fn acquire(&mut self, value: T) -> Option<Handle> {
        let idx = self.free_stack.pop()?;
        self.slots[idx].value = Some(value);
        // La generacion ya se incremento en release (o es 0 initial)
        Some(Handle {
            index: idx,
            generation: self.slots[idx].generation,
        })
    }

    pub fn release(&mut self, handle: Handle) -> Option<T> {
        let slot = &mut self.slots[handle.index];

        // Verificar que el handle es de la generacion correcta
        if slot.generation != handle.generation {
            return None;  // Handle obsoleto
        }

        let value = slot.value.take()?;
        slot.generation += 1;  // Incrementar: invalida handles viejos
        self.free_stack.push(handle.index);
        Some(value)
    }

    pub fn get(&self, handle: Handle) -> Option<&T> {
        let slot = &self.slots[handle.index];
        if slot.generation != handle.generation {
            return None;  // Handle obsoleto
        }
        slot.value.as_ref()
    }

    pub fn get_mut(&mut self, handle: Handle) -> Option<&mut T> {
        let slot = &mut self.slots[handle.index];
        if slot.generation != handle.generation {
            return None;
        }
        slot.value.as_mut()
    }

    pub fn available(&self) -> usize {
        self.free_stack.len()
    }
}
```

```
  Generaciones:
  ─────────────

  acquire("first")  → Handle { index: 0, generation: 0 }
  slot[0].generation = 0

  release(h1)       → slot[0].generation se incrementa a 1
                      h1 tiene generation 0 → ya no coincide

  acquire("second") → Handle { index: 0, generation: 1 }
  slot[0].generation = 1

  get(h1)           → h1.generation(0) != slot[0].generation(1)
                      → return None  ✓ Detectado!

  get(h2)           → h2.generation(1) == slot[0].generation(1)
                      → return Some("second")  ✓ Correcto!
```

```rust
fn main() {
    let mut pool: Pool<String> = Pool::new(4);

    let h1 = pool.acquire("first".into()).unwrap();
    pool.release(h1);

    let h2 = pool.acquire("second".into()).unwrap();

    // h1 y h2 tienen el mismo index pero diferente generacion
    assert_eq!(pool.get(h1), None);                    // Obsoleto
    assert_eq!(pool.get(h2), Some(&"second".into()));  // Valido
}
```

Este patron se usa extensivamente en game engines (ECS: Entity
Component System) y en crates como `slotmap` y `thunderdome`.

---

## 4. El crate slotmap

`slotmap` implementa exactamente este patron, optimizado y probado:

```rust
use slotmap::{SlotMap, DefaultKey};

fn main() {
    let mut sm = SlotMap::new();

    let k1 = sm.insert("hello");    // DefaultKey (index + generation)
    let k2 = sm.insert("world");

    println!("{}", sm[k1]);  // "hello"
    println!("{}", sm[k2]);  // "world"

    sm.remove(k1);

    // k1 ya no es valido
    assert!(!sm.contains_key(k1));
    assert_eq!(sm.get(k1), None);

    // Insertar reutiliza el slot, pero nueva generacion
    let k3 = sm.insert("new");
    assert_ne!(k1, k3);  // Diferente generacion
}
```

### 4.1 Tipos de SlotMap

```
  Tipo            Acceso    Iteracion    Memoria      Uso ideal
  ────            ──────    ─────────    ───────      ─────────
  SlotMap         O(1)      O(capacity)  Sparse       General
  DenseSlotMap    O(1)*     O(len)       Dense        Iteracion frecuente
  HopSlotMap      O(1)      O(len)       Dense        Equilibrio

  * DenseSlotMap usa un nivel de indirecion extra
```

```rust
use slotmap::{DenseSlotMap, new_key_type};

// Key tipado para type safety
new_key_type! {
    pub struct EntityKey;
    pub struct ComponentKey;
}

struct Entity {
    name: String,
    health: i32,
}

fn main() {
    let mut entities: DenseSlotMap<EntityKey, Entity> = DenseSlotMap::with_key();

    let player = entities.insert(Entity {
        name: "Player".into(),
        health: 100,
    });

    let enemy = entities.insert(Entity {
        name: "Goblin".into(),
        health: 30,
    });

    // Iteracion eficiente (solo sobre entidades vivas)
    for (key, entity) in &entities {
        println!("{}: {} HP", entity.name, entity.health);
    }

    entities.remove(enemy);

    // Iteracion sigue siendo eficiente — sin "huecos"
    for (_, entity) in &entities {
        println!("Still alive: {}", entity.name);
    }
}
```

### 4.2 Keys tipados: evitar mezclar pools

```rust
new_key_type! {
    pub struct EnemyKey;
    pub struct BulletKey;
}

let mut enemies: SlotMap<EnemyKey, Enemy> = SlotMap::with_key();
let mut bullets: SlotMap<BulletKey, Bullet> = SlotMap::with_key();

let e = enemies.insert(Enemy { health: 50 });
let b = bullets.insert(Bullet { damage: 10 });

// ERROR DE COMPILACION:
// enemies.get(b);  // b es BulletKey, enemies espera EnemyKey
// bullets.get(e);  // e es EnemyKey, bullets espera BulletKey
```

En C, esto no se puede prevenir — ambos son `void *` o indices `size_t`.

---

## 5. Arena allocator: typed-arena

Para objetos que se crean y se liberan todos juntos (como la arena
pool de T03 seccion 7):

```rust
use typed_arena::Arena;

struct Node {
    value: i32,
    children: Vec<&'static Node>,  // Lo ajustaremos abajo
}

fn main() {
    let arena = Arena::new();

    // Todos los nodos viven en la arena
    let a = arena.alloc(Node { value: 1, children: vec![] });
    let b = arena.alloc(Node { value: 2, children: vec![] });
    let c = arena.alloc(Node { value: 3, children: vec![a, b] });

    println!("c.children: {:?}",
             c.children.iter().map(|n| n.value).collect::<Vec<_>>());
    // [1, 2]

    // Cuando `arena` se destruye (Drop), TODOS los nodos
    // se liberan de una vez — sin release individual
}
```

### 5.1 Arena con referencias internas

El caso real: nodos de un arbol que se referencian entre si:

```rust
use typed_arena::Arena;
use std::cell::Cell;

struct TreeNode<'arena> {
    value: i32,
    left: Cell<Option<&'arena TreeNode<'arena>>>,
    right: Cell<Option<&'arena TreeNode<'arena>>>,
}

fn build_tree(arena: &Arena<TreeNode<'_>>) {
    let root = arena.alloc(TreeNode {
        value: 10,
        left: Cell::new(None),
        right: Cell::new(None),
    });

    let left = arena.alloc(TreeNode {
        value: 5,
        left: Cell::new(None),
        right: Cell::new(None),
    });

    let right = arena.alloc(TreeNode {
        value: 15,
        left: Cell::new(None),
        right: Cell::new(None),
    });

    root.left.set(Some(left));
    root.right.set(Some(right));

    println!("Root: {}", root.value);
    println!("Left: {}", root.left.get().unwrap().value);
    println!("Right: {}", root.right.get().unwrap().value);
}

fn main() {
    let arena = Arena::new();
    build_tree(&arena);
    // arena.drop() libera todo el arbol
}
```

### 5.2 Comparacion con C arena

```
  C (T03 seccion 7):               Rust (typed-arena):
  ──────────────────                ───────────────────
  arena_pool_acquire(arena)         arena.alloc(value)
  → void * (caller castea)         → &T (tipado, lifetime ligado)

  arena_pool_reset(arena)           drop(arena)
  → O(1) reset, reusa memoria     → Drop, libera memoria

  Puede usar objeto post-reset     &T invalido post-drop
  → use-after-free                 → compile error

  No puede referenciar entre       &'arena T permite referencias
  objetos de la arena sin raw ptrs internas — borrow checker verifica
```

### 5.3 Arena vs Pool: cuando usar cada uno

```
  Arena:                              Pool con handles:
  ──────                              ─────────────────
  Crear muchos, destruir todos        Crear/destruir individual
  Lifetime uniforme                   Lifetimes independientes
  Sin release individual              acquire + release
  Ideal: parser AST, frame alloc      Ideal: entities, connections
  Retorna &T (referencia)             Retorna Handle (indice)
```

---

## 6. Pool thread-safe con Mutex

```rust
use std::sync::Mutex;

pub struct SharedPool<T> {
    inner: Mutex<Pool<T>>,
}

impl<T> SharedPool<T> {
    pub fn new(capacity: usize) -> Self {
        Self {
            inner: Mutex::new(Pool::new(capacity)),
        }
    }

    pub fn acquire(&self, value: T) -> Option<Handle> {
        // &self, no &mut self — multiples threads pueden llamar
        self.inner.lock().unwrap().acquire(value)
    }

    pub fn release(&self, handle: Handle) -> Option<T> {
        self.inner.lock().unwrap().release(handle)
    }

    pub fn get<F, R>(&self, handle: Handle, f: F) -> Option<R>
    where
        F: FnOnce(&T) -> R,
    {
        let pool = self.inner.lock().unwrap();
        pool.get(handle).map(f)
    }

    pub fn with_mut<F, R>(&self, handle: Handle, f: F) -> Option<R>
    where
        F: FnOnce(&mut T) -> R,
    {
        let mut pool = self.inner.lock().unwrap();
        pool.get_mut(handle).map(f)
    }

    pub fn available(&self) -> usize {
        self.inner.lock().unwrap().available()
    }
}
```

```rust
use std::sync::Arc;
use std::thread;

fn main() {
    let pool = Arc::new(SharedPool::<String>::new(10));

    let handles: Vec<_> = (0..4).map(|i| {
        let pool = Arc::clone(&pool);
        thread::spawn(move || {
            let h = pool.acquire(format!("thread-{}", i)).unwrap();
            // Acceder al valor dentro del lock
            pool.get(h, |s| println!("{}", s));
            h  // Retornar handle para release despues
        })
    }).collect();

    for h in handles {
        let handle = h.join().unwrap();
        pool.release(handle);
    }

    println!("Available: {}", pool.available());  // 10
}
```

### 6.1 Por que closures en get/with_mut

No podemos retornar `&T` desde un `Mutex`:

```rust
// IMPOSIBLE: la referencia outlive el lock
fn get(&self, handle: Handle) -> Option<&T> {
    let pool = self.inner.lock().unwrap();  // Lock adquirido
    pool.get(handle)                        // &T apunta dentro
}   // Lock liberado — &T queda dangling!

// SOLUCION: closure ejecuta dentro del lock
fn get<F, R>(&self, handle: Handle, f: F) -> Option<R>
where F: FnOnce(&T) -> R
{
    let pool = self.inner.lock().unwrap();  // Lock adquirido
    pool.get(handle).map(f)                 // f ejecuta con &T
}   // Lock liberado — f ya termino, &T ya no existe
```

En C esto no es un error de compilacion — simplemente un bug
latente que causa data races. Rust obliga a manejarlo.

---

## 7. Pool con RAII: devolucion automatica

El patron guard evita que el caller olvide `release`:

```rust
pub struct PoolGuard<'pool, T> {
    pool: &'pool mut Pool<T>,
    handle: Handle,
}

impl<T> Pool<T> {
    pub fn acquire_guard(&mut self, value: T) -> Option<PoolGuard<'_, T>> {
        let handle = self.acquire(value)?;
        Some(PoolGuard { pool: self, handle })
    }
}

impl<'pool, T> PoolGuard<'pool, T> {
    pub fn handle(&self) -> Handle {
        self.handle
    }
}

impl<T> std::ops::Deref for PoolGuard<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        self.pool.get(self.handle).unwrap()
    }
}

impl<T> std::ops::DerefMut for PoolGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.pool.get_mut(self.handle).unwrap()
    }
}

impl<T> Drop for PoolGuard<'_, T> {
    fn drop(&mut self) {
        self.pool.release(self.handle);
    }
}
```

```rust
fn main() {
    let mut pool: Pool<String> = Pool::new(4);

    {
        let mut guard = pool.acquire_guard("hello".into()).unwrap();
        // Usar como &String / &mut String (via Deref)
        guard.push_str(" world");
        println!("{}", *guard);  // "hello world"
    }
    // guard se destruye → release automatico

    println!("Available: {}", pool.available());  // 4 (devuelto)
}
```

```
  Sin guard (C-style):           Con guard (RAII):
  ────────────────────           ─────────────────
  let h = pool.acquire(v);      let guard = pool.acquire_guard(v);
  // ... usar ...                // ... usar via Deref ...
  pool.release(h);              // Drop llama release()
  // ¿Y si olvido release?      // Imposible olvidar
  // ¿Y si panic antes?         // Drop corre en unwind
```

---

## 8. Comparacion completa: C vs Rust

```
  Mecanismo C (T03)                 Equivalente Rust
  ──────────────────                ────────────────
  void* acquire()                   Handle acquire(value)
  → puntero directo, sin tipo       → indice opaco, tipado

  pool_release(pool, ptr)           pool.release(handle)
  → use-after-free posible          → get(old_handle) → None

  free list embebida (union)        Vec<Option<T>> + free stack
  → zero overhead                   → Option<T> usa same memory (niche)

  Pool con flags in_use             Handle con generacion
  → O(N) deteccion double-free     → O(1) deteccion

  Arena con reset()                 typed_arena + Drop
  → manual reset                   → automatico

  Pool con pthread_mutex            Pool con Mutex + closures
  → race si olvidas lock           → compilador obliga lock

  Slab allocator manual             slotmap crate
  → 200 lineas                     → 1 linea de import

  Lo que Rust agrega:
  ─────────────────────
  • Generics: Pool<T> funciona con cualquier tipo
  • Generaciones en handle: detecta use-after-free en O(1)
  • Keys tipados (slotmap): no puedes mezclar pools
  • RAII guard: release automatico via Drop
  • Lifetime en arena: compile error si usas &T post-drop
  • Sin unsafe en toda la implementacion
```

---

## 9. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Retornar `&mut T` de acquire | Congela el pool (borrow checker) | Retornar Handle |
| Handle sin generacion | Use-after-free no detectado | Agregar generacion al handle |
| `pool.get(h).unwrap()` sin verificar | Panic si handle invalido | Usar `if let Some(v) = pool.get(h)` |
| Retornar `&T` desde Mutex | Referencia outlive lock | Closure pattern (`get(h, \|v\| ...)`) |
| Olvidar release | Pool se agota | RAII guard con Drop |
| Pool sin capacidad para picos | Acquire retorna None | Monitorear available(), dimensionar |
| Arena para objetos con lifetime diferente | No puedes liberar individualmente | Pool con handles |
| Mezclar handles de pools diferentes | Acceso a slot incorrecto | Keys tipados (slotmap) |

---

## 10. Ejercicios

### Ejercicio 1 — Pool basico con Vec<Option<T>>

Implementa un `Pool<T>` con `Vec<Option<T>>` y `Vec<usize>` como
free stack. Metodos: `new`, `acquire`, `release`, `get`, `available`.

**Prediccion**: ¿que retorna `get` si el handle fue released?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Handle(usize);

pub struct Pool<T> {
    slots: Vec<Option<T>>,
    free_stack: Vec<usize>,
}

impl<T> Pool<T> {
    pub fn new(capacity: usize) -> Self {
        Self {
            slots: (0..capacity).map(|_| None).collect(),
            free_stack: (0..capacity).collect(),
        }
    }

    pub fn acquire(&mut self, value: T) -> Option<Handle> {
        let idx = self.free_stack.pop()?;
        self.slots[idx] = Some(value);
        Some(Handle(idx))
    }

    pub fn release(&mut self, handle: Handle) -> Option<T> {
        let value = self.slots[handle.0].take()?;
        self.free_stack.push(handle.0);
        Some(value)
    }

    pub fn get(&self, handle: Handle) -> Option<&T> {
        self.slots.get(handle.0)?.as_ref()
    }

    pub fn get_mut(&mut self, handle: Handle) -> Option<&mut T> {
        self.slots.get_mut(handle.0)?.as_mut()
    }

    pub fn available(&self) -> usize {
        self.free_stack.len()
    }
}

fn main() {
    let mut pool = Pool::new(3);

    let h1 = pool.acquire(42).unwrap();
    let h2 = pool.acquire(99).unwrap();

    assert_eq!(pool.get(h1), Some(&42));
    assert_eq!(pool.available(), 1);

    pool.release(h1);
    assert_eq!(pool.get(h1), None);  // ← Released → None
    assert_eq!(pool.available(), 2);
}
```

`get` retorna `None` despues de release porque `Option::take()`
deja `None` en el slot. Es safe, pero no detecta el caso donde
otro acquire reutilizo el slot (ejercicio 2 resuelve esto).

</details>

---

### Ejercicio 2 — Handles con generacion

Agrega numero de generacion al pool del ejercicio 1 para detectar
handles obsoletos.

**Prediccion**: ¿cuando se incrementa la generacion: en acquire
o en release?

<details>
<summary>Respuesta</summary>

En release — asi el handle viejo ya no coincide:

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Handle {
    index: usize,
    generation: u64,
}

struct Slot<T> {
    value: Option<T>,
    generation: u64,
}

pub struct Pool<T> {
    slots: Vec<Slot<T>>,
    free_stack: Vec<usize>,
}

impl<T> Pool<T> {
    pub fn new(capacity: usize) -> Self {
        Self {
            slots: (0..capacity)
                .map(|_| Slot { value: None, generation: 0 })
                .collect(),
            free_stack: (0..capacity).collect(),
        }
    }

    pub fn acquire(&mut self, value: T) -> Option<Handle> {
        let idx = self.free_stack.pop()?;
        self.slots[idx].value = Some(value);
        Some(Handle {
            index: idx,
            generation: self.slots[idx].generation,
        })
    }

    pub fn release(&mut self, handle: Handle) -> Option<T> {
        let slot = &mut self.slots[handle.index];
        if slot.generation != handle.generation {
            return None;
        }
        let value = slot.value.take()?;
        slot.generation += 1;  // Incrementar en RELEASE
        self.free_stack.push(handle.index);
        Some(value)
    }

    pub fn get(&self, handle: Handle) -> Option<&T> {
        let slot = &self.slots[handle.index];
        if slot.generation != handle.generation {
            return None;
        }
        slot.value.as_ref()
    }

    pub fn available(&self) -> usize {
        self.free_stack.len()
    }
}

fn main() {
    let mut pool = Pool::new(4);

    let h1 = pool.acquire("first".to_string()).unwrap();
    pool.release(h1);
    let h2 = pool.acquire("second".to_string()).unwrap();

    // h1 y h2 tienen mismo index pero diferente generacion
    assert_eq!(pool.get(h1), None);
    assert_eq!(pool.get(h2), Some(&"second".to_string()));
}
```

Se incrementa en **release** porque:
- Despues de release, el handle viejo debe ser invalido
- El proximo acquire del mismo slot crea un handle con la
  nueva generacion
- Si incrementaramos en acquire, el handle viejo seguiria
  valido entre release y re-acquire (inconsistente)

</details>

---

### Ejercicio 3 — Pool de conexiones

Implementa un pool de `Connection` con acquire/release. Cada
conexion tiene un id y un flag `alive`.

**Prediccion**: ¿que debes hacer en acquire si la conexion
reciclada no esta alive?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug)]
pub struct Connection {
    id: u64,
    alive: bool,
    host: String,
}

impl Connection {
    fn connect(host: &str, id: u64) -> Self {
        println!("Connecting {} (id={})", host, id);
        Self { id, alive: true, host: host.to_string() }
    }

    fn reconnect(&mut self) {
        println!("Reconnecting {} (id={})", self.host, self.id);
        self.alive = true;
    }

    fn close(&mut self) {
        println!("Closing id={}", self.id);
        self.alive = false;
    }
}

pub struct ConnPool {
    connections: Vec<Option<Connection>>,
    free_stack: Vec<usize>,
    host: String,
    next_id: u64,
}

impl ConnPool {
    pub fn new(host: &str, size: usize) -> Self {
        let mut connections = Vec::with_capacity(size);
        for i in 0..size {
            connections.push(Some(Connection::connect(host, i as u64)));
        }
        Self {
            connections,
            free_stack: (0..size).collect(),
            host: host.to_string(),
            next_id: size as u64,
        }
    }

    pub fn acquire(&mut self) -> Option<(usize, &mut Connection)> {
        let idx = self.free_stack.pop()?;
        let conn = self.connections[idx].as_mut().unwrap();

        // Re-conectar si murio
        if !conn.alive {
            conn.reconnect();
        }

        Some((idx, conn))
    }

    pub fn release(&mut self, idx: usize) {
        self.free_stack.push(idx);
    }

    pub fn available(&self) -> usize {
        self.free_stack.len()
    }
}

fn main() {
    let mut pool = ConnPool::new("db.example.com", 3);
    println!("Available: {}", pool.available());

    let (idx, conn) = pool.acquire().unwrap();
    println!("Got connection id={}", conn.id);

    // Simular que muere
    conn.alive = false;
    pool.release(idx);

    // Re-acquire: reconecta automaticamente
    let (idx2, conn2) = pool.acquire().unwrap();
    println!("Got connection id={}, alive={}", conn2.id, conn2.alive);
    pool.release(idx2);
}
```

En acquire, si `!conn.alive`, llamar `reconnect()`. Esto es
equivalente al patron de T03 seccion 8 donde el pool de
conexiones en C verificaba y re-abria conexiones muertas.

Nota: este ejemplo retorna `&mut Connection` directamente.
Funciona para uso simple, pero congela el pool (no puedes
hacer otro acquire). Para uso real, usar handles (seccion 2).

</details>

---

### Ejercicio 4 — Pool con slotmap

Reescribe el pool del ejercicio 1 usando el crate `slotmap`.

**Prediccion**: ¿cuantas lineas ahorras comparado con la
implementacion manual?

<details>
<summary>Respuesta</summary>

```rust
use slotmap::{SlotMap, DefaultKey};

fn main() {
    let mut pool = SlotMap::new();

    // acquire = insert
    let h1 = pool.insert(42);
    let h2 = pool.insert(99);

    // get
    assert_eq!(pool.get(h1), Some(&42));
    assert_eq!(pool.len(), 2);

    // release = remove
    let removed = pool.remove(h1);
    assert_eq!(removed, Some(42));

    // Handle viejo invalido (generacion no coincide)
    assert_eq!(pool.get(h1), None);

    // Reutiliza el slot
    let h3 = pool.insert(77);
    assert_ne!(h1, h3);  // Diferente generacion
    assert_eq!(pool.get(h3), Some(&77));
}
```

Lineas: ~10 vs ~60 de la implementacion manual. `SlotMap` provee:
- Generaciones automaticas
- Iteracion eficiente
- `len()`, `contains_key()`, `retain()`, `drain()`
- Keys tipados con `new_key_type!`
- Sin unsafe en la API publica

La implementacion manual es pedagogica — en produccion, usar
`slotmap` o `thunderdome`.

</details>

---

### Ejercicio 5 — Arena para AST

Usa `typed_arena::Arena` para construir un AST simple:
- `Expr::Num(i32)`
- `Expr::Add(&Expr, &Expr)`
- `Expr::Mul(&Expr, &Expr)`

Construye: `(2 + 3) * 4`

**Prediccion**: ¿por que arena es mejor que Box para un AST?

<details>
<summary>Respuesta</summary>

```rust
use typed_arena::Arena;

enum Expr<'a> {
    Num(i32),
    Add(&'a Expr<'a>, &'a Expr<'a>),
    Mul(&'a Expr<'a>, &'a Expr<'a>),
}

impl<'a> Expr<'a> {
    fn eval(&self) -> i32 {
        match self {
            Expr::Num(n) => *n,
            Expr::Add(l, r) => l.eval() + r.eval(),
            Expr::Mul(l, r) => l.eval() * r.eval(),
        }
    }
}

fn main() {
    let arena = Arena::new();

    // (2 + 3) * 4
    let two   = arena.alloc(Expr::Num(2));
    let three = arena.alloc(Expr::Num(3));
    let four  = arena.alloc(Expr::Num(4));
    let sum   = arena.alloc(Expr::Add(two, three));
    let expr  = arena.alloc(Expr::Mul(sum, four));

    println!("(2 + 3) * 4 = {}", expr.eval());  // 20
}
// Arena se destruye → todos los nodos se liberan de una vez
```

**¿Por que arena es mejor que Box para AST?**

Con Box:
```rust
enum ExprBox {
    Num(i32),
    Add(Box<ExprBox>, Box<ExprBox>),
    Mul(Box<ExprBox>, Box<ExprBox>),
}
// Cada nodo es un malloc separado
// Nodos dispersos en memoria → cache misses
// Drop recursivo: puede stack overflow en arboles profundos
```

Con arena:
- **Una sola asignacion** (la arena crece en chunks grandes)
- **Memoria contigua** → excelente localidad de cache
- **Drop en bloque** → sin recursion, sin stack overflow
- **Referencias** (`&Expr`) en vez de `Box<Expr>` → mas ligero

El trade-off: no puedes liberar nodos individualmente. Para un
AST esto es perfecto — todo el arbol vive durante el analisis
y se descarta al final.

</details>

---

### Ejercicio 6 — Pool thread-safe

Envuelve el pool con generaciones (ejercicio 2) en `Mutex`
para uso multi-thread. Usa el patron de closure para `get`.

**Prediccion**: ¿por que no puedes retornar `&T` del metodo `get`
de un `Mutex<Pool<T>>`?

<details>
<summary>Respuesta</summary>

```rust
use std::sync::{Arc, Mutex};
use std::thread;

pub struct SharedPool<T> {
    inner: Mutex<Pool<T>>,
}

impl<T> SharedPool<T> {
    pub fn new(capacity: usize) -> Self {
        Self { inner: Mutex::new(Pool::new(capacity)) }
    }

    pub fn acquire(&self, value: T) -> Option<Handle> {
        self.inner.lock().unwrap().acquire(value)
    }

    pub fn release(&self, handle: Handle) -> Option<T> {
        self.inner.lock().unwrap().release(handle)
    }

    pub fn get<F, R>(&self, handle: Handle, f: F) -> Option<R>
    where
        F: FnOnce(&T) -> R,
    {
        let pool = self.inner.lock().unwrap();
        pool.get(handle).map(f)
    }

    pub fn with_mut<F, R>(&self, handle: Handle, f: F) -> Option<R>
    where
        F: FnOnce(&mut T) -> R,
    {
        let mut pool = self.inner.lock().unwrap();
        pool.get_mut(handle).map(f)
    }
}

fn main() {
    let pool = Arc::new(SharedPool::<String>::new(8));

    let handles: Vec<_> = (0..4).map(|i| {
        let pool = Arc::clone(&pool);
        thread::spawn(move || {
            let h = pool.acquire(format!("item-{}", i)).unwrap();
            pool.get(h, |s| println!("Thread got: {}", s));
            h
        })
    }).collect();

    for jh in handles {
        let h = jh.join().unwrap();
        pool.release(h);
    }
}
```

No puedes retornar `&T` porque el `MutexGuard` (que mantiene
el lock) se destruye al final del metodo. La referencia `&T`
apunta dentro del pool, que esta protegido por el mutex.
Si retornaras `&T`, el lock se liberaria pero la referencia
seguiria viva — otro thread podria modificar el pool →
data race. El closure asegura que `&T` solo existe mientras
el lock esta activo.

</details>

---

### Ejercicio 7 — RAII guard

Implementa un `PoolGuard` que devuelve automaticamente el
objeto al pool cuando se destruye (Drop).

**Prediccion**: ¿por que el guard necesita `&mut Pool<T>`
y no `&Pool<T>`?

<details>
<summary>Respuesta</summary>

```rust
pub struct PoolGuard<'a, T> {
    pool: &'a mut Pool<T>,
    handle: Handle,
}

impl<T> Pool<T> {
    pub fn acquire_guard(&mut self, value: T) -> Option<PoolGuard<'_, T>> {
        let handle = self.acquire(value)?;
        Some(PoolGuard { pool: self, handle })
    }
}

impl<T> std::ops::Deref for PoolGuard<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        self.pool.get(self.handle).unwrap()
    }
}

impl<T> std::ops::DerefMut for PoolGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.pool.get_mut(self.handle).unwrap()
    }
}

impl<T> Drop for PoolGuard<'_, T> {
    fn drop(&mut self) {
        self.pool.release(self.handle);
    }
}

fn main() {
    let mut pool = Pool::new(4);

    {
        let mut guard = pool.acquire_guard("hello".to_string()).unwrap();
        guard.push_str(" world");  // DerefMut → &mut String
        println!("{}", *guard);     // Deref → &String → "hello world"
    }   // Drop → release automatico

    assert_eq!(pool.available(), 4);
}
```

Necesita `&mut Pool<T>` porque `release` modifica el pool
(push al free stack, write None al slot). Con `&Pool<T>` el
Drop no podria llamar release.

Limitacion: solo puedes tener un guard a la vez (el `&mut` al
pool impide crear otro). Para multiples guards simultaneos,
necesitas `RefCell` o el patron thread-safe con `Arc<Mutex>`.

</details>

---

### Ejercicio 8 — Pool que crece

Implementa un pool que duplica capacidad cuando se agota.

**Prediccion**: ¿los handles existentes siguen validos despues
del grow? ¿Por que?

<details>
<summary>Respuesta</summary>

```rust
pub struct GrowablePool<T> {
    slots: Vec<Slot<T>>,
    free_stack: Vec<usize>,
}

impl<T> GrowablePool<T> {
    pub fn new(initial_capacity: usize) -> Self {
        Self {
            slots: (0..initial_capacity)
                .map(|_| Slot { value: None, generation: 0 })
                .collect(),
            free_stack: (0..initial_capacity).collect(),
        }
    }

    pub fn acquire(&mut self, value: T) -> Handle {
        if self.free_stack.is_empty() {
            self.grow();
        }
        let idx = self.free_stack.pop().unwrap();
        self.slots[idx].value = Some(value);
        Handle {
            index: idx,
            generation: self.slots[idx].generation,
        }
    }

    fn grow(&mut self) {
        let old_cap = self.slots.len();
        let new_cap = old_cap * 2;

        // Agregar nuevos slots
        for _ in old_cap..new_cap {
            self.slots.push(Slot { value: None, generation: 0 });
        }

        // Agregar nuevos indices al free stack
        for i in old_cap..new_cap {
            self.free_stack.push(i);
        }

        eprintln!("Pool grew: {} → {}", old_cap, new_cap);
    }

    pub fn release(&mut self, handle: Handle) -> Option<T> {
        let slot = self.slots.get_mut(handle.index)?;
        if slot.generation != handle.generation {
            return None;
        }
        let value = slot.value.take()?;
        slot.generation += 1;
        self.free_stack.push(handle.index);
        Some(value)
    }

    pub fn get(&self, handle: Handle) -> Option<&T> {
        let slot = self.slots.get(handle.index)?;
        if slot.generation != handle.generation {
            return None;
        }
        slot.value.as_ref()
    }
}

fn main() {
    let mut pool = GrowablePool::new(2);

    let h1 = pool.acquire("a".to_string());
    let h2 = pool.acquire("b".to_string());
    // Pool lleno → grow al proximo acquire
    let h3 = pool.acquire("c".to_string());  // "Pool grew: 2 → 4"

    // Handles viejos siguen validos!
    assert_eq!(pool.get(h1), Some(&"a".to_string()));
    assert_eq!(pool.get(h2), Some(&"b".to_string()));
    assert_eq!(pool.get(h3), Some(&"c".to_string()));
}
```

**Si, los handles siguen validos** porque:
- Los handles son **indices** (index + generation), no punteros
- `Vec::push` puede mover el buffer interno (realloc), pero
  los indices no cambian — slot 0 sigue siendo slot 0
- En C (T03, ejercicio 8), los punteros se invalidaban con
  realloc. Aqui no hay punteros expuestos al caller

Esta es la ventaja fundamental del patron handle vs puntero
directo: los handles sobreviven a realocaciones.

</details>

---

### Ejercicio 9 — Comparar rendimiento

Escribe un benchmark que compare `Vec::push/pop` (como stack
de objetos reutilizados) vs el `Pool<T>` con handles.

**Prediccion**: ¿cual es mas rapido para acquire+use+release?
¿Por que?

<details>
<summary>Respuesta</summary>

```rust
use std::time::Instant;

struct Particle {
    x: f64, y: f64, z: f64,
    vx: f64, vy: f64, vz: f64,
}

fn bench_pool(iterations: usize) {
    let mut pool: Pool<Particle> = Pool::new(1000);

    let start = Instant::now();
    for _ in 0..iterations {
        let h = pool.acquire(Particle {
            x: 1.0, y: 2.0, z: 3.0,
            vx: 0.1, vy: 0.2, vz: 0.3,
        }).unwrap();

        // Simular uso
        if let Some(p) = pool.get_mut(h) {
            p.x += p.vx;
            p.y += p.vy;
        }

        pool.release(h);
    }
    let elapsed = start.elapsed();
    println!("Pool:      {:?} ({:.1} ns/op)",
             elapsed, elapsed.as_nanos() as f64 / iterations as f64);
}

fn bench_vec_stack(iterations: usize) {
    let mut stack: Vec<Particle> = Vec::with_capacity(1000);
    // Pre-llenar
    for _ in 0..1000 {
        stack.push(Particle {
            x: 0.0, y: 0.0, z: 0.0,
            vx: 0.0, vy: 0.0, vz: 0.0,
        });
    }

    let start = Instant::now();
    for _ in 0..iterations {
        let mut p = stack.pop().unwrap();

        // Simular uso
        p.x = 1.0; p.y = 2.0; p.z = 3.0;
        p.vx = 0.1; p.vy = 0.2; p.vz = 0.3;
        p.x += p.vx;
        p.y += p.vy;

        stack.push(p);
    }
    let elapsed = start.elapsed();
    println!("Vec stack: {:?} ({:.1} ns/op)",
             elapsed, elapsed.as_nanos() as f64 / iterations as f64);
}

fn bench_malloc(iterations: usize) {
    let start = Instant::now();
    for _ in 0..iterations {
        let mut p = Box::new(Particle {
            x: 1.0, y: 2.0, z: 3.0,
            vx: 0.1, vy: 0.2, vz: 0.3,
        });

        p.x += p.vx;
        p.y += p.vy;

        drop(p);  // free
    }
    let elapsed = start.elapsed();
    println!("Box (alloc): {:?} ({:.1} ns/op)",
             elapsed, elapsed.as_nanos() as f64 / iterations as f64);
}

fn main() {
    let n = 1_000_000;
    bench_pool(n);
    bench_vec_stack(n);
    bench_malloc(n);
}
```

Resultado tipico:
```
Pool:        ~15-20 ns/op   (index lookup + generation check)
Vec stack:   ~5-8 ns/op     (pop/push, acceso directo)
Box (alloc): ~40-80 ns/op   (malloc + free cada vez)
```

**Vec stack es mas rapido** porque:
- `pop()` retorna el valor directamente (move), no un handle
- No hay indirecion (generation check, index → slot)
- Mejor localidad (stack tip siempre caliente en cache)

**Pool gana sobre Vec stack cuando**:
- Necesitas acceso aleatorio a multiples objetos vivos
- Necesitas handles estables (para grafos, ECS)
- Vec stack solo sirve como "reciclar el ultimo liberado"

**Ambos ganan sobre Box** por eliminar malloc/free del hot path.

</details>

---

### Ejercicio 10 — ECS minimo con slotmap

Implementa un Entity Component System minimo: un `World` con
SlotMap de entidades, donde cada entidad tiene opcional Position
y Velocity. Implementa un "sistema" que actualiza posiciones.

**Prediccion**: ¿por que SlotMap es ideal para ECS?

<details>
<summary>Respuesta</summary>

```rust
use slotmap::{SlotMap, new_key_type};

new_key_type! { pub struct EntityId; }

#[derive(Debug, Clone)]
struct Position { x: f32, y: f32 }

#[derive(Debug, Clone)]
struct Velocity { vx: f32, vy: f32 }

struct World {
    entities: SlotMap<EntityId, ()>,
    positions: SlotMap<EntityId, Position>,
    velocities: SlotMap<EntityId, Velocity>,
}

impl World {
    fn new() -> Self {
        Self {
            entities: SlotMap::with_key(),
            positions: SlotMap::with_key(),
            velocities: SlotMap::with_key(),
        }
    }

    fn spawn(&mut self) -> EntityId {
        self.entities.insert(())
    }

    fn add_position(&mut self, id: EntityId, pos: Position) {
        self.positions.insert(pos);
    }

    fn add_velocity(&mut self, id: EntityId, vel: Velocity) {
        self.velocities.insert(vel);
    }

    fn despawn(&mut self, id: EntityId) {
        self.entities.remove(id);
        self.positions.remove(id);
        self.velocities.remove(id);
    }

    /// Sistema: actualiza posiciones segun velocidad
    fn system_movement(&mut self, dt: f32) {
        for (id, vel) in &self.velocities {
            if let Some(pos) = self.positions.get_mut(id) {
                pos.x += vel.vx * dt;
                pos.y += vel.vy * dt;
            }
        }
    }

    /// Sistema: imprimir estado
    fn system_debug(&self) {
        for (id, _) in &self.entities {
            let pos = self.positions.get(id);
            let vel = self.velocities.get(id);
            println!("{:?}: pos={:?} vel={:?}", id, pos, vel);
        }
    }
}

fn main() {
    let mut world = World::new();

    let player = world.spawn();
    world.add_position(player, Position { x: 0.0, y: 0.0 });
    world.add_velocity(player, Velocity { vx: 1.0, vy: 0.5 });

    let wall = world.spawn();
    world.add_position(wall, Position { x: 5.0, y: 5.0 });
    // wall no tiene Velocity → el sistema de movimiento lo ignora

    let bullet = world.spawn();
    world.add_position(bullet, Position { x: 0.0, y: 0.0 });
    world.add_velocity(bullet, Velocity { vx: 10.0, vy: 0.0 });

    println!("=== Frame 0 ===");
    world.system_debug();

    // Simular 3 frames
    for frame in 1..=3 {
        world.system_movement(1.0 / 60.0);
        println!("\n=== Frame {} ===", frame);
        world.system_debug();
    }

    // Destruir bullet
    world.despawn(bullet);
    println!("\n=== After despawn ===");
    world.system_debug();
}
```

**¿Por que SlotMap es ideal para ECS?**

1. **Insercion/borrado O(1)**: entidades se crean y destruyen
   constantemente (balas, particulas, enemigos)

2. **Handles estables**: `EntityId` no se invalida cuando otros
   se borran. En un Vec, borrar entidad 3 moveria entidad 4.

3. **Generaciones**: si guardas `EntityId` de una bala destruida
   y luego accedes, obtienes `None` (no otra entidad)

4. **Iteracion eficiente**: `DenseSlotMap` itera solo sobre
   entidades vivas, sin huecos

5. **Keys tipados**: no puedes mezclar `EntityId` con otro pool
   (compile error)

Esto es exactamente lo que hacen Bevy (`Entity`), hecs, y
legion internamente.

</details>
