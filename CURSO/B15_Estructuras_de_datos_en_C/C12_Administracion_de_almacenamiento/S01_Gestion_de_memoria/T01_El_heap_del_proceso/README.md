# T01 — El heap del proceso

## Objetivo

Comprender cómo un proceso gestiona memoria dinámica a nivel de sistema
operativo: el **layout de memoria** del proceso, las syscalls **brk/sbrk** y
**mmap**, la estructura interna de **malloc** (free list, metadata por bloque,
splitting, coalescing), y cómo todo esto se traduce en las llamadas
`malloc`/`free` que usamos diariamente.

---

## 1. Layout de memoria de un proceso

### 1.1 Segmentos

Un proceso en Linux (y la mayoría de sistemas Unix) tiene el siguiente layout
en su espacio de direcciones virtuales (de dirección baja a alta):

```
+---------------------------+  0x000000000000 (bajo)
|         Text (code)       |  Instrucciones del programa (read-only, execute)
+---------------------------+
|       Data (initialized)  |  Variables globales/estáticas inicializadas
+---------------------------+
|         BSS               |  Variables globales/estáticas sin inicializar (zero-filled)
+---------------------------+
|          Heap              |  Memoria dinámica (crece hacia arriba ↑)
|           ↓                |
|           ...              |
|           ↑                |
|          Stack             |  Variables locales, frames (crece hacia abajo ↓)
+---------------------------+  0x7FFFFFFFFFFF (alto, en x86-64)
```

### 1.2 Características de cada segmento

| Segmento | Contenido | Tamaño | Dirección |
|----------|-----------|--------|-----------|
| **Text** | Código compilado | Fijo | Baja |
| **Data** | `int x = 42;` | Fijo | Baja |
| **BSS** | `int y;` (global) | Fijo | Baja |
| **Heap** | `malloc`, `new` | Dinámico ↑ | Media |
| **Stack** | Variables locales | Dinámico ↓ | Alta |

El **heap** y el **stack** crecen en direcciones opuestas. El espacio entre
ellos es el espacio disponible para ambos.

### 1.3 Memory mapping (mmap)

Además de los segmentos clásicos, el kernel puede mapear regiones arbitrarias
del espacio de direcciones usando `mmap`. Estas regiones aparecen típicamente
entre el heap y el stack:

```
Heap (brk) ↑
...
mmap regions (bibliotecas compartidas, allocaciones grandes)
...
Stack ↓
```

---

## 2. brk y sbrk: gestión del heap

### 2.1 El program break

El **program break** es el límite superior del segmento de datos del proceso
(inmediatamente después del BSS). Mover este límite hacia arriba **expande el
heap**; moverlo hacia abajo lo **contrae**.

### 2.2 Syscalls

```c
#include <unistd.h>

void *sbrk(intptr_t increment);
// Mueve el break en 'increment' bytes.
// Retorna el break ANTERIOR (= inicio del nuevo bloque).
// sbrk(0) retorna el break actual sin modificarlo.

int brk(void *addr);
// Establece el break en la dirección exacta 'addr'.
// Retorna 0 en éxito, -1 en error.
```

### 2.3 Ejemplo conceptual

```
Antes:
  BSS end --------- break (0x5000)

sbrk(1024):
  BSS end --------- old break (0x5000)
                    [1024 bytes nuevos]
                    break (0x5400)

sbrk(0) retorna 0x5400 (el break actual)
```

### 2.4 Limitaciones de brk/sbrk

- Solo pueden mover el break **linealmente**: no pueden crear "huecos" en el
  medio del heap.
- Contraer el heap solo es posible si los bytes al final están libres.
- No son thread-safe por sí solos.
- Los allocators modernos usan brk/sbrk para allocaciones pequeñas y **mmap**
  para bloques grandes ($\geq 128$ KB en glibc por defecto).

---

## 3. mmap para allocaciones grandes

### 3.1 Mecanismo

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
```

Con `MAP_ANONYMOUS | MAP_PRIVATE`, mmap asigna memoria que no está respaldada
por ningún archivo — simplemente reserva páginas del kernel.

### 3.2 Cuándo se usa

| Mecanismo | Tamaño típico | Ventaja |
|-----------|:------------:|---------|
| brk/sbrk | < 128 KB | Rápido (solo mover puntero) |
| mmap | ≥ 128 KB | Se puede devolver al OS independientemente |

La ventaja de mmap para bloques grandes: al hacer `munmap`, la memoria se
devuelve inmediatamente al sistema operativo, sin importar el estado del rest
del heap. Con brk, solo se puede contraer si el bloque está al final.

### 3.3 Threshold configurable

En glibc, el umbral mmap se ajusta dinámicamente con `mallopt(M_MMAP_THRESHOLD, size)`. El valor por defecto es 128 KB pero se adapta automáticamente.

---

## 4. Estructura interna de malloc

### 4.1 Metadata por bloque

Cada bloque asignado por malloc tiene un **header** invisible para el usuario,
almacenado justo antes del puntero retornado:

```
        ← header →← payload (lo que ve el usuario) →
       +----------+-------------------------------+
       | size | F |          datos                  |
       +----------+-------------------------------+
       ^          ^
       |          |
  block start   puntero retornado por malloc
