#include <stdio.h>
#include <float.h>

int main(void) {
    printf("=== Sizes ===\n");
    printf("float:       %zu bytes\n", sizeof(float));
    printf("double:      %zu bytes\n", sizeof(double));
    printf("long double: %zu bytes\n", sizeof(long double));

    printf("\n=== Significant decimal digits ===\n");
    printf("float:       %d (FLT_DIG)\n", FLT_DIG);
    printf("double:      %d (DBL_DIG)\n", DBL_DIG);
    printf("long double: %d (LDBL_DIG)\n", LDBL_DIG);

    printf("\n=== Ranges ===\n");
    printf("float:       [%e, %e]\n", FLT_MIN, FLT_MAX);
    printf("double:      [%e, %e]\n", DBL_MIN, DBL_MAX);
    printf("long double: [%Le, %Le]\n", LDBL_MIN, LDBL_MAX);

    printf("\n=== Epsilon (smallest difference near 1.0) ===\n");
    printf("FLT_EPSILON:  %e\n", FLT_EPSILON);
    printf("DBL_EPSILON:  %e\n", DBL_EPSILON);
    printf("LDBL_EPSILON: %Le\n", LDBL_EPSILON);

    printf("\n=== Precision demo ===\n");
    printf("0.1 as float:       %.20f\n", (double)0.1f);
    printf("0.1 as double:      %.20f\n", 0.1);
    printf("0.1 as long double: %.20Lf\n", 0.1L);

    return 0;
}
