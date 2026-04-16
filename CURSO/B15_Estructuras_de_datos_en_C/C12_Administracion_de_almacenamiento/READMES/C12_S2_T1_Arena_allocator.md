# T01 — Arena allocator

## Objetivo

Comprender e implementar un **arena allocator** (tambien llamado *bump
allocator*, *linear allocator* o *region-based allocator*): una estrategia
de gestion de memoria donde se asigna linealmente incrementando un puntero
y se libera **todo de golpe**. Analizar sus casos de uso ideales, sus
limitaciones, y como se integra con allocators de proposito general.

---

## 1. Concepto fundamental

### 1.1 La idea

Un arena allocator es la estrategia de allocacion mas simple posible:

1. Reservar un bloque grande de memoria (la **arena**).
2. Mantener un **puntero de avance** (*bump pointer*) que empieza al
   inicio de la arena.
3. Para cada `alloc(n)`: retornar el puntero actual y avanzarlo en $n$
   bytes (mas alineamiento).
4. **No hay `free` individual**. Para liberar, se **resetea** el puntero
   al inicio — liberando toda la arena de golpe.

```
Arena de 1024 bytes:

Inicial:
  [=========================FREE=========================]
  ^bump

alloc(100):
  [===USED:100===][=================FREE=================]
                  ^bump

alloc(64):
  [===USED:100===][==USED:64==][=========FREE============]
                               ^bump

alloc(200):
  [===USED:100===][==USED:64==][=====USED:200=====][=FREE]
                                                    ^bump

reset():
  [=========================FREE=========================]
  ^bump    (todo liberado instantaneamente)
```

### 1.2 Complejidades

| Operacion | Complejidad | Detalle |
|-----------|:-----------:|---------|
| alloc | $O(1)$ | Incrementar puntero + alineamiento |
| free individual | N/A | No soportado |
| reset (free all) | $O(1)$ | Resetear puntero |

Comparar con `malloc`/`free`:

| Operacion | malloc/free | Arena |
|-----------|:-----------:|:-----:|
| alloc | $O(k)$ free blocks | $O(1)$ |
| free | $O(1)$ a $O(k)$ | N/A |
| free all | $O(n)$ | $O(1)$ |

### 1.3 Overhead por allocacion

- **malloc**: header de 16-32 bytes por bloque + alineamiento.
- **Arena**: **cero metadata** por allocacion. Solo el padding de
  alineamiento.

Para un millon de allocaciones de 8 bytes:
- malloc: $10^6 \times (8 + 16) = 24$ MB.
- Arena: $10^6 \times 8 = 8$ MB (+ unos pocos bytes de padding).

---

## 2. Alineamiento

### 2.1 Por que importa

Muchas arquitecturas requieren que los datos esten alineados a su tamano
natural: un `int32` a 4 bytes, un `double` a 8, un `__m256` (AVX) a 32.
Accesos no alineados pueden ser lentos (penalizacion de rendimiento) o
causar una excepcion (`SIGBUS` en algunas plataformas).

### 2.2 Implementacion en la arena

Antes de retornar el puntero, se **redondea hacia arriba** al multiplo
del alineamiento requerido:

$$\text{aligned} = (\text{bump} + \text{align} - 1)\ \&\ \sim(\text{align} - 1)$$

Si el usuario pide 12 bytes con alineamiento a 8:

```
bump = 100
aligned = (100 + 7) & ~7 = 104
new_bump = 104 + 12 = 116
return 104
```

Los 4 bytes entre 100 y 104 son **padding** — desperdicio inevitable para
garantizar alineamiento.

### 2.3 Alineamiento por defecto

Un arena de proposito general usa alineamiento a $\max(\text{sizeof(void*)}, 8)$
= 8 bytes en 64-bit. Esto satisface la mayoria de tipos. Para tipos con
alineamiento especial (SIMD, paginas), se ofrece una funcion `alloc_aligned`.

---

## 3. Crecimiento de la arena

### 3.1 Arena de tamano fijo

La arena mas simple tiene un buffer estatico o un unico `malloc`. Si se
agota, `alloc` retorna `NULL`.

```
Arena fija de 4096 bytes:
  alloc(3000) -> OK, bump=3000
  alloc(2000) -> FAIL (3000 + 2000 > 4096)
```

### 3.2 Arena con chunks encadenados

Para arenas que pueden crecer, se usa una **lista enlazada de chunks**:

```
Chunk 1 (4096 bytes):
  [=====USED=====][=====USED=====][FULL]
                                   ↓ (get new chunk)
Chunk 2 (8192 bytes):
  [===USED===][===========FREE===========]
               ^bump
```

Cuando el chunk actual se agota:
1. Alocar un nuevo chunk (tipicamente el doble del anterior).
2. Encadenarlo a la lista.
3. Resetear el bump pointer al inicio del nuevo chunk.

Al hacer `reset`, se liberan todos los chunks (o se retienen para
reutilizarlos).

### 3.3 Politica de crecimiento

| Politica | Tamano del chunk $n$ | Uso |
|----------|:-------------------:|-----|
| Fijo | Siempre $C$ | Predecible, bueno para embebido |
| Duplicar | $C, 2C, 4C, \ldots$ | General, amortiza allocaciones |
| Adaptativo | Basado en uso previo | Frameworks de alto rendimiento |

La politica de **duplicar** da un costo amortizado de $O(1)$ por byte
alocado (analisis analogo a `Vec::push` en Rust o `std::vector` en C++).

---

## 4. Casos de uso ideales

### 4.1 Patron de vida uniforme

El arena allocator es optimo cuando todas las allocaciones tienen el
**mismo lifetime** — se crean durante una fase y se descartan juntas:

| Caso de uso | Fase de allocacion | Punto de liberacion |
|-------------|-------------------|---------------------|
| **Parsing** | Tokenizar + construir AST | Despues de procesar el AST |
| **Frame de juego** | Datos por frame (particulas, UI) | Al inicio del siguiente frame |
| **Peticion HTTP** | Headers, body, objetos temporales | Al finalizar la respuesta |
| **Compilador** | IR por funcion | Despues de emitir codigo para esa funcion |
| **Tests** | Fixtures de test | Al finalizar el test |

### 4.2 Antipatrones

El arena **no es adecuado** cuando:

- Los objetos tienen **lifetimes diferentes** (algunos deben sobrevivir
  al reset).
- Se necesita **liberar memoria individual** para mantener el uso bajo.
- La arena crece sin limite porque los objetos se acumulan sin reset.

### 4.3 Arenas anidadas y scoped

Se pueden usar **multiples arenas** para diferentes lifetimes:

