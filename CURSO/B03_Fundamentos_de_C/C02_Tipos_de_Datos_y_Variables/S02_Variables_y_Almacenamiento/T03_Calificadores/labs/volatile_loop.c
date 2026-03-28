#include <stdio.h>

int normal_flag = 1;
volatile int volatile_flag = 1;

void loop_normal(void) {
    while (normal_flag) {
        /* Without volatile, the compiler may optimize this
           to an infinite loop that never re-reads normal_flag */
    }
}

void loop_volatile(void) {
    while (volatile_flag) {
        /* With volatile, the compiler must re-read volatile_flag
           from memory on every iteration */
    }
}

int main(void) {
    printf("This program is for assembly inspection only.\n");
    printf("Do NOT run it — the loops are infinite.\n");
    printf("Compile with: gcc -S -O2 volatile_loop.c\n");
    printf("Then compare loop_normal vs loop_volatile in the .s file.\n");
    return 0;
}
