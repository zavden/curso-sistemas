/*
 * ub_showcase.c — Four classic undefined behaviors, each in its own function.
 *
 * Compile without sanitizers:  gcc ub_showcase.c -o ub_showcase
 * Compile with UBSan:          gcc -fsanitize=undefined -g ub_showcase.c -o ub_showcase
 *
 * Each function triggers a different kind of UB so sanitizers
 * can report them individually.
 */

#include <limits.h>
#include <stdio.h>

/* UB 1: Signed integer overflow (INT_MAX + 1) */
int signed_overflow(void) {
    int x = INT_MAX;
    int y = x + 1;     /* UB: signed overflow */
    return y;
}

/* UB 2: Out-of-bounds array access */
int out_of_bounds(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int val = arr[7];  /* UB: index 7 on a size-5 array */
    return val;
}

/* UB 3: Use of uninitialized variable */
int uninitialized_use(void) {
    int x;              /* not initialized */
    if (x > 0) {       /* UB: reading indeterminate value */
        return 1;
    }
    return 0;
}

/* UB 4: Shift by >= width of the type (32-bit int) */
int bad_shift(void) {
    int x = 1;
    int y = x << 32;   /* UB: shift count >= width of int */
    return y;
}

int main(void) {
    printf("=== UB Showcase ===\n\n");

    printf("1. Signed overflow  (INT_MAX + 1): %d\n", signed_overflow());
    printf("2. Out-of-bounds    (arr[7]):      %d\n", out_of_bounds());
    printf("3. Uninitialized    (int x; x>0):  %d\n", uninitialized_use());
    printf("4. Bad shift        (1 << 32):     %d\n", bad_shift());

    printf("\nAll functions returned without crashing.\n");
    printf("That does NOT mean the code is correct.\n");

    return 0;
}
