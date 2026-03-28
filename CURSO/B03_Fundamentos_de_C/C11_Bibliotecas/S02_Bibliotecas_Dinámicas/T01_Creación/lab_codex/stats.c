#include "stats.h"

#include <stdio.h>

int accumulate_internal(const int *values, size_t len) {
    size_t i;
    int total = 0;

    for (i = 0; i < len; i++) {
        total += values[i];
    }

    return total;
}

int stats_sum(const int *values, size_t len) {
    return accumulate_internal(values, len);
}

double stats_mean(const int *values, size_t len) {
    if (len == 0) {
        return 0.0;
    }

    return (double)accumulate_internal(values, len) / (double)len;
}

void stats_print_summary(const int *values, size_t len) {
    printf(
        "summary: count=%zu sum=%d mean=%.2f\n",
        len,
        stats_sum(values, len),
        stats_mean(values, len)
    );
}
