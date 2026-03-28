#include <stdio.h>

int main(void) {
    int x = 42;

    /* Declaration 1: const int *p
     * Read right to left: p is a pointer (*) to int that is const
     * -> the pointed-to data is const */
    const int *p1 = &x;

    /* Declaration 2: int const *p  (equivalent to const int *p)
     * Read right to left: p is a pointer (*) to const int
     * -> same thing, different syntax */
    int const *p2 = &x;

    /* Declaration 3: int *const p
     * Read right to left: p is a const pointer (*) to int
     * -> the pointer itself is const */
    int *const p3 = &x;

    /* Declaration 4: const int *const p
     * Read right to left: p is a const pointer (*) to const int
     * -> both are const */
    const int *const p4 = &x;

    printf("All four read x=%d correctly:\n", x);
    printf("  const int *p1       = %d\n", *p1);
    printf("  int const *p2       = %d\n", *p2);
    printf("  int *const p3       = %d\n", *p3);
    printf("  const int *const p4 = %d\n", *p4);

    /* Demonstrate equivalence of const int * and int const * */
    p1 = &x;
    p2 = &x;  /* both allow redirect — the pointer is not const */
    /* p3 = &x; */  /* ERROR: p3 is const pointer */
    /* p4 = &x; */  /* ERROR: p4 is const pointer */

    return 0;
}
