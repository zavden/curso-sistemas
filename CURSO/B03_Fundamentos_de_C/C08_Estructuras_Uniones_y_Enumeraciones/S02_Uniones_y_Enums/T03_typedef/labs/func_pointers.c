#include <stdio.h>

/* --- typedef for function pointers --- */
typedef int (*BinaryOp)(int, int);
typedef void (*Formatter)(const char *, int);

/* --- Concrete functions --- */
static int add(int a, int b)      { return a + b; }
static int subtract(int a, int b) { return a - b; }
static int multiply(int a, int b) { return a * b; }

static void print_result(const char *label, int value) {
    printf("  %s -> %d\n", label, value);
}

static void print_hex(const char *label, int value) {
    printf("  %s -> 0x%X\n", label, value);
}

/* --- Dispatch function using typedef --- */
static BinaryOp get_operation(char op) {
    switch (op) {
        case '+': return add;
        case '-': return subtract;
        case '*': return multiply;
        default:  return NULL;
    }
}

/* --- Apply formatter callback --- */
static void show(const char *label, int value, Formatter fmt) {
    fmt(label, value);
}

int main(void) {
    int a = 15, b = 4;

    printf("=== Function pointer typedef: BinaryOp ===\n");

    /* Use BinaryOp as a variable type */
    BinaryOp op = add;
    printf("add(%d, %d) = %d\n", a, b, op(a, b));

    /* Use dispatch function */
    char operators[] = {'+', '-', '*'};
    const char *names[] = {"add", "subtract", "multiply"};

    for (int i = 0; i < 3; i++) {
        BinaryOp fn = get_operation(operators[i]);
        if (fn) {
            printf("%s(%d, %d) = %d\n", names[i], a, b, fn(a, b));
        }
    }
    printf("\n");

    /* Array of function pointers */
    printf("=== Array of BinaryOp ===\n");
    BinaryOp ops[] = {add, subtract, multiply};
    for (int i = 0; i < 3; i++) {
        printf("ops[%d](%d, %d) = %d\n", i, a, b, ops[i](a, b));
    }
    printf("\n");

    /* Formatter callback */
    printf("=== Formatter callback ===\n");
    int result = multiply(a, b);
    show("decimal", result, print_result);
    show("hex",     result, print_hex);

    return 0;
}
