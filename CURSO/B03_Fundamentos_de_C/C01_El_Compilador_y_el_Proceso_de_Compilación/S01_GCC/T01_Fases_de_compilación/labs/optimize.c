#include <stdio.h>

int square(int x) {
    return x * x;
}

int dead_code_example(int x) {
    if (0) {
        printf("This never runs\n");
        return -1;
    }
    int result = square(x);
    return result + 10;
}

int main(void) {
    int val = 2 + 3;
    printf("square(%d) = %d\n", val, square(val));
    printf("example(%d) = %d\n", val, dead_code_example(val));
    return 0;
}
