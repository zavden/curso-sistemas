#include <stdio.h>

#define COLS 4

/* Option 1: array notation — requires fixed column count */
void print_bracket(int mat[][COLS], int rows) {
    printf("--- print_bracket (int mat[][%d]) ---\n", COLS);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

/* Option 2: pointer-to-array — equivalent to option 1 */
void print_ptr_array(int (*mat)[COLS], int rows) {
    printf("--- print_ptr_array (int (*mat)[%d]) ---\n", COLS);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

/* Option 3: VLA parameter (C99+) — flexible dimensions */
void print_vla(int rows, int cols, int mat[rows][cols]) {
    printf("--- print_vla (VLA %dx%d) ---\n", rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

/* Option 4: flat pointer — always works */
void print_flat(int *mat, int rows, int cols) {
    printf("--- print_flat (int *mat) ---\n");
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d", mat[i * cols + j]);
        }
        printf("\n");
    }
}

int main(void) {
    int m[3][COLS] = {
        {1,  2,  3,  4},
        {5,  6,  7,  8},
        {9, 10, 11, 12},
    };

    print_bracket(m, 3);
    printf("\n");

    print_ptr_array(m, 3);
    printf("\n");

    print_vla(3, COLS, m);
    printf("\n");

    print_flat(&m[0][0], 3, COLS);

    return 0;
}
