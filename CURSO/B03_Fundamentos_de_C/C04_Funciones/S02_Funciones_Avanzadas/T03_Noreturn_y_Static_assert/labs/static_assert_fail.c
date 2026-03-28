#include <stdio.h>
#include <assert.h>

/* This assertion will FAIL at compile time */
static_assert(sizeof(int) == 2, "int must be 2 bytes (this will fail on most platforms)");

int main(void) {
    printf("You should never see this message.\n");
    return 0;
}
