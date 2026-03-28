/* program_v2.c -- Client linked against libcalc ABI v2 */
#include <stdio.h>
#include "calc_v2.h"

int main(void)
{
    int a = 10, b = 3, c = 7;

    printf("calc_add(%d, %d, %d) = %d\n", a, b, c, calc_add(a, b, c));
    printf("calc_mul(%d, %d) = %d\n", a, b, calc_mul(a, b));

    return 0;
}
