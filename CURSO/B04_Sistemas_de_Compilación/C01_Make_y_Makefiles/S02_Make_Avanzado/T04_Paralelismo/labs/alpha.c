#include "funcs.h"

int compute_alpha(void) {
    int acc = 0;
    for (volatile int i = 0; i < 5000000; i++) {
        acc += i % 7;
    }
    return acc % 100;
}
