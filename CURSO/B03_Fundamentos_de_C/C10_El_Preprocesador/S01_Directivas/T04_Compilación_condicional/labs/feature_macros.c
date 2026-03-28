/*
 * feature_macros.c -- Feature test macros and platform detection
 *
 * Demonstrates:
 *   - _POSIX_C_SOURCE, _GNU_SOURCE
 *   - __linux__, __GNUC__, __STDC_VERSION__
 *   - How feature macros unlock additional functions
 */

/* Define BEFORE any #include to unlock POSIX/GNU extensions */
#ifdef USE_GNU
#define _GNU_SOURCE           /* unlocks GNU extensions (includes POSIX) */
#elif defined(USE_POSIX)
#define _POSIX_C_SOURCE 200809L  /* unlocks POSIX.1-2008 functions */
#endif

#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Feature test macros ===\n\n");

    /* --- Section 1: What is defined? --- */
    printf("Macros currently defined:\n");

#ifdef _GNU_SOURCE
    printf("  _GNU_SOURCE       = (defined)\n");
#else
    printf("  _GNU_SOURCE       = (not defined)\n");
#endif

#ifdef _POSIX_C_SOURCE
    printf("  _POSIX_C_SOURCE   = %ldL\n", (long)_POSIX_C_SOURCE);
#else
    printf("  _POSIX_C_SOURCE   = (not defined)\n");
#endif

    /* --- Section 2: Platform and compiler detection --- */
    printf("\n=== Platform / compiler info ===\n\n");

#ifdef __linux__
    printf("  Platform: Linux\n");
#else
    printf("  Platform: not Linux\n");
#endif

#if defined(__GNUC__) && !defined(__clang__)
    printf("  Compiler: GCC %d.%d.%d\n",
           __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
    printf("  Compiler: Clang %d.%d.%d\n",
           __clang_major__, __clang_minor__, __clang_patchlevel__);
#else
    printf("  Compiler: unknown\n");
#endif

#ifdef __STDC_VERSION__
  #if __STDC_VERSION__ >= 201710L
    printf("  C standard: C17 (__STDC_VERSION__ = %ldL)\n",
           (long)__STDC_VERSION__);
  #elif __STDC_VERSION__ >= 201112L
    printf("  C standard: C11 (__STDC_VERSION__ = %ldL)\n",
           (long)__STDC_VERSION__);
  #else
    printf("  C standard: pre-C11 (__STDC_VERSION__ = %ldL)\n",
           (long)__STDC_VERSION__);
  #endif
#else
    printf("  C standard: C89/C90 (__STDC_VERSION__ not defined)\n");
#endif

    /* --- Section 3: Feature macros unlock functions --- */
    printf("\n=== Feature-gated functions ===\n\n");

    const char *haystack = "Hello, World!";

#ifdef _GNU_SOURCE
    /* strcasestr() is a GNU extension -- case-insensitive strstr */
    const char *found = strcasestr(haystack, "world");
    if (found) {
        printf("  strcasestr(\"%s\", \"world\") found at offset %ld\n",
               haystack, found - haystack);
    }
    printf("  -> strcasestr() available (GNU extension)\n");
#else
    printf("  strcasestr() NOT available\n");
    printf("  -> Compile with -DUSE_GNU to enable _GNU_SOURCE\n");
    (void)haystack;  /* suppress unused warning */
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
    /* strnlen() requires POSIX.1-2008 or _GNU_SOURCE */
    size_t len = strnlen(haystack, 100);
    printf("  strnlen(\"%s\", 100) = %zu\n", haystack, len);
    printf("  -> strnlen() available (POSIX.1-2008)\n");
#elif !defined(_GNU_SOURCE)
    printf("  strnlen() NOT available (needs _POSIX_C_SOURCE >= 200809L)\n");
#endif

    return 0;
}
