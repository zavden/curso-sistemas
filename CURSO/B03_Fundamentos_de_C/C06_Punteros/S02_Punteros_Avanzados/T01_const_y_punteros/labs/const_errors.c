#include <stdio.h>

int main(void) {
    int x = 10, y = 20;
    const int cx = 30;

    /* --- declarations --- */
    int *p1 = &x;
    const int *p2 = &x;
    int *const p3 = &x;
    const int *const p4 = &x;

    /* --- modify data --- */
    *p1 = 100;      /* OK */
    *p2 = 100;      /* ERROR: assignment of read-only location '*p2' */
    *p3 = 100;      /* OK */
    *p4 = 100;      /* ERROR: assignment of read-only location '*p4' */

    /* --- redirect pointer --- */
    p1 = &y;        /* OK */
    p2 = &y;        /* OK */
    p3 = &y;        /* ERROR: assignment of read-only variable 'p3' */
    p4 = &y;        /* ERROR: assignment of read-only variable 'p4' */

    /* --- const qualification --- */
    int *p5 = &cx;          /* WARNING: discards 'const' qualifier */
    const int *p6 = &cx;   /* OK */

    printf("p1=%p p2=%p p3=%p p4=%p p5=%p p6=%p\n",
           (void *)p1, (void *)p2, (void *)p3, (void *)p4,
           (void *)p5, (void *)p6);

    return 0;
}
