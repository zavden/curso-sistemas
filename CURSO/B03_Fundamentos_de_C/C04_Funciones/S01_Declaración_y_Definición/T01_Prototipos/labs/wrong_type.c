#include <stdio.h>

int add(int a, int b);

int main(void) {
    /* passing string where int expected */
    printf("add(\"hello\", 2) = %d\n", add("hello", 2));
    return 0;
}

int add(int a, int b) {
    return a + b;
}
