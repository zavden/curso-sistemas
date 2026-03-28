#include <stdio.h>
#include <stdint.h>

void print_bits_8(uint8_t value) {
    printf("0x%02X = ", value);
    for (int i = 7; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
        if (i == 4) printf(" ");
    }
    printf("\n");
}

int main(void) {
    uint8_t a = 0xAC;
    uint8_t b = 0xF0;

    printf("=== AND (&) ===\n");
    printf("  a       = ");
    print_bits_8(a);
    printf("  b       = ");
    print_bits_8(b);
    printf("  a & b   = ");
    print_bits_8(a & b);

    printf("\n=== OR (|) ===\n");
    uint8_t c = 0xA0;
    uint8_t d = 0x0C;
    printf("  c       = ");
    print_bits_8(c);
    printf("  d       = ");
    print_bits_8(d);
    printf("  c | d   = ");
    print_bits_8(c | d);

    printf("\n=== XOR (^) ===\n");
    printf("  a       = ");
    print_bits_8(a);
    printf("  b       = ");
    print_bits_8(b);
    printf("  a ^ b   = ");
    print_bits_8(a ^ b);

    printf("\n=== NOT (~) ===\n");
    printf("  a       = ");
    print_bits_8(a);
    printf("  ~a      = ");
    print_bits_8((uint8_t)~a);

    printf("\n=== XOR properties ===\n");
    printf("  a ^ a   = ");
    print_bits_8(a ^ a);
    printf("  a ^ 0   = ");
    print_bits_8(a ^ 0);

    return 0;
}
