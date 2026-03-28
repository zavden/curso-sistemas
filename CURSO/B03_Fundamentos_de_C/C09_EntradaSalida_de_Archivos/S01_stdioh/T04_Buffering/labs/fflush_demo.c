#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Loading...");
    fflush(stdout);
    /* Without the fflush above, "Loading..." would stay in the buffer
       until the '\n' in the next printf (2 seconds later). */
    sleep(2);
    printf(" Done!\n");

    /* Write to a file and flush immediately */
    FILE *f = fopen("fflush_output.txt", "w");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }
    fprintf(f, "event 1\n");
    fflush(f);
    /* At this point "event 1" is already on disk */

    fprintf(f, "event 2\n");
    fflush(f);

    fprintf(f, "event 3\n");
    /* event 3 is NOT yet on disk — still in the buffer */
    fclose(f);
    /* Now event 3 is on disk too */

    printf("Check fflush_output.txt\n");
    return 0;
}
