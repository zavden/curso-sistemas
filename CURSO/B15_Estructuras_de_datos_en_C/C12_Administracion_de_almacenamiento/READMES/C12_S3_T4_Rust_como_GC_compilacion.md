# T04 — Rust como GC en compilación

## Objetivo

Comprender como el sistema de **ownership** de Rust cumple el mismo
rol que un garbage collector pero en **tiempo de compilación**: tracking
de alcanzabilidad (ownership + borrowing), liberación determinista
(`Drop`), prevención de ciclos (borrow checker), todo sin overhead
en tiempo de ejecución. Comparar cada concepto de GC con su equivalente
en Rust.

---

## 1. El problema que resuelven los GC

### 1.1 Recapitulación

A lo largo de S03 hemos visto tres estrategias de GC:

| Tópico | Estrategia | Problema que resuelve |
|--------|-----------|----------------------|
| T01 | Reference counting | Saber cuando nadie usa un objeto |
| T02 | Mark-and-sweep | Detectar objetos inalcanzables (incluidos ciclos) |
| T03 | Generacional | Hacer la colección eficiente (mayoría muere joven) |

Todas resuelven la misma pregunta fundamental:

> **¿Cuándo es seguro liberar esta memoria?**

Los GC responden en **runtime**: recorren el grafo de objetos,
cuentan referencias, o marcan desde las raíces. Esto tiene un costo:
pausas, overhead de CPU, metadata por objeto, write barriers.

### 1.2 La propuesta de Rust

Rust responde la misma pregunta en **compile time**:

> El compilador analiza estáticamente el flujo de datos y determina
> exactamente cuándo cada valor deja de ser usado. Inserta la
> liberación (`drop`) automáticamente en el punto preciso.

**Sin runtime overhead. Sin pausas. Sin metadata por objeto.**

---

## 2. Mapping: conceptos de GC → Rust

### 2.1 Tabla de equivalencias

| Concepto GC | Rust equivalente | Cuándo se resuelve |
|-------------|-----------------|:-:|
| Root set (raíces) | Variables en el stack | Compile time |
| Reachability (alcanzabilidad) | Ownership + borrows | Compile time |
| Mark (marcar vivos) | Borrow checker analiza lifetimes | Compile time |
| Sweep (liberar muertos) | `Drop::drop()` al salir del scope | Runtime (determinista) |
| Cycle detection | Borrow checker prohíbe ciclos de ownership | Compile time |
| Write barrier | `&mut` exclusivo (no hay escrituras concurrentes) | Compile time |
| Generational hypothesis | Stack allocation (valores efímeros no tocan el heap) | Compile time |
| Compaction | No necesaria (sin fragmentación por lifetime management) | N/A |

### 2.2 Análisis detallado

**Root set → Variables del stack**

En un GC, las raíces son los punteros en el stack, globales y
registros. En Rust, toda variable en el stack es propietaria de su
valor. Cuando la variable sale del scope, el valor se libera.

```
GC:    root_set = {stack_vars, globals, registers}
Rust:  fn main() { let x = ... }  // x es "raíz"
```

**Reachability → Ownership**

En GC, un objeto es alcanzable si existe un camino desde las raíces.
En Rust, un valor es "alcanzable" si tiene un propietario (owner).
Cada valor tiene **exactamente un** owner.

```
GC:    root -> A -> B -> C   (todos alcanzables)
Rust:  let a = Box::new(A { b: Box::new(B { c: Box::new(C) }) });
       // a owns A, A owns B, B owns C
       // drop(a) libera toda la cadena
```

**Mark → Borrow checker**

El borrow checker realiza un análisis de lifetimes en compile time,
determinando exactamente cuándo cada referencia es válida. Esto es
equivalente a un "mark" estático que determina alcanzabilidad sin
ejecutar código.

**Sweep → Drop**

`Drop::drop()` se ejecuta automáticamente cuando un valor sale del
scope. Es determinista — sabemos exactamente cuándo se ejecuta, sin
pausas impredecibles.

```
GC:    [programa]  [pausa]  [sweep]  [programa]
Rust:  [programa con drops intercalados — sin pausa global]
```

---

## 3. Ownership: el "mark" estático

### 3.1 Reglas de ownership

1. Cada valor tiene exactamente **un** owner.
2. Cuando el owner sale del scope, el valor se libera (**drop**).
3. El ownership se puede **transferir** (move), pero nunca duplicar*.

*Excepto tipos `Copy` (enteros, floats, etc.) que se copian bit a bit.

### 3.2 Move = transferencia de raíz

Cuando hacemos un move, estamos transfiriendo la "raíz" de un
subgrafo de objetos:

```
let a = vec![1, 2, 3];     // a es raíz del Vec
let b = a;                  // transferir raíz: a → b
// a ya no es válido (el compilador lo prohíbe)
// b es la nueva y única raíz
```

En términos de GC:
- Antes del move: `a` es raíz, Vec es alcanzable vía `a`.
- Después del move: `b` es raíz, Vec es alcanzable vía `b`.
- `a` ya no existe — no hay dos raíces al mismo objeto (sin RC).

### 3.3 Borrows = referencias temporales controladas

Las referencias (`&T` y `&mut T`) son como "punteros observadores"
que el compilador controla estrictamente:

