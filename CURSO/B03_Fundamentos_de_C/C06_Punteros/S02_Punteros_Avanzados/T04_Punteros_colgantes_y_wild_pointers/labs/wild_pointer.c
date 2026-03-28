#include <stdio.h>

int main(void) {
    int *p;  /* wild pointer -- uninitialized, holds garbage value */

    /* Reading the pointer value itself is already UB in C11,
     * but printing the address typically does not crash.
     * Dereferencing it would write to a random address. */
    printf("Wild pointer value: p = %p  (garbage address)\n", (void *)p);

    /* UB -- dereference of wild pointer */
    /* Uncomment the next line to see the crash:
     * *p = 42;
     */

    /* PREVENTION: always initialize pointers */
    int *safe = NULL;
    printf("Initialized to NULL: safe = %p\n", (void *)safe);

    /* Dereferencing NULL gives a guaranteed segfault (detectable),
     * unlike a wild pointer which may corrupt memory silently. */

    return 0;
}
