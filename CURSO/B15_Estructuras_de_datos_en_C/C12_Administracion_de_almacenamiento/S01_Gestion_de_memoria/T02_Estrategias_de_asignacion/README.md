# T02 — Estrategias de asignacion

## Objetivo

Comprender e implementar las cuatro estrategias clasicas de busqueda en la
**free list** — **first fit**, **best fit**, **worst fit** y **next fit** —,
analizar sus tradeoffs en terminos de **fragmentacion**, **velocidad** y
**utilizacion de memoria**, y razonar sobre cuando elegir cada una.

---

## 1. El problema de la seleccion de bloque

Cuando `malloc(n)` necesita satisfacer una peticion, el allocator recorre la
**free list** buscando un bloque libre de tamano $\geq n$. Si hay multiples
bloques que satisfacen la peticion, la **estrategia de seleccion** determina
cual se elige. Esta decision tiene consecuencias profundas:

- **Fragmentacion externa**: huecos inutilizables entre bloques usados.
- **Velocidad de busqueda**: cuantos bloques recorremos antes de decidir.
- **Distribucion de tamanos**: que tan bien conservamos bloques grandes para
  peticiones futuras.

### 1.1 Modelo formal

Sea $F = \{b_1, b_2, \ldots, b_k\}$ el conjunto de bloques libres, donde
$\text{size}(b_i)$ es el tamano del payload de $b_i$. Para una peticion de
tamano $n$, definimos el conjunto de **bloques candidatos**:

$$C(n) = \{b_i \in F \mid \text{size}(b_i) \geq n\}$$

Si $C(n) = \emptyset$, la peticion falla (se necesita mas memoria del OS).
Si $|C(n)| \geq 1$, la estrategia elige un $b^* \in C(n)$.

### 1.2 Desperdicio por bloque

Al asignar un bloque $b^*$ con $\text{size}(b^*) > n$, el **desperdicio**
es:

$$w = \text{size}(b^*) - n - H$$

donde $H$ es el tamano del header. Si $w \geq H + \text{MIN\_BLOCK}$, se
realiza **splitting** (crear un nuevo bloque libre con el sobrante). Si
$w < H + \text{MIN\_BLOCK}$, el sobrante se desperdicia como **fragmentacion
interna**.

---

## 2. First Fit

### 2.1 Algoritmo

Recorrer la free list **desde el inicio** y retornar el **primer** bloque
con $\text{size} \geq n$.

```
first_fit(n):
    for b in free_list (from head):
        if b.size >= n:
            return b
    return NULL
```

### 2.2 Propiedades

| Propiedad | Valor |
|-----------|-------|
| **Complejidad** | $O(k)$ peor caso, pero frecuentemente $O(1)$ amortizado |
| **Fragmentacion** | Tiende a acumular bloques pequenos al inicio |
| **Localidad** | Buena — reutiliza bloques cercanos al inicio |

### 2.3 Analisis

First fit tiene un fenomeno conocido como **acumulacion al inicio**
(*front-end accumulation*): los bloques al inicio de la free list se fragmentan
repetidamente porque siempre son los primeros en ser divididos. Esto genera
muchos bloques pequenos al inicio y bloques grandes al final.

**Ventaja**: si la peticion cabe en un bloque temprano, la busqueda termina
rapido. En la practica, first fit es sorprendentemente bueno — estudios
empiricos (Knuth, 1973; Wilson et al., 1995) muestran que rara vez supera
al best fit en mas de unos pocos puntos porcentuales de utilizacion.

---

## 3. Best Fit

### 3.1 Algoritmo

Recorrer **toda** la free list y retornar el bloque con el **menor** tamano
que satisfaga la peticion: el bloque cuyo $\text{size}(b) - n$ es minimo.

```
best_fit(n):
    best = NULL
    for b in free_list:
        if b.size >= n:
            if best == NULL or b.size < best.size:
                best = b
    return best
```

### 3.2 Propiedades

| Propiedad | Valor |
|-----------|-------|
| **Complejidad** | $O(k)$ siempre (debe recorrer toda la lista) |
| **Fragmentacion** | Minimiza desperdicio por peticion, pero genera fragmentos diminutos |
| **Localidad** | Variable — puede saltar a cualquier posicion |

### 3.3 Analisis

Best fit produce **fragmentos residuales muy pequenos** (*slivers*) que
rara vez son utiles para peticiones futuras. Paradojicamente, estos
micro-fragmentos pueden **empeorar** la fragmentacion total a largo plazo.

Sin embargo, best fit tiende a **preservar bloques grandes**, lo cual es
valioso si el patron de peticiones incluye allocaciones grandes ocasionales.

Para un arbol balanceado o una estructura ordenada de la free list, best fit
puede ejecutarse en $O(\log k)$ en lugar de $O(k)$.

---

## 4. Worst Fit

### 4.1 Algoritmo

Retornar el bloque **mas grande** de la free list.

```
worst_fit(n):
    worst = NULL
    for b in free_list:
        if b.size >= n:
            if worst == NULL or b.size > worst.size:
                worst = b
    return worst
```

### 4.2 Propiedades

| Propiedad | Valor |
|-----------|-------|
| **Complejidad** | $O(k)$ siempre |
| **Fragmentacion** | Evita micro-fragmentos, pero agota bloques grandes rapidamente |
| **Localidad** | Pobre — siempre busca el maximo |

### 4.3 Analisis

La intuicion detras de worst fit es que al dividir el bloque mas grande,
el **residuo** tambien sera grande — y por lo tanto mas util para peticiones
futuras. Esto evita los *slivers* del best fit.

**Problema**: agota los bloques grandes rapidamente. Si llega una peticion
grande despues de muchas pequenas, puede no haber un bloque suficiente. En
la practica, worst fit es la **peor** estrategia para la mayoria de cargas
de trabajo (Shore, 1975) y rara vez se usa en allocators reales.

Con un max-heap, worst fit puede ejecutarse en $O(\log k)$.

---

## 5. Next Fit

### 5.1 Algoritmo

Identico a first fit, pero en lugar de empezar desde el **inicio** de la
free list, continua desde donde **termino la busqueda anterior**.

```
next_fit(n):
    start = last_position    // saved from previous call
    b = start
    do:
        if b.size >= n:
            last_position = next(b)
            return b
        b = next(b)
        if b == end_of_list:
            b = head_of_list  // wrap around
    while b != start
    return NULL
```

### 5.2 Propiedades

| Propiedad | Valor |
|-----------|-------|
| **Complejidad** | $O(k)$ peor caso, pero distribuye la busqueda |
| **Fragmentacion** | Distribuye fragmentacion uniformemente |
| **Localidad** | Pobre — salta por todo el heap |

### 5.3 Analisis

Next fit fue propuesto por Knuth (1973) como mejora a first fit. Al rotar
el punto de inicio, **evita la acumulacion al inicio**: los bloques al
comienzo de la free list no se fragmentan desproporcionadamente.

**Problema**: next fit tiende a **romper bloques grandes** porque no los
preserva — eventualmente el puntero rotativo los encuentra y los divide.
Estudios empiricos muestran que next fit produce **mas fragmentacion** que
first fit en la mayoria de cargas de trabajo (Johnstone & Wilson, 1998).

Next fit es popular en allocators embebidos por su simplicidad y velocidad
promedio.

---

## 6. Comparacion detallada

### 6.1 Tabla resumen