```
let data = vec![1, 2, 3];
let r = &data;          // borrow: r puede leer, data sigue siendo owner
println!("{:?}", r);    // ok
// r sale del scope — no libera nada (no es owner)
drop(data);             // data es owner, drop aquí
```

Reglas de borrowing (garantías en compile time):
- Múltiples `&T` (lecturas) **o** un solo `&mut T` (escritura).
- Nunca ambos simultáneamente.
- Referencias nunca sobreviven al owner.

Esto elimina data races sin write barriers en runtime.

---

## 4. Drop: el "sweep" determinista

### 4.1 El trait Drop

```rust
trait Drop {
    fn drop(&mut self);
}
```

Cuando un valor sale del scope, Rust llama `drop()` automáticamente.
Para tipos compuestos, Rust llama `drop()` recursivamente en todos
los campos (drop glue).

### 4.2 Orden de drop

Rust garantiza un orden **determinista** de destrucción:

1. Variables locales: en **orden inverso** de declaración (LIFO).
2. Campos de struct: en **orden de declaración**.
3. Elementos de Vec/array: en **orden de índice** (0, 1, 2...).

```rust
{
    let a = Resource::new("A");  // creado primero
    let b = Resource::new("B");  // creado segundo
}   // drop B primero, luego drop A (LIFO)
```

### 4.3 Comparación con GC

| Aspecto | GC sweep | Rust Drop |
|---------|:--------:|:---------:|
| Cuándo | Impredecible (cuando GC decide) | Exacto (fin del scope) |
| Orden | Indeterminado | Determinista (LIFO) |
| Overhead | Recorrer todo el heap | Solo los objetos que mueren |
| Pausa | Sí (STW o concurrent) | No (inline en el flujo) |
| Finalizers | Pueden no ejecutarse | Siempre se ejecutan* |
| Recursos no-memoria | Requiere finalizers | Drop maneja todo (RAII) |

*Excepto en abort/panic sin unwinding.

### 4.4 RAII — Drop para cualquier recurso

A diferencia de los GC que solo gestionan memoria, `Drop` maneja
**cualquier recurso**: archivos, sockets, locks, transacciones.

```rust
{
    let file = File::open("data.txt")?;  // abrir
    let lock = mutex.lock()?;            // adquirir
    // ... usar file y lock ...
}   // drop lock (release) luego drop file (close)
    // determinista, sin importar si hubo panic
```

En Java/Python, los finalizers no garantizan ejecución. Se requieren
patrones como try-with-resources o context managers. En Rust, Drop
lo resuelve uniformemente.

---

## 5. El borrow checker como detector de ciclos

### 5.1 Por qué los ciclos son problemáticos

En T01 vimos que RC no detecta ciclos:
```
A [rc=1] -> B [rc=1] -> A   (ciclo, leak permanente)
```

En T02 vimos que mark-and-sweep sí los detecta (pero con overhead runtime).

### 5.2 Rust previene ciclos de ownership

El sistema de ownership de Rust **prohíbe** ciclos de ownership en
compile time:

```rust
struct Node {
    next: Option<Box<Node>>,  // Box = ownership
}

let mut a = Box::new(Node { next: None });
let mut b = Box::new(Node { next: None });

a.next = Some(b);    // a owns b
// b.next = Some(a); // ERROR: a ya fue movido arriba
//                    // (move semantics impide el ciclo)
```

No se puede crear un ciclo con `Box` porque:
1. Cada `Box` tiene exactamente un owner.
2. Mover un valor invalida el origen.
3. No hay dos paths de ownership al mismo objeto.

### 5.3 Ciclos intencionales: Rc + Weak

Cuando sí necesitas compartir ownership (grafos, caches), Rust
ofrece `Rc<T>` (reference counting en runtime). Pero los ciclos con
`Rc` causan leaks — exactamente el problema de T01.

La solución: `Weak<T>` para romper ciclos:

```rust
use std::rc::{Rc, Weak};
use std::cell::RefCell;

struct Node {
    parent: RefCell<Weak<Node>>,    // weak: no impide liberación
    children: RefCell<Vec<Rc<Node>>>, // strong: mantiene vivos
}
```

Insight clave: **Rust te obliga a decidir explícitamente** dónde
están los ownership edges (strong) y dónde los observation edges
(weak). Los GC hacen esta distinción automáticamente pero con overhead.

---

## 6. Stack allocation: la "generación joven" gratis

### 6.1 La hipótesis generacional en Rust

Recordemos de T03: la mayoría de objetos mueren jóvenes. Los GC
generacionales optimizan para esto con un nursery pequeño.

Rust va más lejos: los valores efímeros **viven en el stack**, nunca
tocan el heap:

```rust
fn process(input: &str) -> usize {
    let trimmed = input.trim();        // &str — no allocación
    let parts: Vec<&str> = trimmed.split(',').collect();  // Vec en stack*
    let count = parts.len();
    count
    // parts y trimmed liberados al retornar (stack pop)
    // sin malloc, sin free, sin GC
}
```

*El Vec contiene un puntero al heap para sus elementos, pero los
`&str` dentro apuntan al string original — mínima asignación.

### 6.2 Escape analysis: implícito en Rust

