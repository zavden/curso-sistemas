#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

/* Opaque type: the user cannot see the internals of Stack */
typedef struct Stack Stack;

Stack *stack_create(int capacity);
void   stack_destroy(Stack *s);
bool   stack_push(Stack *s, int value);
bool   stack_pop(Stack *s, int *out);
bool   stack_peek(const Stack *s, int *out);
bool   stack_empty(const Stack *s);
int    stack_size(const Stack *s);

#endif /* STACK_H */
