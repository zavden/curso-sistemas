#include <stdio.h>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_LEVEL_COUNT,
};

static const char *level_names[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO]  = "INFO",
    [LOG_WARN]  = "WARN",
    [LOG_ERROR] = "ERROR",
};

static const char *level_colors[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "gray",
    [LOG_INFO]  = "green",
    [LOG_WARN]  = "yellow",
    [LOG_ERROR] = "red",
};

const char *log_level_str(enum LogLevel level) {
    if (level >= 0 && level < LOG_LEVEL_COUNT) {
        return level_names[level];
    }
    return "UNKNOWN";
}

int main(void) {
    printf("LOG_LEVEL_COUNT = %d\n\n", LOG_LEVEL_COUNT);

    printf("--- Iterar con COUNT ---\n");
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        printf("  [%d] %-5s (color: %s)\n", i, level_names[i], level_colors[i]);
    }

    printf("\n--- Conversion segura ---\n");
    printf("log_level_str(LOG_WARN) = %s\n", log_level_str(LOG_WARN));
    printf("log_level_str(99)       = %s\n", log_level_str(99));

    printf("\n--- Array size check ---\n");
    size_t names_count = sizeof(level_names) / sizeof(level_names[0]);
    printf("sizeof(level_names) / sizeof(level_names[0]) = %zu\n", names_count);
    printf("LOG_LEVEL_COUNT = %d\n", LOG_LEVEL_COUNT);
    printf("Match: %s\n", names_count == LOG_LEVEL_COUNT ? "yes" : "NO - out of sync!");

    return 0;
}
