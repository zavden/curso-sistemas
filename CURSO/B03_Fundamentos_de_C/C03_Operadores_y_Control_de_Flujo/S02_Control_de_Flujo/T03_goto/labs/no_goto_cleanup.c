#include <stdio.h>
#include <stdlib.h>

/*
 * Same logic as goto_cleanup.c but WITHOUT goto.
 * Notice the duplicated cleanup code at each error point.
 */
int process_file(const char *path) {
    /* Step 1: open file */
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("[FAIL] Cannot open '%s'\n", path);
        /* cleanup: nothing to clean */
        return -1;
    }
    printf("[OK] File opened: %s\n", path);

    /* Step 2: allocate buffer */
    char *buffer = malloc(1024);
    if (buffer == NULL) {
        printf("[FAIL] malloc buffer failed\n");
        fclose(f);                  /* cleanup: close f */
        return -1;
    }
    printf("[OK] Buffer allocated (1024 bytes)\n");

    /* Step 3: allocate data array */
    int *data = malloc(100 * sizeof(int));
    if (data == NULL) {
        printf("[FAIL] malloc data failed\n");
        free(buffer);               /* cleanup: free buffer */
        fclose(f);                  /* cleanup: AND close f */
        return -1;
    }
    printf("[OK] Data allocated (%zu bytes)\n", 100 * sizeof(int));

    /* Step 4: use the resources */
    if (fgets(buffer, 1024, f) != NULL) {
        printf("[OK] Read first line: %s", buffer);
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

    /* Cleanup on success — same 3 lines DUPLICATED again */
    printf("--- cleanup ---\n");
    free(data);
    printf("  free(data)\n");
    free(buffer);
    printf("  free(buffer)\n");
    fclose(f);
    printf("  fclose(f)\n");
    return 0;
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
