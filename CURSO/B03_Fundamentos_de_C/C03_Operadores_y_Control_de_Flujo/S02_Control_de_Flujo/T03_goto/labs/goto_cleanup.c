#include <stdio.h>
#include <stdlib.h>

int process_file(const char *path) {
    int ret = -1;
    FILE *f = NULL;
    char *buffer = NULL;
    int *data = NULL;

    /* Step 1: open file */
    f = fopen(path, "r");
    if (f == NULL) {
        printf("[FAIL] Cannot open '%s'\n", path);
        goto cleanup;
    }
    printf("[OK] File opened: %s\n", path);

    /* Step 2: allocate buffer */
    buffer = malloc(1024);
    if (buffer == NULL) {
        printf("[FAIL] malloc buffer failed\n");
        goto cleanup;
    }
    printf("[OK] Buffer allocated (1024 bytes)\n");

    /* Step 3: allocate data array */
    data = malloc(100 * sizeof(int));
    if (data == NULL) {
        printf("[FAIL] malloc data failed\n");
        goto cleanup;
    }
    printf("[OK] Data allocated (%zu bytes)\n", 100 * sizeof(int));

    /* Step 4: use the resources */
    if (fgets(buffer, 1024, f) != NULL) {
        printf("[OK] Read first line: %s", buffer);
        /* Add newline if the line didn't end with one */
        size_t len = 0;
        while (buffer[len] != '\0') {
            len++;
        }
        if (len == 0 || buffer[len - 1] != '\n') {
            printf("\n");
        }
    } else {
        printf("[OK] File is empty\n");
    }

    for (int i = 0; i < 100; i++) {
        data[i] = i * 2;
    }
    printf("[OK] Data filled: data[0]=%d, data[99]=%d\n",
           data[0], data[99]);

    ret = 0;  /* success */

cleanup:
    printf("--- cleanup ---\n");

    /* free(NULL) is safe (no-op) */
    free(data);
    printf("  free(data)   [was %s]\n", data ? "allocated" : "NULL");

    free(buffer);
    printf("  free(buffer) [was %s]\n", buffer ? "allocated" : "NULL");

    /* fclose(NULL) is UB — must check */
    if (f != NULL) {
        fclose(f);
        printf("  fclose(f)    [was open]\n");
    } else {
        printf("  skip fclose  [was NULL]\n");
    }

    return ret;
}

int main(void) {
    /* Create a temporary test file */
    FILE *tmp = fopen("/tmp/goto_lab_test.txt", "w");
    if (tmp != NULL) {
        fprintf(tmp, "Hello from goto lab\n");
        fclose(tmp);
    }

    printf("=== Test 1: valid file ===\n");
    int r1 = process_file("/tmp/goto_lab_test.txt");
    printf("Return: %d (%s)\n\n", r1, r1 == 0 ? "success" : "failure");

    printf("=== Test 2: non-existent file ===\n");
    int r2 = process_file("/tmp/nonexistent_xyz.txt");
    printf("Return: %d (%s)\n", r2, r2 == 0 ? "success" : "failure");

    /* Clean up temp file */
    remove("/tmp/goto_lab_test.txt");
    return 0;
}
