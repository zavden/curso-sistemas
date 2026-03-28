#include <stdio.h>
#include <limits.h>

void print_bits(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
}

int main(void) {
    printf("=== Signed vs Unsigned: same bits, different interpretation ===\n\n");

    printf("%-10s  %-10s  %-10s  %-10s\n", "Bits", "unsigned", "signed", "Hex");
    printf("%-10s  %-10s  %-10s  %-10s\n", "--------", "--------", "------", "---");

    unsigned char values[] = {0, 1, 127, 128, 129, 200, 255};
    int count = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < count; i++) {
        unsigned char uc = values[i];
        signed char sc = (signed char)uc;

        print_bits(uc);
        printf("  %-10u  %-10d  0x%02X\n", uc, sc, uc);
    }

    printf("\n=== Ranges (8 bits) ===\n");
    printf("unsigned char: %d to %d\n", 0, UCHAR_MAX);
    printf("signed char:   %d to %d\n", SCHAR_MIN, SCHAR_MAX);

    printf("\n=== Ranges (32 bits) ===\n");
    printf("unsigned int: %u to %u\n", 0, UINT_MAX);
    printf("signed int:   %d to %d\n", INT_MIN, INT_MAX);

    return 0;
}
