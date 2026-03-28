#include <stdio.h>

struct Flags {
    unsigned int active  : 1;
    unsigned int visible : 1;
    unsigned int locked  : 1;
};

struct Aligned {
    unsigned int a : 3;
    unsigned int   : 0;
    unsigned int b : 5;
};

struct NoZero {
    unsigned int a : 3;
    unsigned int b : 5;
};

int main(void) {
    printf("=== No se puede tomar la direccion ===\n");

    struct Flags f = { .active = 1, .visible = 0, .locked = 1 };

    /* Esto NO compila — descomentar para verificar:
     * int *p = &f.active;
     * scanf("%d", &f.active);
     */

    printf("Intentar &f.active no compila.\n");
    printf("Workaround: usar variable temporal.\n\n");

    int temp = 1;
    f.visible = temp;
    printf("f.visible = %u (via variable temporal)\n\n", f.visible);

    printf("=== Efecto de campo de ancho 0 ===\n");
    printf("sizeof(struct NoZero)  = %zu  (a y b en el mismo int)\n",
           sizeof(struct NoZero));
    printf("sizeof(struct Aligned) = %zu  (a en un int, b en otro)\n\n",
           sizeof(struct Aligned));

    printf("=== Portabilidad: el layout NO esta garantizado ===\n");
    printf("El orden de bits (MSB/LSB) depende del compilador y la arquitectura.\n");
    printf("No usar bit fields para protocolos de red o archivos binarios.\n");

    return 0;
}
