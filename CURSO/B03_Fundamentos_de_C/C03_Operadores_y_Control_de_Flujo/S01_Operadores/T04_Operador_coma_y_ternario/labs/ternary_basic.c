#include <stdio.h>

int main(void) {
    int a = 7, b = 3;

    /* Basic ternary: select the larger value */
    int max = (a > b) ? a : b;
    printf("a=%d, b=%d, max=%d\n", a, b, max);

    /* Ternary as argument to printf */
    int x = -5;
    printf("x is %s\n", (x > 0) ? "positive" : "non-positive");

    /* Absolute value with ternary */
    int abs_val = (x >= 0) ? x : -x;
    printf("x=%d, abs(x)=%d\n", x, abs_val);

    /* Plural formatting */
    for (int n = 0; n <= 3; n++) {
        printf("Found %d item%s\n", n, (n == 1) ? "" : "s");
    }

    /* Initializing a const with ternary */
    int debug = 1;
    const char *mode = (debug) ? "DEBUG" : "RELEASE";
    printf("mode = %s\n", mode);

    return 0;
}
