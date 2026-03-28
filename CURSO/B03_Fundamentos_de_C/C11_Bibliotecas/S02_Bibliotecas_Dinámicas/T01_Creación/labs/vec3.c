#include "vec3.h"
#include <stdio.h>
#include <math.h>

Vec3 vec3_create(double x, double y, double z) {
    return (Vec3){x, y, z};
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double vec3_length(Vec3 v) {
    return sqrt(vec3_dot(v, v));
}

void vec3_print(Vec3 v) {
    printf("(%.2f, %.2f, %.2f)\n", v.x, v.y, v.z);
}
