/* ptrdiff.c -- Demonstrates pointer subtraction and ptrdiff_t */
#include <stdio.h>
#include <stddef.h>

int main(void) {
    int arr[] = {10, 20, 30, 40, 50, 60, 70, 80};

    int *p = &arr[1];
    int *q = &arr[6];

    printf("=== Pointer subtraction ===\n");
    printf("p points to arr[1] = %d  (address %p)\n", *p, (void *)p);
    printf("q points to arr[6] = %d  (address %p)\n", *q, (void *)q);

    ptrdiff_t forward  = q - p;
    ptrdiff_t backward = p - q;

    printf("\nq - p = %td elements\n", forward);
    printf("p - q = %td elements\n", backward);

    printf("\nAddress difference in bytes: %td\n",
           (char *)q - (char *)p);
    printf("Elements = bytes / sizeof(int) = %td / %zu = %td\n",
           (char *)q - (char *)p, sizeof(int), forward);

    /* Using ptrdiff_t with a loop */
    printf("\n=== Walking between p and q ===\n");
    for (ptrdiff_t i = 0; i < forward; i++) {
        printf("p[%td] = %d\n", i, p[i]);
    }

    return 0;
}
