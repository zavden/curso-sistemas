#include <stdio.h>

int main(void) {
    int x = 0;

    // BUG: = (assignment) instead of == (comparison)
    // This ASSIGNS 5 to x, then evaluates 5 as true.
    // The condition is ALWAYS true.
    printf("=== Bug: = instead of == ===\n");
    printf("x before: %d\n", x);

    if (x = 5) {
        printf("x is 5? x = %d (branch taken)\n", x);
    } else {
        printf("x is not 5\n");
    }
    printf("x after: %d (x was modified!)\n", x);

    // CORRECT: == (comparison)
    printf("\n=== Correct: == ===\n");
    x = 0;
    printf("x before: %d\n", x);

    if (x == 5) {
        printf("x is 5\n");
    } else {
        printf("x is not 5 (branch taken)\n");
    }
    printf("x after: %d (x unchanged)\n", x);

    return 0;
}
