/* mix_danger.c -- Demonstrate the danger of mixing stdio and POSIX I/O */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

int main(void)
{
    /* --- Without fflush: order is wrong --- */

    printf("=== Without fflush (broken order) ===\n");

    FILE *f = fopen("mix_broken.txt", "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    int fd = fileno(f);

    fprintf(f, "AAA: written with fprintf (first)\n");
    /* No fflush here -- AAA is still in stdio buffer */
    write(fd, "BBB: written with write() (second)\n", 35);
    fprintf(f, "CCC: written with fprintf (third)\n");

    fclose(f);

    /* Show the result */
    FILE *r = fopen("mix_broken.txt", "r");
    char line[256];
    while (fgets(line, sizeof(line), r)) {
        printf("  %s", line);
    }
    fclose(r);

    /* --- With fflush: order is correct --- */

    printf("\n=== With fflush (correct order) ===\n");

    f = fopen("mix_fixed.txt", "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fd = fileno(f);

    fprintf(f, "AAA: written with fprintf (first)\n");
    fflush(f);  /* Flush stdio buffer before using fd */
    write(fd, "BBB: written with write() (second)\n", 35);
    /* write goes directly to kernel, so no flush needed before fprintf */
    fprintf(f, "CCC: written with fprintf (third)\n");

    fclose(f);

    /* Show the result */
    r = fopen("mix_fixed.txt", "r");
    while (fgets(line, sizeof(line), r)) {
        printf("  %s", line);
    }
    fclose(r);

    return 0;
}
