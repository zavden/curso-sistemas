#include <stdio.h>
#include "math_inline.h"

void test_from_b(void) {
    int x = -5, lo = 0, hi = 10;
    printf("[caller_b] clamp(%d, %d, %d) = %d\n", x, lo, hi,
           clamp(x, lo, hi));
    printf("[caller_b] max(%d, %d) = %d\n", 3, 7, max(3, 7));
}
