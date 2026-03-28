#include <stdio.h>
#include <math.h>

int main(void) {
    /* Trap 1: = vs == */
    printf("--- Trampa 1: = vs == ---\n\n");
    int x = 0;
    if (x = 5) {
        printf("if (x = 5) -> entro (x ahora vale %d)\n", x);
    }
    printf("Esto fue ASIGNACION, no comparacion.\n");
    printf("x = 5 asigna 5, luego evalua 5 -> true.\n");

    /* Trap 2: chained comparisons */
    printf("\n--- Trampa 2: comparaciones encadenadas ---\n\n");
    int val = 100;
    printf("val = %d\n", val);
    printf("(1 < val < 10) -> %d\n", (1 < val < 10));
    printf("Parece correcto, pero 100 NO esta entre 1 y 10.\n");
    printf("Se parsea como: (1 < 100) < 10  ->  1 < 10  ->  1 (true!)\n");
    printf("\nForma correcta: (1 < val && val < 10) -> %d\n",
           (1 < val && val < 10));

    /* Trap 3: float comparison */
    printf("\n--- Trampa 3: comparar floats con == ---\n\n");
    double result = 0.1 + 0.2;
    printf("0.1 + 0.2 = %.20f\n", result);
    printf("0.3       = %.20f\n", 0.3);
    printf("(0.1 + 0.2 == 0.3) -> %d\n", (0.1 + 0.2 == 0.3));
    printf("\nCon epsilon: fabs(0.1 + 0.2 - 0.3) < 1e-9 -> %d\n",
           fabs(0.1 + 0.2 - 0.3) < 1e-9);

    /* Trap 4: signed vs unsigned */
    printf("\n--- Trampa 4: signed vs unsigned ---\n\n");
    int s = -1;
    unsigned int u = 0;
    printf("int s = -1, unsigned int u = 0\n");
    printf("(s < u) -> %d\n", s < u);
    printf("-1 se convierte a unsigned: %u\n", (unsigned int)s);
    printf("Ese valor es UINT_MAX, que es mayor que 0.\n");

    return 0;
}
