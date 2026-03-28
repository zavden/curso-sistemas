#include <stdio.h>

int main(void) {
    printf("Step A\n");
    goto skip;
    printf("Step B\n");    /* skipped by goto */
skip:
    printf("Step C\n");
    printf("Done\n");
    return 0;
}
