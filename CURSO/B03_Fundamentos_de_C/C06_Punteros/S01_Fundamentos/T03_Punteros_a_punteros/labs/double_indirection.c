#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;
    int **pp = &p;

    printf("Valor de x:    %d\n", x);
    printf("Valor de *p:   %d\n", *p);
    printf("Valor de **pp: %d\n", **pp);

    printf("\n--- Direcciones ---\n");
    printf("Direccion de x  (&x):  %p\n", (void *)&x);
    printf("Valor de p      (p):   %p\n", (void *)p);
    printf("Valor de *pp    (*pp): %p\n", (void *)*pp);

    printf("\nDireccion de p  (&p):  %p\n", (void *)&p);
    printf("Valor de pp     (pp):  %p\n", (void *)pp);

    printf("\n--- Modificar x a traves de pp ---\n");
    **pp = 100;
    printf("x despues de **pp = 100: %d\n", x);

    printf("\n--- sizeof ---\n");
    printf("sizeof(x)  = %zu (int)\n", sizeof(x));
    printf("sizeof(p)  = %zu (int *)\n", sizeof(p));
    printf("sizeof(pp) = %zu (int **)\n", sizeof(pp));

    return 0;
}
