#include <stdio.h>

#define ROWS 3
#define COLS 4

void print_matrix(int mat[][COLS], int rows) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

int main(void) {
    int matrix[ROWS][COLS] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
    };

    printf("=== Matrix 3x4 ===\n");
    print_matrix(matrix, ROWS);

    printf("\nAccess examples:\n");
    printf("matrix[0][0] = %d\n", matrix[0][0]);
    printf("matrix[1][2] = %d\n", matrix[1][2]);
    printf("matrix[2][3] = %d\n", matrix[2][3]);

    printf("\n=== Partial initialization ===\n");
    int partial[2][3] = {
        {1},
        {4, 5},
    };
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%4d", partial[i][j]);
        }
        printf("\n");
    }

    return 0;
}
