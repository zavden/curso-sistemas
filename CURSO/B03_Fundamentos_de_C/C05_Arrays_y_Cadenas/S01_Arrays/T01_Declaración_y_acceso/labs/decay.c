#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static void print_info(int arr[], int len) {
    /* Inside a function, arr is a POINTER, not an array */
    printf("  sizeof(arr) inside function = %zu (size of a pointer)\n",
           sizeof(arr));
    printf("  ARRAY_SIZE would give: %zu (WRONG!)\n",
           sizeof(arr) / sizeof(arr[0]));

    printf("  Correct iteration using len parameter:\n");
    for (int i = 0; i < len; i++) {
        printf("    arr[%d] = %d\n", i, arr[i]);
    }
}

int main(void) {
    int data[5] = {10, 20, 30, 40, 50};

    printf("sizeof(data) in main = %zu (real array size)\n", sizeof(data));
    printf("ARRAY_SIZE(data) = %zu (correct count)\n", ARRAY_SIZE(data));
    printf("\nCalling print_info(data, 5):\n");

    print_info(data, (int)ARRAY_SIZE(data));

    return 0;
}
