#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    if (!p) {
        perror("malloc");
        return 1;
    }
    *p = 42;
    printf("Before free: *p = %d, p = %p\n", *p, (void *)p);

    free(p);
    /* p still holds the old address, but the memory is no longer ours.
     * Any access through p from here is UB (use-after-free). */

    printf("After free:  p = %p  (address unchanged, but memory invalid)\n",
           (void *)p);

    /* UB -- use-after-free read */
    printf("After free:  *p = %d  (UB -- may print 42, garbage, or crash)\n",
           *p);

    /* UB -- use-after-free write */
    *p = 100;
    printf("After write: *p = %d  (UB -- wrote to freed memory)\n", *p);

    return 0;
}
