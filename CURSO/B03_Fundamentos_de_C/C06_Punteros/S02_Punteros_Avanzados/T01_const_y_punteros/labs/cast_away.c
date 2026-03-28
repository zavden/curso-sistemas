#include <stdio.h>

int main(void) {
    /* Case 1: casting away const on a truly const object -> UB */
    const int x = 42;
    int *bad_ptr = (int *)&x;   /* cast removes const */
    printf("before: x = %d, *bad_ptr = %d\n", x, *bad_ptr);
    *bad_ptr = 100;             /* UNDEFINED BEHAVIOR: x was declared const */
    printf("after:  x = %d, *bad_ptr = %d\n", x, *bad_ptr);
    /* The compiler may have replaced x with 42 at compile time,
     * so x might still print 42 even though *bad_ptr is 100.
     * This is UB — anything can happen. */

    printf("\n");

    /* Case 2: casting away const on a non-const object -> OK */
    int y = 42;
    const int *const_view = &y;     /* add const — safe */
    int *mutable_ptr = (int *)const_view;  /* remove const — OK because y is not const */
    *mutable_ptr = 200;
    printf("y = %d (modified through cast, original was not const)\n", y);

    return 0;
}
