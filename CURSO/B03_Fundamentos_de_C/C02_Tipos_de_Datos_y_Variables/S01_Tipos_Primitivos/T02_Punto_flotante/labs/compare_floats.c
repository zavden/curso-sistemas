#include <stdio.h>
#include <math.h>
#include <float.h>

/* Absolute epsilon comparison */
int approx_equal_abs(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/* Relative epsilon comparison */
int approx_equal_rel(double a, double b, double rel_epsilon) {
    double max_val = fmax(fabs(a), fabs(b));
    if (max_val == 0.0) return 1;  /* both zero */
    return fabs(a - b) / max_val < rel_epsilon;
}

int main(void) {
    double a = 0.1 + 0.2;
    double b = 0.3;

    printf("=== Comparison with == (WRONG) ===\n");
    printf("0.1 + 0.2 == 0.3? %s\n", (a == b) ? "YES" : "NO");

    printf("\n=== Comparison with absolute epsilon ===\n");
    printf("epsilon = 1e-9:  %s\n",
           approx_equal_abs(a, b, 1e-9) ? "EQUAL" : "NOT EQUAL");
    printf("epsilon = 1e-16: %s\n",
           approx_equal_abs(a, b, 1e-16) ? "EQUAL" : "NOT EQUAL");
    printf("DBL_EPSILON = %e\n", DBL_EPSILON);

    printf("\n=== Problem with absolute epsilon on large numbers ===\n");
    double big_a = 1e15 + 1.0;
    double big_b = 1e15;
    printf("1e15 + 1.0 == 1e15? (with ==):       %s\n",
           (big_a == big_b) ? "YES" : "NO");
    printf("With abs epsilon 1e-9:                %s\n",
           approx_equal_abs(big_a, big_b, 1e-9) ? "EQUAL" : "NOT EQUAL");
    printf("Actual difference: %f\n", fabs(big_a - big_b));

    printf("\n=== Comparison with relative epsilon ===\n");
    printf("0.1+0.2 vs 0.3 (rel 1e-9):           %s\n",
           approx_equal_rel(a, b, 1e-9) ? "EQUAL" : "NOT EQUAL");
    printf("1e15+1 vs 1e15 (rel 1e-9):            %s\n",
           approx_equal_rel(big_a, big_b, 1e-9) ? "EQUAL" : "NOT EQUAL");

    printf("\n=== FLT_EPSILON demo ===\n");
    float one = 1.0f;
    float above = one + FLT_EPSILON;
    float below = one + FLT_EPSILON / 2.0f;
    printf("1.0f + FLT_EPSILON     == 1.0f? %s\n",
           (above == one) ? "YES (indistinguishable)" : "NO (different)");
    printf("1.0f + FLT_EPSILON/2   == 1.0f? %s\n",
           (below == one) ? "YES (indistinguishable)" : "NO (different)");

    return 0;
}
