#include <stdio.h>
#include <string.h>

/* --- API functions with const correctness --- */

/* sum: reads array, does not modify it -> const int * */
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
        /* arr[i] = 0; */  /* ERROR if uncommented: read-only */
    }
    return total;
}

/* fill: modifies array -> no const */
void fill(int *arr, int n, int value) {
    for (int i = 0; i < n; i++) {
        arr[i] = value;
    }
}

/* find_char: does not modify the string -> const char * param and return */
const char *find_char(const char *str, char c) {
    while (*str != '\0') {
        if (*str == c) {
            return str;
        }
        str++;
    }
    return NULL;
}

/* get_error_message: returns pointer to string literal -> const char * */
const char *get_error_message(int code) {
    static const char *const messages[] = {
        "Success",
        "File not found",
        "Permission denied",
        "Out of memory",
    };
    if (code < 0 || code > 3) {
        return "Unknown error";
    }
    return messages[code];
}

int main(void) {
    int data[] = {10, 20, 30, 40, 50};
    int n = 5;

    /* sum() accepts int* as const int* (implicit conversion) */
    printf("sum = %d\n", sum(data, n));

    /* fill() modifies the array */
    fill(data, n, 99);
    printf("after fill: data[0]=%d data[4]=%d\n", data[0], data[4]);

    /* find_char returns const pointer to position in string */
    const char *msg = "Hello, World!";
    const char *found = find_char(msg, 'W');
    if (found) {
        printf("found 'W' at position %ld: \"%s\"\n", found - msg, found);
    }

    /* get_error_message returns const pointer */
    for (int i = 0; i < 5; i++) {
        printf("code %d: %s\n", i, get_error_message(i));
    }

    return 0;
}
