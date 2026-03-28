/* gcc_expand.c -- small file to inspect variadic macro expansion with gcc -E */

#include <stdio.h>

#define LOG(fmt, ...) fprintf(stderr, "[LOG] " fmt "\n", ##__VA_ARGS__)

#define DEBUG(fmt, ...) \
    fprintf(stderr, "[DBG %s:%d] " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

int main(void) {
    int x = 10;

    LOG("starting");
    LOG("x = %d", x);
    DEBUG("checkpoint");
    DEBUG("value: %d", x);

    return 0;
}
