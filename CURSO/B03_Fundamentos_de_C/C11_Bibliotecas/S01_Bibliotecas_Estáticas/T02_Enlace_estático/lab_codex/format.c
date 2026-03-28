#include "format.h"

#include <stdio.h>

const char *format_name(const char *title, const char *first_name, const char *last_name) {
    static char buffer[128];

    snprintf(buffer, sizeof(buffer), "%s %s %s", title, first_name, last_name);
    return buffer;
}
