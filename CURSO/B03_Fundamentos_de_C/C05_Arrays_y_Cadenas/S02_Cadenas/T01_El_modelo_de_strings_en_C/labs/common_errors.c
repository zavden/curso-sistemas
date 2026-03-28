#include <stdio.h>
#include <string.h>

int main(void) {
    /* Error 1: Forgetting the null terminator */
    printf("=== Error 1: Missing null terminator ===\n");
    char good[6] = {'h', 'e', 'l', 'l', 'o', '\0'};
    printf("With '\\0': \"%s\" (strlen = %zu)\n", good, strlen(good));
    /* The bad case is explained in the lab text -- running it would be UB */

    /* Error 2: sizeof on a pointer vs array */
    printf("\n=== Error 2: sizeof pointer vs array ===\n");
    char arr[] = "hello";
    const char *ptr = "hello";
    printf("sizeof(arr) = %zu  <-- size of the array (6 bytes)\n", sizeof(arr));
    printf("sizeof(ptr) = %zu  <-- size of the pointer (8 bytes on 64-bit)\n", sizeof(ptr));
    printf("strlen(arr) = %zu\n", strlen(arr));
    printf("strlen(ptr) = %zu\n", strlen(ptr));

    /* Error 3: Comparing strings with == */
    printf("\n=== Error 3: == compares pointers, not content ===\n");
    char a[] = "hello";
    char b[] = "hello";
    printf("&a[0] == &b[0]   : %s\n",
           (&a[0] == &b[0]) ? "true" : "false");
    printf("strcmp(a, b) == 0: %s\n", (strcmp(a, b) == 0) ? "true" : "false");

    /* Error 4: Embedded null truncates the string */
    printf("\n=== Error 4: Embedded null ===\n");
    char embedded[] = "hello\0world";
    printf("printf: \"%s\"\n", embedded);
    printf("strlen: %zu\n", strlen(embedded));
    printf("sizeof: %zu  <-- compiler knows full size (12)\n", sizeof(embedded));

    return 0;
}
