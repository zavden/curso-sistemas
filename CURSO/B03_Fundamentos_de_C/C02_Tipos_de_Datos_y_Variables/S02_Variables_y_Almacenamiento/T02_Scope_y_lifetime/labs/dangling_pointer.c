#include <stdio.h>
#include <stdlib.h>

int *bad_return(void) {
    int local = 42;
    int *ptr = &local;
    return ptr;       /* WARNING: returning address of local variable */
}

int *good_return_malloc(void) {
    int *p = malloc(sizeof(int));
    if (p != NULL) {
        *p = 42;
    }
    return p;         /* OK: heap memory survives the function */
}

int *good_return_static(void) {
    static int persistent = 42;
    return &persistent;  /* OK: static lifetime survives the function */
}

int main(void) {
    printf("--- good_return_malloc ---\n");
    int *good1 = good_return_malloc();
    if (good1 != NULL) {
        printf("Value: %d (safe, heap allocated)\n", *good1);
        free(good1);
    }

    printf("\n--- good_return_static ---\n");
    int *good2 = good_return_static();
    printf("Value: %d (safe, static lifetime)\n", *good2);

    printf("\n--- bad_return (dangling pointer) ---\n");
    int *bad = bad_return();
    /* Dereferencing bad is UB -- it may print 42, garbage, or crash */
    printf("Pointer bad = %p\n", (void *)bad);
    printf("Attempting to read *bad is undefined behavior.\n");
    printf("The compiler warned us. Do NOT do this in real code.\n");

    return 0;
}
