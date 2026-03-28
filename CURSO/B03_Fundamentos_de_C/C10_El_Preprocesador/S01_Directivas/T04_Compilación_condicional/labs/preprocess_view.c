/*
 * preprocess_view.c -- Small file for examining gcc -E output
 *
 * Demonstrates:
 *   - Using gcc -E to see preprocessor output
 *   - How conditional blocks are included or excluded
 *   - The # line markers in preprocessor output
 */

#include <stdio.h>

#ifdef FEATURE_GREET
static void greet(void) {
    printf("Hello! Greeting feature is active.\n");
}
#endif

#ifdef FEATURE_FAREWELL
static void farewell(void) {
    printf("Goodbye! Farewell feature is active.\n");
}
#endif

int main(void) {
#ifdef FEATURE_GREET
    greet();
#else
    printf("Greeting disabled.\n");
#endif

#ifdef FEATURE_FAREWELL
    farewell();
#else
    printf("Farewell disabled.\n");
#endif

    return 0;
}