```

- **size**: tamaño del bloque (incluyendo o no el header, según implementación).
- **F**: flag indicando si el bloque está libre (free) o usado.
- Opcionalmente: puntero al siguiente bloque libre (para la free list).

### 4.2 Overhead

El header típicamente ocupa 8-16 bytes (una o dos `size_t`). Esto significa que
`malloc(1)` realmente consume 16-32 bytes de heap (header + alineamiento).

Además, malloc alinea los bloques a $2 \times \text{sizeof(size\_t)}$ (16 bytes
en 64-bit), lo que puede introducir **padding** adicional.

### 4.3 Free list

Los bloques libres se organizan en una **lista enlazada** llamada **free list**.
Cuando se llama `malloc(n)`:

1. Recorrer la free list buscando un bloque de tamaño $\geq n$ (la estrategia
   de búsqueda se detalla en T02).
2. Si se encuentra un bloque suficientemente grande:
   - **Splitting**: si el bloque es mucho mayor que $n$, dividirlo en dos:
     uno de tamaño $n$ (retornado al usuario) y uno con el sobrante (devuelto
     a la free list).
   - Marcarlo como usado y retornar el puntero.
3. Si no se encuentra: solicitar más memoria al OS (sbrk o mmap).

### 4.4 Operación de free

Cuando se llama `free(ptr)`:

1. Retroceder desde `ptr` para acceder al header.
2. Marcar el bloque como libre.
3. **Coalescing**: fusionar con bloques adyacentes libres para reducir
   fragmentación.
4. Insertar en la free list.

### 4.5 Splitting y coalescing ilustrados

```
Estado inicial (un bloque libre de 64 bytes):
  [H: size=64, free] [................64 bytes................]

malloc(16):
  Splitting: dividir en bloque de 16 + bloque de 40 (64 - 16 - 8 header)
  [H: size=16, USED] [16 bytes] [H: size=40, free] [......40 bytes......]

malloc(8):
  [H:16,USED][16B] [H:8,USED][8B] [H:24,free][.....24B.....]

free(primer bloque):
  [H:16,free][16B] [H:8,USED][8B] [H:24,free][.....24B.....]
  No se puede coalescer (el vecino derecho está usado).

free(segundo bloque):
  [H:16,free][16B] [H:8,free][8B] [H:24,free][.....24B.....]
  Coalescing: fusionar los 3 bloques:
  [H: size=64, free] [................64 bytes................]
```

---

## 5. Implementación simplificada de un allocator

### 5.1 Diseño

Para entender los conceptos, implementamos un allocator mínimo sobre un array
estático (simulando el heap):

- **Heap simulado**: `char heap[HEAP_SIZE]`.
- **Block header**: `{ size_t size; bool free; }`.
- **Free list implícita**: los bloques son contiguos; se recorren secuencialmente.
- **Estrategia**: first-fit (T02 profundizará en otras).

### 5.2 Pseudocódigo

```
my_malloc(size):
    block = heap_start
    while block < heap_end:
        if block.free and block.size >= size:
            if block.size > size + HEADER_SIZE + MIN_BLOCK:
                split(block, size)
            block.free = false
            return block + HEADER_SIZE
        block = next_block(block)
    return NULL  // out of memory

my_free(ptr):
    block = ptr - HEADER_SIZE
    block.free = true
    coalesce(block)  // merge with adjacent free blocks
```

---

## 6. Allocators reales

### 6.1 Tabla comparativa

| Allocator | Usado por | Estructura | Características |
|-----------|-----------|-----------|-----------------|
| **ptmalloc2** | glibc (Linux default) | Bins por tamaño, arenas por thread | Estándar, no el más rápido |
| **jemalloc** | FreeBSD, Firefox, Redis | Arenas, thread caches, slabs | Buen rendimiento multi-thread |
| **mimalloc** | Microsoft | Segmented pages, free lists por página | Muy rápido, bajo overhead |
| **tcmalloc** | Google (Abseil) | Thread-local caches, central free list | Escalable multi-thread |
| **dlmalloc** | Embebido, base de ptmalloc | Boundary tags, bins | Simple, portable |

### 6.2 Optimizaciones comunes

- **Thread-local caches**: cada thread tiene su propia free list para evitar
  contención.
- **Size classes (bins)**: separar bloques por rango de tamaño. Bloques de
  16 bytes en un bin, 32 en otro, etc. Reduce fragmentación y acelera búsqueda.
- **Boundary tags**: almacenar el tamaño también al final del bloque, permitiendo
  coalescing sin recorrer la lista.

---

## 7. Rust y la gestión de memoria

### 7.1 GlobalAlloc

En Rust, el allocator por defecto es el del sistema (libc malloc). Se puede
reemplazar con `#[global_allocator]`:

```rust
use std::alloc::{GlobalAlloc, Layout};

struct MyAllocator;

unsafe impl GlobalAlloc for MyAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 { ... }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) { ... }
}

#[global_allocator]
static ALLOC: MyAllocator = MyAllocator;
```

### 7.2 Layout y alineamiento

Rust exige especificar el **layout** (tamaño + alineamiento) en cada operación
de allocación. Esto es más seguro que C donde `free` no recibe el tamaño.

```rust
let layout = Layout::new::<[u8; 100]>();
// layout.size() == 100, layout.align() == 1
```

### 7.3 Ownership como allocator estático

El borrow checker de Rust garantiza en compilación que:
- Cada valor tiene exactamente un dueño (`Drop` se llama una sola vez).
- No hay use-after-free (las referencias no sobreviven al dueño).
- No hay double free (ownership se mueve, no se copia).

Esto elimina en compilación los errores que en C solo se detectan con Valgrind.

---

## 8. Programa completo en C

