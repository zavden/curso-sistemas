#include <stdio.h>
#include <stdarg.h>

int sum_until(int first, ...) {
    if (first == -1) {
        return 0;
    }

    va_list args;
    va_start(args, first);

    int total = first;
    int next;
    while ((next = va_arg(args, int)) != -1) {
        total += next;
    }

    va_end(args);
    return total;
}

void print_strings(const char *first, ...) {
    va_list args;
    va_start(args, first);

    const char *s = first;
    while (s != NULL) {
        printf("%s ", s);
        s = va_arg(args, const char *);
    }
    printf("\n");

    va_end(args);
}

int main(void) {
    printf("--- sum_until (sentinel = -1) ---\n");
    printf("sum_until(10, 20, 30, -1) = %d\n", sum_until(10, 20, 30, -1));
    printf("sum_until(5, -1) = %d\n", sum_until(5, -1));
    printf("sum_until(-1) = %d\n", sum_until(-1));

    printf("\n--- print_strings (sentinel = NULL) ---\n");
    print_strings("hello", "world", "foo", NULL);
    print_strings("one", NULL);
    print_strings("a", "b", "c", "d", NULL);

    return 0;
}