| Estrategia | Busqueda | Sobrante tipico | Preserva bloques grandes | Localidad | Uso practico |
|:----------:|:--------:|:---------------:|:------------------------:|:---------:|:------------:|
| First fit | Parcial | Medio | Si (al final) | Buena | ptmalloc2, dlmalloc |
| Best fit | Completa | Minimo | Si | Variable | jemalloc (por bins) |
| Worst fit | Completa | Maximo | No | Pobre | Raro |
| Next fit | Parcial (rotativo) | Medio | No | Pobre | Embebido |

### 6.2 Ejemplo comparativo

Consideremos una free list con estos bloques libres (en orden de direccion):

```
Free list: [A:100] [B:250] [C:60] [D:200] [E:150]
```

Peticion: `malloc(80)`.

| Estrategia | Bloque elegido | Sobrante | Razonamiento |
|:----------:|:--------------:|:--------:|:-------------|
| First fit | A (100) | 100 - 80 = 20 | Primer bloque con $\geq 80$ |
| Best fit | A (100) | 100 - 80 = 20 | Menor bloque con $\geq 80$ |
| Worst fit | B (250) | 250 - 80 = 170 | Mayor bloque |
| Next fit* | A (100) | 100 - 80 = 20 | Desde inicio (primera llamada) |

*Si la busqueda anterior termino despues de C, next fit eligiria D (200),
con sobrante 120.

Segunda peticion: `malloc(200)`.

| Estrategia | Bloque elegido | Sobrante |
|:----------:|:--------------:|:--------:|
| First fit | B (250) | 50 |
| Best fit | D (200) | 0 (fit exacto!) |
| Worst fit | B (250) | 50 |
| Next fit* | D (200) | 0 |

Best fit brilla aqui: encuentra un fit exacto, sin desperdicio ni splitting.

### 6.3 Traza extendida

Secuencia de operaciones sobre un heap de 1024 bytes:

```
Estado inicial: [FREE: 1008]  (1024 - 16 header)

1. malloc(100) -> first_fit elige [FREE:1008], split -> [USED:100][FREE:892]
2. malloc(200) -> [USED:100][USED:200][FREE:676]
3. malloc(50)  -> [USED:100][USED:200][USED:50][FREE:610]
4. free(1)     -> [FREE:100][USED:200][USED:50][FREE:610]
5. malloc(80)  -> ???
```

En el paso 5, cada estrategia elige diferente:

- **First fit**: elige el primer bloque libre [FREE:100] → sobrante 20
  (puede ser un *sliver* inutil si MIN_BLOCK > 20).
- **Best fit**: elige [FREE:100] → sobrante 20 (mismo que first fit aqui,
  porque 100 < 610).
- **Worst fit**: elige [FREE:610] → sobrante 530. Preserva el bloque de
  100 pero consume el grande.
- **Next fit**: depende del puntero. Si apunta al inicio, elige [FREE:100].

```
6. malloc(500) -> despues del paso 5:
```

- Si worst fit consumio [FREE:610], ahora tiene [FREE:530]. 530 >= 500, OK.
- Si first/best fit consumio [FREE:100], el [FREE:610] sigue intacto.
  610 >= 500, OK.

```
7. malloc(600) -> solo posible si hay un bloque >= 600:
```

- First/best fit: [FREE:610] existe → exito (sobrante 10, posible *sliver*).
- Worst fit: [FREE:530] es el maximo → 530 < 600 → **FALLA**.

Este ejemplo muestra por que worst fit es peligroso: consume bloques
grandes prematuramente.

---

## 7. Optimizaciones avanzadas

### 7.1 Segregated free lists

En lugar de una unica free list, mantener **multiples listas** organizadas
por rango de tamano (bins):

```
Bin 0: bloques de 1-8 bytes
Bin 1: bloques de 9-16 bytes
Bin 2: bloques de 17-32 bytes
...
Bin k: bloques de 2^k+1 - 2^(k+1) bytes
```

Para `malloc(n)`, ir directamente al bin correspondiente. Esto convierte
la busqueda de $O(k)$ a $O(1)$ amortizado, independiente del tamano total
de la free list. Es la tecnica usada por **ptmalloc2** (glibc), **jemalloc**
y **tcmalloc**.

### 7.2 Ordered free list

Mantener la free list **ordenada por tamano**. Best fit se reduce a
encontrar el primer bloque $\geq n$ (busqueda binaria con arbol balanceado:
$O(\log k)$). El costo es mantener el orden durante inserciones/liberaciones.

### 7.3 Address-ordered free list

Mantener la free list ordenada por **direccion** (no por tamano). Ventaja:
el coalescing es trivial (el vecino en la lista es el vecino en memoria).
First fit sobre una lista ordenada por direccion produce resultados
similares a best fit en muchos workloads (Wilson et al., 1995).

### 7.4 Bitmaps

Para bloques de tamano fijo (slab/pool allocators), un **bitmap** indica
que slots estan libres. `malloc` = encontrar el primer bit 0 (instruccion
hardware `CTZ`/`BSF` en $O(1)$). No hay fragmentacion porque todos los
bloques son del mismo tamano.

---

## 8. Programa completo en C

