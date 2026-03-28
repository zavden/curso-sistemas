#include <stdio.h>
#include "generated.h"
#include "funcs.h"

int main(void) {
    printf("Build tag: %s\n", BUILD_TAG);
    printf("compute_alpha() = %d\n", compute_alpha());
    return 0;
}
