#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/* --- Simple pool allocator --- */

#define POOL_CAPACITY 16
#define POOL_BLOCK_SIZE 64

struct Pool {
    char memory[POOL_CAPACITY * POOL_BLOCK_SIZE];
    int used[POOL_CAPACITY];
    int alloc_count;
    int free_count;
};

void pool_init(struct Pool *pool) {
    memset(pool->used, 0, sizeof(pool->used));
    pool->alloc_count = 0;
    pool->free_count = 0;
}

void *pool_alloc(struct Pool *pool) {
    for (int i = 0; i < POOL_CAPACITY; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            pool->alloc_count++;
            return &pool->memory[i * POOL_BLOCK_SIZE];
        }
    }
    return NULL;
}

void pool_free(struct Pool *pool, void *ptr) {
    int idx = (int)((char *)ptr - pool->memory) / POOL_BLOCK_SIZE;
    if (idx >= 0 && idx < POOL_CAPACITY) {
        pool->used[idx] = 0;
        pool->free_count++;
    }
}

void pool_status(const struct Pool *pool) {
    printf("  Pool map: [");
    for (int i = 0; i < POOL_CAPACITY; i++) {
        printf("%c", pool->used[i] ? '#' : '.');
    }
    printf("]\n");
    printf("  Allocs: %d, Frees: %d\n", pool->alloc_count, pool->free_count);
}

/* --- Strategy demonstrations --- */

void demo_uniform_sizes(void) {
    printf("=== Strategy 1: Uniform allocation sizes ===\n\n");

    printf("Mixed sizes (bad for fragmentation):\n");
    void *mixed[6];
    size_t sizes[] = {16, 128, 32, 256, 8, 64};
    for (int i = 0; i < 6; i++) {
        mixed[i] = malloc(sizes[i]);
        printf("  malloc(%3zu) -> %p (usable: %zu)\n",
               sizes[i], mixed[i], malloc_usable_size(mixed[i]));
    }
    for (int i = 0; i < 6; i++) {
        free(mixed[i]);
    }

    printf("\nUniform sizes (good for fragmentation):\n");
    void *uniform[6];
    for (int i = 0; i < 6; i++) {
        uniform[i] = malloc(64);
        printf("  malloc( 64) -> %p (usable: %zu)\n",
               uniform[i], malloc_usable_size(uniform[i]));
    }
    for (int i = 0; i < 6; i++) {
        free(uniform[i]);
    }

    printf("\nWith uniform sizes, freed slots can be reused by any new\n");
    printf("allocation of the same size -- no fragmentation.\n");
}

void demo_pool(void) {
    printf("\n=== Strategy 2: Pool allocator ===\n\n");

    struct Pool pool;
    pool_init(&pool);

    printf("Allocating 8 blocks from pool:\n");
    void *ptrs[8];
    for (int i = 0; i < 8; i++) {
        ptrs[i] = pool_alloc(&pool);
        printf("  pool_alloc() -> %p\n", ptrs[i]);
    }
    pool_status(&pool);

    printf("\nFreeing blocks 1, 3, 5 (creating gaps):\n");
    pool_free(&pool, ptrs[1]);
    pool_free(&pool, ptrs[3]);
    pool_free(&pool, ptrs[5]);
    pool_status(&pool);

    printf("\nAllocating 3 more blocks:\n");
    for (int i = 0; i < 3; i++) {
        void *p = pool_alloc(&pool);
        printf("  pool_alloc() -> %p\n", p);
    }
    pool_status(&pool);

    printf("\nThe pool reuses freed slots exactly -- no external fragmentation.\n");
    printf("Every block is %d bytes, so any free slot fits any request.\n",
           POOL_BLOCK_SIZE);
}

void demo_batch_alloc(void) {
    printf("\n=== Strategy 3: One large allocation vs many small ===\n\n");

    int count = 100;

    printf("Many small allocations:\n");
    int **small = malloc((size_t)count * sizeof(int *));
    size_t total_overhead = 0;
    for (int i = 0; i < count; i++) {
        small[i] = malloc(sizeof(int));
        total_overhead += malloc_usable_size(small[i]) - sizeof(int);
    }
    printf("  %d x malloc(sizeof(int)) = %d x malloc(%zu)\n",
           count, count, sizeof(int));
    printf("  Total wasted in overhead: %zu bytes\n", total_overhead);

    for (int i = 0; i < count; i++) {
        free(small[i]);
    }
    free(small);

    printf("\nOne batch allocation:\n");
    int *batch = malloc((size_t)count * sizeof(int));
    size_t batch_overhead = malloc_usable_size(batch) -
                            ((size_t)count * sizeof(int));
    printf("  1 x malloc(%zu)\n", (size_t)count * sizeof(int));
    printf("  Total wasted in overhead: %zu bytes\n", batch_overhead);
    printf("  Saving: %zu bytes\n", total_overhead - batch_overhead);

    free(batch);
}

void demo_realloc(void) {
    printf("\n=== Strategy 4: realloc instead of malloc+copy+free ===\n\n");

    printf("Growing a buffer with realloc:\n\n");

    size_t size = 16;
    char *buf = malloc(size);
    if (buf == NULL) {
        return;
    }
    printf("  Initial: malloc(%zu) -> %p (usable: %zu)\n",
           size, (void *)buf, malloc_usable_size(buf));

    for (int i = 0; i < 4; i++) {
        size_t new_size = size * 2;
        char *old = buf;
        buf = realloc(buf, new_size);
        if (buf == NULL) {
            fprintf(stderr, "realloc failed\n");
            free(old);
            return;
        }
        printf("  realloc(%3zu) -> %p (usable: %zu) %s\n",
               new_size, (void *)buf, malloc_usable_size(buf),
               (buf == old) ? "[in-place]" : "[moved]");
        size = new_size;
    }

    printf("\nWhen realloc grows in-place, no fragmentation is created.\n");
    printf("When it moves, the old block is freed and could cause fragmentation,\n");
    printf("but the allocator handles the copy and merging internally.\n");

    free(buf);
}

int main(void) {
    demo_uniform_sizes();
    demo_pool();
    demo_batch_alloc();
    demo_realloc();

    return 0;
}
