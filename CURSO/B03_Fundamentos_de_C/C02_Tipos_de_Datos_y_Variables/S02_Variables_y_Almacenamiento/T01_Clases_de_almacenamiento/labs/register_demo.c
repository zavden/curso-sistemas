#include <stdio.h>

int sum_with_register(const int *arr, int n) {
    register int total = 0;
    for (register int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

int sum_without_register(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

int main(void) {
    int data[] = {10, 20, 30, 40, 50};
    int n = 5;

    printf("With register:    %d\n", sum_with_register(data, n));
    printf("Without register: %d\n", sum_without_register(data, n));

    return 0;
}
