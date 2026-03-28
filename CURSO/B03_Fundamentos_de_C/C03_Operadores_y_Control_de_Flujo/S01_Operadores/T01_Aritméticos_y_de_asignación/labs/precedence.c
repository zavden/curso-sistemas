#include <stdio.h>

int main(void) {
    printf("=== Precedence: * / %% before + - ===\n\n");

    int a = 2 + 3 * 4;
    int b = (2 + 3) * 4;
    printf("2 + 3 * 4   = %d   (multiplication first)\n", a);
    printf("(2 + 3) * 4 = %d   (parentheses override)\n", b);

    printf("\n=== Associativity: left to right for - ===\n\n");

    int c = 10 - 3 - 2;
    printf("10 - 3 - 2 = %d   ((10 - 3) - 2, left to right)\n", c);

    printf("\n=== Mixed * and %% (same precedence, left to right) ===\n\n");

    int d = 2 * 3 % 4;
    printf("2 * 3 %% 4 = %d   ((2 * 3) %% 4 = 6 %% 4)\n", d);

    int e = 15 / 4 * 4;
    printf("15 / 4 * 4 = %d   ((15 / 4) * 4 = 3 * 4)\n", e);

    printf("\n=== Assignment: right to left ===\n\n");

    int p, q, r;
    p = q = r = 7;
    printf("p = q = r = 7  -> p=%d, q=%d, r=%d   (r=7 first, then q=7, then p=7)\n",
           p, q, r);

    printf("\n=== Complex expression ===\n\n");

    int f = 2 + 3 * 4 - 6 / 2;
    printf("2 + 3 * 4 - 6 / 2 = %d\n", f);
    printf("Breakdown: 2 + (3*4) - (6/2) = 2 + 12 - 3 = 11\n");

    printf("\n=== Why parentheses matter ===\n\n");

    int x = 10;
    int without = x + 4 / 2;
    int with    = (x + 4) / 2;
    printf("x = %d\n", x);
    printf("x + 4 / 2   = %d   (division first: x + 2)\n", without);
    printf("(x + 4) / 2 = %d   (addition first: 14 / 2)\n", with);

    return 0;
}
