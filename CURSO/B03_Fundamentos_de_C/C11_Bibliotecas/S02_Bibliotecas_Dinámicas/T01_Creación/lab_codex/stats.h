#ifndef STATS_H
#define STATS_H

#include <stddef.h>

#ifdef BUILDING_STATS
#define STATS_API __attribute__((visibility("default")))
#else
#define STATS_API
#endif

STATS_API int stats_sum(const int *values, size_t len);
STATS_API double stats_mean(const int *values, size_t len);
STATS_API void stats_print_summary(const int *values, size_t len);

#endif
