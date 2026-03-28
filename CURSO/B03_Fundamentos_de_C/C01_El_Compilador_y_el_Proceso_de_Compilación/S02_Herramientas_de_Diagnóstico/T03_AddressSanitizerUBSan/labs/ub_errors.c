/*
 * ub_errors.c -- Multiple kinds of undefined behavior for UBSan.
 *
 * Each UB type is isolated in its own function so they can be
 * tested independently.  By default, UBSan only prints a warning
 * and continues execution.  Use -fno-sanitize-recover=all to
 * make it abort on the first UB.
 *
 * Compile: gcc -fsanitize=undefined -g ub_errors.c -o ub_errors
 * Run:     ./ub_errors 1   (signed overflow)
 *          ./ub_errors 2   (division by zero)
 *          ./ub_errors 3   (invalid shift)
 *          ./ub_errors 4   (null dereference)
 *          ./ub_errors 0   (all of them, sequentially)
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* UB 1: signed integer overflow. */
void signed_overflow(void) {
    int x = INT_MAX;
    printf("INT_MAX     = %d\n", x);
    int y = x + 1;                    /* UB: signed overflow */
    printf("INT_MAX + 1 = %d\n", y);
}

/* UB 2: integer division by zero. */
void division_by_zero(void) {
    int numerator   = 42;
    int denominator = 0;
    printf("42 / 0 = %d\n", numerator / denominator);   /* UB */
}

/* UB 3: shift exponent out of range. */
void invalid_shift(void) {
    int a = 1;

    int b = a << 32;   /* shift >= width of type (32-bit int) */
    printf("1 << 32 = %d\n", b);

    int c = a << -1;   /* negative shift */
    printf("1 << -1 = %d\n", c);
}

/* UB 4: dereference a null pointer. */
void null_dereference(void) {
    int *p = NULL;
    printf("*NULL = %d\n", *p);       /* UB: null deref */
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <0|1|2|3|4>\n", argv[0]);
        fprintf(stderr, "  0 = all (run sequentially)\n");
        fprintf(stderr, "  1 = signed overflow\n");
        fprintf(stderr, "  2 = division by zero\n");
        fprintf(stderr, "  3 = invalid shift\n");
        fprintf(stderr, "  4 = null dereference\n");
        return 1;
    }

    int choice = atoi(argv[1]);

    switch (choice) {
        case 1: signed_overflow();   break;
        case 2: division_by_zero();  break;
        case 3: invalid_shift();     break;
        case 4: null_dereference();  break;
        case 0:
            signed_overflow();
            division_by_zero();
            invalid_shift();
            null_dereference();
            break;
        default:
            fprintf(stderr, "Invalid option: %s\n", argv[1]);
            return 1;
    }

    return 0;
}
