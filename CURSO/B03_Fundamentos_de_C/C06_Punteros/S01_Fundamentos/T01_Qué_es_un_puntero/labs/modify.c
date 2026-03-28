#include <stdio.h>

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main(void) {
    int x = 10;
    int *p = &x;

    printf("Before: x = %d\n", x);
    *p = 99;
    printf("After *p = 99: x = %d\n", x);

    int y = 20;
    printf("\nBefore swap: x = %d, y = %d\n", x, y);
    swap(&x, &y);
    printf("After swap:  x = %d, y = %d\n", x, y);

    return 0;
}
