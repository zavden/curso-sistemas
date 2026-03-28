/* program_v1.c -- Client linked against libcalc ABI v1 */
#include <stdio.h>
#include "calc.h"

int main(void)
{
    int a = 10, b = 3;

    printf("calc_add(%d, %d) = %d\n", a, b, calc_add(a, b));
    printf("calc_mul(%d, %d) = %d\n", a, b, calc_mul(a, b));

    return 0;
}
