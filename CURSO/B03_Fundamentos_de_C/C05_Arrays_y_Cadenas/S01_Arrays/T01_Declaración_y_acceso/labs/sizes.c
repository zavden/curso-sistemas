#include <stdio.h>

int main(void) {
    int nums[5];
    double temps[3];
    char name[20];

    printf("sizeof(int)    = %zu\n", sizeof(int));
    printf("sizeof(double) = %zu\n", sizeof(double));
    printf("sizeof(char)   = %zu\n", sizeof(char));
    printf("\n");

    printf("sizeof(nums)   = %zu  (5 ints)\n", sizeof(nums));
    printf("sizeof(temps)  = %zu  (3 doubles)\n", sizeof(temps));
    printf("sizeof(name)   = %zu  (20 chars)\n", sizeof(name));
    printf("\n");

    size_t count_nums  = sizeof(nums) / sizeof(nums[0]);
    size_t count_temps = sizeof(temps) / sizeof(temps[0]);
    size_t count_name  = sizeof(name) / sizeof(name[0]);

    printf("count nums  = %zu\n", count_nums);
    printf("count temps = %zu\n", count_temps);
    printf("count name  = %zu\n", count_name);

    /* Suppress unused variable warnings */
    (void)nums;
    (void)temps;
    (void)name;

    return 0;
}
