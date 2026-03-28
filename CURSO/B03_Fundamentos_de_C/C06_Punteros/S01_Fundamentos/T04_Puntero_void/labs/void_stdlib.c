#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Comparators for qsort --- */

int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

struct Student {
    char name[50];
    int grade;
};

int cmp_by_grade(const void *a, const void *b) {
    const struct Student *sa = a;
    const struct Student *sb = b;
    return (sa->grade > sb->grade) - (sa->grade < sb->grade);
}

int cmp_by_name(const void *a, const void *b) {
    const struct Student *sa = a;
    const struct Student *sb = b;
    return strcmp(sa->name, sb->name);
}

/* --- Helper to print arrays --- */

void print_int_array(const int *arr, size_t n) {
    printf("  [");
    for (size_t i = 0; i < n; i++) {
        printf("%d%s", arr[i], (i < n - 1) ? ", " : "");
    }
    printf("]\n");
}

void print_double_array(const double *arr, size_t n) {
    printf("  [");
    for (size_t i = 0; i < n; i++) {
        printf("%.1f%s", arr[i], (i < n - 1) ? ", " : "");
    }
    printf("]\n");
}

void print_students(const struct Student *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        printf("  %s: %d\n", arr[i].name, arr[i].grade);
    }
}

int main(void) {
    /* --- malloc returns void* --- */
    printf("=== malloc returns void* ===\n");
    int *arr = malloc(5 * sizeof(int));
    if (!arr) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    int values[] = {5, 2, 8, 1, 9};
    memcpy(arr, values, 5 * sizeof(int));
    printf("Dynamic array:");
    print_int_array(arr, 5);

    /* --- qsort with ints --- */
    printf("\n=== qsort with ints ===\n");
    printf("Before:");
    print_int_array(arr, 5);
    qsort(arr, 5, sizeof(int), cmp_int);
    printf("After: ");
    print_int_array(arr, 5);
    free(arr);

    /* --- qsort with doubles --- */
    printf("\n=== qsort with doubles ===\n");
    double darr[] = {3.1, 1.4, 2.7, 0.5, 4.2};
    printf("Before:");
    print_double_array(darr, 5);
    qsort(darr, 5, sizeof(double), cmp_double);
    printf("After: ");
    print_double_array(darr, 5);

    /* --- qsort with structs --- */
    printf("\n=== qsort with structs (by grade) ===\n");
    struct Student class[] = {
        {"Charlie", 85},
        {"Alice",   92},
        {"Bob",     78},
        {"Diana",   95},
    };
    size_t n = sizeof(class) / sizeof(class[0]);

    printf("Before:\n");
    print_students(class, n);
    qsort(class, n, sizeof(struct Student), cmp_by_grade);
    printf("After (by grade):\n");
    print_students(class, n);

    /* --- qsort same data, different comparator --- */
    printf("\n=== qsort with structs (by name) ===\n");
    qsort(class, n, sizeof(struct Student), cmp_by_name);
    printf("After (by name):\n");
    print_students(class, n);

    /* --- bsearch --- */
    printf("\n=== bsearch ===\n");
    int sorted[] = {1, 2, 5, 8, 9};
    size_t sn = sizeof(sorted) / sizeof(sorted[0]);
    printf("Array:");
    print_int_array(sorted, sn);

    int key = 5;
    int *found = bsearch(&key, sorted, sn, sizeof(int), cmp_int);
    if (found) {
        printf("Search for %d: found at index %td\n", key, found - sorted);
    } else {
        printf("Search for %d: not found\n", key);
    }

    key = 7;
    found = bsearch(&key, sorted, sn, sizeof(int), cmp_int);
    if (found) {
        printf("Search for %d: found at index %td\n", key, found - sorted);
    } else {
        printf("Search for %d: not found\n", key);
    }

    return 0;
}
