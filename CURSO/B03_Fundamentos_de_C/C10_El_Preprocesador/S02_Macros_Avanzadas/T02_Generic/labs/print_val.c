#include <stdio.h>

#define print_val(x) _Generic((x), \
    int:    printf("%d\n", x),     \
    double: printf("%f\n", x),     \
    char:   printf("'%c'\n", x),   \
    char*:  printf("%s\n", x)      \
)

int main(void) {
    int age = 30;
    double pi = 3.14159;
    char grade = 'A';
    char *name = "Alice";

    printf("int:    ");
    print_val(age);

    printf("double: ");
    print_val(pi);

    printf("char:   ");
    print_val(grade);

    printf("char*:  ");
    print_val(name);

    /* Literal entero: el compilador lo trata como int */
    printf("literal 42: ");
    print_val(42);

    /* Literal flotante: el compilador lo trata como double (no float) */
    printf("literal 2.71: ");
    print_val(2.71);

    return 0;
}
