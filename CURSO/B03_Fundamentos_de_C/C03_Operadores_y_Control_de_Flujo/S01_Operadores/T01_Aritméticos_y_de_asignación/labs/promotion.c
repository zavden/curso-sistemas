#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== Integer promotion: char + char -> int ===\n\n");

    unsigned char a = 200;
    unsigned char b = 100;

    printf("a = %u (unsigned char)\n", a);
    printf("b = %u (unsigned char)\n", b);
    printf("sizeof(a)     = %zu bytes\n", sizeof(a));
    printf("sizeof(a + b) = %zu bytes  (promoted to int!)\n", sizeof(a + b));
    printf("a + b = %d  (no overflow, computed as int)\n", a + b);

    printf("\n=== Truncation when storing back in char ===\n\n");

    unsigned char c = a + b;
    printf("unsigned char c = a + b;\n");
    printf("c = %u  (300 %% 256 = 44, wraps around)\n", c);

    signed char sc = (signed char)(a + b);
    printf("signed char sc = a + b;\n");
    printf("sc = %d  (implementation-defined truncation)\n", sc);

    printf("\n=== Usual arithmetic conversions (mixed types) ===\n\n");

    int i = 5;
    double d = 2.5;
    printf("int i = %d, double d = %f\n", i, d);
    printf("i + d = %f  (i promoted to double)\n", i + d);
    printf("i * d = %f  (i promoted to double)\n", i * d);
    printf("sizeof(i + d) = %zu bytes  (result is double)\n", sizeof(i + d));

    printf("\n=== Dangerous: signed + unsigned ===\n\n");

    int negative = -1;
    unsigned int positive = 1;
    printf("int negative = %d\n", negative);
    printf("unsigned int positive = %u\n", positive);

    if (negative < (int)positive) {
        printf("With cast: negative < positive  (correct)\n");
    }

    /* Without cast, -1 becomes UINT_MAX, which is > 1 */
    unsigned int result = (unsigned int)negative + positive;
    printf("(unsigned)(-1) + 1 = %u  (-1 becomes %u, then +1 wraps to 0)\n",
           result, UINT_MAX);

    return 0;
}
