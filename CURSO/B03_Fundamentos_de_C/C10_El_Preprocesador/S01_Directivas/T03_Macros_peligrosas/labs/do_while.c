/* do_while.c -- Multi-statement macros: naked, braces, do-while(0) */

#include <stdio.h>

/*
 * Three versions of LOG macro to compare behavior inside if/else.
 * Only LOG_GOOD works correctly in all contexts.
 */

/* BAD: two separate statements */
#define LOG_NAKED(msg) printf("[LOG] "); printf("%s\n", msg);

/* BAD: braces alone -- the trailing ; after use breaks if/else */
#define LOG_BRACES(msg) { printf("[LOG] "); printf("%s\n", msg); }

/* GOOD: do { ... } while (0) */
#define LOG_GOOD(msg) do { printf("[LOG] "); printf("%s\n", msg); } while (0)

/* Practical example: SAFE_FREE */
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

int main(void) {
    int error;

    /*
     * Test 1: LOG_NAKED inside if/else
     *
     * This would not compile with if/else because the semicolons
     * break the structure. We demonstrate it standalone first.
     */
    printf("=== Test 1: LOG_NAKED (standalone) ===\n");
    LOG_NAKED("naked macro works alone");

    /* The following would NOT compile:
     *
     * if (error)
     *     LOG_NAKED("fail");
     * else
     *     printf("ok\n");
     *
     * Because it expands to:
     *
     * if (error)
     *     printf("[LOG] ");
     * printf("%s\n", "fail");    <-- outside the if
     * ;                          <-- empty statement
     * else                       <-- error: else without matching if
     *     printf("ok\n");
     */
    printf("  (cannot use LOG_NAKED inside if/else -- would not compile)\n\n");

    /*
     * Test 2: LOG_BRACES inside if/else
     *
     * Same problem: the ; after LOG_BRACES("...") creates an empty statement
     * that disconnects the else from the if.
     *
     * if (error)
     *     { printf("[LOG] "); printf("%s\n", "fail"); };  <-- ; ends the if
     * else                                                <-- error: no if
     *     printf("ok\n");
     */
    printf("=== Test 2: LOG_BRACES (standalone) ===\n");
    LOG_BRACES("braces macro works alone");
    printf("  (cannot use LOG_BRACES inside if/else either)\n\n");

    /*
     * Test 3: LOG_GOOD inside if/else -- works correctly
     */
    printf("=== Test 3: LOG_GOOD inside if/else ===\n");

    error = 1;
    if (error)
        LOG_GOOD("error path taken");
    else
        printf("ok path\n");

    error = 0;
    if (error)
        LOG_GOOD("error path taken");
    else
        printf("ok path\n");

    printf("\n");

    /*
     * Test 4: demonstrate all three in a context that compiles
     * (using braces around the if body to avoid the problem)
     */
    printf("=== Test 4: All three with explicit braces around if ===\n");
    error = 1;

    if (error) {
        LOG_NAKED("naked (inside braces)");
    }

    if (error) {
        LOG_BRACES("braces (inside braces)");
    }

    if (error) {
        LOG_GOOD("good (inside braces)");
    }

    printf("\nOnly LOG_GOOD works correctly without requiring the caller\n");
    printf("to wrap the call in braces.\n");

    return 0;
}
