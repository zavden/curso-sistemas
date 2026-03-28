/* va_opt_demo.c -- C23 __VA_OPT__ for clean zero-arg handling */

#include <stdio.h>

/* __VA_OPT__(,) expands to a comma ONLY if there are variadic args.
   If there are no variadic args, it expands to nothing. */
#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

#define DEBUG(fmt, ...) \
    fprintf(stderr, "[DBG %s:%d] " fmt "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

int main(void) {
    int x = 42;

    /* Zero variadic args -- __VA_OPT__(,) expands to nothing */
    LOG("server starting");
    LOG("ready");

    /* With variadic args -- __VA_OPT__(,) expands to "," */
    LOG("port = %d", 8080);
    LOG("x = %d, name = %s", x, "test");

    /* DEBUG always passes __FILE__ and __LINE__, plus optional extra args */
    DEBUG("entering main");
    DEBUG("x = %d", x);

    return 0;
}
