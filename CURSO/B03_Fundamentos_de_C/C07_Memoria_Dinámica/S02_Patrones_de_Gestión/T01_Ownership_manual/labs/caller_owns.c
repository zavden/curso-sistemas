#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// @return: newly allocated string, caller must free()
char *string_duplicate(const char *s) {
    if (!s) return NULL;
    char *copy = malloc(strlen(s) + 1);
    if (!copy) return NULL;
    strcpy(copy, s);
    return copy;
}

// @return: newly allocated formatted string, caller must free()
char *make_greeting(const char *name, int age) {
    // snprintf with NULL measures needed size
    int len = snprintf(NULL, 0, "Hello, %s! You are %d years old.", name, age);
    if (len < 0) return NULL;

    char *buf = malloc((size_t)len + 1);
    if (!buf) return NULL;

    snprintf(buf, (size_t)len + 1, "Hello, %s! You are %d years old.", name, age);
    return buf;
}

int main(void) {
    // string_duplicate: caller owns the result
    char *s1 = string_duplicate("ownership in C");
    if (!s1) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    printf("s1 = \"%s\"\n", s1);
    printf("s1 address = %p\n", (void *)s1);
    free(s1);  // caller responsibility

    // make_greeting: caller owns the result
    char *greeting = make_greeting("Alice", 30);
    if (!greeting) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    printf("greeting = \"%s\"\n", greeting);
    free(greeting);  // caller responsibility

    printf("All memory freed by caller.\n");
    return 0;
}
