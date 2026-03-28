#include <stdio.h>
#include <string.h>

int main(void) {
    /* --- array of pointers to int --- */
    int a = 10, b = 20, c = 30, d = 40, e = 50;
    int *int_ptrs[] = {&a, &b, &c, &d, &e};
    int count = (int)(sizeof(int_ptrs) / sizeof(int_ptrs[0]));

    printf("=== array of pointers to int ===\n\n");
    for (int i = 0; i < count; i++) {
        printf("int_ptrs[%d] = %p  ->  *int_ptrs[%d] = %d\n",
               i, (void *)int_ptrs[i], i, *int_ptrs[i]);
    }
    printf("\nsizeof(int_ptrs) = %zu bytes (%d elements x %zu bytes each)\n",
           sizeof(int_ptrs), count, sizeof(int_ptrs[0]));

    /* --- array of pointers to char (ragged array) --- */
    const char *names[] = {
        "Alice",
        "Bob",
        "Charlie",
        "Dave",
    };
    int name_count = (int)(sizeof(names) / sizeof(names[0]));

    printf("\n=== array of pointers to char (ragged array) ===\n\n");
    printf("sizeof(names) = %zu bytes (%d pointers x %zu bytes each)\n\n",
           sizeof(names), name_count, sizeof(names[0]));

    for (int i = 0; i < name_count; i++) {
        printf("names[%d] = %-10s  strlen = %zu  (different lengths!)\n",
               i, names[i], strlen(names[i]));
    }

    return 0;
}
