#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *create_greeting(const char *name) {
    size_t len = strlen("Hello, ") + strlen(name) + 2;
    char *msg = malloc(len);
    if (!msg) return NULL;
    snprintf(msg, len, "Hello, %s!", name);
    return msg;  /* caller must free — but will they? */
}

int *create_scores(int count) {
    int *scores = malloc(count * sizeof(int));
    if (!scores) return NULL;
    for (int i = 0; i < count; i++) {
        scores[i] = (i + 1) * 10;
    }
    return scores;  /* caller must free */
}

int main(void) {
    /* Leak 1: 14 bytes (strlen("Hello, ") + strlen("Alice") + 2) */
    char *g1 = create_greeting("Alice");
    (void)g1;  /* use it, but never free */

    /* Leak 2: 12 bytes (strlen("Hello, ") + strlen("Bob") + 2) */
    char *g2 = create_greeting("Bob");
    (void)g2;

    /* Leak 3: 20 bytes (5 * sizeof(int)) */
    int *scores = create_scores(5);
    (void)scores;

    return 0;
}
