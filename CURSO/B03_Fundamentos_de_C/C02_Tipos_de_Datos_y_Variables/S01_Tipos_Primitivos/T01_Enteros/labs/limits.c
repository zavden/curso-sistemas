#include <stdio.h>
#include <limits.h>

int main(void) {
    printf("=== signed type ranges ===\n\n");

    printf("char:      %d to %d\n", CHAR_MIN, CHAR_MAX);
    printf("short:     %d to %d\n", SHRT_MIN, SHRT_MAX);
    printf("int:       %d to %d\n", INT_MIN, INT_MAX);
    printf("long:      %ld to %ld\n", LONG_MIN, LONG_MAX);
    printf("long long: %lld to %lld\n", LLONG_MIN, LLONG_MAX);

    printf("\n=== unsigned type ranges ===\n\n");

    printf("unsigned char:      0 to %u\n", UCHAR_MAX);
    printf("unsigned short:     0 to %u\n", USHRT_MAX);
    printf("unsigned int:       0 to %u\n", UINT_MAX);
    printf("unsigned long:      0 to %lu\n", ULONG_MAX);
    printf("unsigned long long: 0 to %llu\n", ULLONG_MAX);

    return 0;
}
