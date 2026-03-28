#include <stdio.h>
#include <string.h>

int main(void) {
    /* Array: copy of the literal on the stack */
    char arr[] = "hello";

    /* Pointer: points to read-only literal in .rodata */
    const char *ptr = "hello";

    printf("=== sizeof difference ===\n");
    printf("sizeof(arr) = %zu  (array of %zu chars)\n",
           sizeof(arr), sizeof(arr));
    printf("sizeof(ptr) = %zu  (size of a pointer)\n", sizeof(ptr));

    printf("\n=== strlen is the same ===\n");
    printf("strlen(arr) = %zu\n", strlen(arr));
    printf("strlen(ptr) = %zu\n", strlen(ptr));

    printf("\n=== Modifying the array ===\n");
    arr[0] = 'H';
    printf("arr after arr[0] = 'H': \"%s\"\n", arr);

    printf("\n=== Reassigning the pointer ===\n");
    printf("ptr before: \"%s\"\n", ptr);
    ptr = "world";
    printf("ptr after ptr = \"world\": \"%s\"\n", ptr);

    /* arr = "world";  -- ERROR: array is not assignable */

    return 0;
}
