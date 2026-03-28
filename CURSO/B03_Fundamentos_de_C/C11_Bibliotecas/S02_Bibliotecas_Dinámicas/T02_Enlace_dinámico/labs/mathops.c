#include "mathops.h"
#include <stdio.h>

int add(int a, int b)
{
    printf("  [libmathops] add(%d, %d) called\n", a, b);
    return a + b;
}

int mul(int a, int b)
{
    printf("  [libmathops] mul(%d, %d) called\n", a, b);
    return a * b;
}
