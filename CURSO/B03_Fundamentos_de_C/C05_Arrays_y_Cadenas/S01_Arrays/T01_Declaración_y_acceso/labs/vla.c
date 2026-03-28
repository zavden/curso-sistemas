#include <stdio.h>

static void fill_and_print(int n) {
    int data[n];    /* VLA - size determined at runtime */

    for (int i = 0; i < n; i++) {
        data[i] = i * 10;
    }

    printf("n = %d, sizeof(data) = %zu\n", n, sizeof(data));
    printf("  data = {");
    for (int i = 0; i < n; i++) {
        printf("%d", data[i]);
        if (i < n - 1) {
            printf(", ");
        }
    }
    printf("}\n");
}

int main(void) {
    fill_and_print(3);
    fill_and_print(5);
    fill_and_print(8);

    return 0;
}
