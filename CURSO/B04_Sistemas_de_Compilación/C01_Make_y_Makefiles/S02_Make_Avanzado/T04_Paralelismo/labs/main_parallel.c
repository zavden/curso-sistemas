#include <stdio.h>
#include "funcs.h"

int main(void) {
    int total = 0;

    total += compute_alpha();
    total += compute_bravo();
    total += compute_charlie();
    total += compute_delta();
    total += compute_echo();
    total += compute_foxtrot();
    total += compute_golf();
    total += compute_hotel();
    total += compute_india();
    total += compute_juliet();

    printf("Total: %d\n", total);
    return 0;
}
