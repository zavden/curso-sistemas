#include <stdio.h>

int main(void) {
    printf("--- Precedencia de operadores logicos y relacionales ---\n\n");

    int a = 10, b = 3, c = 0;

    /* && has higher precedence than || */
    printf("a=%d, b=%d, c=%d\n\n", a, b, c);

    int r1 = a > 5 && b < 10 || c == 0;
    int r2 = (a > 5 && b < 10) || c == 0;
    int r3 = a > 5 && (b < 10 || c == 0);

    printf("a > 5 && b < 10 || c == 0      -> %d\n", r1);
    printf("(a > 5 && b < 10) || c == 0    -> %d  (asi se parsea)\n", r2);
    printf("a > 5 && (b < 10 || c == 0)    -> %d  (diferente agrupacion)\n\n",
           r3);

    /* ! has highest precedence */
    int x = 0;
    printf("x = %d\n", x);
    printf("!x         -> %d\n", !x);
    printf("!x && 1    -> %d  (! se evalua primero)\n", !x && 1);
    printf("!(x && 1)  -> %d  (negacion del resultado)\n\n", !(x && 1));

    /* De Morgan's laws */
    int p = 1, q = 0;
    printf("p=%d, q=%d\n", p, q);
    printf("!(p && q)      -> %d\n", !(p && q));
    printf("(!p || !q)     -> %d  (De Morgan: equivalente)\n", (!p || !q));
    printf("!(p || q)      -> %d\n", !(p || q));
    printf("(!p && !q)     -> %d  (De Morgan: equivalente)\n", (!p && !q));

    return 0;
}
