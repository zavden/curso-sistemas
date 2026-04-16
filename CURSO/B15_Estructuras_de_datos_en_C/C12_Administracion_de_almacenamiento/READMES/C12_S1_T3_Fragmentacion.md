# T03 — Fragmentacion

## Objetivo

Comprender en profundidad los dos tipos de fragmentacion de memoria —
**externa** e **interna** —, sus causas, como medirlas, y las tecnicas
para mitigarlas: **compactacion**, **coalescing**, **splitting controlado**
y **size classes**. Desarrollar la intuicion para diagnosticar problemas
de fragmentacion en programas reales.

---

## 1. Definicion y tipos

### 1.1 Fragmentacion externa

La **fragmentacion externa** ocurre cuando hay suficiente memoria libre
**en total** para satisfacer una peticion, pero no existe un **bloque
contiguo** lo suficientemente grande.

```
Peticion: malloc(200)

Heap: [USED:80][FREE:60][USED:40][FREE:90][USED:50][FREE:80]

Total libre: 60 + 90 + 80 = 230 >= 200
Pero el bloque mas grande es 90 < 200.  -> FALLA
```

La memoria esta "fragmentada" en huecos dispersos. El espacio existe pero
es inutilizable para la peticion.

### 1.2 Fragmentacion interna

La **fragmentacion interna** ocurre cuando un bloque asignado es **mas
grande** que lo que el usuario pidio. Los bytes sobrantes dentro del bloque
estan desperdiciados.

```
malloc(1) -> el allocator entrega un bloque de 16 bytes (alineamiento)

[HEADER:16][========16 bytes========]
                1 usado, 15 desperdiciados
```

Causas principales:
- **Alineamiento**: los bloques se redondean al multiplo de 8 o 16.
- **Tamano minimo**: el allocator impone un payload minimo (MIN_BLOCK).
- **Sin splitting**: si el sobrante tras un split seria demasiado pequeno,
  se entrega el bloque entero.
- **Size classes**: en allocators con bins, una peticion de 33 bytes se
  redondea a la clase de 48 o 64.

### 1.3 Comparacion

| Aspecto | Externa | Interna |
|---------|---------|---------|
| **Donde** | Entre bloques | Dentro de un bloque |
| **Causa** | Huecos no contiguos | Redondeo, alineamiento, no-split |
| **Afecta a** | Peticiones grandes | Todas las peticiones |
| **Deteccion** | Free list: total libre vs mayor bloque | Pedido vs asignado |
| **Solucion** | Compactacion, coalescing | Size classes, alineamiento fino |

---

## 2. Metricas de fragmentacion

### 2.1 Ratio de fragmentacion externa

$$F_{\text{ext}} = 1 - \frac{\text{bloque\_libre\_mas\_grande}}{\text{total\_libre}}$$

- $F_{\text{ext}} = 0$: toda la memoria libre es un solo bloque contiguo
  (sin fragmentacion).
- $F_{\text{ext}} \to 1$: la memoria libre esta dividida en muchos
  fragmentos pequenos (fragmentacion severa).

Ejemplo: total libre = 230, mayor bloque = 90.
$F_{\text{ext}} = 1 - 90/230 = 1 - 0.391 = 0.609$ (61% fragmentado).

### 2.2 Ratio de fragmentacion interna

$$F_{\text{int}} = \frac{\text{bytes\_asignados} - \text{bytes\_solicitados}}{\text{bytes\_asignados}}$$

- $F_{\text{int}} = 0$: cada bloque tiene exactamente el tamano pedido.
- $F_{\text{int}} = 0.5$: la mitad del espacio asignado es desperdicio.

Ejemplo: `malloc(1)` obtiene 16 bytes. $F_{\text{int}} = (16-1)/16 = 0.9375$
(94% desperdicio).

### 2.3 Utilizacion de memoria

La metrica global mas util es la **utilizacion**:

$$U = \frac{\text{bytes\_realmente\_usados}}{\text{bytes\_totales\_del\_heap}}$$

Un allocator ideal tiene $U$ cercano a 1. La fragmentacion (interna +
externa) reduce $U$. El overhead de metadata (headers) tambien reduce $U$.

### 2.4 Regla del 50% de Knuth

Knuth (1973) demostro que bajo ciertas condiciones de estado estable (donde
las tasas de allocacion y liberacion son iguales), el numero de bloques
**libres** tiende a ser la mitad del numero de bloques **usados**:

$$\text{bloques\_libres} \approx \frac{1}{2} \times \text{bloques\_usados}$$

Esto implica que incluso con coalescing perfecto, la fragmentacion
externa es inherente a los allocators de proposito general: siempre habra
huecos intercalados con bloques usados.

---

## 3. Causas y patrones

### 3.1 Patron ping-pong

Allocaciones y liberaciones alternadas de tamanos diferentes:

```
a = malloc(100)
b = malloc(200)
c = malloc(100)
free(a)         -> hole de 100
free(c)         -> hole de 100 (no contiguo con el primero)
malloc(250)     -> FALLA (200 libres pero en 2 huecos de 100)
```

### 3.2 Patron de envejecimiento

Allocaciones de larga duracion intercaladas con allocaciones efimeras.
Las efimeras se liberan, dejando huecos. Las de larga duracion impiden
el coalescing:

```
Tiempo 0: [A_largo][B_corto][C_largo][D_corto][E_largo]
Tiempo 1: [A_largo][  FREE ][C_largo][  FREE ][E_largo]
```

Los huecos de B y D nunca se podran fusionar porque C los separa.

### 3.3 Patron de escalera

Allocaciones de tamano creciente seguidas de liberacion total y
re-allocacion:

```
Phase 1: malloc(10), malloc(20), malloc(40), malloc(80)
Phase 2: free all -> un solo bloque
Phase 3: malloc(10), malloc(20), malloc(40), malloc(80) -> OK

Phase 1b: malloc(10), malloc(20), malloc(40), malloc(80)
Phase 2b: free(10), free(40) -> holes de 10 y 40
Phase 3b: malloc(50) -> 10 < 50, 40 < 50 -> FALLA (total libre=50)
```

### 3.4 Size classes como mitigacion

Si todas las allocaciones se redondean a **clases de tamano fijas** (8, 16,
32, 64, 128, ...), un hueco de tamano $c$ siempre puede satisfacer una
peticion de tamano $\leq c$ de la misma clase. Esto **elimina** la
fragmentacion externa dentro de cada clase, a costa de aumentar la
fragmentacion interna (redondeo).

---

## 4. Compactacion

### 4.1 Concepto

La **compactacion** mueve los bloques **usados** para que queden contiguos,
eliminando todos los huecos y creando un unico bloque libre grande al final:

```
Antes:  [A:80][FREE:60][B:40][FREE:90][C:50]
Despues:[A:80][B:40][C:50][========FREE:200========]
```

### 4.2 Requisitos

La compactacion requiere **actualizar todas las referencias** (punteros)
a los bloques movidos. Esto es viable solo si:

1. El sistema conoce **todos los punteros** al heap (GC con grafo de
   objetos).
2. Se usan **handles** (punteros indirectos): el usuario tiene un handle
   que apunta a una tabla, y la tabla apunta al bloque real. Al mover el
   bloque, solo se actualiza la tabla.

### 4.3 Por que C no puede compactar

En C (y C++), los punteros son **direcciones reales**. El runtime no sabe
cuales variables son punteros ni a donde apuntan. Mover un bloque
invalidaria todos los punteros existentes a el, causando dangling pointers.

Por eso, los allocators de C/C++ **nunca compactan**. Dependen enteramente
del coalescing y las estrategias de asignacion para mitigar la fragmentacion.

### 4.4 Lenguajes que compactan

