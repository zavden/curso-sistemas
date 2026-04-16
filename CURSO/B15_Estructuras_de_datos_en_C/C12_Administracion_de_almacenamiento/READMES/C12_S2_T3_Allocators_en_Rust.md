# T03 — Allocators en Rust

## Objetivo

Comprender el ecosistema de allocators en Rust: el trait **`GlobalAlloc`**
(estable) para reemplazar el allocator global, el trait **`Allocator`**
(nightly) para allocators parametricos, y los crates de la comunidad
**`bumpalo`** (arena), **`typed-arena`** y otros. Aprender a usar
`Box::new_in`, `Vec::new_in` y a escribir allocators custom que se
integren con el sistema de tipos de Rust.

---

## 1. Arquitectura de allocacion en Rust

### 1.1 Capas

```
Nivel 3:  Colecciones (Vec, Box, String, HashMap, ...)
              ↓ usan
Nivel 2:  Allocator trait (parametro generico A)
              ↓ por defecto
Nivel 1:  GlobalAlloc (#[global_allocator])
              ↓ delega a
Nivel 0:  System allocator (libc malloc / jemalloc / mimalloc)
```

En Rust estable, las colecciones siempre usan el **global allocator**.
En nightly (y eventualmente en estable), las colecciones aceptan un
parametro generico `A: Allocator`, permitiendo allocators custom por
instancia.

### 1.2 GlobalAlloc vs Allocator

| Aspecto | `GlobalAlloc` | `Allocator` |
|---------|:-------------:|:-----------:|
| Estabilidad | Estable | Nightly (feature `allocator_api`) |
| Scope | Un unico allocator global | Por instancia de coleccion |
| Metodos | `alloc`, `dealloc` | `allocate`, `deallocate`, `grow`, `shrink` |
| Retorno | `*mut u8` (raw) | `Result<NonNull<[u8]>, AllocError>` |
| Error handling | `alloc` retorna null | `allocate` retorna `Err` |
| Realloc | `realloc` (opcional) | `grow` / `shrink` (separados) |
| Uso | `#[global_allocator]` | `Vec::new_in(alloc)`, `Box::new_in(val, alloc)` |

---

## 2. GlobalAlloc: el allocator global

### 2.1 Trait

```rust
use std::alloc::{GlobalAlloc, Layout};

unsafe trait GlobalAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8;
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout);

    // Metodos con implementacion por defecto:
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 { ... }
    unsafe fn realloc(&self, ptr: *mut u8,
                      layout: Layout, new_size: usize) -> *mut u8 { ... }
}
```

### 2.2 Layout

`Layout` encapsula tamano y alineamiento:

```rust
let layout = Layout::new::<i32>();      // size=4, align=4
let layout = Layout::from_size_align(100, 16).unwrap();
let layout = Layout::array::<f64>(1000).unwrap(); // size=8000, align=8
```

`Layout` garantiza invariantes: `align` es potencia de 2, `size` es
multiplo de `align` cuando se usa `pad_to_align()`.

### 2.3 Reemplazar el allocator global

```rust
use std::alloc::{GlobalAlloc, Layout, System};

struct MyAlloc;

unsafe impl GlobalAlloc for MyAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        // delegar al sistema, o implementar custom
        unsafe { System.alloc(layout) }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        unsafe { System.dealloc(ptr, layout) }
    }
}

#[global_allocator]
static ALLOC: MyAlloc = MyAlloc;
```

Todos los `Box::new`, `Vec::new`, `String::from`, etc. usaran `MyAlloc`.

### 2.4 Allocators globales populares

| Crate | Allocator | Uso |
|-------|-----------|-----|
| **jemallocator** | jemalloc | Servidores, buen rendimiento multi-thread |
| **mimalloc** | mimalloc (Microsoft) | Muy rapido, bajo overhead |
| **tikv-jemallocator** | jemalloc (fork TiKV) | Bases de datos, profiling |
| (default) | System (libc malloc) | General |

Cambiar el allocator global puede mejorar el rendimiento un 10-30% en
aplicaciones con muchas allocaciones. Firefox usa jemalloc; Cloudflare
usa mimalloc.

---

## 3. Allocator trait (nightly)

### 3.1 Trait

```rust
#![feature(allocator_api)]

use std::alloc::{Allocator, AllocError, Layout};
use std::ptr::NonNull;

unsafe trait Allocator {
    fn allocate(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocError>;
    unsafe fn deallocate(&self, ptr: NonNull<u8>, layout: Layout);

    // Con implementacion por defecto:
    fn allocate_zeroed(&self, layout: Layout) -> Result<NonNull<[u8]>, AllocError>;
    unsafe fn grow(&self, ptr: NonNull<u8>, old: Layout,
                   new: Layout) -> Result<NonNull<[u8]>, AllocError>;
    unsafe fn shrink(&self, ptr: NonNull<u8>, old: Layout,
                     new: Layout) -> Result<NonNull<[u8]>, AllocError>;
}
```

### 3.2 Diferencias clave con GlobalAlloc

1. **Retorna `NonNull<[u8]>`**: un slice, no un puntero crudo. El
   allocator puede retornar **mas** bytes de los pedidos (el slice
   indica el tamano real).

2. **Result en lugar de null**: manejo de errores idiomatico.

3. **`grow` y `shrink` separados**: mas semanticos que `realloc`. `grow`
   puede fallar sin invalidar el bloque original.

4. **Parametro generico**: las colecciones aceptan `A: Allocator`:
   ```rust
   let v: Vec<i32, &MyArena> = Vec::new_in(&arena);
   let b: Box<[u8], &MyArena> = Box::new_in([0u8; 100], &arena);
   ```

### 3.3 Global implementa Allocator

El tipo `Global` (que delega al `#[global_allocator]`) implementa
`Allocator`. Asi que `Vec<T>` es realmente `Vec<T, Global>`.

---

## 4. bumpalo: arena allocator en Rust

### 4.1 Que es

`bumpalo` es el crate de arena allocator mas popular en Rust. Implementa
un bump allocator con chunks que crecen automaticamente.

```rust
use bumpalo::Bump;

let bump = Bump::new();
let x = bump.alloc(42);          // &mut i32 en la arena
let s = bump.alloc_str("hello"); // &str en la arena
let v = bumpalo::vec![in &bump; 1, 2, 3]; // Vec en la arena
```

### 4.2 Caracteristicas

| Caracteristica | Detalle |
|----------------|---------|
| alloc | $O(1)$ bump pointer |
| free individual | No soportado (como toda arena) |
| Drop de objetos | **No se llama** por defecto |
| Crecimiento | Chunks que duplican tamano |
| Thread safety | No es `Sync` (single-thread) |
| Allocator API | Implementa `Allocator` (nightly) |

### 4.3 Por que no llama Drop

`bumpalo` prioriza rendimiento. Llamar `Drop` requeriria rastrear todos
los objetos alocados y sus tipos — overhead que elimina la ventaja de
la arena.

Para objetos que necesitan `Drop`, bumpalo ofrece `bumpalo::boxed::Box`:

```rust
use bumpalo::Bump;
use bumpalo::boxed::Box as BumpBox;

let bump = Bump::new();
let file = BumpBox::new_in(File::open("x.txt").unwrap(), &bump);
// Drop se llama cuando BumpBox sale de scope
```

### 4.4 Uso con colecciones

