#include <stdio.h>

/* --- Alias for primitive types --- */
typedef unsigned long long u64;
typedef double Temperature;
typedef char *String;

/* --- typedef with enum --- */
typedef enum {
    NORTH,
    SOUTH,
    EAST,
    WEST
} Direction;

/* --- typedef with struct (tag + alias) --- */
typedef struct Point {
    int x;
    int y;
} Point;

/* --- typedef with anonymous struct (no tag) --- */
typedef struct {
    float r;
    float g;
    float b;
} Color;

/* --- Helper to print direction --- */
static const char *direction_name(Direction d) {
    switch (d) {
        case NORTH: return "NORTH";
        case SOUTH: return "SOUTH";
        case EAST:  return "EAST";
        case WEST:  return "WEST";
        default:    return "UNKNOWN";
    }
}

int main(void) {
    /* Primitive aliases */
    u64 counter = 1000000ULL;
    Temperature temp = 36.6;
    String greeting = "Hello from typedef";

    printf("=== Primitive type aliases ===\n");
    printf("counter (u64):         %llu\n", counter);
    printf("temp (Temperature):    %.1f\n", temp);
    printf("greeting (String):     %s\n", greeting);
    printf("sizeof(u64):           %zu\n", sizeof(u64));
    printf("sizeof(unsigned long long): %zu\n", sizeof(unsigned long long));
    printf("\n");

    /* Enum alias */
    Direction dir = NORTH;
    printf("=== Enum typedef ===\n");
    printf("dir = %s (%d)\n", direction_name(dir), dir);
    printf("\n");

    /* Struct with tag and alias */
    Point p = {10, 20};
    struct Point p2 = {30, 40};  /* Both forms work */
    printf("=== Struct typedef (with tag) ===\n");
    printf("p  (Point):        (%d, %d)\n", p.x, p.y);
    printf("p2 (struct Point): (%d, %d)\n", p2.x, p2.y);
    printf("\n");

    /* Anonymous struct (only alias works) */
    Color red = {1.0f, 0.0f, 0.0f};
    /* struct Color c; <-- would NOT compile: no tag */
    printf("=== Struct typedef (anonymous) ===\n");
    printf("red (Color): (%.1f, %.1f, %.1f)\n", red.r, red.g, red.b);

    return 0;
}
