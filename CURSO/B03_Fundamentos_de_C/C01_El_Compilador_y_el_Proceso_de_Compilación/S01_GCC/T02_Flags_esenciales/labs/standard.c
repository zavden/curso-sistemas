#include <stdio.h>

int main(void) {
    /* Zero-length array: GNU extension */
    struct flex { int n; int data[0]; };

    /* Statement expression: GNU extension */
    int x = 100;
    int y = ({ int tmp = x * 2; tmp + 1; });

    /* Binary literal: GNU extension (standard in C23) */
    int mask = 0b11110000;

    printf("size of flex = %zu\n", sizeof(struct flex));
    printf("y    = %d\n", y);
    printf("mask = 0x%X\n", mask);

    (void)x;
    return 0;
}
