#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== sizeof of integer types ===\n\n");

    printf("char:      %zu byte(s)\n", sizeof(char));
    printf("short:     %zu bytes\n", sizeof(short));
    printf("int:       %zu bytes\n", sizeof(int));
    printf("long:      %zu bytes\n", sizeof(long));
    printf("long long: %zu bytes\n", sizeof(long long));

    printf("\n=== signed vs unsigned (same sizes) ===\n\n");

    printf("unsigned char:      %zu byte(s)\n", sizeof(unsigned char));
    printf("unsigned short:     %zu bytes\n", sizeof(unsigned short));
    printf("unsigned int:       %zu bytes\n", sizeof(unsigned int));
    printf("unsigned long:      %zu bytes\n", sizeof(unsigned long));
    printf("unsigned long long: %zu bytes\n", sizeof(unsigned long long));

    printf("\n=== is plain char signed or unsigned? ===\n\n");

    if (CHAR_MIN == 0) {
        printf("char is unsigned (CHAR_MIN = 0)\n");
    } else {
        printf("char is signed (CHAR_MIN = %d)\n", CHAR_MIN);
    }

    return 0;
}