```c
/*
 * heap_internals.c
 *
 * Process heap internals: memory layout, sbrk, simple
 * allocator with free list, splitting, coalescing,
 * malloc behavior analysis.
 *
 * Compile: gcc -O0 -o heap_internals heap_internals.c
 * Run:     ./heap_internals
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* =================== SIMPLE ALLOCATOR ============================= */

#define HEAP_SIZE 1024
#define HEADER_SIZE sizeof(BlockHeader)
#define MIN_BLOCK 8
#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN - 1)) & ~(ALIGN - 1))

typedef struct BlockHeader {
    size_t size;    /* payload size */
    bool free;
} BlockHeader;

static char sim_heap[HEAP_SIZE];
static bool heap_initialized = false;

void heap_init(void) {
    BlockHeader *first = (BlockHeader *)sim_heap;
    first->size = HEAP_SIZE - HEADER_SIZE;
    first->free = true;
    heap_initialized = true;
}

static BlockHeader *next_block(BlockHeader *b) {
    return (BlockHeader *)((char *)b + HEADER_SIZE + b->size);
}

void *my_malloc(size_t size) {
    if (!heap_initialized) heap_init();
    size = ALIGN_UP(size);

    BlockHeader *b = (BlockHeader *)sim_heap;
    char *heap_end = sim_heap + HEAP_SIZE;

    while ((char *)b < heap_end) {
        if (b->free && b->size >= size) {
            /* Try splitting */
            if (b->size >= size + HEADER_SIZE + MIN_BLOCK) {
                BlockHeader *new_block = (BlockHeader *)((char *)b +
                                          HEADER_SIZE + size);
                new_block->size = b->size - size - HEADER_SIZE;
                new_block->free = true;
                b->size = size;
            }
            b->free = false;
            return (char *)b + HEADER_SIZE;
        }
        b = next_block(b);
    }
    return NULL;
}

void my_free(void *ptr) {
    if (!ptr) return;
    BlockHeader *b = (BlockHeader *)((char *)ptr - HEADER_SIZE);
    b->free = true;

    /* Coalesce with next block */
    BlockHeader *nxt = next_block(b);
    if ((char *)nxt < sim_heap + HEAP_SIZE && nxt->free) {
        b->size += HEADER_SIZE + nxt->size;
    }

    /* Coalesce with previous block (linear scan) */
    BlockHeader *prev = NULL;
    BlockHeader *cur = (BlockHeader *)sim_heap;
    while (cur != b && (char *)cur < sim_heap + HEAP_SIZE) {
        prev = cur;
        cur = next_block(cur);
    }
    if (prev && prev->free) {
        prev->size += HEADER_SIZE + b->size;
    }
}

void heap_dump(const char *label) {
    printf("  [%s] Heap blocks:\n", label);
    BlockHeader *b = (BlockHeader *)sim_heap;
    char *heap_end = sim_heap + HEAP_SIZE;
    int idx = 0;

    while ((char *)b < heap_end) {
        int offset = (int)((char *)b - sim_heap);
        printf("    #%d: offset=%d, size=%zu, %s\n",
               idx++, offset, b->size, b->free ? "FREE" : "USED");
        b = next_block(b);
    }
    printf("\n");
}

/* =================== DEMOS ======================================== */

int global_init = 42;      /* Data segment */
int global_uninit;          /* BSS segment */

void demo1_layout(void) {
    printf("=== Demo 1: Process Memory Layout ===\n\n");

    int stack_var = 0;
    int *heap_var = malloc(sizeof(int));

    printf("Segment addresses (approximate):\n");
    printf("  Text  (main):    %p\n", (void *)main);
    printf("  Data  (init'd):  %p\n", (void *)&global_init);
    printf("  BSS   (uninit):  %p\n", (void *)&global_uninit);
    printf("  Heap  (malloc):  %p\n", (void *)heap_var);
    printf("  Stack (local):   %p\n", (void *)&stack_var);

    printf("\nAddress ordering (low to high):\n");
    uintptr_t addrs[] = {
        (uintptr_t)main,
        (uintptr_t)&global_init,
        (uintptr_t)&global_uninit,
        (uintptr_t)heap_var,
        (uintptr_t)&stack_var
    };
    const char *names[] = {"Text", "Data", "BSS", "Heap", "Stack"};
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++)
            if (addrs[i] > addrs[j]) {
                uintptr_t ta = addrs[i]; addrs[i] = addrs[j]; addrs[j] = ta;
                const char *tn = names[i]; names[i] = names[j]; names[j] = tn;
            }

    for (int i = 0; i < 5; i++)
        printf("  %s: 0x%lx\n", names[i], (unsigned long)addrs[i]);

    free(heap_var);
    printf("\n");
}

void demo2_sbrk(void) {
    printf("=== Demo 2: sbrk — Heap Growth ===\n\n");

    void *brk0 = sbrk(0);
    printf("Initial program break: %p\n", brk0);

    /* Allocate with malloc (may or may not move brk) */
    void *p1 = malloc(1);
    void *brk1 = sbrk(0);
    printf("After malloc(1):       %p (break %s)\n", brk1,
           brk1 != brk0 ? "moved" : "unchanged");

    void *p2 = malloc(100000);
    void *brk2 = sbrk(0);
    printf("After malloc(100000):  %p (break %s)\n", brk2,
           brk2 != brk1 ? "moved" : "unchanged — may use mmap");

    free(p1);
    free(p2);

    void *brk3 = sbrk(0);
    printf("After free(both):      %p (break %s)\n", brk3,
           brk3 != brk2 ? "shrunk" : "unchanged — free doesn't always shrink");

    printf("\nNote: modern glibc may use mmap for medium allocations,\n");
    printf("      so brk may not move. Behavior is implementation-defined.\n\n");
}

void demo3_simple_allocator(void) {
    printf("=== Demo 3: Simple Allocator — First-Fit ===\n\n");

    heap_initialized = false;
    heap_init();
    heap_dump("initial");

    void *a = my_malloc(64);
    printf("  my_malloc(64) -> offset %d\n", (int)((char *)a - sim_heap));
    heap_dump("after malloc(64)");

    void *b = my_malloc(128);
    printf("  my_malloc(128) -> offset %d\n", (int)((char *)b - sim_heap));
    heap_dump("after malloc(128)");

    void *c = my_malloc(32);
    printf("  my_malloc(32) -> offset %d\n", (int)((char *)c - sim_heap));
    heap_dump("after malloc(32)");
}

void demo4_split_coalesce(void) {
    printf("=== Demo 4: Splitting and Coalescing ===\n\n");

    heap_initialized = false;
    heap_init();

    void *a = my_malloc(100);
    void *b = my_malloc(100);
    void *c = my_malloc(100);
    heap_dump("3 blocks allocated");

    my_free(a);
    heap_dump("after free(a) — first block free");

    my_free(c);
    heap_dump("after free(c) — third block free, coalesced with remainder");

    my_free(b);
    heap_dump("after free(b) — all free, fully coalesced");
}

void demo5_malloc_overhead(void) {
    printf("=== Demo 5: malloc Overhead and Alignment ===\n\n");

    printf("Header size in our allocator: %zu bytes\n", HEADER_SIZE);
    printf("Alignment: %d bytes\n\n", ALIGN);

    printf("Requested vs actual block size:\n");
    printf("  %-10s  %-12s  %-10s\n", "Requested", "Aligned", "Total (+ header)");
    size_t requests[] = {1, 7, 8, 13, 16, 24, 100};
    for (int i = 0; i < 7; i++) {
        size_t aligned = ALIGN_UP(requests[i]);
        printf("  %-10zu  %-12zu  %-10zu\n",
               requests[i], aligned, aligned + HEADER_SIZE);
    }

    printf("\nSystem malloc behavior:\n");
    for (int i = 0; i < 7; i++) {
        void *p = malloc(requests[i]);
        /* malloc_usable_size is a glibc extension */
        printf("  malloc(%zu): returned %p\n", requests[i], p);
        free(p);
    }
    printf("\n");
}

void demo6_fragmentation_visual(void) {
    printf("=== Demo 6: Fragmentation Visualization ===\n\n");

    heap_initialized = false;
    heap_init();

    /* Allocate 5 blocks */
    void *blocks[5];
    for (int i = 0; i < 5; i++)
        blocks[i] = my_malloc(80);
    heap_dump("5 blocks of 80 bytes each");

    /* Free every other block -> external fragmentation */
    my_free(blocks[1]);
    my_free(blocks[3]);
    heap_dump("freed blocks[1] and blocks[3] -> holes");

    /* Try to allocate 200 bytes */
    void *big = my_malloc(200);
    printf("  my_malloc(200): %s\n",
           big ? "SUCCESS" : "FAILED (no contiguous space)");
    printf("  Total free space: 2 * 80 = 160 + remainder,\n");
    printf("  but no single block is >= 200.\n");
    printf("  This is EXTERNAL FRAGMENTATION.\n\n");

    /* Free everything and retry */
    my_free(blocks[0]);
    my_free(blocks[2]);
    my_free(blocks[4]);
    heap_dump("all freed — coalesced");

    big = my_malloc(200);
    printf("  my_malloc(200) after full free: %s\n\n",
           big ? "SUCCESS (coalesced)" : "FAILED");
}

/* =================== MAIN ========================================= */

int main(void) {
    demo1_layout();
    demo2_sbrk();
    demo3_simple_allocator();
    demo4_split_coalesce();
    demo5_malloc_overhead();
    demo6_fragmentation_visual();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * heap_internals.rs
 *
 * Process heap concepts in Rust: memory layout, Layout and
 * alignment, custom allocator (bump), allocation statistics,
 * system allocator behavior.
 *
 * Compile: rustc -o heap_internals heap_internals.rs
 * Run:     ./heap_internals
 */

use std::alloc::{GlobalAlloc, Layout, System};
use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::ptr;

/* =================== COUNTING ALLOCATOR =========================== */

struct CountingAlloc {
    alloc_count: AtomicUsize,
    dealloc_count: AtomicUsize,
    bytes_allocated: AtomicUsize,
    bytes_freed: AtomicUsize,
}

unsafe impl GlobalAlloc for CountingAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        self.alloc_count.fetch_add(1, Ordering::Relaxed);
        self.bytes_allocated.fetch_add(layout.size(), Ordering::Relaxed);
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        self.dealloc_count.fetch_add(1, Ordering::Relaxed);
        self.bytes_freed.fetch_add(layout.size(), Ordering::Relaxed);
        unsafe { System.dealloc(ptr, layout) }
    }
}

unsafe impl Sync for CountingAlloc {}

#[global_allocator]
static ALLOC: CountingAlloc = CountingAlloc {
    alloc_count: AtomicUsize::new(0),
    dealloc_count: AtomicUsize::new(0),
    bytes_allocated: AtomicUsize::new(0),
    bytes_freed: AtomicUsize::new(0),
};

fn alloc_stats() -> (usize, usize, usize, usize) {
    (
        ALLOC.alloc_count.load(Ordering::Relaxed),
        ALLOC.dealloc_count.load(Ordering::Relaxed),
        ALLOC.bytes_allocated.load(Ordering::Relaxed),
        ALLOC.bytes_freed.load(Ordering::Relaxed),
    )
}

/* =================== BUMP ALLOCATOR (not global) ================== */

struct BumpAllocator {
    heap: UnsafeCell<[u8; 4096]>,
    offset: AtomicUsize,
    alloc_count: AtomicUsize,
}

impl BumpAllocator {
    const fn new() -> Self {
        BumpAllocator {
            heap: UnsafeCell::new([0u8; 4096]),
            offset: AtomicUsize::new(0),
            alloc_count: AtomicUsize::new(0),
        }
    }

    fn alloc(&self, size: usize, align: usize) -> Option<*mut u8> {
        let heap_ptr = self.heap.get() as *mut u8;
        let mut offset = self.offset.load(Ordering::Relaxed);

        /* Align up */
        let aligned = (offset + align - 1) & !(align - 1);
        let new_offset = aligned + size;

        if new_offset > 4096 {
            return None;
        }

        self.offset.store(new_offset, Ordering::Relaxed);
        self.alloc_count.fetch_add(1, Ordering::Relaxed);
        Some(unsafe { heap_ptr.add(aligned) })
    }

    fn reset(&self) {
        self.offset.store(0, Ordering::Relaxed);
        self.alloc_count.store(0, Ordering::Relaxed);
    }

    fn used(&self) -> usize {
        self.offset.load(Ordering::Relaxed)
    }

    fn remaining(&self) -> usize {
        4096 - self.used()
    }
}

unsafe impl Sync for BumpAllocator {}

/* =================== SIMPLE FREE LIST (on static buffer) ========== */

const FL_HEAP_SIZE: usize = 1024;
const FL_HEADER: usize = 16; /* size (8) + free flag (8) */

struct FreeListAlloc {
    heap: UnsafeCell<[u8; FL_HEAP_SIZE]>,
    initialized: AtomicUsize,
}

impl FreeListAlloc {
    const fn new() -> Self {
        FreeListAlloc {
            heap: UnsafeCell::new([0u8; FL_HEAP_SIZE]),
            initialized: AtomicUsize::new(0),
        }
    }

    fn init(&self) {
        if self.initialized.load(Ordering::Relaxed) != 0 { return; }
        let heap = self.heap.get() as *mut u8;
        unsafe {
            /* Write first block header: size and free flag */
            let size = FL_HEAP_SIZE - FL_HEADER;
            ptr::write(heap as *mut usize, size);
            ptr::write(heap.add(8) as *mut usize, 1); /* free = true */
        }
        self.initialized.store(1, Ordering::Relaxed);
    }

    fn alloc(&self, size: usize) -> Option<*mut u8> {
        self.init();
        let size = (size + 7) & !7; /* align to 8 */
        let heap = self.heap.get() as *mut u8;
        let mut offset = 0usize;

        while offset + FL_HEADER <= FL_HEAP_SIZE {
            let block_size = unsafe { ptr::read((heap.add(offset)) as *const usize) };
            let free = unsafe { ptr::read(heap.add(offset + 8) as *const usize) };

            if free != 0 && block_size >= size {
                /* Split if possible */
                if block_size >= size + FL_HEADER + 8 {
                    let new_offset = offset + FL_HEADER + size;
                    unsafe {
                        ptr::write(heap.add(new_offset) as *mut usize,
                                   block_size - size - FL_HEADER);
                        ptr::write(heap.add(new_offset + 8) as *mut usize, 1);
                        ptr::write(heap.add(offset) as *mut usize, size);
                    }
                }
                unsafe {
                    ptr::write(heap.add(offset + 8) as *mut usize, 0); /* used */
                }
                return Some(unsafe { heap.add(offset + FL_HEADER) });
            }

            offset += FL_HEADER + block_size;
        }
        None
    }

    fn free(&self, ptr: *mut u8) {
        let heap = self.heap.get() as *mut u8;
        let header = unsafe { ptr.sub(FL_HEADER) };
        unsafe {
            ptr::write(header.add(8) as *mut usize, 1); /* mark free */
        }
    }

    fn dump(&self) {
        self.init();
        let heap = self.heap.get() as *mut u8;
        let mut offset = 0usize;
        let mut idx = 0;

        while offset + FL_HEADER <= FL_HEAP_SIZE {
            let size = unsafe { ptr::read(heap.add(offset) as *const usize) };
            let free = unsafe { ptr::read(heap.add(offset + 8) as *const usize) };

            if size == 0 { break; }

            println!("    #{}: offset={}, size={}, {}",
                     idx, offset, size, if free != 0 { "FREE" } else { "USED" });
            idx += 1;
            offset += FL_HEADER + size;
        }
    }
}

unsafe impl Sync for FreeListAlloc {}

/* =================== DEMOS ======================================== */

fn demo1_layout() {
    println!("=== Demo 1: Memory Layout in Rust ===\n");

    static STATIC_VAR: i32 = 42;
    let stack_var: i32 = 99;
    let heap_var = Box::new(77i32);

    println!("Address comparison:");
    println!("  Static (data):  {:p}", &STATIC_VAR);
    println!("  Heap (Box):     {:p}", &*heap_var);
    println!("  Stack (local):  {:p}", &stack_var);
    println!("  Code (fn):      {:p}", demo1_layout as *const ());

    let static_addr = &STATIC_VAR as *const i32 as usize;
    let heap_addr = &*heap_var as *const i32 as usize;
    let stack_addr = &stack_var as *const i32 as usize;
    let code_addr = demo1_layout as *const () as usize;

    println!("\nOrdering (low to high):");
    let mut addrs = vec![
        ("Code", code_addr), ("Static", static_addr),
        ("Heap", heap_addr), ("Stack", stack_addr),
    ];
    addrs.sort_by_key(|&(_, a)| a);
    for (name, addr) in &addrs {
        println!("  {}: 0x{:x}", name, addr);
    }
    println!();
}

fn demo2_layout_alignment() {
    println!("=== Demo 2: Layout and Alignment ===\n");

    println!("{:<20} {:>5} {:>5}", "Type", "Size", "Align");
    println!("{:<20} {:>5} {:>5}", "----", "----", "-----");

    let types: Vec<(&str, Layout)> = vec![
        ("u8",     Layout::new::<u8>()),
        ("u16",    Layout::new::<u16>()),
        ("u32",    Layout::new::<u32>()),
        ("u64",    Layout::new::<u64>()),
        ("u128",   Layout::new::<u128>()),
        ("f64",    Layout::new::<f64>()),
        ("bool",   Layout::new::<bool>()),
        ("&str",   Layout::new::<&str>()),
        ("String", Layout::new::<String>()),
        ("Vec<i32>", Layout::new::<Vec<i32>>()),
        ("Box<i32>", Layout::new::<Box<i32>>()),
        ("[u8; 100]", Layout::new::<[u8; 100]>()),
    ];

    for (name, layout) in &types {
        println!("{:<20} {:>5} {:>5}", name, layout.size(), layout.align());
    }

    println!("\nLayout::from_size_align examples:");
    for &(size, align) in &[(1, 1), (7, 8), (100, 16), (1000, 4096)] {
        match Layout::from_size_align(size, align) {
            Ok(l) => println!("  size={}, align={} -> OK (padded_size={})",
                              size, align, l.pad_to_align().size()),
            Err(e) => println!("  size={}, align={} -> Error: {}", size, align, e),
        }
    }
    println!();
}

fn demo3_bump_allocator() {
    println!("=== Demo 3: Bump Allocator ===\n");

    static BUMP: BumpAllocator = BumpAllocator::new();
    BUMP.reset();

    println!("Bump allocator: 4096 bytes, allocate linearly.\n");

    for &size in &[64, 128, 32, 256, 100] {
        match BUMP.alloc(size, 8) {
            Some(ptr) => println!("  alloc({:>3}) -> offset={:>4}, remaining={}",
                                   size, BUMP.used() - size, BUMP.remaining()),
            None =>      println!("  alloc({:>3}) -> FAILED (out of space)", size),
        }
    }

    println!("\n  Total used: {}/{} bytes", BUMP.used(), 4096);
    println!("  Cannot free individual allocations (only reset all).");

    BUMP.reset();
    println!("  After reset: used={}, remaining={}\n", BUMP.used(), BUMP.remaining());
}

fn demo4_free_list() {
    println!("=== Demo 4: Free-List Allocator ===\n");

    static FL: FreeListAlloc = FreeListAlloc::new();
    FL.initialized.store(0, Ordering::Relaxed);
    FL.init();

    println!("Initial state:");
    FL.dump();

    let a = FL.alloc(64).unwrap();
    println!("\nAfter alloc(64):");
    FL.dump();

    let b = FL.alloc(128).unwrap();
    println!("\nAfter alloc(128):");
    FL.dump();

    FL.free(a);
    println!("\nAfter free(first block):");
    FL.dump();

    let c = FL.alloc(32).unwrap();
    println!("\nAfter alloc(32) — fits in freed block:");
    FL.dump();
    println!();
}

fn demo5_alloc_stats() {
    println!("=== Demo 5: Allocation Statistics ===\n");

    let (a0, d0, b0, f0) = alloc_stats();

    /* Allocate various things */
    let v: Vec<i32> = (0..1000).collect();
    let s = String::from("Hello, allocator!");
    let b = Box::new([0u8; 256]);

    let (a1, d1, b1, f1) = alloc_stats();
    println!("After creating Vec<i32>(1000), String, Box<[u8;256]>:");
    println!("  Allocations: {} new", a1 - a0);
    println!("  Bytes requested: {} new", b1 - b0);

    drop(v);
    drop(s);
    drop(b);

    let (a2, d2, b2, f2) = alloc_stats();
    println!("\nAfter dropping all:");
    println!("  Deallocations: {} new", d2 - d1);
    println!("  Bytes freed: {} new", f2 - f1);
    println!("  Live allocations: {}", (a2 - a0) - (d2 - d0));
    println!("  Net bytes: {}\n", (b2 - b0) as isize - (f2 - f0) as isize);
}

fn demo6_system_behavior() {
    println!("=== Demo 6: System Allocator Behavior ===\n");

    println!("Allocating increasing sizes and checking addresses:\n");
    println!("{:>10}  {:>18}  Note", "Size", "Address");

    let mut prev_addr = 0usize;
    for &size in &[8, 16, 64, 256, 1024, 4096, 65536, 262144] {
        let layout = Layout::from_size_align(size, 8).unwrap();
        let ptr = unsafe { System.alloc(layout) };
        let addr = ptr as usize;
        let note = if prev_addr != 0 {
            let gap = if addr > prev_addr {
                addr - prev_addr
            } else {
                prev_addr - addr
            };
            if gap > 1_000_000 { "large gap (likely mmap)" }
            else { "nearby (likely sbrk/arena)" }
        } else { "" };

        println!("{:>10}  0x{:016x}  {}", size, addr, note);
        unsafe { System.dealloc(ptr, layout); }
        prev_addr = addr;
    }

    println!("\nLarge allocations (>128KB) often use mmap, returning");
    println!("addresses far from the heap region.\n");
}

fn demo7_ownership_vs_gc() {
    println!("=== Demo 7: Ownership as Compile-Time GC ===\n");

    /* Demonstrate Drop being called automatically */
    struct Tracked {
        name: &'static str,
    }

    impl Drop for Tracked {
        fn drop(&mut self) {
            println!("  Drop called for '{}'", self.name);
        }
    }

    println!("Creating objects in a scope:");
    {
        let _a = Tracked { name: "alpha" };
        let _b = Tracked { name: "beta" };
        println!("  Inside scope: alpha and beta exist");
        /* beta is dropped first (LIFO order) */
    }
    println!("  After scope: both dropped automatically\n");

    println!("Move semantics prevent double free:");
    let v = vec![1, 2, 3];
    let v2 = v; /* move, not copy */
    /* v is no longer valid — compiler prevents use-after-move */
    println!("  v moved to v2: {:?}", v2);
    println!("  v is inaccessible (would be compile error to use)\n");
}

fn demo8_raw_alloc() {
    println!("=== Demo 8: Raw alloc/dealloc ===\n");

    let layout = Layout::from_size_align(100, 16).unwrap();
    println!("Layout: size={}, align={}", layout.size(), layout.align());

    let ptr = unsafe { std::alloc::alloc(layout) };
    if ptr.is_null() {
        println!("  Allocation failed!");
        return;
    }
    println!("  Allocated at: {:p}", ptr);
    println!("  Aligned to 16: {}", (ptr as usize) % 16 == 0);

    /* Write and read */
    unsafe {
        ptr::write_bytes(ptr, 0xAB, 100);
        println!("  First byte: 0x{:02x}", *ptr);
        println!("  Last byte:  0x{:02x}", *ptr.add(99));
    }

    unsafe { std::alloc::dealloc(ptr, layout); }
    println!("  Deallocated.\n");

    println!("In safe Rust, you'd use Box, Vec, or String instead.");
    println!("Raw alloc is only needed for custom allocators.\n");
}

/* =================== MAIN ========================================= */

fn main() {
    demo1_layout();
    demo2_layout_alignment();
    demo3_bump_allocator();
    demo4_free_list();
    demo5_alloc_stats();
    demo6_system_behavior();
    demo7_ownership_vs_gc();
    demo8_raw_alloc();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Layout de memoria

En un programa C con las declaraciones:
```c
int g = 10;          // A
static int s;        // B
const char *msg = "hello"; // C
int main() {
    int x = 5;       // D
    int *p = malloc(4); // E
}
```

¿En qué segmento reside cada variable (A-E)?

<details><summary>Predicción</summary>

- **A** (`g = 10`): **Data** (global inicializada).
- **B** (`static int s`): **BSS** (estática sin inicializar explícita; es 0 por defecto, pero el compilador la coloca en BSS).
- **C** (`msg`): el puntero `msg` está en **Data** (global inicializada). La cadena literal `"hello"` está en **Text** (o en una sección read-only `.rodata`).
- **D** (`x = 5`): **Stack** (variable local).
- **E** (`*p`): el puntero `p` está en **Stack**. La memoria apuntada (`malloc(4)`) está en el **Heap**.

</details>

### Ejercicio 2 — Overhead de malloc

Si `sizeof(size_t) = 8` y el allocator alinea a 16 bytes, ¿cuántos bytes de
heap consume `malloc(1)`? ¿Y `malloc(16)`?

<details><summary>Predicción</summary>

Asumiendo header de 16 bytes (size + flags, alineado):

- `malloc(1)`: payload alineado a 16 = 16 bytes. Total = 16 (header) + 16 (payload) = **32 bytes**.
- `malloc(16)`: payload = 16 (ya alineado). Total = 16 + 16 = **32 bytes**.

En glibc real (ptmalloc2), el overhead mínimo es diferente. El chunk mínimo es 32 bytes (header de 16 + payload de 16). Así que `malloc(1)` consume al menos 32 bytes en la mayoría de implementaciones 64-bit.

**Moraleja**: muchas allocaciones pequeñas desperdician proporcionalmente más memoria (overhead del header domina). Para 1 byte: 97% overhead.

</details>

### Ejercicio 3 — Splitting

Un bloque libre de 256 bytes (payload) recibe `malloc(80)`. Asumiendo header
de 16 bytes y alineamiento a 8 bytes, ¿cómo queda el bloque después del split?

<details><summary>Predicción</summary>

Alinear 80 a 8 bytes: $\lceil 80/8 \rceil \times 8 = 80$ (ya alineado).

Split:
- Bloque 1: header (16) + payload (80) = 96 bytes. Marcado USED.
- Bloque 2: header (16) + payload (256 - 80 - 16 = 160) = 176 bytes. Marcado FREE.

Total: 96 + 176 = 272 = 256 + 16 (el header original ya ocupaba sus 16 bytes, así que 256 payload se reparte en 80 + 16 + 160 = 256). ✓

Si el sobrante fuera < header + mínimo (por ejemplo, sobran solo 12 bytes), **no se divide**: se entrega todo el bloque (internal fragmentation menor que el costo de un header extra).

</details>

### Ejercicio 4 — Coalescing

Estado del heap (bloques consecutivos):

| # | Tamaño | Estado |
|:-:|:------:|:------:|
| 0 | 64 | FREE |
| 1 | 128 | USED |
| 2 | 32 | FREE |
| 3 | 64 | FREE |

Se ejecuta `free(bloque 1)`. ¿Cómo queda el heap después del coalescing?

<details><summary>Predicción</summary>

Tras marcar bloque 1 como FREE:
- Bloque 0: FREE (64), Bloque 1: FREE (128), Bloque 2: FREE (32), Bloque 3: FREE (64)

Coalescing con vecinos:
- Bloque 1 + Bloque 2 (adyacente, free): fusionar → FREE, size = 128 + 16 (header de 2) + 32 = 176.
- El resultado + Bloque 3 (adyacente, free): fusionar → FREE, size = 176 + 16 + 64 = 256.
- Bloque 0 + resultado (adyacente, free): fusionar → FREE, size = 64 + 16 + 256 = 336.

Resultado final: **1 solo bloque FREE de tamaño 336 bytes** (64 + 16 + 128 + 16 + 32 + 16 + 64 = 336, donde los 3 headers intermedios se absorben).

Todo el espacio útil se recupera. Sin coalescing, tendríamos 4 bloques libres pequeños que no podrían satisfacer una petición de 200 bytes.

</details>

### Ejercicio 5 — sbrk vs mmap

¿Por qué los allocators usan mmap para bloques grandes en lugar de sbrk?
Da un escenario concreto donde sbrk causaría problemas.

<details><summary>Predicción</summary>

**Problema con sbrk para bloques grandes**: sbrk solo puede expandir/contraer
el heap desde el final. Si se asignan bloques A (pequeño), B (grande), C
(pequeño) en secuencia y luego se libera B:

```
  [A: 16 bytes] [B: 1MB, FREE] [C: 16 bytes] --- break
