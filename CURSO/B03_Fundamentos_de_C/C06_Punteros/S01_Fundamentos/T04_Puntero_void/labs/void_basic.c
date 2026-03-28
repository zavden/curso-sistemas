#include <stdio.h>

int main(void) {
    int x = 42;
    double d = 3.14;
    char c = 'A';

    void *vp;

    /* void* can point to any type — implicit conversion */
    vp = &x;
    printf("vp points to int:    address = %p\n", vp);

    vp = &d;
    printf("vp points to double: address = %p\n", vp);

    vp = &c;
    printf("vp points to char:   address = %p\n", vp);

    /* Roundtrip: int* -> void* -> int* (implicit both ways in C) */
    int *ip = &x;
    void *generic = ip;     /* int* -> void* implicit */
    int *ip2 = generic;     /* void* -> int* implicit in C */

    printf("\nRoundtrip test:\n");
    printf("  original ip  = %p, *ip  = %d\n", (void *)ip, *ip);
    printf("  recovered ip2 = %p, *ip2 = %d\n", (void *)ip2, *ip2);
    printf("  same address? %s\n", (ip == ip2) ? "yes" : "no");

    return 0;
}
