#include <stdio.h>
#include "linkage_counter.h"
#include "linkage_logger.h"

int main(void) {
    counter_increment();
    counter_increment();
    counter_increment();

    logger_log("system started");
    logger_log("processing data");

    printf("Counter value: %d\n", counter_get());
    printf("Logger count: %d\n", logger_get_count());

    return 0;
}
