#include "vec3.h"

#include <stdio.h>

int main(void) {
    Vec3 a = vec3_create(1.0, 2.0, 3.0);
    Vec3 b = vec3_create(4.0, 5.0, 6.0);
    Vec3 sum = vec3_add(a, b);

    vec3_print("sum", sum);
    printf("sum length = %.2f\n", vec3_length(sum));
    vec3_print_report(sum);

    return 0;
}
