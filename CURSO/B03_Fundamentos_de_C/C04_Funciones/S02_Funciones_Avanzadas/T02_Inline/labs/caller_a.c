#include <stdio.h>
#include "math_inline.h"

void test_from_a(void) {
    int x = 15, lo = 0, hi = 10;
    printf("[caller_a] clamp(%d, %d, %d) = %d\n", x, lo, hi,
           clamp(x, lo, hi));
    printf("[caller_a] min(%d, %d) = %d\n", 3, 7, min(3, 7));
}
