// heap_map.c — show where heap, stack, and globals live in memory
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int global_var = 100;               // .data
int global_zero;                    // .bss (zero-initialized)
const char *literal = "hello";      // .rodata (the string)

int main(void) {
    int stack_var = 42;             // stack
    (void)stack_var;

    int *heap_var = malloc(sizeof(*heap_var));
    if (heap_var == NULL) {
        perror("malloc");
        return 1;
    }
    *heap_var = 99;

    printf("=== Memory addresses ===\n");
    printf(".rodata (literal):   %p\n", (void *)literal);
    printf(".data  (global_var): %p\n", (void *)&global_var);
    printf(".bss   (global_zero):%p\n", (void *)&global_zero);
    printf("Heap   (heap_var):   %p\n", (void *)heap_var);
    printf("Stack  (stack_var):  %p\n", (void *)&stack_var);

    printf("\n=== /proc/self/maps (filtered) ===\n");
    printf("Open another terminal and run:\n");
    printf("  cat /proc/%d/maps\n", getpid());
    printf("\nOr press Enter to print the heap and stack regions...\n");
    getchar();

    // Print maps filtered for [heap] and [stack]
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "[heap]") || strstr(line, "[stack]")) {
                printf("%s", line);
            }
        }
        fclose(fp);
    }

    free(heap_var);
    heap_var = NULL;

    return 0;
}