```
Arena permanente:  datos que viven toda la ejecucion
Arena por-request: datos de una peticion HTTP
Arena temporal:    calculos intermedios dentro de la peticion
```

Al finalizar un calculo intermedio, se resetea la arena temporal. Al
finalizar la peticion, se resetea la arena por-request. La arena
permanente nunca se resetea.

---

## 5. Implementacion: consideraciones

### 5.1 Thread safety

Una arena basica no es thread-safe. Opciones:

1. **Arena por thread**: cada thread tiene su propia arena. Sin contension.
   Ideal para procesamiento paralelo de peticiones.
2. **Arena con mutex**: proteger el bump pointer con un lock. Simple pero
   con contension.
3. **Arena con atomic**: usar `fetch_add` atomico en el bump pointer.
   Lock-free pero puede desperdiciar bytes en contension alta.

### 5.2 Destruccion de objetos

En C, la arena simplemente libera la memoria sin llamar destructores.
Si los objetos contienen recursos (file descriptors, mutexes, etc.), el
usuario debe liberarlos manualmente antes del reset.

En Rust, el problema es mas visible: los objetos en la arena no ejecutan
`Drop` al hacer reset. Crates como `typed-arena` llaman `Drop` en todos
los objetos al destruir la arena; `bumpalo` no (por diseno — los objetos
deben ser `Copy` o no necesitar `Drop`).

### 5.3 Scratch space reutilizable

Un patron comun es usar la arena como **scratch buffer** dentro de un
loop:

```
for each request:
    arena.reset()
    header = arena.alloc(parse_header(request))
    body = arena.alloc(parse_body(request))
    response = process(header, body)
    send(response)
// La arena se reutiliza sin syscalls adicionales
```

Esto elimina completamente la presion sobre `malloc`/`free` en el hot path.

---

## 6. Arena allocator en sistemas reales

### 6.1 Tabla de uso

| Sistema | Arena usada | Proposito |
|---------|-------------|-----------|
| **Linux kernel** | `kmem_cache` + per-CPU pages | Allocacion rapida de objetos frecuentes |
| **Apache** | `apr_pool_t` | Memoria por peticion HTTP |
| **Nginx** | `ngx_pool_t` | Memoria por conexion/peticion |
| **LLVM** | `BumpPtrAllocator` | IR y datos intermedios de compilacion |
| **Zig** | `std.heap.ArenaAllocator` | Allocator composable de primer nivel |
| **Rust** | `bumpalo`, `typed-arena` | Crates de arena allocator |
| **Go** | Allocacion por goroutine stack | Stacks que crecen como arenas |

### 6.2 Rendimiento vs malloc

Benchmarks tipicos muestran que un arena allocator es **5-50x mas rapido**
que `malloc` para workloads de muchas allocaciones pequenas, porque:

1. **Sin busqueda**: no se recorre free list.
2. **Sin metadata**: no se escribe header por bloque.
3. **Sin coalescing**: no se fusionan bloques.
4. **Cache friendly**: las allocaciones son contiguas en memoria →
   excelente localidad espacial.

---

## 7. Relacion con la jerarquia de allocators

```
Nivel 3: Arena / Pool / Slab (per-workload)
    ↓ pide bloques grandes a
Nivel 2: malloc / free (general purpose: ptmalloc2, jemalloc)
    ↓ pide paginas a
Nivel 1: mmap / brk (kernel)
    ↓ gestiona
Nivel 0: Buddy system (paginas fisicas)
```

La arena no reemplaza a malloc — se **construye encima** de el. La arena
pide un chunk grande a malloc (o mmap) y luego distribuye memoria dentro
del chunk sin overhead.

---

## 8. Programa completo en C

