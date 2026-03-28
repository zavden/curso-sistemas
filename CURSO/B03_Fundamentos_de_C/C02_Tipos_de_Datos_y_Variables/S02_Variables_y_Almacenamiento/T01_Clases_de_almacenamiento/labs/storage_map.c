#include <stdio.h>

int global_init = 42;
int global_no_init;
static int file_static_init = 100;
static int file_static_no_init;

void show_addresses(void) {
    int local_var = 7;
    static int local_static = 99;

    printf("=== Variable addresses ===\n\n");

    printf("-- Global (initialized) --\n");
    printf("  global_init:          %p\n", (void *)&global_init);

    printf("\n-- Global (uninitialized) --\n");
    printf("  global_no_init:       %p\n", (void *)&global_no_init);

    printf("\n-- File static (initialized) --\n");
    printf("  file_static_init:     %p\n", (void *)&file_static_init);

    printf("\n-- File static (uninitialized) --\n");
    printf("  file_static_no_init:  %p\n", (void *)&file_static_no_init);

    printf("\n-- Local static (initialized) --\n");
    printf("  local_static:         %p\n", (void *)&local_static);

    printf("\n-- Local (stack) --\n");
    printf("  local_var:            %p\n", (void *)&local_var);
}

int main(void) {
    show_addresses();
    return 0;
}
