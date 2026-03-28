#include <stdio.h>

int main(void) {
    printf("--- Count up ---\n");
    for (int i = 0; i < 5; i++) {
        printf("%d ", i);
    }
    printf("\n");

    printf("--- Count down ---\n");
    for (int i = 4; i >= 0; i--) {
        printf("%d ", i);
    }
    printf("\n");

    printf("--- Step by 3 ---\n");
    for (int i = 0; i < 15; i += 3) {
        printf("%d ", i);
    }
    printf("\n");

    printf("--- Two variables ---\n");
    for (int lo = 0, hi = 9; lo < hi; lo++, hi--) {
        printf("lo=%d hi=%d\n", lo, hi);
    }

    return 0;
}