En Go, el compilador usa **escape analysis** para decidir si un
objeto va al stack o al heap. En Rust, esta decisión es **explícita**:

| Patrón | Ubicación | Equivalente Go |
|--------|:---------:|:--------------:|
| `let x = 42;` | Stack | No escapa |
| `let v = vec![1,2,3];` | Stack (metadata) + heap (datos) | Puede escapar |
| `Box::new(x)` | Heap (explícito) | Escapa |
| `&x` en retorno | Compilador verifica lifetime | Escapa |

Rust no necesita escape analysis heurístico porque el programador
controla explícitamente las asignaciones, y el compilador **verifica**
que las referencias sean válidas.

---

## 7. Programa en C — Simulando ownership y Drop

Este programa demuestra cómo implementar manualmente en C los
conceptos de ownership y drop determinista que Rust proporciona
automáticamente.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Ownership + Drop simulation in C
 *
 *  C has no ownership system. We simulate it manually to show
 *  what Rust's compiler does automatically.
 * ============================================================ */

/* ---- Owned string type (simulates Box<String>) ---- */

typedef struct {
    char *data;
    int   len;
    int   dropped;
} OwnedString;

OwnedString *owned_new(const char *s) {
    OwnedString *os = malloc(sizeof(OwnedString));
    os->len = strlen(s);
    os->data = malloc(os->len + 1);
    strcpy(os->data, s);
    os->dropped = 0;
    printf("  [new]  OwnedString(%s) at %p\n", os->data, (void *)os);
    return os;
}

void owned_drop(OwnedString *os) {
    if (!os || os->dropped) return;
    printf("  [drop] OwnedString(%s) at %p\n", os->data, (void *)os);
    free(os->data);
    os->data = NULL;
    os->dropped = 1;
    free(os);
}

/* ---- Move simulation ---- */

OwnedString *owned_move(OwnedString **src) {
    OwnedString *val = *src;
    *src = NULL;  /* invalidate source — simulates move */
    printf("  [move] OwnedString at %p (source set to NULL)\n",
           (void *)val);
    return val;
}

/* ---- Resource type (simulates a file/socket) ---- */

typedef struct {
    const char *name;
    int         open;
} Resource;

Resource *resource_open(const char *name) {
    Resource *r = malloc(sizeof(Resource));
    r->name = name;
    r->open = 1;
    printf("  [open] Resource(%s)\n", name);
    return r;
}

void resource_drop(Resource *r) {
    if (!r) return;
    if (r->open) {
        printf("  [drop] Resource(%s) — closing\n", r->name);
        r->open = 0;
    }
    free(r);
}

/* ---- Struct with owned fields (Drop glue) ---- */

typedef struct {
    const char   *label;
    OwnedString  *name;
    Resource     *conn;
} Server;

Server *server_new(const char *label, const char *name, const char *res) {
    Server *s = malloc(sizeof(Server));
    s->label = label;
    s->name  = owned_new(name);
    s->conn  = resource_open(res);
    printf("  [new]  Server(%s)\n", label);
    return s;
}

void server_drop(Server *s) {
    if (!s) return;
    printf("  [drop] Server(%s) — drop fields in order:\n", s->label);
    owned_drop(s->name);   /* field 1 */
    resource_drop(s->conn); /* field 2 */
    free(s);
}

/* ============================================================
 *  Demo 1: Scope-based drop (RAII)
 *  Simulates Rust's automatic drop at end of scope.
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: Scope-based drop (RAII) ===\n");

    /* In Rust: { let a = String::from("hello"); } <- drop here */
    OwnedString *a = owned_new("hello");

    printf("  Using a: %s\n", a->data);

    /* Simulate end of scope — must call drop manually in C */
    printf("  -- end of scope --\n");
    owned_drop(a);

    printf("  In Rust, this drop is automatic and guaranteed.\n");
    printf("  In C, forgetting owned_drop() = memory leak.\n");
}

/* ============================================================
 *  Demo 2: Move semantics
 *  After move, source is invalidated.
 * ============================================================ */
void demo2(void) {
    printf("\n=== Demo 2: Move semantics ===\n");

    OwnedString *a = owned_new("world");
    printf("  a points to: %s\n", a->data);

    /* Move a -> b (a becomes NULL) */
    OwnedString *b = owned_move(&a);

    printf("  After move:\n");
    printf("    a = %s\n", a ? a->data : "NULL (moved)");
    printf("    b = %s\n", b->data);

    /* In Rust: using 'a' after move is a COMPILE ERROR */
    /* In C: a is NULL, using it would be a runtime crash */

    /* Only b needs to be dropped — a was moved */
    owned_drop(b);
}

/* ============================================================
 *  Demo 3: Drop order (LIFO)
 *  Variables dropped in reverse declaration order.
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Drop order (LIFO) ===\n");

    OwnedString *first  = owned_new("first");
    OwnedString *second = owned_new("second");
    OwnedString *third  = owned_new("third");

    printf("  -- end of scope (LIFO order) --\n");
    /* Rust drops: third, second, first */
    owned_drop(third);
    owned_drop(second);
    owned_drop(first);
}

/* ============================================================
 *  Demo 4: Drop glue (composite struct)
 *  Struct drops its fields recursively.
 * ============================================================ */
