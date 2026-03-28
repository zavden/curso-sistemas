/* plugin_upper.c -- plugin that converts input to uppercase */
#include "plugin.h"
#include <stdio.h>
#include <ctype.h>

static int init(void) {
    printf("[upper] initialized\n");
    return 0;
}

static void process(const char *input) {
    printf("[upper] ");
    for (const char *p = input; *p; p++) {
        putchar(toupper((unsigned char)*p));
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[upper] cleaned up\n");
}

Plugin *plugin_create(void) {
    static Plugin p = {
        .name    = "uppercase",
        .version = "1.0",
        .init    = init,
        .process = process,
        .cleanup = cleanup,
    };
    return &p;
}
