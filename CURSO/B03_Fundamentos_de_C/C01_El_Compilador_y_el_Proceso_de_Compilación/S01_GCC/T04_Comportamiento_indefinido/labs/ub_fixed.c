/*
 * ub_fixed.c — Corrected version of ub_showcase.c.
 *
 * Every UB has been replaced with a defined, safe pattern.
 * Compile with full checks to verify zero errors:
 *   gcc -Wall -Wextra -fsanitize=address,undefined -g ub_fixed.c -o ub_fixed
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Fix 1: Use unsigned for wrapping arithmetic */
unsigned int safe_wrapping(void) {
    unsigned int x = UINT_MAX;
    unsigned int y = x + 1;   /* defined: wraps to 0 */
    return y;
}

/* Fix 2: Bounds-check before accessing the array */
int safe_access(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int index = 7;
    int size = 5;

    if (index >= 0 && index < size) {
        return arr[index];
    }
    printf("  Index %d out of bounds [0, %d)\n", index, size);
    return -1;              /* safe fallback */
}

/* Fix 3: Always initialize variables */
int safe_initialized(void) {
    int x = 0;              /* initialized to a known value */
    if (x > 0) {
        return 1;
    }
    return 0;
}

/* Fix 4: Validate shift amount before shifting */
int safe_shift(void) {
    int x = 1;
    int shift = 32;
    int width = (int)(sizeof(int) * 8);  /* typically 32 */

    if (shift >= 0 && shift < width) {
        return x << shift;
    }
    printf("  Shift %d exceeds int width (%d bits)\n", shift, width);
    return 0;               /* safe fallback */
}

int main(void) {
    printf("=== UB Fixed ===\n\n");

    printf("1. Safe wrapping   (UINT_MAX + 1): %u\n", safe_wrapping());
    printf("2. Safe access     (index 7):      %d\n", safe_access());
    printf("3. Safe init       (x = 0; x>0):   %d\n", safe_initialized());
    printf("4. Safe shift      (shift 32):     %d\n", safe_shift());

    printf("\nAll functions use defined behavior.\n");
    printf("Sanitizers report zero errors.\n");

    return 0;
}
