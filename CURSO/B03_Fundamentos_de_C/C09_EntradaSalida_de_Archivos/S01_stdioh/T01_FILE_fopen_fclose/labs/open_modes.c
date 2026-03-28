/* open_modes.c -- Demonstrate the effect of w, a, r+ modes */

#include <stdio.h>
#include <stdlib.h>

static void print_file_contents(const char *path, const char *label) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("[%s] File does not exist or cannot be read\n", label);
        return;
    }

    printf("[%s] Contents of '%s':\n", label, path);
    char buf[256];
    while (fgets(buf, sizeof(buf), f) != NULL) {
        printf("  %s", buf);
    }
    printf("\n");
    fclose(f);
}

int main(void) {
    const char *path = "modes_test.txt";

    /* Step 1: "w" creates the file and writes */
    printf("=== Step 1: fopen(\"%s\", \"w\") ===\n", path);
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }
    fprintf(f, "line 1 (written with w)\n");
    fclose(f);
    print_file_contents(path, "After w");

    /* Step 2: "a" appends without truncating */
    printf("=== Step 2: fopen(\"%s\", \"a\") ===\n", path);
    f = fopen(path, "a");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }
    fprintf(f, "line 2 (appended with a)\n");
    fclose(f);
    print_file_contents(path, "After a");

    /* Step 3: "r+" reads and writes from the beginning */
    printf("=== Step 3: fopen(\"%s\", \"r+\") ===\n", path);
    f = fopen(path, "r+");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }
    fprintf(f, "OVERWRITTEN");
    fclose(f);
    print_file_contents(path, "After r+");

    /* Step 4: "w" truncates everything and starts fresh */
    printf("=== Step 4: fopen(\"%s\", \"w\") again ===\n", path);
    f = fopen(path, "w");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }
    fprintf(f, "line 3 (only line after w truncated)\n");
    fclose(f);
    print_file_contents(path, "After second w");

    return EXIT_SUCCESS;
}
