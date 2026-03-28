# T02 — Use-after-free

## Objetivo

Comprender el error **use-after-free** (UAF): acceder a memoria que ya
fue liberada. Es uno de los errores más peligrosos en C/C++ — causa
**undefined behavior**, corrupción de datos, crashes, y es una de las
vulnerabilidades de seguridad más explotadas. Entender cómo Rust lo
previene completamente con el borrow checker.

---

## 1. Qué es use-after-free

### 1.1 Definición

Un **use-after-free** ocurre cuando un programa accede a memoria a
través de un puntero que apunta a un bloque ya liberado con `free()`:

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p);  // USE-AFTER-FREE: undefined behavior
```

Después de `free(p)`, la memoria puede:
- Seguir conteniendo 42 (parece funcionar — el bug más traicionero).
- Haber sido reasignada a otro `malloc` (corrupción silenciosa).
- Haber sido devuelta al OS (SIGSEGV / crash).
- Contener datos arbitrarios del allocator (metadata de free list).

### 1.2 Por qué es tan peligroso

| Peligro | Descripción |
|---------|-------------|
| **Undefined behavior** | El estándar C no define qué pasa — cualquier cosa es válida |
| **Silencioso** | Puede "funcionar" durante meses y fallar en producción |
| **No determinista** | Depende del estado del allocator, timing, carga |
| **Explotable** | Atacantes pueden controlar qué datos leen/escriben |
| **Difícil de reproducir** | El bug puede no manifestarse en testing |

### 1.3 UAF como vulnerabilidad de seguridad

Use-after-free es consistentemente una de las vulnerabilidades más
reportadas en CVEs de Chrome, Firefox, el kernel de Linux, y otros
proyectos en C/C++.

```
Ataque típico:
1. Programa libera bloque A (64 bytes)
2. Atacante causa una asignación de 64 bytes → recibe el mismo bloque
3. Atacante escribe datos controlados en el bloque
4. Programa accede al puntero "stale" de A
5. Lee/escribe datos del atacante → code execution
```

---

## 2. Patrones comunes de use-after-free

### 2.1 Dangling pointer básico

```c
char *name = malloc(32);
strcpy(name, "Alice");
free(name);
/* name sigue apuntando a la dirección liberada */
printf("Name: %s\n", name);  // UAF
```

### 2.2 Retornar puntero a variable local

```c
int *get_value(void) {
    int x = 42;
    return &x;  // x vive en el stack frame de get_value
}   // x destruido aquí

int *p = get_value();
printf("%d\n", *p);  // UAF: stack frame ya no existe
```

### 2.3 Liberar en un path, usar en otro

```c
void process(Node *node, int should_free) {
    if (should_free) {
        free(node);
    }
    /* ... más código ... */
    printf("Node data: %d\n", node->data);  // UAF si should_free=true
}
```

### 2.4 Iterar lista mientras se libera

```c
/* INCORRECTO */
for (Node *n = head; n != NULL; n = n->next) {
    free(n);
    /* n->next es UAF — n ya fue liberado */
}

/* CORRECTO */
Node *n = head;
while (n) {
    Node *next = n->next;  /* guardar ANTES de free */
    free(n);
    n = next;
}
```

### 2.5 Doble indirección: free dentro de función

```c
void remove_node(List *list, Node *target) {
    /* ... unlink target from list ... */
    free(target);
}

Node *n = find_node(list, 42);
remove_node(list, n);
printf("Found: %d\n", n->data);  // UAF: n fue liberado por remove_node
```

### 2.6 Realloc invalida el puntero original

```c
int *arr = malloc(10 * sizeof(int));
int *alias = arr;

arr = realloc(arr, 100 * sizeof(int));
/* Si realloc movió el bloque, 'alias' es un dangling pointer */
alias[0] = 42;  // Posible UAF
```

---

## 3. Prevención en C

### 3.1 Set to NULL after free

```c
free(p);
p = NULL;  /* previene acceso accidental — crash en vez de UB */
```

Limitación: no protege contra **alias** (otros punteros al mismo
bloque):

```c
int *a = malloc(sizeof(int));
int *b = a;  /* alias */
free(a);
a = NULL;
*b = 10;  /* UAF — b sigue apuntando al bloque liberado */
```

### 3.2 Ownership claro

Documentar y mantener un **single owner** por recurso:

```c
/* Convención: la función que crea es responsable de destruir,
   o transfiere ownership explícitamente */
