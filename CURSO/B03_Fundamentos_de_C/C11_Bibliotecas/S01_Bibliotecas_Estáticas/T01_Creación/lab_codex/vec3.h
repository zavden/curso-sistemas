#ifndef VEC3_H
#define VEC3_H

typedef struct {
    double x;
    double y;
    double z;
} Vec3;

Vec3 vec3_create(double x, double y, double z);
Vec3 vec3_add(Vec3 a, Vec3 b);
double vec3_dot(Vec3 a, Vec3 b);
double vec3_length(Vec3 v);
void vec3_print(const char *label, Vec3 v);
void vec3_print_report(Vec3 v);

#endif
