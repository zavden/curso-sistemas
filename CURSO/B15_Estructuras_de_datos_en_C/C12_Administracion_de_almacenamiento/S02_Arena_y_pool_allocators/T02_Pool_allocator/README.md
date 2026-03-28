# T02 — Pool allocator

## Objetivo

Comprender e implementar un **pool allocator** (tambien llamado *slab
allocator* o *fixed-size allocator*): una estrategia que pre-asigna un
bloque grande de memoria, lo divide en **slots de tamano fijo**, y
gestiona la reutilizacion mediante una **free list embebida**. Analizar
por que es la solucion optima para nodos de listas, arboles y grafos, y
como elimina la fragmentacion por construccion.

---

## 1. Concepto fundamental

### 1.1 La idea

Un pool allocator gestiona objetos de un **unico tamano** $S$:

1. Reservar un bloque grande de $N \times S$ bytes.
2. Dividirlo en $N$ **slots** de $S$ bytes cada uno.
3. Encadenar todos los slots en una **free list**.
4. `alloc()`: pop del inicio de la free list → $O(1)$.
5. `free(ptr)`: push al inicio de la free list → $O(1)$.

```
Pool de 5 slots de 32 bytes:

Inicial (todos libres):
  [slot0] -> [slot1] -> [slot2] -> [slot3] -> [slot4] -> NULL
  ^free_head

alloc():  retorna slot0
  [USED ] | [slot1] -> [slot2] -> [slot3] -> [slot4] -> NULL
           ^free_head

alloc():  retorna slot1
  [USED ] | [USED ] | [slot2] -> [slot3] -> [slot4] -> NULL
                      ^free_head

free(slot0):  push slot0 al inicio
  [slot0] -> [slot2] -> [slot3] -> [slot4] -> NULL
  ^free_head
  (slot1 sigue USED, slot0 se reutiliza)
```

### 1.2 Complejidades

| Operacion | Complejidad | Detalle |
|-----------|:-----------:|---------|
| alloc | $O(1)$ | Pop de free list |
| free | $O(1)$ | Push a free list |
| init | $O(N)$ | Encadenar $N$ slots |

### 1.3 Comparacion

| Propiedad | malloc/free | Arena | Pool |
|-----------|:-----------:|:-----:|:----:|
| alloc | $O(k)$ | $O(1)$ | $O(1)$ |
| free individual | $O(1)$-$O(k)$ | N/A | $O(1)$ |
| free all | $O(n)$ | $O(1)$ | $O(1)$ (reset) |
| Frag. interna | Variable | Solo padding | **Cero** |
| Frag. externa | Variable | Cero | **Cero** |
| Tamano variable | Si | Si | **No** |
| Overhead/bloque | 16+ bytes | 0 | **0** |

El pool combina lo mejor del arena (cero overhead) con la capacidad de
liberar individualmente — a cambio de la restriccion de tamano fijo.

---

## 2. Free list embebida

### 2.1 El truco clave

Un bloque **libre** en el pool no tiene datos utiles. Podemos usar los
primeros bytes del propio slot para almacenar el puntero `next` de la
free list:

```
Slot libre (32 bytes):
  [next_ptr (8 bytes)][......unused (24 bytes)......]

Slot usado (32 bytes):
  [...........datos del usuario (32 bytes)............]
```

**No se necesita memoria adicional** para la free list — el puntero esta
**embebido** en el espacio del slot libre. Esto requiere que
$S \geq \text{sizeof(void*)}$ (8 bytes en 64-bit).

### 2.2 Union en C

En C, esto se modela con un union:

```c
typedef union Slot {
    union Slot *next;   /* cuando esta libre */
    char data[SLOT_SIZE]; /* cuando esta usado */
} Slot;
```

El campo `next` y `data` comparten la misma memoria. El pool sabe que
el slot esta libre si esta en la free list; si no, el usuario tiene
sus datos ahi.

### 2.3 Sin metadata

A diferencia de `malloc` (que almacena un header de 16+ bytes por
bloque), el pool **no tiene overhead por slot**. No hay header, no hay
flags, no hay tamano — porque todos los slots tienen el mismo tamano y
el estado (libre/usado) se determina por la pertenencia a la free list.

---

## 3. Fragmentacion: eliminada por construccion

### 3.1 Sin fragmentacion interna

Cada slot tiene exactamente el tamano del objeto. No hay redondeo (excepto
posible alineamiento a 8 bytes al definir el tamano del slot).

Si los objetos son de 24 bytes y el slot es de 24 bytes: desperdicio = 0.

### 3.2 Sin fragmentacion externa

Todos los slots tienen el mismo tamano. Un slot libre **siempre** puede
satisfacer una peticion, sin importar su posicion. No hay "huecos
inutilizables" — cada hueco es exactamente del tamano correcto.

### 3.3 Prueba formal

Sea $F$ el conjunto de slots libres, cada uno de tamano $S$. Para una
peticion de tamano $S$:

$$\forall s \in F: \text{size}(s) = S \geq S$$

Todo slot libre satisface la peticion. $F_{\text{ext}} = 0$ siempre.

---

## 4. Variantes de implementacion

### 4.1 Pool estatico (array)

El pool se construye sobre un array estatico o un unico `malloc`:

```c
#define POOL_SIZE 1000
char pool_mem[POOL_SIZE * SLOT_SIZE];
```

Ventaja: sin syscalls adicionales. Ideal para embebido.
Desventaja: tamano fijo, no puede crecer.

### 4.2 Pool con crecimiento

Cuando todos los slots se agotan, el pool aloca un nuevo bloque
(chunk) de $M$ slots:

```
Chunk 1 (100 slots): [USED][FREE][USED][USED][FREE]...
Chunk 2 (200 slots): [FREE][FREE][FREE][FREE]...
```

Los chunks se encadenan en una lista. Los slots libres de todos los
chunks se unen en una sola free list.

### 4.3 Pool con bitmap

Alternativa a la free list embebida: un **bitmap** de $N$ bits donde
el bit $i$ indica si el slot $i$ esta libre:

```
bitmap: 1101001011...
        slot0=USED, slot1=FREE, slot2=USED, slot3=FREE, ...
```

`alloc`: buscar el primer bit 0 con instruccion hardware `CTZ`/`BSF`
(1-2 ciclos). `free`: encender el bit.

Ventaja: iteracion sobre slots usados (para destruccion masiva).
Desventaja: `alloc` no es estrictamente $O(1)$ si el bitmap es grande
(es $O(N/64)$ — un word a la vez).

### 4.4 Pool con conteo

Mantener un contador de slots usados. Util para diagnosticos y para
saber cuando destruir el pool (contador = 0).

---

## 5. Casos de uso

### 5.1 Nodos de estructuras de datos

El caso de uso canonico: listas enlazadas, arboles, grafos, hash tables
con encadenamiento. Todos los nodos tienen el mismo tamano:

