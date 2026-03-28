#include "calc.h"

#include <stdio.h>

int main(void) {
    int a = 6;
    int b = 7;

    printf("add: %d\n", add(a, b));
    printf("mul: %d\n", mul(a, b));
    printf("signature: %s\n", calc_signature(a, b));

    return 0;
}
