/*
 * stmt_expr.c -- GCC statement expressions and comparison with inline
 *
 * IMPORTANT: This file uses GCC extensions (typeof, statement expressions).
 * Compile with: gcc -std=gnu11 -Wall -Wextra  (NOT -std=c11 -Wpedantic)
 */

#include <stdio.h>

/* Standard macro: evaluates arguments multiple times */
#define MAX_UNSAFE(a, b) ((a) > (b) ? (a) : (b))

/* GCC statement expression: evaluates arguments once */
#define MAX_SAFE(a, b) ({   \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;     \
})

#define SQUARE_SAFE(x) ({   \
    __typeof__(x) _x = (x); \
    _x * _x;                \
})

#define CLAMP(val, lo, hi) ({        \
    __typeof__(val) _val = (val);    \
    __typeof__(lo)  _lo  = (lo);     \
    __typeof__(hi)  _hi  = (hi);     \
    _val < _lo ? _lo :               \
    _val > _hi ? _hi : _val;         \
})

/* Inline function: the standard C11 alternative */
static inline int max_inline(int a, int b) {
    return a > b ? a : b;
}

static inline int square_inline(int x) {
    return x * x;
}

static inline int clamp_inline(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

int main(void) {
    int x, y, result;

    printf("=== Statement expressions (GCC extension) ===\n\n");

    /* --- MAX_SAFE vs MAX_UNSAFE with side effects --- */
    printf("--- MAX with x++, y++ ---\n");

    x = 5; y = 3;
    result = MAX_UNSAFE(x++, y++);
    printf("MAX_UNSAFE(x++, y++): result=%d, x=%d, y=%d\n", result, x, y);

    x = 5; y = 3;
    result = MAX_SAFE(x++, y++);
    printf("MAX_SAFE(x++, y++):   result=%d, x=%d, y=%d\n", result, x, y);

    x = 5; y = 3;
    result = max_inline(x++, y++);
    printf("max_inline(x++, y++): result=%d, x=%d, y=%d\n\n", result, x, y);

    /* --- SQUARE_SAFE --- */
    printf("--- SQUARE with x++ ---\n");

    x = 4;
    result = SQUARE_SAFE(x++);
    printf("SQUARE_SAFE(x++):   result=%d, x=%d  (correct)\n", result, x);

    x = 4;
    result = square_inline(x++);
    printf("square_inline(x++): result=%d, x=%d  (correct)\n\n", result, x);

    /* --- CLAMP: practical multi-argument macro --- */
    printf("--- CLAMP(value, low, high) ---\n");
    printf("CLAMP(15, 0, 10)    = %d  (clamped to 10)\n", CLAMP(15, 0, 10));
    printf("CLAMP(-5, 0, 10)    = %d  (clamped to 0)\n", CLAMP(-5, 0, 10));
    printf("CLAMP(7, 0, 10)     = %d  (within range)\n", CLAMP(7, 0, 10));
    printf("clamp_inline(15, 0, 10) = %d\n", clamp_inline(15, 0, 10));
    printf("clamp_inline(-5, 0, 10) = %d\n", clamp_inline(-5, 0, 10));
    printf("clamp_inline(7, 0, 10)  = %d\n\n", clamp_inline(7, 0, 10));

    /* --- Type genericity: macros work with double too --- */
    printf("--- Type genericity ---\n");
    printf("MAX_SAFE(3.14, 2.71) = %.2f  (works with double)\n",
           MAX_SAFE(3.14, 2.71));
    printf("max_inline(3, 2)     = %d    (only works with int)\n\n",
           max_inline(3, 2));

    printf("--- Comparison ---\n");
    printf("Standard macro:       multiple evaluation, type-generic, portable\n");
    printf("Statement expression: single evaluation, type-generic, GCC/Clang only\n");
    printf("Inline function:      single evaluation, type-safe, portable, debuggable\n");

    return 0;
}
