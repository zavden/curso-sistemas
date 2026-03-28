#include <stdio.h>
#include <string.h>

int main(void) {
    int guard = 0xDEADBEEF;
    char buf[8];
    int canary = 0x41414141;

    printf("Before strcpy:\n");
    printf("  buf address:    %p\n", (void *)buf);
    printf("  canary value:   0x%X\n", canary);
    printf("  guard value:    0x%X\n", guard);

    /* Overflow: 16 bytes into an 8-byte buffer */
    strcpy(buf, "AAAAAAAABBBBBBBB");

    printf("\nAfter strcpy:\n");
    printf("  buf content:    %s\n", buf);
    printf("  canary value:   0x%X\n", canary);
    printf("  guard value:    0x%X\n", guard);

    if (canary != 0x41414141) {
        printf("\n  canary was corrupted!\n");
    }
    if (guard != (int)0xDEADBEEF) {
        printf("  guard was corrupted!\n");
    }

    return 0;
}
