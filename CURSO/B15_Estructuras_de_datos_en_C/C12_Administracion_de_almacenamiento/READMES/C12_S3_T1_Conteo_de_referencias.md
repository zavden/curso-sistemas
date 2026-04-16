# T01 — Conteo de referencias

## Objetivo

Comprender el **conteo de referencias** (*reference counting*, RC) como
estrategia de gestion automatica de memoria: como funciona, por que
los **ciclos** son su talon de Aquiles, como se implementa manualmente
en C, y como Rust lo resuelve con `Rc<T>`, `Arc<T>` y `Weak<T>`.

---

## 1. Concepto fundamental

### 1.1 La idea

Cada objeto en el heap mantiene un **contador** que registra cuantas
referencias apuntan a el:

- Cuando se crea una nueva referencia al objeto: `count++`.
- Cuando una referencia se destruye o se reasigna: `count--`.
- Cuando `count` llega a **0**: el objeto se libera inmediatamente.

```
Objeto A [rc=2]   <--- ptr1
                   <--- ptr2

ptr2 = NULL  =>   A [rc=1]   <--- ptr1

ptr1 = NULL  =>   A [rc=0]   -> FREE!
```

### 1.2 Propiedades

| Propiedad | Valor |
|-----------|-------|
| Liberacion | Inmediata (deterministico) |
| Overhead por objeto | 1 contador (4-8 bytes) |
| Overhead por operacion | Incremento/decremento atomico |
| Pausa (stop-the-world) | Ninguna |
| Ciclos | **No detectados** (memory leak) |
| Localidad | Buena (libera al dejar de usar) |

### 1.3 Comparacion con GC tracing

| Aspecto | Reference counting | Tracing GC |
|---------|:-----------------:|:----------:|
| Cuando libera | Inmediatamente al rc=0 | Periodicamente (GC cycle) |
| Pausas | Ninguna* | Stop-the-world o concurrente |
| Ciclos | Leak | Detectados y liberados |
| Overhead CPU | Cada asignacion de puntero | Solo durante GC cycle |
| Overhead memoria | Contador por objeto | Bits de marcado |
| Determinismo | Si | No |

*Excepto cascadas: si liberar A causa rc=0 en B, que causa rc=0 en C, etc.,
la cascada puede ser larga. Esto se mitiga con **deferred RC** (encolar
liberaciones).

---

## 2. El problema de los ciclos

### 2.1 Ejemplo

```
A [rc=1] ---> B [rc=1] ---> C [rc=1] ---> A  (ciclo!)
^                                          |
ptr_externo ------>------>------->---------+
```

Cuando `ptr_externo` se destruye:
- A.rc = 1 - 1 = ... no, A tiene rc=2 (ptr_externo + C->A).
  Entonces A.rc = 2 - 1 = 1.
- A sigue vivo (rc=1 por C->A).
- B sigue vivo (rc=1 por A->B).
- C sigue vivo (rc=1 por B->C).

**Nadie llega a rc=0**, pero **nadie es accesible** desde el programa.
Es un **memory leak** permanente.

### 2.2 Visualizacion

```
Antes de soltar ptr_externo:

  ptr_externo ---> A [rc=2] ---> B [rc=1] ---> C [rc=1] ---+
                   ^                                         |
                   +-----------------------------------------+

Despues de soltar ptr_externo:

  (nada apunta aqui desde el programa)
                   A [rc=1] ---> B [rc=1] ---> C [rc=1] ---+
                   ^                                         |
                   +-----------------------------------------+

  Ciclo huerfano: rc > 0 en todos, pero inaccesibles.
  Memory leak permanente.
```

### 2.3 Soluciones al problema de ciclos

| Solucion | Mecanismo | Usado por |
|----------|-----------|-----------|
| **Weak references** | Puntero que no incrementa rc | Rust (`Weak<T>`), Swift, Python |
| **Cycle detection** | GC parcial que detecta ciclos | Python (gc module), Objective-C |
| **Prohibir ciclos** | Disenar datos sin ciclos | Muchos programas en la practica |
| **Trial deletion** | Decrementar rc transitivamente | Algoritmo de Lins |
| **Backup tracing GC** | RC + mark-sweep periodico | Python (rc + generational gc) |

---

## 3. Weak references

### 3.1 Concepto

Una **weak reference** es una referencia que **no incrementa** el
reference count. El objeto puede ser liberado aunque existan weak
references a el. Al intentar usar una weak reference, se verifica si
el objeto sigue vivo.

```
A [strong_rc=1, weak_rc=1] <--- strong_ptr
                           <--- weak_ptr (no cuenta para liberacion)

drop(strong_ptr):
  A [strong_rc=0, weak_rc=1] -> LIBERAR DATOS de A
  weak_ptr.upgrade() retorna None (el objeto ya no existe)
```

### 3.2 Dos contadores

Para soportar weak references, cada objeto necesita **dos contadores**:

- **strong_count**: referencias que mantienen vivo el objeto.
- **weak_count**: referencias que solo observan.

El objeto se libera cuando `strong_count == 0`. La metadata (los
contadores mismos) se libera cuando `strong_count == 0 AND weak_count == 0`.

### 3.3 Romper ciclos con weak

```
A [strong] ---> B [strong] ---> C [weak] ---> A

Si la referencia de C a A es weak:
  A.strong_rc = 1 (solo ptr_externo)
  B.strong_rc = 1 (A->B)
  C.strong_rc = 1 (B->C)

drop(ptr_externo):
  A.strong_rc = 0 -> liberar A
  -> B.strong_rc = 0 -> liberar B
  -> C.strong_rc = 0 -> liberar C
  Todo liberado! El weak no impidio la cascada.
```

La regla: en un ciclo, al menos una arista debe ser **weak** para
romper el ciclo de strong references.

---

## 4. Rc y Weak en Rust

### 4.1 Rc\<T\>

`Rc<T>` (*Reference Counted*) es un puntero con conteo de referencias
para uso en **un solo thread** (no es `Send` ni `Sync`):

