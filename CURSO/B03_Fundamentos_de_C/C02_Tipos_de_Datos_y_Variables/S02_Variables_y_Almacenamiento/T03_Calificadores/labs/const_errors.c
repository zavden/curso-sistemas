#include <stdio.h>

int main(void) {
    const int x = 10;
    int y = 20;

    const int *p1 = &x;
    int *const p2 = &y;
    const int *const p3 = &x;

    /* Each of these lines produces a compilation error.
       Uncomment one at a time to see each error. */

    /* ERROR 1: assignment of read-only location */
    /* *p1 = 30; */

    /* ERROR 2: assignment of read-only variable 'p2' */
    /* p2 = &x; */

    /* ERROR 3: assignment of read-only location */
    /* *p3 = 30; */

    /* ERROR 4: assignment of read-only variable 'p3' */
    /* p3 = &y; */

    /* These are valid: */
    p1 = &y;        /* pointer to const can be reassigned */
    *p2 = 30;       /* const pointer can modify the data */

    printf("*p3 = %d (both const — cannot modify data or pointer)\n", *p3);
    printf("After p1 = &y: *p1 = %d\n", *p1);
    printf("After *p2 = 30: y = %d\n", y);

    return 0;
}
