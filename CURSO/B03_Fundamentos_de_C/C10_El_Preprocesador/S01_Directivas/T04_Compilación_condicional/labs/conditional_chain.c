/*
 * conditional_chain.c -- #if, #elif, #else, #endif chains
 *
 * Demonstrates:
 *   - #if / #elif / #else chains for multi-way selection
 *   - Expressions in #if (arithmetic, comparisons, logical)
 *   - Combining defined() with logical operators
 *   - Undefined macros silently become 0 in #if
 */

#include <stdio.h>

int main(void) {
    printf("=== #if / #elif / #else chains ===\n\n");

    /* --- Section 1: LOG_LEVEL selection --- */
#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

    printf("LOG_LEVEL = %d\n", LOG_LEVEL);

#if LOG_LEVEL == 3
    printf("  -> Logging: ERROR + WARN + INFO\n");
#elif LOG_LEVEL == 2
    printf("  -> Logging: ERROR + WARN\n");
#elif LOG_LEVEL == 1
    printf("  -> Logging: ERROR only\n");
#else
    printf("  -> Logging: OFF\n");
#endif

    /* --- Section 2: Combining defined() with && and || --- */
    printf("\n=== Combining defined() ===\n\n");

#if defined(OPT_A) && defined(OPT_B)
    printf("Both OPT_A and OPT_B are defined\n");
#elif defined(OPT_A)
    printf("Only OPT_A is defined\n");
#elif defined(OPT_B)
    printf("Only OPT_B is defined\n");
#else
    printf("Neither OPT_A nor OPT_B is defined\n");
#endif

    /* --- Section 3: Arithmetic expressions in #if --- */
    printf("\n=== Arithmetic in #if ===\n\n");

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

    printf("BUFSIZE = %d\n", BUFSIZE);

#if BUFSIZE < 64
    printf("  -> WARNING: buffer too small\n");
#elif BUFSIZE <= 512
    printf("  -> Buffer size is reasonable\n");
#elif BUFSIZE <= 4096
    printf("  -> Buffer is large\n");
#else
    printf("  -> Buffer is very large (> 4096)\n");
#endif

    /* --- Section 4: Undefined macro in #if becomes 0 --- */
    printf("\n=== Undefined macros in #if ===\n\n");

    /*
     * MYSTERY_MACRO is intentionally NOT defined anywhere.
     * In #if, an undefined macro silently evaluates to 0.
     * This is a common source of bugs -- no warning by default.
     * Use gcc -Wundef to catch this.
     */
#if MYSTERY_MACRO
    printf("MYSTERY_MACRO is true (nonzero)\n");
#else
    printf("MYSTERY_MACRO is false (0 or undefined)\n");
    printf("  -> Was it intentionally 0, or just not defined? No way to tell.\n");
#endif

    /* Better: be explicit */
#if defined(MYSTERY_MACRO) && MYSTERY_MACRO
    printf("MYSTERY_MACRO is defined AND nonzero\n");
#elif defined(MYSTERY_MACRO)
    printf("MYSTERY_MACRO is defined but zero\n");
#else
    printf("MYSTERY_MACRO is not defined at all\n");
#endif

    return 0;
}
