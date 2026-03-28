/* simple_leaks.c -- Three common types of memory leaks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Leak 1: simple leak -- allocate and forget to free */
void simple_leak(void) {
    int *data = malloc(10 * sizeof(int));
    if (!data) return;
    for (int i = 0; i < 10; i++) {
        data[i] = i * i;
    }
    printf("simple_leak: computed %d squares\n", 10);
    /* missing free(data) */
}

/* Leak 2: reassignment leak -- overwrite the only pointer */
void reassignment_leak(void) {
    char *msg = malloc(32);
    if (!msg) return;
    strcpy(msg, "first message");
    printf("reassignment_leak: msg = \"%s\"\n", msg);

    msg = malloc(64);          /* LEAK: first 32 bytes lost */
    if (!msg) return;
    strcpy(msg, "second message");
    printf("reassignment_leak: msg = \"%s\"\n", msg);
    free(msg);                 /* only frees the second allocation */
}

/* Leak 3: error path leak -- early return without free */
int error_path_leak(int trigger_error) {
    char *buffer = malloc(256);
    if (!buffer) return -1;
    strcpy(buffer, "processing data...");
    printf("error_path_leak: %s\n", buffer);

    if (trigger_error) {
        printf("error_path_leak: error detected, returning early\n");
        return -1;             /* LEAK: buffer not freed */
    }

    free(buffer);
    return 0;
}

int main(void) {
    printf("--- Leak 1: simple leak ---\n");
    simple_leak();

    printf("\n--- Leak 2: reassignment leak ---\n");
    reassignment_leak();

    printf("\n--- Leak 3: error path leak ---\n");
    error_path_leak(1);

    printf("\nProgram finished.\n");
    return 0;
}
