#include <stdio.h>
#include <string.h>

void swap_generic(void *a, void *b, size_t size) {
    unsigned char tmp[size];
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}

int main(void) {
    /* Swap ints */
    int x = 10, y = 20;
    printf("Before swap: x=%d, y=%d\n", x, y);
    swap_generic(&x, &y, sizeof(int));
    printf("After swap:  x=%d, y=%d\n", x, y);

    /* Swap doubles */
    double a = 1.1, b = 2.2;
    printf("\nBefore swap: a=%.1f, b=%.1f\n", a, b);
    swap_generic(&a, &b, sizeof(double));
    printf("After swap:  a=%.1f, b=%.1f\n", a, b);

    /* Swap structs */
    struct Point {
        int x;
        int y;
    };

    struct Point p1 = {1, 2};
    struct Point p2 = {3, 4};
    printf("\nBefore swap: p1={%d,%d}, p2={%d,%d}\n",
           p1.x, p1.y, p2.x, p2.y);
    swap_generic(&p1, &p2, sizeof(struct Point));
    printf("After swap:  p1={%d,%d}, p2={%d,%d}\n",
           p1.x, p1.y, p2.x, p2.y);

    return 0;
}
