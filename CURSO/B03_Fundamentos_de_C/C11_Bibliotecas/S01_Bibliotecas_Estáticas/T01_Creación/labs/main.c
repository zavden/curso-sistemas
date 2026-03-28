#include "vec3.h"
#include <stdio.h>

int main(void)
{
    Vec3 a = vec3_create(1.0, 2.0, 3.0);
    Vec3 b = vec3_create(4.0, 5.0, 6.0);

    Vec3 sum = vec3_add(a, b);
    vec3_report("a + b", sum);

    Vec3 scaled = vec3_scale(a, 2.5);
    vec3_report("a * 2.5", scaled);

    double dot = vec3_dot(a, b);
    printf("dot(a, b): %.2f\n", dot);

    double len = vec3_length(a);
    printf("length(a): %.2f\n", len);

    vec3_print(a);
    vec3_print(b);

    return 0;
}
