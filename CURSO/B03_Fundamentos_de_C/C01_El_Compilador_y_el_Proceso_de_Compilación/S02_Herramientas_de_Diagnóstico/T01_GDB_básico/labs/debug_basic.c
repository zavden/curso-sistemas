/*
 * debug_basic.c
 *
 * Program with multiple functions for practicing breakpoints,
 * step/next, and print in GDB.
 *
 * Compile: gcc -g -O0 debug_basic.c -o debug_basic
 */

#include <stdio.h>

int factorial(int n) {
    int result = 1;
    for (int i = 1; i <= n; i++) {
        result *= i;
    }
    return result;
}

int process_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

int main(void) {
    int num = 6;
    int fact = factorial(num);
    printf("factorial(%d) = %d\n", num, fact);

    int data[5] = {10, 20, 30, 40, 50};
    int total = process_array(data, 5);
    printf("sum of array = %d\n", total);

    printf("done\n");
    return 0;
}
