#include <stdio.h>

int main(void) {
    int *arr_of_ptrs[5];
    int (*ptr_to_arr)[5];
    int dummy[5] = {10, 20, 30, 40, 50};

    ptr_to_arr = &dummy;

    printf("=== sizeof comparison ===\n\n");

    printf("int *arr_of_ptrs[5]:\n");
    printf("  sizeof(arr_of_ptrs)    = %zu bytes\n", sizeof(arr_of_ptrs));
    printf("  sizeof(arr_of_ptrs[0]) = %zu bytes (each element is int*)\n",
           sizeof(arr_of_ptrs[0]));
    printf("  number of elements     = %zu\n\n",
           sizeof(arr_of_ptrs) / sizeof(arr_of_ptrs[0]));

    printf("int (*ptr_to_arr)[5]:\n");
    printf("  sizeof(ptr_to_arr)     = %zu bytes (just a pointer)\n",
           sizeof(ptr_to_arr));
    printf("  sizeof(*ptr_to_arr)    = %zu bytes (the whole array)\n",
           sizeof(*ptr_to_arr));

    printf("\n=== pointer arithmetic ===\n\n");

    printf("ptr_to_arr       = %p\n", (void *)ptr_to_arr);
    printf("ptr_to_arr + 1   = %p\n", (void *)(ptr_to_arr + 1));
    printf("difference       = %td bytes (sizeof(int[5]) = %zu)\n",
           (char *)(ptr_to_arr + 1) - (char *)ptr_to_arr,
           sizeof(int[5]));

    /* suppress unused warning */
    (void)arr_of_ptrs;

    return 0;
}
