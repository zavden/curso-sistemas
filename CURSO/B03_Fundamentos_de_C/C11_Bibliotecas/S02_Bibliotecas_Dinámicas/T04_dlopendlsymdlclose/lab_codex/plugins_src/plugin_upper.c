#include "../plugin_api.h"

#include <ctype.h>
#include <stdio.h>

static int init(void) {
    printf("[upper] init\n");
    return 0;
}

static void process(const char *input) {
    const unsigned char *p = (const unsigned char *)input;

    printf("[upper] ");
    while (*p != '\0') {
        putchar(toupper(*p));
        p++;
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[upper] cleanup\n");
}

Plugin *plugin_create(void) {
    static Plugin plugin = {
        "upper",
        "1.0",
        init,
        process,
        cleanup
    };

    return &plugin;
}
