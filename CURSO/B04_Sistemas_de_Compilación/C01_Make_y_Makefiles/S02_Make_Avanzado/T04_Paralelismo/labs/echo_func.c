#include "funcs.h"

int compute_echo(void) {
    int acc = 0;
    for (volatile int i = 0; i < 5000000; i++) {
        acc += i % 19;
    }
    return acc % 100;
}
