#include "greet.h"

#include "format.h"

#include <stdio.h>

void greet_person(const char *title, const char *first_name, const char *last_name) {
    printf("Hello, %s!\n", format_name(title, first_name, last_name));
}
