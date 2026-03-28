/*
 * debug_log.c — Macros de logging y assert personalizado usando
 *               __FILE__, __LINE__ y __func__.
 *
 * Demuestra el patron clave: las macros capturan la ubicacion
 * del caller, no de donde se definio el macro.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* --- Logging macro --- */

#define LOG(level, fmt, ...) \
    fprintf(stderr, "[%s] %s:%d %s(): " fmt "\n", \
            level, __FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * Wrappers that always pass at least one variadic argument.
 * This avoids the GNU ##__VA_ARGS__ extension and keeps
 * the code strictly C11-compliant with -Wpedantic.
 */
#define LOG_INFO(fmt, ...)  LOG("INFO",  fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG("WARN",  fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG("ERROR", fmt, __VA_ARGS__)

/* --- Custom assert --- */

#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, \
            "Assertion failed: %s\n" \
            "  File:     %s\n" \
            "  Line:     %d\n" \
            "  Function: %s\n", \
            #cond, __FILE__, __LINE__, __func__); \
        abort(); \
    } \
} while (0)

/* --- Error return macro --- */

#define RETURN_ERROR(code, fmt, ...) do { \
    LOG_ERROR(fmt, __VA_ARGS__); \
    return (code); \
} while (0)

/* --- Functions that use the macros --- */

static int open_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        RETURN_ERROR(-1, "cannot open '%s': %s", path, strerror(errno));
    }
    fclose(f);
    return 0;
}

static void process_data(int value) {
    LOG_INFO("processing value %d", value);
    ASSERT(value >= 0);
    LOG_INFO("value %d OK, continuing", value);
}

int main(void) {
    printf("=== Logging macros demo ===\n\n");

    LOG_INFO("program started, pid=%d", (int)getpid());
    LOG_WARN("this is a warning from %s", "main");

    printf("\n--- Calling open_config with invalid path ---\n\n");
    int result = open_config("/nonexistent/config.txt");
    printf("open_config returned: %d\n", result);

    printf("\n--- Calling process_data with valid value ---\n\n");
    process_data(42);

    printf("\n--- Calling process_data with invalid value ---\n");
    printf("(this will trigger ASSERT and abort)\n\n");
    process_data(-1);

    /* This line is never reached. */
    return 0;
}
