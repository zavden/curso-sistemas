#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void use_after_free_example(void) {
    char *name = malloc(32);
    if (!name) return;

    strcpy(name, "Valgrind test");
    printf("Before free: %s\n", name);

    free(name);

    /* Use after free: reading memory that was already freed */
    printf("After free: %s\n", name);
}

void double_free_example(void) {
    int *data = malloc(10 * sizeof(int));
    if (!data) return;

    for (int i = 0; i < 10; i++) {
        data[i] = i;
    }

    free(data);

    /* Double free: freeing the same pointer twice */
    free(data);
}

int main(void) {
    use_after_free_example();
    double_free_example();
    return 0;
}
