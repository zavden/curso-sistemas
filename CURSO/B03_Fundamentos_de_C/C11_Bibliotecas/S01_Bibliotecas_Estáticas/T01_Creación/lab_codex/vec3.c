#include "vec3.h"

#include <math.h>

Vec3 vec3_create(double x, double y, double z) {
    Vec3 v = {x, y, z};
    return v;
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 result = {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
    };

    return result;
}

double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double vec3_length(Vec3 v) {
    return sqrt(vec3_dot(v, v));
}
