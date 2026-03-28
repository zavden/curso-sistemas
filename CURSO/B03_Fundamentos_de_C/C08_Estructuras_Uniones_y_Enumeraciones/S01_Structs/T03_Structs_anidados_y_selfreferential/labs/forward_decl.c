#include <stdio.h>

/*
 * Forward declaration: tells the compiler that struct Department
 * exists, without defining its contents yet.
 * This allows struct Employee to contain a pointer to Department.
 */
struct Department;  /* forward declaration */

struct Employee {
    char name[50];
    struct Department *dept;  /* pointer to incomplete type -- OK */
};

struct Department {
    char name[50];
    struct Employee *manager;
    int employee_count;
};

int main(void) {
    struct Employee alice = { .name = "Alice", .dept = NULL };
    struct Employee bob = { .name = "Bob", .dept = NULL };

    struct Department engineering = {
        .name = "Engineering",
        .manager = &alice,
        .employee_count = 15,
    };

    struct Department marketing = {
        .name = "Marketing",
        .manager = &bob,
        .employee_count = 8,
    };

    /* Connect employees to their departments */
    alice.dept = &engineering;
    bob.dept = &marketing;

    /* Access across the mutual references */
    printf("Employee: %s\n", alice.name);
    printf("  Department: %s\n", alice.dept->name);
    printf("  Dept manager: %s\n", alice.dept->manager->name);
    printf("  Dept size: %d\n", alice.dept->employee_count);

    printf("\nEmployee: %s\n", bob.name);
    printf("  Department: %s\n", bob.dept->name);
    printf("  Dept manager: %s\n", bob.dept->manager->name);
    printf("  Dept size: %d\n", bob.dept->employee_count);

    /* Verify circular reference */
    printf("\nCircular check:\n");
    printf("alice.dept->manager->name = %s\n",
           alice.dept->manager->name);
    printf("alice.dept->manager->dept->name = %s\n",
           alice.dept->manager->dept->name);

    return 0;
}
