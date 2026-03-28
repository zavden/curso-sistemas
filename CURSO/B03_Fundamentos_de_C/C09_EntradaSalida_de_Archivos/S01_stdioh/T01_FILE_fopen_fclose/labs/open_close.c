/* open_close.c -- Open a file, verify success, and close it properly */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *path = "testfile.txt";

    /* Open for writing -- creates if not exists, truncates if exists */
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    fprintf(f, "Hello from open_close\n");
    printf("File '%s' opened and written successfully\n", path);

    if (fclose(f) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }

    printf("File '%s' closed successfully\n", path);

    /* Now open for reading */
    f = fopen(path, "r");
    if (f == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    char buf[256];
    if (fgets(buf, sizeof(buf), f) != NULL) {
        printf("Read back: %s", buf);
    }

    fclose(f);
    return EXIT_SUCCESS;
}
