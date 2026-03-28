/*
 * counter_demo.c — __COUNTER__ y generacion de nombres unicos.
 *
 * __COUNTER__ es una extension GCC/Clang que se incrementa
 * con cada uso. Util para generar identificadores unicos.
 *
 * Nota: -Wpedantic da warning porque __COUNTER__ no es estandar C.
 * Se compila sin -Wpedantic para esta demo.
 */

#include <stdio.h>

/* Helper macros for token pasting with __COUNTER__ */
#define CONCAT_INNER(a, b) a ## b
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define UNIQUE_NAME(prefix) CONCAT(prefix, __COUNTER__)

/*
 * Tracing macro that uses __COUNTER__ to number each trace point.
 * Each TRACE() call gets a unique sequential number at compile time.
 */
#define TRACE(msg) \
    printf("[trace #%d] %s:%d: %s\n", __COUNTER__, __FILE__, __LINE__, msg)

int main(void) {
    printf("=== __COUNTER__ demo ===\n\n");

    /* Each TRACE() call gets the next counter value (0, 1, 2). */
    TRACE("program started");
    TRACE("doing work");
    TRACE("more work");

    printf("\n=== Unique variable names with __COUNTER__ ===\n\n");

    /*
     * UNIQUE_NAME(tmp_) generates tmp_N where N increments.
     * This avoids name collisions in macros that declare temporaries.
     * __COUNTER__ continues from 3 after the 3 TRACE() calls above.
     */
    int UNIQUE_NAME(tmp_) = 100;  /* becomes tmp_3 */
    int UNIQUE_NAME(tmp_) = 200;  /* becomes tmp_4 */
    int UNIQUE_NAME(tmp_) = 300;  /* becomes tmp_5 */

    printf("tmp_3 = %d\n", tmp_3);
    printf("tmp_4 = %d\n", tmp_4);
    printf("tmp_5 = %d\n", tmp_5);
    printf("\nThree unique variables created without name collision.\n");
    printf("(Verify with: gcc -E counter_demo.c | tail -20)\n");

    return 0;
}