```c
/*
 * allocation_strategies.c
 *
 * Implements and compares first fit, best fit, worst fit,
 * and next fit allocation strategies on a simulated heap.
 *
 * Compile: gcc -O0 -o allocation_strategies allocation_strategies.c
 * Run:     ./allocation_strategies
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* =================== SIMULATED HEAP ================================ */

#define HEAP_SIZE 1024
#define HEADER_SIZE sizeof(BlockHeader)
#define MIN_BLOCK 8
#define ALIGN 8
#define ALIGN_UP(x) (((x) + (ALIGN - 1)) & ~(ALIGN - 1))

typedef struct BlockHeader {
    size_t size;    /* payload size (aligned) */
    bool free;
} BlockHeader;

typedef enum {
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT,
    NEXT_FIT
} Strategy;

const char *strategy_name(Strategy s) {
    switch (s) {
        case FIRST_FIT: return "First Fit";
        case BEST_FIT:  return "Best Fit";
        case WORST_FIT: return "Worst Fit";
        case NEXT_FIT:  return "Next Fit";
    }
    return "Unknown";
}

typedef struct {
    char heap[HEAP_SIZE];
    int last_offset;  /* for next fit */
} Heap;

static BlockHeader *get_block(Heap *h, int offset) {
    return (BlockHeader *)(h->heap + offset);
}

static int next_offset(BlockHeader *b, int offset) {
    return offset + (int)HEADER_SIZE + (int)b->size;
}

void heap_init(Heap *h) {
    BlockHeader *first = (BlockHeader *)h->heap;
    first->size = HEAP_SIZE - HEADER_SIZE;
    first->free = true;
    h->last_offset = 0;
}

void heap_dump(Heap *h, const char *label) {
    printf("  [%s]\n", label);
    int offset = 0;
    int idx = 0;
    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *b = get_block(h, offset);
        if (b->size == 0) break;
        printf("    #%d: offset=%3d, size=%3zu, %s\n",
               idx++, offset, b->size, b->free ? "FREE" : "USED");
        offset = next_offset(b, offset);
    }
}

/* =================== STRATEGY IMPLEMENTATIONS ===================== */

void *heap_alloc(Heap *h, size_t size, Strategy strategy) {
    size = ALIGN_UP(size);
    if (size == 0) size = ALIGN;

    BlockHeader *chosen = NULL;
    int chosen_offset = -1;
    int offset = 0;
    int start_offset = 0;
    bool wrapped = false;

    /* For next fit, start from last position */
    if (strategy == NEXT_FIT) {
        start_offset = h->last_offset;
        offset = start_offset;
    }

    while (1) {
        if (offset + (int)HEADER_SIZE > HEAP_SIZE) {
            if (strategy == NEXT_FIT && !wrapped) {
                offset = 0;
                wrapped = true;
                if (offset == start_offset) break;
                continue;
            }
            break;
        }

        BlockHeader *b = get_block(h, offset);
        if (b->size == 0) {
            if (strategy == NEXT_FIT && !wrapped) {
                offset = 0;
                wrapped = true;
                if (offset == start_offset) break;
                continue;
            }
            break;
        }

        if (b->free && b->size >= size) {
            switch (strategy) {
                case FIRST_FIT:
                    chosen = b;
                    chosen_offset = offset;
                    goto done;

                case BEST_FIT:
                    if (!chosen || b->size < chosen->size) {
                        chosen = b;
                        chosen_offset = offset;
                    }
                    break;

                case WORST_FIT:
                    if (!chosen || b->size > chosen->size) {
                        chosen = b;
                        chosen_offset = offset;
                    }
                    break;

                case NEXT_FIT:
                    chosen = b;
                    chosen_offset = offset;
                    goto done;
            }
        }

        offset = next_offset(b, offset);

        /* Next fit: wrap around check */
        if (strategy == NEXT_FIT && wrapped && offset >= start_offset) {
            break;
        }
    }

done:
    if (!chosen) return NULL;

    /* Split if possible */
    if (chosen->size >= size + HEADER_SIZE + MIN_BLOCK) {
        int new_off = chosen_offset + (int)HEADER_SIZE + (int)size;
        BlockHeader *new_block = get_block(h, new_off);
        new_block->size = chosen->size - size - HEADER_SIZE;
        new_block->free = true;
        chosen->size = size;
    }

    chosen->free = false;

    /* Update next fit pointer */
    if (strategy == NEXT_FIT) {
        h->last_offset = next_offset(chosen, chosen_offset);
        if (h->last_offset + (int)HEADER_SIZE > HEAP_SIZE) {
            h->last_offset = 0;
        }
    }

    return h->heap + chosen_offset + HEADER_SIZE;
}

void heap_free(Heap *h, void *ptr) {
    if (!ptr) return;
    BlockHeader *b = (BlockHeader *)((char *)ptr - HEADER_SIZE);
    b->free = true;

    int offset = (int)((char *)b - h->heap);

    /* Coalesce forward */
    int noff = next_offset(b, offset);
    if (noff + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *nxt = get_block(h, noff);
        if (nxt->size > 0 && nxt->free) {
            b->size += HEADER_SIZE + nxt->size;
        }
    }

    /* Coalesce backward (linear scan) */
    BlockHeader *prev = NULL;
    int poff = -1;
    int cur_off = 0;
    while (cur_off < offset) {
        BlockHeader *cur = get_block(h, cur_off);
        if (cur->size == 0) break;
        prev = cur;
        poff = cur_off;
        cur_off = next_offset(cur, cur_off);
    }
    if (prev && prev->free) {
        prev->size += HEADER_SIZE + b->size;
    }
}

/* Statistics */
void heap_stats(Heap *h, int *free_blocks, size_t *free_bytes,
                int *used_blocks, size_t *used_bytes, size_t *largest_free) {
    *free_blocks = 0; *free_bytes = 0;
    *used_blocks = 0; *used_bytes = 0;
    *largest_free = 0;
    int offset = 0;

    while (offset + (int)HEADER_SIZE <= HEAP_SIZE) {
        BlockHeader *b = get_block(h, offset);
        if (b->size == 0) break;
        if (b->free) {
            (*free_blocks)++;
            *free_bytes += b->size;
            if (b->size > *largest_free) *largest_free = b->size;
        } else {
            (*used_blocks)++;
            *used_bytes += b->size;
        }
        offset = next_offset(b, offset);
    }
}

void print_stats(Heap *h) {
    int fb, ub;
    size_t fby, uby, lf;
    heap_stats(h, &fb, &fby, &ub, &uby, &lf);
    printf("    Free: %d blocks, %zu bytes (largest=%zu)\n", fb, fby, lf);
    printf("    Used: %d blocks, %zu bytes\n", ub, uby);
}

/* =================== DEMOS ========================================= */

void demo1_first_fit(void) {
    printf("=== Demo 1: First Fit ===\n\n");

    Heap h;
    heap_init(&h);

    printf("Allocating: 100, 200, 50 bytes\n");
    void *a = heap_alloc(&h, 100, FIRST_FIT);
    void *b = heap_alloc(&h, 200, FIRST_FIT);
    void *c = heap_alloc(&h, 50, FIRST_FIT);
    heap_dump(&h, "after 3 allocs");

    printf("\nFree block at offset %d (size 100)\n",
           (int)((char *)a - h.heap - HEADER_SIZE));
    heap_free(&h, a);
    heap_dump(&h, "after free(100)");

    printf("\nmalloc(80) with first fit: takes the first hole (100)\n");
    void *d = heap_alloc(&h, 80, FIRST_FIT);
    printf("  Returned offset: %d\n", (int)((char *)d - h.heap));
    heap_dump(&h, "after malloc(80)");
    print_stats(&h);
    printf("\n");
}

void demo2_best_fit(void) {
    printf("=== Demo 2: Best Fit ===\n\n");

    Heap h;
    heap_init(&h);

    /* Create holes of different sizes */
    void *a = heap_alloc(&h, 100, BEST_FIT);
    void *b = heap_alloc(&h, 50, BEST_FIT);
    void *c = heap_alloc(&h, 200, BEST_FIT);
    void *d = heap_alloc(&h, 80, BEST_FIT);
    void *e = heap_alloc(&h, 30, BEST_FIT);

    /* Free alternating blocks to create holes */
    heap_free(&h, a);  /* hole of 100 */
    heap_free(&h, c);  /* hole of 200 */
    heap_free(&h, e);  /* hole of 30 + remainder */
    heap_dump(&h, "holes: 100, 200, 30+rest");

    printf("\nmalloc(90) with best fit:\n");
    printf("  Candidates: 100, 200, 30+rest\n");
    printf("  Best fit chooses 100 (smallest >= 90)\n");
    void *f = heap_alloc(&h, 90, BEST_FIT);
    printf("  Returned offset: %d\n", (int)((char *)f - h.heap));
    heap_dump(&h, "after best_fit(90)");
    print_stats(&h);
    printf("\n");
}

void demo3_worst_fit(void) {
    printf("=== Demo 3: Worst Fit ===\n\n");

    Heap h;
    heap_init(&h);

    void *a = heap_alloc(&h, 100, WORST_FIT);
    void *b = heap_alloc(&h, 50, WORST_FIT);
    void *c = heap_alloc(&h, 200, WORST_FIT);
    void *d = heap_alloc(&h, 80, WORST_FIT);

    heap_free(&h, a);  /* hole of 100 */
    heap_free(&h, c);  /* hole of 200 */
    heap_dump(&h, "holes: 100 and 200");

    printf("\nmalloc(90) with worst fit:\n");
    printf("  Candidates: 100, 200, remainder\n");
    printf("  Worst fit chooses largest block\n");
    void *e = heap_alloc(&h, 90, WORST_FIT);
    printf("  Returned offset: %d\n", (int)((char *)e - h.heap));
    heap_dump(&h, "after worst_fit(90)");
    printf("\n  Note: the 100-byte hole was preserved,\n");
    printf("  but the large block was consumed.\n");
    print_stats(&h);
    printf("\n");
}

void demo4_next_fit(void) {
    printf("=== Demo 4: Next Fit ===\n\n");

    Heap h;
    heap_init(&h);

    /* Allocate several blocks */
    void *a = heap_alloc(&h, 80, NEXT_FIT);
    void *b = heap_alloc(&h, 80, NEXT_FIT);
    void *c = heap_alloc(&h, 80, NEXT_FIT);
    void *d = heap_alloc(&h, 80, NEXT_FIT);

    /* Free 1st and 3rd */
    heap_free(&h, a);
    heap_free(&h, c);
    heap_dump(&h, "freed 1st and 3rd (two 80-byte holes)");

    printf("\n  last_offset = %d (after block d)\n", h.last_offset);

    printf("\nmalloc(40) with next fit:\n");
    printf("  Starts scanning from last_offset, wraps around.\n");
    void *e = heap_alloc(&h, 40, NEXT_FIT);
    printf("  Returned offset: %d\n", (int)((char *)e - h.heap));
    heap_dump(&h, "after next_fit(40)");

    printf("\nmalloc(40) with next fit (second call):\n");
    printf("  Continues from where last search ended.\n");
    void *f = heap_alloc(&h, 40, NEXT_FIT);
    printf("  Returned offset: %d\n", (int)((char *)f - h.heap));
    heap_dump(&h, "after second next_fit(40)");
    print_stats(&h);
    printf("\n");
}

void demo5_comparison_trace(void) {
    printf("=== Demo 5: Side-by-Side Comparison ===\n\n");

    Strategy strategies[] = {FIRST_FIT, BEST_FIT, WORST_FIT, NEXT_FIT};

    /* Same sequence of operations on each strategy */
    size_t alloc_sizes[] = {120, 64, 200, 32, 80, 150};
    int free_indices[] = {0, 2, 4};  /* free the 1st, 3rd, 5th allocs */
    size_t final_alloc = 100;

    printf("Sequence: alloc 120, 64, 200, 32, 80, 150\n");
    printf("          free  1st, 3rd, 5th\n");
    printf("          alloc 100 -> which block?\n\n");

    for (int s = 0; s < 4; s++) {
        Heap h;
        heap_init(&h);

        void *ptrs[6];
        for (int i = 0; i < 6; i++) {
            ptrs[i] = heap_alloc(&h, alloc_sizes[i], strategies[s]);
        }

        for (int i = 0; i < 3; i++) {
            heap_free(&h, ptrs[free_indices[i]]);
        }

        /* Show state before final alloc */
        int fb;
        size_t fby, lf;
        int ub;
        size_t uby;
        heap_stats(&h, &fb, &fby, &ub, &uby, &lf);

        void *result = heap_alloc(&h, final_alloc, strategies[s]);
        int result_off = result ? (int)((char *)result - h.heap) : -1;

        printf("  %-10s: malloc(100) -> offset=%3d | free_blocks=%d, "
               "free_bytes=%3zu, largest=%3zu\n",
               strategy_name(strategies[s]), result_off, fb, fby, lf);
    }
    printf("\n");
}

void demo6_fragmentation_stress(void) {
    printf("=== Demo 6: Fragmentation Stress Test ===\n\n");

    Strategy strategies[] = {FIRST_FIT, BEST_FIT, WORST_FIT, NEXT_FIT};

    printf("Stress test: alternating alloc/free of varied sizes.\n");
    printf("  Phase 1: alloc 10 blocks of sizes 16,32,48,...,160\n");
    printf("  Phase 2: free even-indexed blocks (0,2,4,6,8)\n");
    printf("  Phase 3: alloc 5 blocks of size 24\n");
    printf("  Measure: remaining fragmentation.\n\n");

    printf("  %-10s  %5s  %5s  %5s  %5s\n",
           "Strategy", "Free", "FrBlk", "Used", "LgFr");

    for (int s = 0; s < 4; s++) {
        Heap h;
        heap_init(&h);

        /* Phase 1: allocate 10 blocks */
        void *blocks[10];
        for (int i = 0; i < 10; i++) {
            blocks[i] = heap_alloc(&h, (i + 1) * 16, strategies[s]);
        }

        /* Phase 2: free even-indexed */
        for (int i = 0; i < 10; i += 2) {
            heap_free(&h, blocks[i]);
        }

        /* Phase 3: allocate 5 blocks of 24 */
        for (int i = 0; i < 5; i++) {
            heap_alloc(&h, 24, strategies[s]);
        }

        int fb, ub;
        size_t fby, uby, lf;
        heap_stats(&h, &fb, &fby, &ub, &uby, &lf);
        printf("  %-10s  %5zu  %5d  %5zu  %5zu\n",
               strategy_name(strategies[s]), fby, fb, uby, lf);
    }
    printf("\n  Lower free bytes and fewer free blocks = better utilization.\n");
    printf("  Larger 'largest free' = better ability to serve big allocs.\n\n");
}

/* =================== MAIN ========================================== */

int main(void) {
    demo1_first_fit();
    demo2_best_fit();
    demo3_worst_fit();
    demo4_next_fit();
    demo5_comparison_trace();
    demo6_fragmentation_stress();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * allocation_strategies.rs
 *
 * Implements and compares first fit, best fit, worst fit,
 * and next fit allocation strategies on a simulated heap.
 *
 * Compile: rustc -o allocation_strategies allocation_strategies.rs
 * Run:     ./allocation_strategies
 */

use std::fmt;

/* =================== HEAP SIMULATION =============================== */

const HEAP_SIZE: usize = 1024;
const HEADER_SIZE: usize = 16; /* size: 8 + free_flag: 8 */
const MIN_BLOCK: usize = 8;
const ALIGNMENT: usize = 8;

fn align_up(x: usize) -> usize {
    (x + ALIGNMENT - 1) & !(ALIGNMENT - 1)
}

#[derive(Clone, Copy, PartialEq)]
enum Strategy {
    FirstFit,
    BestFit,
    WorstFit,
    NextFit,
}

impl fmt::Display for Strategy {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Strategy::FirstFit => write!(f, "First Fit"),
            Strategy::BestFit  => write!(f, "Best Fit"),
            Strategy::WorstFit => write!(f, "Worst Fit"),
            Strategy::NextFit  => write!(f, "Next Fit"),
        }
    }
}

struct SimHeap {
    data: [u8; HEAP_SIZE],
    last_offset: usize,  /* for next fit */
}

impl SimHeap {
    fn new() -> Self {
        let mut h = SimHeap {
            data: [0u8; HEAP_SIZE],
            last_offset: 0,
        };
        h.write_header(0, HEAP_SIZE - HEADER_SIZE, true);
        h
    }

    fn write_header(&mut self, offset: usize, size: usize, free: bool) {
        let bytes = size.to_ne_bytes();
        self.data[offset..offset + 8].copy_from_slice(&bytes);
        let flag = if free { 1usize } else { 0usize };
        let fbytes = flag.to_ne_bytes();
        self.data[offset + 8..offset + 16].copy_from_slice(&fbytes);
    }

    fn read_size(&self, offset: usize) -> usize {
        let mut buf = [0u8; 8];
        buf.copy_from_slice(&self.data[offset..offset + 8]);
        usize::from_ne_bytes(buf)
    }

    fn read_free(&self, offset: usize) -> bool {
        let mut buf = [0u8; 8];
        buf.copy_from_slice(&self.data[offset + 8..offset + 16]);
        usize::from_ne_bytes(buf) != 0
    }

    fn next_offset(&self, offset: usize) -> usize {
        offset + HEADER_SIZE + self.read_size(offset)
    }

    fn alloc(&mut self, size: usize, strategy: Strategy) -> Option<usize> {
        let size = align_up(if size == 0 { ALIGNMENT } else { size });

        let mut chosen: Option<(usize, usize)> = None; /* (offset, block_size) */
        let mut offset = if strategy == Strategy::NextFit {
            self.last_offset
        } else {
            0
        };
        let start = offset;
        let mut wrapped = false;

        loop {
            if offset + HEADER_SIZE > HEAP_SIZE {
                if strategy == Strategy::NextFit && !wrapped {
                    offset = 0;
                    wrapped = true;
                    if offset == start { break; }
                    continue;
                }
                break;
            }

            let bsize = self.read_size(offset);
            if bsize == 0 {
                if strategy == Strategy::NextFit && !wrapped {
                    offset = 0;
                    wrapped = true;
                    if offset == start { break; }
                    continue;
                }
                break;
            }

            let bfree = self.read_free(offset);

            if bfree && bsize >= size {
                match strategy {
                    Strategy::FirstFit | Strategy::NextFit => {
                        chosen = Some((offset, bsize));
                        break;
                    }
                    Strategy::BestFit => {
                        if chosen.is_none() || bsize < chosen.unwrap().1 {
                            chosen = Some((offset, bsize));
                        }
                    }
                    Strategy::WorstFit => {
                        if chosen.is_none() || bsize > chosen.unwrap().1 {
                            chosen = Some((offset, bsize));
                        }
                    }
                }
            }

            offset = self.next_offset(offset);

            if strategy == Strategy::NextFit && wrapped && offset >= start {
                break;
            }
        }

        let (chosen_off, chosen_size) = chosen?;

        /* Split if possible */
        if chosen_size >= size + HEADER_SIZE + MIN_BLOCK {
            let new_off = chosen_off + HEADER_SIZE + size;
            let remainder = chosen_size - size - HEADER_SIZE;
            self.write_header(new_off, remainder, true);
            self.write_header(chosen_off, size, false);
        } else {
            self.write_header(chosen_off, chosen_size, false);
        }

        /* Update next fit pointer */
        if strategy == Strategy::NextFit {
            self.last_offset = self.next_offset(chosen_off);
            if self.last_offset + HEADER_SIZE > HEAP_SIZE {
                self.last_offset = 0;
            }
        }

        Some(chosen_off + HEADER_SIZE)
    }

    fn free(&mut self, ptr: usize) {
        let offset = ptr - HEADER_SIZE;
        let size = self.read_size(offset);
        self.write_header(offset, size, true);

        /* Coalesce forward */
        let noff = self.next_offset(offset);
        if noff + HEADER_SIZE <= HEAP_SIZE {
            let nsize = self.read_size(noff);
            if nsize > 0 && self.read_free(noff) {
                let merged = size + HEADER_SIZE + nsize;
                self.write_header(offset, merged, true);
            }
        }

        /* Coalesce backward (linear scan) */
        let final_size = self.read_size(offset);
        let mut prev_off: Option<usize> = None;
        let mut cur = 0;
        while cur < offset {
            let cs = self.read_size(cur);
            if cs == 0 { break; }
            prev_off = Some(cur);
            cur = cur + HEADER_SIZE + cs;
        }
        if let Some(po) = prev_off {
            if self.read_free(po) {
                let psize = self.read_size(po);
                let merged = psize + HEADER_SIZE + final_size;
                self.write_header(po, merged, true);
            }
        }
    }

    fn dump(&self, label: &str) {
        println!("  [{}]", label);
        let mut offset = 0;
        let mut idx = 0;
        while offset + HEADER_SIZE <= HEAP_SIZE {
            let size = self.read_size(offset);
            if size == 0 { break; }
            let free = self.read_free(offset);
            println!("    #{}: offset={:>3}, size={:>3}, {}",
                     idx, offset, size, if free { "FREE" } else { "USED" });
            idx += 1;
            offset = self.next_offset(offset);
        }
    }

    fn stats(&self) -> (usize, usize, usize, usize, usize) {
        /* (free_blocks, free_bytes, used_blocks, used_bytes, largest_free) */
        let (mut fb, mut fby, mut ub, mut uby, mut lf) = (0, 0, 0, 0, 0);
        let mut offset = 0;
        while offset + HEADER_SIZE <= HEAP_SIZE {
            let size = self.read_size(offset);
            if size == 0 { break; }
            if self.read_free(offset) {
                fb += 1;
                fby += size;
                if size > lf { lf = size; }
            } else {
                ub += 1;
                uby += size;
            }
            offset = self.next_offset(offset);
        }
        (fb, fby, ub, uby, lf)
    }
}

/* =================== DEMOS ========================================= */

fn demo1_first_fit() {
    println!("=== Demo 1: First Fit ===\n");

    let mut h = SimHeap::new();

    println!("Allocating: 100, 200, 50 bytes");
    let a = h.alloc(100, Strategy::FirstFit).unwrap();
    let _b = h.alloc(200, Strategy::FirstFit).unwrap();
    let _c = h.alloc(50, Strategy::FirstFit).unwrap();
    h.dump("after 3 allocs");

    println!("\nFreeing first block (100 bytes)");
    h.free(a);
    h.dump("after free(100)");

    println!("\nmalloc(80): first fit takes the first hole (was 100)");
    let d = h.alloc(80, Strategy::FirstFit).unwrap();
    println!("  Returned: payload offset {}", d);
    h.dump("after first_fit(80)");

    let (fb, fby, _, _, _) = h.stats();
    println!("  Free: {} blocks, {} bytes\n", fb, fby);
}

fn demo2_best_fit() {
    println!("=== Demo 2: Best Fit ===\n");

    let mut h = SimHeap::new();

    let a = h.alloc(100, Strategy::BestFit).unwrap();
    let b = h.alloc(50, Strategy::BestFit).unwrap();
    let c = h.alloc(200, Strategy::BestFit).unwrap();
    let d = h.alloc(80, Strategy::BestFit).unwrap();
    let e = h.alloc(30, Strategy::BestFit).unwrap();

    h.free(a);
    h.free(c);
    h.free(e);
    h.dump("holes: 100, 200, 30+rest");

    println!("\nmalloc(90): best fit chooses 100 (tightest fit)");
    let f = h.alloc(90, Strategy::BestFit).unwrap();
    println!("  Returned: payload offset {}", f);
    h.dump("after best_fit(90)");

    let (fb, fby, _, _, lf) = h.stats();
    println!("  Free: {} blocks, {} bytes, largest={}\n", fb, fby, lf);
}

fn demo3_worst_fit() {
    println!("=== Demo 3: Worst Fit ===\n");

    let mut h = SimHeap::new();

    let a = h.alloc(100, Strategy::WorstFit).unwrap();
    let b = h.alloc(50, Strategy::WorstFit).unwrap();
    let c = h.alloc(200, Strategy::WorstFit).unwrap();
    let _d = h.alloc(80, Strategy::WorstFit).unwrap();

    h.free(a);
    h.free(c);
    h.dump("holes: 100 and 200 + remainder");

    println!("\nmalloc(90): worst fit chooses the LARGEST block");
    let e = h.alloc(90, Strategy::WorstFit).unwrap();
    println!("  Returned: payload offset {}", e);
    h.dump("after worst_fit(90)");

    println!("  Note: the 100-byte hole is preserved, but");
    println!("  the large block was consumed.\n");
}

fn demo4_next_fit() {
    println!("=== Demo 4: Next Fit ===\n");

    let mut h = SimHeap::new();

    let a = h.alloc(80, Strategy::NextFit).unwrap();
    let _b = h.alloc(80, Strategy::NextFit).unwrap();
    let c = h.alloc(80, Strategy::NextFit).unwrap();
    let _d = h.alloc(80, Strategy::NextFit).unwrap();

    h.free(a);
    h.free(c);
    h.dump("freed 1st and 3rd (two 80-byte holes)");
    println!("  last_offset = {}", h.last_offset);

    println!("\nFirst next_fit(40): wraps from last_offset");
    let _e = h.alloc(40, Strategy::NextFit).unwrap();
    h.dump("after first next_fit(40)");
    println!("  last_offset = {}", h.last_offset);

    println!("\nSecond next_fit(40): continues from new position");
    let _f = h.alloc(40, Strategy::NextFit).unwrap();
    h.dump("after second next_fit(40)");
    println!();
}

fn demo5_comparison_trace() {
    println!("=== Demo 5: Side-by-Side Comparison ===\n");

    let strategies = [
        Strategy::FirstFit, Strategy::BestFit,
        Strategy::WorstFit, Strategy::NextFit,
    ];
    let alloc_sizes = [120usize, 64, 200, 32, 80, 150];
    let free_indices = [0usize, 2, 4];

    println!("Sequence: alloc 120, 64, 200, 32, 80, 150");
    println!("          free  1st, 3rd, 5th");
    println!("          alloc 100 -> which block?\n");

    for &strat in &strategies {
        let mut h = SimHeap::new();
        let mut ptrs = Vec::new();

        for &sz in &alloc_sizes {
            ptrs.push(h.alloc(sz, strat));
        }

        for &idx in &free_indices {
            if let Some(ptr) = ptrs[idx] {
                h.free(ptr);
            }
        }

        let (fb, fby, _, _, lf) = h.stats();
        let result = h.alloc(100, strat);
        let roff = result.map(|p| p as isize).unwrap_or(-1);

        println!("  {:>10}: malloc(100) -> offset={:>3} | free_blocks={}, \
                  free_bytes={:>3}, largest={:>3}",
                 strat, roff, fb, fby, lf);
    }
    println!();
}

fn demo6_fragmentation_stress() {
    println!("=== Demo 6: Fragmentation Stress Test ===\n");

    let strategies = [
        Strategy::FirstFit, Strategy::BestFit,
        Strategy::WorstFit, Strategy::NextFit,
    ];

    println!("Phase 1: alloc 10 blocks (16,32,48,...,160)");
    println!("Phase 2: free even-indexed (0,2,4,6,8)");
    println!("Phase 3: alloc 5 blocks of size 24\n");

    println!("  {:>10}  {:>5}  {:>5}  {:>5}  {:>5}",
             "Strategy", "Free", "FrBlk", "Used", "LgFr");

    for &strat in &strategies {
        let mut h = SimHeap::new();
        let mut ptrs = Vec::new();

        for i in 0..10 {
            ptrs.push(h.alloc((i + 1) * 16, strat));
        }

        for i in (0..10).step_by(2) {
            if let Some(ptr) = ptrs[i] {
                h.free(ptr);
            }
        }

        for _ in 0..5 {
            h.alloc(24, strat);
        }

        let (fb, fby, _, uby, lf) = h.stats();
        println!("  {:>10}  {:>5}  {:>5}  {:>5}  {:>5}",
                 strat, fby, fb, uby, lf);
    }
    println!("\n  Lower free bytes / fewer free blocks = better utilization.");
    println!("  Larger 'largest free' = better for future big allocs.\n");
}

fn demo7_exact_fit_advantage() {
    println!("=== Demo 7: Best Fit — Exact Fit Advantage ===\n");

    let mut h = SimHeap::new();

    /* Create blocks and free some to produce specific hole sizes */
    let a = h.alloc(64, Strategy::BestFit).unwrap();
    let _b = h.alloc(32, Strategy::BestFit).unwrap();
    let c = h.alloc(128, Strategy::BestFit).unwrap();
    let _d = h.alloc(32, Strategy::BestFit).unwrap();
    let e = h.alloc(96, Strategy::BestFit).unwrap();

    h.free(a);   /* hole = 64 */
    h.free(c);   /* hole = 128 */
    h.free(e);   /* hole = 96 + rest */
    h.dump("holes: 64, 128, 96+rest");

    println!("\nAllocating exactly 64 with best fit:");
    let f = h.alloc(64, Strategy::BestFit).unwrap();
    println!("  Returned: offset {} (exact fit, no split!)", f);
    h.dump("after best_fit(64)");

    println!("  An exact fit means zero waste and no split overhead.\n");

    println!("Allocating exactly 128 with best fit:");
    let g = h.alloc(128, Strategy::BestFit).unwrap();
    println!("  Returned: offset {} (another exact fit!)", g);
    h.dump("after best_fit(128)");

    let (fb, fby, _, _, _) = h.stats();
    println!("  Remaining: {} free blocks, {} free bytes\n", fb, fby);
}

fn demo8_strategy_distribution() {
    println!("=== Demo 8: Allocation Distribution Map ===\n");

    let strategies = [
        Strategy::FirstFit, Strategy::BestFit,
        Strategy::WorstFit, Strategy::NextFit,
    ];

    /* Many small allocations followed by frees and re-allocs */
    println!("20 allocs of size 32, free odd-indexed, re-alloc 10 of size 24.");
    println!("Show where each strategy places the re-allocations.\n");

    for &strat in &strategies {
        let mut h = SimHeap::new();
        let mut ptrs = Vec::new();

        for _ in 0..20 {
            ptrs.push(h.alloc(32, strat));
        }

        /* Free odd-indexed blocks */
        for i in (1..20).step_by(2) {
            if let Some(ptr) = ptrs[i] {
                h.free(ptr);
            }
        }

        /* Re-allocate 10 blocks of 24 */
        let mut new_offsets = Vec::new();
        for _ in 0..10 {
            if let Some(off) = h.alloc(24, strat) {
                new_offsets.push(off);
            }
        }

        let (fb, fby, ub, _, lf) = h.stats();

        /* Show first 5 offsets to see placement pattern */
        let shown: Vec<String> = new_offsets.iter()
            .take(5)
            .map(|o| format!("{}", o))
            .collect();

        println!("  {:>10}: offsets=[{}...] free_blocks={}, free={}, largest={}",
                 strat, shown.join(", "), fb, fby, lf);
    }
    println!();
}

/* =================== MAIN ========================================== */

fn main() {
    demo1_first_fit();
    demo2_best_fit();
    demo3_worst_fit();
    demo4_next_fit();
    demo5_comparison_trace();
    demo6_fragmentation_stress();
    demo7_exact_fit_advantage();
    demo8_strategy_distribution();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Seleccion manual

Free list (en orden de direccion): `[A:120] [B:50] [C:200] [D:80] [E:160]`.
Peticion: `malloc(100)`. Indica que bloque elige cada estrategia y el sobrante.

<details><summary>Prediccion</summary>

| Estrategia | Bloque | Sobrante |
|:----------:|:------:|:--------:|
| First fit | A (120) | 120 - 100 = 20 |
| Best fit | A (120) | 120 - 100 = 20 |
| Worst fit | C (200) | 200 - 100 = 100 |
| Next fit* | A (120) | 20 (si es primera llamada) |

*Next fit depende del estado del puntero rotativo. Si la busqueda anterior
termino despues de B, next fit elige C (200), sobrante 100.

First fit y best fit coinciden aqui porque A es tanto el primero como el
menor candidato. Worst fit deja un residuo de 100 (util) pero consume el
bloque mas grande.

</details>

### Ejercicio 2 — Secuencia adversa para worst fit

Disena una secuencia de `malloc` y `free` sobre un heap de 512 bytes donde
worst fit falla en una peticion que first fit si satisface.

<details><summary>Prediccion</summary>

```
Heap: 512 bytes (496 payload + 16 header)

