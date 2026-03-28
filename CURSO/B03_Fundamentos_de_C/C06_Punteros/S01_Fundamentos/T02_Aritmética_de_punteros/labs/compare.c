/* compare.c -- Demonstrates pointer comparison and sentinel traversal */
#include <stdio.h>

int main(void) {
    int arr[] = {10, 20, 30, 40, 50};
    int n = sizeof(arr) / sizeof(arr[0]);

    int *begin = arr;
    int *end   = arr + n;   /* one-past-the-end */

    printf("=== Pointer comparison operators ===\n");

    int *p = &arr[1];
    int *q = &arr[3];

    printf("p -> arr[1] = %d  (address %p)\n", *p, (void *)p);
    printf("q -> arr[3] = %d  (address %p)\n", *q, (void *)q);

    printf("\np < q  ? %s\n", p < q  ? "true" : "false");
    printf("p > q  ? %s\n", p > q  ? "true" : "false");
    printf("p == q ? %s\n", p == q ? "true" : "false");
    printf("p != q ? %s\n", p != q ? "true" : "false");
    printf("p <= q ? %s\n", p <= q ? "true" : "false");
    printf("p >= q ? %s\n", p >= q ? "true" : "false");

    /* Sentinel traversal using begin/end */
    printf("\n=== Sentinel traversal (begin/end) ===\n");
    printf("begin = %p\n", (void *)begin);
    printf("end   = %p  (one-past-the-end, NOT dereferenceable)\n",
           (void *)end);

    printf("\nForward traversal:  ");
    for (int *it = begin; it < end; it++) {
        printf("%d ", *it);
    }
    printf("\n");

    /* Reverse traversal */
    printf("Reverse traversal: ");
    for (int *it = end - 1; it >= begin; it--) {
        printf("%d ", *it);
    }
    printf("\n");

    /* Equality-based sentinel (common in C++) */
    printf("\nEquality sentinel:  ");
    for (int *it = begin; it != end; it++) {
        printf("%d ", *it);
    }
    printf("\n");

    return 0;
}
