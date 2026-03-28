#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static void print_array(const char *label, const int *arr, size_t len) {
    printf("%s = {", label);
    for (size_t i = 0; i < len; i++) {
        printf("%d", arr[i]);
        if (i < len - 1) {
            printf(", ");
        }
    }
    printf("}\n");
}

int main(void) {
    /* Full initialization */
    int full[5] = {10, 20, 30, 40, 50};
    print_array("full", full, ARRAY_SIZE(full));

    /* Partial initialization - remaining elements become 0 */
    int partial[5] = {10, 20};
    print_array("partial", partial, ARRAY_SIZE(partial));

    /* All zeros */
    int zeros[5] = {0};
    print_array("zeros", zeros, ARRAY_SIZE(zeros));

    /* Compiler-determined size */
    int auto_size[] = {1, 2, 3, 4};
    printf("\nauto_size count = %zu\n", ARRAY_SIZE(auto_size));
    print_array("auto_size", auto_size, ARRAY_SIZE(auto_size));

    /* Designated initializers (C99) */
    int desig[10] = {
        [0] = 100,
        [5] = 500,
        [9] = 900,
    };
    printf("\n");
    print_array("desig", desig, ARRAY_SIZE(desig));

    return 0;
}
