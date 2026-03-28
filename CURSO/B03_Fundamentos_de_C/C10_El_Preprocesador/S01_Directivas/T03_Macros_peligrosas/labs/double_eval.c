/* double_eval.c -- Multiple evaluation of arguments with side effects */

#include <stdio.h>

#define MAX(a, b)    ((a) > (b) ? (a) : (b))
#define SQUARE(x)    ((x) * (x))

static inline int max_inline(int a, int b) {
    return a > b ? a : b;
}

static inline int square_inline(int x) {
    return x * x;
}

static int call_count = 0;

static int counted_value(int v) {
    call_count++;
    return v;
}

int main(void) {
    int x, y, result;

    /* --- MAX with ++ --- */
    printf("=== Multiple evaluation: MAX(x++, y++) ===\n\n");

    x = 5; y = 3;
    printf("Before macro:  x=%d, y=%d\n", x, y);
    result = MAX(x++, y++);
    printf("After MAX(x++, y++): result=%d, x=%d, y=%d\n", result, x, y);
    printf("Expected if correct: result=5, x=6, y=4\n\n");

    x = 5; y = 3;
    printf("Before inline: x=%d, y=%d\n", x, y);
    result = max_inline(x++, y++);
    printf("After max_inline(x++, y++): result=%d, x=%d, y=%d\n\n",
           result, x, y);

    /* --- SQUARE with ++ --- */
    printf("=== Multiple evaluation: SQUARE(x++) ===\n\n");

    x = 4;
    printf("Before macro:  x=%d\n", x);
    result = SQUARE(x++);
    printf("After SQUARE(x++): result=%d, x=%d\n", result, x);
    printf("Expected if correct: result=16, x=5\n\n");

    x = 4;
    printf("Before inline: x=%d\n", x);
    result = square_inline(x++);
    printf("After square_inline(x++): result=%d, x=%d\n\n", result, x);

    /* --- Function called multiple times --- */
    printf("=== Side effect: function called multiple times ===\n\n");

    call_count = 0;
    result = MAX(counted_value(10), counted_value(20));
    printf("MAX(counted_value(10), counted_value(20))\n");
    printf("result=%d, function called %d times (expected 2)\n\n",
           result, call_count);

    call_count = 0;
    result = max_inline(counted_value(10), counted_value(20));
    printf("max_inline(counted_value(10), counted_value(20))\n");
    printf("result=%d, function called %d times (expected 2)\n",
           result, call_count);

    return 0;
}
