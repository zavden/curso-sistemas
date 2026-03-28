#include <stdio.h>

int global_int;
static double static_double;
int global_array[5];
static int *static_ptr;

int main(void) {
    int local_int;
    static int static_local;
    int local_array[5];
    double local_double;
    int *local_ptr;

    printf("=== Variables con static lifetime (deben ser 0) ===\n");
    printf("global_int:    %d\n", global_int);
    printf("static_double: %f\n", static_double);
    printf("static_local:  %d\n", static_local);
    printf("static_ptr:    %p\n", (void *)static_ptr);
    printf("global_array:  ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", global_array[i]);
    }
    printf("\n");

    printf("\n=== Variables locales (valor indeterminado) ===\n");
    printf("NOTA: estos valores son basura del stack.\n");
    printf("Cada ejecucion puede mostrar valores distintos.\n");

    /* Lectura de variables no inicializadas: comportamiento indefinido.
       Lo hacemos aqui solo para demostrar el concepto.
       En codigo real, NUNCA leer sin inicializar. */
    printf("local_int:     %d\n", local_int);
    printf("local_double:  %f\n", local_double);
    printf("local_ptr:     %p\n", (void *)local_ptr);
    printf("local_array:   ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", local_array[i]);
    }
    printf("\n");

    return 0;
}