void demo4(void) {
    printf("\n=== Demo 4: Drop glue (composite struct) ===\n");

    Server *srv = server_new("web_srv", "api.example.com", "tcp:8080");

    printf("  Server is running.\n");

    printf("  -- end of scope --\n");
    server_drop(srv);
    /* Drops Server, then OwnedString(name), then Resource(conn) */

    printf("  In Rust: all fields dropped automatically,\n");
    printf("  resources released deterministically.\n");
}

/* ============================================================
 *  Demo 5: Ownership prevents double free
 *  Show how move semantics prevent double free in C.
 * ============================================================ */
void demo5(void) {
    printf("\n=== Demo 5: Ownership prevents double free ===\n");

    OwnedString *a = owned_new("critical_data");

    /* Move to b */
    OwnedString *b = owned_move(&a);

    printf("  Attempting to drop both a and b:\n");

    owned_drop(a);  /* a is NULL after move — no-op */
    owned_drop(b);  /* b is the owner — actual free */

    printf("  No double free! Move made a = NULL.\n");
    printf("  In Rust: trying to use 'a' after move doesn't compile.\n");
}

/* ============================================================
 *  Demo 6: Comparison — C manual vs Rust automatic
 *  Side-by-side showing the verbosity difference.
 * ============================================================ */
void demo6(void) {
    printf("\n=== Demo 6: C manual vs Rust automatic ===\n");

    printf("  C code (manual ownership):\n");
    printf("  ┌────────────────────────────────────────────┐\n");
    printf("  │ char *s = malloc(6);                       │\n");
    printf("  │ strcpy(s, \"hello\");                        │\n");
    printf("  │ char *t = s;    // alias! who frees?       │\n");
    printf("  │ // ...                                     │\n");
    printf("  │ free(s);        // must remember            │\n");
    printf("  │ // free(t);     // double free if uncomment │\n");
    printf("  └────────────────────────────────────────────┘\n");
    printf("\n");
    printf("  Rust code (automatic ownership):\n");
    printf("  ┌────────────────────────────────────────────┐\n");
    printf("  │ let s = String::from(\"hello\");             │\n");
    printf("  │ let t = s;    // move — s invalidated      │\n");
    printf("  │ // println!(s) // COMPILE ERROR             │\n");
    printf("  │ println!(t);   // ok                       │\n");
    printf("  │ // t dropped automatically here             │\n");
    printf("  └────────────────────────────────────────────┘\n");
    printf("\n");

    /* Demonstrate the C version */
    printf("  Running C version:\n");
    OwnedString *s = owned_new("hello");
    OwnedString *t = owned_move(&s);

    printf("  s after move: %s\n", s ? s->data : "NULL");
    printf("  t: %s\n", t->data);

    owned_drop(t);
    printf("  Only one free — no leak, no double free.\n");
}

