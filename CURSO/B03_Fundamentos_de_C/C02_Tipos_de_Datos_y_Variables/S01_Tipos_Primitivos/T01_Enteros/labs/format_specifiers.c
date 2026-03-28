#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void) {
    printf("=== format specifiers for stdint.h types ===\n\n");

    int32_t  a = 2147483647;
    uint32_t b = 4294967295U;
    int64_t  c = 9223372036854775807LL;
    uint64_t d = 18446744073709551615ULL;

    printf("int32_t  max: %" PRId32 "\n", a);
    printf("uint32_t max: %" PRIu32 "\n", b);
    printf("int64_t  max: %" PRId64 "\n", c);
    printf("uint64_t max: %" PRIu64 "\n", d);

    printf("\n=== hexadecimal format ===\n\n");

    uint32_t hex_val = 0xDEADBEEF;

    printf("uint32_t hex (lowercase): %" PRIx32 "\n", hex_val);
    printf("uint32_t hex (uppercase): %" PRIX32 "\n", hex_val);

    uint64_t big_hex = 0xCAFEBABEDEADC0DEULL;

    printf("uint64_t hex (lowercase): %" PRIx64 "\n", big_hex);
    printf("uint64_t hex (uppercase): %" PRIX64 "\n", big_hex);

    printf("\n=== limits from stdint.h ===\n\n");

    printf("INT8_MIN:   %" PRId8  "\n", INT8_MIN);
    printf("INT8_MAX:   %" PRId8  "\n", INT8_MAX);
    printf("UINT8_MAX:  %" PRIu8  "\n", UINT8_MAX);
    printf("INT16_MIN:  %" PRId16 "\n", INT16_MIN);
    printf("INT16_MAX:  %" PRId16 "\n", INT16_MAX);
    printf("UINT16_MAX: %" PRIu16 "\n", UINT16_MAX);
    printf("INT32_MIN:  %" PRId32 "\n", INT32_MIN);
    printf("INT32_MAX:  %" PRId32 "\n", INT32_MAX);
    printf("UINT32_MAX: %" PRIu32 "\n", UINT32_MAX);
    printf("INT64_MIN:  %" PRId64 "\n", INT64_MIN);
    printf("INT64_MAX:  %" PRId64 "\n", INT64_MAX);
    printf("UINT64_MAX: %" PRIu64 "\n", UINT64_MAX);

    return 0;
}
