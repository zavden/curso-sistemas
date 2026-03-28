#include <stdio.h>

// --- Transform functions ---
int double_it(int x) { return x * 2; }
int square(int x)    { return x * x; }
int negate(int x)    { return -x; }

// --- apply: transforms each element in place ---
void apply(int *arr, int n, int (*transform)(int)) {
    for (int i = 0; i < n; i++) {
        arr[i] = transform(arr[i]);
    }
}

// --- print_array: helper to display arrays ---
void print_array(const char *label, const int *arr, int n) {
    printf("%s {", label);
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("}\n");
}

// --- Predicate functions ---
int is_even(int x)     { return x % 2 == 0; }
int is_positive(int x) { return x > 0; }

// --- filter_count: counts elements matching predicate ---
int filter_count(const int *arr, int n, int (*predicate)(int)) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (predicate(arr[i])) {
            count++;
        }
    }
    return count;
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    int n = 5;

    printf("=== apply() with different transforms ===\n");
    print_array("Original: ", data, n);

    apply(data, n, double_it);
    print_array("After double_it:", data, n);

    apply(data, n, negate);
    print_array("After negate:   ", data, n);

    // Reset data for filter_count demo
    int data2[] = {-3, 0, 7, -1, 4, 12, -8, 2};
    int n2 = 8;

    printf("\n=== filter_count() with predicates ===\n");
    print_array("Array:", data2, n2);
    printf("Even numbers:     %d\n", filter_count(data2, n2, is_even));
    printf("Positive numbers: %d\n", filter_count(data2, n2, is_positive));

    return 0;
}
