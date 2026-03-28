#include "vec3.h"
#include <stdio.h>

void vec3_print(Vec3 v)
{
    printf("(%.2f, %.2f, %.2f)\n", v.x, v.y, v.z);
}

void vec3_report(const char *label, Vec3 v)
{
    printf("%s: (%.2f, %.2f, %.2f)\n", label, v.x, v.y, v.z);
}
