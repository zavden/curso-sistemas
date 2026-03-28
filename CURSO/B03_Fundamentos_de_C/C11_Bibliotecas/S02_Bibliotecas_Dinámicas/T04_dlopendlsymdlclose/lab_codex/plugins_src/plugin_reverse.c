#include "../plugin_api.h"

#include <stdio.h>
#include <string.h>

static int init(void) {
    printf("[reverse] init\n");
    return 0;
}

static void process(const char *input) {
    size_t len = strlen(input);
    size_t i;

    printf("[reverse] ");
    for (i = len; i > 0; i--) {
        putchar(input[i - 1]);
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[reverse] cleanup\n");
}

Plugin *plugin_create(void) {
    static Plugin plugin = {
        "reverse",
        "1.0",
        init,
        process,
        cleanup
    };

    return &plugin;
}
