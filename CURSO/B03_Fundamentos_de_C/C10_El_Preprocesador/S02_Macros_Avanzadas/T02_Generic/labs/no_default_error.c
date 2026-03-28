#include <stdio.h>

/* print_num: sin clausula default, solo soporta int y double */
#define print_num(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x)      \
)

int main(void) {
    /* Estos funcionan: */
    print_num(42);
    print_num(3.14);

    /* Este causa error de compilacion: float no esta en la lista
       y no hay default */
    float f = 1.0f;
    print_num(f);

    return 0;
}
