#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "sample.txt";

    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 1;
    }

    char buf[256];
    int line_num = 0;

    printf("=== Stripping newlines with strcspn ===\n\n");

    while (fgets(buf, sizeof(buf), f) != NULL) {
        buf[strcspn(buf, "\n")] = '\0';
        line_num++;
        printf("%3d: \"%s\"\n", line_num, buf);
    }

    fclose(f);
    return 0;
}
