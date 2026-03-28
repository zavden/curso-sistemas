#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdnoreturn.h>
#include <stdint.h>
#include <limits.h>

/* ---- Compile-time checks ---- */
static_assert(sizeof(int) >= 4, "need 32-bit ints");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");
static_assert(sizeof(size_t) >= sizeof(int),
              "size_t must be at least as wide as int");

/* ---- Error handling with noreturn ---- */
noreturn void fatal(const char *file, int line, const char *msg) {
    fflush(stdout);  /* flush pending output before aborting */
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

#define FATAL(msg) fatal(__FILE__, __LINE__, msg)

#define ENSURE(cond, msg) do { \
    if (!(cond)) FATAL("assertion failed: " #cond " -- " msg); \
} while (0)

/* ---- Application code ---- */
struct Record {
    int32_t id;
    char    name[32];
    int32_t score;
};
static_assert(sizeof(struct Record) == 40,
              "Record layout changed unexpectedly");

void process_records(const struct Record *records, int count) {
    ENSURE(records != NULL, "records pointer is NULL");
    ENSURE(count > 0, "count must be positive");

    for (int i = 0; i < count; i++) {
        printf("  id=%d name=%-10s score=%d\n",
               records[i].id, records[i].name, records[i].score);
    }
}

int main(void) {
    printf("=== Error Framework Demo ===\n\n");

    struct Record data[] = {
        { 1, "Alice",   95 },
        { 2, "Bob",     87 },
        { 3, "Charlie", 92 },
    };
    int count = sizeof(data) / sizeof(data[0]);

    printf("Processing %d records:\n", count);
    process_records(data, count);

    printf("\nAll OK. Now triggering a fatal error...\n\n");

    /* This will call FATAL -> abort() */
    process_records(NULL, 3);

    /* Never reached */
    printf("This line is never printed.\n");
    return 0;
}
