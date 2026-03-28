#include <stdio.h>
#include <string.h>

/* Pattern 1: Validate length before copying */
int copy_with_check(char *dest, size_t dest_size, const char *src) {
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        fprintf(stderr, "  ERROR: source (%zu bytes) too long for dest (%zu bytes)\n",
                src_len, dest_size);
        return -1;
    }
    strcpy(dest, src);
    return 0;
}

/* Pattern 2: Use snprintf for everything */
void format_greeting(char *buf, size_t buf_size,
                     const char *name, int age) {
    int needed = snprintf(buf, buf_size,
                          "Hello %s, age %d", name, age);
    if (needed < 0) {
        fprintf(stderr, "  ERROR: snprintf encoding error\n");
    } else if ((size_t)needed >= buf_size) {
        fprintf(stderr, "  WARNING: truncated (%d needed, %zu available)\n",
                needed, buf_size);
    }
}

/* Pattern 3: Safe concatenation with remaining-space tracking */
void safe_concat(void) {
    char buf[32] = "";
    size_t remaining = sizeof(buf);
    size_t offset = 0;
    int written;

    const char *parts[] = {"Hello", " ", "World", "!", NULL};

    for (int i = 0; parts[i] != NULL; i++) {
        written = snprintf(buf + offset, remaining, "%s", parts[i]);
        if (written < 0) {
            fprintf(stderr, "  ERROR: encoding error\n");
            return;
        }
        if ((size_t)written >= remaining) {
            fprintf(stderr, "  WARNING: truncated at part %d\n", i);
            break;
        }
        offset += (size_t)written;
        remaining -= (size_t)written;
    }

    printf("  Concatenated: \"%s\" (used %zu of %zu bytes)\n",
           buf, offset, sizeof(buf));
}

/* Pattern 4: fgets for user input */
void safe_input(void) {
    char input[16];

    printf("  Enter text (max %zu chars): ", sizeof(input) - 1);

    if (fgets(input, sizeof(input), stdin) != NULL) {
        /* Remove trailing newline if present */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
            len--;
        }
        printf("  Read %zu chars: \"%s\"\n", len, input);
    }
}

int main(void) {
    char dest[10];

    printf("=== Pattern 1: Validate before copy ===\n");
    printf("Short string:\n");
    if (copy_with_check(dest, sizeof(dest), "Hi") == 0) {
        printf("  Copied: \"%s\"\n", dest);
    }
    printf("Long string:\n");
    copy_with_check(dest, sizeof(dest), "This is too long for the buffer");

    printf("\n=== Pattern 2: snprintf ===\n");
    char greeting[32];
    format_greeting(greeting, sizeof(greeting), "Alice", 30);
    printf("  Result: \"%s\"\n", greeting);
    char small[15];
    format_greeting(small, sizeof(small), "Bartholomew", 99);
    printf("  Result: \"%s\"\n", small);

    printf("\n=== Pattern 3: Safe concatenation ===\n");
    safe_concat();

    printf("\n=== Pattern 4: fgets for input ===\n");
    safe_input();

    return 0;
}