```rust
use bumpalo::Bump;
use bumpalo::collections::Vec as BumpVec;
use bumpalo::collections::String as BumpString;

let bump = Bump::new();
let mut v = BumpVec::new_in(&bump);
v.push(1);
v.push(2);

let mut s = BumpString::new_in(&bump);
s.push_str("hello");
```

Todas las allocaciones de `v` y `s` viven en la arena, no en el heap
global.

---

## 5. typed-arena

### 5.1 Que es

`typed-arena` es un arena para objetos de un **unico tipo**. A
diferencia de bumpalo, **si llama Drop** en todos los objetos cuando la
arena se destruye.

```rust
use typed_arena::Arena;

let arena = Arena::new();
let x: &mut i32 = arena.alloc(42);
let y: &mut i32 = arena.alloc(99);
// x e y viven mientras arena viva
// Drop se llama en todos los objetos cuando arena sale de scope
```

### 5.2 Diferencias con bumpalo

| Aspecto | bumpalo | typed-arena |
|---------|---------|-------------|
| Tipos | Cualquier tipo (heterogeneo) | Un solo tipo (homogeneo) |
| Drop | No (excepto BumpBox) | Si, al destruir arena |
| API | `Allocator` (nightly) | Solo `alloc(&self, T) -> &mut T` |
| Colecciones | BumpVec, BumpString | No |
| Rendimiento | Mas rapido (menos overhead) | Ligeramente mas lento (rastrea objetos) |
| Self-referential | Dificil | Natural (todos los refs tienen mismo lifetime) |

### 5.3 Caso ideal: grafos con referencias

`typed-arena` resuelve el problema clasico de Rust con grafos: los nodos
necesitan referenciarse mutuamente, pero el borrow checker no permite
ciclos de `&mut`.

```rust
use typed_arena::Arena;

struct Node<'a> {
    value: i32,
    neighbors: Vec<&'a Node<'a>>,
}

let arena = Arena::new();
let a = arena.alloc(Node { value: 1, neighbors: vec![] });
let b = arena.alloc(Node { value: 2, neighbors: vec![] });
// a y b tienen lifetime 'arena — pueden referenciarse mutuamente
// (con Cell/RefCell para mutabilidad interior)
```

---

## 6. Otros crates de allocators

### 6.1 Tabla comparativa

| Crate | Tipo | Caso de uso |
|-------|------|-------------|
| **bumpalo** | Arena (bump) | Parsing, scratch buffers, compiladores |
| **typed-arena** | Arena tipada | Grafos, ASTs con self-references |
| **slotmap** | Pool con generational indices | ECS, game entities |
| **slab** | Pool indexado | Futures, event loops (tokio) |
| **generational-arena** | Pool con version counter | Game objects |
| **id-arena** | Pool con IDs opacos | Compiladores, IR |

### 6.2 slotmap

`slotmap` combina un pool con **indices generacionales**: cada slot tiene
un contador de version. Cuando se libera y reutiliza un slot, la version
se incrementa. Un handle viejo con version antigua es detectado como
invalido.

```rust
use slotmap::SlotMap;

let mut sm = SlotMap::new();
let k1 = sm.insert("hello");  // Key con generation=0
sm.remove(k1);                // slot liberado, generation++
let k2 = sm.insert("world");  // puede reutilizar el slot
// k1 ya no es valido (generation mismatch)
assert!(sm.get(k1).is_none());
assert_eq!(sm.get(k2), Some(&"world"));
```

### 6.3 slab (tokio)

`slab` es un pool simple indexado por `usize`, usado internamente por
tokio para gestionar futures y recursos de I/O:

```rust
use slab::Slab;

let mut slab = Slab::new();
let k1 = slab.insert("task_a");  // retorna indice usize
let k2 = slab.insert("task_b");
slab.remove(k1);                 // libera el slot
let k3 = slab.insert("task_c");  // reutiliza el slot de k1
```

---

## 7. Escribir un Allocator custom

### 7.1 Pasos

1. Definir el struct del allocator (buffer, estado).
2. Implementar `unsafe impl Allocator` (o `GlobalAlloc`).
3. Garantizar las invariantes de seguridad:
   - `allocate` retorna memoria valida y alineada.
   - La memoria retornada no se solapa con otras allocaciones vivas.
   - `deallocate` recibe un puntero previamente retornado por `allocate`
     con el mismo Layout.

### 7.2 Safety contract

El trait `Allocator` es `unsafe` porque el compilador confia en que:

- La memoria retornada es valida para lectura/escritura.
- La memoria esta correctamente alineada.
- La memoria no se solapa con otras allocaciones activas.
- `deallocate` no causa undefined behavior si se llama con argumentos
  correctos.

Violar estas invariantes causa UB incluso en "safe" Rust que usa el
allocator.

---

## 8. Programa completo en C

