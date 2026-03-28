#include "portable_math.h"

int sum_i32(const int *values, size_t len) {
    size_t i;
    int total = 0;

    for (i = 0; i < len; i++) {
        total += values[i];
    }

    return total;
}

int mean_i32(const int *values, size_t len) {
    if (len == 0) {
        return 0;
    }

    return sum_i32(values, len) / (int)len;
}

int clamp_i32(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}
