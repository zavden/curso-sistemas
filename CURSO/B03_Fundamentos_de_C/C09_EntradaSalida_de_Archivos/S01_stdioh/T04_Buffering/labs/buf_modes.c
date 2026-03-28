#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* stdout to terminal: line buffered */
    printf("[stdout] tick 1");
    fprintf(stderr, "[stderr] immediate 1\n");
    sleep(1);

    printf("[stdout] tick 2");
    fprintf(stderr, "[stderr] immediate 2\n");
    sleep(1);

    printf("[stdout] tick 3\n");
    /* The '\n' above flushes stdout — all three ticks appear at once */

    /* File: full buffered */
    FILE *f = fopen("buf_output.txt", "w");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }
    fprintf(f, "line one\n");
    fprintf(f, "line two\n");
    fprintf(f, "line three\n");
    /* Data is in the buffer — not yet written to disk */
    fclose(f);
    /* fclose flushes — now it is on disk */

    printf("Wrote 3 lines to buf_output.txt\n");
    return 0;
}
