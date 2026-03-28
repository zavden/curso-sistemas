#include "stack.h"
#include <stdlib.h>

/* --- Full definition: only visible inside this file --- */
struct Stack {
    int *data;
    int  top;
    int  capacity;
};

Stack *stack_create(int capacity) {
    if (capacity <= 0) return NULL;

    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;

    s->data = malloc(sizeof(int) * (size_t)capacity);
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->top = -1;
    s->capacity = capacity;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

bool stack_push(Stack *s, int value) {
    if (!s || s->top >= s->capacity - 1) return false;
    s->data[++s->top] = value;
    return true;
}

bool stack_pop(Stack *s, int *out) {
    if (!s || s->top < 0) return false;
    if (out) *out = s->data[s->top];
    s->top--;
    return true;
}

bool stack_peek(const Stack *s, int *out) {
    if (!s || s->top < 0) return false;
    if (out) *out = s->data[s->top];
    return true;
}

bool stack_empty(const Stack *s) {
    if (!s) return true;
    return s->top < 0;
}

int stack_size(const Stack *s) {
    if (!s) return 0;
    return s->top + 1;
}
