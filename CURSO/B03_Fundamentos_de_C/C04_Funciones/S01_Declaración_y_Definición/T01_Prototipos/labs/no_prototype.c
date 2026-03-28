#include <stdio.h>

int main(void) {
    printf("add(3, 4) = %d\n", add(3, 4));
    printf("multiply(5, 6) = %d\n", multiply(5, 6));
    return 0;
}

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}
