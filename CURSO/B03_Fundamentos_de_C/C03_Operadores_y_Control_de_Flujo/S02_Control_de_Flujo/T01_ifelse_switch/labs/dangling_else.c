#include <stdio.h>

void test_dangling(int a, int b) {
    printf("a=%d, b=%d: ", a, b);

    // BUG: dangling else - the else binds to the NEAREST if (b > 0),
    // not to the outer if (a > 0) as the indentation suggests.
    if (a > 0)
        if (b > 0)
            printf("both positive\n");
    else
        printf("this looks like it handles a<=0, but it doesn't\n");
}

void test_fixed(int a, int b) {
    printf("a=%d, b=%d: ", a, b);

    // FIXED: braces make the intent clear
    if (a > 0) {
        if (b > 0) {
            printf("both positive\n");
        }
    } else {
        printf("a is not positive\n");
    }
}

int main(void) {
    printf("=== Dangling else (buggy) ===\n");
    test_dangling(1, 1);
    test_dangling(1, -1);
    test_dangling(-1, 1);
    test_dangling(-1, -1);

    printf("\n=== Fixed with braces ===\n");
    test_fixed(1, 1);
    test_fixed(1, -1);
    test_fixed(-1, 1);
    test_fixed(-1, -1);

    return 0;
}
