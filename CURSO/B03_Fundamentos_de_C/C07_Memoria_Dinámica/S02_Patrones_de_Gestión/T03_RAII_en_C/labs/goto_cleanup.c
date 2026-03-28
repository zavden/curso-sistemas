#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

int copy_file(const char *input_path, const char *output_path) {
    int result = -1;
    FILE *fin = NULL;
    FILE *fout = NULL;
    char *buffer = NULL;

    fin = fopen(input_path, "r");
    if (!fin) {
        perror("fopen input");
        goto cleanup;
    }

    fout = fopen(output_path, "w");
    if (!fout) {
        perror("fopen output");
        goto cleanup;
    }

    buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        goto cleanup;
    }

    printf("[info] resources acquired: fin, fout, buffer (%d bytes)\n",
           BUFFER_SIZE);

    while (fgets(buffer, BUFFER_SIZE, fin)) {
        fputs(buffer, fout);
    }

    if (ferror(fin)) {
        perror("read error");
        goto cleanup;
    }

    printf("[info] copy complete\n");
    result = 0;

cleanup:
    printf("[cleanup] freeing buffer... ");
    free(buffer);
    printf("done\n");

    printf("[cleanup] closing fout... ");
    if (fout) fclose(fout);
    printf("done\n");

    printf("[cleanup] closing fin... ");
    if (fin) fclose(fin);
    printf("done\n");

    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    printf("--- goto cleanup demo ---\n\n");

    int ret = copy_file(argv[1], argv[2]);

    printf("\nresult: %s (code %d)\n", ret == 0 ? "success" : "failure", ret);
    return ret;
}
