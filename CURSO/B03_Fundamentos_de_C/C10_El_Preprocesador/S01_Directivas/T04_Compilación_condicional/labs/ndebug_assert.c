/*
 * ndebug_assert.c -- NDEBUG and assert()
 *
 * Demonstrates:
 *   - assert() checks conditions at runtime and aborts on failure
 *   - Defining NDEBUG disables ALL assert() calls (they become no-ops)
 *   - NDEBUG must be defined BEFORE #include <assert.h>
 *   - Why assert should not have side effects
 */

#include <stdio.h>

/*
 * NDEBUG, if defined, must come BEFORE #include <assert.h>.
 * We use the command line: gcc -DNDEBUG to define it.
 */
#include <assert.h>

int divide(int a, int b) {
    assert(b != 0 && "divisor must not be zero");
    return a / b;
}

int main(void) {
    printf("=== NDEBUG and assert() ===\n\n");

#ifdef NDEBUG
    printf("NDEBUG is DEFINED -> assert() calls are DISABLED\n\n");
#else
    printf("NDEBUG is NOT defined -> assert() calls are ACTIVE\n\n");
#endif

    /* --- Test 1: successful assertion --- */
    printf("Calling divide(10, 2)...\n");
    int result = divide(10, 2);
    printf("  Result: %d\n\n", result);

    /* --- Test 2: assertion that would fail --- */
    printf("Calling divide(10, 0)...\n");

#ifdef NDEBUG
    printf("  assert(b != 0) is disabled -- this will crash with SIGFPE!\n");
    printf("  (Skipping to avoid crash in this demo)\n\n");
#else
    printf("  assert(b != 0) will FAIL -> program aborts\n");
    /* Uncomment the next line to see assert() abort the program: */
    /* divide(10, 0); */
    printf("  (Line commented out to continue the demo)\n\n");
#endif

    /* --- Why assert must not have side effects --- */
    printf("=== Side effects in assert -- antipattern ===\n\n");

    int counter = 0;

    /*
     * BAD: assert(++counter > 0)
     * If NDEBUG is defined, the entire expression is removed,
     * including the ++counter side effect. The program behaves
     * differently in debug vs release builds.
     *
     * GOOD: separate the side effect from the check.
     */
    ++counter;
    assert(counter > 0 && "counter should be positive");

    printf("counter = %d (should always be 1)\n", counter);

#ifdef NDEBUG
    printf("  -> With NDEBUG, assert was a no-op but ++counter still ran\n");
    printf("     because we separated the side effect from the assertion.\n");
#else
    printf("  -> assert(counter > 0) passed.\n");
#endif

    return 0;
}