```

La 1MB de B no se puede devolver al OS porque C está después. El break no puede retroceder más allá de C. Esta memoria queda "atrapada" indefinidamente.

**Con mmap**: B se mapea como una región independiente. Al liberarlo con `munmap`, la memoria se devuelve inmediatamente al OS, sin importar qué hay antes o después.

**Consecuencia**: para bloques grandes (≥128KB), los allocators usan mmap para evitar este tipo de retención de memoria a largo plazo.

</details>

### Ejercicio 6 — Boundary tags

Explica qué son los **boundary tags** y cómo permiten coalescing en $O(1)$.
¿Cuál es el costo en espacio?

<details><summary>Predicción</summary>

Un **boundary tag** duplica el tamaño del bloque al **final** del bloque (además
del header al inicio):

```
  [header: size, flags] [......payload......] [footer: size, flags]
```

**Ventaja**: para coalescer con el bloque **anterior**, solo necesitamos leer el
footer del bloque previo (ubicado justo antes de nuestro header):
`prev_footer = (char*)our_header - sizeof(footer)`. Esto da el tamaño del
bloque anterior, permitiendo saltar a su header.

Sin boundary tags, para encontrar el bloque anterior necesitaríamos recorrer
toda la lista desde el inicio: $O(n)$.

Con boundary tags: coalescing hacia adelante **y** hacia atrás en $O(1)$.

**Costo**: un `size_t` extra (8 bytes en 64-bit) por bloque. Para bloques
pequeños, esto puede ser significativo (16 bytes de metadata total: header +
footer, sobre un payload de quizás 16-32 bytes).

Optimización (ptmalloc): solo poner footer en bloques **libres** (los usados
no necesitan coalescing inmediato). Esto se indica con un bit en el header del
bloque siguiente.

</details>

### Ejercicio 7 — Layout en Rust

¿Cuál es el `Layout` de cada tipo? `(u8, u32)`, `(u32, u8)`, `(u8, u32, u8)`.
¿Importa el orden de los campos?

<details><summary>Predicción</summary>

- `(u8, u32)`: u8 (size=1, align=1) + padding(3) + u32 (size=4, align=4) = size **8**, align **4**.
- `(u32, u8)`: u32 (4) + u8 (1) + padding(3) = size **8**, align **4**.
- `(u8, u32, u8)`: u8(1) + pad(3) + u32(4) + u8(1) + pad(3) = size **12**, align **4**.

**Sí importa el orden**. En C, el compilador preserva el orden de los campos.
En Rust, por defecto (`repr(Rust)`), el compilador **puede reordenar** los
campos para minimizar padding. Con `#[repr(C)]`, el orden se preserva.

