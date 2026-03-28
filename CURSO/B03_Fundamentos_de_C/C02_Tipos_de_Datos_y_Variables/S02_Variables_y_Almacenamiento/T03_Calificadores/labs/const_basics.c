#include <stdio.h>

int main(void) {
    const int max = 100;
    int mutable_var = 50;

    printf("max = %d\n", max);
    printf("mutable_var = %d\n", mutable_var);

    /* Pointer to const: cannot modify the data */
    const int *ptr_to_const = &max;
    printf("\n--- Pointer to const ---\n");
    printf("*ptr_to_const = %d\n", *ptr_to_const);

    /* ptr_to_const can be reassigned */
    ptr_to_const = &mutable_var;
    printf("After reassign, *ptr_to_const = %d\n", *ptr_to_const);

    /* Const pointer: cannot change where it points */
    int *const const_ptr = &mutable_var;
    printf("\n--- Const pointer ---\n");
    *const_ptr = 99;
    printf("After *const_ptr = 99, mutable_var = %d\n", mutable_var);

    /* Both const */
    const int *const both_const = &max;
    printf("\n--- Both const ---\n");
    printf("*both_const = %d\n", *both_const);

    return 0;
}
