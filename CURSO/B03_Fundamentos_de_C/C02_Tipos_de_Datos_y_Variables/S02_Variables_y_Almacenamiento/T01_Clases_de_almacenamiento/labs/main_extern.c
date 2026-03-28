#include <stdio.h>
#include "config.h"

int main(void) {
    printf("=== Before modification ===\n");
    config_print();

    verbose = 1;
    max_retries = 5;

    printf("\n=== After modification ===\n");
    config_print();

    return 0;
}
