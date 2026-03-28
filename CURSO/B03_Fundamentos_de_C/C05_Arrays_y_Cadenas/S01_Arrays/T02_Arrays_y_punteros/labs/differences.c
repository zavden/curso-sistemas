#include <stdio.h>

void show_sizeof(int param[], int n) {
    printf("Inside function:\n");
    printf("  sizeof(param)    = %zu  (pointer size)\n", sizeof(param));
    printf("  sizeof(param[0]) = %zu  (element size)\n", sizeof(param[0]));
    printf("  n (passed)       = %d\n", n);
    (void)n;
}

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *ptr = arr;

    printf("=== sizeof ===\n");
    printf("sizeof(arr) = %zu  (total array size: %zu elements * %zu bytes)\n",
           sizeof(arr), sizeof(arr) / sizeof(arr[0]), sizeof(arr[0]));
    printf("sizeof(ptr) = %zu  (pointer size)\n", sizeof(ptr));

    printf("\n=== sizeof inside function ===\n");
    show_sizeof(arr, 5);

    printf("\n=== addresses: arr vs &arr ===\n");
    printf("arr      = %p  (decays to &arr[0], type: int *)\n", (void *)arr);
    printf("&arr     = %p  (type: int (*)[5])\n", (void *)&arr);
    printf("&arr[0]  = %p  (type: int *)\n", (void *)&arr[0]);
    printf("Same numeric value: %s\n",
           (void *)arr == (void *)&arr ? "true" : "false");

    printf("\n=== arr + 1 vs &arr + 1 ===\n");
    printf("arr          = %p\n", (void *)arr);
    printf("arr + 1      = %p  (advanced %td bytes = sizeof(int))\n",
           (void *)(arr + 1),
           (char *)(arr + 1) - (char *)arr);
    printf("&arr + 1     = %p  (advanced %td bytes = sizeof(int[5]))\n",
           (void *)(&arr + 1),
           (char *)(&arr + 1) - (char *)&arr);

    return 0;
}
