#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* abs_val: selecciona la funcion de valor absoluto correcta por tipo */
#define abs_val(x) _Generic((x), \
    int:         abs,             \
    long:        labs,            \
    long long:   llabs,           \
    float:       fabsf,           \
    double:      fabs             \
)(x)

/* Funciones auxiliares para max type-safe */
static inline int    max_int(int a, int b)       { return a > b ? a : b; }
static inline double max_dbl(double a, double b) { return a > b ? a : b; }
static inline long   max_lng(long a, long b)     { return a > b ? a : b; }

/* max: selecciona la funcion max correcta por tipo del primer argumento */
#define max(a, b) _Generic((a), \
    int:    max_int,            \
    double: max_dbl,            \
    long:   max_lng             \
)(a, b)

int main(void) {
    /* abs_val con distintos tipos */
    printf("--- abs_val ---\n");

    int ni = -42;
    printf("abs_val(%d)    = %d\n", ni, abs_val(ni));

    long nl = -100000L;
    printf("abs_val(%ldL)  = %ld\n", nl, abs_val(nl));

    double nd = -3.14;
    printf("abs_val(%f) = %f\n", nd, abs_val(nd));

    float nf = -2.71f;
    printf("abs_val(%ff)  = %f\n", nf, abs_val(nf));

    printf("\n");

    /* max con distintos tipos */
    printf("--- max ---\n");

    int a = 10, b = 25;
    printf("max(%d, %d) = %d\n", a, b, max(a, b));

    double x = 1.5, y = 2.7;
    printf("max(%f, %f) = %f\n", x, y, max(x, y));

    long p = 100L, q = -50L;
    printf("max(%ld, %ld) = %ld\n", p, q, max(p, q));

    return 0;
}
