#include <stdio.h>

#define ROWS 3
#define COLS 4

void print_row(int (*row)[COLS]) {
    for (int j = 0; j < COLS; j++) {
        printf("%4d", (*row)[j]);
    }
    printf("\n");
}

void print_matrix(int (*mat)[COLS], int rows) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

int sum_matrix(int (*mat)[COLS], int rows) {
    int total = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            total += mat[i][j];
        }
    }
    return total;
}

int main(void) {
    int m[ROWS][COLS] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
    };

    printf("=== pointer to array for 2D matrices ===\n\n");

    printf("Full matrix (via print_matrix):\n");
    print_matrix(m, ROWS);

    printf("\nRow by row (via print_row):\n");
    for (int i = 0; i < ROWS; i++) {
        printf("  row %d: ", i);
        print_row(&m[i]);
    }

    printf("\n=== pointer arithmetic on int (*p)[%d] ===\n\n", COLS);

    int (*p)[COLS] = m;
    for (int i = 0; i < ROWS; i++) {
        printf("p + %d = %p  (row %d)\n", i, (void *)(p + i), i);
    }
    printf("\nEach step = %zu bytes (sizeof(int[%d]))\n",
           sizeof(int[COLS]), COLS);

    printf("\nSum of all elements = %d\n", sum_matrix(m, ROWS));

    return 0;
}
