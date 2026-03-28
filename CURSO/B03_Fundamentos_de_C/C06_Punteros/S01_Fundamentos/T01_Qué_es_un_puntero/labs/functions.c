#include <stdio.h>

void increment(int *p) {
    (*p)++;
}

void find_extremes(const int *arr, int n, int *min, int *max) {
    *min = arr[0];
    *max = arr[0];

    for (int i = 1; i < n; i++) {
        if (arr[i] < *min) {
            *min = arr[i];
        }
        if (arr[i] > *max) {
            *max = arr[i];
        }
    }
}

int main(void) {
    /* --- increment demo --- */
    int x = 10;
    printf("Before increment: x = %d\n", x);
    increment(&x);
    printf("After increment:  x = %d\n", x);
    increment(&x);
    increment(&x);
    printf("After 2 more:     x = %d\n", x);

    /* --- find_extremes demo --- */
    int data[] = {5, 2, 8, 1, 9, 3};
    int n = sizeof(data) / sizeof(data[0]);

    int min, max;
    find_extremes(data, n, &min, &max);

    printf("\nArray: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", data[i]);
    }
    printf("\nMin: %d\nMax: %d\n", min, max);

    return 0;
}
