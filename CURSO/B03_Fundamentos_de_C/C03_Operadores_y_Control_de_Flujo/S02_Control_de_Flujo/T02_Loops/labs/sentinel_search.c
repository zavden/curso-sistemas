#include <stdio.h>

int find(const int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;
}

int main(void) {
    int data[] = {10, 25, 3, 42, 7, 18, 55};
    int len = (int)(sizeof(data) / sizeof(data[0]));

    printf("Array: ");
    for (int i = 0; i < len; i++) {
        printf("%d ", data[i]);
    }
    printf("\n\n");

    int targets[] = {42, 99, 10, 55};
    int n_targets = (int)(sizeof(targets) / sizeof(targets[0]));

    for (int t = 0; t < n_targets; t++) {
        int pos = find(data, len, targets[t]);
        if (pos == -1) {
            printf("find(%d) = -1 (not found)\n", targets[t]);
        } else {
            printf("find(%d) = %d (found at index %d)\n",
                   targets[t], pos, pos);
        }
    }

    return 0;
}
