/*
 * ifdef_basics.c -- #ifdef, #ifndef, #if defined(), gcc -D
 *
 * Demonstrates:
 *   - #ifdef / #ifndef to test macro existence
 *   - #if defined() as equivalent form
 *   - Difference between existence (#ifdef) and value (#if)
 *   - Defining macros from the command line with gcc -D
 */

#include <stdio.h>

int main(void) {
    printf("=== #ifdef / #ifndef basics ===\n\n");

    /* --- Section 1: #ifdef DEBUG --- */
#ifdef DEBUG
    printf("[DEBUG is defined] Debug mode is ON\n");
#else
    printf("[DEBUG not defined] Debug mode is OFF\n");
#endif

    /* --- Section 2: #ifndef RELEASE --- */
#ifndef RELEASE
    printf("[RELEASE not defined] Not a release build\n");
#else
    printf("[RELEASE is defined] This is a release build\n");
#endif

    /* --- Section 3: #if defined() -- equivalent to #ifdef --- */
#if defined(VERBOSE)
    printf("[VERBOSE is defined] Verbose output enabled\n");
#else
    printf("[VERBOSE not defined] Verbose output disabled\n");
#endif

    /* --- Section 4: existence vs value --- */
    printf("\n=== Existence (#ifdef) vs Value (#if) ===\n\n");

#define FEATURE_A          /* defined, empty value */
#define FEATURE_B 0        /* defined, value 0 */
#define FEATURE_C 1        /* defined, value 1 */

    /* All three are DEFINED -- #ifdef only checks existence */
#ifdef FEATURE_A
    printf("FEATURE_A: #ifdef -> TRUE  (exists, empty value)\n");
#endif
#ifdef FEATURE_B
    printf("FEATURE_B: #ifdef -> TRUE  (exists, value 0)\n");
#endif
#ifdef FEATURE_C
    printf("FEATURE_C: #ifdef -> TRUE  (exists, value 1)\n");
#endif

    /* #if checks the VALUE -- 0 is false, nonzero is true */
#if FEATURE_B
    printf("FEATURE_B: #if    -> TRUE\n");
#else
    printf("FEATURE_B: #if    -> FALSE (value is 0)\n");
#endif

#if FEATURE_C
    printf("FEATURE_C: #if    -> TRUE  (value is 1)\n");
#endif

    /* --- Section 5: LEVEL macro from command line --- */
    printf("\n=== Macro value from -D ===\n\n");

#ifdef LEVEL
    printf("LEVEL is defined, value = %d\n", LEVEL);
  #if LEVEL >= 3
    printf("  -> High level (>= 3)\n");
  #elif LEVEL >= 1
    printf("  -> Low level (1-2)\n");
  #else
    printf("  -> Level is 0\n");
  #endif
#else
    printf("LEVEL is not defined (not passed with -DLEVEL=N)\n");
#endif

    return 0;
}
