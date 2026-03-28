#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== Unsigned overflow: wrapping (defined behavior) ===\n\n");

    unsigned int u = UINT_MAX;
    printf("UINT_MAX     = %u\n", u);
    printf("UINT_MAX + 1 = %u\n", u + 1);

    unsigned int z = 0;
    printf("\n0 - 1 (unsigned) = %u\n", z - 1);

    printf("\n--- Unsigned wrapping demo ---\n");
    unsigned char uc = 250;
    for (int i = 0; i < 10; i++) {
        printf("uc = %3u  ->  uc + 1 = %3u\n", uc, (unsigned char)(uc + 1));
        uc++;
    }

    printf("\n=== Signed overflow: UNDEFINED BEHAVIOR ===\n\n");
    printf("INT_MAX = %d\n", INT_MAX);
    printf("WARNING: The next line triggers signed integer overflow (UB).\n");
    printf("The compiler may produce any result.\n");

    volatile int s = INT_MAX;
    int result = s + 1;
    printf("INT_MAX + 1 = %d  (UB: this result is NOT guaranteed)\n", result);

    return 0;
}
