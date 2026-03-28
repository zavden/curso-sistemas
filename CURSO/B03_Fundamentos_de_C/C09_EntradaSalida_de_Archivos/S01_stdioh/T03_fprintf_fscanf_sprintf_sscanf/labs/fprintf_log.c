/* fprintf_log.c — Write formatted data to a file and to stderr */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int main(void) {
    const char *filename = "server.log";
    FILE *log = fopen(filename, "w");
    if (!log) {
        fprintf(stderr, "Error: cannot open %s: %s\n",
                filename, strerror(errno));
        return 1;
    }

    /* Write structured log entries with timestamp */
    time_t now = time(NULL);

    fprintf(log, "=== Server Log ===\n");
    fprintf(log, "Started at: %ld\n", (long)now);
    fprintf(log, "%-12s %-8s %s\n", "TIMESTAMP", "LEVEL", "MESSAGE");
    fprintf(log, "%-12ld %-8s %s\n", (long)now, "INFO", "Server started");
    fprintf(log, "%-12ld %-8s %s\n", (long)now + 1, "WARN", "High memory usage: 87.3%");
    fprintf(log, "%-12ld %-8s %s\n", (long)now + 2, "ERROR", "Connection refused on port 8080");
    fprintf(log, "%-12ld %-8s %s\n", (long)now + 3, "INFO", "Server stopped");

    /* Also write a summary line with different format specifiers */
    int total_requests = 1542;
    double avg_response_ms = 23.756;
    fprintf(log, "\nSummary: %d requests, avg response %.2f ms\n",
            total_requests, avg_response_ms);

    fclose(log);

    /* Use fprintf to stderr for error messages */
    fprintf(stderr, "Log written to %s\n", filename);

    /* Read back and display */
    log = fopen(filename, "r");
    if (!log) {
        fprintf(stderr, "Error: cannot reopen %s: %s\n",
                filename, strerror(errno));
        return 1;
    }

    printf("--- Contents of %s ---\n", filename);
    char buf[256];
    while (fgets(buf, sizeof(buf), log)) {
        printf("%s", buf);
    }

    fclose(log);
    return 0;
}
