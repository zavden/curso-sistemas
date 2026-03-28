#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bug 1: memory leak — allocated but never freed */
void register_user(const char *username) {
    char *record = malloc(64);
    if (!record) return;
    snprintf(record, 64, "user:%s", username);
    /* forgot to free or return record */
}

/* Bug 2: off-by-one in sprintf — writes past buffer */
void format_id(int id) {
    /* Buffer too small: "ID-1000" is 7 chars + '\0' = 8, but only 7 allocated */
    char *buf = malloc(7);
    if (!buf) return;
    sprintf(buf, "ID-%d", id);
    printf("Formatted: %s\n", buf);
    free(buf);
}

/* Bug 3: uses uninitialized heap memory */
int sum_array(int count) {
    int *arr = malloc(count * sizeof(int));
    if (!arr) return -1;

    /* Only initializes even indices */
    for (int i = 0; i < count; i += 2) {
        arr[i] = i;
    }

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += arr[i];  /* reads uninitialized odd indices */
    }

    free(arr);
    return total;
}

/* Bug 4: double free */
void process_data(void) {
    int *buffer = malloc(20 * sizeof(int));
    if (!buffer) return;

    for (int i = 0; i < 20; i++) {
        buffer[i] = i * 2;
    }

    free(buffer);
    /* ... more code that makes you forget buffer was freed ... */
    free(buffer);
}

int main(void) {
    register_user("admin");
    register_user("guest");
    format_id(1000);
    printf("Sum: %d\n", sum_array(6));
    process_data();
    return 0;
}
