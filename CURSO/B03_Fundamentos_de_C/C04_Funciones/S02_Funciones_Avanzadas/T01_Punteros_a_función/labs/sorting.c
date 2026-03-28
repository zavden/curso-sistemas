#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Comparison functions for ints ---
int compare_ascending(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int compare_descending(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ib > ia) - (ib < ia);
}

// --- Comparison function for strings ---
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// --- Comparison for strings by length ---
int compare_by_length(const void *a, const void *b) {
    size_t la = strlen(*(const char **)a);
    size_t lb = strlen(*(const char **)b);
    return (la > lb) - (la < lb);
}

// --- Helpers ---
void print_int_array(const char *label, const int *arr, int n) {
    printf("%s {", label);
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("}\n");
}

void print_str_array(const char *label, const char **arr, int n) {
    printf("%s {", label);
    for (int i = 0; i < n; i++) {
        printf("\"%s\"", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("}\n");
}

int main(void) {
    // --- Sort integers ---
    int nums[] = {42, 7, 13, 99, 1, 55, 23};
    int n = 7;

    printf("=== qsort with integers ===\n");
    print_int_array("Original:  ", nums, n);

    qsort(nums, n, sizeof(int), compare_ascending);
    print_int_array("Ascending: ", nums, n);

    qsort(nums, n, sizeof(int), compare_descending);
    print_int_array("Descending:", nums, n);

    // --- Sort strings ---
    const char *words[] = {"banana", "apple", "cherry", "date", "elderberry", "fig"};
    int nw = 6;

    printf("\n=== qsort with strings ===\n");
    print_str_array("Original:       ", words, nw);

    qsort(words, nw, sizeof(char *), compare_strings);
    print_str_array("Alphabetical:   ", words, nw);

    qsort(words, nw, sizeof(char *), compare_by_length);
    print_str_array("By length:      ", words, nw);

    return 0;
}