```c
/*
 * arena_allocator.c
 *
 * Arena (bump) allocator: allocate linearly, free all at
 * once. Fixed-size and growable variants with alignment.
 *
 * Compile: gcc -O2 -o arena_allocator arena_allocator.c
 * Run:     ./arena_allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* =================== FIXED ARENA ================================== */

#define ARENA_DEFAULT_ALIGN 8

typedef struct {
    char *buf;
    size_t capacity;
    size_t offset;
    size_t peak;           /* high-water mark */
    size_t alloc_count;
} Arena;

Arena arena_create(size_t capacity) {
    Arena a;
    a.buf = (char *)malloc(capacity);
    a.capacity = capacity;
    a.offset = 0;
    a.peak = 0;
    a.alloc_count = 0;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_aligned(a, size, ARENA_DEFAULT_ALIGN);
}

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    size_t aligned_offset = (a->offset + align - 1) & ~(align - 1);
    if (aligned_offset + size > a->capacity)
        return NULL;

    void *ptr = a->buf + aligned_offset;
    a->offset = aligned_offset + size;
    a->alloc_count++;
    if (a->offset > a->peak)
        a->peak = a->offset;
    return ptr;
}

void arena_reset(Arena *a) {
    a->offset = 0;
    a->alloc_count = 0;
}

void arena_destroy(Arena *a) {
    free(a->buf);
    a->buf = NULL;
    a->capacity = 0;
    a->offset = 0;
}

void arena_stats(Arena *a, const char *label) {
    printf("  [%s] capacity=%zu, used=%zu (%.1f%%), peak=%zu, allocs=%zu\n",
           label, a->capacity, a->offset,
           100.0 * (double)a->offset / (double)a->capacity,
           a->peak, a->alloc_count);
}

/* =================== GROWABLE ARENA ================================ */

typedef struct Chunk {
    char *buf;
    size_t capacity;
    size_t offset;
    struct Chunk *next;
} Chunk;

typedef struct {
    Chunk *head;           /* first chunk */
    Chunk *current;        /* chunk currently being filled */
    size_t initial_cap;
    size_t total_allocs;
    size_t total_bytes;
    size_t chunk_count;
} GrowArena;

static Chunk *chunk_create(size_t capacity) {
    Chunk *c = (Chunk *)malloc(sizeof(Chunk));
    c->buf = (char *)malloc(capacity);
    c->capacity = capacity;
    c->offset = 0;
    c->next = NULL;
    return c;
}

GrowArena garena_create(size_t initial_cap) {
    GrowArena ga;
    ga.head = chunk_create(initial_cap);
    ga.current = ga.head;
    ga.initial_cap = initial_cap;
    ga.total_allocs = 0;
    ga.total_bytes = 0;
    ga.chunk_count = 1;
    return ga;
}

void *garena_alloc(GrowArena *ga, size_t size) {
    size_t align = ARENA_DEFAULT_ALIGN;
    size_t aligned = (ga->current->offset + align - 1) & ~(align - 1);

    if (aligned + size > ga->current->capacity) {
        /* Need a new chunk — at least double the previous, or fit size */
        size_t new_cap = ga->current->capacity * 2;
        if (new_cap < size + align)
            new_cap = size + align;

        Chunk *nc = chunk_create(new_cap);
        ga->current->next = nc;
        ga->current = nc;
        ga->chunk_count++;
        aligned = 0;
    }

    void *ptr = ga->current->buf + aligned;
    ga->current->offset = aligned + size;
    ga->total_allocs++;
    ga->total_bytes += size;
    return ptr;
}

void garena_reset(GrowArena *ga) {
    /* Reset all chunks but keep them allocated for reuse */
    Chunk *c = ga->head;
    while (c) {
        c->offset = 0;
        c = c->next;
    }
    ga->current = ga->head;
    ga->total_allocs = 0;
    ga->total_bytes = 0;
}

void garena_destroy(GrowArena *ga) {
    Chunk *c = ga->head;
    while (c) {
        Chunk *next = c->next;
        free(c->buf);
        free(c);
        c = next;
    }
    ga->head = NULL;
    ga->current = NULL;
}

void garena_stats(GrowArena *ga, const char *label) {
    size_t total_cap = 0;
    size_t total_used = 0;
    Chunk *c = ga->head;
    while (c) {
        total_cap += c->capacity;
        total_used += c->offset;
        c = c->next;
    }
    printf("  [%s] chunks=%zu, total_cap=%zu, used=%zu, allocs=%zu\n",
           label, ga->chunk_count, total_cap, total_used, ga->total_allocs);
}

/* =================== DEMOS ========================================= */

void demo1_basic_arena(void) {
    printf("=== Demo 1: Basic Arena Allocator ===\n\n");

    Arena a = arena_create(512);
    arena_stats(&a, "initial");

    int *nums = (int *)arena_alloc(&a, 10 * sizeof(int));
    for (int i = 0; i < 10; i++) nums[i] = i * i;
    printf("  Allocated 10 ints: [%d, %d, %d, ..., %d]\n",
           nums[0], nums[1], nums[2], nums[9]);
    arena_stats(&a, "after 10 ints");

    char *name = (char *)arena_alloc(&a, 32);
    strcpy(name, "Arena Allocator");
    printf("  Allocated string: \"%s\"\n", name);
    arena_stats(&a, "after string");

    double *vals = (double *)arena_alloc(&a, 5 * sizeof(double));
    for (int i = 0; i < 5; i++) vals[i] = (double)i * 1.1;
    printf("  Allocated 5 doubles: [%.1f, %.1f, ..., %.1f]\n",
           vals[0], vals[1], vals[4]);
    arena_stats(&a, "after doubles");

    printf("\n  All allocations are contiguous in memory:\n");
    printf("    nums:  %p\n", (void *)nums);
    printf("    name:  %p (gap: %td bytes)\n",
           (void *)name, (char *)name - (char *)nums);
    printf("    vals:  %p (gap: %td bytes)\n",
           (void *)vals, (char *)vals - (char *)name);

    arena_reset(&a);
    arena_stats(&a, "after reset — all freed in O(1)");

    arena_destroy(&a);
    printf("\n");
}

void demo2_alignment(void) {
    printf("=== Demo 2: Alignment ===\n\n");

    Arena a = arena_create(256);

    printf("  %-12s  %-6s  %-18s  %-8s\n",
           "Type", "Size", "Address", "Aligned?");

    char *c1 = (char *)arena_alloc_aligned(&a, 1, 1);
    printf("  %-12s  %-6d  %-18p  %s\n",
           "char", 1, (void *)c1,
           ((uintptr_t)c1 % 1 == 0) ? "yes (1)" : "no");

    int *i1 = (int *)arena_alloc_aligned(&a, sizeof(int), 4);
    printf("  %-12s  %-6d  %-18p  %s\n",
           "int", (int)sizeof(int), (void *)i1,
           ((uintptr_t)i1 % 4 == 0) ? "yes (4)" : "no");

    double *d1 = (double *)arena_alloc_aligned(&a, sizeof(double), 8);
    printf("  %-12s  %-6d  %-18p  %s\n",
           "double", (int)sizeof(double), (void *)d1,
           ((uintptr_t)d1 % 8 == 0) ? "yes (8)" : "no");

    /* 16-byte aligned (SSE) */
    void *sse = arena_alloc_aligned(&a, 16, 16);
    printf("  %-12s  %-6d  %-18p  %s\n",
           "SSE vec", 16, sse,
           ((uintptr_t)sse % 16 == 0) ? "yes (16)" : "no");

    /* 32-byte aligned (AVX) */
    void *avx = arena_alloc_aligned(&a, 32, 32);
    printf("  %-12s  %-6d  %-18p  %s\n",
           "AVX vec", 32, avx,
           ((uintptr_t)avx % 32 == 0) ? "yes (32)" : "no");

    printf("\n  Padding bytes wasted for alignment:\n");
    printf("    Total allocated payload: %zu\n",
           1 + sizeof(int) + sizeof(double) + 16 + 32);
    printf("    Arena offset (includes padding): %zu\n", a.offset);
    printf("    Padding overhead: %zu bytes\n\n",
           a.offset - (1 + sizeof(int) + sizeof(double) + 16 + 32));

    arena_destroy(&a);
}

void demo3_growable_arena(void) {
    printf("=== Demo 3: Growable Arena ===\n\n");

    GrowArena ga = garena_create(64);
    garena_stats(&ga, "initial (64 bytes)");

    printf("\n  Allocating increasingly large blocks:\n");
    for (int i = 1; i <= 8; i++) {
        size_t sz = (size_t)i * 30;
        void *p = garena_alloc(&ga, sz);
        printf("    alloc(%3zu) -> %p", sz, p);
        garena_stats(&ga, "");
    }

    printf("\n  Arena grew by adding larger chunks.\n");
    garena_stats(&ga, "final");

    garena_reset(&ga);
    garena_stats(&ga, "after reset (chunks retained)");

    garena_destroy(&ga);
    printf("\n");
}

void demo4_parsing_simulation(void) {
    printf("=== Demo 4: Parsing Simulation ===\n\n");

    Arena a = arena_create(4096);

    /* Simulate parsing a simple expression: (+ 1 (* 2 3)) */
    typedef struct Node {
        enum { NUM, ADD, MUL } type;
        union {
            int value;
            struct { struct Node *left, *right; } children;
        } data;
    } Node;

    printf("  Parsing expression: (+ 1 (* 2 3))\n\n");

    Node *n1 = (Node *)arena_alloc(&a, sizeof(Node));
    n1->type = NUM;
    n1->data.value = 1;

    Node *n2 = (Node *)arena_alloc(&a, sizeof(Node));
    n2->type = NUM;
    n2->data.value = 2;

    Node *n3 = (Node *)arena_alloc(&a, sizeof(Node));
    n3->type = NUM;
    n3->data.value = 3;

    Node *mul = (Node *)arena_alloc(&a, sizeof(Node));
    mul->type = MUL;
    mul->data.children.left = n2;
    mul->data.children.right = n3;

    Node *add = (Node *)arena_alloc(&a, sizeof(Node));
    add->type = ADD;
    add->data.children.left = n1;
    add->data.children.right = mul;

    printf("  AST built with %zu allocations, %zu bytes\n",
           a.alloc_count, a.offset);
    printf("  sizeof(Node) = %zu\n", sizeof(Node));
    printf("  With malloc: %zu bytes overhead (headers)\n",
           a.alloc_count * 16);
    printf("  With arena:  0 bytes overhead\n\n");

    /* "Evaluate" */
    int result = n2->data.value * n3->data.value + n1->data.value;
    printf("  Result: 1 + (2 * 3) = %d\n", result);

    arena_reset(&a);
    printf("  All AST nodes freed in O(1).\n\n");

    arena_destroy(&a);
}

void demo5_scratch_loop(void) {
    printf("=== Demo 5: Scratch Space in a Loop ===\n\n");

    Arena a = arena_create(1024);

    printf("  Simulating 5 iterations of a server loop.\n");
    printf("  Each iteration allocates temporary buffers.\n\n");

    for (int iter = 0; iter < 5; iter++) {
        arena_reset(&a);

        /* Simulate per-request allocations */
        char *header = (char *)arena_alloc(&a, 128);
        snprintf(header, 128, "GET /page/%d HTTP/1.1", iter);

        int *params = (int *)arena_alloc(&a, 10 * sizeof(int));
        for (int j = 0; j < 10; j++) params[j] = iter * 10 + j;

        char *response = (char *)arena_alloc(&a, 256);
        snprintf(response, 256, "200 OK: processed request %d", iter);

        printf("    Iter %d: used=%3zu/%zu | header=\"%.30s...\" | "
               "response=\"%.25s...\"\n",
               iter, a.offset, a.capacity, header, response);
    }

    printf("\n  No malloc/free calls in the loop — only bump + reset.\n");
    printf("  Peak usage: %zu bytes\n\n", a.peak);

    arena_destroy(&a);
}

void demo6_benchmark(void) {
    printf("=== Demo 6: Arena vs malloc Benchmark ===\n\n");

    const int N = 100000;
    const size_t SZ = 64;

    /* malloc/free */
    clock_t start = clock();
    for (int round = 0; round < 10; round++) {
        void *ptrs[100000 / 10]; /* batch to avoid huge array */
        int batch = N / 10;
        for (int i = 0; i < batch; i++)
            ptrs[i] = malloc(SZ);
        for (int i = 0; i < batch; i++)
            free(ptrs[i]);
    }
    clock_t end = clock();
    double malloc_time = (double)(end - start) / CLOCKS_PER_SEC;

    /* Arena */
    Arena a = arena_create((size_t)N * (SZ + 8)); /* generous */
    start = clock();
    for (int round = 0; round < 10; round++) {
        arena_reset(&a);
        for (int i = 0; i < N; i++)
            arena_alloc(&a, SZ);
    }
    end = clock();
    double arena_time = (double)(end - start) / CLOCKS_PER_SEC;

    printf("  %d allocations of %zu bytes, repeated 10 times:\n\n", N, SZ);
    printf("    malloc/free: %.4f s\n", malloc_time);
    printf("    arena:       %.4f s\n", arena_time);
    if (arena_time > 0)
        printf("    speedup:     %.1fx\n", malloc_time / arena_time);
    printf("\n  Arena is faster: no free-list search, no headers,\n");
    printf("  no coalescing, better cache locality.\n\n");

    arena_destroy(&a);
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_basic_arena();
    demo2_alignment();
    demo3_growable_arena();
    demo4_parsing_simulation();
    demo5_scratch_loop();
    demo6_benchmark();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * arena_allocator.rs
 *
 * Arena (bump) allocator in Rust: fixed-size and growable
 * variants with alignment, scratch loops, and benchmarking.
 *
 * Compile: rustc -O -o arena_allocator arena_allocator.rs
 * Run:     ./arena_allocator
 */

use std::cell::Cell;
use std::time::Instant;

/* =================== FIXED ARENA =================================== */

struct Arena {
    buf: Vec<u8>,
    offset: Cell<usize>,
    peak: Cell<usize>,
    alloc_count: Cell<usize>,
}

impl Arena {
    fn new(capacity: usize) -> Self {
        Arena {
            buf: vec![0u8; capacity],
            offset: Cell::new(0),
            peak: Cell::new(0),
            alloc_count: Cell::new(0),
        }
    }

    fn capacity(&self) -> usize { self.buf.len() }
    fn used(&self) -> usize { self.offset.get() }
    fn remaining(&self) -> usize { self.capacity() - self.used() }
    fn peak(&self) -> usize { self.peak.get() }
    fn count(&self) -> usize { self.alloc_count.get() }

    fn alloc(&self, size: usize, align: usize) -> Option<*mut u8> {
        let offset = self.offset.get();
        let aligned = (offset + align - 1) & !(align - 1);
        let new_offset = aligned + size;

        if new_offset > self.capacity() {
            return None;
        }

        self.offset.set(new_offset);
        self.alloc_count.set(self.count() + 1);
        if new_offset > self.peak.get() {
            self.peak.set(new_offset);
        }

        Some(unsafe { (self.buf.as_ptr() as *mut u8).add(aligned) })
    }

    fn alloc_default(&self, size: usize) -> Option<*mut u8> {
        self.alloc(size, 8)
    }

    fn alloc_typed<T>(&self) -> Option<*mut T> {
        let ptr = self.alloc(std::mem::size_of::<T>(),
                             std::mem::align_of::<T>())?;
        Some(ptr as *mut T)
    }

    fn alloc_slice<T>(&self, count: usize) -> Option<*mut T> {
        let ptr = self.alloc(std::mem::size_of::<T>() * count,
                             std::mem::align_of::<T>())?;
        Some(ptr as *mut T)
    }

    fn reset(&self) {
        self.offset.set(0);
        self.alloc_count.set(0);
    }

    fn stats(&self, label: &str) {
        println!("  [{}] capacity={}, used={} ({:.1}%), peak={}, allocs={}",
                 label, self.capacity(), self.used(),
                 100.0 * self.used() as f64 / self.capacity() as f64,
                 self.peak(), self.count());
    }
}

/* =================== GROWABLE ARENA ================================ */

struct GrowArena {
    chunks: Vec<Vec<u8>>,
    current: usize,
    offset: usize,
    total_allocs: usize,
    total_bytes: usize,
    initial_cap: usize,
}

impl GrowArena {
    fn new(initial_cap: usize) -> Self {
        GrowArena {
            chunks: vec![vec![0u8; initial_cap]],
            current: 0,
            offset: 0,
            total_allocs: 0,
            total_bytes: 0,
            initial_cap,
        }
    }

    fn alloc(&mut self, size: usize) -> *mut u8 {
        let align = 8;
        let aligned = (self.offset + align - 1) & !(align - 1);

        if aligned + size > self.chunks[self.current].len() {
            /* Need new chunk */
            let new_cap = std::cmp::max(
                self.chunks[self.current].len() * 2,
                size + align,
            );
            self.chunks.push(vec![0u8; new_cap]);
            self.current = self.chunks.len() - 1;
            self.offset = 0;
            return self.alloc(size); /* retry in new chunk */
        }

        let ptr = unsafe {
            self.chunks[self.current].as_mut_ptr().add(aligned)
        };
        self.offset = aligned + size;
        self.total_allocs += 1;
        self.total_bytes += size;
        ptr
    }

    fn reset(&mut self) {
        /* Reset all chunks, keep allocated */
        self.current = 0;
        self.offset = 0;
        self.total_allocs = 0;
        self.total_bytes = 0;
    }

    fn stats(&self, label: &str) {
        let total_cap: usize = self.chunks.iter().map(|c| c.len()).sum();
        println!("  [{}] chunks={}, total_cap={}, allocs={}, bytes={}",
                 label, self.chunks.len(), total_cap,
                 self.total_allocs, self.total_bytes);
    }
}

/* =================== DEMOS ========================================= */

fn demo1_basic_arena() {
    println!("=== Demo 1: Basic Arena Allocator ===\n");

    let arena = Arena::new(512);
    arena.stats("initial");

    /* Allocate integers */
    let nums = arena.alloc_slice::<i32>(10).unwrap();
    for i in 0..10 {
        unsafe { *nums.add(i) = (i * i) as i32; }
    }
    unsafe {
        println!("  Allocated 10 ints: [{}, {}, {}, ..., {}]",
                 *nums, *nums.add(1), *nums.add(2), *nums.add(9));
    }
    arena.stats("after 10 ints");

    /* Allocate a string buffer */
    let name = arena.alloc_default(32).unwrap();
    let msg = b"Arena Allocator\0";
    unsafe {
        std::ptr::copy_nonoverlapping(msg.as_ptr(), name, msg.len());
        let s = std::ffi::CStr::from_ptr(name as *const i8);
        println!("  Allocated string: {:?}", s);
    }
    arena.stats("after string");

    /* Allocate doubles */
    let vals = arena.alloc_slice::<f64>(5).unwrap();
    for i in 0..5 {
        unsafe { *vals.add(i) = i as f64 * 1.1; }
    }
    unsafe {
        println!("  Allocated 5 f64s: [{:.1}, {:.1}, ..., {:.1}]",
                 *vals, *vals.add(1), *vals.add(4));
    }
    arena.stats("after doubles");

    println!("\n  Contiguous addresses:");
    println!("    nums: {:p}", nums);
    println!("    name: {:p} (gap: {} bytes)",
             name, name as usize - nums as usize);
    println!("    vals: {:p} (gap: {} bytes)",
             vals, vals as *const f64 as usize - name as usize);

    arena.reset();
    arena.stats("after reset");
    println!();
}

fn demo2_alignment() {
    println!("=== Demo 2: Alignment ===\n");

    let arena = Arena::new(512);

    println!("  {:>12}  {:>5}  {:>5}  {:>18}  {:>8}",
             "Type", "Size", "Align", "Address", "OK?");

    let c = arena.alloc(1, 1).unwrap();
    println!("  {:>12}  {:>5}  {:>5}  {:>18p}  {:>8}",
             "u8", 1, 1, c, if c as usize % 1 == 0 { "yes" } else { "no" });

    let i = arena.alloc(4, 4).unwrap();
    println!("  {:>12}  {:>5}  {:>5}  {:>18p}  {:>8}",
             "i32", 4, 4, i, if i as usize % 4 == 0 { "yes" } else { "no" });

    let d = arena.alloc(8, 8).unwrap();
    println!("  {:>12}  {:>5}  {:>5}  {:>18p}  {:>8}",
             "f64", 8, 8, d, if d as usize % 8 == 0 { "yes" } else { "no" });

    let sse = arena.alloc(16, 16).unwrap();
    println!("  {:>12}  {:>5}  {:>5}  {:>18p}  {:>8}",
             "SSE", 16, 16, sse,
             if sse as usize % 16 == 0 { "yes" } else { "no" });

    let avx = arena.alloc(32, 32).unwrap();
    println!("  {:>12}  {:>5}  {:>5}  {:>18p}  {:>8}",
             "AVX", 32, 32, avx,
             if avx as usize % 32 == 0 { "yes" } else { "no" });

    let payload = 1 + 4 + 8 + 16 + 32;
    println!("\n  Payload: {} bytes, Arena used: {} bytes, Padding: {}",
             payload, arena.used(), arena.used() - payload);
    println!();
}

fn demo3_growable_arena() {
    println!("=== Demo 3: Growable Arena ===\n");

    let mut ga = GrowArena::new(64);
    ga.stats("initial");

    println!();
    for i in 1..=8 {
        let sz = i * 30;
        let p = ga.alloc(sz);
        println!("    alloc({:>3}) -> {:p}", sz, p);
        ga.stats("     ");
    }

    ga.stats("\n  final");
    ga.reset();
    ga.stats("after reset");
    println!();
}

fn demo4_ast_parsing() {
    println!("=== Demo 4: AST Parsing Simulation ===\n");

    #[repr(C)]
    #[derive(Clone, Copy)]
    enum NodeKind { Num, Add, Mul }

    #[repr(C)]
    struct Node {
        kind: NodeKind,
        value: i32,
        left: *mut Node,
        right: *mut Node,
    }

    let arena = Arena::new(4096);

    println!("  Parsing: (+ 1 (* 2 3))\n");

    unsafe {
        let n1 = arena.alloc_typed::<Node>().unwrap();
        (*n1) = Node { kind: NodeKind::Num, value: 1,
                        left: std::ptr::null_mut(), right: std::ptr::null_mut() };

        let n2 = arena.alloc_typed::<Node>().unwrap();
        (*n2) = Node { kind: NodeKind::Num, value: 2,
                        left: std::ptr::null_mut(), right: std::ptr::null_mut() };

        let n3 = arena.alloc_typed::<Node>().unwrap();
        (*n3) = Node { kind: NodeKind::Num, value: 3,
                        left: std::ptr::null_mut(), right: std::ptr::null_mut() };

        let mul = arena.alloc_typed::<Node>().unwrap();
        (*mul) = Node { kind: NodeKind::Mul, value: 0, left: n2, right: n3 };

        let add = arena.alloc_typed::<Node>().unwrap();
        (*add) = Node { kind: NodeKind::Add, value: 0, left: n1, right: mul };

        let result = (*n1).value + (*n2).value * (*n3).value;
        println!("  Result: 1 + (2 * 3) = {}", result);
    }

    println!("  Nodes allocated: {}, bytes used: {}",
             arena.count(), arena.used());
    println!("  sizeof(Node) = {}", std::mem::size_of::<Node>());
    println!("  With Box: {} bytes overhead (alloc headers)",
             arena.count() * 16);
    println!("  With arena: 0 bytes overhead");

    arena.reset();
    println!("  After reset: all AST nodes freed in O(1).\n");
}

fn demo5_scratch_loop() {
    println!("=== Demo 5: Scratch Space in a Loop ===\n");

    let arena = Arena::new(2048);

    println!("  5 iterations of a simulated server loop:\n");

    for iter in 0..5 {
        arena.reset();

        /* Simulate per-request work */
        let header = arena.alloc_default(128).unwrap();
        let msg = format!("GET /page/{} HTTP/1.1\0", iter);
        unsafe {
            std::ptr::copy_nonoverlapping(
                msg.as_ptr(), header, msg.len().min(128));
        }

        let _params = arena.alloc_slice::<i32>(10).unwrap();
        let _response = arena.alloc_default(256).unwrap();

        println!("    Iter {}: used={:>4}/{} | allocs={}",
                 iter, arena.used(), arena.capacity(), arena.count());
    }

    println!("\n  No heap allocs in the loop. Peak: {} bytes.\n",
             arena.peak());
}

fn demo6_benchmark_vs_vec() {
    println!("=== Demo 6: Arena vs Vec<Box> Benchmark ===\n");

    let n = 100_000usize;
    let sz = 64usize;

    /* Vec<Box<[u8]>> (simulates malloc/free) */
    let start = Instant::now();
    for _ in 0..10 {
        let mut v: Vec<Box<[u8]>> = Vec::with_capacity(n);
        for _ in 0..n {
            v.push(vec![0u8; sz].into_boxed_slice());
        }
        drop(v);
    }
    let box_time = start.elapsed();

    /* Arena */
    let arena = Arena::new(n * (sz + 8));
    let start = Instant::now();
    for _ in 0..10 {
        arena.reset();
        for _ in 0..n {
            arena.alloc_default(sz);
        }
    }
    let arena_time = start.elapsed();

    println!("  {} allocations of {} bytes, 10 rounds:\n", n, sz);
    println!("    Vec<Box>: {:.4} s", box_time.as_secs_f64());
    println!("    Arena:    {:.4} s", arena_time.as_secs_f64());
    if arena_time.as_nanos() > 0 {
        println!("    Speedup:  {:.1}x",
                 box_time.as_secs_f64() / arena_time.as_secs_f64());
    }
    println!();
}

fn demo7_typed_arena_pattern() {
    println!("=== Demo 7: Typed Arena Pattern ===\n");

    /* A simple typed arena that stores elements of one type */
    struct TypedArena<T> {
        storage: Vec<T>,
    }

    impl<T> TypedArena<T> {
        fn new() -> Self { TypedArena { storage: Vec::new() } }

        fn alloc(&mut self, value: T) -> &T {
            self.storage.push(value);
            self.storage.last().unwrap()
        }

        fn len(&self) -> usize { self.storage.len() }
    }

    #[derive(Debug)]
    struct Token {
        kind: &'static str,
        value: String,
        line: u32,
    }

    let mut arena: TypedArena<Token> = TypedArena::new();

    let tokens = [
        ("keyword", "fn", 1),
        ("ident", "main", 1),
        ("lparen", "(", 1),
        ("rparen", ")", 1),
        ("lbrace", "{", 1),
        ("ident", "println", 2),
        ("string", "\"hello\"", 2),
        ("rbrace", "}", 3),
    ];

    for &(kind, value, line) in &tokens {
        arena.alloc(Token {
            kind,
            value: value.to_string(),
            line,
        });
    }

    println!("  Parsed {} tokens into typed arena:", arena.len());
    for tok in &arena.storage {
        println!("    line {}: {:>8} = {}", tok.line, tok.kind, tok.value);
    }

    println!("\n  All tokens freed when arena is dropped.\n");
    /* arena drops here, all Tokens get Drop called */
}

fn demo8_nested_arenas() {
    println!("=== Demo 8: Nested Arena Lifetimes ===\n");

    let permanent = Arena::new(1024);
    let per_request = Arena::new(512);
    let scratch = Arena::new(256);

    /* Permanent config */
    let _config = permanent.alloc_default(64);
    println!("  Permanent arena: {} bytes (config, lives forever)",
             permanent.used());

    for req in 0..3 {
        per_request.reset();

        let _header = per_request.alloc_default(48);
        let _body = per_request.alloc_default(96);

        /* Scratch for intermediate computation */
        for _step in 0..4 {
            scratch.reset();
            let _tmp = scratch.alloc_default(32);
            let _tmp2 = scratch.alloc_default(16);
            /* scratch reset at next iteration */
        }

        println!("  Request {}: per_request={} bytes, scratch peak={} bytes",
                 req, per_request.used(), scratch.peak());
    }

    println!("\n  Three lifetime tiers:");
    println!("    permanent:   {} bytes (never reset)", permanent.used());
    println!("    per_request: reset each request");
    println!("    scratch:     reset each computation step\n");
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_basic_arena();
    demo2_alignment();
    demo3_growable_arena();
    demo4_ast_parsing();
    demo5_scratch_loop();
    demo6_benchmark_vs_vec();
    demo7_typed_arena_pattern();
    demo8_nested_arenas();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Operacion basica

Una arena de 256 bytes con alineamiento a 8. Se ejecutan:
`alloc(10)`, `alloc(3)`, `alloc(20)`, `alloc(1)`.

Cual es el offset despues de cada allocacion? Cuantos bytes de padding
se desperdiciaron?

<details><summary>Prediccion</summary>

```
alloc(10):
  aligned_offset = (0 + 7) & ~7 = 0
  new_offset = 0 + 10 = 10
  return @0

