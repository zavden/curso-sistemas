#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int safe_mode = 0;

    if (argc > 1 && strcmp(argv[1], "--safe") == 0) {
        safe_mode = 1;
    }

    const char *filename = safe_mode ? "crash_log_safe.txt" : "crash_log.txt";

    FILE *log = fopen(filename, "w");
    if (log == NULL) {
        perror("fopen");
        return 1;
    }

    if (safe_mode) {
        /* Make the log file line buffered — each '\n' flushes to disk */
        setvbuf(log, NULL, _IOLBF, 0);
    }
    /* Default: full buffered — data stays in memory until buffer fills or fclose */

    fprintf(log, "step 1: initialized\n");
    fprintf(log, "step 2: processing\n");
    fprintf(log, "step 3: about to do risky operation\n");

    /* Simulate a crash (abort sends SIGABRT) */
    abort();

    /* This line is never reached */
    fprintf(log, "step 4: completed\n");
    fclose(log);
    return 0;
}
