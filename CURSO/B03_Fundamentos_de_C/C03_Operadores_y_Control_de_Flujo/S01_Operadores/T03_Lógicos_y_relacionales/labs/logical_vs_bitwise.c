#include <stdio.h>

int main(void) {
    printf("--- & vs && y | vs || ---\n\n");

    int a = 6, b = 3;
    printf("a = %d (0b%d%d%d), b = %d (0b%d%d%d)\n",
           a, (a>>2)&1, (a>>1)&1, a&1,
           b, (b>>2)&1, (b>>1)&1, b&1);

    printf("a & b   = %d   (AND bitwise)\n", a & b);
    printf("a && b  = %d   (AND logico)\n", a && b);
    printf("a | b   = %d   (OR bitwise)\n", a | b);
    printf("a || b  = %d   (OR logico)\n\n", a || b);

    int c = 2, d = 4;
    printf("c = %d, d = %d\n", c, d);
    printf("c & d   = %d   (0b010 & 0b100 = 0b000)\n", c & d);
    printf("c && d  = %d   (ambos no-cero -> true)\n\n", c && d);

    int e = 0, f = 5;
    printf("e = %d, f = %d\n", e, f);
    printf("e & f   = %d   (0b000 & 0b101 = 0b000)\n", e & f);
    printf("e && f  = %d   (e es cero -> false, short-circuit)\n", e && f);
    printf("e | f   = %d   (0b000 | 0b101 = 0b101)\n", e | f);
    printf("e || f  = %d   (f es no-cero -> true)\n", e || f);

    return 0;
}
