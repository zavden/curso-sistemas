#include <stdio.h>
#include <stddef.h>

int find_max(const int *arr, size_t n) {
    int max = arr[0];
    for (size_t i = 1; i < n; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    /* arr[0] = 999; */  /* Uncomment to see the error */
    return max;
}

int main(void) {
    int data[] = {3, 7, 2, 9, 5, 1, 8};
    size_t n = sizeof(data) / sizeof(data[0]);

    printf("Array:");
    for (size_t i = 0; i < n; i++) {
        printf(" %d", data[i]);
    }
    printf("\n");

    int max = find_max(data, n);
    printf("Max value: %d\n", max);

    /* Verify the array was not modified */
    printf("Array after find_max:");
    for (size_t i = 0; i < n; i++) {
        printf(" %d", data[i]);
    }
    printf("\n");

    return 0;
}
