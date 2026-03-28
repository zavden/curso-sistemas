#include "calc_v2.h"

#include <stdio.h>

int main(void) {
    printf("build: %s\n", calc_build());
    printf("add(4, 5, 6) = %d\n", add(4, 5, 6));
    return 0;
}
