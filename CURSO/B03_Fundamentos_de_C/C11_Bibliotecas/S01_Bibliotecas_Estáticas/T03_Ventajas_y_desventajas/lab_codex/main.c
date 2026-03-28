#include "portable_math.h"

#include <stdio.h>

int main(void) {
    int samples[] = {8, 11, 12, 14, 16, 20};
    int sum = sum_i32(samples, sizeof(samples) / sizeof(samples[0]));
    int mean = mean_i32(samples, sizeof(samples) / sizeof(samples[0]));
    int clamped = clamp_i32(mean, 10, 15);

    printf("sum: %d\n", sum);
    printf("mean: %d\n", mean);
    printf("clamped mean: %d\n", clamped);

    return 0;
}
