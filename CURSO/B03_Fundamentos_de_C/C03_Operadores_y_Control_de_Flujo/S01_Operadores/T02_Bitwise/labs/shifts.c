#include <stdio.h>
#include <stdint.h>

void print_bits_8(uint8_t value) {
    printf("0x%02X = ", value);
    for (int i = 7; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
        if (i == 4) printf(" ");
    }
    printf(" (%u)", value);
    printf("\n");
}

int main(void) {
    uint8_t x = 10;

    printf("=== Shift left (<<) — multiply by powers of 2 ===\n");
    printf("  x        = ");
    print_bits_8(x);
    printf("  x << 1   = ");
    print_bits_8(x << 1);
    printf("  x << 2   = ");
    print_bits_8(x << 2);
    printf("  x << 3   = ");
    print_bits_8(x << 3);

    printf("\n=== Shift right (>>) — divide by powers of 2 ===\n");
    uint8_t y = 80;
    printf("  y        = ");
    print_bits_8(y);
    printf("  y >> 1   = ");
    print_bits_8(y >> 1);
    printf("  y >> 2   = ");
    print_bits_8(y >> 2);
    printf("  y >> 3   = ");
    print_bits_8(y >> 3);

    printf("\n=== Creating single-bit masks with 1 << N ===\n");
    for (int i = 0; i < 8; i++) {
        printf("  1 << %d   = ", i);
        print_bits_8(1 << i);
    }

    printf("\n=== Extracting nibbles ===\n");
    uint8_t byte = 0xD6;
    printf("  byte          = ");
    print_bits_8(byte);
    printf("  low  nibble   = ");
    print_bits_8(byte & 0x0F);
    printf("  high nibble   = ");
    print_bits_8((byte >> 4) & 0x0F);

    return 0;
}
