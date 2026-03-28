#include <stdio.h>
#include <stdlib.h>

void alloc_wrong(int *p) {
    p = malloc(sizeof(int));   /* modifica la copia local */
    if (p) *p = 42;
    /* memory leak: el caller no ve el cambio, y no puede hacer free */
}

void alloc_correct(int **pp) {
    *pp = malloc(sizeof(int)); /* modifica el puntero del caller */
    if (*pp) **pp = 42;
}

int main(void) {
    int *data = NULL;

    printf("--- alloc_wrong ---\n");
    printf("data antes: %p\n", (void *)data);
    alloc_wrong(data);
    printf("data despues: %p\n", (void *)data);
    printf("data sigue siendo NULL: %s\n", data == NULL ? "si" : "no");

    printf("\n--- alloc_correct ---\n");
    printf("data antes: %p\n", (void *)data);
    alloc_correct(&data);
    printf("data despues: %p\n", (void *)data);
    printf("*data = %d\n", *data);

    free(data);
    return 0;
}
