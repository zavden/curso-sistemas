#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Person struct: create/destroy pattern ---

struct Person {
    char *name;
    char *email;
    int age;
};

// Constructor: allocates and initializes
// @return: newly allocated Person, caller must call person_destroy()
struct Person *person_create(const char *name, const char *email, int age) {
    struct Person *p = malloc(sizeof(struct Person));
    if (!p) return NULL;

    p->name = NULL;
    p->email = NULL;
    p->age = age;

    p->name = malloc(strlen(name) + 1);
    p->email = malloc(strlen(email) + 1);

    if (!p->name || !p->email) {
        free(p->name);
        free(p->email);
        free(p);
        return NULL;
    }

    strcpy(p->name, name);
    strcpy(p->email, email);
    return p;
}

// Destructor: frees all internal memory and the struct itself
void person_destroy(struct Person *p) {
    if (!p) return;
    free(p->name);
    free(p->email);
    free(p);
}

void person_print(const struct Person *p) {
    if (!p) {
        printf("(null person)\n");
        return;
    }
    printf("Person { name=\"%s\", email=\"%s\", age=%d }\n",
           p->name, p->email, p->age);
}

int main(void) {
    struct Person *alice = person_create("Alice", "alice@example.com", 30);
    struct Person *bob = person_create("Bob", "bob@example.com", 25);

    if (!alice || !bob) {
        fprintf(stderr, "allocation failed\n");
        person_destroy(alice);
        person_destroy(bob);
        return 1;
    }

    person_print(alice);
    person_print(bob);

    // The struct owns its internal strings.
    // We do NOT free alice->name or alice->email individually.
    // person_destroy handles everything.
    person_destroy(alice);
    person_destroy(bob);

    printf("All persons destroyed correctly.\n");
    return 0;
}
