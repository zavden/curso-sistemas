#include <stdio.h>
#include <stdint.h>

union FloatBits {
    float f;
    uint32_t u;
};

union IntBytes {
    uint32_t value;
    uint8_t bytes[4];
};

int main(void) {
    /* --- Part A: see the bits of a float --- */
    printf("=== float bit representation ===\n");
    union FloatBits fb;
    fb.f = 3.14f;
    printf("float %.2f as hex: 0x%08X\n", fb.f, fb.u);

    unsigned int sign     = (fb.u >> 31) & 1;
    unsigned int exponent = (fb.u >> 23) & 0xFF;
    unsigned int mantissa = fb.u & 0x7FFFFF;
    printf("  sign     = %u\n", sign);
    printf("  exponent = %u (biased, subtract 127 -> %d)\n",
           exponent, (int)exponent - 127);
    printf("  mantissa = 0x%06X\n", mantissa);

    /* --- Part B: endianness check --- */
    printf("\n=== endianness check ===\n");
    union IntBytes ib;
    ib.value = 0x04030201;
    printf("value = 0x%08X\n", ib.value);
    printf("byte[0] = 0x%02X\n", ib.bytes[0]);
    printf("byte[1] = 0x%02X\n", ib.bytes[1]);
    printf("byte[2] = 0x%02X\n", ib.bytes[2]);
    printf("byte[3] = 0x%02X\n", ib.bytes[3]);

    if (ib.bytes[0] == 0x01) {
        printf("result: little-endian (least significant byte first)\n");
    } else {
        printf("result: big-endian (most significant byte first)\n");
    }

    /* --- Part C: negative float --- */
    printf("\n=== negative float ===\n");
    union FloatBits neg;
    neg.f = -3.14f;
    printf("float %.2f as hex: 0x%08X\n", neg.f, neg.u);
    printf("  sign bit = %u (1 = negative)\n", (neg.u >> 31) & 1);

    printf("\n=== positive vs negative comparison ===\n");
    printf("+3.14f -> 0x%08X\n", fb.u);
    printf("-3.14f -> 0x%08X\n", neg.u);
    printf("XOR    -> 0x%08X (only sign bit differs)\n", fb.u ^ neg.u);

    return 0;
}
