#include <stdio.h>
#include <stdlib.h>

#define NUM_BLOCKS 10
#define BLOCK_SIZE 100

int main(void) {
    void *blocks[NUM_BLOCKS];

    printf("=== Allocating %d blocks of %d bytes ===\n\n",
           NUM_BLOCKS, BLOCK_SIZE);

    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = malloc(BLOCK_SIZE);
        if (blocks[i] == NULL) {
            fprintf(stderr, "malloc failed at block %d\n", i);
            return 1;
        }
        printf("Block %2d: %p\n", i, blocks[i]);
    }

    printf("\n=== Freeing even-indexed blocks (0, 2, 4, ...) ===\n\n");

    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        printf("Freeing block %d (%p)\n", i, blocks[i]);
        free(blocks[i]);
        blocks[i] = NULL;
    }

    printf("\n=== Allocating 5 new blocks of %d bytes ===\n\n", BLOCK_SIZE);

    void *new_blocks[5];
    for (int i = 0; i < 5; i++) {
        new_blocks[i] = malloc(BLOCK_SIZE);
        if (new_blocks[i] == NULL) {
            fprintf(stderr, "malloc failed at new block %d\n", i);
            return 1;
        }
        printf("New block %d: %p\n", i, new_blocks[i]);
    }

    printf("\n=== Comparing addresses ===\n");
    printf("Remaining original blocks:\n");
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        printf("  Original block %2d: %p\n", i, blocks[i]);
    }
    printf("New blocks:\n");
    for (int i = 0; i < 5; i++) {
        printf("  New block      %2d: %p\n", i, new_blocks[i]);
    }

    /* Cleanup */
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        free(blocks[i]);
    }
    for (int i = 0; i < 5; i++) {
        free(new_blocks[i]);
    }

    return 0;
}
