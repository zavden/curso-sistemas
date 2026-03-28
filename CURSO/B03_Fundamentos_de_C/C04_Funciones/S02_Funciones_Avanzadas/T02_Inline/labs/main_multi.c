#include <stdio.h>

void test_from_a(void);
void test_from_b(void);

int main(void) {
    printf("=== Inline functions from header ===\n");
    test_from_a();
    test_from_b();
    return 0;
}
