#include <stdio.h>

/* --- typedef examples --- */

typedef int Row[4];
typedef int (*BinaryOp)(int, int);

/* Functions used as targets for function pointers */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int main(void) {
    printf("=== complex declarations with typedef ===\n\n");

    /* Without typedef: int (*matrix)[4] */
    int data[3][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
    };

    /* With typedef: Row *matrix */
    Row *matrix = data;

    printf("Row *matrix (typedef for int (*matrix)[4]):\n");
    for (int i = 0; i < 3; i++) {
        printf("  row %d:", i);
        for (int j = 0; j < 4; j++) {
            printf(" %2d", matrix[i][j]);
        }
        printf("\n");
    }

    /* Without typedef: int (*fp)(int, int) */
    /* With typedef: BinaryOp fp */
    BinaryOp fp = add;
    printf("\nBinaryOp fp = add:\n");
    printf("  fp(10, 3) = %d\n", fp(10, 3));

    /* Without typedef: int (*ops[3])(int, int) */
    /* With typedef: BinaryOp ops[3] */
    BinaryOp ops[] = {add, sub, mul};
    const char *op_names[] = {"add", "sub", "mul"};
    int count = (int)(sizeof(ops) / sizeof(ops[0]));

    printf("\nBinaryOp ops[3] (array of 3 function pointers):\n");
    for (int i = 0; i < count; i++) {
        printf("  ops[%d](10, 3) = %2d  (%s)\n",
               i, ops[i](10, 3), op_names[i]);
    }

    /* int *(*p)[5] — pointer to array of 5 pointers to int */
    int v1 = 100, v2 = 200, v3 = 300, v4 = 400, v5 = 500;
    int *arr_of_ptrs[5] = {&v1, &v2, &v3, &v4, &v5};
    int *(*p)[5] = &arr_of_ptrs;

    printf("\nint *(*p)[5] — pointer to array of 5 pointers to int:\n");
    for (int i = 0; i < 5; i++) {
        printf("  (*p)[%d] = %p  ->  *(*p)[%d] = %d\n",
               i, (void *)(*p)[i], i, *(*p)[i]);
    }

    return 0;
}