```c
typedef struct BSTNode {
    int key;
    struct BSTNode *left, *right;
} BSTNode;  /* sizeof = 24 bytes (con padding) */
```

Un pool de slots de 24 bytes es perfecto: cada `insert` aloca un nodo
en $O(1)$, cada `delete` lo libera en $O(1)$, sin fragmentacion.

### 5.2 Sistemas de particulas

Los motores de juegos usan pools para sistemas de particulas: miles de
particulas nacen y mueren cada frame, todas del mismo tamano.

### 5.3 Conexiones de red

Cada conexion activa tiene un struct de estado de tamano fijo. Un pool
gestiona la creacion y destruccion de conexiones sin presion sobre
`malloc`.

### 5.4 Allocator del kernel (slab)

El slab allocator del kernel de Linux es un pool sofisticado:

| Cache | Objeto | Tamano |
|-------|--------|:------:|
| `inode_cache` | struct inode | ~600 bytes |
| `dentry_cache` | struct dentry | ~192 bytes |
| `task_struct` | struct task_struct | ~6 KB |
| `mm_struct` | struct mm_struct | ~700 bytes |

Cada cache es un pool separado para un tipo de objeto. Los slabs se
asignan del buddy system (paginas de 4 KB) y se subdividen en slots.

### 5.5 Anti-caso: objetos de tamano variable

Si los objetos tienen tamanos variados, el pool desperdicia espacio
(fragmentacion interna forzada) o necesita multiples pools (uno por
tamano), lo cual complica la gestion.

---

## 6. Pool vs arena: cuando elegir cada uno

| Criterio | Arena | Pool |
|----------|:-----:|:----:|
| Tamano del objeto | Variable | Fijo |
| Lifetime | Todos iguales (batch) | Individual |
| Free individual | No | Si |
| Fragmentacion | Interna (padding) | Cero |
| Overhead/alloc | Cero | Cero |
| Complejidad alloc | $O(1)$ bump | $O(1)$ pop |
| Complejidad free | $O(1)$ reset all | $O(1)$ push |
| Caso ideal | Parsing, scratch | Nodos de lista/arbol |

**Regla**: si todos los objetos mueren juntos → arena. Si mueren
individualmente pero todos son del mismo tamano → pool.

---

## 7. Relacion con el slab allocator

### 7.1 Concepto de slab (Bonwick, 1994)

Jeff Bonwick introdujo el **slab allocator** en Solaris 2.4. Un slab
es un pool que:

1. Obtiene paginas del buddy system.
2. Las divide en slots de tamano fijo para un tipo de objeto.
3. Mantiene tres listas: **full** (todas usadas), **partial** (algunas
   libres), **empty** (todas libres).
4. Pre-construye los objetos (constructor) al crear el slab y los
   recicla sin re-inicializar.

### 7.2 Ventaja de la pre-construccion

Para objetos caros de inicializar (mutex, listas internas, etc.), el
slab llama al constructor una sola vez. Al "liberar" el objeto, no se
destruye — se devuelve al slab en estado inicializado, listo para
reutilizacion inmediata.

Esto es particularmente valioso para el kernel, donde la creacion de
`struct inode` implica inicializar locks, listas, contadores, etc.

### 7.3 Evolucion: SLUB

Linux actualmente usa **SLUB** (Slab Unqueued) como allocator por defecto,
una simplificacion del slab original que elimina las listas full/partial/empty
y usa metadatos en las paginas.

---

## 8. Programa completo en C

