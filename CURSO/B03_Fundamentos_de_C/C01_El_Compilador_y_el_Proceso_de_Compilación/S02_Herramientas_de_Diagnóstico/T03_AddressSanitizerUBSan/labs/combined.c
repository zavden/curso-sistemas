/*
 * combined.c -- Both memory errors AND undefined behavior in one program.
 *
 * Demonstrates using -fsanitize=address,undefined together.
 * UBSan errors are reported first (they happen earlier in the code).
 * ASan aborts on the first memory error it hits.
 *
 * Compile: gcc -fsanitize=address,undefined -g -O0 combined.c -o combined
 * Run:     ./combined
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* --- UB: signed integer overflow (UBSan detects) --- */
    int x = INT_MAX;
    int y = x + 1;
    printf("INT_MAX + 1 = %d (UBSan should warn above)\n", y);

    /* --- Memory: heap buffer overflow (ASan detects) --- */
    int *arr = malloc(4 * sizeof(int));
    if (!arr) return 1;

    arr[0] = 100;
    arr[1] = 200;
    arr[2] = 300;
    arr[3] = 400;
    arr[4] = 500;     /* 1 position past the end of a 4-element array */

    printf("arr[4] = %d (ASan should abort above)\n", arr[4]);

    free(arr);
    return 0;
}
