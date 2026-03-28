#include <stdio.h>
#include <stdlib.h>

void heap_overflow_read(void) {
    int *arr = malloc(5 * sizeof(int));
    if (!arr) return;

    for (int i = 0; i < 5; i++) {
        arr[i] = i * 100;
    }

    /* Invalid read: index 5 is past the end of a 5-element array */
    printf("arr[5] = %d\n", arr[5]);

    free(arr);
}

void heap_overflow_write(void) {
    int *arr = malloc(3 * sizeof(int));
    if (!arr) return;

    /* Invalid write: index 3 is past the end of a 3-element array */
    for (int i = 0; i <= 3; i++) {
        arr[i] = i + 1;
    }

    free(arr);
}

void uninitialized_read(void) {
    int *data = malloc(4 * sizeof(int));
    if (!data) return;

    /* Only initialize the first two elements */
    data[0] = 10;
    data[1] = 20;

    /* Conditional jump depends on uninitialised value */
    if (data[2] > 0) {
        printf("data[2] is positive: %d\n", data[2]);
    } else {
        printf("data[2] is not positive: %d\n", data[2]);
    }

    free(data);
}

int main(void) {
    heap_overflow_read();
    heap_overflow_write();
    uninitialized_read();
    return 0;
}
