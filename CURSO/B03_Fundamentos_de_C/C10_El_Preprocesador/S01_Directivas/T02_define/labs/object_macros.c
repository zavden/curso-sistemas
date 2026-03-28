#include <stdio.h>

/* --- Object-like macros --- */

#define PI          3.14159265358979
#define MAX_SIZE    1024
#define VERSION     "1.0.0"

/* Trap: macro without parentheses */
#define BAD_EXPR    1 + 2
#define GOOD_EXPR   (1 + 2)

int main(void)
{
    double radius = 5.0;
    double area = PI * radius * radius;
    printf("PI         = %f\n", PI);
    printf("Area (r=5) = %f\n", area);
    printf("MAX_SIZE   = %d\n", MAX_SIZE);
    printf("VERSION    = %s\n", VERSION);

    printf("\n--- Parentheses trap ---\n");
    int bad  = BAD_EXPR * 3;
    int good = GOOD_EXPR * 3;
    printf("BAD_EXPR  * 3 = %d  (1 + 2 * 3 = 7)\n", bad);
    printf("GOOD_EXPR * 3 = %d  ((1 + 2) * 3 = 9)\n", good);

    return 0;
}
