/*
 * error_warning.c -- #error and #warning directives
 *
 * Demonstrates:
 *   - #error stops compilation with a message
 *   - #warning emits a warning but continues (GCC/Clang extension, C23 standard)
 *   - Using #error to enforce required macros at compile time
 *   - Using #warning for deprecation notices
 *
 * Compile with different -D flags to trigger different paths.
 */

#include <stdio.h>

/* --- #error: require BUFFER_SIZE to be defined and >= 64 --- */
#ifndef BUFFER_SIZE
    #error "BUFFER_SIZE must be defined. Use -DBUFFER_SIZE=N"
#endif

#if BUFFER_SIZE < 64
    #error "BUFFER_SIZE must be at least 64"
#endif

/* --- #warning: deprecation notice --- */
#ifdef USE_OLD_API
    #warning "USE_OLD_API is deprecated -- migrate to the new API"
#endif

int main(void) {
    char buffer[BUFFER_SIZE];

    printf("=== #error and #warning ===\n\n");
    printf("BUFFER_SIZE = %d\n", BUFFER_SIZE);
    printf("sizeof(buffer) = %zu\n", sizeof(buffer));

#ifdef USE_OLD_API
    printf("\nUsing OLD API (deprecated)\n");
#else
    printf("\nUsing current API\n");
#endif

    return 0;
}
