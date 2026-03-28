#include <stdio.h>

int global_counter = 42;
static int internal_counter = 10;

void public_function(void) {
    printf("public_function: global_counter = %d\n", global_counter);
}

static void helper_function(void) {
    printf("helper_function: internal_counter = %d\n", internal_counter);
}

int main(void) {
    public_function();
    helper_function();
    return 0;
}
