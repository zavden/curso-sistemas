#include <stdio.h>

#define ROWS 3
#define COLS 4

int main(void) {
    int matrix[ROWS][COLS] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
    };

    printf("=== Memory addresses (row-major order) ===\n\n");
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("matrix[%d][%d] = %2d  addr: %p\n",
                   i, j, matrix[i][j], (void *)&matrix[i][j]);
        }
    }

    printf("\n=== Verifying contiguous layout ===\n");
    int *first = &matrix[0][0];
    int *last  = &matrix[ROWS - 1][COLS - 1];
    printf("First element addr: %p\n", (void *)first);
    printf("Last  element addr: %p\n", (void *)last);
    printf("Total bytes: %ld  (expected: %ld)\n",
           (long)((char *)last - (char *)first) + (long)sizeof(int),
           (long)(ROWS * COLS * (int)sizeof(int)));

    printf("\n=== Flat traversal via pointer ===\n");
    int *ptr = &matrix[0][0];
    for (int i = 0; i < ROWS * COLS; i++) {
        printf("ptr[%2d] = %2d\n", i, ptr[i]);
    }

    printf("\n=== sizeof info ===\n");
    printf("sizeof(matrix)    = %zu bytes (%d ints)\n",
           sizeof(matrix), (int)(sizeof(matrix) / sizeof(int)));
    printf("sizeof(matrix[0]) = %zu bytes (%d ints = 1 row)\n",
           sizeof(matrix[0]), (int)(sizeof(matrix[0]) / sizeof(int)));

    return 0;
}