char *name = strdup("Alice");  /* caller owns name */
list_add(list, name);          /* list takes ownership */
/* caller must NOT free name after this point */
```

### 3.3 Defensive programming

```c
void safe_free(void **pp) {
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

/* Uso: */
int *data = malloc(100);
safe_free((void **)&data);  /* free + set to NULL */
```

---

## 4. Cómo Rust previene use-after-free

### 4.1 El borrow checker

Rust previene UAF **en compile time** mediante tres mecanismos:

**1. Ownership + move:**
```rust
let s = String::from("hello");
drop(s);           // s liberado
// println!("{}", s); // COMPILE ERROR: use of moved value
```

**2. Lifetimes de referencias:**
```rust
fn dangling() -> &String {
    let s = String::from("hello");
    &s  // COMPILE ERROR: s no vive lo suficiente
}   // s droppeado aquí, referencia sería inválida
```

**3. Borrow rules (no alias + mutación simultánea):**
```rust
let mut v = vec![1, 2, 3];
let r = &v[0];     // referencia a primer elemento
v.push(4);         // COMPILE ERROR: v se modifica mientras r existe
// push puede reasignar el Vec, invalidando r
```

### 4.2 Por qué funciona

El borrow checker garantiza dos invariantes:

1. **Una referencia nunca sobrevive al valor que referencia.**
   (Previene dangling pointers.)

2. **Mientras exista una referencia inmutable, nadie puede mutar el valor.**
   (Previene invalidación por realloc, push, etc.)

Estas dos reglas eliminan **toda la categoría** de use-after-free
en compile time, sin overhead en runtime.

### 4.3 unsafe Rust

En `unsafe` Rust, es posible crear UAF (raw pointers):

```rust
unsafe {
    let p = Box::into_raw(Box::new(42));
    drop(Box::from_raw(p));  // free
    println!("{}", *p);      // UAF — compila, pero UB
}
```

Por eso `unsafe` existe: marca explícitamente las secciones donde
el programador asume la responsabilidad de la safety. Herramientas
como **Miri** pueden detectar UAF en `unsafe` Rust.

---

## 5. Detección de use-after-free

### 5.1 Herramientas

| Herramienta | Detección | Overhead |
|-------------|-----------|:--------:|
| **ASan** | Quarantine zone: bloques liberados se marcan como "poisoned" | 2-3x |
| **Valgrind** | Shadow memory tracks estado de cada byte | 10-50x |
| **Miri** | Interpreta MIR de Rust, detecta UB | Muy lento |
| **GDB watchpoints** | Breakpoint en acceso a dirección específica | Manual |

### 5.2 ASan quarantine

ASan mantiene una "quarantine zone": los bloques liberados no se
reciclan inmediatamente. Cualquier acceso a un bloque en quarantine
se detecta como UAF:

```
=================================================================
==1234==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010
READ of size 4 at 0x602000000010 thread T0
    #0 0x... in main program.c:8
0x602000000010 is located 0 bytes inside of 4-byte region [0x602..., 0x602...)
freed by thread T0 here:
    #0 0x... in free
    #1 0x... in main program.c:7
previously allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in main program.c:5
```

---

## 6. Programa en C — Demostraciones de use-after-free

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Use-After-Free Demonstrations
 *
 *  WARNING: These demos intentionally create dangling pointers
 *  to illustrate the problem. The actual UAF reads are
 *  commented out or simulated to avoid real UB.
 *
 *  Compile: gcc -std=c11 -Wall -Wextra -g -o uaf uaf.c
 *  With ASan: gcc -fsanitize=address -g -o uaf uaf.c
 * ============================================================ */

/* ---- Simple node ---- */

typedef struct Node {
    int data;
    struct Node *next;
    char label[16];
} Node;

Node *node_new(int data, const char *label) {
    Node *n = malloc(sizeof(Node));
    n->data = data;
    n->next = NULL;
    strncpy(n->label, label, 15);
    n->label[15] = '\0';
    return n;
}

/* ============================================================
 *  Demo 1: Basic dangling pointer
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: Basic dangling pointer ===\n");

    int *p = malloc(sizeof(int));
    *p = 42;
    printf("  Before free: *p = %d (address %p)\n", *p, (void *)p);

    free(p);
    printf("  After free: p still holds address %p\n", (void *)p);
    printf("  Accessing *p now would be USE-AFTER-FREE.\n");

    /* The dangerous line (commented out — real UB):
       printf("  *p = %d\n", *p);
       Could print 42, garbage, or crash. */

    printf("\n  Fix: set p = NULL after free:\n");
    p = NULL;
    printf("  p = %s\n", p ? "non-null" : "NULL (safe)");
    printf("  Accessing *p now would crash (SIGSEGV) — better than UB.\n");
}

/* ============================================================
 *  Demo 2: Dangling pointer with alias
 * ============================================================ */
void demo2(void) {
    printf("\n=== Demo 2: Dangling pointer with alias ===\n");

    Node *original = node_new(100, "original");
    Node *alias = original;  /* alias points to same block */

    printf("  original->data = %d\n", original->data);
    printf("  alias->data    = %d (same block)\n", alias->data);
    printf("  original = %p, alias = %p\n",
           (void *)original, (void *)alias);

    free(original);
    original = NULL;

    printf("\n  After free(original) + original = NULL:\n");
    printf("  original = %s\n", original ? "non-null" : "NULL");
    printf("  alias = %p (STILL POINTS to freed block!)\n",
           (void *)alias);
    printf("  alias->data would be USE-AFTER-FREE.\n");
    printf("\n  Setting to NULL doesn't help aliases.\n");
    printf("  In Rust: alias would be a borrow — compiler prevents this.\n");
}

/* ============================================================
 *  Demo 3: Iterating a list while freeing
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Iterating while freeing ===\n");

    /* Build list: 1 -> 2 -> 3 -> 4 -> 5 */
    Node *head = NULL;
    for (int i = 5; i >= 1; i--) {
        Node *n = node_new(i, "node");
        n->next = head;
        head = n;
    }

    printf("  --- WRONG: access n->next after free(n) ---\n");
    printf("  (Simulated — not actually doing UAF)\n");
    printf("  for (n = head; n; n = n->next) {\n");
    printf("      free(n);     // n freed\n");
    printf("      // n->next   // UAF! n is freed\n");
    printf("  }\n");

    printf("\n  --- CORRECT: save next before free ---\n");
    int count = 0;
    Node *n = head;
    while (n) {
        Node *next = n->next;  /* save before free */
        printf("  free node %d\n", n->data);
        free(n);
        n = next;
        count++;
    }
    printf("  Freed %d nodes safely.\n", count);
}

/* ============================================================
 *  Demo 4: Free inside a function — caller left with dangling ptr
 * ============================================================ */

void process_and_free(Node *n) {
    printf("  process_and_free: processing node %d (%s)\n",
           n->data, n->label);
    free(n);
    printf("  process_and_free: node freed inside function\n");
}

void demo4(void) {
    printf("\n=== Demo 4: Free inside function ===\n");

    Node *n = node_new(42, "important");
    printf("  Caller has node: data=%d\n", n->data);

    process_and_free(n);

    printf("  Caller: n still = %p (dangling!)\n", (void *)n);
    printf("  Accessing n->data would be UAF.\n");
    printf("\n  Problem: caller doesn't know n was freed.\n");
    printf("  Fix: pass Node **n so function can set to NULL,\n");
    printf("  or document ownership transfer clearly.\n");
}

/* ============================================================
 *  Demo 5: Realloc invalidates alias
 * ============================================================ */
void demo5(void) {
    printf("\n=== Demo 5: Realloc invalidates alias ===\n");

    int *arr = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) arr[i] = (i + 1) * 10;

    int *alias = arr;  /* alias to same block */
    printf("  arr = %p, alias = %p (same)\n",
           (void *)arr, (void *)alias);

    printf("  arr: [%d, %d, %d, %d]\n",
           arr[0], arr[1], arr[2], arr[3]);

    /* realloc may move the block */
    int *new_arr = realloc(arr, 100 * sizeof(int));
    if (new_arr) arr = new_arr;

    printf("\n  After realloc(arr, 400):\n");
    printf("  arr   = %p (possibly moved)\n", (void *)arr);
    printf("  alias = %p (still old address!)\n", (void *)alias);

    if (arr != alias) {
        printf("  MOVED! alias is now a DANGLING POINTER.\n");
        printf("  alias[0] would be UAF.\n");
    } else {
        printf("  Not moved (realloc reused same block).\n");
        printf("  alias is still valid — but this is luck, not safety.\n");
    }

    printf("\n  In Rust: Vec::push() invalidates all references.\n");
    printf("  let r = &v[0]; v.push(4); // COMPILE ERROR\n");

    free(arr);
}

/* ============================================================
 *  Demo 6: UAF timing — the silent corruption
 * ============================================================ */
void demo6(void) {
    printf("\n=== Demo 6: UAF — silent corruption scenario ===\n");

    /* Allocate and free a node */
    Node *victim = node_new(999, "victim");
    void *victim_addr = victim;
    printf("  Allocated 'victim' at %p, data=999\n", victim_addr);
    free(victim);
    printf("  Freed 'victim'\n");

    /* Allocate something else — may reuse the same address */
    char *intruder = malloc(sizeof(Node));
    memset(intruder, 0xAA, sizeof(Node));  /* fill with pattern */

    printf("  Allocated 'intruder' at %p\n", (void *)intruder);

    if ((void *)intruder == victim_addr) {
        printf("  SAME ADDRESS! Allocator reused the block.\n");
        printf("  If we read victim->data now, we'd get garbage\n");
        printf("  from intruder's 0xAA pattern — silent corruption.\n");
        printf("  If victim->data were a function pointer:\n");
        printf("  calling it = arbitrary code execution.\n");
    } else {
        printf("  Different address (allocator chose another block).\n");
        printf("  In practice, small blocks are often reused quickly.\n");
    }

    free(intruder);

    printf("\n  This is why UAF is a top security vulnerability:\n");
    printf("  attackers can control what gets allocated in the\n");
    printf("  freed slot and exploit the stale pointer.\n");
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
gcc -std=c11 -Wall -Wextra -g -o uaf uaf.c
./uaf
```

Con ASan para detectar UAF reales:

```bash
gcc -fsanitize=address -g -o uaf_asan uaf.c
./uaf_asan
```

### Salida esperada (extracto)

```
=== Demo 1: Basic dangling pointer ===
  Before free: *p = 42 (address 0x...)
  After free: p still holds address 0x...
  Accessing *p now would be USE-AFTER-FREE.

  Fix: set p = NULL after free:
  p = NULL (safe)
  Accessing *p now would crash (SIGSEGV) — better than UB.

=== Demo 2: Dangling pointer with alias ===
  original->data = 100
  alias->data    = 100 (same block)

  After free(original) + original = NULL:
  original = NULL
  alias = 0x... (STILL POINTS to freed block!)
  Setting to NULL doesn't help aliases.

=== Demo 5: Realloc invalidates alias ===
  arr = 0x..., alias = 0x... (same)
  After realloc(arr, 400):
  arr   = 0x... (possibly moved)
  alias = 0x... (still old address!)
  MOVED! alias is now a DANGLING POINTER.
```

---

## 7. Programa en Rust — El borrow checker previene UAF

```rust
/* ============================================================
 *  Rust prevents Use-After-Free at compile time
 * ============================================================ */

/* ---- Traced type ---- */

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
 *  Demo 1: Use after move — compile error
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Use after move — prevented ===");

    let a = Traced::new("data", 42);
    println!("  a.data = {}", a.data);

    let b = a;  // MOVE
    println!("  b.data = {} (moved from a)", b.data);

    // println!("{}", a.data);
    // ^ COMPILE ERROR: borrow of moved value: `a`

    println!("  Trying to use 'a' after move: COMPILE ERROR.");
    println!("  In C: this would compile and be UAF/UB.");
}

/* ============================================================
 *  Demo 2: Dangling reference — compile error
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Dangling reference — prevented ===");

    // This function would return a reference to a local:
    // fn dangling() -> &i32 {
    //     let x = 42;
    //     &x  // COMPILE ERROR: `x` does not live long enough
    // }

    // Instead, return the value itself (move out):
    fn not_dangling() -> i32 {
        let x = 42;
        x  // move, not reference — safe
    }

    let val = not_dangling();
    println!("  Returned value: {}", val);
    println!("  Returning &local: COMPILE ERROR (would be UAF).");
    println!("  Returning local by value: safe (move).");
}

/* ============================================================
 *  Demo 3: Iterator invalidation — compile error
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Iterator invalidation — prevented ===");

    let mut v = vec![1, 2, 3, 4, 5];
    println!("  v = {:?}", v);

    // This is the C++ equivalent of modifying a container while
    // iterating — causes UAF if the vector reallocates.
    //
    // let r = &v[0];
    // v.push(6);        // COMPILE ERROR: cannot borrow `v` as mutable
    //                   // because it is also borrowed as immutable
    // println!("{}", r);

    println!("  let r = &v[0]; v.push(6);");
    println!("  ^ COMPILE ERROR: mutable borrow while immutable exists.");
    println!("  push() might reallocate, making r a dangling pointer.");
    println!("  Rust prevents this at compile time.\n");

    // Safe alternative: finish using reference before mutating
    {
        let first = v[0];  // copy the value (i32 is Copy)
        println!("  first = {} (copied, not referenced)", first);
        v.push(6);
        println!("  v after push = {:?}", v);
    }
}

/* ============================================================
 *  Demo 4: Scope-based lifetime — reference dies before value
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Scope-based lifetimes ===");

    let outer;
    {
        let inner = Traced::new("inner", 100);
        // outer = &inner;
        // ^ COMPILE ERROR: `inner` does not live long enough
        //   inner is dropped at end of this block,
        //   but outer would still reference it.
        let _ = &inner;  // borrow within the scope — OK
        println!("  &inner valid here (same scope)");
        outer = inner.data;  // copy the i32, not a reference
    }
    println!("  outer = {} (copied value, not a reference)", outer);
    println!("  Referencing inner from outer scope: COMPILE ERROR.");
}

/* ============================================================
 *  Demo 5: Borrow checker with structs
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: Struct field borrows ===");

    struct Container {
        items: Vec<String>,
    }

    let mut c = Container {
        items: vec!["alpha".into(), "beta".into(), "gamma".into()],
    };
    println!("  items: {:?}", c.items);

    // Get a reference to the first item
    let first: &str = &c.items[0];
    println!("  first = {}", first);

    // Cannot modify items while 'first' borrows from them:
    // c.items.push("delta".into());
    // ^ COMPILE ERROR: cannot borrow `c.items` as mutable
    //                  because it is also borrowed as immutable

    // Must drop the reference first:
    println!("  (reference 'first' no longer used after this line)");

    // Now we can mutate:
    c.items.push("delta".into());
    println!("  items after push: {:?}", c.items);

    println!("\n  The borrow checker tracks references through structs.");
    println!("  No runtime cost — all checks at compile time.");
}

/* ============================================================
 *  Demo 6: Safe vs unsafe — raw pointers CAN cause UAF
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Safe vs unsafe Rust ===");

    // Safe Rust: impossible to create UAF
    {
        let data = Box::new(42);
        let _r = &*data;
        // drop(data);
        // ^ COMPILE ERROR: cannot move out of `data` because it is borrowed
        println!("  Safe Rust: can't drop while borrowed.");
    }

    // Unsafe Rust: CAN create UAF (programmer's responsibility)
    println!("\n  Unsafe Rust: raw pointers bypass borrow checker.");
    println!("  unsafe {{");
    println!("      let p = Box::into_raw(Box::new(42));");
    println!("      drop(Box::from_raw(p));  // free");
    println!("      *p  // UAF — compiles but is UB!");
    println!("  }}");
    println!("  Use Miri to detect this: cargo +nightly miri run");

    // Safe alternative: Option to represent nullable ownership
    let mut maybe: Option<Box<i32>> = Some(Box::new(42));
    println!("\n  Option<Box<i32>> = {:?}", maybe);
    maybe = None;  // dropped
    println!("  After = None: {:?} (no dangling pointer)", maybe);
    // Can't access the old value — it's None
}

/* ============================================================
 *  Demo 7: Lifetime annotations — explicit guarantees
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: Lifetime annotations ===");

    // This function guarantees: the returned reference lives
    // as long as the input reference.
    fn longest<'a>(a: &'a str, b: &'a str) -> &'a str {
        if a.len() >= b.len() { a } else { b }
    }

    let result;
    {
        let s1 = String::from("long string");
        let s2 = String::from("short");
        result = longest(&s1, &s2);
        println!("  longest: {}", result);
        // result is used within the scope where s1, s2 are alive — OK
    }
    // Using result here would be COMPILE ERROR:
    // println!("{}", result);
    // ^ s1 and s2 no longer live — result would be dangling

    println!("  Lifetime 'a ties output to inputs.");
    println!("  Using result after inputs die: COMPILE ERROR.");
    println!("  This is the compile-time equivalent of GC tracing.");
}

/* ============================================================
 *  Demo 8: Complete comparison — C patterns vs Rust
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: UAF patterns — C vs Rust ===");

    println!("  +----------------------------------+---------+---------+");
    println!("  | UAF Pattern                      | C       | Rust    |");
    println!("  +----------------------------------+---------+---------+");
    println!("  | Use after free(p)                | UB      | Error*  |");
    println!("  | Return &local                    | UB      | Error   |");
    println!("  | Alias after free                 | UB      | Error   |");
    println!("  | Iterator invalidation (push)     | UB      | Error   |");
    println!("  | Free in function, use in caller  | UB      | Error   |");
    println!("  | Realloc invalidates alias        | UB      | Error   |");
    println!("  | Raw pointer after free (unsafe)  | UB      | UB**    |");
    println!("  +----------------------------------+---------+---------+");
    println!("  * Error = compile-time error (program doesn't build)");
    println!("  ** unsafe Rust can have UAF — use Miri to detect");
    println!();

    println!("  Detection tools comparison:");
    println!("  +---------------+----------+-----------+----------+");
    println!("  | Tool          | Language | When      | Overhead |");
    println!("  +---------------+----------+-----------+----------+");
    println!("  | Borrow checker| Rust     | Compile   | 0%       |");
    println!("  | ASan          | C/C++    | Runtime   | 2-3x     |");
    println!("  | Valgrind      | C/C++    | Runtime   | 10-50x   |");
    println!("  | Miri          | Rust     | Interpret | ~100x    |");
    println!("  +---------------+----------+-----------+----------+");
    println!();
    println!("  Rust's borrow checker: zero overhead, zero false negatives,");
    println!("  catches ALL UAF in safe code before the program even runs.");
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
rustc -o uaf_rs uaf.rs
./uaf_rs
```

### Salida esperada (extracto)

```
=== Demo 1: Use after move — prevented ===
  [new]  data(42)
  a.data = 42
  b.data = 42 (moved from a)
  Trying to use 'a' after move: COMPILE ERROR.
  In C: this would compile and be UAF/UB.
  [drop] data(42)

=== Demo 3: Iterator invalidation — prevented ===
  v = [1, 2, 3, 4, 5]
  let r = &v[0]; v.push(6);
  ^ COMPILE ERROR: mutable borrow while immutable exists.
  push() might reallocate, making r a dangling pointer.
  Rust prevents this at compile time.

=== Demo 7: Lifetime annotations ===
  longest: long string
  Lifetime 'a ties output to inputs.
  Using result after inputs die: COMPILE ERROR.
```

---

## 8. Ejercicios

### Ejercicio 1: Identificar el UAF

```c
Node *a = node_new(10, "A");
Node *b = node_new(20, "B");
a->next = b;
free(b);
printf("%d\n", a->next->data);
```

Hay UAF? Dónde?

<details><summary>Prediccion</summary>

**Sí, hay UAF** en la última línea: `a->next->data`.

`a->next` apunta a `b`, que fue liberado con `free(b)`. Acceder a
`a->next->data` es leer memoria ya liberada — use-after-free.

El puntero `a->next` es un **dangling pointer**: sigue apuntando a
la dirección donde estaba `b`, pero esa memoria ya fue devuelta al
allocator.

Fix: antes de liberar `b`, desconectar de `a`:
```c
a->next = NULL;
free(b);
```

En Rust: `a.next = Some(Box::new(b))` haría que `a` sea owner de `b`.
No se puede liberar `b` independientemente.

</details>

### Ejercicio 2: Realloc con alias

```c
int *arr = malloc(3 * sizeof(int));
int *first = &arr[0];
arr = realloc(arr, 1000 * sizeof(int));
*first = 42;
```

Es seguro?

<details><summary>Prediccion</summary>

**No es seguro.** Si `realloc` necesita mover el bloque (porque no
hay espacio contiguo para expandirlo), el puntero original es
liberado y `arr` recibe una nueva dirección. Pero `first` sigue
apuntando a `&arr[0]` **de la dirección original** (ya liberada).

`*first = 42` sería **UAF**: escribiendo en memoria liberada.

Si `realloc` no mueve el bloque (hay espacio), `first` sigue
siendo válido — pero esto depende del estado del allocator y
**no se puede garantizar**.

Fix: recalcular `first` después de realloc:
```c
arr = realloc(arr, 1000 * sizeof(int));
first = &arr[0];  // recalcular
```

En Rust: `let r = &v[0]; v.push(x);` no compila — el borrow checker
detecta que push puede invalidar `r`.

</details>

### Ejercicio 3: Stack UAF

```c
int *get_array(void) {
    int arr[5] = {1, 2, 3, 4, 5};
    return arr;
}

int *p = get_array();
printf("%d\n", p[0]);
```

Qué pasa?

<details><summary>Prediccion</summary>

**UAF (stack).** `arr` es una variable local que vive en el stack
frame de `get_array`. Cuando la función retorna, ese stack frame
se destruye. `p` apunta a una dirección del stack que ya no es
válida.

`p[0]` podría:
- Devolver 1 (los datos aún están en el stack pero es suerte).
- Devolver basura (otro function call sobrescribió el frame).
- Causar un crash (menos probable para stack).

GCC genera un warning: `function returns address of local variable`.

Fix: usar `malloc` para que los datos vivan en el heap, o pasar un
buffer como parámetro:
```c
void get_array(int *out, int n) {
    for (int i = 0; i < n; i++) out[i] = i + 1;
}
```

En Rust: `fn get() -> &[i32]` con un array local no compila:
`cannot return reference to local variable`.

</details>

### Ejercicio 4: Free en bucle

```c
Node *head = /* lista de 5 nodos */;
Node *n = head;
while (n) {
    free(n);
    n = n->next;
}
```

Cuántos UAF hay?

<details><summary>Prediccion</summary>

**4 UAFs.** Cada iteración (excepto la última) hace `n = n->next`
después de `free(n)` — eso es leer `n->next` de un bloque liberado.

- Iteración 1: free(nodo1), leer nodo1->next → UAF
- Iteración 2: free(nodo2), leer nodo2->next → UAF
- Iteración 3: free(nodo3), leer nodo3->next → UAF
- Iteración 4: free(nodo4), leer nodo4->next → UAF
- Iteración 5: free(nodo5), leer nodo5->next → el valor leído es
  UB, pero si da NULL el bucle termina.

Fix:
```c
while (n) {
    Node *next = n->next;  // guardar ANTES
    free(n);
    n = next;
}
```

</details>

### Ejercicio 5: Rust lifetime error

```rust
fn first_word(s: &String) -> &str {
    &s[..1]
}

let result;
{
    let s = String::from("hello");
    result = first_word(&s);
}
println!("{}", result);
```

Compila?

<details><summary>Prediccion</summary>

**No compila.** El error es:
```
`s` does not live long enough
```

`first_word` retorna un `&str` cuyo lifetime está ligado al lifetime
de `&s` (por lifetime elision: `fn first_word<'a>(s: &'a String) -> &'a str`).

`result` intenta guardar esa referencia, pero `s` se droppea al
cerrar el bloque `{}`. Después del bloque, `result` sería un
dangling reference — exactamente un UAF.

El borrow checker detecta que `result` (que vive hasta el `println!`)
sobrevive a `s` (que muere al final del bloque), y rechaza el código.

Fix: mover `println!` dentro del bloque, o clonar el resultado:
```rust
let result = {
    let s = String::from("hello");
    s[..1].to_string()  // clone — owns the data
};
println!("{}", result);  // OK — result owns its data
```

</details>

### Ejercicio 6: Vec invalidation

```rust
let mut v = vec![1, 2, 3];
let r = &v;
v.clear();
println!("{:?}", r);
```

Compila?

<details><summary>Prediccion</summary>

**No compila.** `r` es una referencia inmutable a `v`. `v.clear()`
requiere `&mut self` — un borrow mutable. Rust prohíbe tener un
borrow inmutable (`r`) y uno mutable (`clear`) simultáneamente.

Error:
```
cannot borrow `v` as mutable because it is also borrowed as immutable
```

Aunque `clear` no libera la memoria del Vec (solo pone len=0),
el borrow checker no analiza la semántica de cada método — solo
verifica las reglas de borrowing. Esto es conservador pero seguro.

Fix: usar `r` antes de mutar:
```rust
let mut v = vec![1, 2, 3];
let r = &v;
println!("{:?}", r);  // usar r aquí
v.clear();            // mutar después — OK
```

</details>

### Ejercicio 7: ASan output

ASan reporta:

```
heap-use-after-free on address 0x602000000010
READ of size 4 at 0x602000000010
freed by thread T0 here:
    #0 in free
    #1 in remove_node (list.c:45)
previously allocated here:
    #0 in malloc
    #1 in create_node (list.c:12)
```

Qué pasó y cómo lo arreglarías?

<details><summary>Prediccion</summary>

El reporte dice:
1. Un bloque fue asignado en `create_node` (list.c:12).
2. Fue liberado en `remove_node` (list.c:45).
3. Después de ser liberado, alguien leyó 4 bytes de ese bloque.

Escenario probable: `remove_node` libera un nodo, pero el caller
(o código posterior) sigue usando un puntero al nodo eliminado.

```c
Node *n = find_node(list, key);
remove_node(list, n);    // free(n) dentro
printf("%d", n->data);   // UAF: n ya liberado
```

Fix:
- Copiar los datos necesarios antes de llamar `remove_node`.
- O hacer que `remove_node` no libere, solo desenlace (unlink), y
  dejar al caller decidir cuándo liberar.
- O pasar `Node **n` para que `remove_node` pueda setear a NULL.

</details>

### Ejercicio 8: Double indirection — safe free

```c
void safe_free(void **pp) {
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}
```

Esto previene UAF completamente?

<details><summary>Prediccion</summary>

**No completamente.** Previene UAF a través del **mismo puntero**
(porque se setea a NULL), pero no protege contra **aliases**:

```c
int *a = malloc(sizeof(int));
int *b = a;                    // alias
safe_free((void **)&a);        // a = NULL
*b = 42;                       // UAF! b sigue apuntando al bloque
```

`safe_free` es una mejora sobre `free()` desnudo, pero no es una
solución completa. Solo un sistema de ownership (como Rust) o un
GC puede rastrear **todos** los alias de un bloque.

Además, el cast `(void **)&a` viola strict aliasing en C — es
technically undefined behavior, aunque funciona en la práctica con
la mayoría de compiladores.

</details>

### Ejercicio 9: Rust unsafe UAF

```rust
unsafe {
    let p: *mut i32 = Box::into_raw(Box::new(42));
    let val1 = *p;
    drop(Box::from_raw(p));
    let val2 = *p;
    println!("{} {}", val1, val2);
}
```

Compila? Es correcto?

<details><summary>Prediccion</summary>

**Compila** (está dentro de `unsafe`, el borrow checker no verifica
raw pointers). Pero es **incorrecto** — tiene UAF.

`Box::into_raw` convierte el Box en un raw pointer `*mut i32`.
`Box::from_raw(p)` reconstruye el Box, y `drop` lo libera.
`*p` después del drop es **use-after-free** — undefined behavior.

`val1` es 42 (lectura antes del free — válida).
`val2` es UB (lectura después del free — UAF).

**Miri** detectaría esto:
```
error: Undefined Behavior: pointer to alloc was dereferenced
       after this allocation got freed
```

Este ejemplo muestra por qué `unsafe` debe usarse con extremo
cuidado y verificarse con Miri.

</details>

### Ejercicio 10: Diseño — quién es owner?

Tienes una estructura donde un `HashMap<String, Node>` y un
`Vec<&Node>` (lista de favoritos) apuntan a los mismos nodos.
Cómo diseñarías esto en C y en Rust para evitar UAF?

<details><summary>Prediccion</summary>

**En C**: documentar que el `HashMap` es el **owner** y el `Vec`
solo tiene punteros observadores. Reglas:
- Solo el HashMap puede liberar nodos.
- El Vec debe invalidar sus punteros cuando un nodo se elimina del
  HashMap (difícil de mantener manualmente).
- Alternativa: usar índices (enteros) en el Vec en lugar de punteros.

**En Rust** — varias opciones:

1. **Indices en lugar de referencias** (mejor opción):
   ```rust
   let map: HashMap<String, Node> = ...;
   let favorites: Vec<String> = ...; // keys, not references
   // Access: map[&favorites[0]]
   ```

2. **Rc compartido**:
   ```rust
   let map: HashMap<String, Rc<Node>> = ...;
   let favorites: Vec<Rc<Node>> = ...;
   // Ambos comparten ownership — sin UAF pero con overhead de RC.
   ```

3. **Arena + referencias** (si lifetimes son uniformes):
   ```rust
   let arena = typed_arena::Arena::new();
   let node = arena.alloc(Node { ... });
   // map y favorites tienen &Node — todos viven mientras arena viva.
   ```

La opción de índices es la más idiomática en Rust: evita lifetimes
complejos, Rc overhead, y UAF. Es el equivalente de generational
indices (slotmap) que vimos en S02/T03.

</details>
