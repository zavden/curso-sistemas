#include <stdio.h>
#include <string.h>

struct Point {
    int x;
    int y;
};

struct WithPointer {
    int id;
    const char *label;    // pointer -- shallow copy danger
};

int point_eq(struct Point a, struct Point b) {
    return a.x == b.x && a.y == b.y;
}

int main(void) {
    // --- Copy with = ---
    struct Point a = { .x = 10, .y = 20 };
    struct Point b = a;    // copy all members

    printf("a = (%d, %d)\n", a.x, a.y);
    printf("b = (%d, %d)  (copied from a)\n", b.x, b.y);

    // Modify b -- a is NOT affected:
    b.x = 999;
    printf("\nAfter b.x = 999:\n");
    printf("a = (%d, %d)  (unchanged)\n", a.x, a.y);
    printf("b = (%d, %d)  (modified)\n", b.x, b.y);

    // --- Comparison: == does NOT work ---
    struct Point c = { .x = 10, .y = 20 };
    struct Point d = { .x = 10, .y = 20 };

    // This would NOT compile:
    // if (c == d) { printf("equal\n"); }

    // Option 1: field-by-field comparison:
    if (c.x == d.x && c.y == d.y) {
        printf("\nc and d are equal (field-by-field)\n");
    }

    // Option 2: comparison function:
    if (point_eq(c, d)) {
        printf("c and d are equal (point_eq function)\n");
    }

    // Option 3: memcmp -- WARNING about padding:
    struct Point e = {0};
    e.x = 10;
    e.y = 20;
    if (memcmp(&c, &e, sizeof(struct Point)) == 0) {
        printf("c and e are equal (memcmp)\n");
    } else {
        printf("c and e differ via memcmp (possible padding issue)\n");
    }

    // --- Shallow copy with pointers ---
    printf("\n--- Shallow copy ---\n");
    struct WithPointer s1 = { .id = 1, .label = "alpha" };
    struct WithPointer s2 = s1;    // copies the POINTER, not the string

    printf("s1.label = \"%s\" at %p\n", s1.label, (const void *)s1.label);
    printf("s2.label = \"%s\" at %p\n", s2.label, (const void *)s2.label);
    printf("Same address? %s\n",
           s1.label == s2.label ? "YES -- shallow copy" : "NO");

    return 0;
}
