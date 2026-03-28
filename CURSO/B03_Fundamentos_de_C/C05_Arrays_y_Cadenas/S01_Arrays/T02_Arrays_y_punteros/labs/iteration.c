#include <stdio.h>

int main(void) {
    int arr[] = {5, 10, 15, 20, 25, 30};
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("=== Method 1: index (arr[i]) ===\n");
    for (int i = 0; i < n; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }

    printf("\n=== Method 2: pointer increment (p++) ===\n");
    for (int *p = arr; p < arr + n; p++) {
        printf("*p = %d  (p = %p)\n", *p, (void *)p);
    }

    printf("\n=== Method 3: arithmetic (*(arr + i)) ===\n");
    for (int i = 0; i < n; i++) {
        printf("*(arr + %d) = %d\n", i, *(arr + i));
    }

    printf("\n=== Method 4: pointer with end sentinel ===\n");
    int *end = arr + n;
    printf("arr   = %p\n", (void *)arr);
    printf("end   = %p  (one past last element)\n", (void *)end);
    printf("count = %td elements\n", end - arr);
    for (int *p = arr; p != end; p++) {
        printf("*p = %d\n", *p);
    }

    return 0;
}
