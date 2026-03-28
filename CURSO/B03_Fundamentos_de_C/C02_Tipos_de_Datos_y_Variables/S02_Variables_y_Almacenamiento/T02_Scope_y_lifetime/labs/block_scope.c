#include <stdio.h>

int x = 100;

void show_shadowing(void) {
    int x = 200;
    printf("Inside function: x = %d\n", x);

    {
        int x = 300;
        printf("Inside inner block: x = %d\n", x);
    }

    printf("Back in function: x = %d\n", x);
}

int main(void) {
    printf("Before call: x = %d\n", x);
    show_shadowing();
    printf("After call: x = %d\n", x);

    for (int i = 0; i < 3; i++) {
        int temp = i * 10;
        printf("Loop i=%d, temp=%d\n", i, temp);
    }
    /* i and temp are not accessible here */

    return 0;
}
