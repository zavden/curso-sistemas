/* type_safety.c -- Macros do not check types; functions do */

#include <stdio.h>
#include <string.h>

/* Macro: accepts ANY type without complaint */
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

/* Inline functions: enforce specific types */
static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

static inline double max_double(double a, double b) {
    return a > b ? a : b;
}

int main(void) {
    printf("=== Type safety: macro vs inline function ===\n\n");

    /* --- Correct use: same types --- */
    printf("--- Correct: same types ---\n");
    printf("MAX(10, 20)       = %d\n", MAX(10, 20));
    printf("max_int(10, 20)   = %d\n", max_int(10, 20));
    printf("MAX(3.14, 2.71)   = %.2f\n", MAX(3.14, 2.71));
    printf("max_double(3.14, 2.71) = %.2f\n\n", max_double(3.14, 2.71));

    /* --- Dangerous: mixed types --- */
    printf("--- Dangerous: int vs unsigned ---\n");
    int neg = -1;
    unsigned int pos = 1;
    printf("MAX(-1, 1u)       = %d  (macro, -1 promoted to unsigned!)\n",
           MAX(neg, pos));
    printf("max_int(-1, 1)    = %d  (function, types match)\n\n",
           max_int(-1, 1));

    /* --- Dangerous: comparing unrelated types --- */
    printf("--- Dangerous: int vs pointer ---\n");
    printf("The macro MAX(42, \"hello\") would compile with only a warning.\n");
    printf("A function max_int(42, \"hello\") would NOT compile (type error).\n\n");

    /* --- Practical: strlen returns size_t (unsigned) --- */
    printf("--- Practical: signed/unsigned mismatch ---\n");
    int threshold = -5;
    size_t len = strlen("abc");
    printf("threshold = %d, strlen(\"abc\") = %zu\n", threshold, len);
    printf("MAX(threshold, (int)len) = %d  (cast to avoid mismatch)\n",
           MAX(threshold, (int)len));

    printf("\n--- Summary ---\n");
    printf("Macros: no type checking, silent promotion, hard-to-find bugs\n");
    printf("Inline functions: compiler enforces types, clear error messages\n");

    return 0;
}