```c
/*
 * allocators_rust_concepts.c
 *
 * Demonstrates the C equivalents of Rust allocator concepts:
 * global allocator replacement, arena allocator, typed pool,
 * generational indices, and allocator composition.
 *
 * Compile: gcc -O2 -o allocators_rust_concepts allocators_rust_concepts.c
 * Run:     ./allocators_rust_concepts
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* =================== COUNTING ALLOCATOR (GlobalAlloc concept) ====== */

typedef struct {
    size_t alloc_count;
    size_t dealloc_count;
    size_t bytes_allocated;
    size_t bytes_freed;
} AllocStats;

static AllocStats global_stats = {0};

void *counted_malloc(size_t size) {
    global_stats.alloc_count++;
    global_stats.bytes_allocated += size;
    return malloc(size);
}

void counted_free(void *ptr, size_t size) {
    if (!ptr) return;
    global_stats.dealloc_count++;
    global_stats.bytes_freed += size;
    free(ptr);
}

void print_alloc_stats(const char *label) {
    printf("  [%s] allocs=%zu, deallocs=%zu, bytes_in=%zu, bytes_out=%zu, "
           "live=%zu\n",
           label, global_stats.alloc_count, global_stats.dealloc_count,
           global_stats.bytes_allocated, global_stats.bytes_freed,
           global_stats.bytes_allocated - global_stats.bytes_freed);
}

/* =================== BUMP ARENA (bumpalo concept) ================== */

typedef struct ArenaChunk {
    char *buf;
    size_t capacity;
    size_t offset;
    struct ArenaChunk *next;
} ArenaChunk;

typedef struct {
    ArenaChunk *head;
    ArenaChunk *current;
    size_t total_allocs;
} BumpArena;

static ArenaChunk *chunk_new(size_t cap) {
    ArenaChunk *c = (ArenaChunk *)malloc(sizeof(ArenaChunk));
    c->buf = (char *)malloc(cap);
    c->capacity = cap;
    c->offset = 0;
    c->next = NULL;
    return c;
}

BumpArena bump_new(size_t initial_cap) {
    BumpArena a;
    a.head = chunk_new(initial_cap);
    a.current = a.head;
    a.total_allocs = 0;
    return a;
}

void *bump_alloc(BumpArena *a, size_t size, size_t align) {
    size_t aligned = (a->current->offset + align - 1) & ~(align - 1);
    if (aligned + size > a->current->capacity) {
        size_t new_cap = a->current->capacity * 2;
        if (new_cap < size + align) new_cap = size + align;
        ArenaChunk *nc = chunk_new(new_cap);
        a->current->next = nc;
        a->current = nc;
        aligned = 0;
    }
    void *ptr = a->current->buf + aligned;
    a->current->offset = aligned + size;
    a->total_allocs++;
    return ptr;
}

void bump_reset(BumpArena *a) {
    ArenaChunk *c = a->head;
    while (c) { c->offset = 0; c = c->next; }
    a->current = a->head;
    a->total_allocs = 0;
}

void bump_destroy(BumpArena *a) {
    ArenaChunk *c = a->head;
    while (c) {
        ArenaChunk *next = c->next;
        free(c->buf);
        free(c);
        c = next;
    }
}

/* =================== TYPED ARENA (typed-arena concept) ============= */

typedef struct {
    void *buf;
    size_t elem_size;
    size_t capacity;
    size_t count;
    void (*destructor)(void *);  /* Drop equivalent */
} TypedArena;

TypedArena typed_arena_new(size_t elem_size, size_t initial_cap,
                            void (*dtor)(void *)) {
    TypedArena ta;
    ta.buf = malloc(elem_size * initial_cap);
    ta.elem_size = elem_size;
    ta.capacity = initial_cap;
    ta.count = 0;
    ta.destructor = dtor;
    return ta;
}

void *typed_arena_alloc(TypedArena *ta) {
    if (ta->count >= ta->capacity) {
        ta->capacity *= 2;
        ta->buf = realloc(ta->buf, ta->elem_size * ta->capacity);
    }
    void *ptr = (char *)ta->buf + ta->count * ta->elem_size;
    ta->count++;
    return ptr;
}

void typed_arena_destroy(TypedArena *ta) {
    /* Call destructor on every element (like Rust Drop) */
    if (ta->destructor) {
        for (size_t i = 0; i < ta->count; i++) {
            void *elem = (char *)ta->buf + i * ta->elem_size;
            ta->destructor(elem);
        }
    }
    free(ta->buf);
    ta->count = 0;
}

/* =================== GENERATIONAL INDEX (slotmap concept) ========== */

typedef struct {
    uint32_t index;
    uint32_t generation;
} GenKey;

typedef struct {
    char *data;           /* array of slots */
    uint32_t *generations; /* generation per slot */
    int *free_list;       /* stack of free indices */
    size_t slot_size;
    size_t capacity;
    size_t free_top;      /* top of free stack */
    size_t used;
} GenPool;

GenPool genpool_new(size_t slot_size, size_t capacity) {
    GenPool gp;
    gp.slot_size = (slot_size + 7) & ~(size_t)7;
    gp.capacity = capacity;
    gp.data = (char *)calloc(capacity, gp.slot_size);
    gp.generations = (uint32_t *)calloc(capacity, sizeof(uint32_t));
    gp.free_list = (int *)malloc(capacity * sizeof(int));
    gp.free_top = capacity;
    gp.used = 0;
    for (size_t i = 0; i < capacity; i++)
        gp.free_list[i] = (int)(capacity - 1 - i); /* stack order */
    return gp;
}

GenKey genpool_insert(GenPool *gp, const void *value) {
    GenKey k = {0, 0};
    if (gp->free_top == 0) return k; /* full */
    gp->free_top--;
    uint32_t idx = (uint32_t)gp->free_list[gp->free_top];
    memcpy(gp->data + idx * gp->slot_size, value, gp->slot_size);
    k.index = idx;
    k.generation = gp->generations[idx];
    gp->used++;
    return k;
}

void *genpool_get(GenPool *gp, GenKey key) {
    if (key.index >= gp->capacity) return NULL;
    if (gp->generations[key.index] != key.generation) return NULL;
    return gp->data + key.index * gp->slot_size;
}

bool genpool_remove(GenPool *gp, GenKey key) {
    if (key.index >= gp->capacity) return false;
    if (gp->generations[key.index] != key.generation) return false;
    gp->generations[key.index]++;  /* invalidate old keys */
    gp->free_list[gp->free_top++] = (int)key.index;
    gp->used--;
    return true;
}

void genpool_destroy(GenPool *gp) {
    free(gp->data);
    free(gp->generations);
    free(gp->free_list);
}

/* =================== DEMOS ========================================= */

void demo1_counting_allocator(void) {
    printf("=== Demo 1: Counting Allocator (GlobalAlloc concept) ===\n\n");

    global_stats = (AllocStats){0};
    print_alloc_stats("initial");

    int *a = (int *)counted_malloc(sizeof(int) * 100);
    char *b = (char *)counted_malloc(256);
    double *c = (double *)counted_malloc(sizeof(double) * 50);
    print_alloc_stats("after 3 allocs");

    counted_free(a, sizeof(int) * 100);
    counted_free(b, 256);
    print_alloc_stats("after 2 frees");

    counted_free(c, sizeof(double) * 50);
    print_alloc_stats("after all frees");

    printf("\n  In Rust, #[global_allocator] replaces ALL heap allocations.\n");
    printf("  Every Box::new, Vec::push, String::from goes through it.\n\n");
}

void demo2_bump_arena(void) {
    printf("=== Demo 2: Bump Arena (bumpalo concept) ===\n\n");

    BumpArena a = bump_new(256);

    int *nums = (int *)bump_alloc(&a, 10 * sizeof(int), 4);
    for (int i = 0; i < 10; i++) nums[i] = i * i;

    char *name = (char *)bump_alloc(&a, 64, 1);
    strcpy(name, "bumpalo in C");

    double *vals = (double *)bump_alloc(&a, 5 * sizeof(double), 8);
    for (int i = 0; i < 5; i++) vals[i] = (double)i * 3.14;

    printf("  Arena allocs: %zu (no free-list, no headers)\n", a.total_allocs);
    printf("  nums:  [%d, %d, %d, ..., %d]\n", nums[0], nums[1], nums[2], nums[9]);
    printf("  name:  \"%s\"\n", name);
    printf("  vals:  [%.2f, %.2f, ..., %.2f]\n\n", vals[0], vals[1], vals[4]);

    printf("  Heterogeneous: different types in same arena.\n");
    printf("  Like bumpalo::Bump — alloc any type, no Drop called.\n\n");

    bump_reset(&a);
    printf("  After reset: arena reusable, all pointers invalid.\n\n");
    bump_destroy(&a);
}

void demo3_typed_arena(void) {
    printf("=== Demo 3: Typed Arena (typed-arena concept) ===\n\n");

    typedef struct {
        int id;
        char label[16];
    } Token;

    void token_drop(void *ptr) {
        Token *t = (Token *)ptr;
        printf("    Drop Token{id=%d, label=\"%s\"}\n", t->id, t->label);
    }

    TypedArena ta = typed_arena_new(sizeof(Token), 8, token_drop);

    const char *labels[] = {"fn", "main", "(", ")", "{", "println", "}", ";"};
    for (int i = 0; i < 8; i++) {
        Token *t = (Token *)typed_arena_alloc(&ta);
        t->id = i;
        strncpy(t->label, labels[i], 15);
        t->label[15] = '\0';
    }

    printf("  Allocated %zu tokens.\n\n", ta.count);
    printf("  Destroying arena (Drop called on each token):\n");
    typed_arena_destroy(&ta);
    printf("\n  Like typed_arena::Arena — homogeneous, Drop is called.\n\n");
}

void demo4_generational_indices(void) {
    printf("=== Demo 4: Generational Indices (slotmap concept) ===\n\n");

    typedef struct { float x, y; } Point;
    GenPool gp = genpool_new(sizeof(Point), 8);

    Point p1 = {1.0f, 2.0f};
    Point p2 = {3.0f, 4.0f};
    Point p3 = {5.0f, 6.0f};

    GenKey k1 = genpool_insert(&gp, &p1);
    GenKey k2 = genpool_insert(&gp, &p2);
    GenKey k3 = genpool_insert(&gp, &p3);

    printf("  Inserted 3 points:\n");
    printf("    k1: index=%u, gen=%u\n", k1.index, k1.generation);
    printf("    k2: index=%u, gen=%u\n", k2.index, k2.generation);
    printf("    k3: index=%u, gen=%u\n\n", k3.index, k3.generation);

    /* Access with valid key */
    Point *got = (Point *)genpool_get(&gp, k2);
    printf("  get(k2): (%.1f, %.1f)\n", got->x, got->y);

    /* Remove k2 */
    genpool_remove(&gp, k2);
    printf("  Removed k2.\n");

    /* Try to access with old key */
    got = (Point *)genpool_get(&gp, k2);
    printf("  get(k2 after remove): %s\n",
           got ? "FOUND (bug!)" : "NULL (generation mismatch — safe!)");

    /* Insert new point — may reuse k2's slot */
    Point p4 = {7.0f, 8.0f};
    GenKey k4 = genpool_insert(&gp, &p4);
    printf("  Inserted new: index=%u, gen=%u", k4.index, k4.generation);
    if (k4.index == k2.index)
        printf(" (reused slot of k2, but gen=%u != %u)\n",
               k4.generation, k2.generation);
    else
        printf("\n");

    printf("\n  In Rust: slotmap::SlotMap provides this pattern.\n");
    printf("  Prevents dangling references to removed entities.\n\n");

    genpool_destroy(&gp);
}

void demo5_allocator_composition(void) {
    printf("=== Demo 5: Allocator Composition ===\n\n");

    printf("  Rust's Allocator trait allows composition:\n\n");
    printf("  1. Fallback allocator: try arena, fall back to global\n");
    printf("     fn allocate(&self, layout) -> Result<...> {\n");
    printf("         self.arena.allocate(layout)\n");
    printf("             .or_else(|_| Global.allocate(layout))\n");
    printf("     }\n\n");
    printf("  2. Logging allocator: wrap any allocator with stats\n");
    printf("     fn allocate(&self, layout) -> Result<...> {\n");
    printf("         self.stats.count += 1;\n");
    printf("         self.inner.allocate(layout)\n");
    printf("     }\n\n");

    /* Demonstrate in C: arena with malloc fallback */
    BumpArena a = bump_new(64); /* tiny arena */

    printf("  C simulation: arena(64 bytes) with malloc fallback:\n\n");

    for (int i = 0; i < 5; i++) {
        size_t size = (size_t)(i + 1) * 20;
        void *ptr = bump_alloc(&a, size, 8);
        if (ptr) {
            printf("    alloc(%zu): arena @%p\n", size, ptr);
        } else {
            ptr = malloc(size);
            printf("    alloc(%zu): fallback to malloc @%p\n", size, ptr);
            free(ptr);
        }
    }
    printf("\n");
    bump_destroy(&a);
}

void demo6_layout_exploration(void) {
    printf("=== Demo 6: Layout — Size and Alignment ===\n\n");

    printf("  In Rust, Layout::new::<T>() captures size + align.\n");
    printf("  C equivalent: sizeof + _Alignof.\n\n");

    printf("  %-20s  %5s  %5s  %5s\n", "Type", "Size", "Align", "Padded");

    struct types {
        const char *name;
        size_t size;
        size_t align;
    } t[] = {
        {"char",     sizeof(char),     _Alignof(char)},
        {"int",      sizeof(int),      _Alignof(int)},
        {"long",     sizeof(long),     _Alignof(long)},
        {"double",   sizeof(double),   _Alignof(double)},
        {"void*",    sizeof(void *),   _Alignof(void *)},
        {"struct{c,i}", 8, 4},  /* char + padding + int */
        {"struct{i,c}", 8, 4},  /* int + char + padding */
    };
    int n = sizeof(t) / sizeof(t[0]);

    for (int i = 0; i < n; i++) {
        size_t padded = (t[i].size + t[i].align - 1) & ~(t[i].align - 1);
        printf("  %-20s  %5zu  %5zu  %5zu\n",
               t[i].name, t[i].size, t[i].align, padded);
    }

    printf("\n  Rust's Layout ensures size is always a multiple of align\n");
    printf("  when using pad_to_align() — critical for arrays.\n\n");
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_counting_allocator();
    demo2_bump_arena();
    demo3_typed_arena();
    demo4_generational_indices();
    demo5_allocator_composition();
    demo6_layout_exploration();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * allocators_in_rust.rs
 *
 * Demonstrates Rust's allocator ecosystem: GlobalAlloc,
 * custom allocators, arena patterns, typed arenas,
 * generational indices, and allocator composition.
 *
 * Compile: rustc -o allocators_in_rust allocators_in_rust.rs
 * Run:     ./allocators_in_rust
 */

use std::alloc::{GlobalAlloc, Layout, System};
use std::cell::Cell;
use std::sync::atomic::{AtomicUsize, Ordering};

/* =================== COUNTING GLOBAL ALLOCATOR ===================== */

struct CountingAlloc {
    alloc_count: AtomicUsize,
    dealloc_count: AtomicUsize,
    bytes_in: AtomicUsize,
    bytes_out: AtomicUsize,
}

unsafe impl GlobalAlloc for CountingAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.alloc_count.fetch_add(1, Ordering::Relaxed);
        self.bytes_in.fetch_add(layout.size(), Ordering::Relaxed);
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.dealloc_count.fetch_add(1, Ordering::Relaxed);
        self.bytes_out.fetch_add(layout.size(), Ordering::Relaxed);
        unsafe { System.dealloc(ptr, layout) }
    }
}

unsafe impl Sync for CountingAlloc {}

#[global_allocator]
static ALLOC: CountingAlloc = CountingAlloc {
    alloc_count: AtomicUsize::new(0),
    dealloc_count: AtomicUsize::new(0),
    bytes_in: AtomicUsize::new(0),
    bytes_out: AtomicUsize::new(0),
};

fn snapshot() -> (usize, usize, usize, usize) {
    (
        ALLOC.alloc_count.load(Ordering::Relaxed),
        ALLOC.dealloc_count.load(Ordering::Relaxed),
        ALLOC.bytes_in.load(Ordering::Relaxed),
        ALLOC.bytes_out.load(Ordering::Relaxed),
    )
}

fn print_delta(label: &str, before: (usize, usize, usize, usize)) {
    let after = snapshot();
    println!("  [{}] +allocs={}, +deallocs={}, +bytes_in={}, +bytes_out={}",
             label,
             after.0 - before.0, after.1 - before.1,
             after.2 - before.2, after.3 - before.3);
}

/* =================== BUMP ARENA =================================== */

struct BumpArena {
    buf: Vec<u8>,
    offset: Cell<usize>,
    alloc_count: Cell<usize>,
}

impl BumpArena {
    fn new(capacity: usize) -> Self {
        BumpArena {
            buf: vec![0u8; capacity],
            offset: Cell::new(0),
            alloc_count: Cell::new(0),
        }
    }

    fn alloc<T>(&self, value: T) -> &mut T {
        let align = std::mem::align_of::<T>();
        let size = std::mem::size_of::<T>();
        let offset = self.offset.get();
        let aligned = (offset + align - 1) & !(align - 1);
        assert!(aligned + size <= self.buf.len(), "arena exhausted");

        self.offset.set(aligned + size);
        self.alloc_count.set(self.alloc_count.get() + 1);

        unsafe {
            let ptr = (self.buf.as_ptr() as *mut u8).add(aligned) as *mut T;
            std::ptr::write(ptr, value);
            &mut *ptr
        }
    }

    fn alloc_slice<T: Copy>(&self, values: &[T]) -> &mut [T] {
        let align = std::mem::align_of::<T>();
        let size = std::mem::size_of::<T>() * values.len();
        let offset = self.offset.get();
        let aligned = (offset + align - 1) & !(align - 1);
        assert!(aligned + size <= self.buf.len(), "arena exhausted");

        self.offset.set(aligned + size);
        self.alloc_count.set(self.alloc_count.get() + 1);

        unsafe {
            let ptr = (self.buf.as_ptr() as *mut u8).add(aligned) as *mut T;
            std::ptr::copy_nonoverlapping(values.as_ptr(), ptr, values.len());
            std::slice::from_raw_parts_mut(ptr, values.len())
        }
    }

    fn reset(&self) {
        self.offset.set(0);
        self.alloc_count.set(0);
    }

    fn used(&self) -> usize { self.offset.get() }
    fn count(&self) -> usize { self.alloc_count.get() }
}

/* =================== TYPED ARENA =================================== */

struct TypedArena<T> {
    storage: Vec<T>,
}

impl<T> TypedArena<T> {
    fn new() -> Self { TypedArena { storage: Vec::new() } }

    fn with_capacity(cap: usize) -> Self {
        TypedArena { storage: Vec::with_capacity(cap) }
    }

    fn alloc(&mut self, value: T) -> &T {
        self.storage.push(value);
        self.storage.last().unwrap()
    }

    fn alloc_mut(&mut self, value: T) -> *const T {
        self.storage.push(value);
        self.storage.last().unwrap() as *const T
    }

    fn len(&self) -> usize { self.storage.len() }

    fn iter(&self) -> std::slice::Iter<'_, T> {
        self.storage.iter()
    }
}

/* Drop is called on all elements when TypedArena drops */

/* =================== GENERATIONAL INDEX ============================ */

#[derive(Clone, Copy, Debug, PartialEq)]
struct GenKey {
    index: u32,
    generation: u32,
}

struct SlotEntry<T> {
    value: Option<T>,
    generation: u32,
}

struct GenPool<T> {
    slots: Vec<SlotEntry<T>>,
    free_list: Vec<u32>,
}

impl<T> GenPool<T> {
    fn new() -> Self {
        GenPool {
            slots: Vec::new(),
            free_list: Vec::new(),
        }
    }

    fn insert(&mut self, value: T) -> GenKey {
        if let Some(idx) = self.free_list.pop() {
            let i = idx as usize;
            let gen = self.slots[i].generation;
            self.slots[i].value = Some(value);
            GenKey { index: idx, generation: gen }
        } else {
            let idx = self.slots.len() as u32;
            self.slots.push(SlotEntry { value: Some(value), generation: 0 });
            GenKey { index: idx, generation: 0 }
        }
    }

    fn get(&self, key: GenKey) -> Option<&T> {
        let slot = self.slots.get(key.index as usize)?;
        if slot.generation != key.generation { return None; }
        slot.value.as_ref()
    }

    fn remove(&mut self, key: GenKey) -> Option<T> {
        let i = key.index as usize;
        if i >= self.slots.len() { return None; }
        if self.slots[i].generation != key.generation { return None; }
        self.slots[i].generation += 1; /* invalidate old keys */
        self.free_list.push(key.index);
        self.slots[i].value.take()
    }

    fn len(&self) -> usize {
        self.slots.iter().filter(|s| s.value.is_some()).count()
    }
}

/* =================== SLAB (tokio-style) ============================ */

struct Slab<T> {
    entries: Vec<Option<T>>,
    free_list: Vec<usize>,
}

impl<T> Slab<T> {
    fn new() -> Self {
        Slab { entries: Vec::new(), free_list: Vec::new() }
    }

    fn insert(&mut self, value: T) -> usize {
        if let Some(idx) = self.free_list.pop() {
            self.entries[idx] = Some(value);
            idx
        } else {
            let idx = self.entries.len();
            self.entries.push(Some(value));
            idx
        }
    }

    fn get(&self, idx: usize) -> Option<&T> {
        self.entries.get(idx)?.as_ref()
    }

    fn remove(&mut self, idx: usize) -> Option<T> {
        if idx >= self.entries.len() { return None; }
        let val = self.entries[idx].take()?;
        self.free_list.push(idx);
        Some(val)
    }

    fn len(&self) -> usize {
        self.entries.iter().filter(|e| e.is_some()).count()
    }
}

/* =================== DEMOS ========================================= */

fn demo1_global_allocator() {
    println!("=== Demo 1: Global Allocator (CountingAlloc) ===\n");

    let before = snapshot();

    let v: Vec<i32> = (0..1000).collect();
    let s = String::from("Every allocation goes through CountingAlloc");
    let b = Box::new([0u8; 256]);

    print_delta("after Vec+String+Box", before);

    let before2 = snapshot();
    drop(v);
    drop(s);
    drop(b);
    print_delta("after dropping all", before2);
    println!();
}

fn demo2_bump_arena() {
    println!("=== Demo 2: Bump Arena (bumpalo concept) ===\n");

    let arena = BumpArena::new(4096);

    let x: &mut i32 = arena.alloc(42);
    let y: &mut f64 = arena.alloc(3.14);
    let nums: &mut [i32] = arena.alloc_slice(&[1, 2, 3, 4, 5]);

    println!("  x = {} (i32)", x);
    println!("  y = {} (f64)", y);
    println!("  nums = {:?} ([i32; 5])", nums);
    println!("  Arena: {} bytes used, {} allocs", arena.used(), arena.count());
    println!("  No headers, no free-list — just bump a pointer.\n");

    println!("  Heterogeneous types in one arena (like bumpalo).");
    println!("  No Drop called on reset.\n");

    arena.reset();
    println!("  After reset: {} bytes used.\n", arena.used());
}

fn demo3_typed_arena() {
    println!("=== Demo 3: Typed Arena (typed-arena concept) ===\n");

    #[derive(Debug)]
    struct Token {
        kind: &'static str,
        text: String,
        line: u32,
    }

    impl Drop for Token {
        fn drop(&mut self) {
            println!("    Drop Token {{ kind: {:>8}, text: {:>8} }}",
                     self.kind, self.text);
        }
    }

    {
        let mut arena = TypedArena::new();

        let tokens = [
            ("keyword", "fn", 1), ("ident", "main", 1),
            ("lparen", "(", 1), ("rparen", ")", 1),
            ("lbrace", "{", 2), ("rbrace", "}", 3),
        ];

        for &(kind, text, line) in &tokens {
            arena.alloc(Token { kind, text: text.to_string(), line });
        }

        println!("  {} tokens in typed arena:", arena.len());
        for tok in arena.iter() {
            println!("    line {}: {:>8} = {}", tok.line, tok.kind, tok.text);
        }

        println!("\n  Dropping arena (Drop called on each Token):");
    }
    println!("  All tokens dropped.\n");
}

fn demo4_generational_index() {
    println!("=== Demo 4: Generational Index (slotmap concept) ===\n");

    let mut pool: GenPool<String> = GenPool::new();

    let k1 = pool.insert("player_1".into());
    let k2 = pool.insert("player_2".into());
    let k3 = pool.insert("player_3".into());

    println!("  Inserted 3 entities:");
    println!("    k1: {:?} -> {:?}", k1, pool.get(k1));
    println!("    k2: {:?} -> {:?}", k2, pool.get(k2));
    println!("    k3: {:?} -> {:?}", k3, pool.get(k3));

    println!("\n  Removing k2:");
    pool.remove(k2);
    println!("    get(k2): {:?} (generation mismatch!)", pool.get(k2));

    println!("\n  Inserting new entity (may reuse k2's slot):");
    let k4 = pool.insert("player_4".into());
    println!("    k4: {:?} -> {:?}", k4, pool.get(k4));
    println!("    get(k2): {:?} (still invalid — safe!)", pool.get(k2));
    println!("    Live entities: {}\n", pool.len());
}

fn demo5_slab() {
    println!("=== Demo 5: Slab (tokio-style pool) ===\n");

    let mut slab: Slab<String> = Slab::new();

    let k0 = slab.insert("task_a".into());
    let k1 = slab.insert("task_b".into());
    let k2 = slab.insert("task_c".into());

    println!("  Inserted 3 tasks: indices {}, {}, {}", k0, k1, k2);
    println!("    slab[{}] = {:?}", k1, slab.get(k1));

    slab.remove(k1);
    println!("  Removed index {}.", k1);
    println!("    slab[{}] = {:?}", k1, slab.get(k1));

    let k3 = slab.insert("task_d".into());
    println!("  Inserted task_d: index {} (reused: {})",
             k3, k3 == k1);
    println!("    slab[{}] = {:?}", k3, slab.get(k3));
    println!("    Live: {}\n", slab.len());

    println!("  Unlike GenPool, Slab uses raw indices (no generation).");
    println!("  Simpler but no protection against stale indices.\n");
}

fn demo6_layout() {
    println!("=== Demo 6: Layout — Size and Alignment ===\n");

    println!("  {:<20} {:>5} {:>5} {:>7}",
             "Type", "Size", "Align", "Padded");

    let types: Vec<(&str, Layout)> = vec![
        ("u8",         Layout::new::<u8>()),
        ("i32",        Layout::new::<i32>()),
        ("f64",        Layout::new::<f64>()),
        ("&str",       Layout::new::<&str>()),
        ("String",     Layout::new::<String>()),
        ("Vec<i32>",   Layout::new::<Vec<i32>>()),
        ("Box<i32>",   Layout::new::<Box<i32>>()),
        ("Option<i32>",Layout::new::<Option<i32>>()),
        ("[u8; 100]",  Layout::new::<[u8; 100]>()),
    ];

    for (name, layout) in &types {
        println!("  {:<20} {:>5} {:>5} {:>7}",
                 name, layout.size(), layout.align(),
                 layout.pad_to_align().size());
    }

    println!("\n  Layout::from_size_align examples:");
    for &(size, align) in &[(10, 4), (100, 16), (1, 4096)] {
        match Layout::from_size_align(size, align) {
            Ok(l) => println!("    ({}, {}) -> padded_size={}",
                              size, align, l.pad_to_align().size()),
            Err(e) => println!("    ({}, {}) -> Error: {}", size, align, e),
        }
    }

    println!("\n  Layout::array for contiguous allocation:");
    let arr_layout = Layout::array::<f64>(1000).unwrap();
    println!("    array::<f64>(1000) -> size={}, align={}",
             arr_layout.size(), arr_layout.align());
    println!();
}

fn demo7_arena_for_ast() {
    println!("=== Demo 7: Arena for AST (Real-World Pattern) ===\n");

    #[derive(Debug)]
    enum Expr {
        Num(i64),
        Add(usize, usize),  /* indices into arena */
        Mul(usize, usize),
    }

    let mut arena: TypedArena<Expr> = TypedArena::with_capacity(16);

    /* Build AST for: (1 + 2) * (3 + 4) */
    let _n1 = arena.alloc_mut(Expr::Num(1)); // index 0
    let _n2 = arena.alloc_mut(Expr::Num(2)); // index 1
    let _add1 = arena.alloc_mut(Expr::Add(0, 1)); // index 2
    let _n3 = arena.alloc_mut(Expr::Num(3)); // index 3
    let _n4 = arena.alloc_mut(Expr::Num(4)); // index 4
    let _add2 = arena.alloc_mut(Expr::Add(3, 4)); // index 5
    let _mul = arena.alloc_mut(Expr::Mul(2, 5)); // index 6

    println!("  AST for (1 + 2) * (3 + 4):");
    for (i, expr) in arena.iter().enumerate() {
        println!("    [{}] {:?}", i, expr);
    }

    /* Evaluate */
    fn eval(arena: &TypedArena<Expr>, idx: usize) -> i64 {
        match &arena.storage[idx] {
            Expr::Num(n) => *n,
            Expr::Add(l, r) => eval(arena, *l) + eval(arena, *r),
            Expr::Mul(l, r) => eval(arena, *l) * eval(arena, *r),
        }
    }

    let result = eval(&arena, 6);
    println!("\n  Result: (1 + 2) * (3 + 4) = {}", result);
    println!("  {} nodes, all in arena, contiguous in memory.\n", arena.len());
}

fn demo8_allocator_comparison() {
    println!("=== Demo 8: Allocator Decision Guide ===\n");

    println!("  Scenario -> Recommended Allocator\n");
    println!("  {:40} {:20}", "Scenario", "Allocator");
    println!("  {:40} {:20}", "--------", "---------");

    let scenarios = [
        ("Parse JSON, discard after processing",   "BumpArena (bumpalo)"),
        ("BST nodes with insert/delete",            "Pool (slab crate)"),
        ("Game entities with spawn/despawn",         "GenPool (slotmap)"),
        ("Compiler IR for one function",             "TypedArena"),
        ("HTTP request temporary buffers",           "BumpArena per-request"),
        ("Variable-size cache entries",              "Global (jemalloc)"),
        ("Async task handles (tokio)",               "Slab"),
        ("Graph nodes with cross-references",        "TypedArena + indices"),
    ];

    for (scenario, alloc) in &scenarios {
        println!("  {:40} {:20}", scenario, alloc);
    }

    println!("\n  Rules of thumb:");
    println!("    Same lifetime, varied size  -> Arena (bumpalo)");
    println!("    Individual lifetime, same size -> Pool (slab)");
    println!("    Need dangling detection     -> GenPool (slotmap)");
    println!("    Need Drop on all objects    -> TypedArena");
    println!("    General purpose             -> Global (jemalloc/mimalloc)");
    println!();
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_global_allocator();
    demo2_bump_arena();
    demo3_typed_arena();
    demo4_generational_index();
    demo5_slab();
    demo6_layout();
    demo7_arena_for_ast();
    demo8_allocator_comparison();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — GlobalAlloc vs Allocator

Explica las diferencias entre `GlobalAlloc` y `Allocator` en Rust.
Por que `Allocator` retorna `NonNull<[u8]>` en lugar de `*mut u8`?

<details><summary>Prediccion</summary>

**Diferencias principales**:

| Aspecto | `GlobalAlloc` | `Allocator` |
|---------|:-------------:|:-----------:|
| Estabilidad | Estable | Nightly |
| Scope | Global (uno por programa) | Por instancia |
| Retorno | `*mut u8` (puede ser null) | `Result<NonNull<[u8]>, AllocError>` |
| Error | Retorna null | Retorna `Err(AllocError)` |
| Realloc | Un metodo `realloc` | Separado en `grow` / `shrink` |

**Por que `NonNull<[u8]>`**:

1. **NonNull**: garantiza que el puntero no es null. Los errores se
   manejan con `Result`, no con null. Esto es mas idiomatico en Rust y
   permite `?` para propagacion de errores.

2. **`[u8]`** (slice): el allocator puede retornar **mas** bytes de los
   pedidos. El slice indica el tamano real. Por ejemplo, si pides 100
   bytes y el allocator tiene un bloque de 128 (size class), retorna un
   slice de 128. El usuario puede usar los 28 bytes extra sin otra
   allocacion.

3. **Separation grow/shrink**: `realloc` en GlobalAlloc puede significar
   crecer o encoger. Con `grow`/`shrink` separados, el allocator puede
   optimizar cada caso (ej: `shrink` nunca necesita mover memoria).

</details>

### Ejercicio 2 — Layout invariantes

Cuales de estos `Layout::from_size_align` son validos y cuales fallan?

1. `(0, 1)` — tamano cero, align 1
2. `(100, 16)` — tamano 100, align 16
3. `(100, 15)` — tamano 100, align 15
4. `(usize::MAX, 8)` — tamano maximo
5. `(8, 4096)` — tamano 8, align 4096

<details><summary>Prediccion</summary>

1. `(0, 1)` — **Valido**. Tamano cero esta permitido (zero-sized types
   como `()`). Align 1 es potencia de 2.

2. `(100, 16)` — **Valido**. Ambos razonables. 16 es potencia de 2.

3. `(100, 15)` — **Error**. Align debe ser potencia de 2. 15 no lo es.

4. `(usize::MAX, 8)` — **Error**. El tamano redondeado al alineamiento
   ($\lceil \text{usize::MAX} / 8 \rceil \times 8$) desborda `usize`.
   Layout rechaza tamanos que, con padding, excedan `isize::MAX`.

5. `(8, 4096)` — **Valido**. 4096 es potencia de 2 ($2^{12}$).
   `pad_to_align()` retorna size = 4096 (redondeado desde 8).
   Util para datos alineados a paginas.

**Regla**: `align` debe ser potencia de 2, y `size` redondeado a
`align` no debe exceder `isize::MAX` (para que el puntero sea
representable como offset).

</details>

### Ejercicio 3 — bumpalo y Drop

Por que `bumpalo` no llama `Drop` en los objetos alocados? Da un
ejemplo donde esto causa un bug. Como se soluciona?

<details><summary>Prediccion</summary>

**Por que**: bumpalo es un bump allocator. Su `reset` simplemente mueve
el puntero al inicio — no sabe que tipos se alocaron, donde estan, ni
cuantos son. Rastrear esa informacion agregaria overhead (un registro por
allocacion), eliminando la ventaja de la arena.

**Ejemplo de bug**:
```rust
use bumpalo::Bump;

