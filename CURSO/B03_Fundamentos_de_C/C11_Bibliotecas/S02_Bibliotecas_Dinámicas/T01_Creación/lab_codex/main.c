#include "stats.h"

#include <stdio.h>

int main(void) {
    int values[] = {3, 6, 9, 12, 15};

    printf("sum: %d\n", stats_sum(values, 5));
    printf("mean: %.2f\n", stats_mean(values, 5));
    stats_print_summary(values, 5);

    return 0;
}
