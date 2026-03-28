#include <stdio.h>

int bar(void);

int main(void) {
    printf("bar(1, 2, 3) = %d\n", bar(1, 2, 3));
    return 0;
}

int bar(void) {
    return 99;
}
