#include <stdio.h>

int main(void) {
    int a = 10, b = 20;

    printf("a = %d, b = %d\n\n", a, b);

    printf("a == b  -> %d\n", a == b);
    printf("a != b  -> %d\n", a != b);
    printf("a <  b  -> %d\n", a <  b);
    printf("a >  b  -> %d\n", a >  b);
    printf("a <= b  -> %d\n", a <= b);
    printf("a >= b  -> %d\n", a >= b);

    printf("\n--- sizeof del resultado ---\n");
    printf("sizeof(a == b) = %zu\n", sizeof(a == b));
    printf("sizeof(a < b)  = %zu\n", sizeof(a < b));

    return 0;
}
