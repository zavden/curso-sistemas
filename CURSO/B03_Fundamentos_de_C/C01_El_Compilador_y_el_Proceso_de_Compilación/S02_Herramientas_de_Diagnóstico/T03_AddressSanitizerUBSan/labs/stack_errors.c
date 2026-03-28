/*
 * stack_errors.c -- Stack buffer overflow (local array out of bounds).
 *
 * This is the key error that Valgrind CANNOT detect but ASan CAN.
 * Valgrind only tracks heap allocations (malloc/free).  Stack
 * variables live outside its tracking, so out-of-bounds access
 * on a local array goes unnoticed.  ASan instruments the stack
 * with red zones, so it catches it immediately.
 *
 * Compile: gcc -fsanitize=address -g stack_errors.c -o stack_errors
 * Run:     ./stack_errors
 */

#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    printf("Legitimate access:\n");
    for (int i = 0; i < 5; i++) {
        printf("  arr[%d] = %d\n", i, arr[i]);
    }

    /*
     * Out-of-bounds read on a stack-allocated array.
     * Without ASan this may silently print garbage or a "valid" value
     * from an adjacent stack variable -- no crash, no warning.
     */
    printf("\nOut-of-bounds access:\n");
    printf("  arr[8] = %d\n", arr[8]);

    return 0;
}
