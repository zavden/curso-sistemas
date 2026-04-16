# T04 — Buddy system

## Objetivo

Comprender el **buddy system** como estrategia de gestion de memoria que
divide y fusiona bloques de **potencias de 2**, implementar un buddy
allocator simplificado en C y Rust, y analizar sus tradeoffs frente a los
allocators de free list vistos en T01-T03.

---

## 1. Motivacion

Los allocators de free list (first fit, best fit, etc.) tienen dos
debilidades estructurales:

1. **Coalescing costoso**: encontrar el bloque vecino anterior requiere
   un scan lineal o boundary tags (overhead de espacio).
2. **Splitting arbitrario**: dividir un bloque de 1000 en uno de 37 y
   otro de 963 dificulta la reutilizacion — el residuo rara vez coincide
   con una peticion futura.

El buddy system resuelve ambos problemas imponiendo una **restriccion**:
todos los bloques tienen tamano potencia de 2, y cada bloque tiene un
unico **buddy** (companero) cuya direccion se calcula con una operacion
de bits en $O(1)$.

---

## 2. Estructura del buddy system

### 2.1 Invariantes

Sea el heap de tamano $2^N$ bytes. El buddy system mantiene:

1. Cada bloque tiene tamano $2^k$ para algun $k \in [\text{MIN\_ORDER}, N]$.
2. Un bloque de tamano $2^k$ esta alineado a $2^k$ (su offset desde el
   inicio del heap es multiplo de $2^k$).
3. Cada bloque de tamano $2^k$ tiene exactamente un **buddy**: otro bloque
   de tamano $2^k$ adyacente, tal que juntos forman un bloque de
   $2^{k+1}$.

### 2.2 Calculo del buddy

Dado un bloque en offset $a$ con tamano $2^k$, su buddy esta en:

$$\text{buddy}(a, k) = a \oplus 2^k$$

donde $\oplus$ es XOR. Esta formula funciona porque:

- Si $a$ es el bloque "izquierdo" (bit $k$ es 0), XOR con $2^k$ enciende
  ese bit → buddy = $a + 2^k$.
- Si $a$ es el bloque "derecho" (bit $k$ es 1), XOR con $2^k$ apaga ese
  bit → buddy = $a - 2^k$.

Ejemplo: bloque en offset 192 ($= 0b11000000$), tamano $2^6 = 64$:

$$\text{buddy} = 192 \oplus 64 = 0b11000000 \oplus 0b01000000 = 0b10000000 = 128$$

El buddy esta en offset 128. Juntos, 128 y 192 forman un bloque de 128
bytes alineado a 128 (offset 128).

### 2.3 Free lists por orden

El buddy system mantiene una **free list por cada orden** $k$:

```
free_list[MIN_ORDER]:  bloques libres de 2^MIN_ORDER bytes
free_list[MIN_ORDER+1]: bloques libres de 2^(MIN_ORDER+1) bytes
...
free_list[N]:          bloque libre de 2^N bytes (todo el heap)
```

Inicialmente, solo `free_list[N]` tiene un bloque (el heap completo).

---

## 3. Operaciones

### 3.1 Allocacion (malloc)

Para una peticion de $n$ bytes:

1. Calcular el orden necesario: $k = \lceil \log_2(\max(n, 2^{\text{MIN\_ORDER}})) \rceil$.
2. Buscar un bloque libre en `free_list[k]`.
3. Si `free_list[k]` esta vacia:
   a. Buscar en `free_list[k+1]`, `free_list[k+2]`, ... hasta encontrar
      un bloque.
   b. **Dividir** recursivamente: un bloque de $2^j$ se divide en dos
      buddies de $2^{j-1}$. Uno se devuelve a `free_list[j-1]`; el otro
      se sigue dividiendo (si $j-1 > k$) o se retorna al usuario (si
      $j-1 = k$).
4. Marcar el bloque como usado y retornarlo.

```
buddy_alloc(n):
    k = ceil_log2(max(n, MIN_SIZE))
    for j = k to MAX_ORDER:
        if free_list[j] is not empty:
            block = remove from free_list[j]
            while j > k:
                j -= 1
                // Split: the block becomes two buddies of 2^j
                buddy = block + 2^j
                add buddy to free_list[j]
            mark block as used
            return block
    return NULL  // out of memory
```

### 3.2 Liberacion (free)

Para liberar un bloque en offset $a$ con orden $k$:

1. Calcular el buddy: $b = a \oplus 2^k$.
2. Si el buddy esta **libre** y tiene orden $k$:
   a. Remover el buddy de `free_list[k]`.
   b. El bloque fusionado es $\min(a, b)$ con orden $k+1$.
   c. **Repetir** con el bloque fusionado (intentar fusionar con su buddy
      de orden $k+1$).
3. Si el buddy esta **usado** (o tiene un orden diferente): insertar el
   bloque en `free_list[k]`.

```
buddy_free(block, k):
    while k < MAX_ORDER:
        buddy = block XOR 2^k
        if buddy is free and buddy.order == k:
            remove buddy from free_list[k]
            block = min(block, buddy)
            k += 1
        else:
            break
    add block to free_list[k]
```

### 3.3 Ejemplo visual

Heap de $2^5 = 32$ bytes (MIN_ORDER = 2, es decir bloques minimos de 4).

```
Estado inicial:
free_list[5]: [0..31]     (un bloque de 32)
free_list[4]: (vacia)
free_list[3]: (vacia)
free_list[2]: (vacia)

alloc(3):  -> orden 2 (ceil_log2(3) = 2, 2^2 = 4)
  free_list[5]: [0..31] -> split -> [0..15] + [16..31]
  free_list[4]: [16..31]
  free_list[3]: (vacia)
  [0..15] -> split -> [0..7] + [8..15]
  free_list[3]: [8..15]
  [0..7] -> split -> [0..3] + [4..7]
  free_list[2]: [4..7]
  Retorna [0..3] (4 bytes, 1 byte de frag interna)

Estado:
  [USED:0..3] [FREE:4..7] [FREE:8..15] [FREE:16..31]
  free_list[2]: [4..7]
  free_list[3]: [8..15]
  free_list[4]: [16..31]

alloc(5):  -> orden 3 (ceil_log2(5) = 3, 2^3 = 8)
  free_list[3]: [8..15] -> Retorna [8..15]

Estado:
  [USED:0..3] [FREE:4..7] [USED:8..15] [FREE:16..31]
  free_list[2]: [4..7]
  free_list[4]: [16..31]

free(0..3):  -> orden 2
  buddy(0, 2) = 0 XOR 4 = 4. ¿[4..7] libre? Si, orden 2.
  Fusionar -> [0..7], orden 3.
  buddy(0, 3) = 0 XOR 8 = 8. ¿[8..15] libre? No (USED).
  Insertar [0..7] en free_list[3].

Estado:
  [FREE:0..7] [USED:8..15] [FREE:16..31]
  free_list[3]: [0..7]
  free_list[4]: [16..31]

free(8..15):  -> orden 3
  buddy(8, 3) = 8 XOR 8 = 0. ¿[0..7] libre? Si, orden 3.
  Fusionar -> [0..15], orden 4.
  buddy(0, 4) = 0 XOR 16 = 16. ¿[16..31] libre? Si, orden 4.
  Fusionar -> [0..31], orden 5.
  Insertar [0..31] en free_list[5].

Estado final: heap completo libre. Identico al inicio.
```

---

## 4. Analisis de complejidad

