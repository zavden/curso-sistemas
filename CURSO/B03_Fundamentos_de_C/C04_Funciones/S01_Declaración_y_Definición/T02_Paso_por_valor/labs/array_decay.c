#include <stdio.h>

void print_sizeof(int arr[], int n) {
    printf("  sizeof(arr) inside function: %zu (pointer size)\n", sizeof(arr));
    printf("  Elements received: %d\n", n);
}

void double_elements(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] *= 2;
    }
}

void print_array(const char *label, const int *arr, int n) {
    printf("%s: {", label);
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) {
            printf(", ");
        }
    }
    printf("}\n");
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    int n = sizeof(data) / sizeof(data[0]);

    printf("sizeof(data) in main: %zu (full array size)\n", sizeof(data));
    print_sizeof(data, n);

    printf("\n");
    print_array("Before double_elements", data, n);
    double_elements(data, n);
    print_array("After double_elements ", data, n);

    return 0;
}