1. a = malloc(200)  ->  [USED:200][FREE:280]
2. b = malloc(100)  ->  [USED:200][USED:100][FREE:164]
3. free(a)          ->  [FREE:200][USED:100][FREE:164]
```

Ahora `malloc(150)`:

- **First fit**: elige [FREE:200] (primer bloque >= 150). Sobrante = 50. **Exito**.
- **Worst fit**: elige [FREE:200] (es el mayor de 200 vs 164). Sobrante = 34. **Exito**.

Ajustemos para que worst fit falle:

```
1. a = malloc(200)  ->  [USED:200][FREE:280]
2. b = malloc(80)   ->  [USED:200][USED:80][FREE:184]
3. free(a)          ->  [FREE:200][USED:80][FREE:184]
4. c = malloc(60) con worst fit  -> elige [FREE:200], sobrante 124
                        [USED:60][FREE:124][USED:80][FREE:184]
5. d = malloc(60) con worst fit  -> elige [FREE:184], sobrante 108
                        [USED:60][FREE:124][USED:80][USED:60][FREE:108]
6. e = malloc(60) con worst fit  -> elige [FREE:124], sobrante 48
                        [USED:60][USED:60][FREE:48][USED:80][USED:60][FREE:108]
```

Ahora `malloc(120)`:
- **Worst fit**: mayor bloque = 108 < 120. **FALLA**.
- **First fit** (en heap sin worst fit): tras pasos 1-3, [FREE:200] disponible.
  Si first fit hubiera hecho pasos 4-6, habria elegido [FREE:200] en paso 4,
  despues [FREE:184] en paso 5, despues [FREE:108] en paso 6 — mismo resultado.

La clave es que worst fit disperso las allocaciones en bloques grandes,
fragmentandolos todos, mientras que first fit tiende a agotar un bloque antes
de pasar al siguiente.

</details>

### Ejercicio 3 — Best fit y slivers

Con free list `[A:104] [B:500]`, se hace `malloc(100)` con best fit.
Asumiendo header de 16 bytes y MIN_BLOCK de 8:

1. Que bloque elige?
2. Hay splitting?
3. Es util el sobrante?

<details><summary>Prediccion</summary>

1. **Best fit elige A (104)** porque $104 - 100 = 4 < 500 - 100 = 400$.

2. Sobrante = $104 - 100 = 4$. Para hacer split necesitamos sobrante
   $\geq$ HEADER_SIZE + MIN_BLOCK = $16 + 8 = 24$. Como $4 < 24$, **no hay
   splitting**.

3. Los 4 bytes sobrantes se convierten en **fragmentacion interna**: estan
   dentro del bloque asignado pero no son usados por el usuario. Son
   **inutiles** — no pueden satisfacer ninguna peticion futura.

   Si hubieramos elegido B (500), el sobrante seria $500 - 100 - 16 = 384$,
   un bloque libre perfectamente util. Best fit "ahorro" 4 bytes de
   sobrante pero creo un *sliver* irrecuperable.

   Este es el caso patologico del best fit: prioriza minimizar el
   desperdicio inmediato a costa de generar fragmentos diminutos.

</details>

### Ejercicio 4 — Next fit vs first fit

Free list: `[A:60] [B:80] [C:40] [D:100] [E:70]`.
Se ejecutan tres `malloc` consecutivos de 50 bytes cada uno.
Compara el comportamiento de first fit vs next fit.

<details><summary>Prediccion</summary>

**First fit** (siempre desde el inicio):
1. `malloc(50)`: elige A (60), sobrante = $60 - 50 = 10$ (no split si 10 < 24).
   Entrega A completo (60 bytes, 10 de fragmentacion interna).
2. `malloc(50)`: elige B (80), split: [USED:50][FREE:14]. O sin split si
   $80 - 50 - 16 = 14 < 8$... $14 \geq 8$, asi que se hace split.
   Resultado: [USED:50][FREE:14].
3. `malloc(50)`: C (40) es muy pequeno, salta. D (100) >= 50. Split:
   [USED:50][FREE:34].

Bloques usados: A(60), parte de B(50), parte de D(50). Tres busquedas
empezaron desde el inicio, revisando A cada vez.

**Next fit** (puntero rotativo):
1. `malloc(50)`: puntero en A. Elige A (60, sin split). Puntero -> B.
2. `malloc(50)`: puntero en B. Elige B (80). Split: [USED:50][FREE:14].
   Puntero -> C.
3. `malloc(50)`: puntero en C. C (40) < 50. D (100) >= 50. Elige D.
   Split: [USED:50][FREE:34]. Puntero -> E.

Mismo resultado en bloques elegidos, pero next fit no re-examino A en las
llamadas 2 y 3 — cada busqueda fue mas corta. La diferencia se amplifica
con listas largas.

</details>

### Ejercicio 5 — Segregated free lists

Explica como las **segregated free lists** convierten best fit en $O(1)$.
Si los bins son [1-8], [9-16], [17-32], ..., [257-512], en que bin cae
`malloc(100)` y por que es rapido?

<details><summary>Prediccion</summary>

**Mecanismo**: en lugar de una free list unica, mantenemos una **lista por
rango de tamano** (bin). Cada bin contiene bloques cuyo tamano cae en un
rango especifico, generalmente potencias de 2.

Bins: [1-8], [9-16], [17-32], [33-64], [65-128], [129-256], [257-512].

`malloc(100)` cae en el bin **[65-128]**. El allocator:
1. Va directamente al bin [65-128] — $O(1)$.
2. Si hay bloques, toma el primero — $O(1)$ (o busca el mejor si quiere).
3. Si el bin esta vacio, busca en el siguiente bin [129-256] — $O(1)$ amortizado.

**Por que es rapido**: no recorremos toda la free list. El indice del bin se
calcula con una operacion de bits:
$\text{bin} = \lfloor \log_2(n) \rfloor$, que en hardware es una
instruccion `BSR`/`CLZ` (1 ciclo).

**Por que es buen fit**: todos los bloques en el bin [65-128] tienen tamano
entre 65 y 128. Para una peticion de 100, el peor desperdicio es
$128 - 100 = 28$ bytes (no $500 - 100 = 400$ como con una lista unica).

ptmalloc2 usa 128 bins: 64 *small bins* (tamano exacto, incrementos de 8)
y 64 *large bins* (rangos, ordenados por tamano dentro del bin).

</details>

### Ejercicio 6 — Analisis asintotico

Para una free list de $k$ bloques, completa la tabla de complejidad de
busqueda:

| Estrategia | Lista simple | Lista ordenada por tamano | Arbol balanceado |
|:----------:|:------------:|:------------------------:|:----------------:|
| First fit | ? | ? | ? |
| Best fit | ? | ? | ? |
| Worst fit | ? | ? | ? |
| Next fit | ? | ? | ? |

<details><summary>Prediccion</summary>

| Estrategia | Lista simple | Lista ordenada por tamano | Arbol balanceado |
|:----------:|:------------:|:------------------------:|:----------------:|
| First fit | $O(k)$ | $O(k)$* | $O(k)$* |
| Best fit | $O(k)$ | $O(k)$** | $O(\log k)$ |
| Worst fit | $O(k)$ | $O(1)$*** | $O(\log k)$ |
| Next fit | $O(k)$ amort. | $O(k)$ amort. | N/A**** |

\* First fit busca por direccion, no por tamano. Ordenar por tamano no
ayuda (no sabemos que bloque es el "primero" en direccion).

\** Con lista ordenada por tamano, best fit busca el primer bloque >= n.
Con **busqueda lineal** sigue siendo $O(k)$, pero con busqueda binaria
sobre la lista seria $O(\log k)$ (si fuera un array o skip list).

\*** Worst fit en lista ordenada por tamano: el mayor bloque esta al final
(o al inicio, segun el orden). Acceso en $O(1)$.

\**** Next fit no tiene sentido sobre un arbol balanceado (requiere
recorrido secuencial con estado).

Para una **lista ordenada por direccion** (address-ordered): first fit es
$O(k)$, pero el coalescing es $O(1)$ porque los vecinos en la lista son
vecinos en memoria.

</details>

### Ejercicio 7 — Impacto del MIN_BLOCK

Si aumentamos MIN_BLOCK de 8 a 64 bytes, como cambia el comportamiento
del splitting y la fragmentacion?

<details><summary>Prediccion</summary>

**Splitting**: solo ocurre si el sobrante $\geq$ HEADER_SIZE + MIN_BLOCK.
Con MIN_BLOCK = 64: sobrante minimo para split = $16 + 64 = 80$ bytes.

**Consecuencias**:

1. **Menos splits**: muchos sobrantes de 20-79 bytes ya no generan split.
   Esos bytes se convierten en **fragmentacion interna** (dentro del
   bloque asignado, inutilizados).

2. **Menos bloques libres**: la free list tiene menos entradas, lo que
   acelera las busquedas ($k$ menor).

3. **Menos *slivers***: no se crean bloques libres diminutos que nunca
   se usarian.

4. **Mayor desperdicio interno**: un `malloc(100)` que obtiene un bloque
   de 170 (sobrante 70 < 80) desperdicia 70 bytes.

**Tradeoff**:
- MIN_BLOCK alto → menos fragmentacion externa, mas fragmentacion interna.
- MIN_BLOCK bajo → mas fragmentacion externa (muchos bloques pequenos),
  menos fragmentacion interna.

Los allocators reales eligen MIN_BLOCK = 16-32 bytes como compromiso. Los
*slab allocators* eliminan el problema al usar tamano fijo.

</details>

### Ejercicio 8 — Comparacion empirica

Dada la secuencia: `malloc(30)`, `malloc(60)`, `malloc(20)`, `free(1st)`,
`free(3rd)`, `malloc(25)`, `malloc(50)`.
Traza el estado del heap (1024 bytes, header=16, align=8) con **first fit**
y **best fit**. Difieren en el resultado final?

<details><summary>Prediccion</summary>

Alinear: 30→32, 60→64, 20→24, 25→32, 50→56. Header = 16.

**Ambas estrategias (paso 1-3 identicos)**:
```
malloc(32): [USED:32][FREE:960]
malloc(64): [USED:32][USED:64][FREE:880]
malloc(24): [USED:32][USED:64][USED:24][FREE:840]
free(1st):  [FREE:32][USED:64][USED:24][FREE:840]
free(3rd):  [FREE:32][USED:64][FREE:24][FREE:840]
            Coalesce 24+840: [FREE:32][USED:64][FREE:880]
