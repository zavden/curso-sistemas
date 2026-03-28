#include "vec3.h"
#include <stdio.h>

int main(void) {
    Vec3 a = vec3_create(1.0, 2.0, 3.0);
    Vec3 b = vec3_create(4.0, 5.0, 6.0);

    printf("a = ");
    vec3_print(a);

    printf("b = ");
    vec3_print(b);

    Vec3 sum = vec3_add(a, b);
    printf("a + b = ");
    vec3_print(sum);

    double dot = vec3_dot(a, b);
    printf("a . b = %.2f\n", dot);

    double len = vec3_length(a);
    printf("|a| = %.4f\n", len);

    return 0;
}
