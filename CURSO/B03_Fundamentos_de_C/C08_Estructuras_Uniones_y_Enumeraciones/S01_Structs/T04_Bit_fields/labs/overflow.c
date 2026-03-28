#include <stdio.h>

struct SmallField {
    unsigned int value : 3;
};

struct SignedField {
    int value : 3;
};

int main(void) {
    struct SmallField sf;

    printf("=== Rango de un campo de 3 bits unsigned ===\n");
    printf("Rango valido: 0 a %d\n\n", (1 << 3) - 1);

    printf("Asignando valores dentro del rango:\n");
    for (unsigned int i = 0; i <= 7; i++) {
        sf.value = i;
        printf("  asignado=%u  leido=%u\n", i, sf.value);
    }

    printf("\nAsignando valores fuera del rango:\n");
    sf.value = 8;
    printf("  asignado=8   leido=%u\n", sf.value);

    sf.value = 15;
    printf("  asignado=15  leido=%u\n", sf.value);

    sf.value = 255;
    printf("  asignado=255 leido=%u\n", sf.value);

    printf("\n=== Signed bit field de 3 bits ===\n");
    struct SignedField sig;

    sig.value = 3;
    printf("  asignado=3   leido=%d\n", sig.value);

    sig.value = 4;
    printf("  asignado=4   leido=%d\n", sig.value);

    sig.value = 5;
    printf("  asignado=5   leido=%d\n", sig.value);

    sig.value = 7;
    printf("  asignado=7   leido=%d\n", sig.value);

    return 0;
}
