#include <stdio.h>
#include "linkage_logger.h"

static int count = 0;

void logger_log(const char *msg) {
    count++;
    printf("[LOG #%d] %s\n", count, msg);
}

int logger_get_count(void) {
    return count;
}
