#include <stdio.h>

/* --- typedef version --- */
typedef char *String_t;

/* --- #define version --- */
#define String_d char *

int main(void) {
    /* typedef: both variables are char* */
    String_t t_a, t_b;
    t_a = "hello";
    t_b = "world";

    /* #define: expands to "char *d_a, d_b;" -> d_a is char*, d_b is char */
    String_d d_a, d_b;
    d_a = "hello";
    d_b = 'X';  /* d_b is char, not char* */

    printf("=== typedef char *String_t ===\n");
    printf("t_a = %s\n", t_a);
    printf("t_b = %s\n", t_b);
    printf("sizeof(t_a) = %zu (pointer)\n", sizeof(t_a));
    printf("sizeof(t_b) = %zu (pointer)\n", sizeof(t_b));
    printf("\n");

    printf("=== #define String_d char * ===\n");
    printf("d_a = %s\n", d_a);
    printf("d_b = '%c'\n", d_b);
    printf("sizeof(d_a) = %zu (pointer)\n", sizeof(d_a));
    printf("sizeof(d_b) = %zu (char!)\n", sizeof(d_b));

    return 0;
}
