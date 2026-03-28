#include <stdio.h>
#include "stack.h"

int main(void) {
    printf("=== Opaque type: Stack ===\n\n");

    Stack *s = stack_create(5);
    if (!s) {
        fprintf(stderr, "Failed to create stack\n");
        return 1;
    }

    printf("Empty? %s\n", stack_empty(s) ? "yes" : "no");
    printf("Size:  %d\n\n", stack_size(s));

    /* Push values */
    printf("Pushing: 10 20 30\n");
    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);
    printf("Size:  %d\n", stack_size(s));
    printf("Empty? %s\n\n", stack_empty(s) ? "yes" : "no");

    /* Peek */
    int val;
    if (stack_peek(s, &val)) {
        printf("Peek:  %d (top, not removed)\n", val);
    }
    printf("Size:  %d\n\n", stack_size(s));

    /* Pop all */
    printf("Popping all:\n");
    while (stack_pop(s, &val)) {
        printf("  popped %d\n", val);
    }
    printf("Size:  %d\n", stack_size(s));
    printf("Empty? %s\n", stack_empty(s) ? "yes" : "no");

    /* s->data = NULL;  <-- WOULD NOT COMPILE: incomplete type */

    stack_destroy(s);
    printf("\nStack destroyed.\n");

    return 0;
}
