#include <stdio.h>

void show_static_defaults(void) {
    static int s_int;
    static double s_double;
    static char s_char;
    static int *s_ptr;

    printf("static int:    %d\n", s_int);
    printf("static double: %f\n", s_double);
    printf("static char:   %d (ASCII)\n", s_char);
    printf("static ptr:    %p\n", (void *)s_ptr);
}

int main(void) {
    printf("=== Default values of static variables ===\n");
    show_static_defaults();
    return 0;
}