```c
/*
 * pool_allocator.c
 *
 * Pool (fixed-size) allocator with embedded free list.
 * Ideal for linked list nodes, tree nodes, particles, etc.
 *
 * Compile: gcc -O2 -o pool_allocator pool_allocator.c
 * Run:     ./pool_allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* =================== POOL ALLOCATOR ================================ */

typedef union FreeSlot {
    union FreeSlot *next;
    char data[1]; /* placeholder for user data */
} FreeSlot;

typedef struct {
    char *memory;       /* backing buffer */
    FreeSlot *free_head; /* free list head */
    size_t slot_size;    /* bytes per slot (>= sizeof(void*)) */
    size_t capacity;     /* total slots */
    size_t used;         /* slots in use */
} Pool;

Pool pool_create(size_t slot_size, size_t capacity) {
    /* Ensure slot can hold a pointer */
    if (slot_size < sizeof(FreeSlot *))
        slot_size = sizeof(FreeSlot *);
    /* Align slot size to 8 bytes */
    slot_size = (slot_size + 7) & ~(size_t)7;

    Pool p;
    p.memory = (char *)malloc(slot_size * capacity);
    p.slot_size = slot_size;
    p.capacity = capacity;
    p.used = 0;

    /* Build free list: chain all slots */
    p.free_head = NULL;
    for (size_t i = capacity; i > 0; i--) {
        FreeSlot *slot = (FreeSlot *)(p.memory + (i - 1) * slot_size);
        slot->next = p.free_head;
        p.free_head = slot;
    }

    return p;
}

void *pool_alloc(Pool *p) {
    if (!p->free_head) return NULL; /* pool exhausted */

    FreeSlot *slot = p->free_head;
    p->free_head = slot->next;
    p->used++;

    return (void *)slot;
}

void pool_free(Pool *p, void *ptr) {
    if (!ptr) return;

    FreeSlot *slot = (FreeSlot *)ptr;
    slot->next = p->free_head;
    p->free_head = slot;
    p->used--;
}

void pool_reset(Pool *p) {
    /* Rebuild free list */
    p->free_head = NULL;
    for (size_t i = p->capacity; i > 0; i--) {
        FreeSlot *slot = (FreeSlot *)(p->memory + (i - 1) * p->slot_size);
        slot->next = p->free_head;
        p->free_head = slot;
    }
    p->used = 0;
}

void pool_destroy(Pool *p) {
    free(p->memory);
    p->memory = NULL;
    p->free_head = NULL;
}

void pool_stats(Pool *p, const char *label) {
    printf("  [%s] slot_size=%zu, capacity=%zu, used=%zu, free=%zu\n",
           label, p->slot_size, p->capacity, p->used,
           p->capacity - p->used);
}

/* =================== BST WITH POOL ================================= */

typedef struct BSTNode {
    int key;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

static Pool bst_pool;

BSTNode *bst_alloc_node(int key) {
    BSTNode *n = (BSTNode *)pool_alloc(&bst_pool);
    if (!n) return NULL;
    n->key = key;
    n->left = NULL;
    n->right = NULL;
    return n;
}

BSTNode *bst_insert(BSTNode *root, int key) {
    if (!root) return bst_alloc_node(key);
    if (key < root->key)
        root->left = bst_insert(root->left, key);
    else if (key > root->key)
        root->right = bst_insert(root->right, key);
    return root;
}

BSTNode *bst_find_min(BSTNode *node) {
    while (node->left) node = node->left;
    return node;
}

BSTNode *bst_delete(BSTNode *root, int key) {
    if (!root) return NULL;

    if (key < root->key) {
        root->left = bst_delete(root->left, key);
    } else if (key > root->key) {
        root->right = bst_delete(root->right, key);
    } else {
        /* Found node to delete */
        if (!root->left) {
            BSTNode *right = root->right;
            pool_free(&bst_pool, root);
            return right;
        }
        if (!root->right) {
            BSTNode *left = root->left;
            pool_free(&bst_pool, root);
            return left;
        }
        /* Two children: replace with in-order successor */
        BSTNode *succ = bst_find_min(root->right);
        root->key = succ->key;
        root->right = bst_delete(root->right, succ->key);
    }
    return root;
}

void bst_inorder(BSTNode *root) {
    if (!root) return;
    bst_inorder(root->left);
    printf("%d ", root->key);
    bst_inorder(root->right);
}

void bst_free_all(BSTNode *root) {
    if (!root) return;
    bst_free_all(root->left);
    bst_free_all(root->right);
    pool_free(&bst_pool, root);
}

/* =================== LINKED LIST WITH POOL ========================= */

typedef struct ListNode {
    int value;
    struct ListNode *next;
} ListNode;

static Pool list_pool;

ListNode *list_push(ListNode *head, int value) {
    ListNode *n = (ListNode *)pool_alloc(&list_pool);
    if (!n) return head;
    n->value = value;
    n->next = head;
    return n;
}

ListNode *list_pop(ListNode *head, int *out_value) {
    if (!head) return NULL;
    *out_value = head->value;
    ListNode *next = head->next;
    pool_free(&list_pool, head);
    return next;
}

/* =================== DEMOS ========================================= */

void demo1_basic_pool(void) {
    printf("=== Demo 1: Basic Pool Operations ===\n\n");

    Pool p = pool_create(sizeof(int) * 2, 8); /* 16-byte slots, 8 slots */
    pool_stats(&p, "initial");

    printf("\n  Allocating 5 slots:\n");
    void *ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = pool_alloc(&p);
        int *data = (int *)ptrs[i];
        data[0] = i * 10;
        data[1] = i * 10 + 1;
        printf("    slot %d: addr=%p, data=[%d, %d]\n",
               i, ptrs[i], data[0], data[1]);
    }
    pool_stats(&p, "after 5 allocs");

    printf("\n  Freeing slots 1 and 3:\n");
    pool_free(&p, ptrs[1]);
    pool_free(&p, ptrs[3]);
    pool_stats(&p, "after 2 frees");

    printf("\n  Allocating 2 more (reuses freed slots):\n");
    void *r1 = pool_alloc(&p);
    void *r2 = pool_alloc(&p);
    printf("    new alloc 1: %p (was slot 3: %s)\n",
           r1, r1 == ptrs[3] ? "yes, reused!" : "different");
    printf("    new alloc 2: %p (was slot 1: %s)\n",
           r2, r2 == ptrs[1] ? "yes, reused!" : "different");
    pool_stats(&p, "after realloc");

    pool_destroy(&p);
    printf("\n");
}

void demo2_embedded_free_list(void) {
    printf("=== Demo 2: Embedded Free List Internals ===\n\n");

    Pool p = pool_create(32, 5);

    printf("  Pool: %zu slots of %zu bytes\n", p.capacity, p.slot_size);
    printf("  Memory starts at: %p\n\n", (void *)p.memory);

    printf("  Free list traversal (initial):\n");
    FreeSlot *cur = p.free_head;
    int idx = 0;
    while (cur) {
        int offset = (int)((char *)cur - p.memory);
        printf("    [%d] addr=%p, offset=%d, next=%p\n",
               idx++, (void *)cur, offset, (void *)cur->next);
        cur = cur->next;
    }

    printf("\n  Each slot's 'next' pointer is stored IN the slot's\n");
    printf("  own memory — no extra metadata needed.\n");
    printf("  When allocated, user data overwrites the pointer.\n\n");

    pool_destroy(&p);
}

void demo3_bst_with_pool(void) {
    printf("=== Demo 3: BST with Pool Allocator ===\n\n");

    bst_pool = pool_create(sizeof(BSTNode), 20);
    printf("  sizeof(BSTNode) = %zu\n", sizeof(BSTNode));
    pool_stats(&bst_pool, "pool ready");

    BSTNode *root = NULL;
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 10, 35};
    int n = sizeof(keys) / sizeof(keys[0]);

    printf("\n  Inserting: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", keys[i]);
        root = bst_insert(root, keys[i]);
    }
    printf("\n");
    pool_stats(&bst_pool, "after insert");

    printf("  Inorder: ");
    bst_inorder(root);
    printf("\n\n");

    printf("  Deleting 30 (two children) and 80 (leaf):\n");
    root = bst_delete(root, 30);
    root = bst_delete(root, 80);
    pool_stats(&bst_pool, "after delete");

    printf("  Inorder: ");
    bst_inorder(root);
    printf("\n\n");

    printf("  Inserting 30 and 80 again (reuses freed slots):\n");
    root = bst_insert(root, 30);
    root = bst_insert(root, 80);
    pool_stats(&bst_pool, "after re-insert");

    printf("  Inorder: ");
    bst_inorder(root);
    printf("\n");

    bst_free_all(root);
    pool_stats(&bst_pool, "after free all");

    pool_destroy(&bst_pool);
    printf("\n");
}

void demo4_linked_list_pool(void) {
    printf("=== Demo 4: Linked List with Pool ===\n\n");

    list_pool = pool_create(sizeof(ListNode), 100);
    printf("  sizeof(ListNode) = %zu\n", sizeof(ListNode));

    ListNode *head = NULL;

    /* Push 10 elements */
    for (int i = 0; i < 10; i++)
        head = list_push(head, i * 10);

    printf("  List after 10 pushes: ");
    for (ListNode *n = head; n; n = n->next)
        printf("%d ", n->value);
    printf("\n");
    pool_stats(&list_pool, "10 nodes");

    /* Pop 5 elements */
    printf("  Popping 5: ");
    for (int i = 0; i < 5; i++) {
        int val;
        head = list_pop(head, &val);
        printf("%d ", val);
    }
    printf("\n");
    pool_stats(&list_pool, "5 nodes");

    /* Push 3 more (reuses freed slots) */
    for (int i = 0; i < 3; i++)
        head = list_push(head, 100 + i);

    printf("  After 3 more pushes: ");
    for (ListNode *n = head; n; n = n->next)
        printf("%d ", n->value);
    printf("\n");
    pool_stats(&list_pool, "8 nodes (3 reused)");

    /* Free remaining */
    while (head) {
        int val;
        head = list_pop(head, &val);
    }
    pool_stats(&list_pool, "all freed");

    pool_destroy(&list_pool);
    printf("\n");
}

void demo5_exhaustion_and_reset(void) {
    printf("=== Demo 5: Pool Exhaustion and Reset ===\n\n");

    Pool p = pool_create(16, 4); /* only 4 slots */
    pool_stats(&p, "4-slot pool");

    void *a = pool_alloc(&p);
    void *b = pool_alloc(&p);
    void *c = pool_alloc(&p);
    void *d = pool_alloc(&p);
    printf("  4 allocs: all succeeded\n");
    pool_stats(&p, "full");

    void *e = pool_alloc(&p);
    printf("  5th alloc: %s\n", e ? "SUCCESS" : "FAILED (pool exhausted)");

    printf("\n  Resetting pool (all slots returned):\n");
    pool_reset(&p);
    pool_stats(&p, "after reset");

    void *f = pool_alloc(&p);
    printf("  Alloc after reset: %s (addr=%p)\n\n",
           f ? "SUCCESS" : "FAILED", f);

    pool_destroy(&p);
}

void demo6_benchmark(void) {
    printf("=== Demo 6: Pool vs malloc Benchmark ===\n\n");

    const int N = 500000;
    typedef struct { int key; void *left, *right; } Node;

    /* malloc/free */
    clock_t start = clock();
    Node **nodes_m = (Node **)malloc(N * sizeof(Node *));
    for (int i = 0; i < N; i++)
        nodes_m[i] = (Node *)malloc(sizeof(Node));
    for (int i = 0; i < N; i++)
        free(nodes_m[i]);
    free(nodes_m);
    clock_t end = clock();
    double malloc_time = (double)(end - start) / CLOCKS_PER_SEC;

    /* Pool */
    Pool p = pool_create(sizeof(Node), N);
    start = clock();
    void **nodes_p = (void **)malloc(N * sizeof(void *));
    for (int i = 0; i < N; i++)
        nodes_p[i] = pool_alloc(&p);
    for (int i = 0; i < N; i++)
        pool_free(&p, nodes_p[i]);
    free(nodes_p);
    end = clock();
    double pool_time = (double)(end - start) / CLOCKS_PER_SEC;

    printf("  %d alloc + free of %zu-byte nodes:\n\n", N, sizeof(Node));
    printf("    malloc/free: %.4f s\n", malloc_time);
    printf("    pool:        %.4f s\n", pool_time);
    if (pool_time > 0)
        printf("    speedup:     %.1fx\n", malloc_time / pool_time);

    printf("\n  Pool advantage: O(1) alloc/free, no headers,\n");
    printf("  no fragmentation, better cache locality.\n\n");

    pool_destroy(&p);
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_basic_pool();
    demo2_embedded_free_list();
    demo3_bst_with_pool();
    demo4_linked_list_pool();
    demo5_exhaustion_and_reset();
    demo6_benchmark();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * pool_allocator.rs
 *
 * Pool (fixed-size) allocator with embedded free list.
 * Demonstrates BST and linked list using pool allocation.
 *
 * Compile: rustc -O -o pool_allocator pool_allocator.rs
 * Run:     ./pool_allocator
 */

use std::time::Instant;

/* =================== POOL ALLOCATOR ================================ */

struct Pool {
    memory: Vec<u8>,
    free_head: Option<usize>,  /* offset of first free slot */
    slot_size: usize,
    capacity: usize,
    used: usize,
}

impl Pool {
    fn new(slot_size: usize, capacity: usize) -> Self {
        let slot_size = std::cmp::max((slot_size + 7) & !7, 8);
        let mut memory = vec![0u8; slot_size * capacity];

        /* Build free list: store next-offset in each slot */
        for i in 0..capacity {
            let offset = i * slot_size;
            let next: usize = if i + 1 < capacity {
                (i + 1) * slot_size
            } else {
                usize::MAX /* sentinel for NULL */
            };
            memory[offset..offset + 8]
                .copy_from_slice(&next.to_ne_bytes());
        }

        Pool {
            memory,
            free_head: Some(0),
            slot_size,
            capacity,
            used: 0,
        }
    }

    fn alloc(&mut self) -> Option<usize> {
        let offset = self.free_head?;

        /* Read next pointer from the free slot */
        let mut buf = [0u8; 8];
        buf.copy_from_slice(&self.memory[offset..offset + 8]);
        let next = usize::from_ne_bytes(buf);

        self.free_head = if next == usize::MAX { None } else { Some(next) };
        self.used += 1;
        Some(offset)
    }

    fn free(&mut self, offset: usize) {
        /* Write current free_head into the slot being freed */
        let next = self.free_head.unwrap_or(usize::MAX);
        self.memory[offset..offset + 8]
            .copy_from_slice(&next.to_ne_bytes());
        self.free_head = Some(offset);
        self.used -= 1;
    }

    fn reset(&mut self) {
        for i in 0..self.capacity {
            let offset = i * self.slot_size;
            let next: usize = if i + 1 < self.capacity {
                (i + 1) * self.slot_size
            } else {
                usize::MAX
            };
            self.memory[offset..offset + 8]
                .copy_from_slice(&next.to_ne_bytes());
        }
        self.free_head = Some(0);
        self.used = 0;
    }

    fn read<T: Copy>(&self, offset: usize) -> T {
        assert!(std::mem::size_of::<T>() <= self.slot_size);
        unsafe {
            std::ptr::read(self.memory.as_ptr().add(offset) as *const T)
        }
    }

    fn write<T: Copy>(&mut self, offset: usize, value: T) {
        assert!(std::mem::size_of::<T>() <= self.slot_size);
        unsafe {
            std::ptr::write(self.memory.as_mut_ptr().add(offset) as *mut T, value);
        }
    }

    fn stats(&self, label: &str) {
        println!("  [{}] slot_size={}, capacity={}, used={}, free={}",
                 label, self.slot_size, self.capacity,
                 self.used, self.capacity - self.used);
    }
}

/* =================== BST WITH POOL ================================= */

const NULL_REF: usize = usize::MAX;

#[derive(Clone, Copy)]
struct BSTNode {
    key: i32,
    left: usize,   /* offset in pool, or NULL_REF */
    right: usize,
}

struct PoolBST {
    pool: Pool,
    root: usize,
}

impl PoolBST {
    fn new(capacity: usize) -> Self {
        PoolBST {
            pool: Pool::new(std::mem::size_of::<BSTNode>(), capacity),
            root: NULL_REF,
        }
    }

    fn alloc_node(&mut self, key: i32) -> usize {
        let off = self.pool.alloc().expect("pool exhausted");
        self.pool.write(off, BSTNode { key, left: NULL_REF, right: NULL_REF });
        off
    }

    fn node(&self, off: usize) -> BSTNode {
        self.pool.read::<BSTNode>(off)
    }

    fn set_node(&mut self, off: usize, node: BSTNode) {
        self.pool.write(off, node);
    }

    fn insert(&mut self, key: i32) {
        self.root = self.insert_at(self.root, key);
    }

    fn insert_at(&mut self, off: usize, key: i32) -> usize {
        if off == NULL_REF {
            return self.alloc_node(key);
        }
        let node = self.node(off);
        if key < node.key {
            let new_left = self.insert_at(node.left, key);
            let mut n = self.node(off);
            n.left = new_left;
            self.set_node(off, n);
        } else if key > node.key {
            let new_right = self.insert_at(node.right, key);
            let mut n = self.node(off);
            n.right = new_right;
            self.set_node(off, n);
        }
        off
    }

    fn delete(&mut self, key: i32) {
        self.root = self.delete_at(self.root, key);
    }

    fn delete_at(&mut self, off: usize, key: i32) -> usize {
        if off == NULL_REF { return NULL_REF; }
        let node = self.node(off);

        if key < node.key {
            let new_left = self.delete_at(node.left, key);
            let mut n = self.node(off);
            n.left = new_left;
            self.set_node(off, n);
            off
        } else if key > node.key {
            let new_right = self.delete_at(node.right, key);
            let mut n = self.node(off);
            n.right = new_right;
            self.set_node(off, n);
            off
        } else {
            if node.left == NULL_REF {
                self.pool.free(off);
                return node.right;
            }
            if node.right == NULL_REF {
                self.pool.free(off);
                return node.left;
            }
            /* Two children: find inorder successor */
            let succ_off = self.find_min(node.right);
            let succ = self.node(succ_off);
            let new_right = self.delete_at(node.right, succ.key);
            let mut n = self.node(off);
            n.key = succ.key;
            n.right = new_right;
            self.set_node(off, n);
            off
        }
    }

    fn find_min(&self, mut off: usize) -> usize {
        loop {
            let node = self.node(off);
            if node.left == NULL_REF { return off; }
            off = node.left;
        }
    }

    fn inorder(&self) -> Vec<i32> {
        let mut result = Vec::new();
        self.inorder_at(self.root, &mut result);
        result
    }

    fn inorder_at(&self, off: usize, result: &mut Vec<i32>) {
        if off == NULL_REF { return; }
        let node = self.node(off);
        self.inorder_at(node.left, result);
        result.push(node.key);
        self.inorder_at(node.right, result);
    }
}

/* =================== DEMOS ========================================= */

fn demo1_basic_pool() {
    println!("=== Demo 1: Basic Pool Operations ===\n");

    let mut pool = Pool::new(16, 8);
    pool.stats("initial");

    println!("\n  Allocating 5 slots:");
    let mut slots = Vec::new();
    for i in 0..5 {
        let off = pool.alloc().unwrap();
        pool.write::<[i32; 2]>(off, [i * 10, i * 10 + 1]);
        let data = pool.read::<[i32; 2]>(off);
        println!("    slot {}: offset={}, data={:?}", i, off, data);
        slots.push(off);
    }
    pool.stats("after 5 allocs");

    println!("\n  Freeing slots 1 and 3:");
    pool.free(slots[1]);
    pool.free(slots[3]);
    pool.stats("after 2 frees");

    println!("\n  Allocating 2 more (reuses freed slots):");
    let r1 = pool.alloc().unwrap();
    let r2 = pool.alloc().unwrap();
    println!("    new 1: offset={} (was slot 3: {})",
             r1, if r1 == slots[3] { "reused!" } else { "different" });
    println!("    new 2: offset={} (was slot 1: {})",
             r2, if r2 == slots[1] { "reused!" } else { "different" });
    pool.stats("after realloc");
    println!();
}

fn demo2_free_list_internals() {
    println!("=== Demo 2: Free List Internals ===\n");

    let pool = Pool::new(32, 5);

    println!("  Pool: {} slots of {} bytes\n", pool.capacity, pool.slot_size);
    println!("  Free list traversal:");

    let mut cur = pool.free_head;
    let mut idx = 0;
    while let Some(offset) = cur {
        let mut buf = [0u8; 8];
        buf.copy_from_slice(&pool.memory[offset..offset + 8]);
        let next_raw = usize::from_ne_bytes(buf);
        let next = if next_raw == usize::MAX { None } else { Some(next_raw) };

        println!("    [{}] offset={:>3}, next={:?}", idx, offset, next);
        cur = next;
        idx += 1;
    }

    println!("\n  The 'next' pointer lives inside the slot itself.");
    println!("  Zero overhead — no headers, no metadata.\n");
}

fn demo3_bst_with_pool() {
    println!("=== Demo 3: BST with Pool Allocator ===\n");

    let mut bst = PoolBST::new(20);
    println!("  sizeof(BSTNode) = {}", std::mem::size_of::<BSTNode>());
    bst.pool.stats("pool ready");

    let keys = [50, 30, 70, 20, 40, 60, 80, 10, 35];
    print!("\n  Inserting: ");
    for &k in &keys {
        print!("{} ", k);
        bst.insert(k);
    }
    println!();
    bst.pool.stats("after insert");

    println!("  Inorder: {:?}\n", bst.inorder());

    println!("  Deleting 30 and 80:");
    bst.delete(30);
    bst.delete(80);
    bst.pool.stats("after delete");
    println!("  Inorder: {:?}\n", bst.inorder());

    println!("  Re-inserting 30 and 80 (reuses freed slots):");
    bst.insert(30);
    bst.insert(80);
    bst.pool.stats("after re-insert");
    println!("  Inorder: {:?}\n", bst.inorder());
}

fn demo4_linked_list_pool() {
    println!("=== Demo 4: Linked List with Pool ===\n");

    #[derive(Clone, Copy)]
    struct ListNode { value: i32, next: usize }

    let mut pool = Pool::new(std::mem::size_of::<ListNode>(), 100);
    let mut head = NULL_REF;

    /* Push 10 */
    for i in 0..10 {
        let off = pool.alloc().unwrap();
        pool.write(off, ListNode { value: i * 10, next: head });
        head = off;
    }

    print!("  List: ");
    let mut cur = head;
    while cur != NULL_REF {
        let node = pool.read::<ListNode>(cur);
        print!("{} ", node.value);
        cur = node.next;
    }
    println!();
    pool.stats("10 nodes");

    /* Pop 5 */
    print!("  Popping 5: ");
    for _ in 0..5 {
        let node = pool.read::<ListNode>(head);
        print!("{} ", node.value);
        let next = node.next;
        pool.free(head);
        head = next;
    }
    println!();
    pool.stats("5 nodes");

    /* Push 3 more */
    for i in 0..3 {
        let off = pool.alloc().unwrap();
        pool.write(off, ListNode { value: 100 + i, next: head });
        head = off;
    }

    print!("  After 3 pushes: ");
    cur = head;
    while cur != NULL_REF {
        let node = pool.read::<ListNode>(cur);
        print!("{} ", node.value);
        cur = node.next;
    }
    println!();
    pool.stats("8 nodes (reused slots)");
    println!();
}

fn demo5_exhaustion_reset() {
    println!("=== Demo 5: Pool Exhaustion and Reset ===\n");

    let mut pool = Pool::new(16, 4);
    pool.stats("4-slot pool");

    for _ in 0..4 { pool.alloc().unwrap(); }
    println!("  4 allocs: all succeeded");
    pool.stats("full");

    let result = pool.alloc();
    println!("  5th alloc: {}",
             if result.is_some() { "SUCCESS" } else { "FAILED (exhausted)" });

    pool.reset();
    pool.stats("after reset");

    let result = pool.alloc();
    println!("  Alloc after reset: {}\n",
             if result.is_some() { "SUCCESS" } else { "FAILED" });
}

fn demo6_benchmark() {
    println!("=== Demo 6: Pool vs Box Benchmark ===\n");

    let n = 500_000usize;

    #[derive(Clone, Copy)]
    struct Node { key: i32, left: usize, right: usize }

    /* Box (simulates malloc/free) */
    let start = Instant::now();
    let mut boxes: Vec<Box<Node>> = Vec::with_capacity(n);
    for i in 0..n {
        boxes.push(Box::new(Node {
            key: i as i32, left: 0, right: 0,
        }));
    }
    drop(boxes);
    let box_time = start.elapsed();

    /* Pool */
    let mut pool = Pool::new(std::mem::size_of::<Node>(), n);
    let start = Instant::now();
    let mut offsets: Vec<usize> = Vec::with_capacity(n);
    for i in 0..n {
        let off = pool.alloc().unwrap();
        pool.write(off, Node { key: i as i32, left: 0, right: 0 });
        offsets.push(off);
    }
    for &off in &offsets {
        pool.free(off);
    }
    let pool_time = start.elapsed();

    println!("  {} alloc + free of {}-byte nodes:\n", n,
             std::mem::size_of::<Node>());
    println!("    Box (heap):  {:.4} s", box_time.as_secs_f64());
    println!("    Pool:        {:.4} s", pool_time.as_secs_f64());
    if pool_time.as_nanos() > 0 {
        println!("    Speedup:     {:.1}x",
                 box_time.as_secs_f64() / pool_time.as_secs_f64());
    }
    println!();
}

fn demo7_multiple_pools() {
    println!("=== Demo 7: Multiple Pools for Different Types ===\n");

    #[derive(Clone, Copy, Debug)]
    struct Vertex { x: f32, y: f32, z: f32 }

    #[derive(Clone, Copy, Debug)]
    struct Edge { from: usize, to: usize, weight: f32 }

    let mut vertex_pool = Pool::new(std::mem::size_of::<Vertex>(), 100);
    let mut edge_pool = Pool::new(std::mem::size_of::<Edge>(), 200);

    println!("  Vertex pool: slot_size={}", vertex_pool.slot_size);
    println!("  Edge pool:   slot_size={}\n", edge_pool.slot_size);

    /* Create a small graph */
    let v: Vec<usize> = (0..4).map(|i| {
        let off = vertex_pool.alloc().unwrap();
        vertex_pool.write(off, Vertex {
            x: i as f32, y: (i * 2) as f32, z: 0.0,
        });
        off
    }).collect();

    let edges = [(0,1,1.0f32), (1,2,2.0), (2,3,3.0), (3,0,4.0)];
    let e: Vec<usize> = edges.iter().map(|&(f, t, w)| {
        let off = edge_pool.alloc().unwrap();
        edge_pool.write(off, Edge { from: v[f], to: v[t], weight: w });
        off
    }).collect();

    println!("  Graph: {} vertices, {} edges", v.len(), e.len());
    vertex_pool.stats("vertices");
    edge_pool.stats("edges");

    /* Delete an edge */
    edge_pool.free(e[1]);
    println!("\n  After removing edge 1:");
    edge_pool.stats("edges");

    /* Each type has its own pool — zero cross-contamination. */
    println!("\n  Separate pools: zero fragmentation per type.\n");
}

fn demo8_pool_vs_arena() {
    println!("=== Demo 8: Pool vs Arena — When to Use Each ===\n");

    println!("  Scenario 1: Parse tokens (all freed together)");
    println!("    -> Arena is ideal: batch lifetime, O(1) reset.\n");

    println!("  Scenario 2: BST nodes (insert/delete individually)");
    println!("    -> Pool is ideal: O(1) alloc/free, same size.\n");

    /* Demonstrate the problem with arena for individual free */
    println!("  Arena problem with individual free:");

    struct SimpleArena { buf: Vec<u8>, offset: usize }
    impl SimpleArena {
        fn new(cap: usize) -> Self { SimpleArena { buf: vec![0u8; cap], offset: 0 } }
        fn alloc(&mut self, size: usize) -> usize {
            let off = (self.offset + 7) & !7;
            self.offset = off + size;
            off
        }
        fn used(&self) -> usize { self.offset }
    }

    let mut arena = SimpleArena::new(1024);
    let mut offsets = Vec::new();
    for _ in 0..10 {
        offsets.push(arena.alloc(24));
    }

    println!("    Arena: 10 nodes allocated, {} bytes used", arena.used());
    println!("    Want to free node 5? Can't! Arena doesn't support it.");
    println!("    Memory grows until full reset.\n");

    println!("  Pool solves this:");
    let mut pool = Pool::new(24, 100);
    let mut pool_offs = Vec::new();
    for _ in 0..10 {
        pool_offs.push(pool.alloc().unwrap());
    }
    pool.stats("10 nodes");
    pool.free(pool_offs[5]);
    pool.stats("freed node 5 — slot reusable");
    println!();
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_basic_pool();
    demo2_free_list_internals();
    demo3_bst_with_pool();
    demo4_linked_list_pool();
    demo5_exhaustion_reset();
    demo6_benchmark();
    demo7_multiple_pools();
    demo8_pool_vs_arena();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Tamano minimo de slot

El pool usa una free list embebida donde cada slot libre almacena un
puntero `next`. En una plataforma de 64 bits, cual es el tamano minimo
de slot? Que pasa si los objetos son de 4 bytes?

<details><summary>Prediccion</summary>

El tamano minimo de slot es **8 bytes** (`sizeof(void*)` en 64-bit),
porque el puntero `next` ocupa 8 bytes.

Si los objetos son de 4 bytes (por ejemplo, un `int`):
- El slot debe ser de al menos 8 bytes.
- Se desperdician 4 bytes por slot: **50% de fragmentacion interna**.

Alternativa: usar **indices** de 4 bytes en lugar de punteros de 8:
```c
typedef union Slot {
    uint32_t next_index;  /* 4 bytes */
    int data;             /* 4 bytes */
} Slot;
```

Esto funciona si el pool tiene menos de $2^{32}$ slots (4 mil millones),
lo cual es casi siempre cierto. Con indices de 2 bytes ($2^{16} = 65536$
slots max), el overhead se reduce a 2 bytes.

</details>

### Ejercicio 2 — Capacidad del pool

Un pool de 1 MB almacena nodos de BST:
```c
struct BSTNode { int key; BSTNode *left, *right; };
```

En 64-bit, `sizeof(BSTNode)` = ? Cuantos nodos caben? Comparar con
malloc (header de 16 bytes por bloque).

<details><summary>Prediccion</summary>

`sizeof(BSTNode)` en 64-bit:
- `int key`: 4 bytes + 4 padding (alineamiento a 8).
- `BSTNode *left`: 8 bytes.
- `BSTNode *right`: 8 bytes.
- Total: **24 bytes**.

Con slot alineado a 8: slot_size = 24 (ya alineado).

**Pool**: $\lfloor 1{,}048{,}576 / 24 \rfloor = 43{,}690$ nodos.

**malloc**: cada nodo consume $24 + 16 = 40$ bytes.
$\lfloor 1{,}048{,}576 / 40 \rfloor = 26{,}214$ nodos.

**El pool almacena 67% mas nodos** que malloc en el mismo espacio
($43{,}690 / 26{,}214 = 1.67\times$).

Ademas: con pool, los nodos estan contiguos en memoria → mejor localidad
de cache al recorrer el arbol.

</details>

### Ejercicio 3 — Free list despues de operaciones

Pool de 6 slots (offsets 0, 24, 48, 72, 96, 120). Secuencia:

```
a = alloc()  -> ?
b = alloc()  -> ?
c = alloc()  -> ?
free(b)
d = alloc()  -> ?
free(a)
free(d)
e = alloc()  -> ?
```

Traza el estado de la free list despues de cada operacion.

<details><summary>Prediccion</summary>

Inicial: `free: 0 -> 24 -> 48 -> 72 -> 96 -> 120 -> NULL`

```
a = alloc(): pop 0.    free: 24 -> 48 -> 72 -> 96 -> 120
b = alloc(): pop 24.   free: 48 -> 72 -> 96 -> 120
c = alloc(): pop 48.   free: 72 -> 96 -> 120