alloc(3):
  aligned_offset = (10 + 7) & ~7 = 16
  new_offset = 16 + 3 = 19
  return @16

alloc(20):
  aligned_offset = (19 + 7) & ~7 = 24
  new_offset = 24 + 20 = 44
  return @24

alloc(1):
  aligned_offset = (44 + 7) & ~7 = 48
  new_offset = 48 + 1 = 49
  return @48
```

Offset final: **49**.
Payload total: $10 + 3 + 20 + 1 = 34$ bytes.
Espacio consumido: 49 bytes.
Padding: $49 - 34 = 15$ bytes desperdiciados en alineamiento.

Sin alineamiento, se usarian solo 34 bytes. El alineamiento a 8 cuesta
un 44% extra en este caso — pero es necesario para acceso correcto a
tipos de mas de 1 byte.

</details>

### Ejercicio 2 — Capacidad de la arena

Una arena de 1 KB (1024 bytes) con alineamiento a 8. Se alocan objetos
de 24 bytes repetidamente. Cuantos objetos caben?

<details><summary>Prediccion</summary>

Cada alloc(24): 24 ya esta alineado a 8, asi que no hay padding.

Objetos: $\lfloor 1024 / 24 \rfloor = 42$ objetos (42 × 24 = 1008).
Sobran 16 bytes — no alcanzan para otro objeto de 24.

**42 objetos** caben.

Si el tamano fuera 25: aligned = $\lceil 25/8 \rceil \times 8 = 32$.
Objetos: $\lfloor 1024 / 32 \rfloor = 32$. La diferencia entre 24 y 25
reduce la capacidad de 42 a 32 — un 24% menos por 1 byte extra.

Leccion: elegir tamanos de estructura que sean multiplos del alineamiento
maximiza la capacidad de la arena.

</details>

### Ejercicio 3 — Growable arena

Una arena comienza con un chunk de 64 bytes y duplica en cada crecimiento.
Se alocan 200 bytes de una vez. Cuantos chunks se crean? Que tamanos?

<details><summary>Prediccion</summary>

1. Chunk 1: 64 bytes. `alloc(200)`: 200 > 64. Necesita nuevo chunk.
2. Chunk 2: $\max(64 \times 2, 200 + 8) = \max(128, 208) = 208$ bytes.
   La politica es "al menos el doble, o lo que se necesite". 200 cabe en
   208.

Resultado: **2 chunks** (64 + 208 = 272 bytes de capacidad total).
El chunk 1 tiene 64 bytes desperdiciados (nadie lo usa, pero se retiene
para future resets).

Si la politica fuera estrictamente "duplicar sin considerar tamano":
- Chunk 2: 128. 200 > 128. Nuevo chunk.
- Chunk 3: 256. 200 <= 256. OK.

Resultado con politica estricta: **3 chunks** (64 + 128 + 256 = 448 bytes).
Los chunks 1 y 2 estan completamente desperdiciados.

Por eso la politica correcta es `max(doble, requerido)`.

</details>

### Ejercicio 4 — Arena vs malloc overhead

Un parser crea 50,000 nodos de AST de 32 bytes cada uno. Compara el
overhead total con malloc (header de 16 bytes) vs arena.

<details><summary>Prediccion</summary>

**Con malloc**:
- Payload: $50000 \times 32 = 1{,}600{,}000$ bytes.
- Headers: $50000 \times 16 = 800{,}000$ bytes.
- Total: $2{,}400{,}000$ bytes (2.34 MB).
- Overhead: 33% del total es metadata.

**Con arena** (alineamiento a 8, 32 ya alineado):
- Payload: $50000 \times 32 = 1{,}600{,}000$ bytes.
- Overhead: 0 bytes de metadata. Solo la estructura de la arena (~24 bytes).
- Total: ~$1{,}600{,}000$ bytes (1.53 MB).

**Ahorro**: 800 KB (33% menos memoria).

Ademas, con arena:
- 50,000 allocs toman ~50,000 operaciones de `bump` ($O(1)$ cada una).
- Con malloc: 50,000 busquedas en free list + 50,000 writes de header.
- Al finalizar: arena reset en $O(1)$. Con malloc: 50,000 llamadas a `free`.

</details>

### Ejercicio 5 — Scratch pattern

Un servidor procesa peticiones que requieren ~2 KB de memoria temporal.
Procesa 1 millon de peticiones. Compara:

1. malloc/free en cada peticion.
2. Arena de 4 KB con reset por peticion.

<details><summary>Prediccion</summary>

**1. malloc/free por peticion**:
- $10^6$ peticiones $\times$ ~10 malloc + 10 free = $20 \times 10^6$
  operaciones de allocator.
- Cada malloc: busqueda en free list + header. Cada free: coalescing.
- Presion constante sobre la free list. Posible fragmentacion progresiva.
- RSS puede crecer por fragmentacion a pesar de que el uso pico es 2 KB.

**2. Arena con reset**:
- $10^6$ peticiones $\times$ ~10 bumps + 1 reset = $11 \times 10^6$
  operaciones.
- Cada bump: $O(1)$ sin lock de free list.
- Reset: $O(1)$ — solo resetear puntero.
- RSS constante: la arena de 4 KB se reutiliza indefinidamente.
- **Cero fragmentacion**: la memoria se compacta automaticamente en cada
  reset.

**Ventaja de la arena**: ~2x menos operaciones, cada operacion mucho mas
rapida, RSS estable, sin fragmentacion. La arena es el patron estandar
para servidores (Apache usa `apr_pool_t`, Nginx usa `ngx_pool_t`).

</details>

### Ejercicio 6 — Limitacion: lifetimes diferentes

Explica por que una arena **no funciona** para un BST con insercion y
eliminacion intercaladas. Que allocator es mejor?

<details><summary>Prediccion</summary>

En un BST, los nodos tienen **lifetimes individuales**: se insertan y
eliminan en cualquier orden. Una arena no puede liberar nodos individuales.

Escenario:
```
insert(5), insert(3), insert(7), insert(2)  -> 4 nodos en arena
delete(3)  -> queremos liberar el nodo de 3
```

Con arena: no hay forma de liberar solo el nodo de 3. La memoria queda
desperdiciada hasta el reset. Si se insertan y eliminan muchos nodos
sin reset, la arena crece sin limite.

**Mejor opcion**: **pool allocator** (T02). Todos los nodos del BST tienen
el mismo tamano (`sizeof(Node)`). El pool pre-asigna slots de ese tamano
y mantiene una free list para reutilizacion.

- `pool_alloc()`: $O(1)$ — pop de free list.
- `pool_free(node)`: $O(1)$ — push a free list.
- Sin fragmentacion (todos los bloques son del mismo tamano).

La arena es para "batch lifetime" (todos mueren juntos). El pool es para
"individual lifetime" con tamano uniforme.

</details>

### Ejercicio 7 — Thread safety

Disena una arena thread-safe con `atomic fetch_add`. Cual es la condicion
de carrera si se usa un incremento no atomico? Hay algun byte desperdiciado
por la solucion atomica?

<details><summary>Prediccion</summary>

**Condicion de carrera sin atomics**:
```
Thread A: lee offset=100, calcula new_offset=164
Thread B: lee offset=100, calcula new_offset=132
Thread A: escribe offset=164, retorna @100
Thread B: escribe offset=132, retorna @100  // SOLAPAMIENTO!
```

Ambos threads obtienen el mismo puntero @100. Corrupcion de datos.

**Solucion con `fetch_add`**:
```
alloc_atomic(size):
    loop:
        offset = atomic_load(&bump)
        aligned = align_up(offset)
        new_offset = aligned + size
        if new_offset > capacity: return NULL
        if atomic_compare_and_swap(&bump, offset, new_offset):
            return buf + aligned
