#include <stdio.h>
#include <limits.h>
#include <string.h>

int main(void) {
    printf("=== Implicit conversion: signed to unsigned ===\n\n");

    int s = -1;
    unsigned int u = (unsigned int)s;
    printf("int s = -1\n");
    printf("unsigned int u = s  ->  u = %u\n\n", u);

    printf("=== Comparison trap: signed vs unsigned ===\n\n");

    int neg = -1;
    unsigned int zero = 0;

    printf("Comparing: int neg = %d  vs  unsigned int zero = %u\n", neg, zero);
    printf("Is neg < zero?  ");

    if (neg < zero) {
        printf("YES (neg is less)\n");
    } else {
        printf("NO (neg is NOT less!)\n");
    }

    printf("\nExplanation:\n");
    printf("  neg (-1) is converted to unsigned before comparison.\n");
    printf("  -1 as unsigned = %u (UINT_MAX)\n", (unsigned int)-1);
    printf("  %u < %u ?  -> false\n\n", (unsigned int)-1, zero);

    printf("=== size_t comparison trap ===\n\n");

    const char *str = "Hello";
    size_t len = strlen(str);
    int n = -1;

    printf("strlen(\"%s\") = %zu\n", str, len);
    printf("n = %d\n", n);
    printf("Is len >= n?  (expecting YES, since 5 >= -1)\n");

    /* n is implicitly converted to size_t before comparison */
    if (len >= (size_t)n) {
        printf("  Result: YES\n");
    } else {
        printf("  Result: NO!  (unexpected)\n");
    }

    printf("\n  Why? n (-1) as size_t = %zu\n", (size_t)n);
    printf("  That is SIZE_MAX. 5 >= SIZE_MAX? -> false!\n");

    return 0;
}
