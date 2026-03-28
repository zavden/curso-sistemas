/* calc_v1_minor.c -- libcalc v1.1.0 implementation (minor bump) */
/* Adds calc_sub() without changing existing functions -- ABI compatible */
#include "calc.h"

int calc_add(int a, int b)
{
    return a + b;
}

int calc_mul(int a, int b)
{
    return a * b;
}

/* New function added in v1.1.0 */
int calc_sub(int a, int b)
{
    return a - b;
}
