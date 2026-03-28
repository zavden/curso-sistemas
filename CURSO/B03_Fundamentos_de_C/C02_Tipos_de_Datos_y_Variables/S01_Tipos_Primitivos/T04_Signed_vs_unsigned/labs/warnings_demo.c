#include <stdio.h>
#include <limits.h>

int main(void) {
    /* Trigger -Wsign-compare warning */
    int s = -1;
    unsigned int u = 0;

    if (s < u) {
        printf("s < u\n");
    } else {
        printf("s >= u  (unexpected!)\n");
    }

    /* Trigger -Wsign-conversion warning */
    int x = 42;
    unsigned int y = x;
    printf("y = %u\n", y);

    /* Trigger -Wconversion warning */
    unsigned int big = UINT_MAX;
    int truncated = big;
    printf("truncated = %d\n", truncated);

    /* Safe pattern: check before converting */
    int value = -10;
    if (value >= 0) {
        unsigned int safe = (unsigned int)value;
        printf("safe conversion: %u\n", safe);
    } else {
        printf("value is negative (%d), cannot convert safely\n", value);
    }

    return 0;
}
