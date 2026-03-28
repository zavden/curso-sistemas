#include "greet.h"
#include "format.h"
#include <stdio.h>

void greet_person(const char *name)
{
    const char *formatted = format_name(name);
    printf("Hello, %s!\n", formatted);
}