| Lenguaje | Mecanismo | Tipo |
|----------|-----------|------|
| **Java** | GC generacional con compactacion | Mueve objetos, actualiza referencias |
| **C#/.NET** | GC con *pinning* para interop | Compacta a menos que se fije un objeto |
| **Go** | GC concurrente (no compacta actualmente) | Usa segmentos de tamano fijo |
| **Lua** | GC incremental + compactacion de strings | Parcial |
| **C/Rust** | No compacta | Punteros son direcciones reales |

### 4.5 Handles como alternativa

Un sistema de **handles** permite compactacion en C:

```
Handle table:
  [0] -> 0x1000  (bloque A)
  [1] -> 0x1080  (bloque B)
  [2] -> 0x10C0  (bloque C)

El usuario tiene: Handle h = 1;
Acceso: *resolve(h) = handle_table[h]

Al compactar, se mueve B de 0x1080 a 0x1050.
Se actualiza: handle_table[1] = 0x1050.
El usuario sigue usando h = 1 — transparente.
```

**Costo**: una indirecion extra en cada acceso. Los gestores de recursos
de Windows 3.1 usaban este patron (LocalAlloc con LMEM_MOVEABLE).

---

## 5. Coalescing: defensa contra fragmentacion externa

### 5.1 Coalescing inmediato vs diferido

| Tipo | Cuando | Ventaja | Desventaja |
|------|--------|---------|------------|
| **Inmediato** | En cada `free()` | Maximo bloque libre siempre | Costo en cada free |
| **Diferido** | Solo cuando `malloc` falla | Free es $O(1)$ | Fragmentacion temporal |

### 5.2 Coalescing inmediato con boundary tags

Con **boundary tags** (footer al final de cada bloque, ver T01), el
coalescing en ambas direcciones es $O(1)$:

```
free(B):
  1. Leer footer del bloque anterior -> obtener su tamano y estado
  2. Leer header del bloque siguiente -> obtener su tamano y estado
  3. Si anterior es FREE: fusionar (prev.size += HEADER + B.size)
  4. Si siguiente es FREE: fusionar (result.size += HEADER + next.size)
```

Cuatro casos posibles al liberar el bloque B:

| Anterior | Siguiente | Accion |
|:--------:|:---------:|--------|
| USED | USED | Solo marcar B como FREE |
| USED | FREE | Fusionar B + siguiente |
| FREE | USED | Fusionar anterior + B |
| FREE | FREE | Fusionar anterior + B + siguiente |

### 5.3 Coalescing diferido

Algunos allocators (como ptmalloc2 para fastbins) **no coalescen
inmediatamente** los bloques pequenos. Esto permite reutilizar bloques
de tamano exacto rapidamente. El coalescing se activa cuando:

- `malloc` no encuentra un bloque adecuado.
- Se consolidan los fastbins (malloc_consolidate en glibc).
- Se llama `malloc_trim` o `mallopt`.

---

## 6. Fragmentacion en la practica

### 6.1 Servidores de larga ejecucion

Un servidor web que procesa millones de peticiones puede sufrir
**fragmentacion progresiva**: cada peticion aloca strings, buffers y
objetos temporales de tamanos variados. Tras horas o dias, la memoria
RSS (Resident Set Size) crece mucho mas de lo esperado, aunque el
uso logico es estable.

Solucion tipica: usar **arena allocators** por peticion (alocar
linealmente, liberar todo al final de la peticion). Esto elimina la
fragmentacion entre peticiones.

### 6.2 Sistemas embebidos

Con heap limitado (ej. 64 KB), incluso un porcentaje pequeno de
fragmentacion puede ser catastrofico. Soluciones:

- **Pool allocators**: todos los objetos del mismo tamano.
- **Buddy system** (T04): fragmentacion controlada con merge rapido.
- **Prohibir malloc dinamico**: todo en stack o arrays estaticos.

### 6.3 jemalloc vs ptmalloc2

| Aspecto | ptmalloc2 | jemalloc |
|---------|-----------|----------|
| Size classes | 128 bins, spacing variable | ~200 clases, spacing regular |
| Thread caches | Arenas (1 per core) | Thread-local caches + arenas |
| Fragmentacion tipica | 5-15% para workloads generales | 2-8%, mejor para workloads variados |
| Coalescing | Inmediato (excepto fastbins) | Diferido con paginas |

### 6.4 Diagnosticando fragmentacion

Senales de fragmentacion en un programa:

1. **RSS crece pero uso logico es estable**: el heap tiene huecos.
2. **malloc retorna NULL** con memoria "disponible": fragmentacion externa.
3. **mallinfo().fordblks** (glibc): bytes en la free list.
4. **Valgrind massif**: visualiza el heap a lo largo del tiempo.
5. **jemalloc stats**: `malloc_stats_print()` muestra fragmentacion por bin.

---

## 7. La regla del 50% y el teorema de Robson

### 7.1 Regla del 50% de Knuth (ampliada)

Bajo un modelo de estado estable donde:
- Las allocaciones y liberaciones llegan a la misma tasa.
- Los tamanos de peticion se distribuyen uniformemente.
- Se usa first fit con coalescing.

Knuth demostro que:

$$N_{\text{free}} \approx \frac{N_{\text{used}}}{2}$$

Consecuencia: si hay 1000 bloques usados, habra ~500 huecos. La
fragmentacion es **inevitable** en allocators de proposito general.

### 7.2 Cota de Robson

Robson (1971) demostro una cota inferior para la **peor fragmentacion
posible**. Para un allocator que debe servir peticiones de tamanos entre
$m$ (minimo) y $M$ (maximo), la memoria requerida en el peor caso es:

$$\text{memoria\_peor\_caso} = M \cdot \lceil \log_2(M/m) \rceil \cdot \text{memoria\_viva\_maxima}$$

Esto significa que para $m = 1, M = 1024$, se podria necesitar hasta
$1024 \times 10 = 10240$ veces la memoria realmente en uso. En la practica,
los patrones de uso son mucho mas benignos que el peor caso de Robson, pero
el teorema demuestra que **ningun allocator de proposito general puede
garantizar fragmentacion cero**.

---

## 8. Programa completo en C

