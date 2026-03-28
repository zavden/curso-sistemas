#include <stdio.h>
#include <stdarg.h>

void my_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putchar(*p);
            continue;
        }
        p++;
        switch (*p) {
            case 'd': {
                int val = va_arg(args, int);
                printf("%d", val);
                break;
            }
            case 's': {
                const char *val = va_arg(args, const char *);
                printf("%s", val);
                break;
            }
            case 'c': {
                int val = va_arg(args, int);
                putchar(val);
                break;
            }
            case '%':
                putchar('%');
                break;
            default:
                putchar('%');
                putchar(*p);
                break;
        }
    }

    va_end(args);
}

int main(void) {
    my_printf("Hello %s, you are %d years old\n", "Alice", 25);
    my_printf("Letter: %c\n", 'Z');
    my_printf("100%% complete\n");
    my_printf("%s scored %d out of %d\n", "Bob", 87, 100);
    return 0;
}
