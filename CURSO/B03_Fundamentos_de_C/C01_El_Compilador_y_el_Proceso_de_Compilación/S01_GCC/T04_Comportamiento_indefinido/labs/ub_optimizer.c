/*
 * ub_optimizer.c — How the optimizer EXPLOITS undefined behavior.
 *
 * Compile twice:
 *   gcc -O0 ub_optimizer.c -o ub_opt_O0
 *   gcc -O2 ub_optimizer.c -o ub_opt_O2
 *
 * Run both and compare the output.  The results differ because
 * the optimizer uses UB as license to remove code.
 */

#include <limits.h>
#include <stdio.h>

/*
 * Example 1: Null-check-after-dereference.
 *
 * The function dereferences p first, THEN checks if p is NULL.
 * With -O0: the null check works (the dereference "succeeds" by luck).
 * With -O2: the compiler reasons:
 *   "p was already dereferenced, so p cannot be NULL (that would be UB).
 *    Therefore the NULL check is dead code — remove it."
 */
int null_check_after_deref(int *p) {
    int x = *p;            /* dereference first */
    if (p == NULL) {       /* then check — too late */
        printf("  p is NULL, returning -1\n");
        return -1;
    }
    return x + 1;
}

/*
 * Example 2: Signed overflow check optimized away.
 *
 * For signed int, x + 1 > x is ALWAYS true (the compiler assumes
 * no overflow, because overflow is UB).
 *
 * With -O0: INT_MAX + 1 wraps to a negative number on most platforms,
 *           so (INT_MAX + 1 > INT_MAX) evaluates to false → returns 0.
 * With -O2: the compiler reduces (x + 1 > x) to true → returns 1.
 */
int overflow_check(int x) {
    if (x + 1 > x) {
        return 1;          /* "x + 1 is greater than x" */
    }
    return 0;              /* "overflow detected" */
}

int main(void) {
    printf("=== Optimizer exploiting UB ===\n\n");

    /* Example 1: pass a valid pointer so it doesn't segfault */
    int value = 42;
    printf("1. null_check_after_deref(&value): %d\n",
           null_check_after_deref(&value));
    printf("   (The NULL check is silently removed at -O2)\n\n");

    /* Example 2: pass INT_MAX to trigger the difference */
    printf("2. overflow_check(INT_MAX): %d\n",
           overflow_check(INT_MAX));
    printf("   Expected with -O0: 0  (wrapping makes x+1 negative)\n");
    printf("   Expected with -O2: 1  (compiler assumes no overflow)\n");

    return 0;
}