let bump = Bump::new();
let data = bump.alloc(vec![1, 2, 3]); // Vec aloca en el heap global
// bump se destruye: el &mut Vec se invalida pero
// el Vec interno NO ejecuta Drop -> heap leak!
```

El `Vec<i32>` dentro de la arena tiene su propio buffer en el heap global.
Cuando bumpalo destruye la arena, la memoria de la arena se libera, pero
el buffer del Vec **no** — porque `Drop::drop` nunca se llama.

**Soluciones**:
1. No alocar tipos con Drop en bumpalo. Usar solo `Copy` types.
2. Usar `bumpalo::boxed::Box` que si llama Drop:
   ```rust
   let data = bumpalo::boxed::Box::new_in(vec![1, 2, 3], &bump);
   ```
3. Usar `typed-arena` que llama Drop en todos los objetos.
4. Llamar `drop` manualmente antes del reset.

</details>

### Ejercicio 4 — Generational index

Un `SlotMap` tiene 4 slots. Se ejecutan estas operaciones:

```
k1 = insert("a")  -> slot 0, gen 0
k2 = insert("b")  -> slot 1, gen 0
remove(k1)         -> slot 0 libre, gen 1
k3 = insert("c")  -> slot 0, gen 1
get(k1)            -> ???
get(k3)            -> ???
```

Que retorna cada `get`?

<details><summary>Prediccion</summary>

**`get(k1)`**: k1 = {index: 0, generation: 0}.
El slot 0 tiene generation = 1 (se incremento al hacer remove).
$0 \neq 1$ → **None** (stale key detectada).

**`get(k3)`**: k3 = {index: 0, generation: 1}.
El slot 0 tiene generation = 1, valor = "c".
$1 = 1$ → **Some("c")**.

El slot 0 fue reutilizado, pero la generacion impide que k1 (vieja)
acceda al nuevo valor. Sin generaciones, k1 retornaria "c" — un bug
silencioso (dangling reference disfrazada).

El costo del generational index: 4 bytes extra por slot (u32 generation)
y 4 bytes extra por key. Comparado con los bugs que previene, es un
tradeoff excelente.

</details>

### Ejercicio 5 — Slab vs SlotMap

Compara `slab::Slab` y `slotmap::SlotMap`. Cuando preferir cada uno?
Que pasa si usas un indice invalidado en cada caso?

<details><summary>Prediccion</summary>

| Aspecto | Slab | SlotMap |
|---------|:----:|:-------:|
| Key type | `usize` (raw index) | `Key` (index + generation) |
| Stale key | Accede al nuevo ocupante (bug!) | Retorna None (safe!) |
| Overhead per slot | 0 extra | 4 bytes (generation) |
| Overhead per key | 8 bytes (usize) | 8 bytes (u32 + u32) |
| API | Mas simple | Mas seguro |

**Indice invalidado**:

- **Slab**: `slab[old_index]` retorna el valor del **nuevo** ocupante del
  slot. No hay deteccion del error. Esto es un bug silencioso equivalente
  a use-after-free.

- **SlotMap**: `sm.get(old_key)` retorna `None` porque la generacion no
  coincide. El bug se detecta inmediatamente.

**Cuando usar cada uno**:

- **Slab**: cuando los indices se gestionan cuidadosamente y nunca se usan
  despues de remove. Tipico en event loops donde el framework controla los
  indices (tokio usa Slab internamente).

- **SlotMap**: cuando los indices se comparten entre sistemas y pueden
  volverse stale. Tipico en game engines (ECS) donde un sistema puede
  tener un handle a una entidad que otro sistema destruyo.

</details>

### Ejercicio 6 — Allocator para Vec

Con el trait `Allocator` (nightly), como se crea un `Vec` que use una
arena? Escribe el pseudocodigo. Que pasa cuando el Vec crece (push) y
necesita mas memoria?

<details><summary>Prediccion</summary>

```rust
#![feature(allocator_api)]
use std::alloc::Allocator;

