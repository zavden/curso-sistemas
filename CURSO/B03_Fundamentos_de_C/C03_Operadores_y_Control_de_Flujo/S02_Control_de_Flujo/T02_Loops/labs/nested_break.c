#include <stdio.h>

int main(void) {
    int matrix[3][4] = {
        { 1,  2,  3,  4},
        { 5,  6, -1,  8},
        { 9, 10, 11, 12}
    };
    int rows = 3;
    int cols = 4;
    int target = -1;

    /* Attempt 1: break only exits inner loop */
    printf("--- Attempt 1: break in inner loop only ---\n");
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (matrix[i][j] == target) {
                printf("Found %d at [%d][%d], breaking inner loop\n",
                       target, i, j);
                break;
            }
        }
        printf("Outer loop: i=%d continues\n", i);
    }

    /* Attempt 2: flag to break both loops */
    printf("\n--- Attempt 2: flag to break both loops ---\n");
    int found = 0;
    for (int i = 0; i < rows && !found; i++) {
        for (int j = 0; j < cols && !found; j++) {
            if (matrix[i][j] == target) {
                printf("Found %d at [%d][%d]\n", target, i, j);
                found = 1;
            }
        }
    }
    if (!found) {
        printf("%d not found\n", target);
    }

    return 0;
}
