/*
 * debug_watch.c
 *
 * Program with a bug: counter goes negative due to a logic
 * error in a nested loop. Good for watchpoint practice.
 *
 * The outer loop increments counter, but the inner loop
 * subtracts too much when a certain condition is met,
 * causing counter to go negative unexpectedly.
 *
 * Compile: gcc -g -O0 debug_watch.c -o debug_watch
 */

#include <stdio.h>

int main(void) {
    int counter = 0;
    int total_ops = 0;

    for (int i = 0; i < 8; i++) {
        counter += 3;
        total_ops++;

        for (int j = 0; j < i; j++) {
            if ((i + j) % 5 == 0) {
                counter -= 7;   /* bug: subtracts too much */
                total_ops++;
            }
        }
    }

    printf("counter = %d\n", counter);
    printf("total_ops = %d\n", total_ops);

    if (counter < 0) {
        printf("ERROR: counter went negative!\n");
    }

    return 0;
}
