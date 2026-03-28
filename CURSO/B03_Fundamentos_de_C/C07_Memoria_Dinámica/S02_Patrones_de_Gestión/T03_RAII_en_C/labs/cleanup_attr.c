#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

/* --- cleanup functions --- */

void cleanup_free(void *p) {
    printf("[auto-cleanup] freeing pointer... ");
    free(*(void **)p);
    printf("done\n");
}

void cleanup_fclose(FILE **fp) {
    printf("[auto-cleanup] closing file... ");
    if (*fp) {
        fclose(*fp);
    }
    printf("done\n");
}

/* --- macros for readability --- */

#define AUTO_FREE __attribute__((cleanup(cleanup_free)))
#define AUTO_FCLOSE __attribute__((cleanup(cleanup_fclose)))

/* --- main logic --- */

int copy_file(const char *input_path, const char *output_path) {
    AUTO_FCLOSE FILE *fin = fopen(input_path, "r");
    if (!fin) {
        perror("fopen input");
        return -1;  /* cleanup_fclose(&fin) called automatically */
    }

    AUTO_FCLOSE FILE *fout = fopen(output_path, "w");
    if (!fout) {
        perror("fopen output");
        return -1;  /* cleanup_fclose(&fout), cleanup_fclose(&fin) called */
    }

    AUTO_FREE char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("malloc");
        return -1;  /* all three cleanups called automatically */
    }

    printf("[info] resources acquired: fin, fout, buffer (%d bytes)\n",
           BUFFER_SIZE);

    while (fgets(buffer, BUFFER_SIZE, fin)) {
        fputs(buffer, fout);
    }

    if (ferror(fin)) {
        perror("read error");
        return -1;  /* all three cleanups called automatically */
    }

    printf("[info] copy complete\n");
    return 0;
    /* all three cleanups called automatically on normal return too */
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input> <output>\n", argv[0]);
        return 1;
    }

    printf("--- cleanup attribute demo ---\n\n");

    int ret = copy_file(argv[1], argv[2]);

    printf("\nresult: %s (code %d)\n", ret == 0 ? "success" : "failure", ret);
    return ret;
}
