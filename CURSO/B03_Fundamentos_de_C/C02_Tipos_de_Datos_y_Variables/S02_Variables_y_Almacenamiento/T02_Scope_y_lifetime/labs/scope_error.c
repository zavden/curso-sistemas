#include <stdio.h>

int main(void) {
    {
        int inner = 42;
        printf("inner = %d\n", inner);
    }

    /* ERROR: inner is out of scope here */
    printf("inner = %d\n", inner);

    return 0;
}
