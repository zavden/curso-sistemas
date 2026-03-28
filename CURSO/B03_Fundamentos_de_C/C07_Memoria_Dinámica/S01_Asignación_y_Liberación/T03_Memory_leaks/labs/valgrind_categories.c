/* valgrind_categories.c -- Demonstrate Valgrind leak categories */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Record {
    char *name;
    int  *scores;
    int   count;
};

/* Definitely lost: all references are destroyed */
void definitely_lost(void) {
    int *data = malloc(64);
    if (!data) return;
    data[0] = 42;
    printf("definitely_lost: data[0] = %d\n", data[0]);
    data = NULL;  /* only reference gone -- definitely lost */
    (void)data;
}

/* Indirectly lost: reachable only through a definitely lost block */
void indirectly_lost(void) {
    struct Record *rec = malloc(sizeof(struct Record));
    if (!rec) return;

    rec->name = malloc(32);
    if (!rec->name) { free(rec); return; }
    strcpy(rec->name, "Alice");

    rec->scores = malloc(5 * sizeof(int));
    if (!rec->scores) { free(rec->name); free(rec); return; }
    rec->count = 5;
    for (int i = 0; i < rec->count; i++) {
        rec->scores[i] = (i + 1) * 10;
    }

    printf("indirectly_lost: %s, scores[0] = %d\n", rec->name, rec->scores[0]);
    /* rec is definitely lost; rec->name and rec->scores are indirectly lost */
}

/* Still reachable: pointer exists at program exit but never freed */
static char *global_config = NULL;

void still_reachable(void) {
    global_config = malloc(128);
    if (!global_config) return;
    strcpy(global_config, "log_level=debug");
    printf("still_reachable: config = \"%s\"\n", global_config);
    /* global_config is accessible at exit but never freed */
}

int main(void) {
    printf("--- Definitely lost ---\n");
    definitely_lost();

    printf("\n--- Indirectly lost ---\n");
    indirectly_lost();

    printf("\n--- Still reachable ---\n");
    still_reachable();

    printf("\nProgram finished. global_config = \"%s\"\n", global_config);
    return 0;
}
