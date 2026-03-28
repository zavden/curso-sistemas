#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "greeting.h"

void greet(const char *name)
{
    printf("Hello, %s!\n", name);
}

void greet_loud(const char *name)
{
    printf("HELLO, ");
    for (size_t i = 0; i < strlen(name); i++) {
        putchar(toupper((unsigned char)name[i]));
    }
    printf("!!!\n");
}
