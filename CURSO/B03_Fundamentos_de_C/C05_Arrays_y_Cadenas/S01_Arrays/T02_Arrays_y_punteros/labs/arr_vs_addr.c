#include <stdio.h>

int main(void) {
    int arr[5] = {100, 200, 300, 400, 500};

    int *p_elem = arr;         /* type: int *       -- pointer to int     */
    int (*p_arr)[5] = &arr;    /* type: int (*)[5]  -- pointer to array   */

    printf("=== Types and values ===\n");
    printf("p_elem (arr)   = %p  (int *)\n", (void *)p_elem);
    printf("p_arr  (&arr)  = %p  (int (*)[5])\n", (void *)p_arr);
    printf("Same address: %s\n",
           (void *)p_elem == (void *)p_arr ? "true" : "false");

    printf("\n=== Step size: +1 ===\n");
    printf("p_elem + 1     = %p  (advanced %td bytes)\n",
           (void *)(p_elem + 1),
           (char *)(p_elem + 1) - (char *)p_elem);
    printf("p_arr + 1      = %p  (advanced %td bytes)\n",
           (void *)(p_arr + 1),
           (char *)(p_arr + 1) - (char *)p_arr);

    printf("\n=== Accessing through pointer-to-array ===\n");
    for (int i = 0; i < 5; i++) {
        printf("(*p_arr)[%d] = %d\n", i, (*p_arr)[i]);
    }

    printf("\n=== Dereferencing comparison ===\n");
    printf("*p_elem        = %d  (first element)\n", *p_elem);
    printf("*p_arr         = %p  (the array itself, decays to &arr[0])\n",
           (void *)*p_arr);
    printf("**p_arr        = %d  (first element via double deref)\n", **p_arr);

    return 0;
}
