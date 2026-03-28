#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "sample.txt";

    /* --- Write sample lines with fputs --- */
    FILE *fw = fopen(path, "w");
    if (!fw) {
        perror(path);
        return 1;
    }

    fputs("short\n", fw);
    fputs("a medium-length line of text\n", fw);
    fputs("this is a deliberately long line that exceeds a small buffer size easily\n", fw);
    fputs("last line without newline", fw);
    fclose(fw);

    /* --- Read with fgets using a small buffer --- */
    FILE *fr = fopen(path, "r");
    if (!fr) {
        perror(path);
        return 1;
    }

    char buf[20];
    int chunk = 0;
    printf("=== Reading with buffer of %zu bytes ===\n\n", sizeof(buf));

    while (fgets(buf, sizeof(buf), fr) != NULL) {
        chunk++;
        size_t len = strlen(buf);
        int has_newline = (len > 0 && buf[len - 1] == '\n');
        printf("chunk %2d | len=%2zu | newline=%s | \"%s\"\n",
               chunk, len, has_newline ? "yes" : "no ", buf);
    }

    fclose(fr);
    return 0;
}
