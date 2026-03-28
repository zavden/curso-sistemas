#include <stdio.h>
#include "arena.h"

struct Point {
    double x, y;
};

int main(void) {
    struct Arena arena;

    if (arena_init(&arena, 4096) != 0) {
        fprintf(stderr, "Error: arena_init failed\n");
        return 1;
    }

    printf("Arena created: cap = %zu bytes, offset = %zu\n",
           arena.cap, arena.offset);

    /* Allocate an array of 10 ints */
    int *nums = arena_alloc(&arena, 10 * sizeof(int));
    if (!nums) {
        fprintf(stderr, "Error: arena_alloc failed for nums\n");
        arena_destroy(&arena);
        return 1;
    }
    printf("After allocating 10 ints: offset = %zu\n", arena.offset);

    for (int i = 0; i < 10; i++) {
        nums[i] = i * 10;
    }

    /* Allocate two Points */
    struct Point *p1 = arena_alloc(&arena, sizeof(struct Point));
    struct Point *p2 = arena_alloc(&arena, sizeof(struct Point));
    printf("After allocating 2 Points: offset = %zu\n", arena.offset);

    *p1 = (struct Point){1.5, 2.5};
    *p2 = (struct Point){3.0, 4.0};

    /* Allocate a string */
    char *name = arena_alloc(&arena, 64);
    snprintf(name, 64, "arena test string");
    printf("After allocating string: offset = %zu\n", arena.offset);

    /* Verify all data */
    printf("\nnums: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", nums[i]);
    }
    printf("\n");
    printf("p1 = (%.1f, %.1f)\n", p1->x, p1->y);
    printf("p2 = (%.1f, %.1f)\n", p2->x, p2->y);
    printf("name = %s\n", name);
    printf("\nTotal used: %zu / %zu bytes\n", arena.offset, arena.cap);

    arena_destroy(&arena);
    printf("Arena destroyed.\n");
    return 0;
}
