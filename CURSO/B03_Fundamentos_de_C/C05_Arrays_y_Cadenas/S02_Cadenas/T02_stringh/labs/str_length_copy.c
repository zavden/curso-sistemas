#include <stdio.h>
#include <string.h>

int main(void) {
    /* --- strlen --- */
    const char *greeting = "Hello, world!";
    size_t len = strlen(greeting);
    printf("strlen(\"%s\") = %zu\n", greeting, len);

    printf("strlen(\"\") = %zu\n", strlen(""));

    /* strlen stops at the first '\0' */
    printf("strlen(\"abc\\0def\") = %zu\n", strlen("abc\0def"));

    /* --- strcpy (unsafe) --- */
    char dest_big[50];
    strcpy(dest_big, greeting);
    printf("\nstrcpy -> dest_big = \"%s\"\n", dest_big);

    /* --- strncpy pitfalls --- */

    /* Pitfall 1: no '\0' when src >= n */
    char buf_no_null[5];
    strncpy(buf_no_null, "hello world", 5);
    /* buf_no_null has NO terminator — print byte by byte */
    printf("\nstrncpy(buf, \"hello world\", 5) bytes: ");
    for (int i = 0; i < 5; i++) {
        printf("'%c' ", buf_no_null[i]);
    }
    printf(" (no '\\0')\n");

    /* Pitfall 2: zero-pads the rest */
    char buf_padded[10];
    strncpy(buf_padded, "hi", sizeof(buf_padded));
    printf("strncpy(buf, \"hi\", 10) bytes: ");
    for (size_t i = 0; i < sizeof(buf_padded); i++) {
        if (buf_padded[i] == '\0')
            printf("\\0 ");
        else
            printf("'%c' ", buf_padded[i]);
    }
    printf("\n");

    /* Safe pattern: force terminator */
    char safe[10];
    strncpy(safe, "a long string that overflows", sizeof(safe) - 1);
    safe[sizeof(safe) - 1] = '\0';
    printf("\nSafe strncpy -> \"%s\" (len %zu)\n", safe, strlen(safe));

    /* --- snprintf as the recommended alternative --- */
    char sn_buf[10];
    int written = snprintf(sn_buf, sizeof(sn_buf), "%s", "hello wonderful world");
    printf("\nsnprintf -> \"%s\" (returned %d, buf size %zu)\n",
           sn_buf, written, sizeof(sn_buf));
    if (written >= (int)sizeof(sn_buf)) {
        printf("Truncation detected: needed %d bytes, had %zu\n",
               written, sizeof(sn_buf));
    }

    return 0;
}
