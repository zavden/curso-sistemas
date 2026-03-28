#include <stdio.h>

int main(void) {
    /* Classic: 0.1 + 0.2 != 0.3 */
    double a = 0.1 + 0.2;
    double b = 0.3;

    printf("=== 0.1 + 0.2 vs 0.3 ===\n");
    printf("0.1 + 0.2 = %.20f\n", a);
    printf("0.3       = %.20f\n", b);
    printf("Are they equal (==)? %s\n", (a == b) ? "YES" : "NO");

    /* Accumulation with float */
    printf("\n=== Accumulation with float (1 million times 0.1f) ===\n");
    float sum_f = 0.0f;
    for (int i = 0; i < 1000000; i++) {
        sum_f += 0.1f;
    }
    printf("Expected: 100000.0\n");
    printf("Got:      %.2f\n", (double)sum_f);
    printf("Error:    %.2f\n", (double)(sum_f - 100000.0f));

    /* Accumulation with double */
    printf("\n=== Accumulation with double (1 million times 0.1) ===\n");
    double sum_d = 0.0;
    for (int i = 0; i < 1000000; i++) {
        sum_d += 0.1;
    }
    printf("Expected: 100000.0\n");
    printf("Got:      %.10f\n", sum_d);
    printf("Error:    %.10f\n", sum_d - 100000.0);

    /* Loss of precision by magnitude */
    printf("\n=== Loss of precision by magnitude ===\n");
    float big = 1e10f;
    float small = 1.0f;
    float result = big + small;
    printf("1e10f + 1.0f = %.0f\n", (double)result);
    printf("Expected:      10000000001\n");
    printf("The 1.0 was %s\n",
           (result == big) ? "LOST (absorbed)" : "preserved");

    return 0;
}