// Suponiendo que BumpArena implementa Allocator:
let arena = BumpArena::new(4096);
let mut v: Vec<i32, &BumpArena> = Vec::new_in(&arena);

v.push(1);  // arena.allocate(Layout for 4 i32s) -> bump
v.push(2);  // fits in current allocation
v.push(3);
v.push(4);
v.push(5);  // Vec needs to grow!
```

**Cuando Vec crece**:
1. Vec llama `arena.grow(old_ptr, old_layout, new_layout)`.
2. La arena **no puede crecer in-place** (bump allocator no soporta
   realloc). `grow` debe allocar nuevo espacio, copiar los datos, y
   "liberar" el viejo (que en una arena es un no-op).
3. Los bytes viejos quedan desperdiciados en la arena.

**Problema**: cada `grow` deja un hueco en la arena. Un Vec con muchos
pushes puede desperdiciar mucha memoria de arena:
$4 + 8 + 16 + 32 + 64 + \ldots$ bytes desperdiciados.

**Solucion**: pre-alocar el Vec con capacidad suficiente:
```rust
let mut v: Vec<i32, &BumpArena> = Vec::with_capacity_in(1000, &arena);
```

Asi no hay grows, y la arena se usa eficientemente.

</details>

### Ejercicio 7 — jemalloc vs system

Un servidor web procesa 10,000 peticiones/segundo. Cada peticion hace
~50 allocaciones de tamanos variados. Que ventaja tiene jemalloc sobre
el allocator del sistema (glibc ptmalloc2)?

<details><summary>Prediccion</summary>

**Carga**: 10,000 req/s × 50 allocs = 500,000 allocs/segundo.

**Ventajas de jemalloc**:

1. **Thread caches**: jemalloc tiene caches per-thread mas agresivos que
   ptmalloc2. Con muchos threads procesando peticiones, reduce la
   contension en locks compartidos.

2. **Size classes granulares**: jemalloc tiene ~200 size classes con
   spacing regular, vs 128 bins con spacing variable en ptmalloc2. Menos
   fragmentacion interna.

3. **Menor fragmentacion a largo plazo**: jemalloc usa "dirty page
   purging" para devolver memoria al OS. ptmalloc2 tiende a retener
   memoria incluso despues de free.

4. **Allocacion de arenas**: jemalloc asigna arenas a threads
   automaticamente, evitando contension. ptmalloc2 tiene un numero fijo
   de arenas.

**Resultado tipico**: 10-30% menos latencia p99, 10-20% menos RSS
despues de horas de ejecucion.

**En Rust**:
```rust
// Cargo.toml: [dependencies] tikv-jemallocator = "0.5"
use tikv_jemallocator::Jemalloc;
#[global_allocator]
static GLOBAL: Jemalloc = Jemalloc;
```

Una sola linea para cambiar el allocator global. Todo `Vec`, `String`,
`Box`, etc. usa jemalloc automaticamente.

</details>

### Ejercicio 8 — Safety de allocators

Un allocator custom tiene un bug: `allocate` a veces retorna punteros
solapados (dos allocaciones comparten bytes). Que consecuencias tiene
esto en safe Rust? Por que el trait es `unsafe`?

<details><summary>Prediccion</summary>

**Consecuencias**:

Dos `Box<i32>` o dos `Vec<u8>` comparten memoria. Escribir en uno
corrompe el otro. Esto es **undefined behavior** — el compilador asume
que las allocaciones no se solapan y optimiza en base a esa asuncion.

Posibles sintomas:
- Datos corrompidos silenciosamente.
- Segfault al acceder a memoria invalida.
- Double free cuando ambos `Box` ejecutan `Drop`.
- Compiler optimizations que eliminan lecturas/escrituras "redundantes"
  porque asumen no-aliasing.

**Por que esto rompe safe Rust**: safe Rust garantiza que `&mut T` es
exclusivo — no hay otro puntero al mismo dato. Si dos allocaciones se
solapan, dos `&mut T` apuntan al mismo byte, violando la garantia
fundamental. El compilador puede reordenar lecturas/escrituras basandose
en exclusividad, produciendo resultados imposibles.

**Por que el trait es `unsafe`**: porque la **seguridad de todo el
programa** depende de que el allocator cumpla su contrato. Un allocator
buggy convierte todo safe Rust en potencial UB. Por eso implementar
`Allocator` requiere `unsafe impl` — el programador garantiza
manualmente que las invariantes se cumplen.

</details>

### Ejercicio 9 — typed-arena para grafos

Explica como `typed-arena` permite construir grafos con referencias
mutuas en Rust, algo que normalmente es dificil con el borrow checker.

<details><summary>Prediccion</summary>

**El problema**: en Rust, un nodo de grafo que referencia a otros necesita
`&Node`. Pero si los nodos se alocan con `Box`, cada nodo tiene su propio
lifetime — y crear un ciclo requeriria que A viva al menos tanto como B
y B al menos tanto como A → imposible con borrows normales.

**La solucion con typed-arena**:

```rust
use typed_arena::Arena;
use std::cell::RefCell;

