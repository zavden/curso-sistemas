#include <stdio.h>

int main(void) {
    int x = 10, y = 20;

    /* 1. int *p — pointer and data are mutable */
    int *p1 = &x;
    *p1 = 100;
    p1 = &y;
    *p1 = 200;
    printf("combo 1 (int *):            x=%d y=%d\n", x, y);

    /* reset */
    x = 10;
    y = 20;

    /* 2. const int *p — data is protected, pointer is mutable */
    const int *p2 = &x;
    printf("combo 2 (const int *):      *p2=%d", *p2);
    /* *p2 = 100; */  /* ERROR: assignment of read-only location */
    p2 = &y;
    printf(" -> after redirect: *p2=%d\n", *p2);

    /* 3. int *const p — pointer is fixed, data is mutable */
    int *const p3 = &x;
    *p3 = 100;
    /* p3 = &y; */  /* ERROR: assignment of read-only variable 'p3' */
    printf("combo 3 (int *const):       x=%d (via *p3)\n", x);

    /* 4. const int *const p — both protected */
    const int *const p4 = &y;
    printf("combo 4 (const int *const): *p4=%d (read only)\n", *p4);
    /* *p4 = 100; */  /* ERROR: read-only location */
    /* p4 = &x;  */   /* ERROR: read-only variable */

    return 0;
}
