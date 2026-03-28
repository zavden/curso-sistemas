#include "vec3.h"
#include <stdio.h>

/* Este programa solo usa vec3_create y vec3_add.
   No usa funciones de vec3_print.c ni vec3_length/vec3_dot. */

int main(void)
{
    Vec3 a = vec3_create(1.0, 2.0, 3.0);
    Vec3 b = vec3_create(4.0, 5.0, 6.0);

    Vec3 sum = vec3_add(a, b);
    printf("sum: (%.2f, %.2f, %.2f)\n", sum.x, sum.y, sum.z);

    return 0;
}