### 4.1 Tiempo

| Operacion | Complejidad | Razon |
|-----------|:-----------:|-------|
| alloc | $O(N)$ | Como maximo $N$ splits (desde $2^N$ hasta $2^k$) |
| free | $O(N)$ | Como maximo $N$ merges (desde $2^k$ hasta $2^N$) |
| Calculo de buddy | $O(1)$ | Una operacion XOR |

Donde $N = \log_2(\text{tamano\_heap})$. Para un heap de 1 GB ($2^{30}$),
$N = 30$, asi que alloc y free son $O(30)$ — efectivamente $O(1)$ en la
practica.

### 4.2 Fragmentacion interna

El buddy system redondea toda peticion a la potencia de 2 mas cercana.
Para una peticion de tamano $n$ donde $2^{k-1} < n \leq 2^k$:

$$\text{desperdicio} = 2^k - n$$

El peor caso ocurre justo por encima de una potencia de 2:
$n = 2^{k-1} + 1 \Rightarrow$ desperdicio $= 2^k - 2^{k-1} - 1 = 2^{k-1} - 1$.

Para $n = 33$: se asigna $2^6 = 64$, desperdicio = 31 bytes (**48%**).

En promedio, para peticiones uniformes entre $2^{k-1}+1$ y $2^k$:
$$\bar{w} = 2^k - \frac{2^{k-1}+1 + 2^k}{2} = 2^k - \frac{3 \cdot 2^{k-1} + 1}{2} = \frac{2^{k-1} - 1}{2} \approx \frac{2^{k-1}}{2}$$

La fragmentacion interna promedio es aproximadamente **25%** del tamano
del bloque. Esto es significativamente peor que un allocator con
alineamiento a 8 bytes.

### 4.3 Fragmentacion externa

El buddy system tiene **excelente** resistencia a la fragmentacion externa
porque:

1. Los bloques solo se dividen en mitades exactas → los tamanos son
   predecibles.
2. La fusion es determinista y completa: si el buddy esta libre, **siempre**
   se fusionan.
3. No hay *slivers* ni residuos de tamano arbitrario.

Sin embargo, puede ocurrir que un buddy no se pueda fusionar porque esta
en uso, incluso si el otro buddy y todos los demas estan libres. Un solo
bloque usado puede **bloquear** la fusion de toda una jerarquia.

---

## 5. Estructura de datos: bitmap o array de estado

### 5.1 Bitmap por nivel

Para cada nivel $k$, un **bitmap** indica que bloques estan disponibles.
El heap de $2^N$ tiene $2^{N-k}$ bloques posibles de orden $k$, asi que
el bitmap de nivel $k$ tiene $2^{N-k}$ bits.

Total de bits: $\sum_{k=0}^{N} 2^{N-k} = 2^{N+1} - 1$ bits $\approx$
$2^{N+1}$ bits $= 2^{N-2}$ bytes.

Para un heap de 1 MB ($2^{20}$): $2^{18}$ bytes = 256 KB de bitmaps.
Eso es el **25%** del heap — demasiado para heaps pequenos.

### 5.2 Bitmap XOR (optimizacion)

En lugar de un bit por bloque, usar un bit por **par de buddies**. El
bit indica si los dos buddies tienen **estados diferentes** (uno libre,
otro usado). Esto reduce el espacio a $2^{N-k-1}$ bits por nivel.

Cuando se aloca o libera un bloque, se hace XOR del bit correspondiente.
Si el bit resultante es 0, ambos buddies estan en el mismo estado →
se puede fusionar (si ambos libres) o no (si ambos usados).

### 5.3 Array de ordenes

Alternativa mas simple: un **array** donde cada entrada almacena el orden
del bloque que empieza en esa posicion. Solo las posiciones alineadas a
$2^{\text{MIN\_ORDER}}$ tienen entradas.

```
orders[i] = k  si hay un bloque de orden k empezando en offset i * MIN_SIZE
           -1  si esta posicion no es inicio de bloque
```

---

## 6. Variantes y uso en la practica

### 6.1 Kernel de Linux

El kernel de Linux usa un buddy system para gestionar **paginas fisicas**
de memoria. Los ordenes van de 0 (1 pagina = 4 KB) a 10 (1024 paginas =
4 MB). El archivo `/proc/buddyinfo` muestra el estado:

```
Node 0, zone   Normal  4   3   2   1   0   1   0   0   1   0   1
```

Cada numero es la cantidad de bloques libres de ese orden (0 = 4KB,
1 = 8KB, ..., 10 = 4MB).

### 6.2 Buddy system con slabs

El kernel complementa el buddy system con **slab allocators** para objetos
de tamano fijo (inodes, dentries, etc.). El buddy system asigna paginas
al slab allocator, que las subdivide en objetos de tamano fijo.

```
Buddy system: gestiona paginas (4KB, 8KB, ...)
    ↓ entrega paginas a
Slab allocator: gestiona objetos (inode: 600 bytes, dentry: 192 bytes, ...)
```

Esto combina lo mejor de ambos mundos:
- Buddy: fusion rapida, sin fragmentacion externa significativa.
- Slab: sin fragmentacion interna para objetos de tamano conocido.

### 6.3 Variante: weighted buddy

El buddy system clasico solo permite potencias de 2, desperdiciando hasta
el 50%. El **weighted buddy** permite divisiones en proporciones 1:3
ademas de 1:1, generando tamanos como $2^k$ y $3 \cdot 2^{k-2}$. Esto
reduce la fragmentacion interna pero complica la fusion.

### 6.4 Variante: Fibonacci buddy

Usa tamanos de la serie de Fibonacci en lugar de potencias de 2. Los
tamanos consecutivos difieren en un factor de $\phi \approx 1.618$ en
lugar de 2, lo cual reduce el desperdicio a ~23% en lugar de ~25%.
Raramente usado por su complejidad.

---

## 7. Comparacion con otros allocators

| Aspecto | Free list (first fit) | Buddy system | Pool/Slab |
|---------|:---------------------:|:------------:|:---------:|
| Frag. interna | Baja (~3-10%) | Alta (~25%) | Cero |
| Frag. externa | Variable | Baja | Cero |
| alloc | $O(k)$ free blocks | $O(\log N)$ | $O(1)$ |
| free | $O(k)$ o $O(1)$* | $O(\log N)$ | $O(1)$ |
| Coalescing | Requiere scan o tags | $O(1)$ XOR por nivel | N/A |
| Flexibilidad | Cualquier tamano | Solo potencias de 2 | Tamano fijo |

*Con boundary tags, free en free-list es $O(1)$.

El buddy system es ideal como **capa intermedia**: gestiona bloques
medianos-grandes (paginas), y los allocators de nivel superior (slab,
pool) subdividen esos bloques para objetos pequenos.

---

## 8. Programa completo en C

