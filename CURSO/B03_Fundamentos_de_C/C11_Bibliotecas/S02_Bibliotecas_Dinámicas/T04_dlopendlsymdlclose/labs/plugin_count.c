/* plugin_count.c -- plugin that counts characters, words, and lines */
#include "plugin.h"
#include <stdio.h>
#include <ctype.h>

static int init(void) {
    printf("[count] initialized\n");
    return 0;
}

static void process(const char *input) {
    int chars = 0, words = 0, lines = 0;
    int in_word = 0;

    for (const char *p = input; *p; p++) {
        chars++;
        if (*p == '\n') lines++;
        if (isspace((unsigned char)*p)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }

    printf("[count] chars=%d words=%d lines=%d\n", chars, words, lines);
}

static void cleanup(void) {
    printf("[count] cleaned up\n");
}

Plugin *plugin_create(void) {
    static Plugin p = {
        .name    = "count",
        .version = "1.0",
        .init    = init,
        .process = process,
        .cleanup = cleanup,
    };
    return &p;
}
