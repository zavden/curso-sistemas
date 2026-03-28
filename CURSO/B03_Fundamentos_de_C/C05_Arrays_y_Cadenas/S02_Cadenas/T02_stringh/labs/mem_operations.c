#include <stdio.h>
#include <string.h>

static void print_int_array(const char *label, const int *arr, size_t count) {
    printf("%s: {", label);
    for (size_t i = 0; i < count; i++) {
        printf("%d", arr[i]);
        if (i < count - 1) printf(", ");
    }
    printf("}\n");
}

int main(void) {
    /* --- memcpy --- */
    printf("=== memcpy ===\n");
    int src[5] = {10, 20, 30, 40, 50};
    int dst[5];

    memcpy(dst, src, sizeof(src));
    print_int_array("src", src, 5);
    print_int_array("dst", dst, 5);

    /* memcpy works with any type, not just strings */
    char str_src[] = "hello";
    char str_dst[10];
    memcpy(str_dst, str_src, strlen(str_src) + 1);
    printf("memcpy string: \"%s\"\n", str_dst);

    /* --- memset --- */
    printf("\n=== memset ===\n");
    char buf[20];
    memset(buf, 'A', 10);
    buf[10] = '\0';
    printf("memset(buf, 'A', 10) -> \"%s\"\n", buf);

    /* Zero out an array */
    int zeros[5];
    memset(zeros, 0, sizeof(zeros));
    print_int_array("memset to 0", zeros, 5);

    /* TRAP: memset with non-zero on ints */
    int ones[3];
    memset(ones, 1, sizeof(ones));
    print_int_array("memset(arr, 1, ...) NOT {1,1,1}", ones, 3);
    printf("Each int = 0x%08x = %d (each BYTE is 0x01)\n",
           ones[0], ones[0]);

    /* --- memmove (handles overlap) --- */
    printf("\n=== memmove ===\n");
    char overlap[] = "hello world";
    printf("Before memmove: \"%s\"\n", overlap);

    memmove(overlap + 2, overlap, 5);
    printf("After memmove(buf+2, buf, 5): \"%s\"\n", overlap);

    /* Reset and show memcpy vs memmove */
    char safe_buf[] = "abcdefghij";
    printf("\nBefore memmove: \"%s\"\n", safe_buf);
    memmove(safe_buf + 3, safe_buf, 5);
    printf("memmove(buf+3, buf, 5): \"%s\"\n", safe_buf);

    /* --- memcmp --- */
    printf("\n=== memcmp ===\n");
    int arr_a[3] = {1, 2, 3};
    int arr_b[3] = {1, 2, 3};
    int arr_c[3] = {1, 2, 4};

    printf("memcmp(a, b) = %d (equal)\n", memcmp(arr_a, arr_b, sizeof(arr_a)));
    int cmp = memcmp(arr_a, arr_c, sizeof(arr_a));
    printf("memcmp(a, c) = %d (%s)\n", cmp, (cmp < 0) ? "a < c" : "a > c");

    return 0;
}
