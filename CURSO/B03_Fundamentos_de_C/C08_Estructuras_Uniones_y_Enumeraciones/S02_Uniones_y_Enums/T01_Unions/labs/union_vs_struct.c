#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* --- Approach 1: struct (wastes memory) --- */
struct EventStruct {
    int type;  /* 0=click, 1=key, 2=resize */
    /* all fields always allocated: */
    int x, y;          /* click */
    char key;          /* key */
    int width, height; /* resize */
};

/* --- Approach 2: union (saves memory) --- */
struct EventUnion {
    int type;  /* 0=click, 1=key, 2=resize */
    union {
        struct { int x, y; }          click;
        struct { char key; }          keypress;
        struct { int width, height; } resize;
    } data;
};

void print_event_union(const struct EventUnion *e) {
    switch (e->type) {
        case 0:
            printf("  click at (%d, %d)\n", e->data.click.x, e->data.click.y);
            break;
        case 1:
            printf("  key pressed: '%c'\n", e->data.keypress.key);
            break;
        case 2:
            printf("  resize to %dx%d\n", e->data.resize.width,
                   e->data.resize.height);
            break;
    }
}

int main(void) {
    printf("=== memory comparison ===\n");
    printf("sizeof(struct EventStruct) = %zu bytes\n", sizeof(struct EventStruct));
    printf("sizeof(struct EventUnion)  = %zu bytes\n", sizeof(struct EventUnion));
    printf("savings per event: %zu bytes\n",
           sizeof(struct EventStruct) - sizeof(struct EventUnion));

    int n = 1000;
    printf("\nwith %d events:\n", n);
    printf("  struct: %zu bytes\n", (size_t)n * sizeof(struct EventStruct));
    printf("  union:  %zu bytes\n", (size_t)n * sizeof(struct EventUnion));
    printf("  saved:  %zu bytes\n",
           (size_t)n * (sizeof(struct EventStruct) - sizeof(struct EventUnion)));

    printf("\n=== tagged union in action ===\n");
    struct EventUnion events[] = {
        { .type = 0, .data.click   = { 100, 200 } },
        { .type = 1, .data.keypress = { 'q' } },
        { .type = 2, .data.resize  = { 1920, 1080 } },
    };

    int count = (int)(sizeof(events) / sizeof(events[0]));
    for (int i = 0; i < count; i++) {
        printf("event[%d]:\n", i);
        print_event_union(&events[i]);
    }

    return 0;
}
