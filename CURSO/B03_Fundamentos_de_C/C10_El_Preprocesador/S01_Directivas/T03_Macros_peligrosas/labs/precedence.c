/* precedence.c -- Operator precedence bugs in macros without parentheses */

#include <stdio.h>

/* BAD: no parentheses around parameters or expression */
#define DOUBLE_BAD(x)  x + x
#define SQUARE_BAD(x)  x * x
#define IS_ODD_BAD(n)  n % 2 == 1

/* GOOD: parentheses around each parameter and the whole expression */
#define DOUBLE_GOOD(x)  ((x) + (x))
#define SQUARE_GOOD(x)  ((x) * (x))
#define IS_ODD_GOOD(n)  (((n) % 2) == 1)

int main(void) {
    printf("=== Precedence bugs without parentheses ===\n\n");

    /* --- SQUARE: no parens around parameter --- */
    printf("--- SQUARE(2 + 3) ---\n");
    printf("BAD:  SQUARE_BAD(2 + 3)  = %d  (expected 25)\n",
           SQUARE_BAD(2 + 3));
    printf("GOOD: SQUARE_GOOD(2 + 3) = %d  (expected 25)\n\n",
           SQUARE_GOOD(2 + 3));

    /* --- DOUBLE: no parens around whole expression --- */
    printf("--- 10 - DOUBLE(3) ---\n");
    printf("BAD:  10 - DOUBLE_BAD(3)  = %d  (expected 4)\n",
           10 - DOUBLE_BAD(3));
    printf("GOOD: 10 - DOUBLE_GOOD(3) = %d  (expected 4)\n\n",
           10 - DOUBLE_GOOD(3));

    printf("--- 2 * DOUBLE(5) ---\n");
    printf("BAD:  2 * DOUBLE_BAD(5)  = %d  (expected 20)\n",
           2 * DOUBLE_BAD(5));
    printf("GOOD: 2 * DOUBLE_GOOD(5) = %d  (expected 20)\n\n",
           2 * DOUBLE_GOOD(5));

    /* --- IS_ODD: operator precedence with bitwise | --- */
    printf("--- IS_ODD(4 | 1) ---\n");
    printf("BAD:  IS_ODD_BAD(4 | 1)  = %d  (expected 1)\n",
           IS_ODD_BAD(4 | 1));
    printf("GOOD: IS_ODD_GOOD(4 | 1) = %d  (expected 1)\n\n",
           IS_ODD_GOOD(4 | 1));

    /* --- Combined: parameter AND expression bug --- */
    printf("--- SQUARE in expression: 100 / SQUARE(2 + 3) ---\n");
    printf("BAD:  100 / SQUARE_BAD(2 + 3)  = %d  (expected 4)\n",
           100 / SQUARE_BAD(2 + 3));
    printf("GOOD: 100 / SQUARE_GOOD(2 + 3) = %d  (expected 4)\n",
           100 / SQUARE_GOOD(2 + 3));

    return 0;
}
