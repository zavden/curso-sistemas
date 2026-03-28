#include <stdio.h>
#include <stddef.h>

void add_arrays(int *dst, const int *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i];
    }
}

void add_arrays_restrict(int *restrict dst, const int *restrict src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i];
    }
}

int main(void) {
    int a[] = {1, 2, 3, 4, 5};
    int b[] = {10, 20, 30, 40, 50};
    size_t n = sizeof(a) / sizeof(a[0]);

    add_arrays(a, b, n);

    printf("Result after add_arrays:");
    for (size_t i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf("\n");

    /* Reset a */
    int a2[] = {1, 2, 3, 4, 5};
    add_arrays_restrict(a2, b, n);

    printf("Result after add_arrays_restrict:");
    for (size_t i = 0; i < n; i++) {
        printf(" %d", a2[i]);
    }
    printf("\n");

    return 0;
}
