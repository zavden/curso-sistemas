/*
 * debug_crash.c
 *
 * Program that crashes due to NULL pointer dereference.
 * Chain: main() -> caller() -> process() -> access_data()
 * Good for backtrace practice.
 *
 * Compile: gcc -g -O0 debug_crash.c -o debug_crash
 */

#include <stdio.h>
#include <stdlib.h>

int access_data(int *p) {
    int value = *p;    /* crashes here if p is NULL */
    return value * 2;
}

int process(int *data) {
    int result = access_data(data);
    return result + 1;
}

void caller(void) {
    int *ptr = NULL;
    int answer = process(ptr);
    printf("answer = %d\n", answer);
}

int main(void) {
    printf("starting program...\n");
    caller();
    printf("done\n");
    return 0;
}
