#include <stdio.h>

struct Point {
    int x;
    int y;
};

// Return struct by value (compound literal):
struct Point make_point(int x, int y) {
    return (struct Point){ .x = x, .y = y };
}

// Return struct by value (named variable):
struct Point midpoint(struct Point a, struct Point b) {
    struct Point mid = {
        .x = (a.x + b.x) / 2,
        .y = (a.y + b.y) / 2,
    };
    return mid;
}

void print_point(const char *label, struct Point p) {
    printf("%s = (%d, %d)\n", label, p.x, p.y);
}

int main(void) {
    // --- Struct as return value ---
    struct Point p = make_point(10, 20);
    print_point("p", p);

    struct Point q = make_point(30, 40);
    print_point("q", q);

    struct Point m = midpoint(p, q);
    print_point("midpoint(p, q)", m);

    // --- Compound literal passed directly ---
    print_point("literal", (struct Point){ .x = 99, .y = -1 });

    // --- Array of structs ---
    printf("\n--- Array of structs ---\n");

    struct Point vertices[] = {
        { .x = 0,   .y = 0   },
        { .x = 10,  .y = 0   },
        { .x = 10,  .y = 10  },
        { .x = 0,   .y = 10  },
    };

    size_t count = sizeof(vertices) / sizeof(vertices[0]);
    printf("Triangle has %zu vertices:\n", count);

    for (size_t i = 0; i < count; i++) {
        printf("  vertices[%zu] = (%d, %d)\n", i, vertices[i].x, vertices[i].y);
    }

    // Modify an element:
    vertices[2] = make_point(15, 15);
    printf("\nAfter vertices[2] = make_point(15, 15):\n");
    for (size_t i = 0; i < count; i++) {
        printf("  vertices[%zu] = (%d, %d)\n", i, vertices[i].x, vertices[i].y);
    }

    // Iterate with pointer:
    printf("\nIterate with pointer:\n");
    for (struct Point *ptr = vertices; ptr < vertices + count; ptr++) {
        printf("  (%d, %d)\n", ptr->x, ptr->y);
    }

    return 0;
}
