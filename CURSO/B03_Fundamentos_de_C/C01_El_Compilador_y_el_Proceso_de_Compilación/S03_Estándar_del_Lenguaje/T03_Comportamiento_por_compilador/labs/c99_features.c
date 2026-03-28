#include <stdio.h>
#include <stdint.h>

int main(void) {
    // C99 feature 1: single-line comments (not valid in C89)

    /* C99 feature 2: mixed declarations and code */
    printf("=== C99 features ===\n");
    int x = 10;

    /* C99 feature 3: variable-length array (VLA) */
    int n = 5;
    int vla[n];
    for (int i = 0; i < n; i++) {   /* C99 feature 4: for-loop declaration */
        vla[i] = i * i;
    }

    /* C99 feature 5: designated initializers */
    int arr[5] = { [2] = 42, [4] = 99 };

    /* C99 feature 6: stdint.h types */
    int32_t fixed = 12345;
    uint8_t byte = 255;

    printf("x     = %d\n", x);
    printf("vla   = {");
    for (int j = 0; j < n; j++) {
        printf("%s%d", j ? ", " : "", vla[j]);
    }
    printf("}\n");
    printf("arr   = {%d, %d, %d, %d, %d}\n",
           arr[0], arr[1], arr[2], arr[3], arr[4]);
    printf("fixed = %d\n", fixed);
    printf("byte  = %u\n", byte);

#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);
#else
    printf("__STDC_VERSION__ not defined (pre-C99)\n");
#endif

    return 0;
}
