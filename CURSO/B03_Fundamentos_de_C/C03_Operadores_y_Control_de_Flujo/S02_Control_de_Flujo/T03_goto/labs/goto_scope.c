#include <stdio.h>

int main(void) {
    int count = 0;

    {
        inner:
        printf("Inside block (count=%d)\n", count);
    }

    count++;
    if (count < 3) {
        goto inner;   /* label defined in a block, but visible in entire function */
    }

    printf("Done (count=%d)\n", count);
    return 0;
}
