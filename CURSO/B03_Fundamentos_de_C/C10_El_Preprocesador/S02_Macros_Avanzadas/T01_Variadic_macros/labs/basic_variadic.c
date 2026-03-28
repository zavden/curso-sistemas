/* basic_variadic.c -- basic usage of __VA_ARGS__ */

#include <stdio.h>

#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", __VA_ARGS__)

#define PRINT_ALL(...) printf(__VA_ARGS__)

int main(void) {
    int x = 42;
    int y = 10;

    LOG("value of x = %d", x);
    LOG("x = %d, y = %d", x, y);
    LOG("sum = %d, product = %d", x + y, x * y);

    PRINT_ALL("--- PRINT_ALL demo ---\n");
    PRINT_ALL("x = %d\n", x);
    PRINT_ALL("x = %d, y = %d\n", x, y);

    return 0;
}
