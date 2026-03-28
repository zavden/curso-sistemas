#include <stdio.h>

void try_to_modify(int x) {
    printf("  Inside (before):  x = %d\n", x);
    x = 999;
    printf("  Inside (after):   x = %d\n", x);
}

int main(void) {
    int a = 42;
    printf("Before call: a = %d\n", a);
    try_to_modify(a);
    printf("After call:  a = %d\n", a);
    return 0;
}
