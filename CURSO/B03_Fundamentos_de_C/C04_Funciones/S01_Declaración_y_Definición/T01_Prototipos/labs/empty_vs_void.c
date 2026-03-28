#include <stdio.h>

int foo();       /* empty parentheses: unspecified parameters */
int bar(void);   /* void: explicitly zero parameters */

int main(void) {
    printf("foo(1, 2, 3) = %d\n", foo(1, 2, 3));
    printf("bar() = %d\n", bar());
    return 0;
}

int foo() {
    return 42;
}

int bar(void) {
    return 99;
}
