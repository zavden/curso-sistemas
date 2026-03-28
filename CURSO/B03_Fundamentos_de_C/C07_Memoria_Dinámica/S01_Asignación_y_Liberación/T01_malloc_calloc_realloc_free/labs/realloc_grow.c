// realloc_grow.c — grow a dynamic array with realloc (safe pattern)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    size_t cap = 4;
    size_t len = 0;

    int *arr = malloc(cap * sizeof(*arr));
    if (arr == NULL) {
        perror("malloc");
        return 1;
    }
    printf("Initial: cap=%zu, len=%zu, addr=%p\n", cap, len, (void *)arr);

    // Push 12 elements, growing as needed
    for (int i = 0; i < 12; i++) {
        if (len == cap) {
            size_t new_cap = cap * 2;
            printf("  Growing: cap %zu -> %zu\n", cap, new_cap);

            uintptr_t old_addr = (uintptr_t)arr;

            // Safe pattern: use temp pointer
            int *tmp = realloc(arr, new_cap * sizeof(*tmp));
            if (tmp == NULL) {
                perror("realloc");
                free(arr);
                return 1;
            }
            if ((uintptr_t)tmp != old_addr) {
                printf("  Block MOVED: 0x%lx -> %p\n",
                       (unsigned long)old_addr, (void *)tmp);
            } else {
                printf("  Block extended in place: %p\n", (void *)tmp);
            }
            arr = tmp;
            cap = new_cap;
        }
        arr[len++] = (i + 1) * 100;
    }

    printf("\nFinal: cap=%zu, len=%zu\n", cap, len);
    printf("Contents: ");
    for (size_t i = 0; i < len; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free(arr);
    arr = NULL;

    return 0;
}
