/* illegal.c -- Demonstrates illegal pointer operations (compiler errors)
 *
 * This file is meant to be compiled to SEE the errors.
 * Uncomment one block at a time to observe each error message.
 * All blocks start commented out so the file compiles cleanly as-is.
 */
#include <stdio.h>
#include <stddef.h>

int main(void) {
    int arr[] = {10, 20, 30, 40, 50};
    int *p = arr;
    int *q = arr + 3;

    /* --- VALID operations (these compile fine) --- */
    int *r1 = p + 2;          /* pointer + integer  -> pointer   */
    int *r2 = q - 1;          /* pointer - integer  -> pointer   */
    ptrdiff_t d = q - p;      /* pointer - pointer  -> ptrdiff_t */
    int cmp = (p < q);        /* pointer comparison -> int        */

    printf("Valid: r1=%p  r2=%p  d=%td  cmp=%d\n",
           (void *)r1, (void *)r2, d, cmp);

    /* --- ILLEGAL: Adding two pointers --- */
    /* Uncomment the next line to see the error: */
    /* int *bad1 = p + q; */

    /* --- ILLEGAL: Multiplying a pointer --- */
    /* Uncomment the next line to see the error: */
    /* int *bad2 = p * 2; */

    /* --- ILLEGAL: Dividing a pointer --- */
    /* Uncomment the next line to see the error: */
    /* int *bad3 = p / 2; */

    /* --- ILLEGAL: Modulo on a pointer --- */
    /* Uncomment the next line to see the error: */
    /* int *bad4 = p % 2; */

    /* --- ILLEGAL: Bitwise AND on a pointer --- */
    /* Uncomment the next line to see the error: */
    /* int *bad5 = p & 0xFF; */

    printf("\nUncomment one illegal block at a time and recompile.\n");
    printf("Observe the error message from the compiler.\n");

    return 0;
}
