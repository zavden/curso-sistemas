/* fixed_leaks.c -- Corrected versions of all leak patterns */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === Fix 1: simple leak -- free before returning === */
void simple_no_leak(void) {
    int *data = malloc(10 * sizeof(int));
    if (!data) return;
    for (int i = 0; i < 10; i++) {
        data[i] = i * i;
    }
    printf("simple_no_leak: computed %d squares\n", 10);
    free(data);
}

/* === Fix 2: reassignment leak -- free before reassigning === */
void reassignment_no_leak(void) {
    char *msg = malloc(32);
    if (!msg) return;
    strcpy(msg, "first message");
    printf("reassignment_no_leak: msg = \"%s\"\n", msg);

    free(msg);                 /* free BEFORE reassigning */
    msg = malloc(64);
    if (!msg) return;
    strcpy(msg, "second message");
    printf("reassignment_no_leak: msg = \"%s\"\n", msg);
    free(msg);
}

/* === Fix 3: error path leak -- goto cleanup pattern === */
int error_path_no_leak(int trigger_error) {
    int result = -1;
    char *buffer = malloc(256);
    if (!buffer) return -1;
    strcpy(buffer, "processing data...");
    printf("error_path_no_leak: %s\n", buffer);

    if (trigger_error) {
        printf("error_path_no_leak: error detected, cleaning up\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    free(buffer);
    return result;
}

/* === Fix 4: struct with proper destructor === */
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

/* CORRECT: free members first, then the struct */
void student_free(struct Student *s) {
    if (!s) return;
    free(s->name);
    free(s->grades);
    free(s);
}

/* === Fix 5: function return -- caller frees === */
char *create_greeting(const char *name) {
    size_t len = strlen("Hello, ") + strlen(name) + strlen("!") + 1;
    char *greeting = malloc(len);
    if (!greeting) return NULL;
    snprintf(greeting, len, "Hello, %s!", name);
    return greeting;
}

int main(void) {
    printf("--- Fix 1: simple -- free before return ---\n");
    simple_no_leak();

    printf("\n--- Fix 2: reassignment -- free before reassigning ---\n");
    reassignment_no_leak();

    printf("\n--- Fix 3: error path -- goto cleanup ---\n");
    error_path_no_leak(1);

    printf("\n--- Fix 4: proper destructor ---\n");
    struct Student *s = student_create("Carlos", 3);
    if (s) {
        s->grades[0] = 85;
        s->grades[1] = 92;
        s->grades[2] = 78;
        printf("Student: %s, grades: %d, %d, %d\n",
               s->name, s->grades[0], s->grades[1], s->grades[2]);
        student_free(s);
    }

    printf("\n--- Fix 5: caller frees returned pointer ---\n");
    char *g1 = create_greeting("Alice");
    if (g1) printf("g1: %s\n", g1);
    free(g1);

    char *g2 = create_greeting("Bob");
    if (g2) printf("g2: %s\n", g2);
    free(g2);

    printf("\nAll memory freed. Program finished.\n");
    return 0;
}
