#include <stdio.h>
#include "arena.h"

struct Point {
    double x, y;
};

struct Player {
    char name[32];
    int health;
    int score;
    struct Point position;
};

int main(void) {
    struct Arena arena;
    arena_init(&arena, 4096);

    printf("=== Allocating different types from one arena ===\n\n");

    /* Allocate ints */
    int count = 5;
    int *scores = arena_alloc(&arena, count * sizeof(int));
    printf("After %d ints:      offset = %3zu  (each int = %zu bytes)\n",
           count, arena.offset, sizeof(int));

    for (int i = 0; i < count; i++) {
        scores[i] = (i + 1) * 100;
    }

    /* Allocate doubles */
    double *temps = arena_alloc(&arena, 3 * sizeof(double));
    printf("After 3 doubles:    offset = %3zu  (each double = %zu bytes)\n",
           arena.offset, sizeof(double));

    temps[0] = 36.5;
    temps[1] = 37.2;
    temps[2] = 38.1;

    /* Allocate structs */
    struct Player *player = arena_alloc(&arena, sizeof(struct Player));
    printf("After 1 Player:     offset = %3zu  (Player = %zu bytes)\n",
           arena.offset, sizeof(struct Player));

    snprintf(player->name, 32, "Alice");
    player->health = 100;
    player->score = 0;
    player->position = (struct Point){10.0, 20.0};

    /* Allocate strings */
    const char *src = "Hello from the arena!";
    size_t len = strlen(src) + 1;
    char *msg = arena_alloc(&arena, len);
    memcpy(msg, src, len);
    printf("After string (%2zu):  offset = %3zu\n", len, arena.offset);

    /* Allocate an array of structs */
    int n_points = 4;
    struct Point *points = arena_alloc(&arena, n_points * sizeof(struct Point));
    printf("After %d Points:    offset = %3zu\n", n_points, arena.offset);

    for (int i = 0; i < n_points; i++) {
        points[i] = (struct Point){i * 1.0, i * 2.0};
    }

    /* Print everything */
    printf("\n=== Verifying data ===\n");

    printf("scores: ");
    for (int i = 0; i < count; i++) printf("%d ", scores[i]);
    printf("\n");

    printf("temps: ");
    for (int i = 0; i < 3; i++) printf("%.1f ", temps[i]);
    printf("\n");

    printf("player: %s, hp=%d, score=%d, pos=(%.0f,%.0f)\n",
           player->name, player->health, player->score,
           player->position.x, player->position.y);

    printf("msg: %s\n", msg);

    printf("points: ");
    for (int i = 0; i < n_points; i++) {
        printf("(%.0f,%.0f) ", points[i].x, points[i].y);
    }
    printf("\n");

    printf("\nTotal used: %zu / %zu bytes (%.1f%%)\n",
           arena.offset, arena.cap,
           (double)arena.offset / arena.cap * 100.0);

    arena_destroy(&arena);
    return 0;
}