int main(void) {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -o ownership_sim ownership_sim.c
./ownership_sim
```

### Salida esperada (extracto)

```
=== Demo 1: Scope-based drop (RAII) ===
  [new]  OwnedString(hello) at 0x...
  Using a: hello
  -- end of scope --
  [drop] OwnedString(hello) at 0x...
  In Rust, this drop is automatic and guaranteed.
  In C, forgetting owned_drop() = memory leak.

=== Demo 2: Move semantics ===
  [new]  OwnedString(world) at 0x...
  a points to: world
  [move] OwnedString at 0x... (source set to NULL)
  After move:
    a = NULL (moved)
    b = world
  [drop] OwnedString(world) at 0x...

=== Demo 4: Drop glue (composite struct) ===
  [new]  OwnedString(api.example.com) at 0x...
  [open] Resource(tcp:8080)
  [new]  Server(web_srv)
  Server is running.
  -- end of scope --
  [drop] Server(web_srv) — drop fields in order:
  [drop] OwnedString(api.example.com) at 0x...
  [drop] Resource(tcp:8080) — closing
```

---

## 8. Programa en Rust — Ownership como GC en compilación

```rust
use std::cell::RefCell;
use std::rc::{Rc, Weak};

/* ============================================================
 *  Rust's ownership as compile-time GC
 * ============================================================ */

/* ---- Custom type with Drop to trace destruction ---- */

struct Traced {
    name: String,
}

impl Traced {
    fn new(name: &str) -> Self {
        println!("  [new]  Traced({})", name);
        Traced { name: name.to_string() }
    }
}

impl Drop for Traced {
    fn drop(&mut self) {
        println!("  [drop] Traced({})", self.name);
    }
}

/* ---- Resource type (RAII) ---- */

struct Connection {
    endpoint: String,
}

impl Connection {
    fn open(endpoint: &str) -> Self {
        println!("  [open] Connection({})", endpoint);
        Connection { endpoint: endpoint.to_string() }
    }
}

impl Drop for Connection {
    fn drop(&mut self) {
        println!("  [close] Connection({})", self.endpoint);
    }
}

/* ---- Server with owned fields ---- */

struct Server {
    label: String,
    name:  Traced,
    conn:  Connection,
}

impl Server {
    fn new(label: &str, name: &str, endpoint: &str) -> Self {
        let s = Server {
            label: label.to_string(),
            name:  Traced::new(name),
            conn:  Connection::open(endpoint),
        };
        println!("  [new]  Server({})", label);
        s
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        println!("  [drop] Server({}) — fields drop after this",
                 self.label);
    }
}

/* ============================================================
 *  Demo 1: Automatic drop at scope end
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Automatic drop at scope end ===");
    {
        let a = Traced::new("hello");
        println!("  Using a: {}", a.name);
        // a dropped automatically here
    }
    println!("  After scope — a is gone, no manual free needed.");
}

/* ============================================================
 *  Demo 2: Move semantics — single owner
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Move semantics ===");

    let a = Traced::new("data");
    println!("  a owns Traced(data)");

    let b = a;  // MOVE
    println!("  After move: b owns Traced(data)");
    // println!("{}", a.name);  // COMPILE ERROR: value moved

    println!("  b.name = {}", b.name);
    // Only b is dropped — a was moved, not duplicated
}

/* ============================================================
 *  Demo 3: Drop order — LIFO for locals, declaration order
 *  for struct fields
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Drop order ===");

    println!("  Local variables (LIFO):");
    {
        let _first  = Traced::new("first");
        let _second = Traced::new("second");
        let _third  = Traced::new("third");
        println!("  -- end of scope --");
        // Drops: third, second, first (reverse order)
    }

    println!("\n  Struct fields (declaration order):");
    {
        let _srv = Server::new("web", "api.example.com", "tcp:8080");
        println!("  -- end of scope --");
        // Drops: Server::drop(), then name (Traced), then conn
    }
}

/* ============================================================
 *  Demo 4: Borrowing — references without ownership transfer
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Borrowing — references without ownership ===");

    let data = Traced::new("shared_data");

    // Multiple immutable borrows — like GC shared references
    // but verified at compile time
    let r1 = &data;
    let r2 = &data;
    println!("  r1 = {}, r2 = {} (two readers, zero overhead)",
             r1.name, r2.name);

    // Borrows end here — no reference counting needed

    // Exclusive mutable borrow
    // (can't coexist with immutable borrows)
    let data2 = Traced::new("mutable_data");
    let _r_mut = &data2;
    // No other references allowed while r_mut exists
    // This prevents data races — write barrier at compile time!

    println!("  No runtime cost for borrow tracking.");
    // data and data2 dropped at scope end
}

/* ============================================================
 *  Demo 5: Ownership prevents cycles (Box can't cycle)
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: Ownership prevents cycles ===");

    struct Node {
        name: String,
        next: Option<Box<Node>>,
    }

    impl Drop for Node {
        fn drop(&mut self) {
            println!("  [drop] Node({})", self.name);
        }
    }

    // Chain: A -> B -> C (ownership chain)
    let chain = Box::new(Node {
        name: "A".into(),
        next: Some(Box::new(Node {
            name: "B".into(),
            next: Some(Box::new(Node {
                name: "C".into(),
                next: None,
            })),
        })),
    });

    println!("  Chain: A -> B -> C (ownership)");
    println!("  Cannot create cycle with Box:");
    println!("  C.next = Some(A) would require moving A,");
    println!("  but A already owns B which owns C.");
    println!("  Compiler prevents this at compile time.\n");

    drop(chain);
    // Drops A, then B, then C — deterministic, no cycle possible
}

/* ============================================================
 *  Demo 6: Rc + Weak — explicit runtime GC when needed
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Rc + Weak — explicit shared ownership ===");

    struct GCNode {
        name: String,
        parent:   RefCell<Weak<GCNode>>,
        children: RefCell<Vec<Rc<GCNode>>>,
    }

    impl Drop for GCNode {
        fn drop(&mut self) {
            println!("  [drop] GCNode({})", self.name);
        }
    }

    // Tree: root -> child1, root -> child2
    // Children have weak ref back to parent (no cycle)
    let root = Rc::new(GCNode {
        name: "root".into(),
        parent:   RefCell::new(Weak::new()),
        children: RefCell::new(Vec::new()),
    });

    let child1 = Rc::new(GCNode {
        name: "child1".into(),
        parent:   RefCell::new(Rc::downgrade(&root)),  // weak!
        children: RefCell::new(Vec::new()),
    });

    let child2 = Rc::new(GCNode {
        name: "child2".into(),
        parent:   RefCell::new(Rc::downgrade(&root)),  // weak!
        children: RefCell::new(Vec::new()),
    });

    root.children.borrow_mut().push(Rc::clone(&child1));
    root.children.borrow_mut().push(Rc::clone(&child2));

    println!("  root strong={} weak={}",
             Rc::strong_count(&root), Rc::weak_count(&root));
    println!("  child1 strong={} weak={}",
             Rc::strong_count(&child1), Rc::weak_count(&child1));

    // Access parent via weak
    let parent = child1.parent.borrow().upgrade();
    if let Some(p) = parent {
        println!("  child1's parent: {}", p.name);
    }

    println!("\n  Dropping all references:");
    // All freed — weak refs don't prevent drop
}

/* ============================================================
 *  Demo 7: RAII — Drop manages any resource, not just memory
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: RAII — Drop for any resource ===");

    struct TempFile { path: String }
    impl TempFile {
        fn create(path: &str) -> Self {
            println!("  [create] TempFile({})", path);
            TempFile { path: path.to_string() }
        }
    }
    impl Drop for TempFile {
        fn drop(&mut self) {
            println!("  [delete] TempFile({})", self.path);
        }
    }

    struct Transaction { id: u32, committed: bool }
    impl Transaction {
        fn begin(id: u32) -> Self {
            println!("  [begin] Transaction({})", id);
            Transaction { id, committed: false }
        }
        fn commit(&mut self) {
            println!("  [commit] Transaction({})", self.id);
            self.committed = true;
        }
    }
    impl Drop for Transaction {
        fn drop(&mut self) {
            if self.committed {
                println!("  [drop] Transaction({}) — already committed", self.id);
            } else {
                println!("  [rollback] Transaction({}) — auto rollback!", self.id);
            }
        }
    }

    // Scenario 1: normal flow
    println!("  Scenario 1: normal flow");
    {
        let _tmp = TempFile::create("/tmp/work.dat");
        let mut tx = Transaction::begin(1);
        tx.commit();
        // Both dropped: file deleted, tx already committed
    }

    // Scenario 2: early return / panic
    println!("\n  Scenario 2: early return (simulated)");
    {
        let _tmp = TempFile::create("/tmp/partial.dat");
        let _tx = Transaction::begin(2);
        // Simulate early return — no commit()
        println!("  -- early return --");
        // tx auto-rollbacks, file auto-deleted
    }

    println!("\n  GC cannot do this — finalizers are unreliable.");
    println!("  Drop is deterministic: resources always cleaned up.");
}

/* ============================================================
 *  Demo 8: Zero-cost summary — GC vs Rust
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: GC vs Rust — complete comparison ===");

    println!("  +-------------------------+----------------------------+");
    println!("  | Runtime GC              | Rust (compile-time)        |");
    println!("  +-------------------------+----------------------------+");
    println!("  | Root set scan           | Stack vars (known at CT)   |");
    println!("  | Mark phase (trace heap) | Borrow checker (CT)        |");
    println!("  | Sweep (free dead)       | Drop (deterministic)       |");
    println!("  | Cycle detection         | Ownership (no cycles)      |");
    println!("  | Write barriers          | &mut exclusivity (CT)      |");
    println!("  | Nursery (young gen)     | Stack allocation           |");
    println!("  | Remembered sets         | Lifetime annotations (CT)  |");
    println!("  | Finalizers (unreliable) | Drop (always runs)         |");
    println!("  | GC thread (CPU cost)    | Zero runtime overhead      |");
    println!("  | GC pause (latency)      | Zero pauses                |");
    println!("  | GC metadata per object  | Zero per-object overhead   |");
    println!("  +-------------------------+----------------------------+");
    println!("  CT = compile time");
    println!();

    // Prove zero overhead
    let v: Vec<i32> = (0..1000).collect();
    println!("  Vec<i32> with 1000 elements:");
    println!("    sizeof(Vec<i32>) on stack = {} bytes",
             std::mem::size_of::<Vec<i32>>());
    println!("    heap: {} bytes (pure data, no GC headers)",
             v.len() * std::mem::size_of::<i32>());
    println!("    GC metadata per element: 0 bytes");
    println!("    Compare: Java int[] has 16-byte array header");
    println!("    + each Integer object has 16-byte object header");
    println!();
    println!("  Rust's ownership IS the garbage collector —");
    println!("  it just runs at compile time instead of runtime.");
}

fn main() {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    demo7();
    demo8();
}
```

### Compilar y ejecutar

```bash
rustc -o ownership_gc ownership_gc.rs
./ownership_gc
```

### Salida esperada (extracto)

```
=== Demo 1: Automatic drop at scope end ===
  [new]  Traced(hello)
  Using a: hello
  [drop] Traced(hello)
  After scope — a is gone, no manual free needed.

=== Demo 3: Drop order ===
  Local variables (LIFO):
  [new]  Traced(first)
  [new]  Traced(second)
  [new]  Traced(third)
  -- end of scope --
  [drop] Traced(third)
  [drop] Traced(second)
  [drop] Traced(first)

=== Demo 5: Ownership prevents cycles ===
  Chain: A -> B -> C (ownership)
  ...
  [drop] Node(A)
  [drop] Node(B)
  [drop] Node(C)

=== Demo 7: RAII — Drop for any resource ===
  Scenario 2: early return (simulated)
  [create] TempFile(/tmp/partial.dat)
  [begin] Transaction(2)
  -- early return --
  [rollback] Transaction(2) — auto rollback!
  [delete] TempFile(/tmp/partial.dat)
```

---

## 9. Ejercicios

### Ejercicio 1: Orden de drop

Dado el siguiente código, predice el orden exacto de los mensajes
`[drop]`:

```rust
{
    let a = Traced::new("alpha");
    let b = Traced::new("beta");
    {
        let c = Traced::new("gamma");
    }
    let d = Traced::new("delta");
}
```

<details><summary>Prediccion</summary>

1. `[drop] Traced(gamma)` — c sale del scope del bloque interno.
2. `[drop] Traced(delta)` — d sale del scope externo (LIFO: último declarado).
3. `[drop] Traced(beta)` — b sale del scope externo.
4. `[drop] Traced(alpha)` — a sale del scope externo (primero declarado, último droppeado).

El bloque interno crea un scope separado. `gamma` se droppea al cerrarse
`}` del bloque interno. Luego `delta` se crea después del bloque. Al
cerrar el scope externo, se droppean en LIFO: delta, beta, alpha.

</details>

### Ejercicio 2: Move y drop

```rust
let a = Traced::new("data");
let b = a;                    // move
let c = Traced::new("other");
```

Cuantas veces se llama `drop`? Sobre cuales valores?

<details><summary>Prediccion</summary>

**Dos veces**: `drop(c)` y `drop(b)`.

`a` fue movido a `b`, por lo que `a` ya no existe — no se droppea.
El valor "data" se droppea exactamente una vez (a través de `b`).
`c` se droppea normalmente.

Orden (LIFO): primero `c` ("other"), luego `b` ("data").

Si Rust droppeara tanto `a` como `b`, seria un **double free**. El
move transfiere ownership, previniendo esto en compile time.

</details>

### Ejercicio 3: Por que Box no puede hacer ciclos

Explica por que este código no compila:

```rust
struct Node { next: Option<Box<Node>> }
let mut a = Box::new(Node { next: None });
let mut b = Box::new(Node { next: None });
a.next = Some(b);
b.next = Some(a);  // error?
```

<details><summary>Prediccion</summary>

La línea `a.next = Some(b)` **mueve** `b` dentro de `a`. Después del
move, `b` ya no es válido. La línea `b.next = Some(a)` intenta usar
`b` después de que fue movido — **error de compilación**:
`use of moved value: b`.

Incluso si intentamos reordenar, cualquier ciclo requiere que dos
valores se posean mutuamente. Con ownership lineal (un solo owner),
esto es imposible: mover X dentro de Y invalida X, así que X no
puede poseer a Y.

Esta es la razón fundamental por la que ownership previene ciclos:
**no puedes crear una referencia circular si mover un valor invalida
el origen**.

</details>

### Ejercicio 4: Rc leak

```rust
use std::rc::Rc;
use std::cell::RefCell;

struct Node {
    next: RefCell<Option<Rc<Node>>>,
}

let a = Rc::new(Node { next: RefCell::new(None) });
let b = Rc::new(Node { next: RefCell::new(None) });
*a.next.borrow_mut() = Some(Rc::clone(&b));
*b.next.borrow_mut() = Some(Rc::clone(&a));
```

Hay memory leak? Drop se llama?

<details><summary>Prediccion</summary>

**Sí, hay memory leak. Drop NO se llama.**

Al final del scope:
- `a` se droppea: strong_count de A pasa de 2 a 1 (B aún tiene Rc a A).
- `b` se droppea: strong_count de B pasa de 2 a 1 (A aún tiene Rc a B).

Ambos tienen strong_count = 1 — ninguno llega a 0. Los objetos nunca
se liberan. Esto es exactamente el problema de ciclos de T01.

**Rust no previene leaks con Rc** — los leaks no son *unsafe* (no
causan undefined behavior). Para evitar esto, usar `Weak` en al menos
una dirección del ciclo.

</details>

### Ejercicio 5: Weak rompe el ciclo

Modifica el ejercicio 4 para que `b.next` sea `Weak` en lugar de `Rc`.
Se libera todo?

<details><summary>Prediccion</summary>

Si `b.next` usa `Weak<Node>` en lugar de `Rc<Node>`:
- `a` tiene strong_count = 1 (solo la variable `a`; `b` tiene Weak, no Strong).
- `b` tiene strong_count = 2 (variable `b` + `a.next`).

Al final del scope:
1. `b` droppea: B strong_count 2 → 1. B sigue vivo (A.next lo tiene).
2. `a` droppea: A strong_count 1 → 0 → **A se libera**.
   Al liberar A, A.next (que contiene Rc a B) se droppea.
   B strong_count 1 → 0 → **B se libera**.

**Todo se libera correctamente.** El Weak no incrementó el strong_count
de A, rompiendo el ciclo de ownership.

</details>

### Ejercicio 6: Drop y RAII para mutex

En Rust, `MutexGuard` implementa Drop para liberar el lock.
Que ventaja tiene sobre el patrón lock/unlock manual de C?

<details><summary>Prediccion</summary>

En C:
```c
pthread_mutex_lock(&mtx);
// ... código ...
if (error) return;  // BUG: mutex queda locked!
pthread_mutex_unlock(&mtx);
```

En Rust:
```rust
let guard = mutex.lock().unwrap();
// ... código ...
if error { return; }  // guard se droppea -> unlock automático
```

Ventajas del RAII con Drop:

1. **Imposible olvidar unlock**: Drop se ejecuta siempre al salir
   del scope, sin importar si es return normal, early return, o panic.
2. **Exception-safe**: si hay un panic, el stack se desenrolla y
   Drop libera el lock (en C, el mutex queda locked tras longjmp).
3. **El scope del lock es visual**: el `{...}` muestra exactamente
   cuánto tiempo se mantiene el lock.

Esto no tiene equivalente en GC — los finalizers de Java/Python no
garantizan cuándo (ni si) se ejecutan, así que no pueden usarse para
liberar locks.

</details>

### Ejercicio 7: Stack vs heap

Clasifica cada variable: vive en stack o heap?

```rust
let a: i32 = 42;
let b: String = String::from("hello");
let c: &str = "world";
let d: Box<[u8; 1024]> = Box::new([0u8; 1024]);
let e: Vec<i32> = vec![1, 2, 3];
```

<details><summary>Prediccion</summary>

| Variable | Stack | Heap | Detalle |
|----------|:-----:|:----:|---------|
| `a` (i32) | Sí | No | Tipo primitivo, siempre stack |
| `b` (String) | Metadata (ptr, len, cap) | Datos ("hello") | String es fat pointer en stack + buffer en heap |
| `c` (&str) | Sí (ptr + len) | No | String literal en segmento de datos estático (no heap) |
| `d` (Box) | Sí (ptr) | 1024 bytes | Box explícitamente asigna en heap |
| `e` (Vec) | Metadata (ptr, len, cap) | 12 bytes (3 × i32) | Similar a String |

Rust hace la ubicación **explícita**: Box = heap, sin Box = stack
(para tipos con tamaño conocido). En Go/Java, esto se decide
implícitamente (escape analysis o siempre heap).

Esto es la "generación joven gratis": `a`, `c` y las metadatas de
`b`, `d`, `e` se liberan por stack pop, sin GC.

</details>

### Ejercicio 8: Java finalizer vs Rust Drop

En Java, este código tiene un bug sutil:

```java
class Connection {
    protected void finalize() { close(); }
}
// Uso:
Connection c = new Connection();
c = null;  // eligible for GC
// ... el GC puede tardar minutos en ejecutar finalize()
```

Por que el equivalente en Rust no tiene este bug?

<details><summary>Prediccion</summary>

En Java:
- `finalize()` se ejecuta "eventualmente" cuando el GC decide
  colectar el objeto. Puede ser milisegundos o **nunca** (si el
  programa termina antes de un GC cycle).
- Si la Connection mantiene un socket abierto, el recurso queda
  ocupado hasta que finalize() se ejecute — posible **resource leak**.
- En Java 9+, `finalize()` está deprecated por exactamente estas
  razones.

En Rust:
```rust
{
    let c = Connection::open();
    // ...
}   // Drop::drop() ejecutado AQUÍ — inmediato, determinista
```

Drop se ejecuta en el **instante exacto** en que `c` sale del scope.
No hay delay, no hay "eventualmente". El socket se cierra
inmediatamente.

La diferencia fundamental: Drop es parte del **flujo de control**
del programa (compilador inserta la llamada), no una solicitud al GC.

</details>

### Ejercicio 9: Overhead de GC vs Rust

Un programa Java crea 1 millón de objetos pequeños. Cada objeto
tiene un header de 16 bytes para el GC. Cuánta memoria extra consume
el GC metadata?

<details><summary>Prediccion</summary>

GC metadata: 1,000,000 × 16 bytes = **16 MB** de overhead solo en
headers de objetos.

En Rust, el equivalente (Vec de structs) tiene **0 bytes** de
overhead por GC — los structs se almacenan contiguos sin headers:

```rust
struct Small { x: i32, y: i32 }  // 8 bytes
let v: Vec<Small> = Vec::with_capacity(1_000_000);
// Heap: 8 MB (puro dato) vs Java: 8 MB + 16 MB headers = 24 MB
```

Además del overhead de memoria, el GC de Java tiene overhead de CPU:
- Write barriers en cada escritura de referencia.
- Threads de GC consumiendo CPU.
- Pausas STW (aunque breves con G1/ZGC).

Rust: zero overhead. La struct de 8 bytes ocupa exactamente 8 bytes.
El precio: el programador debe gestionar lifetimes (con ayuda del
compilador).

</details>

### Ejercicio 10: Cuándo SI usar Rc/Arc (runtime GC)

En qué escenarios es legítimo usar `Rc<T>` o `Arc<T>` en Rust, a
pesar de que introducen overhead de runtime similar a reference counting?

<details><summary>Prediccion</summary>

Escenarios legítimos:

1. **Grafos con shared ownership**: un nodo del grafo referenciado
   desde múltiples padres. No hay un solo "owner" natural.
   ```rust
   // DAG: múltiples padres -> mismo hijo
   let shared_child = Rc::new(Node { ... });
   parent1.add_child(Rc::clone(&shared_child));
   parent2.add_child(Rc::clone(&shared_child));
   ```

2. **Caches**: un valor cacheado que comparten múltiples consumidores.
   El cache y el consumidor comparten ownership — el valor vive
   mientras alguien lo use.

3. **Concurrencia (Arc)**: datos compartidos entre threads. Arc provee
   reference counting **atómico** (thread-safe).
   ```rust
   let config = Arc::new(Config::load());
   for _ in 0..num_threads {
       let cfg = Arc::clone(&config);
       thread::spawn(move || use_config(&cfg));
   }
   ```

4. **Event listeners / callbacks**: múltiples callbacks que necesitan
   acceso al mismo estado. El estado vive mientras haya listeners.

5. **Immutable shared data**: datos que se comparten ampliamente
   pero nunca se mutan (Rc sin RefCell). Overhead mínimo.

La regla: usar ownership directo (Box, Vec, propiedad en struct)
siempre que sea posible. Recurrir a Rc/Arc solo cuando el grafo de
ownership no sea un **árbol** (requiere sharing).

</details>
