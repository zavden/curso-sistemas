#include <stdio.h>

/*
 * This function demonstrates the aliasing problem.
 * Without restrict, the compiler must assume that dst and src
 * might point to overlapping memory.
 *
 * If dst == src, writing to dst[i] changes src[i].
 * The compiler cannot cache src[i] in a register.
 */

void scale_add(double *dst, const double *src, double factor, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] += src[i] * factor;
    }
}

void scale_add_restrict(double *restrict dst, const double *restrict src,
                        double factor, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] += src[i] * factor;
    }
}

int main(void) {
    double a[] = {1.0, 2.0, 3.0, 4.0};
    double b[] = {10.0, 20.0, 30.0, 40.0};

    /* Non-aliased call: dst and src are different arrays */
    scale_add(a, b, 2.0, 4);
    printf("Non-aliased result: ");
    for (int i = 0; i < 4; i++) {
        printf("%.1f ", a[i]);
    }
    printf("\n");

    /* Aliased call: dst and src are the SAME array */
    double self[] = {1.0, 2.0, 3.0, 4.0};
    scale_add(self, self, 1.0, 4);
    printf("Self-aliased result: ");
    for (int i = 0; i < 4; i++) {
        printf("%.1f ", self[i]);
    }
    printf("\n");

    return 0;
}
