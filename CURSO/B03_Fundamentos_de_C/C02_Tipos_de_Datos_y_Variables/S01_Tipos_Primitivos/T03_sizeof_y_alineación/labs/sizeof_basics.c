#include <stdio.h>
#include <stdalign.h>

int main(void) {
    printf("=== sizeof de tipos primitivos ===\n");
    printf("char:        %zu bytes\n", sizeof(char));
    printf("short:       %zu bytes\n", sizeof(short));
    printf("int:         %zu bytes\n", sizeof(int));
    printf("long:        %zu bytes\n", sizeof(long));
    printf("long long:   %zu bytes\n", sizeof(long long));
    printf("float:       %zu bytes\n", sizeof(float));
    printf("double:      %zu bytes\n", sizeof(double));
    printf("long double: %zu bytes\n", sizeof(long double));
    printf("char*:       %zu bytes\n", sizeof(char *));
    printf("int*:        %zu bytes\n", sizeof(int *));
    printf("void*:       %zu bytes\n", sizeof(void *));

    printf("\n=== sizeof de arrays ===\n");
    int arr[10];
    double darr[5];
    char str[] = "hello";

    printf("int arr[10]:    %zu bytes (10 * %zu)\n",
           sizeof(arr), sizeof(arr[0]));
    printf("double darr[5]: %zu bytes (5 * %zu)\n",
           sizeof(darr), sizeof(darr[0]));
    printf("char str[]:     %zu bytes (incluye '\\0')\n", sizeof(str));

    printf("\n=== Conteo de elementos ===\n");
    printf("arr elements:  %zu\n", sizeof(arr) / sizeof(arr[0]));
    printf("darr elements: %zu\n", sizeof(darr) / sizeof(darr[0]));

    printf("\n=== alignof de tipos ===\n");
    printf("alignof(char):        %zu\n", alignof(char));
    printf("alignof(short):       %zu\n", alignof(short));
    printf("alignof(int):         %zu\n", alignof(int));
    printf("alignof(long):        %zu\n", alignof(long));
    printf("alignof(long long):   %zu\n", alignof(long long));
    printf("alignof(float):       %zu\n", alignof(float));
    printf("alignof(double):      %zu\n", alignof(double));
    printf("alignof(long double): %zu\n", alignof(long double));

    return 0;
}
