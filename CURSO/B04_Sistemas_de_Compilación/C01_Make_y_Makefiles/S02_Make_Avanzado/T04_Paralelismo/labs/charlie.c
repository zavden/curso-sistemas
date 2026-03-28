#include "funcs.h"

int compute_charlie(void) {
    int acc = 0;
    for (volatile int i = 0; i < 5000000; i++) {
        acc += i % 13;
    }
    return acc % 100;
}
