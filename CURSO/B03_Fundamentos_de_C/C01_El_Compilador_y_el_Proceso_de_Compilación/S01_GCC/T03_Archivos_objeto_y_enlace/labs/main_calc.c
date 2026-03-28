#include <stdio.h>
#include "calc.h"

int main(void) {
    int a = 10, b = 3;

    printf("%d + %d = %d\n", a, b, add(a, b));
    printf("%d - %d = %d\n", a, b, sub(a, b));
    printf("%d * %d = %d\n", a, b, mul(a, b));

    return 0;
}