```rust
use std::rc::Rc;

let a = Rc::new(42);        // strong_count = 1
let b = Rc::clone(&a);      // strong_count = 2
let c = Rc::clone(&a);      // strong_count = 3

println!("{}", Rc::strong_count(&a)); // 3

drop(b);                     // strong_count = 2
drop(c);                     // strong_count = 1
drop(a);                     // strong_count = 0 -> FREE
```

### 4.2 Weak\<T\>

```rust
use std::rc::{Rc, Weak};

let strong = Rc::new(42);
let weak: Weak<i32> = Rc::downgrade(&strong);

// strong_count = 1, weak_count = 1

match weak.upgrade() {
    Some(rc) => println!("alive: {}", rc),  // strong_count = 2 temporalmente
    None => println!("dead"),
}

drop(strong); // strong_count = 0 -> FREE

match weak.upgrade() {
    Some(rc) => println!("alive: {}", rc),
    None => println!("dead"),  // <- esta rama
}
```

### 4.3 Arc\<T\> y Weak (multi-thread)

`Arc<T>` (*Atomically Reference Counted*) es la version thread-safe de
`Rc<T>`. Usa operaciones atomicas para los contadores:

```rust
use std::sync::Arc;

let a = Arc::new(42);
let b = Arc::clone(&a);  // atomico: strong_count = 2

std::thread::spawn(move || {
    println!("{}", b);
    // b se destruye al final del thread -> atomic decrement
});
```

| Tipo | Thread-safe | Overhead por op | Uso |
|------|:-----------:|:---------------:|-----|
| `Rc<T>` | No | Incremento normal (~1 ns) | Single-thread |
| `Arc<T>` | Si | Atomic fetch_add (~5-20 ns) | Multi-thread |

---

## 5. Implementacion manual en C

### 5.1 Estructura

```c
typedef struct RcInner {
    size_t strong_count;
    size_t weak_count;
    /* datos del usuario siguen aqui */
} RcInner;
```

El layout en memoria:

```
[strong_count (8)][weak_count (8)][...datos del usuario...]
^                                 ^
|                                 |
RcInner*                          puntero que ve el usuario
```

### 5.2 Operaciones

```c
void *rc_new(size_t data_size) {
    RcInner *inner = malloc(sizeof(RcInner) + data_size);
    inner->strong_count = 1;
    inner->weak_count = 0;
    return (char *)inner + sizeof(RcInner);  // puntero a datos
}

void rc_clone(void *ptr) {  // incrementar strong
    RcInner *inner = (RcInner *)((char *)ptr - sizeof(RcInner));
    inner->strong_count++;
}

void rc_drop(void *ptr) {  // decrementar strong
    RcInner *inner = (RcInner *)((char *)ptr - sizeof(RcInner));
    inner->strong_count--;
    if (inner->strong_count == 0) {
        // destruir datos (llamar destructor si hay)
        if (inner->weak_count == 0)
            free(inner);  // liberar todo
        // si weak_count > 0: mantener inner para que weaks sepan que murio
    }
}
```

---

## 6. Conteo de referencias en otros lenguajes

| Lenguaje | Mecanismo RC | Ciclos |
|----------|-------------|--------|
| **Python** | RC + cycle detector (gc module) | Detectados periodicamente |
| **Swift** | ARC (Automatic RC) | Weak/unowned references |
| **Objective-C** | ARC (compilador inserta retain/release) | Weak references |
| **C++** | `shared_ptr` / `weak_ptr` | Weak references |
| **Rust** | `Rc<T>` / `Arc<T>` + `Weak<T>` | Weak references |
| **Perl** | RC | Weaken() en Scalar::Util |

### 6.1 Python: RC + cycle detector

Python usa RC como mecanismo **primario**. Para los ciclos, tiene un
GC generacional que se ejecuta periodicamente y detecta ciclos
inalcanzables. Los ciclos entre objetos con `__del__` (finalizer) son
problematicos y pueden causar leaks.

### 6.2 Swift: ARC

Swift inserta automaticamente `retain` (rc++) y `release` (rc--) en
compilacion. El compilador optimiza eliminando pares retain/release
redundantes. Las weak references usan `weak var` y las unowned
references usan `unowned` (como weak pero sin verificacion — crash si
se accede despues de free).

---

## 7. Overhead y optimizaciones

### 7.1 Costo del RC

Para cada asignacion de puntero `p = q`:
1. `q.rc++` (incrementar el nuevo destino).
2. `old_p.rc--` (decrementar el destino anterior de p).
3. Si `old_p.rc == 0`: liberar old_p (posible cascada).

En hot loops con muchas copias de punteros, esto es significativo.

### 7.2 Optimizaciones

- **Elision de RC**: el compilador puede eliminar pares inc/dec
  redundantes. Swift ARC y Rust optimizer hacen esto.
- **Biased RC**: un thread es el "dueno" y usa operaciones no atomicas;
  otros threads usan atomicas. Reduce overhead cuando un thread domina.
- **Deferred RC**: no decrementar inmediatamente; encolar y procesar en
  batch. Reduce cascadas y mejora localidad.
- **Immortal objects**: objetos con rc = MAX que nunca se decrementan
  (constantes, singletons). Python 3.12 usa esto.

---

## 8. Programa completo en C

