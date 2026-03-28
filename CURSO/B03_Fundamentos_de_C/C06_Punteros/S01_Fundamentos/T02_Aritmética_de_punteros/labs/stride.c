/* stride.c -- Demonstrates that p+n advances sizeof(*p)*n bytes */
#include <stdio.h>

int main(void) {
    int    arr_i[] = {10, 20, 30, 40, 50};
    char   arr_c[] = {'A', 'B', 'C', 'D', 'E'};
    double arr_d[] = {1.1, 2.2, 3.3, 4.4, 5.5};

    int    *pi = arr_i;
    char   *pc = arr_c;
    double *pd = arr_d;

    printf("=== sizeof each type ===\n");
    printf("sizeof(int)    = %zu\n", sizeof(int));
    printf("sizeof(char)   = %zu\n", sizeof(char));
    printf("sizeof(double) = %zu\n", sizeof(double));

    printf("\n=== int* stride ===\n");
    printf("pi     = %p  -> *pi     = %d\n", (void *)pi, *pi);
    printf("pi + 1 = %p  -> *(pi+1) = %d\n", (void *)(pi + 1), *(pi + 1));
    printf("pi + 2 = %p  -> *(pi+2) = %d\n", (void *)(pi + 2), *(pi + 2));
    printf("Difference in bytes: %td\n",
           (char *)(pi + 1) - (char *)pi);

    printf("\n=== char* stride ===\n");
    printf("pc     = %p  -> *pc     = %c\n", (void *)pc, *pc);
    printf("pc + 1 = %p  -> *(pc+1) = %c\n", (void *)(pc + 1), *(pc + 1));
    printf("pc + 2 = %p  -> *(pc+2) = %c\n", (void *)(pc + 2), *(pc + 2));
    printf("Difference in bytes: %td\n",
           (char *)(pc + 1) - (char *)pc);

    printf("\n=== double* stride ===\n");
    printf("pd     = %p  -> *pd     = %.1f\n", (void *)pd, *pd);
    printf("pd + 1 = %p  -> *(pd+1) = %.1f\n", (void *)(pd + 1), *(pd + 1));
    printf("pd + 2 = %p  -> *(pd+2) = %.1f\n", (void *)(pd + 2), *(pd + 2));
    printf("Difference in bytes: %td\n",
           (char *)(pd + 1) - (char *)pd);

    return 0;
}
