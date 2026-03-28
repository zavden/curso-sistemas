#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 256

/* ================================================================
 * Style 1: nested if-else (no goto, no cleanup attribute)
 * ================================================================ */

int nested_style(const char *path1, const char *path2, const char *path3) {
    printf("[nested] acquiring resources...\n");
    int result = -1;

    FILE *f1 = fopen(path1, "r");
    if (f1) {
        FILE *f2 = fopen(path2, "r");
        if (f2) {
            FILE *f3 = fopen(path3, "r");
            if (f3) {
                char *buf1 = malloc(BUFFER_SIZE);
                if (buf1) {
                    char *buf2 = malloc(BUFFER_SIZE);
                    if (buf2) {
                        /* --- actual work --- */
                        if (fgets(buf1, BUFFER_SIZE, f1))
                            printf("[nested] read from file1: %s", buf1);
                        if (fgets(buf2, BUFFER_SIZE, f2))
                            printf("[nested] read from file2: %s", buf2);
                        result = 0;
                        /* --- cleanup --- */
                        free(buf2);
                    }
                    free(buf1);
                }
                fclose(f3);
            } else {
                perror("[nested] fopen path3");
            }
            fclose(f2);
        } else {
            perror("[nested] fopen path2");
        }
        fclose(f1);
    } else {
        perror("[nested] fopen path1");
    }

    printf("[nested] done (result: %d)\n", result);
    return result;
}

/* ================================================================
 * Style 2: goto cleanup
 * ================================================================ */

int goto_style(const char *path1, const char *path2, const char *path3) {
    printf("[goto] acquiring resources...\n");
    int result = -1;
    FILE *f1 = NULL;
    FILE *f2 = NULL;
    FILE *f3 = NULL;
    char *buf1 = NULL;
    char *buf2 = NULL;

    f1 = fopen(path1, "r");
    if (!f1) { perror("[goto] fopen path1"); goto cleanup; }

    f2 = fopen(path2, "r");
    if (!f2) { perror("[goto] fopen path2"); goto cleanup; }

    f3 = fopen(path3, "r");
    if (!f3) { perror("[goto] fopen path3"); goto cleanup; }

    buf1 = malloc(BUFFER_SIZE);
    if (!buf1) { perror("[goto] malloc buf1"); goto cleanup; }

    buf2 = malloc(BUFFER_SIZE);
    if (!buf2) { perror("[goto] malloc buf2"); goto cleanup; }

    /* --- actual work --- */
    if (fgets(buf1, BUFFER_SIZE, f1))
        printf("[goto] read from file1: %s", buf1);
    if (fgets(buf2, BUFFER_SIZE, f2))
        printf("[goto] read from file2: %s", buf2);
    result = 0;

cleanup:
    free(buf2);
    free(buf1);
    if (f3) fclose(f3);
    if (f2) fclose(f2);
    if (f1) fclose(f1);

    printf("[goto] done (result: %d)\n", result);
    return result;
}

/* ================================================================
 * Style 3: __attribute__((cleanup))
 * ================================================================ */

void auto_free(void *p) { free(*(void **)p); }
void auto_fclose(FILE **fp) { if (*fp) fclose(*fp); }

#define AUTO_FREE __attribute__((cleanup(auto_free)))
#define AUTO_FCLOSE __attribute__((cleanup(auto_fclose)))

int cleanup_attr_style(const char *path1, const char *path2,
                       const char *path3) {
    printf("[attr] acquiring resources...\n");

    AUTO_FCLOSE FILE *f1 = fopen(path1, "r");
    if (!f1) { perror("[attr] fopen path1"); return -1; }

    AUTO_FCLOSE FILE *f2 = fopen(path2, "r");
    if (!f2) { perror("[attr] fopen path2"); return -1; }

    AUTO_FCLOSE FILE *f3 = fopen(path3, "r");
    if (!f3) { perror("[attr] fopen path3"); return -1; }

    AUTO_FREE char *buf1 = malloc(BUFFER_SIZE);
    if (!buf1) { perror("[attr] malloc buf1"); return -1; }

    AUTO_FREE char *buf2 = malloc(BUFFER_SIZE);
    if (!buf2) { perror("[attr] malloc buf2"); return -1; }

    /* --- actual work --- */
    if (fgets(buf1, BUFFER_SIZE, f1))
        printf("[attr] read from file1: %s", buf1);
    if (fgets(buf2, BUFFER_SIZE, f2))
        printf("[attr] read from file2: %s", buf2);

    printf("[attr] done (result: 0)\n");
    return 0;
    /* all 5 resources cleaned up automatically */
}

/* ================================================================
 * main — run all three styles
 * ================================================================ */

int main(void) {
    /* create test files */
    FILE *t1 = fopen("/tmp/raii_test1.txt", "w");
    FILE *t2 = fopen("/tmp/raii_test2.txt", "w");
    FILE *t3 = fopen("/tmp/raii_test3.txt", "w");
    if (!t1 || !t2 || !t3) {
        fprintf(stderr, "error: cannot create test files\n");
        if (t1) fclose(t1);
        if (t2) fclose(t2);
        if (t3) fclose(t3);
        return 1;
    }
    fputs("line from file 1\n", t1);
    fputs("line from file 2\n", t2);
    fputs("line from file 3\n", t3);
    fclose(t1);
    fclose(t2);
    fclose(t3);

    printf("=== comparing three styles ===\n\n");

    printf("--- style 1: nested if-else ---\n");
    nested_style("/tmp/raii_test1.txt", "/tmp/raii_test2.txt",
                 "/tmp/raii_test3.txt");

    printf("\n--- style 2: goto cleanup ---\n");
    goto_style("/tmp/raii_test1.txt", "/tmp/raii_test2.txt",
               "/tmp/raii_test3.txt");

    printf("\n--- style 3: cleanup attribute ---\n");
    cleanup_attr_style("/tmp/raii_test1.txt", "/tmp/raii_test2.txt",
                       "/tmp/raii_test3.txt");

    printf("\n--- testing error paths ---\n");
    printf("\n[nested] with bad path:\n");
    nested_style("/tmp/raii_test1.txt", "/tmp/nonexistent.txt",
                 "/tmp/raii_test3.txt");

    printf("\n[goto] with bad path:\n");
    goto_style("/tmp/raii_test1.txt", "/tmp/nonexistent.txt",
               "/tmp/raii_test3.txt");

    printf("\n[attr] with bad path:\n");
    cleanup_attr_style("/tmp/raii_test1.txt", "/tmp/nonexistent.txt",
                       "/tmp/raii_test3.txt");

    /* remove test files */
    remove("/tmp/raii_test1.txt");
    remove("/tmp/raii_test2.txt");
    remove("/tmp/raii_test3.txt");

    return 0;
}