```c
/*
 * reference_counting.c
 *
 * Manual reference counting with strong and weak counts,
 * cycle demonstration, and cycle detection via trial deletion.
 *
 * Compile: gcc -O2 -o reference_counting reference_counting.c
 * Run:     ./reference_counting
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* =================== RC INFRASTRUCTURE ============================= */

typedef struct RcHeader {
    size_t strong;
    size_t weak;
    size_t data_size;
    void (*destructor)(void *);
    const char *label;
} RcHeader;

#define RC_DATA(header) ((void *)((char *)(header) + sizeof(RcHeader)))
#define RC_HEADER(data) ((RcHeader *)((char *)(data) - sizeof(RcHeader)))

void *rc_new(size_t data_size, void (*dtor)(void *), const char *label) {
    RcHeader *h = (RcHeader *)calloc(1, sizeof(RcHeader) + data_size);
    h->strong = 1;
    h->weak = 0;
    h->data_size = data_size;
    h->destructor = dtor;
    h->label = label;
    return RC_DATA(h);
}

void rc_inc(void *data) {
    if (!data) return;
    RC_HEADER(data)->strong++;
}

void rc_dec(void *data) {
    if (!data) return;
    RcHeader *h = RC_HEADER(data);
    h->strong--;
    if (h->strong == 0) {
        printf("    [RC] %s: strong=0, destroying data\n", h->label);
        if (h->destructor)
            h->destructor(data);
        if (h->weak == 0) {
            printf("    [RC] %s: weak=0, freeing memory\n", h->label);
            free(h);
        } else {
            printf("    [RC] %s: %zu weak refs remain (header kept)\n",
                   h->label, h->weak);
        }
    }
}

void *weak_new(void *data) {
    if (!data) return NULL;
    RC_HEADER(data)->weak++;
    return data;
}

void *weak_upgrade(void *data) {
    if (!data) return NULL;
    RcHeader *h = RC_HEADER(data);
    if (h->strong == 0) return NULL; /* object dead */
    h->strong++;
    return data;
}

void weak_drop(void *data) {
    if (!data) return;
    RcHeader *h = RC_HEADER(data);
    h->weak--;
    if (h->strong == 0 && h->weak == 0) {
        printf("    [RC] %s: last weak gone, freeing header\n", h->label);
        free(h);
    }
}

void rc_stats(void *data, const char *ctx) {
    if (!data) { printf("  [%s] NULL\n", ctx); return; }
    RcHeader *h = RC_HEADER(data);
    printf("  [%s] %s: strong=%zu, weak=%zu\n",
           ctx, h->label, h->strong, h->weak);
}

/* =================== GRAPH NODE WITH RC ============================ */

#define MAX_NEIGHBORS 4

typedef struct GraphNode {
    int id;
    void *neighbors[MAX_NEIGHBORS]; /* rc_data pointers */
    bool neighbor_is_weak[MAX_NEIGHBORS];
    int neighbor_count;
} GraphNode;

void graph_node_dtor(void *data) {
    GraphNode *n = (GraphNode *)data;
    printf("    [DTOR] Destroying node %d, decrementing neighbors\n", n->id);
    for (int i = 0; i < n->neighbor_count; i++) {
        if (n->neighbor_is_weak[i])
            weak_drop(n->neighbors[i]);
        else
            rc_dec(n->neighbors[i]);
    }
}

void *make_node(int id) {
    char label[16];
    snprintf(label, sizeof(label), "Node_%d", id);
    /* label is stack — copy it */
    void *data = rc_new(sizeof(GraphNode), graph_node_dtor, "");
    GraphNode *n = (GraphNode *)data;
    n->id = id;
    n->neighbor_count = 0;
    /* Fix label */
    RcHeader *h = RC_HEADER(data);
    static char labels[20][16]; /* simplification: static storage */
    snprintf(labels[id], 16, "Node_%d", id);
    h->label = labels[id];
    return data;
}

void add_strong_edge(void *from_data, void *to_data) {
    GraphNode *from = (GraphNode *)from_data;
    rc_inc(to_data);
    from->neighbors[from->neighbor_count] = to_data;
    from->neighbor_is_weak[from->neighbor_count] = false;
    from->neighbor_count++;
}

void add_weak_edge(void *from_data, void *to_data) {
    GraphNode *from = (GraphNode *)from_data;
    weak_new(to_data);
    from->neighbors[from->neighbor_count] = to_data;
    from->neighbor_is_weak[from->neighbor_count] = true;
    from->neighbor_count++;
}

/* =================== DEMOS ========================================= */

void demo1_basic_rc(void) {
    printf("=== Demo 1: Basic Reference Counting ===\n\n");

    void *a = rc_new(sizeof(int), NULL, "IntA");
    *(int *)a = 42;
    rc_stats(a, "after creation");

    printf("\n  Cloning (rc_inc):\n");
    rc_inc(a);
    rc_stats(a, "after clone 1");
    rc_inc(a);
    rc_stats(a, "after clone 2");

    printf("\n  Dropping clones (rc_dec):\n");
    rc_dec(a);
    rc_stats(a, "after drop 1");
    rc_dec(a);
    rc_stats(a, "after drop 2");
    rc_dec(a);  /* strong=0 -> freed */
    printf("\n");
}

void demo2_weak_ref(void) {
    printf("=== Demo 2: Weak References ===\n\n");

    void *strong = rc_new(sizeof(int), NULL, "IntW");
    *(int *)strong = 99;

    void *weak = weak_new(strong);
    rc_stats(strong, "strong + weak");

    printf("\n  Upgrading weak while strong exists:\n");
    void *upgraded = weak_upgrade(weak);
    if (upgraded) {
        printf("    upgrade succeeded: value=%d\n", *(int *)upgraded);
        rc_stats(upgraded, "after upgrade");
        rc_dec(upgraded); /* drop the upgraded strong ref */
    }

    printf("\n  Dropping strong:\n");
    rc_dec(strong);

    printf("\n  Upgrading weak after strong is gone:\n");
    upgraded = weak_upgrade(weak);
    printf("    upgrade: %s\n", upgraded ? "SUCCESS (bug!)" : "NULL (correct)");

    weak_drop(weak);
    printf("\n");
}

void demo3_cycle_leak(void) {
    printf("=== Demo 3: Cycle -> Memory Leak ===\n\n");

    void *a = make_node(0);
    void *b = make_node(1);
    void *c = make_node(2);

    /* Create cycle: a -> b -> c -> a (all strong) */
    add_strong_edge(a, b);
    add_strong_edge(b, c);
    add_strong_edge(c, a);

    rc_stats(a, "a (strong edge from c)");
    rc_stats(b, "b (strong edge from a)");
    rc_stats(c, "c (strong edge from b)");

    printf("\n  Dropping external references:\n");
    rc_dec(a);
    rc_dec(b);
    rc_dec(c);

    printf("\n  After dropping all external refs:\n");
    /* Cannot access — but memory is NOT freed! */
    printf("    No destructor messages appeared above.\n");
    printf("    All nodes have strong=1 from the cycle.\n");
    printf("    MEMORY LEAK: 3 nodes leaked permanently.\n\n");
}

void demo4_cycle_broken_with_weak(void) {
    printf("=== Demo 4: Cycle Broken with Weak Reference ===\n\n");

    void *a = make_node(3);
    void *b = make_node(4);
    void *c = make_node(5);

    /* a -> b (strong), b -> c (strong), c -> a (WEAK) */
    add_strong_edge(a, b);
    add_strong_edge(b, c);
    add_weak_edge(c, a);  /* weak breaks the cycle! */

    rc_stats(a, "a (weak edge from c)");
    rc_stats(b, "b (strong edge from a)");
    rc_stats(c, "c (strong edge from b)");

    printf("\n  Dropping external references:\n");
    rc_dec(c);
    printf("  -- c external dropped --\n");
    rc_dec(b);
    printf("  -- b external dropped --\n");
    rc_dec(a);
    printf("  -- a external dropped (triggers cascade) --\n");

    printf("\n  All nodes freed! The weak edge broke the cycle.\n\n");
}

void demo5_shared_ownership(void) {
    printf("=== Demo 5: Shared Ownership Pattern ===\n\n");

    /* Two lists share a tail */
    typedef struct ListNode {
        int value;
        void *next; /* rc pointer to next node */
    } ListNode;

    void list_dtor(void *data) {
        ListNode *n = (ListNode *)data;
        printf("    [DTOR] ListNode value=%d\n", n->value);
        if (n->next) rc_dec(n->next);
    }

    /* Shared tail: [3] -> [4] -> NULL */
    void *n4 = rc_new(sizeof(ListNode), list_dtor, "N4");
    ((ListNode *)n4)->value = 4;
    ((ListNode *)n4)->next = NULL;

    void *n3 = rc_new(sizeof(ListNode), list_dtor, "N3");
    ((ListNode *)n3)->value = 3;
    ((ListNode *)n3)->next = n4;
    rc_inc(n4); /* n3 owns a ref to n4 */

    /* List A: [1] -> [2] -> [3] -> [4] */
    void *n2a = rc_new(sizeof(ListNode), list_dtor, "N2a");
    ((ListNode *)n2a)->value = 2;
    ((ListNode *)n2a)->next = n3;
    rc_inc(n3);

    void *n1a = rc_new(sizeof(ListNode), list_dtor, "N1a");
    ((ListNode *)n1a)->value = 1;
    ((ListNode *)n1a)->next = n2a;
    rc_inc(n2a);

    /* List B: [5] -> [3] -> [4]  (shares n3, n4) */
    void *n5b = rc_new(sizeof(ListNode), list_dtor, "N5b");
    ((ListNode *)n5b)->value = 5;
    ((ListNode *)n5b)->next = n3;
    rc_inc(n3);

    printf("  List A: 1 -> 2 -> 3 -> 4\n");
    printf("  List B: 5 -> 3 -> 4\n");
    printf("  Nodes 3 and 4 are shared.\n\n");

    rc_stats(n3, "N3 (shared)");
    rc_stats(n4, "N4 (shared)");

    printf("\n  Dropping List A:\n");
    rc_dec(n1a);

    printf("\n  After dropping A, shared tail survives:\n");
    rc_stats(n3, "N3 (still alive via List B)");

    printf("\n  Dropping List B:\n");
    rc_dec(n5b);

    /* Also drop our local refs */
    rc_dec(n3);
    rc_dec(n4);

    printf("\n  Now everything is freed.\n\n");
}

void demo6_comparison(void) {
    printf("=== Demo 6: RC Overhead Analysis ===\n\n");

    printf("  Operation costs (approximate, single-threaded):\n\n");
    printf("  %-30s  %-12s  %-12s\n", "Operation", "RC (Rc<T>)", "No RC (Box)");
    printf("  %-30s  %-12s  %-12s\n", "Create",    "malloc + rc=1", "malloc");
    printf("  %-30s  %-12s  %-12s\n", "Clone",     "rc++ (~1ns)",   "deep copy");
    printf("  %-30s  %-12s  %-12s\n", "Drop",      "rc-- + cmp",    "free");
    printf("  %-30s  %-12s  %-12s\n", "Read",      "deref (same)",  "deref");
    printf("  %-30s  %-12s  %-12s\n", "Write",     "deref (same)",  "deref");

    printf("\n  Memory overhead per object:\n");
    printf("    Box<i32>:   4 bytes (just the int)\n");
    printf("    Rc<i32>:    4 + 8 + 8 = 20 bytes (int + strong + weak)\n");
    printf("    Arc<i32>:   same as Rc but atomic counters\n\n");

    printf("  Atomic vs non-atomic increment:\n");
    printf("    Rc::clone:  ~1 ns (normal increment)\n");
    printf("    Arc::clone: ~5-20 ns (atomic fetch_add, cache line bounce)\n");
    printf("    Use Rc when single-threaded, Arc when shared across threads.\n\n");
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_basic_rc();
    demo2_weak_ref();
    demo3_cycle_leak();
    demo4_cycle_broken_with_weak();
    demo5_shared_ownership();
    demo6_comparison();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * reference_counting.rs
 *
 * Demonstrates Rc<T>, Arc<T>, Weak<T>, cycle creation
 * and prevention, shared ownership, and interior mutability.
 *
 * Compile: rustc -o reference_counting reference_counting.rs
 * Run:     ./reference_counting
 */

use std::rc::{Rc, Weak};
use std::cell::RefCell;
use std::sync::Arc;

/* =================== DEMOS ========================================= */

fn demo1_basic_rc() {
    println!("=== Demo 1: Basic Rc<T> ===\n");

    let a = Rc::new(42);
    println!("  a = {}, strong={}, weak={}",
             a, Rc::strong_count(&a), Rc::weak_count(&a));

    let b = Rc::clone(&a);
    let c = Rc::clone(&a);
    println!("  After 2 clones: strong={}", Rc::strong_count(&a));

    drop(b);
    println!("  After drop(b): strong={}", Rc::strong_count(&a));

    drop(c);
    println!("  After drop(c): strong={}", Rc::strong_count(&a));

    println!("  a still usable: {}", *a);

    drop(a);
    println!("  After drop(a): value freed (strong reached 0).\n");
}

fn demo2_weak_refs() {
    println!("=== Demo 2: Weak<T> ===\n");

    let strong = Rc::new(String::from("hello"));
    let weak: Weak<String> = Rc::downgrade(&strong);

    println!("  strong={}, weak={}",
             Rc::strong_count(&strong), Rc::weak_count(&strong));

    /* Upgrade while alive */
    match weak.upgrade() {
        Some(rc) => {
            println!("  upgrade while alive: {:?} (strong temp={})",
                     rc, Rc::strong_count(&strong));
        }
        None => println!("  upgrade: dead"),
    }

    /* Drop the strong reference */
    drop(strong);
    println!("  After drop(strong):");

    match weak.upgrade() {
        Some(rc) => println!("  upgrade: {:?} (bug!)", rc),
        None => println!("  upgrade: None (object is dead — correct!)"),
    }
    println!();
}

fn demo3_cycle_leak() {
    println!("=== Demo 3: Cycle with Rc -> Memory Leak ===\n");

    #[derive(Debug)]
    struct Node {
        id: i32,
        next: RefCell<Option<Rc<Node>>>,
    }

    impl Drop for Node {
        fn drop(&mut self) {
            println!("    Drop Node {}", self.id);
        }
    }

    {
        let a = Rc::new(Node { id: 0, next: RefCell::new(None) });
        let b = Rc::new(Node { id: 1, next: RefCell::new(None) });
        let c = Rc::new(Node { id: 2, next: RefCell::new(None) });

        /* Create cycle: a -> b -> c -> a */
        *a.next.borrow_mut() = Some(Rc::clone(&b));
        *b.next.borrow_mut() = Some(Rc::clone(&c));
        *c.next.borrow_mut() = Some(Rc::clone(&a)); /* cycle! */

        println!("  a.strong={}, b.strong={}, c.strong={}",
                 Rc::strong_count(&a),
                 Rc::strong_count(&b),
                 Rc::strong_count(&c));

        println!("  Dropping a, b, c...");
    }

    println!("  No 'Drop Node' messages appeared above!");
    println!("  All 3 nodes leaked (cycle keeps rc >= 1).\n");
}

fn demo4_cycle_broken() {
    println!("=== Demo 4: Cycle Broken with Weak ===\n");

    struct Node {
        id: i32,
        strong_next: RefCell<Option<Rc<Node>>>,
        weak_back: RefCell<Option<Weak<Node>>>,
    }

    impl Drop for Node {
        fn drop(&mut self) {
            println!("    Drop Node {}", self.id);
        }
    }

    {
        let a = Rc::new(Node {
            id: 0, strong_next: RefCell::new(None),
            weak_back: RefCell::new(None),
        });
        let b = Rc::new(Node {
            id: 1, strong_next: RefCell::new(None),
            weak_back: RefCell::new(None),
        });
        let c = Rc::new(Node {
            id: 2, strong_next: RefCell::new(None),
            weak_back: RefCell::new(None),
        });

        /* a -> b (strong), b -> c (strong), c -> a (WEAK) */
        *a.strong_next.borrow_mut() = Some(Rc::clone(&b));
        *b.strong_next.borrow_mut() = Some(Rc::clone(&c));
        *c.weak_back.borrow_mut() = Some(Rc::downgrade(&a)); /* weak! */

        println!("  a: strong={}, weak={}",
                 Rc::strong_count(&a), Rc::weak_count(&a));
        println!("  b: strong={}, weak={}",
                 Rc::strong_count(&b), Rc::weak_count(&b));
        println!("  c: strong={}, weak={}",
                 Rc::strong_count(&c), Rc::weak_count(&c));

        /* Verify weak works */
        let back = c.weak_back.borrow();
        if let Some(ref w) = *back {
            match w.upgrade() {
                Some(node) => println!("  c->a (weak upgrade): Node {}",
                                       node.id),
                None => println!("  c->a: dead"),
            }
        }
        drop(back);

        println!("\n  Dropping a, b, c...");
    }
    println!("  All nodes freed! Weak reference broke the cycle.\n");
}

fn demo5_shared_ownership() {
    println!("=== Demo 5: Shared Ownership (Two Lists, Shared Tail) ===\n");

    #[derive(Debug)]
    struct ListNode {
        value: i32,
        next: Option<Rc<ListNode>>,
    }

    impl Drop for ListNode {
        fn drop(&mut self) {
            println!("    Drop ListNode({})", self.value);
        }
    }

    let shared_tail = Rc::new(ListNode {
        value: 3,
        next: Some(Rc::new(ListNode { value: 4, next: None })),
    });

    /* List A: 1 -> 2 -> 3 -> 4 */
    let list_a = Rc::new(ListNode {
        value: 1,
        next: Some(Rc::new(ListNode {
            value: 2,
            next: Some(Rc::clone(&shared_tail)),
        })),
    });

    /* List B: 5 -> 3 -> 4 */
    let list_b = Rc::new(ListNode {
        value: 5,
        next: Some(Rc::clone(&shared_tail)),
    });

    println!("  List A: 1 -> 2 -> 3 -> 4");
    println!("  List B: 5 -> 3 -> 4");
    println!("  Node 3 strong_count = {}", Rc::strong_count(&shared_tail));

    println!("\n  Dropping List A:");
    drop(list_a);
    println!("  Node 3 strong_count = {} (still alive via B)",
             Rc::strong_count(&shared_tail));

    println!("\n  Dropping List B:");
    drop(list_b);

    println!("\n  Dropping shared_tail ref:");
    drop(shared_tail);
    println!("  Everything freed.\n");
}

fn demo6_tree_parent_weak() {
    println!("=== Demo 6: Tree with Weak Parent Pointer ===\n");

    struct TreeNode {
        name: String,
        parent: RefCell<Weak<TreeNode>>,
        children: RefCell<Vec<Rc<TreeNode>>>,
    }

    impl Drop for TreeNode {
        fn drop(&mut self) {
            println!("    Drop TreeNode({})", self.name);
        }
    }

    let root = Rc::new(TreeNode {
        name: "root".into(),
        parent: RefCell::new(Weak::new()),
        children: RefCell::new(vec![]),
    });

    let child1 = Rc::new(TreeNode {
        name: "child1".into(),
        parent: RefCell::new(Rc::downgrade(&root)),
        children: RefCell::new(vec![]),
    });

    let child2 = Rc::new(TreeNode {
        name: "child2".into(),
        parent: RefCell::new(Rc::downgrade(&root)),
        children: RefCell::new(vec![]),
    });

    let grandchild = Rc::new(TreeNode {
        name: "grandchild".into(),
        parent: RefCell::new(Rc::downgrade(&child1)),
        children: RefCell::new(vec![]),
    });

    root.children.borrow_mut().push(Rc::clone(&child1));
    root.children.borrow_mut().push(Rc::clone(&child2));
    child1.children.borrow_mut().push(Rc::clone(&grandchild));

    println!("  Tree structure:");
    println!("    root (strong={}, weak={})",
             Rc::strong_count(&root), Rc::weak_count(&root));
    println!("    +- child1 (strong={}, weak={})",
             Rc::strong_count(&child1), Rc::weak_count(&child1));
    println!("    |  +- grandchild (strong={})",
             Rc::strong_count(&grandchild));
    println!("    +- child2 (strong={})",
             Rc::strong_count(&child2));

    /* Navigate up via weak parent */
    let gc_parent = grandchild.parent.borrow().upgrade();
    match gc_parent {
        Some(p) => println!("\n  grandchild.parent = {}", p.name),
        None => println!("\n  grandchild.parent = dead"),
    }

    println!("\n  Dropping all (root owns children via strong):");
    drop(grandchild);
    drop(child1);
    drop(child2);
    drop(root);
    println!("  Tree fully freed — weak parents didn't prevent it.\n");
}

fn demo7_arc_multithread() {
    println!("=== Demo 7: Arc<T> for Multi-Thread Sharing ===\n");

    let data = Arc::new(vec![1, 2, 3, 4, 5]);
    println!("  Created Arc<Vec<i32>>, strong={}",
             Arc::strong_count(&data));

    let mut handles = vec![];

    for i in 0..3 {
        let data_clone = Arc::clone(&data);
        println!("  Cloned for thread {}: strong={}",
                 i, Arc::strong_count(&data));

        handles.push(std::thread::spawn(move || {
            let sum: i32 = data_clone.iter().sum();
            println!("    Thread {}: sum={}, strong={}",
                     i, sum, Arc::strong_count(&data_clone));
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    println!("  After all threads: strong={}", Arc::strong_count(&data));
    drop(data);
    println!("  Data freed.\n");
}

fn demo8_rc_vs_box() {
    println!("=== Demo 8: When to Use Rc vs Box vs Arc ===\n");

    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Feature", "Box<T>", "Rc<T>", "Arc<T>", "&T");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Ownership", "Unique", "Shared", "Shared", "Borrowed");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Thread-safe", "Send", "No", "Yes", "Depends");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Overhead", "0", "16 bytes", "16 bytes", "0");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Clone cost", "Deep copy", "~1 ns", "~5-20 ns", "Free");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Drop", "Immediate", "At rc=0", "At rc=0", "Never");
    println!("  {:20} {:10} {:10} {:10} {:10}",
             "Cycles", "N/A", "Possible", "Possible", "N/A");

    println!("\n  Decision guide:");
    println!("    Single owner             -> Box<T>");
    println!("    Shared, single thread    -> Rc<T>");
    println!("    Shared, multi thread     -> Arc<T>");
    println!("    Temporary access         -> &T / &mut T");
    println!("    Tree parent pointers     -> Weak<T>");
    println!("    Graph back-edges         -> Weak<T>");
    println!();
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_basic_rc();
    demo2_weak_refs();
    demo3_cycle_leak();
    demo4_cycle_broken();
    demo5_shared_ownership();
    demo6_tree_parent_weak();
    demo7_arc_multithread();
    demo8_rc_vs_box();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Traza de conteo

Dado:
```
a = Rc::new(10)
b = Rc::clone(&a)
c = Rc::clone(&b)
drop(a)
d = Rc::clone(&c)
drop(b)
drop(c)
```

Cual es el strong_count despues de cada operacion? Cuando se libera
el valor?

<details><summary>Prediccion</summary>

```
a = Rc::new(10)         strong = 1
b = Rc::clone(&a)       strong = 2
c = Rc::clone(&b)       strong = 3
drop(a)                 strong = 2
d = Rc::clone(&c)       strong = 3
drop(b)                 strong = 2
drop(c)                 strong = 1
```

Al final, solo `d` mantiene una referencia. El valor `10` NO se ha
liberado aun (strong = 1).

Se libera cuando `drop(d)` → strong = 0 → FREE.

El orden de creacion/destruccion no importa — solo importa el conteo.
Mientras haya al menos un `Rc` vivo, el valor sobrevive.

</details>

### Ejercicio 2 — Ciclo simple

```rust
let a = Rc::new(RefCell::new(None));
let b = Rc::new(RefCell::new(None));
*a.borrow_mut() = Some(Rc::clone(&b));
*b.borrow_mut() = Some(Rc::clone(&a));
```

Cuales son los strong_counts de `a` y `b`? Se liberan al salir del scope?

<details><summary>Prediccion</summary>

Despues de las asignaciones:
- `a`: strong = 2 (variable `a` + contenido de `b`).
- `b`: strong = 2 (variable `b` + contenido de `a`).

Al salir del scope:
- `drop(b)`: b.strong = 2 - 1 = 1. No se libera.
- `drop(a)`: a.strong = 2 - 1 = 1. No se libera.

Ambos quedan con strong = 1 por la referencia mutua. **Memory leak**.

Para solucionarlo, una de las dos referencias debe ser `Weak`:
```rust
*b.borrow_mut() = Some(Rc::downgrade(&a)); // Weak, no incrementa strong
```

Ahora: a.strong = 1, a.weak = 1, b.strong = 2. Al hacer drop(a):
a.strong = 0 → FREE → b.strong = 1. Al hacer drop(b): b.strong = 0
→ FREE. Todo liberado.

</details>

### Ejercicio 3 — Weak upgrade

```rust
let strong = Rc::new(String::from("data"));
let w1 = Rc::downgrade(&strong);
let w2 = Rc::downgrade(&strong);
drop(strong);
let result = w1.upgrade();
```

Que contiene `result`? Cuantas weak references quedan? Se libero la
memoria del header?

<details><summary>Prediccion</summary>

- Despues de `drop(strong)`: strong_count = 0 → datos de String liberados.
  weak_count = 2 (w1 y w2 siguen existiendo).

- `w1.upgrade()`: strong_count == 0 → retorna **`None`**.

- `result = None`.

- Quedan **2 weak references** (w1 y w2).

- **El header NO se libero** aun. El header (que contiene strong_count y
  weak_count) se mantiene vivo mientras haya weaks, para que puedan
  verificar que strong_count == 0 al intentar upgrade.

  El header se libera cuando weak_count llega a 0:
  ```
  drop(w1) -> weak_count = 1 (header sigue)
  drop(w2) -> weak_count = 0 -> FREE header
  ```

Esto significa que las weak references tienen un **overhead de memoria**:
el header (tipicamente 16-24 bytes) sobrevive despues de que los datos
se liberaron.

</details>

### Ejercicio 4 — Tree con parent pointers

Disena un arbol donde cada nodo tiene una referencia strong a sus hijos
y una referencia weak a su padre. Por que strong hijos + weak padre y no
al reves?

<details><summary>Prediccion</summary>

**Diseno correcto**: parent → children (strong), child → parent (weak).

```
root [strong=1 (variable)] --strong--> child [strong=1 (root)]
                           <--weak---- child
