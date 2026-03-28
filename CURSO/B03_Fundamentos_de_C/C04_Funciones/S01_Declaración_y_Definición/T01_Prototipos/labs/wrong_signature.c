#include <stdio.h>

/* prototype says: two ints */
int add(int a, int b);

int main(void) {
    /* error: too few arguments */
    printf("add(5) = %d\n", add(5));

    /* error: too many arguments */
    printf("add(1, 2, 3) = %d\n", add(1, 2, 3));

    return 0;
}

int add(int a, int b) {
    return a + b;
}
