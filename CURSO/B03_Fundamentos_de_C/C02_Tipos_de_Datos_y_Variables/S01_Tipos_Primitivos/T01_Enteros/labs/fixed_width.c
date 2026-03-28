#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

int main(void) {
    printf("=== exact-width types (stdint.h) ===\n\n");

    printf("int8_t:   %zu byte(s)\n", sizeof(int8_t));
    printf("int16_t:  %zu bytes\n", sizeof(int16_t));
    printf("int32_t:  %zu bytes\n", sizeof(int32_t));
    printf("int64_t:  %zu bytes\n", sizeof(int64_t));

    printf("\nuint8_t:  %zu byte(s)\n", sizeof(uint8_t));
    printf("uint16_t: %zu bytes\n", sizeof(uint16_t));
    printf("uint32_t: %zu bytes\n", sizeof(uint32_t));
    printf("uint64_t: %zu bytes\n", sizeof(uint64_t));

    printf("\n=== least-width types ===\n\n");

    printf("int_least8_t:  %zu byte(s)\n", sizeof(int_least8_t));
    printf("int_least16_t: %zu bytes\n", sizeof(int_least16_t));
    printf("int_least32_t: %zu bytes\n", sizeof(int_least32_t));
    printf("int_least64_t: %zu bytes\n", sizeof(int_least64_t));

    printf("\n=== fast types ===\n\n");

    printf("int_fast8_t:  %zu byte(s)\n", sizeof(int_fast8_t));
    printf("int_fast16_t: %zu bytes\n", sizeof(int_fast16_t));
    printf("int_fast32_t: %zu bytes\n", sizeof(int_fast32_t));
    printf("int_fast64_t: %zu bytes\n", sizeof(int_fast64_t));

    printf("\n=== special types ===\n\n");

    printf("intmax_t:  %zu bytes\n", sizeof(intmax_t));
    printf("intptr_t:  %zu bytes\n", sizeof(intptr_t));
    printf("size_t:    %zu bytes\n", sizeof(size_t));
    printf("ptrdiff_t: %zu bytes\n", sizeof(ptrdiff_t));

    return 0;
}
