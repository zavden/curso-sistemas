#include "format.h"
#include <stdio.h>

static char buf[256];

const char *format_name(const char *name)
{
    snprintf(buf, sizeof(buf), "[%s]", name);
    return buf;
}