free(b=24): push 24.   free: 24 -> 72 -> 96 -> 120

d = alloc(): pop 24.   free: 72 -> 96 -> 120

free(a=0): push 0.     free: 0 -> 72 -> 96 -> 120
free(d=24): push 24.   free: 24 -> 0 -> 72 -> 96 -> 120

e = alloc(): pop 24.   free: 0 -> 72 -> 96 -> 120
```

Resultado: `a=0, b=24, c=48, d=24 (reuso!), e=24 (reuso otra vez!)`.

La free list actua como un **stack** (LIFO): el ultimo liberado es el
primero en re-alocarse. Esto maximiza la localidad temporal — el slot
24 se reuso dos veces porque fue liberado recientemente.

</details>

### Ejercicio 4 — Pool con bitmap

Disena un pool alternativo que use un **bitmap** en lugar de free list
embebida. Compara las complejidades de alloc y free.

<details><summary>Prediccion</summary>

**Estructura**:
```c
struct PoolBitmap {
    char *memory;
    uint64_t *bitmap;    /* 1 bit per slot: 0=free, 1=used */
    size_t slot_size;
    size_t capacity;
    size_t bitmap_words;  /* ceil(capacity / 64) */
};
```

**alloc**: buscar el primer bit 0 en el bitmap.
```c
for (int w = 0; w < bitmap_words; w++) {
    if (bitmap[w] != ~0ULL) {  /* hay un 0 en este word */
        int bit = __builtin_ctzll(~bitmap[w]); /* first zero */
        bitmap[w] |= (1ULL << bit);
        return memory + (w * 64 + bit) * slot_size;
    }
}
return NULL;
```

**free**: apagar el bit.
```c
int index = (ptr - memory) / slot_size;
bitmap[index / 64] &= ~(1ULL << (index % 64));
```

**Complejidades**:

| Operacion | Free list | Bitmap |
|-----------|:---------:|:------:|
| alloc | $O(1)$ | $O(N/64)$ peor, $O(1)$ amortizado |
| free | $O(1)$ | $O(1)$ |
| init | $O(N)$ | $O(N/64)$ |
| iteracion sobre usados | $O(N)$ con flag | $O(N/64)$ con popcount |

**Ventaja del bitmap**: iterar sobre slots usados es rapido (skip
words con todos los bits en 0). Util para destruccion masiva o GC.

**Ventaja de la free list**: alloc es estrictamente $O(1)$ siempre.

**En la practica**: el bitmap es mas lento para alloc pero mas versatil.
Los slab allocators del kernel usan bitmaps para paginas parcialmente
llenas.

</details>

### Ejercicio 5 — Pool growable

Disena un pool que pueda crecer cuando se agota. Que pasa con los
punteros existentes cuando se anade un nuevo chunk?

<details><summary>Prediccion</summary>

**Diseno**: lista enlazada de chunks, cada uno con su propia memoria:

```c
struct PoolChunk {
    char *memory;
    size_t capacity;
    struct PoolChunk *next;
};

