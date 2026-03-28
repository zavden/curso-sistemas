#include <stdio.h>

int main(void) {
    int x = 42;

    printf("Value of x:       %d\n", x);
    printf("Address of x:     %p\n", (void *)&x);

    int *p = &x;

    printf("Value of p:       %p\n", (void *)p);
    printf("Value of *p:      %d\n", *p);
    printf("Address of p:     %p\n", (void *)&p);

    return 0;
}
