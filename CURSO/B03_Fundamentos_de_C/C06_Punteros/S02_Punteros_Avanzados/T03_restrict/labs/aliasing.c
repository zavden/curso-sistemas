#include <stdio.h>

void sum_no_restrict(int *a, int *b, int *out, int n) {
    for (int i = 0; i < n; i++) {
        out[i] = a[i] + b[i];
    }
}

void sum_restrict(int *restrict a, int *restrict b, int *restrict out, int n) {
    for (int i = 0; i < n; i++) {
        out[i] = a[i] + b[i];
    }
}

int main(void) {
    int x[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int y[] = {10, 20, 30, 40, 50, 60, 70, 80};
    int result[8];
    int n = 8;

    sum_no_restrict(x, y, result, n);
    printf("sum_no_restrict: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", result[i]);
    }
    printf("\n");

    sum_restrict(x, y, result, n);
    printf("sum_restrict:    ");
    for (int i = 0; i < n; i++) {
        printf("%d ", result[i]);
    }
    printf("\n");

    return 0;
}
