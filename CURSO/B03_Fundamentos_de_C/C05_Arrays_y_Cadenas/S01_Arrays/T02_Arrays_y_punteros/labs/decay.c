#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    int *p = arr;
    int *q = &arr[0];

    printf("arr          = %p\n", (void *)arr);
    printf("&arr[0]      = %p\n", (void *)&arr[0]);
    printf("p (= arr)    = %p\n", (void *)p);
    printf("q (= &arr[0])= %p\n", (void *)q);
    printf("p == q: %s\n", p == q ? "true" : "false");

    printf("\n--- arr[i] vs *(arr + i) ---\n");
    for (int i = 0; i < 5; i++) {
        printf("arr[%d] = %d,  *(arr + %d) = %d,  *(p + %d) = %d\n",
               i, arr[i], i, *(arr + i), i, *(p + i));
    }

    printf("\n--- pointer arithmetic addresses ---\n");
    for (int i = 0; i < 5; i++) {
        printf("arr + %d = %p  (diff from arr: %td bytes)\n",
               i, (void *)(arr + i),
               (char *)(arr + i) - (char *)arr);
    }

    return 0;
}