struct GrowPool {
    struct PoolChunk *chunks;
    FreeSlot *free_head;  /* unified free list across all chunks */
    size_t slot_size;
    size_t total_capacity;
};
```

Cuando `alloc()` encuentra la free list vacia:
1. Crear un nuevo chunk (por ejemplo, con el doble de slots).
2. Encadenar los slots del nuevo chunk en la free list.
3. Continuar con alloc.

**Punteros existentes**: los punteros a slots en chunks anteriores
**siguen siendo validos** porque cada chunk es un `malloc` independiente.
No se mueve memoria existente (a diferencia de `realloc`).

Esto es critico: si usaramos un solo array y `realloc`, los punteros
existentes se invalidarian. Con chunks separados, el crecimiento es
**seguro para punteros existentes**.

**Desventaja**: los slots de diferentes chunks no son contiguos →
peor localidad de cache comparado con un pool de un solo bloque.

</details>

### Ejercicio 6 — Slab allocator del kernel

El kernel de Linux tiene un cache de inodes (`inode_cache`) con
`sizeof(struct inode) ≈ 600` bytes y paginas de 4096 bytes. Cuantos
inodes caben por pagina? Cual es la fragmentacion interna?

<details><summary>Prediccion</summary>

Inodes por pagina: $\lfloor 4096 / 600 \rfloor = 6$ inodes.
Espacio usado: $6 \times 600 = 3600$ bytes.
Desperdicio: $4096 - 3600 = 496$ bytes (12.1%).

Ese espacio de 496 bytes se usa para:
- Metadata del slab (puntero al cache, lista de slabs, bitmap de slots).
- Coloring (desplazamiento variable del primer objeto para distribuir
  lineas de cache entre diferentes slabs).

**Con 2 paginas** (8192 bytes):
$\lfloor 8192 / 600 \rfloor = 13$ inodes. Usado: $13 \times 600 = 7800$.
Desperdicio: 392 bytes (4.8%). Mejor utilizacion.

El kernel elige el numero de paginas por slab para minimizar el
desperdicio. Para inodes, probablemente usa 1-2 paginas dependiendo
de la version y configuracion.

</details>

### Ejercicio 7 — Pool para tabla hash

Una tabla hash con encadenamiento tiene $M = 1024$ buckets. Cada nodo:
```c
struct HashNode { char key[16]; int value; struct HashNode *next; };
```

Se esperan $N = 10000$ entradas. Disena el pool y calcula la memoria.

<details><summary>Prediccion</summary>

`sizeof(HashNode)` en 64-bit:
- `char key[16]`: 16 bytes.
- `int value`: 4 bytes + 4 padding.
- `HashNode *next`: 8 bytes.
- Total: **32 bytes**.

**Pool**: 10000 slots de 32 bytes.
Memoria del pool: $10000 \times 32 = 320{,}000$ bytes = 312.5 KB.

**Tabla de buckets**: $1024 \times 8 = 8{,}192$ bytes = 8 KB
(array de punteros).

**Total**: 320 KB + 8 KB = **328 KB**.

**Con malloc**:
- Nodos: $10000 \times (32 + 16) = 480{,}000$ bytes = 469 KB.
- Buckets: 8 KB.
- Total: **477 KB**.

**Ahorro del pool**: $477 - 328 = 149$ KB (31% menos memoria).

Ademas, con pool: insercion y eliminacion en $O(1)$ para la allocacion
(la busqueda en la cadena sigue siendo $O(n/M)$). Sin fragmentacion
aunque se inserten y eliminen miles de entradas.

</details>

### Ejercicio 8 — LIFO vs FIFO free list

El pool clasico usa una free list **LIFO** (stack). Que pasaria si
usaramos **FIFO** (queue)? Analiza el impacto en localidad de cache.

<details><summary>Prediccion</summary>

**LIFO** (stack): el ultimo slot liberado es el primero re-alocado.

```
free(slot A) -> A al frente
free(slot B) -> B al frente
alloc()      -> retorna B (el mas reciente)
```

**Ventaja LIFO**: el slot B probablemente esta **caliente en cache** —
fue accedido recientemente. Re-alocarlo inmediatamente tiene buena
localidad temporal.

**FIFO** (queue): el primer slot liberado es el primero re-alocado.

```
free(slot A) -> A al final
free(slot B) -> B al final
alloc()      -> retorna A (el mas antiguo)
```

**Desventaja FIFO**: el slot A fue liberado hace mas tiempo, asi que
probablemente fue **expulsado del cache**. Re-alocarlo causa un cache
miss.

**Ventaja FIFO**: distribuye el uso mas uniformemente entre slots →
desgaste uniforme (relevante para memoria flash). Tambien puede ayudar
a detectar use-after-free: si un slot se reusa despues de mucho tiempo,
la corrupcion es mas obvia.

**Implementacion FIFO**: necesita un puntero extra (`tail`) para insertar
al final en $O(1)$:
```c
struct { FreeSlot *head, *tail; }
```

**En la practica**: LIFO es mejor para rendimiento general (cache).
FIFO se usa en allocators de debug para detectar bugs.

</details>

### Ejercicio 9 — Pool y concurrencia

Dos threads comparten un pool. Thread A hace `alloc()` y thread B
hace `free()` simultaneamente. Describe la condicion de carrera y
dos soluciones.

<details><summary>Prediccion</summary>

**Condicion de carrera**:
```
free_head = slot_5 -> slot_8 -> slot_12 -> ...

