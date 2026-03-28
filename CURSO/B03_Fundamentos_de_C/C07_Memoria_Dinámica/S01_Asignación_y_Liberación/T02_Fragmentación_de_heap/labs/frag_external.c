#include <stdio.h>
#include <stdlib.h>

#define NUM_BLOCKS 20
#define SMALL_SIZE 128

int main(void) {
    void *blocks[NUM_BLOCKS];

    printf("=== Phase 1: Allocate %d blocks of %d bytes ===\n\n",
           NUM_BLOCKS, SMALL_SIZE);

    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = malloc(SMALL_SIZE);
        if (blocks[i] == NULL) {
            fprintf(stderr, "malloc failed at block %d\n", i);
            return 1;
        }
        printf("Block %2d: %p\n", i, blocks[i]);
    }

    printf("\n=== Phase 2: Free odd-indexed blocks (1, 3, 5, ...) ===\n");
    printf("This creates gaps of %d bytes between allocated blocks.\n\n",
           SMALL_SIZE);

    int freed_count = 0;
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
        freed_count++;
    }

    printf("Freed %d blocks = %d bytes total free\n",
           freed_count, freed_count * SMALL_SIZE);
    printf("But each free gap is only %d bytes.\n\n", SMALL_SIZE);

    printf("=== Phase 3: Try to allocate one large block ===\n\n");

    size_t large_size = (size_t)freed_count * SMALL_SIZE;
    printf("Requesting %zu bytes (total freed memory)...\n", large_size);

    void *large = malloc(large_size);
    if (large != NULL) {
        printf("Large block allocated at: %p\n", large);
        printf("NOTE: glibc malloc succeeded because it can extend the heap\n");
        printf("or use mmap. In a constrained allocator, this could fail\n");
        printf("due to external fragmentation.\n\n");

        printf("Compare the large block address with original blocks:\n");
        printf("  Large block:     %p\n", large);
        printf("  First original:  %p\n", blocks[0]);
        printf("  Last original:   %p\n", blocks[NUM_BLOCKS - 2]);

        int between = 0;
        for (int i = 0; i < NUM_BLOCKS; i += 2) {
            if (blocks[i] != NULL) {
                if ((char *)large >= (char *)blocks[i] &&
                    (char *)large < (char *)blocks[i] + SMALL_SIZE) {
                    between = 1;
                }
            }
        }

        if (!between) {
            printf("\nThe large block is NOT in the freed gaps.\n");
            printf("The allocator had to find/create space elsewhere.\n");
            printf("The freed gaps remain as fragmentation.\n");
        }

        free(large);
    } else {
        printf("malloc returned NULL -- fragmentation prevented allocation.\n");
    }

    /* Cleanup remaining blocks */
    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        free(blocks[i]);
    }

    return 0;
}
