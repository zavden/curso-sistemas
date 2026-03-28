/* zero_args_problem.c -- the trailing comma problem with zero variadic args */

#include <stdio.h>

/* This macro has the zero-args problem:
   LOG_BAD("hello") expands to fprintf(stderr, "hello" "\n", )
   The trailing comma before ')' is a syntax error.
   Uncomment the call in main to see the error. */
#define LOG_BAD(fmt, ...) fprintf(stderr, "[BAD] " fmt "\n", __VA_ARGS__)

/* Solution 1: GCC extension ##__VA_ARGS__ removes trailing comma */
#define LOG_GCC(fmt, ...) fprintf(stderr, "[GCC] " fmt "\n", ##__VA_ARGS__)

/* Solution 2: no fixed parameter -- fmt is part of __VA_ARGS__ */
#define LOG_NOFIXED(...) fprintf(stderr, __VA_ARGS__)

int main(void) {
    int code = 404;

    /* Uncomment the next line to see the compilation error: */
    /* LOG_BAD("starting server"); */

    /* This works because there IS a variadic argument: */
    LOG_BAD("error code: %d", code);

    /* ##__VA_ARGS__ handles both cases: */
    LOG_GCC("starting server");
    LOG_GCC("error code: %d", code);

    /* No fixed parameter also works, but limits prefix/suffix ability: */
    LOG_NOFIXED("starting server\n");
    LOG_NOFIXED("error code: %d\n", code);

    return 0;
}