Ejemplo: con repr(Rust), `(u8, u32, u8)` podría reordenarse a `(u32, u8, u8)`
→ size = 4 + 1 + 1 + pad(2) = **8**, ahorrando 4 bytes.

</details>

### Ejercicio 8 — Bump allocator vs free list

Compara un bump allocator vs un free-list allocator para los siguientes
escenarios. ¿Cuál es mejor y por qué?

1. Parsear un JSON grande, procesarlo, y descartar todo.
2. Nodos de un BST con inserción/eliminación frecuente.

<details><summary>Predicción</summary>

**Escenario 1 (JSON parsing)**:
- **Bump allocator** es ideal. Todas las allocaciones son temporales y se
  descartan juntas. Allocar = incrementar puntero ($O(1)$). Liberar = resetear
  puntero ($O(1)$ para todo). Sin fragmentación, sin overhead de metadata.

**Escenario 2 (BST dinámica)**:
- **Free-list allocator** (o pool allocator, T02). Los nodos se crean y destruyen
  individualmente en orden impredecible. Un bump allocator no puede reclamar
  nodos individuales; la memoria solo crecería.
- Un **pool allocator** (bloques de tamaño fijo = sizeof(Node)) sería aún mejor:
  alloc/free en $O(1)$, sin fragmentación (todos los bloques son del mismo tamaño).

