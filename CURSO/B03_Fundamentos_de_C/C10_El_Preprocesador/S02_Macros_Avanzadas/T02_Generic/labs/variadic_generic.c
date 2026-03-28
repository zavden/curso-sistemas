#include <stdio.h>

/* print_one: imprime un valor con el formato correcto segun su tipo */
#define print_one(x) _Generic((x), \
    int:    printf("%d", x),       \
    double: printf("%f", x),       \
    char*:  printf("\"%s\"", x),   \
    long:   printf("%ld", x),      \
    float:  printf("%f", x)        \
)

/* PRINT_VALS: combina _Generic con variadic macros.
   Usa un truco de conteo de argumentos para seleccionar
   la version correcta del macro.

   El macro NARGS cuenta cuantos argumentos recibe (2 o 3).
   Luego PRINT_VALS_N despacha al macro correcto. */
#define NARGS_IMPL(_1, _2, _3, N, ...) N
#define NARGS(...) NARGS_IMPL(__VA_ARGS__, 3, 2, 1)
#define CONCAT(a, b) a##b
#define DISPATCH(name, n) CONCAT(name, n)
#define PRINT_VALS(...) DISPATCH(PRINT_VALS_, NARGS(__VA_ARGS__))(__VA_ARGS__)

#define PRINT_VALS_2(a, b) \
    do { print_one(a); printf(", "); print_one(b); printf("\n"); } while(0)

#define PRINT_VALS_3(a, b, c) \
    do { print_one(a); printf(", "); print_one(b); printf(", "); print_one(c); printf("\n"); } while(0)

int main(void) {
    printf("--- PRINT_VALS con 2 argumentos ---\n");
    PRINT_VALS(42, "hello");
    PRINT_VALS(3.14, 100);
    PRINT_VALS("name", 100);

    printf("\n--- PRINT_VALS con 3 argumentos ---\n");
    PRINT_VALS(42, 3.14, "world");
    PRINT_VALS(100L, 2.5f, 99);

    printf("\n--- Con variables ---\n");
    int age = 25;
    char *city = "Madrid";
    double temp = 21.5;
    PRINT_VALS(age, city);
    PRINT_VALS(age, city, temp);

    return 0;
}
