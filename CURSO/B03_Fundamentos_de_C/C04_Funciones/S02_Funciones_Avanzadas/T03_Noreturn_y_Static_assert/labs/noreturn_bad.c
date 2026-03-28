#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void maybe_exit(int code) {
    if (code > 0) {
        fprintf(stderr, "Exiting with code %d\n", code);
        exit(code);
    }
    printf("Not exiting after all...\n");
    /* If code <= 0, the function returns -- UB */
}

int main(void) {
    maybe_exit(0);
    printf("Reached after maybe_exit\n");
    return 0;
}
