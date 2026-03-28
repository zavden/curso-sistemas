#include <stdio.h>

int main(void) {
    /* Type promotion in ternary */
    int i = 5;
    double d = 3.14;
    int condition = 1;

    /* When branches have different types, usual arithmetic conversions apply */
    double result1 = (condition) ? i : d;
    double result2 = (!condition) ? i : d;
    printf("condition=1: result = %f (from int %d, promoted to double)\n",
           result1, i);
    printf("condition=0: result = %f (double branch selected)\n", result2);

    /* sizeof reveals the type of the ternary expression */
    printf("\nsizeof(int)    = %zu\n", sizeof(int));
    printf("sizeof(double) = %zu\n", sizeof(double));
    printf("sizeof( condition ? i : d ) = %zu\n",
           sizeof(condition ? i : d));

    /* Right-to-left associativity */
    int n = 2;
    const char *s = (n == 1) ? "one"
                  : (n == 2) ? "two"
                  : (n == 3) ? "three"
                  :            "other";
    printf("\nn=%d -> \"%s\"\n", n, s);

    /* Demonstrating associativity explicitly */
    /* a ? b : c ? d : e  is parsed as  a ? b : (c ? d : e) */
    int a = 0, val_b = 10, c = 1, val_d = 20, e = 30;
    int r1 = a ? val_b : c ? val_d : e;       /* a ? val_b : (c ? val_d : e) */
    int r2 = a ? val_b : (c ? val_d : e);     /* explicit: same thing */
    printf("\na ? b : c ? d : e\n");
    printf("  a=0, b=10, c=1, d=20, e=30\n");
    printf("  result (implicit grouping) = %d\n", r1);
    printf("  result (explicit grouping) = %d\n", r2);

    return 0;
}
