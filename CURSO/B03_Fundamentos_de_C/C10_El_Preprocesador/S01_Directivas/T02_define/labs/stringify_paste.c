#include <stdio.h>

/* --- Stringizing (#) --- */
#define STRINGIFY(x)    #x
#define XSTRINGIFY(x)   STRINGIFY(x)

#define APP_VERSION 42

/* --- Token pasting (##) --- */
#define CONCAT(a, b)    a##b
#define MAKE_VAR(n)     var_##n

/* --- Practical use: PRINT_VAR --- */
#define PRINT_INT(var)  printf("%s = %d\n", #var, (var))

/* --- X-macros: enum + string table --- */
#define COLORS(X) \
    X(RED)        \
    X(GREEN)      \
    X(BLUE)       \
    X(YELLOW)

#define AS_ENUM(name)   name,
#define AS_STRING(name) #name,

enum Color { COLORS(AS_ENUM) COLOR_COUNT };
static const char *color_names[] = { COLORS(AS_STRING) };

int main(void)
{
    printf("=== Stringizing (#) ===\n");
    printf("STRINGIFY(hello)       = %s\n", STRINGIFY(hello));
    printf("STRINGIFY(1 + 2)       = %s\n", STRINGIFY(1 + 2));
    printf("STRINGIFY(foo bar)     = %s\n", STRINGIFY(foo bar));

    printf("\n=== STRINGIFY vs XSTRINGIFY ===\n");
    printf("STRINGIFY(APP_VERSION)  = %s  (not expanded)\n",
           STRINGIFY(APP_VERSION));
    printf("XSTRINGIFY(APP_VERSION) = %s  (expanded first)\n",
           XSTRINGIFY(APP_VERSION));

    printf("\n=== Token pasting (##) ===\n");
    int CONCAT(my, Var) = 100;
    printf("myVar = %d\n", myVar);

    int MAKE_VAR(1) = 10;
    int MAKE_VAR(2) = 20;
    printf("var_1 = %d\n", var_1);
    printf("var_2 = %d\n", var_2);

    printf("\n=== PRINT_INT (# in practice) ===\n");
    int count = 7;
    int total = 42;
    PRINT_INT(count);
    PRINT_INT(total);
    PRINT_INT(count + total);

    printf("\n=== X-macros: enum + string table ===\n");
    for (int i = 0; i < COLOR_COUNT; i++) {
        printf("  color_names[%d] = %s\n", i, color_names[i]);
    }

    return 0;
}
