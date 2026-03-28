/* copy_stdio.c -- Copy a file using stdio (fopen/fread/fwrite/fclose) */

#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        return 1;
    }

    FILE *src = fopen(argv[1], "rb");
    if (!src) {
        perror("fopen source");
        return 1;
    }

    FILE *dst = fopen(argv[2], "wb");
    if (!dst) {
        perror("fopen destination");
        fclose(src);
        return 1;
    }

    char buf[BUF_SIZE];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            perror("fwrite");
            fclose(src);
            fclose(dst);
            return 1;
        }
    }

    if (ferror(src)) {
        perror("fread");
        fclose(src);
        fclose(dst);
        return 1;
    }

    fclose(src);
    fclose(dst);

    printf("stdio copy complete: %s -> %s\n", argv[1], argv[2]);
    return 0;
}
