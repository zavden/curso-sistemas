#include <stdio.h>

int main(void) {
    printf("=== while with false condition ===\n");
    int x = 100;
    while (x < 10) {
        printf("while body: x=%d\n", x);
        x++;
    }
    printf("while finished. Executed 0 times.\n\n");

    printf("=== do-while with false condition ===\n");
    int y = 100;
    do {
        printf("do-while body: y=%d\n", y);
        y++;
    } while (y < 10);
    printf("do-while finished. Executed 1 time.\n\n");

    printf("=== while counting to 5 ===\n");
    int a = 1;
    while (a <= 5) {
        printf("%d ", a);
        a++;
    }
    printf("\n\n");

    printf("=== do-while counting to 5 ===\n");
    int b = 1;
    do {
        printf("%d ", b);
        b++;
    } while (b <= 5);
    printf("\n");

    return 0;
}
