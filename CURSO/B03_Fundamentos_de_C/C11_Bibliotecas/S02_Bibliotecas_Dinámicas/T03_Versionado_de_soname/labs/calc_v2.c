/* calc_v2.c -- libcalc v2.0.0 implementation (major bump, ABI break) */
#include "calc_v2.h"

/* Changed: now sums 3 values instead of 2 */
int calc_add(int a, int b, int c)
{
    return a + b + c;
}

int calc_mul(int a, int b)
{
    return a * b;
}
