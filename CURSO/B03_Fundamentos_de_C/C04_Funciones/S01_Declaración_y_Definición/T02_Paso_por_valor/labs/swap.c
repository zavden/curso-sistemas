#include <stdio.h>

void swap_wrong(int a, int b) {
    int tmp = a;
    a = b;
    b = tmp;
    printf("  Inside swap_wrong: a = %d, b = %d\n", a, b);
}

void swap_correct(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(void) {
    int x = 5, y = 10;

    printf("Original:          x = %d, y = %d\n", x, y);

    swap_wrong(x, y);
    printf("After swap_wrong:  x = %d, y = %d\n", x, y);

    swap_correct(&x, &y);
    printf("After swap_correct: x = %d, y = %d\n", x, y);

    return 0;
}
