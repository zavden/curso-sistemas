#include "funcs.h"

int compute_juliet(void) {
    int acc = 0;
    for (volatile int i = 0; i < 5000000; i++) {
        acc += i % 41;
    }
    return acc % 100;
}