Thread A (alloc):              Thread B (free slot_3):
  read free_head = slot_5        read free_head = slot_5
  new_head = slot_5->next = 8   slot_3->next = slot_5
  write free_head = slot_8      write free_head = slot_3

Estado final: free_head = slot_3 -> slot_5 -> slot_8 -> ...
Pero Thread A tiene slot_5, y slot_5 sigue en la free list!
-> Double use: slot_5 puede asignarse a dos usuarios.
```

**Solucion 1: Mutex**
```c
pthread_mutex_lock(&pool->lock);
// alloc o free
pthread_mutex_unlock(&pool->lock);
```
Simple, correcto, pero con contension bajo carga alta.

**Solucion 2: Lock-free con CAS**
```c
void *pool_alloc_lockfree(Pool *p) {
    FreeSlot *head;
    do {
        head = atomic_load(&p->free_head);
        if (!head) return NULL;
    } while (!atomic_compare_exchange_weak(
        &p->free_head, &head, head->next));
    return head;
}
```

Problema: **ABA**: entre la lectura de `head` y el CAS, otro thread
podria haber hecho alloc(head) + free(head) → head esta de vuelta
pero `head->next` cambio. Solucion: tagged pointer (contador de version
en los bits altos del puntero).

**Solucion 3: Pool per-thread** (la mas practica)
Cada thread tiene su propio pool. Sin contension, sin atomics.
Periodicamente, un thread puede "robar" slots de otro pool si se agota.

</details>

### Ejercicio 10 — Pool vs arena decision tree

Para cada escenario, decide si usar arena, pool, o malloc general.
Justifica.

1. Compilador: nodos de IR para una funcion.
2. Servidor de juego: entidades con spawn/despawn individual.
3. JSON parser: tokens y nodos del arbol.
4. Base de datos: registros de cache de tamano variable.
5. Sistema de particulas: 10000 particulas identicas.

<details><summary>Prediccion</summary>

1. **Compilador (IR por funcion)** → **Arena**.
   Todos los nodos de IR se crean al analizar la funcion y se descartan
   al terminar la emision de codigo. Lifetime uniforme, tamanos variados
   (diferentes tipos de nodos IR). Reset al final de cada funcion.

2. **Servidor de juego (entidades)** → **Pool**.
   Las entidades nacen y mueren individualmente. Todas tienen el mismo
   struct (`Entity`). El pool permite alloc/free en $O(1)$ sin
   fragmentacion. Si hay subtipos de diferente tamano: un pool por
   subtipo.

3. **JSON parser** → **Arena**.
   Mismo patron que el compilador: construir un arbol durante el parsing,
   procesar el resultado, descartar todo. Lifetime uniforme.

4. **Base de datos (cache de tamano variable)** → **malloc general**
   (o jemalloc).
   Los registros tienen tamanos variados y lifetimes individuales.
   No son candidatos para pool (tamanos diferentes) ni arena (lifetimes
   diferentes). Un allocator general con size classes es lo apropiado.

5. **Sistema de particulas** → **Pool**.
   Todas las particulas son del mismo tipo y tamano. Nacen y mueren
   individualmente cada frame. Pool con pre-allocacion de 10000 slots.
   Si todas mueren al final del frame: arena tambien funciona.

**Regla de decision**:
- Mismo lifetime + tamano variable → Arena
- Lifetime individual + mismo tamano → Pool
- Lifetime individual + tamano variable → malloc general

</details>
