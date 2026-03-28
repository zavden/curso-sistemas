#include <stdio.h>

struct Point {
    int x;
    int y;
};

struct Person {
    char name[50];
    int age;
    double height;
    char email[100];
};

int main(void) {
    // Positional initialization (order-dependent):
    struct Point p1 = {10, 20};

    // Designated initializers (C99) -- by name:
    struct Point p2 = { .x = 30, .y = 40 };

    // Order does not matter with designated initializers:
    struct Point p3 = { .y = 60, .x = 50 };

    // Omitted fields are zero-initialized:
    struct Person alice = { .name = "Alice", .age = 30 };

    // Full zero initialization:
    struct Person empty = {0};

    printf("p1 = (%d, %d)\n", p1.x, p1.y);
    printf("p2 = (%d, %d)\n", p2.x, p2.y);
    printf("p3 = (%d, %d)\n", p3.x, p3.y);

    printf("\nalice.name   = \"%s\"\n", alice.name);
    printf("alice.age    = %d\n", alice.age);
    printf("alice.height = %.1f (zero -- not set)\n", alice.height);
    printf("alice.email  = \"%s\" (empty -- not set)\n", alice.email);

    printf("\nempty.name   = \"%s\"\n", empty.name);
    printf("empty.age    = %d\n", empty.age);

    // Modify fields with dot operator:
    p1.x = 100;
    p1.y = 200;
    printf("\np1 modified = (%d, %d)\n", p1.x, p1.y);

    return 0;
}
