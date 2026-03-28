#include <stdio.h>

int *return_local_address(void) {
    int x = 42;
    return &x;  /* WARNING: returning address of local variable */
}

void overwrite_stack(void) {
    int arr[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    (void)arr;  /* use arr to avoid unused-variable warning */
}

int main(void) {
    int *p = return_local_address();

    /* p points to the stack frame of return_local_address(),
     * which was destroyed when the function returned. */
    printf("Before another call: *p = %d  (UB -- may still show 42)\n", *p);

    /* Calling another function likely overwrites that stack region: */
    overwrite_stack();
    printf("After another call:  *p = %d  (UB -- stack was overwritten)\n", *p);

    return 0;
}
