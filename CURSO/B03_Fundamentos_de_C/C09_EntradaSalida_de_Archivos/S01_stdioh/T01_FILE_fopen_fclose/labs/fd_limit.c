/* fd_limit.c -- Show what happens when too many files are left open */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

int main(void) {
    /* Lower the soft limit to 30 so we can see the failure quickly.
       3 FDs are already used (stdin, stdout, stderr), so we can open
       at most ~27 additional files before hitting the limit. */
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    printf("Original limit: soft=%lu, hard=%lu\n",
           (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);

    rl.rlim_cur = 30;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        perror("setrlimit");
        return EXIT_FAILURE;
    }
    printf("Reduced soft limit to 30 for this demo\n\n");

    const int MAX_ATTEMPTS = 50;
    FILE *files[50];
    int opened = 0;
    char filename[64];

    printf("Opening files without closing them...\n");

    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        snprintf(filename, sizeof(filename), "tmp_leak_%04d.txt", i);
        files[i] = fopen(filename, "w");
        if (files[i] == NULL) {
            printf("\nFailed at file #%d: %s (errno=%d: EMFILE)\n",
                   i, strerror(errno), errno);
            printf("This is what happens when you forget to fclose()!\n");
            break;
        }
        opened++;
    }

    printf("\nOpened %d files before hitting the limit\n", opened);

    /* Clean up: close all opened files and remove temp files */
    printf("Cleaning up...\n");
    for (int i = 0; i < opened; i++) {
        fclose(files[i]);
        snprintf(filename, sizeof(filename), "tmp_leak_%04d.txt", i);
        remove(filename);
    }

    printf("Cleanup complete\n");
    return EXIT_SUCCESS;
}
