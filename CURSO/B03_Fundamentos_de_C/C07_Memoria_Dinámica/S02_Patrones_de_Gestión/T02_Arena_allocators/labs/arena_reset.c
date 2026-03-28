#include <stdio.h>
#include "arena.h"

struct Point {
    double x, y;
};

int main(void) {
    struct Arena arena;
    arena_init(&arena, 1024);

    printf("=== Arena with reset (simulating frame allocator) ===\n\n");

    int n_frames = 3;
    int points_per_frame = 5;

    for (int frame = 0; frame < n_frames; frame++) {
        arena_reset(&arena);
        printf("--- Frame %d (offset after reset: %zu) ---\n",
               frame, arena.offset);

        /* Allocate temporary data for this "frame" */
        struct Point *pts = arena_alloc(
            &arena, points_per_frame * sizeof(struct Point));

        char *label = arena_alloc(&arena, 32);
        snprintf(label, 32, "frame_%d_data", frame);

        int *ids = arena_alloc(&arena, points_per_frame * sizeof(int));

        /* Fill data */
        for (int i = 0; i < points_per_frame; i++) {
            pts[i] = (struct Point){
                frame * 10.0 + i,
                frame * 100.0 + i
            };
            ids[i] = frame * 1000 + i;
        }

        /* Use data */
        printf("  label: %s\n", label);
        printf("  points: ");
        for (int i = 0; i < points_per_frame; i++) {
            printf("(%.0f,%.0f) ", pts[i].x, pts[i].y);
        }
        printf("\n");
        printf("  ids: ");
        for (int i = 0; i < points_per_frame; i++) {
            printf("%d ", ids[i]);
        }
        printf("\n");
        printf("  offset at end of frame: %zu / %zu bytes\n",
               arena.offset, arena.cap);
    }

    printf("\n=== Summary ===\n");
    printf("Processed %d frames, each reusing the same %zu-byte arena.\n",
           n_frames, arena.cap);
    printf("Without reset: would need %d x %zu = %zu bytes total.\n",
           n_frames,
           (size_t)(points_per_frame * sizeof(struct Point) + 32 +
                    points_per_frame * sizeof(int)),
           (size_t)(n_frames * (points_per_frame * sizeof(struct Point) + 32 +
                                points_per_frame * sizeof(int))));
    printf("With reset: only %zu bytes needed.\n", arena.cap);

    arena_destroy(&arena);
    printf("Arena destroyed. Single free for all frames.\n");
    return 0;
}
