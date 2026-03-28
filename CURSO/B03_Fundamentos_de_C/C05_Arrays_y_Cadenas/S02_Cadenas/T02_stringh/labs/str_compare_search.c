#include <stdio.h>
#include <string.h>

int main(void) {
    /* --- strcmp --- */
    printf("=== strcmp ===\n");
    printf("strcmp(\"abc\", \"abc\") = %d\n", strcmp("abc", "abc"));
    printf("strcmp(\"abc\", \"abd\") = %d\n", strcmp("abc", "abd"));
    printf("strcmp(\"abd\", \"abc\") = %d\n", strcmp("abd", "abc"));
    printf("strcmp(\"abc\", \"abcd\") = %d\n", strcmp("abc", "abcd"));
    printf("strcmp(\"\", \"\") = %d\n", strcmp("", ""));

    /* Comparing with == vs strcmp */
    const char *a = "hello";
    char buf[10];
    strcpy(buf, "hello");
    printf("\nPointer comparison (==): %s\n",
           (a == buf) ? "EQUAL" : "NOT EQUAL");
    printf("Content comparison (strcmp): %s\n",
           (strcmp(a, buf) == 0) ? "EQUAL" : "NOT EQUAL");

    /* --- strncmp --- */
    printf("\n=== strncmp ===\n");
    printf("strncmp(\"hello\", \"help\", 3) = %d\n",
           strncmp("hello", "help", 3));
    printf("strncmp(\"hello\", \"help\", 4) = %d\n",
           strncmp("hello", "help", 4));

    /* Prefix checking */
    const char *line = "ERROR: file not found";
    if (strncmp(line, "ERROR:", 6) == 0) {
        printf("Line starts with ERROR: -> \"%s\"\n", line);
    }

    /* --- strchr --- */
    printf("\n=== strchr ===\n");
    const char *text = "hello world";

    char *p = strchr(text, 'o');
    if (p != NULL) {
        printf("strchr(\"%s\", 'o') -> \"%s\" (index %ld)\n",
               text, p, p - text);
    }

    char *q = strchr(text, 'z');
    printf("strchr(\"%s\", 'z') -> %s\n",
           text, (q == NULL) ? "NULL" : q);

    /* strrchr: last occurrence */
    char *r = strrchr(text, 'o');
    if (r != NULL) {
        printf("strrchr(\"%s\", 'o') -> \"%s\" (index %ld)\n",
               text, r, r - text);
    }

    /* --- strstr --- */
    printf("\n=== strstr ===\n");
    char *sub = strstr(text, "world");
    if (sub != NULL) {
        printf("strstr(\"%s\", \"world\") -> \"%s\" (index %ld)\n",
               text, sub, sub - text);
    }

    char *no_match = strstr(text, "xyz");
    printf("strstr(\"%s\", \"xyz\") -> %s\n",
           text, (no_match == NULL) ? "NULL" : no_match);

    /* --- strcspn / strspn --- */
    printf("\n=== strcspn / strspn ===\n");
    size_t n = strcspn("hello, world", " ,");
    printf("strcspn(\"hello, world\", \" ,\") = %zu\n", n);

    size_t m = strspn("   hello", " ");
    printf("strspn(\"   hello\", \" \") = %zu\n", m);

    /* Practical use: remove trailing newline */
    char input[] = "some text\n";
    printf("Before strcspn: \"%s\"", input);
    input[strcspn(input, "\n")] = '\0';
    printf("After strcspn:  \"%s\"\n", input);

    return 0;
}
