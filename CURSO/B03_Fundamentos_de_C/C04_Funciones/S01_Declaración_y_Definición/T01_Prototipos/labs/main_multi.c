#include <stdio.h>
#include "math_ops.h"

int main(void) {
    int a = 10, b = 3;

    printf("%d + %d = %d\n", a, b, add(a, b));
    printf("%d - %d = %d\n", a, b, subtract(a, b));
    printf("%d * %d = %d\n", a, b, multiply(a, b));

    return 0;
}
