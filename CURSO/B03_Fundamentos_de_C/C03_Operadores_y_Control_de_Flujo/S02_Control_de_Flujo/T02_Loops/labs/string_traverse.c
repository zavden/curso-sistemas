#include <stdio.h>
#include <ctype.h>

int main(void) {
    const char *msg = "Hello, World! 123";

    /* Traverse with index */
    printf("--- Index traversal ---\n");
    printf("String: \"%s\"\n", msg);
    printf("Characters: ");
    for (int i = 0; msg[i] != '\0'; i++) {
        printf("'%c' ", msg[i]);
    }
    printf("\n\n");

    /* Traverse with pointer */
    printf("--- Pointer traversal ---\n");
    printf("Characters: ");
    for (const char *p = msg; *p != '\0'; p++) {
        printf("'%c' ", *p);
    }
    printf("\n\n");

    /* Count character types */
    printf("--- Count character types ---\n");
    int letters = 0, digits = 0, spaces = 0, other = 0;
    for (const char *p = msg; *p != '\0'; p++) {
        if (isalpha((unsigned char)*p)) {
            letters++;
        } else if (isdigit((unsigned char)*p)) {
            digits++;
        } else if (*p == ' ') {
            spaces++;
        } else {
            other++;
        }
    }
    printf("Letters: %d, Digits: %d, Spaces: %d, Other: %d\n",
           letters, digits, spaces, other);

    return 0;
}
