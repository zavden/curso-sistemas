#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void) {
    int data[5] = {10, 20, 30, 40, 50};

    /* Read elements */
    printf("data[0] = %d\n", data[0]);
    printf("data[4] = %d\n", data[4]);

    /* Modify an element */
    data[2] = 99;
    printf("\nAfter data[2] = 99:\n");

    /* Iterate with for */
    int n = (int)ARRAY_SIZE(data);
    for (int i = 0; i < n; i++) {
        printf("  data[%d] = %d\n", i, data[i]);
    }

    return 0;
}