struct Node<'a> {
    value: i32,
    neighbors: RefCell<Vec<&'a Node<'a>>>,
}

let arena: Arena<Node> = Arena::new();

let a = arena.alloc(Node { value: 1, neighbors: RefCell::new(vec![]) });
let b = arena.alloc(Node { value: 2, neighbors: RefCell::new(vec![]) });
let c = arena.alloc(Node { value: 3, neighbors: RefCell::new(vec![]) });

// Crear ciclo: a -> b -> c -> a
a.neighbors.borrow_mut().push(b);
b.neighbors.borrow_mut().push(c);
c.neighbors.borrow_mut().push(a);  // Ciclo!
```

**Por que funciona**: todos los nodos tienen lifetime `'arena` — viven
exactamente tanto como la arena. Cuando la arena se destruye, todos los
nodos se destruyen simultaneamente. No hay dangling references porque
nadie sobrevive a nadie.

`RefCell` proporciona mutabilidad interior para modificar `neighbors`
a traves de `&Node` (referencia compartida). El borrow checker de Rust
no puede verificar grafos estáticamente, pero `RefCell` aplica las reglas
en runtime.

**Sin arena**: tendriamos que usar `Rc<RefCell<Node>>` con `Weak` para
romper ciclos — mas complejo, mas overhead, mas propenso a leaks.

