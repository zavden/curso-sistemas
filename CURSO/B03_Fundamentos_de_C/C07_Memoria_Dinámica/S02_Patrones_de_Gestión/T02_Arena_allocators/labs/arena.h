#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct Arena {
    char *base;
    size_t offset;
    size_t cap;
};

int arena_init(struct Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->offset = 0;
    a->cap = cap;
    return 0;
}

void *arena_alloc(struct Arena *a, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;

    if (a->offset + aligned > a->cap) {
        return NULL;
    }

    void *ptr = a->base + a->offset;
    a->offset += aligned;
    return ptr;
}

void arena_reset(struct Arena *a) {
    a->offset = 0;
}

void arena_destroy(struct Arena *a) {
    free(a->base);
    a->base = NULL;
    a->offset = 0;
    a->cap = 0;
}

#endif
