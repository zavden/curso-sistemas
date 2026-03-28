#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int main(void) {
    size_t test_sizes[] = {1, 7, 16, 17, 24, 32, 33, 48, 64, 100, 128, 256};
    int num_tests = (int)(sizeof(test_sizes) / sizeof(test_sizes[0]));

    printf("=== Internal fragmentation: requested vs usable size ===\n\n");
    printf("%-12s  %-12s  %-12s  %-10s\n",
           "Requested", "Usable", "Overhead", "Waste %");
    printf("%-12s  %-12s  %-12s  %-10s\n",
           "----------", "----------", "----------", "--------");

    for (int i = 0; i < num_tests; i++) {
        void *ptr = malloc(test_sizes[i]);
        if (ptr == NULL) {
            fprintf(stderr, "malloc(%zu) failed\n", test_sizes[i]);
            continue;
        }

        size_t usable = malloc_usable_size(ptr);
        size_t overhead = usable - test_sizes[i];
        double waste_pct = (test_sizes[i] > 0)
            ? (double)overhead / (double)usable * 100.0
            : 0.0;

        printf("%-12zu  %-12zu  %-12zu  %-8.1f%%\n",
               test_sizes[i], usable, overhead, waste_pct);

        free(ptr);
    }

    printf("\n=== Comparing total memory: many small vs one large ===\n\n");

    int count = 1000;

    /* Many small allocations */
    void *small_blocks[1000];
    size_t total_usable_small = 0;

    for (int i = 0; i < count; i++) {
        small_blocks[i] = malloc(1);
        if (small_blocks[i] != NULL) {
            total_usable_small += malloc_usable_size(small_blocks[i]);
        }
    }

    printf("1000 x malloc(1):\n");
    printf("  Requested total:  %d bytes\n", count);
    printf("  Usable total:     %zu bytes\n", total_usable_small);
    printf("  Overhead factor:  %.1fx\n\n",
           (double)total_usable_small / count);

    for (int i = 0; i < count; i++) {
        free(small_blocks[i]);
    }

    /* One large allocation */
    void *big_block = malloc((size_t)count);
    if (big_block != NULL) {
        size_t usable_big = malloc_usable_size(big_block);
        printf("1 x malloc(%d):\n", count);
        printf("  Requested total:  %d bytes\n", count);
        printf("  Usable total:     %zu bytes\n", usable_big);
        printf("  Overhead factor:  %.1fx\n",
               (double)usable_big / count);
        free(big_block);
    }

    return 0;
}
