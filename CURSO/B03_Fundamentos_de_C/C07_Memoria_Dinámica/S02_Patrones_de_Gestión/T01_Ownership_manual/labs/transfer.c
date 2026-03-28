#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Simple stack (LIFO) that takes ownership of data ---

struct Node {
    void *data;          // owned by this node
    struct Node *next;
};

struct Stack {
    struct Node *top;
    int count;
};

void stack_init(struct Stack *s) {
    s->top = NULL;
    s->count = 0;
}

// @param data: this function TAKES OWNERSHIP. Do not use after call.
void stack_push(struct Stack *s, void *data) {
    struct Node *n = malloc(sizeof(struct Node));
    if (!n) {
        fprintf(stderr, "stack_push: allocation failed\n");
        free(data);  // we accepted ownership, so we must free on error
        return;
    }
    n->data = data;
    n->next = s->top;
    s->top = n;
    s->count++;
}

// @return: data pointer. TRANSFERS OWNERSHIP to caller.
//          Caller must free() the returned pointer.
//          Returns NULL if stack is empty.
void *stack_pop(struct Stack *s) {
    if (!s->top) return NULL;

    struct Node *n = s->top;
    void *data = n->data;
    s->top = n->next;
    s->count--;
    free(n);  // free the node, NOT the data (ownership transferred to caller)
    return data;
}

// Destroys the stack AND all remaining data
void stack_destroy(struct Stack *s) {
    struct Node *curr = s->top;
    while (curr) {
        struct Node *next = curr->next;
        free(curr->data);  // we own the data, so we free it
        free(curr);
        curr = next;
    }
    s->top = NULL;
    s->count = 0;
}

int main(void) {
    struct Stack s;
    stack_init(&s);

    // Create strings on the heap and TRANSFER ownership to the stack
    printf("--- Pushing 4 strings onto stack ---\n");
    char *names[] = {"Alice", "Bob", "Charlie", "Diana"};
    for (int i = 0; i < 4; i++) {
        char *copy = malloc(strlen(names[i]) + 1);
        if (!copy) {
            fprintf(stderr, "allocation failed\n");
            stack_destroy(&s);
            return 1;
        }
        strcpy(copy, names[i]);

        printf("Pushing \"%s\" (address %p)\n", copy, (void *)copy);
        stack_push(&s, copy);
        // copy was MOVED into the stack. Do not use copy after this line.
    }
    printf("Stack count: %d\n\n", s.count);

    // Pop 2 items: ownership transfers BACK to us
    printf("--- Popping 2 items (we receive ownership) ---\n");
    for (int i = 0; i < 2; i++) {
        char *item = stack_pop(&s);
        if (item) {
            printf("Popped: \"%s\" (address %p)\n", item, (void *)item);
            free(item);  // we own it now, we must free it
        }
    }
    printf("Stack count: %d\n\n", s.count);

    // Destroy the stack: remaining items are freed by stack_destroy
    printf("--- Destroying stack (frees remaining items) ---\n");
    stack_destroy(&s);
    printf("Stack destroyed. Count: %d\n", s.count);

    return 0;
}
