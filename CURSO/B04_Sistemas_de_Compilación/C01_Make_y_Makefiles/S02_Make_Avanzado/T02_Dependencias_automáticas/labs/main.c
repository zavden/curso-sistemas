#include <stdio.h>
#include "config.h"
#include "engine.h"

int main(void) {
    printf("Starting %s v%d\n", APP_NAME, VERSION);
    run_engine();
    return 0;
}
