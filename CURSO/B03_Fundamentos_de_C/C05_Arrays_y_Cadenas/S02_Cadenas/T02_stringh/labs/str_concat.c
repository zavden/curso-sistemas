#include <stdio.h>
#include <string.h>

int main(void) {
    /* --- strcat --- */
    char buf[50] = "hello";
    printf("Initial: \"%s\"\n", buf);

    strcat(buf, " ");
    printf("After strcat(\" \"): \"%s\"\n", buf);

    strcat(buf, "world");
    printf("After strcat(\"world\"): \"%s\"\n", buf);

    /* --- strncat --- */
    char buf2[20] = "hello";
    strncat(buf2, " wonderful world", 6);
    printf("\nstrncat(buf, \" wonderful world\", 6) -> \"%s\"\n", buf2);

    /* strncat always adds '\0', unlike strncpy */
    printf("strlen after strncat: %zu\n", strlen(buf2));

    /* --- Safe concatenation with snprintf --- */
    char result[20];
    int pos = 0;

    pos += snprintf(result + pos, sizeof(result) - (size_t)pos,
                    "%s", "Hello");
    pos += snprintf(result + pos, sizeof(result) - (size_t)pos,
                    " %s", "World");
    pos += snprintf(result + pos, sizeof(result) - (size_t)pos,
                    "!");
    printf("\nsnprintf concat -> \"%s\" (pos=%d)\n", result, pos);

    /* Demonstrate truncation */
    char small[15];
    int p = 0;
    p += snprintf(small + p, sizeof(small) - (size_t)p,
                  "%s", "first");
    p += snprintf(small + p, sizeof(small) - (size_t)p,
                  " + %s", "second");
    p += snprintf(small + p, sizeof(small) - (size_t)p,
                  " + %s", "third");
    printf("snprintf concat (small buf) -> \"%s\"\n", small);
    printf("Would have needed %d bytes total\n", p);

    return 0;
}
