#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include "arena.h"

struct Record {
    int id;
    double value;
    char tag[16];
};

static double time_diff_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

int main(void) {
    const int ITERATIONS = 100;
    const int ALLOCS_PER_ITER = 1000;

    printf("=== Arena vs malloc/free performance ===\n");
    printf("Iterations: %d, allocations per iteration: %d\n",
           ITERATIONS, ALLOCS_PER_ITER);
    printf("Struct size: %zu bytes\n\n", sizeof(struct Record));

    /* --- malloc/free approach --- */
    struct timespec t_start, t_end;

    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int iter = 0; iter < ITERATIONS; iter++) {
        struct Record *records[ALLOCS_PER_ITER];

        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            records[i] = malloc(sizeof(struct Record));
            records[i]->id = i;
            records[i]->value = i * 1.5;
            snprintf(records[i]->tag, 16, "r%d", i);
        }

        /* Use data to prevent optimizer from removing allocations */
        volatile double sum = 0;
        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            sum += records[i]->value;
        }

        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            free(records[i]);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double malloc_ms = time_diff_ms(t_start, t_end);

    /* --- Arena approach --- */
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    struct Arena arena;
    arena_init(&arena, ALLOCS_PER_ITER * sizeof(struct Record));

    for (int iter = 0; iter < ITERATIONS; iter++) {
        arena_reset(&arena);

        struct Record *base = arena_alloc(
            &arena, ALLOCS_PER_ITER * sizeof(struct Record));

        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            base[i].id = i;
            base[i].value = i * 1.5;
            snprintf(base[i].tag, 16, "r%d", i);
        }

        /* Use data to prevent optimizer from removing allocations */
        volatile double sum = 0;
        for (int i = 0; i < ALLOCS_PER_ITER; i++) {
            sum += base[i].value;
        }
    }

    arena_destroy(&arena);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double arena_ms = time_diff_ms(t_start, t_end);

    /* --- Results --- */
    printf("malloc/free:  %.2f ms  (%d mallocs + %d frees per iteration)\n",
           malloc_ms, ALLOCS_PER_ITER, ALLOCS_PER_ITER);
    printf("arena:        %.2f ms  (1 alloc + reset per iteration)\n",
           arena_ms);
    printf("\n");

    if (arena_ms > 0.0) {
        printf("malloc/free is %.1fx slower than arena\n",
               malloc_ms / arena_ms);
    }

    printf("\nTotal malloc calls:   %d\n", ITERATIONS * ALLOCS_PER_ITER);
    printf("Total free calls:     %d\n", ITERATIONS * ALLOCS_PER_ITER);
    printf("Total arena mallocs:  1 (initial) + 0 (resets)\n");
    printf("Total arena frees:    1 (destroy)\n");

    return 0;
}
