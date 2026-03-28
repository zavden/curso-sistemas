#include <stdio.h>

/*
 * This program demonstrates what happens when the restrict
 * contract is violated: the pointers declared as restrict
 * actually DO alias.
 *
 * Compile with -O0 and -O2 to see different behavior.
 * With -O2, the compiler reorders/optimizes based on the
 * restrict promise, producing different (wrong) results.
 */

void update(int *restrict a, int *restrict b) {
    *a = 10;
    *b = 20;
    /* With restrict, the compiler may assume a != b.
     * It can reorder or cache: it "knows" *a is still 10.
     */
    printf("*a = %d (expect 10 if no alias, 20 if aliased)\n", *a);
}

void copy_restrict(int *restrict dst, int *restrict src, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

int main(void) {
    printf("=== Test 1: update with aliased pointers ===\n");
    int x = 0;
    /* VIOLATION: passing &x for both restrict parameters */
    update(&x, &x);
    printf("Final x = %d\n\n", x);

    printf("=== Test 2: copy with non-overlapping arrays ===\n");
    int src[] = {1, 2, 3, 4, 5};
    int dst[5];
    copy_restrict(dst, src, 5);
    printf("dst: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", dst[i]);
    }
    printf("\n\n");

    printf("=== Test 3: copy with overlapping regions ===\n");
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    /* VIOLATION: dst=data+2, src=data, regions overlap */
    copy_restrict(data + 2, data, 5);
    printf("data: ");
    for (int i = 0; i < 8; i++) {
        printf("%d ", data[i]);
    }
    printf("\n");

    return 0;
}