</details>

### Ejercicio 10 — Compara los crates

Para cada escenario, elige el crate mas apropiado entre `bumpalo`,
`typed-arena`, `slotmap`, `slab`, y justifica:

1. Compilador: nodos de AST con referencias entre si.
2. Servidor async (tokio): pool de conexiones.
3. Game engine: entidades con componentes que spawnan y despawnan.
4. Parser de alto rendimiento: tokens temporales.
5. Base de datos en memoria: indices a registros.

<details><summary>Prediccion</summary>

1. **Compilador (AST con cross-refs)** → **`typed-arena`**.
   Los nodos necesitan referenciarse mutuamente (parent/children). Typed
   arena da lifetime uniforme para todos los nodos, permitiendo `&'arena
   Node` en ambas direcciones. Drop se llama al final. LLVM usa este
   patron.

2. **Servidor async (pool de conexiones)** → **`slab`**.
   Las conexiones se crean y destruyen individualmente. Slab da indices
   `usize` eficientes, y tokio ya usa Slab internamente. No se necesita
   generational safety porque tokio controla los indices.

3. **Game engine (entidades)** → **`slotmap`**.
   Las entidades spawnan y despawnan en cualquier orden. Otros sistemas
   (rendering, physics) guardan handles a entidades. Si una entidad
   muere, los handles viejos deben detectarse como invalidos →
   generational indices son esenciales.

4. **Parser de alto rendimiento** → **`bumpalo`**.
   Los tokens son temporales, se crean durante el parsing y se descartan
   despues. Bumpalo tiene el menor overhead (puro bump pointer).
   No necesitan Drop (los tokens son datos simples). Heterogeneo: tokens,
   strings, nodos, todos en la misma arena.

5. **Base de datos en memoria (indices)** → **`slotmap`**.
   Los registros se insertan y eliminan. Los indices se almacenan en
   tablas secundarias y B-trees. Si un registro se elimina, los indices
   viejos deben invalidarse automaticamente. SlotMap detecta stale keys
   con generaciones.

</details>
