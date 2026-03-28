/* ownership.c -- Leak patterns in functions that allocate and return */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Function that allocates and returns -- caller must free */
char *create_greeting(const char *name) {
    size_t len = strlen("Hello, ") + strlen(name) + strlen("!") + 1;
    char *greeting = malloc(len);
    if (!greeting) return NULL;
    snprintf(greeting, len, "Hello, %s!", name);
    return greeting;
}

/* Struct with internal allocations */
struct Student {
    char *name;
    int  *grades;
    int   grade_count;
};

struct Student *student_create(const char *name, int grade_count) {
    struct Student *s = malloc(sizeof(struct Student));
    if (!s) return NULL;

    s->name = malloc(strlen(name) + 1);
    if (!s->name) { free(s); return NULL; }
    strcpy(s->name, name);

    s->grades = malloc((size_t)grade_count * sizeof(int));
    if (!s->grades) { free(s->name); free(s); return NULL; }
    s->grade_count = grade_count;

    return s;
}

/* WRONG: only frees the struct, not its members */
void student_free_wrong(struct Student *s) {
    free(s);
}

int main(void) {
    printf("--- Part A: function that allocates and returns ---\n\n");

    /* Leak: caller never frees the returned pointer */
    char *g1 = create_greeting("Alice");
    if (g1) printf("g1: %s\n", g1);

    char *g2 = create_greeting("Bob");
    if (g2) printf("g2: %s\n", g2);

    /* Only free g2, forget g1 */
    free(g2);
    /* g1 is leaked */

    printf("\n--- Part B: chain of ownership (wrong destructor) ---\n\n");

    struct Student *s = student_create("Carlos", 3);
    if (s) {
        s->grades[0] = 85;
        s->grades[1] = 92;
        s->grades[2] = 78;
        printf("Student: %s, grades: %d, %d, %d\n",
               s->name, s->grades[0], s->grades[1], s->grades[2]);

        /* Wrong: leaks s->name and s->grades */
        student_free_wrong(s);
    }

    printf("\nProgram finished.\n");
    return 0;
}
