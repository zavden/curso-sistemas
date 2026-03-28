#include <stdio.h>
#include <stddef.h>

int main(void) {
    printf("--- Valores truthy y falsy en C ---\n\n");

    /* Truthy values */
    printf("if (42)    -> %s\n", 42    ? "true" : "false");
    printf("if (-1)    -> %s\n", -1    ? "true" : "false");
    printf("if (1)     -> %s\n", 1     ? "true" : "false");

    /* Falsy values */
    printf("if (0)     -> %s\n", 0     ? "true" : "false");
    printf("if (0.0)   -> %s\n", 0.0   ? "true" : "false");

    /* Character values */
    printf("\nif ('a')   -> %s  (ASCII %d)\n", 'a'  ? "true" : "false", 'a');
    printf("if ('0')   -> %s  (ASCII %d)\n", '0'  ? "true" : "false", '0');
    printf("if ('\\0')  -> %s  (ASCII %d)\n", '\0' ? "true" : "false", '\0');

    /* Pointer */
    int x = 5;
    int *ptr = &x;
    int *null_ptr = NULL;
    printf("\nif (ptr)      -> %s  (points to %d)\n",
           ptr      ? "true" : "false", *ptr);
    printf("if (null_ptr) -> %s\n",
           null_ptr ? "true" : "false");

    return 0;
}