```

**Por que no al reves** (parent weak a hijos, child strong a parent):

Si el padre tiene weak refs a los hijos: al soltar las variables de los
hijos, strong_count de los hijos llega a 0 → se liberan inmediatamente,
aunque el padre los referencia. El arbol se "deshoja" desde abajo.

Peor: si solo el root tiene una variable externa, los hijos no tendrian
strong refs de nadie excepto las variables locales. Al salir del scope,
los hijos mueren y el root queda con weaks a memoria liberada.

**La regla del arbol**: la referencia que define **ownership** (quien
mantiene vivo a quien) debe ser strong. En un arbol, el padre **posee**
a los hijos. La referencia de hijo a padre es solo una **observacion**
(para navegacion hacia arriba), no ownership.

Analogia con Rust: `parent.children: Vec<Rc<Node>>` (strong) y
`child.parent: Weak<Node>` (weak). El arbol se destruye de arriba
hacia abajo: root muere → children strong_count = 0 → children mueren
→ grandchildren mueren → etc.

</details>

### Ejercicio 5 — Rc vs Arc decision

Para cada escenario, elige `Rc<T>` o `Arc<T>`:

1. AST de un parser single-threaded.
2. Cache compartida entre threads de un servidor web.
3. Grafo en un algoritmo single-threaded.
4. Configuracion leida por multiples threads.

<details><summary>Prediccion</summary>

1. **AST single-threaded** → **`Rc<T>`**.
   Un solo thread construye y recorre el AST. No hay razon para pagar
   el costo de atomics. `Rc` es ~5-20x mas rapido que `Arc` para
   clone/drop.

2. **Cache multi-threaded** → **`Arc<T>`**.
   Multiples threads leen la cache. `Arc` garantiza thread safety
   con contadores atomicos. Combinado con `Arc<RwLock<Cache>>` para
   mutabilidad.

3. **Grafo single-threaded** → **`Rc<T>`**.
   Mismo razonamiento que el AST. Usar `Rc<RefCell<Node>>` para
   mutabilidad interior.

4. **Config multi-threaded (read-only)** → **`Arc<T>`**.
   La configuracion se lee pero no se modifica. `Arc<Config>` permite
   que cada thread tenga su referencia. Sin `Mutex`/`RwLock` si es
   inmutable.

**Regla**: `Rc` si el dato nunca cruza threads. `Arc` si puede cruzar.
El compilador **no permite** enviar `Rc` a otro thread (`Rc` no es
`Send`), asi que el error se detecta en compilacion.

</details>

### Ejercicio 6 — Overhead de RC

Un programa tiene 100,000 nodos de 32 bytes cada uno. Compara la
memoria total con:

1. `Box<Node>` (ownership unico).
2. `Rc<Node>` (shared ownership).
3. `Arc<Node>` (shared, thread-safe).

<details><summary>Prediccion</summary>

1. **`Box<Node>`**: cada Box aloca el nodo en el heap.
   Memoria por nodo: 32 bytes (dato) + ~16 bytes (malloc header) = 48.
   Total: $100{,}000 \times 48 = 4.8$ MB.

2. **`Rc<Node>`**: Rc agrega un header con strong_count y weak_count.
   Memoria por nodo: 32 (dato) + 8 (strong) + 8 (weak) + ~16 (malloc
   header) = 64.
   Total: $100{,}000 \times 64 = 6.4$ MB.
   **Overhead vs Box: 33%**.

3. **`Arc<Node>`**: igual que Rc en tamano (los AtomicUsize tienen el
   mismo tamano que usize).
   Total: $100{,}000 \times 64 = 6.4$ MB.
   **Mismo tamano que Rc**, pero mas lento en clone/drop (atomics).

Para datos pequenos (32 bytes), el overhead de RC (16 bytes extra) es
significativo (50% del dato). Para datos grandes (1 KB), el overhead es
trivial (1.6%).

**Alternativa**: usar un arena o pool con indices en lugar de Rc.
Memoria: $100{,}000 \times 32 = 3.2$ MB (cero overhead).

</details>

### Ejercicio 7 — Deteccion de ciclos

Describe un algoritmo para detectar ciclos en un grafo de objetos con
reference counting. Hint: trial deletion.

<details><summary>Prediccion</summary>

**Algoritmo de trial deletion** (Lins, 1990):

1. **Sospecha**: cuando `rc_dec` reduce el conteo a un valor > 0 (no se
   libera), el objeto es un **candidato** para ciclo. Anadirlo a una
   lista de candidatos.

2. **Mark gray**: para cada candidato, decrementar **tentativamente** el
   conteo de todos sus referenciados. Marcar cada objeto visitado como
   "gray" (en proceso).

3. **Scan**: recorrer los objetos gray:
   - Si `tentative_count == 0`: marcar como "white" (basura).
   - Si `tentative_count > 0`: marcar como "black" (vivo — hay
     referencias externas al ciclo).

4. **Collect**: liberar todos los objetos white (son ciclos inalcanzables).

5. **Restore**: para los objetos black, restaurar sus conteos originales.

**Complejidad**: $O(|\text{candidatos}| + |\text{aristas}|)$ por
ejecucion. Se ejecuta periodicamente, no en cada decremento.

**Python** usa este algoritmo (simplificado, generacional) en su modulo
`gc`. Los objetos sin `__del__` se manejan automaticamente; los que
tienen finalizer son mas complicados.

</details>

### Ejercicio 8 — Interior mutability

Por que `Rc<T>` necesita `RefCell<T>` para mutabilidad en Rust?
Que alternativa existe?

<details><summary>Prediccion</summary>

**El problema**: `Rc<T>` da multiples `&T` (referencias compartidas).
En Rust, `&T` es inmutable — no se puede modificar el dato. Esto es
porque si dos partes del codigo tienen `&T`, modificar a traves de una
invalidaria las asunciones de la otra.

**`RefCell<T>`**: proporciona **interior mutability** — permite obtener
`&mut T` a traves de `&T`, verificando en **runtime** que las reglas
del borrow checker se cumplan:

```rust
let node = Rc::new(RefCell::new(value));
let mut borrow = node.borrow_mut(); // runtime check
*borrow = new_value;
```

Si dos `borrow_mut()` se solapan, `RefCell` **panics** en runtime (en
lugar de UB como en C).

**Alternativas**:

1. **`Cell<T>`**: para tipos `Copy`. No necesita borrow — copia el
   valor entero. `Cell::set(new)` y `Cell::get()`.

2. **`Mutex<T>`** / **`RwLock<T>`**: para multi-thread con `Arc`.
   Bloqueo real en lugar de runtime check.

3. **Indices en un `Vec`**: en lugar de `Rc<RefCell<Node>>`, usar
   `Vec<Node>` y referenciar por indice `usize`. El `Vec` es el unico
   dueno, y la mutabilidad es directa: `nodes[i].value = x`.

La opcion 3 (indices) es la mas eficiente y la preferida para grafos y
arboles cuando el rendimiento importa.

</details>

### Ejercicio 9 — Cascada de drops

```rust
// Lista: a -> b -> c -> d -> e (cada nodo es Rc)
let e = Rc::new(Node { value: 5, next: None });
let d = Rc::new(Node { value: 4, next: Some(Rc::clone(&e)) });
let c = Rc::new(Node { value: 3, next: Some(Rc::clone(&d)) });
let b = Rc::new(Node { value: 2, next: Some(Rc::clone(&c)) });
let a = Rc::new(Node { value: 1, next: Some(Rc::clone(&b)) });

