#include "calc_v1.h"

#include <stdio.h>

int main(void) {
    printf("build: %s\n", calc_build());
    printf("add(4, 5) = %d\n", add(4, 5));
    printf("mul(4, 5) = %d\n", mul(4, 5));
    return 0;
}
