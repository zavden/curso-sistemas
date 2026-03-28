#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This program demonstrates ownership errors.
// Compile and run with Valgrind to see the diagnostics.
//
// Usage:
//   ./ownership_errors 1    -> double free
//   ./ownership_errors 2    -> use after free
//   ./ownership_errors 3    -> memory leak
//   ./ownership_errors 4    -> use after transfer (simulated)

void error_double_free(void) {
    printf("=== Error: double free ===\n");
    char *s = malloc(strlen("hello") + 1);
    if (!s) return;
    strcpy(s, "hello");

    printf("s = \"%s\"\n", s);
    free(s);
    printf("First free done.\n");

    // BUG: freeing the same pointer again
    free(s);
    printf("Second free done (undefined behavior occurred!).\n");
}

void error_use_after_free(void) {
    printf("=== Error: use after free ===\n");
    char *s = malloc(strlen("hello") + 1);
    if (!s) return;
    strcpy(s, "hello");

    printf("Before free: s = \"%s\"\n", s);
    free(s);

    // BUG: reading memory after free
    printf("After free: s = \"%s\"\n", s);
    printf("The above line is undefined behavior.\n");
}

void error_memory_leak(void) {
    printf("=== Error: memory leak ===\n");

    char *s = malloc(strlen("first allocation") + 1);
    if (!s) return;
    strcpy(s, "first allocation");
    printf("s = \"%s\"\n", s);

    // BUG: overwriting s without freeing the old allocation
    s = malloc(strlen("second allocation") + 1);
    if (!s) return;
    strcpy(s, "second allocation");
    printf("s = \"%s\" (old allocation leaked!)\n", s);

    free(s);  // only frees the second allocation
    printf("Only freed the second allocation. First is leaked.\n");
}

void error_use_after_transfer(void) {
    printf("=== Error: use after transfer ===\n");

    // Simulate ownership transfer
    char *data = malloc(strlen("important data") + 1);
    if (!data) return;
    strcpy(data, "important data");

    printf("Before transfer: data = \"%s\"\n", data);

    // Simulate transferring ownership (e.g., to a container)
    char *transferred = data;  // container now "owns" data
    // In real code, this would be something like: container_add(c, data);

    // The container frees it
    free(transferred);

    // BUG: original pointer is now dangling
    // data still points to freed memory
    printf("After transfer+free: data = \"%s\"\n", data);
    printf("The above line is undefined behavior (dangling pointer).\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <error_number>\n", argv[0]);
        printf("  1 = double free\n");
        printf("  2 = use after free\n");
        printf("  3 = memory leak\n");
        printf("  4 = use after transfer\n");
        return 1;
    }

    int choice = atoi(argv[1]);
    switch (choice) {
        case 1: error_double_free(); break;
        case 2: error_use_after_free(); break;
        case 3: error_memory_leak(); break;
        case 4: error_use_after_transfer(); break;
        default:
            printf("Invalid choice: %d (use 1-4)\n", choice);
            return 1;
    }

    return 0;
}