**Regla general**:
- Bump: todas las allocaciones tienen el mismo **lifetime** (se liberan juntas).
- Free-list/pool: allocaciones con lifetimes **individuales**.

</details>

### Ejercicio 9 — Detectar leaks conceptualmente

Sin Valgrind, ¿cómo podrías detectar memory leaks usando un allocator custom?
Diseña el mecanismo.

<details><summary>Predicción</summary>

Implementar un **counting allocator** (como el demo 5 en Rust) que registre:
1. Cada `alloc`: incrementar contador, guardar puntero + tamaño + callsite (usando `__FILE__`, `__LINE__` en C).
2. Cada `free`: buscar en el registro, marcarlo como liberado.
3. Al final del programa (`atexit`): iterar el registro. Todo lo que no fue liberado es un **leak**.

En C, esto se logra con macros:
```c
#define malloc(size) my_malloc(size, __FILE__, __LINE__)
#define free(ptr)    my_free(ptr, __FILE__, __LINE__)
```

El registro puede ser un array estático o una tabla hash. Al terminar, imprimir
todas las entradas no liberadas con su archivo y línea de origen.

Esto es esencialmente lo que Valgrind/ASan hacen, pero a nivel de instrumentación
binaria o compilación.

</details>

### Ejercicio 10 — Ownership vs GC

Explica con un ejemplo concreto por qué el modelo de ownership de Rust previene
use-after-free en compilación, mientras que C requiere herramientas de runtime.

<details><summary>Predicción</summary>

**C (use-after-free posible)**:
```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p); // use-after-free: compila, UB en runtime
```
El compilador de C no rastrea lifetimes. Este bug solo se detecta con Valgrind/ASan en runtime.

**Rust (error de compilación)**:
```rust
let p = Box::new(42);
drop(p);
println!("{}", *p); // ERROR: borrow of moved value 'p'
```
El borrow checker sabe que `p` fue moved/dropped. El código ni siquiera compila.

**Por qué funciona**: Rust rastrea estáticamente el **ownership** de cada valor.
Cuando un valor se mueve (`drop`, asignación, paso a función), el original se
invalida. El compilador inserta `Drop::drop()` automáticamente al final del
scope, garantizando exactamente una liberación por valor.

C no tiene este concepto — un `int*` es solo un número, y el compilador no sabe
si la memoria apuntada es válida.

</details>
