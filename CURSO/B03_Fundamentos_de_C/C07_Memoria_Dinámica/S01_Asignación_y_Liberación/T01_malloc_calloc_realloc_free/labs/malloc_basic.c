// malloc_basic.c — malloc and free basics
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Allocate a single int
    int *p = malloc(sizeof(*p));
    if (p == NULL) {
        perror("malloc");
        return 1;
    }

    *p = 42;
    printf("Value: %d\n", *p);
    printf("Address: %p\n", (void *)p);

    free(p);
    p = NULL;

    // Allocate an array of 5 ints
    int n = 5;
    int *arr = malloc(n * sizeof(*arr));
    if (arr == NULL) {
        perror("malloc");
        return 1;
    }

    // Fill with values
    for (int i = 0; i < n; i++) {
        arr[i] = (i + 1) * 10;
    }

    // Print values
    printf("Array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free(arr);
    arr = NULL;

    return 0;
}
