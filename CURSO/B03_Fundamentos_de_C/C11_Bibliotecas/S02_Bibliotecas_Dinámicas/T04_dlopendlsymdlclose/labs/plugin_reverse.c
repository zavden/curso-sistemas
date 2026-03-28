/* plugin_reverse.c -- plugin that reverses the input string */
#include "plugin.h"
#include <stdio.h>
#include <string.h>

static int init(void) {
    printf("[reverse] initialized\n");
    return 0;
}

static void process(const char *input) {
    size_t len = strlen(input);
    printf("[reverse] ");
    for (size_t i = len; i > 0; i--) {
        putchar(input[i - 1]);
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[reverse] cleaned up\n");
}

Plugin *plugin_create(void) {
    static Plugin p = {
        .name    = "reverse",
        .version = "1.0",
        .init    = init,
        .process = process,
        .cleanup = cleanup,
    };
    return &p;
}
