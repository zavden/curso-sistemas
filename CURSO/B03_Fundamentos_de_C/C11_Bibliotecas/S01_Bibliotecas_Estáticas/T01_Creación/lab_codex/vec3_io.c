#include "vec3.h"

#include <stdio.h>

void vec3_print(const char *label, Vec3 v) {
    printf("%s = (%.2f, %.2f, %.2f)\n", label, v.x, v.y, v.z);
}

void vec3_print_report(Vec3 v) {
    printf(
        "report: x=%.2f y=%.2f z=%.2f | len=%.2f\n",
        v.x,
        v.y,
        v.z,
        vec3_length(v)
    );
}
