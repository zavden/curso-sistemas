#include <stdio.h>

int check(int x) {
    printf("check(%d) ", x);
    return x;
}

int main(void) {
    printf("--- Short-circuit evaluation ---\n\n");

    printf("A: %d\n", check(0) && check(1));
    printf("B: %d\n", check(1) && check(0));
    printf("C: %d\n", check(0) || check(1));
    printf("D: %d\n", check(1) || check(0));

    printf("\n--- Side effects con short-circuit ---\n\n");

    int x = 0, y = 0;
    printf("Antes: x=%d, y=%d\n", x, y);

    if (x++ || y++) {
        printf("Entro al if\n");
    } else {
        printf("No entro al if\n");
    }
    printf("Despues: x=%d, y=%d\n", x, y);

    printf("\n--- Segunda ronda (x ya no es 0) ---\n\n");
    y = 0;
    printf("Antes: x=%d, y=%d\n", x, y);

    if (x++ || y++) {
        printf("Entro al if\n");
    } else {
        printf("No entro al if\n");
    }
    printf("Despues: x=%d, y=%d\n", x, y);

    return 0;
}