```c
/*
 * buddy_system.c
 *
 * Simplified buddy allocator on a static heap.
 * Block sizes are powers of 2. Splitting and merging
 * use XOR-based buddy calculation.
 *
 * Compile: gcc -O0 -o buddy_system buddy_system.c
 * Run:     ./buddy_system
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* =================== BUDDY ALLOCATOR =============================== */

/*
 * Heap size: 2^MAX_ORDER = 1024 bytes.
 * Min block: 2^MIN_ORDER = 16 bytes.
 * Orders: 4 (16), 5 (32), 6 (64), 7 (128), 8 (256), 9 (512), 10 (1024).
 */

#define MAX_ORDER  10
#define MIN_ORDER  4
#define HEAP_SIZE  (1 << MAX_ORDER)  /* 1024 */
#define MIN_SIZE   (1 << MIN_ORDER)  /* 16 */
#define NUM_ORDERS (MAX_ORDER - MIN_ORDER + 1)  /* 7 */

/* Free list node (embedded in free blocks) */
typedef struct FreeNode {
    struct FreeNode *next;
    struct FreeNode *prev;
} FreeNode;

typedef struct {
    char heap[HEAP_SIZE];
    FreeNode *free_lists[NUM_ORDERS];  /* index 0 = order MIN_ORDER */
    int8_t block_order[HEAP_SIZE / MIN_SIZE]; /* order of block at each slot */
} BuddyAllocator;

static int order_index(int order) { return order - MIN_ORDER; }
static int slot_of(BuddyAllocator *ba, void *ptr) {
    return (int)((char *)ptr - ba->heap) / MIN_SIZE;
}

void buddy_init(BuddyAllocator *ba) {
    memset(ba->heap, 0, HEAP_SIZE);
    for (int i = 0; i < NUM_ORDERS; i++)
        ba->free_lists[i] = NULL;
    for (int i = 0; i < HEAP_SIZE / MIN_SIZE; i++)
        ba->block_order[i] = -1;

    /* Entire heap is one free block of max order */
    FreeNode *node = (FreeNode *)ba->heap;
    node->next = NULL;
    node->prev = NULL;
    ba->free_lists[order_index(MAX_ORDER)] = node;
    ba->block_order[0] = MAX_ORDER;
}

static void list_remove(BuddyAllocator *ba, int oidx, FreeNode *node) {
    if (node->prev) node->prev->next = node->next;
    else ba->free_lists[oidx] = node->next;
    if (node->next) node->next->prev = node->prev;
}

static void list_insert(BuddyAllocator *ba, int oidx, FreeNode *node) {
    node->prev = NULL;
    node->next = ba->free_lists[oidx];
    if (ba->free_lists[oidx])
        ba->free_lists[oidx]->prev = node;
    ba->free_lists[oidx] = node;
}

static bool is_free(BuddyAllocator *ba, int oidx, void *ptr) {
    FreeNode *cur = ba->free_lists[oidx];
    while (cur) {
        if ((void *)cur == ptr) return true;
        cur = cur->next;
    }
    return false;
}

static int ceil_log2(int n) {
    int k = 0;
    int v = 1;
    while (v < n) { v <<= 1; k++; }
    return k;
}

void *buddy_alloc(BuddyAllocator *ba, size_t size) {
    if (size == 0) size = 1;

    int order = ceil_log2((int)size);
    if (order < MIN_ORDER) order = MIN_ORDER;
    if (order > MAX_ORDER) return NULL;

    /* Find a free block of sufficient order */
    int j;
    for (j = order; j <= MAX_ORDER; j++) {
        if (ba->free_lists[order_index(j)] != NULL)
            break;
    }
    if (j > MAX_ORDER) return NULL;

    /* Remove block from free list */
    FreeNode *block = ba->free_lists[order_index(j)];
    list_remove(ba, order_index(j), block);

    /* Split down to required order */
    while (j > order) {
        j--;
        int buddy_offset = (int)((char *)block - ba->heap) + (1 << j);
        FreeNode *buddy = (FreeNode *)(ba->heap + buddy_offset);
        buddy->next = NULL;
        buddy->prev = NULL;
        list_insert(ba, order_index(j), buddy);
        ba->block_order[buddy_offset / MIN_SIZE] = (int8_t)j;
    }

    /* Mark as used */
    int s = slot_of(ba, block);
    ba->block_order[s] = (int8_t)order;

    return (void *)block;
}

void buddy_free(BuddyAllocator *ba, void *ptr) {
    if (!ptr) return;

    int offset = (int)((char *)ptr - ba->heap);
    int s = offset / MIN_SIZE;
    int order = ba->block_order[s];
    if (order < MIN_ORDER || order > MAX_ORDER) return;

    /* Try to merge with buddy */
    while (order < MAX_ORDER) {
        int buddy_offset = offset ^ (1 << order);
        int buddy_slot = buddy_offset / MIN_SIZE;

        /* Check if buddy is free and same order */
        if (ba->block_order[buddy_slot] != order) break;
        if (!is_free(ba, order_index(order), ba->heap + buddy_offset)) break;

        /* Remove buddy from free list and merge */
        list_remove(ba, order_index(order),
                    (FreeNode *)(ba->heap + buddy_offset));

        /* Merged block starts at the lower address */
        if (buddy_offset < offset)
            offset = buddy_offset;

        ba->block_order[buddy_offset / MIN_SIZE] = -1;
        order++;
    }

    /* Insert merged block into free list */
    int ms = offset / MIN_SIZE;
    ba->block_order[ms] = (int8_t)order;
    FreeNode *node = (FreeNode *)(ba->heap + offset);
    node->next = NULL;
    node->prev = NULL;
    list_insert(ba, order_index(order), node);
}

void buddy_dump(BuddyAllocator *ba, const char *label) {
    printf("  [%s]\n", label);
    printf("    Free lists:\n");
    for (int i = 0; i < NUM_ORDERS; i++) {
        int order = i + MIN_ORDER;
        printf("      order %2d (%4d bytes):", order, 1 << order);
        FreeNode *cur = ba->free_lists[i];
        if (!cur) { printf(" (empty)\n"); continue; }
        while (cur) {
            printf(" @%d", (int)((char *)cur - ba->heap));
            cur = cur->next;
        }
        printf("\n");
    }
}

void buddy_stats(BuddyAllocator *ba) {
    size_t total_free = 0;
    int free_blocks = 0;
    size_t largest_free = 0;

    for (int i = 0; i < NUM_ORDERS; i++) {
        int order = i + MIN_ORDER;
        FreeNode *cur = ba->free_lists[i];
        while (cur) {
            free_blocks++;
            size_t sz = (size_t)(1 << order);
            total_free += sz;
            if (sz > largest_free) largest_free = sz;
            cur = cur->next;
        }
    }

    printf("    Free: %d blocks, %zu bytes (largest=%zu)\n",
           free_blocks, total_free, largest_free);
    printf("    Used: %zu bytes\n", HEAP_SIZE - total_free);
}

/* =================== DEMOS ========================================= */

void demo1_basic_alloc_free(void) {
    printf("=== Demo 1: Basic Alloc and Free ===\n\n");

    BuddyAllocator ba;
    buddy_init(&ba);
    buddy_dump(&ba, "initial: one block of 1024");

    printf("\n  alloc(100): needs order 7 (128 bytes)\n");
    void *a = buddy_alloc(&ba, 100);
    printf("    returned offset: %d\n", (int)((char *)a - ba.heap));
    buddy_dump(&ba, "after alloc(100)");

    printf("\n  alloc(50): needs order 6 (64 bytes)\n");
    void *b = buddy_alloc(&ba, 50);
    printf("    returned offset: %d\n", (int)((char *)b - ba.heap));
    buddy_dump(&ba, "after alloc(50)");

    printf("\n  free(first block at offset %d)\n",
           (int)((char *)a - ba.heap));
    buddy_free(&ba, a);
    buddy_dump(&ba, "after free — buddy not free, no merge");

    printf("\n  free(second block at offset %d)\n",
           (int)((char *)b - ba.heap));
    buddy_free(&ba, b);
    buddy_dump(&ba, "after free — cascading merges back to 1024");
    printf("\n");
}

void demo2_splitting_trace(void) {
    printf("=== Demo 2: Splitting Trace ===\n\n");

    BuddyAllocator ba;
    buddy_init(&ba);

    printf("  alloc(10): needs order 4 (16 bytes)\n");
    printf("  Splitting: 1024 -> 512+512 -> 256+256 -> ... -> 16+16\n\n");

    void *a = buddy_alloc(&ba, 10);
    printf("  Returned offset: %d, block size: 16 (waste: 6 bytes)\n",
           (int)((char *)a - ba.heap));
    buddy_dump(&ba, "after alloc(10)");
    printf("\n");
}

void demo3_buddy_calculation(void) {
    printf("=== Demo 3: Buddy Address Calculation ===\n\n");

    printf("  %-8s  %-8s  %-8s  %-12s\n",
           "Offset", "Order", "Size", "Buddy offset");

    int test_cases[][2] = {
        {0, 4}, {16, 4}, {0, 5}, {32, 5}, {0, 6}, {64, 6},
        {128, 7}, {0, 7}, {256, 8}, {0, 9}, {512, 9},
    };
    int n = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < n; i++) {
        int offset = test_cases[i][0];
        int order = test_cases[i][1];
        int buddy = offset ^ (1 << order);
        printf("  %-8d  %-8d  %-8d  %-12d\n",
               offset, order, 1 << order, buddy);
    }
    printf("\n  Formula: buddy = offset XOR (1 << order)\n\n");
}

void demo4_merge_cascade(void) {
    printf("=== Demo 4: Merge Cascade ===\n\n");

    BuddyAllocator ba;
    buddy_init(&ba);

    /* Allocate 4 blocks of 256 bytes (order 8) */
    void *a = buddy_alloc(&ba, 200);
    void *b = buddy_alloc(&ba, 200);
    void *c = buddy_alloc(&ba, 200);
    void *d = buddy_alloc(&ba, 200);

    printf("  4 blocks of 256 bytes allocated (requests of 200)\n");
    buddy_dump(&ba, "full heap");

    printf("\n  Freeing in order: d, c, b, a\n");
    printf("  Watch the cascading merges:\n\n");

    buddy_free(&ba, d);
    buddy_dump(&ba, "after free(d) — no merge (c is used)");

    buddy_free(&ba, c);
    buddy_dump(&ba, "after free(c) — merge c+d -> 512");

    buddy_free(&ba, b);
    buddy_dump(&ba, "after free(b) — no merge (buddy of b is used: a)");

    buddy_free(&ba, a);
    buddy_dump(&ba, "after free(a) — merge a+b=512, then 512+512=1024");
    printf("\n");
}

void demo5_internal_fragmentation(void) {
    printf("=== Demo 5: Internal Fragmentation ===\n\n");

    printf("  %-10s  %-10s  %-8s  %-8s\n",
           "Requested", "Allocated", "Waste", "Waste %%");

    size_t requests[] = {1, 7, 10, 17, 33, 65, 100, 129, 200, 500};
    size_t total_req = 0, total_alloc = 0;

    for (int i = 0; i < 10; i++) {
        size_t req = requests[i];
        int order = ceil_log2((int)req);
        if (order < MIN_ORDER) order = MIN_ORDER;
        size_t alloc = (size_t)(1 << order);
        size_t waste = alloc - req;
        double pct = 100.0 * (double)waste / (double)alloc;

        printf("  %-10zu  %-10zu  %-8zu  %-8.1f\n",
               req, alloc, waste, pct);
        total_req += req;
        total_alloc += alloc;
    }

    printf("\n  Total: requested=%zu, allocated=%zu\n", total_req, total_alloc);
    printf("  Overall waste: %.1f%%\n\n",
           100.0 * (double)(total_alloc - total_req) / (double)total_alloc);
}

void demo6_blocked_merge(void) {
    printf("=== Demo 6: Blocked Merge (Pinned Block) ===\n\n");

    BuddyAllocator ba;
    buddy_init(&ba);

    /* Allocate many small blocks */
    void *blocks[16];
    int count = 0;
    for (int i = 0; i < 16; i++) {
        blocks[i] = buddy_alloc(&ba, 60);
        if (blocks[i]) count++;
    }
    printf("  Allocated %d blocks of 64 bytes\n", count);
    buddy_dump(&ba, "all blocks used");

    /* Free all except one in the middle */
    for (int i = 0; i < 16; i++) {
        if (i == 7) continue;  /* keep block 7 */
        if (blocks[i]) buddy_free(&ba, blocks[i]);
    }

    printf("\n  Freed all except block #7 (offset %d)\n",
           blocks[7] ? (int)((char *)blocks[7] - ba.heap) : -1);
    buddy_dump(&ba, "one pinned block prevents full merge");
    buddy_stats(&ba);

    printf("\n  Block #7 prevents merging its buddy pair,\n");
    printf("  which prevents the parent from merging, and so on.\n");
    printf("  One 64-byte block pins 960 bytes of free memory\n");
    printf("  into blocks that can't merge to full 1024.\n\n");
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_basic_alloc_free();
    demo2_splitting_trace();
    demo3_buddy_calculation();
    demo4_merge_cascade();
    demo5_internal_fragmentation();
    demo6_blocked_merge();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * buddy_system.rs
 *
 * Simplified buddy allocator. Block sizes are powers of 2.
 * Splitting and merging use XOR-based buddy calculation.
 *
 * Compile: rustc -o buddy_system buddy_system.rs
 * Run:     ./buddy_system
 */

/* =================== BUDDY ALLOCATOR =============================== */

const MAX_ORDER: usize = 10;    /* 2^10 = 1024 bytes total */
const MIN_ORDER: usize = 4;     /* 2^4  = 16 bytes minimum */
const HEAP_SIZE: usize = 1 << MAX_ORDER;   /* 1024 */
const MIN_SIZE: usize  = 1 << MIN_ORDER;   /* 16 */
const NUM_ORDERS: usize = MAX_ORDER - MIN_ORDER + 1; /* 7 */
const NUM_SLOTS: usize = HEAP_SIZE / MIN_SIZE;       /* 64 */

struct BuddyAllocator {
    heap: [u8; HEAP_SIZE],
    /// Each order has a list of free block offsets
    free_lists: [Vec<usize>; NUM_ORDERS],
    /// Order of the block starting at each slot (-1 = not a block start)
    block_order: [i8; NUM_SLOTS],
}

impl BuddyAllocator {
    fn new() -> Self {
        let mut ba = BuddyAllocator {
            heap: [0u8; HEAP_SIZE],
            free_lists: Default::default(),
            block_order: [-1i8; NUM_SLOTS],
        };
        ba.free_lists[Self::oidx(MAX_ORDER)].push(0);
        ba.block_order[0] = MAX_ORDER as i8;
        ba
    }

    fn oidx(order: usize) -> usize { order - MIN_ORDER }

    fn ceil_log2(n: usize) -> usize {
        let mut k = 0;
        let mut v = 1;
        while v < n { v <<= 1; k += 1; }
        k
    }

    fn is_free(&self, order: usize, offset: usize) -> bool {
        self.free_lists[Self::oidx(order)].contains(&offset)
    }

    fn remove_from_list(&mut self, order: usize, offset: usize) {
        let idx = Self::oidx(order);
        if let Some(pos) = self.free_lists[idx].iter().position(|&x| x == offset) {
            self.free_lists[idx].swap_remove(pos);
        }
    }

    fn alloc(&mut self, size: usize) -> Option<usize> {
        let size = if size == 0 { 1 } else { size };
        let mut order = Self::ceil_log2(size);
        if order < MIN_ORDER { order = MIN_ORDER; }
        if order > MAX_ORDER { return None; }

        /* Find a free block */
        let mut j = order;
        while j <= MAX_ORDER {
            if !self.free_lists[Self::oidx(j)].is_empty() { break; }
            j += 1;
        }
        if j > MAX_ORDER { return None; }

        /* Remove block */
        let block_offset = self.free_lists[Self::oidx(j)][0];
        self.remove_from_list(j, block_offset);

        /* Split down */
        let mut offset = block_offset;
        while j > order {
            j -= 1;
            let buddy_off = offset + (1 << j);
            self.free_lists[Self::oidx(j)].push(buddy_off);
            self.block_order[buddy_off / MIN_SIZE] = j as i8;
        }

        self.block_order[offset / MIN_SIZE] = order as i8;
        Some(offset)
    }

    fn free(&mut self, offset: usize) {
        let slot = offset / MIN_SIZE;
        if self.block_order[slot] < 0 { return; }
        let mut order = self.block_order[slot] as usize;
        let mut off = offset;

        /* Try to merge */
        while order < MAX_ORDER {
            let buddy_off = off ^ (1 << order);
            let buddy_slot = buddy_off / MIN_SIZE;

            if buddy_slot >= NUM_SLOTS { break; }
            if self.block_order[buddy_slot] != order as i8 { break; }
            if !self.is_free(order, buddy_off) { break; }

            self.remove_from_list(order, buddy_off);
            self.block_order[buddy_off / MIN_SIZE] = -1;

            if buddy_off < off { off = buddy_off; }
            order += 1;
        }

        self.block_order[off / MIN_SIZE] = order as i8;
        self.free_lists[Self::oidx(order)].push(off);
    }

    fn dump(&self, label: &str) {
        println!("  [{}]", label);
        println!("    Free lists:");
        for i in 0..NUM_ORDERS {
            let order = i + MIN_ORDER;
            print!("      order {:>2} ({:>4} bytes):", order, 1 << order);
            if self.free_lists[i].is_empty() {
                println!(" (empty)");
            } else {
                for &off in &self.free_lists[i] {
                    print!(" @{}", off);
                }
                println!();
            }
        }
    }

    fn stats(&self) -> (usize, usize, usize) {
        /* (free_blocks, free_bytes, largest_free) */
        let mut fb = 0;
        let mut fby = 0;
        let mut lf = 0;
        for i in 0..NUM_ORDERS {
            let order = i + MIN_ORDER;
            let sz = 1 << order;
            let count = self.free_lists[i].len();
            fb += count;
            fby += count * sz;
            if count > 0 && sz > lf { lf = sz; }
        }
        (fb, fby, lf)
    }

    fn print_stats(&self) {
        let (fb, fby, lf) = self.stats();
        println!("    Free: {} blocks, {} bytes (largest={})",
                 fb, fby, lf);
        println!("    Used: {} bytes", HEAP_SIZE - fby);
    }
}

/* Need Default for array of Vec */
impl Default for BuddyAllocator {
    fn default() -> Self { Self::new() }
}

/* =================== DEMOS ========================================= */

fn demo1_basic_alloc_free() {
    println!("=== Demo 1: Basic Alloc and Free ===\n");

    let mut ba = BuddyAllocator::new();
    ba.dump("initial");

    println!("\n  alloc(100): needs order 7 (128)");
    let a = ba.alloc(100).unwrap();
    println!("    returned offset: {}", a);
    ba.dump("after alloc(100)");

    println!("\n  alloc(50): needs order 6 (64)");
    let b = ba.alloc(50).unwrap();
    println!("    returned offset: {}", b);
    ba.dump("after alloc(50)");

    println!("\n  free({})", a);
    ba.free(a);
    ba.dump("after free — buddy used, no merge");

    println!("\n  free({})", b);
    ba.free(b);
    ba.dump("after free — cascading merges to 1024");
    println!();
}

fn demo2_splitting_trace() {
    println!("=== Demo 2: Splitting Trace ===\n");

    let mut ba = BuddyAllocator::new();

    println!("  alloc(10): order 4 (16 bytes), waste = 6");
    println!("  Split chain: 1024->512->256->128->64->32->16\n");

    let a = ba.alloc(10).unwrap();
    println!("  Returned offset: {}", a);
    ba.dump("after alloc(10)");
    println!();
}

fn demo3_buddy_calculation() {
    println!("=== Demo 3: Buddy Address Calculation ===\n");

    println!("  {:>8}  {:>5}  {:>6}  {:>12}",
             "Offset", "Order", "Size", "Buddy");

    let cases: Vec<(usize, usize)> = vec![
        (0, 4), (16, 4), (0, 5), (32, 5), (0, 6), (64, 6),
        (128, 7), (0, 7), (256, 8), (0, 9), (512, 9),
    ];

    for (offset, order) in &cases {
        let buddy = offset ^ (1 << order);
        println!("  {:>8}  {:>5}  {:>6}  {:>12}",
                 offset, order, 1 << order, buddy);
    }
    println!("\n  buddy = offset XOR (1 << order)\n");
}

fn demo4_merge_cascade() {
    println!("=== Demo 4: Merge Cascade ===\n");

    let mut ba = BuddyAllocator::new();

    let a = ba.alloc(200).unwrap();
    let b = ba.alloc(200).unwrap();
    let c = ba.alloc(200).unwrap();
    let d = ba.alloc(200).unwrap();

    println!("  4 x alloc(200) -> 4 blocks of 256");
    ba.dump("full heap");

    println!("\n  Free in order d, c, b, a:");

    ba.free(d);
    ba.dump("free(d) — no merge");

    ba.free(c);
    ba.dump("free(c) — merge c+d -> 512");

    ba.free(b);
    ba.dump("free(b) — no merge (a is used)");

    ba.free(a);
    ba.dump("free(a) — merge a+b=512, 512+512=1024");
    println!();
}

fn demo5_internal_fragmentation() {
    println!("=== Demo 5: Internal Fragmentation ===\n");

    println!("  {:>9}  {:>9}  {:>6}  {:>7}",
             "Requested", "Allocated", "Waste", "Waste%");

    let requests = [1, 7, 10, 17, 33, 65, 100, 129, 200, 500];
    let mut total_req = 0usize;
    let mut total_alloc = 0usize;

    for &req in &requests {
        let mut order = BuddyAllocator::ceil_log2(req);
        if order < MIN_ORDER { order = MIN_ORDER; }
        let alloc = 1 << order;
        let waste = alloc - req;
        let pct = 100.0 * waste as f64 / alloc as f64;

        println!("  {:>9}  {:>9}  {:>6}  {:>6.1}%",
                 req, alloc, waste, pct);
        total_req += req;
        total_alloc += alloc;
    }

    println!("\n  Total: requested={}, allocated={}", total_req, total_alloc);
    println!("  Overall waste: {:.1}%\n",
             100.0 * (total_alloc - total_req) as f64 / total_alloc as f64);
}

fn demo6_blocked_merge() {
    println!("=== Demo 6: Blocked Merge (Pinned Block) ===\n");

    let mut ba = BuddyAllocator::new();

    let mut blocks = Vec::new();
    for _ in 0..16 {
        if let Some(off) = ba.alloc(60) {
            blocks.push(off);
        }
    }
    println!("  Allocated {} blocks of 64 bytes", blocks.len());
    ba.dump("all used");

    let pinned_idx = 7;
    let pinned_off = blocks[pinned_idx];

    for (i, &off) in blocks.iter().enumerate() {
        if i == pinned_idx { continue; }
        ba.free(off);
    }

    println!("\n  Freed all except block #{} (offset {})", pinned_idx, pinned_off);
    ba.dump("one pinned block");
    ba.print_stats();

    println!("\n  One 64-byte block prevents full merge.");
    println!("  Its buddy can't merge -> parent can't merge -> ...\n");
}

fn demo7_utilization_comparison() {
    println!("=== Demo 7: Buddy vs Linear Allocator Utilization ===\n");

    let requests = [13, 27, 5, 41, 19, 33, 9, 50, 7, 45];

    /* Buddy: round to power of 2 */
    let mut buddy_total = 0usize;
    let mut req_total = 0usize;

    println!("  {:>4}  {:>6}  {:>6}  {:>6}",
             "Req", "Buddy", "Align8", "Waste%B");

    for &req in &requests {
        let mut order = BuddyAllocator::ceil_log2(req);
        if order < MIN_ORDER { order = MIN_ORDER; }
        let buddy_alloc = 1 << order;
        let linear_alloc = (req + 7) & !7; /* align to 8 */
        let waste_pct = 100.0 * (buddy_alloc - req) as f64 / buddy_alloc as f64;

        println!("  {:>4}  {:>6}  {:>6}  {:>5.1}%",
                 req, buddy_alloc, linear_alloc, waste_pct);
        buddy_total += buddy_alloc;
        req_total += req;
    }

    let linear_total: usize = requests.iter()
        .map(|&r| (r + 7) & !7)
        .sum();

    println!("\n  Buddy total:  {} (waste {:.1}%)", buddy_total,
             100.0 * (buddy_total - req_total) as f64 / buddy_total as f64);
    println!("  Linear total: {} (waste {:.1}%)", linear_total,
             100.0 * (linear_total - req_total) as f64 / linear_total as f64);
    println!("  Buddy wastes more due to power-of-2 rounding.\n");
}

fn demo8_proc_buddyinfo() {
    println!("=== Demo 8: Simulated /proc/buddyinfo ===\n");

    let mut ba = BuddyAllocator::new();

    /* Simulate a workload */
    let mut live = Vec::new();
    let allocs = [30, 60, 120, 200, 15, 40, 80, 500, 10, 25];

    for &sz in &allocs {
        if let Some(off) = ba.alloc(sz) {
            live.push(off);
        }
    }

    /* Free some */
    let free_indices = [1, 3, 5, 7, 9];
    for &i in &free_indices {
        if i < live.len() {
            ba.free(live[i]);
        }
    }

    /* Print buddyinfo-style */
    println!("  Simulated buddyinfo (blocks free per order):\n");
    print!("  ");
    for i in 0..NUM_ORDERS {
        let order = i + MIN_ORDER;
        print!(" {:>4}B", 1 << order);
    }
    println!();

    print!("  ");
    for i in 0..NUM_ORDERS {
        print!(" {:>5}", ba.free_lists[i].len());
    }
    println!("\n");

    ba.print_stats();
    println!("\n  This mirrors Linux /proc/buddyinfo format,");
    println!("  showing free block counts per order.\n");
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_basic_alloc_free();
    demo2_splitting_trace();
    demo3_buddy_calculation();
    demo4_merge_cascade();
    demo5_internal_fragmentation();
    demo6_blocked_merge();
    demo7_utilization_comparison();
    demo8_proc_buddyinfo();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Calculo de buddy

Un heap de 512 bytes. Bloque en offset 192, tamano 64. Calcula:

1. El offset del buddy.
2. Si se fusionan, que offset y tamano tiene el bloque resultante?
3. Cual es el buddy del bloque fusionado?

<details><summary>Prediccion</summary>

1. $\text{buddy}(192, 6) = 192 \oplus 64 = 192 \oplus 64$.
   $192 = 0b11000000$, $64 = 0b01000000$.
   $192 \oplus 64 = 0b10000000 = 128$.
   **Buddy en offset 128.**

2. Bloque fusionado: $\min(128, 192) = 128$, tamano $= 64 + 64 = 128$
   (orden 7). **Offset 128, tamano 128.**

3. $\text{buddy}(128, 7) = 128 \oplus 128 = 0$. **Buddy en offset 0.**
   Si ambos se fusionan: offset 0, tamano 256 (orden 8).

</details>

### Ejercicio 2 — Secuencia de splits

Heap de 256 bytes (MIN_ORDER = 3, MIN_SIZE = 8). Se pide `alloc(5)`.
Traza todos los splits y el estado final de las free lists.

<details><summary>Prediccion</summary>

$\lceil \log_2(5) \rceil = 3$, necesitamos orden 3 ($2^3 = 8$).

Free list inicial: `free_list[8]: [0..255]` (un bloque de 256).

Splits:
1. Dividir [0..255] (orden 8) en [0..127] y [128..255].
   `free_list[7]: [128..255]`. Seguir con [0..127].
2. Dividir [0..127] (orden 7) en [0..63] y [64..127].
   `free_list[6]: [64..127]`. Seguir con [0..63].
3. Dividir [0..63] (orden 6) en [0..31] y [32..63].
   `free_list[5]: [32..63]`. Seguir con [0..31].
4. Dividir [0..31] (orden 5) en [0..15] y [16..31].
   `free_list[4]: [16..31]`. Seguir con [0..15].
5. Dividir [0..15] (orden 4) en [0..7] y [8..15].
   `free_list[3]: [8..15]`. Retornar [0..7].

Estado final:
```
free_list[3]: [@8]      (8 bytes)
free_list[4]: [@16]     (16 bytes)
free_list[5]: [@32]     (32 bytes)
free_list[6]: [@64]     (64 bytes)
free_list[7]: [@128]    (128 bytes)
```

5 splits para llegar de orden 8 a orden 3. Bloque retornado: [0..7],
8 bytes. Desperdicio: 3 bytes (37.5%).

Total libre: $8 + 16 + 32 + 64 + 128 = 248$. Usado: 8. Los 248 bytes
libres estan en bloques que se podran fusionar si el bloque de 8 se libera.

</details>

### Ejercicio 3 — Merge bloqueado

Heap de 128 bytes. Se alocan 8 bloques de 16 bytes (orden 4). Se liberan
todos excepto el bloque en offset 48. Cual es el estado de las free lists?

<details><summary>Prediccion</summary>

Bloques: [0..15], [16..31], [32..47], **[48..63]** (pinned), [64..79],
[80..95], [96..111], [112..127].

Liberando todos excepto offset 48:

- free(0): buddy(0,4)=16. 16 libre? Si. Merge → [0..31] orden 5.
  buddy(0,5)=32. 32 libre? Si. Merge → [0..63]? No, porque [48..63]
  contiene un bloque usado. El bloque en offset 32 es solo [32..47], y
  su buddy es [48..63] (usado). No se puede fusionar.

Tracemos con mas cuidado, liberando uno por uno:

Estado despues de alocar 8 bloques: todos USED, free lists vacias.

free(0): [0..15] libre. buddy=16 (usado). → free_list[4]: [@0]
free(16): [16..31] libre. buddy=0, libre. Merge → [0..31] orden 5.
  buddy(0,5)=32 (usado). → free_list[5]: [@0]
free(32): [32..47] libre. buddy=48 (usado). → free_list[4]: [@32]
  /* 48 esta pinned, no se puede fusionar */
free(64): [64..79] libre. buddy=80 (usado). → free_list[4]: [@64]
free(80): [80..95] libre. buddy=64, libre. Merge → [64..95] orden 5.
  buddy(64,5)=96 (usado). → free_list[5]: [@64]
free(96): [96..111] libre. buddy=112 (usado). → free_list[4]: [@96]
free(112): [112..127] libre. buddy=96, libre. Merge → [96..127] orden 5.
  buddy(96,5)=64, libre (orden 5). Merge → [64..127] orden 6.
  buddy(64,6)=0. [0..31] es libre pero orden 5, no 6. No merge.
  Espera: buddy(64,6) = 64 XOR 64 = 0. [0..63]? El bloque en 0 es orden 5
  ([0..31]), no 6. No se puede fusionar.
  → free_list[6]: [@64]

Estado final:
```
free_list[4]: [@32]           (16 bytes)
free_list[5]: [@0]            (32 bytes)
free_list[6]: [@64]           (64 bytes)
free_list[7]: (vacia)
```

Bloque pinned: [48..63] (16 bytes).
Total libre: 16 + 32 + 64 = 112. Usado: 16.
Mayor bloque libre: 64.

El bloque de 48 impide que [32..47] se fusione con [48..63], lo cual
impide que [0..63] se forme (orden 6), lo cual impide fusionar con
[64..127] para recrear el heap completo.

</details>

### Ejercicio 4 — Fragmentacion interna

Para peticiones de tamano 1 a 64 (uniformes), calcula la fragmentacion
interna promedio del buddy system con MIN_ORDER = 4 (bloques minimos de
16). Compara con alineamiento a 8 bytes (allocator lineal).

<details><summary>Prediccion</summary>

**Buddy system** (bloques: 16, 32, 64):
- $n \in [1, 16]$: bloque = 16. Waste promedio = $16 - 8.5 = 7.5$.
- $n \in [17, 32]$: bloque = 32. Waste promedio = $32 - 24.5 = 7.5$.
- $n \in [33, 64]$: bloque = 64. Waste promedio = $64 - 48.5 = 15.5$.

Ponderando por rango (16 + 16 + 32 = 64 valores):
$\bar{w}_{\text{buddy}} = \frac{7.5 \times 16 + 7.5 \times 16 + 15.5 \times 32}{64}$
$= \frac{120 + 120 + 496}{64} = \frac{736}{64} = 11.5$ bytes.

Tamano promedio de bloque: $\frac{16 \times 16 + 32 \times 16 + 64 \times 32}{64} = \frac{256 + 512 + 2048}{64} = 44$ bytes.

$F_{\text{int}}^{\text{buddy}} = 11.5 / 44 = 26.1\%$

**Allocator lineal** (alineamiento a 8):
Para $n \in [1, 64]$: bloque = $\lceil n/8 \rceil \times 8$.
- $n \in [1,8]$: bloque = 8, waste = $8 - 4.5 = 3.5$.
- $n \in [9,16]$: bloque = 16, waste = $16 - 12.5 = 3.5$.
- ... cada rango de 8: waste promedio = 3.5.

$\bar{w}_{\text{linear}} = 3.5$ bytes. Bloque promedio = $\bar{n} + 3.5 = 36$.
$F_{\text{int}}^{\text{linear}} = 3.5 / 36 = 9.7\%$

**Buddy desperdicia ~2.7x mas** que un allocator lineal en fragmentacion
interna. Este es el precio de la fusion rapida y la eliminacion de
fragmentacion externa.

</details>

### Ejercicio 5 — /proc/buddyinfo

Un sistema Linux muestra:

```
Node 0, zone Normal  512  256  128  64  32  16  8  4  2  1  0
```

(Ordenes 0-10, donde orden 0 = 4KB.)

1. Cuanta memoria libre total hay?
2. Cual es la allocacion mas grande posible?
3. Que indica el 0 en orden 10?

<details><summary>Prediccion</summary>

1. **Memoria libre total**:
   $512 \times 4\text{KB} + 256 \times 8\text{KB} + 128 \times 16\text{KB}
   + 64 \times 32\text{KB} + 32 \times 64\text{KB} + 16 \times 128\text{KB}
   + 8 \times 256\text{KB} + 4 \times 512\text{KB} + 2 \times 1\text{MB}
   + 1 \times 2\text{MB} + 0 \times 4\text{MB}$

   $= 2048 + 2048 + 2048 + 2048 + 2048 + 2048 + 2048 + 2048 + 2048
   + 2048 + 0 = 20480 \text{ KB} = 20 \text{ MB}$.

2. **Allocacion mas grande**: el bloque libre mas grande es de orden 9
   ($2^9 \times 4\text{KB} = 2\text{MB}$). Se puede servir una peticion
   de hasta **2 MB** contiguos.

3. **0 en orden 10**: no hay ningun bloque libre de $2^{10} \times 4\text{KB}
   = 4\text{MB}$. Peticiones de >2 MB de memoria contigua fallarian.
   Para obtener un bloque de 4 MB, habria que fusionar dos bloques de
   2 MB, pero solo hay 1 de 2 MB (y su buddy no esta libre).

</details>

### Ejercicio 6 — Complejidad de alloc

Demuestra que `buddy_alloc` en un heap de $2^N$ requiere como maximo
$N - \text{MIN\_ORDER}$ splits. Cual es la complejidad si se usa un
bitmap para buscar el primer nivel con bloques libres?

<details><summary>Prediccion</summary>

**Cota de splits**: la peticion necesita orden $k \geq \text{MIN\_ORDER}$.
En el peor caso, solo hay un bloque libre de orden $N$ (maximo). Se hacen
splits: $N \to N-1 \to \cdots \to k$. Eso son $N - k$ splits. Como
$k \geq \text{MIN\_ORDER}$, el maximo es $N - \text{MIN\_ORDER}$.

Para nuestro ejemplo ($N=10$, MIN_ORDER=4): maximo 6 splits.

**Busqueda del nivel**: sin optimizacion, buscamos secuencialmente en
`free_list[k]`, `free_list[k+1]`, ..., `free_list[N]`. Son $N - k + 1$
listas. Si cada verificacion es $O(1)$ (comprobar si la lista esta
vacia), la busqueda es $O(N - k)$.

**Con bitmap**: mantenemos un bitmap de $N - \text{MIN\_ORDER} + 1$ bits
donde el bit $i$ indica si `free_list[i]` no esta vacia. Para encontrar
el primer nivel con bloques libres a partir de $k$, usamos la instruccion
hardware `CTZ` (Count Trailing Zeros) sobre el bitmap con los bits $< k$
enmascarados. Esto es $O(1)$.

Con bitmap: alloc = $O(1)$ busqueda + $O(N-k)$ splits = $O(N)$ peor caso
(dominado por splits). Pero $N$ es tipicamente 20-30 para heaps reales,
asi que es efectivamente constante.

</details>

### Ejercicio 7 — Buddy system vs pool

Compara el buddy system con un pool allocator para un escenario donde
todos los objetos son de 48 bytes. Cual es mejor y por que?

<details><summary>Prediccion</summary>

**Buddy system para objetos de 48 bytes**:
- Cada alloc redondea 48 a $2^6 = 64$ bytes.
- Desperdicio por objeto: 16 bytes (25%).
- alloc: $O(\log N)$ con posibles splits.
- free: $O(\log N)$ con posibles merges.

**Pool allocator para objetos de 48 bytes**:
- Pre-asigna un bloque grande, lo divide en slots de 48 bytes.
- Desperdicio por objeto: 0 bytes (tamano exacto).
- alloc: $O(1)$ — tomar el primer slot libre (pop de free list).
- free: $O(1)$ — devolver el slot (push a free list).
- Sin fragmentacion (todos los bloques son del mismo tamano).

**Pool es claramente superior** para este caso:
- 25% menos memoria por objeto.
- $O(1)$ vs $O(\log N)$ para alloc/free.
- Sin fragmentacion de ningun tipo.

**Cuando el buddy es mejor**: cuando los tamanos son **variados** y no
se conocen de antemano. Un pool necesita un tamano fijo; el buddy maneja
cualquier tamano (con costo de fragmentacion interna). Por eso el kernel
de Linux usa buddy + slab: buddy para paginas de tamano variable, slab
(que es un pool) para objetos de tamano conocido.

</details>

### Ejercicio 8 — Weighted buddy

En un weighted buddy, un bloque de $2^k$ se puede dividir en $2^{k-1}$
+ $2^{k-1}$ (normal) o en $2^{k-2}$ + $3 \cdot 2^{k-2}$ (weighted).

Para un bloque de 64 bytes y una peticion de 20 bytes:
1. Que hace el buddy clasico?
2. Que hace el weighted buddy?
3. Cuanto se ahorra?

<details><summary>Prediccion</summary>

1. **Buddy clasico**: $\lceil \log_2(20) \rceil = 5$, bloque de $2^5 = 32$.
   Split 64 → 32 + 32. Retorna un bloque de 32. Desperdicio: 12 bytes.

2. **Weighted buddy**: puede dividir 64 → 16 + 48. La peticion de 20
   cabe en un bloque de 32, pero no en 16. Alternativa: dividir 64 →
   32 + 32 (normal) → retorna 32. O dividir 48 → 16 + 32, retorna 32.

   Mejor: dividir 64 → 16 + 48. El 48 no es potencia de 2, pero en
   weighted buddy es un tamano valido ($3 \cdot 2^4 = 48$). Luego
   dividir 48 → 16 + 32. Retornar 32. Desperdicio: 12 bytes.

   Hmm, en este caso ambos dan 32. Veamos con peticion de 40:

   - **Clasico**: $\lceil \log_2(40) \rceil = 6$, bloque de 64. Desperdicio:
     24 bytes (37.5%).
   - **Weighted**: dividir 64 → 16 + 48. La peticion de 40 cabe en 48.
     Desperdicio: 8 bytes (16.7%).

3. **Ahorro con peticion de 40**: 24 - 8 = 16 bytes (66% menos desperdicio).

   El weighted buddy brilla cuando la peticion esta justo por encima de
   $2^{k-1}$: el buddy clasico asigna $2^k$ (hasta 50% desperdicio),
   mientras que el weighted puede asignar $3 \cdot 2^{k-2}$ (maximo
   33% desperdicio).

</details>

### Ejercicio 9 — Comparacion de coalescing

Compara el costo de coalescing en:
1. Free list con scan lineal (T01).
2. Free list con boundary tags (T03).
3. Buddy system.

Cual tiene el mejor tradeoff tiempo-espacio?

<details><summary>Prediccion</summary>

| Metodo | Tiempo de coalescing | Espacio extra |
|--------|:-------------------:|:-------------:|
| Scan lineal | $O(n)$ bloques | 0 |
| Boundary tags | $O(1)$ | 1 `size_t` por bloque (8 bytes en 64-bit) |
| Buddy system | $O(1)$ por nivel, $O(\log N)$ total | Bitmap: ~$N$ bits o array de ordenes |

**Scan lineal**: para encontrar el bloque anterior, se recorre desde el
inicio. Gratis en espacio, pero $O(n)$ en tiempo. Impracticable con miles
de bloques.

**Boundary tags**: footer duplica el tamano al final del bloque. Acceso
$O(1)$ al vecino anterior. Costo: 8 bytes extra por bloque. Para bloques
pequenos (16-32 bytes), eso es 25-50% de overhead.

**Buddy system**: el buddy se calcula con XOR ($O(1)$), y se verifica si
esta libre consultando la free list o bitmap ($O(1)$). Se repite hasta
$O(\log N)$ niveles. Costo en espacio: bitmap ($2^{N-\text{MIN\_ORDER}}$
bits) o array de ordenes.

**Mejor tradeoff**: depende del workload.
- Bloques grandes + variados: **boundary tags** (overhead relativo bajo,
  $O(1)$ constante).
- Gestion de paginas: **buddy system** (overhead minimo, $O(\log N)$
  pero $N \leq 30$).
- Bloques pequenos: ni boundary tags (overhead alto) ni buddy
  (fragmentacion alta). Usar **pool/slab**.

</details>

### Ejercicio 10 — Disena un buddy + slab

Tienes un heap de 4096 bytes. Disena un sistema de 2 capas:
1. Buddy system con MIN_ORDER = 7 (bloques de 128 bytes).
2. Slab allocator para objetos de 24 bytes dentro de bloques de 128 bytes.

Cuantos objetos de 24 bytes caben en un slab? Cual es la fragmentacion
interna del slab? Como se solicita un nuevo slab cuando se agotan?

<details><summary>Prediccion</summary>

**Capa buddy**: heap = 4096 = $2^{12}$, MAX_ORDER = 12, MIN_ORDER = 7.
Bloques: 128, 256, 512, 1024, 2048, 4096 bytes.

**Slab de 128 bytes para objetos de 24 bytes**:
- Objetos por slab: $\lfloor 128 / 24 \rfloor = 5$ objetos.
- Espacio usado: $5 \times 24 = 120$ bytes.
- Desperdicio: $128 - 120 = 8$ bytes por slab (6.25% fragmentacion interna).
- Se necesita un bitmap de 5 bits (1 byte) para rastrear slots libres.
  Esto se puede guardar en los 8 bytes de desperdicio.

**Flujo de allocacion**:
1. `slab_alloc(24)`:
   - Si hay un slab con slots libres: encontrar primer bit 0 en bitmap,
     retornar `slab_base + slot_index * 24`. $O(1)$.
   - Si no hay slots: `buddy_alloc(128)` para obtener un nuevo slab.
     Inicializar bitmap a 0b00000. Retornar slot 0. $O(\log N)$.

2. `slab_free(ptr)`:
   - Calcular a que slab pertenece: `slab_base = ptr & ~127`.
   - Calcular slot: `(ptr - slab_base) / 24`.
   - Marcar bit como libre en bitmap.
   - Si todos los slots estan libres: `buddy_free(slab_base)`.

**Capacidad total**: $4096 / 128 = 32$ slabs maximo = $32 \times 5 = 160$
objetos de 24 bytes. Sin la capa slab, el buddy asignaria bloques de 32
($2^5$) para cada objeto → $4096 / 32 = 128$ objetos, con 25% desperdicio.
El slab mejora la capacidad en un 25% ($160 / 128$) y reduce el desperdicio
a 6.25%.

</details>
