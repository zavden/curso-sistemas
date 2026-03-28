#include <stdio.h>

/* Manual strlen: count chars until '\0' */
size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* Manual strcmp: compare char by char */
int my_strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Manual strcpy: copy src into dest, return dest */
char *my_strcpy(char *dest, const char *src) {
    char *start = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return start;
}

int main(void) {
    printf("=== my_strlen ===\n");
    printf("my_strlen(\"\")            = %zu\n", my_strlen(""));
    printf("my_strlen(\"a\")           = %zu\n", my_strlen("a"));
    printf("my_strlen(\"hello\")       = %zu\n", my_strlen("hello"));
    printf("my_strlen(\"hello\\0world\") = %zu\n", my_strlen("hello\0world"));

    printf("\n=== my_strcmp ===\n");
    printf("my_strcmp(\"abc\", \"abc\") = %d\n", my_strcmp("abc", "abc"));
    printf("my_strcmp(\"abc\", \"abd\") = %d\n", my_strcmp("abc", "abd"));
    printf("my_strcmp(\"abd\", \"abc\") = %d\n", my_strcmp("abd", "abc"));
    printf("my_strcmp(\"ab\",  \"abc\") = %d\n", my_strcmp("ab", "abc"));

    printf("\n=== my_strcpy ===\n");
    char buf[20];
    my_strcpy(buf, "copied!");
    printf("buf after my_strcpy: \"%s\"\n", buf);

    return 0;
}
