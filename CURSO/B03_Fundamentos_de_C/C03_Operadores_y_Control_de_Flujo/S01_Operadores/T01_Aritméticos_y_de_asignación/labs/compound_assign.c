#include <stdio.h>

int main(void) {
    printf("=== Compound assignment operators ===\n\n");

    int x = 10;
    printf("Initial x = %d\n\n", x);

    x += 5;
    printf("x += 5  -> x = %d\n", x);

    x -= 3;
    printf("x -= 3  -> x = %d\n", x);

    x *= 2;
    printf("x *= 2  -> x = %d\n", x);

    x /= 4;
    printf("x /= 4  -> x = %d\n", x);

    x %= 3;
    printf("x %%= 3  -> x = %d\n", x);

    printf("\n=== Pre-increment vs post-increment ===\n\n");

    int a = 5;
    int b = ++a;
    printf("a = 5; b = ++a; -> a = %d, b = %d  (prefix: increment THEN use)\n", a, b);

    a = 5;
    b = a++;
    printf("a = 5; b = a++; -> a = %d, b = %d  (postfix: use THEN increment)\n", a, b);

    printf("\n=== Pre-decrement vs post-decrement ===\n\n");

    a = 5;
    b = --a;
    printf("a = 5; b = --a; -> a = %d, b = %d  (prefix)\n", a, b);

    a = 5;
    b = a--;
    printf("a = 5; b = a--; -> a = %d, b = %d  (postfix)\n", a, b);

    printf("\n=== In standalone statements, no difference ===\n\n");

    a = 10;
    a++;
    printf("a = 10; a++; -> a = %d\n", a);

    a = 10;
    ++a;
    printf("a = 10; ++a; -> a = %d\n", a);

    printf("\n=== Chained assignment ===\n\n");

    int p, q, r;
    p = q = r = 42;
    printf("p = q = r = 42; -> p = %d, q = %d, r = %d\n", p, q, r);

    return 0;
}
