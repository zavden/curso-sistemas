#include <stdio.h>
#include "math_utils.h"

int main(void) {
    int a = 7, b = 5;
    printf("%d + %d = %d\n", a, b, add(a, b));
    printf("%d * %d = %d\n", a, b, multiply(a, b));
    return 0;
}
