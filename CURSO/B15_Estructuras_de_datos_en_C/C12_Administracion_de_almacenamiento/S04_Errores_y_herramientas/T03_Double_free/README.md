# T03 — Double free

## Objetivo

Comprender el error **double free**: liberar el mismo bloque de memoria
dos veces. Entender por qué corrompe las estructuras internas del
allocator (free list), cómo se manifiesta (crash, corrupción silenciosa,
vulnerabilidad de seguridad), cómo detectarlo con Valgrind y ASan, y
por qué el sistema de ownership de Rust lo hace imposible en safe code.

---

## 1. Qué es un double free

### 1.1 Definición

Un **double free** ocurre cuando se llama `free()` dos veces sobre el
mismo puntero sin una asignación intermedia:

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
free(p);  // DOUBLE FREE: undefined behavior
```

### 1.2 Por qué es peligroso

A diferencia de un memory leak (que solo desperdicia memoria), un
double free **corrompe** las estructuras internas del allocator:

```
Estado normal del free list:

  free_list -> [bloque_X] -> [bloque_Y] -> NULL

Después de free(p) (p apunta a bloque_Z):

  free_list -> [bloque_Z] -> [bloque_X] -> [bloque_Y] -> NULL

Después de free(p) OTRA VEZ:

  free_list -> [bloque_Z] -> [bloque_Z] -> [bloque_X] -> ...
                    |              ^
                    +--------------+  ¡CICLO!
```

El bloque Z aparece **dos veces** en la free list. Esto causa:

1. **malloc retorna Z dos veces**: dos variables distintas reciben el
   mismo bloque — escribir en una corrompe la otra.
2. **Corrupción del heap**: la metadata interna del allocator queda
   inconsistente — futuros malloc/free pueden crashear.
3. **Explotable**: atacantes pueden manipular la free list para
   ejecutar código arbitrario.

### 1.3 Comparación con otros errores de memoria

| Error | Efecto | Severidad | Detectabilidad |
|-------|--------|:---------:|:--------------:|
| Memory leak | Memoria desperdiciada | Media | Fácil (Valgrind) |
| Use-after-free | Lee/escribe memoria liberada | Alta | Media |
| **Double free** | Corrompe el allocator | **Crítica** | Media |
| Buffer overflow | Escribe fuera de límites | Crítica | Media |

---

## 2. Cómo ocurre la corrupción

### 2.1 Free list antes del double free

El allocator mantiene una **free list** de bloques disponibles. Cada
bloque libre contiene un puntero al siguiente bloque libre:

```
Heap layout:

  [used][FREE -> next][used][FREE -> next][used]
          |                   ^
          +-------------------+

  free_list_head --> bloque_A --> bloque_B --> NULL
```

### 2.2 Primer free(p) — correcto

```
p apunta a bloque_C (en uso).

Después de free(p):
  bloque_C se agrega al inicio de la free list.
  bloque_C.next = free_list_head (bloque_A)
  free_list_head = bloque_C

  free_list: C --> A --> B --> NULL
```

### 2.3 Segundo free(p) — double free

```
p sigue apuntando a bloque_C (ya libre).

Después de free(p) otra vez:
  bloque_C se agrega al inicio OTRA VEZ.
  bloque_C.next = free_list_head (bloque_C!)
  free_list_head = bloque_C

  free_list: C --> C --> C --> C --> ...  (ciclo infinito)
                  ↑_____|
```

### 2.4 Consecuencias

Dos `malloc` posteriores retornan **el mismo bloque**:

```c
free(p);
free(p);  // double free

int *a = malloc(sizeof(int));  // retorna bloque_C
int *b = malloc(sizeof(int));  // retorna bloque_C OTRA VEZ

*a = 100;
*b = 200;

printf("%d\n", *a);  // imprime 200, no 100!
// a y b apuntan al mismo bloque — corrupción silenciosa
```

---

## 3. Patrones comunes de double free

### 3.1 Patrón 1: free explícito dos veces

```c
char *buf = malloc(64);
/* ... */
free(buf);
/* ... mucho código después ... */
free(buf);  // olvidó que ya fue liberado
```

### 3.2 Patrón 2: free en función + free en caller

```c
void cleanup(Node *n) {
    free(n->data);
    free(n);
}

Node *n = create_node();
cleanup(n);
free(n);  // double free: cleanup ya lo liberó
```

### 3.3 Patrón 3: Dos owners del mismo recurso

```c
char *shared = malloc(64);
List *list_a = create_list();
List *list_b = create_list();

list_add(list_a, shared);
list_add(list_b, shared);  // dos listas "ownen" el mismo bloque