```

O mas simple con `fetch_add` (si aceptamos alineamiento fijo):
```
alloc_atomic(size):
    padded_size = align_up(size)
    offset = atomic_fetch_add(&bump, padded_size)
    if offset + padded_size > capacity: return NULL
    return buf + offset
```

**Bytes desperdiciados**: con `fetch_add`, el alineamiento se aplica al
**tamano**, no al **offset**. Si dos threads alocan 1 byte, cada uno
consume `align_up(1) = 8` bytes (7 de padding). Esto es mas desperdicio
que la version single-thread donde el segundo alloc podria empezar en
offset 1.

Pero: con `compare_and_swap` sobre el offset, se puede alinear por
offset exactamente como en single-thread, sin desperdicio extra.

</details>

### Ejercicio 8 — Destruccion y Drop

En Rust, los objetos en una arena no ejecutan `Drop` cuando se resetea.
Da un ejemplo donde esto causa un resource leak. Como lo resuelve
`typed-arena`?

<details><summary>Prediccion</summary>

**Ejemplo de leak**:
```rust
let arena = BumpArena::new();
let file = arena.alloc(File::open("data.txt").unwrap());
arena.reset(); // File no se cierra — fd leak!
```

El `File` tiene un `Drop` que llama `close(fd)`. Pero la arena solo
resetea su puntero — no sabe que tipos almacena ni si necesitan Drop.

**Como lo resuelve `typed-arena`**: el crate `typed-arena` almacena todos
los objetos del mismo tipo en un `Vec<T>`. Cuando la arena se destruye
(sale de scope), Rust llama `Drop` en el `Vec<T>`, que a su vez llama
`Drop` en cada elemento.

```rust
{
    let arena: TypedArena<File> = TypedArena::new();
    let f = arena.alloc(File::open("data.txt").unwrap());
    // f tiene borrow de arena
} // arena drops -> Vec<File> drops -> File::drop -> close(fd)
```

**Tradeoff**:
- `typed-arena`: garantiza Drop, pero es mas lento (usa Vec internamente,
  no bump allocation puro).
- `bumpalo`: no llama Drop, pero es mas rapido y flexible. El usuario
  debe asegurar que los objetos no necesitan Drop, o llamarlo manualmente.

</details>

### Ejercicio 9 — Arena con rewind

Disena una extension de la arena que soporte **rewind** (volver a un
punto anterior, liberando todo lo alocado despues). Que interfaz tendria?

<details><summary>Prediccion</summary>

**Interfaz**:
```c
typedef size_t ArenaMark;

