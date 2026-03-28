#include <stdio.h>

// --- Operations ---
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divi(int a, int b) { return b != 0 ? a / b : 0; }

int main(void) {
    typedef int (*Operation)(int, int);

    // Dispatch table: array of function pointers
    Operation ops[] = {add, sub, mul, divi};
    const char *symbols[] = {"+", "-", "*", "/"};
    int n_ops = 4;

    int a = 20, b = 6;

    printf("=== Dispatch table: calculator ===\n");
    printf("Operands: %d and %d\n\n", a, b);

    for (int i = 0; i < n_ops; i++) {
        printf("  %d %s %d = %d\n", a, symbols[i], b, ops[i](a, b));
    }

    // Selecting operation by index
    printf("\n=== Select by index ===\n");
    int choice = 2;  // multiplication
    printf("Index %d -> %d %s %d = %d\n",
           choice, a, symbols[choice], b, ops[choice](a, b));

    // Division by zero protection
    printf("\n=== Division by zero ===\n");
    printf("  %d / 0 = %d (protected)\n", a, ops[3](a, 0));

    return 0;
}
