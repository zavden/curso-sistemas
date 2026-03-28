/* snprintf_buf.c — Format strings into buffers, detect truncation */

#include <stdio.h>
#include <string.h>

/*
 * build_greeting — format a greeting into dst (at most dst_size bytes).
 * Returns what snprintf returns: the number of chars that WOULD have
 * been written (excluding '\0') if the buffer were large enough.
 */
static int build_greeting(char *dst, size_t dst_size,
                          const char *user, int score) {
    return snprintf(dst, dst_size, "User: %s, Score: %d", user, score);
}

/*
 * copy_string — copy src into dst using snprintf (safe, always NUL-terminated).
 * Returns what snprintf returns: length of src (may exceed dst_size).
 */
static int copy_string(char *dst, size_t dst_size, const char *src) {
    return snprintf(dst, dst_size, "%s", src);
}

int main(void) {
    /* --- Demonstrate snprintf basic usage --- */
    char small_buf[20];
    char large_buf[100];

    const char *user = "Alice";
    int score = 95;

    /* Buffer large enough: no truncation */
    int needed = build_greeting(large_buf, sizeof(large_buf), user, score);
    printf("large_buf: \"%s\"\n", large_buf);
    printf("  needed=%d, capacity=%zu, truncated=%s\n\n",
           needed, sizeof(large_buf),
           needed >= (int)sizeof(large_buf) ? "YES" : "NO");

    /* Buffer too small: truncation occurs */
    needed = build_greeting(small_buf, sizeof(small_buf), user, score);
    printf("small_buf: \"%s\"\n", small_buf);
    printf("  needed=%d, capacity=%zu, truncated=%s\n",
           needed, sizeof(small_buf),
           needed >= (int)sizeof(small_buf) ? "YES" : "NO");
    if (needed >= (int)sizeof(small_buf)) {
        printf("  WARNING: output truncated, needed %d+1 bytes\n", needed);
    }

    /* --- String builder pattern with snprintf --- */
    printf("\n--- String builder ---\n\n");

    char report[80];
    int pos = 0;
    int ret;

    const char *names[] = {"cpu_temp", "gpu_temp", "fan_rpm"};
    double values[] = {72.5, 68.3, 1800.0};
    int count = 3;

    for (int i = 0; i < count; i++) {
        ret = snprintf(report + pos, sizeof(report) - (size_t)pos,
                       "%s=%.1f ", names[i], values[i]);

        if (ret < 0) {
            fprintf(stderr, "snprintf encoding error\n");
            return 1;
        }

        if (pos + ret >= (int)sizeof(report)) {
            printf("Truncation at entry %d (\"%s\")\n", i, names[i]);
            printf("Buffer full: \"%s\"\n", report);
            break;
        }

        pos += ret;
    }

    printf("Report (%d/%zu bytes): \"%s\"\n", pos, sizeof(report), report);

    /* --- Show safety of snprintf with long input --- */
    printf("\n--- snprintf with long input ---\n\n");

    char safe_buf[16];
    const char *long_name = "a_very_long_sensor_name_that_exceeds_the_buffer";

    /* snprintf: safe, truncates instead of overflowing */
    ret = copy_string(safe_buf, sizeof(safe_buf), long_name);
    printf("snprintf result: \"%s\" (needed %d, capacity %zu)\n",
           safe_buf, ret, sizeof(safe_buf));
    printf("strlen(safe_buf) = %zu (always < capacity)\n", strlen(safe_buf));

    return 0;
}
