#include "calc.h"

#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

const char *calc_signature(int a, int b) {
    static char buffer[96];

    snprintf(buffer, sizeof(buffer), "calc-lib: add=%d mul=%d", add(a, b), mul(a, b));
    return buffer;
}
