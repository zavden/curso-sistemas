#include <stdio.h>

int main(void) {
    int score = 75;

    // Simple if/else
    if (score >= 60) {
        printf("Passed\n");
    } else {
        printf("Failed\n");
    }

    // Chained if/else if/else
    if (score >= 90) {
        printf("Grade: A\n");
    } else if (score >= 80) {
        printf("Grade: B\n");
    } else if (score >= 70) {
        printf("Grade: C\n");
    } else if (score >= 60) {
        printf("Grade: D\n");
    } else {
        printf("Grade: F\n");
    }

    // Truthiness in C: non-zero = true, zero = false
    int values[] = {42, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        if (values[i]) {
            printf("%d is true\n", values[i]);
        } else {
            printf("%d is false\n", values[i]);
        }
    }

    return 0;
}
