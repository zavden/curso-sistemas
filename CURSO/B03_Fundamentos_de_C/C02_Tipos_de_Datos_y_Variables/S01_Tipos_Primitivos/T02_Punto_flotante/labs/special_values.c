#include <stdio.h>
#include <math.h>
#include <float.h>

int main(void) {
    printf("=== Infinity ===\n");
    double pos_inf = 1.0 / 0.0;
    double neg_inf = -1.0 / 0.0;
    printf("1.0 / 0.0  = %f\n", pos_inf);
    printf("-1.0 / 0.0 = %f\n", neg_inf);
    printf("isinf(1.0/0.0)? %s\n", isinf(pos_inf) ? "YES" : "NO");
    printf("inf + 1 = %f\n", pos_inf + 1.0);
    printf("inf + inf = %f\n", pos_inf + pos_inf);

    printf("\n=== NaN (Not a Number) ===\n");
    double nan_val = 0.0 / 0.0;
    double nan_sqrt = sqrt(-1.0);
    printf("0.0 / 0.0   = %f\n", nan_val);
    printf("sqrt(-1.0)  = %f\n", nan_sqrt);
    printf("isnan(0.0/0.0)? %s\n", isnan(nan_val) ? "YES" : "NO");

    printf("\n=== NaN comparisons ===\n");
    printf("NaN == NaN?  %s\n", (nan_val == nan_val) ? "YES" : "NO");
    printf("NaN != NaN?  %s\n", (nan_val != nan_val) ? "YES" : "NO");
    printf("NaN > 0?     %s\n", (nan_val > 0.0) ? "YES" : "NO");
    printf("NaN < 0?     %s\n", (nan_val < 0.0) ? "YES" : "NO");
    printf("NaN == 0?    %s\n", (nan_val == 0.0) ? "YES" : "NO");

    printf("\n=== Arithmetic with special values ===\n");
    printf("inf - inf = %f\n", pos_inf - pos_inf);
    printf("inf * 0   = %f\n", pos_inf * 0.0);
    printf("0 / 0     = %f\n", 0.0 / 0.0);
    printf("NaN + 1   = %f\n", nan_val + 1.0);

    printf("\n=== Classification: isfinite, isnormal ===\n");
    double subnormal = DBL_MIN / 2.0;
    printf("isfinite(42.0)?      %s\n", isfinite(42.0) ? "YES" : "NO");
    printf("isfinite(inf)?       %s\n", isfinite(pos_inf) ? "YES" : "NO");
    printf("isfinite(NaN)?       %s\n", isfinite(nan_val) ? "YES" : "NO");
    printf("isnormal(42.0)?      %s\n", isnormal(42.0) ? "YES" : "NO");
    printf("isnormal(0.0)?       %s\n", isnormal(0.0) ? "YES" : "NO");
    printf("isnormal(DBL_MIN/2)? %s (subnormal)\n",
           isnormal(subnormal) ? "YES" : "NO");
    printf("DBL_MIN   = %e\n", DBL_MIN);
    printf("DBL_MIN/2 = %e (subnormal: below DBL_MIN but > 0)\n", subnormal);

    return 0;
}
