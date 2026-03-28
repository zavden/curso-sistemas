#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

/* Create a new node */
struct Node *node_create(int value) {
    struct Node *n = malloc(sizeof(struct Node));
    if (!n) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
    n->value = value;
    n->next = NULL;
    return n;
}

/* Insert at the beginning */
void list_push(struct Node **head, int value) {
    struct Node *n = node_create(value);
    if (!n) return;
    n->next = *head;
    *head = n;
}

/* Insert at the end */
void list_append(struct Node **head, int value) {
    struct Node *n = node_create(value);
    if (!n) return;

    if (*head == NULL) {
        *head = n;
        return;
    }

    struct Node *current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = n;
}

/* Print the list */
void list_print(const struct Node *head) {
    for (const struct Node *n = head; n != NULL; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}

/* Count nodes */
int list_count(const struct Node *head) {
    int count = 0;
    for (const struct Node *n = head; n != NULL; n = n->next) {
        count++;
    }
    return count;
}

/* Free all nodes */
void list_free(struct Node *head) {
    while (head) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    struct Node *list = NULL;

    printf("=== Building list with list_push ===\n");
    list_push(&list, 30);
    list_push(&list, 20);
    list_push(&list, 10);
    printf("After push 30, 20, 10: ");
    list_print(list);
    printf("Count: %d\n", list_count(list));

    printf("\n=== Appending with list_append ===\n");
    list_append(&list, 40);
    list_append(&list, 50);
    printf("After append 40, 50: ");
    list_print(list);
    printf("Count: %d\n", list_count(list));

    printf("\n=== Freeing list ===\n");
    list_free(list);
    list = NULL;
    printf("List freed. Count: %d\n", list_count(list));

    printf("\n=== Building new list with append only ===\n");
    list_append(&list, 100);
    list_append(&list, 200);
    list_append(&list, 300);
    printf("After append 100, 200, 300: ");
    list_print(list);

    list_free(list);
    return 0;
}
