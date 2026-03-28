#include <stdio.h>
#include <string.h>

int main(void) {
    /* Three equivalent ways to declare a string */
    char s1[] = "hello";
    char s2[] = {'h', 'e', 'l', 'l', 'o', '\0'};
    char s3[6] = {'h', 'e', 'l', 'l', 'o', '\0'};

    printf("=== sizeof vs strlen ===\n");
    printf("s1: sizeof = %zu, strlen = %zu\n", sizeof(s1), strlen(s1));
    printf("s2: sizeof = %zu, strlen = %zu\n", sizeof(s2), strlen(s2));
    printf("s3: sizeof = %zu, strlen = %zu\n", sizeof(s3), strlen(s3));

    printf("\n=== Byte-level view of s1 ===\n");
    for (size_t i = 0; i < sizeof(s1); i++) {
        printf("s1[%zu] = '%c' (decimal: %d, hex: 0x%02x)\n",
               i, s1[i] ? s1[i] : '?', s1[i], (unsigned char)s1[i]);
    }

    printf("\n=== Null terminator identity ===\n");
    printf("'\\0' == 0  : %s\n", ('\0' == 0) ? "true" : "false");
    printf("'0'  == 48 : %s\n", ('0' == 48) ? "true" : "false");

    return 0;
}
