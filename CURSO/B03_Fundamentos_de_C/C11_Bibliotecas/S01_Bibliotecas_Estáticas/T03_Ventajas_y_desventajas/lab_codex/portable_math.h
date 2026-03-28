#ifndef PORTABLE_MATH_H
#define PORTABLE_MATH_H

#include <stddef.h>

int sum_i32(const int *values, size_t len);
int mean_i32(const int *values, size_t len);
int clamp_i32(int value, int min_value, int max_value);

#endif
