#include <stdio.h>

/* describe_type: usa default para manejar tipos no listados explicitamente */
#define describe_type(x) _Generic((x), \
    int:     "integer",                 \
    double:  "floating-point",          \
    char*:   "string",                  \
    default: "unsupported type"         \
)

/* format_val: con default imprime un mensaje generico */
#define format_val(x) _Generic((x),             \
    int:     printf("int: %d\n", x),            \
    double:  printf("double: %f\n", x),         \
    char*:   printf("string: %s\n", x),         \
    default: printf("(type not supported)\n")   \
)

/* Tipos custom para demostrar default */
struct Point {
    int x, y;
};

int main(void) {
    /* describe_type con tipos soportados */
    printf("--- describe_type ---\n");
    printf("42      -> %s\n", describe_type(42));
    printf("3.14    -> %s\n", describe_type(3.14));
    printf("\"hi\"    -> %s\n", describe_type("hi"));

    /* describe_type con tipos NO listados: cae en default */
    printf("3.14f   -> %s\n", describe_type(3.14f));
    printf("42L     -> %s\n", describe_type(42L));
    printf("'A'     -> %s\n", describe_type('A'));

    struct Point pt = {1, 2};
    printf("pt      -> %s\n", describe_type(pt));

    printf("\n");

    /* format_val */
    printf("--- format_val ---\n");
    format_val(99);
    format_val(2.718);
    format_val("world");

    /* Estos caen en default */
    float f = 1.0f;
    format_val(f);

    long big = 999999L;
    format_val(big);

    return 0;
}
