#include <stdio.h>
#include "mathops.h"

int main(void)
{
    printf("Dynamic linking demo\n");
    printf("add(3, 5) = %d\n", add(3, 5));
    printf("mul(4, 7) = %d\n", mul(4, 7));
    printf("add(10, mul(2, 3)) = %d\n", add(10, mul(2, 3)));
    return 0;
}