```

**malloc(32)** (align(25)=32):
- First fit: elige [FREE:32] (exacto). No split.
  `[USED:32][USED:64][FREE:880]`
- Best fit: elige [FREE:32] (32 < 880, es el mejor fit). No split.
  `[USED:32][USED:64][FREE:880]`

**malloc(56)** (align(50)=56):
- First fit: elige [FREE:880]. Split: [USED:56][FREE:808].
  `[USED:32][USED:64][USED:56][FREE:808]`
- Best fit: elige [FREE:880] (unico bloque libre). Mismo resultado.
  `[USED:32][USED:64][USED:56][FREE:808]`

**Resultado**: identico. First fit y best fit coinciden cuando hay pocos
bloques libres o cuando el mejor fit es tambien el primero. Difieren mas
con muchos huecos de tamanos variados.

</details>

### Ejercicio 9 — Address-ordered first fit

Explica por que **first fit con lista ordenada por direccion** produce
resultados empiricamente similares a best fit. Que propiedad de la
ordenacion por direccion contribuye a esto?

<details><summary>Prediccion</summary>

Cuando la free list esta ordenada por **direccion** (no por tamano):

1. **Los bloques al inicio tienden a ser pequenos** (resultado de splits
   previos y acumulacion al inicio). First fit los encuentra primero y
   los usa para peticiones pequenas — efecto similar a best fit.

2. **Los bloques grandes tienden a estar al final** (resultado del coalescing
   con el espacio virgen). Esto los **preserva** para peticiones grandes,
   que es exactamente lo que best fit hace (evitar consumir bloques grandes
   para peticiones pequenas).

3. **Coalescing es eficiente**: en una lista ordenada por direccion, dos
   bloques adyacentes en la lista son adyacentes en memoria. Esto hace el
   coalescing trivial y completo, reduciendo la fragmentacion total.

La combinacion de (1) bloques pequenos al inicio para peticiones pequenas,
(2) bloques grandes preservados al final, y (3) coalescing eficiente produce
resultados muy cercanos al best fit sin el costo de recorrer toda la lista.

El estudio clasico de Wilson, Johnstone, Neely y Boles (1995) confirmo
empiricamente que address-ordered first fit es una de las mejores
estrategias en la practica.

</details>

### Ejercicio 10 — Disena tu estrategia

Propone una estrategia hibrida que combine las ventajas de first fit
(velocidad) y best fit (ajuste). Describe el algoritmo y analiza su
complejidad.

<details><summary>Prediccion</summary>

**Estrategia: "Good Fit" (First Fit con umbral de aceptacion)**

```
good_fit(n):
    threshold = n + n/4    // accept blocks up to 25% larger
    best = NULL
    for b in free_list:
        if b.size >= n:
            if b.size <= threshold:
                return b   // "good enough", stop immediately
            if best == NULL or b.size < best.size:
                best = b
            // Keep searching only if this block is too large
    return best
```

**Idea**: si encontramos un bloque que es "suficientemente bueno" (no mas
de 25% mas grande que la peticion), lo aceptamos inmediatamente sin buscar
mas. Solo si no encontramos un buen fit, caemos en best fit sobre lo visto.

**Complejidad**:
- **Mejor caso**: $O(1)$ — el primer bloque libre es un buen fit.
- **Peor caso**: $O(k)$ — ningun bloque es buen fit, recorremos toda la lista.
- **Caso promedio**: entre $O(1)$ y $O(k)$, dependiendo de la distribucion.

**Ventajas**:
- Mas rapido que best fit puro (termina antes).
- Menos *slivers* que best fit (acepta sobrantes razonables).
- Preserva bloques grandes mejor que worst fit.

**Variantes reales**:
- **ptmalloc2**: usa segregated bins, que es esencialmente este concepto
  a nivel de estructura de datos.
- **Good fit** es similar a lo que hacen jemalloc y tcmalloc con sus size
  classes: redondear al tamano de clase mas cercano y tomar el primero
  disponible en esa clase.

</details>
