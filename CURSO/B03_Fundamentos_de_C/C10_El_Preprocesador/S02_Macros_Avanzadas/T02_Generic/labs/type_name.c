#include <stdio.h>
#include <stdbool.h>

/* type_name: retorna el nombre del tipo como string */
#define type_name(x) _Generic((x), \
    _Bool:              "bool",     \
    char:               "char",     \
    signed char:        "signed char", \
    unsigned char:      "unsigned char", \
    short:              "short",    \
    unsigned short:     "unsigned short", \
    int:                "int",      \
    unsigned int:       "unsigned int", \
    long:               "long",     \
    unsigned long:      "unsigned long", \
    long long:          "long long", \
    unsigned long long: "unsigned long long", \
    float:              "float",    \
    double:             "double",   \
    long double:        "long double", \
    char*:              "char*",    \
    void*:              "void*",    \
    default:            "unknown"   \
)

/* PRINT_VAR: combina stringify (#) con _Generic para imprimir
   "nombre = valor" con el formato correcto */
#define PRINT_VAR(var) _Generic((var), \
    int:                printf(#var " = %d\n",    var), \
    unsigned int:       printf(#var " = %u\n",    var), \
    long:               printf(#var " = %ld\n",   var), \
    unsigned long:      printf(#var " = %lu\n",   var), \
    float:              printf(#var " = %f\n",    var), \
    double:             printf(#var " = %f\n",    var), \
    char:               printf(#var " = '%c'\n",  var), \
    char*:              printf(#var " = \"%s\"\n", var), \
    default:            printf(#var " = (unknown type)\n") \
)

int main(void) {
    /* Parte A: type_name */
    printf("--- type_name ---\n");
    printf("42        -> %s\n", type_name(42));
    printf("3.14f     -> %s\n", type_name(3.14f));
    printf("3.14      -> %s\n", type_name(3.14));
    printf("'A'       -> %s\n", type_name('A'));
    printf("\"hello\"   -> %s\n", type_name("hello"));
    printf("(bool)1   -> %s\n", type_name((bool)1));
    printf("42UL      -> %s\n", type_name(42UL));
    printf("(void*)0  -> %s\n", type_name((void*)0));

    printf("\n");

    /* Parte B: PRINT_VAR */
    printf("--- PRINT_VAR ---\n");
    int count = 42;
    double pi = 3.14;
    char *greeting = "hello";
    char letter = 'Z';
    unsigned int flags = 0xFF;

    PRINT_VAR(count);
    PRINT_VAR(pi);
    PRINT_VAR(greeting);
    PRINT_VAR(letter);
    PRINT_VAR(flags);

    return 0;
}
