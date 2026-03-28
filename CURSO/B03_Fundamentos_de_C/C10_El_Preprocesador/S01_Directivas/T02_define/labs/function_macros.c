#include <stdio.h>

/* --- Correct: parentheses everywhere --- */
#define SQUARE(x)       ((x) * (x))
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define ABS(x)          ((x) >= 0 ? (x) : -(x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* --- Incorrect: missing parentheses --- */
#define BAD_SQUARE(x)   x * x

/* --- For side-effect demo --- */
static int call_count = 0;

static int next_val(void)
{
    call_count++;
    return 3;
}

static int square_func(int x)
{
    return x * x;
}

int main(void)
{
    printf("--- SQUARE ---\n");
    printf("SQUARE(5)     = %d\n", SQUARE(5));
    printf("SQUARE(2 + 3) = %d  (should be 25)\n", SQUARE(2 + 3));

    printf("\n--- BAD_SQUARE (no parentheses) ---\n");
    printf("BAD_SQUARE(5)     = %d\n", BAD_SQUARE(5));
    printf("BAD_SQUARE(2 + 3) = %d  (should be 25, but is 11)\n",
           BAD_SQUARE(2 + 3));

    printf("\n--- 100 / SQUARE(5) ---\n");
    printf("100 / SQUARE(5)     = %d  (correct: 4)\n", 100 / SQUARE(5));

    printf("\n--- MIN / MAX / ABS ---\n");
    printf("MIN(10, 3)  = %d\n", MIN(10, 3));
    printf("MAX(10, 3)  = %d\n", MAX(10, 3));
    printf("ABS(-7)     = %d\n", ABS(-7));

    printf("\n--- ARRAY_SIZE ---\n");
    int nums[] = {10, 20, 30, 40, 50};
    printf("Array elements: %zu\n", ARRAY_SIZE(nums));

    printf("\n--- Side-effect danger ---\n");
    /* A real function evaluates its argument once.
       A macro is text substitution, so the argument appears
       multiple times in the expansion and gets evaluated
       multiple times. */

    call_count = 0;
    int r1 = square_func(next_val());
    printf("square_func(next_val()) = %d, next_val called %d time\n",
           r1, call_count);

    call_count = 0;
    int r2 = SQUARE(next_val());
    printf("SQUARE(next_val())      = %d, next_val called %d times!\n",
           r2, call_count);
    printf("Macro expanded to: ((next_val()) * (next_val()))\n");
    printf("This is why inline functions are preferred over macros.\n");

    return 0;
}
