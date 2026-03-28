#include <stdio.h>

enum Direction { NORTH, SOUTH, EAST, WEST };

enum HttpStatus {
    HTTP_OK        = 200,
    HTTP_NOT_FOUND = 404,
    HTTP_ERROR     = 500,
};

enum Mixed {
    A,          // 0
    B = 10,     // 10
    C,          // 11
    D,          // 12
    E = 100,    // 100
    F,          // 101
};

int main(void) {
    printf("--- Valores automaticos ---\n");
    printf("NORTH = %d\n", NORTH);
    printf("SOUTH = %d\n", SOUTH);
    printf("EAST  = %d\n", EAST);
    printf("WEST  = %d\n", WEST);

    printf("\n--- Valores explicitos ---\n");
    printf("HTTP_OK        = %d\n", HTTP_OK);
    printf("HTTP_NOT_FOUND = %d\n", HTTP_NOT_FOUND);
    printf("HTTP_ERROR     = %d\n", HTTP_ERROR);

    printf("\n--- Valores mixtos ---\n");
    printf("A = %d\n", A);
    printf("B = %d\n", B);
    printf("C = %d\n", C);
    printf("D = %d\n", D);
    printf("E = %d\n", E);
    printf("F = %d\n", F);

    printf("\n--- sizeof ---\n");
    printf("sizeof(enum Direction)  = %zu\n", sizeof(enum Direction));
    printf("sizeof(enum HttpStatus) = %zu\n", sizeof(enum HttpStatus));
    printf("sizeof(int)             = %zu\n", sizeof(int));

    return 0;
}
