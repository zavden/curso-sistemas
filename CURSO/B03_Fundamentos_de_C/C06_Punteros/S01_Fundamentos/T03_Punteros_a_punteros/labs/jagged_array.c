#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rows = 4;

    /* Cada fila tiene un tamano diferente */
    int row_sizes[] = {2, 5, 3, 1};

    /* Paso 1: alocar el array de punteros a filas */
    int **matrix = malloc(rows * sizeof(int *));
    if (!matrix) {
        fprintf(stderr, "Error: malloc fallo para matrix\n");
        return 1;
    }

    /* Paso 2: alocar cada fila con su propio tamano */
    for (int i = 0; i < rows; i++) {
        matrix[i] = malloc(row_sizes[i] * sizeof(int));
        if (!matrix[i]) {
            fprintf(stderr, "Error: malloc fallo para fila %d\n", i);
            /* Liberar filas ya alocadas */
            for (int j = 0; j < i; j++) {
                free(matrix[j]);
            }
            free(matrix);
            return 1;
        }
    }

    /* Paso 3: llenar con valores */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < row_sizes[i]; j++) {
            matrix[i][j] = (i + 1) * 10 + j;
        }
    }

    /* Paso 4: imprimir */
    printf("--- Jagged array ---\n");
    for (int i = 0; i < rows; i++) {
        printf("fila %d (%d elementos): ", i, row_sizes[i]);
        for (int j = 0; j < row_sizes[i]; j++) {
            printf("%3d ", matrix[i][j]);
        }
        printf("\n");
    }

    /* Paso 5: imprimir direcciones para ver la estructura */
    printf("\n--- Direcciones ---\n");
    printf("matrix     = %p (el int**)\n", (void *)matrix);
    for (int i = 0; i < rows; i++) {
        printf("matrix[%d]  = %p (puntero a fila %d)\n", i, (void *)matrix[i], i);
    }

    /* Paso 6: liberar en orden inverso */
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);

    printf("\nMemoria liberada correctamente.\n");

    return 0;
}
