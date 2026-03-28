#include <stdio.h>

#define MODE "fast"
#define LIMIT 100

int main(void)
{
    printf("=== Section 1 ===\n");
    printf("MODE  = %s\n", MODE);
    printf("LIMIT = %d\n", LIMIT);

/* Redefine MODE with #undef first (no warning) */
#undef MODE
#define MODE "safe"

/* Redefine LIMIT with #undef first */
#undef LIMIT
#define LIMIT 200

    printf("\n=== Section 2 (after #undef + redefine) ===\n");
    printf("MODE  = %s\n", MODE);
    printf("LIMIT = %d\n", LIMIT);

/* Undefine LIMIT -- it no longer exists */
#undef LIMIT

    printf("\n=== Section 3 (after #undef LIMIT) ===\n");
    printf("MODE  = %s\n", MODE);
    /* LIMIT is no longer defined here.
       The following line would cause a compilation error:
       printf("LIMIT = %d\n", LIMIT);
    */
    printf("LIMIT is not defined in this section\n");

#ifdef LIMIT
    printf("LIMIT is defined\n");
#else
    printf("#ifdef LIMIT -> false (LIMIT was #undef'd)\n");
#endif

    return 0;
}
