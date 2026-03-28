#include <stdio.h>

int main(void) {
    /* Demonstrate precedence interactions between
       comma, ternary, and assignment operators */

    /* --- Ternary vs Assignment --- */
    /* Ternary (level 13) has higher precedence than assignment (level 14) */
    int a = 5, b = 3;
    int max;
    max = (a > b) ? a : b;
    printf("Ternary + assignment: max = %d\n", max);

    /* Without parentheses around condition, it still works because
       > has higher precedence than ?: */
    max = a > b ? a : b;
    printf("Without parens on condition: max = %d\n", max);

    /* --- Assignment inside ternary branches --- */
    int x = 0, y = 0;
    int condition = 1;
    condition ? (x = 10) : (y = 20);
    printf("\ncondition=1: x=%d, y=%d\n", x, y);

    x = 0; y = 0;
    condition = 0;
    condition ? (x = 10) : (y = 20);
    printf("condition=0: x=%d, y=%d\n", x, y);

    /* --- Comma vs Assignment precedence --- */
    /* Comma (level 15) has LOWER precedence than assignment (level 14) */
    int p, q;
    p = 1, q = 2;             /* parsed as: (p = 1), (q = 2) */
    printf("\np = 1, q = 2: p=%d, q=%d\n", p, q);

    p = (1, 2);               /* comma as operator: p = 2 */
    printf("p = (1, 2): p=%d\n", p);

    /* --- Full precedence chain: arithmetic > comparison > ternary > assignment > comma --- */
    int r;
    int val = 7;
    r = val > 5 ? val * 2 : val + 1, printf("Side effect in comma\n");
    /* Parsed as: (r = (val > 5 ? val * 2 : val + 1)), (printf(...)) */
    printf("r = %d\n", r);

    /* --- Practical: ternary precedence with bitwise operators --- */
    /* Common mistake: & has LOWER precedence than == */
    int flags = 0x0F;
    /* printf("%s\n", flags & 0x01 == 1 ? "set" : "clear"); */
    /* Above would be parsed as: flags & (0x01 == 1) which is flags & 1 */
    /* Correct: */
    printf("\nflags = 0x%02X\n", flags);
    printf("(flags & 0x01) ? set : clear -> %s\n",
           (flags & 0x01) ? "set" : "clear");

    return 0;
}
