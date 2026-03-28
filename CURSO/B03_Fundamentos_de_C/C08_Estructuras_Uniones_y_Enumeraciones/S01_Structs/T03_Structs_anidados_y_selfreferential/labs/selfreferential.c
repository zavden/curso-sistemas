#include <stdio.h>
#include <stdlib.h>

/*
 * A struct CANNOT contain an instance of itself (infinite size).
 * It CAN contain a POINTER to itself (fixed 8 bytes on 64-bit).
 */

struct Node {
    int value;
    struct Node *next;  /* pointer to same type -- OK */
};

int main(void) {
    /* Create three nodes on the heap */
    struct Node *a = malloc(sizeof(struct Node));
    struct Node *b = malloc(sizeof(struct Node));
    struct Node *c = malloc(sizeof(struct Node));

    if (!a || !b || !c) {
        fprintf(stderr, "malloc failed\n");
        free(a);
        free(b);
        free(c);
        return 1;
    }

    /* Link them: a -> b -> c -> NULL */
    a->value = 10;
    a->next = b;

    b->value = 20;
    b->next = c;

    c->value = 30;
    c->next = NULL;

    /* Traverse the chain */
    printf("Manual traversal:\n");
    printf("a->value = %d\n", a->value);
    printf("a->next->value = %d\n", a->next->value);
    printf("a->next->next->value = %d\n", a->next->next->value);

    /* Traverse with a loop */
    printf("\nLoop traversal:\n");
    for (const struct Node *n = a; n != NULL; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");

    /* Show addresses to visualize the chain */
    printf("\nAddresses:\n");
    printf("a = %p, a->next = %p\n", (void *)a, (void *)a->next);
    printf("b = %p, b->next = %p\n", (void *)b, (void *)b->next);
    printf("c = %p, c->next = %p\n", (void *)c, (void *)c->next);

    printf("\nsizeof(struct Node) = %zu\n", sizeof(struct Node));
    printf("sizeof(struct Node *) = %zu\n", sizeof(struct Node *));

    /* Free all nodes */
    free(a);
    free(b);
    free(c);

    return 0;
}
