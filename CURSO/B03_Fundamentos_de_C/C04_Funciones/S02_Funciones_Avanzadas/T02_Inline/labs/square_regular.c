#include <stdio.h>

int square(int x) {
    return x * x;
}

int main(void) {
    int a = 5;
    int result = square(a);
    printf("square(%d) = %d\n", a, result);
    return 0;
}
