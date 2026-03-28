/* bridge.c -- Convert between FILE* and fd using fileno() and fdopen() */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    /* --- Part A: fileno() -- get fd from FILE* --- */

    printf("=== fileno: FILE* -> fd ===\n");

    FILE *f = fopen("bridge_test.txt", "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    int fd = fileno(f);
    printf("FILE* opened, underlying fd = %d\n", fd);

    /* Write with stdio first, then flush, then write with POSIX */
    fprintf(f, "Line 1: written with fprintf\n");
    fflush(f);  /* Critical: flush stdio buffer before using fd directly */
    write(fd, "Line 2: written with write()\n", 29);

    fclose(f);  /* Also closes fd */

    printf("Wrote 2 lines to bridge_test.txt\n\n");

    /* --- Part B: fdopen() -- create FILE* from fd --- */

    printf("=== fdopen: fd -> FILE* ===\n");

    int fd2 = open("bridge_test.txt", O_RDONLY);
    if (fd2 == -1) {
        perror("open");
        return 1;
    }

    printf("Opened fd = %d with open()\n", fd2);

    FILE *f2 = fdopen(fd2, "r");
    if (!f2) {
        perror("fdopen");
        close(fd2);
        return 1;
    }

    printf("Created FILE* from fd with fdopen()\n");
    printf("Contents of bridge_test.txt:\n");

    char line[256];
    while (fgets(line, sizeof(line), f2)) {
        printf("  %s", line);
    }

    /* fclose closes both the FILE* and the underlying fd */
    fclose(f2);
    /* Do NOT call close(fd2) after fclose -- fd is already closed */

    printf("\nfclose(f2) also closed fd %d\n", fd2);

    return 0;
}
