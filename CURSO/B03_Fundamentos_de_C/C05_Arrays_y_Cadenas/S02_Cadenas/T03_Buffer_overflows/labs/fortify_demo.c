#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[8];

    printf("Buffer size: %zu bytes\n", sizeof(buf));
    printf("Attempting strcpy of a long string into buf...\n");

    /* This overflows buf[8] -- FORTIFY_SOURCE will abort at runtime */
    strcpy(buf, "This string is way too long for buf");

    printf("buf: %s\n", buf);

    return 0;
}
