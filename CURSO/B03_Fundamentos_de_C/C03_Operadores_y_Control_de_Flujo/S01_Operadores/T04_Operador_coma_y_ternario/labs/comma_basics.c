#include <stdio.h>

int main(void) {
    /* Comma operator: evaluates left to right, returns the rightmost value */
    int x = (1, 2, 3);
    printf("x = (1, 2, 3) -> x = %d\n", x);

    /* Comma operator with side effects */
    int a = 0, b = 0;  /* This comma is a SEPARATOR (declaration) */
    int result = (a = 10, b = 20, a + b);
    printf("(a = 10, b = 20, a + b) -> result = %d\n", result);
    printf("  a = %d, b = %d\n", a, b);

    /* Comma operator vs assignment precedence */
    /* Assignment has higher precedence than comma */
    int y, z;
    y = 1, z = 2;    /* parsed as: (y = 1), (z = 2) */
    printf("\ny = 1, z = 2 -> y=%d, z=%d\n", y, z);

    y = (1, z = 99);  /* parsed as: y = ( (1), (z = 99) ) -> y = 99 */
    printf("y = (1, z = 99) -> y=%d, z=%d\n", y, z);

    /* Comma in for loop — the most common legitimate use */
    printf("\nfor loop with comma operator:\n");
    for (int i = 0, j = 5; i < j; i++, j--) {
        printf("  i=%d, j=%d\n", i, j);
    }

    return 0;
}
