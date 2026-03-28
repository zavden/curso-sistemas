#include <stdio.h>
#include <stddef.h>

int main(void) {
    printf("sizeof(int *):    %zu bytes\n", sizeof(int *));
    printf("sizeof(double *): %zu bytes\n", sizeof(double *));
    printf("sizeof(char *):   %zu bytes\n", sizeof(char *));
    printf("sizeof(void *):   %zu bytes\n", sizeof(void *));

    printf("\nsizeof(int):      %zu bytes\n", sizeof(int));
    printf("sizeof(double):   %zu bytes\n", sizeof(double));
    printf("sizeof(char):     %zu bytes\n", sizeof(char));

    printf("\n--- NULL demo ---\n");
    int *p = NULL;

    if (p == NULL) {
        printf("p is NULL — does not point to anything\n");
    }

    if (!p) {
        printf("!p is true — NULL is falsy\n");
    }

    int x = 42;
    p = &x;

    if (p != NULL) {
        printf("p is not NULL, *p = %d\n", *p);
    }

    return 0;
}
