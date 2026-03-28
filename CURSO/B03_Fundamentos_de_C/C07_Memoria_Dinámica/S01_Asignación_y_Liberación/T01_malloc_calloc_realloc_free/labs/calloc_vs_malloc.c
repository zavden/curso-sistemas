// calloc_vs_malloc.c — compare calloc (zero-initialized) vs malloc (garbage)
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int n = 5;

    // malloc: memory is NOT initialized
    printf("=== malloc (uninitialized) ===\n");
    int *m = malloc(n * sizeof(*m));
    if (m == NULL) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        printf("m[%d] = %d\n", i, m[i]);
    }
    free(m);
    m = NULL;

    // calloc: memory IS initialized to zero
    printf("\n=== calloc (zero-initialized) ===\n");
    int *c = calloc(n, sizeof(*c));
    if (c == NULL) {
        perror("calloc");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        printf("c[%d] = %d\n", i, c[i]);
    }
    free(c);
    c = NULL;

    return 0;
}