list_destroy(list_a);  // free(shared) dentro de destroy
list_destroy(list_b);  // free(shared) OTRA VEZ — double free
```

### 3.4 Patrón 4: Error path duplicado

```c
int process(void) {
    char *buf = malloc(1024);

    if (step1(buf) < 0) {
        free(buf);
        return -1;
    }

    if (step2(buf) < 0) {
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
    /* Correcto si solo un path se ejecuta.
       Pero si step1 internamente llama free(buf)
       antes de retornar error, es double free. */
}
```

### 3.5 Patrón 5: Destruir árbol con nodos compartidos

```c
/*       A
 *      / \
 *     B   C
 *      \ /
 *       D   <-- compartido por B y C
 */
tree_destroy(A);
/* postorder: D, B, D(otra vez!), C, A
   D se libera DOS VECES */
```

---

## 4. Prevención en C

### 4.1 Set to NULL after free

```c
free(p);
p = NULL;
/* Ahora free(p) = free(NULL) = no-op (seguro) */
free(p);  // no hace nada
```

El estándar C garantiza que `free(NULL)` es un no-op. Setear a NULL
es la defensa más simple pero no protege contra aliases.

### 4.2 Ownership claro

Cada bloque de memoria debe tener **exactamente un owner** responsable
de liberarlo:

```c
/* Convención: create_ retorna ownership, destroy_ consume ownership */
Node *n = node_create(42);     /* caller owns n */
list_add(list, n);             /* list takes ownership */
/* caller must NOT free n */
list_destroy(list);            /* list frees all nodes */
```

### 4.3 Macro safe free

```c
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while(0)

char *buf = malloc(64);
SAFE_FREE(buf);   // free + NULL
SAFE_FREE(buf);   // free(NULL) = no-op
```

### 4.4 Reference counting para shared ownership

Si múltiples partes del código necesitan el mismo bloque, usar
reference counting (como vimos en S03/T01):

```c
rc_retain(shared);   // list_a retains
rc_retain(shared);   // list_b retains
rc_release(shared);  // list_a releases (rc=1)
rc_release(shared);  // list_b releases (rc=0 -> free)
```

---

## 5. Cómo Rust previene double free

### 5.1 Ownership — un solo dueño

Rust garantiza que cada valor tiene **exactamente un owner**. Cuando
el owner sale del scope, Drop se llama una sola vez:

```rust
let s = String::from("hello");
drop(s);
// drop(s);  // COMPILE ERROR: use of moved value: `s`
//             drop consume el ownership — s ya no existe
```

### 5.2 Move semantics

Transferir ownership invalida el origen:

```rust
let a = Box::new(42);
let b = a;           // move: a invalidado
// drop(a);          // COMPILE ERROR: a was moved
drop(b);             // OK: b es el único owner
```

No hay forma de que dos variables posean el mismo `Box`. Ergo,
no hay forma de llamar `free` dos veces.

### 5.3 Copy vs Clone — distinción explícita

- **Copy**: duplica bits (para i32, f64, bool). Ambas copias son
  independientes — no hay double free porque no hay `free` en
  tipos Copy.
- **Clone**: duplicación explícita para tipos que poseen heap memory.
  Crea un bloque **nuevo** — cada copia tiene su propio bloque.

```rust
let a = String::from("hello");
let b = a.clone();   // nuevo bloque en heap — b tiene su propia copia
drop(a);             // free bloque de a
drop(b);             // free bloque de b — diferentes bloques, no double free
```

### 5.4 Rc — shared ownership sin double free

Cuando se necesita sharing, `Rc` cuenta referencias y libera una
sola vez cuando el count llega a 0:

```rust
use std::rc::Rc;
let a = Rc::new(42);
let b = Rc::clone(&a);   // rc=2
drop(a);                  // rc=1 (no free)
drop(b);                  // rc=0 -> free (una sola vez)
```

---

## 6. Detección de double free

### 6.1 glibc detection

glibc (Linux) detecta algunos double frees y aborta:

```
*** glibc detected *** ./program: double free or corruption (fasttop): 0x0804a008 ***
======= Backtrace: =========
...
```

Pero esta detección no es exhaustiva — solo funciona para bloques
pequeños en los fastbins.

### 6.2 Valgrind

```
==1234== Invalid free() / delete / delete[] / realloc()
==1234==    at 0x...: free (vg_replace_malloc.c:...)
==1234==    by 0x...: main (program.c:10)
==1234==  Address 0x... is 0 bytes inside a block of size 4 free'd
==1234==    at 0x...: free (vg_replace_malloc.c:...)
==1234==    by 0x...: main (program.c:9)
==1234==  Block was alloc'd at
==1234==    at 0x...: malloc (vg_replace_malloc.c:...)
==1234==    by 0x...: main (program.c:7)
```

### 6.3 ASan

```
=================================================================
==1234==ERROR: AddressSanitizer: attempting double-free on 0x602000000010
    #0 0x... in free
    #1 0x... in main (program.c:10)
0x602000000010 is located 0 bytes inside of 4-byte region
freed by thread T0 here:
    #0 0x... in free
    #1 0x... in main (program.c:9)
previously allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in main (program.c:7)
```

ASan es más rápido que Valgrind (2-3x vs 10-50x slowdown) y
detecta double free de forma más confiable.

---

## 7. Programa en C — Demostraciones de double free

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Double Free Demonstrations
 *
 *  WARNING: Real double frees cause UB. These demos simulate
 *  the problem using tracking instead of actual double frees.
 *
 *  Compile: gcc -std=c11 -Wall -Wextra -g -o dblf dblf.c
 * ============================================================ */

/* ---- Allocation tracker ---- */

#define MAX_TRACKED 64

static struct {
    void *ptrs[MAX_TRACKED];
    int   freed[MAX_TRACKED];
    int   count;
} tracker;

void tracker_init(void) {
    memset(&tracker, 0, sizeof(tracker));
}

void *tracked_malloc(size_t size, const char *label) {
    void *p = malloc(size);
    if (p && tracker.count < MAX_TRACKED) {
        int idx = tracker.count++;
        tracker.ptrs[idx] = p;
        tracker.freed[idx] = 0;
        printf("  [malloc]  %p (%s, %zu bytes)\n", p, label, size);
    }
    return p;
}

int tracked_free(void *p, const char *label) {
    if (!p) {
        printf("  [free]    NULL (%s) — no-op\n", label);
        return 0;
    }

    for (int i = 0; i < tracker.count; i++) {
        if (tracker.ptrs[i] == p) {
            if (tracker.freed[i]) {
                printf("  [FREE]    %p (%s) — DOUBLE FREE DETECTED!\n",
                       p, label);
                return -1;  /* double free */
            }
            tracker.freed[i] = 1;
            printf("  [free]    %p (%s) — OK\n", p, label);
            free(p);
            return 0;
        }
    }
    printf("  [free]    %p (%s) — NOT TRACKED (invalid free)\n",
           p, label);
    return -1;
}

void tracker_report(void) {
    int leaks = 0;
    for (int i = 0; i < tracker.count; i++) {
        if (!tracker.freed[i]) {
            printf("  [LEAK]    %p not freed\n", tracker.ptrs[i]);
            leaks++;
        }
    }
    if (leaks == 0) printf("  No leaks detected.\n");
}

/* ---- List node ---- */

typedef struct LNode {
    int data;
    struct LNode *next;
} LNode;

/* ============================================================
 *  Demo 1: Basic double free detection
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: Basic double free ===\n");
    tracker_init();

    int *p = tracked_malloc(sizeof(int), "p");
    *p = 42;

    tracked_free(p, "p (first)");
    tracked_free(p, "p (second)");  /* double free! */

    printf("\n  In real C, the second free() corrupts the allocator.\n");
    printf("  Our tracker caught it without crashing.\n");

    printf("\n  Fix: set p = NULL after free:\n");
    int *q = tracked_malloc(sizeof(int), "q");
    *q = 99;
    tracked_free(q, "q");
    q = NULL;
    tracked_free(q, "q (NULL)");  /* free(NULL) = no-op */
}

/* ============================================================
 *  Demo 2: Free in function + free in caller
 * ============================================================ */

void process_node(void *ptr) {
    printf("  process_node: processing and freeing\n");
    tracked_free(ptr, "inside process_node");
}

void demo2(void) {
    printf("\n=== Demo 2: Free in function + caller ===\n");
    tracker_init();

    int *data = tracked_malloc(sizeof(int) * 4, "data");
    for (int i = 0; i < 4; i++) data[i] = (i + 1) * 10;

    process_node(data);         /* frees data inside */
    tracked_free(data, "caller after process_node");  /* double free! */

    printf("\n  Problem: caller doesn't know process_node freed data.\n");
    printf("  Fix: document ownership clearly, or pass Node **.\n");
}

/* ============================================================
 *  Demo 3: Two owners — shared pointer problem
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Two owners of same block ===\n");
    tracker_init();

    char *shared = tracked_malloc(64, "shared");
    strcpy(shared, "shared data");

    /* Two "owners" */
    char *owner_a = shared;
    char *owner_b = shared;

    printf("  owner_a and owner_b both point to %p\n", (void *)shared);

    /* owner_a frees */
    tracked_free(owner_a, "owner_a");

    /* owner_b tries to free the same block */
    tracked_free(owner_b, "owner_b");  /* double free! */

    printf("\n  Solution: use reference counting or single owner.\n");
}

/* ============================================================
 *  Demo 4: Tree with shared node — double free
 * ============================================================ */
void demo4(void) {
    printf("\n=== Demo 4: Tree with shared node ===\n");
    tracker_init();

    /*       A
     *      / \
     *     B   C
     *      \ /
     *       D   <-- shared
     */
    int *a = tracked_malloc(sizeof(int), "A");
    int *b = tracked_malloc(sizeof(int), "B");
    int *c = tracked_malloc(sizeof(int), "C");
    int *d = tracked_malloc(sizeof(int), "D_shared");
    *a = 1; *b = 2; *c = 3; *d = 4;

    printf("  Tree: A -> {B, C}, B -> D, C -> D (shared)\n");
    printf("  Postorder traversal frees D twice:\n\n");

    /* Simulate postorder: D, B, D(again!), C, A */
    tracked_free(d, "D (from B's subtree)");
    tracked_free(b, "B");
    tracked_free(d, "D (from C's subtree)");  /* double free! */
    tracked_free(c, "C");
    tracked_free(a, "A");

    printf("\n  Fix: use visited set, or don't share nodes in tree.\n");
}

/* ============================================================
 *  Demo 5: Safe free macro
 * ============================================================ */
void demo5(void) {
    printf("\n=== Demo 5: Safe free macro ===\n");

    #define SAFE_FREE(p) do { \
        printf("  SAFE_FREE(%s): ", #p); \
        if (p) { \
            printf("freeing %p\n", (void *)(p)); \
            free(p); \
            (p) = NULL; \
        } else { \
            printf("already NULL\n"); \
        } \
    } while(0)

    int *x = malloc(sizeof(int));
    *x = 42;
    printf("  x = %p, *x = %d\n", (void *)x, *x);

    SAFE_FREE(x);   /* free + set NULL */
    SAFE_FREE(x);   /* x is NULL — no-op */
    SAFE_FREE(x);   /* still NULL — safe */

    printf("\n  SAFE_FREE prevents double free via the SAME pointer.\n");
    printf("  But does NOT help with aliases.\n");

    #undef SAFE_FREE
}

/* ============================================================
 *  Demo 6: Free list corruption visualization
 * ============================================================ */
void demo6(void) {
    printf("\n=== Demo 6: Free list corruption (simulated) ===\n");

    /* Simulate a simple free list */
    typedef struct FreeBlock {
        int id;
        struct FreeBlock *next;
    } FreeBlock;

    FreeBlock blocks[5];
    for (int i = 0; i < 5; i++) {
        blocks[i].id = i;
        blocks[i].next = NULL;
    }

    /* Build free list: 0 -> 1 -> 2 -> NULL */
    FreeBlock *free_list = &blocks[0];
    blocks[0].next = &blocks[1];
    blocks[1].next = &blocks[2];
    blocks[2].next = NULL;

    printf("  Normal free list:\n  ");
    for (FreeBlock *b = free_list; b; b = b->next)
        printf("[%d] -> ", b->id);
    printf("NULL\n");

    /* "Allocate" block 0 (remove from free list) */
    FreeBlock *allocated = free_list;
    free_list = free_list->next;
    printf("\n  After allocating block 0:\n  ");
    for (FreeBlock *b = free_list; b; b = b->next)
        printf("[%d] -> ", b->id);
    printf("NULL\n");
    printf("  allocated = block %d\n", allocated->id);

    /* "Free" block 0 (add back to front) */
    allocated->next = free_list;
    free_list = allocated;
    printf("\n  After free(block 0):\n  ");
    for (FreeBlock *b = free_list; b; b = b->next)
        printf("[%d] -> ", b->id);
    printf("NULL\n");

    /* "Free" block 0 AGAIN (double free!) */
    allocated->next = free_list;
    free_list = allocated;
    printf("\n  After DOUBLE free(block 0):\n  ");
    /* Print only 6 steps to avoid infinite loop */
    int steps = 0;
    for (FreeBlock *b = free_list; b && steps < 6; b = b->next, steps++)
        printf("[%d] -> ", b->id);
    printf("... (CYCLE!)\n");

    printf("\n  Block 0 points to itself -> infinite cycle.\n");
    printf("  Two mallocs would return the SAME block.\n");
    printf("  Writing to one corrupts the other silently.\n");
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
gcc -std=c11 -Wall -Wextra -g -o dblf dblf.c
./dblf
```

### Salida esperada (extracto)

```
=== Demo 1: Basic double free ===
  [malloc]  0x... (p, 4 bytes)
  [free]    0x... (p (first)) — OK
  [FREE]    0x... (p (second)) — DOUBLE FREE DETECTED!

=== Demo 3: Two owners of same block ===
  [malloc]  0x... (shared, 64 bytes)
  owner_a and owner_b both point to 0x...
  [free]    0x... (owner_a) — OK
  [FREE]    0x... (owner_b) — DOUBLE FREE DETECTED!

=== Demo 6: Free list corruption (simulated) ===
  Normal free list:
  [0] -> [1] -> [2] -> NULL

  After DOUBLE free(block 0):
  [0] -> [0] -> [0] -> [0] -> [0] -> [0] -> ... (CYCLE!)
```

---

## 8. Programa en Rust — Ownership elimina double free

```rust
use std::rc::Rc;

/* ============================================================
 *  Rust prevents double free at compile time
 * ============================================================ */

struct Traced {
    name: String,
    data: i32,
}

impl Traced {
    fn new(name: &str, data: i32) -> Self {
        println!("  [new]  {}({})", name, data);
        Traced { name: name.to_string(), data }
    }
}

impl Drop for Traced {
    fn drop(&mut self) {
        println!("  [drop] {}({})", self.name, self.data);
    }
}

/* ============================================================
 *  Demo 1: Move prevents double drop
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Move prevents double drop ===");

    let a = Traced::new("a", 42);
    let b = a;  // move — a invalidated

    println!("  b.data = {}", b.data);

    // drop(a);  // COMPILE ERROR: use of moved value
    // drop(b);
    // drop(b);  // COMPILE ERROR: use of moved value (already dropped)

    println!("  Can't drop 'a' (moved to b).");
    println!("  Can't drop 'b' twice (second would be moved value).");
    // b dropped once at scope end
}

/* ============================================================
 *  Demo 2: Function takes ownership — caller can't double free
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Function takes ownership ===");

    fn consume(t: Traced) {
        println!("  consume: processing {}({})", t.name, t.data);
        // t dropped here
    }

    let data = Traced::new("data", 100);
    consume(data);  // ownership transferred

    // consume(data);  // COMPILE ERROR: use of moved value
    // drop(data);     // COMPILE ERROR: use of moved value

    println!("  After consume(data): can't use 'data' again.");
    println!("  In C: caller might free() after function already freed.");
}

/* ============================================================
 *  Demo 3: Clone creates independent copies — no shared block
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Clone — independent copies ===");

    let original = vec![1, 2, 3, 4, 5];
    let cloned = original.clone();

    println!("  original: {:?} (at {:p})", original, original.as_ptr());
    println!("  cloned:   {:?} (at {:p})", cloned, cloned.as_ptr());
    println!("  Different heap addresses — each has its own block.");

    drop(original);
    drop(cloned);

    println!("  Both dropped — different blocks, no double free.");
    println!("  In C: memcpy without malloc = two pointers to same block.");
}

/* ============================================================
 *  Demo 4: Rc — shared ownership, single free
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Rc — shared ownership ===");

    let shared = Rc::new(Traced::new("shared", 42));

    let owner_a = Rc::clone(&shared);
    let owner_b = Rc::clone(&shared);

    println!("  strong_count = {} (shared + owner_a + owner_b)",
             Rc::strong_count(&shared));

    drop(owner_a);
    println!("  After drop(owner_a): count = {}",
             Rc::strong_count(&shared));

    drop(owner_b);
    println!("  After drop(owner_b): count = {}",
             Rc::strong_count(&shared));

    drop(shared);
    // Traced::drop called exactly ONCE (when count reaches 0)
    println!("  Freed exactly once — Rc guarantees no double free.");
}

/* ============================================================
 *  Demo 5: Option<Box<T>> — explicit nullable ownership
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: Option<Box<T>> — nullable ownership ===");

    let mut maybe = Some(Box::new(Traced::new("boxed", 99)));

    println!("  maybe = Some(...)");

    // "Free" by taking ownership out of the Option
    if let Some(val) = maybe.take() {
        println!("  Took ownership: {}({})", val.name, val.data);
        // val dropped here
    }

    println!("  maybe = {:?}",
             if maybe.is_some() { "Some" } else { "None" });

    // Try to "free" again
    if let Some(val) = maybe.take() {
        println!("  Took: {}", val.name);
    } else {
        println!("  Nothing to take — already None. No double free.");
    }

    println!("\n  Option::take() is the Rust equivalent of:");
    println!("  ptr = p; p = NULL; free(ptr);");
    println!("  Built into the type system — impossible to forget.");
}

/* ============================================================
 *  Demo 6: Tree — each node has exactly one owner
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Tree — no shared nodes ===");

    struct TreeNode {
        data:  &'static str,
        left:  Option<Box<TreeNode>>,
        right: Option<Box<TreeNode>>,
    }

    impl Drop for TreeNode {
        fn drop(&mut self) {
            println!("  [drop] TreeNode({})", self.data);
        }
    }

    // Can't share node D between B and C with Box:
    // let d = Box::new(TreeNode { data: "D", ... });
    // b.left = Some(d);
    // c.left = Some(d);  // COMPILE ERROR: d already moved

    let tree = Box::new(TreeNode {
        data: "A",
        left: Some(Box::new(TreeNode {
            data: "B",
            left: Some(Box::new(TreeNode {
                data: "D_from_B", left: None, right: None,
            })),
            right: None,
        })),
        right: Some(Box::new(TreeNode {
            data: "C",
            left: Some(Box::new(TreeNode {
                data: "D_from_C", left: None, right: None,
            })),
            right: None,
        })),
    });

    println!("  Tree with separate D nodes (no sharing with Box):");
    println!("  A -> {{B -> D_from_B, C -> D_from_C}}");
    println!("  -- dropping tree --");
    drop(tree);
    println!("  Each node dropped exactly once.");
    println!("  Sharing requires Rc, which handles the count.");
}

/* ============================================================
 *  Demo 7: mem::drop is just a function — safe by design
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: drop() is safe by design ===");

    // drop() in Rust is defined as:
    // pub fn drop<T>(_x: T) { }
    // It simply takes ownership (moves the value in) and lets
    // it go out of scope — triggering Drop::drop.

    let a = Traced::new("explicit", 1);
    drop(a);  // a moved into drop(), dropped

    // drop(a);
    // ^ COMPILE ERROR: use of moved value `a`
    // Because drop(a) MOVES a, it can't be used again.

    println!("  fn drop<T>(x: T) {{ }} — moves x, then x goes");
    println!("  out of scope. The move consumes x, preventing reuse.");
    println!("  Double free is structurally impossible.\n");

    println!("  Compare to C:");
    println!("  void free(void *p) — takes a COPY of the pointer.");
    println!("  The original p still holds the address after free().");
    println!("  Nothing stops calling free(p) again.");
}

/* ============================================================
 *  Demo 8: Comprehensive comparison
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: Double free — C vs Rust ===");

    println!("  +----------------------------------+---------+---------+");
    println!("  | Pattern                          | C       | Rust    |");
    println!("  +----------------------------------+---------+---------+");
    println!("  | free(p); free(p);                | UB      | Error*  |");
    println!("  | Free in function + caller        | UB      | Error   |");
    println!("  | Two owners, both free             | UB      | Error   |");
    println!("  | Shared tree node (postorder)     | UB      | Error   |");
    println!("  | free(p); p=NULL; free(p);        | Safe    | N/A     |");
    println!("  | Rc shared ownership              | N/A     | Safe    |");
    println!("  | unsafe raw pointer double free   | N/A     | UB**    |");
    println!("  +----------------------------------+---------+---------+");
    println!("  * Error = compile-time error");
    println!("  ** Only possible in unsafe blocks");
    println!();

    println!("  Why Rust prevents double free structurally:");
    println!("  1. Each value has ONE owner (no aliased ownership)");
    println!("  2. Move semantics: giving away = can't use again");
    println!("  3. drop() takes ownership: calling it = move");
    println!("  4. Compiler tracks ownership through all code paths");
    println!("  5. No runtime overhead — all checks at compile time");
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
rustc -o dblf_rs dblf.rs
./dblf_rs
```

### Salida esperada (extracto)

```
=== Demo 1: Move prevents double drop ===
  [new]  a(42)
  b.data = 42
  Can't drop 'a' (moved to b).
  Can't drop 'b' twice (second would be moved value).
  [drop] a(42)

=== Demo 4: Rc — shared ownership ===
  [new]  shared(42)
  strong_count = 3 (shared + owner_a + owner_b)
  After drop(owner_a): count = 2
  After drop(owner_b): count = 1
  [drop] shared(42)
  Freed exactly once — Rc guarantees no double free.

=== Demo 6: Tree — no shared nodes ===
  ...
  [drop] TreeNode(A)
  [drop] TreeNode(B)
  [drop] TreeNode(D_from_B)
  [drop] TreeNode(C)
  [drop] TreeNode(D_from_C)
  Each node dropped exactly once.
```

---

## 9. Ejercicios

### Ejercicio 1: Encontrar el double free

```c
char *a = malloc(32);
char *b = a;
strcpy(a, "hello");
free(a);
a = NULL;
free(b);
```

Hay double free? El `a = NULL` lo previene?

<details><summary>Prediccion</summary>

**Sí, hay double free.** `free(b)` libera el mismo bloque que
`free(a)` ya liberó, porque `b = a` creó un alias.

`a = NULL` solo protege contra `free(a)` repetido — no afecta a
`b`, que sigue apuntando al bloque original. `b` es un **dangling
pointer** y `free(b)` es double free.

En Rust: `let b = a;` movería `a`, invalidándolo. No habría alias.
O con `let b = &a;` (borrow), `b` no podría liberar nada.

</details>

### Ejercicio 2: Struct destroy doble

```c
typedef struct { int *data; } Container;

void container_destroy(Container *c) {
    free(c->data);
    free(c);
}

Container *c = malloc(sizeof(Container));
c->data = malloc(100);
container_destroy(c);
container_destroy(c);
```

Cuántos errores hay?

<details><summary>Prediccion</summary>

**Cuatro errores** en la segunda llamada a `container_destroy`:

1. `c->data`: acceder a `c` es **use-after-free** (c fue liberado en
   la primera llamada).
2. `free(c->data)`: si logra leer c->data (UB), libera un puntero
   que ya fue liberado — **double free** de data.
3. `free(c)`: **double free** de c.
4. El propio `c->data` en la lectura puede contener basura si el
   allocator reutilizó el bloque de `c`.

En total: 1 use-after-free + 2 double frees en una sola línea de
código. Este es exactamente el patrón que el `SAFE_FREE` macro
previene (si se setea `c = NULL` después de destroy).

</details>

### Ejercicio 3: Free list ciclo

Si la free list tiene un ciclo (por double free), qué pasa con
malloc en un bucle?

```c
for (int i = 0; i < 1000; i++) {
    ptrs[i] = malloc(16);
}
```

<details><summary>Prediccion</summary>

Con un ciclo en la free list (bloque Z apunta a sí mismo):

```
free_list: Z -> Z -> Z -> Z -> ... (infinito)
```

Cada `malloc(16)` retorna Z, pero Z.next también es Z, así que Z
nunca se "consume" de la free list. Resultado:

- `ptrs[0] = Z`
- `ptrs[1] = Z`
- `ptrs[2] = Z`
- ... todos apuntan al **mismo bloque**

Escribir en `ptrs[0]` sobrescribe `ptrs[1]`, `ptrs[2]`, etc.
Corrupción silenciosa masiva.

En la práctica, el allocator puede detectar el ciclo (glibc hace
validaciones), crashear con "double free or corruption", o
comportarse de formas impredecibles dependiendo de la
implementación.

</details>

### Ejercicio 4: SAFE_FREE macro

```c
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while(0)

int *x = malloc(sizeof(int));
int *y = x;
SAFE_FREE(x);
SAFE_FREE(y);
```

Hay double free?

<details><summary>Prediccion</summary>

**Sí, hay double free.** SAFE_FREE(x) libera el bloque y pone
`x = NULL`. Pero `y` sigue apuntando al bloque original.
SAFE_FREE(y) ejecuta `free(y)` que libera el bloque **otra vez**.

La macro solo protege contra `SAFE_FREE(x); SAFE_FREE(x);` (el
mismo puntero dos veces). No protege contra aliases.

Para proteger contra aliases se necesita:
- **No crear aliases** (ownership claro).
- **Reference counting** (solo free cuando count = 0).
- **Un sistema de tipos** que prevenga aliases (como Rust).

</details>

### Ejercicio 5: Rust move vs C pointer copy

```rust
let a = Box::new(42);
let b = a;
```

vs

```c
int *a = malloc(sizeof(int));
int *b = a;
```

Cuál es la diferencia fundamental?

<details><summary>Prediccion</summary>

**En C**: `b = a` copia el **valor del puntero**. Ahora `a` y `b`
contienen la misma dirección. Ambos son válidos. `free(a); free(b);`
es double free. El compilador no sabe ni le importa.

**En Rust**: `let b = a;` **mueve** el ownership. Ahora `b` es el
único owner. `a` es **invalidado** — usarlo es un error de
compilación. Solo se puede hacer `drop(b)`. Double free es
imposible porque no hay dos owners.

La diferencia fundamental: en C, la asignación de punteros crea
**aliases** (múltiples nombres para el mismo recurso). En Rust,
la asignación **transfiere** ownership (un solo nombre válido).

C: `b = a` → dos punteros, un bloque, quién libera?
Rust: `b = a` → un owner, un bloque, ownership claro.

</details>

### Ejercicio 6: Rc strong_count y double free

```rust
let a = Rc::new(42);
let b = Rc::clone(&a);
let c = Rc::clone(&a);
drop(a);
drop(b);
drop(c);
```

Cuántas veces se llama al deallocator?

<details><summary>Prediccion</summary>

**Una sola vez** — cuando `drop(c)` reduce el strong_count a 0.

Timeline:
- Después de las 3 líneas Rc: strong_count = 3.
- `drop(a)`: strong_count = 2. No se libera.
- `drop(b)`: strong_count = 1. No se libera.
- `drop(c)`: strong_count = 0. **Se libera.**

Rc garantiza que la memoria se libera **exactamente una vez**,
sin importar cuántos clones existan. Cada clone incrementa el
count, cada drop lo decrementa. Solo el último drop libera.

Esto es la versión segura del patrón "dos owners" que en C causa
double free. Rc lo resuelve con reference counting — exactamente
el mecanismo de S03/T01.

</details>

### Ejercicio 7: Valgrind output

Valgrind reporta:

```
Invalid free() / delete / delete[]
   at 0x...: free
   by 0x...: list_destroy (list.c:52)
 Address 0x... is 0 bytes inside a block of size 24 free'd
   at 0x...: free
   by 0x...: list_destroy (list.c:52)
 Block was alloc'd at
   at 0x...: malloc
   by 0x...: node_create (list.c:10)
```

Qué tipo de error es? El mismo free en la misma función — cómo?

<details><summary>Prediccion</summary>

Es un **double free**. El mismo `free` en `list_destroy` (línea 52)
se ejecuta dos veces sobre el mismo bloque.

Causa probable: un nodo aparece **dos veces** en la lista. Podría ser:

1. **Nodo compartido**: el mismo nodo fue agregado a la lista dos
   veces (`list_add(list, node); list_add(list, node);`).

2. **Ciclo**: el último nodo apunta de vuelta a un nodo anterior,
   causando que `list_destroy` visite (y libere) el mismo nodo
   dos veces.

3. **Bug en list_add**: inserta el nodo en dos posiciones.

Fix: agregar un check `if (n->freed) continue;` en list_destroy,
o mejor, no permitir que el mismo nodo se agregue dos veces.

En Rust: `list.push(node); list.push(node);` no compila — el
segundo push usa `node` después de que fue movido al primero.

</details>

### Ejercicio 8: unsafe Rust double free

```rust
unsafe {
    let layout = std::alloc::Layout::new::<i32>();
    let ptr = std::alloc::alloc(layout) as *mut i32;
    *ptr = 42;
    std::alloc::dealloc(ptr as *mut u8, layout);
    std::alloc::dealloc(ptr as *mut u8, layout);
}
```

Compila? Qué herramienta lo detecta?

<details><summary>Prediccion</summary>

**Sí, compila.** El código está en un bloque `unsafe` y usa raw
pointers + la API de allocación directa. El borrow checker no
verifica raw pointers.

Es un **double free** — `dealloc` se llama dos veces sobre el mismo
bloque. Es undefined behavior.

Herramientas de detección:
- **Miri**: `cargo +nightly miri run` detecta el doble dealloc
  con un error claro.
- **ASan**: compilar con `-Zsanitizer=address` (nightly) detecta
  el double free en runtime.
- **Valgrind**: funciona como con código C.

El bloque `unsafe` es una señal: "aquí el programador es responsable
de la safety." Las invariantes de Rust (single owner, no double free)
no se verifican automáticamente dentro de `unsafe`.

</details>

### Ejercicio 9: C ownership transfer

Diseña una API en C donde `list_add` transfiere ownership y
`list_destroy` es el único punto de liberación:

<details><summary>Prediccion</summary>

```c
/* Convención: list_add TAKES ownership of node.
   Caller must NOT free node after adding. */

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct List {
    Node *head;
} List;

/* Creates a node — caller owns it */
Node *node_create(int data);

/* Takes ownership of node — caller must NOT free */
void list_add(List *list, Node *node);

/* Frees all nodes — the ONLY place nodes are freed */
void list_destroy(List *list);
```

Documentar la transferencia de ownership es esencial en C porque
el compilador no la verifica. Convenciones comunes:
- "Takes ownership" en comentarios de función.
- Nombrar parámetros `owned_node` o `consumed`.
- El caller setea su puntero a NULL después de entregar.

Esto es lo que Rust hace automáticamente con move semantics — pero
en C depende 100% de la disciplina del programador.

</details>

### Ejercicio 10: Por qué double free es peor que leak

Compara las consecuencias de un memory leak vs un double free en
un servidor de producción:

<details><summary>Prediccion</summary>

| Aspecto | Memory leak | Double free |
|---------|:-----------:|:----------:|
| **Efecto inmediato** | Ninguno | Crash o corrupción |
| **Progresión** | Gradual (horas/días) | Instantáneo |
| **Detectabilidad** | Monitoreo de memoria | Crash dump / core |
| **Reproducibilidad** | Consistente | No determinista |
| **Seguridad** | No explotable* | Explotable (RCE) |
| **Recuperación** | Reiniciar proceso | Reiniciar + investigar |
| **Daño a datos** | Ninguno | Posible corrupción |

*Memory leaks generalmente no son explotables, pero un leak que
consume toda la RAM puede causar DoS.

El double free es **peor** porque:
1. **Corrompe datos silenciosamente**: dos variables distintas
   comparten el mismo bloque sin saberlo.
2. **Es una vulnerabilidad de seguridad**: atacantes pueden
   controlar qué se escribe en el bloque reutilizado.
3. **Es no determinista**: puede funcionar en testing y fallar
   en producción (depende del estado del allocator).
4. **Causa más errores**: un double free puede desencadenar
   use-after-frees, corrupción de heap, y crashes en cascada.

Un leak es un problema de rendimiento. Un double free es un
problema de **correctitud y seguridad**.

</details>
