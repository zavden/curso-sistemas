#include <stdio.h>
#include <string.h>

/*
 * memcpy vs memmove: the classic restrict example.
 *
 * memcpy signature:   void *memcpy(void *restrict dst, const void *restrict src, size_t n);
 * memmove signature:  void *memmove(void *dst, const void *src, size_t n);
 *
 * memcpy has restrict: it PROMISES dst and src do not overlap.
 * memmove does NOT: it handles overlapping regions correctly.
 *
 * This program shows that memcpy can produce wrong results
 * with overlapping memory, while memmove always works.
 */

static void print_array(const char *label, const int *arr, int n) {
    printf("%s: ", label);
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Non-overlapping copy ===\n");
    int src[] = {1, 2, 3, 4, 5};
    int dst[5];

    memcpy(dst, src, 5 * sizeof(int));
    print_array("memcpy dst", dst, 5);

    int dst2[5];
    memmove(dst2, src, 5 * sizeof(int));
    print_array("memmove dst", dst2, 5);
    printf("Both produce the same result with non-overlapping memory.\n\n");

    printf("=== Overlapping copy (forward) ===\n");
    int data1[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int data2[] = {1, 2, 3, 4, 5, 6, 7, 8};

    printf("Before: ");
    print_array("data", data1, 8);
    printf("Copying data[0..4] -> data[2..6]\n");

    /* Copy 5 elements from start to offset 2 */
    /* This overlaps: src=data[0], dst=data[2], 5 ints */
    memcpy(data1 + 2, data1, 5 * sizeof(int));
    print_array("memcpy result ", data1, 8);

    memmove(data2 + 2, data2, 5 * sizeof(int));
    print_array("memmove result", data2, 8);

    printf("\nmemmove always gives the correct result: {1,2, 1,2,3,4,5, 8}\n");
    printf("memcpy may or may not be correct -- it depends on the implementation.\n\n");

    printf("=== Overlapping copy (backward) ===\n");
    int data3[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int data4[] = {1, 2, 3, 4, 5, 6, 7, 8};

    printf("Before: ");
    print_array("data", data3, 8);
    printf("Copying data[2..6] -> data[0..4]\n");

    memcpy(data3, data3 + 2, 5 * sizeof(int));
    print_array("memcpy result ", data3, 8);

    memmove(data4, data4 + 2, 5 * sizeof(int));
    print_array("memmove result", data4, 8);

    printf("\nBoth should give: {3,4,5,6,7, 6,7,8}\n");

    return 0;
}
