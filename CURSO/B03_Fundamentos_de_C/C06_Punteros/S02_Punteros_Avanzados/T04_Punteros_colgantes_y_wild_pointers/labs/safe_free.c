#include <stdio.h>
#include <stdlib.h>

#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

void use_pointer(const int *p) {
    if (p == NULL) {
        printf("  Pointer is NULL -- safe to detect, no silent corruption.\n");
        return;
    }
    printf("  *p = %d\n", *p);
}

int main(void) {
    /* --- Without SAFE_FREE: dangling pointer remains --- */
    printf("=== Without SAFE_FREE ===\n");

    int *a = malloc(sizeof(int));
    if (!a) {
        perror("malloc");
        return 1;
    }
    *a = 42;
    printf("Before free:\n");
    use_pointer(a);

    free(a);
    /* a still points to the old address -- dangling */
    printf("After free (dangling, UB if dereferenced):\n");
    printf("  a = %p  (non-NULL, but invalid)\n", (void *)a);

    /* --- With SAFE_FREE: pointer is nullified --- */
    printf("\n=== With SAFE_FREE ===\n");

    int *b = malloc(sizeof(int));
    if (!b) {
        perror("malloc");
        return 1;
    }
    *b = 99;
    printf("Before SAFE_FREE:\n");
    use_pointer(b);

    SAFE_FREE(b);
    printf("After SAFE_FREE:\n");
    printf("  b = %p  (NULL -- safe)\n", (void *)b);
    use_pointer(b);

    /* SAFE_FREE is idempotent -- free(NULL) is a no-op */
    printf("\nSecond SAFE_FREE (idempotent):\n");
    SAFE_FREE(b);
    printf("  b = %p  (still NULL, no crash)\n", (void *)b);

    return 0;
}
