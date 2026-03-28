#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int main(void) {
    // Declare a function pointer
    int (*op)(int, int);

    // Assign and call through the pointer
    op = add;
    printf("add(10, 3) = %d\n", op(10, 3));

    op = sub;
    printf("sub(10, 3) = %d\n", op(10, 3));

    op = mul;
    printf("mul(10, 3) = %d\n", op(10, 3));

    // Equivalences: & is optional, (*op)() is the same as op()
    int (*fp)(int, int) = &add;   // & is optional
    printf("\nWith &:    fp(5, 2) = %d\n", fp(5, 2));
    printf("With (*fp): (*fp)(5, 2) = %d\n", (*fp)(5, 2));

    // Using typedef
    typedef int (*BinaryOp)(int, int);

    BinaryOp operation = mul;
    printf("\ntypedef: operation(4, 7) = %d\n", operation(4, 7));

    return 0;
}