```c
/*
 * fragmentation.c
 *
 * Demonstrates internal and external fragmentation,
 * compaction simulation, coalescing strategies, and
 * fragmentation metrics on a simulated heap.
 *
 * Compile: gcc -O0 -o fragmentation fragmentation.c
 * Run:     ./fragmentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* =================== SIMULATED HEAP ================================ */

#define HEAP_SIZE 2048
#define HEADER_SIZE sizeof(BlockHeader)
#define MIN_BLOCK 8
#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN - 1)) & ~(ALIGN - 1))

typedef struct BlockHeader {
    size_t size;      /* payload size */
    size_t requested; /* original requested size (for internal frag) */
    bool free;
} BlockHeader;

typedef struct {
    char heap[HEAP_SIZE];
} Heap;

static BlockHeader *block_at(Heap *h, int offset) {
    return (BlockHeader *)(h->heap + offset);
}

static int block_next(BlockHeader *b, int offset) {
    return offset + (int)HEADER_SIZE + (int)b->size;
}

void heap_init(Heap *h) {
    memset(h->heap, 0, HEAP_SIZE);
    BlockHeader *first = block_at(h, 0);
    first->size = HEAP_SIZE - HEADER_SIZE;
    first->requested = 0;
    first->free = true;
}

void *heap_alloc(Heap *h, size_t req_size) {
    size_t size = ALIGN_UP(req_size);
    if (size == 0) size = ALIGN;

    int offset = 0;
    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *b = block_at(h, offset);
        if (b->size == 0) break;

        if (b->free && b->size >= size) {
            if (b->size >= size + HEADER_SIZE + MIN_BLOCK) {
                int new_off = offset + (int)HEADER_SIZE + (int)size;
                BlockHeader *nb = block_at(h, new_off);
                nb->size = b->size - size - HEADER_SIZE;
                nb->requested = 0;
                nb->free = true;
                b->size = size;
            }
            b->free = false;
            b->requested = req_size;
            return h->heap + offset + HEADER_SIZE;
        }
        offset = block_next(b, offset);
    }
    return NULL;
}

void heap_free(Heap *h, void *ptr) {
    if (!ptr) return;
    BlockHeader *b = (BlockHeader *)((char *)ptr - HEADER_SIZE);
    b->free = true;
    b->requested = 0;
    int offset = (int)((char *)b - h->heap);

    /* Coalesce forward */
    int noff = block_next(b, offset);
    if (noff + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *nxt = block_at(h, noff);
        if (nxt->size > 0 && nxt->free) {
            b->size += HEADER_SIZE + nxt->size;
        }
    }

    /* Coalesce backward */
    BlockHeader *prev = NULL;
    int poff = -1;
    int cur = 0;
    while (cur < offset) {
        BlockHeader *c = block_at(h, cur);
        if (c->size == 0) break;
        prev = c;
        poff = cur;
        cur = block_next(c, cur);
    }
    if (prev && prev->free) {
        prev->size += HEADER_SIZE + b->size;
    }
}

/* No coalescing version of free */
void heap_free_no_coalesce(Heap *h, void *ptr) {
    if (!ptr) return;
    BlockHeader *b = (BlockHeader *)((char *)ptr - HEADER_SIZE);
    b->free = true;
    b->requested = 0;
}

void heap_dump(Heap *h, const char *label) {
    printf("  [%s]\n", label);
    int offset = 0;
    int idx = 0;
    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *b = block_at(h, offset);
        if (b->size == 0) break;
        if (b->free) {
            printf("    #%d: off=%4d, size=%4zu, FREE\n",
                   idx, offset, b->size);
        } else {
            printf("    #%d: off=%4d, size=%4zu, USED (req=%zu, waste=%zu)\n",
                   idx, offset, b->size, b->requested,
                   b->size - b->requested);
        }
        idx++;
        offset = block_next(b, offset);
    }
}

typedef struct {
    int free_blocks;
    size_t free_bytes;
    size_t largest_free;
    int used_blocks;
    size_t used_bytes;
    size_t requested_bytes;
    size_t header_overhead;
} HeapStats;

HeapStats heap_stats(Heap *h) {
    HeapStats s = {0};
    int offset = 0;
    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *b = block_at(h, offset);
        if (b->size == 0) break;
        s.header_overhead += HEADER_SIZE;
        if (b->free) {
            s.free_blocks++;
            s.free_bytes += b->size;
            if (b->size > s.largest_free)
                s.largest_free = b->size;
        } else {
            s.used_blocks++;
            s.used_bytes += b->size;
            s.requested_bytes += b->requested;
        }
        offset = block_next(b, offset);
    }
    return s;
}

void print_frag_metrics(Heap *h) {
    HeapStats s = heap_stats(h);

    double f_ext = (s.free_bytes > 0)
        ? 1.0 - (double)s.largest_free / (double)s.free_bytes
        : 0.0;
    double f_int = (s.used_bytes > 0)
        ? (double)(s.used_bytes - s.requested_bytes) / (double)s.used_bytes
        : 0.0;
    double util = (double)s.requested_bytes / (double)HEAP_SIZE;

    printf("    Free: %d blocks, %zu bytes (largest=%zu)\n",
           s.free_blocks, s.free_bytes, s.largest_free);
    printf("    Used: %d blocks, %zu bytes (requested=%zu)\n",
           s.used_blocks, s.used_bytes, s.requested_bytes);
    printf("    Header overhead: %zu bytes\n", s.header_overhead);
    printf("    F_ext = %.3f (0=none, 1=severe)\n", f_ext);
    printf("    F_int = %.3f (0=none, 1=all waste)\n", f_int);
    printf("    Utilization = %.3f\n", util);
}

/* =================== DEMOS ========================================= */

void demo1_internal_fragmentation(void) {
    printf("=== Demo 1: Internal Fragmentation ===\n\n");

    Heap h;
    heap_init(&h);

    printf("Allocating small sizes (alignment = %d bytes):\n\n", ALIGN);
    printf("  %-10s  %-8s  %-8s  %-8s\n",
           "Requested", "Aligned", "Waste", "F_int");

    size_t total_req = 0, total_alloc = 0;
    size_t sizes[] = {1, 3, 5, 7, 10, 13, 15, 17, 25, 31};

    for (int i = 0; i < 10; i++) {
        size_t req = sizes[i];
        size_t aligned = ALIGN_UP(req);
        size_t waste = aligned - req;
        double f = (double)waste / (double)aligned;
        printf("  %-10zu  %-8zu  %-8zu  %-8.3f\n",
               req, aligned, waste, f);
        total_req += req;
        total_alloc += aligned;
        heap_alloc(&h, req);
    }

    printf("\n  Total requested: %zu, allocated: %zu\n", total_req, total_alloc);
    printf("  Overall internal fragmentation: %.1f%%\n",
           100.0 * (double)(total_alloc - total_req) / (double)total_alloc);
    printf("  Plus header overhead: %zu bytes per block = %zu total\n\n",
           HEADER_SIZE, HEADER_SIZE * 10);
}

void demo2_external_fragmentation(void) {
    printf("=== Demo 2: External Fragmentation ===\n\n");

    Heap h;
    heap_init(&h);

    /* Allocate 8 blocks */
    void *blocks[8];
    for (int i = 0; i < 8; i++)
        blocks[i] = heap_alloc(&h, 100);

    printf("After 8 x malloc(100):\n");
    print_frag_metrics(&h);

    /* Free every other block (no coalescing) */
    for (int i = 0; i < 8; i += 2)
        heap_free_no_coalesce(&h, blocks[i]);

    printf("\nAfter freeing blocks 0,2,4,6 (no coalescing):\n");
    heap_dump(&h, "checkerboard pattern");
    print_frag_metrics(&h);

    /* Try to allocate 200 bytes */
    void *big = heap_alloc(&h, 200);
    printf("\n  malloc(200): %s\n",
           big ? "SUCCESS" : "FAILED (external fragmentation)");
    printf("  400 bytes free but no block >= 200.\n\n");
}

void demo3_coalescing_effect(void) {
    printf("=== Demo 3: Coalescing Effect ===\n\n");

    /* Without coalescing */
    Heap h1;
    heap_init(&h1);

    void *a1 = heap_alloc(&h1, 100);
    void *b1 = heap_alloc(&h1, 100);
    void *c1 = heap_alloc(&h1, 100);
    heap_free_no_coalesce(&h1, a1);
    heap_free_no_coalesce(&h1, b1);
    heap_free_no_coalesce(&h1, c1);

    printf("Without coalescing (free a, b, c individually):\n");
    heap_dump(&h1, "3 separate free blocks");
    void *big1 = heap_alloc(&h1, 250);
    printf("  malloc(250): %s\n", big1 ? "SUCCESS" : "FAILED");
    HeapStats s1 = heap_stats(&h1);
    double f1 = 1.0 - (double)s1.largest_free / (double)s1.free_bytes;
    printf("  F_ext = %.3f\n\n", f1);

    /* With coalescing */
    Heap h2;
    heap_init(&h2);

    void *a2 = heap_alloc(&h2, 100);
    void *b2 = heap_alloc(&h2, 100);
    void *c2 = heap_alloc(&h2, 100);
    heap_free(&h2, a2);
    heap_free(&h2, b2);
    heap_free(&h2, c2);

    printf("With coalescing (free a, b, c with merge):\n");
    heap_dump(&h2, "one merged free block");
    void *big2 = heap_alloc(&h2, 250);
    printf("  malloc(250): %s\n", big2 ? "SUCCESS" : "FAILED");
    HeapStats s2 = heap_stats(&h2);
    double f2 = (s2.free_bytes > 0)
        ? 1.0 - (double)s2.largest_free / (double)s2.free_bytes
        : 0.0;
    printf("  F_ext = %.3f\n\n", f2);
}

void demo4_aging_pattern(void) {
    printf("=== Demo 4: Aging Pattern ===\n\n");

    Heap h;
    heap_init(&h);

    printf("Simulating server: long-lived objects pinning short-lived ones.\n\n");

    /* Long-lived (persistent connections) */
    void *persistent[5];
    void *ephemeral[5];

    for (int i = 0; i < 5; i++) {
        persistent[i] = heap_alloc(&h, 80);
        ephemeral[i] = heap_alloc(&h, 60);
    }

    printf("After interleaved alloc (80 persistent, 60 ephemeral):\n");
    print_frag_metrics(&h);

    /* Free all ephemeral */
    for (int i = 0; i < 5; i++)
        heap_free(&h, ephemeral[i]);

    printf("\nAfter freeing all ephemeral (60-byte holes between 80-byte blocks):\n");
    heap_dump(&h, "aging pattern");
    print_frag_metrics(&h);

    /* Try to allocate a larger block */
    void *big = heap_alloc(&h, 200);
    printf("\n  malloc(200): %s\n",
           big ? "SUCCESS (found a block)" : "FAILED");

    /* The holes are only 60 bytes — too small for 200 */
    /* But the remainder at the end should be large enough */
    printf("  Individual holes are only 60 bytes.\n");
    printf("  Only the remainder block at the end can serve large allocs.\n\n");
}

void demo5_compaction_simulation(void) {
    printf("=== Demo 5: Compaction Simulation ===\n\n");

    Heap h;
    heap_init(&h);

    void *a = heap_alloc(&h, 80);
    void *b = heap_alloc(&h, 120);
    void *c = heap_alloc(&h, 60);
    void *d = heap_alloc(&h, 100);

    heap_free(&h, b);
    heap_free(&h, d);

    printf("Before compaction:\n");
    heap_dump(&h, "holes between used blocks");
    print_frag_metrics(&h);

    /* Simulate compaction by rebuilding the heap */
    printf("\n--- Simulating compaction ---\n\n");

    /* Collect used blocks */
    typedef struct { size_t size; size_t requested; } UsedBlock;
    UsedBlock used[10];
    int used_count = 0;

    int offset = 0;
    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *blk = block_at(&h, offset);
        if (blk->size == 0) break;
        if (!blk->free) {
            used[used_count].size = blk->size;
            used[used_count].requested = blk->requested;
            used_count++;
        }
        offset = block_next(blk, offset);
    }

    /* Rebuild heap with used blocks packed at the start */
    heap_init(&h);
    offset = 0;
    for (int i = 0; i < used_count; i++) {
        BlockHeader *blk = block_at(&h, offset);
        blk->size = used[i].size;
        blk->requested = used[i].requested;
        blk->free = false;
        offset = block_next(blk, offset);
    }

    /* Remaining space is one free block */
    if (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *rem = block_at(&h, offset);
        rem->size = HEAP_SIZE - offset - HEADER_SIZE;
        rem->requested = 0;
        rem->free = true;
    }

    printf("After compaction:\n");
    heap_dump(&h, "all used blocks packed, one large free block");
    print_frag_metrics(&h);
    printf("\n  Note: in C, compaction is impossible with raw pointers.\n");
    printf("  This simulation uses a rebuild — real compaction needs handles.\n\n");
}

void demo6_size_class_effect(void) {
    printf("=== Demo 6: Size Classes vs Raw Allocation ===\n\n");

    /* Raw allocation: exact sizes */
    Heap h_raw;
    heap_init(&h_raw);

    size_t raw_sizes[] = {13, 27, 5, 41, 19, 33, 9, 50, 7, 45};
    void *raw_ptrs[10];
    for (int i = 0; i < 10; i++)
        raw_ptrs[i] = heap_alloc(&h_raw, raw_sizes[i]);

    /* Free odd-indexed */
    for (int i = 1; i < 10; i += 2)
        heap_free(&h_raw, raw_ptrs[i]);

    printf("Raw allocation (varied sizes, free odd-indexed):\n");
    print_frag_metrics(&h_raw);

    /* Size class allocation: round to {16, 32, 48, 64} */
    Heap h_class;
    heap_init(&h_class);

    void *class_ptrs[10];
    printf("\n  Size class rounding:\n");
    for (int i = 0; i < 10; i++) {
        size_t rounded;
        if (raw_sizes[i] <= 16) rounded = 16;
        else if (raw_sizes[i] <= 32) rounded = 32;
        else if (raw_sizes[i] <= 48) rounded = 48;
        else rounded = 64;
        printf("    %2zu -> %zu\n", raw_sizes[i], rounded);
        class_ptrs[i] = heap_alloc(&h_class, rounded);
        /* Update requested to original for metric accuracy */
        BlockHeader *b = (BlockHeader *)((char *)class_ptrs[i] - HEADER_SIZE);
        b->requested = raw_sizes[i];
    }

    for (int i = 1; i < 10; i += 2)
        heap_free(&h_class, class_ptrs[i]);

    printf("\nSize-class allocation (same pattern, rounded):\n");
    print_frag_metrics(&h_class);

    printf("\n  Size classes increase F_int but freed blocks are\n");
    printf("  more likely to be reused (same class).\n\n");
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_internal_fragmentation();
    demo2_external_fragmentation();
    demo3_coalescing_effect();
    demo4_aging_pattern();
    demo5_compaction_simulation();
    demo6_size_class_effect();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * fragmentation.rs
 *
 * Demonstrates internal and external fragmentation,
 * compaction simulation, coalescing strategies, and
 * fragmentation metrics on a simulated heap.
 *
 * Compile: rustc -o fragmentation fragmentation.rs
 * Run:     ./fragmentation
 */

/* =================== HEAP SIMULATION =============================== */

const HEAP_SIZE: usize = 2048;
const HEADER_SIZE: usize = 24;  /* size(8) + requested(8) + free_flag(8) */
const MIN_BLOCK: usize = 8;
const ALIGNMENT: usize = 8;

fn align_up(x: usize) -> usize {
    (x + ALIGNMENT - 1) & !(ALIGNMENT - 1)
}

struct SimHeap {
    data: Vec<u8>,
}

impl SimHeap {
    fn new() -> Self {
        let mut h = SimHeap {
            data: vec![0u8; HEAP_SIZE],
        };
        h.write_header(0, HEAP_SIZE - HEADER_SIZE, 0, true);
        h
    }

    fn write_header(&mut self, off: usize, size: usize, requested: usize, free: bool) {
        self.data[off..off+8].copy_from_slice(&size.to_ne_bytes());
        self.data[off+8..off+16].copy_from_slice(&requested.to_ne_bytes());
        let flag: usize = if free { 1 } else { 0 };
        self.data[off+16..off+24].copy_from_slice(&flag.to_ne_bytes());
    }

    fn read_size(&self, off: usize) -> usize {
        let mut b = [0u8; 8];
        b.copy_from_slice(&self.data[off..off+8]);
        usize::from_ne_bytes(b)
    }

    fn read_requested(&self, off: usize) -> usize {
        let mut b = [0u8; 8];
        b.copy_from_slice(&self.data[off+8..off+16]);
        usize::from_ne_bytes(b)
    }

    fn read_free(&self, off: usize) -> bool {
        let mut b = [0u8; 8];
        b.copy_from_slice(&self.data[off+16..off+24]);
        usize::from_ne_bytes(b) != 0
    }

    fn next_off(&self, off: usize) -> usize {
        off + HEADER_SIZE + self.read_size(off)
    }

    fn alloc(&mut self, req: usize) -> Option<usize> {
        let size = align_up(if req == 0 { ALIGNMENT } else { req });
        let mut off = 0;

        while off + HEADER_SIZE <= HEAP_SIZE {
            let bsize = self.read_size(off);
            if bsize == 0 { break; }

            if self.read_free(off) && bsize >= size {
                if bsize >= size + HEADER_SIZE + MIN_BLOCK {
                    let new_off = off + HEADER_SIZE + size;
                    let remainder = bsize - size - HEADER_SIZE;
                    self.write_header(new_off, remainder, 0, true);
                    self.write_header(off, size, req, false);
                } else {
                    self.write_header(off, bsize, req, false);
                }
                return Some(off + HEADER_SIZE);
            }
            off = self.next_off(off);
        }
        None
    }

    fn free_coalesce(&mut self, ptr: usize) {
        let off = ptr - HEADER_SIZE;
        let size = self.read_size(off);
        self.write_header(off, size, 0, true);

        /* Coalesce forward */
        let noff = self.next_off(off);
        if noff + HEADER_SIZE <= HEAP_SIZE {
            let ns = self.read_size(noff);
            if ns > 0 && self.read_free(noff) {
                let merged = self.read_size(off) + HEADER_SIZE + ns;
                self.write_header(off, merged, 0, true);
            }
        }

        /* Coalesce backward */
        let final_size = self.read_size(off);
        let mut prev: Option<usize> = None;
        let mut cur = 0;
        while cur < off {
            let cs = self.read_size(cur);
            if cs == 0 { break; }
            prev = Some(cur);
            cur = cur + HEADER_SIZE + cs;
        }
        if let Some(po) = prev {
            if self.read_free(po) {
                let ps = self.read_size(po);
                let merged = ps + HEADER_SIZE + final_size;
                self.write_header(po, merged, 0, true);
            }
        }
    }

    fn free_no_coalesce(&mut self, ptr: usize) {
        let off = ptr - HEADER_SIZE;
        let size = self.read_size(off);
        self.write_header(off, size, 0, true);
    }

    fn dump(&self, label: &str) {
        println!("  [{}]", label);
        let mut off = 0;
        let mut idx = 0;
        while off + HEADER_SIZE <= HEAP_SIZE {
            let size = self.read_size(off);
            if size == 0 { break; }
            let free = self.read_free(off);
            if free {
                println!("    #{}: off={:>4}, size={:>4}, FREE", idx, off, size);
            } else {
                let req = self.read_requested(off);
                println!("    #{}: off={:>4}, size={:>4}, USED (req={}, waste={})",
                         idx, off, size, req, size - req);
            }
            idx += 1;
            off = self.next_off(off);
        }
    }

    fn stats(&self) -> Stats {
        let mut s = Stats::default();
        let mut off = 0;
        while off + HEADER_SIZE <= HEAP_SIZE {
            let size = self.read_size(off);
            if size == 0 { break; }
            s.header_overhead += HEADER_SIZE;
            if self.read_free(off) {
                s.free_blocks += 1;
                s.free_bytes += size;
                if size > s.largest_free { s.largest_free = size; }
            } else {
                s.used_blocks += 1;
                s.used_bytes += size;
                s.requested_bytes += self.read_requested(off);
            }
            off = self.next_off(off);
        }
        s
    }

    fn compact(&mut self) {
        /* Collect used blocks */
        let mut used: Vec<(usize, usize)> = Vec::new(); /* (size, requested) */
        let mut off = 0;
        while off + HEADER_SIZE <= HEAP_SIZE {
            let size = self.read_size(off);
            if size == 0 { break; }
            if !self.read_free(off) {
                used.push((size, self.read_requested(off)));
            }
            off = self.next_off(off);
        }

        /* Rebuild */
        self.data = vec![0u8; HEAP_SIZE];
        off = 0;
        for (size, req) in &used {
            self.write_header(off, *size, *req, false);
            off += HEADER_SIZE + size;
        }
        if off + HEADER_SIZE <= HEAP_SIZE {
            self.write_header(off, HEAP_SIZE - off - HEADER_SIZE, 0, true);
        }
    }
}

#[derive(Default)]
struct Stats {
    free_blocks: usize,
    free_bytes: usize,
    largest_free: usize,
    used_blocks: usize,
    used_bytes: usize,
    requested_bytes: usize,
    header_overhead: usize,
}

impl Stats {
    fn f_ext(&self) -> f64 {
        if self.free_bytes > 0 {
            1.0 - self.largest_free as f64 / self.free_bytes as f64
        } else {
            0.0
        }
    }

    fn f_int(&self) -> f64 {
        if self.used_bytes > 0 {
            (self.used_bytes - self.requested_bytes) as f64
                / self.used_bytes as f64
        } else {
            0.0
        }
    }

    fn utilization(&self) -> f64 {
        self.requested_bytes as f64 / HEAP_SIZE as f64
    }

    fn print(&self) {
        println!("    Free: {} blocks, {} bytes (largest={})",
                 self.free_blocks, self.free_bytes, self.largest_free);
        println!("    Used: {} blocks, {} bytes (requested={})",
                 self.used_blocks, self.used_bytes, self.requested_bytes);
        println!("    Header overhead: {} bytes", self.header_overhead);
        println!("    F_ext = {:.3}, F_int = {:.3}, Util = {:.3}",
                 self.f_ext(), self.f_int(), self.utilization());
    }
}

/* =================== DEMOS ========================================= */

fn demo1_internal_fragmentation() {
    println!("=== Demo 1: Internal Fragmentation ===\n");

    let mut h = SimHeap::new();
    let sizes = [1, 3, 5, 7, 10, 13, 15, 17, 25, 31];

    println!("  {:>9}  {:>7}  {:>5}  {:>6}", "Requested", "Aligned", "Waste", "F_int");

    let mut total_req: usize = 0;
    let mut total_alloc: usize = 0;

    for &sz in &sizes {
        let aligned = align_up(sz);
        let waste = aligned - sz;
        let f = waste as f64 / aligned as f64;
        println!("  {:>9}  {:>7}  {:>5}  {:>6.3}", sz, aligned, waste, f);
        total_req += sz;
        total_alloc += aligned;
        h.alloc(sz);
    }

    println!("\n  Total: requested={}, allocated={}", total_req, total_alloc);
    println!("  Overall internal frag: {:.1}%",
             100.0 * (total_alloc - total_req) as f64 / total_alloc as f64);
    println!("  Header overhead: {} bytes/block = {} total\n",
             HEADER_SIZE, HEADER_SIZE * sizes.len());
}

fn demo2_external_fragmentation() {
    println!("=== Demo 2: External Fragmentation ===\n");

    let mut h = SimHeap::new();
    let mut blocks = Vec::new();

    for _ in 0..8 {
        blocks.push(h.alloc(100).unwrap());
    }

    println!("After 8 x alloc(100):");
    h.stats().print();

    /* Free every other block without coalescing */
    for i in (0..8).step_by(2) {
        h.free_no_coalesce(blocks[i]);
    }

    println!("\nAfter freeing 0,2,4,6 (no coalescing):");
    h.dump("checkerboard");
    h.stats().print();

    let big = h.alloc(200);
    println!("\n  alloc(200): {}",
             if big.is_some() { "SUCCESS" } else { "FAILED (external frag)" });
    println!("  ~400 bytes free but no contiguous block >= 200.\n");
}

fn demo3_coalescing_effect() {
    println!("=== Demo 3: Coalescing vs No Coalescing ===\n");

    /* Without coalescing */
    let mut h1 = SimHeap::new();
    let a = h1.alloc(100).unwrap();
    let b = h1.alloc(100).unwrap();
    let c = h1.alloc(100).unwrap();
    h1.free_no_coalesce(a);
    h1.free_no_coalesce(b);
    h1.free_no_coalesce(c);

    println!("Without coalescing:");
    h1.dump("3 separate free blocks");
    let big1 = h1.alloc(250);
    println!("  alloc(250): {}", if big1.is_some() { "SUCCESS" } else { "FAILED" });
    println!("  F_ext = {:.3}\n", h1.stats().f_ext());

    /* With coalescing */
    let mut h2 = SimHeap::new();
    let a = h2.alloc(100).unwrap();
    let b = h2.alloc(100).unwrap();
    let c = h2.alloc(100).unwrap();
    h2.free_coalesce(a);
    h2.free_coalesce(b);
    h2.free_coalesce(c);

    println!("With coalescing:");
    h2.dump("one merged block");
    let big2 = h2.alloc(250);
    println!("  alloc(250): {}", if big2.is_some() { "SUCCESS" } else { "FAILED" });
    println!("  F_ext = {:.3}\n", h2.stats().f_ext());
}

fn demo4_aging_pattern() {
    println!("=== Demo 4: Aging Pattern ===\n");

    let mut h = SimHeap::new();
    let mut persistent = Vec::new();
    let mut ephemeral = Vec::new();

    for _ in 0..5 {
        persistent.push(h.alloc(80).unwrap());
        ephemeral.push(h.alloc(60).unwrap());
    }

    println!("After interleaved alloc (80 persistent, 60 ephemeral):");
    h.stats().print();

    for &ptr in &ephemeral {
        h.free_coalesce(ptr);
    }

    println!("\nAfter freeing all ephemeral:");
    h.dump("holes between persistent blocks");
    h.stats().print();

    let big = h.alloc(200);
    println!("\n  alloc(200): {}",
             if big.is_some() { "SUCCESS (from remainder)" }
             else { "FAILED" });
    println!("  60-byte holes can't serve 200-byte request.\n");
}

fn demo5_compaction() {
    println!("=== Demo 5: Compaction Simulation ===\n");

    let mut h = SimHeap::new();

    let _a = h.alloc(80).unwrap();
    let b = h.alloc(120).unwrap();
    let _c = h.alloc(60).unwrap();
    let d = h.alloc(100).unwrap();

    h.free_coalesce(b);
    h.free_coalesce(d);

    println!("Before compaction:");
    h.dump("holes between used");
    h.stats().print();

    h.compact();

    println!("\nAfter compaction:");
    h.dump("all used packed, one large free");
    h.stats().print();

    println!("\n  F_ext went from >0 to 0.000.");
    println!("  In C, real compaction is impossible (raw pointers).");
    println!("  Use handles or GC languages for true compaction.\n");
}

fn demo6_size_classes() {
    println!("=== Demo 6: Size Classes vs Raw ===\n");

    let raw_sizes = [13, 27, 5, 41, 19, 33, 9, 50, 7, 45];

    /* Raw */
    let mut h_raw = SimHeap::new();
    let mut raw_ptrs = Vec::new();
    for &sz in &raw_sizes {
        raw_ptrs.push(h_raw.alloc(sz).unwrap());
    }
    for i in (1..10).step_by(2) {
        h_raw.free_coalesce(raw_ptrs[i]);
    }

    println!("Raw allocation (varied sizes, free odd):");
    h_raw.stats().print();

    /* With size classes */
    let mut h_class = SimHeap::new();
    let mut class_ptrs = Vec::new();

    println!("\n  Size class rounding:");
    for &sz in &raw_sizes {
        let rounded = if sz <= 16 { 16 }
                      else if sz <= 32 { 32 }
                      else if sz <= 48 { 48 }
                      else { 64 };
        println!("    {:>2} -> {}", sz, rounded);
        let ptr = h_class.alloc(rounded).unwrap();
        /* Fix requested to original for accurate F_int */
        let hoff = ptr - HEADER_SIZE;
        let bsize = h_class.read_size(hoff);
        h_class.write_header(hoff, bsize, sz, false);
        class_ptrs.push(ptr);
    }

    for i in (1..10).step_by(2) {
        h_class.free_coalesce(class_ptrs[i]);
    }

    println!("\nSize-class allocation:");
    h_class.stats().print();

    println!("\n  Size classes: higher F_int, but freed blocks");
    println!("  match future requests of the same class.\n");
}

fn demo7_knuth_50_percent() {
    println!("=== Demo 7: Knuth's 50% Rule Simulation ===\n");

    let mut h = SimHeap::new();
    let mut live: Vec<usize> = Vec::new();
    let mut rng_state: u64 = 12345;

    /* Simple LCG for deterministic "randomness" */
    let mut next_rand = |state: &mut u64| -> u64 {
        *state = state.wrapping_mul(6364136223846793005).wrapping_add(1);
        *state >> 33
    };

    /* Phase: reach steady state */
    for _ in 0..100 {
        let size = (next_rand(&mut rng_state) % 60 + 10) as usize;
        if let Some(ptr) = h.alloc(size) {
            live.push(ptr);
        }

        /* Free with ~50% probability when we have enough blocks */
        if live.len() > 5 && next_rand(&mut rng_state) % 2 == 0 {
            let idx = (next_rand(&mut rng_state) as usize) % live.len();
            h.free_coalesce(live[idx]);
            live.swap_remove(idx);
        }
    }

    let s = h.stats();
    let ratio = if s.used_blocks > 0 {
        s.free_blocks as f64 / s.used_blocks as f64
    } else {
        0.0
    };

    println!("After 100 random alloc/free operations:");
    println!("  Used blocks: {}", s.used_blocks);
    println!("  Free blocks: {}", s.free_blocks);
    println!("  Ratio free/used: {:.2} (Knuth predicts ~0.50)", ratio);
    s.print();
    println!();
}

fn demo8_fragmentation_over_time() {
    println!("=== Demo 8: Fragmentation Over Time ===\n");

    let mut h = SimHeap::new();
    let mut live: Vec<usize> = Vec::new();
    let mut rng_state: u64 = 67890;

    let mut next_rand = |state: &mut u64| -> u64 {
        *state = state.wrapping_mul(6364136223846793005).wrapping_add(1);
        *state >> 33
    };

    println!("  {:>5}  {:>5}  {:>5}  {:>5}  {:>5}  {:>6}  {:>6}",
             "Step", "Used", "Free", "FrBlk", "LgFr", "F_ext", "Util");

    for step in 1..=10 {
        /* Allocate 10 blocks */
        for _ in 0..10 {
            let size = (next_rand(&mut rng_state) % 40 + 8) as usize;
            if let Some(ptr) = h.alloc(size) {
                live.push(ptr);
            }
        }

        /* Free ~5 random blocks */
        for _ in 0..5 {
            if live.is_empty() { break; }
            let idx = (next_rand(&mut rng_state) as usize) % live.len();
            h.free_coalesce(live[idx]);
            live.swap_remove(idx);
        }

        let s = h.stats();
        println!("  {:>5}  {:>5}  {:>5}  {:>5}  {:>5}  {:>6.3}  {:>6.3}",
                 step * 10, s.used_bytes, s.free_bytes,
                 s.free_blocks, s.largest_free,
                 s.f_ext(), s.utilization());
    }

    println!("\n  F_ext tends to stabilize as the heap reaches steady state.");
    println!("  Utilization grows as more blocks accumulate.\n");
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_internal_fragmentation();
    demo2_external_fragmentation();
    demo3_coalescing_effect();
    demo4_aging_pattern();
    demo5_compaction();
    demo6_size_classes();
    demo7_knuth_50_percent();
    demo8_fragmentation_over_time();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Calcular metricas

Dado el siguiente estado del heap (header = 16 bytes):

```
[USED:64 (req=50)] [FREE:32] [USED:128 (req=100)] [FREE:48] [FREE:80]
```

Calcula $F_{\text{ext}}$, $F_{\text{int}}$ y la utilizacion ($U$) sobre
un heap de 1024 bytes.

<details><summary>Prediccion</summary>

**Bytes libres**: 32 + 48 + 80 = 160. Mayor bloque libre = 80.

$$F_{\text{ext}} = 1 - \frac{80}{160} = 1 - 0.5 = 0.500$$

**Bytes asignados** (payload): 64 + 128 = 192.
**Bytes solicitados**: 50 + 100 = 150.

$$F_{\text{int}} = \frac{192 - 150}{192} = \frac{42}{192} = 0.219$$

**Header overhead**: 5 bloques $\times$ 16 = 80 bytes.
**Espacio total consumido**: 192 (usado) + 160 (libre) + 80 (headers) = 432.

$$U = \frac{150}{1024} = 0.146$$

Solo el 14.6% del heap esta realmente en uso util. El resto es:
- Libre: 160 bytes (15.6%)
- Fragmentacion interna: 42 bytes (4.1%)
- Headers: 80 bytes (7.8%)
- Heap no alcanzado: 1024 - 432 = 592 bytes (57.8%)

**Nota**: el heap no alcanzado seria otro bloque libre si estuviera
inicializado, pero en este ejemplo solo mostramos 5 bloques.

</details>

### Ejercicio 2 — Checkerboard

Describe una secuencia minima de malloc/free que cree un patron
**checkerboard** (alternancia USED-FREE-USED-FREE...) con 4 huecos.
Usa tamano fijo de 64 bytes.

<details><summary>Prediccion</summary>

```
1. a = malloc(64)
2. b = malloc(64)
3. c = malloc(64)
4. d = malloc(64)
5. e = malloc(64)
6. f = malloc(64)
7. g = malloc(64)
8. h = malloc(64)

