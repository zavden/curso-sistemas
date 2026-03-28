#include <stdio.h>

int normal_var = 100;
volatile int volatile_var = 100;

int read_normal(void) {
    int a = normal_var;
    int b = normal_var;
    return a + b;
}

int read_volatile(void) {
    int a = volatile_var;
    int b = volatile_var;
    return a + b;
}

int main(void) {
    printf("read_normal()   = %d\n", read_normal());
    printf("read_volatile() = %d\n", read_volatile());
    return 0;
}