ArenaMark arena_save(Arena *a);     // retorna offset actual
void arena_rewind(Arena *a, ArenaMark mark);  // resetea a mark
```

**Uso**:
```c
Arena a = arena_create(4096);
void *persistent = arena_alloc(&a, 100);

ArenaMark m = arena_save(&a);       // m = 104 (100 + padding)
void *temp1 = arena_alloc(&a, 200);
void *temp2 = arena_alloc(&a, 300);

arena_rewind(&a, m);               // libera temp1 y temp2
// persistent sigue valido (offset < m)

void *new_data = arena_alloc(&a, 50); // reutiliza espacio
```

**Implementacion**:
```c
ArenaMark arena_save(Arena *a) {
    return (ArenaMark)a->offset;
}

void arena_rewind(Arena *a, ArenaMark mark) {
    if (mark <= a->offset) {
        a->offset = mark;
    }
}
```

Esto permite un uso tipo **stack** de la arena: push (alloc) y pop
(rewind). Es util para calculos recursivos donde cada nivel de recursion
aloca datos temporales que se descartan al retornar.

**Restriccion**: los punteros a datos despues del mark se invalidan.
En C, el usuario debe tener cuidado de no usarlos. En Rust, se podria
modelar con lifetimes para hacer esto seguro en compilacion (los borrows
expiran al rewind).

</details>

### Ejercicio 10 — Dimension de la arena

Un sistema procesa archivos JSON de hasta 1 MB. Cada byte de JSON genera
aproximadamente 2 bytes de nodos de AST (empiricamente). El parsing usa
un arena allocator. Que tamano de arena recomendarias? Justifica.

<details><summary>Prediccion</summary>

**Calculo**:
- JSON maximo: 1 MB = $1{,}048{,}576$ bytes.
- Nodos: $\sim 2 \times 1{,}048{,}576 = 2{,}097{,}152$ bytes = 2 MB.
- Alineamiento a 8: puede anadir ~3.5 bytes promedio por allocacion. Si
  hay ~10,000 allocaciones (nodos), son ~35 KB de padding.

**Recomendacion**: **2.5 MB** de arena fija.

Justificacion:
1. $2 \text{ MB}$ (nodos) $+ 0.5 \text{ MB}$ (margen para padding y picos).
2. Es mejor sobredimensionar ligeramente que quedarse corto y necesitar
   chunks adicionales (un solo chunk es mas cache-friendly).
3. 2.5 MB es trivial para un servidor moderno.

**Alternativas**:
- Si los JSON varian mucho (1 KB a 1 MB), usar arena growable con chunk
  inicial de 64 KB y duplicar. Asi los JSONs pequenos no desperdician 2.5 MB.
- Si se procesan muchos JSONs en paralelo (10 threads), cada thread tiene
  su propia arena de 2.5 MB. Total: 25 MB — aun razonable.
- Medir el uso real con `arena.peak` en produccion y ajustar.

**Regla general**: arena_size $\approx 2 \times \text{input\_max} + 20\%$
margen. Medir y ajustar.

</details>
