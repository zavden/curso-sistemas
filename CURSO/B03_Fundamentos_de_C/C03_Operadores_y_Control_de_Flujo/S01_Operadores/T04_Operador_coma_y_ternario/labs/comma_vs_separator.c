#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main(void) {
    /* --- Comma as SEPARATOR (not an operator) --- */

    /* 1. Variable declarations: comma separates declarators */
    int a = 1, b = 2, c = 3;
    printf("Declaration separator: a=%d, b=%d, c=%d\n", a, b, c);

    /* 2. Function arguments: comma separates arguments */
    int sum = add(a, b);
    printf("Function arg separator: add(%d, %d) = %d\n", a, b, sum);

    /* 3. Array initializer: comma separates elements */
    int arr[] = {10, 20, 30};
    printf("Initializer separator: arr = {%d, %d, %d}\n",
           arr[0], arr[1], arr[2]);

    /* --- Comma as OPERATOR --- */
    printf("\n--- Comma as operator ---\n");

    /* 4. Parentheses force comma to be an operator in function calls */
    /* add((a++, b)) passes ONE argument: increments a, then passes b */
    a = 1;
    b = 2;
    int r = add((a++, b), 100);
    printf("add((a++, b), 100): a is now %d, r=%d\n", a, r);
    /* a was incremented to 2, then b (=2) was passed as first arg */
    /* result: add(2, 100) = 102 */

    /* 5. Comma operator in while condition */
    printf("\nwhile with comma operator:\n");
    int count = 0;
    int val;
    int data[] = {10, 20, 30, -1};  /* -1 is sentinel */
    int idx = 0;
    while (val = data[idx++], val >= 0) {
        printf("  val = %d\n", val);
        count++;
    }
    printf("Processed %d values\n", count);

    return 0;
}