drop(e); drop(d); drop(c); drop(b);
// Solo queda 'a'. Que pasa al hacer drop(a)?
```

<details><summary>Prediccion</summary>

Despues de los 4 drops, los strong_counts son:
- a: strong=1 (variable `a`), next→b
- b: strong=1 (a→b), next→c
- c: strong=1 (b→c), next→d
- d: strong=1 (c→d), next→e
- e: strong=1 (d→e)

`drop(a)`:
1. a.strong = 0 → Drop a → decrementa b
2. b.strong = 0 → Drop b → decrementa c
3. c.strong = 0 → Drop c → decrementa d
4. d.strong = 0 → Drop d → decrementa e
5. e.strong = 0 → Drop e

**Cascada de 5 drops**. Cada uno se ejecuta inmediatamente dentro del
anterior (recursion en el stack). Para una lista de $N$ nodos, esto
consume $O(N)$ stack frames.

**Problema**: si $N$ es grande (ej. 1 millon), la cascada puede causar
**stack overflow**. Rust no tiene tail-call optimization para Drop.

**Solucion**: implementar Drop manualmente con un loop iterativo:
```rust
impl Drop for Node {
    fn drop(&mut self) {
        let mut current = self.next.take();
        while let Some(node) = current {
            match Rc::try_unwrap(node) {
                Ok(mut inner) => current = inner.next.take(),
                Err(_) => break, // other refs exist, stop
            }
        }
    }
}
```

</details>

### Ejercicio 10 — Disenar un sistema con RC

Un editor de texto colaborativo tiene:
- Documentos (compartidos entre usuarios).
- Usuarios (cada uno tiene una lista de documentos abiertos).
- Historial de ediciones (cada edicion referencia al documento).

Disena las relaciones con `Rc`, `Weak` y `Arc`. Identifica donde hay
riesgo de ciclos.

<details><summary>Prediccion</summary>

**Diseño**:

```
Document {
    content: String,
    editors: Vec<Weak<User>>,       // weak: doc no posee al user
    history: Vec<Rc<Edit>>,         // strong: doc posee su historial
}

User {
    name: String,
    open_docs: Vec<Rc<Document>>,   // strong: user mantiene docs vivos
}

Edit {
    timestamp: u64,
    content: String,
    document: Weak<Document>,       // weak: edit no posee al doc
    author: Weak<User>,             // weak: edit no posee al user
}
```

**Justificacion**:

1. `User → Document`: **strong** (Rc). Un documento debe vivir mientras
   algun usuario lo tenga abierto.

2. `Document → User` (editors): **weak**. Si un usuario cierra sesion,
   no queremos que el documento lo mantenga vivo.

3. `Document → Edit`: **strong** (Rc). El historial pertenece al
   documento.

4. `Edit → Document`: **weak**. Si el documento se elimina, las ediciones
   no deben mantenerlo vivo (referencia circular).

5. `Edit → User` (author): **weak**. Si el autor se va, la edicion
   conserva su registro pero no impide la liberacion del User.

**Riesgo de ciclo**: User → Document → Edit → User. Pero Edit → User es
weak, y Edit → Document es weak, asi que no hay ciclo de strong refs.

**Multi-thread**: si multiples threads editan, reemplazar `Rc` con `Arc`
y agregar `Mutex<Vec<...>>` o `RwLock` para las listas mutables.

</details>
