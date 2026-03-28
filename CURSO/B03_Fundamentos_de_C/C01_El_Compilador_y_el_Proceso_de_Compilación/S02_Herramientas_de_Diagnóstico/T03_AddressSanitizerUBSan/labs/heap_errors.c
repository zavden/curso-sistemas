/*
 * heap_errors.c -- Heap memory errors for ASan detection.
 *
 * Three distinct bugs, each in its own function.
 * Call one at a time by changing the argument to main
 * or by uncommenting the desired call.
 *
 * Compile: gcc -fsanitize=address -g heap_errors.c -o heap_errors
 * Run:     ./heap_errors 1   (heap-buffer-overflow)
 *          ./heap_errors 2   (use-after-free)
 *          ./heap_errors 3   (double-free)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bug 1: write past the end of a heap-allocated array. */
void heap_buffer_overflow(void) {
    int *arr = malloc(5 * sizeof(int));
    if (!arr) return;

    for (int i = 0; i <= 5; i++) {   /* i == 5 is 1 past the end */
        arr[i] = i * 10;
    }

    printf("arr[0] = %d\n", arr[0]);
    free(arr);
}

/* Bug 2: read from a pointer after its memory has been freed. */
void use_after_free(void) {
    int *p = malloc(sizeof(int));
    if (!p) return;

    *p = 42;
    printf("before free: %d\n", *p);

    free(p);

    /* Accessing freed memory -- undefined behavior. */
    printf("after free: %d\n", *p);
}

/* Bug 3: free the same pointer twice. */
void double_free(void) {
    char *buf = malloc(64);
    if (!buf) return;

    strcpy(buf, "hello");
    printf("buf = %s\n", buf);

    free(buf);
    free(buf);   /* second free on the same pointer */
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <1|2|3>\n", argv[0]);
        fprintf(stderr, "  1 = heap-buffer-overflow\n");
        fprintf(stderr, "  2 = use-after-free\n");
        fprintf(stderr, "  3 = double-free\n");
        return 1;
    }

    switch (argv[1][0]) {
        case '1': heap_buffer_overflow(); break;
        case '2': use_after_free();       break;
        case '3': double_free();          break;
        default:
            fprintf(stderr, "Invalid option: %s\n", argv[1]);
            return 1;
    }

    return 0;
}
