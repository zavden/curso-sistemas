#include <stdio.h>
#include "linkage_counter.h"

static int count = 0;

void counter_increment(void) {
    count++;
}

int counter_get(void) {
    return count;
}
