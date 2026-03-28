#include <stdio.h>
#include "config.h"

int verbose = 0;
int max_retries = 3;

void config_print(void) {
    printf("verbose     = %d\n", verbose);
    printf("max_retries = %d\n", max_retries);
}
