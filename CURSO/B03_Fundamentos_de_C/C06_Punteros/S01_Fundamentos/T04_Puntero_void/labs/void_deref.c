#include <stdio.h>

int main(void) {
    int x = 42;
    double d = 3.14;

    void *vp;

    /* To access the value, cast void* to the correct type */
    vp = &x;
    int *ip = vp;
    printf("Via int*:    %d\n", *ip);
    printf("Via inline cast: %d\n", *(int *)vp);

    vp = &d;
    double *dp = vp;
    printf("Via double*: %f\n", *dp);
    printf("Via inline cast: %f\n", *(double *)vp);

    /* Dangerous: casting to the wrong type */
    vp = &x;
    double *wrong = vp;
    printf("\nDangerous wrong cast:\n");
    printf("  x as int:    %d\n", *(int *)vp);
    printf("  x as double: %f  (garbage — UB)\n", *wrong);

    /* sizeof on typed pointers vs void* */
    printf("\nsizeof through typed pointers:\n");
    printf("  sizeof(int):    %zu bytes\n", sizeof(int));
    printf("  sizeof(double): %zu bytes\n", sizeof(double));
    printf("  sizeof(char):   %zu bytes\n", sizeof(char));

    return 0;
}
