#include <stdio.h>
#include "logger.h"
#include "config.h"

void log_message(const char *msg) {
    printf("[%s v%d] %s\n", APP_NAME, VERSION, msg);
}