Estado: [a:64][b:64][c:64][d:64][e:64][f:64][g:64][h:64][FREE:...]

9.  free(b)
10. free(d)
11. free(f)
12. free(h)

Estado: [a:USED][FREE:64][c:USED][FREE:64][e:USED][FREE:64][g:USED][FREE:64+rest]
```

4 huecos de 64 bytes (el ultimo fusionado con el sobrante si hay coalescing).
Total libre = $4 \times 64 = 256$ + sobrante. Pero el mayor bloque es 64
(sin contar el sobrante). Cualquier peticion > 64 falla a pesar de tener
256+ bytes libres.

Secuencia minima para 4 huecos: 8 malloc + 4 free = **12 operaciones**.

</details>

### Ejercicio 3 — Coalescing parcial

Estado del heap:

```
[USED:A] [FREE:40] [USED:B] [FREE:60] [USED:C] [FREE:100]
```

Se ejecuta `free(B)`. Muestra el estado del heap con coalescing inmediato.
Cuanto mide el bloque fusionado resultante? (Header = 16 bytes.)

<details><summary>Prediccion</summary>

Al liberar B, se marca como FREE y se intenta coalescing con vecinos:

- **Vecino izquierdo**: [FREE:40] — es FREE. Fusionar.
- **Vecino derecho**: [FREE:60] — es FREE. Fusionar.

El bloque resultante absorbe los headers de B y del bloque de 60:

$$\text{tamano} = 40 + 16_{\text{header B}} + \text{size}(B) + 16_{\text{header 60}} + 60$$

Si B tiene payload de, digamos, 80 bytes:

$$\text{tamano} = 40 + 16 + 80 + 16 + 60 = 212 \text{ bytes}$$

El heap queda:

```
[USED:A] [FREE:212] [USED:C] [FREE:100]
```

Tres bloques libres (40, 60, y entre ellos) se consolidaron en uno de 212.
$F_{\text{ext}}$ bajo de 3 bloques libres a 2, y el mayor bloque paso de
100 a 212.

Si B = 80: sin coalescing, `malloc(150)` fallaria (mayor bloque = 100).
Con coalescing, `malloc(150)` usa el bloque de 212. **Exito**.

</details>

### Ejercicio 4 — Regla del 50%

Un programa tiene 200 bloques usados en estado estable. Segun la regla
del 50% de Knuth:

1. Cuantos bloques libres habra aproximadamente?
2. Si el tamano promedio de bloque libre es 50 bytes, cuanta memoria
   libre total hay?
3. Esto es un problema?

<details><summary>Prediccion</summary>

1. **Bloques libres** $\approx 200/2 = 100$ bloques.

2. **Memoria libre total** $= 100 \times 50 = 5000$ bytes.

3. **Depende del patron de uso**:
   - Si las peticiones futuras son $\leq 50$ bytes: los 100 huecos
     pueden satisfacerlas. No hay problema.
   - Si se necesita un bloque de 500 bytes: ninguno de los 100 huecos
     es suficiente. Fragmentacion externa severa.

   La regla del 50% dice que **la fragmentacion es inherente**: siempre
   habra huecos intercalados. Lo que importa es la **distribucion de
   tamanos** de esos huecos vs la distribucion de peticiones futuras.

   Con 200 bloques usados y 100 libres, hay 300 bloques totales. Los
   headers consumen $300 \times 16 = 4800$ bytes de overhead. Si el heap
   es de 32 KB, eso es el 14.6% del heap solo en metadata.

</details>

### Ejercicio 5 — Compactacion con handles

Disena una tabla de handles para 8 objetos. Muestra como una compactacion
actualiza la tabla sin afectar al codigo usuario.

<details><summary>Prediccion</summary>

```
Handle table (array de punteros):
  [0] -> 0x100   (obj A, 32 bytes)
  [1] -> 0x140   (FREE, was 48 bytes)
  [2] -> 0x180   (obj C, 64 bytes)
  [3] -> 0x1E0   (FREE, was 32 bytes)
  [4] -> 0x210   (obj E, 16 bytes)

