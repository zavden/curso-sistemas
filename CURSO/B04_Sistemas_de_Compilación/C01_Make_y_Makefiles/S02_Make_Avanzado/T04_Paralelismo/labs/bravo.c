#include "funcs.h"

int compute_bravo(void) {
    int acc = 0;
    for (volatile int i = 0; i < 5000000; i++) {
        acc += i % 11;
    }
    return acc % 100;
}
