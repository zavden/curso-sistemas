#include <stdio.h>

int main(void) {
    printf("--- No init ---\n");
    int i = 10;
    for (; i < 15; i++) {
        printf("%d ", i);
    }
    printf("\n");

    printf("--- No update ---\n");
    for (int j = 0; j < 10;) {
        printf("%d ", j);
        j += 3;
    }
    printf("\n");

    printf("--- No condition (infinite, break at 3) ---\n");
    int count = 0;
    for (;;) {
        if (count >= 3) {
            break;
        }
        printf("iteration %d\n", count);
        count++;
    }

    return 0;
}
