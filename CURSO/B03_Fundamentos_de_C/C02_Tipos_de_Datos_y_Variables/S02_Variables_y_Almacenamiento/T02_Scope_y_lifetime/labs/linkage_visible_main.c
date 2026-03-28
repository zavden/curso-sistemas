#include <stdio.h>
#include "linkage_visible.h"

int main(void) {
    printf("From main: public_var = %d\n", public_var);
    public_func();
    call_private();

    return 0;
}