Codigo usuario:
  Handle h = 2;
  data = read(h);  // resolve: handle_table[2] = 0x180
```

Compactacion — mover objetos para eliminar huecos:
1. A (32 bytes) queda en 0x100 (no se mueve).
2. C (64 bytes) se mueve de 0x180 a 0x120 (justo despues de A).
3. E (16 bytes) se mueve de 0x210 a 0x160 (justo despues de C).

Actualizar tabla:
```
  [0] -> 0x100   (sin cambio)
  [1] -> NULL     (liberado)
  [2] -> 0x120   (actualizado: 0x180 -> 0x120)
  [3] -> NULL     (liberado)
  [4] -> 0x160   (actualizado: 0x210 -> 0x160)
```

El codigo usuario sigue usando `h = 2`. `resolve(h)` ahora retorna 0x120
en lugar de 0x180, pero esto es transparente. El contenido del objeto C
fue copiado de 0x180 a 0x120 durante la compactacion.

**Costo**: una indirecion extra (leer la tabla) en cada acceso, y el
tiempo de copia durante compactacion ($O(n)$ donde $n$ es el tamano total
de objetos vivos).

</details>

### Ejercicio 6 — Size classes optimales

Para peticiones uniformemente distribuidas entre 1 y 256 bytes, compara
la fragmentacion interna promedio con:

1. Clases de potencias de 2: {8, 16, 32, 64, 128, 256}.
2. Clases lineales con paso 32: {32, 64, 96, 128, 160, 192, 224, 256}.

<details><summary>Prediccion</summary>

**Potencias de 2** — {8, 16, 32, 64, 128, 256}:

Para una peticion de tamano $n$, se redondea a la menor potencia de 2
$\geq n$. El desperdicio promedio por clase:
- $n \in [1,8]$: promedio $\bar{n} = 4.5$, clase = 8, waste = 3.5.
- $n \in [9,16]$: $\bar{n} = 12.5$, clase = 16, waste = 3.5.
- $n \in [17,32]$: $\bar{n} = 24.5$, clase = 32, waste = 7.5.
- $n \in [33,64]$: $\bar{n} = 48.5$, clase = 64, waste = 15.5.
- $n \in [65,128]$: $\bar{n} = 96.5$, clase = 128, waste = 31.5.
- $n \in [129,256]$: $\bar{n} = 192.5$, clase = 256, waste = 63.5.

Desperdicio promedio ponderado $\approx \frac{\sum (\text{waste}_i \times \text{rango}_i)}{\sum \text{rango}_i}$:
$= \frac{3.5 \times 8 + 3.5 \times 8 + 7.5 \times 16 + 15.5 \times 32 + 31.5 \times 64 + 63.5 \times 128}{256}$
$= \frac{28 + 28 + 120 + 496 + 2016 + 8128}{256} = \frac{10816}{256} \approx 42.3$ bytes.

$F_{\text{int}} \approx 42.3 / (42.3 + 128.5) \approx 24.8\%$.

**Lineales con paso 32** — {32, 64, 96, 128, 160, 192, 224, 256}:

Cada clase cubre un rango de 32. Waste maximo por clase = 31.
Waste promedio por clase = 15.5.

$F_{\text{int}} \approx 15.5 / (15.5 + 128.5) \approx 10.8\%$.

**Resultado**: las clases lineales producen **menos fragmentacion interna**
(10.8% vs 24.8%) pero necesitan **mas bins** (8 vs 6). Los allocators reales
usan un hibrido: spacing fino para tamanos pequenos (donde el overhead
relativo es alto) y spacing exponencial para tamanos grandes.

</details>

### Ejercicio 7 — Coalescing diferido

Explica por que ptmalloc2 **no** coalesce los fastbins inmediatamente.
Cuando se activa el coalescing? Que tradeoff implica?

<details><summary>Prediccion</summary>

**Fastbins** en ptmalloc2: listas LIFO de bloques pequenos ($\leq 80$ bytes
por defecto, ajustable). Cada fastbin contiene bloques de un **unico tamano**.

**Por que no coalescer**:
1. **Velocidad**: free de un fastbin es simplemente push a una lista LIFO
   ($O(1)$, sin verificar vecinos). El coalescing requiere leer headers
   vecinos.
2. **Reutilizacion rapida**: los programas tipicos liberan y re-alocan
   bloques del mismo tamano. Sin coalescing, el bloque se reutiliza
   exactamente como esta — sin split.
3. **Localidad temporal**: un bloque recien liberado probablemente sera
   re-alocado pronto. Coalescerlo y luego volver a dividirlo es trabajo
   desperdiciado.

**Cuando se coalesce** (malloc_consolidate):
- Cuando `malloc` no encuentra un bloque adecuado en los bins normales.
- Cuando se solicita un bloque grande (> fastbin max).
- Cuando `malloc_trim` o `mallopt` fuerzan consolidacion.
- Periodicamente cuando el heap crece demasiado.

**Tradeoff**:
- **Pro**: alloc/free de bloques pequenos es mucho mas rapido.
- **Contra**: fragmentacion temporal. 10 bloques de 16 bytes libres ocupan
  $10 \times (16 + 16) = 320$ bytes, cuando coalescidos serian un bloque de
  $16 \times 10 + 16 \times 9 = 304$ bytes (9 headers absorbidos).
  Peticiones de 100 bytes fallan mientras haya 10 fastbins de 16.

</details>

### Ejercicio 8 — Patron adverso

Un servidor procesa peticiones HTTP. Cada peticion aloca:
- Header buffer: 256 bytes (liberado al final de la peticion).
- Body buffer: variable, 100-10000 bytes (liberado al final).
- Log entry: 64 bytes (**nunca liberado**, crece indefinidamente).

Describe la fragmentacion que se produce despues de 10000 peticiones.
Propone una solucion.

<details><summary>Prediccion</summary>

**Patron despues de muchas peticiones**:

El heap se llena de log entries de 64 bytes intercalados con huecos de
tamano variable (donde estaban los headers y bodies):

```
[LOG:64][FREE:256+var][LOG:64][FREE:256+var][LOG:64]...
```

**Fragmentacion**:
- **Externa**: los huecos estan separados por log entries. Si los bodies
  variaban entre 100 y 10000, los huecos tienen tamanos variados. Peticiones
  con bodies grandes pueden no encontrar un hueco contiguo.
- **Efecto acumulativo**: los log entries **nunca se liberan**, asi que los
  huecos **nunca se pueden coalescer**. La fragmentacion es permanente e
  irreversible.
- RSS crece continuamente, aun cuando la memoria "en uso" es estable.

**Solucion**:
1. **Arena allocator por peticion**: alocar header y body de una arena que
   se resetea al final de cada peticion. Cero fragmentacion para datos
   efimeros.
2. **Pool allocator para logs**: todos los logs son de 64 bytes; un pool
   los sirve sin fragmentacion y con $O(1)$ alloc/free.
3. **Separar heaps**: logs en una region, datos de peticion en otra. Asi
   los logs no interrumpen el coalescing de datos temporales.

</details>

### Ejercicio 9 — Rust y fragmentacion

El borrow checker de Rust previene use-after-free y double free. Previene
la fragmentacion? Por que si o por que no?

<details><summary>Prediccion</summary>

**No, Rust NO previene la fragmentacion.**

El borrow checker garantiza **seguridad de memoria** (cada valor tiene un
unico dueno, las referencias no sobreviven al dueno, no hay data races).
Pero no controla **cuando** ni **en que orden** se alocan y liberan los
bloques.

Un programa Rust que hace:
```rust
let mut v: Vec<Box<[u8]>> = Vec::new();
for _ in 0..1000 {
    v.push(vec![0u8; 100].into_boxed_slice());
}
// Free every other
for i in (0..1000).step_by(2) {
    v[i] = vec![].into_boxed_slice();
}
```

produce exactamente la misma fragmentacion que el equivalente en C. El
allocator subyacente (jemalloc, system malloc) sufre los mismos problemas.

**Lo que Rust si previene**:
- **Memory leaks por olvido**: Drop se llama automaticamente (pero leaks
  intencionales con `mem::forget` o `Rc` cycles son posibles).
- **Use-after-free**: la memoria se reutiliza de forma segura.
- **Double free**: imposible en safe Rust.

**Mitigar fragmentacion en Rust**: usar los mismos patrones que en C:
arena allocators (`bumpalo`), pool allocators (`typed-arena`), o
reemplazar el global allocator por jemalloc/mimalloc.

</details>

### Ejercicio 10 — Disena una metrica

La metrica $F_{\text{ext}} = 1 - \text{mayor}/\text{total}$ no distingue
entre "2 bloques libres grandes" y "100 bloques libres diminutos" si el
mayor es el mismo. Propone una metrica alternativa que capture la
**dispersion** de los bloques libres.

<details><summary>Prediccion</summary>

**Metrica propuesta: Indice de dispersion de Gini de la free list.**

El **coeficiente de Gini** mide la desigualdad en una distribucion. Para
bloques libres de tamanos $s_1, s_2, \ldots, s_k$ (ordenados):

$$G = \frac{\sum_{i=1}^{k} \sum_{j=1}^{k} |s_i - s_j|}{2k \sum_{i=1}^{k} s_i}$$

- $G = 0$: todos los bloques libres son del mismo tamano (buena
  distribucion).
- $G \to 1$: un bloque tiene casi toda la memoria libre (muy desigual).

**Ejemplo 1**: bloques {100, 100, 100}. $G = 0$. Uniforme.

**Ejemplo 2**: bloques {1, 1, 298}. $G \approx 0.66$. Un bloque domina.

**Ejemplo 3**: bloques {1, 1, 1, ..., 1} (100 bloques de 1).
$G = 0$. Uniforme, pero todos son diminutos.

**Limitacion**: Gini no captura si los bloques son utiles. Complementar
con:

$$F_{\text{usable}}(n) = \frac{\text{bloques con tamano} \geq n}{\text{total bloques libres}}$$

Esta metrica parametrica dice que fraccion de los bloques libres pueden
servir una peticion de tamano $n$. Si $F_{\text{usable}}(100) = 0.05$,
solo el 5% de los huecos sirven para peticiones de 100 bytes.

**Combinacion practica**: reportar $F_{\text{ext}}$ (vision global),
$G$ (dispersion), y $F_{\text{usable}}(n)$ para los tamanos de peticion
mas frecuentes del programa.

</details>
